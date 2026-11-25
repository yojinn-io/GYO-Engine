# 2026-11-25 PvP Gateway 移植第 4 段：Match 讀寫存檔（`v8-product-pvp-gateway`）

Owner：Object_FPS_PVP（產品）。GYO2 主會話開的任務卡（PvP Gateway 移植）。作者 Opus 5.5／high。疊在第 3 段（PR #339）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md)（第 1 節第 4 段、D7）。主會話 2026-11-25：Client 改走 Gateway（原第 5 段）移交遊戲流程框架卡，本卡做到 Match 端與 Go 服務，並寫 Client 介面的交接說明。

## 做了什麼

1. **存檔格式**（`logic`：`match/PlayerSave.hpp`）：`{"saveVersion":1,"matches":N,"deaths":N}`；`Settled`（這場算一場、加上這場的死亡）；讀回時版本不是 1 就拒絕；`null`（從沒寫過）是空的存檔。
2. **Match 的存檔**（`world`：`world/MatchSaves.hpp`）：`WithMatchSaves(world, {service, matchId})` 把 Resource `MatchSaves` 與 System `SyncSaves`（frame 的 last、只在 server）加進 Match 的 World。玩家連上時 `GET /internal/v1/saves/{user}`；玩著時數死亡（沒看過的 respawnTick）；離開時 `PUT` 結算後的存檔（帶讀到的版本、冪等鍵 `match-{id}-user-{user}-leave-{n}`）。呼叫是 `GYO::Http` 的 `HttpCall`，每幀輪詢、不阻塞；沒回應用同一把鍵最多送 3 次；被拒絕（409、存檔版本不對）記成失敗、不重試。讀檔還沒回來就離開的玩家，讀到之後再寫。`Report()` 給讀、寫、失敗數與最後的失敗。
3. **Match 的參數**：`--saves A.B.C.D:P`（要和 `--match-id` 一起：鍵裡有 Match 的 id）。
4. **Go**：matches 的設定與 `Spec` 多了 `Saves`（save 服務的位址），本機 provider 傳 `--saves`；`stack` 把自己的 saves 位址交給 matches；`services matches --saves`。
5. **交接說明** `games/object_fps_pvp/docs/gateway-client-handoff.zh-Hant.md`：外部協議 v1、connect 的拿法（token 2 分鐘、成員每次 GET started 房間都重簽）、Client 可以直接用的部件、Match 端的行為、本機怎麼跑、還沒做的。
6. 宣告：第 5 段標成移交；D7 的存檔格式改成 `matches`、`deaths`（`kills` 要知道兇手，現在的規則層沒記），版本由遊戲自己比（`save_version` 的 `Check` 是讀檔換 World 用的）。

7. **同一 userId 新的頂掉舊的**（主會話 2026-11-25 轉使用者的決定 `v8-arch-gyo2-direct-connect-and-kick-old`：「先直連。同一個userId可以頂掉，現在不用考慮資安」）：
   - 座位按連線計數，admission 對同一 userId 的新連線照收（原本拒絕從別的 endpoint 來的第二條，第 2 段審查時定的「舊的優先」撤回）。
   - `SeatPlayers`：新連線進來時，若同一 userId 還有舊連線，新連線接手舊的角色（Resource `SeatedPlayers`：每條連線控制哪個角色），再用 `NetWorldLink` 的 `ServerHost::Close` 關掉舊連線；斷線時只刪那條連線還控制著的角色（原本是按 userId 一律刪，會連新的一起刪）。不動 net：網路頁寫明 System 經 `NetWorldLink` 拿到傳輸物件。
   - `SaveSync` 改成按連線計數：頂掉的事件順序是「先加入、後舊的離開」，不算離開、不結算。
   - 別家：lightyear 的 netcode 拒絕已在線的 client id（舊的優先），這裡照使用者的決定偏離（宣告 L5）。
   - 宣告寫明「使用者 2026-11-25：現在不用考慮資安」（TLS、rate limit、防偽造都不做）。

## 驗證

- 本機（Intel Mac）：`object_fps_pvp.logic`（存檔格式：寫出與讀回、結算、空存檔、別的版本、壞的）、`object_fps_pvp.world`（Match 對 loopback 上假的 save 服務：Client 連上 → 讀到版本 3 的存檔 → Client 離開 → `PUT` 帶版本 3、鍵 `match-77-user-1-leave-1`、`matches` 從 2 變 3；沒有失敗）。假的服務用第 3 段的測試函式庫 `gyo_test_loopback_listener`，伺服器執行緒有逾時、不在執行緒裡中止測試。
- `stack` 的 Go 測試：provider 收到的 `Spec.Saves` 是 stack 自己的 saves 位址。
- 頂掉：world 測試「同一 userId 的第二個 Client 連上 → Match 只剩一個 Peer、該使用者只有一個角色而且是原本那個、Peer 控制它、座位 1、被頂掉的 Client 沒有連線、新的 Client 控制同一個角色」；座位與 admission 的單元改成新語意；`SaveSync` 加「頂掉不是離開」。
- 突變（`pvp-gw-saves-*`、`pvp-gw-save-*`）7/7 killed。過程：`saves-never-read`、`saves-never-written` 第一次 timeout（Match 沒連過去時，假服務的執行緒卡在 accept 不回來；第 3 段的 listener 改成有逾時的測試函式庫後解決），第二次 wrong_failure（測試把訊息以 `const char*` 交給 doctest、被印成位址，改成 `std::string` 後 killed）。

## 沒驗

- 死亡的計數（測試裡沒有人死）、沒回應時的重試、409 的處理。
- Client 經 Gateway 連上真的 Match、離開後真的 save 服務寫入、下次讀到：Client 端移交框架卡（交接說明第 6 節）。

## 設計審查（Opus／high，唯讀；作者同模型，Fable 暫停中）

blocker 2、should 7、nit 7、要使用者定的 0。報告：主 checkout `build/evidence/v8-product-pvp-gateway/segment4-design-review.zh-Hant.md`。審查者沒有建置、跑測試或突變；狀態機的問題是讀程式推出來的。

審查的兩個 blocker 都在 `SyncSaves` 的狀態機，而它沒有測試（S6）。所以先把狀態機抽成 logic 的純類別 `match/SaveSync`（沒有 World、socket、時鐘；照 `ClientSession` 的作法），System 只接事件與 HTTP，再逐條改、逐條加單元測試：

- B1（寫入在途時玩家回來又離開，第二次離開被丟掉）→ 離開的死亡、鍵、結算後的存檔在**開始寫的時候**就定下；在途時的新離開排在它後面自己寫。測試「在途寫入時又離開」。
- B2（先處理加入、後處理離開：同一 endpoint 的重連在同一幀送出兩個事件，玩家還在場就被結算）→ 先離開後加入（照 `SeatPlayers`），同幀的離開＋加入不算離開。測試「同幀重連」。
- S1（重試沒有間隔、5xx 不重試、讀失敗後這場再也不讀）→ 用 tick 退避（16、32、64、128，共 5 次），5xx 也重試；讀失敗的玩家重新加入時再讀。測試「退避」「重連再讀」。
- S2（409 不重試會丟資料：連著兩場、save 服務重啟後自己的寫入被當成失敗）→ 存檔加 `lastWrite`（寫出它的冪等鍵）；409 後重讀，帶著自己的鍵就是自己的寫入，否則把同一次離開結算在新存檔上再寫。測試兩種 409。
- S3（`Report()` 沒人讀、失敗不寫 log、結束時丟掉在途的寫入）→ `match_main` 失敗時寫 warning（帶 user 與讀／寫）；idle 結束前等在途的存檔，最多 5 秒。
- S4（死亡用輪詢數會漏；D7 的「沒記兇手」理由不對）→ 理由更正；斷線那一幀的死亡會漏，記成限制（準確的做法要在判定死亡處發 Message，動到回滾步驟，本卡不做）。同一幀死亡又復活不會發生（復活要好幾秒）。
- S5（每次斷線都算一場）→ 存檔加 `lastMatch`，同一場只算一次；寫進 D7 與交接說明。
- S6 → 上面的單元測試 9 案（`save sync:*` 7 案＋`player save:*` 2 案）；宣告表的「再連讀到新值」（跨行程）隨 Client 移交框架卡。
- S7（交接說明缺的）→ 補全部錯誤碼與 body／鍵的上限、started 房間 leave 後拿不到 connect、存檔寫入的時機（斷線要等逾時）、重連不多算、不認得的 saveVersion。
- N2 → 分段表跟著改。N3 → 註解重寫。N5 → `matches --saves` 檢查 host:port。N6 → `lastFailure` 帶 user 與讀／寫。不改：N1（Match 的位址解析和大廳的重複；大廳在 PvP 卡的 client 函式庫，留給接 Client 的卡一起搬）、N4（讀檔多轉一次文字，量小）、N7（觀察）。

審查修正＋頂掉之後：`ctest -L "cpu|gpu"` 256/256、`-L "go|network"` 4/4（本機 Intel Mac）；`pvp-gw-` 的第 4 段突變（`save-*`、`sync-*`、`saves-*`）與頂掉的突變（`kick-*`、`seat-*`、`admission-token-ignored`、`sync-replaced-is-leave`）全部 killed（`seat-kept` 第一次 wrong_failure：被一個沒帶訊息的檢查殺掉，那個檢查加上訊息；`sync-reconnect-settled` 的 find 隨按連線計數改了，更新後 killed）。整份目錄沒有 stale 的 find。

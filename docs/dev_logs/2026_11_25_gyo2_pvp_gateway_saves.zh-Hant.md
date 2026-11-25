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

## 驗證

- 本機（Intel Mac）：`object_fps_pvp.logic`（存檔格式：寫出與讀回、結算、空存檔、別的版本、壞的）、`object_fps_pvp.world`（Match 對 loopback 上假的 save 服務：Client 連上 → 讀到版本 3 的存檔 → Client 離開 → `PUT` 帶版本 3、鍵 `match-77-user-1-leave-1`、`matches` 從 2 變 3；沒有失敗）。假的服務用第 3 段的測試函式庫 `gyo_test_loopback_listener`，伺服器執行緒有逾時、不在執行緒裡中止測試。
- `stack` 的 Go 測試：provider 收到的 `Spec.Saves` 是 stack 自己的 saves 位址。
- 突變（`pvp-gw-saves-*`、`pvp-gw-save-*`）7/7 killed。過程：`saves-never-read`、`saves-never-written` 第一次 timeout（Match 沒連過去時，假服務的執行緒卡在 accept 不回來；第 3 段的 listener 改成有逾時的測試函式庫後解決），第二次 wrong_failure（測試把訊息以 `const char*` 交給 doctest、被印成位址，改成 `std::string` 後 killed）。

## 沒驗

- 死亡的計數（測試裡沒有人死）、沒回應時的重試、409 的處理。
- Client 經 Gateway 連上真的 Match、離開後真的 save 服務寫入、下次讀到：Client 端移交框架卡（交接說明第 6 節）。

## 設計審查

（審查後補）

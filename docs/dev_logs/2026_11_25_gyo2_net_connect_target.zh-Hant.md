# 2026-11-25 連線目標跟著 World、socket 延後開（`v8-arch-gyo2-net-connect-target`）

Owner：Engine（net Module）。PvP 移植實作卡（GYO2 主會話開的任務卡）做。作者 Opus 5.5／high。
根據：[事前宣告](../architecture/target/v2/net-connect-target-declaration.zh-Hant.md)；使用者 2026-11-25「選(1)，socket延後開。」（核心：「遊戲開始就會連服務器這個是完全不能接受的，因為我有單人模式」；模型：World＝場景，單人與多人是不同的 World）。起因：R1b 卡查到「連線參數在建 Product 時就固定」，主會話轉來。

## 經過

- 主會話先照新 AGENTS 選 (1)（連誰跟著 client World）。使用者看了說明一度想選 (2)（lightyear 式 Connect 訊息），作者先把四家並列成「都是 (2)」，被指出前後改口後交叉比對：lightyear、Godot 一個程式只有一個 World，連線只能是 World 裡的動作；結構上和 GYO 最像的是 Unity（單人 `CreateLocalWorld`、多人 `CreateClientWorld`）。(1) 在不留殘留、不讓 Engine 替產品刪實體（D-8 退掉的 (a)）、不推翻 D-8 三項勝出；(2) 只多給「斷線超過逾時後不重進場景就重連」，沒人點名。使用者選 (1)＋socket 延後開。
- 教訓：比別家時先對齊「World」在各家是什麼，只比 API 的形狀會比錯。

## 做了什麼

1. `NetConnectTarget{server, userId, token}`（`NetService.hpp`）：client World 的 Resource，net 在每個帶角色的 World 登記；`server` 是 `std::variant<monostate, OS::DatagramAddress, Endpoint>`。
2. `NetProductOptions` 拿掉 `server`、`serverEndpoint`、`userId`、`token`；`NetService` 的建構子不再收 server 的 endpoint。
3. `NetService::SetTarget`：沒有目標（或種類不合，計 `Unreachable`）→ 關掉還活著的連線、不連；同一個目標而連線還活著 → 沿用；不同 → 關舊的（Disconnect 當場送出）；斷掉的 → 丟掉。`SetFacts` 沒有目標時不開 session。
4. net 的準備函式不再替 client World 開 session（作者的準備函式與 `extra` 在它之後），改由 `ReceiveNet` 在 World 的第一幀讀目標；resync 的判斷搬到同一處（`NetWorldLink::resyncPending`，`ApplyReplication` 送出後清掉），`NetReplicaState::resyncNeeded` 拿掉。
5. socket 延後開：Server 照舊在 `Configure` 開；Client 在 `SetTarget` 第一次拿到 UDP 位址時才開（開不了計 link failure、不連）。沒有 socket 時 `Pump`、`Flush` 什麼都不做，`UdpAddress()` 回 nullopt。
6. 使用者改寫：net 的測試（`End` 加 `target`，`Describe` 加準備函式）、`ModuleTests` 直接建 `NetService` 的兩案、「a failing Link…」給目標（原本靠預設的 endpoint 連）、PvP 的 Client 與兩個測試支援。行為不變。
7. 文件：net 頁第 6 節（Product 一份、連誰）、第 9 節（重連）、登記清單；標草稿，最終文字合併前核准。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 253/253（含新的 `net.connect-target`）、`ctest -L network` 1/1。
- `net.connect-target` 4 案：J1 沒有目標不連；J2 J3 同一個目標沿用、另一個目標開新的並關舊的、沒有目標關掉；J3 替換的 `extra` 給目標；J4 J5 UDP：server 啟動就 listen、沒有目標的 client 不開 port、種類不合不連且計一次 link failure、給位址的 client 第一幀才開 socket 並連上。
- 突變 `nt-target-*` 5/5 killed。過程：`nt-target-none-connects` 原本是拿掉 `SetFacts` 的守門（會解參考空指標、當掉），改成「沒有目標時沿用上一個」；「換目標不關舊的」第一次 wrong_failure：同一個 endpoint 帶更大的 salt 的新握手會讓 Match 自己結束舊連線，所以觀察不到，改名 `nt-target-end-not-sent`、由「沒有目標時關掉」那一案殺。
- **沒驗**：真的大廳（PvP 換前台 World）：等 R1b 與本批合併後在 PvP 卡接。

## 設計審查（Opus／high，唯讀；作者同模型，Fable 暫停中）

blocker 1、should 4、nit 7。紀錄：`build/evidence/v8-product-pvp-port/net-connect-target-design-review.zh-Hant.md`（本機）。

- B1（`Close` 後馬上替換、中間沒跑一幀時，舊 session 的 Disconnect 被丟掉，Match 要等逾時；G-9 連同一個 endpoint、salt 規則遮住了它）→ `SetTarget` 不保留的 session 一律 `EndClient`（關掉、送出剩下的 datagram 給舊的位址、丟掉）。測試加「closed just before the replacement…」，突變 `nt-target-closed-dropped`。
- S1（新連線的 epoch 變成「建 session 時的計數減一」，第一幀之前又有一次失敗的替換就錯開，B 的 Confirm 整個生命期被當 StaleEpoch）→ epoch 隨 `SetTarget` 給（`NetWorldLink::clientEpoch`），請求帶它；salt 測試同時檢查 epoch。
- S2（net 頁沒改到的地方）→ 第 6 節第 198、205、212 行與第 9 節第 311 行改寫原句（不是附加），`UdpAddress` 註解，宣告第 4.6 節更正（net 宣告的 D-8 原文沒寫「準備時」）。
- S3（「換目標時關掉舊的」從外面驗不到）→ 照實改宣告第 5 節的突變清單與測試註解；Disconnect 有沒有送由「沒有目標時關掉」與「Close 後馬上替換」兩案判。
- S4（相關既有突變沒重跑）→ 重跑 epoch／resync 相關的 7 個（結果見下）。
- N1（`SetFacts` 的 client `newWorld` 分支成了死碼）→ 拿掉，salt 測試改成「SetTarget＋SetFacts＋Close」的循環。N2（Engine 註解的產品詞）→ 改中性說法。N3（種類不合回 `Unreachable`）→ `LinkFailed`。N4（縮排）→ 改。N5、N6（開不了 socket、給 Link 卻給位址沒測）→ 加一案，突變 `nt-target-open-failure-ignored`。N7（`live` 在 Pump 之前判斷）→ 不改，和原本行為相同。
- 要使用者決定的第 3 項（`beforeSwap` 跑了 `Main` 後替換失敗）→ 作者照 (b) 做：舊 World 的 `ReceiveNet` 發現 session 沒了而 `clientPeer` 還在時，照一般的斷線刪 Peer、發 `PeerDisconnected`。機制內的正確性，不是新規則。
- 交使用者（主會話轉）：第 1 項（讀目標的時機：第一幀讀一次，或 Resource 第一次出現時讀，讓生成中的 World 可以「地圖載完才連」）；第 2 項（回到沒有目標的 World 之後要不要關 UDP socket：現在開了就一直開著）。

修正後：`ctest -L "cpu|gpu"` 255/255、`ctest -L network` 1/1；突變：新的 `nt-target-*` 7 個（含 `nt-target-closed-dropped`、`nt-target-open-failure-ignored`）與相關既有的 epoch／resync 7 個共 14/14 killed。`nt-request-epoch-missing` 的 find 因 epoch 改由 `SetTarget` 給而 stale，照新的字面（`lastSalt_, targetEpoch_};`）改後 killed。

## 使用者定的兩項（2026-11-25）

使用者給了整體遊戲流程（單人：房間 → 載入讀檔 → 遊戲 → 撤離結算；多人：大廳 → 建房或加入 → 房間與準備 → 開始後鎖房 → 載入、讀各玩家的檔 → 遊戲 → 照模式結算），並說明 Gateway 的定位（Match 以外的入口，房間等服務放在它後面，「就像微服務的概念」）。照流程定：

1. 讀目標的時機：維持第一幀讀一次（多人載入時就要連，Match 要先拿到各玩家的檔）。
2. 回到沒有目標的 client World：送完剩下的 datagram 後關掉 UDP socket（打完多人回到單人時不留著 port），下一個帶目標的再開。突變 `nt-target-socket-kept`。`nt-target-socket-early` 的 expect 改成「第一幀之前沒開」（沒有目標的 World 現在第一幀就關掉，原本的檢查看不到）。

過程中查到一件既有的行為（不是本批造成的）：session 還在握手中就被關掉時，`ClientSession::Close` 沒有 Disconnect 可送（還沒有 connection），Match 若已接受就要等逾時。測試改成等兩端都連上才替換；這件記下，要不要處理之後再看。

突變（這一輪）：`nt-target-socket-kept`、`nt-target-closed-dropped`、`nt-target-end-not-sent`、`nt-target-socket-early` 4/4 killed；`ctest -L "cpu|gpu"` 255/255、`ctest -L network` 1/1。

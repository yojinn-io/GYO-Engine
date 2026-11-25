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

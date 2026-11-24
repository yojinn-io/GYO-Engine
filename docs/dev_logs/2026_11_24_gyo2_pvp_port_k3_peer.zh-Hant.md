# 2026-11-24 PvP 移植：K3 Client 的 Peer 實體被回滾刪掉（`v8-product-pvp-port`）

Owner：架構（回滾 Module、網路 Module）。在 PvP 移植實作的任務卡裡發現、修正（事前宣告 `games/object_fps_pvp/docs/port-declaration.zh-Hant.md`）。作者 Opus 5.5／high。

## 現象

PvP 的 World 測試：Match＋兩個 Client（K3：Client 選回滾策略）經 MemoryLink 連上。Client 的 `NetWorldLink::clientPeer` 有值，實體卻不在；Client 找不到自己的 Peer，`PeerControls` 寫不上去，輸入送不出。

## 原因

1. 網路 Module 在 `ReceiveNet`（`Main`／`First`，步外）生成 Client 自己的 Peer 實體。
2. 同一幀，伺服器在連上時就有的實體（PvP：別人的玩家）以 `Entered` 進入回滾的入口，存活集合在確認點 C 和記錄的不同，回滾在 `PreUpdate` 執行。
3. C 早於 Peer 生成的那一步（Client 的時鐘超前伺服器），C 記下的存活集合沒有 Peer；官方策略的還原把存活集合整份放回，Peer 被刪。回滾頁第 3 節的限界「本機的步外生成照舊整份放回」寫的就是這件事，網路 Module 自己踩到。

`net.k3` 原有的測試沒抓到：那個樣本的伺服器實體在 Client 連上之後才生成（第 30 步起的箱子），進入時 C 已晚於 Peer 生成。

## 前提

Engine 要解決：Peer 實體是網路 Module 生成的，存活集合的還原是回滾 Module 的；作者碰不到兩者的交界。

## 修法（取 lightyear 的概念）

- 回滾 Module：新 ECS 元件 `KeepThroughRollback`（選了策略的 World 才登記，R-4）。官方策略記錄存活集合時不含帶它的實體，還原時把它們留在原處（不刪、不放回）。只管存在：實體上可回滾登記的元件照舊回滾（確認點沒有的會被拿掉），不要給它這種元件。取 lightyear 的 `DisableRollback`（`crates/replication/prediction/src/rollback.rs:272-275`）的概念；lightyear 連元件也排除、重跑時把實體藏起來，意思比這裡寬，所以不沿用它的名字（設計審查 S2，原本叫 `DisableRollback`）。
- 網路 Module：K3 的 World 多一個 `KeepPeerInBeing`（回滾的決定 stage、`RunRollback` 之前；Peer 在 `First` 的同步點之後才在，放在 `First` 裡時加不上去，第一次試的時候測試照樣失敗），替 Client 自己的 Peer 加 `KeepThroughRollback`。
- PvP 的 Client 之後也用它標自己的相機與畫面的實體（第 2 段）。

## 測試

- 先寫失敗的測試：`net.k3`「the client's own Peer entity outlives the rollbacks to points before it connected」——兩端從頭跑，連結在伺服器過了第 33 步（箱子已生成）之前不送任何東西，Client 連上時的回滾回到 Peer 生成之前。修正前失敗（`[nt-k3-peer-kept]`），修正後過。前兩版的重現（伺服器先跑、連結大延遲）都沒踩到：前者 Client 沒有沒 Peer 的記錄，後者時鐘往回跳、沒有發生回滾。
- `rollback strategy: an entity with KeepThroughRollback made after C stays in being across the rollback to C; one without is removed`：帶標記的留下、沒帶的照舊被刪；它的可回滾登記元件照舊回滾；之後的回滾也留下；刪掉之後回滾不會放回。
- 本機：`gyo_net_tests` 96/96、`gyo_rollback_tests` 68/68；全量 `ctest -L "cpu|gpu"` 243/243（第 1 段一起，改名前）。
- 突變：`rb-keep-through-recorded`、`rb-keep-through-restored`、`nt-k3-peer-not-kept` 3/3 killed（`rb-keep-through-recorded` 第一次存活：最後一次回滾的權威值和預測相同、回滾沒發生；測試改用預測不會有的值後 killed）。

## 設計審查

和第 1 段一起審（Opus／high，作者同為 Opus；Fable 暫停）。報告在主 checkout 的 `build/evidence/v8-product-pvp-port/segment1-design-review.zh-Hant.md`。和本修正有關的：

- S2（借了 lightyear 的名字、語意較窄）→ 改名、寫明元件的後果、測試驗它。
- S1（存活集合「預設全收、再逐一排除」：之後每個在 Client 步外生成實體的單位都得自己加標記；lightyear 只回滾明確屬於預測的實體）→ 不改、交使用者：要改回滾頁第 3 節已核准的限界（「本機的步外生成照舊整份放回」）。採用時，`KeepThroughRollback` 與 `KeepPeerInBeing` 都可以拿掉。
- N2（`KeepPeerInBeing` 每幀用 `ExclusiveWorld`）→ 不改：標記只在選了策略的 World 登記（R-4），`ReceiveNet` 的 spawn 型別在各角色都登記，帶進去要分出 K3 的版本；S1 採用時一起拿掉。

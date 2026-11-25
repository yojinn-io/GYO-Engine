# 2026-11-25 同一 endpoint 重連先驗再換、整理 stale 突變（`v8-arch-gyo2-net-order-mutations`）

Owner：Engine（net）＋突變清單。GYO2 主會話開的小卡做。作者 Opus 5.5。
根據：Gateway #340 設計審查 N9；R1b #334 dev_log 記的 stale 突變；使用者 2026-11-25「開小卡」。

## 宣告（動工前，小卡）

1. `ServerHost::Respond`：同一 endpoint、更大 salt 的新握手，先比事實、問 `admission`，都過了才結束舊連線；被拒時舊連線不動。重送的回應（同 salt）與舊握手晚到（較小 salt）照舊。不碰 `object_fps_pvp` 經 `NetWorldLink` 頂掉同一 userId 舊連線的做法（`v8-arch-gyo2-direct-connect-and-kick-old`）。
2. 掃描所有突變清單的 find，逐條歸 (a) 改名或移動 → 更新 find／expect、(b) 守的東西已不存在 → 拿掉、(c) 守誤用 → 拿掉。不改測試的判定。

## 1. net：被拒的重連不斷掉自己還活著的連線

- 原本：challenge 通過、salt 較大 → 先 `End` 舊連線（`ClosedByRemote`）→ 才 `CompareFacts`、`admission`。Client 從同一 endpoint 帶過期或不合法的 token 重連時，被拒之前自己的連線已經斷了。
- 改後：`End` 移到 `CompareFacts` 與 `admission` 之後（`engine/modules/net/src/ServerHost.cpp`）。通過時事件順序仍是 Disconnected → Connected，和以前一樣。
- 參考：lightyear netcode `crates/connection/netcode/src/server.rs:799` `process_connection_response`：先解 challenge token，驗過才動連線表；已連線的 client 再來的回應直接忽略，不斷線。這裡的差別是同一 endpoint 帶更大 salt 的新握手被當成重連、驗過後換掉舊的（D-8、審查 S2），所以「驗過才換」是 netcode 順序在重連上的對應。
- PvP 的 `Seats` 按連線計數、在 System 讀 `PeerDisconnected` 時才 `Release`；`admission` 在 `Respond` 裡同步呼叫，改前改後它看到的 `Seats` 都還算著舊連線，行為不變。
- 外部測試：`tests/common/net/SessionTests.cpp`「a newer handshake from the same endpoint that is refused leaves the live connection as it was」：token 被拒、事實不同兩種，各確認連線數仍 1、識別不變、沒有任何事件，舊連線仍收送。
- 突變（`net_mutations.json`）：新增 `nt-reconnect-ends-before-check`（`End` 移回檢查之前）、`nt-reconnect-old-kept`（通過時不結束舊連線）。
- 文件：`net.zh-Hant.md` 第 6 節 admission 一條、第 9 節 salt 一條補上順序與系譜。

## 2. stale 突變的整理

掃描：Python 讀 `git ls-files '*mutations.json'` 列出的 36 份清單，每條 `find` 對目標檔做位元組計數，`count == 1` 才算命中（同 `run_mutations.py` 的條件）；`file` 先照 repo 根、再照清單所在目錄找。正例：`nt-target-socket-kept` 回報 1；反例：R1b dev_log 列的 16 條全被報出（dev_log 寫 15，實數 16：`or-exch-*` 是四條）。

結果：2905 條中 23 條對不上（R1b 的 16 條＋之後合併的 PR 造成的 7 條）。逐條讀程式後全歸 (a)：守的性質都還在，只是周圍程式被改寫。沒有 (b)、(c)。每條在 `what` 末尾加「find/replace updated … by v8-arch-gyo2-net-order-mutations」。expect 都沒改。

| id | 清單 | 原因 | 處置 | 結果 |
|---|---|---|---|---|
| `we-j32-spawn-before-despawn` | plans/kernel | (a) despawn 迴圈加了「同一 parent 不算 detach」（R-1） | find、replace 裡的 `rewired` 那段照現在的程式 | killed |
| `msg-registry-condition-reader-dropped` | plans/kernel | (a) 後面加了 `conditionExtractOutside` | find 改成 `AddMessageReaders` ＋下一行 | killed |
| `msg-registry-set-condition-reader-dropped` | plans/kernel | (a) 同上（set 的條件） | 同上 | killed |
| `or-unit-error-ignored` | orchestration | (a) 條件多了 `unchecked` | find／replace 帶上 `unchecked` | killed |
| `g8-kernel-only-failure` | orchestration | (a) 同上；而且 kernel 錯誤必定同時設 `notChecked`（`Product.cpp:402-403`），只關 `kernelFailed` 是等價突變（第一次重跑 survived） | replace 改成 `((kernelFailed \|\| unchecked) && false)`，兩個一起關 | killed |
| `g8-frame-list-reversed` | orchestration | (a) `RunOn` 多了 lease 參數（O-13） | find／replace 照現在的呼叫 | killed |
| `g8-frame-error-ignored` | orchestration | (a) 同上 | 同上 | killed |
| `g8-runschedule-unknown-world-code` | orchestration | (a) 同一行在 `Lend` 也出現（count 2） | find 往後帶 `slot->lease` 那行，只剩 `RunScheduleSteps` | killed |
| `or-ended-runschedule-unchecked` | orchestration | (a) 同一段在 `Lend` 也出現（count 2） | find 前面帶函式簽名 | killed |
| `or-ended-before-name` | orchestration | (a) 同上 | 同上 | killed |
| `or-failure-not-attached` | orchestration | (a) `Break` 改經 lease 的 `record` | `Break` → `record` | killed |
| `or-run-throw-unrecorded` | orchestration | (a) 同上 | 同上 | killed |
| `or-exch-out-before-round` | orchestration | (a) `RunOn` 多了 lease 參數 | 照現在的呼叫 | killed |
| `or-exch-init-after-in` | orchestration | (a) 迴圈多了 `if (slot.lease) continue;` | find／replace 帶上那行 | killed |
| `or-exch-break-calls-out` | orchestration | (a) `RunOn` 多了 lease 參數 | 照現在的呼叫 | killed |
| `or-exch-round-after-failed-in` | orchestration | (a) 同上 | 同上 | killed |
| `g3-depth-not-cleared` | render | (a) depth 改成依 `Pass3D::clearDepth` 清或留、store 改 STORE | find 照現在五行；replace 一律 LOAD、不 cycle | killed |
| `rb-alive-not-restored` | rollback | (a) 寫進 set 的存活集合改成 `after`（`alive` ∪ 標記的實體，`StateRollbackStrategy.cpp:279-281`），性質（不還原存活集合）不變 | find 照現在那行 | killed |
| `rb-authority-not-laid-over` | rollback | (a) 同上，`AddTo` 也改傳 `after` | 照改 | killed |
| `sw-motion-missing` | window_input | (a) 位移改經 `MotionDelta` 換單位 | find 照現在那行 | killed |
| `pvp-room-not-full` | object_fps_pvp | (a) 座位計數搬進 `Seats::Take` | find 改成 `if (connections_.size() >= capacity_) return` | killed |
| `jw-oom-escapes-endarray` | json | (a) pretty（#343）在 `try` 裡加了換行 | find 從 `out_ += close;` 起 | killed |
| `jw-oom-escapes-key` | json | (a) `':'` 改成依縮排寫 `":"` 或 `": "` | 照現在那行 | killed |

合計 23 條：(a) 23、(b) 0、(c) 0；重跑 23/23 killed。掃描在改完後再跑一次：2907 條（含新增 2 條）stale 0。

## 驗證

- 本機（Intel Mac，test preset）：`ctest -L "cpu|gpu"` 256/256、`ctest -L network` 2/2。
- 突變（D184，只跑相關的）：net 5/5（新的 2 條＋這段程式的 `nt-admission-ignored`、`nt-reconnect-reuses-connection`、`nt-older-salt-ends-connection`）；上表 23/23（`g8-kernel-only-failure` 改 replace 後單獨重跑 killed）。紀錄：`build/target/_build/test/mutations-net-order.txt`（本機）。
- 沒驗：L1 三平台（PR 上 dispatch）。

## 設計審查（Opus／high，唯讀，只給成果物與原始要求）

blocker 0、should 3、nit 7。net 的改動是最小、單一出口的「先驗再換」；其餘 22 條 (a) 的歸類站得住，沒有等價突變、沒有改測試的判定。

- S1（`g8-kernel-only-failure` 的 replace 遷就 `Product.cpp:315-321` 的冗餘條件，打到的性質變寬）→ 不在本卡（orchestration）；轉主會話當後續：拿掉一項，突變回到只關單一項。
- S2（`RunScheduleSteps` 與 `Lend` 各寫一份「RequireNotRunning → Ended → Find → UnknownWorld」，是 3 條 stale 的源頭，`Lend` 那份的順序沒有突變守）→ 不在本卡；轉主會話當後續：抽成共用的前置檢查。
- S3（系譜引錯函式、沒寫和 netcode.io 的偏離）→ 照改：引 `server.rs:691-782`，補「netcode.io 拒絕已連線的位址（`:720-726`），這裡驗過後替換」。
- N1（缺句號）、N2（`nt-reconnect-ends-before-check` 的 find 帶註解）、N3（`nt-reconnect-old-kept` 的 expect 改成 tag `[nt-reconnect-old-ended]`，測試那個 CHECK 加訊息）、N4（`rb-*` 的描述）、N7（NT-1 列上新突變）→ 照改；net 5 條重跑 5/5 killed。
- N5（`what` 一直疊改寫歷史）、N6（`we-j32` 的 find 橫跨 ApplySync 數十行）→ 記錄，不在本卡。

## 留給之後

- 審查 S1、S2（orchestration，見上）；N5、N6。
- `Product.cpp` 建置第 5 步的 `kernelFailed` 與 `unchecked` 現在互相涵蓋（kernel 錯誤必設 `notChecked`），條件裡有一個是多的；要不要拿掉屬於 orchestration，下一個碰它的批決定。

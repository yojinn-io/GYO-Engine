# PvP v7.1 清單核對結果

更新：2026-10-16（P5 等項目的處置依 D94 改為 `edge-contract`）；初版 2026-10-15。Owner：`object_fps_pvp`。計畫與批次見 [README](README.md)，決策見 [交接](HANDOFF.md)。

v7.1 規劃時的唯讀盤點，以 HEAD `c742836`（v7 結束）為準。做法：5 位讀者分組對照程式與文件；2 位對抗式核對，提出的更正已套用；1 位完整性評審補列缺項。各 agent 的原始輸出留在規劃會話的工作檔，不放進 repo；本文件是核對後的結論。

讀法：

- **證據**：只列前兩處。程式行號以 `c742836` 為準（之後合併的 PR #75～#77 只改文件，程式行號不變）。文件行號在同步 master 之後可能漂移，主要是 `network-architecture.zh-Hant.md`、`protocol-v6.zh-Hant.md`、v7 HANDOFF。
- **改 wire**：修正這一項需不需要改 wire（要／可選／否）。pv7 是 v7.1 唯一一次 wire 變更。
- **動 digest**：修正會不會改變權威 digest（會／可能／否）。
- **處置**：
  - 批次代號：見 README 的樹狀總覽；
  - `re-evaluate`：pv7 之後用新數據重新評估；
  - `re-evaluate 之後`：內容不變，等評估之後再排；
  - 取消、已解決、已被取代：附決策編號。
- **代號**：照使用者的初始清單（P、A、B、C、D、E、F、G）。和 2026-10-14 種子 README 的對照：A4＝A2、A5＝A3、B6～B11＝B1～B6、C12～C17＝C1～C6、D58-*＝D-1、D-LOG＝D-2。M0、TT2-B10、IP34-B12、LAN15 是種子 README 第 10～13 項。重複的項目已合併，並在項目欄標「合併」。

## 統計

| 處置 | 項數 |
|---|---|
| pv7-line | 20 |
| after-pv7 | 27 |
| re-evaluate | 12 |
| re-evaluate 之後 | 34 |
| plan-sync-docs | 3 |
| 取消 | 10 |
| 已解決 | 3 |
| 已被取代 | 3 |
| 合計 | 112 |

## P. pv7 與相關任務

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| P1 | pv7 契約文件、ProtocolVersion 6→7、版本不一致的明確錯誤、驗收工具升 pv7。更正：f255bf1（pv6 第 09 批）的單一常數導出是前提，不算部分完成。 | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp:15`<br>`apps/object_fps_pvp/gateway/adapter/adapter.go:15-19` | 要 | 否 | proto-rename-v7＋version-errors |
| P2 | runtime link 每秒 1 次心跳對時（偏移、RTT 最小值濾波、漂移回歸，每 10 秒寫進兩端日誌），不得用在本機年齡判斷 | 未解決 | `apps/object_fps_pvp/protocol/runtime_v6.proto:107-121`<br>`apps/object_fps_pvp/src/Pvp/IpcHost.cpp:385` | 要 | 否 | heartbeat-sync |
| P3 | Client↔Gateway 時間回聲（D35），量跨機器偏移與漂移（含第 15 批移出的 ≥15 分鐘量測） | 暫定 | `apps/object_fps_pvp/protocol/client_v6.proto:7`<br>`apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:440-443` | 要 | 否 | time-echo |
| P4 | GatewayDropReport（D49 第二階段）：原因列舉一次定完整、dwell 只有一個定義、回報有界、轉送身分不轉送內容 | 部分（a700b66、cb6bbe0） | `apps/object_fps_pvp/gateway/ingress_statistics.go:120-170`<br>`apps/object_fps_pvp/gateway/server.go:373` | 要 | 否 | drop-report |
| P5 | 依 D49 分類 Gateway 在 06／08／08b／08c 加的狀態，決定是否轉送 A3／A5，寫「Gateway 職責」正式條文（PEP／PDP） | 未解決 | `apps/object_fps_pvp/gateway/action_delivery.go:20-35`<br>`apps/object_fps_pvp/gateway/action_delivery.go:225-252、:257-273、:500-512` | 可選 | 否 | edge-contract（D94：Gateway 回到無狀態邊緣，A3／A5 隨帳本刪除） |
| P6 | ingress v1 和 GatewayDropReport 是否統一成一個死信 Data Contract。更正：D57 已決定死信只當原則；D73⑤ 又把問題移到 v7.1，待確認 D57 是否仍成立。 | 暫定 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:102`<br>`docs/object_fps_pvp/plans/v7.1/README.md:25` | 可選 | 否 | dead-letter-position |
| P7 | 讓 Match 看見更多遲到 → late 修正變多、D44 風險上升，所以 P4 要和 B6／B8／A5 一起設計 | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:396`<br>`docs/object_fps_pvp/plans/v7.1/README.md:37-38` | 可選 | 可能 | pv7-design |
| P8 | pv7 之後重推 FireGate（重推條件 4、11：重跑 M0 型量測並加跑 Client→Gateway 短延遲注入）與 D44 整體回歸 | 未解決 | `docs/object_fps_pvp/plans/v7/09-firegate-c2.md:125`<br>`docs/object_fps_pvp/plans/v7/08c-drop-report.md:761` | 否 | 否 | d44-regression-pv7（FireGate 重推部分隨 A1 取消） |

## 骨骼 hitbox（D87）

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| HB | 骨骼 hitbox 的契約支持（D87） | 未解決 | `apps/object_fps_pvp/src/Pvp/ShotQuery.cpp`<br>`apps/object_fps_pvp/protocol/client_v6.proto` | 要 | 否 | pv7-design＋hitbox-fields |

## A. 架構與協議

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| A1 | 射擊併入輸入命令、照命令步序判定（替代：權威端早到就延後、不拒絕） | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp:18`<br>`apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:401,420-423` | 要 | 會 | pv7-design＋a1-shot-step |
| A4 | 延遲位移：產生→執行約 38 ms，候選把追蹤用的領先拆成 1 | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp:53,62`<br>`apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:144-145` | 否 | 否 | re-evaluate |
| A5 | D44 偶發 epoch 重設：backlog 門檻 105、約 300 ms 延遲消失時的重設、快速正向回收 | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp:55-57`<br>`apps/object_fps_pvp/src/Pvp/PvpMatch.cpp:436-461` | 否 | 可能 | d44-regression-pv7＋re-evaluate |

## B. 相位追蹤

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| B6 | 重新擷取的樣本邊界 S／M（消掉 1-7 節的三個已知限制） | 未解決 | `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:135-150`<br>`apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:155-158` | 否 | 否 | re-evaluate |
| B7 | Match 在命令到達時就發布 slack 樣本。更正：採用就要改 movement_slack 的語意，算改 wire。 | 未解決 | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:585-627`<br>`apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:224-233` | 要 | 否 | re-evaluate |
| B8 | late 修正的幅度依遲到量決定，或改「連續 N 次」 | 未解決 | `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:149`<br>`apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:155-158` | 否 | 否 | re-evaluate |
| B9 | 停頓後的 reseed 用過時的 snapshot 播種，領先實際歸零 | 未解決 | `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:218-223`<br>`apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:67-97` | 否 | 可能 | re-evaluate |
| B10 | 權威 digest 不保護相位追蹤：補一層相位追蹤的 golden | 未解決 | `tests/object_fps_pvp/AuthorityDigest.cpp:474-480`<br>`tests/object_fps_pvp/AuthorityDigest.cpp:572-575` | 否 | 會 | re-evaluate 之後（修正批之前） |
| B11 | FireGate.cpp 的 earliest 寫死減 2，領先改變時估晚 | 未解決 | `apps/object_fps_pvp/src/Pvp/FireGate.cpp:21-22`<br>`apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:437-442` | 否 | 否 | 取消（D90：A1 讓 FireGate 退役） |

## C. Code smell

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| C12 | token 規則三份拷貝，抽成純函式 | 未解決 | `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:596-622`<br>`apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:690-699` | 否 | 否 | re-evaluate 之後 |
| C13 | 被替代的序號存兩份：SlackTrack.substituted（64）與 ingress 帳本（600） | 未解決 | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:13-15`<br>`apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:606-622` | 否 | 否 | re-evaluate 之後 |
| C14 | Gateway 的 p.commands 衝突檢查和 runtime link 的內容比對重疊 | 未解決 | `apps/object_fps_pvp/gateway/server.go:53`<br>`apps/object_fps_pvp/gateway/server.go:1005-1013` | 否 | 否 | re-evaluate 之後 |
| C15 | quad_evidence.py 和 command_evidence.py 收斂成 N 人版 | 未解決 | `build/acceptance/object_fps_pvp/quad_evidence.py:1-13`<br>`build/acceptance/object_fps_pvp/quad_evidence.py:25-28,:37-45` | 否 | 否 | re-evaluate 之後 |
| C17 | 第 04 批 xhigh 審查只記錄的項目（總項） | 未解決 | `docs/object_fps_pvp/plans/v7/04-client-roles.md:180-184`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:453` | 否 | 否 | re-evaluate 之後 |
| C17-1 | 時間倒退的意圖（多個發布者時） | 未解決 | `apps/object_fps_pvp/src/Pvp/ClientSimulationRole.cpp:142-146`<br>`apps/object_fps_pvp/src/Pvp/ClientSimulationRole.cpp:60-72` | 否 | 否 | re-evaluate 之後 |
| C17-2 | 網路 probe 不再以真實 socket 涵蓋伺服器端的輸入逾時 | 未解決 | `build/acceptance/object_fps_pvp/network_main.cpp:275-280`<br>`build/acceptance/object_fps_pvp/network_main.cpp:282-297` | 否 | 否 | re-evaluate 之後 |
| C17-3 | SendInput(input, generation) 的世代檢查與角色送出路徑沒有 L1 | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/ClientConnection.hpp:91-95`<br>`apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:784-787` | 否 | 否 | re-evaluate 之後 |
| C17-4 | 負向 slew 期間 FireGate 最多保守 0.25×經過時間 | 未解決 | `apps/object_fps_pvp/src/Pvp/ClientSimulationRole.cpp:38-46`<br>`apps/object_fps_pvp/src/Pvp/FireGate.cpp:11-24` | 否 | 否 | 取消（D90：FireGate 退役） |
| C16 | 移除 AppleClang 的 -fexperimental-library | 未解決 | `apps/object_fps_pvp/CMakeLists.txt:30-32`<br>`tests/object_fps_pvp/CMakeLists.txt:253-254` | 否 | 否 | re-evaluate 之後 |

## D. 暫定決定

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| D-LOG | 08c log 行族 v1（ingress 統計行與 match-ingress.jsonl）沿用或改版 | 暫定 | `apps/object_fps_pvp/gateway/ingress_statistics.go:9-24、:386-396`<br>`apps/object_fps_pvp/gateway/server.go:836、:859、:863` | 可選 | 否 | log-contract |
| D58-1 | runtime link 失效時那個資料包的計數歸屬（D58 (1)） | 暫定 | `apps/object_fps_pvp/gateway/server.go:417-425`<br>`tests/object_fps_pvp/mutations.json:1768` | 否 | 否 | log-contract |
| D58-3 | F2b-1 改寫 F2a 測試斷言（D58 (3)） | 暫定 | `tests/object_fps_pvp/PvpMatchTests.cpp:1084`<br>`tests/object_fps_pvp/PvpMatchTests.cpp:1160-1164` | 否 | 否 | log-contract |
| D58-4 | 動作 ack 的計數單位 acks（D58 (4)） | 暫定 | `apps/object_fps_pvp/include/RetroFPS/Pvp/IngressStatistics.hpp:128`<br>`apps/object_fps_pvp/include/RetroFPS/Pvp/IngressStatistics.hpp:256` | 否 | 否 | log-contract |
| D58-5 | slack 樣本的單位 samples，以及只給 --ingress-trace 時的用法錯誤（D58 (5)） | 暫定 | `apps/object_fps_pvp/include/RetroFPS/Pvp/IngressStatistics.hpp:129`<br>`apps/object_fps_pvp/include/RetroFPS/Pvp/IngressStatistics.hpp:257-265` | 否 | 否 | log-contract |
| D58-8 | I3 的「拒絕線命令數」用寫出數（D58 (8)） | 暫定 | `build/acceptance/object_fps_pvp/ingress_evidence.py:111-117`<br>`build/acceptance/object_fps_pvp/ingress_evidence.py:605-610` | 否 | 否 | log-contract |

## E. 量測與可觀測性

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| E2 | 需要 wire 資料的紀錄（原本「併入第 11 批」） | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:384`<br>`docs/object_fps_pvp/plans/v7.1/README.md:86` | 要 | 否 | cause-records |
| E3 | 量測工具時間基準統一（t_frame 與 t_c） | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/MovementTrace.hpp:16-17`<br>`docs/object_fps_pvp/plans/v7/09b-phase-tracking.md:1069` | 否 | 否 | re-evaluate 之後 |
| E4 | network 案例的遺失種子是固定的 | 未解決 | `build/acceptance/object_fps_pvp/action_probe.py:230`<br>`build/acceptance/object_fps_pvp/impaired_network.py:156-157` | 否 | 否 | observability-tools |
| E5 | probe 起點的幀相位（D46⑦） | 未解決 | `build/acceptance/object_fps_pvp/gameplay_action.hpp:109`<br>`build/acceptance/object_fps_pvp/gameplay_action.hpp:170` | 否 | 否 | re-evaluate 之後 |
| E1 | 第 10 批「觀測缺口清單」1～7 在 v7 之後的剩餘 | 部分（a700b66、6abd35a、8a36855、d49ee1e、f535b81） | `docs/object_fps_pvp/plans/v7/HANDOFF.md:385-392`<br>`docs/object_fps_pvp/plans/v7/08c-drop-report.md:763` | 可選 | 否 | log-contract＋observability-tools |
| E1-1 | 觀測缺口 1：Gateway 的丟棄沒有計數或 log | 已解決（8a36855、a700b66、6abd35a） | `apps/object_fps_pvp/gateway/ingress_statistics.go:9-24`<br>`apps/object_fps_pvp/gateway/server.go:836、:859、:863` | 可選 | 否 | 已解決 |
| E1-2 | 觀測缺口 2：Match 的 host_accepted 只記新收下的序號 | 部分（d49ee1e、f535b81） | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:221-226`<br>`apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:243` | 可選 | 否 | log-contract |
| E1-3 | 觀測缺口 3：相位決策（首次決定、視窗修正、late 修正與幅度）不寫進 trace | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/MovementTrace.hpp:16-17、:93-105`<br>`build/acceptance/object_fps_pvp/command_evidence.py:117-119、:126-127、:136-137` | 否 | 否 | observability-tools |
| E1-4 | 觀測缺口 4：Gateway 丟掉非 active 玩家的結果，不寫 log | 已被取代 | `apps/object_fps_pvp/gateway/action_delivery.go:416-425`<br>`docs/object_fps_pvp/plans/v7/08c-drop-report.md:98` | 否 | 否 | 已被取代 |
| E1-5 | 觀測缺口 5：match.log 的 tick_late 漏掉 Tick 間隔 | 未解決 | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:507-510`<br>`apps/object_fps_pvp/src/Pvp/NetworkStatistics.cpp:96-111` | 否 | 否 | log-contract |
| E1-6 | 觀測缺口 6：TimerBaseline 只在 probe 連線前量一次 3 秒 | 未解決 | `build/acceptance/object_fps_pvp/timer_baseline.hpp:3-5、:21`<br>`build/acceptance/object_fps_pvp/gameplay_action.hpp:75；quad_main.cpp:115` | 否 | 否 | observability-tools |
| E1-7 | 觀測缺口 7：Match lane（IpcHost）的送出間隔沒有統計欄位 | 未解決 | `apps/object_fps_pvp/src/Pvp/IpcHost.cpp:28、:58-61`<br>`apps/object_fps_pvp/src/Pvp/IpcHost.cpp:145-161` | 否 | 否 | log-contract |

## F. 平台與雜項

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| F1 | ClientConnection 關閉時阻塞約 3 秒（httplib） | 未解決 | `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:50-52`<br>`apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:154-157` | 否 | 否 | re-evaluate 之後 |
| F2 | LogFile 非同步化 | 未解決 | `apps/object_fps_pvp/src/Pvp/LogFile.cpp:134-139`<br>`apps/object_fps_pvp/src/Pvp/LogFile.cpp:97-109` | 否 | 否 | re-evaluate 之後（和 TT-2 一起） |
| F3 | AssetWatcher 的 detectedNs 改用 Engine 的時間基準 | 未解決 | `engine/asset/src/AssetWatcher.cpp:168-172`<br>`engine/asset/include/engine/asset/hot_reload/AssetChange.hpp:34` | 否 | 否 | re-evaluate 之後 |
| F4 | D30 門檻可能被逐漸變長的發布間隔逐級放大 | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/ClientSimulationRole.hpp:54-65`<br>`apps/object_fps_pvp/src/Pvp/ClientSimulationRole.cpp:55-58、:66-70` | 否 | 否 | re-evaluate 之後 |
| F5 | 期限前自旋（忙等） | 未解決 | `engine/time/include/engine/time/Waiter.hpp:53-65`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:458` | 否 | 否 | re-evaluate 之後 |
| F6 | App Nap（Spaces、縮小時的斷線） | 未解決 | `docs/object_fps_pvp/plans/v7/05-30fps-comparison.md:107、:161`<br>`docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:136、:197` | 否 | 否 | re-evaluate 之後 |
| F7 | Windows Match 的 Tick 與 IPC 精度 | 未解決 | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:499-510`<br>`engine/time/include/engine/time/Waiter.hpp:27-29、:60-65` | 否 | 否 | re-evaluate 之後 |

## G. 文件

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| G1 | v5／v6 缺陷版要不要重新發佈 | 已解決（eca577f） | `docs/object_fps_pvp/plans/v6/STABLE_BASELINE.md:3`<br>`docs/object_fps_pvp/plans/v5/STABLE_BASELINE.md:3` | 否 | 否 | 已解決 |
| G2 | 「時鐘到網路路徑」系列紀錄（5 集） | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:437`<br>`docs/object_fps_pvp/plans/v7.1/README.md:108` | 否 | 否 | re-evaluate 之後 |

## 從 v7 移入、初始清單沒列的項目

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| M0 | M0 影子量測與 FireGate 常數的完整推導（合成時間線 1～10 項改寫） | 部分（6587030） | `git worktree list`<br>`git log HEAD..m0-probe-5ef8a89` | 否 | 否 | 取消（D90：A1 讓 FireGate 退役） |
| TT2-B10 | TT-2（Engine 的 GYO::Trace）＋原第 10 批（任務 8 的紀錄；日誌定為產品 Data Contract） | 部分（10003b9） | `docs/architecture/plans/time-threads-trace/README.md:4`<br>`docs/architecture/plans/time-threads-trace/README.md:26` | 可選 | 否 | tt2-trace＋log-contract |
| IP34-B12 | IP-3、IP-4（Engine 的 SDL 隔離）＋原第 12 批（產品、probe、產品測試不再直接用 SDL） | 未解決 | `docs/architecture/plans/input-and-present/README.md:42-43`<br>`engine/platform/sdl/CMakeLists.txt:11-14` | 否 | 否 | re-evaluate 之後 |
| LAN15 | 原第 15 批：Windows LAN 場次（含 Mac Gateway↔Windows Client 偏移與漂移 ≥15 分鐘）。更正：wire 需求在 P3。 | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:425-427`<br>`docs/object_fps_pvp/plans/v7/09-firegate-c2.md:132` | 否 | 否 | re-evaluate 之後（可選的評估資料） |

## 盤點新發現（N-）

| 代號 | 項目 | 狀態 | 證據（c742836） | 改 wire | 動 digest | 處置 |
|---|---|---|---|---|---|---|
| N-netarch-ambiguity | network-architecture 原第 230-233 行的條文歧義（事故後續表）（合併：N-netarch-230-ambiguity） | 部分（082d4b6） | `docs/object_fps_pvp/network-architecture.zh-Hant.md:237-239`<br>`docs/object_fps_pvp/network-architecture.zh-Hant.md:240-248` | 否 | 否 | edge-contract |
| N-dropreport-enum-drift | design3 B 的原因列舉早於 08c 定案，和 HEAD 的原因集合不一致 | 未解決 | `build/target/_build/test/logs/pvp-v7-infoflow-plan-20261010/batch2/design3-counts-pv7.md`<br>`apps/object_fps_pvp/gateway/ingress_statistics.go:120-170` | 要 | 否 | pv7-design＋drop-report |
| N-leave-reason | Gateway 送給 Match 的 Leave 不帶原因 | 未解決 | `apps/object_fps_pvp/protocol/runtime_v6.proto:21`<br>`apps/object_fps_pvp/gateway/server.go:29-30` | 可選 | 否 | cause-records |
| N-action-nack | Match 整批拒收動作時不回覆（沒有 NACK）（合併：N-action-reject-nack）。更正：pv6 已有 RuntimeError；要讓 Client 停止重送才需改 wire。 | 未解決 | `apps/object_fps_pvp/src/Pvp/IpcHost.cpp:384`<br>`apps/object_fps_pvp/gateway/server.go:652-654` | 可選 | 否 | cause-records |
| N-stale-line-refs | pv7 相關文件引用的行號已漂移 | 未解決 | `docs/object_fps_pvp/plans/v7/README.md:36`<br>`docs/object_fps_pvp/plans/v7/INVENTORY.md:98` | 否 | 否 | plan-sync-docs |
| N-incident-table-stale | 事故紀錄「處置與行動項目」表的狀態過時（合併：N-incident-closeout） | 部分 | `docs/object_fps_pvp/plans/v7/Incident/2026-10-10-gateway-silent-drop.md:214-222`<br>`docs/object_fps_pvp/plans/v7/08c-drop-report.md:695-730` | 否 | 否 | incident-close |
| N-firegate-negative-shift | FireGate 忽略負的 phaseShift，late slew 期間 earliest 可能估晚 | 未解決 | `apps/object_fps_pvp/src/Pvp/FireGate.cpp:18-19`<br>`apps/object_fps_pvp/include/RetroFPS/Pvp/FireGate.hpp:66-67` | 否 | 否 | 取消（D90：FireGate 退役） |
| N-shot-earliest-on-wire | 射擊不帶 Client 的 earliest／點擊步位，R−E 在 GUI 與實際對戰量不到 | 未解決 | `apps/object_fps_pvp/protocol/client_v6.proto:127-134`<br>`docs/object_fps_pvp/plans/v7/09-firegate-c2.md:15` | 可選 | 否 | 已被取代（A1 的命令步欄位） |
| N-shot-origin-lag | 射擊在到達後的 Tick 以當時的權威位置判定，射手起點比 Client 畫面落後約 1～2 步〔推論〕 | 未解決 | `apps/object_fps_pvp/src/Pvp/PvpMatch.cpp:147`<br>`apps/object_fps_pvp/src/Pvp/PvpMatch.cpp:321-322` | 可選 | 會 | a1-shot-step |
| N-analyzer-pv6-pin | 凍結分析器與驗收工具寫死協議版本 6（合併：N-frozen-judges-pv6-locked、N-analyzer-v7-overlays） | 未解決 | `build/acceptance/object_fps_pvp/acceptance_util.py:8`<br>`build/acceptance/object_fps_pvp/gameplay_evidence.py:140-141` | 否 | 否 | version-errors（凍結 pv7 版分析器） |
| N-analyzer-literal-105 | 驗收分析器寫死 backlog 門檻 105，和產品常數不連動 | 未解決 | `apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp:56`<br>`build/acceptance/object_fps_pvp/command_evidence.py:201,:423` | 否 | 否 | re-evaluate（和 A5 一起） |
| N-token-test-gaps | token 折讓式的測試缺口：存活突變與「每日曆秒 ≤61」斷言 | 未解決 | `docs/object_fps_pvp/plans/v7/09a-input-send-pinning.md:99`<br>`apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:616-618` | 否 | 否 | re-evaluate 之後 |
| N-osyncstream-experimental | std::osyncstream 也依賴 -fexperimental-library（C16 的前提要更正） | 未解決 | `apps/object_fps_pvp/match_main.cpp:14、:26-28、:98`<br>`apps/object_fps_pvp/src/Pvp/IpcHost.cpp:22、:33` | 否 | 否 | re-evaluate 之後（和 C16 一起） |
| N-tt1-l2-wake-record | TT-1 L2 的起床精度記錄程式未合入（分支 claude/tt1-l2-wake-record）（合併：N-tt1-l2-branch） | 未解決 | `git: a8f9535 不是 HEAD 的祖先；分支 claude/tt1-l2-wake-record 存在`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:451-452` | 否 | 否 | re-evaluate 之後 |
| N-wait-backend-not-logged | 產品日誌沒有記錄 Waiter 的後端 | 未解決 | `engine/time/include/engine/time/Waiter.hpp:29、:84`<br>`engine/threads/include/engine/threads/RoleThread.hpp` | 否 | 否 | log-contract |
| N-d37-log-location | D37：日誌的位置（留在執行檔旁）仍待確認 | 暫定 | `apps/object_fps_pvp/main.cpp:37-49`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:138` | 否 | 否 | log-contract |
| N-hud-source-tick | HUD 來源 tick 不在觀測 API（分析器只能用區間規則） | 未解決 | `docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:186` | 否 | 否 | re-evaluate 之後 |
| N-gateway-race-flaky | Gateway 的時間敏感測試在高負載下不穩（合併：N-gateway-cadence-test-flaky） | 未解決 | `docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:190`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:50` | 否 | 否 | re-evaluate 之後 |
| N-sigint-background-client | 腳本在背景啟動的 Client 忽略 SIGINT（合併：N-client-sigint-background） | 未解決 | `docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:191` | 否 | 否 | re-evaluate 之後 |
| N-v7-devlog-gap | v7 第 08 批以後沒有 dev_log | 已解決（0c2ef14） | `docs/object_fps_pvp/plans/v7/README.md:148`<br>`docs/dev_logs/` | 否 | 否 | 已解決 |
| N-branch-sync-pr75 | 規劃分支 HEAD 和 origin/master 已經分岔（PR #75） | 未解決 | `git log c742836..origin/master`<br>`git diff --stat c742836 origin/master` | 否 | 否 | plan-sync-docs |
| N-lan3p-v7-kit | v7 的 3 人 LAN 測試改期（D83），要不要在 v7.1 之前跑 | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:128`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:126` | 否 | 否 | 取消（D91：不跑） |
| N-v7-release-tag | v7 沒有發行，GitHub 的 Latest 仍是缺陷版 v1.1.0 | 未解決 | `docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:203-208`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:125` | 否 | 否 | 取消（D91：v7 不發行） |
| N-8ms-client-crosscheck | C1 的 8 ms 狀態與 Client 端主機狀態的橫向對比 | 部分（36c77d4） | `docs/object_fps_pvp/plans/v7/HANDOFF.md:450`<br>`docs/object_fps_pvp/plans/v7/09-firegate-c2.md:788-797` | 否 | 否 | re-evaluate 之後 |
| N-phase-estimator-wan-noise | 相位估計器在 WAN 雜訊下的穩定性（v6 的候選，v7.1／v8 都沒收） | 未解決 | `docs/object_fps_pvp/plans/v6/HANDOFF.md:649`<br>`docs/object_fps_pvp/plans/v6/HANDOFF.md:327` | 可選 | 否 | re-evaluate |
| N-rotation-discarded-detail | rotation_discarded 沒有逐筆紀錄，原因也沒拆 | 未解決 | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:433-441`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:490` | 否 | 否 | log-contract |
| N-conflict-window-permanent-loss | 衝突窗口併進主線造成的永久損失（protocol-v6 已知缺口 9） | 未解決 | `docs/object_fps_pvp/protocol-v6.zh-Hant.md:98`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:487` | 可選 | 否 | re-evaluate |
| N-silent-branches-assert | 推論走不到的無聲分支改成 assert（Programmer Error） | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:491`<br>`apps/object_fps_pvp/gateway/server.go:638-641` | 否 | 否 | re-evaluate 之後 |
| N-09b-followup-measures | 09b §5 裡沒有對應使用者代號的量測與方法項 | 未解決 | `docs/object_fps_pvp/plans/v7/09b-phase-tracking.md:1373`<br>`docs/object_fps_pvp/plans/v7/09b-phase-tracking.md:1376` | 否 | 否 | re-evaluate |
| N-phase-1-7-items-3-6 | 09b 1-7 節的第 3、6 項已知限制（B6 沒涵蓋） | 未解決 | `docs/object_fps_pvp/plans/v7/09b-phase-tracking.md:1176`<br>`docs/object_fps_pvp/plans/v7/09b-phase-tracking.md:1179` | 否 | 否 | re-evaluate |
| N-firegate-unmeasured-residuals | FireGate 的盲點：GUI 取不到 earliest、spread 的假設沒量過 | 未解決 | `docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:165`<br>`docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:182-184` | 否 | 否 | 取消（D90：FireGate 退役） |
| N-trace-overwrite-on-restart | 重新啟動 Match 會覆寫前一段的 movement trace 與 ingress 細節檔 | 未解決 | `docs/object_fps_pvp/plans/v7/LAN_TEST.md:94` | 否 | 否 | observability-tools |
| N-v8-wire-pv8 | v8 需要改 wire 的候選（排 pv8，v7.1 不預留） | 未解決 | `docs/object_fps_pvp/plans/v8/README.md:41`<br>`docs/object_fps_pvp/plans/v8/README.md:43` | 否 | 否 | 已被取代（D87：pv7 只支持 hitbox） |
| N-v8-deps-on-v71 | v8 依賴 v7.1 的項目：IP-3 與 A1 | 未解決 | `docs/object_fps_pvp/plans/v8/README.md:27`<br>`docs/object_fps_pvp/plans/v8/README.md:40` | 否 | 否 | re-evaluate 之後（IP-3） |
| N-firegate-repush-other-conditions | FireGate 重推條件 1、2、3、5、6、9 也會被 v7.1 的其他批次觸發；條件 4 同時是心跳／時間回聲的設計約束（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/09-firegate-c2.md:127`<br>`docs/object_fps_pvp/plans/v7/09-firegate-c2.md:128-129` | — | — | 取消（D90：FireGate 退役，條件 1～11 結案） |
| N-firegate-cond11-rewrite-gate | 08c 對重推條件 11 的改寫建議尚未核准（決策關卡）（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/08c-drop-report.md:761`<br>`docs/object_fps_pvp/plans/v7/09b-phase-tracking.md:1354` | — | — | 取消（D90：FireGate 退役） |
| N-wire-clock-domain | pv7 上時間戳的時鐘域沒有定義（Go 單調時鐘、C++ steady_clock、SDL ticks 不同域）（完整性評審補列） | 未解決 | `engine/time/include/engine/time/MonotonicClock.hpp:8-18`<br>`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:219` | — | — | pv7-design＋heartbeat-sync |
| N-input-spoofing-auth | Input 沒有逐包驗證：偽造來源 endpoint 加上 session id，就能讓受害者被速率限制；未授權計數也可偽造（完整性評審補列） | 未解決 | `services/gyo_gateway/session/session.go:80-92`<br>`services/gyo_gateway/session/session.go:117-125` | — | — | 取消（D90：還沒到那個規模） |
| N-gateway-receive-time-on-forwarded | Gateway 的收包時刻（dwell）要不要也放進轉送的 PlayerInput／Actions，而不只放在 GatewayDropReport 的樣本（完整性評審補列） | 未解決 | `docs/object_fps_pvp/protocol-v6.zh-Hant.md:94`<br>`docs/object_fps_pvp/plans/v7/08-client-worker-asio.md:253` | — | — | pv7-design＋cause-records |
| N-gateway-drop-quality-policy | Gateway 造成的替代要不要免除連線品質判定（D49 ⑤ 的政策），以及 GatewayDropReport 違規時 Match 是否關閉 link（完整性評審補列） | 未解決 | `design3-counts-pv7.md B 節「Match 怎麼用」— 不進 TrackSlack、不進品質判定；「Gateway 造成的替代要不要免除品質判定」另由使用者決定`<br>`design3-counts-pv7.md B 節「回報遺失或被合併時怎麼計數」— 序號沒有遞增、累計值倒退等視為 Gateway 程式錯誤，關閉 link` | — | — | pv7-design |
| N-quality-reference-age-sampling | 連線品質的參考年齡只從帶新命令的窗口取樣（08c 留下「要另外決定」）（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/08c-drop-report.md:56`<br>`apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:236-241` | — | — | re-evaluate |
| N-client-side-loss-counters | Client 端的樣本流失、本地丟棄與送出失敗沒有原因計數（完整性評審補列） | 未解決 | `docs/object_fps_pvp/protocol-v6.zh-Hant.md:95`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:491` | — | — | log-contract |
| N-downstream-drops-report | 下行丟棄 D1、D2（Match 不知道自己的 snapshot 被拒）與 D5、D6（UDP 寫出錯誤、control 佇列滿）的回報與計數（完整性評審補列） | 未解決 | `docs/object_fps_pvp/protocol-v6.zh-Hant.md:93`<br>`docs/object_fps_pvp/plans/v7/08c-drop-report.md:763` | — | — | pv7-design＋drop-report |
| N-stale-seq-actions | 舊序號的 Actions／其他種類仍由 Gateway 直接丟棄（★ 只套用在 Input）（完整性評審補列） | 未解決 | `docs/object_fps_pvp/protocol-v6.zh-Hant.md:90`<br>`docs/object_fps_pvp/plans/v7/08c-drop-report.md:723-724` | — | — | pv7-design＋drop-report |
| N-correct-invariant-assert | D68 ①：Correct() 的不變式要不要改成 GYO_ASSERT，決定一直沒有做（完整性評審補列） | 未解決 | `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp:98-106`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:113` | — | — | re-evaluate 之後（修正批之前） |
| N-late-count-detector-increment | late 修正的計數方式：之後的 L2 改用凍結 D48 偵測器的計數器增量，不用 judge_l2.py 的最大值（D63 ④）（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/08c-drop-report.md:733`<br>`docs/object_fps_pvp/plans/v7/09b-phase-tracking.md:38` | — | — | observability-tools |
| N-d44-network-detector-narrow | D44 回歸的網路母體偵測範圍很窄：E3 只比 6 秒停頓前後的 epoch（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:125`<br>`docs/object_fps_pvp/plans/v7/16-integration-acceptance.md:188` | — | — | observability-tools |
| N-injection-official-mode | Gateway→Match 的 IPC 延遲注入（D50 的 b1 替換 IpcPause）要變成常設回歸時，要升級為凍結工具的正式模式（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:95`<br>`docs/object_fps_pvp/plans/v7/Incident/2026-10-10-gateway-silent-drop.md:219` | — | — | observability-tools |
| N-log-contract-vs-frozen-analyzers | 日誌 Data Contract 改版會碰到凍結分析器解析的行；P2-log 的統計行還是非正式格式（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/08c-drop-report.md:77`<br>`docs/object_fps_pvp/plans/v7/p2-log-network-statistics.md:42-44` | — | — | log-contract |
| N-ip3-review-and-migration | IP-3／IP-4 的前置：公開介面 xhigh 審查（D39）、Engine PLAN 未撰寫、ui_editor 與未啟用產品的遷移清單（D33）（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/HANDOFF.md:140`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:82` | — | — | re-evaluate 之後（和 SDL 隔離一起） |
| N-v7-skipped-acceptance | v7 跳過或沒做的驗收，v7.1 升格時要不要補做（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:153-167`<br>`docs/object_fps_pvp/plans/v7/STABLE_BASELINE.md:139` | — | — | re-evaluate 之後（STABLE_BASELINE v7.1 前決定） |
| N-respawn-reject-volume | respawn 案例的 epoch_old 每跑次 828（預測約 147），「單位與來源之後核對」一直沒有核對（完整性評審補列） | 未解決 | `docs/object_fps_pvp/plans/v7/08c-drop-report.md:719` | — | — | re-evaluate |
| N-baseline-worktrees | v7.1 新舊對比要用的 before 樹與本機未推送分支，要決定保留哪些（完整性評審補列） | 未解決 | `git worktree list`<br>`docs/object_fps_pvp/plans/v7/HANDOFF.md:125` | — | — | plan-sync-docs（HANDOFF 未結事項） |

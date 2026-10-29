# PvP v7.1 穩定基線

> **狀態：升格完成（2026-10-21）。** master `5ee499a`（PR #82 合併）的 CI #541 全部通過，v7.1 成為穩定基線（D157）。v7.1 沒有專門的整體驗收批；驗收由兩條 PR 線（pv7-line：PR #78；after-pv7：PR #82）各批的 L1、L2、L3 構成，最後一次整體回歸是 `d44-regression-pv7`。

> **發布（2026-10-29）：** `object_fps_pvp` v1.2.0（[release](https://github.com/yojinn-io/GYO-Engine/releases/tag/object_fps_pvp-v1.2.0)，master `62fd7ebf`）。本基線之後加入的內容：OS 子系統 10 批（其中 4 批已出貨、未經功能驗收，D219）、windows-long-path、quick-close、worker 探針的修正、人物模型 Player_V20 與一人稱 nailer_v2。發布時的已知限界見 [release-notes-draft.md](release-notes-draft.md)：Windows LAN 場次（lan15）只跑過一次且確認不了；Windows 上 worker 驗收不算發行條件；HUD 仍顯示 `MARK23`、第三人稱仍是舊手槍。下面的內容是 2026-10-21 升格時的紀錄，原樣保留。

撰寫日期：2026-10-21。Owner：`object_fps_pvp`。
範圍為 **macOS 26.7.1、Intel x86_64（i9-9980HK）、SDL 視訊驅動 cocoa、Metal、同機三角色**（L1、L2、L3 都在這台）。Windows 只有 2026-10-16 的一次臨時 LAN 測試（pv7 完成之前的樹），Linux、macOS arm64 只有 CI 的 L1（見「平台」）。
v7.1 **不標「缺陷版」**：沒有任何發現推翻設計前提。已知限制都有範圍與歸屬（v8 或「re-evaluate 之後」）。
v5、v6 的穩定基線是 2026-10-10 事故之前的版本，已標「缺陷版」（D69：[v6 穩定基線](../v6/STABLE_BASELINE.md)）；[v7 穩定基線](../v7/STABLE_BASELINE.md) 原樣保留。

## 已知問題（摘要）

- **完整 pv7 的 Windows 實機未驗證。** 唯一的 Windows 實機紀錄是 2026-10-16 的全 Windows LAN 臨時測試（`16216ff`，heartbeat-sync 之前的樹），可以穩定遊玩；之後的 pv7 各批（三個原樣轉送、A1、token bucket 限速）沒有在 Windows 實機跑過。`lan15-windows` 仍在「re-evaluate 之後」。對外的玩家用 Windows，所以這是本基線最大的未驗證範圍。
- **短延遲之後的撤回殘餘（B6）。** 一次 45～60 ms、持續 150 ms 的 Client→Gateway 延遲，可能留下約 +25 ms、約 4 秒的延遲（`re-evaluate` 補量 4/8 個單位）。這是相位追蹤自己的時序取捨，交給 v8 重新規劃（D155）。

## 固定契約與版本

- **wire＝pv7**（`ProtocolVersion`＝7）：[protocol-v7](../../protocol-v7.zh-Hant.md) 自 PR #78 合併（`0fabb52`）起生效。v7.1 是 pv7 唯一一次 wire 變更；定稿之後任何 wire 的新增、刪除、改號、改型別或改語意，都要先徵求使用者同意。
- **Gateway 是無狀態邊緣**（D94）：輸入、結果、動作原樣轉送，不持有 Match 決策狀態的副本，不做跨訊息、依內容的判斷，也不排程重送。約束寫在 [network-architecture](../../network-architecture.zh-Hant.md) 的 Design Invariants「Gateway」一節，並有 Go 的狀態欄位白名單守衛（`edge-contract`）。
- **權威結果只因 A1 改變**：射擊照命令步判定（`a1-shot-step`，D136）。兩樹比較 36 個情境中只有 `combat-magazine/corridor` 不同，依宣告重產 golden（舊檔保留為 `tests/object_fps_pvp/fixtures/authority_golden.pv7-checkpoint.txt`，D137），並新增 `combat-step/corridor`；golden 31 筆。after-pv7 沒有改權威結果。
- **FireGate 退役**（D90）：`ShotGate` 取代 FireGate，v7 的 guard 2 ms、spread 7 Tick 兩個常數連同 M0 一起消失。
- **日誌 Data Contract v1**（[log-contract](../../log-contract.zh-Hant.md)，D142）：A 級行族由寫出端釘住、凍結讀者守住；取代 protocol-v7 §1 原本的日誌規則。
- **JSONL 紀錄檔**：移動紀錄（`*-commands.jsonl`）是 schema 3，多了 Client 相位追蹤的 `phase_decision`（D152）；`match-shots.jsonl` 是版本 2（A1 之後的判定紀錄）；`match-ingress.jsonl` 是版本 1。
- 版本號：v7.1＝遊戲版本，pv7＝網路協議版本。

## v7.1 交付什麼

v7.1 的定位是 hotfix：2026-10-10 Gateway 悄悄丟命令的事故、2026-10-15 Gateway 狀態漂移的設計事故，加上量不到成因的檢測問題，三者混在一起，所以 v7.1 一次把架構與契約穩定下來，讓 v8 只做功能（D87、D88）。兩份事故紀錄都已結案（`incident-close`）。

| 線 | 批次 | 內容 |
|---|---|---|
| pv7-line（PR #78，`0fabb52`） | `pv7-design` | protocol-v7 定稿（§2a 的 IPC 送達語意）；Gateway 回到無狀態邊緣（D94） |
| | `proto-rename-v7`、`version-errors` | proto 改名；版本 6→7，版本不符時三端都有明確錯誤；凍結 pv7 版分析器 |
| | `heartbeat-sync`、`time-echo` | runtime link 心跳對時；Client↔Gateway 時間回聲（只作診斷，不驅動判斷） |
| | `drop-report` | `GatewayDropReport`：Gateway 依原因回報每一種丟棄，Match 知道 Gateway 丟了什麼 |
| | `edge-results-resend`、`edge-input-passthrough`、`edge-results-passthrough`、`edge-actions-passthrough` | Match 在 Client ACK 之前重送結果；輸入、結果、動作原樣轉送，刪除 Gateway 的去重、拒絕線、帳本與動作視窗 |
| | `edge-analyzers`、`edge-contract`、`edge-rate-limit` | ingress 分析器為直通重新凍結；無狀態邊緣的條文與守衛；通用層的限速改成 token bucket（D121） |
| | `cause-records`、`hitbox-fields` | dwell、Client 序號、停留時間、動作拒收回覆、Leave 原因；骨骼 hitbox 的 wire 欄位與影子紀錄 |
| | `pv7-checkpoint`、`a1-shot-step`、`pv7-merge` | A1 前的整體回歸；射擊照命令步判定、FireGate 退役（唯一的行為改變）；合併 |
| after-pv7（PR #82，`5ee499a`） | `tt2-trace` | Engine 的 `GYO::Trace`（有上限的紀錄 sink、通知喚醒的背景寫檔）與 Base 的日誌 facade |
| | `log-contract` | 日誌 Data Contract v1 與它的紀錄（Tick 停頓、IPC lane、wait backend、受擊效果、結束紀錄等） |
| | `dead-letter-position` | 確認 D57（日誌與丟棄回報兩份契約、一套詞彙），`drop_` 鍵的分類雙向守住 |
| | `d44-regression-pv7` | pv7 之後的 D44 整體回歸（下節） |
| | `incident-close` | 兩份事故紀錄結案 |
| | `observability-tools` | late 計數的共用函式、網路重設分類器、遺失種子參數化、相位決策寫進 trace（schema 3）、跑次後的計時器基準 |
| | `re-evaluate` | 用 pv7 的數據判斷待評估項目還需不需要繼續（[re-evaluate](re-evaluate.md)） |

## 移到 v8 或之後的項目

| 項目 | 去向 | 決定 |
|---|---|---|
| B6：延遲結束後撤回多加的領先（重新規劃，不沿用 09b 的 S／M；09b 1-7 第 3 點、E1-2 的逐筆到達事件隨它） | [v8](../v8/README.md)「v7.1 交過來的項目」 | D155、D157 |
| A4：延遲位移（產生→執行約 38 ms），手感取捨 | v8 | D154 |
| N-analyzer-literal-105：backlog 門檻 105 與視窗 30 在 6 處手動維持 | v8 | D154 |
| N-phase-estimator-wan-noise：相位估計器在真實互聯網雜訊下的穩定性 | 有 WAN 測試時 | D154 |
| `after-re-evaluate`：INVENTORY 處置為「re-evaluate 之後」的 37 項（SDL 隔離 IP-3／IP-4、Windows LAN 場次 LAN15、時間基準統一 E3、code smell 等） | 使用者排序 | README |
| 取消（附重開條件）：B7、A5（照設計保留，連同門檻 105 重推與快速正向回收）、B8、N-rotation-discarded-detail、slack 樣本流失計數、Gateway 的週期性 IPC 寫出延遲 | — | D154、D155、D157 |

## 來源與產物

| 項目 | 值 |
|---|---|
| 產品程式 | master `5ee499a`（PR #82 合併）。最後一次改非文件的 commit 是 `d5a4cec`；`git diff --stat d5a4cec 5ee499a -- . ':!docs' ':!*.md'` 為空 |
| pv7-line 合併 | master `0fabb52`（PR #78），master CI #461（run 39274351098）19 個 job 全 success |
| CI 與 snapshot | master `5ee499a` 的 CI #541（run 39577489227）19 個 job 全 success；`object_fps_pvp-snapshot-20261021-5ee499a`、`tools-snapshot-20261021-5ee499a`（prerelease） |
| 最後一次程式變更的 L1 | #532（run 39566847523，`d5a4cec`）四平台通過 |
| 本機建置（主 checkout，產品程式＝`5ee499a`） | Client `abbbd60da4e29de219767e975984b446d521a0819cb1951b3c4921ba0d53941b`；Match `db8c4e4e91628a6eef696d5a669dec00356a4143928c1e96450e695438586cfb`；Gateway `89d949ce3bb1399b912e51956340cd16fb133e29eeddb0860970b076846a57db`（Gateway 在 `cf40497` 建置，之後 Go 程式沒有改）。對外的包以 CI 的 snapshot 為準 |
| 訓練地圖 `pvp_arena.json` | `0026013c731a7d7317ce6be4f84f897b91fa1b807c9621d60fb608570f1775a5`（和 v6、v7 相同） |

## 驗收結果

### L1

各批的 L1 都是 CI 的四平台（windows-x64、linux-x64、macos-x64、macos-arm64）。只在 CI 的 macos-x64（Rosetta）出現過 `N-movement-recovery-flake` 兩次（`ed3481c` 與 `3346379`），兩次重跑都通過，依平台優先序只記錄；已加上失敗時才輸出的觀測（`98d3c45`）。

### L2（各批）

| 批次 | session | 結果 |
|---|---|---|
| `edge-input-passthrough` | `pvp-v71-edge-input-l2-20261017b` | 98/98 通過（第 1 次停在 I1 的關閉競態，依 D110 只改 I1 規則後重跑） |
| `edge-results-passthrough` | `pvp-v71-edge-results-l2-20261017b` | 120/120 通過（第 1 次因主機干擾停下） |
| `edge-actions-passthrough` | `pvp-v71-edge-actions-l2-20261018` | 停在第 102 項：E1＝1，根因是 Gateway 的固定窗口限速在停頓後丟掉合法輸入（K1）。改成 token bucket（D121）後併入 `edge-rate-limit` 的 L2 |
| `edge-rate-limit` | `pvp-v71-edge-rate-limit-l2-20261018` | 168 項 0 停止，K1a、K1b 成立 |
| `pv7-checkpoint` | `pvp-v71-pv7-checkpoint-l2-20261019` | 232/232、0 停止；判定工具的標籤錯誤依 D135 以重新標記的判定為準，C1～C3 成立 |
| `a1-shot-step` | `pvp-v71-a1-l2-20261019` | 238/238，`passed`＝true；C1／C2 最大 +0.213／+0.149 ms；clean 的權威 Cooldown 0 |
| `tt2-trace` | `pvp-v71-tt2-l2-20261020` | 48/48，0 停止 |
| `log-contract` | `pvp-v71-log-contract-l2-20261020` | 只記錄（D144）：受控停頓的 `tick_overrun` 和 trace 重算的間隔差 34 µs |
| `d44-regression-pv7` | `pvp-v71-d44-l2-20261021c` | 197/197，290 個 case-run 與 60 Hz 長測 0 個 D44 事件（下節） |

### L3 與使用者測試

| 項目 | 結果 |
|---|---|
| `a1-shot-step` 的 L3（`pvp-v71-a1-l3-20261019-233447`，2 個產品 Client＋2 個 bot） | 兩位使用者 67＋46 發全部準時，保留 0，權威 Cooldown 0。使用者：「比之前好了好多，沒有那種黏滯感了。而且連點都比較平均」 |
| 全 Windows LAN 臨時測試（2026-10-16，`16216ff`，3 台 Windows 筆電、Wi-Fi，約 4 分 47 秒） | 可以穩定遊玩：斷線、逐出、`drop_*` 都是 0；替代命令 0.55%／0.37%／0。使用者回報一次受擊沒閃紅（之後由 log-contract 的受擊效果紀錄補上觀測）。只記錄，不是批次 |

## D44 整體回歸（`d44-regression-pv7`）

- **範圍**：master 上 pv7 之後的程式（三個原樣轉送、`GatewayDropReport`、A1、token bucket）。事件定義沿用 v7 第 16 批（E1～E3），另外加上 Client→Gateway 的持續延遲 x1、ramp。
- **結果：290 個 case-run 與 60 Hz 長測都是 0 個 D44 事件，讀法「未再現」。**

| 母體 | case-run | 事件 | 0 事件時的單側 95% 上限 | v7 第 16 批 |
|---|---|---|---|---|
| 遊戲（故障 66、clean 29） | 95 | 0 | 3.1% | 95，0 |
| 背壓 | 120 | 0 | 2.5% | 120，0 |
| 網路一般 | 30 | 0 | 9.5% | 30，0 |
| 網路 impairments | 15 | 0 | 18.1% | 15，0 |
| x1 | 15 | 0 | 18.1% | — |
| ramp | 15 | 0 | 18.1% | — |
| 60 Hz 長測 | 1 | 0 | — | 1，0 |

- 前兩次 session 都因環境停在前半（relay 的送出時序單次異常；主 checkout 被編輯器切換分支），保留為紀錄，不和第 3 次合併計數。
- 使用者的讀法：D44 是 Gateway 設計問題最早、最直接的徵兆。「解釋不了」的重設原因落在 Match 看不到的 Gateway 裡；看不到的參與者被消除之後，它就不再出現。
- 剩下的 A5（300 ms 型的延遲消失重設、backlog 門檻 105）是 Match 自己看得到、解釋得了的回收機制，照設計保留（D155）。
- 細節與限制：[d44-regression-pv7](d44-regression-pv7.md) 第 15 節。

## 平台

| 平台 | 狀態 |
|---|---|
| macOS Intel x86_64（Metal） | **實機驗證**：各批 L1 本機、L2、L3 |
| Windows x64 | 只有 2026-10-16 的臨時 LAN 測試（`16216ff`，完整 pv7 之前）；完整 pv7 未在實機驗證。CI 跑 L1 |
| Linux x64 | 未驗證；CI 跑 L1 |
| macOS arm64 | 未驗證；CI 跑 L1 |
| 兩機時鐘漂移 | 有了 pv7 的心跳對時與時間回聲，但還沒有在兩台機器上量（LAN15） |

## 跳過與未驗證

- v7 跳過的驗收**不補做**（D157），照樣列為「跳過（使用者決定）」：GUI 三輪（各 120 秒）、144 Hz 長測（1808 秒）、完整驗收的 4 人一輪；v7 L3 的「關閉視窗後重新加入」與 `room_full`（第 5 人加入）。L3 那兩項隨 Windows LAN 場次（LAN15）補。
- 真實互聯網（WAN）沒有任何測試（N-phase-estimator-wan-noise）。
- 筆電睡眠後的行為只做過一次只記錄的測試（`heartbeat-sync`：Client 醒來約 1 秒後斷線，歸為體驗優化）。

## 已知限制與殘餘

1. **相位追蹤**（細節：[re-evaluate](re-evaluate.md) 第 4 節）：
   - 100 ms 型的短延遲：撤回後殘餘約 +3.3 ms，約 4.2 秒恢復。
   - 150 ms 型：部分撤回，可能留下約 +25 ms、約 4 秒（B6，v8 重新規劃）。
   - 300 ms 型：backlog 重設，約 0.2～0.6 秒恢復、沒有 held；每次讓一個連線品質視窗判失敗（連續 3 個才踢人）。照設計保留（A5 取消，D155）。
   - 停頓後的 reseed 可能短暫領先不足，約 0.25 秒內自己恢復、替代 0（B9，已知限制）。
2. **延遲位移**：產生→執行約 38 ms（A4，v8；手感取捨，不是缺陷）。
3. **連線品質的參考年齡只從帶新命令的窗口取樣**：偏差方向是低估，每個 10 秒視窗最多 8.7%，L2 沒有踢出（已知限制）。
4. **衝突窗口**：Match 整則拒收衝突的輸入窗口，游標越過衝突的序號後恢復（pv6 缺口 9 的「永久損失」在 pv7 已不存在）。
5. **觀測的已知缺口**（D157 取消，附重開條件）：`rotation_discarded` 沒有逐筆紀錄；Client 端 slack 樣本的流失沒有計數；Gateway 沒有週期性的 IPC 寫出延遲（Match 端的 lane 寫出超時有記錄）。
6. **測試**：
   - `N-movement-recovery-flake`：只在 CI 的 macos-x64（Rosetta）偶發，只記錄；失敗時會輸出觀測。
   - `N-worker-probe-stale`：CTest `object_fps_pvp.worker`（label `network`，不在 CI 範圍）在 HEAD 失敗，mock Welcome 少了 `maximum_shot_damage`，另有一個等待逾時未查。**（基線之後由 `worker-probe-repair` 修好，`d2844a5`：等待逾時的原因是 mock snapshot 少了 `last_hit_part`；22 個 `worker` 突變重判全部 killed。）**
7. **其他**：沿用 [HANDOFF](HANDOFF.md)「未結事項」與 INVENTORY 處置為「re-evaluate 之後」的 37 項。

## 發行

- v7.1 沒有關於正式發行的決定；本文件只升格。
- tag 候選：**`object_fps_pvp-v1.2.0`**（v7 也是候選但沒有發行；上一版 `object_fps_pvp-v1.1.0`＝v6，已標缺陷版）。wire 從 pv6 改成 pv7，是否改用 major 版號，發行時再決定。
- 想試的人可以用 master 合併後自動產生的 snapshot（prerelease）：`object_fps_pvp-snapshot-20261021-5ee499a`。
- Publish 一律由使用者操作（[發行文件](../../../releasing.zh-Hant.md)第 9 節）。

## 怎麼重現

證據與腳本都在 `build/target/_build/test/logs/`（git 忽略，只在本機）。各批的工具、宣告、判定與重播方式寫在各批的計畫文件裡（README「after-pv7 各批」「pv7-line 各批」的連結）。最後一次整體回歸：`pvp-v71-d44-l2-prep-20261021/run_d44.py`、`judge_d44.py`，字面值與重播方式見 [d44-regression-pv7](d44-regression-pv7.md) 第 8、12 節。過去的 A／B 樹 worktree 由架構會話清理；刪除後，舊 session 的產物清單指向的執行檔不在，判定結果本身已經記錄在案。

## 證據索引

- 計畫與決策：[README](README.md)、[HANDOFF](HANDOFF.md)（D86～D157）、[INVENTORY](INVENTORY.md)。
- 事故：[2026-10-10 Gateway 悄悄丟命令](../v7/Incident/2026-10-10-gateway-silent-drop.md)、[2026-10-15 Gateway 狀態漂移](Incident/2026-10-15-gateway-state-drift.md)（第 11 節後日談）。
- 契約：[protocol-v7](../../protocol-v7.zh-Hant.md)、[log-contract](../../log-contract.zh-Hant.md)、[network-architecture](../../network-architecture.zh-Hant.md) 的 Design Invariants。
- 各批 L2 的 session 目錄見「驗收結果」；`re-evaluate` 的補量：`pvp-v71-reeval-map-20261021/`。

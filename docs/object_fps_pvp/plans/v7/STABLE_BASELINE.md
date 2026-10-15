# PvP v7 穩定基線

> **狀態：升格完成（2026-10-15）。** 機器驗收（16-L1、16-removal、16-L2＋16-L2b）全部合格；使用者操作的 L3 完成（部分項目由使用者略過，見「L3」，D85）。先前的狀態：**狀態：L3 待做（2026-10-15）。** 機器驗收（16-L1、16-removal、16-L2＋16-L2b）全部合格；使用者操作的 L3（下面「L3：待做」）還沒有做，所以本基線**尚未完成升格**。L3 做完並記錄之後，才把本行改成升格完成（D81：「STABLE_BASELINE v7 在 L3 完成前標『L3 待做』」）。

撰寫日期：2026-10-15。Owner：`object_fps_pvp`。
依使用者決定 D76（v7 只交付任務 1～3）與 D80 U1（沒有 Windows 實機也升格，D34），v7 以**縮小範圍**升格。
範圍為 **macOS 26.7.1、Intel x86_64（i9-9980HK）、SDL 視訊驅動 cocoa、Metal、120 Hz 顯示器上限、同機三角色**。v7 沒有 LAN 場次（第 15 批移到 v7.1；3 人 LAN 測試改期，D83）。
這是已驗證範圍的開發基線。Windows、Linux、macOS arm64 實機與兩機時鐘漂移都沒有驗證（見「平台」「跳過與未驗證」）。
v7 **不標「缺陷版」**：第 16 批沒有任何發現推翻設計前提（D44 回歸 0／260、HUD 不一致是分析器的 v6 假設，不是產品錯誤）。已知限制都有範圍與歸屬（v7.1／v8）。
v5、v6 的穩定基線原樣保留（v6 已標「缺陷版」，D69：[v6 穩定基線](../v6/STABLE_BASELINE.md)）。

## 已知問題（摘要）

- **D44 偶發 movement epoch 重設：未再現、未結案。** 最終程式上 260 個 case-run 0 事件（各母體上限見「D44 整體回歸」）。不結案的理由：已知的結構性重設途徑仍在（[09b](09b-phase-tracking.md) 1-6 節、1-7 節第 5 點的 300 ms 型延遲消失）；回歸不含 D49 第二階段（`GatewayDropReport`，D73 移到 v7.1）。修法與原因調查在 v7.1 候選 A3，pv7 之後再回歸一次。
- **Windows 實機未驗證，而 v7 換了執行緒與計時器後端。** TT-1 的 Windows 高解析度 waitable timer 與三角色只在 CI 的 hosted runner 跑過 L1；LAN 測試包已備好但改期（D83）。對外的玩家（LAN 的朋友）用 Windows，所以這是本基線最大的未驗證範圍。

## 固定契約與版本

- **wire 不變，仍是 pv6**（D73）。詳細政策以 [v6 契約](../../protocol-v6.zh-Hant.md)為準。v7 的契約追記（不改 wire）：
  - `protocol-v6`「Runtime 輸入流（P2 資訊流修訂）」（D53，第 08c 批）：Gateway 不再依 `lastResolved` 丟掉已解析的命令，Match 是唯一的判斷者。
  - pv5／pv6 的送出政策與相位追蹤追記（D68，第 09a、09b 批）。
  - ingress v1（`match-ingress.jsonl`、丟棄計數的鍵）維持暫定（D53、D76；第 10 批移到 v7.1）。
- **權威結果不變**：對 v6final 的兩樹 digest，scale 1、3 各 35 情境 0 差分；v6 的 30 個 golden 0 不符（16-L1）。
- **FireGate 兩個常數維持 v6 的值**：`FireGateGuardSeconds`＝2 ms、`FireGatePendingSpreadTicks`＝7 Tick（S4-lite 縮小版凍結，D75；測試以字面值釘住）。完整推導與 M0 在 v7.1。
- 版本號：v7＝遊戲版本，pv6＝網路協議版本（v6 D20）。

## v7 交付什麼

v7 的主題是「30 FPS 與網路時序」，只交付任務 1～3（D76）。

| 任務 | 內容 | 批次 | 主要證據 |
|---|---|---|---|
| 1 | 主執行緒（事件＋畫面）、模擬、網路三個角色分離；畫面或事件卡住時命令照樣產生 | 第 04 批、TT-1（`GYO::Time`、`GYO::Threads`） | C1 使用者操作的 L2／L3（拖動、縮放、Spaces、縮小、遮住：替代 0、斷線 0）；本批 L3（待做） |
| 2 | 移動命令在固定步邊界產生，不再跟著畫面幀；相位追蹤處理每一份 snapshot | 第 02、03、04 批 | C1（4 ms 狀態 clean-30 v7 12／12）、C2-A（8 ms 狀態 clean-30 v6 1／6、v7 6／6） |
| 3 | 網路路徑從短休眠輪詢改成事件驅動（Match IPC、Client worker、Gateway 的結果與 link）；另外修好 Gateway 悄悄丟掉已解析命令（08c）、停頓後送輸入被卡住（09a）、相位修正修過頭（09b） | P2-log、第 06、07、08、08b、08c、09a、09b 批 | 各批 L2；C2 回歸；本批 16-L2 |

## 移到 v7.1／v8 的項目

| 項目 | 去向 | 決定 |
|---|---|---|
| pv7（第 11 批）：runtime link 心跳對時（任務 4）、Client↔Gateway 時間回聲、`GatewayDropReport`（D49 第二階段）、Gateway 職責條文 | [v7.1](../v7.1/README.md) | D73 |
| M0 影子量測與 FireGate 常數的完整推導、合成時間線改寫 | v7.1（第 10 項；採用 A1 時不必做） | D75 |
| TT-2（`GYO::Trace`）＋第 10 批（任務 8 的紀錄，日誌格式定為 Data Contract） | v7.1（第 11 項） | D76 |
| IP-3、IP-4＋第 12 批（產品不再直接依賴 SDL，任務 7） | v7.1 | D76 |
| 第 15 批（Windows LAN 場次、兩機偏移與漂移） | v7.1（第 13 項） | D76、D73 |
| D44 原因調查與修法（A3）、重新擷取的樣本邊界（B1）、earliest 寫死減 2（B6）、分析器收斂（C4）、`-fexperimental-library`（C5）、Windows Match Tick 精度（F7）、v5／v6 缺陷版的重新發佈（G1） | v7.1 候選 | D44、D71、D75、D76、D69 |
| IP-5＋第 13 批（解析度、視窗模式、FPS 上限，30 FPS 手感的 L3，任務 6）；設定 UI 與保存 | [v8](../v8/README.md) | D76、D41 |
| AU-1＋第 14 批（音效，任務 5） | v8 | D76 |

## 來源與產物

| 項目 | 值 |
|---|---|
| 產品程式 | master `6d3f287`（PR #73 合併）；產品程式＝C2 after `d685d1e`（`git diff --stat d685d1e 6d3f287 -- . ':!docs' ':!*.md'` 為空） |
| 驗收工具的追加 | 分支 `claude/pvp-v7-p7` 的 `ab992f5`（16-L2b 的 commit H，D84）：`build/acceptance/object_fps_pvp/combat_gui_evidence_v7.py`、`test_combat_gui_evidence_v7.py` 與 `tests/object_fps_pvp/CMakeLists.txt` 的 CTest 登記。對 `6d3f287` 的非文件差異只有這 3 檔，產品程式不變 |
| PR 合併後的 master | `0f0bc91`（PR #74 合併，2026-10-15） |
| 建置 | detached worktree `../GYO-Engine-v7final`（`6d3f287`）從零建置（test preset，`pvp-v7-16-l1-20261014/build.sh`）；相依套件原始碼取自主工作目錄已下載的同版本；和 `../GYO-Engine-c2-after` 正規化比對：C++ 四個執行檔與 8 個產品靜態庫的反組譯 0 差異、Gateway 只差 `vcs.revision`／`vcs.time` 與 pclntab、資產 31 檔相同 |
| Client SHA-256 | `8b62e42ddfb3f546d9fc52d4b02f6f515a63351703dc1226ecf8f77d8fe020c9` |
| Match SHA-256 | `ce76bfa28c96500538c9bb0fa255b7420a05ab9fd060de0a4048367c684c80eb` |
| Gateway SHA-256 | `a7aecefcb1a81a30a5bc18e35cedf3a0460ca336ae737277f9580a536154161d` |
| 訓練地圖 `pvp_arena.json` | `0026013c731a7d7317ce6be4f84f897b91fa1b807c9621d60fb608570f1775a5`（和 v6 相同） |
| probe、SDL、部署資產 31 檔 | `pvp-v7-16-l1-20261014/artifacts.sha256`（清單本身 `f0a87dd7…`，46 行；突變後重建的最終狀態，16-L2／16-L2b 以它核對） |
| 驗收工具與分析器 | 16-L2：`frozen.sha256`（`8263d38c…`，117 行：v7final 與 c2-after 的驗收工具各 52 檔、C1／C2 匯入的凍結模組 7 行、本批腳本 6 檔）；16-L2b：`frozen-16b.sha256`（`bb61cb4f…`） |
| CI 與 snapshot | master `6d3f287` 的 run 38561353343，19 個 job 全 success（L1 四平台、Quick acceptance、`CI gate`、兩個 Snapshot）；`object_fps_pvp-snapshot-20261014-6d3f287`、`tools-snapshot-20261014-6d3f287`（prerelease，target＝`6d3f287`） |

## 驗收結果

### L1 與產品移除（16-L1、16-removal，2026-10-14）

| 項目 | 結果 | 指標 |
|---|---|---|
| CTest 全部（L1-1） | 通過 | `ctest --preset test` 69／69（突變後重建再跑 69／69）；label 外 5／5；go gofmt／vet／test 通過；Python 工具測試通過 |
| go race（L1-1） | 通過（記錄 1 次不穩定） | 6 次中 1 次失敗，建置結束後重跑通過（見「已知限制」第 7 項） |
| TSan（L1-2） | 通過 | `object_fps_pvp.ipc` 3／3、警告 0；worker probe 30／30 在容許內 |
| 權威 35／35（L1-3） | 通過 | 對 v6final scale 1、3 各 35 情境 0 差分；golden 30 個 0 不符；最終執行檔再比一次相同 |
| Match 不連 SDL（L1-4） | 通過 | `otool -L` 只有 CoreFoundation、libc++、libSystem；`_SDL_` 符號 0 |
| 突變（L1-5，U7） | 通過 | `4927d40..6d3f287` 改過的檔案相關 26／26 killed；整份目錄 349 個 0 stale |
| 產品移除（L1-6） | 通過 | 刪除 329 檔＋`projects.csv` 1 列；configure／build exit 0、CTest 37／37（含 `gyo_time_tests`、`gyo_threads_tests`）；共通層修改 0；Dummy Target、空目錄 0；D12 連結 0；殘留提及：程式與登錄 12（和 v6 相同）、文件 268（v6 265，多的 3 行在 `time-threads-trace/HANDOFF.md`，文字提及）。沒有 Architecture Delta |
| CI 與 snapshot（L1-7） | 通過 | 見「來源與產物」 |

### L2（16-L2 2026-10-15 01:06～03:49、16-L2b 同日 14:39～14:42）

16-L2 依宣告（`1b2d2fbf…`，D82）`passed`＝false、168／171：GUI 戰鬥短測 `timing-gui-short` 被凍結分析器判 13 幀 HUD 不一致而停下（停止條件 6）。診斷是分析器的 v6 假設在 v7 不成立（見「已知限制」第 6 項）。D83 選 (a)：只改分析器，以補充宣告（`5b1466f6…`，D84）重跑該項與沒跑的 3 項（16-L2b），`passed`＝true。16-L2 的 `timing-gui-short` 保留為 F、不改判。**16-L2 整體通過**。全部跑次的主機都是 8 ms 狀態（probe TimerBaseline，只記錄、不分層）。

| 項目 | 結果 | 指標 |
|---|---|---|
| L2-1 25 案（clean-30 計入判定，U2） | 通過 | 25／25；relay、Gateway 最短間隔與 D61 規則都沒有問題 |
| L2-2 背壓與網路 | 通過 | 背壓 6／6；`run_network.py` 一般模式通過 |
| L2-3 長測短模式（v7 第一次） | 通過 | Actual P50／P95 37.6／37.7 ms、100%；合法射擊 Match／Client P95 12.6／19.2 ms |
| L2-4 quad clean-60 ×5（v7 第一次） | 通過 | 5／5；Actual 100%、P50 37.4～37.5 ms、P95 37.5～37.7 ms；拒絕 0 |
| 　quad clean-30 ×5 | 記錄（工具 `judged: False`） | 5／5 通過；Actual 100%、P50 37.4～37.5 ms |
| L2-5 1 GUI＋3 bot | 通過 | Actual P50／P95 37.3／37.6 ms、100% |
| L2-6 action short 30／60／144＋capture | 通過 | 16-L2 |
| 　player short 30／60／144＋capture | 通過（player144 無效） | player144 只到 120 FPS（顯示器上限），依 D11③ 不計次 |
| 　雙 GUI timing short（`--combat`） | 16-L2：F（保留）；16-L2b：通過 | 16-L2b：凍結分析器 19 幀 HUD 不一致、v7 規則 `pvp-v7-combat-hud-2` 判 P；其他 combat 錯誤 0、`disturbed` False、runtime gap 0、只在區間相符的幀寬度最大 2 Tick（旗標門檻 6）。16-L2 的跑次：Actual P50／P95 37.5／37.6 ms、presentation P50／P95 37.6／38.4 ms |
| 　2 GUI＋2 bot：action60、player60、timing | 通過（16-L2b） | timing 項目同上：凍結分析器 18 幀、v7 規則 P |
| 60 Hz 長測（1808 秒，D44 的一部分，U6） | 通過 | 113 循環、`full_30_minute_qualified`＝true、非 respawn 重設 0。三角色第一次跑 30 分鐘 |

### 引用的對比結論（C1、C2）

| 項目 | 結果 | 指標 |
|---|---|---|
| C1（第 05 批，2026-10-09，P1b 頭，任務 1、2） | 通過（4 ms 狀態） | clean-30 v6 9／11（替代 0.570%）、v7 12／12（0.010%）；clean-60 v7 12／12、替代 0；v7 的停頓重設與非 respawn 重設 0。8 ms 狀態當時沒有 after 樣本 |
| C1 使用者操作的 L2／L3 | 通過 | 拖動與縮放：Match 替代 0、停頓重設 0；Spaces、縮小、遮住：斷線 0、`runtime_gap` 0 |
| C2-A（第 09 批，2026-10-14，產品＝本基線） | 通過（8 ms 狀態） | clean-30 v6 1／6（替代 1.09%）、v7 6／6（0／10062）；clean-60、144 兩棵都全過。4 ms 狀態沒有樣本（由 C1 補足） |
| C2-B GUI 連點（經過真正的 FireGate） | 通過 | after 判定格 18 次全部 P，權威 Cooldown 拒絕 0（每格 0／92～132 發，95% 上限每發約 2.3～3.3%）；送出→裁決中位數 v7 15～34 ms、v6 52～72 ms |
| C2 回歸（25 案＋背壓＋網路） | 通過 | D61 即時與事後規則 0 問題；M0 沒有觸發 |

C1、C2 只對同一台 Intel Mac、同機三角色成立。

## D44 整體回歸

- **範圍**：master `6d3f287`（任務 1～3、08c、09a、09b、S4-lite）。涵蓋 08c 讓 Match 看得見的 Client→Gateway 遲到；**不涵蓋 D49 第二階段**（`GatewayDropReport`，D73）。不做原因調查（D45）。
- **事件定義**（全部沿用凍結工具）：E1＝遊戲案例與長測中故障窗外（或沒有故障窗）的非 respawn 重設（D61）；E2＝`command_evidence.py` 的「Unexplained epoch reset」；E3＝`run_network.py` 的「an application stall reset the movement epoch」。
- **結果：260 個 case-run 0 事件，60 Hz 長測 0 事件。** 讀法：「在最終程式上未再現、未結案」。

| 母體 | case-run | 事件 | 0 事件時每 case-run 的單側 95% 上限 | 第 08 批開發跑次（新 worker） |
|---|---|---|---|---|
| 遊戲（故障 66、clean 29） | 95 | 0 | 3.1%（故障 4.4%、clean 9.8%） | 1／26 |
| 背壓 | 120 | 0 | 2.5% | 1／19 |
| 網路一般模式 | 30 | 0 | 9.5% | 0／11 |
| 網路 `--network-impairments` | 15 | 0 | 18.1% | 1／1（同一個 seed） |

- **獨立性的保留**：重複跑次不是獨立的情境樣本。矩陣的故障情境是決定性的、impairments 與遊戲網路案例用固定 seed、同一 session 的主機狀態彼此相關，所以**重複只抽樣主機時序，不抽樣丟包型態或故障情境**。合併 260 次的上限 1.15% 只在「每個 case-run 獨立」的前提下成立，只當參考；以各母體的上限為主。
- **網路母體的偵測範圍很窄**：E3 只比較 6 秒停頓前後的 `movementEpoch`，「網路 0 事件」的意思是「停頓期間 epoch 沒變」。每個探針階段正好 1 次 starvation 重設（`record_flags` 空）。
- **普查基準**（16-D44T，同一套偵測器）：08b 起的既有證據全部 0（08 L2 62、08b L2 72、08c L2 114、D72 108、C2 62 個 case-run）；事件只出現在第 08 批開發跑次（25 案 1／414、背壓 1／73、網路 1／24）。
- **只記錄**：late 修正 0、領先跳升 0、被釘住秒數 0；Cooldown 1 次出現在網路案例 `network0-r2`（非 clean，不是停止條件）。
- **後續**：v7.1 候選 A3（backlog 門檻 105、300 ms 型延遲消失、快速正向回收；原因調查用 ultracode，D45）；v7.1 做完 pv7 後再回歸一次。

## L3：完成（部分項目由使用者略過，2026-10-15）

使用者操作的項目（提案 2-1 的 L3-1～L3-4，範圍照 U9）。session：2026-10-15 15:17:36～15:21:07（`build/target/_build/test/logs/pvp-v7-16-l3-20261015-151542`；Match 與 Gateway 由 agent 在背景啟動，兩個產品 Client＋2 個 bot）；判定用 C1 凍結的 judge（`pvp-v7-batch05-l2l3-20261009-1137/judge.py`，SHA-256 `e2a7ec9d…`），輸出在 `pvp-v7-16-l3-prep/judge-pvp-v7-16-l3-20261015-151542/`。使用者：「我略過了一些測試，沒有多大關係，沒做的紀錄就好。」（D85）

| # | 項目 | 結果 |
|---|---|---|
| L3-1 | 連續射擊中切換 Spaces、縮小、遮住視窗 | **通過**：兩個 Client 的 `connection failed` 0、模擬角色晚醒 ≥100 ms 0（`runtime_gap` 8 筆都是每條命開始時的記錄，幀長約 16.7 ms）。各操作的次數未記錄，比清單的 3 次少的部分記為「略過（使用者決定）」 |
| L3-2 | 標題列拖動與角落縮放 | **通過**：每位玩家最長連續替代 1 Tick（門檻 3）、停頓重設 0、重設 24 次全為 life_respawn；視窗模態迴圈 A 1 次、B 5 次（清單要求 A 上各 3 次，不足的部分記為略過） |
| L3-3 | 連點手感（60 FPS） | 使用者評語：「連擊手感好了很多」；換武器之後會如何不知道（見已知限制與 v8 候選「多武器與 FireGate」） |
| L3-4 | v6 原生操作清單抽查 1、3、5、6、10 | 1、3、5 與 6 的 Tab／拖動／縮放：使用者沒有回報異常；**6 的「關閉視窗後重新加入」與 10 的 `room_full`（第 5 人加入）略過**（Gateway 沒有紀錄）。10 的 4 人房成立（2 個 bot 於 15:20:00 加入，擊殺與重生正常）。使用者觀察：重生點沒有保護（已知限制第 9 項） |

- 只記錄：Match 有 3 個 Tick 的間隔 ≥40 ms；bot 的「Gateway connection timed out」是 agent 停掉 Gateway 時 bot 的 300 秒還沒跑完，不是產品問題。

## 平台

| 平台 | 狀態 |
|---|---|
| macOS Intel x86_64（Metal、120 Hz） | **實機驗證**：L1、L2、L3（本基線；L3 部分略過） |
| Windows x64（D3D12） | **未驗證**（D34、D76）。第 15 批移到 v7.1；3 人 LAN 測試改期（D83），測試包 `~/gyo-lan-v7/` 與 [LAN 測試手冊](LAN_TEST.md)保留（Windows Client 用 snapshot `object_fps_pvp-snapshot-20261014-6d3f287` 的 windows-x64 包）。CI 跑 L1 |
| Linux x64 | **未驗證**；CI 跑 L1 |
| macOS arm64 | **未驗證**；CI 跑 L1 |
| 兩機時鐘漂移 | 不量（D73：需要 pv7 的時間回聲，v7.1） |

## 跳過與未驗證

使用者決定跳過（照 v6 D26，D80 U6；標「跳過（使用者決定）」，不算通過）：

- GUI 三輪（各 120 秒）。
- 144 Hz 長測（1808 秒）。60 Hz 長測有跑（見 L2 表）。
- 完整驗收的 4 人一輪（1 GUI＋3 bot 與 quad 有跑）。

其他沒有驗證的：

- Windows、Linux、macOS arm64 實機（見「平台」）；Windows Match 的 Tick 與 IPC 精度（v7.1 F7）。
- 兩機時鐘漂移（D73）。
- FireGate 重推條件 10 中的「窗口外的 R<earliest」「R>earliest＋7」：headless 案例不經閘門、GUI 取不到 earliest，量不到（盲點；只量得到權威 Cooldown 拒絕）。
- M0（v7.1）；30 FPS 手感、解析度、音效（v8）。
- `gui_visible_latency`（發行用的實機證據）只記錄，因為這次不發行（U8）。

## 已知限制與殘餘

1. **相位追蹤（[09b](09b-phase-tracking.md) 1-7 節）**：
   - 撤回少 3.3 ms，殘餘是結構性的（相位偏早，方向安全）。
   - 撤回後約 4 秒內相位誤差 3.3 ms 超過 FireGate 的 guard 2 ms，`earliest` 可能晚 1 Tick（只影響壓著冷卻邊界的射擊）。
   - 只撤回決定那一刻樣本看得到的部分。
   - δ＞70.6 ms 時，要比 slew 結束早超過約 11～19 ms 放出才不串接（D71）。
   - 300 ms 型的延遲消失照舊重設（1-6 節，範圍外；每次重設讓 1 個連線品質視窗判失敗，連續 3 個踢人，實際網路頻率未知）。
   - 送出平均率只在 60 Hz 產生時是 60／秒（任何 1 秒最多 82 個輸入封包）。
   - 修法候選：v7.1 B1（第 1、2、4 點）、A3（第 5 點）。
2. **FireGate 的殘餘（S4-lite，[09 文件](09-firegate-c2.md)「常數涵蓋不了的失敗」）**：
   - guard 的設計最壞值約 3.1 ms（P90 落在死區邊緣）。
   - Match 端領先超過 2 step＋4 ms＋guard 時 `earliest` 可能估晚 1～2 Tick：late slew 與撤回後殘餘（v7.1 B1）；延遲下降 Wdrop（推論：每次延遲下降後至多約 4 秒內，冷卻邊緣的點擊在 network20 約 20～45%、network40 約 75～95% 會被權威拒絕，實際網路頻率未知；v7.1 B6）；D44 領先跳升（`FireGate.cpp:22` 寫死減 2）。
   - 主機停頓與 Match Tick 停頓（2 ms 以上就足以讓 y 超過 guard）。
   - spread 的「晚到 ≤5 ms」是假設、沒有量。
   - GUI 取不到 earliest；M0 沒有做（C2 沒有觸發）。
   - v7.1 的 A1（射擊併入輸入命令）若採用，這一類連同兩個常數一起消失。
3. **延遲位移**：「產生→執行」約 38 ms（30／60 FPS 相同），v6 是 21～25 ms。這是命令改在步邊界產生的代價，不是缺陷（第 04 批、C1、09b）。
4. **08c 的已知缺口**（[08c](08c-drop-report.md)「已知缺口」）：Gateway 拿不到內容的丟棄（速率限制、解碼失敗、未授權、舊序號的 Actions 等）只計數，Match 不知道；下行 D4、D7、D8、D9 不在守恆內；D58 的 (1)(8) 維持暫定。到 v7.1 的 pv7 才處理。
5. **D44**：未再現、未結案（見「D44 整體回歸」）。
6. **HUD 的 `hud_tick` 落後 1～2 Tick（產品正確，分析器 v7 規則）**：第 04 批把 Reconcile 移到模擬角色後，探針記錄的 `hud_tick` 是模擬角色最後觀察到的 snapshot，HUD 的值是主執行緒最新的權威 snapshot。HUD 顯示正確，玩家看不出差異。凍結的 `combat_gui_evidence.py` 會判不一致；v7 用 `combat_gui_evidence_v7.py`（`pvp-v7-combat-hud-2`，區間 [max(hud_tick, 前一行 snapshot_tick), snapshot_tick]）。16-L2b 的落後分布（timing-gui-short）：0 Tick 78、1 Tick 2217、2 Tick 32 幀。精確版（在觀測 API 加 HUD 來源 tick）會改產品，列為之後的工作。診斷：`pvp-v7-16-l2-20261015/hud-diagnosis.md`。
7. **Gateway 的時間敏感測試在高負載下不穩（只記錄）**：16-L1 的 go race 6 次中 1 次失敗（`TestLinkResendsAnUndecidedRequestAtTheActionRateDespiteOtherWakes` 量到 24.0 ms＜下限 30.3 ms，同時 C++ 從零建置、load 約 22），建置結束後重跑通過（`pvp-v7-16-l1-20261014/go.log`、`go-race-rerun.log`）。
8. **腳本在背景啟動的 Client 忽略 SIGINT（只記錄）**：非互動 shell 把背景工作的 SIGINT 設為忽略，SDL 只在預設處理時才裝自己的處理器；SIGTERM 立即結束。Match 與 Gateway 自己註冊，不受影響；使用者在前景執行、用 Esc 或關閉視窗結束也不受影響（`pvp-v7-lan-kit-20261014/summary.md`）。
9. **重生點沒有保護（L3 的觀察，2026-10-15）**：玩家不移動、佔著重生點時，之後加入或重生的人可能疊在一起。v6 起就是這樣（v7 沒有改出生規則）；很多 FPS 也有。列進 [v8](../v8/README.md) 的候選「出生點分散、出生保護」，到時決定要不要處理。
10. **換武器之後的射擊閘（L3-3 的使用者提問，2026-10-15）**：v7 的連點手感變好，來自網路與時序（裁決回程 v6 約 52～72 ms → v7 約 15～34 ms，第 06、08b 批；送輸入不再被卡住，09a；命令在固定步產生，第 04 批），**不是槍械參數**（`cooldownTicks` 10、彈匣 12、換彈 90 Tick 不變，權威 digest 與 v6 相同）。FireGate 的兩個常數（guard 2 ms、spread 7 Tick）是在冷卻 10 Tick、60 Hz 的前提下推導的；換成射速不同的武器時要依重推條件 8 重推，冷卻短於裁決回程（約 1～2 Tick）的武器會更依賴 pending 上界，冷卻邊緣的點擊可能被本機多擋或被權威拒絕。v7.1 候選 A1（射擊併入輸入命令、照命令步序判定）若採用，這兩個常數就不需要。列進 [v8](../v8/README.md) 的候選「多武器與 FireGate」。
11. **其他（沿用 HANDOFF「未結事項」）**：
   - `ClientConnection` 關閉時最多約 3 秒的阻塞（httplib）。
   - D30 門檻可能被逐漸變長的發布間隔逐級放大（低機率）。
   - Spaces、縮小時的斷線可能來自 App Nap（C1 沒有重現；本基線的 L3-1 再確認）。
   - TimerBaseline 被 C2 拿來分層（違反它的標註，C2 決定 8）；本基線沒有用它分層。
   - probe 幀格點的相位隨建置移動（D46⑦），b、a＋b 等不能跨建置比較。
   - `-fexperimental-library` 還在（v7.1 C5）；`quad_evidence`／`command_evidence` 的重複（v7.1 C4）。
   - TT-1 L2 的量測分支 `claude/tt1-l2-wake-record`（`a8f9535`，未推送）不合入 v7，留給 v7.1（U11）。

## 發行

- **這次不正式發行**（D80 U8）：只升格。理由：對外的玩家用 Windows，而 Windows 實機完全沒有驗證；v7.1 很快升 pv7（wire 改變）；發行要的 L4 證據只有 macOS x64。
- tag 候選：**`object_fps_pvp-v1.2.0`**（上一版 `object_fps_pvp-v1.1.0`＝v6，已標缺陷版）。wire、權威結果與玩家操作都不變，屬 minor。
- 想試的人可以用 master 合併後自動產生的 snapshot（prerelease）。目前是 `object_fps_pvp-snapshot-20261014-6d3f287`。GitHub 上的 Latest 仍是 v1.1.0。
- Publish 一律由使用者操作（[發行文件](../../../releasing.zh-Hant.md)第 9 節）。

## 怎麼重現

證據與腳本都在 `build/target/_build/test/logs/`（git 忽略，只在本機）。

1. **建置**：`git worktree add --detach ../GYO-Engine-v7final 6d3f287`，再執行 `pvp-v7-16-l1-20261014/build.sh`（含 TSan 樹；cmake／ninja／go 的路徑寫在腳本裡）。用 `artifacts.sha256` 核對；C++ 執行檔的 SHA-256 會因連結時間戳與 LC_UUID 不同，同一程式以 `binary_compare.sh` 的正規化比對確認。
2. **L1**：指令全在 `pvp-v7-16-l1-20261014/commands.txt`（`tests.sh`、`go.sh`、`digest.sh`、`mutations.sh`）。
3. **產品移除**：`pvp-v7-16-removal-20261014/removal_check.sh`（在暫時 worktree 執行，`commands.txt`）。
4. **16-L2**：`pvp-v7-16-prep-20261014/run_16.py`／`judge_16.py`，字面值照 [16-integration-acceptance.md](16-integration-acceptance.md)「執行步驟」（`--frozen-sha 8263d38c…`、`--artifacts-sha f0a87dd7…`、`--declaration-sha 1b2d2fbf…`）。需要 `../GYO-Engine-c2-after` 還在（`judge_c2.py` 從那裡匯入）。約 2.7 小時，G 部分會開視窗，期間不要碰桌面。
5. **16-L2b**：`pvp-v7-16-l2b-prep-20261015/run_16b.py`／`judge_16b.py`（`--frozen-sha bb61cb4f…`、`--declaration-sha 5b1466f6…`），步驟見同一份文件的 16-L2b「執行步驟」。
6. **事後重判**：`judge_16.py --session pvp-v7-16-l2-20261015 …`、`judge_16b.py --session pvp-v7-16-l2b-20261015 …`。

## 驗收過程中保留的紀錄

- 16-L1：TSan 第一次設定少了 `BUILD_TESTING=ON` 而失敗，補齊後成功；突變後的重建改變了 C++ 執行檔的 SHA-256，對最終執行檔重做正規化比對與權威 digest（`artifacts-initial.sha256` 保留）；go race 的 1 次失敗保留。
- lan-kit-v7：第一次 configure 的輸出預設寫進 v7final 的 `build/target`，約 2 分鐘內停下、沒有產品輸出，事後 v7final 0 新檔。
- 16-L2：`timing-gui-short` 的失敗跑次保留、不改判（D83）；勿擾（專注模式）無法以指令確認，記「未確認勿擾」（D82）；受干擾 0 次。
- 原本的分批順序 I → G → D 依 D45 檢查改成 I → D → G（D82）。

## 證據索引

證據都在 `build/target/_build/test/logs/`（git 忽略，只在本機）：

- `pvp-v7-16-plan-20261014/`：第 16 批清單提案（U1～U12，D80）。
- `pvp-v7-16-l1-20261014/`：建置、CTest、go、TSan、權威 digest、突變、`artifacts.sha256`、`ci-snapshot.json`、`summary.md`。
- `pvp-v7-16-removal-20261014/`：產品移除檢查（`summary.md`）。
- `pvp-v7-16-prep-20261014/`：`run_16.py`、`judge_16.py`、普查、凍結清單、宣告、D45 xhigh 檢查（`summary.md`）。
- `pvp-v7-16-l2-20261015/`：16-L2 的 `judgement.json`、`progress.jsonl`、`runs/`、`hud-diagnosis.md`。
- `pvp-v7-16-l2b-prep-20261015/`、`pvp-v7-16-l2b-20261015/`：16-L2b 的工具、宣告與結果。
- `pvp-v7-lan-kit-20261014/`：3 人 LAN 測試包的建置、打包與冒煙測試（`summary.md`）。
- `pvp-v7-09-c2-20261014/`：C2；`pvp-v7-batch05-20261009-s2/`、`pvp-v7-batch05-l2l3-20261009-1137/`：C1 與其使用者操作。
- `pvp-v7-batch08b-l2-20261010/`、`pvp-v7-08c-l2-20261011/`、`pvp-v7-09ab-l2-20261014/`：各批 L2。
- `pvp-v6-30fps-reference-20261007/`：v6 的 30 FPS 失敗案例基準包。

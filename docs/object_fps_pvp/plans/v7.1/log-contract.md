# log-contract：日誌 Data Contract v1 與 README 指名的紀錄（計畫，第 1 版）

- 狀態：計畫第 1 版已核准（D142）。core 子批完成（第 11 節）；D143 之後 records 子批完成（第 12 節）。只記錄 L2 已跑（D144，第 14 節）；四平台 L1（run 39428630663）通過後結案。
- 依據：README 的 `log-contract` 列（「日誌行的版本與驗證規則：08c 行族 v1 的去留、D58 暫定項的確認、原第 10 批的紀錄（拒絕原因、snapshot 年齡、Match 的裁決與結束紀錄等）」；驗收「跨語言 golden、凍結分析器仍可讀／只記錄／—」；估 16 h）、INVENTORY 指向本批的項目、`plans/v7/HANDOFF.md:380-395`（原第 10 批與觀測缺口清單）、D58（`plans/v7/HANDOFF.md:103`）、`protocol-v7.zh-Hant.md` §1「日誌只加鍵」、`network-architecture.zh-Hant.md:295`（死信三原則 D57）、`tt2-trace.md`（D139 T-1 把 `match-shots.jsonl`、`match-ingress.jsonl` 留給本批）。
- 規劃方法：ultracode（2026-10-20）。盤點 2 位（C++、Go 的行族與讀者）、方案 3 位（最小契約、讀者優先、觀測缺口優先）、評審合成 1 位，以上用 high；對抗式檢查 1 位用 xhigh：MUST-FIX 7、SHOULD-FIX 10、NOTE 5，全部的對應見第 10 節。各 agent 的完整輸出在本 session 的 workflow 紀錄。
- 估時：**拆成兩個子批，共約 23 h**（README 原估 16 h，約 1.4 倍；原因見第 8 節）。每個子批各自以 1.5 倍停損；單一問題累計超過 2 h 就停下回報。
- file:line 以 `2dd5d74` 為準。

## 1. Feature（推導順序：Feature → Contract → Architecture）

| 代號 | 使用者或診斷得到什麼 | 子批 |
|---|---|---|
| F1 | **契約登記表**：所有被工具解析的行族列為 A 級，寫明寫出端、錨定的正規式、鍵的順序、值的文法、版本、golden、權威 owner、讀者與是否凍結；其餘是 B 級，不承諾相容。之後的批次照表就能判斷改動會不會打斷凍結分析器 | core |
| F2 | **凍結分析器不會靜默讀錯**：破壞性變更改用新的事件名或前綴，舊的正規式找不到行，凍結工具大聲失敗 | core |
| F3 | **同一程序多個 Client 時，對時行分得出是誰的**；對時行的本體由 C++ 與 Go 共用一份多列 golden 守住 | core |
| F4 | **Gateway 的斷線原因可以分類**（活性逾時、寫出逾時、版本不符、協定錯誤、EOF），統一兩種 `i/o timeout` 字串 | core |
| F5 | **Client 的摘要不再下溢**（N-summary-counter-wrap） | core |
| F6 | **Match 的 Tick 停頓能從 match.log 數出來**（E1-5：超過名目週期的量與停頓次數） | records |
| F7 | **Match lane 的送出節奏能讀出**（E1-7：snapshot 與 action lane 的寫出超時） | records |
| F8 | **Windows LAN 上的 Client 可以自己診斷**：每次受擊一行（含不閃紅的原因，原因由受擊效果自己判斷）、摘要依拒絕原因分開計數、worker 行尾的流失計數 | records |
| F9 | **主機狀態可以依計時器後端分層**：四條 role 各寫一行 `role started`（含 wait backend） | records |
| F10 | **Match 有自己的結束紀錄** `end`（含原因與 exit code） | records |
| F11 | D-LOG、D58、D37 定案寫進契約；INVENTORY 的對應項目結案 | core |

## 2. Contract（`docs/object_fps_pvp/log-contract.zh-Hant.md`，owner `object_fps_pvp`）

- golden：`tests/object_fps_pvp/fixtures/log_v1/`。既有的 `fixtures/ingress_v1/` 原檔不動（凍結的 `test_ingress_evidence.py` 讀它），狀態註記「v1 的前段樣本」；目前完整的行（Gateway 玩家行 6 組、全域行 1 組）另外放進 `log_v1/`，由 Go 與 C++ 的寫出端測試逐字比對。
- 不做機器可讀的 schema 語言（AGENTS.md §9）；Engine 不認識任何產品的行族（`docs/architecture/trace.md` Design Invariant 6）。
- **取代 `protocol-v7.zh-Hant.md` §1「日誌只加鍵」**（第 9 節 LC-3）：該節改成一行連結，A／B 分級與下列規則從此生效。

### 2.1 兩級行族

- **A 級**（有工具解析）：Match 的 network statistics、ingress v1、start、ready、ipc closed、ipc hint、ipc accept failed、join、evicted；Client 的 worker statistics；Gateway 的 player／gateway ingress v1、gateway statistics、send statistics、gateway transport、runtime transport、drop report、`gw_drop` report／violation、start／HTTP、player evicted；三種 JSONL（移動紀錄 schema_version 2、`match-ingress.jsonl` {schema, version 1}、`match-shots.jsonl` {version 2}）。本批升為 A 級：三端的 clock sync。本批新增：`ipc statistics`、`role started`、`end`、`PvP hit feedback`。
- **B 級**（診斷文字，可自由改）：摘要、連線、cause records、death／respawn、`gw_transport`、`statistics tick=`、lifecycle、`connection failed`（目前只在 `test_network_statistics.py:20` 當雜訊樣本）。工具要開始解析 B 級行，必須先升 A 級並附 golden。
- 讀者的盤點範圍：`build/acceptance`、`tests/object_fps_pvp/*.py`、`build/target/_build/test/logs/**/*.py`（各 L2 的判定工具），以及 Go、C++ 測試裡的字面值。盤點時已漏掉的兩個（`run_quad.py:227`、`:232` 讀的 `ipc closed reason=`、`ipc hint`）列入 A 級。

### 2.2 版本與相容

- R1：同一版內只能在行尾追加鍵。
- R2：**沒有版本鍵的行族（版本＝1），破壞性變更一律改用新的事件名或前綴**（例如 `network statistics.v2 `），讓 v1 的正規式找不到行；凍結工具因此大聲失敗（`network_statistics.py` 丟 `no statistics lines`、`judge_tt2.py` 回 None）。只有本來就帶 `version=` 且讀者會檢查的行族（ingress，以及本批新增的行族）才用 `version=N` 升版。
- R3（D-LOG）：ingress 兩端維持 v1、只在行尾追加；組別推斷規則（`ingress_evidence.py:108-131`，目前 Gateway 玩家行 6 組、全域行 1 組、Match 行 1 組）寫進契約正文；已刪除的路徑照寫 0（墓碑）。
- R4（JSONL）：既有的 kind 可以加欄位；新增 kind 要升版（`command_evidence.py:137-138`、`ingress_evidence.py:782-783` 遇到不認得的 kind 會停）；逐筆的高頻紀錄寫到另外的 JSONL；最後一行是 `trace_end`；寫出或關閉失敗時 exit 1；新檔用 {schema, version}；既有三種檔頭照實登記，不改。
- R5：日誌不影響控制流程（權威 digest 守住）。

### 2.3 值的文法與比對

- 被 `network_statistics.py` 解析的行（Match network、Client worker、Gateway send），追加的值只能是整數、小數、`na`、`-`、`p50/p90/p99/max` 形式或以逗號分隔的整數；其他字串會讓凍結的 `value()` 丟 `ValueError`。ingress 行的值只能是 `^\d+$`。字串值只能放在自己的行族或 B 級行；自由文字只能是最後一個欄位。
- **比對錨定在訊息開頭**（Go：log 時間戳之後；C++：LogFile 時間戳之後）。A 級表的每一族寫出錨定的正規式；「行內不得含別族的錨定式」。Gateway 的 `clock sync window_ms=`、`client clock sync player=` 與 Client 的新前綴因此不會互相誤認。禁用片語集中在 `fixtures/log_v1/prefixes.txt`，C++、Go、Python 守衛都讀這一份（取代 `IngressStatisticsTests.cpp:150-157` 與 `ingress_statistics_test.go:21-23` 兩份寫死的清單）。

### 2.4 Golden 與守衛

- 每個 A 級行族在寫出端的語言各有一個釘住測試（逐字，或「v1 鍵序列是目前鍵序列的前綴」：`fixtures/log_v1/<族>.keys-v1.txt`）。目前沒有釘住的約 11 族在 core 補上。
- `clock-sync.txt`：「輸入欄位 → 預期 body」的多列樣本（na、負 offset、四捨五入邊界、jump=1、final=1）；C++ 與 Go 讀同一份輸入；規定 drift 必須是有限值，兩端都斷言非有限值不寫出。
- CTest `object_fps_pvp.log_contract_frozen_readers`：用 repo 裡的 `network_statistics`、`ingress_evidence`、`command_evidence` 讀 `log_v1`；反向測試：v2 樣本必須讓它們失敗；突變直接打在 fixture 上（worker 樣本加 `foo=bar`、對調兩個 ingress 鍵），守衛要失敗。另外在既有的 `ingress_cross_check.py`（真實的 Gateway＋Match 跑次）之後，也用 `network_statistics.collect` 讀它的日誌。
- 限制：凍結清單與各 L2 的判定工具在 git 忽略的證據目錄，CI 只能證明 repo 裡的分析器讀得到；凍結副本靠本機核對與 L2 重播（第 7 節）。

### 2.5 State Authority 與死信三原則

- 每個元件只記錄自己執行或直接觀察到的事，不重新推導別的元件的判斷：Client 只數收到的裁決（拒絕原因是 Match 給的值），受擊不閃紅的原因由 `LocalHitFeedback` 自己判斷（第 3 節）。
- Client worker 的 player id 存在 worker 自己的傳輸狀態（Welcome／join 時設定，CloseTransport 清掉），是 worker 自己 session 的身分，不是別人權威狀態的複本。
- 契約加一節寫明和 D57 死信三原則（`network-architecture.zh-Hant.md:295`）的對應：每個死信都有原因計數，能拿到內容時寫細節紀錄。

### 2.6 Design Invariants（草案；寫進契約文件需要你同意，第 9 節 LC-6）

1. 被工具解析的行一定是 A 級，並有寫出端的釘住測試。
2. A 級行族在同一版內只在行尾追加；`keys-v1.txt` 是目前鍵序列的前綴（C++ 與 Go 都測）。
3. 破壞性變更讓舊的讀者大聲失敗（R2），不得讓它靜默讀錯。
4. 值的文法照 2.3；自由文字只在最後一欄。
5. 每個元件只記錄自己執行或直接觀察到的事。
6. 日誌不改變控制流程。
7. Engine 不認識產品的行族。
8. 比對錨定在訊息開頭，行內不含別族的錨定式。

Replacement Criteria：ingress 的組別超過 8 組；同一版內第 2 次出現「只在行尾追加不夠用」的需求；一組鍵的變更要改 3 個以上的分析器。出現任一項就停下，提 v2（重新凍結）而不是繼續追加。

## 3. 實作切片

### 子批 log-contract-core（約 12 h）

| 切片 | 內容 | 檔位 | h | 驗收 |
|---|---|---|---|---|
| `contract_doc` | 契約文件（第 2 節）；`protocol-v7` §1 改成連結；讀者盤點（2.1 的範圍）逐條對照 A 級表 | high | 3 | 盤點到的每個前綴都在 A 級表；每列有寫出端與讀者；你同意 Design Invariants |
| `a_grade_pins` | 約 11 個 A 級行族的釘住測試、`keys-v1.txt`、`prefixes.txt`、`log_v1` 的完整 ingress 行 golden | high | 2.5 | 每族至少一個寫出端測試；把鍵改名的突變會被抓到 |
| `frozen_read_guard` | 2.4 的 CTest 與反向測試；`ingress_cross_check.py` 加讀 network statistics | high | 1.5 | 守衛通過；fixture 上的兩個突變與 v2 反向測試都會失敗；`build/acceptance` 的 git diff 是空的 |
| `clock_sync_golden` | 多列 golden；Client 改成 `[ObjectFPS/PvP] clock sync player=<id> `，id 取自 worker 自己的 session 狀態（離開或重新加入時最後一個視窗仍帶原本的 player） | high | 1.5 | 兩端讀同一份輸入並通過；Leave 之後的 final=1 行仍帶原 player 的單元測試 |
| `gateway_disconnect` | 斷線原因：讀取端兩條路徑包成同一個 sentinel（活性逾時），寫出路徑另一個（寫出逾時），用 `errors.Is` 分類；行改成 `runtime disconnected reason=<…> error=%q`。`version_errors_test.go:210` 改比對 reason（AGENTS.md §14.4，寫理由）；突變 `v7-flow-final-skips-runtime-failure` 的 find 同步並重跑；ingress_v1 README:38 的引用更新 | high | 1.25 | Go 測試涵蓋活性到期（Read 前／Read 中）、寫出逾時、版本不符；該突變仍 killed |
| `coalesced_identity` | 兩個 `coalesced_snapshots` 不改名、不加欄位：「送出迴圈中途的取代數＝transport `coalesced_snapshots` − 各視窗全域 `drop_coalesced_snapshots` 之和」寫進契約，Go 測試用已知取代次數的情境（`backpressure_test.go`、`drop_report_test.go`）斷言（第 9 節 LC-7） | medium | 0.5 | 守恆式測試通過 |
| `summary_wrap` | 下溢照 §14.3：Red 測試重現「累積 → 同一幀內 generation 改變 → phase 仍是 Playing → 再累積」；修正是在 `ResetWeaponFeedback()` 內同時把摘要的基準值歸零（小型 `SummaryBaseline`：Reset、Delta）；禁止用飽和減法 | high | 1 | 先 Red 後 Green，測試留作回歸 |
| `decision_record` | LC 結果、INVENTORY 過時行號（D-LOG、D58-1、D58-8、D58-3）、N-* 實為 7 項、D58-8 的 d44 讀法、`tt2-trace.md:34` 的「1 秒」更正（實際是 10 秒的統計視窗，`match_main.cpp:91-112`、`NetworkStatistics.hpp:14`） | medium | 0.75 | 每個項目有處置與證據位置 |

core 結束時停下回報（records 的 LC-8、LC-9 在這裡決定）。

### 子批 log-contract-records（約 11 h）

| 切片 | 內容 | 檔位 | h | 驗收 |
|---|---|---|---|---|
| `tick_overrun` | E1-5：記錄超過名目週期的量 max(0, 間隔 − 1/60 s) 進 `LateWakeStats`（現有 bins 是為晚醒設計的，正常 16.7 ms 和 28～31 ms 的停頓會落在同一格）；`tick_stalls`（間隔 >1.5 Tick 的次數）；`advance_*`（µs 級，沿用 bins）。network 行尾追加 | high；取時點與視窗邊界局部 xhigh（約 1 h） | 2.5 | 兩個注入點：Step 持鎖期間停頓（`tick_late` 與 overrun 都反映）；統計段之前持鎖停頓（只有 overrun 反映）；用既有方法做不到時加標明的 test seam；「改記 deadline」的突變被抓到 |
| `ipc_lane` | E1-7：IpcHost 記錄 snapshot lane 與 action lane（有待送時）的寫出超時，視窗交給 match_main（交換或鎖，寫明）；新行 `ipc statistics version=1` | high | 2 | 單元測試；新行的釘住測試；frozen_read_guard 通過 |
| `client_records` | (a) `LocalHitFeedback::Observe` 回傳 `HitFeedbackOutcome {None, Played, SuppressedDead, SuppressedNewLife}`，PvpApplication 只在非 None 時寫 `PvP hit feedback version=1 …`；(b) 摘要行尾依 ShotRejection 計拒絕次數（同一套 `SummaryBaseline`）；(c) worker 自己的視窗計數：foreign、undecodable、stale、send_blocked、snapshot_overflows、simulation_overflows，加上 received 當分母 | high | 3 | 受擊三個分支的 outcome 測試、既有 5 個 HitFeedback 突變仍 killed；worker golden 逐字通過 |
| `role_started` | Engine `RoleThread::WaitBackend() const`（和 `NameApplied` 並列，既有介面擴充，LC-5）；四條 role 由 owner 在 Start 後寫 `role started version=1 role= wait_backend= name_applied= priority_applied=`；Client 的 gateway 來源另寫一行（不加在 start 行，最後一欄是可能含空白的路徑） | high | 1.25 | Engine 測試：回傳值等於該 role 的 Waiter 實際後端；四平台 L1 |
| `match_end` | `end version=1 reason=signal|runtime_failed|exception exit_code= uptime_s= authority_tick=`，最後一步寫，catch 區塊也寫；契約註明 Windows `TerminateProcess` 時沒有這一行。shots 的 Finish 補 close 與檢查（位元組不變；做不出 Red 就寫明靠審查） | medium | 0.75 | 單元測試；shots 位元組不變 |
| `l2_record` | 第 7 節（LC-9） | medium | 1.5 | 照宣告記錄 |

## 4. Architecture Delta

1. 新增產品 Data Contract（`log-contract.zh-Hant.md`、`fixtures/log_v1/`），**取代 `protocol-v7` §1 的日誌規則**（LC-3）。
2. 契約文件的 Design Invariants 節（LC-6）。
3. Engine：`RoleThread::WaitBackend() const`（既有介面擴充，LC-5）。
4. 不新增 Gateway 狀態欄位（LC-7 選 (a) 時）；選 (b) 時是「Gateway 診斷欄位 +1」，依 `network-architecture.zh-Hant.md:768-769` 要你同意並更新 `state_whitelist_test.go`。
5. 不新增 Top-level 目錄、Subsystem、CMake 模組；依賴方向不變（新的 Python 測試讀同一個 owner 的 `build/acceptance`，有 `ingress_cross_check.py` 的前例；Go 讀 fixtures，有 ingress_v1 的前例）。移除性：整份契約在產品內。
6. 刻意不改：`match-shots.jsonl`、`match-ingress.jsonl` 維持主執行緒每 10 秒寫出（LC-2）。

## 5. INVENTORY 與其他項目的處置

| 項目 | 處置 |
|---|---|
| D-LOG | core：v1 沿用、只在行尾追加、組別推斷規則與 Replacement Criteria 寫進契約（LC-4） |
| D58-1 | core：照現狀定案（link 失效時計在全域 room_unavailable，`server.go:474-482`，突變 `mutations.json:1235` 守住） |
| D58-3、D58-4、D58-5 | D60 當時是「推定接受」（`plans/v7/HANDOFF.md:105`），本批請你明確確認（LC-4）；單位 acks、samples 與 exit 2 寫進契約 |
| D58-8 | core：passthrough 之後 I3 不適用；d44 讀 v7final 舊樹時，I3 照「寫出數」定義解讀 |
| E1 | 1、4 已完成；2 見 E1-2；3、6 到 `observability-tools`（6 由本批的 wait backend 先補後端這一層）；5、7 在 records |
| E1-2 | 部分：substitution 紀錄與 slack 樣本已有；逐筆的到達事件（新的 kind，R4）到 `observability-tools`（LC-8） |
| E1-5、E1-7 | records |
| TT2-B10 | TT-2 已完成。原第 10 批：拒絕原因、gateway 來源、end、ipc statistics 在 records；snapshot 年齡（`PvpApplication.cpp:295`、`:736`）、裁決（match-shots v2）、link 關閉與吞掉的例外已有，只登記；週期性的 IPC 寫出延遲（目前 `max_write_age_us` 只在關閉時寫一次）看 LC-8 |
| N-wait-backend-not-logged | records（`role_started`） |
| N-d37-log-location | core：照現狀定案（LC-4） |
| N-rotation-discarded-detail | LC-8：d44 可以由移動紀錄的 HostAccepted−Resolved 推出，所以不擋 d44；留本批（match-ingress 新 kind 升 v2，要重新凍結）或移到 `observability-tools` |
| N-summary-counter-wrap | core（`summary_wrap`） |
| N-hit-feedback-not-logged | records |
| N-client-side-loss-counters | 拆三部分：本地丟棄與送出失敗在 records；樣本流失（slack 每次只帶最小值，`protocol-v6.zh-Hant.md:95`）到 `observability-tools`（LC-8） |
| N-log-contract-vs-frozen-analyzers | core：A 級表、R1～R4、守衛、L2 重播；不重新凍結 |
| 兩種 i/o timeout | core（`gateway_disconnect`） |
| 兩個 coalesced_snapshots | core（`coalesced_identity`） |
| clock sync 暫定 | core：升 A 級，三端前綴照實登記，Client 加前綴與 player |
| shots／ingress 寫出（D139 T-1） | LC-2 |

## 6. 凍結分析器

不重新凍結。`build/acceptance` 不新增、不修改；各凍結清單（`frozen-pv7-cp`、`-d117`、`-rl`）與各 L2 的 `prep-files.sha256` 照舊有效。被解析的行只在行尾追加數值；改前綴或本體的只有 B 級行與本批新增的行族。core 的 `contract_doc` 列出沒有逐一確認過容忍行尾追加的凍結判定工具，寫進報告。

## 7. L2（records 結束時，只記錄，LC-9）

- 沒有門檻，所以不做 D45 的門檻來源檢查；記錄項與目錄名事前宣告交你核准（§14.1）。
- 做法（約 1.5 h）：同一程序多個 Client 的 probe（timing 或 quad，用既有 runner 的既有參數）跑 1 次；事前宣告的時刻對 Match 程序做一次受控停頓（SIGSTOP 再 SIGCONT 約 50 ms）。
- 記錄項：凍結清單逐一核對（cp、rl、d117 的 `ingress_evidence` 有兩個版本，各自重播）；凍結的 `ingress_evidence`、`network_statistics`、`command_evidence` 與 edge-rate-limit 的 `gateway_scan` 讀新日誌的例外與未知鍵；E1-5 的獨立來源對照（`tick_overrun`、`tick_stalls` 對照 `snapshot_produced` 重算的間隔，含受控停頓）；`ipc statistics` 兩條 lane；對時行能否依 player 分開；受擊行與 hp／death 的對應；wait backend；shots 與 ingress 的 `trace_end`；各日誌的 `[log] dropped_lines`。
- 結果意外時交你決定，不在本批內修正後重跑。Windows／Linux 的實機值留給 `lan15-windows` 與 `d44-regression-pv7`。

## 8. 工時

| 子批 | 估計 | 停損（1.5 倍） |
|---|---|---|
| core | 12 h（切片合計 12.0） | 18 h |
| records | 11 h | 16.5 h |
| 合計 | 約 23 h | — |

README 原估 16 h 沒有包含：A 級行族的釘住測試（約 2.5 h）、Gateway 斷線分類連動的測試與突變（約 0.75 h）、間隔統計的重新設計（約 0.5 h）、四條 role 與 Engine 存取器（約 1 h）、IPC 視窗跨執行緒交出（約 0.75 h）、下溢的 Red 接縫（約 1 h）、L2 的受控停頓（約 1 h）。需要縮減時，先移出 gateway 來源行與 loopback 警告（不擋後續批次）。

## 9. 等你決定

core 開始前：

- **LC-1 拆成兩個子批**：(a) core 與 records 分開，中間停下回報（建議）；(b) 一批做完。
- **LC-2 shots／ingress 的寫出器**（D139 T-1）：(a) 維持主執行緒每 10 秒寫出，只補 shots 的 close 檢查；依據是程式（`match_main.cpp:91-112`）與 `tt2-trace` 的 L2（host 鎖等待 P99 低於 52 µs）（建議）；(b) 改接 `GYO::Trace`：失敗語意可以維持（`Good()` 為 false 就 exit 1），但要把 host 緩衝與 sink 佇列兩種丟棄合併進 `trace_end`，而且觸及 `trace.md` 的 Replacement Criteria 1（shadow 重播需要 shots 不遺失）與 2，另加約 2.5～3 h。
- **LC-3 取代 protocol-v7 §1 的日誌規則**：(a) 由 `log-contract.zh-Hant.md` 取代，§1 改成連結（建議；Client 對時行加前綴、斷線行改本體都只在 B 級或新規則下才成立）；(b) 不取代：對時行改成新增一個行族、斷線行保留舊本體只在行尾加 `reason=`（過渡做法）。
- **LC-4 照現狀定案**：D58(1)、D58(8)、D37；並明確確認 D60 當時推定接受的 D58(3)(4)(5)（建議全部照此；也可以逐項改）。
- **LC-5 wait backend 的讀法**：(a) Engine `RoleThread::WaitBackend() const`（建議；讀到實際執行緒的後端，Windows 高解析計時器建立失敗而退回時也準確）；(b) 產品另建一個 Waiter 讀（不改 Engine，但可能和實際執行緒不同）。
- **LC-6 契約的 Design Invariants 節**：照 2.6 的 8 條與 Replacement Criteria 寫入（AGENTS.md §3 要你同意）。
- **LC-7 coalesced**：(a) 不加 Gateway 欄位，用導出的守恆式（建議）；(b) 加 `send_loop_replaced_snapshots`（Gateway 狀態欄位 +1，Architecture Delta）。
- **檔位**：實作 high、文件與執行 medium；records 的 `tick_overrun` 取時點局部 xhigh（約 1 h），若高於主對話的檔位需要你同意。

core 結束時（records 開始前）：

- **LC-8 移出的範圍**：rotation 拆分、逐筆到達事件、樣本流失、週期性 IPC 寫出延遲 → `observability-tools`（各項工時在 core 結束時列出），或其中幾項留在 records。
- **LC-9 L2**：(a) records 結束時跑第 7 節的只記錄 L2（建議；E1-5 的第一次對照不落在有判定的批次）；(b) 併進 `d44-regression-pv7` 的第一個跑次。

## 10. 對抗式檢查的對應（合成草案 → 第 1 版）

| 嚴重度 | 指出的問題 | 對應 |
|---|---|---|
| MUST | Tick／lane 間隔照晚醒的 bins 記，正常與停頓同格 | 改記超過名目週期的量＋停頓次數（`tick_overrun`、`ipc_lane`） |
| MUST | 「讀者遇到不認得的版本就停」對沒有版本鍵的凍結讀者不成立 | R2 改成換事件名或前綴；守衛加反向測試 |
| MUST | i/o timeout 拆成兩個原因與「統一」相反，漏了寫出逾時 | 兩條讀取路徑同一個 sentinel，寫出另一個（`gateway_disconnect`） |
| MUST | 新增 Gateway 欄位是要你同意的 Architecture Delta | LC-7 (a) 不加欄位 |
| MUST | 改斷線行會讓 `version_errors_test.go:210` 失敗、突變變 stale | 列入 `gateway_disconnect` 的工作與驗收 |
| MUST | 「被解析的行一定有 golden」當天就不成立；漏了兩個讀者 | 新切片 `a_grade_pins`；盤點範圍擴大；`ipc hint` 等升 A 級 |
| MUST | 和 protocol-v7 §1 的日誌規則衝突 | LC-3 |
| SHOULD | 對時行的 player 在 Leave 後會變 0 | 改用 worker 自己 session 的 id |
| SHOULD | 受擊原因由呼叫端推論，違反 State Authority | `Observe` 回傳 outcome |
| SHOULD | 下溢的修法不完整 | 在 `ResetWeaponFeedback()` 同時歸零基準值；禁止飽和減法 |
| SHOULD | overflow 計數是累計值，worker 行是視窗值 | worker 自己的視窗計數；樣本流失明確移出 |
| SHOULD | 只寫兩條 role；RoleContext 的存取器逼 tick 執行緒寫日誌 | 四條 role；`RoleThread::WaitBackend()` 由 owner 寫 |
| SHOULD | 停頓注入點沒指定 | 兩個注入點與 test seam |
| SHOULD | L2 量不到多 Client 與停頓；`--log` 和「不改 runner」矛盾；凍結副本有兩版 | 第 7 節改寫 |
| SHOULD | 守衛的檢出力沒被證明；沒沿用 `ingress_cross_check.py`；ingress_v1 golden 已過時 | 突變打在 fixture；沿用跨程序檢查；完整行 golden 放 `log_v1` |
| SHOULD | 週期性 IPC 寫出延遲被當成「已有」 | 改列入 LC-8 |
| SHOULD | 移出範圍寫死；rotation「沒有具名消費端」的說法不實 | LC-8 交你選；理由改寫 |
| SHOULD | LC-2(b) 的代價寫錯 | 改寫 |
| SHOULD | 「不得含別族前綴」和對時行族矛盾 | 改成錨定式與共用的 `prefixes.txt` |
| SHOULD | 估時偏低，會超過 1.5 倍 | 拆兩個子批、各自停損 |
| NOTE | gateway 來源接在 start 行尾，最後一欄是路徑 | 另寫一行 |
| NOTE | end 行缺例外路徑與 exit code；Windows 終止時沒有 | 補上並寫進契約 |
| NOTE | clock-sync golden 只有一行 body | 多列樣本、共用輸入、非有限值 |
| NOTE | Invariant 5 無法檢查；RC 和 R1 矛盾 | 改寫（2.6） |
| NOTE | 盤點與敘述對齊（N-* 數目、D60、D58-8、connection failed、行號、D57） | `decision_record` 與 2.5 |

## 11. core 的實作紀錄（2026-10-20，D142 之後）

| 切片 | 結果 | commit |
|---|---|---|
| `summary_wrap` | 照 §14.3：接縫 `SessionSummary.hpp`（`SummaryBaseline`、`RestartWeaponCounters`）先以現行行為寫出，Red 測試重現「同一幀內連線世代改變、phase 仍是 Playing」時的下溢；修正是在 `ResetWeaponFeedback()` 同時歸零基準值，Green 後留作回歸。突變 `lc-summary-baseline-kept` killed | `108ca0d` |
| `clock_sync_golden` | `fixtures/log_v1/clock-sync.tsv`（8 列：na、負 offset、四捨五入、`-0.000`、大值、nan、inf）由 C++ 與 Go 讀同一份；非有限的 drift 兩端都寫 `na`。Client 行改成 `[ObjectFPS/PvP] clock sync player=<id> `，id 取自 `ClientClockSession`（worker 自己 session 的身分），Leave 之後的最後一個視窗仍帶原 player。突變 3 個 killed | `63f67e3` |
| `coalesced_identity` | 不加欄位；`backpressure_test.go` 斷言「transport 總數 − 全域 `drop_coalesced_snapshots` ＝ 送出迴圈中途的取代」。突變 killed | `ade1af5`、`72a4098` |
| `a_grade_pins`、`frozen_read_guard` | `keys-v1.tsv`（18 族）＋`test_log_contract.py`（CTest `object_fps_pvp.log_contract`，9 個測試）；`prefixes.txt` 由 C++、Go、Python 共用；`ingress_cross_check.py` 也用 `network_statistics` 讀真實跑次。突變 3 個 killed | `4cc2ec6` |
| `contract_doc` | `docs/object_fps_pvp/log-contract.zh-Hant.md`；`protocol-v7` §1 改成連結；讀者盤點（`build/acceptance`、`tests/object_fps_pvp`、各 L2 的判定工具）：每個被解析的行族都在 A 級表，B 級行族沒有讀者 | 本次 |
| `decision_record` | INVENTORY：D-LOG、D58-1／3／4／5／8、N-d37、N-summary-counter-wrap、N-log-contract-vs-frozen-analyzers 結案，過時行號更正；`tt2-trace.md` 的「1 秒」更正 | 本次 |
| `gateway_disconnect` | D143 後套用 patch：讀取端兩條路徑同一個哨兵（活性逾時）、寫出逾時另一個、版本不符、協定錯誤（`protocolError`）、EOF；行改成 `runtime disconnected reason=<…> error=%q`。`version_errors_test.go` 的期望改成比對 reason（AGENTS.md §14.4：格式改了，判定的內容不變）；突變 `v7-flow-final-skips-runtime-failure`、`v7-flow-final-written-before-write`、`v71ve-envelope-version-unnamed` 的錨點同語意更新並重跑 killed；新突變 2 個 killed | records 開始時 |

本機：CTest `cpu|shader` 74／74、Go `./gateway/...` 全部通過、突變 `lc-*` 9 個全部 killed、`test_run_mutations.py` 通過。

和計畫的偏差（照實記錄）：

- **A 級釘住的語言**：計畫寫「寫出端的語言各有一個釘住測試」。直接以 `log.Printf`／串流寫出的行（Gateway 的 statistics、transport、evicted、start／HTTP，Match 的 start／ready／join／evicted／ipc）沒有可呼叫的組行函式，改由 Python 的 CTest 讀寫出端原始碼、抽出該敘述的鍵序列比對。它守得住改名、刪鍵、重排（突變證明），但不是執行期的輸出。
- **ingress 的完整行 golden**：計畫寫「完整的 ingress 行由 Go 與 C++ 的寫出端逐字比對」。改成用真實跑次的樣本由凍結讀者讀（未知鍵 0），寫出端的鍵序列由既有的 ingress 測試（含 `appendedZeros`）守住；沒有新增逐字 golden。
- 既有突變 `v71te-hello-without-echo` 的錨點因 `ClientClockSession` 的改寫而過時，照同一語意更新（它的 check 是 `worker`，那個測試本來就壞，`N-worker-probe-stale`）。

### LC-10（實作中發現，需要你同意）

`gateway_disconnect` 要用 `errors.Is` 分類斷線原因，需要在 Gateway 套件新增 3 個哨兵錯誤（套件變數，分在既有類別「immutable table」）與 1 個錯誤型別（`protocolError`，欄位分在「per-call context」）。`network-architecture.zh-Hant.md` §7 的 Gateway Design Invariants 規定：新增任何 Gateway 狀態欄位或套件變數，即使分進既有類別，都是要你同意的 Architecture Delta；計畫沒有列這一項。完整的實作（Go 測試全部通過，含 `state_whitelist_test.go` 的分類、`version_errors_test.go` 的期望改寫、分類的單元測試）存成 patch：`build/target/_build/test/logs/pvp-v71-log-contract-core-20261020/gateway-disconnect.patch`（另有新檔 `disconnect_reason_test.go`）。

- (a) 同意：套用 patch，更新突變 `v7-flow-final-skips-runtime-failure` 的錨點並重跑（建議；哨兵錯誤是不可變的值，不是判定用的狀態）。
- (b) 不加變數：以錯誤字串分類（脆弱，原計畫已否決）。
- (c) 這一項移到 `records` 或之後的批次。

### LC-8 的工時（records 開始前決定）

| 項目 | 留在 records 的做法 | 估計 | 移出的去處 |
|---|---|---|---|
| rotation 拆分（N-rotation-discarded-detail） | `match-ingress.jsonl` 新增 kind，升 version 2；`ingress_evidence.py` 的 trace 讀取要改並重新凍結 | 約 4 h | `observability-tools`；d44 可以由移動紀錄的 HostAccepted−Resolved 推出，不擋 d44 |
| 逐筆到達事件（E1-2 的剩餘） | 新的 JSONL 或新 kind（R4） | 約 3 h | `observability-tools` |
| 樣本流失（N-client-side-loss-counters 的一部分） | slack 每次只帶最小值；要在 Client 端計丟掉的樣本數 | 約 1.5 h | `observability-tools` |
| 週期性 IPC 寫出延遲（原第 10 批） | Gateway 每 10 秒的 `link_write_age_*`；是 Gateway 的新欄位，同樣要你同意（LC-10 的同一條規則） | 約 1 h | `observability-tools` |

建議全部移到 `observability-tools`（README 原估 10～13 h，加上這些約 19～23 h，到時候再拆）。

## 12. records 的實作紀錄（2026-10-20，D143 之後）

| 切片 | 結果 | commit |
|---|---|---|
| `gateway_disconnect`（core 遺留，D143） | patch 套用；突變：既有 3 個同語意更新、新 2 個，全部 killed | `a98bceb` |
| `tick_overrun`（E1-5，取時點局部 xhigh） | `TickPaceStatistics`：前後兩次取樣的間隔超過名目 Tick 的量（`tick_overrun_*`）、`tick_stalls`（>1.5 Tick）、`advance_*`；network 行尾追加。兩個注入點的測試都用既有方法：publish listener 停頓（`tick_late` 與 overrun 都反映）、醒來之後被 I/O 側持鎖（只有 overrun 反映：`tick_late` 約 0.1 ms、overrun 約 31 ms）。L1 只判結構性下界（FF-2）。突變 2 個 killed | `17eb1d1` |
| `ipc_lane`（E1-7） | 新行族 `ipc statistics version=1`：snapshot 從排入到開始寫出的等待、action lane 的晚送；IpcHost 持有、自己的小鎖、match_main 在視窗邊界交換取走。突變 2 個 killed | `3646b21`、`a3a1e59` |
| `client_records` | `LocalHitFeedback::Observe` 回傳判斷結果，Client 寫 `PvP hit feedback version=1`；摘要行尾依原因的拒絕數（`ShotRejectionName` 移到 `Combat.hpp`）；worker 自己的視窗流失計數。突變：既有受擊 5 個與新 2 個 killed | `7477ce4` |
| `role_started`、`match_end` | Engine `RoleThread::WaitBackend()`；四條 role 的 `role started version=1`；Match 最後一行 `end version=1`；Client 的 Gateway 指定元另寫一行；shots 檔關檔檢查。突變：Engine 1、產品 1 killed | `5692286` |

開發跑次（`build/target/_build/test/logs/pvp-v71-log-contract-l2-prep-20261020/dev/`，第 13 節的工具）發現並修正的問題：

- **`action_late` 的定義錯**：第 2 次跑次 action lane 的晚送最大 5.2 秒——閒置很久的 lane 保留上次送出的錨點，下一筆可以立刻送，從「到期」起算會把它算成晚。改成從「到期」與「這條 I/O 執行緒第一次看到它有待送內容」兩者較晚的時刻起算（`ActionLane::pendingSince`）；第 3 次最大 0.17 ms。
- **守衛的測試是恆真的**：`test_log_contract.py` 對 `command_evidence.read_trace_events` 的結果（generator）做真假判定。改成數筆數之後，原本的合成樣本過不了讀者的界限檢查，改用真實移動紀錄的前 200 筆。另把 match／gateway／probe 的樣本換成 `ingress_cross` 的真實跑次（含本批的新鍵），凍結讀者照樣讀得到。
- **樣本沒有進 repo**：`*.log` 被 `.gitignore` 排除，core 的四平台 L1（run 39422895712）全部在 `log_contract` 失敗；改成 `.txt`（`70ad10d`）。
- 工具的錯（第 1 次）：`pgrep` 同時比對到 runner 的命令列，停住的是 runner 不是 Match；改成錨定在命令列開頭。

本機：CTest `cpu|shader` 74／74、Go 全部通過、`test_run_mutations.py` 通過。

## 13. 只記錄的 L2 宣告（第 1 版，D144 核准）

沒有門檻、沒有通過或失敗的判定（所以不做 D45 的門檻來源檢查）；結果意外時交你決定，不在本批內修正後重跑（§14.1）。

- 工具：`build/target/_build/test/logs/pvp-v71-log-contract-l2-prep-20261020/run_lc_l2.py`（SHA-256 `60af302b…`）。輸出目錄：`build/target/_build/test/logs/pvp-v71-log-contract-l2-<日期>/`（不存在才執行）。
- 跑法：主 checkout 的建置，`run_quad.py --case clean-60 --rounds 1`（一個 probe 程序裡 4 個 Client）。Match 程序出現後 8 秒，對它 SIGSTOP、50 ms 後 SIGCONT（受控停頓）。clean 情境的 runner 判定可能因停頓而不成立（backlog 規則；開發跑次 2 不成立、3 成立），只記錄。只在本機 macOS 跑一次；Windows／Linux 的實機值留給 `lan15-windows` 與 `d44-regression-pv7`。
- 記錄項：
  1. 三份凍結清單（cp、rl、d117）與 `build/acceptance` 的逐檔核對。
  2. 每個不同版本的凍結分析器讀新日誌：`ingress_evidence` 567198（cp、rl，與 repo 相同）與 95ec65（d117）的狀態、停止、未知鍵；`network_statistics` 的視窗數；`command_evidence` 的事件數；edge-rate-limit 的 `gateway_scan`。例外照記。
  3. E1-5 的獨立來源對照：`tick_overrun_max_us`、`tick_stalls`、`tick_late_max_us`，對照 `match-commands.jsonl` 的 `snapshot_produced` 重算的間隔（最大、>1.5 Tick 的數目、停頓附近的間隔），以及「overrun＋1 Tick−trace 間隔」。
  4. `ipc statistics` 的每一行。
  5. 同一 probe 程序裡的 Client 對時行能否依 player 分開（出現的 player 集合）。
  6. `PvP hit feedback` 行數（probe 沒有產品 Client，預期 0；由 L3／LAN 涵蓋）、`role started` 的 wait backend。
  7. shots 與 ingress 的 `trace_end`、Match 的 `end` 行。
  8. `[log] dropped_lines`（runner 不給 Match `--log`，預期沒有）。
- 開發跑次 3 的值（工具驗證用，不是判定）：停頓後 `tick_overrun_max_us`＝50787、`tick_stalls`＝1、`tick_late_max_us`＝50801，trace 重算的間隔 67459 µs，差 −5.4 µs；對時行 player 1～4 都分得出；`ingress_evidence` 567198 passed、95ec65 failed（未知鍵：edge 系列加鍵之前的舊版，本批之前就讀不了 pv7-line 的日誌，只記錄）；`end` 行 `reason=signal exit_code=0`，`authority_tick=2`（房間清空時 Match 重設 Tick）。

## 14. 只記錄的 L2 結果（2026-10-20，D144）

目錄 `build/target/_build/test/logs/pvp-v71-log-contract-l2-20261020/`（`record.json`）。runner exit 0；受控停頓實際 53.0 ms。

| 記錄項 | 結果 |
|---|---|
| 1. 凍結清單 | cp 與 `build/acceptance` 0 差異；rl 2 個檔（`run_action_short.py` 與其測試，之後的批次改過）；d117 11 個檔（舊清單）。都是本批之前的狀況 |
| 2. 凍結分析器 | `ingress_evidence` 567198（cp、rl，與 repo 相同）passed、未知鍵 0；95ec65（d117）failed，未知鍵 13 個（edge 系列加的鍵，這份舊版本來就讀不了 pv7-line 的日誌）；`network_statistics` 讀到 Match 1、Gateway 4、worker 4 個視窗，沒有例外；`command_evidence` 讀到 Client 31370、Match 11783 筆事件；`gateway_scan` 8 行玩家行、沒有逐出 |
| 3. E1-5 對照 | `tick_overrun_max_us`＝39705、`tick_stalls`＝2、`tick_late_max_us`＝39808；trace 重算：最大間隔 56337 µs、>1.5 Tick 的 2 個、停頓附近的間隔 56337 µs；overrun＋1 Tick−trace 間隔＝34.4 µs（1 Tick 內）。停頓之外還有 1 次 >1.5 Tick 的間隔，兩種量法都數到 |
| 4. ipc statistics | 含停頓的視窗：snapshot 等待最大 4187 µs、action 晚送最大 18699 µs（停頓讓整個程序停住，IPC 也停了）；最後一個視窗：snapshot 等待最大 227 µs、action 晚送最大 190 µs |
| 5. 對時行 | 同一 probe 程序裡 player 1～4 都分得出 |
| 6. hit feedback／wait backend | hit feedback 0 行（probe 沒有產品 Client，如預期，由 L3／LAN 涵蓋）；`role started` 11 行，全部 `Kqueue` |
| 7. trace_end／end | shots `records=57 dropped=0`、ingress `records=4 dropped=0`；`end version=1 reason=signal exit_code=0 uptime_s=19.632 authority_tick=3`（Tick 小，原因見第 12 節） |
| 8. dropped_lines | 沒有（runner 不給 Match `--log`） |

沒有本批造成的意外。只記錄的觀察：d117 的舊版 `ingress_evidence` 已經不能讀現在的日誌，之後的批次若要用它當基準，要先換成 cp／rl 的版本。


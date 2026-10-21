# after-re-evaluate：這些項目還需不需要繼續（判斷清單，第 1 版）

- 狀態：**等使用者決定**。
- 依據：使用者（2026-10-21）：「先評估這份清單，然後優先處理系統和引擎的，其次是觀測工具，再來是測試、最後發行。」
- 範圍：
  - INVENTORY 處置為「re-evaluate 之後」的 37 項；
  - [HANDOFF](HANDOFF.md)「未結事項」裡沒進 INVENTORY 的項目。
- 做法：
  - 和 [re-evaluate](re-evaluate.md) 第 2 版相同：只判斷每項**還需不需要繼續**，附根據與重開條件，不寫修法與工時。
  - 證據由 4 個唯讀子 agent（檔位 medium）分組收集，判斷由主對話做。
  - 還需要的項目，依使用者的優先序分四類排列：系統與引擎 → 觀測工具 → 測試 → 發行。類內的順序只依依賴關係。
- file:line 以 `6fbb6a9` 為準。

## 1. 判斷

```
after-re-evaluate
├── 還需要（依優先序）
│   ├── 1 系統與引擎
│   │   ├── sdl-isolation（IP34-B12＋N-ip3-review-and-migration）「v8 音效的前置；產品的 SDL 直呼反而變多」
│   │   ├── C13                    「同一個事實存兩份，而且參與 slack 判定」
│   │   ├── N-silent-branches-assert「事故就是無聲丟棄；清單已過時，要重新列點」
│   │   └── C16（＋N-osyncstream） 「理由縮小成只剩 osyncstream，但旗標是 PUBLIC，擴散到所有依賴目標」
│   ├── 2 觀測工具
│   │   ├── N-compare-reports-untracked 「log-contract 指定的讀者只存在 git 忽略的目錄」
│   │   ├── N-trace-overwrite-on-restart「LAN 場次重啟就丟前一段 trace，現在靠手動改名」
│   │   ├── N-host-lock-keys-uncontracted「L2 直接讀的鍵不在日誌契約裡，文件也寫錯層」
│   │   └── C15                    「兩個分析器重複；含 105 的 6 處中有 3 處在這裡」
│   ├── 3 測試
│   │   ├── N-worker-probe-stale   「在 HEAD 會失敗；22 個 worker 突變因此判不了」
│   │   ├── N-token-test-gaps（只留存活突變） 「≤61 已不是契約，那一半取消」
│   │   ├── C17-3                  「重連時的世代檢查沒有任何測試」
│   │   └── N-client-other-version-untested「計數在契約 A 級行族，累加路徑沒有測試」
│   └── 4 發行
│       ├── lan15-windows（LAN15＋F7）「完整 pv7 從沒在 Windows 實機跑過」
│       ├── N-windows-max-path     「解壓到較深的資料夾就開不了遊戲」
│       ├── F1                     「關閉時卡約 3 秒」
│       └── N-sleep-reconnect      「睡眠醒來約 1 秒就斷線」
├── 交給 v8（隨 v7.1 已交過去的項目）
│   ├── B10                        「v8 README 已寫：B6 之前先做」
│   ├── N-correct-invariant-assert 「相位追蹤的不變式，B6 會動到」
│   ├── E3                         「B6 要和 09b 的 t_frame 數字對照時才碰到」
│   └── E5                         「A4 做跨建置比較時才碰到」
├── 有條件才需要
│   ├── F5                         「等 lan15-windows 的 Windows 數據（2026-10-09 的決定）」
│   ├── N-tt1-l2-wake-record       「lan15-windows 要量 Waiter 精度時才用」
│   └── N-phase-estimator-wan-noise「不變：有真實 WAN 測試時（D154）」
├── 不需要繼續（結案或取消）
│   ├── F2                         「tt2-trace 已做」
│   ├── C14                        「edge 批已刪除 Gateway 的衝突檢查」
│   ├── N-v8-deps-on-v71           「A1 已做；IP-3 那一半就是 sdl-isolation」
│   ├── N-gateway-race-flaky       「失敗過的那支測試已隨 edge 批刪除，v7.1 沒有新實例」
│   ├── C17 總項、C17-4            「C17-4 的 FireGate 已退役；其餘各自判斷」
│   ├── C17-1                      「產品只有一個發布者」
│   ├── C17-2                      「v7 就決定只記錄，PvpMatchTests 有 L1」
│   ├── C12                        「照 README：哪批改 token 規則就在那批做；目前沒有」
│   ├── F3                         「產品沒有讀者，時間基準已寫在註解」
│   ├── F4                         「v7 已列為已知限制，沒有觀測到」
│   ├── F6                         「從未重現」
│   ├── N-8ms-client-crosscheck    「動機是 FireGate 的常數，已隨 A1 消失」
│   ├── N-sigint-background-client 「一直只記錄，SIGTERM 與 Esc 可替代」
│   ├── N-hud-source-tick          「沒有 GUI 驗收要跑」
│   ├── N-drop-ledger-growth       「還沒到公開伺服器的規模」
│   └── HANDOFF 的 xhigh note（heartbeat-sync、edge-input、edge-results）「已分類或前提消失」
├── 不在四類（你決定）
│   └── G2                         「純文件，沒有功能依賴它」
└── 清單更正
    └── N-v7-skipped-acceptance    「D157 已解決，處置欄沒改」
```

「不需要繼續」的項目，每項都附重開條件：條件出現時再回來，不必重新盤點。

## 2. 還需要的項目

### 2.1 系統與引擎

| 項目 | 為什麼還需要 | 根據 |
|---|---|---|
| sdl-isolation（IP34-B12＋N-ip3-review-and-migration） | v8 的音效依賴 IP-3（`plans/v8/README.md` 的「音效」）。前置 TT-2 已完成。產品直接呼叫 SDL 的次數不減反增：tt2-trace 的 `ForwardEngineLog` 經 `SDL_LogMessage` 轉送（main.cpp 從 12 次變 14 次）。IP-3／IP-4 的 PLAN 還沒寫；D39 要求的公開介面 xhigh 審查也還沒做；D33 的遷移清單（ui_editor、object_fps_preview）同樣沒有 | `engine/platform/sdl/CMakeLists.txt:11-14`（`PUBLIC SDL3::SDL3`）；`SdlPlatform.hpp:8`、`:44`、`:75`；`PvpApplication.cpp` 20 次、probe 82 次 SDL_ 呼叫；`docs/architecture/plans/input-and-present/PLAN.md` 只有 IP-1、IP-2 |
| C13（被替代的序號存兩份） | 「這個序號被替代過嗎」由兩份資料回答，容量不同（64 與 600）：`track.substituted` 和 ingress 帳本。到達時用 `track.substituted` 算負 slack，負 slack 是 late 修正的輸入，所以這不只是診斷資料，也參與判定。〔推論〕兩份不一致時，slack 會漏算。違反「同じ事実を手動で維持する場所は一か所」（AGENTS.md §9） | `MatchRuntimeHost.cpp:17`、`:229-239`、`:654-662`；`MatchIngressLedger.hpp:9-12` |
| N-silent-branches-assert | 2026-10-10 的事故就是無聲丟棄。原清單的位置大多已被 edge 批重寫，但仍有無聲返回：`sendControl` 的 marshal 錯誤直接 return，evicted 時 `p == nil` 也直接 return；`MatchRuntimeHost.cpp` 另有 4 處丟棄回傳值。原清單過時，要對 HEAD 重新列點 | `gateway/server.go:613-617`、`:717-720`；`MatchRuntimeHost.cpp:375`、`:382`、`:419`、`:726`；產品內 0 個 `GYO_ASSERT` |
| C16（＋N-osyncstream-experimental） | `std::jthread` 已在 tt2-trace 消失，理由縮小成只剩 `std::osyncstream`，而 log-contract 又增加了它的使用處。旗標以 PUBLIC 掛在產品 domain 上，所有依賴目標（測試、probe）都會帶著 experimental 程式庫。`CMakeLists.txt:30` 的註解還寫著 jthread，已過時 | `apps/object_fps_pvp/CMakeLists.txt:30-32`；`tests/object_fps_pvp/CMakeLists.txt:282`；osyncstream 用於 `match_main.cpp`、`IpcHost.cpp:37`、`ClientConnection.cpp` 8 處等 |

### 2.2 觀測工具

| 項目 | 為什麼還需要 | 根據 |
|---|---|---|
| N-compare-reports-untracked（新，原在 HANDOFF） | log-contract 把它寫成 Match `gw_drop` 回報的讀者，三個 edge L2 也以雜湊載入它，但它只存在 git 忽略的證據目錄，清掉 build 就沒了。沒有其他工具做逐則比對 | `build/target/_build/test/logs/pvp-v71-drop-report-20261016/compare_reports.py`（`.gitignore:13`）；`log-contract.zh-Hant.md:27`；HANDOFF「未結事項」的 drop-report 項 |
| N-trace-overwrite-on-restart | LAN 場次中途重啟 Match，前一段的 movement trace、ingress 細節與 `-shots.jsonl` 都會被截斷覆寫；v7 的 LAN 手冊靠手動改名避開。TraceSink 已有 `append` 選項，但產品沒用。lan15-windows 之前就會碰到 | `engine/trace/src/TraceSink.cpp:102`；`MovementTraceWriter.hpp:79-80`；`MatchIngressTrace.hpp:255`；`MatchShotTrace.hpp:286`；`plans/v7/LAN_TEST.md:46` 等 |
| N-host-lock-keys-uncontracted（新，原在 HANDOFF 的 edge-input-passthrough note N5） | L2 的 judge 直接讀 `host_lock_*`，但這組鍵不在 log-contract 裡，分析器也不彙總。protocol-v7 第 9 節寫它在「IpcHost 層」，實作其實在 `MatchRuntimeHost` | `protocol-v7.zh-Hant.md:433`；`build/acceptance/object_fps_pvp/*.py` 沒有 `host_lock` |
| C15（`quad_evidence.py`、`command_evidence.py` 收斂） | 兩支仍各算一份命令指標（`command_metrics` 是 `analyze_commands` 的 N 人副本），docstring 自己標著 Code Smell。v8 的 N-analyzer-literal-105 要同步的 6 處裡，有 3 處在這兩支 | `quad_evidence.py:1-13`、`:37-45`、`:228`；`command_evidence.py:214`、`:230`、`:436` |

### 2.3 測試

| 項目 | 為什麼還需要 | 根據 |
|---|---|---|
| N-worker-probe-stale（新，原在 HANDOFF） | 本機實跑仍失敗（3.65 秒，`Invalid Match combat rules`，接著是 mock 等待逾時）。`mutations.json` 有 22 個突變用 `worker` 檢查，包括 token 的 4 個，目前都只能得到 wrong_failure。時間回聲的 probe 驗證也在這支測試裡。它不在 CI 的 `cpu|shader` 範圍，所以一直沒被發現 | `build/acceptance/object_fps_pvp/worker_main.cpp:82-84`；`ClientConnection.cpp:81`；`log-contract.md:224` |
| N-token-test-gaps（只留存活突變那一半） | 拿掉折讓式中的兩項時，突變會存活，這兩個突變仍未登記。「每日曆秒 ≤61」在 09a 已不是契約保證，那一半取消。判定要等 worker probe 修好 | `ClientConnection.cpp:678-680`；`mutations.json:2588-2625`；`plans/v7/09a-input-send-pinning.md:99` |
| C17-3（`SendInput(input, generation)` 的世代檢查） | 重連後擋掉舊世代輸入的檢查，沒有任何測試或突變。重連是真實會發生的路徑（睡眠、斷線） | `ClientConnection.cpp:855-858`；`ClientSimulationRole.cpp:180` |
| N-client-other-version-untested（新，原在 HANDOFF） | 計數在日誌契約的 A 級行族裡，但「收到他版 datagram → 加 1 → 寫進統計行」的累加路徑沒有測試，也沒有突變。只有判定函式與行格式有測試。Gateway 端有正向測試可對照 | `ClientConnection.cpp:500`；`WireTests.cpp:84-87`；`NetworkStatisticsTests.cpp:84-85`；`fixtures/log_v1/keys-v1.tsv:8` |

### 2.4 發行

| 項目 | 為什麼還需要 | 根據 |
|---|---|---|
| lan15-windows（LAN15＋F7） | 這是本基線最大的未驗證範圍：完整 pv7（三個原樣轉送、A1、token bucket）從沒在 Windows 實機跑過。前置 time-echo 已完成。F7（Windows Match 的 Tick 精度）只有 16 個窗口的數據，要靠這個場次補。v7 L3 的「關閉視窗後重新加入」與 `room_full` 也隨它補做 | `STABLE_BASELINE.md:12`、`:121`、`:128`；`engine/time/src/Waiter.cpp:145` |
| N-windows-max-path | 玩家把發行包解壓到較深的資料夾，Client 就開不了 shader（`.dxil` 檔名是 64 字元雜湊）。CI 的路徑短，所以抓不到。STABLE_BASELINE 沒有列這一項 | `client-20261016-104011.log:2`（臨時測試的證據）；repo 內沒有 `longPathAware` |
| F1（關閉時阻塞約 3 秒） | 玩家關閉 Client 時，會卡在同步的 HTTP `/leave`，最多 3 秒 | `ClientConnection.cpp:73-77`、`:278-292`、`:831` |
| N-sleep-reconnect（新，原在 HANDOFF） | 筆電睡眠醒來約 1 秒後就斷線，兩位玩家都 Leave。使用者已把它歸為體驗優化 | `ClientConnection.cpp:31`、`:654`；`STABLE_BASELINE.md:130` |

## 3. 交給 v8

這些項目還需要，但只有 v8 的 B6 或 A4 會用到。它們跟著 v7.1 已交過去的項目走，不在 v7.1 排。

| 項目 | 跟著誰 | 根據 |
|---|---|---|
| B10（相位追蹤的 golden） | B6 | `plans/v8/README.md:55` 已寫「相位追蹤不在 digest 裡，先做 phase-tracking-golden」。`RunLink` 仍不經 TrackSlack，35 個情境的相位樣本都是 0（`AuthorityDigest.cpp:529-531`） |
| N-correct-invariant-assert（D68 ①） | B6 | `Correct()` 仍只有註解、沒有 assert（`LocalPlayerPrediction.cpp:99-106`）。B6 會改到 late 修正之後的流程，這個決定要在 B6 動手前做 |
| E3（時間基準統一） | B6 | B6 要拿 09b 用 t_frame 量的「70～87 ms 邊界」做對照時才會碰到。如果 B6 只用 `phase_decision` 的時刻，E3 就不需要 |
| E5（probe 起點的幀相位） | A4 | 跨建置比較 b、a＋b、`legal_*` 時才會碰到；A4 改領先時要做前後對照。FireGate 退役後，常數凍結不再需要它 |

## 4. 有條件才需要

| 項目 | 條件 | 根據 |
|---|---|---|
| F5（期限前自旋） | 拿到 lan15-windows 的 Windows 晚醒數據時 | 2026-10-09 使用者決定先不加、等第 07 批與第 15 批的數據；要加的話是 Architecture Delta（`plans/v7/HANDOFF.md:459`） |
| N-tt1-l2-wake-record | lan15-windows 決定要量 Waiter 本身的精度時 | TT-1 已完成，數據已記錄；分支 `claude/tt1-l2-wake-record`（`a8f9535`）只剩保存程式碼的價值，master 沒有同類工具。分支要不要刪由你決定 |
| N-phase-estimator-wan-noise | 有真實 WAN 測試時 | 不變（D154、[re-evaluate.md](re-evaluate.md)）。lan15-windows 是 LAN，觸發不了 |

## 5. 不需要繼續

| 項目 | 判斷 | 根據 | 重開條件 |
|---|---|---|---|
| F2（LogFile 非同步） | 結案：已做 | tt2-trace 改走 `TraceSink`，由背景執行緒寫出，另加 crash flush（`LogFile.cpp:128-131`、`:137-198`） | — |
| C14（衝突檢查重疊） | 結案：已被取代 | Gateway 的 `commands` 與衝突檢查由 `6d9ecf5`、`45bda2b` 刪除，改成原樣轉送（`server.go:1094-1127`）。殘留的 0 值鍵是刻意保留的 v1 鍵 | — |
| N-v8-deps-on-v71 | 結案 | A1 已完成（`c6422c5`）；IP-3 那一半就是 sdl-isolation | — |
| N-gateway-race-flaky | 結案 | 失敗過的 `TestLinkResendsAnUndecidedRequestAtTheActionRateDespiteOtherWakes` 已隨 `45bda2b` 刪除；v7.1 多次 `go test -race -count=2/3` 都通過，沒有新實例 | Gateway 的測試在高負載下再次偶發失敗 |
| C17 總項、C17-4 | 結案 | C17-4 的 `FireGate.cpp` 已在 `c6422c5` 刪除；C17-1～3 各自判斷 | — |
| C17-1（時間倒退的意圖） | 取消 | 產品只有一個發布者（`PvpApplication.cpp:996`），觸發不了 | 新增第二個意圖發布者時 |
| C17-2（網路 probe 的伺服器端輸入逾時） | 取消 | v7 第 04 批就決定只記錄，`PvpMatchTests` 有 L1（`plans/v7/04-client-roles.md:182`）；之後沒有新證據 | 伺服器端輸入逾時出現缺陷時 |
| C12（token 規則三份拷貝） | 取消（照 README 的規則） | README 寫「哪個修正批直接需要（例如改 token 規則），就在那一批做」；目前沒有批次要改 token 規則。測試面的代價由 N-token-test-gaps 處理 | 有批次要改 token 規則時 |
| F3（AssetWatcher 的時間基準） | 取消 | 產品沒有讀者；`detectedNs` 是 system_clock，已寫在註解（`AssetChange.hpp:34`） | 有程式拿 `detectedNs` 和 Engine 的時間比較時（例如量熱重載延遲） |
| F4（D30 門檻逐級放大） | 取消（記為已知限制） | v7 STABLE_BASELINE 已列為低機率的已知限制；邏輯沒變（`ClientSimulationRole.cpp:55-73`），v7.1 沒有觀測到 | 觀測到發布間隔逐漸變長後，意圖過期判定失效 |
| F6（App Nap） | 取消 | 從未重現：v7 第 05 批 L3 與 v7 STABLE_BASELINE L3-1 都通過 | 切換 Spaces 或縮小視窗時斷線 |
| N-8ms-client-crosscheck | 取消 | 它要回答的是 FireGate／C1 的常數在 8 ms 狀態下是否成立；FireGate 已隨 A1 退役（D90）。OT-6 的 `timer_after` 已能抓到跑次中途的狀態轉換 | 又有常數依賴主機的計時狀態時 |
| N-sigint-background-client | 取消 | 一直標「只記錄」，有 SIGTERM 與 Esc 可替代；只影響腳本 | 腳本需要用 SIGINT 停 Client 時 |
| N-hud-source-tick | 取消 | 只有 GUI 驗收需要；v7 跳過的 GUI 輪不補（D157），區間規則可以運作 | 重新跑 GUI 的 HUD 判定時 |
| N-drop-ledger-growth（新，原在 HANDOFF：`ended`、Match 的串流表） | 取消 | 每位離開的玩家佔一個 map 項目；edge-contract 已把 `ended` 分類為診斷狀態（`state_whitelist_test.go:109`）。和 D90 取消輸入防偽造同理，還沒到那個規模 | Gateway 當公開伺服器長時間執行，或需要防濫用時 |
| HANDOFF 的 xhigh note | 結案或取消 | heartbeat-sync「只為測試存在的狀態」已由 edge-contract 分類（`state_whitelist_test.go:95`、`:145-148`）。edge-input N3 已列為允許類別（D120）；N10 已寫進語意（I0）；N5 移到上面的 N-host-lock-keys-uncontracted。edge-results N1、N2 隨 Gateway 不再持有結果帳本而被取代；N3 的前提已刪除；N4 只在 TCP 低於約 120 框／秒時有影響，loopback 可忽略 | N4：Match 與 Gateway 跨機器時 |

## 6. 不在四類

| 項目 | 狀態 | 根據 |
|---|---|---|
| G2（「時鐘到網路路徑」系列紀錄，5 集） | 一集都還沒有；純文件，沒有功能或版本依賴它。第 ⑤ 集的前置 time-echo 已完成；〔推論〕第 ④ 集的 FireGate 已退役，變成歷史紀錄。v7 第 08、08b、08c、09、16 批沒有 dev_log，這個系列是補記 | `plans/v7/HANDOFF.md:438`；`docs/checkup/documentation/2026_10_15_documentation.zh-Hant.md:76` |

## 7. 清單更正

- `N-v7-skipped-acceptance`：D157 已決定不補，處置欄卻還寫「re-evaluate 之後」。改成「已解決」後，「re-evaluate 之後」實際是 36 項。
- README「after-re-evaluate」表有三處過時：
  - `cxx-flag-removal` 還寫著 `std::jthread`；
  - `platform-fixes` 還列著 `LogFile` 非同步；
  - `smell-cleanup` 的「衝突檢查重疊」已經消失。
- HANDOFF「未結事項」的 5 項（N-worker-probe-stale、compare_reports、Client 的他版計數、丟棄帳本、睡眠重連）和 note 裡的 N5，沒有進 INVENTORY。本文件給它們 `N-` 代號，決定之後補進 INVENTORY。

## 8. 等你確認

- 第 1 節的判斷是否同意。特別是第 5 節的取消項（C12、F3、F4、F6、N-8ms-client-crosscheck、N-drop-ledger-growth），以及第 3 節交給 v8 的 4 項。
- G2 要不要做、排在哪裡。
- 決定之後，INVENTORY、README、HANDOFF 一起改。還需要的項目要拆成哪些批次，等你同意這份判斷之後再規劃。

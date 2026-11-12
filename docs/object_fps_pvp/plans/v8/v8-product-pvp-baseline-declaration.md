# v8-product-pvp-baseline 事前宣告

更新：2026-11-12。Owner：`object_fps_pvp`（量測對象與量測程式都屬產品；執行者是架構師開的任務卡）。來源：遷移計畫 `docs/architecture/plans/kernel/migration-plan.zh-Hant.md`（分支 `claude/arch-migration-plan`）§2.1 閘門 G2a／G2b、§3.2、§6 預算表 BU-1～BU-8。
分支 `claude/arch-pvp-baseline`，base origin/master `d0d8563f`（#197 已合併）。

**狀態：使用者 2026-11-12 說「開始」（任務卡：使用者直接說開始也算核定）；架構師同日備查無異議**（三個偏離照實寫進報告與限界；審查者改 Sonnet／high；`AuthorityDigest.cpp` 的兩個巨集與 `BaselineRecord.cpp` 是量測用，遷移批 `v8-product-match-sim` 接手時一併處理，不留成常態）。 量什麼、怎麼量、幾次、對帳與穩定性的規則都在看到任何數字之前寫下，看結果後不改（AGENTS §14.1）。

### 修訂

1. **（2026-11-13，架構師轉達使用者對 U8 的決定）追加：Docker Linux 容器內量測。** 量測機器分三層：Intel Mac 原生＝開發參考；**Docker Linux 容器＝閘門判定用**；Linux 實機只在出問題且允許時確認；CI runner 不用。本卡追加：
   - Dockerfile 放 `build/ci/linux-toolchain/Dockerfile`（不改 `.github/`），工具鏈照 L1 linux-x64 列實際使用的版本（2026-11-12 master 成功的 run 43244933991 的 log）：ubuntu 24.04、gcc-14 `14.2.0-4ubuntu2~24.04.1`、ninja-build `1.11.1-2`（apt）、cmake 3.31.6（runner 映像自帶；容器用 Kitware 官方二進位同版本）、Python 3.12、Go 1.23（setup-go 的 spec）。apt 套件清單照 `.github/actions/native-setup`。基底映像以 digest 釘住。
   - 容器內跑同一套量測（M1～M7，同樣次數、同樣對帳與穩定性規則），建置用 `test` preset 的組態（RelWithDebInfo），編譯器 gcc-14。本機數字照舊保留，報告兩欄並列，容器的絕對數字標「容器內」。
   - 記錄映像 digest、Docker Desktop 版本、VM 的 CPU 與記憶體、量測時的容器資源限制（`--cpus`、`--memory`）。
   - 容器建不起來就寫進限界，不卡住。
   - 範囲の限界追加：容器跑在 Docker Desktop 的 Linux VM 上（macOS 主機的虛擬化），不是 Linux 實機；計時含虛擬化的影響。

2. **（2026-11-13，架構師；不是放寬）重複 10 次、限定 CPU、安靜環境，容器與本機各量一組。** 第一組（白天、5 次）照原判定不出基準，結果保留。追加兩組，判定不變（p95 散佈 ≤10% 才出基準，散佈＝(max−min)／中位數）：
   - **容器組**：M1～M7 全部重複 10 次（重放 10 個行程、載入 10、冷啟動 10、場次 10×600 s）；`docker run --cpuset-cpus` 限定 CPU 並記 `--memory`；記映像 digest。
   - **本機安靜組**：同樣 10 次（使用者就寢、本機不跑其他建置）。
   - 量測期間不跑任何建置；容器組與本機組不同時跑。
   - 注意：10 次的 (max−min) 一般比 5 次大，同一門檻在 10 次下較嚴；照實報告，不調整。
   - 判讀：容器組穩＝以容器值當 BU 分母（標「容器內」）；兩組都不穩＝報告寫「µs 級分母在此機器量不穩」，(b)／(c) 由使用者定。

## 0. 為什麼（Feature → Contract → Architecture）

- **Feature**：沒有新 Feature。本批給 §6 預算當分母：G2a、G2b 以「今天的 Match」比較，但今天沒有任何量測，§6 的數字全是推定（AGENTS §2「性能や資源に関わる Feature の計画は、判定に使う予算を書く」）。
- **Contract**：不改 wire、digest 語義、golden。新增的只有量測輸出的格式：CSV／JSON 開頭帶 `format=gyo.kernel-baseline, version=1`（計畫 §3.2）。
- **Architecture**：不變。量測程式只加在 `tests/object_fps_pvp/`（owner＝pvp），不進 ctest（比照 `gyo_time_wake_record`「Deliberately not CTests」），所以不進 L1。

**本批只量、只記錄、不判。** 不做 G2a／G2b 判定，不拿數字對 §6 的候選預算，不提新預算值；報告只給每個 BU 一列「今天的值」。

## 1. 偵察結果（影響量法的事實）

1. **4 人是上限，8 人情境做不到**：C++ 側 `MaxPlayers = 4`（`apps/object_fps_pvp/include/RetroFPS/Sim/Movement.hpp:21`），`PvpMatch::Join` 第 5 人回 `match_full`；Gateway `adapter.MaxPlayers = 4`；兩張 release 地圖各 4 個出生點。計畫 §3.2 的「8 人長情境 ≥3,600 tick」要改產品才做得到，所以**改成 4 人（契約上限）長情境，暖機 600＋量測 3,600 tick**。這是對計畫前提的偏離，回報架構師；§6 BU-4 的「400 實體」與今天的 4 人差兩個數量級，報告照實寫。
2. **golden 是 37 個情境**（`authority_golden.txt`）：35 個直接驅動 `PvpMatch`（其中 7 個 prediction-* 情境也在每幀驅動 Client 預測），2 個（`host-play`、`host-poor-connection`）經 `MatchRuntimeHost::Advance`（＝`Step` 全段）。
3. **真實行程已有計時，不必改產品**：`MatchRuntimeHost::Body` 每圈記 `pace.RecordAdvance(advanced - sampled)`，正好是計畫 §3.2 的口徑「取樣時鐘後到開始等待前」；`match_main` 每 10 s 印一行 `network statistics`（`advance_*`、`tick_overrun_*`、`tick_stalls`、`tick_deadline_wakes` 等）。**限制**：這些是 `LateWakeStats` 的 10 格直方圖，格邊界 250／500／1000／2000／4000／8000／16000／32000／64000 µs，所以真實行程的 p50／p95／p99 只能給「≤ 格上界」，max 是精確值。要更細就得改產品（計畫 §3.2「量測要改產品程式才量得到→停」），本批不做。
4. **World 位元組可不改產品讀出**：`PvpMatch` 有 `friend struct PvpMatchTestAccess;`，`EntityModelTests.cpp` 已有先例在測試執行檔裡定義它。量測執行檔定義自己的版本，只讀各欄的大小。
5. **Linux 量不到**：沒有 Linux 實機可用；`cross-platform.yml` 的 workflow_dispatch 只跑既有 ctest 標籤，沒有能跑量測檔的入口，不改 `.github/` 就量不到；計畫 §6 也說 CI 共用 runner 的計時是雜訊不用。照 U8 建議：Intel Mac 當代理，寫進範囲の限界。
6. **macOS 沒有 CMake 建的 Gateway**（`gyo_app_add_go_service(... PLATFORMS linux-x64 windows-x64)`）：本機以 `go build ./gateway/cmd` 建一份，只當真實行程場次的對手，不量它。

## 2. 量什麼（每列對應 §6 的 BU）

| 代號 | 指標 | 形式 | 對應 BU | 樣本 |
|---|---|---|---|---|
| M1 | 一個 authority step 的時間：`PvpMatch::Tick`（match 組）或 `MatchRuntimeHost::Advance`（host 組）；p50／p95／p99／max／mean | 重放 | BU-1 分母 | golden 37 每個 tick（一遍約 1.6 萬 tick）＋long4 量測段 3,600 tick；每次重複一個行程 |
| M2 | World 位元組：實體欄（`bodies_`＋`combats_`＋`animations_`，列數×元素大小）、預留容量、輸入來源（`actionLedgers_`＋`connections_` 的密集部分與巢狀容器估計）、tick 快取 | 重放（long4） | BU-4 World 分母 | long4 每個 tick 後量一次，記暖機結束、峰值、結束三點 |
| M3 | 載入到第一個 tick：讀 product arena 檔（`pvp_arena.json`）→建 `PvpMatch`＋4 人 Join→第一個 `Tick` 完成；從該模式第一行起算，行程啟動不含 | 重放 | BU-6 G2a 分母 | 10 個行程各一次 |
| M4 | 真實行程一次 Advance 的時間（`advance_*` 直方圖合併）；p50／p95／p99 為格上界、max 精確；Advance ≥16 ms 的次數（直方圖第 8 格起） | 真實 | BU-1′ 分母 | 5 場 × 600 s，每場約 3.6 萬次 |
| M5 | 真實行程 tick 間隔超出名目值（`tick_overrun_*`）與 stall 次數（間隔 >1.5 tick） | 真實 | BU-1′ overrun 欄 | 同 M4 |
| M6 | Match 行程 RSS：穩態（開場 60 s 後的取樣中位數）與峰值 | 真實 | BU-4 RSS 分母 | 同 M4，每秒取樣一次 |
| M7 | 啟動到可接受 join：spawn 行程→Match log 出現 ready 行（IPC 開始收 Gateway；log 每 1 ms 輪詢一次） | 真實 | BU-6 G2b 分母 | 10 次冷啟動 |

BU-2、BU-3、BU-5、BU-7、BU-8 今天沒有對應物（kernel 開銷、finish、Client 的 PredictionHistory、Client 幀、頁保護），報告寫「不適用」與理由，不量。

## 3. 怎麼量

### 3.1 新增與修改的檔（全在 `tests/object_fps_pvp/`）

- `AuthorityDigest.cpp`：加兩個巨集。`PVP_DIGEST_TIMED_STEP(...)` 預設展開成原敘述（三處：`Duel::Tick` 的 `match_.Tick`、`RunHost` 的 `host.Advance`、`RunLink` 的 `match.Tick`）；`PVP_DIGEST_NO_MAIN` 定義時不編 `main`。digest 執行檔不定義它們，所以行為與輸出不變——以 `object_fps_pvp.authority_digest` ctest（golden 37 與 -O0 比對）確認。
- `BaselineRecord.cpp`（新）：定義上面兩個巨集（只把 authority step 包進 `steady_clock`；snapshot 編碼、SHA-256、腳本輸入都在計時外）後 `#include "AuthorityDigest.cpp"`，所以重放情境就是 digest 的情境本身。模式 `--replay`（M1、M2）、`--load`（M3）。
- `CMakeLists.txt`：加 `gyo_object_fps_pvp_baseline_record`，**不加 add_test**。
- `pvp_baseline_session.py`（新）：真實行程。模式 `start`（M7）、`session`（M4～M6）。

### 3.2 步驟

建置：`test` preset（RelWithDebInfo、Apple clang、x86_64），本 worktree 的 `build/target/_build/test`。

1. **重放（M1、M2）**：`baseline_record --replay` 跑 5 次（5 個行程，依序、間隔 ≥5 s）。每個行程先跑一遍不計時的 golden 37（暖機），再跑計時的一遍（scale 1），再跑 long4（前 600 tick 不計）。
2. **載入（M3）**：`baseline_record --load` 跑 10 次（10 個行程）。
3. **冷啟動（M7）**：`pvp_baseline_session.py start --cold-starts 10`，Match 以 `--arena pvp_arena.json`（與重放同一張）啟動，見到 ready 行就送 SIGINT，要求 exit 0；兩次之間停 1 s。「冷」＝新行程；OS 檔案快取不清（清快取要 sudo `purge`，不做）。
4. **場次（M4～M6）**：`pvp_baseline_session.py session --runs 5 --duration 600`：每場 Match（`pvp_arena.json`）→Gateway→`gyo_object_fps_pvp_quad_probe`（4 個 headless Client、60 FPS、會互打的 bot、`--duration 600`）；探測結束後先停 Gateway 再 SIGINT Match，要求 Match exit 0。RSS 用 `ps -o rss=` 每秒取樣。

證據放主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-baseline/`（git 管理外）：每次重複的原始 CSV／JSON、每個 tick 的樣本（`--samples`）、各場 log、機器資訊、建置資訊。

## 4. 件数の突き合わせ

| 量測 | 送入 | 量到 | 規則 |
|---|---|---|---|
| M1 golden | 每個情境的 authority step 數 | 計時樣本數 | 非 prediction 情境：樣本數＝digest 記錄數（一個 step 一筆記錄）；prediction-* 情境：記錄含每幀的預測紀錄，樣本數＝以 `RunLink` 同一算式重算的 tick 數（scale 1 為 360）。不等就該次重複失敗 |
| M1 golden | 情境內容 | digest 值 | 每個 golden 情境的 digest 必須等於 `authority_golden.txt`（證明跑的是同一批情境、計時掛點沒改行為）；且 golden 數＝檔內列數 |
| M1 long4 | 4,200 tick | 量測樣本 3,600、digest 記錄 4,200 | 兩者都要相等；long4 的 digest 記進報告，後續重放 host 用它證明跑的是同一情境 |
| M3 | 10 次 | 10 列 | 每次 `TickCount()==1` |
| M4／M5 | Match 主迴圈每圈記一次 wake、interval、advance | 每個統計視窗的 advance 格數總和、overrun 格數總和、`tick_deadline_wakes + tick_notified` | 三者逐窗相等（同一個鎖區內記錄）；不等的窗數記為 `identity_breaks`，必須為 0 |
| M4 | 60 Hz × 印出視窗的秒數總和 | Advance 總數 | 記錄比值（追趕時一次 Advance 跑多個 tick，比值可略低於 1），不判。**量測前修訂（試跑發現）**：原寫的 end 行 `authority_tick` 不能用——Gateway 斷線時 IpcHost 重置 runtime，試跑的值是 3 |
| M6 | 每秒一次 | 取樣數 | 記錄取樣數與穩態取樣數 |
| M7 | 10 次 | 10 列 | 每次 exit 0 |

報告第一行照 AGENTS §14 寫「件数の突き合わせ：一致／不一致／確認できない」。不一致或確認不能時，該指標不出基準，等使用者判斷。

### 4.1 試跑（不算量測；2026-11-12）

冷啟動 2 次、25 s 場次 1 場，只驗流程，數字不進報告。發現：end 行 `authority_tick` 被重置（上表 M4 改對帳方式）；統計視窗涵蓋 Match 的整個生命（bot 加入前約 1～2 s 也在內，最後一個未滿 10 s 的視窗不印），寫進限界。M7 的 ready 時間含 `match_main` 開頭對執行檔算 SHA-256 與載入動畫、hitbox 內容。

## 5. 統計與穩定性

- 百分位：重放用最近秩（排序後第 ⌈q·n⌉ 個）；真實行程用 `LateWakeStats::QuantileUpperBound` 同一算法套在 5 場各自合併的直方圖。
- 每次重複算一個 p95；**基準值＝5 次 p95 的中位數**（計畫 §6 BU-1「同機 5 次取每次 p95 的中位數」）；**散佈＝(max−min)／中位數**。M3、M7 取 10 次的中位數，散佈同式。
- **散佈 >10%**：寫原因候選（機器負載、其他行程、熱節流、計時器解析度、GC 以外的背景工作），並在 2 h 內最多重量一次（只改量測時機，例如等其他會話停下、負載平均降下來；不改方法、不改門檻）。重量後仍 >10%＝該指標「量不穩、不出基準」，停下回報（計畫 §3.2「若失敗→停下改口徑」由架構師決定）。第一次的結果保留在證據與報告裡，不被重量覆蓋（§14.1）。
- **M4 的散佈沒有檢出力**：p95 是格上界，5 場落在同一格時散佈恆為 0，報告照實寫；另列 max 的散佈當參考。
- 量測時記負載平均（每次重複前後各一次）與同時跑的行程（`ps` 前 10 名 CPU）。

## 6. 不判什麼

- 不判 G2a、G2b，不拿任何數字對 §6 的候選預算比，不提「預算該改成多少」。
- 不判今天的 Match 夠不夠快；超過 16.67 ms 的次數只記錄。
- 不比較 Mac 與 Linux（沒有 Linux 數字）。

## 7. 範囲の限界

1. **機器**：Intel Mac（MacBookPro16,1、i9-9980HK 8 核 16 執行緒、64 GB、macOS 26.7.1、接電源）是代理值（U8）；計畫說真實行程基準只在 Linux 有判定效力，本批沒有 Linux 數字。開工時負載平均約 3.2（有其他會話在跑），量測時記錄。
2. **真實行程的解析度**：p50／p95／p99 是 250 µs 起跳的格上界（§1 第 3 點）。
3. **人數**：4 人，不是計畫的 8 人（§1 第 1 點）。
4. **M3 不含行程啟動**（dyld、靜態初始化）；M7 含行程啟動但不含 Gateway 連上後的 Ready 交換與第一個 join（「可接受 join」以 IPC 開始收連線為準）。
5. **OS 檔案快取不清**；冷啟動只代表新行程。
6. **RSS 用 `ps` 每秒取樣**，不是 VmHWM；峰值可能漏掉 1 s 內的尖峰。
7. **Gateway 在 macOS 是手動 `go build` 的**，不在 CMake 的平台清單裡；它不是量測對象。
8. **Windows 不量**（主戰場是 Linux，計畫 §3.2）。
9. **M4～M6 的視窗涵蓋 Match 整個生命**：bot 加入前約 1～2 s 在內；最後一個未滿 10 s 的視窗不印、不計。

## 8. 機器負載（記憶 warn-before-heavy-local-runs）

全部 headless、記憶體小（Match 預期數十 MB），不用 GPU。場次 5×約 10.5 min≈55 min 牆鐘時間，期間 Match＋Gateway＋4 個 Client 約占 1～2 個核心；其他量測合計數分鐘。不會讓機器卡住；量測期間其他會話的重建置會讓數字變吵（§5 處理）。

## 9. 模型、檔位、審查

- 實作：本會話（開卡時 Fable 5.1，2026-11-12 起由使用者切到 Opus 5.5）／medium，量測程式與統計 high。
- 審查：**Sonnet／high 只讀**（架構師 2026-11-12 核：實作者是 Opus，審查者要不同模型；本批是下段的純量測，照 navigator 用 Sonnet，不用 Fable）。重點：量法與統計是否照本宣告、報告的數字對得上原始輸出。
- 驗證者：不需要（沒有改任何判定）。

## 10. 估時與停止

估 4～6 h（任務卡）。單一指標超過 2 h 量不穩就停下回報。§14 冒頭的停止條件（同一指標換兩次量法仍無結論、原因說明會改計畫前提、發現回歸）照常適用。

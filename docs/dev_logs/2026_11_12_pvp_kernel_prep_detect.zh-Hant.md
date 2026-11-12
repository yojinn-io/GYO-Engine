# PvP v8：v8-product-kernel-prep-detect（kernel 改造前置第一批：偵測與固定現況）

日期：2026-11-12。Owner：`object_fps_pvp`（執行者：架構師會話開的任務卡）。計畫：交接卡 [kernel-prep](../object_fps_pvp/plans/v8/kernel-prep.zh-Hant.md) 第一批；[事前宣告](../object_fps_pvp/plans/v8/v8-product-kernel-prep-detect-declaration.md)架構師 2026-11-12 核定。base origin/master `1e7625d9`。
- 執行：實作者＝本會話（Fable 5.1；D1／D3 high、D2 掃描段 xhigh、文件 medium）；審查者＝`xhigh-reviewer` 子代理（Opus／xhigh，唯讀），只審 D2 的掃描器與 tick 側六函式的判斷。
- 性質：只加 `tests/object_fps_pvp/` 的特徵測試、一支 Python 守衛、六條突變與 CMake 登錄。不改玩法、wire、digest、任何設計；`apps/`、`engine/`、`framework/`、`tests/common/`、`build/`、`.github/` 零修改。

## 起因

遷移批要決定三件事（Life 的漏斗形狀、牆鐘與 IO 執行緒碰權威的讀點怎麼搬、`connectionQualityFailures` 的歸屬），audit §4.1／§3.4 與 spec G.4 只有讀碼的描述，沒有可量、可判的現況紀錄。本批先把現況固定成測試與守衛（「量不到先建檢測」），遷移批據此寫事前宣告。

## 做了什麼

| 項 | 檔 | 內容 |
|---|---|---|
| D1 Life 的寫者與時序 | `tests/object_fps_pvp/KernelPrepDetectTests.cpp`（2 個 TEST_CASE） | 兩人對射到致死，Match 層與 host 發布路徑各一次。實跑：4 槍、致死決定在 **T=35**，snapshot(35) 已 Dead、`lifeStateTick=35`、`respawnTick=215`；snapshot(214) 仍 Dead；snapshot(**R=215**) Alive、`lifeStateTick=215`、`lifeGeneration` 1→2、`respawnTick=0`；R−T=180＝`respawnTicks`。兩層數值相同。 |
| D2 讀點棘輪 | `tests/object_fps_pvp/test_kernel_prep_sites.py`（ctest `object_fps_pvp.kernel_prep_sites`，`cpu;pvp`） | 表＝(類, 檔, 函式, 呼叫形狀)→次數，與白名單相等才過。白名單 47 列、55 個命中：W 牆鐘 12 列、G 全域診斷 13 列、A 模擬 role 以外的執行緒碰權威 22 列。檔頭寫明「白名單是現況紀錄，不是許可」。審查後（第二版）W 類涵蓋任何 `*[Cc]lock::now()`、`Engine::Time::Now*()`、host 的 `Now()`；HOST_TOUCHES 補到所有同步碰 `match_` 的 host 方法（9 個，今天 IpcHost 只呼叫 4 個）；定義跳過只跳函式自己的名字（initializer list、預設引數的讀取會算）。自測 7 條：經真實 `table()` 合成插入 `now_()`／`Engine::Time::Now()`／`match_.TickCount()` 必報新列、同函式多一處必報次數變、刪 `Now` 的讀點必報缺列、`Step` 內加 `match_` 不多列（tick 側過濾）、TICK_SIDE 釘住審過的六個、D3 的 `==` 不算寫、語法解析對已知形狀（class 標頭含括號、initializer list、預設引數、區塊註解、字串與 char literal、lambda、for、定義 vs 呼叫）。 |
| D3 failures 的寫點 | 同 D1 的檔（2 個 TEST_CASE）＋守衛加一條 | host 層（劣化者 id=6 較大）：**W=600**，failures 在 tick **1200**（2W）變 1、**1800**（3W）變 2、**2400**（4W）逐出；之間每 tick 都帶同值；id=5 全程 0。Match 層：Starvation 重置（`resets=1`）前後 42 個 `Snapshot()` 的 failures 全為 0。守衛：`src/Pvp` 內寫點只有 `MatchRuntimeHost::JudgeConnectionQuality` 一處，`PvpMatch.cpp` 零處。 |
| 突變 | `tests/object_fps_pvp/mutations.json`（6 條，batch `v8-product-kernel-prep-detect`；新 check `kernel_prep_detect`＝ctest 只跑 `-tc=kernel-prep*`、`kernel_prep_sites`） | `kp-death-next-tick`（Kill 不寫、下一 tick 補殺）、`kp-respawn-next-tick`、`kp-failures-other-object`、`kp-failures-judgement-tick-only`、`kp-site-new-wall-clock`、`kp-site-new-authority-touch` |
| CMake | `tests/object_fps_pvp/CMakeLists.txt` | 新檔加進 `gyo_object_fps_pvp_tests`；新 ctest `object_fps_pvp.kernel_prep_detect`（只跑 kernel-prep 案例，突變由它判）、`object_fps_pvp.kernel_prep_sites` |

## 受理判定（宣告 §4）

件数の突き合わせ：突變 6 條 → killed 6／survived 0／stale 0／wrong_failure 0；golden 37／比對 37／差 0；白名單 47 列＝掃描 47 列。

| 判定 | 結果 |
|---|---|
| J1 D1 過 | 通過（4 案例 18,131 斷言；T=35、R=215、R−T=180） |
| J2 D1 突變 | `kp-death-next-tick`、`kp-respawn-next-tick` killed。check `kernel_prep_detect` 本身就是只跑 `-tc=kernel-prep*` 的 ctest，expect `kernel-prep D1` 只出現在新測試的斷言訊息，所以 killed＝新測試自己的斷言失敗（不是既有測試） |
| J3 D2 守衛過 | 通過（ctest；W／G／A 各 ≥1 列，表相等） |
| J4 D2 自測＋兩條守衛突變 | 自測 7 條通過；`kp-site-new-wall-clock`、`kp-site-new-authority-touch` killed（expect `QueueJoin`＝守衛報出的新列；守衛改版後重跑 2／2 killed，`j4_sites_mutations_rerun.txt`） |
| J5 D3 過 | 通過（同一次執行） |
| J6 D3 突變 | `kp-failures-other-object`、`kp-failures-judgement-tick-only` killed（同 J2 的理由，expect `kernel-prep D3`） |
| J7 不變 | `object_fps_pvp.authority_digest` 通過；`git diff --stat 1e7625d9 -- fixtures AuthorityDigest.cpp` 為空 |
| J8 既有全過 | 本機 `ctest -L pvp` 57／57 通過（含 `cpu` 53、`network` 5、`acceptance` 30；`mutation_tool` 目錄守衛在內），216 s（`j8_ctest_pvp.txt`） |
| J9 零修改 | `git diff --stat 1e7625d9`：只有 `tests/object_fps_pvp/`（`CMakeLists.txt`、`mutations.json`、新檔 2）與 `docs/`（宣告、HANDOFF、交接卡、README、本 dev_log）；`apps/`、`engine/`、`framework/`、`tests/common/`、`build/`、`.github/` 零（`j9_diff_stat.txt`） |
| J10 L1 | 待 PR（三平台） |

## 回報架構師：spec G.4 補列候選

守衛掃出、audit §3.4／spec G.4 沒列的「模擬 role 以外的執行緒碰權威」（都在 `MatchRuntimeHost.cpp`，持 `mutex_`；執行緒依審查 S5 核對呼叫者）：

1. **IPC 執行緒**：`IngressBucketId` 的 `match_.ContainsPlayer`（`SubmitInput`／`SubmitActionBatch`／`NoteWireRejection` 的計數桶要知道玩家是否在 Match）；`RecordRejection` 的 `match_.ContainsPlayer`、`match_.Snapshot()`（拒收紀錄抄 cursor／epoch／life）。這兩個是 G.4 第 5 條的同類，建議併入第 5 條或補列。
2. **主執行緒（`match_main`）**：`TakeIngressStatistics` 的 `match_.Snapshot().players`（`match_main.cpp:178`，role 執行中、每個統計視窗為每個玩家建桶）；`TickCount()` 的 `match_.TickCount()`（`match_main.cpp:220`，Stop 之後，無並行）。前者是執行中的跨執行緒讀，後者只是收尾。
3. **沒有正式呼叫者**：`QueueActionAcknowledgement`（`match_.CanAcknowledgeActions`）、`SubmitActions`（經 `IngressBucket`）只有測試在用；遷移批可以直接刪或併入。
4. `IpcHost.cpp` 的 `host.GetActionResults` 四處（`Receive`、`Pump`、`ScheduleWrite`、`Write`）與 `host.NoteWireRejection` 兩處（`Receive`）同步讀權威；G.4 只列了 `:645`、`:685`。

另：`MovementTrace.hpp` 的 `TraceMovement` 本身也讀 `MovementTraceNowNs()`（G.4 第 3 條只提 `:59` 的 `steady_clock`）。

## 審查（D2）

`xhigh-reviewer`（Opus／xhigh，唯讀，約 10 分）：must 0、should 6、note 3，全文在證據目錄 `review_d2_opus.md`。should 全部套用：S1 W 類補 Engine 時鐘與別名；S2 定義跳過改成只跳函式名；S3 HOST_TOUCHES 補到 9 個；S4 既有包裝的規避寫進檔頭與宣告 §5；S5 兩列的執行緒性質改正（主執行緒／無正式呼叫者）；S6 自測從 3 條補到 7 條。套用後對守衛自身做 10 條記憶體突變（審查列的 a～g 加取最內層範圍），**10／10 killed**（`j4_guard_selftest_mutants.txt`）。審查時白名單 47 列，修正後仍 47 列、表不變（新增的形狀今天都是 0 命中，只收緊）。

## 經緯

- 宣告寫好後送架構師核定；核定補四點（模型不切、三個未列讀點照現況進白名單並在此回報、golden 以 37 為準、「次數變也失敗」接受）。
- 守衛第一次執行就報出我手寫白名單的四處錯誤（`IpcHost.cpp` 的函式是 `Receive`／`Pump`／`ScheduleWrite`／`Write` 而非我猜的 `RunSession`；`Step` 的 `TraceMovement` 是 2 處、`Body` 是 0；`TraceMovement` 本身讀 `MovementTraceNowNs`）。逐一對原始碲核對後是掃描器對、白名單錯，照掃描結果修白名單。這也是正例驗證：掃描器找得到真實的差異。
- 特徵測試第一次執行 1 個失敗：D3 的件數檢查沒把 tick 1 的快照算進（測試自己的記帳，不是行為）；另 `const char*` 被 doctest 印成指標。兩處都在測試檔內修正。
- 數值全部來自實跑（斷言訊息印出），沒有手算；宣告只寫公式（2W、3W、4W、T+respawnTicks）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-kernel-prep-detect/`（git 管理外，Intel Mac）：`j1_j5_detect_run.txt`（doctest 輸出含 MESSAGE）、`j3_j7_ctest.txt`、`j7_golden_diff.txt`、`j2_j4_j6_mutations_run.txt`／`.json`、`j4_guard_selftest_mutants.txt`、`j8_ctest_pvp.txt`、`j9_diff_stat.txt`、`review_d2_opus.md`。

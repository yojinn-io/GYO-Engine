# v8-product-kernel-prep-detect 事前宣告

更新：2026-11-12。Owner：`object_fps_pvp`（內容歸產品；執行者是架構師開的任務卡）。來源：交接卡 [kernel-prep](kernel-prep.zh-Hant.md) 第一批。出處：audit `docs/architecture/plans/kernel/audit.zh-Hant.md` §4.1、§3.4、§1.3；spec `docs/architecture/plans/kernel/spec.zh-Hant.md` G.4。
**狀態：架構師會話 2026-11-12 核定**（補四點：模型不切換、Fable 5.1 照使用者所開；audit 未列的三個 IO 讀點進白名單並在 dev_log 單列「回報架構師：spec G.4 補列候選」；golden 以 37 為準，交接卡的 34 由架構師改；「次數變也失敗」接受，無關重構導致次數變就停下回報不放寬）。判定與數字在看到任何結果之前寫下，核准後不放寬（AGENTS §14.1）。
分支 `claude/arch-pvp-kernel-prep-detect`，base origin/master `1e7625d9`（#188 已合併）。

## 0. 為什麼（Feature → Contract → Architecture）

- **Feature**：沒有。功能線 2026-11-11 起停止（停法 (a)）。本批是 kernel 改造的前置：把遷移批要決定的三件事（Life 的寫者與時序、牆鐘與 IO 執行緒碰權威的讀點、`connectionQualityFailures` 的歸屬）先變成可量、可判的現況紀錄，遷移批據此寫事前宣告，不憑讀碼推測。
- **Contract**：不改。digest（version 4）、golden、wire、玩法全部不變。
- **Architecture**：不改。只在 `tests/object_fps_pvp/` 加特徵測試、一支 Python 守衛與突變條目。白名單是**現況的紀錄，不是許可**。

## 1. 現況（偵察結果，行號是 `1e7625d9` 的）

| 項 | 看到的 |
|---|---|
| D1 Life | 死亡：`PvpMatch::Decide`（`PvpMatch.cpp:430`）在 `hp == 0` 時呼叫 `Kill`（`:301`），同一 tick 內寫 `lifeState = Dead`、`lifeStateTick = tick_`、`respawnTick = tick_ + respawnTicks(180)`；`Decide` 由 `ResolveActions` 在 `Tick` 內呼叫（`:618`），`Snapshot()`（`:697`）是 `bodies_` 的複本，所以 Tick(T) 之後的 Snapshot 已是 Dead。復活：`ResolveLifeBoundaries`（`:310`）在 `Tick` 開頭、`tick_ >= respawnTick` 時寫 Alive、`lifeStateTick = tick_`、`lifeGeneration + 1`。既有測試（`CombatTests.cpp:466-470`、`:539-545`）查了 `lifeStateTick` 與 `respawnTick` 的值，但沒有一條以「同 tick 讀後寫」為題固定 snapshot(T)／snapshot(R−1)／snapshot(R) 三個時點，也沒有從 host 的發布路徑（`MatchRuntimeHost::Step` → `TakeSnapshot`）看。 |
| D2 讀點 | 牆鐘：`MatchRuntimeHost.cpp` 的 `now_()` 在 `SubmitInput`（`:218`，IPC 執行緒）、`Step`（`:445` ShotReferenceAge、`:483` 發布時刻）、`Now`（`:604`）、`TrackSlack`（`:670`）；`steady_clock::now()` 在 `SubmitInput`／`Step`（lockWaits 診斷，`:162、164、370、372`）、`Body`（`:522、525、529`）；`MovementTraceNowNs()` 在 `RecordRejection`（`:121`）、`Step`（`:449`）。全域診斷入口：`PvpMatch.cpp` 的 `TraceMovement` 在 `ResetMovementEpoch`（`:514`）、`Tick`（`:586`）；`MovementDiagEnabled`／`RecordMovementDiag` 在 `Tick`（`:569、580`）；`LocalPlayerPrediction.cpp` 的 `MovementDiagEnabled`／`RecordMovementDiag` 在匿名函式 `PredictStep`（`:21、31`）、`TraceMovement` 六處（`:110、140、249、281、418、441`）。`Movement.cpp` 零命中（G.4 第 3 條）。IO 執行緒碰權威（`match_.` 在非 tick 側的函式）：`IngressBucketId`（`ContainsPlayer`）、`RecordRejection`（`ContainsPlayer`、`Snapshot`）、`SubmitInput`（`AdmitInput`、`Snapshot`、`RequestMovementResync`、`TickCount`）、`SubmitActionBatch`（`ContainsPlayer`、`CanAcknowledgeActions`、`CanSubmitActions`、`GetActionResults`）、`QueueActionAcknowledgement`（`CanAcknowledgeActions`）、`GetActionResults`（`GetActionResults`）、`TickCount`（`TickCount`）、`TakeIngressStatistics`（`Snapshot`，audit 未列）。審查（D2 S5）核對呼叫者：`IngressBucketId`、`RecordRejection`、`SubmitInput`、`SubmitActionBatch`、`GetActionResults` 在 IPC 執行緒；`TakeIngressStatistics`、`TickCount` 由 `match_main` 在主執行緒呼叫（前者在 role 執行中、持 mutex）；`QueueActionAcknowledgement` 沒有正式呼叫者。`IpcHost.cpp` 從 IPC 執行緒進 host 並同步碰 `match_` 的呼叫：`host.SubmitInput`（`:645`）、`host.SubmitActionBatch`（`:685`）、`host.GetActionResults`（`:681、727、777、833`，函式 `Receive`／`Pump`／`ScheduleWrite`／`Write`）、`host.NoteWireRejection`（`:652、669`，經 `IngressBucketId`）。另 `MovementTrace.hpp` 的 `TraceMovement` 本身讀 `MovementTraceNowNs()`。 |
| D3 failures | `PlayerState::connectionQualityFailures`（`Movement.hpp:179`）進 digest（`AuthorityDigest.cpp:164`）。全 `src/Pvp` 只有一個寫點：`MatchRuntimeHost::JudgeConnectionQuality`（`:745`），寫在 `Step` 從 `match_.Snapshot()` 複製出來的 `state` 上，每 tick 重寫一次（值來自 host 的 `quality_[player].failures`）；`PvpMatch.cpp` 沒有寫點，`PvpMatch::Snapshot()` 永遠帶 0。既有測試 `PvpMatchTests.cpp:625` 查 tick `kW+10` 的值，沒查判定當 tick（`kW`）本身。 |

golden：`fixtures/authority_golden.txt` 有 **37** 行資料（交接卡寫 34；以檔案為準，本宣告用 37）。digest runner 列 42 個情境，其中 37 個 golden。

## 2. 做什麼

| 代號 | 內容 | 檔 |
|---|---|---|
| D1 | 特徵測試 `TEST_CASE("kernel-prep D1 ...")`：(a) Match 層——兩人對射到致死；致死決定的 `resolvedTick` 記為 T；斷言 `Snapshot()` 於 T 已是 Dead、`lifeStateTick == T`、`respawnTick == R`（R 從實跑讀出，再和 `T + respawnTicks` 對照）；於 R−1 仍 Dead；於 R 是 Alive、`lifeStateTick == R`、`lifeGeneration` 加一。(b) host 層——同樣的對射經 `MatchRuntimeHost`（注入時鐘），用 `TakeSnapshot()` 取 tick T 與 R 的發布快照，斷言同上。數值由實跑印進斷言訊息，不手算。 | 新檔 `tests/object_fps_pvp/KernelPrepDetectTests.cpp`，加進 `gyo_object_fps_pvp_tests` |
| D2 | Python 棘輪守衛 `test_kernel_prep_sites.py`（ctest `object_fps_pvp.kernel_prep_sites`，標籤 `cpu;pvp`）。掃三類：**W** 牆鐘讀（`now_()`、`steady_clock::now()`、`system_clock::now()`、`Clock::now()`、`MovementTraceNowNs()`）在 `PvpMatch.cpp`、`MatchRuntimeHost.cpp`、`LocalPlayerPrediction.cpp`、`Movement.cpp`、`MovementTrace.hpp`、`MovementDiag.hpp`；**G** 全域診斷入口（`TraceMovement(`、`MovementDiagEnabled()`、`RecordMovementDiag(`）在前四個檔；**A** IO 執行緒碰權威——`MatchRuntimeHost.cpp` 內非 tick 側函式的 `match_.<方法>(`（tick 側＝`Step`、`Body`、`TrackSlack`、`JudgeConnectionQuality`、`ClearState`、`RemovePlayerState`，理由寫在檔頭），與 `IpcHost.cpp` 內 `host.<方法>(` 中方法會同步碰 `match_` 的（`SubmitInput`、`SubmitActionBatch`、`GetActionResults`、`NoteWireRejection`）。每個命中歸到「檔＋函式名＋呼叫形狀」，表＝(檔, 函式, 形狀) → 次數；守衛斷言掃出來的表**等於**白名單（多一列、少一列、次數變都失敗）。函式歸屬用大括號深度：最外層、標頭含 `(`、不是 `namespace`／`class`／`struct`／`enum` 的範圍；註解與字串先剝掉。自測：(i) 對一份在乾淨函式裡插入 `now_()` 的合成文字跑掃描，必須報出新列；(ii) 白名單本身就是正例（每類至少一列，和現況相等才過）。 | 新檔 `tests/object_fps_pvp/test_kernel_prep_sites.py`、`CMakeLists.txt` 加 `add_test` |
| D3 | 特徵測試 `TEST_CASE("kernel-prep D3 ...")`：(a) host 層——兩人，**id 較大者**連線品質差（`observedAuthorityTick` 落後 12 tick），逐 tick 取發布快照，斷言 `connectionQualityFailures`：join 後第一個窗不判（到 2W−1 為 0）、tick 2W 起為 1、3W 起為 2、4W 逐出；值在判定當 tick（kW）就出現，之間每 tick 都帶同一值；另一人全程 0。(b) Match 層——`PvpMatch` 單獨跑到 Starvation 重置發生（`quality.resets == 1`），斷言 `Snapshot()` 的 `connectionQualityFailures` 全程為 0（Match 不寫它）。D2 守衛加一條：`src/Pvp` 內 `connectionQualityFailures` 的寫點只有 `MatchRuntimeHost.cpp` 的 `JudgeConnectionQuality` 一處、`PvpMatch.cpp` 零處。 | 同 D1 的新檔；守衛同 D2 |
| 突變 | `mutations.json` 加 6 條（batch `v8-product-kernel-prep-detect`，單一 find／replace，照工具限制）：`kp-death-next-tick`（`Kill` 改成不寫、`ResolveLifeBoundaries` 在下一 tick 以 `hp == 0 && Alive` 補殺——一段連續文字 `PvpMatch.cpp:301-320`）、`kp-respawn-next-tick`（`tick_ < respawnTick` → `<=`）、`kp-failures-other-object`（寫到 `state.players.front()` 而不是被判定的玩家）、`kp-failures-judgement-tick-only`（只在判定當 tick 寫，其餘 tick 0）、`kp-site-new-wall-clock`（在 `MatchRuntimeHost::QueueJoin` 加 `static_cast<void>(now_());`，check ＝新 ctest）、`kp-site-new-authority-touch`（在 `QueueJoin` 加 `static_cast<void>(match_.TickCount());`，check ＝新 ctest）。 | `tests/object_fps_pvp/mutations.json` |

## 3. 不做

- 不選漏斗、不拆欄、不搬任何讀點、不改 `Movement.cpp`、不改 `connectionQualityFailures` 的歸屬。
- 不碰 `apps/object_fps_pvp/src`、`apps/object_fps_pvp/include`、`engine/`、`framework/`、`tests/common/`、`build/`、`.github/`、`fixtures/`、`AuthorityDigest.cpp`。
- 不做第二批 `-zones`。

## 4. 判定（先宣告；任一不成立＝停下報告，不補救）

| 代號 | 判定 | 通過條件 |
|---|---|---|
| J1 D1 過 | 新 D1 測試在 `1e7625d9` 的程式上 | 全部斷言通過；斷言訊息裡的 T、R、R−T 印出並記進 dev_log |
| J2 D1 突變 | `kp-death-next-tick`、`kp-respawn-next-tick` | 工具判 **killed**（expect 是 D1 測試自己的訊息）；另以 `-tc` 只跑 D1 測試，每條突變下至少 1 個斷言失敗（證明是新測試殺的，不是既有測試） |
| J3 D2 守衛過 | 守衛對 `1e7625d9` 的 `src/Pvp` | ctest 通過；白名單 W／G／A 三類各 ≥ 1 列，掃出的表與白名單相等 |
| J4 D2 自測 | 合成新增讀點 | 自測 (i) 通過；`kp-site-new-wall-clock`、`kp-site-new-authority-touch` 工具判 killed |
| J5 D3 過 | 新 D3 測試 | 全部斷言通過 |
| J6 D3 突變 | `kp-failures-other-object`、`kp-failures-judgement-tick-only` | 工具判 killed；`-tc` 只跑 D3 測試各至少 1 個斷言失敗 |
| J7 不變 | `object_fps_pvp.authority_digest` ctest；`git diff --stat 1e7625d9 -- tests/object_fps_pvp/fixtures tests/object_fps_pvp/AuthorityDigest.cpp` | ctest 通過（37 golden 一致）；diff 為空 |
| J8 既有全過 | `ctest -L pvp`（本機）；`object_fps_pvp.mutation_tool`（目錄守衛） | 全部通過 |
| J9 零修改 | `git diff --stat 1e7625d9` | 只有 `tests/object_fps_pvp/`（新檔、`CMakeLists.txt`、`mutations.json`）與 `docs/`；其餘路徑零 |
| J10 L1 | 三平台 | 通過（Python 守衛標 `cpu`，L1 會跑） |

- 件数の突き合わせ（AGENTS §14）：J2／J4／J6 報「突變 6 條：killed n／survived／stale／wrong_failure 各幾條」；J7 報「golden 37／比對 37／差 0」；J3 報「白名單列數＝掃描列數」。不一致就不判定。
- 突變只跑本批 6 條（`--only`；D184 規則），不跑完整目錄。

## 5. 範囲の限界

- 守衛讀原始碼文字，不追間接呼叫：新寫一個包住 `now_()` 的輔助函式，守衛會因「新函式列」失敗，但之後對該輔助函式的呼叫不在表內。**既有**的包裝也一樣（審查 D2 S4）：經 `IngressBucket`／`IngressBucketId`、`RecordRejection`、`GetActionResults`、`TickCount` 碰 `match_`、把 `match_` 當引數傳出（同檔 `:449` 已有寫法）、`const auto& m = match_` 都不會多出列。
- A 類是「模擬 role 以外的執行緒」，不全是 IPC 執行緒：`TakeIngressStatistics`、`TickCount` 是主執行緒（`match_main`）、`QueueActionAcknowledgement` 沒有正式呼叫者；各列有註記（審查 D2 S5）。
- W 類只掃權威側六個檔；IO 層（`ClientConnection.cpp`、`IpcHost.cpp`、`PvpApplication.cpp`）的牆鐘讀是 (b) 區的正常用法，不掃。
- A 類的「tick 側函式」是讀碼判斷（六個），寫在檔頭；判錯會讓一處 IO 讀點漏進 tick 側而不被列——審查者已逐一核對（N3），自測把這六個釘住。
- `Step` 內 `:447` 的 `match_.Snapshot()` 是 tick 側（權威自己的執行緒），不列為違反；`Step` 內的 `now_()`（`:445、483`）仍列 W 類。
- D1 的 host 層只做對射致死，不做復活後再殺；D3 (a) 只做 reference-age 一種失敗原因。
- 突變工具一條只能一處 find／replace，`kp-death-next-tick` 以 `Kill` 與 `ResolveLifeBoundaries` 相鄰的一段連續文字實現；若 `PvpMatch.cpp` 之後改寫使兩者不相鄰，該條會 stale（工具會報）。
- 既有 `CombatTests` 很可能也殺得掉 D1 的突變；J2 的 `-tc` 跑法就是為了證明新測試自己殺得掉。

## 6. 模型與檔位

- 下段（交接卡判定）。
- 實作者：本會話，Fable 5.1（架構師核定：由使用者所開，不切換）。檔位：D1、D3 high；D2 的掃描正規式與函式歸屬 **xhigh**（只這段）；文件與證據 medium。
- 審查者（只審 D2 的掃描與 tick 側六函式的判斷）：`xhigh-reviewer` 子代理，模型 Opus（與主會話不同；該 agent 定義沒有模型，起動時指定），唯讀，資料＝守衛腳本、`MatchRuntimeHost.cpp`、本宣告 §2 D2 列。
- §14.4 檢證者：不需要（不改任何既有判定）。
- 估時：D1 1.5 h、D2 2 h、D3 1 h、文件／證據／PR 1 h，合計約 5.5 h（卡估 4～6 h）。單一項超過 2 h 停下回報。

## 7. 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-kernel-prep-detect/`（git 管理外）：ctest 輸出、每條突變下 `-tc` 過濾的 doctest 輸出、`run_mutations.py --report`、守衛輸出、`authority_digest` 輸出、`git diff --stat`。

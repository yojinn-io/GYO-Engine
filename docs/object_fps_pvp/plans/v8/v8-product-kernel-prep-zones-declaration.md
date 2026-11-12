# v8-product-kernel-prep-zones 事前宣告

更新：2026-11-12。Owner：`object_fps_pvp`（內容歸產品；執行者是架構師開的任務卡）。來源：交接卡 [kernel-prep](kernel-prep.zh-Hant.md) 第二批；J-17 掃描器卡 [j17-scanner](../../../architecture/plans/kernel/j17-scanner.zh-Hant.md)「給 -zones 批」節。出處：audit `docs/architecture/plans/kernel/audit.zh-Hant.md` §1.3、§4.5；`docs/architecture/target/naming.zh-Hant.md` 第 4 條；清單契約 `docs/architecture/target/zone-c-sources.zh-Hant.md`；spec J-17 規則 1。
**狀態：草稿，待架構師核定。** 判定與數字在看到任何結果之前寫下，核准後不放寬（AGENTS §14.1）。核定前不改程式。
分支 `claude/arch-pvp-kernel-prep-zones`，base origin/master `3061e879`（#196 已合併；#193 -detect、#192 J-17 都在內）。

## 0. 為什麼（Feature → Contract → Architecture）

- **Feature**：沒有。功能線 2026-11-11 起停止（停法 (a)）。本批是 kernel 改造的前置：照 naming 第 4 條「決定性區＝位置不是名字」，把 audit 判「留」的 (c) 程式集中到產品的一個目錄與 namespace，呈現集中到另一個，讓 J-17 的掃描有對象、讓遷移批每次只動一個區。
- **Contract**：不改 wire、不改 digest 語義、不改 golden。`gyo.zone-c-sources` 清單格式 v1 不改；清單**檔**的位置從共通層搬到產品（契約 §2：刪 owner＝刪它的清單與 ctest 列）。
- **Architecture**：只動 `apps/object_fps_pvp/`、`tests/object_fps_pvp/`、`build/acceptance/object_fps_pvp/` 與共通層的一處**刪除**（`tests/common/zone_scan/baseline/` 與 `zone_scan.baseline` ctest）。不新增 Subsystem、不改 Dependency Direction（Sim 不依賴 Present；Present、Host 依賴 Sim 的方向與今天相同）。P1～P6、P11 是重寫對象，不搬。

## 1. 現況（偵察結果；行號是 `3061e879` 的）

### 1.1 基線（J-17 掃描器，本機重跑與 #192 報告一致）

| 清單 | 檔 | 展開後 | 行 | 違規 | ZS1 | ZS2 | ZS4 | ZS7 |
|---|---|---|---|---|---|---|---|---|
| 確認（12 檔） | 12 | 40 | 4,949 | **26** | 19 | 3 | 2 | 2 |
| 候選（P9 五檔） | 5 | 20 | 2,353 | **9** | 6 | 0 | 0 | 3 |

確認清單 26 處逐條歸因（檔:行 → 規則 → 誰含進來的 → 類別）：

| 命中 | 規則 | 含入鏈（第一次到達） | 類別 |
|---|---|---|---|
| `Combat.hpp:5` `<chrono>` | ZS1 | `PlayerAnimationDigest.cpp` → `PlayerPresentation.hpp` → | **A 本批改**（型別：`maximumReferenceAge`、`ShotReferenceAge`） |
| `Movement.hpp:8` `<chrono>` | ZS1 | `Arena.cpp` → | **A 本批改**（兩個 `std::chrono::milliseconds` 常數是 Client 連線 liveness 的，不是 (c)；見 §2.4） |
| `Arena.hpp:9`、`MovementRulesFile.hpp:6`、`PlayerAnimationContent.hpp:6` `<filesystem>` | ZS1 | 各自的 .cpp | **B P13 載入器簽名**（`Load(const std::filesystem::path&)`；Io 效果在 `NativeFiles`（audit：包）後面。改掉＝把「讀檔」從「解析」拆出去＝API 變更，不是搬位置） |
| `NativeFiles.hpp:4` `<filesystem>` | ZS1 | `Arena.cpp`、`MovementRulesFile.cpp` → | **B**（audit：包；留在 `Pvp/`） |
| `PlayerPresentation.hpp:9` `<filesystem>` | ZS1 | `PlayerAnimationDigest.cpp` → | **C 呈現標頭被 (c) 含入**（digest 的宣告與 `PlayerPresentationDefinition` 住在 P10 的標頭裡；見 §3 Q4） |
| `ClientSimulation.hpp:7` `<chrono>`；`PredictionElapsedTime.hpp:3` `<chrono>`、`:12` `steady_clock` | ZS1、ZS4 | `ClientSimulation.cpp` → | **D P5 Host 時鐘**（audit：「何時跑下一步」判 Host，重寫對象；不搬、不改） |
| `MovementDiag.hpp:18、19、21、23、26`、`MovementTrace.hpp:4、5、9`、`MovementTraceWriter.hpp:9、10` | ZS1（10 處） | `LocalPlayerPrediction.cpp`、`PvpMatch.cpp` → | **E P8 診斷服務**（spec G.4 第 3 條「全域在呼叫點」；遷移批改句柄。不搬、不改） |
| `MovementDiag.hpp:188`、`MovementTrace.hpp:101、102` inline 全域 | ZS2（3 處） | 同上 | **E** |
| `MovementTrace.hpp:59` `steady_clock` | ZS4 | 同上 | **E** |
| `AnimationSetDefinition.hpp:8、23` `unordered_map` | ZS7（2 處） | `PlayerAnimationContent.cpp` → `PlayerAnimationContent.hpp` → `App/CharacterPresentationDefinition.hpp` → | **F 容器**（換 `std::map` 可能改走訪順序→digest 風險；audit §4.5 已交決定性線核對 digest 編碼後再改。不改） |

**結論**：26 處裡只有 A（2）能靠「搬位置＋整數化時間型別」歸零；B（4）是 P13 載入器的 `path` 參數、C（1）是標頭拆分、D（3）與 E（14）住在不搬的 P5／P8 檔裡、F（2）是容器。卡上「26→0」與卡上「P1～P6 不搬」「只搬位置與整數化」兩個前提**互相衝突**；本宣告 §3 列成決定點，不自行選。

### 1.2 其他事實

- namespace：產品用 `fps::pvp`（80 處）；`src/Collision`、`src/Gameplay`、`include/RetroFPS/{World,Data,Gameplay/Weapon,App}` 用 `fps`（19 處）。卡寫的 `RetroFPS::Sim` 是 include 路徑前綴的拼法，程式裡沒有 `RetroFPS` namespace。`using namespace fps::pvp` 51 檔 108 行；`fps::pvp::` 限定名 29 檔 314 行。
- `include/RetroFPS/Pvp/*.hpp` 中被搬動候選含入的標頭與含入站點數（apps＋tests＋acceptance，不含 docs）：共 **179** 行 `#include` 要改路徑（docs 另 22 行，不改——文件是歷史紀錄）。
- `PlayerPose.cpp`（P9 候選）定義了 `PlayerPresentation.hpp` 裡宣告的 12 個自由函式（`LoadPlayer*Definition` ×3、`PlayerJogWeight`、`PlayerCycleDistance`、`PlayerFrameFromAuthority`、`PlayerBodyPointToWorld`、`AdvancePlayerLocomotion`、`ResolvePlayerActions`、`SamplePlayerBodyPose`、`SamplePlayerPresentationPose` ×2）；`PlayerAnimationDigest.cpp` 定義其中 2 個（`AnimationContentDigest`、`AnimationDigestFromSha256`）。也就是 P10 的標頭裝著 P9／P13 的宣告。`PlayerHitbox.hpp:19` 前向宣告 `struct PlayerPresentationDefinition;`。
- 其餘搬動候選標頭裡宣告的自由函式，沒有一個在不搬的 .cpp 裡定義（走查：`apps/object_fps_pvp` 全部 .cpp；只有 `Reset` 這類成員名誤中）。
- `const char* names[]`（ZS2 嚴格讀法）：先以合成正例驗證正規式（2／2 命中：`const char* names[`、`static const char* bad2[`；反例 `const char* const ok[]` 0 命中），再掃 `apps/object_fps_pvp` 全部 `.hpp`／`.cpp`：**3 處**（`PvpApplication.cpp:427、862`、`SoundCues.cpp:15`），三處都是 `constexpr`（掃描器 ZS2 排除 `constexpr`；元素型別 `const char* const`），且都在 Present／P11，不在 (c) 檔。基線的 ZS2 三處是 P8 的 inline 全域，不是這個樣式。**本批「該當なし」**，不改任何一行。
- `tests/object_fps_pvp/test_kernel_prep_sites.py`（-detect 的 D2 棘輪）以 `W_FILES`／`G_FILES` 的**路徑**讀 `Movement.cpp`、`PvpMatch.cpp`、`LocalPlayerPrediction.cpp`、`MovementTrace.hpp`、`MovementDiag.hpp`；白名單的鍵用 `path.name`，只有檔名。搬 `Movement.cpp` 只改 `W_FILES`／`G_FILES` 的目錄，白名單 47 列零變動。
- `tests/object_fps_pvp/mutations.json` 以 repo 相對路徑指到 `src/Pvp/Arena.cpp`、`PlayerPose.cpp`、`ClientSimulation.cpp`、`LocalPlayerPrediction.cpp`、`PvpMatch.cpp`、`include/RetroFPS/Pvp/Combat.hpp` 等；搬動的檔要改 `file` 欄（找到的文字不變）。
- `tests/object_fps_pvp/CMakeLists.txt:436` 以 `sources.cmake` 的 `PVP_DOMAIN_SOURCES` 重編 -O0 的 digest runner；路徑跟著 `sources.cmake` 改。`build/acceptance/object_fps_pvp/CMakeLists.txt`、`.github/`、`build/cmake/` 沒有寫死 `src/Pvp` 等路徑。
- `tests/common/zone_scan/`：`test_zone_scan.py` 與 `run_mutations.py`（18 條突變，check 全是 selftest）都不讀 `baseline/`；`run_baseline.py` 在 `baseline/` 不存在時報「nothing to scan」並以 0 結束。
- golden：`fixtures/authority_golden.txt` **37** 行；`ctest -L pvp` 在 -detect 時 57 個測試。

## 2. 做什麼

### 2.1 目錄與 namespace（§3 Q1）

```text
apps/object_fps_pvp/include/RetroFPS/Sim/*.hpp      namespace fps::pvp { inline namespace sim { … } }
apps/object_fps_pvp/src/Sim/*.cpp
apps/object_fps_pvp/include/RetroFPS/Present/*.hpp  namespace 本批不改（fps::pvp 或 fps 照原樣）
apps/object_fps_pvp/src/Present/*.cpp
apps/object_fps_pvp/include/RetroFPS/Pvp/、src/Pvp/  留：P1～P6、P8、P11、P12、NativeFiles、wire、host 類
apps/object_fps_pvp/include/RetroFPS/App/            留：AnimationSetDefinition、CharacterPresentationDefinition、src/App/AssetDefinitionHelpers.hpp（§3 Q5）
```

- `inline namespace sim`：`fps::pvp::X` 與 `fps::pvp::sim::X` 同一實體，既有 108 行 `using namespace` 與 314 行限定名**零改動**；新程式與遷移批用 `fps::pvp::sim::` 的拼法，`git grep` 得到。`sim` 用小寫，照產品既有的 `fps::pvp::wire`。
- 三個 `namespace fps` 的 Sim 檔（`CharacterCollision`、`PlanarMovement`、`GroundPoint`）改進 `fps::pvp::sim`；受影響的限定名：`MathCharacterizationTests.cpp:345` 的 `fps::ComputePlanarInput`（1 行）、`GridMap.hpp` 的 `GroundPoint`（未限定，`fps` 內查得到 → 改為 `pvp::sim::GroundPoint` 或加 using，3 行）。
- Present 的 namespace 不動：`WeaponViewModel` 等在 `fps`，`PvpApplication.cpp` 有 47 行 `fps::Weapon*` 限定名（P11 重寫對象），改名會動到它；naming 第 5 條的改名（`WeaponViewModel` → `Present::WeaponPose`）本來就是遷移批的事。

### 2.2 搬動清單（逐檔）

**Sim**（30 檔搬動＋1 新標頭）：

| 從 `apps/object_fps_pvp/` | 到 `apps/object_fps_pvp/` | audit |
|---|---|---|
| `include/RetroFPS/Collision/CharacterCollision.hpp`、`src/Collision/CharacterCollision.cpp` | `include/RetroFPS/Sim/`、`src/Sim/` | §1.3 `src/Collision` 留 |
| `include/RetroFPS/Gameplay/Player/PlanarMovement.hpp`、`src/Gameplay/Player/PlanarMovement.cpp` | 同上 | §1.3 `src/Gameplay` 留 |
| `include/RetroFPS/World/GroundPoint.hpp` | `include/RetroFPS/Sim/` | `PlanarMovement.hpp` 的依賴（9 行的 struct） |
| `include/RetroFPS/Pvp/Movement.hpp`、`src/Pvp/Movement.cpp` | 同上 | P7 留 |
| `include/RetroFPS/Pvp/Combat.hpp` | `include/RetroFPS/Sim/` | spec J-17 點名的 (c) 標頭（header-only；P1 的規則資料） |
| `include/RetroFPS/Pvp/EntityId.hpp`、`EntityColumns.hpp` | `include/RetroFPS/Sim/` | (c) 的欄（naming 第 4 條「System 與欄」）；只被 `Movement.hpp`、`PvpMatch.hpp`、1 個測試含入 |
| `include/RetroFPS/Pvp/PlayerAnimation.hpp`、`src/Pvp/PlayerAnimation.cpp` | 同上 | P9 留（推進進 digest） |
| `include/RetroFPS/Pvp/{Arena,ArenaMesh,MovementRulesFile,PlayerAnimationContent}.hpp`、`src/Pvp/{Arena,ArenaMesh,MovementRulesFile,PlayerAnimationContent,PlayerAnimationDigest}.cpp` | 同上 | P13 留（`NativeFiles` 包，不搬） |
| `include/RetroFPS/Pvp/{ShotQuery,ShotGate,PlayerHitModel,PlayerHitbox}.hpp`、`src/Pvp/{ShotQuery,ShotGate,PlayerHitModel,PlayerHitbox,PlayerPose}.cpp` | 同上 | P9 候選（DET-1：E1 前明示 (b)；搬進 Sim 但命中不要求歸零） |
| （新）`include/RetroFPS/Sim/PlayerPose.hpp` | — | 從 `Pvp/PlayerPresentation.hpp` 拆出 `PlayerPose.cpp`／`PlayerAnimationDigest.cpp` 所定義的宣告（§3 Q4） |

**Present**（17 檔）：

| 從 | 到 | audit |
|---|---|---|
| `include/RetroFPS/Pvp/{PlayerPresentation,HitFeedback,PvpSound,SoundCues,PointerCapture,SnapshotTimeline}.hpp`、`src/Pvp/{PlayerPresentation,HitFeedback,PvpSound,SoundCues,PointerCapture}.cpp` | `include/RetroFPS/Present/`、`src/Present/` | P10 呈現（包） |
| `include/RetroFPS/App/{WeaponViewModel,WeaponPresentationDefinition,WeaponEventSounds}.hpp`、`src/App/{WeaponViewModel,WeaponPresentationDefinition,WeaponEventSounds}.cpp` | 同上 | `src/App` 包（§3 Q5：只搬武器呈現三組） |

**不搬**（理由）：P1 `PvpMatch`、P2 `MatchRuntimeHost`、P3 `IpcHost`、P4 `ClientConnection`、P5 `ClientSimulation*`、P6 `LocalPlayerPrediction`、P11 `PvpApplication`（重寫對象）；P8 `MovementDiag`／`MovementTrace`／`MovementTraceWriter`、P12 `ClientLog`／`LogFile`／`NetworkStatistics`／`ClogLine`／`SessionSummary`／`TracePaths`（服務；卡說位置由產品線提、架構師核——本批不提，遷移批改句柄時一起定）；`NativeFiles`（包）；`App/AnimationSetDefinition`、`App/CharacterPresentationDefinition`、`src/App/AssetDefinitionHelpers.hpp`（§3 Q5）；`World/GridMap.hpp`、`Data/GameData.hpp`、`Gameplay/Weapon/*.hpp`（舊 RetroFPS 標頭，只被 App 武器呈現用；audit 沒判，不動）；其餘 `Pvp/*.hpp`（wire、ingress、liveness、host 的介面）。
搬完 `src/Collision`、`src/Gameplay`、`include/RetroFPS/Collision`、`include/RetroFPS/Gameplay/Player` 變空 → 刪目錄（AGENTS §7 不留空目錄）。

### 2.3 隨搬動必須改的地方（只改路徑／結構，不改內容）

- 所有 `#include "RetroFPS/…"` 站點（約 179 行，apps／tests／acceptance）。
- `apps/object_fps_pvp/sources.cmake`（`PVP_DOMAIN_SOURCES`）、`CMakeLists.txt`（`player_content`、`app_support` 的來源列）。
- `tests/object_fps_pvp/test_kernel_prep_sites.py`：`W_FILES`／`G_FILES` 的目錄；白名單不動。
- `tests/object_fps_pvp/mutations.json`：搬動檔的 `file` 欄。
- `tests/object_fps_pvp/test_diagnostic_isolation.py`、`test_protocol_version.py` 等讀固定路徑的守衛：核對後只改路徑（目前看到的都指向不搬的檔）。

### 2.4 時間型別（spec J-17 規則 1；本批唯一的程式邏輯層改動，是型別不是邏輯）

| 位置 | 今天 | 改成 | 數值不變的理由 |
|---|---|---|---|
| `Combat.hpp:26` `CombatRules::maximumReferenceAge` | `std::chrono::nanoseconds{250ms}` | `std::int64_t maximumReferenceAgeNs{250'000'000}` | `nanoseconds` 是 `duration<int64, nano>`；比較在 `.count()` 上逐位相同 |
| `Combat.hpp:228` `ShotReferenceAge` | `std::function<std::optional<std::chrono::nanoseconds>(uint64_t)>` | `std::function<std::optional<std::int64_t>(uint64_t)>`（ns） | 同上 |
| `PvpMatch.hpp:137` `referenceAge`、`PvpMatch.cpp:52、375、377` | `optional<nanoseconds>`、`< nanoseconds::zero()` | `optional<int64_t>`、`< 0`、`> maximumReferenceAgeNs` | 同上（P1 檔只動這四行的型別拼法） |
| `MatchRuntimeHost.cpp:441-445` 的 lambda | `duration_cast<nanoseconds>(now_() - publishedAt)` | 同一式加 `.count()` | `duration_cast` 的結果再取 `.count()`，值相同 |
| `IpcHost.cpp:521`、`ClientConnection.cpp:100、123`、`worker_main.cpp:719` | `duration_cast<ms>(…).count()`、`milliseconds{ms}`、`==333ms` | `/ 1'000'000`、`ms * 1'000'000`、`== 333'000'000` | wire 的 `maximum_reference_age_ms` 不變 |
| 測試：`CombatTests.cpp:37、1129、1432、1444`、`ShotGateTests.cpp:116`、`EntityModelTests.cpp:134、211`、`KernelPrepDetectTests.cpp:54`、`AuthorityDigest.cpp:396-397` | lambda 回 `nanoseconds` | 回 `int64_t`（同一算式 `.count()`） | 同上；`AuthorityDigest.cpp` 只動 lambda 的回傳型別，digest 編碼不碰 |
| `Movement.hpp:96-97` `ConnectionInterruptedAfter`、`ConnectionTimeoutAfter` | `constexpr std::chrono::milliseconds` | **搬到 `Pvp/ConnectionLiveness.hpp`**（唯一用它們的 (b) 標頭），型別不變 | 不是 (c) 的時間；留在 Sim 要改成整數會動到 `ConnectionLiveness.hpp` 的 time_point 算式（P4）。`worker_main.cpp` 加一行 include；`test_diagnostic_isolation.py:55` 斷言這兩個名字不出現在 P5／P6 檔，不受影響 |

改完 `Combat.hpp`、`Movement.hpp` 不再 include `<chrono>`；`PvpMatch.cpp`／`.hpp` 若因此沒有其他 chrono 用法，一併移除 include（只刪 include 行）。

### 2.5 基線清單歸產品（AGENTS §7；J-17 卡「給 -zones 批」）

- 三份清單（§3 Q2）放 `tests/object_fps_pvp/`：`zone-c-sources.json`（確認；門）、`zone-c-candidates.json`（P9；報告）、`zone-c-rewrite.json`（P1 `PvpMatch.cpp`、P5 `ClientSimulation.cpp`、P6 `LocalPlayerPrediction.cpp`；報告）。格式 v1 不變；`repo_commit` 寫產生時的 commit；`files` 用新路徑。
- `tests/object_fps_pvp/CMakeLists.txt` 加三個 ctest（標籤 `cpu;pvp`）：`object_fps_pvp.zone_c_sources`（`zone_scan.py --list … --report …`，有違規＝失敗）、`object_fps_pvp.zone_c_candidates`、`object_fps_pvp.zone_c_rewrite`（`--no-fail`，只報告）。掃描器仍是 `tests/common/zone_scan/zone_scan.py`（共通工具，產品只持清單）。
- 新 Python 守衛 `tests/object_fps_pvp/test_zone_c_lists.py`（ctest `object_fps_pvp.zone_c_lists`）：三份清單互不重疊；`sources ∪ candidates` **等於** `src/Sim/*.cpp` 的集合（位置＝區：Sim 裡每個 .cpp 都在 (c) 清單上，清單上的 Sim 檔都在 Sim 裡）；`rewrite` 的檔都在 `src/Pvp/`。這是判定 J5 第三條突變的紅燈來源（掃描器只查「列的檔存在」，不查「該列的沒列」）。
- 共通層只刪：`tests/common/zone_scan/baseline/`（兩個 json）與 `tests/common/zone_scan/CMakeLists.txt` 的 `zone_scan.baseline` add_test 及其 `set_tests_properties` 名單。`run_baseline.py`、`zone_scan.py`、規則、自測、突變不動。

### 2.6 文件

dev_log `docs/dev_logs/2026_11_12_pvp_kernel_prep_zones.zh-Hant.md`（搬動對照表、殘留逐條、件数の突き合わせ）；kernel-prep 交接卡「現況」加一行；pvp HANDOFF 開始與結束各一行；J-17 卡「給 -zones 批」節現況加一行（清單已搬）；plans/v8 README 一覽加 dev_log。

## 3. 決定點（請架構師選；每點附機制、取捨、證據）

**Q1 namespace 的做法**
- (a) **推薦**：`inline namespace sim`（§2.1）。零 reference 改動、可逆（刪兩行就回去）、`fps::pvp::sim::` 可 grep。代價：編譯器不強迫使用者寫出 `sim`，區只靠目錄與清單守衛。
- (b) 非 inline 的 `fps::pvp::sim`：所有使用點改寫（51 檔 using、314 行限定名的一部分；估 +4～6 h）；Host／測試對 (c) 型別的每個引用都看得見 `sim`。代價：P1～P6、P11 的檔要大量改行（違背「重寫對象不動」的精神）。
- (c) 只搬目錄、不加 namespace。最小；naming 第 4 條只滿足「目錄」半邊。

**Q2 清單再切（26→0 的衝突）**
- (a) **推薦**：確認清單＝Sim 裡「留」的 9 個 .cpp（`CharacterCollision`、`PlanarMovement`、`Movement`、`PlayerAnimation`、`Arena`、`ArenaMesh`、`MovementRulesFile`、`PlayerAnimationContent`、`PlayerAnimationDigest`）；P1／P5／P6 的三個 .cpp 移到第三份「rewrite」清單（報告，不是門；保留它們的命中讓遷移批看得到 §1.1 的 D、E 類）。
- (b) 維持 12 檔確認清單：門不可能歸零（D＋E 共 17 處住在不搬的檔）；只能宣告「26→殘留 N」。
- (c) 把 P1／P5／P6 從清單刪掉：失去 17 處的可見度。

**Q3 確認清單的門（配合 Q2(a)；數字是預測，看結果前寫）**
- (a) **推薦**：ZS2＝0、ZS4＝0、ZS6＝0、ZS3＝0；ZS1 **只剩** 5 行且逐行指定：`Sim/Arena.hpp` `<filesystem>`、`Sim/MovementRulesFile.hpp` `<filesystem>`、`Sim/PlayerAnimationContent.hpp` `<filesystem>`、`Pvp/NativeFiles.hpp:4` `<filesystem>`、`Sim/PlayerPose.hpp` `<filesystem>`（`LoadPlayerPoseDefinition(const std::filesystem::path&)` 的宣告）；ZS7 只剩 `App/AnimationSetDefinition.hpp:8、23`。合計 **26 → 7**，多一處、換一處都算不成立。殘留的理由：B（P13 載入器以 `path` 為參數；Io 在 `NativeFiles`）、F（容器換法待決定性線）。
- (b) 本批就拆載入器：`Arena::Load(path)`／`LoadInstalledArenas(path)`／`LoadStanceRules(path)`／`WithContentAssets(path)`／`LoadPlayerAnimationRules(path)`／`LoadPlayerPoseDefinition(path)`／`LoadPlayerHitbox(path)` 改成「Pvp/ 的 (b) 讀檔入口 → Sim 的 `Parse*(string_view)`」。能讓 ZS1 歸零（ZS7 仍剩 2），但是 API 變更：呼叫點（`match_main.cpp`、`PvpApplication.cpp`、約 10 個測試）都要改，新增 7 個函式與 2～3 個新檔；估 +4～6 h，而且這正是遷移批把它們做成 Startup System 時會再改一次的地方。不推薦。

**Q4 `PlayerPresentation.hpp` 的拆分**
- (a) **推薦**：拆出 `Sim/PlayerPose.hpp`：`PlayerPresentationFrame`、`PlayerPresentationDefinition`、`LoadPlayer*Definition` ×3、`PlayerLocomotionState`、`PlayerJogWeight`、`PlayerCycleDistance`、`PlayerFrameFromAuthority`、`PlayerBodyPointToWorld`、`AdvancePlayerLocomotion`、`PlayerActionPose`、`ResolvePlayerActions`、`PlayerPresentationPose`、`SamplePlayerBodyPose`、`SamplePlayerPresentationPose` ×2、`AnimationContentDigest`、`AnimationDigestFromSha256`（即 `PlayerPose.cpp` 與 `PlayerAnimationDigest.cpp` 所定義的全部宣告，連同各自的註解原文搬過去，不改一字）；`Present/PlayerPresentation.hpp` 留 `PlayerPresentationObservation`、`class PlayerPresentation`，並 include `Sim/PlayerPose.hpp`。既有 15 個含入 `PlayerPresentation.hpp` 的站點只改路徑；Sim 的三個 .cpp 改含 `Sim/PlayerPose.hpp`。理由：Q1(a) 下 `PlayerPose.cpp` 進 `sim` 後，它定義的函式必須在 `sim` 裡宣告，否則是另一個函式（連結失敗）；同時消掉 Sim → Present 的含入（C 類）。
- (b) 不拆：`PlayerPose.cpp`、`PlayerHitModel.cpp`、`PlayerAnimationDigest.cpp` 繼續含 Present 的標頭，且不能進 `sim` namespace（Q1 退成 (c)）。

**Q5 `src/App` 的歸屬**
- (a) **推薦**：只把武器呈現三組（`WeaponViewModel`、`WeaponPresentationDefinition`、`WeaponEventSounds`）搬到 Present；`AnimationSetDefinition`、`CharacterPresentationDefinition`、`AssetDefinitionHelpers.hpp` 留在 `App/`。理由：後三者是 Match 也用的內容定義（`player_content` 函式庫；`PlayerAnimationContent.hpp`、`PlayerHitbox.hpp` 含入它們），audit 把整個 `src/App` 判「包」是因為 `WeaponViewModel` 跨幀存裝置指標，`*Definition.cpp` 註明「是 JSON 載入函式」。搬進 Present 會造成 Sim → Present 的含入；搬進 Sim 又帶 ZS7（F）進門。留在 `App/`，在 dev_log 記「Sim → App 的含入 3 處（`PlayerAnimationContent.hpp:3`、`PlayerHitbox.hpp`、`PlayerAnimationContent.cpp` 的 `AssetDefinitionHelpers.hpp`）待區判定」。
- (b) 整個 `src/App` → Present（卡的字面）：Sim 含入 Present 3 處，方向反。
- (c) `src/App` 整個不動。

**Q6 時間常數 `ConnectionInterruptedAfter`／`ConnectionTimeoutAfter`**
- (a) **推薦**：搬到 `Pvp/ConnectionLiveness.hpp`，型別不變（§2.4 末列）。
- (b) 留在 `Sim/Movement.hpp`，改成 `std::int64_t …Ms`，`ConnectionLiveness.hpp`（P4）與 `worker_main.cpp`、`ConnectionLivenessTests.cpp` 的 time_point 算式包回 `milliseconds{}`。

## 4. 判定（先宣告；數字在看到結果前補齊；任一不成立＝停下報告，不補救）

| 代號 | 判定 | 通過條件 |
|---|---|---|
| J1 掃描 | 三個新 ctest＋本機直接跑 `zone_scan.py` | 確認清單：違規數＝**7**，且逐行等於 Q3(a) 的七行（檔:行:規則 比對）；ZS2／ZS3／ZS4／ZS5／ZS6＝0。候選清單：只報告，命中逐條寫理由（預期 `Arena.hpp`、`PlayerAnimationContent.hpp`、`PlayerHitbox.hpp:9` 的 `<filesystem>`、`PlayerPose.cpp:17` `unordered_set`、`AnimationSetDefinition.hpp` ×2；數字看結果後記，不是門）。rewrite 清單：只報告。 |
| J2 不變 | `object_fps_pvp.authority_digest`；`git diff --stat 3061e879 -- tests/object_fps_pvp/fixtures`；`compare_authority_trees.py`（同主機兩樹：base `3061e879` vs HEAD） | ctest 通過；fixtures diff 為空（golden **37** 行零差）；兩樹比對 **0** 差分（含 `ArenaDigest`、`AuthorityDigest` 全部情境） |
| J3 既有全過 | `ctest -L pvp`（本機） | 全部通過；數量＝57 ＋ 4（三個 zone ctest＋lists 守衛）＝ **61**；`tests/common` 的 `zone_scan.selftest` 通過、`zone_scan.baseline` 不再存在 |
| J4 D2 棘輪 | `object_fps_pvp.kernel_prep_sites` | 通過；白名單 47 列、55 命中**零變動**（`git diff` 只有 `W_FILES`／`G_FILES` 兩行的目錄） |
| J5 突變 | 至少 3 條（`mutations.json`，batch `v8-product-kernel-prep-zones`） | (i) `kz-sim-file-moved-out`：把一個 Sim 的 .cpp（例 `Movement.cpp`）移回 `src/Pvp/` 並改 `sources.cmake`——`object_fps_pvp.zone_c_sources` 以契約錯誤（檔不存在，exit 2）失敗、`zone_c_lists` 失敗；(ii) `kz-ns-off-by-one-digit`：`maximumReferenceAgeNs` 的 `250'000'000` 改成 `25'000'000`（或 `IpcHost.cpp:521` 的 `1'000'000` 改 `100'000`）——`authority_digest`／`CombatTests` 或 `IpcHostTests` 失敗；(iii) `kz-list-drops-a-file`：確認清單少列 `Movement.cpp`——`zone_c_lists` 失敗。三條 killed；expect 是各自測試的名稱。突變工具一條只能一處 find／replace，(i) 若無法以單一替換表達（搬檔不是文字替換），改成手動執行並把輸出存進證據，宣告此條「手動突變」 |
| J6 共通層 | `git diff --stat 3061e879` | `engine/`、`framework/`、`build/`、`.github/`、`tools/`、`tests/common/` 的變更**只有**刪 `tests/common/zone_scan/baseline/*.json`（2 檔）與 `tests/common/zone_scan/CMakeLists.txt` 的刪行；`apps/` 只有 `apps/object_fps_pvp/`；其餘路徑零 |
| J7 搬動數 | `git diff --stat -M` 與 §2.2 | rename 偵測到的檔數＝**47**（Sim 30＋Present 17）＋新檔 `Sim/PlayerPose.hpp`、三份清單、`test_zone_c_lists.py`；空目錄 4 個不存在 |
| J8 審查 | 與實作不同模型的審查者（high，唯讀） | 重點兩項：搬錯區（對照 audit §1.3）、型別轉換改了數值（對照 §2.4 每列）。must 0 才進 J9 |
| J9 L1 | 三平台 | linux-x64、windows-x64、macos-arm64 的 ctest 都含 `object_fps_pvp.zone_c_sources`、`zone_c_lists`、`kernel_prep_sites`、`authority_digest` passed |

件数の突き合わせ（AGENTS §14，報告開頭）：搬動檔數（宣告 47／實際 git rename 數）；基線命中（前 26＋9／後 7＋候選實際數＋rewrite 實際數，且 26 處的歸因 A～F 每處在「後」的表裡找得到去向：A 消失、B／F 仍在、C 轉為 `Sim/PlayerPose.hpp` 的一行、D／E 轉到 rewrite 清單）；golden 行數 37／37；ctest 數 57→61；D2 白名單 47／47。

## 5. 範囲の限界

- 掃描器的已知限界（J-17 dev_log）照舊：識別字引數的括號初始化、多重宣告子、大小寫不敏感檔案系統的 include 拼法；碰到以人工補看，不改規則。
- `inline namespace` 不阻止 Host 程式以 `fps::pvp::X` 引用 (c) 型別；區的強制仍是目錄＋清單守衛＋spec G.1 的啟動檢查（未實作）。
- `zone_c_lists` 只比對 `src/Sim/*.cpp` 與清單；標頭（`include/RetroFPS/Sim/*.hpp`）由掃描器沿 include 展開覆蓋，一個沒被任何 Sim .cpp 含入的孤兒標頭不會被掃到。本批搬進去的 16 個標頭都有含入鏈（§1.1 展開表），不會有孤兒；之後新增要靠審查。
- 殘留的 5 處 ZS1（B）與 2 處 ZS7（F）在本批**不修**；遷移批把載入器做成 Startup System 時處理 B，決定性線核對 digest 編碼後處理 F。
- rewrite 清單的命中（D、E 類）本批不處理：G.4 第 3 條（診斷句柄）與 P5 host 時鐘是遷移批的設計決定。
- 文件裡的舊路徑（`src/Pvp/Movement.cpp` 等 22 行）不改，dev_log 給對照表。
- Present 的 namespace 本批不動（§2.1）；naming 第 5 條的後綴（`ViewModel`、`Renderer`）照 DI 3 候選的白名單處理，隨遷移縮小。
- P8／P12 診斷服務的位置本批不提案（卡：由產品線提、架構師核），等遷移批改句柄時一起定。
- 搬檔用 `git mv`，`git log --follow` 可追；但 GitHub 的 blame 在 rename 加內容改動（namespace 兩行＋include 路徑）下仍會顯示為 rename（相似度 >90%）。
- 兩樹比對（J2）在同一台 Intel Mac 上做，與 -O0 digest runner 一起構成「權威不變」的本機證據；跨平台由 L1 的 `authority_digest` 補。

## 6. 模型與檔位

- 下段（交接卡判定；不碰 Design Invariants 條文、wire、kernel 本體）。
- 實作者：本會話，Fable 5.1（架構師開卡時所開，不切換）。檔位：搬檔、include、CMake、清單 **high**；§2.4 時間型別轉換與 Q4 的標頭拆分 **xhigh**（只這兩段）；文件、證據 medium；逐檔 `git mv` 與路徑替換用腳本（存進證據目錄，可重跑）low。
- 審查者：`xhigh-reviewer` 子代理，模型 **Opus**（與主會話不同；該 agent 定義沒有模型，啟動時指定），檔位 **high**（卡的指定；不是 xhigh），唯讀，資料＝本宣告 §2.2／§2.4、audit §1.3 表、`git diff -M` 的檔案對照、`Combat.hpp`／`PvpMatch.cpp`／`MatchRuntimeHost.cpp`／`IpcHost.cpp`／`ClientConnection.cpp` 的改動片段。
- §14.4 檢證者：不需要（不改任何既有判定；D2 白名單零變動）。
- 估時：偵察與宣告 2.5 h（已用）；搬檔與 include 3 h；namespace 與 Q4 拆分 2 h；時間型別 1.5 h；清單、CMake、lists 守衛 1.5 h；建置與測試迴圈 2 h；突變 1 h；審查與修正 1 h；文件、證據、PR 1.5 h；合計約 **16 h**（卡估 8～16 h）。單一檔搬動磨超過 2 h、golden 變、共通層要改（刪除以外）→ 停下回報。

## 7. 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-kernel-prep-zones/`（git 管理外，Intel Mac）：`before-sources.json`／`before-candidates.json`（搬前掃描報告）、搬後三份報告、`move_files.py`（搬檔與路徑替換腳本）、`git diff --stat -M`、`ctest -L pvp` 輸出、`authority_digest` 輸出、兩樹比對輸出、突變輸出、審查全文。

# PvP v8：v8-product-kernel-prep-zones（kernel 改造前置第二批：位置＝區）

日期：2026-11-12。Owner：`object_fps_pvp`（執行者：架構師會話開的任務卡）。計畫：交接卡 [kernel-prep](../object_fps_pvp/plans/v8/kernel-prep.zh-Hant.md) 第二批；[事前宣告](../object_fps_pvp/plans/v8/v8-product-kernel-prep-zones-declaration.md)架構師 2026-11-12 核定（六個決定點全採 (a)）。base origin/master `3061e879`。
- 執行：實作者＝本會話（Fable 5.1；搬檔／include／CMake／清單 high、時間型別與標頭拆分 xhigh、文件 medium）；審查者＝`xhigh-reviewer` 子代理（Opus／high，唯讀），重點「搬錯區」與「型別轉換改了數值」。
- 性質：搬位置（Sim 30 檔＋Present 17 檔）、一個標頭拆分、時間型別整數化（兩處）、兩個常數搬家、基線清單歸產品。不改玩法、wire、digest 語義；P1～P6、P11 不搬。

件数の突き合わせ：**一致**——搬動檔數 宣告 47／git rename 47（git 把舊 `PlayerPresentation.hpp` 配對到 `Sim/PlayerPose.hpp`（R080），`Present/PlayerPresentation.hpp` 顯示為新增；實際是 46 個純搬＋1 個搬並拆，數量相同）；基線命中 前 26（確認）＋9（候選）／後 7（確認）＋7（候選）＋18（rewrite），26 處的去向逐一對上（下方「殘留」節）；golden 37／37；`ctest -L pvp` 57→61；D2 白名單 47／47 列、55 命中；突變 文字 4 條 → killed 4／survived 0／stale 0／wrong_failure 0（第一輪 2 條因 expect 未跳脫正規式判 wrong_failure，修 expect 後重跑 killed；預期的斷言兩輪都有失敗）＋手動 1 條 killed。

## 起因

naming 第 4 條「決定性區＝位置不是名字」：(c) 的 System 與欄集中到產品的一個目錄與 namespace，讓 J-17 掃描有對象、讓遷移批每次只動一個區。J-17 基線（#192）在 12 檔的確認清單上有 26 處命中；本批前的偵察把 26 處逐條歸因後發現，卡上「26→0」與「P1～P6 不搬、只搬位置」互相衝突（宣告 §1.1），架構師核定為宣告錯誤的修正：門改為 26 → 7，七行逐一指定。

## 做了什麼

### 搬動（`git mv`；腳本 `move_files.py` 在證據目錄，47 檔、90 個檔案的路徑改寫、152 行 `#include`）

| 區 | 從 `apps/object_fps_pvp/` | 到 |
|---|---|---|
| Sim（30） | `src/Collision/CharacterCollision.cpp`、`src/Gameplay/Player/PlanarMovement.cpp`、`src/Pvp/{Movement,Arena,ArenaMesh,MovementRulesFile,PlayerAnimation,PlayerAnimationContent,PlayerAnimationDigest,ShotQuery,ShotGate,PlayerHitModel,PlayerHitbox,PlayerPose}.cpp` | `src/Sim/` |
| | `include/RetroFPS/Collision/CharacterCollision.hpp`、`include/RetroFPS/Gameplay/Player/PlanarMovement.hpp`、`include/RetroFPS/World/GroundPoint.hpp`、`include/RetroFPS/Pvp/{Movement,Combat,Arena,ArenaMesh,MovementRulesFile,PlayerAnimation,PlayerAnimationContent,EntityId,EntityColumns,ShotQuery,ShotGate,PlayerHitModel,PlayerHitbox}.hpp` | `include/RetroFPS/Sim/` |
| Sim（新） | 從 `PlayerPresentation.hpp` 拆出 pose 半邊（`PlayerPresentationFrame`、`PlayerPresentationDefinition`、`LoadPlayer*Definition` ×3、`PlayerLocomotionState`、`PlayerJogWeight`、`PlayerCycleDistance`、`PlayerFrameFromAuthority`、`PlayerBodyPointToWorld`、`AdvancePlayerLocomotion`、`PlayerActionPose`、`ResolvePlayerActions`、`PlayerPresentationPose`、`SamplePlayerBodyPose`、`SamplePlayerPresentationPose` ×2、`AnimationContentDigest`、`AnimationDigestFromSha256`；原文照搬） | `include/RetroFPS/Sim/PlayerPose.hpp` |
| Present（17） | `src/Pvp/{PlayerPresentation,HitFeedback,PvpSound,SoundCues,PointerCapture}.cpp`、`src/App/{WeaponViewModel,WeaponPresentationDefinition,WeaponEventSounds}.cpp` | `src/Present/` |
| | `include/RetroFPS/Pvp/{PlayerPresentation,HitFeedback,PvpSound,SoundCues,PointerCapture,SnapshotTimeline}.hpp`、`include/RetroFPS/App/{WeaponViewModel,WeaponPresentationDefinition,WeaponEventSounds}.hpp` | `include/RetroFPS/Present/` |

- namespace：Sim 的 30 檔（31 個 namespace 區塊）包進 `namespace fps::pvp { inline namespace sim { … } }`；`CharacterCollision`、`PlanarMovement`、`GroundPoint` 從 `fps` 改進同一處（`GridMap.hpp` 3 行、`MathCharacterizationTests.cpp` 1 行改限定名）。Present 的 namespace 不動（v1；宣告 Q1(a)）。
- 留在原處（宣告 §2.2）：P1～P6、P11 的 host 與重寫對象；P8／P12 診斷；`NativeFiles`（包）；`App/AnimationSetDefinition`、`App/CharacterPresentationDefinition`、`src/App/AssetDefinitionHelpers.hpp`（Match 也用的內容定義；搬 Present 會讓 Sim→Present，搬 Sim 會把 ZS7 帶進門——待區判定）；`World/GridMap.hpp`、`Data/GameData.hpp`、`Gameplay/Weapon/*.hpp`（舊 RetroFPS 標頭，只被 App 武器呈現用；audit 沒判）。
- 刪掉的空目錄：`src/Collision`、`src/Gameplay`、`include/RetroFPS/Collision`、`include/RetroFPS/Gameplay/Player`。

### 時間型別（spec J-17 規則 1；型別不是邏輯）

| 位置 | 改動 |
|---|---|
| `Sim/Combat.hpp` | `CombatRules::maximumReferenceAge`（`std::chrono::nanoseconds{250ms}`）→ `maximumReferenceAgeNs`（`std::int64_t`，`250'000'000`）；`ShotReferenceAge` 回 `std::optional<std::int64_t>`（ns）；不再 include `<chrono>` |
| `Pvp/PvpMatch.hpp`／`.cpp` | `referenceAge` 的型別；`< nanoseconds::zero()` → `< 0`；`> maximumReferenceAge` → `> maximumReferenceAgeNs`（只這四行） |
| `Pvp/MatchRuntimeHost.cpp` | lambda 的回傳型別；`duration_cast<ns>(…)` 加 `.count()` |
| `Pvp/IpcHost.cpp`、`Pvp/ClientConnection.cpp`、`build/acceptance/.../worker_main.cpp` | wire 的 `maximum_reference_age_ms`：`/ 1'000'000`、`ms * 1'000'000`、`== 333'000'000`；wire 的值不變 |
| 測試 | `CombatTests`、`ShotGateTests`、`EntityModelTests`、`KernelPrepDetectTests`、`AuthorityDigest.cpp` 的 lambda 回傳型別（`AuthorityDigest.cpp` 只動 lambda，digest 編碼不碰） |
| `Sim/Movement.hpp` → `Pvp/ConnectionLiveness.hpp` | `ConnectionInterruptedAfter`、`ConnectionTimeoutAfter`（`std::chrono::milliseconds`）連同註解搬到唯一用它們的 (b) 標頭，型別不變；`Movement.hpp` 不再 include `<chrono>`；`ConnectionLiveness.hpp` 不再 include `Movement.hpp`；`worker_main.cpp` 加一行 include |

數值不變的理由：`std::chrono::nanoseconds` 是 `duration<int64_t, nano>`，所有比較原本就在 `.count()` 的整數上；`duration_cast` 的結果取 `.count()` 值相同。驗證在判定 J2（golden、digest、兩樹比對）。

### 基線清單歸產品（AGENTS §7）

- `tests/object_fps_pvp/zone-c-sources.json`（確認，9 檔：Sim 裡「留」的 .cpp）、`zone-c-candidates.json`（P9，5 檔）、`zone-c-rewrite.json`（P1 `PvpMatch.cpp`、P5 `ClientSimulation.cpp`、P6 `LocalPlayerPrediction.cpp`；報告）。格式 v1；`repo_commit` 寫 base `3061e879`。
- ctest：`object_fps_pvp.zone_c_scan_{sources,candidates,rewrite}`（共通 `zone_scan.py --no-fail`，報告寫到建置目錄 `zone_scan/`；只在契約錯誤時失敗）＋ `object_fps_pvp.zone_c_lists`（`test_zone_c_lists.py`：三份互不重疊、`sources ∪ candidates` ＝ `src/Sim/*.cpp`、rewrite 都在 `src/Pvp/`、確認清單的掃描**正好等於**記錄的 7 處殘留且 ZS2～ZS6 為 0）。
- 共通層：刪 `tests/common/zone_scan/baseline/`（兩個 json）與 `zone_scan.baseline` ctest；`run_baseline.py`、掃描器、規則、自測、突變不動（自測 65 條綠；`run_baseline.py` 在無清單時報 nothing to scan、exit 0）。
- 其他路徑同步：`test_kernel_prep_sites.py`（`W_FILES`／`G_FILES` 的 `Movement.cpp` 目錄；白名單 47 列零變動）、`test_diagnostic_isolation.py`（`ShotGate.cpp`、`SnapshotTimeline.hpp` 的目錄；`ClockSync` 使用者的掃描改成遞迴涵蓋 Sim／Present／App，不縮小）、`mutations.json`（191 個 `file` 欄）、`test_max_players.py`（`Movement.hpp` 路徑）。

### ZS2 `const char* names[]`（卡範圍 3）

正例驗證（2／2 命中、反例 `const char* const` 0 命中）後掃全產品：3 處（`PvpApplication.cpp:427、862`、`SoundCues.cpp:15`），都是 `constexpr`（掃描器排除）且在 P11／Present，不在 (c) 檔。該當なし，不改。基線的 ZS2 三處是 P8 的 inline 全域（rewrite 清單仍報 3 處）。

## 回報架構師

1. **`ArenaMesh` 疑為 audit P13 的誤判**（審查者指出）：內容是 Client 怎麼畫烘好的地圖（含 `engine/render/*`、回傳 `Engine::Render::MeshData`、只連 `app_support`）。本批照核定清單放 Sim；若改判呈現，下一批搬到 Present 並從確認清單移到 Present（一檔、一行清單）。
2. **`App/AnimationSetDefinition`、`App/CharacterPresentationDefinition`、`AssetDefinitionHelpers.hpp` 的區**：Match 也用的內容定義，Sim 含入 App 三處（`PlayerAnimationContent.hpp:3`、`PlayerHitbox.hpp`、`PlayerAnimationContent.cpp`）。待區判定（宣告 Q5）。
3. **`Combat.hpp` 與 audit P1 分組不同**：audit 產品部分報告把它列在 P1 列；本批依 spec J-17 放 Sim（規則資料與 constexpr）。建議 audit 的 P1 列註明。
4. 既有的區混雜（審查 note）：`PVP_DOMAIN_SOURCES` 含 `Present/HitFeedback.cpp`、`Present/SoundCues.cpp`；兩個 Present 標頭含 P1 的 `PvpMatch.hpp`。遷移批處理。

## 殘留（確認清單 7 處；交遷移批的理由）

| 命中 | 類別 | 理由 |
|---|---|---|
| `Sim/Arena.hpp:9`、`Sim/MovementRulesFile.hpp:6`、`Sim/PlayerAnimationContent.hpp:6`、`Sim/PlayerPose.hpp:8` `<filesystem>` | B | P13 載入器的簽名（`Arena::Load(path)`、`LoadInstalledArenas`、`LoadStanceRules`、`WithContentAssets`、`LoadPlayerAnimationRules`、`LoadPlayerPoseDefinition`）；Io 效果在 `NativeFiles`（audit：包）。拆成「Pvp/ 的讀檔入口 → Sim 的 `Parse*`」是 API 變更，由遷移批把它們做成 Startup System 時一次改 |
| `Pvp/NativeFiles.hpp:4` `<filesystem>` | B | 經 `Arena.cpp`、`MovementRulesFile.cpp` 含入；同上 |
| `App/AnimationSetDefinition.hpp:8、23` `unordered_map` | F | 換 `std::map` 可能改走訪順序（digest 風險）；audit §4.5 已交決定性線核對 digest 編碼後再改 |

候選清單（P9，報告）：7 處——`Sim/Arena.hpp:9`、`Sim/PlayerAnimationContent.hpp:6`、`Sim/PlayerHitbox.hpp:9`（`LoadPlayerHitbox(path)`）、`Sim/PlayerPose.hpp:8` 的 `<filesystem>`（B）、`App/AnimationSetDefinition.hpp:8、23` 的 `unordered_map`（F）、`Sim/PlayerPose.cpp:17` 的 `unordered_set`（F；DET-1 E1 前 (b)，不是門）。rewrite 清單（P1／P5／P6，報告）：18 處——ZS1 13、ZS2 3、ZS4 2：D（P5 Host 時鐘：`ClientSimulation.hpp`、`PredictionElapsedTime.hpp`）與 E（P8 診斷：`MovementDiag`／`MovementTrace`／`MovementTraceWriter` 的 include、inline 全域、`steady_clock`；spec G.4 第 3 條，遷移批改句柄）。

26 處的去向：A 2 處消失（`Combat.hpp`、`Movement.hpp` 的 `<chrono>`）；B 4 處＋C 1 處（轉為 `Sim/PlayerPose.hpp:8`）＝確認清單的 5 處 ZS1；F 2 處＝確認清單的 ZS7；D 3 處＋E 14 處＝rewrite 清單的 17 處（rewrite 另多 `Sim/Arena.hpp:9` 一處，經 `PvpMatch.cpp` 含入）。

## 受理判定（宣告 §4；修訂 1 的名稱）

| 判定 | 結果 |
|---|---|
| J1 掃描 | **成立**。確認清單 9 檔→展開 26 檔、2,382 行、**7** 處，逐行等於釘住的七行（`test_zone_c_lists.py` 的 `RESIDUAL`；ZS1 5、ZS7 2；ZS2～ZS6 0）。候選 5→20 檔 7 處、rewrite 3→21 檔 18 處（報告）。報告 `after-{sources,candidates,rewrite}.json` |
| J2 不變 | **成立**。`authority_digest` ctest 通過；`--check-golden` 37 golden 0 不符；兩樹比對（base `3061e879` 的 runner vs 本分支）**42 情境 0 差異**（`j2_two_trees.txt`）；`git diff --stat 3061e879 -- fixtures` 空 |
| J3 既有全過 | **成立**。`ctest -L pvp` **61／61**（cpu 57、network 5、acceptance 30、gameplay 3、go 2、presentation 4、content 1；201 s，`j3_ctest_pvp.txt`）；`zone_scan.selftest` 通過；`zone_scan.baseline` 不存在 |
| J4 D2 棘輪 | **成立**。`kernel_prep_sites` 通過；白名單 47 列零變動；守衛的 diff＝`W_FILES`／`G_FILES` 的目錄（`SIM`）＋D3 掃描範圍擴到 `src/` 全目錄（修訂 2，締める面） |
| J5 突變 | **成立**。文字 4 條（`kz-ns-off-by-one-digit`、`kz-list-drops-a-file`、`kz-sim-chrono-returns`、`kz-wire-ms-off-by-one-digit`）**4／4 killed**；手動 1 條 `kz-sim-file-moved-out`（`Movement.cpp` 搬回 `src/Pvp/`）：`zone_c_scan_sources` 以契約錯誤失敗、`zone_c_lists` 失敗（`test_sim_sources_equal_confirmed_plus_candidates` 報 `Sim/Movement.cpp`），復原後兩者再通過 |
| J6 共通層 | **成立**。`git diff --name-status 3061e879`：`apps/` 外只有 `tests/object_fps_pvp/`、`build/acceptance/object_fps_pvp/`、`docs/`，以及 `tests/common/zone_scan/` 的兩個清單刪除與 `CMakeLists.txt` 刪 `zone_scan.baseline`；`engine/`、`framework/`、`build/cmake`、`.github/`、`tools/` 零 |
| J7 搬動數 | **成立**。rename 47（R091～R100 42 個、R078～R085 4 個：`GroundPoint`、`PlanarMovement`、`MovementRulesFile` 的 namespace 包裝佈局佔比大；`PlayerPresentation.hpp`→`PlayerPose.hpp` R080 是拆分）；新檔 `Sim/PlayerPose.hpp`（git 標為 `Present/PlayerPresentation.hpp` 新增）、三份清單、`test_zone_c_lists.py`、宣告、本 dev_log；空目錄 4 個已刪 |
| J8 審查 | **成立**（must 1 已修）。見「審查」節 |
| J9 L1 | 待 PR（三平台） |

## 突變

| id | 改法 | check | 預期失敗 | 結果 |
|---|---|---|---|---|
| `kz-ns-off-by-one-digit` | `maximumReferenceAgeNs` 250'000'000 → 25'000'000（25 ms） | `cpu` | `CombatTests` 的 `DecisionOf(match, 1).accepted`（200 ms 的參考被判 Expired） | killed（第二輪；第一輪同一失敗、expect 正規式未跳脫） |
| `kz-list-drops-a-file` | 確認清單少列 `Sim/Movement.cpp` | `zone_c_lists` | `test_sim_sources_equal_confirmed_plus_candidates` | killed |
| `kz-sim-chrono-returns` | `Sim/Movement.hpp` 加回 `#include <chrono>` | `zone_c_lists` | `test_confirmed_findings_are_exactly_the_recorded_residual` | killed |
| `kz-wire-ms-off-by-one-digit` | `IpcHost.cpp` 的除數 1'000'000 → 100'000（wire 2500 ms） | `ipc` | `maximum_reference_age_ms() == 250`（`CHECK( 2500 == 250 )`） | killed（第二輪；同上） |
| `kz-sim-file-moved-out`（手動，`manual_mutation_moved_out.sh`） | `src/Sim/Movement.cpp` 移回 `src/Pvp/` | `zone_c_scan_sources`＋`zone_c_lists` | 掃描器契約錯誤（列的檔不存在）；清單守衛報 `Sim/Movement.cpp` 多列 | killed；復原後兩個 ctest 再通過 |

既有 924 條：`file` 欄全部存在、`find` 各命中 1 次（193 條在搬動檔上）；照 D184 不在本批重跑，PR 線合併前的完整背景跑一次由使用者排。

## 審查

`xhigh-reviewer` 子代理（Opus 5.5／high，唯讀，約 12 分，讀約 50 檔）：**must 1、should 6、note 4**；全文在證據目錄 `review_opus.md`。A（搬錯區）在 must 修好後乾淨、B（型別轉換）乾淨：47 個 rename 都對得上宣告與 audit；拆分逐字一致（基線第 21～192 行對 `Sim/PlayerPose.hpp` 第 25～196 行 diff 為空）；每個整數值都等於原 chrono 的 ns 計數；MSVC 的 100 ns `steady_clock` 經 `duration_cast<nanoseconds>` 後 `.count()` 仍是 ns；wire 的 250 不變。

| 項 | 處置 |
|---|---|
| must：D3 寫點掃描只掃 `src/Pvp`，`Movement.cpp` 搬走後在 `StepMovement` 加寫點不會被抓 | 改成 `src/` 全目錄遞迴（締める面；宣告修訂 2）。守衛通過、`FAILURES_WRITERS` 不變 |
| should：ctest 名稱與判定與宣告原文不同 | 宣告修訂 1 記錄；門由 `zone_c_lists` 精確比對，比原文嚴 |
| should：`match_main.cpp` 含 Present 的標頭（Match host → renderer 類別） | 改含 `Sim/PlayerPose.hpp`（修訂 3）；測試與驗收程式維持只改路徑 |
| should：`Combat.hpp` 在 audit 產品部分報告第 63 行列在 P1（重寫）那一列 | 不改。理由：它是規則資料與 constexpr（`CombatRules`、`ShotDecision`、`HitPart`…），沒有 Tick 順序與 Body 寫入；spec J-17 點名它是 (c) 標頭並要求整數時間；P1 的「重寫」是 `PvpMatch` 的組織層。對照 audit 時以本段為準 |
| should：`ArenaMesh.hpp` 含 `engine/render/*`、回傳 `Engine::Render::MeshData`、只連 Client 的 `app_support` | **回報架構師：audit P13 把 `ArenaMesh` 判「留」疑為誤判**（內容是 Client 怎麼畫烘好的地圖）。本批照核定清單放 Sim，不改；確認清單因此含一個 render 依賴（不是 ZS 規則） |
| should：wire 換算 `/ 1'000'000`、`* 1'000'000` 沒有 CI 測試釘住 | `IpcHostTests` 加 `maximum_reference_age_ms() == 250`＋突變 `kz-wire-ms-off-by-one-digit`（修訂 5）；`ClientConnection.cpp` 的 `* 1'000'000` 仍只有本機 `worker_main.cpp`（333 ms）釘住，記進限界 |
| should：「Sim 不依賴 Present」沒有守衛 | `test_zone_c_lists.py` 加 `test_no_sim_file_includes_a_present_header`（修訂 4） |
| note：gateway 的 Go 註解仍說常數在 `Movement.hpp` | 兩處註解改成 `ConnectionLiveness.hpp` |
| note：清單守衛的正例是恆真式、rewrite 成員沒釘住 | 正例改成 `position_differences` 的真比對；rewrite 釘住三個成員（修訂 4） |
| note：`LoadPlayerPresentationDefinition`、`SamplePlayerPresentationPose`（帶武器配件）只有 Client 用卻進了 Sim | 照 Q4(a)（定義在 `PlayerPose.cpp`）；留給遷移批再拆 |
| note（既有、非本批造成）：`PVP_DOMAIN_SOURCES` 含 `Present/HitFeedback.cpp`、`Present/SoundCues.cpp`；`Present/SnapshotTimeline.hpp`、`Present/SoundCues.hpp` 含 P1 的 `PvpMatch.hpp`；`Sim/Arena.cpp`、`Sim/MovementRulesFile.cpp` 含 `Pvp/NativeFiles.hpp` | 記錄；遷移批處理 |

## 經緯

- 偵察把 26 處命中逐條歸因後發現卡上「26→0」做不到，寫宣告列六個決定點送架構師；架構師核定全採 (a)，並定為宣告錯誤的修正（門 26→7）。
- 搬檔用腳本（`move_files.py`：先檢查再 `git mv`、路徑改寫、namespace 包裝），一次完成 47 檔、90 檔的路徑改寫；之後的手動改動（拆分、時間型別、常數搬家、守衛路徑）以逐條 assert 的 Python 替換做。
- 第一次全建置 7 個錯誤：`fps::CanPlaceCharacterBody` 等 9 個 `fps::` 限定名（`CharacterCollision` 改進 `fps::pvp::sim` 後）與 4 個 chrono 型別的 `ShotReferenceAge` lambda（`std::optional{251ms}`、`std::optional{age}`、`std::optional{-1ns}`；偵察的 grep 只找 `nanoseconds` 字樣，漏了字面值）。都是編譯期錯誤、逐一改成等值的 ns 整數。
- `test_diagnostic_isolation.py` 兩個 ERROR：`ShotGate.cpp`、`SnapshotTimeline.hpp` 的固定路徑；改成新目錄，另把 `ClockSync` 使用者的掃描改成遞迴涵蓋全部產品目錄（不縮小）。
- 審查 must：D3 的寫點掃描因搬檔縮小了範圍——這類「守衛以目錄 glob 定範圍」的縮小是搬檔批的典型風險，宣告 §1.2 只查了 `W_FILES`／`G_FILES` 的路徑，漏了 glob。修正後掃 `src/` 全目錄。
- 突變工具要求 catalog 檔與 HEAD 一致，先提交程式 commit 再跑。
- 清單守衛的「Sim 不含 Present」第一版比對全文，被 `PlayerPose.hpp` 註解裡的路徑誤中；改成只看 `#include` 行。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-kernel-prep-zones/`（git 管理外，Intel Mac）：`before-{sources,candidates}.json`（搬前掃描）、`after-{sources,candidates,rewrite}.json`（搬後）、`move_files.py`（搬檔腳本）、`configure_base.log`、`authority_digest_base_3061e879`（base 版 runner）、`j2_two_trees.txt`／`j2_two_trees/`（兩樹比對）、`j2_check_golden.txt`、`j3_ctest_pvp.txt`、`j3_zone_scan_common.txt`、`j5_mutations_run_1st.txt`／`j5_mutations_1st.json`（第一輪）、`j5_mutations_rerun.txt`／`.json`（第二輪）、`j5_manual_mutant_moved_out.txt`、`manual_mutation_moved_out.sh`、`review_opus.md`。

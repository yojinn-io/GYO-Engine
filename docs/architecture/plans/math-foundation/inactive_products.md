# Math 基礎統一：未啟用產品的遷移清單

量測日期：2026-10-04（B7）。對象：`2117570`（Math 遷移做到 B6c；之後只做不改公開名稱的內部清理與文件）。對照：B0 基準 `0bd5363`。

本計畫依「未啟用產品除外」的範圍，不修改 object_fps、object_fps_v2 與 object_fps_preview。舊名稱移除後，它們重新啟用時需要先遷移；本文件是遷移清單。量測在 scratch 建置中進行，repo 沒有修改。計畫與紀錄見 [README](README.md)、[HANDOFF](HANDOFF.md#b7-收尾)。之後的 Result 統一又增加了遷移項目，見 [Result 統一的遷移清單](../result-unification/inactive_products.md)；重新啟用時兩份都要處理。

## 1. 結論

| 項目 | HEAD 2117570 | 基準 0bd5363 |
|---|---|---|
| 未啟用產品的 TU 數（12 個含 TU 的 target，含 app_support／domain library） | 104 | 104 |
| 編譯失敗的 TU | **39**（錯誤 425 筆，不設上限重編） | 0 |
| 10 個建置目標的 link | 無法 link（compile 失敗） | 全部成功 |
| cpu 標籤測試（9 個，依 test preset 的 `cpu\|shader` 篩選） | — | 9/9 通過 |

- **沒有既有破損**：基準可以完整編譯、link，cpu 測試全部通過。所以 39 個失敗 TU **全部**由 Math 計畫移除舊名稱造成。
- **根因集中在 9 個 header（7 種；WPD 與 WVM 在兩個產品各一份）、共 20 處**（見第 3 節）。有 14 個 TU 本身沒有錯誤，只要修好 header 就能恢復正常。
- **遷移量已驗證**：在 scratch 副本中套用以下修改後，104/104 TU 可以編譯，10 個 target 都能 link，cpu 測試 9/9 通過，結果與基準相同：
  - 機械式改名
  - 12 處 Ray／Segment 呼叫
  - 刪除 1 個本地 helper

  共改 35 個檔案（+109／−108）。這份 patch 只用來確認遷移量，沒有保留；重新啟用時請依本清單重做，並一併處理第 6 節的項目。
- `object_fps_preview` 的 2 個 TU 本身可以編譯，但它 link 到 object_fps 的 `app_support`，所以要等 object_fps 修好才能 link。

## 2. 原因代碼（舊名 → 新名）

| 代碼 | 舊名 | 遷移方式 | 未啟用產品中的出現次數 |
|---|---|---|---|
| R-F3 | `Engine::Render::Float3`（MuzzleProbe 中寫成別名 `Render::Float3`） | `Engine::Math::Vec3` | 23 ＋ 別名 7 |
| R-F2 | `Render::Float2`（只在 MuzzleProbe 的別名中出現） | `Engine::Math::Vec2` | 5 |
| M-V3 | `Engine::Model::Vec3`，以及 `using namespace Engine::Model` 下的非限定 `Vec3` | `Engine::Math::Vec3` | 11 ＋ 非限定（RigInspection、Mark23ModelTests） |
| M-M4 | `using namespace Engine::Model` 下的非限定 `Matrix4` | `Engine::Math::Matrix4` | 1（EnemyAttachmentTests:85） |
| M-TP | `Engine::Model::TransformPoint` | `Engine::Math::TransformPoint` | 10 |
| M-MUL | `Engine::Model::Multiply` | `Engine::Math::Multiply`（語義相同，column-major，先套用 b） | 3（只在 v2） |
| C-F3 | `Engine::Collision::Float3` | `Engine::Math::Vec3` | 11 |
| C-AABB | `Engine::Collision::Aabb` | `Engine::Math::Aabb`（成員仍是 `minimum/maximum`） | 12 |
| C-CAP | `Engine::Collision::Capsule` | `Engine::Math::Capsule`（成員仍是 `segmentStart/segmentEnd/radius`） | 7 |
| U-RECT | `Engine::Ui::UiRect` | `Engine::Math::Rect` | 6 |
| C-RAY | `RaycastAabb/RaycastCapsule(origin, direction, max, …)` | `(Math::Ray{origin, direction}, max, …)`。契約不變：direction 不需正規化，回傳世界距離 | 9 處呼叫 |
| C-SWEEP | `SweepSphereAgainstCapsule(start, end, r, …)` | `(Math::Segment{start, end}, r, …)` | 3 處呼叫 |
| ADL | MuzzleProbe 本地的 `Distance(Float2, Float2)` | 改名後與 `Math::Distance(Vec2, Vec2)` 重載衝突（ambiguous）。刪除本地版本，改用 Math 版。注意：hypot 改為 sqrt，數值可能有微小差異 | 1（第二波） |

未使用（不需處理）：`Render::Rect`、限定寫法的 `Model::Vec2/Quaternion/Matrix4`、`UiFloat2`、`Decode/EncodeSrgbComponent`。另外，`using namespace Engine::Model` 下非限定呼叫的 `TransformPoint(m, {})` 會經由 ADL 自動找到 Math 版本，不需修改。

## 3. Header 根因（先修這些）

| Header | 行：原因 | 受影響的 TU 數 |
|---|---|---|
| object_fps `include/RetroFPS/App/WeaponPresentationDefinition.hpp`（WPD） | 23、29：M-V3；41：R-F3 | 5 |
| object_fps `include/RetroFPS/App/WeaponViewModel.hpp`（WVM） | 34：R-F3 | 4 |
| object_fps_v2 `…/App/WeaponPresentationDefinition.hpp`（WPD） | 23、29：M-V3；41：R-F3 | 3 |
| object_fps_v2 `…/App/WeaponViewModel.hpp`（WVM） | 34：R-F3 | 2 |
| object_fps_v2 `…/Collision/CharacterCollision.hpp`（CharCol） | 10、15、20：C-AABB | 8 |
| object_fps_v2 `…/Collision/CombatCollision.hpp`（CombatCol） | 27：C-CAP | 17 |
| object_fps_v2 `…/Game/GameSession.hpp` | 175：C-AABB | 14 |
| object_fps_v2 `…/Gameplay/Enemy/EnemyRig.hpp` | 16、29、36：M-V3；48：C-CAP | 21 |
| object_fps_v2 `…/Gameplay/Enemy/EnemySystem.hpp` | 63、180：C-CAP；201：C-AABB | 17 |

## 4. 失敗 TU 一覽（39 個）

說明：
- 錯誤數是以 `-fsyntax-only -ferror-limit=0` 重編的結果，包含連鎖錯誤。ninja 原始 log 受 clang 20 筆上限截斷，5 個 TU 的錯誤數不完整。
- 「本檔原因」欄只列根因與行號。

| Owner／target | TU | 錯誤數 | 本檔原因（行） | 經由 header |
|---|---|---|---|---|
| object_fps / app_support | `src/App/CampaignContentLoader.cpp` | 3 | — | WPD |
| object_fps / app_support | `src/App/ObjectFpsPresentation.cpp` | 1 | — | WVM |
| object_fps / app_support | `src/App/WeaponPresentationDefinition.cpp` | 32 | R-F3 19,23,27,58,67；M-TP 64,169 | WPD |
| object_fps / app_support | `src/App/WeaponViewModel.cpp` | 7 | R-F3 39,163,164 | WPD, WVM |
| object_fps / domain | `src/App/ObjectFpsUi.cpp` | 4 | U-RECT 131,147,155 | — |
| object_fps / domain | `src/Collision/CombatCollision.cpp` | 13 | C-F3 61（`ToCollision` 的回傳型別）；C-RAY 110,166；C-SWEEP 173 | — |
| object_fps / acceptance | `diagnostics/MuzzleProbe.cpp` | 22 | R-F2 54,70,146,177；R-F3 112,134,137,138,140,152；M-TP 110；第二波 ADL 70/180/183 | WPD, WVM |
| object_fps / headless_tests | `App/AssetPresentationDefinitionTests.cpp` | 1 | M-V3 140 | — |
| object_fps / headless_tests | `App/ObjectFpsPresentationTests.cpp` | 10 | R-F3 285,304；M-TP 318；M-V3 328,340 | WPD, WVM |
| object_fps / model_tests | `Model/Mark23ModelTests.cpp` | 4 | M-V3 169（非限定） | — |
| object_fps_v2 / app | `main.cpp` | 9 | — | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / app_support | `src/App/CampaignContentLoader.cpp` | 7 | — | WPD, EnemyRig |
| object_fps_v2 / app_support | `src/App/CharacterPresentationDefinition.cpp` | 2 | M-MUL 231 | — |
| object_fps_v2 / app_support | `src/App/EnemyPresentation.cpp` | 10 | C-CAP 242 | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / app_support | `src/App/EnemyPresentationDefinition.cpp` | 13 | M-V3 119；M-TP 145 | EnemyRig |
| object_fps_v2 / app_support | `src/App/ObjectFpsApplication.cpp` | 9 | — | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / app_support | `src/App/ObjectFpsPresentation.cpp` | 10 | — | WVM, CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / app_support | `src/App/ObjectFpsRuntimeClient.cpp` | 9 | — | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / app_support | `src/App/WeaponPresentationDefinition.cpp` | 32 | R-F3 19,23,27,58,67；M-TP 64,169 | WPD |
| object_fps_v2 / app_support | `src/App/WeaponViewModel.cpp` | 7 | R-F3 40,138,139 | WPD, WVM |
| object_fps_v2 / domain | `src/App/ObjectFpsUi.cpp` | 13 | U-RECT 131,147,155 | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / domain | `src/Collision/CharacterCollision.cpp` | 11 | C-F3 15,143；C-AABB 56,62,81,94 | CharCol |
| object_fps_v2 / domain | `src/Collision/CombatCollision.cpp` | 17 | C-F3 62；C-RAY 105（第二波）,118,159；C-SWEEP 166 | CharCol, CombatCol |
| object_fps_v2 / domain | `src/Game/GameSession.cpp` | 12 | — | CharCol, CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / domain | `src/Gameplay/Combat/ProjectileSystem.cpp` | 1 | — | CombatCol |
| object_fps_v2 / domain | `src/Gameplay/Enemy/EnemyRig.cpp` | 8 | M-TP 10；M-MUL 37,40 | EnemyRig |
| object_fps_v2 / domain | `src/Gameplay/Enemy/EnemySpawnDirector.cpp` | 7 | — | EnemyRig, EnemySystem |
| object_fps_v2 / domain | `src/Gameplay/Enemy/EnemySystem.cpp` | 25 | C-AABB 316,1097；C-F3 1124,1125,1147,1154,1157,1158；C-CAP 1148；第二波 C-RAY 1130,1164、C-SWEEP 1152 | CharCol, EnemyRig, EnemySystem |
| object_fps_v2 / domain | `src/Gameplay/Player/PlayerController.cpp` | 4 | C-F3 107 | CharCol |
| object_fps_v2 / acceptance | `RigInspection.cpp` | 30 | M-V3 24–56（非限定，9 行） | — |
| object_fps_v2 / acceptance | `main.cpp` | 13 | C-CAP 214 | CharCol, CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / tests | `App/DebugRuntimeTests.cpp` | 9 | — | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / tests | `App/DebugUiTests.cpp` | 9 | — | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / tests | `AssetIntegrationTests.cpp` | 15 | M-TP 179,182；C-RAY 264 | CharCol, CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / tests | `CharacterAccessoryTests.cpp` | 9 | — | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / tests | `EnemyAttachmentTests.cpp` | 5 | M-M4 85（非限定） | EnemyRig |
| object_fps_v2 / tests | `EnemySystemTests.cpp` | 12 | C-AABB 484 | CharCol, CombatCol, EnemyRig, EnemySystem |
| object_fps_v2 / tests | `PresentationTests.cpp` | 9 | — | CombatCol, GameSession, EnemyRig, EnemySystem |
| object_fps_v2 / tests | `RegionShotTests.cpp` | 11 | C-RAY 154（另有 SDK `count_if.h` 的連鎖錯誤） | CombatCol, GameSession, EnemyRig, EnemySystem |

**第二波**：第一輪中，有些錯誤被前面失敗的宣告遮蔽，所以沒有報出來，只有在完成改名後才出現。共 5 處，都已併入上表：

- v2 `EnemySystem.cpp`：1130 和 1164 的 `RaycastAabb`、1152 的 `SweepSphereAgainstCapsule`
- v2 `CombatCollision.cpp:105` 的 `RaycastAabb`
- MuzzleProbe 的 `Distance` 衝突

修好上述各點後，沒有出現其他錯誤。

## 5. 可以編譯的 TU（65 個）

| Owner／target | TU |
|---|---|
| object_fps / app | `main.cpp` |
| object_fps / app_support | `AnimationSetDefinition`、`CharacterPresentationDefinition`、`ObjectFpsApplication`、`ObjectFpsRuntimeClient` |
| object_fps / domain（20） | `Collision/GridCollision`、`Data/{Csv,GameData}`、`Game/{CampaignContent,CampaignRunState,GameFlow,GameSession}`、`Gameplay/Combat/ProjectileSystem`、`Gameplay/Enemy/{EnemySpawnDirector,EnemySystem}`、`Gameplay/Player/{PlanarMovement,Player,PlayerCombatState,PlayerController,PlayerSettings}`、`Gameplay/Weapon/WeaponController`、`Rendering/MapGeometryGenerator`、`World/{GridMap,GridMapLoader,World}` |
| object_fps / acceptance | `main`、`diagnostics/{HeadlessSmoke,PackageProbes,VisualProbe}` |
| object_fps / headless_tests（17） | `TestMain`、`App/ObjectFpsUiTests`、`Collision/{CollisionTests,CombatCollisionTests}`、`Data/GameDataCatalogTests`、`Game/{CampaignRunStateTests,GameFlowTests,GameSessionTests}`、`Gameplay/{EnemySpawnDirectorTests,EnemySystemTests,PlayerCombatStateTests,PlayerControllerTests,ProjectileSystemTests,WeaponControllerTests}`、`Rendering/{EnemyBillboardTests,MapGeometryTests}`、`World/WorldTests` |
| object_fps_preview（tool） | `main`、`ViewmodelPreview`（link 時依賴 object_fps 的 app_support） |
| object_fps_v2 / app_support | `AnimationSetDefinition` |
| object_fps_v2 / domain（15） | `Collision/GridCollision`、`Data/{Csv,GameData}`、`Game/{CampaignContent,CampaignRunState,GameFlow}`、`Gameplay/Player/{PlanarMovement,Player,PlayerCombatState,PlayerSettings}`、`Gameplay/Weapon/WeaponController`、`Rendering/MapGeometryGenerator`、`World/{GridMap,GridMapLoader,World}` |
| object_fps_v2 / tests | `TestMain` |

## 6. 可以編譯，但屬於計畫要取代的對象

以下項目不影響編譯。重新啟用時是否一併處理，由使用者決定。

- **`fps::Float3`／`fps::Float2` 自有定義**：
  - 位置：`apps/object_fps{,_v2}/include/RetroFPS/Math/Vector.hpp`。
  - 使用量（非限定 Float3／Float2）：
    - object_fps：95／97
    - object_fps_v2：102／94
    - tests/object_fps：22／48
    - tests/object_fps_v2：11／5
  - 注意：`fps::Float2` 是 `{x, z}`（XZ 平面），**不能**機械式替換成 `Math::Vec2{x, y}`。
- **`ToCollision` 轉換器**：object_fps 和 v2 各 12 處呼叫。完成機械改名後，它們會變成 `fps::Float3 → Math::Vec3` 的轉換，正是 PLAN 第 4 節第 2 點指出的重複轉換。
- **本地數學 helper**：`Length`、`Normalize`、`Finite`、`Distance`、`ToCollision` 等，共 13 個檔案有定義（集中在兩個 app 的 `CombatCollision`、`ProjectileSystem`、`EnemySystem`、`GameSession`、`WeaponPresentationDefinition`）。
- **手寫的 Euler XYZ**：兩個 app 的 `EvaluateWeaponMuzzleViewCameraPosition` 自己寫了 Euler XYZ 運算，可以考慮改用 `Math::ComposeEulerXYZ`。這屬於行為等價的改寫，需要另外驗證。

## 7. 方法與環境

**建置來源與設定**
- **來源樹**：以 `git archive HEAD` 和 `git archive 0bd5363` 匯出到 scratch。repo 只讀不寫。
- **選取產品**：
  - 用 `-DGYO_REGISTRY_FILE=<scratch>/projects.csv` 指向 scratch 中的 registry，其中把 object_fps 和 object_fps_v2 設為 enabled=1。
  - 加上 `GYO_APPS=object_fps;object_fps_v2` 和 `GYO_TOOLS=object_fps_preview`。tools.csv 原本就 enabled=true、default=false，不需要修改。
- **其餘設定**：沿用 test preset，包括 RelWithDebInfo、`BUILD_TESTING=ON`、`GYO_TEST_PROFILE=full`、`GYO_TOOLS` 明確指定、`GYO_ENABLE_PACKAGING=OFF`。另外：
  - `GYO_OUTPUT_ROOT` 設在 scratch。
  - `FETCHCONTENT_FULLY_DISCONNECTED=ON`，`FETCHCONTENT_SOURCE_DIR_{SDL3,SDL3_IMAGE,SDL3_TTF,UFBX,NLOHMANN_JSON,DOCTEST}` 指向 repo `build/target/_build/test/_deps/*-src`，只讀。
  - `GYO_SHADER_TOOL_EXECUTABLE` 指向既有的 host 工具，只讀。

**編譯與驗證**
- **編譯器**：AppleClang（`/usr/bin/c++`），Intel Mac x86_64。
- **建置步驟**：用 `ninja -k 0` 建置以下 10 個 target：
  - `gyo_object_fps`、`gyo_object_fps_v2`、`gyo_object_fps_preview`
  - `gyo_object_fps_headless_tests`、`gyo_object_fps_model_tests`、`gyo_object_fps_acceptance`
  - `gyo_object_fps_v2_tests`、`gyo_object_fps_v2_acceptance`
  - 兩個 `*_test_runtime`
- **逐 TU 重編**：之後依 `compile_commands.json`，對每個 TU 以 `-fsyntax-only -ferror-limit=0` 重編並分類錯誤。
- **基準驗證**：在基準樹做完全相同的 configure 和建置。另外跑 `ctest -L "cpu|shader" -R ^object_fps`。
- **遷移驗證（scratch）**：在 scratch 副本套用以下修改後，重新 configure、建置並執行 ctest：
  - 機械改名
  - 12 處 Ray／Segment 包裝
  - 刪除 MuzzleProbe 的 `Distance`

## 8. 做到與沒做到

**做到**
- 離線完成 configure，沒有下載任何東西。protobuf、absl 等依賴只有 pvp 需要，這次不涉及。
- 完成基準的完整比對。
- 驗證了遷移後可以 link，cpu 測試通過。

**沒做到**
- object_fps 的 7 個 GPU smoke 測試和 v2 的 `enemy_gpu`，共 8 個 `gpu` 標籤測試沒有執行。它們會開視窗，而且不在 test preset 的範圍內。
- 沒有做 packaging（`GYO_ENABLE_PACKAGING=OFF`，與 test preset 相同）。
- 只在 macOS x86_64／AppleClang 上量測。MSVC、GCC 或 arm64 上的錯誤訊息和連鎖錯誤數量可能不同，但根因相同。
- 遷移 patch 只用來確認遷移量，沒有保留。

# Assert／Result 統一：未啟用產品的遷移清單

量測日期：2026-10-04（R6）。對象：`f72bfbd`（R0–R5 合併後）。對照：Result 計劃開始前的 `eeebc1e`（math-foundation 完成時）。

本計劃依範圍不修改 object_fps、object_fps_v2 與 object_fps_preview。這些產品重新啟用時，除了 [Math 的遷移清單](../math-foundation/inactive_products.md)，還要處理本文件列出的項目。量測在 scratch 建置中進行，repo 沒有修改。計畫與紀錄見 [README](README.md)、[HANDOFF](HANDOFF.md#r6-收尾)。

## 1. 結論

| 項目 | `f72bfbd`（本計劃完成後） | `eeebc1e`（本計劃開始前） |
|---|---|---|
| 未啟用產品的 TU 數 | 104 | 104 |
| 編譯失敗的 TU | 40（錯誤 507 筆） | 39（錯誤 425 筆，全部來自 Math 計劃） |
| 本計劃新增 | 共 82 筆錯誤，分布在 20 個 TU：1 個新增失敗，19 個原本就因 Math 失敗 | — |

- 新增失敗的 TU 只有 `apps/object_fps/src/App/ObjectFpsApplication.cpp`（R3 的 `Register` 改回傳 `void`）。
- 其餘 78 筆錯誤分布在 19 個原本就因 Math 計劃失敗的 TU。Math 的錯誤修好之後，這些錯誤才會成為唯一的阻礙。
- 以 `-fsyntax-only -ferror-limit=0` 逐 TU 量測，所以 Math 的錯誤不會遮蔽本計劃造成的錯誤。不過 header 中的錯誤可能有連鎖效應，實際修改時以重新編譯為準。

## 2. 原因代碼與遷移方式

| 代碼 | 批次 | 原因 | 遷移方式 | 錯誤數／TU 數 |
|---|---|---|---|---|
| R2-OK | R2 | 靜態 `Result<...>::Ok`／`Err` 已移除 | 成功時直接回傳值（大括號值寫成 `T{...}`），void 寫 `return {};`；失敗寫 `return Engine::Base::Err(e);` | 22／5（全部是測試的 stub） |
| R3-REGISTER | R3 | `LoaderRegistry::Register` 改回傳 `void`（失敗只有 API 誤用，已改成 `GYO_ASSERT`） | 刪除回傳值的檢查，直接呼叫 | 19／6 |
| R4-MODEL-E | R4 | Model 與 ModelRenderer 的 E 從 `std::string` 改成 `ModelError`／`ModelRendererError` | `x.error()` 當成字串使用的地方改成 `x.error().message`；需要 code 時用 `.code`；顯示用 `Engine::Base::Describe` | 41／13 |

## 3. 受影響的 TU

**R2-OK**
- `tests/object_fps/App/AssetPresentationDefinitionTests.cpp`（1）
- `tests/object_fps/App/ObjectFpsPresentationTests.cpp`（9）
- `tests/object_fps_v2/App/DebugRuntimeTests.cpp`（6）
- `tests/object_fps_v2/AssetIntegrationTests.cpp`（1）
- `tests/object_fps_v2/PresentationTests.cpp`（5）

**R3-REGISTER**
- `apps/object_fps/src/App/ObjectFpsApplication.cpp`（4，唯一新增失敗的 TU）
- `apps/object_fps_v2/src/App/ObjectFpsApplication.cpp`（4）
- `tests/object_fps/App/AssetPresentationDefinitionTests.cpp`（3）
- `tests/object_fps/App/ObjectFpsPresentationTests.cpp`（4）
- `tests/object_fps/Model/Mark23ModelTests.cpp`（1）
- `tests/object_fps_v2/AssetIntegrationTests.cpp`（3）

**R4-MODEL-E**
- `apps/object_fps/src/App/WeaponPresentationDefinition.cpp`（2）、`WeaponViewModel.cpp`（2）
- `apps/object_fps_v2/src/App/CharacterPresentationDefinition.cpp`（3）、`EnemyPresentation.cpp`（10）、`EnemyPresentationDefinition.cpp`（2）、`WeaponPresentationDefinition.cpp`（2）、`WeaponViewModel.cpp`（5）
- `apps/object_fps_v2/src/Gameplay/Enemy/EnemyRig.cpp`（1）、`EnemySystem.cpp`（3）
- `build/acceptance/object_fps/diagnostics/MuzzleProbe.cpp`（1）
- `build/acceptance/object_fps_v2/RigInspection.cpp`（4）、`main.cpp`（3）
- `tests/object_fps/App/AssetPresentationDefinitionTests.cpp`（3）

這些修改和 `object_fps_pvp` 在 R2–R4 的被迫修改相同，可以直接參考該批的 diff。

## 4. 編譯期看不到的行為變化

下列程式碼依賴 collision 的 `std::invalid_argument` 做內容驗證。R1 之後，collision 的前提不成立是 Programmer Error，會 abort，不再丟例外，所以這些 catch 接不到：

| 產品 | 位置 |
|---|---|
| object_fps | `src/Gameplay/Player/PlayerController.cpp:59,63`、`src/Gameplay/Enemy/EnemySystem.cpp:636,639`、`src/Game/GameSession.cpp:385,388,589,591` |
| object_fps_v2 | 同名檔案的對應位置（例如 `src/Game/GameSession.cpp:384`） |

重新啟用時，應該在載入內容的時候驗證身體尺寸與生成位置（Q1，回傳 Result），而不是在執行期 catch 例外。`object_fps_pvp` 的 `Arena::Validate` 是可以參考的寫法。

## 5. 方法與環境

- **來源樹**：以 `git archive f72bfbd` 和 `git archive eeebc1e` 匯出到 scratch。repo 只讀不寫。
- **選取產品**：`-DGYO_REGISTRY_FILE=<scratch>/projects.csv`（object_fps、object_fps_v2 設為 enabled=1），加上 `GYO_APPS=object_fps;object_fps_v2` 與 `GYO_TOOLS=object_fps_preview`。
- **其餘設定**：沿用 test preset（RelWithDebInfo、`BUILD_TESTING=ON`、`GYO_TEST_PROFILE=full`、`GYO_ENABLE_PACKAGING=OFF`）。另外：
  - `GYO_OUTPUT_ROOT` 設在 scratch。
  - `FETCHCONTENT_FULLY_DISCONNECTED=ON`，各個 `FETCHCONTENT_SOURCE_DIR_*` 指向 repo 既有的 `build/target/_build/test/_deps/*-src`（唯讀）。
  - `GYO_SHADER_TOOL_EXECUTABLE` 指向既有的 host 工具。
- **建置**：`ninja -k 0`。失敗的建置步驟：`eeebc1e` 39 個、`f72bfbd` 40 個。
- **分類**：依 `compile_commands.json`，對每個未啟用產品的 TU 以 `-fsyntax-only -ferror-limit=0` 重編，再比較兩棵樹相同 TU 的錯誤訊息（忽略行號）。新增的錯誤依訊息歸入上表的三類，沒有無法歸類的錯誤。
- **編譯器**：AppleClang（`/usr/bin/c++`），Intel Mac x86_64。MSVC 或 GCC 上的訊息和連鎖錯誤數量可能不同，但原因相同。
- **沒做到**：沒有實際做遷移並驗證 link 與測試（Math 的清單有做）。本計劃的修改都是逐行的機械替換，`object_fps_pvp` 已經做過同樣的遷移並通過測試。

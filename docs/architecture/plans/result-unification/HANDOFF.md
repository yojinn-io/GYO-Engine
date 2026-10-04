# Assert／Result 統一：交接

更新：2026-10-04。**R0 完成（PR [#30](https://github.com/yojinn-io/GYO-Engine/pull/30) 合併為 `3b9765e`）。R1 本機驗收完成，PR [#31](https://github.com/yojinn-io/GYO-Engine/pull/31) 待 CI。**

## 閱讀入口

1. [進度與執行規則](README.md)。
2. [核准版計畫](PLAN.md)：概念、處理規則、規格、批次與驗收、Architecture Delta。
3. 本文件：各批實際做了什麼、決策、證據位置、未結事項。

## 決策紀錄

| 日期 | 決策 | 來源 |
|---|---|---|
| 2026-10-03 | 保留兩個參數的 `Result<T, E>`；不做全域 error enum、`Result<T>`、`error_code` category、error chain | 使用者 |
| 2026-10-03 | 契約違反（Programmer Error）用 Assert，不用 Result；release 也檢查（永遠生效），handler 可替換 | 使用者 |
| 2026-10-04 | 核心概念：Assert = Programmer Error、Result = Runtime Error | 使用者 |
| 2026-10-04 | 目標：可讀性、Apps 以外的風格統一、統一的處理邏輯；Apps 只做被迫的修改 | 使用者 |
| 2026-10-04 | 資料驗證型 API（wide contract）維持回傳 Result，寫入文件 | 使用者 |
| 2026-10-04 | 只在 debug 生效的 Assert 等第一個昂貴檢查出現時再加 | 使用者 |
| 2026-10-04 | 等 math-foundation 計劃完成（B7 合併）後才開工，第一批為任務校正 | 使用者 |
| 2026-10-04 | 各批的建議檔位（PLAN 4.1） | 使用者核准計劃 |
| 2026-10-04 | 更正：Assert 適用於整個 engine，包括 Math；`GYO::Base` 比 Math 更低。原先「Math 不加 Assert、不連 Base」是規劃 agent 未經查證的推論，`math.md` 沒有這條規則 | 使用者 |
| 2026-10-04 | `Math::Clamp` 的前提檢查改用永遠生效的 `GYO_ASSERT`，並拿掉 `noexcept` | 使用者 |
| 2026-10-04 | 以 Result 回傳的 API 誤用改成 Assert；只有 SdlGpu device 的 `WrongThread`、`InvalidHandle`、frame 狀態錯誤維持 Result 並寫入文件 | 使用者 |
| 2026-10-04 | 29 個未編譯檔：只做被迫的相容修改（範圍界定為 Apps 只做被迫修改後即確定） | 計劃（使用者原定「R0 再決定」） |
| 2026-10-04 | `UiError` 新增 `detail`；`CodedError` 要求 `code`、`message`、`detail`。理由：處理規則 5（跨模組轉換）與 `Describe` 需要統一的欄位 | R0 校正（對抗檢查建議） |
| 2026-10-04 | 跨模組轉換時外層 message 保留內層原文，內層 code 名稱放進 detail。理由：`UfbxModelTests.cpp:172-173` 鎖住了外層 message 含內層訊息 | R0 校正（對抗檢查建議） |
| 2026-10-04 | `Result` 可從任何 `Err<G>` 建構（`is_constructible_v<E, G>`）。理由：R4 之前 model 有 42 處字串字面值 Err | R0 校正（對抗檢查建議） |
| 2026-10-04 | MSVC 全專案改用符合標準的前處理器（`/Zc:preprocessor`），消除巨集寫法上的 MSVC 特例；設定位置為 `GyoBuild.cmake` 的全域 `add_compile_options`（比照 `-ffp-contract=off`），並在 `Assert.hpp` 加防呆 `#error`；放在 R1 | 使用者 |

## R0 任務校正

狀態：**完成**。PR [#30](https://github.com/yojinn-io/GYO-Engine/pull/30) 於 2026-10-04 合併為 `3b9765e`，L1 四列與 CI gate 通過（分支 `claude/result-unification-r0`，基準 `eeebc1e`）。
環境：Intel Mac（x86_64），Apple clang 21；cmake／ninja 以絕對路徑呼叫。

### 盤點方式

使用 ultracode：3 個唯讀盤點 agent（engine 的 Result／Error 介面、Apps 以外的失敗點分類、ui_editor／Apps 的被迫修改點與建置資訊）加 1 次對抗檢查。對抗檢查核對了 30 多項主張，指出 13 項錯誤與 15 處漏列。prompt 帶 commit hash，不沿用規劃階段的快取。

### 基線

| 項目 | 結果 | 保存位置 |
|---|---|---|
| core preset（configure／build／ctest） | 19／19 通過 | [baseline/core_tests.txt](baseline/core_tests.txt) |
| test preset（configure／build／ctest） | 46／46 通過 | [baseline/test_tests.txt](baseline/test_tests.txt) |
| 依賴圖（test preset） | GYO 相關邊 144 條（含 #28 的 `engine -> gyo_math`） | [baseline/gyo_dependency_edges.txt](baseline/gyo_dependency_edges.txt) |
| pvp 未編譯檔的 syntax-only | 29／29 PASS | [baseline/pvp_uncompiled_syntax.tsv](baseline/pvp_uncompiled_syntax.tsv) |
| 稽核計數 | 17 種 pattern × 5 個區域 | [baseline/audit.tsv](baseline/audit.tsv) |
| nodiscard 警告（build.log） | core 0、test 0 | — |
| 失敗點分類 | 173 列，依 Q0–Q4 分類並標出負責批次 | [baseline/failure_sites.tsv](baseline/failure_sites.tsv) |
| 資料驗證型 API | 16 個 | [baseline/wide_contract_sites.tsv](baseline/wide_contract_sites.tsv) |

基線在 `eeebc1e` 上取得：建置期間 characterization 測試先暫存（stash），所以測試清單與建置都是原狀。`audit.tsv` 也在同樣的狀態下產生。

指令（從 repo 根目錄；cmake／ninja 需要在 PATH 上）：

```text
cmake --preset <core|test> && cmake --build --preset <core|test> && ctest --preset <core|test>
ctest --preset <core|test> -N
cmake --preset test --graphviz=<dir>/deps.dot
python3 docs/architecture/plans/math-foundation/scripts/dot_edges.py <dir>/deps.dot
python3 docs/architecture/plans/math-foundation/scripts/syntax_check.py build/target/_build/test/compile_commands.json docs/architecture/plans/math-foundation/baseline/pvp_uncompiled_sources.txt <dir>
python3 docs/architecture/plans/result-unification/scripts/result_audit.py [--detail <pattern>]
```

### Characterization

| 測試 | 鎖住的行為 |
|---|---|
| `tests/common/core/io/VfsTests.cpp`（新增，加入 `engine_tests`） | 正式程式碼唯一依 error code 分支的地方。Stat、讀取用 Open、Exists：遇到 NotFound 就改查下一個 mount；遇到其他錯誤立刻停止並回傳它；全部找不到時回報最後一個 NotFound（Exists 則回傳 false） |
| `AssetManagerTests.cpp`（新增 2 個 case） | 非同步 `KeepOldIfAny` 失敗：published handle 維持 Ready，`GetError` 回報與 candidate 相同的錯誤（`SourceReadFailed`）；之後成功的 reload 清掉錯誤。同步 `KeepOldIfAny` 失敗：回傳 published handle，資料仍是舊的，`GetError` 回報錯誤 |

AssetManager 的兩個 case 用 `REQUIRE(error)`、`error->code`、`CHECK_FALSE(GetError(...))` 寫成，R3 把 `GetError` 改成回傳 optional 後不必修改。
Vfs 的 10 個 `IsNotFound` 中，本批鎖住 Stat、讀取用 Open、Exists 三種；其餘（寫入用 Open、CreateDirectories、Remove、Move、Copy、List、ToNativePathString）在產品中沒有呼叫端，留在 R3 收斂別名時一併檢查。

### 計劃校正（對抗檢查與本批發現）

- **PR 的 CI 涵蓋範圍**：PR 的 L1 四列本來就包含 MSVC 與 clang，會 build 並跑 test preset；只有 core preset 限定 Linux。原計劃「PR 只有 Linux 驗證、需另外觸發 workflow_dispatch」是錯的，已更正。
- **`MountTableTests` 改為 `VfsTests`**：NotFound 改查下一個 mount 的邏輯在 `Vfs.hpp`，`MountTable` 只提供判斷函式。
- **#28 的 `Math::Clamp`**：用了 `<cassert>`。依使用者決定在 R1 改成 `GYO_ASSERT`。
- **API 誤用以 Result 回傳**：盤點漏列，對抗檢查補上（UiRuntime 10、UiRenderer 3、LoaderRegistry 3、AssetManager、Renderer、SdlGpu）。依使用者決定分配到 R3、R4，SdlGpu 例外。
- **數字與位置的更正**：`::Ok({` 是 3 處，不是 10 處；範圍內的描述 helper 是 4 份（漏了 `tests/common/runtime_sdl/main.cpp:29`）；未編譯檔依賴 collision 例外的位置是 `PlayerController.cpp:43-62`、`EnemySystem.cpp:584-620`；`AssetCatalog.cpp:92-98`。
- **其他新增項目**（已寫進 PLAN 對應批次）：`Result<void,E>::value()` 的 `noexcept`、`CatalogParser.cpp:14` 的 `catch (...)`、`UiRuntime.cpp:237` 的 `.at()`、`UiRuntime.cpp:257` 不可到達的分支、`ModelRenderer.cpp:113` 的原樣傳遞、`std::filesystem::absolute` 的丟例外版本、tests/common 中 5 處丟棄 `Register` 回傳值、`MountTable.hpp:165` 的 namespace 註解。
- **已確認無風險**：已編譯的 pvp 程式碼不依賴 collision 的例外（`Arena::Validate` 與 `ValidMovementCommand` 先驗證了輸入）；pvp 的 9 個 `CHECK_THROWS_AS` 都來自 app 自己的檢查，R1 不影響它們。

### 本機驗收（含 characterization）

| 項目 | 結果 |
|---|---|
| core preset | 19／19 通過（測試清單與基線相同） |
| test preset | 46／46 通過（測試清單與基線相同） |
| `engine_tests` | 69 個 case、1056 個 assertion 全部通過（新增 Vfs 5 個、AssetManager 2 個） |
| nodiscard 警告 | core 0、test 0 |
| diff 範圍 | 只有 `docs/` 與 `tests/` |

### 未結事項

- `AssetError.hpp:42-67` 被註解掉的舊 struct 會讓 `error_code_none` 多算 3 處；R3 刪除它。

## R1 概念文件與 Assert

狀態：**本機驗收完成**（2026-10-04，分支 `claude/result-unification-r1`，基準 `3b9765e`），PR [#31](https://github.com/yojinn-io/GYO-Engine/pull/31) 待 CI。

### 變更

- **`GYO::Base`**：新增 `engine/base/CMakeLists.txt`（INTERFACE、header-only、只依賴標準函式庫）。`engine/CMakeLists.txt` 先 `add_subdirectory(base)` 再 math；`engine` 的 `base/include` include 目錄改成 `PUBLIC GYO::Base`。`GYO::Math` 以 INTERFACE、`GYO::Collision` 以 PRIVATE 連它。shader host 仍以路徑 include `Sha256.hpp`，沒有變更。
- **`Assert.hpp`**：`GYO_ASSERT`、`AssertionFailure`（不繼承 `std::exception`）、`SetAssertionHandler`、`DefaultAssertionHandler`、`ReportAssertionFailure`。handler 存放在 `inline constinit std::atomic`；重入 guard 是 `thread_local` 旗標加 RAII scope，handler 丟例外時也會復原。巨集是 variadic，只對 `__VA_ARGS__` 做字串化與原地展開，不轉送給其他巨集。
- **改用 Assert 的地方**：
  - Collision 15 處（`Collision.cpp` 8、`CapsuleQueries.cpp` 7）。條件寫成原本 throw 條件的否定，NaN 的判定與原本完全相同。只放在非 noexcept 的 `Validate*` 與公開查詢。
  - `FixedTickRuntime` 2 處，檢查仍在任何狀態變更之前（`FixedTickRuntimeTests` 的 `TickId()==0` 照樣通過）。
  - `Math::Clamp`：`<cassert>` 改成 `GYO_ASSERT` 並拿掉 `noexcept`。
  - Result 的 `value()`／`error()`（共 11 個存取子）：讀取未持有的一方改成 `GYO_ASSERT`，內部改用 `get_if`；`Result<void,E>::value()` 拿掉 `noexcept`。
- **Lakos rule 的延伸**（計劃原本只寫 `Clamp`）：Vec2／Vec3／Vec3d／Vec4 的 `Clamp` 與 `ClosestPoint(point, Aabb)`／`(point, Aabbd)` 會把呼叫端給的上下界傳給 `Clamp`，所以一併拿掉 `noexcept`。Quaternion、Segment、ColorSpace 傳入的是常數上下界，不可能違反，維持 noexcept。
- **`CatalogParser.cpp:14`**：`catch (...)` 收窄為 `catch (const std::exception&)`。
- **測試支援**：`tests/common/support/AssertTestSupport.hpp` 與 INTERFACE target `gyo_test_support`（`ScopedAssertionHandler`、`GYO_CHECK_ASSERTS`、MSVC 上停用 abort 對話框）。計劃提到的 doctest exception translator 沒有加：預設 handler 在 `GYO_CHECK_ASSERTS` 之外仍是 abort，`AssertionFailure` 不會逃到 doctest；header 中的 translator 也會在每個 TU 重複註冊。
- **測試**：
  - 新增 `gyo_base_tests`（`AssertTests.cpp`、`ResultTests.cpp`），涵蓋條件只求值一次、含逗號的條件式、失敗位置、同一個 case 連續失敗、handler 的替換與還原，以及 Result 讀取未持有一方會觸發 Assert。另有一個反向對照：`GYO_CHECK_ASSERTS` 在沒有觸發 Assert 時判定失敗（doctest `should_fail`）。
  - 新增 abort probe：`gyo_assert_abort_probe` 加 `AssertAbortProbe.cmake`，三種模式（預設 handler、handler 返回、handler 內再失敗），由 `gyo_base.assert_abort.{default,returning,nested}` 執行。
  - 改寫：`CollisionTests` 12 處與 `FixedTickRuntimeTests` 5 處 `CHECK_THROWS_AS(..., std::invalid_argument)` 改成 `GYO_CHECK_ASSERTS`。
  - `gyo_math_tests` 新增 `Clamp`、vector `Clamp`、`ClosestPoint(Aabb)` 違反前提的測試，以及相等上下界和 NaN 上下界不觸發的確認。
- **文件**：新增 `docs/architecture/error-handling.md`；`docs/architecture.md` 的依賴表加入 `GYO::Base`，並加入 Architecture Delta 段落；`math.md` 改為依賴 Base，並寫入 `Clamp` 的新行為；`creating_apps.md` 的 public target 清單加入 `GYO::Base`。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 23／23 通過（R0 的 19 個加上 `gyo_base_tests` 與 3 個 abort probe） |
| test preset | 50／50 通過（46 加上同樣的 4 個） |
| abort probe | 三種模式都以 abort 結束（本機為 `Subprocess aborted`），stderr 含條件式原文 |
| 依賴圖 | 與 R0 相比只多出計劃列出的邊：`engine`、`gyo_math`、`gyo_collision` 到 `gyo_base`；`gyo_test_support` 到 `gyo_base` 與 doctest；`gyo_base_tests`、`engine_tests`、`gyo_collision_tests`、`gyo_math_tests` 到 `gyo_test_support`；probe 到 `gyo_base`。`gyo_base` 沒有對外的邊 |
| 29 檔 syntax-only | 29／29 PASS |
| 稽核（與 R0 比對） | engine 的 `throw_logic` 17→0、`cassert` 2→0、`gyo_assert` 0→29；tests_common 的 `throws_test` 17→0。tests_common 的 Ok／Err 增加（35／23）來自 R0 的 VfsTests 與本批的 ResultTests，R2 一起改寫 |
| 警告 | nodiscard 0；第一方警告與 R0 相同（只有既有的 `ModelTests.cpp:29` braced-scalar-init） |
| pvp | `.cpu`、`.start_phase_record`、`.presentation_cpu` 不改就通過 |

MSVC 上的含逗號條件式與 abort probe 由 PR 的 L1 windows-x64 列驗證。

### MSVC 符合標準的前處理器（R1 追加）

使用者要求在 R1 合併前，讓 MSVC 全專案改用 `/Zc:preprocessor`，消除巨集寫法的 MSVC 特例（方針：統一實作優先於特例）。

- **設定**：`build/cmake/GyoBuild.cmake` 在 `add_subdirectory(third_party)` 之後、`engine` 之前，以 `add_compile_options("$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Zc:preprocessor>" "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:preprocessor>")` 全域加入。用 compiler ID 判斷，所以 clang-cl、Clang、GCC、AppleClang 都不受影響。shader host（`engine/render/shaders/pipeline`）是獨立的 CMake 專案，不繼承這個選項，所以在 `gyo_shader_tool` 的 MSVC 選項中另外加入。repo 內只有這兩個 CMake 專案。
- **防呆**：`Assert.hpp` 開頭在 MSVC（排除 clang-cl）且 `_MSVC_TRADITIONAL` 未定義或非 0 時 `#error`。`Result.hpp` include 它，所以 MSVC 上幾乎所有 GYO 的 TU 都會檢查。`Sha256.hpp` 不受影響。
- **證明選項生效**：`tests/common/base/PreprocessorTests.cpp` 用 `static_assert` 檢查兩件只有符合標準的前處理器才成立的事：把 `__VA_ARGS__` 轉交給另一個巨集後的參數個數，以及 `__VA_OPT__`。MSVC 上另外檢查 `_MSVC_TRADITIONAL == 0`。
- **說明更新**：`Assert.hpp`、`AssertTestSupport.hpp` 的註解與 `error-handling.md` 改為說明所依賴的建置設定；PLAN 2.2 拿掉巨集寫法的限制；`docs/architecture.md` 補上這條全專案規則。

#### 驗收

| 項目 | 結果 |
|---|---|
| 本機 core／test preset | 23／23、50／50 通過 |
| 本機編譯指令（macOS、AppleClang） | `compile_commands.json` 與改動前相比，core 84→85、test 944→945 筆，只多出 `PreprocessorTests.cpp`，其餘 0 筆變動 |
| windows-x64 基準（改動前，#31 第一次 CI） | 警告 138 個、22 組（檔案、代碼），全部在第三方（protobuf、absl、doctest、spirv-cross），加上 pvp 的 2 個 C4456 |
| windows-x64（改動後） | 待 CI |

#### 選項比較（決定時的依據）

| 面向 | (A) GyoBuild 全域（採用） | (B) `gyo_base` 的 INTERFACE |
|---|---|---|
| 涵蓋範圍 | GyoBuild 底下所有目標，不論是否連 Base | 只有連 Base 的目標；`gyo_input` 等不連 Base 的目標會留在傳統模式 |
| Ownership | 工具鏈一致性屬於建置層，與 `-ffp-contract=off` 同類 | 函式庫的使用需求變成決定使用端整個 TU 的編譯模式 |
| Product Removability | 不含產品資訊，刪除產品不需改公共層 | 同上 |

#### Architecture Delta（AGENTS.md §3）

1. **需求來源**：使用者要求（R1 合併前），統一實作優先於特例。
2. **現在的問題**：MSVC 使用傳統前處理器，GYO 的可變參數巨集因此限縮寫法（不轉送 `__VA_ARGS__`、不用 `__VA_OPT__`），註解與文件記載了這個特例；同一份巨集在不同編譯器上展開規則不同。
3. **變化的 boundary**：Build Graph 的全域編譯選項（只在 MSVC 生效）。CMake target 與依賴邊沒有變化。
4. **影響範圍**：MSVC 上所有 GYO 程式碼：engine、`object_fps_pvp`、`ui_editor`、tests、shader host，以及產品從自己目錄加入的相依套件。`third_party` 不受影響。Linux 與 macOS 的編譯指令不變。
5. **依賴方向**：不變。
6. **Ownership**：前處理器模式歸建置層（`GyoBuild.cmake`）；`Assert.hpp` 只宣告並強制它所依賴的前提。
7. **為什麼沒有更小的做法**：維持現狀就是保留特例；改用 (B) 只涵蓋連 Base 的目標，會留下兩種前處理器模式；逐 target 設定會在每個新 target 重複同一個事實。

### 依賴 collision 例外做內容驗證的未編譯檔（R1 之後這些路徑會 abort）

- `apps/object_fps_pvp/src/Gameplay/Player/PlayerController.cpp:43-62`：try 包住 `CanPlaceCharacterBody`（`:46`），catch 在 `:55`、`:59`（後者是 `catch (...)`）。
- `apps/object_fps_pvp/src/Gameplay/Enemy/EnemySystem.cpp:584-620`：try 包住 `CanPlaceCharacterBody`（`:595`）與字串 E 的 `Play`（`:608`），catch 在 `:615`、`:618`。
- 會順帶吞掉例外的 catch-all：`src/Game/GameSession.cpp:366-372`、`570-574`。

這些檔案從未編譯；重新啟用時，內容驗證應移到載入時進行。

### 未結事項

- PR [#31](https://github.com/yojinn-io/GYO-Engine/pull/31) 的 L1 四列結果（MSVC 的前處理器與 abort 行為）。

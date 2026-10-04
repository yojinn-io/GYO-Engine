# Assert／Result 統一：交接

更新：2026-10-04。**R0–R6 完成（#30 `3b9765e`、#31 `9c51b32`、#32 `dedeebd`、#33 `12abe2e`、#34 `88e9641`、#35 `f72bfbd`、#36 `a15c836`）。計劃完成。**

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
| 2026-10-04 | `Result` 的成功建構子沒有預設模板參數，所以非 void 的 `return {};` 編譯不過，大括號值寫成 `return T{...};`（見 `error-handling.md` 的 Result 一節：封住 `{}` 經由 `Result(T&&)` 以預設 T 成功的路徑） | R2 實作 |
| 2026-10-04 | `LoaderRegistry::Register` 的失敗分支全是 API 誤用，改成 Assert 後回傳型別改為 `void`；呼叫端（ui_editor、pvp、tests）的錯誤處理隨之刪除 | R3 實作（依「API 誤用改 Assert」的決定） |
| 2026-10-04 | `AssetErrorCode::UnsupportedRequest` 不再有產生者，刪除；`AssetRecord::ResetToUnloaded`（沒有呼叫者）刪除 | R3 實作 |
| 2026-10-04 | `UiError` 的建構子參數順序為 code、message、source、jsonPointer、detail，維持既有大括號初始化的順序，detail 放最後 | R3 實作 |
| 2026-10-04 | 規則 5 的 detail 以 `Base::CauseDetail(inner, outerDetail)` 產生：外層若有自己的 detail（例如路徑），格式為 `<outer>; <InnerCode>: <inner detail>`。外層 message 保留內層原文，可以在前後加上自己的說明 | R4 實作 |
| 2026-10-04 | ShaderLibrary 的 artifact 讀取失敗改用帶型別的內部例外（`ArtifactReadFailure`，攜帶 `AssetError`），在 `AppendBundle` 邊界依規則 5 轉換。PLAN 原寫「不再經由例外」；但整個 bundle 解析都以例外運作（含 nlohmann `.at()`），只把 `Read` 改成 Result 仍無法去掉例外。重點在保留 code，而內部 throw、邊界轉換符合規則 6 | R4 實作 |
| 2026-10-04 | `Result` 的 E 以 `CodedError` 約束（PLAN 2.4）。`ResultTests` 中為 R2–R3 過渡期寫的 `Result<int, std::string>` 測試改成檢查 string 會被拒絕 | R4 實作 |
| 2026-10-04 | ui_editor 的錯誤型別集中在 `EditorError.hpp`：`FileError`、`CommandLineError`、`CatalogError`、`PreviewError`、`SessionError`（帶 diagnostics）、`EditorError`。只為呼叫端分支或測試斷言的區分建立 code（例如 `SessionErrorCode` 決定結束碼 3／4 與存檔對話框） | R5 實作 |
| 2026-10-04 | `PreviewAdapter::Result` 改名為 `FrameOutput`：它是每一幀的輸出，舊名會遮蔽 ui_editor 的 `Result` 別名而難以閱讀 | R5 實作 |
| 2026-10-04 | `AssetPreviewContext` 的 `Mount`、`Unmount`、第二個 `Initialize` 只有 API 誤用一種失敗，改成 `void` 加 `GYO_ASSERT`；`Texture`／`Text` 在沒有 catalog 時回傳空結果（屬於 Q2），失敗才是 `PreviewError` | R5 實作 |

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

狀態：**完成**。PR [#31](https://github.com/yojinn-io/GYO-Engine/pull/31) 於 2026-10-04 合併為 `9c51b32`，L1 四列與 CI gate 通過（分支 `claude/result-unification-r1`，基準 `3b9765e`）。

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
| windows-x64（改動後，`9f46627`，run 37181489276） | 編譯器為 MSVC 19.51.36260.0（cl，不是 clang-cl）。警告 138 個、22 組，與基準逐項完全相同，沒有 C5105 等新警告，沒有錯誤 |
| 選項確實生效 | `PreprocessorTests.cpp`（含 `_MSVC_TRADITIONAL == 0`、`__VA_ARGS__` 轉送、`__VA_OPT__` 的 `static_assert`）在 MSVC 上編譯通過，`gyo_base_tests` 通過；`Assert.hpp` 的防呆在 engine 的每個 TU 都沒有觸發 |
| abort probe（Windows） | 三種模式都通過，各約 0.03 秒，沒有被 WER 對話框卡住 |
| shader host | `gyo_shader_tool` 加上 `/Zc:preprocessor` 後建置成功 |
| L1 四列 | linux-x64、macos-arm64、macos-x64、windows-x64 全部通過（Windows 上 test preset 50／50） |

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

**連帶影響：Windows 的 compiler cache**。編譯旗標改變後，sccache 的 key 也改變，windows-x64 的「Build the registered graph」步驟從 8 分 52 秒（命中率 85.7%）變成 24 分 23 秒（35.8%）。CI 只在 master 上更新 cache，所以合併前的每次 PR run 都是冷 cache（第三次 run 為 28 分 24 秒）。合併後 master 會以新旗標重建 cache。之後若再改全域編譯旗標，預期會有同樣的一次性成本。

### 依賴 collision 例外做內容驗證的未編譯檔（R1 之後這些路徑會 abort）

- `apps/object_fps_pvp/src/Gameplay/Player/PlayerController.cpp:43-62`：try 包住 `CanPlaceCharacterBody`（`:46`），catch 在 `:55`、`:59`（後者是 `catch (...)`）。
- `apps/object_fps_pvp/src/Gameplay/Enemy/EnemySystem.cpp:584-620`：try 包住 `CanPlaceCharacterBody`（`:595`）與字串 E 的 `Play`（`:608`），catch 在 `:615`、`:618`。
- 會順帶吞掉例外的 catch-all：`src/Game/GameSession.cpp:366-372`、`570-574`。

這些檔案從未編譯；重新啟用時，內容驗證應移到載入時進行。

### 未結事項

- 無。

## R2 Result 的寫法

狀態：**完成**。PR [#32](https://github.com/yojinn-io/GYO-Engine/pull/32) 於 2026-10-04 合併為 `dedeebd`，L1 四列與 CI gate 通過；windows-x64 的警告與 R1 逐項相同（138 個，全為既有），50／50（分支 `claude/result-unification-r2`）。

### 變更

- **`Result.hpp` 重寫**：
  - 新增 `Base::Err<E>`（對應 `std::unexpected`）與 CTAD。
  - 成功值以 conditional explicit 隱式轉換；排除 `Result`、`Err`、`E` 本身，所以漏寫 `Err` 會編譯失敗。
  - `Err<G>` 只要 `E` 可由 `G` 建構就能轉換（字串字面值 → `std::string` E）。
  - `has_value()`、`explicit operator bool`、`value()`、`operator*`、`operator->`、`error()`；讀取未持有的一方走 `GYO_ASSERT`。
  - class-level `[[nodiscard]]`；`static_assert(T != E && !is_reference)`。
  - 移除 static `Ok`／`Err` 與 `ok()`；修正檔頭的「C++17」與 namespace 結尾註解。
- **codemod**（`scripts/r2_result_codemod.py`，已 commit）：66 個檔案。

  | 改寫 | 數量 |
  |---|---|
  | `return X::Ok(v);` → `return v;`（含空的 `Ok()` → `return {};`） | 228 |
  | 運算式中的 `X::Ok(v)` → `X(v)` | 5 |
  | `return X::Err(e);` → `return Base::Err(e);` | 504 |
  | 運算式中的 `X::Err(e)` → `X(Base::Err(e))` | 3 |
  | `IoResultVoid` → `IoResult<void>`（11 個別名宣告刪除） | 62 個 token |
  | `.cpp` 中不再使用的區域 Result 別名刪除 | 52 |

- **手動處理**（codemod 列出的 8 處）：
  - `Err({...})` 5 處改成 `Base::Err(UiError{...})`：`UiDocumentCodec.cpp` 3、`UiSerialization.cpp`、`UiValidation.cpp`。
  - `Ok({...})` 3 處改成具名型別：`UiColor.cpp`、`UiRendererTests.cpp`、`PlayerPresentationTests.cpp`。
  - R0 預計的 lambda（`AssetManager.cpp:56`）不需修改：它只有一個 return，回傳 `AssetHandle` 後由呼叫端隱式轉成 Result。
- **nodiscard**：class-level `[[nodiscard]]` 讓 tests 中 5 處丟棄 `Register` 回傳值的地方產生警告（與 R0 預測相同），改成 `REQUIRE(registry.Register(...))`。
- **`ResultTests.cpp` 重寫**：以 `static_assert` 鎖住建構規則（隱式成功、`Err` 轉換、拒絕裸 `E`、非 void 沒有預設建構、explicit 的 `T`、`Err<const char*>` → `std::string`），並測試 return 寫法、錯誤傳遞、move-only `T`、`operator->`、`Result<bool>`、`Result<optional>`、讀取未持有一方會觸發 Assert。
- **文件**：`error-handling.md` 的 Result 段落寫入建構與查詢規則。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 23／23 通過 |
| test preset | 50／50 通過 |
| 稽核 | `static_ok`、`static_err`、`io_result_void` 全部為 0。`result_ok_call` 剩 1 處，是 `AssetRecord.hpp:76` 的 `Error::ok()`（R3 移除），不是 Result |
| Err 數量守恆 | 起點 512（R0、R1 的測試也算在內）。engine 的 `Base::Err(` 489 處，與改寫前 489 相同；tests_common 20 處；另外 3 處在重寫的 `ResultTests.cpp`，那裡以 using 宣告寫 `Err(`，不在稽核 pattern 內。合計 512 |
| nodiscard 警告 | 0（修正上述 5 處後） |
| 第一方警告 | 與 R1 相同（只有既有的 `ModelTests.cpp:29` braced-scalar-init） |
| 依賴圖 | 與 R1 相同 |
| 29 檔 syntax-only | 29／29 PASS |

### 觀察（未處理）

- `return std::move(x);` 有 41 處，來自原本的 `Ok(std::move(x))`。C++20 對區域變數與參數會隱式 move，所以多數可以寫成 `return x;`；但 x 若是成員，拿掉 `std::move` 會變成複製，無法安全地自動判斷，因此維持原樣。

### 未結事項

- 無。

## R3 Error 語意與 asset

狀態：**完成**。PR [#33](https://github.com/yojinn-io/GYO-Engine/pull/33) 於 2026-10-04 合併為 `12abe2e`，L1 四列與 CI gate 通過；windows-x64 的警告只多 1 個 doctest C5285（新增的 `ErrorTests.cpp` 這個 TU），其餘與 R2 相同（分支 `claude/result-unification-r3`）。

### 變更

- **`Error.hpp`**：
  - `Base::Error<Code>`：`code`、`message`、`detail` 維持 public；建構子改為 private，`Make` 是唯一入口，收到 0 code 時 `GYO_ASSERT`；不可預設建構。移除 `ok`、`operator bool`、`None`、`Clear`、`CodeValue`、`Message`、`Detail`。
  - 新增 concept `Base::CodedError`：enum 的 `code`、`message`、`detail`、經由 ADL 的 `ToString(code)`，且不可預設建構。
  - 新增 `Base::Describe(error)`：`<CodeName>: <message>`，有 detail 時接 ` (<detail>)`。
- **code enum**（Asset、Io、Render、Text、Ui、SdlPlatform、SdlRenderer、SdlInput 共 8 個）：移除 `None = 0`，第一個 enumerator 明寫 `= 1`，每個都有 `ToString` 與 `static_assert(Base::CodedError<...>)`。刪除 `AssetError.hpp` 被註解掉的舊 struct。
- **別名收斂**：`AssetError` 只在 `AssetError.hpp`（`Engine::Asset`），`IoError` 與 `IoResult` 只在 `IoError.hpp`（`Engine::IO`）。刪除 43 個重複宣告；內層 namespace 經由名稱查找取得外層的別名。被迫的修改：`Loading::AssetError`、`FS::IoResult` 等限定寫法。
- **`UiError`**：新增 `detail` 與建構子，移除預設建構；既有的大括號初始化照樣可用。
- **`AssetRecord`**：`error`、`failedError` 改成 `std::optional<AssetError>`；`ErrorFor` 與 `AssetManager::GetError` 改成以值回傳 `std::optional<AssetError>`，不再回傳指向 record 內部的指標；`KeepOldIfAny` 的語意不變。
- **API 誤用改用 Assert**：
  - `LoaderRegistry::Register`：null loader、type 0、重複註冊改成 `GYO_ASSERT`，回傳型別改為 `void`。被迫的修改：ui_editor 的 `AssetPreviewContext`、pvp 的 `PvpApplication`、未編譯的 `ObjectFpsApplication`（相容修改）、`PlayerPresentationTests` 與 tests/common 的 5 處。
  - `AssetManager::Load`：保留欄位 `priority`／`keepAliveFramesOverride` 非預設時改成 `GYO_ASSERT`；`AssetManagerTests` 的 3 個 subcase 改用 `GYO_CHECK_ASSERTS`，仍檢查沒有任何副作用；`docs/architecture.md` 同步更新。
- **`AssetCatalog.cpp:92-98`**：保留 resolver 傳回的 code（`PathEscapesRoot` 不再被改成 `InvalidPath`），新增測試鎖住。
- **其他**：`ShaderLibraryTests.cpp:27` 的 `static_cast<AssetErrorCode>(1)` 改成具名的 `CatalogNotFound`（保留原本的數值意義）；`AssetManagerTests.cpp:609` 的 `== nullptr` 改成 `CHECK_FALSE`；修正錯誤的 namespace 結尾註解（`MountTable.hpp`、`MountPoint.hpp`、`Vfs.hpp`、`FileAllCommon.hpp`、`Span.hpp`）。其中 `MountTable.hpp` 原本列在 R2，當時漏掉，在本批補上。
- **測試**：新增 `ErrorTests.cpp`（`CodedError` 的成立與不成立條件、`Make`、0 code 觸發 Assert、`Describe` 的格式）。
- **文件**：`error-handling.md` 寫入 `CodedError`、`Describe`、0 code 與 optional 的規則；`docs/architecture.md` 的保留欄位敘述。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 23／23 通過 |
| test preset | 50／50 通過 |
| characterization（R0） | Vfs 與 KeepOldIfAny 的測試沒有修改就通過（`GetError` 改成 optional 也照樣編譯） |
| 稽核 | `error_code_none`、`error_code_value_init`、`error_getters`、`result_ok_call` 為 0；`alias_asset_error`、`alias_io_error`、`alias_io_result` 各 1 |
| 第一方警告 | 與 R2 相同 |
| 依賴圖 | 與 R2 相同 |
| 29 檔 syntax-only | 29／29 PASS |

### 未結事項

- 無。

## R4 Model、ui／render 的轉換與 API 誤用

狀態：**完成**。PR [#34](https://github.com/yojinn-io/GYO-Engine/pull/34) 於 2026-10-04 合併為 `88e9641`，L1 四列與 CI gate 通過；windows-x64 的警告與 R3 逐項相同（分支 `claude/result-unification-r4`）。

### 變更

- **`ModelError`**（新 header `model/ModelError.hpp`）：`InvalidModel`、`InvalidArgument`、`IncompatibleAnimation`，以及 `ModelResult<T>`。Model API 的 8 個宣告與 `Animation.cpp`（27 處）、`AnimationTransfer.cpp`（11 處）改用它，訊息逐字保留。轉移中巢狀的 `ValidateModel` 錯誤在同模組內保留 code。
- **`ModelRendererError`**：`InvalidModel`、`InvalidArgument`、`ResourceCreationFailed`、`SubmissionFailed`。Model 與 Render 的錯誤依規則 5 轉換（包括 R0 補列的 `:113` 原樣傳遞），null model 與 null resource 改用 `GYO_ASSERT`。
- **`Result` 以 `CodedError` 約束 E**；`Base::CauseDetail` 新增於 `Error.hpp`；`GYO_UNREACHABLE` 新增於 `Assert.hpp`。
- **跨模組轉換**：
  - UiRenderer 的 6 處（Asset、Render、Text → `UiError`）改成在 detail 保留內層 code。
  - ShaderLibrary 的 artifact 讀取失敗保留 `AssetError` 的 code（見決策紀錄）。
  - `UfbxModelLoader` 的 `ValidateModel` 不再經由例外，直接轉成 `AssetError::DecodeFailed`，detail 為 `<path>; <ModelCode>`。
- **API 誤用改用 Assert**：
  - UiRuntime 10 處（no active canvas 3、not initialized 3、null document 1、evaluated 或 captured element 不在 document 中 2、不可到達的 text source kind 1，最後這處改用 `GYO_UNREACHABLE`）。
  - `UiRuntime.cpp` 的 `placeholders.at(name)` 改成先 `GYO_ASSERT` 再存取。
  - UiRenderer 3 處（`maximumCachedTextRuns == 0`、not initialized、point size）。
  - `Renderer::Render` 的 not initialized。
  - 保留為 Result 的 `RuntimeState`：viewport 3 處、slider 寬度、負尺寸、item_field（呼叫端資料或 document 內容）。
- **被迫的修改**：
  - `ModelTests` 4 處、`ModelRendererTests` 1 處改成 `.error().message`，並補上 code 斷言。
  - pvp 已編譯檔 13 處與 `Require` helper（改成接受任何 E 的 template）。
  - 29 個未編譯檔中的 17 處（相容修改，與 R0 盤點的數量相同）。
- **測試**：`UiRendererTests` 的「before initialization」改成 `GYO_CHECK_ASSERTS`。新增：
  - ModelRenderer 保留 Model／Render code、null 輸入的 Assert。
  - Renderer、UiRuntime 的 API 誤用。
  - `CauseDetail` 的格式。
  - `GYO_UNREACHABLE`。
  - `ResultTests` 中 string E 被拒絕。
- **文件**：`error-handling.md` 寫入 `GYO_UNREACHABLE`、`CauseDetail`、`CodedError` 約束。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 23／23 通過 |
| test preset | 50／50 通過（含 `UfbxModelTests.cpp:172-173` 沒有修改就通過，外層 message 保留內層原文） |
| 稽核 | `string_error_type` 為 0（Apps 也是 0）；engine 的 `gyo_assert` 從 29 增為 50 |
| 第一方警告 | 與 R3 相同 |
| 依賴圖 | 只多出測試專用的 `gyo_render_tests -> gyo_test_support` |
| 29 檔 syntax-only | 29／29 PASS（17 處相容修改後） |

### 未結事項

- 無。

## R5 ui_editor

狀態：**完成**。PR [#35](https://github.com/yojinn-io/GYO-Engine/pull/35) 於 2026-10-04 合併為 `f72bfbd`，L1 四列與 CI gate 通過；windows-x64 的警告與 R4 逐項相同（分支 `claude/result-unification-r5`）。

### 變更

- **錯誤型別**：新增 `tools/ui_editor/include/gyo/ui_editor/EditorError.hpp`（見決策紀錄）與 `Result<T, E>` 別名。所有 E 都有 `static_assert(CodedError)`；engine 錯誤經 `Base::CauseDetail` 保留在 detail。
- **手寫結果型別改用 Result**：
  - `CommandLineResult` → `Result<CommandLineOptions, CommandLineError>`。
  - `TextFileResult` → `Result<std::string, FileError>`。
  - `FileOperationResult` → `Result<void, FileError>`。
  - `ProbeFileStamp`（原本是 optional 加 out-param）→ `Result<FileStamp, FileError>`。
  - `HasExternalModification`（原本是 bool 加 out-param，實際上有三種狀態）→ `Result<bool, FileError>`。
  - `SessionResult`／`SaveFailure` → `Result<SessionReport, SessionError>`，成功與失敗都帶 diagnostics。
- **out-param 改成回傳值**：`ReadOnlyAssetCatalog::Mount`／`MountRoot`、`AssetPreviewContext::Initialize`／`Texture`／`Text`、`EditorApp::Initialize`／`MountCatalog`、內部的 `Upload`。
- **Programmer Error 改用 Assert**：
  - `AssetPreviewContext.cpp` 的 `assert` 改成 `GYO_ASSERT`，刪除後面已經到不了的 if。
  - 第二個 `Initialize`、`Mount`、`Unmount`、`BeginFrame`、在 frame 外呼叫 `Texture`／`Text`。
  - `DocumentSession::Restore` 與 `UiDocumentBridge::Parse` 解析內部產生的 JSON，改用不丟例外的 parse 加 `GYO_ASSERT`，不再 try/catch。
- **`std::filesystem::absolute`**：會丟例外的版本改成 `FileService` 的 `AbsolutePath`（使用 `std::error_code`，失敗時保留原路徑，由後續的檔案操作回報）。`DocumentSession` 4 處、`ReadOnlyAssetCatalog` 3 處；`FileService` 自己的 `AbsoluteNormalized` 也改用它。
- **呈現**：`Describe`、`DescribeError`、ui_editor 的 `LogError`、`tests/common/runtime_sdl/main.cpp` 的 `LogError` 這 4 份 helper 改用 `Base::Describe`。Main 與 GUI 狀態列顯示 `<Code>: <message> (<detail>)`，codec diagnostics 也帶 code。
- **`PreviewAdapter`**：`Result` 改名為 `FrameOutput`，`error` 改成 `std::optional<Engine::Ui::UiError>`，保留 code；資產載入失敗時照舊畫 placeholder。
- **測試**：
  - `EditorCoreTests`、`PreviewAssetTests` 改用新 API。
  - 在 frame 中 Mount／Unmount 改成驗證會觸發 Assert：在測試本地安裝丟例外的 handler，因為這兩個測試不是 doctest。
  - `EditorCliTests.cmake` 新增結束碼 2 的檢查（原本沒有覆蓋）。
- **R4 遺留的修正**：`ResultTests.cpp` 的 `ToString` 只在 concept 中被引用，clang 會發出 `-Wunneeded-internal-declaration`。R4 的驗收漏看了這個警告，本批補上一個使用 `Describe` 的斷言。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 23／23 通過 |
| test preset | 50／50 通過（含 `gyo_ui_editor.core`、`.preview`、`.validate`、`.content_cli`、`.acceptance_contract`） |
| 結束碼 | 0、2、3、4 不變：`content_cli`（含新增的 2）、`validate_cli.py`（`--help` 0、valid 0、invalid 4），手動確認不存在的檔案為 3 |
| 稽核 | ui_editor 的 `string_error_out_param` 23→0、`cassert` 2→0；範圍內沒有會丟例外的 `std::filesystem::absolute` |
| 第一方警告 | 0（修正上述 R4 遺留後） |
| 依賴圖 | 與 R4 相同 |
| 29 檔 syntax-only | 29／29 PASS |

### 未結事項

- 無。

## R6 收尾

狀態：**完成**。PR [#36](https://github.com/yojinn-io/GYO-Engine/pull/36) 於 2026-10-04 合併為 `a15c836`，L1 四列與 CI gate 通過；windows-x64 的警告與 R5 逐項相同（139 個），50／50（分支 `claude/result-unification-r6`，基準 `f72bfbd`）。

### 變更

- `docs/architecture/error-handling.md` 定稿：
  - Result 與 Error types 分成兩節。
  - 規則 2 的例子改成 `RenderQueue::Submit`，並連到 wide contract 清單，註明其中的狀態檢查後來已依規則 3 改成 Assert。
  - 規則 4 改成「同模組內保留 code 與 detail，可以加上脈絡」。
  - 規則 5 寫出完整格式 `[<outer detail>; ]<InnerCode>[: <inner detail>]`。
  - 補上 shader host 自行設定 `/Zc:preprocessor`。
  - Status 的那一句是在 R5 修改的。
- PLAN 開頭加入「實作與本文件的差異」彙整，內文維持核准時的版本。
- `docs/architecture.md` 的 Assert／Result 段落補上 `/Zc:preprocessor`。
- 新增 [inactive_products.md](inactive_products.md)：未啟用產品的遷移清單。
- 本節：全範圍稽核、Apps 剩餘項目、Architecture Report。
- 最終審查（1 個唯讀 agent）指出的程式碼修正：
  - `tests/common/render/sdl_gpu` 的 smoke 測試有 12 處自行輸出 `.error().message`，改用 `Base::Describe`（規則 7）。
  - `AssetCatalog` 轉換 resolver 錯誤時把 detail（路徑）換成了 entry id，改成 `<id>; <resolver detail>`（規則 4），並放寬 R3 測試的 detail 斷言。
  - ui_editor 參數錯誤的輸出也改用 `Describe`。

### 全範圍稽核（Apps 以外，`f72bfbd`）

| pattern | engine | tools_ui_editor | tests_common | tests_ui_editor |
|---|---|---|---|---|
| 靜態 `Ok`（R0 → R6） | 199 → 0 | 0 → 0 | 28 → 0 | 1 → 0 |
| 靜態 `Err` | 489 → 0 | 0 → 0 | 9 → 0 | 0 → 0 |
| `Result::ok()` 與 `Error::ok()` | 1 → 0 | — | — | — |
| `IoResultVoid` | 120 → 0 | — | — | — |
| `ErrorCode::None` | 6 → 0 | — | — | — |
| `AssetError`／`IoError`／`IoResult` 別名 | 17／15／11 → 1／1／1 | — | — | — |
| `Result<…, std::string>` | 21 → 0 | — | — | — |
| `throw std::invalid_argument` 等 | 17 → 0 | 0 | 0 | 0 |
| `CHECK_THROWS*` | — | — | 17 → 0 | 0 |
| `assert(`／`<cassert>` | 2 → 0 | 2 → 0 | 0 | 0 |
| `std::string& error` out-param | 0 | 23 → 0 | 0 | 0 |
| `GYO_ASSERT` | 0 → 50 | 0 → 9 | 0 → 8 | 0 |

- engine 剩下的 `throw` 只在 UiDocumentCodec、ShaderLibrary、UfbxModelLoader 內部使用，都在公開邊界 catch 並轉成 Result（規則 6）。範圍內沒有 `catch (...)`。
- `RuntimeState` 還剩 6 處，都是資料錯誤（viewport 3、slider 寬度、負尺寸、item_field），保留 Result。
- 資料驗證型 API 與 SdlGpu 的例外依處理規則 2、3 保留 Result。
- 依賴圖：與 R0 相比只多出 `gyo_base` 相關的邊（R1）與測試專用的 `gyo_test_support` 邊（R1、R4）。

### Apps 剩餘項目（之後順手修改時的參考，不排批次）

`object_fps_pvp`（含 tests 與 acceptance）中，下列寫法仍然存在。新程式碼依 `error-handling.md` 撰寫；舊程式碼在因其他理由修改時再改。

| pattern | 數量 | 集中的檔案 |
|---|---|---|
| `throw std::invalid_argument` 等 | 66 | `Collision/GridCollision.cpp` 11、`Pvp/IpcHost.cpp` 7、`Collision/CombatCollision.cpp` 7、`Pvp/MatchRuntimeHost.cpp` 5、`Gameplay/Enemy/EnemySystem.cpp` 5 |
| `std::string& error` out-param | 126 | `Game/GameSession.cpp` 12、`Pvp/PlayerPresentation.cpp` 與其 header 各 8、`App/ObjectFpsPresentation.cpp` 8 |
| `CHECK_THROWS_AS` | 9 | `PvpMatchTests.cpp` 5、`ShotQueryTests.cpp` 4 |

依 R0 分類表，pvp 的 throw 多數是 Q0（例如 `ShotQuery.cpp` 在 admission 驗證之後的重新檢查），應改成 `GYO_ASSERT`；`Arena::Load` 這類從檔案讀取的應改成 Result。改動 authority 相關程式碼時，需要先用 digest 鎖住行為。

### Architecture Report

- **Architecture Delta**：
  - 新增概念「Assert = Programmer Error、Result = Runtime Error」與 7 條處理規則，由 `docs/architecture/error-handling.md` 擁有。
  - `engine/base` 從 `GYO::Engine` 的 include 目錄升格為 leaf target `GYO::Base`，位在 Math 之下。
  - MSVC 全專案改用 `/Zc:preprocessor`（建置層的全域選項）。
- **新的依賴邊**：`engine → gyo_base`、`gyo_math → gyo_base`、`gyo_collision → gyo_base`；測試專用的 `gyo_test_support → gyo_base`、`doctest`、5 個測試執行檔到 `gyo_test_support`，以及 abort probe 與 `gyo_base_tests` 到 `gyo_base`。沒有循環，產品的依賴邊沒有變。
- **Ownership**：
  - Base 擁有 Assert、Result、Error、`CodedError`、`Describe`、`CauseDetail`。
  - 各模組擁有自己的 code enum 與邊界轉換（新增 `ModelError`、`ModelRendererError`）。
  - ui_editor 擁有 `EditorError.hpp`。
  - 前處理器模式歸建置層。
- **Product Boundary**：沒有變化。Apps 只做被迫的修改（R2–R4）。公共層沒有加入產品名稱。未啟用產品的破損另列清單。
- **Data Contract**：沒有變化。error code 的數值不是資料契約（R0 確認沒有持久化、序列化或上 wire），wire 字串與檔案格式都沒有改動。
- **行為變更**：
  - engine 與 ui_editor 的 Programmer Error 從丟例外或回傳 Result 改成 Assert 後 abort，release 也生效。
  - `Math::Clamp` 的前提檢查原本在 RelWithDebInfo 中從未生效，現在永遠生效。
  - CLI 的錯誤訊息改成 `<Code>: <message> (<detail>)`（參數錯誤也是），結束碼不變。
- **發現的 Code Smell 與之後可以考慮的事**：
  - `return std::move(x);` 41 處（來自原本的 `Ok(std::move(x))`）。區域變數可以改成 `return x;`。
  - `PreviewAdapter` 在資產載入失敗時只畫 placeholder，沒有回報錯誤。這是既有行為。
  - Apps 的剩餘項目（見上表）。pvp 的 Q0 檢查改成 Assert 時，需要先用 digest 鎖住 authority 行為。
  - 只在 debug 生效的 `GYO_DEBUG_ASSERT` 尚未加入，等第一個昂貴檢查出現時再加。
  - 測試的 assertion handler 是全域狀態，目前所有測試都是單執行緒；如果日後出現平行執行的測試，需要重新檢查。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 23／23 通過（R0 的 19 個加上 `gyo_base_tests` 與 3 個 abort probe） |
| test preset | 50／50 通過 |
| 依賴圖（與 R0 比對） | 只多出 13 條邊：`engine`、`gyo_math`、`gyo_collision`、`gyo_assert_abort_probe` 到 `gyo_base`；`gyo_test_support` 到 `gyo_base` 與 doctest；`gyo_base_tests` 到 doctest、`gyo_base` 與 `gyo_test_support`；`engine_tests`、`gyo_collision_tests`、`gyo_math_tests`、`gyo_render_tests` 到 `gyo_test_support` |
| 29 檔 syntax-only | 29／29 PASS |
| 第一方警告 | 0 |
| 未啟用產品 | 本計劃新增 82 筆錯誤、1 個新的失敗 TU，全部歸類於 [inactive_products.md](inactive_products.md) |

### 最終審查

1 個唯讀 agent 交叉比對文件與程式碼，指出 6 項應修正、7 項次要，以及 PLAN 中需要加註的過時內容，已全部處理：
- **程式碼**：見「變更」的最後一項。
- **事實錯誤**：規則 2 的 `ClipSprite` 例子；`inactive_products.md` 的 TU 數（應為 20 個，其中 19 個原本就失敗）。
- **說法不精確**：規則 4、5、前處理器的說明。
- **紀錄問題**：HANDOFF 開頭的殘句、引用了 PLAN 沒有的句子、把 Status 的修改算在 R6。
- **缺漏**：R6 的 dev log，以及 R5 dev log 中過時的一行。

### 未結事項

- 範圍外、留待日後：Apps 剩餘項目（上表）、未啟用產品的遷移（[inactive_products.md](inactive_products.md)），以及 R1 記錄的 pvp 未編譯檔依賴 collision 例外做內容驗證（見「R1 概念文件與 Assert」）。

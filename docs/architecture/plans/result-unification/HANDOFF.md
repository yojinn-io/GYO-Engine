# Assert／Result 統一：交接

更新：2026-10-04。**R0 本機驗收完成，PR [#30](https://github.com/yojinn-io/GYO-Engine/pull/30) 待 CI。**

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

## R0 任務校正

狀態：**本機驗收完成**（2026-10-04，分支 `claude/result-unification-r0`，基準 `eeebc1e`）。
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

- PR [#30](https://github.com/yojinn-io/GYO-Engine/pull/30) 的 L1 四列結果。
- `AssetError.hpp:42-67` 被註解掉的舊 struct 會讓 `error_code_none` 多算 3 處；R3 刪除它。

# GYO Assert／Result 統一計劃（核准版）

核准：2026-10-04。Owner：Engine（`engine/base`，R1 起為 `GYO::Base`）。進度見 [README](README.md)，紀錄見 [HANDOFF](HANDOFF.md)。
本文件是核准版計劃經 R0 校正（以 `eeebc1e` 重新盤點與對抗檢查）後的版本。

## Context

### 目標（使用者 2026-10-04）

1. **可讀性**：讓程式碼更清楚地表達邏輯。
2. **風格統一**：至少在 Apps 以外（engine、tools、tests/common）做到一致。
3. **統一的處理邏輯**：失敗怎麼分類、怎麼傳遞、怎麼跨模組轉換、怎麼呈現給人看，全部只有一套規則。

### 現況（`eeebc1e`）

GYO 從 2026-01（`17fea39` → `3040537`）起就有 `Base::Result<T, E>` 與 `Base::Error<Code>`，但這套慣例從未寫成文件。

- **可讀性**
  - 前提條件寫成 `throw std::invalid_argument(...)`：Collision 15 處、`FixedTickRuntime` 2 處。
  - 錯誤傳遞冗長：靜態 `Ok`／`Err` 共 727 處、58 檔，其中 Err 498 處；另有 58 個區域 `using Result = …` 別名。
- **風格不一致**
  - model 與 ModelRenderer 用 `std::string` 當 E（21 行）。
  - ui 自訂了 `UiError`，而且沒有 `detail` 欄位。
  - ui_editor 有 7 個手寫結果型別、23 行 `std::string& error`，以及 `assert`。
  - 別名重複宣告：`AssetError` 17 處、`IoError` 15 處、`IoResult`／`IoResultVoid` 各 11 處。
- **處理邏輯不一致**
  - 跨模組轉換時 code 會遺失：UiRenderer 6 處、ModelRenderer 5 處、ShaderLibrary、`AssetCatalog.cpp:92-98`。
  - 錯誤描述 helper 在範圍內有 4 份（ui_editor 的 `Describe`、`DescribeError`、`LogError`，以及 `tests/common/runtime_sdl/main.cpp:29` 的 `LogError`），Apps 另有 2 份。
  - API 誤用以 Result 回傳：UiRuntime 10 處、UiRenderer 3 處、`LoaderRegistry::Register` 3 處、AssetManager 的 `UnsupportedRequest`、Renderer 的「not initialized」。
  - `Error` 的 `operator bool` 意思和 Result 相反；`None`／`ok()` 讓 Error 可以表示「沒有錯誤」。
  - engine 沒有 Assert 設施；#28 在 `Math::Clamp` 用了 `<cassert>`，而所有 preset 都是 RelWithDebInfo，所以這個檢查在 CI 中從未生效。

實際依 error code 分支的正式程式碼只有一處：`Vfs.hpp` 的 10 個 `IsNotFound` 判斷（讀取 overlay 遇到 NotFound 就改查下一個 mount）。

---

## 1. 核心概念與處理規則

```text
Assert = Programmer Error
Result = Runtime Error
```

| | Programmer Error | Runtime Error |
|---|---|---|
| 意思 | 程式寫錯了：前提不成立、內部不變式被破壞、API 誤用、不可到達的分支 | 程式正確，但外部資料或環境導致失敗 |
| 機制 | `GYO_ASSERT`、`GYO_UNREACHABLE` | `Result<T, E>`，E 符合 `CodedError` |
| 呼叫端 | 不處理；正確做法是修 bug | 必須處理（`[[nodiscard]]`） |
| 發生時 | 呼叫 handler，預設 log 後 abort，release 也生效 | 回傳 `Err(E)`，帶模組自己的 code |
| 給人看 | handler 輸出條件式與位置 | `Base::Describe(error)` |
| 測試 | `GYO_CHECK_ASSERTS(expr)` | 斷言 `error().code` |

**不是錯誤的情況**：只是「沒有」而且原因唯一 → `std::optional`；多筆問題或警告 → diagnostics 資料；遊戲規則下的正常結果 → 領域型別。

**處理規則**

1. **判斷流程**：Q0 發生了就代表程式寫錯 → Assert；Q1 外部資料或環境導致失敗 → 在第一個驗證點回傳 Result，下游的重複檢查回到 Q0；Q2 只是「沒有」→ optional；Q3 多筆問題 → diagnostics；Q4 遊戲結果 → 領域型別。
2. **資料驗證型 API（wide contract）**：本身就是外部資料的第一個驗證點（RenderQueue、ClipSprite、Model 的動畫 API 等），輸入錯誤屬於 Runtime Error，回傳 Result。清單見 [baseline/wide_contract_sites.tsv](baseline/wide_contract_sites.tsv)。
3. **API 誤用一律是 Programmer Error**，改用 Assert（使用者 2026-10-04 決定）。唯一的例外是 SdlGpu device 的 `WrongThread`、`InvalidHandle` 與 frame 狀態錯誤：它們是 GPU 後端對外的契約，而且 `WrongThread` 的測試在另一個執行緒上執行，維持 Result 並寫進文件。
4. **傳遞**：同模組內原樣往上傳，寫成 `return Base::Err(std::move(r).error());`。
5. **跨模組轉換**：code 由外層模組決定；外層 message 保留內層 message 原文；內層的 code 名稱（以及原本的 detail）放進外層的 `detail`，格式為 `<InnerCode>: <inner detail>`。code 不得在沒有記錄的情況下遺失。
6. **例外**：例外不是第三種錯誤機制。engine 與 tools 的公開 API 不以例外表示錯誤；內部的 throw 必須在模組公開邊界轉成 Result。邊界的 catch 只接 `std::exception`，不得用 `catch (...)`，否則會吞掉測試中的 `AssertionFailure`。會發生 Runtime Error 的執行緒入口要自己 catch。
7. **呈現**：一律用 `Base::Describe(error)`，不再各自寫格式化函式。

## 2. 規格

### 2.1 `GYO::Base` 與建置

- 新增 `engine/base/CMakeLists.txt`，定義 `gyo_base`：INTERFACE、header-only，只依賴標準函式庫，alias `GYO::Base`。`engine/CMakeLists.txt` 在 math 之前先 `add_subdirectory(base)`。
- 依賴方向：`GYO::Base` 是最底層的 leaf，比 Math 更低。
  - `GYO::Math` 以 INTERFACE 連 `GYO::Base`。
  - `engine` 以 PUBLIC 連 `GYO::Base`，取代 `engine/CMakeLists.txt:9-10` 的 `base/include` include 目錄。
  - `GYO::Collision` 經由 Math 取得 Base；若直接 include Base，就明確連結。
- `Sha256.hpp` 不 include Assert、Result、Error，因為 shader host 直接 include 它（`pipeline/CMakeLists.txt:21-22`），CI 快取鍵也 hash 這個檔。
- 測試支援：INTERFACE target `gyo_test_support`（`tests/common/support/`），連 `GYO::Base` 與 doctest。

### 2.2 Assert（`engine/base/include/engine/base/Assert.hpp`）

- `GYO_ASSERT(cond)`：一律求值，release 也生效。`GYO_UNREACHABLE()` 跟著第一個使用者在 R4 加入（`UiRuntime.cpp:257`）。
- `AssertionFailure { expression, std::source_location }` 不繼承 `std::exception`。
- `SetAssertionHandler(handler)` 回傳前一個 handler。預設 handler 輸出一行到 stderr 並 flush，然後 abort。重入用 RAII 的 thread_local guard 判斷，堆疊展開時也會復原。
- handler 存放在 header 的 `inline constinit std::atomic`；所有 GYO 函式庫都是 STATIC，整個程式只有一份。
- 全專案的 MSVC 使用符合標準的前處理器（`/Zc:preprocessor`，由 `build/cmake/GyoBuild.cmake` 全域設定；shader host 在自己的 target 設定），`Assert.hpp` 在傳統前處理器下以 `#error` 停止，所以巨集可以轉送 `__VA_ARGS__`、使用 `__VA_OPT__`（2026-10-04，R1 追加）。條件式不得有副作用，只求值一次。
- noexcept（Lakos rule）：含 Assert 的函式不標 `noexcept`；解構子與執行緒入口裡不放 Assert。`constexpr` 函式可以使用：常數求值時條件成立就照常運作，條件不成立會變成編譯錯誤。
- 適用範圍是整個 engine，包括 Math（2026-10-04 更正）。Math 中其他「由呼叫端保證」的前提不會一律加上 Assert，等有實際需要時再加。
- 只在 debug 生效的 `GYO_DEBUG_ASSERT` 等第一個昂貴檢查出現時再加。
- C++26 對應：`GYO_ASSERT` → `contract_assert`；`SetAssertionHandler` → 可替換的 violation handler；`GYO_UNREACHABLE` → `contract_assert(false)` 加 `std::unreachable()`。
- 測試：`ScopedAssertionHandler`（RAII，只在 scope 內改成 throw `AssertionFailure`）與 `GYO_CHECK_ASSERTS(expr)`，並用 doctest 的 exception translator 讓報告可讀。測試支援與 abort probe 都要在 MSVC 上停用 `_CALL_REPORTFAULT`。預設 abort 用 probe 執行檔驗證（`cmake -P` 加 `execute_process`，TIMEOUT 30，比照 `tests/ui_editor/EditorCliTests.cmake`）。

### 2.3 Result（`Result.hpp`）

```cpp
Base::Result<Path, IoError> Parse(...) {
    auto n = Normalize(raw);
    if (!n) return Base::Err(std::move(n).error());  // 失敗
    return Path::FromNormalized(std::move(*n));      // 成功：直接回傳值
}
Base::Result<void, IoError> Close() { ...; return {}; }  // void 版的成功
```

- 新增 `Base::Err<E>`，對應 `std::unexpected`，並提供 CTAD。
- `Result` 可以從任何 `Err<G>` 建構，只要 `std::is_constructible_v<E, G>`（同 `std::expected`）。這是必要的：R4 之前 model 的 E 仍是 `std::string`，有 42 處只傳字串字面值，CTAD 會推導成 `Err<const char*>`。
- 成功值可以隱式轉換（conditional explicit）。拒絕 `remove_cvref<U> == E`，讓漏寫 `Err` 的 `return error;` 編譯失敗；並加上 `static_assert(T != E && !is_reference<T>)`。
- 存取：`has_value()`、`explicit operator bool`、`value()`、`operator*`、`operator->`、`error()`。存取狀態不符的一方屬於 Programmer Error，走 `GYO_ASSERT`，內部改用 `get_if`。這些存取子不標 `noexcept`（包括目前 `Result.hpp:56` 的 `Result<void,E>::value() const noexcept`）。
- class-level `[[nodiscard]]`。
- 移除 static `Ok`／`Err`、`ok()`、`IoResultVoid`（改用 `IoResult<void>`），以及不再需要的區域 `using Result = …`。修正 `Result.hpp:8` 的「C++17」與 `:72`、`MountTable.hpp:165` 的 namespace 結尾註解。
- 不可移動的 T 改用 `Result<std::unique_ptr<T>, E>`。

### 2.4 Error 與 E 慣例（`Error.hpp`）

- `Base::Error<Code>`：`code`、`message`、`detail` 維持 public；`Make` 保留為唯一入口（engine 有 142 處）；不可預設建構。移除 `ok`、`operator bool`、`None()`、`Clear`、`CodeValue`、`Message`、`Detail`（除了 `AssetRecord.hpp:76` 的 `ok()` 之外沒有呼叫者）。
- concept `CodedError<E>`：`code` 是 enum、`message` 與 `detail` 是 `std::string`，且不可預設建構。每個 E 旁加 `static_assert`；R4 起以它約束 Result 的 E。
- `UiError`：新增 `detail` 欄位，讓處理規則 5 與 `Describe` 可以統一套用；保留 `source`、`jsonPointer`；新增建構子並移除預設建構（`UiDocumentCodec.cpp:25` 的成員經由 explicit 建構子初始化，不受影響）。
- `Base::Describe(const E&)`：只接受 `CodedError`，輸出 `<CodeName>: <message>`，若有 detail 再接 ` (<detail>)`。每個 code enum 提供 `ToString`；目前只有 Asset 與 Io 有，而且沒有呼叫者，其他 enum 要補上。
- code enum：移除 `None = 0`，第一個 enumerator 明寫 `= 1`，所以現有 code 的數值都不變（`ShaderLibraryTests.cpp:27` 的 `static_cast<AssetErrorCode>(1)` 仍然有效，R3 會順手改成具名的 enumerator）。另加稽核：`ErrorCode{}` 必須為 0 處。
- 別名：每個模組只在 enum 旁宣告一次。`AssetError`（放在 `Engine::Asset`）、`IoError`、`IoResult` 各剩 1 處。
- `AssetRecord`：`error`／`failedError` 改成 `std::optional<AssetError>`；`GetError` 改成以值回傳 `std::optional<AssetError>`；`KeepOldIfAny` 的語意不變；刪除沒有呼叫者的 `ResetToUnloaded`。
- 新增的 E：`ModelError`、`ModelRendererError`；ui_editor 的 `FileError`、`CommandLineError`、`SessionError`（`SessionError` 帶 diagnostics）。只為有呼叫端分支或測試斷言的區分建立 code，其餘歸一個泛用 code。

## 3. 範圍

- **完整統一**：`engine/`（建置期工具 `engine/render/shaders/pipeline` 豁免）、`tools/ui_editor`、`tests/common`、`tests/ui_editor`。
- **Apps**（`apps/object_fps_pvp`、`tests/object_fps_pvp`、`build/acceptance/object_fps_pvp`）：只做被迫的修改。
  - R2：`tests/object_fps_pvp/PlayerPresentationTests.cpp:63` 一處。
  - R4：已編譯檔中 Model 字串 E 的 12 處直接使用，加上 `PlayerPresentation.cpp:36-37` 的 `Require` helper（19 個呼叫端）；29 個未編譯檔另有 17 處。
  - 其餘不變；新程式碼依文件寫。
- **29 個未編譯檔**：只做被迫的相容修改，以 syntax-only 驗證。
- **未啟用產品**：不修改，R6 產出破損清單（目前已知：`tests/object_fps` 10 處、`tests/object_fps_v2` 12 處靜態 Ok/Err）。

## 4. 批次

### 4.0 共同規則

- 一批一個 PR，commit 與 PR 用日語。每批開始、里程碑、停止時更新 HANDOFF 與 dev log，結束後停下等指示。
- **本機驗收**：core 與 test 兩個 preset 都跑 configure、build、ctest。測試清單必須是前一批的超集，既有測試的結果不變；下列「被迫的語法修改」逐項寫進 HANDOFF。
- **PR 驗收**：cross-platform 的 L1 四列（windows-x64 MSVC、linux-x64 g++-14、macos-arm64、macos-x64）都會 build 並跑 test preset 的 ctest；core preset 只在 Linux 列執行。`build-and-validate`（packaging 與 installed acceptance）只在 push 與 workflow_dispatch 時跑。
- **依賴圖**：`cmake --preset test --graphviz=<dir>/deps.dot`，再用 `docs/architecture/plans/math-foundation/scripts/dot_edges.py` 抽出邊，與前一批比對。
- **29 個未編譯檔**：`docs/architecture/plans/math-foundation/scripts/syntax_check.py`，必須 29/29。
- **稽核**：`python3 docs/architecture/plans/result-unification/scripts/result_audit.py` 與 [baseline/audit.tsv](baseline/audit.tsv) 比對，每批列出預期的 delta。
- **nodiscard**：從 R2 起，build.log 中 `warning: .*(nodiscard|unused-result)|warning C4834` 必須為 0（只做 log 閘門，不開 `-Werror`）。
- **停止條件**：非預期回歸，或範圍超出 [baseline/failure_sites.tsv](baseline/failure_sites.tsv)。遇到就停下回報並重新規劃。

### 4.1 批次與檔位

| ID | 內容 | 主檔位 | 局部升降 |
|---|---|---|---|
| R0 | 任務校正、基線、分類表 | medium | 盤點用 ultracode；characterization 測試 high |
| R1 | 概念文件與 Assert | high | `Assert.hpp` 的 handler 機制 xhigh；文件 medium |
| R2 | Result 的寫法 | high | `Result.hpp` 的轉換規則 xhigh；codemod 執行 medium；審查可考慮 ultracode（屆時先詢問） |
| R3 | Error 語意、asset | high | 移除 `None`、收斂別名 medium |
| R4 | Model、ui／render 的轉換與 API 誤用 | high | 無 |
| R5 | ui_editor | high | out-param 的機械替換 medium |
| R6 | 收尾 | medium | 最終稽核與 Architecture Report 的審查 high |

檔位理由見核准版計劃：有外部驗證（編譯器、測試、grep 稽核、characterization）的機械性工作降檔；沒有外部基準或容易悄悄出錯的部分（`Assert.hpp`、`Result.hpp` 的 overload resolution、characterization 測試本身）升檔。

### 4.2 R0 任務校正、基線、分類表（medium）

- 以 `eeebc1e` 重新盤點（ultracode：3 個盤點加 1 次對抗檢查），校正本計劃。
- 建立本目錄與 `baseline/`：測試清單（core 19、test 46）、依賴邊（144 行，含 #28 的 `engine -> gyo_math`）、29 檔的 syntax 結果（29/29 PASS）、`audit.tsv`、`failure_sites.tsv`（173 列）、`wide_contract_sites.tsv`（16 個 API）。
- 新增 `scripts/result_audit.py`。
- characterization 測試：
  - `tests/common/core/io/VfsTests.cpp`：Stat、讀取用 Open、Exists 遇到 NotFound 改查下一個 mount、遇到其他錯誤停止、全部找不到時回報最後一個 NotFound。
  - `AssetManagerTests.cpp`：非同步與同步的 `KeepOldIfAny` 失敗，published handle 維持 Ready 並回報同一個錯誤；之後成功的 reload 會清掉錯誤。寫法讓 R3 把 `GetError` 改成 optional 後不必修改。
- 驗收：diff 只有 `docs/` 與 `tests/`；新測試通過；core 與 test preset 全部通過。

### 4.3 R1 概念文件與 Assert（high；`Assert.hpp` xhigh）

- 新增 `docs/architecture/error-handling.md`（英文，比照 `math.md`），以第 1 節的概念表作為開頭，寫入處理規則、wide contract、SdlGpu 例外與 C++26 對應。同批更新 `docs/architecture.md` 的依賴表、`docs/architecture/math.md`（Math 改為依賴 Base）與 `docs/creating_apps.md`。
- 依 2.1、2.2 新增 `gyo_base`、`Assert.hpp`、`gyo_test_support`、abort probe 與 `gyo_base_tests`。
- 改用 Assert：
  - Collision 的 15 處（只放在非 noexcept 的 `Validate*` 與公開查詢；`RaySphere` 等 noexcept 內部 helper 不放）。
  - `FixedTickRuntime` 的 2 處。
  - `Math::Clamp`：改成 `GYO_ASSERT` 並拿掉 `noexcept`（使用者 2026-10-04 決定）。
  - Result 在錯誤狀態下的存取改用 `get_if` 加 `GYO_ASSERT`；`Result<void,E>::value()` 拿掉 `noexcept`。
- `CatalogParser.cpp:14` 的 `catch (...)` 收窄為 `catch (const std::exception&)`。
- 17 處 `CHECK_THROWS_AS` 改成 `GYO_CHECK_ASSERTS`。
- 驗收：
  - collision 與 runtime 的 `throw` 為 0；範圍內的 `assert(`／`<cassert>` 只剩 `AssetPreviewContext.cpp`（R5）。
  - 依賴 diff 只有 `gyo_base` 相關的邊：`engine -> gyo_base`、`gyo_math -> gyo_base`、`gyo_test_support -> gyo_base`、測試到 `gyo_test_support` 的邊、probe 到 `gyo_base` 的邊。
  - abort probe 與含逗號的條件式在 L1 四平台通過。
  - pvp 的 `.cpu` 系列不改就通過（已確認已編譯的 pvp 程式碼不依賴 collision 的例外）。
- HANDOFF 列出依賴 collision 例外做內容驗證的未編譯檔：`PlayerController.cpp:43-62`、`EnemySystem.cpp:584-620`，以及會順帶吞掉例外的 `GameSession.cpp:366-372`、`570-574`。

### 4.4 R2 Result 的寫法（high；`Result.hpp` xhigh；codemod medium）

- 依 2.3 修改 `Result.hpp`；用 codemod 改寫 727 處，依模組分 commit，腳本 commit 到 `scripts/`。
- 需要人工處理：
  - `::Err({…})` 5 處：`UiDocumentCodec.cpp:931,952,959`、`UiValidation.cpp:21`、`UiSerialization.cpp:353`，改寫成 `Base::Err(UiError{…})`。
  - `::Ok({…})` 3 處：`UiColor.cpp:66`、`UiRendererTests.cpp:38-39`、`PlayerPresentationTests.cpp:63-64`。
  - 三元運算子 1 處（`UiRuntime.cpp:482-484`）；沒有 trailing return type 的 lambda 1 處（`AssetManager.cpp:57-61`）。
  - tests/common 中 5 處丟棄 `Register` 回傳值的地方（`AssetManagerTests.cpp:90,157,291`、`UiRendererTests.cpp:134,185`）。
- 驗收：
  - `static_ok`、`static_err`、`result_ok_call`、`io_result_void` 為 0；`base_err` 等於原 Err 數量 498。
  - `gyo_base_tests` 涵蓋轉換規則、`Err<G>` 轉換、拒絕 `U==E`、move-only T、void 版、錯誤狀態存取會觸發 Assert。
  - nodiscard 警告為 0；29/29。

### 4.5 R3 Error 語意與 asset（high；機械部分 medium）

- 依 2.4 修改 Error、加入 `CodedError` 與 `Describe`、移除 `None`、收斂別名、修改 `AssetRecord` 與 `GetError`。
- 刪除 `AssetError.hpp:42-67` 被註解掉的舊 struct；`AssetError.hpp` include `Error.hpp`，並在 `Engine::Asset` 宣告唯一的別名。
- API 誤用改用 Assert：`LoaderRegistry::Register` 的 3 個分支、AssetManager 的 `UnsupportedRequest`；`AssetManagerTests.cpp:447,457,468` 改用 `GYO_CHECK_ASSERTS`。
- 修正 `AssetCatalog.cpp:92-98`，保留 resolver 傳回的 code，並新增鎖住 `PathEscapesRoot` 的測試。
- 被迫的語法修改：`AssetManagerTests.cpp:609` 的 `== nullptr` 改成 `CHECK_FALSE`；12 行 `Asset::Loading::AssetError` 限定寫法（`SdlImageTextureLoader`、`UfbxModelLoader`、`RendererTests`、`ShaderLibraryTests`）。
- 驗收：
  - grep 為 0：已移除的成員、`ErrorCode::None`、`ErrorCode{}`。
  - 每個別名恰好 1 處；所有 engine 的 E 都有 `static_assert(CodedError)`。
  - R0 的 characterization 測試不改就通過。

### 4.6 R4 Model、ui／render 的轉換與 API 誤用（high）

- `ModelErrorCode`、`ModelRendererErrorCode` 取代 `std::string` E；Result 開始以 `CodedError` 約束 E。
- 依處理規則 5 改寫轉換點：UiRenderer 6 處、ModelRenderer（包括 `:113` 的原樣傳遞）、ShaderLibrary（`:48-53` 的 AssetError 不再經由例外）、`UfbxModelLoader.cpp:206`（不再以例外傳遞 model 的錯誤）。
- API 誤用改用 Assert：UiRuntime 10 處、UiRenderer 3 處（包括 `UiRendererTests:264` 鎖住的「not initialized」）、Renderer 的「not initialized」、ModelRenderer 的 null 指標檢查（`:38`、`:93`）。`UiRuntime.cpp:237` 的 `.at()` 改成先 `GYO_ASSERT` 再存取；`UiRuntime.cpp:257` 改用 `GYO_UNREACHABLE`。
- 被迫的修改：`ModelTests.cpp:109,110,243,322`、`ModelRendererTests.cpp:142` 改成 `.error().message`；Apps 中 Model 字串 E 的 12 處與 `Require` helper；29 檔中的 17 處（相容修改）。
- 驗收：
  - `string_error_type` 為 0，由 concept 在編譯期強制。
  - `UfbxModelTests.cpp:172-173` 不改就通過（外層 message 保留內層原文）。
  - 轉換點全部依 R0 的表處理；`gyo_model*` 與 `presentation_cpu` 通過；29/29。

### 4.7 R5 ui_editor（high；機械部分 medium）

- 結果型別改用 Result：`CommandLineResult`、`TextFileResult`、`FileOperationResult`、`ProbeFileStamp`、`HasExternalModification`（改成 `Result<bool, FileError>`）、`SessionResult`（改成 `Result<SessionReport, SessionError>`，`SaveFailure` 去掉 `None` 改為 `SessionErrorCode`）。
- `PreviewAdapter::Result::error` 改成 `std::optional<Engine::Ui::UiError>`，以 `Describe` 呈現；`DocumentParseResult` 屬於 diagnostics，保留。
- out-param 改成回傳值；`std::filesystem::absolute` 改用 `std::error_code` 版本（`DocumentSession.cpp:46,72,163,220`、`ReadOnlyAssetCatalog.cpp:29,49,65`）。
- Programmer Error 改用 Assert：`AssetPreviewContext.cpp:213`（並刪除後面已到不了的 if，必要時拿掉 `Unmount` 的 `noexcept`）、`AssetPreviewContext::Initialize` 的 `Register` 失敗、`UiDocumentBridge.cpp:103`。
- `Describe`、`DescribeError`、兩份 `LogError` 改用 `Base::Describe`。
- 驗收：
  - `string_error_out_param`、`cassert` 為 0。
  - 結束碼 0、2、3、4 不變（2 與 1 目前沒有測試覆蓋，本批補上 2）。
  - `gyo_ui_editor.*` 與 `build/acceptance/ui_editor` 通過。

### 4.8 R6 收尾（medium；最終審查 high）

- `error-handling.md` 定稿；對 Apps 以外做全範圍稽核；依賴圖與 R0 比對。
- 未啟用產品破損清單：用 scratch registry 加 `ninja -k 0` 產生，並標明每一項是哪一批造成的。
- 列出 Apps 中尚未遷移的項目（例如 `throw_logic` 66 處、`string_error_out_param` 126 處），作為之後順手修改時的參考，不排批次。
- Architecture Report；HANDOFF 標為完成。

## 5. Architecture Delta

1. **需求來源**：使用者明示的 Refactoring：以「Assert = Programmer Error、Result = Runtime Error」統一失敗處理，目標是可讀性、Apps 以外的風格統一與單一套處理規則。
2. **現在的問題**：見 Context。
3. **變化的 boundary**：`engine/base` 從 `GYO::Engine` 的 include 目錄升格為最底層的 leaf target `GYO::Base`；失敗處理成為 engine 全域規則，由 `docs/architecture/error-handling.md` 擁有。
4. **影響範圍**：engine 全部模組（含 Math）、`tools/ui_editor`、`tests/common`、`tests/ui_editor`、docs；Apps 只有被迫的修改。
5. **依賴方向**：新增 `GYO::Base`（只依賴標準函式庫）；`Engine -> Base` 取代 include 目錄，遞移可達的範圍不變；新增 `Math -> Base`；新增測試專用的 `gyo_test_support`。沒有循環；產品的依賴邊不變。
6. **Ownership**：Base 擁有機制、慣例、`Describe` 與 assertion handler；各模組擁有自己的 code enum 與邊界轉換；Apps 擁有自己的 E，以及是否安裝自己的 handler。
7. **為什麼沒有更小的做法**：維持 include 目錄的話，Math 與 Collision 必須依賴 `GYO::Engine` 才能使用 Assert，方向錯誤；放進 Math 則 owner 錯誤；保留例外表達 Programmer Error 會被 `catch(std::exception&)` 吞掉；新舊 API 並存或加 alias 已被使用者禁止。

**行為變更**：engine 的 Programmer Error 從丟例外（或回傳 Result）改成 Assert 後 abort。match 端原本未捕捉的例外就會 terminate；`Math::Clamp` 的檢查從「CI 中從未生效」變成永遠生效。

## 6. 驗證

- 每批：core 與 test preset 的 configure、build、ctest；L1 四列加 Linux core；依賴圖 diff；稽核 delta；29/29 syntax-only。
- R1：abort probe；17 處改寫後的測試；含逗號的條件式在 MSVC 上可編譯；Result 錯誤狀態存取會觸發 Assert。
- R2：Err 數量守恆；nodiscard 警告為 0。
- R3、R4：characterization 不改就通過；`CodedError` 在編譯期強制；轉換點全部依 R0 的表處理。
- R5：ui_editor 的測試、acceptance 與結束碼不變。
- R6：全範圍稽核、未啟用產品破損清單、Architecture Report。

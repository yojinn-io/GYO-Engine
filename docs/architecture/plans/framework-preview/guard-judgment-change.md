# 守衛的判定變更（`v8-framework-preview-guard`，AGENTS.md §14.4）

更新：2026-11-05。批次 `v8-framework-preview-component`。兩個守衛都是 Design Invariants 的守衛，所以不論有沒有放寬面，
都照 §14.4 寫 1～5、做突變、由另一位検証者確認，再交使用者承認。
**使用者 2026-11-06 核准（「Approve」），含第 3 項「限界」所列的三種擋不住的寫法。**

- A：framework 的依賴守衛（`tests/framework/framework_dependency_guard.py`，framework DI 1～3）＋ configure 時的連結檢查
  （`tests/framework/CMakeLists.txt`，DI 2）。
- B：model_viewer 的依賴守衛（`tests/model_viewer/model_viewer_dependency_guard.py`，model_viewer DI 1）。

## A：framework 的守衛

1. **守的性質**：framework 核心（`framework/bindings/` 以外）只依賴登記過的 Engine target；SDL、`*Native`、第三方
   函式庫只能進綁定。每多一條依賴邊，都要先寫進守衛才過，所以每條邊都有紀錄（DI 2）。
2. **放寬面**（以前失敗、以後通過）：核心的 CMake 檔出現 `GYO::Render`、`GYO::RenderBackendSDLGPU`、
   `GYO::RenderBackendSDL`、`GYO::ToolFrameworkPreview` 這四個名字。不論出現在連結、`if(TARGET …)` 或註解裡，
   以前都報「names …」，以後不報。只有這四個名字；其他名字的判定不變。
   另一面（使用者 2026-11-06 核准修法後加的）：每個 library 的清單判斷前先去掉 token 前後的引號（CMake 的 `"X"` 就是 `X`）。
   所以「引號裡、本來就在該 library 清單上的名字」（例如 `gyo_tool_framework` 連 `"GYO::Os"`）在新規則下通過。
   這不是從舊守衛放寬：舊守衛沒有每個 library 的清單，這種寫法本來就通過；這是新規則裡的放寬，列在這裡一起核准。
3. **收緊面**（以前通過、以後失敗）：
   - 每個核心 library 有自己的連結清單。`gyo_tool_framework` 只能連 Engine、Input、Ui、Model、Os、Audio。
     `gyo_tool_framework_window` 只能連核心與 `GYO::PlatformSDL`。`gyo_tool_framework_preview` 只能連核心、
     `GYO::PlatformSDL`、Render 三個 target 與 Os。以前只要名字在核心的總清單裡就過；例如視窗外殼連
     `GYO::InputBackendSDL`，以前過，以後失敗。
   - 連結的名字寫在 generator expression（`$<…>`）裡也照清單判。
   - 核心的 CMake 用 `add_library` 定義了沒有清單的 library，或用變數當名字定義 library，都失敗。
   - configure 時新增一個檢查：核心與視窗外殼實際連到的 target（含經介面帶進來的）裡，不能有 `GYO::Render*`
     或 `gyo_render*`。它讀 CMake 的連結結果（自己的 PRIVATE、PUBLIC、INTERFACE 連結），名字直接寫、經 ALIAS、
     經變數、寫在 genex 裡、genex 裡具名的 target（例如 preview 自己的 target）都會跟進。
     後兩項是使用者 2026-11-06 選「先補強再核准」後加的（突變 `fw-link-window-preview-genex`、`fw-link-window-render-interface`）。
     補強的第一版有回歸：用實名直接連的 target 不跟進介面（検証者第 2 輪）。修正後，「看過的 entry」與「跟進過介面的 target」
     分成兩張清單（突變 `fw-link-window-preview-variable`：經變數用實名連 preview）。
   - **限界**（検証者找到，下面這些寫法新規則抓不到；master 上也都通過，所以不是放寬，是收緊沒涵蓋到）：
     - Python：第一個 token 加引號或用變數（`target_link_libraries("gyo_tool_framework_window" …)`）時，
       不套每個 library 的清單，只套總清單。genex 裡只檢查 `X::Y` 形式的名字。
       `add_library(x)` 沒有來源檔、名字加引號、名字含 `-` 時，「沒有清單」的規則不觸發。
     - configure 時的檢查：名字含 `-` 或 `.` 的 target（例如 `fw-extra`）會被拆開，不會被認出是 target，也不跟進。
       配上 Python 的漏洞（`add_library(fw-extra …)` 不觸發「沒有清單」、它自己的連結不套清單、視窗經 `${V}` 連它），
       一個這樣命名的輔助 library 就能讓視窗外殼拿到 render，兩邊都擋不住（検証者第 3 輪 W34、W35）。
     - configure 時的檢查：經變數放進 archive 的路徑（`/…/libgyo_render.a`）或 `-lgyo_render` 旗標，抓不到。
       這種寫法只連 archive、不帶 include 目錄（W36、W37）。
     - configure 時的檢查：經別的 target 介面的 `$<LINK_ONLY:>`（那個 target 的 PRIVATE 連結）不跟進。
       例如視窗外殼連一個 static 的輔助 library，而那個 library PRIVATE 連 render，就抓不到。
       這個限界和既有的 SDL 連結檢查相同：Engine 的 backend 以 PRIVATE 連 SDL 是設計，跟進就會誤判。
       Python 那一面又會要求輔助 library 有自己的清單（上面的 `add_library` 漏洞除外）。
     - 補強前，configure 檢查只讀 `LINK_LIBRARIES`，也不跟進 genex 裡具名的 target。
       所以 `$<BUILD_INTERFACE:gyo_tool_framework_preview>` 一行就能讓視窗外殼經 preview 拿到 render。
       這兩點已補上。
4. **守的性質是否仍由其他判定守住**：放寬的四個名字都是 Engine 的非 `*Native` target，DI 2 允許加進正向清單。
   SDL、`*Native`、imgui、nlohmann 的規則都沒改。以下兩條原本由「名字不在清單」守，現在改由其他判定守，而且比以前強：
   - 「視窗外殼不建 render device」（DI 6、Replacement Criteria）：改由每個 library 的清單（`fw-shell-core-render`
     的預期訊息改成「`gyo_tool_framework_window links GYO::RenderBackendSDL`」）＋ configure 時的檢查
     （`fw-link-window-render`、`fw-link-window-preview-genex`、`fw-link-window-render-interface`、
     `fw-link-window-preview-variable`）守住。
     一般的寫法和以前一樣擋得住。經變數、別名、genex、INTERFACE、經 preview 的 target 拿到 render 的寫法，比以前多擋。
     剩下的是上面「限界」列的三種：經別人的 PRIVATE 連結、名字含 `-` 或 `.` 的輔助 target、經變數的 archive 路徑或
     `-l` 旗標。新舊都擋不住，以前也是靠審查。
   - 「核心只有 preview 碰 render」：也由這兩個判定守住，限界相同。
   - configure 時既有的 SDL3／imgui／nlohmann／`*Native` 連結檢查沒動（`fw-link-preview-native` 證明它也檢查 preview）。
5. **根據與證據**（使用者的 Mac，主 checkout `build/evidence/framework-preview/`）：
   - `guard_red_green/framework/`：master 的舊守衛跑新自測 54 條中 11 條失敗；新守衛 54/54。
   - 突變：本批 `fw-guard-*`、`fw-link-*` 共 8 條。第一次 7 條 killed，`fw-guard-window-render-genex` 是 wrong_failure
     （引號）。使用者核准修法（守衛去引號）後重跑 3 條，3/3（`mutations_rerun_quotes.txt`）。
     D184 重跑既有的 `fw-guard-*`、`fw-link-*` 18/18、`fw-shell-core-*` 等 5/5（`mutations_d184_*.txt`）。
     補強後：新 2 條＋以 configure 失敗判定的既有 8 條，10/10（`mutations_patch_*.txt`）。修正回歸後：
     以 configure 失敗判定的 11 條（本批 6、既有 5），11/11（`mutations_fix_*.txt`）。
   - 検証者的比對腳本與輸出：`verifier/`。

## B：model_viewer 的守衛

1. **守的性質**：model_viewer 不連任何遊戲，也不自己碰 SDL（DI 1）。它的 CMake 只能具名正向清單裡的 target。
   清單沒有任何 SDL 或 `*Native` target。
2. **放寬面**：model_viewer 的 CMake 具名 `GYO::ToolFrameworkPreview`。以前報「names …」，以後通過。
3. **收緊面**：具名 `GYO::Render` 或 `GYO::RenderBackendSDLGPU`。以前通過，以後失敗：這兩個由 framework 的 preview
   組合後帶給工具，工具再具名它們就是多一條自己的依賴邊。
4. **守的性質是否仍由其他判定守住**：`GYO::ToolFrameworkPreview` 是 framework 核心的 target。依 framework 的守衛
   （A），它只連 Engine 的非 `*Native` target，SDL 留在 Engine backend 的 PRIVATE 裡。所以 model_viewer 經它拿到的，
   和以前自己直接連的 `GYO::Render`、`GYO::RenderBackendSDLGPU` 相同，沒有多出 SDL。
   有一點不同（検証者指出）：`GYO::RenderBackendSDLGPU` 以前是 `gyo_model_viewer_preview` 的 PRIVATE 連結，
   現在經 preview 以 PUBLIC 帶進來，所以會進到 `gyo_model_viewer` 的編譯介面。model_viewer 的守衛只看具名的 target，
   這件事不影響它的判定。
   model_viewer 的 `SDL3/`、`SDL_`、`*Native`、`imgui_internal.h`、遊戲 target、登錄 id 規則都沒改。
5. **根據與證據**：
   - `guard_red_green/model_viewer/`：舊守衛跑新自測 15 條中 2 條失敗；新守衛 15/15。
   - 突變 `mv-guard-*` 5 條重跑。
   - 検証者的確認：`verifier/`。

## 検証者的結論

Opus 5.5／high 子 Agent（2026-11-06），沒有改工作樹。
比對腳本與輸出在主 checkout 的 `build/evidence/framework-preview/verifier/`：`compare_framework.txt`、
`compare_model_viewer.txt`、`analysis.txt`、`unchanged_and_bypass.txt`、`configure_probe_output.txt`。

| 項 | 結論 | 根據 |
|---|---|---|
| A1 | 確認 | 守衛的說明與規則相符 |
| A2 | 確認 | 2110 個合成輸入中，127 個從失敗變通過，全部只因「names 那四個名字之一」。82 個「新舊都失敗、但發現不同」的輸入，消失的發現也都是這四個名字。引號裡的清單內名字新舊都通過 |
| A3 | 部分確認 | 271 個從通過變失敗，全是文件寫的那幾類。但有拼法上的漏洞（已寫進第 3 項的「限界」） |
| A4 | 部分確認 | SDL、`*Native`、imgui、nlohmann 的發現新舊相同（504 個 CMake 輸入、94 個其他輸入）。「比以前強」有反例，已改寫第 4 項 |
| A5 | 部分確認 | 原文的數字過時（53／10 → 54／11、9 → 8 條），也沒提 wrong_failure；已更正 |
| B1～B3、B5 | 確認 | 244 個輸入中，7 個從失敗變通過，全是 `GYO::ToolFrameworkPreview`；12 個從通過變失敗，全是那兩個名字 |
| B4 | 確認，附一點 | `GYO::RenderBackendSDLGPU` 從 PRIVATE 變成經 preview 的 PUBLIC；已寫進第 4 項 |

文件沒寫到的放寬：沒有。

### 第 2、3 輪（使用者選「先補強再核准」之後）

| 輪 | A3（configure 部分） | A4 | 摘要 |
|---|---|---|---|
| 第 2 輪 | 不成立 | 不成立 | 補強修好了第 1 輪的漏洞，但造成回歸：用實名直接連的 target 不跟進介面 |
| 第 3 輪 | 部分確認 | 部分確認 | 回歸已修。文件列的寫法都確認（直接、別名、變數、genex、genex 裡具名的 target、INTERFACE、12 層實名鏈、環）。另找到名字含 `-`／`.` 與 archive 路徑／`-l` 兩種繞法，已寫進第 3 項的「限界」與第 4 項 |

誤判風險低：engine／framework 的 CMake 沒有 `$<`；條件為假的 genex 與 `$<TARGET_PROPERTY:…>` 會被保守地當成連結，
現在的樹沒有這種寫法。探針與輸出在 `verifier/round2/`、`verifier/round3/`。

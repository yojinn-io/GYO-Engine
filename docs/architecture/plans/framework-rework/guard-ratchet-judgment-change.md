# v8-framework-guard-ratchet：判定變更與 Design Invariants 條文

日期：2026-11-01。依 AGENTS.md §3（Design Invariants 的變更）與 §14.4（守衛判定的變更）提出。
**狀態：使用者 2026-11-01 核准**（「核准」）。條文已寫入 `docs/framework/architecture.md`；DI 2 保留了原有的「工具自己的 DI 1 經由 framework 仍成立」一句，因為 `docs/ui_toolchain.md` 引用它。

## 1. 要核准的事

1. `docs/framework/architecture.md` 的 DI 2、DI 5、Replacement Criteria 改寫成第 4 節的條文。
2. framework 守衛的判定變更（第 2、3 節）：Python 守衛改寫、例外清單、configure 時的連結檢查。
3. 突變目錄從 `docs/tools/plans/v8/scripts/` 搬到本計畫的 `scripts/`（工具的兩條留在原處）。

## 2. §14.4 的 1～5 項

1. **守的性質**：DI 2 的依存單向（不 include apps/、tools/，不連遊戲、GYO::Input、SDL_GPU）。這批加上「核心不含 SDL、ImGui、nlohmann、*Native，也不連綁定；綁定各有正向清單，只有 ImGui 綁定可用 Native」。DI 1（不是產品）與 DI 3（不寫已登錄 id）不變。
2. **放寬的面**（舊判定失敗、新判定通過）：只有三個 CMake target 名稱：`GYO::ToolFrameworkWindow`（核心與 ImGui 綁定）、`GYO::ToolFrameworkImGui`（ImGui 綁定）、`GYO::ToolFrameworkJson`（JSON 綁定）。
   舊守衛拒絕的其他東西（engine/input、SDL_GPU、舊前綴以外的 include、舊清單以外的 target、已登錄 id、project.json、add_executable 與安裝）在任何目錄都是不能進例外清單的硬規則。
3. **收緊的面**（舊判定通過、新判定失敗）：
   - 核心 include：`SDL3/`、`backends/imgui_impl_sdl*`、`nlohmann/`、engine 的 `*Native.hpp` 與 `native/` 路徑、SDL／ImGui／nlohmann 自己的無斜線標頭（`SDL.h`、`SDL_*.h`、`imgui*.h`、`json.hpp`）。
   - 核心程式碼（去掉註解、字串與 include 行）出現 `SDL_*`、`ImGui*`、`nlohmann`。
   - 核心 CMake 具名 `SDL3::SDL3`、`imgui::*`、`nlohmann_json::nlohmann_json`、`GYO::*Native`、`GYO::ToolFrameworkGui`、`GYO::RenderBackendSDL`、`GYO::ToolFrameworkImGui`、`GYO::ToolFrameworkJson`。
   - `framework/bindings/` 底下沒有正向清單的目錄裡的任何檔案。ImGui 綁定：呼叫 `SDL_*`、具名 `SDL3::SDL3`、include 指定的兩個以外的 Native 標頭，或 include `SDL3/`、`nlohmann/`。JSON 綁定：include imgui 或 Native、用 `SDL_*`／`ImGui*`。
   - 例外清單本身的錯誤：格式錯、次數 0、檔案不存在、重複、指到綁定、link 行沒點名。
   - 新的 configure 時連結檢查：核心 target（含 OBJECT、INTERFACE 程式庫）的直接連結（包在產生器運算式裡的也算）或經介面閉包碰到 SDL3（含 SDL3_image、SDL3_ttf、`-lSDL3`）、ImGui、nlohmann、*Native 或綁定 target；JSON 綁定碰到 SDL3、ImGui、Native 或別的綁定；ImGui 綁定直接連 SDL3、nlohmann 或別的綁定；沒有規則的綁定。
   - 現有違規逐檔逐規則凍結在 `tests/framework/framework_guard_allowlist.txt`，次數必須相等；link 行點名禁止的 target，不能把一個換成另一個。
4. **放寬的性質是否仍被守**：三個新名稱只能出現在各自允許的位置，或經核准的例外清單行。核心連 `GYO::ToolFrameworkWindow` 時，連結檢查會走它的介面，帶進 SDL3 或 Native 就失敗。
   限界：另一個目錄裡定義、非 GLOBAL 的 IMPORTED target，CMake 在檢查時看不到，無法跟進它的介面。連結檢查只在 tests/framework 被 configure 時存在（BUILD_TESTING 且選了用 framework 的工具），L1 的 ci 三列都符合。
5. **根據與證據**（使用者 Mac，主 checkout `build/evidence/framework-rework/v8-framework-guard-ratchet/`）：
   - 舊自我測試 12 條對新守衛：11 條通過，只有 `test_clean_tree_passes` 失敗，失敗內容正好是三項收緊（核心 include `SDL3/SDL.h`、`backends/imgui_impl_sdl3.h`，CMake 連 `imgui::core`）。
   - 突變 14 條（舊 fw-guard-* 4 條，錨點兩條改到 `ReadOnlyAssetCatalog.cpp`、預期訊息不變；新增 10 條）：宣告 14 = 執行 14 = killed 14，stale 0。
   - 新自我測試 36 條通過；`gyo_framework.*`、`gyo_ui_editor.*`、`gyo_audio_editor.*` 15/15 通過。
   - 獨立審查（Sonnet，提案者以外）：舊新兩版守衛的差分測試約 2 萬棵合成樹，舊拒絕而新接受的只有上述三個名稱；連結檢查找出 7 類洞，補洞後 23 個案例中 22 個如預期，剩下的是上面寫明的 IMPORTED 限界。審查的原始輸出未保存，補洞後的案例結果在 `linkcheck_cases_after_fix.txt`。

## 3. 為什麼這樣設計

- **ratchet 用既有格式**：例外清單照抄 `tests/common/os/os_boundary_guard.py` 的「path rule count」，次數必須相等。增加會失敗，減少會要求縮清單。
- **舊判定變成硬規則**：只記次數的清單擋不住「拿一個已列的違規換一個舊守衛就擋的東西」。所以舊守衛會拒絕的東西一律不能被清單涵蓋。
- **舊件搬出主檔**：舊的 gui target 與核心的 nlohmann 連結搬到 `framework/Legacy.cmake`，`framework/CMakeLists.txt` 零例外。外殼批與 catalog 批要在主檔加東西，不會被清單遮住。
- **兩道檢查**：Python 守衛讀原始碼，configure 檢查讀實際的連結結果。突變 fw-link-closure-sdl 用變數拼出 `SDL3::SDL3`，Python 守衛看不到，只有連結檢查擋得住。

## 4. Design Invariants 條文（英文，照原文件語言）

**DI 2（取代現有條文）**

> Dependencies point one way: game or tool → framework → engine. The framework includes nothing from apps/ or tools/, and the engine never names it; it links no game, no GYO::Input and no SDL_GPU. The core (everything outside framework/bindings/) includes no SDL3/, imgui, backends/, nlohmann/ or Engine *Native header, names no SDL_, ImGui or nlohmann symbol, and links no SDL3, imgui, nlohmann_json or GYO::*Native target and no binding: it reaches SDL only through Engine's neutral targets. A binding is its own target under framework/bindings/<name>/ with its own positive list, and the core never names it. Only the ImGui binding links GYO::*Native, PRIVATE; no framework target names SDL3::SDL3 or an SDL_ symbol. Guards: the dependency guard (core rules, one positive list per binding, and an allowlist of existing core sites whose counts must match and only shrink, empty after v8-framework-invariants) and the configure-time link check in tests/framework/CMakeLists.txt.

**DI 3（補一句，刪「dialog filters」例子）**

> Registration policy is chosen by the caller's options, never by which product calls.

**DI 5（取代現有條文）**

> The core is the product shell: it puts Engine capabilities together and hands control to the product (a headless and a windowed shell, content mounting, registration policy). It holds no product's UI, document format or rules, and names no concrete UI or data-format library; such a library enters only as a binding. A capability Engine lacks is added to Engine, not to the framework. Otherwise only what two or more products share enters. The game part enters only on the condition above. The windowed shell has one user (ui_editor) when it is added; the user accepted that on 2026-11-01 (framework-rework D-8). Replacing ImGui means replacing its binding and the tool's own drawing code; the core shell and Engine stay.

**Replacement Criteria（取代現有清單）**

> - To serve a product, the framework has to know what that product means, for example a branch on which game or tool is calling.
> - Each new tool needs a change to the core's public interface (adding a shared piece does not count).
> - The core needs a concrete UI or JSON library to serve a caller.
> - A binding has to know which product calls it.
> - The windowed shell has to create or choose a render device for its client.
> - The framework copies an Engine rule that no shared fixture checks.

刪掉的是「A tool needs a GUI stack other than ImGui on the SDL renderer」：ImGui 已經變成可換的綁定，換 GUI 不再需要重建 framework。
DI 4 與 Using it 等收尾批再改（PLAN 第 2.6 節）。DI 1 不變。

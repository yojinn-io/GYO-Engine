# 模型檢視工具依賴守衛的判定變更（`v8-tool-model-viewer-preview`，AGENTS §14.4）

**狀態：已核准（2026-11-04，使用者：「核准，提交 -preview」）。**

守衛：`tests/model_viewer/model_viewer_dependency_guard.py`，守的是 [docs/model_viewer/architecture.md](../../../model_viewer/architecture.md) 的 Design Invariant 1 與 6。
這是 Design Invariants 的守衛，所以不論有沒有放寬的部分，都要你核准（AGENTS §14.4）。

## 變更內容

1. 允許的目標多 7 個：`GYO::Render`、`GYO::ModelRenderer`、`GYO::RenderBackendSDLGPU`、`GYO::RenderBackendSDL`、`GYO::PlatformSDL`、`GYO::ToolFrameworkImGui`、`imgui::core`。
   - 原因：`-preview` 的視窗要用 Engine 的 `Renderer`、`ModelRenderer` 畫到離屏 GPU 裝置，經 SDL 2D 渲染器的貼圖顯示，跑在 framework 的 ImGui 外殼上。
   - 事前宣告的 DI 1 草稿已點名前三個（「(B′) 會用到 `GYO::Render`、`GYO::ModelRenderer`、`GYO::RenderBackendSDLGpu`，都不是 `*Native`」）。
2. 目標名的比對改成取完整的 CMake 名稱：允許的名稱後面接 `-`、`.` 或 `+` 的，算另一個目標。
   - 例：`GYO::PlatformSDL-Native` 以前只比對到 `GYO::PlatformSDL` 就通過，現在會失敗。
   - 這是檢驗者找到的舊漏洞。

## AGENTS §14.4 的 1～5

1. **守的性質**：工具不連結遊戲、不提任何產品；不 include `SDL3/`、不使用 `SDL_` 符號、不提 SDL 或 `*Native` 目標、不 include `imgui_internal.h`（DI 1）；不提其他已登錄的 id（DI 6）。
2. **放寬的部分**：工具的 CMake 檔提到或連結上面 7 個目標時，以前失敗，現在通過。除此之外，沒有「以前失敗、現在通過」的輸入。
   - 檢驗者用 78 棵合成樹比對了新舊守衛，結果相符。
   - 它找到的另一類「以前失敗、現在通過」的名稱（允許的名稱後面接 `-` 或 `.`），已經由變更 2 收緊；新的自我測試確認它們失敗。
3. **收緊的部分**：變更 2。
4. **放寬之後性質仍然守得住**：
   - 7 個目標都不是 `*Native`，也都不把 SDL 帶給使用者。
     - 檢驗者查過 CMake：`PlatformSDL`、`RenderBackendSDL`、`RenderBackendSDLGPU` 都以 PRIVATE 連 SDL3，原生的 include 目錄也是 PRIVATE。
     - `ToolFrameworkImGui` 把 ImGui、SDL 後端與 `*Native` 都設為 PRIVATE。
     - `imgui::core` 不連 SDL。
   - 下列輸入在新舊守衛下都失敗：
     - SDL3 的各種目標、所有 `*Native` 目標；
     - ImGui 的 SDL 後端目標；
     - 遊戲的 `Name::Name` 目標與一般目標名；
     - `SDL3/` include、`backends/` include、`imgui_internal.h`、`SDL_` 符號、其他已登錄的 id。
   - 守的位置：同一支守衛的其他規則，以及它的自我測試（13 個）。
5. **根據與證據**（使用者的 Mac，主 checkout）：
   - 守衛在目前的樹上回傳 0；自我測試 13／13 通過。
   - 既有的守衛突變（`mv-guard-*`）在新守衛上 3／3 killed：`build/evidence/tool-v8-model-viewer/preview/mutations.txt`。
   - 檢驗者（Opus 5.5／high，和提案者是不同的 agent）的比對腳本與結果：`build/evidence/tool-v8-model-viewer/preview/guard-verify/`。檢驗者另外加的 2 條突變也 killed。

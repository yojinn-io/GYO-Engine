# 工具 v8：v8-engine-sdl-renderer-texture

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`（PR #146），接在 `v8-engine-render-offscreen` 之後。

- 段：上段（Engine 公開介面、platform-sdl 的 Replacement Criteria 改寫）。
- 實作者：Opus 5.5／high（主會話）。
- 審查者：Opus 5.5／high（子 agent，一次）。
- 沒有放寬判定，沒有開檢驗者。
- 估時 2～3 h；從開工到 T3 決定，約 2 h（含第一次建置有 ImGui 的建置樹）。

**狀態：**
- 程式、測試、突變、審查、T3 都完成。
- L1 的結果記在 HANDOFF。

相關文件：
- [事前宣告](../tools/plans/v8/model-viewer.md)：「`v8-engine-sdl-renderer-texture`（(B′)）」、「實作後的結果」與決定紀錄 `v8-engine-sdl-renderer-texture-t3`；
- [platform-sdl.md](../architecture/platform-sdl.md)：中立層的說明，以及 Replacement Criteria 2；
- [framework architecture.md](../framework/architecture.md)：「Using it」。

## 做了什麼

- Engine（`GYO::RenderBackendSDL`）：
  - `SdlRenderer` 多了串流貼圖，用 `SdlTextureHandle` 指定：
    - `CreateStreamingTexture(width, height)`：格式 `SDL_PIXELFORMAT_RGBA32`，也就是記憶體裡依序 R、G、B、A，色彩空間為 sRGB；
    - `UploadTexture(handle, bytes, rowPitch)`：最後一列只需要 `width * 4` 位元組，所以可以直接上傳 `ReadFrame` 帶補齊列距的讀回；
    - `ReleaseTexture(handle)`。
  - handle 是索引加世代號。世代號在整個行程裡唯一，所以已釋放的 handle、位置被重用後的舊 handle、別的渲染器的 handle 都會被拒。
  - 原生層多了 `NativeTexture`。
  - 錯誤碼多了 `TextureFailed` 與 `InvalidArgument`。
- framework（ImGui 綁定）：
  - 多了 `ImGuiTextureId(renderer, texture)`，回傳 `ImGui::Image` 用的 `uint64` id；不是活著的貼圖就什麼都不回。
  - 標頭裡沒有 ImGui 或 SDL 的型別；原始碼也不寫 `SDL_` 符號（framework DI 2），用的是 `auto*`。
- 文件：
  - platform-sdl 中立層的段落；
  - framework 的「Using it」；
  - platform-sdl 的 Replacement Criteria 2（T3，(a)）。
- 新候選 `v8-tool-ui-editor-texture-api`：讓 ui_editor 的資產預覽改用這個 API，拿掉它的 `*Native` 例外（記在 [README](../tools/plans/v8/README.md)）。

## 驗證與經過

- 本機 macOS：
  - `build/target/_build/mv`：`render.sdl_renderer_texture` 與 `runtime.sdl_smoke` 通過。
  - 新的建置樹 `build/target/_build/ui`（`GYO_TOOLS=ui_editor;model_viewer`，有 ImGui；`GYO_PROJECTS` 留空，用預設，含 PvP）：
    - `imgui|texture|shell` 6／6 通過；
    - `cpu|shader` 117／117 通過。
- CI 只跑 `cpu|shader` 標籤，而 ImGui 外殼的既有測試只標 `editor`，不在 L1 裡。所以 T2 另外登錄了 `gyo_framework.imgui_texture`（`cpu;editor`，只跑 `T2:*`）。T1 在 `tests/common` 下，沒有指定標籤的測試預設是 `cpu`，我另外也標明了 `cpu`。
- 件數の突き合わせ：一致。
  - 宣告 2 條 ＋ 審查後加 1 條 ＝ 3 條，3／3 killed。
  - 第一輪是宣告的 2 條，2／2 killed。
  - 審查後改了上傳檢查那一行，`rt-upload-unchecked-bytes` 的 find 字串跟不上，變成 stale。更新 find 後 3／3 killed。預期訊息沒有改。
  - 突變用 `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software` 執行，和 ctest 的環境相同。
- 審查的指出與處理：
  1. （中）列距超過 `INT_MAX` 時，轉成 SDL 的 int 會變成負數。高度 2 列以上、緩衝區有 2 GiB 以上時，SDL 會讀到緩衝區之前的位置。修正：列距或大小超過 `INT_MAX` 回 `InvalidArgument`；加了測試與突變 `rt-pitch-above-int-max`。
  2. （中）測試在同一個 Draw 裡先畫貼圖再釋放，但 ImGui 在 Draw 之後才畫，所以畫的時候貼圖已經沒了。修正：標頭寫明「在 BeginFrame 釋放」，測試改成在第 4 幀的 BeginFrame 釋放。檢查的項目不變。
  3. （低）SDL 貼圖建好之後如果 `emplace_back` 丟出例外，貼圖會漏掉。修正：建立貼圖之前先預留位置。
  4. 本批合併後，platform-sdl Replacement Criteria 2 的期限條件就成立了。交給使用者決定（T3），使用者選 (a)。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/renderer-texture/`，有：
  - `mutations.txt`（3／3）、`mutations-round1.txt`、`mutations-round2.txt`（含 stale）；
  - `ctest-texture.txt`、`ctest-all.txt`、`ctest-ui-tree.txt`。

## 和事前宣告不同的地方

- 列距或大小超過 `INT_MAX` 也會被拒。
- 釋放的時機有限制：要在 BeginFrame 釋放。
- T2 另外登錄成 `cpu` 的測試，讓它在 L1 裡跑。

## 發現（不在本批範圍）

- ImGui 外殼的既有測試（`gyo_framework.imgui_shell`，I1～I11）只標 `editor`，不在 L1 的 `cpu|shader` 裡。要不要也放進 L1，由 framework 的負責方決定。

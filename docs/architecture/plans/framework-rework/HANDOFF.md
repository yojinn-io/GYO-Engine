# framework 整改：交接

## 代號對照

| 代號 | 白話 |
|---|---|
| v8-framework-rework-plan | 定計畫 |
| v8-framework-guard-ratchet | 先把守衛的規則裝好，舊違規列進只減不增的清單 |
| v8-engine-platform-path-dialog | 選擇窗的 OS 半進 Engine |
| v8-engine-io-user-files | 使用者檔案的缺口補進 engine/io |
| v8-framework-shell | 產品外殼與 ImGui 綁定 |
| v8-framework-catalog-structure | catalog 登錄的結構與 JSON 文字分開 |
| v8-framework-invariants | 工具切換後收尾 |
| T1～T3b | 工具線（另一個會話）的切換工作，見 README |

## 現況

- 2026-11-01：v8-framework-rework-plan 完成（使用者核准 PLAN 第 1 版與 D-1～D-9）。下一批 v8-framework-guard-ratchet。
- 工具線分支 `claude/v8-tool-audio-editor-edit` 只在本機，停在核准關卡等本計畫。

## 決策紀錄

- 2026-11-01，使用者：「照建議核准」。PLAN 第 1 版與 README 待決事項全部採建議：
  - D-1：catalog 規則採 (a)＋b-lite。framework 核心只做 catalog 層級，用 cases.json 子集對齊；Engine 的 `IsRelativeContentPath` 移到 engine/asset 的公開標頭，framework 直接呼叫（AD4，b-lite）。跨 catalog 與 shader 重疊仍由載入時的 ContentManifest 判定。
  - D-2：`AbsolutePath` 留在 framework 核心，與內容掛載放一起；ui_editor 用核心這一份。
  - D-3：選擇窗的跨執行緒信箱歸 Engine；工具留用途對照與每幀套用。這是原則 6 的釐清。
  - D-4：T1 與 T3a 一起做，不給舊宿主加橋接。
  - D-5：守衛採 ratchet，第一批裝好規則，例外清單逐條記次數、只減不增，收尾批清空。
  - D-6：更新條目時丟掉額外欄位的缺陷由本線修，在 catalog-structure 之後另開一小批，依 §14.3 先 Red 後 Green。
  - D-7：PR 兩條：A＝規劃到 catalog-structure（含 D-6 的小批），B＝收尾。
  - D-8：有視窗外殼目前只有 ui_editor 一個使用者，接受，DI 5 註明。
  - D-9：外殼草圖用 ultracode；選擇窗只做 xhigh 審查加別的引擎做一次對抗式檢查。
- 各批的 Design Invariants 條文與守衛變更仍在該批開工或定案前逐批核准。

## 研究事實（2026-11-01 核對，基準 `6e78090b`）

- SDL 版本 3.4.0（`third_party/sdl3/CMakeLists.txt:10`）。`SDL_dialog.h`：回呼可能在別的執行緒（:125-126、:154-156）；Linux 可能要 portal，不用 SDL 處理事件的程式要在主迴圈呼叫 `SDL_PumpEvents`（:131-133）；filter 要活到回呼（:145-146）；只能從主執行緒呼叫（:154-156）。文件沒有寫「在呼叫返回前觸發」，`framework/src/SdlPathDialogs.cpp:50-51` 的註解是推測。`engine/render/shaders/pipeline/cmake/Dependencies.cmake:17-19`：Cocoa 對話窗依賴視訊。
- framework 每一塊目前只有一個使用者；兩個工具共用的只有錯誤型別。這與 framework DI 5 衝突。
- audio_editor 完全沒用 `FileService`，自己用 `Engine::Os::Files` 讀寫（`tools/audio_editor/src/SoundExport.cpp:19,38-69`）；它的守衛不允許 `engine/io/` 與 `GYO::Engine`（`tests/audio_editor/audio_editor_dependency_guard.py:26-27`）。
- `FileService` 用 `std::filesystem::path`（`FileService.hpp:24-45`），`AbsolutePath`／`IsPathWithin` 做 `lexically_normal`（`FileService.cpp:180-211`），與 io.md DI 2、DI 3 不合，不能照搬進 engine/io。
- cases.json 已有 54 個案例，含跨 catalog 重複、shader 重疊、控制字元；framework 沒有測試讀它。
- 突變目錄 `docs/tools/plans/v8/scripts/framework_mutations.json` 的 14 條全部會受影響；執行器在 find 對不上時記 stale 並讓整次回 1（`docs/architecture/plans/time-threads-trace/scripts/run_mutations.py:110-112,134`）。
- core preset 的 `GYO_TOOLS` 為空，不建 framework；L1 只有 ci 三列會建 framework 與它的測試。L1 沒有「只選 audio_editor」或「不選工具」的組態。
- CI 沒有 sanitizer（`.github/workflows/` 的四份工作流程、`CMakePresets.json`、`build/cmake/` 都沒有 sanitize）。
- `platform_sdl_tests` 用 dummy 視訊驅動（`tests/common/platform_sdl/CMakeLists.txt:18-22`）；`ClockReaders` 縫在 Native 標頭（`SdlPlatformNative.hpp:29-36`）。

- PR #125（asset-layout 線）新增 CTest `object_fps_pvp.asset_catalog` 的 `test_no_other_owner_names_a_pvp_asset_path`：掃 framework/、engine/、tools/、tests/framework 等共通層，出現 `assets/object_fps_pvp` 或 PvP catalog 的相對路徑就失敗（PvP 素材標準 DI-6，`docs/object_fps_pvp/assets.zh-Hant.md`）。本線的測試資料只用合成資料或 cases.json，不寫 PvP 素材路徑。

證據：使用者的 Mac，主 checkout `build/evidence/framework-rework/`（見該資料夾的 README）。

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

- 2026-11-01：v8-framework-rework-plan 完成（使用者核准 PLAN 第 1 版與 D-1～D-9，commit `f8878799`）。
- 2026-11-01：v8-framework-guard-ratchet 完成（使用者核准判定變更與 DI 條文，commit `0a635225`，draft PR #127）。L1：手動觸發的 cross-platform run 41227212440，`L1 / linux-x64`、`windows-x64`、`macos-arm64` 都是 success。
- 2026-11-01：v8-engine-platform-path-dialog 完成（使用者核准）。下一批 v8-engine-io-user-files。

### v8-engine-platform-path-dialog 的事前宣告（2026-11-01，執行前）

- 一般突變（`scripts/platform_path_dialog_mutations.json`），全部要 killed、stale 0：pd-drop-answer、pd-tag-lost、pd-order、pd-take-keeps、pd-cancel-as-failed、pd-assert-observer、pd-kind-map、pd-noexcept（預期建置失敗）、pd-g2-neutral。共 9 條。
- 相關的既有突變（D184：錨點被這批改到）：input-and-present 的 m6a-g1、m6a-g2，錨點改到 `#include <vector>`，替換內容不變。共 2 條，要 killed。
- sanitizer 突變（`scripts/platform_path_dialog_mutations-asan.json`，本機 AddressSanitizer 樹）：pd-filter-copy、pd-mailbox-not-shared，共 2 條，要 killed。基準線：ASan 樹上 platform_sdl_tests 的選擇窗案例全過、沒有 ASan 報告。
- 第一次執行（2026-11-01）：一般 9 條中 8 條 killed；pd-g2-neutral 是 build_failed，原因在突變定義（前置宣告寫在 Engine 的命名空間內，產生新型別遮住 SDL 的型別，Engine 本身編不過，沒走到 G2）。改成全域命名空間的前置宣告（可編譯，是真實的洩漏寫法），判定（G2 的訊息）不變，重跑這一條。m6a-g1、m6a-g2：2/2 killed。
- sanitizer 突變第一次執行（ASan 樹，基準線 8/8 通過、無 ASan 報告）：0/2 killed，兩條都是 wrong_failure，**宣告的判定不成立**。pd-filter-copy 被一般 CHECK 抓到（過濾器字串內容已錯，ASan 沒報），pd-mailbox-not-shared 是崩潰（鎖已釋放的 mutex，SIGABRT），沒有 ASan 報告。依 §14.1 不改預期重跑，交使用者決定。探索（不算驗收）：兩條在一般建置上各 5 次都被抓到（CHECK 失敗、SIGABRT），macOS 限定。
- xhigh 審查（2026-11-01）：blocker 1（SDL 3.4 在 Windows 對同一請求回呼兩次，舊碼會 use-after-free 與重複釋放；framework 現有實作同樣有此缺陷）、minor 4、nit 6。處理：待回覆請求改用程序級登記表與編號，第二次回呼找不到就返回；取答案配置失敗時放回；不合法種類與空 pattern 用 GYO_ASSERT；zenity 的空路徑算取消；launcher 型別加 noexcept；取答案只准主執行緒；Impl 先於視窗配置；拿掉觀察不到效果的關閉旗標。測試由 8 條增為 12 條，新增一條用 `SDL_HINT_FILE_DIALOG_DRIVER` 讓真正的 SDL launcher 在 CI 跑。
- 追加宣告（修正後、重跑前）：一般突變 13 條（原 9 條，pd-drop-answer、pd-take-keeps 錨點隨程式改；新增 pd-double-callback、pd-empty-path-chosen、pd-kind-assert、pd-real-launcher-save），全部要 killed、stale 0；m6a-g1、m6a-g2 重跑。
- 第三次執行（對抗式檢查修正後）：一般 16/16 killed，m6a 2/2 killed。
- 第二次執行（xhigh 修正後）：一般 13/13 killed，m6a 2/2 killed。
- Sonnet 對抗式檢查（2026-11-01）：major 6、minor 9。要使用者決定的：Windows 重複回呼的答案政策（第一次 Failed 之後是否交出第二個答案），以及 sanitizer 突變。其餘照做：真正 launcher 改經 `SdlDialogFunctions` 表（測試用假表檢查每種的參數，`ToSdlFileDialogType` 刪除）；答案政策先實作「Failed 之後最多再一個」（每個請求配兩個節點，回呼全程在登記表的鎖內）；補競爭、不去重、64 位元 tag、多檔取第一、無多餘文字、連續兩次 Failed 的測試（共 15 條，競爭測試連跑 30 次 0 失敗）；noexcept 加 static_assert；Linux 上不重設 hint。
- 追加宣告（第三次執行前）：一般突變 16 條（刪 pd-kind-map；pd-drop-answer、pd-order、pd-tag-lost、pd-double-callback、pd-real-launcher-save 錨點或預期隨程式改；新增 pd-third-callback、pd-launcher-many、pd-launcher-folder-window、pd-launcher-drop-filters），全部要 killed、stale 0；m6a-g1、m6a-g2 重跑。sanitizer 兩條等使用者決定，不跑。
- 2026-11-01 使用者核准後的宣告（sanitizer 選項 (1)）：pd-filter-copy 移到一般目錄，預期為 CHECK 失敗（`filters[0].name == "UI layout"`），在一般樹跑；pd-mailbox-not-shared 留在 ASan 目錄，預期為「ASan 的 heap-use-after-free、mutex lock failed 或崩潰」。ASan 樹先用目前的程式重建，基準線（選擇窗 15 條）要全過且沒有 ASan 報告，再跑這一條。兩條都要 killed。
- 使用者決定後的執行：pd-filter-copy 在一般樹 1/1 killed；ASan 樹以定稿程式重建，基準線 15 條全過、無 ASan 報告；pd-mailbox-not-shared 1/1 killed。第一次 ASan 突變的原始輸出被第二次覆蓋，未保存（結果記在本節）。
- 範圍的限界（帶到 T1 的事前宣告）：真實 OS 對話窗的回呼執行緒、Linux portal 需要 pump、平台或父視窗銷毀時的行為、真正的 SDL launcher（`ShowWithSdl`）在 CI 都不執行。

### v8-framework-guard-ratchet 的事前宣告（2026-11-01，執行前）

- 突變（D184：守衛腳本改了，所以跑全部 fw-guard-* 與新增的；fw-catalog-*、fw-path-dot-dot、fw-inbox-* 的檔案與測試都沒變，不跑）。全部要 killed，stale 為 0：
  fw-guard-input-target、fw-guard-tool-header、fw-guard-tool-name、fw-guard-executable（舊，判定不放寬的證據）；
  fw-guard-core-imgui-noslash、fw-guard-core-sdl-forward、fw-guard-core-native-include、fw-guard-core-sdl-call、
  fw-guard-core-json-link、fw-guard-allowlisted-file-one-more、fw-guard-allowlist-stale、fw-link-closure-sdl（新）。
  共 12 條。第一次執行 12/12 killed。
- 2026-11-01 追加宣告（獨立審查後、第二次執行前）：連結檢查補洞並改成點名，加 fw-link-trade、fw-link-genex，共 14 條，全部要 killed、stale 為 0。
  舊件檔改名為 `framework/Legacy.cmake`（核心 CMakeLists 的 nlohmann 也移進去），fw-guard-allowlisted-file-one-more、fw-guard-core-json-link、fw-link-closure-sdl 的錨點與預期訊息跟著改。
- 範圍的限界：綁定目錄還不存在，綁定規則只由自我測試涵蓋；綁定的突變在外殼批與 catalog 批補上。
- 舊自我測試對新守衛：只允許「乾淨的樹」一條失敗，且失敗內容只能是三項收緊。
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
- 2026-11-01，使用者：「照建議核准」。v8-engine-platform-path-dialog 的 [契約與 DI 條文](path-dialog-judgment-change.md)：Windows 重複回呼採 (a)「Failed 之後最多再一個答案」；sanitizer 突變採 (1)（pd-filter-copy 移一般目錄，pd-mailbox-not-shared 改預期在 ASan 樹重跑）；AD1（選擇窗能力與程序級登記表）；測試縫放 Native 標頭；platform-sdl.md 的契約、DI 10、DI 4／7 補句、Replacement Criteria 7～9。Replacement Criteria 2 的改寫延到外殼批。
- 2026-11-01，使用者：「核准」。v8-framework-guard-ratchet 的 [判定變更與 DI 條文](guard-ratchet-judgment-change.md)：framework DI 2、DI 3、DI 5、Replacement Criteria 改寫；守衛、例外清單、configure 連結檢查；突變目錄搬到本計畫。

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

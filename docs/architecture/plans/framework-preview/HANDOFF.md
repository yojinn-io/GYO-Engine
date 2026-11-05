# framework 的 3D 預覽元件：交接

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-framework-preview-component` | 整批：抽出 framework 的 3D 預覽元件，model_viewer 改用 |
| `v8-framework-preview-guard` | 守衛正向清單的變更（§14.4），單獨核准 |
| D-pv-1～4 | 待決：OrbitCamera 搬不搬、幀尺寸的給法、`Device()` 的型別、守衛的形狀（見 [README](README.md)） |

## 現況

- 2026-11-05 23:10 JST 開工（卡片由架構師會話開出；工作樹 `pvp-worker-ack-phase-91261b`，分支改名為
  `claude/v8-framework-preview-component`，基底是本機 master `54948874`；宣告寫的 `a9ccad7d`（#152 合併後的 origin/master）不對，開 PR 前併入 master）。主會話開工時是 Fable 5.1／high，和計畫表指定的實作者 Opus 5.5 不同；開工時沒有提出（應該照計畫提出，見下面使用者的話）。
- 讀過：AGENTS.md、兩份 agents 規則、PR #152 分支上 voxel 計畫的批次表與 HANDOFF 的三個決定（用 `git show`，沒有 checkout）、
  `docs/framework/architecture.md`（DI 1～6、Replacement Criteria、Using it）、`docs/framework/event-sounds.md`（契約文件範本）、
  `docs/architecture.md` Layers 節、2026-11-04 架構健檢的判斷 4、model_viewer 的 `PreviewRenderer.hpp`／`.cpp`、`ViewerWindow.cpp` 的預覽段、
  `PreviewMeasure.cpp`、`ModelViewerPreviewSmoke.cpp`、兩個守衛與自測、`framework/CMakeLists.txt`、`tests/framework/CMakeLists.txt` 的連結檢查、
  Engine 的 `RenderTypes.hpp`／`RenderDeviceTypes.hpp`／`RenderQueue.hpp`／`Renderer.hpp`／`Renderer.cpp` 的剔除段、
  `SdlGpuRenderDevice.hpp` 與 `SetFrameSize`／`ReadFrame` 的實作、`SdlRenderer.hpp`、既有突變目錄裡錨在本批會改的檔案上的條目、
  #146 的 P1 結果（`model-viewer-trial.md`、主 checkout 的 `p1-summary.txt`）。
- **Engine 缺口檢查：沒有缺口**（unlit、順時針為正面、剔背面都是 Engine 的預設），停下條件不成立。
- 既有突變裡會受影響的：`fw-guard-unlisted-engine-target`（例子用了 `GYO::RenderBackendSDLGPU`，要換例子）、`fw-shell-core-render`
  （依 D-pv-4 的選擇改預期或刪）、`mvp-stale-frame`（錨點搬走，改成 framework 的 `pv-stale-frame`）。
- 23:30 JST：事前宣告寫好（[README](README.md)：範圍、元件形狀與檢查 2 的對照表、D-pv-1～4、Architecture Delta 7 項、PV1～PV7、
  P1 重跑、守衛變更的 §14.4 預計內容、估時 7～9 h），**停在關卡 1，等核准。** 核准前不動程式碼。至今約 0.8 h（估 1 h）。

## 估時（開工前）

見 README 的「估時」：合計 7～9 h；超過 13.5 h 或單一問題累計 >2 h 就停下回報。
- 2026-11-05，使用者：「照建議核准」。事前宣告與 D-pv-1～4 都採建議（搬 `OrbitCamera`、尺寸由 `BeginFrame` 每幀帶、`Device()` 回傳 `SdlGpuRenderDevice&`、守衛改成每個核心 library 一張連結清單）。開始實作。
- 實作（主會話 Fable 5.1）：`Preview.hpp`／`PreviewTexture.hpp`／`PreviewCamera.hpp` 與對應 .cpp、`FrameworkError.hpp` 的 `PreviewErrorCode`、`framework/CMakeLists.txt` 的 `gyo_tool_framework_preview`、`tests/framework/PreviewTests.cpp`（PV1～PV6）與 CMake（`gyo_framework.preview` 標籤 gpu、`gyo_framework.preview_cpu` 標籤 cpu）、configure 時的存在檢查。守衛改成 (b)：核心清單加四個名字，另加每個核心 library 的連結清單；自測新增 8 條、修 1 條的預期（多一個發現），51/51。model_viewer 改用：`PreviewRenderer` 只留模型、骨架、`FrameModel`，`Render` 多帶尺寸；視窗、量測、smoke 改用 `Framework::`；守衛清單換成 `GYO::ToolFrameworkPreview`、拿掉 `GYO::Render`、`GYO::RenderBackendSDLGPU`（收緊），自測改 1 條、加 1 條。
- 突變目錄 `scripts/framework_preview_mutations.json` 17 條（預檢：每條 find 恰好一次）。既有突變改 5 條：`fw-guard-unlisted-engine-target`（例子改 `GYO::AudioBackendSDL`）、`fw-shell-core-render`（預期改成每 library 清單的訊息）、`mvp-stale-frame`（改錨到 framework 的 `EndFrame`，殺它的仍是 model_viewer 的 smoke）、`fw-guard-core-ui-dropped` 與 `es-guard-audio-backend-listed`（本批把守衛清單換行，錨點跟著改；意圖與預期訊息不變）。
- 契約 `docs/framework/preview.md`（DI 1～5、Replacement Criteria 1～4、檢查 2 的表）；`docs/framework/architecture.md` 只在 tool part 清單與 Using it 加用法；`docs/model_viewer/architecture.md` 的 Window 節改描述。
- 使用者把主會話切成 Opus 5.5（計畫表的實作者模型），並說：「領航員（模型）應該找規劃，而不是看我修改。如果計劃需要用 Fable，我同意。」本批的計畫三個角色都是 Opus，沒有要 Fable 的角色，所以照計畫用 Opus。之後的建置、測試、突變、審查與 P1 在 Opus 5.5 下做；前半段程式由 Fable 5.1 寫，由 Opus 5.5 的審查者審。
- 第一次建置與測試（Opus 5.5）：worktree 的 test 樹（RelWithDebInfo、`GYO_TOOLS=AUTO`）從頭建，成功；本批檔案沒有警告。全量 `cpu|shader` 125/125、`gpu` 4/4（含 `gyo_framework.preview` 與 `gyo_model_viewer.preview`，Metal）。
- 審査（Opus 5.5／high，子 Agent，唯讀）：0 blocker、3 major、4 minor、4 nit。處理：
  - M1 `fw-link-preview-missing` 在有 model_viewer gui 的樹上，model_viewer 的 FATAL_ERROR 先觸發：預期訊息加上它（比照 `fw-shell-binding-missing`）。
  - M2 每 library 清單讀不到變數或 genex 拼法：守衛改成也讀 `$<…>` 裡的名字、以變數定義 library 也算發現；另在 `tests/framework/CMakeLists.txt` 加 configure 時的檢查（核心與視窗外殼實際連到的、經介面帶進來的，沒有 `GYO::Render*`／`gyo_render*`，不看拼法）。自測加 2 條。
  - M3 model_viewer 守衛是它 DI 1 的守衛，§14.4 不論放寬與否都要 1～5 與使用者承認，而且加 `GYO::ToolFrameworkPreview` 是放寬面：併進關卡 2（宣告寫「收緊，事後報告」是錯的，更正）。
  - m4 PV6 沒有同時給 yaw 和 pitch：補一組、加突變 `pv-orbit-rotation-order`。
  - m5 宣告 PV7 列的 `fw-guard-preview-target-dropped` 沒有做，改成 `fw-guard-link-list-ignored`、`fw-guard-link-list-missing-ignored`（守的是規則本身，比「拿掉一個名字」強）；和宣告的差異記在 dev_log。
  - m6 契約與標頭寫明「`BeginFrame` 成功後一定呼叫一次 `EndFrame`」。
  - m7 `UploadFrame` 檢查 `Rgba8SRgb`，否則 `UploadFailed`；PV5 補案例、突變 `pv-upload-format-unchecked`。
  - nit：文件的回傳型別改成 `RenderQueue*`；preview 明確連 `GYO::PlatformSDL`（標頭用到它）；自測改名。`mvp-stale-frame` 錨在 framework 的檔（跨 owner）記為限界；守衛不讀 `#[[ ]]` 註解、`link_libraries()`、`set_property(LINK_LIBRARIES)` 記為限界（configure 時的檢查補上視窗外殼這一面）。
  - 審查後重建、framework 與 model_viewer 的 17 個測試通過；configure 時的新檢查在實際樹上通過。
- **突變的事前宣告**（預檢：每條 find 恰好一次）：
  - 本批 `framework_preview_mutations.json` 22 條（宣告 17＋審查後 5）。
  - D184 重跑 34 條：framework-rework 的 `framework_mutations.json` 18、`framework_shell_mutations.json` 5（`fw-shell-core-render`、`-core-native`、`-core-symbol`、`-binding-missing`、`fw-imgui-sdl-symbol`）、`framework_catalog_mutations.json` 2、event-sounds 2、model_viewer 7（`mv-guard-*` 5、`mvp-stale-frame`、`mvp-bone-no-pitch`）。選法：find 在本批改的檔案，或殺它的是本批改的守衛、自測、configure 檢查或 smoke。
  - 判定：56 條全部 killed、stale 0。依序跑（突變改工作樹，期間不改檔、不 configure 別的樹）。
- 突變結果（主 checkout `build/evidence/framework-preview/mutations_*.txt`）：本批 21/22 killed；D184 重跑 34/34 killed（framework 18、shell 5、catalog 2、event-sounds 2、model_viewer 7）；stale 0。
  - `fw-guard-window-render-genex` 是 wrong_failure。重現（會話暫存區的複製樹，未保存）：守衛報的是 `gyo_tool_framework_window links "$<BUILD_INTERFACE:GYO::RenderBackendSDL>"`。突變把 genex 寫在引號裡，守衛把引號開頭的 token 當一般名字，連引號一起報出來。守衛有擋住（不是漏檢）；預期訊息沒有考慮引號，守衛也沒去掉 CMake 的引號。同一個原因也讓「引號裡的清單內名字」（例如 `"GYO::Os"`）被誤報。
- P1 重跑（原宣告，試 1 次，主 checkout `build/evidence/framework-preview/p1/`）：**通過**。件數の突き合わせ：一致（每個模型 600 筆，合計 2400）。p95：`ual1_mannequin` 7.454、`player_v20` 5.252、`nailer_v2` 5.663、`nailer_v2.world` 4.067 ms（門檻 16.7）。#146 是 5.957／4.726／5.214／4.150。mannequin 多 1.5 ms，試行 1 次無法分辨是變更還是機器狀態，不當判定。Intel i9-9980HK、macOS 26.7.1、RelWithDebInfo、Metal。
- 守衛的 Red／Green（`guard_red_green/`）：framework 舊守衛跑新自測 53 條中 10 條失敗、新守衛 53/53；model_viewer 舊 2 條失敗、新 15/15。判定變更文件 [guard-judgment-change.md](guard-judgment-change.md) 寫好了，検証者還沒開。
- **停止（§14.1：宣告的突變判定不成立，56 條中 55 條 killed）。** 修正與重跑前等使用者決定。未提交。至今約 0.8 h（不含等待），估 7～9 h。
  - 參考（未套用）：守衛判斷連結 token 前先去掉前後的引號（CMake 的 `"X"` 就是 `X`）；自測加「引號裡的 genex」與「引號裡的清單內名字」兩條；突變定義不變。然後只重跑這條和錨在那幾行的 `fw-guard-link-list-ignored`、`fw-guard-link-genex-ignored`，再開検証者。去掉引號會讓「引號裡的清單內名字」從失敗變通過，這是放寬面，寫進判定變更文件一起核准。
- 2026-11-06，使用者：「照參考修法」。守衛判斷連結 token 前先去掉前後的引號；自測加 1 條（引號裡的 genex、引號裡的清單內名字、引號裡的清單外名字）。判定變更文件寫明新規則內的這一面。
- **重跑的事前宣告**（預檢：每條 find 恰好一次）：`fw-guard-window-render-genex`（定義不變）、`fw-guard-link-list-ignored`、`fw-guard-link-genex-ignored`（錨在改動的那幾行），3 條全部 killed、stale 0。第一次的結果保留在 `mutations_preview.txt`。
- 重跑 3/3 killed（`mutations_rerun_quotes.txt`）。本批 22/22、D184 34/34。守衛 Red／Green 更新：framework 舊 54 條中 11 條失敗、新 54/54。
- dev_log 寫好，列進 README（未提交）。
- 検証者（Opus 5.5／high）：A1、A2、B1～B5 確認；A3、A4、A5 部分確認。沒有文件沒寫到的放寬。
  - 文件把收緊與 configure 檢查的涵蓋說過頭。抓不到的寫法：第一個 token 加引號或用變數；genex 裡的一般名字；`add_library` 沒有來源檔、名字加引號或含 `-`；configure 檢查不讀 `INTERFACE` 連結、不跟進 genex 裡具名的 target（例如 `$<BUILD_INTERFACE:gyo_tool_framework_preview>`，一行就能讓視窗外殼經 preview 拿到 render）。這些在 master 上也都通過，不是放寬。
  - 數字過時（53／10 → 54／11、9 → 8 條）、沒提 wrong_failure；B4 漏了 `GYO::RenderBackendSDLGPU` 從 PRIVATE 變成經 preview 的 PUBLIC。
  - 已照實改寫判定變更文件（限界、第 4 項、數字、検証者的結論）。
- **停在關卡 2**：守衛變更的核准，以及要不要補強 configure 檢查（跟進 genex 裡具名的 target、也讀 `INTERFACE_LINK_LIBRARIES`）。未提交。
- 2026-11-06，使用者：「先補強再核准」（Patch, then approve）。configure 時的檢查改成：也讀目標自己的 `INTERFACE_LINK_LIBRARIES`（INTERFACE 關鍵字的連結）；genex 裡具名的 target 也跟進它的介面。突變加 2 條：`fw-link-window-preview-genex`（`$<BUILD_INTERFACE:gyo_tool_framework_preview>`）、`fw-link-window-render-interface`（INTERFACE＋變數）。
- **重跑的事前宣告**（預檢：每條 find 恰好一次）：新 2 條＋以 configure 失敗判定、殺手是本檔的既有突變 8 條（`fw-link-preview-missing`、`fw-link-preview-native`、`fw-link-window-render`、`fw-link-closure-sdl`、`fw-link-genex`、`fw-shell-binding-missing`、`fw-cat-json-missing`、`es-link-model-json-public`），10 條全部 killed、stale 0。另外：補強後的 configure 在實際樹上要通過（跟進 genex 後 Engine 合法的連結不被誤判；被誤判就停下回報），framework 與 model_viewer 的測試全過。
- 補強後：重新 configure 在實際樹上通過，framework 與 model_viewer 的 17 個測試全過；突變 10/10 killed（`mutations_patch_*.txt`）。判定變更文件的第 3、4 項改成補強後的狀態。請検証者重看第 4 項。
- 検証者第 2 輪（Opus 5.5，`build/evidence/framework-preview/verifier/round2/`）：A3 的 configure 部分與 A4 **不成立**。
  - 補強修好了第 1 輪的漏洞：genex 裡具名的 preview、INTERFACE 關鍵字、巢狀 genex、`$<LINK_ONLY:preview>` 都抓得到（W05～W07、W09、W17、W23～W25）。
  - 補強也造成**回歸**：`seen` 記的是 entry 字串，跟進前又拿實名比對 `seen`；用實名直接連的 target 因此永遠不跟進介面。
    - 第 1 輪抓得到的 W10、W14、W16 漏掉。
    - 新的漏洞：W21、W22（經變數用實名連 preview，Python 守衛也跳過 `${…}`，兩邊都擋不住）、W26、W27、W29、W33。
  - 我的突變都走別名或 genex，所以 10/10 沒有發現；實際的樹也照常 configure。
  - 誤判風險低：engine／framework 的 CMake 沒有 `$<`；`$<$<BOOL:0>:…>` 這類條件會被保守地當成連結（不影響現在的樹）。
- **停止（AGENTS §14 冒頭：發現回歸；同一個判定（configure 時的 render 檢查）第二次改方法）。** 未提交。補強那一輪約 0.3 h，整批至今約 1.3 h（估 7～9 h）。
  - 參考（未套用）：`seen` 分成「看過的 entry」和「展開過的 target 實名」兩張清單。加突變「視窗經變數用實名連 `gyo_tool_framework_preview`」。重跑 configure 失敗判定的 11 條，再請検証者看第 3 輪。
- 2026-11-06，使用者：「Fix and re-verify」。`seen` 拆成 `read_entries`（看過的 entry）與 `expanded`（跟進過介面的 target 實名）。突變加 `fw-link-window-preview-variable`（視窗經變數用實名連 preview；Python 守衛跳過 `${…}`，只有 configure 檢查殺得掉）。
- **重跑的事前宣告**（預檢：每條 find 恰好一次）：以 configure 失敗判定的 11 條（本批 6：`fw-link-preview-missing`、`-preview-native`、`-window-render`、`-window-preview-genex`、`-window-render-interface`、`-window-preview-variable`；既有 5：`fw-link-closure-sdl`、`fw-link-genex`、`fw-shell-binding-missing`、`fw-cat-json-missing`、`es-link-model-json-public`）全部 killed、stale 0；實際的樹 configure 通過、framework 與 model_viewer 的測試全過。之後請検証者第 3 輪用第 2 輪的探針（W01～W33）重看。
- 修正後：用検証者第 2 輪的探針自己試（`build/evidence/framework-preview/round3-self/`）：33 個案例只剩 W08（文件寫的限界）、W28（沒有 render 的環，本來就通過）、W32（誤判檢查）沒抓，其餘都抓到。實際的樹 configure 通過、17 個測試全過；11 條突變 11/11（`mutations_fix_*.txt`）。請検証者第 3 輪。
- 検証者第 3 輪（`verifier/round3/`）：A3（configure 部分）、A4 部分確認。回歸已修，文件列的寫法都確認；另找到文件沒寫的兩種繞法：名字含 `-` 或 `.` 的輔助 target（Python 與 configure 兩邊都擋不住）、經變數的 archive 路徑或 `-l` 旗標。master 上也都通過，不是放寬。
- configure 檢查已改了兩次方法，不再追第三次（§14 冒頭），兩種繞法照實寫進判定變更文件的限界與第 4 項；要不要補記為之後的候選。
- **停在關卡 2**：守衛變更的核准。未提交。
- 2026-11-06，使用者：「Approve」。守衛的判定變更（A、B，含限界）核准。下一步：併入 master、commit、開 PR、通知 voxel 會話與架構師會話。
- 2026-11-06：commit `e49f0752`，併入 origin/master `a9ccad7d`（#152，帶進 voxel_map_editor；沒有衝突，`sync_with_base_branch` 因 master 有 `.gitattributes` 拒絕，內容只有 `* -text`，自行 merge）。合併後的樹：兩個守衛通過、全量 `cpu|shader` 130/130、`gpu` 4/4（`build/evidence/framework-preview/*_after_merge.txt`）。開 PR，L1 的結果在 PR 上看。
- 2026-11-06：PR yojinn-io/GYO-Engine#157。L1 run 42067582426：linux-x64、macos-arm64 通過；windows-x64 編譯失敗，所以 CI gate 失敗。
  - 原因：`tests/framework/PreviewTests.cpp` 的變數 `far` 和 `windef.h` 的巨集撞名（MSVC C2513）。這是我寫的測試的錯（§14.1：Test 自身），和 voxel 線在 #152 遇到的相同。
  - 修正：改名為 `distant`，判定不變。本機 `gyo_framework.preview*` 2/2；殺手是 PV6 的 4 條突變重跑 4/4（`mutations_ci_fix_pv6.txt`）。
  - 第一次的失敗 log 在主 checkout 的 `build/evidence/framework-preview/ci/l1_windows_failed.log`。

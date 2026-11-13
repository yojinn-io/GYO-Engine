# 外部組件改成 adapter plugin 的規劃（統合稿）：`v8-arch-adapter-plugins-plan`

件数の突き合わせ：engine 頂層模組 **23＝23**（事前宣告與 `ls engine` 一致）；third_party **10＝10**（一致）；建置期外部組件 **3**（事前宣告寫 2——**不一致**：偵察 A 多查出 SPIRV-Cross，`engine/render/shaders/pipeline/cmake/Dependencies.cmake` 的 `gyo_spirvcross`、`src/main.cpp` 含 `<spirv_cross_c.h>`；不是判定失敗，三份提案的重數都是 3）；歸類表主列 **36＝23＋10＋3**（一致），另有後端子列 7 與建置期工具子列 1；後端 **9＝9**（一致：7 個子列＋本身就是後端的 platform、net 兩個主列）；受影響批：既有 **17**＋新增 **2**（偵察 B 數 17；本稿既有批也是 17，**數目一致、組成差一處**：偵察 B 列了 wrap-assets，本稿判它不變；本稿另列 closeout（過渡登記表與含入棘輪的歸零判定））。

- 狀態：**統合稿（待敵對檢查與使用者確認）**。日期 2026-11-13。Owner：架構（架構師會話）。
- 性質：ultracode 小編制的統合（事前宣告 `adapter-plugins-plan-declaration.zh-Hant.md` §4）：偵察 Opus／high ×2、提案 Opus／high ×3（①風險與可逆、②價值、③成本）、統合 Opus／high（本稿）；敵對檢查 Fable／high 一次在本稿之後。
- 基準：工作樹 `GYO-Engine-arch`，分支 `claude/arch-adapter-plugins-plan`，HEAD `fd012f4b`。偵察與三份提案的原文在主 checkout `build/evidence/adapter-plugins-plan/`（`recon-engine.md`、`recon-plan.md`、`proposal-{risk,value,cost}.md`、`proposal-risk-scan.{py,txt}`；git 管理外）。
- 已定前提（不重開）：kernel `HANDOFF.md` 決定行 `v8-arch-external-components-as-adapters`（立場、三條原則、方向 C、plugin 兩類、細節六題 (a)～(f)）、`v8-arch-engine-or-plugin-criteria`、`v8-arch-plugin-selection-data`、`v8-arch-framework-retire`、`v8-arch-docs-scope-of-target`、`v8-arch-architecture-entry`。本稿的「建議」都交使用者；「推定」＝無出處的估計；「未查證」＝本次沒讀到能確認的出處。
- 本稿改的其他檔：`migration-plan.zh-Hant.md`（文末「修訂記錄（v8-arch-adapter-plugins-plan）」列出改了哪些段）、`target/runtime-composition.zh-Hant.md` §4（本文；Design Invariants 節不動）。入口 `docs/architecture/README.md` 在本工作樹不存在（只在未合併的 `origin/claude/librarian-architecture-entry`），**產出 6 未做**，見 §4 末「流程事項」。

## 0. 結論（先寫）

1. **Feature 2、3 在 plugins-foundation 段 (i) 交付，不必先搬任何後端程式碼**（三案一致）：選擇欄一次升到 version 2（components 併入、刪除），可執行單位（app 的 role、tool 的 variant）各自一份選擇；還在 Engine 裡的後端以「過渡登記表」掛上最終的 plugin 名，之後每個 adapter 搬家只改登記，產品資料不再動（§3）。
2. **Feature 1 的「升級」靠型別隔離今天已大致成立**：9 個後端的非 native 公開標頭含入外部組件 0 件、型別 0 件（§2）。要改的地方在 Engine 外：三個 `*Native` target、產品與工具直接含入 asio／httplib／protobuf／ImGui／SDL、消費者直接寫具體後端類名；以及 platform、輸入注入沒有中性契約。
3. **搬法**：ufbx 由 assets-core 經過渡登記成為唯一的 ufbx adapter（刪掉計畫裡「pvp 最小 ufbx plugin → model-assets」那一次替換）；`engine/net` 併進 net-link；**SDL 家族與 ufbx 的程式碼在 G2a 之後由一個新的只搬不改形狀的批 `v8-arch-adapters-relocate` 一次搬進 `plugins/`**，排在 wrap-render 之前，wrap-render 再在 plugins 裡改形狀（§5 取捨 T4）。
4. **計畫合計 632～1,299 → 665～1,368 h**（+33～69，全為推定）；非條件批 33 → 34，條件批 1 → 2（新增條件批 `v8-arch-json-adapter`，只在使用者選 JSON 的 J3 時開）；**關鍵路徑 331～692 → 332～694 h**（只有 assets-core +1～2 在路徑上）。不觸發 RC-8（Delta-7 算第 1 個時）與 RC-11。
5. **受影響的批**（計畫修訂記錄逐段列出）：既有 17 批＝plugins-foundation、assets-core、match-sim、net-link、diagnostics-service、match-host、wrap-render、plugins-frame、client-link、client-frame、plugins-editor、tool-voxel、tool-model-viewer、tool-audio、tool-ui、framework-delete、closeout；新增 2 批＝adapters-relocate、json-adapter（條件）。估時有變的是其中 8 批＋新批（§2.2 修訂列）。
6. **交使用者的未決 13 題**（§4）：事前宣告 §3 的三題，加盤點中新出現的十題；其中 Design Invariants 的增改（新 DI 5、DI 6 等）是 Architecture Delta，不在本批寫進 target。

---

## 1. 模組歸類表（產出 1）

**判斷順序**（`v8-arch-engine-or-plugin-criteria`）：**Q1** 沒有它，Engine 是否成立？不成立→留 Engine。成立時看 **Q2a** 有沒有對外的介面、**Q2b** 是固化的基礎還是會變、會被替換的規則；無對外介面且固化→留 Engine，否則→plugin。外部組件另受 `v8-arch-external-components-as-adapters` 的三原則與細節 (e)(f) 約束。

**Q2a 的讀法（未決 4，三案一致的讀法，交使用者確認）**：「對外介面」＝面向 Engine 以外的系統（第三方組件、OS 裝置、網路、外部行程）的介面；Engine 自己定給 plugin 實作、或給產品呼叫的中性 C++ 介面不算。根據：使用者判 collision、math「沒有對外介面」，而兩者都有公開 C++ API（`gyo_collision`、`gyo_math`）；若讀成「C++ 公開 API」，幾乎所有模組都會被推成 plugin，與已定歸屬矛盾。

**推論 R3**（原則 (3)「adapter 的對外介面由 Engine 定」的推論）：承載第 1 類後端所實作之中性介面的模組必須留 Engine；否則介面由 plugin 定。

**停止條件的檢查**：36 列套準則後，**0 列**與已定歸屬相反（collision、math 留；網路含 `engine/net` 是 plugin；SDL、SDL_image、SDL_ttf、ufbx 第 1 類；ImGui 第 2 類）。有四處是原則與現況形狀之間的張力（不是歸屬矛盾），列為未決：nlohmann（未決 1）、ImGui 的工具 widget 呼叫（未決 7）、protobuf generated code（未決 8）、target §4 的 Q2 句（未決 10）。

### 1.1 engine/ 的 23 個模組

| # | 模組 | Q1 | Q2a | Q2b | 歸屬 | 已定／建議 | 證據（檔＋符號） |
|---|---|---|---|---|---|---|---|
| 1 | asset（core） | 不成立：世界內容只經 Loading 的資產階段（runtime-composition DI 3）；`Assets<T>`、請求序號是內容身分 (c) 的契約 | — | — | 留 Engine（R3：`IAssetLoader` 由第 1 類實作） | 建議（未決 13） | `engine/asset/include/engine/asset/loading/IAssetLoader.hpp`；`engine/asset/src/ContentManifest.cpp` `ContentManifest::Parse`；計畫 §3.19 |
| 1a | └ asset 的 sdl_image loader | 成立 | 有（SDL_image） | 會被替換 | **第 1 類**：SDL_image plugin，宣告依賴 SDL plugin | 已定 (e) | `gyo_asset_sdl_image`（`engine/CMakeLists.txt`，`if(TARGET SDL3_image::SDL3_image)` 內）；`SdlImageTextureLoader.hpp` 位於 `GYO::Engine` 的公開 include 目錄 |
| 2 | audio（core：`Mixer`、`Submitter`、`ClipBank`） | 成立（Match 不用） | 無（輸出在後端） | 整數混音是設計（audio.md DI 8、9）；bus／effect 是 RC 的替換點 | 留 Engine（R3：SDL 音訊輸出實作的介面要由 Engine 定） | **待使用者**（未決 3；〈未定〉(1) 候選） | `engine/audio/include/engine/audio/Mixer.hpp` `Mixer`、`Submitter`；`tests/common/audio/CharacterizationTests.cpp`（跨平台錄製雜湊，label `cpu`）；`gyo_expect_audio_links` |
| 2a | └ audio/backend/sdl | 成立 | 有（SDL） | 會被替換 | **第 1 類**：SDL plugin 的音訊輸出部分 | 已定 (e) | `gyo_audio_sdl` `SdlAudioOutput::Open(Mixer&&, …)` |
| 3 | base | **不成立** | — | — | 留 Engine | 已定（準則明列 Base） | `gyo_base`（INTERFACE）；`engine/base/include/engine/base/StrictJson.hpp` `ParseStrictJson`（只以模板參數接 nlohmann，見未決 1） |
| 4 | collision | 成立 | 無 | 固化 | 留 Engine | **已定** | 準則決定行；`gyo_collision` |
| 5 | config | 不適用（不是程式模組） | — | — | 不適用：`projects.csv`、`tools.csv` 是 Product Registration 資料，留原處 | 建議 | `engine/config/`；讀者 `build/cmake/GyoApps.cmake`、`GyoTools.cmake` |
| 6 | entity_store | **不成立**（World 欄的實作，Delta-3） | — | — | 留 Engine | 建議（由 Delta-3 推出） | `gyo_entity_store`；計畫 §4 Delta-3 |
| 7 | input（core：`PhysicalInputFrame`、`InputActionMap`） | 成立（Match 不用） | 無 | 固化：後端要填的 Engine 詞彙 | 留 Engine（R3） | 建議（未決 13） | `engine/input/include/engine/input/PhysicalInputFrame.hpp`；`gyo_input` 只 PRIVATE 連 `GYO::Base` |
| 7a | └ input/backend/sdl（含 native） | 成立 | 有（SDL） | 會被替換 | **第 1 類**：SDL plugin 的輸入部分 | 已定 (e) | `gyo_input_sdl` `SdlInput`、`SdlInputInjector`；`gyo_input_sdl_native` `HandleNativeEvent` |
| 8 | io | 不成立：`IFileSystem`、`IStream` 是資產載入的詞彙，`Assets<T>` 依賴它；若成 plugin，Engine 的 asset 就要 include plugin（違 Delta-2 第 5 項） | 對 OS 有，但已經 `engine/os`；無第三方 | — | 留 Engine（介面與同步實作）；IO 執行緒服務已是 plugin（計畫 §3.11 asset-io） | 建議（未決 13） | 併入 `GYO::Engine`；`IFileSystem`、`IFileWatcher`、`IStream`；計畫 §3.19 `WriteFileAtomically` |
| 9 | kernel | **不成立** | — | — | 留 Engine | 已定（Delta-1） | `gyo_kernel`；target/kernel DI-KS 15 |
| 10 | map_markers | 成立 | 無（nlohmann 是 PRIVATE） | 固化：契約資料 `gyo.map_markers` 的解釋；literal-bit golden 是 (c) 守衛（determinism §4.6） | 留 Engine | 建議（未決 13） | `engine/map_markers/src/MapMarkers.cpp` 經 `Base::ParseStrictJson` |
| 11 | math | 成立 | 無 | 固化 | 留 Engine | **已定** | 準則決定行；`gyo_math` |
| 12 | model（core：`ModelAsset`、`AnimationEvents`、取樣） | 成立 | 無 | 取樣決定命中幾何（determinism §4.2，今天 (b)、同平台 golden）；是否升 (c) 在暫停中的 DET-1／E1 | 留 Engine | **待使用者**（未決 3；〈未定〉(1) 候選） | `engine/model/include/engine/model/ModelAsset.hpp`；`engine/model/src/AnimationEvents.cpp`；`gyo_expect_model_links` |
| 12a | └ model/backend/ufbx | 成立 | 有（ufbx） | 會被替換 | **第 1 類**：ufbx plugin | 已定 (e) | `gyo_asset_ufbx` `UfbxModelLoader`（PRIVATE 連 `gyo_ufbx`）；消費者 `apps/object_fps_pvp/src/Sim/PlayerAnimationContent.cpp`（Match）與 `src/Pvp/PvpApplication.cpp`（Client）都直接 `loaders.Register(std::make_unique<…UfbxModelLoader>())` |
| 13 | net | 成立（只有 object_fps_pvp 用） | 有（wire 的資料框） | — | **第 2 類**：網路 plugin，與 asio／httplib／protobuf 同一個 plugin | **已定 (f)** | `engine/net/include/engine/net/GyopDatagram.hpp` `EncodeGyopDatagram`、`DecodeGyopDatagram`；`gyo_net` 只連 `GYO::Base`；消費者 `apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp` |
| 14 | os | **不成立** | — | — | 留 Engine | 已定（準則明列） | os.md DI 1～3 |
| 15 | platform（今天只有 `sdl/`；本身就是後端） | 成立（Kernel 不需要視窗） | 有（視窗、事件） | 會被替換 | **第 1 類**：SDL plugin 的核心；**Engine 要新立中性的 platform 契約**（今天不存在，§2） | 已定 (e)＋建議（中性契約） | `engine/platform/CMakeLists.txt` 只有 `add_subdirectory(sdl)`；`SdlPlatform`、`PlatformEvent`、`PathDialogRequest`、`SdlLogBridge`、`GyoMain`（`gyo_platform_sdl_main`） |
| 16 | render（core：`IRenderDevice`、`Renderer`、`ShaderLibrary`、`gyo_model_renderer`） | 成立（headless Match 不畫） | 無（GPU 在後端） | 固化：`IRenderDevice` 是 Engine 的詞彙；渲染 World 的組合規則在 kernel DI-KS 13 | 留 Engine（R3） | 建議（未決 13） | `engine/render/include/engine/render/IRenderDevice.hpp`；`engine/render/src/ShaderLibrary.cpp` |
| 16a | └ render/backend/sdl（2D，含 native） | 成立 | 有（SDL_Renderer） | 會被替換；可能整個退役 | **第 1 類**：SDL plugin 的 2D 部分；去留見未決 9 | 已定 (e) | `gyo_render_backend_sdl` `SdlRenderer`（未實作 `IRenderDevice`）；`SdlRendererNative.hpp` |
| 16b | └ render/backend/sdl_gpu | 成立 | 有（SDL_GPU） | 會被替換 | **第 1 類**：SDL plugin 的 GPU 部分 | 已定 (e) | `gyo_render_backend_sdl_gpu` `SdlGpuRenderDevice final : public IRenderDevice` |
| 16c | └ render/shaders/pipeline（建置期工具，組件見 34～36） | 不進執行檔 | — | — | 留 `engine/render`（三原則的範圍寫成「執行期連結的組件」） | **待使用者**（未決 2） | `gyo_shader_tool`；`build/cmake/GyoShaders.cmake` `_gyo_require_shader_tool` |
| 17 | runtime（`RuntimeLoop`、`IRuntimeClient`） | — | — | — | 不歸類：runtime-loop-retire 刪除（計畫 §3.31）；`FixedTickRuntime` 進 runner plugin | 已排程 | `engine/runtime` 併入 `GYO::Engine` |
| 18 | text（core：`ITextRasterizer`） | 成立 | 無 | 固化（Engine 詞彙） | 留 Engine（R3） | 建議（未決 13） | `engine/text/include/engine/text/ITextRasterizer.hpp` |
| 18a | └ text/backend/sdl_ttf | 成立 | 有（SDL_ttf） | 會被替換 | **第 1 類**：SDL_ttf plugin，宣告依賴 SDL plugin | 已定 (e) | `gyo_text_backend_sdl_ttf` `SdlTtfTextRasterizer::Create` |
| 19 | threads | **不成立** | — | — | 留 Engine | 已定（準則明列） | `gyo_threads` |
| 20 | time | **不成立** | — | — | 留 Engine | 已定（準則明列） | `gyo_time` |
| 21 | trace（core：記錄格式、sink） | 成立 | 寫檔經 `engine/os` | 記錄格式會變 | 留 Engine（core）；寫檔服務已是 plugin（計畫 §3.13 trace-diagnostics，`engine/trace` 不改） | 建議（未決 13） | `gyo_trace`（PUBLIC `GYO::Threads`、PRIVATE `GYO::Os`） |
| 22 | ui（文件模型、`gyo.ui` codec、`UiRenderer`） | 成立 | 無（nlohmann 是 PRIVATE） | `gyo.ui` 是與 ui_editor 共用的資料契約、Engine 是 owner；`UiRenderer` 在 wrap-assets 重寫 | 留 Engine（wrap-assets 同批再搬只會擴大變動面） | 建議（未決 13） | `engine/ui/src/UiDocumentCodec.cpp`、`UiSerialization.cpp`；`gyo_ui_renderer` |
| 23 | voxel_mesh | 成立 | 無 | 固化：`gyo.voxel_mesh` 等契約；決定碰撞世界與 `ArenaContentDigest`；literal-bit golden (c)（determinism §4.6） | 留 Engine | 建議（未決 13） | `engine/voxel_mesh/src/VoxelMesh.cpp` 經 `Base::ParseStrictJson` |

### 1.2 third_party 的 10 個與建置期 3 個

| # | 組件 | 歸屬 | 已定／建議 | 現況的 adapter 位置與要處理的事（檔＋符號） |
|---|---|---|---|---|
| 24 | asio | **第 2 類**：網路 plugin 的內部實作 | 已定 (f) | 今天在產品：`apps/object_fps_pvp/src/Pvp/IpcHost.cpp`、`ClientConnection.cpp`；`third_party/asio` 的 `add_subdirectory` 寫在 `apps/object_fps_pvp/CMakeLists.txt` |
| 25 | doctest | 不適用（只在測試：tests 154 檔、spikes 2 檔；不進 Engine、plugin、產品執行檔） | 建議（未決 6） | `doctest::doctest` 的連結者只有 tests 與 spikes |
| 26 | httplib | **第 2 類**：網路 plugin 的內部實作 | 已定 (f) | 今天在產品：`ClientConnection.cpp` |
| 27 | imgui | **第 2 類**：imgui plugin | 已定 | `framework/bindings/imgui` `ImGuiShell.cpp`；工具 9 檔直接含 `<imgui.h>`（audio_editor 1、model_viewer 1、ui_editor 6、voxel_map_editor 1）；`imgui::sdl3_sdlrenderer3` PUBLIC 連 SDL3（ImGui 的 SDL glue＝第二處 SDL 呼叫）；工具的 widget 呼叫見未決 7 |
| 28 | nlohmann_json | **待使用者**（未決 1） | — | engine 7 模組 9 個 `.cpp`（全部 PRIVATE；公開標頭 0 件）＋`StrictJson.hpp` 的 API 形狀依賴；apps 7、tools 17、framework 2、tests 20、acceptance 6 檔 |
| 29 | protobuf | **第 2 類**：網路 plugin 擁有執行期函式庫的加入、版本與 protoc 規則；產品 generated code 的歸屬見未決 8 | 已定 (f)＋待使用者 | `IpcHost.cpp` 含 `google/protobuf/unknown_field_set.h`；`runtime_v9`／`client_v9` 的 `.pb.cc` 由 `protobuf::protoc` 在產品建置期產生 |
| 30 | sdl3 | **第 1 類**：SDL plugin（一個 adapter；內含核心、視窗、輸入、2D、GPU、音訊輸出等後端 target——讀法見未決 5） | 已定 (e) | 今天分散在 `engine/{platform,input,render,audio}` 的 8 個 target（含 3 個 `*Native`）；engine 外直接含入：`tools/ui_editor/src/AssetPreviewContext.cpp`、tests/common 10 檔 |
| 31 | sdl3_image | **第 1 類**：SDL_image plugin，宣告依賴 SDL plugin | 已定 (e) | 只有 `engine/CMakeLists.txt` 連它 |
| 32 | sdl3_ttf | **第 1 類**：SDL_ttf plugin，宣告依賴 SDL plugin | 已定 (e) | 只有 `engine/text` 與 `engine/text/backend/sdl_ttf` 連它 |
| 33 | ufbx | **第 1 類**：ufbx plugin | 已定 (e) | 只有 `engine/model` 連它；Match 經 `player_content` 的 PRIVATE_LIBRARIES 直接連 `GYO::AssetUfbx`（繞過 component 名） |
| 34 | SDL_shadercross（建置期） | 留在 `gyo_shader_tool`（工具本身就是這三個組件唯一的 adapter） | **待使用者**（未決 2） | `Dependencies.cmake` 的 `gyo_shadercross`；`gyo_host_shadercross` |
| 35 | SPIRV-Cross（建置期） | 同上 | 同上 | `gyo_spirvcross`；`main.cpp` 含 `<spirv_cross_c.h>` |
| 36 | DXC（建置期） | 同上 | 同上 | `gyo_dxc`／`gyo_dxc_source`；只以連結庫出現（含入樣式全 repo 0 件，無正例——偵察 A 前言） |

---

## 2. 中性介面盤點（產出 2）

**掃描**（偵察 A §3；風險提案以正例重驗；本統合重驗正例與下文引用的幾項：主 checkout `build/evidence/adapter-plugins-plan/integrator-scan.{py,txt}`，git 管理外；計畫估時的重算在同目錄 `integrator-sum.{py,txt}`）：

- 指令：`git grep -n -P` 以 Python 包起來，回傳碼 0＝命中、1＝無命中、其他＝中止；stderr 不丟。
- 正例：`#\s*include\s*[<"]SDL3/` 在 `engine/platform/sdl/native` 命中 1 筆（`SdlPlatformNative.hpp` 的 `#include <SDL3/SDL.h>`）；外部型別記號 `\b(SDL_\w+|TTF_\w+|IMG_\w+|ufbx_\w+|nlohmann|asio)\b` 在三個 native 標頭共命中 22 處。
- 對 9 個後端的**公開、非 native** 標頭：含入樣式 `#\s*include\s*[<"](SDL3|SDL3_image|SDL3_ttf|ufbx|nlohmann|asio)` **0 件**；型別記號 **0 件**，只命中 2 處註解（`SdlGpuRenderDevice.hpp`「SDL_GPU implementation…」、`SdlTtfTextRasterizer.hpp`「Optional SDL_ttf implementation…」）。
- **結論**：「升級」所需的型別隔離在非 native 標頭已成立。不乾淨的是 (a) 三個 `*Native` target 把 SDL 交給 framework、tools、tests；(b) 消費者直接寫具體後端類名（「替換」不成立）；(c) platform 與輸入注入沒有中性契約、2D renderer 沒有中性介面。

| 後端 | 公開標頭乾淨 | Engine 內的中性介面 | 要改的符號與去向 | 批 | 估時（推定） |
|---|---|---|---|---|---|
| platform/sdl | 是（`class NativeAccess;` 前置宣告；`PlatformEvent` 以不透明指標持有事件） | **無**；消費者直接寫 `Engine::Platform::Sdl::SdlPlatform`（engine 外：apps 3、tools 11、framework 6、tests 29、acceptance 6 檔） | 在 `engine/platform` 新立**中性 platform 契約**：`PlatformEvents` boundary 的型別、建窗、把事件抽進 boundary、live frame 掛鉤、剪貼簿、路徑對話框請求／回覆（`PathDialogRequest`／`PathDialogAnswer` 今天已不含 SDL，可改名搬入）、log 橋接、行程入口。**不做鏡像 `SdlPlatform` 的大類別**（三案一致：遷移後產品不直接碰 platform，鏡像類別在遷移完成時變死介面）；形式（抽象類、函式表或 Resource＋System）由 wrap-render 的事前宣告定。`NativeWindow()`／`NativeEvent()` 只給 SDL 家族與 SDL plugin 自己的測試 | 介面：wrap-render；搬家：adapters-relocate | 4～8（風險、價值兩案同值；成本案 2～4 計在 audit platform 之內） |
| input/backend/sdl | 是 | 輸出資料中性（`PhysicalInputFrame`、`InputEvent`）；沒有「來源」介面 | 來源不需要類別介面：SDL plugin 的 pump 填 boundary；`SdlInputInjector`（tests 6、acceptance 2 檔用）需要中性的注入名，或留在 SDL plugin 的測試支援（二選一由 wrap-render 宣告定）；與 `SdlPlatform&` 的配對留在 SDL plugin 內部 | wrap-render | 0～4 |
| render/backend/sdl（2D） | 是 | **無**；`SdlRenderer` 不實作 `IRenderDevice` | 使用者只有 ImGui 的 SDL 繪製（`framework/bindings/imgui` 經 `RenderBackendSDLNative`）與 `framework/include/gyo/framework/PreviewTexture.hpp` `UploadFrame`（readback 上傳，隨 readback 退役）；工具 4 檔、tests 7 檔。**先不設計 2D 中性介面**，去留見未決 9 | plugins-editor | I1 1～2；I2 4～8 |
| render/backend/sdl_gpu | 是 | `IRenderDevice` ✓ | 兩個建構子收 `SdlPlatform&`→改在 SDL plugin 內建構、以 `IRenderDevice` 交出；`SetFrameSize` 進中性介面或交 runner；`ReadFrame` 的使用者是 `framework/src/Preview.cpp`（readback，退役）；`SaveCaptureBmp`／`SceneCaptureFile` 在介面外（擷取由 wrap-render 宣告定去向） | wrap-render | 1～4 |
| text/backend/sdl_ttf | 是 | `ITextRasterizer` ✓ | 只改建立方式：`Create()`→plugin 的 build 函式登記 | 搬家：adapters-relocate；登記：wrap-render | 0～1 |
| audio/backend/sdl | 是（命名空間 `Engine::Audio::Sdl`，其他後端是 `…::Backend::…`） | 沒有輸出介面類；中性的是被拉取的 `Mixer`、`Submitter` | 不另設輸出介面類：SDL plugin 的 build 函式開裝置、拉 `Mixer`（Engine 定的詞彙就是 `Mixer`／`Submitter`，滿足原則 (3)）；命名空間在搬家時統一 | 形狀：wrap-render；搬家：adapters-relocate | 1～3 |
| asset sdl_image | 是；**但檔案在 `GYO::Engine` 的公開 include 目錄**，target 定義在 `engine/CMakeLists.txt` | `IAssetLoader` ✓ | 標頭與 target 搬出 Engine；plugin 的 build 函式呼叫 assets-core 的登記 API | adapters-relocate | 1～3 |
| model/backend/ufbx | 是 | `IAssetLoader` ✓ | 消費者直接建構 `UfbxModelLoader`（apps 2、tools 2、tests 7）→改由 ufbx plugin 的 build 函式登記；**取代計畫的 pvp 最小 ufbx plugin（S3-3）與 model-assets plugin**——兩者都會成為 ufbx 的第二個 adapter（原則 (2)） | 登記：assets-core（經過渡登記）；搬家：adapters-relocate | 登記 1～2；搬家 1～2 |
| net | 是（整個模組不連任何外部組件） | 本身就是中性的 GYOP 編解碼 | 整個模組搬進網路 plugin（決定 (f)，不是因為不乾淨）；改 `Wire.hpp` 的含入、apps CMake 2 處；`tests/common/net` 跟著搬（只搬不改，§14.4） | net-link | 2～4 |

**`*Native` target 的去處**：`GYO::PlatformSDLNative`、`GYO::InputBackendSDLNative`、`GYO::RenderBackendSDLNative` 在 engine 外的消費者是 `framework/bindings/imgui`、`tools/ui_editor`（`EditorWindow.cpp`）、tests。target §3b 的 ImGui 兩半（輸入是文件 World 的 System、讀 Engine 的輸入；draw list 由渲染 World 繪製）做成之後，ImGui 不再需要 `SDL_Window*`／`SDL_Renderer*`；`AssetPreviewContext.cpp` 的直接 SDL 含入在 tool-ui 去掉。所以 Native target 在遷移完成時只剩 SDL 家族內部（SDL_image、SDL_ttf 若需視窗以外的 SDL 函式）與 SDL plugin 自己的測試使用（新守衛 N5，§5.2 補遺二）。SDL 家族內部互傳 `SDL_Window*` 是 SDL plugin 內部的事（未決 5 採一個 plugin 時）。

**消費者寫具體類名**：三案一致，**不在中性介面立好時就改這些檔**——apps 與 tools 的這些檔在 client-frame 與工具四批會整個重寫（main＝Run），framework 會被刪；提前改＝在即將消失的程式上製造衝突（RC-7）。adapters-relocate 只改它們的含入路徑與 target 名（機械修改），類名不動；歸零在 client-frame（apps）、工具四批（tools）、framework-delete（framework）。tests 隨 SDL plugin 搬成 plugin 自己的測試。

**Engine 外要改的其他地方**（歸批）：`AssetPreviewContext.cpp` 的 `<SDL3/SDL.h>`→tool-ui；`ImGuiShell.cpp`、`EditorWindow.cpp` 的 Native 使用→plugins-editor／tool-ui；apps 的 asio／httplib→match-host、client-link（protobuf 依未決 8）；`player_content` 的 `GYO::AssetUfbx` 直連→match-sim；`GyoRuntimeDeployment.cmake` `gyo_install_product_target` 以 `component MATCHES "^SDL_"` 推定要裝 `SDL3-shared`（AGENTS §9「命名規則だけを利用して Product の意味を推測しない」）→plugins-foundation 段 (i) 改讀登記資料的 `runtime_dependencies`；產品 SDL link guard（`tests/object_fps_pvp/CMakeLists.txt` 的 `entry MATCHES "^SDL3(_image)?(::|-)"`）**漏了 `SDL3_ttf`**（風險提案查到、本統合重驗）→adapters-relocate 補（締める面）。

**合計**：新立中性契約 1 個（platform）、中性注入名 0～1 個、`IRenderDevice` 補尺寸 1 處、sdl_image 標頭移出 Engine 公開 include 目錄 1 處、`*Native` 收窄 3 個；工時分散在 wrap-render（+4～8；扣掉移到 adapters-relocate 的 `if(TARGET)` 硬性化 1～2，淨 +3～6，見計畫 §3.18 修訂）、adapters-relocate、plugins-editor（I2 時另 +4～8）。

---

## 3. 選擇欄的形狀（產出 3；給 plugins-foundation 的事前宣告當輸入，欄名由該批定）

### 3.1 單位：每個可執行單位一份選擇

- app 新增 `roles.<role>`，名稱與 `gyo_app_add_executable(<role> …)` 相同（object_fps_pvp：`main`、`match`）；tool 沿用 `variants.<v>`，`role_components` 改名 `role_plugins`（`build/cmake/GyoToolRegistry.cmake` 讀它；四個工具都沒寫）。同一個讀取函式處理兩種形狀，只差路徑前綴。
- 理由（價值、成本兩案）：Feature 2 的例子「headless 的 Match 不連視窗與 GPU」今天靠產品 CMake 達成——`apps/object_fps_pvp/project.json` 的 `components` 是全產品聯集，Match 與 Client 的差別寫在 `apps/object_fps_pvp/CMakeLists.txt`（`gyo_app_add_executable(match …)` 不帶 COMPONENTS、`gyo_install_product_target(… ROLE match)` 安裝時也沒帶），同一事實兩處。頂層一個清單表達不了 Feature 2。
- configure 檢查 CMake 建的 role 與 `roles` 相符（錯誤 `role-mismatch`）。

### 3.2 欄的形狀（建議 S-A）

```json
{ "version": 2,
  "roles": {
    "main":  { "plugins": [
      { "plugin": "sdl", "backs": ["window", "input_source", "gpu_device", "audio_output"] },
      { "plugin": "sdl_image", "backs": ["texture_decode"] },
      { "plugin": "sdl_ttf", "backs": ["text_raster"] },
      { "plugin": "ufbx", "backs": ["model_import"] },
      { "plugin": "window_runner" }, { "plugin": "net_link" }, { "plugin": "asset_catalog" },
      { "plugin": "event_sounds" }, { "plugin": "trace_diagnostics" }, { "plugin": "fps_pvp_client" } ] },
    "match": { "plugins": [
      { "plugin": "ufbx", "backs": ["model_import"] },
      { "plugin": "headless_runner" }, { "plugin": "net_link" }, { "plugin": "trace_diagnostics" },
      { "plugin": "fps_pvp_sim" } ] } } }
```

- 名稱都是示意；能力名用 Engine 的詞（naming DI 4）。欄名候選：**`plugins`**（建議：runtime-composition §4、DI 2、Delta-5、§3.11 都已用這個詞；第 1 類也是 plugin，名稱不失真）、`uses`、`selection`；保留 `components` 不建議（詞義與 plugin 不同，兩個概念繼續混）。
- 每個 plugin 一列、形狀一致（物件）。第 1 類用 `backs` 綁定它在這個單位當哪些能力的後端；第 2 類只寫名字；第 1 類不寫 `backs` 時只連它的基礎 target。
- **只連被綁定的能力 target**：這樣 Feature 2 在同一個 SDL plugin 內部也成立（選 SDL 當音訊輸出不會連進視窗），原則 (2) 的「一個 adapter」不和 Feature 2 衝突（未決 5）。
- 初始化順序由 plugin 登記資料的 `requires` 做確定性的拓撲排序（同層以名稱排）；清單順序不帶語義（計畫 §3.11「二選一」的建議答案：依賴宣告。有序清單會讓順序在 `project.json` 與 `requires` 各寫一次）。
- 不自動補依賴：選了 `sdl_image` 沒選 `sdl` 是錯誤（三案一致；自動補會讓連結的內容不在資料上）。今天 `GyoComponents.cmake` 的推導（SDL_GPU 推出 SDL_PLATFORM）同批刪——締める面。

### 3.3 plugin 登記資料（`plugins/<name>/plugin.json`；產品自己的 plugin 放產品目錄，同格式）

```json
{ "version": 1, "name": "sdl_image", "class": "backend",
  "base": "GYO::SdlImage",
  "provides": { "texture_decode": "GYO::SdlImage" },
  "requires": ["sdl"],
  "requires_capabilities": [],
  "runtime_dependencies": ["SDL3", "SDL3_image"] }
```

- `class`：`backend`（第 1 類）／`extension`（第 2 類）——決定 (d)：類別寫在登記資料，目錄不分。
- `provides`：能力→target。**只有第 1 類可寫，值必須是 Engine 宣告過的能力名**——決定 (c) 的附加條「實作 Engine 定的後端介面，不新增能力」的機械形；第 2 類寫 `provides` 是錯誤。
- `requires`：plugin 之間的依賴宣告（決定 (e)：SDL_image、SDL_ttf 都寫 `["sdl"]`）。**依賴是 plugin 層級，連結是 target 層級**：`sdl_image` 依賴 SDL plugin 的 `base`（SDL 函式庫本身），不連視窗後端。
- `requires_capabilities`：這個 plugin 用到哪些需要後端的能力（例：`window_runner` 寫 `["window", "input_source"]`）。**產品直接用到的能力由產品自己的 plugin 宣告**（例：match-sim 的 (c) plugin 寫 `["model_import"]`）——「誰用到能力」全部在登記資料，不另開欄。
- `runtime_dependencies`：打包用；**取代** `GyoRuntimeDeployment.cmake` 以 `SDL_` 前綴推定（§2 末段）。
- **Engine 的能力宣告**寫在各能力模組自己的 CMake（例：`gyo_engine_capability(gpu_device INTERFACE GYO::Render NEEDS_BACKEND)` 在 `engine/render/CMakeLists.txt`）。**不放在 Kernel**：target/kernel DI-KS 15 規定 Kernel 的程式碼與 Registry 不含能力的名字。
- 用到的能力 ＝ 所選 plugin（含產品自己的 plugin）的 `requires_capabilities` 聯集；每個用到的能力必須恰好被一個 `backs` 綁定。「能力被綁定但沒人用」不報錯（例：工具選 SDL 當視窗、另選了音訊輸出卻還沒用到）。

### 3.4 configure 錯誤（全部 `FATAL_ERROR`；代號固定、可被測試比對，AGENTS §14.2 的可識別失敗）

共通格式：`GYO configure error [<代號>]: <project.json 路徑> <roles.<role>|variants.<v>>: <訊息>`。

| 代號 | 條件 | 訊息例 |
|---|---|---|
| `missing-backend` | 用到的能力沒有被任何 `backs` 綁定 | `capability 'gpu_device' is required by plugin 'window_runner' but no selected plugin backs it; plugins that provide it: sdl` |
| `duplicate-backend` | 一個能力被兩個以上 `backs` 綁定 | `capability 'gpu_device' is backed twice: 'sdl' and 'other'; a capability takes exactly one backend` |
| `not-provided` | `backs` 列了該 plugin 不提供的能力 | `plugin 'sdl' does not provide capability 'model_import' (provides: window, input_source, …)` |
| `missing-dependency` | `requires` 的 plugin 沒被選（不自動補） | `plugin 'sdl_image' requires plugin 'sdl', which roles.main does not select` |
| `dependency-cycle` | 依賴成環 | `plugin dependency cycle: a -> b -> a` |
| `unknown-plugin` | 名字沒有登記（含過渡登記表） | 沿用今天 `gyo_validate_components` 的形：`Unknown GYO plugin 'z'; expected one of: …` |
| `invalid-backend` | 第 1 類 `provides` 了 Engine 沒宣告的能力，或第 2 類寫了 `provides`／`backs` | `plugin 'x' is class 'backend' but provides 'foo', which is not a capability declared by Engine` |
| `role-mismatch` | CMake 建的 role／variant 與 `project.json` 不相符 | `role 'match' is built by CMake but not declared in roles` |

決定 (b)：「必備」相對於該單位用到的能力——headless 的 Match 沒有任何已選 plugin `requires_capabilities` 視窗與 GPU，就不報錯、也不連。plugins-foundation 判定 (3) 擴成「每個代號一個合成夾具反例」。

### 3.5 版本與過渡

- **只升一次版**（三案一致）：plugins-foundation 段 (i) 一次做「v1→v2、`plugins` 進來、`components` 刪除」，六份 `project.json` 同批升；讀取端只認 v2（計畫 §3.11 判定 (6) 已定不做兩版相容）。照計畫原樣（先加 `plugins` 欄、之後再併）會升兩次版、兩欄並存一段時間（AGENTS §9 同一事實兩處）。
- **過渡登記表**：共通 build 資料裡放**一份表**（位置由 plugins-foundation 定，例 `build/cmake/` 下一個 JSON），列「plugin 名已定、實作 target 還在 Engine」的 plugin，每列寫 plugin 名、`class`、`provides`、`base`、`runtime_dependencies`、指向今天的 Engine target（例 `GYO::PlatformSDL`），以及**退出的批**（`v8-arch-adapters-relocate` 等）。
  - 產品從段 (i) 起就寫最終的 plugin 名；某 adapter 搬家時，把那一列從表移成它自己的 `plugins/<name>/plugin.json`，產品資料一行都不改——這是 Feature 1 在登記資料上的兌現。
  - 表只減不增（守衛 N6，§5.2 補遺二）；closeout 時 0 列（JSON 依未決 1 的選擇另計）。
  - `FRAMEWORK` 在表裡佔一列（`class: extension`），退出批＝framework-delete。
  - 不採「先建一個只有登記檔、沒有程式的 `plugins/<name>/`」（價值案；風險、成本案採它）：那會出現一個名為 SDL plugin、卻什麼都不擁有的目錄，Ownership 與目錄不一致（§5 取捨 T6）。
- 一併被取代（段 (i)）：`GyoAppProject.cmake` `gyo_app_link_components` 的名稱→target 表、`GyoComponents.cmake` 的 `GYO_KNOWN_COMPONENTS` 與推導、`GYO_BUILD_*_LOADER` 選項、以 `SDL_` 前綴推定的打包依賴、`tests/common/Requirements.cmake` 的 `GYO_COMMON_TEST_COMPONENTS`（`GYO_TEST_PROFILE=full` 改為「選全部 plugin 的全部能力」）。`GYO_ENABLE_<COMPONENT>` 改由選擇推導。
- 工具與產品的 CMake 不寫 plugin 或後端的 target 名（守衛 N4；白名單從 `player_content` 與四個工具的 CMakeLists 開始，各自的批刪列）。

### 3.6 不建議的形狀

- S-B：能力→後端的對照表＋另一個 extensions 欄——兩個鍵，違決定 (a)「同一個選擇欄」；結構上不可能「選兩個」，`duplicate-backend` 反而測不出。
- S-C：字串文法（`"gpu_device=sdl"`）——要自定文法、錯誤難指位置，接近 AGENTS §9 不許的任意條件設定言語。
- S-D：頂層一個清單、能力全由 `requires_capabilities` 推導、不寫 `backs`（風險、成本兩案的形）——可行，但「這個單位用 SDL 當哪些能力的後端」不在產品資料上，P2 讀法下要靠推導才能做到 Feature 2；列為次選（§5 取捨 T5）。

---

## 4. 交使用者的未決（事前宣告 §3 的三題＋盤點中新出現的十題）

每題附選項、代價、建議。**本稿不替使用者決定**；計畫修訂以建議案估時，選別的選項時的差額寫在各題。

### 未決 1（JSON 解析庫在 Engine 的契約資料解析裡；事前宣告 §3 第 1 題）

事實：nlohmann 在 engine 7 模組 9 個 `.cpp`（`ContentManifest.cpp`、`catalog/CatalogParser.cpp`、`ShaderLibrary.cpp`、`UiDocumentCodec.cpp`、`UiSerialization.cpp`、`MapMarkers.cpp`、`VoxelMesh.cpp`、`AnimationEvents.cpp`；第 9 個是建置期 `pipeline/src/main.cpp`），全部 PRIVATE、公開標頭 0 件；`StrictJson.hpp` 依賴 nlohmann 的 API 形狀（`sax_parse`、`parse`、`is_discarded`），只有 `MapMarkers.cpp`、`VoxelMesh.cpp` 實例化；`AnimationEvents.cpp` 自己的 `ParseWithoutRepeatedKeys`。determinism §4.6 記錄 voxel_mesh、map_markers 的 literal-bit golden 與 `ArenaDigestTests` 依賴「nlohmann → `std::strtod`」的十進位轉換。使用者先前的方向「JSON 函式庫是可插拔模組」（記憶 framework-controller-principle）。

| 選項 | 內容 | 代價 | 張力 |
|---|---|---|---|
| J1 | 判為固化的基礎，nlohmann 留 Engine；以棘輪凍結含入點（今天 9 檔＋`StrictJson.hpp`） | 2～4 h（守衛＋文件）；升級 nlohmann 要碰 6 個模組 9 檔 | 違原則 (2)（9 處對接）；與「可插拔」的方向不一致；要使用者明示例外 |
| J2 | Engine 定中性文件樹；JSON adapter 做成第 1 類 plugin，連結時選定 | 25～50 h（成本案）／16～32 h（風險案）；所有 Engine 測試 target 都要連 JSON plugin | Engine 的 library 有未解析符號要 plugin 補＝「沒有 plugin，Engine 不成立」，與判斷順序的 Q1 矛盾；連結方向反轉（三案都不建議） |
| **J3** | Engine 定中性文件樹（含嚴格解析、重複鍵規則、寫出），**一個 Engine 內部的 adapter**（例 `GYO::Json`）是唯一含入 nlohmann 的地方；八個解析點改用它 | 16～32 h（推定；風險案，價值案 Engine 側 8～16＋apps／tools 4～8）；回歸用 literal golden、`AnimationContentDigest`、`ArenaDigestTests` | 原則 (1)(2)(3) 全部成立；只對「adapter 都是 plugin」開一個例外（理由：Engine 的契約資料解析不能依賴 plugin）；之後要可選，再把 `GYO::Json` 的實作搬成 plugin（可逆） |
| J4 | 現在做 J1；在 Replacement Criteria 寫觸發條件（nlohmann 升級要改兩個以上模組／出現第二種解析需求／含入點棘輪要加新檔），觸發時做 J3 | 現在 2～4 h；觸發時 16～32 h | 同 J1 的例外，但有條件期限 |

- 不論 J2／J3，**數字以文字（lexeme）交給 Engine，由 Engine 用 `std::from_chars` 轉**（風險案）：(c) 結果不隨 JSON 後端變。判定：`ArenaDigestTests`、voxel_mesh／map_markers 的 literal golden、`ContentManifest`／`CatalogParser`／`gyo.ui` 往返／shader manifest／`gyo.anim_events`（重複鍵）的測試三平台全綠；`StrictJson` 與 `ParseWithoutRepeatedKeys` 的重複鍵語義收進同一處。
- **建議：J3**（風險、價值兩案；成本案建議 J4）。理由：Q1 明確（Engine 讀不進契約資料就不成立），J3 讓原則 (1)～(3) 對 JSON 成立、只在「位置是 plugin」這點例外，並符合使用者「可插拔」的方向。若不要付 16～32 h，選 J4（不是 J1），把觸發條件寫進 RC。
- 計畫的落點：J3 時開條件批 `v8-arch-json-adapter`（17～34 h＝16～32＋守衛去處判斷 1～2），排在 G2a 之後的 Engine／plugins 線、與 adapters-relocate 並行，不在關鍵路徑、不加 G2a 前架構線的負荷（成本案的瓶頸分析）；風險案主張排在階段 1 讓 (c) 守衛早在新路徑上綠——代價是 G2a 前架構線 +17～34 h，本稿不採（§5 取捨 T8）。
- 範圍題（併在未決 6）：產品與工具直接用 nlohmann（apps 7、tools 17、framework 2）要不要也改用中性文件樹。

### 未決 2（建置期工具 `engine/render/shaders/pipeline`；事前宣告 §3 第 2 題）

事實：獨立的 host 專案（`project(GyoShaderTools)`，交叉編譯時 FATAL_ERROR）；三個外部組件只出現在 `src/main.cpp` 與 `gyo_host_shadercross`；工具另抓一份 SDL3（release-3.4.0，關掉 video 等子系統）與 nlohmann；產出 manifest（`abi`＝`gyo.raster.v1`）由 `ShaderLibrary.cpp` 讀；`gyo_add_shader_bundle` 呼叫點 3 處；CI 以 `engine/render/shaders/pipeline/**` 的雜湊當快取鍵（`.github/workflows/build-and-validate.yml`、`cross-platform.yml`）。

| 選項 | 內容 | 代價 |
|---|---|---|
| **B2／T1** | 三原則的範圍寫成「執行期連結進 Engine、plugin、產品與工具的組件」；`gyo_shader_tool` 本身就是 SDL_shadercross、SPIRV-Cross、DXC 唯一的 adapter，位置與 owner（render）不動 | 0.5～1 h（文件）；限界：升級 SDL 時，工具自抓的那份 SDL3 版本也要跟著改（兩處），寫進 SDL plugin 的範囲の限界 |
| B（價值案） | 工具算 GPU 後端 adapter 的建置期那一半，搬進 SDL plugin 的 GPU 部分；Engine 留 ABI 與內建 HLSL | 1～3 h（價值案）／3～6 h（成本案）；CI 快取鍵路徑要改（`.github/` 由使用者改）；ABI 與 manifest 讀取在 Engine、工具在 plugin，Engine 的 shader 契約拆兩處 |
| B1 | 工具化，搬到 `tools/`，manifest 當 Data Contract | 5～10 h；快取鍵、`gyo-build.json` 的 `shader_host_tools`、交叉編譯流程重驗；沒有 Feature 需要 |

**建議：B2／T1**（風險、成本兩案），並寫 RC：出現第二個渲染後端、且它要的 shader 格式與 SDL_GPU 不同時重看；B1 列為遷移之外的候選。理由：三個 Feature 沒有一個需要搬它；遷移期間 CI 是主要判官（記憶 ci-is-primary-standard），不在這時改 CI 快取鍵。

### 未決 3（model 取樣規則與 audio core 的歸屬；事前宣告 §3 第 3 題、HANDOFF〈未定〉(1)）

| 候選 | 對外介面 | 規則會不會被替換 | 決定性守衛怎麼跟著搬 | 搬進 plugin 的代價 | 建議 |
|---|---|---|---|---|---|
| model 取樣（`GYO::Model` 經 `Math::Slerp`） | 無（純計算） | 會（E1 改的是 Engine 定的取樣標準） | `Animation Slerp equals Math Slerp bit for bit`（`MathCharacterizationTests.cpp`，屬 math，不跟著搬）；同平台命中 golden 隨 plugin 走；**Match 與 Client 必須選同一個取樣 plugin，否則命中幾何不同而不報錯**——取樣器身分要進 cFingerprint 或 manifest（新的跨 plugin 契約＋守衛） | 4～10 h＋新守衛 2～4 h（推定）；落在 Match 的關鍵路徑上 | **留 Engine**（三案一致）；DET 恢復時重判。理由：做成可替換＝在暫停中的 DET-1／E1 上先設計契約；無點名 Feature |
| audio core（`Mixer`、`Submitter`、整數混音） | 無（輸出在 SDL 後端，那部分是第 1 類） | bus／effect 是 audio.md RC 的出口；換成第三方 mixer 無點名 Feature | `CharacterizationTests.cpp` 的跨平台雜湊隨 plugin 走——風險在「plugin 沒被選就不建它的測試」，要求 `full` profile 選它；`gyo_expect_audio_links` 是 DI 守衛（audio.md DI 2），改動要使用者核准 | 4～8 h（推定）；若要做，獨立一批排在 wrap-render 之後（不與介面改動疊在同一回退單位） | **留 Engine**（三案一致）。主要理由：原則 (3)——SDL 音訊輸出是第 1 類 adapter，它實作的介面要由 Engine 定（推論 R3） |

兩者都交使用者。若選 plugin，各 +4～8 h（另開批），計畫的關鍵路徑在 model 的情況要重算。

### 未決 4（「對外介面」的讀法；新）

- 讀法 1：面向 Engine 以外的系統（第三方組件、OS 裝置、網路、外部行程）的介面。與 collision、math 已定的歸屬一致。
- 讀法 2：C++ 公開 API。會讓 collision 成為 plugin，與已定矛盾。
- **建議：讀法 1**（三案一致），使用者確認後寫進 runtime-composition §4（本稿寫入 §4 時標了「讀法待確認」）。

### 未決 5（「一個外部組件只有一個 adapter」對 SDL 本身的讀法；新）

- **P2**（風險、價值兩案）：一個 adapter＝一個 plugin（一個目錄、一份 owner 文件、一份登記），內部可以有多個後端 target；SDL 本身是一個 `sdl` plugin，SDL_image、SDL_ttf 各自一個 plugin（決定 (e)）。產品以 `backs` 綁定能力，只連被綁定的 target（§3.2）。原則 (2) 的字面意思。build 層要能表達「能力→target」，+2～4 h（成本案估，已含在 plugins-foundation 段 (i) 的增量內）。
- P1（成本案）：SDL 的子系統各自一個 plugin（`sdl` 核心＋`sdl_gpu`、`sdl_input`、`sdl_audio`、`sdl_renderer2d`），都依賴 `sdl`；與今天的 target 一對一、成本最低；但 SDL 的呼叫出現在多個 plugin，原則 (1)(2) 要讀成「SDL 家族合起來是 SDL 的 adapter」。
- **建議：P2**。P1 等於把決定 (e)「SDL 家族各自一個 plugin」的家族範圍擴到 SDL 自己的子系統，那是決定行沒說的。

### 未決 6（三原則套到哪些層：產品與工具的直接使用、測試框架、測試夾具；新）

- 事實：產品直接含 asio 2 檔、httplib 1、protobuf 1；工具直接含 `<imgui.h>` 9 檔（`ImGui::` 約 814 處）、`<SDL3/SDL.h>` 1 檔；nlohmann 在 apps 7、tools 17、framework 2；doctest 在 tests 154 檔。
- 選項：(a) 原則 (1) 套到所有執行期連結的程式（Engine、plugins、產品、工具），例外明列（ImGui 的 widget 呼叫＝未決 7、protobuf generated code＝未決 8、nlohmann 依未決 1）；(b) 只套 Engine 與 plugins 層，產品與工具的直接使用是 owner 自己的實作（成本案對 nlohmann 的建議）。
- 測試：doctest 是測試框架、不進任何執行檔；測試直接用 nlohmann 造夾具。照字面套用要改 tests 154 檔＋spikes 2 檔（成本案）。
- **建議：(a)，並明寫「測試框架與測試夾具不受原則 (1) 限制（adapter 自己的測試除外）」**。理由：Feature 1 的原話是「用到它的產品（Match、Client、四個工具）不跟著改」——產品直接含 asio 時，升級 asio 產品就得跟著改，(b) 讓 Feature 1 對產品不成立。代價：守衛 N1（第三方含入棘輪）的白名單要列產品與工具的現況並逐批刪；產品的 asio／httplib 本來就在 match-host、client-link 的重寫範圍（0 增量）；nlohmann 在產品與工具的改寫依未決 1 的選擇，在各自的遷移批做（J3 時估 +4～8 h，分攤，不計入本稿合計）。

### 未決 7（ImGui 在工具裡的直接呼叫；新）

| 選項 | 內容 | 代價 |
|---|---|---|
| A | 定一層 GYO 自己的即時模式 UI API，工具不直接呼叫 ImGui | 40～100 h 以上（價值案 40～80、成本案 40～100，推定）；近似 AGENTS §4 不鼓勵的汎用 Framework |
| B | 明列例外：工具可直接呼叫 ImGui（ImGui 的 API 就是它提供的能力） | ImGui 的 Feature 1 不成立（升級 ImGui 工具要跟著改），寫進範囲の限界 |
| **C** | context、輸入、draw list、字型、貼圖 id 收進 imgui plugin（就是 target §3b 的 imgui 兩半），**只有 widget 呼叫**明列例外 | widget API 有破壞性變更時工具仍要改；寫進 imgui plugin 的 RC：出現第二套 UI 後端時重看 |

**建議：C**（價值案；風險、成本兩案的 A／後者與 C 的差別只在是否寫明「後端部分收進 plugin」）。理由：ImGui 升級的破壞性變更多半在 backend、context、字型、貼圖；這些在 plugins-editor 的「imgui 兩半」本來就收進 plugin。例外寫進新 DI 5 的例外清單（若使用者核准未決 11）與 N1 白名單，限 `tools/`。

### 未決 8（protobuf 的 generated code；新）

- A：產品的 protocol 目錄與 generated code 是「依產品 schema 產出的產品的 wire 編解碼」，網路 plugin 擁有 protobuf 執行期函式庫的加入、版本與 protoc 規則；**手寫的產品程式只用 message 存取器，不含 `google/protobuf/*`**（價值案的收窄）；`IpcHost.cpp` 的 `unknown_field_set.h` 在 match-host 歸零。守衛：N1 的白名單只留 generated code 與宣告列名的解碼檔。
- B：由 plugin 依產品 schema 產生並包裝——plugin 就得知道產品 proto，違計畫 §3.12 的反向刪除驗證與 AGENTS §9。
- C：換 wire 表示——wire 不改（計畫 §1.3）。
- **建議：A**（三案一致）。代價 0（match-host、client-link 本來就這樣排）；原則 (2) 對 protobuf 有兩處（plugin＋產品 generated code），寫進範囲の限界。

### 未決 9（SDL 2D renderer 的去留；新）

事實：target §3b 已定「imgui 的 draw list 由渲染 World 繪製」；今天渲染 World 的裝置介面只有 `IRenderDevice`（SDL_GPU 實作）。`tools/audio_editor`、`tools/ui_editor` 的 gui 變體今天只選 `SDL_RENDERER`、不選 `SDL_GPU`。

- I1：照 §3b，draw list 一律經 `IRenderDevice`（GPU）畫，SDL 2D 後端在 plugins-editor 退役。代價 1～2 h；**audio_editor、ui_editor 多出 GPU 需求**（工具執行環境的要求改變＝行為變更，要使用者核准）。
- I2：Engine 立一個小的中性 2D 介面（貼圖、幾何繪製；夠 draw list 用），SDL 2D 後端實作它。代價 +4～8 h；工具的執行需求不變；之後要退役 2D 只刪一個後端（可逆）。但它是只為 ImGui 在兩個工具上用的第二個裝置介面（AGENTS §1：單一 use case 不另立汎用抽象）。
- **建議：I1**，前提是「audio_editor、ui_editor 需要 GPU」可接受；不接受則 I2。由 plugins-editor 的事前宣告定案，但這個前提現在就要使用者表態（影響 plugins-editor 的估時與工具的使用環境）。風險案建議 I2、價值與成本案傾向退役（§5 取捨 T9）。計畫以 I1 估時；選 I2 時 plugins-editor +4～8 h。

### 未決 10（target §4 的 Q2 句；新）

- 現行 `target/runtime-composition.zh-Hant.md` §4（a6-revision 依 Q2 寫入、已核定的本文）：「plugin 對它所包的第三方（SDL、ImGui）是第三方邊界…**只有該第三方的 adapter plugin 可以在公開標頭露出它的型別**」。原則 (3) 要求 adapter 的對外介面用 Engine 的詞——adapter 一露出第三方型別，消費者就看得到該型別。
- 建議改寫：「adapter plugin 的公開標頭只用 Engine 的詞；第三方型別只在該 plugin 的私有標頭，或明示的 native 子標頭（使用者限於同家族的 plugin 與該 plugin 自己的測試，清單只減不增）；例外（ImGui widget，未決 7）寫在該 plugin 的 owner 文件並列入它的 RC」。
- 這句是已核定的本文，**改它要使用者同意**；本稿只在該句後加「與原則 (3) 的關係待使用者定」的註，沒有改寫。計畫 §3.20 的「plugin 公開標頭衛生」守衛隨之改成「第三方 adapter 邊界」守衛（adapters-relocate）。

### 未決 11（runtime-composition 的 Design Invariants 增改；新；Architecture Delta，交使用者）

目標 DI 1～4 已由使用者 2026-11-13 核准（`v8-arch-target-di-approved`）；本節任何變更都是 Architecture Delta（AGENTS §3）。本稿**沒有改 DI 節**，草稿如下：

- **DI 2 增補**（價值案）：在「產品與工具的 plugin 選擇只寫在 `project.json`」之後加三點：選擇以可執行單位（role、variant）為粒度；第 1 類後端也經同一個欄選擇；產品與工具的 CMake 不寫 plugin 或後端的 target 名。守衛：N2（選擇欄的契約測試）、N4。
- **新 DI 5**（三案都提）：「外部組件的型別與呼叫只在它的 adapter plugin（每個組件一個）；Engine 與其他 plugin 的公開標頭只用 Engine 的詞；Engine 不連任何外部組件〔JSON 依未決 1〕。」守衛：N1（第三方含入棘輪，closeout 為 0 或核准的例外）＋第三方 adapter 邊界守衛（G1／G3 的改寫，adapters-relocate）＋依賴方向掃描。理由：沒有寫成 DI，長遷移期間局部合理的「這裡先直接 include 一下」會累積成漂移（AGENTS §3「判斷できる形で書かれた約束だけが…Drift を止められる」）。
- **新 DI 6**（風險案；價值、成本案併在 DI 2／DI 5）：「每個可執行單位用到的、需要後端的能力恰好有一個已選後端；缺或多是 configure 錯誤並指名能力。」守衛：§3.4 的合成反例。
- **新 RC 兩條**：(i) 升級一個第 1 類 adapter 時，它輸出的內容身分 (c) 改變（例：ufbx 的載入結果變了，`AnimationContentDigest` 跟著變）→ 不是「只改 adapter」，走內容身分的版本流程（風險案；Feature 1 的極限寫明、不藏）；(ii) 同一組件的對接出現在第二個 plugin 時，不放寬 DI 5，而是把對接搬回它的 adapter；介面不足就回 Engine 補介面（價值案）。
- **建議**：DI 2 增補＋DI 5＋DI 6＋兩條 RC 一起核准；最晚在 plugins-foundation 的事前宣告之前（N1、N2 要以它們為依據）。條文工時 0.5～1 h；守衛已算在 plugins-foundation 與 adapters-relocate。

### 未決 12（計畫 §8 的三處；新）

- **RC-8**（新 Delta 累計 2 個就重排）：Delta-7 是新 Delta。選項：(a) 算第 1 個（風險、成本兩案）——再出現 1 個就觸發；(b) 比照 Delta-6 的先例「寫進 §4 後不計」（價值案）。**建議 (a)**：本修訂本身就是 RC-8 要求的「回到規劃」，保守計數讓下一個 Delta 一定回到使用者。
- **RC-11**（總量錨點 1,299 h）：本修訂把上限加到 1,368 h。改錨點是改判定，但這是執行前、看到任何實績前的範圍追加（不是看結果後放寬，§14.1 不禁）。**建議改成 1,368**；不改則觸發點從 2,052 h 變成 1,948.5 h（相對收緊約 5%）。
- **RC-12（新增，風險案）**：任一 adapter 的搬家批需要同時改它的中性介面才能過＝介面沒有先立好，停、回到該介面所屬的批（防止搬家與介面改動混在一個回退單位）。**建議新增**（締める面）。

### 未決 13（其餘能力模組的歸屬確認；HANDOFF〈未定〉(3)）

§1 表中標「建議（未決 13）」的 asset core、input core、io、map_markers、render core、text core、trace core、ui、voxel_mesh，以及 config（不適用）、entity_store（由 Delta-3 推出）、runtime（已排程刪除），三案的建議都是**留 Engine**。交使用者一次確認；確認後 HANDOFF〈未定〉(3) 結案。`v8-arch-docs-scope-of-target` 要求的「audio、input、platform-sdl、trace、io 由本批逐個定」：歸類見 §1，開 target 檔的批見計畫 §3 共通的新條（platform-sdl→wrap-render 開 `target/platform`、`docs/plugins/sdl` 由 adapters-relocate 建；input→wrap-render；audio→wrap-render（留 Engine 時）；trace→diagnostics-service；io→assets-core）。

### 流程事項（不是使用者的設計決定）

- **產出 6 未做**：`docs/architecture/README.md` 在本工作樹與 `origin/master` 都不存在，只在未合併的 `origin/claude/librarian-architecture-entry`（`6e55f196`）。該分支的 `Layers and dependencies` 條目有「Decisions not yet written into §4 (the backend plugin planning batch will add them): kernel HANDOFF `v8-arch-engine-or-plugin-criteria` and `v8-arch-external-components-as-adapters`…」一句。§4 已寫入；刪那一句要等入口合併後由後合併的一方做（檔案歸圖書管理員，`v8-arch-architecture-entry`）。
- 未查證、交執行批先查：SDL_Init 在各後端之間的分工（SDL plugin 的 `base` 是否持有 SDL_Init，決定 SDL_image／ttf 依賴 `base` 是否足夠）；Windows 上把 SDL3.dll 放到測試執行檔旁的機制在搬家後是否照舊。兩項寫進 adapters-relocate 的事前宣告第一步。

---

## 5. 三案取捨

| # | 主題 | 風險案 | 價值案 | 成本案 | 本稿 | 理由與放棄的代價 |
|---|---|---|---|---|---|---|
| T1 | 選擇欄只升一次版、刪 components | 是 | 是 | 是 | **採** | 三案一致 |
| T2 | 選擇的單位 | app 頂層、tool variant | **app `roles`、tool variant** | **app `roles`、tool variant** | 採價值、成本案 | Feature 2 要以可執行單位表達；放棄的代價：object_fps_pvp 的 `project.json` 多一層 |
| T3 | ufbx 的時點 | assets-core 經過渡登記登記；程式在 match-sim 後另開 `plugin-ufbx-relocate` 搬 | assets-core 搬＋登記（關鍵路徑 +4～8） | 段 (i) 搬、assets-core 登記 | **登記在 assets-core（經過渡登記，路徑 +1～2）；程式在 adapters-relocate 搬** | 三案都刪掉 pvp 最小 plugin 與 model-assets（ufbx 唯一的 adapter）。不在關鍵路徑上搬（價值案的 +4～8），也不擠進段 (i)（成本案：段 (i) 對 world-exec 的餘裕只剩數小時）；搬在 match-sim 之後（風險案：避免與 match-sim 改同一檔 `PlayerAnimationContent.cpp`），但不另開一批（併進 adapters-relocate，省一次交接）。代價：ufbx 的 Feature 1（升級只改 plugin）晚到 G2a 後 |
| T4 | SDL 家族的時點與批 | 中性介面在 wrap-render 原地立；SDL_image／ttf 在 wrap-render 後一批；SDL 本體在 framework-delete 後一批（尾段路徑 +4～5） | plugins-frame 一次搬（plugins-frame +10～20） | **新批 B：G2a 後、wrap-render 前，只搬不改形狀** | **採成本案的形，併入 ufbx：`v8-arch-adapters-relocate`** | 搬與改形狀分成兩種 diff（審查成本小、搬的部分可交小模型＋腳本檢查）；不進關鍵路徑（階段 2' 鏈 64～147 h 短於 G2a→client-sim 125～260 h）。放棄風險案的「消費者遷完才搬、少碰 framework 與舊產品程式」：代價是 engine 外約 67 檔的含入路徑機械修改（6～12 h，含在本批），其中一部分在之後被刪；G2a～G2b 之間 framework 與 Client 線都沒有在改這些檔，RC-7 的衝突風險低。也放棄價值案的「plugins-frame 一次做」：plugins-frame 的 diff 會同時含 runner 新寫與大量搬檔 |
| T5 | 能力與後端的綁定寫法 | 名字清單＋`uses` 推導 | **`backs` 寫在產品的選擇裡** | 名字清單＋`requires_capabilities` | 採價值案（S-A），推導形列次選 | 「這個單位用 SDL 當哪些能力的後端」在資料上看得到；P2 讀法下 Feature 2 不靠推導。代價：產品資料多寫 `backs` |
| T6 | 過渡期的登記放哪 | `plugins/<name>/plugin.json` 指向 Engine target | **共通 build 的過渡登記表** | `plugins/<name>/plugin.json` 指向 Engine target | 採價值案 | 不出現「名為 SDL plugin、什麼都不擁有」的目錄（Ownership 與目錄一致）；表只減不增、一處對帳。代價：搬家批除了新增 `plugin.json` 還要刪表的一列（共通 build 資料的一行，是遷移本身的變更） |
| T7 | 中性介面的形式 | 新立 platform 後端介面、輸入注入、音訊輸出、2D（I2）；擴 `IRenderDevice` | platform 中性層、input 後端介面、audio 輸出介面 | **不設 OO 介面類**；中性契約＝wrap-render 定的 Resource／boundary 型別 | **混合**：platform 要有中性契約（runner 不含 SDL 時得呼叫它），形式由 wrap-render 定、不做鏡像類；audio 不另設輸出介面類（`Mixer`／`Submitter` 就是 Engine 的詞）；輸入注入二選一；2D 交未決 9 | 三案都反對鏡像 `SdlPlatform` 的大類別；成本案「付兩次中性化的錢」的警告採納為「只立 runner 與 boundary 真正要用的入口」。代價：wrap-render +4～8 h |
| T8 | JSON | J3，條件批排在階段 1 | A（≈J3） | J4 | 建議 J3，條件批排在 G2a 後 | 原則對 JSON 成立且符合使用者「可插拔」的方向；排在 G2a 後不加 G2a 前架構線的負荷（成本案：G2a 前架構線串行和 129～268 h 與路徑同量級）。放棄風險案的「(c) 守衛早在新路徑上綠」 |
| T9 | SDL 2D renderer | I2（中性 2D 介面） | plugins-editor 二選一（退役／補實作 `IRenderDevice`） | 退役候選 | 建議 I1（退役），前提交使用者 | target §3b 已定 draw list 由渲染 World 畫；I2 是單一 use case 的第二個裝置介面。風險案指出的 GPU 需求改變寫成前提，不藏 |
| T10 | window runner | `window_runner` 不含 SDL | `window_runner` 不含 SDL | 保留 `sdl-window-runner` 名 | 採風險、價值案 | 原則 (1)(2)：SDL 呼叫只在 SDL plugin；runner 經中性 platform 契約 pump |
| T11 | imgui 的 SDL glue | plugins-editor +8～16 h 拆掉 | +2～4 h | 0 | +2～4 h | target §3b 的「imgui 兩半」本來就在 plugins-editor 的範圍（計畫 §3.24 Contract），拆掉 `imgui::sdl3_sdlrenderer3` 是它的一部分；增量只算 Native 消費者歸零與依賴宣告。風險案的較高估計列為本批的不確定點（integrator-notes） |
| T12 | 守衛先於搬動 | 掃描根加 `plugins` 提前到段 (i)、含入棘輪、ctest 名單比對、`if(TARGET)` 14 處硬性化 | N1～N6 | 通用第三方 adapter 邊界守衛取代 G1／G3 | **全收** | 三種「靜默消失」（`if(TARGET)` 14 處＋G3 函式內 1 處；OS 邊界三件不掃 `plugins`；plugin 沒被選時它的測試不建）在第一份 `plugins/` 程式與第一次搬家之前就位；硬性化從 wrap-render 移到 adapters-relocate 的第一步（target 在那裡改名） |
| T13 | net 的時點 | net-link | net-link | 段 (i) | 採 net-link | net-link 本來就建 `plugins/net_link`；段 (i) 的餘裕留給選擇欄 |
| T14 | 估時 | +47～96，合計 679～1,395 | +27～54，659～1,353 | +18～43，650～1,342 | **+33～69，665～1,368** | 逐項取有出處或兩案相近的值（計畫 §2.2 修訂列各寫組成）；全部推定 |
| T15 | Delta-7 與 RC-8 | 算 1 | 比照先例不計 | 算 1 | 建議算 1（未決 12） | 保守 |

**不採的**：J2（三案都不建議）；P1（未決 5）；把全部搬家塞進 G2a 前（成本案 Z：G2a 前架構線 +13～26 h）；把搬家併進 wrap-render（成本案 X：diff 混合「形狀」與「搬檔」）；先建只有登記檔的 `plugins/<name>/`（T6）。

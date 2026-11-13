# 外部組件改成 adapter plugin 的規劃（修訂稿）：`v8-arch-adapter-plugins-plan`

件数の突き合わせ：engine 頂層模組 **23＝23**（事前宣告與 `ls engine` 一致）；third_party **10＝10**（一致）；建置期外部組件 **3**（事前宣告寫 2——**不一致**：偵察 A 多查出 SPIRV-Cross，`engine/render/shaders/pipeline/cmake/Dependencies.cmake` 的 `gyo_spirvcross`、`src/main.cpp` 含 `<spirv_cross_c.h>`；不是判定失敗，三份提案的重數都是 3）；歸類表主列 **36＝23＋10＋3**（一致），另有後端子列 7 與建置期工具子列 1；後端 **9＝9**（一致：7 個子列＋本身就是後端的 platform、net 兩個主列）；受影響批：既有 **17**＋新增 **2**（偵察 B 數 17；本稿既有批也是 17，**數目一致、組成差一處**：偵察 B 列了 wrap-assets，本稿判它不變；本稿另列 closeout（過渡登記表與含入棘輪的歸零判定））。**對抗檢查後**：engine 外含入後端標頭的檔 **65**（統合稿寫「約 67」，沿用成本案未重數——**不一致**，已改；apps 4、build 3、framework 6、tests 39、tools 13）；後端公開非 native 標頭的外部型別記號 **5 行、4 檔，全為註解**（統合稿寫「只命中 2 處註解」——**不一致**，已改；「型別 0 件」的結論不變）；`engine/net` 的消費者 **6 處**（統合稿列 1 處——**不一致**，已補）；asio、httplib 的 CMake 連結者 **7 行**（apps 2、tests 2、acceptance 3；統合稿未數）；對抗檢查 blocker 6／should 13／nit 8 ＝ 處置記錄 27 條（**一致**，文末）；掃描與重算在主 checkout `build/evidence/adapter-plugins-plan/reviser-scan.{py,txt}`、`reviser-sum.{py,txt}`。

- 狀態：**修訂稿（Fable／high 敵對檢查一次之後，待使用者確認）**。日期 2026-11-13。Owner：架構（架構師會話）。
- 性質：ultracode 小編制的統合（事前宣告 `adapter-plugins-plan-declaration.zh-Hant.md` §4）：偵察 Opus／high ×2、提案 Opus／high ×3（①風險與可逆、②價值、③成本）、統合 Opus／high；敵對檢查 Fable／high 一次（blocker 6／should 13／nit 8，報告在主 checkout `build/evidence/adapter-plugins-plan/adversary-report.md`）；修訂 Opus／high（本稿；逐條處置見文末）。
- 基準：工作樹 `GYO-Engine-arch`，分支 `claude/arch-adapter-plugins-plan`；統合稿在 HEAD `fd012f4b` 上寫、提交為 `9bcb959b`；修訂時已併入 master（`b77ba68b`，含 PR #210 的入口 README），修訂未提交。偵察與三份提案的原文在主 checkout `build/evidence/adapter-plugins-plan/`（`recon-engine.md`、`recon-plan.md`、`proposal-{risk,value,cost}.md`、`proposal-risk-scan.{py,txt}`；git 管理外）。
- 已定前提（不重開）：kernel `HANDOFF.md` 決定行 `v8-arch-external-components-as-adapters`（立場、三條原則、方向 C、plugin 兩類、細節六題 (a)～(f)）、`v8-arch-engine-or-plugin-criteria`、`v8-arch-plugin-selection-data`、`v8-arch-framework-retire`、`v8-arch-docs-scope-of-target`、`v8-arch-architecture-entry`。本稿的「建議」都交使用者；「推定」＝無出處的估計；「未查證」＝本次沒讀到能確認的出處。
- 本稿改的其他檔：`migration-plan.zh-Hant.md`（文末「修訂記錄（v8-arch-adapter-plugins-plan）」列出改了哪些段）、`target/runtime-composition.zh-Hant.md` §4（本文；只寫使用者已裁決的內容，待定的讀法都在本檔 §4；Design Invariants 節不動，逐位元比對）、`docs/architecture/README.md`（產出 6：`Layers and dependencies` 條目刪「待寫入 §4」那一句；入口已由 PR #210 合併進 master）。

## 0. 結論（先寫）

1. **Feature 2、3 的機制在 plugins-foundation 段 (i) 交付，不必先搬任何後端程式碼**（三案一致）：選擇欄一次升到 version 2（components 併入、刪除），可執行單位（app 的 role、tool 的 variant）各自一份選擇；還在 Engine 裡的後端以「過渡登記表」掛上最終的 plugin 名，之後每個 adapter 搬家只改登記，產品資料不再動（§3）。**對真實產品生效的時點**：「用到哪些能力」來自所選 plugin 的 `requires_capabilities`，段 (i) 時真實產品的 plugin 還不存在——所以過渡登記表的列（含 `FRAMEWORK`、`IMGUI`）也要帶 `requires_capabilities`（手寫的近似），各產品自己的 plugin 出現時（match-sim、client-frame、工具四批）換成精確值；寫進段 (i) 的範囲の限界（對抗檢查 S-2）。
2. **Feature 1 的「升級」靠型別隔離今天已大致成立**：9 個後端的非 native 公開標頭含入外部組件 0 件、型別 0 件（型別記號的掃描命中 5 行，全是註解；§2）。要改的地方在 Engine 外：三個 `*Native` target、產品、工具、測試與 acceptance 直接含入 asio／httplib／protobuf／ImGui／SDL、消費者直接寫具體後端類名與後端定義的型別（例：audio 輸出的設定與錨點）；以及 platform、輸入注入沒有中性契約。
3. **搬法**：ufbx 由 assets-core 登記、成為唯一的 ufbx adapter（過渡期 build 函式的位置依未決 16）（刪掉計畫裡「pvp 最小 ufbx plugin → model-assets」那一次替換）；`engine/net` 併進 net-link；**SDL 家族與 ufbx 的程式碼在 G2a 之後由一個新的只搬不改形狀的批 `v8-arch-adapters-relocate` 一次搬進 `plugins/`**，排在 wrap-render 之前，wrap-render 再在 plugins 裡改形狀（§5 取捨 T4）。
4. **計畫合計（非條件批）632～1,299 → 681～1,400 h**（+49～101，全為推定；統合稿是 665～1,368，對抗檢查後補 S-3、S-4、S-5、S-8、B-1、B-2、B-3 的工時）；**全採本稿建議時 702～1,442 h**（加條件批 json-adapter 17～34 與產品、工具改用中性文件樹 4～8）；非條件批 33 → 34，條件批 1 → 2（新增條件批 `v8-arch-json-adapter`，只在使用者選 J3 時開）；**關鍵路徑 331～692 → 332～694 h**（只有 assets-core +1～2 在路徑上；adapters-relocate 改排到 G2b 合併之後時最多再 +9～37，未決 15）。不觸發 RC-8（Delta-7 算第 1 個時）與 RC-11。重算：主 checkout `build/evidence/adapter-plugins-plan/reviser-sum.{py,txt}`。
5. **受影響的批**（計畫修訂記錄逐段列出）：既有 17 批＝plugins-foundation、assets-core、match-sim、net-link、diagnostics-service、match-host、wrap-render、plugins-frame、client-link、client-frame、plugins-editor、tool-voxel、tool-model-viewer、tool-audio、tool-ui、framework-delete、closeout；新增 2 批＝adapters-relocate、json-adapter（條件）。估時有變的是其中 7 批＋新批（§2.2 修訂列）。
6. **交使用者的未決 17 題**（§4）：事前宣告 §3 的三題，加盤點中新出現的十題，加對抗檢查後新列的四題（14 GYOP 契約的 owner、15 adapters-relocate 與 Match PR 線的順序、16 過渡期 ufbx 登記的位置與 round2 的一句、17 U4 已接受代價的變化）；其中 Design Invariants 的增改（新 DI 5、DI 6 等）是 Architecture Delta，不在本批寫進 target。照判斷順序得到 plugin、本稿建議留 Engine 的兩列（model core、audio core）寫成「待使用者」並在未決 3。

---

## 1. 模組歸類表（產出 1）

**判斷順序**（`v8-arch-engine-or-plugin-criteria`）：**Q1** 沒有它，Engine 是否成立？不成立→留 Engine。成立時看 **Q2a** 有沒有對外的介面、**Q2b** 是固化的基礎還是會變、會被替換的規則；無對外介面且固化→留 Engine，否則→plugin。外部組件另受 `v8-arch-external-components-as-adapters` 的三原則與細節 (e)(f) 約束。

**Q2a 的讀法（未決 4，三案一致的讀法，交使用者確認）**：「對外介面」＝面向 Engine 以外的系統（第三方組件、OS 裝置、網路、外部行程）的介面；Engine 自己定給 plugin 實作、或給產品呼叫的中性 C++ 介面不算。根據：使用者判 collision、math「沒有對外介面」，而兩者都有公開 C++ API（`gyo_collision`、`gyo_math`）；若讀成「C++ 公開 API」，幾乎所有模組都會被推成 plugin，與已定歸屬矛盾。

**推論 R3**（原則 (3)「adapter 的對外介面由 Engine 定」的推論）：承載第 1 類後端所實作之中性介面的模組必須留 Engine；否則介面由 plugin 定。**R3 不是判斷順序的一部分**：歸類表的 Q1／Q2a／Q2b 照順序寫答案、得出歸屬；R3 只在「照順序是 plugin、本稿建議留 Engine」時當建議的理由，那一列的歸屬寫「待使用者」（對抗檢查 B-4）。

**停止條件的檢查**：36 列套準則後，**0 列**與已定歸屬相反（collision、math 留；網路含 `engine/net` 是 plugin；SDL、SDL_image、SDL_ttf、ufbx 第 1 類；ImGui 第 2 類）。**照順序的檢查**（對抗檢查 B-4；統合稿只核對了已定的幾列）：每一列都寫了 Q1、Q2a、Q2b 的答案（不適用的寫理由）；照順序得到 plugin、本稿建議留 Engine 的有 **2 列**——model core（Q2b：取樣規則會被替換）、audio core（Q2b：bus／effect 是替換點）——歸屬寫「待使用者」、列進未決 3；trace core 的 Q2b 依 trace.md DI 6 改正為「固化」（統合稿寫「記錄格式會變」，但 Engine 不持有記錄格式），照順序留 Engine；runtime 補上 `FixedTickRuntime` 的三問。另有四處是原則與現況形狀之間的張力（不是歸屬矛盾），列為未決：nlohmann（未決 1）、ImGui 的工具 widget 呼叫（未決 7）、protobuf generated code（未決 8）、target §4 的 Q2 句（未決 10）。

### 1.1 engine/ 的 23 個模組

| # | 模組 | Q1 | Q2a | Q2b | 歸屬 | 已定／建議 | 證據（檔＋符號） |
|---|---|---|---|---|---|---|---|
| 1 | asset（core） | 不成立：世界內容只經 Loading 的資產階段（runtime-composition DI 3）；`Assets<T>`、請求序號是內容身分 (c) 的契約（若判成立：Q2a 無——檔案經 `engine/io`／`engine/os`；Q2b 固化——內容身分 (c) 的契約；照順序仍留 Engine） | — | — | 留 Engine（照順序；另 R3：`IAssetLoader` 由第 1 類實作） | 建議（未決 13） | `engine/asset/include/engine/asset/loading/IAssetLoader.hpp`；`engine/asset/src/ContentManifest.cpp` `ContentManifest::Parse`；計畫 §3.19 |
| 1a | └ asset 的 sdl_image loader | 成立 | 有（SDL_image） | 會被替換 | **第 1 類**：SDL_image plugin，宣告依賴 SDL plugin | 已定 (e) | `gyo_asset_sdl_image`（`engine/CMakeLists.txt`，`if(TARGET SDL3_image::SDL3_image)` 內）；`SdlImageTextureLoader.hpp` 位於 `GYO::Engine` 的公開 include 目錄 |
| 2 | audio（core：`Mixer`、`Submitter`、`ClipBank`） | 成立（Match 不用） | 無（輸出在後端） | 兩面：整數混音與跨平台記錄雜湊是 DI（audio.md DI 8、9，固化面）；bus／effect 是 audio.md RC 預留的替換點、使用者列為候選（會被替換的一面）→ 照順序取「會被替換」 | **待使用者**（照順序是 plugin；建議留 Engine，理由：R3——SDL 音訊輸出是第 1 類 adapter，它實作的 `Mixer`／`Submitter` 要由 Engine 定；換 mixer 無點名 Feature） | 待使用者（未決 3；〈未定〉(1) 候選） | `engine/audio/include/engine/audio/Mixer.hpp` `Mixer`、`Submitter`；`tests/common/audio/CharacterizationTests.cpp`（跨平台錄製雜湊，label `cpu`）；`gyo_expect_audio_links` |
| 2a | └ audio/backend/sdl | 成立 | 有（SDL） | 會被替換 | **第 1 類**：SDL plugin 的音訊輸出部分 | 已定 (e) | `gyo_audio_sdl` `SdlAudioOutput::Open(Mixer&&, …)` |
| 3 | base | **不成立** | — | — | 留 Engine | 已定（準則明列 Base） | `gyo_base`（INTERFACE）；`engine/base/include/engine/base/StrictJson.hpp` `ParseStrictJson`（只以模板參數接 nlohmann，見未決 1） |
| 4 | collision | 成立 | 無 | 固化 | 留 Engine | **已定** | 準則決定行；`gyo_collision` |
| 5 | config | 不適用（不是程式模組） | — | — | 不適用：`projects.csv`、`tools.csv` 是 Product Registration 資料，留原處 | 建議 | `engine/config/`；讀者 `build/cmake/GyoApps.cmake`、`GyoTools.cmake` |
| 6 | entity_store | **不成立**（World 欄的實作，Delta-3） | — | — | 留 Engine | 建議（由 Delta-3 推出） | `gyo_entity_store`；計畫 §4 Delta-3 |
| 7 | input（core：`PhysicalInputFrame`、`InputActionMap`） | 成立（Match 不用） | 無 | 固化：後端要填的 Engine 詞彙 | 留 Engine（R3） | 建議（未決 13） | `engine/input/include/engine/input/PhysicalInputFrame.hpp`；`gyo_input` 只 PRIVATE 連 `GYO::Base` |
| 7a | └ input/backend/sdl（含 native） | 成立 | 有（SDL） | 會被替換 | **第 1 類**：SDL plugin 的輸入部分 | 已定 (e) | `gyo_input_sdl` `SdlInput`、`SdlInputInjector`；`gyo_input_sdl_native` `HandleNativeEvent` |
| 8 | io | 不成立：`IFileSystem`、`IStream` 是資產載入的詞彙，`Assets<T>` 依賴它；若成 plugin，Engine 的 asset 就要 include plugin（違 Delta-2 第 5 項） | 對 OS 有，但已經 `engine/os`；無第三方 | （若 Q1 判成立：固化——資產載入的詞彙；照順序仍留 Engine） | 留 Engine（介面與同步實作）；IO 執行緒服務已是 plugin（計畫 §3.11 asset-io） | 建議（未決 13） | 併入 `GYO::Engine`；`IFileSystem`、`IFileWatcher`、`IStream`；計畫 §3.19 `WriteFileAtomically` |
| 9 | kernel | **不成立** | — | — | 留 Engine | 已定（Delta-1） | `gyo_kernel`；target/kernel DI-KS 15 |
| 10 | map_markers | 成立 | 無（nlohmann 是 PRIVATE） | 固化：契約資料 `gyo.map_markers` 的解釋；literal-bit golden 是 (c) 守衛（determinism §4.6） | 留 Engine | 建議（未決 13） | `engine/map_markers/src/MapMarkers.cpp` 經 `Base::ParseStrictJson` |
| 11 | math | 成立 | 無 | 固化 | 留 Engine | **已定** | 準則決定行；`gyo_math` |
| 12 | model（core：`ModelAsset`、`AnimationEvents`、取樣） | 成立 | 無 | `ModelAsset`／`AnimationEvents` 是契約詞彙（固化面）；**取樣規則會被替換**（E1 改的是 Engine 定的取樣標準；determinism §4.2，今天 (b)、同平台 golden；是否升 (c) 在暫停中的 DET-1／E1）→ 照順序取「會被替換」 | **待使用者**（照順序是 plugin；建議留 Engine，理由：R3——ufbx adapter 輸出 `ModelAsset`，詞彙要由 Engine 定；取樣做成可替換＝在暫停中的 DET-1／E1 上先設計跨 plugin 契約，且無點名 Feature） | 待使用者（未決 3；〈未定〉(1) 候選） | `engine/model/include/engine/model/ModelAsset.hpp`；`engine/model/src/AnimationEvents.cpp`；`gyo_expect_model_links` |
| 12a | └ model/backend/ufbx | 成立 | 有（ufbx） | 會被替換 | **第 1 類**：ufbx plugin | 已定 (e) | `gyo_asset_ufbx` `UfbxModelLoader`（PRIVATE 連 `gyo_ufbx`）；消費者 `apps/object_fps_pvp/src/Sim/PlayerAnimationContent.cpp`（Match）與 `src/Pvp/PvpApplication.cpp`（Client）都直接 `loaders.Register(std::make_unique<…UfbxModelLoader>())` |
| 13 | net | 成立（Engine 的其他模組都不用它；使用者是 object_fps_pvp 與它的測試、acceptance） | 有（wire 的資料框；與 Go gateway 共用的跨語言契約） | — | **第 2 類**：網路 plugin，與 asio／httplib／protobuf 同一個 plugin | **已定 (f)** | `engine/net/include/engine/net/GyopDatagram.hpp` `EncodeGyopDatagram`、`DecodeGyopDatagram`；`gyo_net` 只連 `GYO::Base`；消費者：`apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp`、apps CMake 的 `GYO::Net` 2 處、`tests/object_fps_pvp/CMakeLists.txt` 的 `GYO::Net`、`build/acceptance/object_fps_pvp/worker_main.cpp`（`EncodeGyopDatagram`）、`tests/common/net`；向量檔 `tests/common/net/fixtures/gyop_datagram_vectors.json` 另被 `services/gyo_gateway/framing/vectors_test.go` 讀（GYOP 與 Go gateway framing 逐位元組共用，`GyopDatagram.hpp` 檔頭；契約 owner 見未決 14） |
| 14 | os | **不成立** | — | — | 留 Engine | 已定（準則明列） | os.md DI 1～3 |
| 15 | platform（今天只有 `sdl/`；本身就是後端） | 成立（Kernel 不需要視窗） | 有（視窗、事件） | 會被替換 | **第 1 類**：SDL plugin 的核心；**Engine 要新立中性的 platform 契約**（今天不存在，§2） | 已定 (e)＋建議（中性契約） | `engine/platform/CMakeLists.txt` 只有 `add_subdirectory(sdl)`；`SdlPlatform`、`PlatformEvent`、`PathDialogRequest`、`SdlLogBridge`、`GyoMain`（`gyo_platform_sdl_main`） |
| 16 | render（core：`IRenderDevice`、`Renderer`、`ShaderLibrary`、`gyo_model_renderer`） | 成立（headless Match 不畫） | 無（GPU 在後端） | 固化：`IRenderDevice` 是 Engine 的詞彙；渲染 World 的組合規則在 kernel DI-KS 13 | 留 Engine（R3） | 建議（未決 13） | `engine/render/include/engine/render/IRenderDevice.hpp`；`engine/render/src/ShaderLibrary.cpp` |
| 16a | └ render/backend/sdl（2D，含 native） | 成立 | 有（SDL_Renderer） | 會被替換；可能整個退役 | **第 1 類**：SDL plugin 的 2D 部分；去留見未決 9 | 已定 (e) | `gyo_render_backend_sdl` `SdlRenderer`（未實作 `IRenderDevice`）；`SdlRendererNative.hpp` |
| 16b | └ render/backend/sdl_gpu | 成立 | 有（SDL_GPU） | 會被替換 | **第 1 類**：SDL plugin 的 GPU 部分 | 已定 (e) | `gyo_render_backend_sdl_gpu` `SdlGpuRenderDevice final : public IRenderDevice` |
| 16c | └ render/shaders/pipeline（建置期工具，組件見 34～36） | 不進執行檔 | — | — | 留 `engine/render`（三原則的範圍寫成「執行期連結的組件」） | **待使用者**（未決 2） | `gyo_shader_tool`；`build/cmake/GyoShaders.cmake` `_gyo_require_shader_tool` |
| 17 | runtime（`RuntimeLoop`、`IRuntimeClient`；`FixedTickRuntime`） | `RuntimeLoop`／`IRuntimeClient`：成立（Kernel 取代它）；`FixedTickRuntime`：成立（Kernel 的 Scheduler 推進 tick） | `FixedTickRuntime`：無第三方；時鐘經 `engine/time` | `FixedTickRuntime`：會變的規則（runner 的時鐘策略，計畫 §3.21 client-sim 的「runner 的時鐘策略」）；`RuntimeLoop`：被 Kernel 取代 | `RuntimeLoop`／`IRuntimeClient`：刪除（runtime-loop-retire，計畫 §3.31）；`FixedTickRuntime`：照順序 plugin——進 runner plugin（計畫 §3.31 已排） | 已排程 | `engine/runtime` 併入 `GYO::Engine` |
| 18 | text（core：`ITextRasterizer`） | 成立 | 無 | 固化（Engine 詞彙） | 留 Engine（R3） | 建議（未決 13） | `engine/text/include/engine/text/ITextRasterizer.hpp` |
| 18a | └ text/backend/sdl_ttf | 成立 | 有（SDL_ttf） | 會被替換 | **第 1 類**：SDL_ttf plugin，宣告依賴 SDL plugin | 已定 (e) | `gyo_text_backend_sdl_ttf` `SdlTtfTextRasterizer::Create` |
| 19 | threads | **不成立** | — | — | 留 Engine | 已定（準則明列） | `gyo_threads` |
| 20 | time | **不成立** | — | — | 留 Engine | 已定（準則明列） | `gyo_time` |
| 21 | trace（core：sink 契約） | 成立 | 無：寫檔經 `engine/os`，trace 自己不面向外部系統 | 固化：sink 的保存律、順序、無輪詢喚醒（trace.md DI 1～5）；**記錄的格式與語義不在 Engine**（DI 6：Engine 不解析、不改寫、不知道一行的意義——統合稿寫「記錄格式會變」，與 DI 6 不符，已改正） | 留 Engine（照順序；core）；寫檔服務已是 plugin（計畫 §3.13 trace-diagnostics，`engine/trace` 不改） | 建議（未決 13） | `gyo_trace`（PUBLIC `GYO::Threads`、PRIVATE `GYO::Os`）；trace.md DI 6、7 |
| 22 | ui（文件模型、`gyo.ui` codec、`UiRenderer`） | 成立 | 無（nlohmann 是 PRIVATE，JSON 的對接依未決 1） | 固化：`gyo.ui` 是有版本的資料契約、Engine 是 owner，Client 與 ui_editor 預覽必須同一份解釋；`UiRenderer` 在 wrap-assets 重寫的是形狀（System＋`Gpu*` Resource），不是被替換的規則（判斷有餘地，未決 13 請使用者確認） | 留 Engine（照順序；統合稿的「wrap-assets 同批再搬只會擴大變動面」是成本、不是準則，刪） | 建議（未決 13） | `engine/ui/src/UiDocumentCodec.cpp`、`UiSerialization.cpp`；`gyo_ui_renderer` |
| 23 | voxel_mesh | 成立 | 無 | 固化：`gyo.voxel_mesh` 等契約；決定碰撞世界與 `ArenaContentDigest`；literal-bit golden (c)（determinism §4.6） | 留 Engine | 建議（未決 13） | `engine/voxel_mesh/src/VoxelMesh.cpp` 經 `Base::ParseStrictJson` |

### 1.2 third_party 的 10 個與建置期 3 個

| # | 組件 | 歸屬 | 已定／建議 | 現況的 adapter 位置與要處理的事（檔＋符號） |
|---|---|---|---|---|
| 24 | asio | **第 2 類**：網路 plugin 的內部實作 | 已定 (f) | 今天在產品：`apps/object_fps_pvp/src/Pvp/IpcHost.cpp`、`ClientConnection.cpp`；測試：`tests/object_fps_pvp/ConnectionLivenessTests.cpp`、`IpcHostTests.cpp`；acceptance：`build/acceptance/object_fps_pvp/worker_main.cpp`；`third_party/asio` 的 `add_subdirectory` 寫在 `apps/object_fps_pvp/CMakeLists.txt`；CMake 連結者 apps 2 行、tests 2 行、acceptance 2 行 |
| 25 | doctest | 不適用（只在測試：tests 154 檔、spikes 2 檔；不進 Engine、plugin、產品執行檔） | 建議（未決 6） | `doctest::doctest` 的連結者只有 tests 與 spikes |
| 26 | httplib | **第 2 類**：網路 plugin 的內部實作 | 已定 (f) | 今天在產品：`ClientConnection.cpp`；acceptance：`network_main.cpp`、`worker_main.cpp`；CMake 連結者 apps 1 行、acceptance 3 行（與 asio 同行者合計 7 行） |
| 27 | imgui | **第 2 類**：imgui plugin | 已定 | `framework/bindings/imgui` `ImGuiShell.cpp`；工具 9 檔直接含 `<imgui.h>`（audio_editor 1、model_viewer 1、ui_editor 6、voxel_map_editor 1）；`imgui::sdl3_sdlrenderer3` PUBLIC 連 SDL3（ImGui 的 SDL glue＝第二處 SDL 呼叫）；工具的 widget 呼叫見未決 7 |
| 28 | nlohmann_json | **待使用者**（未決 1） | — | engine 6 模組 9 個 `.cpp`（asset 2、render 2——其一是建置期 `pipeline/src/main.cpp`——、ui 2、map_markers、voxel_mesh、model；全部 PRIVATE；公開標頭 0 件）＋第 7 個模組 base 的 `StrictJson.hpp`（API 形狀依賴，不是 `.cpp`）；apps 7、tools 17、framework 2、tests 20、acceptance 6 檔 |
| 29 | protobuf | **第 2 類**：網路 plugin 擁有執行期函式庫的加入、版本與 protoc 規則；產品 generated code 的歸屬見未決 8 | 已定 (f)＋待使用者 | `IpcHost.cpp` 含 `google/protobuf/unknown_field_set.h`；`runtime_v9`／`client_v9` 的 `.pb.cc` 由 `protobuf::protoc` 在產品建置期產生 |
| 30 | sdl3 | **第 1 類**：SDL plugin（一個 adapter；內含核心、視窗、輸入、2D、GPU、音訊輸出等後端 target——讀法見未決 5） | 已定 (e) | 今天分散在 `engine/{platform,input,render,audio}` 的 9 個 target（含 3 個 `*Native` 與 OBJECT 的 `gyo_platform_sdl_main`／`GYO::PlatformSDLMain`）；engine 外直接含入：`tools/ui_editor/src/AssetPreviewContext.cpp`、tests/common 10 檔、`tests/ui_editor/PreviewAssetTests.cpp`（另用 `ImGui_ImplSDLRenderer3_*`） |
| 31 | sdl3_image | **第 1 類**：SDL_image plugin，宣告依賴 SDL plugin | 已定 (e) | 只有 `engine/CMakeLists.txt` 連它 |
| 32 | sdl3_ttf | **第 1 類**：SDL_ttf plugin，宣告依賴 SDL plugin | 已定 (e) | 只有 `engine/text` 與 `engine/text/backend/sdl_ttf` 連它 |
| 33 | ufbx | **第 1 類**：ufbx plugin | 已定 (e) | 只有 `engine/model` 連它；Match 經 `player_content` 的 PRIVATE_LIBRARIES 直接連 `GYO::AssetUfbx`（繞過 component 名） |
| 34 | SDL_shadercross（建置期） | 留在 `gyo_shader_tool`（工具本身就是這三個組件唯一的 adapter） | **待使用者**（未決 2） | `Dependencies.cmake` 的 `gyo_shadercross`；`gyo_host_shadercross` |
| 35 | SPIRV-Cross（建置期） | 同上 | 同上 | `gyo_spirvcross`；`main.cpp` 含 `<spirv_cross_c.h>` |
| 36 | DXC（建置期） | 同上 | 同上 | `gyo_dxc`／`gyo_dxc_source`；只以連結庫出現（含入樣式全 repo 0 件，無正例——偵察 A 前言） |

---

## 2. 中性介面盤點（產出 2）

**掃描**（偵察 A §3；風險提案以正例重驗；統合重驗正例與下文引用的幾項：主 checkout `build/evidence/adapter-plugins-plan/integrator-scan.{py,txt}`；**修訂時重跑本節與對抗檢查指出的各項**：同目錄 `reviser-scan.{py,txt}`，git 管理外；計畫估時的重算在同目錄 `reviser-sum.{py,txt}`）：

- 指令：`git grep -n -P` 以 Python 包起來，回傳碼 0＝命中、1＝無命中、其他＝中止；stderr 不丟。
- 正例：`#\s*include\s*[<"]SDL3/` 在 `engine/platform/sdl/native` 命中 1 筆（`SdlPlatformNative.hpp` 的 `#include <SDL3/SDL.h>`）；外部型別記號 `\b(SDL_\w+|TTF_\w+|IMG_\w+|ufbx_\w+|nlohmann|asio)\b` 在三個 native 標頭共命中 22 處。
- 對 9 個後端的**公開、非 native** 標頭：含入樣式 `#\s*include\s*[<"](SDL3|SDL3_image|SDL3_ttf|ufbx|nlohmann|asio)` **0 件**；型別記號的正規式命中 **5 行、4 檔，全部是註解**——`SdlPlatform.hpp` 2 行（`SDL_HINT_IME_IMPLEMENTED_UI`、`SDL_EVENT_WINDOW_EXPOSED`）、`SceneCaptureFile.hpp` 1 行（`SDL_HINT_BMP_SAVE_LEGACY_FORMAT`）、`SdlGpuRenderDevice.hpp` 1 行（「SDL_GPU implementation…」）、`SdlTtfTextRasterizer.hpp` 1 行（「Optional SDL_ttf implementation…」）；**型別 0 件**（統合稿寫「只命中 2 處註解」，是偵察 A 的原文、沒有重跑；修訂時重跑，指令與輸出在 `reviser-scan.txt` 的 B3c 段，排除 `*/native/*` 與 `*Native.hpp`）。
- **結論**：「升級」所需的型別隔離在非 native 標頭已成立。不乾淨的是 (a) 三個 `*Native` target 把 SDL 交給 framework、tools、tests；(b) 消費者直接寫具體後端類名（「替換」不成立）；(c) platform 與輸入注入沒有中性契約、2D renderer 沒有中性介面。

| 後端 | 公開標頭乾淨 | Engine 內的中性介面 | 要改的符號與去向 | 批 | 估時（推定） |
|---|---|---|---|---|---|
| platform/sdl | 是（`class NativeAccess;` 前置宣告；`PlatformEvent` 以不透明指標持有事件） | **無**；消費者直接寫 `Engine::Platform::Sdl::SdlPlatform`（engine 外：apps 3、tools 11、framework 6、tests 29、acceptance 6 檔） | 在 `engine/platform` 新立**中性 platform 契約**：`PlatformEvents` boundary 的型別、建窗、把事件抽進 boundary、live frame 掛鉤、剪貼簿、路徑對話框請求／回覆（`PathDialogRequest`／`PathDialogAnswer` 今天已不含 SDL，可改名搬入）、log 橋接、行程入口。**不做鏡像 `SdlPlatform` 的大類別**（三案一致：遷移後產品不直接碰 platform，鏡像類別在遷移完成時變死介面）；形式（抽象類、函式表或 Resource＋System）由 wrap-render 的事前宣告定。`NativeWindow()`／`NativeEvent()` 只給 SDL 家族與 SDL plugin 自己的測試 | 介面：wrap-render；搬家：adapters-relocate | 4～8（風險、價值兩案同值；成本案 2～4 計在 audit platform 之內） |
| input/backend/sdl | 是 | 輸出資料中性（`PhysicalInputFrame`、`InputEvent`）；沒有「來源」介面 | 來源不需要類別介面：SDL plugin 的 pump 填 boundary；`SdlInputInjector`（以類名計 tests 1 檔、acceptance 2 檔；以含入 `SdlInputInjection.hpp` 計 tests 2 檔——`SdlInputInjectionTests.cpp`、`HeaderHygieneTests.cpp`——、acceptance 2 檔；統合稿的「tests 6」重現不出來，已改）需要中性的注入名，或留在 SDL plugin 的測試支援（二選一由 wrap-render 宣告定）；與 `SdlPlatform&` 的配對留在 SDL plugin 內部 | wrap-render | 0～4 |
| render/backend/sdl（2D） | 是 | **無**；`SdlRenderer` 不實作 `IRenderDevice` | 使用者只有 ImGui 的 SDL 繪製（`framework/bindings/imgui` 經 `RenderBackendSDLNative`）與 `framework/include/gyo/framework/PreviewTexture.hpp` `UploadFrame`（readback 上傳，隨 readback 退役）；工具 4 檔、tests 7 檔。**先不設計 2D 中性介面**，去留見未決 9 | plugins-editor | I1 1～2；I2 4～8 |
| render/backend/sdl_gpu | 是 | `IRenderDevice` ✓（對 Client 與 GPU 預覽；**對 ImGui draw list 不足**：沒有裁切矩形與索引子範圍的繪製——`engine/render/include` 只命中 `RenderDeviceTypes.hpp` 一句「non-indexed」註解與 handle 的 `index`；未決 9 的 I1 要先擴充） | 兩個**靜態工廠** `SdlGpuRenderDevice::Create(SdlPlatform&, …)`、`CreateOffscreen(SdlPlatform&, …)`（唯一的建構子是 private 的 `explicit SdlGpuRenderDevice(std::unique_ptr<Impl>)`）→改在 SDL plugin 內呼叫、以 `IRenderDevice` 交出；`SetFrameSize` 是**離屏專用**（非離屏回 `UnsupportedOperation`），測試以外唯一的呼叫者是 `framework/src/Preview.cpp`——與 `ReadFrame` 同一條要退役的 readback 路徑，隨 plugins-editor 的 offscreen-preview（無 readback）處置，**不進 `IRenderDevice`**（統合稿的「`SetFrameSize` 進中性介面或交 runner」刪）；`SaveCaptureBmp`／`SceneCaptureFile` 在介面外（擷取由 wrap-render 宣告定去向） | wrap-render（I1 的介面擴充在 plugins-editor） | 1～4（I1 的擴充另 2～4，計在 plugins-editor） |
| text/backend/sdl_ttf | 是 | `ITextRasterizer` ✓ | 只改建立方式：`Create()`→plugin 的 build 函式登記 | 搬家：adapters-relocate；登記：wrap-render | 0～1 |
| audio/backend/sdl | 是（命名空間 `Engine::Audio::Sdl`，其他後端是 `…::Backend::…`） | 沒有輸出介面類；中性的是被拉取的 `Mixer`、`Submitter`；**輸出的設定、錨點、統計沒有中性型別** | 不另設輸出介面類：SDL plugin 的 build 函式開裝置、拉 `Mixer`。但產品與工具今天用到的不只 `Mixer`／`Submitter`（對抗檢查 B-3 (a)）：`apps/object_fps_pvp/src/Present/PvpSound.cpp` 用 `SdlAudioOutputConfig`、`std::vector<OutputAnchor>`（經 `TakeAnchors`）與 `SdlAudioOutput::Open`；`tools/audio_editor/src/AudioEditorWindow.cpp`、`tools/model_viewer/src/PreviewSounds.cpp` 直接持有 `SdlAudioOutput`；公開面另有 `Stats()`（`SdlAudioOutputStats`）、`DriverName()`、`DeviceSampleFrames()`——這些型別由後端定義，照原則 (3) 與細節 (c)，**設定、錨點、統計要成為 Engine 的詞**（中性型別，名字由 wrap-render 定），或宣告寫明產品與工具不再取用（例：錨點只給 SDL plugin 自己的測試）；命名空間在搬家時統一 | 形狀與中性型別：wrap-render；搬家：adapters-relocate | 2～5（中性型別 1～2 計在 wrap-render 的增量） |
| asset sdl_image | 是；**但檔案在 `GYO::Engine` 的公開 include 目錄**，target 定義在 `engine/CMakeLists.txt` | `IAssetLoader` ✓ | 標頭與 target 搬出 Engine；plugin 的 build 函式呼叫 assets-core 的登記 API | adapters-relocate | 1～3 |
| model/backend/ufbx | 是 | `IAssetLoader` ✓ | 消費者直接建構 `UfbxModelLoader`（apps 2、tools 2、tests 7）→改由 ufbx plugin 的 build 函式登記；**取代計畫的 pvp 最小 ufbx plugin（S3-3）與 model-assets plugin**——兩者都會成為 ufbx 的第二個 adapter（原則 (2)） | 登記：assets-core（位置與 owner 依未決 16）；搬家：adapters-relocate | 登記 1～2；搬家 1～2 |
| net | 是（整個模組不連任何外部組件） | 本身就是中性的 GYOP 編解碼 | 整個模組搬進網路 plugin（決定 (f)，不是因為不乾淨）；改全部消費者：`Wire.hpp` 的含入、apps CMake 的 `GYO::Net` 2 處、`tests/object_fps_pvp/CMakeLists.txt` 的 `GYO::Net`、`build/acceptance/object_fps_pvp/worker_main.cpp` 的含入與連結；`tests/common/net` 跟著搬（只搬不改，§14.4）——但它的向量檔 `fixtures/gyop_datagram_vectors.json` 也被 `services/gyo_gateway/framing/vectors_test.go` 以相對路徑讀（C++ 與 Go 共用），**只搬 C++ 那一半會弄斷 Go 的契約測試**；向量檔的位置與 GYOP 契約的 owner 見未決 14，判定加「Go 測試照跑」 | net-link | 3～6 |

**`*Native` target 的去處**：`GYO::PlatformSDLNative`、`GYO::InputBackendSDLNative`、`GYO::RenderBackendSDLNative` 在 engine 外的消費者是 `framework/bindings/imgui`、`tools/ui_editor`（`EditorWindow.cpp`）、tests。target §3b 的 ImGui 兩半（輸入是文件 World 的 System、讀 Engine 的輸入；draw list 由渲染 World 繪製）做成之後，ImGui 不再需要 `SDL_Window*`／`SDL_Renderer*`；`AssetPreviewContext.cpp` 的直接 SDL 含入在 tool-ui 去掉。所以 Native target 在遷移完成時只剩 SDL 家族內部（SDL_image、SDL_ttf 若需視窗以外的 SDL 函式）與 SDL plugin 自己的測試使用（新守衛 N5，§5.2 補遺二）。SDL 家族內部互傳 `SDL_Window*` 是 SDL plugin 內部的事（未決 5 採一個 plugin 時）。

**消費者寫具體類名**：三案一致，**不在中性介面立好時就改這些檔**——apps 與 tools 的這些檔在 client-frame 與工具四批會整個重寫（main＝Run），framework 會被刪；提前改＝在即將消失的程式上製造衝突（RC-7）。adapters-relocate 只改它們的含入路徑與 target 名（機械修改），類名不動；歸零在 client-frame（apps）、工具四批（tools）、framework-delete（framework）。tests 隨 SDL plugin 搬成 plugin 自己的測試。

**Engine 外要改的其他地方**（歸批）：`AssetPreviewContext.cpp` 的 `<SDL3/SDL.h>`→tool-ui；`ImGuiShell.cpp`、`EditorWindow.cpp` 的 Native 使用→plugins-editor／tool-ui；apps 的 asio／httplib→match-host、client-link（protobuf 依未決 8）；`player_content` 的 `GYO::AssetUfbx` 直連→match-sim；`GyoRuntimeDeployment.cmake` `gyo_install_product_target` 與 `GyoPackaging.cmake` 兩處以 `component MATCHES "^SDL_" OR component STREQUAL IMGUI` 推定要裝 SDL 的共享庫（AGENTS §9「命名規則だけを利用して Product の意味を推測しない」）→plugins-foundation 段 (i) 改讀登記資料的 `runtime_dependencies`；後端選擇的第二個入口 `GyoRenderOptions.cmake` 的 `GYO_RENDER_DEVICE`／`GYO_BUILD_SDL_GPU_BACKEND`／`GYO_REQUIRE_SDL_GPU`→段 (i) 定處置（計畫 §3.11）；產品 SDL link guard（`tests/object_fps_pvp/CMakeLists.txt` 的 `entry MATCHES "^SDL3(_image)?(::|-)"`）**漏了 `SDL3_ttf`**（風險提案查到、本統合重驗）→adapters-relocate 補（締める面）。

**合計**：新立中性契約 1 個（platform）、中性注入名 0～1 個、audio 輸出的中性型別（設定、錨點、統計）1 組、sdl_image 標頭移出 Engine 公開 include 目錄 1 處、`*Native` 收窄 3 個；I1 時 `IRenderDevice` 擴充 1 處（裁切、索引子範圍；plugins-editor）。統合稿的「`IRenderDevice` 補尺寸 1 處」刪（`SetFrameSize` 隨 readback 退役，沒有留下來的消費者）。工時分散在 wrap-render（+5～10；扣掉移到 adapters-relocate 的 `if(TARGET)` 硬性化 1～2，淨 +4～8，見計畫 §3.18 修訂）、adapters-relocate、plugins-editor（I1 的擴充 2～4；I2 時以 2D 中性介面 4～8 取代）。


### platform-sdl.md 整份的去向（對抗檢查 S-6；HANDOFF 決定行末句「`platform-sdl.md` 不加標註，隨這題整份重看」）

`engine/platform/sdl` 整個搬走後，`docs/architecture/platform-sdl.md` 的條文分兩處落：**SDL 特有的**落 `docs/plugins/sdl`（adapters-relocate 寫入，同批在 platform-sdl.md 加 Legacy 行）；**任何 platform 後端都要守的**成為中性 platform 契約的條款，落 `target/platform`（wrap-render 開檔並更新 Legacy 行的指向）。突變隨條文走（§3 共通「守衛與突變的去處」）。下表是建議，各批事前宣告定案；DI 條文的落點改變不改性質，搬時照 §14.4 檢查有無緩める面。

| 條 | 內容（摘要） | 落點 | 寫入的批 | 守衛與突變 |
|---|---|---|---|---|
| DI 1 | SDL 後端的非 native 公開標頭不帶 SDL 標頭 | `docs/plugins/sdl`（照 Q2：SDL plugin 可露 SDL 型別；條文改寫成 Q2 的形，依未決 10） | adapters-relocate | G1／G2 對搬走後端退役、plugin 版標頭衛生守衛新增（計畫 §5.2 第 9 列） |
| DI 2 | SDL 型別只經 `*Native` target | `docs/plugins/sdl`＋N5 | adapters-relocate | G3 改寫；補5 `sdl_symbol_guard.py`；N5 |
| DI 3 | 平台交出的時間都在 Engine 時基、滿足時間戳契約 | **`target/platform`**（中性契約的條款：任何後端的事件時間戳都在 Engine 時基）；SDL 的換算細節留 `docs/plugins/sdl` | wrap-render（條款）、adapters-relocate（細節） | m1～m4、m-zero、m20～m22、m-watch-* 隨 SDL plugin 的測試搬（改寫：路徑），中性契約的合成實作另加一條時基測試（新增） |
| DI 4 | 測試注入只經事件佇列或真實視窗操作 | `docs/plugins/sdl`（注入的中性名依 wrap-render 的二選一；選中性名時條款進 `target/platform`） | adapters-relocate／wrap-render | m14、m15 隨搬 |
| DI 5 | log 橋接只在有人持有時生效 | `target/platform`（log 橋接是中性契約的入口之一）＋SDL 細節在 `docs/plugins/sdl` | wrap-render | m10、m11 隨搬 |
| DI 6 | 視窗、顯示、文字輸入狀態每次從 SDL／OS 讀，不留判斷用的副本 | `target/platform`（State Authority：platform 狀態的權威在 OS／後端） | wrap-render | 審查（今天無機械守衛） |
| DI 7 | observer 與 live frame 內只做查詢、不做操作 | `target/platform`（runner 的 live frame 契約；window-runner 經中性契約 pump） | wrap-render（plugins-frame 的 runner 依它） | m-main-thread、m-nest 隨搬；計畫 §3.18 的停止條件「包需要改 DI 7→停」不變 |
| DI 8 | Engine 不呼叫全域 `SDL_Init`／`SDL_Quit`，子系統由 owner 參考計數 | `docs/plugins/sdl`（改寫成「SDL plugin 不呼叫全域…」；SDL_image、SDL_ttf 依賴 `base` 時照這條） | adapters-relocate | 隨搬 |
| DI 9 | 後端不帶擷取政策、文字編輯政策與產品知識 | `target/platform`＋plugins 層 DI（`docs/plugins/README`：無產品名） | wrap-render／plugins-foundation | 審查＋層 DI 的守衛 |
| DI 10 | 一個請求在 OS 回一次時得到一個回覆 | `target/platform`（路徑對話框的請求／回覆搬進中性契約） | wrap-render | `PathDialogTests.cpp`、pd-* 突變隨搬（改寫：路徑與型別名） |
| RC 1 | 需要第二個非 SDL 後端時提出中性 platform 介面 | **由本修訂提前滿足**：使用者的 `v8-arch-external-components-as-adapters`（Feature 1 的「替換」、方向 C）是 AGENTS §2 的予見の根拠 1（名指しされた Feature），所以中性契約現在就立（wrap-render），不等第二個後端；這條 RC 在 `docs/plugins/sdl` 改寫成「中性契約不足以讓 SDL plugin 實作→回 Engine 補契約，不在 plugin 內繞」 | adapters-relocate | — |
| RC 2 | ImGui binding 與 ui_editor 預覽以外不得用 Native | N5（白名單只減不增；plugins-editor、tool-ui 歸零） | adapters-relocate | N5 |
| RC 3～9 | `PlatformEvent` 欄位外露、時間戳與 log 橋接的重驗、pump 離開主執行緒、非同步 OS 回覆、對話框回呼 | 與對應 DI 同落點（3、4、5、6、7、8 隨 `target/platform`；9 隨 `docs/plugins/sdl`） | wrap-render／adapters-relocate | — |

統合稿「流程事項」把「SDL_Init 的分工」列為未查證；DI 8 已有答案，已改引。

---

## 3. 選擇欄的形狀（產出 3；給 plugins-foundation 的事前宣告當輸入，欄名由該批定）

### 3.1 單位：每個可執行單位一份選擇

- app 新增 `roles.<role>`，名稱與 `gyo_app_add_executable(<role> …)` 相同（object_fps_pvp：`main`、`match`）；tool 沿用 `variants.<v>`，`role_components` 改名 `role_plugins`（`build/cmake/GyoToolRegistry.cmake` 讀它；四個工具都沒寫）。同一個讀取函式處理兩種形狀，只差路徑前綴。
- 理由（價值、成本兩案）：Feature 2 的例子「headless 的 Match 不連視窗與 GPU」今天靠產品 CMake 達成——`apps/object_fps_pvp/project.json` 的 `components` 是全產品聯集，Match 與 Client 的差別寫在 `apps/object_fps_pvp/CMakeLists.txt`（`gyo_app_add_executable(match …)` 不帶 COMPONENTS、`gyo_install_product_target(… ROLE match)` 安裝時也沒帶），同一事實兩處。頂層一個清單表達不了 Feature 2。
- configure 檢查 CMake 建的 role 與 `roles` 相符（錯誤 `role-mismatch`，兩個方向都查）。
- **role 名出現在兩處**（對抗檢查 S-1 (1)：`gyo_app_add_executable(<role> …)` 與 `roles` 的鍵，正是本節用來否定現況的「同一事實兩處」）。建議的讀法：**哪些 role 存在只在 `roles` 宣告一次**；CMake 以 role 名**引用**它（提供原始檔與產品自己的函式庫），引用不存在的 role、或宣告的 role 沒有 CMake 實作都是 `role-mismatch`——名字出現兩次是外鍵式的引用、不是兩份要手動同步的清單。若 plugins-foundation 判定這仍算兩處，替代是由資料產生 executable target、產品 CMake 只呼叫 `gyo_app_role_sources(<role> …)` 提供原始檔（同樣要引用 role 名，但 target 的存在只來自資料）。由段 (i) 宣告定。

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
- 不自動補依賴：選了 `sdl_image` 沒選 `sdl` 是錯誤（三案一致；自動補會讓連結的內容不在資料上）。今天 `GyoComponents.cmake` 的推導（SDL_GPU 推出 SDL_PLATFORM）同批刪；它由 `tests/common/cmake/ComponentTests.cmake` 斷言（`gyo_resolve_components(SDL_GPU)` → `SDL_GPU;SDL_PLATFORM`），刪推導是改既有測試的判定，照 AGENTS §14.4 寫 1～5（以前會自動補上的選擇，之後是 `missing-dependency`——對使用者是締める面，但要以 1～5 示出，不是只寫「締める面」；對抗檢查 S-3 (4)）。

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
- 用到的能力 ＝ 所選 plugin（含產品自己的 plugin，以及過渡登記表的列——段 (i) 時它們也帶 `requires_capabilities`，§3.5）的 `requires_capabilities` 聯集；每個用到的能力必須恰好被一個 `backs` 綁定。
- **「能力被綁定但沒人用」是錯誤 `unused-backend`**（對抗檢查 S-1 (3)；統合稿寫「不報錯」）：多綁一個能力就多連一個後端，Feature 2（沒選就不連）對這個方向需要機械保證；判定 (14) 只查 Match 的閉包，查不到工具多綁的情形。要改成警告（例：工具先綁、之後才用），由 plugins-foundation 的宣告寫明理由。
- **`backs` 與 `requires_capabilities` 的分工**（對抗檢查 S-1 (2)）：`requires_capabilities` 是「這個 plugin 用到什麼」（plugin 自己的事實，寫在登記資料）；`backs` 是「這個單位讓哪個 plugin 當那個能力的後端」（產品的選擇）。兩者不是同一事實；會重疊的只有「這個單位用到 X」，由 `unused-backend` 與 `missing-backend` 兩向對帳，不靠手動同步。推導形（S-D）省掉 `backs`，代價是「SDL 當哪些能力的後端」不在產品資料上（§3.6）。

### 3.4 configure 錯誤（9 種；全部 `FATAL_ERROR`——`unused-backend` 是否降為警告由段 (i) 定；代號固定、可被測試比對，AGENTS §14.2 的可識別失敗）

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
| `role-mismatch` | CMake 建的 role／variant 與 `project.json` 不相符（兩個方向） | `role 'match' is built by CMake but not declared in roles` |
| `unused-backend` | `backs` 綁了某能力，但這個單位沒有已選 plugin 用到它（對抗檢查 S-1） | `capability 'audio_output' is backed by 'sdl' in variants.gui but no selected plugin requires it` |

決定 (b)：「必備」相對於該單位用到的能力——headless 的 Match 沒有任何已選 plugin `requires_capabilities` 視窗與 GPU，就不報錯、也不連。plugins-foundation 判定 (3) 擴成「每個代號一個合成夾具反例」（9 個）。

### 3.5 版本與過渡

- **只升一次版**（三案一致）：plugins-foundation 段 (i) 一次做「v1→v2、`plugins` 進來、`components` 刪除」，六份 `project.json` 同批升；讀取端只認 v2（計畫 §3.11 判定 (6) 已定不做兩版相容）。照計畫原樣（先加 `plugins` 欄、之後再併）會升兩次版、兩欄並存一段時間（AGENTS §9 同一事實兩處）。
- **過渡登記表**：共通 build 資料裡放**一份表**（位置由 plugins-foundation 定，例 `build/cmake/` 下一個 JSON），列「plugin 名已定、實作還不是 `plugins/<name>/` 的 plugin」——還在 Engine 的後端（sdl、sdl_image、sdl_ttf、ufbx）、`FRAMEWORK`（實作在 `framework/`）、`IMGUI`（實作在 `framework/bindings/imgui`）；統合稿寫「實作 target 還在 Engine」，與 `FRAMEWORK`、`IMGUI` 兩列不符（對抗檢查 N-6）。每列寫 plugin 名、`class`、`provides`、`base`、`requires_capabilities`（對抗檢查 S-2：讓段 (i) 的 `missing-backend` 對真實產品也生效）、`runtime_dependencies`、指向今天的 target（例 `GYO::PlatformSDL`），以及**退出的批**（sdl 家族與 ufbx＝`v8-arch-adapters-relocate`，ufbx 依未決 16 可能在 assets-core；`IMGUI`＝plugins-editor；`FRAMEWORK`＝framework-delete）。
  - 產品從段 (i) 起就寫最終的 plugin 名；某 adapter 搬家時，把那一列從表移成它自己的 `plugins/<name>/plugin.json`，產品資料一行都不改——這是 Feature 1 在登記資料上的兌現。
  - 表只減不增（守衛 N6，§5.2 補遺二）；framework-delete 之後就是 0 列，closeout 判 0。JSON 不論未決 1 選哪一項都不會在表裡（J3 的 adapter 在 Engine 內、不是 plugin；J1／J4 沒有 adapter）——統合稿「只剩 JSON」的說法刪（N-6）。
  - `FRAMEWORK` 在表裡佔一列（`class: extension`），退出批＝framework-delete。
  - 不採「先建一個只有登記檔、沒有程式的 `plugins/<name>/`」（價值案；風險、成本案採它）：那會出現一個名為 SDL plugin、卻什麼都不擁有的目錄，Ownership 與目錄不一致（§5 取捨 T6）。
- 一併被取代（段 (i)）：`GyoAppProject.cmake` `gyo_app_link_components` 的名稱→target 表、`GyoComponents.cmake` 的 `GYO_KNOWN_COMPONENTS` 與推導、`GYO_BUILD_*_LOADER` 選項、以 `SDL_` 前綴推定的打包依賴（`GyoRuntimeDeployment.cmake`、`GyoPackaging.cmake` 兩處）、產品 CMake 的 `PUBLIC_COMPONENTS`／`PRIVATE_COMPONENTS`／`COMPONENTS` 參數（函式庫層級的連結怎麼表達由段 (i) 定，計畫 §3.11）、`GyoRenderOptions.cmake` 的後端選項（降為「能不能建 GPU 後端」的開關，計畫 §3.11）、`tests/common/Requirements.cmake` 的 `GYO_COMMON_TEST_COMPONENTS`。`GYO_TEST_PROFILE=full` 改為「每個能力選一個後端、選全部 plugin」；同一能力出現第二個後端時，`full` 會撞上 `duplicate-backend`——屆時 `full` 改成每個後端一個測試單位（各自一份選擇），不放寬 `duplicate-backend`（對抗檢查 N-8）。`GYO_ENABLE_<COMPONENT>` 改由選擇推導。`tests/common/ci/app_copy_integration.py` 的複製前綴加 `plugins/`（今天只有 `engine/`、`services/`、`third_party/`、`build/cmake/`；不加會直接失敗）。
- 工具與產品的 CMake 不寫 plugin 或後端的 target 名（守衛 N4；白名單的基線＝段 (i) 當時的掃描結果，不預先點名——今天已知的有 `player_content` 的 `GYO::AssetUfbx`、`apps/object_fps_pvp/CMakeLists.txt` 的 `GYO::PlatformSDLMain` 與 `GYO::Net` 2 處、四個工具的 CMakeLists；各自的批刪列；對抗檢查 S-12 (3)）。

### 3.6 不建議的形狀

- S-B：能力→後端的對照表＋另一個 extensions 欄——兩個鍵，違決定 (a)「同一個選擇欄」；結構上不可能「選兩個」，`duplicate-backend` 反而測不出。
- S-C：字串文法（`"gpu_device=sdl"`）——要自定文法、錯誤難指位置，接近 AGENTS §9 不許的任意條件設定言語。
- S-D：頂層一個清單、能力全由 `requires_capabilities` 推導、不寫 `backs`（風險、成本兩案的形）——可行，但「這個單位用 SDL 當哪些能力的後端」不在產品資料上，P2 讀法下要靠推導才能做到 Feature 2；列為次選（§5 取捨 T5）。

---

## 4. 交使用者的未決（事前宣告 §3 的三題＋盤點中新出現的十題＋對抗檢查後的四題）

每題附選項、代價、建議。**本稿不替使用者決定**；計畫修訂以建議案估時，選別的選項時的差額寫在各題。

### 未決 1（JSON 解析庫在 Engine 的契約資料解析裡；事前宣告 §3 第 1 題）

事實：nlohmann 在 engine 6 個模組的 9 個 `.cpp`（`ContentManifest.cpp`、`catalog/CatalogParser.cpp`、`ShaderLibrary.cpp`、`UiDocumentCodec.cpp`、`UiSerialization.cpp`、`MapMarkers.cpp`、`VoxelMesh.cpp`、`AnimationEvents.cpp`；第 9 個是建置期 `pipeline/src/main.cpp`），全部 PRIVATE、公開標頭 0 件；第 7 個模組是 base——`StrictJson.hpp` 依賴 nlohmann 的 API 形狀（`sax_parse`、`parse`、`is_discarded`），只有 `MapMarkers.cpp`、`VoxelMesh.cpp` 實例化；`AnimationEvents.cpp` 自己的 `ParseWithoutRepeatedKeys`。determinism §4.6 記錄 voxel_mesh、map_markers 的 literal-bit golden 與 `ArenaDigestTests` 依賴「nlohmann → `std::strtod`」的十進位轉換。使用者先前的方向「JSON 函式庫是可插拔模組」（記憶 framework-controller-principle）。

| 選項 | 內容 | 代價 | 張力 |
|---|---|---|---|
| J1 | 判為固化的基礎，nlohmann 留 Engine；以棘輪凍結含入點（今天 9 檔＋`StrictJson.hpp`） | 2～4 h（守衛＋文件）；升級 nlohmann 要碰 6 個模組 9 檔 | 違原則 (2)（9 處對接）；與「可插拔」的方向不一致；要使用者明示例外 |
| J2 | Engine 定中性文件樹；JSON adapter 做成第 1 類 plugin，連結時選定 | 25～50 h（成本案）／16～32 h（風險案）；所有 Engine 測試 target 都要連 JSON plugin | Engine 的 library 有未解析符號要 plugin 補＝「沒有 plugin，Engine 不成立」，與判斷順序的 Q1 矛盾；連結方向反轉（三案都不建議） |
| **J3** | Engine 定中性文件樹（含嚴格解析、重複鍵規則、寫出），**一個 Engine 內部的 adapter**（例 `GYO::Json`）是唯一含入 nlohmann 的地方；八個解析點改用它 | 16～32 h（推定；風險案，價值案 Engine 側 8～16＋apps／tools 4～8）；回歸用 literal golden、`AnimationContentDigest`、`ArenaDigestTests` | 原則 (1)(2)(3) 全部成立；只對「adapter 都是 plugin」開一個例外（理由：Engine 的契約資料解析不能依賴 plugin）；之後要可選，再把 `GYO::Json` 的實作搬成 plugin（可逆） |
| J4 | 現在做 J1；在 Replacement Criteria 寫觸發條件（nlohmann 升級要改兩個以上模組／出現第二種解析需求／含入點棘輪要加新檔），觸發時做 J3 | 現在 2～4 h；觸發時 16～32 h | 同 J1 的例外，但有條件期限 |

- 不論 J2／J3，**數字以文字（lexeme）交給 Engine，由 Engine 用 `std::from_chars` 轉**（風險案）：(c) 結果不隨 JSON 後端變。判定：`ArenaDigestTests`、voxel_mesh／map_markers 的 literal golden、`ContentManifest`／`CatalogParser`／`gyo.ui` 往返／shader manifest／`gyo.anim_events`（重複鍵）的測試三平台全綠；`StrictJson` 與 `ParseWithoutRepeatedKeys` 的重複鍵語義收進同一處。
- **建議：J3**（風險、價值兩案；成本案建議 J4）。理由：Q1 明確（Engine 讀不進契約資料就不成立），J3 讓原則 (1)～(3) 對 JSON 成立、只在「位置是 plugin」這點例外，並符合使用者「可插拔」的方向。若不要付 16～32 h，選 J4（不是 J1），把觸發條件寫進 RC。
- 計畫的落點：J3 時開條件批 `v8-arch-json-adapter`（17～34 h＝16～32＋守衛去處判斷 1～2），排在 G2a 之後的 Engine／plugins 線（與 adapters-relocate 同一條線：串行或開第二張卡），不在關鍵路徑、不加 G2a 前架構線的負荷（成本案的瓶頸分析）；風險案主張排在階段 1 讓 (c) 守衛早在新路徑上綠——代價是 G2a 前架構線 +17～34 h，本稿不採（§5 取捨 T8）。
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

**照判斷順序，兩者都是 plugin**（對抗檢查 B-4）：Q1 成立（Engine 沒有它們也成立）、Q2a 無，Q2b 都有「會被替換的規則」一面（model 的取樣標準、audio 的 bus／effect）；準則是「有對外介面，**或**是會變、會被替換的規則→plugin」。本稿建議留 Engine，是**對準則開例外**，理由不在準則之內：R3（原則 (3) 的推論：`ModelAsset` 與 `Mixer`／`Submitter` 是第 1 類 adapter 所輸出、所實作的詞彙，要由 Engine 定）、無點名 Feature、model 的取樣牽動暫停中的 DET-1／E1。歸類表這兩列的歸屬寫「待使用者」。選項：(a) 照準則做成 plugin（各 +4～8 h，另開批；model 的情況關鍵路徑要重算，且要先設計「取樣器身分進 cFingerprint」的跨 plugin 契約）；(b) 開例外留 Engine，並把例外與重判時點（DET 恢復、出現第二種 mixer 的點名 Feature）寫進各自的 RC；(c) 拆：詞彙（`ModelAsset`、`Mixer`／`Submitter`）留 Engine，只把可替換的規則（取樣、bus／effect）做成 plugin 的擴充點——等於 (a) 的範圍縮小版，代價與 (a) 同量級。**建議 (b)**。

### 未決 4（「對外介面」的讀法；新）

- 讀法 1：面向 Engine 以外的系統（第三方組件、OS 裝置、網路、外部行程）的介面。與 collision、math 已定的歸屬一致。
- 讀法 2：C++ 公開 API。會讓 collision 成為 plugin，與已定矛盾。
- **建議：讀法 1**（三案一致），使用者確認後寫進 runtime-composition §4（本稿寫入 §4 時標了「讀法待確認」）。

### 未決 5（「一個外部組件只有一個 adapter」對 SDL 本身的讀法；新）

- **P2**（風險、價值兩案）：一個 adapter＝一個 plugin（一個目錄、一份 owner 文件、一份登記），內部可以有多個後端 target；SDL 本身是一個 `sdl` plugin，SDL_image、SDL_ttf 各自一個 plugin（決定 (e)）。產品以 `backs` 綁定能力，只連被綁定的 target（§3.2）。原則 (2) 的字面意思。build 層要能表達「能力→target」，+2～4 h（成本案估，已含在 plugins-foundation 段 (i) 的增量內）。
- P1（成本案）：SDL 的子系統各自一個 plugin（`sdl` 核心＋`sdl_gpu`、`sdl_input`、`sdl_audio`、`sdl_renderer2d`），都依賴 `sdl`；與今天的 target 一對一、成本最低；但 SDL 的呼叫出現在多個 plugin，原則 (1)(2) 要讀成「SDL 家族合起來是 SDL 的 adapter」。
- **建議：P2**。P1 等於把決定 (e)「SDL 家族各自一個 plugin」的家族範圍擴到 SDL 自己的子系統，那是決定行沒說的。

### 未決 6（三原則套到哪些層：產品與工具的直接使用、測試框架、測試夾具；新）

- 事實：產品直接含 asio 2 檔、httplib 1、protobuf 1；工具直接含 `<imgui.h>` 9 檔（`ImGui::` 約 814 處）、`<SDL3/SDL.h>` 1 檔；nlohmann 在 apps 7、tools 17、framework 2；doctest 在 tests 154 檔。**測試與 acceptance 也直接用外部組件**（對抗檢查 B-1、S-12；統合稿漏）：`tests/object_fps_pvp` 含 `<asio.hpp>` 2 檔（`ConnectionLivenessTests.cpp`、`IpcHostTests.cpp`）、CMake 連 `asio::asio` 2 行；`tests/ui_editor/PreviewAssetTests.cpp` 直接含 SDL3 並用 `ImGui_ImplSDLRenderer3_*`；`build/acceptance/object_fps_pvp` 含 `<asio.hpp>` 1 檔、`<httplib.h>` 2 檔、`client_v9.pb.h` 2 檔、nlohmann 6 檔，CMake 連 asio／httplib 3 行，並直接連 `GYO::PlatformSDL`、`GYO::PlatformSDLMain`、`GYO::InputBackendSDL`、`GYO::RenderBackendSDLGPU`。
- 選項：(a) 原則 (1) 套到所有執行期連結的程式（Engine、plugins、產品、工具），例外明列（ImGui 的 widget 呼叫＝未決 7、protobuf generated code＝未決 8、nlohmann 依未決 1）；(b) 只套 Engine 與 plugins 層，產品與工具的直接使用是 owner 自己的實作（成本案對 nlohmann 的建議）。**acceptance 與產品測試**另選：(a1) 當「測試夾具」，不受原則 (1) 限制，但列進 N1／N7 的白名單、只減不增；(a2) 當產品的執行期程式（acceptance 的 worker、probe 是會跑的行程），隨產品改寫：asio／httplib 的部分在 client-link（worker 與 Client 共用連線碼），SDL 後端 target 在 client-frame——每批 +1～2 h（推定）。
- 測試：doctest 是測試框架、不進任何執行檔；測試直接用 nlohmann 造夾具。照字面套用要改 tests 154 檔＋spikes 2 檔（成本案）。
- **建議：(a)，並明寫「測試框架與測試夾具不受原則 (1) 限制（adapter 自己的測試除外）」；acceptance 與產品測試選 (a1)**（理由：它們驗的是產品的 wire 與畫面，直接用外部組件造情境是夾具的性質；改寫它們不帶來產品可見的 Feature；白名單讓新增被擋住）。理由：Feature 1 的原話是「用到它的產品（Match、Client、四個工具）不跟著改」——產品直接含 asio 時，升級 asio 產品就得跟著改，(b) 讓 Feature 1 對產品不成立。代價：守衛 N1（第三方含入棘輪）的白名單要列產品與工具的現況並逐批刪；產品的 asio／httplib 本來就在 match-host、client-link 的重寫範圍（0 增量）；nlohmann 在產品與工具的改寫依未決 1 的選擇，在各自的遷移批做（J3 時估 +4～8 h，分攤，不計入本稿合計）。

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

- I1：照 §3b，draw list 一律經 `IRenderDevice`（GPU）畫，SDL 2D 後端在 plugins-editor 退役。代價 1～2 h（退役）＋**`IRenderDevice` 擴充 2～4 h**（今天的介面沒有裁切矩形與索引子範圍的繪製，draw list 畫不出來——對抗檢查 B-3 (d)；Engine 公開介面的變更，plugins-editor 宣告列出）；**audio_editor、ui_editor 多出 GPU 需求**（工具執行環境的要求改變＝行為變更，要使用者核准）。
- I2：Engine 立一個小的中性 2D 介面（貼圖、幾何繪製；夠 draw list 用），SDL 2D 後端實作它。代價 +4～8 h；工具的執行需求不變；之後要退役 2D 只刪一個後端（可逆）。但它是只為 ImGui 在兩個工具上用的第二個裝置介面（AGENTS §1：單一 use case 不另立汎用抽象）。
- **建議：I1**，前提是「audio_editor、ui_editor 需要 GPU」可接受；不接受則 I2。由 plugins-editor 的事前宣告定案，但這個前提現在就要使用者表態（影響 plugins-editor 的估時與工具的使用環境）。風險案建議 I2、價值與成本案傾向退役（§5 取捨 T9）。計畫以 I1 估時（plugins-editor 的增量含擴充 2～4）；選 I2 時以 2D 中性介面 4～8 取代擴充 2～4（plugins-editor 再 +2～4）。

### 未決 10（target §4 的 Q2 句；新）

- 現行 `target/runtime-composition.zh-Hant.md` §4（a6-revision 依 Q2 寫入、已核定的本文）：「plugin 對它所包的第三方（SDL、ImGui）是第三方邊界…**只有該第三方的 adapter plugin 可以在公開標頭露出它的型別**」。原則 (3) 要求 adapter 的對外介面用 Engine 的詞——adapter 一露出第三方型別，消費者就看得到該型別。
- 建議改寫：「adapter plugin 的公開標頭只用 Engine 的詞；第三方型別只在該 plugin 的私有標頭，或明示的 native 子標頭（使用者限於同家族的 plugin 與該 plugin 自己的測試，清單只減不增）；例外（ImGui widget，未決 7）寫在該 plugin 的 owner 文件並列入它的 RC」。
- 這句是已核定的本文，**改它要使用者同意**；本稿沒有改寫它，target 頁也不加註（target 頁只寫已裁決的內容，對抗檢查 S-10）。
- **守衛的處置照已核准的 `v8-arch-guards-inventory-dispositions` Q2**（統合稿把它寫成「G1／G3 改寫成第三方 adapter 邊界守衛」——那是本題的答案，不能先當定案，對抗檢查 B-6 (ii)）：搬走的後端其 G1／G2 退役，新增 plugin 版標頭衛生守衛（只有該第三方的 adapter plugin 可在公開標頭露出它的型別）；G3 照盤點改寫（target 名）。**請一併確認的時點移動**：Q2 寫「plugins-frame 新增」，是因為原計畫的搬家在那批；本修訂的搬家提前到 adapters-relocate，新增若留在 plugins-frame，退役與新增之間有一段沒有守衛，所以建議新增也移到 adapters-relocate。
- 選項與差額：(i) 維持 Q2 句——照上段；(ii) 同意改寫——新守衛的規則改成「adapter plugin 的公開標頭只用 Engine 的詞、第三方型別只在私有或 native 子標頭」（第三方 adapter 邊界守衛），同在 adapters-relocate；兩案的工時差約 0～1 h（都在 adapters-relocate 的守衛 4～8 內；(i) 多一份 G1／G2 退役的 §14.4 文件，(ii) 的規則多一類 native 子標頭）。**建議 (ii)**（理由同上：原則 (3)）。

### 未決 11（runtime-composition 的 Design Invariants 增改；新；Architecture Delta，交使用者）

目標 DI 1～4 已由使用者 2026-11-13 核准（`v8-arch-target-di-approved`）；本節任何變更都是 Architecture Delta（AGENTS §3）。本稿**沒有改 DI 節**，草稿如下：

- **DI 2 增補**（價值案）：在「產品與工具的 plugin 選擇只寫在 `project.json`」之後加三點：選擇以可執行單位（role、variant）為粒度；第 1 類後端也經同一個欄選擇；產品與工具的 CMake 不寫 plugin 或後端的 target 名。守衛：N2（選擇欄的契約測試）、N4。
- **新 DI 5**（三案都提）：「外部組件的型別與呼叫只在它的 adapter plugin（每個組件一個）；Engine 與其他 plugin 的公開標頭只用 Engine 的詞；Engine 不連任何外部組件〔JSON 依未決 1〕。」守衛：N1（第三方含入棘輪，closeout 為 0 或核准的例外）＋plugin 版標頭衛生守衛（adapters-relocate 新增；規則依未決 10）＋依賴方向掃描。理由：沒有寫成 DI，長遷移期間局部合理的「這裡先直接 include 一下」會累積成漂移（AGENTS §3「判斷できる形で書かれた約束だけが…Drift を止められる」）。
- **新 DI 6**（風險案；價值、成本案併在 DI 2／DI 5）：「每個可執行單位用到的、需要後端的能力恰好有一個已選後端；缺或多是 configure 錯誤並指名能力。」守衛：§3.4 的合成反例。
- **新 RC 兩條**：(i) 升級一個第 1 類 adapter 時，它輸出的內容身分 (c) 改變（例：ufbx 的載入結果變了，`AnimationContentDigest` 跟著變）→ 不是「只改 adapter」，走內容身分的版本流程（風險案；Feature 1 的極限寫明、不藏）；(ii) 同一組件的對接出現在第二個 plugin 時，不放寬 DI 5，而是把對接搬回它的 adapter；介面不足就回 Engine 補介面（價值案）。
- **建議**：DI 2 增補＋DI 5＋DI 6＋兩條 RC 一起核准；最晚在 plugins-foundation 的事前宣告之前（N1、N2 要以它們為依據）。條文工時 0.5～1 h；守衛已算在 plugins-foundation 與 adapters-relocate。

### 未決 12（計畫 §8 的三處；新）

- **RC-8**（新 Delta 累計 2 個就重排）：Delta-7 是新 Delta。選項：(a) 算第 1 個（風險、成本兩案）——再出現 1 個就觸發；(b) 比照 Delta-6 的先例「寫進 §4 後不計」（價值案）。**建議 (a)**：本修訂本身就是 RC-8 要求的「回到規劃」，保守計數讓下一個 Delta 一定回到使用者。
- **RC-11**（總量錨點 1,299 h）：本修訂把非條件合計上限加到 **1,400 h**；全採本稿建議（J3 的 json-adapter 34、產品與工具改用中性文件樹 8）時 **1,442 h**（統合稿寫 1,368 並建議改成 1,368——沒算建議案的條件批，偏低；對抗檢查 S-8）。改錨點是改判定，但這是執行前、看到任何實績前的範圍追加（不是看結果後放寬，§14.1 不禁）。**建議：錨點＝非條件合計上限 1,400＋使用者選定的條件批與分攤項的上限**（全採建議時 1,442，觸發點 2,163 h）；不改則觸發點停在 1,948.5 h（相對全採建議收緊約 10%）。
- **RC-12（新增，風險案）**：任一 adapter 的搬家批需要同時改它的中性介面才能過＝介面沒有先立好，停、回到該介面所屬的批（防止搬家與介面改動混在一個回退單位）。**建議新增**（締める面）。

### 未決 13（其餘能力模組的歸屬確認；HANDOFF〈未定〉(3)）

§1 表中標「建議（未決 13）」的 asset core、input core、io、map_markers、render core、text core、trace core、ui、voxel_mesh，以及 config（不適用）、entity_store（由 Delta-3 推出）、runtime（`RuntimeLoop` 已排程刪除、`FixedTickRuntime` 照順序進 runner plugin），照判斷順序都是**留 Engine**（對抗檢查後逐列補齊三問；trace core 的 Q2b 依 trace.md DI 6 改正為「固化」）。有判斷餘地、請特別看的：asset core 與 io 的 Q1 判「不成立」（若判成立，Q2a 無、Q2b 固化，仍留 Engine）；ui 的 Q2b 判「固化」（`gyo.ui` 是有版本的資料契約、Client 與 ui_editor 共用；`UiRenderer` 的重寫是形狀不是規則）——若使用者認為 UI 的繪製規則是「會被替換的規則」，照順序 ui 會變成 plugin，那時另開一題。交使用者一次確認；確認後 HANDOFF〈未定〉(3) 結案（model、audio 在未決 3）。`v8-arch-docs-scope-of-target` 要求的「audio、input、platform-sdl、trace、io 由本批逐個定」：歸類見 §1，開 target 檔的批見計畫 §3 共通的新條（platform-sdl→wrap-render 開 `target/platform`、`docs/plugins/sdl` 由 adapters-relocate 建；input→wrap-render；audio→wrap-render（留 Engine 時）；trace→diagnostics-service；io→assets-core）。

### 未決 14（GYOP 契約與向量檔的 owner 與位置；對抗檢查 B-2）

事實：`engine/net/include/engine/net/GyopDatagram.hpp` 的檔頭說 GYOP 是 Engine 的 UDP 傳輸契約、與 Go gateway framing（`services/gyo_gateway/framing`）逐位元組共用；`services/gyo_gateway/framing/vectors_test.go` 的 `vectorFile` 以相對路徑讀 `tests/common/net/fixtures/gyop_datagram_vectors.json`。決定 (f) 讓 GYOP 編解碼的**實作**進網路 plugin、Engine 不留網路模組；但這份**跨語言契約**的 owner、向量檔放哪裡，決定行沒有說。`services/` 是共通層（`app_copy_integration.py` 把它當共通路徑複製）。

| 選項 | 內容 | 代價 | 張力 |
|---|---|---|---|
| G-a | 契約與向量隨實作進 `plugins/net_link/`（owner＝網路 plugin），同批改 `vectors_test.go` 的路徑 | 0.5～1 h | 共通的 Go service 的測試依賴一個 plugin 目錄裡的檔：刪網路 plugin 時 Gateway 的測試壞（AGENTS §7 的可刪性對 plugin 也適用） |
| **G-b** | 契約文件與向量檔放在 plugin 外的固定位置（例：`docs/` 下的契約頁＋向量檔，或保留 `tests/common/net/fixtures/` 原位），owner 寫在網路 plugin 的 owner 文件、Gateway 的文件互引；C++ 測試（隨搬進 plugin）與 Go 測試都讀同一份 | 0.5～1 h | 檔案位置與 owner 目錄不一致（同 T6 的顧慮），但契約本來就是兩個實作之間的東西，不屬任一方 |
| G-c | owner＝Gateway（`services/gyo_gateway`），向量檔搬到 `services/gyo_gateway/framing/` 下，C++ 測試讀它 | 0.5～1 h | plugin 的測試依賴 service 目錄；GYOP 原本是 Engine 定的契約，owner 換邊 |

**建議 G-b**：契約不屬兩個實作的任一方，放在兩者之外、由網路 plugin 的 owner 文件宣告 owner；刪網路 plugin 時 Gateway 的契約與測試仍成立。位置由 net-link 的事前宣告定。三案代價同量（計在 net-link 的 +3～6 內）。

### 未決 15（adapters-relocate 與未合併的 Match PR 線的順序；對抗檢查 S-7）

事實：adapters-relocate 排在 G2a 後；那時 match-sim＋match-host 是一條還沒合併的 PR 線（G2b 才合併，計畫 §1.2 第 4 條、U3），master 上仍是舊產品程式。兩者都要改：`apps/object_fps_pvp/src/Sim/PlayerAnimationContent.cpp`（含入 `UfbxModelLoader.hpp`）、`apps/object_fps_pvp/CMakeLists.txt`、`tests/object_fps_pvp/CMakeLists.txt`（`GYO::AssetUfbx`、`GYO::AssetSdlImage`、`GYO::PlatformSDL`）與 6 個測試檔的 `UfbxModelLoader.hpp` 含入、`build/acceptance/object_fps_pvp/`。統合稿 T3、T4 的「避免與 match-sim 改同一檔」「G2a～G2b 之間沒有人在改這些檔」只看了 framework 與 Client 線，不成立。

| 選項 | 內容 | 代價 |
|---|---|---|
| **R-a** | adapters-relocate 以 master 為基底、先合併；Match 線 rebase 吸收（重疊處都是含入路徑與 target 名的機械改寫）；byte 級等價與「Match 的連結集合差分為空」在本批以 master 上的舊 Match 判，Match 線 rebase 後由該線的 L1 再判一次 | 不改路徑；衝突解決計入 RC-7（>2 h 觸發改串行） |
| R-b | adapters-relocate 排到 G2b 合併之後 | 階段 2' 鏈 68～155 h 改從 G2b 起算，對 client-sim 59～118 h，關鍵路徑最多 +9～37 h（兩端各自相減，推定）；Client 線的 plugins-frame 跟著晚 |
| R-c | 拆兩段：engine／plugins／framework／tools／tests/common 在 G2a 後；apps、`tests/object_fps_pvp`、acceptance 的機械改寫在 G2b 後 | 中間 master 上產品要能 build，等於要相容的含入路徑——AGENTS §7 不許的 Compatibility Branch；不建議 |

**建議 R-a**（機械改寫的衝突成本低，且 RC-7 已有觸發條件）。

### 未決 16（過渡期 ufbx plugin 的 build 函式放哪裡；assets-core 的結束依賴改了 round2 的一句；對抗檢查 S-9）

- 事實 1：本修訂讓 ufbx plugin 在 assets-core 登記 `ModelAsset` loader，程式在 adapters-relocate 才搬。兩批之間 ufbx plugin 的 build 函式寫在哪個目錄、屬哪個 target、owner 是誰，統合稿沒寫。
- 事實 2：assets-core 的**結束**因此另依賴 plugins-foundation 段 (i)；決定行 `v8-arch-migration-plan-round2` 寫「assets-core 本身仍只依賴 world-exec／loading」。round2 的要點（登記者是 plugin，不是 host 或產品程式）不變，但依賴的說法變了。
- 選項（位置）：(a) assets-core 建 `plugins/ufbx/`，只放 build 函式、`plugin.json`（`base` 暫指 Engine 的 `GYO::AssetUfbx`）與 `docs/plugins/ufbx` 頁，owner＝ufbx plugin；loader 程式在 adapters-relocate 搬進同一目錄——ufbx 從過渡登記表移到它自己的 `plugin.json` 的時點提前到 assets-core。目錄擁有登記程式，不是 T6 否定的「什麼都不擁有的目錄」。(b) build 函式放 `engine/model/backend/ufbx`——Engine 目錄裡有一段 plugin 的登記程式，Ownership 與目錄不一致（T6 的理由同樣適用）。(c) 不在 assets-core 登記，改由 match-sim 寫（統合筆記的替代）——產品批寫架構 plugin 的 build 函式，Ownership 不順；但 assets-core 不依賴段 (i)、round2 的句子不變。
- 代價：三者同量（1～2 h，已計在 assets-core 的 +1～2）。
- **建議 (a)**，並請架構師與使用者確認 round2 那一句改為「assets-core 開工只依賴 world-exec；結束依賴 loading 與 plugins-foundation 段 (i)（ufbx 的登記）」。段 (i) 拖到 assets-core 的結束已寫成停止條件（計畫 §3.11）。

### 未決 17（知悉：U4 當時接受的代價變了；對抗檢查 B-5 (d)）

§7 U4（已定 (c) 兩段閘門）的代價句：「若 G2b 不過而 G2a 過，2' 階段約 70～153 h 的工作可能要改；這些批的產出都在 kernel／engine 內」。本修訂後 2' 階段＝rollback-extract 25～46＋adapters-relocate 21～42＋wrap-render 24～63＋wrap-assets 9～20＋plugins-frame 14～30＝**93～201 h**（選 J3 再加 json-adapter 17～34），且 adapters-relocate 改的不只 kernel／engine（apps、tools、framework、tests、acceptance 的含入路徑與 target 名）。§7 是已定事項，本稿不改它的文字；請使用者知悉，若因此要重看 U4，照計畫 §8 RC-9。

### 流程事項（不是使用者的設計決定）

- **產出 6 已做**：入口 `docs/architecture/README.md` 已由 PR #210 合併進 master（修訂時本分支已併入）；`Layers and dependencies` 條目刪了「Decisions not yet written into §4…」一句，只動這一句（檔案歸圖書管理員，連結目標歸架構師，`v8-arch-architecture-entry`）。統合稿寫「入口不存在」是當時的分支落後 #210（對抗檢查 S-11）。
- 未查證、交執行批先查：Windows 上把 SDL3.dll 放到測試執行檔旁的機制在搬家後是否照舊——寫進 adapters-relocate 的事前宣告第一步。SDL_Init 的分工已有答案（platform-sdl.md DI 8：Engine 不呼叫全域 `SDL_Init`／`SDL_Quit`，子系統由各 owner 參考計數；對抗檢查 S-6），不再列為未查證。

---

## 5. 三案取捨

| # | 主題 | 風險案 | 價值案 | 成本案 | 本稿 | 理由與放棄的代價 |
|---|---|---|---|---|---|---|
| T1 | 選擇欄只升一次版、刪 components | 是 | 是 | 是 | **採** | 三案一致 |
| T2 | 選擇的單位 | app 頂層、tool variant | **app `roles`、tool variant** | **app `roles`、tool variant** | 採價值、成本案 | Feature 2 要以可執行單位表達；放棄的代價：object_fps_pvp 的 `project.json` 多一層 |
| T3 | ufbx 的時點 | assets-core 經過渡登記登記；程式在 match-sim 後另開 `plugin-ufbx-relocate` 搬 | assets-core 搬＋登記（關鍵路徑 +4～8） | 段 (i) 搬、assets-core 登記 | **登記在 assets-core（路徑 +1～2；build 函式的位置依未決 16）；程式在 adapters-relocate 搬** | 三案都刪掉 pvp 最小 plugin 與 model-assets（ufbx 唯一的 adapter）。不在關鍵路徑上搬（價值案的 +4～8），也不擠進段 (i)（成本案：段 (i) 對 world-exec 的餘裕只剩數小時）；搬在 match-sim 之後（風險案的理由是避免與 match-sim 改同一檔 `PlayerAnimationContent.cpp`——**對抗檢查 S-7：不成立**，match-sim 是到 G2b 才合併的 PR 線，G2a 後的 adapters-relocate 仍與它改同一檔；順序見未決 15），不另開一批（併進 adapters-relocate，省一次交接）。代價：ufbx 的 Feature 1（升級只改 plugin）晚到 G2a 後；與 Match PR 線的重疊要 rebase 吸收 |
| T4 | SDL 家族的時點與批 | 中性介面在 wrap-render 原地立；SDL_image／ttf 在 wrap-render 後一批；SDL 本體在 framework-delete 後一批（尾段路徑 +4～5） | plugins-frame 一次搬（plugins-frame +10～20） | **新批 B：G2a 後、wrap-render 前，只搬不改形狀** | **採成本案的形，併入 ufbx：`v8-arch-adapters-relocate`** | 搬與改形狀分成兩種 diff（審查成本小、搬的部分可交小模型＋腳本檢查）；不進關鍵路徑（階段 2' 鏈 68～155 h 短於 G2a→client-sim 125～260 h）。放棄風險案的「消費者遷完才搬、少碰 framework 與舊產品程式」：代價是 engine 外 65 檔的含入路徑機械修改（6～12 h，含在本批），其中一部分在之後被刪；G2a～G2b 之間 framework 與 Client 線都沒有在改這些檔，但**未合併的 Match PR 線在改其中的 apps、`tests/object_fps_pvp`、acceptance 檔**（統合稿漏看，對抗檢查 S-7）——RC-7 的衝突風險不是「低」，合併順序見未決 15。也放棄價值案的「plugins-frame 一次做」：plugins-frame 的 diff 會同時含 runner 新寫與大量搬檔 |
| T5 | 能力與後端的綁定寫法 | 名字清單＋`uses` 推導 | **`backs` 寫在產品的選擇裡** | 名字清單＋`requires_capabilities` | 採價值案（S-A），推導形列次選 | 「這個單位用 SDL 當哪些能力的後端」在資料上看得到；P2 讀法下 Feature 2 不靠推導。代價：產品資料多寫 `backs` |
| T6 | 過渡期的登記放哪 | `plugins/<name>/plugin.json` 指向 Engine target | **共通 build 的過渡登記表** | `plugins/<name>/plugin.json` 指向 Engine target | 採價值案 | 不出現「名為 SDL plugin、什麼都不擁有」的目錄（Ownership 與目錄一致）；表只減不增、一處對帳。代價：搬家批除了新增 `plugin.json` 還要刪表的一列（共通 build 資料的一行，是遷移本身的變更） |
| T7 | 中性介面的形式 | 新立 platform 後端介面、輸入注入、音訊輸出、2D（I2）；擴 `IRenderDevice` | platform 中性層、input 後端介面、audio 輸出介面 | **不設 OO 介面類**；中性契約＝wrap-render 定的 Resource／boundary 型別 | **混合**：platform 要有中性契約（runner 不含 SDL 時得呼叫它），形式由 wrap-render 定、不做鏡像類；audio 不另設輸出介面類（`Mixer`／`Submitter` 就是 Engine 的詞）；輸入注入二選一；2D 交未決 9 | 三案都反對鏡像 `SdlPlatform` 的大類別；成本案「付兩次中性化的錢」的警告採納為「只立 runner 與 boundary 真正要用的入口」。代價：wrap-render +4～8 h |
| T8 | JSON | J3，條件批排在階段 1 | A（≈J3） | J4 | 建議 J3，條件批排在 G2a 後 | 原則對 JSON 成立且符合使用者「可插拔」的方向；排在 G2a 後不加 G2a 前架構線的負荷（成本案：G2a 前架構線串行和 129～268 h 與路徑同量級）。放棄風險案的「(c) 守衛早在新路徑上綠」 |
| T9 | SDL 2D renderer | I2（中性 2D 介面） | plugins-editor 二選一（退役／補實作 `IRenderDevice`） | 退役候選 | 建議 I1（退役），前提交使用者 | target §3b 已定 draw list 由渲染 World 畫；I2 是單一 use case 的第二個裝置介面。風險案指出的 GPU 需求改變寫成前提，不藏 |
| T10 | window runner | `window_runner` 不含 SDL | `window_runner` 不含 SDL | 保留 `sdl-window-runner` 名 | 採風險、價值案 | 原則 (1)(2)：SDL 呼叫只在 SDL plugin；runner 經中性 platform 契約 pump |
| T11 | imgui 的 SDL glue | plugins-editor +8～16 h 拆掉 | +2～4 h | 0 | **+8～16 h（風險案）＋I1 的 `IRenderDevice` 擴充 2～4** | 統合稿取價值案 +2～4，前提是「拆 SDL glue 已在原範圍」，但沒讀 audit 原文。對抗檢查 S-8 後查證：audit 的 `ImGuiShell` 12～24 h 是「Host（骨架重寫）」，不含 SDL glue 的替換；原 Q2 允許 imgui plugin 經 runner 拿 SDL 句柄、平台半可沿用 `ImGui_ImplSDL3_*`，現在平台半（按鍵對照、游標、剪貼簿、輸入法位置）與繪製半（`ImGui_ImplSDLRenderer3_*`）都要自己寫，繪製半還要 `IRenderDevice` 有裁切與索引子範圍（B-3 (d)）。改取風險案 |
| T12 | 守衛先於搬動 | 掃描根加 `plugins` 提前到段 (i)、含入棘輪、ctest 名單比對、`if(TARGET)` 14 處硬性化 | N1～N6 | 通用第三方 adapter 邊界守衛取代 G1／G3 | **收風險、價值案；成本案的「取代 G1／G3」不收** | 三種「靜默消失」（`if(TARGET)` tests/common 14 處＋G3 函式內 1 處＋engine 外 4 處；OS 邊界三件不掃 `plugins`——`ROOTS` 與 `RUNTIME_ROOTS` 兩組；plugin 沒被選時它的測試不建）在第一份 `plugins/` 程式與第一次搬家之前就位；硬性化從 wrap-render 移到 adapters-relocate 的第一步（target 在那裡改名）；工具與 framework 守衛的路徑規則在搬家同批改寫。G1 的去處照已核准的 Q2（退役＋plugin 版標頭衛生守衛新增），「改寫成第三方 adapter 邊界守衛」是未決 10 的選項 (ii)，不先當定案（對抗檢查 B-6 (ii)） |
| T13 | net 的時點 | net-link | net-link | 段 (i) | 採 net-link | net-link 本來就建 `plugins/net_link`；段 (i) 的餘裕留給選擇欄 |
| T14 | 估時 | +47～96，合計 679～1,395 | +27～54，659～1,353 | +18～43，650～1,342 | **+49～101，681～1,400**（統合稿 +33～69、665～1,368；全採建議 702～1,442） | 逐項取有出處或兩案相近的值（計畫 §2.2 修訂列各寫組成）；對抗檢查後補 S-3（段 (i) +3～6）、B-1／B-2（net-link +1～2）、S-4／S-5／S-6（adapters-relocate +3～6）、B-3 (a)（wrap-render +1～2）、S-8／B-3 (d)（plugins-editor 改取風險案＋擴充，+8～16）；全部推定 |
| T15 | Delta-7 與 RC-8 | 算 1 | 比照先例不計 | 算 1 | 建議算 1（未決 12） | 保守 |

**不採的**：J2（三案都不建議）；P1（未決 5）；把全部搬家塞進 G2a 前（成本案 Z：G2a 前架構線 +13～26 h）；把搬家併進 wrap-render（成本案 X：diff 混合「形狀」與「搬檔」）；先建只有登記檔的 `plugins/<name>/`（T6）。

---

## 對抗檢查處置記錄（Fable／high 一次；給使用者的審計線）

報告：主 checkout `build/evidence/adapter-plugins-plan/adversary-report.md`（blocker 6／should 13／nit 8，git 管理外）。修訂：Opus／high。事實重驗的掃描在同目錄 `reviser-scan.{py,txt}`（`git grep -n -I -P` 以 Python 包，回傳碼 0＝命中、1＝無命中、其他＝中止；stderr 不丟；正例 `#\s*include\s*[<"]SDL3/` 在 `engine/platform/sdl/native` 命中 `SdlPlatformNative.hpp:7`，rc=0）；批數與工時的重算在 `reviser-sum.{py,txt}`。處置三種：**修正**、**不採**（附理由）、**列未決**（交使用者）。「修正＋列未決」表示事實與計畫已改、但其中有一個選擇交使用者。

**統計**：blocker 6＝修正 4、修正＋列未決 2（B-2→未決 14；B-5 (d)→未決 17）；should 13＝修正 10、修正＋列未決 3（S-7→未決 15；S-9→未決 16；S-12→併入未決 6）；S-1 新提的 `unused-backend` 是錯誤還是警告交 plugins-foundation 的宣告（設計細節，不是使用者題，記為修正）；nit 8＝修正 8。不採 0。

| # | 處置 | 內容 |
|---|---|---|
| B-1 | 修正 | 重驗成立：asio／httplib 的 CMake 連結者 7 行（apps 2、`tests/object_fps_pvp` 2、`build/acceptance/object_fps_pvp` 3），直接含入另有 `ConnectionLivenessTests.cpp`、`IpcHostTests.cpp`、`worker_main.cpp`、`network_main.cpp`。net-link 的判定改成「**加入點**只在網路 plugin；**連結者**＝網路 plugin＋N7 白名單（今天 7 行，只減不增）」；N7 的歸零點寫成 match-host（`ipc`）、client-link（`client_network`），tests 與 acceptance 依未決 6（建議 (a1)：夾具、列白名單）；加 `plugins/` 先於 apps／tests／acceptance configure 的順序檢查（owner 之間的隱含順序）。另：match-host 原判定「`apps/object_fps_pvp` 的 asio 含入 0」在 G2b 時也不成立（`ClientConnection.cpp` 到 client-link），收窄到 `IpcHost.cpp`／`ipc`。計畫 §2.2、§3.12、§3.16、§3.22、§5.2 N7；net-link +1～2 h |
| B-2 | 修正＋列未決 14 | 重驗成立：`services/gyo_gateway/framing/vectors_test.go` 讀 `tests/common/net/fixtures/gyop_datagram_vectors.json`；`GyopDatagram.hpp` 檔頭寫與 Go gateway 共用；漏列的消費者 `tests/object_fps_pvp/CMakeLists.txt`（`GYO::Net`）、`worker_main.cpp`（`EncodeGyopDatagram`）。§1 第 13 列、§2 net 列、計畫 §3.12、Delta-7 第 4、6 項補齊；判定加「Go 測試照跑」；契約與向量檔的 owner 與位置列未決 14（建議 G-b：放在兩個實作之外） |
| B-3 | 修正 | (a) audio：補 `SdlAudioOutputConfig`、`OutputAnchor`／`TakeAnchors`、`Stats()`／`SdlAudioOutputStats`、`DriverName()`、`DeviceSampleFrames()` 與三個消費者（`PvpSound.cpp`、`AudioEditorWindow.cpp`、`PreviewSounds.cpp`）；wrap-render 的 Contract 加「輸出的設定、錨點、統計成為 Engine 的詞，或寫明不再取用」，+1～2 h。(b) sdl_gpu：改成靜態工廠 `Create`／`CreateOffscreen`（private 建構子）；`SetFrameSize` 是離屏專用、唯一非測試呼叫者 `framework/src/Preview.cpp`，隨 readback 處置；刪「`IRenderDevice` 補尺寸」。(c) 型別記號命中改成「5 行、4 檔，全為註解」（`reviser-scan.txt` B3c）。(d) `IRenderDevice` 沒有裁切與索引子範圍：寫進 sdl_gpu 列、未決 9、計畫 §3.24，I1 的代價加 2～4 h |
| B-4 | 修正 | 歸類表照判斷順序逐列補三問：model core、audio core 照順序是 plugin、本稿建議留 Engine——歸屬改「待使用者（照順序是 plugin；建議留 Engine，理由 R3…）」，未決 3 改寫成「對準則開例外」的三選項；trace core 的 Q2b 依 trace.md DI 6 改正為「固化」（Engine 不持有記錄格式；這是事實更正，不是為了配合結論改答案）；ui 刪成本理由、以資料契約判固化並在未決 13 標明判斷餘地；runtime 補 `FixedTickRuntime` 的三問；asset core、io 補「若 Q1 判成立」的備援答案；停止條件的檢查段補「照順序的檢查」。R3 明寫不是判斷順序的一部分 |
| B-5 | 修正（(d) 列未決 17） | (a) §5.2 缺口 8、9 的歸批加「本修訂移到 adapters-relocate」；(b) §2.3 說明段與 §2 取捨的「9～18 h 可交」「不依賴 plugins-foundation」加修訂註（最小形 20～40、結束依賴段 (i)）；(c) §3 共通第 6 點的名單加 adapters-relocate、改成「非條件 30 批＝30～60 h」，§2.2 合計的修訂前說明加註；(d) §7 U4 不改文字，代價的變化（70～153 → 93～201 h、產出不限 kernel／engine）寫進計畫修訂記錄與未決 17 交使用者知悉；(e) §3.19 改「assets-core 13～34 h ≥ loading 11～22 h」。全部數字以 `reviser-sum.py` 重算 |
| B-6 | 修正 | (i) target 頁 SDL 家族條刪「缺了是 configure 錯誤、不自動補；初始化順序依宣告的依賴決定」，只留決定行 (e) 的原意（初始化順序仍是上一條「由資料或 plugin 宣告的依賴決定」，二選一由 plugins-foundation 定）。(ii) G1 的去處回到已核准的 `v8-arch-guards-inventory-dispositions` Q2：G1／G2 對搬走後端退役、新增 plugin 版標頭衛生守衛、G3 改寫；新增的時點隨搬家從 plugins-frame 提前到 adapters-relocate（避免空窗；列進未決 10 一併確認）；「改寫成第三方 adapter 邊界守衛」降為未決 10 的選項 (ii)，估時差 0～1 h。計畫 §2.1、§2.2、§3.17b、§3.18、§3.20、§5.2 第 9 列、Delta-7、T12 同步 |
| S-1 | 修正 | (1) role 名：寫明「哪些 role 存在只在 `roles` 宣告，CMake 以名字引用、兩向 `role-mismatch`」的讀法與替代（資料產生 target），由段 (i) 宣告定；(2) `backs` 與 `requires_capabilities` 的分工寫明；(3) 加錯誤 `unused-backend`（綁定未用），configure 錯誤 8 → 9 種（是否降為警告由段 (i) 定）。§3.1、§3.3、§3.4、計畫 §2.2、§3.11、N2 |
| S-2 | 修正 | 結論 1 改成「機制在段 (i)，對真實產品生效要等各產品的 plugin」；過渡登記表的列（含 `FRAMEWORK`、`IMGUI`）帶 `requires_capabilities`，寫進段 (i) 的範囲の限界。§0、§3.3、§3.5、計畫 §3.11 |
| S-3 | 修正 | 段 (i) 範圍補：產品 CMake 的 `*_COMPONENTS` 參數與函式庫層級連結（`apps/object_fps_pvp/CMakeLists.txt:59-62`）、`GyoRenderOptions.cmake` 的後端選項（建議降為「能不能建 GPU 後端」的開關）、`GyoPackaging.cmake` 的第二處 `SDL_` 前綴、登記契約的既有測試 8 份的去處；刪推導的判定變更照 §14.4 寫 1～5。計數重驗與報告不同（以 `"components"`／`role_components`／`COMPONENTS` 計：ComponentTests 2、ProjectComposition 4、ToolRegistry 12、ProductPackaging 1、GoService 1、RenderOptions 0、test_app_registry 2、test_release_channels 1；報告的 4、19、1、1、2 未說明計數基準，結論相同）。段 (i) +3～6 h（20～40；全批 30～60） |
| S-4 | 修正 | (1) G2 的處置照 Q2（對搬走後端的部分退役、對 `engine/**/include` 留，守「Engine 標頭一律不可」），DI 1 的緩める面在 §14.4 寫明、由新守衛承接；(2) 五支守衛的路徑規則逐支列進 adapters-relocate（`ALLOWED_INCLUDE_PREFIXES` 只加確切新路徑＝緩める面、照 §14.4；`WINDOW_ONLY_INCLUDES`、`NATIVE_HEADER(S)` 加新路徑），判定加「舊規則的已知正例在新路徑上仍命中」；§5.2 第 10、11 列改寫（刪「件數不變、不放寬」）；(3) 含入根不得沿用 `engine/` 前綴，寫進 §3.17b |
| S-5 | 修正 | (1) `RUNTIME_ROOTS` 也加 `plugins`；(2) 中央 allowlist 的 `tests/common/platform_sdl/EntryTests.cpp` 兩列在 adapters-relocate 移列；(3) 段 (i) 宣告寫明加入時 `plugins/` 的狀態（不存在＝靜默跳過、存在但無 C/C++ 檔＝失敗；`os_boundary_guard.py` 的 `scanned_files` 重驗）；(4) engine 外 4 處 `if(TARGET GYO::…)`（acceptance 1、framework 2、imgui binding 1）列進硬性化，判定加 acceptance 的執行檔清單相同。adapters-relocate +1～2 h（計在 +3～6） |
| S-6 | 修正 | §2 新增「platform-sdl.md 整份的去向」：DI 1～10、RC 1～9 逐條寫落點（`docs/plugins/sdl` 或 `target/platform`）、寫入的批與突變（m1～m4、m-zero、m20～m22、m-watch-*、m10、m11、m14、m15、m-main-thread、m-nest、pd-*，以 platform-sdl.md 重驗）；RC 1 寫成「由本修訂提前滿足」及其改寫；「SDL_Init 未查證」改引 DI 8。計畫 §3 共通的文件落點條加指向 |
| S-7 | 修正＋列未決 15 | §3.17b 寫明基底（master）、與 Match PR 線重疊的檔清單（重驗 `PlayerAnimationContent.cpp`、`apps/object_fps_pvp/CMakeLists.txt`、`tests/object_fps_pvp/CMakeLists.txt` 三處與 6 個測試檔、acceptance）與兩處判定在哪個基底上判；T3、T4 的前提改正；順序三選項列未決 15（建議 R-a：先合併、Match 線 rebase） |
| S-8 | 修正 | (1) 合計寫兩個數：非條件 681～1,400、全採建議 702～1,442；RC-11 的建議改成「錨點＝非條件上限＋使用者選定的條件批」（未決 12）；(2) 查 audit：`ImGuiShell` 12～24 h 是骨架重寫、不含 SDL glue 替換——plugins-editor 改取風險案 +8～16，加 I1 的擴充 2～4，合計 31～62 h（T11）；(3) 未決 9 的 I1 代價同步 |
| S-9 | 修正＋列未決 16 | (1) 「assets-core 的結束依賴段 (i)」改了 round2 的一句，寫進計畫修訂記錄「與既有決定行的差異」與未決 16；(2) 過渡期 ufbx build 函式的位置三選項列未決 16（建議 `plugins/ufbx/` 只放 build 函式與登記，owner＝ufbx plugin）；(3) 「段 (i) 超過 25 h 通知架構師」改成停止條件（assets-core 只差 ufbx 登記而段 (i) 未完成＝停、回報，附兩個回退選項） |
| S-10 | 修正 | target 頁 §4 的三個〔…〕註全部移出（未決 4、5、2、6、10 的讀法只在本檔 §4），「各模組的歸類表在同檔 §1」「提案在同檔 §3」兩個指向待定內容的句子也刪；檔頭「更新」改 2026-11-13。Design Invariants 節修訂前後 md5 相同（`be34398d97c60983a79c1d4041828b6e`，與 `9bcb959b~1` 一致） |
| S-11 | 修正 | 入口 README 已在 master（PR #210），本分支已併入；產出 6 執行：`docs/architecture/README.md` 的 `Layers and dependencies` 條目只刪「Decisions not yet written into §4…」一句；本檔第 9 行與流程事項改寫 |
| S-12 | 修正（併入未決 6） | 未決 6 的事實補 tests 與 acceptance 的直接使用（asio、httplib、protobuf、nlohmann、SDL、ImGui 的 SDL renderer），選項加 acceptance 與產品測試的 (a1)／(a2)（建議 (a1)）；N1 與 closeout 的例外清單加「依未決 1」；N4 的基線改成「段 (i) 當時的掃描結果」，列出今天已知的 `GYO::PlatformSDLMain`、`GYO::Net` 2 處 |
| S-13 | 修正 | 計畫 §3.12 的 Contract 與差分統一：IO 服務只搬位元組（U7）；GYOP 編解碼是網路 plugin 提供、**產品呼叫**的函式庫，IO 服務執行緒不拆框 |
| N-1 | 修正 | SDL 的 target 9 個（含 OBJECT 的 `gyo_platform_sdl_main`）：§1 第 30 列、計畫 Delta-7 第 2 項 |
| N-2 | 修正 | nlohmann：6 個模組的 9 個 `.cpp`（含建置期 `pipeline/src/main.cpp`）＋base 的 `StrictJson.hpp`（第 7 個模組）：§1 第 28 列、未決 1 |
| N-3 | 修正 | engine 外含入後端標頭的檔重數為 65（apps 4、build 3、framework 6、tests 39、tools 13）：計畫 §2.2、§3.17b、T4 |
| N-4 | 修正 | `SdlInputInjector` 寫明兩種計數基準（類名：tests 1、acceptance 2；含入 `SdlInputInjection.hpp`：tests 2、acceptance 2）：§2 input 列、計畫 §3.18 |
| N-5 | 修正 | json-adapter 與 adapters-relocate 同在包線：改成「串行或開第二張卡，list scheduling 以串行計」：計畫 §2.2、§3.17c、未決 1 |
| N-6 | 修正 | 過渡登記表的定義改成「plugin 名已定、實作還不是 `plugins/<name>/` 的 plugin」，列出 `FRAMEWORK`、`IMGUI` 與各列的退出批；「只剩 JSON」刪（J3 的 adapter 在 Engine 內，不會有列）：§3.5、計畫 §3.29 |
| N-7 | 修正 | 計畫 Delta-7 第 2 項的「偵察 B 的 A1～A3、A11、A12」改引本檔章節與計畫修訂前的段落 |
| N-8 | 修正 | `GYO_TEST_PROFILE=full` 遇到第二個後端時改成每個後端一個測試單位、不放寬 `duplicate-backend`；`app_copy_integration.py` 的複製前綴加 `plugins/`：§3.5、計畫 §3.11 |

**修訂者沒有把握的地方**（交主會話）：(1) 估時的增量全為推定，特別是 S-3 的 3～6 與 adapters-relocate 的 3～6；(2) S-1 的 `unused-backend` 是本稿新提的錯誤，可能讓工具的選擇比今天嚴（例：先綁後用）；(3) B-4 中 trace 與 ui 的 Q2b 是以文件事實判「固化」，ui 的判斷有餘地，已在未決 13 標出；(4) 未決 15 的建議 R-a 依賴 Match 線 rebase 的衝突真的只是機械改寫——Match 線若同時大改那幾個 CMake 檔，衝突成本可能超過 RC-7 的 2 h；(5) list scheduling 的完成點只給了 G2a 前的後移量，G2a 後 Engine／plugins 線的 +33～68 h 是否成為瓶頸未重算（原計算未保存）。

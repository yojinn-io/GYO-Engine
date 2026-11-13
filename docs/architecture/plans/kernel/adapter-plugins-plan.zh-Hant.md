# 外部組件改成 adapter plugin 的規劃（修訂稿）：`v8-arch-adapter-plugins-plan`

件数の突き合わせ：engine 頂層模組 **23＝23**（事前宣告與 `ls engine` 一致）；third_party **10＝10**（一致）；建置期外部組件 **3**（事前宣告寫 2——**不一致**：偵察 A 多查出 SPIRV-Cross，`engine/render/shaders/pipeline/cmake/Dependencies.cmake` 的 `gyo_spirvcross`、`src/main.cpp` 含 `<spirv_cross_c.h>`；不是判定失敗，三份提案的重數都是 3）；歸類表主列 **36＝23＋10＋3**（一致），另有後端子列 7 與建置期工具子列 1；後端 **9＝9**（一致：7 個子列＋本身就是後端的 platform、net 兩個主列）；受影響批：既有 **17**＋新增 **2**（偵察 B 數 17；本稿既有批也是 17，**數目一致、組成差一處**：偵察 B 列了 wrap-assets，本稿判它不變；本稿另列 closeout（過渡登記表與含入棘輪的歸零判定））。**對抗檢查後**：engine 外含入後端標頭的檔 **65**（統合稿寫「約 67」，沿用成本案未重數——**不一致**，已改；apps 4、build 3、framework 6、tests 39、tools 13）；後端公開非 native 標頭的外部型別記號 **5 行、4 檔，全為註解**（統合稿寫「只命中 2 處註解」——**不一致**，已改；「型別 0 件」的結論不變）；`engine/net` 的消費者 **6 處**（統合稿列 1 處——**不一致**，已補）；asio、httplib 的 CMake 連結者 **7 行**（apps 2、tests 2、acceptance 3；統合稿未數）；對抗檢查 blocker 6／should 13／nit 8 ＝ 處置記錄 27 條（**一致**，文末）；掃描與重算在主 checkout `build/evidence/adapter-plugins-plan/reviser-scan.{py,txt}`、`reviser-sum.{py,txt}`。**第二次修訂（複核與討論後）**：歸類表主列 36 不變（新讀法下 0 列與已定歸屬相反）；configure 錯誤 9 → **3**；補遺二的新守衛 7 → **3**（N1 改為只掃 `engine/`、N2、N3；N4～N7 刪）；未決 17 → **0**（17 題全部已定，§4）；受影響批：既有 17＋新增 **3**（adapters-relocate、json-adapter 由條件改非條件、規劃批 B1）；計畫的批 36 → **37**（非條件 34 → 36、條件 2 → 1）；非條件合計 681～1,400 → **700～1,448 h**；重算在同目錄 `reviser2-sum.{py,txt}`（git 管理外）。

- 狀態：**第二次修訂稿（複核與討論後；17 題全部已定；待 Fable 檢查最終稿一次——判定 6 (c)）**；第一次修訂稿（Fable／high 敵對檢查一次之後）的內容見 git 歷史 `b647de39`。日期 2026-11-13。Owner：架構（架構師會話）。
- 性質：ultracode 小編制的統合（事前宣告 `adapter-plugins-plan-declaration.zh-Hant.md` §4）：偵察 Opus／high ×2、提案 Opus／high ×3（①風險與可逆、②價值、③成本）、統合 Opus／high；敵對檢查 Fable／high 一次（blocker 6／should 13／nit 8，報告在主 checkout `build/evidence/adapter-plugins-plan/adversary-report.md`）；修訂 Opus／high（第一次；逐條處置見文末「對抗檢查處置記錄」）；第二次修訂 Opus／high（照 review 決定行與未決的決定；文末「複核與討論後的修訂記錄」）。
- 基準：工作樹 `GYO-Engine-arch`，分支 `claude/arch-adapter-plugins-plan`；統合稿在 HEAD `fd012f4b` 上寫、提交為 `9bcb959b`；修訂時已併入 master（`b77ba68b`，含 PR #210 的入口 README），修訂未提交。偵察與三份提案的原文在主 checkout `build/evidence/adapter-plugins-plan/`（`recon-engine.md`、`recon-plan.md`、`proposal-{risk,value,cost}.md`、`proposal-risk-scan.{py,txt}`；git 管理外）。
- 已定前提（不重開）：kernel `HANDOFF.md` 決定行 `v8-arch-external-components-as-adapters`（立場、三條原則、方向 C、plugin 兩類、細節六題 (a)～(f)）、`v8-arch-engine-or-plugin-criteria`、`v8-arch-plugin-selection-data`、`v8-arch-framework-retire`、`v8-arch-docs-scope-of-target`、`v8-arch-architecture-entry`；**`v8-arch-adapter-plugins-review`**（2026-11-13，複核討論；與上列決定行不同處以它為準）與 HANDOFF 現況末尾「#212 的使用者決定」兩行；未決 1 的使用者原話（主會話轉達）。本稿的「建議」都交使用者；「推定」＝無出處的估計；「未查證」＝本次沒讀到能確認的出處。
- 本稿改的其他檔：`migration-plan.zh-Hant.md`（文末「修訂記錄（v8-arch-adapter-plugins-plan）」列出改了哪些段）、`target/runtime-composition.zh-Hant.md` §4（本文；只寫使用者已裁決的內容，待定的讀法都在本檔 §4；Design Invariants 節不動，逐位元比對）、`docs/architecture/README.md`（產出 6：`Layers and dependencies` 條目刪「待寫入 §4」那一句；入口已由 PR #210 合併進 master；第二次修訂在同一條目加一句區分「Engine 自有 plugin」與「`plugins/` 的擴展」）。第二次修訂：runtime-composition §4 照 review 決定行改寫（只寫已裁決的內容；Design Invariants 節不動，md5 相同）。

## 0. 結論（先寫；第二次修訂後）

1. **根因與方向**（`v8-arch-adapter-plugins-review`）：第一次修訂把「怎麼碰外部組件」（軸 1，三原則）綁到「能力放哪」（軸 2，判斷順序）——選項 C「adapter 都是 plugin」加草稿 DI 5「Engine 不連外部組件」，使判為 Engine 的能力（JSON、audio core、model 取樣）被強推成 plugin、逐個開例外。改為：**軸 2 決定歸屬**（判斷順序，第一問用 Q-b 檢查，(b) 讀作「產品會在多個實作之間選的規則」）；**軸 1 在哪一層都適用**——Engine 的能力用到外部組件時 adapter 在 Engine 裡，歸 plugin 的能力 adapter 在那個 plugin（選項 C 改窄），產品獨有的能力屬於產品、產品連外部組件時經對接它的 plugin（R3）。plugin 是組成單位，不等於在 Engine 外面；「Engine 自有 plugin」另開規劃批 B1。
2. **Feature 2、3 的機制在 plugins-foundation 段 (i) 交付**：選擇欄一次升到 version 2（components 併入、刪除），可執行單位（app 的 role、tool 的 variant）各自一份、**只寫 plugin 名**；「用到哪些需要後端的能力」由段 (i) 宣告先驗以 CMake 連結圖推導，不可行時回宣告式（plugin 登記資料寫 `requires_capabilities`），兩種情況產品都不另寫；選了 plugin 自動補它依賴的 plugin；configure 錯誤只有三種（缺後端、多後端、選了或依賴了不存在的 plugin）；還沒搬進 `plugins/` 的後端以過渡登記表掛最終名；登記格式預留「Engine 自有 plugin」一類的形狀、不實作（§3）。產品與工具因這次改造多出的步驟只有「在選擇欄寫要用的 plugin」（Feature 2 本身）與 R3 的「產品連外部組件經對接它的 plugin」。
3. **Feature 1 只對 Engine 承諾（範圍 (α)）**：「升級一個外部組件只改對接它的那一塊」對 Engine 成立；產品用了 plugin 露出的第三方型別（(iii) 允許），升級時產品自己負責。「升級」靠型別隔離今天已大致成立：9 個後端的非 native 公開標頭含入外部組件 0 件、型別 0 件（§2）。守衛只守 Engine 這一側（Engine 側的含入與連結掃描、標頭衛生），產品側的棘輪與白名單全部刪。
4. **歸屬**（§1）：model 取樣規則、audio core、ui、JSON 留 Engine（固化或 Q-b 不成立，不是例外）；**JSON 已定**——一個 Engine 的 JSON 模組以 **yyjson** 為唯一的函式庫 adapter，數字走原文路（adapter 交出原文、Engine 以 `std::from_chars` 轉），介面是 GYO 自己的（未決 1）；網路（含 `engine/net`）在 plugin；SDL 家族與 shader 建置期工具在 SDL plugin（P2）；ImGui 是第 2 類 plugin，它官方的 SDL3／SDL_GPU 後端放進 SDL plugin 內的 `sdl_imgui` target（RC (ii) 選 (ii)）。
5. **搬法**：ufbx 由 assets-core 登記、成為唯一的 ufbx adapter（`plugins/ufbx/` 在 assets-core 建，未決 16 (a)）；`engine/net` 併進 net-link；`v8-arch-json-adapter`（非條件批）在 G2a 後、adapters-relocate 之前做，shader 工具在它搬進 SDL plugin 之前改用 Engine 的 JSON 介面；SDL 家族、ufbx 與 shader 工具在 adapters-relocate 一次只搬不改形狀進 `plugins/`，wrap-render 再在 plugins 裡改形狀（§5 取捨 T4）。
6. **計畫合計（非條件批）**：原計畫 632～1,299 h → 第一次修訂 681～1,400 h → **本修訂 700～1,448 h**（非條件 36 批，全為推定；其中規劃批 B1 8～16 h 與由條件改為非條件的 json-adapter 21～42 h 是新算入的；不含這兩批時 671～1,390 h，比第一次修訂少 10 h）。**超出 RC-11 的錨點 1,299 h**（上限多 149 h）；錨點不調（review 決定行）——這次複核就是對「計畫越修越大」的回應，觸發點 1,948.5 h 未到。關鍵路徑 332～694 h 不變。重算：主 checkout `build/evidence/adapter-plugins-plan/reviser2-sum.{py,txt}`。
7. **受影響的批**（計畫修訂記錄逐段列出）：既有 17 批（plugins-foundation、assets-core、match-sim、net-link、diagnostics-service、match-host、wrap-render、plugins-frame、client-link、client-frame、plugins-editor、工具四批、framework-delete、closeout）＋新增 3 批（adapters-relocate、json-adapter、規劃批 `v8-arch-engine-own-plugins-plan`＝B1）。
8. **未決**：17 題全部已定（§4 逐題引決定行）。Design Invariants 的增改（DI 2 增補、新 DI 5、DI 6、兩條 RC）使用者已同意，文字照 review 決定行改寫；落字時點在 plugins-foundation 的事前宣告之前，本修訂不改 target 的 DI 節（§4 未決 11）。

---

## 1. 模組歸類表（產出 1）

**判斷順序**（`v8-arch-engine-or-plugin-criteria`；讀法照 `v8-arch-adapter-plugins-review`）：**Q1** 沒有它，Engine 是否成立？不成立→留 Engine。可檢查的形式 **Q-b**：只用 Engine＋不選任何 plugin 的最小組成，能否 build、跑 Engine 的測試；拿掉它會不會壞。**中性介面算 Engine 的能力**（渲染的中性介面在 Engine、後端在 plugin）。成立時看 **Q2a** 有沒有對外的介面、**Q2b** 是固化的基礎還是會變、會被替換的規則——**「會被替換的規則」讀作「產品會在多個實作之間選的規則」**；無對外介面且固化→留 Engine，否則→plugin。

**Q2a 的讀法**（未決 4，已定讀法 1）：「對外介面」＝面向 Engine 以外的系統（第三方組件、OS 裝置、網路、外部行程）的介面；Engine 自己定給 plugin 實作、或給產品呼叫的中性 C++ 介面不算（collision、math 都有公開 C++ API 而判為「沒有對外介面」）。

**兩軸**（review 決定行的根因與改動 (1)～(3)、(6)）：**軸 2**（能力放哪）只由判斷順序決定。**軸 1**（怎麼碰外部組件＝`v8-arch-external-components-as-adapters` 的三原則）在哪一層都適用、不決定歸屬：Engine 的能力用到外部組件時，adapter 在 Engine 裡（一組件一 adapter、介面用 Engine 的詞）；歸 plugin 的能力，它的 adapter 在那個 plugin（選項 C 改窄）；產品獨有的能力屬於產品，產品連外部組件時經對接它的 plugin，沒有就在產品目錄下做（**R3**）。原則 (3) 是對 Engine 說的（未決 10 (iii)）：Engine 的標頭與 adapter 實作 Engine 介面的部分只用 Engine 的詞，adapter 可對依賴它的產品與 plugin 露出第三方型別。

**plugin 與 owner**：plugin 是組成形式，owner 是責任歸屬。「Engine 自有 plugin」（Engine 維護、隨 Engine 改版；規劃批 B1）與「`plugins/` 的擴展」（外部維護，例：網路）是兩種 owner。本表的「plugin」指後者；判為「留 Engine」的模組將來是否改成 Engine 自有 plugin、由 App 組裝，由 B1 定，不在本表。

**停止條件的檢查**：36 列照新讀法重判後，**0 列**與已定歸屬相反（collision、math 留；網路含 `engine/net` 是 plugin；SDL、SDL_image、SDL_ttf、ufbx 第 1 類；ImGui 第 2 類；model 取樣、audio core、ui、JSON 留 Engine；shader 建置期工具進 SDL plugin）。第一次修訂的「推論 R3」（承載第 1 類後端所實作之中性介面的模組留 Engine）退役：它被 Q-b 的附句「中性介面算 Engine 的能力」取代；**「R3」之後只指產品的規則**（review 決定行 (6)），本檔與計畫裡第一次修訂的「推論 R3」字樣已改掉。第一次修訂列為「照順序是 plugin、建議留 Engine、待使用者」的兩列（model core、audio core），照新 (b) 讀法是固化（產品不在多個取樣規則或 mixer 實作之間選），留 Engine，不是例外。

### 1.1 engine/ 的 23 個模組

| # | 模組 | Q1 | Q2a | Q2b | 歸屬 | 已定／建議 | 證據（檔＋符號） |
|---|---|---|---|---|---|---|---|
| 1 | asset（core） | 不成立：世界內容只經 Loading 的資產階段（runtime-composition DI 3）；`Assets<T>`、請求序號是內容身分 (c) 的契約（若判成立：Q2a 無——檔案經 `engine/io`／`engine/os`；Q2b 固化——內容身分 (c) 的契約；照順序仍留 Engine） | — | — | 留 Engine（照順序；另：`IAssetLoader` 是中性介面＝Engine 的能力） | 已定方向（未決 13，各遷移批確認） | `engine/asset/include/engine/asset/loading/IAssetLoader.hpp`；`engine/asset/src/ContentManifest.cpp` `ContentManifest::Parse`；計畫 §3.19 |
| 1a | └ asset 的 sdl_image loader | 成立 | 有（SDL_image） | 會被替換 | **第 1 類**：SDL_image plugin，宣告依賴 SDL plugin | 已定 (e) | `gyo_asset_sdl_image`（`engine/CMakeLists.txt`，`if(TARGET SDL3_image::SDL3_image)` 內）；`SdlImageTextureLoader.hpp` 位於 `GYO::Engine` 的公開 include 目錄 |
| 2 | audio（core：`Mixer`、`Submitter`、`ClipBank`） | 成立（Match 不用） | 無（輸出在後端） | 固化：整數混音與跨平台記錄雜湊是 DI（audio.md DI 8、9）；bus／effect 是 audio.md RC 預留的擴充點，但產品不在多個 mixer 實作之間選（(b) 的新讀法）；`Mixer`／`Submitter` 也是 SDL 音訊輸出所實作的中性介面 | **留 Engine** | **已定**（review 決定行 (3)；未決 3） | `engine/audio/include/engine/audio/Mixer.hpp` `Mixer`、`Submitter`；`tests/common/audio/CharacterizationTests.cpp`（跨平台錄製雜湊，label `cpu`）；`gyo_expect_audio_links` |
| 2a | └ audio/backend/sdl | 成立 | 有（SDL） | 會被替換 | **第 1 類**：SDL plugin 的音訊輸出部分 | 已定 (e) | `gyo_audio_sdl` `SdlAudioOutput::Open(Mixer&&, …)` |
| 3 | base | **不成立** | — | — | 留 Engine | 已定（準則明列 Base） | `gyo_base`（INTERFACE）；`engine/base/include/engine/base/StrictJson.hpp` `ParseStrictJson`（只以模板參數接 nlohmann；json-adapter 改成經 Engine 的 JSON 模組，未決 1 已定） |
| 4 | collision | 成立 | 無 | 固化 | 留 Engine | **已定** | 準則決定行；`gyo_collision` |
| 5 | config | 不適用（不是程式模組） | — | — | 不適用：`projects.csv`、`tools.csv` 是 Product Registration 資料，留原處 | 建議 | `engine/config/`；讀者 `build/cmake/GyoApps.cmake`、`GyoTools.cmake` |
| 6 | entity_store | **不成立**（World 欄的實作，Delta-3） | — | — | 留 Engine | 建議（由 Delta-3 推出） | `gyo_entity_store`；計畫 §4 Delta-3 |
| 7 | input（core：`PhysicalInputFrame`、`InputActionMap`） | 成立（Match 不用） | 無 | 固化：後端要填的 Engine 詞彙 | 留 Engine（中性介面＝Engine 的能力） | 已定方向（未決 13，各遷移批確認） | `engine/input/include/engine/input/PhysicalInputFrame.hpp`；`gyo_input` 只 PRIVATE 連 `GYO::Base` |
| 7a | └ input/backend/sdl（含 native） | 成立 | 有（SDL） | 會被替換 | **第 1 類**：SDL plugin 的輸入部分 | 已定 (e) | `gyo_input_sdl` `SdlInput`、`SdlInputInjector`；`gyo_input_sdl_native` `HandleNativeEvent` |
| 8 | io | 不成立：`IFileSystem`、`IStream` 是資產載入的詞彙，`Assets<T>` 依賴它；若成 plugin，Engine 的 asset 就要 include plugin（違 Delta-2 第 5 項） | 對 OS 有，但已經 `engine/os`；無第三方 | （若 Q1 判成立：固化——資產載入的詞彙；照順序仍留 Engine） | 留 Engine（介面與同步實作）；IO 執行緒服務已是 plugin（計畫 §3.11 asset-io） | 已定方向（未決 13，各遷移批確認） | 併入 `GYO::Engine`；`IFileSystem`、`IFileWatcher`、`IStream`；計畫 §3.19 `WriteFileAtomically` |
| 9 | kernel | **不成立** | — | — | 留 Engine | 已定（Delta-1） | `gyo_kernel`；target/kernel DI-KS 15 |
| 10 | map_markers | 成立 | 無（nlohmann 是 PRIVATE） | 固化：契約資料 `gyo.map_markers` 的解釋；literal-bit golden 是 (c) 守衛（determinism §4.6） | 留 Engine | 已定方向（未決 13，各遷移批確認） | `engine/map_markers/src/MapMarkers.cpp` 經 `Base::ParseStrictJson` |
| 11 | math | 成立 | 無 | 固化 | 留 Engine | **已定** | 準則決定行；`gyo_math` |
| 12 | model（core：`ModelAsset`、`AnimationEvents`、取樣） | 成立 | 無 | 固化：`ModelAsset`／`AnimationEvents` 是契約詞彙（ufbx adapter 輸出的中性型別）；取樣規則是 Engine 定的標準（E1 改的是這個標準；determinism §4.2，今天 (b)、同平台 golden），產品不在多個取樣規則之間選（(b) 的新讀法） | **留 Engine** | **已定**（review 決定行 (3)；未決 3） | `engine/model/include/engine/model/ModelAsset.hpp`；`engine/model/src/AnimationEvents.cpp`；`gyo_expect_model_links` |
| 12a | └ model/backend/ufbx | 成立 | 有（ufbx） | 會被替換 | **第 1 類**：ufbx plugin | 已定 (e) | `gyo_asset_ufbx` `UfbxModelLoader`（PRIVATE 連 `gyo_ufbx`）；消費者 `apps/object_fps_pvp/src/Sim/PlayerAnimationContent.cpp`（Match）與 `src/Pvp/PvpApplication.cpp`（Client）都直接 `loaders.Register(std::make_unique<…UfbxModelLoader>())` |
| 13 | net | 成立（Engine 的其他模組都不用它；使用者是 object_fps_pvp 與它的測試、acceptance） | 有（wire 的資料框；與 Go gateway 共用的跨語言契約） | — | **第 2 類**：網路 plugin，與 asio／httplib／protobuf 同一個 plugin | **已定 (f)**；owner＝網路 plugin 的 owner，也是 `services/gyo_gateway` 的 owner（review (7)，O1） | `engine/net/include/engine/net/GyopDatagram.hpp` `EncodeGyopDatagram`、`DecodeGyopDatagram`；`gyo_net` 只連 `GYO::Base`；消費者：`apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp`、apps CMake 的 `GYO::Net` 2 處、`tests/object_fps_pvp/CMakeLists.txt` 的 `GYO::Net`、`build/acceptance/object_fps_pvp/worker_main.cpp`（`EncodeGyopDatagram`）、`tests/common/net`；向量檔 `tests/common/net/fixtures/gyop_datagram_vectors.json` 另被 `services/gyo_gateway/framing/vectors_test.go` 讀（GYOP 與 Go gateway framing 逐位元組共用，`GyopDatagram.hpp` 檔頭；契約與向量檔放在兩個實作之外——未決 14 G-b，已定） |
| 14 | os | **不成立** | — | — | 留 Engine | 已定（準則明列） | os.md DI 1～3 |
| 15 | platform（今天只有 `sdl/`；本身就是後端） | 成立（Kernel 不需要視窗） | 有（視窗、事件） | 會被替換 | **第 1 類**：SDL plugin 的核心；**Engine 要新立中性的 platform 契約**（今天不存在，§2） | 已定 (e)；中性契約＝Engine 的能力（Q-b 附句） | `engine/platform/CMakeLists.txt` 只有 `add_subdirectory(sdl)`；`SdlPlatform`、`PlatformEvent`、`PathDialogRequest`、`SdlLogBridge`、`GyoMain`（`gyo_platform_sdl_main`） |
| 16 | render（core：`IRenderDevice`、`Renderer`、`ShaderLibrary`、`gyo_model_renderer`） | 成立（headless Match 不畫） | 無（GPU 在後端） | 固化：`IRenderDevice` 是 Engine 的詞彙；渲染 World 的組合規則在 kernel DI-KS 13 | 留 Engine（中性介面＝Engine 的能力） | 已定方向（未決 13，各遷移批確認） | `engine/render/include/engine/render/IRenderDevice.hpp`；`engine/render/src/ShaderLibrary.cpp` |
| 16a | └ render/backend/sdl（2D，含 native） | 成立 | 有（SDL_Renderer） | 會被替換 | **第 1 類**：SDL plugin 的 2D 部分；**plugins-editor 退役**（未決 9 I1，已定） | 已定 (e) | `gyo_render_backend_sdl` `SdlRenderer`（未實作 `IRenderDevice`）；`SdlRendererNative.hpp` |
| 16b | └ render/backend/sdl_gpu | 成立 | 有（SDL_GPU） | 會被替換 | **第 1 類**：SDL plugin 的 GPU 部分 | 已定 (e) | `gyo_render_backend_sdl_gpu` `SdlGpuRenderDevice final : public IRenderDevice` |
| 16c | └ render/shaders/pipeline（建置期工具，組件見 34～36） | 不進執行檔 | — | — | **SDL plugin 的 GPU 部分**：shader 編譯工具、內建 shader 的編譯步驟、`ShaderLibrary` 挑 SPIR-V／DXIL／MSL 的那段；Engine 留 HLSL 原始碼與 shader ABI；工具的 JSON 改用 Engine 的 JSON 介面（不在 plugin 另連函式庫） | **已定**（未決 2 B；JSON 句：未決 1） | `gyo_shader_tool`；`build/cmake/GyoShaders.cmake` `_gyo_require_shader_tool`；`pipeline/src/main.cpp`（今天含 nlohmann） |
| 17 | runtime（`RuntimeLoop`、`IRuntimeClient`；`FixedTickRuntime`） | `RuntimeLoop`／`IRuntimeClient`：成立（Kernel 取代它）；`FixedTickRuntime`：成立（Kernel 的 Scheduler 推進 tick） | `FixedTickRuntime`：無第三方；時鐘經 `engine/time` | `FixedTickRuntime`：會變的規則（runner 的時鐘策略，計畫 §3.21 client-sim 的「runner 的時鐘策略」）；`RuntimeLoop`：被 Kernel 取代 | `RuntimeLoop`／`IRuntimeClient`：刪除（runtime-loop-retire，計畫 §3.31）；`FixedTickRuntime`：照順序 plugin——進 runner plugin（計畫 §3.31 已排） | 已排程 | `engine/runtime` 併入 `GYO::Engine` |
| 18 | text（core：`ITextRasterizer`） | 成立 | 無 | 固化（Engine 詞彙） | 留 Engine（中性介面＝Engine 的能力） | 已定方向（未決 13，各遷移批確認） | `engine/text/include/engine/text/ITextRasterizer.hpp` |
| 18a | └ text/backend/sdl_ttf | 成立 | 有（SDL_ttf） | 會被替換 | **第 1 類**：SDL_ttf plugin，宣告依賴 SDL plugin | 已定 (e) | `gyo_text_backend_sdl_ttf` `SdlTtfTextRasterizer::Create` |
| 19 | threads | **不成立** | — | — | 留 Engine | 已定（準則明列） | `gyo_threads` |
| 20 | time | **不成立** | — | — | 留 Engine | 已定（準則明列） | `gyo_time` |
| 21 | trace（core：sink 契約） | 成立 | 無：寫檔經 `engine/os`，trace 自己不面向外部系統 | 固化：sink 的保存律、順序、無輪詢喚醒（trace.md DI 1～5）；**記錄的格式與語義不在 Engine**（DI 6：Engine 不解析、不改寫、不知道一行的意義——統合稿寫「記錄格式會變」，與 DI 6 不符，已改正） | 留 Engine（照順序；core）；寫檔服務已是 plugin（計畫 §3.13 trace-diagnostics，`engine/trace` 不改） | 已定方向（未決 13，各遷移批確認） | `gyo_trace`（PUBLIC `GYO::Threads`、PRIVATE `GYO::Os`）；trace.md DI 6、7 |
| 22 | ui（文件模型、`gyo.ui` codec、`UiRenderer`） | 成立 | 無（JSON 經 Engine 的 JSON 模組，未決 1） | 固化：`gyo.ui` 是有版本的資料契約、Engine 是 owner，Client 與 ui_editor 預覽必須同一份解釋；產品不在多個 UI 實作之間選（(b) 的新讀法；GYO 自己的 UI API 是未來目標）；`UiRenderer` 在 wrap-assets 重寫的是形狀，不是規則 | **留 Engine** | **已定**（未決 13：ui 算 Engine，review） | `engine/ui/src/UiDocumentCodec.cpp`、`UiSerialization.cpp`；`gyo_ui_renderer` |
| 23 | voxel_mesh | 成立 | 無 | 固化：`gyo.voxel_mesh` 等契約；決定碰撞世界與 `ArenaContentDigest`；literal-bit golden (c)（determinism §4.6） | 留 Engine | 已定方向（未決 13，各遷移批確認） | `engine/voxel_mesh/src/VoxelMesh.cpp` 經 `Base::ParseStrictJson` |

### 1.2 third_party 的 10 個與建置期 3 個

| # | 組件 | 歸屬 | 已定／建議 | 現況的 adapter 位置與要處理的事（檔＋符號） |
|---|---|---|---|---|
| 24 | asio | **第 2 類**：網路 plugin 的內部實作 | 已定 (f) | 今天在產品：`apps/object_fps_pvp/src/Pvp/IpcHost.cpp`、`ClientConnection.cpp`；測試：`tests/object_fps_pvp/ConnectionLivenessTests.cpp`、`IpcHostTests.cpp`；acceptance：`build/acceptance/object_fps_pvp/worker_main.cpp`；`third_party/asio` 的 `add_subdirectory` 寫在 `apps/object_fps_pvp/CMakeLists.txt`；CMake 連結者 apps 2 行、tests 2 行、acceptance 2 行 |
| 25 | doctest | 不適用（只在測試：tests 154 檔、spikes 2 檔；不進 Engine、plugin、產品執行檔） | 已定（未決 6：測試框架與夾具不受原則 (1) 限制） | `doctest::doctest` 的連結者只有 tests 與 spikes |
| 26 | httplib | **第 2 類**：網路 plugin 的內部實作 | 已定 (f) | 今天在產品：`ClientConnection.cpp`；acceptance：`network_main.cpp`、`worker_main.cpp`；CMake 連結者 apps 1 行、acceptance 3 行（與 asio 同行者合計 7 行） |
| 27 | imgui | **第 2 類**：imgui plugin；ImGui 官方的 SDL3 平台後端與 SDL_GPU 繪製後端放進 **SDL plugin 內的獨立 target（例 `sdl_imgui`）**，只有選了 imgui 的產品連它（取代 `imgui::sdl3_sdlrenderer3`） | 已定（RC (ii) 選 (ii)；工具可直接呼叫 ImGui：未決 7 取消限制） | `framework/bindings/imgui` `ImGuiShell.cpp`；工具 9 檔直接含 `<imgui.h>`（audio_editor 1、model_viewer 1、ui_editor 6、voxel_map_editor 1）；`imgui::sdl3_sdlrenderer3` PUBLIC 連 SDL3（ImGui 的 SDL glue＝第二處 SDL 呼叫，由 `sdl_imgui` 收進 SDL plugin） |
| 28 | nlohmann_json → **yyjson** | **Engine 的能力**（契約資料解析；Q-b：Engine 讀自己的資料契約，拿掉會壞）：一個 Engine 的 JSON 模組是唯一連 JSON 函式庫的地方（函式庫＝yyjson），其他 6 模組與 shader 工具改用它；介面是 GYO 自己的、用 Engine 的詞（使用者拿不到函式庫的 DOM 型別）；數字走原文路（adapter 交原文、Engine 以 `std::from_chars` 轉） | **已定**（未決 1，使用者原話見 §4） | 今天 nlohmann：engine 6 模組 9 個 `.cpp`（asset 2、render 2——其一是建置期 `pipeline/src/main.cpp`——、ui 2、map_markers、voxel_mesh、model；全部 PRIVATE；公開標頭 0 件）＋第 7 個模組 base 的 `StrictJson.hpp`（API 形狀依賴）；apps 7、tools 17、framework 2、tests 20、acceptance 6 檔（產品自己的事：R3、(α)） |
| 29 | protobuf | **第 2 類**：網路 plugin 擁有執行期函式庫的加入、版本與 protoc 規則；產品的 generated code 是產品經網路 plugin 用它（R3、(iii)），不設列名 | 已定 (f)＋未決 8（A；列名機制刪，review (5)） | `IpcHost.cpp` 含 `google/protobuf/unknown_field_set.h`；`runtime_v9`／`client_v9` 的 `.pb.cc` 由 `protobuf::protoc` 在產品建置期產生 |
| 30 | sdl3 | **第 1 類**：SDL plugin（一個 adapter；內含核心、視窗、輸入、2D、GPU、音訊輸出等後端 target——P2，未決 5 已定；另含 `sdl_imgui` 與 shader 建置期工具） | 已定 (e) | 今天分散在 `engine/{platform,input,render,audio}` 的 9 個 target（含 3 個 `*Native` 與 OBJECT 的 `gyo_platform_sdl_main`／`GYO::PlatformSDLMain`）；engine 外直接含入：`tools/ui_editor/src/AssetPreviewContext.cpp`、tests/common 10 檔、`tests/ui_editor/PreviewAssetTests.cpp`（另用 `ImGui_ImplSDLRenderer3_*`） |
| 31 | sdl3_image | **第 1 類**：SDL_image plugin，宣告依賴 SDL plugin | 已定 (e) | 只有 `engine/CMakeLists.txt` 連它 |
| 32 | sdl3_ttf | **第 1 類**：SDL_ttf plugin，宣告依賴 SDL plugin | 已定 (e) | 只有 `engine/text` 與 `engine/text/backend/sdl_ttf` 連它 |
| 33 | ufbx | **第 1 類**：ufbx plugin | 已定 (e) | 只有 `engine/model` 連它；Match 經 `player_content` 的 PRIVATE_LIBRARIES 直接連 `GYO::AssetUfbx`（繞過 component 名） |
| 34 | SDL_shadercross（建置期） | SDL plugin 的 GPU 部分（隨 shader 工具；先不換） | **已定**（未決 2 B） | `Dependencies.cmake` 的 `gyo_shadercross`；`gyo_host_shadercross` |
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
| render/backend/sdl（2D） | 是 | **無**；`SdlRenderer` 不實作 `IRenderDevice` | 使用者只有 ImGui 的 SDL 繪製（`framework/bindings/imgui` 經 `RenderBackendSDLNative`）與 `framework/include/gyo/framework/PreviewTexture.hpp` `UploadFrame`（readback 上傳，隨 readback 退役）；工具 4 檔、tests 7 檔。**不設計 2D 中性介面**：I1 已定（未決 9），plugins-editor 退役 | plugins-editor | 1～2 |
| render/backend/sdl_gpu | 是 | `IRenderDevice` ✓（對 Client 與 GPU 預覽；對 ImGui draw list 沒有裁切矩形與索引子範圍的繪製——`engine/render/include` 只命中 `RenderDeviceTypes.hpp` 一句「non-indexed」註解與 handle 的 `index`；**第二次修訂**：ImGui 改用官方 SDL_GPU 後端（`sdl_imgui`，在 SDL plugin 內直接用 SDL_GPU）後，擴充是否還需要由 plugins-editor 的宣告確認） | 兩個**靜態工廠** `SdlGpuRenderDevice::Create(SdlPlatform&, …)`、`CreateOffscreen(SdlPlatform&, …)`（唯一的建構子是 private 的 `explicit SdlGpuRenderDevice(std::unique_ptr<Impl>)`）→改在 SDL plugin 內呼叫、以 `IRenderDevice` 交出；`SetFrameSize` 是**離屏專用**（非離屏回 `UnsupportedOperation`），測試以外唯一的呼叫者是 `framework/src/Preview.cpp`——與 `ReadFrame` 同一條要退役的 readback 路徑，隨 plugins-editor 的 offscreen-preview（無 readback）處置，**不進 `IRenderDevice`**（統合稿的「`SetFrameSize` 進中性介面或交 runner」刪）；`SaveCaptureBmp`／`SceneCaptureFile` 在介面外（擷取由 wrap-render 宣告定去向） | wrap-render（介面擴充若需要在 plugins-editor） | 1～4（擴充若需要另 0～4，計在 plugins-editor） |
| text/backend/sdl_ttf | 是 | `ITextRasterizer` ✓ | 只改建立方式：`Create()`→plugin 的 build 函式登記 | 搬家：adapters-relocate；登記：wrap-render | 0～1 |
| audio/backend/sdl | 是（命名空間 `Engine::Audio::Sdl`，其他後端是 `…::Backend::…`） | 沒有輸出介面類；中性的是被拉取的 `Mixer`、`Submitter`；輸出的設定、錨點、統計沒有中性型別 | 不另設輸出介面類：SDL plugin 的 build 函式開裝置、拉 `Mixer`。但產品與工具今天用到的不只 `Mixer`／`Submitter`（對抗檢查 B-3 (a)）：`apps/object_fps_pvp/src/Present/PvpSound.cpp` 用 `SdlAudioOutputConfig`、`std::vector<OutputAnchor>`（經 `TakeAnchors`）與 `SdlAudioOutput::Open`；`tools/audio_editor/src/AudioEditorWindow.cpp`、`tools/model_viewer/src/PreviewSounds.cpp` 直接持有 `SdlAudioOutput`；公開面另有 `Stats()`（`SdlAudioOutputStats`）、`DriverName()`、`DeviceSampleFrames()`——這些型別由 SDL plugin 定義；**第二次修訂**：照 (iii) 與 (α)，產品與工具用 SDL plugin 露出的型別是允許的、升級時產品自己負責，所以「成為 Engine 的詞」不再是要求——成為中性型別、或留為 SDL plugin 對依賴者露出的型別，由 wrap-render 的宣告定；命名空間在搬家時統一 | 形狀（與是否立中性型別）：wrap-render；搬家：adapters-relocate | 2～5（中性型別 0～2 計在 wrap-render 的增量） |
| asset sdl_image | 是；**但檔案在 `GYO::Engine` 的公開 include 目錄**，target 定義在 `engine/CMakeLists.txt` | `IAssetLoader` ✓ | 標頭與 target 搬出 Engine；plugin 的 build 函式呼叫 assets-core 的登記 API | adapters-relocate | 1～3 |
| model/backend/ufbx | 是 | `IAssetLoader` ✓ | 消費者直接建構 `UfbxModelLoader`（apps 2、tools 2、tests 7）→改由 ufbx plugin 的 build 函式登記；**取代計畫的 pvp 最小 ufbx plugin（S3-3）與 model-assets plugin**——兩者都會成為 ufbx 的第二個 adapter（原則 (2)） | 登記：assets-core（`plugins/ufbx/`，未決 16 (a) 已定）；搬家：adapters-relocate | 登記 1～2；搬家 1～2 |
| net | 是（整個模組不連任何外部組件） | 本身就是中性的 GYOP 編解碼 | 整個模組搬進網路 plugin（決定 (f)，不是因為不乾淨）；改全部消費者：`Wire.hpp` 的含入、apps CMake 的 `GYO::Net` 2 處、`tests/object_fps_pvp/CMakeLists.txt` 的 `GYO::Net`、`build/acceptance/object_fps_pvp/worker_main.cpp` 的含入與連結；`tests/common/net` 跟著搬（只搬不改，§14.4）——但它的向量檔 `fixtures/gyop_datagram_vectors.json` 也被 `services/gyo_gateway/framing/vectors_test.go` 以相對路徑讀（C++ 與 Go 共用），**只搬 C++ 那一半會弄斷 Go 的契約測試**；向量檔放在兩個實作之外（未決 14 G-b，已定），GYOP 的 owner＝網路 plugin 的 owner、也是 `services/gyo_gateway` 的 owner（review (7)）；判定加「Go 測試照跑」 | net-link | 3～6 |

**`*Native` target 的去處**：`GYO::PlatformSDLNative`、`GYO::InputBackendSDLNative`、`GYO::RenderBackendSDLNative` 在 engine 外的消費者是 `framework/bindings/imgui`、`tools/ui_editor`（`EditorWindow.cpp`）、tests。target §3b 的 ImGui 兩半（輸入是文件 World 的 System、讀 Engine 的輸入；draw list 由渲染 World 繪製）做成之後，ImGui 不再需要 `SDL_Window*`／`SDL_Renderer*`；`AssetPreviewContext.cpp` 的直接 SDL 含入在 tool-ui 去掉。**第二次修訂**：照 (iii)，`*Native` 是 SDL plugin 對依賴它的產品與 plugin 露出的部分，**不設使用者白名單**（review：G04／N5 刪）；ImGui 的 SDL 後端收進 SDL plugin 內的 `sdl_imgui` 後，SDL plugin 外不再有 SDL 呼叫（RC (ii)：整個 repo 的 SDL 呼叫只在 sdl plugin）。SDL 家族內部互傳 `SDL_Window*` 是 SDL plugin 內部的事（P2）。

**消費者寫具體類名**（**第二次修訂**：照 (α)，產品與工具寫 SDL plugin 的具體類名不再是要歸零的判定；下段的「歸零」改讀為「重寫時自然消失的部分照實記錄」）：三案一致，**不在中性介面立好時就改這些檔**——apps 與 tools 的這些檔在 client-frame 與工具四批會整個重寫（main＝Run），framework 會被刪；提前改＝在即將消失的程式上製造衝突（RC-7）。adapters-relocate 只改它們的含入路徑與 target 名（機械修改），類名不動；歸零在 client-frame（apps）、工具四批（tools）、framework-delete（framework）。tests 隨 SDL plugin 搬成 plugin 自己的測試。

**Engine 外要改的其他地方**（歸批）：`AssetPreviewContext.cpp` 的 `<SDL3/SDL.h>`→tool-ui；`ImGuiShell.cpp`、`EditorWindow.cpp` 的 Native 使用→plugins-editor／tool-ui；apps 的 asio／httplib→match-host、client-link（隨 IO 改經 net-link 服務自然移出；不是判定，R3）；`player_content` 的 `GYO::AssetUfbx` 直連→match-sim；`GyoRuntimeDeployment.cmake` `gyo_install_product_target` 與 `GyoPackaging.cmake` 兩處以 `component MATCHES "^SDL_" OR component STREQUAL IMGUI` 推定要裝 SDL 的共享庫（AGENTS §9「命名規則だけを利用して Product の意味を推測しない」）→plugins-foundation 段 (i) 改讀登記資料的 `runtime_dependencies`；後端選擇的第二個入口 `GyoRenderOptions.cmake` 的 `GYO_RENDER_DEVICE`／`GYO_BUILD_SDL_GPU_BACKEND`／`GYO_REQUIRE_SDL_GPU`→段 (i) 定處置（計畫 §3.11）；產品 SDL link guard（`tests/object_fps_pvp/CMakeLists.txt` 的 `entry MATCHES "^SDL3(_image)?(::|-)"`）**漏了 `SDL3_ttf`**（風險提案查到、本統合重驗）→adapters-relocate 補（締める面）。

**合計**：新立中性契約 1 個（platform）、中性注入名 0～1 個、audio 輸出的中性型別（設定、錨點、統計）1 組、sdl_image 標頭移出 Engine 公開 include 目錄 1 處、`*Native` 收窄 3 個；`IRenderDevice` 擴充 0～1 處（裁切、索引子範圍；是否需要由 plugins-editor 確認）。統合稿的「`IRenderDevice` 補尺寸 1 處」刪（`SetFrameSize` 隨 readback 退役，沒有留下來的消費者）。工時分散在 wrap-render（+5～10；扣掉移到 adapters-relocate 的 `if(TARGET)` 硬性化 1～2，淨 +4～8，見計畫 §3.18 修訂）、adapters-relocate、plugins-editor（擴充若需要 0～4）。


### platform-sdl.md 整份的去向（對抗檢查 S-6；HANDOFF 決定行末句「`platform-sdl.md` 不加標註，隨這題整份重看」）

`engine/platform/sdl` 整個搬走後，`docs/architecture/platform-sdl.md` 的條文分兩處落：**SDL 特有的**落 `docs/plugins/sdl`（adapters-relocate 寫入，同批在 platform-sdl.md 加 Legacy 行）；**任何 platform 後端都要守的**成為中性 platform 契約的條款，落 `target/platform`（wrap-render 開檔並更新 Legacy 行的指向）。突變隨條文走（§3 共通「守衛與突變的去處」）。下表是建議，各批事前宣告定案；DI 條文的落點改變不改性質，搬時照 §14.4 檢查有無緩める面。

| 條 | 內容（摘要） | 落點 | 寫入的批 | 守衛與突變 |
|---|---|---|---|---|
| DI 1 | SDL 後端的非 native 公開標頭不帶 SDL 標頭 | `docs/plugins/sdl`（照 (iii)：SDL plugin 可對依賴者露 SDL 型別、實作 Engine 介面的部分只用 Engine 的詞；未決 10 已定） | adapters-relocate | G1／G2 對搬走後端退役、plugin 版標頭衛生守衛新增（計畫 §5.2 第 9 列） |
| DI 2 | SDL 型別只經 `*Native` target | `docs/plugins/sdl`（改寫成 (iii) 的形） | adapters-relocate | G3 改寫；補5 `sdl_symbol_guard.py`（N5 刪，review） |
| DI 3 | 平台交出的時間都在 Engine 時基、滿足時間戳契約 | **`target/platform`**（中性契約的條款：任何後端的事件時間戳都在 Engine 時基）；SDL 的換算細節留 `docs/plugins/sdl` | wrap-render（條款）、adapters-relocate（細節） | m1～m4、m-zero、m20～m22、m-watch-* 隨 SDL plugin 的測試搬（改寫：路徑），中性契約的合成實作另加一條時基測試（新增） |
| DI 4 | 測試注入只經事件佇列或真實視窗操作 | `docs/plugins/sdl`（注入的中性名依 wrap-render 的二選一；選中性名時條款進 `target/platform`） | adapters-relocate／wrap-render | m14、m15 隨搬 |
| DI 5 | log 橋接只在有人持有時生效 | `target/platform`（log 橋接是中性契約的入口之一）＋SDL 細節在 `docs/plugins/sdl` | wrap-render | m10、m11 隨搬 |
| DI 6 | 視窗、顯示、文字輸入狀態每次從 SDL／OS 讀，不留判斷用的副本 | `target/platform`（State Authority：platform 狀態的權威在 OS／後端） | wrap-render | 審查（今天無機械守衛） |
| DI 7 | observer 與 live frame 內只做查詢、不做操作 | `target/platform`（runner 的 live frame 契約；window-runner 經中性契約 pump） | wrap-render（plugins-frame 的 runner 依它） | m-main-thread、m-nest 隨搬；計畫 §3.18 的停止條件「包需要改 DI 7→停」不變 |
| DI 8 | Engine 不呼叫全域 `SDL_Init`／`SDL_Quit`，子系統由 owner 參考計數 | `docs/plugins/sdl`（改寫成「SDL plugin 不呼叫全域…」；SDL_image、SDL_ttf 依賴 `base` 時照這條） | adapters-relocate | 隨搬 |
| DI 9 | 後端不帶擷取政策、文字編輯政策與產品知識 | `target/platform`＋plugins 層 DI（`docs/plugins/README`：無產品名） | wrap-render／plugins-foundation | 審查＋層 DI 的守衛 |
| DI 10 | 一個請求在 OS 回一次時得到一個回覆 | `target/platform`（路徑對話框的請求／回覆搬進中性契約） | wrap-render | `PathDialogTests.cpp`、pd-* 突變隨搬（改寫：路徑與型別名） |
| RC 1 | 需要第二個非 SDL 後端時提出中性 platform 介面 | **由本修訂提前滿足**：使用者的 `v8-arch-external-components-as-adapters`（Feature 1 的「替換」、方向 C）是 AGENTS §2 的予見の根拠 1（名指しされた Feature），所以中性契約現在就立（wrap-render），不等第二個後端；這條 RC 在 `docs/plugins/sdl` 改寫成「中性契約不足以讓 SDL plugin 實作→回 Engine 補契約，不在 plugin 內繞」 | adapters-relocate | — |
| RC 2 | ImGui binding 與 ui_editor 預覽以外不得用 Native | `docs/plugins/sdl`，改寫成 (iii) 的形（依賴 SDL plugin 的產品與 plugin 可用；不設白名單——review 刪 G04） | adapters-relocate | — |
| RC 3～9 | `PlatformEvent` 欄位外露、時間戳與 log 橋接的重驗、pump 離開主執行緒、非同步 OS 回覆、對話框回呼 | 與對應 DI 同落點（3、4、5、6、7、8 隨 `target/platform`；9 隨 `docs/plugins/sdl`） | wrap-render／adapters-relocate | — |

統合稿「流程事項」把「SDL_Init 的分工」列為未查證；DI 8 已有答案，已改引。

---

## 3. 選擇欄的形狀（產出 3；給 plugins-foundation 的事前宣告當輸入，欄名由該批定）

第二次修訂照 review 決定行改寫：刪 `backs`（S03）、能力由推導得（S08 改為段 (i) 先驗）、自動補依賴（S04 反轉）、configure 錯誤只留三種（刪 `role-mismatch`、`unused-backend` 等）、過渡登記表留表刪棘輪；預留「Engine 自有 plugin」的形狀（B1）。第一次修訂的形狀（S-A 與 9 種錯誤）見 git 歷史（`b647de39`）。

### 3.1 單位：每個可執行單位一份選擇

- app 新增 `roles.<role>`，名稱與 `gyo_app_add_executable(<role> …)` 相同（object_fps_pvp：`main`、`match`）；tool 沿用 `variants.<v>`，`role_components` 改名 `role_plugins`（`build/cmake/GyoToolRegistry.cmake` 讀它；四個工具都沒寫）。同一個讀取函式處理兩種形狀，只差路徑前綴。
- 理由：Feature 2 的例子「headless 的 Match 不連視窗與 GPU」今天靠產品 CMake 達成——`apps/object_fps_pvp/project.json` 的 `components` 是全產品聯集，Match 與 Client 的差別寫在 `apps/object_fps_pvp/CMakeLists.txt`（同一事實兩處）。頂層一個清單表達不了 Feature 2。
- role 名：哪些 role 存在由 `roles` 的鍵宣告，CMake 以名字引用；引用不存在的 role 時讀取函式本來就會報錯，**不另設錯誤代號、不做兩向檢查**（review：S02 刪）。

### 3.2 欄的形狀：只寫 plugin 名

```json
{ "version": 2,
  "roles": {
    "main":  { "plugins": ["sdl", "sdl_image", "sdl_ttf", "ufbx", "window_runner", "net_link",
                           "asset_catalog", "event_sounds", "trace_diagnostics", "fps_pvp_client"] },
    "match": { "plugins": ["ufbx", "headless_runner", "net_link", "trace_diagnostics", "fps_pvp_sim"] } } }
```

- 名稱都是示意；欄名候選 **`plugins`**（runtime-composition §4、DI 2、Delta-5 都已用這個詞），由段 (i) 定。
- **不寫 `backs`**（review：S03 刪）。「這個單位用到哪些需要後端的能力」：**段 (i) 的事前宣告先驗以 CMake 連結圖推導**——Engine 的能力模組在自己的 CMake 宣告「需要一個後端」，一個單位的連結閉包（產品自己的程式＋所選 plugin）含那個模組的 target，就是用到那個能力；後端＝所選 plugin 中提供該能力的那一個。**推導不可行時回宣告式**：各 plugin 在登記資料寫 `requires_capabilities`（S08 留），產品自己直接用到的能力由產品自己的 plugin 宣告。兩種情況產品的選擇欄都只寫名字。先驗的內容與成本由段 (i) 宣告寫明（configure 階段讀連結閉包要處理 generator expression 與 `INTERFACE` 傳遞——未查證）。
- **只連用到的能力 target**：Feature 2 在同一個 SDL plugin 內部也成立（只用音訊輸出的單位不連視窗後端），原則 (2)「一個 adapter」與 Feature 2 不衝突（P2，未決 5）。
- 初始化順序由 plugin 登記資料的 `requires` 做確定性的拓撲排序（同層以名稱排）；清單順序不帶語義。
- **自動補依賴**（review：S04 反轉）：選了 `sdl_image` 就帶上 `sdl`；不補不會讓任何東西壞，只會多一個產品步驟。今天 `GyoComponents.cmake` 的推導（SDL_GPU 推出 SDL_PLATFORM）是同一性質，行為保留；`tests/common/cmake/ComponentTests.cmake` 的斷言隨名稱改寫（判定不放寬，§14.4 的四選一由段 (i) 宣告列）。

### 3.3 plugin 登記資料（`plugins/<name>/plugin.json`；產品自己的 plugin 放產品目錄，同格式）

```json
{ "version": 1, "name": "sdl_image", "class": "backend",
  "base": "GYO::SdlImage",
  "provides": { "texture_decode": "GYO::SdlImage" },
  "requires": ["sdl"],
  "runtime_dependencies": ["SDL3", "SDL3_image"] }
```

- `class`：`backend`（第 1 類）／`extension`（第 2 類）——決定 (d)：類別寫在登記資料，目錄不分。
- **預留的形狀（B1）**：登記格式留一個欄位（或值域）區分「Engine 自有 plugin」與「`plugins/` 的擴展」兩種 owner；本計畫只定形狀、不實作、不讀它，語義由規劃批 `v8-arch-engine-own-plugins-plan` 定。
- `provides`：能力→target。第 1 類的附加條「實作 Engine 定的後端介面，不新增能力」的機械形是讀取端的資料驗證（只有 `backend` 可寫、值必須是 Engine 宣告過的能力），屬 plugin 作者側、實作細節，不是選擇欄的錯誤代號。
- `requires`：plugin 之間的依賴宣告（決定 (e)：SDL_image、SDL_ttf 都寫 `["sdl"]`）。依賴是 plugin 層級，連結是 target 層級：`sdl_image` 依賴 SDL plugin 的 `base`，不連視窗後端。
- `requires_capabilities`：只在推導不可行、回宣告式時出現（§3.2）。
- `runtime_dependencies`：打包用；取代 `GyoRuntimeDeployment.cmake`、`GyoPackaging.cmake` 以 `SDL_` 前綴推定（§2 末段；AGENTS §9 的命名推測）。
- **Engine 的能力宣告**寫在各能力模組自己的 CMake（例：`gyo_engine_capability(gpu_device INTERFACE GYO::Render NEEDS_BACKEND)` 在 `engine/render/CMakeLists.txt`）。不放在 Kernel：target/kernel DI-KS 15 規定 Kernel 的程式碼與 Registry 不含能力的名字。
- imgui plugin 需要一個平台與繪製後端（SDL plugin 的 `sdl_imgui` 提供）：「選了 imgui 就要有人提供它」沿用 DI 6 的檢查（RC (ii) 的決定）；這個需求由誰宣告、能力名叫什麼，由 plugins-editor 的宣告定。

### 3.4 configure 錯誤（三種）

| 錯誤 | 條件 |
|---|---|
| 缺後端 | 單位用到的能力，所選 plugin 中沒有一個提供 |
| 多後端 | 單位用到的能力，所選 plugin 中有兩個以上提供 |
| plugin 不存在 | 選擇欄或 `requires` 指名的 plugin 沒有登記（含過渡登記表） |

- 決定 (b)：「必備」相對於該單位用到的能力——headless 的 Match 不用視窗與 GPU，就不報錯、也不連。
- 訊息要指名能力與 plugin；代號與訊息格式是實作細節（review：S11、S12 降級），由段 (i) 定。
- 刪除的（review 決定行）：`role-mismatch`（S02）、`unused-backend`（S10）、`not-provided`（`backs` 已刪）、`missing-dependency`（自動補依賴後只剩「不存在」）、`dependency-cycle` 與 `invalid-backend`（實作細節：登記資料的驗證，產品碰不到）。
- 選擇欄的契約測試：三種各一個合成夾具反例（§5.2 補遺二 N2）。

### 3.5 版本與過渡

- **只升一次版**：plugins-foundation 段 (i) 一次做「v1→v2、`plugins` 進來、`components` 刪除」，六份 `project.json` 同批升；讀取端只認 v2（計畫 §3.11 判定 (6) 已定不做兩版相容）。
- **過渡登記表**（review：留表、刪棘輪）：共通 build 資料裡一份表（位置由段 (i) 定），列「plugin 名已定、實作還不是 `plugins/<name>/` 的 plugin」——還在 Engine 的後端（sdl、sdl_image、sdl_ttf）、`FRAMEWORK`（實作在 `framework/`）、`IMGUI`（實作在 `framework/bindings/imgui`）；ufbx 在 assets-core 就移成自己的 `plugin.json`（未決 16 (a)）。每列寫 plugin 名、`class`、`provides`、`base`、`runtime_dependencies`、指向今天的 target、退出的批（sdl 家族＝adapters-relocate；`IMGUI`＝plugins-editor；`FRAMEWORK`＝framework-delete）；回宣告式時另帶 `requires_capabilities`（手寫的近似，第一次修訂對抗 S-2 的限界；推導可行時不需要）。
  - 產品從段 (i) 起就寫最終的 plugin 名；某 adapter 搬家時，把那一列移成它自己的 `plugin.json`，產品資料一行都不改。
  - **不設「只減不增」的常駐守衛**；closeout 判一次「0 列」。JSON 不在表裡（JSON 是 Engine 的能力、adapter 在 Engine 內）。
  - 不採「先建一個只有登記檔、沒有程式的 `plugins/<name>/`」：會出現名為 SDL plugin、卻什麼都不擁有的目錄（§5 取捨 T6）。
- 一併被取代（段 (i)）：`GyoAppProject.cmake` `gyo_app_link_components` 的名稱→target 表、`GyoComponents.cmake` 的 `GYO_KNOWN_COMPONENTS`（推導的行為併入自動補依賴）、`GYO_BUILD_*_LOADER` 選項、以 `SDL_` 前綴推定的打包依賴（兩處）、產品 CMake 的 `PUBLIC_COMPONENTS`／`PRIVATE_COMPONENTS`／`COMPONENTS` 參數（函式庫層級的連結怎麼表達由段 (i) 定，計畫 §3.11）、`GyoRenderOptions.cmake` 的後端選項（降為「能不能建 GPU 後端」的開關）、`tests/common/Requirements.cmake` 的 `GYO_COMMON_TEST_COMPONENTS`。`GYO_TEST_PROFILE=full` 改為「選全部 plugin、每個能力一個後端」；同一能力出現第二個後端時改成每個後端一個測試單位，不放寬「多後端」。`GYO_ENABLE_<COMPONENT>` 改由選擇推導。`tests/common/ci/app_copy_integration.py` 的複製前綴加 `plugins/`。
- 產品與工具的 CMake 寫不寫 plugin 的 target 名：DI 2 增補第三點是條文（未決 11），**不設產品側白名單**（review：N4 刪）；各遷移批把產品改成「選擇欄＋Run」時自然消失。

### 3.6 不建議的形狀

- S-A（第一次修訂的建議：第 1 類寫 `backs`）：review 刪——能從所選 plugin 推出來，多一個產品步驟。
- S-B：能力→後端的對照表＋另一個 extensions 欄——兩個鍵，違決定 (a)「同一個選擇欄」。
- S-C：字串文法（`"gpu_device=sdl"`）——要自定文法，接近 AGENTS §9 不許的任意條件設定言語。
- 本修訂採的形狀等於第一次修訂列為次選的 S-D（名字清單＋推導），差別是推導的來源先試 CMake 連結圖、不可行才用 `requires_capabilities`。

---

## 4. 未決 17 題的處置（第二次修訂：全部已定）

第一次修訂時這節是「交使用者的未決」，每題附選項、代價、建議。使用者在 #212 的討論與複核中逐題定了；本節只寫決定與它在計畫裡的落點，選項的比較見 git 歷史（`b647de39`）與決定行。引用：**HO-212**＝kernel `HANDOFF.md` 現況末尾「#212 的使用者決定」兩行；**review**＝決定行 `v8-arch-adapter-plugins-review`；兩者不同處以 review 為準。

### 未決 1（JSON）——已定

- **使用者原話**：「yyjson，數字走原文路。（直接加GYO的介面，內化到Engine中未來有變化，也有介面作為緩衝）」；方向的前一句：「我想把 json＋介面內化到 Engine 中沒問題吧」（主 checkout `build/evidence/adapter-plugins-plan/discussion-notes.md`〈追加（JSON）〉；主會話轉達）。調查：`docs/architecture/plans/third-party-evaluation/json-library-survey.zh-Hant.md`（`v8-arch-json-library-survey`，PR #216，已在 master；本分支尚未併入）。
- **讀法**：一般規則的實例，不是例外——JSON 解析是 Engine 的能力（Q-b：nlohmann 今天用在 Engine 7 個模組讀自己的資料契約：asset、ui、map_markers、voxel_mesh、model、render、base；拿掉會壞），所以 adapter 在 Engine 裡（軸 1）。今天未達軸 1 的兩點：一組件一 adapter（7 個模組各自直連）、介面用 Engine 的詞（`StrictJson.hpp` 以 nlohmann 型別為模板參數，使用者拿到 nlohmann 的 DOM）。
- **定案**：一個 Engine 的 JSON 模組是唯一連 JSON 函式庫的地方，函式庫＝**yyjson**；其他 6 個模組與 shader 建置期工具改用它；介面是 GYO 自己的（Engine 的詞，使用者拿不到函式庫的 DOM 型別，換函式庫時那些模組不用改）；**數字走原文路**：adapter 以 `YYJSON_READ_NUMBER_AS_RAW` 交出原文，Engine 以 `std::from_chars` 轉。
- **介面契約要寫明的行為差異**（調查報告 §1、§2.3、§3）：`-0` 讀成 `-0.0`（今天 nlohmann 讀成整數 0；repo 內的 JSON 沒有 `-0`）；下溢（`1e-400` 等）由今天的 `+0.0` 變成錯誤（libstdc++ 的 `from_chars` 回 `result_out_of_range`）；超出範圍的小數在原文旗標下 yyjson 會接受，由 Engine 的轉換擋下；今天 `StrictJson` 接受「`123`＋NUL」，新介面照嚴格解析拒絕；UTF-8 BOM 由接受變拒絕（repo 內容 70 檔無 BOM）。後三項是**締める面**，在 json-adapter 的宣告寫明。
- **限界**：調查只在 linux-x64（Docker）實測；windows-x64、macos-arm64 的原文路要在 json-adapter 以同一組數字向量於 L1 驗 0 不一致。
- **計畫的落點**：`v8-arch-json-adapter` 回到計畫、**不再是條件批**（計畫 §3.17c）；在 G2a 後、adapters-relocate 之前（shader 工具在搬進 SDL plugin 之前改用 Engine 的 JSON 介面，不在 plugin 裡另連一份函式庫）。產品、工具、測試直接用 nlohmann（apps 7、tools 17、framework 2、tests 20、acceptance 6 檔）不在這批：照 R3 與 (α) 是產品自己的事。

### 未決 2（建置期工具 `engine/render/shaders/pipeline`）——已定 B（HO-212）

shader 編譯工具、內建 shader 的編譯步驟、`ShaderLibrary` 挑 SPIR-V／DXIL／MSL 的那段搬進 SDL plugin 的 GPU 部分；Engine 留 HLSL 原始碼與 shader ABI；SDL_shadercross 先不換；CI 的 shader 快取鍵路徑在 `.github/` 兩個 workflow，由使用者自己改（使用者：「這樣關於 SDL 的部分就已經全部放在 Plugin 裡面了」）。工具的 JSON 改用 Engine 的 JSON 介面（未決 1）。落點：工具與編譯步驟的搬家在 adapters-relocate（只搬不改形狀；工具自抓的那份 SDL3 也在 SDL plugin 內）；`ShaderLibrary` 的格式挑選搬家是改形狀，在 wrap-render。

### 未決 3（model 取樣規則與 audio core）——已定：留 Engine（review (3)，改動 HO-212 的 (a)）

照新 (b) 讀法兩者都是固化的基礎（產品不在多個取樣規則或 mixer 實作之間選），不是例外。audio core 留 Engine 時 wrap-render 開 `target/audio`。

### 未決 4（「對外介面」的讀法）——已定讀法 1（HO-212）

面向 Engine 以外的系統（第三方組件、OS 裝置、網路、外部行程）的介面。已寫進 runtime-composition §4。

### 未決 5（SDL 的「一個 adapter」）——已定 P2（HO-212）

一個 `sdl` plugin、內含多個後端 target；SDL_image、SDL_ttf 各自一個 plugin、宣告依賴 `sdl`。ImGui 的 SDL3／SDL_GPU 官方後端也在 `sdl` plugin 內（`sdl_imgui`，RC (ii)）。

### 未決 6（三原則套到哪些層）——已定 (a)＋(a1)，補充句照 R3（HO-212；補充句與守衛的部分以 review 為準）

- 測試框架與測試夾具不受原則 (1) 限制（adapter 自己的測試除外）；acceptance 與產品測試當夾具（(a1)），**不列白名單**（review：產品側的棘輪與白名單刪）。
- **R3**（review (6)）：產品獨有的能力屬於產品（目錄、owner、測試）；包成產品目錄下的 plugin 是產品內部的選擇（應用 Architecture，觀測到 Pressure 再做）；唯一的硬規則是產品連外部組件時經對接它的 plugin，沒有就在產品目錄下做；跨語言、跨行程的可替換性由 wire 契約負責，Gateway 不需包成 plugin。
- 「（Engine + Plugins）⟹ Game」不變（HO-212）。
- **F1 的範圍 (α)**（review）：「升級只改 adapter」只對 Engine 承諾；產品用了 plugin 露出的第三方型別，升級時產品自己負責。

### 未決 7（ImGui 在工具裡的直接呼叫）——已定：取消限制（review (4)，改動 HO-212 的 C）

工具依賴 imgui plugin、直接呼叫它露出的 ImGui API（(iii) 允許），不設「只准 widget」的限制與工具守衛。GYO 自己的 UI API 仍是未來目標。

### 未決 8（protobuf 的 generated code）——已定 A，列名機制刪（review (5)）

網路 plugin 擁有 protobuf 執行期函式庫的加入、版本與 protoc 規則；產品的 protocol 目錄與 generated code 是產品經網路 plugin 用它（R3、(iii)）。「手寫產品程式不含 `google/protobuf/*`、解碼檔要列名」刪。

### 未決 9（SDL 2D renderer）——已定 I1（HO-212）

SDL 2D renderer 在 plugins-editor 退役；接受 audio_editor、ui_editor 需要 GPU。ImGui 的繪製用官方 SDL_GPU 後端（`sdl_imgui`）；`IRenderDevice` 是否還要擴充（裁切、索引子範圍）由 plugins-editor 的宣告確認（用官方後端時可能不需要）。

### 未決 10（target §4 的 Q2 句）——已定 (iii)（HO-212）

原則 3 是對 Engine 說的：adapter plugin 可以對依賴它的產品與 plugin 露出第三方型別；實作 Engine 定的介面的那部分只用 Engine 的詞；Engine 的標頭不露第三方型別。plugin 版標頭衛生守衛（`v8-arch-guards-inventory-dispositions` Q2 的新增）照這個規則、提前到 adapters-relocate。runtime-composition §4 的 Q2 句已照 (iii) 改寫（本修訂）。

### 未決 11（runtime-composition 的 Design Invariants 增改）——已同意，DI 5 照 review 改寫（Architecture Delta）

使用者在 HO-212 一起核准 DI 2 增補、新 DI 5（依 (iii) 改窄）、DI 6 與兩條 RC；review (2) 再改寫 DI 5 的末句。**目前 `target/runtime-composition.zh-Hant.md` 的 Design Invariants 節只有 DI 1～4，沒有 DI 5、DI 6**，本修訂不新增（修訂前後該節 md5 相同，見文末）。**落字時點：plugins-foundation 的事前宣告之前**（N2 與 Engine 側掃描以它們為依據），由架構師在那時寫進 DI 節，文字如下：

- **DI 2 增補**（加在「產品與工具的 plugin 選擇只寫在 `project.json`」之後）：選擇以可執行單位（role、variant）為粒度；第 1 類後端也經同一個欄選擇；產品與工具的 CMake 不寫 plugin 或後端的 target 名。守衛：N2；第三點不設白名單（review：N4 刪）。
- **新 DI 5**：「外部組件的型別與呼叫只在它的 adapter，每個組件一個 adapter：歸 plugin 的能力，adapter 在那個 plugin；**Engine 只在判斷順序判定屬 Engine 的能力裡、經它唯一的 adapter 連外部組件**。Engine 的標頭，以及 adapter 實作 Engine 介面的部分，只用 Engine 的詞；adapter 可以對依賴它的產品與 plugin 露出第三方型別。」守衛：Engine 側的含入與連結掃描（N1，只掃 `engine/`）＋plugin 版標頭衛生守衛（adapters-relocate）＋依賴方向掃描。Engine 目前唯一的外部組件 adapter 是 JSON 模組（yyjson）。
- **新 DI 6**：「每個可執行單位用到的、需要後端的能力恰好有一個已選後端；缺或多是 configure 錯誤並指名能力。」守衛：§3.4 的三個合成反例（N2）。
- **新 RC 兩條**：(i) 升級一個第 1 類 adapter 時，它輸出的內容身分 (c) 改變（例：ufbx 的載入結果變了，`AnimationContentDigest` 跟著變）→ 不是「只改 adapter」，走內容身分的版本流程；(ii) 同一組件的對接出現在第二個 plugin 時，不放寬 DI 5，而是把對接搬回它的 adapter；介面不足就回 Engine 補介面。**RC (ii) 已用一次**：ImGui 的 SDL 後端放進 SDL plugin 內的 `sdl_imgui`，不在 imgui plugin（review）。

### 未決 12（計畫 §8 的三處）——已定（review）

RC-8：Delta-7 算第 1 個（照規劃稿）；RC-11：錨點 1,299 h **不調**；RC-12：**刪**（RC-8 已涵蓋）。

### 未決 13（其餘能力模組的歸屬）——已定（review）

照 Q-b 與新 (b) 讀法逐個判（§1），各遷移批確認；**ui 目前算 Engine**（產品不在多個 UI 實作之間選；GYO 自己的 UI API 為未來目標）。HANDOFF〈未定〉(3) 由主會話結案。

### 未決 14（GYOP 契約與向量檔）——已定 G-b（review）

契約文件與向量檔放在 C++ 與 Go 兩個實作之外（位置由 net-link 宣告定）。協議管理現在只走 G-b，出現第二份跨實作共用協議時再升格，不新立頂層（AGENTS §10）。另：`services/gyo_gateway` 的 owner＝網路 plugin 的 owner（review (7)，O1）。

### 未決 15（adapters-relocate 與 Match PR 線的順序）——已定 R-a（review）

adapters-relocate 以 master 為基底先合併，Match 線 rebase 吸收；衝突解決計入 RC-7。

### 未決 16（過渡期 ufbx plugin 的 build 函式）——已定 (a)（review）

assets-core 建 `plugins/ufbx/`，只放 build 函式、`plugin.json`（`base` 暫指 Engine 的 `GYO::AssetUfbx`）與 `docs/plugins/ufbx` 頁，owner＝ufbx plugin；loader 程式在 adapters-relocate 搬進同一目錄。round2 那一句（「assets-core 本身仍只依賴 world-exec／loading」）隨 (a) 改為「開工只依賴 world-exec；結束依賴 loading 與 plugins-foundation 段 (i)」（記在計畫修訂記錄「與既有決定行的差異」；決定行只寫「16 (a)」，這句的改寫請主會話在 HANDOFF 一併記）。

### 未決 17（U4 的代價變了）——已知悉（review）

刪減後重算：2' 階段＝rollback-extract 25～46＋json-adapter 21～42＋adapters-relocate 22～45＋wrap-render 24～65＋wrap-assets 9～20＋plugins-frame 14～30＝**115～248 h**（U4 當時 70～153 h），且 adapters-relocate 改的不只 kernel／engine。§7 是已定事項，計畫不改它的文字。

### 流程事項

- `docs/architecture/README.md` 的 `Layers and dependencies` 條目：本修訂只加一句區分「Engine 自有 plugin」與「`plugins/` 的擴展」並指向 runtime-composition §4（檔案歸圖書管理員，只動這一條）。
- 未查證、交執行批先查：Windows 上把 SDL3.dll 放到測試執行檔旁的機制在搬家後是否照舊（adapters-relocate 宣告第一步）；ImGui 官方 SDL3／SDL_GPU 後端的版本與授權（plugins-editor 開工時）；段 (i) 的 CMake 連結圖推導是否可行（段 (i) 宣告先驗）；shader 工具（獨立的 host 專案，交叉編譯時為 host 建）連 Engine 的 JSON 模組的建置方式（json-adapter 宣告）。
- **待更正**（本批不改那兩份檔，交圖書管理員或下一次碰它們的批）：`docs/architecture/plans/foundation-followups/PLAN.md:583`「Engine 已擁有 Go 版（`services/gyo_gateway/framing/framing.go`）」——owner 已改為網路 plugin 的 owner（review (7)）；`docs/architecture/plans/kernel/spec.zh-Hant.md:1262` Gateway 列「外部，不變」——owner 是網路 plugin 的 owner，不是 Engine 之外的無主服務。

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

### 複核後的修正（第二次修訂；`v8-arch-adapter-plugins-review`）

上表是第一次修訂時的取捨，保留作審計線；下列各行被 review 決定行與未決的決定改掉，以這裡為準。

- **T5**（綁定寫法）：S-A（`backs`）刪；改為名字清單＋推導（段 (i) 先驗 CMake 連結圖，不可行回 `requires_capabilities`）。放棄的代價：「這個單位用 SDL 當哪些能力的後端」不直接寫在產品資料上，要看推導結果（configure 可印出）。
- **T8**（JSON）：方向與函式庫已定（Engine 的能力、yyjson、原文路）；json-adapter 是非條件批、21～42 h（依 yyjson 重估，推定），排在 G2a 後、adapters-relocate 之前。第一次修訂「只對 adapter 都是 plugin 開一個例外」的說法刪——選項 C 改窄後不是例外。
- **T9**（SDL 2D）：I1 已定。
- **T11**（imgui 的 SDL glue）：+8～16 h 的自寫刪；ImGui 官方 SDL3 平台後端＋SDL_GPU 繪製後端放進 SDL plugin 內的 `sdl_imgui`（RC (ii) 選 (ii)），只有選了 imgui 的產品連它；版本與授權在 plugins-editor 開工時驗。plugins-editor 31～62 → 23～51 h。
- **T12**（守衛）：產品側的棘輪與白名單全刪（N1 的產品側、N4、N5、N7）；過渡登記表留表刪棘輪（N6 刪，closeout 判一次 0 列）；只留 Engine 側掃描（N1 改為只掃 `engine/`）、N2（三種錯誤）、N3。
- **T14**（估時）：本修訂 700～1,448 h（非條件 36 批；其中 B1 8～16 與 json-adapter 21～42 是新算入的；不含這兩批 671～1,390）。
- **T15**（RC-8）：Delta-7 算第 1 個（已定）。
- **歸屬的判法**：第一次修訂的「推論 R3」退役（Q-b 附句「中性介面算 Engine 的能力」取代）；model core、audio core 從「待使用者」改為照新 (b) 讀法留 Engine。

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

---

## 複核與討論後的修訂記錄（第二次修訂，2026-11-13）

依據：kernel `HANDOFF.md` 決定行 `v8-arch-adapter-plugins-review`（使用者在複核討論卡逐項確認；全文紀錄在主 checkout `build/evidence/adapter-plugins-plan/discussion-notes.md`）、`v8-arch-external-components-as-adapters`、`v8-arch-engine-or-plugin-criteria`（被 review 改動的部分以 review 為準）、HANDOFF 現況末尾「#212 的使用者決定」兩行；未決 1 的方向與函式庫由主會話轉達使用者原話（discussion-notes〈追加（JSON）〉與主會話訊息）。架構師的複核意見 `adapter-plugins-review.zh-Hant.md` 是當時的提案，與決定行不同處以決定行為準（例：G01／G06 是刪、不是改成 closeout 掃描；§3.4 是 SDL plugin 內的 `sdl_imgui`，不是 imgui plugin 依賴 SDL plugin）。修訂者：Opus／high；未經不同模型檢查（判定 6 (c)：最終稿由 Fable 檢查一次）。

**改了哪些段**：

| 位置 | 改了什麼 |
|---|---|
| 開頭 | 件数の突き合わせ加第二次修訂的一句；狀態、性質、已定前提、本稿改的其他檔 |
| §0 | 全段改寫（根因與方向、Feature 2／3 的機制、F1 的範圍 (α)、歸屬、搬法、合計、受影響的批、未決全定） |
| §1 前言 | 判斷順序加 Q-b 與 (b) 新讀法；兩軸；plugin 與 owner；停止條件重判；「推論 R3」退役 |
| §1 表 | 第 2（audio core）、12（model core）列改「留 Engine，已定」；第 13 列加 owner（O1）與 G-b；第 16a、16c、22、25、27、28、29、30、34～36 列照未決的決定改；第 1、7、15、16、18 列「（R3）」改成「中性介面＝Engine 的能力」；「建議（未決 13）」改「已定方向（未決 13，各遷移批確認）」 |
| §2 | 後端表的 audio（輸出的中性型別改由宣告定）、2D（I1 已定）、sdl_gpu（擴充是否需要由 plugins-editor 確認）、net（G-b、O1）列；`*Native` 段（N5 刪）；消費者具體類名段（(α)：不再是判定）；Engine 外要改的其他地方（protobuf 句）；合計段；platform-sdl.md 去向表的 DI 1、DI 2、RC 2 列 |
| §3 | 全節改寫（只寫 plugin 名、推導、自動補依賴、三種錯誤、預留 B1 的形狀、過渡登記表留表刪棘輪） |
| §4 | 全節改寫成「17 題的處置」（全部已定；未決 1 照使用者原話；未決 11 的 DI 文字與落字時點；流程事項加待更正清單） |
| §5 | 文末加「複核後的修正」一段（T5、T8、T9、T11、T12、T14、T15 與歸屬判法）；原表保留 |

**刪掉的規則、守衛與錯誤代號**（對照 review 決定行逐項）：

| review 決定行的項 | 第一次修訂的內容 | 處置 |
|---|---|---|
| 產品側棘輪與白名單（G01） | N1 第三方含入棘輪（全 repo、只減不增） | 刪產品側；N1 改為 **Engine 側的含入與連結掃描（只掃 `engine/`）**，在 adapters-relocate 新增 |
| 同上（G06） | N7 asio、httplib 連結者白名單 | 刪 |
| 同上（T10 的產品側） | 產品獨有需求一律做成產品目錄下的 plugin；acceptance／產品測試列白名單 | 改讀 R3；白名單刪 |
| 同上（T12 的產品側） | 手寫產品程式不含 `google/protobuf/*`、解碼檔列名 | 刪（未決 8 的列名機制） |
| 同上（G03，review 提案的同類） | N4 產品與工具 CMake 不寫 plugin／後端 target 名的白名單 | 刪白名單；DI 2 增補第三點留為條文 |
| G04 | N5 `*Native` 只准 SDL 家族使用 | 刪 |
| 未決 7 | 工具只准呼叫 ImGui widget、寫進各工具守衛 | 刪 |
| 未決 8 | protobuf 列名機制 | 刪 |
| S10 | 錯誤 `unused-backend` | 刪 |
| S02 | 錯誤 `role-mismatch`（兩向檢查） | 刪 |
| G07 | configure 順序檢查（`plugins/` 先於 apps／tests／acceptance） | 刪 |
| P15 | RC-12（搬家混入介面改動） | 刪 |
| S04 | 不自動補依賴、`missing-dependency` | 反轉為自動補依賴；`missing-dependency` 不再是錯誤 |
| S03／S08 | 產品寫 `backs`；`requires_capabilities` 全寫在登記資料 | `backs` 刪；能力由段 (i) 先驗 CMake 連結圖推導，不可行回宣告式（留 S08） |
| RC-11 | 錨點建議改 1,400／1,442 | 不調（1,299） |
| （review 提案 §3.2，決定行未逐項列） | 錯誤代號 `not-provided`、`dependency-cycle`、`invalid-backend` 與固定訊息格式（S11、S12）；過渡登記表的棘輪 N6（G05）；各批宣告的「新增必填項」與額外的突變一覽敵對檢查重述（G19、P09～P13） | 降為實作細節或刪（併回各批事前宣告的既有欄位）；**修訂者照 review 提案處理，決定行沒有逐項寫，請主會話確認** |
| RC (ii) 選 (ii) | imgui 的平台半與繪製半自寫（+8～16 h） | 刪；ImGui 官方後端進 SDL plugin 的 `sdl_imgui` |

configure 錯誤 9 → 3；補遺二的新守衛 7 → 3（N1 改為 Engine 側、N2、N3）。

**保留的**（review §3.1，守 Engine 與 adapter 的邊界或 Feature 本身）：判斷順序、三原則（對 Engine）、兩類 plugin、SDL 家族、P2、未決 2 B、(iii)、DI 5（改寫）、DI 6、兩條 RC、SDL 2D 退役；選擇欄與三種錯誤；Engine 側的含入與連結掃描、Engine 標頭衛生與 plugin 版標頭衛生守衛、搬家後的含入根不沿用 `engine/` 前綴、中性 platform 契約的合成實作測試、byte 級等價、ctest 名單相同、產品 SDL link guard 補 `SDL3_ttf`；新批 adapters-relocate（範圍限於機械搬動）。

**還留下的「往產品或工具加步驟」的條目**：(1) 在選擇欄寫要用的 plugin（Feature 2 本身）；(2) R3：產品連外部組件時經對接它的 plugin，沒有就在產品目錄下做。遷移批對產品與工具的一次性改寫（tool-ui 去掉直接的 SDL 呼叫、client-frame 的 main＝選擇＋Run 等）是遷移本身，不是之後的日常步驟。

**件数**：批（計畫）37＝非條件 36＋條件 1（pageguard-os）；第一次修訂 36＝非條件 34＋條件 2。新增 B1（規劃批）；json-adapter 由條件改非條件。重算（腳本與輸出：主 checkout `build/evidence/adapter-plugins-plan/reviser2-sum.{py,txt}`）：非條件合計 700～1,448 h（原計畫 632～1,299；第一次修訂 681～1,400）；關鍵路徑 332～694 h 不變。

**Design Invariants 節**：`target/runtime-composition.zh-Hant.md` 的 Design Invariants 節修訂前後 md5 相同（`be34398d97c60983a79c1d4041828b6e`，與 HEAD `effa46cc` 一致）；DI 5、DI 6、DI 2 增補的落字時點在 plugins-foundation 的事前宣告之前（§4 未決 11）。

**修訂者沒有把握的地方**：(1) 上表倒數第二行——決定行沒逐項列的 review 提案項（S11、S12、G05、G19、P09～P13）照提案刪或降級；(2) json-adapter 的 21～42 h 是推定（yyjson 是 C 函式庫、包一層 C++ adapter；沒有 GYO 內的先例）；(3) 段 (i) 的 CMake 連結圖推導可行性未查證，回宣告式時過渡登記表要帶手寫的 `requires_capabilities`（第一次修訂對抗 S-2 的限界回來）；(4) round2 一句的改寫是否算在「16 (a)」裡；(5) B1 的估時 8～16 h 與排程（Kernel 線內、功能線重開前，依賴段 (i)）是推定，它產出的執行批的成本沒有算進合計；(6) shader 工具是交叉編譯時為 host 建的獨立專案，連 Engine 的 JSON 模組的方式未查證。(7) RC (ii) 的用語「整個 repo 的 SDL 呼叫只在 sdl plugin」與未決 6「測試夾具不受原則 (1) 限制」的交界：`tests/ui_editor/PreviewAssetTests.cpp` 今天直接含 SDL3，本稿交 plugins-editor 的四選一，沒有判它違反 RC (ii)。

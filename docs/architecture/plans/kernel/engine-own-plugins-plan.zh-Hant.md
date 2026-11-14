# 規劃稿：`v8-arch-engine-own-plugins-plan`（B1：Engine 模組改為 Engine 自有 plugin、由 App 組裝）

更新：2026-11-14。Owner：架構（架構師會話的任務卡 04）。**狀態：敵對檢查後修訂稿（Fable／high 一次：blocker 1／should 15／nit 12，全部處置；處置記錄在文末）。**

出處：
- 事前宣告 `plans/kernel/engine-own-plugins-plan-declaration.zh-Hant.md`（已核准，使用者 2026-11-14「核准，選 A」）。範圍、F1～F3、七題、判定、停止條件都以它為準。
- 偵察 a（23 個模組今天怎麼組裝）、偵察 b（Bevy 對照、計畫現況、Kernel 現況）；提案①風險與可逆、②產品自由度、③成本（皆 Opus／high）；統合者的計畫修訂清單；Fable 的敵對檢查報告。全部存在主 checkout（本機 macOS）`/Users/karasu/Code/Source/GYO-Engine/build/evidence/engine-own-plugins-plan/`（git 管理外）：`scout-a.md`、`scout-b.md`、`propose-risk.md`、`propose-freedom.md`、`propose-cost.md`、`synth-plan-edits.md`、`adversarial-fable.md`。本稿寫「偵察 a／b」「提案①②③」處，指的就是這幾個檔。
- 統合者與修訂者（皆 Opus／high）重新核對過的事實都寫「檔:行」；HEAD `d25aff0e`。
- 合計與關鍵路徑的算式直接寫在 §4（不另存腳本）。

名詞：
- **E**＝Engine 自有 plugin（owner 是 Engine，隨 Engine 改版）。
- **擴展**＝`plugins/` 下的 plugin（第 1 類後端、第 2 類能力提昇）。
- **App 記錄**＝App 收集產品直接登記的那一筆 owner 記錄（§2.1）。
- **卡 08**＝架構師已開的缺陷卡「修 cFingerprint 用絕對 id 的缺陷（先 Red）」（§2.7）。

---

## 摘要（結論）

1. **只有「有東西要登記進 Registry」的 Engine 模組成為 E**：asset、audio、input、model、render、text、ui（7 個模組；另衍生 `model_render`、`ui_render`，以及 wrap-render 新立的中性 platform 契約由 E `platform` 承載，共 10 個 E）。其餘是 Kernel 之下的基礎（5）、函式庫（6）、機制或不適用（2）、已定進 `plugins/`（3）。歸屬 0 列改變。
2. **F2 用「App 記錄」達成**：產品在 App 上直接登記 System 與 Resource；App 在建 Registry 時把它們收成一筆排在清單**末尾**的合成 plugin。`Registry`、`Plugin`、`Registrar`、fingerprint 算法都不改；只在 world-exec 本來就要來的 `App` 上加入口。spec C.2 要補一句，列為未決（B1-U1、B1-U16）。Match 與 ClientSim 的 (c) 登記**不改**（沿用產品 plugin，兩端同一份）。
3. **E 與擴展用同一份 `plugin.json` 格式**，多一個 `owner` 欄（`engine`／`extension`／`product`）。owner 不影響選擇、順序、錯誤（F3）；只決定 owner 文件位置、刪除驗證的範圍、過渡登記表拒收。E 的登記資料從第一天就放在 `engine/<module>/plugins/<e>/plugin.json`（每個 E 一個目錄），不借過渡登記表。這把段 (i) 的「預留形狀、不實作」改成實作，三處已定文字要改寫（B1-U8、B1-U12）。
4. **選擇欄以 Runtime 為粒度**（`roles.<role>.runtimes.<rt>`、工具 `variants.<v>.runtimes.<rt>`），要在 plugins-foundation 段 (i) 的 v2 一次定形，否則 Client 三個 Runtime 會逼出 v3。它改動 DI 2 增補第一點、DI 6 等已定文字（§5，B1-U10）。
5. **不設預設組**（建議 D0）：靠 `requires` 的自動補依賴縮短清單；資料組等觀測到壓力再加（B1-U21）。
6. **執行批不另開大批**：新增 1 個機制批 `v8-arch-app-assembly`，其餘是既有批的差分——段 (i)、assets-core、match-host（只有文字）、adapters-relocate、wrap-render、wrap-assets、plugins-frame（只加判定）、client-sim、client-frame、plugins-editor、工具四批、closeout。全部在 closeout 之前，功能線照原計畫在 closeout 後重開，與 AGENTS §2 同方向。
7. **成本（推定）**：執行批淨增 **+23.5～57 h**（差分 15～40＋新批 8.5～17）。若核准，計畫合計 701～1,450 → **724.5～1,507 h**；非條件批 36 → 37；關鍵路徑 333～696 → **338～707 h**。RC-11 觸發點 1,948.5 h 未到（餘裕 **441.5 h**），但超出錨點 1,299 h 的量從 151 h 變成 **208 h**，照實報告。
8. **cFingerprint 的缺陷候選另由卡 08 處理（已開）**：`CFingerprintOf` 對 zone-C 項目寫**絕對 id**（`engine/kernel/src/Fingerprint.cpp:310-326`），與 spec「只含 zone=C 的部分」（`spec.zh-Hant.md:379`）不一致，是已完成批 registry-schedule（#213）的缺陷候選。B1 不在這題上選，也不把它寫成產品契約；B1 只記它對 F1 的影響與批的依賴（§2.7）。

---

## 件数の突き合わせ

- **模組數**：§1 共 23 列；`git ls-tree -d --name-only HEAD engine/` 為 23（統合者 2026-11-14 重跑；先以已知正例 `engine/kernel` 命中，`grep -c '^engine/kernel$'`＝1）。**一致**。沒有自己 CMakeLists 的 asset、io、runtime、config 都列了。
- **分類合計**：E 7＋基礎 5＋函式庫 6＋機制／不適用 2＋已定進 `plugins/` 3＝23。**一致**。
- **E 的個數**：模組 7（asset、audio、input、model、render、text、ui）＋衍生 2（`model_render`、`ui_render`）＋中性 platform 契約 1（`platform`）＝10；§1〈E 的登記資料位置〉表 10 列。**一致**。
- **七題**：§3 有 S-3 (2)（連帶 S-3 (1)）、S-1、S-5、S-8、必答 1、必答 2、必答 3，共 7 題，每題 ≥2 個選項、有「影響的批」欄，都標「交使用者」。
- **〈仍往產品加步驟的條目〉**：提案前 7 列（`adapter-plugins-plan.zh-Hant.md:462-476`），照建議提案後 **7 列**（§7：刪 3、4、5，加 3 列）。
- **`plugins/` 目錄**：今天不存在（`ls plugins` 失敗）。
- 本稿沒有新增的 grep 判定；沿用偵察 a、b 的掃描（兩份都先以正例命中，偵察 a 照實報告第一次 `-E` 掃描失敗、改 `-P` 重跑）。修訂者另以 `sed -n` 直接讀了 `Fingerprint.cpp:300-330`、`CoverageTests.cpp:492-502`、`Registry.hpp:175-192`、`:224-228`、`Registry.cpp:437-445`、`FontLoader.hpp`、`TextLoader.hpp`、`PlayerAnimationContent.cpp:85-97`、`PvpApplication.cpp:955-962`，內容與本稿引用一致。

---

## §1 `engine/` 23 個模組逐一的去處

**判準（提案，交使用者 B1-U9）**：模組有東西要登記進 Registry（Resource、System、資產型別、要宣告「需要後端」的能力），才成為 E。無狀態的函式庫不成 plugin，產品直接呼叫。
- 參照：Bevy 的 `bevy_math`、`bevy_ecs`、`bevy_tasks`、`bevy_color` 沒有 Plugin impl（偵察 b）。
- 宣告的三分類沒有「Kernel 之上、但在 Registry 沒有東西」這一格；表中的「函式庫」是這一格的子標記，**不改歸屬、不改 owner**（判定 2）。

**另一條規則（提案，§6 的新 DI）**：Engine 能力的型別（Resource、資產型別、中性 boundary）只由該能力的 E 登記；runner、後端、產品不代登。理由是 R-6「每個型別恰有一個 owner plugin」（spec `:400`）：若由後端登記，換後端時 owner 跟著變，F3 不成立。text 與中性 platform 契約因此成為 E。

| # | 模組 | 去處 | 今天誰初始化（檔＋符號） | 成為 E 後 build／finish 登記什麼 | 依賴（`requires`／需要後端） | 落在哪批 | 支撐 |
|---|---|---|---|---|---|---|---|
| 1 | asset | **E `asset`** | Client：`PvpApplication.cpp:96-104`（七件一組）、每幀 `BeginFrame`／`Update`（`:1141-1142`）；Match：`PlayerAnimationContent.cpp:87-96`；`tools/ui_editor/src/AssetPreviewContext.cpp:104` | build：請求狀態 Resource、`AssetInbox`、`AssetStaging`／`AssetCursor`、Loading 的 `StageAssets`／`ApplyStagedAssets`、loader 登記 API、快取策略的設定 Resource（§2.6）；**`engine/asset` 自帶的內建 loader**（`TextLoader`、`FontLoader`、`SoundLoader`、`BinaryLoader`；`engine/asset/include/engine/asset/loaders/`）。finish：無（asset-io 服務由 runner 起，計畫 §3.11）。`AssetManager` 轉接到 framework-delete，不納入 | 無後端。`ufbx`、`sdl_image`、`model` requires 它 | assets-core | F1、F2 |
| 2 | audio | **E `audio`**（core） | `apps/object_fps_pvp/src/Present/PvpSound.cpp:68-75`；`tools/audio_editor/src/AudioEditorWindow.cpp:85`、`:96`；`tools/model_viewer/src/PreviewSounds.cpp:39` | build：`Submitter` Resource（親和照 threading）、Mixer 設定。finish：無。開裝置由 sdl 的音訊 target 做（`SdlAudioOutput::Open(Mixer&&)` 會搬走 Mixer），細節由 wrap-render 宣告定 | 需要後端 `audio_output`。離線混音只把 `Mixer` 當函式庫用、不選本 plugin | wrap-render | F1 |
| 3 | base | 基礎（不成 plugin） | 全域 handler：`apps/object_fps_pvp/main.cpp:74`、`src/Pvp/LogFile.cpp:175` | — | Kernel 唯一的 build 依賴（`engine/kernel/CMakeLists.txt:18`） | — | — |
| 4 | collision | 函式庫 | `apps/object_fps_pvp/src/Sim/Arena.cpp:74`（`StaticAabbGrid::Build`，產品資料） | — | — | — | — |
| 5 | config | 不適用（Product Registration 資料，不是程式） | `build/cmake/GyoApps.cmake`、`GyoTools.cmake` 讀 CSV | — | — | — | — |
| 6 | entity_store | 基礎（World 欄的實作，Delta-3） | 只有型別（`EntityColumns.hpp:4-5`） | — | 今天 Kernel 不連它；依據是未來的依賴（沒把握 5） | — | — |
| 7 | input | **E `input`**（core；frame 與 Editor 文件 World） | `PvpApplication.cpp:1093`；`framework/bindings/imgui/src/ImGuiShell.cpp:458`；每幀 `PvpApplication.cpp:1111`、`:1116`、`:1119` | build：`PhysicalInputFrame` boundary Resource（後端在 pump 回呼裡填）；動作對應 System 要不要做由 wrap-render 宣告定（`InputActionMap` 今天只有 tests 用） | 需要後端 `input_source` | wrap-render | F1 |
| 8 | io | 函式庫（介面＋同步實作；IO 執行緒服務已排進 `plugins/`，計畫 §3.11） | 短命物件：`NativeFiles.cpp:47`、`ImGuiShell.cpp:78` 等 | — | — | — | — |
| 9 | kernel | 機制本身 | 只有 tests（`tests/common/kernel/RegistryTests.cpp:114` 等） | 只加 App（§2.1） | — | app-assembly | F2 |
| 10 | map_markers | 函式庫 | `tools/voxel_map_editor/src/MarkersFile.cpp:61` | — | — | — | — |
| 11 | math | 函式庫（header-only；runtime-composition `:56` 末句明列 math 留 Engine，只是不在第一問的例舉裡；不影響本表） | 無 | — | — | — | — |
| 12 | model | **E `model`**（薄）；取樣仍是函式庫 | loader 登記：`PvpApplication.cpp:961`、`PlayerAnimationContent.cpp:90`；取樣：`PlayerPose.cpp:511-533` | build：`Assets<ModelAsset>` 等資產型別。由 Engine 登記，不由 ufbx 登記：換 loader 時型別的 owner 不動（F3）。同一型別兩個 loader 本來就是 finish 的明確錯誤（計畫 §3.20），與 owner 無關 | requires `asset`；`ufbx` requires `asset`、`model` | assets-core | F1、F3 |
| 13 | net | 已定進 `plugins/`（網路 plugin，第 2 類） | `apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp:38`、`:47`、`:51` | 不屬於 B1 | — | net-link | — |
| 14 | os | 基礎（runtime-composition `:56` 明列 OS） | `apps/object_fps_pvp/match_main.cpp:42`、`framework/src/HeadlessShell.cpp:11` | — | — | — | — |
| 15 | platform | **現有程式已定進 `plugins/sdl`**（核心）。**wrap-render 新立的中性 platform 契約由 E `platform` 承載**（新程式的組成形式，不是搬現有模組） | `PvpApplication.cpp:1023`、pump `:1114`；`framework/src/WindowShell.cpp:47` | build：中性 boundary（`PlatformEvents`、視窗設定、路徑對話框的請求與回覆）。runner 仍在 finish 由 window_runner 安裝 | 需要後端 `platform`；`window_runner` requires 它 | wrap-render | F1、F3 |
| 16 | render | **E `render`**（render Runtime）＋衍生 **E `model_render`**（`gyo_model_renderer`） | `PvpApplication.cpp:108`、`:953`、`:957`（ShaderLibrary）、`:1029`（device）、`:1033`（Renderer）；`framework/src/Preview.cpp:64-70`；`ImGuiShell.cpp:385` | `render` build：`ShaderLibrary`（中性部分）、`Renderer`、`RenderQueue` 的 Resource（親和 `GraphicsContextOwner`）、RenderSubmit 的 reset／render System、`RenderAssets<T>` 機制。device 由 sdl 的 GPU target 建，時點（finish 或 Startup）由 wrap-render 宣告定。`model_render` build：`RenderAssets<ModelAsset>`、提交 System | `render` 需要後端 `gpu_device`；`model_render` requires `render`、`model` | wrap-render、wrap-assets | F1 |
| 17 | runtime | 已定：`RuntimeLoop`／`IRuntimeClient` 刪（§3.31）；`FixedTickRuntime` 進 runner plugin（`plugins/`） | `PvpApplication.cpp:1383`；`WindowShell.cpp:62`；`MatchRuntimeHost.hpp:252` | 不屬於 B1 | — | runtime-loop-retire | — |
| 18 | text | **E `text`**（薄） | `PvpApplication.cpp:1035`；`AssetPreviewContext.cpp:156` | build：rasterizer 的 Resource 型別（實例由 sdl_ttf 填）。**字型資產（`FontAsset`、`FontLoader`）仍由 E `asset` 登記**（見下〈內建 loader〉） | 需要後端 `text_rasterizer`；requires `asset` | wrap-render | F1、F3 |
| 19 | threads | 基礎（明列） | `MatchRuntimeHost.cpp:64` 等 | — | — | — | — |
| 20 | time | 基礎（明列） | 自由函式 | — | — | — | — |
| 21 | trace | 函式庫（core 不改；寫檔服務是 `plugins/trace_diagnostics`，計畫 §3.13） | `LogFile.cpp:109` | — | — | — | — |
| 22 | ui | **E `ui`**（frame 或文件 World）＋**E `ui_render`**（render World） | `PvpApplication.cpp:113-114`、`:992`、`:1038`、`:1193`、`:1319`、`:1324`；`tools/ui_editor/src/PreviewAdapter.cpp:251` | `ui`：`UiRuntime` Resource、Update System、Compose 產出的 draw 資料（(b)）。`ui_render`：`Gpu*` Resource 與 Submit（計畫 §3.19b） | `ui_render` requires `render`、`text`；它對 `ui` 是**跨 Runtime 的依賴**（B1-U18） | wrap-assets | F1 |
| 23 | voxel_mesh | 函式庫 | `Arena.cpp:145`；`tools/voxel_map_editor/src/Build.cpp:239` | — | — | — | — |

**偵察 a 的「兩種判法」**（os、threads、time、entity_store）：兩種判法下這四個都不成 plugin（在 Registry 沒有登記物），選哪一種**不改本表任何一列**。B1 不在這題上做決定。

**JSON 模組**（json-adapter 新立，不在今天的 23 個目錄裡；宣告判定 2 明列它留 Engine）：**函式庫**，不成 E——它在 Registry 沒有登記物（解析與寫出是函式）。

**內建 loader（S-12 處置）**：`engine/asset` 自帶的 `TextLoader`（Match 在用，`PlayerAnimationContent.cpp:89`）、`FontLoader`（Client 在用，`PvpApplication.cpp:958`）、`SoundLoader`、`BinaryLoader` **都由 E `asset` 登記**。理由：
- 它們與它們的資產型別都定義在 `engine/asset`（`engine/asset/include/engine/asset/loaders/FontLoader.hpp`、`TextLoader.hpp`、`FontAsset.hpp`），不依賴 `engine/text`。
- 改由 E `text` 登記字型，是 asset → text 的模組間責任移動（AGENTS §3「複数 Module にまたがる Responsibility 移動」），沒有支撐它的 Feature（F1～F3 都不需要）。
- 代價：選了 `asset` 的單位都會帶著這幾個 loader（例：Match 也有 `FontLoader`）。loader 是無狀態的登記，不需要後端，F1「不挑的能力不進 Registry」不受影響（字型的**光柵化**才是能力，那在 E `text`）。
- 第三方 loader（`sdl_image` 的 texture、`ufbx` 的 model）仍由各自的擴展登記。

**target 的形狀與 E 的登記資料位置（S-11 處置；提案，交使用者 B1-U8）**：

建議 A：**每個 E 一個目錄** `engine/<module>/plugins/<e>/`，放 `plugin.json` 與 build 函式的小 target `gyo_<e>_plugin`（連該 E 的核心庫與 `GYO::Kernel`）。段 (i) 的讀取根是 `engine/*/plugins/*/plugin.json`。

| E | 目錄 | 核心庫 target（今天） |
|---|---|---|
| `asset` | `engine/asset/plugins/asset/` | `GYO::Engine`（Q3：v1 不拆） |
| `model` | `engine/model/plugins/model/` | `gyo_model` |
| `audio` | `engine/audio/plugins/audio/` | `gyo_audio` |
| `input` | `engine/input/plugins/input/` | `gyo_input` |
| `text` | `engine/text/plugins/text/` | `gyo_text` |
| `render` | `engine/render/plugins/render/` | `gyo_render` |
| `model_render` | `engine/render/plugins/model_render/` | `gyo_model_renderer`（`engine/render/model/CMakeLists.txt:1`） |
| `ui` | `engine/ui/plugins/ui/` | `gyo_ui`（`engine/ui/CMakeLists.txt:1`） |
| `ui_render` | `engine/ui/plugins/ui_render/` | `gyo_ui_renderer`（`engine/ui/CMakeLists.txt:19`） |
| `platform` | `engine/platform/plugins/platform/` | wrap-render 新立的中性契約 target（現有 `engine/platform/sdl` 在 adapters-relocate 搬走） |

- 同一模組下兩個 E（render、ui）各有自己的目錄，`plugin.json` 與擴展同一格式（一檔一 plugin），F3 不打折。
- 選項 B：一個模組一個 `engine/<m>/plugin.json`，內含多個 plugin。檔數少，但 E 與擴展成了兩種格式，換 owner 時要拆檔（F3 打折）。不建議。

分 target 的理由（是實作選擇，不是新 DI；S-13 處置見 §6 第 5 點）：
- 各模組今天有精確連結集合的守衛，例 `tests/common/collision/CMakeLists.txt:14-16`、`tests/common/audio/CMakeLists.txt:39-41`。把 build 函式放進核心 target 會改這些守衛（AGENTS §14.4 的緩める面）。
- 只把函式庫當型別用的單位（例：Match 的 `player_content` 連 `GYO::Render`，`apps/object_fps_pvp/CMakeLists.txt:50`）不被拖進 Kernel。
- 退回很便宜：刪 plugin target 與它的目錄，核心庫回到今天。
- asset 的特例：計畫 §3.19 的 Q3 字面是「v1 不另開 CMake target」（`migration-plan.zh-Hant.md:551`）。`gyo_asset_plugin` 是一個新 target，**字面上與 Q3 衝突**；它只放 build 函式、不移核心型別，所以 Q3 的用意（核心型別留在 `GYO::Engine`、守衛的連結期望不動）不受影響。Q3 的字面要補「E 的 build 函式 target 不在此限」，**待使用者核准（B1-U8）**。
- `GYO::Engine`（io、asset core、runtime 共用）**不必拆**：io、runtime 都不成 E。

---

## §2 Contract 提案（全部交使用者）

### 2.1 App 的組裝入口（F2；Product：Match 以外的各單位，見下；B1-U1）

事實：
- 今天拿到 `Registrar` 只有一條路：被列進 Registry 的 `Plugin::Build`。`Registrar` 的建構子是 private（`engine/kernel/include/engine/kernel/Registry.hpp:275`），唯一建出點是 `engine/kernel/src/Registry.cpp:1299-1300`。
- `App` 型別還沒有（`Registry.hpp:31-33`：隨 world-exec 來）。

提案（建議 A，「App 記錄」）：
- `GYO::Kernel::App` 提供和 `Registrar` 同一組的登記方法（`AddSystem`、`Resource<T>`、`Component<T>` 等），把呼叫存起來。
- App 建 Registry 時，把一個合成 Plugin 附到清單**末尾**；它的 `Name()` 由建置產生的單位名提供（資料，例：產品 id＋role＋Runtime），`Build` 依序重放那些呼叫。
- 效果：R-1（id 順序＝清單順序＋呼叫順序）、R-6（App 記錄就是那些型別的 owner）、R-E1（單位名撞名可在 configure 先查）、R-E4（移除 plugin 的語義）照舊；`Registry` 不改。
- R-7（Kernel 中性）：名字來自資料；`tests/common/kernel/kernel_guard.py` 只掃 `engine/kernel` 的原始碼文字。
- 產品仍可照 R3 自選包成產品目錄下的 plugin（`plugin.json` 的 `owner: product`）；兩條路並存。
- 純加法：要退回時，只有已改用直接登記的產品要包回 plugin。

**F2 的適用範圍（S-4 處置）**：本計畫**不改 Match 與 ClientSim 的 (c) 登記方式**——兩端沿用同一個產品 plugin（match-sim 建、client-sim 重用）。理由：
- spec 要求「同一組 (c) plugin 登記同一組 (c) System」（`spec.zh-Hant.md:846`）。一邊用 plugin、一邊用 App 記錄，(c) 登記在清單裡的位置就不同；在卡 08 修好之前，這會直接改變 cFingerprint（§2.7）。
- match-sim 在 app-assembly 之前，App 還不存在；之後再改寫成 App 記錄沒有支撐的 Feature（判定 3）。
- 所以 F2 實際交付給：Client 的 frame 與 render Runtime（client-frame）、四個工具（工具四批）。Match 的產品 System 若在 (c) 之外（例：Host 側的 (b) System），可在 app-assembly 之後改用 App 記錄，由那時的產品批決定，本計畫不估時。

**App 記錄的實作細節（N-9；app-assembly 宣告的輸入，本稿不定）**：
1. `Registrar` 的登記方法回傳 id（`Registry.hpp:175-190` 的 `Component`／`Resource`／`Event` 回 `ComponentId` 等，`:226` 起的 `AddSystem` 同類）。App 先存後放，產品在呼叫當下拿不到 id；要嘛 App 的方法不回 id（產品以型別引用），要嘛回一個延遲的句柄。
2. `source_location` 要在 App 的呼叫點捕捉並隨記錄帶進重放，否則 zone-C 的 owner root 檢查（`Fingerprint.cpp:417-435`）會指到 Kernel 的重放程式。
3. `InsertStage` 只能插進已存在的 stage（`Registry.cpp:437-445` 起找不到 anchor 報 D-E1）。App 記錄排在末尾，所以任何 plugin 都插不進產品經 App 記錄登記的 schedule；產品自己的 schedule 若要讓 plugin 插 stage，就要包成產品 plugin。
4. App 記錄沒有 Finish 段的入口（spec C.2 第 4 點 `spec.zh-Hant.md:387` 的 finish 只給 plugin）。產品要在 finish 做事，就要包成產品 plugin。

其他選項：
- B：Bevy 式「函式就是 plugin」（`crates/bevy_app/src/plugin.rs:96-100`）。產品寫 `void Register(Registrar&)`，在選擇欄寫它的名字，產生單元包成合成 plugin。Kernel 0 改；但產品多一個「在選擇欄寫自己的函式名」的步驟，可能被讀成「換了名字的 plugin」。
- C：維持現狀，產品包成 plugin。0 h；〈仍往產品加步驟〉第 4 條留著，與 R3 不一致；F2 不成立，要使用者同意延後。
- D：spec C.2 開第二條路，Registry 接受 owner 為 `<host>` 的登記。要改 Registry、`PluginRecord`、fingerprint 與 J-19；registry-schedule 已完成的部分要重開。不建議。

A 的代價：spec C.2 第 1 點要補「App 可在清單末尾附 App 記錄」（spec 本批不改，列 B1-U16）；App 加入口（app-assembly 批）。

### 2.2 登記資料的 owner 欄（F3；Product：全部單位；B1-U12）

建議：
- `plugin.json` 加 `"owner": "engine" | "extension" | "product"`。
- **`class` 只對 `owner: extension` 有意義**（`backend`＝第 1 類、`extension`＝第 2 類），`engine` 與 `product` 不寫 `class`、寫了是登記資料的驗證錯誤。不另加 `class: capability`——「是 E」已由 `owner: engine` 表達，加了就是同一事實兩處（N-6 處置）。
- **owner 不影響**：選擇的寫法、清單順序、錯誤代號、自動補依賴（F3）。Kernel 不讀它（R-7）。
- **owner 的用途**：
  1. owner 文件位置：`engine` → `docs/architecture/<m>`；`extension` → `docs/plugins/<name>`；`product` → 產品自己的 docs。
  2. 段 (i) 判定 (2)「刪合成 plugin、共通層零修改」只對 `extension`、`product` 適用；E 的刪除＝Engine 的變更。
  3. 過渡登記表拒收 `engine`（必答 3）。
- **只在 Engine 與 adapter 邊界的機械檢查**（configure 時驗登記資料，不是產品步驟）：
  1. `owner` 與 `plugin.json` 所在根目錄一致（比對資料，不是從目錄推 owner，AGENTS §10）。
  2. `owner: engine` 的 plugin 不得 `requires` 任何 `owner: extension` 的 plugin（DI 2「Engine 不 include plugin」在登記層的形）。
  3. `class: backend` 必須是 `owner: extension`。與 R3（「產品連外部組件時經對接它的 plugin，沒有就在產品目錄下做」，`runtime-composition.zh-Hant.md:59`）的關係：產品目錄下的 adapter 是 `owner: product`，它只對接產品自己的能力，不提供 Engine 能力要的後端（第 1 類的定義），所以不是 `class: backend`。產品若要為 Engine 能力提供後端，那個後端就該是擴展——這一點是 R3 與第 1 類定義的推論，交段 (i) 宣告寫明。
- 已排定的 `plugins/` 下 runner 與服務類 plugin（`headless_runner`、`window_runner`、`asset_io`、`trace_diagnostics`）照目錄現況填 `extension`。它們是否其實該是 E，**不在本批重新分類**（改 owner＝判定 2 的停止條件），交使用者。
- **這把段 (i) 的「預留形狀、不實作」改成實作**（S-10 處置）：owner 欄的讀取、三條邊界檢查、過渡表拒收 `engine`、`needs_backend`（§2.4）都是實作。要改寫的已定文字三處，一起交使用者（B1-U12）：
  1. kernel `HANDOFF.md:56` 決定行 `v8-arch-adapter-plugins-review` 的 B1 條「預留形狀、不實作」；
  2. `migration-plan.zh-Hant.md:396`（§3.11 差分的「預留……owner 區分形狀，不實作」）；
  3. `adapter-plugins-plan.zh-Hant.md:184`（「只定形狀、不實作、不讀它」）。

替代：
- 不設欄，由目錄推斷：違反 AGENTS §10。
- E 用 CMake 函式宣告（`adapter-plugins-plan.zh-Hant.md:189` 的 `gyo_engine_capability(...)`）：兩種格式，換 owner 時要改寫成另一種，F3 打折。

### 2.3 預設組（F1；Product：全部單位；B1-U21）

| 選項 | 內容 | 代價 |
|---|---|---|
| **D0（建議）不設** | 靠 `requires` 的自動補依賴（`adapter-plugins-plan.zh-Hant.md:171`）。選 `sdl_image` 帶 `sdl`；選 `ui_render` 帶 `render`、`text`；選 `model_render` 帶 `render`、`model`、`asset` | 清單略長；今天只有 6 份 `project.json`，未見壓力 |
| D1 資料組 | 一種「組」的 `plugin.json`，成員有序，在 configure 展開；日後是純追加（要升版） | 多一種登記類別；「組內關掉某個」要語法，有 AGENTS §9 設定言語的風險 |
| D2 程式組（Bevy `DefaultPlugins`，`crates/bevy_internal/src/default_plugins.rs:3`） | C++ 函式往 App 加一組 | 違反 `v8-arch-plugin-selection-data` 與 DI 2「main 不手寫 plugin 清單」。不採 |

D1 的觸發條件（提案，寫進 app-assembly 的 Replacement Criteria）：三個以上單位的 E 子清單相同。

### 2.4 需求宣告與兩層 `requires`（F1、F3；Product：全部單位；B1-U11、B1-U13）

**「需要後端」寫在哪（B1-U11）**：
- 建議：寫在 E 的 `plugin.json`，例 `render` 寫 `"needs_backend": ["gpu_device"]`。一個單位「用到能力」＝它選了（含自動補）需要該能力的 plugin。能力名也收在這裡，取代 `adapter-plugins-plan.zh-Hant.md:189` 放在各模組 CMake 的寫法（同一事實一處，AGENTS §9）。
- 效果：S-2 的反例自然解掉——Match 連 `GYO::Render` 函式庫（`apps/object_fps_pvp/CMakeLists.txt:50`、`:66`），但沒選 `render`，所以不需要 GPU 後端。段 (i) 的「CMake 連結圖推導」先驗可以不做；條件條目 5（`requires_capabilities`）消失。
- 代價：這是 review 決定行（`HANDOFF.md:56`）選的「先驗 CMake 推導，不可行回宣告式」之外的**第三條路**，可能被讀成重開已定項，所以交使用者。產品不選 `render` 卻寫用 device 的 System 時，Kernel 報 R-E4、指名缺的型別，不靜默。
- 替代：維持段 (i) 原路線（CMake 推導先驗）；本批的其他提案不依賴這題，只是 S-1 的建議會換（§3）。

**兩層 `requires` 的對應（B1-U13）**：
- 事實：Kernel 層 `Plugin::Requires()`（`Registry.hpp:67`）只檢查順序、不補（spec C.2 第 3 點）；建置層 `plugin.json` 的 `requires` 會自動補。
- **前提（S-2 處置）**：建置層的清單順序怎麼定，**是段 (i) 宣告待定的前提，不是已定**。`adapter-plugins-plan.zh-Hant.md:170` 寫「確定性的拓撲排序，同層以名稱排」，那是 #212 規劃稿的形狀；計畫 §3.11 仍寫「有序清單或依賴宣告，二選一在宣告寫明」（`migration-plan.zh-Hant.md:388`、`:394`）；spec C.6 明寫不採拓撲排序，理由是 fingerprint 會跟著變（`spec.zh-Hant.md:426`）。**依賴這個前提的**：本節的 U13 建議、§2.7 對 F1 的影響分析、§3 S-9 的子題 3。段 (i) 若選「有序清單」，產品就能控制順序，U13 與 §2.7 的說法要跟著改。
- 建議（在拓撲排序的前提下）：`plugin.json` 的 `requires` 是唯一事實；建置產生的登記單元把它填進 Kernel 的 `Requires()`（讓自動補帶入的 plugin 在 Kernel 層也有順序檢查，F1）。R-E3 留作第二道檢查：對產生的清單照理不觸發，對測試手寫的清單仍有效。
- **「手寫的 plugin 類別不覆寫 `Requires()`」的守衛延後**（S-13 處置）：它只依據 AGENTS §9「同一事實一處」，標不出支撐的 Feature，照判定 3 不列入、不估時。觀測到手寫覆寫與 `plugin.json` 不一致的事例再提。
- 替代：兩處各寫、加一個相等檢查。同一事實兩處，AGENTS §9 不建議。
- 與 spec C.6「不重排」的關係：若段 (i) 選拓撲排序，排序在建置層一次決定、產物可讀，Kernel 不重排；建議只在 spec C.6 加註（B1-U16）。

### 2.5 一個行程多個 Runtime 的選擇粒度（F1；Product：Client、Editor 形的工具；B1-U10）

| 選項 | 內容 | 代價 |
|---|---|---|
| **A（建議）** | `roles.<role>.runtimes.<rt>.plugins`；工具 `variants.<v>.runtimes.<rt>.plugins`。Runtime 名由產品取，Kernel 不認得；`main` 以名字取產生的清單 | **必須在段 (i) 的 v2 定形**，否則要升兩次版（計畫 §3.11「不升兩次版」）。產品照 Runtime 分寫，這是 F1「三個 Runtime 各組各的」本身的需要 |
| B | `plugin.json` 宣告自己掛哪種 World（sim／frame／render），產品只寫一份平的清單 | Engine 的登記格式要立「World 種類」的詞；Sim／Frame 的劃分是產品的，不是 Engine 的 |
| C | `main` 用程式分配 | 違反 DI 2 |
| D | Bevy 式「plugin 在 build 裡建子 App」（直接建 SubApp 的位置：`crates/bevy_extract/src/extract_plugin.rs:57`、`:96`） | spec 要加「plugin 建另一個 Runtime」；與 spec C.6「多個 Runtime 的順序由 Host 明寫」衝突 |

**A 動到的已定文字（S-3 處置；全部列成未決，本批不改）**：
- DI 2 增補第一點「選擇以可執行單位（role、variant）為粒度」（`adapter-plugins-plan.zh-Hant.md:280`）→ 改成「以可執行單位內的 Runtime 為粒度」。
- DI 6「每個可執行單位用到的、需要後端的能力恰好有一個已選後端」（`adapter-plugins-plan.zh-Hant.md:282`）→ 見下「恰一個後端」的層。
- Delta-7 第 3 項「依 role／variant」（`migration-plan.zh-Hant.md:806`）。
- 選擇欄單位的說明（`adapter-plugins-plan.zh-Hant.md:150-154`）與計畫 §3.11 差分的 Contract 句（`migration-plan.zh-Hant.md:396`）。

**A 要補的三件事（S-3 處置）**：
1. **工具與 Editor**：工具沿用 `variants.<v>`，下一層也是 `runtimes.<rt>`。Editor 形（target §3b，`runtime-composition.zh-Hant.md:40`）是文件 World（兼 Frame）＋渲染 World，所以 gui 變體寫兩個 Runtime（例 `doc`、`render`）；cli 變體不建 Runtime 時寫空或不寫 `runtimes`（由工具批定）。§7 的工具範例照這個寫。
2. **「恰一個後端」在哪一層檢查**（選項，交使用者，併 B1-U10）：
   - (α)（建議）**單位層**：把一個可執行單位所有 Runtime 的選擇取聯集再檢查。理由：後端是連結進執行檔的 target，一個執行檔只連一次；在 Runtime 層檢查會在「frame 用 SDL 輸入、render 用 SDL GPU」這類正常情況下重複報。
   - (β) Runtime 層：每個 Runtime 各自恰一個。能抓到「某 Runtime 漏選後端」，但後端 target 的連結仍是單位層，兩層語義不一致。
3. **同一 plugin 列在多個 Runtime**：spec R-4（`spec.zh-Hant.md:398`）下，每個 Registry 各跑一次 `Build`。對多 target 的 `sdl`，意思是 frame 與 render 兩個 Registry 各有一個 `sdl` 的 Plugin；它在每個 Registry 該登記哪些能力的部分（輸入、GPU…），取決於「一份 `plugin.json` 多個 target 對應幾個 Kernel Plugin」——這是 §3 S-1 的子題 1（B1-U22）。

### 2.6 plugin 的設定值（F1；Product：Match 與 Client；B1-U14）

事實：同一能力在不同行程設定不同。asset 的快取策略：Match 是空 policy（`PlayerAnimationContent.cpp:94-96`），Client 是 `KeepWhileReferenced, 120…`（`PvpApplication.cpp:102-103`）。GYO 沒有對應 Bevy plugin 設定值的規定（偵察 b「未查」）。

| 選項 | 內容 | 代價 |
|---|---|---|
| **A（建議）** | 設定是 E 登記的 Resource（帶預設值，型別的 owner 是 E）；App 提供「寫初值」的入口，在 World 建立時套用（Startup 之前）；不呼叫就用預設 | App 多一個入口（app-assembly 1～2 h，推定）。時點落在 spec 的 Host 寫入窗口 (1)「Startup schedule 執行前的初始內容」（`spec.zh-Hant.md:140`），不經 Startup，所以不碰 DI 3「Startup 不寫世界內容」的分類問題。設定若屬 (c) 的輸入，G-4（`spec.zh-Hant.md:778`）要求可記錄——asset 的快取策略照理是 zone B（只影響保留多久，不影響模擬結果），由 assets-core 宣告確認 |
| B | 產品在 Startup 的 System 寫入 | 要逐條核對 DI 3／spec H.2b（設定不是世界內容，但沒查證） |
| C | 寫在 `project.json` 的設定欄 | AGENTS §9「任意條件設定言語」的風險 |
| D | plugin 在 Build 讀 App 給的值 | plugin 的 `Build(Registrar&)` 拿不到 App；要改 Kernel 或讓產品碰 plugin 物件（DI 2 守衛衝突） |

**App 存在之前的 Match（S-4 處置）**：E `asset` 在 assets-core 建（match-sim 之前），App 在 app-assembly（G2a 之後）。中間這段：
- **Runtime 路由隨段 (i) 交付**：段 (i) 的產生單元依 `roles.<role>.runtimes.<rt>` 為每個 Runtime 產生一個「以 Runtime 名取 plugin 清單」的函式；match-sim／match-host 的 host 以名字（例 `sim`）取清單、交給 Registry。App 來了以後只是包一層。所以 Runtime 路由的估時從 app-assembly 移到段 (i)（§4）。
- **設定初值**：做法（建議）＝match-sim／match-host 期間，Match 的 host 在同一個 Host 寫入窗口 (1) 直接寫 E `asset` 的設定 Resource（經 World API）；app-assembly 之後改經 App 的「寫初值」入口——同一時點、同一 Resource，只是入口換了。替代：assets-core 讓 E `asset` 的預設值就是 Match 的空 policy，Client 在 client-frame 改寫（今天 Client 的值是在初始化程式裡寫，見 §7 新 8）。兩者交 assets-core 宣告選（併 B1-U14）。

### 2.7 cFingerprint 的缺陷候選與 F1（知悉；由卡 08 處理，已開）

事實（修訂者重核，`sed -n 300,330p engine/kernel/src/Fingerprint.cpp`）：
- `CFingerprintOf` 對 zone-C 的 Component、Resource、Event 寫入**在全體中的絕對 id**（`Fingerprint.cpp:312`、`:318`、`:324` 的 `out.U32(id)`）；System 的存取鍵也寫 id（`:78-83`、`:98-104`）。
- spec 寫 cFingerprint「只含 zone=C 的部分」（`spec.zh-Hant.md:379`；`Registry.hpp:451-453` 註解同義）。所以在 zone-C 項目之前多登記一個非 C 的 Component／Resource／Event，cFingerprint 就變——**實作與 spec 不一致**，是已完成批 registry-schedule（#213）的缺陷候選。J-18（`tests/common/kernel/CoverageTests.cpp:339`、`:496` 兩個案例；`Variant` 只改 zone-B System 的名字，`:318-333`）沒有這個案例，所以沒檢出。
- plugin 名與 `ownerPlugin` **不進** cFingerprint（`Fingerprint.cpp` 對 `ownerPlugin` 0 命中；同一 pattern 在 `Registry.cpp` 命中 7 處，掃描有效）。

處置：**另由卡 08 處理（已開）**——照 AGENTS §14.3 先以失敗測試固定，再修成「zone-C 內序位＋名字」並升版到 3；版本期望值（`CoverageTests.cpp:499` 的 `version == 2U`）的判定變更照 §14.4 交使用者；`SystemBytes` 的存取集合用 id 的部分一併由卡 08 查。cFingerprint 是否涵蓋 Plan 屬卡 01 Q1。**B1 不在這題上選，不把它寫成產品契約，也不提議改 spec H.3 去配合實作。**

對 B1 的影響（只記依賴）：
- F1 讓 Match 與 ClientSim 選不同的 plugin 集合（例：ClientSim 的 runner 不同，`runtime-composition.zh-Hant.md:16`）。卡 08 修好以後，非 C 的登記不再影響 cFingerprint，F1 的自由度不碰 H-4。
- (c) 的登記仍要兩端相同——由「兩端沿用同一個產品 plugin」保證（§2.1〈F2 的適用範圍〉）。
- **批的依賴**：H-4 的產品判定在 client-sim（計畫 §3.21 判定 (2)）。卡 08 必須在 client-sim 的 H-4 判定之前合併；它不是 B1 的批，B1 只把這個依賴寫進 client-sim 與 app-assembly 的差分。
- 若卡 08 的結論是「改 spec 配合實作」，F1 下兩端「選的、會登記型別的 plugin 集合」就必須相同（在拓撲排序下，產品控制不了順序，只能控制集合，§2.4 的前提），與 F1 相抵——那時本稿的 F1 要重看，交使用者。

---

## §3 七題（全部「交使用者」）

### S-3 (2)：產品的 System 經 build 函式登記，與 R3 怎麼對齊（B1-U1）

| 選項 | 代價 | 影響的批 |
|---|---|---|
| **A（建議）App 記錄**（§2.1） | Kernel 只加 App 的入口；spec C.2 第 1 點補一句（B1-U16） | app-assembly（新）；client-frame、工具四批可以不寫產品 plugin；match-sim、client-sim 不受影響（(c) 沿用產品 plugin） |
| B 登記函式寫在選擇欄 | Kernel 0 改；產品多寫一個選擇項 | 段 (i)（欄位）、app-assembly |
| C 維持 | 第 4 條留著；F2 不成立 | 無 |
| D Registry 開第二條路 | registry-schedule 重開、J-19 改 | registry-schedule、world-exec |

**DI 2 守衛的讀法（連帶）**：App 的直接登記不是「plugin 的 build 函式」，守衛照掃。段 (i) 的 DI 2 守衛若以全部 `plugin.json` 為資料來源，E 的 build 函式自然在掃描範圍內（讀取根含 `engine/*/plugins/*/`），不必另寫規則。「產品不得引用 E 的**類別**」那一擴張延後（S-13 處置，§6 第 1 點）。

### S-3 (1)：DI 2 增補第三點「產品與工具的 CMake 不寫 plugin 或後端的 target 名」（B1-U2）

| 選項 | 內容 | 代價 | 影響的批 |
|---|---|---|---|
| **(i)（建議）刪** | 不設這條。DI 6「恰一個後端」只看選擇欄 | **風險**：產品可以在自己的 CMake 直連後端 target（例 `GYO::SdlGpu`）繞過選擇欄，configure 看不到這個後端，DI 6 對它失效。**接受的理由**：DI 6 守的是「選擇欄裡的事實」；直連造成的後果（兩個後端或缺後端）只壞那個產品自己的執行檔，不壞 Engine 或其他產品；review 決定行已刪產品側守衛（`HANDOFF.md:56`），這裡不重新引入 | adapters-relocate（不加守衛）；DI 2 增補落字時刪第三點 |
| (ii) 收窄成可檢查的形 | 產品與工具的 CMake 不寫 `class: backend` 的 target 與 E 的 plugin target；守衛由所有 `plugin.json` 產生**禁止清單**（資料為準） | **張力**：這是掃產品與工具 CMake 的守衛，等於重新引入 review 已刪的產品側守衛（`HANDOFF.md:56`，N4 已刪）。回答 `HANDOFF.md:201` 的兩問——**守哪個性質**：DI 6 不被產品直連後端繞過；**不守誰會壞**：直連的那個產品自己（可能連進兩個後端或漏報缺後端），其他產品與 Engine 不壞。+1～2 h（推定） | adapters-relocate（+1～2） |
| (iii) 維持原文 | — | 與 R3、未決 8 A 衝突（`adapter-plugins-plan.zh-Hant.md:470`），沒有守衛 | 無 |

三個選項都改使用者已核准、尚未落字的 DI 增補（`adapter-plugins-plan.zh-Hant.md:280`），所以交使用者。

### S-1：`sdl_imgui` 的依賴與登記（B1-U3；子題 B1-U22）

| 選項 | 內容 | 代價 | 影響的批 |
|---|---|---|---|
| **(a)（建議；前提是 B1-U11 採「需要後端寫在 `plugin.json`」）** | 任何 plugin（E 或擴展）都能宣告 `needs_backend`；imgui 宣告 `imgui_backend`。sdl 的 `provides` 寫成 target 層級：`"imgui_backend": {"target": "GYO::SdlImgui", "requires": ["imgui"]}`，只有能力被需要時才連，那時 imgui 一定在。這和 E 宣告需要後端是同一個機制（F3 的直接結果） | DI 6 從「Engine 的能力」擴成「plugin 宣告的能力」（§6）；`provides` 驗證放寬為「值必須是某個已知 plugin 宣告過的能力」 | 段 (i)（格式）、adapters-relocate、plugins-editor |
| (b′)（B1-U11 不採時建議） | DI 6 只管 Engine 的能力；sdl 的 `plugin.json` 寫 target 層級的 `targets.sdl_imgui.requires: ["imgui"]`，只有同時選了 sdl 與 imgui 的單位才連 | 兩種檢查機制，F3 的「同一形式」打折；1～3 h（推定） | 段 (i) |
| (c) `sdl_imgui` 拆成獨立 plugin | 推翻已用過一次的 RC (ii)（`adapter-plugins-plan.zh-Hant.md:283`），碰判定 2。只列出，不建議 | — | — |

**target 層級的條件是通例，不只 imgui（S-9 處置）**：B1 之後 `sdl` 的每個能力 target 都對應一個 E——GPU target↔`render`、輸入 target↔`input`、音訊 target↔`audio`、平台 target↔`platform`；`sdl_ttf`↔`text`。條件寫法要對所有 `provides` 項通用。下列三個子題**在段 (i) 宣告前要定**（B1-U22）：
1. **一份 `plugin.json` 多個 target 對應幾個 Kernel `Plugin`**：
   - (x) 一份一個 Plugin（名字 `sdl`），`Build` 依「本 Runtime 被需要的能力」只登記對應部分；要讓產生單元把「本 Runtime 需要哪些能力」傳給 Build。
   - (y) 一個能力 target 一個 Plugin（名字例 `sdl.gpu_device`），各自 Build；名字變多，Registry fingerprint 隨之含這些名字。
   - 與 §2.5 第 3 點（同一 plugin 列在多個 Runtime，R-4 各跑一次 Build）連動。
2. **U13「產生單元填 `Requires()`」在條件式依賴下填什麼**：(x′) 填解析後的結果（產生單元知道最後的選擇，條件已定）；(y′) 條件式的不填，只靠建置層檢查。建議 (x′)：產生單元本來就以最後的選擇為輸入。
3. **`needs_backend` 帶不帶順序**：imgui 需要 sdl 的 `imgui_backend`，而 `sdl_imgui` 又 requires imgui；一個 plugin 只有一個清單位置。
   - (p) 不帶順序：`needs_backend` 只是「選擇裡要有」的約束；順序只來自 `requires`。在 (y) 下 `sdl.imgui_backend` 是獨立 Plugin，排在 imgui 之後，沒有循環。
   - (q) 帶順序（後端在需要者之前）：在 (x) 下 imgui 與 `sdl` 互為前後，形成循環，不可行。
   - 建議 (p)。

**刪 imgui 時的預期**：(a) 下 `imgui_backend` 沒人宣告，sdl 那一項成了「`provides` 寫了未宣告的能力」，configure 報登記資料驗證錯誤並指名 `sdl_imgui`；(b′) 下選了 imgui 的單位得「plugin 不存在」。兩者都不靜默消失；修在 sdl 的 `plugin.json`，不在共通層（AGENTS §7 的明確依賴清理）。

**(iii) 的讀法**：第 1 類附加條「實作 Engine 定的後端介面」讀成「實作被依賴者（Engine 或 plugin）宣告的後端介面」。

**imgui 輸入的來源（S-8 處置；未定，B1-U20）**：官方 SDL3 平台後端的輸入入口吃 SDL 事件，且 RC (ii) 已定不自寫（計畫 `migration-plan.zh-Hant.md:637`，最終檢查 S-1 (5) 已刪「imgui plugin 的輸入讀 `PhysicalInputFrame`」）。所以 imgui **不** `requires: ["input"]`，也不說「由 `PhysicalInputFrame` 餵入」。SDL 事件怎麼經 `sdl_imgui` 進文件 World、與 target §3b「輸入是文件 World 的 System、讀 Engine 的輸入」（`runtime-composition.zh-Hant.md:44`）怎麼對上，交段 (i) 與 plugins-editor 的宣告。

### S-5：依賴者拿到 adapter 露出的第三方型別能做什麼；測試夾具（B1-U4）

| 選項 | 代價 | 影響的批 |
|---|---|---|
| **(b)（建議）依賴者可以呼叫**（照 (iii)、範圍 (α)）；runtime-composition §4〈SDL 家族〉「整個 repo 的 SDL 呼叫只在 sdl plugin」（`:63`）改成「SDL 的對接只在 sdl plugin」（即 RC (ii) 本身的文字） | 升級 SDL 時產品可能跟著改，成本由產品自己負責；可檢索（`git grep SDL_`）；不設產品側守衛 | adapters-relocate（plugin 版標頭衛生守衛的範圍）、plugins-editor、tool-ui（判定 `tools/ui_editor` 無 SDL 含入要改寫，見下） |
| (a) 只能持有與傳遞，全 repo 掃描 | 產品側規則，與「不往產品加步驟」相反；`tests/ui_editor/PreviewAssetTests.cpp` 要改寫 2～4 h（推定） | 工具四批 |
| (c) 產品與工具照 (a)、夾具排除 | 仍是產品側規則；守衛 1～2 h | 同上 |

**測試夾具**：建議**不算**在內（與已定的未決 6 一致，`adapter-plugins-plan.zh-Hant.md:253-258`）；算的話夾具要經 plugin 改寫。

**(b) 下 tool-ui 的判定（N-11 處置）**：計畫 §3.25～3.28 的 tool-ui 判定「`tools/ui_editor` 無 SDL 含入」（`migration-plan.zh-Hant.md:651`）改為「`tools/ui_editor` 不直接連 SDL3 的 target；SDL 型別只經 sdl plugin 的公開標頭」。選 (a) 則維持原文。

### S-8：RC-8 對 B1（B1-U5）

| 選項 | 代價 | 影響的批 |
|---|---|---|
| (a) B1 的 Delta 不算新發現 | RC-8 這個煙霧偵測器變鈍 | 無（只改 §8 文字） |
| (b) 照算，B1 完成時觸發 | 觸發後「回到規劃」就是 B1 自己的修訂，空轉 4～8 h（推定），沒有新資訊；觸發後計數要不要歸零也沒規定 | B1（空轉） |
| **(a′)（建議）** | B1 的 Delta-8 是計畫內的（`migration-plan.zh-Hant.md:702-709` 事先排定、Delta-7 第 6 項 `:809` 早已寫「語義由 B1 定」）；**執行批中發現的、不在 Delta-8 七項之內的 Delta 算第 2 個**，觸發本條。RC-8 條文補一句「計畫內預留的 Delta 不計」 | §8 RC-8 文字；各執行批的宣告要對照 Delta-8 七項 |

### 必答 1：執行批與功能線重開的先後（B1-U6）

| 選項 | 代價 | 影響的批 |
|---|---|---|
| **(i)（建議）拆進 closeout 之前的既有批＋一個機制批**（§4） | 功能線重開前全部完成，與 AGENTS §2「基盤の上に機能を積み上げ始める前に、基盤の反復を済ませる」同方向，不需要解釋例外。wrap-*、client-frame、工具批本來就在重寫同一批初始化點，順手決定「誰登記」 | §4.2 各批、app-assembly |
| (ii) 另開一批，排在 runtime-loop-retire 之後、closeout 之前 | 要重做 wrap-*、client-frame、工具裡已放進 runner 或產品的登記，再重驗所有 host：重工 +25～50 h（推定）；Engine 型別的 owner 改變會讓 Registry fingerprint 與 golden 翻動 | 新批＋wrap-*、client-frame、工具四批重驗 |
| (iii) closeout 之後 | 重工同 (ii)，功能線重開後在舊形狀上積的功能越多越貴；與 AGENTS §2 方向相反 | 同 (ii)＋功能線 |

### 必答 2：RC-11 重判（B1-U7）

- 執行批淨增 +23.5～57 h（推定，§4）。宣告 §7：使用者核准前**不加進** §2.2 合計。
- 核准後：合計 701～1,450 → **724.5～1,507 h**；非條件批 36 → 37。
- 上限超出錨點 1,299 h 共 **208 h**（原 151 h）；觸發點 1,948.5 h 未到，餘裕 **441.5 h**。**不觸發**，但延續「計畫越修越大」，照實報告。
- 若 B1-U11 採「需要後端寫在 `plugin.json`」，段 (i) 的 CMake 推導先驗（計畫 `:159` 的一部分）可省 4～8 h（推定），合計變 **720.5～1,499 h**（條件式）。

| 選項 | 代價 | 影響的批 |
|---|---|---|
| **(a)（建議）** 核准後立即加進 §2.2 並重判 | 無 | §2.2、§2.3、§8 RC-11 |
| (b) 等各執行批的宣告再加 | 重判延後，與 RC-11 原文「B1 完成時加入」（`migration-plan.zh-Hant.md:989`）不一致 | 同上（延後） |

### 必答 3：不借過渡登記表（B1-U8）

| 選項 | 代價 | 影響的批 |
|---|---|---|
| **(a)（建議）** E 從第一天就有自己的 `engine/<module>/plugins/<e>/plugin.json`（`owner: engine`，§1 的位置表）；過渡表拒收 `engine`；closeout 的「0 列」（`:718`）不放寬。前提：段 (i) 的讀取根含 `engine/*/plugins/*/`（+0.5～1 h，推定） | 段 (i) 讀這個位置是實作，不是「預留形狀」——與 §2.2 的三處改寫一起交使用者（S-10）；`gyo_asset_plugin` 與 Q3 字面的關係（§1）一併核准 | 段 (i)、assets-core、wrap-render、wrap-assets、closeout |
| (b) 共通 build 資料另開一張 E 表 | 同一事實兩處（AGENTS §9），違反 F3「同一形式」 | 段 (i) |
| (c) 延到 app-assembly 才讀 `engine/`，期間由產品暫代登記 asset | 重工 1～2 h，加上 Registry fingerprint（`RegistryFingerprintOf`，`Fingerprint.cpp:249-253` 含 plugin 名）的 plugin 名改變要重定基線；Engine 型別的 owner 先落在產品（R-6） | assets-core、match-sim、app-assembly |
| (d) 借過渡表 | 已禁（S-7） | — |

---

## §4 執行批

### 4.1 新批：`v8-arch-app-assembly`（App 的組裝入口）

- **Feature**：F2（Client 的 frame 與 render Runtime、四個工具）；F1、F3 的機制部分（設定初值、owner 分流）。Product：object_fps_pvp 的 Client、四個工具；Match 只用到設定初值的入口（§2.6）。
- **Contract**：§2.1 App 記錄；§2.6 設定初值；§2.4 產生單元填 `Requires()`；§2.2 owner 欄的讀取與刪除驗證的分流。
- **宣告的輸入**：§2.1〈App 記錄的實作細節〉四點（N-9）；S-9 的三個子題（B1-U22）的答案。
- **合成夾具**：合成 E、合成擴展、合成產品登記，放 `tests/common/plugins/fixtures/`（不常駐 `plugins/`，AGENTS §7）。
- **判定草案**（細節由本批宣告定）：
  1. E 與擴展用同一種選擇寫法；owner 只改變刪除驗證的範圍。
  2. App 直接登記的型別，`ownerPlugin`＝App 記錄（單位名）。
  3. 改 `plugin.json` 的 `requires` 而不重產生 → 檢出（突變；只在預期訊息出現時算 killed，AGENTS §14.2）。
  4. 兩個 Runtime 得到兩個不同的 Registry。
  5. Match 形狀的合成單位，Registry 沒有 render、input、ui 的記錄（F1 的合成證據；真實 Match 的證據在 client-frame，§4.2）。
  6. 合成：兩個 Runtime 的非 C 登記不同、(c) 登記相同時，cFingerprint 相同（F1 的自由度不碰 H-4）。**前提是卡 08 已合併**；未合併時本判定不判，寫進範囲の限界。
- **依賴**：world-exec（`App`）、plugins-foundation 段 (i)（含 Runtime 路由）。**被依賴**：client-frame、plugins-editor、工具四批。**不被** match-sim、plugins-frame 依賴（match-sim：(c) 沿用產品 plugin；plugins-frame：只加判定，S-5 處置）。
- **排程**：**架構線**（N-8 處置：本批改 Kernel 公開介面，對齊 Kernel 批的慣例——計畫 §2.1 的 Kernel 批都在架構線、Opus／xhigh）。world-exec 與段 (i) 之後可開，必須在 client-frame 開工前完成。不在關鍵路徑：它最晚在 G2a 附近開，到 client-frame 開工前隔著 match-host 與 client-sim（≥125.5 h），本批 8.5～17 h。
- **段**：上段（Kernel 公開介面、Product Registration）。**模型／檔位**：實作 Opus／xhigh；審查 Fable／xhigh；驗證 Opus／high（navigator 上段的下限；交使用者確認）。
- **估時（推定）**：App 記錄 2～4＋設定初值 1～2＋產生單元填 `Requires()` 0.5～1＋owner 分流 2～4＋合成夾具與突變 2～4＋守衛去處判斷 1～2（§3 共通）＝**8.5～17 h**。出處：提案③（`propose-cost.md`）的細算，去掉「E 的產生登記」（移到段 (i)）。相對統合稿的 11～24 h：Runtime 路由 1～3 移到段 (i)（S-4）；`requires` 單一事實由 1～3 改為只做產生單元填 0.5～1，「手寫類別不覆寫」守衛延後（S-13）；DI 2 守衛擴張 1～2 延後（S-13）。宣告輸入的四點（N-9）若需要額外工時，由本批宣告重估。
- **停止條件**：App 記錄需要改 `Registry` 本體；owner 欄必須影響選擇才成立（F3 不成立）；任何模組需要改歸屬或 owner。
- **Replacement Criteria（寫在 App 的 DI 前）**：三個以上單位的 E 子清單相同 → 提 D1 資料組；產品因 App 記錄排在末尾而無法表達需要的順序（含 §2.1 第 3、4 點的 `InsertStage` 與 Finish 限制）→ 停，重看 F2 的形狀。
- **若失敗**：產品照舊包成產品 plugin（C），〈仍往產品加步驟〉第 4 條留著；其餘差分不受影響。

### 4.2 既有批的差分（全部推定）

| 批 | 差分 | +h | 支撐 |
|---|---|---|---|
| plugins-foundation 段 (i) | `owner` 欄值域與三條邊界檢查、`class` 只對 extension；讀取根含 `engine/*/plugins/*/`；`roles.<role>.runtimes.<rt>`／`variants.<v>.runtimes.<rt>` 粒度；**Runtime 路由**（產生單元以 Runtime 名給清單，S-4）；`needs_backend`（看 B1-U11、U3）；target 層級的條件連結與 S-9 三子題（B1-U22）。宣告要明寫 B1 的答案是它的輸入 | +3～8（統合稿 +2～5，加 Runtime 路由 1～3；條件：U11 採用時 −4～8） | F1、F3 |
| assets-core（關鍵路徑） | E `asset`（含內建 loader）、E `model`（`Assets<ModelAsset>` 的 owner 在 Engine）；`ufbx` 的 `requires` 加 `model`；`gyo_asset_plugin` 只放 build 函式（Q3 字面待核准，B1-U8）；設定初值在 App 之前的做法（§2.6） | +2～5 | F1、F3 |
| match-host（關鍵路徑） | 只有文字：`match_main` 以 Runtime 名（`sim`）取段 (i) 產生的清單。統合稿加的判定「真實 Match 的 Registry 不含 render／input／ui／audio」**移到 client-frame**（那時這些 E 才存在，S-4） | 0 | F1 |
| adapters-relocate | S-5 (b) 的標頭衛生守衛範圍改寫。S-3 (1) 選 (i)（建議）時不加守衛；選 (ii) 時 +1～2 | +0～1 | F1、F3 |
| wrap-render | E `render`、`input`、`audio`、`text`、`platform`（中性契約）的 plugin 目錄、target 與 `plugin.json`；sdl 各 target 的 `provides` 對上；每個 E 的刪除驗證（刪了選它的單位得 configure 錯誤或 R-E4） | +5～10 | F1、F3 |
| wrap-assets | E `ui`、`ui_render`、`model_render`；刪除驗證 | +2～5 | F1 |
| plugins-frame | 判定加：window-runner 不代登 Engine 的型別（`ownerPlugin` 檢查）。**不依賴 app-assembly**（S-5） | 0 | F3 |
| client-sim（關鍵路徑） | ClientSim Runtime 的選擇列（`roles.main.runtimes.<sim>`）；H-4（既有判定 (2)）以真清單判，**前提卡 08 已合併**（§2.7） | +0.5～1 | F1 |
| client-frame（關鍵路徑） | 三個 Runtime 各自的清單；`PvpApplication.cpp:934-1093` 的手寫初始化改由清單與 build／finish 承擔；產品 System 經 App 記錄；**判定（自 match-host 移入）：真實 Match 的 Registry 不含 render、input、ui、audio 的記錄**——正例先命中：同一掃描對 Client 的 render Runtime 的 Registry 命中 `render` 的記錄；依賴加 app-assembly | +1.5～3（統合稿 +1～2，加移入的 0.5～1） | F1、F2 |
| plugins-editor、工具四批 | 工具專有的編輯 System 改為直接登記或工具自選包成 plugin（取代 `:648` 的「進該工具自己的 plugin」）；`variants.<v>.runtimes.<rt>`；imgui 的輸入來源依 B1-U20（統合稿的「imgui 的 `requires` 加 `input`」刪，S-8）；依賴加 app-assembly | +0～5 | F2 |
| framework-delete | 無（過渡表 0 列不變） | 0 | — |
| closeout（關鍵路徑） | 掃描：Engine 型別的 `ownerPlugin` 只能是它的 E；`owner` 與路徑一致；過渡表 `owner: engine` 0 列 | +1～2 | F3 |

**算式**：
- 差分下限 3＋2＋0＋0＋5＋2＋0＋0.5＋1.5＋0＋0＋1＝**15**；上限 8＋5＋0＋1＋10＋5＋0＋1＋3＋5＋0＋2＝**40**。
- 新批 8.5～17。淨增 15＋8.5＝**23.5**～40＋17＝**57 h**（推定）。
- 合計（若核准）：701＋23.5＝**724.5**～1,450＋57＝**1,507 h**。U11 採用時再減段 (i) 的 4～8：**720.5～1,499 h**。
- 超錨點：1,507−1,299＝**208 h**；RC-11 餘裕：1,948.5−1,507＝**441.5 h**。

三案的對照：①33～66、②15～36、③23～55（`propose-risk.md`、`propose-freedom.md`、`propose-cost.md` 的估時節）。統合稿取各列的中間值並把 ① 的 sweep 批拆進各 wrap 批與 closeout，得 26～62；本修訂照 S-4、S-7、S-13 移動與延後後為 23.5～57。

### 4.3 排程

- **B1 本身**：照已核准宣告 §8，在 plugins-foundation 段 (i) 的**事前宣告之前**完成（S-1、S-3、S-5 是它的輸入）。計畫 `:132`（階段 5）、`:184`「依賴段 (i)」、`:705`「開工依賴段 (i)」都和宣告相反，要改。
- **關鍵路徑**（S-1 處置）：路徑上的批是 a6-revision → registry-schedule → world-exec → assets-core → match-sim → first-host-cost → match-host → client-sim → client-frame → framework-delete → runtime-loop-retire → closeout（`migration-plan.zh-Hant.md:194-196`）。B1 在路徑上的差分：assets-core +2～5、match-host 0、client-sim +0.5～1、client-frame +1.5～3、closeout +1～2，合計 +5～11。
  - 333＋5＝**338**；696＋11＝**707 h**。
  - M3（到 first-host-cost）：116＋2＝**118**～249＋5＝**254 h**。
  - M4（到 match-host）：182＋2＝**184**～391＋5＝**396 h**（match-host 的判定移到 client-frame 後，match-host 的差分是 0；若照統合稿留在 match-host，則 184.5～397）。
- **client-frame 對 app-assembly 的新依賴為何不進路徑**：app-assembly 在架構線，world-exec 與段 (i) 之後就能開；client-frame 的開工要等 client-sim（G3），而 G2a → match-host → client-sim 至少 125.5 h。app-assembly 8.5～17 h 遠短於這段，所以不改路徑。
- **plugins-frame 不依賴 app-assembly**（S-5 處置）：只加判定。階段 2′ 的鏈（json-adapter → adapters-relocate → wrap-render → wrap-assets → plugins-frame）：91～204＋(0～1)＋(5～10)＋(2～5)＋0＝**98～220 h**，仍短於 G2a → match-host → client-sim 的 125.5～261 h（125～260＋match-host 0＋client-sim 0.5～1）。若 app-assembly 改放同一條 Engine／plugins 線串行，鏈變 106.5～237 h，餘裕約 19～24 h（本稿不這樣排）。
- **工具線**：match-host → plugins-editor → tool-voxel → tool-ui 85～168 h，加工具差分 0～5 後最多 85～173 h，短於 client-sim → client-frame 127～254 h（125～250＋0.5～1＋1.5～3）。
- **段 (i) 的餘裕**：段 (i) 17～36 → **20～44 h**（若核准），仍短於 registry-schedule → world-exec 25～50 h。
- list scheduling 的瓶頸（`:212`「未重算」）本稿也沒重算。
- 全部在 closeout 之前；功能線照原計畫在 closeout 後重開。

---

## §5 新 Delta（暫名 Delta-8）的七項（AGENTS §3）

1. **Feature**：F1～F3（宣告 §1）。Product：object_fps_pvp 的 Match 與 Client、四個工具（F2 的直接登記只交付給 Client 的 frame／render 與四個工具，§2.1）。
2. **現在的問題**：
   - Engine 能力的初始化由 Host 或產品手寫，順序寫在程式裡：shaders → platform → device → renderer → text → uiRenderer → input（`PvpApplication.cpp:934-1093`）；工具在 `framework/src/WindowShell.cpp:47-63`、`ImGuiShell.cpp:381-458`；Match 在 `PlayerAnimationContent.cpp:82-96`。
   - 計畫的 wrap-* 會把能力改成 Resource，但沒寫由誰登記；結果會由 runner 或產品代登，Engine 型別的 owner 記錄散在各處（R-6）。
   - Kernel 沒有不經 plugin 的登記路徑（`Registry.hpp:275`、`Registry.cpp:1299-1300`），與 R3 不一致。
   - 「用到能力」只能從連結圖推，Match 連 `GYO::Render`（`apps/object_fps_pvp/CMakeLists.txt:50`、`:66`）是反例。
   - owner 欄只預留形狀（`adapter-plugins-plan.zh-Hant.md:184`）。
3. **邊界**：10 個 E（7 模組＋衍生 2＋中性 platform 契約 1）各多一個 plugin 目錄與 target；App 多一個直接登記入口；登記資料的 `owner` 欄取得語義；選擇欄多 Runtime 層；spec C.2 第 1 點補一條路（未決）。
4. **影響**：`engine/{asset,model,audio,input,render,text,ui,platform}`（各加 `plugins/<e>/`）；`engine/kernel`（只有 App）；`build/cmake`（`GyoApps.cmake`、`GyoTools.cmake`、`GyoToolRegistry.cmake`、`GyoComponents.cmake` 與產生的登記單元）；`plugins/{sdl,sdl_image,sdl_ttf,ufbx,imgui,window_runner,headless_runner}` 的 `requires`／`provides`；6 份 `project.json`；`tests/common/{plugins/fixtures,kernel,asset,model,audio,input,render,ui}`；`docs/architecture/<m>`、`docs/plugins`。
5. **Dependency**：新邊 `gyo_<e>_plugin` → 該 E 的核心庫、`GYO::Kernel`（同在 Engine 內，往下）；能力的核心庫 target 本身不加這條邊（asset 除外，已由 assets-core 定）。`plugins/` → E（資料上的 `requires`），方向不變；E → `owner: extension` 禁止並機械檢查。Kernel 不依賴任何 E（`gyo_expect_kernel_links`，`tests/common/kernel/CMakeLists.txt:195`，不變）。
6. **Ownership**：模組 owner 不變（仍是 Engine）；第 13、15、17 列的 `plugins/` 歸屬不變；內建 loader 仍歸 asset（§1）。改變的是**初始化的責任**：從 `PvpApplication::Impl`、`ImGuiShell`、`WindowShell`、`PlayerAnimationContent` 移到各 E 的 build／finish；Engine 型別的登記者從「runner／產品（wrap-* 的現行讀法）」定為各自的 E。屬「複数 Module にまたがる Responsibility 移動」。
7. **更小的方案與不採的理由**：
   - 只做 F2（App 記錄）：F1、F3 不成立，Engine 能力仍由 Host 手寫。
   - 只預留形狀、不設 E：需要後端仍要靠連結圖推，S-2 的反例無解；runner 代登讓 F1 由 runner 決定。
   - 一個 `engine` 大 plugin：Match 會得到 render，F1 不成立。
   - build 函式放進核心 target：連結守衛要翻動（緩める面），函式庫使用者被迫連 Kernel。
   - closeout 之後一次搬完：重工 25～50 h，與 AGENTS §2 方向相反。
   - 全部 23 個模組都成 plugin：空 plugin，產品呼叫函式前要先選 plugin，比本案大（B1-U9）。

**這個 Delta 動到的已定文字（全部列成未決，本批不改）**：
- spec C.2 第 1 點（App 記錄）、spec C.6（加註）、spec `:376`「digestSet 由產品 plugin 宣告」與 `:846`「同一組 (c) plugin」的讀法（B1-U16）。
- DI 2 的讀法；DI 2 增補第一點「以可執行單位為粒度」（`adapter-plugins-plan.zh-Hant.md:280`，B1-U10）與第三點（B1-U2）。
- DI 6 的範圍（`adapter-plugins-plan.zh-Hant.md:282`；B1-U3、B1-U10 的「恰一個後端」的層）。
- Delta-7 第 3 項「依 role／variant」（`migration-plan.zh-Hant.md:806`）、選擇欄單位的說明（`adapter-plugins-plan.zh-Hant.md:150-154`）、計畫 §3.11 差分（`migration-plan.zh-Hant.md:396`）（B1-U10）。
- 「預留形狀、不實作」三處：`HANDOFF.md:56` 決定行 review 的 B1 條、`migration-plan.zh-Hant.md:396`、`adapter-plugins-plan.zh-Hant.md:184`（B1-U8、B1-U12）。
- 計畫 §3.19 Q3 字面「v1 不另開 CMake target」（`migration-plan.zh-Hant.md:551`；B1-U8）。
- `adapter-plugins-plan.zh-Hant.md:189`（能力宣告的位置）；段 (i) 的先驗路線（若 B1-U11 採用）。
- runtime-composition §4〈SDL 家族〉一句（`:63`；B1-U4）。
- RC-8 照 B1-U5 處理。

---

## §6 對 target DI 與 spec 的提案（不改原文；以卡 03 落地後的條文為準，衝突交架構師）

1. **runtime-composition DI 2**（`:74`）：
   - 「plugin 選擇只寫在 `project.json`」補一句：「選擇的對象是 E 與擴展；產品自己的登記（App 直接登記，或產品自選的產品 plugin）是程式，不是選擇。」
   - 守衛「不直接引用 plugin 的 build 函式」：App 的直接登記不在內；E 的 build 函式因讀取根含 `engine/*/plugins/*/` 而自然在內。「產品不得直接引用 E 的**類別**」**延後**（S-13 處置：標不出支撐的 Feature；觀測到產品引用 E 類別造成換 owner 要改產品的事例再提）。
   - 「Engine 不 include 任何 plugin」補「E 不 `requires` `owner: extension` 的 plugin」。
2. **DI 2 增補**（未落字，`adapter-plugins-plan.zh-Hant.md:280`）：第一點改成以 Runtime 為粒度（B1-U10）；第三點照 B1-U2 (i) 刪。
3. **DI 6**（未落字，`:282`）：「需要後端的能力」改為「E 或擴展在登記資料宣告的需要後端的能力」（B1-U3 (a)）；檢查的層照 B1-U10 (α) 在單位層。
4. **新 DI（建議）**：「Engine 能力的型別（Resource、資產型別、中性 boundary）只由該能力的 E 登記；Host、runner、後端、產品不代登。」支撐：F3（換後端時型別 owner 不變）。守衛：closeout 的 `ownerPlugin` 掃描（`Registry.hpp:369`、`:382`、`:389`、`:401`）。放 runtime-composition 還是 `docs/architecture/README.md` 的 Engine DI，交架構師。
5. **「E 與核心庫分 target」不提為新 DI**（S-13 處置）：它是實作選擇（§1 的理由是守衛不翻動、退回便宜），標不出支撐的 Feature，照判定 3 不列入。若日後觀測到有人把 build 函式放進核心 target 造成連結守衛翻動，再提。
6. **§4 本文**：「System 本身的登記仍是程式，由 plugin 的 build 函式向 Registry 登記」（`:54`）改成「由 plugin 的 build 函式或 App 的直接登記」；「plugin 分兩類」（`:61`）補 `owner` 欄的三個值。
7. **§4〈SDL 家族〉**（`:63`）：「整個 repo 的 SDL 呼叫只在 `sdl` plugin」改成「SDL 的對接只在 `sdl` plugin；依賴者可以使用露出的 SDL 型別與呼叫，升級自負」（B1-U4 (b)）。
8. **§3b**：imgui 輸入的讀法未定（B1-U20）；不提「imgui 讀 `PhysicalInputFrame`」。
9. **runner 的出處措辭**（spec C.2 第 4 點 `spec.zh-Hant.md:387`、runtime-composition `:51`）：「照 Bevy `WinitPlugin::set_runner`」不精確——Bevy 在 build 裡裝、最後一個生效、不報錯（`crates/bevy_app/src/app.rs:224-227`、`crates/bevy_winit/src/lib.rs:158`）。建議改成「形式參考 Bevy 的 set_runner；GYO 在 finish 安裝、唯一（R-E6）」。語義不動。
10. **spec C.2 第 1 點**：補 App 記錄（B1-U16）。**spec C.6**：段 (i) 若選拓撲排序，加註「排序在建置層一次決定，Kernel 不重排」；證據欄的 `bevy_core:N` 可換成檔:行（偵察 b）。**spec `:376`、`:846`**：「由產品 plugin 宣告」「同一組 (c) plugin」在 App 記錄之下的讀法——(c) 仍由產品 plugin 登記（§2.1），digestSet 的宣告者是登記 (c) 的那個 plugin；寫明即可（B1-U16）。**spec H.3 不提議改**：cFingerprint 與 spec 的不一致由卡 08 處理（§2.7）。
11. **DI-KS 15**（`target/kernel.zh-Hant.md:71-72`）與 spec R-7：E 的名字與 App 記錄名會以資料進 Registry 與 Registry fingerprint（`RegistryFingerprintOf`，`Fingerprint.cpp:249-253`）。建議釐清「不含能力名字」指 Kernel 的程式與內建內容，不指 plugin 提供的資料（B1-U17）。
12. DI 1、3、4 不動。

---

## §7 〈仍往產品加步驟的條目〉前後對照（判定 6）

| # | 條目（`adapter-plugins-plan.zh-Hant.md:462-476`） | 提案後 | 理由與 Feature |
|---|---|---|---|
| 1 | 在選擇欄寫要用的 plugin | **留，按 Runtime 寫**（B1-U10 A）。E 多半由自動補帶入：產品只寫自己直接用到的 plugin（例：Client 的 render Runtime 寫 `model_render`、`ui_render`，`render`、`text`、`model`、`asset` 由 `requires` 帶入） | F1 本身就要求（「三個 Runtime 各組各的」） |
| 2 | 產品連外部組件經對接它的 plugin（R3） | 留 | 使用者已定 |
| 3 | 產品 CMake 不寫 target 名 | **刪**（B1-U2 (i) 建議）；選 (ii) 則留，且從「沒有守衛、closeout 不掃」（`adapter-plugins-plan.zh-Hant.md:470`）變成有掃描——對產品是**加強**，交使用者 | (i)：DI 6 只看選擇欄；(ii)：守 DI 6 不被直連繞過，不是 F1～F3 本身需要 |
| 4 | 產品 System 要經 plugin build 函式登記 | **刪**（B1-U1 A；Match 與 ClientSim 的 (c) 照舊用產品 plugin，那是既有寫法，不是新步驟） | F2 |
| 5 | 回宣告式時寫 `requires_capabilities` | **刪**（B1-U11 採用時；不採時維持「條件」） | F1 |
| 6 | 用 `shader_bundles` 就要選提供編譯器的 plugin | 留 | 未決 19 (b) 已定 |
| 7 | 工具守衛正向清單逐標頭核准（過渡） | 留（過渡期限不變）；E 的標頭不進產品，不增負擔 | 工具四批定 |
| 新 8 | 某能力要非預設設定時，呼叫 App 的「寫初值」入口（App 之前：Host 在寫入窗口 (1) 寫） | **新增（條件）**：只在要非預設值時才寫 | F1：Match 與 Client 的 asset 快取策略本來就不同（`PlayerAnimationContent.cpp:94-96` 對 `PvpApplication.cpp:102-103`）；今天是在初始化程式裡寫，換入口，不是新負擔 |
| 新 9 | 產品 `main` 以 Runtime 名取產生的清單 | **新增** | F1 本身需要：一個行程多個 Runtime 時，main 要說哪個清單給哪個 Runtime；今天的 main 是手寫初始化，這是替代它的那一行 |
| 新 10 | 產品自選包成 plugin 時，產品目錄下的 `plugin.json` 寫 `owner: product` | **新增（條件）**：只在產品選擇包成 plugin 時 | 不是 F1～F3 本身需要（F3 要的是 E 與擴展可互換）；**交使用者**：替代是產品目錄下可省略 `owner`、configure 視為 `product`——但那是從位置推 owner，碰 AGENTS §10 |

統合稿列的另兩條產品側規則**不列入**（S-13 處置，延後、不實作）：「手寫 plugin 類別不得覆寫 `Requires()`」（§2.4）、「產品不得引用 E 的類別」（§6 第 1 點）。統合稿的「新 9 sim 清單前段一致」刪（§2.7，由卡 08 處理）。

**列數**：照建議 **7 → 7**（刪 3、4、5；留 1、2、6、7；加 8、9、10）。若 B1-U2 選 (ii)：8；若 B1-U11 不採：8（5 留條件）；若新 10 改為可省略：6。

### F1～F3 對各單位實際可見的效果（名字都是示意）

- **Match**：`roles.match.runtimes.sim.plugins` 寫 `headless_runner`、`ufbx`、`net_link`、`trace_diagnostics`；`asset`、`model` 由 `ufbx` 的 `requires` 帶入。(c) System 照舊由產品 plugin 登記（與 ClientSim 同一份）。沒選 `render`，就不需要 GPU 後端，雖然連著 `GYO::Render`。
- **Client**：三個 Runtime 各一份清單——frame：`window_runner`、`ui`、`audio`（`platform`、`input` 等由 runner 與 `ui` 的 `requires` 帶入，實際帶入以 wrap-render 宣告的 `requires` 為準）；render：`model_render`、`ui_render`（`render`、`text`、`model`、`asset` 帶入）；sim：ClientSim 的 runner 與 (c) 的產品 plugin——與 Match 的 (c) 同一份；runner 不同，卡 08 修好後不影響 H-4（§2.7）。後端寫 `sdl`、`sdl_image`、`sdl_ttf`，在單位層檢查「恰一個」（§2.5 (α)）。
- **四個工具**：gui 變體寫兩個 Runtime（例 `doc`、`render`）：`doc` 寫 runner、`imgui` 與用到的編輯器 plugin；`render` 寫 offscreen-preview 等；E 的帶入以各 plugin 的 `requires` 為準（imgui 的輸入來源未定，B1-U20）。工具專有的編輯 System 直接登記。audio_editor 的 cli 離線混音不選任何 plugin，把 `Mixer` 當函式庫用；model_viewer 的 cli 要不要 App 由工具批定。
- **F3**：某能力日後改由外部維護時，只搬 `plugin.json` 與程式、改 `owner` 值；所有 `project.json` 0 行改動。

---

## 未決（交使用者）

| # | 題 | 選項（代價） | 建議 |
|---|---|---|---|
| B1-U1 | S-3 (2) App 的組裝入口 | A App 記錄（Kernel 加 App 入口、spec C.2 補句；Match／ClientSim 的 (c) 不改）／B 登記函式寫在選擇欄（Kernel 0 改、產品多一步）／C 維持（F2 不成立）／D Registry 第二條路（registry-schedule 重開） | A |
| B1-U2 | S-3 (1) DI 2 增補第三點 | (i) 刪（產品直連後端可繞過 DI 6，後果只在該產品）／(ii) 收窄為禁 backend 與 E 的 plugin target、由資料產生禁止清單（+1～2 h；重新引入產品側守衛，與 review 決定行有張力）／(iii) 維持（與 R3 衝突、無守衛） | (i) |
| B1-U3 | S-1 `sdl_imgui` | (a) plugin 也能宣告需要後端＋`provides` 的 target 層級條件（DI 6 擴範圍）／(b′) DI 6 只管 Engine、target 層級 `requires`（兩種機制）／(c) 拆 plugin（推翻 RC (ii)） | U11 採用時 (a)；否則 (b′) |
| B1-U4 | S-5 露出型別與夾具 | (b) 可呼叫、改 `:63` 一句、tool-ui 判定改寫／(a) 只持有、全 repo 掃描（產品側規則、2～4 h 改測試）／(c) 產品 (a)、夾具排除；夾具 (x) 不算／(y) 算 | (b)＋(x) |
| B1-U5 | S-8 RC-8 | (a) 不算（偵測變鈍）／(b) 照算（空轉 4～8 h）／(a′) Delta-8 不算、執行批新發現的照算 | (a′) |
| B1-U6 | 必答 1 先後 | (i) 拆進 closeout 前的既有批／(ii) 另批在 closeout 前（重工 25～50 h）／(iii) closeout 後（違反 §2 方向） | (i) |
| B1-U7 | 必答 2 RC-11 | (a) 核准後立即加進 §2.2 重判（724.5～1,507 h，超錨點 208 h，不觸發）／(b) 等各批宣告 | (a) |
| B1-U8 | 必答 3 過渡登記表；E 的登記資料位置；Q3 字面 | (a) `engine/<module>/plugins/<e>/plugin.json`（每個 E 一個目錄）＋段 (i) 讀取根含 `engine/*/plugins/*/`＋過渡表拒收 `engine`；同時改寫「預留形狀、不實作」三處（`HANDOFF.md:56`、`migration-plan.zh-Hant.md:396`、`adapter-plugins-plan.zh-Hant.md:184`）與 Q3 字面（`migration-plan.zh-Hant.md:551`）／一檔多 plugin（兩種格式）／(b) 另一張 E 表／(c) 產品暫代登記（重工＋Registry fingerprint 基線重定）／(d) 借過渡表（已禁） | (a)＋每個 E 一個目錄 |
| B1-U9 | 「函式庫」類不成 plugin | A 只有有登記物的模組成 E（7 個）／B 照字面全部成 plugin（空 plugin，產品呼叫函式前要先選） | A |
| B1-U10 | 多 Runtime 的選擇粒度 | A `roles.<role>.runtimes.<rt>`、`variants.<v>.runtimes.<rt>`（段 (i) v2 必須定；改 DI 2 增補第一點、DI 6、Delta-7 第 3 項、AP:150-154、MP:396）；「恰一個後端」在 (α) 單位層／(β) Runtime 層／B plugin 宣告 World 種類／C main 分配（違反 DI 2）／D plugin 建子 App | A＋(α) |
| B1-U11 | 「需要後端」的宣告位置 | A 寫在 E 的 `plugin.json`、能力使用由選擇推導（取代段 (i) 的 CMake 先驗，省 4～8 h，是第三條路）／B 維持段 (i) 原路線（CMake 推導先驗，不可行回宣告式） | A |
| B1-U12 | owner 欄的值域與 runner 類 plugin 的 owner | 值域 `engine`／`extension`／`product`＋三條邊界檢查＋`class` 只對 extension；runner 與服務類照目錄填 `extension`（重新分類＝改 owner，本批不做）；「預留形狀、不實作」三處改寫（同 U8）／替代：不設欄（違反 §10）、CMake 宣告（兩種格式） | 三值＋照目錄 |
| B1-U13 | 兩層 `requires` | A `plugin.json` 唯一、產生單元填 `Requires()`（「手寫不覆寫」守衛延後）／B 兩處各寫＋相等檢查。前提：段 (i) 選拓撲排序（未定，§2.4） | A |
| B1-U14 | plugin 設定值；App 之前的 Match | A E 的設定 Resource＋App 寫初值（World 建立時、Startup 前，Host 寫入窗口 (1)）／B Startup System 寫（DI 3 未核）／C `project.json` 欄（設定言語）／D plugin 讀 App（改 Kernel）。App 之前：Host 在寫入窗口 (1) 直接寫／或 E 預設值取 Match 的值、Client 改寫（交 assets-core 宣告選） | A；App 之前 Host 直接寫 |
| B1-U15 | cFingerprint 用絕對 id（知悉） | 不選：由卡 08 處理（已開，§2.7）。B1 只記依賴：卡 08 在 client-sim 的 H-4 判定前合併；app-assembly 判定 6 以它為前提 | 知悉 |
| B1-U16 | spec 改字（本批不改） | spec C.2 第 1 點補 App 記錄；C.6 加註建置層排序（段 (i) 選拓撲排序時）；`:376`、`:846` 在 App 記錄下的讀法；C.2 第 4 點與 runtime-composition `:51` 的 Bevy 出處措辭／或維持原文 | 隨 Delta-8 一起改 |
| B1-U17 | DI-KS 15／R-7 的讀法 | 「不含能力名字」指 Kernel 的程式與內建內容（plugin 名作為資料可以進 Registry）／或更嚴（E 名不得進 Registry，則 E 不可行） | 前者；交架構師 |
| B1-U18 | 跨 Runtime 的依賴（`ui_render` 在 render Registry，依賴 frame 側的 `ui`） | A 只在建置層（單位層）檢查＋Extract 源缺型別時由 Kernel 報錯（是否已由 Extract 的既有錯誤涵蓋未查證）／B Kernel 加跨 Registry 的 requires（Kernel 改動大） | A，由 rollback-extract 與 wrap-assets 宣告確認涵蓋 |
| B1-U19 | text 與中性 platform 契約成為 E | A 成為 E（由「Engine 型別只由 E 登記」推出；字型資產與 loader 仍歸 asset）／B text 留函式庫、槽由 sdl_ttf 登記；platform 契約只是介面（owner 隨後端變，F3 打折） | A；最終形狀由 wrap-render 宣告定 |
| B1-U20 | imgui 輸入的來源（S-8） | 官方 SDL3 後端吃 SDL 事件；與 target §3b「讀 Engine 的輸入」怎麼對上：(a) `sdl_imgui` 在文件 World 的 System 裡把 SDL 事件交給 ImGui（§3b 讀成「讀後端交來的輸入」）／(b) 經 E `input` 轉換後餵 ImGui（要自寫轉接，碰 RC (ii) 的「不自寫」） | 不給建議；交段 (i)／plugins-editor 宣告查證後定 |
| B1-U21 | 預設組 | D0 不設（靠自動補依賴）／D1 資料組（新登記類別、設定言語風險）／D2 程式組（違反 DI 2，不採） | D0；觸發條件寫進 app-assembly 的 RC |
| B1-U22 | S-9 的三子題（段 (i) 宣告前要定） | 1 一份 `plugin.json` 多 target：(x) 一個 Plugin 依需要登記／(y) 每個能力 target 一個 Plugin；2 條件依賴下填 `Requires()`：(x′) 填解析後的結果／(y′) 不填；3 `needs_backend` 帶不帶順序：(p) 不帶／(q) 帶（在 (x) 下與 `sdl_imgui` 形成循環） | 1 未定（交段 (i)）；2 (x′)；3 (p) |

---

## 三案比較與取法

| 項目 | ①風險與可逆 | ②產品自由度 | ③成本 | 本稿取法 |
|---|---|---|---|---|
| E 的範圍 | 6 模組／8 plugin；text 是函式庫 | 7 模組＋衍生；platform 契約為 E | 7（text 條件）＋platform 契約 | ②③：7＋衍生；text、platform 由「型別只由 E 登記」推出；字型資產與 loader 留 asset（S-12） |
| math | 基礎 | 函式庫 | 基礎 | 函式庫（RC `:56` 末句明列留 Engine；不在第一問例舉；不改結論） |
| F2 | App 末尾合成 plugin，Registry 不改 | App 直接登記＋隱含 owner 記錄；產品 Plugin 物件可在程式加入 | 登記函式寫在選擇欄 | ① 的機制＋② 的說法；不採 ② 的「程式加入 Plugin 物件」；Match／ClientSim 的 (c) 不改（S-4） |
| owner 欄 | engine／extension／product | engine／plugins＋class capability＋三條檢查 | engine 或省略＋class capability | ① 的三值＋② 的三條檢查；`class` 只對 extension（N-6，不加 capability） |
| E 的 target | 另立 `gyo_<m>_plugin` | 獨立小 target | 子 target | 一致採用；每個 E 一個目錄（S-11）；asset 與 Q3 字面待核准 |
| 需要後端 | 未動段 (i) 路線 | 寫在 E 的 `plugin.json`（第三條路） | 同 ②，條件式 | ②③，交使用者（U11） |
| 兩層 requires | 相等檢查 | `plugin.json` 唯一、產生單元填 | 同 ② | ②③；建置層排序是段 (i) 待定的前提（S-2）；「手寫不覆寫」守衛延後（S-13） |
| Runtime 粒度 | plugin 宣告 World 種類 | 每 Runtime 一份清單 | 同 ② | ②③（World 種類是產品的劃分）；補工具、Editor、後端檢查層（S-3） |
| 設定值 | Startup 寫 Resource | `app.Configure<T>`，plugin 在 Build 讀 | Resource＋寫初值入口 | ③（不經 Startup，也不必讓 plugin 讀 App）；App 之前 Host 直接寫（S-4） |
| cFingerprint | 指出絕對 id 問題（N-7） | 認為不影響（只讀檔頭） | 未談 | ① 的事實屬實；定性為已完成批的缺陷候選，由卡 08 處理（B-1） |
| S-1 | 能力由 Engine 或 plugin 宣告 | 同①＋`provides` target 層級條件 | target 層級 `requires`（b′） | ②，依 U11 換 ③；target 層級條件是通例（S-9） |
| S-3 (1) | 改窄成「不以 target 選能力」 | 只禁 backend | 禁 plugin 與後端＋守衛 | 改為 (i) 刪（S-7：(ii) 重新引入產品側守衛） |
| S-5 | 可呼叫 | 可呼叫＋夾具不算 | 產品只持有、夾具排除 | ②（③ 的 (c) 是產品側規則） |
| S-8 | (a) | (a) 附條件 | (a′) | (a′) |
| 執行批 | 2 新批（app-assembly、sweep）＋差分；+33～66 | 1 新批（可併 world-exec）＋差分；+15～36 | 1 新批＋差分；+23～55 | 1 新批＋差分；sweep 拆進各 wrap 批與 closeout；+23.5～57 |
| app-assembly 時點 | world-exec＋段 (i) 後 | match-sim 之前 | 段 (i) 後、plugins-frame 前 | 架構線，world-exec 與段 (i) 後、client-frame 前；不放關鍵路徑；plugins-frame 不依賴（S-5） |
| 往產品加步驟 | 7→6（＋2 條件） | 7→6 | 7→7（條件 6） | 7→7（刪 3 條、加 3 條，其中 2 條條件） |

---

## 統合者與修訂者沒有把握的地方

1. **R-6 與「Engine 型別只由 E 登記」**：assets-core 原計畫沒寫 `Assets<T>`、`StageAssets` 由誰登記（計畫 §3.19 只寫「在 `engine/asset`」，loader 則明寫「登記者是 plugin」）。① 的「否則會由產品 plugin 代登、日後改 owner 翻動 golden」的前提沒查證。
2. **卡 08 的結論**：本稿假設卡 08 照建議 B 修（zone-C 內序位＋名字）。若改走「改 spec 配合實作」，F1 下兩端的 plugin 集合要相同，本稿的 F1 要重看（§2.7 末點）。
3. **B1-U11 的性質**：把「需要後端」移到 `plugin.json` 是否算重開 review 決定行選的段 (i) 路線，由使用者判斷。
4. **B1-U8／U12 的界線**：段 (i) 讀 `engine/*/plugins/*/` 與 owner 語義是實作，三處「預留、不實作」要改寫；使用者可能想把這些留到 app-assembly 才實作（那就是必答 3 的 (c) 或另一種切法）。
5. **entity_store** 歸「基礎」，依據是未來的依賴（Delta-3）；今天 Kernel 不連它。
6. **跨 Runtime 依賴**（B1-U18）是否已被 Extract 的既有錯誤涵蓋，沒查證。
7. **工具守衛的正向清單**（`ALLOWED_TARGETS`）會不會因產生的登記單元而要加 E 的 plugin target，沒讀守衛實作。
8. **估時全部推定**，沒有先例批可比；client-frame 與工具四批的差分尤其沒有根據。app-assembly 改 xhigh（N-8）後的工時沒有重估。list scheduling 的瓶頸沒重算。
9. **device 建立的時點**（finish 或 Startup）與 spec C.6「GPU 等待由 Host 在 Runtime 建立前處理」怎麼對上，留給 wrap-render 宣告。
10. **Client 的 frame 清單裡實際帶入哪些 E**，要看 wrap-render 定的 `requires`；§7 的範例只是示意。
11. **內建 loader 留 asset 的代價**（Match 帶著 `FontLoader` 等）是否被認為違反 F1 的精神，由使用者判斷；本稿認為 loader 不是能力（不需要後端），不違反。

---

## 敵對檢查處置記錄（Fable／high 一次；報告在主 checkout `build/evidence/engine-own-plugins-plan/adversarial-fable.md`）

修訂者 Opus／high，2026-11-14。處置由主會話定；B-1 依架構師開卡 08 的補充改為「知悉、另由卡 08 處理」。

| 代號 | 處置 | 改了哪裡 |
|---|---|---|
| B-1 | 採。定性為已完成批 registry-schedule（#213）的缺陷候選，另由卡 08 處理（已開）；B1 不選、不當產品契約、不提議改 spec H.3；刪「緩める面」一句與 A／B／C 選項 | 摘要 8、§2.7（改寫為事實＋卡 08＋對 B1 的依賴）、§4.1 判定 6（以卡 08 為前提）與停止條件（刪 §2.7 條）、§4.2 client-sim 列、§6 第 10 點、§7（刪舊新 9、Client 範例）、B1-U15 改「知悉」、沒把握 2 |
| S-1 | 採。關鍵路徑 338～707 h（333＋5、696＋11），M3 118～254、M4 184～396（match-host 判定移走後；留在原處則 184.5～397）；寫明 client-frame 對 app-assembly 的依賴為何不進路徑 | 摘要 7、§4.3 |
| S-2 | 採。建置層排序寫成「段 (i) 宣告待定的前提」，列出依賴它的 U13、§2.7、S-9 子題 3 | §2.4、§2.7 末點、B1-U13、§6 第 10 點 |
| S-3 | 採。列出 U10 動到的已定文字（AP:280、:282、MP:806、AP:150-154、MP:396）；補工具 `variants.<v>.runtimes.<rt>`、Editor 兩個 Runtime、「恰一個後端」的層（(α) 單位層建議／(β) Runtime 層）、R-4 對多 target `sdl` 的意義 | §2.5、§5 末段、§6 第 2、3 點、§7 工具範例、B1-U10 |
| S-4 | 採。Runtime 路由隨段 (i) 交付（估時移入段 (i)）；設定初值在 App 之前由 Host 在寫入窗口 (1) 直接寫（或預設取 Match 值）；match-host 的判定移到 client-frame，正例先命中；寫明 Match／ClientSim 的 (c) 登記不改及理由 | §2.1〈F2 的適用範圍〉、§2.6〈App 存在之前的 Match〉、§4.1、§4.2（段 (i)、match-host、client-frame 列）、B1-U14 |
| S-5 | 採。plugins-frame 不依賴 app-assembly；被依賴清單＝client-frame、plugins-editor、工具四批；寫出鏈長與同線串行時的餘裕 | §4.1 依賴、§4.2 plugins-frame 列、§4.3 |
| S-6 | 採。§7 照實重列：件數 7→7 一致；第 1 列改「按 Runtime 寫、E 多半自動補」並修 Client 範例；新增產品側項目逐列寫 Feature 或「交使用者」；第 3 列不標「收窄」 | 件数の突き合わせ、§7 |
| S-7 | 採。S-3 (1) 建議改 (i) 刪；寫明 (ii) 與 review 決定行（HO:56）的張力、(i) 的風險與接受理由；(ii) 答 HO:201 兩問 | §3 S-3 (1)、§4.2 adapters-relocate 列、§6 第 2 點、§7 第 3 列、B1-U2 |
| S-8 | 採。刪 imgui `requires: ["input"]` 與「由 `PhysicalInputFrame` 餵入」；改為未定 | §2.3 D0 例、§3 S-1〈imgui 輸入的來源〉、§4.2 plugins-editor 列、§6 第 8 點、§7 工具範例、新 B1-U20 |
| S-9 | 採。target 層級條件寫成通例；三子題的選項；標段 (i) 宣告前要定 | §3 S-1、§2.5 第 3 點、新 B1-U22 |
| S-10 | 採。U8、U12 明寫改寫三處（HO:56、MP:396、AP:184），交使用者 | §2.2、必答 3、§5 末段、B1-U8、B1-U12 |
| S-11 | 採。每個 E 一個目錄 `engine/<module>/plugins/<e>/`（10 列位置表），讀取根照改；一檔多 plugin 列為選項 | §1〈target 的形狀與 E 的登記資料位置〉、件数の突き合わせ、B1-U8 |
| S-12 | 採。`FontLoader`、`TextLoader`（以及 `SoundLoader`、`BinaryLoader`）仍由 E `asset` 登記，寫理由；E `text` 只登記 rasterizer 的型別 | §1 第 1、18 列與〈內建 loader〉、§5 第 6 項、B1-U19 |
| S-13 | 採。「手寫類別不覆寫 `Requires()`」守衛、「E 與核心庫分 target」新 DI、DI 2 守衛擴張（E 的類別）三者標不出 Feature，照判定 3 延後；app-assembly 估時扣掉並重算全部合計；§2.x 標題標 F 與 Product | §2.4、§3 S-3 (2)、§4.1 估時、§4.2 算式、§6 第 1、5 點、§7 末段、§2.1～2.7 標題 |
| S-14 | 採。證據已存主 checkout `build/evidence/engine-own-plugins-plan/`（七個檔）；出處段改寫；合計與關鍵路徑的算式寫在 §4 | 出處、§4.2〈算式〉、§4.3 |
| S-15 | 採。〈未決〉加 B1-U21 預設組 | §2.3 標題、B1-U21 |
| N-1 | 採。`Fingerprint.cpp:253` 改稱 Registry fingerprint（`RegistryFingerprintOf`，:249-253） | 必答 3 (c)、§6 第 11 點 |
| N-2 | 採。RC (ii) 的引用改 AP:283 | §3 S-1 (c) |
| N-3 | 採。math 的說法改「RC `:56` 末句明列留 Engine，只是不在第一問的例舉」 | §1 第 11 列、三案比較 |
| N-4 | 採。刪「第二個 loader 也不會撞 R-E2」；改寫為「同型別兩個 loader 本來就是 finish 的明確錯誤，與 owner 無關」 | §1 第 12 列 |
| N-5 | 知悉。計畫 §0 末段已是 37＝36＋1；核准後 38＝37＋1（計畫修訂照此寫） | 計畫 §0 |
| N-6 | 採。不加 `class: capability`；`class` 只對 extension；`product` 不寫 class；邊界檢查 3 與 R3 的關係寫明 | §2.2 |
| N-7 | 採。Bevy 引用改 `crates/bevy_extract/src/extract_plugin.rs:57`、`:96` | §2.5 選項 D |
| N-8 | 採。app-assembly 改架構線、實作 Opus／xhigh、審查 Fable／xhigh，對齊 Kernel 批 | §4.1 排程與段 |
| N-9 | 採。四點寫進 app-assembly 宣告的輸入 | §2.1〈App 記錄的實作細節〉、§4.1 |
| N-10 | 採。U16 加 SPEC:376、:846 | §5 末段、§6 第 10 點、B1-U16 |
| N-11 | 採。四處補進計畫修訂：段 (i) 餘裕五處（MP:159、:197、:200、:399、:558）改 20～44 h（若核准）；Q3 字面與 `gyo_asset_plugin`（§1、B1-U8）；S-5 (b) 下 tool-ui 判定改寫（§3 S-5）；JSON 模組是函式庫（§1） | §1、§3 S-5、§4.3、B1-U8；計畫各處 |
| N-12 | 採。S-8 與必答 1～3 加「影響的批」欄；摘要 6 補 match-host、adapters-relocate、plugins-frame；§2.6 A 引 Host 寫入窗口 (1)（SPEC:140）與 G-4（SPEC:778） | §3、摘要 6、§2.6 |

**統計**：blocker 1（採 1）／should 15（採 15）／nit 12（採 11、知悉 1：N-5）。宣告判定 7「0 blocker」：B-1 的處置不在 B1 內選擇、改由卡 08 處理，本稿不再含 blocker 所指的錯誤主張；是否再檢查一次由主會話定。

**數字的變動（統合稿 → 本修訂）**：執行批淨增 +26～62 → **+23.5～57 h**；合計（若核准）727～1,512 → **724.5～1,507 h**；關鍵路徑 335～701 → **338～707 h**；超錨點 213 → **208 h**；RC-11 餘裕 436.5 → **441.5 h**；U11 條件式 723～1,504 → **720.5～1,499 h**；§7 列數 7→7（內容改變）。

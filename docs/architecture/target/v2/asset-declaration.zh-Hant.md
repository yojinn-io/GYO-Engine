# 事前宣告：asset Module（`v8-arch-gyo2-asset`）

更新：2026-11-21（修訂 0）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：草稿，等動工前核准。實作要等第 5 批的「多個 World、交換點」做完，而且使用者說開始。**

性質：實作段第 6 組的 Module 的事前宣告。涉及 AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta（新 Subsystem、改寫已核准的 Design Invariants）、§4 的系譜與決勝理由、§6 的外部組件 adapter、§8 的 Data Contract。

用詞照日誌、trace 的宣告：設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。交使用者的只有第 12 節。

**核心一句**（每一項都拿來對照）：asset 是**機制**。它告訴外面「要的東西載到哪了、成了沒有、為什麼沒成」，把載好的值交給 World。它**不決定**：
- 要不要等載完；
- 載入失敗後怎麼辦；
- 內容身分怎麼比；
- 檔案是什麼格式。

這四件分別歸作者、作者、作者或網路 Module、擁有那個型別的 Module。

## 0. 根據與讀的版本

**程式**：`gyo2` f25837b0（#258 trace 之後）。行號照這個版本。

**使用者的決定**（主會話轉來；HANDOFF 在文件分支 `origin/claude/gyo2-time-design`）：

| 代號 | 內容 | 本頁怎麼用 |
|---|---|---|
| `v8-arch-gyo2-asset-three-decisions`（2026-11-21） | 原話「asset 三件照建議」：(1) 圖片解碼不走 SDL，先用 stb_image，只在一個 adapter；(2) DI 照新前提重寫；(3) 起步照 Bevy 執行期讀原始檔，loader 介面接得上日後的事先轉檔 | 第 5.11、7、5.10 節 |
| `v8-arch-gyo2-parallel-asset-and-stale-mutants`（2026-11-21） | 舊 DI 2 要「IO 執行緒加交換點」，舊 DI 2、3 用 (c) 區與 Ingress stage；批次表的依賴更正為含交換點 | 第 2.2 節（實作的依賴） |
| `v8-arch-assets-bevy-model`（2026-11-11，kernel HANDOFF:144） | `AssetManager` 不選 World，照 Bevy 拆成三類：請求狀態、每個型別一份 `Assets<T>`、GPU 資源；loader 由各 plugin 登記 | 第 5.1～5.4 節（方向不變） |
| `v8-arch-target-di-approved`（2026-11-13） | 舊 DI 1～3 與 Replacement Criteria 核准 | 第 7 節的系譜 |
| `v8-arch-gyo2-build-world-phase-is-mechanism`（2026-11-16） | 「開局、Loading、讀檔」是遊戲流程的名字，不是機制；有沒有 Loading 階段由作者決定 | 第 5.8 節：「載完才開始」歸作者 |
| `v8-arch-gyo2-save-unaware-means-format-only`（2026-11-16） | Engine 不認得存檔的格式 | 第 5.9 節 |
| `v8-arch-gyo2-module-plugin-owner-definition`、`v8-arch-gyo2-json-adapters-dir` | Module＝我們維護、介面是我們的；第三方放該 Module 的 `src/adapters/<組件>/` | 第 5.1、5.11 節 |
| `v8-arch-gyo2-engine-not-rigid`、`-mechanism-not-conservative` | 預設方便、不禁止別的做法；建機制這一輪不選保守的選項 | 第 5.3 節（帶過去預設開）、第 8 節 |
| `v8-arch-gyo2-infrastructure-before-products`、`-migration-needs-consent` | 先建基礎；搬產品要使用者同意 | 第 3.2 節的 Feature 只用來推需求，不搬產品 |
| `v8-arch-gyo2-replace-order-author-choice` | 使用者點名 BigMap（master `plans/big-map/`）是有的 Feature | 第 3.3 節的規模 |

**已合併、本頁要對齊的**：
- [編排層](orchestration.zh-Hant.md)：O-2 生命週期、O-6 登記檔、O-7 準備時機、O-8 替換、O-10 帶過去、O-11。
- `target/error-handling.md`〈Error levels〉：`…Error`＝E1，`…Failure`＝E2，Engine 的 Module 不寫日誌。
- [事件游標](event-cursor-declaration.zh-Hant.md)：`Message`，以及丟棄記錄由 runner 在兩次 Run 之間取走。
- [時間＋事件](time-events-declaration.zh-Hant.md)第 3.6 節：預設骨架 `Main`（`First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）。

**進行中、本頁對齊的**：
- 第 4 批，draft #259。宣告在分支 `claude/gyo2-authority-rollback-bda9ba` 的 `authority-rollback-declaration.zh-Hant.md` 修訂 5，要對的是第 3.2、4.4、8 節。
- 第 5 批的交換點：還沒有宣告。第 5.5 節寫成給它的需求。

**參考，不帶**：
- master 的 `engine/asset`：`AssetManager`、`AssetCatalog`、`hot_reload/`、`loaders/`、`sdl_image/SdlImageTextureLoader`。
- 舊的 [target/assets.zh-Hant.md](../assets.zh-Hant.md)：11-11 的方向與 11-13 的 DI。
- `target/runtime-composition.zh-Hant.md` §3a 與 DI 3：Loading 的 Ingress、資產 stage。

**證據**（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-asset/`，git 不追蹤）：
- `reference-survey.md`：別家原始碼的檔案與行號、版本；master 的內容種類與大小。
- `declaration-r0-adversarial-check.md`：修訂 0 的敵對式檢查。

---

## 0.1 時間估計

| 段 | 估計 | 內容 |
|---|---|---|
| 設計段 | 4～6 h | 讀現況、對照別家、本頁、Fable 檢查與修訂 |
| 實作段 | 24～38 h（推定） | 第 9 節的 6 步。交換點的形狀還沒定，所以是推定；交換點宣告核准後重估 |

停止條件：超過 1.5 倍（設計段 9 h、實作段 57 h），或單一問題累計超過 2 小時，就停下回報（AGENTS §14 冒頭）。

## 1. 這一批做什麼（先用遊戲講）

FPS 的 Client 進對局之前，要載以下內容：
- 角色模型（fbx）；
- 武器貼圖（png）；
- 字型（ttf）；
- 音效（wav）；
- 規則與地圖的 JSON。

**載入流程**：
1. 生成世界階段，作者的 System 對 `AssetRequests` 說「我要 `weapons/.../Nailer1_D.png`，當成 `Image`」，**當場**拿到 `Handle<Image>`。
2. 讀檔與解碼在 World 的執行緒之外做，不碰 World。
3. 結果在交換點交回這個 World。
4. asset 的 System 在 `First` 把結果放進 `Assets<Image>`，並發一則「加入了」的 `Message`。
5. 作者的 System 看 `AssetRequests` 的狀態：全部載完才離開生成世界。等不等是作者的事。

**失敗時**：檔案不在，狀態就是「失敗」，帶 `AssetLoadFailure`（E2）。asset 不記日誌、不重試，World 照跑。
- 作者決定要停下、換預設圖，還是顯示錯誤畫面。
- 日誌 Module 要記的話，就訂閱那則 `Message`。

**讀檔時**：玩家中途讀檔，編排層替換 World。asset 的 Resource 預設**帶過去**，所以同一關的貼圖不必重讀；只有新 World 沒再要的東西會被放掉。

**模型檢視器的 `info` 指令**：不建 World，直接呼叫模型 Module 的 loader，把位元組交進去、拿回值。loader 本身就是一個能直接呼叫的純函式。

## 2. 範圍

### 2.1 這一批做的（動工前核准之後；實作等交換點）

| 代號 | 內容 |
|---|---|
| AS1 | `engine/modules/asset`（`GYO::Engine::Asset`，target `GYO::Asset`），編排層 `Module` 的子類，登記進 `config/modules.csv` |
| AS2 | 請求狀態 `AssetRequests`、每個型別一份 `Assets<T>`、`Handle<T>`／`AssetId<T>`、`AssetPath`；在請求時配身分、引用計數、沒人用就放掉 |
| AS3 | loader 的介面與登記入口（擁有型別的 Module 在 `Build` 登記）；loader 是可以直接呼叫的純函式 |
| AS4 | 讀檔與解碼的工作（job），在 World 執行緒之外跑；結果經交換點交回、在 `First` 套用。**怎麼交回照交換點的宣告**，第 5.5 節列要求 |
| AS5 | 狀態查詢、`Message`（加入、移除、失敗）、「全部完成」的查詢 |
| AS6 | 錯誤：`AssetLoadFailure`（E2）、登記時的啟動錯誤、丟棄的記錄（E3） |
| AS7 | asset 自己帶的兩個 loader：文字（UTF-8 原文）、二進位（原樣位元組） |
| AS8 | **第 12 節第 1 題選 (B) 時**：`engine/modules/image`（`Image` 型別＋loader，stb_image 只在 `src/adapters/stb_image/`）。選 (A) 時 AS8 不在本批，隨 SDL 那批的 render |
| AS9 | 新頁 `target/v2/asset.zh-Hant.md`：Design Invariants、Replacement Criteria；舊 `target/assets.zh-Hant.md` 頂端標「已由 v2 取代」 |

### 2.2 不在這一批

- **事先轉檔的工具與它的資料契約**（使用者決定 3）。留給工具回來時；本批只保證 loader 介面接得上（第 5.10 節）。
- **其他 loader**：
  - 模型（ufbx）：隨產品搬回進 model Module。**SDL 的 adapter 與 ufbx 歸哪一類，仍在 HANDOFF〈交使用者的〉，本批不判。**
  - 音效（wav）：歸 audio Module，在 audio 重審那批。
  - 字型（ttf）：歸 text Module，隨 SDL 那批。
- **GPU 資源**（Bevy `RenderAssets<T>`）：歸 render，隨 SDL 那批（K.3-6 的渲染 World＋Extract）。
- 熱重載、載入時帶依賴（loader 在解碼中再要別的 asset）、一個檔分出多個子 asset（Bevy 的 `#label`）、Product 層共用的快取。
  - 原因：沒有點名的 Feature 直近要用（第 3.2 節）。
  - 處理：本批只留介面的形狀（第 9 節的 thin start），Replacement Criteria 寫何時回來。
- master 的 `AssetCatalog`（id → 型別與路徑）。照 11-11「拿掉總管物件」與核心不認得格式，catalog 是產品或工具的內容（第 5.2 節）。
- 交換點本身的設計（第 5 批）。
- threads 與 io 的重審（批次表第 6 組）。本批只把 asset 對 io 的要求交過去（第 5.12 節）。

**實作的依賴**：
- 編排層（#244）；
- JSON（#252）：不直接依賴，只是文字 loader 的使用者常常接著用 JSON；
- **第 5 批的多個 World、交換點**；
- 第 4 批合併：要用 Kernel 的逐型別雜湊客製點寫內容身分的範例測試。

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

上面會積的東西：
- render、text、ui、audio、model 的 loader 與 GPU 上傳；
- 每個產品的生成世界；
- 讀檔（替換 World 時帶過去）；
- 工具的資料契約（事先轉檔）；
- PvP 的內容身分比對。

Handle 的生命週期、`Assets<T>` 在哪個 World、結果怎麼交回 World，這三樣改了，上面每個 loader 與每個產品的載入流程都要跟著改。

### 3.2 予見的根據 → 對 asset 的要求（Feature → Contract → Architecture）

Feature 的出處：master 的產品 `apps/object_fps_pvp`、工具 `tools/{model_viewer,voxel_map_editor,audio_editor,ui_editor}`、內容 `assets/object_fps_pvp`。逐檔與行號在證據 `reference-survey.md` 第 2 節。

只用來推需求，**不搬產品**（`-migration-needs-consent`）。

| # | Feature（誰用、看得到什麼） | 用到的內容種類（master 實查） | 對 asset 的要求 | 本批 |
|---|---|---|---|---|
| F1 | 遊戲 Client：進對局前載完角色、武器、字型、音效、UI、地圖 | fbx 4、png 5、ttf 1、wav 4、json 22（`PvpApplication.cpp:958-962` 登記五種 loader） | 多種型別各自的 loader；請求當場拿到 Handle；查「全部完成」；失敗帶原因 | 機制做；fbx／wav／ttf 的 loader 不做 |
| F2 | 遊戲 Match（無畫面）：載角色模型與規則算判定（master 的 `PlayerAnimationContent.cpp:89-90`） | fbx、json | **不需要 SDL 與視窗**也能載；載出的值兩端一樣（內容身分）由作者比 | 機制做；比對歸作者（5.8） |
| F3 | 讀檔（`-restore-entry-scope-player-save`）：遊戲中讀檔＝替換 World | 同 F1 | 替換 World 時不必重讀同一關的內容 | 做（5.3 帶過去） |
| F4 | 模型檢視器：`info`、`models` 指令不建 World（`tools/model_viewer/src/ModelContent.cpp:68`、`:120` 直接用 loader）；`window` 看模型與貼圖 | fbx、png、wav | loader 能**不經 asset 核心**直接呼叫；同一個 loader 也能登記進 World | 做（AS3） |
| F5 | ui_editor 的預覽：在編輯器裡顯示字型與圖片（`AssetPreviewContext.cpp:92-171`） | ttf、png | 和 F1 同一個機制；Editor 的 World 也能載 | 機制做 |
| F6 | audio_editor：讀 wav、轉出 catalog（`tools/audio_editor/src/Main.cpp:27-63`，經 io 直接讀） | wav、json | 工具自己用 io 讀；工具輸出的內容（日後是引擎格式）由執行期的 loader 讀——**接得上事先轉檔** | 只保證介面（5.10） |
| F7 | 地圖編輯器：讀寫自己的地圖檔（`voxel_map_editor/src/FileAccess.cpp:21-27`，經 io） | json、voxel mesh json | 編輯器的檔是它自己的；遊戲讀它的輸出時經 loader（資料契約） | 只保證介面（5.10） |
| F8 | BigMap（使用者點名，master `plans/big-map/`）：約 1 km²、區塊化地圖 242 MB、每個 region 的文字 ≤ 16 MiB | 地圖 region | 一個大內容能拆成許多獨立的請求；單一 asset 的上限可設；**執行中串流進判定狀態**沒有決定 → Replacement Criteria | 只做上限與獨立請求 |
| F9 | 2D 橫版遊戲（`-engine-scope-general`；架構師自建樣本） | png、json | 單機、不需要 PvP 的任何東西；不寫任何雜湊函式也能用 | 做（5.8：內容身分不強加） |

**推不出、不放進來的**：
- 熱重載：master 有 `AssetWatcher`，但 `apps/`、`tools/` 沒有任何呼叫，`git grep` 0 件，正例已確認命中 `engine/asset`。
- 載入時帶依賴：F1 的 viewmodel json 引用 fbx 與 png，但 master 是產品自己依序載。
- 子 asset：master 的 ufbx loader 一個檔出一個 `ModelAsset`。
- Product 層共用快取：沒有量測顯示同一內容在兩個 World 重複解碼是問題。

### 3.3 規模與預算

**已知的規模**（master 實查，`reference-survey.md` 第 2 節）：
- 遊戲內容共 36 個檔、36.7 MB；最大的是 fbx 23.7 MB，其次 ttf 4.5 MB、png 1.34 MB、json 1.45 MB。
- BigMap：242 MB、region 文字 ≤ 16 MiB（master `big-map/engine-format.zh-Hant.md:43`、`:96`）。

**預算：寫不出數字，列為未決（第 12 節第 4 題）**。具體是：
- 載入時間：例如「F1 全部內容在冷啟動 ≤ ? 秒」；
- 記憶體：同時在飛的請求上限、解碼中的峰值；
- 每幀套用的上限：一幀最多套用幾筆或幾 MB，免得卡幀。

**本批不用假設填。** 實作段只做探索量測（標明不是判定），等使用者給數字或明說延後。

## 4. 別家怎麼做（同一種技術比）

**標記**：
- **〔源〕**＝本機原始碼查證，版本：Bevy `6a911653f`（0.20.0-dev），Unity Entities 1.5.0，flecs 本機。
- **〔知〕**＝一般知識，沒有原始碼可查。

| 家 | 核心認不認得格式 | 身分與 Handle | 結果怎麼進 World、順序 | 執行期讀什麼 | 圖片誰解碼 | 為什麼這樣做 |
|---|---|---|---|---|---|---|
| Bevy〔源〕 | 不認得。`AssetLoader` trait（`bevy_asset/src/loader.rs:32-49`：`Settings`、`Error`、`load`、`extensions`），由各 crate 登記 | `load` 當場回 `Handle`（`server/mod.rs:364`）；`Assets<A>` 是 dense storage＋`Uuid` 表（`assets.rs:288-306`）；強 Handle 被丟掉時經 channel 通知（`handle.rs:81-91`） | IO 在 `IoTaskPool` 跑（`server/mod.rs:427`）；完成後送 channel；`handle_internal_asset_events` 在 `PreUpdate` 以 `&mut World` **照完成順序**套用（`server/mod.rs:1890-1895`、`lib.rs:423-433`）；`AssetEvent`（`Added`、`Modified`、`Removed`、`Unused`、`LoadedWithDependencies`，`event.rs:50-60`）在 `PostUpdate` 發 | 預設 `AssetMode::Unprocessed`＝原始檔（`lib.rs:297-336`）；可選 `Processed`：`AssetProcessor` 事先轉，`.meta` 帶 `AssetAction::{Load,Process,Ignore}`（`meta.rs:70-86`），用來源雜湊決定要不要重轉（`processor/mod.rs:86-91`） | `bevy_image` 是**獨立的 crate**，用 `image` 函式庫；依賴 `bevy_asset`，**不依賴 `bevy_render`**；render、ui、text、sprite 都依賴它（各 `Cargo.toml`） | 原始檔讓開發迭代快、不需要工具；事先轉檔留給發佈與重格式。圖片獨立，讓 ui、text 不必依賴 render 也能拿到尺寸與像素 |
| Unity Entities〔源〕 | 不認得。執行期只載 build 時產生的 content archive 與 catalog（`RuntimeContentManager.cs:30-53`、`:412`） | `WeakObjectReference<T>`（`WeakObjectReference.cs:14`）：`LoadAsync`、`LoadingStatus`、`Release`（`:45-85`），引用計數 | `RuntimeContentSystem` 每幀處理排隊的命令（`RuntimeContentSystem.cs:95`）；狀態 `ObjectLoadingStatus`（`RuntimeContentManagerUtility.cs:15`） | **事先轉好的格式**（build pipeline 產生） | Editor 匯入時（`TextureImporter`），執行期不解碼原始檔〔知〕 | 發佈的內容一致、載入快；代價是一定要有 build pipeline |
| flecs〔源〕 | 沒有 asset 系統（`flecs.h` 搜尋 `asset` 0 件） | — | — | — | — | 只做 ECS，資源交給使用者 |
| Unreal〔知〕 | 不認得：`UFactory` 匯入、`.uasset` 序列化 | `TSoftObjectPtr`、`FStreamableManager` 非同步載入 | 載入在 async loading thread，完成後回呼在 game thread | 事先轉好的 `.uasset`／cook 後的 pak | 匯入時（Editor） | cook 讓平台格式事先備好 |
| Godot〔知〕 | `ResourceFormatLoader` 登記 | `Resource` 引用計數；`ResourceLoader.load_threaded_request` | 執行緒載入，主執行緒取結果 | 匯入後的 `.import`／`.ctex`；原始檔留在專案 | 匯入時 | 同上；Editor 自動匯入，使用者看到的還是原始檔 |
| O3DE〔知〕 | Asset Processor 依副檔名轉換 | `Asset<T>`＋`AssetId`（GUID） | `AssetManager` 在執行緒載、主執行緒派發 `OnAssetReady` | Asset Processor 的輸出（cache） | Asset Processor（Image Builder） | 和 Unity、Unreal 同一路 |

**共通**：
- 核心都不認得格式；
- 身分在請求時就給；
- 圖片都不用視窗層解碼。

**分歧**：
- 執行期讀原始檔（Bevy 預設），還是讀事先轉好的格式（其餘）。使用者已定：起步照 Bevy，介面接得上事先轉檔。
- 完成的結果照什麼順序進 World：Bevy 照完成順序，Unity、Godot 是各自的佇列〔知〕。GYO 的取捨在第 12 節第 2 題。

## 5. 設計（Contract）

### 5.1 位置、名字、依賴（卡片照 L17 判，照 N11～N17 與 O-6）

- 目錄 `engine/modules/asset`，含入 `engine/asset/…`，namespace `GYO::Engine::Asset`，target `gyo_asset`／`GYO::Asset`。
- 登記列：`asset,modules/asset,GYO::Asset,"Asset requests, per-type stores, handles and the loader registry",no,yes,base;kernel;orchestration;io,threads,...`。
  - `default=yes`：照「Module 預設全帶、產品可以關」，Bevy `DefaultPlugins` 含 `AssetPlugin`；渲染 World 不需要時，產品在那個 World 關掉。
  - `threads` 是否需要，看交換點把執行緒歸誰（5.5）。
- **不屬 core**：要放 Resource 進 World，依賴 ecs 與 orchestration（`docs/architecture/README.md`〈Directory layers〉：core 不知道 ECS）。
- **不是 Plugin**：我們維護、介面是我們的。
- 名字：
  - 請求狀態叫 `AssetRequests`，不照 Bevy 叫 `AssetServer`。「Server」在 GYO 已經是 PvP Match 伺服器的概念（`target/naming.zh-Hant.md` DI 4「名字是概念的 tag」）。
  - 其餘照 Bevy：`Assets<T>`、`Handle<T>`、`AssetId<T>`、`AssetPath`、`AssetMessage<T>`。

### 5.2 請求、身分、Handle 的生命週期

- **請求**：`AssetRequests::Load<T>(AssetPath, settings = {})` 在 World 裡同步執行，當場回 `Handle<T>`。這時配 `AssetId<T>`（index＋generation）、記一筆「排隊中」、給一個請求序號。
- **去重**：鍵＝（`AssetPath`、`T`、settings）。同鍵已在、而且還沒被放掉時，回同一個 id 的新 Handle，不再讀一次（Bevy 同）。settings 要能比較相等與雜湊。文字、二進位 loader 的 settings 是空的。
- **路徑**：`AssetPath`＝「來源名＋來源內的相對路徑」。來源是 io 的掛載點（`engine/io/fs/MountTable.hpp`、`Vfs.hpp`）。
  - 不認得 catalog：master 的 `asset_catalog.json`、`content.json` 是產品的內容，產品自己用文字 loader 讀了，再照它要的路徑請求。
  - 理由：核心不認得格式（AS-2）；Bevy 也只認路徑。
- **Handle**：
  - 強 Handle 帶引用計數。最後一個強 Handle 消失，那個 id 就在下一次套用時移除，發 `Removed`。不另做快取策略：要留著就拿著 Handle。這是 Bevy 的預設；master 的 `AssetCachePolicy` 不帶，沒有 Feature 要。
  - `AssetId<T>` 是弱的，不保活。
- **Handle 不參照 World**：Handle 的計數接到它那份 store（`AssetRequests` 擁有的共用狀態），不接 World。所以 store 被帶到新 World 時，Handle 照樣有效（5.3）。

### 5.3 `Assets<T>` 放在哪個 World；多個 World；替換 World

- **每個 World 各自一份 `AssetRequests` 與它用到的 `Assets<T>`**，放在**請求它的那個 World**（11-11 方向 2）。
- 渲染 World 不另外請求 CPU 的 asset，經 Extract 唯讀拿「哪些變了」，自己上傳 GPU（Bevy `RenderAssets`；K.3-6，隨 SDL 那批）。
- **兩個非渲染 World 要同一個內容**（例：Client 的模擬 World 與每幀 World），各自請求、各自解碼。
  - 不做 Product 層共用快取：那會長回一個跨 World 的總管，正是 11-11 拿掉的形狀。
  - 量到重複解碼超出預算時照 RC-AS3 重看。
- **替換 World（O-8）時預設帶過去**（卡片照 L17 判）：asset Module 在 `Build` 對自己的 `AssetRequests` 與各 `Assets<T>` 標 `CarryOver`（O-10），產品可以關。
  - 理由：F3 讀檔不必重讀同一關的內容。
  - 為什麼可行：`BuildNewFirst` 時舊 World 還在，舊 World 裡的 Handle 讓內容活著；新 World 的生成世界再要同一路徑時，去重直接命中；舊 World 銷毀後，沒人再要的才被放掉。
  - `DropOldFirst` 時舊的 Handle 先消失，沒被新 World 及時要回的會被放掉。這是作者選它的代價，寫進新頁的說明。
  - **作者的責任**（O-10 的延伸）：關掉帶過去，又在別的帶過去的 Resource 裡存 `Handle`，那個 Handle 會指向已銷毀的 store。不立實作面的守衛，靠新頁與審查（同 O-10）。
- **在飛的請求**：結果交回時對的是 store，不是 World 的位置。
  - store 被帶過去：結果跟著到新 World。
  - store 隨 World 一起被丟掉：結果丟掉，計一筆丟棄記錄（E3，5.7）。

### 5.4 loader 的介面與登記

- **介面**（形狀；名字在實作時照 N15 定）：一個 loader 宣告下列四樣，加一個載入函式：
  - 它產出的型別 `T`；
  - 它認的副檔名（日後也可以是「引擎格式的標記」，5.10）；
  - `Settings`；
  - 錯誤碼。

  載入函式的形狀是 `Load(bytes, settings, LoadInfo) -> Result<T, LoaderFailure<Code>>`。`LoadInfo` 只有路徑（給錯誤訊息）與上限，**沒有 World、沒有時鐘、沒有全域可變狀態**。
- **直接呼叫**：loader 是一般的型別，不建 World、不經 asset 核心也能呼叫（F4、測試）。
- **登記**：擁有型別的 Module 在自己的 `Build` 呼叫 asset 給的入口，把 loader 登記進那個 World 的 asset（11-11 方向 3）。
  - 選 loader：先看請求的 `T`，同一個 `T` 有多個 loader 時再看副檔名。
  - 同一個（`T`、副檔名）登記兩個是啟動錯誤，在 `Finish` 回報，收進 `ProductStartupReport`（O-5）。
  - 請求時找不到 loader 是 E2（`AssetLoadFailure{NoLoader}`）。路徑是外部資料，不是呼叫寫錯，所以不當 E0。
- **asset 自己帶的 loader**：文字（UTF-8 驗證後的 `std::string`）、二進位（`std::vector<std::byte>`）。
- **上限**：每個 loader 給預設的位元組上限；請求可以收緊，不能放寬超過 loader 的上限。超過就是 `TooLarge`（E2）。理由：F8 的 region ≤ 16 MiB；master 的 `ReadAllBytes(…, max)` 同一個做法。

### 5.5 執行緒與交換點：給交換點宣告的需求

**本頁不設計交換點**，只列 asset 要它能做到的事。交換點的宣告可以用任何形狀滿足。

| # | 需求 | 為什麼（Feature） |
|---|---|---|
| X-1 | 一個 Module 能在 World 執行緒之外跑工作（讀檔＋解碼），工作**不碰 World**（M11） | F1、F2：一張 1.3 MB 的 png 或 23.7 MB 的 fbx 解碼不能卡住 World 的幀 |
| X-2 | 工作的結果**只在交換點**進入目標 World，寫進該 Module 指定的 Resource 槽；套用由該 Module 自己的 System 做 | M11；`Assets<T>` 的寫者只有 asset 的 System |
| X-3 | 請求從 World 出去：經交換點（出），或交換點宣告允許的別的通道。**延遲一幀可以接受**；同一個 World 的請求順序要保留 | 請求序號是 AS-5 的根據 |
| X-4 | 結果的目標是一個 **store 的身分**，不是 World 的位置：替換 World 帶過去 store 時結果跟著走；store 不在時丟掉並記數 | 5.3、F3 |
| X-5 | 丟掉的筆數能被看到：送出、收到、丟掉三個數對得上 | AGENTS §14 件數的突き合わせ；E3 |
| X-6 | **測試與無執行緒的 runner** 有一種同步的模式：交換點當場把排隊的工作跑完再交回，讓測試能「請求→跑一幀→已完成」，結果不依賴執行緒時序 | 判定的可重現；只跑一次的 runner（O-3） |
| X-7 | 執行緒的數量與歸屬由交換點／runner 定；asset 的工作是可以在任何執行緒跑的純函式（AS-3），不在乎幾條 | Bevy 用全域 `IoTaskPool`；GYO 不先定 |
| X-8 | 日後的熱重載、事先轉檔的「重轉完成」通知也是 World 外的事件，同一條路進來 | RC-AS4、5.10 |

**套用的位置**（卡片照 L17 判）：`Main` 的 `First`。
- 理由一：在回滾的獨占 System（`PreUpdate`，權威＋回滾宣告第 46 行的情境）之前，所以一幀之內、包含重跑，`Assets<T>` 都不變。
- 理由二：Bevy 在 `PreUpdate`，但 Bevy 沒有回滾的重跑。
- 產品關掉預設骨架時，自己把套用的 System 掛到想要的位置（O-4：編排層不認得 Module 掛了什麼）。

### 5.6 狀態與訊息

- **狀態**：`Queued`、`Loading`、`Loaded`、`Failed`（帶 `AssetLoadFailure`）、`NotRequested`。用 `AssetRequests::State(id)` 查。
- **「全部完成」**：`AssetRequests::Pending()` 回還沒結束的請求數；也可以傳一組 id 問「這些都結束了沒」。**要不要等由作者決定**。生成世界可以分批跑多次，作者在結束條件裡用它（`-build-world-phase-is-mechanism`）。
- **訊息**：每個型別一種保留式 `Message`：`AssetMessage<T>{Added, Removed, Failed}`。
  - 照事件游標的宣告修剪；`Modified` 留給熱重載。
  - 用 `Message` 不用推送式 `Event`：讀者是 render 的 Extract、日誌、作者的 System，各自照自己的步調讀，同 Bevy 的 `AssetEvent`（`Message`）。
  - 同一個交換點的一批之內，訊息照請求序號排（AS-5）。

### 5.7 錯誤（照已合併的〈Error levels〉）

| 情況 | 級別 | 怎麼出現 |
|---|---|---|
| 檔案不在、讀不到、超過上限、沒有 loader、loader 解碼失敗、loader 丟例外 | **E2** | 那筆請求的狀態變 `Failed`，帶 `AssetLoadFailure{code, message, detail}`（`Engine::Base::Error<Code>` 的形狀，**沒有鏈**）。loader 自己的碼名與細節放進 `detail`（error-handling.md「at a module boundary the inner error's code name and detail go into detail」）；同時發 `AssetMessage<T>::Failed` |
| 同一個（`T`、副檔名）登記兩個 loader、loader 的設定不合法 | 啟動錯誤 | `Finish` 回報，收進 `ProductStartupReport`（O-5） |
| 結果到了，但 store 已不在 | **E3** | 丟棄記錄（筆數、請求序號範圍），World 記、runner 在兩次 Run 之間取走交給日誌（同 `DroppedMessages` 的做法） |
| 呼叫寫錯（`Load` 給了未登記的型別參數、在 World 外呼叫） | **E0** | `GYO_ASSERT` |

- **asset 不寫日誌、不重試、不讓 World 中斷**（「Engine 的 Module 回傳錯誤，不自己寫日誌」）。要不要因為載入失敗結束遊戲，由作者的 System 決定：回 `Err` 就是作者把 E2 升成 E1。
- **loader 丟例外**：在工作的邊界接住，轉成 `LoaderThrew`（E2）。工作在 World 外，例外不能沿著 World 的 Run 傳出去。這和「World 裡的 System 丟例外是 E1」不衝突。

### 5.8 決定性、內容身分（和第 4 批的權威、回滾對齊）

舊 DI 2、3 建立在「(c) 區＋Loading 的 Ingress」上（11-13）。第 4 批拿掉區規則之後（權威＋回滾宣告修訂 5 第 2 步），重新推導如下：

1. **身分的決定性**：`AssetId` 在請求時同步配發，只取決於這個 World 的請求順序，和讀檔、解碼的完成時序無關（AS-5）。同一份程式、同樣的請求順序 → 同樣的 id 序列。
2. **內容的決定性**：asset 核心不改位元組。`Assets<T>` 裡的值＝loader(位元組, settings)。核心不加任何隨時序變的東西：不讀時鐘，套用的批內照請求序號排。
   - **loader 的輸出跨平台是否逐位元相同，是那個 loader 的事**（`docs/architecture/determinism.md` DET-2：第三方預設在邊界外）。
   - 各 Module 的 DI 寫自己的 loader 在不在邊界內：stb_image 的 PNG 是整數運算，預期在邊界內；它的 HDR／gamma 路徑用 `pow`，在邊界外（5.11）。
3. **`AssetId` 不跨行程**：兩端的請求順序不保證相同，Client 還多載了呈現用的內容。所以 `AssetId`／`Handle` **不進權威的 digest、不進存檔**（AS-6）。
   - 判定資料要參照內容時，存作者自己的穩定鍵，例如 `AssetPath` 或內容雜湊。
   - 權威的 digest 用 Kernel 的逐型別雜湊客製點（權威＋回滾宣告第 3.2、4.4 節）；作者如果把 `Handle` 放進判定型別，要為它寫雜湊函式，那是作者選了錯的鍵。
   - **asset 給 `AssetPath` 一個雜湊函式**，接上同一個客製點（卡片照 L17 判：一個概念一個實作）。
4. **內容身分**（舊 DI 3：asset 從暫存的位元組算）**不再是 asset 的規則**。
   - 舊的 `AnimationContentDigest` 本來就是產品的概念（`determinism.md` §4.9：算在 ufbx 轉出的值＋`presentation.json` 上，不是原始位元組）。
   - 新的做法：作者（或日後的網路 Module，join 時比）對 `Assets<T>` 裡**已載入的值**，用 Kernel 的逐型別雜湊客製點算。asset 只保證第 2 點：同樣的位元組＋settings → 同樣的值。
   - 單機與 2D 遊戲（F9）不需要寫任何雜湊函式。
5. **回滾**：`Assets<T>` 不是判定資料，回滾 Module 不存它、不倒回它。
   - 套用只在 `First`，所以重跑的幾步看到的 `Assets<T>` 和那一幀一樣。
   - **但**：如果第 100 個 tick 第一次跑時某內容還沒載好、重跑時已經載好，依「載好沒」分支的判定 System 兩次結果會不同。
   - 這是作者的責任：判定用的內容在進入判定之前載完（生成世界等它，F1）。
   - 寫進範囲の限界與 RC-AS1，不在機制裡擋，因為 Engine 不認得「判定」與「呈現」的分法（第 4 批之後）。

### 5.9 讀檔與存檔版本 Module

- **讀檔**（讀檔宣告修訂 11，核准版）＝替換 World，在新 World 的生成世界裡由作者的 System 放回。
  - asset 的部分：store 預設帶過去（5.3）；作者的讀檔 System 照存檔裡記的**路徑**再請求（不是 `AssetId`，AS-6）；結束條件可以用「全部完成」。
  - 存檔參照的內容不在了：那筆請求是 E2。作者的讀檔 System 決定要用預設內容，還是回錯誤（`BuildNewFirst` 時舊 World 照跑，O-8）。
- **存檔不是 asset**：存檔的格式是作者的（`-save-unaware-means-format-only`），讀存檔檔案用 io 或作者自己的方式。
  - 作者**可以**用 asset 的二進位或文字 loader 讀存檔的位元組。這是作者的選擇，asset 不知道那是存檔。
- **存檔版本 Module**（`v8-arch-gyo2-save-version-module`）：比對存檔的版本標記，和 asset 的內容版本**無關**。
  - 內容改版造成存檔參照的路徑消失，由上一點的 E2 處理。
  - asset 不提供「內容版本」的概念。事先轉檔的資料契約回來時，Version 屬那份資料契約（AGENTS §8），不屬 asset 核心。

### 5.10 接得上「工具事先轉檔、執行期讀引擎格式」（AGENTS §8）

使用者決定 3：起步讀原始檔，介面要接得上。本批保證三件，並各有測試（第 10 節）：

1. **選 loader 的鍵不是「原始格式」**，是（`T`、副檔名或格式標記）。工具日後產出的引擎格式（例：`.gyotex`）＝另一個登記給同一個 `T` 的 loader。請求的 API、Handle、`Assets<T>` 都不變。
2. **來源是 io 的掛載點**。工具輸出的資料夾＝另一個來源（掛載點）。asset 不知道是誰產生的（Editor、Script、手做都一樣，AGENTS §8）。
3. **loader 是純函式、可以直接呼叫**。工具自己要驗證輸出時，呼叫同一個 loader 就行，不必依賴 asset 核心。Engine 不承擔工具職能（`engine-not-tool-duties`）。

**不在本批**：轉檔工具、引擎格式、`.meta` 一類的旁邊檔、來源雜湊判斷要不要重轉（Bevy `processor/`）。這些是工具回來時那份資料契約的內容，含 Version 與 Validation Rule（AGENTS §8 MUST）。

### 5.11 第三方：stb_image（使用者決定 1）

| 項目 | 內容 |
|---|---|
| 組件 | `stb_image.h`（nothings/stb，單一標頭）。實作時釘一個 commit（不是 tag），同 yyjson 的做法 |
| 授權 | 雙授權，使用者擇一：MIT 或 Public Domain（Unlicense），寫在檔尾。打包時附授權告示，加進批次表「之後的小事」的第三方告示一條 |
| 放哪裡 | `third_party/stb_image/CMakeLists.txt`（FetchContent，同 `third_party/yyjson`），產生 `gyo_stb_image`（只含一個 `.c` 定義 `STB_IMAGE_IMPLEMENTATION`） |
| 只出現在哪裡 | `engine/modules/image/src/adapters/stb_image/`（第 12 節第 1 題選 B）；選 A 時是 render Module 的 `src/adapters/stb_image/`。`gyo_stb_image` 只被那一個 target **私有** link；標頭不出現在任何 `include/` 下 |
| 編譯設定 | 只開需要的格式（`STBI_ONLY_PNG`，jpg 等有 Feature 時再開）；`STBI_NO_STDIO`（讀檔歸 asset，adapter 只收位元組）；`STBI_NO_HDR`、`STBI_NO_LINEAR`（避開 `pow`，留在決定性邊界內）；`STBI_MAX_DIMENSIONS` 設上限 |
| 為什麼不走 SDL | Match、測試、工具載入不該需要 SDL（F2、F4）；一個外部組件一個 adapter（AGENTS §6） |
| 已知的風險 | stb 的 README 說明 stb_image 不以惡意輸入為設計目標，歷史上有多次針對畸形檔的缺陷回報〔知〕。今天的內容都是自己的 → 接受。**輸入來自不可信來源（玩家自製內容、網路下發的圖）成為 Feature 時，換 adapter**（wuffs、libspng；RC-IM1）。換的時候只改 `src/adapters/stb_image/` 一處 |
| 下載 | 要使用者同意（第 13 節） |

### 5.12 對 io 的要求（交給 threads、io 重審那批）

- 從 World 以外的執行緒讀整個檔，帶位元組上限：今天是 `engine/io/helpers/ReadAllBytes`。**要確認 `IFileSystem`／`Vfs` 在多執行緒同時讀時的契約**。
- 掛載點能用名字找到（`AssetPath` 的來源名）。
- 檔案監看（`IFileWatcher`）留給熱重載，本批不用。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature 需要**：F1～F5、F8、F9（第 3.2 節）。asset 是它們載入內容的唯一入口；使用者決定 1～3。
2. **現在的問題**：
   - `gyo2` 沒有 asset；舊設計（target `assets` DI 2、3）建立在已拿掉或正在拿掉的 (c) 區、Loading 的 Ingress stage 與 Host 上。
   - master 的 `AssetManager` 是總管物件、在主執行緒輪詢解碼，貼圖綁 SDL3_image。
3. **變化的邊界**：
   - 新 Subsystem `engine/modules/asset`；
   - （第 1 題選 B 時）`engine/modules/image`；
   - 新第三方 `third_party/stb_image`；
   - target 文件：舊 `target/assets.zh-Hant.md` 的 DI 1～3 與 RC 由新頁取代。
4. **影響**：
   - 編排層：登記一個 Module；不改編排層的程式，靠 O-10 帶過去、O-5 啟動報告。
   - 交換點：第 5.5 節的需求。
   - 之後的 render、text、ui、audio、model Module：它們的 loader 登記到 asset。
   - 產品與工具：載入改經 asset 或直接呼叫 loader。
5. **Dependency Direction**：
   - asset → base、kernel、orchestration、io（往下），日後可能加 threads。
   - image → asset、base。
   - 沒有反向；core 不依賴 asset；編排層不認得 asset（RC-O4、RC-O8）。
6. **Ownership**：
   - 內容的請求、身分、生命週期 → asset。
   - 格式 → 擁有型別的 Module。
   - 第三方解碼 → 該 Module 的 adapter。
   - **內容身分的比對、「載完才開始」 → 作者**（從舊 DI 3 的「asset＋Loading」移出）。
   - GPU 資源 → render。
7. **為什麼不能更小**：
   - 沒有 asset 的話，每個 Module 與產品各自讀檔、各自管生命週期與錯誤，正是 master 之前的重複。
   - 拿 master 的 `AssetManager` 過來改，是在為舊形狀找位置（`target/README.md`〈每批事前宣告的檢查項〉），11-11 已不採。

## 7. Design Invariants（新頁 `target/v2/asset.zh-Hant.md`，見出し `Design Invariants`）

**舊 → 新的系譜**：

| 舊（11-13 核准） | 新 | 變了什麼、為什麼 |
|---|---|---|
| DI 1 沒有總管物件，三類 Resource 各在它們的 World | **AS-1**（保留） | 只把 GPU 資源的那一類標明歸 render |
| DI 2 載入是外部服務；經 Loading 的 Ingress 進 (c) 暫存，由 (c) System 依請求序號的**連續前綴**套用 | **AS-4**、**AS-5** | (c) 區、Ingress stage 拿掉了（第 4 批、`-build-world-phase-is-mechanism`）。「IO 不碰 World、經交換點」保留（AS-4）；「依連續前綴」改成「身分在請求時給＋批內照序號、不等前面的」（AS-5；**第 12 節第 2 題**） |
| DI 3 內容身分是 (c)，由 (c) System 從暫存的位元組算；世界內容在 Loading 宣告 ready 前載完 | **AS-6**、**AS-7** | 內容身分本來就是產品的概念（§4.9 算在轉出的值上）；「ready 前載完」是遊戲流程，歸作者。asset 給機制（請求順序決定 id、同位元組同值、「全部完成」的查詢），不給規則（**第 12 節第 3 題**） |
| — | **AS-2**、**AS-3**、**AS-8**、**AS-9**（新） | 使用者決定 1、3；11-11 方向 3；異常分級；AGENTS §8 |

**新的 DI**（守衛欄的「延後」照 `v8-arch-gyo2-guards-after-mechanism-stable`：綁實作方式的掃描守衛，等 scenario test 通過再立，這段期間靠本頁與審查）：

- **AS-1 沒有總管物件。**
  - 請求狀態 `AssetRequests` 與每個型別一份 `Assets<T>` 是 Resource，在請求它的 World；GPU 資源是 render 的 Resource，不歸 asset。
  - System 以 Handle 或 id 存取，不持指標。沒有跨 World 的 asset 物件。
  - 守衛：行為測試（兩個 World 各自請求同一路徑，各自有自己的 id 與值）；掃描延後。
- **AS-2 核心不認得格式。**
  - asset 核心不解析任何檔案格式；文字、二進位這兩個 loader 是例外，它們不解析，只驗證 UTF-8 或原樣交出。
  - 格式由擁有型別的 Module 登記 loader；第三方解碼器只在該 Module 的 `src/adapters/<組件>/`，私有 link。
  - 守衛：行為測試（沒有登記任何 loader 的 World 照樣建得起來，請求回 `NoLoader`）；掃描延後（asset 的 `src/` 不含入第三方）。
- **AS-3 loader 是可直接呼叫的純函式。**
  - 輸入是位元組、settings、路徑與上限；輸出是值或失敗。不碰 World，不讀時鐘，不持全域可變狀態。
  - 不建 World 也能呼叫；同樣的輸入呼叫兩次，輸出相等。
  - 守衛：行為測試（直接呼叫；兩次相等；在兩條執行緒同時呼叫）。
- **AS-4 IO 不碰 World。**
  - 讀檔與解碼在 World 執行緒之外。結果只在交換點進 World，由 asset 的 System 在一個固定的位置（預設 `Main`/`First`）套用；`Assets<T>` 的寫者只有它。
  - 守衛：行為測試（X-6 的同步模式＋真執行緒模式）。
- **AS-5 身分在請求時給，完成時序不改結果。**
  - `AssetId` 在請求時同步配發，只依這個 World 的請求順序。
  - 同一次套用的一批照請求序號排。任何完成順序都不改變三樣東西：`Assets<T>` 的內容、id 序列、一批之內的訊息順序。
  - （第 12 節第 2 題選 (b) 時，「一批」改為「連續前綴」。）
  - 守衛：等價測試（同一組請求，打亂完成順序，三者相同）。
- **AS-6 `AssetId`／`Handle` 只在一個 store 裡有效。**
  - 不跨行程，不進權威的 digest，不進存檔。跨行程的參照用 `AssetPath` 或作者自己的內容鍵。
  - 守衛：本頁與審查（作者的責任，同 O-10）。
- **AS-7 「何時載完」與內容身分不是 asset 的規則。**
  - asset 給狀態、訊息、「全部完成」的查詢，以及同位元組同值（AS-3）。
  - 要不要等、內容身分怎麼算怎麼比，屬作者或網路 Module；用 Kernel 的逐型別雜湊客製點，對已載入的值算。
  - 守衛：本頁與審查；範例測試（作者對 `Assets<T>` 的值算雜湊）。
- **AS-8 錯誤只回報。**
  - 每筆請求的失敗是 E2 `AssetLoadFailure`，在狀態與訊息裡；登記錯誤是啟動錯誤；丟棄是 E3 記錄。
  - asset 不寫日誌、不重試、不讓 World 中斷。
  - 守衛：行為測試（各碼）；件數的突き合わせ（送出＝套用＋失敗＋丟棄）。
- **AS-9 介面和格式的來源分開。**
  - 選 loader 用（`T`、副檔名或格式標記）。來源是 io 的掛載點。
  - 加一種格式（含日後的引擎格式）＝登記一個 loader，不改請求的 API 與 asset 核心。
  - 守衛：行為測試（同一個 `T` 兩個 loader、兩個來源）。

### Replacement Criteria（寫在積上去之前）

符合任一條時，停下來交使用者「修還是重建」：

- **RC-AS1**（舊 RC 改寫）：出現必須在遊戲進行中串流載入、**而且結果進判定資料**的內容（例：BigMap 的 region 在對局中進碰撞）。這時「進判定前載完」（AS-7、5.8 第 5 點）要重審，參考 lightyear、Bevy 的串流模型；不在 `Assets<T>` 上加特例。
- **RC-AS2**：每加一種格式都要改 asset 核心，或某個 loader 需要碰 World 才寫得出來。
- **RC-AS3**：同一內容在兩個以上 World 重複解碼，量測超出預算（第 3.3 節的數字定了之後）；或同一個 Product 裡出現第二份跨 World 的 asset 狀態。
- **RC-AS4**：兩個以上的 loader 需要在解碼中途再要別的 asset，而產品的 System 依序請求表達不了；或熱重載成為點名的 Feature。這時才做 `LoadContext` 的依賴與 `Modified`。
- **RC-AS5**：交換點交回結果時，編排層或交換點需要知道 asset 的型別或語意（和 RC-O8 同類）。
- **RC-AS6**：事先轉檔的路回來時，請求的 API 或 Handle 必須改（違反 AS-9 的承諾）。
- **RC-IM1**（image Module 的 DI 節）：不可信來源的圖成為 Feature，或 stb_image 的缺陷影響到我們的內容，就換 adapter。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

每一列都先問「這是不是在為舊形狀找位置」（`target/README.md`〈每批事前宣告的檢查項〉）。

| 選擇 | 採用 | 系譜 | 退掉的 | 決勝理由 | 需要它的 Feature |
|---|---|---|---|---|---|
| 形狀 | 請求狀態＋每型別 `Assets<T>`＋Handle | Bevy（11-11 方向）；新推導 | master `AssetManager` 總管物件；「`AssetManager` 放哪個 World」（舊形狀找位置，11-11 不採） | 拆開後每個 World 只持自己的；沒有跨 World 指標 | F1、F5 |
| 身分 | 路徑（`AssetPath`＋io 掛載點） | Bevy | master 的 catalog id（`asset_catalog.json`）；Unity 的 GUID | 核心不認得 catalog 格式（AS-2）；catalog 是產品／工具的內容，可以先讀它再照路徑請求；GUID 要 build pipeline，工具還沒回來 | F1、F6 |
| 結果的順序 | Handle 在請求時給＋批內照序號、不等前面的（**第 12 節第 2 題**） | 新推導（舊 DI 2 的連續前綴＋Bevy 的請求時配 id） | (b) 連續前綴（舊 DI 2）；(c) 照完成順序（Bevy） | (b) 一個大檔卡住後面所有內容（23.7 MB 的 fbx 擋住 1 KB 的 json），而它保證的「內容身分」已不歸 asset；(c) 批內訊息順序隨執行緒時序變，測試不可重現 | F1、F2 |
| 內容身分 | 作者對已載入的值，用 Kernel 的客製點算（**第 12 節第 3 題**） | 新推導；權威＋回滾宣告 3.2、4.4 | asset 從原始位元組算（舊 DI 3） | 舊的 digest 本來就算在轉出的值上（§4.9）；一個概念一個實作（Kernel 的客製點）；單機遊戲不必付代價 | F2、F9 |
| 「載完才開始」 | 作者在生成世界用「全部完成」決定 | `-build-world-phase-is-mechanism` | Engine 規定 ready 前載完（舊 DI 3、runtime-composition DI 3） | Loading 是遊戲流程的名字，不是機制 | F1、F8、F9 |
| 套用的位置 | `Main`/`First` | Bevy 的 `PreUpdate`，往前移一格 | `PreUpdate`（Bevy 原樣） | 回滾的獨占 System 在 `PreUpdate`；`First` 讓一幀之內含重跑都不變 | F2 |
| 多 World 共用 | 不共用，各自請求 | 新推導 | Product 層共用快取 | 共用快取就是跨 World 的總管（11-11 不採）；沒有量測說重複解碼是問題 → RC-AS3 | F1 |
| 替換 World | 預設帶過去，產品可關 | O-10 的機制 | 不帶（每次讀檔重讀） | F3 讀檔不重讀同一關；`engine-not-rigid` 預設方便、可關 | F3 |
| 錯誤 | 狀態裡的 E2＋`Message`；不記日誌 | 〈Error levels〉 | asset 自己寫 `Warning`（master） | 「Engine 的 Module 回傳錯誤，不自己寫日誌」 | F1、F5 |
| loader | 純函式、直接可呼叫 | Bevy `AssetLoader`（但 Bevy 的 `load` 帶 `LoadContext` 可再要依賴） | 帶 `LoadContext` 依賴（Bevy 原樣）；master 的 `IAssetLoader`（帶 `AnyAsset` 型別抹除） | 工具與測試直接呼叫（F4）；依賴載入沒有 Feature（RC-AS4）；型別化的 `T` 讓 `Assets<T>` 不需要型別抹除 | F4、F6 |
| 圖片解碼 | stb_image，在一個 adapter | 使用者決定 1 | SDL3_image（master）；libspng、wuffs | 使用者已定；不需要 SDL（F2、F4）；單一標頭最輕 | F1、F4、F5 |
| 圖片放哪裡 | **第 12 節第 1 題** | Bevy 的 `bevy_image` | — | 見第 12 節 | F1、F5 |

**沒有用來決勝的**：既有的 Test、守衛、golden 不必改。`gyo2` 上沒有 asset 的測試；master 的 `tests/common/asset*` 不帶。

## 9. thin start 與實作的步驟（動工前核准、交換點合併、使用者說開始之後）

**實作的**（名指的 Feature 直近要用的）：
- AS1～AS7、AS9；
- 第 1 題選 B 時再加 AS8 的 PNG 解碼。PNG 是 F1、F5 直近要用的唯一圖片格式：master 的 5 張全是 png，沒有 jpg。

**只定形狀、不實作的**：
- loader 介面上留給依賴載入的位置（`LoadInfo` 日後可以加，RC-AS4）；
- `AssetMessage<T>::Modified`（熱重載）；
- 子 asset 的路徑語法（`#label`）；
- 引擎格式的 loader（5.10）。

| 步 | 內容 | 模型／檔位 | 上段或下段 |
|---|---|---|---|
| 第 1 步 | Module 骨架、登記列、`AssetPath`、`AssetId`／`Handle` 的配發與計數（不碰 World 的部分先做、先測） | Opus／high | 上段（公開介面） |
| 第 2 步 | `AssetRequests`、`Assets<T>`、去重、放掉、狀態與訊息；X-6 的同步模式 | Opus／high | 上段 |
| 第 3 步 | 接交換點（照它的宣告）、`First` 的套用 System、真執行緒模式；AS-5 等價測試 | Opus／high（時序的部分若檢查認為要，先問再升 xhigh） | 上段（執行緒、決定性） |
| 第 4 步 | 帶過去（O-10）、讀檔案例、丟棄記錄、錯誤各碼 | Opus／high | 上段 |
| 第 5 步 | 文字、二進位 loader；（B）image Module＋stb adapter；第 13 節的下載 | Opus／high；下載與 CMake 的機械部分 Sonnet／medium | 上段，機械部分是 low 的定型作業 |
| 第 6 步 | scenario test、突變、新頁、dev_log；審查 | 實作 Opus／high；審查 Fable／high（和作者不同的模型） | 上段 |

## 10. 測試、判定（先宣告，結果出來之後不放寬）

**scenario test（第 6 步，沿遊戲流程）**：
1. 生成世界請求 3 種型別、5 筆。其中一筆檔案不在、一筆超過上限。
2. 跑到「全部完成」：3 筆 `Loaded`、2 筆 `Failed`，碼分別是 `NotFound`、`TooLarge`。
3. 讀檔＝`ReplaceWorld`（`BuildNewFirst`）。新 World 再要其中 2 筆，命中、不再讀（讀檔次數的計數不增加）；沒再要的 1 筆在舊 World 銷毀後被放掉，發 `Removed`。
4. 件數的突き合わせ：送出 5＝套用 3＋失敗 2＋丟棄 0。

**等價測試（AS-5）**：同一組 20 筆請求，以 X-6 的同步模式給 3 種固定的完成順序（正序、反序、交錯）。`Assets<T>` 的內容、id 序列、一批之內的訊息順序都相同。

**直接呼叫（AS-3、F4）**：不建 World 呼叫文字與 PNG loader；同樣的輸入兩次輸出相等；兩條執行緒同時呼叫。

**接得上事先轉檔（AS-9）**：
- 同一個 `T` 登記兩個 loader（`.txt` 與測試用的 `.gyotxt`），請求 API 不變；
- 兩個掛載點各一個來源；
- 同一個（`T`、副檔名）登記兩次 → 啟動錯誤。

**決定性（5.8）**：PNG loader 對固定的測試圖，輸出的像素的 SHA-256 等於字面值，在 L1 三平台（linux-x64、windows-x64、macos-arm64）一致。

**突變**：每條 DI 的守衛各至少一個專屬的預期失敗（AGENTS §14.2、§14.5；`tests/common/mutation/README.md`）；清單在實作時列，受別模型的敵對式檢查。

**判定**：上面全部通過；L1 三平台通過；突變全部 killed。

**量測**：載入時間、記憶體只做探索量測，標「不是判定」，等第 12 節第 4 題。

## 11. 卡片照 L17 判的（不是使用者的話）

以下是照命名規範、別家原始碼、已定原則判的，條列在這裡讓使用者看得到：
- 位置與名字：`AssetRequests` 不叫 `AssetServer`（5.1）。
- `default=yes`（5.1）。
- 身分用路徑、不認得 catalog（5.2）。
- 去重的鍵、沒人用就放掉、不做快取策略（5.2）。
- 每個 World 各自、不共用（5.3）。
- 替換 World 預設帶過去（5.3）。
- 套用在 `First`（5.5）。
- 用 `Message` 不用 `Event`（5.6）。
- 錯誤碼與級別的對應（5.7）。
- loader 例外轉 E2（5.7）。
- `AssetPath` 的雜湊接 Kernel 的客製點（5.8）。
- stb_image 的編譯設定（5.11）。
- 延後的四件：熱重載、依賴載入、子 asset、共用快取（2.2、RC-AS4）。

## 12. 交使用者的

只列會推翻使用者之前的決定、別家分歧且真有取捨、職責清單要多一項的事。

### 第 1 題：圖片的型別與 stb adapter 放哪個 Module

**為什麼問**：主會話的背景寫「貼圖隨 SDL 批的 render 回來（照 Bevy 的 `bevy_image`）」。但查 Bevy 原始碼，`bevy_image` 是**獨立的 crate**：只依賴 `bevy_asset`，不依賴 `bevy_render`；render、ui、text、sprite 都依賴它。兩種讀法會導向不同的放法。

| 選項 | 內容 | 好處 | 代價 |
|---|---|---|---|
| **(B) 獨立的 `engine/modules/image`（建議）** | CPU 端的 `Image`（尺寸、像素格式、像素）＋loader＋`src/adapters/stb_image/`；render、text、ui 依賴它；本批第 5 步做 PNG | 照 Bevy 的實際切法；ui 排版要圖片尺寸、Match 或工具要像素時不依賴 render；stb 的唯一 adapter 不受 SDL 那批的時程影響；用真的第三方解碼器在積上去之前驗證 loader 介面（settings、上限、解碼錯誤）。文字、二進位驗證不到這些 | 多一個 Module；本批多 3～5 h |
| (A) 放在 render Module 的 adapter，隨 SDL 那批 | 主會話背景的讀法 | 少一個 Module | ui、text 要圖片尺寸時得依賴 render；loader 介面要等 SDL 那批才有真的第三方來驗 |

**不受影響的**：兩個選項都滿足使用者決定 1（不走 SDL、只在一個 adapter）。

### 第 2 題：完成的結果照什麼順序進 `Assets<T>`（改舊 DI 2，11-13 核准過）

| 選項 | 內容 | 例子 |
|---|---|---|
| **(a) 身分在請求時給；每次套用的一批照請求序號排、不等前面的（建議）** | id 與 `Assets<T>` 的內容不依完成時序；一批之內的訊息順序可重現；大檔不擋小檔 | 23.7 MB 的 fbx 還在解碼，同時請求的 1 KB json 下一幀就能用 |
| (b) 照舊：只套用連續完成的前綴 | 套用的順序也和請求順序完全相同 | 那個 json 要等 fbx 解完才進來 |
| (c) 照完成順序（Bevy） | 最簡單 | 一批之內的訊息順序隨執行緒時序變；測試不可重現 |

**別家**：Bevy 是 (c)，但 id 在請求時就給（`server/mod.rs:364`），所以 `Assets<T>` 的內容不受影響，受影響的只有訊息順序。

**(b) 的原意**：「內容身分 (c)＋決定性」，第 3 題把它移出 asset 之後，(b) 的代價（大檔擋小檔）就沒有對應的收益了。

### 第 3 題：「載完才開始」與內容身分移出 asset（改舊 DI 3，11-13 核准過）

**舊的**：asset 從暫存的位元組算內容身分；世界內容在 Loading 宣告 ready 前全部載完（Engine 的規則）。

**建議**：兩件都改成作者的事，asset 只給機制：
- 請求順序決定 id；
- 同位元組同值；
- 「全部完成」的查詢；
- `AssetPath` 的雜湊接 Kernel 的客製點。

**根據**：
- `-build-world-phase-is-mechanism`：Loading 是遊戲流程；
- `determinism.md` §4.9：舊的 digest 算在轉出的值上，本來就是產品的概念；
- 第 4 批：digest 用 Kernel 的逐型別雜湊；
- F9：2D 單機不該付代價。

**代價**：判定用的內容「進判定前載完」變成作者的責任（5.8 第 5 點、RC-AS1）。機制不擋，因為第 4 批之後 Engine 不認得判定與呈現的分法。

### 第 4 題：預算

**寫不出數字的三項**：
- F1 全部內容冷啟動的載入時間上限；
- 同時在飛的請求或解碼中的記憶體峰值；
- 每幀套用的上限。

**選項**：
- (a) 現在給數字；
- (b) 延後到第一個產品搬回時再定（建議）：實作段只做探索量測、標「不是判定」。

### 第 5 件（同意，不是選擇）：下載 stb_image

在實作第 5 步，從 `https://github.com/nothings/stb` 釘一個 commit 下載 `stb_image.h`（單一標頭），只放進建置目錄的 FetchContent 快取，同 yyjson。**現在不下載**；動工前核准時一起同意，或到第 5 步再問。

## 13. 外部組件的下載

| 組件 | 來源 | 用途 | 授權 | 何時 |
|---|---|---|---|---|
| stb_image（`stb_image.h`） | nothings/stb，釘 commit | image Module 的 PNG 解碼 | MIT 或 Public Domain | 實作第 5 步，要使用者同意 |

## 14. 範囲の限界

- **交換點還沒有宣告**：第 5.5 節是需求，不是設計；實作的第 3 步照它的宣告。如果它滿足不了 X-1～X-7，先回報，不在 asset 裡繞過。
- **第 4 批還沒合併**：逐型別雜湊客製點的介面照修訂 5 第 3.2 節。合併時的形狀不同的話，5.8 第 3、4 點跟著改。
- **預算未定**（第 12 節第 4 題）：沒有載入時間與記憶體的判定。
- **loader 的跨平台逐位元**：只對 PNG（stb，整數路徑）宣告判定；其他 loader 由各自的 Module 宣告。
- **「進判定前載完」靠作者**（5.8 第 5 點）。
- **io 的多執行緒讀的契約**未確認（5.12），交 threads、io 重審那批。

## 15. 觀察到、不在本批的（報給主會話）

1. 批次表第 6 組「asset」一列的依賴已更正（含交換點）。第 12 節第 1 題選 B 時，SDL 那批的範圍要記「image Module 已由 asset 批帶來，render 依賴它」。
2. `target/runtime-composition.zh-Hant.md` §3a 與 DI 3（Loading 的 Ingress、資產 stage）、`target/README.md` 的 K.3 表 A3、A4 列，都建立在已拿掉的前提上。本批只動 `assets.zh-Hant.md` 的頂端標記，其餘歸「v2 之前的 target 文件由主會話一次改」那一項。
3. 打包時附第三方授權告示：stb_image 加進「之後的小事」那一條（yyjson、fast_float 起）。
4. HANDOFF〈交使用者的〉「SDL 的 adapter、ufbx 歸哪一類」仍未定；本頁照 JSON 的先例寫 ufbx 進 model Module 的 adapter，只當背景，不判。

## 16. 停止條件

- AGENTS §14 冒頭的三條。
- 交換點的宣告與第 5.5 節的需求衝突。
- 第 4 批合併後雜湊客製點的形狀和 5.8 的假設不同，而且影響 AS-6、AS-7。
- 實作中發現某個 loader 必須碰 World（RC-AS2）。

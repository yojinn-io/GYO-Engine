# 事前宣告：asset Module（`v8-arch-gyo2-asset`）

更新：2026-11-21（修訂 1）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：等動工前核准。實作要等第 5 批的「多個 World、交換點」與「生成世界（重做）」做完，而且使用者說開始。**

**經過**：修訂 0（f93edb7e）送 Fable 5.2／high 敵對式檢查（general-purpose、只讀），結果 blocker 4、should 15、nit 12，證據 `declaration-r0-adversarial-check.md`。修訂 1 照它全部處理，對照在第 17 節。修訂 1 沒有再送檢查。

**性質**：實作段第 6 組的 Module 的事前宣告。涉及：
- AGENTS §2 的基盤 Architecture；
- §3 的 Architecture Delta：新 Subsystem、改寫已核准的 Design Invariants；
- §4 的系譜與決勝理由；
- §6 的外部組件 adapter；
- §8 的 Data Contract。

**用詞**：照日誌、trace 的宣告，即設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。交使用者的只有第 12 節。

**核心一句**（每一項都拿來對照）：asset 是**機制**。它告訴外面三件事——要的東西載到哪了、成了沒有、為什麼沒成——並把載好的值交給 World。

asset **不決定**以下四件事：

| 不決定的事 | 歸誰 |
|---|---|
| 要不要等載完 | 作者 |
| 失敗後怎麼辦 | 作者 |
| 內容身分怎麼比 | 作者或網路 Module |
| 檔案是什麼格式 | 擁有那個型別的 Module |

## 0. 根據與讀的版本

**程式**：`gyo2` f25837b0（#258 trace 之後）。本頁的行號都照這個版本。

**使用者的決定**（主會話轉來；HANDOFF 在文件分支 `origin/claude/gyo2-time-design`）：

| 代號 | 內容 | 本頁怎麼用 |
|---|---|---|
| `v8-arch-gyo2-asset-three-decisions`（2026-11-21） | 原話「asset 三件照建議」：(1) 圖片解碼不走 SDL，先用 stb_image，只在一個 adapter；(2) DI 照新前提重寫；(3) 起步照 Bevy 執行期讀原始檔，loader 介面接得上日後的事先轉檔 | 第 5.11、7、5.10 節 |
| `v8-arch-gyo2-parallel-asset-and-stale-mutants`（2026-11-21） | 舊 DI 2 要「IO 執行緒加交換點」；舊 DI 2、3 用 (c) 區與 Ingress stage；批次表的依賴更正為含交換點 | 第 2.2 節（實作的依賴） |
| `v8-arch-assets-bevy-model`（2026-11-11，kernel HANDOFF:144） | `AssetManager` 不選 World，照 Bevy 拆成三類：請求狀態、每個型別一份 `Assets<T>`、GPU 資源；loader 由各 plugin 登記 | 第 5.1～5.4 節（方向不變） |
| `v8-arch-target-di-approved`（2026-11-13） | 舊 DI 1～3 與 Replacement Criteria 核准 | 第 7 節的系譜 |
| `v8-arch-gyo2-build-world-phase-is-mechanism`（2026-11-16） | 「開局、Loading、讀檔」是遊戲流程的名字，不是機制；有沒有 Loading 階段由作者決定 | 第 5.8 節、第 12 節第 3 題 |
| `v8-arch-gyo2-save-unaware-means-format-only`（2026-11-16） | Engine 不認得存檔的格式 | 第 5.9 節 |
| `v8-arch-gyo2-module-plugin-owner-definition`、`v8-arch-gyo2-json-adapters-dir` | Module＝我們維護、介面是我們的；第三方放該 Module 的 `src/adapters/<組件>/` | 第 5.1、5.11 節 |
| `v8-arch-gyo2-engine-not-rigid`、`-mechanism-not-conservative` | 預設方便、不禁止別的做法；建機制這一輪不選保守的選項 | 第 5.3、5.4 節 |
| `v8-arch-gyo2-infrastructure-before-products`、`-migration-needs-consent` | 先建基礎；搬產品要使用者同意 | 第 3.2 節的 Feature 只用來推需求，不搬產品 |
| `v8-arch-gyo2-replace-order-author-choice` | 使用者點名 BigMap（master `plans/big-map/`）是有的 Feature | 第 3.3 節的規模 |

**已合併、本頁要對齊的**：

- [編排層](orchestration.zh-Hant.md)：
  - O-2 生命週期
  - O-5 啟動報告
  - O-6 登記檔
  - O-7 準備時機
  - O-8 替換（「作者在發出 `ReplaceRequest` 之前驗證」）
  - O-10 帶過去
  - O-11
- `target/error-handling.md`〈Error levels〉：`…Error`＝E1，`…Failure`＝E2；Engine 的 Module 不寫日誌。
- [事件游標](event-cursor-declaration.zh-Hant.md)：`Message`；上限預設 1024；丟棄記錄由 runner 在兩次 Run 之間取走。
- Kernel 的 `DiagnosticsWriter<Record>`（`engine/ecs/kernel/include/engine/kernel/Params.hpp:709`）。
- [時間＋事件](time-events-declaration.zh-Hant.md)第 3.6 節：預設骨架 `Main`（`First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）。
- 先例：「不認得 ECS 的函式庫＋World 端的 Module」兩個單位的拆法——`logging`／`world_logging`、`time`／`world_time`（`config/modules.csv`）。

**進行中、本頁要對齊的**：

- **第 4 批**（draft #259）：分支 `claude/gyo2-authority-rollback-bda9ba` 的宣告 `authority-rollback-declaration.zh-Hant.md` **修訂 6**（07c80c4c）。
  - 第 3.2 節：雜湊客製點。
  - 第 3.3 節：Engine 的 Module 放進 World 的型別都附雜湊函式；有值才報。
  - 第 4.4 節：digest 不含 id。
  - 第 21 節：到達時刻要影響判定時，走 `rollback.Input<T>`。
- **第 5 批**：交換點、生成世界（重做）都還沒有宣告。第 5.5、5.13 節寫成給它們的需求。

**參考，不帶**：

- master 的 `engine/asset`：`AssetManager`、`AssetCatalog`、`hot_reload/`、`loaders/`、`sdl_image/SdlImageTextureLoader`。
- 舊的 [target/assets.zh-Hant.md](../assets.zh-Hant.md)：11-11 的方向與 11-13 的 DI。
- `target/runtime-composition.zh-Hant.md` §3a 與 DI 3：Loading 的 Ingress、資產 stage。
- 藍圖的資產不變量 A1～A6：`plans/kernel/blueprint.zh-Hant.md:28`、`:66`；`rebuild-concept.zh-Hant.md` X17「各能力設計時再判」、E10i「A3 照帶」。逐條判在第 5.14 節。

**證據**：放在主 checkout 的 `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-asset/`，git 不追蹤。

- `reference-survey.md`：別家原始碼的檔案、行號與版本；master 的內容種類與大小。
- `declaration-r0-adversarial-check.md`：修訂 0 的檢查報告。

---

## 0.1 時間估計

| 段 | 估計 | 內容 |
|---|---|---|
| 設計段 | 4～6 h（到修訂 1 約 4.5 h） | 讀現況、對照別家、寫本頁、Fable 檢查與修訂 |
| 實作段 | 22～34 h（推定） | 第 9 節的 6 步。交換點與生成世界的形狀還沒定，所以只能推定；它們的宣告核准後重估 |

超過 1.5 倍（設計段 9 h、實作段 51 h），或單一問題累計超過 2 小時，就停下回報（AGENTS §14 冒頭）。

## 1. 這一批做什麼（先用遊戲講）

FPS 的 Client 進對局之前，要載：
- 角色模型（fbx）
- 武器貼圖（png）
- 字型（ttf）
- 音效（wav）
- 規則與地圖的 JSON

步驟：

1. 生成世界階段，作者的 System 對 `AssetRequests` 說：「我要 `rules/movement_rules.json`，當成文字。」**當場**就拿到 `Handle<Text>`。
2. 讀檔與解碼在 World 的執行緒之外做，不碰 World。
3. 結果在交換點交回 World。
4. asset 的套用 System 把結果放進 `Assets<Text>`，並發一則「加入了」的 `Message`。
5. 作者的 System 看 `AssetRequests` 的狀態。全部載完才離開生成世界——等不等是作者的事。

**失敗時**：例如檔案不在，狀態就是「失敗」，帶 `AssetLoadFailure`（E2）。
- asset 不記日誌、不自己重試，World 照跑。
- 作者決定：停下、再請求一次、換預設內容，或顯示錯誤畫面。
- 日誌要記的話，訂閱那則不分型別的 `AssetLoadFailed`。

**中途讀檔時**，照 O-8「先驗證再替換」：

1. 作者先在**現在的 World** 請求存檔要的內容，等到全部 `Loaded` 或 `Failed`。
2. 沒問題才發 `ReplaceRequest`。
3. 新 World 在準備函式或 `beforeSwap` 裡再請求同一批。
4. asset 的 Resource 預設帶過去。舊 World 這時還在，它的 Handle 讓內容活著，所以新 World 的請求直接命中、不重讀。

**模型檢視器的 `info` 指令**：不建 World，只連不認得 ECS 的 `asset` 函式庫與模型 Module 的 loader；把位元組交進去，拿回值。

## 2. 範圍

### 2.1 這一批做的（動工前核准之後；實作等第 5 批）

| 代號 | 內容 |
|---|---|
| AS1 | 兩個單位（照 `logging`／`world_logging` 的先例；卡片照 L17 判，第 8 節）：**`engine/modules/asset`**（`GYO::Asset`，library-only，不認得 ECS）——`AssetPath`、loader 介面、`LoaderFailure`、整檔讀取（經 io）、文字 loader；**`engine/modules/world_asset`**（`GYO::WorldAsset`，編排層 `Module` 的子類，進 World）——其餘 |
| AS2 | `AssetRequests`、每個型別一份 `Assets<T>`、`Handle<T>`／`AssetId<T>`。身分在請求時配、引用計數、沒人用就放掉、`Failed` 之後可以再請求 |
| AS3 | loader 的登記入口：擁有型別的 Module 在 `Build` 登記 |
| AS4 | 讀檔與解碼的工作（job）在 World 執行緒之外跑；結果經交換點交回，由套用 System 放進 World。**怎麼交回照交換點的宣告**；第 5.5 節是需求 |
| AS5 | 狀態查詢、每型別的 `AssetMessage<T>{Added, Removed}`、不分型別的 `AssetLoadFailed`、「全部完成」的查詢 |
| AS6 | 錯誤：`AssetLoadFailure`（E2）、登記時的啟動錯誤、丟棄記錄（E3） |
| AS7 | asset 放進 World 的型別的雜湊函式（第 5.8.1 節；第 4 批 3.3） |
| AS8 | 上限與擴張點：每幀套用上限、在飛上限、loader 的位元組上限可由產品覆寫（第 5.4、5.5 節） |
| AS9 | 新頁 `target/v2/asset.zh-Hant.md`：Design Invariants、Replacement Criteria。舊 `target/assets.zh-Hant.md` 頂端標「已由 v2 取代」 |

### 2.2 不在這一批

- **事先轉檔的工具與它的資料契約**（使用者決定 3）。留給工具回來時；本批只保證 loader 介面接得上（第 5.10 節）。
- **image Module 與 stb_image**：第 12 節第 5 題建議 (C)，隨 SDL 那批的第一個使用者（render、ui）做。本頁仍先定它的位置、授權與 adapter（第 5.11 節）。選 (B) 時移進本批。
- **其他 loader**：
  - 模型（ufbx）：隨產品搬回，進 model Module。HANDOFF〈交使用者的〉「SDL 的 adapter 與 ufbx 歸哪一類」還沒定，本批不判。
  - 音效（wav）：歸 audio Module，在 audio 重審那批做。
  - 字型（ttf）：歸 text Module，隨 SDL 那批做。
  - 二進位：沒有點名的 Feature。
- **GPU 資源**（Bevy `RenderAssets<T>`）：歸 render，隨 SDL 那批（K.3-6 的渲染 World＋Extract）。
- **沒有點名 Feature 的，本批不做，也不在介面上先留形狀**（第 3.2 節末段、RC-AS4）：
  - 熱重載與 `Modified`。model_viewer 的 `RefreshContent`（`ViewerWindow.cpp:227`）是手動重讀；工具可以直接呼叫 loader，產品可以放掉 Handle 再請求。
  - 載入中帶依賴。
  - 一個檔分出多個子 asset。
  - Product 層共用的快取。
- **master 的 `AssetCatalog`（邏輯 id → 型別與路徑）**：不進 asset 核心。它是工具與遊戲共用的 Data Contract，**owner 未定**（第 12 節第 1 題、第 15 節第 5 點；AGENTS §1：owner 不明，不在 Feature 的作業裡定）。
- 交換點、生成世界本身的設計（第 5 批）。
- threads、io 的重審（批次表第 6 組）。本批只把 asset 對 io 的要求交過去（第 5.12 節）。

**實作的依賴**：
- 編排層（#244）；
- **第 5 批的多個 World、交換點**；
- **第 5 批的生成世界（重做）**；
- 第 4 批合併：雜湊客製點、`Summary` 的規則。

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

上面會積的東西：
- render、text、ui、audio、model 的 loader 與 GPU 上傳；
- 每個產品的生成世界；
- 讀檔；
- 工具的資料契約（事先轉檔）；
- 內容與存檔裡記的資產參照。

以下幾樣一改，上面每個 loader、每個產品、甚至**已存的內容與玩家的存檔**都要跟著改：
- Handle 的生命週期；
- 持久的參照鍵（第 5.2 節）；
- `Assets<T>` 在哪個 World；
- 結果怎麼交回 World。

### 3.2 予見的根據 → 對 asset 的要求（Feature → Contract → Architecture）

**Feature 的出處**：master 的產品 `apps/object_fps_pvp`、工具 `tools/{model_viewer,voxel_map_editor,audio_editor,ui_editor}`、內容 `assets/object_fps_pvp`。逐檔與行號在 `reference-survey.md` 第 2 節，敵對式檢查抽查屬實。

這裡只用來推需求，**不搬產品**（`-migration-needs-consent`）。

| # | Feature（誰用、看得到什麼） | 用到的內容種類（master 實查） | 對 asset 的要求 | 本批 |
|---|---|---|---|---|
| F1 | 遊戲 Client：進對局前載完角色、武器、字型、音效、UI、地圖 | fbx 4、png 5、ttf 1、wav 4、json 22（`PvpApplication.cpp:958-962` 登記五種 loader） | 多種型別各自的 loader；請求當場拿到 Handle；查「全部完成」；失敗帶原因 | 機制做；fbx／png／wav／ttf 的 loader 不做 |
| F2 | 遊戲 Match（無畫面）：載角色模型與規則算判定（`PlayerAnimationContent.cpp:89-90`） | fbx、json | **不需要 SDL 與視窗**也能載；兩端載出的內容身分由作者比 | 機制做；比對歸作者（5.8） |
| F3 | 讀檔（`-restore-entry-scope-player-save`）：遊戲中讀檔＝替換 World | 同 F1 | 替換之前能先驗證內容（O-8）；替換時不必重讀同一關的內容；存檔裡的資產參照跨版本有效 | 做（5.3、5.9）；持久鍵見第 12 節第 1 題 |
| F4 | 模型檢視器：`info`、`models` 指令不建 World（`ModelContent.cpp:66-69`、`:120` 直接用 loader）；`window` 看模型與貼圖 | fbx、png、wav | loader **不依賴 ECS**，可以直接呼叫；同一個 loader 也能登記進 World | 做（AS1 的拆法） |
| F5 | ui_editor 的預覽：顯示字型與圖片（`AssetPreviewContext.cpp:92-171`）；UI 文件用 catalog id 參照資產（`UiDocumentBridge.cpp:137-165`） | ttf、png、json | 和 F1 同一個機制；Editor 的 World 也能載；**參照鍵**（第 12 節第 1 題） | 機制做 |
| F6 | audio_editor：`convert` 把 wav 轉成引擎格式的 clip、寫進 catalog（`tools/audio_editor/src/Main.cpp:29-36`，經 io 直接讀） | wav、json | **事先轉檔現在就有一例**：工具輸出、執行期讀，執行期的 loader 不知道誰產生的（AGENTS §8） | 只保證介面（5.10） |
| F7 | 地圖編輯器：讀寫自己的地圖檔（`voxel_map_editor/src/FileAccess.cpp:21-27`，經 io） | json、voxel mesh json | 編輯器的檔是它自己的；遊戲讀它的輸出時經 loader（資料契約） | 只保證介面（5.10） |
| F8 | BigMap（使用者點名，master `plans/big-map/`）：約 1 km²、區塊化地圖 242 MB、每個 region 的文字 ≤ 16 MiB | 地圖 region | 一個大內容能拆成許多獨立的請求；單一 asset 的上限可設；**同時在飛與每幀套用要有上限**（不然全部同時讀進記憶體）；**執行中串流進判定狀態**還沒決定 → RC-AS1 | 做上限與擴張點（AS8） |
| F9 | 2D 橫版遊戲（`-engine-scope-general`；架構師自建樣本） | png、json | 單機、不需要 PvP 的任何東西；不比摘要時不必寫任何雜湊函式 | 做 |

**推不出、不放進來的**：

- **熱重載**：master 有 `AssetWatcher`，但 `apps/`、`tools/` 沒有任何呼叫——`git grep AssetWatcher` 得 0 件（exit 1）；正例確認 `engine/asset` 有命中；`enableHotReload` 也是 0 件。
- **載入中帶依賴**：F1 的 viewmodel json 會引用 fbx 與 png，但 master 是產品自己依序載。
- **子 asset**：master 的 ufbx loader 一個檔出一個 `ModelAsset`。
- **Product 層共用快取**：沒有 Feature，也沒有量測。
- **二進位 loader**：沒有點名的 Feature。

### 3.3 規模與預算

**已知的規模**（master 實查）：
- 遊戲內容共 36 個檔、36.7 MB。最大的是 fbx 23.7 MB，其次是 ttf 4.5 MB、png 1.34 MB、json 1.45 MB。
- BigMap：242 MB，每個 region 的文字 ≤ 16 MiB。

**預算：寫不出數字，列為未決**（第 12 節第 6 題）。缺的是：
- 載入時間；
- 同時在飛的請求數或解碼中的記憶體峰值；
- 每幀套用的上限。

**本批先做放數字的位置**：每幀套用上限、在飛上限，都是 World 選項（AS8）。預設值是「不設限」，標明不是判定。數字等使用者給，或明說延後。

## 4. 別家怎麼做（同一種技術比）

**來源標記**：
- **〔源〕**＝本機原始碼查證。版本：Bevy `6a911653f`（0.20.0-dev）、Unity Entities 1.5.0、flecs 本機版。
- **〔知〕**＝一般知識，沒有原始碼可查。

| 家 | 核心認不認得格式 | 身分、去重、Handle | 結果怎麼進 World、順序 | 執行期讀什麼；持久鍵 | 圖片誰解碼 | 為什麼這樣做 |
|---|---|---|---|---|---|---|
| Bevy〔源〕 | 不認得。`AssetLoader` trait（`bevy_asset/src/loader.rs:32-49`：`Settings`、`Error`、`load`、`extensions`），`load` 收 `Reader`＋`LoadContext`、async，由各 crate 登記 | `load` 當場回 `Handle`（`server/mod.rs:364`）；**去重的鍵是（路徑、型別）**，同路徑第二次帶不同 settings 會被忽略，原始碼自己標成缺陷（issue 11111，`server/info.rs:265-271`）；`Failed`／`NotLoaded` 再請求會重新載（`info.rs:279-286`）；`Assets<A>` 是 dense storage＋`Uuid` 表（`assets.rs:288-306`）；強 Handle 被丟時經 channel 通知（`handle.rs:81-91`） | IO 在 `IoTaskPool`（`server/mod.rs:427`）；完成後送 Module 自己的 channel；`handle_internal_asset_events` 在 `PreUpdate` 以 `&mut World` **照完成順序**套用（`:1890-1895`、`lib.rs:423-433`）；`AssetEvent`（`Added`、`Modified`、`Removed`、`Unused`、`LoadedWithDependencies`，`event.rs:50-60`）在 `PostUpdate` 發；另有不分型別的 `UntypedAssetLoadFailedEvent`（`lib.rs:421`） | 預設 `AssetMode::Unprocessed`＝原始檔（`lib.rs:297-336`）；可選 `Processed`：`AssetProcessor` 事先轉，**輸出的路徑和原始檔相同**，怎麼載由 `.meta` 的 `AssetAction` 決定（`meta.rs:70-86`），用來源雜湊判斷要不要重轉（`processor/mod.rs:86-91`）；持久鍵＝路徑 | `bevy_image` 是**獨立的 crate**，用 `image` 函式庫；依賴 `bevy_asset`、`bevy_app`、`bevy_ecs`、`bevy_math`、`bevy_color`、`wgpu-types`，**不依賴 `bevy_render`**；render、ui、text、sprite 都依賴它（`bevy_image/Cargo.toml:59-74`） | 原始檔讓開發迭代快；事先轉檔不改路徑，所以內容的參照不受影響；圖片獨立，讓 ui、text 不依賴 render 也能拿到尺寸 |
| Unity Entities〔源〕 | 不認得。執行期只載 build 時產生的 content archive 與 catalog（`RuntimeContentManager.cs:30-53`、`:412`） | `WeakObjectReference<T>`（`WeakObjectReference.cs:14`）：`LoadAsync`、`LoadingStatus`、`Release`（`:45-85`），引用計數 | `RuntimeContentSystem` 每幀處理排隊的命令（`RuntimeContentSystem.cs:95`）；狀態 `ObjectLoadingStatus`（`RuntimeContentManagerUtility.cs:15`） | **事先轉好的格式**；持久鍵＝GUID（`UntypedWeakReferenceId`），與檔名、格式無關 | Editor 匯入時（`TextureImporter`）〔知〕 | 發佈的內容一致、載入快；GUID 讓改名、換格式不影響參照；代價是一定要有 build pipeline |
| flecs〔源〕 | 沒有 asset 系統（`flecs.h` 搜尋 `asset` 0 件，正例 `ecs_` 1761 件） | — | — | — | — | 只做 ECS |
| Unreal〔知〕 | 不認得：`UFactory` 匯入、`.uasset` | `TSoftObjectPtr`、`FStreamableManager` | async loading thread，完成後回呼在 game thread | cook 後的 pak；持久鍵＝物件路徑 | 匯入時 | cook 讓平台格式事先備好 |
| Godot〔知〕 | `ResourceFormatLoader` 登記 | `Resource` 引用計數；`load_threaded_request` | 執行緒載入，主執行緒取結果 | 匯入後的 `.import`／`.ctex`；持久鍵＝`res://` 原始路徑（匯入不改參照）或 uid | 匯入時 | 使用者看到的還是原始檔的路徑 |
| O3DE〔知〕 | Asset Processor 依副檔名轉換 | `Asset<T>`＋`AssetId`（GUID） | 執行緒載、主執行緒派發 `OnAssetReady` | Asset Processor 的輸出；持久鍵＝GUID | Image Builder | 和 Unity 同一路 |
| master（GYO 舊） | 半認得：核心帶 catalog 解析 | `AssetCatalog` 邏輯 id；`AssetCachePolicy` 引用歸零後保留 N 幀（PvP 設 120，`PvpApplication.cpp:102-103`） | 主執行緒每幀輪詢，`maxLoadsPerFrame = 2` | 原始檔＋工具轉出的 clip；持久鍵＝catalog id | SDL3_image | — |

**共通**：
- 核心都不認得格式。
- 身分在請求時就給。
- 圖片都不用視窗層解碼。
- **持久鍵不隨「原始→轉檔」改變**：路徑不變（Bevy、Godot），或用 GUID／邏輯 id（Unity、O3DE、master）。

**分歧**：
- 執行期讀原始檔還是轉好的格式：使用者已定，起步照 Bevy。
- 持久鍵怎麼保持不變：第 12 節第 1 題。
- 完成的結果照什麼順序進 World：Bevy 照完成順序，舊 DI 2 照請求序號的連續前綴。見第 12 節第 4 題。

## 5. 設計（Contract）

### 5.1 位置、名字、依賴（卡片照 L17 判，照 N11～N17、O-6 與 `logging`／`world_logging` 的先例）

兩個單位：

| 單位 | 目錄 | target | 登記檔 | 依賴 | 內容 |
|---|---|---|---|---|---|
| asset | `engine/modules/asset` | `GYO::Asset` | library_only=yes、default=no | base；private io | `AssetPath`、loader 的介面與 `LoaderFailure`、整檔讀取（經 io，帶上限）、文字 loader |
| world_asset | `engine/modules/world_asset` | `GYO::WorldAsset` | library_only=no、default=yes | asset、base、kernel、orchestration；threads 看交換點 | `AssetRequests`、`Assets<T>`、`Handle<T>`、`AssetId<T>`、登記入口、套用 System、訊息、雜湊函式 |

為什麼這樣分：

- **為什麼拆兩個**：F4 的工具與擁有型別的 Module 只要 loader，不該為此連上 kernel、orchestration。這和 `logging`／`world_logging`、`time`／`world_time` 是同一個理由。
- **屬於哪一層**：都放 `engine/modules`。asset 依賴 io 和日後的解碼 Module，是會長大的 Module 層（`docs/architecture/README.md`〈Directory layers〉）；core 是「長期、少變」的層，不放這裡。
- **為什麼不是 Plugin**：兩個都是我們維護、介面是我們的。
- **`default=yes`**：照「Module 預設全帶、產品可以關」。Bevy 的 `DefaultPlugins` 含 `AssetPlugin`。渲染 World 不需要時，產品在那個 World 關掉。

命名：

- 請求狀態叫 `AssetRequests`，不照 Bevy 叫 `AssetServer`。「Server」在 GYO 已經指 PvP 的 Match 伺服器（`target/naming.zh-Hant.md` DI 4「名字是概念的 tag」）。
- 其餘照 Bevy：`Assets<T>`、`Handle<T>`、`AssetId<T>`、`AssetPath`、`AssetMessage<T>`。

### 5.2 請求、身分、Handle 的生命週期

**請求**：`AssetRequests::Load<T>(AssetPath, settings = {})` 在 System 裡（或在編排層給的準備函式裡）同步執行，當場回 `Handle<T>`。這一步會：
- 配 `AssetId<T>`（index＋generation）；
- 記一筆「排隊中」；
- 給一個請求序號。

**去重**：鍵＝（`AssetPath`、`T`、settings）。
- 同鍵已在、而且狀態是 `Queued`、`Loading` 或 `Loaded`：回同一個 id 的新 Handle，不再讀。
- 同鍵狀態是 `Failed`：**重新載入**，照 Bevy。這就是作者重試的方式；asset 自己不重試。
- **和 Bevy 不同**：Bevy 的鍵沒有 settings，是它的已知缺陷（issue 11111）。GYO 把 settings 放進鍵，所以每個 loader 的 `Settings` 要能比較相等、能雜湊（第 11 節的差異清單）。

**路徑**：`AssetPath`＝來源名＋來源內的相對路徑。來源是 io 的掛載點（`engine/io/fs/MountTable.hpp`、`Vfs.hpp`）。
- **它能不能當持久鍵**（進內容、進存檔、進判定資料）：第 12 節第 1 題。
- 建議 (i)：路徑是邏輯路徑，不隨「原始→轉檔」改變（第 5.10 節）。

**Handle**：
- 強 Handle 帶引用計數。最後一個強 Handle 消失後，那個 id 在下一次套用時移除，並發 `Removed`。
- 釋放只有一段，不做 Bevy 的 `Unused`→`Removed` 兩段；理由在第 5.14 節 A4。
- 不做 master 的 N 幀寬限：照第 1 節的讀檔流程，寬限沒有 Feature 需要（RC-AS7）。
- `AssetId<T>` 是弱的，不保活。

**取消**：最後一個 Handle 在 `Queued` 或 `Loading` 時消失，結果回來時丟掉，計入「沒人要了」（第 5.7 節的件數等式）。

**Handle 不參照 World**：Handle 共有 store 的核心（`AssetRequests` 持有的共用狀態）。

- store 被帶到新 World 時，Handle 照樣有效。
- store 被銷毀而 Handle 還在時，Handle **安全失效**：查詢回 `NotRequested`、取值回空。不會成為懸空參照，不靠作者。

**一個例外，寫明**：引用計數是 World 資料裡唯一跨 System 共用的可變狀態，以原子操作實作。它是「World 內不上鎖」（D6b、M11）的例外，只限計數本身。

### 5.3 `Assets<T>` 放在哪個 World；多個 World；替換 World

**每個 World 各自一份** `AssetRequests` 與它用到的 `Assets<T>`，放在請求它的那個 World（11-11 方向 2）。

- **渲染 World**：不另外請求 CPU 的 asset。它經 Extract 唯讀拿「哪些變了」，自己上傳 GPU（Bevy `RenderAssets`；K.3-6）。
- **兩個非渲染 World 要同一個內容**：各自請求、各自解碼。
  - 理由：沒有 Feature 也沒有量測要求共用（第 3.2 節）。
  - 重看的條件：RC-AS3。

**替換 World（O-8）時預設帶過去**（卡片照 L17 判；`engine-not-rigid` 預設方便、可關）：

- **帶什麼**：world_asset 在 `Build` 對 `AssetRequests` 與各 `Assets<T>` 標 `CarryOver`（O-10）。
  - **loader 登記表不帶**：它是另一個 Resource，`AssetLoaders`。替換會重跑 `Build`（O-2），由新的 `Build` 重建。
  - 要關掉時，產品在 World 描述裡給 `AssetOptions{carryOver = false}`，world_asset 用 `setup.Options<AssetOptions>()` 讀（`Setup.hpp:138`）。
- **為什麼有用**（F3）：照第 1 節的流程，舊 World 先驗證完；新 World 在**準備函式或 `beforeSwap`** 裡再請求。這時舊 World 還在（帶過去發生在準備之前，`Product.cpp:692-708`），舊的 Handle 讓內容活著，所以去重命中。
- **時間窗口，寫進契約**：
  - 舊 World 銷毀後，沒有被新 World 及時要回的，會在新 World 的第一次套用時移除。
  - 在第一次套用之後才請求的會重讀（第 10 節的案例）。
  - `DropOldFirst` 時舊的 Handle 先消失，所以全部重讀；這是作者選它的代價。
- **過渡狀態，寫明**：`BuildNewFirst` 期間，舊 World 的實體所持的 Handle 參照的 store 已經在新 World 裡。這是 Handle 共有 store 核心的結果，不是跨 World 的 asset 物件：沒有 System 能從舊 World 讀到新 World 的 `Assets<T>`。
- **`BuildNewFirst` 失敗時**，store 移回舊 World，帶著失敗的新 World 發的請求。這些請求的 Handle 已隨新 World 消失，下一次套用時照取消處理。
- **訊息不帶過去**：讀者游標屬於 World。新 World 的讀者要知道現有內容，就從 `Assets<T>` 對帳；收到沒看過 `Added` 的 id 的 `Removed`，不算錯。寫在新頁。

### 5.4 loader 的介面與登記

**介面**（在 `asset` 函式庫；名字實作時照 N15 定）：

- **一個 loader 宣告**：
  - 產出的型別 `T`；
  - 它認的格式標記：今天是副檔名，日後也可以是檔頭的魔數（5.10）；
  - `Settings`；
  - 預設的位元組上限；
  - 錯誤碼。
- **載入函式**：`Load(bytes, settings, LoadInfo) -> Result<T, LoaderFailure<Code>>`。
  - `LoadInfo` 只有路徑（給錯誤訊息用）與上限。
  - 沒有 World、沒有時鐘、沒有全域可變狀態。

**系譜**：master 的 `IAssetLoader::Load(ConstSpan<byte>, LoadContext)`，去掉 `AnyAsset` 的型別抹除。留下的理由是 F4 要直接呼叫。Bevy 的 `Reader`＋`LoadContext`＋async 不採，因為依賴載入沒有 Feature（RC-AS4）。

**整檔進記憶體**：解碼前整檔讀進記憶體，帶上限；不做串流的 loader。
- 理由：最大的內容（fbx 23.7 MB、BigMap region ≤ 16 MiB）在上限內。
- 需要串流時照 RC-AS1／AS4 重看。

**登記**：擁有型別的 Module 在自己的 `Build` 呼叫 world_asset 的入口，把 loader 登記進那個 World（11-11 方向 3）。

- **選 loader**：先看請求的 `T`；同一個 `T` 有多個 loader 時，再看格式標記。
- **同一個（`T`、格式標記）登記兩個**：啟動錯誤，在 `Finish` 回報，收進 `ProductStartupReport`（O-5）。
- **請求一個沒有 loader 的型別**：E2（`AssetLoadFailure{NoLoader}`）。型別的登記和 loader 的登記是同一個動作，所以沒有「型別登記了、loader 沒登記」的 E0 情況；路徑和格式是外部資料。

**上限**（AS8）：

- 每個 loader 給預設的位元組上限。
- **產品可以在登記時覆寫**，放寬或收緊都可以（`engine-not-rigid`）。
- 單一請求只能收緊。
- 超過上限就是 `TooLarge`（E2）。
- 理由：F8 的 region ≤ 16 MiB；master 的 `ReadAllBytes(…, max)` 也是這樣做。
- 文字 loader 的預設上限定 16 MiB（照 F8 的 region 上限；F1 最大的 json 是 1.45 MB）。

### 5.5 執行緒與交換點：給交換點宣告的需求

**本頁不設計交換點**，只列 asset 要它能做到的事。交換點的宣告可以用任何形狀滿足。

| # | 需求 | 為什麼（Feature） |
|---|---|---|
| X-1 | 一個 Module 能在 World 執行緒之外跑工作（讀檔＋解碼）；工作不碰 World（M11） | F1、F2：一張 1.3 MB 的 png 或 23.7 MB 的 fbx 解碼不能卡住 World 的幀 |
| X-2 | World 外的結果**只在交換點**進入 World；交換點不需要認得 asset 的型別或語意（RC-AS5、RC-O8） | M11；`Assets<T>` 的寫者只有 world_asset 的套用 System |
| X-3 | 請求從 World 出去：經交換點（出），或交換點宣告允許的別的通道。**延遲一幀可以接受**；同一個 store 的請求順序要保留 | 第 1 題之外，id 的配發不依賴這條；保留順序是讓讀檔的驗證可以重現 |
| X-4 | 替換 World 帶過去 store 之後，在飛的結果**不丟、不誤投**；store 被丟掉時，屬於它的結果算得到件數（見下面的做法） | 5.3、F3 |
| X-5 | 送出、收到、丟掉的件數對得上 | AGENTS §14 件數的突き合わせ；E3 |
| X-6 | **測試能指定哪幾筆結果在哪個交換點交回**（含「當場全部跑完」）；結果不依賴執行緒時序 | 等價測試（第 10 節）；只跑一次的 runner（O-3） |
| X-7 | 執行緒的數量與歸屬由交換點或 runner 定；asset 的工作是可以在任何執行緒跑的純函式（AS-3） | Bevy 用全域 `IoTaskPool`；GYO 不先定 |
| X-8 | 每次交換能交回的筆數或位元組可以有上限，剩下的留到下一次（AS8 的每幀套用上限要靠它，或由套用 System 自己分批） | F8 |

**X-4 的做法放在 asset 這一邊**，讓交換點什麼都不用知道：
- 每個結果帶著 store 的實例標記；
- 套用 System 只收自己 store 的結果，其餘丟掉；
- store 已銷毀時，結果由 world_asset 在 Product 層的計數記下（world_asset 在 `Configure` 建立）；
- 這個計數在下一次任何 World 的套用時，經 `DiagnosticsWriter<AssetDiscarded>` 記成 E3；
- Product 已結束的話，這個計數隨 Product 消失。

**套用的位置**（卡片照 L17 判）：預設掛 `Main` 的 `First`。
- 好處：同一幀之內，重跑的步與新的步看到同一份 `Assets<T>`。回滾的獨占 System 在 `PreUpdate`（第 4 批 5.2 節），會重跑的是 Client 的模擬 World；Match 不選回滾。
- Bevy 放在 `PreUpdate`，但 Bevy 沒有回滾的重跑。
- 生成世界期間怎麼套用，照第 5.13 節的需求。
- 產品關掉預設骨架時，自己把套用 System 掛到想要的位置（O-4）。

### 5.6 狀態與訊息

**狀態**：`NotRequested`、`Queued`、`Loading`、`Loaded`、`Failed`（帶 `AssetLoadFailure`）。用 `AssetRequests::State(id)` 查。

**「全部完成」**：`AssetRequests::Pending()` 回還沒結束的請求數；也可以傳一組 id 問「這些都結束了沒」。

**要不要等，由作者決定**（第 12 節第 3 題）。

**訊息**：

- 每個型別一種保留式 `Message`：`AssetMessage<T>{Added, Removed}`。給 render 的 Extract 與作者的 System 用。
- **不分型別的** `AssetLoadFailed{path, typeName, failure}`。給日誌與工具用，因為它們不認得每個 `T`（Bevy `UntypedAssetLoadFailedEvent`）。
- 用 `Message`、不用推送式 `Event`：讀者照自己的步調讀，同 Bevy 的 `AssetEvent`。
- **訊息可能被修剪**：事件游標的上限預設 1024，滿了丟最舊的。讀者不能只靠訊息追蹤現有內容，要能從 `Assets<T>` 對帳；寫在新頁。
- 一批之內的訊息順序**不保證**：第 12 節第 4 題建議照 Bevy。
- `Modified`、`LoadedWithDependencies` 不做（2.2）。

### 5.7 錯誤（照已合併的〈Error levels〉）

| 情況 | 級別 | 怎麼出現 |
|---|---|---|
| 檔案不在、讀不到、超過上限、沒有 loader、loader 解碼失敗 | **E2** | 那筆請求的狀態變 `Failed`，帶 `AssetLoadFailure{code, message, detail}`（`Engine::Base::Error<Code>` 的形狀，**沒有鏈**）。loader 自己的碼名與細節放進 `detail`（error-handling.md「at a module boundary the inner error's code name and detail go into detail」）；同時發 `AssetLoadFailed` |
| loader 丟例外（含 `std::bad_alloc`；`catch (...)`） | **E2**（`LoaderThrew`） | 同上。**註**：〈Error levels〉的 E2 是「外部資料或環境」。loader 丟例外可能是 loader 的程式錯，但工作在 World 外，例外不能沿著 World 的 Run 傳出去，也不能讓整個行程停下（E0 只用在呼叫端看得到的前提）。所以當成這次請求的失敗回報，`detail` 寫例外的 `what()` |
| 同一個（`T`、格式標記）登記兩個 loader；loader 的設定不合法 | 啟動錯誤 | `Finish` 回報，收進 `ProductStartupReport`（O-5） |
| 結果到了，但 store 已不在 | **E3** | `AssetDiscarded`（筆數、請求序號範圍），記法見 5.5 |
| 呼叫寫錯：在編排層給的時機之外、沒有 World 時呼叫 `Load` | **E0** | `GYO_ASSERT` |

**件數等式**（AS-8；第 10 節照它突き合わせ）：

> 送出 ＝ 套用 ＋ 失敗 ＋ 沒人要了（取消）＋ 丟棄（store 不在）

**asset 不寫日誌、不自己重試、不讓 World 中斷**（「Engine 的 Module 回傳錯誤，不自己寫日誌」）。

要不要因為載入失敗結束遊戲，由作者的 System 決定：它回 `Err`，就是作者把 E2 升成 E1。

### 5.8 決定性、內容身分（和第 4 批的權威、回滾對齊）

舊 DI 2、3 建立在「(c) 區＋Loading 的 Ingress」上（11-13 核准）。第 4 批拿掉區規則之後，重新推導如下。

1. **身分的決定性**：`AssetId` 在請求時同步配發。它只取決於**這個 store** 的請求序列（含之前放掉的 index 怎麼重用），和完成的時序無關（AS-5）。
   - 同一個 store 的同一串請求，得到同一串 id。
   - 新開局與讀檔進同一關（store 帶過來、有歷史），id 會不同，所以 id 不是持久鍵（第 3 點）。

2. **內容的決定性**：asset 核心不改位元組。`Assets<T>` 裡的值＝loader(位元組, settings)。
   - 不讀時鐘。
   - 全部結束之後，`Assets<T>` 的內容和完成的順序無關。
   - **哪一筆在哪一幀進來，依完成時序**：這是第 12 節第 4 題建議 (a) 的緩的面。
   - **loader 的輸出跨平台是否逐位元相同，是那個 loader 的事**（`determinism.md` DET-2：第三方預設在邊界外，要旗標受控＋跨平台 golden 才進邊界）。

3. **`AssetId`／`Handle` 不跨行程、不進存檔、不進權威的 digest**（AS-6）。
   - 理由：兩端的請求順序不保證相同，Client 還多載了呈現用的內容；store 帶過去之後，id 還依歷史而變。
   - 判定資料與存檔要參照內容時，用持久鍵（第 12 節第 1 題）。

4. **內容身分**（舊 DI 3：asset 從暫存的位元組算）：第 12 節第 2 題。
   - 建議 (b)：作者（或日後的網路 Module，join 時比）對已載入的值，用 Kernel 的逐型別雜湊客製點算；asset 另外**可選地**記每筆的來源位元組雜湊，**當成資訊，不當規則**。
   - **(a)／(b) 共同的代價**：對「已載入的值」比，等於把第三方解碼（例：ufbx）的跨平台逐位元相同納入比對。這正是 `determinism.md` §4.9、DET-2、F4 記的已知問題。(b) 的來源位元組雜湊讓作者可以避開它。

5. **回滾**：`Assets<T>` 不是判定資料，回滾 Module 不存它、不倒回它。
   - 一幀之內重跑的步看到同一份 `Assets<T>`（5.5 的套用位置）。
   - **但是**：第 100 個 tick 第一次跑時，某內容還沒載好、重跑時已經載好，依「載好沒」分支的判定 System 兩次結果就會不同。
   - 第 4 批修訂 6 第 21 節已經有現成的機制：**到達時刻要影響判定，就當輸入值，走 `rollback.Input<T>`**（例：「第 N 個 tick 起此地圖 region 可用」記成輸入的一欄，重跑時讀歷史）。
   - 不走它，就要靠作者讓判定用的內容在進入判定之前載完（第 12 節第 3 題）。

#### 5.8.1 asset 放進 World 的型別的雜湊函式（第 4 批 3.3「Engine 的 Module 放進 World 的型別都附雜湊函式」）

| 型別 | 雜湊什麼 | 理由 |
|---|---|---|
| `AssetRequests`、各 `Assets<T>`、`AssetLoaders` | **什麼都不寫** | 它們是載入的快取，可以照路徑重建；內容與筆數會隨完成的時序變，寫進去的話 `Summary` 就會依時序變。這和第 4 批的回滾歷史環「什麼都不寫」同一個理由 |
| `AssetMessage<T>`、`AssetLoadFailed` | **什麼都不寫** | 同上；何時發依完成時序 |
| `Handle<T>` | 持久鍵（第 12 節第 1 題的鍵）＋型別序號＋settings 的雜湊；**不雜湊 index** | Handle 經它共有的 store 核心拿得到鍵；這樣同一份存檔讀兩次，`Summary` 相同（第 4 批第 48 行），不受 store 歷史影響 |
| `AssetId<T>` | **不附函式** | 弱 id 拿不到鍵。作者把它放進元件、又呼叫 `Summary`，就照第 4 批 3.3 的 (d) 回錯誤，這是有意的：提示作者改存 Handle 或持久鍵 |
| `AssetPath` | 來源名＋相對路徑 | 接 Kernel 的同一個客製點（一個概念一個實作） |

F9（2D 單機）：不呼叫 `Summary` 就不碰這些。呼叫時，上表讓帶 world_asset 的 World 不報錯。

### 5.9 讀檔與存檔版本 Module

**讀檔**（讀檔宣告修訂 11，核准版）＝替換 World，在新 World 裡放回。asset 的部分照第 1 節的流程：

1. **先驗證**：作者在現在的 World 請求存檔裡參照的內容，等到全部結束。有 `Failed` 就不發替換，回 `Result` 給玩家看（O-8「作者在發出 `ReplaceRequest` 之前驗證」，明列素材）。
2. **再替換**：新 World 在準備函式或 `beforeSwap` 再請求，去重命中（5.3）。
3. **存檔裡的參照用持久鍵**，不用 `AssetId`（AS-6）。

**存檔不是 asset**：存檔的格式是作者的（`-save-unaware-means-format-only`）。讀存檔檔案用 io，或作者自己的方式。

**存檔版本 Module**（`v8-arch-gyo2-save-version-module`）：比對存檔的版本標記，和 asset 的內容**無關**。
- 內容改版造成存檔參照的內容消失時，由第 1 步的驗證發現（E2）。
- asset 不提供「內容版本」。事先轉檔的資料契約回來時，Version 屬那份資料契約（AGENTS §8）。

### 5.10 接得上「工具事先轉檔、執行期讀引擎格式」（AGENTS §8）

使用者決定 3：起步讀原始檔，介面要接得上。本批保證三件，每件都有測試（第 10 節）。

1. **選 loader 的鍵是（`T`、格式標記）**，不是「原始格式」。工具產出的引擎格式＝另一個登記給同一個 `T` 的 loader。請求的 API、Handle、`Assets<T>` 都不變。
2. **來源是 io 的掛載點**。工具輸出的資料夾＝另一個來源，asset 不知道它是誰產生的（Editor、Script、手做都一樣）。
3. **loader 是純函式**，在不認得 ECS 的 `asset` 函式庫上就能呼叫。工具要驗證自己的輸出，就呼叫同一個 loader（`engine-not-tool-duties`）。

**持久鍵是否也不變**（Bevy、Godot：轉檔後路徑不變；Unity、O3DE：GUID）：第 12 節第 1 題。
- 選 (i)：轉檔的輸出用**同一個來源名、同一個相對路徑**掛在原始檔之上，格式標記改用檔頭的魔數。`AssetPath` 不變，內容與存檔不受影響。
- 選 (ii)：持久鍵是工具與遊戲共用的邏輯 id。

**不在本批**：轉檔工具、引擎格式、旁邊的 meta 檔、用來源雜湊判斷要不要重轉（Bevy `processor/`）。這些是工具回來時那份資料契約的內容，含 Version 與 Validation Rule（AGENTS §8 MUST）。

### 5.11 第三方：stb_image（使用者決定 1；實作時機見第 12 節第 5 題）

| 項目 | 內容 |
|---|---|
| 組件 | `stb_image.h`（nothings/stb，單一標頭）。實作時釘 commit，不釘 tag（同 yyjson） |
| 授權 | 雙授權，使用者擇一：MIT 或 Public Domain（Unlicense），寫在檔尾。下載時以檔案原文確認。打包時附授權告示（批次表「之後的小事」的第三方告示一條） |
| 放哪裡 | `third_party/stb_image/CMakeLists.txt`（FetchContent，同 `third_party/yyjson`），產生 `gyo_stb_image`：只含一個 `.c`，在裡面定義 `STB_IMAGE_IMPLEMENTATION` |
| 只出現在哪裡 | `engine/modules/image/src/adapters/stb_image/`（第 5 題 (B)、(C)）；選 (A) 時是 render Module 的 `src/adapters/stb_image/`。`gyo_stb_image` 只被那一個 target **私有** link；標頭不出現在任何 `include/` 下 |
| 編譯設定 | `STBI_ONLY_PNG`（master 的 5 張全是 png；jpg 等有 Feature 時再開）；`STBI_NO_STDIO`（讀檔歸 asset）；`STBI_NO_HDR`、`STBI_NO_LINEAR`（避開 `pow`）；`STBI_NO_FAILURE_STRINGS`；`STBI_MAX_DIMENSIONS` 設上限。adapter **不用** `stbi_failure_reason`、`stbi_set_flip_vertically_on_load`（全域或 thread-local 狀態），維持 AS-3 |
| 建置旗標 | 照 `determinism.md` DET-2：第三方預設在決定性邊界外。本頁**不宣稱**它在邊界內；圖片是呈現用，沒有 Feature 需要跨平台逐位元。image 批的 golden 是**回歸測試**，不是邊界的證明 |
| 為什麼不走 SDL | Match、測試、工具載入不該需要 SDL（F2、F4）；一個外部組件只有一個 adapter（AGENTS §6） |
| 已知的風險 | stb 的 README 說明 stb_image 不以惡意輸入為設計目標，歷史上有多次針對畸形檔的缺陷回報〔知〕。今天的內容都是自己的，所以接受。**不可信來源的圖成為 Feature 時換 adapter**（wuffs、libspng；RC-IM1），只改 `src/adapters/stb_image/` 一處 |
| 下載 | 要使用者同意，在 image 那批（第 13 節） |

### 5.12 對 io 的要求（交給 threads、io 重審那批）

- 從 World 以外的執行緒讀整個檔，帶位元組上限（今天是 `engine/io/helpers/ReadAllBytes`）。**`IFileSystem`／`Vfs` 在多執行緒同時讀時的契約要確認。**
- 掛載點能用名字找到（`AssetPath` 的來源名）。
- 第 12 節第 1 題選 (i) 的話，同一個來源名要能疊兩個掛載點：轉檔的輸出在原始檔之上，找得到就用上面那個。
- 檔案監看（`IFileWatcher`）留給熱重載，本批不用。

### 5.13 給生成世界（重做）那一批的需求

舊設計靠「Loading 的 Ingress stage」，把載入的結果在進入遊戲之前收進來。新設計拿掉了它，生成世界那批還沒有宣告。asset 要它做到下面四件：

| # | 需求 | 為什麼 |
|---|---|---|
| G-1 | 生成世界分批跑時，每一批之間有交換點，World 外的結果能進來 | 第 1 節的流程：作者等「全部完成」才離開；結果進不來就永遠等不到 |
| G-2 | world_asset 的套用 System 在生成世界期間會跑。由誰掛、掛在哪，由那批的宣告定（例：生成世界的每一批前面固定有一個「套用外部結果」的位置） | 同上；AS-4 只說套用由 world_asset 的 System 做，不釘死在 `Main` |
| G-3 | 結束條件由作者的 System 給，可以是「`Pending()==0`」，也可以是作者自己的條件 | `-build-world-phase-is-mechanism` |
| G-4 | `beforeSwap` 是同步連跑的（`Product.cpp:740-749`），期間沒有交換點。這不是要改它，是寫明：`beforeSwap` 裡發出的請求要麼命中去重，要麼在替換之後才完成 | 5.3 的時間窗口；第 1 節的讀檔流程 |

### 5.14 藍圖的資產不變量 A1～A6（X17「各能力設計時再判」）

| # | 藍圖的內容 | 本頁 | 理由 |
|---|---|---|---|
| A1 | 每型別一個 store＋每型別一份事件，core 只持 TypeId→fn 表 | **留** | `Assets<T>`、`AssetMessage<T>`、登記表 |
| A2 | id 只在行程內有效，path（或 Uuid）才是契約 | **留**（AS-6）；契約的鍵是哪一個，見第 12 節第 1 題 | — |
| A3 | 非同步完成序列化到單一套用點（E10i 照帶） | **留**（AS-4）；「序列化」照第 12 節第 4 題的選項 | — |
| A4 | 釋放兩段式（`Unused`→`Removed`） | **改成一段** | 兩段的用處是讓讀者在移除前看到「沒人用了」、可以再拿回來（Bevy）。本頁的讀檔流程靠「舊 World 還在時再請求」拿回（5.3），沒有 Feature 需要兩段 → RC-AS7 |
| A5 | 依賴圖由 core 擁有、loader 只宣告 | **延後** | 沒有 Feature（3.2）→ RC-AS4 |
| A6 | 格式解析器與執行期解讀分層 | **留**，形狀是 asset（library）與 world_asset 的拆法、loader 歸擁有型別的 Module | — |
| （:66）| catalog 邏輯 id、StrictJson、Result、ready 前全載「比 Bevy 嚴」；loader 副檔名衝突應為錯誤 | catalog：owner 未定（第 12 節第 1 題）；StrictJson 歸 JSON Module；Result：留（5.7）；ready 前全載：第 12 節第 3 題；副檔名衝突：**留**，是啟動錯誤（5.4） | — |

## 6. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature 需要**：F1～F5、F8、F9（第 3.2 節），加上使用者決定 1～3。
2. **現在的問題**：
   - `gyo2` 沒有 asset。
   - 舊設計（target `assets` DI 2、3）建立在已拿掉或正在拿掉的 (c) 區、Loading 的 Ingress stage 與 Host 上。
   - master 的 `AssetManager` 是總管物件，在主執行緒輪詢解碼，貼圖綁 SDL3_image。
3. **變化的邊界**：
   - 新 Subsystem：`engine/modules/asset`、`engine/modules/world_asset`。
   - 第 12 節第 5 題選 (B) 時，另有 `engine/modules/image` 與 `third_party/stb_image`。
   - target 文件：舊 `target/assets.zh-Hant.md` 的 DI 1～3 與 RC，由新頁取代。
4. **影響**：
   - 編排層：登記兩個單位；不改它的程式（靠 O-10、O-5、`Options`）。
   - 交換點、生成世界：第 5.5、5.13 節的需求。
   - 第 4 批：`Summary` 的雜湊（5.8.1）。
   - 日誌與 `world_logging`：`AssetLoadFailed`、`AssetDiscarded`。
   - 之後的 render、text、ui、audio、model Module：它們的 loader 登記到 world_asset。
   - 產品與工具：載入改經 world_asset，或直接呼叫 loader。
   - **失去 master 的 catalog 的工具**（audio_editor、ui_editor、model_viewer）：catalog 的 owner 未定（第 15 節第 5 點）。
5. **Dependency Direction**：
   - asset → base（private io）。
   - world_asset → asset、base、kernel、orchestration（日後可能加 threads）。
   - 日後的 image → asset，登記時再加 world_asset。
   - 全部往下，沒有反向。core 不依賴它們；編排層不認得它們（RC-O4、RC-O8）。
6. **Ownership**：
   - 內容的請求、身分、生命週期 → world_asset。
   - loader 介面與整檔讀取 → asset。
   - 格式 → 擁有型別的 Module；第三方解碼 → 該 Module 的 adapter。
   - **內容身分的比對、「載完才開始」 → 作者**（第 12 節第 2、3 題）。
   - GPU 資源 → render。
   - catalog → 未定。
7. **為什麼不能更小**：
   - 沒有 asset，每個 Module 與產品就各自讀檔，各自管生命週期與錯誤。
   - 拿 master 的 `AssetManager` 改，是在為舊形狀找位置，11-11 已經不採。
   - 只做一個單位（修訂 0）的話，F4 的工具要連上 kernel 與 orchestration。

## 7. Design Invariants（新頁 `target/v2/asset.zh-Hant.md`，見出し `Design Invariants`）

**舊 → 新的系譜**（「緩」＝舊的保證、新的不保證；「緊」＝反過來）：

| 舊（11-13 核准） | 新 | 緩了什麼 | 緊了什麼、其他 |
|---|---|---|---|
| DI 1 沒有總管物件，三類 Resource 各在它們的 World | **AS-1** | 無 | GPU 資源標明歸 render；寫明替換期間的過渡狀態 |
| DI 2 載入是外部服務；經 Loading 的 Ingress 進 (c) 暫存，由 (c) System 依請求序號的**連續前綴**套用；「套用順序與批次切分都與完成時序無關」 | **AS-4**、**AS-5** | 第 4 題選 (a) 時：**批次切分、跨批的套用順序、訊息順序改依完成時序**；只保留「id 在請求時給」「全部結束後內容與完成順序無關」。守衛「打亂完成順序，`Assets<T>` 與 Handle 序列相同」只對最終狀態成立 | 「IO 不碰 World、經交換點、單一套用點」保留（A3、E10i）；不再要 (c) 區與 Ingress stage |
| DI 3 內容身分是 (c)，由 (c) System 從暫存的**位元組**算；世界內容在 Loading 宣告 ready 前載完；守衛：「資產未就緒即 ready」是 Loading 的 System 以 Err 回傳的 Programmer Error | **AS-6**、**AS-7** | 第 2、3 題照建議時：(1) 內容身分不再由 asset 算；改對已載入的值算的話，第三方解碼的跨平台差異會進入比對（(b) 的來源位元組雜湊可以避開）；(2) 「ready 前載完」不再是 Engine 的規則；(3) **舊守衛消失**：沒有任何機制擋「未載完就進遊戲」 | 寫明 `AssetId` 不進 digest、不進存檔（舊的沒有寫） |
| 舊 RC：Running 期間串流進 (c) → 重審 | **RC-AS1** | — | 改寫成不用區的措辭；指向 `rollback.Input<T>` |
| — | **AS-2**、**AS-3**、**AS-8**、**AS-9**（新） | — | 使用者決定 1、3；11-11 方向 3；異常分級；AGENTS §8 |

**新的 DI**（守衛欄的「延後」照 `v8-arch-gyo2-guards-after-mechanism-stable`：綁實作方式的掃描守衛，等 scenario test 通過再立，這段期間靠本頁與審查）：

- **AS-1 沒有總管物件。**
  - 請求狀態 `AssetRequests` 與每個型別一份 `Assets<T>` 是 Resource，放在請求它的 World。
  - GPU 資源是 render 的 Resource。
  - System 以 Handle 或 id 存取，不持指標；沒有 System 能讀到別的 World 的 `Assets<T>`。
  - 替換期間，舊 World 的 Handle 共有已帶到新 World 的 store 核心（5.3），這是唯一的過渡狀態。
  - 守衛：行為測試（兩個 World 各自請求同一路徑，各有自己的 id 與值）。掃描延後。
- **AS-2 核心不認得格式。**
  - asset 與 world_asset 不解析任何檔案格式。文字 loader 例外：它只驗證 UTF-8。
  - 格式由擁有型別的 Module 登記 loader。
  - 第三方解碼器只在該 Module 的 `src/adapters/<組件>/`，私有 link。
  - 守衛：行為測試（請求一個沒有 loader 的型別，回 `NoLoader`；同一個（`T`、格式標記）登記兩次是啟動錯誤）。掃描延後（asset 的 `src/` 不含入第三方）。
- **AS-3 loader 是不依賴 ECS、可直接呼叫的純函式。**
  - 輸入：位元組、settings、路徑與上限。輸出：值或失敗。
  - 不碰 World，不讀時鐘，不持全域可變狀態。
  - 只連 `GYO::Asset` 就能呼叫。
  - 守衛：
    - 行為測試：只連 `GYO::Asset` 的測試 target；同輸入呼叫兩次結果相等；兩條執行緒各呼叫 1,000 次，結果都等於單執行緒的結果。
    - CMake 掃描延後：`GYO::Asset` 不 link kernel。
- **AS-4 IO 不碰 World；單一套用點。**
  - 讀檔與解碼在 World 執行緒之外。
  - 結果只在交換點進入 World，由 world_asset 的套用 System 放進 `Assets<T>`。它是唯一的寫者。
  - 預設掛 `Main`/`First`；生成世界期間照 5.13。
  - 守衛：行為測試（X-6 的控制模式＋真執行緒模式）。
- **AS-5 身分在請求時給。**
  - `AssetId` 在請求時同步配發，只依這個 store 的請求序列。
  - 全部結束之後，任何完成順序、任何批次切分都得到相同的 `Assets<T>` 內容。
  - 第 4 題選 (b) 時加一句：「套用照請求序號的連續前綴，批次切分與套用順序都和完成時序無關」。
  - 守衛：等價測試（第 10 節）。
- **AS-6 `AssetId`／`Handle` 的 index 只在一個 store 裡有效。**
  - 不跨行程，不進權威的 digest，不進存檔。
  - 跨行程、跨版本的參照用持久鍵（第 12 節第 1 題）。
  - `Handle` 的雜湊用持久鍵、不用 index（5.8.1）。
  - 守衛：行為測試（帶 world_asset 的 World 讀同一份存檔兩次，`Summary` 相同）；其餘靠本頁與審查。
- **AS-7 「何時載完」與內容身分不是 asset 的規則。**
  - asset 給狀態、訊息、「全部完成」的查詢，保證同位元組同值（AS-3）；第 2 題選 (b) 時，另給可選的來源位元組雜湊。
  - 要不要等、內容身分怎麼算怎麼比，屬作者或網路 Module。
  - 到達時刻要影響判定，走 `rollback.Input<T>`。
  - 守衛：本頁與審查；範例測試（作者對 `Assets<T>` 的值用 Kernel 的客製點算雜湊）。
- **AS-8 錯誤只回報。**
  - 每筆請求的失敗是 E2 `AssetLoadFailure`，在狀態、`AssetLoadFailed` 訊息裡；登記錯誤是啟動錯誤；丟棄是 E3。
  - asset 不寫日誌，不自己重試，不讓 World 中斷。
  - 件數等式成立：送出＝套用＋失敗＋取消＋丟棄。
  - 守衛：行為測試（各碼、各項）。
- **AS-9 介面與格式、來源分開。**
  - 選 loader 用（`T`、格式標記）；來源是 io 的掛載點。
  - 加一種格式（含日後的引擎格式）＝登記一個 loader，不改請求的 API 與核心。
  - 第 1 題選 (i) 時加一句：「同一個 `AssetPath` 在原始來源與轉檔來源下都解得到」。
  - 守衛：行為測試（第 10 節）。

### Replacement Criteria（寫在積上去之前）

符合任一條時，停下來交使用者決定「修還是重建」：

- **RC-AS1**（舊 RC 改寫）：出現必須在遊戲進行中串流載入、**而且結果影響判定**的內容（例：BigMap 的 region 在對局中進碰撞），而 `rollback.Input<T>` 的做法表達不了。這時「進判定前載完」要重審，參考 lightyear、Bevy 的串流模型；不在 `Assets<T>` 上加特例。
- **RC-AS2**：每加一種格式都要改 asset 核心，或某個 loader 必須碰 World 才寫得出來。
- **RC-AS3**：同一內容在兩個以上 World 重複解碼，量測超出預算；**預算定下之前的暫時條件**：一個點名的 Feature 讓同一個 ≥ 1 MiB 的內容在同一 Product 的兩個 World 解碼。或者，同一個 Product 裡出現第二份跨 World 的 asset 狀態。
- **RC-AS4**：兩個以上的 loader 需要在解碼中途再要別的 asset，而產品的 System 依序請求表達不了；或熱重載、子 asset 成為點名的 Feature。這時才做依賴、`Modified`、`#label`。
- **RC-AS5**：交換點交回結果時，交換點或編排層需要知道 asset 的型別或語意（和 RC-O8 同類）。
- **RC-AS6**：事先轉檔的路回來時，請求的 API、Handle，**或內容與存檔裡的持久鍵**必須改（違反 AS-9 與第 1 題的承諾）。
- **RC-AS7**：讀檔或其他 Feature 需要在舊 World 銷毀之後才再請求、又不能重讀（要寬限或兩段釋放）。
- **RC-IM1**（image Module 的 DI 節）：不可信來源的圖成為 Feature，或 stb_image 的缺陷影響我們的內容，就換 adapter。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

每一列都先問：「這是不是在為舊形狀找位置？」（`target/README.md`〈每批事前宣告的檢查項〉）

| 選擇 | 採用 | 系譜 | 退掉的 | 決勝理由 | 需要它的 Feature |
|---|---|---|---|---|---|
| 形狀 | 請求狀態＋每型別 `Assets<T>`＋Handle | Bevy（11-11 方向） | master `AssetManager` 總管物件；「`AssetManager` 放哪個 World」 | 後者是在為舊形狀找位置，11-11 不採；拆開後每個 World 只持自己的 | F1、F5 |
| 單位的拆法 | `asset`（library）＋`world_asset`（Module） | `logging`／`world_logging`、`time`／`world_time` | 一個 Module（修訂 0）；loader 介面放 core | 一個 Module 讓 F4 的工具與解碼 Module 連上 ECS；core 是長期少變的層，loader 介面會隨引擎格式長大 | F4、F6 |
| 持久鍵 | **第 12 節第 1 題** | Bevy／Godot（路徑不變）、Unity／O3DE／master（GUID／邏輯 id） | — | 見第 12 節 | F3、F5、F6 |
| 結果怎麼進 World | 經交換點；asset 帶 store 標記，自己篩 | M11、D6b、舊 DI 2 的「外部服務」 | Bevy：Module 自己的 channel，在自己的 System 裡排空；舊 DI 2 的 `AssetInbox`＋Ingress 暫存 | Bevy 的做法讓每個 Module 在自己的 System 裡各開一個 IO 邊界，交換點無法統一計數與在測試裡控制（X-5、X-6）。`AssetInbox`＋Ingress 是舊形狀，(c) 區已拿掉；本頁只寫需求，槽的形狀交給交換點 | F1、F2 |
| 結果的順序 | **第 12 節第 4 題** | Bevy（請求時配 id、照完成順序） | 舊 DI 2 的連續前綴 | 見第 12 節 | F1、F8 |
| 內容身分 | **第 12 節第 2 題** | 新推導；第 4 批 3.2、4.4 | 舊 DI 3 | 見第 12 節 | F2、F9 |
| 「載完才開始」 | **第 12 節第 3 題** | `-build-world-phase-is-mechanism` | 舊 DI 3、runtime-composition DI 3 | 見第 12 節 | F1、F8、F9 |
| 套用的位置 | 預設 `Main`/`First`；生成世界期間照 5.13 | Bevy 的 `PreUpdate`，往前移一格 | `PreUpdate`（Bevy 原樣） | 同一幀的重跑步與新步看到同一份（Client 的模擬 World 在 `PreUpdate` 重跑） | F1（Client） |
| 多 World 共用 | 不共用，各自請求 | 新推導 | Product 層共用快取 | 沒有 Feature，也沒有量測 → RC-AS3 | — |
| 替換 World | 預設帶過去（loader 表不帶），產品可以關；先驗證再替換 | O-8、O-10 | 不帶（每次讀檔重讀）；master 的 N 幀寬限 | F3 不重讀同一關；寬限在「舊 World 還在時再請求」的流程下沒有用處 → RC-AS7 | F3 |
| 釋放 | 一段 | master／Bevy 的引用計數 | 兩段（藍圖 A4、Bevy `Unused`） | 同上 | F3 |
| 去重的鍵 | （路徑、`T`、settings） | Bevy 的（路徑、型別）＋修它的已知缺陷 | Bevy 原樣（settings 被忽略） | 同路徑不同 settings 是兩個內容（例：日後的 sRGB／linear）；Bevy 自己標成缺陷 | F1 |
| 錯誤 | 狀態裡的 E2＋不分型別的訊息；不記日誌 | 〈Error levels〉；Bevy `UntypedAssetLoadFailedEvent` | asset 自己寫 `Warning`（master） | 「Engine 的 Module 回傳錯誤，不自己寫日誌」；日誌不認得每個 `T` | F1、F5 |
| loader | 純函式、整檔、直接可呼叫 | master `IAssetLoader` 去掉型別抹除 | Bevy 的 `Reader`＋`LoadContext`＋async；master 的 `AnyAsset` | F4 要直接呼叫；依賴載入與串流沒有 Feature（RC-AS1、AS4）；型別化的 `T` 讓 `Assets<T>` 不需要抹除 | F4、F6 |
| 上限 | 每幀套用、在飛：World 選項；loader 上限：產品可覆寫 | AGENTS §2（予見的要求寫成上限與擴張點）；master 的 `maxLoadsPerFrame` | 沒有上限（修訂 0）；loader 上限只能收緊 | F8 的 242 MB；`engine-not-rigid` | F8 |
| 圖片解碼 | stb_image，在一個 adapter | 使用者決定 1 | SDL3_image；libspng、wuffs | 使用者已定 | F1、F4、F5 |
| 圖片放哪裡、何時做 | **第 12 節第 5 題** | Bevy 的 `bevy_image` | — | 見第 12 節 | F1、F5 |

**沒有用來決勝的**：既有的 Test、守衛、golden 不必改。`gyo2` 上沒有 asset 的測試。

## 9. thin start 與實作的步驟（動工前核准、第 5 批合併、使用者說開始之後）

**實作**：AS1～AS9，也就是名指的 Feature 直近要用的部分。

**只定形狀、不實作的**：只有引擎格式的 loader 一項。根據是 F6、F7 與使用者決定 3：留在 AS-9 的（`T`、格式標記）與掛載點裡，只用測試用的 loader 驗證。

**沒有根據的不留形狀**（敵對式檢查 S12）：熱重載、依賴、子 asset。

| 步 | 內容 | 模型／檔位 | 上段或下段 |
|---|---|---|---|
| 第 1 步 | `asset` 函式庫：`AssetPath`、loader 介面、整檔讀取、文字 loader；只連它的測試 | Opus／high | 上段（公開介面） |
| 第 2 步 | `world_asset` 骨架、登記列、`AssetId`／`Handle` 的配發、計數、安全失效 | Opus／high | 上段 |
| 第 3 步 | `AssetRequests`、`Assets<T>`、去重、`Failed` 再請求、取消、狀態與訊息、5.8.1 的雜湊函式 | Opus／high | 上段 |
| 第 4 步 | 接交換點與生成世界（照它們的宣告）、套用 System、真執行緒模式、上限；等價測試 | Opus／high（時序的部分若檢查認為要，先問再升 xhigh） | 上段（執行緒、決定性） |
| 第 5 步 | 帶過去、讀檔案例、丟棄記錄、錯誤各碼 | Opus／high | 上段 |
| 第 6 步 | scenario test、突變、新頁、dev_log、審查 | 實作 Opus／high；審查 Fable／high（和作者不同的模型） | 上段 |

第 12 節第 5 題選 (B) 時加第 7 步：image Module＋stb adapter＋第 13 節的下載，Opus／high（新第三方與 Build Graph 的變更，不是 low 的定型作業）。

## 10. 測試、判定（先宣告；結果出來之後不放寬）

測試用的型別：
- 文字（真的 loader）；
- 測試目錄裡的兩個測試 loader：一個慢、可以指定失敗；一個用「`.gyotxt`＋魔數」模擬引擎格式。

測試圖：選 (B) 時用程式產生的 PNG，沒有授權問題。

**scenario test（第 6 步，沿遊戲流程）**：

1. 生成世界分兩批：第一批請求 5 筆，其中一筆檔案不在、一筆超過上限。
2. 跑到「全部完成」：3 筆 `Loaded`，2 筆 `Failed`，碼是 `NotFound`、`TooLarge`；`AssetLoadFailed` 2 則。
3. **讀檔，先驗證**：在現在的 World 請求存檔要的 2 筆（已載入），等到結束，發 `ReplaceRequest`（`BuildNewFirst`）。新 World 在 `beforeSwap` 再請求這 2 筆：命中，讀檔計數不增加。沒再要的 1 筆，在新 World 的第一次套用時移除，發 `Removed`。
4. **時間窗口**：新 World 在第一次套用之後才請求那 1 筆，結果是重讀，讀檔計數加 1。
5. **先驗證失敗**：存檔參照一個不在的檔。驗證得到 `Failed`，不發替換，舊 World 照跑。
6. 件數的突き合わせ：送出＝套用＋失敗＋取消＋丟棄，各項的數字在測試裡寫明。

**等價測試（AS-5）**：同一組 20 筆請求，用 X-6 的控制模式比較：

| 組 | 內容 |
|---|---|
| (1) | 一次全部交回 |
| (2) | 反序分 4 次交回 |
| (3) | 交錯分 7 次交回 |
| (4) | 載入中途丟掉 3 個 Handle，同時再請求 3 筆新的；index 重用不得依完成時序 |

判定：
- 全部結束後，四組的 `Assets<T>` 內容相同；
- id 序列相同：(4) 和「同樣的請求與放掉、一次交回」相比；
- 件數等式成立。

**取消與丟棄**：
- 載入中丟掉最後的 Handle：計入取消。
- `DropOldFirst` 時有 2 筆在飛：計入丟棄，`AssetDiscarded` 1 則、筆數 2。
- 關掉帶過去時有 1 筆在飛：計入丟棄。

**`Summary`（AS-6、5.8.1）**：帶 world_asset 的 World，同一份存檔讀兩次，`Summary` 相同；元件裡放 `AssetId<T>` 時，`Summary` 回缺函式的錯誤。

**直接呼叫（AS-3、F4）**：
- 只連 `GYO::Asset` 的測試 target 呼叫文字 loader；
- 同輸入兩次結果相等；
- 兩條執行緒各呼叫 1,000 次，結果全部等於單執行緒的結果（判定：0 件不一致）。

**真執行緒模式（AS-4）**：同一組 20 筆請求跑 50 次。判定：每次的最終 `Assets<T>` 都等於控制模式；件數等式 50/50 成立。

**接得上事先轉檔（AS-9）**：
- 同一個 `T` 登記兩個 loader（`.txt` 與 `.gyotxt`），請求的 API 不變；
- 兩個掛載點各一個來源；
- 同一個（`T`、格式標記）登記兩次 → 啟動錯誤；
- 第 1 題選 (i) 時加：同一個 `AssetPath`，在只有原始來源、和疊上轉檔來源兩種情況下都解得到，值相同。

**突變**：每條 DI 的守衛各至少一個專屬的預期失敗（AGENTS §14.2、§14.5；`tests/common/mutation/README.md`）。清單在實作時列，受別的模型做敵對式檢查。

**判定**：上面全部通過；L1 三平台（linux-x64、windows-x64、macos-arm64）通過；突變全部 killed。

**量測**：載入時間、記憶體只做探索量測，標「不是判定」，等第 12 節第 6 題。

## 11. 卡片照 L17 判的（不是使用者的話）

**照命名規範、別家原始碼、已定原則判的**：
- 兩個單位的拆法與名字，`AssetRequests` 不叫 `AssetServer`（5.1）；
- `default=yes`（5.1）；
- `Failed` 之後可以再請求，取消算「沒人要了」（5.2）；
- 每個 World 各自，不共用（5.3）；
- 替換 World 預設帶過去，loader 表不帶，訊息不帶（5.3）；
- 釋放一段，不做寬限（5.2、5.14 A4）；
- 套用在 `First`（5.5）；
- X-4 由 asset 自己篩（5.5）；
- 用 `Message`，加不分型別的失敗訊息（5.6）；
- 錯誤碼與級別的對應，`LoaderThrew` 歸 E2（5.7）；
- 5.8.1 的雜湊函式；
- 文字 loader 的預設上限 16 MiB（5.4）；
- stb_image 的編譯設定（5.11）；
- 不做二進位 loader（2.2）。

**和別家不同的地方**（照 `-confirm-only-differences` 列出，不是問題）：
- 去重的鍵含 settings（Bevy 不含）；
- 結果經交換點，不經 Module 自己的 channel（Bevy 用 channel）；
- 套用在 `First`（Bevy 在 `PreUpdate`）；
- 釋放一段（Bevy 兩段）。

## 12. 交使用者的

只列三類：會推翻使用者之前的決定的、別家分歧且真有取捨的、職責清單要多一項的。

建議的回法：「1～6 照建議」，或逐題回。

### 第 1 題：內容與存檔裡用什麼鍵參照資產（持久鍵）；catalog 歸誰

**為什麼問**：使用者決定 3 要「接得上事先轉檔」。修訂 0 讓 `AssetPath`（來源名＋相對路徑，含副檔名）當持久鍵，但轉檔後路徑的兩半都會變，所有內容檔與玩家的舊存檔裡的參照會一起失效（敵對式檢查 B3）。別家在這裡分兩路。

| 選項 | 內容 | 別家 | 代價 |
|---|---|---|---|
| **(i) 邏輯路徑不變（建議）** | `AssetPath` 是邏輯路徑。轉檔的輸出用同一個來源名、同一個相對路徑，疊在原始檔之上；loader 由格式標記（引擎格式用檔頭魔數）選 | Bevy Processed（輸出路徑不變，由 `.meta` 決定怎麼載）、Godot（`res://` 原始路徑） | io 要支援疊掛載點（5.12）；改檔名就會斷參照（Bevy 也一樣） |
| (ii) 持久鍵是邏輯 id（catalog 或 GUID） | 內容與存檔記 id，執行期經對照表換成路徑；asset 核心照舊只認路徑，對照表由 catalog 的 owner 提供 | Unity、O3DE（GUID）、master（catalog id） | 要先定 catalog 的 owner 與格式（今天未定）；多一份要維護的對照表 |
| (iii) 照修訂 0 | 路徑含副檔名；轉檔時改所有鍵 | — | 舊存檔失效；違反 RC-AS6 |

**catalog**：不論選哪個，master 的 `AssetCatalog` 都不進 asset 核心。

- 選 (i)：catalog 只剩「UI 文件用短 id」的用途，由工具搬回時決定要不要留。
- 選 (ii)：catalog 就是持久鍵的對照表，**它的 owner 要先定**（AGENTS §1：owner 不明，不在 Feature 的作業裡定；建議交主會話，在 ui_editor 或 audio_editor 搬回時定）。

### 第 2 題：內容身分（改舊 DI 3 的前半，11-13 核准過）

| 選項 | 內容 | 代價 |
|---|---|---|
| (a) 只由作者對已載入的值算（Kernel 的客製點） | asset 什麼都不算 | 第三方解碼（ufbx）的跨平台差異進入比對（`determinism.md` §4.9、DET-2 的已知問題） |
| **(b) (a)＋asset 可選地記每筆的來源位元組雜湊，當資訊（建議）** | 預設關；開了之後 `AssetRequests` 對每筆給 SHA-256（`engine/base` 的 `Sha256`）；比什麼由作者決定 | 開的時候多一次雜湊（36.7 MB 的內容，量測後寫進 dev_log） |
| (c) 照舊：asset 從位元組算，當規則 | asset 決定內容身分 | 違反「Engine 只提供機制」；單機遊戲也付代價 |

### 第 3 題：「載完才開始」（改舊 DI 3 的後半，11-13 核准過）

| 選項 | 內容 | 代價 |
|---|---|---|
| **(a) 作者的事（建議）** | asset 給「全部完成」的查詢；作者在生成世界的結束條件裡用；到達時刻要影響判定，走 `rollback.Input<T>` | 舊守衛（未載完就 ready 是錯誤）消失，靠作者 |
| (b) 照舊：Engine 規定進遊戲前載完 | 要有一個 Engine 認得的「Loading 結束」時點 | 和 `-build-world-phase-is-mechanism`（Loading 不是機制）衝突，等於推翻那一條 |

### 第 4 題：完成的結果照什麼順序進 `Assets<T>`（改舊 DI 2，11-13 核准過）

**依賴**：第 2、3 題照建議時，舊 DI 2 的原意（(c) 區的內容身分＋決定性）已經不在 asset 裡。

| 選項 | 內容 | 例子 |
|---|---|---|
| **(a) 照 Bevy：id 在請求時給，到了就套用（建議）** | id 與最終內容和完成時序無關；**緩的面**：哪一筆在哪一幀進來、跨批的順序、訊息順序依完成時序 | 23.7 MB 的 fbx 還在解碼，同時請求的 1 KB json 下一幀就能用 |
| (b) 照舊：只套用連續完成的前綴 | 套用的順序與批次切分也和完成時序無關 | 那個 json 要等 fbx 解完才進來；它保證的性質沒有 Feature 需要 |

### 第 5 題：圖片的型別與 stb adapter 放哪個 Module、何時做

**為什麼問**：本卡的任務說明（主會話 2026-11-21）寫「貼圖隨 SDL 批的 render 回來（照 Bevy 的 `bevy_image`）」。但查 Bevy 原始碼，`bevy_image` 是**獨立的 crate**：依賴 `bevy_asset`、`bevy_ecs` 等，不依賴 `bevy_render`；render、ui、text、sprite 都依賴它。

| 選項 | 內容 | 好處 | 代價 |
|---|---|---|---|
| **(C) 獨立的 `engine/modules/image`，隨 SDL 那批的第一個使用者做（建議）** | 本頁先定位置、授權、adapter（5.11）；SDL 那批做 | 照 Bevy 的實際切法：ui 排版要尺寸時不依賴 render；本批只做有直近使用者的部分（AGENTS §2 thin start） | loader 介面要等 SDL 那批才有真的第三方來驗 |
| (B) 獨立的 image Module，本批做 PNG | 同上，但在本批第 7 步做 | 積上去之前，就用真的第三方解碼器驗證 loader 介面（settings、上限、解碼錯誤）——**這不是 Feature** | 本批沒有使用者；多 3～5 h |
| (A) 放在 render Module 的 adapter，隨 SDL 那批 | 任務說明的讀法 | 少一個 Module | ui、text 要圖片尺寸時得依賴 render |

三個選項都滿足使用者決定 1：不走 SDL，只在一個 adapter。

### 第 6 題：預算

**寫不出數字的三項**：F1 全部內容冷啟動的載入時間、同時在飛的請求或解碼中的記憶體峰值、每幀套用的上限。放數字的位置在本批做（AS8）。

| 選項 | 內容 |
|---|---|
| (a) | 現在給數字 |
| **(b) 延後到第一個產品搬回時再定（建議）** | 實作段只做探索量測，標「不是判定」；RC-AS3 用暫時條件 |

## 13. 外部組件的下載

| 組件 | 來源 | 用途 | 授權 | 何時 |
|---|---|---|---|---|
| stb_image（`stb_image.h`） | nothings/stb，釘 commit | image Module 的 PNG 解碼 | MIT 或 Public Domain | image 那批（第 5 題 (C)：SDL 那批；(B)：本批第 7 步）。要使用者同意，到時再問 |

## 14. 範囲の限界

- **交換點、生成世界還沒有宣告**：第 5.5、5.13 節是需求，不是設計。實作的第 4 步照它們的宣告做；滿足不了的話先回報，不在 asset 裡繞過。
- **第 4 批還沒合併**：雜湊客製點與 `Summary` 的規則照修訂 6 第 3.2、3.3 節。合併時形狀不同，5.8.1 跟著改。
- **預算未定**（第 12 節第 6 題）：沒有載入時間與記憶體的判定。
- **載入中的 World 的 `Summary`**：asset 的 Resource 不寫進摘要（5.8.1），所以載入進度不同的兩個 World，摘要可能相同。比摘要要在「全部完成」之後比，才有意義。
- **loader 的跨平台逐位元**：本批的文字 loader 只做 UTF-8 驗證，原樣交出位元組。其他 loader 由各自的 Module 宣告；stb_image 不宣稱在邊界內。
- **「進判定前載完」靠作者，或走 `rollback.Input<T>`**（5.8 第 5 點）。
- **io 的多執行緒讀的契約**未確認（5.12），交給 threads、io 重審那批。
- **catalog 的 owner 未定**（第 12 節第 1 題）。

## 15. 觀察到、不在本批的（報給主會話）

1. **批次表第 6 組「asset」一列**的依賴要再加「生成世界（重做）」（5.13）。單位改成兩個（asset、world_asset）。第 12 節第 5 題選 (C) 時，SDL 那批的範圍要記上「image Module（stb adapter）」。
2. **v2 之前的 target 文件**：`target/runtime-composition.zh-Hant.md` §3a 與 DI 3（Loading 的 Ingress、資產 stage），以及 `target/README.md` K.3 表的 A3、A4 列，都建立在已拿掉的前提上。本批只動 `assets.zh-Hant.md` 的頂端標記，其餘歸「v2 之前的 target 文件由主會話一次改」那一項。
3. **打包時附第三方授權告示**：stb_image 加進「之後的小事」那一條。
4. **HANDOFF〈交使用者的〉「SDL 的 adapter、ufbx 歸哪一類」仍未定**。本頁照 JSON 的先例寫 ufbx 進 model Module 的 adapter，只當背景，不判。
5. **catalog 的 owner**：master 的 `AssetCatalog` 是 audio_editor 寫、遊戲與 ui_editor、model_viewer 讀的 Data Contract（`git grep -l AssetCatalog origin/master -- apps tools`）。owner 要在工具搬回之前定（AGENTS §1）；第 12 節第 1 題選 (ii) 時，要先定。
6. **生成世界那批的宣告要收第 5.13 節的 G-1～G-4，交換點那批要收第 5.5 節的 X-1～X-8。**

## 16. 停止條件

- AGENTS §14 冒頭的三條。
- 交換點或生成世界的宣告和第 5.5、5.13 節的需求衝突。
- 第 4 批合併後，雜湊客製點或 `Summary` 的規則和 5.8.1 的假設不同，而且影響 AS-6。
- 實作中發現某個 loader 必須碰 World（RC-AS2）。

## 17. 修訂 0 的敵對式檢查：處理對照

檢查者 Fable 5.2／high（作者 Opus 5.5）。證據 `declaration-r0-adversarial-check.md`。31 條全部採用。

| 條 | 處理 |
|---|---|
| B1 asset 的型別沒有雜湊函式 | 新增 5.8.1（逐型別）；第 10 節加 `Summary` 兩案；第 14 節加「載入中的 `Summary`」 |
| B2 生成世界期間結果進不來 | 新增 5.13（G-1～G-4）；AS-4 不釘死 `First`；2.2 依賴、第 14、15 節補上 |
| B3 路徑當持久鍵和轉檔矛盾 | 第 12 節第 1 題（(i)～(iii)）；5.2、5.10、AS-9、RC-AS6 改；第 10 節加 (i) 的案例 |
| B4 讀檔失敗路徑不可達、窗口 | 第 1 節、5.3、5.9 改成「先驗證再替換」；刪不可達句；scenario 第 3～5 步；寬限寫進 RC-AS7 與 5.14 A4 |
| S1 AS-5、AS-1 與帶過去矛盾 | AS-5 主詞改 store；AS-1 寫明過渡狀態；5.8 第 1 點 |
| S2 順序的緩的面 | 系譜表加「緩」欄；第 4 題改成 (a) 照 Bevy／(b) 照舊，寫明緩的面；批內順序不再是 DI |
| S3 X-6 不夠、執行緒測試無判定 | X-6 改成能指定交回的時點；等價測試四組；真執行緒 50 次、直接呼叫 1,000 次，都寫明判定 |
| S4 X-2、X-4 越界 | 改寫成結果需求；X-4 的做法放 asset 這邊；第 8 節加「結果怎麼進 World」一列（Bevy 的 channel 為退掉的選項）；E3 的記法寫明 |
| S5 帶過去的細節 | loader 表不帶；訊息不帶、讀者對帳；移回的處理；Handle 安全失效；用 `Options` 關 |
| S6 catalog 的 owner | 改成 owner 未定；第 12 節第 1 題、第 15 節第 5 點；F6 照 master 改 |
| S7 內容身分的系譜與第 3 題 | 拆成第 2、3 題各給選項，排在第 4 題之前；DET-2 的代價；(b) 來源位元組雜湊；引第 4 批第 21 節；舊守衛消失寫進系譜 |
| S8 狀態機的洞 | `Failed` 再請求＝重新載入；取消；件數等式加兩項；第 10 節加取消與丟棄的案例 |
| S9 不分型別的失敗訊息 | 加 `AssetLoadFailed`；訊息會被修剪、讀者對帳 |
| S10 去重「Bevy 同」不實 | 改寫，列進第 11 節的差異 |
| S11 loader 介面依賴 ECS、系譜標錯 | 拆兩個單位；第 8 節加「單位的拆法」一列；loader 的系譜改成 master；整檔的取捨寫明 |
| S12 只定形狀和推不出矛盾 | 第 9 節只留引擎格式一項；`Modified`、`#label`、依賴的形狀都拿掉 |
| S13 第 1 題少選項、PNG 無使用者 | 第 5 題加 (C) 並建議它；(B) 的好處標明不是 Feature；PNG 改稱回歸測試，不宣稱邊界；任務說明的出處寫明；Bevy 的依賴寫正確 |
| S14 預算沒有位置 | AS8 擴張點（每幀、在飛、loader 上限可覆寫）；X-8；RC-AS3 的暫時條件 |
| S15 A1～A6 | 新增 5.14 |
| nit 1 E0／E2 界線、AS-2 守衛矛盾 | 5.4 寫明型別與 loader 同一個動作；AS-2 守衛改 |
| nit 2 `LoaderThrew` | 5.7 加註，`catch (...)` 含 `bad_alloc` |
| nit 3 套用位置的 Feature | 改成 Client 的模擬 World；理由改準 |
| nit 4 第 4 批修訂號 | 改為修訂 6（07c80c4c） |
| nit 5 第三方的模型分派 | 改 Opus／high |
| nit 6 型別數、測試圖 | 第 10 節寫明測試型別、測試圖由程式產生 |
| nit 7 stb 的執行緒面 | 5.11 加 `STBI_NO_FAILURE_STRINGS`，不用全域旗標 |
| nit 8 計數是例外 | 5.2 寫明 |
| nit 9 第 3 題與 DI 對齊 | AS-5、AS-7、AS-9 加各題選項的條件句 |
| nit 10 影響漏列 | 第 6 節第 4 項補上 |
| nit 11 二進位 loader | 拿掉（2.2） |
| nit 12 共用快取的理由、手動重讀 | 理由改成「沒有 Feature、沒有量測」；2.2 提 `RefreshContent` |

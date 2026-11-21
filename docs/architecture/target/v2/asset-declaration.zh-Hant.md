# 事前宣告：asset Module（`v8-arch-gyo2-asset`）

更新：2026-11-21（修訂 3）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：使用者 2026-11-22 撤回凍結機制、把 Product 層 Resource 交給編排層（決定 13）；送敵對式檢查，之後交動工前核准。實作要等第 5 批的「多個 World、交換點」與「生成世界（重做）」做完，而且使用者說開始。**

**經過**：
- 修訂 0（f93edb7e）送 Fable 5.2／high 敵對式檢查，blocker 4、should 15、nit 12，全部採用成修訂 1（b050e13b），對照在第 17 節。
- 修訂 2（7adbcb77）：使用者要「參考其他家如何做 assets 的管理」。卡片補了管理面對照，經敵對式檢查（blocker 4、should 9、nit 4）改正後寫進第 4 節，對照在第 18 節；使用者選了 (a')。
- 修訂 2 送 Fable 5.2／high 敵對式檢查，blocker 3、should 12、nit 10（證據 `declaration-r2-adversarial-check.md`），處理在第 19 節。
- **修訂 3**：使用者在 asset 卡的 side chat 定了十件（12.1 決定 4～13）：時間上限放寬（已作廢）；跨執行緒約束現在寫進契約；**作者只用 id**；**資產管理在 World 之外**；大地圖先照 Bevy；第 4 題（到了就可見＋就緒查詢）；第 2 題（原始位元組雜湊，預設開）；**依賴由 Engine 推出**；catalog 先比對既有設計（決定 12）；撤回凍結機制、Product 層 Resource 交編排層（決定 13，2026-11-22，第 22 節）。修訂 3 送 Fable 5.2／high 敵對式檢查（blocker 4、should 12、nit 5，證據 `declaration-r3-adversarial-check.md`），全部處理，對照在第 20 節。本頁的形狀因此重寫：持久鍵改成 catalog 的 id；`Assets<T>` 不再每個 World 一份；CarryOver、時間窗口、失敗搬回、訊息對帳那一套整個拿掉。

**性質**：實作段第 6 組的 Module 的事前宣告。涉及：
- AGENTS §2 的基盤 Architecture；
- §3 的 Architecture Delta：新 Subsystem、改寫已核准的 Design Invariants、推翻 11-11 方向 2（使用者已同意）、新 Data Contract（catalog）；
- §4 的系譜與決勝理由；
- §6 的外部組件 adapter；
- §8 的 Data Contract。

**用詞**：照日誌、trace 的宣告，即設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。使用者的決定、卡片的判定、待使用者定的，分開記在第 12 節。範圍的代號用 R1～R10，Design Invariants 用 AS-1～AS-13，兩者不混。

**核心一句**（每一項都拿來對照）：asset 是**機制**。作者只說「我要 id 為 X 的內容」；載入、就緒、生命週期由 Engine 提供。asset 告訴外面三件事——載到哪了、成了沒有、為什麼沒成——並讓 World 唯讀地看到載好的值。

asset **不決定**以下四件事：

| 不決定的事 | 歸誰 |
|---|---|
| 要不要等載完 | 作者 |
| 失敗後怎麼辦 | 作者 |
| 內容身分怎麼比、不同時怎麼辦 | 作者或網路 Module（asset 只提供原始位元組雜湊，決定 10） |
| 檔案是什麼格式 | 擁有那個型別的 Module |

## 0. 根據與讀的版本

**程式**：`gyo2` f25837b0（#258 trace 之後）；修訂 2 起合入 2acf9cee（#261 只改登記與 CMake，本頁引用的原始碼行號不受影響）。

**使用者的決定**（出處見各列；第 12.1 節有原話與比過的選項）：

| 代號 | 內容 | 本頁怎麼用 |
|---|---|---|
| `v8-arch-gyo2-asset-three-decisions`（2026-11-21，主會話轉來） | 「asset 三件照建議」：(1) 圖片解碼不走 SDL，先用 stb_image，只在一個 adapter；(2) DI 照新前提重寫；(3) 起步照 Bevy 執行期讀原始檔，接得上日後的事先轉檔 | 5.11、第 7 節、5.10 |
| `v8-arch-gyo2-asset-immutable-shared`（2026-11-21，本卡） | 「照 (a')，寫修訂 2」：載好的值不可變、共享唯讀 | AS-10（「每 World 各自解碼」的部分由決定 7 取代） |
| `v8-arch-gyo2-asset-exclude-non-decisions`（2026-11-21，本卡） | 「排除掉不需要我決策的」 | 第 12 節的分法；第 2、4 題依修訂 2 檢查 B3 拉回，使用者已在 side chat 定（決定 9、10） |
| 時間上限放寬（2026-11-21，side chat 轉來）——**作廢** | 設計段放寬到約 11 h；之後又放寬到 13 h。兩次都作廢：設計段沒有時間上限的規則 | 0.1 |
| 跨執行緒約束現在寫進契約（同上） | 多執行緒是已定前提，不等 RC 才寫 | AS-10、AS-11 |
| 作者只用 id（同上） | 持久鍵＝id；id→路徑對照表的機制與格式歸資產管理，執行期可用 | 5.2、5.3、AS-5、AS-6、AS-12 |
| 資產管理在 World 外（同上） | World 只持 id；換 World、多 World 不是 asset 的事；推翻 11-11 方向 2 | 5.4、AS-1、AS-4 |
| 大地圖先照 Bevy（同上） | 不內建串流；日後用 Module 加 | 2.2、RC-AS1 |
| 第 4 題：到了就可見（同上） | 單筆到了就讓 World 看到（照 Bevy）＋Engine 提供一組 id 的就緒查詢；轉場歸作者 | 5.6、5.7、AS-5 |
| 第 2 題：原始位元組雜湊（同上） | asset 對每個 id 的原始位元組算雜湊，作為能力，預設開、可關；比不比與不同時怎麼辦歸作者或網路 Module | 5.9、AS-7 |
| 撤回凍結機制（2026-11-22，side chat 轉來） | 設定在建立時帶；loader 帶鎖隨時登記；Module 拿到 Product 層 store 的路交編排層 | 5.4、12.3 第 13 題、第 22 節 |
| 依賴由 Engine 推出（同上） | loader 宣告引用；就緒查詢的「好了」包含整串依賴 | 5.17、AS-13 |
| `v8-arch-gyo2-parallel-asset-and-stale-mutants`（2026-11-21） | IO 執行緒加交換點；批次表依賴含交換點 | 2.2 |
| `v8-arch-assets-bevy-model`（2026-11-11，kernel HANDOFF:144） | 不選 World，照 Bevy 拆成請求狀態、每型別一份 `Assets<T>`、GPU 資源；loader 由各 plugin 登記 | 方向 2「放在使用它的 World」由決定 7 推翻；三類拆法與「System 不持指標」保留 |
| `v8-arch-target-di-approved`（2026-11-13） | 舊 DI 1～3 與 Replacement Criteria 核准 | 第 7 節的系譜 |
| `v8-arch-gyo2-build-world-phase-is-mechanism`（2026-11-16） | 有沒有 Loading 階段由作者決定 | 5.7、12.2 第 3 題 |
| `v8-arch-gyo2-save-unaware-means-format-only`（2026-11-16） | Engine 不認得存檔的格式 | 5.9 |
| `v8-arch-gyo2-module-plugin-owner-definition`、`v8-arch-gyo2-json-adapters-dir` | Module＝我們維護、介面是我們的；第三方放該 Module 的 `src/adapters/<組件>/` | 5.1、5.11 |
| `v8-arch-gyo2-engine-not-rigid`、`-mechanism-not-conservative` | 預設方便、不禁止別的做法；建機制這一輪不選保守的選項 | 全頁 |
| `v8-arch-gyo2-infrastructure-before-products`、`-migration-needs-consent` | 先建基礎；搬產品要使用者同意 | 3.2 的 Feature 只用來推需求 |
| `v8-arch-gyo2-replace-order-author-choice` | 使用者點名 BigMap 是有的 Feature | 3.3 |

side chat 轉來的九件，卡片已照四樣轉主會話記 HANDOFF（2026-11-21）；記好之前，HANDOFF 裡查不到它們。

**已合併、本頁要對齊的**：
- [編排層](orchestration.zh-Hant.md)：O-2 生命週期（`Configure` 在產品層一次、`Build` 每個 World）、O-5 啟動報告、O-6 登記檔、O-7 準備時機、O-8 替換、O-10 帶過去、O-11。
- `target/error-handling.md`〈Error levels〉：`…Error`＝E1，`…Failure`＝E2；Engine 的 Module 不寫日誌。
- [事件游標](event-cursor-declaration.zh-Hant.md)：`Message`；上限預設 1024。
- Kernel 的 `DiagnosticsWriter<Record>`（`engine/ecs/kernel/include/engine/kernel/Params.hpp:709`）。
- [時間＋事件](time-events-declaration.zh-Hant.md)第 3.6 節：預設骨架 `Main`（`First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）。
- 先例：「不認得 ECS 的函式庫＋World 端的 Module」——`logging`／`world_logging`（`config/modules.csv`）、`time`／`world_time`（`time` 在 core，#261 起不登記）。
- 登記單位的實例活到 Product 結束（`RegistrationUnit.hpp` 的註解：「a unit keeps what it records per World by WorldName()」）；`ProductSetup` 沒有選項包、沒有 Product 層的共用值（`Setup.hpp:88-110`）。

**進行中、本頁要對齊的**：
- **第 4 批**（draft #259）：分支 `claude/gyo2-authority-rollback-bda9ba` 的宣告修訂 6（07c80c4c）。3.2 雜湊客製點；3.3「Engine 的 Module 放進 World 的型別都附雜湊函式」；4.4 digest 不含 id；第 21 節「到達時刻要影響判定，走 `rollback.Input<T>`」。
- **第 5 批**：交換點、生成世界（重做）都還沒有宣告。5.6、5.13 寫成給它們的需求。

**參考，不帶**：
- master 的 `engine/asset`：`AssetManager`、`AssetCatalog`、`hot_reload/`、`loaders/`、`sdl_image/SdlImageTextureLoader`。
- 舊的 [target/assets.zh-Hant.md](../assets.zh-Hant.md)：11-11 的方向與 11-13 的 DI。
- `target/runtime-composition.zh-Hant.md` §3a 與 DI 3。
- 藍圖的資產不變量 A1～A6：`plans/kernel/blueprint.zh-Hant.md:28`、`:66`。逐條判在 5.15。

**證據**：主 checkout 的 `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-asset/`，git 不追蹤。
- `reference-survey.md`：別家原始碼（載入面）；master 的內容種類與大小。
- `reference-survey-management.md`、`management-cross-table.md`：管理面的出處與交叉比對。
- `declaration-r0-adversarial-check.md`、`management-comparison-adversarial-check.md`、`declaration-r2-adversarial-check.md`：三次檢查（後兩份是摘要，全文在卡片會話紀錄）。
- `management-comparison-proposal.md`：修訂 2 送檢的對照稿。

---

## 0.1 時間估計

| 段 | 估計 | 內容 |
|---|---|---|
| 設計段 | 原估 4～6 h；到修訂 2 約 8.5 h；修訂 3 的重寫（含決定 9～11）＋送檢約 4 h，合計約 12.5 h | 只是資訊，不是停止條件（2026-11-21 使用者更正：時間盒子只用在查問題，AGENTS §14 只管測試與判定；見 12.1 決定 4 的作廢註記） |
| 實作段 | 28～42 h（推定；修訂 2 是 22～34 h） | 多了 catalog 的 Data Contract、World 外的 store 與無鎖的請求路徑；交換點與生成世界的形狀還沒定，它們的宣告核准後重估 |

## 1. 這一批做什麼（先用遊戲講）

FPS 的 Client 進對局之前，要載角色模型、武器貼圖、字型、音效、規則與地圖的 JSON。

**內容先有 catalog**：內容根目錄的 `content.json` 列出 catalog 檔；catalog（Data Contract）寫「id `rules.movement` 是文字，在 `rules/movement_rules.json`」「id `char.hero` 是模型，在 `models/hero.fbx`」。catalog 由工具、Script 或手寫產生（AGENTS §8）。作者**只認得 id**，不管檔在哪、怎麼解碼。

步驟：
1. 生成世界階段，作者的 System 對 `AssetRequests` 說：「我要 `rules.movement`。」**當場**拿到 `Handle<Text>`。
2. 讀檔與解碼由 World 外的資產管理（`AssetStore`）在 World 執行緒之外做，不碰 World。
3. 載好的值放進資產管理，**不可變**。
4. 每個 World 在自己的套用點（預設 `Main`/`First`）換成資產管理最新的一份唯讀視圖；一幀之內看到的都是同一份。同時在 `AssetChanges` 看到「加入了」。
5. 作者的 System 看狀態，全部載完才離開生成世界——等不等是作者的事。

**失敗時**：例如檔案不在，狀態是「失敗」，帶 `AssetLoadFailure`（E2）。asset 不記日誌、不自己重試，World 照跑；作者決定停下、再請求、換預設內容或顯示錯誤。日誌要記，就在自己的 System 讀 `AssetChanges` 的失敗。

**中途讀檔、換 World**：不是 asset 的事。資產管理在 World 外，Handle 的引用計數跨 World 有效。作者照 O-8 在現在的 World 驗證存檔要的 id 都載得到，再發 `ReplaceRequest`；新 World 請求同樣的 id，資產還活著就直接命中。

**Client 的 Sim 與 Present 都用角色模型**：兩個 World 請求同一個 id，資產管理只載一次、只存一份，兩邊唯讀共用。

**模型檢視器的 `info` 指令**：不建 World，只連不認得 ECS 的 `asset` 函式庫與模型 Module 的 loader；把位元組交進去，拿回值。

## 2. 範圍

### 2.1 這一批做的（動工前核准之後；實作等第 5 批）

| 代號 | 內容 |
|---|---|
| R1 | 兩個單位（照 `logging`／`world_logging` 的先例；卡片照 L17 判，第 8 節）：**`engine/modules/asset`**（`GYO::Asset`，library-only，不認得 ECS）——id 與 catalog（Data Contract 的讀、寫、驗證）、loader 介面、整檔讀取（經 io）、文字 loader、`AssetStore`（World 外的資產管理）、`Handle<T>`、`AssetRef<T>`；**`engine/modules/world_asset`**（`GYO::WorldAsset`，編排層 `Module` 的子類）——擁有 Product 的 `AssetStore`（建立參數 `AssetOptions`）、每個 World 的 `AssetFrame`、請求入口與 `AssetChanges`、套用點的 System 與準備函式、雜湊函式 |
| R2 | catalog：id→（型別、路徑、settings）的對照表；格式、Version、Validation Rule 是 asset 擁有的 Data Contract；執行期讀（不是 build 的產物） |
| R3 | `AssetStore`：以 id 請求、引用計數、沒人用就放掉、`Failed` 之後可以再請求；載好的值不可變、共享唯讀；**依賴追蹤**（loader 宣告引用、連同依賴的狀態、循環偵測）；**原始位元組雜湊**（預設開） |
| R4 | loader 的介面與登記入口：擁有型別的 Module 登記（5.16） |
| R5 | 讀檔與解碼在 World 執行緒之外；World 端的請求、放掉、讀取不上鎖 |
| R6 | 每個 World 的 `AssetFrame` 與唯讀視圖、套用點、狀態查詢、一組 id 的**就緒查詢**（連同依賴）、變更清單 `AssetChanges` |
| R7 | 錯誤：請求級與 id 級的 `AssetLoadFailure`（E2）、依賴失敗、catalog 與登記的啟動錯誤；丟棄件數（E3 的出口不在本批，5.8） |
| R8 | asset 放進 World 的型別的雜湊函式（5.9.1；第 4 批 3.3） |
| R9 | 上限與擴張點：在飛上限、loader 的位元組上限可由產品覆寫；每個包的變更清單上限（5.6） |
| R10 | 新頁 `target/v2/asset.zh-Hant.md`：Design Invariants、Replacement Criteria。舊 `target/assets.zh-Hant.md` 頂端標「已由 v2 取代」 |

### 2.2 不在這一批

- **事先轉檔的工具與它的資料契約**（三件之 3）。本批只保證接得上（5.10）。
- **image Module 與 stb_image**：隨 SDL 那批的第一個使用者做（12.2 第 5 題）。本頁先定位置、授權與 adapter（5.11）。
- **其他 loader**：模型（ufbx，進 model Module；SDL adapter 與 ufbx 歸哪一類，HANDOFF 未定）、音效（audio 重審那批）、字型（text Module，SDL 那批）、二進位（沒有點名的 Feature）。
- **GPU 資源**（Bevy `RenderAssets<T>`）：歸 render，隨 SDL 那批。
- **大地圖的串流**（決定 8）：照 Bevy 不內建。asset 只給上限與 region 級的 id；串流的規則與「進判定」屬日後的地圖 Module（RC-AS1）。
- **沒有點名 Feature 的，本批不做，也不在介面上先留形狀**：熱重載與 `Modified`（model_viewer 的 `RefreshContent` 是手動重讀，工具可以直接呼叫 loader）、一個檔分出多個子 asset、作者在執行期自己加值（Bevy `Assets::add`）、資料夾自動產生 catalog（產品可以在程式裡加 catalog 項目，5.3）。
- 交換點、生成世界本身的設計（第 5 批）。threads、io 的重審（批次表第 6 組）；本批只交需求（5.6、5.12）。

**實作的依賴**：編排層（#244）；第 5 批的多個 World、交換點；第 5 批的生成世界（重做）；第 4 批合併（雜湊客製點、`Summary` 的規則）；**編排層的 Product 層 Resource**（Module 拿到 Product 的 store 來登記 loader；12.3 第 13 題交主會話，5.4）；JSON Module（catalog 的格式）。

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

上面會積的東西：render、text、ui、audio、model 的 loader 與 GPU 上傳；每個產品的生成世界；讀檔；工具產生的 catalog 與事先轉檔；**內容與存檔裡記的 id**。

以下幾樣一改，上面每個 loader、每個產品、甚至**已存的內容與玩家的存檔**都要跟著改：
- 持久鍵（id）與 catalog 的格式；
- 資產管理放在哪、World 怎麼看到它；
- Handle 的生命週期；
- 值可不可變、能不能跨執行緒唯讀。

### 3.2 予見的根據 → 對 asset 的要求（Feature → Contract → Architecture）

**Feature 的出處**：master 的產品 `apps/object_fps_pvp`、工具 `tools/{model_viewer,voxel_map_editor,audio_editor,ui_editor}`、內容 `assets/object_fps_pvp`。逐檔與行號在 `reference-survey.md` 第 2 節。只用來推需求，**不搬產品**。

| # | Feature（誰用、看得到什麼） | 用到的內容種類（master 實查） | 對 asset 的要求 | 本批 |
|---|---|---|---|---|
| F1 | 遊戲 Client：進對局前載完角色、武器、字型、音效、UI、地圖；Sim 與 Present 都用角色模型（`PlayerAnimationContent` 被 `Sim/PlayerHitbox`、`Sim/PlayerPose`、`Present/PlayerPresentation` 用；`git grep -l PlayerAnimationContent origin/master -- apps`） | fbx 4、png 5、ttf 1、wav 4、json 22（`PvpApplication.cpp:958-962` 登記五種 loader） | 多種型別各自的 loader；以 id 請求、當場拿到 Handle；查「全部完成」；失敗帶原因；**兩個 World 用同一內容只載一次**（23.7 MB 的 fbx）；**模型引用的貼圖由 Engine 推出**，作者只寫模型的 id（viewmodel json 會引用 fbx 與 png，master 是產品自己依序載） | 機制做；fbx／png／wav／ttf 的 loader 不做 |
| F2 | 遊戲 Match（無畫面）：載角色模型與規則算判定（`PlayerAnimationContent.cpp:89-90`） | fbx、json | **不需要 SDL 與視窗**也能載；兩端載出的內容身分由作者比，asset 給原始位元組雜湊；**沒有貼圖 loader 時，模型引用的貼圖略過、不擋就緒**（5.17） | 機制做（決定 10） |
| F3 | 讀檔（`-restore-entry-scope-player-save`）：遊戲中讀檔＝替換 World | 同 F1 | 替換前能先驗證內容（O-8）；替換時不必重讀；**存檔裡的參照跨版本有效** | 做：id 是持久鍵（5.2）；換 World 不是 asset 的事（5.4） |
| F4 | 模型檢視器：`info`、`models` 不建 World（`ModelContent.cpp:66-69`、`:120` 直接用 loader） | fbx、png、wav | loader **不依賴 ECS**，可以直接呼叫 | 做（R1 的拆法） |
| F5 | ui_editor：預覽字型與圖片（`AssetPreviewContext.cpp:92-171`）；UI 文件用 catalog id 參照資產（`UiDocumentBridge.cpp:137-165`） | ttf、png、json | 同一個機制；**UI 文件記 id** | 機制做 |
| F6 | audio_editor：`convert` 把 wav 轉成引擎格式的 clip、**寫進 catalog**（`tools/audio_editor/src/Main.cpp:29-36`） | wav、json | **事先轉檔現在就有一例**；工具照 catalog 的 Data Contract 寫，執行期不知道誰寫的 | catalog 的格式與讀寫做（R2）；工具不做 |
| F7 | 地圖編輯器：讀寫自己的地圖檔（`voxel_map_editor/src/FileAccess.cpp:21-27`） | json、voxel mesh json | 編輯器的檔是它自己的；遊戲讀它的輸出時經 catalog 與 loader | 只保證介面（5.10） |
| F8 | BigMap（使用者點名）：約 1 km²、242 MB、每個 region ≤ 16 MiB | 地圖 region | 一個大內容能拆成許多 id；單一 asset 的上限可設；**同時在飛要有上限**（不然全部同時讀進記憶體）；串流的規則照決定 8 屬日後的地圖 Module | 做上限（R9）與 region 級 id |
| F9 | 2D 橫版遊戲（`-engine-scope-general`） | png、json | 單機、不需要 PvP 的任何東西；catalog 可以手寫或由產品程式產生 | 做 |

**推不出、不放進來的**：熱重載（master 的 `AssetWatcher` 在 `apps/`、`tools/` 0 件呼叫）；子 asset（ufbx loader 一檔一個值）；二進位 loader。

### 3.3 規模與預算

**已知的規模**（master 實查）：遊戲內容 36 個檔、36.7 MB；最大 fbx 23.7 MB、ttf 4.5 MB、png 1.34 MB、json 1.45 MB。BigMap 242 MB，region ≤ 16 MiB（region 的個數未查）。

**預算：寫不出數字，延後到第一個產品搬回時**（12.2 第 6 題）。缺：載入時間；同時在飛的請求數或解碼中的記憶體峰值；catalog 的項目數與啟動時讀 catalog 的時間。

**本批先做放數字的位置**：在飛上限、loader 的位元組上限是 Product 級設定（R9）；預設「不設限」，標明不是判定。

## 4. 別家怎麼做（同一種技術比）

**來源標記**：
- **〔源〕**＝本機原始碼或本機文件查證。Bevy `6a911653f`（0.20.0-dev）、Unity Entities 1.5.0、flecs 本機版。
- **〔文〕**＝官方文件（2026-11-21 讀：Godot 4.7、Unreal 5.x、Unity Addressables 2.3）。
- **〔檢〕**＝檢查者查到，卡片沒有親自核。**〔知〕**＝一般知識。**〔未〕**＝沒查。
- 逐條出處：`reference-survey.md`、`reference-survey-management.md`。

### 4.1 載入面

| 家 | 核心認不認得格式 | 身分、去重、Handle | 結果怎麼進 World、順序 | 執行期讀什麼；持久鍵 | 圖片誰解碼 | 為什麼這樣做 |
|---|---|---|---|---|---|---|
| Bevy〔源〕 | 不認得。`AssetLoader` trait（`bevy_asset/src/loader.rs:32-49`），由各 crate 登記 | `load` 當場回 `Handle`（`server/mod.rs:364`）；**去重的鍵是（路徑、型別）**，settings 被忽略，原始碼自己標成缺陷（issue 11111，`server/info.rs:265-271`）；`Failed`／`NotLoaded` 再請求會重新載（`info.rs:279-286`）；強 Handle 被丟時經 channel 通知（`handle.rs:81-91`）；歸零後到下一次處理前又有新 Handle，就不移除（`assets.rs:578-580`） | IO 在 `IoTaskPool`（`server/mod.rs:427`）；`handle_internal_asset_events` 在 `PreUpdate` **照完成順序**套用（`:1890-1895`、`lib.rs:423-433`）；不分型別的 `UntypedAssetLoadFailedEvent`（`lib.rs:421`） | 預設原始檔（`lib.rs:297-336`）；可選 `Processed`，**輸出的路徑和原始檔相同**（`meta.rs:70-86`）；持久鍵＝路徑 | `bevy_image` 是**獨立的 crate**，不依賴 `bevy_render`（`bevy_image/Cargo.toml:59-79`）；render、ui、text、sprite 都依賴它 | 原始檔讓迭代快；轉檔不改路徑；圖片獨立讓 ui、text 不依賴 render |
| Unity Entities〔源〕 | 不認得。執行期只載 build 產生的 content archive 與 catalog（`RuntimeContentManager.cs:30-53`、`:412`） | `WeakObjectReference<T>`（`:14`）：`LoadAsync`、`LoadingStatus`、`Release`（`:45-85`），引用計數 | `RuntimeContentSystem` 每幀處理排隊的命令（`RuntimeContentSystem.cs:95`）；狀態 `None`、`Queued`、`Loading`、`Completed`、`Error` | **事先轉好的格式**；持久鍵＝GUID | Editor 匯入時〔知〕 | 發佈的內容一致；GUID 讓改名、換格式不影響參照；代價是要有 build pipeline |
| flecs〔源〕 | 沒有 asset 系統（`flecs.h` 搜 `asset` 0 件，正例 `ecs_` 1761 件） | — | — | — | — | 只做 ECS |
| Unreal〔文〕 | 不認得（`UFactory` 匯入〔知〕） | `FSoftObjectPath`、`FStreamableManager` 的 Handle | 非同步；完成時呼叫委派，可延後幾幀 | cook 後的 pak；持久鍵＝物件路徑，改名靠 redirector | 匯入時〔知〕 | cook 讓平台格式事先備好 |
| Godot〔文〕 | `ResourceFormatLoader` 登記，依序試 | `Resource` 引用計數；全域快取以路徑為鍵 | 背景執行緒；作者輪詢狀態，未完成時取值會阻塞 | 匯入結果在 `.godot/imported/`；**轉檔一軸**路徑不變；**改名一軸**：UID＋備用路徑 | 匯入時 | 使用者看到原始檔的路徑；UID 讓改名不斷參照 |
| O3DE〔知〕 | Asset Processor 依副檔名轉換 | `Asset<T>`＋`AssetId`（GUID） | 執行緒載、主執行緒派發 | Asset Processor 的輸出；持久鍵＝GUID | Image Builder | 和 Unity 同一路 |
| master（GYO 舊） | 半認得：核心帶 catalog 解析 | `AssetCatalog` 邏輯 id；歸零後保留 N 幀（PvP 120，`PvpApplication.cpp:102-103`） | 主執行緒每幀輪詢，`maxLoadsPerFrame = 2` | 原始檔＋工具轉出的 clip；持久鍵＝catalog id | SDL3_image | — |

### 4.2 管理面（交叉比對）

| 面向 | Bevy | Unity Entities | Godot 4 | Unreal 5 | Addressables | master | 本頁 |
|---|---|---|---|---|---|---|---|
| 資產管理的範圍 | 每個 App 一個 `AssetServer`，clone 進渲染 World（`bevy_render/src/lib.rs:427,435`）〔源〕 | `RuntimeContentManager` 是 static、整個行程一份（`:27`），管 Object 與 content scene（`:1042`）；ECS 實體場景載入要指定 World（`SceneSystem.cs:381`）〔源〕 | 全域〔文〕 | Asset Manager 全域單例〔文〕 | 全域〔文〕 | 一個 `AssetManager`，跨 World 持指標 | **Product 一份**，在 World 外（AS-1） |
| 值存在哪 | main World 的 `Assets<A>`；渲染 World 另有衍生的 `RenderAssets<T>`〔源〕 | Object 全域；實體場景資料每 World 一份（`SerializeUtility.cs:1079`）〔源〕 | 全域快取〔文〕 | UObject〔知〕 | bundle 內〔文〕 | `AssetManager` 內 | `AssetStore`；World 只有唯讀視圖 |
| 兩個 World 要同一內容 | 只在 main 載，渲染經 Extract 拿衍生物 | Object 共用；實體場景資料各 World 各載 | 一棵場景樹，不適用〔文〕 | 〔未〕 | 〔未〕 | 同一物件 | 一份，唯讀共用 |
| 值可不可變 | 可變：`get_mut`（`assets.rs:440`）〔源〕 | blob 唯讀、限 unmanaged 型別（`Documentation~/blob-assets-concept.md:7`）〔源〕 | 可變、共享〔文〕 | 〔未〕 | 〔未〕 | — | **不可變、共享唯讀**（AS-10） |
| 何時卸載 | 最後一個強 Handle 消失，下一個 `PreUpdate` 移除，無寬限（`assets.rs:570-587`）；GPU 那份等 `Unused`（`render_asset.rs:332-333`）〔源〕 | 引用計數傳到檔、archive；釋放排隊每幀處理（`:514`、`:685`）〔源〕 | 歸零即釋放〔文〕 | 完成委派後放硬參照；Handle 有效時保活；之後交 GC〔文／檢〕 | 記憶體以 bundle 為單位放〔文〕 | 歸零後保留 120 幀 | 歸零後下一次維護移除，無寬限；維護前再請求就救回（5.5） |
| 去重的鍵 | 路徑＋型別〔源〕 | GUID〔源〕 | 路徑〔文〕 | 物件路徑〔文〕 | 〔未〕 | catalog id | **id**（settings 在 catalog 裡） |
| 改名、搬移 | 斷 | GUID，不斷 | UID＋備用路徑〔文〕 | redirector〔文〕 | GUID〔文〕 | catalog id，不斷 | **id，不斷**：只改 catalog 的路徑 |
| 依賴、分組 | 「連同依賴載完」的狀態（`info.rs:30-31`）〔源〕 | 依賴組〔源〕 | 依賴自動遞迴〔文〕 | Primary／Secondary＋bundle〔文〕 | label 批次〔文〕 | — | **Engine 推出**：loader 宣告引用（5.17） |
| 上限 | 每幀上傳位元組（`render_asset.rs:509`）〔源〕 | 〔未〕 | 文件沒看到 | 串流池〔檢〕 | bundle 快取上限〔文〕 | 每幀 2 個 | 在飛、loader 位元組（R9）；沒有每次發佈的上限（5.6） |
| 結果怎麼讓 World 看到 | `PreUpdate` 照完成順序〔源〕 | 每幀處理命令〔源〕 | 作者輪詢〔文〕 | 委派〔文〕 | 完成事件〔文〕 | 主執行緒輪詢 | 完成了、下一次維護就發佈；每個 World 的套用點換視圖（決定 9） |
| 失敗怎麼告知 | `Failed`＋失敗事件〔源〕 | 狀態 `Error`〔源〕 | FAILED，沒有細節〔文〕 | 缺檔也呼叫完成委派〔文〕 | handle 狀態〔文〕 | — | 狀態＋`AssetChanges`（E2；不用 Kernel 的 `Message`，5.7） |
| 熱重載 | 監看＋`Modified`〔源〕 | 〔未〕 | 原地刷新〔文〕 | 〔未〕 | 〔未〕 | Engine 有、產品沒用 | 不在本批 |

### 4.3 共通、分歧、本頁的位置

**共通**：核心不認得格式；身分在請求時就給；圖片不用視窗層解碼；持久鍵不隨「原始→轉檔」改變；**資產管理都在 World 外或高於 World**（Bevy 每 App，其餘行程單例）。

**本頁的位置**（使用者的整理經 side chat 轉來，卡片照它寫）：
- **管理骨架像 Unity Entities**：id 當持久鍵、資產管理在 World 外、值不可變。
- **載入方式像 Bevy**：執行期讀原始檔、各 Module 登記 loader、不內建串流。
- **和 Unity 的差別**：執行期就讀 id 對照表（catalog），不靠 build 的產物。

**仍和別家不同的**（第 11 節；另見那裡的完整清單）：Product 一份而不是行程一份（GYO 沒有行程級靜態物件的先例；回主選單另建 Product 時資產重載）；去重的鍵只有 id，settings 隨 catalog 項目；值不可變（Bevy、Godot 可變）。

## 5. 設計（Contract）

### 5.1 位置、名字、依賴（卡片照 L17 判，照 N11～N17、O-6 與 `logging`／`world_logging` 的先例）

| 單位 | 目錄 | target | 登記檔 | 依賴 | 內容 |
|---|---|---|---|---|---|
| asset | `engine/modules/asset` | `GYO::Asset` | library_only=yes、default=no | base；private io、json | `AssetId`（id）、catalog 的讀寫與驗證、loader 的介面（`LoadContext`、`LoaderFailure`）、整檔讀取、文字 loader、`AssetStore`（含依賴圖、原始位元組雜湊、快照與交接槽）、`Handle<T>`、`AssetRef<T>` |
| world_asset | `engine/modules/world_asset` | `GYO::WorldAsset` | library_only=no、default=yes | asset、base、kernel、orchestration；threads 看交換點 | 擁有 Product 的 `AssetStore`（建立參數 `AssetOptions`）；每個 World 的 `AssetRequests`、`AssetFrame`（`Assets<T>` 是它的型別化讀法）、`AssetChanges`；套用點 System、準備函式、雜湊函式 |

- **為什麼拆兩個**：F4、F6 的工具與擁有型別的 Module 只要 loader 與 catalog，不該為此連上 kernel、orchestration。`AssetStore` 也不認得 ECS，工具不建 World 也能用。
- **屬於哪一層**：都放 `engine/modules`（會長大的 Module 層）。
- **為什麼不是 Plugin**：都是我們維護、介面是我們的。
- **`default=yes`**：照「Module 預設全帶、產品可以關」。

命名：
- World 外的資產管理叫 `AssetStore`。不照 Bevy 叫 `AssetServer`：「Server」在 GYO 指 PvP 的 Match 伺服器（`target/naming.zh-Hant.md` DI 4）；不叫 `AssetManager`：那是 master 的總管物件，11-11 不採。
- id 的型別叫 `AssetId`：作者寫的、存檔裡記的、catalog 裡的那個字串（例：`char.hero`）。修訂 2 的 `AssetId<T>`（index＋generation）拿掉；store 內部的索引不對外。
- 其餘照 Bevy：`Assets<T>`、`Handle<T>`。新的：`AssetRef<T>`（不持有的引用，5.17）、`AssetFrame`（這一幀的快照）、`AssetChanges`（這一幀的變更清單）。

### 5.2 id 與持久鍵

**`AssetId`** 是 catalog 裡的字串鍵，**它就是持久鍵**（決定 6）：作者在程式、內容檔、存檔、判定資料裡都只記它。

- 字元集與長度：ASCII，`[a-z0-9._-]`，1～128 字元（卡片照 L17 判；跨平台、可以放進檔名與 URL、可雜湊）。不合規是 catalog 的啟動錯誤。
- **改名、搬移、事先轉檔**：只改 catalog 項目的路徑或格式，id 不變，內容與存檔不受影響。
- **跨行程穩定**：同一份 catalog，id 在 Client、Match、工具裡都相同。store 內部用的索引依登記順序而異，不出現在任何對外的值裡（AS-6）。

### 5.3 catalog（asset 擁有的 Data Contract）

使用者 2026-11-21 要卡片「比對一下之前的設計」再提案（12.1 決定 12）。逐檔出處在 `catalog-prior-designs.md`。

**比對**：

| | master（GYO 舊） | Unity Entities〔源〕 | Bevy〔源〕 | Godot〔文〕 | 本頁 |
|---|---|---|---|---|---|
| 放在哪 | 一份或多份 `asset_catalog.json`，由 `content.json` 列出（`ContentManifest.hpp`） | build 產生的 catalog（`RuntimeContentCatalogData.cs:186-206`） | 每個檔旁的 `.meta`（`meta.rs`） | 資源本身或 `.uid` 旁檔 | 照 master：集中的 catalog＋`content.json` 列出 |
| 鍵 | 字串 id，執行期換成 FNV-1a 64 雜湊、**等值只看雜湊**（`AssetId.hpp`） | GUID | 沒有 id（路徑） | UID | 字串 id，等值看字串 |
| 一筆的內容 | `id`、`type`、`path`；多的欄位保留 | 物件→檔→archive、事先算好的依賴組 | loader、settings、processor 資訊 | — | `id`、`type`、`path`＋可選 `format`、`settings` |
| 路徑 | 內容根目錄下的相對路徑，規則 `IsRelativeContentPath`，與工具共用 | archive 內 | 原始檔旁 | `res://` | 照 master |
| 誰產生 | 手寫＋工具（audio_editor 登記）；build 時組裝與驗證（`build/content_contract.py`） | build | Editor／processor | Editor | 手寫、Script、工具；執行期讀，不需要 build |
| Version | 整數 `version: 1` | 隨 package | `meta_format_version` "1.0"（`meta.rs:31`） | — | 整數 `version: 1`，和 master 相容 |
| 驗證 | version、陣列、id／type／path 非空、全內容 id 唯一、路徑規則；C++ 與 Python 共用 valid／invalid 範例（`docs/architecture.md:301`） | build 時 | 解析時 | — | 照 master；資料規則與 Product 的啟動檢查分開（見 (3)） |

**(1) 格式**
- **採**：JSON，頂層 `{"version": 1, "assets": [{"id", "type", "path", "format"?, "settings"?}]}`。入口是內容根目錄的 `content.json`（`{"version": 1, "catalogs": [...]}`）。
  - master 現有的內容檔照樣有效。`format`、`settings` 是新加的可選欄位，master 本來就允許多的欄位。
  - `path` 是內容根目錄下的相對路徑，規則照 master 的 `IsRelativeContentPath`（相對、`/` 分隔、沒有空段或 `.`、`..`、沒有 `\`、`:`、控制字元，不能是 `content.json`），和寫 catalog 的工具共用一個實作。
  - id：字串，等值看字串（5.2）。字元集 `[a-z0-9._-]`，master 現有的 id（例：`object_fps_pvp.player.character`）都合。
  - `content.json` 的 `shader_bundles` 屬 render：master 是必填（`ContentManifest.cpp:76`），本批**放寬成可省**，有的話照收、不驗，SDL 那批定它的規則（含 master 的「資產路徑不可與 shader bundle 重疊」）。
  - 不認得的頂層鍵、項目裡多的欄位：保留、不解讀（新加可選欄位不升版，舊讀者才收得下新內容）。
  - **`format` 省略時**：取 `path` 的副檔名（去掉點、轉小寫）。`path` 沒有副檔名又沒寫 `format`，是資料規則的違反（修訂 3 檢查 S4）。master 的 catalog 34 筆都只有 `id`、`type`、`path`，全部有副檔名，照樣通過。
- **系譜**：master 的 `CatalogFormat.hpp`、`ContentManifest.hpp`、`docs/architecture.md:285-312`；藍圖 `:66`。
- **退掉的**：
  - Bevy、Godot 的每檔旁檔：id 散在每個檔旁，改名要連旁檔一起搬，唯一性要掃全部才查得到。
  - Unity 的 build 產物：使用者三件之 3 要執行期讀原始檔。
  - master 的「等值只看 64 位元雜湊」：兩個 id 撞雜湊時會被當成同一個，master 沒有處理；改成看字串，雜湊只在內部當索引。
  - 修訂 3 先前寫的 `content://` 來源名：master 是一個內容根目錄、沒有分享的掛載，照它比較簡單。
- **決勝理由**：沿用既有的契約，現有的內容與工具不必改；集中一份才查得到全內容 id 唯一；JSON 可以手寫（F9）。

**(2) 誰產生**（AGENTS §8：手寫、Script、工具都一樣有效）
- **採**：
  - asset 函式庫提供 catalog 的讀、寫、驗證（不認得 ECS），工具照它寫，執行期照它讀。
  - **工具的登記政策歸工具**：master audio_editor 的「id 已屬別的型別就拒絕」「路徑已屬別的 id 就拒絕」（`RegistrationPolicy`）是工具的政策，不是契約的規則。
  - 產品把**內容根目錄**當 world_asset 的建立參數給（5.4；一個 Product 一個根目錄，照 master「Runtime content has one root」）。
  - 產品也可以在建立參數裡用程式加項目（F9 不想手寫時，自己掃資料夾），和 catalog 的項目一起驗（id 衝突是啟動錯誤）。asset 不掃資料夾。
- **系譜**：master 的 audio_editor 登記、`framework-rework` 的 `CatalogPlan`（framework 已退役，政策回到工具）。
- **退掉的**：build 時才組裝與驗證（master 的 `build/content_contract.py`）。GYO2 還沒有 build 的組裝管線；執行期讀的時候就驗證。日後組裝管線回來，照同一組 valid／invalid 範例對齊。
- **決勝理由**：使用者決定 6「id→路徑對照表的機制與格式歸資產管理，執行期可用」；工具自己持有內容知識，經契約交接（`engine-not-tool-duties`）。

**(3) Version 與 Validation Rule**
- **Version**：整數。新加可選欄位不升版；改了既有欄位的意思、或新增必填欄位才升版。讀到不認得的 version 是啟動錯誤。`content.json` 與 catalog 各有自己的 version。
- **規則分兩組**（修訂 3 檢查 S5）。每條標它對 master 是照舊、新增、放寬還是丟掉。
- **(A) 資料契約的規則**：只看資料本身，只連 asset 函式庫的工具與日後的組裝管線都查得了。違反是啟動錯誤（收進 `ProductStartupReport`，O-5）。
  1. `content.json`：JSON 解析得了；`version` 是整數、本版認得；`catalogs` 是陣列，每筆是合 `IsRelativeContentPath` 的相對路徑、彼此不重複、不是 `content.json`；列出的 catalog 檔讀得到（**照舊**，`ContentManifest.cpp:76-87`）。`shader_bundles` 可省（**放寬**，見上）。
  2. catalog：JSON 解析得了；`version` 是整數、本版認得；`assets` 是陣列，每筆是物件，`id`、`type`、`path` 是非空字串（**照舊**，`CatalogParser.cpp:19-37`）。
  3. id 在**全部 catalog** 裡唯一（**照舊**，`AssetCatalog.cpp:61-84`）。
  4. id 合字元集 `[a-z0-9._-]`、1～128 字元（**新增**；master 只要求非空）。
  5. `path` 合 `IsRelativeContentPath`、不是 `content.json`（**照舊**）。
  6. `path` 沒有副檔名時要寫 `format`（**新增**）。副檔名＝`path` 最後一段裡最後一個點之後的部分，轉小寫；目錄名裡的點不算（修訂 3 修正版檢查 S-8）。
  7. `format` 有寫時是 `[a-z0-9]+`；`settings` 有寫時是 JSON 物件（**新增**）。
- **(B) Product 的啟動檢查**：看這個 Product 登記了哪些 loader，不是資料本身的規則，不放進範例集。
  1. 同一個（`type`、`format`）兩個不同的 loader 是啟動錯誤（**新增**，藍圖 `:66`「副檔名衝突應為錯誤」）。
  2. loader 登記時，對 catalog 裡它的（`type`、`format`）的每個項目解讀 `settings`；解讀不了的列在登記回傳的錯誤裡（**新增**）。catalog 在登記之前就讀好（world_asset 的 `Configure`），所以每個 loader 只驗一次、和 World 的順序無關。`settings` 只在這裡驗，載入時不再驗。
  3. （`type`、`format`）沒有 loader 也沒有預先登記：不在啟動時報（loader 隨時可以登記），請求時是 id 級的 E2 `NoLoader`（5.8）。
  4. 同一個 `type` 名的 loader 產出的 C++ 型別必須相同（**新增**；修訂 3 修正版檢查 S-3）。
- **驗證時點**：(A) 在 world_asset 的 `Configure` 讀 catalog 時做，錯誤在它第一個跑到的 `Finish` 回報（Kernel 有啟動錯誤的 World 不跑 `Finish`，就由下一個；O-2 有任何錯誤就不建任何 World，所以不會漏）。(B) 在 loader 登記當下做，錯誤回給登記的一方（5.4）。沒有凍結點（決定 13）。
- **刻意不查的**：
  - **沒有 loader 的型別不是錯誤**（**照舊**，master「Custom types remain supported」）。例：Match 沒有 render，catalog 裡的貼圖照樣合法。
  - **檔案存不存在**：載入時才是 E2（BigMap 有數百個 region，啟動時逐一查檔太慢；master 在 build 的組裝時查）。
  - **兩個 id 指向同一個檔**：合法（**照舊**）。依賴經路徑反查時遇到它，那筆引用是 E2 `AmbiguousReference`（5.17）。
  - 不認得的頂層鍵與多的欄位：保留、不解讀（**照舊**於項目；頂層鍵是**新寫明**）。
- **丟掉的**：「資產路徑不可與 shader bundle 重疊」移到 SDL 那批（屬 render 的 `shader_bundles`）。
- **範例集**：照 master 的做法（`docs/architecture.md:301`），(A) 組的每條規則各準備合法與不合法的範例，當契約的一部分。C++ 的驗證與日後的工具、組裝管線都對同一組（AGENTS §8 的 Validation Rule）。
- **`content.json` 的 owner**：asset 擁有它的格式與 Version；`shader_bundles` 的內容歸 render。render 要改它時，由 asset 升 `content.json` 的版（第 6 節第 6 項）。
- **系譜**：master 的 `CatalogParser.cpp:19-37`、`AssetCatalog.cpp:61-84`、`ContentManifest.cpp:76-87`、`docs/architecture.md:301`。
- **決勝理由**：F5（UI 文件以 id＋型別參照，`UiDocumentBridge.cpp:130-170` 的四種診斷正好是 (A) 規則）、F6（audio_editor 要能照同一組規則寫）、F9（手寫的 catalog 要在啟動時就知道錯在哪）；新增的五條（(A)4、6、7，(B)1、2、4）都為了 loader 與 id 字串，放寬的兩條（`shader_bundles` 可省、`shader_bundles` 的項目規則不驗）為了支援還沒回來的 render。現有的 master 內容照樣通過是費用上的好處，不是決勝理由（修訂 3 修正版檢查 S-9、N-3）。

**交使用者的**：無。三件都沿用 master 的既有契約（不是使用者之前的決定），和別家的分歧（集中 vs 旁檔）由使用者決定 6 的「對照表」已經定了。

### 5.4 資產管理在 World 外（決定 7）

**`AssetStore`**：每個 Product 一份。world_asset 在 `Configure`（Product 層一次，O-2）建立它，存在 world_asset 這個登記單位的實例裡，活到 Product 結束。登記單位的實例本來就活到 Product 結束、可以持有自己的資料（`RegistrationUnit.hpp` 註解）。

**設定與 loader 怎麼到 store**（使用者 2026-11-22 決定 13；修訂 3 修正版的凍結機制撤回）：
- **設定在建立時帶**：內容根目錄、原始位元組雜湊的開關、在飛上限、loader 上限的覆寫，是 world_asset 的建立參數 `AssetOptions`，照 Bevy 的 `AssetPlugin { file_path, ... }`（`bevy_asset/src/lib.rs:237-248`）。不經任何槽轉交，沒有「每個 World 再給一次」。
- **catalog 在 world_asset 的 `Configure` 讀**：讀內容根目錄的 `content.json` 與 catalog，驗 (A) 組的規則（5.3）。錯誤先記著，在 world_asset 第一個跑到的 `Finish` 回報（收進 `ProductStartupReport`，O-5）。catalog 是內容，讀進來之後 Product 存活期間不變；槽表（5.5）這時配好。
- **loader 隨時可以登記**：store 的 loader 表用一把鎖保護（照 Bevy `server/mod.rs:73` 的 `RwLock`）。登記在啟動時發生、讀表的是 store 的工作執行緒與維護，都不在 World 執行緒的幀裡，所以不違反「World 內不上鎖」（AS-11 管的是 World 端）。
  - **預先登記**：擁有型別的 Module 可以先宣告「（`type`、`format`）的 loader 會來」，照 Bevy 的 `preregister_asset_loader`（`lib.rs:608`、`server/loaders.rs:144`、`:276-286`）。之後到的請求先排隊等，loader 到了才載。
  - 沒有 loader、也沒有預先登記：那筆是 id 級的 E2 `NoLoader`（5.8）；之後登記了 loader，再請求就重新載入（`Failed` 再請求的規則）。依賴遇到這種情況是略過（5.17）。
  - 登記當下的檢查（5.3 (B)）：同一個（`type`、`format`）已有別的 loader、同一個 `type` 名對到別的 C++ 型別，登記回錯誤；同時對 catalog 裡它的（`type`、`format`）的項目解讀 `settings`，解讀不了的列在回傳的錯誤裡。登記的一方把錯誤當自己的啟動錯誤回報。
- **Module 怎麼拿到 Product 層的 store 來登記 loader，不是 asset 該解的**：這是編排層缺一個概念——Product 層、Module 互相拿得到的 Resource（像 Bevy 的 App 層 Resource）。asset 只是第一個需要它的。交給主會話與編排層（12.3 第 13 題）。asset 依賴它，在它定下之前，loader 的登記入口只定成 store 上的函式。
- 沒有內容根目錄：catalog 是空的，每個請求都是 `UnknownId`。不是啟動錯誤（只直接呼叫 loader 的工具就是這樣）。
- **擁有型別的 Module 的拆法**：照 asset／world_asset 的先例拆兩個單位。loader 的函式庫只依賴 asset（F4 的工具只連它）；World 端的單位依賴 world_asset，在登記檔寫明。

**World 端**：world_asset 在每個 World 的 `Build` 登記這些 Resource 的型別，值在它的準備函式裡放進去（`Build` 時還沒有 World，5.6）：
- `AssetRequests`：請求入口、狀態與就緒查詢；
- `AssetFrame`：這個 World 這一幀的快照（5.6）；`Assets<T>` 是它的型別化讀法（`frame.Get<T>(handle)`），不是另一個 Resource，所以不必每個型別各登記一次；
- `AssetChanges`：這一幀的變更清單（5.7）。

System 經這些 Resource 存取，不持指標；元件裡只放 `Handle<T>`、`AssetRef<T>` 或 `AssetId`（AS-1）。store 端記每個 World 的資料時，用準備函式發的不透明記號，不用 World 名（5.6）。

**換 World、多 World 不是 asset 的事**：
- Handle 的引用計數跨 World 有效；舊 World 銷毀時它的 Handle 才放掉。
- `BuildNewFirst`：新 World 的準備函式、`beforeSwap` 在舊 World 還在時跑，請求同樣的 id 一定命中，**保證不重讀**。
- `DropOldFirst`：舊 World 先銷毀（`Product.cpp:646`），之後才準備新 World（`:710`）；中間 store 的維護可能已經把歸零的項目移除，所以是**盡力**，不保證。作者要保證，就把 Handle 放進自己標 `CarryOver` 的 Resource（O-10），計數就不會歸零。
- 回主選單另建 Product（O-3）：舊 Product 的 store 隨它結束；新 Product 重新載入（RC-AS3）。

### 5.5 請求、Handle 的生命週期

**請求**：`AssetRequests::Load<T>(AssetId)`，在 System 或編排層給的準備函式裡同步執行，**當場**回 `Handle<T>`。
- **請求級的失敗**：id 不在 catalog（`UnknownId`）、`T` 和項目的 `type` 不合（`TypeMismatch`）。catalog 讀進來之後不變，所以 World 端查它不需要同步。只影響**這一次請求**：回一個帶失敗的 Handle，不碰那個 id 的槽。失敗記在這個 World 本地，由下一個套用 System 直接放進 `AssetChanges`（時序固定，不等維護；修訂 3 修正版檢查 N-1）。
- **id 級**：查得到、型別合、有 loader 的，照下面的協定持有那個 id 的槽。

**去重**：鍵＝id。settings 跟著 catalog 項目，同一個 id 只有一種 settings（修了 Bevy issue 11111 的根源）。

**每個 id 一個固定的槽**（修訂 3 修正版檢查 B-1）：
- world_asset 的 `Configure` 讀 catalog 時，store 為每個 id 配一個槽，以 catalog 內部索引編號，**Product 存活期間不釋放**。所以 World 端「找到某個 id 的槽」只是查不變的表，不建立、不回收任何東西。
- 槽裡有：原子的引用計數、原子的**世代**、「死了」標記、「放掉待處理」旗標。

**持有與放掉的協定**：
- **請求**：讀槽。活著就原子加一（命中）。死了（或從沒載過）就以比較並交換把「死了」換成 1、世代加一；**只有換成功的那一方**排一筆工作（帶新世代）。兩個 World 同時對死的 id 請求，只會排一筆。
- **複製 Handle**：Handle 存在時計數 > 0，槽不會死，所以直接原子加一。
- **放掉**：原子減一；減到 0 時，以比較並交換把「放掉待處理」從否換成是，換成功才把這個槽推進放掉佇列。佇列的節點就在槽裡（侵入式），所以 World 端不配置；同一個槽不會在佇列裡兩次。
- **維護處理放掉**：清掉「放掉待處理」，再看計數：仍是 0 就換成「死了」、從下一份快照移除值、記 `Removed`；> 0 就是**救回**（維護之前又有人持有），同一份、不重讀。0→1→0 只會在佇列裡留一筆，判定只看當下的計數，不會把「已經死了」誤當救回。
- **世代**：工作、Handle、快照裡的項目都帶世代。工作的結果回來時，世代對不上（那段生命已經死了又復活）就算取消、不發佈。快照以（槽、世代）查值，新 Handle 不會對到舊值。

**系譜**：Bevy 的救回規則（`assets.rs:578-580`）與防重複的計數（`server/info.rs:773` 的 `handle_drops_to_skip`）；Bevy 整段持鎖（`assets.rs:571-575`），Unity 的請求與釋放佇列每次推送都拿 SpinLock（`RuntimeContentManagerUtility.cs:147`、`:168`）。**World 端無鎖是新推導**，沒有一家可以直接對照（修訂 3 修正版檢查 S-9）。退掉 Unity 式「計數全在單一消費者」：每次複製 Handle 都要推一筆訊息，World 端的成本與配置比原子加一大。

**Handle 比 store 活得久**：Handle 共有 store 核心的參照計數。store 已銷毀時 Handle **安全失效**：查詢回 `NotRequested`、取值回空。

**取消**：最後一個 Handle 在工作還沒完成時消失、那段生命死了，結果回來時世代對不上，計入「取消」。

**值**：不可變、共享唯讀（AS-10）。`AssetFrame` 的型別化讀法 `Get<T>(handle)`（即 `Assets<T>`）回 `const T*`（沒載好回空），**只在這次 System 執行期間有效**。

**原始位元組雜湊**（決定 10）：store 讀檔時對讀到的位元組算 SHA-256（`engine/base` 的 `Sha256`），每個 id 一個，只算自己的檔、不含依賴。
- 預設開；Product 級設定可關，關時查詢回「沒算」。`AssetRequests::SourceDigest(handle)` 讀，值在快照裡。
- 比不比、不同時怎麼辦，歸作者或網路 Module；asset 不拿它做任何判斷。
- **限界**：它只代表檔的位元組。兩端同一個檔、catalog 項目的 `format` 或 `settings` 不同時，雜湊相同、值不同；要比內容身分，作者也要比 catalog 項目（第 14 節）。
- 介面不綁「讀檔時算」：日後 catalog 項目可以帶工具事先算好的雜湊（Unity content catalog、Bevy `processor/mod.rs:86-91`），那是 catalog 的 Version 升級，查詢 API 不變。
- 全部內容 36.7 MB 的雜湊成本在實作時量測，記進 dev_log。

### 5.6 執行緒、維護、快照；給交換點的需求

**store 的工作**（World 執行緒之外）：讀檔＋解碼。在交換點或 runner 給的執行器上跑（X-1）。同時在飛的工作數有上限（Product 級設定，R9）。

**store 的維護**（單一處理者）：
- **喚醒**：請求、放掉、完成任一個佇列推進東西時，以比較並交換把「已排」旗標從否換成是，換成功才排一次維護。維護結束前先清旗標、**再查一次佇列**，有東西就自己再排一次，避免最後一筆卡住（修訂 3 修正版檢查 S-5）。
- **排在哪**：維護不能排在大檔的解碼後面。X-1 要求執行器給維護專屬或優先的執行機會，且從 World 執行緒「排一次」不上鎖。
- **做什麼**：收請求，排工作（受在飛上限）；收完成的結果（世代對上才採用）；處理放掉佇列（5.5）；更新依賴圖與「連同依賴」的狀態（5.17）；做出下一份快照，把變更併進每個 World 的包（下面）。
- 維護在 World 執行緒之外，內部可以上鎖。
- **沒有「每次發佈上限」**：修訂 3 曾寫，但維護由事件驅動、沒有時間基準，World 一幀看到的是多次發佈的合計，上限限制不了任何東西。World 端只換一個指標，不需要它。記憶體由在飛上限與 loader 的位元組上限管（F8）。

**快照**：一份不可變的物件，帶原子的參照計數（不用 `std::atomic<shared_ptr>`：主流實作內部有鎖）。內容：
- 每個（槽、世代）的值與狀態、原始位元組雜湊；
- 依賴邊、每個項目的「連同依賴」狀態、整串 id、失敗與略過的清單（5.17；修訂 3 修正版檢查 S-7 (d)）；
- catalog 的指標（讀進來之後不變）。loader 表不在快照裡：只有工作執行緒與維護在鎖下讀它。

快照是所有 World 共用的；**變更清單不放在快照裡**（修訂 3 修正版檢查 B-2）。

**交給 World：每個 World 一個交接槽，放一個「包」**＝{最新快照、這個 World 自上次取走以來的變更清單}。
- **維護發佈時**：先把那個 World 的槽原子交換成空，拿回沒被取走的舊包；把舊包的變更併進新的清單，放掉舊包的快照；再把新包換進槽。
- **World 取包時**（套用點）：把槽原子交換成空。拿到包就換進 `AssetFrame` 與 `AssetChanges`；拿到空（維護正在換，或沒有新東西）就這一幀不換。
- 這樣維護在兩個套用點之間發佈幾次，變更都會累積到下一個包，一筆不漏。
- **上限與溢出**：每個包的變更清單有上限。超過時清單換成一筆「溢出」，讀者要用 `State`／`Ready` 重掃。World 長時間不套用就會溢出。

**World 看到結果**：world_asset 的套用 System（預設 `Main`/`First`）取包。
- **每個 World 每幀只讀一次**：各 `Assets<T>`、就緒查詢、`SourceDigest` 都從同一份 `AssetFrame` 導出，就緒查詢說好了，`Get` 一定拿得到值。
- **一幀之內看到同一份**：只在套用點與準備函式（下面）換，同一幀的重跑步與新步看到同一份（回滾的重跑在 `PreUpdate`，第 4 批 5.2）。
- **只有 store 是寫者**：World 不寫值。

**World 在 store 的登記**（修訂 3 修正版檢查 S-6）：
- `Build` 時還沒有 World，Kernel 只登記 Resource 的型別，值要到準備函式才放得進去（`Registry.hpp:176-180`、`Storage.hpp:375-378`）。所以 world_asset 在**準備函式**裡做：發一個不透明記號、在 store 開這個 World 的交接槽、取一份當下的快照放進 `AssetFrame`。這三件在 store 的鎖下做，不在任何一幀之內。
- world_asset 的準備函式排在依賴它的單位之前（O-7 照登記檔的順序），所以後面的準備函式、`beforeSwap`、遊戲初始化裡命中的值當場讀得到（G-3）。
- 登記失敗的 World 不跑準備函式，store 裡什麼都沒留。
- `AssetFrame` 解構時推一筆「註銷」給維護，維護關掉那個 World 的槽、清掉它的「請求過」集合。
- store 端用記號、不用 World 名（`BuildNewFirst` 時新舊同名 World 並存）。
- **限界**：World 手上的舊快照會讓已移除的值不釋放，直到它下一次套用（第 14 節）。

**World 端的操作不上鎖**（AS-11）。例外的完整清單：
1. 槽的引用計數、世代、「死了」標記、「放掉待處理」旗標（原子、比較並交換，5.5）；
2. 請求佇列與放掉佇列（無鎖、多生產者單消費者；放掉佇列的節點在槽裡）；
3. 每個 World 的交接槽（原子交換）；
4. 快照與包的參照計數（原子）；
5. Handle 對 store 核心的參照計數（原子）；
6. 維護的「已排」旗標（原子）。

**不在幀內、用 store 的鎖的**：準備函式裡的 World 登記與第一份快照、`AssetFrame` 解構的註銷推送（推送本身無鎖）。catalog 與槽表在 world_asset 的 `Configure` 讀好，在任何 World 跑起來之前就已發佈，World 端讀它們不需要同步。loader 表帶鎖，但只有工作執行緒與維護讀它，World 端不碰。

**給交換點、threads 那批的需求**：

| # | 需求 | 為什麼 |
|---|---|---|
| X-1 | 給 store 一個執行器：工作與維護在 World 執行緒之外跑，不碰 World（M11）；從 World 執行緒提交不上鎖；維護有專屬或優先的執行機會 | F1、F2：23.7 MB 的 fbx 解碼不能卡住 World，也不能擋住維護 |
| X-2 | 交換點不需要認得 asset 的型別或語意；World 靠自己的套用點看到結果，不經交換點搬值（RC-AS5、RC-O8） | 5.6 的交接槽 |
| X-3 | 若交換點宣告定了「每幀的邊界」，套用 System 掛在它之後 | 同一幀看到同一份 |
| X-4 | 執行緒的數量與歸屬由交換點或 runner 定 | O-3 |
| X-5 | 測試能控制哪幾筆工作何時完成、維護何時跑（含「當場全部跑完」）；結果不依賴執行緒時序 | 等價測試（第 10 節）；只跑一次的 runner |
| X-6 | Product 結束時，未完成的工作能停下或等完 | 件數（5.8） |

### 5.7 狀態、就緒查詢、變更清單

**項目的狀態**：`NotRequested`、`Queued`、`Loading`、`Loaded`、`Failed`（帶 `AssetLoadFailure`）。`AssetRequests::State(handle)` 回這個 Handle 看到的（請求級的失敗也在這裡）；`State(id)` 回這個 World 目前快照裡那個 id 現世代的狀態。

**就緒查詢**（決定 9）：`AssetRequests::Ready(一組 Handle)` 回：
- `AllFinished`：每一筆都結束了（`Loaded` 或失敗）；
- `Complete`：每一筆都 `Loaded`，而且「連同依賴」也 `Complete`（5.17）；
- `Loaded` 筆數、`Failed` 筆數與清單（含依賴的失敗）、略過的依賴、整串的 id 清單。

`AssetRequests::Unfinished()` 回這個 World 請求過、還沒結束的筆數，**含本幀剛送出、store 還沒收到的**（修訂 3 修正版檢查 N-1）。

- 讀取畫面、淡出、等哪些才開始 tick，是遊戲設計，**歸作者**（決定 9）。單筆到了就看得到，所以「先看到地面才看到樹」要由作者用就緒查詢擋住。

**變更清單 `AssetChanges`**：一個 Resource，每個套用點換成那個包的清單，只在這一幀有效。
- **單位**（修訂 3 修正版檢查 N-2）：每個（World、id）一筆。這個 World 開始持有某個 id、那個 id 第一次在它的視圖裡看得到（`Added`）或失敗（`Failed`，帶原因）時一筆；這個 World 持有過的 id 被移除時一筆 `Removed`；它請求過的 id 的遞迴依賴失敗時一筆（帶父項，5.17）；請求級的失敗一筆。同一個 id 已經看得到之後再 `Load`，不再發 `Added`。
- **「請求過」**：這個 World 經 `AssetRequests` 請求過的 id。用 `CarryOver` 帶進來的 Handle 不算這個 World 請求過，收不到它們的告知，也不進 `Unfinished()`；要知道它們的狀態用 `State`／`Ready`。
- 新 World 從自己在 store 登記那一刻開始；命中已載好的 id，也在它的下一個套用點收到一筆 `Added`。
- 有不分型別的讀法（給日誌、工具）與 `ForType<T>()` 的讀法；每筆帶一個遞增的序號。
- **讀者的規則**（修訂 3 修正版檢查 S-4）：清單只在這一幀有效，所以讀它的 System 掛在每幀跑一次的 schedule（預設 `Main`），並標「重跑時不跑」。只在 `FixedUpdate` 跑、帶執行條件、或由 `RunSchedule` 驅動的 System，可能漏讀或重讀；它們要知道狀態，用 `State`／`Ready`，或比對序號。
- **為什麼不用 Kernel 的保留式 `Message`**：第 4 批的 `Summary` 格式 3 對每個訊息緩衝雜湊下一個序號、筆數與游標的讀取位置（第 4 批分支 `engine/ecs/kernel/src/World.cpp:1188`、`:1191`、`:1201`），和 payload 的雜湊函式無關。載入的告知依完成時序，放進 `Message` 會讓 `Summary` 依時序變；放在雜湊什麼都不寫的 Resource 裡就不會（合第 4 批 3.3：Resource 只雜湊有沒有與值，`World.cpp:1181-1185`）。代價是告知對「不是每幀剛好讀一次」的讀者有損（上一點）。另一個選項是請第 4 批或 Kernel 給訊息型別「不進摘要」的選項，告知就能沿用游標、不漏不重（第 8 節「告知」一列、第 15 節）。

### 5.8 錯誤（照已合併的〈Error levels〉）

| 情況 | 級別 | 怎麼出現 |
|---|---|---|
| **請求級**：id 不在 catalog（`UnknownId`）、型別不合（`TypeMismatch`） | **E2** | 那個 Handle 帶失敗；下一個套用點出現在 `AssetChanges`。不改那個 id 的項目 |
| **id 級**：沒有 loader 也沒有預先登記（`NoLoader`；之後登記了，再請求就重新載入）、檔案不在、讀不到、超過上限（`TooLarge`）、loader 解碼失敗（`settings` 在登記時已驗，5.3） | **E2** | 項目的狀態是 `Failed`，帶 `AssetLoadFailure{code, message, detail}`（`Engine::Base::Error<Code>` 的形狀，沒有鏈）；loader 自己的碼名與細節放進 `detail` |
| loader 丟例外（含 `std::bad_alloc`；`catch (...)`） | **E2**（`LoaderThrew`） | 同上。工作在 World 外，例外不能沿 World 的 Run 傳出；`detail` 寫 `what()` |
| **依賴的失敗**：經路徑引用但 catalog 沒有（`UnlistedReference`）或有多個 id（`AmbiguousReference`）、路徑不合法（`BadReference`）、引用的 id 不在 catalog（`UnknownId`）或型別不合（`TypeMismatch`）、循環（`CyclicReference`）、依賴本身失敗（5.17 的先後） | **E2** | 父項自己的狀態不變；「連同依賴」的狀態是 `Failed`，帶 `DependencyFailure{parent, reference, code}`——`reference` 是 id 或檔裡寫的路徑，所以沒有 id 的失敗也記得下來 |
| catalog 不合資料規則 | 啟動錯誤 | world_asset 第一個跑到的 `Finish` 回報，收進 `ProductStartupReport`（O-5） |
| loader 登記的檢查不過（5.3 (B)） | 登記回錯誤 | 登記的一方回報（啟動時就是它的啟動錯誤） |
| 同一個 id 在 `Loaded` 之後又送來一筆結果（AS-10 (b) 不該發生） | 內部計數 | 不改值；計數讓測試讀 |
| 呼叫寫錯：在編排層給的時機之外、沒有 World 時呼叫 `Load` | **E0** | `GYO_ASSERT` |

**件數**（AS-8；第 10 節照它突き合わせ；修訂 3 檢查 S10）：
- **工作**（讀檔＋解碼的次數）：開始的工作 ＝ 發佈成 `Loaded` ＋ id 級失敗 ＋ 取消 ＋ 丟棄（Product 結束時還沒完成）。在沒有工作在飛、發佈上限也沒有留存時成立。
- **請求**：請求 ＝ 新工作 ＋ 命中（含救回）＋ 請求級失敗。
- **丟棄的出口**：Product 結束時 World 已經先銷毀（`Product.hpp:194` 的註解），登記單位也沒有結束的回呼（`RegistrationUnit.hpp:36-46`），所以 E3 記錄沒有可寫的 World。本批把丟棄件數留在 store 的原子計數，測試在 store 銷毀前讀；給 E3 的出口需要編排層的結束回呼，報給主會話（第 15 節），不在本批。

asset 不寫日誌、不自己重試、不讓 World 中斷。要不要因為載入失敗結束遊戲，由作者的 System 決定（回 `Err` 就是作者把 E2 升成 E1）。

### 5.9 決定性、內容身分（和第 4 批的權威、回滾對齊）

1. **身分的決定性**：id 是 catalog 的字串，和請求順序、完成時序、World 的歷史都無關。新開局與讀檔進同一關，同一個內容的 id 相同。
2. **內容的決定性**：asset 核心不改位元組。值＝loader(位元組, catalog 的 settings)。不讀時鐘。全部結束之後，store 的內容和完成順序無關。**哪一筆在哪一幀讓 World 看到，依完成時序**（決定 9：到了就可見）。判定要用的內容在生成世界時載完才開始 tick，所以決定性不受影響；對局中影響判定的串流屬日後的地圖 Module（RC-AS1）。loader 的輸出跨平台是否逐位元相同，是那個 loader 的事（`determinism.md` DET-2）。
3. **id 可以跨行程、進存檔、進權威的資料**；store 內部的索引不行（AS-6）。
4. **內容身分**（舊 DI 3）：asset 提供每個 id 的原始位元組雜湊（5.5），比不比歸作者或網路 Module（決定 10）。對原始位元組比，可以繞開第三方解碼（ufbx）的跨平台差異（`determinism.md` §4.9、DET-2）。它只代表檔的位元組，catalog 項目的 `format`、`settings` 不同時要另比（5.5 的限界）。
5. **回滾**：asset 的視圖不是判定資料，回滾 Module 不存它、不倒回它。一幀之內重跑的步看到同一份。但第 100 個 tick 第一次跑時某內容還沒載好、重跑時已載好，依「載好沒」分支的判定就會不同；照第 4 批第 21 節，**到達時刻要影響判定就走 `rollback.Input<T>`**，否則作者要讓判定用的內容在進判定之前載完。

#### 5.9.1 asset 放進 World 的型別的雜湊函式（第 4 批 3.3）

| 型別 | 雜湊什麼 | 理由 |
|---|---|---|
| `AssetRequests` | **什麼都不寫** | 視圖，可以重建；內容依完成時序變，寫進去 `Summary` 就依時序變 |
| `AssetFrame`、`AssetChanges` | **什麼都不寫** | 依完成時序變；告知不用 Kernel 的 `Message`，所以沒有序號與游標進摘要（5.7） |
| `AssetRef<T>` | id 的字串 | 同 `Handle<T>` |
| `Handle<T>` | **id 的字串** | 跨行程穩定；不含型別序號（`Handle<T>` 的靜態型別已決定是哪一欄；修訂 2 檢查 S10）；不含 store 內部索引 |
| `AssetId` | id 的字串 | 同上；接 Kernel 的同一個客製點（一個概念一個實作） |

F9（2D 單機）：不呼叫 `Summary` 就不碰這些。

### 5.10 接得上「工具事先轉檔、執行期讀引擎格式」（AGENTS §8）

三件之 3：起步讀原始檔，介面要接得上。id 當持久鍵之後，這件事變簡單：

1. **事先轉檔＝改 catalog 項目**：工具輸出引擎格式的檔，把那個 id 的 `path` 與 `format` 改成新的。請求的 API、Handle、id、內容與存檔都不變。修訂 2 的「疊掛載點」不再需要。
2. **選 loader 的鍵是（`type`、`format`）**：引擎格式＝另一個登記給同一個型別的 loader。
3. **loader 是純函式**，在不認得 ECS 的 `asset` 函式庫上就能呼叫；工具要驗證自己的輸出，就呼叫同一個 loader。

**不在本批**：轉檔工具、引擎格式、用來源雜湊判斷要不要重轉（Bevy `processor/`）。這些屬工具回來時的資料契約。catalog 本身的 Version 屬 asset（5.3）。

### 5.11 第三方：stb_image（三件之 1；隨 SDL 那批的 image Module，12.2 第 5 題）

| 項目 | 內容 |
|---|---|
| 組件 | `stb_image.h`（nothings/stb，單一標頭）。實作時釘 commit，不釘 tag（同 yyjson） |
| 授權 | 雙授權，使用者擇一：MIT 或 Public Domain（Unlicense），寫在檔尾。下載時以檔案原文確認。打包時附授權告示 |
| 放哪裡 | `third_party/stb_image/CMakeLists.txt`（FetchContent，同 `third_party/yyjson`），產生 `gyo_stb_image`：只含一個 `.c`，在裡面定義 `STB_IMAGE_IMPLEMENTATION` |
| 只出現在哪裡 | `engine/modules/image/src/adapters/stb_image/`。`gyo_stb_image` 只被那一個 target **私有** link；標頭不出現在任何 `include/` 下 |
| 編譯設定 | `STBI_ONLY_PNG`；`STBI_NO_STDIO`（讀檔歸 asset）；`STBI_NO_HDR`、`STBI_NO_LINEAR`；`STBI_NO_FAILURE_STRINGS`；`STBI_MAX_DIMENSIONS` 設上限。adapter **不用** `stbi_failure_reason`、`stbi_set_flip_vertically_on_load`（全域或 thread-local 狀態），維持 AS-3 |
| 建置旗標 | 照 DET-2：第三方預設在決定性邊界外。本頁不宣稱它在邊界內；image 批的 golden 是回歸測試 |
| 為什麼不走 SDL | Match、測試、工具載入不該需要 SDL（F2、F4）；一個外部組件只有一個 adapter（AGENTS §6） |
| 已知的風險 | stb 的 README 說明 stb_image 不以惡意輸入為設計目標〔知〕。今天的內容都是自己的，所以接受。不可信來源的圖成為 Feature 時換 adapter（RC-IM1） |
| 下載 | 要使用者同意，在 image 那批（第 13 節） |

### 5.12 對 io 的要求（交給 threads、io 重審那批）

- 從 World 以外的執行緒讀整個檔，帶位元組上限（今天是 `engine/io/helpers/ReadAllBytes`）。**`IFileSystem`／`Vfs` 在多執行緒同時讀時的契約要確認。**
- 內容根目錄由 world_asset 掛在 io 的一個 scheme 下（`MountPoint.hpp:15`）；catalog 的路徑是根目錄下的相對路徑（5.3）。
- 疊掛載點（`MountPoint.hpp:16`「overlay 用」、`Vfs.hpp:86-88` 找不到就往下一個）io 已經有；本批不靠它（5.10）。
- 檔案監看（`IFileWatcher`）留給熱重載，本批不用。

### 5.13 給生成世界（重做）那一批的需求

| # | 需求 | 為什麼 |
|---|---|---|
| G-1 | 生成世界分批跑時，每一批之間 world_asset 的套用 System 會跑（由誰掛、掛在哪由那批定） | 作者等「全部完成」才離開；視圖不換就永遠等不到 |
| G-2 | 結束條件由作者的 System 給，可以是就緒查詢「全部好了」，也可以是作者自己的條件 | `-build-world-phase-is-mechanism` |
| G-3 | `beforeSwap` 同步連跑（`Product.cpp:740-749`）、期間沒有套用點：裡面命中已載好的值，經 world_asset 在準備函式之前取的快照當場讀得到；還沒載好的，在替換之後的套用點才看到。寫明，不要求改 | 5.4、5.6 |

### 5.14 和讀檔、存檔版本 Module 的關係

**讀檔**＝替換 World（讀檔宣告修訂 11，核准版）。asset 的部分：
1. **先驗證**：作者在現在的 World 請求存檔裡記的 id，等到全部結束。有 `Failed`（含 `UnknownId`：內容改版後某個 id 不在了）就不發替換，回 `Result` 給玩家（O-8）。
2. **再替換**：新 World 請求同樣的 id，命中（5.4）。
3. **存檔裡記 id**（AS-6）。

**存檔不是 asset**：存檔的格式是作者的；讀存檔檔案用 io 或作者自己的方式。**存檔版本 Module** 比對存檔的版本標記，和 asset 的內容無關；內容改版造成 id 消失由第 1 步發現。catalog 的 Version 是 catalog 格式的版本，不是內容的版本。

### 5.15 藍圖的資產不變量 A1～A6

| # | 藍圖的內容 | 本頁 | 理由 |
|---|---|---|---|
| A1 | 每型別一個 store＋每型別一份事件 | **留**，形狀改：一個 `AssetStore` 內每型別一張表，World 端一份 `AssetFrame`、以型別讀（`Assets<T>`）；告知併成一份 `AssetChanges`（5.7） | — |
| A2 | id 只在行程內有效，path（或 Uuid）才是契約 | **改**：契約的鍵是 catalog 的 id 字串（決定 6）；store 內部索引只在行程內 | — |
| A3 | 非同步完成序列化到單一套用點 | **留**：store 的維護是單一處理者；World 看到結果只在自己的套用點 | 順序：完成了就發佈（決定 9） |
| A4 | 釋放兩段式（`Unused`→`Removed`） | **本批一段** | Bevy 兩段的實際用途是渲染：只給渲染用的 asset 用 `get_mut_untracked` 與 `take_gpu_data` 把資料**就地取走**、main World 只留中繼資料（`bevy_render/src/render_asset.rs:351-353`、`:96`），其餘整份 `clone`（`:364`）；GPU 那份等最後一個 Handle 消失（`Unused`）才移除（`:332-333`）。AS-10 禁止就地取走，所以渲染那批要丟 CPU 副本時，路是「第二段釋放：移除值、Handle 還在」→ RC-AS7。AS-10 的共享唯讀也省掉 Bevy 的 `clone` |
| A5 | 依賴圖由 core 擁有、loader 只宣告 | **留**（決定 11）：依賴圖在 store，loader 經 `LoadContext` 宣告引用（5.17） | 修訂 2 的「延後」翻掉 |
| A6 | 格式解析器與執行期解讀分層 | **留**：asset（library）與 world_asset 的拆法；loader 歸擁有型別的 Module | — |
| （:66）| catalog 邏輯 id、StrictJson、Result、ready 前全載；副檔名衝突應為錯誤 | catalog 邏輯 id：**採**（5.3）；StrictJson 歸 JSON Module；Result：留；ready 前全載：歸作者（12.2 第 3 題）；（`type`、`format`）衝突是啟動錯誤 | — |

### 5.16 loader 的介面與登記

**介面**（在 `asset` 函式庫；名字實作時照 N15 定）：
- **一個 loader 宣告**：產出的型別 `T` 與型別名（catalog 的 `type`）；它認的一個或多個 `format`（文字 loader 認 `txt`、`json`，所以 master 的 20 筆 text＋`.json` 載得了；修訂 3 修正版檢查 S-8）；`Settings` 與從 catalog 的 JSON 解讀它的函式；預設的位元組上限；錯誤碼。
- **載入函式**：`Load(bytes, settings, LoadContext&) -> Result<T, LoaderFailure<Code>>`。
  - `LoadContext` 有：這筆的 id 與路徑（給錯誤訊息）、上限、宣告引用的入口（5.17）。
  - 沒有 World、沒有時鐘、沒有全域可變狀態；同樣的位元組與 settings 得到同樣的值與同樣的引用清單（AS-3）。
  - 產出的 `T` 要合 AS-10 (c)。

**系譜**：master 的 `IAssetLoader::Load(ConstSpan<byte>, LoadContext)`，去掉 `AnyAsset` 的型別抹除；引用的入口照 Bevy `LoadContext::load`（`loader.rs:735`），但回的是不持有的 `AssetRef`（5.17）。Bevy 的 `Reader`＋async 不採：串流沒有 Feature（決定 8）。

**整檔進記憶體**：解碼前整檔讀進來，帶上限；不做串流的 loader（最大的內容 fbx 23.7 MB、region ≤ 16 MiB 在上限內）。

**登記**：隨時可以登記到 store 的 loader 表（帶鎖），也可以先預先登記（5.4）。同一個 loader（同一個 C++ 型別）重複登記是冪等的；同一個（`type`、`format`）兩個不同的 loader，登記回錯誤。Module 怎麼拿到 store，待編排層的 Product 層 Resource（12.3 第 13 題）。

**上限**（R9）：每個 loader 給預設的位元組上限；產品可以覆寫（Product 級設定，5.4），放寬或收緊都可以（`engine-not-rigid`）；超過是 E2 `TooLarge`。文字 loader 的預設上限 16 MiB（照 F8 的 region 上限）。

### 5.17 依賴（Engine 推出；決定 11）

**loader 宣告引用**：`LoadContext` 有兩個入口，回**不持有的** `AssetRef<U>`（只記 id，不加計數）：
- `Reference<U>(AssetId)`：引用另一個 id；
- `ReferencePath<U>(相對路徑)`：檔案格式裡記的是路徑（例：fbx 裡的貼圖檔名）時用。
  - 相對於父項的檔解析，再正規化成內容根目錄下的路徑：絕對路徑、反斜線、超出根目錄的 `..`、不合 `IsRelativeContentPath` 的，是 E2 `BadReference`；比對分大小寫。
  - 經 catalog 的反查（路徑 → id）換成 id。catalog 裡沒有這個路徑是 E2 `UnlistedReference`；有兩個以上的 id 指向它是 E2 `AmbiguousReference`。
  - **限界**：經路徑的引用只對「被引用的檔沒有事先轉檔」成立。貼圖轉檔、改了它的 `path` 之後，還記著原檔名的父項會變成 `UnlistedReference`。轉檔工具要連父項一起轉，否則觸發 RC-AS6。

**一條引用怎麼判**（先後固定；修訂 3 修正版檢查 S-7 (b)）：
1. 經路徑的，先解成 id（`BadReference`、`UnlistedReference`、`AmbiguousReference`）；
2. id 在 catalog 裡（否則 `UnknownId`）；
3. `U` 和項目的 `type` 相合（否則 `TypeMismatch`）；
4. 這個 Product 有那個（`type`、`format`）的 loader；有預先登記就照一般請求等它；沒有 loader 也沒有預先登記就**略過**（不載、不算失敗）。看的是（`type`、`format`），不只 `type`。略過和循環一樣是單調的：之後才登記 loader，已略過的邊也不重新持有，直到父項那段世代被移除。

1～3 的失敗是**邊級的依賴失敗**，帶 `DependencyFailure{parent, reference, code}`。

在 store 裡，通過 1～4 的引用立刻持有、需要時排工作；直接呼叫 loader（F4）時只回引用清單。

**所有權**：
- **持有依賴的是 store 的依賴邊，不是值**。父項的值裡放的是 `AssetRef`，不加計數；值可以不可變（AS-10），store 也不必認得 `T` 的內部。
- 父項（那段世代）還活著時，它的每條依賴邊對依賴加一個持有；父項被移除時，它的邊一起放掉，依賴照一般的引用計數走（5.5）。
- 作者要自己持有某個依賴，就用 `AssetRequests::Load(ref)` 拿一個 Handle（一般的請求）。`AssetRef` 本身不持有。
- 去重照 id：兩個模型引用同一張貼圖，只讀一次。

**經 `AssetRef` 讀值的契約**（修訂 3 修正版檢查 S-7 (c)）：`Assets<U>::Get(ref)` 在同一份 `AssetFrame` 裡，父項的「連同依賴」是 `Complete` 時，非循環、非略過的依賴**一定**讀得到。其他時候（還沒完成、循環的邊、略過的、父項已被移除）可能回空。

**連同依賴的狀態**：
- `Incomplete`：自己或某個遞迴的依賴還沒結束；
- `Complete`：自己與遞迴的全部依賴都 `Loaded` 或略過；
- `Failed`：帶 `DependencyFailure` 的清單。

父項自己的狀態不因依賴失敗而變（照 Bevy：自己 `Loaded`、連同依賴 `Failed`，`server/info.rs:661-716`）。作者可以決定沒有某張貼圖也照用。

**略過的例子**：無畫面的 Match 沒有貼圖 loader，模型照樣 `Complete`，也不會去載用不到的貼圖。

**循環引用**（和完成順序無關）：
- 維護每加入一個項目的依賴邊，就對已知的邊重算強連通分量。
- 同一個強連通分量裡的邊（含自我引用 A→A）**全部**標 `CyclicReference`，都不持有；之前持有的一起放掉。分量裡每個成員的「連同依賴」狀態都是 `Failed`。
- **標記是單調的**（修訂 3 修正版檢查 S-7 (a)）：一條邊被標成循環之後，直到它的父項那段世代被移除，都不會因為重算而重新持有。這樣不會出現「放掉→不成環→重新持有→重讀→又成環」的反覆。
- 最後的依賴圖只依內容而定，所以結果和 A、B 誰先解完無關（AS-5）。
- Bevy 沒有處理循環（第 11 節），這是 GYO 的差異。

**告知**（修訂 3 修正版檢查 S-7 (f)）：依賴的失敗告知給所有**遞迴祖先**的請求者。C→A→X、X 失敗時，只請求 C 的 World 也收到一筆（帶父項 A）。不另加「連同依賴載完」的告知；就緒查詢就是它的介面（決定 9）。

**雜湊**：原始位元組雜湊每個 id 各自一個（5.5）。要比整串，作者用就緒查詢給的 id 清單逐一比。

**件數**（修訂 3 修正版檢查 S-7 (e)）：引用 ＝ 新工作 ＋ 命中 ＋ 略過 ＋ 邊級失敗 ＋ 循環（循環的邊在標記前已觸發的工作，照一般工作計入 5.8 的工作等式）。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature 需要**：F1～F6、F8、F9；12.1 的決定 1～12（決定 4 作廢）。
2. **現在的問題**：`gyo2` 沒有 asset；舊設計（target `assets` DI 1～3）建立在已拿掉的 (c) 區、Loading 的 Ingress 與「資產放在使用它的 World」上；master 的 `AssetManager` 是總管物件、在主執行緒輪詢、貼圖綁 SDL3_image；master 的 catalog 是 owner 未定的共用契約。
3. **變化的邊界**：
   - 新 Subsystem：`engine/modules/asset`、`engine/modules/world_asset`。
   - **新 Data Contract：catalog**（格式、Version、Validation Rule 歸 asset）。
   - **推翻 11-11 方向 2**（資產放在使用它的 World）：資產管理改在 World 外，Product 一份（決定 7）。保留三類拆法與「System 不持指標」。
   - target 文件：舊 `target/assets.zh-Hant.md` 的 DI 1～3 與 RC，由新頁取代。
   - `engine/modules/image`、`third_party/stb_image` 在 SDL 那批，不在本批。
4. **影響**：
   - 編排層：登記兩個單位；設定是 world_asset 的建立參數；Module 拿到 Product 層 store 的路要編排層的「Product 層 Resource」（12.3 第 13 題，交主會話與編排層定）。
   - Kernel：告知不用 `Message`（第 4 批的 `Summary` 雜湊訊息序號與游標，5.7）；這對其他依完成時序發訊息的 Module 也成立，報給主會話（第 15 節）。
   - 交換點、threads：5.6 的需求（比修訂 2 少：不經交換點搬值）。生成世界：5.13。
   - 第 4 批：`Summary` 的雜湊（5.9.1）。
   - 日誌與 `world_logging`：讀 `AssetChanges` 的失敗；丟棄件數的 E3 出口要編排層的結束回呼（第 15 節）。
   - 之後的 render、text、ui、audio、model Module：登記 loader；它們的值要合 AS-10。
   - 產品：載入改經 id 與 catalog。工具：照 catalog 的 Data Contract 寫（audio_editor、ui_editor），或直接呼叫 loader（model_viewer）。
5. **Dependency Direction**：asset → base、private io、private json；world_asset → asset、base、kernel、orchestration（日後可能加 threads）；日後的 image → asset。擁有型別的 Module 拆兩個單位：loader 函式庫 → asset；World 端單位 → world_asset（登記檔寫明，5.4）。全部往下，沒有反向；core 不依賴它們；編排層不認得它們（RC-O4、RC-O8）。
6. **Ownership**：
   - catalog 的格式與驗證、id、loader 介面、`AssetStore` → asset。
   - Product 的 store 實例、`AssetOptions`、World 端的 `AssetFrame`、請求入口與 `AssetChanges` → world_asset。
   - 格式 → 擁有型別的 Module；第三方解碼 → 該 Module 的 adapter。
   - 「載完才開始」與轉場 → 作者（asset 給就緒查詢）；內容身分的比對 → 作者或網路 Module（asset 給原始位元組雜湊）。
   - 依賴圖 → asset 的 store；引用了什麼 → loader 宣告。
   - GPU 資源 → render。catalog 的內容（哪些 id、指到哪）→ 產品與它的工具。
   - `content.json` 的格式與 Version → asset；其中 `shader_bundles` 的內容 → render（5.3）。
7. **為什麼不能更小**：
   - 沒有 asset，每個 Module 與產品各自讀檔、管生命週期與錯誤。
   - 拿 master 的 `AssetManager` 改，是在為舊形狀找位置。
   - 留在 World 裡（修訂 2）要 CarryOver、時間窗口、搬回、對帳一整套，換 World 與多 World 都要 asset 自己處理；使用者判定「這應該和 asset 無關」。
   - 路徑當持久鍵（修訂 2），改名或轉檔時內容與存檔要跟著改；作者也得知道檔在哪。

## 7. Design Invariants（新頁 `target/v2/asset.zh-Hant.md`，見出し `Design Invariants`）

**舊 → 新的系譜**（「緩」＝舊的保證、新的不保證；「緊」＝反過來）：

| 舊（11-13 核准） | 新 | 緩了什麼 | 緊了什麼、其他 |
|---|---|---|---|
| DI 1 沒有總管物件，三類 Resource **各在它們的 World** | **AS-1** | 「資產在使用它的 World」不再成立：值在 World 外的 store（決定 7） | System 不持指標照留；GPU 資源標明歸 render |
| DI 2 載入是外部服務；經 Ingress 進 (c) 暫存，依請求序號的**連續前綴**套用；套用順序與批次切分和完成時序無關 | **AS-4**、**AS-5** | **批次切分、發佈順序、訊息順序改依完成時序**（決定 9）；只保留「全部結束後內容與完成順序無關」 | id 改成 catalog 的字串，**和請求順序也無關**（比舊的緊）；「IO 不碰 World、單一處理點」保留；不再要 (c) 區與 Ingress |
| DI 3 內容身分由 asset 從位元組算；ready 前載完；守衛：未就緒即 ready 是 Programmer Error | **AS-6**、**AS-7** | 「ready 前載完」不再是 Engine 的規則，舊守衛消失（12.2 第 3 題、決定 9）；內容身分不再由 asset 當規則，改成提供雜湊（決定 10） | id 可進存檔與權威資料，寫明 |
| 舊 RC：Running 期間串流進 (c) → 重審 | **RC-AS1** | — | 改成日後地圖 Module 的事（決定 8） |
| — | **AS-2**、**AS-3**、**AS-8**、**AS-9**（新） | — | 三件之 1、3；11-11 方向 3；異常分級；AGENTS §8 |
| — | **AS-10**、**AS-11**（新） | — | **緊**：值不能就地改；loader 的 `T` 要 const 並行安全、解構不綁執行緒；World 端操作不上鎖。決定 2、5 |
| — | **AS-12**（新） | — | catalog 是 Data Contract。決定 6 |
| — | **AS-13**（新） | — | 依賴由 Engine 推出、循環是錯誤。決定 11 |

**新的 DI**（守衛欄的「延後」照 `v8-arch-gyo2-guards-after-mechanism-stable`：綁實作方式的掃描守衛，等 scenario test 通過再立；檢查公開契約的負向編譯不在此限）：

- **AS-1 資產管理在 World 外；System 不持指標。**
  - 每個 Product 一個 `AssetStore`，由 world_asset 持有；值只在 store 裡。
  - World 端只有 `AssetRequests`、`AssetFrame`、`AssetChanges` 三種 Resource（請求入口、這一幀的快照、變更清單；`Assets<T>` 是 `AssetFrame` 的型別化讀法），元件裡只有 `Handle<T>`、`AssetRef<T>` 或 `AssetId`。
  - GPU 資源是 render 的 Resource；渲染 World 讀別的 World 的資料只經 Extract（K.3-6）。asset 的 store 不是哪個 World 的資料，渲染 World 也和其他 World 一樣經自己的 `AssetFrame` 讀它，不算跨 World 讀（修訂 3 修正版檢查 N-6）。
  - 守衛：行為測試（兩個 World 請求同一個 id，store 只載一次、兩邊讀到相等的值；換 World 後同一 id 不重讀）。掃描延後。
- **AS-2 核心不認得格式。** asset 與 world_asset 不解析任何內容格式（catalog 的 JSON 是 asset 自己的契約，不算；文字 loader 只驗證 UTF-8）。格式由擁有型別的 Module 登記 loader；第三方只在該 Module 的 `src/adapters/<組件>/`，私有 link。守衛：行為測試（沒有 loader 的型別在 catalog 裡合法、請求時是請求級 E2 `NoLoader`；同一（`type`、`format`）登記兩個不同的 loader 是啟動錯誤）。
- **AS-3 loader 是不依賴 ECS、可直接呼叫的純函式。** 輸入位元組、settings、id 與上限；輸出值或失敗；不碰 World、不讀時鐘、不持全域可變狀態；只連 `GYO::Asset` 就能呼叫。守衛：只連 `GYO::Asset` 的測試 target；同輸入兩次結果相等；**會宣告引用的測試 loader 直接呼叫兩次，引用清單相同**。CMake 掃描延後。
- **AS-4 IO 不碰 World；store 是唯一的寫者；World 只在套用點換視圖。** 讀檔與解碼在 World 執行緒之外；值只由 store 的維護放進快照；World 的視圖只在套用 System 與 world_asset 的準備函式（World 第一次登記時）換，一幀之內看到同一份。守衛：行為測試（X-5 的控制模式：一幀中途完成的工作，同一幀內看不到、下一個套用點看到；維護在兩個套用點之間跑兩次，`AssetChanges` 一筆不漏）。
- **AS-5 身分在請求時給，而且就是 id。** Handle 在請求時同步給出，指向 id；全部結束之後，任何完成順序、任何發佈切分都得到相同的 store 內容。守衛：等價測試（第 10 節）。
- **AS-6 持久鍵是 id 字串；store 內部索引只在行程內。** id 可進內容、存檔、權威資料；`Handle<T>`、`AssetId` 的雜湊只用 id 字串。守衛：行為測試（兩份項目順序不同的 catalog，同一組 Handle 的 `Summary` 相同；新開局與讀檔進同一關的 `Summary` 相同）。
- **AS-7 「何時載完」與內容身分不是 asset 的規則；asset 提供能力。** asset 給狀態、`AssetChanges`、就緒查詢（連同依賴），保證同位元組同值（AS-3），並給每個 id 的原始位元組雜湊（預設開、可關）。要不要等、轉場、比不比雜湊與不同時怎麼辦，屬作者或網路 Module；到達時刻要影響判定，走 `rollback.Input<T>`。守衛：行為測試（雜湊開：同位元組同雜湊、不同位元組不同雜湊；關：查詢回「沒算」）；「asset 不拿雜湊做判斷」靠本頁與審查。
- **AS-8 錯誤只回報。** 請求級與 id 級的失敗是 E2，在狀態與 `AssetChanges` 裡；依賴的失敗帶 `DependencyFailure`；catalog 的資料規則是啟動錯誤，loader 登記的檢查回給登記的一方；不寫日誌、不自己重試、不讓 World 中斷；工作與請求的件數等式成立（5.8）。守衛：行為測試（各碼、各項）。
- **AS-9 介面與格式、位置分開。** 作者只用 id；catalog 把 id 對到（`type`、`path`、`format`、`settings`）；選 loader 用（`type`、`format`）；加一種格式（含引擎格式）＝登記一個 loader＋改 catalog 項目，不改請求的 API 與核心。守衛：行為測試（同一 id 從原始格式改指到引擎格式，作者的程式不變、值相同）。
- **AS-10 載好的值不可變。**（使用者 (a')；跨執行緒的部分照決定 5 現在寫）拆成三條：
  - **(a) 物件不改**：放進 store 的值在發佈之後不再改變；World 端只拿得到 `const` 存取。守衛：負向編譯測試（經 `Assets<T>` 拿不到可寫的存取；先例 `tests/common/kernel/CMakeLists.txt:176` 的 `gyo_kernel_compile_fail_samples`），第 6 步立。
  - **(b) 綁定只有 store 換，本批不換**：一個 id 在一段生命裡只發佈一次；熱重載回來時是「換一個值」，由 store 做（RC-AS4）。守衛：行為測試（同一 id 在 `Loaded` 之後再送一筆結果，值不變、計入異常的件數）。
  - **(c) loader 的 `T` 要 const 並行安全、解構不綁執行緒**：沒有 `mutable` 成員、沒有惰性快取、沒有非原子的內部計數；解構可以在任何執行緒發生（不綁執行緒、不持執行緒局部狀態）。作者要改就複製一份。字型的 glyph 快取這類可變狀態放在擁有型別的 Module 自己的 Resource。守衛：行為測試（最後一個持有者在別的執行緒放掉，解構正常；測試型別在解構子記執行緒）；其餘**只靠審查**——C++ 擋不住 `mutable`（Unity 是把 blob 限成 unmanaged 型別才做得到）。
- **AS-11 World 端的操作不上鎖。** 請求、放掉、讀只用原子操作與無鎖佇列；鎖只在 store 的維護裡，不在 World 執行緒上。例外清單在 5.6。守衛：本頁與審查；並行案例只在 TSan 建置有檢出力（CI 沒有 TSan：`git grep -i sanitize -- .github` 0 件），寫明不當判定。
- **AS-13 依賴由 Engine 推出。** loader 經 `LoadContext` 宣告引用（id 或 catalog 裡的路徑），拿到不持有的 `AssetRef`；持有依賴的是 store 的依賴邊；就緒查詢的「好了」包含遞迴的全部依賴；父項自己的狀態不因依賴失敗而變；依賴的型別在這個 Product 沒有 loader 就略過；同一個強連通分量裡的邊全部是 E2 `CyclicReference`、都不持有，結果和完成順序無關。守衛：行為測試（鏈、菱形去重、循環兩種完成順序與自我引用、略過、經路徑引用的三種失敗、父項放掉後依賴跟著歸零；第 10 節）。
- **AS-12 catalog 是 asset 擁有的 Data Contract。** 格式、Version、Validation Rule 在新頁；工具、Script、手寫產生的 catalog 一樣有效；執行期讀，不需要 build 的產物；asset 函式庫提供讀、寫、驗證，工具照它寫。守衛：契約的範例集（5.3）：(A) 組每條規則各一個不合的範例是啟動錯誤；刻意不查的各一個範例合法；master 的 `asset_catalog.json` 與 `content.json` 原樣通過；寫出再讀回相等。(B) 組的啟動檢查各一個行為測試。

### Replacement Criteria（寫在積上去之前）

符合任一條時，停下來交使用者決定「修還是重建」：

- **RC-AS1**（舊 RC 改寫）：日後的地圖 Module 用 asset 的 id 與上限表達不了它的串流（例：region 要在對局中進碰撞，而 `rollback.Input<T>` 表達不了），而必須在 asset 核心加特例。
- **RC-AS2**：每加一種格式都要改 asset 核心，或某個 loader 必須碰 World 才寫得出來。
- **RC-AS3**：Product 一份的代價超出預算：回主選單另建 Product 的重載時間，或多個 Product 要共用同一份內容（今天沒有這種 Feature）。這時才看行程級。
- **RC-AS4**：熱重載或子 asset 成為點名的 Feature（這時才做 `Modified`、`#label`）；或某個 loader 的依賴必須在解碼中途**拿到依賴的值**才解得下去（今天的引用只宣告、不等值）。
- **RC-AS5**：World 看到結果時，交換點或編排層需要知道 asset 的型別或語意（和 RC-O8 同類）。
- **RC-AS6**：事先轉檔的路回來時，請求的 API、Handle 或 id 必須改（違反 AS-9）；或經路徑的依賴引用因為被引用的檔轉檔而解不到，工具又無法連父項一起轉（5.17 的限界）。
- **RC-AS7**：某個 Feature 需要在最後一個 Handle 消失後保留值（寬限），或渲染那批要在上傳 GPU 之後丟掉 CPU 的副本（第二段釋放；5.15 A4）。
- **RC-AS8**：執行期讀 catalog 的成本（啟動時間、記憶體）超出預算，或 catalog 必須由 build 產生才能保證一致（像 Unity）。
- **RC-AS9**：World 端為了請求或讀取必須上鎖（AS-11 保不住），或某個點名的型別做不到 AS-10 (c)。
- **RC-IM1**（image Module 的 DI 節）：不可信來源的圖成為 Feature，或 stb_image 的缺陷影響我們的內容，就換 adapter。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

每一列都先問：「這是不是在為舊形狀找位置？」（`target/README.md`〈每批事前宣告的檢查項〉）

| 選擇 | 採用 | 系譜 | 退掉的 | 決勝理由 | 需要它的 Feature |
|---|---|---|---|---|---|
| 資產管理放哪 | World 外，Product 一份，由 world_asset 持有 | Unity Entities（行程一份）、Bevy（App 一份）；決定 7 | 每 World 一份＋CarryOver（修訂 2）；行程級靜態物件；改編排層加 Product 層共用值 | 資產的生命週期和 World 無關（使用者）；Product 一份讓 Sim 與 Present 共用同一份值（F1）、換 World 不重讀（F3）；GYO 沒有行程級靜態先例，回主選單另建 Product 時重載可以接受（RC-AS3）。設定在建立時帶、loader 隨時登記（照 Bevy）；Module 拿到 store 的路是編排層的概念（12.3 第 13 題） | F1（Sim＋Present）、F3 |
| loader 表 | 帶鎖、隨時可以登記；可以預先登記，請求先等 | Bevy `server/mod.rs:73`（`RwLock`）、`preregister_asset_loader`（`lib.rs:608`、`server/loaders.rs:144`、`:276-286`）；Godot `add_resource_format_loader` | 凍結點、凍結後只准相同、每個 World 再交一次（修訂 3 修正版，使用者 2026-11-22 撤回） | 沒有一家有凍結點；讀表的是工作執行緒，不在 World 的幀裡，一把鎖就夠（決定 13） | F1、F4 |
| 持久鍵 | catalog 的 id 字串 | 藍圖 :66、master catalog、Unity GUID；決定 6 | 路徑（修訂 2、Bevy）；GUID（不可讀、要工具產生） | 作者只用 id（使用者）；改名與轉檔不影響內容與存檔；字串讓手寫 catalog（F9）可行 | F3、F5、F6 |
| catalog | asset 擁有的 Data Contract：master 的格式（version 1）＋可選 `format`、`settings`；`content.json` 列出；執行期讀 | master `CatalogFormat.hpp`、`ContentManifest.hpp`、`docs/architecture.md:285-312`；藍圖 :66 | Bevy／Godot 的旁檔；Unity 的 build 產物；master 的「等值只看雜湊」；`content://` 來源名（修訂 3 初稿） | F5、F6 要以 id＋型別參照、照同一組規則寫；F9 要能手寫並在啟動時知道錯在哪；集中才查得到全內容唯一；執行期讀（三件之 3）。沿用 master 讓現有內容照樣有效，只是費用上的好處（比對見 5.3） | F5、F6、F9 |
| 單位的拆法 | `asset`（library，含 store）＋`world_asset`（Module） | `logging`／`world_logging` | 一個 Module；store 放 world_asset | 工具不建 World 也要 loader、catalog 與 store | F4、F6 |
| World 怎麼看到結果 | store 發佈不可變快照；World 在套用點換視圖 | 新推導；Unity 的每幀處理命令、Bevy 的 `PreUpdate` 套用 | 經交換點把值搬進 World（修訂 2）；World 每次讀都看最新的 | 一幀看到同一份；World 不寫值；交換點不必搬值 | F1（Client 的回滾重跑） |
| 值的可變性 | 不可變、共享唯讀；三條分開寫 | Unity blob（唯讀）；使用者 (a') | Bevy、Godot 的可變值 | 不可變是日後最難改的約束；跨執行緒共用的前提（決定 5） | F1、F2 |
| World 端不上鎖 | 原子＋無鎖佇列＋交接槽，鎖只在 store 的維護與準備函式 | **新推導**：Unity 的佇列每次推送拿 SpinLock（`RuntimeContentManagerUtility.cs:147`、`:168`），Bevy 用 `RwLock`（`server/mod.rs:65-72`），沒有一家可直接對照 | World 端直接拿 store 的鎖；照搬 Unity 的佇列 | 「World 內不上鎖」（D6b、M11）；多執行緒是已定前提（決定 5） | F1、F2 |
| 結果的順序 | 完成了就發佈＋一組 id 的就緒查詢；轉場歸作者 | Bevy（照完成順序） | 舊 DI 2 的連續前綴；整組才可見；作者可設組、Engine 保證同幀 | 使用者決定 9：轉場是遊戲設計；判定用的內容在開始 tick 前載完 | F1、F8 |
| 內容身分 | asset 提供原始位元組雜湊（預設開、可關）；比對歸作者或網路 Module | Unity content catalog、Bevy processor 的來源雜湊；第 4 批 3.2、4.4 | 只由作者對值算；asset 算且當規則（舊 DI 3） | 使用者決定 10：作者只用 id、asset 本來就讀位元組；繞開第三方解碼的跨平台差異；提供能力、不替外面決定 | F2、F9 |
| 依賴 | Engine 推出：loader 宣告引用，store 擁有依賴圖 | Bevy `LoadContext::load`（`loader.rs:735`）與失敗傳播（`server/info.rs:661-716`）；Unreal、Addressables、Godot 都由 Engine 推 | 作者自己列；本批只留介面 | 使用者決定 11：作者只關心使用；依賴是機制的核心能力 | F1（模型引用貼圖） |
| 「載完才開始」 | 作者的事 | `-build-world-phase-is-mechanism` | 舊 DI 3、runtime-composition DI 3 | Loading 不是機制 | F1、F8、F9 |
| 套用的位置 | 預設 `Main`/`First`；生成世界期間照 5.13 | Bevy 的 `PreUpdate`，往前移一格 | `PreUpdate` | 同一幀的重跑步與新步看到同一份 | F1（Client） |
| 釋放 | 一段；維護前再請求就救回 | Bevy 的引用計數與救回規則（`assets.rs:578-580`） | 兩段（藍圖 A4、Bevy `Unused`）；master 的 N 幀寬限 | 兩段的實際用途是渲染丟 CPU 副本，本批沒有渲染；換 World 靠救回規則，不需要寬限 → RC-AS7 | F3 |
| 去重的鍵 | id | Bevy 的（路徑、型別）＋修它的缺陷 | Bevy 原樣（settings 被忽略） | settings 隨 catalog 項目，同一 id 只有一種 | F1 |
| 錯誤 | 請求級與 id 級分開的 E2；依賴失敗帶父項與引用；不記日誌 | 〈Error levels〉；Bevy `UntypedAssetLoadFailedEvent`、`info.rs:661-716` | asset 自己寫 `Warning`（master）；請求級和 id 級不分（修訂 3 初稿） | Engine 的 Module 不自己寫日誌；兩個 World 用不同 `T` 請求同一 id 時，請求級失敗不該改那個 id 的狀態 | F1、F5 |
| 告知 | `AssetChanges`：經交接槽累積、每個套用點換一次的 Resource，只含本 World 請求過的 | 新推導 | Kernel 的保留式 `Message`（Bevy `AssetEvent`）；請第 4 批或 Kernel 給訊息型別「不進摘要」的選項 | `Message` 會讓 `Summary` 依完成時序變；「不進摘要」的選項能保留游標、不漏不重，但要改 Kernel 與第 4 批的格式，報給主會話判斷（第 15 節）。本批先用 Resource，代價是不是每幀剛好讀一次的讀者會漏或重（5.7 的讀者規則） | F1、F3 |
| 引用計數 | 每個 id 一個固定槽（讀 catalog 時配、不釋放）＋世代＋「放掉待處理」旗標，比較並交換 | Bevy 的救回規則（`assets.rs:578-580`）與防重複（`server/info.rs:773`） | Bevy 整段持鎖（`:571-575`）；Unity 式計數全在單一消費者（每次複製 Handle 都要推訊息）；修訂 3 的「建新項目」（兩個 World 會各建一個） | AS-11；只有換成功的一方排工作；世代讓舊結果算取消、新 Handle 不對到舊值 | F1、F3 |
| loader | 純函式、整檔、直接可呼叫；引用經 `LoadContext` 宣告、回不持有的 `AssetRef` | master `IAssetLoader` 去掉型別抹除；Bevy `LoadContext::load`（`loader.rs:735`） | Bevy 的 `Reader`＋async；Bevy 的回強 Handle（值持有，循環無解）；master `AnyAsset` | F4 要直接呼叫；串流沒有 Feature（決定 8）；依賴由 Engine 推出（決定 11），持有放在 store 的邊才能和不可變的值相容 | F1、F4、F6 |
| 上限 | 在飛：Product 級設定；loader 上限：產品可覆寫；包的變更清單：有上限、溢出時告知 | AGENTS §2；master `maxLoadsPerFrame` | 沒有上限；每次發佈的上限（修訂 3，事件驅動下沒有時間基準） | F8 的 242 MB 要靠在飛上限管記憶體；`engine-not-rigid` | F8 |
| 大地圖 | 不內建串流；region 級 id＋上限 | Bevy（決定 8） | 現在設計串流（Unreal World Partition、Unity scene section） | 使用者決定 | F8 |
| 圖片 | stb_image；獨立的 image Module，SDL 那批 | 三件之 1；Bevy `bevy_image`（獨立 crate） | SDL3_image；放 render；本批做 | 使用者決定；原始碼事實＋thin start | F1、F4、F5 |

**沒有用來決勝的**：既有的 Test、守衛、golden 不必改。`gyo2` 上沒有 asset 的測試。

## 9. thin start 與實作的步驟（動工前核准、第 5 批合併、使用者說開始之後）

**實作**：R1～R10，也就是點名的 Feature 直近要用的部分。

**只定形狀、不實作的**：引擎格式的 loader（留在 AS-9 的（`type`、`format`），只用測試用的 loader 驗證）。

**沒有根據的不留形狀**：熱重載、子 asset、行程級 store、作者在執行期加值。

**依賴的限制**：本批只有文字 loader，它不引用別的資產；依賴追蹤只能用測試用的 loader 驗。第一個真正的使用者是模型（引用貼圖），在產品搬回或 SDL 那批。

| 步 | 內容 | 模型／檔位 | 上段或下段 |
|---|---|---|---|
| 第 1 步 | `asset` 函式庫：`AssetId`、catalog 的讀寫與驗證、loader 介面、整檔讀取、文字 loader；只連它的測試 | Opus／high | 上段（公開介面、Data Contract） |
| 第 2 步 | `AssetStore`：請求（請求級與 id 級分開）、引用計數與「死了」標記、救回、取消、維護的驅動、快照與交接槽、每 World 的變更清單、依賴圖（邊持有、強連通分量、略過）、原始位元組雜湊；X-5 的控制模式 | Opus／high（無鎖的協定合變速箱 xhigh 的定義，但使用者定 xhigh 只用在調查；照 high，審查時若認為要，先問使用者） | 上段（執行緒） |
| 第 3 步 | `world_asset`：store 的持有與 `AssetOptions`、`AssetRequests`、`AssetFrame` 與型別化讀法、`AssetChanges`、套用 System 與準備函式、5.9.1 的雜湊函式 | Opus／high | 上段 |
| 第 4 步 | 接交換點與生成世界（照它們的宣告）、真執行緒模式、上限；等價測試 | Opus／high | 上段（執行緒、決定性） |
| 第 5 步 | 換 World 與讀檔案例、丟棄件數（store 的計數）、錯誤各碼 | Opus／high | 上段 |
| 第 6 步 | scenario test、負向編譯、突變、新頁、dev_log、審查；AS-10 (c) 與「判定用的內容在進判定前載完」登記到 `docs/cross-component-rules.md`（AGENTS §1；owner asset） | 實作 Opus／high；審查 Fable／high | 上段 |

## 10. 測試、判定（先宣告；結果出來之後不放寬）

測試用的型別：文字（真的 loader）；測試目錄裡三個測試 loader：一個慢、可以指定失敗；一個模擬引擎格式（`format` 為 `gyotxt`）；一個**會宣告引用**的（檔案內容列出它引用的 id 或路徑）。測試用的 catalog 由測試程式產生。

**scenario test（第 6 步，沿遊戲流程）**：
1. 生成世界分兩批：第一批請求 6 個 id，其中一個檔案不在、一個超過上限、一個 id 不在 catalog。
2. 跑到就緒查詢 `AllFinished`（`Complete` 不成立，因為有失敗）：3 筆 `Loaded`；2 筆 id 級 `Failed`（`NotFound`、`TooLarge`）與 1 筆請求級 `UnknownId`；`AssetChanges` 在那幾個套用點合計 3 筆 `Added`、3 筆失敗；就緒查詢的失敗清單是那 3 個。
3. **兩個 World 共用**：第二個 World 請求同樣 3 個 id：讀檔計數不增加，兩邊讀到相等的值。
4. **讀檔，先驗證**：在現在的 World 請求存檔要的 2 個 id，等到結束，發 `ReplaceRequest`（`BuildNewFirst`）。新 World 在 `beforeSwap` 再請求：命中，讀檔計數不增加。沒再要的 1 個在下一次維護移除；用 store 的測試讀法看它的槽變成「死了」（不靠 `AssetChanges`：請求過它的舊 World 已銷毀，新 World 沒請求過它）。
5. **`DropOldFirst`**：(a) 作者把 Handle 放在自己標 `CarryOver` 的 Resource：讀檔計數不增加（保證）。(b) 不放：控制模式下讓維護在銷毀舊 World 與準備新 World 之間跑一次，結果是重讀，讀檔計數加 1；不讓維護跑，就救回、不增加（盡力，5.4）。
6. **先驗證失敗**：存檔記一個不在 catalog 的 id。驗證得到請求級失敗（`UnknownId`），不發替換，舊 World 照跑。
7. **新 World 的起點**：替換來的新 World 在 `beforeSwap` 命中已載好的 id，當場讀得到值；它的下一個套用點在 `AssetChanges` 收到那筆 `Added`；它沒請求過的 id 的變化不在它的 `AssetChanges` 裡。
8. 件數的突き合わせ：工作與請求兩條等式（5.8），各項數字寫在測試裡。

**等價測試（AS-5）**：同一組 20 個 id，用 X-5 的控制模式比較：(1) 一次全部完成；(2) 反序分 4 次完成；(3) 交錯分 7 次完成，每次維護上限 3 筆；(4) 載入中途丟掉 3 個 Handle、同時再請求 3 個新的；(5) 其中含循環 A→B→A 與自我引用 C→C，A、B 的完成順序兩種都跑。判定：每一組和**它自己的基準**比——同樣的請求與放掉、一次全部完成的那一次——全部結束後 store 內容與依賴狀態相同；(1)～(3) 彼此也相同；件數等式成立。

**依賴（AS-13）**：用會宣告引用的測試 loader：
- 鏈 A→B→C：只請求 A，就緒查詢在 C 載好之前回「沒好」，之後回「好了」，id 清單是 A、B、C；
- 菱形 A→B、A→C、B→D、C→D：D 只讀一次；
- 循環 A→B→A，A、B 的完成順序兩種都跑：兩條邊都是 `CyclicReference`、都不持有，A、B 的「連同依賴」都是 `Failed`；作者只持有 B 時，A 在下一次維護被移除（兩種順序結果相同）；自我引用 C→C 同樣處理；
- 依賴失敗 A→X（X 不在）：A 自己 `Loaded`，就緒查詢回「失敗 1 筆：X，經由 A」；
- 經路徑引用：catalog 裡有的路徑解成 id；沒有的是 `UnlistedReference`；兩個 id 指向它是 `AmbiguousReference`；`../` 超出根目錄、絕對路徑、反斜線是 `BadReference`；
- 略過：依賴的型別在這個 Product 沒有 loader，那條依賴略過，父項 `Complete`，就緒查詢列在「略過」裡，讀檔計數不增加；
- `AssetRef` 不持有：父項移除後，作者手上只有 `AssetRef` 的依賴也跟著移除；作者另外 `Load(ref)` 拿了 Handle 的就留著；
- 放掉 A：下一次維護 A 移除，B、C 跟著歸零移除。

**原始位元組雜湊（AS-7）**：開：同位元組同雜湊、改一個位元組雜湊不同、等於 `Sha256` 直接算的值；關：查詢回「沒算」。

**同一幀看到同一份（AS-4）**：一幀中途完成並維護的工作，同一幀之內的 System 看不到；下一個套用點看到。

**值不可變（AS-10）**：
- (a) 負向編譯：經 `AssetFrame` 的型別化讀法取可寫存取、經 `Get` 拿到非 const 指標，各一個樣本，編譯必須失敗。
- (b) 同一 id 在 `Loaded` 之後再送一筆結果：值不變。
- (c) 最後一個持有者在別的執行緒放掉：測試型別的解構子記下執行緒，解構正常、只發生一次。

**`Summary`（AS-6、5.9.1）**：兩份項目順序不同的 catalog（store 內部索引不同），同一組 Handle 的 `Summary` 相同；新開局與讀檔進同一關（後者命中、`AssetChanges` 的筆數不同），`Summary` 相同。

**catalog（AS-12）**：(A) 組每條規則各一個不合的範例 → 對應的啟動錯誤；刻意不查的各一個範例合法；master 的 `asset_catalog.json` 與 `content.json` 原樣通過；寫出再讀回相等。(B) 組：同一（`type`、`format`）兩個不同 loader、同一 `type` 兩個 C++ 型別、`settings` 解讀不了，各一個登記錯誤。loader 表：先預先登記、請求、再登記 loader → 請求先等、之後載好；沒有預先登記就請求 → `NoLoader`，登記之後再請求 → 載好；啟動之後（第一幀之後）再登記 loader 也可以。

**事先轉檔（AS-9）**：同一 id 從 `.txt` 改指到 `gyotxt` 格式的檔，作者的程式不變、值相同；同一（`type`、`format`）登記兩次 → 啟動錯誤。

**直接呼叫（AS-3、F4）**：只連 `GYO::Asset` 的測試 target 呼叫文字 loader；同輸入兩次結果相等；兩條執行緒各呼叫 1,000 次，結果全部等於單執行緒（0 件不一致）；會宣告引用的測試 loader 直接呼叫兩次，引用清單相同。

**引用計數（AS-11、5.5）**：控制模式下：
- 「最後一個 Handle 放掉」與「同一 id 再請求」在維護之前發生：救回、同一份、讀檔計數不增加；在維護之後發生：復活成新世代、重讀；
- 兩個 World 對同一個死的 id 同時請求：只排一筆工作；
- 同一個槽連續兩次歸零（0→1→0）才跑維護：放掉佇列裡只有一筆，判定正確；
- 舊世代的工作在復活之後才完成：算取消、不發佈，新 Handle 讀到的是新世代的值。

**交接槽（AS-4、5.6）**：控制模式下讓維護在兩個套用點之間發佈兩次，`AssetChanges` 一筆不漏；讓 World 不套用直到超過包的上限，`AssetChanges` 是一筆「溢出」，`State`／`Ready` 的結果正確。

**真執行緒模式（AS-4、AS-11）**：同一組 20 個 id、兩個 World 同時請求，跑 50 次。判定：每次的最終 store 內容都等於控制模式；每個 World 收到的 `Added`／`Failed` 合計等於控制模式；件數等式 50/50 成立。資料競爭的檢出只在 TSan 建置有效，不當判定。

**突變**：每條 DI 的守衛各至少一個專屬的預期失敗（AGENTS §14.2、§14.5）。AS-7、AS-11、AS-10 (c) 的「只靠審查」部分不立突變，寫明。清單在實作時列，受別的模型做敵對式檢查。

**判定**：上面全部通過；L1 三平台（linux-x64、windows-x64、macos-arm64）通過；突變全部 killed。

**量測**：載入時間、記憶體、catalog 的讀取時間只做探索量測，標「不是判定」；數字延後（12.2 第 6 題）。

## 11. 卡片照 L17 判的（不是使用者的話）

**照命名規範、別家原始碼、已定原則判的**：
- 兩個單位的拆法；`AssetStore` 在 asset 函式庫；名字 `AssetStore`、`AssetId`（5.1）；
- id 的字元集與長度（5.2）；catalog 的三件（格式、誰產生、Version 與 Validation Rule）照 master 沿用、加兩條、放寬一條（5.3）；
- store 放在 world_asset 登記單位的實例、經 `Build` 給內容根目錄與登記 loader、冪等（5.3、5.4、5.16）；
- 救回規則照 Bevy，用比較並交換做成不上鎖；`DropOldFirst` 是盡力、作者用 `CarryOver` 保證；取消的計法（5.5）；
- 維護由執行器驅動；快照與交接槽；每 World 每幀一份 `AssetFrame`；新 World 的起點；套用在 `First`；例外清單（5.6）；
- 告知用 `AssetChanges` 不用 `Message`；只含本 World 請求過的（5.7）；
- 請求級與 id 級分開；錯誤碼與級別的對應；丟棄的出口留給編排層（5.8）；
- 設定在建立時帶、loader 帶鎖隨時登記與預先登記（5.4，照使用者 2026-11-22 的方向）；`Assets<T>` 是 `AssetFrame` 的型別化讀法；catalog 規則分兩組（5.3）；
- 5.9.1 的雜湊函式；
- 文字 loader 的預設上限 16 MiB；stb_image 的編譯設定（5.11）；
- 12.2 的三題；
- 依賴的細節：持有在 store 的邊、`AssetRef` 不持有；經路徑引用由 catalog 反查；強連通分量整個標循環；沒有 loader 的依賴略過；父項狀態不因依賴失敗而變；雜湊各 id 分開；不另加「連同依賴載完」的告知（5.17）。

**和別家不同的地方**（照 `-confirm-only-differences` 列出，不是問題）：
- 資產管理 Product 一份（Bevy 每 App、其餘行程一份）；
- 去重的鍵只有 id，settings 隨 catalog（Bevy 是路徑＋型別）；
- catalog 執行期讀（Unity 是 build 的產物）；
- 套用在 `First`（Bevy 在 `PreUpdate`）；
- 釋放一段（Bevy 兩段；本批沒有渲染）；
- 值不可變（Bevy、Godot 可變）——使用者選的，不是卡片判的；
- 循環引用是錯誤（Bevy 沒有處理：`bevy_asset/src` 的 `server/`、`loader.rs` 搜 circular、cycle 0 件）；
- 原始位元組雜湊預設開（Bevy 只在 processor 用來源雜湊）；
- 告知不用 Kernel 的 `Message`（Bevy 用 `AssetEvent`），因為第 4 批的 `Summary` 雜湊訊息序號（5.7）；
- 依賴由 store 的邊持有、loader 拿不持有的 `AssetRef`（Bevy 是值持有強 Handle）。

## 12. 使用者的決定、卡片的判定、待使用者定的

### 12.1 使用者的決定（2026-11-21）

| # | 決定 | 原話 | 比過的選項 | 出處 |
|---|---|---|---|---|
| 1 | asset 三件 | 「asset 三件照建議」 | 見第 0 節 | 主會話轉來 |
| 2 | 載好的值不可變、共享唯讀（(a')） | 「照 (a')，寫修訂 2」 | (a) 不立、各 World 各自解碼；(a') 現在立 ←；(c) Product 級共用快取；(d) 單一擁有 World。其中「各 World 各自解碼」的前提由決定 7 取代，不可變照留 | 本卡 |
| 3 | 排除不需要使用者決策的 | 「排除掉不需要我決策的。」 | 全交使用者；只留真正要定的 ← | 本卡；第 2、4 題依修訂 2 檢查 B3 拉回，之後由使用者定（決定 9、10） |
| 4 | 時間上限放寬——**作廢** | 使用者問「Asset 是機制還是功能？」「如果是機制，那麼未來修改成本如何？」，同意 (a) | (a) 放寬、全部修再送檢 ←；(b) 縮小範圍 | side chat。**作廢**（2026-11-21）：之後的「放寬到 13 h」一併作廢。使用者更正：「§14 時他媽的 測試。你別那麼扯蛋」「time-box-and-decision-gate 這個是他媽的查問題的時間盒子。」設計段沒有時間上限的規則，卡片和 side chat 拿它們停下、要使用者放寬是誤用 |
| 5 | 跨執行緒約束現在寫進契約 | 「如果現在以單線程的方向去設計，如果未來需要多線程的時候，影響範圍多大？」 | 延後到 RC；現在寫 ← | side chat |
| 6 | 作者只用 id；Asset 提供能力 | 「99.9%的情況，作者都不需要關心資產如何導入到Engine中的……」「我們定的是用id，所以在製作遊戲中時，作者也只會call ID」 | 路徑當持久鍵、id 日後接（修訂 2）；作者只用 id，載入、就緒、生命週期由 Engine 提供 ← | side chat |
| 7 | 資產管理在 World 外 | 「【換 World 時要不要重讀、兩個 World 要不要共用】這個應該和asset 無關」；對「放在 World 外（Product 級或 Engine 級）」回「可以」 | 每 World 一份＋CarryOver（修訂 2）；World 外 ← | side chat |
| 8 | 大地圖先照 Bevy | 「我們先按Bevy的來，後續，如果需要，我們可以使用Module來增加地圖。」 | 現在設計串流；照 Bevy 不內建、日後 Module 加 ← | side chat |
| 9 | 第 4 題：單筆到了就可見＋一組 id 的就緒查詢；轉場歸作者 | 「a 選項有問題。你見過什麼遊戲是那種先看到地面，才看到樹的？」更正：「是我想叉了……確實 a選項直接展示是有問題的，但是這種loading 轉場是需要作者自己來做的。」複述分工後：「可以。」 | (a) 單筆到了就可見 ←（＋就緒查詢）；(b) 照請求序或整組才可見；(c) 預設 (a)＋作者可設組、Engine 保證同幀；另提過「以組為單位、Engine 推依賴」 | side chat |
| 10 | 第 2 題：asset 對每個 id 的原始位元組算雜湊，作為能力；預設開、可關；比不比與不同時怎麼辦歸作者或網路 Module | 「同意你的建議。」 | (a) 作者對載好的值算；(b) ←；(c) asset 算且當規則（不同就拒絕開局） | side chat |
| 11 | 依賴由 Engine 推出 | 「我認為b」 | (a) 作者自己列模型與它的貼圖、材質 id；(b) Engine 推出：loader 宣告引用，就緒查詢包含整串依賴 ←；(c) 本批只留宣告引用的介面，推算日後做 | side chat |
| 12 | id 對照表先比對既有設計再提案 | 「id 對照表（格式、誰產生、Version）這個。讓任務卡去比對一下之前的設計。」 | （流程指示，不是選項） | side chat；落點 5.3 |
| 13 | 撤回凍結機制；設定在建立時帶、loader 帶鎖隨時登記；「Product 層 Resource」交編排層 | 「？？？？我怎麼覺得這個機制怪怪的」「我想的是，為何asset 要管這個， 其他家的 assetStore 和我說下。」看了別家的對照後：「轉回去」 | 原第 13 題的 (P) Product 層槽、(W) 第 4 批的每 World 槽，連同凍結點、凍結後只准相同、每個 World 再交一次、冪等判斷；對照 Bevy、Godot、Unreal、Unity，沒有一家有凍結點 | side chat（2026-11-22）；落點 5.4、5.3 (B)、5.16、12.3 第 13 題 |

決定 4～13 的理由照 side chat 的轉述記在主會話的 HANDOFF。正文裡的「三件之 N」指決定 1 的第 N 件；「決定 N」指本表的編號。

### 12.2 卡片的判定（使用者看過、沒有拉回）

| 題 | 採 | 退 | 根據、代價 |
|---|---|---|---|
| 3 載完才開始 | 作者的事；asset 給就緒查詢（決定 9 同時確定） | Engine 規定進遊戲前載完 | `-build-world-phase-is-mechanism` 直接推出；異常分級只告知、不規定處理。代價：舊守衛（未載完就 ready 是錯誤）消失 |
| 5 圖片 | 獨立的 `engine/modules/image`，隨 SDL 那批第一個使用者做 | 放 render；本批做 | 原始碼事實：`bevy_image` 不依賴 render；批次先後由卡片判 |
| 6 預算 | 延後到第一個產品搬回時；本批只做放數字的位置與探索量測 | 現在給數字 | 本批沒有用到數字的 Feature；到時交使用者定數字 |
| （1 持久鍵） | — | — | 由決定 6 取代（id） |

### 12.3 待使用者定的

修訂 2 檢查 B3 拉回的第 2、4 題，使用者已定（決定 10、9）；依賴也已定（決定 11）。

**第 13 題（交出，不在 asset 裡定）：Product 要不要有讓 Module 互相拿得到的 Product 層 Resource（像 Bevy 的 App 層 Resource）**

- 使用者 2026-11-22 撤回原第 13 題的 (P)／(W) 與凍結那一串（決定 13）：「Module 怎麼拿到 Product 層的東西」是編排層缺一個概念，asset 只是第一個需要它的。
- asset 需要它的地方：擁有型別的 Module 拿到 Product 的 store 來登記 loader（5.4、5.16）；world_asset 的建立參數 `AssetOptions` 由產品給（5.4）。
- 交給主會話與編排層定；照四樣轉給主會話（2026-11-22）。asset 依賴它（2.2），定之前 loader 的登記入口只定成 store 上的函式。

**仍歸使用者的**：本頁的動工前核准；第 6 題的數字（產品搬回時）；stb_image 的下載（image 那批）。

## 13. 外部組件的下載

| 組件 | 來源 | 用途 | 授權 | 何時 |
|---|---|---|---|---|
| stb_image（`stb_image.h`） | nothings/stb，釘 commit | image Module 的 PNG 解碼 | MIT 或 Public Domain | SDL 那批的 image Module（12.2 第 5 題）。要使用者同意，到時再問 |

## 14. 範囲の限界

- **交換點、生成世界還沒有宣告**：5.6、5.13 是需求，不是設計。滿足不了的話先回報，不在 asset 裡繞過。
- **第 4 批還沒合併**：雜湊客製點、`Summary` 的規則（含訊息序號進摘要），照第 4 批宣告（分支頭 1855d16a）。合併時形狀不同，5.7、5.9.1 跟著改。
- **依賴只用測試用 loader 驗**：第一個真正的使用者（模型引用貼圖）在產品搬回或 SDL 那批（第 9 節）。
- **預算延後**：沒有載入時間、記憶體、catalog 讀取時間的判定。
- **載入中的 World 的 `Summary`**：asset 的視圖不寫進摘要，載入進度不同的兩個 World 摘要可能相同；比摘要要在「全部完成」之後比。
- **`Summary` 讀兩次相同的前提**：兩次之間 catalog 沒有換（同一份內容版本）。
- **原始位元組雜湊只代表檔的位元組**：catalog 項目的 `format`、`settings` 不同時，雜湊相同、值不同（5.5）。
- **`DropOldFirst` 不重讀是盡力**：作者要保證就用自己的 `CarryOver` Resource 持有 Handle（5.4）。
- **丟棄件數沒有 E3 出口**：等編排層的結束回呼（5.8）。
- **不套用的 World 留住舊值**：它手上的快照讓已移除的值不釋放，直到下一次套用（5.6）。
- **`AssetChanges` 對不是每幀剛好讀一次的讀者有損**：讀者規則見 5.7；要不漏不重得等 Kernel 的「不進摘要」選項（第 15 節）。
- **Module 拿到 Product 層 store 的路未定**：交編排層（12.3 第 13 題）；在那之前 loader 的登記入口只定成 store 上的函式。
- **loader 的跨平台逐位元**：文字 loader 原樣交出位元組；其他 loader 由各自的 Module 宣告。
- **io 的多執行緒讀的契約**未確認（5.12）。
- **無鎖與資料競爭**：CI 沒有 TSan，AS-11 與並行案例的檢出只在本機 TSan 建置。
- **BigMap 的 region 個數**未查；catalog 的規模估計等產品搬回。

## 15. 觀察到、不在本批的（報給主會話）

1. **批次表第 6 組「asset」一列**：依賴加「生成世界（重做）」與 JSON Module；單位改成兩個；SDL 那批的範圍記上「image Module（stb adapter）」。
2. **v2 之前的 target 文件**：`target/runtime-composition.zh-Hant.md` §3a 與 DI 3、`target/README.md` K.3 表的 A3、A4 列建立在已拿掉的前提上；本批只動 `assets.zh-Hant.md` 的頂端標記。
3. **打包時附第三方授權告示**：stb_image 加進「之後的小事」。
4. **SDL 的 adapter、ufbx 歸哪一類**仍未定。
5. **catalog 的 owner 已由決定 6 解消**：格式歸 asset；master 的 `AssetCatalog` 的工具（audio_editor 寫，ui_editor、model_viewer 讀）搬回時照新的契約。
6. **生成世界那批收 5.13 的 G-1～G-3，交換點、threads 那批收 5.6 的 X-1～X-6。**
7. **11-11 方向 2 已推翻**（決定 7）：舊頁 `assets.zh-Hant.md` 與 `rebuild-1111-decisions-status.zh-Hant.md` 對這一條的狀態要更新（主會話的文件整理）。
8. **Kernel 的 `Message` 與 `Summary`**：第 4 批的 `Summary` 雜湊每個訊息緩衝的序號與游標（第 4 批分支 `World.cpp:1188-1201`），和 payload 的雜湊函式無關。依完成時序或外部事件發訊息的 Module（asset 以外，例：日後的網路、輸入）都會讓摘要依時序變。asset 改用 Resource 繞開（5.7）；要不要給訊息型別「不進摘要」的選項，交主會話與第 4 批判斷。
9. **編排層的結束回呼**：登記單位沒有 Product 結束時的回呼，World 也先銷毀，所以 asset 的丟棄件數（E3）沒有出口（5.8）。要不要加，交主會話。
10. **cross-component-rules**：AS-10 (c)（loader 的值 const 並行安全、解構不綁執行緒）與「判定用的內容在進判定前載完」在實作第 6 步登記，owner asset。

## 16. 停止條件

- 實作段的測試、計測、判定卡住時：AGENTS §14 冒頭的三條（只用在測試與判定，不是設計段的時間上限）。
- 交換點或生成世界的宣告和 5.6、5.13 的需求衝突。
- 第 4 批合併後，雜湊客製點或 `Summary` 的規則和 5.9.1 的假設不同，而且影響 AS-6。
- 實作中發現某個 loader 必須碰 World（RC-AS2），或 World 端必須上鎖（RC-AS9）。

## 17. 修訂 0 的敵對式檢查：處理對照

檢查者 Fable 5.2／high（作者 Opus 5.5）。證據 `declaration-r0-adversarial-check.md`。31 條全部採用。表裡的節號與題號都是修訂 1 的（修訂 3 重寫了形狀，節號已變）。

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

## 18. 修訂 2：管理面對照的敵對式檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。
**對象**：卡片給使用者的管理面對照與兩個提案（`management-comparison-proposal.md`）。
**結果**：blocker 4、should 9、nit 4，證據 `management-comparison-adversarial-check.md`。表裡的節號、AS-1～AS-10、RC 都是修訂 2 的；其中 (a')、5.3 的共用等處理，修訂 3 隨使用者的決定 6、7 改寫。

卡片抽查了關鍵反證，都屬實：Unity `SceneSystem.cs:381`、`SerializeUtility.cs:1079`；Bevy `render_asset.rs:332-333`、`:509`；`runtime-composition.zh-Hant.md:12`、`:16`。17 條全部採用。

| 條 | 處理 |
|---|---|
| B1「五家都是行程級一份」不成立 | 4.2、4.3 改成「請求服務都跨 World 共用；值存在哪各家不一致」；Bevy 的 `RenderAssets<T>`、Unity 的場景資料每 World 一份寫進表 |
| B2 (c) 的代價歸因錯 | (c) 退（12.1），退掉的理由照檢查寫；5.3 末段記下日後共用要解的問題 |
| B3 (c) 沒寫共用的值怎麼進 World | 5.3 末段與 RC-AS3：日後任何共用做法都要先回答它（同步拿撞 AS-4、5.5；經交換點撞 G-4） |
| B4 可選 id 欄位有矛盾 | 不留欄位；改用 S3 的來源名擴張點（5.2、12.2 第 1 題） |
| S1「Godot 寫錯」過頭 | 4.1 的 Godot 列分成轉檔、改名兩軸 |
| S2 第 1 題的銜接 | 5.8.1：Handle 雜湊解析後的路徑、不雜湊 id；5.2：路徑永遠是有效的鍵、id 是加法；5.10：轉檔一軸照疊掛載，id 只管改名一軸 |
| S3 漏了來源名當擴張點 | 採為第 1 題的做法 |
| S4 第 7 題有現成 Feature | 3.2、5.3、12.1 引 `PlayerAnimationContent`；註明 Client 的 World 分法未判；RC-AS3 改寫 |
| S5 (c) 沒寫的代價 | (c) 退；「loader 產出不可變、解構可在任何執行緒」隨 (a') 進 AS-10 與 5.4；第 8 節「多 World 共用」一列標新推導 |
| S6 漏了 (d)、(a') | (a') 採、(d) 延後（12.1）；`Assets<T>` 值的可變性補上（AS-10）；作者在執行期自己加值不在本批（5.2） |
| S7 11-11 的轉述偏窄 | 12.1 改引舊頁 §1 第 1 點原文；AS-10 是新 DI，不改 AS-1 的三類 |
| S8 過度概括 | 4.3：立即釋放只有 Bevy、Godot；含 settings 是 GYO 的差異；上限在每幀與子系統層 |
| S9 CPU 副本、`Unused` 的用途 | 5.14 A4 的理由改正；RC-AS7 加「渲染要在上傳後丟 CPU 副本」 |
| N1 Godot 依賴字串的格式 | 不寫格式，只寫「UID＋備用路徑」 |
| N2 Unreal 的保活 | 4.2 把「完成委派後放硬參照」和「Handle 保活」分開標 |
| N3「主執行緒每幀套用」 | 4.2 的本頁欄寫「每個 World 自己的套用點」 |
| N4 熱重載在共用下的形狀 | 不影響本批；AS-10 寫明熱重載是「換一個值」 |

**修訂 2 本身**：送 Fable 5.2／high 再做一次敵對式檢查（第 19 節記結果）。

## 19. 修訂 2 的敵對式檢查與修訂 3 的處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。**結果**：blocker 3、should 12、nit 10，證據 `declaration-r2-adversarial-check.md`。卡片抽查了 B2（`Product.cpp:634-646`：`DropOldFirst` 先取出帶過去的值再銷毀舊 World），屬實。

修訂 3 同時照使用者的決定 6、7（id、World 外）重寫了形狀，所以不少條是「隨形狀消失」。

| 條 | 處理 |
|---|---|
| B1 AS-10 的守衛沒有檢出力 | AS-10 拆三條，各配會失敗的守衛：(a) 負向編譯（先例 `gyo_kernel_compile_fail_samples`）、(b) 同 id 再送值不變、(c) 別的執行緒解構；並行案例寫明只在 TSan 有檢出力（AS-11、第 10 節） |
| B2 `DropOldFirst` 也帶過去 | 隨形狀消失（CarryOver 拿掉）；救回規則照 Bevy 定在 5.5；scenario 第 5 步測救回 |
| B3 第 2、4 題要使用者定 | 拉回交使用者，附遊戲例子；使用者在 side chat 定了（決定 9、10）；第 3 題留卡片判（12.2） |
| S1 AS-10 三件事混寫 | 拆成 (a)(b)(c)；(c) 寫明只靠審查的部分 |
| S2 存放形狀觀察不到、例外不只一個 | 值在 World 外的 store，兩個 World 共用是 scenario 第 3 步可觀察的；`Get` 只回 `const T*`、只在 System 執行期間有效（5.5）；5.6 列出完整的例外清單 |
| S3 Bevy 丟 CPU 副本是就地取走 | 5.15 A4 寫明 Bevy 的做法與行號，以及 AS-10 之下的路是第二段釋放 |
| S4「共用之後再加不改 API」無根據 | 隨形狀消失：共用就是本頁的形狀（5.4） |
| S5 決定不在 HANDOFF | 已照四樣轉主會話（2026-11-21，兩次）；第 0 節標出處 |
| S6 持久鍵在改名一軸自相矛盾 | 隨形狀消失：持久鍵是 id（5.2）；雜湊用 id 字串（5.9.1）；讀兩次相同的前提寫進第 14 節 |
| S7 改名擴張點沒有 DI 與 RC | 隨形狀消失：id 是核心（AS-6、AS-12）；RC-AS6、RC-AS8 |
| S8「來源」三義、魔數 | 不再有來源名：catalog 的路徑是內容根目錄下的相對路徑（照 master，5.3、5.12）；疊掛載不再需要；loader 由 catalog 的（`type`、`format`）選，不靠魔數 |
| S9 來源位元組雜湊無條件卻沒有範圍 | 決定 10：進範圍（R3）、第 2 步與測試（第 10 節）；介面在 5.5 |
| S10 型別序號 | 拿掉（5.9.1） |
| S11 `Summary` 測試殺不掉「雜湊索引」 | 改成兩份項目順序不同的 catalog（內部索引不同）比 `Summary`（第 10 節） |
| S12「不是孤例」拿不同種類比 | 4.3 改寫：資產管理在 World 外是共通的，本頁照它 |
| N1 X-3「第 1 題之外」 | 交換點需求表重寫（5.6） |
| N2 3.2 標題與內容矛盾 | 3.2 重寫；共用快取那一條拿掉 |
| N3 12.1 缺 (b) | 12.1 改成決定一覽；(a') 的比較寫在決定 2 一列 |
| N4 io 已有疊掛載 | 5.12 引 `MountPoint.hpp:16`、`Vfs.hpp:86-88`，寫明本批不靠它 |
| N5 來源標記 | Unity blob 改〔源〕（`blob-assets-concept.md:7`）；`RuntimeContentManager` 也管 content scene（`:1042`）；`bevy_image/Cargo.toml:59-79` |
| N6 F2 不確定 | 「兩個 World 用同一內容」只寫在 F1 |
| N7 證據檔不一致 | `management-cross-table.md` 的 E1 改 E2；`reference-survey-management.md` 的 Godot 依賴格式加更正註記 |
| N8 時間 | 設計段沒有時間上限的規則（使用者 2026-11-21 更正）；0.1 的估計只當資訊 |
| N9 AS-1 漏 Extract | AS-1 寫明 Extract 是唯一跨 World 讀的路；守衛改成「讀到相等的值」 |
| N10 範圍代號與 DI 代號混 | 範圍改用 R1～R10 |

**修訂 3 本身**：送 Fable 5.2／high 敵對式檢查（第 20 節記結果）。

## 20. 修訂 3 的敵對式檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。對象 7a4dd177。**結果**：blocker 4、should 12、nit 5，證據 `declaration-r3-adversarial-check.md`。同日先送的一輪在 catalog 改寫前停掉，沒有結果。卡片抽查了第 4 批的共用槽（第 4 批宣告第 6 節、11.5），屬實。21 條全部採用；沒有要交使用者定的（都在已定的決定之內落地）。

| 條 | 處理 |
|---|---|
| B1 別的單位碰不到 world_asset 的實例 | 5.4：用第 4 批的共用槽 `AssetSetup`（`Create`／`Find`），world_asset 在 `Finish` 交給 store；凍結點；2.2 與第 6 節加上依賴；第 8 節第一列的決勝理由改成以 Feature 決勝 |
| B2 訊息的序號與游標進 `Summary` | 告知改成每幀整份換掉的 Resource `AssetChanges`（5.7），不用 Kernel 的 `Message`；5.9.1 改；第 6 節第 4 項與第 15 節報給主會話 |
| B3 循環的「不持有」沒有機制、依完成順序 | 持有改由 store 的依賴邊，loader 拿不持有的 `AssetRef`（5.17）；強連通分量整個標循環，和順序無關；第 10 節兩種順序、自我引用，並放進等價測試 |
| B4 AS-2 守衛與 5.3 互斥 | AS-2 守衛改成「沒有 loader 的型別合法、請求時 `NoLoader`」 |
| S1 救回的競態、維護的驅動、`DropOldFirst` | 5.5 的「死了」標記與比較並交換；5.6 維護由執行器驅動；5.4 `DropOldFirst` 是盡力、作者用 `CarryOver` 保證；第 10 節引用計數與 scenario 第 5 步 |
| S2 例外清單不全、快照回收 | 5.6：快照帶原子參照計數、每 World 一個交接槽；World 讀的東西都在快照裡或凍結後不變；例外清單六項；store 端用不透明記號不用 World 名 |
| S3 凍結時點、每 World 給一次、Product 級設定 | 5.4 凍結點與「凍結後只准相同」（RC-AS10）；Product 級設定經共用槽、同樣冪等；5.3 (B) 每個 loader 只驗一次；loader 在 Product 層、讀值要本 World 的 `Assets<T>` |
| S4 `format` 省略 | 5.3：取副檔名；沒有副檔名要寫 `format` |
| S5「沿用 master」不實處 | 5.3 每條標照舊／新增／放寬／丟掉；`content.json` 的規則與 owner；`shader_bundles` 放寬；不認得的鍵保留；規則分 (A)(B) 兩組 |
| S6 經路徑引用與轉檔、沒有 id 的失敗、請求級與 id 級 | 5.17 的限界與 RC-AS6；`DependencyFailure{parent, reference, code}`；5.5、5.8 分開請求級與 id 級 |
| S7 Match 沒有貼圖 loader | 5.17：依賴的型別沒有 loader 就略過；F2 補這個要求 |
| S8 原始位元組雜湊不等於內容身分 | 5.5、5.9 寫明限界，第 14 節 |
| S9 新 World、只發本 World 的、每幀一個指標 | 5.6：`Build` 與準備函式之前取快照、每 World 每幀一份 `AssetFrame`；5.7：只含本 World 請求過的、新 World 從建起來開始、命中也收到 `Added` |
| S10 丟棄沒有出口、件數等式 | 5.8：工作與請求兩條等式；丟棄留在 store 的計數，E3 出口要編排層的結束回呼，報給主會話 |
| S11 第 8 節、第 6 節殘留 | loader 列改寫；第一列決勝理由改寫；第 6 節第 1、4 項補上 |
| S12 AS-10 (c) 沒登記 | 第 6 步登記到 `docs/cross-component-rules.md`；AS-3 守衛與第 10 節補會引用的 loader |
| N1 編號 | 頂端改成九件（決定 4～12）、AS-1～AS-13 |
| N2 `Pending` 兩義 | 筆數改叫 `Unfinished()`；連同依賴的狀態是 `Incomplete`／`Complete`／`Failed` |
| N3 第一個 World 不跑 `Finish` | 5.3 驗證時點寫明由下一個跑到 `Finish` 的 World 做 |
| N4 `ReferencePath` 的壞路徑 | 5.17：`BadReference`，比對分大小寫 |
| N5 沒給根目錄 | 5.4：catalog 是空的，不是啟動錯誤 |

**修訂 3 的這一版**：再送 Fable 5.2／high 敵對式檢查，結果與處理在第 21 節。

## 21. 修訂 3 修正版（59a63c16）的敵對式檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。**結果**：blocker 2、should 11、nit 7，證據 `declaration-r3fix-adversarial-check.md`。第 20 節的 21 條裡 B4、S8、N1～N5 確實解決，其餘換了機制但留洞；這一輪全部採用。S-2 會改編排層，交使用者（12.3 第 13 題）。

| 條 | 處理 |
|---|---|
| B-1 引用計數的找、建、世代 | 5.5：凍結時每個 id 一個固定槽（不釋放）＋世代＋「放掉待處理」旗標；只有換成功的一方排工作；世代不合的結果算取消；快照以（槽、世代）查。第 8 節「引用計數」列寫明退掉 Unity 式的理由；第 10 節加四個案例 |
| B-2 變更清單在共享快照裡會丟 | 5.6：變更清單不放快照；交接槽放「包」，維護換出、併入、放回；包有上限與溢出告知；第 10 節加兩個案例 |
| S-1 凍結點、settings 驗證時點、「沒設定」 | 5.4：(W) 的凍結點是 world_asset 的第一個準備函式，(P) 是 `Configure` 結束；5.3 (B)2 改在凍結點一次驗；5.8 拿掉 settings；「沒設定」算沒意見 |
| S-2 Product 層的資料走 World 層的槽 | 交使用者：12.3 第 13 題 (P)／(W)，卡片建議 (P)；第 8 節第一列寫兩個選項的理由 |
| S-3 依賴方向、判等、同名兩型別 | 5.4：擁有型別的 Module 拆兩個單位；`Find` 回空是斷言；loader 以 C++ 型別判等；5.3 (B)4 同一 `type` 產出型別相同；第 6 節第 5 項 |
| S-4 `AssetChanges` 的讀者 | 5.7 的讀者規則與每筆序號；第 8 節「告知」列補「不進摘要」的選項與取捨；第 14 節 |
| S-5 維護的驅動 | 5.6：清旗標後再查佇列；拿掉每次發佈上限（R9、3.3、第 8 節）；X-1 加「提交不上鎖、維護有專屬或優先的執行機會」 |
| S-6 World 的登記、第一份快照、回收 | 5.6：在準備函式裡發記號、開槽、取快照（store 的鎖、不在幀內）；`AssetFrame` 解構時註銷；登記失敗的 World 不留東西；刪掉「`Build` 先取」；第 14 節 |
| S-7 依賴的六處 | 5.17：一條引用的判定先後與邊級失敗；`Get(ref)` 的契約；循環標記單調；告知給所有遞迴祖先；件數；5.6 的快照內容補依賴；5.8 補碼 |
| S-8 `format` 與 master 的 text＋json | 5.16：loader 可認多個 `format`，文字 loader 認 `txt`、`json`；5.3 (A)6、7 的形狀規則與副檔名定義 |
| S-9 決勝理由與系譜 | 5.3 與第 8 節 catalog 列改以 F5、F6、F9 決勝，「現有內容照樣有效」降為費用；World 端無鎖標為新推導（Unity 的佇列拿 SpinLock） |
| S-10 DI 條文與第 5 節衝突 | AS-1（四種 Resource、`AssetRef`）、AS-4（準備函式也換）、AS-7、R1、R7、第 6 節第 6 項、第 9 節第 5 步 |
| S-11 第 10 節判不了的四處 | `AllFinished` 與 `Complete` 分開；`Removed` 用 store 的讀法觀察；等價測試各組和自己的基準比；補 B-1、B-2 的案例；真執行緒也比 `AssetChanges` 的合計 |
| N-1 請求級失敗的時序、`Unfinished()` | 5.5：記在 World 本地、由套用 System 直接放進清單；5.7：`Unfinished()` 含本幀剛送出的 |
| N-2 `Added` 的單位、`CarryOver` 的 Handle | 5.7：每個（World、id）一筆；`CarryOver` 帶來的不算這個 World 請求過 |
| N-3 5.3 的標記與數目 | 5.3 決勝理由寫明新增五條、放寬兩條 |
| N-4 引用小處 | `Product.cpp:710`；準備函式的先後照 O-7 寫；例外清單第 6 項改成維護的「已排」旗標。第 4 批的實作名 `ModuleBuffer` 等合併後再對（第 14 節） |
| N-5 `DropOldFirst` 下替換失敗 | 5.4 (W)：是 Product 結束 |
| N-6 Extract 與共用 store | AS-1：store 不是哪個 World 的資料，經自己的 `AssetFrame` 讀不算跨 World |
| N-7 第 2 步的檔位 | 第 9 節：照 high（使用者定 xhigh 只用在調查），審查時若認為要，先問使用者 |

**這一版**：使用者 2026-11-22 撤回凍結機制（第 22 節）之後，再送 Fable 5.2／high 敵對式檢查。

## 22. 使用者 2026-11-22 的決定 13：撤回凍結機制

使用者在 side chat 看了第 13 題的 (P)／(W) 後說「我怎麼覺得這個機制怪怪的」「為何asset 要管這個」，看了別家的對照後說「轉回去」。side chat 轉來的理由：凍結那一串是卡片自己繞出來的——「World 內不上鎖」只管 World 執行緒在幀裡的操作，登記在啟動時、讀 loader 表的是工作執行緒，照 Bevy 一把鎖就夠；「Module 怎麼拿到 Product 層的 store」是編排層缺的概念，不是 asset 該解的（no-shit-mountain）。

| 改了什麼 | 落點 |
|---|---|
| 撤回凍結點、凍結後只准相同、每個 World 再交一次、冪等判斷、`AssetSetup` 槽、RC-AS10 | 5.4、5.16、第 8 節、第 10 節、第 11 節 |
| 設定改成 world_asset 的建立參數 `AssetOptions`（照 Bevy `AssetPlugin`） | 5.4、5.3 (2) |
| loader 表帶鎖、隨時可以登記；預先登記的請求先等（照 Bevy 的 preregister） | 5.4、5.16、第 8 節「loader 表」列 |
| (B) 組的檢查在登記當下做，錯誤回給登記的一方 | 5.3 (B)、5.8 |
| `NoLoader` 改成 id 級（loader 隨時可能登記，不是「這一次請求」的事）；依賴的略過看「沒有 loader 也沒有預先登記」，單調 | 5.5、5.8、5.17 |
| `Assets<T>` 改成 `AssetFrame` 的型別化讀法，不必每個型別各登記一個 Resource（卡片照 no-shit-mountain 判） | 5.4、AS-1 |
| 第 13 題改寫成「Product 層 Resource」交主會話與編排層 | 12.3、2.2、第 6 節第 4 項、第 14 節 |

修訂 3 修正版檢查的 S-1（凍結點）、S-2（Product 層的槽）因此不再適用；第 21 節的對應列照歷史保留。

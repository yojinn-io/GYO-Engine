# 事前宣告：asset Module（`v8-arch-gyo2-asset`）

更新：2026-11-22（修訂 3）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：動工前核准（使用者 2026-11-22，原話「核准」；核准的版本 55a14825）；實作完成；合併前核准（使用者 2026-11-23：「Probe 留著，adapter 交主會話」、「AS-1、AS-11 和突變數都核准」——DI 的最終文字與突變 108 個）。**核准後加入 adapter**：使用者 2026-11-23 在主會話「adapter 照建議放 world_asset」，world_asset 加 `WorkerPoolExecutor` 與對 `GYO::Threads` 的連結（第 15 節第 13 項、30.3 的 S3 列）；這部分等使用者確認。** 實作和本頁不同、或本頁沒寫清楚而在實作時判的地方，列在第 30 節；正式頁是 [asset.zh-Hant.md](asset.zh-Hant.md)。

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
| 執行演算法照 Bevy（2026-11-22，本卡，決定 14） | 拿掉卡片自己加的前提；表與鎖、強核心、待略過、套用點、輪詢 | 5.5～5.7、AS-11、第 24、25 節 |
| 撤回凍結機制（2026-11-22，side chat 轉來） | 設定在建立時帶；loader 帶鎖隨時登記；Module 拿到 `AssetLoaderRegistry` 的路交編排層 | 5.4、12.3 第 13 題、第 22 節 |
| 依賴由 Engine 推出（2026-11-21，side chat 轉來） | loader 宣告引用；就緒查詢的「好了」包含整串依賴 | 5.17、AS-13 |
| `v8-arch-gyo2-parallel-asset-and-stale-mutants`（2026-11-21） | IO 執行緒加交換點；批次表依賴含交換點 | 2.2 |
| `v8-arch-assets-bevy-model`（2026-11-11，kernel HANDOFF:144） | 不選 World，照 Bevy 拆成請求狀態、每型別一份 `Assets<T>`、GPU 資源；loader 由各 plugin 登記 | 方向 2「放在使用它的 World」由決定 7 推翻；三類拆法與「System 不持指標」保留 |
| `v8-arch-target-di-approved`（2026-11-13） | 舊 DI 1～3 與 Replacement Criteria 核准 | 第 7 節的系譜 |
| `v8-arch-gyo2-build-world-phase-is-mechanism`（2026-11-16） | 有沒有 Loading 階段由作者決定 | 5.7、12.2 第 3 題 |
| `v8-arch-gyo2-save-unaware-means-format-only`（2026-11-16） | Engine 不認得存檔的格式 | 5.9 |
| `v8-arch-gyo2-module-plugin-owner-definition`、`v8-arch-gyo2-json-adapters-dir` | Module＝我們維護、介面是我們的；第三方放該 Module 的 `src/adapters/<組件>/` | 5.1、5.11 |
| `v8-arch-gyo2-engine-not-rigid`、`-mechanism-not-conservative` | 預設方便、不禁止別的做法；建機制這一輪不選保守的選項 | 全頁 |
| `v8-arch-gyo2-infrastructure-before-products`、`-migration-needs-consent` | 先建基礎；搬產品要使用者同意 | 3.2 的 Feature 只用來推需求 |
| `v8-arch-gyo2-replace-order-author-choice` | 使用者點名 BigMap 是有的 Feature | 3.3 |

side chat 轉來的十件（決定 4～13），卡片已照四樣轉主會話記 HANDOFF（2026-11-21～22）；決定 14 由本卡轉。記好之前，HANDOFF 裡查不到它們。

**已合併、本頁要對齊的**：
- [編排層](orchestration.zh-Hant.md)：O-2 生命週期（`Configure` 在產品層一次、`Build` 每個 World）、O-5 啟動報告、O-6 登記檔、O-7 準備時機、O-8 替換、O-10 帶過去、O-11。
- `target/error-handling.md`〈Error levels〉：`…Error`＝E1，`…Failure`＝E2；Engine 的 Module 不寫日誌。
- [事件游標](event-cursor-declaration.zh-Hant.md)：`Message`；上限預設 1024。
- Kernel 的 `DiagnosticsWriter<Record>`（`engine/ecs/kernel/include/engine/kernel/Params.hpp:709`）。
- [時間＋事件](time-events-declaration.zh-Hant.md)第 3.6 節：預設骨架 `Main`（`First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）。
- 先例：「不認得 ECS 的函式庫＋World 端的 Module」——`time`／`world_time`（`time` 在 core，#261 起不登記；`world_logging` 是 library_only，不是有登記單位的先例）。
- 登記單位的實例活到 Product 結束（`RegistrationUnit.hpp` 的註解：「a unit keeps what it records per World by WorldName()」）；`ProductSetup` 沒有選項包、沒有 Product 層的共用值（`Setup.hpp:88-110`）。

**進行中、本頁要對齊的**：
- **第 4 批**（draft #259）：分支 `claude/gyo2-authority-rollback-bda9ba` 的宣告（分支頭 15e79bd1；本頁引用的 `World.cpp` 行號與修訂 6 07c80c4c 相同）。3.2 雜湊客製點；3.3「Engine 的 Module 放進 World 的型別都附雜湊函式」；4.4 digest 不含 id；第 21 節「到達時刻要影響判定，走 `rollback.Input<T>`」。
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
| 實作段 | 28～42 h（推定；修訂 2 是 22～34 h） | 多了 catalog 的 Data Contract、World 外的 store（照 Bevy 的表與鎖）；交換點與生成世界的形狀還沒定，它們的宣告核准後重估 |

## 1. 這一批做什麼（先用遊戲講）

FPS 的 Client 進對局之前，要載角色模型、武器貼圖、字型、音效、規則與地圖的 JSON。

**內容先有 catalog**：內容根目錄的 `content.json` 列出 catalog 檔；catalog（Data Contract）寫「id `rules.movement` 是文字，在 `rules/movement_rules.json`」「id `char.hero` 是模型，在 `models/hero.fbx`」。catalog 由工具、Script 或手寫產生（AGENTS §8）。作者**只認得 id**，不管檔在哪、怎麼解碼。

步驟：
1. 生成世界階段，作者的 System 對 `AssetRequests` 說：「我要 `rules.movement`。」**當場**拿到 `Handle<Text>`。
2. 讀檔與解碼由 World 外的資產管理（`AssetStore`）在 World 執行緒之外做，不碰 World。
3. 載好的值放進資產管理，**不可變**。
4. 每個 World 在自己的套用點（預設 `Main`/`First`）把 store 裡所有活著的項目的狀態與值拉進自己的視圖 `AssetView`（照 Bevy 的 `Assets<A>` 裝全部）；一幀之內看到的都是同一份。
5. 作者的 System 看狀態，全部載完才離開生成世界——等不等是作者的事。

**失敗時**：例如檔案不在，狀態是「失敗」，帶 `AssetLoadFailure`（E2）。asset 不記日誌、不自己重試，World 照跑；作者決定停下、再請求、換預設內容或顯示錯誤。日誌要記，就在自己的 System 輪詢 `AssetView::Failures()`。

**中途讀檔、換 World**：不是 asset 的事。資產管理在 World 外，Handle 的引用計數跨 World 有效。作者照 O-8 在現在的 World 驗證存檔要的 id 都載得到，再發 `ReplaceRequest`；新 World 請求同樣的 id，資產還活著就直接命中。

**Client 的 Sim 與 Present 都用角色模型**：兩個 World 請求同一個 id，資產管理只載一次、只存一份，兩邊唯讀共用。

**模型檢視器的 `info` 指令**：不建 World，只連不認得 ECS 的 `asset` 函式庫與模型 Module 的 loader；把位元組交進去，拿回值。

## 2. 範圍

### 2.1 這一批做的（動工前核准之後；實作等第 5 批）

| 代號 | 內容 |
|---|---|
| R1 | 兩個單位（照 `time`／`world_time` 的先例；卡片照 L17 判，第 8 節）：**`engine/modules/asset`**（`GYO::Asset`，library-only，不認得 ECS）——id 與 catalog（Data Contract 的讀、寫、驗證）、loader 介面、整檔讀取（經 io）、文字 loader、`AssetStore`（World 外的資產管理）、`Handle<T>`、`AssetRef<T>`、`AssetLoaderRegistry`；**`engine/modules/world_asset`**（`GYO::WorldAsset`，編排層 `Module` 的子類）——擁有 Product 的 `AssetStore`（建立參數 `AssetOptions`）、每個 World 的 `AssetRequests` 與 `AssetView`、套用點的 System 與準備函式、雜湊函式 |
| R2 | catalog：id→（型別、路徑、settings）的對照表；格式、Version、Validation Rule 是 asset 擁有的 Data Contract；執行期讀（不是 build 的產物） |
| R3 | `AssetStore`：以 id 請求、引用計數、沒人用就放掉、`Failed` 之後可以再請求；載好的值不可變、共享唯讀；**依賴追蹤**（loader 宣告引用、連同依賴的狀態、循環偵測）；**原始位元組雜湊**（預設開） |
| R4 | loader 的介面與登記入口：擁有型別的 Module 登記（5.16） |
| R5 | 讀檔與解碼在 World 執行緒之外、不持 store 的鎖；World 的資料不上鎖（AS-11） |
| R6 | 每個 World 的 `AssetView`、套用點、狀態查詢、一組 id 的**就緒查詢**（連同依賴） |
| R7 | 錯誤：請求級與 id 級的 `AssetLoadFailure`（E2）、依賴失敗、catalog 與登記的啟動錯誤；丟棄件數（E3 的出口不在本批，5.8） |
| R8 | asset 放進 World 的型別的雜湊函式（5.9.1；第 4 批 3.3） |
| R9 | 上限與擴張點：在飛上限、loader 的位元組上限，都在 `AssetOptions`（5.4） |
| R10 | 新頁 `target/v2/asset.zh-Hant.md`：Design Invariants、Replacement Criteria。舊 `target/assets.zh-Hant.md` 頂端標「已由 v2 取代」 |

### 2.2 不在這一批

- **事先轉檔的工具與它的資料契約**（三件之 3）。本批只保證接得上（5.10）。
- **image Module 與 stb_image**：隨 SDL 那批的第一個使用者做（12.2 第 5 題）。本頁先定位置、授權與 adapter（5.11）。
- **其他 loader**：模型（ufbx，進 model Module；SDL adapter 與 ufbx 歸哪一類，HANDOFF 未定）、音效（audio 那一側的小批〔實作時改：原寫「world_audio 那批」，world_audio 已撤（使用者 `v8-arch-gyo2-world-audio-withdrawn`），第 30 節第 1 項〕；clip 的來源與預設起播時刻也在那批，使用者 2026-11-22 核准 audio 重審 `v8-arch-gyo2-audio-review-approved`）、字型（text Module，SDL 那批）、二進位（沒有點名的 Feature）。
- **GPU 資源**（Bevy `RenderAssets<T>`）：歸 render，隨 SDL 那批。
- **大地圖的串流**（決定 8）：照 Bevy 不內建。asset 只給上限與 region 級的 id；串流的規則與「進判定」屬日後的地圖 Module（RC-AS1）。
- **沒有點名 Feature 的，本批不做，也不在介面上先留形狀**：熱重載與 `Modified`（model_viewer 的 `RefreshContent` 是手動重讀，工具可以直接呼叫 loader）、一個檔分出多個子 asset、作者在執行期自己加值（Bevy `Assets::add`）、資料夾自動產生 catalog（產品可以在程式裡加 catalog 項目，5.3）。
- 交換點、生成世界本身的設計（第 5 批）。threads、io 的重審（批次表第 6 組）；本批只交需求（5.6、5.12）。

**實作的依賴**：編排層（#244）；第 5 批的多個 World、交換點；第 5 批的生成世界（重做）；第 4 批合併（雜湊客製點、`Summary` 的規則）；**編排層的 Product 層 Resource**（Module 拿到 Product 的 `AssetLoaderRegistry` 來登記 loader；12.3 第 13 題交主會話，5.4）；JSON Module（catalog 的格式）。

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
| 結果怎麼讓 World 看到 | `PreUpdate` 照完成順序〔源〕 | 每幀處理命令〔源〕 | 作者輪詢〔文〕 | 委派〔文〕 | 完成事件〔文〕 | 主執行緒輪詢 | 工作完成時在 store 的鎖下寫進表；每個 World 的套用點換視圖（決定 9） |
| 失敗怎麼告知 | `Failed`＋失敗事件〔源〕 | 狀態 `Error`〔源〕 | FAILED，沒有細節〔文〕 | 缺檔也呼叫完成委派〔文〕 | handle 狀態〔文〕 | — | 狀態與就緒查詢，輪詢（E2；照 Unity、Godot，5.7） |
| 熱重載 | 監看＋`Modified`〔源〕 | 〔未〕 | 原地刷新〔文〕 | 〔未〕 | 〔未〕 | Engine 有、產品沒用 | 不在本批 |

### 4.3 共通、分歧、本頁的位置

**共通**：核心不認得格式；身分在請求時就給；圖片不用視窗層解碼；持久鍵不隨「原始→轉檔」改變；**資產管理都在 World 外或高於 World**（Bevy 每 App，其餘行程單例）。

**本頁的位置**（使用者的整理經 side chat 轉來，卡片照它寫）：
- **管理骨架像 Unity Entities**：id 當持久鍵、資產管理在 World 外、值不可變。
- **載入方式像 Bevy**：執行期讀原始檔、各 Module 登記 loader、不內建串流。
- **和 Unity 的差別**：執行期就讀 id 對照表（catalog），不靠 build 的產物。

**仍和別家不同的**（第 11 節；另見那裡的完整清單）：Product 一份而不是行程一份（GYO 沒有行程級靜態物件的先例；回主選單另建 Product 時資產重載）；去重的鍵只有 id，settings 隨 catalog 項目；值不可變（Bevy、Godot 可變）。

## 5. 設計（Contract）

### 5.1 位置、名字、依賴（卡片照 L17 判，照 N11～N17、O-6 與 `time`／`world_time` 的先例）

| 單位 | 目錄 | target | 登記檔 | 依賴 | 內容 |
|---|---|---|---|---|---|
| asset | `engine/modules/asset` | `GYO::Asset` | library_only=yes、default=no | base；private io、json | `AssetId`（id）、catalog 的讀寫與驗證、loader 的介面（`LoadContext`、`LoaderFailure`）、整檔讀取、文字 loader、`AssetStore`（含表與鎖、強核心、依賴圖、原始位元組雜湊）、`Handle<T>`、`AssetRef<T>`、`AssetLoaderRegistry`、執行器的介面（asset 用自己的話定，world_asset 在第 4 步接上交換點給的）、`AssetRef<T>` |
| world_asset | `engine/modules/world_asset` | `GYO::WorldAsset` | library_only=no、default=yes | asset、base、kernel、orchestration；threads 看交換點 | 擁有 Product 的 `AssetStore`（建立參數 `AssetOptions`）；每個 World 的 `AssetRequests`、`AssetView`；套用點 System、準備函式、雜湊函式 |

- **為什麼拆兩個**：F4、F6 的工具與擁有型別的 Module 只要 loader 與 catalog，不該為此連上 kernel、orchestration。`AssetStore` 也不認得 ECS，工具不建 World 也能用。
- **屬於哪一層**：都放 `engine/modules`（會長大的 Module 層）。
- **為什麼不是 Plugin**：都是我們維護、介面是我們的。
- **`default=yes`**：照「Module 預設全帶、產品可以關」。

命名：
- World 外的資產管理叫 `AssetStore`。不照 Bevy 叫 `AssetServer`：「Server」在 GYO 指 PvP 的 Match 伺服器（`target/naming.zh-Hant.md` DI 4）；不叫 `AssetManager`：那是 master 的總管物件，11-11 不採。
- id 的型別叫 `AssetId`：作者寫的、存檔裡記的、catalog 裡的那個字串（例：`char.hero`）。修訂 2 的 `AssetId<T>`（index＋generation）拿掉；store 內部的索引不對外。
- 其餘照 Bevy：`Handle<T>`。新的：`AssetRef<T>`（不持有的引用，5.17）、`AssetView`（store 裡所有活著的項目的狀態與值，在套用點拉進來）。`Assets<T>` 只是說法，讀值一律 `AssetView::Get<T>`。

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
| 驗證 | version、陣列、id／type／path 非空、全內容 id 唯一、路徑規則；C++ 與 Python 共用 valid／invalid 範例（`docs/architecture.md:301`） | build 時 | 解析時 | — | 照 master；資料規則與登記時的檢查分開（見 (3)） |

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
- **決勝理由**：F5、F6 要以 id＋型別參照、照同一組規則寫；集中一份才查得到全內容 id 唯一；JSON 可以手寫（F9）。沿用 master 讓現有內容照樣有效，是費用上的好處。

**(2) 誰產生**（AGENTS §8：手寫、Script、工具都一樣有效）
- **採**：
  - asset 函式庫提供 catalog 的讀、寫、驗證（不認得 ECS），工具照它寫，執行期照它讀。
  - **工具的登記政策歸工具**：master audio_editor 的「id 已屬別的型別就拒絕」「路徑已屬別的 id 就拒絕」（`RegistrationPolicy`）是工具的政策，不是契約的規則。
  - 產品把**內容根目錄**當 world_asset 的建立參數給（5.4；一個 Product 一個根目錄，照 master「Runtime content has one root」）。
  - 產品也可以在建立參數裡用程式加項目（F9 不想手寫時，自己掃資料夾），和 catalog 的項目一起驗（id 衝突是 catalog 的資料規則違反，啟動錯誤）。asset 不掃資料夾。
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
- **(B) 登記時的檢查**：看登記進來的 loader，不是資料本身的規則，不放進範例集；在登記當下做（5.4），啟動時登記的錯誤成為啟動錯誤，之後只有回傳值。
  1. 同一個（`type`、`format`）已有別的 loader：登記回錯誤、那個 loader 不登記（**新增**，藍圖 `:66`「副檔名衝突應為錯誤」）。
  2. loader 登記時，對 catalog 裡它的（`type`、`format`）的每個項目解讀 `settings`；解讀不了的那幾個 id 是 id 級的 E2 `BadSettings`，loader 照樣登記（**新增**）。catalog 在登記之前就讀好（world_asset 的 `Configure`），所以每個 loader 只驗一次、和 World 的順序無關。`settings` 只在這裡驗，載入時不再驗。
  3. （`type`、`format`）沒有 loader 也沒有預先登記：不在啟動時報（loader 隨時可以登記），請求時是 id 級的 E2 `NoLoader`（5.8）。
  4. 兩個 C++ 型別在型別特性裡宣告同一個型別名：登記回錯誤（**新增**；型別名綁在 C++ 型別上，5.4）。只有一個有 loader 時登記查不到，`Get<T2>` 永遠回空（第 14 節）。
  5. 預先登記與登記的衝突（**新增**；預先登記也回傳結果）：某個 loader 型別預先登記期間，別的 loader 型別登記同一（`type`、`format`）→ 登記回錯誤；預先登記時那一格已有別的 loader 登記 → 預先登記回錯誤；兩個 loader 型別先後預先登記同一格 → 第二個回錯誤。Bevy 登記時的重複檢查在 `server/loaders.rs:45-67`，預先登記的在 `:100-133`（`:132` 是 warn）。 預先登記的清單有任何一格衝突就整筆不記；同一個 loader 型別重複預先登記是冪等的；loader 登記失敗（衝突）時，它先前的預先登記與等待者照舊（`WaitingForLoader`），直到它以不衝突的形式登記。
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
- **決勝理由**：F5（UI 文件以 id＋型別參照，`UiDocumentBridge.cpp:130-170` 的四種診斷正好是 (A) 規則）、F6（audio_editor 要能照同一組規則寫）、F9（手寫的 catalog 要在啟動時就知道錯在哪）；新增的（(A)4、6、7，(B)1、2、4、5）都為了 loader 與 id 字串，放寬的兩條（`shader_bundles` 可省、`shader_bundles` 的項目規則不驗）為了支援還沒回來的 render。現有的 master 內容照樣通過是費用上的好處，不是決勝理由（修訂 3 修正版檢查 S-9、N-3）。

**交使用者的**：無。三件都沿用 master 的既有契約（不是使用者之前的決定），和別家的分歧（集中 vs 旁檔）由使用者決定 6 的「對照表」已經定了。

### 5.4 資產管理在 World 外（決定 7）

**`AssetStore`**：每個 Product 一份。world_asset 在 `Configure`（Product 層一次，O-2）建立它，存在 world_asset 這個登記單位的實例裡，活到 Product 結束。登記單位的實例本來就活到 Product 結束、可以持有自己的資料（`RegistrationUnit.hpp` 註解）。

**執行器**（實作時，2026-11-22，threads／io 重審核准後）：產品自己建 threads 的 `WorkerPool`，經產品描述的選項交給 world_asset；world_asset 把它接上 asset 自己定的 `AssetExecutor`（只有 `Submit`），不自己建池、不在 World 執行緒上當場跑。**沒有給執行器時**，需要排工作的請求是 id 級的 `Failed(NoExecutor)`（E2），不是啟動錯誤：world_asset 是 default=yes，不載資產的產品也帶著它（卡片照 L17 判）。

**設定在建立時帶**（決定 13）：內容根目錄、原始位元組雜湊的開關、在飛上限、loader 上限的覆寫，是 world_asset 的建立參數 `AssetOptions`，照 Bevy 的 `AssetPlugin { file_path, ... }`（`bevy_asset/src/lib.rs:237-248`）。

**catalog 在 world_asset 的 `Configure` 讀**：讀內容根目錄的 `content.json` 與 catalog，驗 (A) 組的規則（5.3），為每個 id 建 catalog 項目（5.5）。catalog 是內容，讀進來之後 Product 存活期間不變。錯誤先記著，在 world_asset 第一個跑到的 `Finish` 回報（`Configure` 沒有回報入口，只有 `WorldFinish::Report`，`Setup.hpp:203`）。

**型別名綁在 C++ 型別上**（修訂 3 決定 13 後檢查 B-2）：每個資產型別 `T` 在 asset 函式庫的一個編譯期特性裡宣告自己的型別名（catalog 的 `type`），由擁有型別的 Module 的函式庫宣告，和 loader 有沒有登記無關。所以 World 端只用 catalog 與這個特性，就能當場判 `TypeMismatch`，不必碰 loader 表。loader 宣告的型別名必須等於它產出的 `T` 的型別名（編譯期檢查）。

**loader 隨時可以登記**（決定 13）：
- loader 表和 info 的表用同一把讀寫鎖保護（Bevy 是 `infos`、`loaders` 兩把 `RwLock`，`server/mod.rs:72-73`；GYO 合成一把是改裝，5.5 末的表）。`Load` 在 System 裡會讀 loader 表（判 `Queued`／`WaitingForLoader`／`NoLoader`），這是 store 的鎖，不是 World 的資料（AS-11）。
- **登記在什麼時候**：任何時點都可以（照 Bevy），經**只有登記函式的介面** `AssetLoaderRegistry`（編排層的需求 O-A2 交出的是它，不是整個 store）。修訂 3 決定 13 後寫的「不能在 System 裡呼叫」撤回（理由是已撤回的「幀內不上鎖」）。登記的先後：在鎖外對 catalog 項目解讀 `settings`；然後一次拿寫鎖，查衝突、進表、寫解讀結果、轉等待者（0cad 檢查 N-10）。
- **預先登記**（決定 13：「loader 還沒到的請求先等待，照 Bevy 的待定機制」）：擁有型別的 Module 可以先宣告「（`type`、`format`）的 loader 會來」，照 Bevy 的 `preregister_asset_loader`（`lib.rs:608`、`server/loaders.rs:100-146`、`:274-287`）。
  - 場景：貼圖的 loader 要等 render 的裝置建好才能建立（SDL 那批），但遊戲初始化已經在請求貼圖。
  - 之後到的請求，info 的狀態是 `WaitingForLoader`，不排工作；loader 登記時（同一次寫鎖內）把等它的 info 照下面的「轉成 `Queued` 的共用規則」處理，放鎖後排工作。
- **轉成 `Queued` 的共用規則**（0cad 檢查 S-1）：任何把 info 轉成 `Queued` 的地方（請求的三支、loader 登記時轉等待者、`Failed` 的重判），先看 catalog 項目記的 `settings` 解讀結果；壞的就是 `Failed(BadSettings)`、不排工作。要排工作時，嘗試序號加一（含 `WaitingForLoader` 轉 `Queued`）。
  - 預先登記的鍵是**loader 的 C++ 型別**加它會認的 `format` 清單（照 Bevy `server/loaders.rs:99-108`）。那個 loader 登記時，清單裡它沒認的 `format` 的等待者才變成 id 級的 `NoLoader`；別的 loader 之後再來認那些 `format` 仍然合法（b6d3 檢查 S-9）。
  - **loader 永遠不來**：請求一直是 `WaitingForLoader`；就緒查詢另回一份「在等 loader 的 id」（5.7），讓外面看得到，不規定處理。Bevy 是永遠等（`server/loaders.rs:283-286`）。
- 沒有 loader、也沒有預先登記：id 級的 `NoLoader`（5.8）。之後登記了 loader，再請求就重判、載入（5.5）。
- **會晚登記的 loader 必須預先登記**（修訂 3 決定 13 後檢查 S-1）：依賴的「略過」只對整個 Product 存活期間都不登記、也沒預先登記的（`type`、`format`）是確定的（5.17）。這條寫進 AS-13 與第 14 節。
- **登記時的檢查**（5.3 (B)）：同一個（`type`、`format`）已有別的 loader、兩個 C++ 型別宣告同一個型別名，登記回錯誤、那個 loader 不登記；catalog 裡它的（`type`、`format`）的項目 `settings` 解讀不了的，**那幾個 id 是 id 級的 E2 `BadSettings`**、loader 照樣登記。
- **錯誤怎麼回報**：登記入口回傳錯誤。啟動時（`Configure`、`Build`）登記的一方照 catalog 的手法，記著、到自己的 `Finish` 回報成啟動錯誤；啟動之後只有回傳值。
- **Module 怎麼拿到 Product 層的 `AssetLoaderRegistry`**（只有登記與預先登記的介面；System 要在幀內登記時，擁有型別的 Module 在自己的準備函式把它放進自己的 Resource）：不是 asset 該解的，交給主會話與編排層（12.3 第 13 題）。下面是 asset 對編排層的需求，編排層可以用任何形狀滿足：

| # | 需求 | 為什麼 |
|---|---|---|
| O-A1 | 產品給的 `AssetOptions` 在 world_asset 的 `Configure` 之前就在。今天產生的單位表用預設建構（`build/cmake/GyoUnitRegistry.cmake:351`），單位的 `Configure` 先於產品自己的 `configure`（`Product.cpp:180-188`） | world_asset 在 `Configure` 讀 catalog、建 catalog 項目 |
| O-A2 | 依賴 world_asset 的單位，在自己的 `Configure`（或 `Build`）拿得到 Product 的 `AssetLoaderRegistry`（只有登記函式的介面），用來登記 loader | 5.16 |
| O-A3 | `Configure` 階段的錯誤回報（今天只有 `WorldFinish::Report`） | catalog 與登記的錯誤不必繞到 `Finish` |
| O-A4 | Product 結束時的回呼 | 丟棄件數的 E3 出口（5.8） |

- 沒有內容根目錄：catalog 是空的，每個請求都是 `UnknownId`。不是啟動錯誤。
- **擁有型別的 Module 的拆法**：照 asset／world_asset、`time`／`world_time` 的先例拆兩個單位。loader 與型別特性的函式庫只依賴 asset（F4 的工具只連它）；World 端的單位依賴 world_asset，在登記檔寫明。

**World 端**：world_asset 在每個 World 的 `Build` 登記兩個 Resource 的型別，值在它的準備函式裡放進去（`Build` 時還沒有 World）：
- `AssetRequests`：請求入口；
- `AssetView`：store 裡**所有活著的項目**的狀態與值（照 Bevy：`Assets<A>` 本來就裝全部，不分誰請求的），在套用點從 store 拉進來（5.6）。`State`、`Ready`、`SourceDigest`、讀值 `Get<T>(handle)`、失敗的列舉都讀它。

所以 `CarryOver` 帶來的 Handle、Extract 進來的 Handle、依賴的 `AssetRef`，只要項目活著，在任何 World 的 `AssetView` 都讀得到，不必先「認領」（b6d3 檢查 B-1）。

**「作者不寫 `AssetView`」分兩層守**（照 M10 的寫法；b6d3 檢查 S-8）：成員層級的寫，用負向編譯樣本守：`AssetView` 對外沒有可變操作；可移動、不可複製，建構子私有，作者造不出一個來指派（Kernel 的 `SetResource` 要移動指派，`World.hpp:263-270`，所以不刪移動）；整個換掉、拿掉、搬走（`Commands::SetResource` 等，`Params.hpp:535`、`:545`），靠審查，套用 System 發現 `AssetView` 不在就重建；「上次拉的版本號」存在 `AssetView` 裡，被整個換掉的就對不上、會重拉——所以會自癒（0cad 檢查 S-9）。第 4 批合併後 Kernel 不數寫者（`-authority-no-writer-count`），這兩個 Resource 不登記寫入政策。

System 不持指標；元件裡只放 `Handle<T>`、`AssetRef<T>` 或 `AssetId`（AS-1）。`Handle<T>` 的相等以 id 比（救回之後核心換了，id 不變；第 4 批回滾的比較預設用 `==`）。

**換 World、多 World 不是 asset 的事**：
- Handle 的引用計數跨 World 有效；舊 World 銷毀時它的 Handle 才放掉。asset 的 Handle 指向 store，不指向舊 World，所以放進 `CarryOver` 的 Resource 不違反 O-10 防的「指向舊 World 的句柄」。
- `BuildNewFirst`：新 World 的準備函式、`beforeSwap` 在舊 World 還在時跑，請求同樣的 id 一定命中，**不重讀**。
- `DropOldFirst`：舊 World 先銷毀（`Product.cpp:646`），之後 world_asset 的準備函式先跑一次維護（5.6），舊 World 的「丟了」在那時就處理掉，所以**沒有別的 World 持有的就重讀**（確定的行為；b6d3 檢查 S-7）。作者要不重讀，就把 Handle 放進自己標 `CarryOver` 的 Resource（O-10）。
- 回主選單另建 Product（O-3）：舊 Product 的 store 隨它結束；新 Product 重新載入（RC-AS3）。
- 回滾的歷史若存著元件裡的 Handle 副本，資產要等歷史修剪後才釋放（第 14 節）。

### 5.5 請求、Handle 的生命週期（照 Bevy 的演算法）

**這一節照 Bevy `bevy_asset`**（決定 14；第 24、25 節）。每一處和 Bevy 的對應，以及 GYO 改裝的地方，列在本節末的表。

**store 的表**：每個 catalog id 一筆 **catalog 項目**（world_asset 的 `Configure` 讀 catalog 時建，永久；另記生命序號、跨生命遞增的嘗試序號、loader 登記時算出的 settings 解讀結果）。catalog 項目底下，**活著時才有一筆 info**（照 Bevy 的 `AssetInfo`：info 存在＝活著；Bevy 處理 drop 時 `entry.remove()`，`server/info.rs:782`）。info 記：生命序號、對強核心的弱參照、「待略過」計數、狀態、值（`shared_ptr<const T>`）、失敗、原始位元組雜湊、依賴邊。catalog 項目記下一個生命序號（每次 info 被移除就加一，對應 Bevy 的 `AssetIndex` generation，定義在 `assets.rs:18-41`，以 `contains_key` 判在 `server/info.rs:470`、`:691`）。**整張表由 store 的一把讀寫鎖保護**（照 Bevy 的 `AssetInfos` 與 `RwLock`，`server/mod.rs:72-73`）：套用點拉視圖拿讀鎖；請求、完成、維護、登記拿寫鎖。

**Handle**：強核心是原子計數的共享物件（同 Bevy 的 `Arc<StrongHandle>`）。核心共有 store 的「丟了」佇列（不是 store 那把鎖保護的東西；佇列本身無鎖或用自己的小鎖），最後一個放掉時推（id、生命序號）（照 Bevy `handle.rs:84-96` 的 drop channel）。複製 Handle＝原子加一。**一段生命可以先後有多個核心**（救回時換新核心）。

**請求** `AssetRequests::Load<T>(AssetId)`：
1. **請求級的檢查**（不拿鎖）：id 不在 catalog（`UnknownId`）、`T` 的型別名和項目的 `type` 不合（`TypeMismatch`）。回一個帶失敗、**不帶核心**的 Handle，放掉時不推「丟了」，不碰任何 info。這是 GYO 的改裝：Bevy 的去重鍵含型別，型別不合會建一筆正常的 info、之後載入失敗；路徑不合法時回 `default_for_type` 的 Handle（`server/mod.rs:385-389`）。GYO 的去重鍵只有 id，所以請求級要和 id 級分開。
2. **拿 store 的鎖**，照 Bevy `get_or_create_path_handle`（`server/info.rs:274-310`），三支窮舉：
   - **沒有 info**：建 info（新生命序號）、新核心。catalog 項目記著 `BadSettings` 就是 `Failed(BadSettings)`；沒有 loader 時，有預先登記就 `WaitingForLoader`，沒有就 `Failed(NoLoader)`；其餘是 `Queued`、嘗試序號加一、要排工作（記下，放鎖後才交給執行器）。
   - **有 info、弱參照升級得了**：回共有同一個核心的新 Handle（命中）。
   - **有 info、升級不了**（最後的 Handle 剛放掉、「丟了」還沒處理）：新核心、**「待略過」加一**（照 Bevy `:303-304`），值留著（救回）。
   - 後兩支共用（照 Bevy：`should_load` 在升級判斷之前算，`:281-288`）：狀態是 `Failed` 時重判——`BadSettings` 留在 `Failed`；`NoLoader` 看現在有沒有 loader 或預先登記，轉成 `Queued`、`WaitingForLoader` 或留著；其餘改回 `Queued`、嘗試序號加一、要排工作。
3. 放開鎖；把記下的工作交給執行器。回 Handle（帶這段生命的序號，S-5 用）。Bevy 只有單執行緒設定才先放鎖，多執行緒是持鎖 spawn、在同一把鎖下記 `pending_tasks`（`server/mod.rs:413-419`、`:437-446`）；GYO 一律放鎖後交，讓「當場跑完」的執行器不自鎖——這是改裝。

`Load` 的回傳不帶「命中與否」，Handle 與 `AssetRequests` 也沒有直接讀 store 當下狀態的入口；讀狀態一律經 `AssetView`（AS-11 的條件，b6d3 檢查 S-5）。

**維護處理「丟了」**（拿同一把鎖，照 Bevy `track_assets`／`process_handle_drop`，`assets.rs:570-587`、`server/info.rs:758-800`）：把佇列處理到空（含這次處理中新推入的）。每筆：
- 沒有 info，或 info 的生命序號不同：略過（過期的事件）；
- 「待略過」> 0：減一，什麼都不做；
- 否則：移除 info（catalog 項目的生命序號加一），值的共享指標移出、放鎖之後才放掉（大物件的解構不擋別人的 `Load`；b6d3 檢查 S-1）。

**工作完成**（工作執行緒）：工作執行緒拿寫鎖，若 info 還在且生命序號相同，就寫入值或失敗、改狀態、加依賴邊並對依賴走一次「請求」的三支（記下要排的工作）、重算受影響的強連通分量（5.17）、把在飛的名額交給下一筆 `Queued`；否則算取消。放鎖後交出記下的工作、放掉被換下的大物件。Bevy 把結果送到 `PreUpdate` 是因為要 `&mut World` 才能放進 `Assets<A>`（`server/mod.rs:1890`）；GYO 的值在 World 外，不需要這個延後。

**在飛上限與名額**：`Queued` 照請求的先後排隊；工作執行緒開工前先拿鎖看 info 的生命序號，對不上（還沒開工就被移除）就不讀檔、直接把名額交給下一筆。開工後狀態改 `Loading`。被取消的工作做完才放名額（工作是純函式，不中途停）。

**依賴何時排**：父項解碼完、在「工作完成」裡才排（loader 是純函式，`LoadContext` 只記引用清單）。Bevy 是父項的 loader 執行中就啟動（`loader.rs:728-735`）。所以 23.7 MB 的 fbx 解完才開始讀它的貼圖，父項解碼失敗時依賴不載——這是改裝的代價，列在 5.5 末的表。

**為什麼正確**：「有沒有人持有」（弱參照升級）、「丟了要不要移除」（待略過、生命序號）、「結果採不採用」（生命序號）都在同一把鎖下判定。Bevy 的原始碼註解寫明這一點（`assets.rs:571-574`）。

**Handle 比 store 活得久**：store 關閉後，「丟了」的推送是空操作。`NotRequested` 只出現在 store 的測試讀法；`AssetView::State(handle)` 照 5.7 的規則回（有核心而視圖沒有就是 `Queued`）。

**值**：不可變、共享唯讀（AS-10）。`AssetView::Get<T>(handle)` 回 `const T*`（沒載好回空），**只在這次 System 執行期間有效**；以 C++ 型別核對，不合回空。

**原始位元組雜湊**（決定 10）：工作執行緒讀檔時對讀到的位元組算 SHA-256（`engine/base` 的 `Sha256`），每個 id 一個，只算自己的檔、不含依賴。預設開；`AssetOptions` 可關，關時回「沒算」。`AssetView::SourceDigest(handle)` 讀。比不比、不同時怎麼辦，歸作者或網路 Module。**限界**：只代表檔的位元組，catalog 項目的 `format`、`settings` 不同時值會不同（第 14 節）。介面不綁「讀檔時算」。成本在實作時量測，記進 dev_log。

**和 Bevy 的對應**（AGENTS §2：相似點、照用的、改裝的；b6d3 檢查 S-11）：

| 面向 | Bevy | GYO | 照用或改裝 |
|---|---|---|---|
| 表與鎖 | `AssetInfos`、`loaders` 各一把 `RwLock` | info 與 loader 表合成一把讀寫鎖 | **改裝**：登記與請求都要同時看兩者，一把較單純；代價是 loader 登記會短暫擋 `Load` |
| 活著 | info 存在 | catalog 項目底下有 info | 照用（catalog 項目是 GYO 多的一層：id 來自 catalog） |
| 生命 | `AssetIndex` 的 generation | catalog 項目的生命序號 | 照用 |
| Handle | `Arc<StrongHandle>`＋drop channel | 強核心＋「丟了」佇列 | 照用 |
| 救回 | `handle_drops_to_skip` | 「待略過」 | 照用 |
| `Failed` 再請求 | `should_load` 在升級前算 | 同 | 照用；`BadSettings`、`NoLoader` 的重判是 GYO 多的 |
| 請求級失敗的 Handle | 型別不合建正常 info；路徑不合法回 `default_for_type`（`server/mod.rs:385-389`） | 不帶核心、不碰 info | **改裝**：GYO 的去重鍵只有 id |
| 結果進表 | channel 送到 `PreUpdate`（要 `&mut World`） | 工作執行緒拿鎖直接寫 | **改裝**：值在 World 外，不需要延後 |
| 交工作給執行器 | 多執行緒持鎖 spawn、記 `pending_tasks`；單執行緒先放鎖（`server/mod.rs:413-419`） | 一律放鎖後交 | **改裝**：讓「當場跑完」的執行器不自鎖 |
| 依賴何時排 | 父項的 loader 執行中（`loader.rs:728-735`） | 父項解碼完才排 | **改裝**：loader 是純函式；代價是依賴晚一步、父項失敗時依賴不載 |
| 依賴失敗後 | 清空等待名單，之後不回復（`server/info.rs:708-709`） | 「連同依賴」每次拉視圖由邊重新推導，重載成功後回到 `Complete` | **改裝**：輪詢下作者要看得到恢復 |
| 在飛上限 | 沒有 | `AssetOptions` 的上限，`Queued` 照先後、開工前比生命序號 | **新加**：F8 靠它管記憶體 |
| 處理 drop | main World 的 `PreUpdate` | 每個 World 的套用點、world_asset 的準備函式、`Pump()` | **改裝**：GYO 有多個 World 與不建 World 的工具；後果是別的 World 的維護可能在本 World 幀中途移除歸零的項目（5.6） |
| 讀值 | `Res<Assets<A>>`，同一個 World | 每個 World 的 `AssetView`，套用點拉全部活著的 | **改裝**：store 在 World 外，一幀一致靠套用點 |
| 取消 | 丟掉 Task（`server/info.rs:778`） | 開工前被移除就不做；開工後做完再丟（生命序號不同） | **改裝**：工作是純函式，不中途取消 |
| 等 loader | 工作裡 `recv().await`（`server/loaders.rs:283-286`） | info 的狀態 `WaitingForLoader` | **改裝**：讓就緒查詢看得到 |
| 預先登記的鍵 | loader 型別（`server/loaders.rs:99-108`）；loader 到時沿用 reserve 時的副檔名清單（`:37-41`、`:76-92`） | loader 的 C++ 型別＋它會認的 `format` 清單；loader 到時，清單裡它沒認的 `format` 的等待者變 `NoLoader`；預先登記期間，別的 loader 型別來登記同一（`type`、`format`）是登記錯誤 | 鍵照用；**「沒認的變 `NoLoader`」與衝突規則是 GYO 才有** |

### 5.6 執行緒、維護、套用點

**store 的工作**（World 執行緒之外）：讀檔＋解碼，在交換點或 runner 給的執行器上跑（X-1）。同時在飛的工作數有上限（`AssetOptions`）。讀檔與解碼不持 store 的鎖。

**維護**（只處理「丟了」佇列，5.5）：拿 store 的鎖跑。誰跑：
- 每個 World 的套用 System（照 Bevy 的 `track_assets` 在 `PreUpdate`，`lib.rs:687-688`）；
- world_asset 的準備函式（World 還沒開始跑幀時）；
- 不建 World 的使用者呼叫 `AssetStore::Pump()`（b6d3 檢查 S-2）。
多個 World 同時跑維護是安全的：都在同一把鎖下，處理到空；後到的看到空佇列就直接結束。

**套用點**（預設 `Main`/`First`）：world_asset 的套用 System
1. 跑一次維護；
2. 拿 store 的讀鎖：store 的版本號（任何 info 改變就加一）和這個 World 上次拉的相同就跳過；不同就建一份新的 `AssetView` 內容——**所有活著的 info** 的（id、生命序號）、狀態、值的共享指標、失敗內容、雜湊、嘗試序號、依賴邊、「連同依賴」的狀態、略過與循環標記；
3. 放開鎖。

之後這一幀的 System 只讀 `AssetView`，不碰 store。
- **一幀之內看到同一份**：`AssetView` 只在套用點與準備函式換，同一幀的重跑步與新步看到同一份（回滾的重跑在 `PreUpdate`，第 4 批 5.2）。重跑步裡呼叫 `Load`，元件的 Handle 被還原時原核心可能已死，所以是命中或救回；本 World 的維護只在 `First` 跑。**但別的 World 的套用點、準備函式或 `Pump()` 可能在本 World 的幀中途跑維護、移除歸零的項目**（預設 `RunFrame` 逐 World 串行，`Product.cpp:536-539`，不會發生；自訂 runner 交錯 schedule，或第 5 批讓 World 並行時才會），那時 `Load` 得到新生命、重讀（Bevy 只有一個 World 跑 `track_assets`，同一幀放掉再載一定救回；GYO 不保證）。作者指引：判定用內容的 Handle 要一直持有，放在不會被回滾倒回的地方（第 14 節）。
- **新 World 的起點**：world_asset 的準備函式跑一次維護、拉一次視圖。之後的準備函式、`beforeSwap`、遊戲初始化裡命中的、**已經載好的**值當場讀得到（G-3）；這些時點裡新送出的請求，下一個套用點才進視圖。
- 預設骨架關掉的 World，產品自己掛套用 System（O-4）。
- **成本**：版本號不同時，對全部活著的 info 拉一次。BigMap 有數百到數千個 region 時要量測；`Load` 等鎖、套用點持鎖的時間也列進 3.3 延後的預算項（b6d3 檢查 N-9）。超出時改成只拉變了的。

**不建 World 的使用者**（F4、F6 的工具，b6d3 檢查 S-2）：`AssetStore` 給 `Request(id) -> Handle`、`Pump()`（處理「丟了」）、`Read<T>(handle)`（拿鎖讀值的共享指標）。工具也可以完全不經 store，直接呼叫 loader（master 的 model_viewer 就是這樣）。

**鎖**（AS-11）：
- World 的資料不上鎖（D6b、M11 的原意）。
- System 呼叫 store（`Load`、套用點）時拿 store 自己的鎖；持鎖只做表的操作（查、改欄位、加依賴邊、重算受影響的強連通分量、建新的視圖內容），不讀檔、不解碼、不呼叫 loader 作者的函式、不交工作給執行器、不解構大物件。強連通分量與建視圖的成本列進 3.3 的預算項。
- Handle 的複製與放掉是原子操作；「丟了」佇列不用 store 的鎖。
- loader 登記拿同一把寫鎖；可以在任何時點呼叫（照 Bevy）。對 catalog 項目解讀 `settings` 在鎖外做（呼叫 loader 作者的函式），只把結果寫進 catalog 項目時拿鎖。

**給交換點、threads 那批的需求**：

| # | 需求 | 為什麼 |
|---|---|---|
| X-1 | 給 store 一個執行器：讀檔與解碼在 World 執行緒之外跑，不碰 World（M11）；執行器停了之後提交是安全的空操作；執行器的介面由 asset 用自己的話定（asset 不依賴 threads），world_asset 接上這裡給的 | F1、F2：23.7 MB 的 fbx 解碼不能卡住 World |
| X-2 | 交換點不需要認得 asset 的型別或語意；World 靠自己的套用點看到結果（RC-AS5、RC-O8） | 5.6 |
| X-3 | 若交換點宣告定了「每幀的邊界」，套用 System 掛在它之後 | 同一幀看到同一份 |
| X-4 | 執行緒的數量與歸屬由交換點或 runner 定 | O-3 |
| X-5 | 測試能**分開控制**每筆工作何時開工、何時完成（含「當場全部跑完」，用佇列式、不遞迴；交工作在放鎖之後，所以不會自鎖）；或測試用慢的 loader 的閂加真執行緒 | 等價測試（第 10 節）；只跑一次的 runner |
| X-6 | Product 結束時，未完成的工作能停下或等完 | 件數（5.8） |

### 5.7 狀態、就緒查詢（輪詢，照 Unity、Godot）

**項目的狀態**：`NotRequested`、`WaitingForLoader`、`Queued`、`Loading`、`Loaded`、`Failed`（帶 `AssetLoadFailure`）。`AssetView::State(handle)` 回這個 Handle 在這一幀看到的：請求級的失敗回那個失敗；**Handle 有核心、而視圖裡沒有相同的（id、生命序號）一律回 `Queued`**（本幀剛送出的、舊視圖配上新生命的、經 Extract 或 `CarryOver` 拿到但建在本 World 上次拉取之後的，都是這一條；1622 檢查 S-5）。`Failed` 之後再請求，那一幀視圖仍是上一次的 `Failed`，下一個套用點才變 `Queued`。

**就緒查詢**（決定 9）：`AssetView::Ready(一組 Handle)` 回：
- `AllFinished`：每一筆都結束了（`Loaded` 或失敗；`WaitingForLoader`、照上一條回 `Queued` 的不算結束）；
- `Complete`：每一筆都 `Loaded`，而且「連同依賴」也 `Complete`（5.17）；
- `DependenciesFinished`：連同依賴都結束了（含失敗）；
- `Loaded` 筆數、`Failed` 筆數與清單（含依賴的失敗，帶父項與這一組裡的哪個 Handle）、略過的依賴、在等 loader 的 id、整串的 id 清單。

`AssetView::Unfinished(一組 Handle)` 回其中還沒結束的筆數。

讀取畫面、淡出、等哪些才開始 tick，是遊戲設計，**歸作者**（決定 9）。單筆到了就看得到，所以「先看到地面才看到樹」要由作者用就緒查詢擋住。

**告知用輪詢**：作者與日誌照 Unity（`GetObjectLoadingStatus`）、Godot（`load_threaded_get_status`）的做法，每幀看狀態。
- **日誌怎麼知道新的失敗**（b6d3 檢查 S-6）：`AssetView::Failures()` 列舉所有活著、狀態是 `Failed` 的項目，每筆帶嘗試序號（每次排工作加一，跨生命遞增）與失敗碼。日誌記下自己看過的（id、嘗試序號、失敗碼），比一比就知道哪些是新的、哪些是重載後又失敗、哪些換了原因。這是項目的狀態欄位，不是變更清單。
- 修訂 3 起的 `AssetChanges` 撤回（第 24 節）。不用 Kernel 的 `Message`，第 4 批 `Summary` 雜湊訊息序號的問題不碰到 asset（仍報給主會話，第 15 節）。

### 5.8 錯誤（照已合併的〈Error levels〉）

| 情況 | 級別 | 怎麼出現 |
|---|---|---|
| **請求級**：id 不在 catalog（`UnknownId`）、型別名不合（`TypeMismatch`） | **E2** | 那個 Handle 帶失敗，`State` 讀得到。不碰項目 |
| **id 級**：沒有 loader 也沒有預先登記（`NoLoader`）、`settings` 解讀不了（`BadSettings`，loader 登記時判）、檔案不在（`NotFound`）、讀不到（`ReadFailed`）、超過上限（`TooLarge`）、loader 解碼失敗 | **E2** | 項目是 `Failed`，帶 `AssetLoadFailure{code, message, detail}`（`Engine::Base::Error<Code>` 的形狀，沒有鏈）。再請求照 5.5 重判：`BadSettings` 留在 `Failed`；`NoLoader` 看現在有沒有 loader 或預先登記；其餘重載 |
| loader 丟例外（含 `std::bad_alloc`；`catch (...)`） | **E2**（`LoaderThrew`） | 同上；`detail` 寫 `what()` |
| **依賴的失敗**：`BadReference`、`UnlistedReference`、`AmbiguousReference`、引用的 id 不在 catalog（`UnknownId`）或型別名不合（`TypeMismatch`）、循環（`CyclicReference`）、依賴本身失敗（5.17 的先後） | **E2** | 父項自己的狀態不變；「連同依賴」的狀態是 `Failed`，帶 `DependencyFailure{parent, reference, code}` |
| catalog 不合資料規則 | 啟動錯誤 | world_asset 第一個跑到的 `Finish` 回報，收進 `ProductStartupReport`（O-5） |
| loader 登記的檢查不過 | 登記回錯誤 | 那個 loader 不登記；啟動時登記的一方記著、到自己的 `Finish` 回報 |
| 內部不一致（例：維護時表與核心對不上）；同一條執行緒重入 store 的鎖（鎖記著獨占持有者的執行緒 id，一律檢查） | **E0** | `GYO_ASSERT` |

**件數**（AS-8；第 10 節照它突き合わせ）：
- **請求**：請求 ＝ 命中 ＋ 救回 ＋ 新的一段生命 ＋ 請求級失敗。命中與救回再分「是否觸發重載」；新的一段生命再分：排工作／等 loader／`NoLoader`／`BadSettings`（後三者不排工作）。回滾重跑步裡的 `Load` 照樣計入，通常屬命中或救回；別的 World 在幀中途跑過維護時可能是新的一段生命（5.6）。
- **工作**（讀檔＋解碼）：排入的工作 ＝ 發佈成 `Loaded` ＋ 排過工作後的 id 級失敗 ＋ 取消 ＋ 丟棄（Product 結束時還沒完成）。在沒有工作在飛時成立。
- **丟棄的出口**：Product 結束時 World 先銷毀、登記單位沒有結束的回呼，E3 記錄沒有可寫的 World。store 給明確的關閉步驟（停工作、清佇列、記丟棄件數），測試在關閉之後、銷毀之前讀；E3 的出口要編排層的結束回呼（O-A4）。

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
| `AssetRequests` | **什麼都不寫** | 只是請求入口，沒有要比的內容 |
| `AssetView` | **什麼都不寫** | 依完成時序變；告知用輪詢、不用 Kernel 的 `Message`，所以沒有序號與游標進摘要（5.7） |
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

- 從 World 以外的執行緒讀整個檔，帶位元組上限：asset 的 `ReadWholeFile`（走 `NativeFileUri`、`NativeFileSystem::Open`、`IStream`）。threads／io 重審（使用者 2026-11-22 核准）在 io 的錯誤碼加 `TooLarge` 之後，可改用 `ReadAllBytes`，那批合併後再評估。
- 內容根目錄是原生檔案系統上的一個目錄，由 `AssetOptions` 給；catalog 的路徑是根目錄下的相對路徑（5.3）。
- io 的 `Vfs`、`MountTable`、`MountPoint` 已由 threads／io 重審拿掉（使用者 2026-11-22 核准）；asset 本來就不靠它們（事先轉檔改 catalog 項目，5.10）。
- 檔案監看（`IFileWatcher`）留給熱重載，本批不用。

### 5.13 給生成世界（重做）那一批的需求

| # | 需求 | 為什麼 |
|---|---|---|
| G-1 | 生成世界分批跑時，每一批之間 world_asset 的套用 System 會跑（由誰掛、掛在哪由那批定） | 作者等「全部完成」才離開；視圖不換就永遠等不到 |
| G-2 | 結束條件由作者的 System 給，可以是就緒查詢「全部好了」，也可以是作者自己的條件 | `-build-world-phase-is-mechanism` |
| G-3 | `beforeSwap` 同步連跑（`Product.cpp:740-749`）、期間沒有套用點：裡面命中已載好的值，world_asset 的準備函式拉過一次視圖，當場讀得到；還沒載好的，在替換之後的套用點才看到。寫明，不要求改 | 5.4、5.6 |

### 5.14 和讀檔、存檔版本 Module 的關係

**讀檔**＝替換 World（讀檔宣告修訂 11，核准版）。asset 的部分：
1. **先驗證**：作者在現在的 World 請求存檔裡記的 id，等到全部結束。有 `Failed`（含 `UnknownId`：內容改版後某個 id 不在了）就不發替換，回 `Result` 給玩家（O-8）。
2. **再替換**：新 World 請求同樣的 id，命中（5.4）。
3. **存檔裡記 id**（AS-6）。

**存檔不是 asset**：存檔的格式是作者的；讀存檔檔案用 io 或作者自己的方式。**存檔版本 Module** 比對存檔的版本標記，和 asset 的內容無關；內容改版造成 id 消失由第 1 步發現。catalog 的 Version 是 catalog 格式的版本，不是內容的版本。

### 5.15 藍圖的資產不變量 A1～A6

| # | 藍圖的內容 | 本頁 | 理由 |
|---|---|---|---|
| A1 | 每型別一個 store＋每型別一份事件 | **留**，形狀改：一個 `AssetStore` 一張表（項目帶型別），World 端一份 `AssetView`、以型別讀；告知用輪詢（5.7） | — |
| A2 | id 只在行程內有效，path（或 Uuid）才是契約 | **改**：契約的鍵是 catalog 的 id 字串（決定 6）；store 內部索引只在行程內 | — |
| A3 | 非同步完成序列化到單一套用點 | **改**：結果由工作執行緒在 store 的鎖下寫進表（照 Bevy 的判定、拿掉 Bevy 為了 `&mut World` 的延後）；World 看到結果只在自己的套用點 | 順序：完成了就發佈（決定 9） |
| A4 | 釋放兩段式（`Unused`→`Removed`） | **本批一段** | Bevy 兩段的實際用途是渲染：只給渲染用的 asset 用 `get_mut_untracked` 與 `take_gpu_data` 把資料**就地取走**、main World 只留中繼資料（`bevy_render/src/render_asset.rs:351-353`、`:96`），其餘整份 `clone`（`:364`）；GPU 那份等最後一個 Handle 消失（`Unused`）才移除（`:332-333`）。AS-10 禁止就地取走，所以渲染那批要丟 CPU 副本時，路是「第二段釋放：移除值、Handle 還在」→ RC-AS7。AS-10 的共享唯讀也省掉 Bevy 的 `clone` |
| A5 | 依賴圖由 core 擁有、loader 只宣告 | **留**（決定 11）：依賴圖在 store，loader 經 `LoadContext` 宣告引用（5.17） | 修訂 2 的「延後」翻掉 |
| A6 | 格式解析器與執行期解讀分層 | **留**：asset（library）與 world_asset 的拆法；loader 歸擁有型別的 Module | — |
| （:66）| catalog 邏輯 id、StrictJson、Result、ready 前全載；副檔名衝突應為錯誤 | catalog 邏輯 id：**採**（5.3）；（`type`、`format`）的衝突是登記錯誤（5.3 (B)）；StrictJson 歸 JSON Module；Result：留；ready 前全載：歸作者（12.2 第 3 題）； | — |

### 5.16 loader 的介面與登記

**介面**（在 `asset` 函式庫；名字實作時照 N15 定）：
- **一個 loader 宣告**：產出的型別 `T`（型別名來自 `T` 的型別特性，loader 宣告的必須相同、編譯期檢查；5.4）；它認的一個或多個 `format`（文字 loader 認 `txt`、`json`，所以 master 的 20 筆 text＋`.json` 載得了；修訂 3 修正版檢查 S-8）；`Settings` 與從 catalog 的 JSON 解讀它的函式；預設的位元組上限；錯誤碼。
- **載入函式**：`Load(bytes, settings, LoadContext&) -> Result<T, LoaderFailure<Code>>`。
  - `LoadContext` 有：這筆的 id 與路徑（給錯誤訊息）、上限、宣告引用的入口（5.17）。
  - 沒有 World、沒有時鐘、沒有全域可變狀態；同樣的位元組與 settings 得到同樣的值與同樣的引用清單（AS-3）。
  - 產出的 `T` 要合 AS-10 (c)。

**系譜**：master 的 `IAssetLoader::Load(ConstSpan<byte>, LoadContext)`，去掉 `AnyAsset` 的型別抹除；引用的入口照 Bevy `LoadContext::load`（`loader.rs:735`），但回的是不持有的 `AssetRef`（5.17）。Bevy 的 `Reader`＋async 不採：串流沒有 Feature（決定 8）。

**整檔進記憶體**：解碼前整檔讀進來，帶上限；不做串流的 loader（最大的內容 fbx 23.7 MB、region ≤ 16 MiB 在上限內）。

**登記**：隨時可以登記到 store 的 loader 表（帶鎖），也可以先預先登記（5.4）。同一個 loader（同一個 C++ 型別）重複登記是冪等的；同一個（`type`、`format`）兩個不同的 loader，登記回錯誤。Module 怎麼拿到 `AssetLoaderRegistry`，待編排層的 Product 層 Resource（12.3 第 13 題；需求 O-A1～O-A4 在 5.4）。

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
3. `U` 的型別名（型別特性，5.4）和項目的 `type` 相合（否則 `TypeMismatch`）；沒有 loader 也判得了；
4. 這個 Product 有那個（`type`、`format`）的 loader，或有預先登記（照一般請求等它，`WaitingForLoader`）；兩者都沒有就**略過**（不載、不算失敗）。看的是（`type`、`format`），不只 `type`。略過和循環一樣是單調的：直到父項那段生命被移除都不重新持有。所以**會晚登記的 loader 必須預先登記**（5.4）；略過只對整個 Product 存活期間都不登記、也沒預先登記的（`type`、`format`）是確定的，最後的 store 內容才只依內容而定。

1～3 的失敗是**邊級的依賴失敗**，帶 `DependencyFailure{parent, reference, code}`。

在 store 裡，通過 1～4 的引用立刻持有、需要時排工作；直接呼叫 loader（F4）時只回引用清單。

**所有權**：
- **持有依賴的是 store 的依賴邊，不是值**。父項的值裡放的是 `AssetRef`，不加計數；值可以不可變（AS-10），store 也不必認得 `T` 的內部。
- 父項（那段生命）還活著時，它的每條依賴邊對依賴加一個持有；父項被移除時，它的邊一起放掉，依賴照一般的引用計數走（5.5）。
- 作者要自己持有某個依賴，就用 `AssetRequests::Load(ref)` 拿一個 Handle（一般的請求）。`AssetRef` 本身不持有。
- 去重照 id：兩個模型引用同一張貼圖，只讀一次。

**經 `AssetRef` 讀值的契約**（修訂 3 修正版檢查 S-7 (c)）：`AssetView::Get<U>(ref)` 在同一幀的 `AssetView` 裡，父項的「連同依賴」是 `Complete` 時，非循環、非略過的依賴**一定**讀得到。其他時候（還沒完成、循環的邊、略過的、父項已被移除）可能回空。

**連同依賴的狀態**：
- `Incomplete`：自己或某個遞迴的依賴還沒結束；
- `Complete`：自己與遞迴的全部依賴都 `Loaded` 或略過；
- `Failed`：帶 `DependencyFailure` 的清單。

父項自己的狀態不因依賴失敗而變（照 Bevy：自己 `Loaded`、連同依賴 `Failed`，`server/info.rs:661-716`）。作者可以決定沒有某張貼圖也照用。「連同依賴」的狀態每次拉視圖由依賴邊重新推導，所以失敗的依賴之後被重載成功，父項會回到 `Complete`（GYO 的選擇；Bevy 不回復，見下面「告知」）。

**略過的例子**：無畫面的 Match 沒有貼圖 loader，模型照樣 `Complete`，也不會去載用不到的貼圖。

**循環引用**（和完成順序無關）：
- 工作執行緒在「工作完成」裡（寫鎖下）每加入一個項目的依賴邊，就對受影響的邊重算強連通分量。
- 同一個強連通分量裡的邊（含自我引用 A→A）**全部**標 `CyclicReference`，都不持有；之前持有的一起放掉。分量裡每個成員的「連同依賴」狀態都是 `Failed`。
- **標記是單調的**（修訂 3 修正版檢查 S-7 (a)）：一條邊被標成循環之後，直到它的父項那段生命被移除，都不會因為重算而重新持有。這樣不會出現「放掉→不成環→重新持有→重讀→又成環」的反覆。
- 最後的依賴圖只依內容而定，所以結果和 A、B 誰先解完無關（AS-5）。
- Bevy 沒有處理循環（第 11 節），這是 GYO 的差異。

**告知**：用輪詢。`Ready(C)` 的失敗清單含遞迴祖先：C→A→X、X 失敗時，`Ready(C)` 列出一筆（帶父項 A）。「連同依賴」由邊每次拉視圖重新推導，X 之後被重載成功，`Ready(C)` 會回到 `Complete`——這是和 Bevy 的差異：Bevy 失敗時清空等待名單（`server/info.rs:708-709`），之後不回復（5.5 末表、第 11 節）。

**雜湊**：原始位元組雜湊每個 id 各自一個（5.5）。要比整串，作者用就緒查詢給的 id 清單逐一比。

**件數**：引用 ＝ 命中 ＋ 救回 ＋ 新的一段生命（排工作／等 loader／`BadSettings`）＋ 略過 ＋ 邊級失敗 ＋ 循環。各類互斥：一條引用在加邊時歸一類；之後被標成循環的邊，從原本那一類移到「循環」，它已觸發的工作照一般工作計入 5.8 的工作等式。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature 需要**：F1～F6、F8、F9；12.1 的決定 1～14（決定 4 作廢）。
2. **現在的問題**：`gyo2` 沒有 asset；舊設計（target `assets` DI 1～3）建立在已拿掉的 (c) 區、Loading 的 Ingress 與「資產放在使用它的 World」上；master 的 `AssetManager` 是總管物件、在主執行緒輪詢、貼圖綁 SDL3_image；master 的 catalog 是 owner 未定的共用契約。
3. **變化的邊界**：
   - 新 Subsystem：`engine/modules/asset`、`engine/modules/world_asset`。
   - **新 Data Contract：catalog**（格式、Version、Validation Rule 歸 asset）。
   - **推翻 11-11 方向 2**（資產放在使用它的 World）：資產管理改在 World 外，Product 一份（決定 7）。保留三類拆法與「System 不持指標」。
   - target 文件：舊 `target/assets.zh-Hant.md` 的 DI 1～3 與 RC，由新頁取代。
   - `engine/modules/image`、`third_party/stb_image` 在 SDL 那批，不在本批。
4. **影響**：
   - 編排層：登記兩個單位；設定是 world_asset 的建立參數；Module 拿到 `AssetLoaderRegistry` 的路要編排層的「Product 層 Resource」（12.3 第 13 題，交主會話與編排層定）。
   - Kernel：告知不用 `Message`（第 4 批的 `Summary` 雜湊訊息序號與游標，5.7）；這對其他依完成時序發訊息的 Module 也成立，報給主會話（第 15 節）。
   - 交換點、threads：5.6 的需求（比修訂 2 少：不經交換點搬值）。生成世界：5.13。
   - 第 4 批：`Summary` 的雜湊（5.9.1）。
   - 日誌與 `world_logging`：輪詢 `AssetView::Failures()`；丟棄件數的 E3 出口要編排層的結束回呼（第 15 節）。
   - 之後的 render、text、ui、audio、model Module：登記 loader；它們的值要合 AS-10。
   - 產品：載入改經 id 與 catalog。工具：照 catalog 的 Data Contract 寫（audio_editor、ui_editor），或直接呼叫 loader（model_viewer）。
5. **Dependency Direction**：asset → base、private io、private json；world_asset → asset、base、kernel、orchestration、threads（實作時加，只為 `WorkerPoolExecutor`；第 15 節第 13 項）；日後的 image → asset。擁有型別的 Module 拆兩個單位：loader 函式庫 → asset；World 端單位 → world_asset（登記檔寫明，5.4）。全部往下，沒有反向；core 不依賴它們；編排層不認得它們（RC-O4、RC-O8）。
6. **Ownership**：
   - catalog 的格式與驗證、id、loader 介面、`AssetStore` → asset。
   - Product 的 store 實例、`AssetOptions`、World 端的 `AssetRequests` 與 `AssetView` → world_asset。
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
| DI 2 載入是外部服務；經 Ingress 進 (c) 暫存，依請求序號的**連續前綴**套用；套用順序與批次切分和完成時序無關 | **AS-4**、**AS-5** | **批次切分、發佈順序改依完成時序**（決定 9）；只保留「全部結束後內容與完成順序無關」 | id 改成 catalog 的字串，**和請求順序也無關**（比舊的緊）；「IO 不碰 World、結果在 store 的鎖下進表、World 只在套用點看到」保留；不再要 (c) 區與 Ingress |
| DI 3 內容身分由 asset 從位元組算；ready 前載完；守衛：未就緒即 ready 是 Programmer Error | **AS-6**、**AS-7** | 「ready 前載完」不再是 Engine 的規則，舊守衛消失（12.2 第 3 題、決定 9）；內容身分不再由 asset 當規則，改成提供雜湊（決定 10） | id 可進存檔與權威資料，寫明 |
| 舊 RC：Running 期間串流進 (c) → 重審 | **RC-AS1** | — | 改成日後地圖 Module 的事（決定 8） |
| — | **AS-2**、**AS-3**、**AS-8**、**AS-9**（新） | — | 三件之 1、3；11-11 方向 3；異常分級；AGENTS §8 |
| — | **AS-10**、**AS-11**（新） | — | **緊**：值不能就地改；loader 的 `T` 要 const 並行安全、解構不綁執行緒；World 的資料不上鎖、store 的鎖不含讀檔解碼。決定 2、5、14 |
| — | **AS-12**（新） | — | catalog 是 Data Contract。決定 6 |
| — | **AS-13**（新） | — | 依賴由 Engine 推出、循環是錯誤。決定 11 |

**新的 DI**（守衛欄的「延後」照 `v8-arch-gyo2-guards-after-mechanism-stable`：綁實作方式的掃描守衛，等 scenario test 通過再立；檢查公開契約的負向編譯不在此限）：

- **AS-1 資產管理在 World 外；System 不持指標。**
  - 每個 Product 一個 `AssetStore`，由 world_asset 持有；值只在 store 裡。
  - World 端只有 `AssetRequests` 與 `AssetView` 兩種 Resource（請求入口、store 裡所有活著的項目的狀態與值），元件裡只有 `Handle<T>`、`AssetRef<T>` 或 `AssetId`。
  - GPU 資源是 render 的 Resource；渲染 World 讀別的 World 的資料只經 Extract（K.3-6）。asset 的 store 不是哪個 World 的資料，渲染 World 也和其他 World 一樣經自己的 `AssetView` 讀它，不算跨 World 讀（修訂 3 修正版檢查 N-6）。
  - 守衛：行為測試（兩個 World 請求同一個 id，store 只載一次、兩邊讀到相等的值；`BuildNewFirst` 或用 `CarryOver` 持有時，換 World 後同一 id 不重讀）。掃描延後。
- **AS-2 核心不認得格式。** asset 與 world_asset 不解析任何內容格式（catalog 的 JSON 是 asset 自己的契約，不算；文字 loader 只驗證 UTF-8）。格式由擁有型別的 Module 登記 loader；第三方只在該 Module 的 `src/adapters/<組件>/`，私有 link。守衛：行為測試（沒有 loader 的型別在 catalog 裡合法、請求時是 id 級 E2 `NoLoader`；同一（`type`、`format`）登記兩個不同的 loader，登記回錯誤、第二個不登記）。
- **AS-3 loader 是不依賴 ECS、可直接呼叫的純函式。** 輸入位元組、settings、id 與上限；輸出值或失敗；不碰 World、不讀時鐘、不持全域可變狀態；只連 `GYO::Asset` 就能呼叫。守衛：只連 `GYO::Asset` 的測試 target；同輸入兩次結果相等；**會宣告引用的測試 loader 直接呼叫兩次，引用清單相同**。CMake 掃描延後。
- **AS-4 IO 不碰 World；值只在 store 的鎖下、由完成的工作寫進表；World 只在套用點拉視圖。** 讀檔與解碼在 World 執行緒之外；World 的 `AssetView` 只在套用 System 與 world_asset 的準備函式換，一幀之內看到同一份。守衛：行為測試（X-5 的控制模式：一幀中途完成的工作，同一幀內看不到、下一個套用點看到）；`AssetView` 對外沒有可變操作的負向編譯樣本。
- **AS-5 身分在請求時給，而且就是 id。** Handle 在請求時同步給出，指向 id；全部結束之後，任何完成順序、任何發佈切分都得到相同的 store 內容。守衛：等價測試（第 10 節）。
- **AS-6 持久鍵是 id 字串；store 內部索引只在行程內。** id 可進內容、存檔、權威資料；`Handle<T>`、`AssetId` 的雜湊只用 id 字串。守衛：行為測試（兩份項目順序不同的 catalog，同一組 Handle 的 `Summary` 相同；新開局與讀檔進同一關的 `Summary` 相同）。
- **AS-7 「何時載完」與內容身分不是 asset 的規則；asset 提供能力。** asset 給狀態與就緒查詢（連同依賴，輪詢），保證同位元組同值（AS-3），並給每個 id 的原始位元組雜湊（預設開、可關）。要不要等、轉場、比不比雜湊與不同時怎麼辦，屬作者或網路 Module；到達時刻要影響判定，走 `rollback.Input<T>`。**判定用內容的 Handle 要一直持有**，放在沒有用 `rollback.` 登記的 Resource 裡（第 4 批宣告第 6 節的用語）；否則回滾倒回後再 `Load`，別的 World 的維護可能已經移除它，得到 `Queued`（5.6）。守衛：行為測試（雜湊開：同位元組同雜湊、不同位元組不同雜湊；關：查詢回「沒算」）；「asset 不拿雜湊做判斷」靠本頁與審查。
- **AS-8 錯誤只回報。** 請求級與 id 級的失敗是 E2，在狀態與就緒查詢裡；依賴的失敗帶 `DependencyFailure`；catalog 的資料規則是啟動錯誤，loader 登記的檢查回給登記的一方；不寫日誌、不自己重試、不讓 World 中斷；工作與請求的件數等式成立（5.8）。守衛：行為測試（各碼、各項）。
- **AS-9 介面與格式、位置分開。** 作者只用 id；catalog 把 id 對到（`type`、`path`、`format`、`settings`）；選 loader 用（`type`、`format`）；加一種格式（含引擎格式）＝登記一個 loader＋改 catalog 項目，不改請求的 API 與核心。守衛：行為測試（同一 id 從原始格式改指到引擎格式，作者的程式不變、值相同）。
- **AS-10 載好的值不可變。**（使用者 (a')；跨執行緒的部分照決定 5 現在寫）拆成三條：
  - **(a) 物件不改**：放進 store 的值在發佈之後不再改變；World 端只拿得到 `const` 存取。守衛：負向編譯樣本，照先例的極性（`tests/common/kernel/CMakeLists.txt:219-234`、`compile_fail/SystemContractSample.cpp:110-114`）：用 concept 讓它相依，`static_assert` 斷言「禁止的性質成立」，必須以該樣本的標記失敗；另有對照組（同一個 concept 形狀測一個確實存在的成員，必須編得過）。樣本：經 `AssetView::Get<T>` 拿到可寫的存取、對 `AssetView` 呼叫可變操作、`std::is_copy_assignable_v<AssetView>`、作者可建構 `AssetView`，第 6 步立。
  - **(b) 綁定只有 store 換，本批不換**：一段生命只發佈一次 `Loaded` 的值；`Failed` 之後重載、成功時才發佈，不算第二次（5.5）；熱重載回來時是「換一個值」，由 store 做（RC-AS4）。守衛：行為測試（工作的結果帶的生命序號和 info 不同 → 不採用、計入取消）。
  - **(c) loader 的 `T` 要 const 並行安全、解構不綁執行緒**：沒有 `mutable` 成員、沒有惰性快取、沒有非原子的內部計數；解構可以在任何執行緒發生（不綁執行緒、不持執行緒局部狀態）。作者要改就複製一份。字型的 glyph 快取這類可變狀態放在擁有型別的 Module 自己的 Resource。守衛：行為測試（最後一個持有者在別的執行緒放掉，解構正常；測試型別在解構子記執行緒）；其餘**只靠審查**——C++ 擋不住 `mutable`（Unity 是把 blob 限成 unmanaged 型別才做得到）。
- **AS-11 World 的資料不上鎖；store 有自己的一把鎖。**（D6b、M11 的原意；2026-11-22 收窄，第 24 節）World 的資料只經套用點與交換點批次進入，World 內不需要鎖。System 呼叫 store（`Load`、套用點、登記）時拿 store 自己的鎖；持鎖只做 5.6 列的表的操作，不讀檔、不解碼、不呼叫 loader 作者的函式、不交工作、不解構大物件（照 Bevy 的 `AssetInfos`）。M11 全句是「外部系統不持有、不修改 World；只經受控的交換點交換資料」：store 不持有、不修改 World 的資料；System 向 store 送出的是對外部服務的請求，結果只在套用點批次進 World。成立的條件：Handle 與 `AssetRequests` 不提供直接讀 store 當下狀態的入口；登記與預先登記的回傳值只是設定錯誤的回報（正確設定下一律成功），System 不得拿它分支判定；`Load` 的回傳除了 Handle 帶的生命序號（只用來判「視圖比 Handle 舊 → `Queued`」）之外不帶 store 的當下狀態；讀狀態一律經 `AssetView`；**System 不呼叫 store 的讀取與維護入口**（`Request`／`Read`／`Pump`／測試讀法，給不建 World 的使用者；靠審查），登記經 `AssetLoaderRegistry` 可以。請主會話在 `rebuild-concept.zh-Hant.md` 的 D6b、M11 加註這個讀法（第 15 節第 11 項）。守衛：負向編譯樣本，照先例的極性（AS-10 (a) 同一套）。例：`template<class H> concept HasState = requires(const H& h){ h.State(); }; static_assert(HasState<Handle<Text>>, "GYO_ASSET_SAMPLE_HANDLE_HAS_NO_STATE");` 必須以該標記失敗（非相依的 `requires` 在 clang 是沒有標記的硬錯誤，所以要用 concept）；`handle.Get()`、`requests.State(handle)`、`requests` 給出 store 各一個樣本，各帶標記與對照組；其餘「沒有這種成員」靠審查；行為測試「解碼中的工作不擋 `Load`」。
- **AS-13 依賴由 Engine 推出。** loader 經 `LoadContext` 宣告引用（id 或 catalog 裡的路徑），拿到不持有的 `AssetRef`；持有依賴的是 store 的依賴邊；就緒查詢的「好了」包含遞迴的全部依賴；父項自己的狀態不因依賴失敗而變；依賴的（`type`、`format`）在這個 Product 沒有 loader 也沒有預先登記就略過，略過是單調的，所以會晚登記的 loader 必須預先登記；同一個強連通分量裡的邊全部是 E2 `CyclicReference`、都不持有，結果和完成順序無關。守衛：行為測試（鏈、菱形去重、循環兩種完成順序與自我引用、略過、經路徑引用的三種失敗、父項放掉後依賴跟著歸零；第 10 節）。
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
- **RC-AS9**：World 的資料需要鎖才保得住；或 store 的鎖必須含讀檔或解碼；或 `Load` 等鎖、套用點持鎖的時間超出預算（3.3）；或某個點名的型別做不到 AS-10 (c)。日後 schedule 改成 Parallel（LIM-2）時，多個 System 同時 `Load` 的 `AssetRequests` 要重看。
- **RC-IM1**（image Module 的 DI 節）：不可信來源的圖成為 Feature，或 stb_image 的缺陷影響我們的內容，就換 adapter。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

每一列都先問：「這是不是在為舊形狀找位置？」（`target/README.md`〈每批事前宣告的檢查項〉）

| 選擇 | 採用 | 系譜 | 退掉的 | 決勝理由 | 需要它的 Feature |
|---|---|---|---|---|---|
| 資產管理放哪 | World 外，Product 一份，由 world_asset 持有 | Unity Entities（行程一份）、Bevy（App 一份）；決定 7 | 每 World 一份＋CarryOver（修訂 2）；行程級靜態物件；在 asset 裡解「Module 怎麼拿到 Product 層的東西」（凍結那一串，使用者 2026-11-22 撤回，交編排層） | 資產的生命週期和 World 無關（使用者）；Product 一份讓 Sim 與 Present 共用同一份值（F1）、換 World 不重讀（F3）；GYO 沒有行程級靜態先例，回主選單另建 Product 時重載可以接受（RC-AS3）。設定在建立時帶、loader 隨時登記（照 Bevy）；Module 拿到 `AssetLoaderRegistry` 的路是編排層的概念（12.3 第 13 題） | F1（Sim＋Present）、F3 |
| loader 表 | 帶鎖、隨時可以登記；可以預先登記，請求先等 | Bevy `server/mod.rs:73`（`RwLock`）、`preregister_asset_loader`（`lib.rs:608`、`server/loaders.rs:100-146`、`:274-287`）；Godot `add_resource_format_loader` | 凍結點、凍結後只准相同、每個 World 再交一次（修訂 3 修正版，使用者 2026-11-22 撤回） | 沒有一家有凍結點；登記與請求要同時看 loader 表與 info 的表，合成一把讀寫鎖（5.5 末表）（決定 13） | F1（貼圖的 loader 要等 render 的裝置，請求可能先到） |
| 持久鍵 | catalog 的 id 字串 | 藍圖 :66、master catalog、Unity GUID；決定 6 | 路徑（修訂 2、Bevy）；GUID（不可讀、要工具產生） | 作者只用 id（使用者）；改名與轉檔不影響內容與存檔；字串讓手寫 catalog（F9）可行 | F3、F5、F6 |
| catalog | asset 擁有的 Data Contract：master 的格式（version 1）＋可選 `format`、`settings`；`content.json` 列出；執行期讀 | master `CatalogFormat.hpp`、`ContentManifest.hpp`、`docs/architecture.md:285-312`；藍圖 :66 | Bevy／Godot 的旁檔；Unity 的 build 產物；master 的「等值只看雜湊」；`content://` 來源名（修訂 3 初稿） | F5、F6 要以 id＋型別參照、照同一組規則寫；F9 要能手寫並在啟動時知道錯在哪；集中才查得到全內容唯一；執行期讀（三件之 3）。沿用 master 讓現有內容照樣有效，只是費用上的好處（比對見 5.3） | F5、F6、F9 |
| 單位的拆法 | `asset`（library，含 store）＋`world_asset`（Module） | `time`／`world_time` | 一個 Module；store 放 world_asset | 工具不建 World 也要 loader、catalog 與 store | F4、F6 |
| World 怎麼看到結果 | 套用點先跑維護（寫鎖）、再拉視圖（讀鎖）；store 的版本號變了就把所有活著的 info 拉進 `AssetView`；幀內只讀視圖 | Bevy 的 `Assets<A>` 裝全部、在 `PreUpdate` 一次套用（`lib.rs:423-433`）；**每個 World 各拉一份是新推導**（store 在 World 外） | 經交換點把值搬進 World（修訂 2）；不可變快照＋交接槽（修訂 3，撤回）；只拉本 World 持有的（b6d3，持有無法定義） | 一幀看到同一份；`CarryOver`、Extract 帶來的 Handle 也讀得到 | F1（Client 的回滾重跑） |
| 結果怎麼進表 | 工作執行緒完成時在 store 的寫鎖下直接寫 | Bevy 的判定（`server/info.rs:466-470`）；**改裝** | Bevy 經 channel 送到 `PreUpdate`（要 `&mut World`，`server/mod.rs:1890`）；1622 之前的「維護時發佈」 | 值在 World 外，不需要延後；延後會讓依賴鏈每層多等一幀、沒有 World 跑幀時結果堆住 | F1、F8 |
| 依賴何時排 | 父項解碼完、在工作完成裡排 | **改裝** | Bevy 的 loader 執行中啟動（`loader.rs:728-735`）；`LoadContext` 當場通知 store 排工作（對 F4 仍是純函式，但加邊分散在兩處） | 父項解碼失敗或被取消時沒有要收回的依賴；加邊與強連通分量只在「工作完成」一處（F4 要直接呼叫）；代價是依賴晚一步、父項失敗時依賴不載 | F1、F4 |
| 值的可變性 | 不可變、共享唯讀；三條分開寫 | Unity blob（唯讀）；使用者 (a') | Bevy、Godot 的可變值 | 不可變是日後最難改的約束；跨執行緒共用的前提（決定 5） | F1、F2 |
| 鎖 | World 的資料不上鎖；store 一把鎖，持鎖不含讀檔解碼 | Bevy `AssetInfos`＋`RwLock`（`server/mod.rs:72-73`、`:392`）；Godot ResourceCache 的 mutex〔知〕 | World 呼叫 store 也不上鎖（修訂 3 起的 AS-11，卡片自己加的前提，撤回）；Unity 式「請求只進佇列、計數由單一消費者」（每次複製 Handle 都要推，GYO 的多 World 下還要有人當消費者） | D6b、M11 只管 World 的資料；別家沒有一家做到呼叫外部服務也不上鎖；無鎖協定修了三輪不穩（第 24 節） | F1、F2 |
| 結果的順序 | 完成了就發佈＋一組 id 的就緒查詢；轉場歸作者 | Bevy（照完成順序） | 舊 DI 2 的連續前綴；整組才可見；作者可設組、Engine 保證同幀 | 使用者決定 9：轉場是遊戲設計；判定用的內容在開始 tick 前載完 | F1、F8 |
| 內容身分 | asset 提供原始位元組雜湊（預設開、可關）；比對歸作者或網路 Module | Unity content catalog、Bevy processor 的來源雜湊；第 4 批 3.2、4.4 | 只由作者對值算；asset 算且當規則（舊 DI 3） | 使用者決定 10：作者只用 id、asset 本來就讀位元組；繞開第三方解碼的跨平台差異；提供能力、不替外面決定 | F2、F9 |
| 依賴 | Engine 推出：loader 宣告引用，store 擁有依賴圖 | Bevy `LoadContext::load`（`loader.rs:735`）與失敗傳播（`server/info.rs:661-716`）；Unreal、Addressables、Godot 都由 Engine 推 | 作者自己列；本批只留介面 | 使用者決定 11：作者只關心使用；依賴是機制的核心能力 | F1（模型引用貼圖） |
| 「載完才開始」 | 作者的事 | `-build-world-phase-is-mechanism` | 舊 DI 3、runtime-composition DI 3 | Loading 不是機制 | F1、F8、F9 |
| 套用的位置 | 預設 `Main`/`First`；生成世界期間照 5.13 | Bevy 的 `PreUpdate`，往前移一格 | `PreUpdate` | 同一幀的重跑步與新步看到同一份 | F1（Client） |
| 釋放 | 一段；維護前再請求就救回 | Bevy 的引用計數與救回規則（`assets.rs:578-580`） | 兩段（藍圖 A4、Bevy `Unused`）；master 的 N 幀寬限 | 兩段的實際用途是渲染丟 CPU 副本，本批沒有渲染；換 World 靠救回規則，不需要寬限 → RC-AS7 | F3 |
| 去重的鍵 | id | Bevy 的（路徑、型別）＋修它的缺陷 | Bevy 原樣（settings 被忽略） | settings 隨 catalog 項目，同一 id 只有一種 | F1 |
| 錯誤 | 請求級與 id 級分開的 E2；依賴失敗帶父項與引用；不記日誌 | 〈Error levels〉；Bevy `UntypedAssetLoadFailedEvent`、`info.rs:661-716` | asset 自己寫 `Warning`（master）；請求級和 id 級不分（修訂 3 初稿） | Engine 的 Module 不自己寫日誌；兩個 World 用不同 `T` 請求同一 id 時，請求級失敗不該改那個 id 的狀態 | F1、F5 |
| 告知 | 輪詢 `State`／`Ready` | Unity `GetObjectLoadingStatus`、Godot `load_threaded_get_status` | Kernel 的 `Message`（Bevy `AssetEvent`，只在一個 main World）；每 World 的變更清單 `AssetChanges`（修訂 3 起，撤回） | 多 World 下事件與變更清單改了四次不穩；輪詢不碰 `Summary` 的訊息序號 | F1、F3 |
| 引用計數 | 一段生命一個強核心（原子計數）＋表裡的弱參照＋「待略過」計數，判定都在 store 的鎖下 | Bevy `StrongHandle` 的 drop channel（`handle.rs:89-96`）、`get_or_create_path_handle`（`server/info.rs:274-310`）、`process_handle_drop`（`:773-775`） | 無鎖的比較並交換協定（修訂 3 起三版，撤回）；Unity 式計數全在單一消費者 | 照搬已解決的演算法；有沒有人持有與要不要移除在同一把鎖下判 | F1、F3 |
| loader | 純函式、整檔、直接可呼叫；引用經 `LoadContext` 宣告、回不持有的 `AssetRef` | master `IAssetLoader` 去掉型別抹除；Bevy `LoadContext::load`（`loader.rs:735`） | Bevy 的 `Reader`＋async；Bevy 的回強 Handle（值持有，循環無解）；master `AnyAsset` | F4 要直接呼叫；串流沒有 Feature（決定 8）；依賴由 Engine 推出（決定 11），持有放在 store 的邊才能和不可變的值相容 | F1、F4、F6 |
| 上限 | 在飛與 loader 上限：`AssetOptions` | AGENTS §2；master `maxLoadsPerFrame` | 沒有上限；每次發佈的上限（修訂 3，事件驅動下沒有時間基準） | F8 的 242 MB 要靠在飛上限管記憶體；`engine-not-rigid` | F8 |
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
| 第 2 步 | `AssetStore`：catalog 項目與 info、表與鎖、強核心與「丟了」佇列、請求的三支、救回、重載、工作完成時的寫入與依賴、在飛上限與名額、loader 表與預先登記、`AssetLoaderRegistry`、維護、`Pump()`、依賴圖（邊持有、強連通分量、略過）、原始位元組雜湊；X-5 的控制模式 | Opus／high | 上段（執行緒） |
| 第 3 步 | `world_asset`：store 的持有與 `AssetOptions`、`AssetRequests`、`AssetView`、套用 System 與準備函式、5.9.1 的雜湊函式 | Opus／high | 上段 |
| 第 4 步 | 接交換點與生成世界（照它們的宣告）、真執行緒模式、上限；等價測試 | Opus／high | 上段（執行緒、決定性） |
| 第 5 步 | 換 World 與讀檔案例、丟棄件數（store 的計數）、錯誤各碼 | Opus／high | 上段 |
| 第 6 步 | scenario test、負向編譯、突變、新頁、dev_log、審查；AS-10 (c)、「判定用的內容在進判定前載完」、「判定用內容的 Handle 要一直持有」登記到 `docs/cross-component-rules.md`（AGENTS §1；owner asset） | 實作 Opus／high；審查 Fable／high | 上段 |

## 10. 測試、判定（先宣告；結果出來之後不放寬）

測試用的型別：文字（真的 loader）；測試目錄裡三個測試 loader：一個慢、可以指定失敗；一個模擬引擎格式（`format` 為 `gyotxt`）；一個**會宣告引用**的（檔案內容列出它引用的 id 或路徑）。測試用的 catalog 由測試程式產生。

**scenario test（第 6 步，沿遊戲流程）**：
1. 生成世界分兩批：第一批請求 6 個 id，其中一個檔案不在、一個超過上限、一個 id 不在 catalog。
2. 跑到就緒查詢 `AllFinished`（`Complete` 不成立，因為有失敗）：3 筆 `Loaded`；2 筆 id 級 `Failed`（`NotFound`、`TooLarge`）與 1 筆請求級 `UnknownId`；就緒查詢的失敗清單是那 3 個。
3. **兩個 World 共用**：第二個 World 請求同樣 3 個 id：讀檔計數不增加，兩邊讀到相等的值。第 3 步結尾第二個 World 放掉這些 Handle，並跑過一次它的套用點（維護）。
4. **讀檔，先驗證**：在現在的 World 請求存檔要的 2 個 id，等到結束，發 `ReplaceRequest`（`BuildNewFirst`）。新 World 在 `beforeSwap` 再請求：命中，讀檔計數不增加。沒再要的 1 個在下一次維護移除；用 store 的測試讀法看它的項目回到 `NotRequested`。
5. **`DropOldFirst`**：(a) 作者把 Handle 放在自己標 `CarryOver` 的 Resource：讀檔計數不增加。(b) 不放：world_asset 的準備函式先跑維護，舊 World 的「丟了」被處理，重讀，讀檔計數加 1（確定的行為）。
6. **先驗證失敗**：存檔記一個不在 catalog 的 id。驗證得到請求級失敗（`UnknownId`），不發替換，舊 World 照跑。
7. **新 World 的起點**：替換來的新 World 在 `beforeSwap` 命中已載好的 id，當場讀得到值，`State` 是 `Loaded`。
8. 件數的突き合わせ：工作與請求兩條等式（5.8），各項數字寫在測試裡。

**等價測試（AS-5）**：同一組 20 個 id，用 X-5 的控制模式比較：(1) 一次全部完成；(2) 反序分 4 次完成；(3) 交錯分 7 次完成（3、3、3、3、3、3、2 筆），每次之後跑一次套用點；(4) 載入中途丟掉 3 個 Handle、同時再請求 3 個新的；(5) 其中含循環 A→B→A 與自我引用 C→C，A、B 的完成順序兩種都跑。判定：每一組和**它自己的基準**比——同樣的請求與放掉、一次全部完成的那一次——全部結束後 store 內容與依賴狀態相同（不比生命序號與嘗試序號，兩者依順序而異）；(1)～(3) 彼此也相同；件數等式成立。

**依賴（AS-13）**：用會宣告引用的測試 loader：
- 鏈 A→B→C：只請求 A，就緒查詢在 C 載好之前回「沒好」，之後回「好了」，id 清單是 A、B、C；
- 菱形 A→B、A→C、B→D、C→D：D 只讀一次；
- 循環 A→B→A，A、B 的完成順序兩種都跑：兩條邊都是 `CyclicReference`、都不持有，A、B 的「連同依賴」都是 `Failed`；作者只持有 B 時，A 在下一次維護被移除（兩種順序結果相同）；自我引用 C→C 同樣處理；
- 依賴失敗 A→X（X 不在）：A 自己 `Loaded`，就緒查詢回「失敗 1 筆：X，經由 A」；
- 經路徑引用：catalog 裡有的路徑解成 id；沒有的是 `UnlistedReference`；兩個 id 指向它是 `AmbiguousReference`；`../` 超出根目錄、絕對路徑、反斜線是 `BadReference`；
- 略過：依賴的（`type`、`format`）沒有 loader 也沒有預先登記，那條依賴略過，父項 `Complete`，就緒查詢列在「略過」裡，讀檔計數不增加；之後才登記 loader，已略過的邊不重新持有（單調）；有預先登記的依賴是 `WaitingForLoader`，父項 `Incomplete`，loader 到了才 `Complete`；
- 祖先：C→A→X，X 的測試 loader 第一次指定失敗：`Ready(C)` 列出一筆依賴失敗（帶父項 A）；作者 `Load(ref X)` 觸發重載，成功後下一個套用點 `Ready(C)` 是 `Complete`。另一案：C→A→Y、Y 不在 catalog（邊級 `UnknownId`），永遠不回復。另一案：父項 A 解碼失敗 → 它的依賴不載。
- `AssetRef` 不持有：父項移除後，作者手上只有 `AssetRef` 的依賴也跟著移除；作者另外 `Load(ref)` 拿了 Handle 的就留著；
- 放掉 A：下一次維護 A 移除，B、C 跟著歸零移除。

**原始位元組雜湊（AS-7）**：開：同位元組同雜湊、改一個位元組雜湊不同、等於 `Sha256` 直接算的值；關：查詢回「沒算」。

**同一幀看到同一份（AS-4）**：一幀中途完成的工作，同一幀之內的 System 看不到；下一個套用點看到。

**舊視圖配新生命（5.7）**：World A 套用後放掉 X 的最後一個 Handle；World B 的套用點跑維護、移除 X；A 同一幀稍後 `Load(X)` 得到新生命 → A 這一幀 `State` 是 `Queued`、`Get` 回空（不是舊生命的 `Loaded`）。經 Extract 拿到、建在本 World 上次拉取之後的 Handle → `Queued`。

**值不可變（AS-10）**：
- (a) 負向編譯：經 `AssetView::Get<T>` 取可寫存取、對 `AssetView` 呼叫可變操作，各一個樣本（帶自己的 `static_assert` 標記與對照組），編譯必須失敗。
- (b) 舊生命的工作結果晚到：移除 X → 再請求（新生命，工作壓住）→ 舊工作先完成（測試 loader 第一次指定失敗，兩次結果分得開）→ 舊結果算取消，新生命的值來自新工作。
- (c) 最後一個持有者在別的執行緒放掉：測試型別的解構子記下執行緒，解構正常、只發生一次。

**`Summary`（AS-6、5.9.1）**：兩份項目順序不同的 catalog（store 內部索引不同），同一組 Handle 的 `Summary` 相同；新開局與讀檔進同一關（後者命中），`Summary` 相同。

**catalog（AS-12）**：(A) 組每條規則各一個不合的範例 → 對應的啟動錯誤；刻意不查的各一個範例合法；master 的 `asset_catalog.json` 與 `content.json`（從 origin/master 3b411e21 的 `assets/object_fps_pvp/` 複製到 asset 的測試 fixture）原樣通過；寫出再讀回相等。(B) 組：同一（`type`、`format`）兩個不同 loader、兩個 C++ 型別宣告同一型別名，各一個登記錯誤。loader 表：先預先登記、請求 → `WaitingForLoader`、就緒查詢列在「在等 loader」；再登記 loader → 等待者在鎖下變 `Queued`、載好。某個 loader 型別預先登記 {`png`, `jpg`}、它登記時只認 `png` → `jpg` 的等待者變成 `NoLoader`；預先登記期間別的 loader 型別登記同一（`type`、`format`）→ 登記回錯誤。沒有預先登記就請求 → `NoLoader`；登記之後**在第一個 Handle 還持有時**再請求（同一個 World 或另一個 World）→ 重載、載好，`AllFinished` 先真後假再真。`settings` 解讀不了 → 那幾個 id 是 `BadSettings`，loader 照樣登記。在 System 裡登記也可以（照 Bevy）。

**型別名**：沒有登記任何 loader 時，`Load<T>` 用錯 `T` → 當場請求級 `TypeMismatch`，不碰 info。

**事先轉檔（AS-9）**：同一 id 從 `.txt` 改指到 `gyotxt` 格式的檔，作者的程式不變、值相同；同一（`type`、`format`）登記兩個不同的 loader → 登記回錯誤；同一個 loader 登記兩次 → 冪等。

**直接呼叫（AS-3、F4）**：只連 `GYO::Asset` 的測試 target 呼叫文字 loader；同輸入兩次結果相等；兩條執行緒各呼叫 1,000 次，結果全部等於單執行緒（0 件不一致）；會宣告引用的測試 loader 直接呼叫兩次，引用清單相同。

**引用計數與救回（5.5，照 Bevy）**：控制模式下（有界等待的界值一律 2 秒，正常路徑不靠它）：
- 「最後一個 Handle 放掉」之後、維護之前再請求：救回、「待略過」加一、同一份、讀檔計數不增加；維護處理那筆「丟了」時扣掉「待略過」、不移除；
- 維護之後再請求：新的一段生命、重讀；
- 兩個 World 對同一個沒載的 id 先後請求：只排一筆工作；
- 救回後再放掉要移除：救回（待略過＝1）→ 放掉 → 維護處理兩筆「丟了」→ 第一筆扣掉待略過，第二筆移除；
- 請求級 Handle 不帶核心：World A 持有 X；World B 以錯的 `T` 請求 X（`TypeMismatch`）後放掉；跑維護 → A 讀得到 X、待略過仍是 0；
- 舊生命的結果晚到：見 AS-10 (b)，舊工作完成後、新工作放行前跑一次套用點，`State` 必須是 `Queued` 或 `Loading`、取消件數＝1；
- 舊視圖配新生命：用 `RunScheduleSteps` 交錯兩個 World 的 schedule（`RunFrame` 是逐 World 串行，`Product.cpp:536-539`、`:544`）；
- 在飛上限（上限 2、請求 5 筆）：同時 `Loading` 不超過 2、先後照請求順序；還沒開工就被移除的讀檔計數不增加、名額交給下一筆；開工後被取消的做完才放名額；父項完成時排的依賴拿得到名額；工作等式成立；
- `BadSettings` 的兩條路：(a) 預先登記 → 請求（`WaitingForLoader`）→ 登記 settings 壞的 loader → `Failed(BadSettings)`、不排工作；(b) 請求（`NoLoader`）→ 登記 settings 壞的 loader → 再請求 → `Failed(BadSettings)`、不排工作；
- 解碼中的工作不擋 `Load`（真執行緒）：慢的測試 loader 卡在一個閂上；另一條執行緒對別的 id 呼叫 `Load`，主執行緒等 2 秒並斷言它已回來；斷言之後一定放閂、join，所以突變下是斷言失敗、不是掛住；
- 在飛上限：用慢的測試 loader 的閂加真執行緒，「同時 `Loading` 不超過 2」由測試 loader 的並行計數觀察；「舊工作先完成」用 X-5 的開工、完成分開控制；
- (B)5 的三個方向各一案：預先登記期間別的 loader 登記、已登記時別的 loader 型別預先登記、兩個 loader 型別先後預先登記，各回錯誤；
- `Failed` 之後在別的 World 再請求（第一個 Handle 還持有）：重載；`BadSettings` 再請求：仍是 `Failed`，不排工作。
- 突變（各一個專屬的預期失敗，寫在突變清單裡）：(1) 處理「丟了」時不看「待略過」→「救回之後值仍在」失敗；(2) 「待略過」> 0 卻不減一 →「救回後再放掉要移除」失敗；(3) 工作結果不比生命序號 →「舊生命的結果晚到」的中間斷言（`State` 必須是 `Queued` 或 `Loading`）失敗；(4) 套用 System 在幀中途再拉視圖 →「同一幀看到同一份」失敗；(5) 請求級失敗的 Handle 帶核心（帶當前生命序號）→「請求級 Handle 不帶核心」失敗；(6) `Load` 路徑交工作給執行器時還持鎖（執行器是當場跑完的模式）→ store 的鎖記著獨占持有者，同執行緒重入是 `GYO_ASSERT`，預期失敗是那條斷言的訊息（工作完成那條路的巢狀提交只入列、不重入，殺不到，不宣稱）；(7) 工作執行緒持鎖讀檔或解碼 →「解碼中的工作不擋 `Load`」在 2 秒內沒回來；(8) 視圖不比生命序號 →「舊視圖配新生命」失敗；(9) 開工前不比生命序號 →「在飛上限」的「讀檔計數不增加」失敗；(10) 給 `Handle` 加 `State()` → AS-11 那個樣本變成編得過，configure 報「variant HANDLE_STATE compiled」（照先例的判法）。
- 對應的案例都列在本節，步驟見上。

**真執行緒模式（AS-4、AS-11）**：
- **store 層**（第 2 步，不需要 World）：N 條測試執行緒直接對 `AssetStore` 同時 `Request`、放掉、`Pump`，20 個 id 跑 50 次。判定：每次的最終 store 內容等於控制模式；件數等式 50/50 成立。
- **World 層**：照第 5 批給的能力。今天經 Product 入口不能兩條執行緒各跑一個 World（`Product.cpp:364-370` 的 `RequireNotRunning`）；只給串行時，這一項由 store 層案例擔保，寫在 dev_log。
- **持鎖不做的事**（AS-11）：測試執行器的 `Submit`、測試型別的解構子、測試 loader 的 settings 解讀函式，各回呼一次 store 的入口；持鎖時就是「同執行緒重入」的 `GYO_ASSERT`。突變 (11)～(13)：完成路徑持鎖交工作、持鎖解構大物件、持鎖解讀 settings，預期失敗都是那條斷言的訊息。
- **負向編譯的判法**：asset 的 try_compile 比對「靜態斷言失敗＋標記在同一行」（例：`static[_ ]assert(ion)? failed[^\n]*<標記>`），對照組用和樣本完全相同的型別寫法；每個樣本在突變清單各配一個「加上那個成員」的突變。先例 `tests/common/kernel/CMakeLists.txt:231` 只找標記，會被同一行的無關硬錯誤騙過，報給主會話（第 15 節）。

**突變**：每條 DI 的守衛各至少一個專屬的預期失敗（AGENTS §14.2、§14.5）。AS-7「asset 不拿雜湊做判斷」、AS-10 (c)、AS-11「作者的 System 不呼叫 store 的讀取與維護入口」、負向編譯樣本以外的「沒有這種成員」、「持鎖讀檔」只靠審查，不立突變，寫明。清單在實作時列，受別的模型做敵對式檢查。

**判定**：上面全部通過；L1 三平台（linux-x64、windows-x64、macos-arm64）通過；突變全部 killed。

**量測**：載入時間、記憶體、catalog 的讀取時間只做探索量測，標「不是判定」；數字延後（12.2 第 6 題）。

## 11. 卡片照 L17 判的（不是使用者的話）

**照命名規範、別家原始碼、已定原則判的**：
- 兩個單位的拆法；`AssetStore` 在 asset 函式庫；名字 `AssetStore`、`AssetId`（5.1）；
- id 的字元集與長度（5.2）；catalog 的三件（格式、誰產生、Version 與 Validation Rule）照 master 沿用、加兩條、放寬一條（5.3）；
- store 放在 world_asset 登記單位的實例；設定在建立時帶、loader 帶鎖隨時登記（照使用者 2026-11-22 的方向）；型別名綁在 C++ 型別的特性上（5.4、5.16）；
- 請求、救回、維護、工作完成照 Bevy 的演算法，判定在 store 的一把鎖下；`DropOldFirst` 不放 `CarryOver` 就重讀；取消的計法（5.5）；改裝的地方列在 5.5 末的表；
- 維護掛在套用點（照 Bevy 的 `PreUpdate`）；版本號變了才拉視圖；新 World 的起點；套用在 `First`（5.6）；
- 請求級與 id 級分開；錯誤碼與級別的對應；丟棄的出口留給編排層（5.8）；
- 設定在建立時帶、loader 帶鎖隨時登記與預先登記（5.4，照使用者 2026-11-22 的方向）；讀值一律 `AssetView::Get<T>`；catalog 規則分兩組（5.3）；
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

### 12.1 使用者的決定（2026-11-21～22）

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
| 13 | 撤回凍結機制；設定在建立時帶、loader 帶鎖隨時登記，loader 還沒到的請求先等（照 Bevy 的待定機制）；「Product 層 Resource」交編排層 | 「？？？？我怎麼覺得這個機制怪怪的」「我想的是，為何asset 要管這個， 其他家的 assetStore 和我說下。」看了別家的對照後：「轉回去」 | 原第 13 題的 (P) Product 層槽、(W) 第 4 批的每 World 槽，連同凍結點、凍結後只准相同、每個 World 再交一次、冪等判斷；對照 Bevy、Godot、Unreal、Unity，沒有一家有凍結點 | side chat（2026-11-22）；落點 5.4、5.3 (B)、5.16、12.3 第 13 題 |
| 14 | 執行演算法照別家已解決的做法；拿掉卡片自己加的前提 | 「執行算法不穩定，那可以去找別人的實現。看看別人的演算法能不能處理我們的問題。」「我100%可以確定，我們碰到的問題，別人一定已經碰到，並且已經解決了。」「最有可能的地方就是我們在解一個1+1 = 3 的證明」 | 繼續修無鎖協定／宣告退回契約層／改照別家的演算法 | 本卡（2026-11-22）；落點第 24 節 |
| 15 | 本宣告的動工前核准 | 「核准」 | 核准／再改 | 本卡（2026-11-22）；核准的版本 55a14825 |

決定 4～13 的理由照 side chat 的轉述記在主會話的 HANDOFF；決定 14 由本卡轉。正文裡的「三件之 N」指決定 1 的第 N 件；「決定 N」指本表的編號。

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
- asset 需要它的地方：擁有型別的 Module 拿到 Product 的 `AssetLoaderRegistry` 來登記 loader（5.4、5.16）；world_asset 的建立參數 `AssetOptions` 由產品給（5.4）。
- 交給主會話與編排層定；照四樣轉給主會話（2026-11-22）。asset 依賴它（2.2），定之前 loader 的登記入口只定成 `AssetLoaderRegistry` 這個介面。

**仍歸使用者的**：本頁的動工前核准；第 6 題的數字（產品搬回時）；stb_image 的下載（image 那批）。

## 13. 外部組件的下載

| 組件 | 來源 | 用途 | 授權 | 何時 |
|---|---|---|---|---|
| stb_image（`stb_image.h`） | nothings/stb，釘 commit | image Module 的 PNG 解碼 | MIT 或 Public Domain | SDL 那批的 image Module（12.2 第 5 題）。要使用者同意，到時再問 |

## 14. 範囲の限界

- **交換點、生成世界還沒有宣告**：5.6、5.13 是需求，不是設計。滿足不了的話先回報，不在 asset 裡繞過。
- **第 4 批還沒合併**：雜湊客製點、`Summary` 的規則（含訊息序號進摘要），照第 4 批宣告（分支頭 15e79bd1；`World.cpp` 的行號不變，該分支的 `Product.cpp` 多 9 行，合併後本頁引的 `Product.cpp` 行號會移）。合併時形狀不同，5.7、5.9.1 跟著改。
- **依賴只用測試用 loader 驗**：第一個真正的使用者（模型引用貼圖）在產品搬回或 SDL 那批（第 9 節）。
- **預算延後**：沒有載入時間、記憶體、catalog 讀取時間的判定。
- **載入中的 World 的 `Summary`**：asset 的視圖不寫進摘要，載入進度不同的兩個 World 摘要可能相同；比摘要要在「全部完成」之後比。
- **`Summary` 讀兩次相同的前提**：兩次之間 catalog 沒有換（同一份內容版本）。
- **原始位元組雜湊只代表檔的位元組**：catalog 項目的 `format`、`settings` 不同時，雜湊相同、值不同（5.5）。
- **`DropOldFirst` 不放 `CarryOver` 就重讀**：作者要不重讀，就用自己的 `CarryOver` Resource 持有 Handle（5.4）。
- **丟棄件數沒有 E3 出口**：等編排層的結束回呼（5.8）。
- **不套用的 World 留住舊值**：它的 `AssetView` 讓已移除的值不釋放，直到下一次套用（5.6）。
- **套用點拉視圖的成本**：store 的版本號變了的套用點，對所有活著的 info 拉一次；BigMap 的 region 數千時要量測（3.3 的預算項）。
- **Module 拿到 `AssetLoaderRegistry` 的路未定**：交編排層（12.3 第 13 題；需求 O-A1～O-A4 在 5.4）；在那之前 loader 的登記入口只定成 `AssetLoaderRegistry` 這個介面。
- **會晚登記的 loader 必須預先登記**：否則依賴會被永久略過，最後的 store 內容依登記時刻而定（5.4、5.17）。
- **F1 的 viewmodel 依賴**：master 的 viewmodel 是 `text` 型別的 json，文字 loader 不宣告引用；要由 Engine 推出它引用的 fbx 與 png，需要產品自己的 loader。fbx 內部有沒有記貼圖檔名：未查。
- **loader 的跨平台逐位元**：文字 loader 原樣交出位元組；其他 loader 由各自的 Module 宣告。
- **io 的多執行緒讀的契約**未確認（5.12）。
- **資料競爭**：CI 沒有 TSan，store 的鎖與原子操作的檢出只在本機 TSan 建置；判定靠控制模式的案例。
- **別的 World 的維護會在本 World 幀中途移除歸零的項目**：同一幀放掉再 `Load` 不保證救回；判定用內容的 Handle 要一直持有（5.6）。
- **同型別名、只有一個有 loader**：登記時查不到，`Load<T2>` 通過、`Get<T2>` 永遠回空（5.3 (B)4）。
- **回滾重跑的測試**：「同一幀的重跑步與新步看到同一份」與重跑步裡 `Load` 的計數，要等第 4 批合併才測得了。
- **回滾歷史延後釋放**：歷史裡存著元件的 Handle 副本時，資產要等歷史修剪後才釋放（5.4）。
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
10. **cross-component-rules**：AS-10 (c)（loader 的值 const 並行安全、解構不綁執行緒）與「判定用的內容在進判定前載完」、「判定用內容的 Handle 要一直持有，放在沒有用 `rollback.` 登記的 Resource」在實作第 6 步登記，owner asset。
11. **D6b、M11 的加註**（決定 14）：asset 把「World 內不上鎖」讀成「World 的資料不上鎖；System 呼叫外部服務時，那個服務可以拿它自己的鎖」，結果只在套用點批次進 World。同步呼叫外部服務可以帶回的，只有不進雜湊、不進相等的身分資訊（asset 是 Handle 的生命序號，效果只有 `State` 回 `Queued`）。請主會話在 `rebuild-concept.zh-Hant.md` 的 D6b、M11 加註這個讀法，免得日後網路等 Module 各自解讀。 登記與預先登記的回傳值是設定錯誤的回報（正確設定下一律成功），System 不得拿它分支判定。通則是：同步呼叫外部服務帶回的值，不得讓判定依時序而變。
12. **負向編譯先例的判法**：`tests/common/kernel/CMakeLists.txt:231` 只在輸出裡找標記，同一行有無關的硬錯誤（例：型別名打錯）時，編譯器會把那一行原始碼印出來、裡面就有標記，樣本永遠空過。asset 改用「靜態斷言失敗＋標記在同一行」；kernel 的先例不在本批，交主會話。
13. **`WorkerPool` 的 adapter 定在哪裡**：~~交主會話~~ 已決定（使用者 2026-11-23 在主會話「adapter 照建議放 world_asset」）：world_asset 提供 `WorkerPoolExecutor(WorkerPool&)`，只轉接、不開執行緒；用哪個池、開幾條執行緒仍由產品選，也可以給別的執行器。world_asset 連 `GYO::Threads`（core，手寫在 CMake，不走 O-6）；asset 函式庫照舊不依賴 threads。

## 16. 停止條件

- 實作段的測試、計測、判定卡住時：AGENTS §14 冒頭的三條（只用在測試與判定，不是設計段的時間上限）。
- 交換點或生成世界的宣告和 5.6、5.13 的需求衝突。
- 第 4 批合併後，雜湊客製點或 `Summary` 的規則和 5.9.1 的假設不同，而且影響 AS-6。
- 實作中發現某個 loader 必須碰 World（RC-AS2），或符合 RC-AS9（World 的資料需要鎖、store 的鎖必須含讀檔解碼、等鎖時間超出預算）。
- 編排層定下的「Product 層 Resource」形狀和 5.4 的需求 O-A1～O-A4 衝突。

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

## 23. 決定 13 之後的敵對式檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。對象 133ae36e。**結果**：blocker 3、should 9、nit 8，證據 `declaration-d13-adversarial-check.md`。全部採用；都能照已定的決定、別家原始碼與命名規範判，沒有要交使用者的。

| 條 | 處理 |
|---|---|
| B-1「`Failed` 再請求」在槽協定裡沒有分支 | 5.5 原地重試（照 Bevy `server/info.rs:279-286`）：命中遇到 `Failed` 推一筆重試，維護改回 `Queued`、世代不變；AS-10 (b) 改成「一段生命只發佈一次 `Loaded` 的值」；第 10 節加兩個 World 的重試案例 |
| B-2 型別名只在 loader 表 | 5.4：型別名綁在 `T` 的編譯期型別特性上，loader 宣告的必須相同；World 端只用 catalog 加特性判 `TypeMismatch`；5.3 (B)4、5.17 第 3 步改 |
| B-3 槽協定的競態 | 5.5：計數、死了、世代同一個 64 位元原子字；請求用比較並交換的迴圈；維護「取出節點 → 清旗標 → 判」三支；取消條件「已死或世代不同」；世代繞回以 `GYO_ASSERT` 擋；第 10 節加不靠時序的交錯列舉 |
| S-1 略過單調與晚登記 | 寫成前提：會晚登記的 loader 必須預先登記（5.4、5.17、AS-13、第 14 節）；第 10 節加案例 |
| S-2 預先登記的洞 | 5.4：`WaitingForLoader` 狀態、就緒查詢回「在等 loader」、登記喚醒維護、只認一部分 `format` 時其餘變 `NoLoader`；場景寫明（貼圖等 render 的裝置）；件數加「等 loader」；12.1 決定 13 補上使用者要的「先等待」 |
| S-3 登記的錯誤與時機 | 5.4：`BadSettings` 是 id 級、loader 照樣登記；登記只在 Product 的執行緒、幀外，System 裡呼叫是 `GYO_ASSERT`；啟動時記著到 `Finish` 回報；5.3 (B) 改名「登記時的檢查」 |
| S-4 `AssetOptions` 與 Product 層 Resource | 5.4 給編排層的需求表 O-A1～O-A4（`AssetOptions` 要早於 world_asset 的 `Configure`、依賴的單位拿得到 store、`Configure` 的錯誤回報、結束回呼）；第 8 節第一列「退掉的」改寫；第 16 節加停止條件 |
| S-5 維護的驅動 | 5.6：再排經同一個旗標、維護彼此不重疊；執行器停了之後排維護是空操作、store 解構時清佇列；X-1 補上；喚醒事件補齊 |
| S-6 Resource 的存取宣告 | 5.4 的存取政策表：`AssetRequests` 多寫者共用、`AssetFrame`／`AssetChanges` 只有套用 System 寫（作者宣告寫它是 D-E2，即 AS-4 的守衛）；`Assets<T>` 只是說法，讀值一律 `AssetFrame::Get<T>`；RC-AS9 加 Parallel 的檢查 |
| S-7 命中的 `Added` 由誰產生 | 5.7：`AssetChanges` 由 World 自己比較前後兩份快照算；交接槽只放最新快照（取代「包」，溢出不再需要）；`Unfinished()` 本地算 |
| S-8 第 10 節缺的與判不了的 | 加祖先告知、略過單調、預先登記的三種、重試、交錯列舉、維護；負向編譯每個樣本帶 `static_assert` 標記與對照組；「每次讓 3 筆完成再跑維護」；登記兩次的判定改成冪等／回錯誤；`DropOldFirst` 的插入點用新 World 描述的 `build` 回呼 |
| S-9 殘留 | AS-2 守衛、「有 loader 的才持有槽」、發佈上限的殘句、決定 1～13、第 11 節、AS-11「幀內不上鎖」與持鎖範圍、5.3 (1) 決勝理由 |
| N-1 編號與日期 | 頂端 2026-11-22、12.1 標題、決定 11 的出處列 |
| N-2 第 4 批分支頭 | 8d41da98（行號相同） |
| N-3 第 21、22 節的對應 | 第 21 節照歷史保留；`AssetSetup`、`ModuleBuffer` 相關的處理已由第 22 節撤回 |
| N-4 G-3 措辭 | 「在準備函式裡取的快照」 |
| N-5 件數類別 | 5.8「取消含排了還沒開始的」；5.17 各類互斥、被標成循環的邊從原類移出 |
| N-6 上限、`NotFound`、骨架關掉 | 包已拿掉、沒有清單上限；5.8 列出 `NotFound`、`ReadFailed`；5.6 寫骨架關掉時由產品掛套用 System |
| N-7 先例 | 改成 `time`／`world_time`（`world_logging` 是 library_only） |
| N-8 F1 的 viewmodel | 第 14 節：viewmodel 是 text 型別的 json，文字 loader 不宣告引用，那一例要產品自己的 loader；fbx 內是否記貼圖檔名未查 |

**這一版**：再送 Fable 5.2／high 敵對式檢查。

## 24. 2026-11-22：執行演算法改照 Bevy（拿掉卡片自己加的前提）

**經過**：dd31fff8 的敵對式檢查（blocker 4、should 10、nit 11，證據 `declaration-dd31-adversarial-check.md`）之後，卡片停下來問「修還是重來」。使用者：「執行算法不穩定，那可以去找別人的實現。看看別人的演算法能不能處理我們的問題。」「我100%可以確定，我們碰到的問題，別人一定已經碰到，並且已經解決了。」「最有可能的地方就是我們在解一個1+1 = 3 的證明」。

**查證**（`execution-algorithm-survey.md`）：
- Bevy：`load` 拿 `AssetInfos` 的 RwLock 寫鎖（`server/mod.rs:392`）；Handle 是 Arc，最後一個放掉經 drop channel 通知（`handle.rs:89-96`）；救回用「待略過」計數（`server/info.rs:292-310`、`:773-775`）；處理 drop 整段持鎖（`assets.rs:570-587`）；在 `PreUpdate` 一次套用（`lib.rs:423-433`）。
- Unity：請求與釋放推進拿 SpinLock 的佇列，計數由主執行緒單一消費者處理，狀態輪詢。
- Godot、Unreal〔知〕：mutex 或單一執行緒；輪詢或委派。
- D6b（`rebuild-concept.zh-Hant.md:68`）、M11（`:94`）只管 World 的資料不被外部寫，沒有要求「System 呼叫外部服務也不能上鎖」。

**結論**：「World 呼叫 store 也不能上鎖」（修訂 3 起的 AS-11）與「每 World 算變更清單」是卡片自己加的前提，是那個「1+1=3」。拿掉之後，剩下的就是 Bevy 已解決的問題。

| 改了什麼 | 落點 |
|---|---|
| 引用計數改成 Bevy 的強核心＋弱參照＋「待略過」，判定都在 store 的一把鎖下；撤回比較並交換、固定槽的原子字、世代、侵入式節點 | 5.5、第 8 節「引用計數」列 |
| World 端改成套用點拿鎖一次、把持有的 id 拉進 `AssetView`；撤回不可變快照與交接槽 | 5.4、5.6、AS-4、第 8 節 |
| 維護掛在套用點（照 Bevy 的 `PreUpdate`），撤回另外的喚醒協定 | 5.6 |
| 告知改成輪詢 `State`／`Ready`（照 Unity、Godot）；撤回 `AssetChanges` | 5.7、5.9.1、第 8 節「告知」列 |
| AS-11 收窄回 D6b、M11 原意：World 的資料不上鎖；store 有自己的一把鎖，持鎖不含讀檔解碼 | AS-11、第 8 節「鎖」列 |
| 第 4 批拿掉寫者數量：`AssetView` 的「作者不寫」改由型別本身與負向編譯守 | 5.4、AS-4 |
| 第 10 節：撤回交錯列舉與交接槽的案例，改成照 Bevy 演算法的救回、取消、重載、同一幀的案例與四個專屬突變 | 第 10 節 |

dd31fff8 檢查的 B-1～B-3、S-1、S-6、S-7、S-9 隨形狀消失；B-4 照上表處理。其餘 should、nit（預先登記的鍵、晚登記的界線、件數、就緒查詢的 `DependenciesFinished`、型別名與 `Get<T>` 的核對等）已在 5.4、5.5、5.7、5.8 處理，送下一輪檢查時一併核對。

## 25. b6d314bd 的敵對式檢查與處理（更忠於 Bevy）

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。**結果**：blocker 3、should 11、nit 10，證據 `declaration-b6d3-adversarial-check.md`。方向對得上 Bevy；三個 blocker 都出在卡片改裝 Bevy 時沒列出來的地方。這一輪照 Bevy 原樣改回，並在 5.5 末列出「照用／改裝」的對應表（AGENTS §2）。全部採用，沒有要交使用者的。

| 條 | 處理 |
|---|---|
| B-1「持有的 id」沒定義 | `AssetView` 拉 store 裡**所有活著的項目**（照 Bevy 的 `Assets<A>` 裝全部），用 store 的版本號擋不必要的拉取；`CarryOver`、Extract 的 Handle、依賴的 `AssetRef` 都讀得到（5.4、5.6） |
| B-2 項目永久存在、沒有生命編號 | catalog 項目底下「活著才有 info」（照 Bevy 處理 drop 時移除 info）；catalog 項目的生命序號（照 Bevy 的 generation）；請求三支窮舉；「丟了」帶生命序號、過期的略過；工作結果比生命序號；「一段生命可以先後有多個核心」（5.5） |
| B-3 RC-AS9 等仍寫「World 端不上鎖」 | RC-AS9 改成新 AS-11 的反面；R5、第 7 節系譜表、第 16 節同步 |
| S-1 排工作在持鎖區內 | 鎖內只記下要排的，放鎖之後才交執行器（照 Bevy `server/mod.rs:413-416`）；「丟了」佇列不用 store 的鎖；大物件在放鎖後才解構 |
| S-2 結果延到維護 | 工作執行緒在鎖下直接寫表、加依賴邊並當場排依賴、交接在飛名額；維護只處理「丟了」；不建 World 的使用者有 `Request`／`Pump`／`Read`（5.5、5.6） |
| S-3 `Failed` 再請求的寫法 | 5.5 一處定：`BadSettings` 留著；`NoLoader` 看現在有沒有 loader 或預先登記；其餘重載；命中與救回兩支共用 |
| S-4 請求級失敗的 Handle | 不帶核心、放掉不推「丟了」；第 10 節加案例與突變 (6) |
| S-5 當幀的 `State`、D6b／M11 | 本幀剛送出的回 `Queued`；AS-11 引 M11 全句並寫出成立條件與負向編譯守衛 |
| S-6 日誌怎麼知道新失敗 | `AssetView::Failures()` 列舉＋嘗試序號（5.7） |
| S-7 `DropOldFirst` | 改成確定的說法：不放 `CarryOver` 就重讀（5.4、第 10 節） |
| S-8「型別本身守」太滿 | 分兩層：成員與指派用負向編譯；整個換掉靠審查與套用 System 的自癒（5.4） |
| S-9 登記的殘留 | 拿掉「不能在 System 裡呼叫」與它的斷言；預先登記的鍵改成 loader 的 C++ 型別＋`format` 清單；登記時在鎖下直接轉等待者 |
| S-10 第 10 節 | 突變改成七個、各寫專屬的預期失敗（含「待略過不減一」、請求級 Handle 帶核心、持鎖交工作）；真執行緒比最後的 `State`／`Ready`；AS-10 (b) 的守衛改成生命序號；AS-11 有行為守衛；件數等式重分 |
| S-11 第 8、11 節的改裝 | 5.5 末的對應表；第 8 節「World 怎麼看到結果」標新推導、「鎖」把 Unity 移到退掉的；第 11 節改寫 |
| N-1 第 4 批分支頭 | aa2dee2f |
| N-2 決定 14 | 第 0 節、第 6 節第 1 項、「十件」 |
| N-3 殘詞 | 無鎖、槽、單一處理者、訊息順序、原地重試 |
| N-4 核心共有的東西 | 核心共有「丟了」佇列 |
| N-5 `Handle` 的 `==` | 以 id 比（5.4） |
| N-6 維護處理到空 | 5.5 寫明 |
| N-7 回滾歷史 | 5.4、第 14 節 |
| N-8 行號 | `lib.rs:687-688`、`Setup.hpp:203` |
| N-9 鎖的時間進預算 | 5.6、RC-AS9 |
| N-10 AS 排序 | 照歷史保留，新頁成稿時按號排 |

**這一版**：再送 Fable 5.2／high 敵對式檢查。

## 26. 1622f4a7 的敵對式檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。**結果**：blocker 3、should 9、nit 9，證據 `declaration-1622-adversarial-check.md`。**5.5 的核心演算法和 Bevy 一致，檢查者逐一推演沒有找到競態**；剩下的是殘句、突變清單、對照表的準確度，以及改裝帶出的四個洞。全部採用，沒有要交使用者的。

| 條 | 處理 |
|---|---|
| B-1 AS-4 與殘句 | AS-4 改成「值只在 store 的鎖下、由完成的工作寫進表」；4.2、5.17、第 10 節的殘句改 |
| B-2「登記不能在 System 裡」的殘留 | 5.16、第 10 節、第 11 節刪掉；第 10 節改成「登記後等待者變 `Queued`、載好」 |
| B-3 突變清單 | 改成八個、各寫專屬的預期失敗與對應案例；(1)(2) 合併；自鎖與持鎖解碼改成有界等待的斷言；AS-10 (b) 的案例改成「移除→再請求→舊工作先完成」；補「救回後再放掉要移除」 |
| S-1 請求級 Handle 的出處 | 對照表改標改裝，出處 `server/mod.rs:385-389` |
| S-2 對照表不完整 | 加「交工作給執行器」「依賴何時排」「在飛上限」三列，「表與鎖」「預先登記的鍵」改寫；5.5 寫在飛上限的排隊、開工前比生命序號、名額；依賴在父項解碼完才排的代價 |
| S-3 `BadSettings` 沒地方放 | 解讀結果存在 catalog 項目；「沒有 info」一支加 `Failed(BadSettings)` |
| S-4 嘗試序號歸零 | 移到 catalog 項目，跨生命遞增 |
| S-5 舊視圖配新生命、Handle 比視圖新 | Handle 與視圖都帶生命序號；「有核心而視圖沒有相同（id、生命）」一律 `Queued`；`AssetRequests` 的本幀清單拿掉；第 10 節加案例與突變 (8) |
| S-6 AS-11 的持鎖範圍 | 寫成實際清單（含加邊、重算強連通分量、建視圖內容）；settings 在鎖外解讀；舊視圖、舊值放鎖後才放；`Load` 讀 loader 表寫明；讀寫鎖 |
| S-7 M11 的條件與加註 | 條件加「System 不直接呼叫 `AssetStore`」；負向編譯列出樣本；雙向的解釋；第 15 節第 11 項請主會話加註 |
| S-8 回滾重跑步的 `Load` | 「命中或救回」；5.8 的指向改；第 14 節的限界 |
| S-9 依賴重載後父項 | 「連同依賴」每次拉視圖由邊重新推導，會回到 `Complete`（GYO 的選擇） |
| N-1 殘詞 | 配槽、單一處理點、盡力、`world_logging` 先例 |
| N-2 行號 | `server/info.rs:782`、`:778`、generation 在 `assets.rs:18-41` |
| N-3 拉取清單 | 補失敗內容、依賴邊、略過與循環標記、生命序號 |
| N-4 AS-1 守衛 | 加「`BuildNewFirst` 或 `CarryOver`」 |
| N-5 突變 (6) | 寫明假核心帶當前生命序號（現為突變 (5)） |
| N-6 鎖的種類 | 讀寫鎖：拉視圖拿讀鎖，其餘拿寫鎖 |
| N-7 X-5 | 佇列式、不遞迴 |
| N-8 告知時代的說法 | 改成 `Ready` 列出 |
| N-9 重試當幀 | 5.7 寫明那一幀看到的是上一次的 `Failed` |

**這一版**：再送 Fable 5.2／high 敵對式檢查。

## 27. 0cadb4ca 的敵對式檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。**結果**：blocker 2、should 10、nit 12，證據 `declaration-0cad-adversarial-check.md`。新加的 5.5 重新推演沒有競態，Bevy 引文都對得上；剩下的是突變的殺法、AS-11 的條件、測試的細節與殘句。全部採用，沒有要交使用者的。

| 條 | 處理 |
|---|---|
| B-1 突變 (6) 殺不掉 | 測試建置的 store 鎖記擁有者，同執行緒重入是 `GYO_ASSERT`，預期失敗是那條斷言的訊息 |
| B-2 AS-11 的條件與「System 裡可以登記」 | 登記經只有登記函式的 `AssetLoaderRegistry`（O-A2 交出的是它）；條件收窄成「System 不呼叫 store 的讀取與維護入口」 |
| S-1 `BadSettings` 的兩條路 | 5.4「轉成 `Queued` 的共用規則」，三處都經它；第 10 節加案例 |
| S-2 AS-11 條文、第 8 節理由 | AS-11 引用 5.6 的持鎖清單；「loader 表」列理由改成「要同時看兩張表」 |
| S-3 別的 World 的維護在幀中途移除 | 5.6、5.8 的「不會重讀」「命中或救回」加條件；AS-11 寫明 Handle 的生命序號是唯一帶出的當下狀態；5.5 末表與第 14 節寫這個改裝的後果與作者指引 |
| S-4 在飛上限沒測試 | 第 10 節加一組與突變 (9) |
| S-5 突變 (3) 殺不掉 | 加中間斷言 |
| S-6 scenario 第 3～5 步 | 第 3 步結尾第二個 World 放掉並跑過套用點 |
| S-7 祖先案例 | 改成第一次指定失敗、作者觸發重載；加「邊級失敗不回復」「父項失敗時依賴不載」兩案；Bevy 不回復（`server/info.rs:708-709`）列為差異 |
| S-8 AS-11 的負向編譯樣本 | `static_assert(!requires{...}, "<標記>")` 的形狀、各帶標記與對照組；突變 (10) |
| S-9「會自癒」 | 版本號存在 `AssetView` 裡，被換掉就重拉；不在就重建 |
| S-10 第 8 節缺兩列 | 補「結果怎麼進表」「依賴何時排」 |
| N-1～N-12 | 告知殘句、`NotRequested` 只在測試讀法、條數、(B)5 預先登記期間的衝突、嘗試序號的規則與日誌的鍵加失敗碼、7 次的筆數、第 4 批行號以 aa2dee2f 為準、殘詞、有界等待 2 秒與兩案步驟與 `RunScheduleSteps`、登記的先後、父項失敗的案例與第 2 步的內容、同型別名只有一個有 loader 的限界 |

**這一版**：再送 Fable 5.2／high 敵對式檢查。

## 28. 3fd3e85f 的敵對式檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。**結果**：blocker 1、should 7、nit 6，證據 `declaration-3fd3-adversarial-check.md`。核心演算法沒有改動；新加的部分重走沒有競態。全部採用，沒有要交使用者的。

| 條 | 處理 |
|---|---|
| B-1 負向編譯樣本寫不出來、極性相反 | AS-10 (a)、AS-11 的樣本照先例的極性：用 concept 讓它相依、`static_assert` 斷言禁止的性質成立、以標記失敗、有對照組（檢查者以 Apple clang 21 實測）；突變 (10) 的預期失敗改成「variant HANDLE_STATE compiled」 |
| S-1 `AssetLoaderRegistry` 的殘留與取得方式 | 2.2、5.4、5.16、第 6 節、12.3、第 14 節統一；System 要登記時，擁有型別的 Module 在準備函式把它放進自己的 Resource；介面含預先登記；R1 補上 |
| S-2 突變 (6) 的「測試建置」 | 鎖的擁有者檢查一律開，5.8 的 E0 加「同執行緒重入」；突變 (6) 寫明是 `Load` 路徑與當場跑完的執行器，完成路徑不宣稱 |
| S-3 開工與完成分開控制 | X-5 改成分開控制，或用慢的 loader 的閂加真執行緒；在飛上限由測試 loader 的並行計數觀察 |
| S-4「解碼中不擋 `Load`」的步驟 | 第 10 節寫出步驟（閂、2 秒、斷言後一定放閂並 join） |
| S-5 (B)5 的方向 | 三個方向都寫、預先登記也回傳結果、各一案；Bevy 行號改正 |
| S-6 加註少了例外 | 第 15 節第 11 項補「只能帶回不進雜湊、不進相等的身分資訊」 |
| S-7「Handle 要一直持有」 | 進 AS-7、第 15 節第 10 項的跨組件規則；寫成「沒有用 `rollback.` 登記的 Resource」 |
| N-1～N-6 | 殘句；第 4 批分支頭 15e79bd1；「依賴何時排」的決勝理由改成「父項失敗或取消時沒有要收回的依賴、加邊只在一處」並列出退掉的；`BadSettings` 案例的語序；`requests` 給出 store 的樣本；幀中途移除只在自訂 runner 交錯或第 5 批並行時發生 |

**這一版**：送 Fable 5.2／high 做收尾的敵對式檢查。

## 29. ae5e6150 的收尾檢查與處理

**檢查者**：Fable 5.2／high，唯讀（作者 Opus 5.5）。**結果**：**blocker 0**、should 5、nit 5，證據 `declaration-ae5e-adversarial-check.md`。檢查者的結論：可以交動工前核准。負向編譯樣本的新形狀以 Apple clang 21 實測成立；十個突變逐個推演都有專屬的預期失敗。剩下的十條各一兩句，全部採用。

| 條 | 處理 |
|---|---|
| S-1「`AssetView` 不可指派」做不到 | 改成「可移動、不可複製、建構子私有，作者造不出來」；樣本加 `is_copy_assignable_v` 與作者可建構 |
| S-2 兩個 World 同時請求今天做不到 | 真執行緒拆成 store 層（N 條執行緒直接對 `AssetStore`）與 World 層（照第 5 批，串行時由 store 層擔保） |
| S-3 三個「持鎖不做」沒有守衛 | 測試執行器、測試型別的解構子、測試 loader 的 settings 解讀函式各回呼 store，持鎖時就是重入斷言；突變 (11)～(13)；「持鎖讀檔」列入只靠審查 |
| S-4 加註比自己做的嚴 | AS-11 與第 15 節第 11 項加「登記的回傳值是設定錯誤的回報，System 不得拿它分支判定」與通則「帶回的值不得讓判定依時序而變」 |
| S-5「以標記失敗」會被無關硬錯誤騙過 | asset 的判法比對「靜態斷言失敗＋標記在同一行」；對照組同寫法；每個樣本配一個「加上那個成員」的突變；先例的弱點報主會話（第 15 節第 12 項） |
| N-1～N-5 | `AssetLoaderRegistry` 的殘留；件數加 `BadSettings`；(B)5 的整筆不記、冪等、登記失敗後的等待者；master fixture 從 origin/master 3b411e21 複製；執行器的介面由 asset 自己定 |

**狀態**：可以交動工前核准。

## 30. 實作時的調整

第 1～5 步實作（a5d6bdf7、ed2e70b8、26a77b38、318f6699、6ab6d72e、8d3ca694）之後，和本頁不同、或本頁沒寫清楚而在實作時判的地方。標「卡片照 L17 判」的照命名規範、別家原始碼與已定原則判，不是使用者的話。都沒有放寬第 7 節的 Design Invariants、第 10 節的判定；放寬或收緊的地方逐項寫明。正式頁 [asset.zh-Hant.md](asset.zh-Hant.md) 照實作的事實寫。

### 30.1 和本頁不同的

| # | 本頁原寫 | 實作 | 理由 | 落點 |
|---|---|---|---|---|
| 1 | 音效 loader 在「world_audio 那批」（2.2；9e45eaff 改的字） | 「audio 那一側的小批」 | world_audio 已撤（使用者 `v8-arch-gyo2-world-audio-withdrawn`）；2.2 已改字並加註 | 2.2 |
| 2 | 雜湊函式在 world_asset（R1、5.1、5.9.1） | `AssetId`、`Handle<T>`、`AssetRef<T>` 的雜湊函式在 asset 函式庫，寫成受約束的泛型 `template <class Sink> requires Detail::HashesText<Sink> void HashValue(Sink&, const X&)`；`AssetRequests`、`AssetView` 的（什麼都不寫）在 world_asset | Kernel 的客製點經 ADL 找 `HashValue`，必須和型別同一個 namespace（`GYO::Engine::Asset`）；world_asset 若在自己的原始碼裡開 `Asset` namespace，會違反 naming 守衛（namespace 要對得上單位）。寫成泛型、以「Sink 能用自己的雜湊函式寫字串」約束，asset 就不必依賴 Kernel（Kernel 的 `Hasher` 符合） | `AssetId.hpp`、`Handle.hpp`、`AssetRef.hpp`；正式頁第 8 節 |
| 3 | catalog 的錯誤記著，到 world_asset 第一個跑到的 `Finish` 回報（5.3 驗證時點、5.4、5.8） | 在 `Configure` 用 `ProductSetup::Report` 回報（`WA-E1`；文字 loader 登記失敗是 `WA-E2`）；store 照樣以空 catalog 建好 | #270（`v8-arch-gyo2-exchange`）給了 `Configure` 的回報入口，就是 O-A3；有它就不建任何 World，不必繞到 `Finish` | `WorldAssetModule.cpp`；測試 `view: content that breaks the catalog's rules is a startup error reported from Configure` |
| 4 | Module 拿到 `AssetLoaderRegistry`、`AssetOptions` 的路交編排層（O-A1、O-A2、O-A4，12.3 第 13 題） | #270 的 Product 共用處與選項：`ProductSetup::Options<AssetOptions>()`（O-A1）、`setup.Buffer().Create<AssetStore>`／`Create<AssetLoaderRegistry>`，擁有型別的單位 `Find` 它（O-A2）、`setup.OnDestroy` 裡 `Shutdown(Discard)`（O-A4） | 編排層的形狀定了，照它接；E3 的丟棄件數仍沒有可寫的 World（見 dev_log 的未做） | `WorldAssetModule.cpp` |
| 5 | `AssetView::State(handle)` 回「`Failed`（帶 `AssetLoadFailure`）」（5.7） | `State` 回 `AssetState`；失敗另由 `Failure(handle)` 取（請求級或 id 級，沒有就空） | 狀態是列舉、失敗是帶訊息的值，`State` 只回列舉，`Ready`、`Unfinished` 的比較也只看列舉；失敗要細節時另讀（卡片照 L17 判） | `AssetView.hpp` |
| 6 | 套用點的 System 經交換點接上（第 9 節第 4 步「接交換點」） | 套用點是 world_asset 自己的 System `ApplyAssets`，不用交換函式 | 交換點宣告 E-6：交換點不認得 asset，World 靠自己的套用點看到結果（X-2）；asset 的套用 System 是交換點宣告「套用點」一形的實例 | `WorldAssetModule.cpp` |
| 7 | 執行器由交換點或 runner 給（X-1、X-4） | 產品自己建 threads 的 `WorkerPool`，經 `AssetOptions::executor` 交給 world_asset；沒給時需要排工作的請求是 id 級 `Failed(NoExecutor)`，不是啟動錯誤（9e45eaff 已寫進 5.4） | world_asset 是 default=yes，不載資產的產品也帶著它；store 不在呼叫者的執行緒上代跑（卡片照 L17 判） | `AssetExecutor.hpp`、`AssetStore.hpp`、`WorldAssetModule.hpp` |

### 30.2 本頁沒寫清楚、實作時判的（卡片照 L17 判）

**World 端（第 3 步）**
- **`UntypedHandle`**：`Ready`、`Unfinished` 收一組不同型別的 Handle，用 `UntypedHandle`（從任何 `Handle<T>` 隱式轉來，持有同一個核心）。
- **`Load` 是 `const`**：`AssetRequests::Load` 是 const，System 以 `Res<AssetRequests>` 呼叫。store 有自己的鎖，`AssetRequests` 本身沒有要改的狀態；用 `ResMut` 會讓所有請求的 System 互相排先後。
- **`WorldAssetOptions`**：套用點掛在哪由每個 World 的 `WorldDescription::options` 給（`applySchedule`、`applyStage`，預設 `Main`/`First`）。預設骨架關掉的 World 用它指定（5.6「產品自己掛」的落點）。
- **`AssetRequests` 被移除時不重建**：`ApplyAssets` 看到 `AssetRequests` 不在就什麼都不做；`AssetView` 不在則重建（5.4 的自癒）。`ApplyAssets` 經 `AssetRequests` 拿到 store（程式的註解：「With no AssetRequests there is no store to reach」），它不在就沒有 store 可拉；`AssetView` 是讀的結果，可以從 store 重建，`AssetRequests` 不是。
- **`ReferencePath` 的 id 由 store 注入**：`LoadContext` 收一個可選的 `ContentPathIndex`（catalog 的路徑 → id），store 建 `LoadContext` 時給 catalog 的索引，`ReferencePath` 當場回帶 id 的 `AssetRef`，或記下 `UnlistedReference`／`AmbiguousReference`。直接呼叫 loader（沒有索引）時只記路徑、id 空。這樣 loader 的介面在兩種用法下相同（AS-3）。
- **`ParseSettings` 收 JSON 文字**：loader 的 `ParseSettings(std::optional<std::string_view>)` 收 `settings` 的緊湊 JSON 文字（沒寫時是空），不收 JSON Module 的值。asset 對 json 是私有依賴，loader 的公開介面不帶 JSON Module 的型別；loader 要用 JSON Module 解讀就自己連。

**store（第 2 步實作者的自判）**
- **「連同依賴」的優先序**：自己或任何遞迴依賴還沒結束 → `Incomplete`（即使已有失敗）；全部結束、有失敗 → `Failed`；其餘 `Complete`。5.17 只列三種狀態，沒寫同時成立時誰優先。照「`Failed` 是結束後的結論」判，和 `DependenciesFinished` 一致。
- **`maxInFlight` 預設 0＝不設限**：3.3「預設不設限，標明不是判定」的落點。
- **建 store 時 catalog 有重複 id 是 `GYO_ASSERT`**：`AssetStore` 的建構收「已照 (A) 驗過」的項目；重複 id 是呼叫者的程式錯誤（E0），不是內容錯誤。內容的重複由 (A) 3 在讀 catalog 時回 `DuplicateId`。
- **`Shutdown(Drain)` 的語意**：先等到沒有排隊與在飛的工作（含完成的工作排進來的依賴），要求執行器還在跑；之後和 `Discard` 一樣停。只有第一次呼叫有作用；不能從工作裡呼叫；~~不等已在跑的工作（它們發現 store 已關，什麼都不留）~~（30.3 B2 改成兩種模式都等已開工的工作跑完）。X-6「能停下或等完」的落點。
- **強連通分量把已標循環的邊也算進圖**：已標 `CyclicReference` 的邊不持有，但重算時仍是圖的邊。不算的話，經已標循環的邊閉合的新環會依完成順序有時被標、有時不被標（AS-5）。守衛：`depend: a cycle closed through an edge already marked is marked too`，突變 `as-store-cycle-skips-marked-edges`。
- **等價測試的批次**：(2) 反序改成每批 5 筆、直到做完（依賴會排出更多工作，所以多於宣告的 4 批）；(3) 交錯照 3、3、3、3、3、3、2 筆之後再跑完其餘。判定（和自己的基準比、不比生命序號與嘗試序號）不變。
- **件數多了幾項**：`newLivesNoExecutor`、`referenceNewLivesNoExecutor`、`jobsWaiting`、`jobsInFlight`、`fileReads` 與維護的 `dropsTaken == dropsStale + dropsSkipped + removals`。5.8 的等式在沒有排隊與在飛時成立，這是同一條等式加上進行中的兩項，沒有放寬。

**catalog（第 1 步實作者的自判）**
- **讀到第一個錯就停**：回那一條規則的碼，不收集全部錯誤。範例集每份只違反一條，判定不受影響；手寫 catalog 的作者一次只看到一個錯。
- **catalog 檔沒有大小上限**：`ReadContent` 以 `std::numeric_limits<std::size_t>::max()` 讀；catalog 是產品寫的內容，沒有自己的上限（RC-AS8 的成本另量）。
- **由副檔名推出的 `format` 不驗**：(A) 7 只驗「有寫的 `format`」是 `[a-z0-9]+`；推出來的不驗（副檔名轉小寫後照用；沒有 loader 認它時，請求是 `NoLoader`）。
- **寫入多驗保留欄位**：`WriteContentManifest`、`WriteCatalog` 除了 (A) 規則，還驗保留欄位不和已知鍵或彼此重複、本身是 JSON（`WriteFailed`），保證寫出的讀得回來且相等。

### 30.3 合併前設計審查的處理（2026-11-23）

Fable 的合併前設計審查（blocker 2、should 8、nit 7；摘要與探針在主 checkout `build/evidence/gyo2-asset/design-review/`）之後，主會話逐條判定，實作者（Opus 5.5／high）照判定做。正式頁照實作的事實改；Design Invariants 的文字仍等合併前核准。

| # | 審查指出 | 處理 | 落點（測試／突變） |
|---|---|---|---|
| B1 | `LoaderTests.cpp` 靠間接含入拿 `std::ostream`，windows-x64 的 MSVC 編不過 | 用 `<<` 給 doctest 的測試檔與兩個支援標頭都直接含 `<ostream>` | `LoaderTests.cpp`、`CatalogTests.cpp`、`ContentPathTests.cpp`、`StoreSupport.hpp`、`WorldAssetSupport.hpp` |
| B2 | `Shutdown(Discard)` 返回時工作還在 loader 裡；world_asset 的 `OnDestroy` 返回時工作沒停，違反 O-15 | 兩種模式都等「已開工、未完成」的工作歸零（開工時在鎖下加一，`Complete` 之後減一，條件變數等）；`Discard` 仍不開新工作；值在返回前解構。30.2 的「不等已在跑的工作」因此改掉 | `store: Shutdown waits for a job that is already in its loader, in both modes`（另一條執行緒 100 ms 後放閂，閂本身 10 s 有界）；`as-store-shutdown-waits-running` |
| S1 | 已 `Failed` 的依賴被之後完成的每個父項重載 | 保留（核准的 5.5，同 Bevy）；正式頁第 4 節寫明，AS-13 加守衛 | `depend: a failed dependency is read again by each parent that finishes after it (5.5, as Bevy)`；`as-store-edge-reloads-failed` |
| S2 | `Handle::Body()`、`Life()` 公開；store 在 Product 的共用處，作者的 Module 可 `Find` 它呼叫 `Request`／`Read`／`Pump`／`Shutdown` | `Body()`、`Life()` 改 private，store、world_asset、測試經 `Detail::HandleAccess`；store 由 `WorldAssetModule` 持有（`unique_ptr`），共用處只 `Create` `AssetLoaderRegistry` 與唯讀的 `AssetStoreDiagnostics`（`Counts()`、`Probe(id)`；`Probe` 是實作者為了測試讀單一 id 而加，使用者 2026-11-23 核定「Probe 留著」）。測試的 `Find<AssetStore>` 改成 `Find<AssetStoreDiagnostics>`；`SummaryTests.cpp` 與突變 `aw-hash-index` 改經 `Detail::HandleAccess` | 樣本 `GYO_ASSET_SAMPLE_HANDLE_BODY_IS_PRIVATE`、`GYO_ASSET_SAMPLE_HANDLE_LIFE_IS_PRIVATE`；`aw-sample-handle-body`、`aw-sample-handle-life` |
| S3 | `AssetExecutor.hpp` 說 threads 的 `WorkerPool`「照原樣接得上」，但 gyo2 上沒有它 | 註解改成事實；正式頁第 4、9 節與 dev_log 加限界：今天沒有正式的執行器，產品自己寫 adapter；`WorkerPool` 合進來時把 adapter 定在一處並加真 pool 的案例。之後合併 gyo2 時 #277 已帶進 `WorkerPool`：加了真 pool 的 scenario 案例（測試裡的 adapter）；adapter 定在 world_asset（使用者 2026-11-23「adapter 照建議放 world_asset」，核准後加入）：`WorkerPoolExecutor`，測試改用它 | `AssetExecutor.hpp`；`world_asset/WorkerPoolExecutor.hpp`；正式頁；`tests/common/world_asset/ScenarioTests.cpp` |
| S4 | 已登記的 loader 再預先登記它不認的 `format` 會被記下，那個 `format` 的請求永遠等 | 最小修法：這時回 `FormatNotTaken`（新碼，什麼都不記）；同一 loader 再登記時清掉它的預先登記（`InstallLoader` 的「同一 loader 再登記」不再提前返回）。不改成「直接用 `kFormats`」（會改核准的 5.4） | `store: preregistering a format its registered loader does not take is refused`；`as-store-preregister-not-taken` |
| S5 | 探針存活的決定與「測試已在、清單沒有突變」的機制 | 存活的 8 個：r1、r4、r5、r9、r10、r16、r20 加案例或斷言與突變；r19 見下。已有測試的 7 個掛成突變。AS-1 加專屬案例與突變（每個 World 各建一個 store） | r1 `as-store-noloader-then-waits`；r4 `aw-waiting-finished`；r5 `aw-request-failure-complete`；r9 `aw-get-ignores-cpp-type`；r10 `aw-unseen-dependencies-finished`；r16 `as-snapshot-root-failed`；r20 `aw-world-options-ignored`；7 個 `aw-prepare-no-maintenance`、`aw-apply-no-maintenance`、`aw-no-on-destroy-shutdown`、`aw-prepare-no-view`、`as-store-edge-holds-nothing`、`as-store-pump-one-round`、`aw-view-rebuilt-empty`；AS-1 `aw-one-store-per-world` |
| S6 | 骨架關掉、沒給 `WorldAssetOptions` 時的錯誤看不出是 asset | 照 world_time 的先例加案例：給了選項正常 `Loaded`；沒給時是 Kernel 的 D-E1，啟動報告寫「System 'ApplyAssets' is mounted in schedule 'Main', which this Product does not have」，已經看得出是 world_asset 的 System，**不另加回報碼**（卡片判） | `view: a World with the skeleton off names where ApplyAssets goes (WorldAssetOptions)`；`aw-world-options-ignored` |
| S7 | 兩份突變清單要在分支頭完整跑一次 | 分支頭各完整跑一次，輸出在主 checkout `build/evidence/gyo2-asset/final/` | dev_log |
| S8 | `docs/cross-component-rules.md` 的三條沒登記 | 登記 CCR-8（AS-10 (c)）、CCR-9（判定用的內容在進判定前載完）、CCR-10（判定用內容的 Handle 一直持有，放在沒有用 `rollback.` 登記的 Resource），owner asset | `docs/cross-component-rules.md` |
| N1 | 突變的 `guards` 指宣告的 DI | 兩份清單的 `asset-declaration AS-n` 改成 `asset AS-n`（守第 5 節規則的照舊指宣告） | 兩份 `mutations.json` |
| N2 | dev_log 說雜湊成本沒量（同頁已記約 150 ms）；正式頁與 dev_log 說 MSVC、GCC 未實測 | 刪掉「雜湊成本沒量」；改成 CI 的事實（L1 三平台的 configure 照判法通過；linux-x64、macos-arm64 通過，windows-x64 修正 B1 後看 L1） | 正式頁第 9 節、dev_log |
| N3 | 「內容根目錄＋相對路徑」兩處各寫一份；相對的根目錄隨工作目錄變 | 收成 `ContentPath.hpp` 的 `JoinContentRoot`，catalog 的讀取與 store 共用；建 store 時把相對的 `contentRoot` 經 io（`NativeFileUri`、`NativePathFromFileUri`）轉成絕對路徑 | `ContentPath.cpp`、`Catalog.cpp`、`AssetStore.cpp` |
| N4 | `MarkCycles` 每次都掃全部 info；`Ready` 的去重是 O(n²) | 完成的項目沒有圖的邊（持有或已標循環）就提前返回；`Ready` 用集合去重 | `AssetStore.cpp`、`AssetView.cpp`（行為不變，沒有加突變） |
| N5 | `Catalog.cpp:253` 的 `text` 遮住參數（MSVC C4457） | 改名 `bundleText` | `Catalog.cpp` |
| N6 | `ReadWholeFile` 與 `Sha256` 丟例外（`bad_alloc`）時名額不還 | 包進 `try`，丟例外是 id 級 `ReadFailed`（讀檔這一步的碼，不是 `LoaderThrew`），名額照還；`LoadContext::References()` 的複製也移進 loader 的 `try`。開工的計數用 RAII 減，例外離開 `Run` 時 `Shutdown` 也不會永遠等 | `AssetStore.cpp`（注入 `bad_alloc` 需要測試掛鉤，沒有加案例與突變） |
| N7 | `AssetStore::Read` 不查 Handle 是不是這個 store 的 | 比 `body->drops` 與 store 的佇列，不是就回空 | `store: a Handle of another store reads as null`；`as-store-read-other-store` |

**r19（「已登記的 loader 不再記成預先登記」）沒有加突變**（卡片判）：同一個（`type`、`format`）已有 loader 時，判定（`Judge`）、引用的略過與登記的衝突檢查都先看 loader 表；loader 不會被取消登記，所以多記的預先登記沒有任何讀出點，突變和原碼等價。S4 的「再登記時清掉預先登記」同理：有了 `FormatNotTaken`，已登記的 loader 不會再留下預先登記，清掉的那一段今天也沒有讀出點；它是防守，不立突變。兩者都記在 dev_log。

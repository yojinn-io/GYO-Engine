# 事前宣告：asset Module（`v8-arch-gyo2-asset`）

更新：2026-11-21（修訂 3）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：使用者的決定都已寫進；送敵對式檢查，之後交動工前核准。實作要等第 5 批的「多個 World、交換點」與「生成世界（重做）」做完，而且使用者說開始。**

**經過**：
- 修訂 0（f93edb7e）送 Fable 5.2／high 敵對式檢查，blocker 4、should 15、nit 12，全部採用成修訂 1（b050e13b），對照在第 17 節。
- 修訂 2（7adbcb77）：使用者要「參考其他家如何做 assets 的管理」。卡片補了管理面對照，經敵對式檢查（blocker 4、should 9、nit 4）改正後寫進第 4 節，對照在第 18 節；使用者選了 (a')。
- 修訂 2 送 Fable 5.2／high 敵對式檢查，blocker 3、should 12、nit 10（證據 `declaration-r2-adversarial-check.md`），處理在第 19 節。
- **修訂 3**：使用者在 asset 卡的 side chat 定了八件（12.1 決定 4～11）：時間上限放寬；跨執行緒約束現在寫進契約；**作者只用 id**；**資產管理在 World 之外**；大地圖先照 Bevy；第 4 題（到了就可見＋就緒查詢）；第 2 題（原始位元組雜湊，預設開）；**依賴由 Engine 推出**。本頁的形狀因此重寫：持久鍵改成 catalog 的 id；`Assets<T>` 不再每個 World 一份；CarryOver、時間窗口、失敗搬回、訊息對帳那一套整個拿掉。

**性質**：實作段第 6 組的 Module 的事前宣告。涉及：
- AGENTS §2 的基盤 Architecture；
- §3 的 Architecture Delta：新 Subsystem、改寫已核准的 Design Invariants、推翻 11-11 方向 2（使用者已同意）、新 Data Contract（catalog）；
- §4 的系譜與決勝理由；
- §6 的外部組件 adapter；
- §8 的 Data Contract。

**用詞**：照日誌、trace 的宣告，即設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。使用者的決定、卡片的判定、待使用者定的，分開記在第 12 節。範圍的代號用 R1～R10，Design Invariants 用 AS-1～AS-12，兩者不混。

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
| 時間上限放寬（2026-11-21，side chat 轉來） | 設計段放寬到約 11 h，修訂 3 全部修再送檢 | 0.1 |
| 跨執行緒約束現在寫進契約（同上） | 多執行緒是已定前提，不等 RC 才寫 | AS-10、AS-11 |
| 作者只用 id（同上） | 持久鍵＝id；id→路徑對照表的機制與格式歸資產管理，執行期可用 | 5.2、5.3、AS-5、AS-6、AS-12 |
| 資產管理在 World 外（同上） | World 只持 id；換 World、多 World 不是 asset 的事；推翻 11-11 方向 2 | 5.4、AS-1、AS-4 |
| 大地圖先照 Bevy（同上） | 不內建串流；日後用 Module 加 | 2.2、RC-AS1 |
| 第 4 題：到了就可見（同上） | 單筆到了就讓 World 看到（照 Bevy）＋Engine 提供一組 id 的就緒查詢；轉場歸作者 | 5.6、5.7、AS-5 |
| 第 2 題：原始位元組雜湊（同上） | asset 對每個 id 的原始位元組算雜湊，作為能力，預設開、可關；比不比與不同時怎麼辦歸作者或網路 Module | 5.9、AS-7 |
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

side chat 轉來的八件，卡片已照四樣轉主會話記 HANDOFF（2026-11-21）；記好之前，HANDOFF 裡查不到它們。

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
| 設計段 | 原估 4～6 h；使用者放寬到約 11 h。到修訂 2 約 8.5 h；修訂 3 的重寫（含決定 9～11 的依賴、雜湊、就緒查詢）＋送檢估 3.5～4.5 h，合計約 12.5 h | 決定 6、7、11 讓修訂 3 從「修 25 條」變成重寫形狀，超過放寬後的約 11 h。到 13 h 還收不了就停下回報 |
| 實作段 | 28～42 h（推定；修訂 2 是 22～34 h） | 多了 catalog 的 Data Contract、World 外的 store 與無鎖的請求路徑；交換點與生成世界的形狀還沒定，它們的宣告核准後重估 |

## 1. 這一批做什麼（先用遊戲講）

FPS 的 Client 進對局之前，要載角色模型、武器貼圖、字型、音效、規則與地圖的 JSON。

**內容先有 catalog**：一份 catalog 檔（Data Contract）寫「id `rules.movement` 是文字，在 `content://rules/movement_rules.json`」「id `char.hero` 是模型，在 `content://models/hero.fbx`」。catalog 由工具、Script 或手寫產生（AGENTS §8）。作者**只認得 id**，不管檔在哪、怎麼解碼。

步驟：
1. 生成世界階段，作者的 System 對 `AssetRequests` 說：「我要 `rules.movement`。」**當場**拿到 `Handle<Text>`。
2. 讀檔與解碼由 World 外的資產管理（`AssetStore`）在 World 執行緒之外做，不碰 World。
3. 載好的值放進資產管理，**不可變**。
4. 每個 World 在自己的套用點（預設 `Main`/`First`）換成資產管理最新的一份唯讀視圖；一幀之內看到的都是同一份。同時收到「加入了」的 `Message`。
5. 作者的 System 看狀態，全部載完才離開生成世界——等不等是作者的事。

**失敗時**：例如檔案不在，狀態是「失敗」，帶 `AssetLoadFailure`（E2）。asset 不記日誌、不自己重試，World 照跑；作者決定停下、再請求、換預設內容或顯示錯誤。日誌要記，就訂閱不分型別的 `AssetLoadFailed`。

**中途讀檔、換 World**：不是 asset 的事。資產管理在 World 外，Handle 的引用計數跨 World 有效。作者照 O-8 在現在的 World 驗證存檔要的 id 都載得到，再發 `ReplaceRequest`；新 World 請求同樣的 id，資產還活著就直接命中。

**Client 的 Sim 與 Present 都用角色模型**：兩個 World 請求同一個 id，資產管理只載一次、只存一份，兩邊唯讀共用。

**模型檢視器的 `info` 指令**：不建 World，只連不認得 ECS 的 `asset` 函式庫與模型 Module 的 loader；把位元組交進去，拿回值。

## 2. 範圍

### 2.1 這一批做的（動工前核准之後；實作等第 5 批）

| 代號 | 內容 |
|---|---|
| R1 | 兩個單位（照 `logging`／`world_logging` 的先例；卡片照 L17 判，第 8 節）：**`engine/modules/asset`**（`GYO::Asset`，library-only，不認得 ECS）——id 與 catalog（Data Contract 的讀、寫、驗證）、loader 介面、整檔讀取（經 io）、文字 loader、`AssetStore`（World 外的資產管理）、`Handle<T>`；**`engine/modules/world_asset`**（`GYO::WorldAsset`，編排層 `Module` 的子類）——擁有 Product 的 `AssetStore`、每個 World 的唯讀視圖與請求入口、套用點的 System、訊息、雜湊函式 |
| R2 | catalog：id→（型別、路徑、settings）的對照表；格式、Version、Validation Rule 是 asset 擁有的 Data Contract；執行期讀（不是 build 的產物） |
| R3 | `AssetStore`：以 id 請求、引用計數、沒人用就放掉、`Failed` 之後可以再請求；載好的值不可變、共享唯讀；**依賴追蹤**（loader 宣告引用、連同依賴的狀態、循環偵測）；**原始位元組雜湊**（預設開） |
| R4 | loader 的介面與登記入口：擁有型別的 Module 登記（5.16） |
| R5 | 讀檔與解碼在 World 執行緒之外；World 端的請求、放掉、讀取不上鎖 |
| R6 | 每個 World 的唯讀視圖、套用點、狀態查詢、每型別的 `AssetMessage<T>{Added, Removed}`、不分型別的 `AssetLoadFailed`、一組 id 的**就緒查詢**（連同依賴） |
| R7 | 錯誤：`AssetLoadFailure`（E2）、catalog 與登記的啟動錯誤、丟棄記錄（E3） |
| R8 | asset 放進 World 的型別的雜湊函式（5.9.1；第 4 批 3.3） |
| R9 | 上限與擴張點：每次發佈的上限、在飛上限、loader 的位元組上限可由產品覆寫 |
| R10 | 新頁 `target/v2/asset.zh-Hant.md`：Design Invariants、Replacement Criteria。舊 `target/assets.zh-Hant.md` 頂端標「已由 v2 取代」 |

### 2.2 不在這一批

- **事先轉檔的工具與它的資料契約**（三件之 3）。本批只保證接得上（5.10）。
- **image Module 與 stb_image**：隨 SDL 那批的第一個使用者做（12.2 第 5 題）。本頁先定位置、授權與 adapter（5.11）。
- **其他 loader**：模型（ufbx，進 model Module；SDL adapter 與 ufbx 歸哪一類，HANDOFF 未定）、音效（audio 重審那批）、字型（text Module，SDL 那批）、二進位（沒有點名的 Feature）。
- **GPU 資源**（Bevy `RenderAssets<T>`）：歸 render，隨 SDL 那批。
- **大地圖的串流**（決定 8）：照 Bevy 不內建。asset 只給上限與 region 級的 id；串流的規則與「進判定」屬日後的地圖 Module（RC-AS1）。
- **沒有點名 Feature 的，本批不做，也不在介面上先留形狀**：熱重載與 `Modified`（model_viewer 的 `RefreshContent` 是手動重讀，工具可以直接呼叫 loader）、一個檔分出多個子 asset、作者在執行期自己加值（Bevy `Assets::add`）、資料夾自動產生 catalog（產品可以在程式裡加 catalog 項目，5.3）。
- 交換點、生成世界本身的設計（第 5 批）。threads、io 的重審（批次表第 6 組）；本批只交需求（5.6、5.12）。

**實作的依賴**：編排層（#244）；第 5 批的多個 World、交換點；第 5 批的生成世界（重做）；第 4 批合併（雜湊客製點、`Summary` 的規則）；JSON Module（catalog 的格式）。

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
| F2 | 遊戲 Match（無畫面）：載角色模型與規則算判定（`PlayerAnimationContent.cpp:89-90`） | fbx、json | **不需要 SDL 與視窗**也能載；兩端載出的內容身分由作者比，asset 給原始位元組雜湊 | 機制做（決定 10） |
| F3 | 讀檔（`-restore-entry-scope-player-save`）：遊戲中讀檔＝替換 World | 同 F1 | 替換前能先驗證內容（O-8）；替換時不必重讀；**存檔裡的參照跨版本有效** | 做：id 是持久鍵（5.2）；換 World 不是 asset 的事（5.4） |
| F4 | 模型檢視器：`info`、`models` 不建 World（`ModelContent.cpp:66-69`、`:120` 直接用 loader） | fbx、png、wav | loader **不依賴 ECS**，可以直接呼叫 | 做（R1 的拆法） |
| F5 | ui_editor：預覽字型與圖片（`AssetPreviewContext.cpp:92-171`）；UI 文件用 catalog id 參照資產（`UiDocumentBridge.cpp:137-165`） | ttf、png、json | 同一個機制；**UI 文件記 id** | 機制做 |
| F6 | audio_editor：`convert` 把 wav 轉成引擎格式的 clip、**寫進 catalog**（`tools/audio_editor/src/Main.cpp:29-36`） | wav、json | **事先轉檔現在就有一例**；工具照 catalog 的 Data Contract 寫，執行期不知道誰寫的 | catalog 的格式與讀寫做（R2）；工具不做 |
| F7 | 地圖編輯器：讀寫自己的地圖檔（`voxel_map_editor/src/FileAccess.cpp:21-27`） | json、voxel mesh json | 編輯器的檔是它自己的；遊戲讀它的輸出時經 catalog 與 loader | 只保證介面（5.10） |
| F8 | BigMap（使用者點名）：約 1 km²、242 MB、每個 region ≤ 16 MiB | 地圖 region | 一個大內容能拆成許多 id；單一 asset 的上限可設；**同時在飛與每次發佈要有上限**；串流的規則照決定 8 屬日後的地圖 Module | 做上限（R9）與 region 級 id |
| F9 | 2D 橫版遊戲（`-engine-scope-general`） | png、json | 單機、不需要 PvP 的任何東西；catalog 可以手寫或由產品程式產生 | 做 |

**推不出、不放進來的**：熱重載（master 的 `AssetWatcher` 在 `apps/`、`tools/` 0 件呼叫）；子 asset（ufbx loader 一檔一個值）；二進位 loader。

### 3.3 規模與預算

**已知的規模**（master 實查）：遊戲內容 36 個檔、36.7 MB；最大 fbx 23.7 MB、ttf 4.5 MB、png 1.34 MB、json 1.45 MB。BigMap 242 MB，region ≤ 16 MiB（region 的個數未查）。

**預算：寫不出數字，延後到第一個產品搬回時**（12.2 第 6 題）。缺：載入時間；同時在飛的請求數或解碼中的記憶體峰值；每次發佈的上限；catalog 的項目數與啟動時讀 catalog 的時間。

**本批先做放數字的位置**：每次發佈上限、在飛上限是產品選項（R9）；預設「不設限」，標明不是判定。

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
| 上限 | 每幀上傳位元組（`render_asset.rs:509`）〔源〕 | 〔未〕 | 文件沒看到 | 串流池〔檢〕 | bundle 快取上限〔文〕 | 每幀 2 個 | 每次發佈、在飛、loader 位元組（R9） |
| 結果怎麼讓 World 看到 | `PreUpdate` 照完成順序〔源〕 | 每幀處理命令〔源〕 | 作者輪詢〔文〕 | 委派〔文〕 | 完成事件〔文〕 | 主執行緒輪詢 | 完成了、下一次維護就發佈；每個 World 的套用點換視圖（決定 9） |
| 失敗怎麼告知 | `Failed`＋失敗事件〔源〕 | 狀態 `Error`〔源〕 | FAILED，沒有細節〔文〕 | 缺檔也呼叫完成委派〔文〕 | handle 狀態〔文〕 | — | `Failed`＋`AssetLoadFailed`（E2） |
| 熱重載 | 監看＋`Modified`〔源〕 | 〔未〕 | 原地刷新〔文〕 | 〔未〕 | 〔未〕 | Engine 有、產品沒用 | 不在本批 |

### 4.3 共通、分歧、本頁的位置

**共通**：核心不認得格式；身分在請求時就給；圖片不用視窗層解碼；持久鍵不隨「原始→轉檔」改變；**資產管理都在 World 外或高於 World**（Bevy 每 App，其餘行程單例）。

**本頁的位置**（使用者的整理經 side chat 轉來，卡片照它寫）：
- **管理骨架像 Unity Entities**：id 當持久鍵、資產管理在 World 外、值不可變。
- **載入方式像 Bevy**：執行期讀原始檔、各 Module 登記 loader、不內建串流。
- **和 Unity 的差別**：執行期就讀 id 對照表（catalog），不靠 build 的產物。

**仍和別家不同的**（第 11 節）：Product 一份而不是行程一份（GYO 沒有行程級靜態物件的先例；回主選單另建 Product 時資產重載）；去重的鍵只有 id，settings 隨 catalog 項目；值不可變（Bevy、Godot 可變）。

## 5. 設計（Contract）

### 5.1 位置、名字、依賴（卡片照 L17 判，照 N11～N17、O-6 與 `logging`／`world_logging` 的先例）

| 單位 | 目錄 | target | 登記檔 | 依賴 | 內容 |
|---|---|---|---|---|---|
| asset | `engine/modules/asset` | `GYO::Asset` | library_only=yes、default=no | base；private io、json | `AssetId`（id）、catalog 的讀寫與驗證、loader 的介面（`LoadContext`、`LoaderFailure`）、整檔讀取、文字 loader、`AssetStore`（含依賴圖、原始位元組雜湊）、`Handle<T>` |
| world_asset | `engine/modules/world_asset` | `GYO::WorldAsset` | library_only=no、default=yes | asset、base、kernel、orchestration；threads 看交換點 | 擁有 Product 的 `AssetStore`；每個 World 的 `AssetRequests` 與 `Assets<T>`（唯讀視圖）、套用點 System、訊息、雜湊函式 |

- **為什麼拆兩個**：F4、F6 的工具與擁有型別的 Module 只要 loader 與 catalog，不該為此連上 kernel、orchestration。`AssetStore` 也不認得 ECS，工具不建 World 也能用。
- **屬於哪一層**：都放 `engine/modules`（會長大的 Module 層）。
- **為什麼不是 Plugin**：都是我們維護、介面是我們的。
- **`default=yes`**：照「Module 預設全帶、產品可以關」。

命名：
- World 外的資產管理叫 `AssetStore`。不照 Bevy 叫 `AssetServer`：「Server」在 GYO 指 PvP 的 Match 伺服器（`target/naming.zh-Hant.md` DI 4）；不叫 `AssetManager`：那是 master 的總管物件，11-11 不採。
- id 的型別叫 `AssetId`：作者寫的、存檔裡記的、catalog 裡的那個字串（例：`char.hero`）。修訂 2 的 `AssetId<T>`（index＋generation）拿掉；store 內部的索引不對外。
- 其餘照 Bevy：`Assets<T>`、`Handle<T>`、`AssetMessage<T>`。

### 5.2 id 與持久鍵

**`AssetId`** 是 catalog 裡的字串鍵，**它就是持久鍵**（決定 6）：作者在程式、內容檔、存檔、判定資料裡都只記它。

- 字元集與長度：ASCII，`[a-z0-9._-]`，1～128 字元（卡片照 L17 判；跨平台、可以放進檔名與 URL、可雜湊）。不合規是 catalog 的啟動錯誤。
- **改名、搬移、事先轉檔**：只改 catalog 項目的路徑或格式，id 不變，內容與存檔不受影響。
- **跨行程穩定**：同一份 catalog，id 在 Client、Match、工具裡都相同。store 內部用的索引依登記順序而異，不出現在任何對外的值裡（AS-6）。

### 5.3 catalog（asset 擁有的 Data Contract）

**一個項目**：`id`、`type`（型別名，由擁有型別的 Module 登記，例：`text`、`model`）、`path`（來源名＋相對路徑，例：`content://models/hero.fbx`；來源名是 io 的 scheme，`MountPoint.hpp:15`）、可選的 `format`（沒寫就取副檔名）、可選的 `settings`（給那個 loader 的 JSON 物件）。

**檔案格式**：JSON（經 JSON Module），頂層 `{"version": 1, "assets": [...]}`。**Version 與 Validation Rule 是契約的一部分**（AGENTS §8 MUST）。

**Validation Rule**（違反＝啟動錯誤，收進 `ProductStartupReport`，O-5）：
- `version` 是本版認得的值；
- id 合規、在所有 catalog 裡唯一（同一 id 的兩個項目內容完全相同才算同一項，否則是衝突）；
- `type` 有登記的 loader；（`type`、`format`）找得到 loader；
- `settings` 能被那個 loader 解讀；
- `path` 的語法合規（檔案存不存在**不在**啟動時查，載入時才是 E2：BigMap 有數百個 region，啟動時逐一查檔太慢）。

**誰產生**：工具、Script、手寫都一樣（AGENTS §8）；asset 函式庫提供讀、寫、驗證，工具照它寫（`engine-not-tool-duties`：工具自己持有內容知識，經契約交接）。產品也可以在自己的 `build` 裡用程式加項目（F9 不想手寫時，自己掃資料夾產生）——asset 不掃資料夾。

**怎麼給 store**：產品在自己的 `build` 經 world_asset 的入口加 catalog 檔或項目（`ProductSetup` 沒有選項包，所以走 `Build`；同一份在每個 World 的 `Build` 再給一次是冪等的，衝突是啟動錯誤）。

**系譜**：概念來自藍圖 `:66`「catalog 邏輯 id」、master 的 `AssetCatalog`、Unity 的 content catalog；格式是新寫的，不沿用 master 的檔案格式（master 的 `AssetCatalog` 是工具與遊戲共用但 owner 未定的契約；決定 6 把它歸資產管理，owner 的未決項解消）。

### 5.4 資產管理在 World 外（決定 7）

**`AssetStore`**：每個 Product 一份。world_asset 在 `Configure`（Product 層一次，O-2）建立它，存在 world_asset 這個登記單位的實例裡，活到 Product 結束。

- **為什麼放在登記單位的實例**：編排層的 `ProductSetup` 沒有 Product 層的共用值（`Setup.hpp:88-110`）；登記單位的實例本來就活到 Product 結束、可以持有自己的資料（`RegistrationUnit.hpp` 註解）。這樣**編排層不必改**。
- **World 端**：world_asset 在每個 World 的 `Build` 放進兩個 Resource：
  - `AssetRequests`：請求入口與狀態查詢；
  - `Assets<T>`：每個型別一份的**唯讀視圖**。
  - 兩者都只持有對 store 的共享參照。System 經這兩個 Resource 存取，不持指標；元件裡只放 `Handle<T>` 或 `AssetId`（AS-1）。
- **換 World、多 World**：不是 asset 的事。Handle 的引用計數跨 World 有效；舊 World 銷毀時它的 Handle 才放掉。新 World 在準備函式或 `beforeSwap` 請求同樣的 id，資產還活著就命中；`DropOldFirst` 時舊 Handle 先放掉，但在 store 下一次維護之前再請求就救回（5.5）。所以讀檔不重讀，也不需要 CarryOver。
- **回主選單另建 Product**（O-3）：舊 Product 的 store 隨它結束；新 Product 重新載入。這是「Product 一份」的代價，寫明（RC-AS3）。

**這一節取代了修訂 2 的**：每 World 一份 `Assets<T>`、CarryOver 與 `AssetOptions{carryOver}`、時間窗口、`BuildNewFirst` 失敗搬回、訊息不帶過去的對帳、X-4 的 store 實例標記、AS-1 的過渡狀態。修訂 2 檢查的 B2（`DropOldFirst` 其實也帶過去）隨之消失。

### 5.5 請求、Handle 的生命週期

**請求**：`AssetRequests::Load<T>(AssetId)`，在 System 或編排層給的準備函式裡同步執行，**當場**回 `Handle<T>`。
- 找 catalog：沒有這個 id → E2 `UnknownId`；`T` 和項目的 `type` 不合 → E2 `TypeMismatch`（id 是外部資料，所以是 E2 不是 E0）。兩者都回一個狀態為 `Failed` 的 Handle，同時發 `AssetLoadFailed`。
- 引用計數加一（原子）。狀態是 `NotRequested` 或 `Failed` 的，排一筆載入（`Failed` 再請求＝重新載入，照 Bevy；asset 自己不重試）。

**去重**：鍵＝id。同一 id 已 `Queued`、`Loading` 或 `Loaded`：回同一份的新 Handle，不再讀。settings 跟著 catalog 項目，同一個 id 只有一種 settings（修了 Bevy issue 11111 的根源：同一路徑不同 settings 在 GYO 是兩個 id）。

**Handle**：
- 強 Handle 帶跨 World 的引用計數（原子）。最後一個消失時，那一項成為「待放掉」。
- **store 的維護**（5.6）處理「待放掉」：計數仍是零就移除值、發 `Removed`；**計數已回升（維護之前又有人請求）就救回，同一份、不重讀**（照 Bevy `assets.rs:578-580`）。
- 不做寬限、不做兩段釋放（5.15 A4、RC-AS7）。
- Handle 被複製、放進別的 World 的元件都可以；Handle 比 store 活得久時**安全失效**：查詢回 `NotRequested`、取值回空，不成為懸空參照。

**取消**：最後一個 Handle 在 `Queued` 或 `Loading` 時消失，結果回來時丟掉，計入「沒人要了」。

**值**：不可變、共享唯讀（AS-10）。`Assets<T>::Get(handle)` 回 `const T*`（沒載好回空），**只在這次 System 執行期間有效**；System 之間不保留。

**原始位元組雜湊**（決定 10）：store 讀檔時對讀到的位元組算 SHA-256（`engine/base` 的 `Sha256`），每個 id 一個，只算自己的檔、不含依賴。
- 預設開；產品選項可關，關時查詢回「沒算」。`AssetRequests::SourceDigest(handle)` 讀。
- 比不比、不同時怎麼辦，歸作者或網路 Module。asset 不拿它做任何判斷。
- 介面不綁「讀檔時算」：日後 catalog 項目可以帶工具事先算好的雜湊（Unity content catalog、Bevy `processor/mod.rs:86-91` 的來源雜湊），那是 catalog 的 Version 升級，查詢的 API 不變。
- 全部內容 36.7 MB 的雜湊成本在實作時量測，記進 dev_log。

### 5.6 執行緒、維護與發佈；給交換點的需求

**store 的工作**（World 執行緒之外）：讀檔＋解碼。工作在哪條執行緒跑由交換點或 runner 給的執行器決定（X-1、X-4）；asset 的工作是可以在任何執行緒跑的純函式（AS-3）。

**store 的維護**（store 內部，單一處理者）：
- 收請求佇列，排工作；
- 收完成的結果，放進下一份**快照**；
- 處理「待放掉」（5.5）；
- 每次維護最多發佈 N 筆或 M 位元組，其餘留到下一次（R9）。
- 維護在 store 內部可以上鎖（不在 World 執行緒上）。

**發佈**：維護把新的一份唯讀快照（id→值與狀態）以一次原子指標交換發出，並在變更紀錄（上限有界）追加 `Added`／`Failed`／`Removed`。

**World 看到結果**：每個 World 的套用 System（預設 `Main`/`First`）讀一次最新快照的指標，放進自己的 `Assets<T>` 視圖，並把變更紀錄裡自己還沒看過的部分發成 `AssetMessage<T>` 與 `AssetLoadFailed`。
- **一幀之內看到同一份**：視圖只在套用點換，所以同一幀的重跑步與新的步看到同一份（回滾的重跑在 `PreUpdate`，第 4 批 5.2）。
- **只有 store 是寫者**：World 不寫值；修訂 2 的「`Assets<T>` 唯一寫者」問題（AS-4）消失。
- 變更紀錄被修剪時（某個 World 太久沒套用），讀者從狀態查詢對帳。

**World 端的操作不上鎖**（AS-11）：請求（原子計數＋無鎖佇列）、放掉（原子計數）、讀（一次原子指標讀取）。「World 內不上鎖」（D6b、M11）的例外完整清單：
1. Handle 的引用計數（原子）；
2. 請求佇列（無鎖，多生產者單消費者，照 Unity `RuntimeContentManager.cs:112`）；
3. 快照指標（原子讀取）；
4. Handle 對 store 核心的共享參照計數（Handle 比 store 活得久時用）；
5. 丟棄計數（Product 結束時未完成的工作）。

**給交換點、threads 那批的需求**（交換點的宣告可以用任何形狀滿足）：

| # | 需求 | 為什麼 |
|---|---|---|
| X-1 | 給 store 一個執行器：工作在 World 執行緒之外跑，不碰 World（M11） | F1、F2：23.7 MB 的 fbx 解碼不能卡住 World |
| X-2 | 交換點不需要認得 asset 的型別或語意；World 看到結果靠自己的套用點，不經交換點搬值（RC-AS5、RC-O8） | 5.6 的快照 |
| X-3 | 套用點和交換點的關係：若交換點宣告定了「每幀的邊界」，套用 System 掛在它之後 | 同一幀看到同一份 |
| X-4 | 執行緒的數量與歸屬由交換點或 runner 定（Bevy 用全域 `IoTaskPool`；GYO 不先定） | O-3 |
| X-5 | **測試能控制哪幾筆工作何時完成、何時維護**（含「當場全部跑完」）；結果不依賴執行緒時序 | 等價測試（第 10 節）；只跑一次的 runner |
| X-6 | Product 結束時，未完成的工作能停下或等完，件數算得到 | 件數等式（5.8） |

### 5.7 狀態與訊息

**狀態**：`NotRequested`、`Queued`、`Loading`、`Loaded`、`Failed`（帶 `AssetLoadFailure`）。用 `AssetRequests::State(handle 或 id)` 查；看到的是這個 World 目前的視圖。

**就緒查詢**（決定 9）：`AssetRequests::Ready(一組 Handle)` 回：全部好了沒（**連同依賴**，5.17）、`Loaded` 筆數、`Failed` 筆數與清單（含依賴，帶經由哪個父項）、整串的 id 清單。`AssetRequests::Pending()` 回這個 World 請求過、還沒結束的數目。
- 讀取畫面、淡出、等哪些才開始 tick，是遊戲設計，**歸作者**（決定 9）。單筆到了就看得到，所以「先看到地面才看到樹」要由作者用就緒查詢擋住。

**訊息**：
- 每型別一種保留式 `Message`：`AssetMessage<T>{Added, Removed}`；
- 不分型別的 `AssetLoadFailed{id, typeName, failure}`（日誌與工具不認得每個 `T`；Bevy `UntypedAssetLoadFailedEvent`）；
- 用 `Message`、不用推送式 `Event`；
- 訊息可能被修剪（事件游標上限 1024），讀者要能從狀態對帳；
- 一次套用之內的訊息順序**不保證**；`Modified`、`LoadedWithDependencies` 不做。

### 5.8 錯誤（照已合併的〈Error levels〉）

| 情況 | 級別 | 怎麼出現 |
|---|---|---|
| id 不在 catalog、型別不合、檔案不在、讀不到、超過上限、loader 解碼失敗、經路徑引用但 catalog 沒有（`UnlistedReference`）、循環引用（`CyclicReference`） | **E2** | 那筆的狀態是 `Failed`，帶 `AssetLoadFailure{code, message, detail}`（`Engine::Base::Error<Code>` 的形狀，沒有鏈）。loader 自己的碼名與細節放進 `detail`；同時發 `AssetLoadFailed` |
| loader 丟例外（含 `std::bad_alloc`；`catch (...)`） | **E2**（`LoaderThrew`） | 同上。工作在 World 外，例外不能沿 World 的 Run 傳出，也不能讓行程停下；`detail` 寫 `what()` |
| catalog 不合 Validation Rule；同一個（`type`、`format`）登記兩個 loader；loader 的設定不合法 | 啟動錯誤 | `Finish` 回報，收進 `ProductStartupReport`（O-5） |
| Product 結束時還在飛的工作 | **E3** | `AssetDiscarded`（筆數），由 world_asset 在結束時記（X-6） |
| 呼叫寫錯：在編排層給的時機之外、沒有 World 時呼叫 `Load` | **E0** | `GYO_ASSERT` |

**件數等式**（AS-8；第 10 節照它突き合わせ）：

> 送出 ＝ 發佈成 Loaded ＋ 失敗 ＋ 沒人要了（取消）＋ 丟棄（Product 結束）

asset 不寫日誌、不自己重試、不讓 World 中斷。要不要因為載入失敗結束遊戲，由作者的 System 決定（回 `Err` 就是作者把 E2 升成 E1）。

### 5.9 決定性、內容身分（和第 4 批的權威、回滾對齊）

1. **身分的決定性**：id 是 catalog 的字串，和請求順序、完成時序、World 的歷史都無關。新開局與讀檔進同一關，同一個內容的 id 相同。
2. **內容的決定性**：asset 核心不改位元組。值＝loader(位元組, catalog 的 settings)。不讀時鐘。全部結束之後，store 的內容和完成順序無關。**哪一筆在哪一幀讓 World 看到，依完成時序**（決定 9：到了就可見）。判定要用的內容在生成世界時載完才開始 tick，所以決定性不受影響；對局中影響判定的串流屬日後的地圖 Module（RC-AS1）。loader 的輸出跨平台是否逐位元相同，是那個 loader 的事（`determinism.md` DET-2）。
3. **id 可以跨行程、進存檔、進權威的資料**；store 內部的索引不行（AS-6）。
4. **內容身分**（舊 DI 3）：asset 提供每個 id 的原始位元組雜湊（5.5），比不比歸作者或網路 Module（決定 10）。對原始位元組比，可以繞開第三方解碼（ufbx）的跨平台差異（`determinism.md` §4.9、DET-2）。
5. **回滾**：asset 的視圖不是判定資料，回滾 Module 不存它、不倒回它。一幀之內重跑的步看到同一份。但第 100 個 tick 第一次跑時某內容還沒載好、重跑時已載好，依「載好沒」分支的判定就會不同；照第 4 批第 21 節，**到達時刻要影響判定就走 `rollback.Input<T>`**，否則作者要讓判定用的內容在進判定之前載完。

#### 5.9.1 asset 放進 World 的型別的雜湊函式（第 4 批 3.3）

| 型別 | 雜湊什麼 | 理由 |
|---|---|---|
| `AssetRequests`、各 `Assets<T>` | **什麼都不寫** | 視圖，可以重建；內容依完成時序變，寫進去 `Summary` 就依時序變 |
| `AssetMessage<T>`、`AssetLoadFailed` | **什麼都不寫** | 何時發依完成時序 |
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
- 來源名＝io 的 scheme（`MountPoint.hpp:15`）。
- 疊掛載點（`MountPoint.hpp:16`「overlay 用」、`Vfs.hpp:86-88` 找不到就往下一個）io 已經有；本批不靠它（5.10）。
- 檔案監看（`IFileWatcher`）留給熱重載，本批不用。

### 5.13 給生成世界（重做）那一批的需求

| # | 需求 | 為什麼 |
|---|---|---|
| G-1 | 生成世界分批跑時，每一批之間 world_asset 的套用 System 會跑（由誰掛、掛在哪由那批定） | 作者等「全部完成」才離開；視圖不換就永遠等不到 |
| G-2 | 結束條件由作者的 System 給，可以是就緒查詢「全部好了」，也可以是作者自己的條件 | `-build-world-phase-is-mechanism` |
| G-3 | `beforeSwap` 同步連跑（`Product.cpp:740-749`）、期間沒有套用點：裡面發出的請求要麼命中，要麼在替換之後才讓新 World 看到。寫明，不要求改 | 5.4 |

### 5.14 和讀檔、存檔版本 Module 的關係

**讀檔**＝替換 World（讀檔宣告修訂 11，核准版）。asset 的部分：
1. **先驗證**：作者在現在的 World 請求存檔裡記的 id，等到全部結束。有 `Failed`（含 `UnknownId`：內容改版後某個 id 不在了）就不發替換，回 `Result` 給玩家（O-8）。
2. **再替換**：新 World 請求同樣的 id，命中（5.4）。
3. **存檔裡記 id**（AS-6）。

**存檔不是 asset**：存檔的格式是作者的；讀存檔檔案用 io 或作者自己的方式。**存檔版本 Module** 比對存檔的版本標記，和 asset 的內容無關；內容改版造成 id 消失由第 1 步發現。catalog 的 Version 是 catalog 格式的版本，不是內容的版本。

### 5.15 藍圖的資產不變量 A1～A6

| # | 藍圖的內容 | 本頁 | 理由 |
|---|---|---|---|
| A1 | 每型別一個 store＋每型別一份事件 | **留**，形狀改：一個 `AssetStore` 內每型別一張表，World 端每型別一個 `Assets<T>` 視圖與 `AssetMessage<T>` | — |
| A2 | id 只在行程內有效，path（或 Uuid）才是契約 | **改**：契約的鍵是 catalog 的 id 字串（決定 6）；store 內部索引只在行程內 | — |
| A3 | 非同步完成序列化到單一套用點 | **留**：store 的維護是單一處理者；World 看到結果只在自己的套用點 | 順序：完成了就發佈（決定 9） |
| A4 | 釋放兩段式（`Unused`→`Removed`） | **本批一段** | Bevy 兩段的實際用途是渲染：只給渲染用的 asset 用 `get_mut_untracked` 與 `take_gpu_data` 把資料**就地取走**、main World 只留中繼資料（`bevy_render/src/render_asset.rs:351-353`、`:96`），其餘整份 `clone`（`:364`）；GPU 那份等最後一個 Handle 消失（`Unused`）才移除（`:332-333`）。AS-10 禁止就地取走，所以渲染那批要丟 CPU 副本時，路是「第二段釋放：移除值、Handle 還在」→ RC-AS7。AS-10 的共享唯讀也省掉 Bevy 的 `clone` |
| A5 | 依賴圖由 core 擁有、loader 只宣告 | **留**（決定 11）：依賴圖在 store，loader 經 `LoadContext` 宣告引用（5.17） | 修訂 2 的「延後」翻掉 |
| A6 | 格式解析器與執行期解讀分層 | **留**：asset（library）與 world_asset 的拆法；loader 歸擁有型別的 Module | — |
| （:66）| catalog 邏輯 id、StrictJson、Result、ready 前全載；副檔名衝突應為錯誤 | catalog 邏輯 id：**採**（5.3）；StrictJson 歸 JSON Module；Result：留；ready 前全載：歸作者（12.2 第 3 題）；（`type`、`format`）衝突是啟動錯誤 | — |

### 5.16 loader 的介面與登記

**介面**（在 `asset` 函式庫；名字實作時照 N15 定）：
- **一個 loader 宣告**：產出的型別 `T` 與型別名（catalog 的 `type`）；它認的 `format`；`Settings` 與從 catalog 的 JSON 解讀它的函式；預設的位元組上限；錯誤碼。
- **載入函式**：`Load(bytes, settings, LoadContext&) -> Result<T, LoaderFailure<Code>>`。
  - `LoadContext` 有：這筆的 id 與路徑（給錯誤訊息）、上限、宣告引用的入口（5.17）。
  - 沒有 World、沒有時鐘、沒有全域可變狀態；同樣的位元組與 settings 得到同樣的值與同樣的引用清單（AS-3）。
  - 產出的 `T` 要合 AS-10 (c)。

**系譜**：master 的 `IAssetLoader::Load(ConstSpan<byte>, LoadContext)`，去掉 `AnyAsset` 的型別抹除；引用的入口照 Bevy `LoadContext::load`（`loader.rs:735`）。Bevy 的 `Reader`＋async 不採：串流沒有 Feature（決定 8）。

**整檔進記憶體**：解碼前整檔讀進來，帶上限；不做串流的 loader（最大的內容 fbx 23.7 MB、region ≤ 16 MiB 在上限內）。

**登記**：擁有型別的 Module 在自己的 `Build` 經 world_asset 的入口登記 loader，登記進 Product 的 store。
- 每個 World 的 `Build` 都會再登記一次，同一個 loader 是冪等的；同一個（`type`、`format`）兩個不同的 loader 是啟動錯誤，在 `Finish` 回報（O-5）。
- 為什麼走 `Build`：`ProductSetup` 沒有 Product 層的共用值（5.4）。
- catalog 的驗證在第一個 World 的 `Finish` 做（這時 loader 都登記好了）。

**上限**（R9）：每個 loader 給預設的位元組上限；產品可以覆寫，放寬或收緊都可以（`engine-not-rigid`）；超過是 E2 `TooLarge`。文字 loader 的預設上限 16 MiB（照 F8 的 region 上限）。

### 5.17 依賴（Engine 推出；決定 11）

**loader 宣告引用**：`LoadContext` 有兩個入口：
- `Reference<U>(AssetId) -> Handle<U>`：引用另一個 id；
- `ReferencePath<U>(相對路徑) -> Handle<U>`：檔案格式裡記的是路徑（例：fbx 裡的貼圖檔名）時，經 catalog 的反查（同一來源、正規化後的路徑 → id）換成 id。catalog 裡沒有這個路徑，那筆依賴是 E2 `UnlistedReference`。

在 store 裡，引用會立刻排請求（照 Bevy）；直接呼叫 loader（F4）時只記下引用清單，回不連 store 的 Handle。

**生命週期**：值裡持有依賴的 Handle（例：模型持有貼圖的 Handle），所以依賴活得和父項一樣久。父項移除時它的 Handle 放掉，依賴照一般的引用計數走（5.5）。去重照 id：兩個模型引用同一張貼圖，只讀一次。

**狀態**：每個 id 有自己的狀態（5.7），另有**連同依賴的狀態**：
- `Pending`：自己或某個遞迴的依賴還沒結束；
- `Loaded`：自己與遞迴的全部依賴都 `Loaded`；
- `Failed`：列出失敗的 id 與經由哪個父項。

父項自己的狀態不因依賴失敗而變（照 Bevy：自己 `Loaded`、連同依賴 `Failed`，`server/info.rs:661-716`）。作者可以決定沒有某張貼圖也照用。就緒查詢（5.7）用的是連同依賴的狀態。

**循環引用**：A 引用 B、B 又引用 A。關上循環的那一筆引用是 E2 `CyclicReference`，**不持有**那個 Handle，否則兩邊的計數永遠不歸零。偵測在 store 的維護裡做（加邊時查可達）。Bevy 沒有處理循環（第 11 節），這是 GYO 的差異。

**雜湊**：原始位元組雜湊每個 id 各自一個，只算自己的檔（5.5）。要比整串，作者用就緒查詢給的 id 清單逐一比。

**訊息**：不另加 `LoadedWithDependencies`。就緒查詢就是它的介面（決定 9）。

**件數**：依賴的請求也計入送出；件數等式不變（5.8）。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature 需要**：F1～F6、F8、F9；12.1 的決定 1～8。
2. **現在的問題**：`gyo2` 沒有 asset；舊設計（target `assets` DI 1～3）建立在已拿掉的 (c) 區、Loading 的 Ingress 與「資產放在使用它的 World」上；master 的 `AssetManager` 是總管物件、在主執行緒輪詢、貼圖綁 SDL3_image；master 的 catalog 是 owner 未定的共用契約。
3. **變化的邊界**：
   - 新 Subsystem：`engine/modules/asset`、`engine/modules/world_asset`。
   - **新 Data Contract：catalog**（格式、Version、Validation Rule 歸 asset）。
   - **推翻 11-11 方向 2**（資產放在使用它的 World）：資產管理改在 World 外，Product 一份（決定 7）。保留三類拆法與「System 不持指標」。
   - target 文件：舊 `target/assets.zh-Hant.md` 的 DI 1～3 與 RC，由新頁取代。
   - `engine/modules/image`、`third_party/stb_image` 在 SDL 那批，不在本批。
4. **影響**：
   - 編排層：登記兩個單位；**不改它的程式**（store 放在登記單位的實例，5.4）。
   - 交換點、threads：5.6 的需求（比修訂 2 少：不經交換點搬值）。生成世界：5.13。
   - 第 4 批：`Summary` 的雜湊（5.9.1）。
   - 日誌與 `world_logging`：`AssetLoadFailed`、`AssetDiscarded`。
   - 之後的 render、text、ui、audio、model Module：登記 loader；它們的值要合 AS-10。
   - 產品：載入改經 id 與 catalog。工具：照 catalog 的 Data Contract 寫（audio_editor、ui_editor），或直接呼叫 loader（model_viewer）。
5. **Dependency Direction**：asset → base、private io、private json；world_asset → asset、base、kernel、orchestration（日後可能加 threads）；日後的 image → asset。全部往下，沒有反向；core 不依賴它們；編排層不認得它們（RC-O4、RC-O8）。
6. **Ownership**：
   - catalog 的格式與驗證、id、loader 介面、`AssetStore` → asset。
   - Product 的 store 實例、World 端的視圖與請求入口、訊息 → world_asset。
   - 格式 → 擁有型別的 Module；第三方解碼 → 該 Module 的 adapter。
   - 「載完才開始」與轉場 → 作者（asset 給就緒查詢）；內容身分的比對 → 作者或網路 Module（asset 給原始位元組雜湊）。
   - 依賴圖 → asset 的 store；引用了什麼 → loader 宣告。
   - GPU 資源 → render。catalog 的內容（哪些 id、指到哪）→ 產品與它的工具。
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
  - World 端只有 `AssetRequests` 與 `Assets<T>` 兩種 Resource（唯讀視圖＋請求入口），元件裡只有 `Handle<T>` 或 `AssetId`。
  - GPU 資源是 render 的 Resource；渲染 World 經 Extract 讀（Extract 是唯一跨 World 讀的路，照 K.3-6）。
  - 守衛：行為測試（兩個 World 請求同一個 id，store 只載一次、兩邊讀到相等的值；換 World 後同一 id 不重讀）。掃描延後。
- **AS-2 核心不認得格式。** asset 與 world_asset 不解析任何內容格式（catalog 的 JSON 是 asset 自己的契約，不算；文字 loader 只驗證 UTF-8）。格式由擁有型別的 Module 登記 loader；第三方只在該 Module 的 `src/adapters/<組件>/`，私有 link。守衛：行為測試（catalog 指向沒有 loader 的型別是啟動錯誤；同一（`type`、`format`）登記兩次是啟動錯誤）。
- **AS-3 loader 是不依賴 ECS、可直接呼叫的純函式。** 輸入位元組、settings、id 與上限；輸出值或失敗；不碰 World、不讀時鐘、不持全域可變狀態；只連 `GYO::Asset` 就能呼叫。守衛：只連 `GYO::Asset` 的測試 target；同輸入兩次結果相等。CMake 掃描延後。
- **AS-4 IO 不碰 World；store 是唯一的寫者；World 只在套用點換視圖。** 讀檔與解碼在 World 執行緒之外；值只由 store 的維護放進快照；World 的視圖只在套用 System 換，一幀之內看到同一份。守衛：行為測試（X-5 的控制模式：一幀中途完成的工作，同一幀內看不到、下一個套用點看到）。
- **AS-5 身分在請求時給，而且就是 id。** Handle 在請求時同步給出，指向 id；全部結束之後，任何完成順序、任何發佈切分都得到相同的 store 內容。守衛：等價測試（第 10 節）。
- **AS-6 持久鍵是 id 字串；store 內部索引只在行程內。** id 可進內容、存檔、權威資料；`Handle<T>`、`AssetId` 的雜湊只用 id 字串。守衛：行為測試（兩份項目順序不同的 catalog，同一組 Handle 的 `Summary` 相同；新開局與讀檔進同一關的 `Summary` 相同）。
- **AS-7 「何時載完」與內容身分不是 asset 的規則；asset 提供能力。** asset 給狀態、訊息、就緒查詢（連同依賴），保證同位元組同值（AS-3），並給每個 id 的原始位元組雜湊（預設開、可關）。要不要等、轉場、比不比雜湊與不同時怎麼辦，屬作者或網路 Module；到達時刻要影響判定，走 `rollback.Input<T>`。守衛：行為測試（雜湊開：同位元組同雜湊、不同位元組不同雜湊；關：查詢回「沒算」）；「asset 不拿雜湊做判斷」靠本頁與審查。
- **AS-8 錯誤只回報。** 每筆的失敗是 E2，在狀態與 `AssetLoadFailed` 裡；catalog 與登記錯誤是啟動錯誤；丟棄是 E3；不寫日誌、不自己重試、不讓 World 中斷；件數等式成立。守衛：行為測試（各碼、各項）。
- **AS-9 介面與格式、位置分開。** 作者只用 id；catalog 把 id 對到（`type`、`path`、`format`、`settings`）；選 loader 用（`type`、`format`）；加一種格式（含引擎格式）＝登記一個 loader＋改 catalog 項目，不改請求的 API 與核心。守衛：行為測試（同一 id 從原始格式改指到引擎格式，作者的程式不變、值相同）。
- **AS-10 載好的值不可變。**（使用者 (a')；跨執行緒的部分照決定 5 現在寫）拆成三條：
  - **(a) 物件不改**：放進 store 的值在發佈之後不再改變；World 端只拿得到 `const` 存取。守衛：負向編譯測試（經 `Assets<T>` 拿不到可寫的存取；先例 `tests/common/kernel/CMakeLists.txt:176` 的 `gyo_kernel_compile_fail_samples`），第 6 步立。
  - **(b) 綁定只有 store 換，本批不換**：一個 id 在一段生命裡只發佈一次；熱重載回來時是「換一個值」，由 store 做（RC-AS4）。守衛：行為測試（同一 id 在 `Loaded` 之後再送一筆結果，值不變、計入異常的件數）。
  - **(c) loader 的 `T` 要 const 並行安全、解構不綁執行緒**：沒有 `mutable` 成員、沒有惰性快取、沒有非原子的內部計數；解構可以在任何執行緒發生（不綁執行緒、不持執行緒局部狀態）。作者要改就複製一份。字型的 glyph 快取這類可變狀態放在擁有型別的 Module 自己的 Resource。守衛：行為測試（最後一個持有者在別的執行緒放掉，解構正常；測試型別在解構子記執行緒）；其餘**只靠審查**——C++ 擋不住 `mutable`（Unity 是把 blob 限成 unmanaged 型別才做得到）。
- **AS-11 World 端的操作不上鎖。** 請求、放掉、讀只用原子操作與無鎖佇列；鎖只在 store 的維護裡，不在 World 執行緒上。例外清單在 5.6。守衛：本頁與審查；並行案例只在 TSan 建置有檢出力（CI 沒有 TSan：`git grep -i sanitize -- .github` 0 件），寫明不當判定。
- **AS-13 依賴由 Engine 推出。** loader 經 `LoadContext` 宣告引用（id 或 catalog 裡的路徑）；store 擁有依賴圖；就緒查詢的「好了」包含遞迴的全部依賴；父項自己的狀態不因依賴失敗而變；關上循環的引用是 E2 `CyclicReference`，不持有。守衛：行為測試（鏈、菱形去重、循環、依賴失敗、父項放掉後依賴跟著歸零；第 10 節）。
- **AS-12 catalog 是 asset 擁有的 Data Contract。** 格式、Version、Validation Rule 在新頁；工具、Script、手寫產生的 catalog 一樣有效；執行期讀，不需要 build 的產物；asset 函式庫提供讀、寫、驗證，工具照它寫。守衛：行為測試（每條 Validation Rule 各一個不合的 catalog 是啟動錯誤；寫出再讀回相等）。

### Replacement Criteria（寫在積上去之前）

符合任一條時，停下來交使用者決定「修還是重建」：

- **RC-AS1**（舊 RC 改寫）：日後的地圖 Module 用 asset 的 id 與上限表達不了它的串流（例：region 要在對局中進碰撞，而 `rollback.Input<T>` 表達不了），而必須在 asset 核心加特例。
- **RC-AS2**：每加一種格式都要改 asset 核心，或某個 loader 必須碰 World 才寫得出來。
- **RC-AS3**：Product 一份的代價超出預算：回主選單另建 Product 的重載時間，或多個 Product 要共用同一份內容（今天沒有這種 Feature）。這時才看行程級。
- **RC-AS4**：熱重載或子 asset 成為點名的 Feature（這時才做 `Modified`、`#label`）；或某個 loader 的依賴必須在解碼中途**拿到依賴的值**才解得下去（今天的引用只宣告、不等值）。
- **RC-AS5**：World 看到結果時，交換點或編排層需要知道 asset 的型別或語意（和 RC-O8 同類）。
- **RC-AS6**：事先轉檔的路回來時，請求的 API、Handle 或 id 必須改（違反 AS-9）。
- **RC-AS7**：某個 Feature 需要在最後一個 Handle 消失後保留值（寬限），或渲染那批要在上傳 GPU 之後丟掉 CPU 的副本（第二段釋放；5.15 A4）。
- **RC-AS8**：執行期讀 catalog 的成本（啟動時間、記憶體）超出預算，或 catalog 必須由 build 產生才能保證一致（像 Unity）。
- **RC-AS9**：World 端為了請求或讀取必須上鎖（AS-11 保不住），或某個點名的型別做不到 AS-10 (c)。
- **RC-IM1**（image Module 的 DI 節）：不可信來源的圖成為 Feature，或 stb_image 的缺陷影響我們的內容，就換 adapter。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

每一列都先問：「這是不是在為舊形狀找位置？」（`target/README.md`〈每批事前宣告的檢查項〉）

| 選擇 | 採用 | 系譜 | 退掉的 | 決勝理由 | 需要它的 Feature |
|---|---|---|---|---|---|
| 資產管理放哪 | World 外，Product 一份，由 world_asset 持有 | Unity Entities（行程一份）、Bevy（App 一份）；決定 7 | 每 World 一份＋CarryOver（修訂 2）；行程級靜態物件；改編排層加 Product 層共用值 | 資產的生命週期和 World 無關（使用者）；GYO 沒有行程級靜態先例；登記單位的實例本來就活到 Product 結束，編排層不必改 | F1（Sim＋Present）、F3 |
| 持久鍵 | catalog 的 id 字串 | 藍圖 :66、master catalog、Unity GUID；決定 6 | 路徑（修訂 2、Bevy）；GUID（不可讀、要工具產生） | 作者只用 id（使用者）；改名與轉檔不影響內容與存檔；字串讓手寫 catalog（F9）可行 | F3、F5、F6 |
| catalog | asset 擁有的 Data Contract，執行期讀 | master `AssetCatalog`（概念）、Unity content catalog（概念）；格式新寫 | build 時產生（Unity）；owner 未定（修訂 2） | 起步讀原始檔（三件之 3）；工具、Script、手寫一樣有效（AGENTS §8） | F5、F6、F9 |
| 單位的拆法 | `asset`（library，含 store）＋`world_asset`（Module） | `logging`／`world_logging` | 一個 Module；store 放 world_asset | 工具不建 World 也要 loader、catalog 與 store | F4、F6 |
| World 怎麼看到結果 | store 發佈不可變快照；World 在套用點換視圖 | 新推導；Unity 的每幀處理命令、Bevy 的 `PreUpdate` 套用 | 經交換點把值搬進 World（修訂 2）；World 每次讀都看最新的 | 一幀看到同一份；World 不寫值；交換點不必搬值 | F1（Client 的回滾重跑） |
| 值的可變性 | 不可變、共享唯讀；三條分開寫 | Unity blob（唯讀）；使用者 (a') | Bevy、Godot 的可變值 | 不可變是日後最難改的約束；跨執行緒共用的前提（決定 5） | F1、F2 |
| World 端不上鎖 | 原子＋無鎖佇列，鎖只在 store 的維護 | Unity 的多生產者單消費者佇列（`RuntimeContentManager.cs:112`）；Bevy 的 `RwLock`（`server/mod.rs:65-72`）不採 | World 端直接拿 store 的鎖 | 「World 內不上鎖」（D6b、M11）；多執行緒是已定前提 | F1、F2 |
| 結果的順序 | 完成了就發佈＋一組 id 的就緒查詢；轉場歸作者 | Bevy（照完成順序） | 舊 DI 2 的連續前綴；整組才可見；作者可設組、Engine 保證同幀 | 使用者決定 9：轉場是遊戲設計；判定用的內容在開始 tick 前載完 | F1、F8 |
| 內容身分 | asset 提供原始位元組雜湊（預設開、可關）；比對歸作者或網路 Module | Unity content catalog、Bevy processor 的來源雜湊；第 4 批 3.2、4.4 | 只由作者對值算；asset 算且當規則（舊 DI 3） | 使用者決定 10：作者只用 id、asset 本來就讀位元組；繞開第三方解碼的跨平台差異；提供能力、不替外面決定 | F2、F9 |
| 依賴 | Engine 推出：loader 宣告引用，store 擁有依賴圖 | Bevy `LoadContext::load`（`loader.rs:735`）與失敗傳播（`server/info.rs:661-716`）；Unreal、Addressables、Godot 都由 Engine 推 | 作者自己列；本批只留介面 | 使用者決定 11：作者只關心使用；依賴是機制的核心能力 | F1（模型引用貼圖） |
| 「載完才開始」 | 作者的事 | `-build-world-phase-is-mechanism` | 舊 DI 3、runtime-composition DI 3 | Loading 不是機制 | F1、F8、F9 |
| 套用的位置 | 預設 `Main`/`First`；生成世界期間照 5.13 | Bevy 的 `PreUpdate`，往前移一格 | `PreUpdate` | 同一幀的重跑步與新步看到同一份 | F1（Client） |
| 釋放 | 一段；維護前再請求就救回 | Bevy 的引用計數與救回規則（`assets.rs:578-580`） | 兩段（藍圖 A4、Bevy `Unused`）；master 的 N 幀寬限 | 兩段的實際用途是渲染丟 CPU 副本，本批沒有渲染；換 World 靠救回規則，不需要寬限 → RC-AS7 | F3 |
| 去重的鍵 | id | Bevy 的（路徑、型別）＋修它的缺陷 | Bevy 原樣（settings 被忽略） | settings 隨 catalog 項目，同一 id 只有一種 | F1 |
| 錯誤 | 狀態裡的 E2＋不分型別的訊息；不記日誌 | 〈Error levels〉；Bevy `UntypedAssetLoadFailedEvent` | asset 自己寫 `Warning`（master） | Engine 的 Module 不自己寫日誌 | F1、F5 |
| loader | 純函式、整檔、直接可呼叫 | master `IAssetLoader` 去掉型別抹除 | Bevy 的 `Reader`＋`LoadContext`＋async；master `AnyAsset` | F4 要直接呼叫；依賴載入與串流沒有 Feature | F4、F6 |
| 上限 | 每次發佈、在飛：產品選項；loader 上限：產品可覆寫 | AGENTS §2；master `maxLoadsPerFrame` | 沒有上限 | F8 的 242 MB；`engine-not-rigid` | F8 |
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
| 第 2 步 | `AssetStore`：請求、引用計數、救回、取消、維護、快照發佈、變更紀錄、依賴圖與循環偵測、原始位元組雜湊；X-5 的控制模式 | Opus／high（無鎖的部分若檢查認為要，先問再升 xhigh） | 上段（執行緒） |
| 第 3 步 | `world_asset`：store 的持有、`AssetRequests`、`Assets<T>` 視圖、套用 System、訊息、5.9.1 的雜湊函式 | Opus／high | 上段 |
| 第 4 步 | 接交換點與生成世界（照它們的宣告）、真執行緒模式、上限；等價測試 | Opus／high | 上段（執行緒、決定性） |
| 第 5 步 | 換 World 與讀檔案例、丟棄記錄、錯誤各碼 | Opus／high | 上段 |
| 第 6 步 | scenario test、負向編譯、突變、新頁、dev_log、審查 | 實作 Opus／high；審查 Fable／high | 上段 |

## 10. 測試、判定（先宣告；結果出來之後不放寬）

測試用的型別：文字（真的 loader）；測試目錄裡三個測試 loader：一個慢、可以指定失敗；一個模擬引擎格式（`format` 為 `gyotxt`）；一個**會宣告引用**的（檔案內容列出它引用的 id 或路徑）。測試用的 catalog 由測試程式產生。

**scenario test（第 6 步，沿遊戲流程）**：
1. 生成世界分兩批：第一批請求 6 個 id，其中一個檔案不在、一個超過上限、一個 id 不在 catalog。
2. 跑到就緒查詢「全部好了」：3 筆 `Loaded`；3 筆 `Failed`，碼是 `NotFound`、`TooLarge`、`UnknownId`；`AssetLoadFailed` 3 則；就緒查詢的失敗清單是那 3 個 id。
3. **兩個 World 共用**：第二個 World 請求同樣 3 個 id：讀檔計數不增加，兩邊讀到相等的值。
4. **讀檔，先驗證**：在現在的 World 請求存檔要的 2 個 id，等到結束，發 `ReplaceRequest`（`BuildNewFirst`）。新 World 在 `beforeSwap` 再請求：命中，讀檔計數不增加。沒再要的 1 個在下一次維護移除，發 `Removed`。
5. **`DropOldFirst` 的救回**：同樣的流程用 `DropOldFirst`：舊 Handle 先放掉，新 World 在準備函式再請求，在維護之前，所以救回、讀檔計數不增加。
6. **先驗證失敗**：存檔記一個不在 catalog 的 id。驗證得到 `Failed`（`UnknownId`），不發替換，舊 World 照跑。
7. 件數的突き合わせ：送出＝發佈成 Loaded＋失敗＋取消＋丟棄，各項數字寫在測試裡。

**等價測試（AS-5）**：同一組 20 個 id，用 X-5 的控制模式比較：(1) 一次全部完成；(2) 反序分 4 次完成；(3) 交錯分 7 次完成，每次維護上限 3 筆；(4) 載入中途丟掉 3 個 Handle、同時再請求 3 個新的。判定：全部結束後四組的 store 內容相同；件數等式成立。

**依賴（AS-13）**：用會宣告引用的測試 loader：
- 鏈 A→B→C：只請求 A，就緒查詢在 C 載好之前回「沒好」，之後回「好了」，id 清單是 A、B、C；
- 菱形 A→B、A→C、B→D、C→D：D 只讀一次；
- 循環 A→B→A：關上循環的那筆是 `CyclicReference`，A、B 的計數在放掉 A 之後都歸零；
- 依賴失敗 A→X（X 不在）：A 自己 `Loaded`，就緒查詢回「失敗 1 筆：X，經由 A」；
- 經路徑引用：catalog 裡有的路徑解成 id；沒有的是 `UnlistedReference`；
- 放掉 A：下一次維護 A 移除，B、C 跟著歸零移除。

**原始位元組雜湊（AS-7）**：開：同位元組同雜湊、改一個位元組雜湊不同、等於 `Sha256` 直接算的值；關：查詢回「沒算」。

**同一幀看到同一份（AS-4）**：一幀中途完成並維護的工作，同一幀之內的 System 看不到；下一個套用點看到。

**值不可變（AS-10）**：
- (a) 負向編譯：經 `Assets<T>` 取可寫存取、經 `Get` 拿到非 const 指標，各一個樣本，編譯必須失敗。
- (b) 同一 id 在 `Loaded` 之後再送一筆結果：值不變。
- (c) 最後一個持有者在別的執行緒放掉：測試型別的解構子記下執行緒，解構正常、只發生一次。

**`Summary`（AS-6、5.9.1）**：兩份項目順序不同的 catalog（store 內部索引不同），同一組 Handle 的 `Summary` 相同；新開局與讀檔進同一關，`Summary` 相同。

**catalog（AS-12）**：每條 Validation Rule 各一個不合的 catalog → 對應的啟動錯誤；寫出再讀回相等；同一份 catalog 在兩個 World 的 `Build` 都給 → 不是錯誤；同一 id 兩個不同項目 → 啟動錯誤。

**事先轉檔（AS-9）**：同一 id 從 `.txt` 改指到 `gyotxt` 格式的檔，作者的程式不變、值相同；同一（`type`、`format`）登記兩次 → 啟動錯誤。

**直接呼叫（AS-3、F4）**：只連 `GYO::Asset` 的測試 target 呼叫文字 loader；同輸入兩次結果相等；兩條執行緒各呼叫 1,000 次，結果全部等於單執行緒（0 件不一致）。

**真執行緒模式（AS-4、AS-11）**：同一組 20 個 id、兩個 World 同時請求，跑 50 次。判定：每次的最終 store 內容都等於控制模式；件數等式 50/50 成立。資料競爭的檢出只在 TSan 建置有效，不當判定。

**突變**：每條 DI 的守衛各至少一個專屬的預期失敗（AGENTS §14.2、§14.5）。AS-7、AS-11、AS-10 (c) 的「只靠審查」部分不立突變，寫明。清單在實作時列，受別的模型做敵對式檢查。

**判定**：上面全部通過；L1 三平台（linux-x64、windows-x64、macos-arm64）通過；突變全部 killed。

**量測**：載入時間、記憶體、catalog 的讀取時間只做探索量測，標「不是判定」；數字延後（12.2 第 6 題）。

## 11. 卡片照 L17 判的（不是使用者的話）

**照命名規範、別家原始碼、已定原則判的**：
- 兩個單位的拆法；`AssetStore` 在 asset 函式庫；名字 `AssetStore`、`AssetId`（5.1）；
- id 的字元集與長度（5.2）；catalog 的欄位、JSON、Validation Rule、「檔案存在與否載入時才查」（5.3）；
- store 放在 world_asset 登記單位的實例、經 `Build` 給 catalog 與登記 loader、冪等（5.3、5.4、5.16）；
- 救回規則照 Bevy；取消的計法（5.5）；
- 快照發佈、變更紀錄、套用在 `First`（5.6）；例外清單（5.6）；
- 錯誤碼與級別的對應，`UnknownId`、`TypeMismatch` 歸 E2（5.8）；
- 5.9.1 的雜湊函式；
- 文字 loader 的預設上限 16 MiB；stb_image 的編譯設定（5.11）；
- 12.2 的三題；
- 依賴的細節：經路徑引用由 catalog 反查、循環是錯誤、父項狀態不因依賴失敗而變、雜湊各 id 分開、不另加 `LoadedWithDependencies` 訊息（5.17）。

**和別家不同的地方**（照 `-confirm-only-differences` 列出，不是問題）：
- 資產管理 Product 一份（Bevy 每 App、其餘行程一份）；
- 去重的鍵只有 id，settings 隨 catalog（Bevy 是路徑＋型別）；
- catalog 執行期讀（Unity 是 build 的產物）；
- 套用在 `First`（Bevy 在 `PreUpdate`）；
- 釋放一段（Bevy 兩段；本批沒有渲染）；
- 值不可變（Bevy、Godot 可變）——使用者選的，不是卡片判的；
- 循環引用是錯誤（Bevy 沒有處理：`bevy_asset/src` 的 `server/`、`loader.rs` 搜 circular、cycle 0 件）；
- 原始位元組雜湊預設開（Bevy 只在 processor 用來源雜湊）。

## 12. 使用者的決定、卡片的判定、待使用者定的

### 12.1 使用者的決定（2026-11-21）

| # | 決定 | 原話 | 比過的選項 | 出處 |
|---|---|---|---|---|
| 1 | asset 三件 | 「asset 三件照建議」 | 見第 0 節 | 主會話轉來 |
| 2 | 載好的值不可變、共享唯讀（(a')） | 「照 (a')，寫修訂 2」 | (a) 不立、各 World 各自解碼；(a') 現在立 ←；(c) Product 級共用快取；(d) 單一擁有 World。其中「各 World 各自解碼」的前提由決定 7 取代，不可變照留 | 本卡 |
| 3 | 排除不需要使用者決策的 | 「排除掉不需要我決策的。」 | 全交使用者；只留真正要定的 ← | 本卡；第 2、4 題依修訂 2 檢查 B3 拉回，之後由使用者定（決定 9、10） |
| 4 | 時間上限放寬 | 使用者問「Asset 是機制還是功能？」「如果是機制，那麼未來修改成本如何？」，同意 (a) | (a) 放寬、全部修再送檢 ←；(b) 縮小範圍 | side chat |
| 5 | 跨執行緒約束現在寫進契約 | 「如果現在以單線程的方向去設計，如果未來需要多線程的時候，影響範圍多大？」 | 延後到 RC；現在寫 ← | side chat |
| 6 | 作者只用 id；Asset 提供能力 | 「99.9%的情況，作者都不需要關心資產如何導入到Engine中的……」「我們定的是用id，所以在製作遊戲中時，作者也只會call ID」 | 路徑當持久鍵、id 日後接（修訂 2）；作者只用 id，載入、就緒、生命週期由 Engine 提供 ← | side chat |
| 7 | 資產管理在 World 外 | 「【換 World 時要不要重讀、兩個 World 要不要共用】這個應該和asset 無關」；對「放在 World 外（Product 級或 Engine 級）」回「可以」 | 每 World 一份＋CarryOver（修訂 2）；World 外 ← | side chat |
| 8 | 大地圖先照 Bevy | 「我們先按Bevy的來，後續，如果需要，我們可以使用Module來增加地圖。」 | 現在設計串流；照 Bevy 不內建、日後 Module 加 ← | side chat |
| 9 | 第 4 題：單筆到了就可見＋一組 id 的就緒查詢；轉場歸作者 | 「a 選項有問題。你見過什麼遊戲是那種先看到地面，才看到樹的？」更正：「是我想叉了……確實 a選項直接展示是有問題的，但是這種loading 轉場是需要作者自己來做的。」複述分工後：「可以。」 | (a) 單筆到了就可見 ←（＋就緒查詢）；(b) 照請求序或整組才可見；(c) 預設 (a)＋作者可設組、Engine 保證同幀；另提過「以組為單位、Engine 推依賴」 | side chat |
| 10 | 第 2 題：asset 對每個 id 的原始位元組算雜湊，作為能力；預設開、可關；比不比與不同時怎麼辦歸作者或網路 Module | 「同意你的建議。」 | (a) 作者對載好的值算；(b) ←；(c) asset 算且當規則（不同就拒絕開局） | side chat |
| 11 | 依賴由 Engine 推出 | 「我認為b」 | (a) 作者自己列模型與它的貼圖、材質 id；(b) Engine 推出：loader 宣告引用，就緒查詢包含整串依賴 ←；(c) 本批只留宣告引用的介面，推算日後做 | side chat |

決定 4～11 的理由照 side chat 的轉述記在主會話的 HANDOFF。正文裡的「三件之 N」指決定 1 的第 N 件；「決定 N」指本表的編號。

### 12.2 卡片的判定（使用者看過、沒有拉回）

| 題 | 採 | 退 | 根據、代價 |
|---|---|---|---|
| 3 載完才開始 | 作者的事；asset 給就緒查詢（決定 9 同時確定） | Engine 規定進遊戲前載完 | `-build-world-phase-is-mechanism` 直接推出；異常分級只告知、不規定處理。代價：舊守衛（未載完就 ready 是錯誤）消失 |
| 5 圖片 | 獨立的 `engine/modules/image`，隨 SDL 那批第一個使用者做 | 放 render；本批做 | 原始碼事實：`bevy_image` 不依賴 render；批次先後由卡片判 |
| 6 預算 | 延後到第一個產品搬回時；本批只做放數字的位置與探索量測 | 現在給數字 | 本批沒有用到數字的 Feature；到時交使用者定數字 |
| （1 持久鍵） | — | — | 由決定 6 取代（id） |

### 12.3 待使用者定的

無。修訂 2 檢查 B3 拉回的第 2、4 題，使用者已定（決定 10、9）；依賴也已定（決定 11）。

**仍歸使用者的**：本頁的動工前核准；第 6 題的數字（產品搬回時）；stb_image 的下載（image 那批）。

## 13. 外部組件的下載

| 組件 | 來源 | 用途 | 授權 | 何時 |
|---|---|---|---|---|
| stb_image（`stb_image.h`） | nothings/stb，釘 commit | image Module 的 PNG 解碼 | MIT 或 Public Domain | SDL 那批的 image Module（12.2 第 5 題）。要使用者同意，到時再問 |

## 14. 範囲の限界

- **交換點、生成世界還沒有宣告**：5.6、5.13 是需求，不是設計。滿足不了的話先回報，不在 asset 裡繞過。
- **第 4 批還沒合併**：雜湊客製點與 `Summary` 的規則照修訂 6。合併時形狀不同，5.9.1 跟著改。
- **依賴只用測試用 loader 驗**：第一個真正的使用者（模型引用貼圖）在產品搬回或 SDL 那批（第 9 節）。
- **預算延後**：沒有載入時間、記憶體、catalog 讀取時間的判定。
- **載入中的 World 的 `Summary`**：asset 的視圖不寫進摘要，載入進度不同的兩個 World 摘要可能相同；比摘要要在「全部完成」之後比。
- **`Summary` 讀兩次相同的前提**：兩次之間 catalog 沒有換（同一份內容版本）。
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

## 16. 停止條件

- AGENTS §14 冒頭的三條。
- 設計段到 13 h 還收不了（0.1）。
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
| S8「來源」三義、魔數 | 來源名＝io 的 scheme（5.3、5.12）；疊掛載不再需要；loader 由 catalog 的（`type`、`format`）選，不靠魔數 |
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
| N8 時間 | 使用者放寬（決定 4）；0.1 寫新的估計與 13 h 的停止點 |
| N9 AS-1 漏 Extract | AS-1 寫明 Extract 是唯一跨 World 讀的路；守衛改成「讀到相等的值」 |
| N10 範圍代號與 DI 代號混 | 範圍改用 R1～R10 |

**修訂 3 本身**：送 Fable 5.2／high 敵對式檢查（第 20 節記結果）。

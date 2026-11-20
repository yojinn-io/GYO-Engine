# 事前宣告：JSON Module（`v8-arch-gyo2-json-module`）

更新：2026-11-20（修訂 1）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：動工前核准（2026-11-20）；照第 10 節實作，合併前做合併前核准。**

> **核准**：使用者 2026-11-20 在卡片對第 12 節四題與第 13 節的下載回「1、2、3 照建議，4 同意，可以下載」。即：第 1 題 view；第 2 題結構自寫、數字用 `yyjson_write_number`；第 3 題非有限數由 `Finish` 回 `NotRepresentable`（NaN 的表示法由日誌批定）；第 4 題全程 high、審查 Fable／high；同意下載第 13 節的兩個組件。已照四樣格式轉主會話。
>
> **2026-11-20 判定 3 更正（使用者，主會話轉達）**：第 9 節判定 3 的 `y_` 預期改成「93 件收、兩件 `y_object_duplicated_key*` 照 J-2 回 `RepeatedKey`」。原話「這個照建議」。見判定 3 與合併前核准頁。
>
> **2026-11-20 追加（使用者，卡片）**：adapter 在 Module 裡有自己的子目錄 `src/adapters/<組件>/`（一個組件一個）。原話：對卡片的建議「`src/backend/yyjson/`」回「我不想要backend這個名字。有其他選擇嗎？這個backend 太含糊了。」；卡片列 `src/adapters/yyjson/`＋`src/adapters/fast_float/`（建議；`adapter` 是 AGENTS §6 的詞）、`src/yyjson/`＋`src/fast_float/`、`src/external/`，使用者回「用第一個 adapters」。J-1、5.1、8.5 照此收緊（只緊不緩）。另：使用者確認外部組件照之前的 CMake 建置用 FetchContent（「FetchContent 可以用，參考之前的camke 建置」）。

經過：修訂 0（d142d8e8）送 Fable／high 敵對式檢查（general-purpose、只讀），必改 9、建議改 12、小問題 9（證據 `declaration-r0-adversarial-check.md`）。修訂 1 照它改，處理對照在第 17 節；修訂 1 沒有再送檢查。

性質：實作批的事前宣告（AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta、新 Component 的 Design Invariants 與 Replacement Criteria、§6 的外部組件 adapter）。第 6 組（基礎 Module）的第一批。

根據：
- HANDOFF 決定表（文件分支 `origin/claude/gyo2-time-design` 23def9e4）：`v8-arch-gyo2-json-early`（「這個算是封裝的實作」）、`-infrastructure-before-products`、`-module-plugin-owner-definition`（JSON 是 Module：介面是我們的，yyjson 是外部組件）、`-module-plugin-registers`（Module 登記檔、不進 World 的標記）、`-confirm-only-differences`、`-batch3-no-xhigh`、`-save-unaware-means-format-only`（存檔的格式是作者的）、`-replace-order-author-choice`（第 201 行；BigMap 是點名的 Feature：「有Feature，BigMap就是」）。
- master 時代、使用者已定、本批不重議的兩條（`docs/architecture/plans/kernel/HANDOFF.md:53-54`）：`v8-arch-json-yyjson-raw-number`（原話「yyjson，數字走原文路。（直接加GYO的介面，內化到Engine中未來有變化，也有介面作為緩衝）」）、`v8-arch-json-fast-float`（小數的轉換用 fast_float；浮點 `std::from_chars` 在 macOS 部署目標 13.3 不可用；「以調查卡同一組數字向量在 L1 三平台驗 0 不一致」）。細節：`adapter-plugins-plan.zh-Hant.md` 第 15、16、80、230～233、281 行（第 15 行：守衛只守 Engine 這一側）；計畫 `migration-plan.zh-Hant.md` §3.17c。這兩份寫於 Module／Plugin 重新定義（#240）之前：「Engine 的 JSON 模組」照 `gyo2` 的同詞異義表讀成「JSON Module」。和計畫不同的地方逐條列在第 12.0 節。
- 調查報告 `docs/architecture/plans/third-party-evaluation/json-library-survey.zh-Hant.md`（PR #216；向量、實測程式與 yyjson 0.13.0 原始碼在主 checkout `build/evidence/json-library-survey/`）。
- BigMap：`docs/architecture/plans/big-map/HANDOFF.md`（:56 同一陣列內物件數的二次方、:71 契約只有 JSON 文字與 region 平行解析由呼叫端做、:79 區塊化地圖 242 MB、:221 B2 `bad_alloc` 漏出）、`engine-format.zh-Hant.md`（:43、:96 每個 region 文字 ≤ 16 MiB；:264 每層存完整路徑的記憶體；:282 「轉成錯誤或加深度上限」沒做）、`engine-grid.zh-Hant.md:170`（5.3 MB ≤ 450 ms）。
- 批次表（文件分支）第 40～46 行：第 6 組「JSON Module（最先）→ 日誌機制…trace 一起看 → … → asset（依賴 2；JSON）」。
- AGENTS §6；`docs/architecture/error-handling.md` 規則 1～7 與 Assert 節（`GYO_CHECK_ASSERTS`）；`docs/architecture/target/determinism.md` §4.6、§4.7a（契約資料解析是 (c) 路徑）。
- 命名：`rebuild-naming.zh-Hant.md` N11～N17。
- 程式：`origin/gyo2` 7673a419。

**標記**：寫「卡片照 L17 判」的，是任務卡照命名規範、別家原始碼與已定的原則自己判的，不是使用者的話（使用者 2026-11-17、2026-11-19 `-confirm-only-differences`）。交使用者的只有第 12 節。

證據（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-json-module/`，git 不追蹤）：`reference-survey.md`（別家對照與本機實測的出處、tag 的 commit、調查向量的雜湊）、`declaration-r0-adversarial-check.md`（修訂 0 的檢查）。

---

## 0. 時間估計

| 段 | 估計 | 內容 |
|---|---|---|
| 設計段 | 3～4 h（到修訂 1 約 3.5 h） | 讀現況與 master 的決定、對照別家、本頁、Fable 檢查與修訂 |
| 實作段 | 22～34 h（推定） | 第 10 節的 7 步：外部組件加入與查證 2～3、骨架 1、讀 5～7、數字與向量重生 4～6、寫 3～5、情境測試與守衛 2～3、突變與文件與收尾 5～9 |

超過 1.5 倍（設計段 6 h、實作段 51 h）或單一問題累計超過 2 小時，停下回報（AGENTS §14 冒頭）。master 計畫的估時 22～44 h 含 7 個既有使用者的遷移；`gyo2` 上沒有既有使用者，本批不含遷移。

## 1. 這一批做什麼（先用遊戲講）

- **trace 一行一筆**：下一批的日誌與 trace 要把「第 1207 tick，玩家 3 的移動指令排隊 2 筆」寫成一行 JSON。master 的 PvP 在 producer 執行緒上手寫字串（`master@08566260:apps/object_fps_pvp/include/RetroFPS/Pvp/MovementTraceWriter.hpp:30-42`，每行約 420 位元組、`reserve(420)`）；Kernel 的登記指紋報告也手寫了一個 JSON 字串跳脫（`engine/kernel/src/Fingerprint.cpp:231`）。本批給一個串流寫出器：照呼叫順序往字串後面接，跳脫、數字格式、結構都由它負責。
- **讀內容檔**：之後的 asset 批讀資源目錄，BigMap 的每個 region（最大 16 MiB）可以在多個執行緒上同時讀，UI 讀 UI 文件，作者若選 JSON 當存檔格式也要讀回來。本批給一個嚴格的讀取器：同一個鍵出現兩次、檔頭有 BOM、數字超出 double 的範圍都是錯誤；同一份檔在 Windows、Linux、macOS 讀出逐位元相同的數值；再深、再寬的文件，時間與記憶體都跟值的個數成正比。
- **換函式庫只改一處**：yyjson 只出現在 `engine/json/src/adapters/yyjson/`、fast_float 只出現在 `engine/json/src/adapters/fast_float/`；之後換函式庫，Engine 裡用它的 Module 都不用改（使用者原話「內化到Engine中未來有變化，也有介面作為緩衝」）。

## 2. 範圍

### 2.1 這一批做的（J1～J8）

| 代號 | 內容 |
|---|---|
| J1 | `third_party` 加 yyjson 與 fast_float（第 13 節，使用者同意後才下載）；各自一個 GYO 的 target，只給 `engine/json` PRIVATE 連 |
| J2 | 新 Module `engine/json`：Module 登記檔加一列（不進 World）、CMake target `gyo_json`／`GYO::Json`、namespace `GYO::Engine::Json` |
| J3 | 錯誤型別 `JsonError`（`::Engine::Base::Error<JsonErrorCode>`）與 `JsonResult<T>` |
| J4 | 讀：`Parse` → `Document`；`Value`、`ObjectView`、`ArrayView`，依文件順序走訪（第 5.2 節） |
| J5 | 數字：原文路（yyjson 交原文，小數以 fast_float、整數以 `std::from_chars` 轉）；解析時驗證每個數字（第 5.3 節） |
| J6 | 寫：串流的 `Writer`（第 5.4 節；數字格式照第 12 節第 2 題）；`WriteValue`（把讀到的 `Value` 寫出，數字照原文） |
| J7 | 測試、數字向量在 L1 重生、情境測試、連結與含入守衛、突變清單（第 8 節） |
| J8 | 文件：新頁 `target/v2/json.zh-Hant.md`（Design Invariants J-1～J-9 與 Replacement Criteria）、v2 README 索引、dev_log、合併前核准頁 |

### 2.2 不在這一批

| 項目 | 留到 | 為什麼 |
|---|---|---|
| 可編輯的文件（讀進來改值、插入、刪除再寫回，保鍵序；`gyo.ui` 往返） | 第一個要它的批（UI 或工具） | 直近沒有使用者（AGENTS §2「実装は薄く始める」）。形狀到那一批再定；它的寫出經本批的 `Writer`（J-5：只有一種輸出格式）。和 master 計畫的差異，第 12.0 節 |
| 縮排排版的輸出（給人看、給 git diff） | 第一個寫內容檔的工具批 | trace 用一行一筆；縮排是 `Writer` 的一個選項，到時加 |
| 型別對應（flecs `ecs_ptr_to_json`、Bevy serde＋`bevy_reflect` 那種） | 不排 | GYO 沒有反射；存檔格式是作者的。RC-3 寫什麼時候重看 |
| Kernel 指紋報告改用本 Module | 交主會話（第 15 節） | Kernel 的連結守衛是 Base＋EntityStore（`tests/common/kernel/CMakeLists.txt:239`）；本批不碰 Kernel |
| JSON Pointer 查詢、JSON Patch、增量解析、寬鬆語法 | 不排 | 沒有使用者；寬鬆語法違反 J-2 |
| 產品側（apps、tools）的含入規則 | 不設 | 使用者已定守衛只守 Engine 這一側（`adapter-plugins-plan` 第 15、464 行）；產品要不要直連 yyjson 是產品自己的事。一組件一 adapter 由 Engine 這一側的守衛守 |
| 授權告示收進產品的打包 | 打包流程回到 `gyo2` 時 | `gyo2` 現在沒有打包 |

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

JSON Module 的介面上會積：日誌與 trace 的紀錄格式、資源目錄、BigMap 的 region、UI 文件、工具交接的檔案、作者的存檔。介面一旦被用上，改讀取的形狀（view 或樹）、錯誤的分類、數字的語義，每個使用者都要改，而且越積越多；數字語義還牽涉保存下來的資料與跨平台的決定性（determinism §4.7a）。所以照基盤處理：予見的要求寫進 Contract 與 Design Invariants，只實作直近要用的部分。

### 3.2 予見的根據 → 基盤的要求（Feature → Contract → Architecture）

根據的種類：1＝名指的 Feature、2＝成熟的參考實作、3＝規模與預算（AGENTS §2）。

| # | Feature | 根據 | Contract | Architecture | 本批 |
|---|---|---|---|---|---|
| F1 | 日誌與 trace 的結構化紀錄（一行一筆；之後的分析工具讀它） | 1：批次表第 42 行（下一批）；觀察：master 的 PvP 手寫 JSON 行、Kernel 手寫跳脫；3：每行約 420 位元組、在 producer 執行緒上組 | 串流寫出：照呼叫順序往呼叫者的字串追加；不建 DOM；容量足夠時不配置；輸出逐位元決定；寫出的一定讀得回來（前提：呼叫者不在同一物件寫重複鍵） | `Writer`（第 5.4 節） | 實作 |
| F2 | 資源目錄（asset）讀內容 | 1：批次表第 44 行；master 的 `ContentManifest.cpp`、`CatalogParser.cpp` | 嚴格解析（J-2）、數字原文路與三平台同值（J-3）、型別不符是 `Result` 錯誤（error-handling 規則 2）、依文件順序走訪 | `Parse`／`Document`／view（第 5.2 節） | 實作 |
| F6 | BigMap：區塊化地圖的 region 載入 | 1：`-replace-order-author-choice`（「有Feature，BigMap就是」）、big-map HANDOFF :71（region 平行解析由呼叫端做）；3：region 文字 ≤ 16 MiB（engine-format :43、:96）、整張 242 MB（HANDOFF :79）、5.3 MB ≤ 450 ms（engine-grid :170）；踩過的坑：同陣列物件數二次方（HANDOFF :56）、每層存路徑的記憶體（engine-format :264）、`bad_alloc` 漏出（HANDOFF :221 B2） | 多個執行緒同時 `Parse` 不同文件；時間與記憶體對值的個數與深度線性（走訪不遞迴、JSON Pointer 只在出錯時組）；陣列與物件依序走訪是每步常數時間；配置失敗回 `Result`（`OutOfMemory`），不讓 `bad_alloc` 從 `Parse` 漏出 | 同 F2；走訪的 view 以 yyjson 的迭代器實作；`Parse` 在邊界攔 `std::bad_alloc` | 實作 |
| F3 | UI 文件讀取（之後 UI 編輯器寫回） | 1：主會話對 `-infrastructure-before-products` 的讀法列了 ui（使用者只更正了 platform 與 net）；master `UiDocumentCodec.cpp`（`.items()` 走訪成員 8 處） | 讀同 F2（走訪成員）；寫回保鍵序（可編輯文件，延後） | 讀用 F2 的 | 讀＝實作；寫回＝延後 |
| F4 | 作者的存檔（選 JSON 時） | 1：編排層情境 S1-5（`orchestration-skeleton-declaration` 第 546 行）、`-save-unaware-means-format-only`；2：Bevy `DynamicWorld`、flecs `ecs_world_to_json` | 作者用 `Writer` 寫、用 view 讀；Engine 不定存檔格式 | 用 F1、F2 的 | 不另做 |
| F5 | 工具經資料契約交接（AGENTS §8） | 1：AGENTS §8；記憶 engine-not-tool-duties | 工具寫的檔要讓 Runtime 讀得進（J-5 往返）；`GYO::Json` 對產品公開（Tool → Engine 是允許的方向），產品用不用它是產品自己的事 | 不另做 | 縮排、可編輯文件延後 |
| — | `engine/config` 的登記檔 | 查證：`engine/config/*.csv` 是 CSV，由 CMake（`build/cmake/GyoUnitRegistry.cmake`）讀 | 無 | 不動 | — |
| — | 網路 Module 的 wire | 查證：沒有點名 JSON（master 的 wire 是 protobuf） | 無 | 不動 | — |

導不出的要求：沒有。大小與深度的上限見 5.6 節（明寫「不設上限＋理由」）。

### 3.3 Feature 與 Product

- 用到本批實作部分的 Product：Game（PvP 的 trace、資源目錄、BigMap、存檔）與 Tool（之後的編輯器）。本批沒有產品程式；F1、F2、F6 由情境測試代表（第 8.4 節），真正的使用者是下一批（日誌與 trace）與 asset 批。
- 支撐的下一批 Feature：日誌與 trace 的結構化輸出（F1）。

## 4. 別家怎麼做（同一種技術比）

本機原始碼：flecs 09790b5f7、Bevy 6a911653f、yyjson 0.13.0（調查時下載，在證據資料夾）。**Unreal、Godot、O3DE、serde_json、simdjson 的行為來自知識（本機沒有原始碼），沒有查證**；只用來說明分歧，不是任何一條判法的唯一根據。行號在 `reference-survey.md`。

| 技術 | yyjson（要包的） | flecs `flecs_json` | Bevy | Unreal（知識） | GYO 本批 |
|---|---|---|---|---|---|
| 讀成樹 | 唯讀 DOM `yyjson_doc`；陣列與物件是串列，`yyjson_arr_get`／`yyjson_obj_get` 線性（`yyjson.h:2168-2172`、`:2282`），迭代器每步常數 | 沒有樹：自己的 tokenizer（`src/addons/json/json.c:42`）直接讀進反射描述的型別 | Engine 本身不讀 JSON；serde 的格式無關資料模型；glTF 用 `serde_json`（`crates/bevy_gltf/Cargo.toml:71`） | `FJsonObject`（自己的樹） | yyjson 唯讀 DOM 上的 view（第 12 節第 1 題） |
| 串流寫出 | 沒有串流 API；有可變 DOM＋固定緩衝配置器（`yyjson_alc_pool_init`，`:719`）＋不配置的 `yyjson_mut_write_buf`（`:1593`）；有不需要 DOM、不配置、執行緒安全的 `yyjson_write_number`（`:1885-1905`） | 自己的寫出（寫進 `ecs_strbuf_t`，`src/addons/json/json.h:106-187`） | `serde_json::Serializer` | `TJsonWriter` | 結構與跳脫自己寫、數字用 `yyjson_write_number`（第 12 節第 2 題） |
| 數字（讀） | 預設自己轉；`YYJSON_READ_NUMBER_AS_RAW` 交原文 | 自己轉 | serde_json 自己轉 | 自己轉 | 原文路＋fast_float（已定） |
| 非有限數（寫） | 預設錯誤（`YYJSON_WRITE_ALLOW_INF_AND_NAN` 才寫，`:1264`） | 寫成字串 `"NaN"`／`"Inf"`（`src/datastructures/strbuf.c:81-92`、`json.c:395`） | serde_json 寫 `null`（知識） | — | 第 12 節第 3 題 |
| 重複鍵 | DOM 保留全部成員，自己走訪偵測（調查 §2.2） | — | serde 看型別 | 後者蓋前者 | 解析後走訪偵測，錯誤 |
| 深度 | 解析不遞迴；`YYJSON_READER_DEPTH_LIMIT` 預設 0（不限，`:115`） | — | — | — | 不限，走訪也不遞迴（5.6） |
| 型別對應 | 無 | `ecs_ptr_to_json` 等（靠 meta 反射；`include/flecs/addons/json.h:69-169`） | serde derive＋`bevy_reflect`；場景用 RON | `FJsonObjectConverter` | 不做 |

## 5. 設計（Contract）

### 5.1 位置與名字（卡片照 L17 判，照 N11～N17）

- 目錄 `engine/json/`，標頭 `engine/json/include/engine/json/`，實作 `engine/json/src/`。**直接碰外部組件的程式（adapter）放 `src/adapters/<組件>/`，一個組件一個子目錄**：`src/adapters/yyjson/`（`Parse`、`Document`、`Value` 走 yyjson 的 DOM，數字寫出走 `yyjson_write_number`）、`src/adapters/fast_float/`（數字讀取的十進位轉 double）。`src/` 的其他檔不碰外部組件（`Writer` 的結構、跳脫、UTF-8 檢查、錯誤碼）。不另開 CMake target、不加執行時的抽象介面：換函式庫是換 `adapters/` 底下的目錄（使用者 2026-11-20 定，見文首〈核准〉）。
- target `gyo_json`（STATIC），別名 `GYO::Json`；namespace `GYO::Engine::Json`（N17）。在這個 namespace 裡寫 Base 要寫 `::Engine::Base::`（同 `engine/orchestration/include/engine/orchestration/Product.hpp:82`）。
- Module 登記檔加一列：`json,GYO::Json,"JSON reading and writing behind the engine's own interface",yes,no,base,,,`——`library_only=yes`、`default=no`、依賴只有 `base`；排在 `base` 之後。
- 外部組件：`third_party/yyjson/`、`third_party/fast_float/`。照 master 的 ufbx 先例（`FetchContent`、`SOURCE_SUBDIR` 指向不存在的目錄以不跑上游 CMake、GYO 自己建 target），再加三點：`GIT_TAG` 寫 40 位元的 commit SHA（tag 可以移動；第 13 節的表）、`GIT_SUBMODULES ""`、GYO 的 target 以 `SYSTEM` 含入（`gyo_json` 開 `-Wall -Wextra -Wpedantic`／`/W4`）。target 名 `gyo_yyjson`（STATIC，C）、`gyo_fast_float`（INTERFACE）。只有 `engine/json/CMakeLists.txt` 以 PRIVATE 連它們。
- 模組之間的依賴照 O-6 只寫在登記檔；外部組件不是 Module，連結寫在 `engine/json/CMakeLists.txt`（同 ufbx）。
- **下載的後果**：`third_party/CMakeLists.txt` 現在只在 `BUILD_TESTING` 時加 doctest；JSON 是 Engine 的 Module，yyjson 與 fast_float 不論建不建測試都加，所以**每個新的建置目錄第一次 configure 都要連 GitHub**（之後 `FETCHCONTENT_UPDATES_DISCONNECTED ON` 不再更新）；L1 的 runner 是一次性的，每次跑都下載。沒有網路的場合用 `FETCHCONTENT_SOURCE_DIR_YYJSON`／`_FAST_FLOAT` 指向事先放好的原始碼（CMake 內建）。
- **退掉「原始碼放進 repo」的理由**（不是「沒有先例」）：釘 commit SHA 後內容由 Git 的雜湊定死，重現性與「放進 repo」相同；供應鏈上兩者都只信任一個固定的內容；離線靠上面的覆寫；放進 repo 要多約 1 MB 與三份授權檔，升版是大段 diff。差別只在離線的第一次 configure，取 FetchContent。

### 5.2 讀

```cpp
namespace GYO::Engine::Json {
enum class Kind : std::uint8_t { Null = 1, Bool, Number, String, Array, Object };

class Document {                                      // move-only; owns the parsed data
public:
    Value Root() const;
};
[[nodiscard]] JsonResult<Document> Parse(std::string_view text);

class Value {                                         // a view into a Document
public:
    Kind GetKind() const noexcept;
    bool IsIntegerLiteral() const noexcept;           // a number written without '.', 'e' or 'E'
    JsonResult<bool> AsBool() const;
    JsonResult<std::int64_t> AsInt64() const;
    JsonResult<std::uint64_t> AsUint64() const;
    JsonResult<double> AsDouble() const;
    JsonResult<std::string_view> AsString() const;
    JsonResult<std::string_view> AsNumberText() const; // the number's text as written
    JsonResult<ObjectView> AsObject() const;
    JsonResult<ArrayView> AsArray() const;
};
class ObjectView { // range-for yields Member{key, value} in document order
public:
    std::size_t Size() const noexcept;
    std::optional<Value> Find(std::string_view key) const; // linear in Size()
};
class ArrayView {  // range-for yields Value in document order
public:
    std::size_t Size() const noexcept;
};
}
```

名字與簽名是草案，實作時照命名規範微調不必再問；**形狀**是 Contract：view、型別不符回 `Result`、陣列與物件依文件順序走訪且每步常數時間、`Find` 線性、沒有隨機索引。

- **沒有 `operator[]`**：yyjson 對含容器的陣列取第 i 個是線性的，用索引跑迴圈就是二次方（BigMap 踩過的坑，HANDOFF :56）。只給依序走訪；要隨機存取的使用者自己把走訪結果放進陣列。
- **view 與生命週期**：`Document` 擁有解析出的資料；`Value`、view、取出的 `std::string_view` 在 `Document` 存活期間有效，移動 `Document` 不失效。`Document` 解析後不再引用輸入文字（yyjson 非 insitu 模式會複製輸入；在第 3 步以測試固定）。
- **型別不符是 Runtime Error**：內容檔說「這裡是陣列」而讀的人要物件，是資料錯誤（error-handling 規則 2），回 `WrongKind`，細節寫期待與實際的種類。（卡片照 L17 判：yyjson `yyjson_get_*` 型別不符回 0／NULL；serde_json、Unreal 也當資料問題——後兩者來自知識。）
- **物件**：`Find` 回唯一的那一個（解析已保證沒有重複鍵）；查不到回 `std::nullopt`（缺欄位算不算錯誤由各資料契約定）。
- **多執行緒**：不同的執行緒可以同時 `Parse` 不同的文件（BigMap 的 region 平行解析），也可以同時讀同一份 `Document`（yyjson 沒有全域狀態、唯讀文件可多執行緒讀：`doc/API.md:1840-1842`）。

### 5.3 數字（原文路；已定，這裡只寫細節）

- yyjson 以 `YYJSON_READ_NUMBER_AS_RAW` 交出原文；JSON 的數字文法由 yyjson 驗（調查 §2.1：原文旗標下只多接受超大指數，由下一條擋）。
- **解析時驗證每個數字**：每個數字的原文都要轉得成**有限**的 double，而且**非零的文字不得捨入成 0**；否則 `Parse` 回 `NumberOutOfRange`。「非零的文字」指有效數字裡有非 0 的數字：`0e999999`、`-0e-999`、`0.000` 不算下溢，讀成 ±0。所以「解析成功」表示整份文件的數字都能讀成 double；不讀的欄位裡的 `1e400` 一樣讓解析失敗。（master 的契約：下溢與溢位是錯誤，`adapter-plugins-plan` 第 233 行。`gyo2` 沒有既有使用者，沒有締める面。）
- **小數**：`AsDouble` 以 fast_float 轉原文，三平台同一份實作。正確捨入（round-to-nearest-even）。次正規數照常讀。整數字面也可以用 `AsDouble` 讀（同樣正確捨入；大於 2^53 的整數捨入到最近的 double）。每次 `AsDouble` 都重新轉一次（解析時驗證過一次；不另存，見第 12 節第 1 題的代價）。
- **整數**：`AsInt64`／`AsUint64` 只收整數字面；`1.0`、`1e2` 回 `WrongKind`；超出範圍回 `NumberOutOfRange`；`AsUint64` 讀負的整數（`-5`）也回 `NumberOutOfRange`（數學上的值不在 uint64 的範圍；`std::from_chars` 的無號版對它回 `invalid_argument`，adapter 自己分辨，不照搬）。整數以 `std::from_chars` 轉（整數版在 13.3 可用）。（卡片照 L17 判：yyjson `yyjson_is_int` 對 `1.0` 為假；serde_json、simdjson 也嚴格——來自知識。）
- **`-0`**：`AsDouble` 得 `-0.0`；`AsInt64` 得 `0`；`AsUint64` 得 `0`——**這是 adapter 自己的特例**：`std::from_chars` 的無號版不收 `-0`（檢查者實測 ec=22），adapter 先認出 `-0` 再轉。（卡片照 L17 判：整數沒有負零；master 留給本宣告定，最終檢查 S-11 (3)。）
- **原文**：`AsNumberText` 交出原文（內容身分若要雜湊十進位原文，用它）；`IsIntegerLiteral` 讓讀的人先問再選 `AsInt64` 或 `AsDouble`（master 的 `is_number_integer` 用法）。
- fast_float 對溢位與下溢怎麼回報（`result_out_of_range` 或回 inf／0）在第 1 步查證；不論哪一種，adapter 都以上面的定義判，不靠函式庫的回報方式。

### 5.4 寫

```cpp
class Writer {                       // appends to the caller's string
public:
    explicit Writer(std::string& out);
    void BeginObject(); void EndObject();
    void BeginArray();  void EndArray();
    void Key(std::string_view key);
    void String(std::string_view text);
    void Int(std::int64_t); void Uint(std::uint64_t); void Double(double);
    void Bool(bool); void Null();
    [[nodiscard]] JsonResult<void> Finish();
};
void WriteValue(Writer& writer, Value value);
```

- **形狀照第 12 節第 2 題的建議**：結構、跳脫、UTF-8 檢查由本 Module 自己寫；數字格式交給 yyjson 的 `yyjson_write_number`（不需要 DOM、不配置、執行緒安全）。所以 yyjson 的寫出部分要編進來（不設 `YYJSON_DISABLE_WRITER`：它會連 `yyjson_write_number` 一起拿掉，`yyjson.h:1353-1913`）。
- **輸出**：緊湊（無空白）；鍵與值照呼叫順序；字串照 RFC 8259 跳脫（`"`、`\`、控制字元；`\b \f \n \r \t` 用短形，其他控制字元 `\u00XX`；`/` 與非 ASCII 原樣輸出 UTF-8）。
- **數字格式**（yyjson 0.13.0 實測，`reference-survey.md`）：整數照十進位；double 是最短往返形，沒有小數點與指數時保留 `.0`——`100000.0`、`1000000000000000.0`、`1e21`、`1e23`、`0.0001`、`-0.0`、`5e-324`、`1.5e-7`。double 寫出再讀回得同一個位元型樣，讀回時仍是小數字面（`IsIntegerLiteral` 為假）。數字格式是 yyjson 一份 C 程式碼，三平台相同（同讀取端用 fast_float 的理由）；由跨平台 golden 守。
- **重複鍵是呼叫者的責任**：`Writer` 不偵測同一物件裡同一個 `Key` 寫兩次（要偵測就得記每層的鍵、配置記憶體，違反 F1 的零配置）。寫了的輸出會被 `Parse` 以 `RepeatedKey` 拒——J-5 的往返以「呼叫者不寫重複鍵」為前提。這屬 Programmer Error，但不檢查（同 `std::vector` 的越界在 release 不檢查；文件寫明）。
- **不可表示的資料**：非有限數照第 12 節第 3 題；`String`／`Key` 不是合法 UTF-8（接受的集合和 yyjson 讀取端相同：拒過長編碼、代理碼位 U+D800～DFFF、大於 U+10FFFF、截斷的序列；`\u0000` 經跳脫可寫）——記下第一個錯誤，`Finish` 回 `NotRepresentable`（細節：哪一種、第幾個值）。**`Finish` 失敗時把 `out` 截回建構時的長度**（多行共用一個字串時，呼叫者不必自己記位置）。
- **結構誤用是 Programmer Error（斷言）**：物件裡沒先 `Key` 就寫值、物件外寫 `Key`、`End` 對不上、頂層寫第二個值、`Finish` 時還有沒關的、`Finish` 後再寫（error-handling 規則 3）。測試照 repo 的做法用 `GYO_CHECK_ASSERTS`（丟例外的 handler；error-handling 的 Assert 節）。
- **配置**：只往 `out` 追加；巢狀狀態放在 32 層的內建空間，超過 32 層才配置。**`out` 的容量足夠、而且深度不超過 32 時，整筆不配置記憶體**。trace 可以重用同一個字串（`clear()` 保留容量）。配置失敗（`std::bad_alloc`）在每個呼叫的邊界攔下、記成錯誤，`Finish` 回 `OutOfMemory`（error-handling 規則 6：邊界攔 `std::exception` 的子類，不用 `catch (...)`）。
- **`WriteValue`**：把讀到的 `Value` 寫出；物件成員照文件順序；**數字照原文寫出**（`AsNumberText`；`0.10` 仍是 `0.10`，超出 64 位元的整數不變形）；字串照上面的跳脫重寫。走訪不遞迴。
- **往返**：`Writer` 的輸出（不寫重複鍵時）一定能被 `Parse` 讀回，讀回的值與寫入的相同（double 逐位元、字串逐位元組、鍵順序相同）。

### 5.5 錯誤

```cpp
enum class JsonErrorCode : std::uint16_t {
    Syntax = 1,        // not JSON: grammar, invalid UTF-8 or escapes, BOM, trailing content, empty text
    RepeatedKey,       // a key appears twice in one object (after unescaping)
    NumberOutOfRange,  // overflow, non-zero text rounding to zero, or an integer read out of range
    WrongKind,         // read as a kind the value is not
    NotRepresentable,  // Writer: a non-finite number (per question 3) or invalid UTF-8
    OutOfMemory,       // parsing or writing could not allocate
};
using JsonError = ::Engine::Base::Error<JsonErrorCode>;
template <class T> using JsonResult = ::Engine::Base::Result<T, JsonError>;
```

- 細節：`Syntax` 寫「line L, column C」（1 起算、欄以位元組計；從 yyjson 回的位元組位置自己算——`yyjson_locate_pos` 的欄以 Unicode 字元計，不用）；`RepeatedKey` 與解析時的 `NumberOutOfRange` 寫行與欄（鍵與數字原文指向 yyjson 自己複製的緩衝 `str_pool`，以位移換算；第 3 步查證，換算不成立時只寫 JSON Pointer）＋JSON Pointer（例 `/items/3`）；`WrongKind` 寫期待與實際的種類。用的模組照規則 5 以 `::Engine::Base::CauseDetail` 轉成自己的碼。
- 配置失敗的處置只有一種：`Parse`（yyjson 回配置失敗，或 adapter 自己的配置丟 `std::bad_alloc`）與 `Writer` 都回 `OutOfMemory`，不讓 `bad_alloc` 從公開介面漏出（BigMap B2 的教訓）。
- 不丟例外（規則 6）：yyjson、fast_float 不丟例外；adapter 內部的 `std::bad_alloc` 在公開函式的邊界攔下（`catch (const std::bad_alloc&)`）。
- 碼的數值不是資料契約。

### 5.6 執行緒、記憶體、上限

- 沒有全域可變狀態（不設 yyjson 的全域配置器、不設 locale）；`Writer` 一個執行緒一個。
- yyjson 用預設的 `malloc` 配置器；GYO 還沒有記憶體管理的方針，換配置器只改 adapter。
- **不設大小與深度的上限**（明寫）：yyjson 的解析不遞迴、深度上限預設 0（`yyjson.h:115`）；本 Module 的走訪（重複鍵、數字驗證、`WriteValue`）也不遞迴，JSON Pointer 只在出錯時從走訪的堆疊組出；所以時間與記憶體都對值的個數與深度線性，極大或極深的文件最多得到 `OutOfMemory`，不會用盡堆疊、不會二次方。理由：任何合法的內容都讀得進（BigMap 的 region 16 MiB）；上限是資料契約的事，各契約自己定（例 BigMap 的 16 MiB）。使用者自己的遞迴走訪要自己處理深度（文件寫明）。Boost.JSON 預設 32 層、yyjson 預設不限（調查 §2.1、`yyjson.h:115`）——本 Module 照它包的 yyjson。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature**：F1（下一批）、F2、F6；使用者決定 `-json-early`、`v8-arch-json-yyjson-raw-number`。
2. **現在的構造有什麼問題**：`gyo2` 沒有 JSON 能力；master 時代 7 個模組各自直連 nlohmann（違反一組件一 adapter）、`StrictJson` 以 nlohmann 型別為模板參數；另外 PvP 與 Kernel 各自手寫 JSON（同樣的跳脫與數字格式重複，AGENTS §2 Valid evidence「同じ Conversion の繰り返し」）。
3. **哪個邊界改變**：新 Module `engine/json`（新 Subsystem）；`gyo2` 第一個外部組件 adapter（yyjson、fast_float 兩個組件，都只在這個 adapter）；`third_party` 多兩個不分建置種類都加入的組件。
4. **影響的 Module／Product／Tool**：新增 `engine/json`、`third_party/yyjson`、`third_party/fast_float`、`engine/config/modules.csv` 一列、`tests/common/json`。既有 Module 都不改。
5. **依賴方向**：`GYO::Json → GYO::Base`；`GYO::Json → gyo_yyjson、gyo_fast_float`（PRIVATE）。沒有 Module 依賴 JSON（之後日誌、asset 依賴它）。沒有循環。
6. **Ownership**：JSON 的讀寫與數字轉換歸 JSON Module。yyjson、fast_float 的版本與建置歸 `third_party`，Engine 裡的使用者只有 JSON Module。
7. **為什麼不能更小**：不開 Module、讓日誌自己寫 JSON 字串＝重複 master 的手寫（跳脫與數字格式出現在日誌、PvP、Kernel 三處）；只做寫出不做讀＝已定的原文路與三平台數字驗證延到 asset 批，基盤的反復留到上面積了日誌之後（AGENTS §2「基盤の上に機能を積み上げ始める前に、基盤の反復を済ませる」）。

**Batch declaration checks**（AGENTS §4、`target/README.md` 每批事前宣告的檢查項）：

| 選擇 | 系譜 | 退掉的選項與決定理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 新 Module `engine/json`（不放 Base） | 新導出；master 把 `StrictJson.hpp` 放在 Base（R0 不帶） | 放 Base：Base 是「只依賴標準庫」的最底層（`engine/base/CMakeLists.txt` 註解），放進來會讓每個連 Base 的模組（含 Kernel）間接連到 yyjson——是在為舊形狀找位置，不採 | F1、F2、F6；Game、Tool |
| 讀取用 view、只給依序走訪 | yyjson 唯讀 DOM 的形狀；新導出 | 自己的樹：第 12 節第 1 題；隨機索引：yyjson 上線性、會二次方（BigMap 的坑） | F2、F3、F4、F6 |
| 寫出：結構自寫、數字用 yyjson | flecs、Unreal、serde_json 的串流寫出＋yyjson 的數字寫出 | 全部自寫（數字用 `std::to_chars`）、yyjson 可變 DOM：第 12 節第 2 題 | F1、F5 |
| 外部組件以 FetchContent 釘 commit SHA、GYO 自己建 target | master 的 ufbx、doctest | 原始碼放進 repo：5.1 節的比較（重現性、供應鏈、離線、大小） | — |

既有的 Test、守衛、golden 都不受影響，「不必改測試」沒有被當成決定理由。

## 7. Design Invariants（新頁 `target/v2/json.zh-Hant.md`，見出し `Design Invariants`）

本批新增，最終文字在合併前核准定稿。守衛欄：行為的測試照常是守衛；綁實作方式的含入掃描在情境測試通過之後才立（AGENTS §3「ガードを立てる時期」），同一批的第 6 步。連結的期待（`gyo_expect_links`）與「公開標頭編不過」是現有的守衛形式，和測試同時立。

| 條 | 內容 | 守衛 |
|---|---|---|
| **J-1 唯一的 adapter** | yyjson 的標頭、型別、呼叫只在 `engine/json/src/adapters/yyjson/`，fast_float 的只在 `engine/json/src/adapters/fast_float/`；`engine/json/include/` 不含它們，也不露出它們的型別；Engine 的其他 Module 經 `GYO::Json` 讀寫 JSON，不直接連 `gyo_yyjson`、`gyo_fast_float`。（AGENTS §6；「一個外部組件」的數法：yyjson 一個、fast_float 一個，兩個都只在這個 adapter。守衛只守 Engine 這一側，`adapter-plugins-plan` 第 15 行） | `gyo_expect_links(gyo_json GYO::Base gyo_yyjson gyo_fast_float)` 與 interface 不露出（`tests/common/time/CMakeLists.txt:16`、`:41-44` 的寫法）；含入掃描（第 6 步，照 `tests/common/kernel/kernel_guard.py` 的 Python 形式；範圍 `engine/` 與 `tests/common/json/`） |
| **J-2 嚴格解析** | 只收 RFC 8259 的 JSON 文字：UTF-8、無 BOM、無註解、無尾隨內容（含數字後接 NUL）、無 NaN／Infinity 字面、非空；不成對或不完整的代理跳脫是錯誤。同一物件裡解跳脫後相同的鍵出現兩次是錯誤。每個數字都要轉得成有限的 double，非零的文字不得捨入成 0。違反都是 `Parse` 回的 `Result` 錯誤，不是斷言、不是取最後一個。 | 單元測試（每種各一，位元組字面寫在測試裡）＋本機的 JSONTestSuite 全集（證據） |
| **J-3 數字走原文路、三平台同值** | 讀出的數值只由本 Module 的轉換得到：小數 fast_float、整數 `std::from_chars`；不用函式庫自帶的數字轉換，也不用平台的 `strtod`。同一份文字在 L1 三平台得到逐位元相同的值（determinism §4.7a）。整數讀取只收整數字面；`-0` 讀成 `-0.0`（小數）與 `0`（整數，含無號）；`AsUint64` 讀負數回 `NumberOutOfRange`。 | 調查的全部向量在 L1 三平台重生並比對（8.2）＋固定表 |
| **J-4 Engine 的詞** | 公開介面只有 GYO 的型別；不露出函式庫的 DOM 型別、旗標、錯誤碼。換函式庫時 Engine 裡的使用者不用改。 | 測試執行檔只拿得到 `engine/json/include` 的路徑：公開標頭含 `yyjson.h` 就編不過（entity-store DI 9 的先例）；J-1 的掃描 |
| **J-5 寫出端** | `Writer` 的結構、跳脫、UTF-8 檢查由本 Module 實作，數字格式只有一份實作（第 12 節第 2 題）。輸出是 `Parse` 讀得回的嚴格 JSON，讀回的值與寫入的相同（前提：不寫重複鍵）。不可表示的資料由 `Finish` 回錯誤，並把 `out` 截回原長度。結構誤用是斷言。容量足夠且深度不超過 32 時不配置。同樣的呼叫序列在三平台輸出相同的位元組。`WriteValue` 的數字照原文。 | 往返測試、跨平台 golden、配置計數測試（測試執行檔換掉 `operator new`）、`GYO_CHECK_ASSERTS` |
| **J-6 不知道 ECS 與產品** | 只依賴 Base；不依賴 Kernel、編排層、時間；不進 World（`library_only=yes`）；名字與註解不含遊戲或產品的概念（N5、N6）。 | `gyo_expect_links`（同 J-1）、`cmake.UnitRegistry`；審查 |
| **J-7 執行緒** | 沒有全域可變狀態；多個執行緒可以同時 `Parse` 不同文件、同時讀同一份 `Document`；`Writer` 一個執行緒一個。 | 多執行緒同時 `Parse` 與同時讀的測試（ThreadSanitizer 不在 L1，範囲の限界） |
| **J-8 生命週期** | `Document` 擁有解析出的資料，解析後不再引用輸入文字；`Value`、view 與取出的字串在 `Document` 存活期間有效，移動 `Document` 不失效。 | 測試：輸入解析後覆寫再讀；移動後再讀 |
| **J-9 線性與無上限** | 解析、驗證、走訪、`WriteValue` 的時間與記憶體對值的個數與深度線性；不遞迴；不設大小與深度的上限（上限由各資料契約定）；陣列與物件只給依序走訪（每步常數），`Find` 線性；配置失敗回 `OutOfMemory`。 | 深度 100 萬層的陣列、10 萬個物件的陣列、1 萬個鍵的物件的測試（時間上限寬鬆，抓二次方）；`OutOfMemory` 以測試用的配置失敗注入 |

### Replacement Criteria（新 Component，寫在積上去之前）

- **RC-1 換函式庫**：要維持 J-2，得在 adapter 裡自己再寫一個語彙分析器（例：yyjson 改版後原文旗標放寬文法）；或 yyjson 12 個月沒有 commit、授權改變。→ 換組件，介面（J-4）不變。
- **RC-2 數字轉換獨立**：Engine 裡第二個 Module 需要十進位轉 double 時，把 fast_float 移成自己的 adapter（把 `src/adapters/fast_float/` 搬出 JSON Module），JSON Module 改用它；不讓別的模組經 JSON 拿數字轉換，也不另連一份 fast_float。
- **RC-3 型別對應**：三份以上的資料契約重複同樣的「逐欄讀、型別錯誤補 context」程式，提出型別對應（反射或 schema 綁定）的設計交使用者；不在 JSON Module 裡長出設定語言（AGENTS §9）。
- **RC-4 view 的生命週期**：觀察到 `Value` 或取出的字串活得比 `Document` 久的缺陷，評估改成擁有型的樹。
- **RC-5 預算**：第 9 節判定 6 的停止條件觸發——停下重看 adapter 的走訪或 `Writer` 的形狀。
- **RC-6 數字寫出的單一實作**：跨平台 golden 在任一平台分歧，或 yyjson 的數字寫出改變格式（升版時 golden 變），停下、交使用者（換寫出的實作或改 golden 都是 J-5 的變更）。

## 8. 測試、守衛、突變

### 8.1 測試先行

每一步先寫會失敗的測試（Red，記下失敗的輸出），再實作到通過（Green）。新測試在 `tests/common/json/`，執行檔 `gyo_json_tests`，標籤 `cpu`。

### 8.2 數字向量：調查的全部向量在 L1 三平台

- **照 `v8-arch-json-fast-float` 的字面**：「以調查卡同一組數字向量在 L1 三平台驗 0 不一致」。向量 54 MB 在主 checkout，L1 拿不到，所以**在 L1 上重生**：把調查的生成腳本 `probe/gen.py` 的數字部分（固定表、生成集 200,000、整數 10,010 與參考值；只用 Python 標準庫、種子 20261113）抽成 `tests/common/json/vectors/gen_numbers.py`，ctest 先跑它產生六個檔，**比對 SHA-256 等於調查的檔**（證明是同一組），再由 C++ 測試逐一比對。雜湊（主 checkout `build/evidence/json-library-survey/vectors/`）：
  - `num_fixed.txt` `e195ee51906d61bbf25702b2a9bebf9540215e98153fbc829fad9e33aa1c0b7c`、`num_fixed_ref.txt` `c99b1b87292f6f91c9c9809ebe6ca28ae90d66ba269b5e3fb2c1d4b7015afcc7`
  - `num_gen.txt` `8b0b9525d4267579e6d938d3328ba823614cdd4b5c3ac6a104bd15617dd11af1`、`num_gen_ref.txt` `5a0baf2d81a392f7da9f8d07488b6325f8f7de5f803c9d2dc9f41153888226a0`
  - `num_int.txt` `e8b7a2d6637248763cbc6dd5f3208faf540a36839d6f3322120ff08b56fb9631`、`num_int_ref.txt` `7c86ca1448c111d96f21b8c15f500d084556efabfe9a5e2fff5d196b04a0504f`
- Python 在 L1 的 ctest 已經是必要的（`tests/common/kernel/CMakeLists.txt:100`、`tests/common/entity_store/CMakeLists.txt:22`）。生成檔寫在建置目錄（不進 git，所以沒有 CRLF 的問題）；雜湊不符（例：L1 的 Python 版本讓 `random` 產生不同序列）→ 測試失敗、停下回報，不改雜湊。
- **判定的規則**：參考值是 bit 的，`AsDouble` 逐位元相同；參考值 `E:range` 或 `E:underflow` 的，`Parse` 回 `NumberOutOfRange`；整數參考值在 int64／uint64 範圍內的，`AsInt64`／`AsUint64` 相同、範圍外的回 `NumberOutOfRange`。每檔的筆數寫成常數，測試斷言讀到的筆數（防截斷）。
- **固定表補強**（直接寫在測試裡）：溢位邊界 `1.7976931348623158079e308`（收，捨入到最大 double）與 `1.797693134862315808e308`（拒）、下溢的正中點、`0e999999`、`-0e-999`（零的文字，讀成 ±0）、指數超過 64 位元的字面。
- **本機全集（證據）**：JSONTestSuite 318 檔＋GYO 補充 24 檔在本機跑一次（判定 3）；不進 L1（語料是調查下載的，不放進 repo）。

### 8.3 寫出端的數字

- **最短的獨立參考**：生成集的第 1 類是 Python `repr`（最短往返）；`Writer` 寫同一個 double 得到的有效數字位數，要等於 `repr` 的位數（證明是最短，不只是能往返）。
- **結構化的值**：10 的 −324～308 次方、1～17 位有效數字乘各種指數、整數值的 double（`100000.0` 這類）、`-0.0`、次正規數的邊界；寫出→讀回位元相同，輸出取 SHA-256 與 golden 比（三平台相同）。golden 在第 5 步以本機算出；L1 三平台綠才算成立。
- **往返性質**：splitmix64（固定種子）產生 200,000 個有限 double 的位元型樣，寫出→讀回位元相同。
- **第 1 步先探**：第 1 步的 L1 dispatch 放一個 `yyjson_write_number` 的小探測與小 golden（上面結構化的值的前 100 個），在寫 `Writer` 之前先確認三平台輸出相同（L1 的 macOS 是 Xcode 16.4，`.github/actions/native-setup/action.yml:78`）。

### 8.4 情境測試（第 6 步）

- 「結構化紀錄往返」（F1）：用 `Writer` 寫 1,000 筆像 trace 的紀錄（tick、id、有號與無號整數、小數、帶跳脫的字串、巢狀物件與陣列），一行一筆、共用一個字串；逐行 `Parse`，逐欄比對；再 `WriteValue` 回去，位元組相同。
- 「內容文件讀取」（F2）：一份像資源目錄的文件，走訪讀出各欄；同一份文件改成重複鍵、型別不符、超出範圍各一次，碼與細節（行與欄）符合 5.5 節。
- 「region 平行讀取」（F6）：8 個執行緒同時各 `Parse` 一份不同的合成 region（數字為主、寬陣列），結果與單執行緒相同。

### 8.5 守衛（AGENTS §3「ガードを立てる時期」）

- 和測試同時立（現有的守衛形式，不綁新實作方式）：`gyo_expect_links(gyo_json …)` 與 interface 不露出；測試執行檔只拿到公開 include 路徑（J-4）。
- 情境測試通過之後立（第 6 步）：J-1 的含入掃描（Python，照 `kernel_guard.py`）。範圍：`engine/`（`yyjson.h` 唯一准許的位置是 `engine/json/src/adapters/yyjson/`、`fast_float` 是 `engine/json/src/adapters/fast_float/`；兩條規則分開判）與 `tests/common/json/`（adapter 自己的測試准許，同 `adapter-plugins-plan` 第 255 行）。產品側不掃（2.2 節）。自我測試三件：規則全關的突變要被抓到、實際樹上的正例（`engine/json/src` 的含入要被數到 1 個以上）、掃描根不存在時失敗而不是零命中。判定 6 的效能比較程式要直連 yyjson，放在 git 不追蹤的 `build/evidence/gyo2-json-module/bench/`，不在掃描範圍、不進 repo。

### 8.6 突變（AGENTS §14.2；§14.5 與 `tests/common/mutation/README.md`）

- **條件**：AGENTS §14.5 與 `tests/common/mutation/README.md` 在 PR #251（還沒合併）。合併了就照它；沒合併時照 §14.2 與下列：一個改變一個突變；Design Invariants 的條文與判定項每個獨立的決定（一個條件、順序、預設值、錯誤的種類、「不做的事」）至少一個，加的時候寫它抓的、清單裡別的突變抓不到的錯誤；守衛每條規則三件（全關、正例、fail closed）＋每個獨立分支一個。
- 清單 `tests/common/json/mutations.json`，以 `tests/common/mutation/run_mutations.py` 跑；每個突變寫預期失敗的測試名或訊息片段，只有那個失敗才算 killed。
- **時點**：每一步寫它的測試時一起寫那一步的突變，那一步跑相關的（新加的、`find` 在改動範圍的、殺它的測試被改的；D184）。第 7 步把整份清單交 Fable／high 敵對式檢查缺口，補進去，完整跑一次。
- **條數的估計**：J-1 守衛 5、J-2 約 12（每種拒絕）、J-3 約 10（整數字面、`-0` 三種、負的無號、下溢與溢位的定義、零的文字）、J-5 約 14（跳脫、截回、結構斷言 6 種、不配置、原文寫出）、J-7～J-9 約 6——約 47，超過 README 的「9 條 × 5＝45」，在合併前核准說明理由。

### 8.7 既有測試與守衛

不改任何既有的判定（AGENTS §14.4 不適用）。`cmake.UnitRegistry` 讀實際的登記檔，`engine/json` 有 `CMakeLists.txt` 就要有登記列（`UnitRegistryTests.cmake:87`）——本批加列，判定不變。

## 9. 判定（先宣告，結果出來之後不放寬）

1. **通過**：全量 `ctest` 本機綠；L1 三平台（linux-x64、windows-x64、macos-arm64）綠。
2. **數字**：調查的全部向量（雜湊相符後）、固定表、寫出端的最短參考、結構化 golden、往返性質在 L1 三平台 **0 不一致**（規則 8.2）。任一平台不一致→停、回報，不為某平台開分支。
3. **嚴格解析（本機，JSONTestSuite 1ef36fa0）**：`y_` 95 件中 93 件收，`y_object_duplicated_key`、`y_object_duplicated_key_and_value` 兩件照 J-2 回 `RepeatedKey`（**2026-11-20 更正**：原寫「`y_` 95 件全收」，和 J-2 矛盾——這兩件的內容就是重複鍵；結果出來後判定不成立，停下回報，使用者在主會話回「這個照建議」，HANDOFF `v8-arch-gyo2-json-criterion3-corrected`。方向是收緊（以前要收、現在要拒），實作與測試不改；照 §14.4 寫在合併前核准頁）；`n_` 全拒（含「`123`＋NUL」）；`i_` 35 件：字串、代理、編碼、BOM 類 24 件拒（調查 `yyjson-strict-vs-raw.txt` 的原文旗標欄）、數字類 10 件中溢位 5 件與下溢 2 件因 J-2 拒、超出 64 位元的整數 3 件收、`i_structure_500_nested_arrays` 收——共拒 31、收 4。GYO 補充 24 件與調查記錄的原文旗標結果一致，下溢與溢位類照 J-2 改判拒。
4. **檢出力**：突變清單（Fable 檢查補過後）全部 killed（預期的那個失敗）；沒殺的停下回報。
5. **J-1 掃描**：正例命中 1 個以上、其他位置 0；自我測試三件通過；`gyo_expect_links` 綠。
6. **預算（停止條件，不是受理的閘門）**：本機（macOS x86_64）量並記錄：合成 5.3 MB 與 16 MiB 的 region（數字為主、含寬陣列與寬物件）的 `Parse`（含驗證走訪）時間、yyjson 單獨解析同一份的時間、`Writer` 寫一筆約 420 位元組紀錄的時間。**停止條件**：5.3 MB 超過 150 ms（BigMap 整個載入的預算 450 ms 的三分之一，engine-grid :170）、或比 yyjson 單獨解析慢 10 倍以上、或單筆寫出超過 5 µs（中位數）→ RC-5，停下回報。單筆寫出不配置是 J-5 的測試，不在這裡。
7. **件數的突き合わせ**：數字向量每檔的筆數＝常數＝比對數；JSONTestSuite 318＋GYO 24＝判定 3 的件數總和；往返 200,000；情境 1,000 行；突變 killed／總數。不一致時照 AGENTS §14 冒頭。

## 10. 實作的步驟（動工前核准之後）

段：**上段**（資料契約的解析、公開介面、外部組件 adapter）。模型／檔位：實作者 Opus／high（本卡）；審查者 Fable／high；突變清單的敵對式檢查 Fable／high；§14.4 檢證者不需要（沒有判定變更）。檔位全程 high，**和 master 計畫的指定（數值轉換段 xhigh、審查 Fable／xhigh）不同，要使用者同意（第 12 節第 4 題）**。碰到原因不明的失敗照 §14 停下回報，不自己升檔。

| 步 | 內容 | 完成的樣子 |
|---|---|---|
| 第 1 步 | 下載與查證（第 13 節，使用者同意後）：fast_float 的版本、授權、C++20、三平台編譯與溢位／下溢的回報方式；yyjson 的 `NUMBER_AS_RAW`、BOM、尾隨內容、非 insitu 的輸入複製；`yyjson_write_number` 的小探測與小 golden；推 draft PR、手動 dispatch L1 看三平台 | 本機與 L1 三平台編得過、探測三平台相同；查證結果記 dev_log。不過＝停、回報 |
| 第 2 步 | 骨架：登記檔一列、`engine/json` 的 CMake、`gyo_expect_links`、`JsonError`；先讓 `cmake.UnitRegistry` 抓到缺列（Red）再加列 | 全量 ctest 綠 |
| 第 3 步 | 讀：文法、BOM、尾隨、空、代理、重複鍵（含跳脫後相同）、型別不符、走訪、行與欄、JSON Pointer、生命週期與移動、多執行緒同時 `Parse` 與讀、深層與寬容器、`OutOfMemory` 注入 | `gyo_json_tests` 讀的部分綠；相關突變 |
| 第 4 步 | 數字：解析時驗證、`AsDouble`／`AsInt64`／`AsUint64`／`AsNumberText`／`IsIntegerLiteral`、`-0`、固定表、向量重生腳本與雜湊比對 | 判定 2 的本機部分 |
| 第 5 步 | 寫：`Writer`、跳脫、UTF-8 檢查、數字（`yyjson_write_number`）、非有限數（照第 3 題）、截回、結構斷言、配置計數、`WriteValue`、最短參考、結構化 golden、往返性質 | 判定 2 的寫出部分（L1 在第 7 步一起看） |
| 第 6 步 | 情境測試（8.4）通過 → J-1 含入掃描與自我測試（8.5） | 判定 5 |
| 第 7 步 | 突變清單交 Fable 檢查 → 補 → 完整跑；全量 ctest；判定 3、6 的本機量測；文件（`json.zh-Hant.md`、v2 README、dev_log）；合併前核准頁；開 PR、L1 三平台 | 判定 1～7；交使用者合併前核准 |

**和第 3 批共用的檔**：`engine/config/modules.csv`、`tests/common/CMakeLists.txt`、`docs/architecture/target/v2/README.md`。各只加一兩行；誰後合併誰 rebase 解衝突（只是並列的行，不改對方的內容）。

## 11. 卡片照 L17 判的（不是使用者的話）

| 項目 | 判法 | 根據 |
|---|---|---|
| 位置與名字（`engine/json`、`GYO::Engine::Json`、`gyo_json`／`GYO::Json`、登記列） | 命名規範 N11～N17、`-module-plugin-registers` | 5.1 |
| 外部組件的建法（FetchContent 釘 commit SHA、不跑上游 CMake、`SYSTEM`、`GIT_SUBMODULES ""`） | master 的 ufbx 先例＋CI 自己釘雜湊的作法 | 5.1 |
| 型別不符回 `Result` | yyjson 型別不符回 0／NULL；error-handling 規則 2 | 5.2 |
| 只給依序走訪、沒有隨機索引 | yyjson 的串列佈局；BigMap 的二次方坑 | 5.2 |
| 整數讀取只收整數字面；`AsUint64` 讀負數回 `NumberOutOfRange` | yyjson `yyjson_is_int`；數學上的範圍 | 5.3 |
| `-0` 讀成 `-0.0`／`0`（無號是 adapter 的特例） | 整數沒有負零 | 5.3 |
| 解析時驗證每個數字；零的文字不算下溢 | 「解析成功＝整份文件合法」；J-2 | 5.3 |
| `Writer` 不偵測重複鍵（呼叫者的責任） | F1 的零配置 | 5.4 |
| `Finish` 失敗截回原長度 | 多行共用字串 | 5.4 |
| UTF-8 的接受集合和 yyjson 讀取端相同 | J-5 往返 | 5.4 |
| `WriteValue` 的數字照原文 | 往返不變形 | 5.4 |
| 錯誤碼的分法、行與欄（位元組）、`OutOfMemory` 統一 | yyjson 的位元組位置；BigMap B2 | 5.5 |
| 不設大小與深度的上限 | yyjson 預設不限；上限是各資料契約的事 | 5.6 |
| 可編輯文件、縮排排版延後 | 直近沒有使用者（AGENTS §2） | 2.2 |
| 守衛只守 Engine 這一側 | 使用者已定（`adapter-plugins-plan` 第 15 行） | 2.2、8.5 |
| 數字向量在 L1 重生並比對雜湊 | `v8-arch-json-fast-float` 的字面；Python 已是 L1 的依賴 | 8.2 |
| 含入掃描用 Python | `kernel_guard.py` 的先例 | 8.5 |

## 12. 交使用者的

### 12.0 和 master 計畫（`migration-plan` §3.17c、`v8-arch-json-fast-float`）不同的地方

| # | master 計畫 | 本宣告 | 處理 |
|---|---|---|---|
| 1 | 寫出走 yyjson（可變 DOM，鍵的取代用 `yyjson_mut_obj_replace`） | 串流寫出：結構自寫、數字用 `yyjson_write_number`；之後的可編輯文件也經這個 `Writer` 寫出 | **第 2 題** |
| 2 | Contract 有「寫出：保序，`gyo.ui` 的往返」 | 延後到第一個要它的批（`gyo2` 沒有 UI 的使用者） | 卡片照 L17 判（2.2 節；直近沒有使用者） |
| 3 | 同一組數字向量在 L1 三平台 | 同（在 L1 重生並比對雜湊，8.2 節） | 沒有差異 |
| 4 | 模型／檔位：實作 Opus／high（數值轉換段 xhigh）；審查 Fable／xhigh | 全程 high；審查 Fable／high | **第 4 題** |
| 5 | 守衛只掃 `engine/`（N1） | 同（修訂 0 擴到產品側，修訂 1 收回） | 沒有差異 |
| 6 | 七份資料契約的驗證規則收緊、版本升不升逐份交使用者 | `gyo2` 現在沒有這些契約；收緊的內容（下溢、BOM、「`123`＋NUL」、超出 64 位元的整數）在各契約回到 `gyo2` 的批（asset、ui 等）的宣告寫明 | 記在第 15 節，交主會話排 |

### 第 1 題：讀取端給「看著 yyjson 文件的 view」，還是「複製成 GYO 自己的樹」

**例子**：BigMap 讀一個 16 MiB 的 region（數字為主）；UI 讀一份 UI 文件，取出按鈕的標籤字串存進 UI 的資料。

- **view（建議）**：`Document` 擁有 yyjson 解析出的唯讀 DOM，`Value` 只是指向它的小物件；字串是 `std::string_view`。好處：不複製、不多配置。代價：讀的人要讓 `Document` 活得比 view 久（存成自己的資料時要複製字串）；存取的複雜度跟著 yyjson 的串列佈局（只能依序走訪、`Find` 線性，5.2 節）；數字每讀一次轉一次（解析時已驗證過一次，所以每個被讀的數字共轉兩次）；換函式庫時新函式庫要有可走訪的樹（simdjson、Boost.JSON、RapidJSON 都有）。
- **複製成自己的樹**：解析後把 yyjson 的 DOM 複製成 GYO 的節點、丟掉 yyjson 的；數字可以在複製時轉好存起來（每個數字只轉一次）、物件可以建索引（`Find` 常數）。好處：生命週期簡單、存取形狀自己定。代價：每份文件多一次全量走訪、複製與大量小配置；16 MiB 的 region 記憶體峰值約兩份。
- **量級**：調查量的 3.8 ms（1.4 MB）是 yyjson 自己轉數字、沒有重複鍵走訪的設定（調查 §2.6 註明），不是本 Module 的時間；兩個選項都要再加驗證走訪。本批的判定 6 會量 view 的實際時間。
- **別家**：自己就是解析器的（Unreal、Godot、serde_json；知識）都用自己的樹——那棵樹就是解析器的輸出，不是複製；包第三方又用 view 的例子，O3DE 直接露出 RapidJSON 的型別（知識；違反 GYO 的 J-4）；flecs 與 Bevy 不建樹，直接讀進反射描述的型別（GYO 沒有反射）。「包第三方、用自己的詞」沒有一家可以照抄。
- **改變的決定**：介面的形狀（asset、BigMap、UI、存檔都積在上面）；RC-4 是 view 選錯時的回頭條件。
- **證據**：調查 §2.6、§2.2；`yyjson.h:2168-2172`、`:2282`；`reference-survey.md`。

### 第 2 題：寫出的數字格式由誰做（改 master 計畫的寫出路徑）

**例子**：trace 每 tick 每個玩家寫一行，其中有 `"speed":100000.0`、`"dt":0.0001`；之後工具寫 UI 文件的 `"width":100000.0`。

三個做法都是串流的結構與跳脫，差在數字與配置：

- **結構自寫、數字用 `yyjson_write_number`（建議）**：yyjson 0.13.0 有這個不需要 DOM、不配置、執行緒安全的函式（`yyjson.h:1885-1905`）。輸出（本機實測）：`100000.0`、`0.0001`、`1e21`、`-0.0`、`1.5e-7`。數字格式是 yyjson 一份 C 程式碼，三平台相同——和讀取端用 fast_float 而不用平台 `from_chars` 是同一個理由。代價：yyjson 的寫出部分要編進來；`NaN` 會被寫成非標準的 `NaN`，adapter 要先擋（第 3 題）。
- **全部自寫、數字用 `std::to_chars`**：標準庫，輸出由標準唯一規定。但實際格式是 `100000.0`→`1e+05`、`0.0001`→`1e-04`、`200000.0`→`2e+05`（檢查者與本卡在本機實測），內容檔與 trace 會出現 `"width":1e+05`；「三平台相同」依賴 MSVC STL、libstdc++、libc++ 三份實作各自正確（本機只能驗 libc++），和讀取端「不靠平台實作」的理由不對稱。
- **經 yyjson 可變 DOM**：每筆用 `yyjson_alc_pool_init`（`:719`，固定緩衝）建一棵小樹、`yyjson_mut_write_buf`（`:1593`，0.13.0 新增、不配置）寫出。好處：寫出全在函式庫裡，最接近 master 計畫。代價：每筆建樹再丟（熱路徑上多一倍工作）；固定緩衝的大小要依紀錄估，超過就失敗或退回配置；`Writer` 的呼叫式介面要先收集再一次寫出。
- **為什麼要問**：master 計畫假設寫出走 yyjson 的可變 DOM（鍵的取代用 `yyjson_mut_obj_replace`）；建議的做法改成串流、只借 yyjson 的數字寫出，之後的可編輯文件也經這個 `Writer` 寫出（只有一種輸出格式）。使用者定的是「yyjson，數字走原文路」，寫出路徑是計畫寫的、使用者核准過。
- **改變的決定**：J-5；日誌與 trace 那一批直接用它。
- **證據**：`MovementTraceWriter.hpp:30-42`（master）；flecs `src/addons/json/json.h:106-187`；`yyjson.h:719`、`:1593`、`:1885-1905`；`reference-survey.md` 的實測。

### 第 3 題：寫出時遇到 NaN 或無限大

**例子**：第 1207 tick，玩家 3 的速度算成 NaN（除以零），trace 要把這一筆寫出去——這正是最需要留下的那一行。

- **回錯誤（建議，配合日誌批決定表示法）**：`Writer::Double(NaN)` 讓 `Finish` 回 `NotRepresentable`、截回原長度。JSON Module 只產生標準 JSON、不替資料決定意義；日誌與 trace 那一批的紀錄格式自己定 NaN 怎麼寫（例：寫成字串 `"NaN"` 或另加一欄），呼叫 `Writer` 前先判斷。代價：每個寫小數的使用者都要知道這件事，否則那一行會失敗（不是靜默變值）。
- **寫成字串 `"NaN"`／`"Inf"`（flecs 的做法，`strbuf.c:81-92`）**：那一行保得住，讀的人看得懂。代價：讀回時那一欄是字串不是數字，讀取端的 `AsDouble` 回 `WrongKind`；數字欄和字串欄混在一起，是 JSON Module 替所有使用者定了一個約定。
- **寫成 `null`（serde_json，知識）**：那一行保得住、仍是標準 JSON。代價：NaN、+inf、−inf 與「沒有值」分不出來，資料靜默變了。
- **yyjson 預設回錯誤**（`yyjson.h:1264` 的 `ALLOW_` 旗標才寫非標準的 `NaN`）；master 的 trace 用 `%.17g` 印出 `nan`（`MovementTraceWriter.hpp:21-25`），那一行其實不是合法 JSON。
- **改變的決定**：J-5 的「不可表示的資料」；日誌批的紀錄格式。
- **證據**：上列行號；`reference-survey.md`。

### 第 4 題：檔位全程 high（低於 master 計畫的指定）

- master 計畫（`migration-plan` §3.17c）指定：實作 Opus／high、**數值轉換段 xhigh**、**審查 Fable／xhigh**。本卡照使用者 2026-11-19 的通用原則（`-batch3-no-xhigh` 的補充：「xhight 如果不是什麼需要調查的情況可以不需要的。」）提全程 high、審查 Fable／high。
- 低於已核准計畫的指定，`agents/effort-gearbox.md`〈ギアを決める人〉要使用者同意。navigator 的下限（上段不用小模型、檢查者換模型）不受影響。
- 數值轉換段的風險由 L1 三平台的全量向量（8.2）與寫出端的最短參考（8.3）承擔；碰到需要調查的失敗時停下回報、提議用 xhigh。

## 13. 外部組件的下載（要使用者同意才下載）

下載發生在 CMake 設定時（`FetchContent` 淺層 clone）；本機每個新建置目錄第一次 configure 一次，L1 每次跑一次（5.1 節）。

| 組件 | 來源（官方） | 版本與 commit | 大小 | 授權 | 用的部分 |
|---|---|---|---|---|---|
| yyjson | `https://github.com/ibireme/yyjson.git`（作者 ibireme 的官方 repo） | **0.13.0**（2026-09-08，最新；調查用的同一版）；commit `6447536015f3d600f3d65323b10976103b337ca7` | repo 全史約 4.3 MB（淺層 clone 更小）；用到 `src/yyjson.c` 425 KB＋`src/yyjson.h` 331 KB＋`LICENSE` 1 KB | MIT | 只編 `yyjson.c`；關掉 `YYJSON_DISABLE_INCR_READER`、`_UTILS`、`_NON_STANDARD`、`_FILE`（旗標名已在本機 0.13.0 的標頭查證）；**寫出部分保留**（`yyjson_write_number`） |
| fast_float | `https://github.com/fastfloat/fast_float.git`（官方 repo） | **v8.3.3**（2026-10-31，最新）；commit `dbeabeeb3e6791cb96994c2a7a084c36e62ccfb2` | repo 全史約 1.3 MB；標頭 `include/fast_float/` 9 檔共約 185 KB | Apache-2.0、BSL-1.0、MIT 三選一（`LICENSE-APACHE`、`LICENSE-BOOST`、`LICENSE-MIT`）；GYO 選 MIT | header-only，只含標頭 |

查詢方式：`gh api repos/<repo>/releases`、`contents`、`git/ref/tags` 與 `git/tags`（2026-11-19～20，只讀 metadata，沒有下載）。yyjson 0.13.0 的原始碼在主 checkout 有調查時下載的一份（`build/evidence/json-library-survey/lib/`），本頁的 yyjson 行號與實測都用它；fast_float 本機沒有，版本、授權、溢位與下溢的回報在第 1 步查證。兩個都是寬鬆授權；yyjson 是 0.x 版（API 可能變，釘版本＋adapter）。

## 14. 範囲の限界

- 「換函式庫」要到第二個實作出現才算驗證過；本批只以 J-1、J-4 的守衛與審查支撐。
- JSONTestSuite 只在本機（macOS x86_64）跑；L1 三平台跑全部數字向量、固定表與寫出端的測試。
- ThreadSanitizer 不在 L1；J-7 的多執行緒測試只證明「沒當掉、讀到的值對」，不證明沒有資料競爭。
- 效能只在本機量一次，是停止條件，不是受理的閘門。
- `OutOfMemory` 只以測試注入的配置失敗驗證，沒有在真的記憶體不足下驗證。
- 可編輯文件與縮排排版沒有做；F3 的寫回、F5 的工具寫檔在它們的批才驗證。

## 15. 觀察到、不在本批的（報給主會話）

- **Kernel 指紋報告手寫 JSON**（`engine/kernel/src/Fingerprint.cpp:231` `JsonString`、`:513-521`）：同樣的跳脫會在 JSON Module 有一份。Kernel 的連結守衛是 Base＋EntityStore（`tests/common/kernel/CMakeLists.txt:239`），改用 JSON Module 要動 Kernel 的依賴（Architecture Delta）。本批不碰；交主會話決定要不要排。
- **資料契約的驗證規則收緊**（master 計畫的七份契約：`gyo.ui`、`gyo.voxel_mesh`、`gyo.map_markers`、content manifest、catalog、`gyo.anim_events`、shader manifest）：各契約回到 `gyo2` 時，它的批要在宣告寫明經 JSON Module 讀的行為（下溢與溢位是錯誤、BOM 與「`123`＋NUL」拒、超出 64 位元的整數讀整數欄是錯誤）與版本升不升，交使用者（AGENTS §8：Validation Rule 是 Data Contract 的一部分）。
- **授權告示**：產品打包時附 yyjson、fast_float 的 MIT 告示；打包流程回到 `gyo2` 的那一批處理。

## 16. 停止條件

- 第 1 步的查證任一項不過（版本、授權、C++20、三平台編譯、yyjson 的旗標行為與調查不同、`yyjson_write_number` 三平台輸出不同）→ 停、回報。
- 數字向量的雜湊不符，或數字在任一平台有不一致 → 停、回報（不改雜湊、不開平台分支）。
- 判定 6 的停止條件觸發 → 停、回報（RC-5）。
- 原因不明的失敗、同一問題換兩次方法仍無結論、超過時間（第 0 節）→ AGENTS §14 冒頭的報告形式停下。
- 要碰 Kernel、編排層、時間的檔 → 停（和第 3 批並行的前提）。

## 17. 修訂 0 的敵對式檢查：處理對照

| 條 | 處理 |
|---|---|
| M1 漏 BigMap | 採：3.2 加 F6；5.2、5.5、5.6、J-7、J-9、8.4、判定 6 |
| M2 二次方、無走訪 | 採：拿掉 `operator[]`、依序走訪、`Find` 線性、`IsIntegerLiteral`（5.2） |
| M3 掃描範圍超出已定 | 採：收回 `engine/`＋`tests/common/json/`；比較程式放 `build/evidence`（2.2、8.5、12.0 #5） |
| M4 無號 `-0` | 採：adapter 的特例；負數回 `NumberOutOfRange`（5.3） |
| M5 判定 3 件數 | 採：改成明確件數（判定 3；查證 `yyjson-strict-vs-raw.txt`） |
| M6 Writer 重複鍵 | 採：呼叫者的責任、不偵測；J-5 加前提（5.4） |
| M7 第 2 題不完整 | 採：三個做法、實測格式、改建議為數字用 `yyjson_write_number`；和計畫不同處列 12.0 |
| M8 §14.5 不存在 | 採：寫明條件與沒合併時的規則（8.6） |
| M9 非有限數 | 採：第 3 題 |
| S1 L1 抽樣 | 採：L1 重生全集並比對雜湊（8.2） |
| S2 golden 不足 | 採：最短參考、結構化的值、第 1 步探測、RC-6（8.3） |
| S3 `WriteValue` 數字 | 採：照原文（5.4） |
| S4 Writer 契約 | 採：截回原長度、32 層、配置計數（5.4、J-5） |
| S5 J-2 與測試缺口 | 採：代理、UTF-8 集合、固定表補強（5.4、J-2、8.2） |
| S6 守衛先例 | 採：`gyo_expect_links`、interface 不露出、Python 掃描（J-1、8.5） |
| S7 FetchContent | 採：commit SHA、`GIT_SUBMODULES ""`、`SYSTEM`、下載的後果、放進 repo 的比較（5.1、13） |
| S8 第 1 題代價 | 採（第 1 題） |
| S9 預算 | 採：改稱停止條件、絕對值、寬容器（判定 6） |
| S10 突き合わせ與換行 | 採：筆數常數、擴大範圍、生成檔在建置目錄、位元組字面（8.2、判定 7、J-2） |
| S11 突變時點與上限 | 採：每步寫、第 7 步檢查；估約 47（8.6） |
| S12 上限與 J-4 | 採：J-9、5.6；J-4 以 include 路徑守 |
| N1 死亡測試 | 採：`GYO_CHECK_ASSERTS` |
| N2 Kernel 守衛 | 採：Base＋EntityStore |
| N3 手寫不是一組件一 adapter 的違反 | 採：第 6 節第 2 項改寫 |
| N4 判定 2 的說法 | 採：直接寫規則（8.2） |
| N5 草案 | 採：`::Engine::Base::`、`Root()`、`AsDouble` 讀整數字面 |
| N6 與第 3 批共用的檔 | 採：第 10 節末 |
| N7 yyjson 本機就有 | 採：行號與旗標改用本機 0.13.0 查證；`yyjson_locate_pos` 不用；不設 `DISABLE_WRITER` |
| N8 配置失敗兩種處置 | 採：統一 `OutOfMemory` |
| N9 行與欄 | 採：以 `str_pool` 位移換算（第 3 步查證） |

# 事前宣告：JSON Module（`v8-arch-gyo2-json-module`）

更新：2026-11-19（修訂 0）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：設計段草稿；送 Fable／high 敵對式檢查，改完交使用者做動工前核准。核准前不改程式、條文、守衛。**

性質：實作批的事前宣告（AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta、新 Component 的 Design Invariants 與 Replacement Criteria、§6 的外部組件 adapter）。第 6 組（基礎 Module）的第一批。

根據：
- HANDOFF 決定表（文件分支 `origin/claude/gyo2-time-design` 23def9e4）：`v8-arch-gyo2-json-early`（JSON 早於日誌與 trace，「這個算是封裝的實作」）、`-infrastructure-before-products`、`-module-plugin-owner-definition`（JSON 是 Module：介面是我們的，yyjson 是外部組件）、`-confirm-only-differences`、`-batch3-no-xhigh`；`-save-unaware-means-format-only`（存檔的格式是作者的）；`-module-plugin-registers`（Module 登記檔、不進 World 的標記）。
- master 時代、使用者已定、本批不重議的兩條（`docs/architecture/plans/kernel/HANDOFF.md:53-54`）：`v8-arch-json-yyjson-raw-number`（原話「yyjson，數字走原文路。（直接加GYO的介面，內化到Engine中未來有變化，也有介面作為緩衝）」）、`v8-arch-json-fast-float`（小數的轉換用 fast_float；浮點 `std::from_chars` 在 macOS 部署目標 13.3 不可用）。細節：`docs/architecture/plans/kernel/adapter-plugins-plan.zh-Hant.md` 第 16、80、230～233、281 行；計畫 `migration-plan.zh-Hant.md` §3.17c。這兩份寫於 Module／Plugin 重新定義（#240）之前：當時說的「Engine 的 JSON 模組」照 `gyo2` 的同詞異義表讀成「JSON Module」。
- 調查報告：`docs/architecture/plans/third-party-evaluation/json-library-survey.zh-Hant.md`（PR #216；向量與實測程式在主 checkout `build/evidence/json-library-survey/`）。
- 批次表（文件分支）第 40～46 行：第 6 組「JSON Module（最先）→ 日誌機制（兩個入口…trace 一起看）→ … → asset（依賴 2；JSON）」。
- AGENTS §6（外部組件的型別與呼叫只在它的 adapter、一個組件一個 adapter、adapter 對外的介面由使用方用自己的話定）；`docs/architecture/error-handling.md` 規則 1～7；`docs/architecture/target/determinism.md` §4.6、§4.7a（契約資料解析是 (c) 路徑：同一份文件在每個平台得到同樣的值）。
- 命名：`rebuild-naming.zh-Hant.md` N11～N17（新 Module 的 namespace 照 N17 寫 `GYO::Engine::<Module>`）。
- 程式：`origin/gyo2` 7673a419。

**標記**：寫「卡片照 L17 判」的，是任務卡照命名規範、別家原始碼與已定的原則自己判的，不是使用者的話（使用者 2026-11-17、2026-11-19 `-confirm-only-differences`）。交使用者的只有第 12 節。

證據（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-json-module/`，git 不追蹤）：`reference-survey.md`（第 4 節別家對照的出處與行號）、`declaration-r0-adversarial-check.md`（修訂 0 的檢查，檢查後放入）。

---

## 0. 時間估計

| 段 | 估計 | 內容 |
|---|---|---|
| 設計段 | 3～4 h | 讀現況與 master 的決定、對照別家、本頁、Fable 檢查與修訂 |
| 實作段 | 20～32 h（推定） | 第 10 節的 7 步：外部組件加入與查證 2～3、骨架 1、讀 4～6、數字 3～5、寫 3～5、情境測試與守衛 2～3、突變與文件與收尾 5～9 |

超過 1.5 倍（設計段 6 h、實作段 48 h）或單一問題累計超過 2 小時，停下回報（AGENTS §14 冒頭）。master 計畫的估時是 22～44 h（`migration-plan` §3.17c），它含 7 個既有使用者的遷移；`gyo2` 上沒有既有使用者，本批不含遷移。

## 1. 這一批做什麼（先用遊戲講）

- **trace 一行一筆**：下一批的日誌與 trace 要把「第 1207 tick，玩家 3 的移動指令排隊 2 筆」這樣的紀錄寫成一行 JSON。master 的 PvP 是在 producer 執行緒上手寫字串拼起來的（`master@08566260:apps/object_fps_pvp/include/RetroFPS/Pvp/MovementTraceWriter.hpp:30-42`，每行約 420 位元組、`reserve(420)`）；Kernel 的登記指紋報告也手寫了一個 JSON 字串跳脫（`engine/kernel/src/Fingerprint.cpp:231`）。本批給一個寫出器：照呼叫順序往字串後面接，跳脫、數字格式、結構都由它負責。
- **讀內容檔**：之後的 asset 批要讀資源目錄（master 的 `asset_catalog.json`），UI 要讀 UI 文件，作者若選 JSON 當存檔格式也要讀回來。本批給一個嚴格的讀取器：同一個鍵出現兩次、檔頭有 BOM、數字超出 double 的範圍都是錯誤，不是「後面的蓋掉前面的」或「讀成 0」；同一份檔在 Windows、Linux、macOS 讀出逐位元相同的數值。
- **換函式庫只改一處**：yyjson 與 fast_float 只出現在 `engine/json/src/`；之後換函式庫，用它的 Module、產品、工具都不用改（使用者原話「內化到Engine中未來有變化，也有介面作為緩衝」）。

## 2. 範圍

### 2.1 這一批做的（J1～J8）

| 代號 | 內容 |
|---|---|
| J1 | `third_party` 加 yyjson 與 fast_float（第 13 節的下載資訊，使用者同意後才下載）；各自一個 GYO 的 target，只給 `engine/json` PRIVATE 連 |
| J2 | 新 Module `engine/json`：Module 登記檔加一列（不進 World）、CMake target `gyo_json`／`GYO::Json`、namespace `GYO::Engine::Json` |
| J3 | 錯誤型別 `JsonError`（`Base::Error<JsonErrorCode>`）與 `JsonResult<T>` |
| J4 | 讀：`Parse` → `Document`；`Value`、`ObjectView`、`ArrayView`（第 5.2 節） |
| J5 | 數字：原文路（yyjson 交原文，小數以 fast_float、整數以 `std::from_chars` 轉）；解析時驗證每個數字（第 5.3 節） |
| J6 | 寫：串流的 `Writer`（第 5.4 節）；`WriteValue`（把讀到的 `Value` 原樣寫出） |
| J7 | 測試、情境測試、J-1 的含入掃描守衛、突變清單（第 8 節） |
| J8 | 文件：新頁 `target/v2/json.zh-Hant.md`（Design Invariants J-1～J-8 與 Replacement Criteria）、v2 README 索引、dev_log、合併前核准頁 |

### 2.2 不在這一批

| 項目 | 留到 | 為什麼 |
|---|---|---|
| 可編輯的文件（讀進來改值、插入、刪除再寫回，保鍵序；`gyo.ui` 往返這一類） | 第一個要它的批（UI 或工具） | 直近沒有使用者（AGENTS §2「實装は薄く始める」）。形狀（GYO 自己的可編輯樹，或 yyjson 的可變 DOM 藏在介面後）到那一批再定；本批的 `Writer` 是它的寫出端 |
| 縮排排版的輸出（給人看、給 git diff） | 第一個寫內容檔的工具批 | trace 用一行一筆；縮排是 `Writer` 的一個選項，到時加 |
| 型別對應（把結構或 ECS 元件直接讀寫成 JSON，flecs `ecs_ptr_to_json`、Bevy serde＋`bevy_reflect` 那種） | 不排 | GYO 沒有反射；存檔格式是作者的（`-save-unaware-means-format-only`）。Replacement Criteria RC-3 寫什麼時候重看 |
| Kernel 指紋報告改用本 Module | 交主會話（第 15 節） | Kernel 的守衛只准連 Base；本批不碰 Kernel |
| JSON Pointer、JSON Patch、增量解析、註解或尾逗號等寬鬆語法 | 不排 | 沒有使用者；寬鬆語法違反嚴格解析（J-2） |
| 授權告示收進產品的打包 | 打包流程回到 `gyo2` 時 | `gyo2` 現在沒有打包；MIT 要求散佈時附告示，記為那一批的項目 |

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

JSON Module 的介面上會積：日誌與 trace 的紀錄格式、資源目錄與 UI 文件的讀取、工具經資料契約交接的檔案、作者的存檔。介面一旦被這些用上，改讀取的形狀（view 或樹）、錯誤的分類、數字的語義，每個使用者都要改，而且越積越多；數字語義還牽涉保存下來的資料與跨平台的決定性（determinism §4.7a）。所以照基盤處理：予見的要求寫進 Contract 與 Design Invariants，只實作直近要用的部分。

### 3.2 予見的根據 → 基盤的要求（Feature → Contract → Architecture）

根據的種類照 AGENTS §2：1＝名指的 Feature、2＝成熟的參考實作、3＝規模與預算。

| # | Feature（使用者看得到的） | 根據 | Contract（要約定什麼） | Architecture | 本批 |
|---|---|---|---|---|---|
| F1 | 日誌與 trace 的結構化紀錄（一行一筆；之後的分析工具讀它） | 1：批次表第 42 行（下一批）；觀察：master 的 PvP 手寫 JSON 行（`MovementTraceWriter.hpp:30-42`）、Kernel 手寫跳脫（`Fingerprint.cpp:231`）；3：每行約 420 位元組、在 producer 執行緒上組 | 串流寫出：照呼叫順序往呼叫者的字串追加；不建 DOM；容量足夠時不另配置記憶體；輸出逐位元決定；寫出的一定讀得回來 | `Writer`（GYO 自己實作，第 5.4 節） | 實作 |
| F2 | 資源目錄（asset）讀內容 | 1：批次表第 44 行（asset 依賴 JSON）；master 的 `ContentManifest.cpp`、`CatalogParser.cpp`；3：master repo 最大的 JSON 1.4 MB（調查 §2.6），km² 地圖分塊 | 嚴格解析（J-2）、數字原文路與三平台同值（J-3）、型別不符是 `Result` 錯誤（資料錯誤，error-handling 規則 2） | `Parse`／`Document`／`Value` 的 view（第 5.2 節） | 實作 |
| F3 | UI 文件讀取（與之後的 UI 編輯器寫回） | 1：主會話對 `-infrastructure-before-products` 的讀法列了 ui（使用者只更正了 platform 與 net）；master 的 `UiDocumentCodec.cpp`、`UiSerialization.cpp`（`gyo.ui` 往返） | 讀同 F2；寫回保鍵序（可編輯文件，2.2 節延後） | 讀用 F2 的；寫回的形狀延後 | 讀＝實作；寫回＝只定介面的方向 |
| F4 | 作者的存檔（選 JSON 時） | 1：編排層情境 S1-5（`orchestration-skeleton-declaration` 第 546 行）、`-save-unaware-means-format-only`（格式是作者的）；2：Bevy `DynamicWorld`、flecs `ecs_world_to_json`、Unity `SerializeWorld` | 作者自己把 `SaveData` 用 `Writer` 寫、用 `Value` 讀；Engine 不定存檔格式 | 用 F1、F2 的，不另設 | 不另做（用得到 F1、F2） |
| F5 | 工具經資料契約交接（Editor 寫、Runtime 讀，AGENTS §8） | 1：AGENTS §8；記憶 engine-not-tool-duties；AGENTS §6「一個外部 Component 一個 adapter」 | 工具（Tool 產品）也經 `GYO::Json` 讀寫，不自己連 yyjson（一組件一 adapter）；寫的檔必須是 Runtime 讀得進的（J-5 往返） | `GYO::Json` 對產品公開（Tool → Engine 是允許的方向） | 不另做；縮排、可編輯文件延後 |
| — | `engine/config` 的登記檔 | 查證：`engine/config/*.csv` 是 CSV，由 CMake（`build/cmake/GyoUnitRegistry.cmake`）讀，不經 C++ | 無 | 不動 | — |
| — | 網路 Module 的 wire | 查證：pv8 的 wire 是二進位（master 用 protobuf）；沒有點名 JSON | 無 | 不動 | — |

導不出的要求：沒有。

### 3.3 Feature 與 Product

- 用到本批實作部分的 Product：Game（PvP 的 trace、資源目錄、存檔）與 Tool（之後的編輯器讀寫內容檔）。本批本身沒有產品程式；F1、F2 由情境測試代表（第 8.3 節），真正的使用者是下一批（日誌與 trace）與 asset 批。
- 支撐的下一批 Feature：日誌與 trace 的結構化輸出（F1）。

## 4. 別家怎麼做（同一種技術比）

出處的行號在證據 `reference-survey.md`；本機原始碼：flecs 09790b5f7（2026-11-16）、Bevy 6a911653f（2026-11-10）。yyjson 的 API 照調查（0.13.0）與知識，第 1 步下載後對照標頭查證。**Unreal、Godot、O3DE、serde_json、simdjson 的行為來自知識（本機沒有原始碼），沒有查證**；它們只用來說明分歧，不是任何一條判法的唯一根據。

| 技術 | yyjson（要包的函式庫） | flecs `flecs_json` | Bevy | Unreal | GYO 本批 |
|---|---|---|---|---|---|
| 讀成樹 | 唯讀 DOM `yyjson_doc`（一塊連續記憶體，值是 view） | 沒有樹：自己的 tokenizer（`src/addons/json/json.c:42` `flecs_json_parse`）直接讀進反射描述的型別 | Engine 本身不讀 JSON；serde 的格式無關資料模型，`serde_json::Value` 是 serde_json 自己的樹；glTF loader 用 `serde_json`（`crates/bevy_gltf/Cargo.toml:71`） | `FJsonObject`（自己的樹，`TSharedPtr` 節點） | **看著 yyjson 唯讀 DOM 的 view**（`Document` 擁有、`Value` 是 view）——第 12 節第 1 題 |
| 串流寫出 | 沒有；先建可變 DOM 再 `yyjson_mut_write` | 自己的寫出（`flecs_json_object_push`、`flecs_json_member`… 寫進 `ecs_strbuf_t`，`json.h:106-187`） | `serde_json::Serializer` 寫進 `io::Write` | `TJsonWriter` | **GYO 自己的 `Writer`**——第 12 節第 2 題 |
| 數字 | 預設自己轉；`YYJSON_READ_NUMBER_AS_RAW` 交原文 | 自己轉 | serde_json 自己轉 | 自己轉 | 原文路＋fast_float（已定） |
| 重複鍵 | DOM 保留全部成員，自己走訪偵測（調查 §2.2） | — | serde 預設看型別（struct 欄位重複是錯誤） | 後者蓋前者 | 解析後走訪 DOM 偵測，錯誤（已定的嚴格語義） |
| 型別對應 | 無 | `ecs_ptr_to_json`／`ecs_ptr_from_json`（靠 meta 反射；`include/flecs/addons/json.h:69-169`） | serde derive＋`bevy_reflect` 的 `ReflectSerializer`；場景用 RON（`crates/bevy_world_serialization/Cargo.toml:36`） | `FJsonObjectConverter`（UStruct 反射） | 不做（2.2 節） |
| 錯誤 | `yyjson_read_err{code,msg,pos}` | `ecs_parser_error` | `serde_json::Error`（行、欄） | 回 false＋訊息 | `JsonError`：碼、給人看的訊息、細節（行與欄，或 JSON Pointer） |

## 5. 設計（Contract）

### 5.1 位置與名字（卡片照 L17 判，照 N11～N17）

- 目錄 `engine/json/`，標頭 `engine/json/include/engine/json/`，實作 `engine/json/src/`（adapter 的程式都在這裡）。
- target `gyo_json`（STATIC），別名 `GYO::Json`；namespace `GYO::Engine::Json`（N17：新 Module 照 N17；帶過來的 `Engine::<模組>` 不改，見 rebuild-naming §3 末）。
- Module 登記檔（`engine/config/modules.csv`）加一列：`json,GYO::Json,"JSON reading and writing behind the engine's own interface",yes,no,base,,,`——`library_only=yes`（不進 World，`-module-plugin-registers` (2)）、`default=no`、依賴只有 `base`。位置排在 `base` 之後、`math` 之前（只依賴 base；登記檔的列序是建置順序）。
- 外部組件的 target：`third_party/yyjson/`、`third_party/fast_float/`，照 master 的 ufbx 先例（`master@08566260:third_party/ufbx/CMakeLists.txt`：`FetchContent` 釘版本、`SOURCE_SUBDIR` 指向不存在的目錄以不跑上游 CMake、GYO 自己建 target）。target 名 `gyo_yyjson`（STATIC，C）、`gyo_fast_float`（INTERFACE）。只有 `engine/json/CMakeLists.txt` 以 PRIVATE 連它們。
- 模組的依賴照 O-6 只寫在登記檔；外部組件不是 Module，不進登記檔，連結寫在 `engine/json/CMakeLists.txt`（同 master 的 ufbx：外部組件的連結由用它的 target 寫）。

### 5.2 讀

```cpp
namespace GYO::Engine::Json {
enum class Kind : std::uint8_t { Null = 1, Bool, Number, String, Array, Object };

class Document;                                       // move-only; owns the parsed tree
[[nodiscard]] JsonResult<Document> Parse(std::string_view text);

class Value {                                         // a view into a Document
    Kind GetKind() const noexcept;
    JsonResult<bool> AsBool() const;
    JsonResult<std::int64_t> AsInt64() const;
    JsonResult<std::uint64_t> AsUint64() const;
    JsonResult<double> AsDouble() const;
    JsonResult<std::string_view> AsString() const;
    JsonResult<std::string_view> AsNumberText() const; // the number's text as written
    JsonResult<ObjectView> AsObject() const;
    JsonResult<ArrayView> AsArray() const;
};
class ObjectView { std::size_t Size() const; std::optional<Value> Find(std::string_view key) const; /* members in document order */ };
class ArrayView  { std::size_t Size() const; Value operator[](std::size_t) const; /* elements in order */ };
}
```

名字與簽名是本頁的草案，實作時照命名規範微調不必再問；**形狀**（view、型別不符回 `Result`、物件依文件順序）是 Contract。

- **view 與生命週期**：`Document` 擁有解析出的樹；`Value`、`ObjectView`、`ArrayView`、取出的 `std::string_view` 在 `Document` 存活期間有效，移動 `Document` 不使它們失效。`Document` 解析後不再引用輸入文字（輸入可以馬上丟）——這點在第 1 步以測試查證 yyjson 非 insitu 模式的行為。
- **型別不符是 Runtime Error**：內容檔說「這裡是陣列」而讀的人要物件，是資料錯誤（error-handling 規則 2：資料驗證的入口回 `Result`），回 `WrongKind`，細節寫「期待 X、得到 Y」。`ArrayView::operator[]` 超出範圍是 Programmer Error（斷言；索引來自呼叫者的迴圈），先 `Size()`。（卡片照 L17 判：serde_json `as_*` 回 `Option`、yyjson `yyjson_get_*` 型別不符回 0／NULL、Unreal `TryGet*` 回 false——都把型別不符當資料問題，不當程式錯誤。）
- **物件**：成員依文件順序；`Find` 回第一個（解析已保證沒有重複鍵）；查不到回 `std::nullopt`（缺欄位要不要算錯誤由各資料契約定）。

### 5.3 數字（原文路；已定，這裡只寫細節）

- yyjson 以 `YYJSON_READ_NUMBER_AS_RAW` 交出原文；JSON 的數字文法由 yyjson 驗（調查 §2.1：原文旗標下比 `NOFLAG` 只多接受 5 件超大指數，由下一條擋）。
- **解析時驗證每個數字**：每個數字的原文都要能轉成**有限**的 double，而且**非零的文字不得捨入成 0**；否則 `Parse` 回 `NumberOutOfRange`，細節是那個數字的 JSON Pointer。所以「解析成功」就表示整份文件的數字都能讀成 double；不讀的欄位裡的 `1e400` 一樣讓解析失敗。（master 的契約：下溢由「讀成 +0.0」改為錯誤、溢位為錯誤；`adapter-plugins-plan` 第 233 行。`gyo2` 沒有既有使用者，所以沒有締める面。）
- **小數**：`AsDouble` 以 fast_float 轉原文，三平台同一份實作（`std::from_chars` 浮點在 macOS 13.3 不可用：本機 Apple clang 21 對部署目標 13.3 重測仍是「introduced in macOS 26.0」）。正確捨入（round-to-nearest-even）。次正規數照常讀。
- **整數**：`AsInt64`／`AsUint64` 只收整數字面（沒有小數點、沒有指數）；超出範圍回 `NumberOutOfRange`；`1.0`、`1e2` 回 `WrongKind`。以 `std::from_chars`（整數版在 13.3 可用）轉。（卡片照 L17 判：serde_json `as_i64()` 對 `1.0` 回 `None`、yyjson `yyjson_is_int` 對 `1.0` 為假、simdjson `get_int64` 回 `INCORRECT_TYPE`；Unreal `TryGetNumber(int32&)` 從 double 轉——多數嚴格。）
- **`-0`**：`AsDouble` 得 `-0.0`；`AsInt64`／`AsUint64` 得 `0`（整數沒有負零；`std::from_chars` 的整數版也是）。（卡片照 L17 判；master 留給本宣告定，最終檢查 S-11 (3)。）
- **原文**：`AsNumberText` 交出原文（內容身分若要雜湊十進位原文而不是 double，用它）。
- fast_float 對溢位與下溢怎麼回報（回 `result_out_of_range` 或回 inf／0）在第 1 步查證；不論哪一種，adapter 都以上面「有限、非零不得變零」的定義判，不靠函式庫的回報方式。

### 5.4 寫

```cpp
class Writer {                       // appends to the caller's string
    explicit Writer(std::string& out);
    void BeginObject(); void EndObject();
    void BeginArray();  void EndArray();
    void Key(std::string_view key);
    void String(std::string_view text);
    void Int(std::int64_t); void Uint(std::uint64_t); void Double(double);
    void Bool(bool); void Null();
    [[nodiscard]] JsonResult<void> Finish();
};
void WriteValue(Writer& writer, Value value); // writes a parsed value back, member order kept
```

- **GYO 自己實作，不經 yyjson**（第 12 節第 2 題）。理由：yyjson 沒有串流寫出，要先建可變 DOM（每筆紀錄配置一棵樹）；有串流寫出的 flecs、Unreal、serde_json 都是自己寫的。寫出只有跳脫與數字格式兩件技術活，兩件都由標準庫或自寫的小段程式完成，不需要外部組件。
- **輸出**：緊湊（無空白）；鍵與值照呼叫順序；字串照 RFC 8259 跳脫（`"`、`\`、控制字元；`\b \f \n \r \t` 用短形，其他控制字元 `\u00XX`；`/` 與非 ASCII 原樣輸出 UTF-8）。
- **數字格式**：整數照十進位；`Double` 用 `std::to_chars` 的最短往返形（C++17 起標準規定輸出唯一：最短、最接近、定點與科學記號取較短者；本機對部署目標 13.3 實測可用），形式上沒有小數點也沒有指數時補 `.0`（`100.0`、`-0.0`；`1e+23` 照原樣）。所以 double 寫出再讀回得同一個位元型樣，而且讀回時仍是「小數」（`AsInt64` 對它回 `WrongKind`）。（卡片照 L17 判：yyjson、serde_json、Python `json` 都保留 `.0`。）三平台輸出逐位元相同，由第 8 節的跨平台 golden 守。
- **不可表示的資料**：非有限數（NaN、±inf）、`String`／`Key` 不是合法 UTF-8——記下第一個錯誤，`Finish` 回 `NotRepresentable`（細節：哪一種、第幾個值）；**不改寫成 `null` 或替代字元**。發生錯誤後 `out` 追加的內容未定義，呼叫者丟掉。（卡片照 L17 判：yyjson 寫出對 NaN／inf 預設回錯誤、對不合法 Unicode 預設回錯誤；serde_json 把 NaN 寫成 `null` 是少數。錯誤延到 `Finish` 一次回報，是讓 trace 這種逐欄呼叫不必每欄檢查——同 `std::ostream` 的失敗位元。）
- **結構誤用是 Programmer Error（斷言）**：物件裡沒先 `Key` 就寫值、物件外寫 `Key`、`End` 對不上、頂層寫第二個值、`Finish` 時還有沒關的、`Finish` 後再寫（error-handling 規則 3：API 誤用）。
- **配置**：`Writer` 只往 `out` 追加；巢狀狀態放在固定大小的內建空間（深度超過內建空間才配置）；`out` 容量足夠時整筆不配置記憶體。trace 可以重用同一個字串（`clear()` 保留容量）。
- **往返**：`Writer` 的輸出一定能被 `Parse` 讀回，讀回的值與寫入的相同（double 逐位元、字串逐位元組、鍵順序相同）；由 J-5 與往返性質測試守。

### 5.5 錯誤

```cpp
enum class JsonErrorCode : std::uint16_t {
    Syntax = 1,        // not JSON: grammar, invalid UTF-8, BOM, trailing content, empty text
    RepeatedKey,       // a key appears twice in one object (after unescaping)
    NumberOutOfRange,  // overflow, non-zero text rounding to zero, or an integer read out of range
    WrongKind,         // read as a kind the value is not
    NotRepresentable,  // Writer: a non-finite number or invalid UTF-8
    ParserFailure,     // the library failed for a reason other than the text (allocation)
};
using JsonError = Base::Error<JsonErrorCode>;
template <class T> using JsonResult = Base::Result<T, JsonError>;
```

- 細節（detail）：`Syntax` 寫「line L, column C」（1 起算、欄以位元組計，從 yyjson 的位元組位置算）；`RepeatedKey` 與解析時的 `NumberOutOfRange` 寫 JSON Pointer（例 `/items/3` 與鍵名）；`WrongKind` 寫期待與實際的種類。用的模組照規則 5 以 `Base::CauseDetail` 轉成自己的碼。
- 不丟例外（規則 6）：yyjson、fast_float 都不丟例外；`std::string` 配置失敗的 `std::bad_alloc` 照標準傳出，不攔。
- 數值是否進契約：碼的數值不是資料契約（同 `IoErrorCode` 的註解）。

### 5.6 執行緒與記憶體

- 沒有全域可變狀態（不設 yyjson 的全域配置器、不設 locale）；`Document` 解析後唯讀，可以多個執行緒同時讀同一份；`Writer` 一個執行緒一個。
- yyjson 用預設的 `malloc` 配置器。GYO 還沒有記憶體管理的方針；換配置器只改 adapter（Replacement 的位置在 `engine/json/src/`）。
- 深度：yyjson 的解析不遞迴（沒有深度上限）；本 Module 的走訪（重複鍵、數字驗證、`WriteValue`）也不用遞迴，深層文件不會用盡堆疊。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature**：F1（日誌與 trace 的結構化紀錄，下一批）、F2（asset 的資源目錄）；使用者決定 `-json-early`、`v8-arch-json-yyjson-raw-number`。
2. **現在的構造有什麼問題**：`gyo2` 沒有 JSON 能力；master 時代的做法是每個用的地方各自直連 nlohmann 或手寫字串（7 個模組直連、`StrictJson` 以 nlohmann 型別為模板參數、PvP 與 Kernel 手寫 JSON），違反 AGENTS §6 的一組件一 adapter。
3. **哪個邊界改變**：新 Module `engine/json`（新 Subsystem）；`gyo2` 第一個外部組件 adapter（yyjson、fast_float 兩個組件，都只在這個 adapter）；`third_party` 多兩個組件（測試用的 doctest 之外的第一批）。
4. **影響的 Module／Product／Tool**：新增的只有 `engine/json`、`third_party`、`engine/config/modules.csv` 一列、`tests/common/json`。既有 Module 都不改。
5. **依賴方向**：`GYO::Json → GYO::Base`；`GYO::Json → gyo_yyjson、gyo_fast_float`（PRIVATE）。沒有 Module 依賴 JSON（之後日誌、asset 依賴它）。沒有循環。
6. **Ownership**：JSON 的讀寫與數字轉換歸 JSON Module（Engine 的 Module）。yyjson、fast_float 的版本與建置歸 `third_party`，使用者只有 JSON Module。
7. **為什麼不能更小**：不開 Module、讓下一批（日誌）自己寫 JSON 字串＝重複 master 的手寫（同樣的跳脫與數字格式會出現在日誌、PvP、Kernel 三處）；只做寫出不做讀＝把已定的 yyjson 原文路與三平台數字驗證延到 asset 批，基盤的反復留到上面已經積了日誌之後（AGENTS §2「基盤の上に機能を積み上げ始める前に、基盤の反復を済ませる」）。

**Batch declaration checks**（AGENTS §4、`target/README.md` 每批事前宣告的檢查項）：

| 選擇 | 系譜 | 退掉的選項與決定理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 新 Module `engine/json`（不放 Base） | 新導出；master 把 `StrictJson.hpp` 放在 Base（R0 不帶） | 放 Base：Base 是「只依賴標準庫」的最底層（`engine/base/CMakeLists.txt` 註解），放進來會讓每個連 Base 的模組（含 Kernel）間接連到 yyjson——是在為舊形狀（`StrictJson` 在 Base）找位置，不採 | F1、F2；Game、Tool |
| 讀取用 view | yyjson 唯讀 DOM 的形狀；新導出 | 自己的樹（複製）：見第 12 節第 1 題 | F2、F3、F4 |
| 寫出自己實作 | flecs、Unreal、serde_json 的串流寫出 | yyjson 可變 DOM：每筆配置一棵樹、在 producer 執行緒上；第 12 節第 2 題 | F1、F5 |
| 外部組件以 FetchContent 釘版本、GYO 自己建 target | master 的 ufbx、doctest、nlohmann 的先例 | 把原始碼放進 repo：repo 現在沒有放第三方原始碼的先例，第三方都是 FetchContent | — |

既有的 Test、守衛、golden 都不受影響，所以「不必改測試」沒有被當成決定理由。

## 7. Design Invariants（新頁 `target/v2/json.zh-Hant.md`，見出し `Design Invariants`）

本批新增，最終文字在合併前核准定稿。守衛欄：行為的測試照常是守衛；綁實作方式的掃描守衛（J-1 的含入掃描）在情境測試通過之後才立（AGENTS §3「ガードを立てる時期」），同一批的第 6 步。

| 條 | 內容 | 守衛 |
|---|---|---|
| **J-1 唯一的 adapter** | yyjson 與 fast_float 的標頭、型別、呼叫只在 `engine/json/src/`；`engine/json/include/` 不含它們，也不露出它們的型別；其他 Module、Plugin、產品經 `GYO::Json` 讀寫 JSON，不直接連 `gyo_yyjson`、`gyo_fast_float`。（AGENTS §6 的實例；「一個外部組件」的數法：yyjson 一個、fast_float 一個，兩個都只在這個 adapter） | 含入與連結的掃描（第 6 步）；自我測試三件（規則全關、實際樹的正例、找不到目錄時失敗） |
| **J-2 嚴格解析** | 只收 RFC 8259 的 JSON 文字：UTF-8、無 BOM、無註解、無尾隨內容（含數字後接 NUL）、無 NaN／Infinity 字面、非空。同一物件裡解跳脫後相同的鍵出現兩次是錯誤。每個數字都要轉得成有限的 double，非零的文字不得捨入成 0。違反都是 `Parse` 回的 `Result` 錯誤，不是斷言、不是取最後一個。 | 單元測試（每種各一）＋本機的 JSONTestSuite 全集（證據） |
| **J-3 數字走原文路、三平台同值** | 數值只由本 Module 的轉換得到：小數 fast_float、整數 `std::from_chars`；不用函式庫自帶的數字轉換，也不用平台的 `strtod`。同一份文字在 L1 三平台得到逐位元相同的值（determinism §4.7a 的契約資料解析路徑）。整數讀取只收整數字面；`-0` 讀成 `-0.0`（小數）與 `0`（整數）。 | 固定表與抽樣向量（Python 正確捨入的參考值）＋往返性質測試，L1 三平台 |
| **J-4 Engine 的詞** | 公開介面只有 GYO 的型別（`Document`、`Value`、`ObjectView`、`ArrayView`、`Writer`、`JsonError`）；不露出函式庫的 DOM 型別、旗標、錯誤碼。換函式庫時使用者不用改。 | J-1 的掃描（標頭不含）；審查 |
| **J-5 寫出端** | `Writer` 由本 Module 自己實作；輸出是 `Parse` 讀得回的嚴格 JSON，讀回的值與寫入的相同（double 逐位元、字串逐位元組、鍵順序）。不可表示的資料（非有限數、不合法 UTF-8）由 `Finish` 回錯誤，不改寫成別的值。結構誤用是斷言。只往呼叫者的字串追加；容量足夠時不配置記憶體。同樣的呼叫序列在三平台輸出相同的位元組。 | 往返性質測試、跨平台 golden、容量測試、斷言的死亡測試 |
| **J-6 不知道 ECS 與產品** | 只依賴 Base；不依賴 Kernel、編排層、時間；不進 World（Module 登記檔 `library_only=yes`）；名字與註解不含遊戲或產品的概念（N5、N6）。 | `cmake.UnitRegistry`（登記檔）；審查 |
| **J-7 執行緒** | 沒有全域可變狀態；解析後的 `Document` 唯讀、可以多個執行緒同時讀；`Writer` 一個執行緒一個。 | 多執行緒同時讀同一份 `Document` 的測試（ThreadSanitizer 不在 L1，範囲の限界） |
| **J-8 生命週期** | `Document` 擁有解析出的資料，解析後不再引用輸入文字；`Value`、view 與取出的字串在 `Document` 存活期間有效，移動 `Document` 不失效。 | 測試：輸入解析後覆寫或釋放再讀；移動後再讀 |

### Replacement Criteria（新 Component，寫在積上去之前）

- **RC-1 換函式庫**：要維持 J-2，得在 adapter 裡自己再寫一個語彙分析器（例：yyjson 改版後原文旗標放寬了文法）；或 yyjson 12 個月沒有 commit、授權改變。→ 換組件，介面（J-4）不變。
- **RC-2 數字轉換獨立**：第二個 Module 需要十進位轉 double 時，把 fast_float 移成自己的 adapter，JSON Module 改用它；不讓別的模組經 JSON 拿數字轉換，也不另連一份 fast_float（一組件一 adapter）。
- **RC-3 型別對應**：三份以上的資料契約重複同樣的「逐欄讀、型別錯誤補 context」程式（觀察到的重複），提出型別對應（反射或 schema 綁定）的設計交使用者；不在 JSON Module 裡長出設定語言（AGENTS §9）。
- **RC-4 view 的生命週期**：觀察到 `Value` 或取出的字串活得比 `Document` 久的缺陷（跨 Module 的懸空），評估改成擁有型的樹。
- **RC-5 預算**：第 9 節的記錄值顯示解析（含驗證走訪）比 yyjson 單獨解析慢 10 倍以上，或 trace 的單筆寫出需要配置記憶體——停下重看 adapter 的走訪或 `Writer` 的形狀。

## 8. 測試、守衛、突變

### 8.1 測試先行

每一步先寫會失敗的測試（Red，記下失敗的輸出），再實作到通過（Green）。新測試在 `tests/common/json/`，執行檔 `gyo_json_tests`，標籤 `cpu`（L1 跑 `cpu|shader`）。

### 8.2 數字向量（master 計畫的實作前提 (2)：向量 54 MB 在主 checkout、L1 拿不到）

- **固定表**：調查的 40 個（`vectors/num_fixed.txt` 與 `num_fixed_ref.txt`：`-0`、`1e400`、`1e-400`、`2.4703282292062327e-324` 與 `…28e-324`、最大 double、最小次正規數、中點、800 位有效數字…），直接寫進測試。
- **抽樣夾具**：從調查的生成集 200,000 個（四類：最短表示、`%.17e`、1～40 位有效數字、相鄰 double 的中點）每類各抽 1,000 個，連同 Python 正確捨入的參考位元，放進 `tests/common/json/vectors/`（文字檔，上限 512 KB；抽法：固定步長取樣，腳本一起放進去，可以從主 checkout 的向量重生）；整數 10,010 個抽 1,000 個（含兩端與超出 64 位元的）。
- **往返性質**：測試裡以 splitmix64（固定種子）產生 200,000 個有限 double 的位元型樣，經 `Writer` 寫出、`Parse` 讀回，位元相同；另以 17 位有效數字的科學記號（`std::to_chars` 的 `scientific, 16`）各一次。不需要參考值，L1 三平台都跑。
- **跨平台 golden**：上面 200,000 個的 `Writer` 輸出取 SHA-256（`engine/base/Sha256.hpp`），與 golden 比：輸出位元組三平台相同（J-5）。golden 在第 5 步以本機算出；L1 三平台綠才算成立。
- **本機全集（證據）**：調查的全部向量（200,000＋10,010＋40）與 JSONTestSuite 318 檔＋GYO 補充 24 檔在本機跑一次，記不一致數；不進 L1。

### 8.3 情境測試（第 6 步）

「結構化紀錄往返」：用 `Writer` 寫 1,000 筆像 trace 的紀錄（tick、id、有號與無號整數、小數、帶跳脫的字串、巢狀物件與陣列），一行一筆；逐行 `Parse`，逐欄比對；再把每行 `WriteValue` 回去，位元組相同。「內容文件讀取」：一份像資源目錄的文件，讀出各欄；同一份文件改成重複鍵、型別不符、超出範圍各一次，碼與細節符合第 5.5 節。

### 8.4 守衛（第 6 步，情境測試通過之後）

J-1 的掃描：`engine/`、`tests/`、`plugins/`（存在時）、`apps/`、`tools/`（存在時）之下，`yyjson.h`、`fast_float` 的含入只准出現在 `engine/json/src/`；`gyo_yyjson`、`gyo_fast_float` 的連結只准出現在 `engine/json/CMakeLists.txt` 與 `third_party/`。實作成 `cmake -P` 腳本（同 `tests/common/cmake/UnitRegistryTests.cmake` 的作法，不另需 Python）。自我測試三件：規則全關的突變要被抓到、實際樹上的正例（含入在 `engine/json/src` 的那一個要被數到）、目錄不存在時失敗而不是零命中（記憶 grep-p-macos 的教訓）。

### 8.5 突變（AGENTS §14.2、§14.5；`tests/common/mutation/README.md`，若 PR #251 已合併）

- 清單 `tests/common/json/mutations.json`，以共通執行器 `tests/common/mutation/run_mutations.py` 跑；每個突變寫預期失敗的測試名或訊息片段，只有那個失敗才算 killed。
- 照 README 基準 3：Design Invariants 的條文與判定項每個獨立的決定至少一個（J-2 的每種拒絕、J-3 的整數字面與 `-0`、下溢與溢位的判定、J-5 的 `.0`、跳脫、不可表示資料、結構斷言…）；守衛照基準 4（三件＋每個獨立分支一個）。上限：8 條 × 5＝40，超過在合併前核准說明。
- 清單在實作前由實作者（Opus）定案，交 Fable／high 敵對式檢查缺口，補進去再跑（§14.2）。
- 每步只跑相關突變（新加的、`find` 在改動範圍的、殺它的測試被改的；記憶 mutation-rerun-scope），合併前完整跑一次本批清單。

### 8.6 既有測試與守衛

不改任何既有的判定（AGENTS §14.4 不適用）。`cmake.UnitRegistry` 讀實際的登記檔，`engine/json` 有 `CMakeLists.txt` 就要有登記列（`UnitRegistryTests.cmake:87`）——本批加列，判定不變。

## 9. 判定（先宣告，結果出來之後不放寬）

1. **通過**：全量 `ctest` 本機綠；L1 三平台（linux-x64、windows-x64、macos-arm64）綠。
2. **數字**：固定表、抽樣夾具、往返性質、跨平台 golden 在 L1 三平台 **0 不一致**；本機全集 0 不一致（判定範圍照調查 §0 的第二次：參考值為「範圍錯誤／下溢」的項目判為「本 Module 回 `NumberOutOfRange`」，不是只記錄）。任一平台有不一致→停、回報，不為某平台開分支。
3. **嚴格解析**：JSONTestSuite（本機）`y_` 全收、`n_` 全拒；`i_` 的結果記錄（只有超大指數與超出 64 位元的整數會因 J-2 的數字規則被拒，記下件數）；GYO 補充 24 件與調查記錄的 yyjson `NOFLAG` 結果一致。
4. **檢出力**：突變清單（Fable 檢查補過後）全部 killed（預期的那個失敗）；沒殺的停下回報。
5. **J-1 掃描**：正例命中 1 個以上（`engine/json/src` 的含入）、其他位置 0；自我測試三件通過。
6. **預算（只記錄，不是閘門）**：本機（macOS x86_64）合成 1.4 MB 文件的解析（含驗證走訪）時間與 yyjson 單獨解析的比、`Writer` 單筆 420 位元組級紀錄的時間；比超過 10 倍或單筆寫出需要配置＝RC-5，停下回報。
7. **件數的突き合わせ**：數字向量每檔送出數＝比對數；不一致時照 AGENTS §14 冒頭。

## 10. 實作的步驟（動工前核准之後）

段：**上段**（資料契約的解析、公開介面、外部組件 adapter；navigator 的上段條件）。模型／檔位：實作者 Opus／high（本卡）；審查者 Fable／high；突變清單的敵對式檢查 Fable／high；§14.4 檢證者不需要（沒有判定變更）。檔位全程 high（`-batch3-no-xhigh` 的補充：xhigh 只在需要調查時用；碰到原因不明的失敗照 §14 停下回報）。

| 步 | 內容 | 完成的樣子 |
|---|---|---|
| 第 1 步 | 下載與查證：`third_party/yyjson`、`third_party/fast_float`（第 13 節，使用者同意後）；查證 fast_float 的版本、授權、C++20、三平台編譯，yyjson 的 `NUMBER_AS_RAW`／BOM／尾隨內容／非 insitu 的輸入不再引用；推 draft PR、手動 dispatch L1 看三平台編譯 | 本機與 L1 三平台編得過；查證結果記 dev_log。不過＝停、回報 |
| 第 2 步 | 骨架：登記檔一列、`engine/json` 的 CMake、`JsonError`；先寫 `cmake.UnitRegistry` 會抓的缺列（Red）再加列 | 全量 ctest 綠 |
| 第 3 步 | 讀：文法、BOM、尾隨、空、重複鍵（含跳脫後相同）、型別不符、行與欄、JSON Pointer、生命週期與移動、多執行緒同時讀 | `gyo_json_tests` 讀的部分綠；相關突變 |
| 第 4 步 | 數字：解析時驗證、`AsDouble`／`AsInt64`／`AsUint64`／`AsNumberText`、`-0`、固定表、抽樣夾具、本機全集（證據） | 判定 2 的本機部分 |
| 第 5 步 | 寫：`Writer`、跳脫、數字格式（`.0`）、不可表示資料、結構斷言（死亡測試）、容量、`WriteValue`、往返性質、跨平台 golden | 判定 2 的往返與 golden（L1 在第 7 步一起看） |
| 第 6 步 | 情境測試（8.3）通過 → J-1 掃描守衛與自我測試（8.4） | 判定 5 |
| 第 7 步 | 突變清單定案 → Fable 檢查 → 補 → 完整跑；全量 ctest；文件（`json.zh-Hant.md`、v2 README、dev_log）；合併前核准頁；開 PR、L1 三平台 | 判定 1～7；交使用者合併前核准 |

## 11. 卡片照 L17 判的（不是使用者的話）

| 項目 | 判法 | 根據 |
|---|---|---|
| 位置與名字（`engine/json`、`GYO::Engine::Json`、`gyo_json`／`GYO::Json`、登記列） | 命名規範 N11～N17、`-module-plugin-registers` | 5.1 |
| 外部組件的建法（FetchContent 釘版本、不跑上游 CMake、GYO 自己的 target） | master 的 ufbx 先例 | 5.1 |
| 型別不符回 `Result`；陣列越界是斷言 | serde_json、yyjson、Unreal 都當資料問題；error-handling 規則 2、3 | 5.2 |
| 整數讀取只收整數字面 | serde_json、yyjson、simdjson 多數嚴格 | 5.3 |
| `-0` 讀成 `-0.0`／`0` | 整數沒有負零；`std::from_chars` 整數版 | 5.3 |
| 解析時驗證每個數字（不讀的欄位也驗） | 「解析成功＝整份文件合法」；J-2 | 5.3 |
| double 寫出補 `.0` | yyjson、serde_json、Python 保留 | 5.4 |
| 不可表示資料是錯誤、延到 `Finish` 回報 | yyjson 寫出預設回錯誤；串流寫出的失敗位元慣例 | 5.4 |
| 錯誤碼的分法與細節（行與欄、JSON Pointer） | yyjson 的位元組位置、serde_json 的行與欄 | 5.5 |
| 不設全域配置器、不遞迴的走訪 | J-7；深層文件 | 5.6 |
| 可編輯文件、縮排排版延後 | 直近沒有使用者（AGENTS §2） | 2.2 |
| 數字向量的抽樣與往返性質（取代 L1 上拿不到的 54 MB） | master 計畫的實作前提 (2) 留給宣告定 | 8.2 |
| 守衛用 `cmake -P` | `UnitRegistryTests.cmake` 先例 | 8.4 |

## 12. 交使用者的（各家做法不同且有取捨／推翻之前的計畫）

### 第 1 題：讀取端給「看著 yyjson 文件的 view」，還是「複製成 GYO 自己的樹」

**例子**：asset 讀一份 1.4 MB 的地圖資料（master 最大的 JSON `biome.voxel_mesh.json`）；UI 讀一份 UI 文件，取出按鈕的標籤字串。

- **view（建議）**：`Document` 擁有 yyjson 解析出的唯讀 DOM，`Value` 只是指向它的小物件；取出的字串是 `std::string_view`，在 `Document` 活著時有效。好處：不複製，解析多快就多快（調查：yyjson 3.8 ms、nlohmann 41.2 ms，同一份 1.4 MB，`json-library-survey` §2.6，只在 linux-x64 量過）。代價：讀的人要讓 `Document` 活得比取出的 view 久（存成自己的資料時要複製字串）；換函式庫時新函式庫要有可走訪的樹（yyjson、simdjson、Boost.JSON、RapidJSON 都有）。
- **複製成自己的樹**：解析後把 yyjson 的 DOM 複製成 GYO 的節點（像 Unreal 的 `FJsonObject`、Godot 的 `Variant`、`serde_json::Value`），然後丟掉 yyjson 的。好處：生命週期簡單（樹自己擁有字串）、換函式庫只要能產生事件。代價：每份文件多一次全量複製與大量小配置（Unreal 每個節點一個 `TSharedPtr`）。
- **別家**：自己就是解析器的（Unreal、Godot、serde_json）都用自己的樹——但那棵樹就是它們解析器的輸出，不是複製；包第三方而用 view 的，O3DE 直接把 RapidJSON 的 `rapidjson::Value` 露給使用者（沒有包，違反 GYO 的 J-4）；flecs 與 Bevy（serde）根本不建樹，直接讀進反射描述的型別（GYO 沒有反射）。所以「包第三方、用自己的詞」沒有一家可以照抄，兩種都說得通。
- **改變的決定**：介面的形狀（之後 asset、UI、存檔都積在上面）；RC-4 是 view 選錯時的回頭條件。
- **證據**：調查 §2.6、§2.2；`reference-survey.md`。

### 第 2 題：寫出由 GYO 自己寫串流寫出器，不經 yyjson（改 master 計畫的一句）

**例子**：trace 每 tick 每個玩家寫一行「tick、玩家 id、排隊筆數…」，在模擬的執行緒上組好再交給寫檔執行緒。

- **自己寫（建議）**：`Writer` 照呼叫順序往字串追加，跳脫與數字格式自己做（數字用標準庫 `std::to_chars`，三平台輸出由標準唯一規定，本機對 macOS 13.3 實測可用）。好處：不建樹、字串容量夠時零配置；有串流寫出的 flecs、Unreal、serde_json 都是自己寫的。代價：跳脫與數字格式是我們自己的程式（由往返性質測試與跨平台 golden 守）；之後的可編輯文件若用 yyjson 的可變 DOM，要讓它經這個 `Writer` 寫出，才不會有兩種輸出格式。
- **經 yyjson**：每筆先建 `yyjson_mut_doc`、加欄位、`yyjson_mut_write` 成字串。好處：寫出也在函式庫裡，GYO 少一段程式。代價：每筆配置一棵樹再丟掉（trace 在熱路徑上）。
- **為什麼要問**：master 的計畫（`migration-plan` §3.17c 的 Contract：「寫出：保序…寫出時物件鍵的取代用 `yyjson_mut_obj_replace`」）假設寫出也走 yyjson；使用者定的是「yyjson，數字走原文路」，寫出走哪條沒有逐字定，但計畫是使用者核准過的。本題只改寫出端，讀取端照舊走 yyjson。
- **改變的決定**：J-5；日誌與 trace 那一批直接用它。
- **證據**：`MovementTraceWriter.hpp:30-42`（master）；flecs `src/addons/json/json.h:106-187`；調查 §2.4（yyjson 寫出的數字格式）。

## 13. 外部組件的下載（要使用者同意才下載）

下載發生在 CMake 設定時（`FetchContent` 淺層 clone，本機與 L1 每台各一次；之後設定不再更新，`FETCHCONTENT_UPDATES_DISCONNECTED ON`）。

| 組件 | 來源（官方） | 版本 | 大小 | 授權 | 用的部分 |
|---|---|---|---|---|---|
| yyjson | `https://github.com/ibireme/yyjson.git`（作者 ibireme 的官方 repo） | **0.13.0**（2026-09-08 發行，最新；調查用的同一版） | repo 全史約 4.3 MB，淺層 clone 更小；用到 `src/yyjson.c` 425 KB＋`src/yyjson.h` 331 KB＋`LICENSE` 1 KB | MIT | 只編 `yyjson.c`；關掉用不到的部分（寫出、增量讀、JSON Pointer／Patch：`YYJSON_DISABLE_WRITER` 等，第 1 步確認旗標名） |
| fast_float | `https://github.com/fastfloat/fast_float.git`（官方 repo） | **v8.3.3**（2026-10-31 發行，最新） | repo 全史約 1.3 MB；標頭 `include/fast_float/` 9 檔共約 185 KB | Apache-2.0、BSL-1.0、MIT 三選一（`LICENSE-APACHE`、`LICENSE-BOOST`、`LICENSE-MIT`）；GYO 選 MIT | header-only，只含標頭 |

查詢方式：`gh api repos/<repo>/releases` 與 `contents`（2026-11-19，只讀 metadata，沒有下載）。兩個都是寬鬆授權；yyjson 是 0.x 版（API 可能變，釘版本＋adapter，同調查 §2.5 的看法）。

## 14. 範囲の限界

- 「換函式庫」要到第二個實作出現才算驗證過（同 master 計畫）；本批只以 J-1、J-4 的掃描與審查支撐。
- 本機的全集（數字 210,050 個、JSONTestSuite）只在 macOS x86_64 跑；L1 三平台只跑抽樣夾具、固定表與往返性質。
- ThreadSanitizer 不在 L1；J-7 的多執行緒測試只證明「沒當掉、讀到的值對」，不證明沒有資料競爭。
- 效能只記錄，單機一次，只當量級參考。
- 可編輯文件與縮排排版沒有做；F3 的寫回、F5 的工具寫檔在它們的批才驗證。

## 15. 觀察到、不在本批的（報給主會話）

- **Kernel 指紋報告手寫 JSON**（`engine/kernel/src/Fingerprint.cpp:231` `JsonString`、`:513-521` 的組字串）：同樣的跳脫會在 JSON Module 有一份。Kernel 的守衛只准連 Base，改用 JSON Module 要動 Kernel 的依賴（Architecture Delta）。本批不碰；記為觀察，交主會話決定要不要排。
- **授權告示**：產品打包時要附 yyjson、fast_float 的 MIT 告示；打包流程回到 `gyo2` 的那一批處理。

## 16. 停止條件

- 第 1 步的查證任一項不過（版本、授權、C++20、三平台編譯；yyjson 的旗標行為與調查不同）→ 停、回報。
- 數字在任一平台有不一致 → 停、回報（不開平台分支）。
- 原因不明的失敗、同一問題換兩次方法仍無結論、超過時間（第 0 節）→ AGENTS §14 冒頭的報告形式停下。
- 要碰 Kernel、編排層、時間的檔 → 停（本批不碰；和第 3 批並行的前提）。

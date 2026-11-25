# JSON Module（`engine/modules/json`）

Owner：架構。來源：事前宣告 [json-module-declaration.zh-Hant.md](json-module-declaration.zh-Hant.md)（修訂 1，2026-11-20 動工前核准；第 7 節）。最終文字在 `v8-arch-gyo2-json-module` 的合併前核准定稿。

Engine 讀寫 JSON 的唯一入口：GYO 自己的介面（`Document`、`Value`、`ObjectView`、`ArrayView`、`Writer`、`JsonError`）後面，是 yyjson（解析、數字寫出）與 fast_float（十進位轉 double）兩個外部組件。只依賴 Base，不進 World（Module 登記檔 `library_only=yes`）。

目錄：`include/engine/json/` 是介面；`src/` 是不碰外部組件的部分（`Writer` 的結構、跳脫、UTF-8 檢查、錯誤碼、位置）；`src/adapters/<組件>/` 是直接碰外部組件的程式，一個組件一個子目錄（`src/adapters/yyjson/`、`src/adapters/fast_float/`；使用者 2026-11-20）。換函式庫＝換 `adapters/` 底下的目錄；不另開 CMake target、不加執行時的抽象介面。外部組件的宣告在 `third_party/yyjson/`、`third_party/fast_float/`（FetchContent 釘 commit SHA），原始碼在建置目錄的 `_deps/`，不進 repo。

## Design Invariants

守衛欄：行為的測試照常是守衛；綁實作方式的含入掃描在情境測試通過之後才立（AGENTS §3「ガードを立てる時期」）。

<a id="j-1"></a>
**J-1 唯一的 adapter。** yyjson 的標頭、型別、呼叫只在 `engine/modules/json/src/adapters/yyjson/`，fast_float 的只在 `engine/modules/json/src/adapters/fast_float/`；`engine/modules/json/include/` 不含它們，也不露出它們的型別；Engine 的其他 Module 經 `GYO::Json` 讀寫 JSON，不直接連 `gyo_yyjson`、`gyo_fast_float`。（AGENTS §6；「一個外部組件」的數法：yyjson 一個、fast_float 一個。守衛只守 Engine 這一側：`adapter-plugins-plan` 第 15 行；`tests/common/json/` 是 adapter 自己的測試，准許。）
守衛：`gyo_expect_links(gyo_json GYO::Base gyo_yyjson gyo_fast_float)` 與「不露出給使用者」（`tests/common/json/CMakeLists.txt`）；ctest `json.adapter_guard`（`tests/common/json/json_guard.py`：兩條含入規則、連結規則、正例、根目錄不存在時失敗）與 `json.adapter_guard_selftest`。

<a id="j-2"></a>
**J-2 嚴格解析。** 只收 RFC 8259 的 JSON 文字：UTF-8、無 BOM、無註解、無尾隨內容（含數字後接 NUL）、無 NaN／Infinity 字面、非空；不成對或不完整的代理跳脫是錯誤。同一物件裡解跳脫後相同的鍵出現兩次是 `RepeatedKey`。每個數字都要轉得成有限的 double，非零的文字不得捨入成 0（`NumberOutOfRange`；零的文字不論指數都是 0）。違反都是 `Parse` 回的 `Result` 錯誤，不是斷言、不是取最後一個。
守衛：`gyo_json_tests`（`ReadTests.cpp`、`NumberTests.cpp`）；本機 JSONTestSuite 的判定（宣告判定 3）。

<a id="j-3"></a>
**J-3 數字走原文路、三平台同值。** 讀出的數值只由本 Module 的轉換得到：小數 fast_float、整數 `std::from_chars`；不用函式庫自帶的數字轉換，也不用平台的 `strtod`。同一份文字在 L1 三平台得到逐位元相同的值（determinism §4.7a 的契約資料解析路徑）。整數讀取只收整數字面（`1.0`、`1e2` 是 `WrongKind`）；`-0` 讀成 `-0.0`（小數）與 `0`（整數，含無號）；`AsUint64` 讀負數是 `NumberOutOfRange`。
守衛：`json.vectors.generate`＋`gyo_json_vector_tests`（調查的全部數字向量 210,050 個，在 L1 重生並以 SHA-256 確認是同一組）；`NumberTests.cpp`。

<a id="j-4"></a>
**J-4 Engine 的詞。** 公開介面只有 GYO 的型別；不露出函式庫的 DOM 型別、旗標、錯誤碼。換函式庫時 Engine 裡的使用者不用改。
守衛：`gyo_json_tests` 只拿得到 `engine/modules/json/include`，公開標頭含組件的標頭就編不過；J-1 的掃描。

<a id="j-5"></a>
**J-5 寫出端。** `Writer` 的結構、跳脫、UTF-8 檢查、排版由本 Module 實作，double 的文字只有一份實作（yyjson 的 `yyjson_write_number`：最短往返、整數值保留 `.0`）。輸出是 `Parse` 讀得回的嚴格 JSON，讀回的值與寫入的相同（前提：呼叫者不在同一物件寫重複鍵，`Writer` 不偵測）。不可表示的資料（非有限數、不合法 UTF-8；接受的 UTF-8 和 `Parse` 相同）由 `Finish` 回 `NotRepresentable`，並把字串截回建構時的長度；配置失敗是 `OutOfMemory`。結構誤用是斷言。字串容量足夠且深度不超過 32 時不配置記憶體。同樣的呼叫序列與排版在三平台輸出相同的位元組。`WriteValue` 的數字照原文。
排版由作者選（`Writer(out, Layout)`，`v8-arch-gyo2-json-pretty`）：預設 `Layout{}` 是緊湊的一行（不加空白）；`Layout{.indent = n}`（n > 0）讓每個成員與元素各起一行、每層縮排 n 個字元（`IndentCharacter::Space` 或 `Tab`），鍵後接 `": "`，非空容器的收尾括號自成一行，空容器維持 `{}`／`[]`，單一純量不換行。排版只改 token 之間的空白：讀回的值不變，結尾不加換行（由呼叫者決定）。縮排字元用列舉，所以寫不出 JSON 以外的空白。
別家對照：nlohmann::json `dump(indent, indent_char)`、serde_json `to_string_pretty`／`PrettyFormatter::with_indent` 是同一個形狀（空容器 `[]`、`": "`）；master 凍結的 `gyo.ui` 輸出（nlohmann `dump(2)`）：沒有指數的 `document_v1_master_canonical.json` 和 `Layout{.indent = 2}` 的輸出逐位元組相同；`golden_v1_master_canonical.json` 只差在指數的寫法（nlohmann `1e+300`、`2.5e-07`，本 Module 照最短往返寫 `1e300`、`2.5e-7`——數字文字屬 J-5 的單一實作，不跟排版改），由 `ui.contract` 檢查。Godot `JSON.stringify(data, indent)`（`core/io/json.cpp:57-190`）縮排傳字串、鍵預設排序、空容器寫成 `[` 換行換行 `]`——不照：鍵的順序是呼叫順序（J-5 往返），空容器照前兩家。`indent` 用數字＋字元列舉而不是任意字串（Godot 的形），同 nlohmann。不用 yyjson 的 `YYJSON_WRITE_PRETTY`：`Writer` 的結構本來就不經 yyjson（只有數字文字經 adapter），排版也不經；adapter 不變（J-1、J-4）。
守衛：`WriteTests.cpp`（往返 200,000、結構化 golden、斷言；排版：兩格與 tab 的 golden、空容器與純量、`Parse`→`WriteValue` 兩方向只差空白、2,000 筆生成文件的重複輸出與 FNV-1a golden、排版下的截回）、`WriteAllocationTests.cpp`（配置計數——含縮排、配置失敗）、`VectorTests.cpp`（最短位數與 Python `repr` 比對 50,000）。

<a id="j-6"></a>
**J-6 不知道 ECS 與產品。** 只依賴 Base；不依賴 Kernel、編排層、時間；不進 World；名字與註解不含遊戲或產品的概念（N5、N6）。
守衛：`gyo_expect_links`（同 J-1）、`cmake.UnitRegistry`；審查。

<a id="j-7"></a>
**J-7 執行緒。** 沒有全域可變狀態；多個執行緒可以同時 `Parse` 不同的文件、同時讀同一份 `Document`；`Writer` 一個執行緒一個。
守衛：`ReadTests.cpp` 與 `ScenarioTests.cpp`（F6）的多執行緒案（ThreadSanitizer 不在 L1）。

<a id="j-8"></a>
**J-8 生命週期。** `Document` 擁有解析出的資料，解析後不再引用輸入文字；`Value`、view 與取出的字串在 `Document` 存活期間有效，移動 `Document` 不失效。
守衛：`ReadTests.cpp`（輸入覆寫後再讀、移動後再讀）。

<a id="j-9"></a>
**J-9 線性與無上限。** 解析、驗證、走訪、`WriteValue` 的時間與記憶體對值的個數與深度線性；不遞迴；不設大小與深度的上限（上限由各資料契約定）；陣列與物件只給依序走訪（每步常數），`Find` 線性；配置失敗回 `OutOfMemory`。
守衛：`ReadTests.cpp`（深度 100 萬、10 萬個物件的陣列、1 萬個鍵的物件）、`AdapterTests.cpp`（yyjson 配置失敗的注入）。

### Replacement Criteria

- **RC-1 換函式庫**：要維持 J-2，得在 adapter 裡自己再寫一個語彙分析器（例：yyjson 改版後原文旗標放寬文法）；或 yyjson 12 個月沒有 commit、授權改變。→ 換 `src/adapters/yyjson/`，介面（J-4）不變。`gyo_json_component_tests` 固定了組件本身的行為，升版時先在那裡失敗。
- **RC-2 數字轉換獨立**：Engine 裡第二個 Module 需要十進位轉 double 時，把 `src/adapters/fast_float/` 搬出 JSON Module 成為自己的 adapter，JSON Module 改用它；不讓別的模組經 JSON 拿數字轉換，也不另連一份 fast_float。
- **RC-3 型別對應**：三份以上的資料契約重複同樣的「逐欄讀、型別錯誤補 context」程式，提出型別對應（反射或 schema 綁定）的設計交使用者；不在 JSON Module 裡長出設定語言（AGENTS §9）。
- **RC-4 view 的生命週期**：觀察到 `Value` 或取出的字串活得比 `Document` 久的缺陷，評估改成擁有型的樹。
- **RC-5 預算**：本機量測（宣告判定 6）中，5.3 MB 的解析超過 150 ms、比 yyjson 單獨解析慢 10 倍以上、或單筆紀錄的寫出超過 5 µs——停下重看 adapter 的走訪或 `Writer` 的形狀。
- **RC-6 數字寫出的單一實作**：跨平台 golden 在任一平台分歧，或 yyjson 升版改變數字格式（golden 變），停下、交使用者（換寫出的實作或改 golden 都是 J-5 的變更）。

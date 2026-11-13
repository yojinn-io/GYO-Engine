# JSON 函式庫的選擇調查：報告（`v8-arch-json-library-survey`）

件數の突き合わせ：不一致（simdjson 的整數檔：送 10,010、收 1——整份被拒，屬候選行為，見下）；其餘一致。評估格 7 候選 × 6 判準＝42 格（實測 36、文件判定 1、不評 5）。向量：判準 1 每候選送 412 收 412（JSONTestSuite 318＋GYO 補充 24＋repo 內容 70）；判準 2 每候選送 8 收 8；判準 3 的 3b 送 40／200,000／10,010 收同數；3a、3c 每候選每檔收同數，唯一例外是 simdjson 的整數檔（10,010 個，含 2 個超出 64 位元的值）整份被拒、只回 1 行 `REJECT`——這是 simdjson 的行為（超出 64 位元的整數預設是錯誤）。這 10,009 個沒有逐個結果的數字列為「計數した破棄」。另以去掉這 2 個值的 10,008 個重量（這個檔和整數檔都是宣告外的補充：宣告只寫了固定表與生成集），收 10,008。依 AGENTS §14 冒頭，件數不一致時受け入れ判定不確定，全部判定等使用者確認。

更新：2026-11-13。Owner：架構。作者：Opus 5.5／high。事前宣告：[`json-library-survey-declaration.zh-Hant.md`](json-library-survey-declaration.zh-Hant.md)。
**狀態：實測完成，只讀複查（Opus 另一代理）14 條已處置（§7）；結論只列建議，方向（未決 1）與函式庫由使用者選。**

## 0. 讀法與範圍的限制

- **平台**：使用者選 C（只在本機）並要求在 Docker 下測。全部實測在本機的 `gyo-linux-toolchain:l1-20261112` 容器（L1 linux-x64 的工具鏈：Ubuntu 24.04、gcc-14 14.2.0、libstdc++）裡跑，編譯與執行時斷網。**windows-x64（MSVC STL）與 macos-arm64（libc++）沒有實測**：判準 3 的三平台判定與判準 5 的三平台編譯記「確認できない」。本報告的「通過」都只代表 linux-x64。容器跑在 Docker Desktop 的 linuxkit VM（Intel x86_64 主機），不是 GitHub runner。
- **與宣告的小差異**（複查 #14）：生成集的指數範圍實作是 −330～308（宣告寫 −330～310）；GYO 補充向量 24 個（宣告寫約 20 個）；下載後解壓共約 550 MB（宣告估 300 MB 以內），Boost 一項就 493 MB。
- **判定照事前宣告的字面**：看了結果之後不放寬（AGENTS §14.1）。幾處字面判定和「對 GYO 的實際影響」不同的地方，寫在 §6 交使用者決定，不由我改判。
- **事前宣告的核准範圍**：使用者核准了下載清單與平台（C＋Docker）；判準細則（維護的定義、判準 1 的「條件通過」）與審查者沒有明確回覆。本報告照宣告的寫法判定，§6 再列一次。
- **容器的執行紀錄**：映像 id `sha256:4b443a1281195fa961582807bbae8c11e211ce1d73219af7c86fba4c0b937d8d`（開跑前以 `docker image inspect` 核對）與每次 `docker run` 的旗標（`--network none --cpuset-cpus=8-15 --memory=4g`、掛載清單）原本沒有存進 `container.txt`；複查指出後，依本會話的執行記錄**事後補記**在 `results/docker-runs.txt`。
- **證據**：主 checkout `build/evidence/json-library-survey/`（本機 macOS x86_64 的磁碟，git 管理外）。實測程式 `probe/`（`p_<候選>.cpp`、`fromchars.cpp`、`gen.py`、`run.py`、`build_and_run.sh`、`extra.sh`）；向量 `vectors/`（種子 20261113，重跑得同一份）；結果 `results/linux-x64-docker/`（`container.txt` 映像與工具鏈、`build/` 建置記錄、`raw/` 每次呼叫的原始輸出、`summary-run1-c1-c2-c3-c4-c6.json` 第一次全跑、`summary-c3.json` 判準 3 重跑）、`results/extra/`（寫出格式與警告）；`downloads.txt`（URL、SHA-256、大小）；`maintenance-20261113.json`（`gh api` 原始回應）；`usage-grep-20261113.txt`。
- **判準 3 跑了兩次**：第一次的比對腳本把整數用「表示法」比（`u:0` 和 `i:0` 被當成不同），而且 simdjson 會因為檔中 2 個超出範圍的整數拒絕整份檔。改成「依整數值比」、另加只含範圍內整數的檔，重跑判準 3（`summary-c3.json`）。浮點部分兩次結果相同。
  **判定範圍也跟著變了，要揭露**：第一次把整數檔裡 2 個超出 64 位元的值（參考值 `E:range`）也當判定對象，轉成 double 的候選（nlohmann、Boost.JSON、RapidJSON、yyjson）各多 2 件不一致；第二次把參考值為 `E:` 的項目改成只記錄。而固定表裡同樣的兩個值，參考值是 Python `float()` 的 double、有判定。同一個值在兩份檔的參考值不同，是我生成向量時的不一致；本報告的判準 3 結論以固定表（有判定）為準，整數檔的這 2 個值只記錄。第一次 c3 的 `raw/` 檔被第二次覆寫，只剩 `summary-run1-…json`。

## 1. 總表（linux-x64，Docker）

| 候選 | 1 嚴格解析 | 2 重複鍵 | 3 數字：原文路（J2／J3） | 3 數字：候選轉換路（J1／J4） | 4 寫出與順序 | 5 C++20／授權／維護 | 6 效能（ms，biome 1.4 MB／合成 10 MB） |
|---|---|---|---|---|---|---|---|
| nlohmann 3.12.0（基準） | **不通過**（接受 1 個 `n_`：`123` 後接 NUL） | 通過（SAX，ii） | 不一致 3（`-0`、±`1e400`）；三平台確認できない | 不一致 1（`-0`）；三平台確認できない | 通過（`ordered_json`） | linux 通過；MIT；有人維護（19 個月沒有 release） | 41.2／283.6 |
| Boost.JSON 1.92.0 | **條件通過**（較寬 5 件：超大指數接受成 ±inf；較嚴：BOM、深度 > 32） | 通過（handler，ii） | 0 不一致；三平台確認できない | 不一致 1（`-0`）；三平台確認できない | 通過（`stable_erase`） | linux 通過；BSL-1.0；有人維護 | 8.4／80.7 |
| RapidJSON master `297637bc` | **不通過**（同基準，接受 `123`＋NUL） | 通過（SAX，ii） | 不一致 2（±`1e400` 整份被拒） | 不一致 1（`-0`） | 通過（`EraseMember`） | linux 通過；MIT；有人維護但**風險**（12 個月 1 commit、release 停在 2016） | 12.7／66.0 |
| yyjson 0.13.0 | **通過** | 通過（DOM 保留重複成員，iii） | 0 不一致 | 不一致 1（`-0`） | 通過 | linux 通過；MIT；有人維護 | 3.8／18.5 |
| simdjson 5.1.1 | **通過** | 通過（DOM 保留重複成員，iii） | 0 不一致 | 不一致 4（`-0`、3 個超出 64 位元的整數被拒） | **條件通過**（沒有可改的樹，呼叫端用 builder 重寫） | linux 通過；Apache-2.0（附帶告示）；有人維護 | 4.3／16.4 |
| jsoncons 1.10.0 | **不通過**（較寬 10 件；其中 5 件代理跳脫被靜默改寫，adapter 補不上） | 通過（visitor，ii） | 不一致 1（`-0`） | 不一致 4（`-0`、3 個大整數交出文字） | 通過（`ojson`） | linux 通過；BSL-1.0；有人維護 | 26.4／130.4 |
| glaze 9.1.0 | 不評 | 不評 | 不評 | 不評 | 不評 | **不通過**（README：「Requires C++23」） | 不評 |

「三平台確認できない」適用於判準 3 與判準 5 的每一格；上表只寫一次。

## 2. 逐判準

### 2.1 判準 1：嚴格解析

方法：JSONTestSuite（`1ef36fa0`）318 檔、GYO 補充 24 檔、repo 內的 JSON 70 檔（不含 docs），每檔在子程序裡以候選的最嚴設定解析一次，記 ACCEPT／REJECT／崩潰／逾時。基準＝nlohmann 3.12.0 以 `StrictJson` 今天的呼叫方式（`sax_parse` 再 `parse(..., nullptr, false)`），不含重複鍵規則（重複鍵在判準 2）。原始結果 `raw/<候選>-c1.txt`。

各候選用的設定：nlohmann 預設；Boost.JSON `parse_options{}`（只收標準 JSON，`max_depth` 預設 32）；RapidJSON `kParseValidateEncodingFlag | kParseIterativeFlag`；yyjson `YYJSON_READ_NOFLAG`；simdjson DOM parser（On-Demand 是惰性解析，不能當嚴格檢查）；jsoncons `json_options().allow_comments(false)`（**預設是接受註解**，預設的錯誤處理也會略過註解）。

- **全部候選**：`y_` 95 檔全收；崩潰、逾時 0。repo 內 70 檔，每個候選的結果都和基準相同（不會讓今天有效的 GYO 內容變無效）。
- **基準本身**：nlohmann 接受 `n_multidigit_number_then_00.json`（內容是 `123` 後接一個 NUL 位元組）。今天的 `StrictJson` 也一樣：文字裡數字後面接 NUL 會被接受。RapidJSON 相同（以 NUL 當輸入結束）。照宣告「接受任何 `n_` 即不通過」，兩者判不通過；這是一條觀察到的既有行為，本批不修。
- **Boost.JSON**：比基準寬 5 件，都是 `i_` 的超大指數或溢位數字（`i_number_huge_exp` 等），以預設的 `parse_options{}`（`number_precision::imprecise`）解析成 ±inf 接受（初稿誤寫成 precise，複查 #7）。補法：原文路下 Engine 的 `std::from_chars` 會回 `result_out_of_range`；候選轉換路下 adapter 檢查 `isinf`。比基準嚴：UTF-8 BOM、`i_structure_500_nested_arrays`（`max_depth` 預設 32，可設定）、`123`＋NUL。
- **jsoncons**：比基準寬 10 件：同上 5 件超大指數（`lossless_bignum` 預設開，保留成文字），加 5 件不成對或不完整的代理跳脫。初稿寫「拒絕不成對的高代理、接受低代理」與「adapter 檢查解碼後的字串有沒有代理碼位即可補」，**兩者都錯**（複查 #1）：高代理後面只要接任何 `\uXXXX`，jsoncons 不檢查第二個是否為低代理，直接合成一個補充平面碼位（`["\uD888\u1234"]` 變成 U+32234）；孤立的低代理在轉碼時被丟掉、回傳值被忽略（`json_parser.hpp` 2184～2316 行、2191 行）。資料被**靜默改寫**，解碼後的字串裡看不出來。唯一的補法是 adapter 在原文上自己再掃一遍字串跳脫——等於 adapter 自己寫一個字串語彙分析器。照宣告「有補不上的差異→不通過」判不通過。
- **yyjson**：比基準嚴 3 件（BOM 兩件、`123`＋NUL）。這是 `YYJSON_READ_NOFLAG` 的結果。**J2／J3 的 adapter 要用 `YYJSON_READ_NUMBER_AS_RAW` 取原文**，在這個旗標下另跑一次（只記錄，`results/extra/yyjson-strict-vs-raw.txt`，412 檔）：多接受 5 件，正好是 Boost.JSON 較寬的那 5 件超大指數（`i_number_huge_exp` 等），其他 407 件相同。所以在原文路的組態下，yyjson 對溢位數字一樣比基準寬，補法也相同（Engine 的 `from_chars` 回 `result_out_of_range`）。
- **simdjson**：（判準 1、2 用會驗證全文的 DOM parser；3a 的原文要用不驗證的 On-Demand 取，所以 J2／J3 下要解析兩遍，或另想辦法。DOM 的結果與 On-Demand 的原文不能直接合在一起看，複查 #2、#9。）比基準嚴 4 件：超出 64 位元的整數（`i_number_too_big_*`、`i_number_very_big_negative_int`，預設是 `BIGINT_ERROR`）、`123`＋NUL。
- **BOM**：基準接受 UTF-8 BOM；Boost.JSON、RapidJSON、yyjson 拒絕（yyjson 有 `YYJSON_READ_ALLOW_BOM` 可開）。repo 內容沒有帶 BOM 的檔（70 檔全部一致）。

### 2.2 判準 2：重複鍵偵測

向量 6 個，加 JSONTestSuite 的 2 個 `y_object_duplicated_key*`。**6 個候選全對**（4 個應拒、2 個應收；`{"a":1,"a":2}` 這種跳脫後才相同的鍵也都抓到）。原始結果在 `summary-run1-…json` 的 `c2`。

| 候選 | 偵測方式（分級） | 備註 |
|---|---|---|
| nlohmann | SAX `key()`（ii） | 與今天的 `StrictJson` 相同 |
| Boost.JSON | `basic_parser` handler 的 `on_key`（ii） | DOM 只留最後一個（文件 `parser.hpp` 的〈Duplicate Keys〉），所以只能用 handler |
| RapidJSON | SAX `Key()`（ii） | DOM 也保留重複成員 |
| yyjson | 解析後走訪 DOM（iii） | DOM 保留全部成員；`yyjson.h` 寫多個 API「allows duplicate keys」 |
| simdjson | 解析後走訪 DOM（iii） | DOM 保留全部成員 |
| jsoncons | visitor 的 `visit_key`（ii） | |

沒有一個候選用內建選項做（本調查讀過的設定：Boost.JSON `parse_options`、yyjson 的 read flags、jsoncons `json_options`，都沒有重複鍵選項；其他三個沒有逐項查）。六個都能把偵測放在 adapter 裡，Engine 不必自己做。宣告要回答「要不要第二遍掃描」：(ii) 類（nlohmann、Boost.JSON、RapidJSON、jsoncons）可以在建中性樹的同一遍事件流裡偵測，不必像今天的 `StrictJson` 那樣兩遍；(iii) 類（yyjson、simdjson）是解析成 DOM 後再走訪一遍（同一份 DOM，不重新解析）。這是讀程式得到的結論，沒有量。

### 2.3 判準 3：數字轉換

向量：固定表 40 個（每個一份文件）、生成集 200,000 個（一個陣列；最短表示、`%.17e`、1～40 位有效數字、相鄰 double 的中點各 5 萬）、整數 10,010 個。參考值是 Python `float()`／`int()`（正確捨入），不用任何 C++ 實作當參考。

**3b：`std::from_chars`（與候選無關；libstdc++、gcc-14）**——判定對象（溢位與下溢以外的全部，含 `-0`、`0e0` 等零值；整數是範圍內的）：固定 36＋生成 200,000＋整數 10,008，**0 件不一致**。只記錄的部分：`1e400`、`-1e400`、`1e-400`、`2.4703282292062327e-324`（比最小次正規數的一半略小）都回 `errc::result_out_of_range` 且**不寫入值**（哨兵值保留）；`-9223372036854775809`、`18446744073709551616` 回 `result_out_of_range`。原始輸出 `raw/fromchars-*.txt`。

**3a：候選交出原文（原文路，J2／J3）**

| 候選 | 交出方式 | 固定 40 | 生成 200,000 | 整數 10,010 |
|---|---|---|---|---|
| nlohmann | SAX `number_float(val, string)` 帶原文；整數只有值（ii，部分） | 不一致 3：`-0` 變 `0`；`±1e400` 整份被拒 | 0 | 不一致 1（`-0`） |
| Boost.JSON | handler 的 `on_number_part`＋`on_int64`／`on_uint64`／`on_double` 帶原文（ii） | 0 | 0 | 0 |
| RapidJSON | `kParseNumbersAsStringsFlag` → `RawNumber()`（i） | 不一致 2：`±1e400` 整份被拒（開了旗標仍拒） | 0 | 0 |
| yyjson | `YYJSON_READ_NUMBER_AS_RAW`（i） | 0 | 0 | 0 |
| simdjson | On-Demand `raw_json_token()`，指向輸入，adapter 要去掉尾端空白（iii） | 0 | 0 | 0 |
| jsoncons | `lossless_number(true)`：小數保留成文字；整數只有值（ii，部分） | 不一致 1（`-0`） | 0 | 不一致 1（`-0`） |

**3c：候選自己的轉換（候選轉換路，J1／J4）**——生成 200,000 與範圍內整數 10,008：**6 個候選都 0 件不一致**。差異只在固定表：

- `-0`：6 個都當成整數 0，轉 double 得 `+0.0`（參考值 `-0.0`）。
- 超出範圍的小數（`±1e400`）：nlohmann、RapidJSON、yyjson、simdjson 拒絕整份；Boost.JSON 給 ±inf；jsoncons 給文字（`lossless_bignum`）。下溢（`1e-400`、`2.47…e-324`）：nlohmann、Boost.JSON、RapidJSON、yyjson、simdjson 給 `+0.0`，jsoncons 給文字（初稿寫「接受的都給 +0.0」，錯，複查 #8）。
- 超出 64 位元的整數：nlohmann、Boost.JSON、RapidJSON、yyjson 轉成 double（正確捨入）；simdjson 拒絕整份；jsoncons 給文字。

**用到的旗標**（複查 #7）：3c 用 Boost.JSON `number_precision::precise`、RapidJSON `kParseFullPrecisionFlag`；3a 用 RapidJSON `kParseNumbersAsStringsFlag`、yyjson `YYJSON_READ_NUMBER_AS_RAW`、jsoncons `lossless_number(true)`、Boost.JSON 的 `basic_parser` handler、simdjson On-Demand；其他照 §2.1。**Boost.JSON 與 RapidJSON 預設是不精確模式**（`parse_options.hpp` 的 `numbers = imprecise`；RapidJSON 不加旗標時同樣不保證正確捨入）。J1／J4 下採用它們，每個解析點都要記得設定，這是一條限制；本報告沒有量預設模式的不一致數。

**下溢對 GYO 的影響**：libstdc++ 的 `from_chars` 對 `1e-400`、`2.47…e-324` 回 `result_out_of_range` 且不寫入值；今天的 nlohmann 路徑給 `+0.0`。改走原文路，含極小數的內容會從「讀成 0」變成「錯誤」（複查 #8）。

**`-0` 對 GYO 的影響**：repo 內的 JSON（不含 docs）裡沒有 `-0` 這個數字（`git ls-files '*.json'` 走查 0 件；先以 `{"a":-0}` 正例命中）。今天的路徑（nlohmann）把 `-0` 讀成整數 0；原文路（`from_chars`）會讀成 `-0.0`。改走原文路時這是一個行為差異，要在遷移批的契約裡寫明。

### 2.4 判準 4：寫出與鍵順序

輸入 `{"z":1,"a":{"y":true,"b":[{"k2":0,"k1":1}]},"m":"s"}` → 把 `z` 改成 2、刪 `m`、加 `new` → 寫出；另從空白依序建 `{"c":1,"b":{"y":1,"x":2},"a":[3]}`；再把 `assets/object_fps_pvp/asset_catalog.json` 讀進來原樣寫出。以 Python `object_pairs_hook` 比鍵順序。**6 個候選三項全對**（目錄的鍵與值也全部相同）。原始輸出 `raw/<候選>-order{1,2,3}.json`。

- 要用保留順序的刪除：Boost.JSON 用 `stable_erase`（`object::erase` 會和最後一個交換，見 `object.hpp`），RapidJSON 用 `EraseMember`（`RemoveMember` 會交換）。nlohmann 用 `ordered_json`、jsoncons 用 `ojson`；yyjson 的可變 DOM 本身保留順序。
- simdjson 沒有可改的樹：測試是 adapter 讀 DOM（依文件順序）、用 `builder::string_builder` 逐項重寫，所以判條件通過。

只記錄（`results/extra/numwrite.txt`，輸入 `0.1, 20.1, 1e23, 0.10, 1.5e-7, -0.0, 100, 1.0, 123456789.125`）：

| 候選 | `1e23` 寫成 | `0.10` | `-0.0` | 其他 |
|---|---|---|---|---|
| nlohmann | `9.999999999999999e+22` | `0.1` | `-0.0` | |
| Boost.JSON | `1E23` | `1E-1` | `-0E0` | 一律大寫 E 的科學記號（`20.1` → `2.01E1`） |
| RapidJSON | `9.999999999999999e22` | `0.1` | `-0.0` | |
| yyjson | `1e23` | `0.1` | `-0.0` | |
| simdjson | `1e+23` | `0.1` | `-0.0` | builder 有 `append_raw` 可照原文寫 |
| jsoncons | `1e+23` | `0.1` | `0.0`（符號掉了） | |

沒有一個候選的 DOM 會照原文寫回數字（`0.10` 都變 `0.1`）；資產目錄改寫今天只寫整數與字串，不受影響。

**未測**（宣告承諾、本報告沒做，複查 #10）：縮排與換行的控制、字串跳脫的寫法（只記錄項）；真實資產目錄只做了原樣讀寫，沒有做「改值、插入、刪除」（改動只在小向量上做）。

實作細節（複查 #13）：yyjson 的 `yyjson_mut_obj_put` 文件寫「加在物件尾端，可能先刪除同名鍵」（`yyjson.h:3946`），實作是原地取代（`yyjson.h:7275`）；順序測試通過靠的是實作，adapter 宜改用 `yyjson_mut_obj_replace`。另外 `run.py` 判準 4 沒有先刪舊輸出檔，崩潰時可能讀到舊檔；這次各候選狀態都是 OK，沒有影響。

### 2.5 判準 5：C++20、三平台、授權、維護

- **編譯**：linux-x64 gcc-14 `-std=c++20 -O2 -Wall -Wextra -Wpedantic`，6 個都成功，警告 0（以 `-isystem` 含入；另以一般 `-I` 含入也是 0，`results/extra/warnings.txt`）。**MSVC 與 macos-arm64 確認できない。** 各候選官方 CI 的 workflow 檔名（2026-11-13 以 `gh api` 讀，回應未保存；只讀檔名，未讀內容，不當作 GYO 的實測）：nlohmann `ubuntu.yml`、`macos.yml`、`windows.yml`；jsoncons `ubuntu.yml`、`macos.yml`、`windows.yml`；yyjson `compilers.yml`、`archs.yml` 等；simdjson 有 `macos.yml`、`msys2*.yml`、`aarch64.yml` 等；Boost.JSON 的 repo 內 GitHub workflow 只有 bloat 與 fuzzer；RapidJSON 沒有 GitHub workflow（有 `appveyor.yml`）。
- **授權**（讀下載後的檔）：nlohmann MIT；Boost.JSON BSL-1.0；RapidJSON MIT（`include/rapidjson/msinttypes` 是 BSD，只給舊 MSVC 用；`bin/jsonchecker/` 是 JSON License，不使用）；yyjson MIT；simdjson Apache-2.0（只下載了合併單檔、沒有 LICENSE 檔，這是 GitHub API 的值，不是讀檔），單檔內另有 pytorch 衍生程式碼的告示（`simdjson.h` 約 15200 行）、string-view-lite（BSL-1.0，`simdjson.h:892`）與 fast_float 的 Apache-2.0／MIT／BSL-1.0 三選一，Apache-2.0 要附 NOTICE；jsoncons BSL-1.0。走查 GPL／LGPL（`-w`）0 件。**6 個都寬鬆。**
- **維護**（2026-11-13 查，`maintenance-20261113.json`；12 個月＝2025-11-13 起）：

| 候選 | archived | 12 個月 commit／作者 | 最多的作者 | 最後 release | 12 個月內 release | open issue＋PR |
|---|---|---|---|---|---|---|
| nlohmann | 否 | 699／56 | nlohmann 484、dependabot 141 | v3.12.0（2025-04-11） | 0 | 40 |
| Boost.JSON | 否 | 97／6 | grisumbras 62（64%） | 隨 Boost 發行（1.90 2025-12、1.91 2026-04、1.92 2026-08；`boostorg/boost` 的 releases，`gh api` 回應未保存，boostorg/json 本身沒有 GitHub release） | 3（Boost） | 89 |
| RapidJSON | 否 | **1／1** | miloyip 1 | **v1.1.0（2016-08-25）** | 0 | **817** |
| yyjson | 否 | 126／25 | ibireme 75 | 0.13.0（2026-09-08） | 1 | 32 |
| simdjson | 否 | 371／57 | lemire 232 | v5.1.1（2026-11-01） | 30 | 78 |
| jsoncons | 否 | 1,135／26 | danielaparker 1,046 | v1.10.0（2026-10-05） | 6 | 16 |
| glaze | 否 | 900／43 | stephenberry 728 | v9.1.0（2026-10-28） | 39 | 84 |

照宣告的定義（未 archived 且 12 個月 commit > 0），全部「有人維護」。風險：RapidJSON（實質停止）；nlohmann 19 個月沒有 release；實質單人主導的有 jsoncons、nlohmann、yyjson、simdjson、glaze、Boost.JSON（grisumbras 62／97，和 yyjson 的 ibireme 75／126 同量級；初稿漏列，複查 #4）（和 #176 對 ufbx 的看法相同：釘版本＋adapter）。yyjson 0.x 版本號，API 可能變。

### 2.6 判準 6：效能（只記錄）

容器內，`-O2`，解析＋走訪所有數值，21 次取中位數（ms）：

| | nlohmann | Boost.JSON | RapidJSON | yyjson | simdjson | jsoncons |
|---|---|---|---|---|---|---|
| `biome.voxel_mesh.json`（1.4 MB，repo 最大的 JSON） | 41.2 | 8.4 | 12.7 | 3.8 | 4.3 | 26.4 |
| 合成 10.6 MB | 283.6 | 80.7 | 66.0 | 18.5 | 16.4 | 130.4 |

一台非 L1 主機、一次量測，只當量級參考。效能的設定和嚴格解析的設定不同（複查 #12）：RapidJSON 沒開驗證編碼與迭代旗標；nlohmann 只解析一遍（今天的 `StrictJson` 是兩遍）；jsoncons 用 `json` 不是 `ojson`；simdjson 只量 DOM（J2／J3 若再加 On-Demand 取原文要多一遍）。

## 3. 對各方向的影響（J1～J4；不推薦方向）

| | J1：nlohmann 留 Engine、凍結含入點 | J2：中性文件樹＋JSON adapter 做成 plugin | J3：中性文件樹＋Engine 內唯一 adapter | J4：現在 J1，觸發條件寫進 RC，觸發時做 J3 |
|---|---|---|---|---|
| 函式庫出現在幾處 | 今天的位置：Engine 8 檔＋建置期 1、framework 2、apps 7、tools 18 | 1 個 adapter（＋產品、工具是否也改用中性樹，依未決 6；建置期的 shader pipeline 依未決 2） | 同 J2 | 現在同 J1 |
| 數字走哪條路 | 候選轉換路（3c） | 原文路（3a＋3b） | 原文路（3a＋3b） | 現在候選轉換路 |
| 換函式庫的代價 | 改上面所有檔（量級：程式 36 檔＋測試） | 只改 adapter | 只改 adapter | 現在同 J1 |
| 重複鍵偵測放哪裡 | 每個需要的使用點各自做（今天 `StrictJson`、`AnimationEvents` 兩處） | adapter 一處（6 個候選都可以） | 同 J2 | 現在同 J1 |
| 嚴格解析的差異由誰補 | 各使用點 | adapter 一處 | adapter 一處 | 現在同 J1 |
| 寫出與鍵順序 | 函式庫的 DOM 要保留順序（simdjson 不行） | 中性樹自己保存順序，adapter 只要能依序讀、依序寫（simdjson 也可以） | 同 J2 | 現在同 J1 |

各候選在各方向下的意義：

- **nlohmann**：J1／J4 不必遷移（今天就是）；候選轉換路的生成集 0 不一致，與其他候選同等。J2／J3 下，整數沒有原文（`-0` 失去符號）、超大數整份被拒。
- **Boost.JSON**：J2／J3 下原文完整（handler）；要用 handler 自己建中性樹（DOM 會丟重複鍵）；`max_depth` 要設定；溢位數字要補（Engine 的 `from_chars` 會擋）；寫出是大寫 E 科學記號（若 adapter 用它寫出，資產目錄的 diff 會變）。J1 下要改所有使用點、每處設精確模式；Boost 發行包解壓 493 MB（其中 `boost/` 標頭 184 MB，本機 `du`）。
- **RapidJSON**：J2／J3 下原文完整（旗標），但超大數整份被拒；維護實質停止。J1 下同上要改所有使用點。
- **yyjson**：J2／J3 下原文完整（旗標）；在原文旗標下溢位數字比基準寬（同 Boost.JSON，Engine 的 `from_chars` 會擋）；DOM 可直接走訪做重複鍵與建中性樹、可寫出；C 介面，adapter 要包。J1 下要改所有使用點。
- **simdjson**：J2／J3 下，嚴格檢查用 DOM、原文用 On-Demand，要解析兩遍（DOM 交不出小數的原文；DOM 有 `number_as_string(true)` 可把大整數存成文字，`simdjson.h:7060`，沒有測）；沒有可改的樹，寫出靠 adapter 自己（中性樹本來就自己保存順序，所以這點在 J2／J3 下影響小）；超出 64 位元的整數預設拒絕。J1／J4 下不適合（沒有可改的 DOM，資產目錄改寫要每處自己重寫）。
- **jsoncons**：預設接受註解（要明設 `allow_comments(false)`）；不成對的代理跳脫被靜默改寫且 adapter 補不上（除非自己再掃原文）；整數沒有原文。

## 4. 結論（只列建議，由使用者選）

方向與函式庫是兩個決定，下面依方向分開寫。

- **若選 J1 或 J4**：**維持 nlohmann**。理由：判準 2、4 與其他候選同等，判準 3 的候選轉換路在生成集上與其他 5 個相同（0 不一致）；換成別的庫要改約 36 個檔。換成 yyjson 或 simdjson 時，判準 1 會從「不通過」變「通過」（差在 `123`＋NUL 這一個 `n_`）；這個差異對 GYO 的影響大小，由使用者判斷（初稿寫「沒有一條判準變好」，與 §1 矛盾，複查 #3）。其他要知道的事：19 個月沒有 release。
- **若選 J2 或 J3**：第一候選 **yyjson**，第二候選 **Boost.JSON**。兩者在原文路的組態下，判準 1 都要補溢位數字（Engine 的 `from_chars` 會擋），這點不是兩者的差別。
  - yyjson：3a 原文 0 不一致（含 `-0` 與超大數）、重複鍵與寫出都直接用同一份 DOM、量到的最快、依賴小（單一 C 檔）；代價：C 介面要包、0.x 版本、主要一人維護。
  - Boost.JSON：3a 原文 0 不一致、隨 Boost 定期發行；代價：重複鍵與原文都要用 handler 自己建樹、`max_depth` 要設、Boost 標頭量大、寫出格式是大寫 E、主要一人維護（初稿寫「多人維護」，與證據不符，複查 #4）。
  - simdjson 也滿足判準 1、3a，但嚴格檢查與原文要兩種 parser（兩遍）、沒有可改的樹；只在「adapter 只讀、寫出由中性樹自己做」且接受兩遍解析的設計下才接近，列第三。
- **不建議**：RapidJSON（維護實質停止）、glaze（C++23）、jsoncons（判準 1 不通過：代理跳脫被靜默改寫；預設接受註解；整數沒有原文）。

不論選哪一個：**windows-x64 與 macos-arm64 的數字轉換（3b 或 3c）沒有驗**。選定後，建議在遷移批的 L1 上用本批的向量（`vectors/num_*`，種子固定）各跑一次，作為那一批的事前宣告判定。

## 5. 對 J2／J3 中性介面的觀察（只陳述）

- 數字原文的取法分三種：旗標（RapidJSON、yyjson）、事件帶原文（Boost.JSON、nlohmann 只限小數、jsoncons 只限小數）、指向輸入的位移（simdjson，要去尾端空白）。介面若定成「數字＝原文字串」，前兩種都能直接交出；nlohmann 與 jsoncons 交不出整數的原文。
- 重複鍵偵測能全部放在 adapter，Engine 的中性樹可以假設「鍵已唯一」。
- `-0`、超出範圍的數字與下溢，原文路由 Engine 的 `from_chars` 決定（`-0.0`；`result_out_of_range`，含今天讀成 `+0.0` 的極小數）；這三條要寫進中性介面的契約。

## 6. 要使用者決定的事

1. **方向（未決 1）與函式庫**（§4）。
2. **字面判定與實際影響不同的格**，要不要改判準的範圍（看了結果之後的放寬，依 §14.1 不由我做）：
   - 判準 1：jsoncons 改判不通過是照字面（複查發現補法不成立），不是放寬。
   - 判準 3：`-0` 讓 6 個候選的候選轉換路都「不一致 1」；超出 double 範圍的數字讓 nlohmann、RapidJSON 的原文路「不一致」（它們拒絕整份文件，而原文路下 Engine 的 `from_chars` 也會拒絕）。
   - 判準 1：基準 nlohmann（今天的 `StrictJson`）與 RapidJSON 因 `123`＋NUL 判不通過。
3. **宣告中未回覆的兩項**：判準 5「有人維護」的定義、判準 1「條件通過」的分級——本報告照宣告寫法判定。
4. **三平台**：要不要在選定之後補 windows-x64、macos-arm64（建議在遷移批的 L1 上做，見 §4 末）。
5. 觀察到的既有行為（本批不修）：今天的 `StrictJson` 接受數字後接 NUL 的文字。要不要開卡由使用者決定。

## 7. 複查

- 複查者：general-purpose 代理，**Opus 5.5**／high（繼承），只讀。與作者同模型（架構師建議；使用者沒有選 Fable），盲點可能重疊。
- 範圍：§1 總表 49 格＋§2.3 3a 表 24 格＝73 格，對照 `raw/`、summary、函式庫原始碼。
- 發現 14 條：must 4、should 6、note 4。**全部接受**。改判 1 格：jsoncons 判準 1「條件通過」→「不通過」（#1）。其餘是理由與措辭的更正、揭露與補記：
  - #2 判準 1 與 3a 的組態不同：補跑 yyjson 原文旗標下的判準 1（只記錄，`results/extra/yyjson-strict-vs-raw.txt`），§2.1、§3、§4 改寫；simdjson 兩遍解析寫入。
  - #3 §4 J1／J4 的理由與 §1 矛盾：改寫。
  - #4 Boost.JSON「多人維護」：刪除，列入單人主導。
  - #5 件數の突き合わせ標記與判定範圍的變更：改標「不一致」並揭露（開頭、§0）。
  - #6 容器紀錄：事後補記 `results/docker-runs.txt`。
  - #7 旗標表、Boost／RapidJSON 預設不精確的限制、§2.1 的誤寫：補與更正。
  - #8 下溢的行為差異與措辭：補與更正。
  - #9 simdjson 兩遍解析、`number_as_string`：補。
  - #10 未測項目：明寫未測；判準 2 的「第二遍掃描」補答。
  - #11 出處：simdjson 授權是 API 值、string-view-lite、CI 檔名與 Boost 發行日期的回應未保存、解壓量。
  - #12 效能設定的偏向：補一行。
  - #13 yyjson `put` 的文件與實作、判準 4 舊輸出檔：補。
  - #14 與宣告的小差異：§0 補一行。
- 複查者核過、沒有發現問題的：崩潰與逾時不會被當成正常結果；Boost `erase`／`stable_erase`；jsoncons 預設接受註解；simdjson 預設 `BIGINT_ERROR`；yyjson 沒有重複鍵旗標；基準與 `StrictJson.hpp` 的呼叫方式相同；repo 內 70 檔各候選與基準相同（最深 7 層，小於 Boost 的上限 32）；寫出格式與維護數字（Boost 除外）。

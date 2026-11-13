# JSON 函式庫的選擇調查：事前宣告（`v8-arch-json-library-survey`）

更新：2026-11-13。Owner：架構。卡片由架構師會話開出。作者：Opus 5.5／high（卡片指定 Opus／high）。
**狀態：宣告草稿，停下等使用者核准。核准前不下載任何東西、不寫實測程式。**
只寫文件與 git 管理外的實測：不改 `engine/`、`apps/`、`tools/`、`framework/`、`.github/` 與主建置，不定 JSON 的方向（未決 1），不定用哪個庫。

## 0. 起因與範圍

- 後端 plugin 規劃批（PR #212，分支 `origin/claude/arch-adapter-plugins-plan` 的 `plans/kernel/adapter-plugins-plan.zh-Hant.md` §4 未決 1）列了 JSON 的四個方向：J1（nlohmann 留 Engine、凍結含入點）、J2（中性文件樹＋JSON adapter 做成 plugin）、J3（中性文件樹＋Engine 內唯一的 adapter；數字以原文交給 Engine，用 `std::from_chars` 轉）、J4（現在 J1，觸發條件寫進 RC，觸發時做 J3）。
- **未決 1 與函式庫的選擇都還沒定**，都等本批的結果（使用者 2026-11-13 更正，經架構師轉達：「未決 1：就是未決，方向不是 J3，等調查完畢之後在決定。」；任務卡背景原寫「已定 J3」，作廢）。使用者原話另有：「nlohmann 問題在調查後決定」。
- 本批回答的問題：六條判準上，每個候選的實測結果、限制與代價，以及**每個候選在 J1～J4 各方向下意味著什麼**（§3.7）。不以任何方向為前提。結論只列建議，由使用者選方向與函式庫。
- **不做**：不改程式與建置；不定方向；不定庫；不設計中性文件樹介面（若選 J2／J3，那是條件批 `v8-arch-json-adapter`）。

## 1. 今天的使用點（2026-11-13，`origin/master` `b9bf1962`）

走查方法：`git grep -c -P '#\s*include\s*[<"]nlohmann/'`（終端碼 0；先以已知正例 `engine/base/include/engine/base/StrictJson.hpp` 的 `nlohmann::basic_json` 命中一行，確認走查有效；stderr 不丟）。另以 `git grep -l -P 'ordered_json'`、`'ParseStrictJson|StrictJson\.hpp'`、CMake 的 `nlohmann` 走查。

| 層 | 含入 nlohmann 的檔（程式碼，不含 docs 與 mutation json） | 備註 |
|---|---|---|
| Engine | `asset/src/ContentManifest.cpp`、`asset/src/catalog/CatalogParser.cpp`、`render/src/ShaderLibrary.cpp`、`ui/src/UiDocumentCodec.cpp`、`ui/src/UiSerialization.cpp`、`map_markers/src/MapMarkers.cpp`、`voxel_mesh/src/VoxelMesh.cpp`、`model/src/AnimationEvents.cpp`（8）；建置期 `render/shaders/pipeline/src/main.cpp`（1） | 全部 PRIVATE。`StrictJson.hpp`（base）以模板參數接 nlohmann、不含入；實例化的只有 MapMarkers、VoxelMesh。`AnimationEvents.cpp` 用 parser callback 自做重複鍵偵測（`ParseWithoutRepeatedKeys`）。`ordered_json`：MapMarkers、UiDocumentCodec、UiSerialization |
| framework | `bindings/json/src/CatalogText.cpp`、`EventSoundsText.cpp`（2） | `CatalogText.cpp` 用 `ordered_json` 改寫資產目錄 |
| apps | `object_fps_pvp` 7 檔（`AssetDefinitionHelpers.hpp`、`HitFeedback.cpp`、`SoundCues.cpp`、`WeaponPresentationDefinition.cpp`、`ClientConnection.cpp`、`Arena.cpp`、`MovementRulesFile.cpp`） | `Arena.cpp` 的 `ArenaContentDigest` 取 JSON 小數（determinism.md §4.6） |
| tools | `audio_editor` 1、`ui_editor` 14、`voxel_map_editor` 3（18） | `audio_editor/src/ProjectFile.cpp` 用 `ordered_json` |
| 測試／acceptance | `tests/` 22 檔（C++ 與守衛 py）、`build/acceptance/object_fps_pvp/` 6 檔 | 守衛 py 是字串比對，不是使用 |

來源版本：`third_party/nlohmann_json/CMakeLists.txt` 與 shader pipeline 的 `Dependencies.cmake` 都是 `v3.12.0`（FetchContent）。原始輸出：主 checkout `build/evidence/json-library-survey/usage-grep-20261113.txt`（本機 macOS x86_64；156 行，含各走查的終端碼）。

## 2. 候選與取捨

| 候選 | 版本（2026-11-13 查） | 官方來源 | 授權（API 讀的 SPDX，下載後讀 LICENSE 核對） | 處置與理由 |
|---|---|---|---|---|
| nlohmann/json | v3.12.0（2025-04-11；最新 release，與現用相同） | https://github.com/nlohmann/json | MIT | **實測**。現用；同時是判準 1 的基準欄（今天 `StrictJson` 的拒絕集合＝nlohmann 3.12.0 預設設定的拒絕集合） |
| Boost.JSON | Boost 1.92.0（2026-08-12；1.93.0 只有 beta1） | https://github.com/boostorg/json（發行包 https://github.com/boostorg/boost/releases） | BSL-1.0 | **實測**。物件保留插入順序；有逐事件的 `basic_parser` handler |
| RapidJSON | 無新 release（最後 v1.1.0，2016-08-25）；用 master `297637bc`（2026-11-03） | https://github.com/Tencent/rapidjson | MIT（`bin/jsonchecker/` 是 JSON License，不使用；msinttypes 是 BSD） | **實測**。有 SAX 與數字以字串交出的旗標（實測確認）。release 停在 2016 是維護風險，判準 5 記錄 |
| yyjson | 0.13.0（2026-09-08） | https://github.com/ibireme/yyjson | MIT | **實測**。C 函式庫；有「數字以原文讀」的旗標、可寫出（實測確認） |
| simdjson | v5.1.1（2026-11-01） | https://github.com/simdjson/simdjson | Apache-2.0（內含 Dragonbox：Apache-2.0 with LLVM exception 或 BSL-1.0） | **實測**。側邊會話推測「不能寫出」；README 有 Builder（`doc/builder.md`）可寫 JSON 字串，所以不在判準 4 先排除，實測判定 |
| jsoncons | v1.10.0（2026-10-05） | https://github.com/danielaparker/jsoncons | BSL-1.0（API 回 NOASSERTION；LICENSE 檔頭寫 Boost Software License 1.0） | **新增，實測**。理由：有保留插入順序的 `ojson` 與「數字不失真」（lossless number）選項，直接對應判準 3、4（實測確認） |
| glaze | v9.1.0（2026-10-28） | https://github.com/stephenberry/glaze | MIT | **只做文件判定，不下載**。README 第 256 行「Requires C++23」；GYO 是 C++20（`build/cmake/GyoBuild.cmake:4`），判準 5 不過；其餘五條記「不評（判準 5 前提不成立）」 |
| taoJSON | 無正式 release（tag 只到 1.0.0-beta.14） | https://github.com/taocpp/json | MIT | **不採**。沒有正式版本可釘；與 jsoncons 的強項重疊。使用者要加時再補 |

件數：實測 6 個、文件判定 1 個，評估格數＝7 候選 × 6 判準＝**42 格**（glaze 的 5 格記「不評」）。報告開頭以這個數字做件数の突き合わせ。

## 3. 判準的實測方法與判定

共通：每個候選寫一支最小的 C++20 程式（或 C 介面的 C++ 包裝）跑同一組向量，輸出逐行結果；由一支 Python 比對腳本（`python3 -I`，跨平台；逾時用 Python `subprocess` 的 timeout）判定。每個向量在子程序裡跑，崩潰或逾時記為該向量的結果（不讓一個崩潰吃掉整組）。每個候選用它文件寫的**最嚴設定**；用到的旗標逐一寫在報告。

### 判準 1：嚴格解析

- **向量**：
  1. JSONTestSuite（https://github.com/nst/JSONTestSuite，master `1ef36fa0`，MIT）的 `test_parsing/` 318 檔：`y_`（必須接受）、`n_`（必須拒絕）、`i_`（實作自訂）。這是外部語料，不是作者自己的理解（AGENTS §14.2）。
  2. GYO 補充向量（約 20 個）：`//` 與 `/* */` 註解、物件與陣列的尾逗號、`01`、`+1`、`.5`、`1.`、`NaN`、`Infinity`、`-Infinity`、`0x10`、單引號、未加引號的鍵、字串內控制字元、`\x41`、孤立代理 `"\ud800"`、非法 UTF-8 位元組、UTF-8 BOM、值後的多餘內容、多個頂層值、空輸入、只有空白。
- **基準**：nlohmann 3.12.0 以 `StrictJson` 今天的呼叫方式（`sax_parse` 與 `parse(…, nullptr, false)`，預設不忽略註解、不允許尾逗號）跑同一組，得到「今天的拒絕集合」。已知 nlohmann 預設會跳過 UTF-8 BOM——這是基準的一部分，報告照實寫。
- **判定**（每個候選，以最嚴設定）：
  - **通過**：所有 `y_` 接受、所有 `n_` 拒絕，且在全部向量上「候選拒絕的 ⊇ 基準拒絕的」（一致或更嚴）。
  - **條件通過**：較寬鬆的差異只在 `i_` 或 GYO 補充向量，且每一項都能由 adapter 的前置檢查補上（逐項列出差異與補法）。
  - **不通過**：接受了任何 `n_`、拒絕了任何 `y_`、崩潰或逾時，或有補不上的差異。
  - 更嚴的差異（基準接受、候選拒絕）逐項列出，標明會不會讓今天有效的 GYO 內容變無效（以 repo 內所有 `*.json` 內容跑一次確認）。

### 判準 2：重複鍵偵測

- **向量**（6 個）：`{"a":1,"a":2}`（拒）；巢狀 `{"o":{"a":1,"a":2}}`（拒）；陣列中的物件 `[{"a":1},{"a":1,"a":2}]`（拒）；跳脫後相同 `{"a":1,"a":2}`（拒，今天 nlohmann 比的是解碼後的鍵）；兄弟物件同名 `{"x":{"a":1},"y":{"a":2}}`（收）；大小寫不同 `{"a":1,"A":2}`（收）。
- **偵測位置的分級**：(i) 函式庫內建選項；(ii) 逐事件 API（SAX／handler），在 adapter 寫最小偵測器；(iii) DOM 保留重複成員，解析後走訪偵測；(iv) 做不到（DOM 丟掉前者、又沒有事件可接）。
- **判定**：**通過**＝(i)～(iii) 之一，且 6 個向量全對；報告寫偵測放在哪一層（adapter 或 Engine）以及要不要第二遍掃描（今天 `StrictJson` 是兩遍：SAX 偵測＋DOM）。**不通過**＝(iv)，或任何向量錯。

### 判準 3：數字轉換三平台位元一致

數字經過哪一條路轉成 double，依方向而不同：J2／J3 是「候選交出原文 → Engine 用 `std::from_chars`」；J1／J4 是「候選自己轉」（nlohmann 今天交給 libc `strtod`）。所以判準 3 拆成三部分，兩條路都量：

- **3a 候選交出原文**（判定用）：候選能否把每個數字的**原文位元組**交給呼叫端（數字以字串讀的旗標、事件 API 帶原文、或在輸入中的位移）。向量＝下面的固定表加生成集；比對＝交出的字串與輸入中該數字的位元組逐位元組相同。方式依 (i) 旗標／(ii) 事件帶原文／(iii) 位移／(iv) 不可 分級。
- **3b `std::from_chars` 三平台一致**（判定用，與候選無關、每平台跑一次）：把生成集與固定表的原文以 `std::from_chars` 轉 `double`（整數轉 `int64_t`／`uint64_t`），輸出位元的十六進位，與參考值比對。
  - **參考值**：Python `float()`（正確捨入）與 `int()` 產生，不用任何 C++ 實作當參考。
  - **固定表**（約 40 個）：`0.1`、`0.7`、`20.1`（`ArenaDigestTests` 的值）、`1e23`、`8.98846567431158e307`、`2.2250738585072011e-308`、`2.2250738585072014e-308`、`4.9406564584124654e-324`、`5e-324`、`1.7976931348623157e308`、`9007199254740993`、`-0`、`-0.0`、`0e0`、`1E+2`、正好在兩個 double 中點的十進位（以 `fractions` 算出）、800 位尾數、`1e400`（溢位）、`1e-400`（下溢）、`int64` 上下限與超出一位、`uint64` 上限與超出一位。
  - **生成集**：固定種子 `20261113`，200,000 個字串：隨機位元型樣 double 的最短表示 5 萬、`%.17e` 5 萬、1～40 位有效數字且指數 −330～310 的隨機十進位 5 萬、相鄰 double 的中點 5 萬。生成腳本與輸出檔一起存，任何平台重跑得同一份。
  - 溢位、下溢的行為（`errc::result_out_of_range`、值是否被寫入）逐平台記錄並比對。
- **3c 候選自己的數字轉換**（判定用，J1／J4 的路）：同一組向量經候選自己的轉換（以它最精確的設定）得到的位元，逐平台與參考值比對。
- **判定**（分兩條路記，各自判）：
  - **原文路（J2／J3）通過**＝3a 為 (i)～(iii) 且原文全對，且 3b 三平台與參考值 0 件不一致。3b 與候選無關：3b 不通過時寫成「這條路在該平台不成立」（即 J2／J3 選項在此條件下不成立），照常完成其他格，不停下。
  - **候選轉換路（J1／J4）通過**＝3c 三平台與參考值 0 件不一致。

### 判準 4：寫出並保留鍵順序

- **向量**：輸入 `{"z":1,"a":{"y":true,"b":[{"k2":0,"k1":1}]},"m":"s"}` 加一份 repo 的真實資產目錄。操作：讀入 → 改一個值 → 插入一個新鍵 → 刪一個鍵 → 寫出；另一個從空白依指定順序建文件並寫出。
- **比對**：Python 以 `object_pairs_hook` 讀出寫出的檔，鍵序列與預期（原順序，新鍵接在最後）逐項相同。
- **判定**：**通過**＝函式庫的文件樹可修改後寫出並保留順序；**條件通過**＝只能由呼叫端逐項寫出（adapter 要自建樹，例如 simdjson 的 Builder），寫出工時影響；**不通過**＝不能寫出。
- 只記錄：縮排與換行的控制、數字寫出的格式（是否能照原文寫回，例如 `0.10` 保持 `0.10`；最短表示）、字串跳脫的寫法。這些影響資產目錄改寫的 diff，但不是判準（判準不增減）。

### 判準 5：C++20、三平台、授權、維護

- **編譯**：每個候選的實測程式以 `-std=c++20 -Wall -Wextra -Wpedantic`（clang／gcc）與 `/std:c++20 /W4 /permissive- /utf-8`（MSVC）編譯，照 GYO 的設定（`build/cmake/GyoAppProject.cmake:104-106`）。函式庫標頭以 system include 方式含入（與 FetchContent 的現況相同）。判定：三平台無錯誤編譯並跑完判準 1～4；警告數記錄、不當門檻。
- **授權**：讀下載後的 LICENSE 與附帶的第三方授權檔，記錄 SPDX 與例外。判定：MIT、BSD、BSL-1.0、Apache-2.0、zlib 類為寬鬆；含 GPL／LGPL 或 JSON License 必要部分則不通過（RapidJSON 的 `bin/jsonchecker/` 不使用則不算）。
- **維護**（查證日期寫進報告）：最後 release 與日期、過去 12 個月的 commit 數與不同作者數、open issue 數、是否 archived（`gh api`）。判定：**有人維護**＝未 archived 且過去 12 個月 commit > 0；release 停滯一年以上或實質單人維護另標「風險」，不判不通過（與 #176 對 ufbx 的處理一致：釘版本＋adapter）。

### 判準 6：效能（只記錄）

容器內（§5.1），Release（`-O2`），以 repo 中最大的 JSON 內容檔與一份合成的 10 MB 檔，量「解析＋走訪全部數值」的時間，21 次取中位數。只記錄，不當門檻，不在三平台量。

### 3.7 對各方向的影響（報告的一節，不是判準）

報告用一張表，每個候選 × J1～J4，寫該候選在那個方向下意味著什麼，不以任何方向為前提：

- **出現在幾處**：J1／J4 下函式庫直接出現在今天的 9 個 Engine 檔＋產品、工具（換庫＝改這些檔）；J2／J3 下只出現在一個 adapter。
- **數字**：判準 3 的哪一條路通過（原文路或候選轉換路）。
- **重複鍵偵測放哪裡**：判準 2 的分級 (i)～(iii) 決定偵測能放在 adapter 內，還是要 Engine 自己做（例：DOM 保留重複成員時，Engine 走訪偵測）。
- **嚴格解析的差異由誰補**：判準 1 的「條件通過」項目在 J1／J4 下由各使用點補，在 J2／J3 下由 adapter 一處補。
- **寫出與鍵順序**：判準 4 的結果；J2／J3 下中性文件樹要不要自己保存順序。
- **遷移量**：從 nlohmann 換過去要動的檔數（以 §1 的盤點算，量級即可）。

這節只陳述事實與推論，不推薦方向。

## 4. 下載清單（核准後才下載）

全部放主 checkout `build/evidence/json-library-survey/`（git 管理外），每個函式庫一個新的空目錄；實測程式與腳本放另一個目錄 `build/evidence/json-library-survey/probe/`。下載物不執行其中的建置腳本與測試；只把標頭與原始檔交給我寫的編譯命令。

| 項目 | 來源（官方） | 版本／commit | 大約大小 | 放哪裡 |
|---|---|---|---|---|
| nlohmann/json | https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz | v3.12.0 | 115 KB（release asset 實值 114,576 B） | `lib/nlohmann-3.12.0/` |
| Boost（只用 Boost.JSON 的 header-only 模式 `boost/json/src.hpp` 與它依賴的標頭） | https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-b2-nodocs.tar.xz | 1.92.0 | 52.6 MB 壓縮（release asset 實值 52,633,196 B）；解壓約 200 MB 以內（估） | `lib/boost-1.92.0/` |
| RapidJSON | https://codeload.github.com/Tencent/rapidjson/tar.gz/297637bc0a0f00dc504dbc1a47ebdaa577f8ae19 | master `297637bc`（2026-11-03） | 約 1～3 MB（估，codeload 不回 Content-Length；下載後記實值） | `lib/rapidjson-297637bc/` |
| yyjson | https://codeload.github.com/ibireme/yyjson/tar.gz/refs/tags/0.13.0 | 0.13.0 | 約 1～2 MB（估） | `lib/yyjson-0.13.0/` |
| simdjson | https://github.com/simdjson/simdjson/releases/download/v5.1.1/simdjson.h 與 `simdjson.cpp` | v5.1.1 | 13.5 MB（實值 10,071,593＋3,382,592 B） | `lib/simdjson-5.1.1/` |
| jsoncons | https://codeload.github.com/danielaparker/jsoncons/tar.gz/refs/tags/v1.10.0 | v1.10.0 | 約 3～10 MB（估） | `lib/jsoncons-1.10.0/` |
| JSONTestSuite（只取 `test_parsing/`） | https://github.com/nst/JSONTestSuite（`git clone --filter=blob:none --sparse` 後只 checkout `test_parsing`） | master `1ef36fa0` | 318 檔，約 1 MB 以內（估；整個 repo 87 MB，不整個取） | `corpus/JSONTestSuite-1ef36fa0/` |

合計約 75～85 MB 下載、解壓後約 300 MB 以內。每個檔下載後記 SHA-256 與實際大小到 `downloads.txt`；release 有 `.asc` 或 `.sha256` 的（nlohmann、Boost）一併核對。（使用者選 C：只在本機；實測在 Docker 容器內，§5.1。下載在主機上進行（放進掛載目錄），解壓與編譯、執行都在容器內。）

## 5. 三平台怎麼跑（已定：C＋Docker，見 §5.1、§5.2；下面的選項表留作決定的紀錄）

本機是 **macOS x86_64**（Intel i9），不是 L1 的三平台之一（linux-x64、windows-x64、macos-arm64）。所以：

- **本機可做**：判準 1、2、3a、4、6 的全部，3c 的 macOS x86_64 結果（輔助），判準 5 的 macOS clang 編譯與授權、維護；3b 的 macOS libc++ 結果（輔助，非 L1；部署目標 13.3，libc++ 的浮點 `from_chars` 可用）。
- **一定要三平台的**：判準 3b（三套標準庫的 `from_chars`：libstdc++、MSVC STL、libc++ arm64）與 3c（候選自己的轉換；nlohmann 經各平台 libc 的 `strtod`），判準 5 的三平台編譯（尤其 MSVC）。判準 1、2、3a、4 的邏輯與平台無關，三平台各跑一次作確認。

| 選項 | 做法 | 代價 | 限制 |
|---|---|---|---|
| **A（建議）** | 開一個臨時分支 `claude/tmp-json-survey-ci`，只放一個 `.github/workflows/json-survey.yml`（`on: push`，只限該分支）與實測程式；三平台 job 從 §4 的 URL 下載、核對 SHA-256、編譯、跑、上傳 artifact；結果下載到主 checkout `build/evidence/json-library-survey/ci/`。完成後刪分支，不開 PR、不合併 | 改 `.github/`（在臨時分支上，**卡片規定要先問**）；CI 約 3 job × 15～25 分鐘；實測程式短暫出現在遠端分支歷史中（刪分支後不留在 master） | 與 L1 的 runner 相同映像，結果代表 L1 平台 |
| B | 給使用者 Windows、Linux 實機各一支離線腳本（程式與函式庫事先放好，不假設現場有網路） | 使用者的空檔與手動操作 | macos-arm64 沒有實機時缺一平台，3b 的 arm64 libc++ 記「確認できない」 |
| C | 只做本機 | 0 | 3b 與三平台編譯在報告記「確認できない」，判準 3、5 不能判定 |

建議 A，理由：只有 CI 有 macos-arm64；與 L1 同映像；不碰 master 的 `.github/` 與主建置。

**使用者 2026-11-13 決定：C（只在本機），且實測放在 Docker 下。** 因此：

### 5.1 容器

- **映像**：本機已有的 `gyo-linux-toolchain:l1-20261112`（id `sha256:4b443a12811…`；由 repo 的 `build/ci/linux-toolchain/Dockerfile` 建成，基底 `ubuntu:24.04@sha256:2198e4c6…`；gcc-14 14.2.0-4ubuntu2~24.04.1、cmake 3.31.6、ninja 1.11.1、Python 3.12.3；照 L1 linux-x64 的工具鏈，出處 `docs/object_fps_pvp/plans/v8/pvp-baseline-report.zh-Hant.md`）。**不拉新映像、不改 Dockerfile**。開工時以 `docker image inspect` 核對 id，不符就停下報告。
- **掛載**：只掛主 checkout 的 `build/evidence/json-library-survey/`（`lib/`、`corpus/` 唯讀，`probe/` 唯讀，`results/` 可寫）；不掛 repo 原始碼與 home。
- **隔離**：編譯與執行時 `--network none`；資源限制沿用基線報告的 `--cpuset-cpus=8-15 --memory=4g`（記憶 warn-before-heavy-local-runs：本批的負載是編譯 6 個函式庫的小程式與 20 萬個字串，量級遠小於 km² 預覽）。
- **主機上做的**：下載與 SHA-256 核對（`curl`）、`gh api` 查維護狀態、比對結果（`python3 -I`）。主機上不編譯、不執行下載物。

### 5.2 平台覆蓋

- **linux-x64（容器內，gcc-14／libstdc++）**：判準 1～6 全部實測。這是 L1 平台之一、工具鏈與 L1 相同，但跑在 Docker Desktop 的 linuxkit VM 上（x86_64 Intel 主機），不是 GitHub runner；報告寫明。
- **windows-x64（MSVC STL）、macos-arm64（libc++）**：判準 3 的 3b、3c 與判準 5 的編譯記「確認できない（選項 C）」，不判通過。候選的官方 CI 平台以文件讀取、標明來源，不當作 GYO 的實測。
- 本機 macOS x86_64 原生不跑（使用者要求在 Docker 下；它也不是 L1 平台）。
- 判準 3 的兩條路（原文路、候選轉換路）只能判「linux-x64 上 0 件不一致」；三平台判定留給使用者決定之後要不要補（例：選定候選後在其遷移批的 L1 上驗）。這是 AGENTS §14 冒頭的範圍限制，後續批的事前宣告要照寫。

## 6. 證據、時間、停止條件

- **證據位置**：主 checkout `build/evidence/json-library-survey/`（本機 macOS x86_64 的主機磁碟；實測在容器內產出）：`downloads.txt`（URL、版本、SHA-256、大小）、`usage-grep-20261113.txt`、`probe/`（實測程式、生成與比對腳本、種子）、`results/linux-x64-docker/<候選>-<判準>.txt`、`container.txt`（映像 id、`docker run` 命令全文、gcc 版本）、`maintenance-20261113.json`（`gh api` 原始回應）。報告與 dev_log 只寫結論與判定用的數字，引用這些路徑。
- **件數的突き合わせ**：報告與 dev_log 開頭一行：候選 7 × 判準 6＝42 格，實測 36、文件 1、不評 5（判準 3、5 的三平台部分在選項 C 下另計「確認できない」）；另寫每個判準送出的向量數與收到的結果數（例：判準 1 每候選 318＋約 20 個，結果行數相同；3b 每平台 200,000＋約 40 個）。不一致時不下判定、先報告。
- **估時**：宣告 1.5 h（已用）；本機實測 5～7 h；不跑 CI（選 C）；報告、dev_log、審查與回應 3～4 h；合計 **9.5～12.5 h**。超過 1.5 倍（19 h）或單一問題累計 > 2 h 就停下報告（記憶 time-box；AGENTS §14 冒頭）。
- **停止條件**（AGENTS §14 冒頭）：同一判準的測法改 2 次仍無結論；任何候選的實測需要改 repo 的程式或建置；CI 需要改 master 的 `.github/`。
- **審查**：報告完成後，一次只讀複查，交使用者選：(a) **Opus 另一個代理／high**（架構師建議：使用者的臨時做法是 Fable 每批最多一次，留給計畫最終稿的檢查；與作者同模型，報告寫明「同模型、盲點可能重疊」）；(b) **Fable／high**（與作者不同模型；比主會話大，依 navigator 要使用者同意）。複查者拿到：本宣告、報告、`results/` 與 `downloads.txt`；任務是找沒驗到的判準格、與證據不符的判定、漏掉的限制。
- **上段／下段**：架構師判下段（第三方評估、只寫文件）；本宣告初稿判上段（結果會影響資料契約的解析規則與未決 1）。兩者的實作者都是 Opus 5.5／high，差別只在審查者（上一條），交使用者定。

## 7. 要使用者決定的事

1. **核准 §4 的下載清單**（來源、版本、大小、位置）。
2. ~~三平台的跑法~~：**已定 C（只在本機），實測放在 Docker 下（§5.1）**，2026-11-13。
3. **候選的增刪**：新增 jsoncons；glaze 只做文件判定（C++23）；不採 taoJSON。要不要改。
4. **判準 5 的「有人維護」定義**（未 archived 且 12 個月內 commit > 0；release 停滯另標風險）與判準 1 的「條件通過」分級，是否接受。
5. **審查者**：(a) Opus 另一代理（架構師建議）或 (b) Fable／high。

## 8. 範圍的限制

- 判準 3 只驗「讀」：Engine 寫出數字（例如資產目錄改寫）的格式不在判準內，判準 4 只記錄。
- 判準 1 的基準是 nlohmann 3.12.0 的實際行為，不是 RFC 8259 本身；兩者的差（例如 BOM）報告照實寫。
- 效能只在一台非 L1 機器量，只作參考。
- 候選的事實（旗標名、API）在本宣告寫「實測確認」的，都是查證前的預期；報告以實測與原始碼為準，不符時照實改寫。

# 合併前核准：JSON Module（`v8-arch-gyo2-json-module`）

件數の突き合わせ：一致（數字向量：送 210,050＝重生的六個檔的筆數常數＝比對 210,050，另最短寫出 50,000，L1 linux-x64／windows-x64 與本機各 260,050；JSONTestSuite 318＋GYO 補充 24＝判定 3 的件數總和 342；往返 200,000；情境 1,000 行；突變 107 killed／107）。

日期：2026-11-20。分支 `claude/gyo2-json-module-9cb753`（draft PR #252）。事前宣告：[json-module-declaration.zh-Hant.md](json-module-declaration.zh-Hant.md) 修訂 1（2026-11-20 動工前核准）。經過與證據：[dev_log](../../../dev_logs/2026_11_20_gyo2_json_module.zh-Hant.md)。Design Invariants：[json.zh-Hant.md](json.zh-Hant.md)。

**請你核准的**：第 2 節（判定 3 的更正，照 §14.4 寫明；你已在主會話回「這個照建議」，這裡補齊格式）、第 3 節 Design Invariants 的最終文字、第 4 節突變的數目超過基準、第 5 節和宣告不同的地方。核准後我把 draft 改成 ready，看 L1 三平台，你說「合併」才合併。

## 1. 判定的結果（宣告第 9 節）

| # | 結果 | 證據（主 checkout `build/evidence/gyo2-json-module/`） |
|---|---|---|
| 1 | 本機全量 ctest 120/120（macOS x86_64）；L1 三平台綠（run 44577566411，見本節末） | `ctest-final.log`、`l1-final-44577566411/` |
| 2 | 數字向量 0 不一致：L1 linux-x64（Python 3.12.3）、windows-x64（3.15.2）、macos-arm64（3.15.2）與本機（3.14.8）都重生出和調查相同的六個檔（SHA-256 一致），260,050 個 0 不一致，數字串和 Python `repr` 也完全一致；結構化 golden 與往返 200,000 三平台綠 | `l1-final-44577566411/` 的 `tests.xml` |
| 3 | **更正後成立**（第 2 節）：`y_` 93 收＋兩件 `y_object_duplicated_key*` 回 `RepeatedKey`；`n_` 188 全拒；`i_` 拒 31（字串、代理、編碼、BOM 24＋溢位 5＋下溢 2）收 4（超出 64 位元的整數 3＋500 層 1）；GYO 補充 24 件和調查一致 | `judgement3/results.txt` |
| 4 | 突變 107/107 killed（Fable／high 檢查補過後） | `mutants-full-final.log` |
| 5 | J-1 掃描：正例 yyjson 含入 3、fast_float 含入 1、yyjson 名字 69、fast_float 名字 1；其他位置 0；自我測試 22 件；`gyo_expect_links` 綠 | ctest `json.adapter_guard*` |
| 6 | 沒有觸發停止條件：5.3 MB 解析 54.7 ms（上限 150）、16 MiB 168.8 ms；和 yyjson 單獨解析的比 2.4～2.8 倍（上限 10）；一筆 473 位元組紀錄的寫出中位數 1.24 µs（上限 5）；本機 i9-9980HK，一次 | `bench/results.txt` |
| 7 | 件數一致（文首） | 同上 |

**L1**：第 1 步的 run 44556667124 三平台綠。run 44562576673 的 windows-x64 失敗：MSVC 的 doctest 印 `std::string_view` 要 `<ostream>`（測試檔的問題，照 `OrchestrationTestSupport.hpp:16` 的先例補上）。run 44564613610 linux、windows 綠；macos-arm64 兩次被 GitHub 取消（「The job was not acquired by Runner of type hosted even after multiple attempts」，0 個步驟，runner 容量）。run 44574178460（c8ba3a39）的 windows-x64 又因同一原因失敗（第 7 步加的 `WriteAllocationTests.cpp` 比較 `string_view`），macOS 再次被取消；補 `<ostream>` 後 **run 44577566411（7314c282）三平台綠**：linux-x64 120 件、windows-x64 123 件（只有既有的 `os.console_close` 是 skipped：runner 收不到關閉主控台的事件，和本批無關）、macos-arm64 120 件；三平台的 json 測試各 7 件、數字向量各 260,050 個 0 不一致（macOS 以 Python 3.15.2 重生，SHA-256 一致）。證據 `l1-final-44577566411/`。

## 2. 判定 3 的更正（AGENTS §14.4 的格式）

1. **守的性質**：嚴格解析——RFC 8259 的合法文字都收、不合法的都拒，GYO 的規則（J-2）另外拒重複鍵與超出範圍的數字。
2. **緩的面**：沒有。
3. **締める面**：`y_object_duplicated_key.json`、`y_object_duplicated_key_and_value.json` 兩件，以前（宣告原文）要「收」，更正後要「回 `RepeatedKey`」。
4. **守的性質是否由別的判定守著**：重複鍵由 `ReadTests.cpp` 的「a key repeated in one object」「repeated keys are found in large objects」守（各有突變 `js-repeated-*`）；其他 93 件 `y_` 的「全收」不變。
5. **根據**：宣告第 7 節 J-2（重複鍵是錯誤，master 時代使用者已定的嚴格語義）；調查報告 §2.1（判準 1 的基準也把重複鍵排除）。判定是結果出來後才發現寫錯的，所以停下回報、等你決定（§14.1）；你的決定：主會話 2026-11-20「這個照建議」，HANDOFF `v8-arch-gyo2-json-criterion3-corrected`。實作與測試沒有改。

## 3. Design Invariants 的最終文字

照 [json.zh-Hant.md](json.zh-Hant.md)。和宣告第 7 節的差：

- **J-1**：adapter 的位置收窄成 `src/adapters/yyjson/`、`src/adapters/fast_float/`（你的決定）；守衛除了含入，再查 adapter 目錄外的 `yyjson_`／`fast_float::` 名字（J-1 寫的是「標頭、型別、呼叫」，只看含入抓不到從 adapter 的內部標頭拿到型別的寫法——Fable 的檢查指出）；連結規則擴到 `*.cmake`、`build/cmake/`、根目錄的 `CMakeLists.txt`，也抓 `yyjson_SOURCE_DIR`／`fast_float_SOURCE_DIR`。都只緊不緩。
- **J-5**：補一句實作的行為——超過 32 層時若層的狀態配置失敗，之後的 Begin／End 只計數、呼叫者照常寫完就由 `Finish` 回 `OutOfMemory`（D2 的修正）。
- 其他各條照宣告。

## 4. 突變的數目（基準 8：一批加的數超過「變的條文數 × 5」時說明）

本批新增 9 條 Design Invariants，基準是 45，實際 107（js 38、jw 47、jg 19、jb 3）。基準在 PR #251（還沒合併到 `gyo2`），這裡照它說明：

- 第 1～6 步各步寫的是 48 個。第 7 步的 Fable／high 敵對式檢查找到清單外 38 個改壞實作、6 個改壞守衛而現有測試全過的位置，以及 3 個實作缺陷。照基準 6（「捕まえる Test がないなら、Test とミュータントを加える」），每個存活的位置補一個能抓到它的測試與一個突變；每個加的突變都寫了它抓的、清單裡別的突變抓不到的錯誤（`what` 欄）。
- 數目多的三塊：UTF-8 的接受集合 11 個（每個前導位元組的範圍是獨立的決定：兩位元組、E0、E1～EC、ED、EE～EF、F0、F1～F3、F4、過長、代理、超出 U+10FFFF；靠一個 346,200 列的窮舉測試，每個突變的預期失敗是它特有的第一個不一致序列）；配置失敗 13 個（`Writer` 每個會配置的呼叫各一、`WriteValue`、讀取錯誤、`Parse`、yyjson 配置器；5.5 要求每個公開函式的邊界都攔下）；守衛 19 個（基準 4：每條規則三件＋每個獨立分支一個；5 條規則、3 種 CMake 位置、2 種根目錄）。
- 沒加的（description 列理由）：空文字的預檢查、`BeforeValue`／`Key`／`EndContainer` 的 finished 檢查、`EndContainer` 的深度檢查、`NumberText` 的 catch（都等價）；移動建構子不清空來源（未定義行為）；`isfinite`→`isnan`（只靠斷言當掉）；沒有測試讀的訊息文字；J-9 的時間線性（沒有不靠計時的穩定測試，由審查與尺寸案守）；守衛的讀法（註解與字串的遮蔽，基準 4：觀察到漏看才加）。

## 5. 和宣告不同的地方

| # | 宣告 | 實際 | 理由 |
|---|---|---|---|
| 1 | J-1：`engine/json/src/` | `src/adapters/<組件>/` | 你的決定（2026-11-20，`backend` 太含糊，「用第一個 adapters」） |
| 2 | 判定 3：`y_` 95 件全收 | 93 收＋2 件 `RepeatedKey` | 第 2 節 |
| 3 | J-1 守衛：含入與連結 | 加名字規則、`.cmake`／`build/cmake`／根目錄、`*_SOURCE_DIR` | Fable 的檢查；只緊不緩（第 3 節） |
| 4 | 5.5「不讓 `bad_alloc` 從公開介面漏出」 | 原實作有 3 處漏（D1～D3），已照 §14.3 先 Red（`d1-d3-red.txt`）後修 | 實作缺陷，Contract 不變 |
| 5 | 8.3「第 1 步的 L1 放 `yyjson_write_number` 的小探測」 | 小 golden 55 個值（宣告寫「前 100 個」） | 用編譯器直接讀的字面當輸入，避開計算；三平台綠 |
| 6 | 測試先行（Red） | 第 3、5 步的 Red 是「還沒有實作」的連結失敗，不是斷言失敗 | 照實記；第 2、4 步與 D1～D3 是斷言失敗 |
| 7 | — | 寫檔工具把測試裡 4 處 `\uXXXX` 解成字元，Fable 發現後改回 | dev_log「測試字面被改的事故」 |
| 8 | 8.6「上限 45」 | 107 | 第 4 節 |
| 9 | 時間估計：實作段 22～34 h | 約 13 h（推定，含第 7 步的補強） | 沒有超過 |

## 6. 範囲の限界

- 「換函式庫」要到第二個實作出現才算驗證過；本批只以 J-1、J-4 的守衛與審查支撐。
- JSONTestSuite 只在本機（macOS x86_64）跑；L1 三平台跑全部數字向量、固定表與寫出端的測試。
- ThreadSanitizer 不在 L1；多執行緒的測試只證明「沒當掉、讀到的值對」。
- 效能只在本機量一次，是停止條件，不是受理的閘門。
- `OutOfMemory` 只以測試注入的配置失敗驗證。
- J-9 的時間線性沒有突變守（第 4 節）；J-1 守衛的讀法（巨集含入、`.mm`、字串字面裡的 `/*`）在罕見寫法下會漏看（Fable 的檢查列了，屬基準 4 的「觀察到才加」）。
- 可編輯文件與縮排排版沒有做。

## 7. 交主會話的（宣告第 15 節與本批發現的）

- Kernel 指紋報告手寫 JSON（`engine/kernel/src/Fingerprint.cpp:231`、`:513-521`）：改用 JSON Module 要動 Kernel 的依賴（Architecture Delta）。
- master 七份資料契約的驗證規則收緊（下溢與溢位是錯誤、BOM 與「`123`＋NUL」拒、超出 64 位元的整數讀整數欄是錯誤）：各契約回到 `gyo2` 的批要在宣告寫明。
- 日誌與 trace 那一批要定 NaN／無限大在紀錄裡的表示法（第 12 節第 3 題的結論：JSON Module 回錯誤）。
- 打包時附 yyjson、fast_float 的 MIT 告示。
- 寫檔工具會把合法的 `\uXXXX` 解成字元（本批 4 處）：別的卡寫含跳脫的測試時要注意，可以考慮記進記憶或交風紀委員。

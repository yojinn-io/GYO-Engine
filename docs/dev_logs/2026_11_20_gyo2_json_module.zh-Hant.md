# JSON Module（`v8-arch-gyo2-json-module`）

日期：2026-11-19～20。分支 `claude/gyo2-json-module-9cb753`（從 `gyo2` 7673a419），draft PR #252。**狀態：實作完成，等合併前核准**（[合併前核准頁](../architecture/target/v2/json-module-merge-approval.zh-Hant.md)）。

這一批由任務卡做（GYO2 主會話開出；使用者 2026-11-19「開始 JSON Module，建一張任務卡」）。事前宣告：[json-module-declaration.zh-Hant.md](../architecture/target/v2/json-module-declaration.zh-Hant.md) 修訂 1；使用者 2026-11-20 在卡片「1、2、3 照建議，4 同意，可以下載」（動工前核准）。修訂 0 送 Fable／high 敵對式檢查（必改 9、建議改 12、小問題 9），修訂 1 照它改。Design Invariants：[json.zh-Hant.md](../architecture/target/v2/json.zh-Hant.md)。

## 做了什麼

| 步 | 提交 | 內容 |
|---|---|---|
| 設計段 | d142d8e8、ad9ab318 | 事前宣告修訂 0、修訂 1（BigMap 加進予見的根據、讀取只給依序走訪、守衛收回 Engine 側、數字向量在 L1 重生） |
| 第 1 步 | 0ec437a9 | `third_party/yyjson`（0.13.0，commit 64475360）、`third_party/fast_float`（v8.3.3，commit dbeabeeb）以 FetchContent 釘 commit SHA；`gyo_json_component_tests` 固定組件本身的行為（fast_float 對溢位回 `result_out_of_range`＋inf、對下溢回 `result_out_of_range`＋0、`0e999999` 不算下溢；yyjson 的嚴格性、非 insitu 複製輸入、重複成員保留；`yyjson_write_number` 的格式與小 golden） |
| 第 2 步 | 63e36105 | Module 登記檔一列、`gyo_json`、`JsonError`、`gyo_expect_links`；先確認 `cmake.UnitRegistry` 抓到缺列（Red） |
| （決定） | f33a5303 | adapter 放 `src/adapters/<組件>/`（使用者：不要 backend、「用第一個 adapters」）；J-1 收窄 |
| 第 3 步 | 82e894ba | `Parse`／`Document`／`Value`／`ObjectView`／`ArrayView`（yyjson 唯讀 DOM 上的 view）；重複鍵（小物件兩兩比、大物件雜湊、不遞迴）；行與欄（用原始輸入數）與 JSON Pointer；yyjson 配置失敗的注入 |
| 第 4 步 | fd904491 | 數字：原文路、解析時驗證、`AsInt64`／`AsUint64`／`AsDouble`／`IsIntegerLiteral`；`gen_numbers.py` 在 ctest 裡重生調查的全部向量並以 SHA-256 比對 |
| 第 5 步 | 2ed00a6d | `Writer`、`WriteValue`；double 用 `yyjson_write_number`；非有限數由 `Finish` 回 `NotRepresentable`；配置計數；往返 200,000、最短位數與 Python `repr` 比對 50,000、結構化 golden；MSVC 的 `<ostream>` |
| 第 6 步 | c554e4ee | 情境測試 F1（trace 1,000 行）、F2（資源目錄）、F6（8 執行緒 region）；通過後立 J-1 的掃描守衛 |
| 第 7 步 | b17218a7、c8ba3a39 | 判定 3 的更正；Design Invariants 頁；突變清單的 Fable／high 敵對式檢查與補強（D1～D3 修正、測試、守衛、107 個突變） |

## 驗證與經過

- **全量 ctest**：本機（macOS x86_64）120/120（`ctest-final.log`）。
- **突變**：本批清單 107/107 killed（`mutants-full-final.log`）。每步只跑相關的（D184），第 7 步跑完整一次。加的數與理由見合併前核准頁第 4 節（超過「條文數 × 5」）。
- **數字向量**：L1 linux-x64（Python 3.12.3）與 windows-x64（Python 3.15.2）都重生出和調查相同的六個檔（SHA-256 一致），讀取 210,050、最短寫出 50,000，共 260,050，0 不一致（run 44564613610）。本機（Python 3.14.8）同。macos-arm64：第 1 步的 run 44556667124 綠；之後三次被 GitHub 取消（runner 沒拿到，「capacity constraints」，0 個步驟）。最後 **run 44577566411（7314c282）三平台綠**（macOS 以 Python 3.15.2 重生，SHA-256 一致；windows 只有既有的 `os.console_close` skipped）。windows 失敗過兩次，都是 MSVC 的 doctest 印 `string_view` 缺 `<ostream>`（44562576673、44574178460）。
- **判定 3**（本機 JSONTestSuite 1ef36fa0）：`y_` 93 收＋兩件 `y_object_duplicated_key*` 回 `RepeatedKey`、`n_` 188 全拒、`i_` 拒 31 收 4、GYO 補充 24 件和調查一致。宣告原寫「`y_` 95 件全收」和 J-2 矛盾，結果出來後判定不成立，停下回報；使用者「這個照建議」更正（HANDOFF `v8-arch-gyo2-json-criterion3-corrected`）。
- **判定 6**（本機，i9-9980HK）：5.3 MB 解析 54.7 ms（上限 150）、16 MiB 168.8 ms；和 yyjson 單獨解析的比 2.4～2.8 倍（上限 10）；一筆 473 位元組紀錄的寫出中位數 1.24 µs（上限 5）。沒有觸發停止條件。
- **Red 的形式**：第 2 步是斷言失敗（`cmake.UnitRegistry`）；第 3、5 步是「還沒有實作」的連結失敗，不是斷言失敗；第 4 步是斷言失敗（`step4-red.txt`）；D1～D3 是斷言失敗（`d1-d3-red.txt`）。
- **測試字面被改的事故**：檢查者發現 `ReadTests.cpp` 的兩行「跳脫後相同的鍵」一模一樣。原因是寫檔工具把 `a` 這類合法的 `\uXXXX` 解成了字元（孤立代理與 `\u0000` 沒被解）。共 4 處，第 7 步以 Python 拼出反斜線改回；其他檔沒有非 ASCII 殘留。
- **Fable／high 敵對式檢查（突變清單）**：清單 48 個在他的副本全殺，但清單外 38 個實作突變、6 個守衛突變存活，3 處實作原樣不符 Contract（D1 `WriteValue` 的配置失敗漏出、D2 超過 32 層的配置失敗讓 `Finish` 走不到、D3 讀取錯誤組字串時的配置失敗漏出），守衛有 14 種寫法掃不到。處理：D1～D3 照 §14.3 先 Red 後修；存活的各補測試與突變；守衛加名字規則、fast_float 的路徑前綴、`.cmake`／`build/cmake`／根目錄的 CMake 與 `*_SOURCE_DIR`；expect 收窄成只有那個突變會造成的失敗。等價、靠當掉、或只是讀法的突變不加（清單 description 列理由）。報告：`declaration-r0-adversarial-check.md`（宣告的）、`mutation-check-fable/`（清單的）。

## 下一步

- 使用者的合併前核准 → 開 PR（draft 改 ready）→ L1 三平台 → 使用者說「合併」才 `gh pr merge --merge`。
- 交主會話（宣告第 15 節）：Kernel 指紋報告手寫 JSON；master 七份資料契約的驗證規則收緊要在各契約回到 `gyo2` 的批寫明；打包時附授權告示。日誌與 trace 那一批要定 NaN 的表示法（第 12 節第 3 題）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-json-module/`（git 不追蹤，本機 macOS x86_64）：`reference-survey.md`、`declaration-r0-adversarial-check.md`、`download/`（下載 tarball 與 SHA-256，內容和 FetchContent 取得的逐位元組相同）、`l1-step1-44556667124/`、`l1-step4-44562576673/`（windows 的 `<ostream>` 失敗）、`l1-step5-44564613610/`、`l1-final-44574178460/`、`l1-final-44577566411/`、`step4-red.txt`、`d1-d3-red.txt`、`judgement3/`、`bench/`、`mutation-check-fable/`、`mutants-full-run1.log`、`mutants-full-final.log`、`ctest-final.log`。

# 架構 v8：v8-arch-j17-scanner（J-17 掃描器：(c) 區來源檔的文字守衛、清單契約 v1、pvp 基線）

日期：2026-11-12。Owner：架構（J-17 掃描器線，架構師開出的並行線）。計畫與記錄器：[交接卡 j17-scanner.zh-Hant.md](../architecture/plans/kernel/j17-scanner.zh-Hant.md)（判定門檻與基線清單在卡上，架構師 2026-11-12 核定）。PR：待開（本 dev_log 隨 PR 更新）。
- 執行：J-17 線主會話（Fable）；規則與遮罩 xhigh、其餘 high；審查者 Opus／high（只讀，找誤報與漏報）。
- 性質：新增守衛 Script＋ctest 測試（AGENTS §5：手動→Script，本線停在 Script）；文件（契約草案）。不改 Engine、不改 pvp、不改 preset 與 CI workflow。

**件数の突き合わせ：一致**（兩份基線各三條，見「驗證」）。

## 起因

spec J-17 要求對 Kernel 匯出的「(c) 區 System 來源檔清單」做文字掃描；它是之後每批遷移（pvp「位置＝區」整理、Match／Client host 遷移）的驗收工具，而規則只依賴清單格式、不依賴 kernel API，所以先於 kernel 本體做。-audit 的一次性腳本 `audit_scan.py` 漏了三類（第三方全域 context、匿名命名空間非 const 變數、時鐘別名），且不是守衛。

## 做了什麼

- **開始前**：補齊卡「判定」節的數字門檻、寫基線清單 v1、寫清單格式草案 `docs/architecture/target/zone-c-sources.zh-Hant.md`，列六題送架構師；架構師同日核定（代號改 `ZS1～ZS7`、加 ZS7 雜湊、效果類標頭進 ZS1、清單加 `owner_root`／`include_roots`、註解一律遮、P9 五檔改第二份候選清單）。核定前未寫程式。
- **掃描器** `tests/common/zone_scan/zone_scan.py`：讀清單（`gyo.zone-c-sources` v1；未知版本／格式／欄、路徑不合規、檔不存在＝結束碼 2 一行一錯），沿引號 include 在 `owner_root` 之下傳遞展開（先含入檔目錄、再 `include_roots`；角括號不展開；每檔一次；報告附含入鏈與未解析 include），遮掉註解、字串、字元字面值後掃七條規則；候選（ZS5）不算失敗；`--no-fail` 給基線。
- **規則資料** `zone_scan_rules.json`：ZS1 標頭名（含效果類）、ZS3／ZS4／ZS6／ZS7 的名稱正規式；ZS2、ZS5 是結構判法在腳本裡。
- **ZS2 的判法**（xhigh 段）：`static`／`thread_local` 宣告照 entity_store W4 的做法再加 const 判定（const 落在最後一個 `*`／`&` 之後才算不可變；`<…>` 先剝掉）；命名空間層級的變數以大括號範圍機（namespace／class／other 三種範圍）切語句，`(` 內是「參數列」還是「建構引數」用逐項判法（型別形狀＝參數；數字、字面值、運算式＝引數）；`struct S {…} s;` 的尾端宣告子也抓。
- **自測** `test_zone_scan.py`：正例 9、反例 7、ZS1～ZS7 各自的邊界、遮罩邊界、展開、契約，共 60 條。
- **突變** `mutations.json`＋`run_mutations.py`：16 條（ZS1×2、ZS2×3、ZS3～ZS7 各 1、遮罩×3、版本檢查、展開範圍、排序檢查），killed 只算「自測失敗且輸出含宣告的測試名」。
- **基線** `baseline/object_fps_pvp.zone-c-sources.json`（確認的 (c) 12 檔）與 `object_fps_pvp.zone-c-candidates.json`（P9 候選 5 檔）；`run_baseline.py` 跑全部清單、只報告；ctest `zone_scan.baseline` 只在契約錯誤時失敗。
- **接法**：`tests/common/zone_scan/CMakeLists.txt`（`zone_scan.selftest`、`zone_scan.baseline`，label `cpu`），`tests/common/CMakeLists.txt` 加一行 `add_subdirectory(zone_scan)`。
- 文件：卡（判定、基線清單、問題與處置、現況）、target 草案、kernel README（批次樹、dev_log 一覽）、HANDOFF 現況一行。

## 驗證與經緯

證據（主 checkout，git 管理外）：`/Users/karasu/Code/Source/GYO-Engine/build/evidence/zone-scan/`：`selftest-2026-11-12.txt`、`mutations-2026-11-12.json`／`-stdout.txt`、`baseline-2026-11-12/{object_fps_pvp.zone-c-sources.json, object_fps_pvp.zone-c-candidates.json, baseline-stdout.txt, independent-counts.txt}`。

| 判定 | 門檻（卡） | 結果 |
|---|---|---|
| 1 自測 | 正例 9 各只被指定規則抓到且行號正確；反例 7 零違規零候選；unittest 全綠 | **成立**：60 條全綠（9＋7 逐一對應，其餘 44 條為各規則邊界、遮罩、展開、契約） |
| 2 突變 | ≥9 條全殺，killed 只算預期失敗 | **成立**：16/16 killed |
| 3 共通層零修改 | 既有檔只有 `tests/common/CMakeLists.txt` +1 行；`apps/object_fps_pvp` 0 變更 | **成立**：`git diff --name-status origin/master...HEAD` 既有檔變更＝1（`tests/common/CMakeLists.txt`，+1／−0）；`apps/` 0 |
| 4 基線 | 兩份報告、件数の突き合わせ三條各算 | **成立**（下表） |
| 5 三平台 | L1 三平台 `zone_scan.selftest`、`zone_scan.baseline` passed | 待 L1（PR 開後補） |

基線（commit `2b4618f9` 的 pvp；掃描器 `80c61a23`）：

| 清單 | N 清單 | M 掃描（展開） | L 行 | H 違規 | C 候選 | 每規則 |
|---|---|---|---|---|---|---|
| 確認的 (c) 12 檔 | 12 | 39（27） | 4,908 | 26 | 0 | ZS1 19、ZS2 3、ZS4 2、ZS7 2 |
| 候選 (c) 5 檔 | 5 | 19（14） | 2,312 | 9 | 0 | ZS1 6、ZS7 3 |

件数の突き合わせ（兩份各三條，`independent-counts.txt`）：(a) H＝Σ每規則＝違規行數：一致；(b) M＝N＋展開標頭數：一致；(c) 對同一組 M 檔用 `git grep -nP` 獨立計 ZS1 include 與 ZS4 別名（正規式先以 `Combat.hpp:5`、`ClientConnection.cpp` 做正例控制）：確認 19／19、2／2，候選 6／6、0／0，逐行集合相等。

基線的內容（給產品線 -zones 批當起點；命中數不是門檻）：ZS1 以 `<filesystem>`（7）、`<chrono>`（5）為主，另 `<atomic>`、`<mutex>`、`<condition_variable>`、`<thread>`、`<cstdio>`；spec J-17 點名的 `Combat.hpp:5`、`Movement.hpp:8` 都在內。ZS2 三處全是 `MovementDiag.hpp:188`、`MovementTrace.hpp:101-102` 的 inline 全域（spec G.4 第 3 條）。ZS4 兩處：`MovementTrace.hpp:59`、`PredictionElapsedTime.hpp:12` 的 `steady_clock`。ZS7：`AnimationSetDefinition.hpp` 的 `unordered_map`（2）、`PlayerPose.cpp:17` 的 `unordered_set`。候選清單的命中幾乎都來自共用標頭（`Combat.hpp`、`CharacterCollision.hpp`、`AnimationSetDefinition.hpp`）。ZS5 候選 0。

經緯（照 §14 記錄，不以後來的成功覆蓋）：

1. 自測第一輪 58 條 3 失敗：一條是測試寫錯（`const char* s` 放在檔案層級本來就是可變全域，掃描器對）；兩條是掃描器缺陷——預設引數為字串字面值的函式宣告被讀成變數（遮罩後 `= ` 後面空了）、`extern template class X<int>;` 被讀成變數。修後 58 綠。
2. 突變第一輪 13/16：兩條遮罩突變回報的是「行註解」突變的失敗（wrong_failure）、一條展開突變 survived。原因一：Python 以「大小＋mtime 秒」判 pyc 有效，`if X`→`if False and X` 長度相同、同一秒寫回，跑到上一條突變的 bytecode；修法 `-B`＋每次清 `__pycache__`（記進記憶 `python-mutation-pycache`）。原因二：展開範圍檢查是清單驗證之後的多餘防線（`include_roots` 已限制在 owner 下），對整合測試是等價突變；補一條直接呼叫解析函式的單元測試後可殺。第二輪 16/16。
3. 基線第一輪 ZS2 命中 20＋8，逐行讀原文發現 12 處誤報：`enum class X {`（`class` 被數成識別字）、`const T &name` 無空格的參數（名稱前要求空白）。修正並補測試後 ZS2 剩 3 處、全為真；自測 60、突變重跑 16/16、基線重跑、獨立計數重做。這是看結果後**修實作**、不是改判定；判定 4 的門檻（突き合わせ）沒變。
4. 審查（Opus／high，只讀，找誤報與漏報）：（待補）
5. 估時：卡估 7.5 小時；實際（待補）。

## 範囲の限界

- 靜態文字掃描（卡原文）：執行期繞路、巨集與模板間接產生的呼叫、`using` 改名的別名掃不到。
- ZS2：`Foo f(x);`（單一識別字引數）照 C++ 讀成函式宣告，掃不到；命名空間層級的巨集呼叫 `GYO_X(a, b);` 以引數形狀判，可能誤報或漏報（pvp 基線無此例）。
- ZS5 只是三個啟發式（存進成員的捕獲 lambda、`std::function` 成員、尾底線命名的裸指標／參考成員）；pvp 基線 0 候選，可能是啟發式太窄，產品線用時再調。
- 基線清單是手寫的（v1）；Kernel G-5 匯出落地後換來源，格式不變。
- 判定 5（三平台）待 L1。

## 架構報告

- **Architecture Delta**：無新 Delta。新目錄 `tests/common/zone_scan/`（卡 §3 指定；不進 `engine/`、`tools/`）；新契約草案 `target/zone-c-sources.zh-Hant.md`（架構師核定，狀態仍 target：生產端 Kernel G-5 未實作）。
- **新 Dependency**：`zone_scan.baseline` 讀 `apps/object_fps_pvp/` 的檔（經清單檔，不經 CMake target）。**觀測到的壓力（AGENTS §7／§9）**：兩份清單檔住在共通目錄 `tests/common/zone_scan/baseline/`，刪除 pvp 時要連帶刪它們，等於「刪 Product 需要改共通層」；spec J.4 也把 J-17 的產品端掃描列為「由 owner 持有」。本批照卡放共通目錄（pvp 零變更是本批的硬規則）；**建議**產品線 -zones 批把清單搬到 `tests/object_fps_pvp/`、由產品 CMake 註冊基線測試，共通層只留掃描器與自測。交架構師決定。
- **Data Contract**：`gyo.zone-c-sources` v1（新；target）。
- **Code Smell**：無新增；基線列出的 pvp 違規屬 spec G.4 已知（G.4 第 3 條的全域診斷入口、`Combat.hpp`／`Movement.hpp` 的 `<chrono>`）。

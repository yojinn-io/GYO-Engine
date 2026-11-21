# 合併前核准：trace（`v8-arch-gyo2-trace`）

件數の突き合わせ：該当なし（本批的判定沒有送出與收到的件數；trace 的紀錄經日誌的傳輸，日誌的件數守恆由日誌批的判定 3 守）。

日期：2026-11-21。分支 `claude/gyo2-trace`（draft PR #258，base `gyo2` bb2497b0）。事前宣告：[trace-declaration.zh-Hant.md](trace-declaration.zh-Hant.md)（修訂 1，2026-11-21 動工前核准「照建議，全部核准」）。Design Invariants：[trace.zh-Hant.md](trace.zh-Hant.md)。經過與證據：[dev_log](../../../dev_logs/2026_11_21_gyo2_trace.zh-Hant.md)。

**核准紀錄**（2026-11-21，日誌卡）：使用者原話「照建議，全部核准，合併」。範圍：下面第 2～5 節全部照卡片的建議核准，包括第 5 節「超過 64 層的標記丟掉」先只記在 TR-4、碰到再改；L1 三平台通過（第 1 節）之後合併 PR #258。

**請你核准的**：第 2 節（Design Invariants 的最終文字，含動到的別的 Component 的條文）、第 3 節（突變的數目）、第 4 節（和宣告不同的地方）、第 5 節（沒做、記下的事）。核准後我把 draft 改成 ready，看 L1 三平台；你說「合併」才合併。

## 1. 判定的結果（宣告第 10 節）

| # | 結果 | 證據 |
|---|---|---|
| 1 | 本機 macOS x86_64 全量 ctest 156/156（core preset，審查修正後）。L1：linux-x64、windows-x64、macos-arm64 全部通過（2038602d，手動 dispatch run 44875051388） | 本機 ctest（`trace-review-ctest.txt`）；GitHub Actions |
| 2 | 情境 S4-1：三段（`RunFrame`、`Main@Match`、`MoveRacer@Match`）父子相連、System 段有 `error`、路徑紀錄一筆；S4-2：例外展開的段帶 `exception`、最深那段寫路徑一筆；S4-3：斷言時日誌那一筆 `Fatal` 帶 `trace_path`、恰好一筆 `Fatal` | `scenario.segment4` |
| 3 | 單元案：巢狀、例外、提前結束、延後開始、跳過的步不開、重裝（世代）、不平衡、跨執行緒、64 層以上、Close 之後、段開著時 Logger 卸下：每段恰好一筆、`parent_id` 對 | `gyo_base_tests`、`trace.module`、Kernel 與編排層的 trace 案 |
| 4 | 情境 S4-4：trace 開關，`RacerState`、`LobbyFrames` 逐位元相同 | `scenario.segment4` |
| 5 | 紀錄經日誌寫出，欄位照 TR-5；沒人收時沒有紀錄、不讀時鐘 | `trace.module` |
| 6 | 突變 trace 62/62、logging 的 trace 相關 5/5 killed（Fable 5.2 的敵對式檢查與審查之後，完整跑一次；那一次 61/62，`tr-slot-childunwound-stale` 被審查的新規則在測試的順序下蓋掉，調換測試兩段的順序後 killed，見 dev_log） | 主 checkout `build/evidence/gyo2-logging/trace-mutations-run2.txt`（審查前 56/56：`trace-mutations-run1.txt`） |
| 7 | 只量（Intel i9-9980HK、Release，一次）：一段，沒裝 43 ns、裝了沒人收 83 ns、進佇列 2.4 µs；一幀 20 個 System，trace 關 6.7 µs、沒人收 6.7 µs、全收 78 µs | `trace-judgement7-measure.txt` |
| 8 | 在 repo 根跑測試之後，根目錄沒有 `logging-test-files/`、`scenario-segment3-files/` | 手動確認（第 4 步） |

**本批找到並修好的缺陷**（Fable 5.2 對突變清單的敵對式檢查，`trace-mutation-adversarial-fable.md`）：
- F1：同步點、hook 清空輪裡被拒絕的 hook 已經標過 origin，同步點又標一次，一個錯誤兩筆路徑。
- F2：DropOldFirst 的替換在編排層自己的步驟失敗時，被當成「經過」，一筆路徑都沒有。
- 兩個都是實作沒照核准的 D4 (i)；照 AGENTS §14.3 先 Red（`trace-f1-f2-red.txt`）再修，契約不變。

**審查找到並修好的**（Fable 5.2／high 的 code review，`trace-review-fable.md`；全部先 Red 再修，`trace-review-red.txt`）：
- 必改 1：F1 沒修完——trigger 的兩個出口（`TriggerFromHost`、System 當下觸發）在清空輪的 hook 已標過 origin 時又標一次。修法同 F1。
- 建議 2：子段的例外被接住後，父段自己的例外沒有路徑（旗標設了不清）。父段開始新的子段或被標記時清掉；清不到的情形寫進 TR-3。
- 建議 3：category 超過 64 位元組時斷言（追問題的機制把行程停下）。改成截斷，寫進 TR-5。
- 建議 4：段開著時 Logger 卸下，結束時段紀錄寫到標準錯誤。結束時再問一次有沒有人收。
- 建議 5：給的欄位已有 `trace_path` 時，致命紀錄同一個鍵寫兩次（JSON 讀不回，違反 LG-5）。已有就不加。
- 小問題 7～12：寫進條文或第 4、5 節（下面）；第 10 項（TLS 的初始化檢查）改成 `constinit`，沒有行為變化。

## 2. Design Invariants 的最終文字

照 [trace.zh-Hant.md](trace.zh-Hant.md)（TR-1～TR-7、RC-T1～RC-T5）。和宣告第 8 節的差：TR-3 寫明 origin 的判法（Kernel：巢狀或 hook 留下的錯誤往外經過時不是；編排層：`ScheduleFailed`、`ReplaceScheduleFailed`、`WorldInterrupted`、`WorldMissing` 是經過，其餘是 origin——F2 的修正）；TR-4 寫明 64 層與 Close 之後；TR-5 寫明錯誤文字最多 256 位元組、紀錄和日誌共用有上限的佇列。審查之後再加：TR-3 子段的例外被接住後何時清旗標、清不到的情形；TR-4 超過 64 層的標記丟掉、段在別的執行緒結束不約定；TR-5 category 最多 64 位元組（截斷）、結束時再問一次有沒有人收；TR-7 安裝與卸下在別的執行緒發段之外；RC-T1 加段在別的執行緒結束。

**動到的別的 Component 的條文**（宣告第 7 節，你在動工前核准時同意）：
- 日誌 LG-2（致命紀錄帶 `trace_path`，超過 32 個欄位時不加；審查之後改寫：條件是「路徑非空」，照實作，原文「trace 有接收者時」不準；自己已經有 `trace_path` 時不加）、LG-5（`span.` 的紀錄是一般紀錄）、LG-8（`Logging::Installed()`）。
- Kernel DI-KS 15：呼叫 Base 的 trace 門面不是清單上的職責，清單不加項。
- `target/error-handling.md` DI 2、DI 4：標在 trace 段上的錯誤不是日誌行，不算處理第二次。
- `kernel_symbols`：准許清單多 `Engine::Base::Detail::traceReceiver`（§14.4 的 1～5 在宣告第 9 節；Kernel 函式庫裡確實出現這個符號，被認成外來的）。

## 3. 突變的數目（README 基準 8）

Design Invariants 7 條 × 5 ＝ 35；trace 的清單 62，另 logging 加 5（`lg-installed-always`、`lg-fatal-no-trace-path`、`lg-fatal-fields-lost`、`lg-fatal-limit-off-by-one`、`lg-fatal-path-twice`）。

- 第 1～5 步立了 31 個：門面 4、trace Module 14、Kernel 6、編排層 3、守衛 4。
- Fable 5.2 的敵對式檢查找到清單外 19 處測試缺口與 2 個缺陷：條文或判定寫明的都補了測試與突變（加 25：缺陷 2、Module 15、Kernel 2、編排層 5、情境 1、修正 `expect` 與 `what` 的不算）；「不建議加」的 4 個、「等價」的 5 個不加。
- 審查加 7（trace 6、logging 1）：每一個抓一處修正（兩個 trigger 出口、新子段清旗標、標記清旗標、category 截斷、結束時再問、`trace_path` 不重複），舊的突變都抓不到（§14.5）。另有 4 個舊突變的 `find` 跟著修正改寫（`tr-stack-shared`、`tr-kernel-trigger-through`、`lg-fatal-no-trace-path`、`lg-fatal-limit-off-by-one`），不算新的。
- 多的主因：TR-3（origin 與經過，每一層各有一個判法）與 TR-4（堆疊的生命期：世代、不平衡、槽的重用、64 層、跨執行緒、Close），每一個都是會獨立出錯的決定。

## 4. 和宣告不同的地方

| # | 宣告 | 實際 | 理由 |
|---|---|---|---|
| 1 | D8：`Trace::Install` 回 `Result` | 回 `TraceHandle` | 沒有會失敗的路 |
| 2 | 第 1 節的例子：路徑用 `›` | ASCII 的 ` > ` | 各平台的主控台都讀得出 |
| 3 | D4 (a)：致命紀錄帶 `trace_path` | 給的欄位超過 32 個時不加 | 固定大小的陣列，不配置 |
| 4 | D6：編排層五個入口各一段（沒寫做法） | 本體改名成私有的 `…Steps`，公開的入口是包一段的薄包裝 | 不必把 `ReplaceWorld` 兩百行重新縮排；和第 4 批改同檔時衝突最少 |
| 5 | D6：Kernel 在 `Fail` 標當下那一段 | 同；另外同步點、清空輪的 `Fail` 在 hook 已留下錯誤時只標經過 | F1 的修正（照 D4 (i)） |
| 6 | D4 (i)：編排層「Kernel 帶上來的是往外傳」 | 只看碼（第 2 節） | F2 的修正：帶 `failure` 不等於 Kernel 產生的 |
| 7 | D8：`options.clock` | `TraceOptions{monoNs}`（函式指標） | 和日誌的 `mono_ns` 同名；不配置 |
| 8 | 登記列 `…,base;logging,time,,` | `…,base,logging;time,,` | 宣告的寫法錯了欄：`depends` 一欄 `base`，`private_depends` 一欄 `logging;time` |
| 9 | 第 8 節 TR-5：沒寫 category 的上限 | 64 位元組，超過截斷 | 審查建議 3：原來是斷言 |

## 5. 沒做、記下的事

| 項 | 內容 | 為什麼沒做 |
|---|---|---|
| hook 清空輪上限、當場改的錯誤 | 這兩處 `Fail` 的 origin 沒有 trace 的測試（標成經過的突變會存活） | 要的設置在 Kernel 測試的別的檔案裡（file-local 的輔助）；預設就是 origin，和 F1 的修正無關 |
| `MarkError` 送給「現在」的接收者 | 宣告只約定 `end` 回到開始時的接收者 | 宣告沒約定；觀察到問題再加（README） |
| 路徑紀錄的 `world` | 有寫，但 TR-5 沒列成約定 | 要不要算約定，交你（目前不改） |
| 沒裝 Logger 時路徑寫不寫 | 寫不寫只看 `Taken`，標準錯誤上看不到；沒有行程外的探測 | 機制上不寫（`Installed()`）；測試只數時鐘 |
| Parallel 下的父子 | 只有 Sequential（LIM-2） | RC-T1 |
| 回滾重跑的段和第一次分不出 | 第 4 批的事 | 宣告第 7 節交界 |
| 超過 64 層的標記 | 丟掉，路徑一筆都沒有（審查第 8 項；更好的做法是用存著的 64 層寫路徑並標省略） | 64 層很少碰到；寫進 TR-4，觀察到再改 |
| 段在別的執行緒結束 | 不寫、原執行緒的那一層留著，之後的路徑讀到它的名字（審查第 9 項） | 只約定同一條執行緒；Parallel 時碰到（RC-T1） |
| TR-1「Module 不依賴 trace Module」 | 只有 Kernel（K5）與編排層（O4）有守衛，別的 Module 靠審查 | 別的 Module 現在沒有理由含入；有了再立守衛 |
| Base 門面的 `world` 參數 | 最底層出現 Kernel 的概念（審查第 12 項，AGENTS §6） | 宣告 M6 已核准，不重開，只記一筆 |

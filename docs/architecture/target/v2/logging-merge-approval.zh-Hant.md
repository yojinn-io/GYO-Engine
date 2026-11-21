# 合併前核准：日誌機制（`v8-arch-gyo2-logging`）

件數の突き合わせ：一致（判定 3：檔案輸出端 `Close` 後「推入＝寫出＋滿了丟＋關了才來」由 `logging.file` 各案與情境 S3-4 驗，S3-4 推入 1,000＝寫出 1,000（999 筆加 1 筆重複鍵的替換紀錄）、丟 0；結尾行的件數＝`Counts()`；`fatal_direct`＝致命寫出口真正交出去的行數）。

日期：2026-11-21。分支 `claude/gyo2-logging-10dfcd`（draft PR #256，base `gyo2` e4072b5c）。事前宣告：[logging-declaration.zh-Hant.md](logging-declaration.zh-Hant.md)（修訂 1 動工前核准、修訂 2 第 17 節、修訂 3 第 18 節）。Design Invariants：[logging.zh-Hant.md](logging.zh-Hant.md)。經過與證據：[dev_log](../../../dev_logs/2026_11_20_gyo2_logging.zh-Hant.md)。

**核准紀錄**（2026-11-21，日誌卡；PR #256 合併為 d3371355）：使用者原話依序「OK，合併，讓互毆繼續」（中途被打斷）、「可以合併，然後繼續」。卡片把「可以合併」讀成下面第 2～6 節整頁一起核准（使用者沒有逐節回覆）；主會話已記進 HANDOFF。本紀錄由 trace 批（`v8-arch-gyo2-trace`）補上，因為 logging 的分支已經合併。

**請你核准的**：第 2 節（判定 2 的變更，AGENTS §14.4）、第 3 節（Design Invariants 的最終文字，含 `world_time` 的 WT-4）、第 4 節（突變的數目超過基準）、第 5 節（和宣告不同的地方）、第 6 節（沒修、記下的事）。PR 已改成 ready、L1 三平台綠；你說「合併」才合併。

## 1. 判定的結果（宣告第 10 節）

| # | 結果 | 證據 |
|---|---|---|
| 1 | 本機 macOS x86_64 全量 ctest 151/151（core preset）；L1 三平台綠（run 44781745152） | 本機 ctest；L1 run 見本節末 |
| 2 | 照第 2 節變更後成立：注入「永不回來的寫出」時 `WriteFatalRecord` 回來、致命那一行在、回報標逾時（`logging.fatal`）；致命寫出口都寫完才 flush、全部輸出端共用一個期限；`std::terminate` 三種情形各恰好一筆 `Fatal` 並交回之前的處理器（`logging.terminate.*`）；③ 收到的一行在兩個輸出端各一筆（S3-1） | ctest；`step7-terminate-red.txt`、`review-fixes-red.txt` |
| 3 | 件數一致（文首） | ctest |
| 4 | 情境 S3-3：日誌開關，遊戲自己的 Resource 與讀者讀到的逐位元相同 | ctest |
| 5 | 每行 `Parse` 讀回（含致命路徑的重複鍵，審查 S2 修正）；NaN、±Inf 照約定；golden | ctest；L1 三平台 |
| 6 | 突變 87 個全部 killed（第 4 回與補跑）；Fable 5.2 的敵對式檢查補的 22 個、§14.4 驗證者建議的 1 個也在其中 | `mutations-run4.txt` |
| 7 | 只量（第 5 步）：直接寫一筆進佇列中位數 3199 ns、p99 23386 ns；經 `Forward` 一筆約 6759 ns；被篩掉 47 ns（Intel i9-9980HK，Release，一次） | `judgement7-measure.txt` |

證據都在主 checkout `build/evidence/gyo2-logging/`（本機 macOS，git 不追蹤）。

**L1**：第 2 步的 run 44704959856 三平台綠（含兩個附加 handle 的並行寫）。審查前的 run 44774463275：linux-x64、macos-arm64 綠；windows-x64 的 `logging.terminate.*` 三件失敗——探測程式用結束碼 3 表示「Install 失敗」，而 Windows 的 `abort()` 結束碼也是 3（terminate 本身成功，stderr 有那一行）。改成提前離開時印標記、腳本看標記（不是實作缺陷，是測試的判讀，AGENTS §14.1 的切分：測試自身）。第二次 run 44779446258：linux-x64 的 `logging.console_probe.*` 三件失敗——`ConsoleProbe.cmake` 沒有 `cmake_minimum_required`，runner 的 CMake 3.31.6 用舊政策讓 `while(TRUE)` 不執行、計數是 0（同樣是測試自身；本機 CMake 4.4.3 沒抓到），改用 `string(REGEX MATCHALL)` 並明寫政策（7a7175d5）。**修正後 run 44781745152（c00c1ac1）：linux-x64、windows-x64、macos-arm64 三平台綠，CI gate success。**

## 2. 判定 2 的變更（AGENTS §14.4；和宣告第 18 節同一份）

驗證者：Fable 5.2／high（提案者 Opus 5.5 以外的模型），2026-11-21，報告 `judgement2-verification-fable.md`。結論：五點沒有「不成立」；1、2、4、5 原本只部分成立（不完整、兩份文件不一致），照它的更正補齊如下。

1. **守的性質**：
   - 舊判定守的：World 壞掉時，`WriteFailure`／`WithFailureLog` 自己寫恰好一筆 `Fatal`，欄位等於 `WorldFailure`，回傳時已經在檔裡，runner 寫完就退出；致命路徑不等寫者。（舊判定沒有守 `std::terminate` 留一筆：當時只 flush。）
   - 新判定守的：斷言、`std::terminate` 時日誌確實留下一筆而且只有一筆，致命路徑不等寫者；World 壞掉時，產品經 ③ 寫來的那一行確實成為紀錄。
2. **緩める面**（以前判失敗、現在判通過的）：
   - World 壞掉的那一筆不再檢查是 `Fatal`、帶 `WorldFailure` 的逐欄欄位、經致命寫出口立刻進檔。
   - 不再檢查 runner 包裝（`WithFailureLog`）的行為（拿掉了；退役的突變 `lg-failure-*` 2 個、`lg-withfailurelog-*` 4 個，`FailureTests.cpp` 8 案）。
   - 不再檢查 `DropOldFirst` 替換失敗時的那一筆，也不再檢查編排層的 `Warning` 成為紀錄、在 `Fatal` 之前（舊 S3-3；異常分級拿掉了那幾處 `Warning`）。
   - System 丟例外的情形（舊 S3-2、`failure: a System that throws…`）情境不再跑；日誌這一側由 `logging.terminate.std` 守，那一筆不帶 World 名。
   - World 壞掉那一行不再無條件寫標準錯誤（舊 `CHECK(report.console)`）：改走 ③ 後，只有裝了主控台輸出端才印。
   - `WriteFailure` 先、`TakeKernelDiagnostics` 後的順序（舊 `failure: WriteFailure first…`）不再判（LG-1 的變更）。
   - 不算放寬的：作者經 ④ 寫的 `Fatal` 不數（舊檢查數的是致命路徑的 `record.fatal`，對 ④ 本來就沒有限制）。
3. **締める面**（以前判通過、現在判失敗的；都由舊檢查通過、新檢查失敗驗過）：
   - `std::terminate` 三種情形各恰好一筆 `Fatal`（舊實作 0 筆也過，`step7-terminate-red.txt`），並交回之前的處理器（`lg-terminate-not-chained`）。
   - 致命寫出口都寫完才 flush（`lg-fatal-flush-first`）、全部輸出端共用一個期限（`lg-fatal-deadline-per-sink`）、不收這個 category 的輸出端也 flush（`lg-fatal-flush-only-taking`）。這三項舊的 9 個 `fatal:` 案都通過，只有新加的案失敗（敵對式檢查找到）。
4. **緩める面守的性質由誰守**：
   - 「致命路徑不等寫者」照舊由 `logging.fatal` 與 `lg-fatal-*` 守，對象是 E0（斷言、`std::terminate`）。
   - 「World 壞掉時留下一行」：日誌這一側（收到就寫成紀錄）由情境 S3-1 守（突變 `lg-baselog-not-written`）；誰寫、寫不寫歸產品。
   - **不再有任何判定守**：World 壞掉那一行是 `Fatal`、立刻進檔（它走佇列，S3-1 先 `Flush` 才讀檔）、逐欄欄位、無條件寫主控台。理由：使用者定「日誌只是看。trace才是追問題。」，Engine 不給標準 main（PR #255）。
5. **根據與證據**：使用者原話記在本節（「④ 不限制」「terminate 先寫一筆」兩件是主會話轉達的「照主會話的看法」，沒有逐字原話）；HANDOFF 代號是主會話轉達時給的，HANDOFF 由主會話記，本卡在 `origin/gyo2` 與各工作樹的 `HANDOFF.md` 沒有讀到落檔（驗證者 2026-11-21 指出），出處以主會話的對話為準。證據：ctest；突變第 4 回（`mutations-run4.txt`：第一輪 85/86，`lg-file-batch-reversed` stale，照審查修正後的程式改寫後補跑 killed；再加 `lg-baselog-not-written` killed，共 87/87）；`step7-terminate-red.txt`、`review-fixes-red.txt`；驗證報告 `judgement2-verification-fable.md`（Fable 5.2／high，提案者 Opus 5.5 以外的模型）。

## 3. Design Invariants 的最終文字

照 [logging.zh-Hant.md](logging.zh-Hant.md)。和動工前核准（宣告第 8 節）的差：

- **LG-1**：⑤ 拿掉，入口是 ①②③④⑥；World 壞掉時記不記由產品的 main 定、經 ③ 或 ④；拿掉「先 `WriteFailure` 再取」，補「取診斷紀錄失敗時那一行照寫」。
- **LG-2**：改成「致命路徑只給 E0」（斷言、`std::terminate`）；`std::terminate` 先寫一筆再交回；「不保證」加「Windows 上 `std::set_terminate` 是每條執行緒各一份」（審查 S1）。
- **LG-5**：沒有 `product`、`begin_ns`、`duration_ns`；`kind` 只有 `log` 與 `end`；重複鍵的替換紀錄也送到原本要收它的輸出端（審查 m1）；致命紀錄有重複鍵時仍是 `Fatal`，欄位換成 `dropped_fields`（審查 S2）。
- **LG-7**：`SetLevel` 在鎖下存層級並重算下限（審查 S3）。
- **LG-9**：退役（span 歸 trace Module）。RC-L4、RC-L7 退役，RC-L8 改成「第六種」。
- **LG-11**：寫失敗只在輸出端自己身上表示；佇列丟的行是 E3、計數進結尾行。
- **`world_time` 的 WT-4**（[world-time.zh-Hant.md](world-time.zh-Hant.md)）：依賴加 `logging`（描述器 `TimeDiscardedLog` 放在訊息旁邊，宣告第 13 節第 1 題 D 已核准；條文原本沒跟著改，審查 M1）。這是另一個 Component 的 Design Invariants，請一起核准。

## 4. 突變的數目（README 基準 8）

Design Invariants 在用的 10 條 × 5 ＝ 50，清單 87。

- 第 1～6 步 64 個；修訂 3 退役 9（`WriteFailure`／`WithFailureLog` 6、span 3）、加 terminate 5。
- 審查的修正 4（S2、S3、S4、m1，各先 Red）。
- **Fable 5.2 的敵對式檢查**：在清單外找到 31 處改壞實作而測試全過（38 個候選存活）。條文或判定寫明的 17 處全部補了測試與突變；既有測試抓得到、只缺突變的 3 處補了突變；級別名稱三個合成一個（清單一個突變就夠）。共加 22。
- §14.4 的驗證者建議替 ③ 立一個突變：`lg-baselog-not-written`（`guards` 指判定 2）。
- 標頭或宣告的細節 14 處沒補（第 6 節）。
- 每個加的突變都寫了只有它捕的誤（`what`）與 `guards`；`expect` 分得出是哪個突變造成的（terminate 探測改成分開報 fatal／category／text，`lg-file-full-not-dropped`、`lg-setlevel-not-stored` 改指只有它會讓失敗的判定）。
- 多的主因是 LG-2（致命路徑，26 個）：順序、期限、重入、主控台、處理器的交回、terminate 的三種情形，每一個都是會獨立出錯的決定。

## 5. 和宣告不同的地方

宣告第 5 節開頭的表（〔修訂 3〕本節和實作不同的地方）列了 9 處：`kind` 欄位、寫者執行緒的起動順序、⑤、`LogWorldLabel<D>`、診斷紀錄的先後、`Sink` 的介面（篩選在 `Logger`）、`product`、span 的鍵、寫失敗後的件數。其餘照宣告。

## 6. 沒修、記下的事

| 項 | 內容 | 為什麼沒修 |
|---|---|---|
| 審查 S1 | Windows 上別的執行緒走到 `std::terminate` 時沒有那一筆 | 要在起執行緒的地方另裝（`engine/threads` 的事）；LG-2 的「不保證」寫明 |
| 審查 m2 | 替換紀錄也寫不出（沒有記憶體）時，檔裡多一個空行 | 只在記憶體耗盡時；建議另開 |
| 審查 m3 | `TakeKernelDiagnostics` 取走之後組字串丟例外，剩下的診斷紀錄不見、不計數 | 同上 |
| 敵對式檢查 B 類 14 處 | 標頭或宣告的細節（移動賦值、`Base::Log` 的 Debug、`RejectedSpawn` 的欄位、讀者名的 `@observer` 等） | README：細節的突變在觀察到漏看之後才加 |
| trace | span 歸 trace Module | 另寫宣告、另開 PR（使用者 2026-11-21 核准的方向） |

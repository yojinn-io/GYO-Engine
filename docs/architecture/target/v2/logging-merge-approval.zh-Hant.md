# 合併前核准：日誌機制（`v8-arch-gyo2-logging`）

件數の突き合わせ：一致（判定 3：檔案輸出端 `Close` 後「推入＝寫出＋滿了丟＋關了才來」由 `logging.file` 各案與情境 S3-4 驗，S3-4 推入 1,000＝寫出 1,000（999 筆加 1 筆重複鍵的替換紀錄）、丟 0；結尾行的件數＝`Counts()`；`fatal_direct`＝致命寫出口真正交出去的行數）。

日期：2026-11-21。分支 `claude/gyo2-logging-10dfcd`（draft PR #256，base `gyo2` e4072b5c）。事前宣告：[logging-declaration.zh-Hant.md](logging-declaration.zh-Hant.md)（修訂 1 動工前核准、修訂 2 第 17 節、修訂 3 第 18 節）。Design Invariants：[logging.zh-Hant.md](logging.zh-Hant.md)。經過與證據：[dev_log](../../../dev_logs/2026_11_20_gyo2_logging.zh-Hant.md)。

**請你核准的**：第 2 節（判定 2 的變更，AGENTS §14.4）、第 3 節（Design Invariants 的最終文字，含 `world_time` 的 WT-4）、第 4 節（突變的數目超過基準）、第 5 節（和宣告不同的地方）、第 6 節（沒修、記下的事）。核准後我把 draft 改成 ready，看 L1 三平台；你說「合併」才合併。

## 1. 判定的結果（宣告第 10 節）

| # | 結果 | 證據 |
|---|---|---|
| 1 | 本機 macOS x86_64 全量 ctest 151/151（core preset）。L1：見本節末 | 本機 ctest；L1 run 見下 |
| 2 | 照第 2 節變更後成立：注入「永不回來的寫出」時 `WriteFatalRecord` 回來、致命那一行在、回報標逾時（`logging.fatal`）；致命寫出口都寫完才 flush、全部輸出端共用一個期限；`std::terminate` 三種情形各恰好一筆 `Fatal` 並交回之前的處理器（`logging.terminate.*`）；③ 收到的一行在兩個輸出端各一筆（S3-1） | ctest；`step7-terminate-red.txt`、`review-fixes-red.txt` |
| 3 | 件數一致（文首） | ctest |
| 4 | 情境 S3-3：日誌開關，遊戲自己的 Resource 與讀者讀到的逐位元相同 | ctest |
| 5 | 每行 `Parse` 讀回（含致命路徑的重複鍵，審查 S2 修正）；NaN、±Inf 照約定；golden | ctest；L1 三平台 |
| 6 | 突變 86 個全部 killed（第 4 回）；Fable 5.2 的敵對式檢查補的 22 個也在其中 | `mutations-run4.txt` |
| 7 | 只量（第 5 步）：直接寫一筆進佇列中位數 3199 ns、p99 23386 ns；經 `Forward` 一筆約 6759 ns；被篩掉 47 ns（Intel i9-9980HK，Release，一次） | `judgement7-measure.txt` |

證據都在主 checkout `build/evidence/gyo2-logging/`（本機 macOS，git 不追蹤）。

**L1**：第 2 步的 run 44704959856 三平台綠（含兩個附加 handle 的並行寫）。審查前的 run 44774463275：linux-x64、macos-arm64 綠；windows-x64 的 `logging.terminate.*` 三件失敗——探測程式用結束碼 3 表示「Install 失敗」，而 Windows 的 `abort()` 結束碼也是 3（terminate 本身成功，stderr 有那一行）。改成提前離開時印標記、腳本看標記（實作缺陷不是，是測試的判讀，AGENTS §14.1 的切分：測試自身）。修正後的 run：〔待補〕。

## 2. 判定 2 的變更（AGENTS §14.4；全文在宣告第 18 節）

1. **守的性質**：斷言、`std::terminate` 時，日誌確實留下一筆而且只有一筆，致命路徑不等寫者；World 壞掉時，產品寫來的那一行確實成為紀錄。
2. **緩める面**：不再檢查 World 壞掉的那一筆是 `Fatal`、帶 `WorldFailure` 的逐欄欄位、經致命寫出口立刻進檔；不再檢查 runner 包裝（`WithFailureLog`）；不再檢查 `DropOldFirst` 替換失敗時的那一筆。理由：使用者 2026-11-21「日誌只是看。trace才是追問題。」、Engine 不給標準 main（PR #255），這些已經不是日誌的事。
3. **締める面**：`std::terminate` 三種情形各恰好一筆 `Fatal`、交回之前的處理器（以前 0 筆也過）；致命寫出口都寫完才 flush、全部共用一個期限、不收這個 category 的輸出端也 flush（以前沒有判定守，敵對式檢查找到）。
4. **緩める面守的性質由誰守**：「致命路徑不等寫者」照舊由 `logging.fatal` 與 `lg-fatal-*` 守；「World 壞掉時留下一行」日誌這一側由 S3-1 守（收到就寫成紀錄），誰寫、寫不寫歸產品；逐欄不再守（日誌只是看）。
5. **根據與證據**：使用者原話（宣告第 18 節）；ctest 與突變第 4 回。驗證者（AGENTS §14.4：緩める面がある場合、提案者と別の検証者）：〔待補〕。

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

Design Invariants 在用的 10 條 × 5 ＝ 50，清單 86。

- 第 1～6 步 64 個；修訂 3 退役 9（`WriteFailure`／`WithFailureLog` 6、span 3）、加 terminate 5。
- 審查的修正 4（S2、S3、S4、m1，各先 Red）。
- **Fable 5.2 的敵對式檢查**：在清單外找到 31 處改壞實作而測試全過（38 個候選存活）。條文或判定寫明的 17 處全部補了測試與突變；既有測試抓得到、只缺突變的 3 處補了突變；級別名稱三個合成一個（清單一個突變就夠）。共加 22。
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

# retention-600：判定變更（AGENTS §14.4 的 1～5）與 L2 不退步紀錄的宣告草案

- **狀態**：**核准**（v7.1 D176，2026-10-24：判定變更 J-A～J-C 與第 6 節 L2 不退步紀錄的宣告草案）。程式在分支上（`d1bcb47`）。
- **計畫**：[outage-resync.md](outage-resync.md) 的 `retention-600`（D172 的 D-6：行為改變的兩面一起核准，§14.4 的材料在這批交給你）。
- **條文**：[network-architecture](../../network-architecture.zh-Hant.md) §4「連線中斷與恢復」、§7「被替代序號的單一紀錄」。

## 0. 這批改了什麼（和計畫的不同）

計畫原本預期 `retention-600` 會讓「600 序號內、但被最新 64 筆擠出去的晚到」多出 −1 s 樣本，`:1363` 的判定因此反轉。`match-resync` 先做之後，情況變了：

- **樣本這一面沒有變化**：
  - 樣本只來自晚到不超過 N_o（20 Tick）的第一份副本；
  - 每個 Tick 只解析一個序號，Host 一次 Advance 最多追趕 5 步；所以晚到不超過 N_o 的序號，比它新的被替代序號最多約 30 個（同一次 Advance 4 個、下一次 5 個、之後約 21 個），一定在最新 64 筆之內（獨立核對的計算）；
  - 因此拿掉 64 的上限，產品發布的樣本不變。`:1363` 維持 CHECK_FALSE，不反轉。
- **真正改變的一面**：
  - 以前 600 序號以外、但還在最新 64 筆內的紀錄，會以「只供樣本」的形式保留，副本到達時算一次 `late_beyond_no`，並觸發 Resync；
  - 現在紀錄在 600 序號時關閉，之後的副本在帳本記成 `resolved_untracked`，不產生樣本、不計數、不觸發 Resync。
  - 這是條文 §4 的規則：「晚到判斷需要替代紀錄還在」。
- **新增的檢查**：S3（`late_first == slack_late + late_beyond_no`）成為結構上的等式，加進 C++ 測試的守恆檢查（`CheckSlackConservation`，每個 `CheckSubstitutionConservation` 都會一起檢查）。凍結的 `ingress_evidence.py` 不改（D-12）。

## 1. 判定變更 J-A：Design Invariant 守衛「稀疏替代」

測試：`tests/object_fps_pvp/PvpMatchTests.cpp` 的 `PvP outage characterization: a sparse substitution is sampled at any age`。它在 `match-resync` 時升格為 §7 的守衛。

| 晚到 | 之前（match-resync） | 之後（retention-600） |
|---|---|---|
| 100、590 Tick | 不產生樣本、`late_beyond_no` 1、Resync（epoch 2）、late_first 1 | 相同，另外檢查 S3 |
| 610、700 Tick | 不產生樣本、`late_beyond_no` 1、Resync（epoch 2）、resolved_untracked 1 | 不產生樣本、`late_beyond_no` 0、不換 epoch、resolved_untracked 1，另外檢查 S3 |

1. **守的性質**：晚到超過 N_o 的第一份副本絕不成為樣本；紀錄還在時，它會觸發 Resync。
2. **放寬的面**：600 序號（約 10 s）以前就被替代的序號，它的副本以前會觸發 Resync、計 `late_beyond_no`，現在不會。舊判定對新行為會失敗。（`resolved_untracked` 不是新變化：之前就是。）
3. **收緊的面**：
   - 翻轉的一面：610、700 Tick 現在要求「不換 epoch、`late_beyond_no` 0」，所以「保留只供樣本的紀錄、因此 Resync」的實作在新判定下會失敗；
   - S3：本測試裡的 S3 是冗餘的（前面的明確檢查已經蘊含）；真正增加檢出力的是 5 個 `CheckSubstitutionConservation` 呼叫點都一起檢查 S3；
   - 「不產生樣本」對每一種晚到都照舊檢查。
4. **性質由哪些判定繼續守著**：
   - 「不產生樣本」：本測試的 4 種晚到全部檢查；
   - 「紀錄還在時觸發 Resync」：本測試的 100、590 Tick，以及 `PvP host turns a first copy later than N_o into a Resync instead of a slack sample`、`PvP host gives no sample for any first copy later than N_o and asks for one Resync`、`PvP host measures N_o in time…`；
   - 保留期的精確邊界（正好 600 時還開著、超過就關閉）由單元測試 `PvP ingress ledger keeps a substitution through the retention and closes it aged after` 與突變 `v7-flow-ledger-retention-off-by-one`、`v7-flow-ledger-copy-retention-off-by-one` 守著；
   - 被放寬的只有「紀錄已經不在」的情況，這正是條文 §4 的定義。
   - **產品上的意義**：玩家持續送命令、只有一個封包卡了 10 s 以上才到。這時相位追蹤已經靠之後的命令建立，沒有要重新對時的對象；如果是真的中斷，10 s 之內 Starvation 就會換 epoch。
5. **根據與證據**：條文 §4；測試；突變（第 3 節）；獨立核對（第 4 節）。

## 2. 判定變更 J-B：紀錄的單元測試

測試：`tests/object_fps_pvp/MatchIngressLedgerTests.cpp`。拿掉 2 個測試：

- `…sample only the newest candidates and keep an aged candidate until it arrives`
- `…forget a record that ages out without candidacy`

換成 1 個：`PvP substitution records let the first copy of every open record decide, once`。

1. **守的性質**：過渡規則：只有最新的候選會產生樣本；老化的候選保留到到達。
2. **放寬的面**：下列不再檢查，因為對應的程式已經刪除（D-6 照設計移除過渡規則）：
   - 被擠出去的候選不產生樣本；
   - 老化的候選照樣產生樣本，而且到達時讀時鐘（翻轉：現在要求不讀）；
   - 只供樣本的紀錄，第二份副本不是樣本；
   - `Unsample` 會清掉候選。
3. **收緊的面**（實際檢查到的）：
   - 開著的紀錄的後續副本不是 `first`，而且不讀時鐘；
   - 老化的紀錄的副本既不是 open 也不是 first，而且不讀時鐘。
   - 「最舊的開著的紀錄的第一份副本是 first」在舊測試裡已經檢查過，不算新增。
4. **性質由哪些判定繼續守著**：不是放寬，是規則被取代。樣本的決定變成「first＋N_o 規則」，由 Host 層的測試守著（J-A 第 4 點列的 3 個）。
5. **根據與證據**：計畫第 13 節、條文 §7、突變（第 3 節）。

## 3. 判定變更 J-C：突變 `v7-flow-ledger-drives-sample` 的改寫

這個突變是 §7「被替代序號的單一紀錄」列出的方向守衛，所以照 §14.4 交你核准。

1. **守的性質**：診斷（帳本的分類）不驅動判定（樣本與 Resync）。
2. **放寬的面**：
   - 原本的寫法（「晚到判斷改跟帳本的 late_first 走」），在拿掉只供樣本的紀錄之後，和產品等價：late_first 成立若且唯若 `arrival.first` 成立。所以它不再能被殺掉；
   - 第一版改寫把 `expect` 放寬成正規表示式 `outage characterization|Resync|N_o`；結果出來之後，已收窄成實際殺掉它的 §7 守衛：`outage characterization: ((two-way|one-way) outages|partial delivery)`。
3. **收緊的面**：新寫法是「晚到判斷也放過帳本的 resolved_copy」。被執行過的命令的副本會因此被當成晚到過久，觸發不該有的 Resync，所以測得到。
4. **方向由哪些判定繼續守著**：
   - 靜態的 `test_the_substitution_records_read_no_ledger`：紀錄的標頭不得引用帳本或 IngressStatistics；
   - 改寫後的本突變。
5. **證據**：突變結果（第 4 節）；獨立核對指出，新突變能否被殺掉取決於測試時鐘：非 open 的到達，`substitutedAt` 是預設值 `time_point{}`。

## 4. 突變

- **退役 2 個**：`ssr-unsample-empty`、`ssr-aged-noncandidate-kept`，它們改的程式已經刪除。
- **改寫**：
  - `v7-flow-ledger-late-copies-undercount`：find 改指新的程式碼，意圖不變。
  - `v7-flow-ledger-drives-sample`：拿掉「只供樣本」之後，「第一份」和帳本的 late_first 在構造上相同，原本的寫法變成等價突變。改成「晚到判斷也放過帳本的 resolved_copy」，這仍是「診斷驅動判定」的一種，而且測得到。
- **重新判定**：變動檔案（`MatchRuntimeHost.cpp`、`MatchIngressLedger.hpp`、`MatchSubstitutionRecords.hpp`、`IngressStatistics.hpp`）上的 84 個：**84／84 killed**（`mutants-retention-600.json`）。
- **`v7-flow-ledger-drives-sample` 改寫後**：手動套用，26 個測試失敗，包括 §7 的 3 個守衛（雙向、單向、部分送達）與 `PvP host ledger classifies late copies…`。`expect` 收窄成那 3 個守衛之後再判定：killed。

## 5. 獨立核對（另一位審查者，2026-10-24，只讀 `d1bcb47` 與父 commit）

- **J-A**：第 1、4、5 點成立。第 2 點措辭有誤：`resolved_untracked` 不是新變化，已修正。第 3 點漏了翻轉的收緊面，以及 S3 在本測試裡是冗餘的，都已補上。
- **J-B**：第 1、4、5 點成立。第 2 點漏了 3 項不再檢查的內容、第 3 點過度宣稱，都已修正。
- **漏報**：`v7-flow-ledger-drives-sample` 的改寫與 `expect` 放寬，已補成 J-C；兩個突變的退役要記進 dev_log。
- **「拿掉 64 的上限不改變樣本」**：成立。上界約 30 筆，不是 20 筆，已修正。依賴「Host 時鐘與 Advance 的 elapsed 互相耦合」這個產品條件。
- **文件小誤**：§7 寫「之前兩個方向都不保證（187 對 64）」，這只適用於 match-resync 之前；match-resync 到 retention-600 之間，只有稀疏的方向會破壞 S3。已修正。

## 6. L2 不退步紀錄的宣告草案（D-13 的一部分，請一起核准或修改）

- **目的**：確認 `retention-600`（以及到它為止的 `match-resync`、`single-store-refactor`）在既有的網路情境下沒有退步。
  - 這批的行為改變只影響「10 s 以前被替代的序號」，既有語料裡不會出現，所以這是不退步的紀錄，不是行為改變的證明。
  - 行為改變由第 1、2 節的測試證明。
- **樹**：只跑頭（`retention-600` 的 commit），不做 base 對照。比較用的數字取自既有語料：pv7-checkpoint、d44 的同一情境。
- **項目**（每項 3 次，共 9 次）：
  - `x1`：`run_client_delay.py`，d44 的 x1 情境；
  - `ramp`：`run_client_delay.py`，d44 的 ramp 情境；
  - `gateway-250ms`：`run_network.py`，pv7-checkpoint 的 backpressure 中的 gateway-250ms 子情境。
- **判定**（每次都要成立，0 容許）：
  - S1、S2、J5a 成立（凍結的 `ingress_evidence.py`）；
  - S3 成立：ingress 行的 `late_first` 等於 `slack_late`＋`resync statistics` 行的 `late_beyond_no_commands`，由新的判定工具計算，工具的雜湊在開跑前記錄；
  - runtimeFailed 為 0：Match 的 end 行不是 `runtime_failed`。
- **只記錄、不判定**：
  - `resyncs`、`late_beyond_no_commands`、`slack_clamped_samples`；
  - Client 的 `phase_samples_unmatched`、Late 決定次數；
  - 換 epoch 的次數與原因。
  - 推論（不作判定）：gateway-250ms 的晚到最大約 13 Tick，小於 N_o，`resyncs` 應為 0。
- **重試規則**：不重試。任何一次判定不成立，就停下回報，不以之後的成功覆蓋（§14.1）。
- **估時**：準備（判定工具、乾跑）2～3 h，執行約 1～1.5 h。你在場與否都可以，不需要 GUI。
- **D45**：宣告前做一次門檻來源的對抗式檢查，用小規模 ultracode：約 3 個方案、1 位審查者、1 次對抗式檢查。

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

## 6. L2 不退步紀錄的宣告（D176 核准草案；第 2 版依 D45 檢查修訂，D177 確認）

- **目的**：確認 `retention-600`（以及到它為止的 `match-resync`、`single-store-refactor`）在既有的網路情境下沒有退步。
  - 這批的行為改變只影響「10 s 以前被替代的序號」，既有語料不會出現，所以這是不退步的紀錄，不是行為改變的證明。
  - **P 不能當作 retention-600 行為改變的證據**；那由第 1、2 節的測試證明。
- **樹**：只跑頭（measured commit `1713191`，之後只允許改文件），不做 base 對照。比較用的數字取自既有語料（pv7-checkpoint、d44 的同一情境）。
- **項目**：3 輪，每輪依序 x1、ramp、gateway-250ms，共 9 次。
  - `x1`：`run_client_delay.py --profile x1 --delay-ms 200 --window-ms 2000 --fault-at 7.5 --rounds 1`（d44 的參數）；
  - `ramp`：`run_client_delay.py --profile ramp --peak-ms 250 --rise-ms 1500 --fall-ms 500 --fault-at 7.5 --rounds 1`；
  - `gateway-250ms`：**更正**：草案寫成 `run_network.py`，正確的是 `backpressure_probe.py --layers gateway --durations-ms 250`，只跑這一個案例。語料（pv7-checkpoint）是一次依序跑 6 個案例、gateway-250ms 排第 3，所以順序與暖機條件不同，和語料比較時列為差異。
- **有效性的前提**（新增，D177 確認；不是判定，不成立時記為 E）：
  - x1、ramp：`summary.json` 的 completed 為 true；剛好 1 輪，status 是 completed；`plan.json` 的 profile 參數、fault_at [7.5]、rounds 1 與上面一致；
  - gateway-250ms：`backpressure.json` 存在，layer 是 gateway、requested_ms 是 250，fault 的 start_ns 與 release_ns 都有值，relay_error 是 null。
  - **E 的處理**：E 不算 P 也不算 F，session 停下回報，並區分原因（環境、Test、實作）。不自動補跑；要不要補跑由你決定。
  - 語料中出現過環境造成的 E：d44 的 ramp-r7，send lateness p99 22 ms 超過 10 ms。審查者估計，這次 6 次 x1、ramp 至少碰到一次的機率約 11%。
- **判定**（每次都要成立，0 容許）：
  - **S1、S2、J5a**：凍結的 `ingress_evidence.py` 報告 passed。**J5a 的範圍（新增，D177 確認）**：x1、ramp 是注入跑次，帶 `--injection`，也就是另外要求 never_arrived＝0，比照 README 的注入判定式；gateway-250ms 不帶。
  - **S3**：
    - 每位玩家（0 以外）都恰好有 1 行 final=1 的 ingress 行與 1 行 final=1 的 resync statistics 行，必要的鍵都在；
    - `late_first == slack_late + late_beyond_no`；
    - 至少要有 1 位玩家；
    - player 0（沒有 resync 行）要求 `late_first == slack_late`。
    - 完整性與 player 0 是新增的收緊。
  - **runtime**（收緊，D177 確認）：Match 的 end 行恰好 1 行，reason 是 signal、exit_code 是 0，而且沒有 "Match simulation failed"。草案只寫「不是 runtime_failed」；`reason=exception` 代表診斷資料遺失，也算不成立。
- **旗標**（新增，D177 確認；判定都成立、但要停下回報，記為 FLAG）：`resyncs` > 0、`late_beyond_no_commands` > 0，或移動紀錄裡出現 resync 重設。
  - 理由：語料中這三種情境的晚到最大是 gateway-250ms 221 ms、x1 166 ms、ramp 68 ms，都小於 N_o（333 ms），出現 Resync 就是沒預期到的變化。
  - 所有判定都抓不到 Resync 的退步：門檻只決定副本計進 slack_late 還是 late_beyond_no，S3 的總和不變。所以要靠旗標。
- **只記錄、不判定**：
  - `slack_clamped_samples`、換 epoch 的原因與次數；
  - Client 的 `phase_samples_unmatched`、Late 決定次數；
  - runner 自己的判定（x1、ramp 的 run_case.passed、輪的 status；gateway-250ms 的 backpressure.json passed、command evidence 的 errors、blockage 期間的權威 Tick）；
  - timer baseline 前後的值；完整的 ingress evidence。
  - 有語料基準的量，和只有試跑一次的量，報告時分開寫。
- **停止規則**：
  - 不重試。F、E、FLAG、逾時（記為 E）、項目結束後有程序殘留，都停下回報，不以之後的成功覆蓋（§14.1）；
  - 逾時照 run_d44 的方式處理：先 SIGTERM，15 s 後對整個程序群組 SIGKILL。
- **工具**（證據夾 `build/target/_build/test/logs/pvp-v71-retention-600-l2-prep-20261024/`，git 忽略）：
  - `run_r600.py` sha256 `58987b1c…`；
  - `judge_r600.py` `9f425876…`；
  - `artifacts.sha256` `977d6afd…`：Match、Gateway、action／timing probe、場地檔；Gateway 的 vcs.revision 是 `1713191`，vcs.modified false；
  - `frozen-r600-rooted.sha256` `f3c1fec7…`：re-evaluate 凍結的 59 個分析器，只有 `test_presentation_evidence.py` 之後改過，以目前的雜湊重新凍結；
  - measured commit `171319158b…`。
  - 完整值在 `literals.txt`；`--preflight` 已通過。
- **估時**：約 5～6 分鐘（試跑：x1、ramp 約 40 s，gateway-250ms 約 13 s）。不需要 GUI，也不需要你在場；跑的時候主機不要有其他重負載。

### D45 檢查（小規模 ultracode：2 位審查者＋1 位對抗式驗證者，2026-10-24）

- **判定**：照原本的準備不能開跑。必修的有 3 項：有效性前提、更正 runner 名稱、寫明 J5a 的範圍。另有 7 項建議修正。全部已處理，而且沒有放寬任何已宣告的判定。
- **被驗證者駁回的修正方法**（用了會把「只記錄」的量變成判定）：
  - 「沒有 failure.json」當有效條件：command evidence 不成立時，backpressure.json 與 failure.json 會同時寫出；
  - 「blockage 期間的權威 Tick > 0」當有效條件。
- **judge 的檢出力**（§14.2，竄改試跑的副本，9／9 判對）：
  - injection_invalid → E、plan 的 profile 不符 → E、沒有 backpressure.json → E；
  - 缺 final resync 行 → F、end 是 runtime_failed → F、end 是 exception → F、S3 被破壞 → F、resync 行缺鍵 → F；
  - resyncs=1 → FLAG。
  - 副本在 `judge-tamper/`。
- **試跑**（探索用，不算受入判定）：x1、ramp、gateway-250ms 各 1 次，新 judge 都判 P。

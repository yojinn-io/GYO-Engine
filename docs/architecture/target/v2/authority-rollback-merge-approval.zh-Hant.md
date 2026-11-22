# 合併前核准：權威 Module＋回滾 Module（`v8-arch-gyo2-authority-rollback`）

件數の突き合わせ：該当なし（本批的判定沒有送出與收到的件數；判定 5 的量測是「件數的突き合わせ：一致」，見第 1 節與 dev_log）。

日期：2026-11-22。分支 `claude/gyo2-authority-rollback-bda9ba`（draft PR #259，base `gyo2`）。事前宣告：[authority-rollback-declaration.zh-Hant.md](authority-rollback-declaration.zh-Hant.md)（修訂 6）。Design Invariants：[authority.zh-Hant.md](authority.zh-Hant.md)、[rollback.zh-Hant.md](rollback.zh-Hant.md)，以及第 2 節列的別頁。經過與證據：[dev_log](../../../dev_logs/2026_11_22_gyo2_authority_rollback.zh-Hant.md)；證據在主 checkout `build/evidence/gyo2-authority-rollback/`。

**請你核准的**：第 2 節（各頁 Design Invariants 的最終文字）、第 3 節（9.1 判定變更的緩的面，含檢證者補出的）、第 4 節（突變的數目）、第 5 節（和宣告不同的地方）、第 6 節（新型別與新碼）、第 7 節（沒做、記下的事）。**第 8 節**：兩題都已定（第 1 題 (a2)、第 2 題 (c)）並實作，請核准兩個 Architecture Delta 與條文。核准後我把 draft 改成 ready、看 L1；你說「合併」才合併。

## 1. 判定的結果（宣告第 13 節）

| # | 結果 | 證據 |
|---|---|---|
| 1 | 本機 macOS x86_64（Intel i9-9980HK）`ctest -L 'cpu\|shader'` 161/161（ecae9977）。L1 三平台（linux-x64、windows-x64、macos-arm64）在 ecae9977 全部通過（手動 dispatch，run 44932280155）。`git grep -n -i "summary-padding-a-then-c" -- tests engine` 0 命中（終了碼 1；正例 59fd4da9 命中 14 行） | 本機 ctest；GitHub Actions |
| 2 | 第 9.3 節的案全過；scenario 第 5 段 S5-1～S5-8 全過（宣告寫第 3 段，見第 5 節）。9.3 漏的 `BuildNewFirst` 失敗案第 8 步補上 | `rollback.*`、`authority.*`、`kernel.*`、`scenario.segment5` |
| 3 | 三份新清單經 Fable 5.2／high 敵對式檢查補齊。從頭完整跑（ecae9977）：回滾 74/74、權威 115/115、時間 95/95、`authority_scan` 18/18；編排層 237/242、Kernel 659/671——本分支造成的 7 個已修或退役（再跑都 killed，dev_log 逐個寫），其餘 10 個是 `gyo2` 上本來就壞的（Kernel 的 7 個 stale 與 `ks-k8-skips-executor-files`、編排層 2 個），列為後續。之後 A3、A5 的修正與突變另外跑過，都 killed | `full-mutation-ecae9977/`、`step8-checks/` |
| 4 | 31 個樣式在 HEAD 零命中，每個樣式在 f25837b0 先命中（正例）；方向兩條（`engine/(authority\|rollback)/` 的 include、`Authority::`／`Rollback::`）零命中 | `judgement4_scan.py`、`judgement4-ecae9977.txt` |
| 5 | 使用者的 Linux 實機（Ryzen 7 6800H），程式 9d956ed5：(1) 記錄每步 P1 37.871 µs ≤ 156 µs；(2) 一次回滾 708 µs ≤ 3,125 µs；(3) P2÷P1 5.347 ≤ 6.027，分母的四分位距比 1.7%、4.6%。件數の突き合わせ一致。中止一次（量法不符，結果沒看，改程式後重量） | `budget-v1/RESULT.zh-Hant.md` |

判定 5 之後回滾的修正（審查 1-2、1-3、1-5、1-6，第 8 步的 A2 trace 段、A4 預設、B2）都不經過量測走的路徑，或只在沒裝 trace 接收者時讀一個原子指標（TR-6），不重量。

## 2. Design Invariants 的最終文字（各頁標「草稿，合併前核准」的段落）

- **權威**（新頁）：[authority.zh-Hant.md](authority.zh-Hant.md) DI-A 1～12、AU-E1～AU-E9、第 4 節的限界。第 8 步：DI-A 2 的守衛照實寫（判定 4 的一次掃描＋審查，常設守衛照宣告 9.5 延到第 5 批）；AU-E7 含 Derived 與元件；`MarkComponent` 拿掉。
- **回滾**（新頁）：[rollback.zh-Hant.md](rollback.zh-Hant.md) R-1～R-8、RB-E1～RB-E10、第 3 節的限界。第 8 步：第 2 節加 trace 段、掛載逐個問、跨種類裁決、預設跟著時間 Module 與 RB-E10。
- **Kernel**：[kernel.zh-Hant.md](../kernel.zh-Hant.md) DI-KS 6、7、8、22、23 拿掉（保留編號）；DI-KS 5、12、15、25、27 改寫；RC 23（草稿）。
- **時間 Module**：[world-time.zh-Hant.md](world-time.zh-Hant.md) WT-1（`Resimulation` 每次準備放 false、`CarryOver` 不帶）、WT-5（伺服器權威查 `TickWriters()` 名單外的寫者）。
- **編排層**：[orchestration.zh-Hant.md](orchestration.zh-Hant.md) O-2 的 `ModuleBuffer`、RC-O10。第 8 步：實作改成和條文一致（準備函式跑完就放掉）。
- **其他頁**：[determinism.md](../determinism.md) DI 1、2 的過渡句；[world.zh-Hant.md](../world.zh-Hant.md) DI 4 拿掉；[system-contract.zh-Hant.md](../system-contract.zh-Hant.md) DI 3；[limits.zh-Hant.md](../limits.zh-Hant.md) LIM-4 解除、LIM-8 實作、LIM-1／2／5 的改寫。
- **契約頁改名**：`zone-c-sources.zh-Hant.md` → [authority-sources.zh-Hant.md](../../authority-sources.zh-Hant.md)（格式名 `gyo.authority-sources` 在宣告 4.3 已核准；頁名照宣告第 7 步的待定由我定）。**條文沒改**。DI 1 的「不讀 Kernel 內部結構」與 Replacement Criteria 的「Kernel 的 G-5 匯出落地後」還是舊的生產端；我的建議是改成「不讀 Engine 的內部結構」「權威 Module 的匯出」，意思不變。要改的話請一起核准。

## 3. 9.1 判定變更的緩的面（§14.4；方向 2026-11-20 已核准）

檢證者（Fable 5.2／high，`step8-checks/verify/REPORT.md`）：A1、A3、A8～A11 屬實；A2、A4～A7 不完整。補出的緩的面與處置：

| 組 | 檢證者補的 | 處置 |
|---|---|---|
| A2 | 「宣告的鏈把寫入帶到最後一環」那一案沒列（`InitialValueTests:812`） | 由權威的 `[au-z6-funnel-chain]` 守，突變 `au-funnel-no-chain`；只補記 |
| A4 | 重跑時不跑的 System、有 `rollback.Input` 歷史的輸入不受 Z6（只寫在 dev_log）；Z8 不再擋 Derived；豁免看登記不看掛載；錯誤訊息不再說經哪個請求填的；漏斗走訪三項沒人守 | Z8 擋 Derived **補回**（AU-E7）；豁免改成逐個掛載看條件（B2）；走訪三項補案與突變（V-E1～3）；前兩項與訊息照實列為緩的面（請核准） |
| A5 | 只讀判定資料的 System、型別大小與對齊、`Local`／不帶型別的 `Commands`／effects 等不進指紋；格式沒有 golden 與版本突變；hook 清空輪上限、observer 的先後、`InitialValue` 的請求、hook 的順序沒有判定守 | 後四項補案與突變；前幾項是權威份 5.5 內容表的選擇（不收），版本常數照分類屬低價值不補——請核准為緩的面 |
| A6 | `OneEndOnly` 的寫者不列；範圍外的錯誤不再點名 System；hook 與 observer 新列入沒有判定守；一般元件的 `InitialValue` 不列沒人守 | hook、observer、一般 `InitialValue` 補案與突變；前兩項照權威份 5.9 列為緩的面 |
| A7 | 登記先後的平手不再算排在前面（以前通過，現在 RB-E7） | 照 R-7 的條文（宣告 9.3 的 `[rb-r7-tie-not-order]`），列為緊的面 |

## 4. 突變的數目（README 基準 8）

| 清單 | 本分支加 | 退 | 現在 |
|---|---|---|---|
| `authority_mutations.json`（新） | 117 | 0 | 117 |
| `rollback_mutations.json`（新） | 78 | 1 | 77 |
| Kernel `mutations.json` | 44 | 110 | 671 |
| `orchestration_mutations.json` | 7 | 2 | 241 |
| `world_time_mutations.json` | 4 | 1 | 95 |

- 基準：權威 DI-A 12 條 × 5＝60、回滾 R-1～R-8 共 8 條 × 5＝40。兩份都超過。
- 多的主因：
  - 指紋的內容表有 16 項，每項有「該進」與「該不進」兩個方向，都是獨立的誤點。
  - AU-E 九碼各自有漏報與誤報兩面。
  - R-8 的入口有三條修剪路、C 的兩個邊界、比較的兩個方向。
  - 第 8 步的敵對式檢查找到約 120 處沒人守的誤點，照分類只補了契約類（權威 40、回滾 28、Kernel 15）；不補的逐項寫在 dev_log。

## 5. 和宣告不同的地方

| # | 宣告 | 實際 | 理由 |
|---|---|---|---|
| 1 | scenario 第 3 段（S3-1～S3-8） | 第 5 段（S5-1～S5-8） | 第 3、4 段在動工後被日誌、trace 用掉 |
| 2 | 2.3：官方策略的預設跟著時間 Module | 第 7 步以前兩個參數都必填、沒有預設；第 8 步改成 `StepSchedule()` 與 `frameSchedule` 空字串跟著時間 Module，並加 RB-E10 | 審查 A4 探針重現：沒登記步的 schedule 時權威值永遠不採用、啟動不報 |
| 3 | 10.4：重跑每步開一段 trace | 第 7 步以前沒做，第 8 步補上（案與突變） | 審查 A2 |
| 4 | 4.3 只有 `Mark<T>` 與 `MarkResource` | 第 5 步多做了 `MarkComponent<T>`，第 8 步拿掉 | 審查 A6：沒有 Feature、沒有測試 |
| 5 | — | Z6 不看「重跑時不跑」的 System 與有 `rollback.Input` 歷史的輸入；`OneEndOnly` 可以是 hook、observer；指紋收 set 的條件與 observer 的對象、不收回滾的條件；名字標記的型別不進 digest | 審查 1-5 等（dev_log「和宣告字面不同」） |
| 6 | — | 回滾：入口在 C 前進而不回滾時也修剪；比對只看 C 當時存活的實體；重跑後輸入放回這一幀的值 | 第 4 步審查 1-2、1-3、1-6 |
| 7 | — | 回滾的 `skipped` 名字表拿掉，「重跑時跑不跑」改由掛載的條件判（回滾公開 `RunsWhileResimulating` 等） | 審查 B2：同一件事兩份事實 |
| 8 | — | 同一型別的可回滾與 Derived 跨種類裁決（作者的蓋過代為的；兩筆作者的是 RB-E1） | 審查 B3 |
| 9 | — | 編排層：`ModuleBuffer` 與準備函式不再存在 World 的 slot 裡 | 審查 E8：和 O-2 的條文一致 |

## 6. 新型別與新碼（宣告 10.3：新型別照後綴取名，合併前給你看）

- `RollbackFailure`（E2：`NotImplemented`）、`AuthorityFailure`（E2：`NoAuthority`、`SourceOutsideRoot`）。
- 新碼：RB-E9（輸入歷史短於深度）、RB-E10（官方策略沒有步的 schedule）。
- 回滾的新型別與掛點：`InputHistoryLength`（`StrategyDepth`、`Short`、`Medium`、`Long`）、`RollbackStrategy::DefaultInputLength()`；權威的 `DataOps::recordInput`。
- 回滾的公開函式：`ResimulatedSchedules`、`IsSkipCondition`、`SkippedWhileResimulating`、`RunsWhileResimulating`。
- 契約頁的檔名 `authority-sources.zh-Hant.md`。

## 7. 沒做、記下的事

| 項 | 內容 | 為什麼沒做 |
|---|---|---|
| 常設守衛 | DI-A 2、R-3（Kernel 不認得權威與回滾）的常設掃描 | 綁實作的守衛，宣告 9.5 延到第 5 批開頭 |
| `gyo2` 上本來就壞的突變 10 個 | Kernel 的 7 個 stale（`el-kernel-print-back` 等）與 `ks-k8-skips-executor-files`；編排層 `or-failure-no-product`（等價）、`or-runner-logs-back`（缺含入） | 不是本分支的，列為後續 |
| 測試的註解 | Kernel 測試裡還有約 100 處註解提到區（不影響判定、不在判定 4 的目錄） | 低價值；碰到再改 |
| 回滾 Resource 不收權威值 | `RollbackInbox` 只有元件 | 宣告的範圍（Client 那一批） |
| digest 何時比 | 只在回歸測試與 scenario 用 | 延到網路 Module |

## 8. 審查 A3、A5 的處置（兩個 Architecture Delta）

### 第 1 題（已定 (a2)）：`Restore` 照給的內容放回，不補 `InitialValue`

**決定**：使用者 2026-11-22，原話「同意轉給任務卡」，回覆主會話的提案「(a2) 從源頭修 Restore：Restore 只放回快照的內容，不觸發 InitialValue；OnAdd 這類通知照跑，hook 讀得到『現在是重跑』。要在 Kernel 裡把『通知』和『補資料』這兩件事分開。這是 Kernel 機制的修改，算 Architecture Delta，要你同意。」比過的選項：(a1) 第 5a 步跳過 Restore 來的格（補丁，不採用）；(b) 寫成作者的前提（把缺陷推給作者，不採用）。主會話查證：`Restore` 把放回的格做成 Insert 紀錄、走立即變更的套用；第 5a 步不分來源對新格補 `InitialValue`；差異在套用前算好，途中補出來的格沒有人收回；兩邊的 owner 都是 Kernel。

**現象**（審查 A3 的探針，寫成測試）：可回滾的 `Req`、`Tgt`，`InitialValue<Tgt, FromReq>`。第 2 步插入 `Req`（補出 `Tgt`），第 4 步拿掉 `Tgt`，第 7 步拿掉 `Req`；第 9 步回滾到第 5 步（有 `Req`、沒有 `Tgt`）。改之前：放回 `Req` 時補出 `Tgt`，重跑後 `World::Summary` 和直跑不同；預測死亡被推翻、實體復活時也多出 `Tgt` 與它的 OnAdd。先紅（`step8-checks/a2-restore-initial-value-red.txt`）、後綠（`…-green.txt`）。

**Architecture Delta（AGENTS §3 的七項）**：

1. 需要它的：回滾 R-2（回滾等價）；Feature 是 Client 的預測與回滾（scenario 第 5 段的 S5-5 走同一條路）。
2. 現在的問題：Kernel 的 `Restore` 和 `InitialValue` 兩個機制互相干擾，放回的格被當成新的變更補資料。
3. 變的邊界：Kernel 的套用分成「套用與通知」和「從新格推導資料」兩件；套用的呼叫者說要不要推導（`Detail::Derive`）。`Restore` 原語的套用不推導；一般的立即變更、同步點、hook 回合照常推導。第 5a 步不認記錄的來源。
4. 影響：`engine/ecs/kernel`（`World::ApplySync`、`WorldExecutor::ApplyImmediateRecords`、`RestoreFrom`）；回滾 Module 不改程式；權威 Module 的漏斗（G-5、Z6 看的「`InitialValue` 會填的格」）不變——它看的是 System 的變更，不是放回。
5. 依賴方向：不變。
6. Ownership：不變（兩件都在 Kernel）。
7. 更小的改法為什麼不行：(a1) 在第 5a 步認 Restore 的格，等於讓推導的步驟知道誰呼叫它；(b) 把 Kernel 的缺陷推給作者。

**Kernel 的 DI 要改的**（[kernel.zh-Hant.md](../kernel.zh-Hant.md)）：

- **DI-KS 12** 加一段：「`Restore` 照給的內容放回，不推導資料：通知（OnAdd、OnRemove、observer）照跑，`Registrar::InitialValue` 不從放回的格補目標（放回的是當時的樣子，當時沒有的就沒有；復活的實體也一樣）；放回時 hook 記下的變更是一般的變更，照常補。Kernel 裡『套用與通知』和『從新格推導資料』是兩件事，由套用的呼叫者說要不要推導，第 5a 步不認記錄的來源。」守衛與突變一併列。（已照這段改成草稿。）
- 其他條文不改：DI-KS 18（當場改的做法）照舊；`InitialValue` 的條文（「同一個同步點就補好」）照舊，只對變更成立。
- 回滾頁 R-2：寫明這件不必當前提（已改）。
- 突變：`ks-restore-derives-initial-values`（Kernel 的案）、`rb-restore-derives-initial-values`（回滾的兩案），都 killed。

### 第 2 題（已定 (c)）：判定的輸入和它的紀錄綁在一起

**決定**：使用者 2026-11-22，經主會話轉。side chat 原話：「我有一個問題，按鍵屬於玩家操作，為何要rollback ？」「交叉比對個家方案。」「我的意思是，我把2個綁在一起，作者就不得不預測了」→「嗯，加過去吧。」主會話原話：「【rollback.Input<T>(n)】不需要讓作者直接填數字（作者可能不知道這些概念），可以預設一個Enum，分幾個檔位（檔位裡面是數字，讓作者直接調用這個Enum）。當然作者像直接寫數字，也不攔著。」比過的選項：(a) 新碼 AU-E10；(b) 寫進限界；(c) 綁在一起（採用，因此不需要 AU-E10）。對照：lightyear 的輸入 crate 自帶 InputBuffer；Unity Netcode 的 `IInputComponentData` 自動配 command buffer——兩家都是「宣告一次輸入，紀錄自動就有」。

**現象**（審查 A5）：作者宣告 `authority.Resource<Command>({.input = true})`、忘了 `rollback.Input<Command>(n)` 時，AU-E6、AU-E7、RB-E9 都不報，重跑的每一步都用最新的輸入。先紅：scenario 第 5 段拿掉作者那行後，S5-3、S5-5 的 digest 不同、生命值錯（`step8-checks/q2-input-history-red.txt`）；後綠（`…-green.txt`）。

**做了什麼**：

- 伺服器權威對 Resource 的 `.input` 代為登記 `rollback.Input<T>()`，走既有的代為登記路徑（`DataOps::recordInput`），拿掉原本特意跳過輸入的那一行；作者選 `Self`／`NoRollback` 時不代為登記。
- 長度：`rollback.Input<T>(InputHistoryLength)` 或 `Input<T>(步數)`。檔位 `StrategyDepth`（0，預設：準備 World 時取策略的深度，經新的 `RollbackStrategy::DefaultInputLength()`，官方策略回它的 `depth`）、`Short` 20、`Medium` 32、`Long` 64。系譜：`Short` 是預算 P1、P3 的深度與 lightyear 的預設回滾深度；`Medium` 是 P2 的深度；`Long` 是時間 Module 預設 64 Hz 的一秒。直接寫數字不攔；RB-E9 照舊（預設長度不會小於深度）。
- 零成本的約束：紀錄只在選了回滾策略的 World 建（R-4、DI-A 1），原本的機制已經這樣；這次寫進條文、用測試守（`[au-input-no-strategy-no-history]`：沒選策略的 World 宣告 `.input` 之後沒有 `RollbackHistory`）。
- 元件的 `.input`：回滾 Module 只記 Resource 的輸入，元件的照舊不代為登記、受 Z6 檢查（寫進權威頁）。

**放寬的地方**（§14.4 的緩的面）：以前作者要寫兩行（`.input` 與 `rollback.Input`），現在不必寫第二行；以前「沒寫第二行」在消耗它的步上會被 AU-E6 報出（作者寫了第二行才不報），現在預設自己帶紀錄、不報——守住它的性質（重跑時輸入是那一步的值）改由紀錄本身與 scenario 第 5 段守。`[au-z6-input-no-history]` 改成用 `Self`（作者不要代為登記、自己也沒登記）時照樣報 AU-E6。

**Architecture Delta**（AGENTS §3 的七項）：

1. 需要它的：Client 的預測與回滾（scenario 第 5 段）；審查 A5 的洞。
2. 現在的問題：輸入的宣告與它的紀錄分在兩個 Module 兩行，漏寫一行時靜靜地錯。
3. 變的邊界：權威 Module 代為登記的範圍多了輸入的紀錄（原本只代登記可回滾狀態）；`RollbackStrategy` 多一個有預設實作的掛點 `DefaultInputLength()`（R-1 的掛點清單加一項；不是純虛擬，作者的策略不必改）。
4. 影響：`engine/modules/authority`（`Registrations`）、`engine/modules/rollback`（`Registrations`、`RollbackStrategy`、`StateRollbackStrategy`、`RollbackModule`）。Kernel、編排層不變。
5. 依賴方向：不變（權威→回滾，本來就有）。
6. Ownership：紀錄仍由回滾 Module 擁有；權威只代為呼叫它的公開入口。
7. 更小的改法為什麼不行：(a) AU-E10 只是把漏寫報出來，作者還是得寫第二行；(b) 寫成限界等於把洞留給作者。

**要你核准的條文**：權威頁第 4 節的新一條（代為登記、零成本、元件不在內）；回滾頁第 2 節 `Input<T>(length)` 那一列（檔位與系譜）；R-1 的掛點多 `DefaultInputLength()`（回滾頁 R-1 的條文我照實作補一句，草稿）。突變：`au-input-history-not-recorded`、`au-input-self-recorded`、`rb-input-default-length-ignored`、`rb-e9-default-length-reported`，都 killed。

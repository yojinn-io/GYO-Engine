# 2026-11-23 網路 Module PR-2 合併前設計審查的修正（v8-arch-gyo2-net-module PR-2）

- 審查報告：主 checkout `build/evidence/gyo2-net-module/design-review-pr2-opus.md`（審查者 Opus／high，讀碼推得、未實跑）。
- 每一條先自己確認成立再修。缺陷照 §14.3：先寫會失敗的測試（Red），不改測試修實作（Green）。
- 使用者 2026-11-23 對 U1～U3 回「照建議」：U1 選 (c)、U2 選 (a)、U3 加計數與觀測項。三條一起做進這一輪。
- 實作者 Opus／high（和之前一樣）。

## 1. 每條的處理

| 條 | 確認 | 處理 | 測試（Red→Green） |
|---|---|---|---|
| B1 | 成立。實跑：隱藏期間拿掉的 `Speed` 回來了，`Digest` 不等 | 修。兩條路一起守：(1) 回滾端 S6 讓入口在退出生效後忘掉實體，舊值不會再放回；(2) 網路端：入口還記得的實體再生成時，生成沒帶的會回滾元件寫 `Removed`。不回滾元件：生成是整份，寫的時候拿掉清單外的 | `net.k3`「hidden and seen again」、「with losses」 |
| B2 | 成立（`toInbox` 只在 V＝R 時設） | 修：`Check` 回 `NT-E6`（U2 (a)：一直拒絕） | `net.replication`「K3 refuses … (NT-E6, B2)」 |
| S1 a | 成立。無損的 Link 就重現：生成後下一 tick 拿掉 `Tint`，回滾放回實體的那一幀又被寫回去 | 修：拿掉不回滾元件時，丟掉留著的那一個 wire | `net.k3`「hidden and seen again」的 `tintSoon` |
| S1 b | 成立。要有掉包與亂序才重現（種子 1、18、24） | 修：生成的不回滾值留到「回滾在 ≥ 生成 tick 的確認點決定過、實體在 World」才寫；留著的期間，後來的值也留著 | `net.k3`「with losses」 |
| S2／U1 | 成立 | 修，照 U1 (c)：歷史有就用歷史，沒有就用入口。欄的歷史多一個 `Kept`，策略只把「歷史有的」當成保留歷史 | `rollback strategy: R-8 third state` 的新一案 |
| S3 | 成立 | 宣告 D-7 與第 3 節表加註「第 5 步偏離，未經核准」，原文不改 | — |
| S4 | 成立（實測看得到約 38、動的約 10） | 情境改成 2.1；原本的結果照 §14.1 留在 step5 dev_log 並加註 | `net.bandwidth` 加情境的兩個檢查 |
| S5 | 成立 | 見第 3 節 | — |
| S6 | 成立 | 修：`Prune(oldest, C 的存活集合)` 忘掉退出已生效、存活集合沒有它的實體 | `rollback strategy: R-8 A:` 兩案 |
| M1 | 成立 | 修：實體刪除、整份隨結構送出、Resync 時，累積的優先度與等待的 tick 歸零 | 沒有專用的測試（診斷與插隊順序，§14.5 不加突變） |
| M2 | 成立 | 修：`kNetProtocolVersion` 升到 3 | G-3「版本不同」照舊用 `+1` |
| M3 | 成立 | 文件與註解更正出處（取概念、做法不同） | — |
| M4 | 成立 | 寫成限界：回滾沒有查深度的 API，這一步不加 | — |
| M5 | 成立 | 兩句標「閘口後新增、未經閘口」 | — |
| M6、M7 | 成立 | 寫進限界（net 第 14、13 節） | — |
| M8 | 第一點成立、第二點成立但走不到 | 第一點寫成限界；第二點修：`Restore` 讀不到入口時回錯誤，不吞掉 | — |
| U3 | — | `RollbackHistory::rollbacksForEntries`（明說進入的實體讓存活集合不同而回滾的次數）；net Replacement Criteria 加 RC-NT9 | `net.k3`「hidden and seen again」的 U3 檢查 |
| S2 以外的 U1 | — | rollback 第三態的條文改成 (c)，標「使用者 2026-11-23 核准（U1 (c)）」 | — |
| U2 | — | net 第 13 節：限界與待辦（有點名的功能需要時由狀態機 Module 提供轉換，會碰狀態機 DI 8） | — |

## 2. 缺陷的 Red 與 Green

- **Red**（修之前的程式，加了新的測試與計數）：`build/evidence/gyo2-net-module/pr2-review-fixes/red.log`。
  - `net.k3`「hidden and seen again」：`hiddenBodySame`、`!hiddenSpeed`（B1）、`!tintSoon`（S1 a）、`Digest` 失敗。
  - `net.replication`「NT-E6」：建得起來（B2）。
  - `rollback strategy`：S6 的三個檢查、U1 (c) 的一案失敗。
- **S1 b 的 Red 怎麼證明**：第一次寫的「with losses」在修之前也失敗，但失敗的是另一個實體。`tintsWrong` 把「兩邊都沒有 `Tint`」也算錯，第 61 tick 拿掉 `Tint` 的那個箱子在兩邊都沒有。
  - 改成只比「有沒有、值一不一樣」。對 G-10 等價：G-10 的箱子在伺服器一定有 `Tint`，所以不是放寬。
  - 檢查也改成只看閃爍的那個箱子。
  - 然後把 S1 b 的修正拿掉，跑種子 1～24，看哪些種子會重現。
    - 腳本：`pr2-review-fixes/probe.sh`。
    - 結果：`probe-24-seeds-{none,s1b,b1net,whole}.log`。
  - 修正都在時：24 個種子全過。
  - 拿掉 S1 b：種子 1、18、24 失敗。
  - 拿掉 B1 網路端的 `Removed`：種子 1、18 失敗。
  - 拿掉「整份」的拿掉：種子 1、18 失敗。
  - 測試固定用 1、18、24（選法寫在測試的註解）。
- **Green**：`green.log`、`green-k3.log`；全部 ctest 199／199（`ctest-all.log`）。

## 3. S5 的各項

- `[rb-resource-adopted]` 的兩個突變拆開：「differs」一案加 `[rb-resource-differs]`（有沒有回滾），`rb-resource-authority-ignored` 改指它；`rb-resource-not-put-back` 照舊指 `[rb-resource-adopted]`。step5 dev_log「第 4 步以前就共用」的敘述錯，已加更正註。
- `removedToInbox` 的守衛：`net.k3`「hidden and seen again」第 110 tick 拿掉看得到的箱子的 `Speed`；突變 `nt-k3-removed-not-to-inbox`。
- K3＋可見性：`net.k3` 兩案（隱藏、再看到；閃爍＋掉包）。
- 私人元件不再顯示：`net.visibility` 的新一案（第 100 tick 起不再 `ShowPrivate`，Client 1 的 `Secret` 拿掉、實體留著）。
- 「整份重送」：G-11 的 E2 多帶一個從不變的 `Mark`，再看到後要有它。
- 新加的兩個突變沒有共用 expect：見第 5 節。

## 4. 數字

- **頻寬判定（S4，照 2.1 的情境重跑；判定式不動）成立**：
  - 情境：看得到平均 57.54、動的 15.61（跑之前宣告的範圍是 40～60、10～18）。
  - 8 個 Client 的平均 207.64～219.73 kbps（≤ 512）；兩次執行逐 tick 相同。
  - 只記錄的部分：最大 4 360～4 416 B/tick；局部密集（同樣的實體全部在動）709.89～754.92 kbps、看得到約 57。
  - step5 那一次（較輕的情境：看得到約 38）是 164.38～183.69 kbps，照 §14.1 留在 step5 dev_log。
- **K3 隱藏、再看到**（無損）：P＝324，`Digest` 等於 Match；inbox 值 2 990、拿掉 1、進入 9、退出 2；值訊息收到 304＝處理 304；`rollbacksForEntries`＝8。
- **K3 閃爍＋掉包**（種子 1、18、24）：
  - 生成時之前那份還在 World 裡的次數 13、11、9。
  - 值訊息收到＝處理（320、327、315），待處理 0。
  - `Tint` 錯 0。
- **G-10**：P＝324，數字和第 5 步相同；三步一幀 P＝355、確認點跳 101 次；G-11 的 NotSent：延後 1 274＝`NotSent` 1 274。
- **G-11**：Client 1 的 E1／E2／E3 是 158／118／158 幀，Client 2 是 0／158／158，和第 5 步相同；再看到的 E2 帶著 `Mark`。私人元件不再顯示一案：Client 1 有 `Secret` 97 幀，最後沒有、實體還在。
- **G-8**：44 334 bytes（和第 5 步相同）。G-5 觀測 165、不一致 0；G-6 錯 0；G-9 觀測 42、不一致 0。
- **全部 ctest**：199／199。

## 5. 突變

照 D184：跑的是兩份目錄的全部，因為改到的檔與被改的測試涵蓋了大部分 net、rollback 的突變。

- **rollback 95／95 killed**：
  - 新加 4 個：`rb-not-sent-entered-empty`（U1 (c)）、`rb-exited-never-forgotten`、`rb-exited-forgotten-while-alive`（S6）、`rb-entry-rollbacks-not-counted`（U3）。
  - find 改寫 5 個（程式的位置變了，判定沒變）：`rb-authority-not-laid-over`、`rb-history-not-adopted`、`rb-inbox-not-pruned`、`rb-inbox-not-pruned-after-rollback`、`rb-inbox-not-pruned-on-abandon`。
  - expect 改 1 個：`rb-resource-authority-ignored` 改指 `[rb-resource-differs]`（S5 拆開）。
- **net 93／93 killed**：
  - 新加 6 個：`nt-k3-removed-not-to-inbox`（S5）、`nt-k3-reentry-removed-missing`、`nt-k3-whole-not-taken-off`（B1）、`nt-k3-spawn-written-early`（S1 b）、`nt-k3-waiting-removal-kept`（S1 a）、`nt-k3-resource-other-type-accepted`（B2）。
  - find 改寫 2 個：`nt-k3-entered-missing`、`nt-k3-waiting-not-written`。
- **第一次全跑 net 92／93**：`nt-k3-spawn-written-early` 存活。
  - 那時它拿掉的是「確認點 ≥ 生成 tick 才寫」。
  - 種子 1～120 都測不出這一條（`probe-120-seeds-notbefore-only.log`）。實際守住 S1 b 的是「之前那份還在時，生成的值也先留著」：拿掉它，120 個種子裡 29 個失敗（`probe-120-seeds-pending-only.log`）。
  - 照 §14.5（誤りが起きるか分からない細部は観測後に加える），突變改成拿掉測得到的那一條，重跑 killed。
  - 「確認點 ≥ 生成 tick」照推理留在程式裡（確認點在之後的幀才落到 [退出, 進入) 時需要它），沒有測試守，寫在第 7 節。
- **共用 expect**：新加的突變各指自己的檢查，沒有和別的共用。

## 6. 自己判的（揭露）

- **B1 兩條路都留**：S6 讓「退出已生效」的實體被忘掉，無損時網路端的 `Removed` 用不到。掉包時，入口還記得這個實體（退出還沒在確認點生效，或 C 跳過了 [退出, 進入)），這時要靠網路端的 `Removed`。種子 1、18 證明這條路有用。審查的 (b)（`Entered` 當屏障）會改 R-8 的文字，沒採。
- **S1 b 的「決定過」用網路自己上一幀寫的確認點**，不讀回滾的 `RollbackHistory::decided`。
  - 理由：網路不根據回滾持有的狀態做判斷（AGENTS §3 狀態的權威）。
  - 這樣夠的原因：回滾在同一幀的決定階段用網路寫的確認點，確認點只往前。
  - 走不到的情況寫成限界：回滾放棄時不放回實體，值一直留著。
- **NT-E6 放在 `Check`**：讀網路與回滾兩邊的登記表（`Resolve` 是純函式）。協調者說「Check／登記時」，登記時還不知道回滾的登記，所以選 `Check`。
- **U3 的計數放在 `RollbackHistory`**：「為什麼回滾」只有回滾知道，網路不知道。
- **S4 的情境調了兩次**（判定式沒動，都在跑之前宣告了範圍「看得到 40～60、動的 10～18」）：
  - 第一次：附近的物品 11 個。看得到 42.85、動的 15.61、平均 207.73～219.38 kbps。紀錄在 `bandwidth-try1-11-items-near.log`。
  - 第二次：附近的物品 18 個。看得到 57.54、動的 15.61。正式結果是這一次。
  - 兩次都在範圍內，第一次離 50 偏輕，所以取重的一邊。範圍是寫在跑之前的，選重的不是放寬。
  - 種子的序列跟著物品數變，所以看得到的數字跳了 15。之後不再調。
- **私人元件不再顯示一案沒有新突變**：拿掉元件走的是既有的「結構更新送 removed」，`nt-removed-not-encoded` 已守。

## 7. 範圍的限界

- 本機 macOS x86_64、`core` preset。頻寬算位元組，平台不影響。
- S1 b、B1 網路端的情況只在有掉包、亂序的種子重現。種子以外的組合沒有逐一證明。
- M1 沒有專用的測試。
- S1 b 的「確認點 ≥ 生成 tick 才寫」沒有測試守（種子 1～120 都觀測不到它單獨的效果），照推理保留。
- S6 之後，比退出更早的值晚到時，入口會再為它留一筆（寫在 net 第 13 節的限界）。

## 8. 證據

主 checkout `build/evidence/gyo2-net-module/pr2-review-fixes/`（git 不追蹤）：`red.log`、`green.log`、`green-k3.log`、`probe.sh`、`probe-24-seeds-*.log`、`probe-120-seeds-*.log`、`bandwidth-try1-11-items-near.log`、`bandwidth-official.log`、`ctest-all.log`、`net-messages.txt`、`mutations-rollback.log`、`mutations-net.log`。

## 之後要接的

- 合併核准頁要列：S3 的兩項偏離（看不到＝`Exited`、`NotSent`＝預算延後；值訊息的延後清單）、M5 的兩句、U1 (c) 的第三態條文、`NT-E6`、S6 的入口遺忘、`kNetProtocolVersion` 3。
- U2 (a) 的待辦：有點名的功能需要 K3 上 V ≠ R 的會回滾 Resource 時，由狀態機 Module 提供轉換（會碰狀態機 DI 8）。
- RC-NT9 的觀測項：`rollbacksForEntries`。

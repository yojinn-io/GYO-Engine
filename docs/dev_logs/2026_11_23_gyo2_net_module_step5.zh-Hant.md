# 2026-11-23 網路 Module 第 5 步：相關性、優先度、回滾的 NotSent（v8-arch-gyo2-net-module PR-2）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-pr2`（第 4 步 b28565fa 之後；PR #290 draft）。
- **事前宣告與核准**：
  - [`net-module-pr2-gates.zh-Hant.md`](../architecture/target/v2/net-module-pr2-gates.zh-Hant.md) 閘口 2 與第 3 節：典型情境 8 人、看得到約 50、每 tick 變約 14；預算 (b) ≤ 512 kbps；量法照 2.3。
  - 宣告 F4、F7、F19、C-VIS1、C-VIS2、D-5、D-7（`NotSent`）、判定 G-11。
- **實作者**：Opus／high。

件數の突き合わせ：一致（G-11：結束時每個 Client 的實體剛好是它的 Peer 看得到的；K3 的 NotSent：伺服器延後 1 274 筆＝Client 寫的 NotSent 1 274 筆；G-10 的件數照舊）

## 1. 做了什麼

- **可見性**（`Replication.hpp` 的 `PeerVisibility`，伺服器的 Peer 實體上）：
  - 作者的 System 照自己的規則填（replicon 的 `ClientVisibility` 的形狀）。
  - `everything`（預設 true）、`entities`（遞增）、`Show`／`Hide`／`Sees`；私人元件 `ShowPrivate<T>`／`HidePrivate<T>`／`SeesPrivate`。
  - 沒有它的 Peer 看得到全部。
- **元件範圍**：`Replicate<T>(名字, codec, ReplicateOptions{.privateComponent = true})`，只送給 `ShowPrivate<T>(entity)` 的 Peer。
- **送件**（`ReplicateOut`）：每個 Peer 的遮罩＝快照的遮罩減去看不到的（整個實體）與沒給的私人元件。
  - 遮罩變 0（看不到）就送刪除（只用 `WhileVisible`）；再看到時是生成，同號整份。
  - Client 端沿用第 2、4 步的寫法：K1、K2 `SpawnWithId`／`Despawn`；K3 `Entered`／`Exited`。
- **優先度**：`NetEntry::ValueBytesPerTick(bytes)`（預設 0＝不限）。
  - 超過時照 lightyear 的 priority accumulator：分數＝累積＋權重（`NetPriority::weight`，預設 1），高的先放，第一個一定放。
  - 放不下的累積起來（`PeerLedger::waiting`），送出後歸零。
  - 診斷用 `waitedTicks`、`longestWait`。
- **延後的告訴 Client**：值訊息的每一則在 Resource 之後多一段（`u16` 個數＋編號，只有第 0 則有內容）。K3 寫 `NotSent(entity, tick)`。
- **回滾的 `NotSent`**（閘口 1 的 C）：
  - `RollbackInbox::NotSent(entity, tick)`、`NotCarriedAt(tick)`。
  - 官方策略在 C 把這些實體從比對拿掉；還原時它們用這個 World 自己的歷史（`AddTo` 多一個 `notCarried` 參數），也不採用權威值。
  - 修剪丟比 C 舊的。
  - `rollback checks: the entries not made…` 拿掉 `NotSent`。
- **文件**：
  - [`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md)：新的第 14 節、NT-1、NT-10。
  - [`rollback.zh-Hant.md`](../architecture/target/v2/rollback.zh-Hant.md)：第 2 節、R-8 第三態、限界、守衛。
  - 都標「實作，合併前核准定稿」。

## 2. K3 對看不到的實體用 `Exited`，不用 `NotSent`（R-8 的語意）

- 宣告選的是「失去可見＝刪除」（只用 `WhileVisible`）：看不到的實體在 Client 上不該存在，所以是退出（`Exited`），再看到是進入（`Entered`）。
- `NotSent` 是「這份沒帶、實體還在」：對看不到的實體用它，實體會留在 Client 上、值停在舊的，和 `WhileVisible` 不合。
- 所以 `NotSent` 用在「實體看得到、但這個 tick 的值因為預算延後」：這時 R-8 B 的前提（寫入的一方寫出 C 當時的值）不成立，要明說「這份沒帶」。

## 3. 測試

- **G-11**（`net.visibility`，判定照宣告）：伺服器加兩個 K1 Client。實體 E1、E2、E3 每步都動，E3 帶私人元件 `Secret`。
  - user 1 看得到 E1、E3、E2（伺服器 tick 40～79 除外）與 E3 的 `Secret`；user 2 看得到 E2、E3、看不到 E1、沒有私人元件。
  - 逐幀記下兩個 Client 有哪些實體、E3 有沒有 `Secret`。
- **G-11 的 NotSent**（`net.k3`）：第 4 步的 2D 樣本，伺服器每 tick 給這個 Peer 90 bytes 的值。
  - 檢查延後的件數＝Client 寫的 `NotSent` 件數。
  - 檢查 Client 第 P 步的 `Digest` 照樣等於 Match 的，沒有把 `NotSent` 當成「沒變」。
  - 檢查最長等待 ≤ 8 tick。
  - 計數之前，Client 單獨多跑 6 幀（伺服器停），讓已送出的都到。
- **頻寬判定**（`net.bandwidth`，閘口 2 的 2.3，事前宣告、不放寬）：
  - 500 m × 500 m 兩層、固定種子。
  - 實體：玩家 8 個（第 0 層扎堆、每秒換方向、離中心 30 m 內）、AI 20 個（兩層，來回 20 m）、Boss 1 個（第 0 層，5 m 的方形）、物品 200、門 50、其他 129（兩層，不動）。
  - 規則：同層、半徑 100 m。
  - 8 個 K1 Client，MemoryLink 不掉包，連上之後量 2 000 tick；算 Match 送給每個 Client 的全部 datagram 的位元組。
  - 局部密集情境只記錄：其他 129 個裡有 42 個改成在玩家附近巡邏。
- **第 4 步的提醒**（§14.2：預期的失敗要認得出是哪個突變引起）：K3 的五個突變原本共用 G-10 的 `Digest`。G-10 加了各自的檢查，五個突變的 expect 各指向自己的那一條；`Digest` 的判定照舊。
  - 伺服器生成的實體到了；
  - 確認點寫進回滾；
  - 回滾的值沒在 `First` 直接寫（`unchangedNotWritten` 為 0）；
  - 回滾的 Resource 沒走 `apply`（`resourceValuesApplied` 為 0）；
  - 進入的件數。

## 4. 數字

- **頻寬判定成立**（第一次正式跑就過，沒有重跑；兩次執行每 tick 的位元組逐項相同）〔PR-2 設計審查 S4 註：這個情境比閘口 2 的 2.1 輕（看得到約 38、每 tick 動的約 10，2.1 是約 50、約 14）；本節的結果照 §14.1 留著，照 2.1 的情境重跑見 `2026_11_23_gyo2_net_module_pr2_review_fixes.zh-Hant.md`〕：
  | Client | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
  |---|---|---|---|---|---|---|---|---|
  | 平均（kbps，判定 ≤ 512） | 181.06 | 181.51 | 180.92 | 181.03 | 165.13 | 171.10 | 183.69 | 164.38 |
  | 最大（B/tick，只記錄） | 4 544 | 4 544 | 4 556 | 4 566 | 4 522 | 4 511 | 4 567 | 4 499 |
  | 最後看得到的實體 | 38 | 38 | 40 | 39 | 33 | 37 | 38 | 40 |
  - 局部密集（只記錄）：平均 683.99～704.57 kbps、最大 5 129～5 186 B/tick、看得到 72～78 個。超過 512 kbps，作者要靠預算（`ValueBytesPerTick`）或較窄的規則。
- **G-11（可見性）成立**：Client 1 的 E1／E2／E3 在 158／118／158 幀、`Secret` 158 幀；Client 2 是 0／158／158、`Secret` 0。E2 在 Client 1 消失後以同一個 id 回來（true／true）。最後每個 Client 恰好是它的 Peer 看得到的實體。這個案例沒設預算，延後 0。
- **G-11（NotSent）成立**：伺服器延後 1 274 件＝Client 寫的 `NotSent` 1 274 件；Client 第 P＝324 步的 `Digest` 等於 Match；最長等待 ≤ 8。inbox 的值 1 236（沒預算時 3 784）、進入 8、離開 1、確認點 313；值訊息收到 304＝處理 304。
- **G-10**：P＝324，inbox 值 3 784、進入 8、離開 1、確認點 313；值訊息收到 304＝處理 304、待處理 0；丟棄全為 0。三步一幀：P＝355、確認點跳 101 次。
- **G-8**：判定成立；合計 44 334 bytes（之前 44 078，多 256＝128 則值訊息各多 2 bytes 的延後清單），每 tick 最大 628、平均 341.0。
- 其他照舊：G-5 觀測 165 個 tick、不一致 0（之前 164：位元組變了，有損 Link 的抽樣跟著變）；G-6 比對 404、錯 0；G-9 觀測 42、不一致 0、連線識別不符 5。
- **全部 ctest**：199／199 通過（`net.visibility`、`net.bandwidth` 新加，都在 `cpu` 標籤；`net.bandwidth` 本機 4.7 秒）。

## 5. 突變

照 §14.5：每個突變寫了它才抓得到的錯與守的對象；預期的失敗都指向那個突變自己的檢查（§14.2）。

- **rollback ＋3，91／91 killed**：
  - `rb-not-sent-compared`（第 C 步照樣比對 `NotSent` 的實體）→ `[rb-not-sent-not-compared]`；
  - `rb-not-sent-restored-from-inbox`（回到 C 時拿 inbox 的值蓋掉自己的歷史）→ `[rb-not-sent-history-kept]`；
  - `rb-not-sent-at-any-later-tick`（`NotSent` 在 C 之後的每一步都不比）→ `[rb-not-sent-only-at-its-tick]`。
  - `rb-authority-not-laid-over`、`rb-history-not-adopted` 的 find 跟著 `Restore` 的新寫法改寫（改的是程式的位置，不是判定），重跑照樣 killed。
- **net ＋7，87／87 killed**：
  - `nt-vis-ignored`（不看 Peer 的可見性，全送）、`nt-vis-loss-not-removed`（看不到之後不刪）、`nt-private-to-all`、`nt-private-to-none` → G-11 各自的檢查；
  - `nt-budget-ignored`（不管預算）→ 「values over the budget wait」；
  - `nt-priority-not-accumulated`（延後的不累積）→ 最長等待 ≤ 8；
  - `nt-deferred-not-notsent`（K3 收到延後清單不寫 `NotSent`）→ 「the simulating client does not take NotSent as unchanged」。
  - 第一次全跑 85／87：`nt-k3-not-confirmed` 與 `nt-deferred-not-notsent` 是 wrong_failure（被殺了，但不是被預期的檢查）。
    - `nt-k3-not-confirmed`：只拿掉寫 inbox，`inboxConfirmed` 計數照樣加，原檢查看計數所以沒抓到、別的檢查失敗。檢查改成讀 Client 的 `RollbackInbox::Confirmed()`（真正的效果），重跑 killed。
    - `nt-deferred-not-notsent`：計數照樣加、件數相等，失敗的是 `Digest`。expect 改指 G-11 NotSent 案例的 `Digest` 檢查（這個案例只有它會壞），重跑 killed。
- **第 4 步的提醒（共用 `Digest` 的 expect）**：K3 的五個突變拆成各自的檢查（見第 3 節），重跑都 killed：`nt-k3-entered-missing`→Entered 到了、`nt-k3-not-confirmed`→確認點寫進回滾、`nt-k3-as-k1`→進入的件數、`nt-k3-values-direct`→`First` 沒直接寫、`nt-k3-resource-direct`→Resource 進 inbox。K3 的突變不再共用同一條檢查。
- **第 4 步以前就共用 expect 的**（不在這一步改，列給之後看）：net 有 5 組（`nt-magic`／`nt-version`；G-5 (1) 的 3 個；G-6 的 2 個；B1 的 2 個；R2 a 的 2 個），rollback 有 3 組（`[rb-r2-equal]` 的 4 個；`[rb-history-pruned]` 的 2 個；`[rb-resource-adopted]` 的 2 個）。〔更正，PR-2 設計審查 S5：`[rb-resource-adopted]` 的兩個（`rb-resource-authority-ignored`、`rb-resource-not-put-back`）是 PR-2 第 4 步新增的，不是「第 4 步以前」；`origin/gyo2` 沒有這個標記。已在審查修正拆開，見 `2026_11_23_gyo2_net_module_pr2_review_fixes.zh-Hant.md`〕拆的話要多加檢查（例如 G-5 (1) 分出「值早於結構」「刪除沒編碼」「舊值被套用」），是否值得照 §14.5 判。

## 6. 自己判的（揭露）

- **可見性的形狀**：作者填 Peer 上的集合（replicon 的 `ClientVisibility`），不做 replicon 的「可見性篩選元件」。篩選元件要另一套登記；作者的 System 一樣表達得出來，產品真的有多種篩選時再看（§1：單一用途不另立抽象）。
- **私人元件用登記時的旗標＋Peer 上的白名單**：F4 是「只給開啟者」，預設不給。
- **優先度選 lightyear 的 accumulator**，不選 replicon 的 `priority × tick 差`：後者限的是頻率，不是「一個 tick 放不下時分到後面」。
- **預算的位置**：`NetEntry::ValueBytesPerTick`（登記時）。放在 `NetWorldOptions` 會讓現有的聚合初始化都多一個欄位。預算只算值的實體部分，結構更新（可靠）與伺服器訊息不算。
- **延後的清單放進值訊息**：K3 要知道哪些實體在這個 tick 沒帶，才能遵守 R-8 B 的前提（`NotSent`）。每則多 2 bytes，G-8 的數字跟著變。
- **最長等待的上限 8**：樣本裡每 tick 約 2～3 個實體放得下、約 8 個在動，累積的優先度讓每個實體幾 tick 內輪到。8 是寬鬆的上界，只在 NotSent 的案例裡用來抓「不累積就有實體一直等」。

## 7. 範圍的限界

- 本機 macOS x86_64、`core` preset。頻寬是算位元組，平台不影響。
- 頻寬的情境是合成的，沒有實際玩家的移動資料；之後有產品時換成實際的情境再量（AGENTS §2）。
- 可見性的規則每 tick 由作者重算；伺服器不做空間索引（作者的事）。

## 8. 證據

主 checkout `build/evidence/gyo2-net-module/step5/`（git 不追蹤）：`bandwidth-run1.log`（第一次正式跑的輸出）、`ctest-all.log`、`net-messages.txt`、`mutations-rollback.log`、`mutations-net.log`。

## 之後要接的

- 合併前核准：net 第 13、14 節、rollback 的 A～G 與第三態的條文定稿。

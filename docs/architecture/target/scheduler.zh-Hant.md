# Scheduler

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §D（Scheduler）、§G.1（區規則）。
本文件只寫**使用者已裁決**的部分；規格中仍標「待使用者」的條目不在這裡。

## 1. 衝突邊與順序邊分開；歧義的處置（mechanism §3.2 審查修正 1、2；主張 A）

- 衝突邊只說「不能同時」（同 id 且至少一方寫），順序邊說「先後」（before／after、stage 界線、同步點）。Scheduler 不假設業務順序。
- 有衝突、無順序路徑＝歧義：任一方 zone=C → **啟動錯誤 D-E8**，要人寫 before／after；兩方都 zone=B → 依登記序平手，以診斷等級列出。Kernel 不提供歧義豁免。
- 資料衝突本身不是錯誤：有順序路徑的衝突對序列化執行，先後由順序邊決定。

## 2. 讀 (c) 資料的 (b) System 必須與 (c) 寫者有顯式順序（使用者 2026-11-11 決定，spec K.3 第 5 項）

- 區規則下 (c) 與 (b) 的 System 互相衝突只可能是「(b) 讀 (c) 資料」（呈現讀模擬）。這種衝突照第 1 節處理：任一方是 C 就是錯誤，所以呈現 System 必須與 (c) 寫者有顯式順序，或位於不同 stage。
- **配套慣例是限制**：呈現放在模擬之後的 stage，一條 stage 界線就給了全部順序，不必逐個寫 before／after。成本落在呈現 System 的數量（每個讀 (c) 的呈現 System 都要落在正確的 stage）；產品線在遷移時確認。修改條件與解除方式見 `limits.zh-Hant.md`。

## Design Invariants（候選；核准後成為正式條文）

條號不變。標「受 kernel DI-KS n 約束」的條，全文只在 kernel 頁的那一條；其餘是本部件特有的條文。

1. **衝突與順序分開存**——受 kernel [DI-KS 5](kernel.zh-Hant.md#di-ks-5) 約束：**全文與守衛見該條，改動前必讀**。
2. **(c) 的歧義是啟動錯誤，(b) 依登記序平手，無豁免**——受 kernel [DI-KS 5](kernel.zh-Hant.md#di-ks-5) 約束：**全文與守衛見該條，改動前必讀**。
3. **Plan 唯一且建一次**——受 kernel [DI-KS 3](kernel.zh-Hant.md#di-ks-3) 約束：**全文與守衛見該條，改動前必讀**。

### Replacement Criteria（候選）

- LIM-2：v1 只有 Serial，任何 Parallel 宣告是啟動錯誤；解除條件＝量到 tick 超預算且有逐位元等價的守衛（spec J-27）（詳細在 `limits.zh-Hant.md`）。
- LIM-1：(b) 讀 (c) 要顯式順序；為消除 D-E8 加的順序宣告一半以上與資料流無關 → 區劃錯了，重審區劃（詳細在 `limits.zh-Hant.md`）。
- **第二種排序機制**——受 kernel [RC 4](kernel.zh-Hant.md#rc-4) 約束，改動前必讀該條。

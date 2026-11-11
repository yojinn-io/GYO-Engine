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

1. **衝突與順序分開存**：Plan 的建立只讀兩種邊；衝突邊不隱含先後。守衛：spec J-21、J-22。
2. **(c) 的歧義是啟動錯誤，(b) 依登記序平手，無豁免**：守衛 spec J-22、J-23、J-44（(c) System 登記順序的排列測試：要嘛全部 D-E8，要嘛結果不變）。
3. **Plan 唯一且建一次**：只取決於登記序與宣告，不依雜湊、位址、執行緒、平台；Running 期間不重建。守衛：spec J-20。

### Replacement Criteria（候選）

- LIM-2：v1 只有 Serial，任何 Parallel 宣告是啟動錯誤；解除條件＝量到 tick 超預算且有逐位元等價的守衛（spec J-27）（詳細在 `limits.zh-Hant.md`）。
- LIM-1：(b) 讀 (c) 要顯式順序；為消除 D-E8 加的順序宣告一半以上與資料流無關 → 區劃錯了，重審區劃（詳細在 `limits.zh-Hant.md`）。
- 為表達需求必須引入依賴求解以外的排序（優先權數字、依資料值決定順序、執行期改 Plan）→ 第二種排序機制，停下（spec K.2 第 4 條）。

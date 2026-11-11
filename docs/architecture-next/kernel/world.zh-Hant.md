# World（目標架構，尚未實作）

更新：2026-11-11。Owner：架構（架構師會話）。**狀態：目標架構，尚未實作。** 規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §A（World）、§F（延後佇列、hook、事件）；概念基準：`mechanism.zh-Hant.md` §2、§3.3。
本文件只寫**使用者已裁決**的部分與它們的後果；規格中仍標「待使用者」的條目不在這裡。

## 1. 實體的鍵（使用者 2026-11-11 決定，spec K.3 第 1 項）

- **格式是 Kernel 的**：`EntityId` 是一個 `uint64`，低 32 位 index、高 32 位 generation（照 Bevy 的封裝格式）。
- **Kernel 不看兩段的意義**：World 的疊代順序＝整個 64 位元值的數值升冪；欄的 `keys` 依這個值嚴格升冪、無重複；多欄查詢是對各欄 keys 的 merge-join，輸出同樣升冪。
- **配發策略是產品的 Resource**（說明名 `EntityAllotter`，由產品 plugin 登記）。World 本身不配發 id、不保留任何值。
  - GYO PvP 選「**單調不重用、generation 恆 0**」：產生的值與今天的 `EntityId`（E-1～E-3：Gateway 配玩家段、PvpMatch 配 Match 段、Client 不配發）**逐位元相同**，wire 與 digest 不改；Match 與 Client 的 World 對同一組實體產生同一疊代順序，(c) 區 System 不必自己排序。
  - 別的產品可選「槽回收加版本號」。代價自付：兩端配發歷史不同時，數值順序不再是契約鍵的順序，需要該產品自己排序；跨機器一致性也由該產品保證。
- **後果（寫明）**：stale 句柄的偵測能力取決於配發策略。Kernel 的 debug 核對只保證「鍵還活著」（句柄記錄取得時的欄 epoch 與 run id，解參考時比對）；不重用的策略下，被刪除的 id 永不再出現，所以不需要 generation 來偵測；槽回收策略下，generation 才承擔偵測。
- 理由：鍵格式是 Kernel 的、配發策略是產品的——Engine 保通用、PvP 保決定性。

## 2. change stamp（使用者 2026-11-11 決定，spec K.3 第 2 項）

- 每格一個 `uint64` 的 `added` 與 `changed`。stamp 由 World 依 **Plan 位置**取值：每一個 Plan 步驟（System 或同步點）開始時取下一個值；條件為假被跳過的 System 也消耗它的值。取值依 Plan 位置而不依完成先後，所以 Parallel 與 Serial 相同。
- stamp **單調、不重置、不回滾、跨世代（World 重建）不比較**。溢出無實際可能（u64）。
- tick 的視角由 `tickStamps[domain]`（有上限的環形表：tick → 該 tick 第一個 stamp）換算；複製層以 `changed > tickStamps[T+1] − 1` 判「比 Ledger 確認的 tick T 新」。
- **stamp 不是 (c) 資料**：不進回滾快照、不回滾；(c) 區 System 不得用它做變更過濾（spec G-3：Match 執行一次與 Client 回滾重跑時「上次執行之後的變更」不同，兩端的 stamp 毫無關係）。
- 理由（為什麼不用遊戲 tick）：只用 tick 時，同一 tick 內排在讀者之後的寫入會被讀者永久漏掉（`T > T` 不成立），用 `>=` 則重複讀到。
- 連動修改：blueprint DI-K5 與 networking §1 第 7 條原寫「以遊戲 tick 標記」，已各改一句。

## 3. 其餘（照規格 §A，待實作時搬進本文件）

資料模型（欄的 SoA 佈局、ColumnInfo 的 zone／writePolicy／role／rollback）、A.2 的操作與允許的狀態、A.4 參考失效規則、A.5 不變量 W-1～W-11、A.6 的 C++ 實現手段與 debug 核對兩層，見 spec §A。這些在規格核准前仍是草案。

## Design Invariants（候選；核准後成為正式條文）

1. **鍵的疊代順序只來自數值**：World 對 `EntityId` 只做 64 位元整數比較，不解讀 index／generation；任何疊代、交集、同步點套用的順序都以此為鍵。守衛：spec J-1、J-2。
2. **World 不配發、不重用鍵**：配發器是產品的 Resource；World 內沒有配發、回收、歸零的操作；世代切換＝重建 World。守衛：spec J-8（含突變「用盡時回繞」「加 Reset 歸零」）。
3. **stamp 依 Plan 位置取值、單調、不進 (c)**：跳過的 System 也消耗；不回滾；(c) 區禁變更過濾。守衛：spec J-30、J-38。

### Replacement Criteria（候選）

- 某個產品需要 World 解讀 generation（例如由 World 拒絕 stale 鍵），或需要第二個鍵空間的對照表 → 鍵的形狀要重新設計，不在 World 上加分支。
- 同一 tick 內需要「本 tick 之後的變更」以外的變更過濾語義且無法以 `tickStamps` 區間查詢表達 → stamp 模型重審。

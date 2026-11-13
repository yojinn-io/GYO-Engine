# World

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §A（World）、§F（延後佇列、hook、事件）；概念基準：`mechanism.zh-Hant.md` §2、§3.3。
本文件只寫**使用者已裁決**的部分與它們的後果；規格中仍標「待使用者」的條目不在這裡。

## 1. 實體的鍵（使用者 2026-11-11 決定，spec K.3 第 1 項）

- **格式是 Kernel 的**：`EntityId` 是一個 `uint64`，低 32 位 index、高 32 位 generation（照 Bevy 的封裝格式）。
- **Kernel 不看兩段的意義**：World 的疊代順序＝整個 64 位元值的數值升冪；欄的 `keys` 依這個值嚴格升冪、無重複；多欄查詢是對各欄 keys 的 merge-join，輸出同樣升冪。
- **配發策略是產品的 Resource**（說明名 `EntityAllotter`，由產品 plugin 登記）。World 本身不配發 id、不保留任何值。
  - GYO PvP 選「**單調不重用、generation 恆 0**」：產生的值與今天的 `EntityId`（E-1～E-3：Gateway 配玩家段、PvpMatch 配 Match 段、Client 不配發）**逐位元相同**，wire 與 digest 不改；Match 與 Client 的 World 對同一組實體產生同一疊代順序，(c) 區 System 不必自己排序。
  - 別的產品可選「槽回收加版本號」。代價自付：兩端配發歷史不同時，數值順序不再是契約鍵的順序，需要該產品自己排序；跨機器一致性也由該產品保證。
- **後果（寫明）**：stale 句柄的偵測能力取決於配發策略。Kernel 的存取驗證（可關，違規以 Err 回傳）只保證「鍵還活著」（句柄記錄取得時的欄 epoch 與 run id，解參考時比對）；不重用的策略下，被刪除的 id 永不再出現，所以不需要 generation 來偵測；槽回收策略下，generation 才承擔偵測。
- 理由：鍵格式是 Kernel 的、配發策略是產品的——Engine 保通用、PvP 保決定性。

## 2. change stamp（使用者 2026-11-11 決定，spec K.3 第 2 項）

- 每格一個 `uint64` 的 `added` 與 `changed`。stamp 是 World **依 Plan 順序推進的全域計數**（跨同一 World 的所有 schedule 單調）：每一個 Plan 步驟（System 或同步點）開始時取下一個值；條件為假被跳過的 System 也消耗它的值。取值依 Plan 順序而不依完成先後，所以 Parallel 與 Serial 相同；不是 (c) 資料。
- stamp **單調、不重置、不回滾、跨世代（World 重建）不比較**。溢出無實際可能（u64）。
- tick 的視角由 `tickStamps[domain]`（有上限的環形表：tick → 該 tick 第一個 stamp）換算；複製層以 `changed > tickStamps[T+1] − 1` 判「比 Ledger 確認的 tick T 新」。
- **stamp 不是 (c) 資料**：不進回滾快照、不回滾；(c) 區 System 不得用它做變更過濾（spec G-3：Match 執行一次與 Client 回滾重跑時「上次執行之後的變更」不同，兩端的 stamp 毫無關係）。
- 理由（為什麼不用遊戲 tick）：只用 tick 時，同一 tick 內排在讀者之後的寫入會被讀者永久漏掉（`T > T` 不成立），用 `>=` 則重複讀到。
- 連動修改：blueprint DI-K5 與 networking §1 第 7 條原寫「以遊戲 tick 標記」，已各改一句。

## 3. 寫入政策：預設獨占（使用者 2026-11-11 決定，spec K.3 第 3 項）

- 每個欄與 Resource 的 `ColumnInfo.writePolicy ∈ {Exclusive, Shared}`，**預設 Exclusive**；登記時標 `Shared` 才允許多寫者。宣告進 authority digest 的欄禁 Shared（spec S-12）。
- **獨占是啟動時數寫者的檢查**（spec D-E2：同一 World、所有非 Startup schedule 中，寫者**以 System 計**（使用者 2026-11-11 決定，K.3 第 9 項；不以 owner plugin 計，否則檢查的粒度變成程式碼組織而非資料流）、含型別化 Commands、Spawn 的 bundle、hook，至多一個；Startup schedule 的寫者不計）。後果：PvP 今天由三段程式寫的 Body 要依產品自己的欄位分類拆欄（分類表在產品的目標文件 `docs/object_fps_pvp/target/entity-model.zh-Hant.md` §1，不在本文件——產品的欄位不進 Engine 層的設計文件），不併 System、不改 Shared——使用者：不拆的話架構改造就沒有意義。範圍與 golden 影響由 -audit 量，**執行期零成本、不是鎖**；Exclusive 與 Shared 的執行期行為完全相同。Bevy 等於全 Shared、順序隱性；本機制把「最後寫贏」換成「唯一寫者定合併規則」。不採資料庫式的執行期仲裁（鎖、MVCC）：寫入路徑慢 20～100 倍且買不到決定性。
- **資料路徑**：獨占欄預設由它唯一的寫者直接寫，不需要事件。只有多個來源要影響同一欄時才用漏斗——各來源發事件或寫自己的中間欄，唯一寫者讀取、套用合併規則、寫欄；漏斗可以是事件也可以是中間欄，是可選的路徑（使用者更正：不是標準進料方式）。事件也有寫者，(c) 區多個 `EventWriter<E>` 之間仍要顯式順序。整欄替換（`Restore`）只用於跨 World 接收權威狀態，不是 World 內的寫法。

| 資料 | 寫入政策 | 資料怎麼進來 | 為什麼 |
|---|---|---|---|
| 玩家狀態（位置、血量） | 獨占，進 digest | 事件漏斗加中間欄：Movement 寫 Velocity，Integrate 寫 Position；Damage 發事件，ApplyHealth 寫 Health | 權威判定，要可回滾、可比對；合併規則寫在唯一寫者裡 |
| 大量非主要場景物（可破壞木門、散落物） | 獨占（Match 端一個 System 管全部） | Match 端同一個 System 批次寫；到 Client 走複製與整欄替換 Restore，不逐物件發事件 | 數量大、規則簡單，一個 System 疊代整欄最便宜；Client 不預測它們，整欄覆蓋就好 |
| 唯一狀態（按鈕開關、Boss 擊殺歸屬） | 獨占，進 digest | 單一 System 寫，輸入來自事件（誰按了、誰補刀） | 權威只有一個的典型；擊殺歸屬是判定，多寫者會兩人同時拿到 |

## 4. 結構變更的套用（使用者 2026-11-11 決定，spec K.3 第 4 項）

- 生成、刪除、加欄、移欄在 System 執行期間只進延後佇列；Running 期間成員不變（spec W-3）。
- **Despawn 不算獨占寫者、不產生衝突邊**：它是成員關係的結束，不是值的寫入；同一窗內的查詢結果不受 despawn 記錄影響。(c) 資料的保護由區規則 Z5（帶 (c) 欄的實體只能由 (c) 的 System 刪除；記錄當下查、以 Err 回傳，F.2 第 2 步再複核本窗 (c) 的待套用 insert／spawn）。
- **同步點的套用順序寫死**（spec F.2）：hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource。despawn 階段依 EntityId 升冪、冪等（同一 id 記兩次只處理一次）、對窗開始時不存在的 id 無作用。despawn 排在 spawn 之前，是為了同一窗內「刪 X 再以同 id 生成 X」（重連）成立。
- 說明案例（使用者）：同一 tick 玩家 10 中彈死亡又退出。Combat 在 tick 內已寫下玩家 7 的擊殺——結構凍結，玩家 10 仍在；同步點才刪它。所以擊殺成立，而且與 despawn 的排序無關。若產品要「退出者不算被擊殺」，那是 Combat 與 ApplyMembership 兩個 System 的 Plan 順序與規則問題，(c) 區的歧義檢查會強迫把順序寫明，不是 despawn 該管的。

## 5. 事件只在該 domain 的 clock schedule 內讀寫（使用者 2026-11-11 決定，spec K.3 第 15 項）

- 每個事件型別登記時屬於一個 clock domain；寫者與讀者都必須在該 domain 的 **clock schedule** 中（含 hook：所屬同步點在該 schedule 內）。同 domain 的其他 schedule 讀或寫事件＝啟動錯誤 F-E1（spec F-9）。跨 schedule、跨 domain 的傳遞用元件或 Resource，或 Host 的交換點。
- 理由：「schedule 在 domain 內的序」沒有定義、Host 也沒被約束照它執行，另一個 schedule 若在修剪點之後執行就會靜默漏讀；與其漏讀不如禁止。
- 讀取規則無游標：位置在寫者之後的讀者本 tick 就讀到，之前的下 tick 讀到；緩衝保留到下一 tick 末；被 run 條件跳過的讀者錯過那個 tick 的事件（寫明）。

## 6. 欄的儲存與 `entity_store` 的關係（通用規則；`v8-arch-a6-revision`）

- **`engine/entity_store` 不持有、不讀寫 change stamp。** store 的 Design Invariant 6（狀態權威、沒有 change tick）對 store 本身繼續成立；stamp 是 World 的東西，它的儲存與讀寫只在 `engine/kernel`。
- World 的一個欄有三樣東西：鍵（`keys`）、值、每格的 `added`／`changed` stamp。規格 A.6 留了兩種佈局，哪一種由 `v8-arch-kernel-world-exec` 的事前宣告定，兩種都守上一條：
  1. **分開存（SoA）**：鍵、值、stamp 各一個陣列，World 用 store 的「回傳位置」查找版本同步搬動平行陣列。stamp 陣列是 kernel 的成員。
  2. **格子包法**：元素型別是 kernel 定義的格子（鍵、兩個 stamp、值），直接重用 store 的排序容器。stamp 在元素裡，但元素型別與對 stamp 的每一次讀寫都在 kernel；store 只把它當不透明的元素搬動。
- 判斷一個改動有沒有違反本節：`engine/entity_store` 的原始碼裡出現 stamp 的欄位名、或依 stamp 做任何判斷，就是違反。守衛：審查加文字掃描（world-exec 新增）；spec J-30。
- 本文件不列任何產品的欄位名。產品欄位怎麼分、哪些進 digest，寫在該產品自己的目標文件。

## 7. 其餘（照規格 §A，待實作時搬進本文件）

資料模型（欄的 SoA 佈局、ColumnInfo 的 zone／writePolicy／role／rollback）、A.2 的操作與允許的狀態、A.4 參考失效規則、A.5 不變量 W-1～W-11、A.6 的 C++ 實現手段與存取驗證兩層，見 spec §A。這些在規格核准前仍是草案。

## Design Invariants

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。本節的變更是 Architecture Delta（AGENTS §3），要使用者同意。

條號不變。標「受 kernel DI-KS n 約束」的條，全文只在 kernel 頁的那一條；其餘是本部件特有的條文。

1. **鍵的疊代順序只來自數值**——受 kernel [DI-KS 3](kernel.zh-Hant.md#di-ks-3) 約束：**全文與守衛見該條，改動前必讀**。
2. **World 不配發、不重用鍵；世代切換＝重建 World**——受 kernel [DI-KS 4](kernel.zh-Hant.md#di-ks-4)、[DI-KS 14](kernel.zh-Hant.md#di-ks-14) 約束：**全文與守衛見該條，改動前必讀**。
3. **stamp 是依 Plan 順序推進的全域計數、單調、不進 (c)**：跳過的 System 也消耗；同一 World 的執行歷史相同時序列相同；Changed／Added 的「上次執行」以掛點為單位；不回滾；(c) 區禁變更過濾。守衛：spec J-30、J-38、J-52。
4. **寫入政策預設獨占，獨占是啟動檢查**——受 kernel [DI-KS 6](kernel.zh-Hant.md#di-ks-6) 約束：**全文與守衛見該條，改動前必讀**。
5. **結構變更只在同步點、順序寫死、Despawn 不是寫者**——受 kernel [DI-KS 9](kernel.zh-Hant.md#di-ks-9) 約束：**全文與守衛見該條，改動前必讀**。
6. **事件屬於一個 clock domain，只在它的 clock schedule 內讀寫**——受 kernel [DI-KS 10](kernel.zh-Hant.md#di-ks-10)、[DI-KS 14](kernel.zh-Hant.md#di-ks-14) 約束：**全文與守衛見該條，改動前必讀**。

### Replacement Criteria（候選）

- LIM-4：進 digest 的欄不得 Shared；為繞過獨占檢查改 Shared 兩次以上 → 重新劃分元件（詳細在 `limits.zh-Hant.md`）。
- 某個產品需要 World 解讀 generation（例如由 World 拒絕 stale 鍵），或需要第二個鍵空間的對照表 → 鍵的形狀要重新設計，不在 World 上加分支。
- 同一 tick 內需要「本 tick 之後的變更」以外的變更過濾語義且無法以 `tickStamps` 區間查詢表達 → stamp 模型重審。

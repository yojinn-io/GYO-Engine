# docs/architecture/target：目標架構的文件

資料夾與規則由圖書管理員定（2026-11-11），依使用者的要求「新版架構文件與舊版分開、至少分資料夾、不混放」。

## 分界

- `docs/architecture/*.md`（頂層檔案）＝**現況**：已實作、正在跑的架構與它們的 Design Invariants。
- `docs/architecture/target/`＝**目標架構**：把 Bevy 的抽象能力（World／System Contract／Scheduler／Executor）帶入 GYO 之後各部件的文件、Design Invariants 與 Replacement Criteria、使用者已定的設計決定。
- `docs/architecture/plans/`＝**過程紀錄**：計畫、HANDOFF、dev_log 的索引、spike 報告、規格草案。

放在 `docs/architecture/` 之下而不是同層另一個資料夾，是因為 AGENTS.md §3 規定 Engine 的 Design Invariants 放在 `docs/architecture/` 配下；目標架構的 DI 也要落在 agent「改 Component 前先讀該 owner 的 DI」會看到的範圍內。

## 命名

- 平鋪，不分子資料夾：檔名＝搬回後在 `docs/architecture/` 的檔名。例：`target/world.zh-Hant.md` → `docs/architecture/world.zh-Hant.md`。工作線（kernel）不是文件的最終位置，所以不出現在路徑裡；搬回只是 `git mv`。
- 搬回時若取代現況的同名文件，取代後刪除舊檔（歷史留在 git），不並存。

## 狀態標記

標題下第一行，固定英文、只有兩種（和 `Design Invariants` 見出し同理，方便 grep、不因翻譯走樣）：

```text
> Status: target, not implemented
> Status: implemented in <batch-id>, pending move to docs/architecture/
```

搬回 `docs/architecture/` 時刪掉這一行。

## 目標 DI 與現況 DI 衝突時（架構師決定，2026-11-11）

- **未遷移的程式照現況 DI**（`docs/architecture/*.md`）；**遷移批次照目標 DI**（`target/*.md`）。這是 AGENTS §3「改 Component 前先讀該 owner 的 Design Invariants」在兩套文件並存期間的解釋：一個 Component 在任一時刻只受一套 DI 約束，由它是否已遷移決定。
- 遷移批次的事前宣告要寫明它遷移哪些 Component、從此它們受哪份 target 文件約束；遷移完成後文件搬回，衝突自然消失。
- 目標 DI 本身的變更仍是 Architecture Delta（§3），要使用者同意。

## 來源與引用

- 概念基準：`docs/architecture/plans/kernel/mechanism.zh-Hant.md`；規格草案：`docs/architecture/plans/kernel/spec.zh-Hant.md`。規格是草案，決定以使用者在 K.3 的裁決為準；裁決後寫進對應的部件文件。
- checkup：圖書管理員在 `docs/checkup/documentation/README.md` 加項目核對 Status 行、implemented 久留未搬、同名並存、現況檔混入目標內容。

## 目錄

- [limits.zh-Hant.md](limits.zh-Hant.md)：**限制與修改條件**（使用者 2026-11-11 要求的集中頁；LIM-1～LIM-4）。位置待圖書管理員確認。
- [runtime-composition.zh-Hant.md](runtime-composition.zh-Hant.md)：Runtime 的組合（Client 的 Frame 與 Render World 分開、Extract 唯讀無反饋、一 World 一執行緒）。
- [scheduler.zh-Hant.md](scheduler.zh-Hant.md)：Scheduler（衝突邊與順序邊、歧義處置、(b) 讀 (c) 的顯式順序）。
- [world.zh-Hant.md](world.zh-Hant.md)：World（鍵格式與配發策略、change stamp、寫入政策預設獨占與資料路徑、結構變更的套用順序與 Despawn；其餘待規格核准）。

規則（使用者 2026-11-11）：spec K.3 的**每一項**決定都要寫進對應的部件文件（Design Invariants 或資料路徑），不只改 spec 的 K.3 與對應節。

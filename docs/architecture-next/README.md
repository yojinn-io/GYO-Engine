# architecture-next：目標架構的文件（尚未實作）

**狀態：資料夾名稱暫定，最終名稱與結構由圖書管理員定（架構師 2026-11-11 轉交）。** 使用者 2026-11-11 的規則（側邊會話轉述）：新版架構文件與舊版分開、至少分資料夾、不混放。

## 這個資料夾放什麼

- 把 Bevy 的抽象能力（World／System Contract／Scheduler／Executor）帶入 GYO 的**目標架構**：各部件的文件、它們的 Design Invariants 與 Replacement Criteria、使用者已定的設計決定。
- 每份文件開頭標明狀態：「**目標架構，尚未實作**」或「**已實作於 <批次代號>**」。
- 不放：計畫、HANDOFF、dev_log、spike 報告（留在 `docs/architecture/plans/kernel/`）；現況架構（留在 `docs/architecture/`）。

## 與 `docs/architecture/` 的關係

- `docs/architecture/` 描述**現況**（已實作、正在跑的架構）；本資料夾描述**目標**。兩者不並存同一部件的兩個版本於同一資料夾。
- 一個部件遷移完成後：它的文件搬回 `docs/architecture/`（或取代同名文件），舊文件刪除或標廢止；本資料夾不留副本。
- 本資料夾的文件引用規格（`docs/architecture/plans/kernel/spec.zh-Hant.md`）與概念基準（`mechanism.zh-Hant.md`）；規格是草案，決定以使用者在 K.3 的裁決為準，裁決後寫進對應的部件文件。

## 目錄

- [kernel/world.zh-Hant.md](kernel/world.zh-Hant.md)：World（鍵格式與配發策略、疊代順序、change stamp、參考失效、debug 核對）。目標架構，尚未實作。

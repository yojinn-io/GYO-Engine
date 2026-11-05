# 實體容器（entity store）：Engine 共通模組的分批計畫

更新：2026-11-05。Owner：Engine（架構）。任務卡 `v8-arch-entity-store`，由架構師會話（例行架構掃描）於 2026-11-05 開出。
**狀態：batch 2 `v8-arch-entity-store-delta` 完成，等使用者決定 [delta §7](delta.zh-Hant.md) 的 10 項；batch 3 未開始。** 每批結束時停下，取得使用者核准後才進下一批。

## 這是什麼

把「實體的容器」（entity store）放進 Engine，成為共通模組。
架構師開卡時的暫定立場：容器＝實體 id（index＋generation）、SoA 的元件欄、以 EntityId 升冪的決定性迭代、延後到同步點才套用的結構變更（Commands）、change tick；不放進 Engine 的是通用執行期（system、scheduler、world、resources、events、app、plugin）；id 分段的政策與實體種類的意義留在產品。
batch 1 的 [survey](survey.zh-Hant.md) 檢驗後，batch 2 的 [Delta](delta.zh-Hant.md) 建議的「容器版」只剩四件：外部給的 `EntityId` 鍵（不帶 generation）、每元件一欄依 id 升冪、依 id 的查找／插入／保序刪除、生成與刪除的待辦在呼叫端指定的同步點依 id 升冪套用；generation 與 change tick 都不收（條件列為 Replacement Criteria）。採哪一版由使用者在核准 Design Invariants 時決定。

## 來源（決定的經過）

- 2026-11-01：使用者決定「ECS 只採資料佈局、不採通用執行期、留在產品 `object_fps_pvp`」。PR #140 放入 pvp 的實體模型：`apps/object_fps_pvp/include/RetroFPS/Pvp/EntityColumns.hpp`、`EntityId.hpp`，契約 [entity-model.zh-Hant.md](../../../object_fps_pvp/entity-model.zh-Hant.md)（Design Invariants E-1～E-12、Replacement Criteria RC-1～RC-5）。
- 2026-11-05：使用者（經圖書管理員會話）決定「ECS 放進 Engine 的共通模組（案 (b)）」，並核准架構師的進行方式：先做 Bevy／EnTT 的參照調查表，作為 Design Invariants 的根據。
- 既有約束：AGENTS.md §1（不從單一 use case 做通用抽象）、§2（觀測到的壓力）、§3（Architecture Delta 的 7 項；Design Invariants 與 Replacement Criteria 要在堆功能前寫）、§6（Engine 不知道產品）。`docs/architecture.md` 第 129 行「No umbrella framework, scene hierarchy, ECS, plugin ABI or service locator…」在 Delta 的 PR 內由圖書管理員會話把 ECS 拿掉並加連結；在那之前不改。

## 批次

代號＝函數名，說明＝註釋。模型／檔位依 `agents/navigator.md` 與 `agents/effort-gearbox.md`；本任務屬上段（Architecture 判斷、資料形式），不用小模型。

| 代號 | 一句話 | 模型／檔位 | 產出 | 狀態 |
|---|---|---|---|---|
| `v8-arch-entity-store-survey` | 讀 Bevy ECS 與 EnTT 的實際程式碼，逐機制判定「採／改裝後採／不採」，作為 Design Invariants 的根據 | 實作者 Opus 5.5／high；敵對檢查者 Fable 5.1／xhigh（與實作者不同模型，避免盲點重疊） | [survey.zh-Hant.md](survey.zh-Hant.md)、本 README、[HANDOFF](HANDOFF.md)、dev_log | 完成，已核准 |
| `v8-arch-entity-store-delta` | 寫 Architecture Delta 與 Engine 契約文件 `docs/architecture/entity-store.md`（英文）：Design Invariants、Replacement Criteria、AGENTS §3 的 7 項、pvp 的 DI 哪些搬進 Engine、哪些留在產品；不寫程式 | 實作者 Opus 5.5／high；審查者 Opus 5.5／high | [entity-store.md](../../entity-store.md)（英文契約）、[delta.zh-Hant.md](delta.zh-Hant.md) | 完成，等決定 |
| `v8-arch-entity-store-impl` | 照核准的契約實作 Engine 模組（暫提 `engine/entity/`，batch 2 定案），pvp 作為第一個消費端在同一條 PR 線換上；守衛、突變、digest 前後相同為驗收 | 實作者 Opus 5.5／high（id 配發與 Commands 套用順序局部 xhigh）；審查者 Opus 5.5；檢證者 Fable 5.1（啟動前問使用者） | 程式、守衛、dev_log | 待 batch 2 |

## 不做的事

- 不碰 framework（ECS 是 Engine 的模組）。
- 不碰 Gateway（`services/gyo_gateway`）與 pv10 的 wire；需要時停下回報。
- 不把架構師的暫定表當結論；由 survey 與 DI 核准決定。

## 成果物與證據

- 文件：本資料夾（README、HANDOFF、survey）；dev_log 在 `docs/dev_logs/`（zh-Hant）；Engine 契約文件在 `docs/architecture/entity-store.md`（batch 2，英文）。
- 證據不進 repo：主 checkout 的 `build/evidence/entity-store/`（git 管理外；不用家目錄）。
- PR 一條線：batch 1～2 只有文件，batch 3 有程式。PR 與 commit 用日語。

## dev_log 一覽

- [2026-11-05 v8-arch-entity-store-survey](../../../dev_logs/2026_11_05_arch_v8_entity_store_survey.zh-Hant.md)：Bevy ECS 與 EnTT 的逐機制參照調查；敵對檢查 must 2／should 7／note 8 全部處置。
- [2026-11-05 v8-arch-entity-store-delta](../../../dev_logs/2026_11_05_arch_v8_entity_store_delta.zh-Hant.md)：Engine 契約 `entity-store.md`（DI 10、RC 15）與 Architecture Delta；審查 must 1／should 12／note 11 全部處置。

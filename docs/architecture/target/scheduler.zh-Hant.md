# Scheduler

> Status: target, not implemented

更新：2026-11-13（`v8-arch-kernel-registry-schedule`：§3 API 名稱）。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §D（Scheduler）、§G.1（區規則）。
本文件只寫**使用者已裁決**的部分；規格中仍標「待使用者」的條目不在這裡。

## 1. 衝突邊與順序邊分開；歧義的處置（mechanism §3.2 審查修正 1、2；主張 A）

- 衝突邊只說「不能同時」（同 id 且至少一方寫），順序邊說「先後」（before／after、stage 界線、同步點）。Scheduler 不假設業務順序。
- 有衝突、無順序路徑＝歧義：任一方 zone=C → **啟動錯誤 D-E8**，要人寫 before／after；兩方都 zone=B → 依登記序平手，以診斷等級列出。Kernel 不提供歧義豁免。
- 資料衝突本身不是錯誤：有順序路徑的衝突對序列化執行，先後由順序邊決定。

## 2. 讀 (c) 資料的 (b) System 必須與 (c) 寫者有顯式順序（使用者 2026-11-11 決定，spec K.3 第 5 項）

- 區規則下 (c) 與 (b) 的 System 互相衝突只可能是「(b) 讀 (c) 資料」（呈現讀模擬）。這種衝突照第 1 節處理：任一方是 C 就是錯誤，所以呈現 System 必須與 (c) 寫者有顯式順序，或位於不同 stage。
- **配套慣例是限制**：呈現放在模擬之後的 stage，一條 stage 界線就給了全部順序，不必逐個寫 before／after。成本落在呈現 System 的數量（每個讀 (c) 的呈現 System 都要落在正確的 stage）；產品線在遷移時確認。修改條件與解除方式見 `limits.zh-Hant.md`。

## 3. API 名稱（`v8-arch-kernel-registry-schedule` 定；只對應名字與條文，不改上面的規則）

程式在 `engine/kernel/include/engine/kernel/`（`Registry.hpp`、`Plan.hpp`、`Declarations.hpp`）。

| 名字 | 是什麼 | 對應條文 |
|---|---|---|
| `ScheduleOptions` | Host 給的 ScheduleDef：`domain`、`startup`、`clock`、`extract`、`rollbackEnabled`、`stages`、`ingressStage`、`syncPoints`、`eventTrim`、`executor`、`parallelGuardTest`、`runner` | spec D.1 |
| `Registrar::Schedule`、`Registrar::InsertStage` | 登記 schedule；在既有 stage 前後插入 stage | spec C.2 第 2 點 |
| `Mount` | 一個掛點：`schedule`、`stage`、`before`、`after`、`sets`、`conditions`、`notInRollback` | spec B.1 mounts；D-E19 |
| `Registrar::Set`（`SetOptions`） | 具名的 System 群組：before／after 與 run 條件一次給全部成員 | spec C.1 sets、E.4 |
| `Plan`、`Step`、`StepKind` | 每個 schedule 一張；步驟是 System、同步點（stage 末以 stage 名、具名同步點以其名）、事件修剪 | spec D.1、D-1、D-6 |
| `Plan::declaredEdges`（`DeclaredEdge`）、`Plan::NonDataflowEdges` | before／after 邊（set 展開後的 System 對）與是否為資料流邊 | 遷移計畫 §3.5、G1 謂詞 (ii)、RC 3 |
| `FrozenRegistry::TieBreaks`（`TieBreak`） | (b) 依 registrationSeq 加的順序，以診斷等級列出 | spec D.2 第 10 步、D-4 |
| `Fingerprint`（`version`、`sha256`、`Text()`＝`"<版本>:<64 位十六進位>"`） | Plan 指紋、Registry 指紋、C 指紋；版本進雜湊的位元組也進文字 | spec C.2 第 5 點、H-4、J-18、J-20 |
| `FrozenRegistry::ExportPlans` | Plan 的文字匯出（`gyo.kernel.plans 1`）：步驟、指紋、每條 before／after 邊、`non_dataflow_edges n/m p%`、平手 | 遷移計畫 §3.5 |
| `ExecutorKind::Serial`／`Parallel` | v1 只有 Serial；任何 Parallel＝D-E17 | LIM-2、spec X-2 |

Plan 的平手鍵寫成程式的規則（Kahn 演算法）：可選節點中取 registrationSeq 最小的 System；沒有可選的 System 時才取同步點或事件修剪（同步點因此盡量晚）。(b) 歧義對依 (seqA, seqB) 的順序處理，每加一條邊就更新可達性，結果固定且無環。

## Design Invariants

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。本節的變更是 Architecture Delta（AGENTS §3），要使用者同意。

條號不變。標「受 kernel DI-KS n 約束」的條，全文只在 kernel 頁的那一條；其餘是本部件特有的條文。

1. **衝突與順序分開存**——受 kernel [DI-KS 5](kernel.zh-Hant.md#di-ks-5) 約束：**全文與守衛見該條，改動前必讀**。
2. **(c) 的歧義是啟動錯誤，(b) 依登記序平手，無豁免**——受 kernel [DI-KS 5](kernel.zh-Hant.md#di-ks-5) 約束：**全文與守衛見該條，改動前必讀**。
3. **Plan 唯一且建一次**——受 kernel [DI-KS 3](kernel.zh-Hant.md#di-ks-3) 約束：**全文與守衛見該條，改動前必讀**。

### Replacement Criteria

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。

- LIM-2：v1 只有 Serial，任何 Parallel 宣告是啟動錯誤；解除條件＝量到 tick 超預算且有逐位元等價的守衛（spec J-27）（詳細在 `limits.zh-Hant.md`）。
- LIM-1：(b) 讀 (c) 要顯式順序；為消除 D-E8 加的順序宣告一半以上與資料流無關 → 區劃錯了，重審區劃（詳細在 `limits.zh-Hant.md`）。
- **第二種排序機制**——受 kernel [RC 4](kernel.zh-Hant.md#rc-4) 約束，改動前必讀該條。

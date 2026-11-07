# v8-product-pm-detect-diag 事前宣告

更新：2026-11-07。Owner：`object_fps_pvp`。來源：[movement-fix](movement-fix.md) 的 pm-detect 拆成兩半（使用者 2026-11-07，架構師轉達）：「診斷」在 11/9 場次之前做，「量、判定、修」在 lag-l2 之後。
**狀態：使用者 2026-11-07 照推薦核准**（`-diag-scope` (a) 每一步都記、`-radius` (a) 維持 0.25）。判定與閾值在看到任何結果之前寫下，核准後不放寬（AGENTS §14.1）。

## 0. 為什麼（Feature → Contract → Architecture）

- **Feature**：biome 上的移動缺陷（卡在空中、空氣牆）要能在 11/9 場次 2～3 人的真實遊玩裡留下紀錄，之後 pm-detect 用它重現、固定成失敗的測試（§14.3）。
- **Contract**：診斷是「可開關、只記錄」的檔案；不進 digest、不上 wire、不改任何移動的結果。
- **Architecture**：沿用既有的 movement trace（`MovementTrace.hpp`、`MovementTraceWriter.hpp`：產品自己的可選診斷、執行時以旗標開啟），不新增子系統。

## 1. 範圍

- **記錄的值**（每一步移動一行）：時刻、玩家、生命代、epoch、指令序號、authority tick（Match）、移動前後的位置、desiredY、解出的 y、最深的嵌入（法線、深度）、最後一次掃描接觸（法線、fraction）、supported、grounded、verticalVelocity、crouched。
- **取值的方式**：`MoveCharacterBody` 與 `StepMovement` 加一個可選的輸出參數（預設 `nullptr`），只在給了時寫入。計算的路徑與結果不變；沒開診斷時多一個指標判斷。
- **輸出**：新的旗標 `--movement-diag <path>`（Match 與 Client），寫一個獨立的 JSON Lines 檔（`schema: "gyo.pvp.movement_diag"`, `version: 1`）。不放進現有的 `--movement-trace`：現有的分析器（lan15、ingress、shot 的離線對照）讀那個檔，新的 kind 會讓它們失敗。
- **寫入的執行緒**：行的格式化與寫檔不在 Match 的 authority 鎖裡做（現有 trace 是在鎖裡格式化）。用上限固定的佇列交給寫檔執行緒，滿了就丟棄並計數，結束時寫一行 `diag_end`（`records`、`dropped`）。
- **Client 記什麼**：只記第一次預測（`Advance` 走到的那一步），不記 reconcile 時的重播（重播的同一步會重複很多次）。
- **場次 kit**：`mac.sh match` 與 `windows.ps1` 加 `--movement-diag`，log 一起收進 zip；`lag_l2_summary.py` 不讀它。

## 2. 待決（使用者）

| 代號 | 問題 | (a) | (b) | 推薦 |
|---|---|---|---|---|
| `-diag-scope` | 記多少 | 每一步都記（Match 3 人約 180 行／秒，約 130 MB／小時；Client 每台約 60 行／秒） | 只在有接觸或沒有 grounded 時記 | (a)：卡住之前的那幾步也要留下，(b) 會漏掉「正常走到卡住」的過程；量照估算可接受 |
| `-radius` | biome 的 `radius` 在場次用 0.3 嗎 | 不用，維持 0.25 | 用 0.3（純資料） | (a)：診斷的目的就是記下現在的缺陷；0.3 會讓「一格縫＝2r」的退化點消失，場次就記不到它，lag-l2／K8 也換了一個變因 |

## 3. 不做

- 不改任何移動行為、容差、地板、半徑（pm-vertical 的範圍）。
- 不做重現與失敗的測試（pm-detect，lag-l2 之後）。

## 4. 判定（先宣告）

| 代號 | 判定 | 通過條件 |
|---|---|---|
| D1 行為不變（既有） | PvP 既有測試（含 AuthorityDigest 的 golden、`WallGridEquivalenceTests` 的 B2）不改判定 | 全部通過 |
| D2 行為不變（新） | 隨機交叉：同樣的 arena 與指令序列，開診斷與不開診斷的 `StepMovement` 結果逐位元比較（200 組 × 200 步，含 biome） | 全部 bit 一致；比較數＝產生數 |
| D3 記錄完整 | 本機（Intel Mac）：biome 的 Match＋2 個 Client 跑 2 分鐘以上（我在本機自動送指令；不需要你操作） | Match 的診斷行數＋`dropped`＝Match 解出的步數；每台 Client 的行數＋`dropped`＝`Advance` 的步數；`dropped`＝0；每行都能被 JSON 解析且欄位齊全 |
| D4 成本 | B4 的量法（4 人 × 20,000 tick），診斷寫到檔案 | Match 一 tick（4 步＋4 發）的 p99 增加 ≤ 0.1 ms；authority 鎖內的時間 p99 增加 ≤ 0.02 ms |
| D5 突變 | 新突變（診斷改到結果、漏記一步、在鎖裡寫檔、Client 記到重播、`dropped` 不計） | 全部 killed |
| D6 L1 | 三平台 | 通過 |

- D4 的量法（2026-11-07 補，跑之前寫下）：`MovementDiagTests.cpp` 的 D4（CI 跳過，`--no-skip` 執行），biome、Intel Mac、RelWithDebInfo。(i)「一 tick」：B4 的迴圈（4 人 × 20,000 tick，每人一步＋一發），一次不開診斷、一次開診斷並寫到檔案（每步建事件並 `RecordMovementDiag`），比 p99 的差。(ii)「鎖內」：`PvpMatch::Tick`（正式執行時在 authority 鎖內呼叫）4 人各送一個指令、20,000 tick，計時 `Tick` 本身，開與不開比 p99 的差。兩次的順序固定為先關後開，各跑一次。
- D3 的「本機自動送指令」：用現有的 network probe 或測試裡的 IPC driver，不靠 GUI；具體做法在實作時選，寫進 dev_log，不改判定。
- 件數的突き合わせ照 AGENTS §14：D2、D3 都先對件數，不一致就不判定。

## 5. 模型與檔位

- 上段（動到 Match 判定用的共用函式簽名，必須證明行為不變）。
- 實作者：主會話 Opus 5.5／high。審查者：Opus 5.5／high（general-purpose 子 agent，唯讀）。
- 估時：4～6 h。超過 9 h 停下回報。

## 6. 場次（lag-l2 宣告修訂 9，另寫）

- 程式改為本批合併後的 commit；場次時 Match 與每台 Client 都開 `--movement-diag`。
- 退路：本批在 11/8 晚上前沒合併，就用 `8b8eaa19` 的 biome kit（沒有診斷）；再不行用 `54948874` 的 pvp_corners kit。
- 限界：Client 的診斷寫檔在 Client 的程序裡，會稍微增加 Client 的負擔；D4 只量 Match。lag-l2 的 view_lag 來自 Client 的畫面時刻，這個影響沒有量。

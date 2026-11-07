# PvP v8：v8-product-pm-detect-diag（移動診斷，只記錄）

日期：2026-11-07。分支 `claude/pvp-v8-lag-l2-biome-kit`。主會話 Opus 5.5／high；審查者 Opus 5.5／high（general-purpose 子 agent，唯讀）。估 4～6 h。

**狀態：實作與 D1～D5 完成；審查進行中，D6（L1）未跑。** [事前宣告](../object_fps_pvp/plans/v8/v8-product-pm-detect-diag-declaration.md)使用者 2026-11-07 照推薦核准（`-diag-scope` (a) 每步都記、`-radius` (a) 維持 0.25）。

## 為什麼

pm-detect 拆成兩半（使用者 2026-11-07，架構師轉達）：診斷在 11/9 場次之前，讓 2～3 人的真實遊玩留下卡在空中、空氣牆的紀錄；量、判定、修在 lag-l2 之後（[movement-fix](../object_fps_pvp/plans/v8/movement-fix.md)）。

## 做了什麼

- `MoveCharacterBody`、`StepMovement` 加可選的輸出參數（`CharacterMoveDiagnostics`、`MovementStepDiagnostics`，預設 `nullptr`）：移動前後的位置、desiredY、開始時與落地判定的 supported／grounded、第一輪最深的嵌入（法線、深度）與推出次數、最後一次掃描接觸（法線、fraction）與次數。
- `MovementDiag.hpp`：JSON Lines（`gyo.pvp.movement_diag` version 1），一步一行，結尾 `diag_end`（`records`、`dropped`）。生產者只把事件複製進上限 65,536 的佇列；格式化在自己的執行緒，寫檔交給 Engine 的 `TraceSink`。
- Match：`PvpMatch` 的每一個解出的步。Client：只記第一次預測（seed lead 與 `Advance`），不記 reconcile 的重播。
- 旗標 `--movement-diag <path>`（Match、Client、quad probe），已有同名檔時照 D191 取下一個號碼。
- 場次 kit：`mac.sh` 的 `MOVEMENT_DIAG=1`、`windows.ps1` 的 `-MovementDiag`。

## 判定

| 判定 | 結果 |
|---|---|
| D1 行為不變（既有） | 通過：`cpu\|shader` 138／138（含 AuthorityDigest 的 golden、pa-broadphase 的 B2） |
| D2 行為不變（新） | 通過：40,000 步（200 組 × 200 步，每 10 組一組 biome），開與不開逐位元相同（PlayerState 全欄位），不一致 0；有接觸的步 4,277、離地的步 24,616 |
| D3 記錄完整 | 通過：biome 的真 Match＋Gateway＋quad probe（3 個 bot，150 s）：Match 27,864 行＝resolved 27,864，Client 27,876 行＝generated 27,876，`dropped` 都是 0，每行可解析、欄位齊全 |
| D4 成本 | 試行 1 **不成立**；修正後試行 2 通過（見下） |
| D5 突變 | 通過：9／9 killed（新 5＋改動範圍 4） |
| D6 L1 | 未跑 |

```text
件數的突き合わせ：一致（D2 產生 40,000＝比較 40,000；D3 Match 27,864＋0＝27,864、Client 27,876＋0＝27,876；D4 兩次各 160,000＝records＋dropped）
```

### D4

| 試行 | (i) 一 tick 的 p99 增加（≤ 0.1 ms） | (ii) `PvpMatch::Tick` 的 p99 增加（≤ 0.02 ms） |
|---|---|---|
| 1（15:32，負載 2.5） | 0.0017 ms 通過 | **0.0205 ms 不成立**（關 0.0295、開 0.0500） |
| 2（15:56，負載 3.0，修正後） | 0.0112 ms 通過 | −0.070 ms 通過（關 0.122、開 0.052） |

- 試行 1 依 §14.1 停下回報。使用者 2026-11-07 選 (a)：修實作，用同一判定重跑。
- 修正：生產者讀一個 atomic 指標（不再每步加鎖、複製 `shared_ptr`）；只有發現佇列是空的那一筆才喚醒寫入執行緒。
- **試行 2 的通過不是強的證據**：(ii) 的「不開」p99 從 0.030 跳到 0.122 ms，量測的波動（約 ±0.09 ms）大過閾值 0.02 ms。這個量法跑得比 60 Hz 快很多（2 萬 tick 連續跑），寫入執行緒一直在搶 CPU，佇列也會滿（試行 1 丟 434、試行 2 丟 10,686）；60 Hz 時的實際損失看 D3（0）。
- D4 測試裡原本多寫了「`dropped`＝0」的檢查，它不是宣告的 D4 條件，又因上面的原因必然失敗，改成只印出（理由寫在 commit）。

## 經過

- D3 的「本機自動送指令」用 quad probe（無 GUI 的 bot，Client 的程式碼在同一程序）。為此給它加了 `--movement-diag`。真的 Client 程序（GUI）沒有在這批跑；Client 端的記法另由單元測試「first predictions, not replays」驗（122 行＝generated 122，重播約 180 步不記）。
- 宣告列的突變「在鎖裡寫檔」沒有加：功能測試分不出來，只有 D4（計時，CI 不跑）看得到。

## 證據

主 checkout 的 `build/evidence/v8-pm-detect-diag/`（Intel Mac）：`d4.txt`（試行 1）、`d4_trial2.txt`、`d3-*/`（Match、Gateway、probe 的 log、trace、diag 與 `d3_check.txt`）、`dryrun-mac/`（`MOVEMENT_DIAG=1` 的 `mac.sh`）、`mutations.txt`／`.json`。

## 範圍的限界

- D3 的 bot 沒有碰牆也沒有跳（接觸 0、離地 0）：真實程序裡的接觸值沒有被走到，內容由 D2 驗。
- D4 的量測波動大過 (ii) 的閾值，見上。
- Client 寫診斷會增加 Client 的負擔，lag-l2 的 view_lag 受到的影響沒有量（宣告 §6）。
- `windows.ps1` 的 `-MovementDiag` 沒有在 Windows 上跑過（這台 Mac 沒有 PowerShell）。

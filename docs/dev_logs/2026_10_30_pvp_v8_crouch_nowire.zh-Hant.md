# PvP v8：v8-product-crouch 不碰 wire 的部分

日期：2026-10-30。分支 `claude/pvp-v8-pv8-line`。檔位 high，引擎 Opus 5.5。

**狀態：** 不碰 wire 的部分完成（commit 1 `fd950ea3`、commit 2 `347fa9ba`）。wire 的接入（proto、版本值 8、Ready／Welcome 的參數、移動規則資料檔、按鍵）與遠端蹲姿動畫還沒做。

相關文件：[事前宣告](../object_fps_pvp/plans/v8/v8-product-crouch-declaration.md)（`v8-product-crouch-declaration-approve`）、[protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md) §4、§5、[v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md)。

## 做了什麼

- `MovementCommand` 加 `crouchHeld`、`sprintHeld`；`PlayerState` 加 `crouched`、`sprinting`（都在 struct 最後）。
- `StanceRules`（蹲高 1.2、眼高 1.0、蹲速 0.5、衝刺 1.0）與 `ValidStanceRules`、`StanceBodyHeight`、`StanceEyeHeight`。不放進 `Arena`：`ArenaContentDigest` 用 structured binding 綁住 `Arena` 的全部成員（`Arena.cpp:100-104`）。`StepMovement` 與 `QueryShot` 多一個帶 `StanceRules` 的版本，舊簽名保留並用預設值，既有的測試呼叫不必改。
- `StepMovement` 的姿態：alive 時照 `crouchHeld` 切換（地面與空中都可以）；地面上按跳時目標是站姿；蹲變站在目前位置用 `CanPlaceCharacterBody` 檢查；蹲姿乘蹲速；支撐、掃掠用蹲姿高度；站不起來就不跳；死亡時姿態不變。站立分支直接用 `arena.bodyHeight`、`arena.movementSpeed`，浮點運算不變。
- Match：建構時驗證 `StanceRules`；重生時 `crouched`、`sprinting` 設為 false；Neutral 替補清掉 `sprintHeld`、保留 `crouchHeld`；射擊用 `stance_`。
- Client 預測：`SetStanceRules`；`SeedLead` 的領先命令繼承 `authority.crouched`；`Advance` 多一個 `crouchHeld`（預設 false，還沒接按鍵）；顯示用的掃掠依姿態取身高。
- 權威 digest v2：`crouched`、`sprinting` 只在 true 時符號化；新增 golden 情境 `move-crouch/contract`、`move-crouch-slab/slab`（合成的 1.5 m 高 slab 地圖）。
- 測試：`CrouchTests.cpp` 6 個、`CombatTests.cpp` 1 個（蹲著死亡後重生為站姿）。

## 檢驗與經過

- J1（事前宣告，看結果前核准）：base `de1c679b`、branch `fd950ea3`，同一台 Intel Mac，`compare_authority_trees.py` 各 1 次：scale 1、scale 3 都是「36 scenarios, 0 differing」。
- J2：commit 2 之後 `authority_digest` 通過，既有 golden 行不變，只加 2 行。第 1 次跑失敗：只重建了最佳化版，`-O0` 執行檔是舊的（環境）；全部重建後通過。
- J3：蹲下測試的突變 9 個，8 個 killed。`reset-substitution-stands` 存活：Match 在新 epoch 收到第 1 個命令前不推進該玩家（`PvpMatch.cpp:473-476`），先前 pv8-design 寫的「epoch 輪替後替補會把蹲著的玩家拉起來」不成立。該規則刪除，pv8-design、protocol-v8 §5、事前宣告已更正；測試改成守住 epoch 輪替時保留姿態。
- 偏差：事前宣告寫「先寫失敗的測試再實作」，實際先實作，Red 改用突變證明（使用者在核准宣告時接受）。
- J4：`test` preset 93 個測試，第 1 次 `object_fps_pvp.mutation_tool` 失敗：突變 `v71a1-rules-ignored` 的 find 對準舊的 `PvpMatch` 建構子。更新 find／replace（語意相同），單獨跑確認 killed；之後通過，其餘 92 個通過。
- D184 的相關突變（find 在改動檔案中的 54 個）：54／54 killed。

## 證據

- 兩樹比對的輸出在會話的工作檔（`j1-scale1`、`j1-scale3`），不放進 repo；結果行照抄在上面。
- 突變報告在會話的工作檔（`related-mutants-report.json`）。

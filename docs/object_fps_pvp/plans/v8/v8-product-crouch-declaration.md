# v8-product-crouch 的事前宣告（不碰 wire 的部分）

更新：2026-10-30。Owner：`object_fps_pvp`。**狀態：已核准（`v8-product-crouch-declaration-approve`，2026-10-30）。J1 通過：36 個情境 × scale 1、3，0 差分。** 範圍：`v8-product-crouch-nowire-first`（[HANDOFF](HANDOFF.md)）。契約條文：[protocol-v8](../../protocol-v8.zh-Hant.md) §4、§5（草稿，蹲下與衝刺的條文保留）。

## 範圍

做（只改 C++ 的領域層與測試，不改 proto、版本值、Gateway、IpcHost／ClientConnection 的轉換）：

- `MovementCommand` 加 `crouchHeld`、`sprintHeld`；`PlayerState` 加 `crouched`、`sprinting`（都加在 struct 最後）。
- 新的 `StanceRules`（蹲姿高度、蹲姿眼高、蹲速倍率、衝刺倍率）當參數傳給 `StepMovement`。不放進 `Arena`：`Arena` 是地圖內容，`ArenaContentDigest` 用 structured binding 綁住全部成員，加成員就是 wire 變更（`Arena.cpp:100-104`）。預設值先放產品常數（例如蹲高 1.2、眼高 1.0、蹲速 0.5、衝刺 1.0，開工時核對 arena 的 `bodyHeight`、`radius`）；Match 端的移動規則資料檔與 Ready／Welcome 的下發，在 wire 那一步做。
- `StepMovement` 的姿態規則（protocol-v8 §4）、依姿態取高度的單一函式、5 個膠囊呼叫點、蹲速。
- Match：重生重設、死亡凍結、缺命令的替補（Held、Neutral）。
- Client 預測：`SeedLead` 繼承權威姿態；`InterpolateLocalPresentation` 依姿態取高度。
- 呈現：本機相機的眼高依姿態；遠端蹲姿動畫（`Crouch_Idle_Loop`、`Crouch_Fwd_Loop`）的選擇邏輯，模型縮放照舊用站立高度。
- 權威 digest：`DigestVersion` 2（加 `crouched`、`sprinting`）與蹲下的新情境。

不做（wire 那一步）：proto 欄位、`pv8-rename`、版本值 8、Gateway 的逐欄複製、Ready／Welcome 的參數、移動規則資料檔、按鍵綁定。按鍵先不接：wire 接上之前 Match 收不到 `crouchHeld`，Client 預測蹲下而 Match 沒蹲，會一直修正。

## 受入判定（看結果之前宣告）

### J1：站立路徑的權威結果不變

- 做法：commit 分兩個。
  - commit 1：姿態邏輯（上面「做」的前五項），**不改 digest 的欄位與情境**。
  - commit 2：`DigestVersion` 2 與蹲下的新情境。
- 比對：同一台機器（本機 Intel Mac），base＝commit 1 之前的本分支 HEAD，branch＝commit 1；兩棵樹各建 `gyo_object_fps_pvp_authority_digest`，用 `tests/object_fps_pvp/compare_authority_trees.py` 在 scale 1 與 scale 3 各比一次。
- 判定：既有的全部情境（約 35 個）、兩個 scale，**差異 0 個**才通過。
- 試行與重試：1 次。digest 是決定性的，不重跑。有任何差異就停下回報（AGENTS §14 開頭），不修改後重比，等使用者決定。

### J2：既有的 golden 不變

- commit 2 之後，`authority_digest` 的 golden 檢查（既有行）全部通過；新的蹲下情境只加新行，舊行不改。

### J3：新行為的測試（先 Red 後 Green，AGENTS §14.3 的順序）

先寫成失敗的測試，再實作讓它通過；突變確認檢出力：

| 測試 | 現在的程式應該 | 突變（實作後改壞，測試必須失敗） |
|---|---|---|
| `SeedLead` 在蹲姿時的領先命令帶 `crouchHeld`＝true，權威端不會站起 | 失敗（寫死 `{seq,0,0,yaw,pitch}`） | SeedLead 不繼承姿態 |
| ~~epoch 重設後、第一個實際命令前，替補命令保持蹲姿~~（2026-10-30 刪除：前提不成立，Match 在新 epoch 的第 1 個命令前不推進該玩家；突變存活證實。測試改成守住 epoch 輪替時保留姿態） | — | — |
| Neutral 替補保留 `crouchHeld`、清掉 `sprintHeld` | 失敗 | Neutral 也清 `crouchHeld` |
| 頭上被擋時放開蹲鍵仍維持蹲姿 | 失敗 | 拿掉 `CanPlaceCharacterBody` |
| 蹲著按跳：站得起來就站起並跳；被擋就不跳 | 失敗 | 被擋時仍然跳 |
| 空中可以蹲；空中站起被擋時維持蹲姿 | 失敗 | 空中不解析姿態 |
| 死亡期間 `crouchHeld` 改變不影響姿態；重生後是站姿 | 失敗 | 重生不重設 |
| 蹲姿時移速乘 `crouchSpeedScale`；支撐、掃掠、眼高、受擊膠囊用蹲姿高度 | 失敗 | 站立分支也走參數化算式（J1 也會抓到） |
| 蹲下前後模型的縮放相同 | — | 縮放改用蹲姿高度 |

- 突變以 `tests/object_fps_pvp/mutations.json` 登記（只跑這批相關的突變，D184 的規則）；只在預期失敗時算 killed。

### J4：既有測試

- 本機 `test` preset 的全部 CTest 通過（含 `object_fps_pvp.cpu`、`authority_digest`）。四平台 L1 在 PR 時跑。

## 範圍的限界

- 本機只有 Intel Mac；J1 只在這台比。跨平台的保證靠 golden 子集（零角度情境）與 L1。
- 蹲下要到 wire 接上（新的 pv8 定稿之後）才能在真的連線中使用；這一步不做 L2／L3。
- 估時：19～25 h 中不碰 wire 的部分約 12～16 h。超過 1.5 倍（24 h）就停下回報。

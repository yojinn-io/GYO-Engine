# PvP v8：pv8-design（第 1 輪與第 2 輪）

日期：2026-10-29～30。分支 `claude/pvp-v8-pv8-line`。檔位 ultracode（兩輪），引擎：方案與審查 Opus 5.5，偵察與對抗式檢查 Sonnet 5.5。

**狀態：** 完成。第 1 輪的結果是 [protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md)（蹲下與衝刺，現為草稿）；第 2 輪的結論與分批在 [pv8-round2](../object_fps_pvp/plans/v8/pv8-round2.md)，6 項已決定（`pv8-round2-picks`）。下一批是 `pv8-contract-round2`（把第 2 輪寫進 protocol-v8）。

相關文件：[pv8-design](../object_fps_pvp/plans/v8/pv8-design.md)、[pv8 需求問卷](../object_fps_pvp/plans/v8/pv8-requirements.md)、[協議壓力調研](../object_fps_pvp/plans/v8/protocol-pressure-survey.md)、[多通道評估](../object_fps_pvp/plans/v8/multi-channel-evaluation.md)、[v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md)。決策代號見 HANDOFF；2026-10-29 的舊號 D243～D254 用 HANDOFF 的對照表讀。

## 經過

1. 第 1 輪（`pv8-design-ultracode`，run `wf_f6c34055-22e`）：蹲下、衝刺、ADS 的欄位。評分：最小加法 26、風險優先 24、功能優先 19。之後 ADS 取消（`v8-product-ads-cancel`），蹲跳規則與參數校準改了兩次（`pv8-design-air-calibration`、`v8-product-ads-cancel`、`v8-product-crouch-jump-blocked`），呈現用現有模型的蹲姿（`v8-product-crouch-pose`）。
2. 型別最大的實測（暫時的 Go 測試，量完刪除）：pv7 契約最大 1073 B、型別最大 1151 B；pv8 第 1 輪 1101／1175 B。用接近實際的值量：1 小時 406 B、24 小時 439 B。帳面的壓力來自 uint64 上限。
3. 協議壓力調研（背景子 agent）與多通道評估。使用者把 pv8 重開（`pv8-scope-reopen`），一起評估流水號、多通道、action 分離、撿物品、背包、Boss、技能、任務、房間規則；需求用典型值（`pv8-req-typical-values`）。
4. 第 2 輪（`pv8-design-round2-ultracode`，run `wf_0b9d1bf5-4aa`）：評分：風險與權威優先 27、擴充現有 23、通道分離 16。結論分三層：A 層（流水號縮窄、刪 `match_id`、預留號碼）隨 pv8-line 上 wire；B 層（可靠事件串流、對局狀態、動作種類）在 ⑤ 時升 pv9（`pv8-version-split` (a)）；C 層只留形狀。對抗式檢查 must 4 個，主會話把 u32 截斷降為 should（Client 本來就能送任何 u32 值），其餘 3 個列為 B 層上線的前置條件。

## 判讀上的更正

- 第 1 輪合成時，主會話補了一條「epoch 輪替後替補會把蹲著的玩家拉起來」的規則。實作 crouch 時突變存活，查明 Match 在新 epoch 的第 1 個命令前不推進該玩家，這條不成立，已刪除並更正文件（見 [crouch 的 dev_log](2026_10_30_pvp_v8_crouch_nowire.zh-Hant.md)）。

## 證據

- 兩輪 workflow 的原始輸出在會話的工作檔，不放進 repo（同 IP-3／IP-4 的做法）。
- 所有大小數字除了上面的實測以外，都是 varint 算術的推論；A 層要在 `pv8-seq-narrow` 用 `proto.Size` 實測。

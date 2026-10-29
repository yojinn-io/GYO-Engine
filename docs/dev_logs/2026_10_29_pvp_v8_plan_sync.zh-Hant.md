# PvP v8：v8-plan-sync（範圍、順序、分批、檔位與引擎）

日期：2026-10-29。分支 `claude/pvp-v8-plan-sync`（從 origin/master `62fd7ebf`）。檔位 medium，引擎 Opus 5.5。

**狀態：** 完成。只改文件，沒有程式變更。

相關文件：[v8 README](../object_fps_pvp/plans/v8/README.md)、[v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md)（D243～D246）。

## 做了什麼

- v8 README 改寫為分批計畫：五組（① 基礎功能、② 體驗優化、③ 手感時序、④ 工具、⑤ 流程項目）、各批的檔位與引擎、估時、PR 線、不在 v8 的項目。2026-10-22 版的候選與規劃輸入保留在 README 後半。
- 新建 v8 HANDOFF（記錄器），決策從 D243 起。
- 沒有另建 INVENTORY：候選的去處寫在 README。

## 經過

1. 第 1 版草案依 README 的候選與 v7.1 交過來的清單排，手感時序佔了大半；範圍的選項是「留在 pv7」或「走塔科夫」。
2. 使用者指出 v8 的核心是補完遊戲功能與工具（D243）。於是盤點 origin/master 的程式碼（本機 master 落後 161 個 commit，以 origin 為準）：
   - `apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp`：`MovementCommand` 只有 `moveForward`、`moveRight`、`yaw`、`pitch`、`jumpRequested`。蹲下與衝刺要改 wire。
   - `Combat.hpp`：`CombatRules` 只有一把武器的參數；`HitPart` 已有 `Body`／`Head`（pv7 的 hitbox 欄位，v7.1 D87、D89）。部位 hitbox 不必改 wire。
   - 產品與 protocol 沒有計分、回合、勝負；`PvpApplication.cpp` 的 Esc 呼叫 `connection.Leave()`；Engine 沒有 Audio 模組（只有 `engine/asset` 的 `SoundLoader`）。
   - 工具：`tools/ui_editor`、`tools/object_fps_preview`（`requires_apps` 是已停用的 `object_fps`）。
3. 使用者排順序（D244、D245），並核准 pv8 一次設計、隨蹲下合併（D246）。對局規則排在 ⑤，依 AGENTS §2 不進 pv8。
4. 依變速箱規則的「エンジン（モデル）」節，在每批的檔位旁加上引擎。

## 核對

- AU-1 的前提 IP-3：Engine 計畫 `input-and-present/HANDOFF.md` 開頭寫「IP-3 進行中」，但 v7.1 HANDOFF 記錄 IP-3 在 2026-10-22 完成、PR #85（IP-3／IP-4）在 2026-10-23 合併（`054b08f`，`git log --merges` 確認）。該 HANDOFF 開頭已過時，記在 v8 HANDOFF 的「發現」，沒有修改（不是本 owner 的文件）。
- README、HANDOFF、本 dev_log 的相對連結以腳本確認存在。

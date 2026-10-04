# Engine Math 基礎統一 B2：Collision 改用 GYO::Math

日期：2026-10-03。Owner：Engine（`GYO::Collision`），連帶 `object_fps_pvp`。
狀態：完成，PR [#19](https://github.com/yojinn-io/GYO-Engine/pull/19) 已合併（`5c8fd10`），CI 四平台通過（分支 `claude/math-foundation-b2`，自 master `6cc2829`）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b2-collision)，本文只記經過。

## 經過

1. 使用者定下 Collision 公開介面的原則：`Ray` 是幾何 primitive，`RaycastAabb`／`RaycastCapsule` 是 Collision 演算法，所以 raycast 改為接收 `Math::Ray`。
   同一原則下，`SweepSphereAgainstCapsule` 的球心路徑改為接收 `Math::Segment`。
2. 遷移 Collision：
   - header 刪除 `Float3`、`Aabb`、`Capsule`，改用 Math 的型別。
   - 兩個 .cpp 刪除本地 helper：double 路徑改用 `Math::Vec3d`、`Segmentd`、`Aabbd`；演算法本體不動。
   - B1 的 characterization 已確認這些 helper 和 Math 逐位元相同。
3. 遷移 pvp：
   - 有編譯的 5 個檔案，以及 29 個未編譯檔案中引用 Collision 的 8 檔，只做型別替換和 `Ray`／`Segment` 呼叫。
   - 第一次套用時有兩個問題：zsh 不會自動拆開變數，所以替換沒有執行；參數合併腳本也會誤改 pvp 自己的同名成員函式 `CombatCollision::RaycastCapsule`。
   - 改為只改寫 `Engine::Collision::` 限定的呼叫，並逐檔套用。
4. 驗證：
   - 背景 agent 用 master 版與 B2 版的 Collision 跑同一組 25 萬筆查詢，結果逐位元相同。
   - 靈敏度檢查：把 `kEpsilon` 改成 1.1e-6 時出現 737 行差異，證明比對抓得到細微的數值變化。
   - core 19／19、test 45／45 通過，pvp 未編譯 29 檔 syntax-only 全部通過，依賴圖只多兩條預期的邊。
5. test 數從 B1 的 42 增加到 45。查證原因：B0 之後 master 先合併了 PvP v5 的 PR #11–#16，新增了三項 pvp 驗收測試，跟 Math 計畫無關。
   pvp 有 28 個檔案變動，B6 開始時必須重新盤點。

## 留給下一步

- PR 與 CI 四平台驗收。
- B3（Model）：`AnimationTransfer` 的 `Inverse` 要改寫成 `Conjugate`，見 PLAN 第 2 節的 ADL 陷阱說明。

# Engine Math 基礎統一 後續：純量 Min／Max／Clamp 的統一

日期：2026-10-04。Owner：Engine（`GYO::Math`），連帶使用 Math 的模組、工具與 `object_fps_pvp`。
狀態：完成，PR [#28](https://github.com/yojinn-io/GYO-Engine/pull/28) 已合併（`574ae32`），CI 四平台通過（分支 `claude/math-scalar-unification`，自 master `fdc72e9`）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#後續純量-minmaxclamp-的統一)，本文只記經過。

## 經過

1. B6a–B7（#24–#27）依序合併後開始。使用者決定：float 純量 clamp／min／max 的寫法統一；Math 計畫的範圍外事項轉入 PvP v6 計畫。
2. 方針：使用者的樹狀圖把 Clamp、Min／Max 放在 Math 的 Scalar 底下，先統一 float。之後使用者追加決定：double 與整數也統一（std 受 C／C++ 版本與平台巨集影響），並讓 `GYO::Engine` 連結 Math。
3. Math 的 `Min`／`Max`／`Clamp` 改為 GYO 自己的 constexpr template。實作就是標準的定義，結果與 std 相同；引數須同型別。
4. 以 2 個 agent 分兩輪替換，約 120 個呼叫處：第一輪 float，第二輪 double、整數與 initializer-list。本機 core、test、Go、29 檔 syntax-only 全部通過；依賴圖只多出 `engine → gyo_math`。
5. 驗證（ultracode，2 個 agent）：
   - **對抗式審查**：確認推導型別在三個平台都與原本相同，沒有推翻正確性。
   - **殘留稽核**：範圍內已沒有算術型別的 std 呼叫。
   - 指出的事項都已處理：`Clamp` 的 debug 前置條件檢查、ui_editor core 的明確連結、死掉的 `<algorithm>`、兩處手寫的 min／max、文件中的舊說法。
   - 審查另外發現：Collision 的退化膠囊會違反 clamp 的前置條件，轉入 v6 清單。

## 留給下一步

- PR 與 CI 四平台驗收。
- 使用者合併 #28。

# Engine Math 基礎統一 B5：Ui 與 ui_editor 改用 GYO::Math

日期：2026-10-03。Owner：Engine（`GYO::Ui`、UiRenderer），連帶 `tools/ui_editor` 與 `object_fps_pvp`。
狀態：完成，PR [#23](https://github.com/yojinn-io/GYO-Engine/pull/23) 已合併（`efe4a30`），CI 四平台通過（分支 `claude/math-foundation-b5`，自 master `43bccad`）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b5-ui-與-ui_editor)，本文只記經過。

## 經過

1. 依全程 ultracode 的方針，先用 3 個 agent 分別盤點 Ui 本體、UiRenderer 與 ui_editor、gyo.ui 資料契約。
   - 確定了每個 helper 的處置。
   - 找出 PLAN 漏列的 pvp 檔案（`PvpApplication.cpp`、`ObjectFpsUi.cpp`）。
   - `ClipSprite` 改用 `Math::Intersection` 後，非有限的目標矩形會有不同行為，交由使用者決定：統一並一律報錯。
2. 前置測試先在 master 上通過，才開始遷移，內容包括：
   - gyo.ui golden 兩份；
   - sRGB 全 byte 往返；
   - JSON 陣列位置對應；
   - 半開區間的邊界；
   - 精確 layout 與 draw list；
   - `ClipSprite` 的有限值；
   - ui_editor 的 sRGB characterization。

   其中 sRGB decode 原本寫死了經過 `std::pow` 的常數，各平台的 libm 可能算出不同位元，改成在執行期與凍結的舊版函式比對。
3. 遷移：
   - `UiFloat2`、`UiRect` 移除。
   - `IntersectRect`、兩份 sRGB、`IsFinite(UiFloat2)`、`ConvertRect` 刪除，改用 Math。
   - `ContainsHalfOpen` 保留為 Ui 政策。
   - `ClipSprite` 依決定改寫，並新增「非有限目標矩形一律報錯」的測試。
   - 遷移後全部前置測試照樣通過。
4. 差異比對：master 與 B5 在有限輸入下全部逐位元相同，範圍包括 codec 突變、layout、Update 序列、104 萬個點的 hit test、368 萬次 renderer submit，以及 sRGB 窮舉。
5. 對抗式審查的 major：我向使用者描述的舊版 `ClipSprite` 行為不完整。舊版除了報錯和靜默丟棄，還會在寬或高為 +inf 時送出 UV 範圍為 0 的 sprite；文字對齊溢位也會觸發。
   補齊說明後，使用者再次確認維持一律報錯，並更正了註解、測試與 HANDOFF 的描述。其餘 5 個 minor 也都已處理。
6. 本機驗收：core 19／19、test 46／46，pvp 未編譯 29 檔 syntax-only 全部通過，依賴圖只多三條預期中的邊。
7. CI 第一次執行時 windows-x64 建置失敗：兩個新測試把 `std::string_view` 串進 doctest 訊息卻沒有 include `<ostream>`，MSVC 需要完整的 `std::ostream`。補上 include 後重跑。

## 留給下一步

- B6a：B0 之後 pvp 有大量變動，開始時必須重新盤點，再依 ultracode 方針實作與驗證。

# Engine Math 基礎統一 B4b：Renderer 矩陣切換到 Math 慣例

日期：2026-10-03。Owner：Engine（`GYO::Render`）。
狀態：本機驗收完成，PR [#22](https://github.com/yojinn-io/GYO-Engine/pull/22) 待 CI 四平台（分支 `claude/math-foundation-b4b`，自 master `ed7a08a`）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b4b-render-矩陣切換到-math-慣例)，本文只記經過。

## 經過

1. 先寫 memcmp 測試：
   - 凍結 master 的 row-vector 矩陣碼為 `Legacy`。
   - 用執行期隨機的 transform、camera、sprite 與視窗尺寸跑 Renderer，逐筆比對每個 draw 上傳的 64 位元組 uniform。
   - 測試先在未修改的 Renderer 上通過，確認凍結碼與原實作一致。
2. 改寫 `Renderer.cpp`：
   - 刪除 7 個私有矩陣 helper。World 改用 `ComposeEulerXYZ`；View 與 Sprite 改為 column-vector 的鏡像鏈；投影改用 `MakePerspective` 與 `MakeOrthographicPixels`。
   - WVP 維持 `(Proj·View)·World` 的結合方式；上傳時以 `ToShaderMatrix` 依序 memcpy 16 個 float，不轉置。
   - 改寫後 memcmp 的 20410 個斷言逐位元相同，數值漂移為 0。
3. 使用者決定本計劃全程使用 ultracode：底層概念模型的變更，而且已有大量實作依賴它。
4. ultracode 驗證：
   - **靈敏度檢查**：在 scratch 複本中做 8 種刻意的錯誤改動，全部被 memcmp 抓到。其中包括數學上等價、只改變捨入順序的 WVP 重新結合，以及只差 1 ulp 的 `xScale` 算法。
   - **對抗式審查**：只有 3 個 minor。Legacy 的來源標註已更正；測試替身重複的問題，改為把 memcmp 併入 `RendererTests.cpp`、重用既有替身；文件已補上。
5. 本機驗收：core 19／19、test 45／45，pvp 未編譯 29 檔 syntax-only 全部通過。

## 留給下一步

- PR 與 CI 四平台驗收。CI Linux 的 lavapipe GPU smoke 也會跑 `RenderFeatureSmoke`；它的獨立 oracle 與 memcmp 互相印證。
- B5（Ui、ui_editor）：依 ultracode 方針，以多 agent 盤點 Ui 本體、ui_editor 與 gyo.ui 資料契約後再實作。

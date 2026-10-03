# Engine Math 基礎統一 B4a：Render 型別與 helper 改用 GYO::Math

日期：2026-10-03。Owner：Engine（`GYO::Render`），連帶 `render/model`、UiRenderer、`object_fps_pvp`。
狀態：本機驗收完成，PR [#21](https://github.com/yojinn-io/GYO-Engine/pull/21) 待 CI 四平台（分支 `claude/math-foundation-b4a`，自 master `14a32ac`）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b4a-render-型別與-helper)，本文只記經過。

## 經過

1. `Render::Float2/Float3/Rect` 移除，Render 的提交與 GPU 契約型別改用 Math 的值型別；`Renderer.cpp` 的矩陣碼留給 B4b。
2. 重複的 helper 收斂：
   - 向量與 `Rect` 版的 `IsFinite` 刪除，改經 ADL 使用 `Math::IsFinite`。
   - PrimitiveMesh 的 `Add/Subtract/Scale/Cross/Length` 改用 Math。
   - ColorTransform 公開的 sRGB 元件函式與 Math 完全重複，依「舊名稱完全移除」直接刪除。
3. 第一次建置失敗：刪除 sRGB 函式前搜尋呼叫者時，結果被 `head` 截斷，漏掉 `Renderer.cpp` 與兩個測試。
   編譯器在第一次建置時就抓到了，修正後全部通過。
4. 背景 agent 量測漂移：
   - libc++ 的 `hypot` 在正常範圍與 `Math::Length` 相同。
   - 漂移只來自倒數相乘改成除法（27.5% 的分量差 1 ulp），線框頂點最大誤差約 4e-4。
   - 其他 Render 函式逐位元相同。
5. 對抗式審查發現一個語義問題：`Math::Length` 不做縮放，邊長超過約 1.8e19 時，線框函式會回傳 Ok 卻輸出 NaN。
   修正為回傳前檢查頂點都是有限值，否則回傳 `InvalidArgument`，並補上測試。
6. 本機驗收：core 19／19、test 45／45，pvp 未編譯 29 檔 syntax-only 全部通過，依賴圖只多 `render → math`。

## 留給下一步

- PR 與 CI 四平台驗收。Linux、Windows 的 `hypot` 與 libc++ 不同，線框網格可能另有少量 ulp 差異，現有測試不做精確比較。
- B4b：`Renderer.cpp` 的矩陣碼改用 Math 慣例。WVP 必須維持 `Multiply(Multiply(Proj, View), World)` 的結合方式。

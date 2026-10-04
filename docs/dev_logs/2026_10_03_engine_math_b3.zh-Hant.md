# Engine Math 基礎統一 B3：Model 改用 GYO::Math

日期：2026-10-03。Owner：Engine（`GYO::Model`），連帶 ufbx loader、`render/model` 與 `object_fps_pvp`。
狀態：完成，PR [#20](https://github.com/yojinn-io/GYO-Engine/pull/20) 已合併（`14a32ac`），CI 四平台通過（分支 `claude/math-foundation-b3`，自 master `5c8fd10`）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b3-model)，本文只記經過。

## 經過

1. Model 的 `Vec2/Vec3/Quaternion/Matrix4` 與 `Multiply`、`TransformPoint` 移除，改用 Math 的型別與函式。
   `Transform`（資產節點的 TRS）與 `ToMatrix` 是 Model 的契約，保留；`ToMatrix` 改為呼叫 `Math::ComposeTRS`。
2. 公開 header 一律寫 `Math::` 限定名稱；`.cpp` 檔在 `namespace Engine::Model` 內用 using-declaration，函式本體大多不必改動。
3. `AnimationTransfer` 的舊 `Inverse` 其實算的是共軛，三處呼叫全部明確改為 `Math::Conjugate`。
   若只刪掉本地函式，未限定的呼叫會經 ADL 靜默選到真正的 `Math::Inverse`。
4. `Animation.cpp` 的 `Sample()` 把插值函式當成 callable 傳入，Math 的多載無法推導，改傳 lambda。
5. 背景 agent 做了兩項驗證：
   - **digest**：master 與 B3 共 1207 萬筆輸出逐位元相同。9 種刻意改動都會被偵測到，包括把 `Conjugate` 換回 `Inverse`。
   - **對抗式審查**：只有 3 個 minor。用不到的 using-declaration 已刪除；`Finite` 驗證系列保留並記錄理由；計畫文件已補上。
6. 本機驗收：core 19／19、test 45／45，pvp 未編譯 29 檔 syntax-only 全部通過，依賴圖只多 `model → math`。

## 留給下一步

- PR 與 CI 四平台驗收。
- B4a（Render 型別）：`PrimitiveMesh` 的倒數相乘正規化換成 `Math::Normalize` 會有最多 4 ulp 的漂移（依平台而異），要記錄。

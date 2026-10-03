# Engine Math 基礎統一 B6a：pvp 模擬層 `match_domain` 改用 GYO::Math

日期：2026-10-04。Owner：`object_fps_pvp`（`match_domain`），連帶 pvp 的測試與 acceptance，以及 Math 的角度換算。
狀態：本機驗收完成，PR 待開（分支 `claude/math-foundation-b6a`，自 master `efe4a30`）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b6a-pvp-模擬層-match_domain)，本文只記經過。

## 經過

1. B5（PR #23）合併後開始。B0 之後 pvp 有大量變動，所以先以 3 個 agent 重新盤點，再由 1 個 agent 檢查遺漏。
   - 結論：大部分替換可以逐位元相同；只有平面輸入的倒數相乘正規化與碰撞的 `hypot` 會改變數值。
   - wire 上的 float 原樣傳送，沒有狀態 hash，client 每個 snapshot 都以 authority 重建，所以不需要升 protocol。
2. 使用者決定兩件事：
   - Math 的 `DegreesToRadians`、`RadiansToDegrees` 改為 constexpr，讓 `MovementMaximumPitch` 能直接使用。
   - 跨平台驗證採本機全模擬 digest，加上在 CI 四平台執行的 characterization 測試。
3. 實作：
   - `fps::Float3` 全部換成 `Engine::Math::Vec3`。
   - `fps::Float2` 改名為產品語義型別 `GroundPoint`，移到 `RetroFPS/World/`；`RetroFPS/Math/Vector.hpp` 刪除。
   - `match_domain` 的 helper 改用 Math。
   - 換型別後，29 個未編譯檔中有 3 個檔案的 helper 會與 Math 經 ADL 歧義：逐位元相同的直接刪除；會 throw 的 `Normalize` 改名為 `NormalizeOrThrow`。
   - 新增 pvp 的 characterization 測試。本機實測：平面正規化最多 1 ulp（推導上限 2）；水平法線長度最多 1 ulp、方向最多 2 ulp。
4. 驗證（ultracode，2 個 agent）：
   - **全模擬 digest**：把兩處漂移改回舊寫法的 isolation 版，在 62 個情境、兩種最佳化等級、兩種規模下都與 master 逐位元相同。
   - **master 對 branch**：差異只來自那兩處。authority 位置最大差約 2.7e-5 m，沒有離散狀態分歧；prediction 的 snap 與平滑修正次數不變。
   - **靈敏度檢查**：7 種刻意改動中 5 種被抓到，另外 2 種已證明是等價改動。
   - **對抗式審查**：沒有推翻正確性，只指出紀錄未更新與 2 個 nit，都已處理。
5. 本機驗收：core 19／19、test 46／46、gateway Go 測試通過；未編譯 29 檔 syntax-only 全部通過；依賴圖沒有新的邊。

## 留給下一步

- PR 與 CI 四平台驗收（characterization 測試的推導上限在 Linux、Windows 上實測）。
- B6b：pvp 表現層與 acceptance。盤點已在 B6a 進行期間完成。

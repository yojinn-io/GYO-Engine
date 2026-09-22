# 2026-09-22：v2 骨架敵人與碰撞可視化

本次實作範圍是 Object_FPS v2 的 3D 敵人、共用模型呈現、角色膠囊查詢與 F3 除錯顯示。
操作與資料契約見 [v2 敵人文件](../object_fps_v2/enemies.zh-Hant.md)，
責任分配見 [整體架構](../architecture.md)與[渲染架構](../rendering_architecture.zh-Hant.md)。

## Architecture Delta

| 變更 | 已觀察到的實作壓力／較小方案不足之處 | 責任與依賴 |
| --- | --- | --- |
| v2 domain 新增 Model 依賴 | 骨架姿勢現在決定受擊區及出招掛點；若仍由呈現層獨立推進動畫，命中與畫面會有兩個時鐘 | v2 gameplay → Model；狀態、時間、窗口及傷害仍由遊戲持有，Model 只提供取樣及混合 |
| 新增 GYO::ModelRenderer library | v2 武器與敵人都需要 CPU 蒙皮、材質上傳、每實例 mesh 更新與回收；複製武器程式會形成兩套資源生命週期 | ModelRenderer → Model + Render；AssetId／JSON 解讀留在 App，Model 與 Render 不互相依賴 |
| v2 敵人資料契約改版 | sprite 幀、圖集像素與像素槍口不能表達骨架命中及事件掛點 | v2 自有 CSV、character／animset／enemy JSON；不更改 v1 契約或資產 |
| Collision 加入 Capsule／Contact 查詢 | 穩定 body、任意方向的骨架區及拳部掃掠需要共用幾何查詢 | Engine 只持有數值幾何；GridMap 轉換、滑牆、平面敵人政策在 v2 adapter |
| Render WorldOverlay、幾何線框與 Input F3 | 除錯形狀需要世界相機與透視顯示，既有 ViewModel 相機及深度政策不適用 | 加法式 API；Render 不依賴 Collision；開關、顏色及哪些形狀生效由 v2 App 決定 |

未增加 top-level directory，也未移動 v1 的責任。ModelRenderer 位於
`engine/render/model`，是連接兩個既有模組的獨立 target，並非新的遊戲框架。
既有射線、直立膠囊、取樣、蒙皮與世界繪製介面仍可使用；沒有世界 overlay 時不增加 pass。

v2 測試與驗收工具分別由 `tests/object_fps_v2` 和 `build/acceptance/object_fps_v2` 擁有，
由既有 owner discovery 啟用，沒有共通程式中的產品名稱分支。
共通 Collision／Model／ModelRenderer／Render 測試使用合成資料；UAL 與 Mark23 的驗證屬於 v2。

## 基線與環境

修改前基線是 Git `e1afcc7b2ada95e0d04039a314fbbbb128d859a2` 的獨立 archive checkout。
基線 source、registry 副本、build tree、GPU 輸出與 log 皆在忽略的
`build/target/validation/enemy3d/`，不更改 repository 的預設產品選擇。

本機使用 Windows x64、MSVC、Ninja、RelWithDebInfo，以及已快取的第三方原始碼和 host shader tool。
此環境的 sandbox 子編譯程序會停滯，經同一最小專案比較後，建置改以已核准的非 sandbox 執行。
這是本機執行環境差異，沒有為此修改 Engine 或建置契約。

修改前 v1-only 已完成建置，產品 CPU 檢查 6/6 通過：headless、model assets、
package tools、startup smoke、headless smoke、package。

## 取樣與修正

UAL 實際取樣確認模型朝 +Z、左右手節點及 reference 高度；近戰左拳窗口設為
4/30 至 8/30 秒，遠程使用左手在 0.10 秒發射。右手在施法動作中位於身後，
不能沿用先前猜測的右手掛點。驗收工具的 `--inspect-rig` 可重新產生逐幀位置報告。

部署資料測試攔截了 `Head` 節點大小寫錯誤。頭部區域也改用實際主要受 Head 影響的
746 個蒙皮頂點包圍中心，轉回骨骼局部座標後設定偏移與半徑。
另以實際模型的頭頂高度做射線回歸，避免測試只重複同一套配置換算而漏掉受擊區偏低。

角色 adapter 的驗證發現並修正兩項 3D 接觸問題：玩家走離高處角色支撐後需重新檢查接地；
地面敵人在碰到跳躍玩家時，不能先沿 3D 接觸面滑動，再丟棄垂直位移。
玩家使用完整 3D sweep，平面敵人在同一查詢結果上限制水平滑動，沒有在 Engine 裡加入敵人特例。
開放邊緣地圖的既有阻擋政策則以四個邊界 AABB 表達；移動、射擊與線框都讀取同一份世界幾何。

## 驗證結果

已完成的檢查：

| 範圍 | 結果 | 本機證據（`build/target/validation/enemy3d/` 下） |
| --- | --- | --- |
| 修改前 v1-only | 建置與 6/6 產品 CPU 檢查通過 | `baseline-z7.log` |
| 修改後 Engine-only，沒有遊戲或工具 | 建置與 17/17 CPU 檢查通過 | `core.log` |
| 修改後雙產品共通／v1 | 建置與 26/26 既有 CPU 檢查通過 | `combined.log` |
| 修改後 v1-only 獨立 build tree | 建置與 26/26 CPU 檢查通過 | `matrix-v1.log` |
| 修改後 v2-only 獨立產品選擇 | 建置與 23/23 CPU 檢查通過；只註冊 v2 owner 測試／驗收 | `matrix-v2.log` |
| 修改後雙產品 v2 | 建置與 3/3 產品 CPU 檢查通過；25 案例、1,587 斷言 | `v2-final.log` |
| 玩家線寬調整後的雙產品／v2-only | 兩種配置各自增量建置，受影響的 3/3 產品 CPU 檢查再通過 | `v2-visual-final.log`、`matrix-v2-visual-final.log` |
| 完整配額與回收驗收修正後 | 雙產品／v2-only 的 acceptance 增量建置及 headless 檢查通過 | `v2-wave-final.log`、`matrix-v2-wave-final.log`，以及對應 `-ctest.log` |
| v1 基線／修改後 GPU | 兩版本 × D3D12／Vulkan × world／viewmodel／reload／muzzle，共 16 次通過 | `v1-gpu-results.json` |
| v1 畫面保存 | 248/248 張 BMP 的 SHA256 完全相同，沒有缺檔、多檔或差異 | `v1-gpu-comparison.json` |

v1 圖像比較在各後端內比較基線與修改後版本，不把不同後端的像素差異當成回歸。
每個後端有 18 張 viewmodel、91 張 reload、15 張 muzzle capture；world smoke 另做流程檢查。
v1 的 source 與 assets 沒有修改。

雙產品共通／v1 的 26 項與最終 v2 的 3 項合計涵蓋全部 29 項 CPU 檢查。
第一版驗收在 193 幀／8 發時達到開門門檻，但截圖仍有活敵人。
`clear_kill_count` 原本小於總配額，因此「門已解鎖」不是整波全滅的證據。
驗收條件改為全部配額擊殺、沒有活敵人、屍體完成回收，再產生最終清波畫面；
遊戲原有開門政策不變，測試也不提高玩家生命或修改敵人數值。
修正後兩種配置都在 378 ticks／6.3 秒模擬時間完成：`kills=4 quota=4`、
`remaining_enemy_instances=0`、10 發、生命 100。

第一次 D3D12 擷取確認模型與骨架形狀相符，但相機位於玩家 body 內部，
預設 0.012 公尺線寬投影成粗綠色條帶。玩家專用線寬因此調整為 0.0006 公尺，
不改變 capsule 的端點、半徑或碰撞。驗收鏡頭稍微下俯以納入腳部及完整倒地姿勢。

最終 headless、D3D12／DXIL、Vulkan／SPIR-V 都通過完整配額驗收，
結果均為 378 ticks、4/4 擊殺、0 剩餘敵人實例、10 發及生命 100。
GPU 程序從 repository 外的工作目錄執行，使用 executable-relative 已部署資產。
每個後端各產生 24 張動作／碰撞開關比較圖及 1 張完整清波圖。
重新檢視最終圖片確認頭部與四肢對位、有效拳部紅框、綠色 body 與青色牆框，
倒地後沒有生效碰撞框；清波圖中敵人與黃色受擊區完全消失。
最終 RenderQueue 另確認敵人網格已移除，裝備中的 Mark23／手臂仍有 ViewModel 提交。

日誌與圖片位於同一驗證目錄的：

- `v2-gpu/headless-full-wave.log`
- `v2-gpu/d3d12/run.log`、`v2-gpu/d3d12/*.bmp`
- `v2-gpu/vulkan/run.log`、`v2-gpu/vulkan/*.bmp`

GPU probe 中額外取樣 24 個角色姿勢的 CPU 蒙皮合計約 52.43 ms（D3D12 執行時）
與 51.68 ms（Vulkan 執行時），估計頂點上傳量皆為 13,526,400 bytes。
這是此機器的診斷樣本總量，不是單幀時間或正式效能基準；目前仍是 CPU 蒙皮與 unlit 材質。

Scene capture 不含最終 HUD，不能把它當成完整螢幕截圖；HUD 的圖例、順序與 F3 操作由 UI／Runtime 測試另外覆蓋。
本次實機驗證限 Windows x64 的 D3D12／Vulkan，不宣稱已執行 Linux／macOS／Metal 驗收。

最後檢查沒有 v1 source／assets／預設 registry 修改，`git diff --check` 通過。
新 Engine 程式沒有遊戲或 Editor 依賴；遊戲特有配置、支援程式與測試皆留在 v2 owner。

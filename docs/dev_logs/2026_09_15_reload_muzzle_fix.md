# 2026-09-15：換彈局部抽動與貼牆吞彈修復

> Historical record: this document preserves the implementation, commands and results from its original date. Paths and ownership may predate the current architecture; use [the current architecture](../architecture.md) and [build guide](../creating_apps.md) for present instructions. These results do not validate later revisions.

## 換彈局部抽動

症狀是每次換彈都在短暫區段出現手指、手腕或彈匣抽動。
先前只檢查動畫起點／中段／終點，無法涵蓋這些原始幀之間的異常。

Mark-23 已由作者烘焙為 30 FPS。相鄰原始幀可能使用等價但差異很大的
Euler 角表示姿勢，例如兩個軸同時變動約 180°，實際手腕方向只變動少量。
匯入器強制 `minimum_sample_rate=60`，讓 ufbx 在這些幀之間再次用 Euler
曲線插入姿勢，生成短暫大角度旋轉。GPU 頂點更新與深度測試沒有混用不同幀。

### GYO 的修正

- 保留 ufbx 的預設最低重取樣門檻 19.5 Hz；已烘焙的 24／30 FPS 動畫
  保留其原始姿勢，稀疏非線性曲線仍可使用 60 Hz 重取樣。
- GYO::Model 以四元數在保留的姿勢之間插值，不重新插入 Euler 分支旋轉。
- 原始片段時間、每個原始幀的姿勢與作者的快速彈匣動作均保留。
  沒有加入角速度上限、逐幀平滑或換彈專用的骨骼修補。

回歸判定分為兩層：原始幀必須與 ufbx 評估一致；原始幀之間則確認沿
四元數插值路徑連續，不能把原始 Euler 分支旋轉當成正確的中間姿勢。
113 個換彈原始幀的最大頂點誤差約 **0.000733 mm**。

新增 `--reload-smoke-test`，以 60 Hz 時間步進提交完整換彈過程。
搭配 `--capture-dir <directory>` 會輸出逐幀 BMP，可檢查 2.5–2.8 秒等
曾經漏掉的區段；這些是 HUD 合成前的場景影格。

## 貼牆吞彈

舊版玩家膠囊可合法貼近牆面約 0.25 m；虛擬槍口卻位於相機右側 0.28 m、
前方 0.35 m、下方 0.22 m。靠右牆時，射擊起點可能已在牆內。
射線查詢因此正確回報距離 0 的牆壁碰撞，表現為準星前方可見的敵人不受傷。

可重現案例：相機 X=1.75 m，牆面 X=2.0 m，偏移後的槍口 X≈2.01073 m。
相機瞄準射線命中敵人，原本的槍口射線卻立刻命中牆壁。

### Object_FPS 的修正

1. 從相機射線取得原本的瞄準點。
2. 以 `CombatCollision::ClampSegmentToWorld` 檢查相機到期望槍口的線段。
3. 遇牆或地板時，把槍口退到首次碰撞之前 1 mm。
4. 使用修正後的槍口繼續執行既有的「槍口 → 瞄準點」碰撞查詢與 tracer。

這個線段限制屬於 Object_FPS 的格子世界適配；底層 AABB／膠囊射線數學
仍由 GYO::Collision 提供。碰撞半徑與牆壁範圍沒有放寬，真正擋住彈道的
正面牆、薄牆與轉角仍會阻擋射擊，也不會把槍口直接移到牆的另一側。

## 驗證

- FBX：**9 tests / 27,616 assertions**，涵蓋五個片段所有原始幀、
  完整換彈的手腕子幀，以及等價 Euler 旋轉的合成案例。
- Gameplay：**767 assertions**，含側牆實際扣血、正面牆阻擋、薄牆、
  轉角、俯視地板及跳躍高度；一發仍消耗一顆子彈並造成 25 點傷害。
- Windows／VS 18 Debug 整合 CTest **15/15 通過**，含新增
  `object_fps.reload_smoke`。修正前後各輸出完整換彈序列共 226 張 GPU
  影格，對照確認約 2.68 秒的手腕翻轉已消失。

# Player_V19／nailer_v2 換模：兩項判定變更（AGENTS §14.4 的 1～5）

- 狀態：**兩項都已核准**（2026-10-29，使用者）。
  - J-muzzle 原話：「核准這次判定變更，包含俰角路徑只剩上限保護的盲點。」
  - J-accessory 原話：「核准頭髮判定變更，包含「沒宣告配件時頭髮是否呈現只靠截圖」的放寬。」
- 起因：人物換成 `characters/player/Player_V19.fbx`（自帶頭髮、沒有配件）；第一人稱換成 `weapons/nailer_v2/viewmodel/`（取代 PvP 的 Mark23），並保留模型上 15° 的偏航以展示槍身，再仰 8.2115° 讓槍身延長線穿過準星。
- 兩項都不是 Design Invariants 的守衛；都有放寬的一面，所以照 §14.4 先做突變檢查與另一位審查者（Sonnet 子 Agent，只讀）的核對，再請使用者核准。
- 證據放在本機 scratchpad（未提交）；結論與數字記在本文件。

## J-muzzle：正式第一人稱槍口的特性測試

對象：`tests/object_fps_pvp/PlayerPresentationTests.cpp` 的
`characterization: production weapon muzzle keeps its load-time value and drifts only under recoil`（math-foundation 留下的特性測試）。

### 1. 守的性質
正式第一人稱武器的槍口位置：Math 版 `fps::EvaluateWeaponMuzzleViewCameraPosition` 與凍結的舊算法
`PresentationLegacy::MuzzleViewCameraPosition`，在載入時的 shot 與不加後座時數值完全相等（±0 視為相同）；
加後座時差距 ≤ 32 u M（註解假設 M < 2 m）。槍口在 near clip 之外（未變）。

### 2. 放寬的面
當正式擺放不是「X、Z 旋轉為 0 且縮放 1」時：
- 不加後座的樣本、與載入時的 shot，不再要求完全相等，只要求 ≤ 3.8147e-06 m。
  本次內容（`[8.2115, 180, 0]`）192 個不加後座的樣本有 21 個不相等，最大絕對差 5.96e-08 m。
- 盲點（已知並核准）：X、Z 旋轉與縮放路徑的 1 ulp 級錯誤，在正式擺放上只剩上述上限保護（約為實測差的 64 倍）。
  突變「`(point - anchor)` 乘 `1 + 2^-23`」在新內容下最大差 1.19e-07，上限不抓，只靠下面的 yaw-only 投影檢查抓到。

### 3. 收緊的面
沒有。正式擺放是 yaw-only 時，新判定與舊判定等價。

### 4. 放寬後由哪裡守
- 同一測試：正式擺放的 yaw-only 投影（相同平移、只留 Y 旋轉、縮放 1），在所有 clip 與取樣時間，新舊算法數值完全相等。
- 同一測試：32 u M 上限（含後座樣本）。
- `PresentationMathCharacterizationTests.cpp`：100000 個 yaw-only 單位縮放樣本完全相等；100000 個隨機樣本（含縮放 0.05–2）≤ 32 u M（實測最大 2.96 u M）。
- 以上都不涵蓋 X、Z 旋轉與縮放路徑的 1 ulp 級誤差（第 2 點的盲點）。

### 5. 證據
- 突變矩陣：M0 不突變、M1 拿掉 anchor、M2 X／Y 對調、M3 忽略縮放、M4 乘 1+2^-23、M5 拿掉俯仰、M6 拿掉平移；× 舊／新判定 × 舊內容 `[0,195,0]`／新內容 `[8.2115,180,0]`。
  - 舊判定在舊內容抓到 M1、M2、M4、M5、M6；新判定在新、舊內容都抓到。
  - M3 在兩種判定都存活：所有用到的擺放縮放都是 1，在此內容下是等價突變。
  - 「舊判定 × 新內容」欄因 M0 就失敗，不納入判讀。
  - 矩陣未涵蓋：只影響 X、Z 旋轉係數的 1 ulp 擾動；載入時 shot 的計算路徑錯誤（選錯 clip、時間、錨點）。
- 獨立審查：確認上述 1～5，指出的修正已納入。另記一個既有缺陷：`std::max({a,b,c})` 會吞掉 NaN，y 分量 NaN 不會被比較抓到（舊判定同樣）。

## J-accessory：GUI 驗收的配件（頭髮）判定

對象：`build/acceptance/object_fps_pvp/player_short.hpp`（GUI probe 的 Require）與
`build/acceptance/object_fps_pvp/player_presentation_evidence.py`（`validate_samples`）。
新增觀測欄位 `PlayerPresentationObservation::declaredAccessories`（證據 JSON 的 `declared_accessories`），
單元測試 `test_player_presentation_evidence.py` 加 3 個案例。

### 1. 守的性質
遠端角色成功呈現時，身體、手上武器與上半身遮罩存在（未變）。舊判定另要求 `hair_meshes > 0`；
這個值由定義加總每個配件模型的網格數，不是實際送出的數量，所以實際守的是「定義裡有配件，且其模型有網格」。

### 2. 放寬的面
- 沒宣告配件且配件網格為 0 時通過（舊判定一律失敗）。
- 「定義被拿掉配件」不再被任何自動判定抓到；正式內容只剩 `PlayerPresentationTests.cpp` 要求 `accessories.empty()`。
- 沒宣告配件時「頭髮有被呈現」沒有自動判定守，只靠人看 capture 截圖（已知並核准）。V19 的頭髮屬於身體網格，`body_meshes > 0` 不檢查其中有頭髮。

### 3. 收緊的面
- 宣告 N 個配件但配件網格總數 < N → 失敗；沒宣告卻有配件網格 → 失敗。
- 真實資料下兩者都不可達：兩個值來自同一份 `accessories`，且模型載入保證至少 1 個網格。只對手工或竄改的證據有效。判定是總數比較，不是逐個配件。
- 缺 `declared_accessories` 的舊證據，新 validator 會以不明顯的錯誤判 failed（repo 未追蹤舊 frames）。

### 4. 放寬後由哪裡守
- 有宣告配件時：配件網格總數 ≥ 宣告數（比舊的 > 0 更嚴，真實資料下等價）。
- 沒宣告配件時：沒有自動判定；靠 capture 截圖。

### 5. 證據
- ctest 84/84；GUI `capture`、`player60` 通過；正式內容遠端角色樣本 3142 個全部是（網格 0、宣告 0）。
- GUI 突變矩陣（`player60`；內容 new＝正式，buns＝V19 加舊髮髻配件）：
  - M0：舊判定 × 正式內容失敗（起因），其餘通過。
  - 不計配件網格：舊、新判定在 buns 都抓到。宣告數變 0：只有新判定有此欄位，在 buns 抓到。
  - 不把配件送去渲染：新、舊判定都沒抓到（既有缺口，觀測值由定義算出）。
  - 被抓到的 run 都確認失敗訊息是配件判定；GUI 矩陣驗的是 C++ probe（Require 先中斷）。
- Python validator：拿掉檢查、只要 > 0、允許未宣告網格，加上審查者另做的 4 個突變，全被單元測試抓到；舊 validator 對 3 個新案例都給出不同結論（對應第 2、3 點）。
- 名稱：欄位仍叫 `hair_meshes`／`hairSubmittedMeshes`，實際意義是「配件網格」，未改名以縮小變更。

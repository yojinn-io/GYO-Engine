# 角色移動的缺陷：pm-detect → pm-vertical（產品線）

更新：2026-11-07。Owner：`object_fps_pvp`（產品會話）。來源：使用者用 biome 試玩後的回饋，架構師分類（[big-map HANDOFF](../../../architecture/plans/big-map/HANDOFF.md)「觀測：試玩回饋」；決定 `v8-arch-big-map-gravity`、`v8-arch-big-map-scale`）。
**不在 big-map 下**：是角色移動的事，拿一張真的地圖玩才露出來。與 `pa-broadphase` 共用 `Movement.cpp`，排在它合併之後、`scene v1` 之前。

## 症狀（使用者，2026-11-06）

1. 跳到兩塊方塊之間會卡在空中，之後不能跳。
2. 碰到空氣牆，看不到被什麼擋住。
3. 沒有自然重力。

## 從程式碼可證明的機制（架構師的子代理，2026-11-07；`apps/object_fps_pvp/src/Collision/CharacterCollision.cpp`、`Movement.cpp`）

- y＝0 是三重保證的隱藏地板（`Movement.cpp:87-90`、`MoveCharacterBody:145`、`IsCharacterSupported:71`），x/z 無限延伸：坑掉不下去、掉出地圖邊緣落到 0 被四周方塊側面包住。
- 支撐＝往下掃 2 mm 且法線 y＞0.5；側面不算、邊緣算（球心可超出邊緣約 0.87r 仍算站著）。
- 沒有跨階：高度 ≥ r 的台階接觸在赤道、法線水平＝硬牆。
- 容差不一致：產品的穿透解除只處理 depth＞1e-5，Engine 的掃描對 depth＞1e-7 的任何方向位移都回 fraction 0 → depth 在 (1e-7, 1e-5] 時全方向凍結（float 在 8～16 m 的 ulp 約 1e-6）。
- r 0.25＝半格：一格縫＝2r，兩側同時接觸時穿透解除來回彈，8 次後仍嵌入約 1e-4，掃描回 0，每 tick 重複。
- 解出的 y 與 desiredY 差＞1e-4 就把垂直速度歸零（`Movement.cpp:94`）；懸空又不算支撐 → 不能跳 → 永久懸空；擦邊時下落速度也歸零（一格一格飄）。
- arena 的 `width/depth` 不限制移動（只畫棋盤地板與驗出生點）：空氣牆不是矩形邊界。候選：身高 1.8 m 撞 1.5 m 開口、無跨階、一格寬走廊＝2r、凍結區間。

哪一個是現場碰到的，要先有檢測。

## 批次

```text
movement-fix
├── pm-detect-diag  上段｜high｜Opus｜估 4～6 h｜**11/9 場次之前**（使用者 2026-11-07，架構師轉達：pm-detect 拆兩半）
│                   可開關的移動診斷（每 tick 位置、desiredY、解出的 y、接觸法線、嵌入深度、supported、grounded、verticalVelocity），
│                   Match 與 Client 預測都能開；只記錄、不改任何行為與判定（不進 digest、不上 wire）；場次開著收 2～3 人的真實遊玩。
│                   [事前宣告](v8-product-pm-detect-diag-declaration.md)。
├── pm-detect       medium→high｜Opus｜估 3～5 h｜**lag-l2 之後**
│                   用場次的診斷 log 重現症狀 1、2 並存成重放；判定先宣告（哪個機制、重現率）；
│                   順帶量「現在的 stair 半格 0.25 m（＝r）能不能上」（不能就回報使用者，`v8-arch-big-map-gravity` 說不做跨階）。
│                   §14.3：把現象固定成失敗的測試，再修。
└── pm-vertical   high（判定局部 xhigh）｜實作 Opus／審查 Opus／驗證 Fable xhigh（比主會話強，開卡時確認）｜估 10～16 h
                  去 y＝0 隱藏地板（地板只來自地圖；掉出範圍的規則由產品定）；下落不被擦邊歸零；貼地吸附；容差統一（1e-5／1e-7）；角色 r 0.25 → 0.3（arena 欄位）；
                  症狀 1、2 的修正照 pm-detect 固定的測試；**不做跨階**。
                  wire：若 state 已有 verticalVelocity 則不變；否則是 pv 變更，停下回報。
```

## 順序（2026-11-07 更正）

pa-broadphase（已合併，#167）→ **pm-detect-diag** → 11/9 場次（lag-l2＋K8，開著診斷）→ hitbox-rewind → pm-detect（用場次的 log）→ pm-vertical。
行為的改動（pm-vertical 的任何一項）等場次量完 lag-l2／K8 才做，否則場次多一個變因。純資料的緩解（biome 的 `radius` 0.3）由使用者決定場次用不用，用的話記在場次紀錄。

## 開卡時

- 產品會話補事前宣告（判定、閾值、試行次數），使用者核准後才動。
- 證據放主 checkout `build/evidence/movement-fix/`；dev_log 在產品線。

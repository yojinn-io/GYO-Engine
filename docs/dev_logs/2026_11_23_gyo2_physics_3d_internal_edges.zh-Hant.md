# 2026-11-23 3D 物理：`BoxSet` 的內部邊（S1，`v8-arch-gyo2-physics-3d`）

Owner：架構（`engine/modules/physics_3d`）。同一張「3D 物理」任務卡；主會話 2026-11-23：「請在本卡開第三個 PR 做 (b)：照 parry 的 `FIX_INTERNAL_EDGES`，`BoxSet` 加一個作者可選的旗標，疊在 #311 上」「閾值在看結果之前就定好」。分支 `claude/gyo2-physics-3d-internal-edges`（開向 #311 的分支）。作者 Opus 5.5／high；實作子 Agent Opus／high。

## 做了什麼

1. **事前判定先 commit**（84d09ac3，宣告第 12 節）：旗標打開時每一步抬高 ≤ 1×10⁻⁵ m、|水平速度| ≥ 2.997；旗標關閉照舊；真的外緣不被抹掉；既有測試與 golden 不變。
2. **`BoxSetFlags{None, FixInternalEdges}`**、`BoxSet::Build(boxes, flags)`（`Build(boxes)` 是 `None`，照 parry 的預設：`Compound::new` 設 `CompoundFlags::empty()`）、`Flags()`。flags 進 digest（不是 `None` 時才混入，`None` 的 digest 不變）、`==`、`Collider` 的雜湊（CCR-13）。
3. **照 parry 的做法**（b3c3584）：旗標 `src/shape/compound.rs:30-40`；pseudo-normal 的計算 `compound.rs:490-636`（兩塊共有的面是切口、外面的邊取朝隣面的半角、切口那一側退化成面法線）；夾回 `compound_pseudo_normals.rs` 的 `project_local_normal_mut`、`pseudo_normals.rs:9` 的 `project_into_cone`。對軸對齊方塊：切口的邊把延續那一面的軸分量丟掉再正規化；真的凸外緣錐開 90°、法線不變；穿透時不從切口的面脫出。偏離 parry：parry 預設把 4 ULP 內的頂點焊在一起，這裡只認 float 完全相等的相接（規則寫在 `Shapes.hpp` 的 `BoxSetFlags` 註解與 owner 頁）。
4. 建立時對每個方塊每一面找一次隣居（BVH 挑候選、精確判定），存在隱藏的 `BoxSetData`；單方塊查詢讀「這個方塊＋建立時找到的隣居」，不靠 BVH，P3-3 照樣成立。只改法線，不改距離與點；射線只回面法線，不受影響。

## 查證（本機 macOS Intel，`core` preset）

| §12 的情境（兩個方向相同） | 最大 y（靜止 1.00999999） | 抬高 | 最小 \|vx\| |
|---|---|---|---|
| 旗標打開 | 1.00999999 | 0 | 3 |
| 旗標關閉 | 1.01882386 | 8.82 mm | 2.70010996 |

- 判定 1、2：通過（閾值沒有改）。判定 3：旗標關閉的抬高仍在、數值和 #311 的量測一致到 6 位（案例的容差 5×10⁻⁷）；**逐位元相同**由判定 5 承擔：旗標關閉時兩份 golden（含 `MoveAndSlide` 的一份）位元不變。判定 4：孤立方塊的頂邊、一組的遠側邊、台階的頂邊仍給傾斜法線。判定 5：既有測試全過。
- 旗標打開的等價測試：300 個方塊的格子語料、400 個查詢，參照是「方塊 i 加上測試自己獨立找的隣居」；相接的組 3102、接觸 2669、其中法線被旗標改的 569 件逐位元一致。
- `ctest -L cpu -j8`：214/214；doctest 53 pass、0 skip。
- 突變（新的 5 個）：`p3-ie-flag-ignored`、`-coverage-ignored`、`-outer-edge-clamped`、`-face-exit-ignored`、`-digest-no-flags` 全 killed。`-digest-no-flags` 第一版（只拿掉 `^=` 那一行）存活：`*= prime` 還在、digest 照樣變，沒有代表「忘了把 flags 放進去」；改成拿掉整個 if 區塊後 killed。

## 範囲の限界

- 只認完全相接的隣居（差 1 ULP 不算延續）；只在同一個 `BoxSet` 裡找（`BoxSet` 之間、`BoxSet` 與 `Cuboid` 的接縫不處理）；射線的法線不夾。
- 旗標打開時，對 `BoxSet` 的掃掠沒有逐位元等價測試（用接縫的情境與固定案例確認）。
- golden 在 Intel Mac 產生；三平台由 L1 確認。

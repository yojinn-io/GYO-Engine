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

- 判定 1、2：通過（閾值沒有改）。判定 3：第一版只比到 6 位（容差 5×10⁻⁷），設計審查指出這是放寬了宣告；改成逐位元：在暫時的工作樹建 80ff74e3（#311），以 `--no-skip` 跑原本 skip 的案例、印出每一步 position、velocity 的位元（記錄在本機 `build/evidence/s1-seam-80ff74e3-steps.log`，git 不追蹤），判定 3 用 `std::bit_cast<uint32_t> ==` 比 maxY、minSpeed、全部步的 FNV-1a 64 摘要、最後一步 6 個值：一致。判定 4：孤立方塊的頂邊、一組的遠側邊、台階的頂邊仍給傾斜法線。判定 5：既有測試全過。
- 旗標打開的等價測試：300 個方塊的格子語料、400 個查詢，參照是「方塊 i 加上測試自己獨立找的隣居」；相接的組 3102、接觸 2669、其中法線被旗標改的 569 件逐位元一致。
- `ctest -L cpu -j8`：214/214；doctest 53 pass、0 skip。
- 突變（新的 5 個）：`p3-ie-flag-ignored`、`-coverage-ignored`、`-outer-edge-clamped`、`-face-exit-ignored`、`-digest-no-flags` 全 killed。`-digest-no-flags` 第一版（只拿掉 `^=` 那一行）存活：`*= prime` 還在、digest 照樣變，沒有代表「忘了把 flags 放進去」；改成拿掉整個 if 區塊後 killed。

## 設計審查（Opus／high，唯讀；作者同為 Opus）與預設值

blocker 0、should 5、nit 4。should 1（判定 3 放寬）照上面收緊；should 2：偏離 parry 的四處寫成衝突點（owner 頁第 3 節：(a) 逐點的部分覆蓋 vs `face_key` 整面；(b) 只改法線、距離與點不變 vs parry 沿新法線重算或丟掉接觸——主會話同意，寫成 Design Invariant 草稿 P3-7；(c) 掃掠也夾、時間 0 的分離判定用夾過的法線，旗標會改變有沒有撞到；(d) 六面全被包住的方塊照舊取推出量最小的面）與不焊接 4 ULP 的理由；should 3：重疊的方塊不當延續，寫進限制（契約要不要禁止重疊，主會話記進批次表，等地圖與體素那一批）；should 4：旗標打開的等價測試加上 `CastShape`（參照是新的公開函式 `CastShapeOnBox`，即 P3-3 的單方塊掃掠；掃掠命中 351、接觸 2613 件逐位元一致）；should 5：膠囊沿兩塊並排的牆滑過垂直接縫（旗標關閉：被推離牆 8.2 mm、速度降到 2.511；打開：0 與 3，用 §12 同樣的閾值）——第一次用 0.5 m/s 壓向牆、掃掠沒撞到牆、兩種情形沒有差，改成 2 m/s；nit 1：`BoxSetFlags` 改成位元遮罩（`HasFlag`）。

**預設改成打開**（主會話 2026-11-23 判，宣告 §12 修訂 3）：`Build(boxes)` 等於 `Build(boxes, FixInternalEdges)`；判定 1～5 不變、打開與關閉都測。釘住旗標關閉行為的測試改成明確傳 `None`（兩份 golden 的場景、線性掃描等價、空集合、旗標打開等價裡的對照組），期待值一個都沒改、golden 沒有重產；唯一改了期待值的是確認「`Build(boxes)` 的意思」那一行（它就是在測預設值）。`BoxSet{}`（沒有旗標）和 `Build({})`（預設打開的空集合）的 `==` 與摘要不同：接受（空集合沒有接縫，旗標只是資料）。

突變：`find` 或殺它的測試改過的 4 個重跑 4/4 killed。全部 cpu：212/214，`gyo_os_tests`、`os.wake_fallback` 在負載平均約 43（別的工作樹並行建置）時逾時，單獨重跑 1.31 秒通過；和 physics_3d 無依賴。沒有在無負載時重跑全部。

## 範囲の限界

- 只認完全相接的隣居（差 1 ULP 不算延續）；只在同一個 `BoxSet` 裡找（`BoxSet` 之間、`BoxSet` 與 `Cuboid` 的接縫不處理）；射線的法線不夾。
- 旗標打開時，對 `BoxSet` 的掃掠沒有逐位元等價測試（用接縫的情境與固定案例確認）。
- golden 在 Intel Mac 產生；三平台由 L1 確認。

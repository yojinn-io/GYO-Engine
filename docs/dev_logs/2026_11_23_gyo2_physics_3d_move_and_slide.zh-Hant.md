# 2026-11-23 3D 物理：MoveAndSlide（運動學角色的機制，`v8-arch-gyo2-physics-3d`）

Owner：架構（`engine/modules/physics_3d`）。同一張「3D 物理」任務卡；主會話 2026-11-23 定「角色控制器在這張卡接著做，另開一個 PR，疊在 #309 上」。分支 `claude/gyo2-physics-3d-move-and-slide`（開向 `claude/gyo2-physics-3d-7d88e9`，#309 合併後改開向 `gyo2`）。作者 Opus 5.5／high；實作子 Agent Opus／high；設計審查 Opus／high（**作者與審查者同為 Opus**：Fable 暫停）。
根據：PvP 移植準備卡的盤點（推出、掃掠滑動、著地探測、放置檢查、相機防穿牆）；avian e496f4a `src/character_controller/move_and_slide.rs`、`velocity_project.rs`；rapier 475caa3 `src/control/character_controller.rs:173`（對照）。不看 master 的 `MoveCharacterBody`。

## 做了什麼

1. `MoveAndSlide.hpp`／`.cpp`：`ProjectVelocity`（avian 的暴力版）、`CastMove`／`CastMoveWith`（avian 的 `cast_move`＋`pull_back`）、`Depenetrate`／`DepenetrateIntersections`（Gauss-Seidel）、`MoveAndSlide`（avian 的迴圈：先推出 → 掃掠、收平面、投影 → 最後推出；`onHit` 回 Accept／Ignore／Abort）。預設值同 avian。
2. `SpatialQuery.hpp`：`ForEachContact`（每個 collider、`BoxSet` 每個方塊一個接觸）；`Detail::ForEachCollider` 的 visit 可要求停止，`ForEachContact`、`Intersections` 共用（設計審查 S2）。
3. 偏離 avian（都寫在 owner 頁第 3 節）：`collisionDistance` 照 avian 文件的意思填命中距離（它的程式填掃掠長度，`intersects()` 因此幾乎不成立）；接觸平面的 `onHit` 傳那個接觸的實體（avian 丟掉 closure 的實體參數）；命中點取在 collider 上；推出設定是一個 `DepenetrationConfig` 成員（S3）。
4. 作者的規則不放進來（§1-1）：站地、坡度、台階、貼地、重力（rapier 綁在控制器裡）。

## 查證（本機 macOS Intel，`core` preset）

- `ctest -L cpu -j8`：214/214 通過；doctest 46 pass、1 skip（S1 的量測案例）；兩份 golden 的值不變。
- 突變：新的 `p3-mas-pull-back-sign`、`p3-mas-similar-planes-less-blocking` killed；`p3-mas-rejection-ignored` 依審查 N6 拿掉（門檻是 avian 繞過 parry 錯誤的權宜，留作設定、不用突變釘死）；`find` 或殺它的測試改過的重跑 2/2 killed。

## 設計審查（Opus／high，唯讀）

blocker 0、should 4、nit 6。S2、S3、S4（Unsupported／UnsupportedScale 往上傳的測試）、N1～N4、N6 照改；N5 不動（avian 也一樣，等作者需要）。

**S1：`BoxSet` 的內部邊——量到了，是已知缺陷。** 兩個相鄰同高的方塊、`CapsuleY(0.5, 0.5)` 以 (∓3, −1, 0) 越過接縫：最大 y 高出 8.8 mm、水平速度慢約 10%（數字與條件在 owner 頁第 3 節）。修法照 parry 複合形狀的 `FIX_INTERNAL_EDGES`（`src/shape/compound.rs:30-40`）在 `BoxSet` 加旗標；屬 Engine 的機制、主參考有先例，卡片判斷選它（不選「工具烘焙時合併」：接縫減少但不會消失；不選「交給作者在 `onHit` 忽略」：每個產品各寫一份規則）。估計超過時間盒子，下一個 PR 做，這個 PR 先標明缺陷（stable version first）。

## 範囲の限界

- golden 在 Intel Mac 產生；三平台由 L1 確認。
- `CastMoveWith` 是公開函式（`std::function` 帶入形狀掃掠，N4 把 `Detail` 從標頭拿掉的代價）。

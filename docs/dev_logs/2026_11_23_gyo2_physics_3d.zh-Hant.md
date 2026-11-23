# 2026-11-23 3D 物理 Module 薄起步（`v8-arch-gyo2-physics-3d`）

Owner：架構（`engine/modules/physics_3d`）。GYO2 主會話開的任務卡（使用者「開 W、A、3D 物理、PvP 移植準備」）。分支 `claude/gyo2-physics-3d-7d88e9`（開向 `gyo2`）。作者 Opus 5.5／high（卡片會話）；實作子 Agent Opus／high；設計審查 Opus／high（**作者與審查者同為 Opus**：Fable 暫停）。
根據：HANDOFF `v8-arch-gyo2-2d-and-3d`、`-transform-camera-physics-modules`、`-hierarchy-bevy-and-primary-reference`；[事前宣告](../architecture/target/v2/physics-3d-declaration.zh-Hant.md)（修訂 1）；owner 頁 [physics-3d.zh-Hant.md](../architecture/target/v2/physics-3d.zh-Hant.md)。

## 做了什麼

1. **需求**：從 `origin/master` 的 PvP 與 `engine/collision` 盤點（只當需求來源），和「PvP 移植準備」卡的盤點（`local_8e95f98a`，主會話轉來）對齊：七類查詢（推出牆的重疊、掃掠滑動、著地探測、放置檢查、射線、骨頭上的 hitbox、相機防穿牆）；沒用到的（移動中角色互推、台階、斜坡、三角網格、OBB、剛體）不做。
2. **新 Module `physics_3d`**（`default=no`、`depends=transform`）：形狀（`Ball`、線段膠囊 `Capsule{a, b, radius}`＋`CapsuleY`、`Cuboid`、`BoxSet`，照 parry）、純查詢（`CastRay`、`FindContact`、`FindDeepestBoxContact`、`FindBoxContacts`、`CastShape`；不支援的組合回 `Unsupported`）、`BoxSet`（靜態 BVH；加速結構不改結果）、元件 `Collider`（登記名 `Physics3d.Collider`）、接作者 Query 的空間查詢（`Without<ChildOf>` 編譯期要求、縮放回 `UnsupportedScale`）。
3. **數值**：內部 double、結果捨入成 float 一次；只用四則、`sqrt`、比較；三平台 golden（字面 binary32）。

## 查證（本機 macOS Intel，`core` preset，RelWithDebInfo）

- `ctest -L cpu -j8`：213/213 通過（含 `physics_3d.queries`、`.box_set`、`.golden`、`.world`，`naming.guard`、`transform.module`、`camera.module`）；physics_3d 的編譯警告 0。configure 時的負向編譯樣本：少 `Without<ChildOf>` 被拒（`GYO_PHYSICS3D_REQUIRE_WITHOUT_CHILD_OF`）。
- 先用 stub 的 `Queries.cpp` 看到 28/31 失敗，再換實作（TDD）。
- 突變 `physics_3d_mutations.json`：第一輪 8/8 killed（第一次 `p3-boxset-candidates-too-small` 存活：等價測試的參照「只有一個方塊的 `BoxSet`」也走同一個候選選擇；改成根只有一個葉子時不看查詢範圍、全部測，參照才和候選選擇無關）。設計審查之後重跑殺它的測試被改過的 7 個：7/7 killed（`p3-boxset-ray-prune-equal` 的 `find` 與殺它的案例都沒動，不重跑；D184）。golden 在審查修改前後一個位元都沒變。

## 設計審查（Opus／high，唯讀）

blocker 0、should 2、nit 4。處置：
- S1（BVH 的內部佈局在公開標頭）：搬到 `src/BoxSetData.hpp`，公開標頭只前向宣告，`Data()` 拿掉、改 friend 的內部存取。
- S2（膠囊兩種表示、兩個世界座標的射線多載）：照 parry 改成局部線段 `Capsule{a, b, radius}`＋`CapsuleY`，拿掉兩個多載；掃掠的條件改看 `b − a` 在方塊座標裡恰有兩個分量為 0。
- N1：`Intersection.hpp` 的註解從舊的 Collision module 改指 `physics_3d`（只有註解）。
- N2：邊距只挑候選的推理寫進宣告 4.4。
- N3：純查詢連帶 Kernel：記成 RC-P3-4，現在不拆。
- N4：`Sphere` 改名 `Ball`（parry 的名字，避開 `Math::Sphere`）。
- World 查詢遇到不支援的組合整個回 `Unsupported`：維持（P3-4、§1-9），作者用 `filter` 排除玩家。

## 範囲の限界

- 別家原始碼：使用者同意後下載（`v8-arch-gyo2-physics-reference-downloads`），宣告第 5 節附檔名行號；實作時（下載前）依記憶寫的語意已對原始碼查過，差異寫在第 5 節。
- golden 在 Intel Mac 產生；三平台由 L1 確認。MSVC 只靠 L1。

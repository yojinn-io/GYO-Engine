# PvP v8：v8-product-hitbox-data-pose（D2：Client 與 Match 共用姿勢組合）

日期：2026-11-01。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 4～6 h，實際約 1.5 h。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-data-declaration.md)的 D2（`v8-product-hitbox-pose-sharing` (a)）。

## 做了什麼

- **搬家：** `PlayerPresentation.cpp` 的 CPU 部分原樣搬到新檔 `PlayerPose.cpp`，放進 `player_content`。搬過去的有：
  - 定義的載入；
  - jog 權重與一圈距離；
  - `AdvancePlayerLocomotion`、`ResolvePlayerActions`、`SamplePlayerPresentationPose`。
  - `PlayerPresentation.cpp` 只留 GPU 的資源與提交。
  - 標頭（`PlayerPresentation.hpp`）不變，所以呼叫端不必改。
- **Match 的定義：** `LoadPlayerPoseDefinition(assets 或 Assets 資料夾, 身高)` 和 Client 共用同一個載入函式，只是：
  - 角色用 `LoadCharacterAnimationDefinition`，不讀材質、不載配件；
  - 不載入武器。
- **共用的身體姿勢：** 姿勢組合抽成 `ComposeBody`。
  - `SamplePlayerBodyPose` 只算身體，給 Match 用；
  - `SamplePlayerPresentationPose` 也呼叫同一個 `ComposeBody`，再加上武器與配件。
  - 判定姿勢與呈現姿勢因此是同一段運算。
- 依賴方向不變：`player_content` → `match_domain`、`GYO::Model`、`GYO::Render`（核心），`app_support` → `player_content`。

## 判定

- **H3：通過。** `anim_bottlenecks` 在這台 Intel Mac 上的 H-pose（24 個姿勢、29568 個 float）：
  - 搬家前是 `dc7544253139d07e`；
  - 純搬家之後，與抽出 `ComposeBody` 之後，都還是 `dc7544253139d07e`；
  - 呈現測試全部通過。
- **H4：通過。** 「PvP Match body pose equals the presented body bit for bit」：
  - 範圍：A1 的 12 條 E1 路線，每 10 個 Tick 取一次；每一個取樣點比 8 種情況：原樣、pitch −1／0／+1、射擊、換彈、受擊、死亡。共 **5760** 個姿勢。
  - 每一個姿勢，Match 路徑（不載入武器、不登記貼圖解碼器）的身體全域變換，和 Client 路徑的都**逐位元相同**。
  - 兩條路徑的 `scale` 與 `anchor` 也逐位元相同。形狀換到世界座標要用到它們。這一條是突變 `d2-match-other-scale` 存活之後補上的，見「突變」。

## 突變

**17／17 killed**（`batch` 為 `v8-product-hitbox-data-pose`，3 個），加上搬了檔的既有突變 14 個，在未提交的工作樹上跑。

- 第一輪 16／17：`d2-match-other-scale`（Match 路徑的身體縮放不同）存活。H4 比的是模型空間的骨架，縮放要到換成世界座標時才用到。所以在 H4 加上 `scale` 與 `anchor` 也要逐位元相同（收緊），單獨重跑後 killed。
- 搬了檔的 14 個：`11-pitch-*`、`11-action-replaces`、`11-dead-aims`、`13-remote-*`、`v8cw-remote-crouch-clips-unused`、`a4-*` 6 個，全部 killed。

## 其他測試

- `test` preset 的 CTest 111／111 通過。

## 範圍的限界

- H4 只要求同一個平台上逐位元相同。跨平台的姿勢本來就不同（B3），照 `v8-product-hitbox-golden-policy`。
- Match 還沒有在啟動時載入姿勢的定義，也還不判定，這些在 `v8-product-hitbox-shot` 才做。這批只提供函式與證明。
- 突變清單裡原本指向 `PlayerPresentation.cpp` 的 14 個既有突變，find 的內容沒變，檔名改成 `PlayerPose.cpp`。

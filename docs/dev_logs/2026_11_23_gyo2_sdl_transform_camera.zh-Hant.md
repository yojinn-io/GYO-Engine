# 2026-11-23 `transform` 與 `camera` Module（SDL 線子批 X1，`v8-arch-gyo2-sdl-transform-camera`）

Owner：架構（GYO2 主會話開的任務卡）。分支 `claude/youthful-mcnulty-8c4678`（開向 `gyo2`）。作者 Opus 5.5。
宣告：[`transform-camera-declaration.zh-Hant.md`](../architecture/target/v2/transform-camera-declaration.zh-Hant.md)（修訂 1，使用者 2026-11-23「照建議，第 1 題選乙，同意副本」）。合併前核准頁：[`transform-camera-merge-approval.zh-Hant.md`](../architecture/target/v2/transform-camera-merge-approval.zh-Hant.md)。

## 做了什麼

- `engine/modules/transform`（新，`default=yes`）：`Transform`（逐位元 `==`、逐欄雜湊）、`GlobalTransform`（`Matrix4`、雜湊空）、`InitialValue<GlobalTransform>`、OnAdd<`Transform`> hook（補 `Restore` 的洞）、`SyncTransforms`（`Changed<Transform>`，預設 `Main`／`PostUpdate`，set `TransformPropagate`）、`TransformOptions`。
- `engine/modules/camera`（新，`default=yes`，依賴 `transform`）：`Camera`、`CameraTarget`、`Projection`（透視／正交）、`ScalingMode`（`WindowSize`、`Fixed`）、`Camera2d`（`Default2d()` 對調近遠：z 大的在前面）、`Camera3d`、`ComputedCamera`（逐位元不同才寫）、`CameraTargetSizes`（Resource，視窗大小的副本，雜湊空，準備函式沒有才放）、`UpdateCameras`（set `CameraUpdate`）、`Compute`、`WorldToViewport`／`ViewportToWorld`／`ViewportToWorld2d`（座標系與錯誤判法照 Bevy）、`CameraOptions`。
- `engine/core/math`：`MakeOrthographic`（左手、深度 0～1，near 可大於 far）、`Multiply(Matrix4, Vec4)`；math.md Conventions 兩行。
- 文件：`target/v2/transform.zh-Hant.md`（TF-1～4）、`camera.zh-Hant.md`（CM-1～5）、`docs/cross-component-rules.md` 的 CCR-12（判定不讀 `GlobalTransform`）、CCR-13（回滾用的相等逐位元，owner 回滾）、`target/v2/README.md`。
- 測試：`tests/common/transform`（11 案，含用回滾官方策略的 R-2 等價案）、`tests/common/camera`（17 案）、`tests/common/math` 加 4 案；`gyo_expect_links` 兩個。
- 突變：`transform_mutations.json` 11、`camera_mutations.json` 34（含 `math-ortho-depth-flipped`），共 45；照 `tests/common/mutation/README.md` 第 8 條確認：每個都寫了它唯一抓到的誤與 `guards`；45 等於「9 條 DI × 5」的上限。

## 驗證（本機：Intel Mac，macOS，`build/target/_build/test`，RelWithDebInfo）

- `ctest -L cpu`：198/198 通過兩次（補測試之前與之後；新的兩個 ctest `transform.module`、`camera.module` 在兩次裡都有）。
- 突變：
  - 第一輪 25 個：24 killed，`tf-initial-identity` 存活——T-1 生成在 `Update`，套用之後 `PostUpdate` 的 `SyncTransforms` 把值蓋對了，T-1 沒有隔離出初始值。把 T-1 的同步掛到 `First`（第一幀它在生成之前跑），重跑 killed。
  - 敵對式檢查之後加 20 個、修 3 個的 expect／guards，45 個重跑：42 killed、3 個 `wrong_failure`（`tf-set-membership-missing`、`tf-default-stage-last`、`cm-update-set-missing`）：set 沒有成員時 Kernel 在啟動就報 D-E4，Product 建不起來，失敗訊息裡沒有案例的標籤。讓這兩個案例在建置失敗時也印標籤，三個重跑 killed。最後 45/45。
  - 兩次都是測試的寫法（隔離、標籤），判定沒有改。
- L1 三平台：draft PR 手動 dispatch 後補記。

## 經過與判斷

1. 宣告修訂 0 → 敵對式檢查（Opus／high，同模型）B1 S9 N12 全部採用 → 修訂 1 → 使用者核准（第 1 題選 (乙)、同意副本）。
2. 實作中發現：Kernel 的 `InitialValue` 只准 Target 的擁有者宣告（RE4），所以 `Camera` 不帶 `Transform`（宣告已寫）；`Restore` 寫的格標 changed（`detail/Storage.hpp:135-143`），所以洞 (2) 由 Kernel 現行行為滿足，T-6 釘住。
3. 突變清單的敵對式檢查（Opus／high，同模型）推出 27 個存活候選。分類（`classify-gaps-before-asking`）：
   - 契約要補（全部補測試＋突變）：hook 放的值、公開 set 與預設 stage、`CameraOptions`、`CameraTargetSizes` 三個操作、`SameComputed`「不同要寫」、scale 0、`ClampAxis` 兩個分支、射線方向與旋轉、錯誤碼、HiDPI 換算、相機的初始值鏈、連結集的陽性對照兩個、WorldToViewport 的 x 偏移。
   - 逐欄細項（只補 assert、不加突變）：`Transform` 雜湊的 translation、`Camera`／`CameraTarget`／正交投影的雜湊。
   - 不補：Prepare 不蓋帶過來的 `CameraTargetSizes`（要 `CarryOver` 的測試環境；壞了的後果是帶過來的大小被換成空的，下一次膠水寫入就回來）；hook 的 `Without<GlobalTransform>`（拿掉只會在生成時重插相同的值）。
4. 設計審查（Opus／high，同模型）：要使用者定的 3 件寫在合併前核准頁；卡片改了錯誤訊息留空、`SameComputed` 移進 `CameraModule.cpp`、`Compute` 的用途寫進 camera 頁、CCR-13、camera 頁「不支援被 `Restore` 復活」、RC-TF2 改寫。
5. 宣告第 5 節偏離表沒列的三處（審查指出，本紀錄補）：`ViewportToWorld` 的方向不正規化（Bevy 用 `Dir3`）；`Viewport` 用有號 `Vec2i` 與 `minDepth`／`maxDepth`（Bevy 是 `UVec2` 與 `depth: Range`）；錯誤型別是 `Base::Error<Code>`（Bevy 是純 enum）。宣告第 4.2 節「照 Bevy `camera.rs:555-560` 的判法」應讀成「語意照 Bevy」（Bevy 反向 Z，字面相反）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-sdl-x1/`（本機 Intel Mac，git 不追蹤）：`mutations-run1.txt`（第一輪＋T-1 修正後）、`mutations-run2.txt`（45 個重跑＋標籤修正後）、`build.txt`、`ctest-cpu.txt`、`ctest-cpu-2.txt`。敵對式檢查與設計審查的報告未另存檔（重點在宣告第 14 節、合併前核准頁、本紀錄）。

## 後續（回報主會話）

- R2b：2D 排序照 (乙)（z 大的在前面）；讀 `GlobalTransform`、`ComputedCamera`（`Changed<ComputedCamera>` 是真的變了）。
- W：把視窗大小寫進 `CameraTargetSizes` 的膠水放哪；新 World 第一輪要寫一次。
- 內插的「上一步的姿態」：本線沒有點名的 Feature，X1 不做。
- 合併前核准頁第 3 節第 1 題若選 (b)，回滾那一側另排一批。

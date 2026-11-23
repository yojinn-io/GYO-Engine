# 相機（`engine/modules/camera`）

更新：2026-11-23（新頁，`v8-arch-gyo2-sdl-transform-camera`）。由來：[事前宣告](transform-camera-declaration.zh-Hant.md)（SDL 線子批 X1，2026-11-23 動工前核准；2D 的預設方向與目標大小的副本是使用者的決定，第 13 節、第 7 節第 3 項）第 4.2、8 節。**Design Invariants 的最終文字：使用者 2026-11-23 合併前核准**（[合併前核准頁](transform-camera-merge-approval.zh-Hant.md)）。

Owner：架構。程式：`engine/modules/camera/`（`GYO::Engine::Camera`，target `GYO::Camera`，登記單位 `CameraModule`）。行為測試：`tests/common/camera/`。

## 一個例子

Object_Connect 的畫面固定 1280×720。作者生成一個相機：bundle 帶 `Camera2d` 與 `Transform`（相機的視點；`Camera` 不會自己帶 `Transform`）。Kernel 在同一個同步點補上 `Camera`、`CameraTarget{0}`、正交的 `Projection`（`Default2d()`）與 `ComputedCamera`。作者把投影的縮放模式改成 `ScalingFixed{1280, 720}`。有人把視窗大小寫進 `CameraTargetSizes`（之後由子批 W 定的膠水寫；現在是作者或測試）之後，`UpdateCameras` 算出投影。玩家把視窗拉到 2560×1440，滑鼠在右下角：`ViewportToWorld2d(computed, 相機的 GlobalTransform, {2560, 1440})` 回 (640, −360)。節點 z=2、血管 z=1、背景 z=0：z 大的在前面（使用者選 (乙)）。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::Transform ◄── GYO::Camera
```

- 登記檔一列：`library_only=no`、`default=yes`、`depends=transform`。不依賴 `window`、`input`、render。
- 標頭：`Camera.hpp`（元件、投影、`ComputedCamera`、`CameraTargetSizes`、`Compute`、三個換算函式、雜湊）、`CameraModule.hpp`（登記單位、`CameraOptions`、公開的 set 名 `kUpdateSet`）。

## 2. 用法與後果

- **元件**（照 Bevy `bevy_camera`）：`Camera`（`order`、`active`、`viewport`）、`CameraTarget{id}`（中性的 id；0＝主要的目標，有視窗時是主視窗；其他 id 由 render 與 GPU 後端對）、`Projection`（`std::variant<PerspectiveProjection, OrthographicProjection>`）、標記 `Camera2d`、`Camera3d`。一個實體只放其中一個標記。
- **初始值**：`Camera2d` → `Camera`、`Projection{Default2d()}`；`Camera3d` → `Camera`、`Projection{PerspectiveProjection{}}`；`Camera` → `CameraTarget{0}`、`ComputedCamera{}`。`Camera` 不給 `Projection`（同 Bevy），沒有 `Projection` 的相機 `UpdateCameras` 不處理。
- **2D 的前後**：`Default2d()` 把 near、far 對調（nearClip 1000、farClip −1000），z 大的離相機近、畫在前面，同 Bevy 2D（GYO 的座標是左手、看 +Z、深度 0～1 不反向，math.md）。同一個相機混畫 3D 模型時，深度方向和 3D 相機相反。
- **縮放模式**：`ScalingWindowSize`（1 世界單位＝1 邏輯像素，再乘 `scale`）、`ScalingFixed{width, height}`（固定看到這麼大、會拉伸）。區域照 Bevy：以 `viewportOrigin` 定原點、乘 `scale`。其他四種 Bevy 的模式有點名的 Feature 時再加（加法、不破壞）。
- **`ComputedCamera`**：`valid`、目標的物理大小、縮放比、實際用的視區（夾進目標，負的位置當 0）、`clipFromView`。只由 `UpdateCameras` 寫，**值（逐位元）不同才寫**，所以 `Changed<ComputedCamera>` 代表真的變了。目標大小未知或視區大小 0 時 `valid == false`。
- **`CameraTargetSizes`**（Resource）：目標 id → 物理大小＋縮放比。是視窗（與之後 render 的貼圖目標）大小的**副本**，只拿來算投影（使用者同意，AGENTS §3）。每個 World 一開始是空的（準備函式「沒有才放」）；新建或替換的 World 要由膠水在第一輪寫一次（交子批 W）。雜湊不寫內容（視窗大小每台機器不同）。
- **`UpdateCameras`**：預設掛 `Main`／`PostUpdate`，在 set `CameraUpdate` 裡；不讀 `GlobalTransform`，和 `TransformPropagate` 沒有先後。每幀比較（Bevy 只在視窗事件時重算）。縮放比改變時不改作者給的視區（Bevy 會照比例改）。
- **換算**（照 Bevy）：`WorldToViewport`、`ViewportToWorld`（從近平面出發、朝遠平面的射線，方向不正規化）、`ViewportToWorld2d`。位置是目標的邏輯像素、左上原點、+Y 向下、**含視區的偏移**（黑邊時把滑鼠的視窗座標直接傳進來）。失敗是 `ViewportConversionFailure`（E2）：`NoViewportSize`、`InvalidData`、`PastNearPlane`（NDC 深度 < 0）、`PastFarPlane`（> 1；透視相機**後面**的點也是這個，同 Bevy）。
- **骨架關掉的 World**：用 `CameraOptions{schedule, stage}` 指名。
- **`Compute`** 是 `UpdateCameras` 用來算 `ComputedCamera` 的純函式，公開給不在 World 裡的相機（工具的預覽、測試）用；它只回傳值、不寫任何東西，所以不違反 CM-3。
- **失敗的訊息是空的**：錯誤碼就是原因；每幀換算（例：身後玩家的名牌）失敗時不配置記憶體。
- **相機不支援被 `Restore` 復活**：`Restore` 不補初始值（Kernel DI-KS 12），`camera` 沒有像 `transform` 那樣的 hook。被放回的相機實體若當時的格裡沒有 `CameraTarget`、`ComputedCamera`，`UpdateCameras` 不處理它、也不報錯。相機要回滾時，把這兩個元件也登記成可回滾；第二個 Module 也需要「放回時補推導元件」的 hook 時，照 transform 的 RC-TF2 停下來交使用者。

## Design Invariants

守衛欄：行為測試（ctest `camera.module`）與突變（`docs/architecture/target/v2/scripts/camera_mutations.json`，`cm-` 與 `math-ortho-depth-flipped`）。連結集照 `entity_ids` 的先例立。

<a id="cm-1"></a>
**CM-1 職責。** 相機的設定、目標、投影、2D／3D 標記、推導值、目標大小的副本、更新的 System、座標換算。不做：畫面、可見性、清除色、視窗。守衛：C-1、`CameraOptions` 案、「a System ordered after CameraUpdate…」；突變 `cm-camera-no-target`、`cm-options-ignored`、`cm-update-set-missing`；審查。

<a id="cm-2"></a>
**CM-2 依賴。** 只依賴 `transform` 與 base、math、kernel、orchestration；不依賴 `window`、`input`、render。守衛：`gyo_expect_links(gyo_camera GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Transform)`（`tests/common/camera/CMakeLists.txt`）；突變 `cm-links-more`（陽性對照）。

<a id="cm-3"></a>
**CM-3 `ComputedCamera` 是推導值。** 只由初始值與 `UpdateCameras` 寫；值（逐位元）相同不寫；目標大小未知或視區大小 0 時 `valid == false`；每個相機讀自己目標的大小；雜湊不寫內容。守衛：C-2、C-3、C-4、摘要案；突變 `cm-valid-without-size`、`cm-writes-when-equal`、`cm-samecomputed-valid-only`、`cm-target-always-zero`、`cm-viewport-ignored`、`cm-viewport-not-clamped`、`cm-clamp-outside`、`cm-clamp-zero-target`、`cm-zero-scale-valid`、`cm-hash-computed-nonempty`。

<a id="cm-4"></a>
**CM-4 慣例。** 投影照 math.md（左手、深度 0～1）；`Default2d()` 對調近遠（z 大的在前面，使用者的決定）；正交的區域照 Bevy 的 `ScalingMode`（邏輯大小、原點、`scale`）；換算的位置是目標的邏輯像素、含視區偏移、左上原點、+Y 向下，深度的判法照 Bevy。守衛：C-1、C-2、C-5、math 的正交投影案；突變 `cm-camera2d-perspective`、`cm-default2d-not-swapped`、`cm-perspective-aspect-inverted`、`cm-windowsize-physical`、`cm-origin-ignored`、`cm-scale-ignored`、`cm-ndc-reverse-z`、`cm-ndc-y-not-flipped`、`cm-viewport-offset-ignored`、`cm-past-planes-swapped`、`cm-ray-direction-reversed`、`cm-ray-not-rotated`、`cm-convert-physical`、`cm-world-to-viewport-offset`、`cm-2d-valid-unchecked`、`math-ortho-depth-flipped`。

<a id="cm-5"></a>
**CM-5 目標大小是副本。** `CameraTargetSizes` 只用來算投影；這個 Module 不拿它做別的判斷、不寫回擁有者；雜湊不寫內容。守衛：摘要案、`CameraTargetSizes` 案；突變 `cm-hash-target-sizes-nonempty`、`cm-sizes-set-no-update`、`cm-sizes-remove-noop`、`cm-sizes-find-any`；其餘審查。

### Replacement Criteria

符合任一條時，停下來交使用者「修還是重建」。

- **RC-CM1**：要多視窗，或膠水必須知道 `camera` 以外的視窗細節才能填（副本開始要判斷）→ 重比宣告第 4.2 節的 (a)（`camera` 依賴 `window`）。
- **RC-CM2**：副本和擁有者不同步造成畫面錯（例：改大小的那一幀投影錯了、作者要補）。

# 相機（`engine/modules/camera`）

更新：2026-11-28（A4：`Hdr`、`Tonemapping`、`Exposure` 也是 render 的相機元件，見 render 頁第 8.2 節；這個 Module 不認得它們，CM-2 不變）；2026-11-27（A2：相機的更新模式 `TargetUpdate` 是 render 的元件，見 render 頁第 8.2 節；`Camera::active = false` 的相機不畫，它的 render target 在程式畫過時被沿用，從沒畫過時取樣它是 RP-3 的錯（作者照 Bevy 的 `is_active` 停住監視器時，target 要先畫過一次；畫的那一幀 GPU 沒拿到時，後端跳過取樣、計數，第 14.14 節）；主目標的大小副本也是渲染 World 決定 target 大小的來源，`camera_render` 在 extract 讀 `CameraTargetSizes[kMainTarget]`）；2026-11-27（渲染管線線 A1 `v8-arch-gyo2-render-a1`，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 13 節：`kMainTarget` 與其他 target id 的擁有者、`CameraView` 與 `UpdateCameraViews`、寫大小的唯一出口 `WriteTargetSize`；CM-1、CM-5、新 CM-7。CM-3 的文字不變）；2026-11-24（R2b 的 2D 段 `v8-arch-gyo2-sdl-r2b-2d`：`Letterbox`、CM-6；新頁是 2026-11-23 的 `v8-arch-gyo2-sdl-transform-camera`）。由來：[事前宣告](transform-camera-declaration.zh-Hant.md)（SDL 線子批 X1，2026-11-23 動工前核准；2D 的預設方向與目標大小的副本是使用者的決定，第 13 節、第 7 節第 3 項）第 4.2、8 節。**Design Invariants 的最終文字：使用者 2026-11-23 合併前核准**（[合併前核准頁](transform-camera-merge-approval.zh-Hant.md)）。

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

- **元件**（照 Bevy `bevy_camera`）：`Camera`（`order`、`active`、`viewport`）、`Letterbox`（可選）、`CameraTarget{id}`（中性的 id；`kMainTarget`（0）＝主要的目標，有視窗時是主視窗；其他 id 全部歸 render 的 `RenderTargets`——它在來源 World 配 id（[render 頁](render.zh-Hant.md)第 3 節），別人不配也不寫它的大小；這個 Module 只認得 id 的大小）、`Projection`（`std::variant<PerspectiveProjection, OrthographicProjection>`）、標記 `Camera2d`、`Camera3d`。一個實體只放其中一個標記。
- **初始值**：`Camera2d` → `Camera`、`Projection{Default2d()}`；`Camera3d` → `Camera`、`Projection{PerspectiveProjection{}}`；`Camera` → `CameraTarget{0}`、`ComputedCamera{}`、`CameraView{}`（A1）。宿主自己 `Insert` 組相機時（不經 spawn，初始值不補）要一起放 `ComputedCamera` 與 `CameraView`。`Camera` 不給 `Projection`（同 Bevy），沒有 `Projection` 的相機 `UpdateCameras` 不處理。
- **2D 的前後**：`Default2d()` 把 near、far 對調（nearClip 1000、farClip −1000），z 大的離相機近、畫在前面，同 Bevy 2D（GYO 的座標是左手、看 +Z、深度 0～1 不反向，math.md）。同一個相機混畫 3D 模型時，深度方向和 3D 相機相反。
- **`Letterbox`**（R2b 的 2D 段加，可選元件）：相機照這個比例（寬：高）出畫面，不管目標是什麼形狀——`ComputedCamera::viewport` 是相機的區域（作者的 `Camera::viewport`，沒給是整個目標）裡置中、照這個比例的最大矩形；放不下的那一邊照原樣、另一邊四捨五入到整像素，剩下的像素兩邊平分（奇數的那一個給右、下）。目標其餘的部分留給清除色（黑邊；render 頁第 7 節）。配 `ScalingFixed` 同一個大小，就是固定解析度的遊戲（Object_Connect 的 1280×720）不拉伸地縮放；只用 `ScalingFixed` 是拉伸。**不改作者的 `Camera::viewport`**：它進雜湊，視窗大小每台機器不同，寫進去會讓兩台機器的摘要不一樣——所以算在推導值裡。寬或高不是正的有限數時相機 `valid == false`。Bevy 沒有內建（作者自己改 `Camera.viewport`，或用 `ScalingMode::AutoMin` 露出多的區域）；Godot 的 stretch aspect `keep` 是同一個規則〔知〕；master 的 UI 有 `FitDesignCanvas`（等比、置中）。
- **縮放模式**：`ScalingWindowSize`（1 世界單位＝1 邏輯像素，再乘 `scale`）、`ScalingFixed{width, height}`（固定看到這麼大、會拉伸）。區域照 Bevy：以 `viewportOrigin` 定原點、乘 `scale`。其他四種 Bevy 的模式有點名的 Feature 時再加（加法、不破壞）。
- **`ComputedCamera`**：`valid`、目標的物理大小、縮放比、實際用的視區（夾進目標，負的位置當 0）、`clipFromView`。只由 `UpdateCameras` 寫，**值（逐位元）不同才寫**，所以 `Changed<ComputedCamera>` 代表真的變了。目標大小未知或視區大小 0 時 `valid == false`。
- **`CameraTargetSizes`**（Resource）：目標 id → 物理大小＋縮放比。是視窗與 render target 大小的**副本**，只拿來算投影（使用者同意，AGENTS §3）。寫的是擁有者那一側的膠水：主目標是 `window_camera`，render target 是 `world_render`（A1）；兩邊都經這個 Module 的 `WriteTargetSize`／`RemoveTargetSize`——逐位元相同不寫、沒有就不拿，所以 `CameraTargetSizes` 只在大小真的變了時才標成變了（設計審查 S-3：原本兩邊各抄一份比較）。每個 World 一開始是空的（準備函式「沒有才放」）；新建或替換的 World 要由膠水在第一輪寫一次（交子批 W）。雜湊不寫內容（視窗大小每台機器不同）。
- **`UpdateCameras`**：預設掛 `Main`／`PostUpdate`，在 set `CameraUpdate` 裡；不讀 `GlobalTransform`，和 `TransformPropagate` 沒有先後。每幀比較（Bevy 只在視窗事件時重算）。縮放比改變時不改作者給的視區（Bevy 會照比例改）。
- **換算**（照 Bevy）：`WorldToViewport`、`ViewportToWorld`（從近平面出發、朝遠平面的射線，方向不正規化）、`ViewportToWorld2d`。位置是目標的邏輯像素、左上原點、+Y 向下、**含視區的偏移**（黑邊時把滑鼠的視窗座標直接傳進來）。失敗是 `ViewportConversionFailure`（E2）：`NoViewportSize`、`InvalidData`、`PastNearPlane`（NDC 深度 < 0）、`PastFarPlane`（> 1；透視相機**後面**的點也是這個，同 Bevy）。
- **骨架關掉的 World**：用 `CameraOptions{schedule, stage}` 指名。
- **`CameraView`**（A1，選用相機的推導值）：`valid`、`viewFromWorld`、`clipFromWorld`（＝`clipFromView × viewFromWorld`）。`UpdateCameraViews`（在 set `CameraUpdate` 裡、`UpdateCameras` 與 `TransformPropagate` 之後）從 `ComputedCamera` 與這一幀的 `GlobalTransform` 寫，值（逐位元）不同才寫。`ComputedCamera` 無效或視點沒有反矩陣時 `valid == false`；沒有 `GlobalTransform` 的相機留著無效的 `CameraView`（沒有「當作在原點」的路）。**世界到 clip 在這裡算一次**：visibility 的視錐、2D 與 3D 管線的 pass 取它，3D 的排序距離取 `viewFromWorld`。照 Bevy：`Camera::computed` 只有投影，視點相關的 `Frustum` 由 `update_frusta` 在 propagation 之後另寫（`bevy_camera/src/visibility/mod.rs:503-507,627`）。換算函式（`WorldToViewport` 等）照 Bevy 收傳進來的 `GlobalTransform` 自己算（`bevy_camera/src/camera.rs:740-747`），不在「一個值」的範圍裡。所以 `CameraOptions` 要指 transform 的 schedule 與 stage（同 visibility；跨 stage 的先後是骨架的順序）。純函式 `ViewOf(computed, view)` 給不在 World 裡的相機（測試、工具）用。
- **`Compute`** 是 `UpdateCameras` 用來算 `ComputedCamera` 的純函式，公開給不在 World 裡的相機（工具的預覽、測試）用；它只回傳值、不寫任何東西，所以不違反 CM-3。
- **失敗的訊息是空的**：錯誤碼就是原因；每幀換算（例：身後玩家的名牌）失敗時不配置記憶體。
- **相機不支援被 `Restore` 復活**：`Restore` 不補初始值（Kernel DI-KS 12），`camera` 沒有像 `transform` 那樣的 hook。被放回的相機實體若當時的格裡沒有 `CameraTarget`、`ComputedCamera`（A1 起還有 `CameraView`），`UpdateCameras` 不處理它、也不報錯。相機要回滾時，把這兩個元件也登記成可回滾；第二個 Module 也需要「放回時補推導元件」的 hook 時，照 transform 的 RC-TF2 停下來交使用者。

## Design Invariants

守衛欄：行為測試（ctest `camera.module`）與突變（`docs/architecture/target/v2/scripts/camera_mutations.json`，`cm-` 與 `math-ortho-depth-flipped`；A1 的在 `tests/common/render/target_mutations.json`）。連結集照 `entity_ids` 的先例立。

<a id="cm-1"></a>
**CM-1 職責。** 相機的設定、目標、投影、2D／3D 標記、推導值（含 `CameraView`，A1）、目標大小的副本與寫它的唯一出口、更新的 System、座標換算。不做：畫面、可見性、清除色、視窗。守衛：C-1、`CameraOptions` 案、「a System ordered after CameraUpdate…」；突變 `cm-camera-no-target`、`cm-options-ignored`、`cm-update-set-missing`；審查。

<a id="cm-2"></a>
**CM-2 依賴。** 只依賴 `transform` 與 base、math、kernel、orchestration；不依賴 `window`、`input`、render。守衛：`gyo_expect_links(gyo_camera GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Transform)`（`tests/common/camera/CMakeLists.txt`）；突變 `cm-links-more`（陽性對照）。

<a id="cm-3"></a>
**CM-3 `ComputedCamera` 是推導值。** 只由初始值與 `UpdateCameras` 寫；值（逐位元）相同不寫；目標大小未知或視區大小 0 時 `valid == false`；每個相機讀自己目標的大小；雜湊不寫內容。守衛：C-2、C-3、C-4、摘要案；突變 `cm-valid-without-size`、`cm-writes-when-equal`、`cm-samecomputed-valid-only`、`cm-target-always-zero`、`cm-viewport-ignored`、`cm-viewport-not-clamped`、`cm-clamp-outside`、`cm-clamp-zero-target`、`cm-zero-scale-valid`、`cm-hash-computed-nonempty`。

<a id="cm-4"></a>
**CM-4 慣例。** 投影照 math.md（左手、深度 0～1）；`Default2d()` 對調近遠（z 大的在前面，使用者的決定）；正交的區域照 Bevy 的 `ScalingMode`（邏輯大小、原點、`scale`）；換算的位置是目標的邏輯像素、含視區偏移、左上原點、+Y 向下，深度的判法照 Bevy。守衛：C-1、C-2、C-5、math 的正交投影案；突變 `cm-camera2d-perspective`、`cm-default2d-not-swapped`、`cm-perspective-aspect-inverted`、`cm-windowsize-physical`、`cm-origin-ignored`、`cm-scale-ignored`、`cm-ndc-reverse-z`、`cm-ndc-y-not-flipped`、`cm-viewport-offset-ignored`、`cm-past-planes-swapped`、`cm-ray-direction-reversed`、`cm-ray-not-rotated`、`cm-convert-physical`、`cm-world-to-viewport-offset`、`cm-2d-valid-unchecked`、`math-ortho-depth-flipped`。

<a id="cm-5"></a>
**CM-5 目標大小是副本。** `CameraTargetSizes` 只用來算投影；這個 Module 不拿它做別的判斷、不寫回擁有者；雜湊不寫內容。主目標的大小由 `window_camera`、render target 的由 `world_render` 寫（A1），都經 `WriteTargetSize`／`RemoveTargetSize`（不同才寫、有才拿）；id 0 以外的 id 只有 `RenderTargets` 配。守衛：摘要案、`CameraTargetSizes` 案、「WriteTargetSize writes only a size that differs…」；突變 `cm-hash-target-sizes-nonempty`、`cm-sizes-set-no-update`、`cm-sizes-remove-noop`、`cm-sizes-find-any`、`cm-size-written-when-equal`、`cm-size-removed-when-absent`（`tests/common/render/target_mutations.json`）；其餘審查。

<a id="cm-6"></a>
**CM-6 `Letterbox` 只改推導的視區。**（R2b）有 `Letterbox` 時 `ComputedCamera::viewport` 是相機區域裡置中、照比例的最大矩形；作者的 `Camera::viewport` 不變；`Letterbox` 本身是作者的設定，雜湊寫它的值。守衛：「Letterbox cuts the viewport…」「UpdateCameras uses a camera's Letterbox」、摘要案；突變 `cm-letterbox-ignored`、`cm-letterbox-not-centred`、`cm-letterbox-axis-swapped`、`cm-letterbox-whole-target`、`cm-letterbox-invalid-accepted`、`cm-hash-letterbox-empty`。

<a id="cm-7"></a>
**CM-7 `CameraView` 是世界到 clip 的唯一一個值。**（A1）只由初始值與 `UpdateCameraViews` 寫；`UpdateCameraViews` 在 `UpdateCameras` 與 `TransformPropagate` 之後、值（逐位元）相同不寫；`clipFromWorld`＝`clipFromView × inverse(這一幀的視點)`；`ComputedCamera` 無效或視點沒有反矩陣時 `valid == false`；visibility 與 render 的管線取它、不另算；雜湊不寫內容。守衛：「ViewOf makes viewFromWorld and clipFromWorld…」「UpdateCameraViews takes this frame's GlobalTransform…」、render 的 `[vs-clip-from-world]`、`[p2-map-view]`、`[p3-clip-from-world]`；突變 `cm-clip-from-world-view-ignored`、`cm-view-singular-valid`、`cm-view-ignores-computed`、`cm-view-not-written`、`cm-view-written-every-frame`、`vs-clip-from-world-own`（`tests/common/render/target_mutations.json`）、`p2-view-ignored`、`p3-view-ignored`（`render_mutations.json`）。

### Replacement Criteria

符合任一條時，停下來交使用者「修還是重建」。

- **RC-CM1**：要多視窗，或膠水必須知道 `camera` 以外的視窗細節才能填（副本開始要判斷）→ 重比宣告第 4.2 節的 (a)（`camera` 依賴 `window`）。
- **RC-CM2**：副本和擁有者不同步造成畫面錯（例：改大小的那一幀投影錯了、作者要補）。

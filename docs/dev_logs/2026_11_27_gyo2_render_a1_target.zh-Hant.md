# 2026-11-27 渲染管線線 A1：render target（`v8-arch-gyo2-render-a1`）

Owner：渲染管線線のカード。分支 `claude/gyo2-render-a`（從 `origin/gyo2` `172b7546`）。作者 Opus 5.5／high（子 Agent）。宣告：[渲染管線宣告](../architecture/target/v2/render-pipeline-declaration.zh-Hant.md)第 13 節（A1 小宣告）；render 頁第 3 節、RD-3、RD-11；相機頁 CM-1、CM-3、CM-5；SDL 頁第 7 節。

## 前提

- 使用者：總宣告（#369）合併後說「A 段開始」（HANDOFF `v8-arch-gyo2-render-a-start`），主會話交 A1；小宣告寫完直接實作、不等核准（主會話的指示）。
- A1 的範圍（總宣告第 5 節）：畫到貼圖、貼圖格式與色彩空間、clip-from-world 收一處、`RenderCommands` 開頭註解。上段（公開介面、camera 的推導值條文）。
- 〔Engine〕target 的 id、GPU 貼圖、畫進去再取樣、跟視窗大小走、target 的大小進相機的推導值、世界到 clip 只算一次；〔作者〕哪張圖是彩色（解碼）、哪張是資料，用 target 做什麼。

## 做了什麼

（這一節是第一個 commit `ac2f7a57` 的內容。審查之後改了的地方在「設計審查與修正」：clip-from-world 移到 `CameraView`、pass 的目標改成 `std::optional<TextureId>`、`FollowTarget`、畫面跨幀保留、表的身分、後端的一次解析。）

1. **render target**（`RenderTextures.hpp`、`Render.cpp`）：`CreateTarget(RenderTargetDesc{size, format, filter})` 回 `TextureId`，和圖片共用計數器、不重用（RD-3）；pass 以 `target = id.value` 畫進去、draw 以 `texture = id` 取樣（同 Bevy 的 `Image::new_target_texture`）。`size` 沒給＝跟主目標走。新要求 `TextureTargetCreate`；`FindTarget`。
2. **格式**：`TextureFormat{Rgba8Unorm, Rgba8UnormSrgb}`，`TextureDesc::format` 預設 `Rgba8Unorm`（既有的呼叫者全部照舊）。`Rgba16Float`（A4）、深度（A2）、壓縮（F1）沒加，是加 enum 值與後端一列。
3. **SDL_GPU 能不能對一張貼圖開兩種格式的 view**（總宣告第 4.4 節的未確認點）：**不能**。`SDL_GPUTextureCreateInfo` 只有一個 `format`（`SDL_gpu.h:1769-1781`），沒有建 view 的 API，綁定 `SDL_GPUTextureSamplerBinding{texture, sampler}`（`:2142`）不帶格式；Vulkan 後端不開 `MUTABLE_FORMAT`（`SDL_gpu_vulkan.c:5607-5640`）。Bevy（wgpu `view_formats`，`bevy_image/src/image.rs:30-50`）、Godot（`texture_create_shared`＋`shareable_formats`→`MUTABLE_FORMAT`，`rendering_device_driver_vulkan.cpp:2316-2317`）都做得到。判：要兩種讀法時建兩份（B4 在 `RenderImages` 以圖＋格式為鍵）。
4. **來源 World 的 target**（新 `RenderTargets.hpp`、`RenderTargets.cpp`）：
   - `RenderTargets`（`world_render` 登記）：`Create` 回 `CameraTarget`（從 1 起、不重用）。
   - `world_render` 新的 System `WriteRenderTargetSizes`（`PostUpdate`、`CameraUpdate` 之前）：大小抄進 `CameraTargetSizes`（固定的 scale 1；跟主目標的抄 target 0；值不同才寫；放掉的拿掉）。`world_render` 的登記列加 `camera` 依賴、新 `WorldRenderOptions`。**這改了 render 頁「`world_render` 不跑 System」那一條**（大地圖宣告第 13.1 節當 Architecture Delta 沒改的那一條），比過的選項與理由在宣告第 13.3 節第 2 點。
   - `camera_render` 新的 `ExtractRenderTargets`（`ExtractCameras` 之前）：`SyncTargetTextures` 維持 `TargetTextures`（來源 id → 渲染 World 的 `TextureId`）；`ExtractCameras` 把相機的目標換成對應的 id，來源沒有的 id 的相機不抽。
5. **後端**（`GPUDevice.cpp`）：每個 pass 畫在自己的目標上；每個目標自己的「這一幀第一個 pass」與自己的深度貼圖；跟主目標的 target 在拿到主目標大小後建、大小變了重建；格式對到 SDL 的兩種；建之前 `SDL_GPUTextureSupportsFormat` 查格式與用途。`RecordPasses` 拆成 `RecordPasses`（目標的選擇）＋`RecordPass2D`＋`RecordPass3D`。
6. **clip-from-world 收一處**：`ComputedCamera::clipFromWorld`（`clipFromView × inverse(GlobalTransform)`），`UpdateCameras` 排在 `TransformPropagate` 之後寫；沒有視點＝原點；視點沒有反矩陣時 `valid == false`；`Compute` 多一個可選的 `view` 參數。visibility、`Queue2d`、`Queue3d` 改讀它。
7. **註解**：`RenderCommands.hpp` 開頭（「R2a 只有一種 pass、3D 在 R2b」）改成現在的樣子；`Camera.hpp` 的 `CameraTarget` 註解（「R2a、R2b 對其他 id」）同。
8. **文件**：宣告第 13 節；render 頁（第 2、3、7.2、7.4、8.3、8.6 節、RD-3、新 RD-11、範囲の限界）；相機頁（第 2 節、CM-1、CM-3、CM-5）；SDL 頁第 7 節。

## 測試

- 新：
  - `render.data` 五案：target 的 id（共用計數器、不重用、`Find`／`FindTarget` 分開、要求的順序）、格式、錯的 target 要求（Programmer Error）、來源的 `RenderTargets` 與 `TargetSizeOf`、`SyncTargetTextures`／`PassTargetOf`。
  - `render.extract` 一案：來源的固定與跟主目標的 target、相機在同一幀算得出來並畫在渲染 World 的 target 上、沒有對應的相機不畫、視窗變大跟著變、放掉後大小與渲染 World 的 target 都放掉。來源先建一個再放掉，讓來源的 id（2、3）和渲染 World 的（1、2）不同（第一次跑時兩邊碰巧相等，`re-target-not-mapped` 存活；改了之後 killed）。
  - `render.visibility` 一案：相機站在 z 50 時視錐跟著視點。
  - `camera.module` 兩案：`Compute` 的 `clipFromWorld`、原點、沒有反矩陣；`UpdateCameras` 用這一幀 propagation 後的視點、動了才寫。
  - 新 ctest `sdl_gpu.targets`（標籤 `gpu`）四案：8×8 target 畫 2D 再在主目標取樣（逐像素）、同一個 target 畫 3D（它自己的深度）、`Rgba8UnormSrgb` 的 188／128 取樣成 128／55（±1）而 `Rgba8Unorm` 照存、跟主目標的 target 在 8×8 與 16×16 都和主目標一樣大、target 第一個沒清除色的 pass 從透明黑開始（前一幀留的紅不見）。
- 改了的既有測試（判斷不變）：`Pipeline2dTests.cpp` 的 `Camera2d`、`Pipeline3dTests.cpp` 的 `Camera3d` 把視點也交給 `Compute`（管線改讀 `computed.clipFromWorld`）。算出來的矩陣和以前在管線裡算的是同一個運算、逐位元相同（`[p3-clip-from-world]` 逐位元比對照樣通過）；斷言一行都沒改。

## 驗證（本機 macOS Intel、Metal，preset `test`）

- 建置：依賴借另一個工作樹（`gyo2-big-map-s4c`）抓好的 `_deps`（`FETCHCONTENT_SOURCE_DIR_*`），shader 工具借同一個工作樹建好的 `gyo_sdl_gpu_shader_tool`（`GYO_SHADER_TOOL_EXECUTABLE`；本批沒改 shader 與 shader 工具）。全量建置成功（無警告；`GPUDevice.cpp` 的 `-Wmissing-field-initializers` 已修）。
- `ctest -L 'cpu|shader'`：254 個裡 253 通過。失敗的 `cmake.PluginSelection` 是借用依賴造成的（巢狀配置的 `FETCHCONTENT_FULLY_DISCONNECTED` 要求這個建置自己有 `yyjson-src`），同 S6 的紀錄，和本批無關。
- GPU（Metal，8×8～16×16 讀回，是測試、不是量測）：`sdl_gpu.pass2d`、`sdl_gpu.pass3d`、`sdl_gpu.frames`、`sdl_gpu.targets`、`sdl_gpu.budget`（既有的只印不判的那一案，跟著 `gpu` 標籤一起跑；Metal 5.8／6.4 ms）全部通過。
- 突變（D184）：見下一節。

## 突變

- 新 `tests/common/render/target_mutations.json`：27 個，全部 killed（`gt-` 的 5 個在 Metal 上）。
  - 寫了又拿掉的一個：`cm-clip-from-world-before-propagate`（拿掉 `UpdateCameras` 的 `after TransformPropagate`）存活——Kernel 對沒宣告先後的衝突照登記順序排，`camera` 依賴 `transform`，所以 propagation 本來就先跑。順序照 visibility 的寫法明寫，不算突變（理由在清單的 `description`）。
  - 第一次跑時 `rd-target-no-ask`、`rt-size-follows-ignored` 是 wrong_failure（先停在沒有標籤的 `REQUIRE`）：把那兩個 `REQUIRE` 改成帶標籤的 `REQUIRE_MESSAGE`，重跑 killed。
  - 數量：改的條文 4 條（CM-3、CM-5、RD-3、新 RD-11）×5＝20 < 27。超過的理由：A1 是三件事（target、格式、clip-from-world），每個獨立可錯的判斷一個（README 基準 3）；合併前審查時說明。
- 既有突變：
  - find 失效的 16 個：15 個改寫到新程式、守同一個性質，`g3-unmapped-drawn` 併入 `gp-unmapped-drawn`（2D 與 3D 的 pass 現在走同一條路到目標，同一個改動讓兩邊都畫到主目標；cases 與 expect 照 README 的合わせ方）。
  - find 落在改過的程式裡、或被改過的測試（`Camera2d`／`Camera3d`）殺的：`render_mutations.json` 21 個（改寫的與 find 在改過的程式裡的）＋`p2-`／`p3-`／`cp-`／`mh-` 其餘 54 個、`instancing_mutations.json` 13 個（`in-g3-instance-base-not-advanced`＋CPU 的 12 個）、`camera_mutations.json` 4 個、`visibility_mutations.json` 1 個。
  - 結果：93 個全部 killed（`gp-`／`g3-`／`in-g3-` 在 Metal 上）。加上新的 27 個，本批共跑 120 個、120 killed。

## 設計審查與修正（同日）

兩份審查，報告在主 checkout 的 `build/evidence/v8-arch-gyo2-render-a1/`（不在 git）：
- 每批的設計審查（Opus／high）：Blocker 0、Should 3、Nit 7、Note 6。
- 純淨 Code Review（Fable／high，只看程式）：高 3、中 8、低 5。

修正是另一個 commit（不 amend）。小宣告第 13.3～13.6 節照修正後的程式重寫（修訂 1）。

### 設計審查

| 項 | 處理 | 一句理由 |
|---|---|---|
| S-1 「跟主目標走」太窄 | 採用 | `size` 改成 `variant<TargetSize, FollowTarget>`，可以跟主目標或另一個 target；來源的 `SourceTargetDesc` 跟一個相機目標。A4 的 HDR 中間圖要跟相機的目標 |
| S-2 每幀第一個 pass 清成透明黑 | 採用（照 Bevy） | render target 沒清除色時 `LOAD`，只有剛建或重建後清；主目標照舊。突變 `gt-target-first-shared` 退役，改成 `gt-target-not-kept` |
| S-3 「不同才寫」兩份 | 採用 | 相機的 `WriteTargetSize`／`RemoveTargetSize` 是唯一的出口，`window_camera` 與 `world_render` 都用它 |
| N-1 前綴 `a1-` | 採用 | 改成 `vs-clip-from-world-own` |
| N-2 CMake 註解 | 採用 | `world_render/CMakeLists.txt` 寫明 `RenderTargets` 與 System |
| N-3 `Queue3d` 的死分支 | 採用 | `Queue3d` 改讀 `CameraView::viewFromWorld`，不再自己求反矩陣 |
| N-4 CM-3 寫成全域唯一 | 採用 | 範圍寫成「visibility 與 render 的管線取它」；換算函式照 Bevy 自己算 |
| N-5 `EnsureTargets` 掃整張表、兩次查詢 | 採用 | target 另成一張表；pass 的目標每幀只解析一次 |
| N-6 字串字面量 | 採用 | `kExtractCameras` 常數 |
| N-7 條文數不一 | 採用 | 統一成 5 條（CM-1、CM-5、CM-7、RD-3、RD-11） |
| No-1 clipFromWorld 放在 `ComputedCamera` | 改了（見 H1） | 純淨審查 H1 指出的問題更重；改成照 Bevy 分開 |
| No-2 兩套 id、身分用近似 | 採用 | 身分改用 `RenderTargets::Table()`：換了表就全部放掉。移到 `render_world` 留給 RC-A1-1 |
| No-3、No-4 A2／A4 的準備 | 記錄 | 宣告第 13.3 節第 3 點、RC-A1-3 |
| No-5 多視窗時的 id 空間 | 記錄 | 新 RC-A1-4 |
| No-6 sRGB target 與 2D | 採用 | sRGB 改成不能當 target（見 M3） |
| (a) `world_render` 跑 System | 照判定 | 理由改寫成依賴的方向（宣告第 13.3 節第 2 點） |
| (b) 突變 27 個 | 照判定 | 合併前核准用的理由寫在宣告第 13.5 節；修正後是 33 個 |

### 純淨 Code Review

| 項 | 處理 | 一句理由 |
|---|---|---|
| H1 clipFromWorld | 採用 (1) | 新元件 `CameraView{valid, viewFromWorld, clipFromWorld}`，由 `UpdateCameraViews` 在 propagation 之後寫（Bevy 的 `Frustum`／`update_frusta`，`bevy_camera/src/visibility/mod.rs:503-507,627`）。`ComputedCamera` 回到原樣（CM-3 不動），視點不可逆時回 `InvalidData`（`[cm-singular-invalid-data]`）。可選的 `view` 參數拿掉，`ExtractedCamera` 只帶 `CameraView` |
| H1 跨 stage 過期 | 寫成契約、不加守衛 | `CameraOptions` 要指 transform 的 schedule 與 stage（同 visibility）。Kernel 只在同一個 stage 加順序邊 |
| H1 `WorldToViewport` 自己算 | 不改 | Bevy 的 `world_to_ndc` 也收 `GlobalTransform` 現算（`camera.rs:740-747`） |
| H2 主目標沒有名字 | 採用 | `Camera::kMainTarget`；pass 的目標改成 `std::optional<TextureId>`（`nullopt`＝主目標） |
| H2 id 空間沒有擁有者 | 採用 | 0 以外只有 `RenderTargets` 配（CM-5）。作者亂寫大小是誤用，不加守衛；測試裡的 `CameraTarget{9}` 拿掉，改測「相機算完後才放掉 target」這個真的會發生的情形 |
| H2 身分靠啟發式 | 採用 | `Table()` 世代 |
| H3 兩階段兩條件 | 採用 | `ResolveTargets` 一張表，`PlaceGeometry` 與 `RecordPasses` 都照它，沒有對應的只計一次 |
| H3 swapchain 拿不到時離屏也不畫 | 採用 | 先拿畫面，沒有畫面時 render target 照畫 |
| H3 Submit 與 Capture 共用跟隨者 | 不改 | `Capture` 只給測試與工具用，交替時只多一次重建、結果正確（主目標的深度本來也共用）；寫在 render 頁的範囲の限界 |
| H3 旗標結構 | 採用 | `GPUTexture`（圖片）與 `GPUTarget`（target）分開，查詢合成 `SampledOf` |
| H3 回授 | 採用 | 跳過並計 `skippedDraws`（`[gt-feedback-skipped]`） |
| H3 格式檢查寫兩次 | 採用 | 只在 `MakeTexture` 查 |
| H3 每幀 `++refusedAsks` | 採用 | 記住被拒的大小，同一個大小不再試、不再計 |
| H3 引數位置的副作用 | 採用 | `StartColour` 在呼叫之前的一句裡 |
| M1 兩張平行表 | 採用 | `RenderTextures` 一張表（`variant<TextureDesc, RenderTargetDesc>`）；`Update` 的訊息寫明 target 沒有像素 |
| M2 四份 id 表 | 部分 | 零尺寸 assert 的重複隨 `SourceTargetDesc` 分開了。共用的 `IdTable` 要動 A1 以前的 `RenderMeshes`，不在 A1 |
| M3 sRGB target 未定義 | 採用（拒絕） | target 只收 `Rgba8Unorm`（Programmer Error），A4 定線性路徑時再加 |
| M3 `ToString(TextureFormat)` 沒人用 | 採用 | 拿掉 |
| M3 enum 從 1 開始 | 不改 | 全專案的 enum 都這樣 |
| M3 sRGB 沒有生產者、`ImageRef` 不能指 target | 不改 | B4 才有生產者；RC-A1-1 |
| M4 `SameSize` 重複、差分 O(n²)、optional 取址 | 採用 | 改用 `WriteTargetSize`；差分照 id 順序用 `binary_search`；`TargetSizeOf` 直接收 `CameraTargetSizes` |
| M4 `world_render` 要和 camera 同 schedule | 寫明 | `WorldRenderOptions` 的註解 |
| M5 兩個同名的 `TargetSize` | 不改名 | `Render::TargetSize` 是 A1 以前就有的公開名字；新程式一律寫 `Camera::` 加以區分。uint32→int32 不檢查：超過 2³¹ 的大小 SDL 本來就拒絕 |
| M6 重複字面值 | 採用 | 同 N-6 |
| M7 `HashValue(RenderTargets)` 是空的 | 不改，寫明理由 | 作者給 render 的資料，同可畫元件；`CameraTarget` 是相機的設定，所以進雜湊 |
| M8 靜默丟掉相機、14 個參數 | 不改 | 只有「算完後才放」那一幀會發生，同無效的相機不計數；參數數是 Extract 的形狀 |
| 低：一案驗太多件 | 部分 | 拆出 helper（`PassesOn`、`TargetsMade`、`Released`）；跨幀的情節保留在一案 |
| 低：variant 索引 3 | 採用 | 改用 `get_if` |
| 低：`Device()` 單例 | 採用 | `TargetCase` RAII，每案自己放掉 |
| 低：缺 sRGB target 案 | 採用 | 改成 Programmer Error 的案（`render.data`） |
| 低：缺不支援格式被拒的案 | 不加 | CI 的 driver 都支援這兩種格式，碰不到 |
| 低：缺取樣自己 target、指向已放掉 target 的案 | 採用 | `[gt-feedback-*]`、`[gt-target-released-unmapped]` |
| 低：`MoveCamera` 的魔術數字 | 採用 | 改成宿主在兩幀之間移動相機 |
| 低：`PassTargetOfOk` 命名 | 採用 | `PassTargetOf` 拿掉了（主目標由 `ExtractCameras` 自己判） |
| 低：visibility 的 `after(kPropagateSet)` 多餘 | 不改 | A1 以前的程式，無害 |

### 修正後的突變

- 新 `target_mutations.json` 重寫成 33 個，全部 killed（`gt-` 的 7 個在 Metal 上）。
- 沒加的：
  - `UpdateCameraViews` 排在 `UpdateCameras`／propagation 之後的順序：Kernel 照登記順序排，拿掉也一樣，存活。
  - 新 target 是透明黑：Metal 上新貼圖本來是 0，分不出來。
  - 其餘理由寫在清單的 `description`。
- 既有的：find 落在改過的程式裡的 176 個重跑（render 118、instancing 15、visibility 4、camera 38、window_input 1），結果 175/176 killed。存活的 1 個是 `cm-update-set-missing`：`UpdateCameraViews` 也在 set 裡、又排在 `UpdateCameras` 之後，只把 `UpdateCameras` 拿出 set，set 的讀者照樣在它之後，所以成了等價突變。改寫成兩個 System 都拿出 set（守同一個性質：set 的讀者看得到這一幀的相機），重跑 killed。
  - 改寫的：`vs-inactive-checked`、`gp-load-always`、`gp-clear-ignored`、`gp-release-kept`、`gp-unmapped-drawn`、`p2-view-ignored`、`p2-3d-drawn`、`p3-view-ignored`、`p3-2d-drawn`、`g3-texture-missing-not-counted`、`g3-first-kept`、`gp-frames-never-shown`、`g3-frames-never-shown`。
  - 回到 A1 前 find 的：`rd-tex-reuse`、`rd-tex-release-kept-live`、`cm-target-always-zero`、`cm-letterbox-ignored`。
  - 改寫的（存活後）：`cm-update-set-missing`（見上）。
  - 合併的：`g3-colour-cleared`→`gp-load-always`；`rd-target-release-kept`→`rd-tex-release-kept-live`（照 README 的合わせ方）。

### 修正後的驗證（本機 macOS Intel、Metal）

- 全量建置成功。新出現的警告只有 A1 以外既有的（`Scenes.cpp:370`、`SDLWindowTests.cpp`），因為標頭改了而重新編譯。
- `ctest -L 'cpu|shader'`：253/254。失敗的是 `cmake.PluginSelection`，同上，是借用依賴的環境問題。
- GPU 的 `render.*`／`sdl_gpu.*`：5/5，`sdl_gpu.targets` 五案在其中。
- PvP 的程式：`ClientWorld.cpp`、`Scenes.cpp` 宿主組的相機各加一行 `CameraView{}`。PvP 的 cpu 測試在上面的 253 裡，通過。

## 沒做／沒跑

- A2（phase、全畫面 pass、只寫深度的 pass、RP-3 的「讀沒寫過的 target」斷言）、A3、A4（`Rgba16Float`、HDR）、B1、F1（mip、壓縮）、D2（MSAA 的 target）。
- 來源的 sprite／網格直接拿來源的 target 當圖（RC-A1-1）、target 的比例大小（RC-A1-2）、每個 target 的深度貼圖共用（RC-A1-3）。
- 沒跑：Linux（Vulkan／Lavapipe）、Windows（D3D12）上的 `sdl_gpu.targets`——交給 L1。重 GPU 量測沒做。

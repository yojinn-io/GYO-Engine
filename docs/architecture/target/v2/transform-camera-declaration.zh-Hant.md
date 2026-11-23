# 事前宣告：`transform` 與 `camera` Module（SDL 線子批 X1，`v8-arch-gyo2-sdl-transform-camera`）

更新：2026-11-23（修訂 1）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：交主會話轉使用者做動工前核准；核准前不動程式。** 要使用者定的一題在第 13 節。

**經過**：修訂 0（8b092f1f）送 Opus／high 敵對式檢查（**作者與檢查者同為 Opus**：Fable 用量不足），blocker 1、should 9、nit 12，全部採用或寫明處置（對照在第 14 節）。修訂 1 沒有再送檢查。

上層：SDL 總宣告 `sdl-declaration.zh-Hant.md` 修訂 21（分支 `claude/gyo2-sdl-declaration-7e7a1f`，26e37273；使用者 2026-11-23「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 3.3 節（列的角色表）、第 6.4 節、第 10 節 X1 列。總宣告第 11 節的七項整條線只寫一次，本宣告只寫差分（第 7 節）。

## 0. 根據與讀的版本、時間估計

- 程式：`origin/gyo2` 545b5a67。`engine/core/math`（`Matrix4.hpp`：`ComposeTRS`、`MakePerspective`〔左手、深度 0～1〕、`MakeOrthographicPixels`、`Inverse`；沒有一般的正交投影）、`docs/architecture/math.md`（Conventions：左手、+X 右、+Y 上、+Z 前；Boundary：「相機、語意上的 transform 留在擁有者」）。Kernel：`Registrar::InitialValue`（`Registry.hpp:295-318`；只有 Target 的擁有者能宣告，`Registry.cpp:758-764` 是 RE4）、`Hook`、`HookCtx`（`Params.hpp:1344`）、`Changed<T>`（`Params.hpp:361`；覆寫也標 changed，`detail/Storage.hpp:135-143`）、`Hash.hpp`（`HashValue` 以 ADL 找；沒有函式時 `Summary` 才報錯，DI-KS 27）。DI-KS 12（`Restore` 不補 `InitialValue`、通知照跑、hook 記的變更照常補）、DI-KS 9（hook 記的變更屬下一窗）、DI-KS 18。系統契約說明頁第 17 節（讀檔走 `Spawn`，初始值照補；準備函式的 `World::Insert` 不補）。`engine/modules/world_time`（掛載選項的先例）、`entity_ids`（預設帶 Module 的最小形）。
- 文件：HANDOFF `v8-arch-gyo2-2d-and-3d`、`-transform-camera-physics-modules`、`-hierarchy-bevy-and-primary-reference`、`-sdl-declaration-approved`；批次表〈每批的設計審查〉第 5 項（Bevy 主參考）；命名規範 N13、N17、N18、N19、N25；`docs/cross-component-rules.md`（CCR-8）。
- 別家（本機）：Bevy 6a911653（`~/Code/GitHome/bevy`）：`bevy_transform/src/components/transform.rs:69-104`（`require(GlobalTransform, TransformTreeChanged)`、`translation` 的 z 在 2D 用來排序）、`global_transform.rs:60`（`Affine3A`）、`plugins.rs:21-48`（`PostStartup` 與 `PostUpdate` 各跑一次，`TransformSystems::Propagate`）、`systems.rs:42-79`（`sync_simple_transforms`：`Changed<Transform>`／`Added<GlobalTransform>`，`Without<ChildOf>`、`Without<Children>`）；`bevy_camera/src/camera.rs:63-71`（`Viewport`）、`:222-227`（`ComputedCameraValues`）、`:380-417`（`Camera`，`require(Transform, RenderTarget, Frustum, …)`）、`:713-725`（`viewport_to_world_2d`）、`:896-913`（`RenderTarget`）；`components.rs:11-31`（`Camera2d` 帶正交 `default_2d`、`Camera3d` 帶 `Projection` 預設透視）；`projection.rs:217-221`（`Projection` 三種）、`:284-310`（透視）、`:523-555`（`ScalingMode`）、`:771-791`（`default_2d` near −1000、`default_3d` near 0、far 1000）；`bevy_render/src/camera.rs:66-83`、`:357-435`（`camera_system` 在 `PostUpdate`／`PostStartup`、讀視窗大小更新投影）。
- **時間估計**：實作 14～18 小時（transform 3、camera 5、math 的正交投影 1、測試 4、突變 2、文件與 dev_log 2、L1 與審查 1）。照時間盒子，超過 1.5 倍或單一問題超過 2 小時就停下回報。

## 1. 先用遊戲講

**Object_Connect（2D）**：每個節點是一個帶 `Transform` 的實體，`translation` 的 x、y 是畫面上的位置、z 決定前後。生成時那一個同步點，`GlobalTransform` 就已經等於 `Transform`（不必等到 `PostUpdate`）。相機是一個帶 `Camera2d` 的實體（作者在同一筆 bundle 帶 `Transform`）：正交投影、`ScalingMode::Fixed{1280, 720}`；玩家把視窗拉大時，`camera` 從「目標大小」重算投影，滑鼠位置用 `ViewportToWorld2d` 換回 1280×720 的遊戲座標。黑邊（視區）怎麼畫是 R2b 的。

**PvP Client（3D）**：角色帶 `Transform`；作者把 `Transform` 登記成可回滾（`Transform` 有逐位元的 `==`，回滾的登記照用）。回滾把一個預測中被打死的角色放回來（`Restore` 讓它復活）——放回的格子裡有 `Transform`、沒有 `GlobalTransform`（它不登記回滾、`Restore` 不補初始值）。`transform` 的 OnAdd hook 在那次放回裡補上 `GlobalTransform`，角色不會在畫面上消失。判定（命中、移動）只讀 `Transform`，不讀 `GlobalTransform`。

**Match（沒有畫面）**：`transform`、`camera` 預設帶；沒有相機實體、沒有目標大小，`camera` 的 System 什麼都不做；`transform` 照常維護 `GlobalTransform`（判定不讀它）。

## 2. 範圍

### 2.1 這一批做的

| # | 內容 |
|---|---|
| X1-1 | 新 Module `transform`（`engine/modules/transform`，`GYO::Transform`，`TransformModule`）：`Transform`（含逐位元的 `==`）、`GlobalTransform`、`GlobalTransform` 從 `Transform` 的初始值（`InitialValue<GlobalTransform, &FromTransform>`）、OnAdd<`Transform`> hook（補 `Restore` 的洞）、同步 System `SyncTransforms`、公開的 set、掛載選項。登記檔 `default=yes`、沒有 Module 依賴 |
| X1-2 | 新 Module `camera`（`engine/modules/camera`，`GYO::Camera`，`CameraModule`）：`Camera`、`CameraTarget`、`Projection`（透視／正交）、`ScalingMode`（`WindowSize`、`Fixed`）、`Camera2d`、`Camera3d`、`ComputedCamera`、Resource `CameraTargetSizes`、System `UpdateCameras`、公開的 set、掛載選項、座標換算三個函式。登記檔 `default=yes`、依賴 `transform` |
| X1-3 | `engine/core/math` 加一般的正交投影 `MakeOrthographic`（左手、深度 0～1，同 `MakePerspective` 的慣例）與 math.md 的一行（第 4.4 節；總宣告第 6.4 節寫「投影的數學用 core/math 現有的」，這一項是對它的差分） |
| X1-4 | 兩頁 owner 文件（`target/v2/transform.zh-Hant.md`、`camera.zh-Hant.md`）含 `Design Invariants` 與 `Replacement Criteria`、用法與後果（含總宣告第 6.4 節要的「非等比縮放配上父子會出現剪切」，X2 才有父子、說明先寫）；`docs/cross-component-rules.md` 加一列（第 4.5 節） |
| X1-5 | 測試、突變清單、dev_log、批次表與 README 的列 |

### 2.2 不在這一批

- 讀 `ChildOf` 的階層傳播、孤兒與換父的重算：X2（等 KR）。X1 的 `SyncTransforms` 是 X2 的「沒有父子的那一半」（Bevy 的 `sync_simple_transforms`），X2 加上 `Without<ChildOf>`／`Without<Children>` 與傳播的 System。
- 靜態樹的最佳化（Bevy `TransformTreeChanged`、`StaticTransformOptimizations`）：沒有量到的需要，RC-TF 1。
- 內插要的「上一步的姿態」（總宣告第 6.1 節 7、6.4 節的洞 (4)）：本線沒有點名的 Feature 用到內插（Object_Connect 每幀算、KeepTool 不用），X1 不加元件；第 9 節回報主會話。
- 畫面的一切（`Frustum` 元件與可見性、清除色、render target 的貼圖、MSAA、排序與合批、2D 的前後方向怎麼排）：R2b。
- 「目標大小」由視窗填進來的膠水 System 放哪：W（第 4.2 節）。
- Transform 的回滾、複製、權威登記：作者與網路那一側（總宣告第 6.4 節）。
- 物理：不在本線。

## 3. 基盤 Architecture 的根據（AGENTS §2）

**為什麼是基盤**：每個 2D／3D 遊戲、render 的 Extract（R2b）、網路複製、回滾、權威登記都碰 `Transform`；相機的資料是 R2b、U、Object_Connect 的滑鼠換算積在上面的介面。上面積的越多，換形越貴。

| 根據 | 種類 | 導出的要求 |
|---|---|---|
| Object_Connect（Q5） | 1 名指的 Feature | 一套 `Transform`、z 決定前後；正交相機、固定解析度的縮放（`Fixed`）與視區欄位；螢幕座標換回遊戲座標 |
| PvP Client（計畫上） | 1 | 3D 透視相機；回滾復活的實體仍有 `GlobalTransform`；判定不讀 `GlobalTransform` |
| R2b（本線） | 1 | Extract 讀 `GlobalTransform` 與 `ComputedCamera`（投影矩陣、視區）。`Changed<ComputedCamera>` 只在值真的變時成立；`Changed<GlobalTransform>` 照 Bevy 是「`Transform` 被寫過」（值相同也算，CCR-8），不是「真的變了」 |
| Match | 1 | 沒有畫面也建得起、跑得起；每幀成本接近零 |
| Bevy `bevy_transform`、`bevy_camera`（主參考）；Unity Entities `LocalTransform`（對照） | 2 | 第 6 節 |
| 規模與預算 | 3 | **沒有出處**；X1 不宣告效能判定（實體數萬以下每幀一次 `ComposeTRS` 量級），預算由 R2b 宣告，超過時 RC-TF 1 |

## 4. 設計（Contract）

### 4.1 `transform`

```cpp
namespace GYO::Engine::Transform {
struct Transform final {            // 作者寫的權威姿態：相對父的局部姿態；沒有父時等於全域
    Math::Vec3 translation{};
    Math::Quaternion rotation{};    // 預設單位四元數
    Math::Vec3 scale{1, 1, 1};
    friend bool operator==(const Transform&, const Transform&) noexcept;   // 逐位元（每個 float 比 bit），和 HashValue 一致
};
struct GlobalTransform final {      // 推導值：全域矩陣，只由 transform 寫
    Math::Matrix4 matrix{};         // 預設單位矩陣
};
[[nodiscard]] Math::Matrix4 ToMatrix(const Transform&) noexcept;   // = Math::ComposeTRS
struct TransformOptions final {     // WorldDescription::options；骨架關掉的 World 由作者指名
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::postUpdate);
};
inline constexpr std::string_view kPropagateSet = "TransformPropagate";   // 公開的 set（Bevy TransformSystems::Propagate）
class TransformModule final : public Orchestration::Module { void Build(Orchestration::WorldSetup&) override; };
}
```

- **相等**（檢查 B1）：Math 的 float 向量沒有 `==`（math.md Types），回滾的登記沒有 `==` 時要 `DataOptions::equal`（`rollback/Registrations.hpp:281`），`Restore` 以 eq 判斷寫不寫（`Params.hpp:902-918`），摘要卻逐位元雜湊（±0 不同）。用 float 的 `==` 當 eq，−0 與 +0 被當成相同而不寫，摘要就和直跑不同、R-2 破。所以 `transform` 給 `Transform` 一個**逐位元**的 `operator==`（每個 float 比 bit；NaN 與 NaN 位元相同就相等），和 `HashValue` 一致；作者登記回滾時照用、不必自寫 equal。這是 `transform` 這個 owner 對自己型別的定義，不改 Math 的「float 向量沒有 `==`」。`GlobalTransform` 不給 `==`（不登記回滾）。
- **登記**：兩個元件；`HashValue(Transform)` 逐欄（translation 3、rotation 4、scale 3 個 float，照 `Hash.hpp` 的 float 規則）；`HashValue(GlobalTransform)` **什麼都不寫**（總宣告第 6.4 節：推導值，同 `AssetView`；不寫就是 `HashMissing`）。
- **`InitialValue<GlobalTransform, &FromTransform>`**（transform 是 `GlobalTransform` 的擁有者，RE4 允許）：`Transform` 出現在實體上的那個同步點（生成的 bundle、或往沒有它的實體插入），Kernel 當場放 `GlobalTransform{ToMatrix(transform)}`。對應 Bevy 的 `require(GlobalTransform)`，而且一出現就是對的值（Bevy 先放單位矩陣、等 `PostStartup`／`PostUpdate`）。所以總宣告第 6.4 節洞 (1)「建 World 或讀檔後第一幀」不需要 Bevy 的 `PostStartup` 那一次：讀檔走 `Spawn`（說明頁第 17 節），生成世界也是。
- **OnAdd<`Transform`> hook `EnsureGlobalTransform`**：實體沒有 `GlobalTransform` 時，用 `Commands` 插入 `GlobalTransform{ToMatrix(transform)}`。只在 `InitialValue` 沒補到的路上起作用——`Restore`（DI-KS 12：不補初始值、通知照跑、hook 記的變更在那次當場改回傳前套用，DI-KS 18）；一般的生成與插入時 `InitialValue` 已先放好（OnAdd 在初始值之後），hook 看到已有、不做事。準備函式的 `World::Insert` 不跑 hook 也不補初始值，說明頁第 17 節已寫「要靠初始值補衍生元件的，不要走這條」，照舊——後果要寫明：那樣放的實體**一直**沒有 `GlobalTransform`（`SyncTransforms` 要求 `Write<GlobalTransform>`，不會替它補），不只第一幀。
  - **兩條路都只補「沒有」的情形**（初始值不蓋已有的值，`detail/Storage.hpp:360-365`；hook 也只在沒有時插入）。實體上已有舊的 `GlobalTransform`（例：拿掉 `Transform`、留下 `GlobalTransform`，之後又插入或放回 `Transform`）時，放回那一刻是舊值；`Transform` 是新的格（`Changed` 成立），下一次 `kPropagateSet` 就對了。TF-3 照這個寫。
  - **退掉的選項**（AGENTS §4）：(i) 只用一個 System（例如 `SyncTransforms` 多一個 `Without<GlobalTransform>` 的 query、經 `Commands` 補）——它涵蓋 `Restore`、`World::Insert`、作者拿掉的情形，但補上要等 `PostUpdate` 之後的同步點，回滾復活的角色至少少畫一幀；一般的生成也晚一窗。退掉的理由：初始值是 Kernel 給「推導的初始值」的機制，`Restore` 的通知是 Kernel 給「放回之後要做的事」的機制，兩條都是用機制、不繞過；`World::Insert` 與作者拿掉是誤用，不替它加補救（`v8-arch-gyo2-design-review-stage`）。(ii) 只用 hook——一般的生成晚一窗（DI-KS 9：hook 記的變更屬下一窗），同上。
- **`SyncTransforms`**（Bevy `sync_simple_transforms`）：`Query<Read<Transform>, Write<GlobalTransform>, Changed<Transform>>`，把 `matrix` 設成 `ToMatrix(transform)`。預設掛 `Main`／`PostUpdate`，放進 `kPropagateSet`。`Transform` 被寫過就重寫（值相同也寫，同 Bevy `systems.rs:66-71`），所以 `Changed<GlobalTransform>` 的意思是「`Transform` 被寫過」（CCR-8），說明頁寫明。
  - **變更偵測照 Bevy**：Kernel 的覆寫、`GetMut`、`Restore` 寫的格、讀檔放回的格都標 changed（第 0 節；說明頁第 17 節第 581 行），所以總宣告洞 (2)「放回要被看成變更」由 Kernel 的現行行為滿足——X1 用測試釘住（第 8.1 節 T-5、T-6），不另做機制。
  - Bevy 另有 `Added<GlobalTransform>` 那一支（因為 require 放的是單位矩陣）；GYO 的初始值與 hook 放的已是對的值，**不需要**這一支。
  - X1 的 Kernel 沒有 `ChildOf`，所以 X1 的 `SyncTransforms` 走所有帶 `Transform` 的實體；X2 加 `Without<ChildOf>`、`Without<Children>` 與傳播。
- **掛載選項**（總宣告洞 (3)）：照 `WorldTimeOptions` 的先例；骨架關掉的 World 由作者指名 schedule 與 stage，名字不存在是 Kernel 的 D-E1。

### 4.2 `camera`

```cpp
namespace GYO::Engine::Camera {
struct Viewport final {             // 目標裡的矩形，物理像素，左上原點（Bevy 用 UVec2；GYO 用 Vec2i，夾進目標時負的位置當 0）
    Math::Vec2i position{};
    Math::Vec2i size{};
    float minDepth = 0.0F;
    float maxDepth = 1.0F;
};
struct Camera final {               // 作者寫的設定
    std::int32_t order = 0;         // 大的後畫（Bevy isize；GYO 固定寬度，雜湊不看平台）
    bool active = true;
    std::optional<Viewport> viewport;   // 沒有：整個目標
};
struct CameraTarget final {         // 畫到哪個目標：中性的 id；0＝主要的目標（有視窗時是主視窗）
    std::uint32_t id = 0;           // 其他 id 的意思由 render／sdl_gpu 對（R2a、R2b）
};
struct PerspectiveProjection final { float verticalFov = Math::Pi / 4; float nearClip = 0.1F; float farClip = 1000.0F; };
struct ScalingWindowSize final { };                       // 1 世界單位＝1 邏輯像素（再乘 scale）
struct ScalingFixed final { float width = 1.0F; float height = 1.0F; }; // 不管目標大小，固定看到這麼大（會拉伸）
using ScalingMode = std::variant<ScalingWindowSize, ScalingFixed>;
struct OrthographicProjection final {
    float nearClip = 0.0F; float farClip = 1000.0F;
    Math::Vec2 viewportOrigin{0.5F, 0.5F};
    ScalingMode scalingMode = ScalingWindowSize{};
    float scale = 1.0F;
    static OrthographicProjection Default2d() noexcept;   // 近遠的取法交使用者（第 13 節）
};
using Projection = std::variant<PerspectiveProjection, OrthographicProjection>;   // 預設透視（Bevy Projection::default）
struct Camera2d final { };          // 標記：InitialValue 給 Camera 與正交的 Projection
struct Camera3d final { };          // 標記：InitialValue 給 Camera 與透視的 Projection
struct ComputedCamera final {       // 推導值，只由 UpdateCameras 寫
    bool valid = false;             // 目標大小已知、視區大小非零
    Math::Vec2i targetPhysicalSize{};
    float scaleFactor = 1.0F;       // 物理像素／邏輯像素
    Viewport viewport{};            // 實際用的視區（沒指定時＝整個目標）
    Math::Matrix4 clipFromView{};   // 投影矩陣（math.md 的 clip 慣例）
};
struct TargetSize final { Math::Vec2i physicalSize{}; float scaleFactor = 1.0F; };
class CameraTargetSizes final {     // Resource：目標 id → 大小。寫的是作者、測試與之後的膠水（W 定）
public:
    void Set(CameraTarget, TargetSize); void Remove(CameraTarget);
    [[nodiscard]] const TargetSize* Find(CameraTarget) const;
};
enum class ViewportConversionFailure : std::uint8_t { NoViewportSize, PastNearPlane, PastFarPlane, InvalidData };
// 位置：目標（視窗）的邏輯像素，左上原點、+Y 向下，含視區的偏移（同 Bevy viewport_to_ndc 先減視區的左上角）
[[nodiscard]] Base::Result<Math::Vec2, ViewportConversionFailure> WorldToViewport(const ComputedCamera&, const Transform::GlobalTransform&, Math::Vec3);
[[nodiscard]] Base::Result<Math::Ray,  ViewportConversionFailure> ViewportToWorld(const ComputedCamera&, const Transform::GlobalTransform&, Math::Vec2);
[[nodiscard]] Base::Result<Math::Vec2, ViewportConversionFailure> ViewportToWorld2d(const ComputedCamera&, const Transform::GlobalTransform&, Math::Vec2);
struct CameraOptions final {        // 預設同 TransformOptions（DefaultSchedules::frame／postUpdate）
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::postUpdate);
};
inline constexpr std::string_view kUpdateSet = "CameraUpdate";   // Bevy CameraUpdateSystems
class CameraModule final : public Orchestration::Module { void Build(Orchestration::WorldSetup&) override; };
}
```

（欄名、預設值照 Bevy；`near`／`far` 改成 `nearClip`／`farClip` 是因為 Windows 的 `windef.h` 把 `near`、`far` 定義成空巨集，PR #152、#163 各踩一次。）

- **初始值的鏈**（全部由 camera 自己擁有的 Target 宣告，RE4 允許）：`Camera2d` → `Camera{}`、`Projection{Default2d()}`；`Camera3d` → `Camera{}`、`Projection{PerspectiveProjection{}}`；`Camera` → `CameraTarget{}`、`ComputedCamera{}`。Kernel 的鏈在同一個同步點補完（`Registry.hpp:303-306`）。`Camera` 本身**不**給 `Projection`（同 Bevy：`Camera` 不 require `Projection`，`Camera2d`／`Camera3d` 才給）——否則同時帶 `Camera` 與 `Camera2d` 的生成，`Projection` 由 ComponentId 小的那個給，結果依登記先後。沒有 `Projection` 的 `Camera`，`UpdateCameras` 不處理（同 Bevy `camera_system` 的 query）。
- **`Camera` 不給 `Transform`**（**偏離 Bevy**：Bevy 的 `Camera` require `Transform`）：`Transform` 的擁有者是 `transform`，Kernel 只准擁有者宣告初始值（RE4，`Registry.cpp:758-764`）；讓 `transform` 宣告「從 `Camera` 補 `Transform`」會讓 `transform` 依賴 `camera`（方向反了）。所以作者在相機的 bundle 帶 `Transform`（Bevy 的範例也幾乎都寫 `(Camera2d, Transform::from_xyz(…))`）。膠水：作者多寫一個元件。沒有 `GlobalTransform` 的相機沒有視點：座標換算拿不到參數；render 怎麼處理在 R2b。**退掉的選項**：`camera` 的 OnAdd<`Camera`> hook 插入 `Transform{}`——hook 寫別人的元件不受 RE4 限制，能保住 Bevy「只寫 `Camera2d` 也看得到」的預設；退掉的理由：RE4 的用意是「推導的初始值只由擁有者宣告」（權威 Module 才查得到誰寫什麼），用 hook 達成同一件事是繞過這個機制；而且晚一窗（DI-KS 9）。
- **`UpdateCameras`**（Bevy `camera_system`）：`Query<Read<Camera>, Read<CameraTarget>, Read<Projection>, Write<ComputedCamera>>` 與 `Res<CameraTargetSizes>`。每幀對每個相機：找目標大小 → 算實際的視區（沒指定＝整個目標；指定的視區照 Bevy `clamp_to_size` 夾進目標）→ 算投影：透視用 `Math::MakePerspective(verticalFov, 視區寬／高, nearClip, farClip)`；正交照 Bevy 的 `ScalingMode`：`WindowSize` 用**邏輯**大小（物理÷`scaleFactor`）×`scale`、`Fixed` 用給的寬高×`scale`，以 `viewportOrigin` 定原點，用 `Math::MakeOrthographic`。找不到目標大小或視區大小是 0：`valid = false`、其他欄照預設。**值和現在的不同才寫**（`GetMut` 才拿；比較是逐位元，矩陣有 NaN 時也不會每幀都寫）：照 CCR-8，`Changed<ComputedCamera>` 才代表真的變了，R2b 的 Extract 可以靠它。和 Bevy 不同的兩點（第 5 節）：Bevy 只在視窗事件或相機改變時重算，GYO 每幀比較（相機少、不必接視窗事件）；Bevy 在縮放比改變時把作者給的視區照比例放大縮小（`bevy_render/src/camera.rs:399-414`），GYO 的視區是作者給的物理像素、不改作者的值（縮放比改變時要不要跟著放大由作者決定）。預設掛 `Main`／`PostUpdate`、放進 `kUpdateSet`；不讀 `GlobalTransform`，所以和 `kPropagateSet` 沒有先後（同 Bevy）。
- **座標換算**（Bevy `world_to_viewport`、`viewport_to_world`、`viewport_to_world_2d`）：自由函式、吃 `ComputedCamera` 與相機的 `GlobalTransform`（Bevy 是 `Camera` 的方法，因為它的推導值在 `Camera` 裡）。位置的座標系照 Bevy：目標的邏輯像素、含視區的偏移（`camera.rs:803-814`）——Object_Connect 的黑邊（帶偏移的視區）時，滑鼠的視窗座標直接傳進來。`valid == false` → `NoViewportSize`；矩陣不可逆或結果不是有限數 → `InvalidData`；`WorldToViewport` 的 NDC 深度小於 0 → `PastNearPlane`、大於 1 → `PastFarPlane`（照 Bevy `camera.rs:555-560` 的判法；所以透視相機**後面**的點深度大於 1、回 `PastFarPlane`，同 Bevy，說明頁寫明）。NDC 照 math.md：x、y 在 −1～1、+Y 向上，z 在 0（近）～1（遠）——Bevy 是反向 Z（1 近 0 遠），所以 `ViewportToWorld` 的近點取 z=0、遠點取 z=1。錯誤是 E2（`…Failure`，N25）：這次換算沒做成，其他都健康。
- **「目標大小誰給相機」**（總宣告第 3.3 節列給 X1 定的三個做法）：

| 做法 | 內容 | 代價 |
|---|---|---|
| (a) 照 Bevy，`camera` 依賴 `window` | `UpdateCameras` 直接讀 `window` 的大小 | X1 等 W（W 等 P）；貼圖目標（R2b）還是要另一條路；列了 `camera` 的 World 也要列 `window` |
| **(b) `camera` 定「目標大小」的資料，膠水或作者填** ← 選（總宣告的傾向，任務卡的指示） | Resource `CameraTargetSizes`：目標 id → 物理大小＋縮放比；`UpdateCameras` 只讀它 | 多一份「目標大小」的副本（下一段）；之後要有一個膠水 System 把視窗大小寫進來（W 定放哪） |
| (c) 只在渲染側算投影 | 來源 World 沒有投影 | 來源側做不了滑鼠換算與射線（Object_Connect 要），不採 |

  **State Authority（AGENTS §3）**：視窗的大小由 `window`（W）擁有、貼圖目標的大小由 render（R2b）擁有；`CameraTargetSizes` 是它們的副本。它屬於 §3 允許的那一類——`camera` 為自己的責務持有、每幀可以從擁有者重建、不參與判定（相機不是判定的資料），`camera` 也不拿它做重複除去、衝突解決之類的判斷，只拿來算投影。照 §3「加入關於別人擁有的狀態的欄位本身是 Ownership 的變更」，這一條列進第 7 節的 Architecture Delta，隨本宣告交使用者同意。
  **之後的膠水放哪（交 W）**：候選——作者或產品寫一個讀 `window`、寫 `CameraTargetSizes` 的 System；一個依賴 `window` 與 `camera` 的小單位。不採：`window` 寫它（`window` 會依賴 `camera`）、`sdl_window` 寫它（違反 SDL-7「只翻譯成 `input`、`window` 的 `Message`」）、渲染側寫回（違反 DI-KS 13）。
- **雜湊**：`Camera`、`CameraTarget`、`Projection`（先寫是哪一種，再逐欄）、`Viewport`、標記（空）照作者寫的值逐欄；`ComputedCamera` 什麼都不寫（推導值，同 `GlobalTransform`）；`CameraTargetSizes`（Resource）**什麼都不寫**：視窗大小每台機器不同，不是兩次執行必須一致的 World 狀態（先例：net 的 `Peer`，`NetModule.hpp:45-47`；`AssetView`）。
- **`CameraTargetSizes` 的初值**：`camera` 的準備函式在沒有值時放一個空的（照 `entity_ids` 的「沒有值才放」，O-7 帶過來的值留著），所以 `UpdateCameras` 的 `Res` 一定讀得到。新建或替換的 World 一開始是空的：膠水若只在大小改變時寫，`valid` 會一直是 false——交 W（「新 World 的初值」那一題）。

### 4.3 兩個 Module 和 Kernel、編排層的接法

- 兩個都是來源側的單位、`default=yes`（P16；總宣告第 3.3 節）。在每個選了它們的 World 的 `Build` 登記（`camera` 另有一個準備函式放 `CameraTargetSizes` 的初值）；沒有 `Configure`、`Check`、`OnDestroy`、共用處的東西。
- 骨架關掉、又沒有 `PostUpdate` 的 World：作者用選項指名，否則 Kernel 報 D-E1（同 `world_time` 的 `advanceStage`）。明列 `units` 的多 World 產品要自己列它們（總宣告第 3.3 節）。
- 依賴（`config/modules.csv`）：`transform` 沒有；`camera` → `transform`。CMake 連結：`transform` → Base、Kernel、Orchestration、Math；`camera` → 同上＋登記檔來的 `GYO::Transform`。

### 4.4 `engine/core/math` 加 `MakeOrthographic`

`Math::MakeOrthographic(left, right, bottom, top, nearClip, farClip)`：左手、x、y 映到 −1～1、深度 near→0、far→1（同 `MakePerspective` 的 clip 慣例，`MakeFrustum` 照用）。理由：math.md 的 Boundary 把「clip-space conventions」放在 Math、Numeric policy「每個運算在 GYO 只有一個實作」——正交投影是 clip 慣例的一部分，不放 `camera` 裡自寫。math.md Conventions 的 Clip space 一行補上它（英文，照檔案原語言）；`tests/common/math` 加特徵測試（角點、深度、`MakeFrustum` 的六面）。

### 4.5 跨部件規則（`docs/cross-component-rules.md`）

照 AGENTS §1：「`GlobalTransform` 是畫面用的推導值，一幀傳播一次、可能落後固定步、回滾後還是舊時間線的值 → 判定的 System（固定步、回滾重跑的步、權威登記的寫者）不讀它；要全域姿態時自己從 `Transform`（X2 之後加上 `ChildOf`）算」。查過規則頁沒有同類的列（CCR-1～CCR-11），owner＝`transform`，X1 加一列（Current owner：transform Module；Guard：沒有機械守衛、設計審查看；Known sites：沒有）。

## 5. 偏離 Bevy 與接合點（批次表〈每批的設計審查〉第 5 項）

| 項 | Bevy | GYO（X1） | 衝突點 | 膠水量 | 處置 |
|---|---|---|---|---|---|
| 座標慣例 | 右手、+Y 上、−Z 前；反向 Z（透視是無限遠，正交也在投影裡對調 near／far，`projection.rs:637-647`） | 左手、+Z 前、深度 0～1、不反向（math.md，已核准的 Math 統一） | 2D 的「z 大的在前面」在 Bevy 是 −Z 前加反向深度的結果；GYO 的正交相機看 +Z，照 near −1000／far 1000 取，深度隨 z 增加（z 小的在前面） | 無（Math 現成） | **`Default2d` 的近遠交使用者**（第 13 節）：對調就和 Bevy 的作者體驗一樣 |
| 相等 | glam 的 `==`（float 比較）；Bevy 沒有逐位元的摘要 | `Transform` 的逐位元 `==` | GYO 的 `Restore` 用 eq、摘要逐位元（R-2） | 一個 `operator==` | 檢查 B1；第 4.1 節 |
| `GlobalTransform` 的存法 | `Affine3A`（48 bytes） | `Math::Matrix4`（64 bytes；Math 沒有仿射型別） | 無 | 無 | 照 Math 現有型別 |
| 「有 `Transform` 就有 `GlobalTransform`」 | `require`，任何插入的路都補 | `InitialValue`＋OnAdd hook（`Restore` 不補初始值，DI-KS 12） | Kernel 把「推導資料」交給呼叫者，`Restore` 只跑通知 | 一個 hook（約 10 行） | 寫明兩條路各管哪裡；第三條路出現時 RC-TF 2 |
| `Camera` require `Transform` | 有 | 沒有（RE4：只有擁有者宣告初始值） | Kernel 的擁有者規則 | 作者在 bundle 帶 `Transform` | 照 Kernel 規則；說明頁寫 |
| 推導值的位置 | `Camera.computed`；`PerspectiveProjection.aspect_ratio`、`OrthographicProjection.area` 由 `camera_system` 寫回作者的元件 | 另一個元件 `ComputedCamera`；`Projection` 只有作者寫的欄 | 一個元件兩個寫者時，摘要與 `Changed` 混在一起 | 一個元件＋一個初始值 | 和 `GlobalTransform` 同一個形：作者寫的與推導的分開 |
| 目標大小 | `camera_system` 讀 `Window` | `CameraTargetSizes`（第 4.2 節 (b)） | GYO 的視窗不是實體、`camera` 不依賴 `window` | 一個 Resource；之後一個膠水 System（W 定） | 總宣告第 3.3 節的傾向；RC-CM 1 |
| 目標的表示 | `RenderTarget` 列舉（Window／Image／TextureView／None） | `CameraTarget{id}`，中性 id | Bevy 的視窗與貼圖是實體或素材把手；GYO 的 `camera` 不認得 render、`window` | id 的對照在 render／`sdl_gpu`（R2a、R2b） | 總宣告第 6.6 節已列 |
| 縮放模式 | 六種 | `WindowSize`、`Fixed`（點名的 Feature 只用到這兩種） | 無 | 無；之後加種類是加法、不破壞 | 薄起步（AGENTS §2） |
| 投影種類 | 透視、正交、Custom | 透視、正交 | 無 | 無 | Custom 沒有點名的 Feature |
| `Frustum`、可見性、清除色、MSAA、sub camera | 在 `bevy_camera`／`Camera` | 不在 X1 | 無 | R2b 加（加在 `Camera` 或自己的元件，R2b 定） | 渲染那一側 |
| 掛載位置 | 固定 `PostUpdate`（＋`PostStartup`） | 預設 `Main`／`PostUpdate`，選項可改；不需要 `PostStartup` 那一次 | GYO 的骨架可以關（O-4） | 一個選項 struct | 照 `world_time` 的先例 |
| 座標換算 | `Camera` 的方法；位置是目標的邏輯座標、含視區偏移；透視相機後面的點回 `PastFarPlane` | 自由函式，吃 `ComputedCamera`＋`GlobalTransform`；座標系與錯誤的判法照 Bevy | 推導值不在 `Camera` 裡 | 無 | — |
| 重算的時機 | 視窗事件或相機改變時才重算 | 每幀比較、不同才寫 | GYO 的 `camera` 不讀視窗事件 | 無（相機少） | — |
| 縮放比改變時的視區 | 照比例改作者給的視區（`bevy_render/src/camera.rs:399-414`） | 不改作者的值 | 一個元件不讓兩個寫者改（作者的 `Camera` 不被 System 寫） | 要跟著放大的作者自己改 | 和 `ComputedCamera` 另立同一個理由 |
| 變更偵測 | `Changed<Transform>`＋`Added<GlobalTransform>` | 只有 `Changed<Transform>` | 初始值與 hook 放的已是對的值 | 無 | — |
| 名字 | `translation`、`near`、`far`、`isize order` | `translation`；`nearClip`、`farClip`（`windef.h` 巨集）；`std::int32_t order` | 平台巨集；雜湊要固定寬度 | 無 | — |
| 命名空間與型別同名 | `bevy_transform::Transform` | `GYO::Engine::Transform::Transform`、`Camera::Camera` | C++ 裡同名類別遮住 namespace | 引用時照 N13 的例外多寫一層 | N13 已有先例（`IO::Path`） |

## 6. 測試、守衛、突變

### 6.1 測試（先寫、先看到失敗）

`tests/common/transform`、`tests/common/camera`、`tests/common/math`；Product 用合成的單位表（同 `entity_ids` 的測試），不依賴任何產品。T-7 的測試執行檔另連回滾 Module（只在測試；`transform` 本身不連）。

| # | 案 | 守的 |
|---|---|---|
| T-1 | 生成帶 `Transform` 的實體（一般的生成，與說明頁第 17 節讀檔式的生成各一）：那個同步點之後 `GlobalTransform.matrix == ComposeTRS(…)`（不必等 `PostUpdate`） | TF-3、洞 (1) |
| T-2 | 改 `Transform`（`GetMut`）：`PostUpdate` 之後 `GlobalTransform` 跟上；`PostUpdate` 之前讀到的是舊值（說明頁寫的後果） | TF-3 |
| T-3 | 沒改 `Transform` 的幀，`SyncTransforms` 不寫 `GlobalTransform`（`Changed<GlobalTransform>` 不成立） | CCR-8 的讀法 |
| T-4 | 骨架關掉的 World 用選項指名掛載位置；不指名是 D-E1 | 洞 (3) |
| T-5 | `Restore` 讓一個帶可回滾 `Transform` 的實體復活：放回之後立刻有 `GlobalTransform`、值等於 `ComposeTRS(放回的 Transform)` | TF-3、hook |
| T-6 | `Restore` 把 `Transform` 改回不同的舊值（實體一直活著；值相同的格 `Restore` 不寫）：下一次 `PostUpdate` 之後 `GlobalTransform` 等於舊值 | 洞 (2) |
| T-7 | 回滾等價（檢查 S9）：用回滾 Module 的官方策略、`Transform` 登記成可回滾（用 `transform` 的 `==`，不給 `DataOptions::equal`），一個實體在預測中被刪、回滾讓它復活；直跑與回滾重跑的 `Summary` 相同（摘要逐型別雜湊「有格的實體」，`World.cpp:1176-1183`，所以 `GlobalTransform` 的有無本身也比到）。另一案：`translation.x` 是 −0 與 +0 時 `==` 為假 | TF-2、TF-3、R-2 |
| T-8 | 兩個 World 的 `Transform` 相同、`GlobalTransform` 不同（一個還沒傳播）：`Summary` 相同；`Transform` 的任一欄不同：`Summary` 不同 | TF-2 |
| T-9 | `transform` 的 `Build` 不登記任何回滾、權威的資料（讀唯讀登記表；總宣告 X1 列「`GlobalTransform` 不在回滾的登記裡」；連結集 C-6 不連回滾、權威是第二層） | TF-2 |
| C-1 | `Camera2d` 生成：同一個同步點有 `Camera`、`CameraTarget{0}`、正交 `Projection`（`Default2d()`，近遠照第 13 節第 1 題）、`ComputedCamera{valid=false}`；`Camera3d`：透視 | CM-1 |
| C-2 | 沒有目標大小：`valid == false`；設了大小之後下一次 `UpdateCameras` 變 `true`，投影等於期望矩陣（透視：寬／高；正交 `WindowSize`：邏輯大小，`scaleFactor` 2 時是物理的一半；`Fixed`：不隨大小變；`viewportOrigin` (0, 1)、`scale` 2 各一案；兩個相機分別畫到 id 0 與 id 1、兩個目標大小不同） | CM-3、CM-4 |
| C-3 | 指定視區：比例用視區不用目標；視區超出目標時夾進去（含負的位置） | CM-3 |
| C-4 | 大小沒變的幀不寫 `ComputedCamera`（`Changed` 不成立） | CM-3、CCR-8 |
| C-5 | 座標換算：`WorldToViewport` 與 `ViewportToWorld` 來回一致（透視、正交各一）；`ViewportToWorld2d` 在 1280×720 `Fixed` 相機、視窗 2560×1440 時把右下角換成 (640, −360)、左上角換成 (−640, 360)（相機在原點、`viewportOrigin` 0.5）；**帶偏移的視區**（視窗 2560×1600、視區在 (0, 80) 大小 2560×1440，黑邊）時把視窗座標 (2560, 1520) 換成 (640, −360)；`valid == false` 回 `NoViewportSize`；正交相機近平面前的點回 `PastNearPlane`、遠平面後回 `PastFarPlane`；透視相機在 0 與 near 之間的點回 `PastNearPlane`、相機後面的點回 `PastFarPlane`（同 Bevy） | CM-4 |
| C-6 | `camera` 的連結集只有 Base、Kernel、Orchestration、Math、Transform；`transform` 的沒有 Transform 以外的 Module | CM-2、TF-4 |
| M-1 | `MakeOrthographic`：八個角點映到 clip 的八個角、深度 near 0 far 1；`MakeFrustum(MakeOrthographic(…))` 的六面 | math 的慣例 |

數值比較：矩陣用測試裡寫明的容差比（`MakePerspective` 用 `tan`，libm 不保證跨平台逐位元相同，math.md Numeric policy）；`ComposeTRS` 的比較用逐位元相等（兩邊同一個函式、run-time 資料）。

### 6.2 守衛（AGENTS §3「ガードを立てる時期」）

- 行為的測試與機構同時寫（上表）。
- 實作方式的守衛（掃描名字、寫者數）不立：等 R2b 的 scenario（真的有畫面讀這兩個 Module）通過之後再看；之前 Design Invariants 寫在文件、由設計審查看。連結集（`gyo_expect_links`）照 `entity_ids` 的先例立（它是建置圖的事實，不是實作方式）。
- 不替誤用加守衛（`v8-arch-gyo2-design-review-stage`）：作者在判定裡讀 `GlobalTransform`、相機不帶 `Transform`，由 CCR 與說明頁講、審查看。

### 6.3 突變（AGENTS §14.2、§14.5；D184）

清單檔 `docs/architecture/target/v2/scripts/transform_mutations.json`、`camera_mutations.json`；每個突變寫它唯一抓到的誤與守的條文；只在預期的失敗（測試名＋專用訊息）出現時算 killed。候選（實作時照 §14.5 收斂，數與理由寫 dev_log）：

| id | 改法 | 預期失敗 |
|---|---|---|
| `tf-initial-identity` | 初始值放單位矩陣 | T-1 |
| `tf-sync-not-mounted` | 不登記 `SyncTransforms` | T-2 |
| `tf-sync-ignores-changed` | 拿掉 `Changed<Transform>`、每幀都寫 | T-3 |
| `tf-restore-hook-missing` | 不登記 OnAdd hook | T-5 |
| `tf-hash-global-nonempty` | `GlobalTransform` 的雜湊寫矩陣 | T-8 |
| `tf-hash-transform-drops-scale` | `Transform` 的雜湊漏 scale | T-8 |
| `tf-eq-float-compare` | `Transform` 的 `==` 改成 float 比較 | T-7（−0／+0 那一案） |
| `cm-camera2d-perspective` | `Camera2d` 的初始值給透視 | C-1 |
| `cm-valid-without-size` | 沒有目標大小也 `valid = true` | C-2 |
| `cm-windowsize-physical` | `WindowSize` 用物理大小 | C-2 |
| `cm-viewport-ignored` | 比例用目標不用視區 | C-3 |
| `cm-viewport-not-clamped` | 視區不夾進目標 | C-3 |
| `cm-origin-ignored` | 正交的區域固定以中心為原點 | C-2（`viewportOrigin` 那一案） |
| `cm-scale-ignored` | 正交忽略 `scale` | C-2（`scale` 那一案） |
| `cm-target-always-zero` | 一律讀 id 0 的大小 | C-2（兩個目標那一案） |
| `cm-writes-when-equal` | 值相同也寫 | C-4 |
| `cm-ndc-reverse-z` | 換算照 Bevy 的反向 Z（近點取 z=1） | C-5 |
| `cm-ndc-y-not-flipped` | 視區座標換 NDC 時 y 不翻 | C-5（左上角那一案） |
| `cm-viewport-offset-ignored` | 換算不減視區的左上角 | C-5（黑邊那一案） |
| `math-ortho-depth-flipped` | `MakeOrthographic` 的深度 near→1 | M-1 |

清單在實作完成時送和作者不同模型的敵對式檢查（§14.2；Fable 用量不足期間 Opus／high，報告寫明同模型）。D184：本批只跑新的突變；合併前背景完整跑一次 PR 線。

### 6.4 既有測試的影響（總宣告第 15 節「預設帶的 Module 增加對 Registry 指紋與 golden 的影響」）

`git grep -n MakeRegisteredUnits -- tests engine games`（先以已知命中 `tests/common/entity_ids/DefaultUnitsTests.cpp` 驗過掃描有效）：用真的登記檔建 Product 的只有 `gyo_entity_ids_default_units_tests`（`DefaultUnitsTests.cpp`），它只檢查生成與 `entity_ids` 在表裡，不比指紋（它會真的跑 `RunFrame`：`UpdateCameras` 的 `Res<CameraTargetSizes>` 靠 `camera` 的準備函式放初值才讀得到，第 4.2 節）；其餘用合成的登記檔（`UnitsTests.cpp`、`UnitRegistryTests.cmake`）。寫死 Registry 指紋或 golden 的測試都用自己組的 Registry，不經預設單位——**預期不受影響**；實作時跑全部測試確認，有受影響的照 §14.4 處理（只換固定值、判定不變時事後報）。

## 7. Architecture Delta（總宣告第 11 節的差分）

總宣告第 11 節已含「新的列 `transform`、`camera`」。本批的差分：

1. **Feature**：Object_Connect（正交相機、固定解析度、滑鼠換算）、R2b 的 Extract、PvP Client（回滾復活）。
2. **現在的問題**：gyo2 沒有姿態與相機；`GlobalTransform` 與相機的推導值要有唯一的寫者。
3. **變的邊界**：(a) `camera` 持有「目標大小」的副本 `CameraTargetSizes`（擁有者是 `window` 與 render；第 4.2 節的 State Authority 分析）——**要使用者同意**；(b) `engine/core/math` 多一個函式 `MakeOrthographic`（Math 的公開介面加法，不改既有函式）；(c) `docs/cross-component-rules.md` 多一列（owner＝transform）。
4. **影響的 Module／Product／Tool**：新 Module 兩個；math；`DefaultUnitsTests`（預設帶多兩個 Module，第 6.4 節）。
5. **依賴方向**：`camera` → `transform` → core／ecs／orchestration；沒有 Module 依賴它們（R2b 的 `render` 之後只取型別）。
6. **Ownership**：兩個 Module 的 owner 是架構（Engine）；`CameraTargetSizes` 的寫者在 X1 是作者與測試，之後的膠水 W 定。
7. **為什麼沒有更小的改法**：不帶 `CameraTargetSizes` 就得選 (a)（等 W、`camera` 依賴 `window`）或 (c)（來源側做不了滑鼠換算）；不加 `MakeOrthographic` 就得在 `camera` 裡自寫 clip 慣例（Math 的「一個運算一個實作」）；不加 hook，回滾復活的實體沒有 `GlobalTransform`（PvP 的角色會消失）。

## 8. Design Invariants 的草案（寫進兩頁 owner 文件；最終文字在合併前核准）

**`transform`**（`target/v2/transform.zh-Hant.md`）：
- **TF-1 職責**：只有 `Transform`、`GlobalTransform`、補 `GlobalTransform` 的初始值與 hook、同步（X2 起加傳播）的 System 與公開的 set。不做：父子關係（Kernel，KR）、回滾／複製／權威的登記（作者與網路那一側）、內插、畫面。
- **TF-2 `GlobalTransform` 是推導值，`Transform` 是作者的值**：`GlobalTransform` 只由 `transform` 寫（初始值、hook、同步／傳播的 System）；雜湊不寫內容；`transform` 不把它登記回滾、權威、複製。判定不讀它（CCR，第 4.5 節）。`Transform` 的雜湊逐欄、`==` 逐位元，兩者一致（回滾等價要的）。守衛：T-7、T-8、T-9；`tf-hash-global-nonempty`、`tf-hash-transform-drops-scale`、`tf-eq-float-compare`。
- **TF-3 沒有父時等於本身**：實體原本沒有 `GlobalTransform` 時，`Transform` 出現的那個同步點、`Restore` 放回之後，`GlobalTransform` 就在、而且 `matrix == ComposeTRS(transform)`；每次 `kPropagateSet` 跑完，沒有父、帶兩者的實體都相等（原本留著舊 `GlobalTransform` 的，到這時才對）。守衛：T-1、T-2、T-5、T-6、T-7。
- **TF-4 依賴**：只依賴 base、kernel、orchestration、math。守衛：`gyo_expect_links`（C-6）。
- **Replacement Criteria**：RC-TF 1 傳播超過 R2b 宣告的幀時間預算（計測）；RC-TF 2 要第三條路才保得住「有 `Transform` 就有 `GlobalTransform`」（初始值、hook 之外）→ 停下交使用者（候選：Kernel 加照 Bevy 的必要元件）；RC-TF 3 判定需要讀 `GlobalTransform`（它不再是推導值）→ 重新設計。

**`camera`**（`target/v2/camera.zh-Hant.md`）：
- **CM-1 職責**：相機的設定、目標、投影、2D／3D 標記、推導值、目標大小的副本、更新的 System、座標換算。不做：畫面、可見性、清除色、視窗。
- **CM-2 依賴**：只依賴 `transform` 與 base、kernel、orchestration、math；不依賴 `window`、`input`、render。守衛：`gyo_expect_links`（C-6）。
- **CM-3 `ComputedCamera` 是推導值**：只由初始值與 `UpdateCameras` 寫；值（逐位元）相同不寫；目標大小未知或視區大小 0 時 `valid == false`；雜湊不寫內容。守衛：C-2、C-3、C-4；`cm-valid-without-size`、`cm-writes-when-equal`。
- **CM-4 慣例**：投影照 math.md（左手、深度 0～1）；座標換算的位置是目標的邏輯像素、含視區偏移、左上原點、+Y 向下（同 Bevy）。守衛：C-2、C-5、M-1。
- **CM-5 目標大小是副本**：`CameraTargetSizes` 只用來算投影；`camera` 不拿它做別的判斷、不寫回擁有者；雜湊不寫內容。守衛：審查。
- **Replacement Criteria**：RC-CM 1 要多視窗、或膠水 System 必須知道 `camera` 以外的視窗細節才能填（副本開始要判斷）→ 重比第 4.2 節 (a)；RC-CM 2 副本與擁有者不同步造成畫面錯（例：改大小的那一幀投影錯了、作者要補）→ 停下交使用者。

## 9. 卡片照 L17 判的（不是使用者的話；可以推翻），與回報主會話

照 L17 判的：
1. 「有 `Transform` 就有 `GlobalTransform`」用初始值＋OnAdd hook 兩條路（第 4.1 節；Bevy 是 require 一條）。
2. `ComputedCamera` 另立元件（第 5 節）。
3. 縮放模式只做 `WindowSize`、`Fixed`；不做 Custom 投影（薄起步）。
4. `Camera2d`、`Camera3d` 在 X1（Bevy 在 `bevy_camera`；2D、3D 都要，任務卡的指示）；R2b 可以往它們加欄。
5. 正交投影放 `engine/core/math`（第 4.4 節）。
6. `CameraTargetSizes` 與 `ComputedCamera` 的雜湊不寫內容（照 `Peer`、`AssetView` 的先例）。
7. 欄名 `nearClip`／`farClip`、`order` 用 `std::int32_t`。
8. `Transform` 的 `==` 逐位元（檢查 B1）；`ComputedCamera` 的比較逐位元、不公開 `==`；相機的設定元件不給 `==`（沒有點名的 Feature 要回滾相機）。
9. 座標換算的座標系與錯誤的判法照 Bevy（檢查 S1、S2）。

回報主會話（不在本批）：
- **R2b**：2D 的前後方向由第 13 節的決定加上 R2b 的排序一起定；R2b 的排序要和 `Default2d` 的深度方向一致。
- **W**：把視窗大小寫進 `CameraTargetSizes` 的膠水放哪（第 4.2 節的候選）；新建或替換的 World 一開始沒有目標大小，膠水要在第一輪就寫一次（併進 W 的「新 World 的初值」那一題）。
- **內插的「上一步的姿態」**：本線沒有點名的 Feature，X1 不做；第一個要內插的產品（PvP Client）回來時定（作者的元件或 `transform` 給，Bevy 生態是 `bevy_transform_interpolation` 一類的第三方）。
- **圖書管理員**：CCR 新列（第 4.5 節）由 X1 加，之後的維護照規則頁。

## 10. 實作的步驟（動工前核准之後）

1. `MakeOrthographic` 與 M-1（先紅後綠）、math.md 一行。
2. `transform`：登記列、CMake、元件、雜湊、初始值、hook、`SyncTransforms`、選項；T-1～T-8 先紅後綠。
3. `camera`：登記列、CMake、元件與標記、初始值的鏈、`CameraTargetSizes`、`UpdateCameras`、換算；C-1～C-6 先紅後綠。
4. 兩頁 owner 文件（DI、RC）、CCR 列、說明頁（`system-contract-page` 不改；兩頁自己寫用法）。
5. 突變清單、跑新的突變、清單的敵對式檢查（Opus／high，同模型寫明）。
6. 全部測試、L1 三平台（draft PR 手動 dispatch）。
7. 設計審查（和實作者不同模型；Fable 用量不足期間 Opus／high、寫明同模型）；合併前核准頁；dev_log 同一變更。

## 11. 判定（先宣告，結果出來之後不放寬）

- 第 6.1 節全部的測試在 L1 三平台（linux-x64、windows-x64、macos-arm64）通過；既有測試不退步。
- 第 6.3 節最後收斂的突變全部 killed（只算預期的失敗）。
- 沒有效能判定（第 3 節：沒有出處的預算）。

## 12. 範囲の限界

- 父子（X2）、可見性與畫面（R2b）、視窗的膠水（W）不在本批，第 1 節的 Object_Connect 例子只有 X1 那一半能測。
- 本線沒有 Feature 實際用到 `camera`（KeepTool 不用；總宣告第 15 節）：驗收只靠測試，第一個實際的使用者是 R2b 與 Object_Connect。
- 透視投影的 `tan` 跨平台不保證逐位元相同（math.md）：投影的測試用容差。
- 準備函式的 `World::Insert` 放的 `Transform` 不補 `GlobalTransform`（Kernel 的設計，說明頁第 17 節已寫）。

## 13. 要使用者定的

動工前核准時請一併同意第 7 節第 3 項 (a)：`camera` 持有目標大小的副本（AGENTS §3）。第 4.2 節的 (b) 是總宣告的傾向、任務卡已指定；其餘照 Bevy 或照 Kernel、Math 的現行規則推得出（第 9 節列為卡片判的，可以推翻）。

### 第 1 題：2D 相機的預設，z 大的在前面還是 z 小的在前面

**例子**：Object_Connect 的背景 z=0、血管 z=1、節點 z=2。照 (乙)，節點蓋在血管上、血管蓋在背景上（數字大的在上面，同 Bevy 的教學與範例）；照 (甲)，要反過來寫成背景 z=2、節點 z=0。

**機制**：GYO 的座標是左手、相機看 +Z、深度 0（近）～1（遠）不反向（math.md）。Bevy 是右手、看 −Z，而且正交投影本身把 near／far 對調成反向深度（`projection.rs:637-647`），所以 Bevy 的 2D「z 大的在前面」。`Camera2d` 的預設投影 `Default2d()` 是 X1 定的基盤契約，近遠怎麼取決定方向；R2b 的排序跟著它。

| 做法 | 內容 | 代價 |
|---|---|---|
| (甲) 照 GYO 的座標：near −1000、far 1000 | z 小的離相機近、在前面（同 Unity 的 2D 相機看 +Z） | 和 3D 相機同一個慣例；和 Bevy 的 2D 作者體驗相反，從 Bevy 的教學搬過來的寫法 z 要反 |
| **(乙) 照 Bevy 的作者體驗：near 1000、far −1000（對調）** ←傾向 | z 大的在前面；相機在原點看得到 z −1000～1000 | 2D 相機的深度方向和 3D 相機相反（同一個相機混畫 3D 模型時要注意，R2b 的說明頁寫）；程式膠水沒有，只是預設的兩個數字 |

**傾向 (乙)**：使用者定 Bevy 為主參考的理由是不當縫合怪、作者面對的語意少衝突；「數字大的在上面」也是 Godot 的 `z_index`、Unity 的 sorting order 共通的直覺。(甲) 的好處（和 3D 相機同一個慣例）只在同一個相機混畫 3D 時看得到，本線沒有這樣的 Feature。**證據**：Bevy `projection.rs:637-647`、`:771-791`；math.md Conventions；Object_Connect 的畫的先後（總宣告第 6.4 節）。

## 14. 修訂 0 的敵對式檢查：處理對照

檢查者 Opus／high（作者同模型；Fable 用量不足）；報告的要點在本節，原文未另存檔（未保存）。

| # | 指摘 | 處置 |
|---|---|---|
| B1 | float 沒有 `==`：回滾的登記撞斷言、`ComputedCamera` 的「不同才寫」沒定義、float `==` 會讓 ±0 破 R-2 | 採用：`Transform` 逐位元 `==`、`ComputedCamera` 逐位元比較（第 4.1、4.2 節、TF-2、CM-3、T-7、`tf-eq-float-compare`） |
| S1 | 換算的座標系和 Bevy 不同（Bevy 是目標座標含視區偏移）、沒列；黑邊沒測 | 採用：改照 Bevy；C-5 加黑邊案、`cm-viewport-offset-ignored` |
| S2 | 透視相機後面的點是 `PastFarPlane` 不是 `PastNearPlane` | 採用：照 Bevy 的判法、C-5 分正交與透視寫 |
| S3 | TF-3 說太滿（已有舊 `GlobalTransform` 時兩條路都不蓋） | 採用：TF-3 加條件、第 4.1 節寫後果 |
| S4 | `CameraTargetSizes` 逐項雜湊與先例相反、各機器不同會誤報 | 採用：不寫內容 |
| S5 | `CameraTargetSizes` 的初值沒寫；新 World 是空的 | 採用：準備函式「沒有才放」；新 World 的事交 W（第 9 節） |
| S6 | 退掉的選項沒列（只用 System 補；`camera` 的 hook 插 `Transform`）；`World::Insert` 的實體一直沒有 `GlobalTransform` | 採用：兩個選項與理由寫進第 4.1、4.2 節；後果寫明 |
| S7 | 2D 前後方向不只能由 R2b 定，`Default2d` 對調就能照 Bevy | 採用：改成第 13 節第 1 題交使用者 |
| S8 | 測試只用 origin 0.5、scale 1、id 0；夾進與 y 翻轉沒有突變 | 採用：C-2、C-3、C-5 加案，加 5 個突變 |
| S9 | hook 對 R-2 的影響沒有測試 | 採用：T-7（回滾官方策略的等價案） |
| nit | `&FromTransform` 寫法不一致；預設值字面；`ScalingFixed` 沒初值；`Viewport` 有號；沒列的偏離（縮放比改變、重算時機）；`Changed<GlobalTransform>` 的語意；T-7 與 T-1 重複；「不在回滾登記」的對應；`MakeOrthographic` 對總宣告的差分；剪切的說明；NaN 每幀寫；T-5、T-6 依賴 `Restore` 用現在的 stamp | 採用：各處改（T-1 併讀檔式、T-9、第 5 節三列、X1-3、X1-4）。最後一條：原語 5（指定 stamp 寫回）沒有實作，等它實作時回頭看 T-5、T-6，記在這裡 |

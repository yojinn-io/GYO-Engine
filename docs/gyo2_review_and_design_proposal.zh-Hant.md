# GYO-Engine gyo2 架構審查與設計提案

- 狀態：**提案，尚未核准**
- 日期：2026-11-23
- 對象：`gyo2` 分支，提交 `0bb64a2`

## 0. 摘要

gyo2 的依賴方向乾淨，但引擎沒有任何真實產品在用，ECS 核心的實作撐不住規模，也沒有視窗、輸入和渲染。

這份提案做三件事：

1. 保留核心已經做對的三個決定，只改實作方式和邊界。這三個決定是：按 id 排序的儲存、結構變更延後到 sync point、編譯期推導存取宣告。
2. 把 orchestration 和 authority / rollback / net 裡靠隱式約定維持的東西，改成由型別和擁有者表達。
3. 補上平台與渲染：以 SDL3 為唯一後端元件，2D 和 3D 共用一條 SDL_GPU 路徑，文字用 FreeType。

## 1. 範圍與依據

這份文件來自一次**只看 C++ 程式碼**的外部審查，以及之後幾輪設計討論。審查時刻意不讀專案文檔、不讀守衛腳本、不遵循既有規範。

| 項目 | 狀態 |
| --- | --- |
| 審查對象 | `engine/` 與 `examples/`，約 4.1 萬行；去掉註釋約 3.0 萬行 |
| 細讀 | kernel、orchestration、authority、net 的模組層、math 的慣例 |
| 只看宣告與結構 | rollback、asset、logging、audio、os、io、json |
| 沒做 | 建置、執行測試、效能量測 |
| 憑記憶、未對照標頭 | SDL3、SDL_GPU、FreeType 的 API 細節 |

兩點請讀者留意：

- **效能判斷沒有量測數據**，全部來自程式碼的複雜度分析。
- **`docs/` 裡已有 `rendering_architecture.zh-Hant.md` 以及數個產品的文檔，我沒有讀。** 本提案的渲染與 Plugin 設計可能與它們衝突，採用前需要有人對照。

## 2. 現況審查

### 2.1 做得好的地方

- **依賴無環且單向**。從 `#include` 重建的依賴圖是 core → ecs → orchestration → modules，沒有反向邊。
- **System 簽名在編譯期驗證**。存取宣告從參數型別推導，寫錯直接 `static_assert`（`engine/ecs/kernel/include/engine/kernel/SystemTraits.hpp`）。
- **外部依賴被關住**。yyjson、fast_float 只出現在 `json/src/adapters/`，平台 API 只出現在 `core/os/src/sys/`。
- **部分模組有清楚的「純函式庫 + World 接合層」拆分**：`asset` / `world_asset`、`logging` / `world_logging`、`time` / `world_time`。
- **sync point 已經把延後的結構變更分階段排序後套用**（`World::ApplySync`），套用順序與提交順序無關。

### 2.2 問題

按嚴重度排列。

**問題 1：引擎沒有產品在用。**
`games/` 和 `tools/` 只有 `.gitkeep`，`examples/` 只有一個 99 行的程式。測試約 5.6 萬行，比引擎還多。`math`、`audio`、`state_machine`、`save_version`、`trace`、`world_asset`、`world_logging` 除了測試以外沒有呼叫者。API 只被自己寫的測試驗證過。

**問題 2：ECS 的儲存與查詢實作成本過高。**

- 每個 component 是一條 `std::vector<Cell<T>>`，id、兩個 stamp 和值綁在同一個結構裡（`detail/Storage.hpp:148`）。刪除、對既有實體補 component、多區段發號下的插入，都要搬移整段。
- `Query::ForEach`（`Params.hpp:326`）先配置一份實體清單，再對每個實體的每個 term 做一次二分搜尋。
- 每次 System 呼叫、每個參數都線性掃描型別表（`Registry.cpp:166` 的 `FindType`）。
- 訊息緩衝滿了之後，每次 `Append` 都 `erase(begin())`。
- 每筆命令一次堆配置（`unique_ptr<ValueBox>`）。

**問題 3：World 的封裝是假的。**

- `World.hpp` 有 29 個 `friend` 宣告，`WorldExecutor` 直接改寫 `state_`、`runStack_`、`stamp_`。
- 測試後門在公開標頭裡：`TypeKeyCollisionProbe`、`SchedulerShuffleProbe`（`Registry.hpp:361`、`:370`）。
- 執行期靠字串查找：`RunPlan` 每次建構一個 `std::string`（`Executor.cpp:215`），再用字串比對找 plan、schedule、hook queue 和 mount。

**問題 4：公開 API 裡有做不到的功能。**

- `ExecutorKind::Parallel` 可以宣告，啟動時被拒絕（`Registry.cpp:814`）。
- `Extract<Read<T>>` 沒有任何存取方法（`Params.hpp:1323`）。
- `PeerAuthorityStrategy`、`PeerRollbackStrategy` 只回報「not implemented yet」。

**問題 5：程式碼離開文檔就讀不懂。**
錯誤碼是 `StartupCode::DE14`、`OrchestrationCode::OE3`，規則是字串 `"W-8"`、`"DI-KS 18"`。錯誤有三條通道並存：`Result`、永遠開啟且會 abort 的 `GYO_ASSERT`、orchestration 攔截的例外。

**問題 6：Orchestration 用隱式約定代替型別。**

- `Module` 和 `Plugin` 是內容相同的空子類。兩者的差別是一條依賴規則，靠執行期比對字串名稱檢查（`Product.cpp:185`）。目前沒有任何 Plugin 實作。
- `ModuleBuffer` 和 `OptionBag` 是以型別為鍵的 `shared_ptr<void>` 袋子，模組之間靠「你有沒有先放東西」耦合。
- 模組的 `Prepare` lambda 以引用捕獲 per-world buffer 裡的物件（`NetModule.cpp:220`）。它不懸空，只因為 `Product::Build` 的區域變數活得夠久。
- `NetModule::Check` 呼叫 `service->Bind(...)`（`NetModule.cpp:281`），檢查階段有副作用。
- `Product.cpp` 有 932 行，`ReplaceWorldSteps`（`:688`）約 170 行，內含多層 try/catch 和手動回滾。
- 引擎層出現遊戲概念 `gameInit`（`Product.hpp:59`）。

**問題 7：authority / rollback / net 把同一套骨架抄了三遍。**
三個模組各有 `XRegistrations`、`XRegistry`、`XEntry`、`XStrategy` 和一串 `*Context`。`DataOps` 和 `DataKind` 各有兩份。`Peer` 為了滿足 `Hashable` 約束寫了一個空的 `HashValue`（`NetModule.hpp:47`）。`net` 把傳輸層和 ECS 複製層放在同一個 target。

**問題 8：零散項目。**

- 命名空間不一致：kernel 是 `GYO::Kernel`，其餘是 `GYO::Engine::*`。
- 自製 `Base::Span` 和 `std::span` 並存。檔案存取有 `os/Files` 和 `io/fs` 兩條路。`IFileSystem` 只有一個實作。
- `Kernel::Hasher` 其實是把值序列化進 `vector<byte>`。
- `AuthoritySourcesOptions` 帶 `repoCommit`、`repositoryRoot`，卻放在執行期模組。
- assert handler、log handler、trace receiver 都是行程級全域掛鉤。

## 3. 設計原則

- **保留排序儲存。** 走訪順序只取決於 id，和插入歷史無關，rollback、重模擬和一致性摘要都靠這個性質。因此不改用 sparse-set 或 archetype。
- **保留 64 位單調 id，不加 generation。** `RangeAllotter` 單調發號、不重用。
- **接口先於 Plugin。** 引擎的概念和介面由 Module 定義，Plugin 只用某個外部元件去實作它。
- **一個外部元件只對應一個 Plugin。**
- **不為沒有被點名的需求設計。** 每一節都列出「第一個切片不做」的項目。

## 4. Kernel

### 4.1 儲存

每條 column 拆成平行陣列，仍然按 id 嚴格遞增：

```cpp
template <class T>
struct Column {
    std::vector<EntityId>    ids;      // 二分搜尋只碰這條
    std::vector<ChangeStamp> added;
    std::vector<ChangeStamp> changed;
    std::vector<T>           values;
};
```

- **批次套用**：`ApplySync` 已經把 despawn、remove、spawn 各自排序，只是逐筆套用。改成每條受影響的 column 在一個階段內做一次合併壓縮，成本從「筆數 × N」降到「N + 筆數」。hook 的觸發順序不變。
- **訊息緩衝**改環形。
- **命令緩衝**改成每個 System 一塊 bump arena，sync 後整塊重置。

### 4.2 查詢

- **merge-join**：所有 column 都按 id 排序，多個 term 的交集就是多路游標同步前進。不需要逐實體二分搜尋，也不需要先配置實體清單。
- `ForEach` 直接在游標上走。`Entities()` 只留給真的需要清單的呼叫者。
- component、resource、message 的 id 在 executor `Attach()` 時解析一次，存進每個 System 的狀態。

### 4.3 邊界

| 物件 | 擁有 | 誰能改 |
| --- | --- | --- |
| `WorldData` | columns、resources、messages、存活清單 | 只透過 `WorldAccess` |
| `RunState` | run stack、hook 佇列、stamp、sync epoch、目前呼叫 | 只有 executor |
| `AccessVerifier` | 快照與比對 | 只在開啟驗證時存在 |

handle（`Query`、`Res`、`Commands`）只依賴內部介面 `WorldAccess`，取代現在的 friend 清單。

### 4.4 註冊 API 改用具型別的 handle

```cpp
ScheduleId update = r.Schedule({.name = "Update", .stages = {...}});
SystemId   recv   = r.AddSystem<&ReceiveNet>({.in = update.Stage(first)});
r.AddSystem<&ApplyReplication>({.in = update.Stage(first), .after = {recv}});
```

- 名稱在凍結時換成整數 id，字串只留給診斷。
- 拼錯名字變成編譯錯誤，`RunPlan` 裡的字串比對全部消失。
- 代價：跨 unit 引用別人的 system，必須透過對方公開的 handle。這是比現在更強的耦合宣告。

### 4.5 公開 API 清理

- `ExecutorKind::Parallel`、`Extract<>`、兩個 `Peer*Strategy` 拿掉，做出來再加。
- 兩個 `*Probe` 移到只有測試 target 看得到的 `kernel/testing/` 標頭。
- 命名空間統一成 `GYO::Engine::Kernel`。

### 4.6 錯誤模型

- 枚舉值用描述性名字（例如 `ParallelExecutorUnsupported`）。規格編號放進一張 `code → rule` 的資料表，只在輸出訊息時附上。
- 三條通道的用途明文規定：
  - `Result`：呼叫者可以處理的失敗。
  - `GYO_ASSERT`：程式設計錯誤，立即終止。
  - 例外：只允許 `bad_alloc` 穿過，orchestration 是唯一攔截點。

## 5. Orchestration

- **一個 `Unit` 介面。** `Configure` / `Build` / `Finish` / `Check` 對 Module 和 Plugin 完全相同。種類是註冊資料的屬性（現有的 `RegisterRow::kind`），不需要兩個空類別。
- **取代 `ModuleBuffer`。** 每個 unit 的 `Build` 回傳自己的建置產物，由 `WorldInstance` 持有，活到該 World 被替換為止。其他 unit 用 `setup.Require<T>()` 取得引用；缺少時回報啟動錯誤，不回傳 `nullptr`。
- **階段各司其職**：`Build`（註冊）→ `Finish` → `Check`（全 const，無副作用）→ `Bind`（新階段，放 `service->Bind` 這類會改狀態的動作）。
- **拆開 `Product`**：

| 物件 | 職責 |
| --- | --- |
| `ProductBuilder` | 驗證 unit 清單和依賴，產生 `Product` |
| `WorldInstance` | 一個 World 的 registry、executor、frame 清單、carry 清單、unit 產物 |
| `WorldReplacement` | 替換交易：建構時取出 carry，`Commit()` 換入，解構時若未提交就還回去 |
| `Product` | 主迴圈和失敗狀態 |

- **改名**：`gameInit` 改成 `startup`。

## 6. Module 與 Plugin

兩個概念都保留，區分改由建置圖保證。

| | Module | Plugin |
| --- | --- | --- |
| 位置 | `engine/` 內 | `engine/` 外，頂層 `plugins/` |
| 內容 | 自己的介面、自己維護 | 接上一個第三方元件的 adapter |
| 誰能依賴它 | 其他 Module、Plugin、產品 | 只有產品 |
| 它能依賴誰 | core、kernel、orchestration、其他 Module | 以上全部，加上外部函式庫 |

engine 的 target 不連結 `plugins/` 下的 target，所以 Module 想 include Plugin 的標頭會直接編不過。`Product.cpp:185` 的執行期檢查可以刪掉。

**進 `plugins/` 的判斷標準**，三條都成立：

1. 它包的是第三方維護的東西。
2. 拿掉它之後，引擎的無頭建置和所有引擎測試照樣通過。
3. 只有產品決定要不要用它。

依此標準：

- yyjson、fast_float 留在 `json` 模組內。`logging` 和 `asset` 依賴 `json`，拿掉引擎就不成立。
- `core/os` 是我們自己的平台層，不是 Plugin。
- 手寫、無外部依賴的實作（`WavLoader`、`TextLoader`、`UdpLink`、`MemoryLink`）留在 Module。
- rollback 和 authority 的策略是我們自己的邏輯，屬於 Module 或產品。

**現有程式碼已留好、Plugin 可以接的接口：**

| 接口 | 位置 | 可接的 Plugin |
| --- | --- | --- |
| `ProductSetup::InstallRunner` | orchestration | 由 `world_window` 使用，見第 9 節 |
| `Audio::Mixer::Render` | audio | 音訊裝置輸出 |
| `Asset::AssetLoader` concept | asset | 圖片、模型、壓縮音訊的解碼 |
| `Net::Link` | net | 平台中繼、NAT 穿透 SDK |
| `Base::TraceReceiver`、`Logging::Sink` | base、logging | 外部 profiler、當機回報 |
| `IO::FS::IFileSystem` | io | 封裝檔案系統 |

## 7. 模擬資料描述符

一個型別只描述一次，authority、rollback、net 各自讀同一張表：

```cpp
sim.Component<Position>("Position", {
    .rollback  = Rollback::Snapshot,     // 或 Derived / None
    .replicate = Replicate::ToClients,   // 或 None
    .digest    = Digest::Include,        // 或 Exclude
});
```

- 描述符帶一份共用的 ops：id 查詢、hash、相等、編解碼、快照。
- 三個模組只保留自己的策略邏輯。
- 「不進摘要」是一個選項，不必再寫空的 `HashValue`。
- `net` 拆成 `net`（傳輸：Connection、Codec、Link、Handshake，不 include kernel）和 `world_net`（複製系統與 module）。

這一節是從介面形狀推出來的，rollback 的內部我沒有逐行讀。它牽涉 wire 格式和指紋，需要產品的實際資料型別來驗證。

## 8. 目標目錄結構

```
third_party/     yyjson/  fast_float/  doctest/  sdl/  freetype/     各自釘住 commit
engine/
  core/          base  os  io  time  threads  math
  ecs/           kernel（含原 entity_store）
  orchestration/
  modules/
    libs/        json  logging  asset  net  audio  input  window  render  text
    world/       world_asset  world_logging  world_time  world_net
                 world_window  world_input  world_render  world_text
    sim/         sim_data  authority  rollback  entity_ids
plugins/         sdl/  freetype/  （圖片解碼）/  （glTF）/
tools/           shader_compiler/
```

- `libs/` 不碰 kernel。`world/` 把對應的 lib 接進 World。
- `state_machine`、`save_version` 等到有產品用到再放回來。

## 9. 平台接口：視窗、輸入、音訊、主迴圈

### 9.1 一幀的流程

用的是程式碼裡已有的接縫：`WorldSetup::ExchangeIn` / `ExchangeOut`（在 frame schedule 前後、World 處於 Ready 時被呼叫）和 `ProductSetup::InstallRunner`。

```
world_window 的主迴圈（阻塞式 while）
  │
  ├─ 1. Pump         Host::Pump → Plugin 把 SDL 事件翻譯成引擎事件
  ├─ 2. ExchangeIn   world_input / world_window 把事件寫進 World
  ├─ 3. Schedules    遊戲 System 讀輸入，寫 DrawList
  └─ 4. ExchangeOut  world_render 展開 DrawList → FramePacket → Backend::Present
```

主迴圈屬於 `world_window`。Plugin 不安裝 Runner。

### 9.2 接口

```cpp
namespace GYO::Engine::Window {
class Host {
public:
    virtual ~Host() = default;
    [[nodiscard]] virtual Result<void, WindowError> Open(const WindowDesc&) = 0;
    virtual void Pump(EventSink& sink) = 0;            // 只在主執行緒
    [[nodiscard]] virtual WindowState State() const = 0; // 邏輯尺寸、像素尺寸、縮放比、焦點、最小化
    virtual void Apply(const WindowRequest&) = 0;       // 標題、全螢幕、游標、文字輸入開關與輸入框矩形
};
}

namespace GYO::Engine::Audio {
class Device {
public:
    virtual ~Device() = default;
    [[nodiscard]] virtual Result<void, AudioError> Start(Mixer& mixer) = 0;
    virtual void Stop() = 0;
};
}
```

### 9.3 輸入

- `libs/input` 只是詞彙：`Key`、`MouseButton`、`GamepadButton`、`InputEvent`、`InputState`。
- 文字輸入事件：`TextCommitted{utf8}`、`TextComposing{utf8, cursor, length}`。
- 裝置輸入**不是**模擬資料。遊戲從 `InputState` 取樣出自己的指令結構，那個結構才進 rollback 和複製。

### 9.4 限制

- 第一階段一個視窗只綁一個 World，和 `NetService::Bind` 的單一 World 限制相同。
- 視窗、事件、繪圖全在主執行緒。綁主執行緒的 handle 用 kernel 現有的 `ResourceOptions::threadBound`。

## 10. 渲染

### 10.1 慣例

`core/math` 已經選好，渲染直接採用：

| 項目 | 慣例 | 依據 |
| --- | --- | --- |
| 矩陣 | 行主序儲存、行向量、`M * v` | `MakeTranslation` 把位移放在 12–14 |
| 3D 座標 | 左手系、+Z 向前、Y 向上 | `MakePerspective` 讓 w 取 +z |
| 裁切空間深度 | 0 到 1 | 同上 |
| 2D 螢幕空間 | 左上原點、y 向下、單位像素 | `MakeOrthographicPixels` |
| 顏色 | 貼圖存 sRGB，光照在線性空間算 | `ColorSpace.hpp` |

### 10.2 分層

```
遊戲 System ──寫──▶ DrawList        高階圖元（2D 和 3D）
                      │  libs/render 的 Renderer：剔除、排序、批次、選 pipeline
                      ▼
                   FramePacket      GPU 層級的命令：上傳、pass、繪製
                      │
                      ▼
                Render::Backend     plugins/sdl 用 SDL_GPU 實作
```

- 剔除、排序、批次是我們自己的邏輯，留在引擎。後端只重播 `FramePacket`。
- `FramePacket` 是不引用 World 的獨立值。錄製用的假後端可以直接比對它，引擎測試不需要 GPU。
- `DrawList` 是立即式 API：
  - 2D：sprite、矩形、線段，帶 layer 和排序鍵。
  - 3D：`Mesh(mesh, material, worldMatrix)`、相機、方向光。
- 遊戲的 System 走訪自己的 component，把東西推進 `DrawList`。

### 10.3 後端接口

```cpp
namespace GYO::Engine::Render {
class Backend {
public:
    virtual ~Backend() = default;
    [[nodiscard]] virtual BackendCaps Caps() const = 0;   // shader 格式、貼圖上限、呈現模式

    [[nodiscard]] virtual Result<BufferId,   RenderError> CreateBuffer(const BufferDesc&) = 0;
    [[nodiscard]] virtual Result<TextureId,  RenderError> CreateTexture(const TextureDesc&) = 0;
    [[nodiscard]] virtual Result<SamplerId,  RenderError> CreateSampler(const SamplerDesc&) = 0;
    [[nodiscard]] virtual Result<ShaderId,   RenderError> CreateShader(const ShaderBlob&) = 0;
    [[nodiscard]] virtual Result<PipelineId, RenderError> CreatePipeline(const PipelineDesc&) = 0;
    virtual void Destroy(ResourceId) = 0;

    [[nodiscard]] virtual Result<FrameStatus, RenderError> Present(const FramePacket&) = 0;
};
}
```

`FramePacket` 依序包含：

1. **上傳**：這一幀新增或變動的 buffer 和貼圖資料，含貼圖的局部更新。
2. **Pass 清單**：目標、清除值、視口。
3. **繪製命令**：pipeline、頂點和索引 buffer、貼圖與取樣器綁定、uniform 資料塊、索引範圍。

接口只取第一個切片用得到的子集。`PipelineDesc` 和 `TextureDesc` 先留 `sampleCount`、`usage` 欄位但只接受預設值。

### 10.4 一幀的組成

| 順序 | Pass | 深度 | 排序 |
| --- | --- | --- | --- |
| 1 | Scene3D 不透明 | 測試並寫入 | 先按 pipeline 和材質，再由近到遠 |
| 2 | Scene3D 半透明 | 只測試 | 由遠到近 |
| 3 | Overlay2D | 關閉 | layer、排序鍵、提交順序 |

- 純 2D 遊戲只有第 3 個 pass。文字和 UI 也在第 3 個。
- backbuffer 和它的深度緩衝由後端擁有，視窗改變大小時由後端重建。
- 3D 剔除用 mesh 的 AABB 對相機視錐，`math` 已有 `Frustum`、`Aabb`、`Intersection`。

### 10.5 資源的權威

- 貼圖、mesh、pipeline「應該存在哪些」由引擎決定，後端是快取。
- 後端回報 `ResourcesLost` 時，引擎從資產庫重建。代價是 CPU 端要保留一份資料。
- 快取放在產品範圍。`Product::ReplaceWorld` 換掉 World 時不需要重傳。

### 10.6 資產

| 型別 | 內容 |
| --- | --- |
| `Image` | RGBA8 或 R8 像素、尺寸、色彩空間；非預乘 alpha、由上到下 |
| `Mesh` | 頂點資料、索引、`VertexLayout` 描述、AABB |
| `Material` | 內建 pipeline 種類、參數、貼圖引用 |
| `ShaderBundle` | 見第 11 節 |
| `Font` | 字型檔的原始位元組 |

- 頂點佈局用資料描述，pipeline 以佈局為鍵。日後加骨骼動畫不必改 `Mesh` 的形狀。
- 模型格式用 glTF。圖片和 glTF 的解碼各是一個 loader Plugin，跑在工作執行緒上，和 SDL 無關。

### 10.7 與 rollback 的關係

`DrawList`、`InputState` 不是模擬資料，不進摘要、不複製、不回滾。繪製 System 掛在 frame schedule；重模擬只重跑 fixed schedule，所以不會重複執行。

### 10.8 內建 pipeline

`Sprite2D`、`AlphaMaskSprite2D`、`MeshUnlit`、`MeshLit`（一盞方向光加環境光）。

### 10.9 第一個切片不做

陰影、PBR、後處理、多重取樣、render-to-texture、instancing、骨骼動畫、壓縮貼圖、世界空間的 2D、自訂材質 shader、標準 `Transform` component 與場景階層。

## 11. Shader 管線

SDL_GPU 要各圖形 API 的原生格式：Vulkan 要 SPIR-V，D3D12 要 DXIL，Metal 要 MSL。這件事完全移到執行期之外。

| 部分 | 內容 |
| --- | --- |
| Data Contract | shader bundle 檔：每種目標格式的位元碼、反射資訊（頂點輸入、uniform 佈局、貼圖槽位）、原始碼雜湊、格式版本、工具版本 |
| Tool | `tools/shader_compiler`，讀 HLSL，產出 bundle。外部編譯器只存在於這個工具裡 |
| 執行期 | `ShaderLoader` 是普通的 `AssetLoader`。後端依 `Caps()` 挑對應的位元碼。執行期不編譯 shader |

**bundle 提交進版本庫**（已定）：

- 原始碼放 `engine/modules/libs/render/shaders/src/*.hlsl`，產物放旁邊的 `bundles/`。
- 一個純 C++ 測試重算原始碼雜湊，和 bundle 裡記錄的比對。改了 HLSL 卻沒重新產生，測試就失敗。這個測試不需要 shader 編譯器。
- 內建 bundle 在建置時嵌進 `render` 的二進位。
- 只有改 shader 的人需要裝編譯工具鏈。

## 12. 文字

### 12.1 做法

執行期按需點陣化，配動態圖集。不採用預先烘焙圖集，因為中日韓字集太大，玩家輸入的文字也無法預知。

### 12.2 分層

```
libs/text ──▶ libs/render          text 在 render 之上，render 不認識字型
    │
    └── 接口 Text::GlyphSource  ◀── plugins/freetype 實作
```

- `libs/render` 提供兩樣與字型無關的通用設施：
  - `DynamicTexture`：CPU 端的像素頁加髒矩形，由 `FramePacket` 的上傳段同步。
  - `AlphaMaskSprite2D` pipeline：取樣單通道遮罩，乘上頂點色。
- `libs/text` 擁有所有文字邏輯：UTF-8 解碼、字型後備鏈、字形快取、圖集裝箱、排版。產出的是推進 `DrawList` 的遮罩 sprite。
- Plugin 只做字形層級的事：

```cpp
namespace GYO::Engine::Text {
class GlyphSource {
public:
    virtual ~GlyphSource() = default;
    [[nodiscard]] virtual Result<FaceId, TextError> OpenFace(std::span<const std::byte> fontFile) = 0;
    virtual void CloseFace(FaceId) = 0;
    [[nodiscard]] virtual FaceMetrics Metrics(FaceId, float pixelSize) const = 0;
    [[nodiscard]] virtual std::optional<GlyphIndex> GlyphOf(FaceId, char32_t codepoint) const = 0;
    [[nodiscard]] virtual float Kerning(FaceId, float pixelSize, GlyphIndex left, GlyphIndex right) const = 0;
    [[nodiscard]] virtual Result<GlyphBitmap, TextError> Rasterize(FaceId, float pixelSize, GlyphIndex) = 0;  // 8 位元覆蓋率
};
}
```

### 12.3 為什麼是 FreeType（已定）

SDL_ttf 自帶排版和圖集引擎，底層仍是 FreeType。用它等於把排版和圖集的所有權交給外部元件，同時還是得帶 FreeType。直接接 FreeType 少一層。

代價：排版要自己寫。FreeType 的授權要求在產品文件中註明使用。

### 12.4 遊戲端的 API

```cpp
Text::Draw(drawList, textView, {.fonts = uiFonts, .pixelSize = 18, .position = {x, y},
                                .color = white, .layer = Layer::Ui, .maxWidth = 320, .align = Align::Left}, utf8);
Text::Measure(textView, style, utf8);
```

- `textView` 是 `world_text` 提供的資源，綁主執行緒。
- `FontSet` 是有序的字型清單，用來做後備（拉丁字型在前、中日韓字型在後）。

### 12.5 範圍（語言：拉丁與中日韓，已定）

| 做 | 不做 |
| --- | --- |
| 逐字對應字形、字距調整、明確換行 | 複雜字形組合（阿拉伯文、泰文、印度系文字） |
| 依寬度自動換行：空白處斷行，中日韓字元之間可斷 | 雙向文字 |
| 左、中、右對齊 | 彩色表情符號 |
| 字型後備鏈 | 距離場字型、3D 世界中的文字 |
| 依視窗縮放比決定點陣化尺寸 | 文字編輯元件 |
| 輸入法事件的傳遞（第 9.3 節） | |

日後需要複雜字形組合時，加一個 `Shaper` 接口由另一個 Plugin 實作，不影響 `GlyphSource`。

### 12.6 測試

引擎測試用假的 `GlyphSource`（每個字形回傳固定大小的方塊）。排版和圖集邏輯用 `FramePacket` 比對來測，不需要 FreeType。

## 13. SDL Plugin 與接入契約

### 13.1 結構

```
third_party/sdl/                        釘住 commit 的 FetchContent，和 yyjson 同一個做法
plugins/sdl/
  include/plugins/sdl/SdlPlugin.hpp     唯一公開標頭：SdlPlugin、SdlOptions，不含任何 SDL 型別
  src/
    SdlPlugin.cpp                       Configure：初始化、建立各接口實作、放進產品範圍
    SdlRuntime.cpp                      SDL_Init / SDL_Quit、版本檢查、hint、錯誤轉換
    window/SdlWindowHost.cpp            實作 Window::Host
    input/KeyMap.cpp  SdlGamepads.cpp   scancode 對照表
    render/SdlGpuBackend.cpp            實作 Render::Backend
    render/SdlGpuResources.cpp          id → SDL 物件的對照表、generation 檢查
    render/SdlGpuFormats.cpp            引擎的格式與狀態列舉 → SDL 列舉
    audio/SdlAudioDevice.cpp            實作 Audio::Device，回呼裡呼叫 Mixer::Render
```

視窗由 `SdlWindowHost` 建立，GPU 裝置向它認領 swapchain。這兩個實作在 Plugin 內部互相認識，對引擎仍是兩個獨立的接口。

### 13.2 從 SDL 用什麼、不用什麼

用：video、events、gamepad、audio、GPU。

不用，而且要主動關掉：

| SDL 的功能 | 引擎的對應 | 處理方式 |
| --- | --- | --- |
| `SDL_main` | 產品自己的 `main` | 定義 `SDL_MAIN_HANDLED` |
| 訊號處理 | `core/os` 的 Shutdown | 用 hint 關掉 |
| `SDL_Log` | `logging` | 把輸出函式導向引擎的 logger |
| 執行緒、計時器、檔案系統 | `threads`、`time`、`io` | 不呼叫 |
| 事件時間戳 | `Time::NowNs` | Pump 時用引擎時鐘重新蓋章 |
| `SDL_Renderer` | 無 | 不使用，所有繪圖走 SDL_GPU |

### 13.3 Plugin 必須遵守

1. SDL 的標頭、型別、列舉值只出現在 `plugins/sdl/src/`。
2. 不 include kernel，不碰 World，不註冊 System，只實作接口。
3. 不安裝 Runner，不呼叫 `exit`。
4. 初始化失敗用 `setup.Report(code, message)` 回報，不 abort。`SDL_Quit` 掛在 `OnDestroy`，在所有接口實作解構之後。
5. 視窗、事件、繪圖呼叫全在呼叫 `Configure` 的那條執行緒上。音訊回呼是唯一進入引擎的 SDL 執行緒，只准呼叫 `Mixer::Render`。
6. 事件完整翻譯。無法翻譯的鍵給 `Key::Unknown`，其餘無法翻譯的事件丟棄並計數。
7. 錯誤用帶錯誤碼的 `Result`，`SDL_GetError` 的文字只放進 detail。
8. `Present` 嚴格按命令順序執行。除了已建立的資源以外，兩次 `Present` 之間不保留狀態。
9. 回報 `ResourcesLost` 之後，所有舊的資源 id 視為失效。
10. 後端不解讀 shader 內容，只依反射資訊綁定。槽位規則的轉換完全在 `plugins/sdl` 內。

### 13.4 引擎保證

1. 每幀先 `Pump` 一次，再跑 schedule，最後每個視窗最多 `Present` 一次。
2. World 設了渲染選項但產品沒有後端時，`Check` 回報啟動錯誤，和 `NetModule` 的 NT-E1 同一個做法。
3. `libs/render`、`libs/window`、`libs/text` 各附一個錄製用或假的實作（類比 `MemoryLink`），引擎測試永遠不需要 SDL 和 FreeType。

## 14. 相容性與代價

- **SDL 版本**：釘住一個 commit，靜態連結。升級的改動範圍必須只有 `third_party/sdl` 和 `plugins/sdl`；要改到別處就表示契約被破壞。不支援 SDL2。
- **平台**：桌面三平台，和 `core/os` 目前支援的一致。**不支援網頁**（已定）。行動平台不在範圍內，主迴圈因此寫成普通的阻塞迴圈。
- **硬體門檻**：就我所知 SDL_GPU 只有 Vulkan、D3D12、Metal 三個後端，沒有 OpenGL。不支援這些 API 的舊顯示卡無法執行。
- **沒有軟體渲染**：引擎測試比對 `FramePacket`。像素級測試只能在有 GPU 的機器上跑，或在 Linux CI 上裝軟體 Vulkan 驅動。
- **無頭**：伺服器產品不帶 Plugin。Plugin 自己的測試用 SDL 的 dummy 影像與音訊驅動。
- **視窗最小化**：`Present` 回 `Skipped`，改由引擎的 `Time::Waiter` 限速。
- **裝置重置**：走 `ResourcesLost` 路徑。

## 15. 實作順序

兩條線。B 線的前三步不依賴 A 線，也不需要任何外部函式庫。

### A 線：既有程式碼

1. **儲存與查詢**（第 4.1、4.2 節）。對外介面幾乎不變，風險最低。
2. **id 化與公開 API 清理**（第 4.3 到 4.6 節）。破壞性改動，趁沒有產品依賴時做。
3. **Orchestration**（第 5 節）。先做 `WorldReplacement` 和 `Check` 無副作用，再換掉 `ModuleBuffer`。
4. **模擬資料描述符**（第 7 節）。需要產品的實際資料型別來驗證，最後做。

### B 線：平台與渲染

1. `libs/render` 的型別、`Renderer`、錄製後端、`DynamicTexture`，和 `world_render`。
2. shader bundle 格式、`tools/shader_compiler`、四個內建 shader、雜湊檢查測試。
3. `libs/text` 和 `world_text`，搭配假的 `GlyphSource`。`libs/window`、`libs/input` 和對應的 `world_*`。
4. `plugins/sdl`：音訊，然後視窗和輸入（含文字輸入）。
5. `SdlGpuBackend`：先 `Sprite2D`，再 `AlphaMaskSprite2D`，最後兩條 mesh pipeline。
6. `plugins/freetype`。
7. 圖片和 glTF 的 loader Plugin。

### 兩條線的關係

- B 線第 1、2 步完成前不要動第 5 步，否則後端會倒過來決定接口的形狀。
- A2 最好排在 B 線的 `world_*` 模組之前。這些模組會註冊 System；如果 A2 晚做，它們要從字串排序遷移到 handle 一次。
- 整件事的目的是讓第一個真實產品切片能跑起來：有視窗、能收輸入、能出聲、能畫 2D 和 3D、能顯示文字。產品切片應該和 B 線同步開始，用它來否決接口。

## 16. 已定決策

| 決策 | 結論 |
| --- | --- |
| 後端元件 | SDL3，單一 `plugins/sdl` |
| 維度 | 2D 和 3D 都要，共用 SDL_GPU 一條路徑 |
| 網頁平台 | 不需要 |
| 編譯好的 shader bundle | 提交進版本庫，以原始碼雜湊檢查是否過期 |
| 文字 | 進第一個切片 |
| 字型元件 | FreeType，獨立的 `plugins/freetype` |
| 語言範圍 | 拉丁與中日韓 |

## 17. 未驗證事項與風險

- **既有文檔未對照。** `docs/rendering_architecture.zh-Hant.md` 和其他專案文檔沒有讀，本提案可能與已記錄的決定衝突。
- **SDL_GPU、SDL3 事件、FreeType 的細節憑記憶**，包括綁定槽位規則、swapchain 取得的執行緒限制、支援的圖形 API、裁切空間慣例。實作時要對照釘住版本的標頭。
- **效能主張沒有量測。** 第 4 節的收益是複雜度分析的結論。動手前應先量現況，定出預算。
- **字形同步點陣化可能卡頓。** 一幀內出現大量新的中日韓字會在送出畫面時集中點陣化。先不預防，量到再處理（限制每幀新字形數量，或預熱常用字）。
- **CPU 端保留資源副本的記憶體成本**（第 10.5 節）沒有估算。
- **Plugin 的能力邊界只由 SDL 和 FreeType 兩個實例推出。** 第三個 Plugin 出現時可能需要調整。
- **SDL 的連結方式**預設靜態，沒有再確認。若要讓發行平台替換 SDL，需改為動態連結並加執行期版本檢查。

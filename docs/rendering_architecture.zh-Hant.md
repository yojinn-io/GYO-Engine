# GYO 渲染架構與跨平台建置

[日本語](rendering_architecture.ja.md) · [整體架構](architecture.md)

兩種語言的章節編號、程式識別字與圖示保持一致。本文描述本次實作的責任邊界；實際驗證狀態見第 10 節。

<a id="r01"></a>
## 1. 先理解一幀怎樣變成畫面

CPU 執行遊戲規則，決定畫什麼；GPU 平行處理頂點和畫面片段。Shader 是 GPU 執行的小程式，並不是整個渲染器。

```mermaid
flowchart LR
    A[CPU: 遊戲狀態] --> B[RenderQueue: 物件與攝影機]
    B --> C[Renderer: 整理繪製與 pass]
    C --> D[IRenderDevice: GPU 命令]
    D --> E[Vertex shader: 頂點投影]
    E --> F[Rasterization: 三角形變成片段]
    F --> G[Fragment shader: 貼圖與顏色]
    G --> H[深度測試與混合]
    H --> I[畫面]
```

例如 Mark-23：模型資料先由 CPU 取樣骨骼動畫、執行蒙皮，再更新既有 GPU mesh 的頂點。Vertex shader 把這些頂點轉到螢幕座標；fragment shader 依 UV 取貼圖顏色。深度測試決定手指和槍身誰在前方，背面剔除決定三角形哪一面可見。這些固定管線設定仍由 C++ 的 pipeline 描述管理。

這張圖是概念順序；GPU 可以提早執行部分深度測試。現階段模型蒙皮在 CPU，沒有 GPU 骨骼動畫或完整 PBR 光照。

<a id="r02"></a>
## 2. 同一份 HLSL，產生不同平台格式

HLSL 是原始語言，DXIL、SPIR-V、Metallib 是不同後端接受的編譯產物。GYO 維護共用 HLSL，在建置時產生目標平台需要的檔案；遊戲啟動時不編譯 HLSL。

```mermaid
flowchart TD
    A[共用 HLSL 與 RasterAbi.hlsli] --> B[原生 host shader 工具]
    B --> C[DXIL: Direct3D 12]
    B --> D[SPIR-V: Vulkan]
    B --> E[MSL 中間碼]
    E --> F[Apple metal 與 metallib]
    F --> G[Metallib: Metal]
    C --> H[Bundle: manifest 與編譯產物]
    D --> H
    G --> H
```

工具使用固定版本的 DXC、SDL_shadercross、SPIRV-Cross；版本與校驗碼位於 `tools/shader_pipeline/cmake/Dependencies.cmake`。Windows／Linux 使用固定 DXC 發行包，macOS 原生建置固定 DXC 原始碼，再透過所選 Xcode 的工具產生 Metallib。MSL 是建置中間產物，不能把產出 MSL 誤記為已完成 Metal GPU 驗證。

Shader 工具屬於建置主機（host），遊戲屬於執行目標（target）。原生建置會在 build tree 的 `host-tools` 建置工具；交叉編譯必須提供可在主機執行的 `GYO_SHADER_TOOL_EXECUTABLE`。下載、工具、中間碼與 bundle 都留在 build tree。已部署遊戲不需要 DXC、shadercross 或 Xcode。

<a id="r03"></a>
## 3. 責任分配與資料流

```mermaid
flowchart TD
    A[Object_FPS: 遊戲規則與呈現] --> B[RenderQueue 與 MaterialDesc]
    B --> C[GYO Renderer]
    D[Bundle manifest 與 shader bytes] --> E[GYO ShaderLibrary]
    E --> C
    C --> F[PreparedFrame 與 IRenderDevice]
    F --> G[SDL_GPU adapter]
    G --> H[D3D12 / Vulkan / Metal]
```

| 元件 | 負責 | 不負責 |
|---|---|---|
| Object_FPS | 決定物件、相機、材質 shader ID、武器動作 | SDL_GPU 命令、shader 格式選擇 |
| `RenderQueue`／`MaterialDesc` | 中立的幾何、材質與畫面提交資料 | Native handle 或檔案編譯 |
| `ShaderLibrary` | 原子載入 bundle、驗證 manifest、保存不可變 shader bytes | 建立 GPU pipeline 或紋理 |
| `Renderer` | 攝影機與矩陣、世界／武器／後處理／HUD 順序、pipeline 選用與畫面資源 | 平台驅動細節或遊戲命中規則 |
| `IRenderDevice` | GPU 資源、pipeline、frame 取得／提交、讀回契約 | 理解槍械或場景政策 |
| SDL_GPU adapter | 把中立契約轉成 SDL_GPU 操作與同步 | 組裝遊戲 pass 或內嵌 HLSL 原始碼 |

這是可替換的 C++ API 邊界，尚未承諾跨編譯器的動態外掛 ABI。`ShaderLibrary` 的 CPU 資料可以共用，每個 device 的 GPU handle、pipeline 與在途資源必須分開。

<a id="r04"></a>
## 4. Shader 資料契約：`gyo.raster.v1`

「同一份 HLSL 可編譯」還不夠：CPU 傳的位元組必須正好符合 shader 的讀法。

| 項目 | 契約 |
|---|---|
| 座標 | 左手系，+Y 向上，+Z 向前；矩陣以 row-major 儲存，使用 row vector |
| 頂點 | `Vertex3D`：位置 float3、UV float2；順序及 offset 由固定 vertex layout 定義 |
| 一般頂點 uniform | `worldViewProjection`：64 bytes，vertex 階段邏輯 uniform 槽 0 |
| 一般 fragment 資源 | 一張貼圖與一個 sampler，fragment 階段邏輯槽 0 |
| 一般 fragment uniform | tint、UV scale/offset、alpha 參數：三個 float4，共 48 bytes，fragment 邏輯 uniform 槽 0 |
| 場景後處理 uniform | exposure 與 gamma 位於一個 float4，共 16 bytes，fragment 邏輯 uniform 槽 0 |
| 顏色 | 線性 RGB 運算，顏色貼圖使用 sRGB 解碼；最後由 sRGB render target 做顯示轉換 |

`render/shaders/common/RasterAbi.hlsli` 是共用 HLSL 宣告。編譯工具以反射資料檢查已支援介面的資源數量與 uniform 大小，再把契約與格式資料寫入 manifest。版本不符、缺少程式、格式不完整、重複 ID 或非法路徑都應明確失敗，不能默默改用別的 shader。

共用宣告使用邏輯資源巨集，不在公開 ABI 寫死 SDL register／descriptor 規則。工具的 SDL_GPU profile 注入實際映射：vertex uniform 為 `b0/space1`，fragment texture／sampler 為 `t0/space2`／`s0/space2`，fragment uniform 為 `b0/space3`；再處理 SPIR-V／Metal 的對應。更換裝置 adapter 時，物理綁定是工具與 adapter 的責任。

本輪只開放既有 `unlit` 與 `scene_post` 介面。自訂 shader 可以在既有資料契約內改變顏色計算；增加新的頂點屬性、額外 texture 或新的 uniform 意義時，必須一起擴充中立介面、反射驗證與 Renderer 的傳值程式。

<a id="r05"></a>
## 5. 內建與遊戲 shader 的所有權

| Shader ID | 所有者 | 用途 |
|---|---|---|
| `builtin/unlit` | GYO Render | 網格與 sprite 的貼圖、tint、alpha cutoff |
| `builtin/scene_post` | GYO Render | 場景曝光與 gamma |
| `game/object_fps/channel_swap` | Object_FPS | 使用同一 `unlit` 契約的自訂 shader 驗證樣本 |

內建來源位於 `render/shaders/builtin/`，遊戲來源位於 `apps/object_fps/shaders/`。各自有 `bundle.json` 建置規格和獨立 runtime bundle。遊戲增加 shader ID 不需要把遊戲名稱或檔案路徑放進 GYO 後端。

`MaterialDesc` 按 shader ID 指定程式，並提供 texture、tint 與 sampler；shader ID 不是 C++ 函式指標，也不授權 shader 改變遊戲狀態。資料設定只能選擇引擎已支援的介面能力。

例如 `apps/object_fps/shaders/bundle.json` 明確指定各階段入口；省略時預設 `main`：

```json
{
  "version": 1,
  "include_directory": "../../../render/shaders/common",
  "programs": [{
    "id": "game/object_fps/channel_swap",
    "interface": "unlit",
    "vertex": "../../../render/shaders/builtin/unlit.vert.hlsl",
    "vertex_entrypoint": "main",
    "fragment": "channel_swap.frag.hlsl",
    "fragment_entrypoint": "main"
  }]
}
```

原始 HLSL 入口名稱與產物入口不一定相同；例如轉成 MSL 後可能改名。工具把產物的真實入口寫入 runtime manifest，Renderer/device 讀取該值。

<a id="r06"></a>
## 6. 一幀與資源生命週期

```text
World meshes + Scene sprites
        ↓ 保留場景顏色，重新清除深度
ViewModel meshes
        ↓ 場景曝光／gamma
Scene post process
        ↓ 不受場景曝光影響
Overlay / HUD → Present
```

世界與武器使用各自相機；武器 pass 清除深度後，手與槍之間仍正常遮擋。`Renderer` 產生 `PreparedFrame`，device 只執行其中明確描述的 pass、附件與 draw。

`AcquireFrame` 在視窗最小化時可以沒有 frame；成功取得的 token 必須經 `SubmitFrame` 或 `AbandonFrame` 恰好消耗一次。提交使用的 CPU 資料在呼叫期間被消費；後端負責保護尚在 GPU 執行的資源。畫面大小改變時重建對應 render target，清理時先解除 Renderer 的 device 資源，再銷毀 device。

動畫每幀更新固定大小的頂點內容，保留 mesh handle 與 index topology。診斷讀回只在明確請求時等待 GPU，日常呈現不進行這種同步。Scene capture 是曝光／gamma／Overlay 之前的場景資料，並非最終螢幕截圖。

<a id="r07"></a>
## 7. CMake 自動選擇與明確覆寫

專案選取來自 `config/engine/projects.csv` 的 `enabled` 與目標平台欄位。`GYO_APPS=AUTO` 使用此集合，空值不建 apps，名稱或分號清單只能縮小集合，不能繞過停用設定。需求由各 app 的 `CMakeLists.txt` 宣告，根建置先收集，再建立能力與 app targets；詳見[建置設計](architecture.md#build-project-management)。選到需要 GPU 的 app 時，`GYO_RENDER_DEVICE=NONE` 是配置錯誤。

| 設定 | 值 | 意義 |
|---|---|---|
| `GYO_RENDER_DEVICE` | `AUTO`／`SDL_GPU`／`NONE` | 選擇 device 實作；`NONE` 用於無 GPU 的建置 |
| `GYO_GPU_DRIVER` | `AUTO`／`D3D12`／`VULKAN`／`METAL` | 部署程式的預設驅動政策 |
| `GYO_SHADER_BUNDLE` | `AUTO` 或格式清單 | 建置並部署哪些 shader 格式，例如 `"DXIL;SPIRV"` |
| `GYO_SHADER_TOOL_EXECUTABLE` | host executable 的絕對路徑 | 覆寫原生 shader 工具，供交叉編譯等情況使用 |

| 目標平台 | `AUTO` 的 shader bundle | 可用驅動 |
|---|---|---|
| Windows | DXIL＋SPIR-V | D3D12、Vulkan |
| Linux | SPIR-V | Vulkan |
| macOS | Metallib | Metal |

CMake 使用 `CMAKE_SYSTEM_NAME` 判斷**目標**平台；preset 的 `hostSystemName` 條件只用來限制 CI 原生建置入口。執行時 `--gpu-driver d3d12|vulkan|metal` 可明確覆寫驅動。AUTO 依可提供的完整 shader 格式與 SDL_GPU 可用驅動選擇；強制指定不可用的驅動必須失敗，不以另一個驅動掩蓋問題。

Windows 的 AUTO bundle 同時包含 DXIL／SPIR-V，因此可以驗證兩種驅動。僅包 DXIL 就不能退回 Vulkan。錯誤的平台／驅動／格式組合會在配置或啟動時被拒絕；CMake 無法預先保證玩家 GPU 與驅動可用。

```sh
# 原生建置：Windows 需先進入 x64 MSVC 開發環境；另需 Ninja。
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --install build/dev --prefix /absolute/path/to/stage

# 無遊戲、無 SDL adapter、無 FBX adapter 的核心建置
cmake --preset core
cmake --build --preset core
ctest --preset core
```

`dev` test preset 只執行 `cpu|shader` 標籤；GPU 測試需在具有可用顯示與 GPU 的環境另行執行。`ci-windows`、`ci-linux`、`ci-macos` 明確固定 CI 的編譯器／架構入口。

CLion 可沿用現有 MSVC CMake profile，執行 **Reload CMake Project**，再選擇並執行 `gyo_object_fps` target。原生 shader 工具的子建置沿用該 profile 選定的編譯器與 Ninja 路徑；不必為了散佈 DLL 的偵測重建 IDE profile。

macOS CI 套件的 deployment target 維持 **13.3**，遊戲與 Metallib 使用相同值。最低執行系統版本與 SDK 是否提供某個 API 是兩項獨立條件：Xcode 16.4 實際沒有浮點 `std::from_chars` overload，因此 CSV 改用明確的十進位／指數語法檢查，再以 `std::locale::classic()` 解析為 float。解析保留有限值、範圍、非零值下溢與完整字串檢查，並涵蓋可表示的次正規數；系統語系不改變資料中的小數點。

Ubuntu 需安裝 `libxtst-dev`，供 SDL 預設啟用的 XTest 偵測使用；CI 以 `pkg-config --modversion xtst` 確認套件。離線 shader 工具的 SDL 同時停用 video 與 dialog，避免 macOS 靜態連結引用未編入的 Cocoa 視窗符號。遊戲的 SDL 設定獨立於 host 工具。[SDL Linux 相依套件](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies)

離線 host 工具還明確設定 `SDL_UNIX_CONSOLE_BUILD=ON`。它刻意不使用 X11／Wayland video，必須告知 SDL 這是 console build，否則 Linux configure 會將缺少視訊後端視為錯誤。此設定只作用於 shader host 的 SDL；遊戲仍建置自己的圖形後端。

<a id="r08"></a>
## 8. App 隔離部署與套件契約

每個 app × 平台 CI job 使用獨立建置目錄，只選該 app 並關閉 Editor。app 自己維護 executable、內容、shader、安裝與額外驗收；共用部署處理其實際需要的動態庫、RPATH 與 MSVC CRT。

```text
stage/
├─ bin/<app executable> + app-owned content + Windows DLLs
├─ lib/                         Linux/macOS non-system libraries
└─ share/gyo/apps/<name>/manifest.json
```

CMake 在建置目錄生成 manifest，app 不必人工維護另一份清單。它包含必要檔案、強制的安裝後 startup 命令，以及依 quick／release、平台與 GPU 需求選取的額外驗收。共用 runner 從安裝包以外的工作目錄執行；失敗、逾時或缺少必要證據都阻擋產包。資產與 shader 相對 executable 定位，不以來源目錄掩蓋缺檔。Object_FPS 的內容負向與玩法檢查見[app 驗收指南](../apps/object_fps/docs/acceptance.zh-Hant.md)。

每個選中組合輸出 `gyo-<name>-<platform>.tar.gz` 與 `.tar.gz.sha256`，archive 根為 `gyo-<name>`。Tar 保留 Unix 執行權限。這是原生驗收套件，不含 macOS 簽章／公證或跨 Linux 發行版相容性承諾；目標系統仍提供圖形驅動與系統庫。封裝記錄 app、目標平台、來源 SHA、profile 及驗收證據。Release 從同一 commit 的 CSV 重建預期集合並核對完整證據，quick 或普通本機包不能冒充 release。

Windows Release／RelWithDebInfo 的 `cmake/GyoMsvcRuntime.cmake` 依選定編譯器找對應可散佈 DLL，放在 `bin/`。`GYO_MSVC_REDIST_DIR` 可指定根目錄；缺少 DLL 不阻擋開發配置／編譯，但 release install 明確失敗。不散佈 Debug CRT，Windows 10+ 提供 UCRT。使用者不需要 Visual Studio、shader 編譯器或 SDK。

CI 使用 `dumpbin` 檢查 Windows VC runtime 引用，`ldd` 檢查 Linux 套件庫解析，`otool` 檢查 macOS install name／RPATH。工具只屬於建置機；Windows 本機驗收可用 `--dumpbin /absolute/path/to/dumpbin.exe` 指定位置。

<a id="r09"></a>
## 9. 通用 CI、Release 與 app 驗收

三平台固定建置並測試引擎與 UI editor。固定來源 SHA 的 CSV 另生成 app × 已啟用平台矩陣，沿用 Windows x64／MSVC、Linux x64／GCC 14、macOS ARM64／Xcode 16.4；每個 app job 只有自己的套件，Editor 不隨 app 發行。

```text
固定來源 SHA → CMake 解析 CSV → 三平台引擎／Editor baseline
                              → app × 平台獨立建置／安裝
                              → manifest 的 quick／release 驗收
                              → archive + SHA-256 + Actions artifacts
全部必要檢查成功 → Prepare Release 建立 tag／Draft／預期附件
                → 使用者 Publish release（不重建）
```

沒有 app 的平台仍測試引擎與工具，不產包；全部停用的一般 CI 仍可成功，Prepare Release 前置檢查則拒絕空產物。無 GPU／shader 需求的 app 不執行對應步驟。必要 GPU 驗收缺少驅動、失敗或逾時會失敗，不能以跳過冒充成功。

`cross-platform.yml` 的 push／PR／手動執行使用 quick；`prepare-release.yml` 使用完整 release。兩者共用 `build-and-validate.yml` 並傳入固定 SHA。Release 驗證 app、平台、SHA、profile、必要檔案與完整證據，拒絕缺包、多包或 quick 證據。相同版本 Draft 重試保留已驗證附件及手寫說明；tag 必須指向相同 commit，已公開版本不可覆寫。推 tag 或 Publish release 不重新建置。操作與恢復見[發佈指南](releasing.zh-Hant.md)。

Object_FPS 的 startup／gameplay／缺檔與 Linux quick 一項、release 八項 GPU 驗收由 app 自己註冊；命令、套件內容及完整實機程序已移至[Object_FPS 驗收指南](../apps/object_fps/docs/acceptance.zh-Hant.md)。共享 runner 不保存這些遊戲專屬規則。Windows／macOS hosted runner 不宣稱實體 GPU 通過，Linux Lavapipe 是軟體 Vulkan。各 app 的硬體與互動驗收須另有實機證據。

<a id="r10"></a>
## 10. 驗證狀態、限制與參考

以下保留 2026-09-16 舊建置配置的歷史證據，未用來宣稱本次 CSV／app 矩陣通過。舊 Object_FPS `NONE` 配置已由新的必要 GPU 宣告取代；目前選中此 app 並設定 `NONE` 會配置失敗。

截至 2026-09-16，本次實作的本機證據如下：

| 檢查 | 狀態 |
|---|---|
| Windows 無遊戲／無選用 adapter 的核心配置 | CTest 6/6 通過，包含真實 CMake 平台政策與中立 render 測試 |
| Windows 原生 shader host 工具 | CLion profile 的 CTest 3/3 通過，包含反射、拒絕非法 shader、增量相依、重建與子工具鏈傳遞 |
| 由主建置自動建立 host shader 工具 | 已通過 |
| Windows 完整 RelWithDebInfo 建置與 CPU/headless | 建置通過；CTest 12/12 通過，其中 headless 包含 1,169 個 assertions |
| Windows RelWithDebInfo 的 engine GPU 數值 smoke | 明確指定 D3D12 與 Vulkan 均 exit 0；實際使用 `direct3d12`＋DXIL、`vulkan`＋SPIR-V |
| Windows Object_FPS GPU 驗收 | D3D12 與 Vulkan 各 8/8 案例通過，兩次 helper 均 exit 0 並保存 summary |
| Windows 部署驗收 | 完整套件通過；缺少 common、內建 shader、遊戲 shader 的 3 項負向案例正確失敗；app-local CRT 相依檢查通過 |
| Object_FPS 的 `NONE` 配置 | 指定不存在的 shader 工具仍可建置；43 個 target 無 SDL_GPU、shader host 或 bundle；headless／render 2/2 通過 |
| CLion 既有 MSVC profile | CMake 4.1.2＋Ninja＋VS18 cl 14.51 Release，原 `cmake-build-msvc` 配置、host 工具自動建置、Object_FPS／UI editor 建置通過 |
| CLion 回歸與部署 | CMake 政策／CRT 2/2、headless／render／package／CRT 4/4、遊戲／選單／shader GPU 3/3 通過（`direct3d12`＋DXIL）；release 安裝、隔離套件及 3 項缺檔、CRT 檢查通過 |
| 本次 CI 相容性修正的本機回歸 | Windows Object_FPS 與 host 工具重建通過；CMake／render／headless／package／GPU 合計 7/7、host 3/3；CSV 專項在 MSVC 與 MinGW GCC 各 192 assertions 通過 |
| GitHub Windows x64 | 先前完整 CI 通過；最新 quick run 35097049659 的建置、安裝與 startup 通過，但 MSVC 覆寫 `PLATFORM=x64` 使 archive 參數失敗。已改用 `GYO_PACKAGE_PLATFORM`，待新 CI |
| GitHub Linux／macOS core-only | 兩平台各 7/7 通過 |
| GitHub Linux 建置與 quick smoke | run 35097049659 成功：建置、部署版 startup、Lavapipe 單項 shader 渲染與封裝；先前 XTest／console-build 問題已解決 |
| GitHub macOS 完整建置 | run 35093916457 通過：CPU/headless/shader 13/13、host shader 3/3、core-only 7/7、Metallib、安裝與部署；最新 quick run 的 macOS 結果尚未確認 |
| Linux／macOS 實體 GPU 驗收 | 尚待各平台實機執行；Linux 軟體 Vulkan 的 quick smoke 已通過 |
| 新 smoke helper 的案例與錯誤路徑 | 八項真實 subprocess 測試通過：quick 僅渲染一項、非零退出、逾時、無法啟動、未建立 GPU 卻退出 0、driver／shader 不符及結果彙整 |
| 完整 CI helper 與 workflow 靜態檢查 | 49 項 helper 測試通過，包含發佈政策、封裝內容與 smoke；actionlint 1.7.12 與 `git diff --check` 通過 |
| 新 helper 對既有 Windows 套件 | `--suite ci` 在本機 D3D12／DXIL 與 Vulkan／SPIR-V 各 3/3 通過，`--suite quick` 在 Vulkan 1/1 通過；使用既有套件，並非新 hosted 流程的通過證據 |
| 新 Windows 執行檔的 CPU 回歸 | MSVC 重建通過；startup smoke、gameplay headless smoke、package、domain headless 共 4/4 通過。設定無效 SDL video／GPU driver，確認這些路徑不依賴視窗或 GPU |
| 新 Windows 安裝套件驗收 | 從 TEMP 工作目錄執行 startup／gameplay smoke 通過；部署正向與缺少 common／builtin shader／game shader 三項負向結果正確；Vulkan `full` 8/8、D3D12 `quick` 1/1 通過 |
| CI／Release 流程 | 先前 quick 路徑已有 hosted 記錄；本次共用驗證流程與 GUI Prepare Release／Draft 建立尚待新的 GitHub 執行驗證 |

既有 Windows Mark-23／跳躍驗收不等同於新渲染架構驗收。請以此次建置日誌、Actions summary 與實機輸出作為具體通過證據。

首輪 hosted 記錄為 [Actions run 35079389797](https://github.com/yojinn-io/GYO-Engine/actions/runs/35079389797)。該次執行的是修正前 commit；重新執行舊 job 仍會使用舊版本，需將修正提交並推送後，由新 commit 觸發驗收。

後續記錄為 [Actions run 35093916457](https://github.com/yojinn-io/GYO-Engine/actions/runs/35093916457)：Windows 與 [macOS job](https://github.com/yojinn-io/GYO-Engine/actions/runs/35093916457/job/104786488372) 成功，macOS 包含 Metallib、完整測試與部署檢查；Linux 日誌確認離線 SDL 的 console-build 設定缺失。這兩次都是新 quick／Release 流程落地前的歷史證據；macOS 實體 GPU 仍未驗證。

最新 [quick run 35097049659](https://github.com/yojinn-io/GYO-Engine/actions/runs/35097049659) 已驗證 Linux startup、shader 渲染與封裝。Windows 的 MSVC 初始化會設定工具鏈用的 `Platform=x64`，而 Windows 環境變數不分大小寫；因此套件識別改用 `GYO_PACKAGE_PLATFORM=windows-x64`，避免污染報告與封裝參數。

上述 engine GPU smoke 實際涵蓋 frame 生命週期、動態 mesh、ViewModel 深度、剔除、UV、矩陣投影、alpha 混合與色彩處理。Object_FPS 在兩種驅動、三種寬高比、各 7 組攝影機情境中，槍口／tracer 標記中心差為 0.0000 像素，數值參考投影最大誤差為 0.3823 像素。

Windows 平台變數修正已在本機初始化 MSVC 後，直接執行 workflow 的 startup／archive 區塊驗證；封裝名稱、內嵌平台資料與 SHA-256 均通過。記錄位於 `build/ci-platform-env-check/result.log`，GitHub 修正後結果仍待新 commit 的 CI。

本機建置與套件位於 `build/render-cross-vs18`、其 `stage` 子目錄。可重跑 `ctest --test-dir build/render-cross-vs18 -C RelWithDebInfo -L cpu --output-on-failure`，GPU 與部署使用第 8、9 節 helper。此次證據保存於：

- `build/render-cross-vs18/cpu-tests.xml`，同目錄的 `gpu-d3d12/summary.json`、`gpu-vulkan/summary.json`；GPU 子目錄同時保留各案例日誌與 BMP。
- `build/render-cross-vs18/package-logs/package-*.log`：完整套件、三項缺檔與 CRT 相依檢查。
- `build/render-cross-vs18/validation-none-{configure,build,ctest,restore}.log`：停用後端及恢復 AUTO 的檢查。
- `build/render-neutral-vs18/test-results.xml`、`build/shader-host-vs18/Testing/Temporary/LastTest.log`：獨立核心與 host 工具驗收。
- `build/clion-configure.log`、`build/clion-build.log`、`build/clion-regression-tests.log`、`build/clion-host-tests.log`、`build/clion-gpu-tests.xml`、`build/clion-package-logs/`：既有 CLion profile 的修正驗收。
- `build/ci-portability-build.log`、`build/ci-portability-tests.xml`、`build/ci-portability-host-tests.xml`：本次 Linux／macOS CI 相容性修正的 Windows 回歸；原失敗日誌在 `build/ci-35079389797-logs/`。
- `build/ci-35093916457-linux.log`：後續 hosted Linux 的離線 SDL console-build 配置錯誤。
- `build/ci-35093916457-macos.log`：後續 hosted macOS 的建置、Metallib、13/13＋3/3＋7/7 測試與部署成功記錄。
- `build/ci-smoke-support/windows-{d3d12,vulkan}/summary.json`：新 helper 對既有 Windows 部署包的三項診斷。
- `build/ci-smoke-support/windows-vulkan-quick/summary.json`：新 quick suite 的單項 shader 渲染。
- `build/ci-release-smoke-tests.log`、`build/ci-release-smoke-tests.xml`：新 Windows 執行檔的 4/4 CPU 回歸；新安裝套件位於 `build/ci-release-stage/`。
- `build/ci-release-gpu/vulkan/summary.json`、`build/ci-release-gpu/d3d12-quick/summary.json`：新安裝套件的本機 Vulkan 八項與 D3D12 單項渲染驗收，尚不代表 Linux Lavapipe 通過。

本輪範圍包含共用 HLSL、離線 shader bundle、中立 Renderer/device 邊界與三平台建置流程。Compute shader、GPU skinning、PBR、任意 material graph、shader hot reload、完整 RenderGraph、device-loss 自動復原，以及二進位外掛 ABI 均未納入。

- [SDL_GPU device 與 shader 格式](https://wiki.libsdl.org/SDL3/SDL_CreateGPUDevice)
- [SDL_GPU shader 資源綁定規則](https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader)
- [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross)
- [DXC](https://github.com/microsoft/DirectXShaderCompiler)
- [CMake target system](https://cmake.org/cmake/help/latest/variable/CMAKE_SYSTEM_NAME.html)
- [GitHub hosted runner 規格](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)
- [Apple Metal Toolchain 安裝](https://developer.apple.com/documentation/xcode/downloading-and-installing-additional-xcode-components)

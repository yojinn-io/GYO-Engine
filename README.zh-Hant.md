[日本語](README.md) | [繁體中文](README.zh-Hant.md) | [English](README.en.md)

# GYO-Engine

GYO 是**可重用的 C++ 遊戲 Runtime**，把 SDL、作業系統 API 與圖形 API 提供的能力，轉為可重用的遊戲機制。遊戲負責政策與內容；可選的外部控制器只能透過中立的公開邊界觀察或影響 Runtime。

GYO 不以整合式大型編輯器生態、或小型 Godot、Unity、Unreal 為目標。工具是圍繞具體執行期資料標準的獨立執行檔；專案仍從已證明的遊戲需求成長，不預先追求想像中的框架完整性。

`apps/object_fps` 是目前儲存庫內的適配驗證遊戲。它直接使用 GYO 的輸入 action、資產身份／載入、render submission／device 契約、生命週期與型別化 Runtime 邊界，驗證骨架是否實用。被驗證的標準是 GYO；GYO 永遠不依賴 Object_FPS 專屬政策或資料。

所有權與依賴規則見 [docs/architecture.md](docs/architecture.md)。渲染管線、共用 HLSL、shader ABI、部署與原生驗收說明提供[繁體中文](docs/rendering_architecture.zh-Hant.md)與[日本語](docs/rendering_architecture.ja.md)。

## 實作狀態標示

- `[已實作]`：目前適配里程碑之前已存在的程式。
- `[本階段]`：使用 Object_FPS 驗證骨架時，新增或修正的明確範圍程式。
- `[按需新增]`：僅為架構方向；目錄或型別可能不存在，具體責任出現前不建立。

以下保留**帶有狀態標記的架構願景**，不表示圖中的每個目錄或功能都已存在。

## 架構願景

```text
GYO-Engine/
├─ CMakeLists.txt
├─ README.md                                      日文入口，另有英文／繁中版本
├─ config/engine/projects.csv                     app 選取與個人備註
├─ docs/
│  └─ architecture.md                            [本階段]
│
├─ third_party/                                  相依套件包裝
│  ├─ sdl3/                                      [已實作]
│  ├─ sdl3_image/                                [本階段; 選用 PNG 解碼]
│  ├─ sdl3_ttf/                                  [本階段; 選用字型光柵化]
│  ├─ nlohmann_json/                             [已實作; 資產目錄]
│  ├─ ufbx/                                      [本階段; 選用 FBX 載入器]
│  ├─ imgui/                                     [已實作; 編輯器操作介面]
│  └─ doctest/                                   [已實作; 僅測試]
│
├─ engine/                                       可重用、後端中立的機制
│  ├─ include/engine/
│  │  ├─ base/                                   [已實作]
│  │  ├─ io/                                     [已實作]
│  │  ├─ asset/                                  [已實作 + 本階段]
│  │  │  ├─ catalog/                             身份與路徑中繼資料
│  │  │  ├─ core/                                記錄、handle、快取、生命週期
│  │  │  ├─ loading/
│  │  │  │  ├─ IAssetSource.hpp
│  │  │  │  ├─ NativeFileAssetSource.hpp         [本階段]
│  │  │  │  ├─ LoaderRegistry.hpp
│  │  │  │  └─ AssetPipeline.hpp
│  │  │  └─ loaders/
│  │  │     ├─ FontAsset.hpp                     [本階段; 編碼字型位元組]
│  │  │     ├─ FontLoader.hpp                    [已實作; 位元組 -> FontAsset]
│  │  │     ├─ TextureAsset.hpp                  CPU 解碼像素
│  │  │     └─ sdl_image/                        [本階段; 選用載入器]
│  │  └─ runtime/                                [本階段]
│  │     ├─ FrameContext.hpp
│  │     ├─ IRuntimeClient.hpp
│  │     ├─ IRuntimePort.hpp                     型別化 Query/Command/Event 邊界
│  │     ├─ RuntimeControl.hpp
│  │     └─ RuntimeLoop.hpp
│  └─ src/
│     ├─ io/                                     [已實作]
│     ├─ asset/                                  [已實作 + 本階段]
│     └─ runtime/RuntimeLoop.cpp                  [本階段]
│
├─ platform/                                     OS／視窗／事件介接器
│  ├─ sdl/                                       [本階段]
│  └─ win32/                                     [按需新增; 尚未建立]
│
├─ input/                                        輸入機制，不混入 systems/
│  ├─ include/engine/input/                      [本階段]
│  │  ├─ PhysicalInputFrame.hpp
│  │  └─ InputActionMap.hpp
│  ├─ backend/sdl/                               [本階段]
│  │  └─ SdlInput                                SDL 事件 -> 實體輸入
│  └─ tests/                                     [本階段]
│
├─ text/                                         最小字型／文字機制
│  ├─ include/text/                              [本階段; 後端中立]
│  │  ├─ ITextRasterizer.hpp                     編碼字型 + UTF-8 文字段 -> RGBA8 點陣圖
│  │  ├─ TextTypes.hpp                          請求與擁有資料的 CPU 點陣圖
│  │  └─ TextError.hpp
│  └─ backend/sdl_ttf/                           [本階段; 選用介接器]
│
├─ model/                                        [本階段; CPU 模型／動畫／蒙皮]
│  └─ backend/ufbx/                              [選用； FBX 位元組 -> ModelAsset]
├─ collision/                                    [本階段; 膠囊／射線／球體查詢]
├─ render/                                       渲染契約與實作
│  ├─ include/render/                            [本階段; 後端中立]
│  │  ├─ RenderQueue.hpp
│  │  ├─ IRenderDevice.hpp
│  │  ├─ RenderHandle.hpp
│  │  └─ RenderTypes.hpp
│  ├─ backend/
│  │  ├─ sdl/                                    [本階段; 清除／呈現介接器]
│  │  ├─ sdl_gpu/                                [本階段; 選用 SDL_GPU device]
│  │  ├─ dx12/                                   [按需新增; 尚未建立]
│  │  ├─ vulkan/                                 [按需新增; 尚未建立]
│  │  └─ opengl/                                 [按需新增; 尚未建立]
│  ├─ shaders/                                   共用 HLSL ABI 與內建 bundle
│  └─ tests/                                     [本階段]
│
├─ ui/                                           [本階段; 封閉的 JSON UI v1 標準]
│  ├─ include/ui/                                文件／codec／runtime／renderer 邊界
│  ├─ src/                                       佈局、binding、焦點、draw-list 橋接
│  └─ tests/                                     codec／runtime／renderer 測試
│
├─ framework/                                    可重用的遊戲領域政策 [按需新增]
│  ├─ stage/
│  ├─ combat/
│  └─ ai/
│
├─ apps/                                         組裝入口與具體遊戲
│  ├─ runtime/                                   [本階段] 最小清除／呈現 app
│  └─ object_fps/                                [本階段; 目前的適配驗證遊戲]
│     ├─ include/RetroFPS/                       Object_FPS 政策／領域型別
│     ├─ src/                                    介接器直接使用 GYO 契約
│     │  └─ App/ObjectFpsUi.cpp                  遊戲 binding／action 與 C++ HUD 政策
│     └─ tests/                                  無視窗的遊戲政策與 UI command 測試
│
├─ assets/                                       遊戲擁有的執行期內容
│  ├─ common/                                    [本階段; 獨立根目錄的共用基礎資產]
│  ├─ object_fps/                                [本階段]
│  │  ├─ asset_catalog.json
│  │  ├─ data/                                   戰役／敵人／武器定義
│  │  ├─ fonts/                                  遊戲選用 UI 字型與授權
│  │  ├─ maps/                                   目前的適配驗證資料
│  │  └─ textures/
│  └─ <game_id>/                                 [按需新增; 每個遊戲一個獨立根目錄]
│
├─ tools/                                        獨立、可選的執行檔
│  └─ editor/                                    [本階段; JSON UI 編輯工具]
│
└─ tests/
   ├─ engine_tests/                              [已實作 + 本階段]
   └─ backend integration tests/                 [按需新增]
```

刻意不設置 `systems/` 萬用分類桶。輸入放在 `input/`、模型動畫放在 `model/`、幾何查詢放在 `collision/`。未來的 audio、完整 physics 與 navigation 也只在實作時加入自己的模組。

`apps/<game_id>` 與 `assets/<game_id>` 成對存在。Object_FPS 使用 `apps/object_fps` 與 `assets/object_fps`，其目錄與內容不放入全域共用遊戲資產桶。

## 建置目前的骨架

需要 CMake 3.30 以上與 C++20 編譯器。第一次配置可能將啟用的第三方原始碼抓入建置目錄。新的 Visual Studio generator 可能需要更新的 CMake；已可用的 CLion／Ninja MSVC profile 可保留目前選擇。原生 preset 需要 Ninja，Windows 另需 x64 MSVC 開發者 shell。

[config/engine/projects.csv](config/engine/projects.csv) 是本機與 CI 共用的唯一 app 選取清單，欄位為 `name,description,version,enabled,windows,linux,macos`。名稱對應儲存庫根目錄下的 `apps/<name>`。簡介與版本只供個人記錄，不影響建置、套件名稱或 Release tag。布林欄接受 `1/0` 與 `true/false`；只有 `enabled` 與**目標平台**都啟用才選取。完整 CSV 與依賴契約見[建置設計](docs/architecture.md#build-project-management)。

建置 CSV 選中的 apps 與獨立 UI editor：

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --install build/dev --prefix /absolute/path/to/stage
```

不包含 apps、tools 或選用 adapters，只建置中立模組與測試：

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

可傳入 `-DGYO_APPS=object_fps`，或以引號包住分號清單，如 `"-DGYO_APPS=object_fps;runtime"` 來縮小範圍。這是子集選擇，必須先在 CSV 啟用相應 app 與目標平台，不能繞過停用設定。Editor-only 配置如下：

```sh
cmake -S . -B build-ui-editor -DGYO_APPS= -DGYO_BUILD_UI_EDITOR=ON
cmake --build build-ui-editor --config Debug --target gyo_ui_editor
```

新增或移除使用既有引擎能力的 app，只修改自己的目錄與 CSV。它的 `CMakeLists.txt` 宣告需求、target、資產、安裝與套件驗收；共用模組及 CI 不維護 app 名稱清單。CSV 修改觸發重新配置與需求重算。舊的逐 app 建置開關已移除，舊 cache 會收到遷移指引。使用新的建置目錄，或移除錯誤訊息指出的舊項目，再使用 CSV 與 `GYO_APPS`。

| 選項 | 預設 | 作用 |
|---|---:|---|
| `GYO_APPS` | `AUTO` | 依 CSV 與目標平台選取；空值不建 apps，分號清單縮小範圍。 |
| `GYO_BUILD_UI_EDITOR` | `ON` | 建置獨立 SDLRenderer／ImGui UI 編輯器；`GYO_APPS=` 為 editor-only。 |
| `GYO_BUILD_SDL_GPU_BACKEND` | `OFF` | 明確請求 `IRenderDevice` 的選用 SDL_GPU 實作；app 也能透過需求選取。 |
| `GYO_RENDER_DEVICE` | `AUTO` | `AUTO`、`SDL_GPU` 或 `NONE`；GPU 必要 app 配上 `NONE` 是配置錯誤。 |
| `GYO_GPU_DRIVER` | `AUTO` | 執行期驅動政策：`AUTO`、`D3D12`、`VULKAN`、`METAL`；不相容目標在配置時拒絕。 |
| `GYO_SHADER_BUNDLE` | `AUTO` | 目標 shader 格式；`"DXIL;SPIRV"` 等清單限制部署驅動。 |
| `GYO_SHADER_TOOL_EXECUTABLE` | 未設定 | 原生 host shader 工具絕對路徑；交叉編譯時必要。 |
| `GYO_MSVC_REDIST_DIR` | 未設定 | 可指定 MSVC 可散佈檔根目錄；否則依選定編譯器位置尋找。 |
| `GYO_BUILD_SDL_IMAGE_LOADER` | `OFF` | 明確請求選用 PNG 載入器。 |
| `GYO_BUILD_SDL_TTF_ADAPTER` | `OFF` | 明確請求選用 SDL_ttf 文字光柵化器。 |
| `GYO_BUILD_UFBX_LOADER` | `OFF` | 明確請求選用 ufbx 模型載入器；中立 Model／Collision 保持獨立。 |
| `BUILD_TESTING` | `ON` | 將 doctest 與整合測試加入 CTest。 |

App 需求與明確 adapter 選項合併，但不改寫使用者 cache 選項。中立的 Engine、Input、Model、Collision、Text、Render、UI 不依賴具體遊戲。Apps、tools 與選用 adapters 全部停用時，不需要 SDL、SDL_image、SDL_ttf 或 ImGui。

### 渲染與原生開發

GPU app 使用共用 HLSL 離線編譯：Windows 產出 DXIL 與 SPIR-V，Linux 產出 SPIR-V，macOS 產出 Metallib。Runtime 不含 HLSL 編譯器。原生 shader 工具在建置目錄內獨立建置，macOS 另需所選 Xcode 的 Metal tools。`dev` 測試 preset 執行 CPU／headless 與 shader 測試；GPU 測試需在可用顯示與 GPU 環境另跑。`ci-windows`、`ci-linux`、`ci-macos` 提供 CI 原生工具鏈入口。

CLion 可保留 MSVC profile、重新載入 CMake，再選取需要的 app target。Shader host 子建置使用該 profile 的編譯器與 Ninja 路徑。缺少選用散佈檔不阻擋開發配置或編譯。macOS 在 `project()` 前設定 deployment target 13.3，app 與 Metallib 一致。最低 OS 版本不能證明 SDK 已實作某個 API；Xcode 16.4 沒有浮點 `std::from_chars`，因此遊戲 CSV 數值使用明確十進位語法、classic C++ locale 與 float 範圍檢查。

Ubuntu 的 SDL XTest 支援需要 `libxtst-dev`。離線 host 工具停用 SDL video／dialog 並設置 `SDL_UNIX_CONSOLE_BUILD=ON`，避免 macOS 缺少 Cocoa symbol，及 Linux 拒絕刻意不含 video 的配置。這些設定不改變 app 自己的 SDL video 配置。

### CI、套件與 Release

[GitHub Actions](.github/workflows/cross-platform.yml) 固定在 Windows x64／MSVC、Linux x64／GCC 14、macOS ARM64／Xcode 16.4 建置並測試引擎與 UI editor。另外由固定來源 commit 的 CSV 產生 app × 平台矩陣。各組合使用獨立建置目錄與安裝區，只包含自身 app 與必要依賴；Editor 不放入 app 包。無 GPU／shader 需求的 app 不執行相應步驟。

每個啟用 app 都註冊安裝後 startup 測試。共用 runner 使用生成的 `share/gyo/apps/<app>/manifest.json`，從安裝包外的工作目錄執行；app 專屬驗收依 quick／release、平台與 GPU 需求選擇。失敗、逾時或缺少必要證據都阻擋產包。Object_FPS 保留 startup、gameplay、缺檔與 Linux Lavapipe 驗收，見[app 驗收指南](apps/object_fps/docs/acceptance.zh-Hant.md)。沒有 app 的平台仍跑引擎／工具檢查；全部停用的一般 CI 仍有效，但 Prepare Release 在前置檢查拒絕空集合。

各組合產出 `gyo-<name>-<platform>.tar.gz` 與 `.tar.gz.sha256`，archive 只有一個 `gyo-<name>` 根目錄。Release 從相同 commit 的 CSV 計算預期集合，檢查 app、平台、來源 SHA、release profile、必要檔案、完整驗收證據與 checksum。Quick 證據不能替代 release 證據；archive 安全、相對庫路徑及依賴檢查也都是必要關卡。

使用 **Actions → Prepare Release → Run workflow**，選擇來源分支、輸入 `v1.0.1` 等版本，需要時選 prerelease。流程固定 SHA 並執行完整 profile；全部必要檢查成功後，具寫入權限的 job 才建立 tag、Draft Release 與計算出的套件集合。從 Actions Summary 開啟 Draft，檢查說明與附件後按 **Publish release**。公開或推 tag 都不會觸發另一次建置。CSV 版本僅是個人記錄，不提供這個 Release 版本。

一般 push／PR／手動 quick 只產生 Actions artifacts。同版本 Draft 重試保留已驗證附件與手寫說明，要求既有 tag 完全符合來源 SHA，拒絕覆蓋公開版本。重跑原執行可保留 SHA，新 dispatch 可能選到更新 commit。先將 workflow 合併至預設分支才會顯示手動入口；舊執行保留原定義。完整 GUI 與恢復步驟：[繁體中文](docs/releasing.zh-Hant.md)・[日本語](docs/releasing.ja.md)。

App 內容相對執行檔安裝；非系統庫在 Windows 放 `bin`，Linux／macOS 放 `lib`。Windows Release／RelWithDebInfo 由 `cmake/GyoMsvcRuntime.cmake` 找對應 MSVC runtime DLL 並隨 app 部署。找不到時可設 `GYO_MSVC_REDIST_DIR`；缺少 DLL 會使 release install 失敗，但開發建置仍可用。不散佈 Debug CRT，Windows 10+ 提供 UCRT。使用者不需要編譯器或 SDK。CI 檢查 VC import 與 Unix link，避免開發機已有函式庫掩蓋套件缺漏。

### 過往驗證證據

渲染指南與帶日期的開發紀錄中的數量，是特定 commit／配置的歷史證據，不代表本次專案管理重構或新矩陣已通過 hosted CI。[渲染狀態](docs/rendering_architecture.zh-Hant.md#r10)保留 Windows CPU／GPU／部署、先前 Linux Lavapipe quick、macOS Metallib／CPU／部署、失敗 hosted runs 與未驗證的實體 GPU 情況。舊的 GPU 停用 Object_FPS 配置也是歷史；目前其必要 GPU 宣告會拒絕 `GYO_RENDER_DEVICE=NONE`。本次實際狀態請以新建置日誌與 Actions Summary 為準。

## 責任一覽

```text
Infrastructure 提供能力。
GYO 提供可重用的遊戲機制。
Game 提供政策與內容。
```

目前的例子：

- SDL 回報按鍵、按鈕、指標位移與焦點；`SdlInput` 轉成 GYO 實體輸入 frame；`InputActionMap` 產生具名 action／axis；Object_FPS 決定如何移動、瞄準、射擊、換彈、暫停或操作選單。
- `NativeFileAssetSource` 讀取已由 catalog 解析的執行期檔案；選用 SDL_image loader 解碼為 CPU `TextureAsset`；`AssetManager` 負責身份、handle、快取／生命週期與 loader dispatch；只有選定的 render device 建立 GPU texture。
- `FontLoader` 在中立 `FontAsset` 保留字型位元組；`ITextRasterizer` 把借用的字型 span 與一段 UTF-8 文字轉為擁有資料的 CPU RGBA8 `TextBitmap`。選用 SDL_ttf adapter 不公開 `TTF_Font`、`SDL_Surface` 或 `SDL_Texture`；render device 上傳後沿既有 sprite submission 繪製。
- Object_FPS 將不可變 game snapshot 投影為 GYO `RenderQueue` submissions。`Renderer` 負責 camera／matrix 與 world／viewmodel／post／HUD passes；`IRenderDevice` 管理不透明 GPU handles 並執行已準備 frame。`ShaderLibrary` 保持不可變 CPU shader artifacts，獨立於各 device 資源。SDL_GPU、D3D12、Vulkan、Metal 的原生 handles 留在 adapter 私有區。

`GYO::Ui` 是刻意封閉的 v1 標準：JSON codec／validation、RectTransform 佈局、型別化 bindings／actions、焦點／命中測試、buttons、sliders、固定 step lists 與有序 draw list。`GYO::UiRenderer` 解析字型／紋理資產、管理有界的整段文字快取，只提交 Overlay sprites。Rich text、shaping、localization、Flex／Grid、scripts 與 widget／plugin ABI 仍在範圍外，見 [docs/ui_toolchain.md](docs/ui_toolchain.md)。

## Object_FPS 是適配驗證使用端

這項整合不是 KamataEngine 的逐段原始碼移植。依賴方向如下：

```text
Object_FPS 遊戲政策／內容
        |
        v
GYO lifecycle + input actions + assets + render contracts + runtime port
        |
        v
選定 SDL platform / SDL_GPU / SDL_image
```

當舊遊戲暴露可重用需求，先在 GYO 新增中立機制，再由 Object_FPS 使用。它不攜帶平行的 input framework、resource manager、renderer contract 或 application loop；遊戲專屬名稱與狀態留在 GYO Core 外。

目前 maps 與 CSV 是適配測試資料，不是引擎硬編碼知識。Asset IDs 登錄在 `object_fps.*` 命名空間，campaign data 決定使用哪些內容。

### 可見的 MVP 流程

Object_FPS 擁有以下內容、佈局資料、binding 值與 action 結果：

- MainMenu：Start Game、Controls、Quit。
- Controls：輸入操作說明與 Back。
- Pause：在遊戲畫面上提供 Gamma、Exposure、Resume、Main Menu、Quit。
- Results：campaign 結果、room 結果與回到 Main Menu。
- Playing HUD：準星、HP、彈匣／備彈、換彈狀態與目前 stage 資訊。

### 跳躍與 Mark-23

- **Space** 進行一次接地跳躍；腳底位置、hit capsule、相機與射擊原點一起移動，按住不連跳。預設高度 0.6 m、重力 18 m/s²；空中仍受牆壁／敵人 grid blocking 限制。
- **滑鼠左鍵**發射半自動 Mark-23；**R** 換彈；**H** 收槍／拔槍。新 campaign 從 Draw 開始。原生動畫時間為 Shoot 0.333 s、Reload 3.733 s、Draw 0.833 s、Hide 0.367 s；初始彈匣／備彈 12/48，換彈完成才移轉彈藥。
- 模型包含有動畫的手、滑套、彈匣與三張 diffuse textures。右下獨立 camera 保留自身遮擋，附近牆壁不裁切武器，武器參與 scene exposure／gamma。
- `assets/object_fps/data/mark23_viewmodel.json` 定義 model／material／clip IDs、固定 Idle anchor、repeat sampler、camera-relative offset、rotation、scale 與武器 FOV。校準 offset 為 `(0.12, -0.18, 0.55)` m，垂直 FOV 55°，槍口定義於 `main_j` local 座標。共用 loader 以 Shoot 時間零與此配置推導每把武器的射擊幾何；武器 55°／世界 60° 的 FOV 橋接保留螢幕位置。調整配置或槍口後，建置以複製資產並重啟即可重新校準，不改變 CSV action 時間。
- 命中依該次新反衝之前的準星解析。視覺 tracer 在舊 projectiles 更新後，從帶反衝的渲染 frame 槍口出發，仍做 world obstruction 檢查，但不重複傷害。過去 GPU 投影驗收通過三種寬高比，見[校準記錄](docs/dev_logs/2026_09_15_model_muzzle_calibration.md)。
- 門透過獨立 catalog 使用真正的 `assets/common/white1x1.png`。部署包含兩個 asset roots，禁止 catalog path 逃逸。

模型分析、所有權、驗收與目前限制見[迭代記錄](docs/dev_logs/2026_09_15_mark23_jump.md)。

四個非 Playing 畫面只從 `assets/object_fps/ui/screens.json` 載入一次，沒有編譯內建 fallback、live link 或 hot reload。`UiRuntime` 負責 selection、focus、pointer capture、hit testing；Object_FPS 將不透明 action IDs 映射成型別化 commands。Playing HUD 政策維持 C++ 並輸出同一個 `UiDrawList`，GYO 不知道 Object_FPS menu action 的意義。

執行檔的 smoke 路徑檢查不同邊界：

- `--startup-smoke-test` 不建立視窗／GPU，驗證真正的部署內容載入。Quick 與 release 從不相關工作目錄啟動三平台對應的安裝程式。
- `--headless-smoke-test` 另外驗證遊戲開始、跳躍、暫停／恢復、射擊與換彈時間，不依賴視窗／GPU；release 執行此較重情境。
- `object_fps.smoke` 進入 Playing 並驗證 world frame 可提交與呈現。
- `object_fps.menu_smoke` 走正常 MainMenu 啟動；第一個 menu frame 若沒有可見 UI submission 就失敗。
- `object_fps.viewmodel_smoke` 呈現各 Mark-23 action 的開始、中段與結束。使用 `--viewmodel-smoke-test --capture-dir <directory>` 保存診斷 scene frames，可加 `--preview-4x3`／`--preview-21x9`。
- `object_fps.reload_smoke` 以 60 Hz 提交完整 Reload，包含原 frames 間短暫手腕動作。`--reload-smoke-test --capture-dir <directory>` 保存完整診斷序列，見[換彈與槍口修正](docs/dev_logs/2026_09_15_reload_muzzle_fix.md)。
- `object_fps.muzzle_smoke`、`object_fps.muzzle_smoke_4x3`、`object_fps.muzzle_smoke_21x9` 在七種 camera 情境比較 GPU 槍口 markers。16:9 使用 `--muzzle-smoke-test --capture-dir <directory>`，其他比例加 `--preview-4x3`／`--preview-21x9`。保存的 model review 影像以青色標出校準槍口；[校準記錄](docs/dev_logs/2026_09_15_model_muzzle_calibration.md)包含指令、誤差與證據。

歷史槍口校準完成 18 項整合檢查，包含重新建置的 headless 測試 1,169 assertions。三種 GPU 寬高比檢查的 marker pair 誤差為 0.0000 pixel，理論投影誤差最多 0.3823 pixel。

Headless `ObjectFpsUi` 測試解析真正 JSON，驗證日文標籤、四個 canvases、Results bindings、C++ HUD、JSON 重排後的 action ID 行為及同 frame slider 更新，不需要 SDL_ttf 或 render backend。

## Runtime Boundary 與 Weaver

與呼叫者無關的公開邊界：

```text
Game / Test / Debug Console / Replay / AI / Weaver
                         |
                         v
               GYO Runtime Boundary
              Query / Command / Event
                         |
                         v
                 GYO 機制
```

`IRuntimePort<Snapshot, Command, Event>` 是目前最小型別化邊界：控制器可查詢不可變 snapshot、提交合法的型別化 intent、檢視 Runtime 發出的型別化事實。`Query()` 與 `Events()` 是借用 view，只有效至下次 Runtime 更新；需要長期保留的資料由呼叫者複製。具體 payload 意義仍屬各 Runtime／game domain；interface 形狀、所有權規則與不傳遞 backend pointers 是 GYO 標準。

這不是完整 world／entity API、remote protocol、command scheduler 或通用 event bus；在實際使用端證明責任前繼續延後。

Weaver 尚未實作。它是可選的外部高階 Runtime，未來也只能使用與 game、test、debug tool 相同的公開機制。GYO 必須在沒有 Weaver 時仍可建置與執行普通遊戲，GYO Core 永遠不能依賴 Weaver。

## 成長規則

功能加入其責任所屬位置。新的 text backend、Render3D、Physics3D、Navigation 或外部 controller 整合，通常新增自己的 module／backend／adapter，而不改寫無關的 Asset、Input、Render、Runtime 或 game 程式。

不預先建立 editor ecosystems、node trees、通用 ECS、visual scripting、plugin frameworks、完整 RenderGraphs／physics engines、大型 DI containers、generic managers 或空 interfaces。只有現有程式提供具體責任與真實呼叫者時，才引入抽象化。

## 命名

| 種類 | 規則 | 範例 |
|---|---|---|
| 目錄 | 小寫，必要時以 `_` 分隔 | `render/backend/sdl_gpu` |
| C++ namespace | PascalCase 或以 engine 為根 | `Engine::Asset` |
| C++ source／header | PascalCase | `RuntimeLoop.hpp` |
| 每遊戲 asset root | 小寫遊戲識別字 | `assets/object_fps/` |
| 資料檔 | 小寫 | `asset_catalog.json`、`levels.csv` |

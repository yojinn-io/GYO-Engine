# 為何拆出 SDL_GPU HLSL，並在 Sprite 邊界修正貼圖方向

> Historical record: this document preserves the implementation, commands and results from its original date. Paths and ownership may predate the current architecture; use [the current architecture](../architecture.md) and [build guide](../creating_apps.md) for present instructions. These results do not validate later revisions.

## 修改的起點

Object_FPS 的文字已經經過 FontAsset、SDL_ttf、RGBA bitmap 與 GPU upload 成功出現在畫面上，但每個 glyph 都上下顛倒。這代表資料確實走完整條路徑，卻也暴露出兩個先前沒有被具體內容驗證的問題：Sprite UV 方向錯誤，以及 SDL_GPU shader source 仍直接內嵌在 C++ implementation。

這次修改的目標不是在 Text path 補一個能讓畫面暫時正常的 flip，而是修正所有 screen-space sprite 共用的語義，並讓 shader 成為 backend 擁有的獨立原始檔。

## 為何不能在 SDL_ttf 或 TextBitmap 翻轉

SDL_ttf surface、`TextBitmap`、`ImageView` 與 SDL_GPU upload 都按照相同 row index 複製資料，沒有改變左右或上下順序。真正的衝突發生在 presentation：

- 共用 XY quad 來自 3D、Y-up 座標，底部 vertex 使用 `v=1`，頂部使用 `v=0`。
- Sprite pixel projection 使用螢幕 top-left、Y 向下座標。
- 結合後，螢幕頂端落在 quad 的 `v=1` vertex，因此取到 texture 底部。

若在 SDL_ttf adapter 翻轉，只能讓文字看似正常；武器 overlay、atlas sprite 與未來所有有方向性的 screen texture 仍會錯，而且 neutral `TextBitmap` 會被迫知道某個 render backend 的 quad convention。

因此修復放在 `SpriteSubmission::sourceUv` 到 backend UV transform 的單一邊界。Public contract 明確定義 source rectangle 的 `(x, y)` 是視覺左上角，轉換為：

```text
scale  = { width, -height }
offset = { x, y + height }
```

這個公式同時適用完整 texture 與 atlas sub-rectangle，Text 與 Asset 不需要任何特例。

## 為何 HLSL 必須離開 C++ source

Vertex 與 fragment shader 是 SDL_GPU backend implementation resource，不是 C++ control flow，也不是 Object_FPS content。把整段 HLSL raw string 放在 `SdlGpuRenderDevice.cpp` 有幾個問題：

- IDE 無法把 shader 當成獨立語言與資源瀏覽。
- Shader diff 被埋在大型 backend C++ diff 中。
- C++ implementation 同時承擔 pipeline orchestration 與 shader authoring content。
- 未來增加不同 pipeline 時會持續擴大單一 `.cpp`。

因此 HLSL 的唯一來源改放在：

```text
render/backend/sdl_gpu/shaders/unlit.vert.hlsl
render/backend/sdl_gpu/shaders/unlit.frag.hlsl
```

Shader 留在 render backend，而不是 `assets/object_fps`，因為它描述的是 SDL_GPU 如何實作 GYO Render contract，不是某個遊戲的美術或內容 policy。

## 為何使用 CMake-generated private embed

本輪比較了三種做法：

1. Runtime 從 executable 附近讀 `.hlsl`。
2. CMake 從獨立 `.hlsl` 生成 backend-private embedded header。
3. Build time 直接產生 DXBC binary。

Runtime file loading 便於即時替換，但每個 executable 都必須正確部署 shader；CLion working directory、CTest、安裝目錄與漏拷貝都會成為新的 failure mode。單純 `POST_BUILD` copy 還可能在只修改 HLSL、executable 不需 relink 時留下舊檔。

Build-time DXBC 長期有價值，但目前需要先選定並維護 FXC、DXC/DXIL 或 SDL_shadercross toolchain，以及 multi-config output/package 規則。這超出本輪只修正既有 Windows/MSVC SDL_GPU backend 的需求。

所以目前選擇第二種：

- Repo 中獨立 `.hlsl` 是唯一來源。
- CMake `file(READ)` 後在 build tree 產生 private header。
- `CMAKE_CONFIGURE_DEPENDS` 讓修改 HLSL 自動觸發 regeneration 與 backend recompilation。
- Runtime 繼續使用既有 `D3DCompile` 產生 DXBC。
- Public Render API、Object_FPS 與工作目錄都不知道 generated header 或 shader path。

這不是把 HLSL 再手動複製回 C++；generated header 只是 static library 的封裝產物，不進 Git，也不是第二份 source of truth。

## 為何新增 UV mapping 單元測試

先前 `menu_smoke` 能證明 shader compile、texture upload、submission 與 present 都成功，但只檢查 submission 非零，不能判斷最終像素方向。Headless UI tests 也只驗證 commands 與 layout。

因此新增純 UV mapping regression：

- 完整 texture 的 screen top-left／top-right／bottom-right 必須對應 source 的相同視覺角落。
- Atlas sub-rectangle 的四個角必須保持指定範圍，且只翻轉該範圍內的 V 軸。

這能鎖住本次錯誤的數學原因。真正的 GPU pixel/readback test 仍然延後，因為目前 backend 尚未提供 readback mechanism；submission smoke 不能被描述成 pixel-perfect 測試。

## 驗證結果

- Windows/MSVC Release 完整 build：成功。
- Render tests：`8 cases / 87 assertions` 通過。
- CTest：`6/6` 通過。
- `object_fps.smoke` 與 `object_fps.menu_smoke`：通過。
- 實際修改 HLSL 後執行普通 build，確認會自動 regenerate private header 並重編 SDL_GPU backend。

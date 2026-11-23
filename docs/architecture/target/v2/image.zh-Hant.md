# 圖片 Module（`engine/modules/image`）

更新：2026-11-23（`v8-arch-gyo2-sdl-i`：SDL 線子批 I 新建；照 [SDL 總宣告](sdl-declaration.zh-Hant.md)第 10 節的 I 與 [asset 宣告](asset-declaration.zh-Hant.md) 5.4、5.11（使用者 2026-11-23「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`；stb_image 走 Module 這一側，`v8-arch-gyo2-sdl-q2-ufbx-stb-module`）。新 AGENTS（#296）下不另寫事前宣告；卡片自己判的取捨在第 5 節，**最終文字在合併前核准時定稿**）。

## 一個例子

Object_Connect 進一關：節點的貼圖（`assets/textures/node/heart.png`，48×48 RGBA）是 catalog 裡 `type` 為 `image` 的項目。遊戲經 asset 請求 `Handle<ImageData>`；worker 讀檔、`PngLoader` 解成 RGBA8 的像素，store 共用這一份。渲染那一側（R2b）經 Extract 拿到 handle、讀 `ImageData`，照第 2 節的排法上傳成貼圖。Match（沒有視窗）、測試、工具載同一張圖不需要 SDL 或 GPU。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::Image ──► GYO::Asset（loader 介面）
                  │  └─(私有)─► GYO::Json（settings）
                  └─(私有)─► gyo_stb_image（只在 src/adapters/stb_image/）
```

- 函式庫（`library_only`），不進 World：連結恰好是 `GYO::Base`、`GYO::Asset`、`GYO::Json`、`gyo_stb_image`（IM-3）。不認得 SDL、World、渲染、產品。
- loader（`PngLoader`）由產品登記：在 `Configure` 經 `AssetLoaderRegistry::Register<PngLoader>()`，同 audio 的 `WavLoader`。忘了登記不是啟動錯誤，是請求時的 `Failed(NoLoader)`。
- 不需要預先登記：asset 宣告 5.4 舉的「貼圖的 loader 要等 render 的裝置建好」不成立——`PngLoader` 只解到 CPU 的像素，上傳 GPU 是 R2b 經 Extract 做的事。

## 2. 像素的排法（R2b 照這裡讀）

`ImageData` 只有一種排法（IM-2）。T 的字形圖集（`GlyphAtlas`）用同一套說法：`rowPitch`、row 0 是最上面一列。

| 項目 | 定的 |
|---|---|
| 每個像素 | 4 位元組，依序 R、G、B、A，各 8 位元（`std::uint8_t`） |
| alpha | straight（沒有預乘）。PNG 本身就是 straight；要預乘是 R2b 上傳或 shader 的事 |
| 原點 | 第 0 列是畫面**最上面**一列，每列第 0 個像素在最左邊：第一個位元組＝左上角＝貼圖座標 (0, 0)。載入時**不翻轉** |
| 列距 | `RowPitch()`＝`Width() × 4`，列與列之間沒有填充。像素 (x, y) 的第 c 個 channel 在 `y × RowPitch() + x × 4 + c` |
| 色彩空間 | `Srgb`（預設，彩色的圖）或 `Linear`（法線圖等資料）。只是標籤：位元組不換算。R2b 用它選 GPU 的格式（`*_UNORM_SRGB` 或 `*_UNORM`），照 Bevy `is_srgb` → `Rgba8UnormSrgb`／`Rgba8Unorm`（`bevy_image/src/image_texture_conversion.rs:56-75`） |
| 大小 | 寬、高各 1～16384（`PngLoader::kMaxDimension`）；`ImageData{}` 是 0×0 |

**上下顛倒（master 坑 17）**：master 的 SDL_ttf 文字上下顛倒，原因是 3D Y-up 的 quad（底 v=1）配上螢幕 top-left 的投影，修在後端 UV 轉換的單一邊界（`scale={w,−h}`、`offset={x,y+h}`），不在資料路徑補翻轉（主 checkout `build/evidence/gyo2-sdl-declaration/master-platform-survey.md` 第 17 條）。所以這裡定死「第一個位元組是左上角、不翻轉」，quad 的方向由 R2b 在它的 UV 轉換那一處處理；`stbi_set_flip_vertically_on_load` 不用（它也是全域狀態，asset AS-3）。

`ImageData` 是不可變的值：任何執行緒都能讀、在任何執行緒解構（asset AS-10 (c)）。`ImageData(width, height, colorSpace, pixels)` 讓 loader 以外的地方（R2b、工具）也能做一張；`pixels` 的長度不是寬×高×4 是 Programmer Error（照 Bevy `Image::new` 的 assert）。

## 3. 圖片的 Data Contract（`image`／`png`）

**誰產生、誰讀**：任何能輸出 PNG 的軟體（繪圖軟體、Script、之後的工具），一樣有效；遊戲經 asset 讀，執行期讀原始檔、不需要 build 的產物（asset 決定 3）。工具要驗證自己的輸出就呼叫同一個 loader（只連 `GYO::Image`）。

**catalog 的項目**：

```json
{"id": "object_connect.node.heart", "type": "image", "path": "assets/textures/node/heart.png"}
{"id": "game.rock.normal", "type": "image", "path": "textures/rock_normal.png", "settings": {"colorSpace": "linear"}}
```

- `type`：`image`。`format`：`png`（副檔名推得出，可省）。
- `settings`：可以不寫（＝`srgb`）。寫的話只能有 `colorSpace`，值是 `"srgb"` 或 `"linear"`；其他成員、其他值都是 `BadSettings`（store 回 id 級的 `BadSettings`）。不認得的成員不忽略：之後加設定是放寬（加版本），不會改變已收內容的意思。

**大小**（不是 loader 的規則）：經 store 載入時先判檔案大小，預設上限 64 MiB，產品可用 asset 的 `byteLimits` 依（`image`、`png`）覆寫；超過是 asset 的 `TooLarge`，loader 不被呼叫。

**檔案格式（`png` 第 1 版）**：PNG（ISO/IEC 15948）。loader 照下表的順序判，回第一條違反的碼（`PngLoaderCode`）：

| # | 規則 | 碼 |
|---|---|---|
| 1 | 至少 8 位元組，開頭是 PNG 的簽名 `89 50 4E 47 0D 0A 1A 0A` | `NotPng` |
| 2 | 簽名之後第一個 chunk 是完整的 `IHDR`（長度欄是 13、型別是 `IHDR`、內容與 CRC 都在檔案內）；寬、高都不是 0 | `BadHeader` |
| 3 | 寬、高都不超過 16384 | `TooLarge` |
| 4 | 其餘解得出寬×高的像素：色彩型別與位元深度是 PNG 有的組合、濾波型別 0～4、`IDAT` 的 zlib 串完整且夠長、調色盤圖有 `PLTE` | `Undecodable` |

- 規則 1～3 只讀檔頭，規則 4 交給解碼器（stb_image）。超過 16384 的檔不會被解碼。
- **收的**：五種色彩型別（灰階、RGB、調色盤、灰階＋alpha、RGBA）的每種合法位元深度；濾波 0～4；Adam7 交錯；`IDAT` 分成多個 chunk；`tRNS`。
- **都變成第 2 節的 RGBA8**：灰階 g → (g, g, g, 255)；灰階＋alpha → (g, g, g, a)；RGB → (r, g, b, 255)；調色盤 → 那一格的顏色，alpha 取 `tRNS` 的那一格、沒有就 255；灰階與 RGB 的 `tRNS` → 等於那個顏色的像素 alpha 0、其他 255。1、2、4 位元的灰階放大到 0～255（乘 255、85、17）。**16 位元取高位元組**（精度掉到 8 位元）。
- **不看的**：`gAMA`、`cHRM`、`sRGB`、`iCCP`（不做色彩管理，位元組照原樣；色彩空間只由 `settings` 定）、`pHYs`、文字與其他輔助 chunk；chunk 的 CRC 不查。
- **Version**：PNG 沒有 GYO 的版本欄，版本記在這一節（今天第 1 版）。放寬（多收，例：保留 16 位元、多一個設定）時版本加一、`format` 名不變：舊執行檔拒收用到新功能的檔，內容與執行檔一起出貨、不另設協商。收窄、或改變已收內容的意思，用新的 `format` 名登記另一個 loader（AS-9）。同 audio 第 6 節。
- **標準寫法**（工具、腳本照這個寫）：8 位元 RGBA（色彩型別 6）、不交錯。Object_Connect 的貼圖就是這種（另有一張 8 位元 RGB 的 `white1x1.png`）。
- **範例**：`tests/common/image/contract/gradient_rgba_16x8.png`（`make_example.py` 用 zlib 的壓縮區塊、五種濾波寫的）；其餘的規則由 `PngLoaderTests.cpp` 以逐位元組產生的檔逐條驗。Object_Connect 的 11 張 PNG 在本機載過一次（dev_log），不放進 repo（圖的授權沒寫）。

## 4. 不在本 Module

- 上傳 GPU、取樣器（過濾、重複）、mipmap、圖集：R2b（Bevy 的 `ImageSampler` 也是渲染才用，`bevy_image/src/image_loader.rs:149-151`）。
- 自動登記 loader 的登記單位：等 R2b 需要時再加（Bevy 的 `ImagePlugin` 做這件事；今天照 audio，產品自己登記）。
- PNG 以外的格式（JPG、KTX2、DDS…）：有點名的 Feature 時另加 loader（`kFormats` 或另一個 loader 類別）。Object_Connect 沒有用到（`NoviceResources/axis/axis.jpg` 是 KamataEngine 附的、遊戲不讀）。
- 第三方授權告示：打包時附，在批次表後續（K 的 tools 線）。

## 5. 卡片照 L17 判的（不是使用者的話；可以推翻）

| # | 判的 | 根據 | 比過的 |
|---|---|---|---|
| I-1 | 只有一種像素排法（RGBA8），沒有 `PixelFormat` 欄位 | Object_Connect 全部是 8 位元；R2b 只要讀一種；Bevy 預設也把灰階、RGB 展開成 RGBA8（`expand_grayscale` 預設 true，`image_loader.rs:160-169`） | 照 Bevy 帶 `TextureFormat`（16 位元、灰階單通道）——沒有點名的 Feature，要時見 RC-IM2 |
| I-2 | `settings` 只有 `colorSpace`，預設 `srgb`；不認得的成員拒收 | Bevy 的 `is_srgb`（預設 true，`image_loader.rs:145-148`、`:176`）；PvP 的移植是 3D，法線圖要 linear；拒收讓之後加設定是放寬 | 不設 settings（照 audio 的 `WavLoader`，之後加會改變已收內容的意思）；照收忽略 |
| I-3 | 函式庫、不進 World，產品自己登記 loader | audio 的 `WavLoader` 先例；loader 不需要裝置（第 1 節） | 加 `world_image` 登記單位（SDL 總宣告第 3.3 節寫「依賴多半還要 world_asset」——今天沒有要放進 World 的東西） |
| I-4 | 型別名 `ImageData`，namespace `GYO::Engine::Image` | 和 T 的 `FontData` 同一個寫法；`Image::Image` 會讓類別遮住 namespace（命名規範 N13 的例外） | 照 Bevy 叫 `Image` |
| I-5 | 上限 16384×16384、檔案 64 MiB | Direct3D 12、Vulkan、Metal 都保證的 2D 貼圖最大邊；檔案上限同 `sound` | stb 的預設（2^24） |
| I-6 | stb_image 只抓 `stb_image.h` 一個檔（FetchContent 的 URL＋SHA-256），不 clone 整個 stb | 只用一個檔；釘 commit 的網址加 SHA-256，內容一樣不會動（asset 宣告 5.11 要的是釘 commit） | 照 yyjson 用 git clone |

## Design Invariants

條號用 IM-（asset 宣告的 RC-IM1 已用這個前綴）。

1. **IM-1 stb_image 只在一個 adapter。** `stb_image.h` 只被 `engine/modules/image/src/adapters/stb_image/` 與 `third_party/stb_image/` 的實作檔 include；`gyo_stb_image` 只被 `gyo_image` **私有**連結，標頭不出現在任何 `include/` 下（AGENTS §1-4）。編譯設定照 asset 宣告 5.11（`STBI_ONLY_PNG`、`STBI_NO_STDIO`、`STBI_NO_HDR`、`STBI_NO_LINEAR`、`STBI_NO_FAILURE_STRINGS`、`STBI_MAX_DIMENSIONS=16384`），放在 `gyo_stb_image` 的 PUBLIC 定義，實作與 adapter 看到同一份。守衛：設定時的檢查 `gyo_expect_image_links`（`tests/common/image/CMakeLists.txt`：連結恰好那四個、`gyo_stb_image` 不在 `gyo_image` 的 INTERFACE 連結裡；照 json 的 J-1、J-6）；別的 target 沒有 stb 的 include 路徑，include 了就編不過；「沒有別的 target 連 `gyo_stb_image`」靠審查。
2. **IM-2 一種像素排法。** `ImageData` 是第 2 節的排法：RGBA8、straight alpha、第 0 列在最上面、不翻轉、列距＝寬×4、色彩空間只是標籤。守衛：「PngLoader gives RGBA8, the top row first, rows packed (section 2)」「reads a file from a real encoder」「ImageData: pixels that do not fill width by height by 4 are a Programmer Error」。
3. **IM-3 中立、沒有裝置。** `GYO::Image` 的連結恰好是 `GYO::Base`、`GYO::Asset`、`GYO::Json`、`gyo_stb_image`：沒有 SDL、渲染、World、產品名。守衛：`gyo_expect_image_links`；中立靠審查。
4. **IM-4 圖片的 Data Contract。** `image`／`png` 的規則、先後、轉換、settings 與 Version 在第 3 節；手寫、腳本、工具產生的一樣有效；loader 是純函式（沒有 World、時鐘、全域可變狀態——不讀 stb 的失敗原因、不設翻轉旗標），只連 `GYO::Image` 就能呼叫。守衛：`PngLoaderTests.cpp` 的每條規則各一案（每個條件）、規則的先後、每種色彩型別與位元深度、濾波、Adam7、不看的 chunk、settings、「gives the same image for the same bytes, on any thread (AS-3)」「through the asset store」兩案。

### Replacement Criteria

下列任一成立時，不再修補，交使用者決定修還是重建。

1. **RC-IM1**（asset 宣告第 13 節定的）：不可信來源的圖成為 Feature，或 stb_image 的缺陷影響我們的內容，就換 adapter。
2. **RC-IM2**：點名的 Feature 需要 RGBA8 以外的排法（保留 16 位元或浮點、單通道省記憶體、GPU 壓縮格式），`ImageData` 要帶格式——讀它的每一處都要改。
3. **RC-IM3**：整檔讀進來再整張解碼放不下（串流、分塊、mip 層），或解碼的時間超出載入的預算。

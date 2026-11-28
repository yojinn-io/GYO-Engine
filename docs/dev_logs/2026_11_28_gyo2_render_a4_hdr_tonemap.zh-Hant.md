# 2026-11-28 渲染管線線 A4：HDR 與 tonemapping（`v8-arch-gyo2-render-a4`）

Owner：渲染管線線的卡片。分支 `claude/gyo2-render-a4`（從 `claude/gyo2-render-a` 的 `859258d6`，PR #370 的 head；之後 merge 回 `claude/gyo2-render-a`）。作者 Opus 5.5／high（子 Agent）。宣告：[渲染管線宣告](../architecture/target/v2/render-pipeline-declaration.zh-Hant.md)第 16 節（A4 小宣告）；render 頁第 2、3、4、8.2 節、RD-4、RD-11、新 RD-13、RC-RD 3；相機頁的更新行；SDL 頁第 7 節。A3（除錯標記與計時）同時在另一個工作樹做，本批沒有碰它的範圍；宣告第 15 節留給 A3。

## 前提

- 主會話交 A4（上段：公開介面＝相機的元件；顏色的路徑）；小宣告寫完直接實作、不等核准（主會話的指示）。
- 範圍（總宣告第 5.1 節）：`Hdr`、tonemapping、`Exposure`、gamma。驗收：cpu 每條曲線的純函式對照 Bevy 的式子；gpu `Rgba16Float` 寫 4.0 → 輸出 byte 和 CPU 參考值差 ≤ 1；不帶 `Hdr` 的相機像素不變（`sdl_gpu.pass3d` 原樣通過）。
- 〔Engine〕中間圖、tonemapping pass 接在 phase 上、`Color8` 在線性 target 上的意思；〔Engine〕給預設、〔作者〕選曲線；〔作者〕exposure 的值、哪個模式開 `Hdr`、玩家的亮度設定。

## 做了什麼

（這一節是第一個 commit `e9a19889` 的內容。審查之後改了的地方在「設計審查與修正」：每台 Hdr 相機一張中間圖、共用深度、tonemapping 的混合、`ColourSpace`、`Exposure` 移到 B4、`Tonemapping.hpp` 拆掉。）

1. **元件與曲線**（新 `render/Tonemapping.hpp`、`Tonemapping.cpp`）：`Hdr`、`Tonemapping{method}`、`Exposure{ev100}`（`world_render` 登記）；Bevy 不需要 LUT 的曲線六條（`None`、`Reinhard`、`ReinhardLuminance`、`AcesFitted`、`SomewhatBoringDisplayTransform`、`KhronosPbrNeutral`），**預設 `ReinhardLuminance`**；精確的 sRGB 轉換；`TonemapToBytes`（CPU 參考值）；`TonemappingPass`；`HdrTargets`／`SyncHdrTargets`。
2. **格式**：`RenderTargetFormat::Rgba16Float`（線性的光）。`TextureFormat`（CPU 像素）不加；不做 sRGB 的 target（A1 留下的題：編碼在 tonemapping 的 shader 裡）。
3. **抽出**（`camera_render`）：新 System `ExtractHdr`（`ExtractCameras` 之後）：每台相機的曲線與 `Exposure::Scale()`；這一幀要畫的 Hdr 3D 相機依目標共用一張中間圖（`Rgba16Float`、`Nearest`、跟著目標的大小），沒有就放掉。`ExtractedCamera::hdr`、`SceneTarget()`；`Queue3d` 畫在 `SceneTarget()`。
4. **合成**（`ComposePasses`）：Hdr 相機只在目標上第一個時清目標；中間圖由第一個用它的相機（沒有清除色時透明黑）與帶清除色的相機清整張；`PostProcess` 之後、`Overlay` 之前放 tonemapping pass。
5. **shader**：`tonemapping.frag.hlsl`、`mesh3d_linear.frag.hlsl`、`Tonemapping.hlsli`、`Color.hlsli`；`sdl_gpu` 的 CMake 多兩行 `--shader`。`ShaderAbi.hpp` 加兩個名字、參數的排法、方法值的 `static_assert`。
6. **後端**（`GPUDevice.cpp`，局部）：`SDLTargetFormat` 多一列；`ClearColour`（線性 target 上解碼）；3D pipeline 在線性 target 上選 `mesh3d_linear.frag`；內建 shader 表多一列。沒有改共用的結構。
7. **文件**：宣告第 16 節；render 頁、相機頁（更新行）、SDL 頁。

## 宣告裡交使用者知悉的判斷（卡片判）

- **預設曲線 `ReinhardLuminance`，偏離 Bevy 現在的 `TonyMcMapface`**：Bevy 的要 48³ 的 3D LUT（KTX2 嵌入，`bevy_core_pipeline/src/tonemapping/mod.rs:56-84`）；GYO 沒有 3D 貼圖與 KTX2（F1），加了是擴大範圍。根據是 Bevy 自己在 LUT 之前的預設（#7594，`912fb5886`，本機 bevy 的 git 歷史）。衝突點：亮部的色相與去飽和不同；膠水：無；RC-A4-1。
- **`TONEMAP_IN_SHADER` 與「不帶 Hdr 的像素不變」同時成立**：第 4.4 節那一句接在「打光必須在線性空間」之下，給 B4 的打光材質；unlit 照 bytes 算、不 tonemap。偏離 Bevy（Bevy 的 unlit 也 tonemap，`bevy_pbr/src/render/pbr.wesl:83-92`）。
- **`Exposure` 在 A4 只抽出、不影響像素**（同 Bevy：乘在打光上，unlit 與 tonemapping pass 不乘）。PvP 盤點的「exposure／gamma」（整體曝光＋gamma 調整）在 Bevy 是 `ColorGrading`，歸 D1；A4 的 gamma＝sRGB 編碼。
- PvP 現在沒有 exposure／gamma 的程式；這條線沒有改 PvP。

## 測試

（結果見下一節。）

- `render.data`（新 `TonemappingTests.cpp`）五案：六條曲線在五個刺激上等於另外以倍精度照 Bevy 的式子算的值（容許 2e-5，參考值由 scratchpad 的 Python 轉寫 `tonemapping.wesl` 算，不在 git）；黑回黑；預設；sRGB 的轉換（含 256 個 byte 的來回）；`Exposure`；`SyncHdrTargets`；`TonemapToBytes`、`TonemappingPass`。
- `render.pipeline3d` 三案：Hdr 相機的 `Pass3D` 在中間圖；合成的順序與清除；共用的中間圖。
- `render.extract` 一案：`ExtractHdr` 的接線（共用、2D 不看、曲線與 exposure、下一幀同一張、放掉、RP-3）。
- `sdl_gpu.shaders` 一案：兩個新 shader 的資源數。
- 新 `sdl_gpu.hdr`（label `gpu`）三案：寫 4.0 等的光 → 六條曲線 ±1；tonemapping 只畫 viewport、在自己的像素取樣；unlit 與清除色經中間圖＋`None` 和直接畫的一樣（±1）。測試的 shader `test_constant.frag.hlsl`。

## 驗證

- **本機**（macOS、Intel，只跑 cpu；cmake／ninja 用絕對路徑；依賴借 `gyo2-big-map-s4c` 的 `_deps`、shader 工具借渲染管線工作樹建好的那一個，同 A2）：全量建置成功（新 shader 編成 metallib）；`ctest -L 'cpu|shader'`（`1d125956` 之前的 `767c4801`）257/258，失敗的 `cmake.PluginSelection` 是借用依賴的環境問題（同 A1、A2）。第一次（`e9a19889`）另有 `gyo_os_tests`、`os.wake_fallback` 逾時：A2 工作樹以基底程式建的同一個執行檔在同一時刻也卡住，機器的 load 約 165（別的會話同時在跑突變），之後重跑通過——環境（確切原因〔未確認〕）。最後的 `1d125956` 只改了兩個測試檔的訊息，本機重建後 `gyo_render_tests` 全部 91 案 937 個判斷通過。本機沒有跑任何 GPU 測試、沒有開視窗或對話框。
- **Linux 主機**（192.168.3.20，Ubuntu 24.04；`~/Workspace/render-card-a4/`：從 GitHub clone `claude/gyo2-render-a` 的 `859258d6`，本批的 commit 用 git bundle 送過去；preset `test`＋`gcc-14`，**`CMAKE_BUILD_TYPE=Release`**（不帶除錯資訊：`/home` 和 A3 共用、只剩約 22G），shader 工具照正常的主機建置從原始碼建）。Vulkan 是 **Lavapipe**（`llvmpipe (LLVM 20.1.2, 256 bits)`；`VK_DRIVER_FILES`／`VK_ICD_FILENAMES` 只給 `lvp_icd.json`，`xvfb-run`；cpu 那一步不帶 `SDL_VIDEODRIVER`）。`1d125956`：
  - `ctest -L 'cpu|shader'` 258/258。
  - `sdl_gpu.hdr`、`phases`、`unwritten`、`contracts`、`framedraws`、`targets`、`pass2d` 7/7；`sdl_gpu.pass3d`（去掉 `GPUSubmit` 一案）11/11——**不帶 Hdr 的 3D 像素不變**；`sdl_gpu.hdr` 3/3（第 5.1 節的驗收：4.0 等的光經六條曲線，byte 和 CPU 參考值差 ≤ 1）。第一次建好（`e9a19889`）就全部通過。
- 沒有改那台機器的設定、沒有用 sudo、沒有碰 `render-card-a2`（只讀了它的腳本）與 `render-card-a3`。

## 突變

- 新 `tests/common/render/hdr_mutations.json`：52 個（H1 的曲線、sRGB、`Exposure`、pass 的資料、`SyncHdrTargets` 23 個；H2 的合成 11 個；H3 的抽出 5 個；H5、H6 的 GPU 13 個——其中 7 個改 shader 的 HLSL）。沒加的與理由在檔案的 `description`。
- 既有的（D184：find 落在本批改過的行的前後）9 個：`cp-non-camera-dropped`、`cp-non-camera-not-emptied`、`cp-written-not-first`、`cp-phase-order-reversed`（合成的迴圈改了：改成照 phase 的索引走時這一個會因為兩邊都反過來而活下來，所以合成照舊走 `InOrder()`）、`cp-later-clears-target`、`cp-clear-after`、`cp-target-ignored`、`gp-load-always`、`gp-clear-ignored`（find 改成新的 `ClearColour` 那一行，守同一個性質）。
- 結果（全部在 Linux 主機跑：本機的 load 太高，第一輪在本機跑到一半以 SIGINT 停下，runner 還原了原始檔、`git status` 乾淨）：**61/61 killed**（新 52、既有 9）。經過三輪：
  1. `e9a19889`：新的 expect 把 `-` 跳脫了兩次（`\\-`），失敗了也對不上 → wrong_failure；`gh-decode-power` 活下來：測試的顏色 (200, 100, 10) 在 2.2 次方與 sRGB 之間的差 ≤ 1 byte、10 在線性的 toe 裡 → 綠改成 40（2.2 次方時變 35）。
  2. `b94e70f8`：`CHECK_MESSAGE` 的訊息用逗號串、成了逗號運算子，標籤沒印出來；合成的三個與 `re-hdr-not-registered` 先停在別的判斷（`REQUIRE` 的數目、Product 組不起來的 R-E4）→ 改成 `<<`、expect 指實際停下的判斷（`[cp-hdr-all]`、`[cp-hdr-shared-all]`、`R-E4 … 'ExtractHdr'`）。
  3. `767c4801`：`const char*` 放在 `<<` 的最前面時 doctest 印的是位址（在主機上手動重現：`logged: 0x65475d21f018 stimulus 0`）→ `std::string(tag)`；`1d125956` 重跑這 19 個：19/19 killed。其餘 42 個在 `767c4801` 已 killed，之後只改了兩個測試檔的訊息。
- 數量（README 基準 8）：改的條文 RD-4、RD-11、新 RD-13，共 3 條，門檻 15；52 個超過的理由：RD-13 綁了中間圖（每個目標一張、格式、跟著大小、放掉）、合成（兩種清除、tonemapping 的位置與參數）、六條曲線、sRGB 的兩個方向、`Color8` 在線性 target 上的三種意思、`Exposure`、2D 不看這些獨立的決定；曲線在 CPU 與 HLSL 各寫一次（RD-4），GPU 那一份的每條曲線各一個。每一個都寫出別的突變抓不到的錯（`what`）。

## 已知缺口、沒跑的

- **Metal（Mac）上的 GPU 行為沒有驗證**：本機只跑 cpu（主會話的限制）；`sdl_gpu.hdr`、`pass3d` 只在 Linux 的 Lavapipe 跑。Metal 特有的事（`GetDimensions` 經 SPIRV-Cross、`R16G16B16A16_FLOAT` 的混合與清除）未確認。macOS 本機的建置把新 shader 編成 metallib 成功（只建置）。
- **Windows D3D12 未驗**（DXIL 的編譯也沒有在這批跑）。
- RADV 與會開視窗的案（`GPUSubmit`、`sdl_gpu.frames`）沒跑。
- 本機 `ctest -L 'cpu|shader'` 的 `gyo_os_tests`、`os.wake_fallback` 逾時：同一台機器上 A2 工作樹以基底程式建的同一個執行檔也一樣卡住，機器的 load 約 165（別的會話同時在跑突變）；判為環境，不是本批的程式（〔未確認〕確切原因）。Linux 主機上全過。
- `tests/common/ci/r0b3_mutations.json` 的 `ci-plan-no-notice` 的 find 在基底就找不到（不是本批改的），沒動。

## 設計審查與修正（修訂 1，宣告第 16.8 節）

審查兩份（主 checkout `build/evidence/v8-arch-gyo2-render-a4/`，不在 git）：設計審查（Opus／high：Blocker 0、Should 6、Nit 6、Note 8）、純淨 Code Review（Fable／high：高 2、中 7、低 7）。主會話的指示：S1／U1、U2 等使用者，其餘照下表。修正是 `90a774cf`（另一個 commit，不 amend）。

| 項 | 處理 | 理由與做法 |
|---|---|---|
| 設計 S1／U1（`TONEMAP_IN_SHADER` 涵不涵蓋 unlit） | 已定（修訂 2） | 使用者 2026-11-28 選 (c) Godot 式：不帶 Hdr 的 unlit 不 tonemap、帶 Hdr 才經 pass（宣告第 16.9 節） |
| 設計 U2（預設曲線） | 已定（修訂 2） | 使用者 2026-11-28：預設改 `None`（宣告第 16.9 節）；B4 時預設的去留交使用者（RC-A4-7） |
| 純淨 高-1＋設計 S3／U5（Hdr 的合成語意） | 採用 | 每台 Hdr 相機一張中間圖、每幀清整張；tonemapping 對目標上第一個相機取代、之後的 alpha 混合（`PassFullscreen::blend`，照 Bevy upscaling）；目標的清除照不帶 Hdr 的規則；深度每個目標一張、所有 3D 相機共用（照 Bevy 的 `ViewDepthTexture` 鍵）。中間圖不依目標共用是偏離 Bevy：共用時後面相機的 tonemapping 會把前面相機畫在目標上的 UI 蓋掉（GYO 的 UI 照 bytes 在目標上，RP-4）。測試：`[cp-hdr-later-*]`、`[cp-hdr-none-transparent]`、GPU 的 `[gh-ui-kept]`（Hdr 世界相機後接非 Hdr 的 UI 相機）、`[gh-split-kept]`（兩台 Hdr 相機分 viewport）、`[gh-blend-over]`（背景＋`None` 的 Hdr 相機） |
| 純淨 高-2＋中-7＋設計 N4（線性 target 上的顏色） | 採用 | `ColourSpace`／`ColourSpaceOf`／`SampledAsLinear` 在 render 一處；清除色、2D、3D 一致（`Color8` 與 bytes 取樣的 texel 解碼、線性取樣的 texel 照原值）；後端照它選 `*_linear.frag`、pipeline 的鍵明寫顏色空間、每個 draw 推 `LinearColourUniform`。2D 在線性 target 的語意照 Bevy／Godot（線性空間混合）。`[gh-2d-linear]`、`[gh-srgb-texture-once]`、`[gh-bytes-texture-decoded]` |
| 設計 S2＋純淨 中-6（`Exposure`） | 採用 | 移到 B4；第 5 節的批次樹與 B4 列照實改；使用者的「空殼刪掉」（HANDOFF `v8-arch-gyo2-mode-config-m1`） |
| 設計 S4（Hdr 的 `PostProcess`） | 採用（寫明） | RD-14 與 `CameraPhase::PostProcess` 的意思寫明；RC-A4-2 改成 D1 的前置。「tonemapping 讀 PostProcess 的輸出」就是互換本身（要第二張圖與寫哪裡的約定），屬 D1 |
| 設計 S5（TargetUpdate＋Hdr） | 採用 | 新的 `render.extract` 一案 `[re-hdr-update-*]`；突變 `tm-sync-picture-never-let-go` 合到它 |
| 設計 S6（RD-13 拆條、突變數） | 採用 | RD-13（target）、RD-14（合成與 tonemapping）、RD-15（顏色空間）；`guards` 照新條文；數量見「突變」 |
| 純淨 中-1＋設計 N3（索引還原 phase） | 採用 | `CameraPhases::Through`／`After(PostProcess)` |
| 純淨 中-2（phase 的 target 是約定） | 採用 | `PhaseTarget(camera, phase)`，`Queue2d`、`Queue3d`、`ui_render` 都取它 |
| 純淨 中-3（`Tonemapping.hpp` 四種責任） | 採用 | 元件到 `Drawables.hpp`、`HdrTargets` 到 `RenderTargets.hpp`、`TonemappingPass` 到 `Compose.hpp`、曲線的 CPU 實作到測試的 `TonemappingReference.hpp`；`Tonemapping.hpp`／`.cpp` 拿掉 |
| 純淨 中-4（sRGB 重複） | 採用 | Math 的 `DecodeSrgb`／`EncodeSrgb`；HLSL 的解碼也先 clamp |
| 純淨 中-5（方法的值三處） | 採用 | `TonemappingMethods.h` 一份 `#define`，C++ 與 HLSL 都 include（shader 工具的 include 目錄是 `shaders/`，以 `../include/...` 取；CMake 的 DEPENDS 加上它）；`static_assert` 拿掉 |
| 純淨 低-1（`ExtractHdr`） | 採用 | 只抽 `Tonemapping` 與 target；`kExtractHdr` 常數；`std::optional` |
| 純淨 低-2（Compose 的重複） | 部分 | 只清除的 `Pass2D` 抽成 `ClearOnly`；`PixelsOf` 在 Pipeline2d／3d 的另兩處不動（不屬這批的改動） |
| 純淨 低-3（tonemapping shader 不在內建表） | 拒絕 | 嵌入的 shader 由同一次工具呼叫編成同樣的格式；釋放的三種寫法是 A3 也碰的地方 |
| 純淨 低-4（`Load` 代替 `Sample`） | 拒絕 | 全畫面 pass 的 ABI 是貼圖＋sampler 一對（A2），`Load` 會讓 sampler 被拿掉、資源數不合 |
| 純淨 低-5（零碎） | 部分 | `kTransparentBlack`、HLSL 的 `GYO_TONEMAP_NONE` case；`/1.2` 隨 `Exposure` 拿掉；亮度係數兩處、`Mix` 隨曲線移到測試；中間圖格式固定（Hdr 的定義） |
| 純淨 低-6（測試空白） | 採用 | 2D 在中間圖的中間值、非 Hdr 在前 Hdr 在後的順序都加了（見上） |
| 純淨 低-7（元件不進雜湊） | 拒絕 | 同既有的相機元件，作者的設定、不是被判定的狀態 |
| 設計 N1（照比例取樣） | 採用 | 改成 1:1、超出的取邊緣 |
| 設計 N2（`guards` 的文件名） | 採用 | `render-pipeline-declaration RP-3`／`RP-4` |
| 設計 N5（`tonemaps_in_shader`） | 採用 | 第 16.1 節補上完整條件 |
| 設計 N6（pass3d「原樣」） | 採用 | 照實寫成 Linux 上 11/12（`GPUSubmit` 要視窗） |
| 設計 Note 1～8 | 知悉 | Note 3（記憶體）寫進 RC-A4-6；Note 7（sRGB 貼圖）隨高-2 解了；其餘不改 |

### 修訂 1 的驗證與突變

- **本機**（macOS、只跑 cpu；`90a774cf` 的程式）：全量建置成功（新的 `pass2d_linear.frag`、`LinearColour.hlsli`、`TonemappingMethods.h` 的 include 編成 metallib）；`ctest -L 'cpu|shader'` 257/258（`cmake.PluginSelection` 同前）。`569448b7` 只改了一個測試的訊息與突變目錄：重建後 `gyo_render_tests` 93 案 696 個判斷全過、`sdl_gpu shaders:` 全過。沒有跑 GPU、沒有開視窗。
- **Linux 主機**（Lavapipe `llvmpipe (LLVM 20.1.2, 256 bits)`、xvfb、Release 建置，同前）：`90a774cf` 與 `569448b7` 都是 `ctest -L 'cpu|shader'` 258/258；`sdl_gpu.hdr`、`phases`、`unwritten`、`contracts`、`framedraws`、`targets`、`pass2d` 7/7；`sdl_gpu.pass3d`（去掉要視窗的 `GPUSubmit` 一案）11/12 中的 11 案全過——不帶 Hdr 的 3D 像素不變；`sdl_gpu hdr:` 5/5（新加的顏色空間一案、合成三種情形一案）。第一次建好就全過。
- **突變**：`hdr_mutations.json` 改成 51 個（退役與理由在檔案的 `description`：CPU 曲線的 7 個——曲線的 CPU 實作現在是測試自己的參考——、sRGB 函式的 4 個、`Exposure` 的 4 個、`tm-bytes-alpha`、`gh-method-values`（值只剩一份，兩邊一起錯就等於沒錯）、依目標共用的 `tm-sync-per-camera`；新增顏色空間、混合、共用深度、`PhaseTarget`、render target 上的 Hdr 相機等）。D184 重跑 find 落在改過的行附近的既有突變 24 個（`cp-*` 7、`gd-*` 4、`gt-*` 2、`rt-sync-gone-kept`、`gp-*` 5、`p2-viewport-whole`、`g3-*` 3、`ui-pass-viewport-top`）；`gp-clear-ignored`、`cp-later-clears-target`、`gt-target-not-mapped`、`gt-target-drawn-on-main`、`cp-clear-after` 的 find 改到新的行（守同一個性質）。
  - 結果（Linux）：`90a774cf` 75 個裡 73 killed；`cp-hdr-tonemapping-after-overlay` wrong_failure（先停在沒有標籤的 `REQUIRE`）、`cp-clear-after` survived（第一台相機的清除改成 `ClearOnly` 那一行，舊的 find 只落在之後相機的 viewport 清除上）；`569448b7` 修好這兩個後 2/2 killed。合計 **75/75 killed**（新 51、既有 24）。
  - 數量（README 基準 8）：改的條文是 RD-4、RD-11、RD-13、RD-14、RD-15，共 5 條，門檻 25；51 個超過的理由：RD-14 的曲線在 HLSL 各一個（6）與合成的獨立決定（清除兩種、混合兩向、位置、曲線、viewport）、RD-15 在 render（規則 2）與後端（片段 shader 兩種、uniform、清除色）與 shader（兩個分支、解碼）各自可錯、RD-13 的 target 生命週期（每台一張、每個目標一張深度、跟著大小、格式、過濾、留著、換目標、放掉兩種）。每一個都寫出別的突變抓不到的錯（`what`）。拆條與去掉 `Exposure` 少了 16 個重複或沒有讀者的。**已核准**（使用者 2026-11-28，side chat，經主會話；HANDOFF `v8-arch-gyo2-render-a4-mutations`，原話「核准。」）：用修訂 1 之後的 51 個（修訂 2 只改 `tm-default-method` 的 find／what，總數仍是 51）；比過的：砍到門檻 25 以內——最可能被砍的是 GPU 那份的曲線突變，但那份最難用肉眼看出錯；理由：這批同時涵蓋 CPU 與 HLSL 兩份實作、六條曲線、sRGB 雙向、`Color8` 的三種意思、中間圖的生命週期與合成，每個突變都寫了別的突變抓不到的錯、全部屬於契約，Linux 上全部 killed。

## 修訂 2：U1、U2 的決定（宣告第 16.9 節）

- **決定**（使用者 2026-11-28，主會話轉 side chat，HANDOFF `v8-arch-gyo2-render-a4-u1-u2`；原話「照建議，CG方向我是新人，需要你來輔助判斷。」）：U1＝(c) Godot 式（不帶 Hdr 的 unlit 不 tonemap，帶 Hdr 才經 pass）；U2：預設曲線 `ReinhardLuminance` → `None`。比過的：(a) 預設 `ReinhardLuminance`（開 Hdr 時 unlit 變暗，1.0 變 0.5）；(b) 照 Bevy 字面、不帶 Hdr 也在 shader 裡 tonemap（今天的畫面全部變暗、像素測試要重定）。理由：壓不壓亮部、用哪條曲線是作者的美術規則，Engine 的預設不替作者決定；`None` 讓開不開 Hdr 預設畫面都一樣。Godot 的預設不壓（`scene/resources/environment.h:116` `TONE_MAPPER_LINEAR`；`servers/rendering/renderer_rd/shaders/effects/tonemap.glsl:246-248` linear 原值返回）；Bevy 預設有曲線是因為它預設就有 PBR 打光。
- **做法**：先把 `[tm-default]` 改成期待 `None`，在舊碼上確認失敗（`TonemappingTests.cpp:85: ERROR: CHECK( R::Tonemapping{}.method == Method::None ) is NOT correct!`），再改 `Drawables.hpp` 的預設值（`68fed0b4`）。查過依賴預設曲線的測試：GPU 案（`HdrTests.cpp`）與合成案都明寫曲線，`[re-hdr-tonemapping]` 比的是「沒有元件＝`Tonemapping{}`」（跟著預設走），所以**沒有要重定的像素或期待值**，只改了 `[tm-default]`。B4 時預設的去留寫成新的 RC-A4-7（RC-A4-1 是 LUT 曲線本身，另立一條比較清楚）。
- **突變**：只改 `tm-default-method` 的 find／what（不新增、不刪，總數 51）。本機（CPU）跑它：1/1 killed。D184：find 落在這次改動範圍的只有它。
- **本機驗證**（macOS、只跑 cpu）：全量建置成功；`SDL_ASSERT=abort ctest -L 'cpu|shader'` 256/258：`cmake.PluginSelection`（借用依賴的環境問題，同前）與 `sdl.unattended_assertion`——後者是因為把 `SDL_ASSERT=abort` 匯出給整個 ctest，SDL 的預設 handler 照 hint 直接 abort、不印測試自己 handler 的字；不帶這個環境變數單獨跑通過（產品的 ctest 本來就自己帶這個 hint）。沒有跑 GPU、沒有開視窗。


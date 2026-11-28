# 2026-11-28 渲染管線線 A4：HDR 與 tonemapping（`v8-arch-gyo2-render-a4`）

Owner：渲染管線線的卡片。分支 `claude/gyo2-render-a4`（從 `claude/gyo2-render-a` 的 `859258d6`，PR #370 的 head；之後 merge 回 `claude/gyo2-render-a`）。作者 Opus 5.5／high（子 Agent）。宣告：[渲染管線宣告](../architecture/target/v2/render-pipeline-declaration.zh-Hant.md)第 16 節（A4 小宣告）；render 頁第 2、3、4、8.2 節、RD-4、RD-11、新 RD-13、RC-RD 3；相機頁的更新行；SDL 頁第 7 節。A3（除錯標記與計時）同時在另一個工作樹做，本批沒有碰它的範圍；宣告第 15 節留給 A3。

## 前提

- 主會話交 A4（上段：公開介面＝相機的元件；顏色的路徑）；小宣告寫完直接實作、不等核准（主會話的指示）。
- 範圍（總宣告第 5.1 節）：`Hdr`、tonemapping、`Exposure`、gamma。驗收：cpu 每條曲線的純函式對照 Bevy 的式子；gpu `Rgba16Float` 寫 4.0 → 輸出 byte 和 CPU 參考值差 ≤ 1；不帶 `Hdr` 的相機像素不變（`sdl_gpu.pass3d` 原樣通過）。
- 〔Engine〕中間圖、tonemapping pass 接在 phase 上、`Color8` 在線性 target 上的意思；〔Engine〕給預設、〔作者〕選曲線；〔作者〕exposure 的值、哪個模式開 `Hdr`、玩家的亮度設定。

## 做了什麼

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

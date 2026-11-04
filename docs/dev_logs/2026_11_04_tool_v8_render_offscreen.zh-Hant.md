# 工具 v8：v8-engine-render-offscreen

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`（PR #146），接在 `-core` 之後。

- 段：上段（Engine 的公開介面、GPU 同步：讀回的時機）。
- 實作者：Opus 5.5／high（主會話）。宣告允許讀回時機的部分局部用 xhigh，但沒有用。
- 審查者：Opus 5.5／high（子 agent，一次）。
- 沒有放寬判定，沒有開檢驗者。
- 估時 4～6 h；從開工到審查的指出處理完，約 1.5 h。

**狀態：**
- 程式、測試、突變、審查都完成。
- 本機 Metal（L2）通過；Linux（Lavapipe）要看 L1。
- Windows（D3D12）不驗（宣告的範圍的限界）。

相關文件：
- [事前宣告](../tools/plans/v8/model-viewer.md)：「`v8-engine-render-offscreen`（(B′)）」與「實作後的結果」；
- [渲染架構](../rendering_architecture.zh-Hant.md) §6 新加的一段（日語版同步）。

## 做了什麼

- `SdlGpuRenderDevice` 多一種建立方式 `CreateOffscreen(SdlPlatform&, SdlGpuOffscreenOptions)`：
  - 不呼叫 `SDL_ClaimWindowForGPUDevice`，視窗留給 SDL 2D 渲染器；
  - 每幀畫到裝置自己的 RGBA8 sRGB 貼圖；`AcquireFrame` 一定交出目前大小的幀，`SubmitFrame` 回傳 `Presented`；
  - `SetFrameSize`：從下一幀生效，幀進行中拒絕，大小為 0 拒絕；
  - `ReadFrame`：同步讀回最後送出的幀，和 `ReadTexture` 共用同一段下載程式（`Download`）；
  - 視窗模式的裝置呼叫這兩個函式，回 `UnsupportedOperation`。
- `Initialize` 拆成兩部分：兩種裝置共用的 `CreateDevice`（驅動、裝置、frames in flight、取樣器），以及視窗模式自己的認領與 swapchain 設定。
  - 視窗模式的變化：frames in flight 和取樣器改在認領視窗之前設定。
  - 審查查了 SDL3 原始碼：三個驅動的預設本來就是 2，認領時才讀這個值，所以行為不變。
- `IRenderDevice`、`Renderer` 的介面沒有改；`Renderer` 直接在離屏裝置上畫（R3 就這樣測）。
- 測試 `render.sdl_gpu_offscreen`（標籤 `gpu`，`tests/common/render/sdl_gpu/OffscreenSmoke.cpp`）：
  - R1：視窗已經有 SDL 2D 渲染器時，建立、畫、讀回、銷毀離屏裝置，2D 渲染器照常 `Clear`／`Present`。另外，離屏裝置還在時，視窗模式的裝置仍能建立（SDL 把認領記在視窗上，第二次認領會被拒）。
  - R2：清成 sRGB 位元組正中的線性值，每個像素逐位元組相同，容差 0。連續兩幀用不同顏色，各自讀回自己那一幀。寬度用奇數 41，順便測補齊過的列寬。
  - R3：紅色四邊形正對鏡頭，綠色四邊形繞垂直軸轉 45°，兩者交叉。正面與背面看，左右兩個取樣點都必須是「紅、綠」。
  - R4：改大小後讀回新的寬高；改大小後、送出新幀前讀回會被拒；幀進行中讀回或改大小會被拒；放棄的幀不能讀回；什麼都沒寫進幀貼圖的幀也不能讀回。
- 文件：渲染架構 §6 加一段（正體中文、日語）。

## 驗證與經過

- 本機 macOS（`build/target/_build/mv`，Metal）：
  - `render.*` 3／3 通過，含既有的 `render.sdl_gpu_mesh_smoke`（R5）；
  - 整棵樹的建置成功，`cpu|shader` 56／56 通過。
- 件數の突き合わせ：一致。
  - 宣告 4 條 ＋ 審查後加 1 條 ＝ 5 條，5／5 killed，沒有 skipped。
  - 第一輪 `ro-reads-earlier-frame` 是 wrong_failure：突變用的快取是整個行程共用的，把第一個裝置（32×24）的讀回帶到了第二個裝置，所以失敗在「first frame」，不是預期的「second frame」。
  - 這是突變本身寫錯，不是測試或實作的問題。改成每個裝置各自的快取後 killed。預期訊息沒有改。
- 審查的指出與處理：
  1. （中）R3 原本只檢查「兩點顏色不同、轉到背面時同一側換人」，深度順序反過來（較遠的贏）也會通過。改成比對確切的顏色：正面與背面都是「紅、綠」。根據是 Engine 的左手座標、+Z 朝前，以及 `MakeRotationY` 的方向；Metal 上通過。屬於收緊，沒有放寬。
  2. （低）離屏的 `AcquireFrame` 不等之前的幀，不讀回的呼叫端可能讓 CPU 跑在 GPU 前面太多。工具每幀都讀回，所以沒有改行為，只在標頭寫明。
  3. （低）原本只要送出成功就算可讀，即使沒有 pass 寫進幀貼圖。改成必須有 pass 寫進幀貼圖並儲存；加測試與突變 `ro-ready-without-store`。
  4. （低）先改大小再改回原大小，也算「改過大小」：照標頭的寫法，不改。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/render-offscreen/`，有：
  - `mutations.txt`（最後一輪 5／5）、`mutations-round1.txt`、`mutations-round2.txt`；
  - `offscreen-metal.txt`、`ctest-all.txt`、`base-commit.txt`。

## 和事前宣告不同的地方

- 平台仍要傳入：Vulkan 載入驅動要用 SDL 的視訊子系統，這由平台持有。宣告只寫了「不認領視窗」。
- 讀回的條件比宣告更嚴：改大小後、放棄的幀、沒寫進幀貼圖的幀，讀回都會被拒（R4 的延伸）。

## 發現（不在本批範圍）

- 沒有。

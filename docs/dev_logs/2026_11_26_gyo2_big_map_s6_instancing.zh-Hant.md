# 2026-11-26 大地圖線 S6：instancing（`v8-arch-gyo2-big-map-s6`）

Owner：大地圖線（GYO2 主會話轉達）。分支 `claude/gyo2-big-map-s6-instancing`（draft PR，開向 `gyo2`）。作者 Opus 5.5／high（子 Agent）。宣告：[大地圖宣告](../architecture/target/v2/big-map-declaration.zh-Hant.md)第 16 節；render 頁第 8.7 節（IN-1～IN-3）。

## 前提

- 使用者（主會話轉達）：「S6：instancing — 照宣告寫的做：做成 Engine 的公開介面，用測試場景裡重複出現的模型驗證。照 Bevy 的做法，偏離 Bevy 時寫出理由。」
- 〔Engine〕判斷哪些 draw 可併、把實例資料交給 GPU：作者伸不進 `Queue3d` 和後端。
- 〔作者〕讓東西重複：給同一個 `ModelRef`＋`ImageRef` 即可，不加元件。
- 主會話的選項：clip-from-world（第 14 節 S-4）可順手收斂。**沒做**：S6 沒碰相機的推導值（`Pass3D::clipFromWorld` 原本就每相機每幀算一次），收斂要改 camera 的 CM 條文（宣告 16.3 末）。

## 做了什麼

1. **資料契約**（`RenderCommands.hpp`）：
   - 新 `Instance3D{world, color}`（68 B）、`Pass3D::instances`、`Draw3D::instanceOffset／instanceCount`。
   - 拿掉 `Draw3D::world／color`，畫一個東西＝一個實例（只留一條路）。
2. **shader ABI**（`ShaderAbi.hpp`、`Mesh3D.hlsli`、`mesh3d.*.hlsl`）：
   - slot 1 每實例（location 2～5 world 的四列、6 顏色）。
   - vertex uniform 改成 pass 的 `clipFromWorld`。
   - fragment uniform 拿掉，顏色是 `nointerpolation` 的 varying。
3. **`Queue3d` 自動合批**（`Pipeline3d.cpp`），照 Bevy 的 binned／sorted phase：
   - 鍵＝`MeshId`、`TextureId`、混合、深度測試、深度寫入、cull。
   - 不透明的進 bin，bin 在最近成員的位置、bin 內近到遠。
   - 半透明與不測深度的只併相鄰同鍵的。
   - CPU 蒙皮的不併。
4. **`sdl_gpu`**（`GPUDevice.cpp`）：
   - 每幀的實例上傳到 instance buffer。
   - 每個 draw 把 slot 1 綁在它的第一個實例，`first_instance` 恆 0（`SDL_gpu.h:3502-3525`）。
   - pipeline 多一個 instance-rate 的頂點緩衝。
5. **文件**：宣告第 16 節、render 頁（8.3 的資料、8.5 的偏離說明、8.7、範囲の限界）。

## 測試

- 新：
  - `render.pipeline3d` 三案：不透明的鍵與 bin、半透明／疊層只併相鄰、剔除／層／蒙皮。
  - `render.extract` 一案（測試場景）：
    - 20 個箱子＋3 個換圖的＋1 盞燈＋5 個在相機背後的（帶 `visibility`），結果是 3 個 draw、24 個實例。
    - 再加 5 個箱子，仍是 3 個 draw、29 個實例。
  - `sdl_gpu.pass3d` 一案（GPU）：三個實例＋第二個 draw 從自己的第一個實例開始＋混合實例照順序疊。
- 改了量法（守的性質不變）：
  - `[vs-queue]`／`[vs-queue-no-list]` 原本數 draw，改數實例（同網格的現在會併）。
  - `sdl_gpu` 3D 像素測試的 `Draw3D` 由實例給 world 與顏色。
  - `[g3-abi-fragment-uniforms]` 的期待值由 1 改為 0。

## 驗證（本機 Apple Silicon Mac，preset `test`）

- 建置：
  - 依賴的原始碼借用另一個工作樹已抓好的 `_deps`（`FETCHCONTENT_SOURCE_DIR_*`）。
  - shader 工具用同一個工作樹已建好的 `gyo_sdl_gpu_shader_tool`（`GYO_SHADER_TOOL_EXECUTABLE`）。
  - 全量建置成功。
- `ctest -L 'cpu|shader'`：254 個裡 253 通過。
  - 失敗的 `cmake.PluginSelection` 是上面借用依賴造成的：巢狀配置設了 `FETCHCONTENT_FULLY_DISCONNECTED`，要求這個建置自己的 `_deps` 裡有 yyjson-src。和本批無關，L1 會重跑。
- GPU（Metal，8×8 讀回，不是量測）：`sdl_gpu.pass2d`、`sdl_gpu.pass3d` 通過。
- 突變：
  - `tests/common/render/instancing_mutations.json`：15/15 killed（GPU 的 3 個在 Metal 上）。
  - `render_mutations.json` 裡 find 失效而改寫的 7 個（`g3-abi-frag-no-texture`、`-no-colour`、`g3-transform-world-ignored`、`g3-transform-order`、`g3-colour-ignored`、`g3-mesh-index-offset-ignored`、`g3-vertex-offset-ignored`）：7/7 killed（Metal）。其他既有的 3D 突變的 find 都還找得到（腳本逐一確認），沒有重跑。
- 突變數量：15 個＝新 DI（IN-1～IN-3）3 條 × 5，在上限內。每個都是獨立可錯的判斷，沒加的寫在清單的 description。

## 沒做／沒跑

- GPU 效能量測（要先問）、LOD、串流、`NoAutomaticBatching`（RC-IN1）、GPU 剔除與 indirect（RC-IN2）、`MultiMesh` 形狀的元件（RC-IN3）、clip-from-world 收斂（S-4）。
- Windows（D3D12）、Linux（Vulkan／Lavapipe）上的實際畫面：交給 L1。

# 模型檢視工具的事前宣告（`v8-tool-model-viewer`，2026-11-04 核准）

更新：2026-11-04（第 2 版：`-render` 加入 (B′)，重比、重估，建議改為 (B′)；見文末「修訂紀錄」）。Owner：新工具 `model_viewer`（`tools/model_viewer/`，本批建立）。所屬 PR 線：anim-events-line（[assets.md](assets.md)「實作順序」）。記錄器是 [HANDOFF](HANDOFF.md)。

**狀態：已核准（2026-11-04，第 2 版，全部照建議；見「決定紀錄」）。`-core` 完成（見「`-core` 實作後的結果」）；`v8-engine-render-offscreen` 完成（見「`v8-engine-render-offscreen` 實作後的結果」）；`v8-engine-sdl-renderer-texture` 2026-11-04 開始。** 本文件是 AGENTS §14.1 的事前宣告：判定、門檻、試行次數在看結果之前寫好，核准後不放寬。你核准之前不寫程式。

## 代號對照

| 代號 | 一句話 |
|---|---|
| `v8-tool-model-viewer-core` | 子批 1：工具登錄、核心程式庫、CLI；讀模型、讀寫 `gyo.anim_events` |
| `v8-tool-model-viewer-preview` | 子批 2：視窗；模型資訊面板、骨骼動畫預覽、播放控制 |
| `v8-tool-model-viewer-events` | 子批 3：在時間軸上編輯事件時刻、存檔 |
| `v8-tool-model-viewer-event-sounds` | 子批 4（等 PR #145 合併）：`gyo.event_sounds` 的顯示、試聽、編輯 |
| `v8-tool-model-viewer-trial` | 子批 5：本機量測（L2）＋你在 GUI 上試用（L3，和 audio_editor 一起） |
| `v8-engine-render-offscreen` | （選 (B′) 才有）Engine：GPU 裝置不認領視窗，畫到指定大小的貼圖並讀回 |
| `v8-engine-sdl-renderer-texture` | （選 (B′) 才有）Engine：SDL 2D 渲染器的貼圖 API；framework 的 ImGui 綁定把它交給 ImGui |

## 現況（2026-11-04，以 origin/master `4918b1fa` 為準）

| 項目 | 現況 | 證據 |
|---|---|---|
| 動畫事件契約 | `gyo.anim_events` v1，Engine（`GYO::Model`）持有；有 `ParseAnimationEvents`、`BindAnimationEvents`，**沒有寫出（serializer）**。契約寫明「工具若自己留一份規則，要有和這個 parser 對齊的測試」 | [animation-events.md](../../../architecture/animation-events.md)「Contracts」、`engine/model/include/engine/model/AnimationEvents.hpp` |
| 動畫事件的資料 | PvP 只有一份：`weapons/nailer_v2/viewmodel/viewmodel.anim_events.json`（`Armature\|Reload` 2.35 s `magazine_in`），catalog type `text` | `assets/object_fps_pvp/asset_catalog.json` 第 29 行 |
| 模型的能力 | `ModelAsset`（節點、材質名與底色、蒙皮網格、片段）；`SamplePose`、`SkinMesh`、`AnimationInstance`、`PlaybackInterval::Crossed`；FBX 由 `UfbxModelLoader`（`IAssetLoader`）從位元組讀 | `engine/model/include/engine/model/{ModelAsset,Animation}.hpp`、`engine/model/backend/ufbx/` |
| 材質與貼圖 | 模型只帶材質名稱；材質名 → 貼圖 ID 的對應是產品的（PvP 的 `character.json` 等） | `ModelAsset.hpp` 的 `Material` 註解 |
| 工具的視窗 | framework 的 `RunImGuiShell`：ImGui 畫在 Engine 的 **SDL 2D 渲染器**上；沒有 3D 裝置。ImGui 的 SDL 渲染後端支援 64k 以上頂點（`RendererHasVtxOffset`） | [framework architecture.md](../../../framework/architecture.md)、`imgui_impl_sdlrenderer3.cpp`（ImGui v1.92.5）第 83 行 |
| Engine 的 3D 繪製 | 已有 `GYO::ModelRenderer`（`ModelAsset`＋每個材質的可選貼圖與色調 → `RenderQueue`，PvP 用的同一條路）與 `Renderer`（需要 `ShaderLibrary`，從著色器 bundle 載入） | `engine/render/model/`、`engine/render/include/engine/render/{Renderer,ShaderLibrary}.hpp` |
| GPU 裝置與讀回 | `SdlGpuRenderDevice::Create(SdlPlatform&)` **一定認領視窗**（`SDL_ClaimWindowForGPUDevice`），每幀 `AcquireFrame()` 拿 swapchain。讀回已有：`IRenderDevice::ReadTexture`（同步：送出後等 fence）、`Renderer::RequestSceneCapture()`／`TakeSceneCapture()`（診斷用，讀的是曝光前的 Rgba16Float 場景，在 CPU 逐像素解 half）。**缺的**是「不認領視窗、畫到自己的貼圖」的幀 | `SdlGpuRenderDevice.cpp` 第 263～275、930～970、1068～1105 行；`Renderer.cpp` 第 243～260、307 行 |
| SDL 2D 渲染器的貼圖 | `SdlRenderer` 只有 `Clear`／`Present`，**沒有貼圖 API**。ui_editor 的資產預覽靠 `*Native` 例外自己 `SDL_CreateTexture`；platform-sdl 的 Replacement Criteria 2：除了 ImGui 綁定與「ui_editor 的資產預覽（到 SDL 渲染器有貼圖 API 為止）」，再有產品為新能力要 `*Native`，就表示中立 API 切錯了 | `engine/render/backend/sdl/include/engine/render/backend/sdl/SdlRenderer.hpp`、`tools/ui_editor/src/AssetPreviewContext.cpp` 第 52 行、[platform-sdl.md](../../../architecture/platform-sdl.md) 第 212～214 行 |
| GPU 的 CI | Linux 列在 Lavapipe（Vulkan 軟體實作）上跑 `-L gpu -R '^render\.'` | `.github/workflows/cross-platform.yml` 第 144～173 行 |
| 事件 → 聲音 | 2026-11-04 決定由 framework 持有新格式 `gyo.event_sounds` v1（一個 anim_events 檔配一個；一個事件一個聲音）。契約與實作在 [PR #145](https://github.com/yojinn-io/GYO-Engine/pull/145)，**未合併**。它也沒有寫出；`EventSoundOffsetSeconds` 是「預覽與遊戲共用的唯一時刻公式」 | PR #145 的 `docs/framework/event-sounds.md`、`framework/include/gyo/framework/EventSounds.hpp` |
| 先例工具 | audio_editor（[PR #141](https://github.com/yojinn-io/GYO-Engine/pull/141)，未合併，GUI 試用延到本工具完成後一起做）：核心程式庫＋CLI＋ImGui 視窗、依賴守衛、寫 catalog；ui_editor（#132 等）：唯讀掛載、Undo | `docs/audio_editor/architecture.md`（#141 分支）、`tools/ui_editor/` |
| 測試用模型 | 共通的 FBX fixture 沒有；只有 PvP 的 `tests/object_fps_pvp/fixtures/hair_buns/Hair_Buns.fbx`（產品所有，工具的測試不能用） | `find tests -iname '*.fbx'` |

## Feature → Contract → Architecture（AGENTS §2）

### Feature（你看得到的）

1. **看模型**：從掛載的產品 catalog 選一個 `model` 資產，看節點樹、網格（頂點數、三角形數、骨骼數）、材質（名稱、底色）、片段（名稱、長度、軌道數）。
2. **看動作**：3D 預覽蒙皮後的網格與骨架；選片段、播放／暫停、拖時間、變速、循環；繞軌道轉鏡頭。
3. **調事件時刻**：在片段的時間軸上看 `gyo.anim_events` 的事件；新增、拖動、改名、刪除；Undo／Redo；存檔。播到事件時刻時，事件標記會亮。
4. **（等 #145）聽事件的聲音**：看每個事件配的聲音、`sync_seconds`、`gain`；播放時在事件時刻照遊戲的公式出聲；修改這三個值並存檔。

這些支援的使用者 Feature：PvP 換彈聲的時刻（`v8-tool-assets-anim-events` 留下的「等實聽」）不用手改 JSON；以後角色動作的事件也用同一個工具。

### Contract（要約定什麼）

- **不新增、不改任何契約。** 工具只是 `gyo.anim_events` v1（Engine）與 `gyo.event_sounds` v1（framework）的又一個產生者（AGENTS §8：手寫、腳本、工具都能產生）。
- **寫出的規則**：兩個格式都沒有寫出函式，工具自己寫文字，**寫之前把文字交給 Engine／framework 的 parser 重新解析並綁定**，通過才寫。工具不複製任何驗證規則，所以不需要另外的規則對齊測試；對齊靠「寫出的文字＝parser 接受、解析結果＝工具手上的資料」這一條往返測試（見判定）。
- **寫出會正規化排版**：手寫檔的空白、縮排、片段順序可能變（Engine 的 `AnimationEventSet` 依片段名排序）；值（秒數逐位元、名稱、順序）不變。
- **工具自己的檔**：沒有。工具不需要工程檔（事件檔本身就是可再編輯的資料）；視窗狀態不存（framework DI 4：不寫 `imgui.ini`）。

### Architecture（要怎麼撐）

- **新 Component**：`model_viewer` 工具（新 owner）。依賴：`model_viewer → framework → Engine`，以及 `model_viewer → Engine`（單一能力直接呼叫，framework DI 5）。不連結遊戲、不知道產品。Runtime 不知道工具（AGENTS §8）。
- **Architecture Delta**：只有「新增一個工具 owner」（`tools.csv` 一列、`tools/model_viewer/`、`tests/model_viewer/`、`docs/model_viewer/architecture.md`）。Engine 與 framework **不改**（若實作中發現要改，停下回報）。
- **3D 預覽怎麼畫**：決定點 `v8-tool-model-viewer-render`（見「等你決定」）。建議 (B′)：工具自己建一個**不認領視窗**的 GPU 裝置，用 Engine 的 `ModelRenderer`＋`Renderer` 畫到離屏貼圖並讀回 CPU，再上傳成 SDL 2D 渲染器的貼圖，用 ImGui 顯示。ImGui 外殼照舊在 SDL 2D 渲染器上，外殼不替客戶建渲染裝置（framework 的 Replacement Criteria 不碰）。
- **(B′) 的 Architecture Delta**（要你同意；選 (A) 就沒有）：
  1. `v8-engine-render-offscreen`：`GYO::RenderBackendSDLGpu` 的公開介面多一種建立方式：不認領視窗、畫到呼叫者指定大小的 RGBA8 sRGB 貼圖，每幀讀回最後畫好的結果。`IRenderDevice` 與 `Renderer` 的介面不改（離屏裝置的 `AcquireFrame` 交出自己的貼圖當畫面）。依賴方向不變；owner 是 Engine 的 render。
  2. `v8-engine-sdl-renderer-texture`：`SdlRenderer` 多一個中立的串流貼圖（建立、上傳 RGBA8、釋放）；framework 的 ImGui 綁定多一個把它交給 ImGui 的函式（回傳不透明的整數 id，標頭仍不含 ImGui 型別）。這補上 platform-sdl Replacement Criteria 2 裡「到 SDL 渲染器有貼圖 API 為止」的那個缺口，所以本工具不需要 `*Native`。ui_editor 改用它、拿掉自己的例外，**不在本批**（記成之後的候選）。
  3. 誰做：看 `v8-tool-model-viewer-engine-owner`。
- **Design Invariants 與 Replacement Criteria**：新 Component 要先寫（AGENTS §3）。草稿在下一節，核准後由 `-core` 寫進 `docs/model_viewer/architecture.md`。

## Design Invariants（`docs/model_viewer/architecture.md`，2026-11-04 同意，由 `-core` 寫入）

1. **依賴**：不連結遊戲、不提任何產品。可用任何 Engine 模組（(B′) 會用到 `GYO::Render`、`GYO::ModelRenderer`、`GYO::RenderBackendSDLGpu`，都不是 `*Native`）；兩個以上產品同樣組合的東西經 framework（`gyo.event_sounds` 用 framework 的 `ParseEventSounds`、`BindEventSounds`、`EventSoundOffsetSeconds`）。不 include `SDL3/`、不呼叫 `SDL_`、不連 `*Native` 或 SDL 目標；不 include `imgui_internal.h`。守衛：工具的依賴守衛（目標與 include 前綴的正面清單）。
2. **一個驗證者**：檔案合不合法只由 Engine（`ParseAnimationEvents`＋`BindAnimationEvents`）與 framework（`ParseEventSounds`＋`BindEventSounds`）決定。工具不留規則的副本；每次寫出前把要寫的文字重新解析並綁定到開著的模型，任何一步失敗就不寫。守衛：存檔路徑的測試＋突變。
3. **不失真**：載入後不編輯就存，解析結果和原檔相同（秒數逐位元相同；用最短往返的十進位表示）。編輯只改被編輯的那一筆。守衛：往返測試。
4. **只寫它編輯的檔**：開著的事件檔（和 #145 之後的 event_sounds 檔），用 `engine/io` 的原子取代寫入；載入後磁碟上的檔被別人改過就拒絕覆寫。模型檔、貼圖、產品的定義檔（例如武器的 presentation JSON）、catalog 的其他條目都不碰。catalog 只在「新建事件檔」時加一條 type `text`（`v8-tool-model-viewer-new-file` 選 (b)），用 framework 的 `RegisterAsset`；產品定義裡的引用由使用者手改。守衛：測試＋突變。
5. **預覽就是 Engine 的結果**：姿勢＝`SamplePose`（片段自己的秒數），蒙皮與繪製＝Engine 的 `ModelRenderer`＋`Renderer`（(B′)；選 (A) 時是 `SkinMesh`＋工具的投影）；工具沒有自己的取樣、內插或著色。變速只把牆鐘時間換成片段秒數，不進檔案。聲音的時刻只用 `EventSoundOffsetSeconds`。守衛：姿勢相等測試＋突變。
6. **中立**：事件名稱對工具是不透明的字串；不知道事件代表什麼，不依產品分支。守衛：審查＋依賴守衛（不提 `tools.csv`／`projects.csv` 裡的其他 id）。

### Replacement Criteria（2026-11-04 同意）

符合任一條就停止修補，交你決定修還是重建：

1. 預覽要工具自己畫（自己的投影、排序或著色）才夠用，或 Engine 的離屏繪製＋同步讀回＋上傳在判定 P1 達不到門檻。這時提出下一步讓你選（非同步讀回、縮小預覽、把 ImGui 綁定改到 SDL_GPU 等），不在工具裡補。（選 (A) 時這條改成：draw list 給不了貼圖或正確的深度，或 P1 不過，就提議 (B′)。）
2. 編輯的對象超出這兩個格式（碰撞體、animset、材質對應），工具要變成通用的模型編輯器。
3. 要預覽別的模型的片段（重定向、混合），也就是 anim_events 自己的 Replacement Criteria 2。
4. 工具得自己重寫某條格式規則，或每加一個功能都要重新驗證格式規則。

## 範圍與分批

```text
v8-tool-model-viewer                      「anim-events-line 的第二批：模型檢視工具」
│
├── 不依賴 #145
│   ├── v8-tool-model-viewer-core         「high｜Opus 5.5｜登錄、核心程式庫、CLI、anim_events 讀寫」
│   ├── v8-engine-render-offscreen        「high｜Opus 5.5｜（(B′)）不認領視窗的 GPU 裝置、離屏幀與讀回」
│   ├── v8-engine-sdl-renderer-texture    「high｜Opus 5.5｜（(B′)）SDL 2D 渲染器的串流貼圖＋ImGui 綁定交出貼圖」
│   ├── v8-tool-model-viewer-preview      「high｜Opus 5.5｜視窗、模型資訊、骨骼動畫預覽」
│   └── v8-tool-model-viewer-events       「high｜Opus 5.5｜時間軸上編輯事件時刻、Undo、存檔」
│
├── 依賴 #145（合併前不開工）
│   └── v8-tool-model-viewer-event-sounds 「high｜Opus 5.5｜event_sounds 的顯示、試聽、編輯」
│
└── v8-tool-model-viewer-trial            「medium｜Opus 5.5｜L2 量測＋L3 GUI 試用（和 audio_editor 一起）」
```

| 代號 | 做什麼 | 不做什麼 | 估時 |
|---|---|---|---|
| `-core` | `tools.csv` 一列（enabled、default 為 true；release 為 false，和 audio_editor 相同）、`project.json`、依賴守衛與自我測試、`docs/model_viewer/architecture.md`（Design Invariants）。核心程式庫：掛載產品內容（framework 的唯讀 catalog＋Engine 的 ufbx 載入器）、列 `model` 資產、模型摘要、事件文件的模型（載入、編輯操作、寫出、存檔）。CLI：`info <asset_root> <catalog> <model_id>` 印摘要；`events roundtrip <…> <events_id>` 讀→寫到暫存→重新解析比對 | 視窗 | 4～6 h |
| `v8-engine-render-offscreen` | （(B′)）`SdlGpuRenderDevice` 的離屏建立（不呼叫 `SDL_ClaimWindowForGPUDevice`）、離屏的 `AcquireFrame`／`SubmitFrame`、改變大小、讀回最後一幀；Engine 的測試（標籤 `gpu`，Linux CI 的 Lavapipe 與你的 Mac）；render 的文件寫明用法 | 非同步讀回（P1 不過時才排）；Windows 的 D3D12 不另驗（CI 沒有 Windows 的 GPU 步驟） | 4～6 h |
| `v8-engine-sdl-renderer-texture` | （(B′)）`SdlRenderer` 的串流貼圖（建立、上傳 RGBA8、釋放，錯誤用 `SdlRendererError`）；ImGui 綁定交出貼圖 id；platform-sdl 與 framework 文件的字句跟著改 | ui_editor 改用它（之後的候選） | 2～3 h |
| `-preview` | ImGui 視窗：選 catalog 與模型（Engine 的系統選擇窗，經 framework 外殼的 context）、資訊面板、3D 預覽（(B′)：載入 Engine 的著色器 bundle、`ModelRenderer`＋`Renderer` 畫到離屏、讀回、上傳、ImGui 顯示）、骨架疊加、鏡頭、播放控制 | 貼圖（材質名 → 貼圖的對應是產品的；只用材質底色當色調）；重定向 | 5～8 h |
| `-events` | 片段時間軸：事件標記、新增、拖動（可輸入數值）、改名、刪除、Undo／Redo、未存檔提示、存檔；播放經過事件時標記亮（`PlaybackInterval::Crossed`） | 事件的參數（格式沒有）；同時編多個模型 | 3～5 h |
| `-event-sounds` | 讀開著的事件檔對應的 `gyo.event_sounds` 檔；列出每個事件的聲音；播放時照 `EventSoundOffsetSeconds` 在 `GYO::Audio` 出聲；改 `sound_asset_id`、`sync_seconds`、`gain` 並存檔 | 聲音的剪裁（audio_editor 的事）；一個事件多個聲音（格式不允許） | 3～5 h（原估 12～20 h 之外） |
| `-trial` | L2 量測（P1、P2）；你在 GUI 上試用本工具與 audio_editor（#141） | — | L2 約 1 h；L3 是你的時間 |

估時合計（(B′)）：不依賴 #145 的部分 18～28 h（`-core` 4～6、兩個 Engine 批 6～9、`-preview` 5～8、`-events` 3～5），比原估 12～20 h 多 6～9 h，多的是兩個 Engine 批；加上 `-event-sounds` 21～33 h。選 (A) 時是 12～19 h（沒有 Engine 批）。第 1 版寫的 (B)「+15～25 h」沒有查證，撤回（見修訂紀錄）。照 HANDOFF 的規則，任一子批超過估時 1.5 倍、或單一問題累計超過 2 小時，就停下回報。

**依賴 #145 的部分**：`-event-sounds` 要 framework 的 `EventSounds.hpp` 與 JSON 綁定的 `ParseEventSounds`；在 #145 合併進 master 之前不開工、不在本分支 cherry-pick #145。#145 若在合併前改了介面，開工時以合併後的版本重讀，判定裡和介面有關的字句跟著改（只改名稱、不改判定的意思；意思要變就停下問你）。

## 判定（都通過才算完成）

試行次數：除了 P1，所有測試都是決定性的，**不重試**；失敗就修（照 §14.3：先 Red 後 Green）或停下回報。CI 上偶發的失敗照 §14.1 切分原因並記錄，不靠重跑蓋掉。

### `-core`

- C1：L1 的 ci-windows、ci-linux、ci-macos 三列綠；`model_viewer` 的測試（標籤 `cpu`）全部通過。
- C2：寫出往返（DI 3）。合成資料（程式裡建的 `ModelAsset`，`ValidateModel` 通過，不用任何產品的檔）上：
  - 多片段、同一時刻不同名、同名不同時刻、0 秒、正好等於片段長度的秒數、需要 17 位有效數字才能往返的秒數（例如 `0.1+0.2`）；
  - 寫出的文字經 `ParseAnimationEvents` 接受、`BindAnimationEvents` 綁定成功，結果與寫出前**逐欄相同**（秒數以位元比較）。
- C3：一個驗證者（DI 2）。編輯造成以下任一情況時，存檔被拒、磁碟上的檔不變（位元組比較）：秒數超過片段長度、負數、名稱不合 Engine 的規則、同一時刻同名重複、片段名不在模型裡。拒絕的原因是 Engine 回的錯誤路徑（例如 `clips."A"[0].seconds`），工具不自己判斷。
- C4：只寫它編輯的檔（DI 4）。存檔只動事件檔本身；同目錄的其他檔、catalog（不新建時）位元組不變。載入後被外部改過的檔，存檔被拒、外部的內容留著。
- C5：`info` 的輸出對合成模型逐行符合預期（節點數、網格的頂點／三角形／骨骼數、材質名、片段名與長度）。
- C6：依賴守衛（DI 1）：自我測試通過；加一個遊戲目標、`SDL3/` include、`imgui_internal.h` include 時都失敗。

#### `-core` 實作後的結果（2026-11-04）

- 本機 macOS（`build/target/_build/mv`，`GYO_TOOLS=model_viewer`）：整棵樹的 `cpu|shader` 56／56 通過，其中 `gyo_model_viewer.*` 4／4（core 的測試 14 個 case、157 個 assertion）。
- 件數の突き合わせ：一致。
  - 宣告 7 條，全部 killed。
  - 第一輪 13／13 killed；審查後補了 5 條，第二輪 18／18 killed，stale 0。
  - 證據在使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/core/`（`mutations.txt`、`mutations-round1.txt`、`ctest.txt`、`ctest-all.txt`、`pvp-explore.txt`）。
- 探索（不是判定 P2，P2 在 `-trial` 正式做）：PvP 的 nailer 事件檔 `roundtrip` 後逐位元組相同。
- 審查（Opus 5.5／high，子 agent）：沒有擋住這批的錯誤。指出的問題都在本批修掉了：
  - 存檔後要核對 stamp 和寫入的位元組；
  - 新建事件檔時要檢查模型 id；
  - `info` 要綁定到模型之後才說檔案合法；
  - 守衛抓不到遊戲的一般目標名（遊戲的目標不是 `Name::Name` 的形式），已收緊；
  - DI 6 的守衛字句要回到核准的版本（「不提已登錄的其他 id」），已實作；
  - CLI 成功的路徑、`SameEvents`、「只改被編輯的那一筆」、存檔不動 catalog、catalog 寫失敗，這幾項補了測試。
- 和事前宣告不同的地方：
  - CLI 的形式改成 `info --asset-root … [--asset-catalog …] --model <id>` 與 `roundtrip … --events <id>`，另加 `models`。宣告原本寫的是 `info <asset_root> <catalog> <model_id>`、`events roundtrip`；改成選項的形式，是為了和 audio_editor 一致，而且 `--asset-catalog` 可以省略（省略時掛載整個 content root）。P2 照舊跑往返，只是命令名稱換成 `roundtrip`。
  - `roundtrip` 在記憶體裡寫出、交給 Engine 解析，**不寫暫存檔**。宣告原本是「寫到暫存再重新解析」。這樣它和存檔走同一個 `CheckedText`，而且 DI 4（只寫它編輯的檔）不用為 CLI 開例外。比對的內容不變。
  - 工具帶一個自己的 fixture `tests/model_viewer/fixtures/animated_triangle.fbx`：一個三角形加一個片段，由 Engine 測試裡的 ASCII FBX 組成，不屬於任何產品。
- 範圍的限界多兩項：
  - 測試「catalog 寫失敗」靠把目錄改成唯讀。唯讀目錄仍寫得進去的主機（Windows 不看這些權限位元，root 也不受限）上，這個 case 會跳過並印出訊息，不算通過也不算失敗。
  - 突變 `mv-info-without-binding` 經 ctest 跑 CLI 測試，`--build-dir` 要用 `build/target/_build/mv`。
- 發現（不在本批範圍，只報告）：audio_editor 的依賴守衛也只看 `Name::Name`，抓不到遊戲的一般目標名。

### `-preview`

- V1：預覽的姿勢＝`SamplePose(model, clip, t, mode)` 的結果（每個節點的全域矩陣逐元素相等），t 取片段的 0、中間、結尾，與變速 0.5／2.0 下經過同一牆鐘時間後換算出的 t（DI 5）。
- V2：（(B′)）繪製經 Engine：預覽的每一幀都是 `Renderer::Render` 對離屏裝置畫出、`ReadTexture` 讀回的結果；上傳的位元組和讀回的相同（逐位元組）。（選 (A) 時改成：合成的兩個重疊三角形，近的畫在後；鏡頭繞到背面時順序反過來。）
- V3：沒有片段的模型、沒有網格只有骨架的模型都能開，不當機；載入失敗的模型顯示 Engine 回的錯誤，不顯示舊模型的內容。
- V4：C1、C6 照舊。

### `v8-engine-render-offscreen`（(B′)）

- R1：離屏裝置在**別的東西已經佔用視窗**（SDL 2D 渲染器）時仍能建立，且不認領視窗；建立、畫、讀回、銷毀之後，視窗的 SDL 2D 渲染器照常 `Clear`／`Present`。
- R2：清成已知顏色的離屏幀，讀回的每個像素等於該顏色的 sRGB 編碼（容差 0：RGBA8 的同一值）。
- R3：深度正確：合成的兩個重疊、交錯的四邊形（不同顏色、已知深度），讀回後指定的取樣點是近的那個的顏色；鏡頭轉到背面時反過來。
- R4：改變大小後，讀回的寬高是新的大小；畫面進行中（`AcquireFrame` 之後、`SubmitFrame` 之前）讀回被拒（和現在的 `ReadTexture` 規則相同）。
- R5：視窗模式的既有測試全部照舊通過（`render.*`）。
- 平台：R1～R5 在 Linux CI（Lavapipe）與你的 Mac（Metal，L2）各跑一次；Windows（D3D12）不驗（範圍的限界）。

#### `v8-engine-render-offscreen` 實作後的結果（2026-11-04）

- R1～R4：在你的 Mac（Metal）通過，`render.sdl_gpu_offscreen`。R5：`render.*` 3／3 通過。Linux（Lavapipe）：L1 run 41821955996 通過。
- 介面：`SdlGpuRenderDevice::CreateOffscreen(SdlPlatform&, SdlGpuOffscreenOptions)`、`SetFrameSize`、`ReadFrame`。`IRenderDevice` 與 `Renderer` 沒有改。
- 平台仍要傳入：Vulkan 載入驅動時需要 SDL 的視訊子系統，它由平台持有（platform-sdl DI 8）。
- R1 的觀測方式：SDL 把「已認領」記在視窗自己身上，同一個視窗不能認領兩次。所以「離屏裝置還在時，視窗模式的裝置仍能建立」就證明離屏裝置沒有認領。
- 和宣告不同的地方：
  - 讀回只有在某個 pass 真的寫進幀貼圖時才可用；放棄的幀、沒寫進幀貼圖的幀、改了大小還沒送出新幀時，讀回都會被拒。
  - 離屏的 `AcquireFrame` 不等之前的幀；不每幀讀回的呼叫端要自己控速（標頭寫明）。
- 突變：宣告 4 條，加上審查後的 1 條，共 5／5 killed。

### `v8-engine-sdl-renderer-texture`（(B′)）

- T1：建立、上傳、釋放串流貼圖；大小或位元組數不對時回錯誤，不寫越界。這些用 SDL 的軟體渲染器跑（`cpu`，三平台 CI）。
- T2：ImGui 綁定交出的 id 在客戶的整個生命期內有效；貼圖釋放後不再交出。
- T3：platform-sdl 的 Replacement Criteria 2 與 framework 文件的字句改寫，要你同意（是 Design Invariants 節的變更）。

### `-events`

- E1：編輯操作（新增、移動、改名、刪除）後，資料仍是 Engine 會接受的排序（同一片段內秒數不減）；移動跨過別的事件時順序跟著改。
- E2：Undo／Redo 回到每一步之前／之後的狀態（逐欄相同）；存檔後 Undo 會讓「未存檔」再出現。
- E3：拖動時標記停在 [0, 片段長度] 的邊界上（這是 UI 的行為，寫進檔的是使用者最後停下的值）。數值輸入超出範圍時不夾住，照 C3 的路徑由 Engine 拒絕存檔（anim_events DI 4「不修補」）。
- E4：播放經過事件時刻時，該事件標記亮起，判斷只用 `PlaybackInterval::Crossed`（循環跨界時也亮一次）。

### `-event-sounds`（#145 合併後）

- S1：讀寫往返與一個驗證者，同 C2、C3，換成 `ParseEventSounds`／`BindEventSounds`；`gain` 往返後經 float 捨入相同。
- S2：試聽的開始時刻＝`EventSoundOffsetSeconds(事件秒數, 片段長度, 片段長度 ÷ 播放速度, sync_seconds)`；測試以假的音訊輸出記錄排程時刻，和直接呼叫該函式的結果逐位元相同（DI 5）。
- S3：event_sounds 檔指向的 anim_events id 不是開著的那一份時，拒絕顯示並說明，不猜。

### `-trial`

L2（本機，CI 做不到或不好觀測的；你的 Mac，Release 建置）：

- P1（決定所選做法是否夠用，不是本批的成敗）：PvP catalog 的 4 個 `model` 資產，各播最長的片段 600 幀，預覽大小 1280×720，記錄每幀的時間。(B′)：「取樣＋`ModelRenderer` 更新＋`Renderer::Render`＋讀回（含等 GPU）＋上傳」；(A)：「取樣＋蒙皮＋投影＋排序＋送進 draw list」。**門檻：每個模型的 p95 ≤ 16.7 ms**。試行 1 次，不重試。不通過就停下，照 Replacement Criteria 1 交你決定；不放寬門檻。量測的件數突き合わせ：每個模型應記錄 600 筆，少了就是「不一致」。
- P2：PvP 實際資料的往返：對 PvP catalog 裡每個 `gyo.anim_events` 檔跑 `events roundtrip`，解析結果全部相同。（這是 L2 不是 L1：工具的測試不能假設 PvP 存在，AGENTS §7。）

L3（你操作；和 audio_editor 的 GUI 試用同一場）：

1. 開 PvP 的 catalog，選 `object_fps_pvp.model.nailer_v2`，資訊面板的片段有 `Armature|Reload`。
2. 預覽播換彈：網格與骨架跟著動，變速與拖時間正常，看得出彈匣入槽的時刻。
3. 在 `Armature|Reload` 把 `magazine_in` 拖到別的時刻、存檔、重開工具，時刻是存的值；PvP 遊戲裡換彈聲跟著變（實聽）。之後改回或保留由你決定。
4. 新增一個事件、改名、刪除、Undo／Redo；故意拖到超出片段長度再存，看到被拒的訊息。
5. 用 audio_editor 做一個聲音，（#145 合併後）在本工具把它配到事件上、試聽、存檔，遊戲裡聽到。
6. 關閉時有未存檔提示。

突變（D184：每批只跑相關的；file 與 find 在實作後、執行前寫進 `docs/tools/plans/v8/scripts/model_viewer_mutations.json`；PR 線合併前在背景完整跑一次；全部預期 killed）：

| 子批 | 突變 |
|---|---|
| `-core` | 寫出時漏掉最後一個事件／秒數只寫 6 位有效數字／存檔前不經 Engine 解析／存檔不檢查外部修改／寫到臨時檔後不換名（非原子）／守衛的遊戲目標規則失效（自我測試失敗）／工具 include `SDL3/`（守衛失敗） |
| `v8-engine-render-offscreen` | 離屏建立仍認領視窗（R1 失敗）／讀回上一幀的貼圖而非這一幀／關掉深度測試（R3 失敗）／改變大小後沿用舊貼圖 |
| `v8-engine-sdl-renderer-texture` | 上傳不檢查位元組數／釋放後仍交出 id |
| `-preview` | 預覽用綁定姿勢（不取樣）／把變速乘進片段秒數兩次／上傳前漏掉讀回（顯示舊幀） |
| `-events` | 移動後不重新排序／Undo 不還原名稱／`Crossed` 換成「當下 ≥ 事件」（循環時漏亮或連續亮） |
| `-event-sounds` | 試聽用工具自己的公式（漏掉 `− sync_seconds`）／存檔前不經 `ParseEventSounds`／`gain` 以 double 比較而非 float |

## L1／L2／L3 的分工

| 層 | 做什麼 | 在哪 |
|---|---|---|
| L1（主要標準） | 所有決定性的判定：C1～C6、V1～V4、T1～T2、E1～E4、S1～S3；R1～R5 只在 Linux 列 | CI 三平台跑 `cpu\|shader`；Linux 列另跑 `gpu`（Lavapipe） |
| L2 | P1（量測）、P2（PvP 實際資料往返）、R1～R5 在 Metal 上、相關突變 | 你的 Mac（主 checkout 建置） |
| L3 | GUI 試用 1～6，和 audio_editor 一起 | 你的 Mac（看 `v8-tool-model-viewer-trial-platform`） |

L1／L2／L3 不排固定順序，依機器與你的空檔。

## 證據位置

- 主 checkout 的 `build/evidence/tool-v8-model-viewer/<子批>/`（git 不追蹤）：P1 的逐幀 CSV 與摘要、P2 的輸出、突變結果 `mutations.txt`、§14.3 的 `red.txt`／`green.txt`。
- dev_log：每個子批完成時和該批同一變更寫 `docs/dev_logs/2026_11_xx_tool_v8_model_viewer_<子批>.zh-Hant.md`，寫結論與判定用的數值，並寫證據在哪台機器的哪裡。
- 不放 home、`/tmp` 或會話的暫存區。

## 模型與檔位（`agents/effort-gearbox.md`、`agents/navigator.md`）

| 子批 | 上段／下段與根據 | 實作者 | 審查者 | 檢驗者 |
|---|---|---|---|---|
| `-core` | 上段：新 Component 的 Design Invariants、寫出 Data Contract 的檔 | Opus 5.5／high（主會話） | Opus 5.5／high（子 agent，code review） | 只在出現放寬時開，Opus 5.5／high |
| `v8-engine-render-offscreen` | 上段：Engine 公開介面、GPU 同步（讀回的時機） | Opus 5.5／high（主會話；讀回時機的部分局部 xhigh 要你同意） | Opus 5.5／high（子 agent） | 只在出現放寬時開，Opus 5.5／high |
| `v8-engine-sdl-renderer-texture` | 上段：Engine 公開介面、platform-sdl 的 Replacement Criteria 改寫 | Opus 5.5／high（主會話） | Opus 5.5／high（子 agent） | 同上 |
| `-preview` | 下段：工具內的繪製與播放，不碰契約 | Opus 5.5／high（主會話） | Opus 5.5／high（子 agent） | 同上 |
| `-events` | 下段：用 `-core` 的寫出，不改它 | Opus 5.5／high（主會話） | Opus 5.5／high（子 agent） | 同上 |
| `-event-sounds` | 上段：寫出 framework 格式的檔 | Opus 5.5／high（主會話） | Opus 5.5／high（子 agent） | 同上 |
| `-trial` | 下段：執行與記錄 | Opus 5.5／medium（主會話） | — | — |

- 守衛是新的（不是改既有守衛的判定），§14.4 不適用；若實作中要動 framework 或其他工具的守衛，停下回報。
- 審查者和實作者同模型：這幾批的風險在「寫出」，而寫出的對齊靠 Engine／framework 的 parser 往返檢查，不靠審查者的判斷。要換成不同模型（Fable 5.1）見 `v8-tool-model-viewer-reviewer`。
- 起動子 agent 前照規則先在主會話說明模型、檔位、類型、交給它的資料。

## PR 與分支

- 本分支 `claude/v8-tool-model-viewer-77f310`（切自 master `4918b1fa`）。`-core`、`-preview`、`-events` 在同一條 PR（fewer PRs）；選 (B′) 且 `-engine-owner` 選 (a) 時，兩個 Engine 批也在這條 PR，排在 `-preview` 之前。
- `-event-sounds` 進不進同一條 PR，看 `v8-tool-model-viewer-sounds-timing`。
- audio_editor 的 #141 不併進來；兩條 PR 各自合併，L3 同一場試。

## 範圍的限界

- 預覽沒有貼圖（只有材質底色當色調），因為材質名 → 貼圖的對應是產品的；(A)、(B′) 相同。(B′) 的光照與著色照 Engine 的 `Renderer`。
- (B′) 的離屏繪製只驗 Linux（Lavapipe）與 macOS（Metal）；Windows（D3D12）不驗。同步讀回每幀約 3.7 MB（1280×720），會等 GPU；夠不夠快由 P1 決定。
- 只看模型自己的片段；PvP 角色用別的 FBX 的動作重定向，本工具看不到重定向後的樣子（anim_events 的 Replacement Criteria 2）。
- 碰撞體不在本批（assets.md：格式 owner 由產品定，之後併進模型檢視）。
- CI 不驗 GUI 與實際音訊；GUI 只靠 L3，音訊只靠 L3 的實聽。
- P1 只在你的 Mac 量；Windows、Linux 的幀時間不量。

## 等你決定

（2026-11-04 全部決定，見「決定紀錄」；下表保留選項與理由。）

每項：機制、取捨、建議。「會改變」寫最後一欄。

| 代號 | 問題 | 選項與取捨 | 建議 | 會改變 |
|---|---|---|---|---|
| `v8-tool-model-viewer-render` | 3D 預覽怎麼畫 | (A) CPU 蒙皮＋Engine Math 投影＋畫家排序，ImGui draw list 畫三角形：Engine、framework 都不改，估時最少；但沒有深度緩衝（手和槍交疊的地方可能畫錯，正好是看換彈時刻的地方）、畫法和遊戲不同、工具要自己做投影與排序。(B′) 工具自建不認領視窗的 GPU 裝置，用 Engine 的 `ModelRenderer`＋`Renderer` 畫到離屏、讀回、上傳成 SDL 2D 渲染器的貼圖：深度正確、畫法和遊戲同一條路、工具不寫繪製；代價是兩個小的 Engine 批（+6～9 h，Engine 公開介面的 Architecture Delta）、每幀同步讀回（約 3.7 MB，等 GPU）、Windows 的 GPU 路徑不驗。離屏繪製與 SDL 渲染器的貼圖 API 歸 Engine 是對的：前者也能用在縮圖與無視窗測試，後者補上 platform-sdl Replacement Criteria 2 已記下的缺口。(B) ImGui 綁定整個搬到 SDL_GPU：要改 framework 的外殼與綁定，沒有估、不建議。(C) 只畫骨架線與線框：看不出彈匣這種網格上的時刻 | (B′)（第 1 版建議 (A)，改變的原因：(B) 的代價沒查證就拿來比；查證後 (B′) 只多 6～9 h，而 (A) 在交疊處會畫錯，正好是要看的地方） | `-preview` 的做法與估時；有沒有兩個 Engine 批與 Architecture Delta |
| `v8-tool-model-viewer-engine-owner` | （選 (B′) 才有）兩個 Engine 批誰做 | (a) 本線做，Architecture Delta 記在本文件與 render／platform-sdl 文件（先例：`v8-engine-input-composition` 由工具線做）：一條 PR 線、等待最少。(b) 交給架構會話：歸屬更清楚，但要等它排程，`-preview` 被擋 | (a)：兩批都小、範圍是本工具直接需要的 | `-preview` 何時能開工；PR 的範圍 |
| `v8-tool-model-viewer-new-file` | 能不能替還沒有事件檔的模型新建一份 | (a) 只編已登錄的事件檔：工具不寫 catalog，最簡單；新模型要先手寫一份最小的檔並登錄。(b) 也能新建：工具寫事件檔並在 catalog 加一條 type `text`（和 audio_editor 一樣用 framework 的 `RegisterAsset`）；產品的定義裡要引用它（例如 `animation_events_asset_id`）仍由你手改，工具不碰產品檔。(b) 對應 assets.md 的「工具與 Asset 只有一個交接：放進資產根目錄並登錄」 | (b)：角色動作的事件下一步就會用到，手寫第一份是多餘的步驟 | DI 4 的字句、`-core` 多約 1 h、C4 多一條測試 |
| `v8-tool-model-viewer-sounds-timing` | `-event-sounds` 放哪條 PR | (a) #145 在 L3 試用之前合併就進同一條 PR，否則另開一條小 PR：不讓 #145 擋住前三批。(b) 等 #145 合併再一起送：L3 一次試完，但前三批可能等很久。(c) 一律另開 PR：PR 數多一條 | (a) | L3 是一場還是兩場；PR 數 |
| `v8-tool-model-viewer-trial-platform` | L3 在哪個平台 | macOS（你的開發機，IU-2 的先例；audio_editor 的試用也在 macOS）／再加 Windows（主戰場是 Windows Client，但這是開發工具；Windows 只多驗系統選擇窗與 IME，已有 ui_editor 驗過） | 只 macOS | L3 的場次數 |
| `v8-tool-model-viewer-reviewer` | 審查者的模型 | Opus 5.5（同實作者，用量少；寫出靠 parser 往返檢查）／Fable 5.1（不同模型，盲點不重疊；比主會話大，要你同意，用量多） | Opus 5.5 | 用量；`-core`、`-event-sounds` 的審查角度 |
| `v8-tool-model-viewer-declaration` | 本宣告（判定、門檻、突變、L3、範圍限界、Design Invariants 草稿、模型表）照草稿核准嗎 | 照草稿／修改後核准 | — | 能不能開工 `-core` |

## 修訂紀錄

- 第 2 版（2026-11-04）：你認為 (B′)「Engine 加離屏繪製＋讀回 CPU，再上傳成 SDL_Renderer 的貼圖」是好方案（經分支會話轉述）。查證後：
  - 第 1 版把 (B) 寫成「framework 綁定改 SDL_GPU、+15～25 h」當成定論，沒有查證，撤回。
  - 讀回已有（`ReadTexture`、`RequestSceneCapture`），缺的是不認領視窗的幀；`ModelRenderer` 已有，工具不必自己畫。
  - 轉述說 (B′)「不動 framework」，查證後要動一處：上傳成 SDL 2D 渲染器的貼圖需要貼圖 API，現在只有 ui_editor 的 `*Native` 例外；第二個工具再要 `*Native` 就觸發 platform-sdl Replacement Criteria 2。所以 (B′) 加上 `v8-engine-sdl-renderer-texture`（Engine 的貼圖 API＋ImGui 綁定交出貼圖），framework 的外殼不動。
  - Linux CI 有 Lavapipe 的 `gpu` 步驟，離屏繪製能在 CI 上驗（Linux）。
  - 建議改為 (B′)，估時、判定、突變、模型表、範圍的限界、Replacement Criteria 1 跟著改。

## 決定紀錄

| 代號 | 決定 | 使用者原話 |
|---|---|---|
| `v8-tool-model-viewer-declaration` | （2026-11-04）本宣告第 2 版照草稿核准：判定、門檻（P1 p95 ≤ 16.7 ms，試行 1 次）、突變、L1／L2／L3、範圍的限界、Design Invariants 1～6 與 Replacement Criteria 1～4、模型表。實作等使用者說開始 | 「沒有，照建議核准」 |
| `v8-tool-model-viewer-render` | （2026-11-04）(B′)：Engine 離屏繪製＋讀回＋上傳成 SDL 2D 渲染器的貼圖；Architecture Delta 為 `v8-engine-render-offscreen` 與 `v8-engine-sdl-renderer-texture` | 同上（使用者先前經分支會話轉述：(B′)「是好方案」） |
| `v8-tool-model-viewer-engine-owner` | （2026-11-04）(a)：兩個 Engine 批由本線做，同一條 PR，排在 `-preview` 之前；Delta 記在本文件與 render／platform-sdl 文件 | 同上 |
| `v8-tool-model-viewer-new-file` | （2026-11-04）(b)：可新建事件檔並在 catalog 登錄 type `text`；產品定義的引用由使用者手改 | 同上 |
| `v8-tool-model-viewer-sounds-timing` | （2026-11-04）(a)：#145 在 L3 試用前合併就進同一條 PR，否則另開小 PR | 同上 |
| `v8-tool-model-viewer-trial-platform` | （2026-11-04）L3 只在 macOS | 同上 |
| `v8-tool-model-viewer-reviewer` | （2026-11-04）審查者 Opus 5.5／high | 同上 |
| `v8-tool-model-viewer-start` | （2026-11-04）開工，先寫事前宣告 | 「現在開工先寫宣告」（經啟動本會話的會話轉述） |

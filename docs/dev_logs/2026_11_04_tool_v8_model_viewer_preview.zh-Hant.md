# 工具 v8：v8-tool-model-viewer-preview

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`（PR #146），接在 `v8-engine-sdl-renderer-texture` 之後。

- 段：下段（工具內的繪製與播放，不碰契約）。依賴守衛的判定變更依 AGENTS §14.4 處理，見下文。
- 實作者：Opus 5.5／high（主會話）。
- 審查者：Opus 5.5／high（子 agent，一次）。
- 檢驗者（§14.4）：Opus 5.5／high（子 agent），和審查者、實作者都不是同一個 agent。
- 估時 5～8 h；實際約 3.5 h（不含你試用的時間）。

**狀態：**
- 程式、測試、突變、審查都完成，守衛的判定變更你已核准。
- L1 的結果記在 HANDOFF。

相關文件：
- [事前宣告](../tools/plans/v8/model-viewer.md)：「`-preview`」與「`-preview` 實作後的結果」；
- [守衛的判定變更](../tools/plans/v8/model-viewer-guard-judgment-change.md)；
- [docs/model_viewer/architecture.md](../model_viewer/architecture.md)「Window」。

## 做了什麼

- `gyo_model_viewer window`（gui 變體，成為預設）：
  - 跑在 framework 的 ImGui 外殼上；
  - 內容可以從命令列指定，也可以用系統的資料夾選擇窗打開；
  - 有模型清單，並顯示 Engine 對模型的描述；
  - 播放：片段、播放／暫停、拖時間、變速、循環；
  - 3D 預覽：拖曳轉鏡頭、滾輪拉遠拉近、「Frame model」、骨架開關。
  - cli 變體執行 `window` 時，說明這個版本沒有視窗，並回傳 3。
- 核心程式庫（`cpu` 可測）：
  - `PlaybackClock`：牆鐘時間乘上變速，換成片段秒數；
  - `PreviewPose`：`SamplePose`，沒有片段時用 `MakeDefaultPose`；
  - `ModelSession`：打開失敗時，不保留之前的模型。
- `gyo_model_viewer_preview`：
  - `PreviewRenderer`：Engine 的 `ModelRenderer` 加 `Renderer` 畫到離屏裝置，每幀用 `ReadFrame` 讀回；
  - 材質的底色當色調；
  - 骨架是 `WorldOverlay` 層上的細長方塊；
  - `OrbitCamera` 與 `FrameModel` 決定鏡頭位置；
  - `UploadFrame` 把讀回的畫面上傳成 SDL 渲染器的貼圖。
- Engine 的內建著色器在每次建置時複製到執行檔旁邊的 `shaders/builtin`。
- 依賴守衛：
  - 正面清單加 7 個目標；
  - 目標名改成取完整的 CMake 名稱；
  - 自我測試從 10 個增加到 13 個。

## 驗證與經過

- 本機 macOS，`build/target/_build/ui`（`GYO_TOOLS=ui_editor;model_viewer`，含 PvP）：
  - `gyo_model_viewer.*` 5／5 通過，含 GPU 的 `gyo_model_viewer.preview`（Metal）；
  - `cpu|shader` 117／117 通過。
- 件數の突き合わせ：一致。
  - 宣告的 3 條，加上審查後的 `mvp-bone-no-pitch`，共 4 條，4／4 killed。
  - 守衛改了，所以既有的 3 條 `mv-guard-*` 也重跑，3／3 killed。最後一輪合計 7／7。
  - 第一輪：宣告的 3 條 3／3 killed，`mv-guard-*` 3／3 killed。
  - 第二輪：審查修正改了變速那一行，`mvp-speed-twice` 的 find 字串跟不上，變成 stale。更新 find 後重跑，就是最後一輪的 7／7。預期訊息沒有改。
- 依賴守衛（§14.4）：
  - 1～5 寫在判定變更裡；
  - 檢驗者用 78 棵合成樹比對了新舊守衛。它找到一個舊漏洞：允許的名稱後面接 `-` 或 `.` 的名稱也會通過。這次一併收緊；
  - 你核准了（「核准，提交 -preview」）。
- 審查的指出與處理：
  - （中）新模型的姿勢取樣失敗時，預覽還留著舊模型。修正：失敗時清空預覽。
  - （中）骨架的測試只確認「能畫」，沒檢查畫出什麼。修正：用一根沿 X 軸的骨頭，檢查中線的左右兩側都畫出骨頭、上下兩側沒有。加了突變 `mvp-bone-no-pitch`。
  - （低）預覽的錯誤訊息不會清掉。修正：上傳成功就清掉。
  - （低）只改著色器時，執行檔旁邊的副本不會更新。修正：改成每次建置都用 `copy_directory_if_different` 複製。
  - （低）高密度螢幕上預覽會模糊。修正：畫面用像素大小、圖片用 ImGui 的單位。
  - （低）變速不是有限數字時的處理。修正：當成 0。
  - 沒有改的兩點，理由如下：
    - 改大小失敗時每幀都會重試：不會當機，而 `SetFrameSize` 只在大小為 0 或幀進行中時才會失敗；
    - 鏡頭取景用剛體的位置：蒙皮網格的範圍可能和 Engine 畫出的略有不同，但這只影響鏡頭放在哪裡，不影響畫出的內容。
- GUI：你用 PvP 的內容開了三次（`window-trial*.txt`），沒有問題，log 裡沒有錯誤。聲音還沒有，屬於 `-event-sounds`。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/preview/`，有：
  - `mutations.txt`（7／7）、`mutations-round1.txt`、`mutations-round2.txt`（含 stale）、`mutations-guard.txt`；
  - `guard-verify/`（檢驗者的比對腳本）；
  - `ctest-viewer.txt`、`ctest-all.txt`、`window-start.txt`、`window-trial*.txt`。

## 和事前宣告不同的地方

- V2 只有 L2，沒有 L1：CI 的 GPU 步驟只跑 `render.*`。
- V2 讀回用的是 `ReadFrame`，不是 `ReadTexture`。
- gui 變體成為預設。

## 發現（不在本批範圍）

- CI 的 GPU 步驟只跑 Engine 的 `render.*`。工具的 GPU 測試要放進 L1，就得改 CI 的範圍，由 CI 的負責方決定。

# 輸入與呈現：批次計畫

更新：2026-10-04。狀態：**未開始**（IP-1、IP-2 都未開始）。先讀 [進度與執行規則](README.md)、[交接](HANDOFF.md)。

本文件寫兩批的範圍、驗收、平台表與 Architecture Delta。現況與證據的完整敘述在 [HANDOFF](HANDOFF.md)「正式項目清單」。
file:line 以 master `05042fa` 為準，批次開始時重新核對。

## 共通規則（兩批都適用）

- 每次只做使用者指定的批次；一批一個 PR（commit 與 PR 用日語）；開始、里程碑、停止時更新 README／HANDOFF／dev_log 後停止，不自動開始下一批。
- 先凍結來源、產物與分析器再量測；開發與乾淨量測不同時進行；失敗跑次保留，先有限定位；長測另外授權。
- **權威不變的證明**：兩批都不改權威結果。正式證明是同機兩樹（base／branch）digest 比對；CI 只做自洽檢查與不依賴 libm 的 golden 子集（yaw＝pitch＝0、軸向牆）。不修改共通 workflow。
  digest 的情境 runner 由提出需求的消費端提供，它不經過 SDL 輸入與呈現，所以對本計畫只是附帶檢查；runner 尚未合併時標「未執行」並寫明理由。
- **量測基線世代**：B0＝消費端在阻塞取得下的量測基線（含拖動重現），只作歷史參照；IP-2 本身也在自己的 base commit 上以同一流程量 before／after；IP-2 合併後由消費端重取 B1；之後各批在自己的 base commit 上以凍結工具量 before／after。消費端既有穩定基線的數字只用來對門檻，不作回歸比較。
- **檔位**：主對話檔位由使用者決定。ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意（D8）。xhigh 只用在局部。
- **遷移清單**：每批同步更新未啟用產品（`object_fps`、`object_fps_v2`、`tools/object_fps_preview`）的遷移清單，寫入 [foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md) 的「IP」節（本夾不另建），格式沿用 [Math](../math-foundation/inactive_products.md)。未啟用產品本身不修改。
- **合併順序**：只是建議，用來減少衝突與方便歸因，不是依賴；後合併的一方 rebase。render 建議 FF-2 → IP-2 → FF-4 → FF-7；`InputActionMap.cpp` 的 IP-1 與 FF-3 不同時進行、先後不限。依賴欄只寫真正的依賴；例外是 FF-2 必須先於 IP-2（`MeshUpdateSmoke` 語意），FF-7 依賴 IP-1、IP-2、FF-4、FF-5。
- **匿名（D12）**：本文件只寫「提出需求的消費端」，不寫產品名、不連結消費端文件；程式檔案路徑與證據路徑作為資料保留。
- 平台表的六列固定；「預定執行」是計畫，不是結果。

---

## IP-1：輸入層：完整 scancode 與視窗互動事件

狀態：未開始。

### 目標與範圍

讓消費端不必讀原生 `SDL_Event` 就能取得任意鍵與視窗互動事件。

做：

- `engine/input/include/engine/input/PhysicalInputFrame.hpp:8-24` 的 `Key`（14 鍵）以加法擴充為完整鍵盤 scancode。
  - 保留既有名稱與列舉值；既有 `InputActionMap::Bind(…, Key)` 的呼叫不需修改。
  - `Key` 只在程式碼中使用，沒有資料檔以名稱引用它（規劃時已檢查），擴充不涉及 Data Contract。
- 同一幀內的按鍵與點擊不遺失：
  - 每幀提供按下上升沿的次數（或依序的事件清單），保留「同一幀內按下又放開」的上升沿。現況 `SdlInput.cpp:64-72` 的 `ApplyTransition` 在同一幀按下又放開時把 `pressed` 覆寫成 false，上升沿消失；消費端目前自行以原生事件補（`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:209-217` 的 `pendingShotEdge`）。
  - 保留事件的先後順序（例如視窗互動、取得指標與點擊在同一幀內的先後），消費端的擷取政策依賴它。
  - 點擊當下的座標，以及點擊是否在視窗內。
  - 表示方式（計數或事件清單）在介面審查時決定；限制是既有 `pressed`／`released`／`held` 的語意不變。
- 視窗互動事件：移動、縮放（含新寬高，消費端需要更新寬高）、縮小、還原。消費端在失焦、移動、縮放、縮小時都會釋放指標。失焦已由 `PhysicalInputFrame::windowFocused` 提供（`SdlInput.cpp:80,99-107`），沿用。
- 只屬本視窗的過濾：`SdlInput` 已對鍵盤、滑鼠、焦點事件比對本視窗 ID（`SdlInput.cpp:96-139`），這部分不是缺口。缺的是：視窗互動事件（移動、縮放、縮小、還原）的 Engine 表示與本視窗過濾；以及 `SdlPlatform::PumpEvents`（`SdlPlatform.cpp:61-83`）只對關閉請求過濾本視窗，傳給 observer 的原生事件沒有過濾（observer 本身維持原樣，見「不做」）。
- SDL 轉換放在 `engine/input/backend/sdl/src/SdlInput.cpp`；`engine/platform/sdl` 只在必要時修改。
- 遷移清單加入 preview 的 `SDL_SCANCODE_1..6`（`tools/object_fps_preview/ViewmodelPreview.cpp:74-82`）。preview 目前無法建置（依賴停用的 `object_fps`），只寫清單，不改程式。

不做：

- 何時擷取或釋放指標（Tab、點擊擷取、失焦釋放）：留在各消費端。
- 移除 `NativeEventObserver`（`SdlPlatform.hpp:43,56`）：ui_editor 的 ImGui（`ImGuiSdlRendererHost.cpp:218`）、未啟用產品與 preview 仍經由它取得原生事件。
- 文字輸入與剪貼簿（候選，D11②）。
- 消費端改用 Engine 輸入：由消費端自己的批次執行。
- 修改未啟用產品或 preview 的程式碼。

### 交付

- `engine/input` 公開介面擴充與 `SdlInput` 轉換。
- `tests/common/input` 測試：全鍵轉換表、視窗事件序列（含縮放的新寬高）、其他視窗的事件被過濾、既有 14 鍵行為不變、同一幀按下再放開仍保留上升沿（次數或事件順序）、點擊座標與是否在視窗內。
- 遷移清單更新、dev_log（`docs/dev_logs/YYYY_MM_DD_engine_ip1.zh-Hant.md`）、本文件與 HANDOFF 更新。

### 驗收點

- L1（自動）：
  - `tests/common/input` 新增測試與既有 `SdlInputTests`、`InputActionMapTests` 通過；新增測試包含「同一幀按下再放開」的上升沿保留。
  - 依賴邊比對：沒有新邊。
  - CI 四平台通過；ui_editor 建置並通過測試。
  - 權威 digest：兩樹比對（消費端 runner 已合併時）；本批不碰模擬，只是附帶檢查。
- L2（實機）：本批無。實機操作在消費端改用輸入時執行。
- L3（人工）：本批無。

### 平台表

| 平台 | 預定 | 說明 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | 本機建置與 L1；不做原生操作（在消費端遷移時做） |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 預定執行 | `tests/common/input` 於四平台執行 |
| Linux lavapipe GPU | 預定執行 | 既有 `gpu` 標籤測試作回歸；本批不新增 GPU 測試 |
| Windows D3D12 實機 | 未執行 | 沒有實機；本批不涉及呈現 |
| Linux 實機 | 未執行 | 沒有實機 |
| macOS arm64 實機 | 未執行 | 沒有實機；CI 的 macos-arm64 只跑 L1 |

### 建議檔位

- high：跨檔案修改，要先讀懂 platform、input 與消費端三層；不涉及時序決定性，不需 xhigh。
- 公開介面設計建議以 ultracode 審查（約 3 方案、1 評審、1 次對抗式檢查）：公開介面屬高風險改動。依 D8，開始時說明並徵求同意。

### 依賴

- 無前置批次。
- 下游：消費端改用 Engine 輸入；IP-2（只在 D7 選「暫停 acquire」時）；FF-7（include 統一排在本批之後）。
- 與 FF-3 都可能修改 `engine/input`（FF-3 改 `InputActionMap.cpp` 的 FNV-1a）：這是合併順序建議，不是依賴；不同時進行，後合併的一方 rebase。

### Architecture Delta

1. 需求：輸入層缺口（HANDOFF 第11項）。遊戲（Tab、視窗事件、同一幀的點擊上升沿與座標）與工具（數字鍵）兩個獨立消費端都繞過 Engine；過去 F3、H、R 都是為產品需求直接改 Engine 的 enum。
2. 問題：`Key` 是 14 鍵的封閉清單；`ButtonState` 每幀只有一組 `pressed`／`released`，同一幀按下又放開的上升沿會消失；視窗互動事件只能經 `NativeEventObserver` 取得原生 `SDL_Event`，Engine 沒有對應表示。
3. 邊界：`engine/input` 公開介面（`Key`、`PhysicalInputFrame` 或新增的視窗事件表示）與 SDL 轉換擴充。擷取政策的邊界不變，留在消費端。
4. 影響：
   - Engine：`engine/input`、`engine/input/backend/sdl`，必要時 `engine/platform/sdl`。
   - `tests/common/input`。
   - 啟用中的消費端：提出需求的消費端（改用在其自身批次）；ui_editor 只需重新編譯。
   - 未啟用產品與 preview：寫進遷移清單。
5. 依賴方向：不變（消費端→Engine）。`gyo_input` 目前不連結任何模組，`gyo_input_sdl` 已連結 `GYO::Input`、`GYO::PlatformSDL`、`SDL3::SDL3`；本批不新增邊。
6. Ownership：鍵盤與視窗事件的轉換歸 Engine；何時擷取或釋放指標歸消費端。
7. 更小的方案：每次為某個產品在 enum 加一個鍵，正是已觀測到的漂移本身；只加註解或文件無法讓工具停止繞過 Engine。

Fitness：Engine 不出現產品名；刪除任何消費端不需修改 `engine/input`；沒有新 Top-level。

### 完成條件

- L1 全部通過；CI 四平台通過；依賴邊無變化。
- 遷移清單、HANDOFF、README、dev_log 已更新。
- 平台表照實標示。

### 停止條件

- 擴充無法以加法完成（必須改既有名稱、列舉值或移除 `NativeEventObserver`）：停下，帶影響範圍請使用者決定。
- 需要新增依賴邊，或必須修改 `engine/platform/sdl` 的公開介面超出事件轉換：停下並補寫 Delta。
- 發現消費端的行為必須改變才能通過測試：停下，屬消費端批次的範圍。

---

## IP-2：呈現不阻塞主迴圈

狀態：未開始。

### 目標與範圍

讓視窗拖動／縮放期間，主迴圈的事件處理、模擬與命令產生不被 swapchain 取得卡住。依 D7 先量測再選修法。

已查證的事實（master `05042fa`）：

- 目前的呼叫：`engine/render/backend/sdl_gpu/src/SdlGpuRenderDevice.cpp:937` 的 `SDL_WaitAndAcquireGPUSwapchainTexture`（阻塞）。
- 既有 `Skipped` 路徑：取得結果為空時 `:942-946` 回傳 `std::nullopt`，`engine/render/src/Renderer.cpp:318` 轉成 `PresentStatus::Skipped`。
- 主迴圈是單執行緒：`engine/runtime/src/RuntimeLoop.cpp:18-44` 依序 `ProcessEvents` → `Update` → `Render`。
- SDL 版本：`third_party/sdl3/CMakeLists.txt` 的 `GIT_TAG release-3.4.0`。
- SDL 3.4.0 Metal 後端（`src/gpu/metal/SDL_gpu_metal.m`）：
  - `:3880-3899`：不阻塞模式只略過 in-flight fence 的等待（fence 未完成時直接回傳 true、不給 texture）。
  - `:3908`：兩種模式都會呼叫 `[layer nextDrawable]`。沒有可用 drawable 時，這個呼叫最多阻塞約 1 秒（Apple `CAMetalLayer` 的逾時），與消費端觀測到的約 1199 ms 吻合。
  - `:3922`：一律回傳非空的 texture container，即使 drawable 為 nil。所以 Metal 上的長幀仍回報 `Presented`。
  - `METAL_WaitForSwapchain`（`:3816-3840`）只等 fence，不呼叫 `nextDrawable`。
- 結論：只把呼叫換成不阻塞的 `SDL_AcquireGPUSwapchainTexture`，在 Metal 上**不一定**能解除停頓；必須先量出停頓在哪一段。

做：

1. **拆分量測**：在 IP-2 分支分別計時 `SDL_WaitForGPUSwapchain`（只等 fence）與 `SDL_AcquireGPUSwapchainTexture`（含 `nextDrawable`），在 macOS 拖動／縮放時重現。量測碼的去留在量測後決定。
2. 依 D7 分支：
   - **停頓在 fence**：改為不阻塞取得；取不到時走既有 `Skipped`（`SdlGpuRenderDevice.cpp:937,942-946` → `Renderer.cpp:318`），並加節流避免忙等（例如 `SDL_WaitForGPUSwapchain` 或 frames-in-flight 限制，於本批決定並記錄理由）。
   - **停頓在 `nextDrawable`**：停下（見停止條件）。
3. 待查並記錄：Metal 長幀仍回報 `Presented` 時，Engine 如何辨識「沒有真正呈現」。
4. `Skipped`、節流的新語意由 Engine 層的假 device 單元測試與 `MeshUpdateSmoke`（FF-2 已改為容許 `Skipped`）定義，不依賴產品驗收。
5. 語意變更寫進 render 的公開註解與本文件。

不做：

- 未經使用者選擇就把模擬與呈現分到不同執行緒。
- 宣稱 Windows 已解決：Windows 拖動標題列可能進入 Win32 modal 迴圈，停頓可能在事件泵而不是 render，本批沒有量測。
- 修改消費端對 `Skipped` 的處理；消費端的呈現分析器若需修改，屬消費端批次。
- 修改 ui_editor（使用 SDL_Renderer，不受影響）。

### 交付

- 拆分量測紀錄（停頓位置、各段時間分布）與 D7 判斷。
- 停頓在 fence 時：不阻塞取得＋節流的實作、假 device 單元測試、render 註解更新。
- 遷移清單更新（未啟用的 sdl_gpu 產品 `object_fps`、`object_fps_v2` 會看到更常見的 `Skipped`）、dev_log（`docs/dev_logs/YYYY_MM_DD_engine_ip2.zh-Hant.md`）、本文件與 HANDOFF 更新。

### 驗收點

- L1（自動）：
  - 假 device 單元測試：取得結果為空時迴圈繼續、`Skipped` 計數正確、節流生效（不忙等）。
  - `MeshUpdateSmoke` 在 Linux lavapipe 通過。
  - CI 四平台建置（Metal 只做離線編譯，D3D12 沒有執行）。
  - 權威 digest 兩樹比對不變（附帶檢查）。
- L2（macOS Intel／Metal 實機）：
  - 拆分量測：拖動、縮放時分別記錄 fence 等待與 `nextDrawable` 的時間。
  - 修正後：在消費端以雙 GUI 拖動、縮放各 10 次，記錄最長主迴圈停頓、10 秒窗口的 Held 比例（門檻 5% 不變）、是否出現 `CONNECTION POOR`。
  - before：在 IP-2 自己的 base commit 上，以同一凍結工具、同一流程（拖動、縮放各 10 次）重量。B0 是使用者在場的人工重現，不一定照這個流程，只作歷史參照。
  - 乾淨 GUI 短測的可見 P50／P95 不退化；消費端的 `Skipped` 計數與呈現分析器在 `Skipped` 常見時仍正確。
  - L2 必須在合併前完成（D13）。若分析器需要修改才能處理常見的 `Skipped`：IP-2 停下；先由消費端做驗收工具修正批（在 IP-2 的 base commit 上，仍是阻塞取得）；修正合併後，IP-2 rebase 到含工具修正的 master，以新凍結的工具在新的 base commit 重量 before，再跑 L2。
  - 不與消費端改用 Engine 輸入那一批的實機量測同時進行，以免無法歸因。
- L3（人工）：使用者在 macOS 拖動、縮放視窗，確認不再出現 `CONNECTION POOR`。

### 平台表

| 平台 | 預定 | 說明 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | 拆分量測、L2、L3 |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 預定執行 | 建置與單元測試；macOS 只做 Metal 離線編譯，Windows D3D12 沒有執行 |
| Linux lavapipe GPU | 預定執行 | `MeshUpdateSmoke` 與新增的 `gpu` 標籤測試；CI 唯一跑 GPU 測試處 |
| Windows D3D12 實機 | 未執行 | 沒有實機；拖動標題列可能停在事件泵（Win32 modal 迴圈），根因可能不同，本批不宣稱已解決 |
| Linux 實機 | 未執行 | 沒有實機；Vulkan 實機的取得行為未量測 |
| macOS arm64 實機 | 未執行 | 沒有實機 |

### 建議檔位

- 主體 high：跨檔案修改，要讀懂 render device、Renderer 與 RuntimeLoop；拆分量測與 L2 的執行、記錄也用 high。
- 局部 xhigh：只限 swapchain 取得、節流與 `Skipped` 語意。錯了會靜默改變延遲與掉幀數字；各後端行為不同，本機只能驗 Metal。
- 若 D7 選「分執行緒」，屬更大的改動，需重新規劃並另訂檔位。超過主對話檔位時依 D8 徵求同意。

### 依賴

- FF-2（`MeshUpdateSmoke` 改為容許 `Skipped`，先在現行阻塞取得下通過）。這是依賴，不只是合併順序。
- 消費端條件：需要一個 sdl_gpu 產品在 macOS 的拖動重現與阻塞下的基線 B0（目前由提出需求的消費端在其量測基線批次提供）。
- 若 D7 選「暫停 acquire」：另需 IP-1 的視窗事件。
- 下游：消費端重取 B1；FF-7 依賴本批。FF-4 排在本批之後只是合併順序建議（後合併的一方 rebase），不是依賴。

### Architecture Delta

1. 需求：渲染阻塞主迴圈（HANDOFF 第3項）。消費端的 L3 人工驗收觀測到 render 內約 1.2 秒停頓；停頓期間不產生移動命令，Host 以 Held 替代，10 秒窗口的 Held 達 12%（門檻 5%），觸發連線品質警告。
2. 問題：`AcquireFrame` 以阻塞方式取得 swapchain；`RuntimeLoop` 是單執行緒，整個迴圈跟著停住。
3. 邊界：Render device 的取得語意從「等到有 drawable」改為「可能回傳 `Skipped`」。`Skipped` 從罕見變常見，這是 Runtime 對所有 sdl_gpu 消費端的行為契約。
4. 影響：
   - Engine：`engine/render/backend/sdl_gpu`；`Renderer` 的 `Skipped` 路徑只讀確認；`engine/runtime` 只讀。
   - `tests/common/render`（`MeshUpdateSmoke` 已由 FF-2 先改）。
   - 啟用中的 sdl_gpu 消費端只有提出需求的消費端；另有共通 GPU 測試。
   - ui_editor 使用 SDL_Renderer（`tools/ui_editor/project.json` 的 `SDL_RENDERER`），不受影響。
   - 未啟用產品 `object_fps`、`object_fps_v2`（`project.json` 宣告 `SDL_GPU`）：寫進遷移清單。
5. 依賴方向：fence 方案不變。若選「暫停 acquire」，視窗事件到 render 的傳遞不得新增 `render → input` 的邊；路徑在使用者選擇後設計，並補寫 Delta。
6. Ownership：不變。`Skipped` 已是 Engine 既有概念（`PresentStatus`），取得策略與節流歸 sdl_gpu 後端；收到 `Skipped` 後怎麼做歸消費端。
7. 更小的方案：消費端無法繞過 Engine 的阻塞呼叫。分執行緒是更大的 Delta，只作為 D7 的選項之一，由使用者決定。

Fitness：Engine 不出現產品名；刪除提出需求的消費端後，新語意仍由假 device 單元測試與 `MeshUpdateSmoke` 定義。

### 完成條件

- 拆分量測已記錄，D7 的分支有證據。
- 停頓在 fence 時：L1、L2、L3 依平台表在合併前完成（D13）；before 為本批 base commit 上的重量；Held 比例與 `CONNECTION POOR` 結果照實記錄。
- 遷移清單、HANDOFF、README、dev_log 已更新；Windows／Linux 實機與 macOS arm64 實機標「未執行」。

### 停止條件

- **停頓在 `nextDrawable`（SDL 內部）**：停下，帶拆分量測證據請使用者選擇：
  - A'：視窗移動／縮放期間暫停 acquire，回傳 `Skipped`（需 IP-1 的視窗事件）。
  - B：分執行緒（模擬與呈現分離，屬更大的 Architecture Delta）。
- 量到停頓在事件泵而不在 render：停下回報，不擴大成執行緒模型變更。
- 消費端的分析器需要修改才能處理常見的 `Skipped`：停下（D13）。先由消費端在本批的 base commit 上做驗收工具修正批（仍是阻塞取得），再回到本批：rebase 到含工具修正的 master，以新凍結的工具重量 before 後跑 L2；L2 未通過不合併。消費端在重取 B1 時才暴露的問題（本批 L2 已通過）屬消費端自己的停止路徑，不在本批。
- 需要修改 SDL 原始碼或升級 SDL：停下，請使用者決定。
- 乾淨 GUI 短測的可見延遲退化，或權威 digest 改變：停下回報，不放寬門檻。

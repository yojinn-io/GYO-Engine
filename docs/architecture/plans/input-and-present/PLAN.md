# 輸入與呈現：批次計畫

更新：2026-10-21（IP-3／IP-4 計畫核准，D31，見文末）；2026-10-05。狀態：IP-1、IP-2 完成；IP-3 進行中（ip3a）。先讀 [進度與執行規則](README.md)、[交接](HANDOFF.md)。

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
- **合併順序**：只是建議，用來減少衝突與方便歸因，不是依賴；後合併的一方 rebase。render 實際為 FF-2 → FF-4 → FF-7 → IP-2（2026-10-05 使用者決定 FF-7 先於 IP-2）；`InputActionMap.cpp` 的 IP-1 與 FF-3 不同時進行、先後不限。依賴欄只寫真正的依賴；例外是 FF-2 必須先於 IP-2（`MeshUpdateSmoke` 語意），FF-7 在 IP-1、FF-4、FF-5 之後。
- **匿名（D12）**：本文件只寫「提出需求的消費端」，不寫產品名、不連結消費端文件；程式檔案路徑與證據路徑作為資料保留。
- 平台表的六列固定；「預定執行」是計畫，不是結果。

---

## IP-1：輸入層：完整 scancode 與視窗互動事件

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ip1.zh-Hant.md)。

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

狀態：完成（2026-10-05，縮小交付）：IP-2b live frame 與取得的拆分診斷；IP-2c 依 D19 不實作，延到分執行緒的計畫。L2 結果見 [HANDOFF](HANDOFF.md) 的 IP-2 紀錄。

### 2026-10-05 範圍擴大：拖動＋縮放（D16）

使用者決定 IP-2 同時處理拖動標題列與縮放兩種停頓。本節是擴大後的計畫；下方原有各節仍適用於拖動（render 側），與本節衝突處以本節為準。

已查證的事實（SDL 3.4.0 原始碼，靜態）：

- macOS 縮放：
  - 拖動右下角時，Cocoa 在 `[NSApp sendEvent:]` 內執行 live resize 的追蹤迴圈，直到放開滑鍵才返回。這個呼叫位於 `SDL_PollEvent` → `Cocoa_PumpEventsUntilDate`（`src/video/cocoa/SDL_cocoaevents.m:588-624`）之內，所以縮放期間 `SDL_PollEvent` 不返回。
  - B0 縮放的 394.9／2974.4／1006.4 ms 落在消費端的事件處理計時內，與此吻合：停頓長度約等於使用者按住縮放的時間。
  - SDL 在 `windowWillStartLiveResize:` 掛上 60 Hz 的 `NSTimer`（`src/video/cocoa/SDL_cocoawindow.m:1140-1152`，`NSRunLoopCommonModes`，追蹤迴圈中也會觸發），呼叫 `SDL_OnWindowLiveResizeUpdate`（`src/video/SDL_video.c:4302-4313`）。沒有使用 main callbacks 時，它送出 `SDL_EVENT_WINDOW_EXPOSED`（`data1`＝1）。
  - `SDL_PushEvent` 在入列前同步呼叫 event watcher（`src/events/SDL_events.c:1800-1806`），`SDL_events.h:139` 也寫明這個事件「可以直接在 event watcher 中重繪」。所以縮放期間，唯一能在主執行緒上執行一幀的地方，是 event watcher。
- Windows：`WM_ENTERSIZEMOVE`（拖動與縮放都會進入 modal 迴圈）設定計時器，`WM_TIMER` 呼叫同一個 `SDL_OnWindowLiveResizeUpdate`（`src/video/windows/SDL_windowsevents.c:1869-1901`）。所以在 Windows，拖動與縮放都走這條路徑（靜態分析，沒有實機）。
- macOS 拖動標題列：沒有 live resize 計時器（`windowWillMove:` 只設定旗標，`SDL_cocoawindow.m:1160-1166`）；B0 的停頓在 render，事件泵沒有停住。所以在 macOS，**拖動與縮放的原因不同**，修法分成兩部分。
- Engine 目前沒有 event watcher，也沒有使用 main callbacks；`RuntimeLoop` 是單執行緒，依序 `ProcessEvents` → `Update` → `Render`。

做（三部分，同一條功能線、同一個 PR）：

1. **IP-2a 拆分量測**（先做，不改語意）：
   - 拖動：分別計時 `SDL_WaitForGPUSwapchain`（fence）與 `SDL_AcquireGPUSwapchainTexture`（含 `nextDrawable`）。
   - 縮放：分別計時 `SDL_PollEvent` 與消費端回呼，並記錄停頓期間收到的 `EXPOSED`（`data1`＝1）事件數與時間戳，確認停頓就是追蹤迴圈。
   - 量測碼的去留在量測後決定。
2. **IP-2b 縮放：live frame**（停頓在事件泵；macOS 縮放、Windows 拖動與縮放）：
   - `SdlPlatform` 註冊一個 event watcher。只有本視窗、`EXPOSED` 且 `data1`＝1、在主執行緒、而且不在 live frame 之中時，才呼叫消費端設定的 live frame 處理函式。
   - `RuntimeLoop` 新增「live frame」：在 `ProcessEvents` 尚未返回時，執行一次 `Update` → `Render`。它沿用同一個 frame 編號與時間基準，不再呼叫 `ProcessEvents`；live frame 中要求 Stop 時，在 `ProcessEvents` 返回後結束迴圈。
   - 是否啟用由消費端決定（opt-in）：不設定處理函式時，行為與現在相同。`IRuntimeClient` 介面不變；契約寫進公開註解：live frame 中沒有新的輸入，`Update`／`Render` 可能在 `ProcessEvents` 的呼叫堆疊內被呼叫。
   - 提出需求的消費端在同一個 PR 中接上處理函式（消費端的部分），並確認它的 `Update`／`Render` 可以在這個時點被呼叫。
3. **IP-2c 拖動**（render 側）：照下方原計畫與 D7。停頓在 fence → 不阻塞取得＋`Skipped`＋節流；停頓在 `nextDrawable` → 停下，請使用者在 A'（暫停 acquire）與 B（分執行緒）之間選擇。

不做（追加）：

- 不改用 SDL main callbacks：那會改變所有消費端的進入點與主迴圈擁有權，屬更大的變更。
- 不在 live frame 中處理輸入事件：期間到達的事件照常入列，在 `ProcessEvents` 返回後處理（Windows 進入 modal 迴圈時 SDL 會重設鍵盤，`SDL_windowsevents.c:1893-1894`）。

驗收點（追加，其餘見下方「驗收點」）：

- L1：
  - `RuntimeLoop` 單元測試：
    - live frame 的 `Update`／`Render` 次數與 frame 編號連續；
    - 時間差不為負，且與外層幀的總和連續；
    - live frame 中要求 Stop 時，在 `ProcessEvents` 返回後結束；
    - 沒有設定處理函式時，呼叫順序與現在逐一相同。
  - `SdlPlatform` 測試（dummy video driver，以合成事件驅動）：
    - 只有本視窗的 `EXPOSED`（`data1`＝1）會觸發；`data1`＝0、其他視窗、其他事件都不觸發；
    - 處理函式內再送出事件時不會重入。
- L2（macOS Intel／Metal 實機，需要使用者操作）：
  - 縮放 10 次、拖動 10 次，before／after 都量；記錄事件處理與 render 的最長停頓、10 秒窗口的 Held 比例（門檻 5%）、是否出現 `CONNECTION POOR`。
  - 縮放期間的 live frame 數與幀間隔。

平台表（追加）：Windows 的拖動與縮放都會走 IP-2b 的路徑，但沒有實機，標「未執行」，不宣稱已解決；Linux（X11／Wayland）沒有 live resize 計時器，行為未量測。

建議檔位（追加）：
- IP-2b 的重入與時間連續性局部 xhigh，與 IP-2c 的取得／節流／`Skipped` 一起，交給 1 個 xhigh 審查 agent 做對抗式審查。依 D8，使用者已同意這個 agent（2026-10-05）。
- 其餘 high。

Architecture Delta（IP-2b，七點）：

1. 需求：縮放期間主迴圈停在事件泵（B0：394.9～2974.4 ms），Host 以 Held 替代，窗口 14.8%（門檻 5%）。
2. 問題：作業系統在 `SDL_PollEvent` 內執行 modal 追蹤迴圈；單執行緒的 `RuntimeLoop` 在那段期間沒有任何地方可以前進。
3. 邊界：
   - `RuntimeLoop` 從「依序三段」擴充為「`ProcessEvents` 期間可以插入 live frame」，這是新的執行時契約，只對 opt-in 的消費端生效。
   - `SdlPlatform` 新增 event watcher 與處理函式。
4. 影響：
   - Engine：`engine/runtime`（`RuntimeLoop`）、`engine/platform/sdl`（`SdlPlatform`）、`tests/common` 的對應測試。
   - 提出需求的消費端：接上處理函式。
   - 其他消費端（ui_editor、`tests/common/runtime_sdl`、未啟用產品）不設定處理函式，行為不變；未啟用產品寫進遷移清單。
5. 依賴方向：不新增邊。`SdlPlatform` 已依賴 `engine/runtime`（`RuntimeControl`）；處理函式是 `std::function`，platform 不認識 `RuntimeLoop`；`RuntimeLoop` 不認識 platform。由消費端把兩者接起來。
6. Ownership：何時插入 live frame 由 platform（知道作業系統事件）決定；live frame 做什麼由 `RuntimeLoop` 與消費端決定；消費端是否啟用由消費端決定。
7. 更小的方案：
   - 消費端無法在 `SDL_PollEvent` 內前進，必須由持有 SDL 的 platform 提供掛點。
   - 只在 platform 掛點、讓消費端自己呼叫 `Update`／`Render`，會讓每個消費端各自複製時間與 Stop 的處理，所以把 live frame 放進 `RuntimeLoop`。
   - main callbacks 與分執行緒都是更大的 Delta。

停止條件（追加）：

- 量測顯示縮放期間沒有收到 `EXPOSED`（`data1`＝1），或停頓不在追蹤迴圈內：停下回報。
- 消費端的 `Update`／`Render` 必須重新設計才能在 `ProcessEvents` 的堆疊內被呼叫：停下。這部分改由消費端自己的批次處理，IP-2b 的 Engine 部分照常以 L1 定義。
- live frame 中 Metal 取得也停住（例如 `nextDrawable` 在縮放中逾時）：照實記錄，與 IP-2c 一起請使用者選擇。

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
- 下游：消費端重取 B1。FF-7 原本排在本批之後，2026-10-05 使用者決定 FF-7 先做，本批建在搬移後的 master 上（include 根目錄為 `engine/<m>/`）。

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

---

# IP-3／IP-4（2026-10-21 核准，D31）

**狀態：核准（D31：U1～U10 照建議）；IP-3 完成（ip3a～ip3f，含 L3；宣告 D32，判定變更 D35、D36）；下一步 IP-4。** 設計已寫成架構文件 [platform-sdl.md](../../platform-sdl.md) 與 [input.md](../../input.md)（使用者要求，提前到 ip3a）。 2026-10-21 以 ultracode 規劃（2 位盤點、3 個獨立方案、1 位評審合成），再經 1 位 xhigh 公開介面審查（README 的 IP-3 列）與 1 次對抗式檢查。審查的必修項目已併入本節；各項的處理見 [HANDOFF](HANDOFF.md)「IP-3／IP-4 規劃紀錄」。待使用者決定的事列在本節最後。
file:line 以 master `6fbb6a9` 為準，子批開始時重新核對。SDL 原始碼的引用以 SDL 3.4.0（`third_party/sdl3`）為準。

## 共通規則的補充（IP-3、IP-4）

- **PR**：IP-3、IP-4 與提出需求的消費端的遷移**同一個 PR**（D29，見 HANDOFF）。消費端早已決定「標頭去 SDL、PRIVATE 連結與產品遷移必須一起合併」，也符合 D18（以功能線為單位）。IP-3、IP-4 是 PR 內的里程碑，各自寫 dev_log。消費端的工作、L2 與估時寫在消費端自己的計畫，本文件只列「需求來源→契約」。
- **ui_editor**：在 `engine/config/tools.csv` 是啟用狀態。Engine 的修改讓它編不過時，本 PR 只做機械修改（改 include、成員呼叫改自由函式、改連 `*Native` 目標）；其餘只寫遷移清單（U2）。
- **README 方針 3 的更正**：README 的 IP-3 列（README:42）已把文字輸入與剪貼簿納入 IP-3，取代方針 3「不在本計畫」與 D11②。
- **相容性**：輸入幀以加法擴充；但下列公開符號會移除或移到 `*Native`，屬於 source-breaking，逐一列在「移除的符號」表。IP-1 既有測試的機械改寫（include、呼叫形式）不算修改判定。
- **事前宣告**：機率性的測試（ip3e 的並行日誌橋、ip4b 的並行解構）在 ip3a 宣告試行次數、成功判定與重試規則（AGENTS §14.1）。突變只有「預期的訊息」才算殺掉。

## 範圍：README 原列 vs 規劃新增

README 的 IP-3、IP-4 列之外，規劃時依需求加入下列項目，在 ip3a 關卡一次取得核准（U3）：

| 新增項目 | 需求來源 | 子批 |
|---|---|---|
| `PlatformName()`（只供診斷，不用來分支） | 消費端的啟動日誌寫平台名（`apps/object_fps_pvp/main.cpp:119-120`） | ip3e |
| `TextEvent` 的 `layoutKey`、`KeyModifiers` | 位址輸入框的 Ctrl+A／Ctrl+V 依配置判斷（`PvpApplication.cpp:233-250`） | ip3d |
| `GYO::PlatformSDLMain`（OBJECT library） | README 的「進入點」；靜態庫裡的 `main` 不會被抽出 | ip3e |
| 相對滑鼠模式的查詢（放在 `SdlPlatform` 的 `WindowFlags`） | probe 6 處（`gui_main.cpp:161,952,964,970,976`、`gui_quad_main.cpp:74`）；probe 只拿得到 `SdlPlatform&` | ip4a |
| `VideoDriverName()`、refresh rate | probe 的平台指紋（`platform_fingerprint.hpp`） | ip4a |
| X11 window number | `native_window.hpp:19`（只有這一處） | ip4a；或列為例外，見 U7 |

## 移除與移到 Native 的符號

| 符號（現在） | 替代 | 受影響的呼叫端 |
|---|---|---|
| `SdlPlatform::NativeEventObserver`（`SdlPlatform.hpp:44`） | `EventObserver`（`const PlatformEvent&`）＋ Native 的 `NativeEvent()` | `PvpApplication.cpp:940`、`ImGuiSdlRendererHost.cpp`、`tests/common` |
| `SdlPlatform::NativeWindow()`（`:75`） | Native 的自由函式 `NativeWindow(const SdlPlatform&)`；IP-4 起產品改用查詢 API | `PvpApplication.cpp:1205`、probe（`gui_main.cpp` 27 處等）、`ImGuiSdlRendererHost.cpp:190,198,206,218,249` |
| `SdlInput::HandleEvent(const SDL_Event&)`（`SdlInput.hpp:32`） | `HandleEvent(const PlatformEvent&)`；Native 的 `HandleNativeEvent(SdlInput&, const SDL_Event&, Time::TimePoint)`（**沒有預設時間**） | `PvpApplication.cpp:940`、`SdlInputTests.cpp:16,32` |
| `SdlRenderer::NativeRenderer()`（`SdlRenderer.hpp:63`） | Native 的 `NativeRenderer(const SdlRenderer&)` | `ImGuiSdlRendererHost.cpp:190-249` |

## IP-3：SDL 隔離核心

### 推導起點（Feature → Contract → Architecture）

| 需求來源 | 證據 | Engine 契約 |
|---|---|---|
| 位址輸入：打字、按住 Backspace 連刪、Ctrl+A／V、Return／Esc（只在大廳） | `PvpApplication.cpp:186-190`、`:233-250`、`:940-945` | 文字輸入開關；`text` 內依到達順序的文字與按鍵；剪貼簿 |
| 下一版的音效：以輸入事件的時間戳排程（消費端需求，見 HANDOFF） | — | `InputEvent` 帶 Engine 時間基準的時間戳 |
| 產品不直接呼叫 SDL（消費端的完成條件以 SDL 符號判定） | `PvpApplication.cpp:940-945`；`main.cpp:9,24-50,95,103-120` | 中立的 `PlatformEvent`；進入點；執行檔目錄；平台名；SDL→Base 日誌橋 |

`TextEvent` 不帶時間戳：沒有功能需要它（AGENTS §2），延後。

### 目標與範圍

做：

- 後端的一般公開標頭（`SdlPlatform.hpp`、`SdlInput.hpp`、`SdlRenderer.hpp`、`SdlGpuRenderDevice.hpp`）不直接、也不間接 include SDL，不出現 `SDL_` 符號。帶 SDL 型別的 API 移到另一個 include 根下的 `*Native.hpp`，每個後端一個 Native 目標。
- 時間戳在 `SdlPlatform::PumpEvents` 換算：每次 pump 取一個時鐘配對。
- 文字輸入與剪貼簿以加法加入 IP-1 的輸入幀。
- 執行檔目錄（名稱見 U8 前的 F7 處理）、平台名、進入點、SDL→Base 日誌橋。

不做：移除 SDL；main callbacks；文字編輯政策（和擷取政策一樣留在消費端）；PRIVATE 連結（IP-4）；視窗查詢（IP-4）。

### 公開 API 草圖（ip3a 定案）

```cpp
// SdlPlatform.hpp (no SDL)
namespace Engine::Platform::Sdl {
class NativeAccess;                                   // defined only in SdlPlatformNative.hpp
class PlatformEvent final {                           // valid only during the observer call
public:
    PlatformEvent(const PlatformEvent&) = delete; PlatformEvent& operator=(const PlatformEvent&) = delete;
    [[nodiscard]] Time::TimePoint Timestamp() const noexcept;   // see "時間戳的契約"
private:
    PlatformEvent(const void* native, Time::TimePoint timestamp) noexcept;
    friend class NativeAccess; friend class SdlPlatform;
    const void* native_{}; Time::TimePoint timestamp_{};
};
class SdlPlatform final {
public:
    using EventObserver = std::function<void(const PlatformEvent&)>;
    using LiveFrameHandler = std::function<void()>;              // unchanged
    static Base::Result<std::unique_ptr<SdlPlatform>, SdlPlatformError> Create(const SdlPlatformOptions& = {});
    [[nodiscard]] Runtime::RuntimeControl PumpEvents(const EventObserver& = {});   // may nest; each pump has its own clock pair (D33)
    void SetLiveFrameHandler(LiveFrameHandler);
    [[nodiscard]] Base::Result<std::string, SdlPlatformError> ClipboardText() const;   // main thread; empty -> Ok("")
private:
    struct Impl; std::unique_ptr<Impl> impl_;                  // SDL_Window*, WatchEvent, clock pair, pumping flag
    friend class NativeAccess;
};
// Main thread, or before other threads start (SDL caches without a lock).
[[nodiscard]] Base::Result<std::filesystem::path, SdlPlatformError> BaseDirectory();   // SDL_GetBasePath semantics, UTF-8 -> char8_t path
[[nodiscard]] std::string_view PlatformName() noexcept;                                 // diagnostics only
// SdlLogBridge.hpp (no SDL): opt-in RAII; one at a time (GYO_ASSERT)
struct SdlLogBridgeOptions final { bool keepSdlOutput = true; };   // default: tee, as the consumer does today
class SdlLogBridge final { public: explicit SdlLogBridge(SdlLogBridgeOptions = {}) noexcept; ~SdlLogBridge(); };
// EntryPoint.hpp (no SDL)
int GyoMain(int argc, char* argv[]);                  // defined by the product; argv is UTF-8 on every platform
// SdlInput.hpp (no SDL)
void HandleEvent(const Platform::Sdl::PlatformEvent&) noexcept;
[[nodiscard]] Base::Result<void, SdlInputError> SetTextInput(bool enabled);   // new code TextInputFailed
}
// PhysicalInputFrame.hpp, additive
struct KeyModifiers final { bool shift{}, ctrl{}, alt{}, gui{}; };
struct TextEvent final {
    enum class Kind : std::uint8_t { Text, Key } kind{};
    std::string text;                 // Kind::Text
    Key key{Key::Count};              // Kind::Key: physical position, as everywhere in Input
    Key layoutKey{Key::Count};        // the key the current layout produces at that position; Count if none
    KeyModifiers modifiers{};
    bool repeat{};                    // Kind::Key records presses only (including repeats), never releases
};
struct InputEvent  { /* existing */ std::chrono::steady_clock::time_point timestamp{}; };   // U4: static_assert == Time::TimePoint in the backend; no Input->Time edge
struct PhysicalInputFrame { /* existing */ std::vector<TextEvent> text; };   // BeginFrame clears it
```

Native 側（另一個 include 根 `engine/<backend>/native/include/...`）：

| 標頭 | 內容 | 目標 |
|---|---|---|
| `SdlPlatformNative.hpp` | `SDL_Window* NativeWindow(const SdlPlatform&)`；`const SDL_Event& NativeEvent(const PlatformEvent&)`；`NativeAccess`；`PushInjectedEvent(SdlPlatform&, SDL_Event&)`（IP-4，`injected` 旗標由 platform 擁有）；換算的純函式 `ConvertSdlTimestamp(std::uint64_t sdlNs, ClockPair, TimePoint ceiling)`；測試縫 `SampleClockPairForTesting()`（回傳 `{engine, sdl, bracket}`，release 建置也存在） | `GYO::PlatformSDLNative` |
| `SdlInputNative.hpp` | `void HandleNativeEvent(SdlInput&, const SDL_Event&, Time::TimePoint)` | `GYO::InputBackendSDLNative` |
| `SdlRendererNative.hpp` | `SDL_Renderer* NativeRenderer(const SdlRenderer&)` | `GYO::RenderBackendSDLNative`（只在 `GYO_ENABLE_SDL_RENDERER` 時建立） |

接線（避免循環，F3）：

- Native 目標是 INTERFACE library，只以 `INTERFACE` 連自己的後端與 `SDL3::SDL3`。
- 後端以 `target_include_directories(<backend> PRIVATE native/include)` 取得自己的 Native 標頭，不連自己的 Native 目標。
- `gyo_input_sdl` 與兩個 render 後端以 PRIVATE 連 `GYO::PlatformSDLNative`。
- 產品目標不連任何 `*Native`（消費端的守衛檢查產品目標自己的 `LINK_LIBRARIES`，不是連結閉包）。

### 時間戳的契約

- **取樣**：進入 pump 時依序取 `e0=Time::Now()`、`s=SDL_GetTicksNS()`、`e1=Now()`；配對 `engine=mid(e0,e1)`，`bracket=e1−e0`。整個 Engine 只有 `SdlPlatform` 持有配對（狀態的權威）。
- **換算**：`ts = min(engine + (int64)(sdlNs − s), 交付時刻的 Now())`；`sdlNs==0`（`SDL_PeepEvents(SDL_ADDEVENT)` 不蓋時間，`SDL_events.c:1043,1167-1175`）時取交付時刻。
- **可以檢查的四個條件**：
  1. 不晚於交付時刻。
  2. 同一次 pump 內，時間戳不遞減的事件換算後也不遞減。
  3. 不保證依 `events` 的到達順序遞增，也不保證跨 pump 保序（macOS 的 NSEvent 時間與推入時蓋的時間會交錯，`SDL_cocoaevents.m:570-586`；配對誤差 ±bracket/2）。
  4. `sdlNs==0` 時取交付時刻。
- **已知限制**（寫進文件，不修正）：
  - 事件入列與 pump 之間若經過系統睡眠，macOS 的結果會晚一段睡眠時長。
  - Linux 上 Engine 用 `CLOCK_MONOTONIC`、SDL 用 `MONOTONIC_RAW`，速率最多差約 500 ppm；誤差隨「事件相對於配對的年齡」增加〔推論〕。
  - 平台精度：Windows 的時間來自 `GetMessageTime`，ms 級（`SDL_windowsevents.c:149-179`）；macOS 以第一個事件的延遲作初始偏移（`SDL_cocoaevents.m:576-578`）。下一版音效的排程精度受它限制。

### 日誌橋

- 預設 tee：轉送到 Base，同時保留 SDL 原本的輸出（`keepSdlOutput=true`），與消費端現在的行為相同；`false` 時只轉送。
- 等級：trace／verbose／debug→Debug、info→Info、warn→Warning、error／critical→Error。分類用靜態表 `sdl.<category>`，自訂分類一律 `sdl.custom`。整條路徑 noexcept，不在 callback 內配置可能丟例外的記憶體。
- 同一執行緒重入時，只送 SDL 原本的輸出（thread_local）。
- **前提**：橋存在期間，Base 的 LogHandler 不呼叫 SDL 的日誌函式（否則兩條執行緒各持一把鎖互等；SDL 的鎖是遞迴鎖，`SDL_sysmutex.c:37-38`，thread_local 擋不住跨執行緒）。
- 解構時先用 `SDL_GetLogOutputFunction` 確認目前仍是橋，才還原；不是就 `GYO_ASSERT`。橋必須在 `SDL_Quit` 之前解構（`SDL_Quit` 會銷毀日誌鎖，`SDL.c:711`、`SDL_log.c:147-165`）。
- 放在 `GYO::PlatformSDL`：Trace 與 Base 依 `docs/architecture/trace.md:52-55` 不得依賴 SDL。

### 進入點

- `GYO::PlatformSDLMain` 是 OBJECT library，內部 include `<SDL3/SDL_main.h>`，定義 `main → GyoMain`，以 PRIVATE 連 SDL3。產品在自己的 CMake 明確連結它，不修改共通的 `GyoAppProject.cmake`。
- `main` 以 try/catch 包住 `GyoMain`：例外不得穿過 SDL 與 OS 的 C 堆疊（同 `SdlPlatform.cpp:109-110` 的先例）；記錄 Error 後回傳 1。
- 產品是 Windows console 程式（`apps/object_fps_pvp/CMakeLists.txt:54` 沒有 `WIN32`）；保留 `SDL_main` 才能維持 argv 的 UTF-8。
- init／quit 維持現狀：`SdlPlatform` 擁有 VIDEO；自由函式不隱含 init；Engine 不呼叫全域的 `SDL_Init`／`SDL_Quit`。

### 子批（PR 內的里程碑）

```text
IP-3
├── ip3a-interface-paper    high＋xhigh 審查已完成｜簽章定案、DI 草案、事前宣告 → 使用者關卡
├── ip3b-guards-native      high                          ｜G1／G2（先 Red）、pimpl、PlatformEvent、Native 目標、機械修改
├── ip3c-event-time         局部 xhigh                     ｜配對取樣、換算、InputEvent 時間戳
├── ip3d-text-entry         high                          ｜SetTextInput、text、ClipboardText
├── ip3e-process-services   high（日誌橋的重入局部 xhigh）  ｜GyoMain、BaseDirectory、PlatformName、SdlLogBridge
└── ip3f-close              medium                        ｜全突變、L3、遷移清單、platform-sdl.md
```

| 代號 | 驗收點 | 估時（h） | 停止條件 |
|---|---|---|---|
| ip3a | 簽章與本節的契約定案；設計寫成架構文件（`docs/architecture/platform-sdl.md`、`input.md`，D31）；ip3c～ip3e 的判定、突變表、機率性測試的試行次數（§14.1）；核對改走 Base handler 的日誌行是否都屬消費端日誌契約的 B 級 | 3～4 | **必停**，交使用者核准 |
| ip3b | G1：衛生 TU include 所有非 Native 的後端公開標頭，`#if defined(SDL_stdinc_h_) \|\| defined(SDL_h_) \|\| defined(SDL_platform_defines_h_)` 就 `#error`。G2：ratchet 掃描 `\bSDL_\w+\|\bSDLK_\w+\|\bSDLCALL\b\|<SDL3/`（先去掉註解），範圍 `engine/**/include`（排除 `native/`），allowlist 只減不增，本批結束時為空。兩者先 Red 後 Green；突變 m6a（放回 `SDL.h`）、m6b（放回 `SDL_events.h`）都要被抓到。IP-1 既有測試判定不變。L1 四平台。同機兩樹 digest | 8～12 | 出現計畫外的依賴邊；IP-1 測試的判定（期待值、條件）必須修改；ui_editor 的修改超出機械修改 |
| ip3c | 純函式：signed delta、clamp、`sdlNs==0`。整合：`p0=Now()`；`SDL_PushEvent`（ts=0，由 SDL 補上）；`p1=Now()`；`Waiter` 等 ≥5 ms；pump；斷言 `p0−β/2 ≤ ts ≤ p1+β/2`（β 由測試縫讀出），另立「`ts ≤ 交付時刻`」。假配對在兩次 pump 之間跳 10 s，結果跟著跳。突變（逐項標明由純函式或整合判定殺掉）：m1 只在 `Create` 取一次配對、m2 負的 delta 歸零、m3 正負號反轉、m4 拿掉 clamp、m-zero 不特判 0 | 4～5 | 宣告的判定不成立時先報告，不重跑；兩種量法都無法定論就停 |
| ip3d | `text` 只在文字輸入啟用期間有內容（收集條件在事件到達時查 SDL 的狀態，不保存副本）；`Kind::Key` 只記按下（含 repeat），repeat 不進 `events`；文字與按鍵在 `text` 內依到達順序；`layoutKey` 以 Native 構造 `scancode=Q, key='a'` 的事件驗證；剪貼簿 set→get（先確認 dummy driver 的行為）。突變：m7 停用時仍填、m8 repeat 進 `events`、m9 順序反轉、m-up 放開也記、m-layout `layoutKey=key` | 4 | 表示方式需要改既有欄位的語意 |
| ip3e | 橋：等級對照；tee 與只轉送兩種模式的行數守恆（Base 收到推入數；原輸出收到推入數或 0）；同執行緒重入時原輸出恰好 1 行；並行建立與解構（試行次數在 ip3a 宣告）；解構檢查目前輸出。`BaseDirectory` 與 `SDL_GetBasePath` 一致，非 ASCII 的 UTF-8 在 windows CI round-trip。`tests/common/runtime_sdl` 改用 `GyoMain`，CI 四平台檢查結束碼。突變：m10 拿掉防重入、m11 不還原、m12 丟掉 `GyoMain` 的回傳值、m-throw 例外穿出 | 4～5 | Windows CI 拿不到 main；日誌行觸及消費端日誌契約的 A 級 |
| ip3f | 突變全跑；L3（macOS Intel）：位址輸入、Ctrl+V、按住 Backspace、Ctrl+A、Return／Esc 與以前相同；live resize 正常；日誌檔裡的 SDL 行與以前相同；架構文件的「Status」改成實作後的狀態 | 3～4 | — |

**IP-3 合計 26～34 h（Engine 部分）。**

### 驗收點

- L1：如上；CI 四平台；ui_editor 建置並通過測試；依賴邊只有 Architecture Delta 列的那些。
- L2：IP-3 本身沒有；消費端的 L2 在整個 PR 的 base 與頭跑一次（消費端計畫）。
- L3：ip3f。

### 平台表

| 平台 | 預定 | 說明 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | L1、L3 |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 預定執行 | G1、G2、`GyoMain`、換算、日誌橋、UTF-8 路徑 |
| Linux lavapipe GPU | 預定執行 | 既有 `gpu` 測試作回歸 |
| Windows D3D12 實機 | 未執行 | `GyoMain` 的 UTF-8 argv 與非 ASCII 路徑只有 CI 證據 |
| Linux 實機 | 未執行 | ppm 誤差只有推論 |
| macOS arm64 實機 | 未執行 | — |

### 建議檔位

high。ip3c 局部 xhigh；ip3e 的日誌橋重入局部 xhigh。公開介面的 xhigh 審查已在規劃時完成，ip3a 只核對必修項目是否已併入。依 D8，各子批開始時說明。

### 依賴

- TT-2（已完成）。
- 下游：IP-4；提出需求的消費端下一版的音效（只依賴 ip3c）。

### Architecture Delta（七點）

1. **需求來源**：消費端的完成條件（產品不直接呼叫 SDL，以符號判定）；下一版音效的時間戳；位址輸入（既有功能）。
2. **現在的問題**：SDL 經公開標頭傳出去（`SdlPlatform.hpp:8,44,75`、`SdlInput.hpp:8,32`、`SdlRenderer.hpp:6,63`）；事件沒有時間戳（`PhysicalInputFrame.hpp:95-106`）；消費端同時有兩個方向的日誌轉送（`main.cpp:24-50`）。
3. **改變的邊界**：後端分成中立 API 與 `*Native` 兩層；`NativeEventObserver` 由 `PlatformEvent`＋`NativeEvent()` 承接（README 方針 3 的「保留」）；輸入幀以加法擴充；process 進入點的掛點由 Engine 擁有，本體由消費端擁有。
4. **影響範圍**：`engine/platform/sdl`、`engine/input`、`engine/input/backend/sdl`、`engine/render/backend/{sdl,sdl_gpu}`、`tests/common`；提出需求的消費端；ui_editor（機械修改）；未啟用產品只寫遷移清單。
5. **依賴方向**：新目標 3 個 `*Native`（INTERFACE）與 `GYO::PlatformSDLMain`（OBJECT）；`gyo_input_sdl` 與兩個 render 後端 PRIVATE 連 `GYO::PlatformSDLNative`；`GYO::Input` 不新增 → `GYO::Time` 的邊（U4：時間戳用 `std::chrono::steady_clock::time_point`，後端 `static_assert` 與 `Time::TimePoint` 相同）。沒有循環，方向仍是 Application／Editor → Engine。
6. **Ownership**：SDL 時鐘的換算由 platform 獨佔；文字的收集歸 input 後端；剪貼簿、日誌橋與注入旗標歸 platform；文字編輯政策歸消費端。
7. **為什麼不能更小**：forward declaration 會讓 `SDL_*` 名稱留在標頭，消費端的符號判定過不了；只改 PRIVATE 而不拆 Native，ui_editor 與 `tests/common` 會編不過；把文字放進 `events` 會改變 IP-1 的契約（`PhysicalInputFrame.hpp:113-114`）。

另外報告、本批不處理的 code smell：消費端的 `RunningExecutablePath`（`apps/object_fps_pvp/src/Pvp/LogFile.cpp:55-80`）與 `SDL_GetBasePath` 職責重疊。

### 完成條件

L1 全通過；G1／G2 生效，Engine 的 allowlist 為空；突變全殺；L3 完成；遷移清單、HANDOFF、README、dev_log 已更新。

### 停止條件

- 加法無法完成，必須改既有 kind 或 `pressed`／`released`／`held` 的語意。
- 出現 Architecture Delta 以外的依賴邊。
- 實作中發現 `PlatformEvent`、每個後端一個 Native 目標、每次 pump 一個配對這三個前提之一不可行。
- 時間戳的判定不成立。
- 日誌契約衝突。
- 估時超過 1.5 倍。

## IP-4：測試用事件注入與觀測、視窗查詢

### 目標與範圍

做：

- 注入：key、mouse、focus 走 `SDL_PushEvent` 的同一條佇列（經 platform 的 `PushInjectedEvent`）；move／resize 只用真正的視窗操作。
- 帶 Engine 時間戳的觀測 hook，只交出本視窗的事件。
- 視窗與顯示器的唯讀查詢，每次直接讀 SDL／OS，不快取；`SetWindowSize`／`SetWindowPosition`／`Raise`／`Sync`。
- capture 存檔（位置見 U9）。
- 最後把 SDL3 改為 PRIVATE。
- 規劃新增的查詢（相對滑鼠模式、`VideoDriverName`、refresh rate、X11 window number，見 U3、U7）。

不做：合成的 MOVED／RESIZED；`SetWindowTitle`（只有未啟用產品使用，只寫遷移清單）；Metal 長幀仍回報 `Presented` 的問題（未結事項，不宣稱）。

### 公開 API 草圖

```cpp
// SdlPlatform (no SDL); main thread unless noted; none may be called inside an observer or a live frame (GYO_ASSERT)
struct WindowRect { int x{}, y{}, width{}, height{}; };
struct WindowBorders { int top{}, left{}, bottom{}, right{}; };          // zeros if unsupported
struct WindowFlags { bool occluded{}, hidden{}, minimized{}, inputFocus{}, relativeMouseMode{}; };
struct WindowState { WindowRect rect; WindowBorders borders; WindowFlags flags; };
struct DisplayState { WindowRect usableBounds; float refreshRate{}; };
Base::Result<WindowState, SdlPlatformError>  QueryWindow() const;
Base::Result<DisplayState, SdlPlatformError> QueryDisplay() const;
std::string_view VideoDriverName() const noexcept;
Base::Result<void, SdlPlatformError> SetWindowSize(int w, int h);
Base::Result<void, SdlPlatformError> SetWindowPosition(int x, int y);    // Wayland: Err with SDL reason
Base::Result<void, SdlPlatformError> Raise();
Base::Result<bool, SdlPlatformError> Sync();                            // false = timed out; Err cases listed at ip4a
// Observation: kinds and named fields follow InputEventKind / InputEvent; observation-only kinds get new names
struct ObservedEvent { ObservedEventKind kind{}; std::uint32_t sourceId{};   // platform-defined, observation only
    float dx{}, dy{}; int windowX{}, windowY{}, windowWidth{}, windowHeight{};
    bool injected{}; Time::TimePoint timestamp{}; Time::TimePoint observedAt{}; };
using EventWatchCallback = void (*)(void* context, const ObservedEvent&) noexcept;  // any thread; must not block
class EventWatch;   // RAII; destroyed before SdlPlatform (GYO_ASSERT); destruction waits for an in-flight callback
[[nodiscard]] Base::Result<EventWatch, SdlPlatformError> AddEventWatch(EventWatchCallback, void* context);
// SdlInputInjection.hpp in GYO::InputBackendSDL (no SDL); main thread; calls PushInjectedEvent only
class SdlInputInjector final { public: explicit SdlInputInjector(Platform::Sdl::SdlPlatform&) noexcept;
    Base::Result<void, SdlInputError> PushKey(Key, bool down);          // Enter -> RETURN
    Base::Result<void, SdlInputError> PushMouseButton(MouseButton, bool down);
    Base::Result<void, SdlInputError> PushMouseMotion(float dx, float dy);
    Base::Result<void, SdlInputError> PushFocus(bool gained); };
```

- 觀測 hook 的時間戳：每次回呼自己取一個配對（保留 probe「入列時刻」的語意，`player_short.hpp:17-22`）；判定用與 ip3c 相同的夾住 push 的窗口，不用交付時刻當上緣。
- `injected`：由 platform 的 `PushInjectedEvent` 在 push 前後設定與還原 thread_local；SDL 在 push 的同一執行緒同步呼叫 watcher（`SDL_events.c:1806`）。真正的視窗操作產生的事件 `injected=false`。
- 注入的限制（寫進文件）：`SDL_PushEvent` 直接入列，不更新 SDL 內部的滑鼠、鍵盤、修飾鍵狀態，也不產生 TEXT_INPUT；非相對模式下 `EndFrame` 會用 `SDL_GetMouseState` 覆寫指標座標（`SdlInput.cpp:281-284`）；motion 不進 `events`，只在聚焦時累加到 `pointer.delta`（`SdlInput.cpp:266-274`）。
- 所有回呼都在 SDL 的全域 watcher 鎖內執行（`SDL_eventwatch.c:59-87`）；live frame 也在同一把鎖內（`SdlPlatform.cpp:94-117`）。
- PRIVATE 連結：`gyo_platform_sdl`、`gyo_input_sdl` 改成 `PRIVATE SDL3::SDL3`；兩個 render 後端各自補上 `PRIVATE SDL3::SDL3`；ui_editor 與 `tests/common` 改連 `*Native`。

### 子批

```text
IP-4
├── ip4a-window-inject   high                                 ｜查詢、操作、注入器、規劃新增的查詢
├── ip4b-watch-capture   high（watch 的執行緒與生命週期局部 xhigh）｜觀測 hook、capture
├── ip4c-private-link    high                                 ｜PRIVATE、G3
└── ip4d-close           medium                               ｜全突變、L3、文件
```

| 代號 | 驗收點 | 估時（h） | 停止條件 |
|---|---|---|---|
| ip4a | dummy driver 下：key、button、focus 的注入在下一次 pump 進入 `events`，而且是本視窗的事件；motion 在聚焦時進入 `pointer.delta`；setter 之後用 query 讀回；`SetWindowSize` 產生真的 RESIZED（dummy driver 下 `SetWindowPosition` 是否送 MOVED 先實測）。突變：m13 windowID 填 0、m14 注入直接寫幀 | 4 | dummy driver 不支援某項操作時，該項只在 L3 驗證並寫明理由 |
| ip4b | 注入的事件 `injected=true`、真實事件 false；另一條執行緒持續 push 時解構，之後不再收到回呼（試行次數在 ip3a 宣告）；時間戳在夾住 push 的窗口內；只交出本視窗的事件；`AddEventWatch` 失敗路徑；BMP 與直接呼叫 SDL 的輸出位元組相同。突變：m15 不設 injected、m16 不過濾視窗、m17 解構時不移除、m18 用 BGRA | 3～4 | 回呼的生命週期無法用 SDL 的鎖語意證明 |
| ip4c | G3：configure 時檢查兩類目標——四個後端與 `GYO::PlatformSDLMain` 的 `INTERFACE_LINK_LIBRARIES` 中 SDL3 只能以 `$<LINK_ONLY:>` 出現；三個 Native 目標允許 INTERFACE。既有的 `gyo_expect_links` 分不出 PUBLIC 與 PRIVATE（`tests/common/time/CMakeLists.txt:10-29`），另寫 `gyo_expect_private_link`。突變：m19 改回 PUBLIC。CMake 依賴圖的變化只有預期的那些；AGENTS §12 的 Fitness Check；L1 四平台 | 2.5～3 | 有啟用中的目標無法建置，而且修正超出機械修改 |
| ip4d | 突變全跑；L3（macOS Intel）：live resize 時大小正確 | 1.5 | — |

**IP-4 合計 11～12.5 h（Engine 部分）。**

- 驗收點：L1 如上；L2 在消費端；L3 只有 ip4d。
- 平台表：六列同 IP-3；Wayland 的 `SetWindowPosition` 失敗路徑只在程式碼層處理。
- 建議檔位：high；ip4b 局部 xhigh。IP-4 的公開介面是否另做 1 次 xhigh 審查見 U10。
- 依賴：IP-3 的里程碑完成（同一個 PR）。

### Architecture Delta 補充（IP-4）

- 邊界：SDL3 從 PUBLIC 改為 PRIVATE，只有 `*Native` 目標會傳遞 SDL3。注入器在 input 後端（`Key` 屬於 Input），推送原語與 `injected` 旗標在 platform。capture 的位置見 U9。
- 依賴：render 後端新增 `PRIVATE SDL3::SDL3`，沒有其他新邊。
- Ownership：視窗狀態的權威是 SDL／OS，Engine 不持有副本；文字輸入狀態的權威也是 SDL（ImGui 的 SDL3 後端會自己開關文字輸入，`imgui_impl_sdl3.cpp:187,206`）。
- 為什麼不能更小：只做 PRIVATE 而不提供查詢、注入與觀測 API，probe 與產品就只能永久連 Native，消費端的符號判定不會歸零。

### 完成條件

G1、G2、G3 生效；ui_editor 與 `tests/common` 經 Native 目標建置；突變全殺；遷移清單、HANDOFF、README、dev_log 已更新。

## Design Invariants（正式條文在 `docs/architecture/platform-sdl.md` 與 `input.md`；本節是規劃時的摘要）

1. SDL 後端的非 Native 公開標頭不直接、也不間接 include SDL，不出現 `SDL_` 符號。守衛：G1、G2。
2. SDL 型別只能經 `*Native` 目標取得；連結 `*Native` 等於宣告直接使用 SDL。從 IP-4 起，後端以 PRIVATE 連 SDL3。守衛：G3。
3. platform 交出的時間都在 Engine 時間基準上，滿足「時間戳的契約」的四個條件；換算一律同時取樣兩個時鐘，不用固定 offset；pump 路徑每次 pump 一個配對，watch 路徑每次回呼一個；只有 `SdlPlatform` 持有配對。守衛：m1～m4、m-zero。
4. 測試注入只走 SDL 事件佇列或真正的視窗操作，不提供直接寫幀的後門；`injected` 旗標由 platform 擁有。守衛：m14、m15。
5. 日誌橋只有消費端建立時才生效；沒有建立時，SDL 的輸出完全不變。橋存在期間，Base 的 LogHandler 不呼叫 SDL 日誌函式。橋不 throw、不遞迴、解構時確認後還原，並在 `SDL_Quit` 之前解構。守衛：m10、m11。
6. 視窗、顯示器與文字輸入的狀態每次直接讀 SDL／OS，不保存拿來做判斷的副本。
7. pump 可以巢狀（IP-2 的保證）：每次 pump 各自取配對，巢狀的 pump 返回時還原外層的配對（D33，守衛 m-nest）；`Sync`／`SetWindow*`／`Raise` 不得在 observer 或 live frame 內呼叫（`GYO_ASSERT`）。watch 回呼不得阻塞或等待其他執行緒；`EventWatch` 先於 `SdlPlatform` 解構。
8. Engine 不呼叫全域的 `SDL_Init`／`SDL_Quit`；子系統由 owner 以參考計數管理。進入點的例外不穿過 SDL 與 OS 的 C 堆疊。
9. Engine 不帶擷取政策、文字編輯政策與產品知識。

輸入幀的加法擴充（既有 kind、`events` 的內容與 `pressed`／`released`／`held` 的語意不變；文字只在啟用期間出現在 `text`）屬於 `GYO::Input` 的 owner，寫進 input 的 architecture 文件，不放在本節。

### Replacement Criteria

符合任一條時，停止修補，交使用者決定修補還是重建：

- 需要第二個非 SDL 後端：改提後端中立的平台介面。
- 除了 IP-3 結束時列出的 ui_editor ImGui 整合以外，啟用中的產品為了新能力而需要 `*Native`：中立 API 的切法錯了。
- `PlatformEvent` 需要對 `SdlInput`／Native 以外的使用者公開欄位。
- 每加一種事件或視窗功能，都要重新驗證時間戳換算或日誌橋。
- 要守住第 3 條的 pump 路徑，需要逐事件取樣或偵測睡眠。
- pump 要移出主執行緒時，`SdlPlatform` 無法滿足 SDL 的主執行緒限制。

## 遷移清單（預定；各里程碑完成時寫進 foundation-followups/inactive_products.md「3. IP」）

### IP-3

- **ui_editor（啟用中）**
  - 本 PR 的機械修改：`ImGuiSdlRendererHost.cpp:21-22,189-249` 改成 `NativeWindow(p)`／`NativeRenderer(r)`／`NativeEvent(e)`，連 `GYO::PlatformSDLNative`、`GYO::RenderBackendSDLNative`。
  - 尚未遷移：`SDL_Log*`（`:118-200`）改用 `Base::Log`；`SDL_getenv`（`:74`）可改用 `std::getenv`。
  - 不在 IP 範圍、繼續直連 SDL3（`tools/ui_editor/CMakeLists.txt:65-67`）：SDL_Renderer 紋理（`AssetPreviewContext.hpp:10-55`、`AssetPreviewContext.cpp:46-208`、`PreviewAdapter.cpp:122`）；軟體 renderer 測試（`tests/ui_editor/PreviewAssetTests.cpp:57-124`）；ImGui 的 SDL3 後端。
- **object_fps／object_fps_v2**
  - 進入點：`main.cpp:5,14` 改成 `GyoMain`＋`GYO::PlatformSDLMain`。
  - 執行檔目錄：`main.cpp:31`、`tests/object_fps/TestAssets.hpp:19`、`tests/object_fps_v2/TestAssets.hpp:16` 改用 `BaseDirectory`。
  - 日誌：`main.cpp:21-40`、`ObjectFpsApplication.cpp:124,149` 改用 `Base::Log`；SDL 的輸出可用 `SdlLogBridge` 轉送。
  - 事件 pump：`ObjectFpsRuntimeClient.cpp:233-234` 改用 `PlatformEvent`。這兩處目前只靠間接 include 取得 SDL，IP-3 之後**編譯失敗**，重新啟用時一定要遷移。
  - 合成事件：`tests/object_fps_v2/App/DebugRuntimeTests.cpp:53-85` 改用 `HandleNativeEvent`＋`GYO::InputBackendSDLNative`，或改用注入器。
- **tools/object_fps_preview**：`ViewmodelPreview.cpp:6` 的舊 include 路徑；pump 改用 `PlatformEvent`；`main.cpp:5`、`:25` 的進入點與執行檔目錄同上。

### IP-4

- capture：`diagnostics/MuzzleProbe.cpp:44-51`、`VisualProbe.cpp:122-132`、`build/acceptance/object_fps_v2/main.cpp:78-82` 改用 capture 存檔 API。
- delay：`MuzzleProbe.cpp:36`、`VisualProbe.cpp:118,136`、`object_fps_v2/main.cpp:77`、`ViewmodelPreview.cpp:95` 改用 `Waiter`。
- 視窗標題：`VisualProbe.cpp:106`、`ViewmodelPreview.cpp:91` 是**缺口**，不新增 API。
- 連結：PRIVATE 之後，仍直接使用 SDL 的未啟用目標要自己連 `SDL3::SDL3` 或 `*Native`。
- ui_editor：連 Native 目標。

## 使用者的決定（D31：全部照建議）

| # | 問題 | 選項 | 建議 |
|---|---|---|---|
| U1 | 估時：Engine 37～46.5 h，加消費端約 19～26 h，合計約 56～73 h；原本估 26～34 h（約 2 倍） | 接受，各子批開始前重估、超過 1.5 倍就停／縮減範圍（規劃新增的查詢改列例外、capture 延後，約 −3 h）／拆開：IP-3 先做、IP-4 與 probe 遷移另排（但會有暫時改連 Native 的雙重修改，也違反「一起合併」的決定） | 接受。增加的主要來源是守衛與突變（ip3b）和 probe 約 300 處的遷移，原估沒有算進去 |
| U2 | ui_editor 在本 PR 做機械修改（消費端的決定字面上是「只寫清單」） | 允許／不允許 | 允許：不改就編不過 |
| U3 | 「範圍：README 原列 vs 規劃新增」表的 6 項 | 全部核准／逐項 | 全部核准；X11 另見 U7 |
| U4 | `InputEvent` 時間戳的型別 | `Time::TimePoint`（新邊 Input→Time）／`std::chrono::steady_clock::time_point` 並在 `SdlInput.cpp` 加 `static_assert` 與 `Time::TimePoint` 相同（不新增邊）／`int64` ns | 第二個：型別安全、不新增依賴邊、容易撤回 |
| U5 | `NativeEventObserver` 由 `PlatformEvent`＋`NativeEvent()` 承接（README 方針 3） | 同意／保留別名在 Native | 同意：別名沒有其他使用者 |
| U6 | 凍結 oracle（消費端的 pointer capture 特性測試）改成 golden | golden（預設視為有放寬的一面，走 §14.4 的核准流程：突變、他人驗證、你核准）／保留並列例外／退役 | golden，走核准流程 |
| U7 | X11 window number 進中立標頭 | 加中立查詢／列為消費端符號守衛的例外 | 加中立查詢，回傳 `std::optional`，例外清單維持空 |
| U8 | 新文件 `docs/architecture/platform-sdl.md` | 新建（英文，比照 `trace.md`）／寫進既有文件 | 新建，英文 |
| U9 | capture 存檔的位置 | `GYO::RenderBackendSDLGPU`（已依賴 Render，不新增邊，但綁 GPU 後端）／`GYO::PlatformSDL`（不綁後端，但 platform 要知道 `SceneCapture`，新增 platform→Render 邊） | 前者：只有 GPU 後端的使用者 |
| U10 | IP-4 的公開介面（`EventWatch` 的生命週期與執行緒）是否另做 1 次 xhigh 審查 | 做（ip4a 開始前，約 1 h）／不做（ip4b 局部 xhigh 已涵蓋） | 做：執行緒與生命週期比 IP-3 更容易出錯 |

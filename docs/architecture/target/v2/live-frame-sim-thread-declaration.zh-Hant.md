# live frame 與模擬在自己的執行緒的事前宣告（PvP 移植第 4 段，`v8-product-pvp-port`）

日期：2026-11-24。PvP 移植實作卡（GYO2 主會話開的任務卡）。作者 Opus 5.5／high。
根據：[SDL 總宣告](sdl-declaration.zh-Hant.md)第 3 節的表（live frame 列）、R3、R5、第 6 節「執行緒」一段；[編排層](orchestration.zh-Hant.md) O-3、O-11、O-12、O-13、RC-O10；[交換點的宣告](exchange-declaration.zh-Hant.md)第 5.3 節（「留給實作那一批的」清單）與第 13 節 W-5、W-10；[時間設計](rebuild-time-design.zh-Hant.md)第 5.5 節組合（一）、（三）；[runtime 的組合](../runtime-composition.zh-Hant.md)第 2 節（使用者 2026-11-11：Client 三個 World）；PvP 的[移植宣告](../../../../games/object_fps_pvp/docs/port-declaration.zh-Hant.md) PV10、PV11。
master 只看理由與坑：偵察報告 `build/evidence/gyo2-sdl-declaration/master-platform-survey.md` 第 1.1 節（live frame 列）、第 7.1 節坑 1～7（本機證據，主 checkout）。

**動工的條件**（主會話 2026-11-24）：前提與別家對照寫完就動工，不必先交使用者。R3、R5、O-13 都在已核准的 SDL 總宣告與編排層裡，照它實作不算推翻使用者的決定。**只有兩種情況停下問主會話**：要改總宣告（或 O-13 等已核准條文）已定的內容，或要動 AGENTS §1。順序：PvP 第 5 段（大廳）先，這一段後。

## 0. 代號對照

| 代號 | 白話 |
|---|---|
| `v8-product-pvp-port-live-frame` | R3：使用者拖視窗角、拖標題列時，OS 的 modal 迴圈卡住事件泵；這段期間照樣畫畫面 |
| `v8-product-pvp-port-lend` | O-13 的入口：把一整個 World 交給別的執行緒（租約），交出期間編排層不碰它 |
| `v8-product-pvp-port-sim-thread` | R5：PvP Client 的模擬 World 在自己的執行緒跑，主執行緒只做事件＋畫面；遊戲的 Client 拆成三個 World |

```text
第 4 段
├─ live-frame   high｜sdl_window：event watch → 在 modal 迴圈中跑一幀（Engine，Plugin）
├─ lend         high｜編排層：Product::Lend／WorldLease／Return（Engine）
└─ sim-thread   high｜sdl_window 的 runner 開模擬執行緒＋PvP Client 拆 Sim／Frame／Render（Engine＋遊戲）
```

Engine 的改動（lend、live-frame、runner）與遊戲的改動分開 commit；Engine 的部分一個 PR（base `gyo2`），遊戲的部分疊在第 5 段上。

## 1. 要解決的問題（遊戲的例子）

- **拖視窗時畫面停住**：macOS 上按住視窗角拖動，OS 進自己的 modal 迴圈，`SDL_PumpEvents` 一直不返回；runner 卡在泵裡，畫面停在拖動前那一張。Windows 拖標題列、拖邊框同樣（`WM_ENTERSIZEMOVE` 進 Win32 的 modal 迴圈）。
- **拖視窗時人也停住**：PvP Client 現在是一條執行緒（第 2、3 段：`Game` World 同時跑模擬、網路與畫面）。主迴圈停住時模擬與網路一起停：這段期間沒有送輸入，Match 那邊自己的玩家原地不動；停太久 Match 判斷斷線（liveness 逾時）。
- **master 的經驗**（只看坑）：
  - 坑 1：macOS 縮放、拖動時主迴圈停住最長約 1.2 秒，原因之一是 Metal 取 swapchain 時 `nextDrawable` 最多阻塞約 1 秒；PvP 因此約 72 個 tick 被替代。
  - 坑 2：live frame 只解了一半——按下縮放角到開始拖動之間 macOS 不送 `windowWillStartLiveResize`，0 個 live frame，實測空窗最長 4.3 秒。單執行緒消不掉，master 改成分執行緒（主＝事件＋畫面；模擬、網路另外），不開 render 執行緒（D19、D27）。
  - 坑 3：live frame 裡用到舊的視窗大小（事件要等泵返回才處理），要直接讀大小。
  - 坑 4：例外穿過 SDL 與 OS 的 modal 迴圈；提早跳出泵會截斷下一次泵。
  - 坑 5、6：event watch 在別的執行緒解構有競態；watch 的回呼不一定在主執行緒（Windows 相對滑鼠模式是 raw input 執行緒）。

所以 R3（live frame）與 R5（模擬執行緒）一起做：live frame 讓畫面在拖動期間大致照跑，模擬執行緒保證「主執行緒停多久，模擬與網路都照跑」——後者才是對局不受影響的保證。

## 2. 前提：Engine 要解決，還是作者能處理

| 項目 | 判斷 | 理由 |
|---|---|---|
| live frame 的回呼點 | **Engine 要解決**（`sdl_window`） | OS 的 modal 迴圈與 SDL 的 event watch 只有 `sdl_window` 碰得到（SDL-2：SDL 的呼叫只在 `sdl*`）；作者的程式在泵返回之前沒有機會跑 |
| World 交出的入口 | **Engine 要解決**（編排層） | O-13 (2)「交出期間 Product 與別的執行緒不碰它」只有編排層能保證；`Product` 的入口、`~Product` 的順序、O-11 的「壞了就結束」都在編排層 |
| 模擬執行緒的迴圈（推一輪、等下一步、停止、收回） | **Engine 要解決**（`sdl_window` 的 runner） | O-3：runner 擁有迴圈與執行緒的安排；時間設計 5.5（三）：「runner 建立並負責收模擬的執行緒」 |
| Client 拆成哪幾個 World、哪個 World 交出 | **作者能處理**（遊戲） | runtime-composition 第 2 節定的是組法；哪些 System 在模擬、哪些在畫面是遊戲的事 |
| 模擬與畫面之間交換什麼、用什麼有鎖物件 | **作者能處理**（遊戲） | 內容（玩家的狀態、控制）是遊戲的；O-13 (3) 只規定「各自有鎖、不等另一條執行緒」。Engine 不加通用的信箱：只有一個使用者，做出來是猜形狀（AGENTS §1-10「既定值便利、不綁作者」不要求先做） |

## 3. 別家怎麼做

| | live frame（modal 迴圈中畫圖） | 模擬在別的執行緒 |
|---|---|---|
| **SDL 3**（我們的後端） | macOS：`windowWillStartLiveResize` 起一個 60 Hz 的 `NSTimer`（`NSRunLoopCommonModes`），每次 `SDL_OnWindowLiveResizeUpdate`（`src/video/cocoa/SDL_cocoawindow.m:1139-1157`）；Windows：`WM_ENTERSIZEMOVE` 起 `SetTimer(USER_TIMER_MINIMUM)`，`WM_TIMER` 時同一個函式（`src/video/windows/SDL_windowsevents.c:1869-1922`）。沒有 main callbacks 時它送 `SDL_EVENT_WINDOW_EXPOSED`、`data1 = 1`（`src/video/SDL_video.c:4302-4312`），文件說可以「directly from event watchers」重畫（`include/SDL3/SDL_events.h:139-140`） | 不管 |
| **Godot** | macOS：`displayLayer:` 在縮放中呼叫 `Main::force_redraw()`，`!Main::is_iterating()` 時 `Main::iteration()`（`platform/macos/godot_content_view.mm:61-73`）；Windows：`WM_ENTERSIZEMOVE` 起計時器，`WM_TIMER` 時 `Main::iteration()`，同樣先看 `is_iterating`（`platform/windows/display_server_windows.cpp:6905-6928`）。**在 modal 的回呼裡跑一整個迭代，靠旗標防重入** | 物理伺服器可以在自己的執行緒（`PhysicsServer3DWrapMT`），但 `step`／`sync` 每幀由主迭代驅動、命令排佇列（`servers/physics_3d/physics_server_3d_wrap_mt.cpp:44-82`）：主執行緒停住時物理也停 |
| **Bevy** | winit 反轉控制：OS 在 modal 中送的 `RedrawRequested` 由 runner 跑 `app.update()`（`crates/bevy_winit/src/state.rs:421-447`、`:520`） | pipelined 渲染：render SubApp 整個當值經容量 1 的 channel 送到渲染執行緒，同步點在 extract——主執行緒會等渲染交回上一幀（`crates/bevy_render/src/pipelined_rendering.rs:155-214`、`:217-240`）。沒有獨立時鐘的模擬執行緒 |
| **Unity Entities** | （引擎核心，不開放） | `ExclusiveEntityTransaction` 把 staging World 整個交給工作執行緒，交出期間一般 `EntityManager` 丟例外，結束只在主執行緒（交換點的宣告第 4.2 節已查） |
| **master** | event watch 收 `EXPOSED(1)`，只處理本視窗、主執行緒、泵進行中、不重入；例外先存、整輪泵完才丟（偵察報告第 1.1 節） | 分執行緒（D19、D27），不開 render 執行緒 |

**結論**：
- live frame 照 SDL 給的路與 Godot 的做法：在 modal 的回呼裡跑一整個主執行緒的幀，旗標防重入，條件照 master 踩過的坑。
- 模擬執行緒是 **GYO 多做的**：Bevy、Godot、Unity 的「別的執行緒」都和主執行緒同步（主執行緒停住它也停）；PvP 要「主執行緒停住時模擬照跑」（交換點的宣告第 8 節表中的 E-13、時間設計 5.5（三）），所以形狀取 Bevy pipelined 的「整個交出」（O-13 (2)、交換點的宣告第 5.3 節已選），同步點不取 extract，改成 O-13 (3) 的「各自有鎖、不等」。膠水量：一個租約型別、`RunFrame` 跳過交出的 World、一條原子旗標（第 4.2 節）。

## 4. 設計

### 4.1 live frame（`v8-product-pvp-port-live-frame`，R3）

1. adapter 在第一次 `SetLiveFrame` 給函式時（主執行緒）以 `SDL_AddEventWatch` 登記 watch，隨 adapter 的解構（主執行緒）`SDL_RemoveEventWatch`——watch 只在主執行緒裝、拆（坑 5）。〔審查 N4 更正：原寫 `Configure`／`OnDestroy`〕
2. watch 只在**全部**成立時動作：事件是 `SDL_EVENT_WINDOW_EXPOSED`、`data1 == 1`、是這個視窗；在主執行緒（`SDL_IsMainThread`；坑 6）；runner 正在泵**或等事件**（`SDL_WaitEventTimeout` 也會泵，OS 的 modal 迴圈可能在那裡開始；審查 S1）；不在一幀裡（不重入）。其他情形什麼都不做（事件照常進佇列）。
3. 動作：先把**現在的**視窗大小（直接查，坑 3）寫成 `window` 的 `WindowResized`（每次都寫、不比較：不持狀態；審查 S3 更正原寫的「值不同才送」；它的時刻是 `Now()`，同一次泵裡之後泵出的事件可能比它早，SDL-3 不保證跨泵的先後），再呼叫 runner 給的「一幀」函式。預設 runner 的「一幀」＝`Product::RunFrame()`（交出的 World 不跑，第 4.2 節）。**這一筆是 SDL-7 以外的合成**：要不要列為 SDL-7 的例外 (3)，已交主會話與使用者（審查 U1），合併前定。
4. 錯誤：一幀回錯時存起來、不從 watch 跳出；泵這一輪處理完才由 runner 照原路結束（坑 4）。`RunOn` 會讓 System 的例外記下失敗後照樣丟出（error levels D4），所以**例外也存起來**（`std::exception_ptr`），不穿過 SDL 的 C 程式、watcher 鎖與 OS 的 modal 迴圈；泵或等事件返回後再丟（審查 B1 更正原寫的「沒有例外」）。
4a. live frame 整段在 SDL 的 watcher 鎖裡：別的執行緒的 `SDL_PushEvent` 都等它，所以「一幀」不能在主執行緒等會推 SDL 事件的執行緒；「一幀」也不能呼叫 `SetLiveFrame`（會銷毀自己）。live frame 裡 `ApplyWindowRequests` 不呼叫 adapter：狀態式的請求由下一個 modal 外的幀套用，一次性的（`WarpCursor`、`ResizeWindow`、`CopyToClipboard`）存在 Service、下一個 modal 外的幀照順序先套用（審查 S2、S7）。`SDL_AddEventWatch` 失敗時記為 `SDLWindowPump` 的一次拒絕（N5）。
5. 自己的 runner（`SDLWindowOptions::installRunner = false`）：`SDLWindowPump` 加 `SetLiveFrame(std::function<void()>)`；不設就沒有 live frame（今天的行為）。
6. 選項：`SDLWindowOptions::liveFrame`（預設 `true`）。關掉＝不裝 watch。

Linux（X11、Wayland）沒有這種 modal 迴圈，watch 裝了也不會收到 `data1 = 1`。

### 4.2 World 交出（`v8-product-pvp-port-lend`，O-13 的入口）

交換點的宣告第 5.3 節列的「留給實作那一批的」逐條：

| 那一節的問題 | 這裡的答案 |
|---|---|
| 入口的形 | `Product::Lend(std::string_view world) → Result<WorldLease, OrchestrationError>`；`WorldLease` 只能移動，`RunRound() → Result<void, OrchestrationError>`；`Product::Return(WorldLease&&)` |
| 推一輪的內容 | `WorldLease::RunRound` 和 `RunFrame` 的一個 World 共用 `Product::RunRound(slot)`（生成的處理、交換函式的「進」→ 一輪 →「出」；W-10） |
| `RunFrame` 遇到交出的 World | 跳過，其他 World 照順序跑。交出的 World 是 Extract 的來源或讀者時不能交出（下一列） |
| 指名交出的 World 的入口 | `RunSchedule`、`ReplaceWorld`、`InsertSystems` 回新的碼 `WorldLent`（E2，請求沒做成）；`World(name)`、`Generating(name)` 是斷言（拿參照、讀狀態都是跨執行緒碰它，W-5、檢查 B4） |
| 遊戲初始化 | `Lend` 在 `RunGameInit` 之前回 `LendRefused`（新碼：既有的碼沒有「拒絕交出」這一類，理由在 dev_log；審查 N4） |
| 讀者交出期間來源 `InsertSystems`（跨 Registry 的讀） | Extract 的來源與讀者都不能交出（`Lend` 回 `LendRefused`），所以不會發生。PvP 的模擬 World 不是 Extract 的來源（渲染 World 讀的是畫面 World） |
| 準備函式、綁執行緒的 Resource | 準備函式在 Product 的執行緒跑（建 World 時，交出之前）；交出的 World 裡有綁執行緒的 Resource（`threadBound`）時，那條執行緒推一輪會照現有規則斷言——`Lend` 不另外查（Bevy 的 `!Send` 也是用到時才擋）。PvP 的模擬 World 不帶 `sdl_*` |
| runner 直接讀寫交出的 World | 由持有租約的執行緒做（租約給 `World&`，只在不在 Run 裡時） |
| 別的 World 要知道交出的 World 在生成 | 只能經它自己的「出」＋有鎖的物件（W-5） |
| 銷毀前收回 | `~Product` 時還有交出的 World 是斷言（交換點的宣告第 5.2 節第 5 點的前提）；runner 在 `Run` 返回之前 `Return` |
| **O-11**（交出的 World 壞了，Product 要結束） | 租約帶一個編排層的 `std::atomic<bool>`（交換點的宣告第 5.3 節第 4 點的候選）：`RunRound` 壞掉當下，租約存 `WorldFailure` 並設旗標；Product 每個入口先看旗標，設了就回 `Ended`（E1，帶「交出的 World 壞了」）；`Return` 時把 `WorldFailure` 搬進 Product（`Failure()` 照舊讀得到）。Kernel 不加原子操作 |

旗標只回答「壞了沒有」：持有的執行緒先寫 `WorldFailure` 再以 release 設旗標，之後不再寫它；Product 的執行緒以 acquire 看到旗標後讀它（`Ended()`、`Return`）。〔審查 N4 更正：原寫「只在 `Return` 之後讀」〕

### 4.3 模擬執行緒（`v8-product-pvp-port-sim-thread`，R5）

**runner（Engine，`sdl_window`）**：

1. 選項 `SDLWindowOptions::ownThreadWorlds`（`std::vector<std::string>`，預設空＝今天的行為）。
2. 第一次 `RunFrame` 之前（遊戲初始化之後）：對每個名字 `Lend`，各開一條 `Threads::RoleThread`（`engine/core/threads`：停止請求＋自己的 `Waiter`），名字 `gyo-world-<名字>`。
3. 模擬執行緒的迴圈＝時間設計 5.5（一）：記下這一輪開始的時刻（它的 `AdvanceTime` 緊接著取樣）→ `RunRound` → 讀交出的 World 的 `world_time` `FixedTime`，`WaitUntil`（那個時刻＋離下一步還差的時間÷倍率，最多一秒；沒有 world_time、暫停或倍率 0 時用閒置間隔）→ 停止請求就出迴圈（審查 S4 更正：原本用等待當下的時刻，每次晚醒一輪的耗時）。`RunRound` 回錯（World 壞了，或交換函式的 E2）或有例外離開一輪時，記下、以 release 設「已停」旗標、出迴圈；例外不逃出執行緒（`RoleThread` 的契約）。
4. 主執行緒：照舊「泵 → `RunFrame`（跳過交出的）→ 等畫面」；live frame 的「一幀」也是 `RunFrame`。
5. 結束：視窗關、主執行緒的 `RunFrame` 回錯、live frame 回錯，或主迴圈每圈看到某條模擬執行緒「已停」→ 對每條送停止請求、`join`、`Return`；之後在主執行緒：模擬執行緒的例外照樣丟出（和 `RunFrame` 丟出的一樣），否則它的錯誤或 Product 的 `Failure()` 讓 runner 回 Failure（O-3；審查 B2：原本 E2 與例外沒有回報，正常關窗時才壞的 World 也回了 Success）。
6. runner 依賴 `world_time`（讀 `FixedTime`）與 `core/threads`：`config/plugins.csv` 的 `sdl_window` 的 `private_depends` 加 `world_time`（審查 S6），`core/threads` 是 core、直接連。

**遊戲（作者，`games/object_fps_pvp`）**：

1. Client 拆三個 World：`Sim`（PvP World＋`net` 的 K3 client、`rollback`、`authority`、`physics_3d`、`world_time`；交出）、`Frame`（`sdl_window`、`input`、`window`、控制、相機、HUD、角色與 viewmodel、聲音、`world_asset`；主執行緒）、`Render`（`extractFrom = Frame`，今天的渲染 World）。
2. 交換物件 `ClientExchange`（遊戲自己的型別，Product 一份，`std::mutex`），O-12 的「交換函式」：
   - `Sim` 的「出」寫**最新一份**呈現快照：每個玩家的 `Owner`、`PlayerBody`、`PlayerCombat`、`HitRecord`、`ShotRecord`、`LocalUser`、`Tick`、步長、`NetReplicaState::latestConfirmed`、連線狀態（`clientPeer`）、`ContentMismatch`。
   - `Frame` 的「進」讀最新一份寫進自己的 Resource `ClientView`（`Frame` 裡沒有 PvP 的元件；PoseCharacters、HUD、聲音等改讀它）。
   - `Frame` 寫控制（按住的鍵、視角）與**累計**的按下（點擊、跳、換彈）；`Sim` 的「進」取走（按下取了就清），`WriteStepInput` 照舊在固定步寫這一步的格——第 2 段審查 B1 的語意不變（按下不丟）。
   - 呈現只留最新一份（畫面只要最新的）；控制的按下是累計（不能丟）。兩邊都不等另一邊（O-13 (3)）。
3. 跨 World 的時間：`Frame` 的動畫、HUD 用快照裡 `Sim` 的 tick 與步長（時間設計 5.5（三）「兩條執行緒的 `RealTime` 不同步時以誰為準」：以模擬的為準）；`Frame` 自己的 `RealTime` 只用在幀的取樣（聲音的時刻、受擊特效的淡出）。
4. Product 一份的物件誰碰（RC-O10）：`NetService`（交出期間只有 `Sim` 的執行緒；`Return` 之後主執行緒在 `client_main` 裡 `Close`＋`Flush`）、`SDL::NativeWindows`、`SDLWindowPump`、混音器（只有主執行緒）、`ClientExchange`（兩邊，自己的鎖）。每個 World 一份的 `ModuleBuffer` 照 CCR-5。沒有兩條執行緒都碰、又沒有鎖的物件——實作時逐一列進 dev_log。
5. 暫停：PvP 沒有暫停，不做。

## 5. 測試與驗收（事前宣告的判定）

| # | 判定 | 怎麼量 |
|---|---|---|
| J1 | `RunFrame` 跳過交出的 World；交出的 World 經租約推一輪，交換函式的「進」「出」各被呼叫一次 | `orchestration.lend` 新案 |
| J2 | 指名交出的 World 的入口回 `WorldLent`；Extract 的來源與讀者不能交出 | 同上 |
| J3 | 交出的 World 在別的執行緒壞掉：主執行緒下一個入口回 `Ended`（E1）；`Return` 後 `Failure()` 讀得到那個 `WorldFailure` | 同上（兩條執行緒） |
| J4 | live frame：泵進行中、主執行緒、`EXPOSED` 且 `data1 = 1` 時「一幀」被呼叫；`data1 = 0`、泵外、重入時不呼叫；一幀回錯時泵照樣處理完、runner 之後回 Failure | `sdl_window` 新案（dummy 驅動；用測試掛的第二個 event watch 在泵中 `SDL_PushEvent`） |
| J5 | 主執行緒停 250 ms：模擬執行緒的 tick 照樣前進，Match 照樣每步收到輸入（時間設計 5.5（三）「主執行緒停 250 ms 時模擬仍每步產生命令」） | `object_fps_pvp.client` 新案（MemoryLink，主執行緒在兩幀之間睡 250 ms） |
| J6 | 拆成三個 World 後，第 2、3 段的 client 測試照樣過（行為不變）；按下累計不丟（一步四幀的案照樣 4／4） | 既有的 `object_fps_pvp.client` 各案 |
| J7 | 實機（交使用者）：macOS 拖視窗角、拖標題列時畫面照跑、Match 不判斷線、自己的玩家在對手的畫面上照常動；Windows 同樣（有 Windows 機器時） | 實機場次 |

坑 2 的「按下到開始拖動之間沒有 live frame」macOS 不給回呼，畫面那段會停（最長數秒）；J7 只要求模擬與網路不停。

突變（只跑相關的，實作時登進 `games/object_fps_pvp/docs/mutations.json` 與編排層、`sdl_window` 各自的清單）：`RunFrame` 不跳過交出的 World、旗標不設、live frame 不看 `data1`、live frame 不看「泵進行中」、模擬執行緒不等下一步（空轉）、按下不累計。

## 6. 不做

- render 執行緒（runtime-composition 第 2 節：渲染與畫面同在主執行緒；master D27）。
- 遠端玩家內插（PV17，主會話 2026-11-24：這一卡不做）、幀內插。
- 通用的跨執行緒信箱進 Engine（第 2 節）。
- Linux 的 live frame（沒有 modal 迴圈）；Windows、macOS arm64 的實機只能交使用者。

## 7. 模型與檔位

實作 Opus 5.5／high；設計審查 Opus／high（Fable 暫停中，作者與審查者同模型，報告寫明）。租約的旗標與執行緒的停止、收回是時序上容易錯的地方，審查時點名看；使用者的原則是 xhigh 只用在調查，這裡不升。

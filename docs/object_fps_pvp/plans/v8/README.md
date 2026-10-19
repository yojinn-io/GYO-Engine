# PvP v8 玩法優化（未開始）

更新：2026-10-15（v7 第 16 批 L3 之後新增「多武器與本機射擊閘」候選、補充出生保護；先前：2026-10-09 新增「大地圖與塔科夫類玩法的預計壓力」；加入 v7 D41 移來的畫面設定 UI；2026-10-14 加入 v7 D76 移來的解析度與 FPS 選擇（IP-5＋第 13 批）與音效（AU-1＋第 14 批））。Owner：`object_fps_pvp`。**v8 尚未開始**：使用者 2026-10-07 開立，專門處理遊戲玩法的調整。本文件只收集候選與規劃輸入，還沒有分批、契約或檔位。

版本號：v8＝遊戲版本，pvN＝網路協議版本（見 [v6 交接](../v6/HANDOFF.md) D20）。與 [v7](../v7/README.md) 的分工：v7 處理架構與平台（執行緒、時間、網路、音效、顯示、SDL 隔離），v8 處理玩法本身，以及 v7 D41 移來的畫面設定 UI。
更正（2026-10-14，[v7 交接](../v7/HANDOFF.md) D76）：v7 縮小成只交付任務 1～3（執行緒、時間、網路）；音效與顯示（解析度、視窗模式、FPS 上限）移到 v8，SDL 隔離與日誌移到 [v7.1](../v7.1/README.md)。

## 範圍

- 做：規則、地圖、出生、回合、命中與傷害、角色辨識等直接影響遊玩的調整。
- 不做：v7 負責的架構與平台工作；不以玩法為理由在 Engine 加入產品專用的知識（AGENTS §6）。

## 已決定的項目

- 遊戲內的畫面設定 UI（[v7 交接](../v7/HANDOFF.md) D41，2026-10-09 使用者決定）：玩家在遊戲中選擇解析度、視窗模式與 FPS 上限，包含設定選單、套用後未確認就還原，以及選擇的保存（`settings.json`：版本與驗證規則屬 Data Contract，可以手寫）。
  - v7 的第 13 批先以命令列選項提供同樣的選擇；UI 共用它的限幀與視窗模式機制。
  - Engine 的 `UserDataDirectory` 與 `engine/io` 的 `AtomicWriteFile`（原本在 IP-5）隨這項一起做。
  - 補充（2026-10-14，v7 D76）：v7 的第 13 批也移到 v8（下一項），所以命令列選項不再先在 v7 提供；命令列選項與設定 UI 的先後在 v8 規劃時決定。
- 解析度、視窗模式與 FPS 上限的選擇（[v7 交接](../v7/HANDOFF.md) D76，2026-10-14 使用者以「照建議」核准；原 v7 的 P5＝IP-5＋第 13 批，v7 任務 6）：
  - 內容（v7 的原計畫）：玩家以產品 Client 的命令列選項選擇解析度、視窗模式與 FPS 上限（例如 `--resolution`、`--window-mode`、`--fps <上限>`）；指定的值不能用時退回預設並記一行日誌；HUD 依解析度縮放，移除寫死的 1280×720；L1 含選項解析與限幀的節拍；L3 含 30 FPS 的手感（v7 D41）。細節見 v7 交接「P5：解析度與 FPS 的選擇」。
  - Engine 的部分是 IP-5（顯示器與模式列舉、視窗模式、像素密度），見 Engine 計畫 [輸入與呈現](../../../architecture/plans/input-and-present/README.md)。
  - 理由（D76）：這是玩家功能，設定 UI 與保存已經在 v8（D41），一起做比較自然。
  - 相關：v7 的 FireGate 重推條件 7（低於 30 FPS 時重新確認，`../v7/09-firegate-c2.md`）在這項觸發。
- 音效（[v7 交接](../v7/HANDOFF.md) D76，2026-10-14 使用者以「照建議」核准；原 v7 的 P6＝AU-1＋第 14 批，v7 任務 5）：
  - 內容（v7 的原計畫）：Engine 新增 `GYO::Audio`（中立混音器、時間戳排程）＋SDL 後端（AU-1，Engine 計畫夾在開始時建立，v7 D28）；本產品在事件真正發生的地方以 Engine 時間戳觸發射擊、命中確認、受擊、換彈、遠端射擊的聲音；L2 以 `SDL_AUDIO_DRIVER=disk` 在 30 FPS 連射，檢查起音間隔對應射擊 Tick。細節見 v7 交接「P6：音效」；音效資產的來源見 v7 交接 D38（待確認）。
  - 理由（D76）：和網路與重構無關；使用者決定音效不是 v7 的發行條件。
  - 依賴：AU-1 需要 TT-1（v7 已完成）與 IP-3（移到 v7.1），所以排在 v7.1 之後。

## 規劃方式

- v8 開始時依變速箱規則規劃：先列出要做的玩法項目並由使用者排序，再分批。
- 玩法調整多半會改變權威結果或協議。沿用 v6 的做法：事前宣告預期變化，用同機兩樹的 digest 比對證明，改 wire 時三個角色同一個 PR。
- 手感類的項目以 L3（使用者實際遊玩）為主要驗收；可量化的部分（例如重生後多久被擊殺）先定義指標再調整。

## 候選（未定案；來源與現況）

| 候選 | 來源 | 備註 |
|---|---|---|
| 正式的 4 人地圖：出生點分散（例如四角）、出生保護 | v6 第 16 批（D23③）：為了不改變 2 人的權威結果與驗收幾何，第 16 批把新出生點放在既有兩點之間的線段上，4 人時容易出生即被擊殺 | 會改變 2 人的出生位置與驗收幾何，需要權威事前宣告與驗收工具改寫 補充：使用者（2026-10-15，v7 第 16 批 L3）：重生點沒有保護；玩家不移動、佔著重生點時，之後加入或重生的人可能疊在一起。很多 FPS 都有這種情況，不確定算不算問題，放到 v8 看要不要處理。 |
| 多武器與本機射擊閘（FireGate） | 使用者（2026-10-15，v7 第 16 批 L3-3）：「連擊手感好了很多，但是不知道後面如果換武器就不知道了」 | v7 的手感改善來自網路與時序，不是槍械參數（`cooldownTicks` 10 不變）。FireGate 的 guard 2 ms、spread 7 Tick 以冷卻 10 Tick、60 Hz 推導；新增射速不同的武器時，每種武器都要依 v7 09 文件的重推條件 8 重推並驗證（冷卻越短越依賴 pending 上界，冷卻邊緣的點擊可能被本機多擋或被權威拒絕）。若 v7.1 採用候選 A1（射擊併入輸入命令、照命令步序判定），這兩個常數就不需要；所以排在 v7.1 的決定之後。補充（2026-10-15，[v7.1 交接](../v7.1/HANDOFF.md) D90）：v7.1 採用 A1，FireGate 在 `a1-shot-step` 退役，換武器不必再重推這兩個常數。但 pv7 只給骨骼 hitbox 契約支持（D87），武器 id 等多武器需要的 wire 欄位不在 pv7；v8 做多武器時若要改 wire，先徵求使用者同意。補充（2026-10-19，[v7.1 交接](../v7.1/HANDOFF.md) D136）：A1 的冷卻保留上限 K＝min(K_max, cooldownTicks)、K_max＝2 Tick，射速改成資料驅動也成立（[聯網架構](../network-architecture.zh-Hant.md) §7「射擊判定的冷卻保留與命令步」）；做多武器時要先定兩件事：(1) 武器資料的權威在 Match（載入、驗證，經 `CombatRules` 告訴 Client），Client 本機的資料不得決定射速；(2) 射速的單位建議直接存 Tick（60 Hz 下只能是 3600÷Tick 發／分，例如 MP5 的 800 發／分只能取 4 或 5 Tick），若存發／分要把換算與取整寫進契約。全自動武器上線時，用「故障情境＋全自動」的量測重新評估 K_max。來源：[v7 STABLE_BASELINE](../v7/STABLE_BASELINE.md) 已知限制第 10 項、[v7 09 文件](../v7/09-firegate-c2.md)、[v7.1](../v7.1/README.md) A1 |
| 建房時在 Client 選地圖 | v6 第 17 批（D25）：Client 已能安裝多張地圖並依 Match 選擇；建房時選地圖還需要多房間或換地圖的 Match／Gateway、建房請求帶地圖 id（協議變更）與大廳選單 | 第 17 批的多地圖安裝是前提 |
| 角色外觀區分（顏色、名牌） | v6 第 16 批的已知限制：4 人時遠端外觀相同 | 呈現為主；若需要隊伍資訊則牽涉 wire |
| CS 式開局／回合準備期 | v6 交接第 9 項（候選）：全員凍結、無敵、倒數 | 需要 Match 的回合狀態與契約變更 |
| 屍體手上的世界手槍掉落或隱藏 | v6 交接（候選，`PlayerPresentation.cpp`） | 呈現 |
| 受擊方向對多位攻擊者的處理 | v6 第 16 批的已知限制：只指向最後一位攻擊者 | 第 09 批的受擊欄位只有最後攻擊者；更多資訊需要改 wire |
| PvP 骨骼 hitbox、爆頭倍率 | 列在 [v7 候選](../v7/README.md)（2026-10-07 使用者提出） | 前置條件（Match 權威姿勢、射擊回溯等）屬 v7 的架構工作；玩法規則部分可在 v8 決定。更正（2026-10-15，[v7.1 交接](../v7.1/HANDOFF.md) D87、D89）：v7 沒有做這些前置條件。pv7 只做 hitbox 的契約支持（命中部位、射手開槍時看到的遠端時刻等欄位，v7.1 先填等同現行的值）；hitbox 的實作在 v8，包括 Match 端權威姿勢與射擊回溯。骨骼對應的形狀與部位標記屬通用格式（Engine 計畫，Assets Editor 編輯），部位倍率等產品語意留在 PvP 的資料 |
| 單機劇情＋聯機選項 | v6 交接（v7 之後的產品方向候選） | 規模大，可能另立計畫 |

## 大地圖與塔科夫類玩法的預計壓力（未觀測，規劃輸入）

- 2026-10-09 使用者在側聊提出：之後預計做 GridMap 與「迷你版逃離塔科夫」。
- 以下都是**預計的壓力，還沒有觀測到**。依 AGENTS §2，開工前要先量到實際的壓力才動手；本節只記錄，不分批、不定檔位、不改計畫。
- v8 的範圍仍然不做 v7 的架構工作。但這些壓力是玩法帶來的架構需求，開工時架構的部分可能另立 Engine 計畫或批次。
- 目前 v7 的三角色分離（主、模擬、網路）不需要改。最先出現的壓力多半是演算法問題（O(N) 的寫法），不是執行緒的數量。
- 程式證據的行號以 2026-10-09（`claude/pvp-v7-p1b`）為準，開工時重新核對。

### 1. 碰撞是線性掃描

- 程式證據：
  - `apps/object_fps_pvp/src/Collision/CharacterCollision.cpp`：`MoveCharacterBody`（:69）先做最多 8 輪的穿透修正（:76），每輪掃過全部 `walls`（:83）；再做最多 5 輪的掃掠（:103），每輪也掃過全部 `walls`（:119）。`CanPlaceCharacterBody` 同樣逐面檢查（:59）。
  - `apps/object_fps_pvp/src/Pvp/Movement.cpp`：站立判定逐面掃描（:36）；每一步呼叫 `MoveCharacterBody`（:59）。
  - `apps/object_fps_pvp/src/Pvp/ShotQuery.cpp`：射線逐面檢查（:42）。
- 成本落在哪條執行緒：同一段程式跑在三條執行緒。
  - Match 的 Tick：每位玩家每個 Tick 走一步（`PvpMatch.cpp:420` 的 `StepMovement`）；射擊判定（`PvpMatch.cpp:312` 的 `QueryShot`）只在 Match。
  - Client 的模擬角色：每一步（`LocalPlayerPrediction.cpp:309`），以及 Reconcile 時重播待確認的命令（:204）。
  - 主執行緒：`InterpolateLocalPresentation` 每次放置呈現位置都做一次掃掠（`LocalPlayerPrediction.cpp:370`）。
- GridMap 會讓牆從幾十面變成幾千格。
- 修法方向：空間索引，以格子座標直接查附近的格子。這解決的是演算法問題，加執行緒沒有用。
- 開工前要收集的證據：Match 的 Tick 處理時間、模擬角色每一步的時間，和牆數的關係。
- owner 的候選：通用的格子空間索引放 Engine（Collision），地圖資料放產品。

### 2. 繪製每幀逐格、逐牆提交，沒有剔除

- 程式證據：`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` 的地板每一格送一次繪製（:566-579），每面牆送兩個 box（:580-586）。
- 成本落在哪條執行緒：全部在主執行緒。
- 修法方向：靜態格子按區塊合併成網格或改用 instancing，再依區塊剔除。舊的 `object_fps` 已有 `MapGeometryGenerator`（`apps/object_fps/src/Rendering/MapGeometryGenerator.cpp`），可以參考。
- 開工前要收集的證據：主執行緒的 prepare／render 時間、`longest_frame_ms`。
- owner 的候選：區塊網格的建構可以升到 Engine；地圖內容與配色留在產品。

### 3. 同步載入卡住主執行緒

- 程式證據：`engine/asset/include/engine/asset/AssetManager.hpp:36-37` 的 Async 只是擬似 async：在主執行緒的 `Update`（`engine/asset/src/AssetManager.cpp:25-27`、`:226-232`）每幀處理 `maxLoadsPerFrame = 2` 件；`engine/asset/` 沒有任何同步保護（沒有 mutex）。產品在 `PvpApplication.cpp:941-942` 每幀呼叫 `BeginFrame`／`Update`。
- 成本落在哪條執行緒：主執行緒。地圖和資產變大之後，同步載入會讓事件 pump 停好幾秒，macOS 會判定視窗沒有回應。
- 修法方向：背景 worker 做讀檔、解壓、解碼，主執行緒在 `Update` 收完成的結果。交給 GPU 的上傳要先查證 SDL GPU 的執行緒規則。這就是 [v7 候選](../v7/README.md)中「非同步資產載入（Job 的第一個實際需求）」的具體化。
- 開工前要收集的證據：載入時的主執行緒停頓時間、資產大小與件數。
- owner 的候選：Engine（Asset）。

### 4. snapshot 的大小

- 程式證據：UDP 一個封包上限 1200 bytes（`apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp:32`、`apps/object_fps_pvp/protocol/README.md:19`）；現在每個 snapshot 都帶全部玩家。
- 成本落在哪條執行緒：Match 組 snapshot、網路角色收送；主要是頻寬與封包上限。
- 修法方向：加入 AI、戰利品、門之後，需要 interest management（只送附近的實體）與差量壓縮。這是協議變更（新的 pvN）。
- 開工前要收集的證據：snapshot 的大小分布與實體數的關係。
- owner 的候選：產品（協議與 Match）。

### 5. Match 的邏輯負擔

- 內容：AI 在格子上尋路（A*）、戰利品與背包的權威、撤離計時。
- 成本落在哪條執行緒：Match 的 Tick 執行緒。
- 修法方向：尋路交給 worker，結果在指定的 Tick 套用，維持權威的決定性。伺服器端最可能在這裡第一次需要 Job 系統。
- 開工前要收集的證據：Tick 處理時間與 AI 數量、尋路次數的關係。
- owner 的候選：玩法規則在產品；Job 系統在 Engine（建在 Threads 上）。

### 6. 跨局保存（倉庫）

- 內容：需要後端服務與 Data Contract（版本、驗證規則），和執行緒無關。
- 開工前要收集的證據：保存的資料範圍與一致性要求（玩法決定之後才能定）。
- owner 的候選：產品（服務與 Data Contract）。

### 7. Render 執行緒的拆分

- 時機：排在第 2、3 項之後；主執行緒量到長幀才考慮。
- 限制：SDL 3.4 規定 swapchain 的 claim／acquire／present 與事件 pump 只能在視窗執行緒，所以能拆的只有錄製：render 執行緒畫到離屏貼圖，主執行緒負責事件、貼到 swapchain、present。
- 代價：多一次拷貝，延遲可能多一幀；command buffer 能否跨執行緒錄製要先查證。
- 開工前要收集的證據：第 2、3 項處理後，主執行緒仍有的長幀與其內容。
- owner 的候選：Engine（Render）。

### 未決事項（由使用者決定）

- a. 迷你塔科夫是新產品，還是 `object_fps_pvp` 的演進？如果是新產品，`ClientSimulation`、`MatchRuntimeHost`、三角色編排、碰撞索引都會有第二個使用者，符合 [v7 README](../v7/README.md) 寫的 framework 層啟動條件，也牽涉 Product Boundary（AGENTS §7）。
- b. GridMap 要沿用舊的 2D 格子，還是重做成在 3D 格子上擺放網格模型（類似 Godot 的 GridMap）？
  - 舊的 2D 格子：`object_fps`、`object_fps_v2` 的 `GridMap`／`GridMapLoader`／`GridCollision`／`MapGeometryGenerator`（這兩個產品在 `engine/config/projects.csv` 都是 `enabled=0`）；`apps/object_fps_pvp/include/RetroFPS/World/GridMap.hpp` 也有一份。
  - 3D 格子需要編輯器，要走 Editor → Data Contract → Application（AGENTS §8）。
- c. 建議的 owner（只是建議，未決）：GridMap 先放在產品內，只把通用部分（格子空間索引、區塊網格的建構）升到 Engine；等第二個遊戲用同樣的方式組合，再抽 framework。

## 依賴

- 建議在 v7 之後開始：v7 的執行緒分離與時間服務會改變手感相關的時序；在那之前調整玩法，v7 之後可能要重調。
- 第 16 批之後房間是 4 人；v8 的玩法以 4 人為前提。
- 上面「預計壓力」的修法多數建立在 v7 的 Time／Threads 之上，例如 Job 系統建在 Threads 上，worker 數＝核心數扣掉角色執行緒。

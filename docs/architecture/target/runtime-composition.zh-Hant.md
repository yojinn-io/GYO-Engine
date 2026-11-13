# Runtime 的組合

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §H（多 World 與 schedule 的組合）、§0 用語。
本文件只寫**使用者已裁決**的部分；規格中仍標「待使用者」的條目不在這裡。

## 1. 名詞（spec §0）

- **Runtime**＝一個 World、它的 Registry、它的 schedule 集合，加每個 schedule 的 Executor 策略與執行緒綁定。
- **Host**＝建立一或多個 Runtime、擁有執行緒、時鐘、外部服務與交換點的那一層：**提供 runner 的 plugin**（行程入口、pump、交換點、多時鐘協調）加產品的 `main`（plugin 清單加 Run）；framework 已退役（§4）。
- 一個 World 一條擁有執行緒；兩個時鐘在不同執行緒就是兩個 World，經 Host 的交換點交換（spec H-2）。

## 2. Client：Frame World 與 Render World 分開（使用者 2026-11-11 決定，spec K.3 第 6 項）

- Client 有三個 Runtime：`ClientSim`（World S，FixedUpdate，runner＝`gyo-client-sim`）、`ClientFrame`（World F，FrameUpdate，runner＝Main）、`ClientRender`（World R，`Extract` → `RenderSubmit`，與 F 同在主執行緒）。
- **Render World 自己的 Registry**（各自凍結；render plugin 只登記到渲染 Registry，主 Registry 不因渲染而變 fingerprint）。
- **渲染只經 Extract 以 const 方式讀來源 World，無反饋**：extract System 的參數 `Extract<Read<T>>`／`ExtractRes<R>` 只讀來源；`Extract<Write<…>>` 不存在（編譯期擋下）；來源 World 在 extract 期間任何經 World API 的寫入都由存取驗證（可關）記為違規、以 Err 回傳（2026-11-12 改寫，原文「debug 下…斷言」）；渲染 Runtime 不得以任何參數寫回來源，含 boundary Resource；來源 Registry 必須先凍結（spec H-6、J-43；DI-KS 13）。
- 代價：多一個 World 與每幀一次 extract。使用者的理由：合起來看著簡單，代價很高——合併後「渲染不改模擬」只能靠 RenderSubmit 唯讀的存取宣告守，而宣告的真實性在 C++ 攔不到（LIM-3）；分開則由機制守。
- 交換點：意圖 F → S、呈現 S → F 經 Host 擁有的 Mailbox，只在 schedule 之間存取；Link 經 Host 與網路執行緒在 S 的 schedule 前後交換。

## 3. Match：Join／Leave 在 tick 內（使用者 2026-11-11 決定，spec K.3 第 10 項）

- Match 的 FixedUpdate：`Ingress`（zone=B，Z3）的 `StageMembershipRequests` 把 Link 收到的加入／離開請求寫進 `MembershipRequests`（以請求序號為鍵、帶 ingestedAt），`StageInputs` 把 ack、動作、輸入、到達時刻寫進 `InputRing[tick]`，**不生成、不刪除實體**；`Simulate`（zone=C）的 `ApplyMembership` 依游標讀 `MembershipRequests`（spec H.2b 第 4 點），以 `SpawnWithId<Bundle<…>>`（id 由 Gateway 配，E-2）生成、以 `Despawn` 刪除（它是 (c)，Z5 允許）。
- 不為 Host 開 Z2／Z5 的例外：Host 在 World Ready 時只寫 boundary 與 external Resource，不直接生成或刪除帶 (c) 欄的實體。
- 後果記為限制 LIM-5（`limits.zh-Hant.md`）：加入與離開晚最多一個 tick；golden 可能差一 tick、要重設基準；產品線在遷移批確認。entity-model §5.2「Join／Leave 在 Tick 外」改寫，排進改造的第一個文件批。

## 3a. Loading schedule：世界內容的載入階段（使用者 2026-11-11 決定，spec H.2b）

- Startup 只放 Runtime 建構時就確定、不依內容的東西；**世界內容全部經 `Loading` schedule 進來**，初始成員不走 Startup。
- Loading 的 stage 固定序：**Ingress** → **資產** → 地圖 → 場景 → 可互動物件 → 敵人 → 玩家；可分批（Loading 可跑多次，每次一批，由 Host 迴圈驅動）。Ingress 是 Z3 的 ingress stage，Loading 掛兩個 (b) System（`StageMembershipRequests`、`StageAssets`；第三個 `StageInputs` 只在 FixedUpdate），各消費自己那份 external 收件 Resource（`LinkMembershipInbound`、`AssetInbox`），寫進 role=Input 的 (c) 暫存（`MembershipRequests`、`AssetStaging`，都以請求序號為鍵）；`資產` stage 的 `ApplyStagedAssets`（(c)，`AssetCursor` 唯一寫者）依序號的連續前綴套用進 `Assets<T>`，之後各 stage 的 (c) System 從 `Assets<T>` 建內容，每個 stage 等前一個 stage 的完成旗標 `LoadingStageDone<Stage>`（每 stage 一個 Resource，唯一寫者是該 stage 的 `Finish<Stage>` System；`資產` 的完成條件＝`AssetCursor` 到 `AssetRequestList` 長度；spec H.2b 第 3 點）（assets §1；spec H.2b、N-2／N-8 修訂）。資產載入本身是 IO 服務，只到 `AssetInbox` 為止。
- 玩家在「玩家」stage 由 `ApplyMembership` 以 `SpawnWithId` 生成，與中途加入是同一個 System；Z2 無例外。
- 對局時鐘在 Loading 宣告 ready 後才開始：FixedUpdate 的 domain tick 計數在 ready 時為 0、第一個模擬 tick 是 1（spec H.2b 第 5 點）；Loading 的計數不是遊戲 tick。ready 是 (c) 的判定（`Finish<玩家>` 在所有 stage 旗標齊時寫 `LoadingState = Ready`），Host 在交換點讀它。Client 的 `ClientSim` 也有 Loading schedule：請求清單來自 Match 的 manifest、序號同源，成員與 `tick[Fixed]` 在 Client 自己 ready 且收到 Match 的 ready 後經第一個權威快照的 `Restore` 進來，之後才跑 FixedUpdate（spec H.2b 第 8 點、H.4.2）。Client 收到 Match 的 ready（wire 事件；pv10-requirements）前停在 loading 狀態。
- 決定性：載入的內容身分是 (c)，載入順序是 Plan 的函數，ready 時兩端 (c) 狀態可比對。
- Feature：loading 頁面與分批載入。使用者理由：現在物件少就算 Startup，將來東西多就撐不住，先把契約訂好。
- **Peer 與 Player 分開**：Player 實體（玩家段 id；Body／Life／InputCursor／Connection 診斷）；Peer 實體（每收件者的 Ledger、Visibility）獨立，為觀戰者等留位置；Link 是 external Resource 不是實體（networking §3b）。

## 3b. Editor：文件 World 加渲染 World，資料只往前流（使用者 2026-11-11 決定，audit §3.2）

- Editor 的組合與 Client **同形**：文件 World（兼 Frame）加渲染 World，Extract 單向、唯讀。
- 文件 World 產出場景（幾何、相機）與 **UI 繪製資料**（ImGui draw list，CPU 端頂點與指令，(b) 資料）；兩者經 Extract 進渲染 World。合成在渲染 World：先畫場景到 render target，再畫 UI draw list；預覽視窗是 draw list 裡引用該 render target 的貼圖 id，由渲染 World 解析。**沒有影像回到文件 World；H-6 原樣、不開例外、沒有晚一幀。**
- 選取與編輯用 CPU 射線打文件 World 的幾何，不依賴影像。
- ImGui 是 plugin，拆兩半：輸入與 widget 邏輯是文件 World 的 System；draw list 的 GPU 繪製是渲染 World 的 System。今天 `OffscreenPreview` 的 readback 與 `ImGuiShell` 的「一個函式從 NewFrame 到 Present」消失。參考：Bevy 的 bevy_egui。
- 使用者原則：不要用舊思路處理新架構——「readback 放哪」三個選項（晚一幀、H-6 例外、文件與渲染合一）全部不採。

## 4. framework 退役；組合與第三方對接由 plugins 層承擔（使用者 2026-11-11 決定，audit §3.1）

- **三層改為**：Engine（Kernel＋能力）／**plugins**（`plugins/`：組合與第三方對接，每個 plugin 可選可刪）／產品與工具（plugin 清單＋Run）。層與目錄叫 plugins，單個才叫 plugin（使用者 2026-11-11）。framework 是舊架構的產物，其兩個核心職責（組合 Engine 能力、對接第三方）在 plugin 機制下都由 plugin 承擔，概念淘汰；`docs/framework/` 的 Design Invariants 整節廢止（第一個文件批）。
- **runner plugin**：Editor host 的骨架（行程入口、pump、交換點）是一個在 finish 安裝 runner 的 plugin（照 Bevy `WinitPlugin::set_runner`）；一個 Runtime 只能有一個 runner（spec R-E6）；沒有 plugin 安裝時由 Host 程式自己跑。
- **標準 Editor stage 序**＝plugin 群在 build 插入 stage（spec C.2 第 2 點）；ImGui、SDL 的相容在各自的 plugin 內實現，全域 context 明列例外。
- **`plugins/` 是新的 top-level**（AGENTS §10 Delta，已同意）：每個 plugin 一個 target、自己的依賴正向清單；Engine 永不 include 它。
- **plugin 的選擇是資料，不是程式**（使用者 2026-11-12 決定，`v8-arch-plugin-selection-data`；取代上一版「產品與工具只在 main 列它」）：產品與工具在 `project.json` 加 `plugins` 欄列要用的 plugin；共通 CMake 讀它連結對應的 plugin target，並**由建置產生登記的編譯單元**（與今天 `components` → `GyoComponents.cmake` 同一條路）；`main` 保持薄，只呼叫 runner plugin 或 Host 自跑。同一事實只在 `project.json` 一處（AGENTS §9）；加一個產品仍是「內容＋登記資料」就完結（§7、§12 Add／Clone）。代價：plugin 的初始化順序由資料或 plugin 自己宣告的依賴決定，不靠 main 的呼叫順序；缺 plugin 或依賴衝突是 Contract 的明確錯誤（§9）。System 本身的登記仍是程式（plugin 的 build 函式向 Registry 登記），不從資料讀。
- 「兩個以上產品以同一方式組合者先進 framework」→「先成為共用 plugin」。架構師保留：四個工具重複的選擇窗記帳與關窗提示必須真的進共用 plugin，否則是 framework 的老問題換名。
- 組合格式（catalog plan、event sounds）是資料契約，owner 是提供該 plugin 的人；純函式（EventSounds、CatalogPlan、PreviewCamera、UiTextInput、json binding）是函式庫，與層無關。

## Design Invariants（候選；核准後成為正式條文）

1. **渲染只經 Extract 以 const 方式讀來源 World，無反饋**；渲染 World 有自己的 Registry；來源 Registry 先凍結。守衛：spec J-43（含編譯失敗測試 `Extract<Write>`）、J-16。
2. **一個 Runtime 只有一個 runner，由 plugin 在 finish 安裝或 Host 自跑**；Engine 不 include 任何 plugin；**產品與工具的 plugin 選擇只寫在 `project.json`（資料），登記由建置產生，`main` 不手寫 plugin 清單**。守衛：spec R-E6；依賴方向掃描（`engine/` 不 include `plugins/`）；守衛掃 `main` 與產品程式不直接引用 plugin 的 build 函式（名稱由 plugins 規劃批定）。
3. **世界內容只經 Loading schedule 進來（且只經它的 Ingress stage 進 (c)）；對局 tick 計數在 ready 時為 0、第一個模擬 tick 是 1，Loading 期間 `tick[Fixed]` 不推進；`tick[Loading]` 是批次計數，(c) System 可讀它（S-4）但等價測試不比它；Player 與 Peer 是不同實體**。守衛：啟動檢查（Startup 的 System 不得寫世界內容欄；Loading 非 Ingress stage 的 System 不得讀 external Resource）；等價測試（分批數不同、資產完成順序打亂：ready 時 (c) 欄與 Resource 逐位元相同，排除 role=Input 的暫存（`AssetStaging`、`MembershipRequests`、`InputRing`，修剪時機隨批數而異）與 `tick[Loading]`；spec J-51）；掃描（Peer 元件不掛在 Player 實體）。
4. **一個 World 一條執行緒**：同一 World 的所有 schedule 綁同一執行緒；不同執行緒上的時鐘是不同的 World，只經 Host 交換點交換。守衛：spec J-15、J-6；finish 檢查 H-2。

### Replacement Criteria（候選）

- LIM-5：Join／Leave 在 tick 內，加入與離開晚最多一 tick；不解除（詳細在 `limits.zh-Hant.md`）。
- 渲染需要寫回模擬 World（例如渲染驅動的 LOD 狀態要進判定）→ 不是放寬 DI 1，而是把那份狀態搬到來源 World 由模擬側的 System 寫，渲染仍只讀。
- 第一個真實能力接進機制時仍要改 Host 迴圈或 Kernel 本體，或 Host 的協調邏輯在兩個以上 host 重複 → 「Host 又長回來」，停下（spec K.2 第 7 條）。

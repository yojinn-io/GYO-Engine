# 事前宣告：多個 World、交換點、Product 層 Resource（`v8-arch-gyo2-exchange`）

狀態：修訂 0（草稿，敵對式檢查中）。動工前核准之前不實作。

性質：實作段第 5 批「多個 World、交換點」的事前宣告（AGENTS §2 基盤 Architecture、§3 Architecture Delta、§4 系譜）。連同編排層缺的「Product 層 Resource」一起推（`v8-arch-gyo2-product-level-resource-open`、`v8-arch-gyo2-batch5-start`）。

用詞：「設計段／實作段」「動工前核准／合併前核准」「第 N 步」照第 3 批。標「**本卡照 L17 判**」的是這張卡的判斷（名字照命名規範、細節照別家原始碼），**不是使用者的話**。本卡新加的詞第一次出現時標「本卡加的詞」。

---

## 0. 根據與讀的版本

**程式**：`gyo2` 45f13757（#266 之後）。講現況的句子是讀程式寫的，行號照這個版本。

**別家原始碼**（`~/Code/GitHome`，本機）：偵察報告在主 checkout `build/evidence/gyo2-exchange/`：
- `survey-bevy-lightyear.md`：Bevy、lightyear、bevy_replicon（Opus 5.5／high）。
- `survey-unity-flecs-mass.md`：Unity Entities、Unity Netcode、flecs、MassSample（Opus 5.5／high）。

版本與引用見第 4 節。第 4 節每一列標明「原始碼」或「一般知識」。

**文件**：

| 文件 | 狀態 | 本頁怎麼用 |
|---|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md)第 5、6 組各列（文件分支 `claude/gyo2-time-design`） | 主會話 | 誰等這一批、等什麼（第 3.2 節） |
| [統合](kernel-app-boundary-declaration.zh-Hant.md)修訂 38：L19、第 3 節、9.2 的 O-3、O-4 | 已核准 | 交換點、extract 的時機、執行緒的安排歸編排層 |
| [編排層](orchestration.zh-Hant.md) O-1～O-11、RC-O1～RC-O10 | 已合併 | 本批改 O-2、O-3、O-4、O-5，加 O-12、O-13（第 7 節） |
| [時間設計](rebuild-time-design.zh-Hant.md)第 5.1～5.5、6.4 節 | 已核准（決定 1～7） | 交換點的原始形狀、三種組合、跨 World 事件的形狀 |
| [world-time](world-time.zh-Hant.md) 第 3 節、WT-6 | 已合併 | 取樣的過渡（第 5.5 節、第 12 節第 1 題） |
| [rebuild-concept](rebuild-concept.zh-Hant.md) M11、D6b（含 2026-11-22 的加註） | 已核准 | 交換點的不變量 |
| [runtime-composition](../runtime-composition.zh-Hant.md) 第 2、3b 節（11-11 K.3 第 6 項） | 使用者 11-11 決定 | Client 三個 World、Editor 文件 World＋渲染 World、Extract 唯讀 |
| [kernel](../kernel.zh-Hant.md) DI-KS 13、15、26；[cross-component-rules](../../../cross-component-rules.md) CCR-5 | 已合併 | Extract 的 Kernel 那一半；單執行緒的前提 |
| [回滾](rollback.zh-Hant.md)（`RollbackInbox`、R-3）、[權威](authority.zh-Hant.md)（DI-A 7） | 已合併（#259） | 網路寫進 World 的位置 |
| [日誌宣告](logging-declaration.zh-Hant.md) 5.4 | 已合併（#256） | Kernel 診斷紀錄「runner 在兩次 Run 之間取走」 |
| asset 宣告（分支 `claude/gyo2-asset-585196`，55a14825 動工前核准，1b120a86 紀錄） | **已核准、未合併** | X-1～X-6（5.6）、O-A1～O-A4（5.4）、G-1～G-3（5.13）、第 22 節 |

任務卡寫「asset 5.5 的 X-1～X-8」「G-1～G-4」：核准版（55a14825）的交換點需求是 5.6 的 **X-1～X-6**（X-7、X-8 是修訂 0、1 的編號，已併入），生成世界的是 5.13 的 **G-1～G-3**。本頁照核准版。

---

## 1. 這一批做什麼（先用遊戲講）

**PvP 的 Client**：三個 World——模擬（固定步，自己的執行緒）、畫面（每幀，主執行緒）、渲染（每幀，主執行緒，只經 Extract 唯讀拿畫面 World 的資料）。網路的執行緒收到伺服器的快照，模擬 World 每一輪開頭把它們寫進 `RollbackInbox`；畫面 World 把玩家的指令交給模擬 World，模擬 World 把「這一步角色在哪」交回畫面 World。貼圖的解碼在 IO 執行緒，畫面與渲染兩個 World 都要用同一張貼圖。

這裡有三種「資料進出 World」：
1. **外部服務的結果進 World**（網路的快照、asset 的載入結果、OS 的輸入）：服務在 World 之外、有自己的鎖；World 在每一輪的固定位置一次拉進來。
2. **World 之間的資料**（指令、位置）：兩個 World 可能在不同執行緒，誰也不能等誰。
3. **渲染讀畫面**：渲染 World 在一個時點唯讀地看畫面 World 的資料（Extract），不寫回。

還有一個缺口：asset、網路、audio 的混音器、執行緒池都是「整個 Product 一份」的東西，一個 Module 建、別的 Module 要拿得到。今天編排層只有每個 World 一份的地方（Resource、選項包、`ModuleBuffer`），沒有 Product 一份的。

**單機的 2D 遊戲**：一個 World、一條執行緒。上面這些都不需要時，作者什麼都不用寫、也不付費。

---

## 2. 範圍

### 2.1 這一批做的（動工前核准、使用者說開始之後）

- **第 0 步**（已做，見第 15 節）：第 4 批宣告 9.5 延下來的兩條守衛。
- **Product 層 Resource**（第 5.2 節）：Product 一份、以型別為鍵的共用處；產品給 Module 的 Product 層選項；`Configure` 的錯誤回報；Product 銷毀時的回呼。**這部分實作**（asset 是直近的使用者）。
- **交換點的契約**（第 5.1 節）：資料進出 World 的三種形、各自的時點與規則，寫進編排層的 Design Invariants。
- **多個 World 的執行緒契約**（第 5.3 節）：一個 Product 的 World 能不能、怎樣在不同執行緒跑。**只定契約與介面的形，不實作**（點名的使用者 PvP Client 還沒搬回）。
- **Extract 的時機與來源替換**（第 5.4 節）：**只定契約與介面的形**；執行的原語與編排層的呼叫在 SDL 那批（render 回來時）實作（批次表 SDL 列「render 回來時一起做 K.3-6」）。
- 編排層、Kernel 的 Design Invariants 與 Replacement Criteria 的改寫（第 7 節）。

### 2.2 不在這一批

- 網路 Module 本身、SDL 的 runner、asset 的實作、threads／io／audio 的重審：各自的批，照第 5.6 節的對照接上。
- 跨 World 的事件（時間設計 6.4，決定 7）：照舊延後，形狀不變（第 5.1 節末）。
- runner 的取樣與等待零件（時間設計 5.4）：看第 12 節第 1 題。
- 生成世界：另一張卡（`gyo2-world-gen`）；交界在第 13 節。

---

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

- Product 層 Resource 是之後每個「整個 Product 一份」的 Module（asset store、網路的連線、audio 的混音器、執行緒池、SDL 的裝置）接進來的地方；形狀一改，它們全要改。
- 交換點的規則決定每個接外部資料的 Module 怎麼寫（在哪個時點拉、能不能上鎖、能不能等別的執行緒）。網路、SDL 的輸入、asset 都會照它寫。
- 多個 World 的執行緒契約決定 runner、Product 的入口、`Product` 的內部狀態能不能被兩條執行緒碰。PvP Client 與 pipelined 渲染都積在上面。
- Extract 的時機決定渲染 World 與它的來源怎麼一起建、一起替換。

### 3.2 予見的根據 → 要求（Feature → Contract → Architecture）

根據的種類照 AGENTS §2：1＝名指的 Feature；2＝參考實作；3＝規模與預算。

| # | 根據（種類） | 從誰來 | 要求（Contract） | 本頁 |
|---|---|---|---|---|
| E-1 | asset：擁有型別的 Module 拿到 Product 的 `AssetLoaderRegistry`（1） | asset O-A2、5.4、5.16 | Product 一份、一個單位建、別的單位找得到的共用處 | 5.2 |
| E-2 | asset：產品給 `AssetOptions`，在 world_asset 的 `Configure` 之前就在（1） | asset O-A1 | 產品在描述裡給 Product 層的選項，單位的 `Configure` 讀得到 | 5.2 |
| E-3 | asset：`Configure` 階段的錯誤回報（1） | asset O-A3 | `ProductSetup` 能回報錯誤，收進 `ProductStartupReport` | 5.2 |
| E-4 | asset：Product 結束時記丟棄件數、停下工作（1） | asset O-A4、X-6 | Product 銷毀時，依序呼叫單位登記的回呼；在所有 World 銷毀之後、共用處銷毀之前 | 5.2 |
| E-5 | asset：讀檔與解碼在 World 執行緒外跑；執行器由這裡給（1） | asset X-1、X-4 | 執行器（或執行緒池）是 Product 一份的物件，放在共用處；執行緒的數量與歸屬由產品／runner 定 | 5.2、5.3 |
| E-6 | asset：交換點不認得 asset；World 靠自己的套用點看到結果（1） | asset X-2、X-3 | 「外部服務的結果進 World」走套用點（每一輪固定位置的 System），編排層不認得內容 | 5.1 |
| E-7 | asset：測試能控制工作何時開工、完成（1） | asset X-5 | 執行器可換（放在共用處的是介面） | 5.2 |
| E-8 | 網路：伺服器的權威值寫進 `RollbackInbox`，下一幀開頭判斷（1） | rollback.zh-Hant.md 第 2 節、DI-A 7 | 網路的收件在模擬 World 每一輪開頭的套用點寫進 World；送件在每一輪最後 | 5.1 |
| E-9 | 網路：同一個行程裡的伺服器 World 與 Client World（host、測試）；IO 可能在別的執行緒（1、2） | 批次表網路列；Unity Netcode、lightyear（第 4.2 節） | 每個 World 有自己的驅動（Unity Netcode 的 singleton 驅動）；World 之間不直接碰，經行程內的傳輸接；行程內的傳輸、IO 執行緒是 Product 一份的物件、有自己的鎖 | 5.2、5.1 |
| E-10 | SDL：OS 事件在每一輪開頭進 World；有視窗的 runner（1、2） | 批次表 SDL 列；時間設計 5.5 (二)；Bevy winit（第 4 節） | runner 在兩次 Run 之間可以寫 World；或輸入服務＋套用點 | 5.1 |
| E-11 | SDL：渲染 World 有自己的 Registry，只經 Extract 唯讀拿資料（1） | runtime-composition 第 2 節（K.3 第 6 項）、DI-KS 13、批次表 SDL 列 | Extract 的時機、來源的設定與替換、Kernel 的執行原語 | 5.4 |
| E-12 | SDL：render 的裝置是 Product 一份；貼圖的 loader 要等裝置建好（1） | asset 5.4「預先登記」 | 裝置放共用處 | 5.2 |
| E-13 | PvP Client：模擬 World 在自己的執行緒，主執行緒停 250 ms 時模擬照跑（1） | 時間設計 5.5 (三)；runtime-composition 第 2 節 | 一個 Product 的 World 可以在不同執行緒跑；World 之間的交換不等另一條執行緒 | 5.3、5.1 |
| E-14 | Editor：文件 World＋渲染 World（1） | runtime-composition 第 3b 節 | 同 E-11 | 5.4 |
| E-15 | threads 重審：`RoleThread` 的②③是 runner 的執行緒（1） | 批次表 threads 列、R0 附錄 | 執行緒由 runner 建、由 runner 收；World 不綁執行緒 | 5.3 |
| E-16 | audio 重審：`Submitter` 歸第一個呼叫它的執行緒；混音器有自己的迴圈（1） | 批次表 audio 列 | 混音器放共用處；綁執行緒的物件自己記執行緒（`v8-arch-gyo2-thread-binding-by-runner`） | 5.2、5.3 |
| E-17 | io 重審：多執行緒同時讀檔的契約（1） | asset 5.12 | 不在本頁定；本頁只要求「World 外的服務自己負責自己的同步」 | 5.1 |
| E-18 | 時間：「一次醒來只取樣一次」、取樣歸 runner（時間設計決定 1 B；WT-6 第 (2) 項的過渡） | 時間設計 5.2、5.4；world-time 第 3 節 | 看第 12 節第 1 題 | 5.5 |
| E-19 | 日誌：Kernel 的診斷紀錄由 runner 在兩次 Run 之間取走（1） | 日誌宣告 5.4 | runner 在兩次 Run 之間讀寫 World 是正式的時點 | 5.1 |
| E-20 | 生成世界：分批之間 asset 的套用 System 會跑（1） | asset G-1～G-3 | 套用點是 World 裡的 System，跟著 World 的每一輪走 | 5.1、13 |
| E-21 | 規模：PvP 64 Hz、Client 三個 World；Match 一個 World；工具一到兩個 World（3） | 時間設計、runtime-composition | World 數是個位數；Product 層的查找不必比線性快（照 `OptionBag`、`ModuleBuffer` 的線性表） | 5.2 |

根據 2（參考實作）單獨的要求不收（AGENTS §2）。

**導不出的、交使用者的**：第 12 節。

---

## 4. 別家怎麼做

讀的版本：Bevy 0.20.0-dev（6a911653，2026-11-10；extract 已在 `bevy_extract` crate，PR #22852）、lightyear 0.31.0、bevy_replicon 0.45.5、Unity Entities 1.5.0、Unity Netcode 7.2.0（它宣告依賴 Entities 6.7.0，本機鏡像是 1.5.0，從 Netcode 看到的 Entities 行為可能不同，未確認）、flecs 4.2.0、MassSample（UE 5.8）。路徑相對於 `~/Code/GitHome`。〔原〕＝讀了原始碼；〔知〕＝一般知識，沒有在原始碼確認（Unreal 引擎本體不在本機）。細節與更多引用在兩份偵察報告。

### 4.1 資料進出 World

| 別家 | 怎麼做 | 種類 |
|---|---|---|
| Bevy asset server | IO 工作經 crossbeam channel 送結果；`PreUpdate` 的 `handle_internal_asset_events` 一次收（`bevy_asset/src/server/mod.rs:896-923`、`:1889-1938`） | 〔原〕形 (1) |
| Bevy gilrs | `PreUpdate` 的 System 把手把事件取完（`bevy_gilrs/src/lib.rs:109-111`） | 〔原〕形 (1) |
| Bevy winit | 視窗事件先存在 runner 的緩衝，update 之前 `forward_bevy_events` 一次寫進 World（`bevy_winit/src/state.rs:779-787`、`:908-926`） | 〔原〕形 (2) |
| lightyear | 收件全部在 `PreUpdate`、送件全部在 `PostUpdate` 的 set；權威值先進 `ConfirmedHistory`，回滾在 `PreUpdate` 判斷並重跑 `FixedMain`（`io/link/src/lib.rs:313-348`、`prediction/src/plugin.rs:246-253`、`rollback.rs:1186-1307`）。UDP 在 System 裡直接讀非阻塞 socket，沒有 IO 執行緒（`io/udp/src/lib.rs:186-251`） | 〔原〕形 (1) |
| bevy_replicon | `PreUpdate` 收、`PostUpdate` 送（`bevy_replicon/src/client.rs:53-69`、`server.rs:151-163`）；兩個 App 之間的測試交換在兩次 update 之間搬緩衝（`test_app.rs:136-153`） | 〔原〕形 (1)、(2) |
| Unity Netcode | receive group 在 Simulation 的最前面，傳輸的 pump 是 job，結果填進每個連線的緩衝；快照只留最新一份（`Runtime/Connection/NetworkStreamReceiveSystem.cs:24-38`、`:983-992`）；送件在最後（`GhostSendSystem.cs:505-510`） | 〔原〕形 (1) |
| flecs | 慣例：`OnLoad` 把外部資料拉進來、`OnStore` 寫出去（`docs/DesignWithFlecs.md:135-136`）；別的執行緒用自己的 stage 累積、`ecs_merge` 在獨占時套用（`src/stage.c:169-189`）；`ecs_run_post_frame` 在 `ecs_frame_end` 跑（`src/addons/frame.c:151-199`） | 〔原〕形 (1)、(2) |
| Unreal Mass | 引擎與 Mass 的交換是處理器群組 `SyncWorldToMass`、`UpdateWorldFromMass`；物理是有自己鎖的外部服務，平行讀、最後一次寫（MassSample `MSSceneCompTransformToActorTranslators.cpp:13-22`、`:63-112`） | 〔原〕形 (1) |

**共通**：World 的資料不上鎖、不共享；一條執行緒在一個時點擁有一個 World。外部的生產者寫進自己擁有的緩衝（channel、stage、驅動的佇列），World 在固定的頭（收、`OnLoad`、`SyncWorldToMass`）尾（送、`OnStore`、`UpdateWorldFromMass`）一次套用。**沒有一家有「編排層依序呼叫的交換函式登記」**；最接近的是 flecs 的 `ecs_run_post_frame`（一次性的動作佇列）。

**落差與坑**（偵察報告列的，和本頁有關的）：交換位置排晚了就晚一幀（Unity 的串流文件：排在 `SceneSystemGroup` 之前的 System 晚一幀看到）；佇列的語意要寫明（Netcode 只留最新一份）；無上限的 channel 沒有流量控制；一個執行緒一個 stage。

### 4.2 World 之間、World 交給別的執行緒

| 別家 | 怎麼做 | 種類 |
|---|---|---|
| Bevy SubApp | 一次 update：main 的 schedule → 每個 sub-app `extract(&mut main)` → sub-app `update()`（`bevy_app/src/sub_app.rs:575-591`）。extract 時把 main World 用 `mem::replace` 取出、當 `MainWorld` Resource 放進 render World 跑 `ExtractSchedule`，完了放回，不上鎖（`bevy_extract/src/extract_plugin.rs:136-147`）。`Extract<P>` 是唯讀參數（`extract_param.rs:19-20`、`:51-90`） | 〔原〕形 (3) |
| Bevy 的寫回 | **render 在 extract 時經 `ResMut<MainWorld>` 寫回 main**：GPU readback 的結果、surface 的色彩空間（「main world sees the previous frame's result」）（`bevy_render/src/gpu_readback.rs:66`、`:275-289`；`view/window/mod.rs:301-319`）。只有 `Extract<P>` 是唯讀 | 〔原〕；**GYO 不照**（使用者 11-11 定「無反饋」，第 5.4 節） |
| Bevy pipelined 渲染 | `cleanup` 把 RenderApp 從 App 拿走，兩條容量 1 的 channel 把整個 SubApp 當值送來送去；唯一的同步點在 `renderer_extract`：main 等渲染執行緒交回上一幀的 render app，extract 完再送出去（`bevy_render/src/pipelined_rendering.rs:155-214`、`:217-240`）。PR #6503 的理由：scope 的做法讓渲染沒辦法在 IO 處理之前開始；關閉時的死結另修（PR #25722） | 〔原〕World 整個交出 |
| Unity Entities | 每個 World 的三個群組掛進 PlayerLoop，全在主執行緒，World 之間的先後＝加入的先後（`ScriptBehaviourUpdateOrder.cs:280-297`、`:400-412`）。載入用的 staging World 以 `ExclusiveEntityTransaction` 整個交給工作執行緒：交出期間 staging World 的一般 `EntityManager` 會丟例外，main World 照跑；結束只在主執行緒、先等 job 完成（`ExclusiveEntityTransaction.cs:6-19`、`EntityManager.cs:69-90`、`EntityManagerExclusiveEntityTransaction.cs:102-118`）；之後 `MoveEntitiesFrom` 帶重編號搬進 main World（`EntityManager.cs:3925-3946`），一次 update 最多搬一個 World（`Unity.Scenes/SceneSectionStreamingSystem.cs:71`） | 〔原〕World 整個交出 |
| Unity Netcode | 伺服器 World 與 Client World 在同一個行程時，**World 之間不直接碰**，只經 IPC 驅動（`DefaultDriverConstructor.cs:203-229`；`Documentation~/binary-worlds-host-mode.md:3-5`）；每個 World 有自己的驅動（singleton `NetworkStreamDriver`） | 〔原〕 |
| lightyear、bevy_replicon | 同一個行程的 host：一個 App、一個 World，Client 端模擬（`replication/src/client.rs:117-123`）；replicon 明寫一個 World 同時當兩端會讓複製繞圈（`src/lib.rs:485-528`）；測試用兩個 App 以 crossbeam 接起來，用 `ManualInstant` 發同一個時間再依序 update（lightyear `tests/src/stepper.rs:803-844`） | 〔原〕 |

**共通**：World 交給別的執行緒時是**整個交出**（值的移動或所有權的交接），不是加鎖共享；兩邊只在交還的同步點碰面。模擬的 World 之間不直接共享資料。

### 4.3 App（Product）層的共用物件

| 別家 | 怎麼做 | 種類 |
|---|---|---|
| Bevy | **`App` 只持有 `SubApps`、runner、錯誤處理，World 之外沒有 Resource**（`bevy_app/src/app.rs:85-96`）。俗稱的「App 層 Resource」是 main World 的 Resource：Plugin 在 `build` 時 `insert_resource` 進 main World。要兩個 World 都用的物件（`AssetServer` 是 `Arc`、`RenderDevice`／`RenderQueue`）各放一份副本（`bevy_render/src/lib.rs:427`、`:435`；`settings.rs:184-218`）；非同步建好的東西經 `Arc<Mutex<Option<_>>>` 從 `ready` 交到 `finish`（`lib.rs:375-376`、`:478-507`）。Plugin 的設定在建構時是 Plugin 的欄位（`AssetPlugin`，`bevy_asset/src/lib.rs:237-248`），執行時讀的才放 Resource | 〔原〕 |
| lightyear | 登記表 `ComponentRegistry` 是 App（main World）的 Resource，預測套件與作者在 build 時改它 | 〔原〕（第 4 批第 6 節已引） |
| Unity Entities | singleton 是每個 World 一份，文件建議放持有服務的容器（`Documentation~/components-singleton.md:19-28`）；行程一份的只有 `SharedStatic` 這類內部機制 | 〔原〕 |
| flecs | 每個 world 一個 ctx 槽＋釋放函式（`flecs.h:2296-2341`）；文件建議用 singleton 放「物理引擎、網路 socket 的 handle」（`docs/EntitiesComponents.md:2147`）；`ecs_atfini` 的回呼在根實體刪完之後、儲存銷毀之前，**依登記的順序**跑，ctx 的釋放最後（`src/world.c:1066-1076`、`:1079-1155`） | 〔原〕 |
| Unreal | `UWorldSubsystem`、`UGameInstanceSubsystem`、`UEngineSubsystem`：以類別為鍵、自動建立、有依賴的先後；Mass 用到的 World subsystem 要宣告自己的執行緒安全（MassSample `README.md:262-288`〔原〕） | 〔知〕（subsystem 的範圍與生命週期） |

**共通與分歧**：別家的共用物件都放在「某個 World」或「行程全域」；**沒有一家有獨立於 World 的、Product 一份的型別化存放處**，最接近的是 Unreal 的 GameInstance subsystem〔知〕。Bevy 能用 main World 當這個地方，是因為它有一個「主」World、而且 Plugin 的 `build` 時 World 已經在；GYO 的 World 彼此平等（沒有主 World），而且 `Configure` 時還沒有任何 World（O-2），所以 Bevy 的做法搬不過來。這是第 12 節第 3 題。

### 4.4 時間的取樣

| 別家 | 怎麼做 | 種類 |
|---|---|---|
| Bevy | `First` 的 `time_system` 在 World 裡取樣，策略是 `TimeUpdateStrategy` Resource；有渲染時，取樣點是 render 在 present 之後送來的 `Instant`（crossbeam 容量 2），main 用它——一幀一個取樣，產生在 render、用在 main（`bevy_time/src/lib.rs:143-195`、`bevy_render/src/lib.rs:424-434`、`:522-537`；PR #4744：取樣要盡量貼近 present）；pipelined 下留著 TODO（`bevy_time/src/lib.rs:164`） | 〔原〕World 裡取樣 |
| Unity Entities | `InitializationSystemGroup` 裡的 `UpdateWorldTimeSystem` 讀引擎的 delta，寫進 World 的 `WorldTime`（`Unity.Entities.Hybrid/UpdateWorldTimeSystem.cs:12-43`） | 〔原〕World 裡取樣 |
| flecs | `ecs_progress(world, delta)`：呼叫者給 delta；給 0 時 `ecs_frame_begin` 自己量（`src/addons/frame.c:111-149`、`pipeline/pipeline.c:767-801`） | 〔原〕呼叫者（runner）取樣，可選 |
| lightyear 的測試 | 用 `ManualInstant` 把同一個時間發給兩個 App | 〔原〕runner 取樣（測試） |


---

## 5. 設計（Contract）

### 5.1 交換點

**已定的不變量**：外部系統不持有、不修改 World；只經受控的交換點交換資料（M11）；外部訊息只在邊界批次進入 World（D6b）。2026-11-22 的加註（asset 宣告第 15 節第 11 項）：World 的資料不上鎖；System 呼叫外部服務時，服務可以拿它自己的鎖，結果只在套用點或交換點批次進 World；同步呼叫外部服務帶回的值，只能是不進雜湊、不進相等的身分資訊，不得讓判定依時序而變。

**時間設計 5.3 的原形**（規格 H.1）：每個 World 每一輪有「進」（跑 schedule 之前）與「出」（跑完之後）兩個交換點；登記單位登記交換函式，runner 依登記的順序呼叫。這是 11-14 寫的，早於 11-16～17 的分層（Kernel 只做 ECS、編排層、時鐘出 Kernel），也早於 asset 照 Bevy 定下的套用點。

**這一批要回答的**：資料進出 World，有哪幾種形、各在什麼時點、規則是什麼；要不要一個「交換函式的登記」機制。

**三種形**（本卡照 L17 整理，系譜見第 4 節）：

| 形 | 什麼 | 時點 | 誰寫 | 用它的（點名的） | 系譜 |
|---|---|---|---|---|---|
| **(1) 套用點** | World 裡的 System 呼叫 World 外的服務（有自己的鎖），把結果一次寫進 World；或把要送的交給服務 | 「進」：每一輪最前面的 stage（預設骨架的 `Main`／`First`；生成中的一輪是 `Generate`／`First`，第 13 節 W-3）；「出」：最後（`Main`／`Last`）。骨架關掉的 World 由產品指定 | 擁有服務的 Module | asset 的套用 System（E-6、E-20）、網路的收件與送件（E-8、E-9）、World 之間的郵箱（E-13）、SDL 輸入若走服務 | Bevy 的 asset server、gilrs、lightyear、bevy_replicon；Unity Netcode 的 receive system group（第 4 節） |
| **(2) runner 在兩次入口之間** | runner（或 Product 的持有者）在 World 不在 Run 裡、Ready 時直接讀寫 World | 兩次 `Product` 入口之間 | runner | Kernel 診斷紀錄的取走（E-19）、有視窗的 runner 寫 OS 事件（E-10，若不走服務） | Bevy winit 的 runner 在兩次 update 之間把視窗事件寫進 World（第 4 節） |
| **(3) Extract** | 一個 World 唯讀地讀另一個 World | 來源跑完、讀者的這一輪之前（「一輪」見第 13 節），兩個都停著 | 讀者的 extract schedule | 渲染 World（E-11、E-14） | Bevy `SubApp::extract`（第 4 節） |

**每一種的規則**（寫進 O-12）：
- (1)：World 不上鎖；服務自己同步。套用 System 只在自己的時點把結果寫進 World，World 的其餘 System 只讀 World（asset 的 AS-4、AS-11 是先例）。服務帶回的值照 D6b 的加註。**不等另一條執行緒**：拿服務的鎖只做表的操作，不等工作完成（時間設計 5.5 (三) 的條件）。
- (2)：只在 World 不在 Run 裡時；寫的是一般的 World 寫入（不跑 hook 的那條路，DI-KS 18）。runner 只有一個（O-3），所以 (2) 只適合「本來就屬於 runner 的」事（它擁有的視窗、它取走的診斷）；別的 Module 的資料走 (1)，不要求 runner 認得它們（RC-O1「Host 長回來」）。
- (3)：第 5.4 節。

**要不要「交換函式的登記」**（時間設計 5.3 的原形）：**本卡的建議是不做**，交使用者定（第 12 節第 2 題）。理由：
- 點名的使用者全部落在 (1)～(3)：asset 已照 Bevy 定成套用 System（asset X-2：「交換點不需要認得 asset」）；網路照 lightyear、Unity Netcode 是收件、送件的 System；World 之間的郵箱是有鎖的服務＋套用點；渲染是 Extract；診斷是 runner。沒有一個需要「World 不在 Run 裡、由編排層依序呼叫的單位函式」。
- 別家沒有這一層：Bevy、lightyear、bevy_replicon、Unity Netcode、Mass 都用 (1)，Bevy winit 與 replicon 的測試用 (2)，Bevy 的渲染用 (3)（第 4.1、4.2 節）。flecs 的 `ecs_progress` 前後由呼叫者做事，等於 (2)；最接近交換函式的是 `ecs_run_post_frame`（一次性的動作佇列，不是每輪的登記）。
- 加了它，套用 System 與交換函式會是兩條做同一件事的路（作者要選、審查要看兩處），違反「一個概念不要分為兩個實作」（`v8-arch-gyo2-game-time-in-world-time` 使用者原話）。
- 交換函式比套用 System 多的只有：在 World 的 Run 之外（不受 run 條件、不進 trace 的 System 段）、拿得到「第幾輪」。前者沒有點名的需要；後者若時間取樣移給 runner（第 12 節第 1 題 (乙)）才用得到，而那一題也可以用 (1) 做（第 5.5 節）。

**跨 World 的事件**（時間設計 6.4，決定 7）：照舊只定形狀、不實作。形狀落在 (1)：來源 World 的「出」套用 System 讀自己的訊息（自己的游標）、交給郵箱，目的 World 的「進」套用 System 寫進自己的訊息；每筆帶來源的 tick 與取樣時間由用它的 Module 定；郵箱有上限、丟了要記（時間設計 6.4 的兩個洞照那裡的改法）。

### 5.2 Product 層 Resource

**要解的事**（E-1～E-5、E-7、E-9、E-12、E-16）：「整個 Product 一份」的物件，一個登記單位建，別的登記單位、產品自己的程式、runner 拿得到；產品要在單位的 `Configure` 之前把設定給它們；它們在 Product 結束時要收尾。

**現況**：每個 World 一份的有三種（World 的 Resource；`WorldDescription::options` 的 `OptionBag`；每次建 World 一份的 `ModuleBuffer`）；Product 一份的沒有。登記單位的實例活到 Product 結束、可以自己持有資料（`detail/RegistrationUnit.hpp`），但別人拿不到它：用產生的單位表的產品手上沒有單位物件，單位之間也不互相交出（第 4 批第 6 節查到同一個缺口）。

**建議的做法**（要不要有這一層是第 12 節第 3 題；形狀照 `ModuleBuffer`、不加凍結，是任務卡的指定）：

1. **Product 一份的 `ModuleBuffer`**：沿用第 4 批的 `ModuleBuffer` 這個型別，`Product` 多持有一份，活到 Product 結束。規則和每次建 World 那一份相同：每種型別只由擁有它的一方 `Create<T>`（第二次 `Create` 同一型別是斷言），別人 `Find<T>`（找不到回 `nullptr`、不代建）。**不凍結**：任何時點都可以 `Create`、可寫的 `Find`（Bevy 的 main World Resource 也是任何時點都能 `insert_resource`、`resource_mut`，第 4.3 節）。拿得到它的地方：
   - `ProductSetup::Buffer()`（`Configure`，單位依序、產品自己的最後）；
   - `WorldSetup`、`WorldFinish`、`WorldCheck` 的 `ProductBuffer()`（建每個 World 的各階段；`WorldFinish`、`WorldCheck` 照它們拿每 World 那一份的寫法給 `const`）；
   - `Product::Buffer()`（runner、Product 的持有者，在兩次入口之間）。
   - 準備函式只拿 `World&`（O-7）；要用的單位在 `Build` 時把參照捕進自己的準備函式。System 要用的，由擁有的 Module 在準備函式裡放一個指向它的 World Resource（asset 5.4 已是這個寫法）。
2. **Product 層的選項**：`ProductDescription` 加 `options`（`OptionBag`，和 `WorldDescription::options` 同一個型別）。產品在描述裡放，單位在 `Configure` 起經 `ProductSetup::Options<T>()` 讀；編排層只轉交、不解讀（O-2 的寫法）。這是 Bevy `AssetPlugin { file_path, … }` 這類「Plugin 的欄位」的對應：GYO 用產生的單位表時產品手上沒有單位物件，所以設定經描述給（E-2）。
3. **`Configure` 的錯誤回報**：`ProductSetup::Report(code, message)`，形狀同 `WorldFinish::Report`；收進 `ProductStartupReport` 的 `unitErrors`（`world` 欄空）。有回報就是啟動錯誤，不建任何 World（O-2「有任何錯誤就不建任何 World」）。
4. **Product 銷毀時的回呼**：`ProductSetup::OnDestroy(std::function<void()>)`（名字本卡照 L17：同組的 `Prepare(fn)`、`InstallRunner` 用動詞；flecs 的 `ecs_atfini` 是同一件事，第 4.3 節）。`~Product` 的順序：
   1. 所有 World 銷毀（現在的順序）；
   2. `OnDestroy` 的回呼，**登記的反序**（後登記的先跑：依賴者在被依賴者之後 `Configure`，所以先收尾，被依賴者的物件在依賴者收尾時還在；和 C++ 解構、`atexit` 同向。flecs 的 `ecs_atfini` 是登記的順序（`src/world.c:1071`），本卡照 L17 取反序，理由是 O-6 的依賴保證了 `Configure` 的先後就是依賴的先後）；
   3. Product 一份的 `ModuleBuffer` 銷毀（物件依建立的反序解構）；
   4. 登記單位銷毀（現在的順序）。
   - `Product::Build` 失敗時（啟動錯誤、`Configure` 回報），已經跑過的 `Configure` 登記的回呼照樣在那個未完成的 Product 銷毀時跑（asset 的 store 在 `Configure` 建，要收尾）。
   - 回呼不回錯誤：它在解構裡跑，沒有可以交給的上層。要記的自己記（日誌的 ④ 直接寫一筆，或 `Base::Log`）。回呼丟例外是 Programmer Error（解構裡的例外會 `std::terminate`；不另包）。
   - E-4 的「停下或等完未完成的工作」由擁有執行器的一方在自己的回呼裡做。

**生命週期的保證**：Product 一份的 `ModuleBuffer` 比 Product 的每個 World 活得久。所以 World Resource 裡可以放指向它的物件的指標（O-10 禁的是指向**舊 World** 的參照，不是指向 Product 層物件的）。

**執行緒**：共用處的表（`Create`、`Find`）只在擁有 Product 的那條執行緒上碰（CCR-5，第 5.3 節）。放進去的物件若會被別的執行緒用（asset store、執行緒池、混音器、網路傳輸），**同步是那個物件自己的事**（D6b、M11 的加註：「外部服務可以有自己的鎖」）；編排層不加鎖、不知道它們是什麼（O-2）。

**不做的**：
- 凍結、只准一個階段寫（使用者撤回 asset 的凍結，`v8-arch-gyo2-asset-no-freeze`；任務卡指定不加）。第 4 批每 World 那一份的凍結照舊（使用者 2026-11-20 的決定，用途是「建 World 期間的登記表、凍結後統一裁決」，和 Product 層的長壽物件不同）。
- 每個 World 拿到的「Product 層 Resource 的投影」（World 裡自動出現一份）：Bevy 的 render 與 main 是兩個 World 各放一份 `Arc` 的副本（第 4 節），沒有自動投影；GYO 照「擁有的 Module 自己在準備函式放」。
- 新的型別名：`ModuleBuffer` 沿用。它原本的註解寫「one World build」，實作時改成「每次建 World 一份，以及 Product 一份」。

### 5.3 多個 World 的執行緒（只定契約與介面的形）

**已定的前提**：
- 一個 `Product` 持有多個 World（時間設計決定 2；編排層骨架已實作）。
- runner 決定哪條執行緒跑哪個 World；World 不綁執行緒，需要綁執行緒的 Resource 自己記來源執行緒、碰錯就斷言（`v8-arch-gyo2-thread-binding-by-runner`）。Bevy 現在把綁執行緒的 handle 放在 World 外的 `thread_local!`（`bevy_winit/src/lib.rs:55-59`，issue #17667），方向一致。
- Kernel 沒有鎖與原子操作；同一份 Registry 的登記、建 Plan、World 的建立、銷毀與執行在一條執行緒上，或由呼叫者排好不重疊（CCR-5，DI-KS 26）。
- 編排層現在把 `Product` 的建立、執行、替換、插入、銷毀都放在呼叫的執行緒上（`Product.hpp` 開頭的註解，CCR-5）。

**要求**（E-13、E-15）：PvP Client 的模擬 World 在自己的執行緒跑，主執行緒停住時模擬照跑；渲染日後可能也要自己的執行緒（Bevy 的 pipelined 渲染，第 4 節）。

**契約**（本卡照 L17 判，照第 4 節的成熟做法：**整個 World 交給另一條執行緒，交出期間只有那條執行緒碰它；兩邊只在交還的時點、或經各自有鎖的服務交換**）：

1. **單位是一整個 World**。不把一個 World 拆給兩條執行緒（World 的資料不上鎖，D6b、M11）。同一個 World 裡的並行是 Kernel 執行器的事（LIM-2），不在本頁。
2. **交出與交還**（本卡加的詞，暫名 `Product::Lend(world)` → `WorldLease`、`Product::Return(lease)`；名字實作時照命名規範定）：runner 把一個 World 交給別的執行緒時，從 `Product` 拿走那個 World 的執行入口（只能跑**它自己的**每幀清單、`RunSchedule`），`Product` 在交還之前不碰它。交出期間：
   - `RunFrame` 跳過它（照 World 順序跑其餘的）；`RunSchedule`、`ReplaceWorld`、`InsertSystems` 指名它是 E2（新碼，`WorldLent`）；`Product` 銷毀前必須全部交還（沒交還是斷言）。
   - 它在別的執行緒壞了（O-11）：那條執行緒的入口回 E1 給它的 runner；`Product` 在交還時記下失敗。交還之前 `Product` 不知道（**限界**：同一時間別的 World 可能還在跑；Product 的結束晚到交還為止）。
   - CCR-5 對交出的 World 照樣成立：它的 Registry 只在持有它的執行緒上碰；它的 `ReplaceWorld` 要先交還。
   - 交出的入口推動的是那個 World 的「一輪」（含生成世界的那幾批，第 13 節）；生成的狀態（在 Product 的 World 槽）跟著交出，交出期間只由持有的執行緒讀寫。
3. **Product 層的共用處**（第 5.2 節）的表只在擁有 Product 的執行緒上碰；交出的那條執行緒要用的物件，在交出之前拿到參照（或經 World Resource）。物件的同步是物件自己的事。
4. **World 之間的資料**（E-13 的指令、位置）：經 Product 一份、有自己的鎖的服務（郵箱；本卡加的詞，概念，不是新型別）交換，各 World 在自己的套用點拉、在每一輪最後送（第 5.1 節的第一種形）。**不等另一條執行緒**：郵箱是「只留最新一份」還是佇列、誰持有、上限，由用它的 Module 定（時間設計 5.5 (三) 的「沒寫的」照舊延到 PvP 移植）。
5. **Extract 在兩邊都停著的時點**：渲染 World 讀來源 World 時，兩個 World 都不在 Run 裡（第 5.4 節）。若日後渲染在自己的執行緒，照 Bevy pipelined 的做法：Extract 在主執行緒、渲染那條執行緒等著的同步點做，做完才把渲染 World 交出去（第 4 節）。

**thin start**：本批**不實作**交出與交還。理由：點名的使用者（PvP Client 的模擬執行緒、pipelined 渲染）都還沒搬回；現在實作的只有它自己的測試在用。本批只把上面五條寫進編排層的 Design Invariants（O-13），並把「CCR-5 放寬」寫成 Replacement Criteria 的觸發（RC-O10 已有，改寫）。實作在第一個點名的使用者那批（PvP 的 Client 移植，或 SDL 那批若要 pipelined 渲染）。

**比過的選項**：
- **(甲) 整個 World 交出／交還**（**選這個**）：系譜 Bevy pipelined 渲染（render SubApp 整個送到渲染執行緒、extract 在同步點）、Unity Entities 的 `ExclusiveEntityTransaction`（載入用的 World 整個交給工作執行緒，主執行緒在完成前不碰它）（第 4 節）。
- (乙) `Product` 的每個入口都可以從任何執行緒呼叫、內部加鎖：退掉。World 的資料不上鎖（D6b），鎖只能包住 `Product` 自己的表，兩條執行緒仍會同時跑同一個 World 的 Run；而且等於讓 Product 自己的狀態（`failure_`、`gameInitRan_`）變成跨執行緒的權威狀態。
- (丙) 一個 World 綁一條執行緒（規格 H-2）：退掉（`v8-arch-gyo2-thread-binding-by-runner` 已拿掉）。

### 5.4 Extract 的時機與來源替換（只定契約與介面的形）

**現況**：Kernel 有 Extract 的宣告那一半——`Extract<Read<T>>`、`ExtractRes<R>` 參數（`Params.hpp:1293-1312`；寫的版本編譯不過）、`ScheduleOptions::extract`、`Registry::SetExtractSource`（要在讀的那一份第一次 `BuildPlans` 之前設，`Registry.cpp:671-674`）、來源沒設或沒建過 Plan 是 D-E14（`Registry.cpp:908-911`）、Extract 參數只在 extract schedule（H-6）。**執行沒有**：執行器碰到 extract schedule 就拒絕（`Executor.cpp:499-501`，X-15：舊的 rollback-extract 已隨第 4 批拿掉），參數的建構子斷言（`Params.hpp:1299`）。編排層沒有呼叫 `SetExtractSource` 的地方（編排層骨架宣告 3.4 第 3 步留了位置）。

**要求**（E-11、E-14）：渲染 World 有自己的 Registry，每一輪在來源跑完之後唯讀地讀來源一次，不寫回（11-11 K.3 第 6 項）。

**和 Bevy 不同的一點**：Bevy 的 render 在 extract 時經 `ResMut<MainWorld>` 寫回 main（GPU readback、surface 的色彩空間，第 4.2 節）。GYO 照使用者 11-11 的決定不寫回；畫面要讀 GPU 的結果（截圖、像素拾取）時，走形 (1)：渲染那一側把結果交給 Product 一份、有鎖的 readback 服務，畫面 World 在自己的「進」拉。結果一樣晚一幀（Bevy 的註解也寫 main 下一幀才看到）。這不改使用者的決定，只是寫明 Bevy 那條路不照。

**契約**（本卡照 L17 判；系譜 Bevy 的 `SubApp::extract`、`ExtractSchedule`，第 4 節）：

1. **宣告來源**：`WorldDescription` 加 `extractFrom`（來源 World 的名字；空＝不讀別的 World）。來源必須是描述裡**排在前面**的 World；不在、排在後面、指自己是啟動錯誤（新碼 O-E8）。編排層在建這個 World 的 Registry 時、第一次 `BuildPlans` 之前呼叫 `SetExtractSource`（統合 3.1：「`SetExtractSource` 留作 finish 的輸入」）。
2. **時機**：`RunFrame` 跑到這個 World 時，**在它的「這一輪」之前**（一輪＝這個 World 在這次 `RunFrame` 裡跑的全部：平常是每幀清單；生成中是一批生成清單；生成結束的那一批是那批生成清單＋接著的每幀清單，第 13 節），照登記的順序跑它所有的 extract schedule，來源是 `extractFrom` 那個 World（這時來源已跑完這一輪：World 照描述的順序跑）。兩個 World 都不在 Run 裡。runner 自己排入口時，用一個新入口（暫名 `Product::RunExtract(world)`）在同樣的條件下跑。
3. **Kernel 那一半**（DI-KS 15 第 (3)、(4) 項之內，不是清單多一項）：一個「拿另一個 World 當唯讀來源跑 extract schedule」的原語（暫名 `WorldExecutor::RunExtract(schedule, const World& source)`）。來源用 `const` 傳，型別上就寫不回；來源在 extract 期間被寫的偵測（舊的存取驗證）照 DI-KS 13 的守衛重做。
4. **來源被替換**（`ReplaceWorld` 換掉來源）：讀它的 World 的 Registry 指著舊來源的登記表（`SetExtractSource` 存的是 `RegistryData*`），所以**讀它的 World 跟著一起重建**（照 O-8 重新登記、帶過去、準備；同一次 `ReplaceWorld` 裡做；`BuildNewFirst` 時兩個都建好才一起換）。退掉的選項：改 Kernel 讓 extract 的來源可以換（`Registry` 重新解析 id）——要改 Kernel 的登記表與 Plan 的不變量（DI-KS 3：Plan 只在 `BuildPlans` 成功時建、衍生欄位每次從登記重算；DI-KS 26：用這份 Registry 建的 World 在跑時不重建），只為了一個替換的用途；渲染 World 的內容本來就每一輪從來源重拿，重建的代價是它自己的登記與 GPU 物件之外的 Resource（GPU 物件在 Product 層，第 5.2 節）。RC-O9 的第二句（extract 為了替換要改 Kernel）照舊是觸發。
5. **渲染 World 自己的替換**：照 O-8，只換它自己。
6. **來源壞了**：Product 已經結束（O-11），不跑 Extract。

**thin start**：本批只把上面寫進編排層與 Kernel 的 Design Invariants（O-12 的 (3)、DI-KS 13 的補句）。實作（Kernel 的原語、`extractFrom`、O-E8、替換時一起重建、測試）在 SDL 那批 render 回來時做（批次表 SDL 列「render 回來時一起做 K.3-6」）；那一批不必再推這些契約。

### 5.5 時間的取樣（第 12 節第 1 題）

**現況**（world-time 第 3 節、WT-6 第 (2) 項）：照 Bevy `TimeUpdateStrategy`，`AdvanceTime` 在 World 裡依 `TimeSampling` 取樣（`TimeBase`、`ManualInstant`、`ManualDelta`）；標成「過渡」，寫明「交換點做出來後這一項移給 runner」。後果之一：多個 World 的產品同一幀各自取樣（WT-2 的「每幀只取一次樣」是每個 World）。

**時間設計的原形**（決定 1 B，使用者 11-14）：runner 每輪取樣一次，寫進 World；時間的 System 從它算 `RealTime`、`VirtualTime`。

**別家**（第 4.4 節）：Bevy、Unity 都在 World 裡取樣；Bevy 有渲染時，取樣點來自 render 的 present，main 用它（一幀一個取樣，靠 channel）；flecs 讓呼叫者給 delta（給 0 時自己量）。

**本卡的看法**：
- 「多個 World 同一個取樣」的點名使用者：Client 的模擬與畫面在兩條執行緒，各自取樣（時間設計 5.5 (三)、v7 第 04 批的設計），不需要同一個；渲染 World 的時間經 Extract 從畫面 World 拿（Bevy 的 render 也是從 main extract `Time`）。目前找不到「兩個都推進時間、在同一條執行緒、又要同一個取樣」的點名 Feature。
- 不加機制也做得到：要同一個取樣的 runner 在每次入口之前，把同一個時間點以 `ManualInstant` 寫進每個 World 的 `TimeSampling`（形 (2)；lightyear 的測試就是用 `ManualInstant` 把同一個時間發給兩個 App）。
- 所以建議 (甲)：預設照 Bevy、Unity 在 World 裡取樣，拿掉「過渡」的標記；要同一個取樣的產品用形 (2)。這會改已核准的「交換點做出來後移給 runner」，交使用者（第 12 節第 1 題）。

### 5.6 各批的需求怎麼接

| 需求 | 出處 | 接法 | 本批做到哪 |
|---|---|---|---|
| X-1 執行器：讀檔解碼在 World 執行緒外；停了之後提交是空操作；介面由 asset 自己的話定 | asset 5.6 | 執行器（介面由 asset 定）的實作是 Product 一份的物件：產品或執行緒的 Module 建、放共用處（第 5.2 節）；world_asset 在 `Configure` 經 `Find` 拿到。停了之後的行為是執行器自己的契約 | 共用處實作；執行器本身在 asset 實作批與 threads 重審 |
| X-2 交換點不認得 asset | asset 5.6 | 形 (1)：asset 自己的套用 System（第 5.1 節） | 契約（O-12） |
| X-3 若定了「每幀的邊界」，套用 System 掛在它之後 | asset 5.6 | 「進」＝每一輪最前面的 stage（預設 `Main`／`First`）；asset 的套用點預設就是它 | 契約（O-12） |
| X-4 執行緒的數量與歸屬由交換點或 runner 定 | asset 5.6 | 執行緒池是 Product 一份的物件，產品（或 threads 的 Module）建；World 的執行緒由 runner 定（第 5.3 節） | 契約 |
| X-5 測試能控制工作何時開工、完成 | asset 5.6 | 執行器是放在共用處的介面，測試放自己的實作 | 共用處實作 |
| X-6 Product 結束時，未完成的工作能停下或等完 | asset 5.6 | `OnDestroy` 回呼（World 都銷毀之後、共用處銷毀之前，第 5.2 節） | 實作 |
| O-A1 `AssetOptions` 在 world_asset 的 `Configure` 之前就在 | asset 5.4 | `ProductDescription::options`（第 5.2 節第 2 點） | 實作 |
| O-A2 依賴 world_asset 的單位拿得到 `AssetLoaderRegistry` | asset 5.4 | world_asset 在 `Configure` `Create`，別的單位在 `Configure`／`Build` `Find`（O-6 的依賴保證先後） | 實作 |
| O-A3 `Configure` 的錯誤回報 | asset 5.4 | `ProductSetup::Report` | 實作 |
| O-A4 Product 結束時的回呼 | asset 5.4 | `ProductSetup::OnDestroy` | 實作 |
| G-1～G-3 生成世界分批之間的套用點 | asset 5.13 | 第 13 節 | 和生成世界卡對齊 |
| 網路：權威值寫進 `RollbackInbox`、下一幀開頭判斷 | rollback 第 2 節 | 形 (1)：網路的收件 System 在「進」寫 `RollbackInbox`；回滾的決定與重跑在 frame schedule 的 `PreUpdate`（`RollbackOptions::decideStage` 的預設，`RollbackStrategy.hpp:177`；照 lightyear 的 `PreUpdate`）。收件在 `First`，早於決定：同一張 frame schedule 裡由 stage 的先後保證。傳輸物件在共用處 | 契約；網路 Module 批照它寫 |
| 網路：同一個行程裡伺服器 World 與 Client World | 批次表網路列；Unity Netcode、lightyear | 兩個 World 各有自己的驅動與收件、送件 System；經 Product 一份的行程內傳輸接（Unity Netcode 的 IPC 驅動）；World 之間不直接碰。lightyear 的單一 World host 是網路 Module 自己的選擇，不在本頁 | 契約 |
| SDL：OS 事件進 World | 批次表 SDL 列 | 有視窗的 runner 走形 (2)（Bevy winit），或輸入服務＋形 (1)（Bevy gilrs）；由 SDL 那批照 Bevy 選 | 契約 |
| SDL：渲染 World、Extract | runtime-composition 第 2 節 | 第 5.4 節 | 契約；實作在 SDL 那批 |
| SDL：裝置是 Product 一份 | asset 5.4「預先登記」 | 共用處 | 共用處實作 |
| threads：runner 的執行緒 | R0 附錄 | runner 建、runner 收；World 交出／交還（第 5.3 節） | 契約；threads 重審照它重寫 `RoleThread` 的用途 |
| io：多執行緒同時讀檔 | asset 5.12 | io 自己的契約；本頁只要求 World 外的服務自己同步 | 不在本頁 |
| audio：混音器、`Submitter` | 批次表 audio 列 | 混音器是 Product 一份的物件（共用處）；`Submitter` 綁執行緒、自己記（`-thread-binding-by-runner`）；World 交出到別的執行緒時，要在那條執行緒上另拿一個 `Submitter`——這是 audio 重審要對的點 | 契約；audio 重審照它 |
| 日誌：Kernel 診斷紀錄 runner 取走 | 日誌宣告 5.4 | 形 (2)，介面不變 | 契約 |
| 時間：取樣 | world-time 第 3 節、WT-6 | 第 5.5 節、第 12 節第 1 題 | 待使用者 |

---

## 6. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：第 3.2 節的 E-1～E-21。直近的是 asset 的實作（O-A1～O-A4、X-1、X-5、X-6），之後是網路、SDL、threads／io／audio 重審。
2. **現在的問題**：Product 一份的東西沒有地方放，單位之間拿不到彼此的物件（第 5.2 節）；交換點只有 11-14 的原形，和後來的分層、asset 的套用點不一致（第 5.1 節）；`Product` 的執行緒前提（CCR-5）沒有寫明放寬的路（第 5.3 節）；Extract 在 Kernel 只剩宣告那一半，編排層沒有接它（第 5.4 節）。
3. **變動的邊界**：編排層的公開介面（`ProductDescription::options`、`ProductSetup::Buffer`／`Options`／`Report`／`OnDestroy`、`WorldSetup`／`WorldFinish`／`WorldCheck::ProductBuffer`、`Product::Buffer`）；之後（不在本批實作）`WorldDescription::extractFrom`、`Product::RunExtract`、`Product::Lend`／`Return`、O-E8、`WorldLent`；Kernel 之後（SDL 那批）多一個 Extract 的執行原語。
4. **影響的 Module／Product／Tool**：編排層（實作）；asset、網路、SDL、threads、audio（照本頁寫）；world_time（第 12 節第 1 題 (乙) 時）；現有產品沒有（`gyo2` 上沒有產品）。
5. **Dependency Direction**：不變。共用處的型別由放的一方定，編排層不 include 任何 Module（O-4、RC-O4）。Module 之間經共用處互相拿物件時，依賴照舊寫在登記檔（O-6）：`Find` 別人的型別要 include 它的標頭，所以必須依賴它。
6. **Ownership**：「Product 一份的共用處、Product 層的選項、`Configure` 的回報、銷毀的回呼」歸編排層（O-2 的登記單位與生命週期、O-5 的啟動錯誤；不是職責清單多一項，第 11 節）。共用處裡的物件歸放它的單位。交換點、extract 的時機、執行緒的安排本來就在 O-4。
7. **為什麼不能更小**：只給 asset 一個專用的入口（例：`ProductSetup::AssetLoaderRegistry()`）會讓編排層認得 asset（RC-O4、RC-O8），下一個（網路、SDL）又要加一個；只做共用處、不寫交換點與執行緒的契約，網路、SDL、threads 會各自定一套，之後對不上（時間設計 5.1 的四個問題就是這樣留下的）。

---

## 7. Design Invariants 的改動（編排層 `orchestration.zh-Hant.md`、Kernel `kernel.zh-Hant.md`）

草案；最終文字在合併前核准定稿。

- **O-2 改寫（加一段）**：Product 一份的 `ModuleBuffer`（第 5.2 節第 1 點）：活到 Product 結束、比每個 World 久；不凍結；表只在擁有 Product 的執行緒上碰；物件的同步歸物件。Product 層的選項只轉交、不解讀。`OnDestroy` 回呼的順序（World 之後、共用處之前、登記的反序；`Build` 失敗也跑）。
- **O-3 改寫（加一句）**：runner 在兩次入口之間讀寫 World 是交換點的形 (2)；runner 只做屬於它自己的事（視窗、取走的診斷），別的 Module 的資料走形 (1)。
- **O-5 改寫（加一句）**：`Configure` 的回報（`ProductSetup::Report`）收進 `ProductStartupReport`。
- **O-12 新增「交換點」**：三種形與各自的規則（第 5.1 節）；Extract 的來源、時機、來源替換時讀它的 World 一起重建（第 5.4 節；實作前標「契約，實作在 SDL 那批」）。不做交換函式的登記（第 12 節第 2 題定了之後照定的寫）。
- **O-13 新增「多個 World 的執行緒」**：第 5.3 節的五條；實作前標「契約，實作在第一個點名的使用者那批」。
- **DI-KS 13 補一句**：Extract 的執行原語拿來源當 `const` 參數；來源與時機由編排層給（O-12）。
- **O-4 不改**（交換點、extract 的時機、執行緒的安排已在清單）。

### Replacement Criteria（寫在積上去之前）

- **RC-O10 改寫**：CCR-5 放寬時（建 World 要並行、一個 World 的登記單位並行 `Build`、或 World 交出到別的執行緒），每 World 與 Product 一份的 `ModuleBuffer` 需要加鎖或改成別的同步方式；或交出／交還（O-13）做不出點名的 Feature。
- **RC-O11 新增**：某個點名的 Feature 需要「World 不在 Run 裡、由編排層依序呼叫的單位函式」才做得出，三種形都做不到（第 12 節第 2 題選 (乙) 時改寫成「交換函式的登記要擴充」）。
- **RC-O12 新增**：Product 一份的共用處需要知道放進去的物件是什麼（例：要依型別做不同的收尾、要排銷毀的先後以外的順序），或兩個單位需要共同擁有一個型別。
- **RC-O13 新增**：替換 Extract 來源時連帶重建讀它的 World，成為替換時間的主要部分，或讀它的 World 有不能重建的狀態（和 RC-O9 第二句一起看）。

---

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的選項 | 決勝理由 | 需要它的 Feature／Product |
|---|---|---|---|---|
| Product 一份的 `ModuleBuffer` | 第 4 批的 `ModuleBuffer`（形狀）；Bevy App 的 main World Resource、lightyear App 上的 `ComponentRegistry`；flecs 的 world ctx（第 4 節） | (i) 給每個需要的 Module 一個專用入口（認得 Module，RC-O4）；(ii) 新型別 `ProductResources`（和 `ModuleBuffer` 同一個形狀，兩個名字一件事）；(iii) 放在第一個 World 裡（Product 層的物件和某個 World 的生命綁在一起，替換那個 World 時要搬，O-10 的限制對它不合） | 同一個形狀已經在用、已有測試；不加凍結照使用者撤回 asset 凍結的理由 | asset、網路、SDL、audio、threads（E-1～E-5、E-9、E-12、E-16） |
| Product 層的選項經描述給 | `WorldDescription::options` 的 `OptionBag`（形狀）；Bevy Plugin 的欄位（`AssetPlugin { file_path }`） | (i) 單位表讓產品自己建單位物件、帶參數（產生的單位表的形狀要改，O-6）；(ii) 產品在 `configure` 寫進共用處（產品的 `configure` 在所有單位之後，O-A1 不成立） | 不改產生的單位表；和 World 的選項同一個型別 | asset（E-2） |
| `OnDestroy` 回呼，World 之後、共用處之前、反序 | flecs `ecs_atfini`（時點同：實體刪完之後、儲存銷毀之前；先後不同：flecs 是登記順序）；C++ 解構與 `atexit` 的反序 | (i) 單位的解構子裡收尾（單位比共用處晚銷毀，它要收的物件已經不在）；(ii) 回呼回錯誤（解構裡沒有上層可交） | 解構順序對得上依賴；不需要新的狀態機 | asset（E-4、X-6） |
| 交換點＝三種形，不做交換函式的登記（建議，第 12 節第 2 題） | Bevy（套用 System、winit runner、`SubApp::extract`）、lightyear、Unity Netcode（第 4 節） | 時間設計 5.3 的交換函式登記 | 點名的使用者都落在三種形；一個概念兩條路 | asset、網路、SDL、PvP Client |
| World 整個交出／交還（契約） | Bevy pipelined 渲染；Unity `ExclusiveEntityTransaction` | `Product` 入口加鎖；World 綁執行緒 | 第 5.3 節 | PvP Client、pipelined 渲染（E-13） |
| Extract：來源排在前面、讀者的清單之前跑、來源替換時讀者一起重建（契約） | Bevy `SubApp::extract`；K.3 第 6 項 | 改 Kernel 讓來源可換 | 不動 Kernel 的 Plan 不變量 | Client、Editor 的渲染（E-11、E-14） |

既有的 Test、守衛不變，不是任何一列的決勝理由（AGENTS §4）。

---

## 9. thin start 與實作的步驟（動工前核准、使用者說開始之後）

只實作名指的 Feature 直近要用的部分：asset 實作批要的 Product 層 Resource。其餘（交換點的三種形、執行緒、Extract）只寫契約。

| 步 | 內容 | 實作者（模型／檔位） |
|---|---|---|
| 第 0 步 | 兩條守衛（已做，第 15 節） | Opus 5.5／high |
| 第 1 步 | Product 一份的 `ModuleBuffer`、`ProductDescription::options`、`ProductSetup::Report`、`OnDestroy`；測試（第 10 節） | Opus 5.5／high |
| 第 2 步 | 編排層、Kernel 的 Design Invariants 改寫（第 7 節）；第 12 節第 1、2 題定了之後照定的寫（第 1 題選 (乙) 時另加 world_time 的改動，範圍另估） | Opus 5.5／high |
| 第 3 步 | 突變、敵對式檢查、設計審查（批次表〈每批的設計審查〉）、dev_log | 見下 |

上段（公開介面、Design Invariants）。全部 high、不用 xhigh（`xhigh-only-for-investigation`）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus 5.5／high | 上段，不用小模型 |
| 新突變清單的敵對式檢查 | Fable 5.2／high | §14.2；和定清單的人不同模型 |
| 設計審查 | Fable 5.2／high | 批次表〈每批的設計審查〉 |
| 本宣告的敵對式檢查 | Fable 5.2／high | 任務卡的指定 |

估計（agent 時間）：第 1 步 6～10 h、第 2 步 3～5 h、第 3 步 4～6 h；合計 13～21 h（第 1 題選 (乙) 另加）。

---

## 10. 測試、判定（先宣告；結果出來之後不放寬）

行為測試放 `tests/common/orchestration/`（合成的單位與 World，不依賴任何 Module）：

| 案 | 內容 |
|---|---|
| P-1 | 單位 A 在 `Configure` `Create<T>`；單位 B（依賴 A）在 `Configure`、`Build`、`Finish`、`Check` `Find<T>` 拿到同一個物件；產品自己的 `configure`、`build` 也拿到；runner 經 `Product::Buffer()` 拿到 |
| P-2 | `Find` 沒人建的型別回 `nullptr`、不代建；兩個單位 `Create` 同一型別是 Programmer Error |
| P-3 | 不凍結：World 都建好之後（runner 裡）`Create` 新型別、可寫的 `Find` 都可以 |
| P-4 | 替換 World 之後，Product 一份的物件還是同一個（位址相同），每 World 那一份是新的 |
| P-5 | World Resource 裡指向 Product 層物件的指標，在每個 World 銷毀時都還有效（銷毀的順序：World → 回呼 → 共用處；用記錄解構順序的物件驗） |
| P-6 | `ProductDescription::options` 在第一個單位的 `Configure` 就讀得到；編排層不解讀 |
| P-7 | `ProductSetup::Report` 讓 `Build` 失敗、不建任何 World，錯誤在 `ProductStartupReport::unitErrors`（`world` 空） |
| P-8 | `OnDestroy`：所有 World 銷毀之後、共用處銷毀之前，登記的反序；`Build` 失敗時已登記的照跑；沒登記時什麼都不做 |

- **判定 1**：本機完整 ctest 全過；L1 三平台（linux-x64、windows-x64、macos-arm64）全過。
- **判定 2**：P-1～P-8 全過。
- **判定 3**：新突變（README 第 3 點：每個獨立可錯的決定至少一個；上限參考「改的 DI 條數 × 5」）經 Fable 5.2／high 檢查補齊後全殺；每批只跑相關突變，合併前背景完整跑一次（Kernel＋編排層＋`world_time`＋權威＋回滾＋base＋naming）。
- **判定 4**：設計審查的報告與處理寫進合併前核准頁。

本批沒有效能的判定：共用處是個位數項目的線性表（E-21），每個 World 建一次、runner 偶爾查；不在每幀的路上。

---

## 11. 本卡照 L17 判的（不是使用者的話）

- **不是職責清單多一項**：Product 一份的共用處、選項、回報、銷毀回呼屬 O-4 的「登記單位與生命週期」「啟動錯誤的彙整」（第 4 批的 `ModuleBuffer` 也是這樣判，使用者 2026-11-20 確認「不算 O-4 多一項」）；交換點、extract、執行緒本來就在 O-4。Kernel 的 Extract 原語屬 DI-KS 15 第 (3)、(4) 項。
- **名字**：沿用 `ModuleBuffer`、`OptionBag`；`ProductBuffer()` 照 `Buffer()` 的寫法；`OnDestroy` 見第 5.2 節；暫名（`Lend`、`Return`、`WorldLease`、`RunExtract`、`extractFrom`）在實作的那批照命名規範定。
- **反序**：照 C++ 解構、`atexit`；flecs 是登記順序，不照（第 5.2 節第 4 點的理由）。
- **Extract 來源替換時一起重建**：第 5.4 節第 4 點。
- **網路收件排在回滾決定之前**：stage 的先後（`First` 早於 `PreUpdate`），照 lightyear。

---

## 12. 要使用者定的

只交三類：推翻使用者之前的決定、別家分歧且真有取捨、職責清單多一項。三題都屬前兩類；職責清單沒有多一項（第 11 節）。

### 第 1 題：時間的取樣留在 World 裡，還是照原計畫移給 runner（推翻已核准的計畫）

- **背景**：時間設計決定 1 B（使用者 2026-11-14）是「runner 每輪取樣、寫進 World」；第 3 批照 Bevy 先放在 World 裡，標「過渡、交換點做出來後移給 runner」（world-time 第 3 節、WT-6 第 (2) 項，使用者 2026-11-19 核准）。
- **遊戲的例子**：Editor 有兩個都推進時間的 World 在主執行緒上，同一幀裡一個讀到 10.000 s、另一個讀到 10.001 s。(甲) 時預設就是這樣，要一致的 runner 自己發同一個時間；(乙) 時編排層保證一致。
- **(甲) 留在 World 裡（建議）**：預設照 Bevy `TimeUpdateStrategy`、Unity `UpdateWorldTimeSystem`；要一個取樣的 runner 用形 (2) 寫 `ManualInstant`。WT-6 第 (2) 項拿掉「過渡」。沒有新機制，world_time 不改程式。代價：「一次醒來只取樣一次」不是預設的保證，要 runner 做。
- **(乙) 移給 runner（原計畫）**：照 flecs 的 `ecs_progress(delta)`。runner 取樣一次，編排層把它交給每個 World（形 (2) 的固定做法，或新的入口參數）；world_time 改成讀 runner 給的值，`TimeSampling` 的三種模式搬到 runner 的取樣零件（時間設計 5.4）。代價：新介面、world_time 改程式、預設 runner 也要會取樣；而目前沒有點名的 Feature 需要它（第 5.5 節）。
- **根據**：第 4.4 節、第 5.5 節。

### 第 2 題：要不要「交換函式的登記」（推翻時間設計 5.3 的形狀；別家沒有這一層）

- **背景**：時間設計 5.3（規格 H.1）寫「登記單位登記交換函式、runner 依序呼叫」。之後 asset 照 Bevy 定成套用 System（asset X-2：交換點不需要認得 asset）。
- **遊戲的例子**：網路 Module 要在每一輪開頭把伺服器的快照寫進 `RollbackInbox`。(甲) 時它是一個掛在 `Main`／`First` 的 System，讀自己的傳輸物件（有鎖）；(乙) 時它可以改登記一個交換函式，由編排層在 World 跑之前呼叫。兩條路做同一件事。
- **(甲) 不做（建議）**：交換點＝三種形（第 5.1 節）：套用點（System）、runner 在兩次入口之間、Extract。Bevy、lightyear、Unity Netcode、flecs、Mass 都是這樣（第 4.1 節）。新增 RC-O11：哪天有點名的 Feature 三種形都做不到，再回來加。
- **(乙) 做**：編排層加每個 World 的「進」「出」兩份登記，`RunFrame` 在每個 World 的每幀清單前後依序呼叫（單位先、產品的最後），函式拿 `World&` 與第幾輪。代價：和套用 System 是同一件事的兩條路；目前點名的使用者沒有一個需要它。
- **根據**：第 4.1 節、第 5.1 節。

### 第 3 題：Product 層 Resource 要不要、用什麼形（使用者開的題，`v8-arch-gyo2-product-level-resource-open`；別家分歧）

- **背景**：asset 的 O-A1～O-A4（第 5.6 節）；網路的行程內傳輸、audio 的混音器、執行緒池、SDL 的裝置也都是 Product 一份。**前提的更正**：這一題原本寫「像 Bevy 的 App 層 Resource」。偵察查到 Bevy 的 `App` 在 World 外沒有 Resource（`bevy_app/src/app.rs:85-96`），那是 main World 的 Resource；Bevy 能這樣做是因為它有主 World、而且 Plugin `build` 時 World 已經在。GYO 的 World 彼此平等、`Configure` 時還沒有 World（第 4.3 節）。
- **遊戲的例子**：world_asset 在 `Configure` 建 asset store；image Module（之後的 SDL 那批）要在 `Configure` 拿到它來登記貼圖的 loader；產品要在描述裡給內容根目錄；測試要換成自己控制的執行器。
- **(甲) Product 一份的 `ModuleBuffer`（建議）**：第 5.2 節。放的可以是單位、產品、runner、測試；共用的是「服務物件」，單位的類別不必公開。形狀已在用（第 4 批）。
- **(乙) 單位互相找**：`ProductSetup::Unit<T>()`，拿到別的登記單位的實例、呼叫它的公開方法。系譜：Unreal 的 subsystem（以類別為鍵、有依賴的先後）〔知〕、Bevy 的 `get_added_plugins::<T>()`（唯讀、讀設定）。代價：每個單位的類別變成公開介面；產品、runner、測試放的物件（X-5 的執行器）沒有地方放，還是要 (甲) 或描述的選項。
- **(丙) 照 Bevy 放在某個 World**：第一個 World 或每個 World 一份副本。`Configure` 時沒有 World，O-A1、O-A2 做不到；要改生命週期（O-2），而且第一個 World 被替換時要搬。不建議。
- 三個選項都配 `ProductDescription::options`（O-A1）、`ProductSetup::Report`（O-A3）、`OnDestroy`（O-A4）；(乙) 的 `OnDestroy` 也可以改成單位的解構子。
- **根據**：第 4.3 節、第 5.2 節。

---

## 13. 和生成世界（重做）的交界

生成世界的宣告由另一張卡寫（`gyo2-world-gen`，`docs/architecture/target/v2/world-gen-declaration.zh-Hant.md`，分支 `claude/gyo2-world-gen-77331d`，修訂 0 c8b5faf4，第 5.1～5.5、5.14 節）。2026-11-22 兩張卡用 SendMessage 對齊，結果如下（**已對齊**；兩份宣告各自核准時以這張表為準）：

| # | 事項 | 誰負責 | 對齊的內容 |
|---|---|---|---|
| W-1 | 生成的形 | 生成世界 | 每個 World 有生成清單與每幀清單；生成中的 World 在一次 `RunFrame` 裡跑一批生成清單、不跑每幀清單；哪一批跑完沒人 `Hold()` 就結束，同一次 `RunFrame` 接著跑每幀清單（照 Bevy 第一次 update 先跑 startup）。一批＝一次 `RunFrame`，批與批之間 runner 回到迴圈 |
| W-2 | 「一輪」的定義 | 本頁（生成世界提案、本頁同意） | 一輪＝這個 World 在這次 `RunFrame` 裡跑的全部：平常＝每幀清單；生成中、這批有人 `Hold()`＝這一批生成清單；生成結束的那一批＝那批生成清單＋接著的每幀清單。Extract 在讀者的一輪之前（第 5.4 節）；第 12 節第 2 題若選 (乙)，交換函式的「進」「出」包住一輪，結束那一批也只各一次 |
| W-3 | 套用點（形 (1)） | 擁有服務的 Module | 把 World 外的結果帶進 World 的 System（asset 的套用、網路的收件），預設骨架下同時掛 `Main`／`First` 與 `Generate`／`First`；骨架關掉時作者自己掛（同 O-4）。asset G-1 由此成立 |
| W-4 | `beforeSwap` | 生成世界（保留）；本頁寫明 | `beforeSwap` 是換上之前同步跑一次的關卡，不是生成；期間沒有套用點、沒有交換點（asset G-3），不加機制 |
| W-5 | 別的 World 知道某個 World 在生成 | 本頁 | 不加新機制：runner 在兩次入口之間讀 `Product::Generating(world)` 寫進別的 World（形 (2)），或 Module 的服務＋套用點（形 (1)）。生成世界提供 `Product::Generating`、`GenerationPasses` 與 World 內的 `WorldGeneration` |
| W-6 | 產品給 Module 的設定 | 本頁（Product 層）；生成世界（配發器） | 預設實體配發器的範圍每個 World 可能不同，用 `WorldDescription::options`（`EntityAllotterOptions`），不用 Product 層 Resource；Product 層 Resource 的形狀歸本頁 |
| W-7 | 執行緒 | 本頁 | 生成的狀態在 Product 的 World 槽、照 CCR-5 在呼叫的執行緒上讀寫；World 交出到別的執行緒時，生成的狀態跟著交出，那幾批由持有的執行緒經交出的入口推動（第 5.3 節） |
| W-8 | 結束條件 | 生成世界 | 由作者的 System 給（asset G-2），不經本頁 |
| W-9 | 遊戲初始化 | 生成世界 | 照舊先跑，跑在第一個 World 的生成之前 |

## 14. 範圍的限界

- 交出／交還、Extract 只寫契約；它們的實作批可能發現契約不夠（那時照 §14 冒頭停下、回到本頁改）。
- 共用處不加凍結：「邊讀邊寫」的風險在物件自己身上（第 4 批使用者擔心的是建 World 期間的登記表，那一份照舊凍結）。誤用（把某個 World 的參照放進共用處、讓它活過那個 World）不加守衛，交設計審查（`no-guards-against-misuse`）。
- `OnDestroy` 只在 Product 銷毀時跑；行程被殺、`std::terminate` 時不跑。

---

## 15. 第 0 步：第 5 批開頭的兩條守衛

任務卡排在宣告之前先做、可以獨立成一個 PR（分支 `claude/gyo2-exchange-guards`）。內容、取捨、敵對式檢查（Fable 5.2／high：必修 1、提案 3、輕微 5，全部處理）與驗證寫在 dev_log [`2026_11_22_gyo2_exchange_guards.zh-Hant.md`](../../../dev_logs/2026_11_22_gyo2_exchange_guards.zh-Hant.md)。摘要：

- Kernel「拿掉的名字不回來」：`kernel_guard.py` K9 加 `K9_REMOVED`（判定 4 的樣式改寫成識別字的形狀；舊的啟動錯誤碼不列，`StartupCode` 的比對已守）。突變 `ks-k9-removed-dropped`。
- 權威 Module 的連結集：`gyo_expect_links(gyo_authority …)`（DI-A 9）。突變 `au-links-more`；不加 DEFER 的突變。
- 守衛欄：DI-KS 15、DI-A 2、DI-A 9。只收緊；DI 的守衛照 §14.4 要使用者核准（dev_log 的表）。
- 和命名收尾卡（`gyo2-naming-cleanup`）碰到的檔已通知對方；誰後合併誰 merge 跟上。

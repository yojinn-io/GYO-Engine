# 事前宣告：多個 World、交換點、Product 層 Resource（`v8-arch-gyo2-exchange`）

狀態：修訂 1（修訂 0 的敵對式檢查已處理，第 16 節）。動工前核准之前不實作。

性質：實作段第 5 批「多個 World、交換點」的事前宣告（AGENTS §2 基盤 Architecture、§3 Architecture Delta、§4 系譜）。連同編排層缺的「Product 層 Resource」一起推（`v8-arch-gyo2-product-level-resource-open`、`v8-arch-gyo2-batch5-start`）。

用詞：「設計段／實作段」「動工前核准／合併前核准」「第 N 步」照第 3 批。標「**本卡照 L17 判**」的是這張卡的判斷（名字照命名規範、細節照別家原始碼），**不是使用者的話**。本卡新加的詞第一次出現時標「本卡加的詞」。

經過：修訂 0（a12b9fc2）送 Fable 5.2／high 敵對式檢查：必修 6、提案 9、輕微 5（報告：主 checkout `build/evidence/gyo2-exchange/declaration-r0-adversarial-check.md`）。修訂 1 全部處理，對照在第 16 節。最大的兩處：交換函式的登記從「建議不做」改成**照已核准的時間設計 5.3 做**（M2：有點名的使用者）；時間的取樣從「交使用者」改成**維持過渡、到 SDL 那批定**（S3）。所以交使用者的題目只剩一題（第 12 節）。

---

## 0. 根據與讀的版本

**程式**：`gyo2` 45f13757（#266 之後）。講現況的句子是讀程式寫的，行號照這個版本。

**別家原始碼**（`~/Code/GitHome`，本機）：偵察報告在主 checkout `build/evidence/gyo2-exchange/`：
- `survey-bevy-lightyear.md`：Bevy、lightyear、bevy_replicon（Opus 5.5／high）。
- `survey-unity-flecs-mass.md`：Unity Entities、Unity Netcode、flecs、MassSample（Opus 5.5／high）。

版本與引用見第 4 節。第 4 節每一列標明〔原〕（讀了原始碼）或〔知〕（一般知識）。

**文件**：

| 文件 | 狀態 | 本頁怎麼用 |
|---|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md)第 5、6 組各列（文件分支 `claude/gyo2-time-design`） | 主會話 | 誰等這一批、等什麼（第 3.2 節） |
| [統合](kernel-app-boundary-declaration.zh-Hant.md)修訂 38：L19（交換函式的登記改在編排層）、第 3 節、9.2 的 O-3、O-4 | 已核准 | 交換點、extract 的時機、執行緒的安排歸編排層 |
| [編排層](orchestration.zh-Hant.md) O-1～O-11、RC-O1～RC-O10 | 已合併 | 本批改 O-2、O-3、O-5、O-7、O-8，加 O-12、O-13（第 7 節） |
| [時間設計](rebuild-time-design.zh-Hant.md)第 5.1～5.5、6.4 節 | 已核准（決定 1～7） | 交換點的原形（5.3：進、出、交換函式的登記、七個點名的使用者）、三種組合、跨 World 事件的形狀 |
| [world-time](world-time.zh-Hant.md) 第 3 節、WT-6 | 已合併 | 取樣的過渡（第 5.5 節） |
| [rebuild-concept](rebuild-concept.zh-Hant.md) M11、D6b（含 2026-11-22 的加註） | 已核准 | 交換點的不變量 |
| [runtime-composition](../runtime-composition.zh-Hant.md) 第 2 節（Client 三個 World、Mailbox 由 Host 擁有、只在 schedule 之間存取）、第 3b 節（Editor；沒有影像回到文件 World） | 使用者 11-11 決定 | 第 5.1、5.4 節 |
| [kernel](../kernel.zh-Hant.md) DI-KS 13、15、26；[cross-component-rules](../../../cross-component-rules.md) CCR-5 | 已合併 | Extract 的 Kernel 那一半；單執行緒的前提 |
| [回滾](rollback.zh-Hant.md)（`RollbackInbox`、R-3）、[權威](authority.zh-Hant.md)（DI-A 7） | 已合併（#259） | 網路寫進 World 的位置 |
| [日誌宣告](logging-declaration.zh-Hant.md) 5.4 | 已合併（#256） | Kernel 診斷紀錄「runner 在兩次 Run 之間取走」，「交換點在第 5 條才由編排層排；那時這個呼叫可以移到交換點，介面不變」（第 228 行） |
| asset 宣告（分支 `claude/gyo2-asset-585196`，55a14825 動工前核准，1b120a86 紀錄） | **已核准、未合併** | X-1～X-6（5.6）、O-A1～O-A4（5.4）、G-1～G-3（5.13）、第 22 節 |
| 生成世界的宣告（分支 `claude/gyo2-world-gen-77331d`，修訂 0 c8b5faf4） | 寫作中 | 交界（第 13 節） |

任務卡寫「asset 5.5 的 X-1～X-8」「G-1～G-4」：核准版（55a14825）的交換點需求是 5.6 的 **X-1～X-6**（X-7、X-8 是修訂 0、1 的編號，已併入），生成世界的是 5.13 的 **G-1～G-3**。本頁照核准版。

---

## 1. 這一批做什麼（先用遊戲講）

**PvP 的 Client**：三個 World——模擬（固定步，自己的執行緒）、畫面（每幀，主執行緒）、渲染（每幀，主執行緒，只經 Extract 唯讀拿畫面 World 的資料）。網路收到伺服器的快照，模擬 World 每一輪開頭把它們寫進 `RollbackInbox`；畫面 World 把玩家的指令交給模擬 World，模擬 World 把「這一步角色在哪」交回畫面 World（經 Host 擁有的郵箱，只在 schedule 之間存取，使用者 11-11）。貼圖的解碼在 IO 執行緒。每個 World 跑完一輪，Kernel 的診斷紀錄（丟掉的訊息、被拒的生成）要被取走交給日誌。

這裡有四種「資料進出 World」：
1. **外部服務的結果在幀裡進 World**（asset 的載入結果、網路的快照）：服務在 World 之外、有自己的鎖；World 裡的 System 在每一輪的固定位置一次拉進來。
2. **在兩次 Run 之間、拿整個 World 做的事**（郵箱的進出、診斷紀錄的取走）：World 不在 Run 裡時，由編排層依序呼叫各單位登記的函式。
3. **runner 自己的事**（它擁有的視窗的事件）：runner 在兩次入口之間直接寫。
4. **渲染讀畫面**：渲染 World 在一個時點唯讀地看畫面 World 的資料（Extract），不寫回。

還有一個缺口：asset、網路的傳輸、audio 的混音器、執行緒池都是「整個 Product 一份」的東西，一個 Module 建、別的 Module 要拿得到。今天編排層只有每個 World 一份的地方（Resource、選項包、`ModuleBuffer`），沒有 Product 一份的。

**單機的 2D 遊戲**：一個 World、一條執行緒。上面這些都不需要時，作者什麼都不用寫、也不付費（沒有登記交換函式就不呼叫；共用處空著）。

---

## 2. 範圍

### 2.1 這一批做的（動工前核准、使用者說開始之後）

- **第 0 步**（已做，見第 15 節）：第 4 批宣告 9.5 延下來的兩條守衛（PR #268）。
- **Product 層 Resource**（第 5.2 節）：Product 一份、以型別為鍵的共用處；產品給 Module 的 Product 層選項；`Configure` 的錯誤回報；Product 銷毀時的回呼；明確的銷毀順序。**實作**（asset 是直近的使用者）。
- **交換函式的登記**（第 5.1 節的形 (2)）：每個 World 的「進」「出」兩份登記，`RunFrame` 包住每個 World 的一輪呼叫。**實作**（點名的使用者：Kernel 診斷紀錄的取走，日誌宣告 5.4 等它）。
- **交換點的契約**（第 5.1 節）：四種形、各自的時點與規則，寫進編排層的 Design Invariants。
- **多個 World 的執行緒**（第 5.3 節）：只寫不變量（整個 World 交出、交出期間別的執行緒不碰）；入口層級的行為留給實作那一批。**不實作**。
- **Extract**（第 5.4 節）：只寫時機與「來源替換後讀者不得讀舊來源」；執行的原語與編排層的呼叫在 SDL 那批實作。
- 編排層、Kernel 的 Design Invariants 與 Replacement Criteria 的改寫，相關文件的加註，跨元件規則的登記（第 7 節）。

### 2.2 不在這一批

- 網路 Module 本身、SDL 的 runner、asset 的實作、threads／io／audio 的重審：各自的批，照第 5.6 節的對照接上。
- 跨 World 的事件（時間設計 6.4，決定 7）：照舊延後，形狀落在形 (2)（第 5.1 節末）。
- 時間的取樣移給 runner（時間設計 5.4、world-time 第 3 節的過渡）：維持過渡，到 SDL 那批有了有視窗的 runner 再定（第 5.5 節）。
- 生成世界：另一張卡（`gyo2-world-gen`）；交界在第 13 節。

---

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

- Product 層 Resource 是之後每個「整個 Product 一份」的 Module（asset store、網路的傳輸、audio 的混音器、執行緒池、SDL 的裝置）接進來的地方；形狀一改，它們全要改。
- 交換點的規則決定每個接外部資料的 Module 怎麼寫（在哪個時點、能不能上鎖、能不能等別的執行緒）；交換函式的登記是每個要在兩次 Run 之間碰 World 的 Module 的插口。
- 多個 World 的執行緒不變量決定 runner、Product 的入口能不能被兩條執行緒碰。PvP Client 積在上面。
- Extract 的時機決定渲染 World 與它的來源怎麼一起建、一起替換。

### 3.2 予見的根據 → 要求（Feature → Contract → Architecture）

根據的種類照 AGENTS §2：1＝名指的 Feature；2＝參考實作；3＝規模與預算。根據 2 單獨的要求不收。

| # | 根據（種類） | 從誰來 | 要求（Contract） | 本頁 |
|---|---|---|---|---|
| E-1 | asset：擁有型別的 Module 拿到 Product 的 `AssetLoaderRegistry`（1） | asset O-A2、5.4、5.16 | Product 一份、一個單位建、別的單位找得到的共用處 | 5.2 |
| E-2 | asset：產品給 `AssetOptions`，在 world_asset 的 `Configure` 之前就在（1） | asset O-A1 | 產品在描述裡給 Product 層的選項，單位的 `Configure` 讀得到 | 5.2 |
| E-3 | asset：`Configure` 階段的錯誤回報（1） | asset O-A3 | `ProductSetup` 能回報錯誤，收進 `ProductStartupReport` | 5.2 |
| E-4 | asset：Product 結束時記丟棄件數、停下工作（1） | asset O-A4、X-6 | Product 銷毀時，依序呼叫單位登記的回呼；所有回呼跑的時候，共用處的物件都還在 | 5.2 |
| E-5 | asset：讀檔與解碼在 World 執行緒外跑；執行器由這裡給；執行緒的數量與歸屬由 runner／產品定（1） | asset X-1、X-4 | 產品或測試給的物件（執行器的實作）在單位的 `Configure` 就拿得到；單位建的物件放共用處 | 5.2 |
| E-6 | asset：交換點不認得 asset；World 靠自己的套用點看到結果（1） | asset X-2、X-3 | 外部服務的結果在幀裡進 World 的走套用點（形 (1)），編排層不認得內容 | 5.1 |
| E-7 | asset：測試能控制工作何時開工、完成（1） | asset X-5 | 測試能給自己的執行器（介面型別的物件） | 5.2 |
| E-8 | 網路：伺服器的權威值寫進 `RollbackInbox`，下一幀開頭判斷（1） | rollback.zh-Hant.md 第 2 節、DI-A 7 | 收件在模擬 World 每一輪的回滾決定之前寫進 World；送件在每一輪最後 | 5.1 |
| E-10 | SDL：OS 事件在每一輪開頭進 World；有視窗的 runner（1） | 批次表 SDL 列；時間設計 5.5 (二) | runner 在兩次入口之間可以寫 World（形 (3)）；或輸入服務＋套用點 | 5.1 |
| E-11 | SDL：渲染 World 有自己的 Registry，只經 Extract 唯讀拿資料，無反饋（1） | runtime-composition 第 2、3b 節（K.3 第 6 項）、DI-KS 13、批次表 SDL 列 | Extract 的時機、來源的設定與替換、Kernel 的執行原語 | 5.4 |
| E-12 | SDL：render 的裝置是 Product 一份；貼圖的 loader 要等裝置建好（1） | asset 5.4「預先登記」 | 裝置放共用處 | 5.2 |
| E-13 | PvP Client：模擬 World 在自己的執行緒，主執行緒停 250 ms 時模擬照跑（1） | 時間設計 5.5 (三)；runtime-composition 第 2 節 | 一個 Product 的 World 可以在不同執行緒跑；World 之間的交換不等另一條執行緒 | 5.3、5.1 |
| E-14 | Editor：文件 World＋渲染 World（1） | runtime-composition 第 3b 節 | 同 E-11 | 5.4 |
| E-15 | threads 重審：`RoleThread` 的②③是 runner 的執行緒（1） | 批次表 threads 列、R0 附錄 | 執行緒由 runner 建、由 runner 收；World 不綁執行緒 | 5.3 |
| E-16 | audio 重審：`Submitter` 歸第一個呼叫它的執行緒；混音器有自己的迴圈（1） | 批次表 audio 列 | 混音器放共用處；綁執行緒的物件自己記執行緒（`v8-arch-gyo2-thread-binding-by-runner`） | 5.2、5.3 |
| E-17 | io 重審：多執行緒同時讀檔的契約（1） | asset 5.12 | 不在本頁定；本頁只要求「World 外的服務自己負責自己的同步」 | 5.1 |
| E-18 | 時間：「一次醒來只取樣一次」、取樣歸 runner（時間設計決定 1 B；WT-6 第 (2) 項的過渡）（1） | 時間設計 5.2、5.4；world-time 第 3 節 | 維持過渡，到 SDL 那批定（第 5.5 節）；交換函式的登記讓那時有地方接 | 5.5 |
| E-19 | 日誌：Kernel 的診斷紀錄在每個 World 跑完之後取走；World 不在 Run 裡、做不成 System（1） | 日誌宣告 5.4、第 228 行 | 編排層在每個 World 的一輪之後依序呼叫單位登記的函式（形 (2)「出」） | 5.1 |
| E-20 | 生成世界：分批之間 asset 的套用 System 會跑（1） | asset G-1～G-3 | 套用點是 World 裡的 System，跟著 World 的每一輪走 | 5.1、13 |
| E-21 | 規模：PvP 64 Hz、Client 三個 World；Match 一個 World；工具一到兩個 World（3） | 時間設計、runtime-composition | World 數是個位數；Product 層的查找不必比線性快（照 `OptionBag`、`ModuleBuffer` 的線性表） | 5.2 |
| E-22 | PvP Client：意圖 F → S、呈現 S → F 經 Host 擁有的郵箱，**只在 schedule 之間存取**（1；使用者 11-11） | runtime-composition 第 2 節 | 兩次 Run 之間、拿整個 World 的插口（形 (2)） | 5.1、5.3 |
| E-23 | 時間設計 5.3 點名的七個使用者：時間的投影、`HostClockSample`（PvP）、網路的收發緩衝、平台事件、資產 IO 的完成通知、World 之間的資料、診斷的輸出（1） | 時間設計 5.3（第 328 行） | 逐一落到四種形（第 5.1 節的表） | 5.1 |

修訂 0 的 E-9（同一個行程裡的伺服器 World 與 Client World）只有根據 2（Unity Netcode、lightyear 有 host 模式），批次表網路列沒有點名，修訂 1 拿掉（檢查 S4）；網路 Module 若點名，照第 5.6 節的對照接。

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

**共通**：World 的資料不上鎖、不共享；一條執行緒在一個時點擁有一個 World。外部的生產者寫進自己擁有的緩衝（channel、stage、驅動的佇列），World 在固定的頭（收、`OnLoad`、`SyncWorldToMass`）尾（送、`OnStore`、`UpdateWorldFromMass`）一次套用。**沒有一家有「編排層依序呼叫的交換函式登記」**；最接近的是 flecs 的 `ecs_run_post_frame`（一次性的動作佇列）與 replicon 測試在兩次 update 之間搬緩衝。GYO 照時間設計 5.3 留這一層，理由是 GYO 自己的兩個決定（第 5.1 節）：Kernel 的診斷紀錄要在 Run 之外取（日誌宣告 5.4）、Client 的郵箱只在 schedule 之間存取（使用者 11-11）。

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

**共通與分歧**：別家的共用物件都放在「某個 World」或「行程全域」；**沒有一家有獨立於 World 的、Product 一份的型別化存放處**，最接近的是 Unreal 的 GameInstance subsystem〔知〕。Bevy 能用 main World 當這個地方，是因為它有一個「主」World、而且 Plugin 的 `build` 時 World 已經在；GYO 的 World 彼此平等（沒有主 World），而且 `Configure` 時還沒有任何 World（O-2），所以 Bevy 的做法搬不過來。這是第 12 節第 1 題。

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

**已核准的原形**：時間設計 5.3（規格 H.1）——每個 World 每一輪有「進」（跑之前）與「出」（跑完之後）兩個交換點；登記單位登記交換函式，依登記的順序呼叫；交換函式不等待別的執行緒、只用短的鎖。統合 L19 把它的位置定在編排層；runtime-composition 第 2 節（使用者 11-11）要 Client 的郵箱「只在 schedule 之間存取」。之後 asset 照 Bevy 定了套用點（asset X-2：交換點不需要認得 asset）。

**四種形**（本卡照 L17 整理；系譜見第 4 節）：

| 形 | 什麼 | 時點 | 誰寫 | 用它的（點名的） | 系譜 |
|---|---|---|---|---|---|
| **(1) 套用點** | World 裡的 System 呼叫 World 外的服務（有自己的鎖），把結果一次寫進 World；或把要送的交給服務 | 「進」：一輪最前面（預設骨架 `Main`／`First`；生成中是 `Generate`／`First`，第 13 節）；「出」：最後（`Main`／`Last`）。骨架關掉的 World 由產品指定 | 擁有服務的 Module | asset 的套用 System（E-6、E-20）；網路的收件與送件（E-8，網路 Module 批照下面的規則選 (1) 或 (2)） | Bevy asset server、gilrs；lightyear、bevy_replicon；Unity Netcode 的 receive group；flecs 的 `OnLoad`／`OnStore`；Mass 的 `SyncWorldToMass`（第 4.1 節） |
| **(2) 交換函式** | 單位登記的函式，World 不在 Run 裡、Ready 時拿整個 World 做事 | 編排層在每個 World 的**一輪**之前（進）、之後（出）依登記的順序呼叫（一輪的定義見第 13 節 W-2） | 登記它的單位（或產品自己的 `build`） | Kernel 診斷紀錄的取走（E-19）；Client 的郵箱（E-22）；之後：時間的投影（E-18，若 SDL 那批移給 runner）、`HostClockSample`（PvP） | 時間設計 5.3（規格 H.1）；別家沒有這一層，最近的是 flecs 的 `ecs_run_post_frame`（一次性的佇列）與 replicon 測試在兩次 update 之間搬緩衝（第 4.1 節） |
| **(3) runner 在兩次入口之間** | runner（或 Product 的持有者）在 World 不在 Run 裡時直接讀寫 World，或呼叫某個 Module 給的函式 | 兩次 `Product` 入口之間 | runner／產品 | runner 擁有的視窗的事件（E-10，若不走服務）；World 壞掉之後先寫失敗、再取診斷（日誌宣告 5.4 的順序規則） | Bevy winit 的 runner 在 update 之前寫視窗事件（第 4.1 節） |
| **(4) Extract** | 一個 World 唯讀地讀另一個 World | 來源跑完、讀者的一輪之前，兩個都停著 | 讀者的 extract schedule | 渲染 World（E-11、E-14） | Bevy `SubApp::extract`（第 4.2 節） |

時間設計 5.3 點名的七個使用者（E-23）：時間的投影 → (2)（若移給 runner，第 5.5 節）；`HostClockSample` → (2)（PvP 移植時）；網路的收發緩衝 → (1) 或 (2)；平台事件 → (3) 或 (1)；資產 IO 的完成通知 → (1)；World 之間的資料 → (2)；診斷的輸出 → (2)。

**選哪一種的規則**（寫進 O-12）：
- **(1) 是 Module 擁有的外部服務在幀裡進出 World 的預設**：結果要和 World 裡的 System 排先後（例：網路的收件要在回滾的決定之前）、要跟著生成的每一批走、或由很多 System 讀的，用 (1)。
- **(2) 給「必須在 schedule 之間」或「要整個 World、在 Run 之外」的事**：使用者定了只在 schedule 之間存取的（郵箱，E-22）；Run 裡看不到全部結果的（診斷紀錄要在最後一個同步點之後取）；骨架關掉時也要自動接上、不靠作者掛的。
- **(3) 只給 runner 自己擁有的東西**；Module 的資料不要求 runner 認得（RC-O1「Host 長回來」）。runner 也可以呼叫 Module 給的函式（例：`WorldLogging::TakeKernelDiagnostics`），接線歸 runner／產品。
- 同一件事不要兩種形都做：網路 Module、SDL 那批選定一種，寫在自己的宣告。

**每一種的規則**：
- (1)：World 不上鎖；服務自己同步。套用 System 只在自己的時點把結果寫進 World（asset 的 AS-4、AS-11 是先例）。服務帶回的值照 D6b 的加註。**不等另一條執行緒**：拿服務的鎖只做表的操作，不等工作完成（時間設計 5.5 (三) 的條件）。**和別家的差別**：別家的套用多在 `PreUpdate`、`First` 放時間（Bevy）；GYO 的回滾決定在 `PreUpdate`（`RollbackOptions::decideStage` 的預設，`RollbackStrategy.hpp:177`），所以要在決定之前看到的收件掛 `First`，和 `AdvanceTime` 同一個 stage。同一個 stage 裡各 Module 的套用 System 之間沒有預設的先後，要先後的自己宣告（Kernel 的順序宣告）。
- (2)：見下面「交換函式的登記」。
- (3)：只在 World 不在 Run 裡時；寫的是一般的 World 寫入（不跑 hook 的那條路，DI-KS 18）。runner 只呼叫 `Product` 的執行入口與 Module 給的函式，不排 System 的先後（O-3 不變）。
- (4)：第 5.4 節。

**交換函式的登記**（形 (2)；本卡照 L17 定細節，形狀照時間設計 5.3）：
1. **登記**：`WorldSetup::ExchangeIn(fn)`、`WorldSetup::ExchangeOut(fn)`（名字照同組的 `Prepare(fn)`），在 `Build` 時登記（單位依序、產品自己的 `build` 最後）；替換 World 時隨 `Build` 重新登記（和準備函式同一個生命）。函式的型別 `Result<void, OrchestrationError>(Kernel::World&)`，要用 Product 層物件的在 `Build` 時捕進去（第 5.2 節）。
2. **呼叫**：`RunFrame` 對每個 World：進 → 一輪 → 出（一輪的定義：第 13 節 W-2；生成結束的那一批也只各一次）。`RunGameInit`、`RunSchedule` 不呼叫（遊戲初始化不是一輪；`RunSchedule` 是 runner 自己排的原語，runner 要時用形 (3)）。沒有登記就不呼叫。
3. **壞掉的 World**：O-11 照舊——有 World 壞了，`RunFrame` 什麼都不跑，所以也不呼叫交換函式。壞掉之後要取診斷，走形 (3)（日誌宣告 5.4：先寫失敗、再取）。
4. **失敗**：函式回錯誤 → `RunFrame` 停下、回 E2（新碼 `ExchangeFailed`，帶函式自己的碼，同準備函式的 `PrepareFailed` 寫法）；World 沒有在 Run 裡，**不算壞**、Product 不結束。前面的 World 已經跑了這一輪（和一張 schedule 在中途失敗時一樣，不留回滾）；接下來怎麼辦由 runner 決定（錯誤只告訴外面發生了什麼）。函式丟例外：原樣往外傳，World 不標成壞（不在 Run 裡）。
5. **不等另一條執行緒**、只用短的鎖（時間設計 5.3）；不加守衛，交設計審查。
6. **和 (1) 是兩個時點**：(2) 在 World 的 Run 之外、包住整個一輪；(1) 在 Run 裡、某個 stage。不是同一件事的兩條路（選哪一種照上面的規則）。

**跨 World 的事件**（時間設計 6.4，決定 7）：照舊只定形狀、不實作。形狀落在 (2)：來源 World 的「出」讀自己的訊息（自己的游標）、交給郵箱，目的 World 的「進」寫進自己的訊息；每筆帶來源的 tick 與取樣時間由用它的 Module 定；郵箱有上限、丟了要記（時間設計 6.4 的兩個洞照那裡的改法）。

### 5.2 Product 層 Resource

**要解的事**（E-1～E-5、E-7、E-12、E-16）：「整個 Product 一份」的物件，一個登記單位建，別的登記單位、產品自己的程式、runner 拿得到；產品與測試要在單位的 `Configure` 之前把設定與物件給它們；它們在 Product 結束時要收尾。

**現況**：每個 World 一份的有三種（World 的 Resource；`WorldDescription::options` 的 `OptionBag`；每次建 World 一份的 `ModuleBuffer`）；Product 一份的沒有。登記單位的實例活到 Product 結束、可以自己持有資料，但別人拿不到它：用產生的單位表的產品手上沒有單位物件，單位之間也不互相交出（第 4 批第 6 節查到同一個缺口）。順序：單位的 `Configure` 全部先跑、產品自己的 `configure` 最後（`Product.cpp:177-188`）。

**建議的做法**（要不要有這一層、用什麼形，是第 12 節第 1 題；形狀照 `ModuleBuffer`、不加凍結是任務卡的指定）：

1. **Product 一份的 `ModuleBuffer`**（放「單位建的物件」）：沿用 `ModuleBuffer` 型別，`Product` 多持有一份，活到 Product 結束。每種型別只由擁有它的一方 `Create<T>`（第二次 `Create` 同一型別是斷言），別人 `Find<T>`（找不到回 `nullptr`、不代建）。**不凍結**：不呼叫 `Freeze`。拿得到它的地方：
   - `ProductSetup::Buffer()`（`Configure`）；
   - `WorldSetup`、`WorldFinish`、`WorldCheck` 的 `ProductBuffer()`，**都給可寫的**（每 World 那一份在 `Finish`／`Check` 給 `const` 是凍結的結果；Product 這一份不凍結，給 `const` 等於半個凍結，檢查 N1）；
   - `Product::Buffer()`（runner、Product 的持有者，在兩次入口之間）；
   - 交換函式、準備函式在 `Build` 時捕參照。System 要用的，由擁有的 Module 在準備函式裡放一個指向它的 World Resource（asset 5.4 已是這個寫法）。
   - 「World 都建好之後還能 `Create`」是不凍結的結果，沒有點名的使用者，不寫成需求（檢查 N5）。
   - 型別的註解寫明兩種用法（檢查 N4）：每次建 World 一份（`Build` 後凍結、建完即丟、只有單位寫）；Product 一份（不凍結、活到 Product 結束、單位與 runner 寫、物件可能被別的執行緒用）。兩者共用的只有「一方建、別人找」這條規則，所以同一個型別；`frozen_` 在 Product 那一份永遠是 false。
2. **Product 層的選項**（放「產品或測試給的東西」）：`ProductDescription` 加 `options`（`OptionBag`，和 `WorldDescription::options` 同一個型別）。產品在描述裡放，單位在 `Configure` 起經 `ProductSetup::Options<T>()` 讀；編排層只轉交、不解讀。**通則**（檢查 M1）：產品、測試給的物件，單位要在 `Configure` 就拿到的，一律走這裡——產品自己的 `configure` 在所有單位之後，寫進共用處的單位在 `Configure` 看不到。物件放成持有者的小結構，例：`struct AssetExecutorOption { std::shared_ptr<AssetExecutor> executor; };`（`OptionBag::Get` 回 `const` 的持有者，指向的執行器可以照常呼叫）。這也是介面型別的放法（`ModuleBuffer::Create<T>` 是 `make_shared<T>`，以具體型別為鍵，`Setup.hpp:150-153`；單位要放介面時同樣用持有者結構 `Create<Holder>(shared_ptr<I>)`）。這是 Bevy `AssetPlugin { file_path, … }` 這類「Plugin 的欄位」的對應：GYO 用產生的單位表時產品手上沒有單位物件，所以經描述給（E-2）。
3. **`Configure` 的錯誤回報**：`ProductSetup::Report(code, message)`，形狀同 `WorldFinish::Report`；收進 `ProductStartupReport` 的 `unitErrors`（`world` 欄空；`unit` 是單位的登記列名，產品自己的 `configure` 是產品名，和 `ProductSetup` 的 installer 同一個值）。**有任何 `Configure` 回報，就不跑任何 World 的 `Build`～`Check`**，直接回報告（回報的單位可能沒建它該建的物件，依賴它的單位再跑只會在 `Find` 斷言；其餘單位的 `Configure` 照跑完、錯誤一起收齊）。
4. **Product 銷毀時的回呼**：`ProductSetup::OnDestroy(std::function<void()>)`（名字本卡照 L17：同組的 `Prepare(fn)`、`InstallRunner` 用動詞）。
   - **為什麼不靠物件自己的解構子**（檢查 S1）：回呼是兩段式的——所有回呼跑的時候，共用處裡**所有**物件都還在；接著才照建立的反序解構。asset 的收尾要停工作、清佇列、經日誌記丟棄件數，用到的執行器、日誌物件可能由別的單位建、也可能由產品經選項給；只靠解構子時，誰先解構取決於建立的先後，收尾的那一方可能看到已經解構的依賴。asset 的 O-A4 要的也是「結束的回呼」。
   - **回呼的先後**：登記的反序（後登記的先跑）。依賴者在被依賴者之後 `Configure`（O-6 的依賴保證），所以先收尾。flecs 的 `ecs_atfini` 是登記的順序（`src/world.c:1071`），本卡照 C++ 解構、`atexit` 取反序，理由是這個依賴關係。執行器由產品給（選項）時，它在所有回呼之後才放掉（選項最後銷毀，見下）。
   - `Product::Build` 失敗時（任何一種啟動錯誤、`Configure` 回報），已經跑過的 `Configure` 登記的回呼照樣在那個未完成的 Product 銷毀時跑，每個恰好一次。
   - 回呼不回錯誤（解構裡沒有上層可交）；要記的自己記（日誌的 ④ 直接寫一筆，或 `Base::Log`）。回呼丟例外是 Programmer Error（解構裡的例外會 `std::terminate`；不另包）。
5. **明確的銷毀順序**（檢查 M4：`std::vector` 元素的解構先後標準沒有規定，libc++ 與 libstdc++／MSVC 不同；今天 `~Product() = default`、`runner_` 比所有 World 先解構）：`~Product` 寫成明確的步驟：
   1. 所有 World，照描述的**反序**銷毀（Extract 的讀者排在來源之後，所以讀者先銷毀；第 5.4 節）；
   2. `OnDestroy` 的回呼，登記的反序；
   3. runner（它可能持有視窗、執行緒，回呼可能還要用它；它可能指著共用處的物件，所以在共用處之前）；
   4. Product 一份的 `ModuleBuffer`，照建立的反序逐個解構（不靠 `vector` 的解構順序）；
   5. 產品自己的 `configure` 與描述裡的閉包、選項（`ProductDescription::options`、各 World 的 `options`）；
   6. 登記單位。

**生命週期的保證**：Product 一份的 `ModuleBuffer` 比 Product 的每個 World 活得久。所以 World Resource 裡可以放指向它的物件的指標（O-10 禁的是指向**舊 World** 的參照）。

**執行緒**：共用處的表（`Create`、`Find`）只在擁有 Product 的那條執行緒上碰（CCR-5）。放進去的物件若會被別的執行緒用（asset store、執行緒池、混音器、網路傳輸），**同步是那個物件自己的事**（D6b、M11 的加註）；編排層不加鎖、不知道它們是什麼。

**不做的**：
- 凍結、只准一個階段寫（使用者撤回 asset 的凍結，`v8-arch-gyo2-asset-no-freeze`）。第 4 批每 World 那一份的凍結照舊（使用者 2026-11-20 的決定）。
- 每個 World 自動出現一份「Product 層 Resource 的投影」：Bevy 是各 World 自己放一份 `Arc` 的副本（第 4.3 節），沒有自動投影；GYO 照「擁有的 Module 自己在準備函式放」。

### 5.3 多個 World 的執行緒（只寫不變量）

**已定的前提**：
- 一個 `Product` 持有多個 World（時間設計決定 2；編排層骨架已實作）。
- runner 決定哪條執行緒跑哪個 World；World 不綁執行緒，需要綁執行緒的 Resource 自己記來源執行緒、碰錯就斷言（`v8-arch-gyo2-thread-binding-by-runner`）。Bevy 現在把綁執行緒的 handle 放在 World 外的 `thread_local!`（`bevy_winit/src/lib.rs:55-59`，issue #17667），方向一致。
- Kernel 沒有鎖與原子操作；同一份 Registry 的登記、建 Plan、World 的建立、銷毀與執行在一條執行緒上，或由呼叫者排好不重疊（CCR-5，DI-KS 26）。
- 編排層現在把 `Product` 的建立、執行、替換、插入、銷毀都放在呼叫的執行緒上（`Product.hpp` 開頭的註解，CCR-5）。
- 一個 World 壞了，Product 就結束；每個入口先檢查所有 World（O-11，使用者 `v8-arch-gyo2-broken-world-ends-product`）。

**要求**（E-13、E-22）：PvP Client 的模擬 World 在自己的執行緒跑，主執行緒停住時模擬照跑；畫面與模擬之間的郵箱只在 schedule 之間存取。

**本批寫進 O-13 的不變量**（本卡照 L17 判；檢查 M5 (a)：只寫不變量，入口層級的行為留給實作那一批）：
1. **單位是一整個 World**。不把一個 World 拆給兩條執行緒（World 的資料不上鎖，D6b、M11）。同一個 World 裡的並行是 Kernel 執行器的事（LIM-2），不在本頁。
2. **整個交出**：runner 把一個 World 交給別的執行緒時，交出期間只有那條執行緒碰它（它的 Registry、執行、交換函式、生成的狀態，第 13 節 W-7）；Product 與別的執行緒不碰。CCR-5 對每個 World 照舊成立。
3. **兩邊只經有鎖的服務交換**（郵箱是 Product 一份、有自己的鎖的物件，各 World 在自己的交換函式裡碰，E-22）；不等另一條執行緒。郵箱是「只留最新一份」還是佇列、誰持有、上限，由用它的 Module 定（Unity Netcode 的快照只留最新一份，第 4.1 節）。
4. **O-11 照舊成立**：交出不改「一個 World 壞了 Product 就結束」。實作那一批要說明它怎麼保住（候選：交出的租約帶一個放在編排層的原子旗標，壞掉當下設、每個入口先看；Kernel 不加原子操作），保不住就照 §3 交使用者。
5. **Extract 在兩邊都停著的時點**（第 5.4 節）。

**留給實作那一批的**（第一個點名的使用者：PvP Client 移植；檢查 S8）：入口的形（暫名 `Product::Lend(world)` → `WorldLease`、`Return`）、`RunFrame` 遇到交出的 World 怎麼辦、指名它的入口回什麼；遊戲初始化要在交出之前跑完（O-1）；讀者交出期間來源 `InsertSystems`（讀者的 Registry 讀來源的登記表，CCR-5 蓋不到這條跨 Registry 的讀）；準備函式在 Product 的執行緒跑、放進去的綁執行緒 Resource 交出後會斷言（Bevy 的 `!Send` 坑）；交出的 World 的形 (3) 由誰做。

**系譜與比過的**：
- **整個交出**（**選這個**）：Bevy pipelined 渲染把 render SubApp 整個當值經 channel 送到渲染執行緒、同步點只在 extract（第 4.2 節）——這是「在別的執行緒跑整個 World」的直接先例。Unity 的 `ExclusiveEntityTransaction` 也是整個交出，但只開放一部分 `EntityManager` 操作給載入用的 World、不在那條執行緒跑 System，作為先例較弱（檢查 S8 (e)）。
- `Product` 的每個入口都可以從任何執行緒呼叫、內部加鎖：退掉。World 的資料不上鎖，鎖只能包住 `Product` 自己的表，兩條執行緒仍會同時跑同一個 World；而且讓 Product 自己的狀態（`failure_`、`gameInitRan_`）變成跨執行緒的權威狀態。
- 一個 World 綁一條執行緒（規格 H-2）：退掉（`v8-arch-gyo2-thread-binding-by-runner` 已拿掉）。
- 渲染在自己的執行緒（Bevy pipelined）：runtime-composition 第 2 節定的是渲染與畫面同在主執行緒，沒有點名，不收（檢查 S4）；若日後點名，照 Bevy 在 extract 的同步點交出。

### 5.4 Extract 的時機與來源替換（只寫契約）

**現況**：Kernel 有 Extract 的宣告那一半——`Extract<Read<T>>`、`ExtractRes<R>` 參數（`Params.hpp:1293-1312`；寫的版本編譯不過）、`ScheduleOptions::extract`、`Registry::SetExtractSource`（要在讀的那一份第一次 `BuildPlans` 之前設，`Registry.cpp:671-674`）、來源沒設或沒建過 Plan 是 D-E14（`Registry.cpp:908-911`）、Extract 參數只在 extract schedule（H-6）。**執行沒有**：巢狀跑 extract schedule 會被拒（`Executor.cpp:473-501`，X-15）；最外層 `Run` 不擋（`Executor.cpp:177-191`），會跑到參數建構子的斷言（`Params.hpp:1299`；舊的 rollback-extract 已隨第 4 批拿掉）。編排層沒有呼叫 `SetExtractSource` 的地方（編排層骨架宣告 3.4 第 3 步留了位置）。

**要求**（E-11、E-14）：渲染 World 有自己的 Registry，每一輪在來源跑完之後唯讀地讀來源，不寫回（使用者 11-11）。runtime-composition 3b：沒有影像回到文件 World、不開 H-6 的例外、選取與編輯用 CPU 射線；「readback 放哪」的三個選項全部不採。

**和 Bevy 不同的一點**：Bevy 的 render 在 extract 時經 `ResMut<MainWorld>` 寫回 main（GPU readback、surface 的色彩空間，第 4.2 節）。GYO 不照（使用者 11-11 的決定）。

**寫進 O-12 的契約**（本卡照 L17 判；系譜 Bevy 的 `SubApp::extract`）：
1. **來源**：一個讀者 World 宣告一個來源 World（描述裡）；來源排在讀者之前（World 照描述的順序跑，所以讀的是來源這一輪跑完的狀態）。編排層在讀者的第一次 `BuildPlans` 之前呼叫 `SetExtractSource`。
2. **時機**：讀者的 extract schedule 在讀者的一輪之前跑（兩個 World 都不在 Run 裡）。extract schedule 照樣寫在讀者的每幀清單裡（不另立一套排序；檢查 S2），編排層看到 `extract` 旗標就改用 Extract 的入口跑。
3. **唯讀**：來源在 extract 期間不被寫（DI-KS 13）；Kernel 的執行原語拿來源當 `const`（暫名 `WorldExecutor::RunExtract(schedule, const World& source)`；DI-KS 15 第 (3)、(4) 項之內，不是清單多一項）。
4. **來源被替換後，讀者不得再讀舊來源**。做法留給實作那一批比（檢查 S2）：(a) 讀者跟著一起重建（只動編排層；代價：讀者的 Resource、準備函式、`beforeSwap`／`extra` 的歸屬，`DropOldFirst` 時兩個都先丟）；(b) 准許讀者再設一次來源、再建一輪 Plan（DI-KS 13 已寫「來源再登記後，讀它的 Registry 下一次建 Plan 讀到新的內容」，DI-KS 26 有之後一輪的 `BuildPlans`；要把 `SetExtractSource` 的「第一次 `BuildPlans` 之前」放寬）。
5. **銷毀**：讀者先於來源（第 5.2 節第 5 點的反序）。
6. **來源壞了**：Product 已經結束（O-11），不跑 Extract。

**留給 SDL 那批的**：Kernel 的原語、描述的欄位名（暫名 `extractFrom`）與它的啟動錯誤、4 的 (a)／(b)、清單或 `RunSchedule` 指名 extract schedule 時的處理（今天最外層會斷言）、測試。批次表 SDL 列「render 回來時一起做 K.3-6」。

### 5.5 時間的取樣（維持過渡，到 SDL 那批定）

**現況**（world-time 第 3 節、WT-6 第 (2) 項，使用者 2026-11-19 核准）：照 Bevy `TimeUpdateStrategy`，`AdvanceTime` 在 World 裡依 `TimeSampling` 取樣；標成「過渡」，寫明「交換點做出來後這一項移給 runner」。**原形**（時間設計決定 1 B，使用者 11-14）：runner 每輪取樣一次、寫進 World。

**本卡照 L17 判：這一批不移、維持過渡，到 SDL 那批定**（不推翻任何決定，不交使用者；檢查 S3）。理由：
- 別家分成兩種（第 4.4 節）：Bevy、Unity 在 World 裡取樣；flecs 由呼叫者給 delta。Bevy 的教訓是取樣點要貼近 present（PR #4744）：答案取決於有視窗的 runner，那在 SDL 那批才有。
- 目前沒有點名的 Feature 需要「同一條執行緒上兩個都推進時間的 World 用同一個取樣」：Client 的模擬與畫面在兩條執行緒、各自取樣（時間設計 5.5 (三)）；渲染 World 的時間經 Extract 從畫面 World 拿。
- 這一批做的交換函式的登記，讓那時有地方接（world_time 登記一個「進」，把 runner 給的取樣寫進 `TimeSampling` 的 `ManualInstant`）。
- **那一批要一起定的**（檢查 S3）：等待零件在哪、時間設計 5.5 (一) 第 5 步的「取樣時刻」runner 從哪讀；渲染 World 的描述要不要帶 world_time（今天 `default=yes`，帶了就自己取樣，和「時間經 Extract 拿」衝突；描述的 `units` 可以不帶它）；WT-6 第 (2) 項、WT-2 與 world-time 第 3 節的文字。

### 5.6 各批的需求怎麼接

| 需求 | 出處 | 接法 | 本批做到哪 |
|---|---|---|---|
| X-1 執行器：讀檔解碼在 World 執行緒外；停了之後提交是空操作；介面由 asset 自己的話定 | asset 5.6 | 執行器的實作由產品（或測試）放進 `ProductDescription::options` 的持有者結構，world_asset 在 `Configure` 讀；或由 world_asset 依賴的單位（之後的 threads Module）在它自己的 `Configure` `Create` 進共用處，world_asset 在 `Configure` `Find`（第 5.2 節第 1、2 點）。停了之後的行為是執行器自己的契約 | 選項、共用處實作；執行器本身在 asset 實作批與 threads 重審 |
| X-2 交換點不認得 asset | asset 5.6 | 形 (1) | 契約（O-12） |
| X-3 若定了「每幀的邊界」，套用 System 掛在它之後 | asset 5.6 | 形 (1) 的「進」＝一輪最前面的 stage（`Main`／`First`、生成中 `Generate`／`First`）；交換函式的「進」在它之前 | 契約（O-12） |
| X-4 執行緒的數量與歸屬由交換點或 runner 定 | asset 5.6 | 執行緒池由產品或 threads 的 Module 建（同 X-1 的兩條路）；World 的執行緒由 runner 定（第 5.3 節） | 契約 |
| X-5 測試能控制工作何時開工、完成 | asset 5.6 | 測試在 `ProductDescription::options` 放自己的執行器（持有者結構） | 選項實作 |
| X-6 Product 結束時，未完成的工作能停下或等完 | asset 5.6 | world_asset 的 `OnDestroy` 回呼叫執行器停下（執行器介面是 asset 的話，可以帶 `Stop`）；回呼跑時執行器一定還在（第 5.2 節第 4、5 點） | 實作 |
| O-A1 `AssetOptions` 在 world_asset 的 `Configure` 之前就在 | asset 5.4 | `ProductDescription::options` | 實作 |
| O-A2 依賴 world_asset 的單位拿得到 `AssetLoaderRegistry` | asset 5.4 | world_asset 在 `Configure` `Create`（介面用持有者結構），依賴它的單位在 `Configure`／`Build` `Find`（O-6 的依賴保證先後） | 實作 |
| O-A3 `Configure` 的錯誤回報 | asset 5.4 | `ProductSetup::Report` | 實作 |
| O-A4 Product 結束時的回呼 | asset 5.4 | `ProductSetup::OnDestroy` | 實作 |
| G-1～G-3 生成世界分批之間的套用點 | asset 5.13 | 第 13 節 W-2～W-4 | 和生成世界卡對齊 |
| 網路：權威值寫進 `RollbackInbox`、下一幀開頭判斷 | rollback 第 2 節 | 收件在回滾的決定（`PreUpdate`）之前：形 (1) 掛 `First`，或形 (2) 的「進」；網路 Module 批照第 5.1 節的規則選一種。傳輸物件在共用處 | 契約 |
| SDL：OS 事件進 World | 批次表 SDL 列 | 有視窗的 runner 走形 (3)（Bevy winit），或輸入服務＋形 (1)（Bevy gilrs）；SDL 那批選 | 契約 |
| SDL：渲染 World、Extract | runtime-composition 第 2 節 | 第 5.4 節 | 契約；實作在 SDL 那批 |
| SDL：裝置是 Product 一份 | asset 5.4「預先登記」 | 共用處 | 共用處實作 |
| PvP Client：郵箱 | runtime-composition 第 2 節 | 郵箱是共用處裡有鎖的物件；兩個 World 各在自己的交換函式裡碰（形 (2)） | 交換函式的登記實作；郵箱在 PvP 移植 |
| threads：runner 的執行緒 | R0 附錄 | runner 建、runner 收；World 整個交出（第 5.3 節） | 不變量；threads 重審照它重寫 `RoleThread` 的用途 |
| io：多執行緒同時讀檔 | asset 5.12 | io 自己的契約；本頁只要求 World 外的服務自己同步 | 不在本頁 |
| audio：混音器、`Submitter` | 批次表 audio 列 | 混音器是 Product 一份的物件；`Submitter` 綁執行緒、自己記；World 交出到別的執行緒時要在那條執行緒上另拿一個——audio 重審要對的點 | 不變量；audio 重審照它 |
| 日誌：Kernel 診斷紀錄 | 日誌宣告 5.4 | 形 (2) 的「出」：取的一方（產品自己的 `build`，或之後把 world_logging 做成登記單位）登記一個呼叫 `WorldLogging::TakeKernelDiagnostics` 的「出」；World 壞掉之後照日誌宣告的順序走形 (3)。`TakeKernelDiagnostics` 的介面不變 | 交換函式的登記實作；接線在日誌那一側 |
| 時間：取樣 | world-time 第 3 節、WT-6 | 第 5.5 節 | 維持過渡 |

---

## 6. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：第 3.2 節的 E-1～E-23。直近的是 asset 的實作（O-A1～O-A4、X-1、X-5、X-6）與 Kernel 診斷紀錄的接線（E-19），之後是網路、SDL、PvP Client、threads／io／audio 重審。
2. **現在的問題**：Product 一份的東西沒有地方放，產品與測試的物件到不了單位的 `Configure`（第 5.2 節）；交換點只有 11-14 的原形，沒有實作，和後來的分層、asset 的套用點沒有整理（第 5.1 節）；`~Product` 的銷毀順序靠 `vector` 的解構與成員的宣告順序（第 5.2 節第 5 點）；`Product` 的執行緒前提（CCR-5）沒有寫明放寬的不變量（第 5.3 節）；Extract 在 Kernel 只剩宣告那一半（第 5.4 節）。
3. **變動的邊界**：編排層的公開介面（`ProductDescription::options`、`ProductSetup::Buffer`／`Options`／`Report`／`OnDestroy`、`WorldSetup`／`WorldFinish`／`WorldCheck::ProductBuffer`、`Product::Buffer`、`WorldSetup::ExchangeIn`／`ExchangeOut`、`OrchestrationErrorCode::ExchangeFailed`）；`~Product` 的順序；之後（不在本批）Extract 的描述欄位與入口、Kernel 的 Extract 原語、World 的交出。
4. **影響的 Module／Product／Tool**：編排層（實作）；asset、網路、SDL、threads、audio、world_logging（照本頁接）；world_time（SDL 那批若移取樣）；現有產品沒有（`gyo2` 上沒有產品）。
5. **Dependency Direction**：不變。共用處與選項的型別由放的一方定，編排層不 include 任何 Module（O-4、RC-O4）。Module 之間經共用處互相拿物件時，依賴照舊寫在登記檔（O-6）。
6. **Ownership**：「Product 一份的共用處、Product 層的選項、`Configure` 的回報、銷毀的回呼與順序」歸編排層（O-2、O-5；是否算職責清單多一項，交使用者確認，第 12 節）；交換函式的登記歸編排層（O-4 已列「交換點」）。共用處裡的物件歸放它的一方。
7. **為什麼不能更小**：只給 asset 一個專用入口會讓編排層認得 asset（RC-O4、RC-O8），下一個又要加；不做交換函式的登記，Kernel 診斷紀錄的取走要每個 runner 各接一次（RC-O1 第二句、日誌的 RC-L2），郵箱也沒有「只在 schedule 之間」的地方；不寫交換點與執行緒的契約，網路、SDL、threads 會各自定一套。

---

## 7. Design Invariants 與相關文件的改動

草案；最終文字在合併前核准定稿。

**編排層 `orchestration.zh-Hant.md`**：
- **O-2 加一段**：Product 一份的 `ModuleBuffer`（不凍結、活到 Product 結束、表只在擁有 Product 的執行緒上碰、物件的同步歸物件）；Product 層的選項只轉交、不解讀，產品／測試的物件經它在 `Configure` 就到；`Configure` 有回報就不建任何 World；`OnDestroy`（兩段式、登記的反序、`Build` 失敗也恰好一次）；`~Product` 的六步順序。
- **O-3 加一句**：runner 在兩次入口之間讀寫 World、呼叫 Module 給的函式是交換點的形 (3)；只做屬於 runner 的事，Module 的資料走 (1)、(2)。「只呼叫 Product 的執行入口」改成「只呼叫 Product 的執行入口與 Module 給的函式」。
- **O-5 加一句**：`ProductSetup::Report` 收進 `ProductStartupReport`；交換函式的失敗是 E2 `ExchangeFailed`。
- **O-7 加一句**：交換函式和準備函式一樣在 `Build` 登記、隨替換重新登記。
- **O-8 加一句**：Extract 的來源被替換後，讀者不得讀舊來源（做法在 SDL 那批）。
- **O-12 新增「交換點」**：四種形、選哪一種的規則、各自的規則、交換函式的登記與呼叫（第 5.1 節）、Extract 的契約（第 5.4 節；標「實作在 SDL 那批」）。
- **O-13 新增「多個 World 的執行緒」**：第 5.3 節的五條不變量（標「入口的形在第一個點名的使用者那批」）。
- **O-11 不改**（O-13 第 4 條寫明它照舊成立）；**O-4 不改**（交換點、extract 的時機、執行緒的安排已在清單；Product 層 Resource 那幾項交使用者確認，第 12 節）。

**Kernel `kernel.zh-Hant.md`**：
- **DI-KS 13 補一句**：Extract 的執行原語拿來源當 `const`；來源與時機由編排層給（O-12）。

**跨元件規則 `docs/cross-component-rules.md`**（AGENTS §1：Contract 直接推出、每個使用者都要照做的規則要登記；檢查 S7）：
- 新登記一條（owner 編排層）：「World 外的服務自己同步；套用 System 與交換函式拿服務的鎖只做表的操作、不等另一條執行緒；同步呼叫帶回的值照 D6b 的加註」。
- CCR-5 的「誰負責」欄：加「O-13：World 整個交出時，持有的執行緒負責」。

**加註**（不改當時的紀錄，只在頁內加一行指向本頁）：runtime-composition 第 2 節末列（郵箱 → 形 (2)）、日誌宣告 5.4 末句（交換點已排：形 (2) 的「出」）、時間設計 5.3（交換函式的登記照它做，細節在本頁 5.1）與 6.4（形狀落在形 (2)）。

### Replacement Criteria（寫在積上去之前）

- **RC-O10 改寫**：CCR-5 放寬時（建 World 要並行、一個 World 的登記單位並行 `Build`、或 World 交出到別的執行緒），每 World 與 Product 一份的 `ModuleBuffer` 需要加鎖或改成別的同步方式；或整個交出（O-13）保不住 O-11、做不出點名的 Feature。
- **RC-O11 新增**：交換函式的登記沒有點名的使用者了（例：診斷紀錄與郵箱都改成別的形），或某個點名的 Feature 需要進、出之外的呼叫點（例：每張 schedule 之間），或需要知道「這一輪是第幾輪、是不是生成」才做得出。
- **RC-O12 新增**：Product 一份的共用處需要知道放進去的物件是什麼（例：要依型別做不同的收尾）、`OnDestroy` 的反序排不出某個收尾的先後，或兩個單位需要共同擁有一個型別。
- **RC-O13 新增**：Extract 的來源替換（O-8）讓讀者的重建成為替換時間的主要部分，或讀者有不能重建的狀態（和 RC-O9 第二句一起看）。

---

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的選項 | 決勝理由 | 需要它的 Feature／Product |
|---|---|---|---|---|
| Product 一份的 `ModuleBuffer`（交使用者，第 12 節） | 第 4 批的 `ModuleBuffer`（規則「一方建、別人找」）；Bevy 的 main World Resource、lightyear 的 `ComponentRegistry`；flecs 的 world ctx（第 4.3 節） | 第 12 節的 (乙) 單位互相找、(丙) 放在某個 World；給每個 Module 專用入口（認得 Module，RC-O4）；新型別（見第 5.2 節第 1 點：共用的只有「一方建、別人找」，兩種用法的差別寫在型別註解） | E-1、E-12、E-16 要的只是「一方建、別人找、Product 一份」；產品、runner、測試也要放（E-5、E-7），單位的類別不必公開 | asset、SDL、audio、threads |
| Product 層的選項經描述給 | `WorldDescription::options`；Bevy Plugin 的欄位（`AssetPlugin { file_path }`） | 單位表讓產品自己建單位物件（產生的單位表的形狀要改，O-6）；產品在 `configure` 寫進共用處（產品的 `configure` 在所有單位之後，O-A1、X-1、X-5 不成立） | E-2、E-5、E-7 要「產品與測試的東西在單位的 `Configure` 就在」 | asset |
| `OnDestroy`：兩段式、World 之後、反序 | flecs `ecs_atfini`（時點同；先後不同：flecs 是登記順序）；C++ 解構與 `atexit` 的反序 | 只靠共用處物件的解構子（收尾時依賴可能已解構）；單位的解構子（單位最後銷毀，物件已不在）；回呼回錯誤（解構裡沒有上層） | E-4：收尾時所有依賴都要在 | asset |
| 明確的銷毀順序 | — | `= default` 靠成員宣告與 `vector` 的解構順序（標準未規定、平台不同） | 三平台同一個結果；Extract 的讀者先於來源 | 全部 |
| 交換函式的登記（形 (2)） | 時間設計 5.3（規格 H.1）、統合 L19、runtime-composition 第 2 節 | 不做（只有形 (1)、(3)、(4)；修訂 0 的建議） | E-19：診斷紀錄要在 Run 之外取；E-22：郵箱只在 schedule 之間（使用者 11-11）。別家沒有這一層（第 4.1 節），GYO 有這兩個自己的決定 | 日誌、PvP Client |
| 四種形與選擇的規則 | Bevy、lightyear、Unity Netcode、flecs、Mass（形 (1)、(3)、(4)） | 一律交換函式（asset 已定套用 System，X-2；和 System 排先後做不到） | E-6、E-8、E-10、E-20 | asset、網路、SDL |
| World 整個交出（只寫不變量） | Bevy pipelined 渲染 | `Product` 入口加鎖；World 綁執行緒 | E-13：主執行緒停住時模擬照跑，且 World 的資料不上鎖 | PvP Client |
| Extract：來源在前、讀者的一輪之前、替換後不讀舊來源（只寫契約） | Bevy `SubApp::extract`；K.3 第 6 項 | 現在就定「讀者一起重建」或「准許換來源」（兩種都留給 SDL 那批比） | E-11、E-14 要的是時機與唯讀；替換的做法要等渲染 World 的實際狀態 | Client、Editor 的渲染 |
| 時間的取樣維持過渡 | Bevy、Unity（World 裡）；flecs（呼叫者） | 現在移給 runner；現在拿掉「過渡」 | 沒有點名的 Feature；取樣點跟著有視窗的 runner（Bevy PR #4744） | — |

既有的 Test、守衛不變，不是任何一列的決勝理由（AGENTS §4）。

---

## 9. thin start 與實作的步驟（動工前核准、使用者說開始之後）

只實作名指的 Feature 直近要用的部分：asset 實作批要的 Product 層 Resource、日誌等著的交換函式的登記。其餘（執行緒的交出、Extract、時間的取樣）只寫契約或維持現狀。

| 步 | 內容 | 實作者（模型／檔位） |
|---|---|---|
| 第 0 步 | 兩條守衛（已做，PR #268，第 15 節） | Opus 5.5／high |
| 第 1 步 | Product 一份的 `ModuleBuffer`、`ProductDescription::options`、`ProductSetup::Report`、`OnDestroy`、`~Product` 的明確順序；測試 P-1～P-9 | Opus 5.5／high |
| 第 2 步 | 交換函式的登記與呼叫、`ExchangeFailed`；測試 X-1～X-6 | Opus 5.5／high |
| 第 3 步 | 第 7 節的 Design Invariants、跨元件規則、加註；第 12 節定了之後照定的寫 | Opus 5.5／high |
| 第 4 步 | 突變、敵對式檢查、設計審查（批次表〈每批的設計審查〉）、dev_log、合併前核准頁 | 見下 |

上段（公開介面、Design Invariants）。全部 high、不用 xhigh（`xhigh-only-for-investigation`）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus 5.5／high | 上段，不用小模型 |
| 新突變清單的敵對式檢查 | Fable 5.2／high | §14.2；和定清單的人不同模型 |
| 設計審查 | Fable 5.2／high | 批次表〈每批的設計審查〉 |
| 本宣告的敵對式檢查 | Fable 5.2／high | 任務卡的指定 |

估計（agent 時間）：第 1 步 8～12 h、第 2 步 4～6 h、第 3 步 3～5 h、第 4 步 5～8 h；合計 20～31 h。

---

## 10. 測試、判定（先宣告；結果出來之後不放寬）

行為測試放 `tests/common/orchestration/`（合成的單位、World 與服務，不依賴任何 Module）：

| 案 | 內容 |
|---|---|
| P-1 | 單位 A 在 `Configure` `Create<T>`；單位 B（依賴 A）在 `Configure`、`Build`、`Finish`、`Check` `Find<T>` 拿到同一個物件（`Finish`、`Check` 可寫）；產品自己的 `configure`、`build` 也拿到；runner 經 `Product::Buffer()` 拿到。產品在自己的 `configure` 放的物件，單位在 `Configure` 拿不到、在 `Build` 拿得到 |
| P-2 | `Find` 沒人建的型別回 `nullptr`、不代建；兩個單位 `Create` 同一型別是 Programmer Error |
| P-3 | 替換 World 之後，Product 一份的物件還是同一個（位址相同）、`WorldSetup::ProductBuffer()` 是同一份；每 World 那一份是新的 |
| P-4 | `ProductDescription::options` 在第一個單位的 `Configure` 就讀得到；持有者結構裡的介面物件（測試的執行器）可以呼叫；編排層不解讀 |
| P-5 | 銷毀順序：World 照描述的反序 → `OnDestroy` 照登記的反序 → runner → 共用處照建立的反序 → 描述與選項 → 單位（用記錄解構先後的物件驗；L1 三平台都跑） |
| P-6 | `OnDestroy` 在每一種結束路徑各恰好一次：正常銷毀、O-E 啟動錯誤、Kernel 啟動錯誤、`Configure` 回報、準備失敗（O-E7）；回呼裡 `Find` 還拿得到共用處的物件；沒登記時什麼都不做 |
| P-7 | `ProductSetup::Report`：`Build` 失敗、不跑任何 World 的 `Build`、其餘單位的 `Configure` 照跑完；錯誤在 `unitErrors`（`world` 空；`unit` 是登記列名，產品自己的是產品名） |
| P-8 | World Resource 裡指向 Product 層物件的指標，在每個 World 銷毀時都還有效 |
| P-9 | 形 (1) 走一遍（檢查 S9）：合成的服務（一條真的工作執行緒＋閂）由單位 A 放進共用處；World 的 `First` 套用 System 拉結果寫進 World；`OnDestroy` 停工作；工作在回呼之後不再碰服務 |
| X-1 | 交換函式：`RunFrame` 對每個 World 照「進 → 一輪 → 出」呼叫，單位依序、產品的最後；沒有登記時不呼叫 |
| X-2 | `RunGameInit`、`RunSchedule` 不呼叫交換函式 |
| X-3 | 函式回錯誤：`RunFrame` 回 `ExchangeFailed`（帶函式的碼），World 沒有壞、Product 沒有結束，前面的 World 已跑；下一次 `RunFrame` 照常 |
| X-4 | 函式丟例外：原樣往外傳，World 沒有標成壞 |
| X-5 | 有 World 壞了：`RunFrame` 不呼叫任何交換函式（O-11） |
| X-6 | 替換 World 之後，交換函式是新的 `Build` 登記的那一組（舊的不再被呼叫） |

- **判定 1**：本機完整 ctest 全過；L1 三平台（linux-x64、windows-x64、macos-arm64）全過。
- **判定 2**：P-1～P-9、X-1～X-6 全過。
- **判定 3**：新突變（README 第 3 點：每個獨立可錯的決定至少一個；上限參考「改的 DI 條數 × 5」）經 Fable 5.2／high 檢查補齊後全殺；每步只跑相關突變，合併前背景完整跑一次（Kernel＋編排層＋`world_time`＋權威＋回滾＋base＋naming）。
- **判定 4**：設計審查的報告與處理寫進合併前核准頁。
- 生成世界的「一輪」（第 13 節 W-2）合起來的案例，在兩批都合併之後由後合併的那一批補（交換函式的進、出包住生成的一批、結束那一批只各一次）。

本批沒有效能的判定：共用處是個位數項目的線性表（E-21），交換函式是每個 World 每輪各一次的 `std::function` 呼叫，沒有登記時不付費。

---

## 11. 本卡照 L17 判的（不是使用者的話）

- **交換函式的登記照做**：時間設計 5.3、統合 L19、runtime-composition 第 2 節（使用者 11-11）都已經是這個形；修訂 0 建議不做是推翻，檢查 M2 指出它有點名的使用者（E-19、E-22），所以照原形做，不交使用者。細節（`Build` 登記、只在 `RunFrame`、失敗是 E2、壞掉的 World 不呼叫）照準備函式與 O-11 的寫法。
- **時間的取樣維持過渡**：第 5.5 節；不推翻、不交使用者。
- **執行緒只寫不變量**、**Extract 的替換兩種做法都留給 SDL 那批**：thin start；名指的使用者還沒搬回。
- **名字**：沿用 `ModuleBuffer`、`OptionBag`；`ProductBuffer()`、`ExchangeIn`／`ExchangeOut`、`OnDestroy`、`ExchangeFailed` 照同組的寫法；暫名（`Lend`、`WorldLease`、`RunExtract`、`extractFrom`）在實作的那批照命名規範定。
- **反序**：照 C++ 解構、`atexit`；flecs 是登記順序，不照（第 5.2 節第 4 點）。
- **網路收件在回滾決定之前**：stage 的先後（`First` 早於 `PreUpdate`）或交換函式的「進」，照 lightyear 的「收 → 判斷」。
- **E-9 拿掉**：只有根據 2。

---

## 12. 要使用者定的

只交三類：推翻使用者之前的決定、別家分歧且真有取捨、職責清單多一項。修訂 1 只剩一題（修訂 0 的第 1、2 題改成照已核准的計畫做，第 11 節）。

### 第 1 題：Product 層 Resource 要不要、用什麼形；算不算職責清單多一項（使用者開的題，`v8-arch-gyo2-product-level-resource-open`；別家分歧）

- **背景**：asset 的 O-A1～O-A4、X-1、X-5、X-6（第 5.6 節）；SDL 的裝置、audio 的混音器、執行緒池也都是 Product 一份。**前提的更正**：這一題原本寫「像 Bevy 的 App 層 Resource」。偵察查到 Bevy 的 `App` 在 World 外沒有 Resource（`bevy_app/src/app.rs:85-96`），那是 main World 的 Resource；Bevy 能這樣做是因為它有主 World、Plugin `build` 時 World 已經在。GYO 的 World 彼此平等、`Configure` 時還沒有 World（第 4.3 節）。別家沒有一家有獨立於 World 的 Product 一份的型別化存放處，最接近的是 Unreal 的 GameInstance subsystem〔知〕。
- **遊戲的例子**：world_asset 在 `Configure` 建 asset store；image Module（之後的 SDL 那批）要在 `Configure` 拿到它來登記貼圖的 loader；產品要在描述裡給內容根目錄；測試要換成自己控制的執行器；Product 結束時 asset 要停工作、記丟棄件數。
- **(甲) Product 一份的 `ModuleBuffer`＋描述的選項（建議）**：第 5.2 節。單位建的放共用處；產品、測試給的放選項。共用的是「服務物件」，單位的類別不必公開。
- **(乙) 單位互相找**：`ProductSetup::Unit<T>()` 拿到別的登記單位的實例、呼叫它的公開方法。系譜：Unreal 的 subsystem（以類別為鍵、有依賴的先後）〔知〕、Bevy 的 `get_added_plugins::<T>()`（唯讀、讀設定，`bevy_app/src/app.rs:596`）。代價：每個單位的類別變成公開介面；runner 拿物件要經單位。產品、測試的物件照樣走描述的選項。
- **(丙) 照 Bevy 放在某個 World**（第一個 World，或每個 World 一份副本）：`Configure` 時沒有 World，O-A1、O-A2 做不到；要改生命週期（O-2），第一個 World 被替換時要搬。不建議。
- 三個選項都配 `ProductDescription::options`（O-A1）、`ProductSetup::Report`（O-A3）、`OnDestroy` 與明確的銷毀順序（O-A4）。
- **職責清單**（檢查 S6）：第 4 批每 World 那一份「不算 O-4 多一項」是使用者 2026-11-20 確認的。這一次多了執行期也能讀寫的 `Product::Buffer()`（runner 用）與銷毀回呼，超出「登記單位與生命週期」的字面。本卡的判斷是仍屬「登記單位與生命週期」「啟動錯誤的彙整」、不算多一項；請一起確認。
- **根據**：第 4.3 節、第 5.2 節。

---

## 13. 和生成世界（重做）的交界

生成世界的宣告由另一張卡寫（`gyo2-world-gen`，`docs/architecture/target/v2/world-gen-declaration.zh-Hant.md`，分支 `claude/gyo2-world-gen-77331d`，修訂 0 c8b5faf4，第 5.1～5.5、5.14 節）。2026-11-22 兩張卡用 SendMessage 對齊，結果如下（**已對齊**；兩份宣告各自核准時以這張表為準）：

| # | 事項 | 誰負責 | 對齊的內容 |
|---|---|---|---|
| W-1 | 生成的形 | 生成世界 | 每個 World 有生成清單與每幀清單；生成中的 World 在一次 `RunFrame` 裡跑一批生成清單、不跑每幀清單；哪一批跑完沒人 `Hold()` 就結束，同一次 `RunFrame` 接著跑每幀清單（照 Bevy 第一次 update 先跑 startup）。一批＝一次 `RunFrame`，批與批之間 runner 回到迴圈 |
| W-2 | 「一輪」的定義 | 本頁（生成世界提案、本頁同意） | 一輪＝這個 World 在這次 `RunFrame` 裡跑的全部：平常＝每幀清單；生成中、這批有人 `Hold()`＝這一批生成清單；生成結束的那一批＝那批生成清單＋接著的每幀清單。交換函式的「進」「出」包住一輪，結束那一批也只各一次（第 5.1 節）；Extract 在讀者的一輪之前（第 5.4 節）。生成世界不必提供「這一輪是不是生成」給交換函式（要的時候是 RC-O11） |
| W-3 | 套用點（形 (1)） | 擁有服務的 Module | 把 World 外的結果帶進 World 的 System（asset 的套用、網路的收件），預設骨架下同時掛 `Main`／`First` 與 `Generate`／`First`；骨架關掉時作者自己掛（同 O-4）。asset G-1 由此成立 |
| W-4 | `beforeSwap` | 生成世界（保留）；本頁寫明 | `beforeSwap` 是換上之前同步跑一次的關卡，不是生成；期間沒有套用點、沒有交換點（asset G-3），不加機制 |
| W-5 | 別的 World 知道某個 World 在生成 | 本頁 | 不加新機制：runner 在兩次入口之間讀 `Product::Generating(world)` 寫進別的 World（形 (2)），或 Module 的服務＋套用點（形 (1)）。生成世界提供 `Product::Generating`、`GenerationPasses` 與 World 內的 `WorldGeneration` |
| W-6 | 產品給 Module 的設定 | 本頁（Product 層）；生成世界（配發器） | 預設實體配發器的範圍每個 World 可能不同，用 `WorldDescription::options`（`EntityAllotterOptions`），不用 Product 層 Resource；Product 層 Resource 的形狀歸本頁 |
| W-7 | 執行緒 | 本頁 | 生成的狀態在 Product 的 World 槽、照 CCR-5 在呼叫的執行緒上讀寫；World 交出到別的執行緒時，生成的狀態跟著交出，那幾批由持有的執行緒經交出的入口推動（第 5.3 節） |
| W-8 | 結束條件 | 生成世界 | 由作者的 System 給（asset G-2），不經本頁 |
| W-9 | 遊戲初始化 | 生成世界 | 照舊先跑，跑在第一個 World 的生成之前 |

## 14. 範圍的限界

- 執行緒的交出、Extract 只寫契約；它們的實作批可能發現契約不夠（那時照 §14 冒頭停下、回到本頁改）。
- 共用處不加凍結：「邊讀邊寫」的風險在物件自己身上（第 4 批使用者擔心的是建 World 期間的登記表，那一份照舊凍結）。誤用（把某個 World 的參照放進共用處、讓它活過那個 World；交換函式等另一條執行緒）不加守衛，交設計審查（`no-guards-against-misuse`）。
- `OnDestroy` 只在 Product 銷毀時跑；行程被殺、`std::terminate` 時不跑。
- 交換函式失敗時，前面的 World 已經跑了這一輪（同一張 schedule 在中途失敗時的寫法）；Product 不結束，下一輪各 World 的輪數會差一。錯誤只告訴外面，怎麼處理由 runner 決定。
- 生成世界與交換函式合起來的案例，要兩批都合併才測得到（第 10 節末）。

---

## 15. 第 0 步：第 5 批開頭的兩條守衛

任務卡排在宣告之前先做、可以獨立成一個 PR（分支 `claude/gyo2-exchange-guards`）。內容、取捨、敵對式檢查（Fable 5.2／high：必修 1、提案 3、輕微 5，全部處理）與驗證寫在 dev_log [`2026_11_22_gyo2_exchange_guards.zh-Hant.md`](../../../dev_logs/2026_11_22_gyo2_exchange_guards.zh-Hant.md)。摘要：

- Kernel「拿掉的名字不回來」：`kernel_guard.py` K9 加 `K9_REMOVED`（判定 4 的樣式改寫成識別字的形狀；舊的啟動錯誤碼不列，`StartupCode` 的比對已守）。突變 `ks-k9-removed-dropped`。
- 權威 Module 的連結集：`gyo_expect_links(gyo_authority …)`（DI-A 9）。突變 `au-links-more`；不加 DEFER 的突變。
- 守衛欄：DI-KS 15、DI-A 2、DI-A 9。只收緊；DI 的守衛照 §14.4 要使用者核准（dev_log 的表）。
- 和命名收尾卡（`gyo2-naming-cleanup`）碰到的檔已通知對方；誰後合併誰 merge 跟上。

---

## 16. 修訂 0 的敵對式檢查：處理對照

檢查者 Fable 5.2／high（唯讀；作者 Opus 5.5）。報告：主 checkout `build/evidence/gyo2-exchange/declaration-r0-adversarial-check.md`。別家引用抽查 12 處全部對得上。

| # | 指出 | 處理 | 落點 |
|---|---|---|---|
| M1 必修 | 產品／測試在 `configure` 放進共用處的東西，單位的 `Configure` 拿不到；`Create<T>` 放不進介面型別 | 通則：產品、測試的物件走 `ProductDescription::options`（持有者結構）；介面用持有者結構；X-1、X-5 改寫；P-1、P-4 加案 | 5.2 第 1、2 點、5.6 |
| M2 必修 | 形 (2) 的規則和 E-19（診斷紀錄）矛盾；第 2 題漏了點名的使用者、runtime-composition 第 2 節（郵箱只在 schedule 之間）與 L19 | 交換函式的登記照時間設計 5.3 做（不推翻、不交使用者）；加 E-19 的改寫、E-22、E-23（七個使用者逐一對到四種形）；形 (3) 允許 runner 呼叫 Module 給的函式 | 3.2、5.1、5.6、11、12 |
| M3 必修 | readback 經服務、晚一幀，牴觸使用者 11-11（沒有影像回到文件 World） | 整段刪掉，只留「Bevy 的寫回不照」 | 5.4 |
| M4 必修 | 銷毀順序靠 `vector` 的解構順序（標準未規定）、`runner_` 比 World 先解構、讀者要先於來源 | `~Product` 明確的六步；P-5 三平台驗 | 5.2 第 5 點、10 |
| M5 必修 | O-13 第 2 點弱化 O-11 卻寫成「限界」 | 照 (a)：O-13 只寫不變量，第 4 條寫明 O-11 照舊成立，入口層級的行為留給實作那一批（附候選做法） | 5.3 |
| M6 必修 | 第 8 節兩列的決勝理由是「既有的不必改」 | 改成以 Feature 為準；回答「兩種 `ModuleBuffer` 是不是同一個概念」 | 5.2 第 1 點、8 |
| S1 提案 | `OnDestroy` 沒和 RAII 比 | 寫明兩段式的理由與 asset 的收尾走法；RC-O12 加「反序排不出的收尾」 | 5.2 第 4 點、7、8 |
| S2 提案 | Extract 來源替換的退掉選項寫得太重；extract schedule 的排序另立一套；最外層 `Run` 不擋 extract schedule | O-12 只寫「替換後不讀舊來源」，(a)／(b) 留給 SDL 那批；extract schedule 照樣寫在每幀清單；現況更正 | 5.4 |
| S3 提案 | 第 1 題少「延後」的選項與兩個後果 | 改成維持過渡、到 SDL 那批定（不交使用者）；那一批要一起定的列出 | 5.5 |
| S4 提案 | E-9、pipelined 渲染只有根據 2 | E-9 拿掉；pipelined 從 Feature 欄拿掉、降成註記 | 3.2、5.3 |
| S5 提案 | `Configure` 回報之後的流程沒定、測試缺口 | 有回報就不建任何 World；`unit` 欄的值；`OnDestroy` 各路徑恰好一次；替換後同一份；回呼裡 `Find` 還在 | 5.2 第 3 點、10 |
| S6 提案 | 「不算 O-4 多一項」要交使用者確認 | 併進第 12 節第 1 題 | 12 |
| S7 提案 | 跨元件規則、要改的文件沒列全 | 跨元件規則新登記一條、CCR-5 的「誰負責」；O-3、O-5、O-7、O-8 的改寫；加註清單 | 7 |
| S8 提案 | 交出／交還的五個缺口；Unity 的先例偏弱 | 列進「留給實作那一批的」；系譜改寫 | 5.3 |
| S9 提案 | 形 (1) 沒有測試 | P-9 | 10 |
| N1 | `Finish`／`Check` 給 `const` 等於半個凍結 | 都給可寫的 | 5.2 第 1 點 |
| N2 | 套用掛 `First` 和別家不同；同一 stage 的先後 | 寫明 | 5.1 |
| N3 | 第 13 節的對齊結果是空的 | 已對齊（W-1～W-9） | 13 |
| N4 | `ModuleBuffer` 的註解只講一種用法 | 型別註解寫兩種用法 | 5.2 第 1 點 |
| N5 | 「建好之後還能 `Create`」沒有點名的使用者 | 不寫成需求；P-3 改驗替換後同一份 | 5.2 第 1 點、10 |

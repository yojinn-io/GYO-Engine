# 事前宣告：Extract 的來源被替換（`v8-arch-gyo2-sdl-r1b`，SDL 線子批 R1b）

更新：2026-11-25（修訂 0）。Owner：Kernel＋編排層（GYO2 主會話開的任務卡，SDL 線子批 R1b）。**狀態：等使用者選做法（第 10 節 Q1）**。選定之前不動兩個做法涉及的程式。

根據：
- SDL 總宣告（`sdl-declaration.zh-Hant.md` 修訂 21，已核准）第 6.1 節第 4 點、第 6.1 節第 8 點（實體對應）、第 10.1 節（條號：DI-KS 38～40、RC 29 給 R1b）、第 16 節分批表的 R1b 列（上段；驗收：來源替換後讀新的、`BuildNewFirst` 的重設時點、換來源時對照整批作廢、`DropOldFirst`）。
- 前提 P7（使用者 2026-11-11）：模擬的 World 與渲染的 World 分開，渲染的 World 有自己的 Registry，只經 Extract 以 const 讀來源，不寫回。
- [Kernel](../kernel.zh-Hant.md) DI-KS 13、25、26、35～37、RC 28；[編排層](orchestration.zh-Hant.md) O-2、O-8、O-10、O-11、O-12、O-14、O-15、RC-O9、RC-O13；[net](net.zh-Hant.md) 第 6、9 節（D-8 重連）。
- 已合併的程式：R1a（#299）的 `RunExtract`／`extractFrom`、R2a／R2b 的 `render_world`、`sdl_gpu`。讀的版本：`origin/gyo2` 981f2604。
- 對照：Bevy（本機 `~/Code/GitHome/bevy`，2026-11-10）、Godot（`~/Code/GitHome/godot`）、Unity Netcode for Entities（`~/Code/GitHome/com.unity.netcode`）。

---

## 0. 前提表

| # | 前提 | 誰定的 | 掛在上面的決定 |
|---|---|---|---|
| P1 | 渲染 World 只經 Extract 唯讀讀來源，不寫回 | 使用者 11-11（DI-KS 13） | 兩個做法都不改唯讀 |
| P2 | `ReplaceWorld` 照產品描述重新登記，給**新的 Registry**；先後由作者選 `BuildNewFirst`／`DropOldFirst` | 使用者（O-8） | 第 3 節兩個做法的步驟 |
| P3 | 讀者的 Registry 存來源 Registry 的裸指標；Kernel 不保存來源 World | R1a（DI-KS 37，使用者 11-23 核准） | 重設必須在換上的同一步 |
| P4 | 來源替換的做法在 R1b 比、交使用者；要改的條文先列 | 總宣告 6.1-4（已核准） | 第 5、10 節 |
| P5 | 錯誤只告訴外面發生了什麼；誤用不加守衛，由設計審查抓 | 使用者 11-20、11-21 | 第 3.2 節「以舊實體為鍵」的責任 |
| P6 | ECS 機制以 Bevy 為主參考；偏離寫衝突點與膠水量 | 使用者 11-23 | 第 4 節 |
| P7 | Engine 不把規則釘死：預設方便、不禁止別的做法 | 使用者 11-17 | 第 7 節的理由 3 |

## 1. 用遊戲講

PvP 的 Client 有兩個 World：**Client World**（收網路、跑預測、放角色和地圖的實體）與**渲染 World**（`extractFrom` 指向 Client World，把角色、地圖抄過來畫）。玩家斷線，網路 Module 發 `PeerDisconnected`；產品要重連，就換一個新的 Client World（net D-8：重連＝新的 Client World）。

- **(A) 讀者一起重建**：Client World 換新的時候，渲染 World 也跟著換新的。新的渲染 World 是空的：角色與地圖的貼圖、網格要重新上傳 GPU，HUD 的字形圖集重建，渲染這一側記著的任何東西都歸零。換一次就重來一次。
- **(B) 准許換來源**：渲染 World 留著，只把「它讀誰」從舊 Client World 改成新的。貼圖、網格、圖集都還在。渲染 World 若記了「以舊 Client 的實體編號為鍵」的東西（例：實體 7 的網格實例），要整批丟掉，因為新 Client World 的實體 7 可能是另一個東西——編排層在換上的同一步給渲染 World 一個「來源換了」的通知。

兩者都**不會**關掉視窗：視窗與 GPU 裝置在 Product 層（`sdl_window`、`sdl_gpu` 的 `Configure`），不屬於任何 World。

## 2. 現況（照程式）

- **擋住的地方**：`Product::ReplaceWorldSteps` 在丟掉任何東西之前斷言「沒有 World 以它為來源」（`engine/orchestration/src/Product.cpp:801-807`）。替換讀者已經可以：新的讀者 Registry 接來源目前的 Registry（`Product.cpp:893-896`）。
- **為什麼擋**：讀者的 `RegistryData::extractSource` 是來源 `RegistryData` 的裸指標（`Registry.cpp:686-690`），只准在第一次 `BuildPlans` 之前設（`GYO_ASSERT(!data_->finished)`）；`BuildNewFirst` 換上時舊的 Registry 銷毀（`Product.cpp:992-996`），指標就懸空。`RunExtract` 斷言給的來源 World 是由這個指標指的 Registry 建的（`Executor.cpp:185-186`）。
- **讀者的 Plan 綁了來源的什麼**：Extract 參數的存取在登記時解析成**來源的** ComponentId／ResourceId（`Registry.cpp:309-325`，`access.extractReads`），進指紋（`Fingerprint.cpp:139`）；來源沒建過 Plan 或沒登記那個型別是 D-E14／R-E4。執行時的句柄每次照型別鍵在給的來源 World 上查 id（`Params.hpp:1371`、`:1422`），不靠登記時的 id。
- **之後一輪的機制已有**：之後一輪寫暫存副本（`Registry.cpp:1305`），`BuildPlans` 成功才換進同一個表物件（`Registry.cpp:1336-1348`、`Adopt` 在 `:1263`），失敗丟掉、留在上一個版本；`ResolveSystems` 每一輪重算每個 System 的存取，Extract 的部分會重新對來源解析（`Registry.cpp:1215-1236`）。讀者的 World 下一次被用到時自己接上新版本，保留 `lastRun` 與 `Local`（DI-KS 26）。但 `Register` 開之後一輪時，只要有一個 World 宣告 `dynamicInsert = false` 就回 `DynamicInsertRefused`（`Registry.cpp:1294-1301`）。
- **GPU 這一側放在哪**：裝置與 pipeline 快取在 Product 層（`sdl_gpu` 的 `Configure` 建 `Service`，`plugins/sdl_gpu/src/SDLGPUPlugin.cpp:121-144`；pipeline 第一次用時建、存在裝置裡，`adapters/sdl/GPUDevice.cpp:201-204`）。**貼圖與網格是每個渲染 World 一份**：每次準備渲染 World 拿一個新號碼（`SDLGPUPlugin.cpp:158-160`），換了號碼的那一幀 `ForgetMeshes`／`ForgetTextures`（`SDLGPUPlugin.cpp:33-40`）。渲染 World 的 `RenderImages` 以 `ImageKey`（素材 id＋life，或作者持有的影像位址）為鍵（`engine/modules/render/include/engine/render/Pipeline2d.hpp:77-86`、`:102-121`）；素材的 id 與 life 屬 Product 一份的素材庫，不是來源 World 的（`world_asset/AssetView.hpp:20-32`），所以**換來源不會讓這張表的鍵指錯東西**。
- **以來源實體為鍵、跨幀留著的東西**：今天沒有。R2b 的相機、sprite、shape、3D item 都是每幀重抽的暫時資料（`Extracted2dItems`、`ExtractedCameras` 等），實體 id 只當排序的平手鍵。總宣告第 6.1 節第 8 點的保留式對應（Bevy 的 `RenderMeshInstances`）是之後的事（R6）。

**更正總宣告的一句**：總宣告第 6.1 節第 4 點把 DI-KS 13 與 D-E14、DI-KS 26、RC-O13 列成「兩者要改的條文」。照上面讀程式的結果，**(A) 不改任何 Kernel 條文**（每個讀者 Registry 一輩子只有一個來源，指標不懸空），只改編排層；Kernel 的三條只有 (B) 要改。第 5 節逐條列。

## 3. 兩個做法

### 3.1 (A) 讀者一起重建

替換一個 World 時，**以它為來源的讀者，連同讀者的讀者（整條鏈的下游）一起替換**。

- `BuildNewFirst`：照 World 的描述順序，先建新的被替換者、再依序建新的讀者（每個新讀者接新來源的 Registry）；每個都走 O-2 的 `Build`～`Check`、O-10 的帶過去、準備函式；被替換者跑 `beforeSwap`（讀者沒有自己的 `beforeSwap`，`ReplaceRequest` 不變）。全部成功才換上：舊的照反序銷毀（讀者先、來源後，守 DI-KS 37 的「讀者不比來源晚銷毀」），新的就位。任何一步失敗，新的全丟、舊的全留。
- `DropOldFirst`：先照反序銷毀舊的讀者與被替換者，再照順序建；失敗時那些位置都是空的，Product 結束（O-11）。
- 讀者的 `extra`（準備函式）沒有；讀者要沿用的東西用 `CarryOver<T>`（O-10）。
- 遊戲初始化、生成世界（O-14）照替換的規則：新讀者也從生成中開始。

**代價**：每次換來源，下游的每個讀者重新登記（`BuildPlans`）、重跑 `Build` 與準備；`sdl_gpu` 給新渲染 World 新號碼，下一幀丟掉所有貼圖與網格、之後逐一重新上傳；渲染這一側的狀態全部歸零（`RenderImages`、UI 與 ImGui 的圖集、PvP 第 4 段 Frame World 的內插緩衝）。pipeline 與裝置不重建（R2a 已放在 Product 層）。重傳要多久**沒有量**。

### 3.2 (B) 准許換來源

替換一個 World 時，**直接以它為來源的讀者留著，在換上的同一步把來源改成新的 Registry**；讀者的讀者不受影響（它們的來源——這個讀者——沒換）。

**Kernel**：新增 `Registry::ReplaceExtractSource(const Registry& source) -> Result<void, StartupReport>`（名字照 `ReplaceWorld`；之後一輪的一種，一次做完）：

1. 在第一次 `BuildPlans` 成功之後才能呼叫（之前用 `SetExtractSource`）。斷言：在 `build` 回呼裡、有用這份 Registry 建的 World 在 Run 裡、第一輪失敗過、已有開著的之後一輪（和 `Register`／`BuildPlans` 的斷言同一組，DI-KS 26）。
2. 開暫存副本，把來源改成 `source`，照之後一輪的 `BuildPlans` 重算（Extract 存取重新對新來源解析；新來源沒建過 Plan 是 D-E14，沒登記讀者要的型別是 R-E4）。成功：版本加一、換進同一個表物件，World 下一次被用到時接上（保留 `lastRun`、`Local`）。失敗：丟掉，Registry 與 World 留在舊來源與舊版本。
3. **不受 `dynamicInsert` 擋**：它不登記也不拿掉 System，只改「讀誰」；新來源照同一份描述登記時，解析出的 id 和舊的一樣，Plan 只差版本號。
4. 沒有新的錯誤碼。

**編排層**（`ReplaceWorldSteps`）：

1. 拿掉 R1a 的斷言。
2. `BuildNewFirst`：`beforeSwap` 成功之後、銷毀舊的之前，對每個直接讀者呼叫 `ReplaceExtractSource(新 Registry)`。全部成功才換上；第 k 個失敗時，前 k−1 個改回舊 Registry（舊的沒動過、它們原本就對它建過 Plan，必然成功；不成功是斷言），新 World 丟掉，回 E2 `ReplaceRegistrationFailed`（報告帶讀者的 `StartupReport`），舊的照跑。
3. `DropOldFirst`：舊的先銷毀；讀者的來源指標在換上之前懸空，期間不碰讀者（不跑、不登記）。新的建好、`beforeSwap` 成功後同樣改來源；失敗時位置是空的，Product 結束（O-11），讀者不再跑。
4. **通知**：編排層在每個有 `extractFrom` 的 World 登記一個 Resource（暫名 `ExtractSourceSwaps { std::uint64_t count; }`，owner `orchestration`，照 O-14 的 `WorldGeneration` 的前例），改來源成功、換上之後把讀者的 `count` 加一（World 不在 Run 裡、直接寫，和 `WorldGeneration` 同一條路）。讀者這一側凡是**以來源實體為鍵、跨幀留著的東西**，由擁有它的 System 讀 `Res<ExtractSourceSwaps>` 的 `Changed()`，在用到它之前整批丟掉。這是照 Bevy 同步的形——觀察者記下「發生了什麼」，下一次 extract 開頭處理（`bevy_extract/src/sync_world.rs:93-108` 記到 `PendingSyncEntity`，extract 前的 `entity_sync_system` 處理）。今天沒有這樣的表（第 2 節），這個通知的第一個使用者會是 R6。
5. 替換讀者自己照舊（新 Registry 接來源目前的 Registry），不加通知（新 World 本來就是空的）。

**代價**：Kernel 公開面多一個函式、三條條文改（第 5 節）；編排層多一段「全部改來源、失敗改回」；作者有一條要記得的責任：以來源實體為鍵的東西要聽通知（誤用不加守衛，交設計審查，P5）。換一次的成本是每個直接讀者一輪 `BuildPlans`（不加 System，只重算），貼圖、網格、圖集都留著。

### 3.3 比較

| | (A) 讀者一起重建 | (B) 准許換來源 |
|---|---|---|
| 改 Kernel | 不改 | 加 `ReplaceExtractSource`；DI-KS 13、26、37 與 D-E14 的守衛 |
| 改編排層 | O-8 替換擴到整條下游；O-12 一句 | O-8 改來源與失敗改回；O-12 一句；通知的 Resource |
| 換一次的成本 | 下游每個 World 重登記、重準備；貼圖網格全部重傳；渲染側狀態歸零 | 直接讀者各一輪 `BuildPlans`；GPU 與渲染側狀態留著 |
| 鏈（Sim→Frame→Render） | 換 Sim 連 Frame、Render 一起換 | 換 Sim 只有 Frame 改來源；Render 不動 |
| 作者要記得的 | 沒有新的（新 World 是空的） | 以來源實體為鍵的表要聽通知 |
| 另一種行為還做得到嗎 | 讀者的狀態留不住 | 要讀者重建，作者在換來源之後自己 `ReplaceWorld` 讀者 |
| RC-O13 | 觸發條件就是它的代價，條文不改、成為現行的風險 | 改寫觸發條件（第 5 節） |
| 估時（推定） | 10～16 h | 14～22 h |

## 4. 和別家的對照

**Bevy（主參考）**：`extract(main_world, sub_world)` 每幀把 main world 暫時放進渲染 world 的 `MainWorld` Resource、跑 `ExtractSchedule`、再放回（`bevy_extract/src/extract_plugin.rs:136-147`）；渲染 world（`SubApp`）活得比 main world 的任何內容變化都久。但 `Extract<P>` 的狀態在 `init_state` 時對當時的 main world 建 `SystemState`（`extract_param.rs:76-81`），之後每次 `get` 都檢查 world id，不同就 panic（`bevy_ecs/src/system/function_system.rs:410-415`）。也就是說 **Bevy 不支援換掉 main world 這個物件**；它的「讀檔」是在同一個 main world 裡 despawn 再 spawn，渲染側經同步（`MainEntity`／`RenderEntity`、`PendingSyncEntity`，`sync_world.rs:93-108`）拿到移除通知、丟掉對應實體。結果上是「渲染側留著，以來源實體為鍵的東西隨通知丟掉」＝(B) 的形。GYO 和 Bevy 的衝突點在前提：O-8 的替換給**新的 Registry 與新的 World**（Bevy 沒有），所以要有「改讀誰」這一步；Bevy 的同步寫回來源（DI-KS 13 不准），GYO 的通知由編排層寫進讀者、不碰來源。膠水：Kernel 一個函式、編排層一個 Resource。

**Godot**：`RenderingServer` 是單例，活過所有場景。`change_scene_to_packed` 先記下舊場景、下一幀 `_flush_scene_change` 把它 `memdelete`（`scene/main/scene_tree.cpp:1729-1752`、`:1691-1696`）；每個 `VisualInstance3D` 建構時向 server 要一個 instance RID、解構時還回去（`scene/3d/visual_instance_3d.cpp:210-221`）。貼圖、網格是引用計數的 Resource，新場景還用得到就不重傳。結果：**渲染的一側不重建，場景這一側的「實例」逐一作廢**＝(B) 的形（Godot 是逐一還、GYO 是整批通知）。

**Unity Netcode for Entities**：重連是建一個新的 Client World（`ClientServerBootstrap.CreateClientWorld`，`Runtime/ClientServerWorld/ClientServerBootstrap.cs:339`）；畫面的 System 在同一個 World 的 `PresentationSystemGroup` 裡，所以跟著 World 一起重建＝(A) 的形。它便宜的原因是 GPU 素材（Mesh、Material、Texture）是 World 外的 UnityEngine 物件，不跟著 World 走。GYO 今天貼圖與網格是每個渲染 World 一份（第 2 節），照 (A) 做就要重傳；要像 Unity 一樣便宜，得先把 `sdl_gpu` 的貼圖網格改成 Product 一份、以素材鍵共用——那是 R2a／`sdl_gpu` 的契約，不在 R1b。

## 5. 各自要改的條文（草案；最終文字合併前核准）

### 5.1 (A)

- **O-8**：末句「R1b 定做法之前，替換有讀者的來源……是斷言」改成：「替換一個 World 時，以它為來源的讀者與讀者的讀者一起替換：`BuildNewFirst` 照描述順序全部建好、被替換者的 `beforeSwap` 成功才照反序銷毀舊的；`DropOldFirst` 先照反序銷毀；讀者沒有 `extra` 與 `beforeSwap`，要沿用的用 `CarryOver<T>`。」
- **O-12** Extract 那一項：「來源被替換後讀者不得讀舊來源（O-8）」補「（讀者跟著替換）」。
- **RC-O13**：條文不改；照 (A) 它從預見變成現行的代價，HANDOFF 記一筆。
- Kernel：不改。DI-KS 38～40、RC 29 不用。

### 5.2 (B)

- **DI-KS 13** 第二段「來源的 Registry 至少建好過一次 Plan（D-E14；……）」後加：「讀者的 Registry 可以在之後換來源（DI-KS 38），新來源同樣要建好過 Plan。」守衛欄加 DI-KS 38 的案。
- **D-E14 的守衛**：加「`ReplaceExtractSource` 給沒建過 Plan 的來源是 D-E14，Registry 留在舊來源」一案。
- **DI-KS 26**：「之後一輪只准……」那句後加：「換 extract 來源（DI-KS 38）也是之後一輪的一種，不受 `dynamicInsert` 擋（不登記、不拿掉 System）。」斷言那一句把 `ReplaceExtractSource` 列進「那三個」（成為四個）。
- **DI-KS 37**：呼叫的人的規則「讀者的 Registry 不比來源的晚銷毀」改成「讀者的 Registry 在登記、建 Plan、`RunExtract` 期間，它**目前的**來源要活著；換來源成功之後舊來源才可以走；`DropOldFirst` 的替換期間來源先走，期間不碰讀者」。
- **DI-KS 38（新增）**：`Registry::ReplaceExtractSource` 的契約（第 3.2 節 Kernel 1～4）。守衛：`ExtractTests.cpp` 的「DI-KS 38: …」各案（換了之後讀新來源；失敗留在舊來源與舊版本；不受 `dynamicInsert` 擋；第一次 `BuildPlans` 之前呼叫是斷言；`lastRun` 與 `Local` 留著）。
- **DI-KS 39、40**：不用（留給之後）。
- **RC 29（新增）**：換來源的一輪會因 D-E14／R-E4 以外的原因失敗，或讀者需要在換來源時讀到舊來源（兩個來源同時活著的比對）——重評「改回」的必然成功與 DI-KS 38。
- **O-8**：末句改成：「替換一個 World 時，直接讀它的 World 在換上的同一步改讀新的 Registry（DI-KS 38）：`BuildNewFirst` 在 `beforeSwap` 成功之後、銷毀舊的之前全部改，有一個失敗就把改了的改回、丟掉新 World、回 E2；`DropOldFirst` 在新的就緒後改，失敗時 Product 結束。改成功後讀者的 `ExtractSourceSwaps` 加一（O-12）。」
- **O-12** Extract 那一項加：「讀者有以來源實體為鍵、跨幀留著的東西，聽 `ExtractSourceSwaps` 的變化整批丟掉；編排層只發通知、不認得那些東西。」
- **RC-O13**：改成「換來源（O-8）之後讀者有以舊來源為鍵、經通知也丟不乾淨的狀態，或改來源的那一輪成為替換時間的主要部分」。

## 6. 測試（驗收的骨幹）與突變

**兩個做法共用**（觀察面相同，選定前可以先寫骨架；`tests/common/orchestration/ExtractTests.cpp`）：

- 來源替換後讀新的：來源 World 有 Resource／元件值 1；讀者抽成自己的值。`ReplaceWorld(來源)` 讓新來源的值是 2；下一個 `RunFrame` 讀者抽到 2。`BuildNewFirst`、`DropOldFirst` 各一案。
- `BuildNewFirst` 的重設時點：新來源的 `beforeSwap` 失敗 → 讀者照讀舊來源（值 1），沒有懸空（ASan 下跑得過）；成功 → 換上之後第一次 `RunFrame` 讀到 2。
- 鏈中間的被替換：Sim→Frame→Render，換 Frame，Render 讀新 Frame 的值。
- 一個來源兩個讀者都讀到新的。
- `DropOldFirst` 失敗：Product 結束，之後入口回 `WorldMissing`。

**只有 (A)**：讀者是新的 World（讀者的一個計數 Resource 歸零）；`BuildNewFirst` 讀者建不起來時來源與讀者都留舊的；銷毀照反序（`orchestration.extract`「the reader World is destroyed before its source」延伸到替換）。

**只有 (B)**：讀者的計數 Resource 留著；換來源時對照整批作廢——測試用的讀者 System 把來源實體 id 記在 `Local` 的表裡，新來源同一個 id 是不同的值，聽 `ExtractSourceSwaps` 清表後讀到新值、不聽的版本讀到舊對照（突變：不加 `count`）；兩個讀者第二個改來源失敗時第一個改回（新來源少登記一個型別）；Kernel `ExtractTests.cpp` 的 DI-KS 38 各案；`dynamicInsert = false` 的讀者照樣改得了。

突變（只跑相關的）：(A) `or-rx-` 前綴：不換讀者、銷毀順序反過來、失敗不丟新讀者；(B) `or-rx-`：不改來源、不加通知、失敗不改回；`ks-rx-`：不重新解析、失敗不丟暫存、被 `dynamicInsert` 擋。

## 7. 建議：(B) 准許換來源

1. **換一次的代價小**：GPU 的貼圖與網格、UI 與 ImGui 的圖集、PvP 第 4 段 Frame World 的內插都留著。(A) 每次斷線重連、每次大廳↔遊戲都要重傳全部素材（量沒量過：沒有，第 3.1 節）。
2. **鏈自然**：只動直接讀者；PvP 第 4 段的 Sim→Frame→Render 換 Sim 時 Render 完全不動。(A) 要整條鏈一起建、一起失敗、反序銷毀。
3. **不禁止另一種**（P7）：選 (B) 時，作者要讀者重建就在換來源後自己 `ReplaceWorld(讀者)`；選 (A) 時讀者的狀態沒有辦法留。
4. **和 Bevy、Godot 的結果一致**：渲染側活過模擬內容的變化，以模擬實體為鍵的東西隨通知丟掉（第 4 節）。
5. **Kernel 的改動在既有機制上**：之後一輪的暫存副本、`Adopt`、`ResolveSystems` 重新解析都已存在；新函式只是「開一輪、改指標、建」。

(B) 的風險與接手：作者忘了聽通知、用舊來源的實體 id 讀新來源——不加守衛（P5），由設計審查看；R6 做保留式對應時把「聽通知」寫進那個 Module 的契約。

## 8. PvP 怎麼用它來避免「切大廳時關窗再開」

今天（#330）大廳與 Client 是兩個 Product，切換時整個 Product 重建，視窗跟著關掉再開。原因有兩個：(1) 沒有 R1b 時，有渲染 World 讀它的 World 不能替換；(2) 連線目標在 `NetProductOptions::server`（`engine/modules/net/include/engine/net/NetService.hpp:26-40`），Product 建立時就定了（net D-8）。

R1b（兩個做法都行）解掉 (1)。PvP 的做法：

1. 一個 Product，兩個 World：**前台 World**（大廳與 Client 用同一份描述；大廳是它的一個狀態，或產品自己的 `build` 依 O-15 共用處裡的「現在要哪一種」登記不同的東西）＋**渲染 World**（`extractFrom` 指向前台 World，帶 `ui_render`、`sprite_render`、`mesh_render` 等）。`sdl_window`、`sdl_gpu` 在 Product 層建視窗與裝置，整個行程只有一個視窗。
2. 大廳送出位址 → runner 在兩次入口之間 `ReplaceWorld("front", {extra: 設定這次的連線})` → 新的前台 World 準備時開握手。斷線（`PeerDisconnected`）→ 同樣 `ReplaceWorld` 回大廳的狀態。視窗不關。
3. 照 (B)：渲染 World 留著，大廳的字形圖集、遊戲的貼圖網格第一次上傳後一直在；照 (A)：每次切換渲染 World 重建、重傳。

**(2) 不在 R1b**：連線目標要能在每次換前台 World 時給，是 net Module 的事（例：`NetService` 給 runner 一個「下一次連這裡」的函式，O-12 的「runner 直接讀寫／呼叫 Module 給的函式」；或連線目標改進 `NetWorldOptions`／準備函式）。這要 PvP 卡或 net 的卡定，R1b 回報時一起轉主會話。

## 9. 範圍、協調、分工與估時

- **範圍外**：保留式的實體對應（R6）；`sdl_gpu` 的貼圖網格改成 Product 一份（第 4 節 Unity 那段）；net 的連線目標（第 8 節 (2)）；多執行緒下的替換（O-13，PvP 第 4 段定）。
- **協調**：PvP 卡的 #331（`Product::Lend`、模擬執行緒）改編排層的 `Product`。本批改 `ReplaceWorldSteps` 與 Extract 那一段；碰到同一批檔案時以先合的為準，後合的 merge 跟上（不 rebase）。R1b 的替換在 `Lend` 出去的 World 上怎麼辦（替換時 World 必須在 Product 手上）照 O-13 (2) 由 #331 定，本批只寫「替換時 World 都在 Product 手上」的前提。
- **分工**：作者 Opus／high（本卡）；宣告與實作各一次設計審查，Opus／high 子 Agent（作者與審查者同模型：Fable 暫停）。
- **估時**（推定）：(A) 10～16 h；(B) 14～22 h（Kernel 函式＋案 4～6 h、編排層改來源與改回 4～6 h、通知與作廢測試 2～4 h、文件與審查 4～6 h）。

## 10. 要使用者定的

### Q1：來源被替換時，讀它的 World 怎麼辦？

- **(A) 讀者一起重建**：不改 Kernel；每次替換下游全部重建，GPU 貼圖網格重傳、渲染側狀態歸零。
- **(B) 准許換來源**（**建議**）：Kernel 加 `ReplaceExtractSource`，改 DI-KS 13、26、37 與 D-E14 的守衛，新增 DI-KS 38、RC 29；編排層在換上的同一步改來源、發通知；渲染側留著，以來源實體為鍵的東西由擁有者聽通知丟掉。

其餘（名字 `ReplaceExtractSource`、`ExtractSourceSwaps`，失敗時改回的做法，通知用 Resource 的 `Changed` 而不是 `Message`）卡片照第 3.2 節判，可以推翻。

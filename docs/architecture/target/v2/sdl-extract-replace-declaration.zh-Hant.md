# 事前宣告：Extract 的來源被替換（`v8-arch-gyo2-sdl-r1b`，SDL 線子批 R1b）

更新：2026-11-25（修訂 1）。Owner：Kernel＋編排層（GYO2 主會話開的任務卡，SDL 線子批 R1b）。**狀態：動工前核准（2026-11-25）**——使用者在本卡「選 B，開始」（之前問「別家如何？」，卡片回了第 4 節的對照）：Q1＝(B) 准許換來源；其餘照第 3.2 節卡片的判斷。最終文字在合併前核准。

經過：
- 修訂 0（d4529577）：照卡片寫。
- 實作（2026-11-25）：照修訂 1 第 3.2 節。Kernel 的三個函式定名 `ReplaceExtractSource`（開之後一輪、改來源；拒絕時回 `KernelError`，同 `Register`）、`StagePlans`（建好留著，同 `BuildPlans` 的錯誤）、`CommitPlans`（只搬移）——第 3.2 節的「stage 一次做開輪與建」拆成開輪與建兩步，是為了沿用 `Register`／`BuildPlans` 的兩種錯誤型別，不新增錯誤碼；`BuildPlans` 的之後一輪改成 `StagePlans`＋`CommitPlans`。
- 修訂 1：修訂 0 送設計審查（**Opus／high，作者與審查者同為 Opus**：Fable 暫停）：blocker 1、should 5、nit 4，全部採用（對照在第 11 節）。審查者沒有建置、沒有跑測試；第 2 節的行號逐一核對過。最大的改動：(B) 的換來源改成「全部暫存、全部成功才一起換進」（B-1，拿掉「失敗改回」）；不繞過 `dynamicInsert = false`（S-1）；列第三個做法 (C) 延後綁定與不選的理由（S-5）。

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
- **(B) 准許換來源**：渲染 World 留著，只把「它讀誰」從舊 Client World 改成新的。和 Client 的實體無關的東西（以素材為鍵的貼圖、網格、字形圖集）都還在。渲染 World 若記了「以舊 Client 的實體編號為鍵」的東西（例：實體 7 的網格實例），要整批丟掉，因為新 Client World 的實體 7 可能是另一個東西——編排層在換上的同一步給渲染 World 一個「來源換了」的通知。

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

**代價**：每次換來源，下游的每個讀者重新登記（`BuildPlans`）、重跑 `Build` 與準備；`sdl_gpu` 給新渲染 World 新號碼，下一幀丟掉所有貼圖與網格、之後逐一重新上傳；渲染這一側的狀態全部歸零（`RenderImages`、UI 與 ImGui 的圖集）。pipeline 與裝置不重建（R2a 已放在 Product 層）。重傳要多久**沒有量**。以來源實體為鍵的狀態（例：PvP 第 4 段 Frame World 以 Sim 實體為鍵的內插緩衝）不論 (A)、(B) 都要丟，不算 (A) 多出的代價。

### 3.2 (B) 准許換來源

替換一個 World 時，**直接以它為來源的讀者留著，在換上的同一步把來源改成新的 Registry**；讀者的讀者不受影響（它們的來源——這個讀者——沒換）。

**Kernel**：換來源分兩步（修訂 1 依審查 B-1：一步做完的版本在多個讀者、例外的路上會留下懸空指標）。名字暫定，實作時照同組 API 定：

1. `Registry::StageExtractSource(const Registry& source)`：在第一次 `BuildPlans` 成功之後才能呼叫（之前用 `SetExtractSource`）。開之後一輪的暫存副本，把來源改成 `source`，照之後一輪的 `BuildPlans` 在副本上重算（`ResolveSystems` 讓 Extract 存取重新對新來源解析；新來源沒建過 Plan 是 D-E14，沒登記讀者要的型別是 R-E4），**建好但不換進**。失敗：丟掉副本，回建 Plan 的 `StartupReport`（同 `BuildPlans`）。有一個 World 宣告 `dynamicInsert = false` 時回 `DynamicInsertRefused`（同 `Register`，DI-KS 26；修訂 1 依審查 S-1 不繞過：新來源少了之前動態插入的登記、或產品依狀態登記不同的東西時，解析出的 id 與指紋都會變，宣告不准的 World 不能讓它變）。
2. `Registry::CommitStaged() noexcept`：把暫存的一輪換進同一個表物件（既有的 `Adopt`，只搬移；實作時用 `static_assert` 確認 `RegistryData` 的移動不丟例外），版本加一；World 下一次被用到時接上（保留 `lastRun`、`Local`，DI-KS 26）。
3. 丟掉暫存用既有的 `DiscardRegistrations()`（不丟例外）。
4. 斷言：在 `build` 回呼裡、有用這份 Registry 建的 World 在 Run 裡、第一輪失敗過、已有開著或暫存的一輪時 stage；沒有暫存時 commit；有暫存時 `Register`／`BuildPlans`。暫存期間 World 照舊讀舊來源。沒有新的錯誤碼。

**編排層**（`ReplaceWorldSteps`）：

1. 拿掉 R1a 的斷言。
2. `BuildNewFirst`：`beforeSwap` 成功之後，對每個直接讀者 `StageExtractSource(新 Registry)`。任一失敗或丟例外：所有已暫存的讀者 `DiscardRegistrations()`，新 World 丟掉，回 E2 `ReplaceRegistrationFailed`（報告帶讀者的錯誤）或讓例外往外，舊的照跑。全部成功：換上的那一步（不丟例外的區段）裡每個讀者 `CommitStaged()`、銷毀舊的、裝上新的。沒有「改回」。
3. `DropOldFirst`：舊的先銷毀；讀者的來源指標在換上之前懸空，期間不碰讀者（`~Registry`、`~World` 都不碰來源指標，`Registry.cpp:684`、`World.cpp:152-157`）。新的就緒、`beforeSwap` 成功後同樣先全部暫存、再一起換進；失敗時位置是空的，Product 結束（O-11），讀者不再跑。
4. **通知**：編排層在每個有 `extractFrom` 的 World 登記一個 Resource（暫名 `ExtractSourceSwaps { std::uint64_t count; }`，owner `orchestration`，照 O-14 的 `WorldGeneration` 的前例），換進之後把讀者的 `count` 加一（World 不在 Run 裡、直接寫，和 `WorldGeneration` 同一條路，不配置）。讀者這一側凡是**以來源實體為鍵、跨幀留著的東西**，在用到它之前整批丟掉：`Changed()` 是每個 System 掛載各自的 `lastRun`，所以用到那張表的每個 System 都自己看，或由擁有者在最早用到它的那張 schedule 清掉（審查 N-4）。這是照 Bevy 同步的形——觀察者記下「發生了什麼」，下一次 extract 開頭處理（`bevy_extract/src/sync_world.rs:93-108` 記到 `PendingSyncEntity`，`:221` 的 `entity_sync_system` 處理）。今天沒有這樣的表（第 2 節），通知的第一個使用者會是 R6。
5. 替換讀者自己照舊（新 Registry 接來源目前的 Registry），不加通知（新 World 本來就是空的）。

**通知為什麼在編排層、不在 Kernel**（審查 S-3）：「哪個 World 被換掉」是 O-8 的概念，決定換的是編排層（或直接用 Kernel 的 host）；Kernel 只提供「改讀誰」的原語，和它不保存來源 World（DI-KS 37）一致。直接用 Kernel 的 host 換來源時自己呼叫了 stage／commit，知道自己換了，要通知就自己寫。Bevy 的同步也不在 `bevy_ecs`，在它上面的 `bevy_extract`。代價：Kernel 測試與之後的工具若要同樣的通知，要各寫一行；兩個以上的 host 這樣寫時，重評搬進 Kernel（記成 RC 29 的後半）。

**代價**：Kernel 公開面多兩個函式、三條條文改（第 5 節）；編排層多一段「全部暫存、一起換進」；作者有一條要記得的責任：以來源實體為鍵的東西要聽通知（誤用不加守衛，交設計審查，P5）；讀者宣告 `dynamicInsert = false` 時不能換來源（要換就照 (A) 自己 `ReplaceWorld` 讀者）。換一次的成本是每個直接讀者一輪 `BuildPlans`（不加 System，只重算），以素材為鍵的貼圖、網格、圖集都留著。

### 3.3 第三個做法 (C) 延後綁定（列出、不建議）

審查 S-5 提的：Kernel 不存來源的 Registry，到 `RunExtract` 時才對給的來源解析、驗證（Bevy 的 `Extract` 也是第一次跑時才對 MainWorld 建狀態，`extract_param.rs:76-81`）。GYO 的句柄本來就每次照型別鍵查 id（`Params.hpp:1371`、`:1422`），存下來的來源指標只用在啟動時的驗證（D-E14、R-E4）與指紋。好處：DI-KS 37 的壽命規則、新函式、懸空的期間全都不要。**不建議的理由**：(1) R-E4／D-E14 從「啟動時一次收齊」（O-5）變成執行時才撞到的錯；(2) 指紋看不到讀者讀了來源的什麼；(3) 推翻 R1a 已核准的 DI-KS 37（使用者 11-23）。使用者若偏好「規則最少」，(C) 是可選的，那時 R1a 的條文一起改。

### 3.4 比較

| | (A) 讀者一起重建 | (B) 准許換來源 |
|---|---|---|
| 改 Kernel | 不改 | 加 stage／commit 兩個函式；DI-KS 13、26、37 與 D-E14 的守衛；新增 DI-KS 38、RC 29 |
| 改編排層 | O-8 替換擴到整條下游；O-12 一句 | O-8 全部暫存、一起換進；O-12、O-15 各一句；通知的 Resource |
| 換一次的成本 | 下游每個 World 重登記、重準備；貼圖網格全部重傳；渲染側狀態歸零 | 直接讀者各一輪 `BuildPlans`；以素材為鍵的 GPU 資源與圖集留著 |
| 以來源實體為鍵的狀態 | 隨 World 丟掉 | 擁有者聽通知丟掉（兩者結果一樣） |
| 鏈（Sim→Frame→Render） | 換 Sim 連 Frame、Render 一起重建 | 換 Sim 只有 Frame 改來源；Render 的 Registry 不動（它讀到的 Frame 內容照樣跟著變） |
| 作者要記得的 | 沒有新的（新 World 是空的） | 以來源實體為鍵的表要聽通知 |
| 另一種行為還做得到嗎 | 讀者的狀態留不住 | 要讀者重建，作者在換來源之後自己 `ReplaceWorld` 讀者 |
| `dynamicInsert = false` 的讀者 | 照樣換 | 不能換來源（回 E2），要換就自己替換讀者 |
| RC-O13 | 觸發條件就是它的代價，條文不改、成為現行的風險 | 改寫觸發條件（第 5 節） |
| 估時（推定） | 10～16 h | 16～24 h |

## 4. 和別家的對照

**Bevy（主參考）**：`extract(main_world, sub_world)` 每幀把 main world 暫時放進渲染 world 的 `MainWorld` Resource、跑 `ExtractSchedule`、再放回（`bevy_extract/src/extract_plugin.rs:136-147`）；渲染 world（`SubApp`）活得比 main world 的任何內容變化都久。但 `Extract<P>` 的狀態在 `init_state` 時對當時的 main world 建 `SystemState`（`extract_param.rs:76-81`），之後每次 `get` 都檢查 world id，不同就 panic（`bevy_ecs/src/system/function_system.rs:410-415`）。也就是說 **Bevy 不支援換掉 main world 這個物件**；它的「讀檔」是在同一個 main world 裡 despawn 再 spawn，渲染側經同步（來源側的 `SubEntity<L>`、渲染側的 `MainEntity`、`PendingSyncEntity`，`sync_world.rs:130`、`:159`、`:93-108`）拿到移除通知、丟掉對應實體。結果上是「渲染側留著，以來源實體為鍵的東西隨通知丟掉」＝(B) 的形。GYO 和 Bevy 的衝突點在前提：O-8 的替換給**新的 Registry 與新的 World**（Bevy 沒有），所以要有「改讀誰」這一步；Bevy 的同步寫回來源（DI-KS 13 不准），GYO 的通知由編排層寫進讀者、不碰來源。膠水：Kernel 兩個函式（stage／commit）、編排層一個 Resource。

**Godot**：`RenderingServer` 是單例，活過所有場景。`change_scene_to_packed` 先記下舊場景、下一幀 `_flush_scene_change` 把它 `memdelete`（`scene/main/scene_tree.cpp:1729-1752`、`:1691-1696`）；每個 `VisualInstance3D` 建構時向 server 要一個 instance RID、解構時還回去（`scene/3d/visual_instance_3d.cpp:210-221`）。貼圖、網格是引用計數的 Resource，新場景還用得到就不重傳。結果：**渲染的一側不重建，場景這一側的「實例」逐一作廢**＝(B) 的形（Godot 是逐一還、GYO 是整批通知）。

**Unity Netcode for Entities**：重連是建一個新的 Client World（`ClientServerBootstrap.CreateClientWorld`，`Runtime/ClientServerWorld/ClientServerBootstrap.cs:339`）；畫面的 System 在同一個 World 的 `PresentationSystemGroup` 裡，所以跟著 World 一起重建＝(A) 的形。這只是其中一種用法：Unity 也能在同一個 World 裡重新連線（`NetworkStreamDriver.Connect`，`Runtime/Connection/NetworkStreamDriver.cs:259`），那種用法 World 與畫面都留著，比較接近 (B)。它便宜的原因是 GPU 素材（Mesh、Material、Texture）是 World 外的 UnityEngine 物件，不跟著 World 走。GYO 今天貼圖與網格是每個渲染 World 一份（第 2 節），照 (A) 做就要重傳；要像 Unity 一樣便宜，得先把 `sdl_gpu` 的貼圖網格改成 Product 一份、以素材鍵共用——那是 R2a／`sdl_gpu` 的契約，不在 R1b。

## 5. 各自要改的條文（草案；最終文字合併前核准）

### 5.1 (A)

- **O-8**：末句「R1b 定做法之前，替換有讀者的來源……是斷言」改成：「替換一個 World 時，以它為來源的讀者與讀者的讀者一起替換：`BuildNewFirst` 照描述順序全部建好、被替換者的 `beforeSwap` 成功才照反序銷毀舊的；`DropOldFirst` 先照反序銷毀；讀者沒有 `extra` 與 `beforeSwap`，要沿用的用 `CarryOver<T>`。」
- **O-12** Extract 那一項：「來源被替換後讀者不得讀舊來源（O-8）」補「（讀者跟著替換）」。
- **RC-O13**：條文不改；照 (A) 它從預見變成現行的代價，HANDOFF 記一筆。
- Kernel：不改。DI-KS 38～40、RC 29 不用。

### 5.2 (B)

- **DI-KS 13** 第二段「來源的 Registry 至少建好過一次 Plan（D-E14；……）」後加：「讀者的 Registry 可以在之後換來源（DI-KS 38），新來源同樣要建好過 Plan。」守衛欄加 DI-KS 38 的案。
- **D-E14 的守衛**：加「stage 給沒建過 Plan 的來源是 D-E14，Registry 留在舊來源」一案。
- **DI-KS 26**：「之後一輪只准……」那句後加：「換 extract 來源（DI-KS 38）也是之後一輪的一種，照樣受 `dynamicInsert` 擋；它先暫存、再換進。」斷言那一句把 stage／commit 列進去，並加「有暫存的一輪時 `Register`／`BuildPlans`」。
- **DI-KS 37**：呼叫的人的規則「讀者的 Registry 不比來源的晚銷毀」改成「讀者的 Registry 在登記、建 Plan、`RunExtract` 期間，它**目前的**來源要活著；換進新來源之後舊來源才可以走；`DropOldFirst` 的替換期間來源先走，期間不碰讀者」。
- **DI-KS 38（新增）**：換 extract 來源的契約（第 3.2 節 Kernel 1～4）。守衛：Kernel `ExtractTests.cpp` 的「DI-KS 38: …」各案（換進之後讀新來源；stage 失敗或丟掉時留在舊來源與舊版本；暫存期間照讀舊來源；`dynamicInsert = false` 時拒絕；第一次 `BuildPlans` 之前呼叫是斷言；`lastRun` 與 `Local` 留著）。
- **DI-KS 39、40**：不用（留給之後）。
- **RC 29（新增）**：(a) 讀者需要在換來源時同時讀到舊來源與新來源（兩個來源的比對）；(b) 兩個以上直接用 Kernel 的 host 各自寫「換了來源」的通知——重評 DI-KS 38 與把通知搬進 Kernel。
- **O-8**：末句改成：「替換一個 World 時，直接讀它的 World 在換上的同一步改讀新的 Registry（DI-KS 38）：新 World 就緒（`BuildNewFirst` 在 `beforeSwap` 成功之後、銷毀舊的之前；`DropOldFirst` 在新的就緒後）時對每個直接讀者暫存，全部成功才在換上的那一步一起換進；任一失敗丟掉所有暫存，`BuildNewFirst` 回 E2、舊的照跑，`DropOldFirst` 的 Product 結束。換進後讀者的 `ExtractSourceSwaps` 加一（O-12）。」
- **O-12** Extract 那一項加：「讀者有以來源實體為鍵、跨幀留著的東西，聽 `ExtractSourceSwaps` 的變化整批丟掉；編排層只發通知、不認得那些東西。」
- **O-15**「誰比誰活得久」裡「讀者的 Registry 也先於來源的 Registry」（審查 N-1）補：「`DropOldFirst` 的替換期間與失敗之後例外：舊來源先走，讀者不再被用到（DI-KS 37）。」
- **RC-O13**：改成「換來源（O-8）之後讀者有以舊來源為鍵、經通知也丟不乾淨的狀態，或換來源的那一輪成為替換時間的主要部分」。

## 6. 測試（驗收的骨幹）與突變

**兩個做法共用**（觀察面相同，選定前可以先寫骨架；`tests/common/orchestration/ExtractTests.cpp`）：

- 來源替換後讀新的：來源 World 有 Resource／元件值 1；讀者抽成自己的值。`ReplaceWorld(來源)` 讓新來源的值是 2；下一個 `RunFrame` 讀者抽到 2。`BuildNewFirst`、`DropOldFirst` 各一案。
- `BuildNewFirst` 的重設時點：新來源的 `beforeSwap` 失敗 → 讀者照讀舊來源（值 1），沒有懸空（ASan 下跑得過）；成功 → 換上之後第一次 `RunFrame` 讀到 2。
- 鏈中間的被替換：Sim→Frame→Render，換 Frame，Render 讀新 Frame 的值。
- 一個來源兩個讀者都讀到新的。
- `DropOldFirst` 失敗：Product 結束，之後入口回 `WorldMissing`。

**只有 (A)**：讀者是新的 World（讀者的一個計數 Resource 歸零）；`BuildNewFirst` 讀者建不起來時來源與讀者都留舊的；銷毀照反序（`orchestration.extract`「the reader World is destroyed before its source」延伸到替換）。

**只有 (B)**：讀者的計數 Resource 留著；換來源時對照整批作廢——測試用的讀者 System 把來源實體 id 記在 `Local` 的表裡，新來源同一個 id 是不同的值，聽 `ExtractSourceSwaps` 清表後讀到新值、不聽的版本讀到舊對照（突變：不加 `count`）；兩個讀者第二個 stage 失敗時第一個的暫存也丟掉、兩個都照讀舊來源（新來源少登記一個型別）；Kernel `ExtractTests.cpp` 的 DI-KS 38 各案；`dynamicInsert = false` 的讀者換來源回 E2、舊的照跑。

突變（只跑相關的）：(A) `or-rx-` 前綴：不換讀者、銷毀順序反過來、失敗不丟新讀者；(B) `or-rx-`：不改來源、不加通知、一個失敗時不丟別的暫存；`ks-rx-`：不重新解析、stage 就換進、失敗不丟暫存、不受 `dynamicInsert` 擋。

## 7. 建議：(B) 准許換來源

1. **換一次的代價小**：以素材為鍵的 GPU 貼圖與網格、UI 與 ImGui 的圖集都留著（以來源實體為鍵的狀態兩者都要丟，不算在這裡）。(A) 每次斷線重連、每次大廳↔遊戲都要重傳全部素材（量沒量過：沒有，第 3.1 節）。
2. **鏈自然**：只動直接讀者；PvP 第 4 段的 Sim→Frame→Render 換 Sim 時 Render 的 Registry 與 World 不動（讀到的 Frame 內容照樣跟著變）。(A) 要整條鏈一起建、一起失敗、反序銷毀。
3. **不禁止另一種**（P7）：選 (B) 時，作者要讀者重建就在換來源後自己 `ReplaceWorld(讀者)`；選 (A) 時讀者的狀態沒有辦法留。
4. **和 Bevy、Godot 的結果一致**：渲染側活過模擬內容的變化，以模擬實體為鍵的東西隨通知丟掉（第 4 節）。
5. **Kernel 的改動在既有機制上**：之後一輪的暫存副本、`Adopt`、`ResolveSystems` 重新解析都已存在；新函式只是「開一輪、改指標、建」與「換進」。

(B) 的風險與接手：作者忘了聽通知、用舊來源的實體 id 讀新來源——不加守衛（P5），由設計審查看；R6 做保留式對應時把「聽通知」寫進那個 Module 的契約。

## 8. PvP 怎麼用它來避免「切大廳時關窗再開」

今天（#330）大廳與 Client 是兩個 Product，切換時整個 Product 重建，視窗跟著關掉再開。原因有兩個：(1) 沒有 R1b 時，有渲染 World 讀它的 World 不能替換；(2) 連線目標在 `NetProductOptions::server`（`engine/modules/net/include/engine/net/NetService.hpp:26-40`），Product 建立時就定了（net D-8）。

R1b（兩個做法都行）解掉 (1)。PvP 的做法：

1. 一個 Product，兩個 World：**前台 World**（大廳與 Client 用同一份描述；大廳是它的一個狀態，或產品自己的 `build` 依 O-15 共用處裡的「現在要哪一種」登記不同的東西）＋**渲染 World**（`extractFrom` 指向前台 World，帶 `ui_render`、`sprite_render`、`mesh_render` 等）。`sdl_window`、`sdl_gpu` 在 Product 層建視窗與裝置，整個行程只有一個視窗。照 (B)，渲染 World 保持預設的 `dynamicInsert = true`（前台 World 照權威策略設 `false` 不影響，它是來源）。
2. 大廳送出位址 → runner 在兩次入口之間 `ReplaceWorld("front", {extra: 設定這次的連線})` → 新的前台 World 準備時開握手。斷線（`PeerDisconnected`）→ 同樣 `ReplaceWorld` 回大廳的狀態。視窗不關。
3. **兩種前台都要登記渲染 World 抽的型別**（審查 S-4）：讀者抽的型別必須在來源登記（`Registry.cpp:316-323`），大廳那一種若沒登記遊戲的型別，(A)、(B) 的替換都回 E2。最簡單是前台 World 一直登記全部型別，大廳時只是沒有那些實體。
4. 照 (B)：渲染 World 留著，大廳的字形圖集、遊戲的貼圖網格第一次上傳後一直在；照 (A)：每次切換渲染 World 重建、重傳。

**(2) 不在 R1b**：連線目標要能在每次換前台 World 時給，是 net Module 的事（例：`NetService` 給 runner 一個「下一次連這裡」的函式，O-12 的「runner 直接讀寫／呼叫 Module 給的函式」；或連線目標改進 `NetWorldOptions`／準備函式）。同一類的還有 `userId`／`token`（`NetService.hpp:37-38`），也是建 Product 時定；而且帶著 net 的前台 World 一準備好就開握手（`net.zh-Hant.md:198`），大廳狀態要不要 net、怎麼不連，也在那邊定。這要 PvP 卡或 net 的卡定，R1b 回報時一起轉主會話。

## 9. 範圍、協調、分工與估時

- **範圍外**：保留式的實體對應（R6）；`sdl_gpu` 的貼圖網格改成 Product 一份（第 4 節 Unity 那段）；net 的連線目標（第 8 節 (2)）；多執行緒下的替換（O-13，PvP 第 4 段定）。
- **協調**：PvP 卡的 #331（`Product::Lend`、模擬執行緒）改編排層的 `Product`。本批改 `ReplaceWorldSteps` 與 Extract 那一段；碰到同一批檔案時以先合的為準，後合的 merge 跟上（不 rebase）。R1b 的替換在 `Lend` 出去的 World 上怎麼辦（替換時 World 必須在 Product 手上）照 O-13 (2) 由 #331 定，本批只寫「替換時 World 都在 Product 手上」的前提。
- **分工**：作者 Opus／high（本卡）；宣告與實作各一次設計審查，Opus／high 子 Agent（作者與審查者同模型：Fable 暫停）。
- **估時**（推定）：(A) 10～16 h；(B) 16～24 h（Kernel 兩步＋案 5～7 h、編排層暫存與換進 5～7 h、通知與作廢測試 2～4 h、文件與審查 4～6 h）。

## 10. 要使用者定的

### Q1：來源被替換時，讀它的 World 怎麼辦？

- **(A) 讀者一起重建**：不改 Kernel；每次替換下游全部重建，GPU 貼圖網格重傳、渲染側狀態歸零。
- **(B) 准許換來源**（**建議**）：Kernel 加「暫存換來源／換進」兩個函式，改 DI-KS 13、26、37 與 D-E14 的守衛，新增 DI-KS 38、RC 29；編排層在換上的同一步一起換進、發通知；以素材為鍵的渲染側狀態留著，以來源實體為鍵的東西由擁有者聽通知丟掉；`dynamicInsert = false` 的讀者不能換來源。
- （(C) 延後綁定：第 3.3 節，不建議；選它會一起推翻 R1a 的 DI-KS 37。）

其餘（名字 `StageExtractSource`／`CommitStaged`／`ExtractSourceSwaps`、通知放編排層、用 Resource 的 `Changed` 而不是 `Message`）卡片照第 3.2 節判，可以推翻。

## 11. 修訂 0 的設計審查：處理對照

審查者 Opus／high（作者同為 Opus）。全部採用。

| 項 | 內容 | 處理 | 改的節 |
|---|---|---|---|
| B-1 | 「失敗改回」在 bad_alloc 時不成立，讀者會指向被丟掉的新來源 | 改成兩步：全部暫存、全部成功才用不丟例外的換進；拿掉改回與 RC 29 舊的前半 | 3.2、3.4、5.2、6 |
| S-1 | 「id 不變」不保證（動態插入重置、依狀態登記）；繞過 `dynamicInsert` 違背每個 World 的宣告 | 不繞過，照樣回 `DynamicInsertRefused` | 3.2、3.4、5.2、6 |
| S-2 | 內插以 Sim 實體為鍵，(B) 也要丟，不能算 (B) 的好處 | 從 (B) 的好處拿掉；鏈那列補「讀到的內容照樣變」 | 1、3.1、3.4、7 |
| S-3 | 換來源在 Kernel、通知在編排層，出口分散 | 寫明理由（替換是 O-8 的概念、Bevy 的同步在 `bevy_extract`）；兩個 host 自寫時重評（RC 29 (b)） | 3.2 |
| S-4 | 大廳與 Client 兩種前台的型別要求；`userId`／`token` 與握手時機 | 第 8 節加第 3 步與轉 net 的事項 | 8 |
| S-5 | 第三個做法：延後綁定 | 列成 (C)，寫不選的理由 | 3.3、10 |
| N-1 | O-15 的「讀者的 Registry 先於來源」漏列 | 5.2 加 O-15 | 5.2 |
| N-2 | 本機 Bevy 是 `SubEntity<L>`，沒有 `RenderEntity` | 改正 | 4 |
| N-3 | Unity 也能在同一個 World 重連 | 第 4 節補一句 | 4 |
| N-4 | `Changed()` 是每個 System 各自的 | 第 3.2 節第 4 點補寫法 | 3.2 |


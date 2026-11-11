# GYO Kernel 設計圖：從 Bevy 抽出的不變量，與 GYO 刻意的偏離

更新：2026-11-10。Owner：架構（架構師會話）。**性質：設計圖（研究稿）。這裡的 DI-K 是候選，使用者核准後才成為 `Design Invariants`。**
證據：主 checkout `build/evidence/kernel-study/`——`bevy_core.md`（bevy_app／bevy_ecs）、`replicon.md`、`lightyear.md`、`render.md`、`assets.md`、`platform.md`（time／state／transform／input／window／tasks）、`capabilities.md`（animation／audio／ui／picking／gizmos／diagnostic／remote）、`os_io.md`。八份只讀審閱，每條有檔名行號；bevy 0.20.0-dev、bevy_replicon 0.45.5、lightyear 0.31.0。
方法：不抄功能，抽不變量——問「這些功能都靠哪幾條不變量才成立」。驗證：兩萬二千行的複製層加一整套 netcode、渲染、資產、UI、動畫、工具，接進引擎時 bevy_ecs／bevy_app 一行未改。使用者的要求（2026-11-10）：「設計圖對了，功能是遲早的事；設計圖錯了，功能越多，債務越多。」

## 1. Kernel 的八條核心不變量（八份報告共同確認）

| # | 不變量 | 一句話 | 八份報告中的證據（摘） |
|---|---|---|---|
| K1 | **時間是具名、顯式、可插入的階段清單** | 「何時」有名字，plugin 以名字接入 | MainScheduleOrder 是 Vec（bevy_core）；replicon 收在 PreUpdate 送在 PostUpdate；lightyear 回滾在 PreUpdate；render 的 RenderScheduleOrder；state 用 insert_after 插 StateTransition；remote 自插 RemoteLast |
| K2 | **存取即契約，衝突在初始化檢查** | System 宣告讀寫什麼；Kernel 不懂它做什麼也能排序與防衝突 | SystemParam init_access／B0002（bevy_core）；replicon 與 lightyear 手寫 SystemParam 宣告讀取；render 的 Extract 只宣告唯讀；animation 用 EntityMutExcept 排除集合證明可平行 |
| K3 | **型別抹除的統一登錄：id → 函數表／儲存** | 核心不認識 plugin 的型別 | ComponentId／Resource／Message（bevy_core）；序列化表、時間線表、回滾比較函數（網路）；Assets<A>＋TypeId→fn 表（assets）；EvaluatorId（animation） |
| K4 | **結構變更延後、在同步點依序套用** | 疊代中不生成不刪除；順序可定義 | Commands／ApplyDeferred（bevy_core）；replicon 的 despawn 緩衝；render 的 ExtractCommands；assets 的 queued_events；gizmos／diagnostics 的 Deferred |
| K5 | **plugin 兩段生命週期：build 收集、finish 凍結** | 登錄完成後才能算雜湊、配 id、組系統 | replicon finish 結算協議雜湊；render ready／finish 等 GPU；assets 的 preregister→finish；runner 輪詢 ready |
| K6 | **生命週期 hook（add／insert／remove／despawn）** | 結構變了有人知道 | replicon 的 observer；render 的 sync_world；ui 互動狀態；relationship 靠 hook |
| K7 | **每元件每實體的變更 tick** | 增量的基準 | replicon「跟該 client 上次確認的 tick 比」；transform 的 mark_dirty_trees；ui 佈局讀 Ref 判髒；assets 的 AssetChanges |
| K8 | **時間是可替換的 Resource，固定步可重入** | 回滾與對時的前提 | lightyear 同步重跑 N 次 FixedMain、手動推進 Time<Fixed>、改 virtual 速度；bevy_time 的 TimeUpdateStrategy；gizmos 的 Fixed 緩衝 |

這八條就是 Kernel 的協定。下面各層的不變量都建在它們之上。

## 2. 各層補充的不變量（能力層怎麼用 Kernel）

| 層 | 不變量 | 來源 |
|---|---|---|
| 網路 N1～N12 | 見 [networking.zh-Hant.md](networking.zh-Hant.md)：兩個 tick 領域、wire id 由契約、結構走可靠值走不可靠、ack 帳本歸複製元件、遮罩歸送端、時間線調速度不調步長、輸入 ring buffer 與不回滾、step 可重入、延遲補償查歷史、transport＝位元組＋狀態、事件不早於依賴、決定性由模擬層保證 | replicon、lightyear |
| 渲染 R1～R6 | **R1 模擬與渲染是兩個 world，只在一個 extract 點交會**；R2 GPU 資源生命週期綁 device 世代（RenderStartup 可重跑）；R3 Draw＝Queue 進 phase→Sort→每 view 執行唯讀的 RenderCommand；R4 pass 順序用標籤集合宣告，不另設圖；R5 Asset→GPU 有明確轉換契約（來源型別、依賴、可重試、可限流）；R6 pipeline 依 key 特化並快取 | render |
| 資產 A1～A6 | A1 每型別一個 store＋每型別一份事件，core 只持 TypeId→fn 表；A2 id 只在行程內有效，path（或 Uuid）才是契約；A3 非同步完成序列化到單一套用點；A4 釋放兩段式（Unused→Removed）；A5 依賴圖由 core 擁有、loader 只宣告；A6 格式解析器與執行期解讀分層（bevy_bsn） | assets |
| 平台 P1～P6 | P1 平台事件先緩衝、update 開頭整批寫成 message、由具名階段消費；P2 視窗是 entity、主視窗是標記；P3 多個時鐘、單一權威驅動點，Time 只是投影；P4 runner 可替換（App 不擁有迴圈）；P5 狀態轉換固定插入點、一幀一次；P6 跨固定步的 message 生命週期由時間系統負責 | platform |
| 能力 C1～C8 | C1 能力的狀態放在 entity 的元件上，不放能力自己的單例；C2 給玩法的事件經 entity observer 派送並保留 Message 拉取；C3 階層是一等關係（layout、傳遞、冒泡共用）；C4 擴充點＝「訊息契約＋約定的集合」，不是 trait 物件登錄；C5 跨 plugin 的排序錨點下沉到最底層（**代價：底層知道上層名字——GYO 不採**）；C6 固定步的除錯緩衝獨立；C7 模擬與渲染以 extract 分開、渲染 plugin 可移除；C8 外部控制通道的方法＝已註冊的 system、在專用 schedule 獨占執行 | capabilities |
| OS／IO O1～O5 | O1 核心（ecs／app）不含 OS，OS 按能力分散在轉接 crate（GYO 用集中 adaptor，效果相同，要有守衛）；O2 OS 事件與網路收發只在幀邊界、固定階段批次進入；O3 IO 位置＝來源 id＋相對路徑，來源只在初始化註冊、晚註冊明確報錯；O4 時間策略可注入、測試用 Manual；O5 網路邊界只放「位元組＋連線狀態」，socket 由轉接層擁有 | os_io |

## 3. GYO 刻意的偏離：候選 Design Invariants

八份報告一致指出：Bevy 的**實作選擇**（不是設計思想）在決定性上處處留洞，而且它自己的 plugin 都在上面補（replicon 自建 wire id 與帳本、lightyear 換 SeaHash、server 不回滾）。GYO 要把這些補在核心，寫成約束：

| 候選 DI | 內容 | 取代的 Bevy 實作選擇 | 守衛 |
|---|---|---|---|
| **DI-K1 固定序列** | 每種 host 一份固定的階段清單（Match、Client、Editor）；階段內的順序是可重現的全序（註冊順序＋before/after，未排序即錯誤）；不做自動平行；第一版單執行緒 | 拓撲平手看插入順序、ambiguity 預設 Ignore、MultiThreaded executor 完成順序不定、sub app 依 HashMap | build 時全序檢查（相當於 ambiguity＝Error）；digest 跨平台比對 |
| **DI-K2 顯式存取、單一 writer** | 註冊時宣告 read／write／exclusive；Kernel 驗證衝突；同階段同一 Resource／欄只有一個 writer（狀態的權威） | Rust 借用檢查＋SystemParam 推導；ambiguous_with_all 逃生口 | 註冊時 assert；受限 view 型別；突變測試 |
| **DI-K3 id 由契約或登錄順序決定** | 元件、Resource、事件的 id 依登錄順序；wire id 由契約（.proto 欄位號／契約檔）；實體 id 單執行緒依階段內順序配發；回呼依註冊順序 | ComponentId 依首次使用、並行 entity allocator、Observer 依 EntityHashMap、InstanceId 隨機 UUID | 登錄表測試；禁止雜湊順序參與判定（review＋守衛掃描容器型別） |
| **DI-K4 Kernel 中性** | Kernel 不認識任何產品、資產意義、能力名字；跨能力的排序錨點用中性的階段名，不像 bevy_app 持有 AnimationSystems、TransformGizmoRenderStep | C5 | 名詞守衛（現有 neutral_names_guard 的做法）；Layers 表 |
| **DI-K5 step 是純函數、時間只來自 tick** | 模擬 step＝Step(state, inputs, tick)，可在同一幀重入（回滾）；固定步內不用 f32 delta；變更以依 Plan 位置取值的單調 stamp 標記、tick 視角以 tickStamps 換算（使用者 2026-11-11 改，原文「用遊戲 tick 標記」；見 spec A.1），不用執行計數 | delta_secs f32、overstep_fraction f32、變更 tick＝System 執行次數、動畫 seek_time 累加 | 既有 digest 與重放；回滾重跑的等價測試 |
| **DI-K6 核心不含 OS；OS 事件幀邊界批次進入** | Kernel 與 ECS 不出現平台條件分支；OS 事件與網路收發在幀邊界、固定階段批次交付；收包時間取該幀的時鐘取樣 | lightyear 收包 Instant::now()、winit 幀內時序 | 守衛掃描 Kernel 目錄的平台分支；中立標頭 |
| **DI-K7 render 只經 extract 讀模擬快照，無反饋** | 模擬與渲染兩個 world；render 只讀 const 快照；禁止 render→Time、readback→模擬事件、搬走資產資料 | Bevy 的四條反饋路徑（send_time、take_gpu_data、entity_sync 寫回、ReadbackComplete） | review；digest 不受渲染影響的測試 |

兩處「不是偏離、是 GYO 本來就有」：狀態的權威（AGENTS §3）對應 DI-K2 的單一 writer；產品可刪除（§7）往下推到能力層＝「plugin 就是 owner，註冊自己的一切、能整個拿掉」。

## 4. 反例清單（Bevy 的實作選擇，GYO 不照抄）

插入順序平手；ambiguity 預設關；多執行緒 executor；Observer 依雜湊順序；ComponentId 依首次使用；並行 entity allocator；變更 tick≠遊戲 tick；sub app HashMap；f32 delta／overstep；max_delta 截斷讓時間消失；render→Time 的通道；SortedRenderPhase 同距離順序不定；PipelineCache id 在 mutex 內配發＋非同步編譯；資產事件順序＝task 完成順序；loader 衝突後註冊者勝只 warn；meta 版本未檢查；InstanceId 隨機；動畫 ParallelCommands 跨目標順序依執行緒；音訊無時間戳；picking HoverMap 雜湊順序；bevy_ui 為宣告歧義依賴 bevy_sprite；bevy_app 持有上層名字；block_on 忙等；執行緒數依機器。

## 5. GYO 現況對照

| 不變量 | GYO 現在 | 缺口 |
|---|---|---|
| K1 階段 | 迴圈有（RuntimeLoop、FixedTickRuntime），階段隱含在 host 程式裡 | 具名階段清單與登錄 |
| K2 存取 | 無 | 顯式宣告與驗證 |
| K3 登錄 | AssetManager 的 LoaderRegistry 有此形狀；其餘無 | 統一登錄 |
| K4 延後 | EntityStore 3b 已設計（RecordSpawn／Destroy，依 id 升冪） | 做完 3b |
| K5 生命週期 | 無 | build／finish |
| K6 hook | 無 | add／remove 回呼（依註冊順序） |
| K7 變更 tick | 無（digest 是另一回事） | 以遊戲 tick 標記的 dirty |
| K8 時間 | Time／Threads 有基礎、決定性等待器比 Bevy 嚴 | 「可替換、可重入」寫成約定 |
| R1 雙 world | RenderQueue 是那條邊界；無獨立 render 狀態容器 | 明確的 extract 介面 |
| A1～A6 | catalog 邏輯 id、StrictJson、Result、ready 前全載——**比 Bevy 嚴** | loader 副檔名衝突應為錯誤 |
| P1～P6 | PlatformSDL 中立標頭、Input 只看中立事件、FixedTickRuntime 可掛不同迴圈——大致有 | 「BeforeFixed 累積、AfterFixed 清除」寫進契約；狀態轉換插入點 |
| C1～C8 | anim_events 契約（Engine 解析、產品賦義）＝C2 的精神；時間戳 mixer 比 Bevy 嚴 | 能力狀態放元件（C1）要等 Kernel |
| O1～O5 | 集中 OS adaptor＋中立標頭、具名角色執行緒、Result IO——大致有且更嚴 | 「OS 需求只落在 OS 層」要守衛 |

**總結：缺的是 K1、K2、K3、K5、K6、K7（Kernel 本體）；K4、K8 有一半；各能力層 GYO 多半已有、且在決定性上更嚴。**

## 6. 「framework 強、Engine 弱」的診斷

framework 一直在代替不存在的 Kernel：shell、預覽、檔案服務、選擇窗記帳，都是「跨產品共用的組合」往 framework 堆，因為 Engine 被定義成「只提供能力、不組合」。Bevy 的答案：組合協定（Kernel）是引擎的一部分，能力是 plugin，共用的組合只是更多 plugin。Kernel（K1～K8）完全中性，照 Layers 表它屬於 Engine（`engine/kernel`）；進 Engine 後 framework 縮成「產品形狀但共用的組合」。

## 7. Replacement Criteria（這張設計圖什麼時候算失敗）

1. 第一個真實能力（場景線的拉桿開門、或 VFX）接進 Kernel 時，仍要改 host 迴圈或 Kernel 本體。
2. 為了決定性，階段內必須引入第二種排序機制（例如依賴求解）才能表達需求。
3. 顯式存取宣告與實作脫節到守衛抓不住（宣告讀、實際寫）的案例反覆出現。
4. 遷移一個 host 的成本超過重寫它。

## 8. 用場景驗證（願景稿的四個場景 → Fitness Check）

1. 新增 VFX／互動物件 plugin：host 迴圈與 Kernel 的 diff 行數＝0。
2. 刪掉 FPS 產品：Engine＋Kernel＋plugin 建置通過（既有 removability 檢查）。
3. Headless Match 用同一個 Kernel：Match 的階段清單不含任何視窗／渲染階段，同一份 Kernel 程式碼。
4. 換遊戲規則：Transport／Session／Asset plugin 不動，只換產品 plugin 與同步策略 plugin。

## 9. 下一步（待使用者決定）

- 本稿的 DI-K1～K7 是否核准為候選 Design Invariants。
- Kernel 的協定介面稿（Plugin／Stage／Resource／System／Event 的 C++ 形狀）與 spike；遷移順序建議：編輯器（framework 已收成 shell）→ Match（階段最清楚）→ Client（最大）。
- AGENTS §2 修訂：「基盤の協定は先に設計する」（交風紀委員）。
- 第三方評估卡（進行中）以本稿的 K1～K8 判「能不能當 plugin」。

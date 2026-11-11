# Kernel 機制規格：World／System 契約／Registry／Scheduler／Executor

狀態：**規格草案 v0.1（已對抗檢查，待使用者）**。Owner：架構（架構師會話）。日期：2026-11-11。
性質：`v8-arch-kernel-spec` 的統合稿，經一輪對抗檢查（`v8-arch-kernel-spec-adversary`：7 blocker、18 should、9 nit）後的修訂版。只讀產出：未改程式、未 commit。每條的處置見文末「§對抗檢查」；修訂輪新增或改寫的句子在各節以「（修訂：Bn／Sn／Nn）」標出處。待使用者決定的事項彙整在 K.3。

來源：三份提案者稿（Opus／high），各從一個角度推導同一套機制：

| 提案 | 角度 | 本稿主要取用 |
|---|---|---|
| 提案 1（data） | 資料面優先：先定 World、欄、id、stamp、佇列、事件的資料模型，其餘推出 | A、F、G、J 的資料模型與套用順序；K 的候選條文 |
| 提案 2（contract） | 契約與排程優先：System 描述的兩半、Registry 生命週期、Scheduler 的錯誤清單、區規則、MSVC 與 Clang 的差異清單 | B、C、D、G.1 的區規則；事件的無游標讀取規則；E.5 的例外處置 |
| 提案 3（runtime） | Runtime 組合與驗證優先：三個 host 的 World 與 schedule 組合、兩個時鐘、回滾、extract、Host 保留的職責 | H、I 的組合；一個 World 一條執行緒；L 的逐模組證據 |

基準：[mechanism.zh-Hant.md](mechanism.zh-Hant.md)（概念基準；其 §3 的審查修正 1～6 與主張 A、B 全部被本稿吸收，見各節「與 mechanism 對照」與附錄一）。
輸入：使用者的「GYO 架構設計思想」v0.2（`~/Documents/Blog/GYO 架構設計思想.md`，§6 五種契約、§8、§11、§13）、[determinism.md](../../determinism.md)（(c)／(b) 的定義與區）、[blueprint.zh-Hant.md](blueprint.zh-Hant.md)（K1–K8、DI-K1～K7 候選、反例清單）、[networking.zh-Hant.md](networking.zh-Hant.md) §1（網路 plugin 用到 Kernel 的八條）、主 checkout `build/evidence/kernel-study/bevy_core.md`（Bevy 原始碼審閱；下文 `bevy_core:N` 指該報告第 N 行）。
現況程式：三份提案各自讀過的範圍已於統合時逐條複核（檔名行號在各節；統合者未能複核的標「未複核」）。

讀法：
- 每個部件一節（A～I），固定小節：資料模型／操作／不變量／C++ 實現手段／Bevy 對照與不採的理由／取捨。
- 不變量寫成可檢查的句子，每條附「檢查」（守衛測試、啟動檢查、編譯失敗測試、文字掃描，或「審查」）。
- 「啟動錯誤」＝Runtime 建構時（plugin finish 之後、第一次執行 schedule 之前）回報、Runtime 不啟動；全部收集後一次回報，不在第一個就停。「debug 斷言」＝`GYO_ASSERT`，只在 debug 建置有效；release 不攔，本稿每處都寫明。「常開檢查」（修訂：B4／S6）＝不依 `NDEBUG`、所有組態都做的檢查，只用在以執行期 id 為輸入的 untyped 路徑（untyped `Commands`、`Despawn`、`ExclusiveWorld` 的寫入）：id 是執行期資料，查一次 `ColumnInfo` 的旗標與寫入本身同量級，所以不必只留給 debug；違反時呼叫 assertion handler（巨集名未定）。
- 本稿決定語義與資料佈局，不決定 API 名稱。`Read<T>`、`Commands`、`Link` 等只是說明用的名字。
- 標「統合者補」的段落：三份提案都沒寫、由統合者依 A～M 的要求補上。標「取捨」的小段：三份提案衝突處與取捨理由。
- 不變量編號：W（World）、S（System）、R（Registry）、D（Scheduler）、X（Executor）、F（延後佇列／hook／事件）、Z 與 G（決定性）、H（多 World）、I（邊界）。守衛測試編號 J-n（J 節）。

---

## 0. 用語

| 用語 | 定義 |
|---|---|
| Runtime | 一個 World、它的 Registry、它的 schedule 集合，加上每個 schedule 的 Executor 策略與執行緒綁定 |
| Host | 行程的入口程式：建立一或多個 Runtime，擁有執行緒、時鐘、外部服務與交換點。是產品程式或 framework 的 host 骨架 |
| World | 一個 Runtime 的資料容器：欄、Resource、事件緩衝、延後佇列、change stamp、狀態 |
| System | 一個無狀態的可呼叫物加一份描述（`SystemDescriptor`）。描述的一半從參數型別推出，另一半在登記時顯式宣告 |
| schedule | 一份有序的 stage 清單，屬於一個 World 與一個 clock domain；由 Host 定義，plugin 把 System 掛到具名 stage |
| stage | schedule 中的一段；stage 結束是預設的同步點 |
| Plan（執行計畫） | 每個 schedule 一張，finish 時建一次：步驟的線性序列（System、同步點、事件修剪） |
| 同步點 | 套用延後佇列與 hook 的步驟；一個同步點涵蓋「上一個同步點之後到它之前」的 Plan 範圍，稱一個同步窗 |
| clock domain | 一個時鐘：`Fixed`（模擬 tick）或 `Frame`（畫面）。名字由 Host 給；Kernel 只有這個概念 |
| (c)／(b) | determinism.md §1 的等級與 §2 的區。「(c) 區 System」指顯式宣告 `zone = C` 的 System |
| boundary Resource | zone=C、`role = Input` 的 Resource：(c) 的輸入，由 Host 在兩次執行之間（World `Ready`）寫入，或由 Ingress System 在 ingress stage 執行期間寫入（修訂：N4）。例：`InputRing[tick]`、`HostClockSample`、產品的 `FixedStep{stepNs}`。它是被記錄的輸入，不是讀時鐘（G-4） |
| external Resource | 與 Runtime 外部交換用的 Resource（例：`Link`、`FrameTime`）；一律 zone=B；(c) 區不得讀、不得寫（Z1、Z7） |
| domain tick | World 每個 clock domain 一個 `uint64` 計數，是該 domain「現在是第幾個 tick」的唯一來源（修訂：B5）；由 clock schedule 的 `Run` 在第一步推進；在回滾快照內 |

---

## A. World

### A.1 資料模型

```text
World
├── state        : Building | Ready | Running(plan, step) | Applying(sync point) | Interrupted
├── registry     : const Registry*（finish 後凍結）
├── ownerThread  : 只有這條執行緒能存取 World（W-6）
├── entities     : vector<EntityId>，嚴格升冪：這個 World 所有活著的實體
├── columns[ComponentId]
│     ├── keys    : vector<EntityId>   嚴格升冪、無重複
│     ├── values  : 對齊的位元組緩衝，第 i 格 = keys[i] 的元件
│     ├── added   : vector<ChangeStamp>  與 keys 同長
│     ├── changed : vector<ChangeStamp>  與 keys 同長
│     └── epoch   : uint64（debug：成員每變一次 +1；句柄失效檢查用）
├── resources[ResourceId] : { present, bytes, changed: ChangeStamp }
├── events[EventTypeId]   : 見 F.3
├── deferred              : 每個 System 一份佇列 ＋ 一份 hook 佇列（見 F.1）
├── stamp                 : ChangeStamp（uint64，單調，不回捲，不回滾）
├── tick[domain]          : uint64，該 domain 的 domain tick（§0；修訂：B5。唯一來源；在回滾快照內）
├── tickStamps[domain]    : 有上限的環形表 { tick → 該 tick 第一個 stamp }
└── syncEpoch             : uint64（每過一個同步點 +1，包括佇列為空的同步點）
```

**EntityId**
- `std::uint64_t`，與 `Engine::EntityStore::EntityId` 同型別（`EntityId.hpp:11`）。
- 在一個 World 的生命期內代表同一個實體，**不重用**。World 不保留任何值（「0 表示無」是產品規則 E-1，Kernel 不採）。
- **World 的生命期＝一個「世代」**（修訂：S4）。entity-model E-3 的 Match 段「Reset 歸回 First」（`PvpMatch::Reset()`；IpcHost 每條新連線與關閉都 `RequestReset`，entity-model §5 第 83 行）在機制裡是 **Host 銷毀 World 再以同一份凍結的 Registry 重建**（Registry 不含 World 狀態，可重用；Plan 不重建）。World 內沒有「Reset」操作；計數器跨 Reset 保留或在 World 內歸零都不允許（W-9）。這是 Host 的職責（H.7）。
- **不帶 generation**。理由：entity-store Design Invariant 1 規定 store 無 generation bits、不配發；entity-model E-3 規定 Match 段單調不回收、「舊參照只會查不到」。不重用就不需要 generation 來偵測 stale 句柄。
- 產品的 `EntityId`（E-1～E-3：Gateway 配玩家段、PvpMatch 配 Match 段、Client 不配發）**就是** World 的鍵，不另設一層對照表。id 的數值與順序因此能直接進 digest（E-9）、上 wire（N1）。
- 見 A.6「取捨」第 1 條：這一點與任務敘述「Entity（index＋generation）」不同，**要使用者決定**。

**配發**
- World 本身不配發 id。配發器是一個 Resource（說明名 `EntityAllotter`）：一段 `[first, last]`、單調、用盡時回報錯誤、不回繞。段界由產品 plugin 登記時給（對應 E-2）。
- 會生成實體的 System 持有 **型別化的** `Spawn<Bundle<A, B, …>>` 參數（修訂：B3）。它推出 `write(EntityAllotter)` **加上 A、B、… 每一個的延後寫**：bundle 的元件型別在簽名上，所以進存取集合、計入 B.3 的寫者數、受 Z2 檢查。因此兩個會生成實體的 System 一定有資料衝突：(c) 區沒寫順序就是歧義錯誤（D 節），(b) 區依登記序平手。「配發序列化」是存取契約推出來的結果，不是另一條規則。id 在**記錄時**就配好（不是套用時），所以延後佇列能依 id 排序套用（F.2）。函式體裡不能決定 bundle 的型別：沒有 untyped 的 Spawn（untyped `Commands` 只能 insert／remove，不能 spawn；F.1）。
- `EntityAllotter` 是普通 Resource，由產品 plugin 登記並宣告 `zone`（修訂：B3）。在含 (c) 資料的模擬 World 它是 zone=C、`rollback = Yes`（它本來就在快照裡，H.5）；於是 (b) System 持 `Spawn` 就是 Z2 違反（啟動錯誤），(c) 的生成只能由 (c) System 做。在純 (b) 的 World（ClientFrame、編輯器文件）它是 zone=B。
- `SpawnWithId<Bundle<…>>(id)`（修訂：B3／S3）：以**給定的 id** 生成，存取＝bundle 元件的延後寫（不含 `write(EntityAllotter)`）。用途兩個：(1) 沒有配發器的 World（Client 的模擬 World，E-3；在這種 World 登記帶 `Spawn` 的 System 是「缺 Resource」啟動錯誤 D-E3）；(2) 有配發器的 World 中由**別的配發者**配好的 id（E-2 的玩家段由 Gateway 配）。套用時 id 落在本 World 配發器的 `[first, last]` 內＝Programmer Error（debug 斷言）；id 目前活著且本窗沒有 despawn 它＝debug 斷言。權威快照寫進 Client 的 (c) 欄**不走** SpawnWithId，走 H.5 的 `Restore`（修訂：S2）。

**ColumnInfo（每個 ComponentId 一份，型別擦除；Registry 擁有）**

```text
name（顯式字串）, size, align
moveConstruct(dst, src) noexcept, destroy(p) noexcept
copyConstruct(dst, src)          // 可為空；有值才能進回滾集
trivialRelocate : bool           // == std::is_trivially_copyable_v<T>，登記時由型別推出，不可覆寫（修訂：S16）
writePolicy     : Exclusive | Shared   // 預設 Exclusive（A.6 取捨 3）
zone            : C | B                // 資料的決定性區（G 節）
role            : State | Input        // Input＝Ingress 可寫的 (c) 資料（Z3）；role=Input 必須 rollback=No（Z8，修訂：B7）
rollback        : No | Yes | Derived   // Yes 才進回滾集；Derived 的定義見下（修訂：S15）
ownerPlugin
```

`rollback = Derived` 的可檢查定義（修訂：S15）：在每個 `rollbackEnabled` 的 schedule 中，這個欄的每個讀者 r 之前，順序圖上必有同一 schedule 內的一個寫者 w（存在路徑 w → r），且寫者是 `Write<T>`（立即寫，不是延後寫）。finish 時檢查，違反＝啟動錯誤 D-E16。Derived 欄不進快照；Rewind 後第一個讀者讀到的一定是 Replay 本 tick 的寫者寫的值，所以不會讀到 tNow 的殘值。「每 tick 重算」這個形容詞由這條順序條件取代。

**Resource**
- 獨立的表，不是實體（與 Bevy 0.20 不同，見 A.5）。每個 `ResourceId` 一格 `{ present, bytes, changed }`，描述欄位同 ColumnInfo，另加 `affinity`（`Any` 或一個執行緒角色名）、`external`（I 節）、`boundary`（G 節）。
- Resource 在 Running 期間不搬動位址。

**ChangeStamp**
- `uint64`。每一個 Plan 步驟（System 或同步點）開始時，World 依 **Plan 位置**取下一個值；條件為假被跳過的 System 也消耗它的值（X-5）。取值依 Plan 位置而不依完成先後，所以 Parallel 時與 Serial 相同。
- `tickStamps[domain]` 記每個 tick 的第一個 stamp。複製層用它把「Ledger 已確認到 tick T」換算成 stamp 界線（`changed > tickStamps[T+1] − 1`），這就是 networking 圖 3 第 2 點的機制形式。
- stamp **不是** (c) 資料：不進回滾快照、不回滾、(c) 區 System 不得用它過濾（G-6）。
- 見 A.6 取捨 2：這與 DI-K5／networking §1 第 7 條「變更以遊戲 tick 標記」不同。

### A.2 操作與允許的狀態

| 操作 | 誰 | 允許的狀態 |
|---|---|---|
| 登記元件、Resource、事件、System、hook | plugin 的 build（C 節） | Building |
| 插入或移除 Resource 的值 | Host；或經延後佇列的 `resourceOps` | Ready（Host）、Applying（佇列） |
| 查詢疊代（單欄或多欄交集）、`Get(e)`、讀寫 Resource | System，經受限句柄 | Running |
| 記錄延後變更 | System（Running）；hook（Applying，記到下一窗） | Running、Applying |
| 套用延後變更與 hook | Executor 的同步點步驟 | Applying |
| 快照與還原（回滾集） | Host 或 Runtime 的 Rewind；`Restore` 的來源可以是 PredictionHistory 的一格，或 Host 提供的（可部分的）快照（修訂：S2） | Ready |
| 推進 domain tick | 只有 clock schedule 的 `Run` 第一步（修訂：B5）；Host 不直接寫 | Running（第一步） |
| Host 直接讀寫 World | Host | Ready 且在 ownerThread 上 |
| 位元組疊代（digest、複製用） | System，經受限句柄 | Running。Kernel 只給順序，編碼是 plugin 的事 |

**Host 寫入窗口（統合者補；回應提案 3 的開放問題「Startup 前直接寫入的資料是否也要被記錄」）**：Host 只在 `Ready` 狀態寫 World，寫入限於 (1) Startup schedule 執行前的初始內容，(2) `boundary` Resource（`HostClockSample`、產品的 `FixedStep`）與 `external` Resource 的交換（`Link`），(3) 回滾的 `Restore`——來源是 PredictionHistory 的一格，或 Host 從外部（權威快照）組出的快照，後者可以只含部分欄與部分實體（修訂：S2）。其中 (1)、(2) 與 (3) 的外部快照都是 (c) 的輸入：重放 host（H.4.4）必須能從記錄重現它們，所以產品要為它們提供序列化（G-4）。Host 在 Ready 狀態寫非 boundary 的 (c) 資料（例如直接改一個 (c) 欄）在 debug 斷言（W-6b）。Host 不寫 domain tick（修訂：B5）。

### A.3 疊代順序契約

1. 單欄疊代依 EntityId 嚴格升冪。
2. 多欄查詢是對各欄 keys 的 merge-join（leapfrog），輸出依 EntityId 升冪。
3. `With<T>`／`Without<T>` 只讀成員關係；成員關係只在同步點改變，所以它不產生衝突邊（D 節），也不減少衝突（D-8）。
4. 順序不受插入與刪除的歷史、容量、元件登記順序、執行緒影響（W-2）。因為 id 是契約鍵（E-2），Match 與 Client 的 World 對同一組實體產生同一順序，不需要每個 System 自己排序。

### A.4 參考失效規則

- 從句柄取得的參考、指標、span、`Mut<T>` 代理，在**該次 System 呼叫返回時**失效。
- 同步點之後所有參考一律視為失效（插入與刪除會搬動整欄，entity-store「Invalidation」）。
- debug：句柄記得取得時的 `{column epoch, run id}`，解參考時比對；不符就斷言（W-4）。存下來的裸指標或 `T&` 攔不到（B.5）。

### A.5 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| W-1 | 每個欄的 `keys` 嚴格升冪、無重複；`added`、`changed` 與 `keys` 同長；`entities` 嚴格升冪，且是所有欄 `keys` 的超集合 | debug 在每個同步點後對有改動的欄跑 `StrictlyAscending` 與長度檢查；J-1 |
| W-2 | 查詢疊代的順序是 EntityId 嚴格升冪（多欄為交集的升冪），只取決於成員集合 | J-2：兩條增刪歷史不同、最後成員相同的 World，疊代序列逐一相同 |
| W-3 | Running 期間任何欄的成員（`keys`）不變、配發器狀態不變；成員只在 Applying 改變 | debug：欄 `epoch` 在 Running 期間不變；J-3 |
| W-4 | 句柄只在發出它的那次 System 呼叫內有效；同步點後所有參考失效 | debug 比對 epoch 與 run id；J-4 |
| W-5 | 每次經 World API 存取 id 為 X 的欄、Resource 或事件，目前 System 的描述必須宣告 X 有對應種類的存取（讀、寫、延後寫、事件讀、事件寫）；沒有宣告是 Programmer Error | debug 斷言；J-5（spike 判定 (2)） |
| W-6 | Serial 下 World 的所有存取都在 `ownerThread` 上；Parallel 下（v1 不實作）一個 System 對 World 的存取只能來自 Executor 派它去的那條 worker、且只在該 System 的那次呼叫內，Running 以外的狀態仍只有 `ownerThread`（修訂：S11）；Running 期間「目前沒有 System」的存取（Host 在 schedule 執行中讀 World）是 Programmer Error | debug 斷言；J-6、J-15 |
| W-6b | Host 在 Ready 狀態的寫入只限 A.2 的三類 | debug 斷言（寫入非 boundary 的 (c) 欄或 Resource）；J-6 |
| W-7 | `destroy` 的呼叫順序有定義：despawn 時依 ComponentId 升冪；World 解構時依 (ComponentId 升冪, EntityId 升冪) | J-7 |
| W-8 | 元件型別 nothrow move 且 nothrow destroy | 登記時 `static_assert` |
| W-9 | World 不配發 id；只有 `EntityAllotter` Resource 配發，單調、不重用、用盡時回報錯誤不回繞；World 內沒有把配發器歸零的操作（世代的切換＝重建 World，A.1；修訂：S4） | J-8，含突變「用盡時回繞」「加 Reset 歸零」 |
| W-10 | World、Scheduler、Executor 中會影響結果或 Plan 的程式碼，不疊代雜湊容器、不比較指標、不讀時鐘或亂數。排序鍵只有 EntityId、ComponentId（登記順序）、Plan 位置、登記序 | 文字掃描（照 entity-store 守衛列 5、6、8）：Kernel 目錄不出現 `unordered_`、`std::hash` 疊代、`<random>`、`<chrono>`；Parallel 模組之外無 `<thread>`、`<atomic>`；附自測；J-9 |
| W-11 | 經可變句柄取可變存取時，那格的 `changed` 設為目前 stamp；唯讀存取不寫 | J-30 |

### A.6 C++ 實現手段

- **欄的儲存**：keys、值、stamps 分開存（SoA）。值是對齊配置的位元組緩衝。插入與刪除保持順序（entity-store DI 2）：`trivialRelocate` 為真時 `memmove`，否則逐格 `moveConstruct` 再 `destroy`。`trivialRelocate` 只能是 `std::is_trivially_copyable_v<T>`（C++20 只對 trivially copyable 型別定義 `memmove` 複製物件表示的結果；P1144 的 trivially relocatable 不在 C++20），登記時由型別推出、沒有登記參數可以覆寫；從位元組緩衝取回 `T*` 用 `std::launder`（spike 已用；修訂：S16）。
- **與 EntityStore 的關係**：`lower_bound` 插入、保序刪除、二分搜尋就是 `EntityColumns.hpp:74-91` 的演算法。但 EntityStore 把鍵放在元素內（`EntityKey<T>`，`EntityKey.hpp:26-33`），World 把 keys 與值分開。World 需要「回傳位置」的版本才能同步搬動平行陣列，這是 EntityStore 的一個小介面擴充（L 節）；或者 World 用 `vector<Cell<T>>{key, added, changed, value}` 直接重用 EntityStore（提案 2 的做法）。兩者語義相同；spike 選其一並報告。
- **同步點的合併**：同一欄的插入先排好序（F.2），再一次 O(n＋k) 合併；不做 k 次 O(n) 插入。第一版可以逐筆 `InsertEntity`（O(n·k)），以 entity-store RC-1 的門檻（4,000 實體、1 ms p95）為換結構的條件。
- **多欄查詢**：對已排序的 keys 做 leapfrog 交集，由最小的欄驅動；輸出依 id 升冪。
- **change stamp 標記**：可寫的疊代產出 `Mut<T>` 代理，只有取可變存取的成員（`Get()`、非 const `operator->`）才寫 `changed[i] = currentStamp`。API 沒有「拿到 `T&` 卻不標記」的路徑；把 `T&` 存起來再寫會漏標記，這是 C++ 的上限（B.5）。
- **debug 存取核對，兩層**（修訂：S8，吸收 spike 的做法）：
  1. **存取當下**：`constinit thread_local const SystemDescriptor* tlCurrent = nullptr;`，由 Executor 以 RAII 在呼叫 System 前設定、後清除。每個受限句柄在取用欄或 Resource 時，於 `#ifndef NDEBUG` 下查 `tlCurrent->access`（依 id 排序的小陣列，二分搜尋）比對 `(id, mode)`；`tlCurrent == nullptr` 且 `state == Running` 時斷言（W-6）；`state == Ready` 時允許 Host 存取。只對 System 自己的執行緒有效：System 自己另起的執行緒上 `tlCurrent` 是空的，那些存取被當成 host（spike R-03）。
  2. **System 邊界的淨變化比對**：debug 下 World 在每個 System 呼叫前後對欄、Resource、事件緩衝的位元組做快照比對：呼叫後，沒宣告寫的儲存不得有變化；呼叫前，自上次比對以來除了經 World API 的寫入（含同步點套用與 Host 在 Ready 的寫入）不得有變化。這層攔得到 `const_cast`、存下的參考在別的 System 寫、`reinterpret_cast` 改唯讀欄（spike 的 (a1)(a2)(b1)–(b3)）。攔不到：寫了又在返回前寫回、經存下的參考**讀**、物件內堆積成員（`std::vector` 的元素）的變化、有宣告 `Write<T>` 的 System 繞過句柄寫（漏 stamp）。成本與欄的總位元組成正比，只在 debug；可用 `ColumnInfo` 層級的開關關掉大欄。
  release 兩層都不檢查。這兩層就是 spike 判定 2 的「成立的部分」；判定 2 的上限照 spike 報告〈判定 2 的上限〉，本稿 B.5 的表與它一致。
- **句柄失效檢查**：debug 的句柄帶 `{column epoch, run id}`，每次存取比對（W-4）。release 下句柄只是指標加長度。
- **型別到 id**：每個 Registry 一張 `TypeKey → id` 的排序表，只在 build 與 finish 期間查詢，執行期不查（id 已快取進描述）。`TypeKey<T>` 用**非 const** 的 `static inline char` 的位址（可寫資料不會被 MSVC `/OPT:ICF` 或 lld `--icf=all` 合併；spike 第二輪前已從 `static constexpr char` 改成這個寫法；`engine/asset/detail/TypeId.hpp:37` 用 `static const int kToken` 有被合併的風險，是否真會合併未查證），Registry 對位址碰撞做啟動斷言（R-E5）。GYO 目前沒有 SHARED target（CMake 掃描為 0），跨 DLL 的 token 唯一性不是今天的問題；將來有 DLL 時重審。位址只用於查表，不用於排序或疊代（W-10）。
- **J-9 掃描的兩個白名單項**（修訂：S10）：`tlCurrent`（debug 核對模組；只在 `#ifndef NDEBUG`，不參與任何排序或結果）與 `TypeKey<T>` 的 `static inline char`（只取位址當查表鍵，W-10 第二句）。J-9 的「無非 constexpr `static`／`thread_local`」對這兩個符號名放行，其餘一律違規；白名單寫在掃描腳本裡並附理由，加一個符號就要改腳本（審查可見）。

### A.7 Bevy 對照與不採的理由

| Bevy | 證據 | GYO | 理由 |
|---|---|---|---|
| Entity 由並行配發器配發（`alloc(&self)`），多執行緒時順序非決定性 | bevy_core:32 | 配發器是 Resource，由衝突分析序列化；id 在記錄時配好 | id 順序進 digest（E-9）；配發順序必須是 Plan 的函數 |
| Entity 帶 generation、index 重用 | bevy_core.md **未查證**（報告無此行） | u64、不重用、無 generation | A.1；entity-store DI 1、E-3 |
| ComponentId 依首次登記（可能在某 System 初始化時） | bevy_core:45 | 依 plugin build 的登記順序，finish 後凍結 | id 順序要和 plugin 清單一樣可以 diff（DI-K3） |
| Table／SparseSet 兩種儲存、archetype | bevy_core:55 | 每個元件一個依 id 排序的欄 | M 節；疊代順序必須只取決於 id |
| Resource 是單例實體上的 Component（0.20） | bevy_core:3、:44 | 獨立的表 | Resource 不該出現在實體疊代與 digest 列中；hook／despawn 語義不碰 Resource |
| Tick 是 u32、回捲、每次 System 執行遞增、夾 MAX_CHANGE_AGE | bevy_core:50、:52 | u64、不回捲、依 Plan 位置取值 | 消除回捲判斷；Parallel 仍決定；(c) 不用它 |
| DerefMut 即標記變更 | bevy_core:51 | `Mut<T>` 代理在取可變存取時標記 | C++ 沒有 DerefMut；代理是最接近的對應 |
| NonSend 記 origin_thread_id 並驗證 | bevy_core:27、:46 | 放大為整個 World 一條擁有執行緒（W-6）；Resource 的 `affinity` 在 finish 靜態檢查（X-3） | 靜態檢查比執行期驗證早；World 內不上鎖 |
| 查詢疊代順序 | bevy_core.md **未查證** | 依 id 升冪是契約（W-2） | — |

### A.8 取捨

1. **Entity 不帶 generation**（採提案 1；提案 2、3 與任務敘述採 index＋generation、World 配發、產品 `EntityId` 另成元件）。理由：(1) 提案 2、3 的模型讓疊代順序依配發歷史而定，Match 與 Client 的 World 歷史不同，於是「結果依跨實體順序而定的 (c) System 必須依契約鍵排序」（提案 2 A.3 第 4 點）變成每個 System 的負擔，而 mechanism §4 第 3 點要 Match 與 Client 共用同一份 FixedUpdate System；提案 1 的模型下順序天生是契約鍵的順序。(2) 提案 2、3 要同時改 entity-store DI 1、entity-model E-3、E-5、E-6、E-9（digest 編碼要先依 `EntityId` 元件排序），是四處 Architecture Delta；提案 1 只延伸 DI 4（F.2）。(3) generation 的用途是偵測 stale 句柄；E-3 的「不重用」讓它沒有用途，W-9 守住不重用。代價：id 不回收（entity-model RC-2 觸發時再加 generation，K.2 第 2 條）。**這與任務敘述不同，要使用者決定**；若使用者選 index＋generation，F.2 的套用順序改為「Plan 位置＋FIFO」（提案 2 F.1），A.3 第 4 點的跨 World 順序承諾撤回，L 節 entity_store 與 entity-model 列改寫。
2. **change stamp 依 Plan 位置取值，不等於遊戲 tick**（採提案 1、2；提案 3 以 World 的 clockTick 當 change tick；DI-K5 與 networking §1 第 7 條也寫「以遊戲 tick 標記」）。理由：只用 tick 時，同一 tick 內排在讀者之後的寫入會被讀者永久漏掉（讀者下次執行 last_run = T，寫入的 tick 也是 T，`T > T` 不成立）；用 `>=` 則重複讀到。Bevy 用執行計數不是任性（bevy_core:50）。遊戲 tick 的視角由 `tickStamps` 推出。配套：(c) 區禁止變更過濾（G-6）。**要使用者確認**，因為它修改 DI-K5 候選的一句。
3. **寫入政策預設 Exclusive，Shared 要明寫**（採提案 3；提案 1 無預設必須明寫；提案 2 預設 Shared）。理由：mechanism §3.1「進 authority digest 的欄預設獨占」的意圖是 AGENTS §3 的狀態權威；Kernel 不知道 digest，所以把預設放在全部元件上才能變成啟動檢查，又不要 Kernel 認識 digest。另加提案 1 的 S-9：產品 plugin 宣告「這些元件進 digest」，其中有 Shared 就是啟動錯誤。**要使用者確認**（提案 2 列為待決）。
4. **一個 World 一條擁有執行緒**（採提案 3 的 W-6；提案 1 把「Client 的 FixedUpdate 與 FrameUpdate 是一個 World 還是兩個」留給 -audit）。理由：不這樣就得在 World 外上鎖，等於回到今天 `MatchRuntimeHost::Step` 整段持 `mutex_`（`MatchRuntimeHost.cpp:371`）；兩個時鐘在不同執行緒就是兩個 World（H-2）。
5. 欄的 `zone`、`role`、`rollback` 三個屬性（提案 2 的 zone／role、提案 3 的 rollback）合併進 ColumnInfo；提案 1 的 `domainHint` 由 `zone` 加 Z 規則取代。

---

## B. System 契約

### B.1 資料模型

```text
SystemDescriptor（不可變，finish 時產生）
├── name            : 顯式字串（不從編譯器取，B.6 風險 1）；只用於 before／after 與診斷，不參與排序
├── registrationSeq : 登記序（D 節的平手鍵）
├── derived（從參數型別推出，編譯期收集型別，finish 時換成 id）
│     reads / writes       : {(Kind ∈ {Component, Resource, Event}, Id)}
│     membershipReads      : {ComponentId}        With／Without；不產生衝突
│     changeFilters        : bool                 有 Changed／Added
│     deferred             : {ComponentId 可延後 insert／remove}（含 Spawn／SpawnWithId 的 bundle 元件，修訂：B3）、despawn、spawn（spawn 另推出 write(EntityAllotter)）
│     setReads             : {SetId}              SetRead<SetId>：finish 時展開成該具名集合的 ComponentId 讀（修訂：S13）
│     local                : bool                 有 Local<T>
│     exclusive            : bool                 參數是 ExclusiveWorld（唯一參數；只允許 zone=B，S-13）
│     extract              : {(sourceRuntime, Kind, Id)} 來源 World 的唯讀（H.6）
├── declared（登記時顯式）
│     schedule, stage, sets
│     runConditions   : [ConditionDesc]（條件也從簽名推出，且只能唯讀；其 reads 併入本 System）
│     before / after  : [SystemName | SetName]
│     affinity        : Any | Role(name)
│     effects         : {EffectId}（socket、file、gpu、device… 由 plugin 登記的不透明 id）
│     zone            : C | B（沒有預設，S-6）
│     ingress         : bool（只在 zone=B 有意義，Z3）
│     rollbackSafe    : bool（只對有副作用、掛在可回滾 schedule 的 System 有意義，H-4）
│     source          : std::source_location（登記處；給 J-17 的掃描匯出用）
└── runtime
      run   : void(*)(SystemContext&)   型別擦除的跳板
      state : 參數狀態（解析好的 id、欄指標快取）
```

### B.2 兩半的來源

| 半 | 內容 | 怎麼保證可信 |
|---|---|---|
| 推導（編譯期型別 → 登記時 id） | Component R／W、With／Without、Changed／Added、Resource R／W、事件讀寫、延後佇列句柄（及其型別化的結構寫入）、Local、exclusive、extract | 一、句柄只開放宣告過的型別，型別錯就編譯不過。二、debug 建置在 World 每次存取時核對（A.6）。三、`static_assert` 擋掉不允許的參數形狀（B.4） |
| 顯式（登記時） | schedule／stage／set、run 條件、before／after、執行緒親和、外部副作用、決定性區、ingress、rollbackSafe | 啟動檢查驗「一致性」（D 節的錯誤、G 節的區規則）；驗不到「真實性」（宣告沒有 socket 卻開了 socket）→ 審查與葉子三否定條件（I 節） |

### B.3 欄的獨占寫入契約

- `writePolicy == Exclusive` 的欄或 Resource，**在同一個 World 上、跨這個 Runtime 的所有非 Startup schedule**，寫者至多一個。寫者的單位是 **System**（不是 owner plugin；修訂：S1，理由見下）。寫者包括：`Write<T>`／`ResMut<T>` 的 System；型別化 Commands 中宣告 insert／remove `T` 的 System；`Spawn<Bundle<…>>`／`SpawnWithId<Bundle<…>>` 的 bundle 含 `T` 的 System（修訂：B3）；宣告寫 `T` 的 hook。不算寫者：Runtime 的回滾還原、Host 在 Ready 狀態的寫入、**Startup schedule 上的 System**（只跑一次、在任何 clock schedule 之前，與其他寫者沒有同時性；初始化與執行期的權威分開數，修訂：S1）。`ExclusiveWorld` 的 System 寫不到 Exclusive 欄（S-13），所以不計。
- **Despawn 不算寫者**：它是成員關係結束，不是值的寫入。對 (c) 資料的限制由區規則 Z5 處理。
- 第二個寫者＝啟動錯誤 D-E2，訊息列出所有寫者的 name 與 `source`。這是 AGENTS §3「狀態的權威只有一個」的機制版。
- **寫者以 System 計的後果**（修訂：S1）：今天 `bodies_`（`PlayerState`）由三段寫（`ResolveLifeBoundaries` 寫 position 與 lifeState，`PvpMatch.cpp:310-331`；移動迴圈 `StepMovement`，`:571`、`:582`；`Kill` 寫 lifeState 與 respawnTick，`:301-304`）。照 H.4.1 拆成三個 System 就是三個寫者，D-E2 必發。本稿**預期**這條檢查迫使 Body 依 entity-model 第 139 行已做的欄位分類拆欄（身體、生命、輸入游標、host 診斷四組），不是把三段併成一個 System，也不是把 Body 改 Shared（S-12 不准）。這是機制要施加的壓力，不是規格自相矛盾；K.2 第 10 條的計數從遷移後開始算，遷移本身的拆欄不算。替代方案「寫者以 owner plugin 計」能免掉拆欄，但 Scheduler 檢查的粒度就變成 plugin 的程式碼組織而不是資料流，所以本稿不採；**要使用者確認**（K.3）。

### B.4 C++ 實現手段（從簽名推出）

1. **入口形式**（修訂：S9）：`AddSystem<&Fn>(…)`，`Fn` 是非成員、非模板的函式，以**非型別模板引數**傳入（執行期的函式指標不是常數運算式，不能當 `Thunk<Fn>` 的模板引數，所以沒有 `AddSystem(fnPtr)` 這種形式）；無捕獲 lambda 以**型別**傳入 `AddSystem<F>(…)`，C++20 的無捕獲閉包型別可預設建構，跳板內以 `+F{}` 取函式指標或直接呼叫 `F{}(…)`。有捕獲的 lambda、成員函式、泛型 lambda（`auto` 參數）、多載的 `operator()`：`static_assert` 失敗並附說明。理由：捕獲與 `this` 是 World 以外的隱藏狀態與繞路通道；禁止它們把繞路面縮到全域變數與 `Local`。成本：每個 System 一個 `Thunk` 的實例化（一個函式）；沒有執行期間接呼叫。
2. **型別萃取**：`FunctionTraits<R(*)(Args...)>` 與 `FunctionTraits<R(*)(Args...) noexcept>` 兩個特化（noexcept 是函式型別的一部分）。回傳型別限 `void`。只特化預設呼叫慣例（B.6 風險 2）。
3. **參數映射**：每個參數型別 `P` 必須有 `ParamTraits<P>` 特化，否則 `static_assert(dependent_false<P>)`。`ParamTraits<P>` 提供 `using Contribution = TypeList<Read<A>, Write<B>, …>`（編譯期只收集型別）、`static P Fetch(SystemContext&)`、`static constexpr bool readOnly`。
4. **允許的參數型別**（全部以值傳遞的小句柄）：`Query<Q…, F…>`（`Q` ∈ `Read<T>`、`Write<T>`；`F` ∈ `With<T>`、`Without<T>`、`Changed<T>`、`Added<T>`）、`Res<T>`、`ResMut<T>`、`EventReader<E>`、`EventWriter<E>`、`Commands`（untyped；只能 insert／remove zone=B、Shared 的欄，F.1）、`CommandsFor<Inserts<…>, Removes<…>>`、`Spawn<Bundle<…>>`、`SpawnWithId<Bundle<…>>`（修訂：B3；bundle 型別進存取集合）、`Despawn`、`Local<T>`、`ExclusiveWorld`（必須是唯一參數；只允許 zone=B，S-13）、`Extract<Read<T>>`／`ExtractRes<R>`（只在 extract schedule 合法）、`SetRead<SetId>`（修訂：S13；讀 Registry 中一個**具名元件集合**的全部欄，以型別擦除的位元組疊代加 stamp 提供；集合的成員由各 plugin 在 build 登記、finish 時凍結，所以 Scheduler 在 finish 時知道確切的 ComponentId 集合。這是 replicon「讀全部被複製元件」（replicon.md §3，手寫 SystemParam）與 digest 編碼在本機制的形狀；它不是動態查詢：執行期不查型別、不改集合。`digestSet`（S-12）是其中一個具名集合）。任何 `T&`、`T*`、`const T&`、`World&`、`int`：`static_assert` 擋下。
5. **編譯期自我衝突檢查**：把所有 Contribution 串成一個 TypeList，O(n²) 的 constexpr 比較：同一 `T` 不得同時 `Read` 與 `Write`（要讀又寫只宣告寫）；`Write<T>` 不得出現兩次；`ResMut<T>` 與 `Res<T>` 不得並存。參數上限 16，讓 pack 運算保持在兩個編譯器的 constexpr 與模板深度預設之內（B.6 風險 3）。
6. **型別 → id**：登記時進行，不在編譯期（id 是執行期依登記順序配的）。查不到＝啟動錯誤 R-E4「System X 引用未登記的型別 T」。
7. **呼叫跳板**（修訂：S8／S9／N8，採 spike 的形狀）：`template <auto Fn> void Thunk(SystemContext& ctx) { Fn(ParamTraits<Args>::Fetch(ctx)...); }`。句柄是不可複製、不可移動的 prvalue，經 pack expansion 直接初始化以值接收的參數（C++17 保證的複製省略），所以不能先裝進 `std::tuple` 再 `std::apply`（那要移動）。函式呼叫引數的求值順序未指定（MSVC 與 Clang 不同），因此 **`Fetch` 不得有副作用、不得依賴順序**（只解析指標與 id）：這是契約，J-12 的 System 以兩種引數順序的組合檢查結果相同；不依賴 `[dcl.init.list]/4` 的由左到右保證（歷史上編譯器對它有缺陷，GCC PR 51253）。
8. **句柄的受限性**（修訂：S8）：`Query<Read<A>>` 只提供 `const A&`；`Write` 才給 `Mut<A>`。句柄**刪除複製（因而不可移動）、建構子私有、只有 `ParamTraits` 的工廠能造**（spike 的做法）：System 在編譯期就不能把句柄存到 `Local`、全域或靜態變數，也不能自己造一個沒宣告的句柄。句柄內只有 id 與 World 的私有指標；World 的型別化存取 API 放在 `detail`，以 friend 或 passkey 只開放給 `ParamTraits`。能存起來的只剩從句柄取出的 `T&`／`T*`（B.5）。
9. **型別擦除**：一個 System 一個 `Thunk<Fn>` 函式指標放進描述；參數狀態是 `std::tuple<typename ParamTraits<Args>::State...>`，配置在 System 的節點上。
10. **debug 核對**：A.6。

### B.5 從簽名推導的上限（誠實清單）

| 繞路 | 能否攔 | 怎麼攔 |
|---|---|---|
| 經未宣告的 id 讀寫 World（例如把 `World*` 存進全域再呼叫存取 API） | debug 攔，**存取當下**；限 System 自己的執行緒（System 另起的執行緒上當成 host） | A.6 第一層（W-5、W-6）；spike (c1)–(c5) |
| 把句柄存起來，下次執行再用 | **編譯失敗**（句柄不可複製、不可移動；修訂：S8） | B.4 第 8 點；J-4 改成編譯失敗測試 |
| 把 `Read<T>` 取出的 `const T&` 存起來，在別的 System 或 host 程式中 `const_cast` 寫 | debug 攔，**System 邊界**（下一個 System 開始或本 System 結束時的淨變化比對） | A.6 第二層；spike (a1)(a2) |
| System 內對 `Read` 的參考 `const_cast`、對 `Res` `const_cast`、`reinterpret_cast` 改唯讀欄 | debug 攔，System 邊界 | A.6 第二層；spike (b1)–(b3)；另有文字掃描（J-17） |
| 寫了又在返回前寫回原值；經存下的參考**讀**未宣告的欄；物件內堆積成員（`std::vector` 元素）被改 | 不攔 | spike〈判定 2 的上限〉；審查；頁保護是已知但未採的手段（spike 報告末段） |
| 把 `Mut<T>` 取出的 `T&` 存起來，下次再寫（有宣告 `Write<T>`） | 不攔（存取合法；change stamp 不會更新） | W-4 寫成契約；審查；J-11 的「全量對增量」等價測試能抓到漏標記 |
| 讀寫全域或 static 變數（含 `Base::Log` 的 `logHandler`，`Log.hpp:43`；Assert 的 `assertionHandler`，`Assert.hpp:49`，與 `thread_local reportingAssertion`，`Assert.hpp:50`；修訂：S17） | 不攔 | (c) 區 System 的來源檔清單由 `source` 匯出，對這些檔做文字掃描（J-17）：禁止非 constexpr 的 `static`、`thread_local`、`<random>`、`<chrono>`、`std::hash`；log 與 assert 寫出（不讀回）是明列例外（I-5） |
| 在 `Local<T>` 裡存指向 World 的指標 | 不攔 | (c) 區禁用 Local（S-7）；(b) 區審查 |
| 經葉子模組的全域狀態（`GlobalCache::Update()` 類） | 不攔 | 葉子三否定條件（I-3）；審查 |
| 未宣告的外部副作用（開 socket、寫檔） | 不攔 | 審查；(c) 區禁止任何 effect（S-8），所以 (c) 檔出現 OS 標頭時 J-17 能抓 |
| release 建置 | 全部不攔 | 寫明；spike 判定 (2) 已要求寫明 |

這張表就是 mechanism §3.5 的三層（API 讓繞路醜、debug 核對、審查）加第四層：(c) 區 System 的來源檔可匯出，「審查」中有一部分變成掃描。它也是 v0.2 §11 與 §13 第 4 問的答案：C++ 能做到「推導可信＋debug 攔得到經 World 的繞路」，做不到「絕對」。

### B.6 MSVC 與 Clang 的差異風險（spike 要驗）

1. **型別名字**：`__PRETTY_FUNCTION__` 與 `__FUNCSIG__` 格式不同 → 名字一律顯式；任何雜湊、digest、排序都不得用編譯器產生的名字。
2. **呼叫慣例**：MSVC x64 的 `__vectorcall` 仍是不同的函式型別；無捕獲 lambda 在 MSVC 提供多個慣例的轉換運算子，x86 上 `+lambda` 有歧義，x64 是否也有**未查證** → `FunctionTraits` 只特化預設慣例。
3. **constexpr 與模板深度**：`/constexpr:steps`、`/constexpr:depth` 與 `-fconstexpr-steps` 預設不同 → 參數上限 16、自我衝突檢查 O(n²)、不做 constexpr 排序。
4. **concept 約束的部分特化**：MSVC 歷來有缺陷（`EntityKey.hpp:26-33` 已用這種特化，表示目前版本可用；對 `ParamTraits` 的複雜約束**未查證**）→ 盡量用完全特化或單層 `requires`。
5. **TypeKey 位址唯一性**：A.6；spike 驗 ICF 是否合併 `static const` 唯讀資料。
6. **`[[no_unique_address]]`**：MSVC 忽略它 → 句柄不依賴它。
7. **引數求值順序**：B.4 第 7 點（`Fetch` 無副作用是契約；不依賴求值順序）。
8. **符合性模式**（修訂：N2）：`/permissive-` 在 app target（`GyoAppProject.cmake:104`）、framework（`framework/CMakeLists.txt:9`）、`tests/common/{base,math,entity_store}`（各自的 CMakeLists `:12`、`:10`、`:10`）都有；`engine` target 本身只設 `cxx_std_20`（`engine/CMakeLists.txt:24`），沒看到顯式的 `/permissive-`。MSVC 的 `/std:c++20` 是否隱含 `/permissive-`（記憶中自 VS 2019 16.11 起是）**未查證** → spike 的 target 明確加 `/permissive-`；風險比原稿寫的小。

### B.7 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| S-1 | System 的可呼叫物無狀態：函式指標或無捕獲 lambda；有捕獲的 lambda、成員函式、泛型 lambda 不能登記 | `static_assert`；J-10（編譯失敗測試） |
| S-2 | 每個參數型別都有 `ParamTraits` 特化；`World&`、產品物件、裸指標、`int` 不能當參數 | 編譯失敗；J-10 |
| S-3 | 同一 System 內同一 id 不能寫兩次、不能同時讀又寫 | constexpr 檢查；J-11 |
| S-4 | derived 存取集合等於各參數 Contribution 的聯集，不多不少；run 條件的 reads 併入 | J-12（spike 判定 (1)：10 個以上簽名） |
| S-5 | run 條件的參數全唯讀 | `static_assert`；J-13 |
| S-6 | 每個 System 都顯式宣告 `zone`，沒有預設 | finish 檢查（D-E7）；J-14。理由：預設 B 讓 (c) 靜默變寬鬆，預設 C 讓呈現層被迫寫順序 |
| S-7 | zone=C 的 System 不得有 `Local<T>`（Local 不在 World 裡、不進快照，回滾時殘留） | 啟動錯誤（D-E7）；J-14 |
| S-8 | zone=C 的 System 不得宣告任何 effect | 啟動錯誤（D-E7）；J-14 |
| S-9 | zone=C 的 System 不得用 `Changed`／`Added` 過濾 | 啟動錯誤（D-E7）；J-38 |
| S-10 | `ExclusiveWorld` 參數單獨出現；exclusive System 與同 stage 的所有 System 衝突（衝突分析視為讀全部 id、寫全部 zone=B 且 Shared 的 id） | `static_assert`；Scheduler |
| S-11 | 獨占寫入：B.3 | finish 檢查 D-E2；J-15 |
| S-12 | 宣告「進權威 digest」的元件，寫入政策必須是 Exclusive | finish 檢查；J-16 |
| S-13 | `ExclusiveWorld`（修訂：B4）：只允許 zone=B 的 System（zone=C 持 `ExclusiveWorld`＝D-E7）；它的句柄對所有資料提供唯讀的 untyped 存取，寫入 API 只接受 zone=B 且 `Shared` 的欄與 Resource，對其他 id 的寫入是**常開檢查**（§讀法）的 Programmer Error；它不能 spawn、不能 despawn 帶 zone=C 欄的實體（同 Z5）。因此 exclusive System 永遠不是 Exclusive 欄的寫者、不碰 (c) 資料，B.3 與 Z2 不需要列舉它。編輯器檢視器要改的欄必須登記為 zone=B、Shared，或改用型別化 System | `static_assert`（zone 在登記時給）；常開檢查；J-45 |

### B.8 Bevy 對照與不採的理由

| Bevy | 證據 | GYO | 理由 |
|---|---|---|---|
| `SystemParamFunction` 以 `all_tuples!` 支援 0～16 參數 | bevy_core:25 | `ParamTraits` 特化＋TypeList；上限同為 16 | B.6 風險 3 |
| `SystemParam` 有 init_state、init_access、get_param、apply | bevy_core:26 | `Contribution`、`Fetch`、`State`；不帶 apply（佇列由 World 持有，依 Plan 位置套用） | F.2 |
| Res → add_read，ResMut → add_write，衝突在初始化（B0002） | bevy_core:27 | S-3 提前到編譯期；跨 System 衝突在 D 節 | — |
| `Local` 不參與存取 | bevy_core:27 | 相同；(c) 區禁用（S-7） | 回滾 |
| `&mut World` 獨占 | bevy_core:27 | `ExclusiveWorld`（S-10） | — |
| NonSend 記 origin thread 驗證 | bevy_core:46 | System 層的顯式 affinity（E.3）；debug 驗執行緒 | v0.2 §6.4 |
| 借用檢查保證宣告與實作一致 | bevy_core:65 | 受限句柄、debug 核對、掃描、審查 | C++ 沒有借用檢查（v0.2 §11） |
| 閉包可為 System（FnMut） | bevy_core.md **未查證** | S-1 拒絕捕獲 | 捕獲是 C++ 最容易的繞路 |
| bevy_core 的結論「註冊時明確宣告，模板萃取只輔助」 | bevy_core:29、:64 | **與該建議不一致**：照 mechanism §3.1 審查修正 4，描述從參數型別推出，不手寫第二份。spike 判定 (1) 不成立時退回 bevy_core 的建議（手寫宣告＋debug 核對），那是 spike 的退路，不是本規格的預設；中間層則照 mechanism §1 全部重寫 | 審查修正 4 |

### B.9 取捨

1. Despawn 不算獨占寫者（採提案 2）；提案 1 以虛擬鍵「結構」讓 despawn 與每個查詢衝突，提案 3 讓所有 `Commands` 持有者互相衝突。理由：結構變更在同步點才套用，同一窗內的查詢結果不受 despawn 記錄影響，所以不需要衝突邊；(c) 的 despawn 由 Z5 守，順序由 F.2 的「依 id、去重」保證與記錄順序無關。**提案 2 把它列為待使用者決定**；本稿維持。
2. (c) 區禁用 `Local`（採提案 2、3）；提案 1 只禁指標型的 Local。理由：回滾只還原 World，Local 殘留會讓重跑與一次跑完不同（H-2）。
3. 參數上限 16（採提案 2）；提案 1 不設上限。理由：B.6 風險 3。

---

## C. Registry

### C.1 資料模型

```text
Registry
├── plugins     : vector<PluginRecord{name, requires[], ownerOf[], state: Built|Finished}>（依 Host 清單順序）
├── components  : vector<ColumnInfo>         // index = ComponentId
├── resources   : vector<ResourceInfo>       // index = ResourceId
├── events      : vector<EventInfo{name, domain, zone}>  // index = EventTypeId
├── effects     : vector<EffectInfo>         // index = EffectId
├── schedules   : vector<ScheduleDef>（Host 提供，D.1）
├── systems     : vector<SystemDescriptor>   // index = registrationSeq
├── hooks       : 每個 ComponentId 的 {onAdd[], onRemove[]}（依登記順序）
├── sets        : vector<SetDesc{name, before, after, conditions}>
├── namedSets   : vector<{name, members: {ComponentId}}>（修訂：S13；具名元件集合，各 plugin 在 build 把自己的元件加入既有集合；finish 凍結；`SetRead<SetId>` 在 finish 展開成這些 id 的讀）
├── digestSet   : namedSets 中的一個（由產品 plugin 宣告；S-12）
├── typeIndex   : TypeKey → id（行程內查表，只在 build／finish 期間用）
├── frozen      : bool
└── fingerprint : SHA-256（finish 後算）；另算 cFingerprint（只含 zone=C 的部分，H.3）
```

### C.2 操作與生命週期

1. **Host 列出 plugin**：有序清單，是資料。
2. **build 段**：依清單順序呼叫每個 plugin 的 `Build(Registrar&)`。plugin 登記它擁有的元件、Resource、事件、effect、System、hook、set；可以在既有 schedule 的具名 stage 前後插入新 stage。id 在登記當下依序配發。每個型別只能由一個 plugin 登記（owner）；第二次登記＝啟動錯誤 R-E2。別的 plugin 只在 System 參數中使用它，不登記它。
3. **依賴檢查**：`requires = {plugin name…}`。被依賴者不在清單中、或排在依賴者之後＝啟動錯誤 R-E3。Kernel **不做拓撲排序**：清單順序就是 id 順序，要顯式；錯了就報錯。
4. **finish 段**：全部 build 完成後，依同一順序呼叫 `Finish(const FrozenRegistry&, FinishContext&)`。plugin 讀最終的 id，建自己的表（複製的 wire 序列化表、協議雜湊，K5；`bevy_core:8`），初始化 Resource 的值。**不得再登記**：`Registrar` 在 finish 時已不可取得，型別上做不到（R-2）。
5. **凍結**：`frozen = true`，算 fingerprint（依 id 順序對 (name, size, align, zone, writePolicy, role) 與 (System name, schedule, stage, zone, 存取集合) 以及各 Plan 的步驟序列做 SHA-256）。用途：診斷；兩端比對 FixedUpdate 的 Plan 是否相同（H-4）。
6. **建 Plan**（D 節）。任何啟動錯誤在這裡一次回報。

### C.3 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| R-1 | ComponentId、ResourceId、EventTypeId、EffectId、registrationSeq 等於 Host 清單的 plugin 順序加 plugin 內的呼叫順序；不受型別首次使用、雜湊、位址、連結順序影響 | J-18：兩次建置 id 相同；交換兩個 plugin 的順序後 id 依新順序；fingerprint 跨平台 golden（`cpu` 標籤） |
| R-2 | finish 之後任何登記在型別上不可能或是斷言；跑 Running 前必須已凍結 | 編譯失敗測試（finish 的 context 不含 Registrar）；J-19 |
| R-3 | 執行期的 id 不出現在 wire 或 digest 上；wire id 由 `.proto` 契約決定（N2） | 文字掃描：digest 與 wire 的程式碼不引用 `ComponentId` 型別；審查 |
| R-4 | 一個 plugin 的 build 在每個 Runtime 最多執行一次；plugin 名字重複＝R-E1 | J-19 |
| R-5 | 所有 build 在任何 finish 之前完成；finish 依清單順序 | J-19 |
| R-6 | 每個型別恰有一個 owner plugin；從清單移除該 plugin，引用它的 System 得到 R-E4，不靜默缺漏 | J-19（移除 plugin → 預期錯誤清單）。這是 AGENTS §7「Product 削除」在 Kernel 層的檢查方式 |
| R-7 | Kernel 中性：Kernel 的程式碼與 Registry 不含任何產品、工具、能力的名字；schedule 與 stage 名稱由 Host 或 framework 的 plugin 登記，Kernel 只定義 `Startup`、clock domain 的概念 | 名詞守衛（照現有 neutral_names_guard 的做法）；J-9（DI-K4） |

### C.4 啟動錯誤

| 代號 | 條件 |
|---|---|
| R-E1 | plugin 名字重複 |
| R-E2 | 型別被兩個 plugin 登記 |
| R-E3 | `requires` 的 plugin 缺漏或排在後面 |
| R-E4 | System 或 hook 參數引用了未登記的型別 |
| R-E5 | TypeKey 位址碰撞（A.6） |

### C.5 C++ 實現手段

- `Registrar` 與 `FrozenRegistry` 是兩個型別。`Plugin` 是小介面（`Name()`、`Requires()`、`Build(Registrar&)`、`Finish(const FrozenRegistry&, FinishContext&)`），虛擬函式或函式表；只在啟動時呼叫。
- 元件屬性以登記參數傳入：`registrar.Component<T>({.name = "…", .zone = Zone::C, .writePolicy = Shared})`。不用型別上的 trait，同一事實只在 owner 的登記點一處（AGENTS §9）。
- plugin 名字由 plugin 自己提供字串常數，不用 `type_name`（bevy_core:10）。

### C.6 Bevy 對照與不採的理由

| Bevy | 證據 | GYO | 理由 |
|---|---|---|---|
| Plugin：build、ready、finish、cleanup；runner 輪詢 ready | bevy_core:6、:8 | build、finish 兩段。不做 ready（GPU 等待由 Host 在 Runtime 建立前處理）與 cleanup | GYO 資產 ready 前全載（blueprint §5 A 列）；少一個狀態 |
| 沒有中央登記表，各 API 直接改 World／Schedules | bevy_core:11 | 中央 Registry、凍結 | 凍結是 id 決定性、fingerprint、Plan 建一次的前提 |
| PluginGroup 有序但 plugin 間無宣告式依賴；`is_plugin_added` 以 type_name 比對 | bevy_core:9、:10 | 顯式名字＋`requires` 檢查，**不重排** | 與 bevy_core:72 的建議（「明確宣告依賴，框架拓撲排序並報缺漏」）**部分不一致**（修訂：N1）：採「明確宣告、報缺漏」，不採「拓撲排序」。理由：清單順序就是 id 順序與 (b) 平手順序（R-1、D-4）；由框架重排會讓加入順序成隱性語義，fingerprint 也跟著變 |
| `register_component_checked` 已登記則回原 id | bevy_core:56 | 第二次登記＝錯誤（R-E2） | ownership（AGENTS §7） |
| DuplicatePlugin | bevy_core:7 | 同（R-E1） | — |
| sub_apps 是 HashMap，順序由雜湊決定 | bevy_core:12 | 多個 Runtime 的順序由 Host 明寫 | 決定性 |

### C.7 取捨

三份提案在 C 節一致。取提案 2 的「一個型別一個 owner」與錯誤代號、提案 1 的 fingerprint 內容、提案 3 的「build 內可插入 stage」。

---

## D. Scheduler

### D.1 資料模型

```text
ScheduleDef（Host 提供；Kernel 不內建任何名字）
├── name, world, domain（Fixed | Frame | Startup）
├── stages        : [StageName]   固定序列；plugin 在 build 可於具名 stage 前後插入
├── syncPoints    : 每個 stage 末一個（預設）＋顯式宣告的具名同步點 [(stage, after System 或 Set)]
├── eventTrim     : 哪個同步點修剪該 domain 的事件（預設：最後一個 stage 末）
├── executor      : Serial | Parallel{guardTestId}
├── runner        : Main | Role(name)   由 Host 宣告：誰呼叫 Run
├── rollbackEnabled : bool
└── clock         : bool（`Run` 的第一步把 World 的 `tick[domain]` +1；每個 World 每個 domain 恰一個，H-1。修訂：B5——domain tick 只有這一個來源，Host 與產品 Resource 都不另記）

ScheduleGraph（每個 schedule 一張）
├── nodes         : System | SyncPoint | EventTrim
├── orderEdges    : u → v（u 必須在 v 前）。來源：before／after、Set 展開、stage 界線、同步點節點、(b) 平手加入的邊
├── conflictEdges : u — v（不能同時；無方向）。只供歧義分析、診斷與 Parallel
└── Plan          : vector<Step{kind, index}>（Serial 的全序；finish 時建一次、不可變）
```

**衝突邊的條件**（同一 stage 內的每一對 (a, b)，跨同步點者已由順序隔開）：
1. `writes(a) ∩ (reads(b) ∪ writes(b)) ≠ ∅` 或反之；Kind 含 Component、Resource、Event（事件：寫者對寫者、寫者對讀者）；
2. 任一方 exclusive；
3. 雙方宣告同一個 EffectId；
4. **延後寫**（`CommandsFor` 的 insert／remove、`Spawn`／`SpawnWithId` 的 bundle、hook 的寫）只用於歧義分析（第 10 步）與獨占檢查，不產生 Parallel 的衝突（每個 System 有自己的佇列）。歧義分析中算衝突的配對只有一種（修訂：S18）：**延後寫對延後寫，同一 ComponentId**（兩筆對同一 (實體, 元件) 的 insert 以 (planPos, recSeq) 決勝，F.2；remove 對 insert 同欄也依階段序而定）。不算：延後寫對立即讀（同窗的讀者永遠看不到延後寫）、延後寫對立即寫（延後寫必在同步點落地、晚於任何立即寫，順序無關）、延後寫對 membershipReads。配對表寫在 J-22 的案例裡。
5. `membershipReads`（With／Without）與 untyped `Commands` 不產生衝突；Despawn 不產生衝突（B.9 取捨 1）。Spawn 經 `write(EntityAllotter)` 自然衝突。`SetRead` 展開後的 id 與一般讀同樣處理。

**衝突邊與順序邊分開存**（mechanism §3.2，審查修正 2）：衝突邊只說「不能同時」，順序邊說「誰先誰後」。Scheduler 不假設業務順序。

### D.2 分析流程（finish 時，依序；錯誤全部收集後一次回報）

| 步 | 檢查 | 結果 |
|---|---|---|
| 1 | System 的 schedule 或 stage 不在這個 Runtime 的 ScheduleDef 中 | 啟動錯誤 D-E1 |
| 2 | 獨占違反：對每個 Exclusive 欄或 Resource，數這個 World 上所有非 Startup schedule 的寫者（B.3，含 bundle）> 1；digestSet 中有 Shared 欄（S-12） | D-E2 |
| 3 | 缺 Resource：`Res`／`ResMut`／`Spawn` 讀的 Resource 未登記，或 Startup 結束時仍未初始化且 Host 未宣告「啟動時插入」。不做 Optional 參數（M 節） | D-E3 |
| 4 | before／after 目標不存在 | D-E4。不靜默忽略：被引用的 System 所屬 plugin 被刪時，引用者必須顯式改（AGENTS §7 的明示依存） |
| 5 | 建順序 DAG：stage 序的邊、before／after、set 展開、同步點節點（stage 內的 System 都在它之前；具名同步點以 before／after 連接）。逆 stage 序的 before／after | D-E5 |
| 6 | 順序圖成環 | D-E6，回報環上的節點名 |
| 7 | 區規則 Z1～Z8（G.1；Z5 的靜態可判部分）、S-6～S-9、S-13、事件跨 domain 或在非 clock schedule 讀寫（F-9） | D-E7 |
| 8 | 親和不符（X-3） | D-E10 |
| 8b | 含 (c) 的 schedule 宣告 Parallel 而無守衛（X-2） | D-E11 |
| 8c | 每個 World 每個 domain 的 clock schedule 數不是 1（H-1）（修訂：N7，原稿雙重否定的筆誤） | D-E12 |
| 8d | 同一 World 的 schedule 綁不同執行緒（H-2） | D-E13 |
| 8e | extract 來源 Registry 未凍結（H-6） | D-E14 |
| 8f | `rollbackEnabled` 的 schedule 上，`ingress = true` 的 System 沒有 `NotInRollback` 條件（H-10；修訂：B7） | D-E15 |
| 8g | `rollback = Derived` 的欄在某個 `rollbackEnabled` schedule 中有讀者而順序圖上沒有先於它的同 schedule 立即寫者（A.1；修訂：S15） | D-E16 |
| 8h | v1 宣告 `Parallel`（任何 schedule）：v1 不實作，不靜默降為 Serial（X-2；修訂：S11） | D-E17 |
| 9 | 衝突邊（D.1 的條件）；同 stage 內的遞移閉包 | — |
| 10 | 對每條衝突邊 (u, v)：有順序路徑 → **資料衝突**，不是錯誤，序列化、先後由順序邊定。沒有順序路徑 → **歧義**：u 或 v 任一方 zone=C → **啟動錯誤 D-E8**（主張 A），回報兩個 System 的 name、`source` 與衝突的 id；兩方都 zone=B → 加順序邊「registrationSeq 小的在前」，以診斷等級列出 | 三種結果 |
| 11 | zone=C 的 hook 衝突：同一觸發點（同一 ComponentId 的 OnAdd 或 OnRemove）的兩個 hook 存取衝突且任一方 zone=C（hook 沒有 before／after 可寫） | D-E9（主張 A 延伸到 hook） |
| 12 | 全序化：Kahn 演算法，可選節點中取 registrationSeq 最小者（min-heap）。產出 Plan 與 Plan 指紋 | — |

歧義的判斷涵蓋立即寫、延後寫（`CommandsFor`、bundle、hook；配對表見 D.1 第 4 條）、事件寫。hook 與 System 之間不會出現歧義（修訂：S12）：hook 的存取掛在它所屬同步點的節點上，同窗的每個 System 都有順序邊指向該同步點，不同窗的也隔著同步點，所以永遠有順序路徑。錯誤代號表：D-E1～D-E17，加 R-E1～R-E5、X-E1～X-E3、F-E1、H-A1；J-24 以這張表為準（修訂：N7）。

### D.3 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| D-1 | 同一 Registry 與 ScheduleDef，Plan 唯一：只取決於登記序與宣告，不取決於雜湊、位址、執行緒、平台 | J-20：建兩次相同；debug 下打亂內部容器順序仍相同；Plan 指紋跨平台 golden |
| D-2 | 每一條衝突邊在最終 DAG 中都有確定的先後 | 計畫建完後自檢（debug 與測試）；J-21 |
| D-3 | 有 (c) 區端點的衝突邊若無順序路徑，finish 失敗 | J-22，含突變「(c) 歧義改成平手」「不計延後寫」 |
| D-4 | (b) 區沒有順序的衝突對，registrationSeq 小的先跑 | J-23 |
| D-5 | D-E1～D-E17、R-E1～R-E5、X-E1～X-E3、F-E1 每一種都讓 finish 失敗且報告指名雙方 | J-24（每種一個最小 Registry，比對錯誤清單） |
| D-6 | Plan 在 finish 時建一次；Running 期間不重建、不改步驟；run 條件只決定跑或跳過 | Plan 是 const 物件；審查 |
| D-7 | Scheduler 只讀描述，不打開函式體，不認識任何型別名稱 | Scheduler 標頭不 include 任何元件型別（掃描）；R-7 |
| D-8 | With／Without 在 v1 不減少衝突 | J-25：兩個 System 各寫 A，一個帶 `With<B>`、一個帶 `Without<B>`，仍算衝突 |
| D-9 | (c) 區對登記順序不敏感：把同一組 (c) System 的登記順序任意排列，要嘛全部 D-E8，要嘛每一對衝突 System 的先後相同、World 摘要相同。「World 摘要」＝所有欄的 keys 與值、Resource 的值、事件 payload，**不含** `added`／`changed` stamp、`tickStamps`、Plan 位置（重排無衝突的 System 會改 stamp，那不是 (c) 資料；修訂：N6） | J-44（排列測試；主張 A 的守衛） |
| D-10 | 全序的平手鍵只有 registrationSeq；不用 name、指標、雜湊 | 突變：改依 name → D-9 仍成立但 J-23 失敗；改依指標 → J-20 失敗 |

### D.4 C++ 實現手段

- 純執行期演算法，輸入是 POD 描述。存取集合用排序後的 `vector<uint32>` 合併比較，或 bitset（id 數凍結後固定）。
- 衝突檢查 O(N²·|access|)，以 stage 為單位；遞移閉包每個 stage 一個 bitset 矩陣，N 是數百時 O(N²/64)（未量測）。只在 finish 做一次。
- 不用雜湊容器（W-10），全部以 index 排序。
- 錯誤以 `Result<Plan, vector<ScheduleError>>` 回報；依 error-handling 契約，這是啟動時的 Runtime Error（組合者的資料錯了），Host 拒絕啟動並決定輸出。

### D.5 Bevy 對照與不採的理由

| Bevy | 證據 | GYO | 理由 |
|---|---|---|---|
| hierarchy 與 dependency 兩張 DAG；`ambiguous_with` | bevy_core:18 | 順序邊與衝突邊兩張；**不提供** `ambiguous_with`／`ambiguous_with_all` 等豁免 | 豁免讓「(c) 歧義即錯誤」失效；豁免的方式就是寫 before 或 after |
| 0.20 weak edge：無存取衝突時忽略 | bevy_core:18 | 不採 | 順序依衝突有無而變是隱性的 |
| 拓撲＝Tarjan SCC 反序，平手看插入順序（隱性） | bevy_core:20 | Kahn＋最小 registrationSeq；(c) 平手＝錯誤 | 平手規則寫成契約、可測試 |
| Ambiguity 預設 Ignore；Error 時 build 失敗 | bevy_core:21 | (c) 固定 Error、(b) 固定穩定平手；不可設定 | 不讓每個會話選 |
| debug 的 shuffle_seed 檢查漏寫的約束 | bevy_core:20 | 不需要：(c) 漏寫是錯誤；(b) 用排列測試（J-44）代替 | — |
| 自動插入 ApplyDeferred | bevy_core:33 | 不自動插：每個 stage 末＋顯式宣告 | 同步點位置決定誰看得到生成，必須明寫（mechanism §3.4） |
| FilteredAccess 的過濾式不相交判斷 | bevy_core.md **未查證**（bevy_core:28 只列 FilteredAccessSet 與檔案行號，沒有「用 Without 證明不相交」的主張；修訂：N1） | v1 不做（D-8） | 簡化；誤報多時由 K.2 第 5 條處理 |
| Schedules 存 HashMap，執行序由 Vec 決定 | bevy_core:20 | schedule 執行序由 Host 決定（H 節） | — |

### D.6 與 mechanism／blueprint 的不一致

- blueprint DI-K1「未排序即錯誤」不分區，比 mechanism §3.2 嚴。本稿照 mechanism：只有 (c) 是錯誤。
- (c) 與 (b) 的 System 互相衝突時（區規則下只可能是「(b) 讀 (c) 資料」），依第 10 步任一方是 C 就是錯誤。所以讀 (c) 資料的 (b) System（呈現）必須與 (c) 寫者有顯式順序或位於不同 stage。三份提案一致；提案 2 指出這比 mechanism 嚴，是否太嚴列為 K.2 第 3 條（為消除 D-E8 而加的 before／after 一半以上與資料流無關時，區劃錯了）。

### D.7 取捨

1. 歧義的範圍涵蓋延後寫與事件寫（三份一致；提案 1 寫得最明）。
2. 具名同步點（提案 1、2）對「只在 stage 邊界、要多一個就拆 stage」（提案 3 X2）：採前者。理由：mechanism §3.4 寫「schedule 可以多宣告」；兩者表達力相同，具名同步點在 ScheduleDef 中可檢查，不需要為一個同步點發明 stage 名。
3. D-E4（before／after 目標不存在）與「錯誤全部收集後一次回報」採提案 2。

---

## E. Executor

### E.1 資料模型

```text
ExecutorConfig（Host 給）
├── roles        : { roleName → 執行緒 }（Main 是 Host 綁定的角色之一；Dedicated 用 Engine::Threads::RoleThread）
└── workerCount  : Parallel 時；Serial 忽略

每次 Run(world, plan)：呼叫者必須是該 schedule 的 runner 執行緒，且 world.state == Ready
```

### E.2 操作

`Run(world, plan)`：
1. 斷言 `state == Ready` 且呼叫者在 `ownerThread`；設 `Running`。
2. 依 Plan 逐步：
   - **System**：①依序求 run 條件（短路；全部為真才執行）；②取 stamp（條件為假也取，X-5）；③設 `tlCurrent`；④`run(ctx)`；⑤清 `tlCurrent`。
   - **SyncPoint**：設 `Applying`，依 F.2 套用；`syncEpoch++`；回 `Running`。
   - **EventTrim**：依 F.3 修剪。
3. 設 `Ready`。

- **Serial**：在 runner 執行緒上依 Plan 逐一執行。
- **Parallel**（v1 不實作，只是規格；v1 的 Registry 中出現 `Parallel` 宣告＝啟動錯誤 D-E17，不靜默降為 Serial，修訂：S11）：stage 內依順序 DAG 派工到 worker；一個 System 的順序前驅都完成、且沒有和正在執行的 System 衝突時才派；`affinity == Any` 才能上 worker，有角色的留在 runner；同步點是 barrier；stamp 依 Plan 位置預先分配；佇列依 Plan 位置套用；完成順序不影響任何資料。W-6 在 Parallel 下的形式見 A.5（存取只來自派工的 worker、只在那次呼叫內）。
- **預設**：所有 schedule 預設 Serial。
- **含 zone=C System 的 schedule 開 Parallel**：必須宣告 `Parallel{guardTestId}`；J-27 列舉 Registry 中所有這類宣告，核對該 CTest 名稱存在且帶 `cpu` 標籤；該守衛以同一組情境比對 Serial 與 Parallel 執行後 (c) 欄的 digest 逐位元相同（主張 B，比照 collision DI 1 的「StaticAabbGrid 等價於線性掃描」）。沒有守衛＝啟動錯誤 X-E1。
- **只含 zone=B 的 schedule**：可宣告 Parallel，不需守衛。殘餘風險：宣告不實（宣告讀、實際經全域寫）時結果可能依時序而變，屬 B.5 攔不到的類別。

### E.3 執行緒親和的三種落點

| affinity | 意義 | 啟動檢查 | 執行 |
|---|---|---|---|
| `Any` | 任意執行緒 | — | Serial：runner 自己跑；Parallel：worker |
| `Role(Main)` | 平台執行緒（建立視窗、抽事件） | 只能放在 `runner = Main` 的 schedule，否則 X-E2 | runner 直接呼叫 |
| `Role(name)`（專用） | 具名角色執行緒（GPU 提交、音訊、`gyo-match-sim`、`gyo-client-sim`） | `name` 必須是該 schedule 的 runner 角色，否則 X-E3 | 該執行緒就是 runner（整個 schedule 在它上面跑） |

- v1 **不把單一 System 轉送到別的執行緒**（X-6）：親和不符在啟動時報錯。理由：同步轉送在 System 中間引入跨執行緒等待，那是 Host 的職責（H.7）。
- 宣告 `affinity` 的 Resource 同樣檢查（X-3）。
- debug：呼叫前斷言目前執行緒符合 affinity（比照 NonSend 的 origin thread 驗證，bevy_core:46）。
- Executor 不擁有執行緒：專用執行緒由 Host 用 `Engine::Threads::RoleThread::Start`（`RoleThread.hpp:73`）建立並決定停止順序（`RoleThread.hpp:62`：擁有者決定 stop 與 join 的順序）。worker pool 屬於另一個 Engine 能力（entity-store「Out of scope」的 Jobs），不在 Kernel。

### E.4 run 條件

- 在**執行時**求值，位置緊接在該 System 之前，看到的是前面所有步驟執行後的狀態；每次執行求一次，不快取（X-4）。
- 條件的存取併入該 System 的讀集合，參與衝突分析（S-4）。
- zone=C 的 System，其條件只能讀 (c) 資料（Z1）。
- Set 的條件：每個成員各自求值一次（不像 Bevy 只對 set 求一次，E.7）。理由：成員之間可能有寫者改變條件結果。

### E.5 例外

- Executor 不捕捉 System 或 hook 拋出的例外。`tlCurrent` 以 RAII 清除。
- 例外穿出時，World 標記 `Interrupted`：當前同步窗已記錄的延後佇列**丟棄**，事件緩衝不動，已套用的部分保持原樣。之後任何 `Run` 在 debug 與 release 都斷言，直到 Host 呼叫 `Restore` 或重建 World。
- 理由：半個 stage 的狀態不能被下一個 tick 當完整狀態使用。現況 `MatchRuntimeHost::Body` 捕捉例外並停止（`MatchRuntimeHost.cpp:542`），語義一致。

### E.6 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| X-1 | Serial 的 System 呼叫序列＝Plan 中 System 的子序列，扣掉條件為假者 | J-26（記錄型的測試 System） |
| X-2 | 含 zone=C System 的 schedule 預設 Serial；宣告 Parallel 必須指名一個存在、帶 `cpu` 標籤、比對 Serial 與 Parallel 逐位元相同的守衛測試，否則 X-E1；v1 任何 Parallel 宣告都是 D-E17（修訂：S11） | J-27 |
| X-3 | 親和：schedule 中每個 System 與 Resource 的親和都必須由 runner 滿足；不滿足是 X-E2／X-E3；執行時不符則 debug 斷言 | J-28 |
| X-4 | run 條件每次執行前求值一次，結果不快取 | J-29：條件讀的 Resource 在前一步被改，條件看到新值 |
| X-5 | 被跳過的 System 也消耗 stamp；stamp 只取決於 Plan 位置與執行次數 | J-30 |
| X-6 | v1 不跨執行緒轉送單一 System | 啟動檢查 X-E2／X-E3；審查 |
| X-7 | Parallel 時每個 System 的 stamp、佇列套用順序、事件 seq 都與 Serial 相同 | J-27 的守衛（Parallel 實作時啟用） |
| X-8 | Executor 不讀時鐘、不讀亂數、不依執行緒數決定任何順序；worker 數由設定決定、與機器無關 | J-9 文字掃描 |
| X-9 | `Interrupted` 的 World 不再執行 | J-35 |
| X-10 | 策略不是決定性：決定性來自 G 的四處契約；Executor 只保證 Serial 下依 Plan 全序執行 | 文字；J-26 |

### E.7 Bevy 對照與不採的理由

| Bevy | 證據 | GYO | 理由 |
|---|---|---|---|
| multi_threaded feature → 預設 MultiThreaded；完成順序非決定性 | bevy_core:22 | 預設 Serial；(c) 的 Parallel 要守衛 | mechanism §3.3、主張 B；PR #177 的證據：序列化只是必要條件 |
| Main、FixedMain、RunFixedMainLoop 強制 SingleThreaded，但 FixedUpdate 等內層用預設 Executor | bevy_core:17 | 每個 schedule 顯式宣告；含 (c) 的預設 Serial | Bevy 的內層預設正是決定性的洞 |
| 延後佇列依拓撲序 index 套用、佇列內 FIFO | bevy_core:34 | 依 F.2 的「階段＋id」 | 結果不依記錄順序 |
| set 條件只求一次 | bevy_core.md **未查證** | 每個成員各求一次 | E.4 |
| 在 Executor 記 change tick、每個 System 前遞增並記 last_run | bevy_core:50 | stamp 依 Plan 位置取；World 持有 | X-5、X-7 |
| run condition 求值時機、存取併入 | bevy_core.md **未查證** | X-4、S-4 | — |

### E.8 取捨

1. 例外處置採提案 2（`Interrupted`，Host 必須 Restore 或重建）；提案 1 的 F-6 讓 World 回 Ready 繼續用。理由：半個 stage 的狀態對 (c) 不是完整狀態；entity-store 契約對 throw 的「佇列清空、可繼續用」只對 store 自身成立，對 World 整體不成立。
2. 親和採「`Any` 或具名角色」（提案 3），把提案 1、2 的 Main／Worker／Dedicated 收成角色表的三種落點。理由：v0.2 §6.4 自己說不能假設圖形 API 必在主執行緒，角色由 Host 綁。
3. 只含 (b) 的 schedule 可以不帶守衛開 Parallel（提案 2）；提案 1、3 只規定含 (c) 的 schedule。採提案 2 並寫明殘餘風險。

---

## F. 延後佇列、hook、事件

### F.1 延後佇列：資料模型

每個 System 一份 `CommandBuffer`（arena 加命令清單），加一份 hook 佇列：

```text
spawns      : vector<{EntityId, bundle: [ComponentId, bytes]…, planPos, recSeq}>   // id 在記錄時由配發器配好，或由 SpawnWithId 給定
inserts     : vector<{EntityId, ComponentId, bytes, planPos, recSeq}>
removes     : vector<{EntityId, ComponentId}>
despawns    : vector<EntityId>
resourceOps : vector<{ResourceId, insert|remove, bytes, planPos, recSeq}>
```

- **`recSeq`**（修訂：B1）：每個佇列自己的記錄序號，從 0 起、每記一筆 +1；同一 System 一次呼叫內的紀錄因此有全序。`planPos` 是記錄者在 Plan 中的位置；hook 的紀錄 `planPos` = 它所屬同步點的 Plan 位置、`recSeq` 由 hook 佇列統一計數（hook 的執行順序由 F.2 的階段與 id 決定，所以 recSeq 是決定性的）。(planPos, recSeq) 在一個同步窗內**唯一**：不同 System 的 planPos 不同；同一 System 的 recSeq 不同；hook 佇列是獨立的一個 planPos。排序鍵的唯一性由這兩個欄位保證，不靠 `stable_sort`。
- 同一個 System 本次呼叫中 `Spawn` 回傳的 id 立即可用於 `Insert`（id 已配好）；因此不需要 provisional handle。
- 未型別化的 `Commands` 只能 insert／remove zone=B 且 Shared 的欄，不能 spawn；動 zone=C 或 Exclusive 的欄要用 `CommandsFor<Inserts<…>, Removes<…>>`（型別參數進存取集合、參與 D-E2 與區規則）。違反是**常開檢查**（§讀法；修訂：S6／B4）：記錄當下查 `ColumnInfo` 的 `zone` 與 `writePolicy`，不符就呼叫 assertion handler，debug 與 release 都攔（F-10）。理由：untyped `Commands` 是正式 API，不是繞路；只靠 debug 斷言會讓 release 靜默通過。

### F.2 同步點的套用（Applying），逐階段

每一步的排序鍵寫死；結果不依**各 System 之間**記錄的先後；同一 System 內的多筆同鍵紀錄依該 System 的記錄序（recSeq，就是函式體的程式順序，Serial 下決定）。階段順序（修訂：S4——Despawn 與 Remove 移到 Spawn 之前，讓同窗「刪 X 再以同 id 生成 X」成立；原稿的 spawn 先、despawn 後會把新實體刪掉）：

0. **hook 佇列先套用**：上一個同步窗的 hook 記錄的變更，planPos 為該同步點的 Plan 位置、recSeq 依 hook 佇列計數（F.1）。它們觸發的 hook 同樣記到下一窗。
1. **收集**：同步窗內各 System 的佇列。
2. **Despawn**：依 EntityId 升冪，只對**窗開始時活著**的實體；對不存在的 id 無作用、不計數。對該實體的每個元件依 ComponentId 升冪執行 OnRemove，再移除並 `destroy`，最後從 `entities` 移除。同一 id 記錄兩次只處理一次（冪等）。(b) 發出者對帶 zone=C 欄的實體的 despawn 是常開檢查（Z5）。
3. **Remove**：依 (EntityId 升冪, ComponentId 升冪)。先執行 OnRemove hook（此時格子還讀得到），再移除。移除不存在的格子（含第 2 步已 despawn 的）無作用、不觸發 hook。
4. **Spawn**：依 EntityId 升冪。加入 `entities`，插入 bundle 的元件；`added` 與 `changed` 設為這個同步點的 stamp。同一 id 本窗記錄兩次 → debug 斷言（entity-store 契約）；id 在窗開始時活著且本窗未 despawn → debug 斷言（只有 SpawnWithId 會碰到）。
5. **Insert**：依 (EntityId 升冪, ComponentId 升冪, planPos 升冪, recSeq 升冪)。目標實體不存在（含第 2 步 despawn 的）且不在本窗 spawn → 丟棄，計入診斷計數 `deferredDroppedMissingEntity`（結果，不是錯誤）。格子不存在 → 插入，`added`、`changed` 設 stamp。格子已存在 → 覆寫，`changed` 設 stamp；同一 (實體, 元件) 多筆時 (planPos, recSeq) 最大者勝出（跨 System 的兩筆已被 D-E8 強制寫順序，所以 planPos 的大小就是宣告的先後；同一 System 的兩筆依程式順序，後寫的勝）。同一欄的插入已排序，一次合併套用（A.6）。
6. **OnAdd hook**：對 4、5 中**新增**的格子，依 (EntityId 升冪, ComponentId 升冪)；每格的 hook 依登記順序。覆寫不觸發 OnAdd。
7. **Resource 操作**：依 (ResourceId 升冪, planPos 升冪, recSeq 升冪)。
8. **清空**佇列；`syncEpoch++`（佇列為空也加）；結束 Applying。

結果：同一窗內刪除 X 又以 `SpawnWithId(X)` 生成（Rejoin 同 id，entity-model §5.3）→ 舊實體銷毀、新實體在下一窗可見，hook 看到 remove 再 add。同一窗內生成 Y（新 id）又 despawn Y：despawn 對窗開始時不存在的 id 無作用，所以 Y 存活——這與原稿「生成又刪除的實體不可見」相反；只有生成者自己（它才知道 Y）能寫出這種程式，本稿把它定義成「生成勝出」並寫明。不留孤兒格：對第 2 步 despawn 的實體的 Insert 在第 5 步被丟棄。

**同步點的位置**：每個 stage 末（預設），加 ScheduleDef 顯式宣告的具名同步點。Scheduler 不自動插入（mechanism §3.4）。

### F.3 事件

**資料模型**：每個 EventTypeId 一份 `EventBuffer`，屬 World，帶 `domain` 與 `zone`。每筆 `{tick, writerPos, seq, payload}`：`writerPos` 是寫者在該 domain 的 **clock schedule** 的 Plan 位置；hook 寫的事件 `writerPos` = 它所屬同步點的 Plan 位置、`tick` = 當下的 domain tick（修訂：S12）；`seq` 是寫入計數。緩衝只存兩個 tick：t 與 t−1。**事件只能在該 domain 的 clock schedule 中讀寫**（修訂：S14；F-9）：同 domain 的其他 schedule（例如 Startup 以外、每 tick 另跑的一份）讀或寫事件＝啟動錯誤 F-E1。理由：原稿「schedule 在 domain 內的序」沒有定義，Host 也沒被約束照它執行；另一個 schedule 在修剪點之後執行就會漏讀。跨 schedule 的傳遞用元件或 Resource。

**寫入**：`EventWriter<E>` 是對 E 的寫入；多個寫者彼此衝突，讀者與寫者也衝突。(c) 要求顯式順序；(b) 依登記序平手。所以「本 tick 讀到還是下 tick」在 (c) 是寫明的。

**讀取規則（無游標）**：位置在 r 的讀者，在 tick t 讀到
`{ e | e.tick = t−1 ∧ e.writerPos > r } ∪ { e | e.tick = t ∧ e.writerPos < r }`，依 (tick, writerPos, seq) 升冪。
- 效果：排在寫者之後的讀者**本 tick 就讀到**；排在寫者之前的讀者下個 tick 讀到；每筆事件對每個執行了的讀者恰好一次。
- 讀者若在某 tick 被 run 條件跳過，那個 tick 應讀的事件就錯過了（寫明；與 Bevy 的游標不同）。需要不漏的讀者不要掛 run 條件，或改用元件。

**修剪**：在該 domain 的 `eventTrim` 同步點（預設每個 tick 最後一個 stage 末）丟掉 `tick = t−1` 的紀錄，等於保留到下一 tick 末尾（mechanism §3.4：不是一律延一 tick）。

**domain**：事件型別登記時屬於一個 clock domain；寫者與讀者都必須在同一 domain 的 schedule 中，否則啟動錯誤 F-E1。理由：Frame 比 Fixed 慢時（一幀跑多個固定步），兩 tick 緩衝會讓 Frame 的讀者漏讀；與其靜默漏讀不如禁止。跨 domain 的傳遞是 Host 交換點的職責（H.7）。

**回滾**：Fixed domain 的事件緩衝屬於快照（H.5）；沒有游標，重跑時讀取規則仍正確。

### F.4 hook

- 種類：`OnAdd<T>`、`OnRemove<T>`。hook 是受限的 System：參數只能是 `HookCtx{entity, component}`、讀取該實體元件的 `Read<…>`（Get 形式）、`CommandsFor<…>`／`Commands`（寫到**下一個**同步窗）、`EventWriter`；不得有 `Local`、不得有 effect（`Diagnostics` 以外）。存取從簽名推出（同 B.4），寫入計入 B.3 的寫者數；zone 受 Z 規則約束，**Z6 對 hook 同樣適用**：`rollbackEnabled` schedule 的同步點上，zone=C 的 hook 寫的欄與 Resource 必須 `rollback = Yes`（不接受 Derived：hook 維護的衍生狀態沒有「每 tick 的寫者」；修訂：S12）。
- **同步執行**：在 F.2 的對應階段立刻執行；同一 (元件, 種類) 的多個 hook 依登記順序。hook 的存取在順序圖上掛在同步點節點，所以與 System 之間永遠有順序（D.2 末段）。
- hook 寫的事件：`writerPos` = 同步點的 Plan 位置（F.3）；因此排在同步點之後的讀者本 tick 讀到、之前的下 tick 讀到，F-7 的「恰好一次」對 hook 寫的事件同樣成立。
- **Restore 不觸發 hook**（修訂：S12）：快照還原是整欄替換，不是結構變更；用 hook 維護的狀態若要跨回滾正確，必須自己在回滾集（上一條 Z6 對 hook 的規則就是為此）。
- (c) 的一致性：同一觸發點的兩個 hook 存取衝突且任一方 zone=C → D-E9。

### F.5 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| F-1 | 延後佇列只在 Plan 的同步點步驟套用；Running 期間 System 看不到本窗任何人（含自己）記錄的結構變更 | J-31 |
| F-2 | 套用順序是 F.2 的 0～8；結果不受各 System **之間**記錄的先後影響；同一 System 內同鍵的多筆依 recSeq；排序鍵 (EntityId, ComponentId, planPos, recSeq) 在一窗內唯一 | J-32：不同 System 執行順序（Plan 重排、無衝突者）、相同記錄集合，World 位元組相同；同一 System 對同一 (e, c) 記兩筆、兩個 hook 對同一 (e, c) 各記一筆，三平台結果相同（golden）；突變：依記錄順序套用、spawn 先於 despawn、不去重、Insert 平手勝者反轉、**丟掉 recSeq 用 `std::sort`**（修訂：B1） |
| F-3 | 每個 stage 末與宣告的同步點都套用；沒宣告的地方不存在同步點 | J-33 |
| F-4 | hook 同步執行、依登記順序；hook 記錄的變更屬於下一個同步窗 | J-34；突變：在本次 Applying 立即套用、hook 反序 |
| F-5 | Applying 期間從 System 路徑記錄延後變更會斷言（只有 hook 能記錄） | J-34 |
| F-6 | 同步點後 `syncEpoch` +1，佇列為空也加 | J-4 |
| F-7 | 事件：tick T 寫入的項目，位置在寫者之後的讀者 T 就讀到、之前的讀者 T＋1 讀到，T＋1 的 eventTrim 之後讀不到；每筆對每個執行了的讀者恰好一次 | J-36（寫者與讀者四種相對順序 × 兩個 tick） |
| F-8 | 讀者讀到的順序＝(tick, writerPos, seq) 升冪 | J-36 |
| F-9 | 事件的寫者與讀者都在該事件 domain 的 **clock schedule** 中（含 hook：所屬同步點在該 schedule 內） | J-37（F-E1） |
| F-10 | untyped `Commands` 只能 insert／remove zone=B 且 Shared 的欄、不能 spawn；延後插入 Exclusive 欄的記錄者必須是該欄唯一的寫者 | J-15（S-11 涵蓋延後寫與 bundle）；J-32b（**常開檢查**，debug 與 release 都測；修訂：S6） |
| F-11 | (c) 的 hook 衝突被拒 | J-34（D-E9） |

### F.6 C++ 實現手段

- 佇列用每個 System 自己的 arena，存位元組與 `{offset, ComponentId}`；bundle 是型別清單（在 `Spawn<Bundle<…>>` 的簽名上，B3），編譯期展開成對 `ColumnInfo::moveConstruct` 的呼叫。
- 排序用 `std::sort` 依明確的元組鍵 (EntityId, ComponentId, planPos, recSeq)；鍵在一窗內**唯一**（F.1 的論證），所以等鍵不存在，`std::sort` 在三個標準庫的結果相同；debug 下排序後斷言相鄰鍵不相等（修訂：B1。原稿的「鍵唯一」對同一 System 的兩筆與兩個 hook 的兩筆不成立，libstdc++、MSVC STL、libc++ 對等鍵的相對順序不同，libc++ 還有 `_LIBCPP_DEBUG_RANDOMIZE_UNSPECIFIED_STABILITY`）。
- 事件 payload 存每型別一個 `std::deque` 或環形緩衝；修剪時從前端彈出。
- hook 以 `HookDesc{component, kind, run, access, zone, registrationSeq}` 存，執行方式同 System 的跳板。

### F.7 Bevy 對照與不採的理由

| Bevy | 證據 | GYO | 理由 |
|---|---|---|---|
| Commands 在 spawn 當下配 Entity、結構變更延後 | bevy_core:32 | 相同（配發器是 Resource，由衝突序列化） | — |
| 套用依拓撲序 index、佇列內 FIFO | bevy_core:34 | F.2 的「階段＋id」，不用 FIFO | 結果不依記錄順序（entity-store DI 4、E-12）；換 plugin 順序不會靜默改結果 |
| 自動插入 ApplyDeferred；schedule 末 apply_final_deferred | bevy_core:33 | stage 末＋宣告的同步點 | 明寫 |
| Messages 雙緩衝，一則最多存活兩次 update | bevy_core:37 | 兩 tick 緩衝、以 writerPos 定讀取 | 效果同（T＋1 末尾），但本 tick 排在寫者之後就讀得到 |
| message_update 在 First；Fixed 側用 signal | bevy_core:38 | 每個 domain 各自的 eventTrim 寫在 ScheduleDef；跨 domain 禁止 | 寫明，不靠 signal |
| MessageReader 是 Local 游標 | bevy_core:39 | 無游標，依 Plan 位置 | 回滾安全；(c) 不靠 System 本地狀態 |
| Observer 同步執行、順序依 EntityHashMap | bevy_core:40 | 不做 Observer（M 節） | 雜湊順序 |
| Hook：on_add／on_insert／on_discard／on_remove／on_despawn，拿 DeferredWorld，同步 | bevy_core:41 | 只做 OnAdd、OnRemove；hook 的變更進下一窗；存取從簽名推出 | 網路與 render 需要的最小集合（networking §1 第 6 條）；避免同一 Applying 內連鎖 |

### F.8 取捨

1. **套用順序「階段＋id」**（採提案 1）對「Plan 位置＋FIFO」（提案 2、3）。提案 2、3 的理由是 id 在套用時才配、記錄時無 id 可排；在本稿的 id 模型下（A.8 取捨 1）id 在記錄時已配好，所以能延伸 entity-store DI 4 到 insert 與 remove，結果與 System 之間的記錄順序無關（System 內部依 recSeq，修訂：B1）。若使用者選 generation 模型，改回提案 2 的順序。階段順序在修訂輪改為 despawn、remove 先於 spawn、insert（修訂：S4），理由見 F.2。
2. **hook 的變更進下一窗**（採提案 1、2）對「同一同步點內做 hook 輪，最多 N 輪」（提案 3）。理由：與 entity-store「a change caused by a spawn or destroy belongs to the next sync point」一致；避免 N 的設定與連鎖。
3. **事件無游標、依 Plan 位置讀**（採提案 2）對提案 1（游標在參數狀態、附遺失計數）與提案 3（游標在 World，以 (System, 通道) 為鍵）。理由：無游標時回滾不需要處理游標、Parallel 下也不需要同步；代價是被 run 條件跳過的讀者會錯過，寫明。
4. 事件保留期三份一致（T＋1 末尾）；跨 domain 禁止採提案 2。

---

## G. 決定性 (c) 區的四處契約

區的定義引用 determinism.md §1（三級）與 §2（區表）：預測核心、host 判斷、契約資料、內容身分為 (c)；中間地帶照 DET-1（E1 落地前明示為 (b)）；邊界外為 (b)。Kernel 不判斷區：區由產品依 determinism.md 宣告在每個 System、元件、Resource、事件上（`zone ∈ {C, B}`），Kernel 檢查宣告之間的一致性。這是 determinism.md DET-3 所說 E4 在 System 上的落點。

**前提**：DET-0 的 E0 還沒找到 ULP 的來源（determinism.md §0、§5）。下列契約是 (c) 的**必要條件**，不是充分條件；改造本身不會消除 win1 的差異（mechanism §4 末段）。

### G.1 區規則（Kernel 的啟動檢查，D-E7）

| 代號 | 規則 |
|---|---|
| Z1 | zone=C 的 System 的**全部存取**都落在 zone=C 的資料上。「全部存取」＝`reads ∪ writes ∪ membershipReads（With／Without）∪ 事件讀寫 ∪ 延後寫（含 bundle）∪ run 條件的 reads ∪ SetRead 展開的 id`（修訂：B2。原稿只寫「讀」，而 `Mut<T>` 讀得到舊值、With／Without 決定疊代集合，兩者都讓 (c) 結果取決於 (b) 資料）。`external` Resource（Link、FrameTime）一律是 B，所以 (c) 碰不到它 |
| Z2 | zone=C 的資料只由 zone=C 的 System 或 hook 寫入（含 bundle 與 `EntityAllotter`）；例外見 Z3。Runtime 的回滾還原與 Host 在 Ready 狀態對 boundary Resource 的寫入不算 |
| Z3 | zone=B 且 `ingress = true` 的 System 可寫 `role = Input` 的 zone=C 資料；它必須位於 ScheduleDef 指定為 ingress 的 stage（schedule 的第一個 stage）；可以讀 Link |
| Z4 | zone=C 的 System：無 effect（S-8）、無 Local（S-7）、無 Changed／Added（S-9）、無 `ExclusiveWorld`（S-13） |
| Z5 | Despawn 一個帶有 zone=C 欄的實體，發出者必須是 zone=C。檢查：套用時**常開檢查**（修訂：S6；despawn 的目標是執行期 id，查該實體有沒有 zone=C 欄與寫入同量級）；finish 時對 (b) System 的 `Despawn` 無法靜態判斷目標，不做啟動檢查 |
| Z6 | `rollbackEnabled` 的 schedule 上，zone=C 的 System 寫的每個欄與 Resource 必須是 `rollback ∈ {Yes, Derived}`（Derived 另受 D-E16 的順序條件）；zone=C 的 hook 寫的必須是 `rollback = Yes`（修訂：S12） |
| Z7 | zone=C 的 System 不得寫 `external` Resource（`ResMut<Link>` 在 (c) 是 D-E7）；由 Z1 的「全部存取」涵蓋，單列是為了 J-41 的案例（修訂：B2） |
| Z8 | `role = Input` 的欄與 Resource 必須 `rollback = No`（修訂：B7）：輸入是以 tick 為鍵的歷史，Rewind 不得抹掉 t0 之後已記錄的輸入，Replay 從它讀 |

這張表把 mechanism §3.3 的「四處契約」化為登記時可檢查的規則；沒有 Z2，一個 (b) 的呈現 System 寫一個 (c) 欄，(c) 就靜默破掉，正是 README -spec 判定要找的反例類型。Z1～Z3 合起來等於提案 1 的 G-9「污染檢查」與提案 3 的 G-2「區的傳播」。(b) System 讀 (c) 資料不受限（呈現要讀模擬），但與 (c) 寫者的衝突照 D.2 第 10 步處理（D.6）。

### G.2 四處契約

| 處 | 契約（可檢查的句子） | 機制裡的落點 | 守衛 |
|---|---|---|---|
| **World** | 疊代依 EntityId 升冪（W-2）；同步點後所有參考失效（W-4）；Running 期間結構變更只進佇列（W-3、F-1）；佇列依 F.2 套用（F-2）；配發器單調、由衝突序列化（W-9）；World 不含雜湊或位址順序（W-10） | A、F | J-2、J-3、J-4、J-8、J-9、J-31、J-32 |
| **Executor** | Plan 唯一（D-1），(c) 歧義是錯誤（D-3）；含 (c) 的 schedule 預設 Serial，Parallel 要逐位元守衛（X-2）；**時間只來自 tick 與被記錄的 boundary 輸入**（修訂：B5／B6）：(c) 區讀時間的來源只有兩個——World 的 domain tick（由 clock schedule 推進，Kernel 以 `Res<Tick<domain>>` 類的句柄提供）與 boundary Resource（zone=C、role=Input，由 Host 在 Ready 寫入、被記錄：`HostClockSample`、產品的 `FixedStep{stepNs}`）。System 宣告寫這兩類是啟動錯誤（boundary 只有 Host 與 ingress 可寫）。`FrameTime` 等牆鐘 Resource 是 `external`、zone=B，Z1 擋住。**本稿在這裡與 mechanism §3.3「Time 只來自 tick」不一致、與 determinism.md DI 4「the tick, or a clock injected」一致**；衝突本身要使用者裁決（K.3 第 11 條）。**亂數**只來自 World 中 `rollback = Yes` 的 (c) 狀態（E-10 的 `Brain.rngState`），種子由產品給並進 digest，Kernel 不提供亂數源 | D、E、G.1 | J-20、J-22、J-27、J-39；J-17 的掃描（`<random>`、`rand`、`random_device`） |
| **Ingress** | 外部訊息只經 Link（I 節）進入；只有 `ingress` System 在 ingress stage 把資料寫進以目標 tick 為鍵的 `InputRing[tick]`（`role = Input`、`rollback = No`，Z8）；(c) 區 System 只讀 `InputRing[目前 tick]`，不碰 Link；到達的牆鐘時機只影響「寫進哪一格、或因過期而丟棄並計數」，不影響 (c) 結果。**到達時刻是資料**（修訂：S5）：IO 執行緒收到每筆訊息時取樣一次時鐘，連同位元組放進 `Link.inbound`（I.2 的 `{arrivalNs, bytes}`）；Ingress 把它當輸入欄位寫進 ring；Host judgement 的 reference age 於是從「到達時刻」算，粒度不變，而且是可記錄、可重放的輸入。(c) 區不回呼 Host 的時鐘。Host 每次喚醒另取樣一次寫進 `HostClockSample`，給需要「現在」的判斷（determinism.md §2：time is an input here, not a read）。Kernel 只能保證「寫入只在一個 stage 發生」；「晚到的輸入寫進已模擬過的 tick 怎麼辦」是產品規則（Match 不回滾只外推，N7）。`rollbackEnabled` 的 schedule 上 ingress System 必須帶 `NotInRollback`（D-E15，修訂：B7），Replay 時 ring 只讀不寫 | Z3、Z8、I 節 | J-40、J-41、J-42；產品守衛：同一份 `InputRing` 內容（含到達時刻欄位）不論實際到達時序如何排列 digest 相同（重放） |
| **Math／編譯** | determinism.md Design Invariant 3：不收縮、不用 FMA 與 ISA intrinsic、不用 fast-math、不用 long double、只用精確的 libm 函式；超越函式來自 Math 自己的實作（E1 之後）；每條 (c) 路徑有三平台 `cpu` golden 或列在 determinism.md §7 | **Kernel 檢查不了**。Kernel 只要求 zone=C System 所在的編譯單位用 GYO 的旗標，並在啟動時輸出「(c) 區 System 的來源檔清單」供 E2 的掃描與審查 | determinism.md 的 E0～E5；J-17 |

### G.3 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| G-1 | 區是宣告：每個 System、元件、Resource、事件都宣告 `zone`，沒有預設 | S-6；J-14 |
| G-2 | Z1～Z8 每一條違反都是啟動錯誤（Z5 是套用時的常開檢查） | J-40、J-41、J-38、J-39、J-14、J-32b；突變：移除每一條檢查、Ingress 豁免擴大到所有 (b) |
| G-3 | (c) 區不用變更過濾：理由是 Match 執行一次與 Client 回滾重跑時「上次執行之後的變更」不同，兩端的 stamp 毫無關係 | J-38 |
| G-4 | Host 寫入的 (c) 輸入（Startup 初始內容、boundary Resource）必須可記錄：產品為它們提供序列化，重放 host 從記錄重現 | 產品守衛（AuthorityDigest 的 scripted clock 就是 `HostClockSample` 的現況形式）；審查 |
| G-5 | 啟動時輸出「(c) 區資料集」與「(c) 區 System 來源檔清單」兩份報告；前者與 `DigestFields` 對照（determinism.md DI 1） | J-17 讀清單 |
| G-6 | 產品的 `FixedStep.stepNs` 用整數；`deltaSeconds` 由它推出只用於呈現。累加器屬於 Host「這一幀跑幾個 tick」的邏輯（(b) 的時序），不進 (c) 的資料；Host 的累加器計數（`FixedTickRuntime::tickId_`）不是 domain tick 的來源，只是 Host 自己的簿記（debug 可與 World 的 tick 比對；修訂：B5） | 現況對照：`FixedTickRuntime` 以 double 累加並傳出 `deltaSeconds`（`FixedTickRuntime.hpp:19`、`:44`），`PvpMatch::Tick` 以容差 1e-12 檢查它（`PvpMatch.cpp:522-523`） |

### G.4 現況中的違反（交 -audit）

- `PvpMatch::Tick` 接收 `ShotReferenceAge` 回呼（`Combat.hpp:228`），Host 在 Tick 內以 `now_()` 計算（`MatchRuntimeHost.cpp:441-445`）。注入的時鐘讓它在 digest runner 中可重現，但在機制中必須改成以 `Link.inbound` 的到達時刻為輸入、在 Ingress 算好進 ring（G.2 Ingress 列）；否則射擊裁決得標成 (b)。
- **連線品質的 reference age**（修訂：S5）：今天在 IPC 執行緒的 `SubmitInput` 裡以到達當下的 `now_()` 計算（`MatchRuntimeHost.cpp:161`、`:218`、`:222`），存進 `quality_`（`:262-265`）；`JudgeConnectionQuality`（`:700-741`）取中位數判 late 與逐出（進 digest，`host-poor-connection` golden）。機制下：到達時刻隨訊息進 `Link.inbound`，Ingress 寫進 ring 的輸入紀錄；Judge stage 的 (c) System 從 ring 讀到達時刻與 `HostClockSample` 算年齡。粒度仍是到達時刻（不是喚醒時刻），所以 golden 可望不變；差異只在「IO 執行緒直接寫權威的 `quality_`」改成「資料經 Link 與 ring」。-audit 確認 golden。
- `StepMovement` 的呼叫路徑經全域的 `movementDiagWriter`（`MovementDiag.hpp:188`，atomic 指標）與 `MovementTrace`（`MovementTrace.hpp:59` 讀 `steady_clock`，`:100` 全域 mutex），違反 I-3 第 3 條；要改成宣告的 `Diagnostics` 副作用（System 經句柄取得 sink）。

### G.5 不解決的部分

- 序列化只是必要條件之一（mechanism §3.3；PR #177：今天就是單執行緒，仍有 1～4 ULP 差）。Kernel 完成後 DET-0 仍在，E0 仍要做。
- zone 的標示正確與否 Kernel 驗不到：產品把該標 C 的 System 標成 B，Z 規則不會攔。守衛：determinism.md DI 1（邊界寫下來）＋審查。
- Z 規則只看宣告；函式體經全域變數讀到 (b) 資料攔不到（B.5）。

### G.6 Bevy 對照

| Bevy | 證據 | GYO |
|---|---|---|
| 固定步累加器是 Duration 整數，但 delta 來自牆鐘；`overstep_fraction` 是 f32 | bevy_core:17、:74 | domain tick 是整數、步長 `FixedStep.stepNs` 是整數；累加器在 Host（G-6） |
| 變更 tick 是 System 執行次數，不能當協議序號 | bevy_core:52、:73 | stamp 不進 (c)（G-3）；複製層用 `tickStamps` 換算 |
| 決定性靠關 feature 或手動設 SingleThreaded | bevy_core:22 | 預設 Serial，Parallel 要守衛（X-2） |
| lightyear 收包時呼叫 `Instant::now()` | blueprint DI-K6 列（證據在 lightyear.md，不在 bevy_core.md） | Host 交換點取樣一次（`HostClockSample`） |

### G.7 取捨

1. 區規則採提案 2 的 Z1～Z5 為骨架，併入提案 1 的 G-9（污染檢查）與提案 3 的 G-2～G-5（區的傳播、external 隔離、boundary 寫入點、Host 注入時鐘）。三者是同一條規則的三種寫法，合併後沒有衝突。
2. `rollback` 屬性與 Z6 採提案 3；提案 1 以 `rollbackSet` 由產品宣告，等價，但放在元件屬性上才能做 Z6 的靜態檢查。
3. 時間的形狀（修訂：B5）：tick 是 World 的 domain tick（Kernel 概念，clock schedule 推進，在快照內）；步長是產品的 boundary Resource（說明名 `FixedStep{stepNs}`，只有 Host 寫）。三份提案的 `FixedTime`／`Tick`＋`StepLength` 都把兩者放在一個 Resource 裡並由 Host 寫 tick；本稿拆開，因為 tick 必須與事件緩衝、tickStamps 一起還原，放在 Resource 裡會變成第二個來源。

---

## H. 多 World 與 schedule 的組合

### H.1 名詞

- **Runtime**、**Host**：§0。
- **clock schedule**：推進 World 某個 domain 的 tick 的 schedule（每個 World 每個 domain 恰好一個，H-1）。
- **交換點**：Host 在兩次 schedule 執行之間、World 處於 Ready 時，把資料搬進或搬出 World 的位置。這是唯一允許上鎖的地方；World 內不上鎖。

### H.2 兩個時鐘

- **FixedUpdate**（Fixed domain）：由 Host 的固定步累加器驅動（沿用 `FixedTickRuntime` 的語義：最多追 `maximumCatchUpSteps` 步，`FixedTickRuntime.hpp:14`、`:44`）。累加器給出 k 步，Host 就呼叫 k 次 `Run(FixedUpdate)`；每次 `Run` 的第一步把 World 的 `tick[Fixed]` +1（修訂：B5；Host 不寫 tick）。(c) 區只讀 domain tick 與 boundary（G.2）。
- **FrameUpdate**（Frame domain）：每幀一次。Host 寫 `FrameTime{frameIndex, deltaSeconds}`，旗標 `external`；(c) 不可讀（Z1）。
- 兩者可共用同一 World（Bevy 的做法），前提是同一條執行緒（H-2）。共用時 Frame 寫者寫 Fixed 讀者讀的資料必須經 Ingress（Z3）。
- 兩個時鐘在不同執行緒時，必須是兩個 World 加 Host 的交換點（H-2）。跨 domain 的資料不靠 Scheduler 自動同步（v0.2 §10.1）。

### H.3 Match 與 Client 共用 FixedUpdate

- 同一組 (c) plugin 登記同一組 (c) System（mechanism §4 第 3 點）；Client 多出的 Rollback 與 PredictionHistory 在 FixedUpdate 之外（zone=B）。
- Registry 的 `cFingerprint`（只含 zone=C 的部分與 FixedUpdate 的 Plan）：Match 與 Client 若宣稱「同一份模擬」，兩端的 `cFingerprint` 必須相同（H-4）；是否在 Welcome 時比對並拒絕，由產品決定（比照 AnimationContentDigest 在 join 時比對，determinism.md §4.9）。
- 因為 EntityId 是契約鍵（A.1），兩端的疊代順序對同一組實體相同，不需要每個 System 自己排序。

### H.4 三個 host 的組合（現況 → 改造後；行為差異交 -audit）

#### H.4.1 Match host（headless 權威）

現況：主執行緒每 50 ms 醒一次、寫統計、處理關機訊號，最後 IPC 先停再停 host（`match_main.cpp:170`、`:194`）。`gyo-match-sim` 執行緒每次喚醒 `Advance → Step`，整個 `Step` 持有 `mutex_`（`MatchRuntimeHost.cpp:371`），在回呼裡依序：join／leave → ack → 動作 → 輸入交接 → `match_.Tick`（`:441`，帶 `now_()` 的回呼）→ `TrackSlack`（`:450`）→ `JudgeConnectionQuality`（`:451`）→ 每 `SnapshotIntervalTicks` 發布（`:471`）；`Body`（`:513`）的牆鐘迴圈用絕對 deadline 等待。`PvpMatch::Tick`（`PvpMatch.cpp:520`）內部：`ResolveLifeBoundaries`（`:526`）→ 每個 body `StepMovement`（`:571`、`:582`）→ `ResolveActions`（`:617`）→ `AdvanceAnimation`（`:623`）。`gyo-match-ipc` 執行緒同步呼叫 `host.SubmitActionBatch(…, &refusal)`（`IpcHost.cpp:685`），IO 執行緒直接碰權威狀態。

改造後：

| 項 | 內容 |
|---|---|
| Runtime | `Match`：World M，runner＝`gyo-match-sim` 角色 |
| schedules | `Startup`（一次）；`FixedUpdate`（clock、Serial） |
| FixedUpdate 的 stages（中性名；產品掛 System） | `Ingress`（Z3，zone=B：Link 收件 → **join／leave 請求、ack、動作、輸入、到達時刻**全部以資料寫進 `InputRing[tick]`；Ingress 不生成、不刪除實體，修訂：S3）→ `Simulate`（(c)：**`ApplyMembership`**——讀 ring 的 join／leave 請求，以 `SpawnWithId<Bundle<Body, Combat, Animation, Connection, …>>` 生成玩家（id 由 Gateway 配，E-2）、以 `Despawn` 刪除（它是 zone=C，Z5 允許）；→ 生死邊界 → 移動 → 動作裁決 → 動畫推進；順序由 before／after 宣告，是 (c) 所以歧義即錯誤）→ `Judge`（快照組裝、連線品質、修剪；Host judgement 區讀 ring 的到達時刻與 `HostClockSample`）→ `Egress`（run 條件 `tick % N == 0` 寫快照；寫 Link 送件） |
| Host 的每次喚醒 | (1) 取樣時鐘一次，寫 `HostClockSample`；(2) 交換點 A：swap IO 執行緒的收件緩衝（每筆帶到達時刻）進 `Link.inbound`；(3) 累加器給出 k 步，呼叫 k 次 `Run(FixedUpdate)`（tick 由 Run 推進）；(4) 交換點 B：swap `Link.outbound` 給 IO 執行緒並 Notify；(5) 以 `secondsUntilNextTick` 算絕對 deadline，用 `Waiter` 等（同今天 `Body`） |
| 外部服務 | IPC（Gateway 的 loopback TCP）：IO 執行緒只處理位元組與到達時刻；trace writer |
| 行為差異（-audit） | (a) `ShotReferenceAge` 與連線品質的 reference age 改以 ring 內的到達時刻計算（G.4；粒度不變，golden 預期不變，待 -audit 確認）。(b) IPC 執行緒同步拿到的 refusal（`IpcHost.cpp:685`）改由下一個 tick 的 Ingress System 產生，回覆時機會變；pv9 契約有沒有規定回覆時機**未查證**（本輪 grep `network-architecture.zh-Hant.md` 的 refusal 無結果）。(c) 一次 Advance 內的追趕步拿不到新收件：與今天相同。(d) Join／Leave 從「Tick 外由 host 呼叫」（entity-model §5.2 第 237 行）變成 tick 內的 (c) System（修訂：S3）：生成與刪除晚最多一個 tick、進快照與 digest 的 tick 可能差一；`combat-rejoin` 等 golden 要重核 |

#### H.4.2 Client host（視窗、預測）

現況：主執行緒跑 `RuntimeLoop`（`PvpApplication.cpp:1383`）：`ProcessEvents` 呼叫 SDL pump，期間 live frame 可能重入（`:1389-1401`）；`Update` 做資產 `BeginFrame`（`:1141`）、輸入（`:1111-1119`）、UI、`PublishIntent`（`:1172`）、`PresentAt` 內插（`:1173`）；`Render`。`gyo-client-sim`（`ClientSimulationRole.cpp:151` 的 `RoleThread::Start`）依自己算的 deadline 跑預測步，以 `mutex_` 與主執行緒交換意圖與呈現（`:161-211`）。asio 的 IO 執行緒；音訊回呼執行緒；trace writer。

改造後：

| Runtime | World | schedules／runner | 內容 |
|---|---|---|---|
| `ClientSim` | S | `FixedUpdate`（clock、`rollbackEnabled`、Serial，runner＝`gyo-client-sim`） | `Ingress`（全部帶 `NotInRollback`，D-E15）：mailbox 的意圖 → `InputRing[tick]`（role=Input、rollback=No）。權威快照**不走 System**（修訂：S2）：Host 在交換點把 Link 收到的權威快照解碼成 `Restore` 用的部分快照（含 spawn／despawn 的成員變化），比較 PredictionHistory[T]，不一致就 `Restore(world, 權威快照@T)` 再 `Replay(T+1 → tNow)`；一致就丟棄。這就是今天 `LocalPlayerPrediction::Reconcile` 的注入點。`Simulate`：**與 Match 同一份 System**（`ApplyMembership` 在 Client 讀不到 join 請求，而權威的成員變化經 Restore 進來；cFingerprint 相同，H-4）。`Egress`：輸入視窗 → Link 送件（run 條件 `NotInRollback`）；呈現快照 → mailbox。PredictionHistory 由 Runtime 在每次 schedule 執行後自動存，不是 System（H.5） |
| `ClientFrame` | F | `FrameUpdate`（clock＝frameIndex，runner＝Main） | `Platform`（boundary：Host 寫 `PlatformEvents`）→ `Input` → `UiAndLobby` → `Intent`（寫 `IntentOut`）→ `Present`（讀 mailbox 的 `PresentationIn`，內插與命中回饋）→ `Audio`（effect `Device:audio`，親和＝擁有 Submitter 的角色） |
| `ClientRender` | R | `Extract`（從 F 唯讀）→ `RenderSubmit`（親和 `GraphicsContextOwner`），同在主執行緒 | `RenderQueue`、裝置、`Renderer` 是 R 的 Resource |

- **交換點**：意圖 F → S、呈現 S → F，經 Host 擁有的 `Mailbox<T>`（latest-wins 或有上限的佇列），只在 schedule 之間存取。Link 經 Host 與 asio 執行緒在 S 的 schedule 前後交換。
- **時鐘策略**：今天模擬執行緒的 deadline 含相位與 re-anchor 判斷（`ClientSimulationRole.cpp`、`LocalPlayerPrediction::TrackPhase`）。「何時跑下一步」是 Host 的時鐘協調；放在 Host 還是 S 裡交 -audit；本稿立場：不是 System。
- **live frame**：Host 的 live frame 處理器執行 F 的 FrameUpdate，`LiveFrame` 為真使 `Input`、`Intent` 被 run 條件跳過（今天 live frame 看不到輸入，`PvpApplication.cpp:1139`），然後跑 R。
- **F 與 R 合併的選項**：R 獨立是 blueprint R1 與 DI-K7 的形狀；代價是多一個 World 與一次 extract。合併則 DI-K7 只能靠 `RenderSubmit` 只讀的存取宣告守。**要使用者選；本稿推薦獨立。**

#### H.4.3 Editor host（以 voxel_map_editor 為例）

現況：`EditorClient : Framework::ImGuiShellClient` 在主執行緒；`EditSession` 有 Undo、Revision、`TakeChanges`；預覽用離屏 Renderer 加 readback；ImGui 有全域 context（`ImGuiShell.cpp:225`、`:267-269`）。

改造後：`Document` Runtime（World D，`FrameUpdate`，主執行緒）：文件與 `EditSession` 先當 Resource（要讓編輯工具成為 System 時再拆成實體與元件，交 -audit）；存檔與載入是有 `Io:file` 副作用的 System，經資料契約 `gyo.voxel_map` 讀寫（AGENTS §8）。`Preview` Runtime（World R，`Extract` 從 D 唯讀 → `RenderSubmit` → `Readback`）；ImGui 的 System 都宣告 effect `Gpu:imgui`，彼此序列化。全部 zone=B，依登記順序平手，不需寫順序。文件 World 不是產品 Runtime 的 World，Runtime 不知道編輯器存在；工具知識留在工具。「編輯器直接重用 Engine 的 System」（mechanism §7）＝在編輯器多建一個 Match 形狀的 Runtime（play-in-editor），不在 v1，機制允許。

#### H.4.4 測試 host（驗證的主路徑）

- **重放 host**：建 Match Runtime，用腳本時鐘與記錄的 boundary 內容驅動 FixedUpdate，輸出 digest。這是今天 `AuthorityDigest` 執行器的位置（determinism.md §2「the digest runner injects a scripted clock」）。
- **等價 host**：同一組 System，Serial 對 Parallel、原順序對排列順序、直接跑對回滾重跑，各比對 World 摘要（J-44、J-27、J-42）。

### H.5 固定步重入（回滾）

- **操作**（Runtime 提供、Host 呼叫）：`Rewind(world, t0)`（來源 PredictionHistory[t0]）或 `Restore(world, snapshot)`（來源 Host 提供的快照，可部分；修訂：S2），加 `Replay(world, schedule, → tNow)`。只能在 `Ready` 狀態、不在任何 System 內呼叫（巢狀執行同一 schedule 是 debug 斷言 H-A1）。
- **快照內容**（PredictionHistory 的每格）：**該 domain 的 tick**（修訂：B5）；所有 `rollback = Yes` 的欄（keys、值、stamps）與 Resource；配發器狀態（否則重跑時 spawn 得到不同 id）；Fixed domain 的事件緩衝（t 與 t−1）。不含：stamp 計數器（單調前進；Restore 寫回的格以目前 stamp 標 `changed`，讓 extract 與複製看得到）、Local（(c) 不得有，S-7）、`role = Input` 的資料（Z8：輸入歷史不回滾）、`rollback = Derived` 的欄（D-E16 保證重算先於讀）。
- **部分快照**（Host 從權威資料組出）：一組 `rollback = Yes` 的欄的（keys, 值），只含權威送來的實體與欄；Restore 時這些欄的成員集合**整欄替換**成快照的集合（權威沒送的實體在這些欄被移除，相當於 despawn 的成員效果；沒在快照裡的欄不動）。這是 Client 把權威狀態寫進 (c) Exclusive 欄的唯一路徑：Restore 不算寫者（B.3）、不受 Z2（Z2 的例外），而且不觸發 hook（F.4）。
- **PredictionHistory**：Runtime 在 `rollbackEnabled` 的 schedule 每次執行後存一份快照（以 tick 為索引、有上限的 ring；上限由登記決定；比照 lightyear 的預測歷史，N8）。
- **Rewind／Restore**：還原快照的全部內容，包括 `tick[Fixed]`（設回 t0）與事件緩衝；`tickStamps` 中重跑的 tick 以新值覆寫；`InputRing` 等 role=Input 資料不動。
- **Replay**：設 `RollbackActive = true`（boundary Resource），逐步 `Run` 直到 `tick[Fixed] == tNow`（每次 Run 推進 tick，所以 Replay 不另算步數）。`ingress = true` 的 System 全部被 `NotInRollback` 跳過（D-E15 強制），ring 只讀不寫；Host 在 Replay 期間不寫任何 boundary（Replay 是一次 Host 呼叫，中間沒有交換點）。
- **與今天的差距**：`LocalPlayerPrediction::Reconcile` 只重放本地玩家的待確認命令（determinism.md §3）。機制是一般化的；v1 的回滾集合可以只登記本地玩家的欄，部分快照也只含本地玩家。落差交 -audit。

### H.6 extract（主世界 → 渲染世界）

- 渲染 World 有自己的 Registry（各自凍結；render plugin 只登記到渲染 Registry）。extract System 在渲染 Runtime 的 `Extract` schedule 中，參數 `Extract<Read<T>>`／`ExtractRes<R>` 以 const 方式讀來源 World，存取記在描述中（種類「來源 World 的讀」）；對渲染 World 可讀寫。
- 來源 World 在 extract 期間 `Ready`、且與目標在同一執行緒（v1）。
- 來源 Registry 必須先凍結，目標在 finish 時才解析來源 id；順序錯了是啟動錯誤（Host 必須依依賴順序建 Runtime）。

### H.7 Host 保留的職責（IoC 消不掉也不該消；mechanism §2 末段、§6）

| 職責 | 為什麼不交給 Scheduler | 今天在哪裡 |
|---|---|---|
| 建 World、選 plugin 清單（有序）、提供 ScheduleDef（stage、同步點、executor、runner）、指定 Executor 策略 | 組合是資料，由 Host 給 | — |
| 多時鐘協調：固定步累加、追趕上限、Client 的相位策略、絕對 deadline 等待、這一幀跑幾次 FixedUpdate | Scheduler 不懂時間域（v0.2 §10.1、§13 第 3 問） | `FixedTickRuntime`；`MatchRuntimeHost::Body`；`ClientSimulationRole.cpp:151-211` |
| Runtime 的建立、啟停、停止順序（IO 先停）；**世代的切換**：Match 的 Reset＝銷毀 World、以同一份凍結 Registry 重建（A.1；修訂：S4） | 生命週期跨 Runtime；World 內沒有 Reset | `match_main.cpp:194`；`PvpMatch::Reset()`、`IpcHost.cpp:478-489`、`:545-553` 的 `RequestReset` |
| 權威快照的解碼與 `Restore`／`Replay` 的呼叫（Client） | 在 schedule 之外；是 (c) 的輸入，要可記錄 | `LocalPlayerPrediction::Reconcile` |
| 執行緒的建立與角色綁定；RoleThread 的停止順序 | 執行緒是 OS 資源 | `RoleThread::Start` 的各處呼叫；`RoleThread.hpp:62` |
| IO 與 World 的交換點（Link 換入換出、mailbox、`PlatformEvents`、`HostClockSample`） | 只有這裡可以上鎖；World 內不上鎖 | `MatchRuntimeHost::mutex_`、`ClientSimulationRole::mutex_` |
| 平台 pump 與 live frame | pump 進入 OS 的 modal loop，期間要執行 frame schedule；pump 在 schedule 之外 | `PvpApplication::ProcessEvents`；`RuntimeLoop::RunLiveFrame`（`RuntimeLoop.hpp:31-37`） |
| 回滾的觸發（何時 Rewind 與 Replay、k 的大小） | 在 schedule 之外 | `LocalPlayerPrediction::Reconcile` |
| Runtime Error 與 `Interrupted` 的處置（結束碼、記錄） | 行程層級的策略 | `match_main` 讀 `runtime.Error()` |
| 診斷視窗的彙整 | 外部輸出、與 tick 無關 | `match_main.cpp:170` 附近的統計行 |

### H.8 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| H-1 | 每個 World 每個 domain 恰好一個 clock schedule；World 的 `tick[domain]` 由它的 `Run` 推進，是該 domain「現在的 tick」、事件 tick、`tickStamps` 的唯一來源；Host 與 Resource 都不另記 tick；同一 World 上同一 domain 的其他 schedule 使用當下的 tick，不讀寫事件（F-9） | finish 計數（D-E12）；J-42 的突變「不還原 domain tick」 |
| H-2 | 同一個 World 的所有 schedule 綁同一條執行緒（`ownerThread`）；兩個時鐘在不同執行緒必須是兩個 World | finish 比對 runner（D-E13）；J-15 |
| H-3 | 一個 System 的親和角色必須等於它所屬 schedule 的 runner 角色；headless 的 Match 沒綁 `Main` 與 `GraphicsContextOwner`，需要它們的 System 一登記就啟動失敗（blueprint §8 場景 3 的機制形式） | J-28、J-17b |
| H-4 | Match 與 Client 宣稱「同一份模擬」時，兩端 `cFingerprint` 相同 | 產品守衛：兩個 Runtime 的組合各自產生指紋並比對 |
| H-5 | 回滾等價：從狀態 s 直接跑 k 個 tick，與「跑到 tNow、Rewind 到 t0、Replay 回 tNow」所得 World 的回滾集合位元組逐位元相同（stamps 除外）；ingress 在兩種跑法中寫進 ring 的內容相同 | J-42；突變：還原時漏掉事件緩衝、漏掉配發器、不還原 domain tick、ingress 在 Replay 中執行、InputRing 標成 rollback=Yes |
| H-6 | extract 對來源只讀：`Extract<Write<…>>` 不存在（編譯期擋下）；debug 下來源 World 在 extract 期間任何寫入都斷言；渲染 Runtime 不得以任何參數寫回來源（含 boundary Resource）；來源 Registry 先凍結 | J-43、J-16；編譯失敗測試 |
| H-7 | Host 不決定 System 順序：已遷移的 host 只呼叫 `Run`、`Rewind`／`Replay` 與交換點，不依序呼叫模組的 `Update`／`Tick`；一個 host 不會一半在機制上（不 include `IRuntimeClient.hpp`） | 文字掃描 host 的原始碼（清單由 -audit 產出）；審查（mechanism §7、v0.2 §10.4） |
| H-8 | `rollbackEnabled` 的 schedule 上，有非 `Diagnostics` 外部副作用的 System 必須有 `NotInRollback` 條件或宣告 `rollbackSafe`，否則啟動錯誤 | J-42b |
| H-9 | Rewind／Restore／Replay 只能由 Host 在 Ready 狀態呼叫；在 System 內呼叫斷言 | J-42 |
| H-10 | `rollbackEnabled` 的 schedule 上，每個 `ingress = true` 的 System 都有 `NotInRollback` 條件（不能以 `rollbackSafe` 代替：ingress 寫 ring，就是會改歷史）；`role = Input` 的資料 `rollback = No`（Z8）（修訂：B7） | D-E15、D-E7；J-42 |
| H-11 | Restore 不觸發 OnAdd／OnRemove；Restore 寫回的格 `changed` 標目前 stamp（修訂：S12） | J-42c |

### H.9 C++ 實現手段

- Runtime 是普通物件：`vector<Plan>`、一個 World、一個 Registry；不是框架。多個 Runtime 的順序由 Host 程式明寫。
- `Restore` 對每個欄呼叫 `destroy` 再用 `copyConstruct` 從快照複製；trivially copyable 時 `memcpy`。
- 專用執行緒與等待：`Engine::Threads::RoleThread`、`Engine::Time::Waiter` 原樣使用（threads、time 的契約不變）。
- Mailbox 與 Link 的佇列：雙緩衝 `vector<byte>`，Host 在交換點 `swap`，不鎖 World。

### H.10 Bevy 對照與不採的理由

| Bevy | 證據 | GYO | 理由 |
|---|---|---|---|
| MainScheduleOrder 是 Vec 可插入；FixedMain 有自己的順序；RunFixedMainLoop 以 while 迴圈跑 | bevy_core:16、:17 | ScheduleDef 由 Host 給；stage 可插入；固定步迴圈在 Host | plugin 不改 schedule 序列，只掛 stage |
| 累加器 Duration 整數但 delta 來自牆鐘；`TimeUpdateStrategy` 可手動推進 | bevy_core:17 | 累加器在 Host；domain tick 由 clock schedule 的 Run 推進，Host 只決定呼叫幾次 | G.2 |
| SubApp 的順序由 HashMap 決定 | bevy_core:12 | 多個 Runtime 的順序由 Host 明寫 | 決定性 |
| Extract 把主 World 換成 MainWorld Resource；ExtractSchedule 關掉自動 apply_deferred | bevy_core:13 | 型別化的 const 視圖，不搬移 World | 簡單；DI-K7 |

### H.11 取捨

1. **一個 World 一條執行緒、兩時鐘異執行緒即兩個 World**（提案 3）；提案 1 留給 -audit，提案 2 未明寫。採提案 3，理由見 A.8 取捨 4。
2. **Restore 後的 stamp**：提案 2「以目前 stamp 標 changed」對提案 1「保留舊 stamp」。採提案 2：extract 與複製要看得到還原；回滾等價（H-5）比對的是回滾集合的位元組，不含 stamps（stamps 不是 (c) 資料）。
3. **PredictionHistory 由 Runtime 自動做**（提案 3）對提案 1 的 Host 呼叫 `Snapshot`：採提案 3，因為快照時機（每次 schedule 執行後）是機制的一部分，由 Host 做會漏。
4. 渲染 World 自己的 Registry（提案 2 建議兩個 Registry）：採兩個，理由是 render plugin 只登記到渲染 Registry，主 Registry 不因渲染而變 fingerprint。

---

## I. Runtime 邊界與葉子模組

### I.1 外部系統（不持有、不修改 World）

| 外部系統 | 執行緒 | 與 World 的介面 |
|---|---|---|
| Gateway（Go，另一行程） | — | 訊息邊界：Match 的 IO 執行緒與 Link |
| Match 的 IPC 執行緒 | `gyo-match-ipc` | 位元組佇列；Host 在交換點換入換出 |
| Client 的 asio 執行緒 | `gyo-client-net` | 同上 |
| SDL 平台 | 主執行緒 | Host 的 pump 寫 `PlatformEvents`（boundary）；pump 有 `thread_local` 狀態（`SdlPlatform.cpp:61`、`:73`、`:75`），不能當葉子 |
| GPU 裝置 | 建立它的執行緒（提案 3 引 `SdlGpuRenderDevice.cpp:122-150`，統合者**未複核**） | 渲染 World 的 Resource，親和 `GraphicsContextOwner` |
| 音訊 Mixer 的回呼執行緒 | SDL 音訊執行緒 | `Submitter` 是 frame World 的 Resource；擁有者是第一個呼叫它的執行緒（audio.md），使用它的 System 綁同一角色 |
| Trace writer、LogFile | 各自的 RoleThread | `Diagnostics` 副作用；只寫不讀回 |

### I.2 Link 是 Resource

```text
Link（Resource，zone = B，external）
├── inbound  : vector<{arrivalNs, bytes}>（外部執行緒在收到時取樣一次時鐘、連同位元組填入另一半緩衝；交換點 swap 進來。修訂：S5）
├── outbound : 位元組佇列（Egress System 填入，Host 在交換點交給外部執行緒）
└── state    : 連線狀態（connected、epoch、計數）
```

Gateway → 外部執行緒 → `Link.inbound` →（交換點）→ Ingress System → `InputRing[tick]` → (c) 模擬 → Egress System → `Link.outbound` →（交換點）→ 外部執行緒 → Gateway。每個 peer 一個或多個 peer 共用，由產品決定。Link 內部的同步原語是 Link 的實作細節；Kernel 規定的可檢查句子（修訂：N3）：**`Running` 期間沒有外部執行緒存取 `Link.inbound`／`outbound` 的 System 側緩衝；外部執行緒只寫另一半；兩半只在交換點（World `Ready`、`ownerThread` 上）swap**。debug：Link 帶「目前持有者」標記（System 側／外部側），存取時比對執行緒。對應 N10（transport＝位元組加連線狀態）、mechanism §4 第 4 點。`arrivalNs` 是到達時刻的取樣，進 ring 後是 (c) 的輸入（G.2 Ingress 列）。

### I.3 葉子「留」的三個否定條件（可掃描）

一個模組要同時滿足三條，才原樣作為函式庫、由 System 的函式體直接呼叫：
1. **沒有生命週期**：沒有需要與其他模組排序的 `Create`／`Start`／`Stop`／`Init`，不持有 OS 資源（執行緒、socket、裝置控制代碼）。檢查：公開 API 與成員掃描。
2. **沒有更新迴圈**：沒有預期每幀或每 tick 被呼叫的函式（`BeginFrame`／`Update`／`Tick`／`Advance`／`Pump`／`Poll`），不擁有執行緒。檢查：函式名掃描加 `std::thread`／`RoleThread`／`std::mutex`／`std::atomic` 掃描。
3. **沒有全域狀態**：沒有命名空間範圍或函式內 `static` 的可變變數、沒有 `thread_local`、不經單例或第三方的全域 context、不讀時鐘或亂數。檢查：文字掃描（照 entity-store 守衛列 5、6、8），**掃描項必含 `<chrono>`、`<random>`、`steady_clock`、`random_device`**（修訂：S17：L.1 第一版只掃執行緒，漏了 `WriteFileAtomically` 的時鐘與亂數）。

「包」與「重寫」的分界（v0.2 §11 的契約可靠性；修訂：S8，採 spike〈對「中間層包或重寫」的意義〉）：原本的問法「實際讀寫能不能列成有限的 World id 集合加宣告的 effect」在 C++ 裡不可直接檢查，改成三個可檢查的問題：(i) 模組是否**持有跨 tick 的指標或參考**指向 World 資料（成員指標、快取的參考）；(ii) 是否有**全域可變狀態**（第 3 條的掃描）；(iii) 是否**自己起執行緒碰 World**。三者皆否＝**包**：狀態變 Resource、更新變 System、生命週期交 Host 或 plugin，內部不改；包起來的契約可以靠 A.6 的兩層 debug 核對守。任一為是＝契約只能靠審查，判斷偏向**重寫**那一層介面，或先拆掉那些狀態再包。

### I.4 不變量

| # | 不變量 | 檢查 |
|---|---|---|
| I-1 | Kernel 不提供任何把 `World*`／`World&` 交給 Runtime 外部的 API；句柄生命期只在一次 System 呼叫內；外部執行緒碰 World 由 W-6 斷言 | API 形狀；J-15 |
| I-2 | 外部系統的狀態（socket、裝置、writer）不是 World 的 Resource；World 只放交換用的資料（佇列、狀態值） | 審查（v0.2 §8.2） |
| I-3 | Link 是 Resource 不是 System；讀寫 Link 的 System 必須宣告 ingress 或 egress（effect `net`），而且是 zone=B | Z1、Z3；J-41 |
| I-4 | `Running` 期間外部執行緒不存取 Link 的 System 側緩衝；兩半只在交換點 swap；交換點只在 World Ready 且在 ownerThread 上（修訂：N3） | debug 的持有者標記（I.2）；Kernel 不提供 Link 的鎖 API；W-6；審查 |
| I-5 | log 與 assert 的全域狀態（`Log.hpp:43` 的 `logHandler`；`Assert.hpp:49` 的 `assertionHandler`、`:50` 的 `thread_local reportingAssertion`；修訂：S17）是明列的例外：只寫出、不回饋到 World | J-17 的白名單；審查 |
| I-6 | 「留」的判定附掃描結果（檔名、行號）；任一條不符就進「包或重寫」的判斷 | -audit 的判準 |

### I.5 Bevy 對照

- OS 事件與網路收發在幀邊界批次進入：blueprint §2 的 O2、P1，證據在 `os_io.md`、`platform.md`，不在 bevy_core.md。
- Bevy 的 IO 與網路 crate 只放「位元組＋連線狀態」到 ECS（blueprint O5；networking N10）。GYO 同；差別是 GYO 的 OS adaptor 集中（O1）。
- bevy_core.md 沒有 Link 的對應項目，**未查證**。

### I.6 取捨

三份一致。提案 3 的外部系統表、提案 2 的「包／重寫分界」、提案 1 的 I-5 例外合併。

---

## J. 不含遊戲型別的驗證

### J.1 原則

- 測試放在 `tests/common/kernel/`（名稱候選），CTest 標籤 `cpu`，L1 三平台（linux-x64、windows-x64、macos-arm64）都跑。
- 測試型別（`TestPos`、`TestVel`、`TestMarker`、`CountingDtor`、`TestEvent`、`TestRes`）只定義在測試中；Kernel 的標頭與原始碼不出現任何遊戲型別。
- 測試執行檔只連 Kernel 加 `gyo_test_support`，並以 `gyo_expect_links` 在 configure 時檢查（照 entity-store 守衛列 9）。
- 規則測試依本規格撰寫（AGENTS §14.2），每條用突變證明檢出力；「killed」只算預期的測試失敗（memory portable-verification）。
- 共通 Test 自己宣告需要的機能，不依賴特定 Product（AGENTS §7）。

### J.2 守衛測試清單

| # | 測試 | 守的不變量 | 突變（必須被殺） |
|---|---|---|---|
| J-1 | 同步點之後 keys 嚴格升冪、各陣列同長、`entities` 是超集合 | W-1 | 插入改 append；刪除改 swap-and-pop |
| J-2 | 兩條增刪歷史不同、最後成員相同的 World，單欄與三欄查詢的疊代序列逐一相同 | W-2、G.2 World | 疊代改依插入順序；交集由第一欄驅動卻不依 id 合併 |
| J-3 | Running 期間對欄做結構修改會斷言（`GYO_CHECK_ASSERTS`）；Spawn 改成立即套用被抓到 | W-3 | 移除 epoch 斷言 |
| J-4 | 把句柄存到 Local 或 static：**編譯失敗測試**（句柄不可複製、不可移動；修訂：S8）；從句柄取出的 `const T&` 存到 Local、下一次執行時 `const_cast` 寫：debug 在 System 邊界斷言；只讀：不攔，寫成 Limit 測試固定現況；跨過空佇列的同步點後經 debug 句柄的 epoch 比對斷言 | W-4、F-6 | 句柄可複製；移除邊界比對；空佇列不加 syncEpoch |
| J-5 | README `-spike` 判定 (2) 的**原文三種繞路**（修訂：S7；不改寫事前宣告）：(a) 存參考——在 System A 由 `Read<T>` 取得參考存到外面，System B 經 `const_cast` 寫：debug 在 B 的邊界斷言；(b) 拿裸指標——`reinterpret_cast` 改唯讀欄：debug 在 System 邊界斷言；(c) 經未宣告 id 讀 World（經捕獲或全域的 `World*`）：debug 在存取當下斷言。release 三者都不攔並寫明。另加：只宣告 Read 卻經 World API 取可寫：存取當下斷言。spike 的 (a)(b) 只在 System 邊界攔、且有攔不到的形狀；**使用者 2026-11-11 已決定（spike R-01）：判定 (2) 照原文記「成立」，攔不到的形狀記為範圍の限界（spike-report L1～L8），後續批次的事前宣告必須寫上**；本測試照原文寫 | W-5 | 移除每一個核對；核對只比 id 不比 mode；移除邊界比對 |
| J-6 | Host 在 Running 期間讀寫 World 斷言；Host 在 Ready 寫非 boundary 的 (c) 欄斷言 | W-6、W-6b | 移除 |
| J-7 | `CountingDtor` 記錄的解構順序：despawn 依 ComponentId，World 解構依 (ComponentId, EntityId) | W-7 | 依登記反序解構 |
| J-8 | 配發器單調、用盡時回報錯誤、不回繞；沒有配發器的 World 登記 `Spawn` System 在 finish 失敗（D-E3） | W-9 | 用盡時回繞；缺 Resource 檢查被移除 |
| J-9 | Kernel 目錄的文字掃描（含自測）：無 `unordered_`、`std::hash` 疊代、`<random>`、`<chrono>`；Parallel 模組外無 `<thread>`／`<atomic>`；無非 constexpr `static`／`thread_local`，**白名單只有 `tlCurrent` 與 `TypeKey<T>` 的 `static inline char`**（A.6；修訂：S10）；無產品名 | W-10、X-8、R-7 | 加 `unordered_map` 索引；加函式內 `static` 快取；加一個產品名；白名單加第三個符號（自測必須失敗） |
| J-10 | 編譯失敗測試（CMake `try_compile` 預期失敗）：帶捕獲的 lambda、泛型 lambda、成員函式、`World&`／`T&`／`int` 參數 | S-1、S-2 | 移除 `static_assert` |
| J-11 | 編譯失敗測試：同一型別寫兩次、同時讀又寫、`Res` 與 `ResMut` 並存；另做「全量對增量重建」等價測試：以 stamp 做差分重建的結果等於全量（抓漏標記） | S-3、W-11 | 移除檢查；`Mut` 不標記 |
| J-12 | 12 個以上不同簽名的 System，推出的存取集合與手寫期望逐一相同（static_assert 加執行期；spike 判定 (1) 本機 18 個簽名成立、三平台待 L1），含 Query 的 Read／Write／With／Without／Changed、Res／ResMut、EventReader／Writer、Commands／CommandsFor、`Spawn<Bundle<…>>`（bundle 元件進延後寫，B3）、`SetRead`（finish 展開）、Local、ExclusiveWorld、run 條件併入；另以兩種參數順序的同一 System 檢查 `Fetch` 無順序相依（B.4 第 7 點） | S-4 | 某個參數的 Contribution 漏掉一個型別；bundle 不進存取集合 |
| J-13 | 編譯失敗測試：run 條件帶 Write 參數 | S-5 | 移除 |
| J-14 | 漏寫 zone、(c) System 帶 Local、(c) System 宣告 effect，各在 finish 失敗 | S-6、S-7、S-8 | 改成預設 (b)；移除各項 |
| J-15 | Exclusive 欄有第二個寫者（立即寫、延後插入、`Spawn<Bundle<T>>`、`SpawnWithId<Bundle<T>>`、hook 各一例），finish 失敗；Startup 的寫者不計（通過）；Shared 欄兩個寫者通過 | S-11、F-10 | 只檢查立即寫；漏算 hook；漏算 CommandsFor；漏算 bundle |
| J-16 | 宣告進 digest 的 Shared 欄，finish 失敗 | S-12 | 移除 |
| J-17 | (c) 區來源檔清單的掃描：Kernel 用合成的 (c) System 檔自測；產品守衛讀 Kernel 匯出的清單並掃描 | B.5、G.2 Math、G-5 | 合成檔加 `static int` 或 `std::random_device`，必須被抓 |
| J-17b | 合成的 headless 組合（沒綁 `Main`）加一個需要 `Main` 的 System＝啟動錯誤 | H-3 | 移除 |
| J-18 | 登記順序決定 id；交換 plugin 順序後 id 依新順序；不同連結順序 id 相同；fingerprint 跨平台 golden | R-1 | id 改依型別雜湊或位址 |
| J-19 | 重複 plugin、依賴缺漏或排在後面、凍結後登記、build 與 finish 的順序、型別被兩個 plugin 登記、移除 plugin 後引用者得 R-E4 | R-2～R-6 | 移除每一項 |
| J-20 | Plan 建兩次相同；debug 下打亂內部容器順序仍相同；Plan 指紋跨平台 golden | D-1 | 平手改用不穩定鍵 |
| J-21 | 每一條衝突邊在 Plan 中有確定先後 | D-2 | 拿掉 (b) 平手加邊 |
| J-22 | (c) 歧義在 finish 失敗：立即寫、延後寫對延後寫（同欄）、事件寫各一例；加 before 後通過。**不算歧義的配對通過**（D.1 第 4 條；修訂：S18）：延後寫對立即讀同欄、延後寫對立即寫同欄、延後寫對 With | D-3 | 改成平手；不計延後寫；不計事件寫；把延後寫對立即讀算成衝突（通過的案例必須失敗） |
| J-23 | (b) 平手依 registrationSeq | D-4、D-10 | 平手改依 name |
| J-24 | D-E1～D-E17、R-E1～R-E5、X-E1～X-E3、F-E1 每種一個最小 Registry，比對錯誤清單；報告指名雙方；多個錯誤一次回報 | D-5 | 移除每一項；第一個錯誤就停 |
| J-25 | With 與 Without 不減少衝突 | D-8 | — |
| J-26 | Serial 的呼叫序等於 Plan 序 | X-1 | — |
| J-27 | 含 (c) 的 schedule 設 Parallel 卻無守衛 id，finish 失敗；（Parallel 實作時）Serial 與 Parallel 逐 tick 的 World 雜湊相同，含 stamp、Entity 序列、事件 seq | X-2、X-7 | 移除守衛 id 檢查；套用改依完成順序 |
| J-28 | `Role(Main)` 的 System 放在 runner 是角色執行緒的 schedule，finish 失敗；執行時執行緒不符 debug 斷言 | X-3 | 移除 |
| J-29 | run 條件看到前一步寫入的值；set 的條件每個成員各求一次 | X-4 | 快取條件結果；set 只求一次 |
| J-30 | 條件為假時 stamp 仍被消耗；stamp 只取決於 Plan 位置；寫入時標 stamp、唯讀不標 | X-5、W-11 | 跳過時不消耗 |
| J-31 | 本窗的 spawn 在同一窗內不可見，過了同步點才可見 | F-1 | 立即套用 |
| J-32 | 不同 System 執行順序（無衝突者重排）、相同記錄集合，World 位元組相同；同一 System 兩筆同鍵、兩個 hook 各一筆同鍵的勝者有 golden（三平台）；同窗 despawn X 再 `SpawnWithId(X)`：新實體存活、hook 看到 remove 再 add；對已 despawn 實體的 Insert 被丟棄並計數 | F-2 | 依記錄順序套用；spawn 先於 despawn；不去重；Insert 平手勝者反轉；丟掉 recSeq 用 `std::sort`（修訂：B1／S4） |
| J-32b | untyped `Commands` 動 (c) 或 Exclusive 欄、untyped spawn、(b) System despawn 帶 (c) 欄的實體→**debug 與 release 都**斷言（常開檢查；修訂：S6） | F-10、Z5 | 移除；只在 debug 檢查（release 的測試必須失敗） |
| J-33 | stage 末與宣告的同步點都套用；未宣告處不套用 | F-3 | 每個 System 後都套用 |
| J-34 | hook 依登記順序；hook 的變更下一窗才生效；Applying 期間 System 路徑記錄斷言；(c) hook 衝突 finish 失敗 | F-4、F-5、F-11 | 立即套用；hook 反序；移除 D-E9 |
| J-35 | System 拋例外→World `Interrupted`、本窗佇列丟棄、下一次 Run 斷言；Restore 後可用 | E.5、X-9 | 拋出後不標記 |
| J-36 | 事件：寫者與讀者四種相對順序 × 兩個 tick，每筆恰好一次；同 tick 讀到、下 tick 讀到、隔兩 tick 讀不到；讀到的順序＝(tick, writerPos, seq)；hook 寫的事件以同步點位置為 writerPos，之後的讀者本 tick 讀到（修訂：S12） | F-7、F-8 | 一律延一 tick；修剪改在 T 末；`>`／`<` 互換；hook 事件的 writerPos 設 0 |
| J-37 | 事件跨 domain，finish 失敗；同 domain 的非 clock schedule 讀或寫事件，finish 失敗（修訂：S14） | F-9 | 移除 |
| J-38 | (c) System 用 Changed 過濾，finish 失敗 | S-9、G-3 | 移除 |
| J-39 | (c) System 讀 FrameTime、任何 System 寫 domain tick 或 boundary Resource（ingress 以外），finish 失敗 | G.2 Executor | 移除 |
| J-40 | (b) 寫者寫的欄被 (c) 讀者讀到，finish 失敗；**(c) System 以 `With<BTag>`／`Without<BTag>` 過濾 (b) 欄，失敗；(c) System `Write<BMode>` (b) 欄，失敗；(c) System 的 run 條件讀 (b) Resource，失敗**（修訂：B2）；ingress 寫者寫 `role = Input` 的欄通過；ingress 寫 `role = State` 失敗；ingress 不在 ingress stage 失敗；`role = Input` 標 `rollback = Yes` 失敗（Z8） | Z1～Z3、Z8 | 移除污染檢查；Z1 只看 reads；Ingress 豁免擴大到所有 (b) |
| J-41 | (c) System 讀 Link（external）、`ResMut<Link>`（修訂：B2），finish 失敗 | Z1、Z7、I-3 | 移除；只攔讀 |
| J-42 | 回滾等價：合成 FixedUpdate（**含 ingress stage**：合成的 ingress System 寫 ring，帶 `NotInRollback`）跑 10 tick，與「跑到第 5、Rewind 到第 3、Replay 到第 10」比較回滾集合逐位元相同（含 spawn 情境）、ring 內容相同；`Restore` 以部分快照還原後 Replay，與直接跑的結果比對；在 System 內呼叫 Rewind 斷言；ingress 沒有 `NotInRollback` 時 finish 失敗（D-E15） | H-5、H-9、H-10 | 還原時漏掉事件緩衝；漏掉配發器；**不還原 domain tick**（修訂：B5）；ingress 在 Replay 中執行（修訂：B7）；InputRing 進快照 |
| J-42b | 可回滾 schedule 上有副作用的 System 無 `NotInRollback` 且非 `rollbackSafe`，finish 失敗 | H-8 | 移除 |
| J-42c | Restore 整欄替換後 OnAdd／OnRemove 不執行；Restore 寫回的格 `changed` = 目前 stamp（修訂：S12） | H-11 | Restore 觸發 hook |
| J-43 | Extract 期間來源 World 的位元組與 stamps 不變；寫入斷言；來源未凍結先建目標＝啟動錯誤；`Extract<Write>` 編譯失敗 | H-6 | 移除 |
| J-44 | (c) System 的登記順序做 20 種排列：要嘛全部 D-E8，要嘛每一對衝突 System 的先後與 World 摘要（D-9 的定義：不含 stamps）都不變 | D-9 | 平手改用插入順序以外的不穩定鍵；(c) 歧義改平手 |
| J-45 | `ExclusiveWorld` System：zone=C 持有它→finish 失敗；(b) 的 exclusive System 寫 zone=C 欄、寫 Exclusive 欄、spawn→常開檢查斷言（debug 與 release）；寫 zone=B 且 Shared 的欄通過；讀任何欄通過（修訂：B4） | S-13 | 移除常開檢查；只在 debug 檢查 |
| J-46 | `rollback = Derived` 的欄：讀者前無同 schedule 立即寫者→finish 失敗（D-E16）；有則通過，且 Rewind 後 Replay 第一步讀到的值等於重算值 | A.1 Derived、Z6 | 移除順序檢查 |
| J-47 | 具名集合：兩個 plugin 各加一個元件進同一集合，`SetRead` 的 System 在 finish 得到兩個 id 的讀；移除其中一個 plugin 後只剩一個；集合中有 (b) 欄而讀者是 (c) → D-E7（修訂：S13） | B.4 第 4 點、Z1 | 展開漏掉後登記的成員 |

### J.3 ミューテーションテスト的對象

上表「突變」欄全部登記在 Kernel 計畫資料夾的 `scripts/mutations.json`（照 entity-store 的做法）。依 D184（memory mutation-rerun-scope），每批只跑與本批相關的突變，PR 線合併前在背景完整跑一次。

最優先的突變（漏網就靜默破壞 (c) 或獨占寫入）：
1. 套用改依記錄順序（J-32）
2. (c) 歧義改成平手（J-22、J-44）
3. 不計延後寫或事件寫（J-22）
4. 移除污染檢查 Z1／Z2（J-40）
5. 移除變更過濾的禁令（J-38）
6. Plan 平手鍵改變（J-20、J-23）
7. 疊代改依插入順序（J-2）
8. stamp 依完成順序取值（J-30）
9. 配發器回繞（J-8）
10. 回滾快照漏掉事件緩衝或配發器（J-42）
11. 獨占寫者計數漏算 hook、CommandsFor 或 bundle（J-15）
12. debug 核對只比 id 不比 mode（J-5）
13. 丟掉 recSeq 用 `std::sort` 排延後佇列（J-32；三平台 golden 才抓得到）
14. Z1 只看 reads、不看 writes 與 With／Without（J-40）
15. Rewind 不還原 domain tick；ingress 在 Replay 中執行（J-42）
16. untyped 路徑的常開檢查退成 debug 斷言（J-32b、J-45 的 release 目標）

### J.4 產品守衛（不在 Kernel，由 owner 持有）

- 既有 `authority_digest`（37 個 golden）。遷移到機制後 golden 一個位元組都不變，是「遷移沒有改變判定」的驗收。
- H-4 的 `cFingerprint` 比對。
- G.2 Ingress 的到達時序排列重放。
- J-17 的產品端掃描（使用 Kernel 匯出的 (c) System 檔案清單）。
- H-7 的 host 原始碼掃描（清單由 -audit 產出）。

---

## K. Design Invariants 草案與 Replacement Criteria 草案

候選。使用者核准後才放進 `docs/architecture/kernel.md`（位置未定）的 `Design Invariants` 節。每條附守衛與對應的 blueprint DI-K。

### K.1 Design Invariants（候選 DI-KS）

1. **描述從簽名推出，一份。** System 的資料存取集合等於其參數型別的 Contribution 聯集（含 bundle 的延後寫、`SetRead` 在 finish 展開的讀；`ExclusiveWorld` 的 Contribution 是「讀全部、寫 zone=B 且 Shared 的全部」）；登記 API 不接受手寫的第二份存取宣告；System 是無狀態的可呼叫物，參數只能是 `ParamTraits` 特化過的型別，句柄不可複製、不可移動。守衛：J-10～J-12。（DI-K2 的推導半；S-1～S-4、S-13）
2. **經 World 的繞路在 debug 被攔；untyped 路徑常開檢查。** System 自己的執行緒上經 World API 存取未宣告的 `(id, mode)`，debug 在存取當下斷言；未宣告寫入的欄在 System 邊界有淨變化，debug 斷言；release 兩者都不攔，寫明。untyped `Commands`、`Despawn`、`ExclusiveWorld` 對 zone=C 或 Exclusive 資料的寫入，在所有組態斷言。守衛：J-5、J-6、J-32b、J-45。（W-5、W-6、F-10、Z5、S-13；修訂：B4／S6／S8）
3. **順序只來自 id 與 Plan。** 每個欄依 EntityId 嚴格升冪，查詢依升冪或交集的升冪產出；World、Scheduler、Executor 不用雜湊順序、指標比較、執行緒完成順序決定任何順序。守衛：J-1、J-2、J-9。（DI-K3 的一部分；W-1、W-2、W-10）
4. **id 由登記順序或配發器決定，凍結後不變。** ComponentId、ResourceId、EventTypeId、registrationSeq＝登記順序；TypeKey 只用於行程內查表；執行期 id 不上 wire、不進 digest；World 不配發實體 id，配發器是 Resource，單調、不重用。守衛：J-8、J-18、J-19；掃描。（DI-K3；R-1～R-3、W-9）
5. **衝突與順序分開；(c) 的歧義是啟動錯誤。** 衝突邊只表示「不能同時」，順序邊表示先後。有衝突且無順序的對：任一方 zone=C→啟動錯誤；兩方 zone=B→登記序平手。Kernel 不提供歧義豁免。(c) 區對登記順序不敏感。守衛：J-20～J-24、J-44。（DI-K1 的修正版；D-1～D-10；主張 A）
6. **獨占寫入是啟動檢查。** 元件與 Resource 預設 Exclusive；Exclusive 的欄在一個 World 的所有非 Startup schedule 中寫者（以 System 計）至多一個，寫者含 `Write`、型別化 Commands、`Spawn`／`SpawnWithId` 的 bundle、hook；沒有函式體內決定型別的寫入路徑能繞過計數（untyped 路徑寫不到 Exclusive 欄，常開檢查）；進 digest 的欄必須 Exclusive。守衛：J-15、J-16、J-32b。（DI-K2 的單一 writer，範圍從「同階段」改為整個 World；AGENTS §3；修訂：B3／S1）
7. **(c) 預設 Serial；Parallel 要逐位元守衛。** 含 zone=C System 的 schedule 宣告 Parallel 時，必須指名一個存在、帶 `cpu` 標籤、比對 Serial 與 Parallel 的 (c) digest 逐位元相同的測試，否則啟動錯誤。守衛：J-27。（DI-K1「不做自動平行」的修正版；主張 B）
8. **區規則是啟動檢查。** 每個 System、元件、Resource、事件宣告 zone，沒有預設；Z1～Z8。zone=C System 的全部存取（讀、寫、With／Without、事件、延後寫、run 條件）都落在 zone=C 資料；(c) 只從 domain tick 與被記錄的 boundary 輸入取時間、從 World 取亂數、從 `InputRing` 取輸入；不用變更過濾；不讀寫 external Resource；role=Input 的資料不回滾。守衛：J-14、J-38～J-41；J-17。（DI-K5、DI-K6；G 節；修訂：B2／B6／B7）
9. **結構變更延後、在宣告的同步點依「hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource」套用，每階段依 id，同鍵依 (planPos, recSeq)，鍵唯一。** Running 期間成員不變；hook 的變更屬下一窗；結果不依 System 之間的記錄順序。守衛：J-3、J-31～J-34。（K4；W-3、F-1～F-5；延伸 entity-store DI 4；修訂：B1／S4）
10. **事件無游標、本 tick 寫者之後就可讀、保留到下 tick 末、不跨 clock domain。** 守衛：J-36、J-37。（F-7～F-9）
11. **hook 只有 OnAdd 與 OnRemove，同步執行、依登記順序，寫入計入獨占，(c) 的 hook 衝突是啟動錯誤。** 守衛：J-34。（K6；F-4、F-11）
12. **回滾所需的狀態全在 World，固定步可重入。** (c) System 不得有 Local；domain tick 只有一個來源且在快照內；快照含 `rollback = Yes` 的欄與 Resource、配發器、Fixed domain 事件緩衝，不含 role=Input 的歷史；Rewind 或 Restore 加 Replay 與一次跑完逐位元相同（stamps 除外）；可回滾 schedule 的 ingress 在 Replay 中不執行；Rewind／Restore／Replay 只由 Host 在 Ready 呼叫；Restore 不觸發 hook。守衛：J-42、J-42b、J-42c、J-46。（K8、DI-K5；H-5、H-8～H-11；修訂：B5／B7／S2／S12／S15）
13. **渲染只經 Extract 以 const 方式讀來源 World，無反饋。** 守衛：J-43。（DI-K7；H-6）
14. **一個 World 一條執行緒、每個 domain 一個 clock schedule、每個 domain 的事件只在它的 clock schedule 內。** 同一 World 的所有 schedule 綁同一執行緒（Parallel 下一個 System 的存取只來自派它的 worker、只在那次呼叫內）；不同執行緒上的時鐘是不同的 World，經 Host 的交換點交換；外部系統不持有、不修改 World，只經 Link 類 Resource 在交換點、World Ready 時交換，Running 期間不碰 System 側緩衝；World 的生命期是一個世代，世代切換＝重建 World。守衛：J-15、J-6、J-37；finish 檢查 H-1、H-2；審查。（W-6、W-9、H-1、H-2、I-1～I-4、F-9；修訂：S4／S11／S14／N3）
15. **Kernel 中性，Host 保留多時鐘、啟停、IO 交換點、runner 的指定；Host 不排 System 順序。** Kernel 的程式碼與 Registry 不含產品、工具、能力的名字，不內建 schedule 或 stage 名，不擁有執行緒，不讀時鐘或亂數；已遷移的 host 只呼叫 `Run`、`Rewind`／`Replay` 與交換點。守衛：J-9；H-7 的掃描；審查。（DI-K4；R-7、X-8、H-7）

### K.2 Replacement Criteria（候選）

符合任一條時，停止在 Kernel 上修補，向使用者提出「修還是重建」（memory repair-vs-rebuild）：
1. **契約不可靠**（修訂：S8。原稿的條件「判定 (2) 在任一編譯器不成立」已被 spike 的「判定 2 部分不成立」觸發，但觸發的部分是 mechanism §3.5 事前寫明的上限（不經 World 的讀取），不是推導或核對本身失效；而且原稿的處置「退回顯式宣告」對那種形狀同樣無效。條件改寫成只指向本機制真正依賴的性質）：(a) spike 判定 (1)（簽名推存取集合）在任一 L1 編譯器不成立；或 (b) 「System 自己的執行緒上經 World API 的存取」在 debug 攔不到（A.6 第一層失效）；或 (c) 上線後「宣告與實際存取不符、A.6 兩層核對與 J-17 掃描都攔不到、且不屬於 spike〈判定 2 的上限〉已列的形狀」的案例在一個版本內出現兩次以上。→ (a)：退回「顯式宣告＋debug 核對」（bevy_core:29、:64）；(b)：核對層重寫；(c)：評估頁保護（spike 報告末段）或縮小「包」的範圍。中間層照 mechanism §1 全部重寫。（blueprint RC 3）**這條的改寫是 Replacement Criteria 的變更，要使用者核准**（K.3）。
2. **id 要重用**：有 World 必須在一個生命期內重用 id，或兩個 World 需要 id 對照表。→ 提案加 generation 與對照表，K.1 第 3、4 條改寫。（entity-model RC-2、RC-5）
3. **歧義錯誤成為負擔**：同一版本內，為消除 D-E8 而加的 before／after 有一半以上與資料流無關（只是為了讓 Plan 建立）。→ (c) 的粒度或區劃錯了。
4. **第二種排序機制**：為表達需求，stage 內必須引入依賴求解以外的排序（優先權數字、依資料值決定順序、執行期改 Plan）。（blueprint RC 2）
5. **過濾誤報**：D-8 造成的誤報使某個 (c) schedule 中的 `after` 宣告超過 System 數的一半，或需要並行的 schedule 因誤報無法並行。→ 提案加入過濾式的不相交判斷。
6. **欄結構不夠**：entity-model RC-1 或 RC-3 觸發（4,000 實體、1 ms p95、動態組合過半）。→ 換儲存，K.1 第 3 條的順序不變，不在上面加 archetype。
7. **Host 又長回來**：第一個真實能力（場景線的拉桿開門、VFX）接進機制時仍要改 Host 迴圈或 Kernel 本體；或 H.7 以外的協調邏輯在兩個以上 host 中重複。（blueprint RC 1）
8. **遷移成本**：遷移一個 host 的實測成本超過重寫它，或遷移到一半必須讓兩種動詞長期並存（違反 H-7）。（blueprint RC 4）
9. **同步點模型錯了**：(c) 的某條規則必須在同一窗內讀到自己剛生成的實體，而宣告同步點無法滿足。
10. **獨占模型與需求不符**：同一 World 中 (c) 的 Exclusive 欄為繞過 K.1 第 6 條被改成 Shared 兩次以上。→ 重新劃分元件。
11. **回滾等價破裂**：J-42 在 L1 平台因 Kernel 以外的原因（Math、編譯器）失敗，且 E0 的結論是 determinism.md RC 3。→ (c) 縮為 (b)，K.1 第 7、12 條的「逐位元」改寫。
12. **並行不是策略問題**：為讓 Parallel 通過 X-2 的守衛必須改 (c) System 的語義或放寬 golden。→ 停下。
13. **每加一個 System 或 plugin 都要重新驗證 Kernel 本體的守衛**（突變名單以外）。→ Kernel 不是穩定的基盤。

### K.3 待使用者決定（規格內產生）

1. Entity 不帶 generation、產品 `EntityId` 就是 World 的鍵（A.8 取捨 1；與任務敘述不同）。
2. change stamp 依 Plan 位置取值、不等於遊戲 tick（A.8 取捨 2；修改 DI-K5 候選與 networking §1 第 7 條的一句）。
3. 寫入政策預設 Exclusive（A.8 取捨 3）。
4. Despawn 不算獨占寫者（B.9 取捨 1）。
5. 讀 (c) 的 (b) System 必須與 (c) 寫者有顯式順序（D.6）。
6. Client 的 Frame 與 Render 兩個 World 合併還是獨立（H.4.2；推薦獨立）。
7. Architecture Delta：`docs/architecture.md:136`（Engine 不得有 system／scheduler／world／resources／events／app／plugin）要修訂；entity-store DI 4 延伸、`DeferredChanges`（3b，未實作）撤回由 World 的佇列取代；determinism.md §4.7 已預告「columns become the World's storage」。
8. `apps/object_fps`、`object_fps_v2` 是整個遷移、維持舊動詞直到移除、還是移除（H-7 不允許一半）。

修訂輪（對抗檢查後）新增：

9. **獨占寫者的單位是 System、Startup 不計**（B.3；S1）。後果是 Body 要依 entity-model 的欄位分類拆欄；替代方案「以 owner plugin 計」免拆欄但失去資料流粒度。
10. **Join／Leave 進 tick 內**（H.4.1 (d)；S3）：Ingress 只寫請求進 ring，(c) 的 `ApplyMembership` 生成與刪除；與 entity-model §5.2「Join／Leave 在 Tick 外」不同，golden（`combat-rejoin` 等）可能差一個 tick。
11. **時間的來源**（G.2 Executor；B6）：mechanism §3.3「Time 只來自 tick」與 determinism.md DI 4「the tick, or a clock injected」衝突。本稿採 DI 4 的形狀（tick ＋ 被記錄的 boundary 取樣 `HostClockSample`），因為 Host judgement 區（determinism.md §2，(c)）本來就要量到達時機。選項：(i) 修 mechanism §3.3 的 Executor 列為「tick 與被記錄的 boundary 輸入」；(ii) 把 Host judgement 改成 (b)，HostClockSample 變 external——這改 determinism.md 的區表與 golden `host-poor-connection` 的地位。
12. **F.2 的階段順序改成 despawn、remove 先於 spawn、insert**（S4）：讓同窗「刪 X 再以同 id 生成 X」成立；代價是「同窗生成又刪除同一個新 id」從不可見變成生成勝出。
13. **K.2 第 1 條（Replacement Criteria）的改寫**（S8）：原條件已被 spike 的結果觸發；本稿主張觸發的是事前寫明的上限、不是機制失效，並把條件改寫成三個分項。Replacement Criteria 的變更要使用者核准；若不核准，依原條文本規格進入「修還是重建」的提案。
14. **`Parallel` 在 v1 是啟動錯誤 D-E17**（S11）而不是靜默 Serial；只含 (b) 的 schedule 也一樣，等 Parallel 實作時再開放。
15. **事件只在 clock schedule 內讀寫**（S14；F-9）：同 domain 的其他 schedule 用元件或 Resource 傳遞。
16. ~~**spike 判定 (2) 的狀態**（S7）~~ **已決定（使用者 2026-11-11，側邊會話轉述）**：判定 (2) 照原文「成立」；上限的處置＝接受（World 以外靠 code review，mechanism §3.5 三層）；-audit 第二問改為「跨 tick 指標或參考」「全域可變狀態」兩問（I.3 已照此寫）；另加 `-spike-pageguard` 批（debug 頁保護，只對寫設保護、只保護獨占寫入的欄、只在 (c) 區 System 執行時）排在本規格之後。本規格 A.6、B.5、I.3 的寫法與決定一致，不需改。
17. **untyped 路徑的「常開檢查」**（B4／S6）：引入一個不依 `NDEBUG` 的檢查類別，與現有 error-handling 契約（Assert＝Programmer Error、debug only）的關係要定：是新的巨集（例如 `GYO_VERIFY`）還是 `Result` 回報。本稿寫成「呼叫 assertion handler、所有組態」。

---

## L. 現有模組對照（初判；-audit 的起點，不是結論）

判準：I.3 的三否定條件（掃描項含時鐘與亂數，S17）、I.3 的三問（跨 tick 指標、全域可變狀態、自起執行緒碰 World；S8）加 mechanism §1 的三問。**留**＝原樣作為函式庫，由 System 呼叫；**包**＝狀態變 Resource、更新變 System、生命週期交 Host 或 plugin，內部不改；**重寫**＝更新介面或資料取得方式框不進一份契約；**服務**＝外部系統，經 I.1 邊界接入；**Host**＝成為 Host 的零件。行數是提案 3 腳本統計的 .cpp／.hpp 行數（不含 tests；統合者未重跑）。標「未複核」的證據行號由統合者保留提案的引用但未再次打開檔案。

### L.1 Engine

| 模組 | 行數 | 更新入口／執行緒／全域狀態 | 初判 | 理由與證據 |
|---|---|---|---|---|
| `base` | 670 | `Log.hpp:43` 的 `inline constinit std::atomic<LogHandler>`；Assert 的 `assertionHandler`（`Assert.hpp:49`；`:34` 是註解）與 `thread_local reportingAssertion`（`Assert.hpp:50`）（修訂：S17） | 留（附例外） | Result、Error、StrictJson、Sha256 是純工具；log 與 assert 的全域狀態只寫不讀回、不影響結果，依 I-5 明列為例外 |
| `math` | 1451 | 無 | 留 | 純函式；(c) 待 E1（determinism.md §4.8） |
| `collision` | 962 | 無；格子建好後不變 | 留 | 純查詢（double、一次捨入）；StaticAabbGrid 跨 tick 保存時放 Resource；DI 1 的等價契約不受影響 |
| `voxel_mesh`、`map_markers` | 1012／457 | 無 | 留 | 契約資料的純展開（determinism.md §4.6） |
| `entity_store` | 155 | 無 | 留（演算法）＋成為 World 欄的實作 | `EntityColumns.hpp:74-91` 的 insert／erase／find 正是 W-1、W-2 的實作；DI 1（不配發、無 generation）、DI 3、DI 5（store 不跑任何東西）、DI 6（無 change tick）對 store 本身仍成立，World 是它之上的新元件。需要的 Delta：回傳位置的查找版本或 `Cell<T>` 包法（A.6）；批次合併；DI 4 由 F.2 延伸到 insert／remove；`DeferredChanges`（entity-store.md 標 proposed，3b 未實作）撤回 |
| `net` | 144 | 無 | 留 | GYOP datagram 編解碼，純函式（determinism.md §2 Outside） |
| `text` | 285 | sdl_ttf 後端 `TTF_Init`（提案 3 引 `:58`，未複核） | 核心留；後端包 | 後端有生命週期（I-3 第 1 條） |
| `io` | 4371 | 未見執行緒（掃描）；1 處 `Poll`；`WriteFileAtomically.cpp:4`、`:15` 讀 `steady_clock` 並用 `std::random_device` 產暫存檔名（修訂：S17） | 路徑與 JSON 輔助**留**；檔案服務**包** | 路徑與 JSON 輔助是純函式。`WriteFileAtomically` 違反 I.3 第 3 條（時鐘與亂數），不能判「留」：只能從宣告 `Io:file` 副作用的 (b) System 呼叫，(c) 檔不得 include（J-17 的掃描抓 `<chrono>`／`<random>`）；檔案服務部分未逐檔讀 |
| `os` | 2355 | 執行緒、Console、Shutdown（`os/src/common/Shutdown.cpp` 有 atomic） | Host（adaptor） | OS 層原意是 adaptor（memory os-layer-original-intent）；System 不直接呼叫 |
| `time` | 408 | `Waiter`（OS 資源）、`MonotonicClock` | Host | 時鐘與等待是 Host 的職責；牆鐘只進 zone=B 或 boundary 的 Resource；(c) 區檔案不得 include（J-17） |
| `threads` | 221 | `RoleThread` | Host（runner 的落點） | DI 4「the owner orders roles」（`RoleThread.hpp:62`）正是 H.7 的職責 |
| `trace` | 362 | writer 執行緒（`TraceSink.hpp`） | 服務 | `Diagnostics` 副作用的接收端；System 只推已序列化的列 |
| `runtime`：`FixedTickRuntime` | 291（整個模組） | 純累加器，呼叫者給 elapsed；自有 `tickId_`（`:54-56`、`:68` Reset 歸 0、`:76`） | 留（Host 的時鐘工具） | `TickContext.deltaSeconds`（double，`:19`）由 `FixedStep.stepNs` 取代（G-6）；`tickId_` 不再是 tick 的來源，只供 Host 決定呼叫 `Run` 幾次（B5） |
| `runtime`：`RuntimeLoop`、`IRuntimeClient` | — | ProcessEvents／Update／Render 三段（`IRuntimeClient.hpp:12-14`）；live frame（`RuntimeLoop.hpp:11-37`） | 重寫 | 三段是硬編碼的調度，由 Host 加 ScheduleDef 取代；live frame 是平台約束，留成 Host 的職責（H.7） |
| `runtime`：`IRuntimePort` | — | Query／Submit／Events（`IRuntimePort.hpp:17-19`） | -audit 定 | 概念接近 Link；只有 object_fps 與 v2 在用 |
| `platform`（sdl） | 1285 | `PumpEvents`；`thread_local`（`SdlPlatform.cpp:61`、`:73`、`:75`；`SdlLogBridge.cpp:24`，修訂：S17） | 包（Host 擁有） | 違反 I-3 第 3 條，不能當葉子；pump 是 Host 的步驟，事件以 `PlatformEvents` boundary 進 World（O2、P1）；platform-sdl.md DI 7「observer 與 live frame 內不得做 operation」是 Host 的 pump 與 live frame 處理器要守的，不進 System |
| `input` | 1156 | `BeginFrame`／`HandleEvent`／`EndFrame`（呼叫序 `PvpApplication.cpp:1111-1119`） | 後端包；`InputActionMap` 留 | 每幀狀態機變 Resource 加 `Input` stage 的 System |
| `asset` | 3333 | `AssetManager::BeginFrame`（`:59`）、`Update`（`:62`）；`LoaderRegistry` 已有 K3 形狀；`detail/TypeId.hpp:37` 的 `static const` token（B.6 風險 5） | 包；loader 留 | 存取可框住（`ResMut<AssetManager>`）；非同步完成的套用點對應 A3；`Load`／`Acquire` 的呼叫點 -audit 逐一列 |
| `model` | 1174 | 取樣純函式；ufbx loader；`Advance` 2 處 | 取樣留；loader 留（asset loader）；動畫推進包 | 取樣是 (b)（determinism.md §4.2） |
| `render` | 4300 | `Renderer::Render(queue)`；裝置綁建立它的執行緒（未複核） | 包（渲染 World 的 Resource，親和 `GraphicsContextOwner`）；`RenderQueue` 是 extract 的目標 | blueprint §5 R1 列 |
| `ui` | 4471 | `UiRuntime::Update`（`UiRuntime.hpp:86`）含焦點狀態 | 包；版面與編解碼留 | 狀態變 Resource |
| `audio` | 1619 | Mixer 的 SDL 回呼執行緒（`Mixer.cpp`、`VoiceMixer.cpp` 有 atomic） | 服務；`Submitter` 是 Resource | 整數混音契約不變；Submitter 綁第一個呼叫它的執行緒→親和 |
| `config` | — | 只有 `projects.csv`、`tools.csv` | 不適用 | 登記資料，不是程式 |

### L.2 framework

| 模組 | 初判 | 理由 |
|---|---|---|
| `HeadlessShell` | 留（行程入口） | 轉換命令列、呼叫一次 body |
| `WindowShell`、`WindowShellClient` | 重寫，成為視窗 host 的骨架與標準 ScheduleDef | 今天是 `RuntimeLoop` 加 `Update`／`Render` 的 client 介面（`WindowShell.hpp:19-28`、`:49`），與 H-7 不相容；改造後 framework 提供標準 ScheduleDef 與 Host 骨架（blueprint §6；三層原則：framework 持通用結構） |
| `ImGuiShell`（binding） | 包 | 全域 context（`ImGuiShell.cpp:225`、`:267-269`）不符 I-3 第 3 條；ImGui System 用 effect `Gpu:imgui` 序列化；可插拔模組（memory framework-controller-principle） |
| `Preview`、`PreviewCamera`、`PreviewTexture` | 包（渲染 Runtime 的 plugin） | 離屏裝置加 readback |
| `EventSounds`、`CatalogPlan`、`ReadOnlyAssetCatalog`、`AbsolutePath` | 留 | 資料契約與純函式 |
| `UiTextInput`、`bindings/json` | 未查證／留 | — |

### L.3 產品 `object_fps_pvp`

| 模組 | 初判 | 理由與證據 |
|---|---|---|
| `StepMovement`／`PlanarMovement` | 函式留；呼叫點包成 (c) System | 純函式 (state, command)→state。呼叫路徑經全域 `movementDiagWriter`（`MovementDiag.hpp:188`）與 `MovementTrace`（`:59` 讀時鐘、`:100` 全域 mutex），要改成宣告的 `Diagnostics` 副作用（G.4） |
| `PvpMatch`（715 行） | 重寫為 (c) plugin | 五欄是產品自己持有的 vector→World 的欄；`Tick`（`PvpMatch.cpp:520`）是寫死的序列：`ResolveLifeBoundaries`（`:526`）→ `StepMovement`（`:571`、`:582`）→ `ResolveActions`（`:617`）→ `AdvanceAnimation`（`:623`），就是 mechanism §1 說的「順序是程式碼不是資料」；E-12 的待辦守衛（`inTick_`，entity-model 第 3 行寫「第一個非玩家批才有程式」，`PvpMatch.cpp` 今天 grep 無此符號；修訂：N5）由 W-3 取代，不是取代既有程式；`Decide`、`ResolveActions` 的函式本體大半可搬成 System 本體，但 `bodies_` 的三個寫入點拆成 System 後觸發 D-E2，要先依 entity-model 第 139 行的分類拆欄（B.3；S1）；`Join`／`Leave`／`Reset` 不再是 World 內的操作（H.4.1 (d)、A.1） |
| `MatchRuntimeHost`（814 行） | 重寫：Host（時鐘、執行緒、交換點）＋Ingress、Judge、Egress System | 一把 `mutex_` 包住整個 `Step`（`:371`）；`:386-440` 的交接是 Ingress；`:450-451` 是 host 判斷（(c)）；`:471` 起是 Egress；`now_()` 回呼（`:441-445`）改 `HostClockSample`（G.4）；`Body`（`:513`）留 Host |
| `IpcHost`（943 行） | 位元組收送包成服務；同步呼叫權威的部分重寫 | `IpcHost.cpp:685` 在 IO 執行緒同步呼叫 `SubmitActionBatch`，必須改經 Link；回覆時機會變（H.4.1 (b)） |
| `ClientConnection`（1093 行） | 重寫：asio 層變服務加 Link；狀態變 Resource | 有自己的執行緒、mutex 與狀態；`SendInput` 從模擬執行緒呼叫 |
| `ClientSimulationRole`／`ClientSimulationLoop`／`ClientSimulation` | 重寫：ClientSim Runtime 加 Host 的時鐘策略 | `RoleThread::Start`（`ClientSimulationRole.cpp:151`）上的手寫迴圈、`mutex_` 交換（`:161-211`） |
| `LocalPlayerPrediction`（539 行） | 重寫為回滾機制的使用者 | `Reconcile` 的重放就是 H.5 的形狀；`PredictStep` 就是 `StepMovement` |
| `SnapshotTimeline`、`InterpolateLocalPresentation`、`PresentAt` | 函式留；成為 `Present` stage 的 System | — |
| `PlayerAnimation`、`PlayerPose`、`PlayerHitModel`、`PlayerHitbox`、`ShotQuery`、`ShotGate` | 留（System 本體）；`ShotReferenceAge` 回呼要改 | 多為函式；區照 DET-1；G.4 |
| `PlayerPresentation`、`WeaponViewModel`、`HitFeedback`、`PvpSound`、`SoundCues` | 包（ClientFrame 的 Resource 與 System） | 呈現狀態 |
| `PvpApplication`（1453 行） | 重寫：Client host 加 ClientFrame、ClientRender 的 System | `Update`（`:1139-1173`）在一個函式內做資產、輸入、UI、意圖發布、呈現；`RuntimeLoop` 驅動（`:1383`） |
| `AuthorityDigest`（tests） | 包成 System | 讀 World 的欄（依 id 升冪，與 E-9 一致）；golden 不變是遷移的驗收（J.4） |
| `ClientLog`、`LogFile`、`NetworkStatistics`、`MovementTraceWriter`、`MatchIngressTrace` | 服務（`Diagnostics`） | 有自己的執行緒 |
| `Arena`、`ArenaMesh`、`MovementRulesFile`、`*Digest`、`NativeFiles` | 留（Startup 或 Host 初始化時呼叫） | 載入與內容身分（determinism.md §4.9） |
| `entity-model` 的 E 規則 | 改寫 | E-12 → DI-KS 9；E-5 → DI-KS 3；E-1～E-3 的 `EntityId` 就是 World 的鍵（A.1）；E-9 不變 |
| Gateway（Go，`services/gyo_gateway`） | 外部，不變 | 訊息邊界；維持無狀態 |

### L.4 其他產品與工具

| 對象 | 行數 | 初判 |
|---|---|---|
| `apps/object_fps`、`apps/object_fps_v2` | 約 10k 各 | 未審。用 `RuntimeLoop` 加 `IRuntimePort`；依 H-7 要嘛整個遷移、要嘛維持舊動詞直到移除；**要使用者決定**（K.3 第 8 條） |
| `tools/voxel_map_editor` | 5967 | `EditSession` 包（文件 World 的 Resource）；`EditorClient` 重寫成 Editor host（H.4.3） |
| `tools/audio_editor`、`model_viewer`、`ui_editor` | 1852／3453／6260 | 未審；同為 ImGuiShell 形狀，預期判定同 voxel_map_editor |
| `tools/object_fps_preview` | 144 | 未審 |

### L.5 中間層成本的形狀（未估工時）

重寫集中在三處：產品的組織層（`PvpMatch::Tick`、`MatchRuntimeHost`、`PvpApplication`、Client 的模擬角色）、兩個連線元件的同步介面（`IpcHost:685`、`ClientConnection`）、framework 的 `WindowShell`。Engine 本身大多是留或包。與 mechanism §1 的預測一致。行數與工時由 -audit 估。

---

## M. 刻意不做的事與理由

| 不做 | 理由 | 何時重新考慮 |
|---|---|---|
| 多執行緒 Executor 為預設；第一版不實作 Parallel | 主張 B；完成順序非決定性（bevy_core:22）；序列化只是必要條件（PR #177）；(b) 的 Parallel 依賴宣告真實而 C++ 攔不到全域繞路（B.5）；GYO 今天單執行緒，未觀測到 CPU 壓力 | 某個 schedule 量到超出計畫預算（AGENTS §2 計畫條件 2），且有 J-27 的守衛 |
| Observer | 順序依 EntityHashMap（bevy_core:40）；hook 加事件已涵蓋網路需要的「結構變了有人知道」（networking §1 第 6 條）；多一條派送路徑就多一個順序契約 | 兩個以上能力需要「每實體訂閱」且 hook 加事件表達不了，而且能定義依登記序的執行 |
| 動態查詢（執行期組 Query、以 id 字串查元件、反射） | 存取必須在 finish 時已知，Scheduler 才能分析；E-11「不出現執行期屬性系統」；AGENTS §8。「finish 時由 Registry 資料決定的讀取集合」不是動態查詢：`SetRead<SetId>`（B.4 第 4 點，修訂：S13）供複製與 digest 用 | 編輯器的檢視器需要時，以 exclusive System＋zone=B 提供（只能寫 zone=B、Shared 的資料，S-13），不進一般 System |
| Archetype、Table／SparseSet | 實體移動時順序依歷史；GYO 的元件組合是固定表（entity-model RC-3）；欄模型的順序天生依 id | K.2 第 6 條 |
| generation 位元 | A.8 取捨 1 | K.2 第 2 條 |
| 第三方 ECS 選庫 | v0.2 §11：先確認責任邊界再選庫；EnTT 的 sparse set 用 swap-and-pop、順序依增刪歷史；PR #176 的評估等本規格 | 本規格核准後，評估卡以 K.1 判斷候選能否當 World 的實作 |
| 變更過濾用在 (c) 區 | G-3 | 有一條 (c) 規則必須依賴「某 tick 的變更」，且可改寫成以 tick 為基準的區間查詢（附重放守衛） |
| 自動插入同步點、`ambiguous_with` 等豁免、weak edge、巢狀 SystemSet hierarchy、set 條件只求一次 | 同步點位置是契約；豁免讓 DI-KS 5 失效；沒有壓力；多一種語義 | 豁免不重新考慮；其餘有 Feature 時 |
| Plugin 的 ready 與 cleanup、非同步啟動、動態載入、plugin ABI | 資產 ready 前全載；`docs/architecture.md:136` 仍禁止 plugin ABI（本稿只把 world／system／scheduler 的禁令改成 Delta） | 需要串流載入時 |
| Optional Resource 參數 | 讓 D-E3 有漏洞 | 有 Feature |
| Kernel 內建 schedule 或 stage 名字 | DI-K4 中性；名字屬 Host 或 framework 的標準 ScheduleDef | 不重新考慮 |
| 執行期 id 上 wire 或進 digest | R-3；wire 由 `.proto` 定（N2） | 不重新考慮 |
| 跨執行緒轉送單一 System（v1） | X-6 | 出現 Serial 下必須跨角色的 System |
| 一律延一 tick 的雙緩衝事件 | mechanism §3.4 | — |
| required components、relationship／hierarchy（bevy_core:55；blueprint C3） | 沒有壓力（AGENTS §2） | 場景線的父子關係 |
| Kernel 擁有執行緒或 worker pool | 執行緒屬 Host 與 `threads`；worker pool 是另一個 Engine 能力 | 實作 Parallel 時 |
| 在本規格決定停功能線的條文、AGENTS 修訂、DET-0～3、開放 PR 的處置 | 使用者的決定（mechanism §9、HANDOFF） | — |

---

## 附錄一：與基準文件不一致之處（彙整）

| # | 文件與位置 | 本稿 | 理由與處置 |
|---|---|---|---|
| X1 | 任務敘述「Entity（index＋generation）」；提案 2、3 | 不帶 generation；產品 `EntityId` 就是鍵；配發器是 Resource | A.8 取捨 1；**要使用者決定** |
| X2 | blueprint DI-K5、networking §1 第 7 條「變更以遊戲 tick 標記」 | stamp 依 Plan 位置取值，`tickStamps` 提供 tick 視角；(c) 不用變更過濾 | A.8 取捨 2；**要使用者確認** |
| X3 | mechanism §3.1「進 digest 的欄預設獨占」 | 所有元件預設 Exclusive；digestSet 中有 Shared 是啟動錯誤 | Kernel 不知道 digest（A.8 取捨 3） |
| X4 | mechanism §3.3「佇列依順序套用」 | 明定為「階段＋id」，不用 FIFO（bevy_core:34）；entity-store DI 4 延伸到 insert、remove | F.8 取捨 1 |
| X5 | blueprint DI-K1「未排序即錯誤」不分區 | 只有 (c) 是錯誤；(b) 依登記序平手 | mechanism §3.2 審查修正 1、2 |
| X6 | blueprint DI-K2「同階段同一欄只有一個 writer」 | 範圍改成整個 World、所有 schedule；只限 Exclusive 欄 | 狀態的權威不分 stage；Shared 欄可多寫者 |
| X7 | mechanism §3.2 未提 hook 的歧義 | 主張 A 延伸到 hook（D-E9） | 登記順序在 hook 也會讓 plugin 順序改變結果 |
| X8 | mechanism §3.4「本 tick 可讀、保留到下 tick 末」 | 一致；另加「不跨 clock domain」與「無游標、被跳過的讀者錯過」 | F.3 |
| X9 | mechanism 圖 1「Executor 記 change tick」 | stamp 由 World 依 Plan 位置取、句柄在可變存取時寫 | A.1、X-5 |
| X10 | mechanism §2「World 存 id→位元組」 | 契約照舊；手段是登記時實例化有型別的欄再以函式表擦除（A.6 兩種包法由 spike 選） | 結果等價 |
| X11 | mechanism §3.3 的四處契約沒有登記時可檢查的形式 | 區規則 Z1～Z8（G.1） | 新增 |
| X12 | mechanism、v0.2 都沒寫 System 必須無狀態、(c) 禁 Local、事件 domain | S-1、S-7、F-9 | 新增 |
| X13 | bevy_core:29、:64「明確宣告 read／write，模板萃取只輔助」 | 照 mechanism §3.1 審查修正 4 從簽名推導；bevy_core 的做法是 spike 失敗時的退路 | B.8 |
| X14 | `docs/architecture.md:136`（Engine 只有 entity store，無 system／scheduler／world…）；entity-store DI 5 引用它；entity-store.md 的 `DeferredChanges` 列 | Kernel 進 Engine，這些要修訂；`DeferredChanges` 撤回 | Architecture Delta，要使用者同意 |
| X15 | entity-model E-12「新 id 依裁決順序配發」、blueprint DI-K3「依階段內順序配發」 | 配發在記錄時由配發器 Resource 配，順序由衝突分析序列化 | 等價：(c) 的配發順序是 Plan 的函數 |
| X16 | v0.2 §6.2／§6.4 的 `WorkerEligible`、`PlatformThread`、`GraphicsContextOwner` | 親和＝`Any` 或具名角色，角色由 Host 綁；`WorkerEligible`＝`Any`，不代表會並行 | E.3 |
| X17 | v0.2 §13 第 1～3 問 | 答：System＝操作 World 的函式，外部服務不是 System（I 節）；World 持 Resource（含 Link），不持服務本身（I-2）；跨時鐘、跨執行緒、跨 Runtime 的協調在 Host 不在 Scheduler（H.7） | — |
| X18 | mechanism §3.5 的三層 | 加第四層：(c) 區 System 來源檔清單可匯出並掃描（J-17） | B.5 |
| X19 | 提案 1 F-6「callback 拋出後 World 回 Ready 可繼續用」 | 改為 `Interrupted`（E.5） | E.8 取捨 1 |
| X20 | mechanism §3.3 Executor 列「RNG 是 resource」 | 亂數狀態是每實體的 (c) 元件（E-10 的 `Brain.rngState`，`rollback = Yes`），不是單一 Resource | 回滾與每實體獨立性：Resource 形式要全體共用一個序列，重放時任一實體的順序變動就牽連全部；元件形式與 entity-model E-10 一致（修訂：N9） |
| X21 | v0.2 §10.1 的圖：一個 Runtime 內含 Simulation Clock 與 Frame Clock | Runtime＝一個 World；兩個時鐘在不同執行緒時是兩個 Runtime（ClientSim／ClientFrame，H.4.2） | H-2：一個 World 一條執行緒；兩時鐘同執行緒時仍可以是一個 Runtime 兩個 schedule（H.2 第 3 點），所以 v0.2 的圖是其中一種組合，不是被否定（修訂：N9） |
| X22 | mechanism §3.3 Executor 列「Time 只來自 tick」 | tick 加被記錄的 boundary 取樣（`HostClockSample`） | 與 determinism.md DI 4 一致、與 mechanism 不一致；**要使用者裁決**（K.3 第 11 條；修訂：B6） |
| X23 | entity-model §5.2「Join／Leave 在 Tick 外（host 呼叫）」 | Ingress 寫請求進 ring、(c) System 在 Simulate 生成與刪除（H.4.1） | Z3 不允許 (b) 寫 role=State、Z5 不允許 (b) despawn 帶 (c) 欄的實體；**要使用者確認**（K.3 第 10 條；修訂：S3） |
| X24 | entity-model E-3「Reset 歸回 First」、§5 第 83 行的世代 | World 內無 Reset；世代切換＝Host 重建 World（A.1、H.7） | W-9 的「不重用」只在一個 World 內成立；跨世代的重用由重建 World 隔開（修訂：S4） |

## 附錄二：未查證

bevy_core.md 沒有證據行（要查證得回到 `~/Code/GitHome/bevy` 原始碼）：Bevy 的 Entity 是否帶 generation 與其佈局；查詢疊代順序；run condition 的求值時機與存取怎麼併入 System；set 條件的求值次數；System 是否允許捕獲（FnMut）；動態查詢 API；hook 的遞迴上限；與 Link 對應的東西。

另：`typeTag` 位址在 ICF 下是否會被合併（spike 已改成 `static inline char` 預防，本機未觀測到合併；L1 待驗）；MSVC x64 無捕獲 lambda 的呼叫慣例歧義（spike 驗）；MSVC `/std:c++20` 是否隱含 `/permissive-`（N2）；pv9 契約是否規定 refusal 的回覆時機（-audit；本輪 grep `network-architecture.zh-Hant.md` 無 refusal 條目）；`SdlGpuRenderDevice.cpp:122-150` 的裝置執行緒斷言（提案 3 引用，統合者未複核）；sdl_ttf 後端 `:58` 的 `TTF_Init`（同上）；`WriteFileAtomically` 以外 `io` 的檔案服務是否另有時鐘或亂數（S17 只抽查了一處）。

---

## §對抗檢查（v8-arch-kernel-spec-adversary）

檢查者：Opus／xhigh（架構師安排）。判定：不通過（7 blocker、18 should、9 nit），依 README `-spec` 列的判定回提案。統合者（Fable／high）逐條處置如下；「接受」的條目都已改進規格正文並在改處標「（修訂：代號）」。處置統計：**接受 32、拒絕 0、待使用者 1**（B6 的契約衝突；S7 已由使用者 2026-11-11 決定）。另有 7 條「接受」的處置牽涉使用者層級的決定（S1、S3、S4、S8、S11、S14、B4／S6 的常開檢查），列在 K.3 第 9～17 條。

| id | severity | 處置 | 理由與改處 |
|---|---|---|---|
| B1 | blocker | 接受 | 鍵確實不唯一（同 System 兩筆、兩個 hook 兩筆）。F.1 每筆紀錄加 `recSeq`（每佇列計數；hook 佇列獨立一個 planPos），排序鍵改成 (EntityId, ComponentId, planPos, recSeq)，F.6 改寫「鍵唯一」的論證並加 debug 相鄰鍵斷言；F.2 第 5 步「(planPos, recSeq) 最大者勝出」；F-2、J-32 加突變「丟掉 recSeq 用 `std::sort`」與三平台 golden；K.1 第 9 條改寫 |
| B2 | blocker | 接受 | Z1 改成「全部存取」＝reads ∪ writes ∪ membershipReads ∪ 事件 ∪ 延後寫 ∪ run 條件 ∪ SetRead；新增 Z7（(c) 不寫 external）；J-40 加 With／Without、Write (b) 欄、run 條件讀 (b) 三例；J-41 加 `ResMut<Link>`；K.1 第 8 條改寫 |
| B3 | blocker | 接受 | `Spawn` 改成型別化 `Spawn<Bundle<…>>`，bundle 元件進延後寫、計入寫者、受 Z2（A.1、B.1、B.3、B.4 第 4 點、F.6）；沒有 untyped spawn（F.1）。`EntityAllotter` 宣告 zone，模擬 World 中是 C，(b) 的 Spawn 即 Z2 違反。`SpawnWithId<Bundle<…>>` 定義存取與兩個用途，權威快照改走 Restore（A.1）。J-12、J-15 加 bundle 案例；K.1 第 1、6 條改寫 |
| B4 | blocker | 接受 | 擇一：`ExclusiveWorld` 只允許 zone=B，句柄唯讀全部、只能寫 zone=B 且 Shared，違反是常開檢查（新 S-13；S-10 改寫；Z4 加「無 ExclusiveWorld」；J-45）。因此它不是 Exclusive 欄的寫者、碰不到 (c) 資料，M 節編輯器檢視器的用途仍成立（改 Shared 或型別化）。K.1 第 1、2 條取消「非 exclusive」的豁免寫法 |
| B5 | blocker | 接受 | 單一來源：World 的 `tick[domain]`（A.1 資料模型、§0），由 clock schedule 的 `Run` 第一步推進（D.1、A.2），Host 不寫；在快照內，Rewind 一併還原（H.5）；Replay 跑到 `tick == tNow`。產品的 `FixedTime` 拆成只含步長的 `FixedStep`（G.2、G-6、G.7 第 3 條、H.2）；`FixedTickRuntime::tickId_` 降為 Host 簿記（L.1）。H-1 改寫；J-42 加突變「不還原 domain tick」 |
| B6 | blocker | **待使用者**（規格文字已改成一致） | 衝突屬實：mechanism §3.3「Time 只來自 tick」與 determinism.md DI 4「the tick, or a clock injected」互斥，Host judgement 區（(c)）本來就量到達時機。本稿採 DI 4 的形狀：`HostClockSample` 是 zone=C、role=Input 的 boundary，(c) 讀它合法（G.2 Executor 列、§0），並在 G.2 與附錄一 X22 標明與 mechanism 不一致。選項寫在 K.3 第 11 條：修 mechanism §3.3，或把 Host judgement 改 (b) |
| B7 | blocker | 接受 | 新增 Z8（role=Input 必須 rollback=No）、H-10 與啟動錯誤 D-E15（`rollbackEnabled` 的 ingress 必帶 `NotInRollback`，不能用 `rollbackSafe` 代替）；H.5 Replay 寫明 ring 只讀不寫、Host 在 Replay 中不寫 boundary；J-42 的合成 schedule 加 ingress stage 與兩個突變；K.1 第 8、12 條改寫 |
| S1 | should | 接受（含一項待使用者） | B.3 寫明寫者以 System 計、Startup schedule 不計、`ExclusiveWorld` 不計；寫出 `bodies_` 三個寫入點（`PvpMatch.cpp:301-304`、`:310-331`、`:571`、`:582`）會觸發 D-E2，本稿預期的解法是依 entity-model 第 139 行的欄位分類拆欄，不是併 System、不是改 Shared；替代方案「以 owner plugin 計」列為 K.3 第 9 條。L.3 PvpMatch 列同步 |
| S2 | should | 接受 | H.5 加 `Restore(world, snapshot)`，來源可以是 Host 從權威資料組出的**部分快照**（整欄替換成員集合、不算寫者、Z2 例外、不觸發 hook）；A.2 與 Host 寫入窗口的第 (3) 類改寫；H.4.2 ClientSim 列寫出 Reconcile 的注入點（Host 比較 PredictionHistory[T] 與權威快照，不一致就 Restore 再 Replay）；H.7 加 Host 職責；J-42 加部分快照案例 |
| S3 | should | 接受 | H.4.1：Ingress 只把 join／leave 請求寫進 ring；(c) 的 `ApplyMembership` 在 Simulate 以 `SpawnWithId<Bundle<…>>`／`Despawn` 生成與刪除（Z3、Z5 都滿足）；行為差異 (d) 交 -audit；附錄一 X23；K.3 第 10 條 |
| S4 | should | 接受 | A.1 定義 World 生命期＝世代，Reset＝Host 重建 World（Registry 重用）；W-9 加「World 內無歸零操作」與突變；H.7 加職責；F.2 階段順序改成 despawn、remove 先於 spawn、insert，使同窗「刪 X 再 SpawnWithId(X)」成立，並寫明「同窗生成又刪除新 id」的語義變成生成勝出（K.3 第 12 條）；J-32 加案例；附錄一 X24 |
| S5 | should | 接受 | I.2 的 `Link.inbound` 改成 `{arrivalNs, bytes}`：IO 執行緒收到時取樣一次，到達時刻成為資料；G.2 Ingress 列與 G.4 新增 `SubmitInput`（`MatchRuntimeHost.cpp:161`、`:218`、`:222`、`:262-265`）與 `JudgeConnectionQuality`（`:700-741`）的現況與機制形狀；粒度不變，golden 預期不變（-audit 確認）；H.4.1 行為差異 (a) 改寫 |
| S6 | should | 接受 | 引入「常開檢查」類別（§讀法）：untyped `Commands`（F.1、F-10）、(b) 的 `Despawn` 對帶 (c) 欄的實體（Z5）、`ExclusiveWorld` 的寫入（S-13）都在所有組態檢查；J-32b、J-45 的 release 目標必須斷言；與 error-handling 契約的接法列 K.3 第 17 條 |
| S7 | should | 已決定（使用者 2026-11-11：判定 (2) 成立、上限記為範圍の限界；J-5 已照原文改寫並註記） | 接受指正：J-5 不得改寫事前宣告。J-5 改回 README 判定 (2) 的原文三種繞路，逐一標出 spike 實測的攔截層級（(a)(b) 在 System 邊界、(c) 在存取當下），並寫明判定 (2) 依 spike 報告是「部分不成立」、狀態由使用者依 spike R-01／R-06 裁決；本規格不替 README 改判定（K.3 第 16 條） |
| S8 | should | 接受（Replacement Criteria 的改寫待使用者） | 已補讀 spike 報告。A.6 加第二層（System 邊界淨變化比對）與其上限；B.5 表依 spike 的 (a)(b)(c) 與 Limit 重寫；B.4 第 8 點採不可複製、不可移動的句柄，第 7 點的跳板改成直接呼叫（不再 tuple＋apply）；J-4 改成編譯失敗測試；I.3 的「包／重寫」分界改成 spike 建議的三個可檢查問題，L 的判準同步；K.2 第 1 條改寫成三個分項並說明原條件為何已觸發卻不應視為機制失效——這是 Replacement Criteria 的變更，列 K.3 第 13 條 |
| S9 | should | 接受 | B.4 第 1 點：`AddSystem<&Fn>()`（NTTP）與 `AddSystem<F>()`（無捕獲閉包型別，C++20 可預設建構），寫明執行期函式指標不能當模板引數；成本一句 |
| S10 | should | 接受 | A.6 新增「J-9 掃描的兩個白名單項」（`tlCurrent`、`TypeKey<T>` 的 `static inline char`）並說明兩者不參與排序；J-9 改寫並加突變「白名單加第三個符號」 |
| S11 | should | 接受 | W-6 改寫（Serial／Parallel 兩種形式）；E.2、X-2 寫明 v1 宣告 Parallel＝啟動錯誤 D-E17，不靜默降 Serial；K.1 第 14 條同步；K.3 第 14 條 |
| S12 | should | 接受 | F.3：hook 事件的 writerPos＝同步點 Plan 位置、tick＝當下；D.2 末段論證 hook 與 System 之間不會歧義（hook 掛在同步點節點）；F.4：Z6 對 hook 適用且只接受 rollback=Yes、Restore 不觸發 hook；新增 H-11、J-42c；J-36 加 hook 事件案例 |
| S13 | should | 接受 | 已補讀 replicon.md §1～§3。B.4 第 4 點新增 `SetRead<SetId>`（finish 時由 Registry 的具名集合展開成讀集合，不是動態查詢）；C.1 加 `namedSets`，`digestSet` 成為其中之一；B.1 加 `setReads`；D.1 第 5 條；M 節動態查詢列改寫；J-47 |
| S14 | should | 接受 | F.3、F-9：事件只在該 domain 的 clock schedule 內讀寫，其他同 domain schedule 讀寫＝F-E1；拿掉未定義的「schedule 在 domain 內的序」；H-1 同步；J-37 加案例；K.1 第 14 條；K.3 第 15 條 |
| S15 | should | 接受 | A.1 把 Derived 定義成順序條件（每個 rollbackEnabled schedule 內，讀者前必有同 schedule 的立即寫者），finish 檢查 D-E16；Z6 引用；快照內容寫明 Derived 不進；J-46 |
| S16 | should | 接受 | ColumnInfo 與 A.6：`trivialRelocate == std::is_trivially_copyable_v<T>`，登記時推出、不可覆寫；取回 `T*` 用 `std::launder` |
| S17 | should | 接受 | 已複核 `WriteFileAtomically.cpp:4`、`:15`（`<chrono>`、`<random>`）、`Assert.hpp:49-50`、`SdlLogBridge.cpp:24`。L.1 的 io 列改成「路徑與 JSON 留；檔案服務包」並寫明 `WriteFileAtomically` 不能判留；base 列改行號並列 `reportingAssertion`；platform 列加 `SdlLogBridge.cpp:24`；B.5、I-5 的行號同步；I.3 第 3 條的掃描項明列 `<chrono>`、`<random>`；附錄二加「io 其餘未抽查」 |
| S18 | should | 接受 | D.1 第 4 條列出配對表：只有「延後寫對延後寫、同欄」算歧義；延後寫對立即讀、對立即寫、對 With 不算；J-22 加通過與失敗兩組案例與突變 |
| N1 | nit | 接受 | C.6 的 PluginGroup 列改成「與 bevy_core:72 部分不一致：採宣告與報缺漏、不採拓撲排序」並給理由；D.5 的 FilteredAccess 列改「未查證」 |
| N2 | nit | 接受 | 已複核 `framework/CMakeLists.txt:9`、`tests/common/{base,math,entity_store}` 的 `/permissive-`、`engine/CMakeLists.txt:24` 只有 `cxx_std_20`。B.6 風險 8 改寫，`/std:c++20` 隱含 `/permissive-` 標未查證；附錄二同步 |
| N3 | nit | 接受 | I.2 與 I-4 改成可檢查的句子（Running 期間外部執行緒不存取 System 側緩衝、只在交換點 swap）並加 debug 持有者標記 |
| N4 | nit | 接受 | §0 boundary Resource 的定義改成「Host 在 Ready 寫，或 Ingress 在 ingress stage 執行期間寫」 |
| N5 | nit | 接受 | 已確認 `PvpMatch.cpp` grep `inTick_` 無結果、entity-model 第 3 行寫待辦。L.3 改成「取代的是尚未實作的守衛」 |
| N6 | nit | 接受 | D-9 定義「World 摘要」（不含 stamps、tickStamps、Plan 位置）；J-44 引用 |
| N7 | nit | 接受 | D.2 第 8 步拆成 8～8h，代號 D-E10～D-E17 一一對應（含新增的 D-E15～D-E17）；修正雙重否定與「H-6d」；D-5、J-24 改成完整代號表；Z6 多出的欄已移除 |
| N8 | nit | 接受 | 與 S8／S9 合併：跳板直接呼叫，`Fetch` 無副作用是契約，J-12 以兩種參數順序檢查；不再依賴 `[dcl.init.list]/4` |
| N9 | nit | 接受 | 附錄一加 X20（RNG 是元件不是 Resource）、X21（Runtime＝一個 World 對 v0.2 §10.1 的圖）；另加 X22～X24 記錄本輪新增的不一致 |

### 檢查者列出的未讀來源與本輪處置

| 來源 | 本輪 | 用在哪裡 |
|---|---|---|
| `spike-report.zh-Hant.md`、`spikes/kernel/` | **已補讀**報告全文（原型程式未讀） | A.6 兩層、B.4 第 7～8 點、B.5、I.3 三問、J-4、J-5、K.2 第 1 條 |
| `build/evidence/kernel-study/replicon.md` | **已補讀** §1～§3（第 1～40 行） | `SetRead`（B.4、C.1）、M 節動態查詢列 |
| `build/evidence/kernel-study/lightyear.md` | 部分補讀（§1～§4 的開頭） | H.5 的 PredictionHistory 與 Rollback 形狀未改；確認 Time<Fixed> 以外的時間 resource 不回滾與本稿 Z8 方向一致 |
| `docs/object_fps_pvp/network-architecture.zh-Hant.md`、protocol-v8／v9 | 只 grep refusal／無狀態／whitelist；refusal 回覆時機仍**未查證** | H.4.1 行為差異 (b) 保持未查證；I.1 Gateway 無狀態與 state whitelist 一致 |
| `docs/architecture/platform-sdl.md` | 只 grep live frame／PumpEvents／DI | L.1 platform 列加 DI 7 的歸屬（Host）；H.4.2 的 `PlatformEvents` 寫入點未改 |
| 本機 Bevy 原始碼 `~/Code/GitHome/bevy` | 未讀 | 附錄二的未查證項目維持 |
| `docs/architecture/entity-model.zh-Hant.md`（任務指定路徑） | 路徑不存在；本稿引用的一直是 `docs/object_fps_pvp/entity-model.zh-Hant.md`（本輪再讀第 3、83、91、139、237、242、244、264-265 行） | A.1、B.3、H.4.1、L.3 |

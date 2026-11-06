# 實體容器的參照調查：Bevy ECS 與 EnTT 的逐機制判定

更新：2026-11-05（`v8-arch-entity-store-survey`）。Owner：Engine（架構）。
參照版本：Bevy `v0.20.1`（commit `a0577b5c6a7b208719dd761c5c6a7a539e472a97`，`crates/bevy_ecs`、`crates/bevy_app`）；EnTT `v3.16.0`（commit `b4e58bdd364ad72246c123a0c28538eab3252672`，`src/entt/entity`、`docs/md/entity.md`）。

**狀態／用途**：本文件是 batch 2（`v8-arch-entity-store-delta`）寫 Engine 實體容器的 `Design Invariants` 與 `Replacement Criteria` 時的根據。每個判定都附上參照程式的行號 permalink（連結定義集中在 §7）。§1 引用的「架構師的假說」**尚未被使用者採納**；本文件只檢驗它，不決定它。判定「採／改裝後採／不採」是調查者的建議，不是核准。敵對檢查（Fable 5.1／xhigh）的 must 2、should 7、note 8 已全部處置，見 §6a。

## 1. 範圍與起點

**已由 entity-model 定下的（不重做）**：[entity-model.zh-Hant.md](../../../object_fps_pvp/entity-model.zh-Hant.md) §1 已調查 EnTT 與 flecs：§1.1（各自為何這麼做）、§1.2（約四成可直接用：每元件一個 packed 陣列、id 分段、用盡報錯、Client 只採用權威 id）、§1.3（約六成要改裝：保序 erase、lower_bound 插入、排序合併代替 view／group、二分搜尋代替 sparse 陣列、generation 機制不採而目的保留）、§1.4（不採：sparse set 主索引、archetype、owning group、每 Tick 排序、執行期查詢、change detection、observer）。核准的不變式是 E-1～E-12、RC-1～RC-5。

**本文件補的**：(1) Bevy ECS（entity-model 沒讀）；(2) 把 §1 的結論逐「機制」對到程式碼行號，並對到 GYO 的約束；(3) 檢驗下面的假說；(4) 把「不採」轉成 batch 2 的 Replacement Criteria 候選。flecs 不在範圍，只在 §1 已提及處引用。

**架構師的假說（原文照錄，是假說，不是結論）**：
> Engine 只收「容器」：Entity＝index＋generation、SoA column、依 id 順序的決定性迭代、以 Commands 延後結構變更、change tick。Engine 不收 system／scheduler／world／resources／events／app／plugin（通用執行期；理由與 `docs/architecture.md` 第 129 行禁止 service locator／umbrella framework 相同）。id 分段政策與實體種類留在產品。

**對照用的 GYO 約束**（下表「對 GYO 約束的套用」欄逐項對這些）：
- C++20；Engine 契約不用例外，Assert＝Programmer Error、Result＝Runtime Error（`docs/architecture/error-handling.md`）。
- 固定 Tick 的決定性 Match；權威 digest 依 EntityId 嚴格升冪編碼每一欄（E-5、E-6、E-9）；迭代順序只能取決於 id，不能取決於增刪歷史。
- 兩個配發者（Gateway 配玩家段、Match 配 Match 段，E-2）；Engine 不知道種類與分段（AGENTS §6）。「單調、一個世代內不回收」**只由 E-3 對 Match 段保證**；玩家段依 entity-model §5.3，Match 既不保證也不禁止重用（測試情境 Rejoin 用同一 id），正式環境的不重用靠 Gateway 的單調配發。所以「不需要 generation」是**配發者的保證**帶來的結果，不是容器本身的性質。
- E-12：Tick 內不改欄的成員；生成與刪除記入待辦，在固定階段依 EntityId 升冪套用。
- pv10 每收件者一份 WorldSnapshot、欄依 interest 過濾（entity-model §4、§8 第 5 項）；priority accumulator 是送出排程狀態，不進 digest（E-8）。
- 規模：同時約 400 個實體、約 6 種固定元件組合（§1.4、RC-1、RC-3）。
- AGENTS §1（不從單一 use case 做通用抽象）、§2（依觀測到的壓力；預測的壓力要標明）、§3（DI 與 RC 要在堆功能前寫）。

## 2. 主表

判定欄的意思：**採**＝機制照用；**改裝後採**＝目的與大致形狀照用、細節改（改了什麼列在 §4.2）；**不採**＝Engine 容器不收（收的條件列在 §4.1）。「採」不代表引入該函式庫：兩者都不以依賴形式進來（Bevy 是 Rust；EnTT 是 C++17，[CMakeLists.txt:96][e-cmake]），採的是機制。

### 2.1 Bevy ECS

| 機制 | 參照方為何這麼做 | 對 GYO 約束的套用（C++20、固定 Tick 決定性 Match、EntityId 升冪 digest、pv10 每收件者的欄分送、E-12） | 判定 | 根據（permalink） |
|---|---|---|---|---|
| `bevy_ecs` 作為獨立 crate | 讓不用 Bevy 引擎的人也能單獨用 ECS。依賴只有 `bevy_ptr`、`bevy_tasks`（非 optional）、`bevy_utils`、`bevy_platform`、巨集，`bevy_reflect` 是 optional（預設開）；`multi_threaded` 是 feature。反向：repo 內 46 個 crate 依賴它，`bevy_app` 也依賴它，它不依賴 `bevy_app` | 依賴方向的形狀（App 在外、ECS 在內）與我們的「Engine ← 產品」一致。但 **crate 內 world、schedule、system、query、observer、message 全部無條件編入**，只有 `reflect` 有 feature 閘；容器與執行期在 Bevy 是**同一個編譯單位，不可分別編入**（使用上仍可只用 World 層、不跑 Schedule）。所以「只收容器」在 Bevy 沒有先例，先例在 EnTT（2.2 的 registry 列） | 改裝後採（只採「模組不依賴上層」的方向；不採「容器＋執行期同捆」） | [Cargo.toml:14-26][b-cargo-feat]、[Cargo.toml:90-101][b-cargo-deps]、[bevy_app/Cargo.toml:85][b-app-cargo]、[lib.rs:29-57][b-lib-mods] |
| `Entity`（index＋generation） | `EntityIndex(NonMaxU32)`＋`EntityGeneration(u32)`，`repr(C)` 拼成 u64。despawn 時 `mark_free(index, 1)` 把 generation＋1 再還給 allocator；allocator 先取 free list、空了才 `fetch_add` 新 index。守的是「舊 handle 不誤指重用同 index 的新實體」；generation 會回繞，回繞時只 `warn!`（承認可能 alias）。`Ord` 比較 `to_bits()`，generation 在高 32 位元，所以**排序鍵是 (generation, index)** | E-3：Match 段單調、不回收 → 舊 id 只會「查不到」不會「指錯」，generation 恆為 0，機制沒有工作可做（entity-model §1.3 第 5 項已定）。玩家段的不重用靠 Gateway 的單調配發（§5.3），所以「不需要 generation」以兩個配發者的保證為前提。E-2：id 由兩個外部配發者給。**Bevy 不是「容器不配 id」的先例**：`spawn_at` 文件明說對未經 allocator 配出的 index 呼叫是壞主意（allocator 日後可能配出同一 index → panic）；文件的「自訂 allocator」是先 `alloc()` 再 `spawn_at`、以 `despawn_no_free` 控制重用，index 仍由 Bevy 配；`Commands::spawn` 一律向共用 allocator 取號；未見過的 index 能成功只是因為 `check_can_spawn_at` 讀到 `FRESH` 的 meta。digest 只用 id 數值：若帶 generation，id 升冪≠(generation, index) 升冪的情況只在回收時出現 | 改裝後採：Engine 的鍵＝外部給的單調 id，**不帶 generation**；generation 只在 RC-2（需要回收）時引入。「Engine 不配發」不是改裝自 Bevy（Bevy 保留容器為 id 權威），根據只有 E-2 | [entity/mod.rs:60-74][b-ent-life]、[:151][b-ent-index]、[:252-291][b-ent-gen]、[:424-433][b-ent-struct]、[:468-476][b-ent-ord]、[:566-567][b-ent-bits]、[:764-781][b-ent-alloc]、[:846-848][b-ent-fresh]、[:922-928][b-ent-canspawn]、[:989-1001][b-ent-markfree]、[:1198-1205][b-ent-freshmeta]、[remote_allocator.rs:773-778][b-remote-fresh]、[:853-858][b-remote-alloc]、[world/mod.rs:1060-1085][b-spawn-at]、[:1646-1655][b-despawn-nofree]、[commands/mod.rs:268-270][b-cmd-spawn] |
| Table／SoA column（`storage/table`；v0.20.1 的 `Column`＝`BlobArray`＋`ThinArrayPtr`） | 一張 table＝一種元件組合；每元件一欄（型別抹除的連續陣列），**同一列號在各欄是同一實體**；`entities: Vec<Entity>` 記列→實體。欄旁另有 `added_ticks`、`changed_ticks` 兩條等長陣列。目的：同組合的實體零間接線性走訪 | 「每元件一個連續陣列」與 GYO 的欄一致（E-5 的欄是 `std::vector<T>`）。但 Bevy 的欄是**按組合切開、按列對齊**，同一元件分散在多張 table；GYO 的欄是**一個元件一條、跨所有實體、按 id 排序**（交集靠排序合併，§1.3 第 3 項）。型別抹除（`BlobArray`）的目的推測是為了執行期註冊元件（推論，未在程式註解中確認）；GYO 的元件是編譯期已知的 6 種組合，C++20 的 `std::vector<T>` 就夠。註：任務說明寫的 `BlobVec`／`ThinColumn` 在 v0.20.1 已不存在（grep 0 件），以 clone 為準 | 改裝後採（一元件一條型別化欄、按 id 排序；不按組合切、不型別抹除） | [table/mod.rs:187-207][b-table]、[column.rs:9-31][b-column]、[table/mod.rs:489-496][b-table-alloc] |
| Archetype graph 與 table 間搬移（`archetype.rs`、`bundle/`） | 加減元件＝搬到另一個 archetype／table；`Edges` 快取「加這個 bundle 會到哪個 archetype」。搬移時舊 table `swap_remove`（最後一列填洞），新 table `allocate`＝push 到尾端。despawn 同樣 archetype 與 table 都 swap_remove。archetype 建了就不刪 | 每次搬移與刪除都改列順序，順序取決於增刪歷史，與 E-5 正面衝突（§3a）。組合是 6 種固定表（RC-3），archetype 的好處（動態組合下的快速交集）用不到；E-12 已規定 Tick 內不改成員，搬移本身也不該在 Tick 內發生 | 不採 | [archetype.rs:1-15][b-arch-doc]、[:195-215][b-arch-edges]、[:600-617][b-arch-alloc]、[:628-640][b-arch-swap]、[table/mod.rs:228-260][b-table-swap]、[:743-818][b-table-move]、[bundle/insert.rs:260-300][b-insert-move]、[bundle/remove.rs:255-310][b-remove-move]、[world_mut.rs:1807-1868][b-despawn-swap] |
| `StorageType::SparseSet`（元件可選 sparse set） | 加減頻繁的元件改放 sparse set，避免 table 搬移；代價是走訪多一層間接 | 同 entity-model §1.4：sparse 陣列以 index 為下標，靠回收才有界；GYO 單調不回收，sparse 陣列會長在死掉的區段 | 不採 | [component/mod.rs:763-770][b-storage-type] |
| `Commands`（延後結構變更） | 系統平行執行時只持有部分存取權，而結構變更需要獨佔 `World`，所以 spawn／despawn／insert／remove 先進每個系統自己的 `CommandQueue`，在 `ApplyDeferred` 同步點套用。**`Commands::spawn` 在入隊當下就向共用 allocator 取 id**（`fetch_add`）；套用順序＝系統的拓撲索引順序，同一系統內 FIFO。Commands 守的是**存取衝突**，不是「階段內不改成員」：exclusive system 拿 `&mut World` 可隨時增刪（結束時才 flush 命令），`World::spawn_at`、`despawn_no_free` 都是即時結構變更 | E-12 要的是「固定階段、依 EntityId 升冪」。Bevy 的保證是「依入隊順序」，不是 id 順序；而且 id 在入隊時就由 allocator 決定，平行系統下取號順序是競爭（推論，未量）。GYO 需要的只是「生成待辦＋刪除待辦，在呼叫端指定的階段排序後套用」，不需要可裝任意閉包的命令佇列（§3b） | 改裝後採（只留「延後到固定階段」與「套用順序決定性」；順序改成 id 升冪；不收任意 `Command`、不在入隊時配 id） | [commands/mod.rs:38-57][b-cmd-doc]、[:268-270][b-cmd-spawn]、[command_queue.rs:178-190][b-cq-apply]、[executor/mod.rs:134-157][b-applydef]、[multi_threaded.rs:764-784][b-mt-apply]、[single_threaded.rs:190-215][b-st-apply]、[schedule.rs:1852-1861][b-autoapply]、[function_system.rs:695-711][b-sys-exclusive]、[:735-739][b-sys-exclusive-flush]、[world/mod.rs:1078][b-spawn-at-fn]、[world_mut.rs:1700-1703][b-despawn-nofree-flush] |
| `Query`（迭代順序） | 外層走「符合的 archetype／table」（依它們第一次被 query 看到的順序 push），內層走列。官方文件對每個 iter 都寫「Iteration order is not guaranteed」。要固定順序就用 `iter().sort::<K>()`：每次呼叫把 (key, Entity) collect 成 Vec 排序，再逐實體隨機存取；**不跨系統執行快取**。另有 `iter_many(list)`：依呼叫端給的實體清單順序逐一取出（O(n) 隨機存取，不排序） | E-5／E-9 要求每欄依 id 升冪，且 digest 每段都是這個順序。Query 沒有任何順序保證；排序版本每 Tick 每個走訪點都要付 O(n log n)＋隨機存取，而且以 `Entity` 排序其實是 (generation, index)（§3a）。`iter_many` 可避開每 Tick 排序，但那份已排序的清單要由呼叫端維護——等於把 GYO 欄的不變式搬到呼叫端，且失去線性存取。執行期查詢語言本身也在 entity-model §1.4 不採 | 不採（容器自己的不變式就是順序，不需要查詢層） | [system/query.rs:650-654][b-q-noorder]、[query/state.rs:646-670][b-q-matched]、[query/iter.rs:544-546][b-q-sort-doc]、[:605][b-q-sort-nocache]、[:1015-1030][b-q-sort-impl]、[system/query.rs:853-856][b-q-itermany-doc]、[:918][b-q-itermany] |
| Change detection（元件 `Tick`、`Added`／`Changed`、`check_tick`） | 每個元件**每個實體**存兩個 u32：`added`、`changed`；`Mut<T>` 的 `DerefMut` 一被取用就 `set_changed`（不比值；要比值得用 `set_if_neq`）。World tick **每執行一個系統就＋1**；系統記 `last_run`；`Changed<T>` 判斷「changed 比 last_run 新」（以 this_run 為參考處理回繞）。超過 `MAX_CHANGE_AGE` 的舊 tick 由 `check_change_ticks` 定期夾住，`is_newer_than` 內也 `min(MAX_CHANGE_AGE)` 以保決定性 | 「容器每元件每實體一條 changed tick＋分送層每收件者每實體的 lastSent」是正確的組合，也不違反 E-8（§3c）；所以不採的理由不是「做不到」，而是：(a) C++ 沒有 `DerefMut` 可攔截，每個寫入點都要手動標記或經代理，漏一處＝某個 Client 永遠收不到那次更新（entity-model §1.4「API 滲透每個系統」）；(b) entity-model §1.4 已核准不採 change detection；(c) 沒有觀測到的消費者（AGENTS §2，預測的壓力） | 不採（第一版；假說的這一項與已核准的 §1.4 衝突，見 §5） | [column.rs:27-31][b-column]、[change_detection/mod.rs:13-26][b-cd-const]、[tick.rs:23-25][b-tick]、[:53-66][b-tick-newer]、[:77-92][b-tick-check]、[:196-221][b-cticks]、[traits.rs:221-233][b-setifneq]、[:493-500][b-derefmut]、[function_system.rs:709-742][b-sys-lastrun]、[query/filter.rs:1140-1148][b-changed-filter]、[world/mod.rs:3385-3422][b-world-checkticks] |
| 平行 scheduler（`schedule/`、system set、ambiguity） | 系統依宣告的存取權自動平行；`multi_threaded` feature 開時預設用 `MultiThreadedExecutor`。順序只由使用者加的約束決定；沒有約束又存取衝突的系統叫 ambiguity，**偵測預設 `Ignore`**。文件自承多執行緒 executor 會產生單執行緒拓撲排序下不可能的順序；`shuffle_seed`（只在 feature `debug` 下存在）是用來抓「碰巧正確」的順序 | 決定性 Match 的 Tick 是一條固定的函式序列（entity-model §5.1：預設不並行，並行要證明與升冪循序結果相同）。自動平行＋預設不報 ambiguity 正好是我們要排除的不決定性來源。Job 是 Engine 該補的「能力」（entity-model §0 第 2 項），不是 scheduler | 不採 | [executor/mod.rs:44-63][b-exec-default]、[:67-71][b-exec-topo]、[schedule.rs:1838-1845][b-ambig]、[:1869-1888][b-shuffle]、[:1900-1903][b-ambig-default] |
| `Resource`／`World` | `World` 收所有實體、元件、resource 與 metadata；v0.20.1 的 `Resource` 是 `Component` 的子 trait，resource 以實體存放（`ResourceEntities` 對照表） | `PvpMatch` 自己持有欄與 Tick 狀態；Engine 若提供 World，就是「萬物都經它」的 umbrella（architecture.md:129 的理由）。resource 存成實體還會讓 singleton 進入實體的 id 空間，與 E-1（一個 id 空間只給遊戲實體）的語意不合 | 不採 | [world/mod.rs:79-101][b-world]、[resource.rs:18-22][b-resource-doc]、[:87-91][b-resource] |
| `App`／`Plugin`（`bevy_app`） | App 自動化標準生命週期，plugin 在 `build` 裡把系統、resource 註冊進 App；同名 plugin 預設只能加一次 | 產品自己組裝 Engine（architecture.md:129「No umbrella framework … plugin ABI」；framework 層也不是 umbrella）。容器不需要知道誰組裝它 | 不採 | [app.rs:58-59][b-app]、[plugin.rs:1-20][b-plugin] |
| `Events`／`Messages` | `Messages<T>` 是雙緩衝，跨一次 `update` 存活，未讀就靜默丟棄；文件自承沒排序時讀寫是競爭。`Event` 是 `trigger` 當下同步跑 observer | Match 的事件（生成、刪除、擊殺）是權威結果，要進 digest（entity-model §5.4 的 `spawned`／`destroyed`）或走 B-1 的可靠串流；「靜默丟棄」與「讀寫競爭」都不能接受 | 不採 | [message/messages.rs:17-38][b-messages]、[event/mod.rs:15-19][b-event] |
| `Observers`（與元件 hook） | push 式：事件或元件生命週期（add／remove／despawn）一發生就跑；同一事件的多個 observer **順序任意**（文件明說） | 生成／刪除的副作用若藏在 observer，順序不決定、也不在固定階段，違反 E-12 與 E-6。entity-model §1.4 已不採 | 不採 | [observer/distributed_storage.rs:150-155][b-obs-order]、[world_mut.rs:1700-1790][b-despawn-hooks] |
| `Reflect` | 執行期型別資訊，供序列化、編輯器、場景；`bevy_ecs` 以 feature 閘（預設開） | E-11：投影是每元件一支手寫函式，不做執行期屬性系統或反射。編輯器與工具經資料契約交接（AGENTS §8），不經 runtime 反射 | 不採 | [lib.rs:47-48][b-reflect-gate]、[reflect/component.rs:81][b-reflect-comp] |

### 2.2 EnTT

| 機制 | 參照方為何這麼做 | 對 GYO 約束的套用（C++20、固定 Tick 決定性 Match、EntityId 升冪 digest、pv10 每收件者的欄分送、E-12） | 判定 | 根據（permalink） |
|---|---|---|---|---|
| `basic_registry` 作為「容器，不是框架」 | 文件〈Build your own〉：ECS 模組是一組容器，「不接管主迴圈或排程」。registry 持有各元件的 pool 與一個 entity storage；task graph（`organizer`）是另外的工具，且只產圖不執行 | 這是假說「Engine 只收容器」最直接的先例：成熟的 C++ ECS 確實把容器與執行期分開。但 registry 也帶著 `ctx()`（任意型別的全域變數表）與 signal mixin，這些是執行期的入口 | 改裝後採（「容器、不接管迴圈」照採；`ctx()`、signal、organizer 不收） | [entity.md:96-106][e-md-build]、[registry.hpp:503-518][e-reg-create]、[organizer.hpp:109-117][e-organizer] |
| `basic_sparse_set`／`basic_storage`（sparse＋packed、分頁） | sparse 陣列以 entity index 為下標、分頁（預設 4096），packed 緊密排列；storage 的元件也分頁（預設 1024），所以擴容不使參照失效。類別註解明寫「不保證插入順序，任何情況都不要假設順序」；`begin()` 從 packed 尾端往回走 | 一元件一個 pool 與 GYO 的欄同形；查找改二分搜尋（entity-model §1.3 第 4 項）。分頁帶來的「擴容不使參照失效」對 E-12 有意義，但保序插入（lower_bound）會位移後面的元素，分頁也救不了；E-12 用「Tick 內不改成員」解決，比指標穩定更簡單 | 改裝後採（一元件一個型別化陣列；不用 sparse 陣列、不分頁；分頁只在 RC-1 的「換結構但不改順序」時再看） | [sparse_set.hpp:139-153][e-ss-doc]、[:177-211][e-ss-page]、[:650-653][e-ss-begin]、[storage.hpp:212-217][e-st-doc]、[:237-260][e-st-page]、[config.h:46-51][e-config-page] |
| 刪除策略 swap_and_pop／in_place／swap_only | swap_and_pop（預設）：最後一個搬進洞再 pop，緊密但改順序。in_place：留 tombstone，順序與指標穩定，但不能排序（`sort_n` Assert「Sorting with tombstones not allowed」）、不能進 group。swap_only：只換到活躍區尾端並 bump version，entity storage 用它 | E-5 要「刪除後其他元素的相對順序不變」。swap_and_pop、swap_only 都改順序；in_place 保序但留洞，digest 與交集都要跳 tombstone。GYO 的保序 erase（memmove）是第四種策略，EnTT 沒有 | 不採；以保序 erase 取代（只有這一種，不做可選策略） | [fwd.hpp:17-26][e-policy]、[sparse_set.hpp:253-258][e-ss-swaponly]、[:264-279][e-ss-swappop]、[:282-286][e-ss-inplace]、[storage.hpp:338-354][e-st-pop]、[component.hpp:15-54][e-comp-traits]、[entity.md:1478-1513][e-md-pstab] |
| `sort`／`sort_as` | 原地排序 packed（及元件），可用比較子；`sort_as` 讓一個 pool 跟另一個的順序對齊，讓兩個 pool 一起走時少 cache miss | 目的（讓多欄對齊）採；機制改成不變式：所有欄恆依 id 升冪，所以永遠對齊、不需要執行期排序呼叫（entity-model §1.3 第 3 項）。digest 也不必先排 | 改裝後採（「排序」變成恆真的不變式，不是操作） | [sparse_set.hpp:998-1037][e-ss-sort]、[:1052-1054][e-ss-sortas]、[entity.md:664-700][e-md-sort] |
| `entt::entity` 的 version 位元（`entt_traits`） | uint32 拆 20 位元 index＋12 位元 version（uint64 是 32＋32）；`next` 時 version＋1 並跳過全 1（全 1 保留給 null／tombstone）。**traits 允許 `version_mask == 0`**：此時 `to_version` 恆 0、`construct` 只留 index。`create(hint)` 可提示 id（不保證採用）：`generate(hint)` 只在該 id 未見過或位於 free 區時採用，否則另配一個，呼叫端要比對回傳值 | E-3 的「generation 恆 0」在 EnTT 是可表達的設定（version 位元＝0），印證「無回收就不需要 version」。hint 不是權威：EnTT 和 Bevy 一樣保留容器為 id 權威，所以「容器不配 id」在 EnTT 也沒有先例，根據只有 E-2。但 EnTT 的 entity storage 仍會回收 free list（`generate()` 先取 `data()[free_list]`），version 0 時回收就會 alias——所以它的 version_mask＝0 只在「使用者自己保證不回收」時安全，與 GYO 的 E-3 一致。20 位元 index 約 104 萬，不夠 340 萬累計（§1.3 第 5 項） | 改裝後採（鍵型別可以沒有 version 位元；Engine 不回收、不配發——後者的根據是 E-2，不是 EnTT） | [entity.hpp:31-51][e-traits]、[:104-108][e-vmask0]、[:116-119][e-next]、[storage.hpp:1042][e-st-entity]、[:1135-1158][e-st-generate]、[registry.hpp:507-518][e-reg-hint]、[:544-551][e-reg-destroy] |
| Group（full／partial／non-owning） | owning group 把被擁有的 pool 前段重排成「同時有這些元件的實體」，交集零間接；代價：被擁有的 pool 不能再一般排序（`Cannot sort owned storage`）、兩個 group 不能擁有同一型別（`Conflicting groups`）、與 in_place 不相容；元件增刪時 `swap_elements` 重排 | 重排破壞 E-5。固定 6 種組合＋全欄同序，雙指標合併已是線性（entity-model §1.3 第 3 項）。non-owning group 文件自承耗記憶體且「盡量避免」 | 不採 | [group.hpp:103-130][e-group-handler]、[registry.hpp:1119][e-reg-groupconf]、[:1186][e-reg-nosort]、[entity.md:1987-2148][e-md-groups] |
| View 的迭代順序 | 多型別 view 以元素最少的 pool 領頭，順序跟它；可用 `use<T>()` 指定 | 迭代順序取決於「哪個 pool 最小」，最小者會隨增刪改變。GYO 的交集一律依 id 升冪合併 | 不採 | [entity.md:1897-1925][e-md-viewiter] |
| Tick 內的增刪規則 | 文件〈What is allowed〉：走訪中可以建立實體與元件，刪除只限目前這個，刪別的是 UB；分頁讓新增不使參照失效 | GYO 用「Tick 內完全不改成員」（E-12）取代「哪些增刪可以」的細則：規則更少、可以 Assert（`!inTick_`） | 不採（細則）；以 E-12 取代 | [entity.md:2236-2262][e-md-allowed] |
| `snapshot`／`snapshot_loader`／`continuous_loader` | snapshot 依 pool 的 packed 順序（`reach()`／`rbegin`）寫出，不依 id；`snapshot_loader` 要求目的 registry 為空並保留原 id；`continuous_loader` 維護遠端 id→本地 id 對照（`remloc`）供 client-server | E-1／E-3：Client 與 Match 同一個 id，不維護對照表（entity-model §1.3 第 6 項）。WorldSnapshot 是 pv 契約（protobuf）＋每元件手寫投影（E-11），不是 registry 傾印；而且 snapshot 的順序是 packed 順序，與 digest 的 id 升冪不同 | 不採 | [snapshot.hpp:94-123][e-snap-get]、[:193][e-snap-empty]、[:306-317][e-snap-cont]、[entity.md:1089-1103][e-md-snap] |
| Signal mixin／reactive storage（EnTT 的變更偵測） | 每個 storage 預設帶 signal（`on_construct`／`on_update`／`on_destroy`）；`on_update` 只在經 `patch`／`replace` 寫入時觸發；reactive mixin 收集「被觸發的實體」 | 與 Bevy 的 change tick 同一族：要每個寫入點經過 registry 的 API，才知道「變了」。GYO 的系統直接改欄的元素參照 | 不採 | [entity.md:339-365][e-md-observe]、[:481-499][e-md-reactive] |
| Context variables（`ctx()`） | registry 附一個以型別或數字 id 為鍵的 `any` 表，放全域狀態 | 等同 Bevy 的 resource；Match 狀態由 `PvpMatch` 的成員持有 | 不採 | [registry.hpp:1217-1222][e-reg-ctx]、[entity.md:1003-1010][e-md-ctx] |
| 錯誤處理（`ENTT_ASSERT`、`ENTT_THROW`） | 契約違反用 `ENTT_ASSERT`（可重定義、可關閉）；有例外時配置失敗路徑會 `ENTT_THROW` | 對到 GYO：重複插入、找不到要刪的實體的列等「呼叫端用錯」是 `GYO_ASSERT`（現行 `InsertEntity` 已如此）；容器不擲例外。id 用盡是配發者（產品）的 Result，不是容器的事（entity-model 決定 6） | 改裝後採（Assert 照 GYO 規則；不用例外） | [config.h:8-15][e-config-throw]、[:54-60][e-config-assert] |

## 3. 三個必須以程式碼確認的點

### 3a. 「archetype 與 EntityId 升冪 digest 衝突」

- **假說**：archetype／table 的儲存無法讓迭代依 id 順序，所以與 E-5、E-9 衝突。
- **程式實際的行為**：
  1. table 的列順序＝`allocate` 的 push 順序（[table/mod.rs:489-496][b-table-alloc]、[archetype.rs:600-617][b-arch-alloc]）。
  2. despawn：archetype 與 table 都 `swap_remove`，最後一列搬進被刪的位置，並更新被搬實體的位置（[world_mut.rs:1807-1868][b-despawn-swap]、[table/mod.rs:228-260][b-table-swap]）。
  3. 加減元件：`move_row` 從舊 table `swap_remove` 取出、在新 table 尾端 `allocate`，舊 table 的最後一列填洞（[table/mod.rs:743-818][b-table-move]、[bundle/insert.rs:260-300][b-insert-move]、[bundle/remove.rs:255-310][b-remove-move]）。
  4. Query 外層依 archetype／table 被 query 第一次看到的順序（`matched_storage_ids.push`，[query/state.rs:646-670][b-q-matched]），內層依列；所有 iter 都標「Iteration order is not guaranteed」（[system/query.rs:650-654][b-q-noorder]）。
  5. 同一元件（例如 Body）分散在每一個含它的 table；GYO 的 6 種組合裡 Body 至少在 3 種（P、A、B），digest 的 Body 段要跨 table 合併。
- **能否每 Tick 排序補回、代價**：`iter().sort::<Entity>()` 每次 collect 成 Vec、排序、再逐實體隨機存取，文件明寫不快取（[query/iter.rs:605][b-q-sort-nocache]、[:1015-1030][b-q-sort-impl]）。代價是每個需要順序的走訪點每 Tick 一次 O(n log n) 加上失去線性存取；400 實體下的絕對時間：推論（未量）。更大的問題不是時間而是**責任分散**：順序變成「每個走訪點記得排序」，漏一處就 desync，守衛只能逐點加。另外 `Entity` 的 `Ord` 比 `to_bits()`，generation 在高位（[entity/mod.rs:468-476][b-ent-ord]、[:566-567][b-ent-bits]），一旦回收，「依 Entity 排序」≠「依 index 排序」。
- **不必每 Tick 排序的路徑**：`Query::iter_many(list)` 依呼叫端給的實體清單順序逐一取出，文件明寫「依清單順序」（[system/query.rs:853-856][b-q-itermany-doc]、[:918][b-q-itermany]）。呼叫端若自持一份已排序的 `Vec<Entity>`，就能 O(n) 依 id 走、不必每 Tick 排序。所以 Bevy **有** O(n) 的 id 順序路徑；但那份排序清單的維護（插入、刪除時保序）落在呼叫端，等於把 GYO 欄的不變式原封搬到呼叫端，而且走訪變成逐實體隨機存取、失去線性。結論不變：順序是呼叫端的責任，不是容器的保證。
- **判定：假說成立。** archetype 不採（與 entity-model §1.4 一致），並以 Bevy 的程式碼補上根據：列順序取決於 spawn、despawn、加減元件的歷史。

### 3b. 「Commands 可以在結構上守住 E-12」

- **假說**：Bevy 的 Commands 把結構變更延後到同步點，可借來守 E-12。
- **程式實際的行為**：
  1. 延後的原因是存取權：文件說「每個命令都需要獨佔 `World`」，平行的系統沒有獨佔權，所以每個系統把命令推進自己的佇列（[commands/mod.rs:38-41][b-cmd-doc-why]）。**Commands 守的是存取衝突，不是階段紀律**：Bevy 本身不保證「某個階段內不改成員」——exclusive system 拿 `&mut World`，執行中可以隨時 spawn／despawn，結束時才 flush（[function_system.rs:695-711][b-sys-exclusive]、[:735-739][b-sys-exclusive-flush]）；`World::spawn_at`（[world/mod.rs:1078][b-spawn-at-fn]）與 `despawn_no_free`（內部立刻 `flush`，[world_mut.rs:1700-1703][b-despawn-nofree-flush]）都是即時的結構變更。
  2. 套用時機：`ApplyDeferred` 系統。預設在 schedule 結束後、以及有順序依賴且前者有延後緩衝的系統之間自動插入；文件註明「改 Schedule 可能改變緩衝套用的順序」（[executor/mod.rs:134-157][b-applydef]、[schedule.rs:1852-1861][b-autoapply]）。
  3. 套用順序：executor 依「未套用系統」的索引（拓撲排序後的索引，[executor/mod.rs:67-71][b-exec-topo]）逐一呼叫 `apply_deferred`（[multi_threaded.rs:764-784][b-mt-apply]、[single_threaded.rs:190-215][b-st-apply]）；每個佇列內從頭依序執行（[command_queue.rs:178-190][b-cq-apply]）。所以在 schedule 不變時**順序是決定性的，但它是入隊順序，不是 id 順序**。
  4. id 何時決定：`Commands::spawn` 在入隊當下呼叫 `allocator.alloc()`（[commands/mod.rs:268-270][b-cmd-spawn]）；allocator 先取 free list、否則 `fetch_add(1, Relaxed)`（[remote_allocator.rs:853-858][b-remote-alloc]、[:773-778][b-remote-fresh]）。兩個平行系統同時 spawn 時，誰拿較小的 id 取決於執行緒的先後：推論（未量，由程式結構推得，未實際重現）。free list 的重用也讓 id 取決於刪除歷史。
- **GYO 要怎麼改裝**：E-12 已規定要的東西比 Commands 少：(1) 兩份待辦（生成、刪除），不是可裝任意閉包的佇列；(2) 套用點由呼叫端（`PvpMatch` 的固定階段：`ResolveActions` 之後、`AdvanceAnimation` 之前）指定，不由 scheduler 推導；(3) 套用前依 EntityId 升冪排序；(4) 新 id 由產品的配發器依裁決順序 (A, EntityId, ActionId) 配（E-12、E-2），容器不在入隊時配 id。E-12 的「單一延後入口＋固定階段套用」在 C++ **可以在結構上表達**：型別分離（Tick 內只拿得到「記入待辦」的介面，拿不到可增刪的介面），或執行期 Assert（現行 E-12 的 `GYO_ASSERT(!inTick_)`）——哪一種由 batch 2 決定。C++ 缺的只是「參照不跨越套用點」的編譯期證明（Rust 借用檢查能給的那一種），而那不是 E-12 的條文。
- **判定：部分成立。** 「延後到固定階段、套用順序決定性」可以採，而且在 C++ 可以用型別分離在結構上守住。但它不是向 Bevy 的 Commands 借來的：Commands 守的是存取衝突，Bevy 自己有多條即時改結構的路徑，不提供階段紀律；順序也要從入隊順序改成 id 升冪。容器能提供的是「待辦＋指定階段的排序套用」，不是 Commands 這個通用機制。

### 3c. 「change tick 可以服務 pv10 的每收件者分送」

- **假說**：元件的 change tick 可以用來判斷「這一欄要不要送給收件者 R」。
- **程式實際的行為**：
  1. 記錄的東西：每個元件、每個實體各兩個 u32——`added` 與 `changed`（[tick.rs:196-221][b-cticks]），在 table 的欄旁以兩條等長陣列存放（[column.rs:9-31][b-column]）。
  2. 何時寫：`Mut<T>` 被 `DerefMut` 時就 `set_changed`，**不比較值**（[traits.rs:493-500][b-derefmut]）；要比值得改用 `set_if_neq`（[traits.rs:221-233][b-setifneq]）。
  3. tick 的單位：World tick 在**每個系統執行時**＋1；系統執行完把自己的 `last_run` 設成這次的 tick（[function_system.rs:709-742][b-sys-lastrun]）。`Changed<T>` 判斷 `changed.is_newer_than(last_run, this_run)`（[query/filter.rs:1140-1148][b-changed-filter]）。比較基準是「讀者上次執行」，每個讀者（系統）一個數值。
  4. 回繞：u32 tick 用 `wrapping_sub` 算相對年齡；`CHECK_TICK_THRESHOLD = 518,400,000`、`MAX_CHANGE_AGE = u32::MAX − (2N − 1)`；`check_change_ticks` 在 world tick 前進超過門檻後把比 `MAX_CHANGE_AGE` 舊的 tick 夾住；`is_newer_than` 內也 `min(MAX_CHANGE_AGE)`，註解說「clamp 是為了決定性（掃描時機在不同執行間可能不同）」（[change_detection/mod.rs:13-26][b-cd-const]、[tick.rs:53-66][b-tick-newer]、[:77-92][b-tick-check]、[world/mod.rs:3385-3422][b-world-checkticks]）。
- **對到 pv10**：
  1. 「元件在 tick T 之後是否變過」只需要每元件每實體一個 changed tick（它是單調的：最後變更 > T ⇔ T 之後變過），對任意 T 成立。所以「R 上次確認的 Tick 之後變過嗎」**對單一實體**可以用一個 tick 回答，R 只要存一個數值。
  2. 但 pv10 的每收件者份是**依預算選實體**（entity-model §4.3、§8 第 5 項）：Tick T 時實體 X 的變更若因預算沒放進 R 的那一份，R 確認 T 之後，「X 在 R 確認之後變過嗎」答「否」，X 的變更就永遠漏送。所以要再記「R 對 X 最後收到的是哪一 Tick」（`lastSent[R][X]`）——**每收件者 × 每實體**的狀態，屬於送出排程（priority accumulator 那一類，依 E-8 不參與判定、不進 digest、可重建），放在 Match 的分送層。**「容器每元件每實體一條 changed tick＋分送層的 `lastSent[R][X]`」是正確的組合，也不違反 E-8**；一條 tick 欄單獨不夠是事實，但這不構成否定的理由。
  3. pv10 目前規劃的公開通道是 latest-wins 的完整狀態（Body、Animation，entity-model §4.1），不是對已確認基準的差量；私人通道的「基準＋差量」（Inventory）在背包批才有。現在沒有觀測到需要 change tick 的消費者：引入它是**預測的壓力**（AGENTS §2）。
  4. 需要「這一 Tick 誰變了」時，不侵入寫入點的替代：分送層保留上一份已送出的值，逐欄比對（等同 `set_if_neq` 的語意，但只在分送層做一次）。它是可重建的派生暫存（E-8）。成本：推論（未量）。
- **判定：不採（第一版）。** 上面第 2 點的組合可行，所以不採不是因為「做不到」，理由是：(a) C++ 沒有 `DerefMut` 可攔截（Bevy 靠它在取得可變參照時自動標記，[traits.rs:493-500][b-derefmut]），每個寫入點都要手動標記或經代理，漏標一處＝某個 Client 永遠收不到那次更新，而這種漏標測不到（entity-model §1.4「API 滲透每個系統」）；(b) entity-model §1.4 已核准不採 change detection；(c) 沒有觀測到的消費者（第 3 點；AGENTS §2，預測的壓力）。引入條件列為 RC 候選（§4.1）。

## 4. 不採與改裝後採

### 4.1 不採的項目＝Replacement Criteria 的候選

| 不採的機制 | GYO 會需要它的條件（RC 候選） |
|---|---|
| generation（Bevy `EntityGeneration`、EnTT version 位元） | 需要回收 id：一個世代內實際用盡，或要跨世代保留 id（＝entity-model RC-2）；或玩家段失去 Gateway 的單調配發保證。同時要決定 digest 編碼 generation 與否 |
| 內建 id 配發器（Bevy `EntityAllocator`、EnTT `create()`） | 兩個以上產品要相同的配發政策（AGENTS §1：單一 use case 不一般化）。在那之前配發政策留在產品（E-2、E-3）。注意：兩個參照都把容器當 id 權威，「容器不配發」沒有參照先例，根據只有 E-2 |
| archetype／table 與搬移、`StorageType::SparseSet`、sparse set 主索引 | 元件組合變成執行期動態且成為多數，或種類 > 16（＝RC-3）；或實測同時實體 > 4,000、欄維護 p95 > 1 ms（＝RC-1）。換結構時仍不得改變順序（RC-1 的但書） |
| `Query`／view／group／執行期查詢 | 交集的組合數多到手寫雙指標合併的函式超過可列舉的範圍（例：加一種實體要改 > 3 個判定函數，＝RC-4） |
| change detection（Bevy tick、EnTT signal／reactive） | 某個 pv 通道改成「對已確認基準的差量」，且在 Linux Match 上實測「分送層比對上一份」每 Tick 的 p95 超過門檻（沿用 RC-1 的 1 ms，或由 batch 2 另定）；或出現非分送的消費者（例如工具的增量同步）需要元件級的變更集合 |
| 平行 scheduler、system set、ambiguity 偵測 | Match 的 Tick 實測超出預算，且證明並行結果與 id 升冪循序逐位元相同（entity-model §5.1 的條件）；即使如此，先用 Engine 的 Job 能力，不收 scheduler |
| `World`、`Resource`、`ctx()` | 第二個產品要同一套「容器之外的全域狀態」組裝方式（framework 層的「第二個遊戲以同樣方式組裝時才抽出」，architecture.md:129） |
| `App`／`Plugin` | 同上；且 architecture.md:129 的「plugin ABI」禁止要先經使用者改寫 |
| `Events`／`Messages`／`Observers`／hook | 出現可指認的規則：某條判定規則必須在**同一 Tick 內**讀到它自己剛生成的實體（或剛刪除的實體在同一 Tick 內必須立刻對其他規則不可見），而 E-12 的固定階段套用無法滿足它。目前沒有這樣的規則。措辭由 batch 2 定案 |
| `Reflect` | 工具或編輯器需要不經資料契約直接讀寫執行期元件（AGENTS §8 目前禁止；條件成立前要先改 §8） |
| `snapshot`／`continuous_loader` | Client 與 Match 改用不同 id 空間（E-1、E-3 失效，＝RC-5 的方向） |
| 可選刪除策略（swap_and_pop／in_place／swap_only；以保序 erase 取代） | 出現不需要 id 升冪的欄（例如純診斷、不進 digest、不交集的欄），且實測保序 erase 讓欄維護超過 RC-1 的門檻（Linux Match 每 Tick p95 > 1 ms） |

### 4.2 改裝後採：改了什麼

| 採的機制 | 改裝的內容 |
|---|---|
| 「容器，不接管迴圈」（EnTT registry；Bevy crate 的依賴方向） | 只收欄與欄的操作；不收 `ctx()`、signal、organizer、world、schedule。Engine 模組不依賴任何產品，產品依賴它 |
| Entity 鍵 | 外部給的單調 id（uint64 內部、wire uint32，現行 `EntityId.hpp`）；沒有 generation 位元（前提：配發者保證不重用——Match 段由 E-3、玩家段由 Gateway 的單調配發）；Engine 不配發、不知道分段（E-2、E-4）。「不配發」沒有參照先例，根據只有 E-2 |
| 一元件一條連續陣列（EnTT pool；Bevy column 的「每元件一條」部分） | 型別化 `std::vector<T>`，不型別抹除；跨所有實體一條，不按組合切；元素依 id 嚴格升冪、無重複（E-5）。刪除只有保序 erase（memmove）；EnTT 的三種刪除策略不採（§4.1） |
| 插入 | lower_bound 位置插入（不是 append）；重複插入是 Programmer Error（`GYO_ASSERT`） |
| `sort`／`sort_as` 的目的（多欄對齊） | **只保留目的，不保留形狀**：沒有排序操作或 API；對齊變成恆真的不變式；交集是雙指標合併 |
| Commands 的目的（延後結構變更） | 兩份待辦（生成、刪除），由呼叫端在指定階段套用，套用前依 id 升冪排序；不收任意閉包；不在入隊時配 id；Tick 內禁止直接增刪，以型別分離（結構上）或 Assert（執行期）守住，batch 2 選。這是 E-12 的實作，不是 Bevy Commands 的移植（Commands 守的是存取衝突，§3b） |
| 錯誤處理 | 呼叫端用錯＝`GYO_ASSERT`；容器不回 Result、不擲例外（容器沒有「外部資料導致失敗」的入口；id 用盡屬於配發者） |

## 5. 對假說的結論

| 假說的要素 | 結論 | 根據 |
|---|---|---|
| Entity＝index＋generation | **部分支持**。「實體鍵是單一整數 id」支持；「generation」與已核准的 E-3、entity-model §1.3 第 5 項**矛盾**：配發者保證不重用（Match 段由 E-3、玩家段由 Gateway 的單調配發），generation 恆 0、機制無工作可做；這是配發者的保證，不是容器的性質。EnTT 有沒有 version 位元的設定（`version_mask == 0`）；Bevy 沒有。另外「index」一詞暗示 Engine 配發與回收 index，與 E-2（兩個外部配發者）不合：容器的鍵應是外部給的 id。這一點**沒有參照先例**（Bevy 的 `spawn_at` 與 EnTT 的 `create(hint)` 都保留容器為 id 權威），根據只有 E-2 | 2.1 `Entity` 列、2.2 version 位元列、§4.1 第 1～2 列 |
| SoA column | **支持（改裝）**。「每元件一條連續陣列」支持；Bevy 的 table 是「按組合切、按列對齊」的 SoA，與 GYO「每元件一條跨所有實體、按 id 排序」不同。採的是後者 | 2.1 Table 列、2.2 sparse set／storage 列 |
| 依 id 順序的決定性迭代 | **支持**，而且兩個參照都**不以不變式提供**、只以操作提供：Bevy 每個 iter 標不保證順序，要順序就 `sort::<Entity>()`（每次排序）或 `iter_many`（呼叫端自持排序清單）；EnTT 的 sparse set／storage 註解明寫不要假設順序，要順序就 `registry.sort` 依 entity 排序（[entity.md:664-683][e-md-sort]）。不變式不能「借」，只能由容器自己的保序插入與 erase 提供 | §3a、2.1 Query 列、2.2 sparse set／刪除策略／sort 列 |
| 以 Commands 延後結構變更 | **部分支持**。延後到固定階段、順序決定性：支持。「Commands」這個通用機制：否定（任意閉包、入隊時配 id、入隊順序而非 id 順序；Commands 守的是存取衝突，不是階段紀律——Bevy 有 exclusive system、`spawn_at`、`despawn_no_free` 等即時改結構的路徑）。容器該提供的是 E-12 的「待辦＋排序套用」，在 C++ 可用型別分離在結構上守住 | §3b、2.1 Commands 列 |
| change tick | **否定**（Engine 容器第一版不採）。「容器的 changed tick＋分送層的 `lastSent[R][X]`」是可行且不違反 E-8 的組合，所以理由不是做不到，而是：(a) C++ 無 `DerefMut` 攔截，每個寫入點手動標記，漏一處＝某個 Client 永遠收不到；(b) 與已核准的 entity-model §1.4（change detection 不採）衝突；(c) 沒有觀測到的消費者（AGENTS §2 的預測壓力）。列為 RC 候選 | §3c、§4.1 change detection 列 |
| 不收 system／scheduler | **支持**。scheduler 是 Bevy 不決定性的主要來源（自動平行、ambiguity 預設 Ignore）；EnTT 也刻意不收 | 2.1 scheduler 列、2.2 registry 列 |
| 不收 world／resources | **支持**。Bevy 的 World 就是 umbrella；v0.20.1 的 resource 甚至存成實體 | 2.1 Resource／World 列、2.2 ctx 列 |
| 不收 events | **支持**。雙緩衝的靜默丟棄、observer 的任意順序都與權威事件不合 | 2.1 Events／Messages、Observers 列 |
| 不收 app／plugin | **支持**。理由與 architecture.md:129 一致 | 2.1 App／Plugin 列 |
| id 分段政策與種類留在產品 | **支持**。根據是已核准的 E-2（兩個外部配發者）與 E-4（段不表示種類）、AGENTS §6（Engine 不知道產品）。參照不支持也不否定：兩者都保留容器為 id 權威（Bevy 的 `spawn_at` 用在未配發的 index 會被文件警告；EnTT 的 `create(hint)` 只是提示），所以這一點沒有參照先例 | E-2、E-4、2.1 Entity 列、2.2 version 位元列 |

**和假說不一致的地方**：(1) generation 與 E-3 矛盾（以配發者的保證為前提）；(2) change tick 與 entity-model §1.4 矛盾（單獨一條 tick 欄對 pv10 不夠，但配上分送層的 `lastSent` 可行，所以這一點不是否定的理由；否定的理由是 §3c 的 a～c）；(3)「Commands 在結構上守住 E-12」的前提不對：Bevy 的 Commands 守的是存取衝突，不是階段紀律；E-12 在 C++ 可以用型別分離在結構上守住，但那是 GYO 自己的設計，不是從 Commands 借來的；(4)「index」暗示 Engine 配發，與 E-2 不合，而「容器不配發」沒有參照先例。另外 Bevy 本身沒有「只收容器」的先例（容器與執行期在同一 crate，不可分別編入），先例只有 EnTT。

**範圍外但 batch 2 要面對的事實**：假說的五項裡，經本調查後留在容器的是「外部 id 鍵、每元件一條排序欄、依 id 的交集與迭代、待辦的排序套用」。這和 pvp 現行 `EntityColumns.hpp`（三個自由函數）與 E-12 的待辦幾乎同形。所以 batch 2 要回答的不只是「Engine 收什麼」，還有「目前只有一個消費端（pvp），搬進 Engine 的根據是不是觀測到的壓力」（AGENTS §1、§2）。使用者 2026-11-05 已決定搬（案 (b)），本文件不重新討論；只記錄：容器的 API 面很小，Replacement Criteria 要在它長大之前寫好（AGENTS §3）。
另外，本文件六次以 `docs/architecture.md` 第 129 行當作不收 world、app、plugin 的根據，但**同一句也禁止 ECS**（「No umbrella framework, scene hierarchy, ECS, plugin ABI or service locator…」）。把容器搬進 Engine，這一行本身就得在 Delta 內修改（依 README，由圖書管理員會話在同一條 PR 內拿掉 ECS 並加連結）。batch 2 的 DI 引用第 129 行時，要以修改後的文字為準，並明示「保留 umbrella／plugin ABI／service locator 的禁止、只拿掉 ECS」，免得變成選擇性引用。

## 6. 推論與未量測

- 400 實體下每 Tick 以 `sort::<Entity>()` 補回順序的絕對成本：推論（未量）。§3a 的判定不依賴這個數字，依賴的是「責任分散到每個走訪點」。
- Bevy 平行系統同時 `Commands::spawn` 時 id 取號順序不決定：推論（未量），由 `fetch_add(Relaxed)` 與平行 executor 的程式結構推得，未實際重現。
- 「分送層比對上一份」取代 change tick 的成本：推論（未量）。
- 保序 erase、lower_bound 插入的成本：沿用 entity-model §13 的推論與 RC-1 的門檻（未量）。
- EnTT `version_mask == 0` 時回收會 alias：由 `generate()` 先取 free list 的程式推得（推論，未以測試確認）。
- 本文件沒有執行任何參照程式（依規定只讀）；所有行為描述來自原始碼與原始碼內的文件註解。

## 6a. 敵對檢查的處置

檢查者 Fable 5.1／xhigh（唯讀），2026-11-05。報告原文存在主 checkout 的 `build/evidence/entity-store/survey-adversarial-check-20261105.md`（git 管理外）。處置時每一條引用都在 clone 上重新核對過行號。

| 嚴重度 | 發現 | 處置 |
|---|---|---|
| must-1 | 把「Commands 在 Rust 守住、C++ 守不住 E-12」混成一件事；Bevy 的 Commands 守存取衝突，不守階段紀律 | 採修正：2.1 Commands 列、§3b 第 1 點與判定、§4.2 Commands 列、§5 Commands 列與不一致段 (3) 改寫；補 exclusive system、`spawn_at`、`despawn_no_free` 的根據；判定仍為「部分成立」 |
| must-2 | Bevy `spawn_at`／`despawn_no_free`、EnTT `create(hint)` 被誤引為「容器不配 id」的先例 | 採修正：2.1 Entity 列、2.2 version 位元列、§4.1 配發器列、§4.2 Entity 鍵列、§5 兩列改寫：兩個參照都保留容器為 id 權威，「不配發」的根據只有 E-2；補 `check_can_spawn_at`／`FRESH`、`spawn_at` 的警告、`generate(hint)` 的根據 |
| should-1 | 3c 的否定理由落點錯：changed tick＋分送層 `lastSent[R][X]` 是正確組合 | 採修正：§3c 對到 pv10 第 2 點承認組合可行；判定改為「不採（第一版）」，理由 a～c；刪掉「tick 是系統執行次數」的論證（2.1 change detection 列、§3c 原第 5 點） |
| should-2 | 刪除策略在 §2.2 判「改裝後採」、在 §4.1 列「不採」，互相矛盾；sort 的形狀是否保留沒寫 | 採修正：2.2 刪除策略列改「不採；以保序 erase 取代」，§4.2 刪掉該列、併入欄的那一列；§4.2 sort 列寫明「只保留目的，不保留形狀」 |
| should-3 | §4.1 有四條 RC 候選無法檢查 | 採修正：change detection 引 RC-1 的 1 ms p95（或 batch 2 另定）；Events 改寫成可指認的規則（同 Tick 讀到自己剛生成的實體），措辭由 batch 2 定；刪除策略引 RC-1 門檻；配發器刪掉「第三個配發者」 |
| should-4 | 引用 architecture.md:129 六次，但同一句也禁止 ECS | 採修正：§5「範圍外」段補一段：第 129 行本身要在 Delta 內修改，DI 引用時以修改後的文字為準 |
| should-5 | 「不回收、舊 id 不指錯」的範圍過寬（E-3 只管 Match 段） | 採修正：§1 約束、2.1 Entity 列、§4.1 generation 列、§4.2 Entity 鍵列、§5 Entity 列寫明「不需要 generation」以配發者的保證為前提 |
| should-6 | 漏了 `Query::iter_many`（呼叫端自持排序清單時 O(n)） | 採修正：2.1 Query 列與 §3a 補上；結論不變（順序是呼叫端的責任、失去線性存取） |
| should-7 | 「兩個參照都不提供 id 順序迭代」說過頭 | 採修正：§5 改為「都不以不變式提供，只以操作提供」，引 EnTT `registry.sort` 與 Bevy `sort::<Entity>()` |
| note-1 | 表頭「仕組み（機制）」是日語 | 採修正：改為「機制」。HANDOFF.md:30 的「乘せ換え」不在本文件，本批不改（交給協調者） |
| note-2 | [b-resource] 行號 87-90 應為 87-91 | 採修正 |
| note-3 | 「不可分割」過強 | 採修正：改為「不可分別編入（使用上仍可只用 World 層）」 |
| note-4 | 「型別抹除是為了執行期註冊元件」沒標推論 | 採修正：標為推論 |
| note-5 | `shuffle_seed` 只在 feature `debug` 下存在 | 採修正：寫明，引用延到 schedule.rs:1888 |
| note-6 | `BlobVec`／`ThinColumn` 為 0 件，實作者說法正確 | 無需修改 |
| note-7 | 數字核對全部相符 | 無需修改 |
| note-8 | 「`create(hint)` 可要求指定 id」不精確 | 採修正：改為「可提示 id（不保證採用）」，與 must-2 一起處理 |

## 7. 根據的所在

- 參照程式：shallow clone，位於本會話 scratchpad `ref/bevy`、`ref/entt`（**不保存**，會話結束即消失）。耐久的出處是下列 permalink（tag＋路徑＋行號）；tag 與 commit：Bevy `v0.20.1`＝`a0577b5c6a7b208719dd761c5c6a7a539e472a97`、EnTT `v3.16.0`＝`b4e58bdd364ad72246c123a0c28538eab3252672`。行號以上述 commit 的 clone 用 `grep -n`／`sed -n` 取得。
- 本 repo 的根據：[entity-model.zh-Hant.md](../../../object_fps_pvp/entity-model.zh-Hant.md)（§1、§4、§5、E-1～E-12、RC-1～RC-5）、`apps/object_fps_pvp/include/RetroFPS/Pvp/EntityColumns.hpp`、`EntityId.hpp`、`docs/architecture/error-handling.md`、`docs/architecture.md` 第 129 行。
- 本批沒有另存證據檔（調查只有讀取，輸出就是本文件）。

<!-- Bevy v0.20.1 -->
[b-cargo-feat]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/Cargo.toml#L14-L26
[b-cargo-deps]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/Cargo.toml#L90-L101
[b-app-cargo]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_app/Cargo.toml#L85
[b-lib-mods]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/lib.rs#L29-L57
[b-reflect-gate]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/lib.rs#L47-L48
[b-ent-life]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L60-L74
[b-ent-index]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L151
[b-ent-gen]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L252-L291
[b-ent-struct]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L424-L433
[b-ent-ord]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L468-L476
[b-ent-bits]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L566-L567
[b-ent-alloc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L764-L781
[b-ent-markfree]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L989-L1001
[b-remote-fresh]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/remote_allocator.rs#L773-L778
[b-remote-alloc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/remote_allocator.rs#L853-L858
[b-spawn-at]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/mod.rs#L1060-L1085
[b-despawn-nofree]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/mod.rs#L1646-L1655
[b-world]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/mod.rs#L79-L101
[b-world-checkticks]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/mod.rs#L3385-L3422
[b-despawn-hooks]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/entity_access/world_mut.rs#L1700-L1790
[b-despawn-swap]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/entity_access/world_mut.rs#L1807-L1868
[b-table]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/storage/table/mod.rs#L187-L207
[b-table-swap]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/storage/table/mod.rs#L228-L260
[b-table-alloc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/storage/table/mod.rs#L489-L496
[b-table-move]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/storage/table/mod.rs#L743-L818
[b-column]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/storage/table/column.rs#L9-L31
[b-arch-doc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/archetype.rs#L1-L15
[b-arch-edges]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/archetype.rs#L195-L215
[b-arch-alloc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/archetype.rs#L600-L617
[b-arch-swap]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/archetype.rs#L628-L640
[b-insert-move]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/bundle/insert.rs#L260-L300
[b-remove-move]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/bundle/remove.rs#L255-L310
[b-storage-type]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/component/mod.rs#L763-L770
[b-q-noorder]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/query.rs#L650-L654
[b-q-matched]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/query/state.rs#L646-L670
[b-q-sort-doc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/query/iter.rs#L544-L546
[b-q-sort-nocache]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/query/iter.rs#L605
[b-q-sort-impl]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/query/iter.rs#L1015-L1030
[b-changed-filter]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/query/filter.rs#L1140-L1148
[b-cmd-doc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/commands/mod.rs#L38-L57
[b-cmd-spawn]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/commands/mod.rs#L268-L270
[b-cq-apply]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/command_queue.rs#L178-L190
[b-sys-lastrun]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/function_system.rs#L709-L742
[b-exec-default]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/executor/mod.rs#L44-L63
[b-exec-topo]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/executor/mod.rs#L67-L71
[b-applydef]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/executor/mod.rs#L134-L157
[b-mt-apply]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/executor/multi_threaded.rs#L764-L784
[b-st-apply]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/executor/single_threaded.rs#L190-L215
[b-ambig]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/schedule.rs#L1838-L1845
[b-autoapply]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/schedule.rs#L1852-L1861
[b-shuffle]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/schedule.rs#L1869-L1888
[b-ambig-default]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/schedule/schedule.rs#L1900-L1903
[b-cd-const]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/change_detection/mod.rs#L13-L26
[b-tick]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/change_detection/tick.rs#L23-L25
[b-tick-newer]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/change_detection/tick.rs#L53-L66
[b-tick-check]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/change_detection/tick.rs#L77-L92
[b-cticks]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/change_detection/tick.rs#L196-L221
[b-setifneq]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/change_detection/traits.rs#L221-L233
[b-derefmut]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/change_detection/traits.rs#L493-L500
[b-resource-doc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/resource.rs#L18-L22
[b-resource]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/resource.rs#L87-L91
[b-messages]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/message/messages.rs#L17-L38
[b-event]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/event/mod.rs#L15-L19
[b-obs-order]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/observer/distributed_storage.rs#L150-L155
[b-reflect-comp]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/reflect/component.rs#L81
[b-app]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_app/src/app.rs#L58-L59
[b-plugin]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_app/src/plugin.rs#L1-L20

<!-- EnTT v3.16.0 -->
[e-cmake]: https://github.com/skypjack/entt/blob/v3.16.0/CMakeLists.txt#L96
[e-config-throw]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/config/config.h#L8-L15
[e-config-page]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/config/config.h#L46-L51
[e-config-assert]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/config/config.h#L54-L60
[e-traits]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/entity.hpp#L31-L51
[e-vmask0]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/entity.hpp#L104-L108
[e-next]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/entity.hpp#L116-L119
[e-policy]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/fwd.hpp#L17-L26
[e-comp-traits]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/component.hpp#L15-L54
[e-ss-doc]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L139-L153
[e-ss-page]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L177-L211
[e-ss-swaponly]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L253-L258
[e-ss-swappop]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L264-L279
[e-ss-inplace]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L282-L286
[e-ss-begin]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L650-L653
[e-ss-sort]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L998-L1037
[e-ss-sortas]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/sparse_set.hpp#L1052-L1054
[e-st-doc]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/storage.hpp#L212-L217
[e-st-page]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/storage.hpp#L237-L260
[e-st-pop]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/storage.hpp#L338-L354
[e-st-entity]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/storage.hpp#L1042
[e-st-generate]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/storage.hpp#L1135-L1158
[e-reg-create]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/registry.hpp#L503-L518
[e-reg-destroy]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/registry.hpp#L544-L551
[e-reg-groupconf]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/registry.hpp#L1119
[e-reg-nosort]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/registry.hpp#L1186
[e-reg-ctx]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/registry.hpp#L1217-L1222
[e-group-handler]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/group.hpp#L103-L130
[e-snap-get]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/snapshot.hpp#L94-L123
[e-snap-empty]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/snapshot.hpp#L193
[e-snap-cont]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/snapshot.hpp#L306-L317
[e-organizer]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/organizer.hpp#L109-L117
[e-md-build]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L96-L106
[e-md-observe]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L339-L365
[e-md-reactive]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L481-L499
[e-md-sort]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L664-L700
[e-md-ctx]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L1003-L1010
[e-md-snap]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L1089-L1103
[e-md-pstab]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L1478-L1513
[e-md-viewiter]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L1897-L1925
[e-md-groups]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L1987-L2148
[e-md-allowed]: https://github.com/skypjack/entt/blob/v3.16.0/docs/md/entity.md?plain=1#L2236-L2262

<!-- 敵對檢查後追加 -->
[b-ent-fresh]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L846-L848
[b-ent-canspawn]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L922-L928
[b-ent-freshmeta]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/entity/mod.rs#L1198-L1205
[b-q-itermany-doc]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/query.rs#L853-L856
[b-q-itermany]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/query.rs#L918
[b-cmd-doc-why]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/commands/mod.rs#L38-L41
[b-sys-exclusive]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/function_system.rs#L695-L711
[b-sys-exclusive-flush]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/system/function_system.rs#L735-L739
[b-spawn-at-fn]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/mod.rs#L1078
[b-despawn-nofree-flush]: https://github.com/bevyengine/bevy/blob/v0.20.1/crates/bevy_ecs/src/world/entity_access/world_mut.rs#L1700-L1703
[e-reg-hint]: https://github.com/skypjack/entt/blob/v3.16.0/src/entt/entity/registry.hpp#L507-L518

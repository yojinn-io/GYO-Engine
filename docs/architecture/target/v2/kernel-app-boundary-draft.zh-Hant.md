# Kernel 與編排層的邊界：設計稿（照 Bevy、flecs、Unity Entities、Unreal Mass 改寫）

> 性質：**設計稿，給使用者看的提案。沒有任何一項經使用者判過。** 不是事前宣告；事前宣告在使用者看過這份、定了前提之後另寫。

2026-11-16，任務卡 `v8-arch-gyo2-kernel-app-boundary`。程式 gyo2 cb6bccca。

**這份稿的起點**（使用者的話，逐字）：

- 「Kernel是機制，那組裝放在哪裡？之前我認為我自己把bevy_ecs 和 bevy_app的職責都放到Kernel裡了。」（2026-11-16，還原入口卡）
- 「不，所有人都沒有還有一種可能，他們踩過坑。其實我是沒想清楚，Kernel其實就是個ECS，本身就不應該迭代限制在裡面的。」（「迭代」＝「疊加」，使用者確認）
- 「先移除出來。然後得重新評估設計。」
- 「首先看看別人是如何做的，然後去查查他們為何這麼設計，最後重新評估我們的設計。」
- 「你先參照這些Engien的設計，改一份設計稿給我看看我。主要是我越看GYO的設計越是異類。而且限制極多。」

**證據**（主 checkout `build/evidence/gyo2-kernel-app-boundary/`）：`inventory.md`（Kernel 公開型別的歸類）、`bevy-recon.md`、`cross-comparison.md`、`single-writer-origin-and-pvp.md`、`restrictions-bevy.md`、`restrictions-flecs-unity-mass.md`、`restrictions-reevaluation.md`。比較對象不含 Godot（使用者：架構不同）。

---

## 1. 先用遊戲講

### 1.1 單機 2D 橫版：啟動到第一幀

**現在的 GYO**：

1. 產品的 `main` 列出 `Plugin`，建 `Registry`（Kernel），`Finish()`。Kernel 在這裡檢查：血量有沒有兩個寫者、每個 System 的 zone、zone C 的先後有沒有寫……
2. 「受傷」與「撿補血包」都想改主角的血量 → 啟動失敗（D-E2）。作者得做漏斗（兩邊發事件、一個 `ApplyHealth` 寫）或標 `Shared`。
3. 建 `Product`（Kernel）。作者自己寫迴圈呼叫 `Product::Run("Update")`；第一次呼叫時 Kernel 先把開局跑完（K0）。
4. 開局裡寫的血量初值不算寫者——因為 Kernel 保證開局只跑一次、最先、獨自跑。

**這份稿**：

1. 產品的 `main` 只寫「用這些登記單位，跑起來」。組裝那一層（編排層，Engine 的一個 Module）照登記檔列出 Module 與 Plugin，各自 build、finish。
2. 「受傷」與「撿補血包」都直接寫血量。Kernel 不攔；誰先誰後照它們宣告的順序，沒宣告就照登記順序（和現在 zone B 的平手一樣）。
3. 編排層的 runner 擁有迴圈：開局清單跑一次，之後每幀跑每幀清單（固定步由時間的 System 在每幀清單裡推）。
4. Kernel 不知道哪張是開局，也不需要知道：沒有寫者規則，就沒有豁免。

### 1.2 PvP 的 Match：同樣的血量

**現在的 GYO**：血量只准一個寫者、在 zone C、進 digest 不准 Shared——都是 Kernel 的啟動錯誤。

**這份稿**：PvP 開一個「權威檢查」（一個 Module，PvP 選；第 4 節）。它在啟動時讀 Kernel 交出的「每個 System 讀寫什麼、掛在哪、先後」，加上它自己的宣告（哪些資料是決定性區、哪些進 digest、哪幾張是建世界的階段），檢查「血量只有一個寫者」「決定性區不讀一般區」。檢查不過，PvP 的產品啟動失敗。單機的產品不開這個檢查，就沒有這些限制。

規則還在，只是**歸要它的人**，不是 Kernel 的天生性質。

### 1.3 PvP 的 Client：三個 World

模擬、每幀、渲染三個 World（使用者 2026-11-11）。**這份稿**：三個 World 由 編排層的 `Product`（見第 3.3 節的名字）持有；runner 決定誰在哪條執行緒、交換點什麼時候跑、渲染的 extract 什麼時候做。Kernel 只提供「一個 World」「在這個 World 上跑一張 schedule」「讀另一個 World 的唯讀參數」。

---

## 2. 四家的共同做法（摘要；細節與出處在證據檔）

| | Bevy | flecs | Unity Entities | Unreal Mass |
|---|---|---|---|---|
| ECS 核心有沒有開局 | 沒有（`bevy_ecs` 搜不到 startup） | core 只預留 id；跑一次在 pipeline addon | 沒有 World 級開局 | 沒有 |
| 每幀順序、runner | `bevy_app`（順序表、`set_runner`） | pipeline addon（可拿掉）；迴圈是作者的 | 引擎本體的 PlayerLoop | 引擎的 World tick |
| 登記單位 | `bevy_app` 的 `Plugin` | 模組 | 系統自動登記＋bootstrap | 引擎的模組 |
| 多個 World | `bevy_app` 的 SubApp | 作者建 | Entities 的 bootstrap、Netcode | UWorld |
| 讀寫宣告的用途 | 排程、平行、可選的歧義偵測 | 插同步點 | 串 job 依存 | 依存求解、平行 |
| 寫者數量、決定性區、digest 規則 | 沒有 | 沒有 | 沒有（Netcode：「不是決定性套件」） | 沒有 |
| 時鐘 | `bevy_time` | timer addon | 一般 System 推、策略可換 | 引擎 tick |
| 硬錯誤 | 排不出（成環、型別錯）；歧義預設不報 | 同 | 成環才丟例外 | — |

**拿掉過的**（「踩過坑」的證據）：Bevy 0.10 拿掉「整個 World 綁一條執行緒」改成逐資源（soundness bug）；Bevy 0.11 拿掉只為開局存在的 `StartupSet`；Bevy 兩次拿掉預設的系統分組（隱含複雜度）；Bevy 的歧義檢查五年沒能預設開（誤報淹沒）；Unity 兩次拿掉「沒有實害」的 throw；Mass 5.6 拿掉隱性的主執行緒限制後發現它在藏 bug。**沒找到**有人做過寫者數量、決定性區、digest 規則後拿掉——四家都沒做過。

---

## 3. 新的分層

```
產品（Game／Tool）       main：用哪些登記單位（產品登記檔）、跑起來
   │
編排層（Engine 的 Module）  組裝、登記單位的生命週期、runner、開局與每幀的順序、
   │                        多個 World、交換點、extract 的時機
   │
時間（Engine 的 Module）     時鐘、固定步的推動、時間的投影（T1／T3 已定的方向）
   │
權威檢查（Module，產品選用） 寫者數量、決定性區、digest、回滾的限制……（PvP 選）
   │
Kernel（ECS）              World、ECS 元件／Resource／事件、System 參數與存取推導、
                           schedule 與 Plan、Executor、延後變更與同步點、hook、
                           巢狀跑 schedule、存取驗證（除錯用）、對外交出登記資訊
```

依賴方向：上層依賴下層；Kernel 不認得上面任何一層。

編排層本身不帶任何第三方的能力。各種能力各自是獨立的 Module 或 Plugin（獨立的 lib），由它們的登記單位接進來；編排層只提供「接進來」的介面。歸屬照 2026-11-16 的新定義（`v8-arch-gyo2-module-plugin-owner-definition`；條文由任務卡 gyo2-module-plugin-definition 收斂）：**Module**（我們自己維護、介面是我們的）——Kernel、編排層、時間、執行緒、OS、base、math、JSON（yyjson 藏在我們的介面後面）、權威檢查、狀態機、audio、網路；**Plugin**（第三方官方維護、接進來用的）——SDL 家族（視窗、輸入、GPU 後端、SDL_image、SDL_ttf）、ImGui。系譜：Bevy 的 `bevy_app` 只依賴 `bevy_ecs` 等底層 crate 與幾個小工具（`crates/bevy_app/Cargo.toml`），wgpu 在 `bevy_render`（`Cargo.toml:90`，由 `RenderPlugin` 接進來）、serde_json 在 `bevy_gltf`、`bevy_remote` 等用到它的 crate；方向是那些 crate 依賴 `bevy_app`，不是反過來。（使用者 2026-11-16：「懂了，都是獨立的lib」）

### 3.1 Kernel 留下什麼（＝`bevy_ecs` 那一半，加 GYO 已定的 ECS 規則）

| 留下 | 現在的程式 | 備註 |
|---|---|---|
| World、ECS 元件、Resource、事件緩衝 | `World.hpp`、`detail/Storage.hpp` | 不變 |
| System 參數與「存取從簽名推出」 | `Params.hpp`、`SystemTraits.hpp`、`Access.hpp`（DI-KS 1） | 不變 |
| 登記的動作 | `Registrar` 的 `Component`／`Resource`／`Event`／`AddSystem`／`Hook`／`Schedule`／`Set`…… | 不再屬於 `Plugin`：登記單位在 編排層，build 時拿到 `Registrar` |
| schedule、Plan、執行器、同步點、hook 清空輪 | `Plan.hpp`、`Scheduler.cpp`、`Executor.cpp`（DI-KS 3、9、11） | 不變 |
| 巢狀跑 schedule | `ExclusiveWorld::RunSchedule`（K0，DI-KS 16～21） | 不變。Bevy 的 `run_main`、固定步都是靠這個組出來的 |
| 存取驗證（除錯用，可關） | W-5、A.6（DI-KS 2 的前半：經 World API 的未宣告存取、System 邊界的快照比對） | **已定留**（使用者 2026-11-16「照建議」）：它驗的是 ECS 自己的前提「宣告就是實際做的事」，不是遊戲規則。系譜：Unity 的 job safety system（核心、Editor 開、Player 只有 Checked 版）；Mass 5.6 的執行期偵測器；Bevy 由 Rust 編譯期保證。DI-KS 2 的後半（untyped `Commands` 只寫 zone B 的 Shared、`ExclusiveWorld` 不寫 zone C 與 Exclusive，所有組態回 Err）是寫者規則與區規則的執行期那一半，**跟著權威檢查移出**（見 3.2） |
| 對外交出登記資訊 | `FrozenRegistry`（`Systems()`、`Plans()`、`AccessSet`）、指紋 | 留，而且是權威檢查要讀的來源 |
| **只留「排不出」的硬錯誤** | R-E2（型別登記兩次）、R-E4（未登記的型別）、R-E5（TypeKey 碰撞）、D-E1（掛到不存在的 schedule 或 stage）、D-E3（缺 Resource；「開局結束時仍未初始化」那半句跟著開局搬走）、D-E4（before／after 的目標不存在）、D-E5（逆 stage 序）、D-E6（成環）、D-E19（同 System 同 schedule 掛兩次）、D-E20（hook 輪上限 0） | 四家共同的做法 |

### 3.2 移出 Kernel 的

| 移出 | 現在的程式／條文 | 去哪 | 系譜 |
|---|---|---|---|
| 登記單位 `Plugin`、`Requires`、`Finish`、`InstallRunner`；R-E1、R-E3、R-E6 | `Registry.hpp:41-75`、`Registry.cpp:646-724` | 編排層 | Bevy `bevy_app` 的 `Plugin` |
| 開局什麼時候跑（K0 的排程部分）、一張開局（D-E21）、開局中不准跑別的（H-13） | `Executor.cpp:151-170`、`:457-468`；DI-KS 22 | 編排層的開局清單（第 5 節有選項） | Bevy `run_main` 的開局清單；flecs pipeline 的 OnStart |
| 開局的寫者豁免 | `Registry.cpp:1448-1457`；DI-KS 6 | 跟著寫者規則走（第 4 節） | — |
| 每個時鐘一張 clock schedule、Kernel 推 tick（H-1、D-E12）、`Domain` 列舉（含 `Loading`） | `Declarations.hpp:56-62`、`Executor.cpp:192-196` | 時間的 Module（tick 是時間的 System 寫的 Resource） | `bevy_time`；Unity 的 World 時間由一般 System 推 |
| 事件按時鐘修剪 | Plan 的 `EventTrim` 步（`Scheduler.cpp:224`、`Executor.cpp:417-418`）、DI-KS 10 | **已定：Kernel 照游標修剪，不看時鐘**（使用者 2026-11-16）——丟掉所有讀者都讀過的、或超過上限的紀錄（丟了有記錄）。理由（使用者的）：「它讓『不漏讀』成為 Kernel 自己的保證，不必依賴編排層排得對。」比過的：Kernel 提供「更新事件」的 System、由編排層或時間排（Bevy：`message_update_system` 在 `bevy_ecs`、`bevy_app` 排進 `First`，`bevy_app/src/sub_app.rs:91-100`，`bevy_time` 另加訊號 System 讓它只在固定步跑過後更新，`bevy_time/src/lib.rs:103`、`bevy_ecs/src/message/update.rs:15-24`）；時鐘留 Kernel 照舊 | 你 11-14 定的游標方向（時間設計第 0 節「每個讀者一個游標；留到讀過或碰到上限，丟了有記錄」，T4）。代價：T4 要排在 Kernel 減法之前或一起；DI-KS 10「不跨時鐘」改寫，碰到 `v8-arch-gyo2-cross-clock-events-open` |
| World 綁一條執行緒（H-2、D-E13）、執行緒親和的啟動檢查（X-3、D-E10） | DI-KS 14、`ScheduleOptions::runner` | runner 負責呼叫的執行緒；需要綁執行緒的 Resource 自己記（第 5 節有選項） | Bevy 0.10 #6534 |
| extract 的來源與時機 | `SetExtractSource` | 編排層（Extract 參數本身留 Kernel） | Bevy `SubApp::extract` |
| 寫者數量、digest 禁 Shared、決定性區 Z1～Z8、zone C 歧義錯誤、回滾的限制、ingress | DI-KS 5～8、12 的檢查部分；D-E2、D-E7、D-E8、D-E15、D-E16；DI-KS 2 後半（untyped `Commands`、`ExclusiveWorld` 對 zone C／Exclusive 的寫入回 Err）、DI-KS 23 | 權威檢查（第 4 節） | 四家都沒有；GYO 自己的 |
| `KernelSettings` 的「System 不准讀」（D-E18） | — | 留在 Kernel（它是 Kernel 自己的 Resource，屬於「排不出」一類）——或刪。列為小項 | — |

### 3.3 名字（都已定）

`App` 這個詞：使用者 2026-11-14 對型別說「這個不叫 App 了，太模糊」，同詞異義表寫「App」在 gyo2 新文件不用。`Product`：現在指 Kernel 裡「一個 World 加 Executor」；同詞異義表也記了它和中文「產品」同名會混淆。

| 東西 | 候選 | 說明 |
|---|---|---|
| 編排層這個 Module | **已定：編排層，`engine/orchestration`**（使用者 2026-11-16：「算了用這個吧，編排也是挺應景的。」） | 照 N11～N17：目錄 `engine/orchestration`、命名空間 `GYO::Engine::Orchestration`、target `gyo_orchestration`／`GYO::Orchestration`。比過的候選：Product 層（`engine/product`，主型別同名）、組裝層（`engine/setup`，比實際範圍小）、Shell 層（`engine/shell`，舊 framework shell 的包袱；使用者先選後改）。新詞，要進同詞異義表，並寫明和 Kernel 的 schedule、Plan 的分別：Kernel 決定「一張 schedule 裡 System 的先後」，編排層決定「哪幾張 schedule、什麼時候、在哪個 World 跑」 |
| 一個產品的 World 群＋runner（Bevy 的 `App`） | `Product`（從 Kernel 搬上來） | 使用者「直接用 Product」；搬上來後「產品＝`Product` 物件」對得上，同詞異義表記的混淆反而消失 |
| Kernel 裡「一個 World 加跑它的執行狀態」 | **已定：保留一個型別、另取名 `WorldExecutor`**（`GYO::Kernel::WorldExecutor`；使用者 2026-11-16：型別「第二個，保留一個型別、另取名」，名字「照建議」）。比過的名字：`WorldExecutor`（建議）／`Executor`（和 `ExecutorKind` 撞）／`Session`（易讀成網路 session）／`WorldInstance`（太泛） | 現在的 `Product`（`Product.hpp:89-123`：World、凍結 Registry 的指標、每掛點的 `lastRun` 與 `Local`、錯誤處理器）改名 `WorldExecutor` 留在 Kernel；編排層的 `Product` 持有幾個它。比過的：併進 `World`（Bevy：schedule 與 System 狀態放在 World 的 `Schedules` Resource）——要改 B-12（`Local` 在 World 外）、DI-KS 12（回滾狀態全在 World）、同詞異義表「World＝全部執行期資料」；`Product` 留 Kernel、編排層另取名——和「直接用 Product」相反。理由：卡片建議的（「World＝資料」與回滾、`Local` 的規則不動；已合併的程式只改名），使用者選了，沒有另外說 |
| 登記單位 | `Module`／`Plugin`（名字已定），命名空間從 `GYO::Kernel::` 改到編排層 | 名字照 `v8-arch-gyo2-module-plugin-registers`；**定義**照 2026-11-16 的新定義（`v8-arch-gyo2-module-plugin-owner-definition`：Module＝我們自己維護、介面是我們的；Plugin＝第三方官方維護），條文由任務卡 gyo2-module-plugin-definition 收斂，這裡不另寫 |

---

## 4. 權威檢查放哪

**已定**（使用者 2026-11-16，在從本卡分出的側邊會話；HANDOFF `v8-arch-gyo2-module-plugin-owner-definition` 第 (4) 點）：權威檢查——寫者數量、決定性區、digest 禁 Shared、zone C 歧義錯誤、回滾的限制，也就是從 Kernel 移出的那些——放 **Module，產品選用**（PvP 選、單機不選）；在編排層的 finish 讀 `FrozenRegistry` 檢查、回報啟動錯誤。原話：「權威檢查我想放Module，這個是底層能力。」

同一則定了 Module／Plugin 的新定義（Module＝我們自己維護、介面是我們的；Plugin＝第三方官方維護、接進來用的）。定義的條文由任務卡 gyo2-module-plugin-definition 收斂；這份稿與之後的宣告引用它，不另寫一版定義。

**依賴那張卡、還沒確認的前提**：Module 登記檔裡，產品能不能只選部分 Module。「產品選用」成立要靠它；由定義那張卡列給使用者。

**被移出的規則各自的來歷**（`single-writer-origin-and-pvp.md`）：寫者數量——AGENTS §3 狀態的權威（2026-10-16，起因 PvP 的 Gateway）→ 架構師研究稿 DI-K2（11-10）→ 使用者核准預設獨占（11-11，案例都是 PvP）。決定性區、digest、回滾——spec G、H（11-11），為 PvP 的權威比對與回滾。

比過的選項（側邊會話列的）：拿掉／Module 產品選用／PvP 自己的 plugin／留 Kernel 預設關；之後討論過放 Plugin（Engine 維護）。

### 4.1 權威檢查需要的宣告放哪

**已定**（使用者 2026-11-16，在從本卡分出的側邊會話「重新過一次問題」，經該會話轉達；**取代**同日稍早在本卡定的「Kernel 留中性標籤」）：權威策略要讀的宣告（決定性區、digest、建世界的階段）由**權威 Module 自己的登記**承載，並由它代為登記進 Kernel——作者寫一行 `authority.Component<Health>({...})`，權威 Module 再呼叫 Kernel 的 `Registrar::Component`。**Kernel 完全不動、不存標籤。** 原話：「4.1 權威 Module 自己登記。這個轉給任務卡，讓他同步主會話」。

比過的選項（側邊會話列）：Kernel 留型別化的標籤（前例只有 Bevy TypeData，用於序列化，不是 PvP）／權威 Module 自己登記、代為登記進 Kernel（建議；前例 lightyear `register_component` 同時登記進 ECS——`lightyear@736e1695:crates/replication/replication/src/registry/mod.rs:196-200`；replicon `app.replicate::<C>()`——`bevy_replicon@d80c849:src/shared/replication/rules.rs:23`；代價：權威 Module 包住 Kernel 的登記 API）／宣告寫在型別旁邊（C++ trait 特化；前例 Unity Netcode 的屬性；代價：同一個元件不能有不同宣告、schedule 不是型別寫不了）。理由：側邊會話的（有 PvP 前例；Kernel 完全不動，最合「Kernel 就是 ECS」；作者只寫一行、不會兩處登記）；使用者選了，沒有另外說。

### 4.2 權威是遊戲設計的一部分：策略式（方向已定，細節未定）

使用者 2026-11-16（同一個側邊會話）：「等等，這麼說其實是我吧權威判定落的太深了，想想也是，這個東西本身就是遊戲邏輯的一部分，是否權威，走什麼協議，其實都是看遊戲如何設計的，例如我做格鬥遊戲，就直接p2p了。」「能不能把權威檢查，做成策略式的，放在Module裡。這樣作者就可以有一不用考慮那麼多了。？」→「不不不，我的意思是由作者去選。」→ 讀法一：**作者選一個策略，由策略決定開哪些檢查**。

側邊會話列的策略例子（不是使用者定的）：

| 策略 | 開哪些檢查 | 例子 |
|---|---|---|
| 不用 | 不開 | 單機 2D 橫版 |
| 伺服器權威 | 決定性區、digest、回滾的限制、每欄一個寫者、權威在伺服器 | PvP 的 Match／Client |
| P2P 回滾 | 決定性區、digest、回滾的限制 | 格鬥遊戲（使用者舉的例子） |

**已定**（使用者 2026-11-16，同一個側邊會話；建議代號 `v8-arch-gyo2-authority-strategy-mechanism`）：
- 原話：「作者也可以自己寫策略，也可以直接用Engine帶的權威策略。這個核心是要建立程機制而不是寫死。」（「程」讀作「成」）「伺服器權威（PvP 射擊）就行，其他的可以先佔個位子，但是不實現，調用的話報錯。」
- 權威檢查的核心是**機制**：策略介面＋策略可以開的檢查，不寫死模式。作者可以自己寫策略，也可以用 Engine 附的；兩者走同一個介面，Engine 附的沒有特權。
- Engine 附的現成策略，第一版只**實作「伺服器權威（PvP 射擊）」**。其他（例如 P2P 回滾）先**佔位、不實作**：名字與介面留著，作者選了就報錯（說明還沒實作）。
- 「不用（單機）」＝不選權威 Module、沒有檢查；側邊會話讀成這不是一個策略、不佔位（使用者沒有對這句表態）。
- 比過的：只能挑 Engine 的／可以自己寫 → 可以自己寫；伺服器權威與 P2P 回滾都實作（P2P 可接 GekkoNet）／只實作伺服器權威、其他佔位 → 後者。理由：使用者的「這個核心是要建立程機制而不是寫死」；只實作伺服器權威的理由使用者沒有另外說（側邊會話提過 AGENTS §2「只做點名的」）。

**已定**（使用者 2026-11-16，本卡：「啟動時報」）：佔位策略被選了，在**啟動時**報——編排層 finish，當啟動錯誤，訊息寫明那個策略還沒實作。比過的：啟動時報（側邊會話與本卡建議；和權威檢查在 finish 做檢查的時機一致）／用到時才報（遊戲能啟動，第一次需要策略做事時才出錯）。理由：建議的（「選了一個不存在的功能」啟動時就能確定）；使用者沒有另外說。側邊會話曾提把「決定性／digest／回滾」（底層能力）與「權威、每欄一個寫者」（遊戲設計）拆開，使用者接著提策略式，沒有對拆法單獨表態。

會改到：`v8-arch-gyo2-module-plugin-owner-definition` 第 (4) 點「權威檢查放 Module、產品選用」→「策略式 Module、作者選策略」。

**可整合的開源方案**（側邊會話的偵察摘要；證據 `build/evidence/gyo2-kernel-app-boundary/authority-open-source.md`）：P2P 回滾——GekkoNet（C++20、C API、BSD-2，照新定義是 Plugin）可直接整合，GGRS、GGPO 當設計參考；伺服器權威——沒有開源、與引擎無關的 C++ 同步／預測庫，只能接傳輸（yojimbo、GameNetworkingSockets、ENet），同步層自己做；Nakama 只能當配對與大廳。

---

## 5. 其他三件（都已定）

5.1～5.3 由使用者 2026-11-16 在從本卡分出的側邊會話「btw: 重新過一次問題。」定，經該會話照四樣格式轉達；原話逐字見各小節。

### 5.1 遊戲初始化（`v8-arch-gyo2-game-init-list`）

**原話**（逐字、依序）：「首先這個開局是整個遊戲初始化，還是進入遊戲後生成世界？」→「對，另外可以有多個World嗎？」→「對。」→「第一個World，內容有作者自己訂，Engine只提供機制」→（是否也指清單內可插入）「對的。」

**定下的**：K0 的「開局」切成兩件——

| | 什麼時候 | 屬誰 | 例子 |
|---|---|---|---|
| **遊戲初始化** | 行程啟動時一次，在所有 World 開始每幀之前跑完 | 編排層（`Product`）；跑在**第一個 World（主 World）** | 讀按鍵設定、UI 設定；狀態機進入主選單 |
| **生成世界** | 每個 World 各自、可多次、可分批 | Loading 那張卡（「建世界的階段」） | 進關卡、讀檔、載入大地圖 |

遊戲初始化的內容由作者訂，**Engine 只提供機制**：一份清單，清單內的先後作者與 Module 可以插入（系譜 Bevy `startup_labels`＋`insert_startup_before`／`_after`，`bevy_app/src/main_schedule.rs:240-305`）。

比過的：清單內可插入（建議）／只有一張、位置固定（K0 原樣搬到編排層）；跑在哪：第一個 World／編排層另建一個／每個 World 各跑一份。理由（使用者的）：「Engine只提供機制」。側邊會話補的（使用者看過）：切開後遊戲初始化不碰寫者豁免，K0「位置固定」的主要理由（保護豁免）不在這裡。

會改到：K0／DI-KS 22 的「一個 World 一張 Startup、Kernel 排第一」→ 遊戲初始化清單在編排層；生成世界的形狀由 Loading 卡定。狀態機宣告（11-16 核准，初始進入在開局之後）可維持，也可改成自己插進清單——由狀態機卡另定。

### 5.2 World 與執行緒（`v8-arch-gyo2-thread-binding-by-runner`）

**原話**：「第二個，別人掉過的坑，我沒必要在跳一次。」

**定下的**：哪條執行緒跑哪個 World 由 runner（編排層）決定；只能在某條執行緒用的 Resource 自己記來源執行緒，碰錯就斷言；Kernel 啟動時的執行緒檢查（H-2／D-E13、X-3／D-E10）拿掉。系譜：Bevy 0.10 #6534（整個 World 綁執行緒改成逐資源，起因 soundness bug #3310；現在 `bevy_ecs/src/storage/non_send.rs:74-84`）。

比過的：照舊 Kernel 啟動時擋／runner 負責＋逐資源斷言（建議）。理由（使用者的）：「別人掉過的坑，我沒必要在跳一次。」

會改到：DI-KS 14、runtime-composition DI 4（11-11）的「啟動時擋」→ 執行時斷言；`ScheduleOptions::runner`、`Affinity` 的啟動檢查；守衛 J-15 的相關案例、J-28。

### 5.3 歧義（`v8-arch-gyo2-no-ambiguity-detection`）

**原話**（逐字、依序）：「第一個，這個有個很麻煩的東西，偵測歧義其實是很麻煩的。我個人覺得沒必要。（要做就一定要了解業務，或者需要在設計System時就加互斥，否則普通程序無法判斷）」→「我每太明白你的這個讀法？究竟是做好還是不做好？」→（側邊會話：建議兩邊都不做；決定性靠伺服器權威策略在連線時比對 C 指紋）「可以。」

**定下的**：Kernel 照登記順序決定先後、不報錯（平手清單 `TieBreaks` 照舊交出）；**Kernel 與權威策略都不做歧義偵測**；伺服器權威策略在連線時比對兩邊的 C 指紋（`FrozenRegistry::CFingerprint()`），不同就拒絕連線（兩邊程式版本不同）。

比過的：Kernel 平手＋策略報錯／Kernel 留預設關的偵測開關（Bevy 的 LogLevel、現在的 `RequireExplicitOrder`）／兩邊都不偵測＋連線比指紋（建議）。理由（使用者的）：偵測歧義要懂業務或在設計時加互斥，普通程式判斷不了。側邊會話補的（使用者看過）：Bevy 的歧義檢查因誤報五年沒能預設開；登記順序相同時平手本身是決定性的，只有兩邊版本不同才會出錯，比指紋就能抓到。

會改到：DI-KS 5（(c) 歧義是啟動錯誤）、D-E8、`RequireExplicitOrder`、守衛 J-20～J-24、J-44。C 指紋的比對寫進伺服器權威策略的設計。

---

## 6. 會改到的已定條文與程式（逐條；使用者判之前不動）

| 已定的事（日期） | 原文要點 | 這份稿怎麼改 |
|---|---|---|
| `v8-arch-kernel-exclusive-default`（2026-11-11） | 預設獨占、標 Shared 允許多寫者、進 digest 禁 Shared | 移出 Kernel，歸權威檢查（第 4 節） |
| DI-KS 5、6、7、8、12（2026-11-13 核准，11-16 修訂） | 歧義、獨占、Parallel 守衛、區規則、回滾 | 檢查的部分移出；條文改寫或搬到權威檢查的頁 |
| DI-KS 10 | 事件不跨 clock domain、在 clock schedule 修剪 | 「更新事件」由上層排；時鐘不在 Kernel |
| DI-KS 14、runtime-composition DI 4 | 一個 World 一條執行緒（啟動時擋） | 第 5.2 節的選項 |
| DI-KS 15（2026-11-16 改寫） | Kernel 中性；runner 保留多時鐘、啟停、交換點；例外：Startup 由 Kernel 決定 | 例外拿掉；runner 與交換點寫在 編排層 |
| DI-KS 22、K0（`v8-arch-gyo2-startup-run-by-kernel`、`-startup-before-any-schedule`，11-15；#239 已合併） | Kernel 在第一次跑別的 schedule 前跑開局一次 | 搬到 編排層（第 5.1 節的選項）；Kernel 的 `startupRan_`、`StartupNotNestable`、`StartupRunning`、D-E21 與守衛 J-58 拿掉或搬家 |
| `v8-arch-gyo2-module-plugin-registers`（11-14） | `GYO::Kernel::Module`／`Plugin` 兩種型別、兩份登記檔在建置層 | 型別搬到編排層；兩份登記檔照 11-16 的新定義重分（定義卡的範圍） |
| `v8-arch-gyo2-module-plugin-owner-definition`（11-16） | Module＝自己維護、介面是自己的；Plugin＝第三方官方維護；權威檢查放 Module、產品選用 | 這份稿照它寫；產品能否只選部分 Module 由定義卡列給使用者 |
| runner 由登記單位在 finish 時安裝、一個 Runtime 一個（11-11） | — | 照舊，只是在 編排層；「一個 Runtime」改成「一個 `Product`」 |
| 時間設計 T2「Kernel：可呼叫的 runner、交換函式的登記」 | — | 改成 編排層 |
| 時間設計 T5「`Product` 多個 World」、決定 2（11-14） | — | 照舊，`Product` 在 編排層 |
| runtime-composition DI 2（11-13） | 一個 Runtime 一個 runner | 同上 |
| runtime-composition DI 3、3a（11-11、11-13） | 世界內容只經 Loading、Loading 的 stage 序 | 交給建世界的階段那張卡；「寫者不計」的需要隨權威檢查走 |
| `v8-arch-gyo2-product-type-name`（11-14） | Kernel 的型別叫 `Product` | `Product` 搬到 編排層（第 3.3 節） |
| (c) 區的獨占套用（`v8-arch-gyo2-zone-c-exclusive-apply`） | Kernel 允許 (c) 區獨占 System、登記它寫的 (c) Resource | Kernel 沒有區之後，這一批的形狀要重看 |
| 狀態機 (b) 區宣告（11-16 核准） | Kernel 不改；初始狀態在開局之後 | 「在開局之後」改由 編排層保證；第 5.1 節選「可插入」時，狀態機的 Module 自己插 |

**守衛測試**（AGENTS §3、§14.4：Design Invariants 的守衛改判定要使用者逐條核准）：J-14～J-17、J-20～J-24、J-27、J-37～J-42、J-58，以及 DI-KS 2 裡和區、獨占相關的 J-45、J-57。搬到權威檢查的，判定照搬；拿掉的，每一條列「守的性質、放寬的面」給使用者。

---

## 7. 和「建世界的階段」那張卡的分工

- **這一批定的**：層，以及「開局」切成遊戲初始化與生成世界（5.1）。Kernel 不認得開局、Loading、讀檔。遊戲初始化的清單在編排層、跑在主 World、行程一次。「建世界的寫入不算寫者」不再是 Kernel 的需要，而是權威策略的宣告（4.1、4.2：權威 Module 自己登記）。
- **留給那張卡的**：生成世界在編排層長什麼樣——每個 World 跑一次還是分批、誰推動、什麼時候算好、讀取畫面怎麼畫、讀檔與進關卡是不是新 World。權威策略要知道哪些 schedule 是生成世界的階段，由權威 Module 的登記宣告。
- 那張卡留給這一題的開放問題「Engine 認得建世界的階段」與「ECS 不該有階段」：這份稿的讀法是兩層——Engine（編排層）認得，Kernel（ECS）不認得。使用者在 5.1 說「Engine只提供機制」並確認了遊戲初始化／生成世界的切法；兩層的讀法本身沒有單獨問過，宣告時列出來請使用者確認。

---

## 8. 批次（粗排；宣告核准後細排）

| 批 | 內容 | 依賴 |
|---|---|---|
| 編排層骨架 | 新 Module；`Module`／`Plugin` 型別與生命週期搬出 Kernel；`Product` 搬上來、Kernel 只留 `World::RunSchedule`；開局清單＋每幀清單；runner 介面與「只跑一次」的 runner（測試用） | 這份稿核准 |
| Kernel 減法 | 移出寫者、區、歧義錯誤、回滾的檢查與 K0 的開局排程；`Domain` 拿掉；守衛逐條處理（§14.4） | 編排層骨架；權威檢查的位置定了 |
| 權威檢查 | 照第 4 節選的位置，把移出的規則接回（PvP 用） | Kernel 減法；PvP 移植前完成即可 |
| 時間（T1、T3） | 時鐘、固定步、事件的更新 System | 編排層骨架 |
| 多個 World（T5）、交換點（原 T2） | 在 編排層 | 編排層骨架 |
| 建世界的階段 | Loading 那張卡 | 編排層骨架 |

時間估計（實作，未含宣告與審查）：編排層骨架 6～10 小時；Kernel 減法 6～10 小時（守衛的處理占大半）；權威檢查 6～12 小時（看位置）。沒有量測，是讀程式量估的。

---

## 9. 範圍的限界

- 這是設計稿，沒有寫程式。四家的做法來自子 agent 讀原始碼與文件，主會話抽查了一部分（每份證據檔的開頭寫了抽查了哪幾處）；Unity 讀的是非官方鏡像，Unreal 沒讀到原始碼。
- 「踩過坑」：只找到第 2 節列的那些；寫者數量、決定性區、digest 沒有前例，所以沒有「做了又拿掉」的證據，也沒有「做了而且成功」的證據。
- 沒有送換模型檢查（照任務卡，宣告寫好後才送）。
- 啟動錯誤碼沒有逐條歸類完：D-E9、D-E11（含 zone C 的 Parallel 守衛，看起來屬權威檢查）、D-E14（extract 來源未凍結，看起來隨 extract 到 編排層）、D-E17（Parallel 未實作，看起來留 Kernel）、X-E1～X-E3（執行緒角色，隨第 5.2 節）、F-E1（事件不屬任何時鐘，隨時鐘移出）——括號裡是讀了一行說明後的猜測，宣告時逐條核對。
- 第 6 節的「已定的事」只核對了 HANDOFF 與 target 文件的原文，沒有重讀 11 月 11 日側邊會話的原始紀錄。

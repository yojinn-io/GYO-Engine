# 同詞異義表（`v8-arch-rebuild` 步驟 1）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：v2，交使用者確認（第 7 節）。v2 修訂後沒有再檢查。**

出處：[判斷舊設計的機制](rebuild-judgment-rules.zh-Hant.md) 第 2 題（名字）。過程：
- 底稿是盤點 (b) 第 4 節的 20 個詞。
- v1 由主會話（Fable）寫。
- 別模型檢查（Opus／high，一次）：blocker 8、should 14、nit 8，全部採用。

證據在主 checkout `build/evidence/rebuild-concept/`：`vocabulary-v1.md`、`vocabulary-checker-opus.md`。

程式與文件的出處都指 `master@08566260`；帶過來的程式指分支 `claude/gyo2-r0b-delete`。Bevy 指本機 `~/Code/GitHome/bevy` 的 6a911653f，路徑從 `crates/` 起算。

## 1. 這張表管什麼，不管什麼

v1 被檢查指出的主要錯誤：在名字這一層偷判了內容與歸屬。例如寫了「決定性不算在 Kernel 裡」、替 main 與 runner 分了職責、用還沒判過的舊設計來定義 App。v2 把這些都拿掉。

- **只管名字**：一個詞有幾個意思、各自的出處、以後哪個意思用原詞、其他意思怎麼說。
- **不判內容**：一個概念存不存在、是什麼、對不對，由去留登記表判。
- **不判歸屬**：一件事由誰持有、住在哪一層，由去留登記表判；層叫什麼由 R1 定。
- **還沒判的舊概念**：寫成「X（舊）」，意思是「舊設計裡叫 X 的那個東西」，不替它下新的定義。
- **適用範圍**：gyo2 上新寫的文件與之後的程式。AGENTS、舊文件、master 的程式不回頭改；引用它們時照原文的意思讀。

## 2. 準則與優先順序

衝突時，前面的贏後面的。

1. **一詞一義**：在新寫的文件與程式裡，一個詞不加限定時只指一件事。
2. **先占**：帶過來的程式的公開介面與 AGENTS 已經在用的詞義，優先保有原詞；新概念加限定詞或換詞。
3. **Bevy 的詞只當參照**：表裡寫「Bevy 用這個詞指什麼」。新概念要不要用同一個詞，看登記表判它們是不是同一個概念；這張表不宣稱「同概念」。
4. **設計沒定的不硬定名**：寫「X（舊）」。

v1 的準則 4「角色後綴不當型別名」拿掉了：
- 它出自舊的命名條文第 5 條（`docs/architecture/target/naming.zh-Hant.md:18`）。那一條的來源寫的是側邊會話轉述，沒有原話；它的理由也是內容主張，要過登記表。
- 所以 Runtime、Host、Shell 的處置改用準則 1 與準則 2 來判。

## 3. 處置的種類

| 處置 | 意思 |
|---|---|
| 沿用 | 只有一個意思，照用 |
| 加限定詞 | 幾個意思都在用；寫出每個意思的說法，並標明不加限定時指哪一個 |
| 寫「X（舊）」 | 舊設計的概念還沒判；引用時加「（舊）」 |
| 不用它命名新概念 | 這個詞意思太多或已被先占；一般語意與既有的型別名照用 |

## 4. 表

### 4.1 新舊都在用的核心詞

| 詞 | 現有的意思（出處） | 處置 | 新文件的寫法 |
|---|---|---|---|
| World | (i) PvP 的遊戲世界狀態：`WorldSnapshot`（`PvpMatch.hpp:22`）；(ii) 舊 Kernel 設計：資料容器；Bevy 同詞（`bevy_ecs/src/world/mod.rs:96`）；(iii) 舊文件「世界內容」指關卡資料 | 加限定詞 | 不加限定時指 (ii)。(i)(iii) 寫「關卡內容」「地圖內容」 |
| Entity | (i) 產品的契約 id：`EntityId`、`EntityKey`；(ii) 舊 Kernel 設計：World 的鍵；Bevy 同詞（`bevy_ecs/src/entity/mod.rs:447`） | 加限定詞 | 不加限定時指 World 裡的一個實體。產品給的 id 寫「契約 id」。兩者是不是同一個值，登記表判 |
| Component | (i) AGENTS 的 Component＝部件：State Authority、Design Invariants、外部 Component 的條文都用它（AGENTS.md:249、:273-281、:386-393）；(ii) os 的公開介面：路徑的一段，`SamePathComponent`（`engine/os/include/engine/os/Files.hpp:153`）；(iii) 建置的元件（`GyoComponents.cmake:5`）；(iv) 舊 Kernel 設計：ECS 的元件，中文也寫「欄」；Bevy 同詞（`bevy_ecs/src/component/mod.rs:530`） | 加限定詞 | (iv) 寫「ECS 元件」，程式裡的型別名可以是 `Component`。(i) 寫「部件」，引用 AGENTS 時照它的原文。(ii) 寫「路徑的一段」。(iii) 寫「建置元件」。「欄」除了引用舊文以外不用：它預設了欄式儲存，那是內容 |
| Resource | (i) AGENTS 的「資源」：性能與資源的預算、OS 資源；(ii) 舊程式的 `ModelResource`、`TextResource`、`ResourceHandle`（都不帶）；(iii) 舊 Kernel 設計：ECS 的 Resource；Bevy 同詞（`bevy_ecs/src/resource.rs:87`） | 加限定詞 | (iii) 一律寫英文「Resource」。中文「資源」留給 (i)。GPU 上的物件叫什麼、是不是 Resource，登記表判 |
| System | (i) AGENTS 的 Subsystem＝子系統；(ii) 舊 Kernel 設計：一個 System 函式 | 沿用 | 「System」指 (ii)。(i) 寫全稱「Subsystem」 |
| Schedule | 舊 Kernel 設計：一組排好順序的 System；Bevy 同詞（`bevy_ecs/src/schedule/schedule.rs:413`）。audio 的 `Scheduled` 是動詞的一般語意 | 沿用 | 同左 |
| Plugin | (i) 使用者 2026-11-11：「層與目錄叫 plugins，單個才叫 plugin」，單個指 `plugins/` 下的一個套件；(ii) 舊 Kernel 設計：登記的類別 `Plugin`（`Registry.hpp:60`）；Bevy 的 `Plugin` 是這個意思（`bevy_app/src/plugin.rs:57`）；(iii) AGENTS.md:659「Plugin System」指外掛機制 | 加限定詞 | (i) 寫「plugin 套件」；(ii) 寫「`Plugin` 型別」；(iii) 照 AGENTS 的原文。層名「plugins」是你定的，目前照用；之後維持不維持，由 R1 與登記表判 |
| App | (i) AGENTS 的 Application＝產品（AGENTS.md:377）；(ii) 建置的 KIND app 與 `apps/` 目錄；(iii) 舊產品的類別 `EditorApp`、`PvpApplication`；(iv) 舊 Kernel 設計：把原本叫 Runtime 的型別定名為 `App`（使用者 2026-11-13「名字用 App」，是在宣告的選項裡選的） | 加限定詞 | (iv) 的名字保留，寫「App」。它裡面有什麼、含幾個 World、runner 歸誰，登記表判，這裡不定義。(i) 寫「產品」。(ii) 寫「遊戲類產品」或直接寫 KIND app |
| Extract | 舊 Kernel 設計：把資料從主 World 單向取到渲染 World；Bevy 同詞 | 沿用 | 同左 |
| Asset | (i) `assets/<產品>` 內容根目錄；(ii) 舊程式的 `AssetManager`、`AssetHandle`；(iii) 舊 Kernel 設計的 `Assets<T>`；Bevy 的 `Assets`（`bevy_asset/src/assets.rs:288`） | 沿用 | 一份載入後的內容。`assets/` 是內容根目錄的名字 |
| Handle | (i) 舊程式的 `AssetHandle`：id 加世代的記號，不讓資產保持存活；(ii) audio 的 `SoundHandle`：一次播放的 id（`Mixer.hpp:50`，帶過來）；(iii) OS 的 handle；(iv) 舊 Kernel 設計的「句柄」：System 參數的基底，也拿來說實體的鍵；(v) Bevy 的 `Handle<A>`（`bevy_asset/src/handle.rs:115`）：強參照會讓資產保持存活，和 (i) 不同 | 加限定詞 | (iv) 不用「句柄」：寫「System 參數」「實體的鍵」。(i)(ii)(iii) 各自寫全名。資產的參照要不要保活，登記表判 |

### 4.2 帶過來的程式已經在用、和舊 Kernel 設計相撞的詞

| 詞 | 現有的意思（出處） | 處置 | 新文件的寫法 |
|---|---|---|---|
| Frame | (i) 畫面的一幀；(ii) audio 的取樣幀：`kSampleRate = 48000; // frames per second`（`AudioFormat.hpp:10`，帶過來）；(iii) 舊 Kernel 設計的 `Domain::Frame` 與 Frame World | 加限定詞 | 「畫面幀」「音訊幀」。(iii) 寫「Frame domain（舊）」 |
| Tick | (i) 固定步的步數：`TickContext`（`FixedTickRuntime.hpp:17`）、PvP 的 tick；(ii) 舊 Kernel 設計：domain tick，也拿來指 Loading 的批次計數；(iii) Bevy 的 `Tick`：變更偵測的計數（`bevy_ecs/src/change_detection/tick.rs:23`） | 加限定詞 | 不加限定時指 (i)。變更偵測的計數寫「stamp」。其他的計數寫「計數」，不叫 tick |
| Clock、Time | (i) time 的 `MonotonicClock`、`TimePoint`（`MonotonicClock.hpp:18-19`，帶過來）：真實時間的來源；命名空間 `GYO::Time`；(ii) 舊 Kernel 設計的 clock domain、`HostClockSample`；(iii) Bevy 的 `Time<T>` Resource | 加限定詞 | 「時鐘」不加限定時指 (i)。(ii) 寫「clock domain（舊）」。World 裡的時間 Resource 叫什麼，R1 定，要避開 `GYO::Time` |
| Domain | (i) os 的 `NativeErrorDomain`（`OsError.hpp:46`，帶過來）：錯誤來源的分類；(ii) PvP 文件的 match_domain；(iii) 舊 Kernel 設計：clock domain，封閉列舉（`Declarations.hpp:54`） | 不用它命名新概念 | (i) 是既有的型別名，照用。(iii) 寫「clock domain（舊）」；它是什麼、Loading 算不算，登記表判 |
| Mount | (i) io 的 `MountTable`、`MountPoint`（`engine/io/include/engine/io/fs/MountTable.hpp:88`，帶過來）：檔案系統的掛載；(ii) 舊 scheduler 的 `Mount`：System 掛在哪個 schedule | 加限定詞 | 不加限定時指 (i)。(ii) 寫「掛點（舊）」 |
| Command | (i) audio 內部的 `Detail::Command`（`Mixer.hpp:17`，帶過來）；(ii) 舊 Kernel 設計的 `Commands`：延後的結構變更；Bevy 的 `Commands` | 加限定詞 | (ii) 寫「`Commands`」或「延後的變更」。(i) 是 audio 的內部型別 |
| Sample | (i) audio 的取樣；(ii) 舊命名條文：boundary 輸入帶 `Sample` 後綴 | 加限定詞 | 不加限定時指 (i)。(ii) 隨 boundary（舊）由登記表判 |
| Role | (i) threads 的執行緒角色：`RoleThread`（`RoleThread.hpp:67`，帶過來）；舊 Kernel 的 `Affinity::role` 同義；(ii) 舊 Kernel 設計：欄的角色 State／Input（`Declarations.hpp:41`）；(iii) 建置登記的 ROLE 欄 | 加限定詞 | 不加限定時指 (i)。(ii) 寫「欄角色（舊）」。(iii) 是建置登記的欄名 |

### 4.3 其他一詞多義的詞

| 詞 | 現有的意思（出處） | 處置 | 新文件的寫法 |
|---|---|---|---|
| Event | (i) 資料與輸入：`AnimationEvent`、`InputEvent`、`FileChangeEvent`；(ii) 舊 Kernel 設計：World 裡的事件緩衝；(iii) 這一版 Bevy：緩衝的叫 `Message`（`bevy_ecs/src/message/mod.rs:100`），`Event` 留給觸發式的（`bevy_ecs/src/event/mod.rs:89`） | 加限定詞 | (ii) 寫「World 的事件緩衝」。(i) 寫全名。若之後照 Bevy 改叫 Message，會撞到 base 的 `LogMessage` 與 wire 的訊息；名字 R1 定 |
| Snapshot | (i) PvP wire 的狀態：`WorldSnapshot`、`SnapshotFrame`；(ii) 舊 Kernel 設計：回滾用、驗證用的快照 | 加限定詞 | 「wire 快照」「回滾快照」「驗證快照」 |
| Registry、登記表 | (i) 舊 Kernel 設計：型別與 System 的登記（`Registry.hpp`）；(ii) 建置的產品登記：`GyoAppRegistry.cmake`、`projects.csv`；(iii) `LoaderRegistry`（不帶）；(iv) 判斷機制第 5 節的登記表 | 加限定詞 | (i)「型別與 System 的 Registry」；(ii)「產品登記檔」；(iv)「去留登記表」 |
| Layer | (i) AGENTS §6 的架構層；(ii) AGENTS §2「層の区別」：基盤與應用；(iii) 渲染的層：`CompositeLayer`（`RenderTypes.hpp:98`） | 加限定詞 | 「架構層」「基盤／應用之分」「渲染層」 |
| Module | (i) engine 的模組；(ii) Go module；(iii) C++20 module | 加限定詞 | 不加限定時指 (i) |
| Stage | (i) 打包的 stage 目錄；(ii) `ShaderStage`（`ShaderTypes.hpp:18`）；(iii) 舊 Kernel 設計：schedule 裡固定順序的一段；(iv) 舊設計的動詞：`StageMembershipRequests`；Bevy 沒有 stage，用 `SystemSet` | 加限定詞 | (iii) 寫「stage（舊）」。(iv) 不沿用這個動詞。(i)(ii) 是各自領域的詞 |
| Scene | (i) `SceneCapture`（`Renderer.hpp:12`）、`PreviewScene`；(ii) 舊 Kernel 設計：Loading 的「場景」階段、渲染的輸入；(iii) Bevy 的 `Scene`（`bevy_scene/src/scene.rs:49`）：可以生成的實體範本，和 (i)(ii) 都不同 | 不用它命名新概念 | 關卡的資料寫「關卡內容」，渲染的輸入寫「渲染輸入」。之後要不要有「實體範本」這個概念，登記表判 |
| Input | (i) 玩家的輸入、輸入模組；(ii) 舊 Kernel 設計：欄角色 `Input`、boundary 輸入 | 加限定詞 | 不加限定時指 (i)。(ii) 寫「Input 欄角色（舊）」 |
| Boundary | (i) AGENTS 的 Architecture／Product／Module Boundary；(ii) 舊 Kernel 設計的 boundary Resource | 加限定詞 | 不加限定時指 (i)。(ii) 寫「boundary Resource（舊）」 |
| Zone | (i) os 的時區；(ii) 舊 Kernel 設計：決定性的區 | 加限定詞 | (ii) 寫「決定性區」。(i) 寫「時區」 |
| Target | (i) CMake 的 target；(ii) render target；(iii) `docs/architecture/target/`：改造期間的目標條文 | 加限定詞 | 不加限定時指 (i)。(ii) 寫全稱。(iii) 寫「舊的目標條文」；gyo2 的設計文件怎麼命名，交圖書管理員 |

### 4.4 舊的角色詞

| 詞 | 現有的意思（出處） | 處置 | 新文件的寫法 |
|---|---|---|---|
| Runtime | (i) 錯誤契約的 Runtime Error（帶過來）；(ii) AGENTS §8 的 Runtime：執行中的遊戲，相對於 Editor（AGENTS.md:522）；(iii) 舊的 `engine/runtime` 模組與 `UiRuntime`（不帶）；(iv) 舊 Kernel 設計：後來定名 App 的那個型別 | 不用它命名新概念 | (i)(ii) 照用。(iv) 寫「App」 |
| Host | (i) AGENTS.md:147 的 host：搭載基盤的一個應用；(ii) CI 的 host：建置機；(iii) 舊程式的 `MatchRuntimeHost`、`IpcHost`（不帶）；(iv) 舊 Kernel 設計：提供 runner 的一方加 main | 不用它命名新概念 | (i)(ii) 照原文。(iv)：Host 改說「組裝（setup）」與「runner」（使用者 2026-11-14 定名「組裝（setup）」；先前的提案「main」看起來像主執行緒，舊設計裡執行緒的角色也有一個叫 Main）。組裝指產品啟動時建 App、列出要用的東西的那一段。runner 指跑迴圈的函式（Bevy 的 `set_runner`，`bevy_app/src/app.rs:224`）。舊 Host 做的事各自歸誰，登記表判，這裡不分 |
| Shell | 舊 framework 的 `WindowShellClient`、`ImGuiShellClient`（不帶） | 不用它命名新概念 | gyo2 上沒有對應的概念。CI 的 shell 是另一回事 |
| Session | (i) AGENTS.md:276 的 session；(ii) 工具的編輯階段；(iii) 實機場次（field-session）；(iv) 連線 | 不用它命名新概念 | 各處的一般語意照用 |
| Service | (i) 獨立行程的後端服務：`services/`、Go service；(ii) 舊 Kernel 設計：World 之外做 IO 的「外部服務」 | 加限定詞 | 不加限定時指 (i)。(ii) 寫「World 外的 IO（舊稱外部服務）」 |
| Link | (i) 連結的一般語意：CMake 的 link、AGENTS.md:526；(ii) 舊 Kernel 設計：和外部交換資料的 Resource，舊 spec §0 另稱 external Resource | 加限定詞 | 不加限定時指 (i)。(ii) 寫「external Resource（舊）」 |
| Kernel | (i) `engine/kernel` 與它的設計；(ii) 「kernel 改造」計畫線；(iii) spike | 寫「X（舊）」 | 「Kernel」只用來指舊的 `engine/kernel` 與它的設計。新的概念用判斷機制的詞「通用機制」；它之後叫什麼、要不要分塊，R1 定 |
| Engine | (i) 架構層與 `engine/` 目錄；(ii) CMake target `GYO::Engine`（R0b 之後不存在） | 沿用 | 指 (i) |
| Product、Tool | AGENTS：Product＝有 owner 的遊戲或工具；Tool＝設計支援工具 | 沿用 | 同 AGENTS |
| Player、Peer、Match、Gateway；PvP 意思的 Client | PvP 產品的概念；舊 Kernel 的條文裡也出現 | 沿用 | 產品的詞。通用機制的文件要舉例時，標明是 PvP 的例子。「client」的一般語意不在此限 |

## 5. 和底稿（盤點 b）不同的地方

| 詞 | 底稿的建議 | 本表 |
|---|---|---|
| Runtime | 禁用 | 不用它命名新概念；Runtime Error 與 AGENTS 的用法照用 |
| Host | 改名為 runner 加 main | 改說「組裝（setup）」與「runner」；職責歸屬不在這裡分 |
| Domain | 改名為 clock | 寫「clock domain（舊）」；「時鐘」被 time 模組先占 |
| Scene | 照 Bevy 重新定義 | 不用它命名新概念：Bevy 的 Scene 是實體範本，是另一個概念 |
| Role | 執行緒的改叫 ThreadRole | 執行緒的保有原詞（帶過來的 threads 先占） |
| Service | IO 的概念改叫 IO worker | 寫「World 外的 IO（舊稱外部服務）」：worker 會和執行緒的角色相撞 |
| Frame | World F 照 Bevy 改名 | 寫「Frame domain（舊）」；另補音訊幀 |
| Layer | 渲染的叫 RenderLayers | 寫「渲染層」；另補 AGENTS §2 的意思 |
| App | 沿用 Bevy 的意思；建置的改名 game 或 product | 名字保留、不定義內容；建置的寫「遊戲類產品」 |
| Component | ECS 沿用，建置的改名 | 四個意思各加限定詞；AGENTS 的照原文 |
| Plugin | 只當登記單位 | 「plugin 套件」與「`Plugin` 型別」分開 |
| Kernel | 改名或拆開，交 R1 | 只指舊的；新概念寫「通用機制」 |
| Link | Kernel 層換中性的名字 | 寫「external Resource（舊）」 |
| Stage | 改名 | 寫「stage（舊）」 |
| Tick | 重新定義並寫明 | 只指固定步的步數；其他的寫 stamp 或計數 |

底稿沒有、本表補的詞：Entity、System、Schedule、Extract、Handle、Event、Snapshot、Registry／登記表、Module、Zone、Engine、Target、Clock／Time、Mount、Command、Sample、Input、Boundary，以及 Product／Tool 與 PvP 的產品詞。

底稿有、本表沒有單列的出處：AGENTS.md:276 的 session 已併進 Session 列；`KernelSettings` 是舊型別名，不是一詞多義。

## 6. 這張表不做的事

- 不改 AGENTS。AGENTS 的 Component、Runtime、host、Plugin System、資源，新文件引用時照它的原文讀。
- 不改 master，也不改 gyo2 上帶過來的程式。
- 不替「X（舊）」的概念取新名字：等登記表判了內容，R1 再定。

## 7. 要使用者確認的事

1. **第 1 節的界線**：這張表只管名字；內容與歸屬都留給去留登記表。
2. **第 2 節的優先順序**：一詞一義，其次先占，再其次才參照 Bevy。
3. **三個要你親自看的詞**：
   - **Component**：AGENTS 的「部件」、ECS 元件、os 的「路徑的一段」三方相撞。建議：AGENTS 不改；新文件裡 ECS 的一律寫「ECS 元件」，引用 AGENTS 時寫「部件」。若你想改 AGENTS 的用詞，那是風紀委員的事，要另外處理。
   - **Plugin**：你 11 月 11 日說「單個才叫 plugin」，指套件。舊 Kernel 設計另有一個叫 `Plugin` 的登記類別。建議分開寫「plugin 套件」與「`Plugin` 型別」。
   - **App**：名字是你定的。v1 用「一個 World 加 Registry 加 schedules」定義它，那是舊設計的內容，而且和 Bevy 的 App 不同：Bevy 的 App 底下有多個 SubApp，每個 SubApp 一個 World。v2 只保留名字，內容等登記表判。
4. **Host 只定名字**：改說「組裝（setup）」與「runner」（「組裝」已由你定名）。舊 Host 做的事各自歸誰，留給登記表。

## 8. 範囲の限界

- 出處：檢查者抽查舊程式與 AGENTS 18 處、Bevy 13 處；Bevy 的 Scene 一處 v1 寫錯，v2 已改。v2 新增的出處（audio、os、io、time 的幾處）由主會話用 `git grep` 核對過。
- 詞是從型別名與文件裡找的，可能有漏。之後發現新的相撞，補進這張表並標日期。
- [概念層重構稿](rebuild-concept.zh-Hant.md) 用的是 v1 的詞，其中「交換 Resource」「外部工作者」「階段」，以及 main 與 runner 的分工，都要照 v2 改；等它自己的檢查回來一起改。

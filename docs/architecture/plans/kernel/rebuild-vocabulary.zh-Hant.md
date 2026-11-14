# 同詞異義表（`v8-arch-rebuild` 步驟 1）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：提案，別模型檢查中；檢查後交使用者確認。**

出處：[判斷舊設計的機制](rebuild-judgment-rules.zh-Hant.md) 第 2 題（名字）。底稿是盤點 (b) 第 4 節的 20 個詞；本表逐一核對了出處（`master@08566260`，以下的檔:行都指這個版本），補了 12 個詞，並改了底稿的 4 處（第 5 節）。Bevy 指本機 `~/Code/GitHome/bevy` 的 6a911653f。

## 1. 這張表管什麼

- **只管名字。** 一個詞在這裡被改名或限定，不代表它指的設計有問題；設計對不對由登記表的第 1 題判。
- **「舊 Kernel 設計裡的意思」不是新義。** spec 與 target 的設計本身還沒過登記表，所以表裡只寫「之前的文件用這個詞指什麼」。
- **適用範圍**：gyo2 上新寫的文件、登記表、之後的程式。舊文件與 master 的程式不回頭改。

## 2. 定名的準則

1. **一詞一義**：在 Engine 的文件與程式裡，一個詞只指一件事。同一個詞有兩件事要說時，其中一件加限定詞或換詞。
2. **同概念用參考實作的詞**：概念和 Bevy 相同時用 Bevy 的詞，讀 Bevy 的資料不必換腦。概念不同時不借它的詞。
3. **帶過去的東西先占**：已核准帶過去的模組與 AGENTS 已經在用的詞義優先，新概念讓開。
4. **角色後綴不當型別名**：`Manager`、`Host`、`Shell`、`Runtime`、`Controller` 這類「擁有東西的物件」的後綴不用（使用者 2026-11-11 定的命名第 5 條）。這條只管型別名，不禁止在句子裡說這些事。
5. **名字跟著設計走的，現在不硬定**：概念本身還要過登記表或 R1 的，只給暫用詞，標「R1 定」。

## 3. 處置的種類

| 處置 | 意思 |
|---|---|
| 沿用 | 意思沒變 |
| 沿用、收窄 | 之前一詞多義，以後只留其中一義；其他意思換說法 |
| 加限定詞 | 幾個意思都要留，各自加前綴 |
| 暫用詞，R1 定 | 概念還沒定案；先給一個不會混淆的說法 |
| 不當概念名 | 不再用它命名 Engine 的概念或型別；一般語意照用 |

## 4. 表

### 4.1 與 Bevy 同概念的核心詞

| 詞 | 之前的用法（出處） | 處置 | gyo2 的用法 | 依據 |
|---|---|---|---|---|
| World | (i) PvP 的遊戲世界狀態：`WorldSnapshot`（`PvpMatch.hpp:22`）；(ii) 舊 Kernel 設計：資料容器；(iii) 文件說「世界內容」指關卡資料 | 沿用、收窄 | 只指資料容器（Bevy `world/mod.rs:96`）。遊戲的世界說「關卡內容」或「地圖內容」 | 準則 1、2 |
| Entity | (i) 產品的契約 id：`EntityId`、`EntityKey`；(ii) 舊 Kernel 設計：World 的鍵 | 沿用 | World 裡的一個實體（Bevy `entity/mod.rs:447`）。產品給的 id 說「契約 id」。兩者是不是同一個值是設計題，不在這裡定 | 準則 2 |
| Component | (i) 建置的元件：`GYO_KNOWN_COMPONENTS`（`GyoComponents.cmake:5`）；(ii) AGENTS 的 Component＝部件（AGENTS.md:249）；(iii) 舊 Kernel 設計：ECS 的元件，中文也寫「欄」 | 沿用、收窄 | ECS 的元件（Bevy `component/mod.rs:530`）。「欄」只指儲存裡的一欄。建置的那個說「建置元件」，它的去留由 R1 定。AGENTS 的用法交風紀委員 | 準則 1、2 |
| Resource | (i) 渲染與資產的物件：`ModelResource`、`TextResource`、`ResourceHandle`；(ii) 舊 Kernel 設計：ECS 的 Resource | 沿用、收窄 | ECS 的 Resource（Bevy `resource.rs:87`）。GPU 或載入的物件說「資產」或「GPU 物件」 | 準則 1、2 |
| System | (i) AGENTS 的 Subsystem＝子系統；(ii) 舊 Kernel 設計：一個 System 函式 | 沿用 | ECS 的 System。子系統寫全稱「Subsystem」 | 準則 2 |
| Schedule | 舊 Kernel 設計：一組排好順序的 System | 沿用 | 同左（Bevy `schedule/schedule.rs:413`） | 準則 2 |
| Plugin | 舊 Kernel 設計裡三個意思：登記單位（`Registry.hpp:60`）、plugins 層、依 owner 的分類 | 沿用、收窄 | 只指登記單位（Bevy `plugin.rs:57`）。層寫「plugins 層」（使用者 2026-11-11 定的層名），不說「一個 plugin 層」 | 準則 1、2 |
| App | (i) 建置的產品種類與目錄：`apps/`、KIND app；(ii) 產品的類別：`EditorApp`（`EditorApp.hpp:23`）、`PvpApplication`（`PvpApplication.hpp:110`）；(iii) 舊 Kernel 設計：World 加 Registry 加 schedules（使用者 2026-11-13 定名 App） | 沿用、收窄 | 只指 (iii)（Bevy `app.rs:85`）。建置的那個在文件裡說「產品」；目錄與 KIND 改不改由 R1 定 | 準則 1、2；使用者的定名 |
| Extract | 舊 Kernel 設計與 Bevy 同義：把主 World 的資料單向取到渲染 World | 沿用 | 同左 | 準則 2 |
| Asset | (i) `AssetManager`、`AssetHandle`；(ii) `assets/<產品>` 內容根目錄；(iii) 舊 Kernel 設計：`Assets<T>` | 沿用 | 一份載入後的內容。`assets/` 仍是內容根目錄的名字 | 準則 2 |
| Handle | (i) 資產與物件的參照：`AssetHandle`、`SoundHandle`、`ResourceHandle`；(ii) 舊 Kernel 設計：System 參數的基底（`Detail::Handle`），中文寫「句柄」 | 沿用、收窄 | 只指資產或物件的參照（Bevy `handle.rs:115`）。System 的參數說「System 參數」（Bevy 的 SystemParam） | 準則 1、2 |

### 4.2 意思要收窄或加限定詞的詞

| 詞 | 之前的用法（出處） | 處置 | gyo2 的用法 | 依據 |
|---|---|---|---|---|
| Tick | (i) 固定步的步數：`TickContext`（`FixedTickRuntime.hpp:17`）、PvP 的 tick；(ii) 舊 Kernel 設計：domain tick，也拿來指 Loading 的批次計數；(iii) Bevy 的 Tick 是變更偵測的計數（`change_detection/tick.rs:23`） | 沿用、收窄 | 只指固定步走了幾步。變更偵測的計數說「stamp」。批次計數不叫 tick | 準則 1、3。這裡不用 Bevy 的詞：GYO 的 tick 是時間，概念不同 |
| Frame | (i) 畫面的一幀；(ii) 輸入的一幀：`PhysicalInputFrame`；(iii) PvP wire 的 `SnapshotFrame`；(iv) 舊 Kernel 設計：`Domain::Frame`、Frame World | 沿用、收窄 | 只指畫面的一幀。(iv) 的概念名由 R1 定 | 準則 1 |
| Event | (i) 資料與輸入：`AnimationEvent`、`InputEvent`、`FileChangeEvent`；(ii) 舊 Kernel 設計：World 裡的事件緩衝（`EventReader`、`EventWriter`）；(iii) 這一版 Bevy 把緩衝的叫 Message（`message/mod.rs:100`），Event 留給觸發式的（`event/mod.rs:89`） | 暫用詞，R1 定 | (ii) 先寫「Kernel 事件」；(i) 寫全名。要不要照 Bevy 改叫 Message，和事件的設計一起定 | 準則 5 |
| Snapshot | (i) PvP wire 的狀態：`WorldSnapshot`、`SnapshotFrame`；(ii) 舊 Kernel 設計：回滾用的快照、驗證用的快照 | 加限定詞 | 「wire 快照」「回滾快照」「驗證快照」 | 準則 1 |
| Registry | (i) `LoaderRegistry`；(ii) 建置的產品登記：`GyoAppRegistry.cmake`；(iii) 舊 Kernel 設計：型別與 System 的登記表（`Registry.hpp`） | 加限定詞 | 「Kernel 的 Registry」「產品登記表」 | 準則 1 |
| Layer | (i) 渲染的層：`CompositeLayer`（`RenderTypes.hpp:98`）；(ii) 架構的層（AGENTS §6） | 加限定詞 | 「架構層」「渲染層」 | 準則 1 |
| Module | (i) engine 的模組；(ii) Go module；(iii) C++20 module | 加限定詞 | 單說「模組」指 engine 的模組；其他兩個寫全稱 | 準則 1 |
| Stage | (i) 打包的 stage 目錄；(ii) `ShaderStage`（`ShaderTypes.hpp:18`）；(iii) 舊 Kernel 設計：schedule 裡固定順序的一段；(iv) 動詞「暫存」：`StageMembershipRequests` | 暫用詞，R1 定 | (iii) 先寫「階段」；它在新設計裡還在不在、叫什麼，由 R1 定（Bevy 已經沒有 stage，用 SystemSet）。(i)(ii) 是各自領域的詞，不動。(iv) 寫「暫存」 | 準則 1、5 |
| Scene | (i) `SceneCapture`（`Renderer.hpp:12`）、`PreviewScene`；(ii) 舊 Kernel 設計：Loading 的「場景」階段、渲染的輸入 | 暫用詞，R1 定 | 先不單用：關卡資料寫「關卡內容」，渲染的輸入寫「渲染輸入」。底稿說 Bevy 的 Scene 在 `scene.rs:49`，查不到（這一版是 `ScenePatch`），不引用 | 準則 5 |

### 4.3 舊的角色詞

| 詞 | 之前的用法（出處） | 處置 | gyo2 的用法 | 依據 |
|---|---|---|---|---|
| Runtime | (i) `engine/runtime` 模組：`RuntimeLoop`、`FixedTickRuntime`、`IRuntimeClient`；(ii) `UiRuntime`；(iii) 舊 Kernel 設計：一個 World 加它的 Registry 與 schedules；(iv) 錯誤契約的「Runtime Error」；(v) AGENTS §8 的 Runtime＝執行中的遊戲（AGENTS.md:522） | 不當概念名 | (iii) 的概念叫 App。不當型別後綴。(iv)(v) 照用：它們是「執行期」的一般語意，又在帶過去的契約與 AGENTS 裡 | 準則 3、4 |
| Host | (i) `MatchRuntimeHost`（`MatchRuntimeHost.hpp:71`）、`IpcHost`；(ii) AGENTS 的 host＝一個行程（AGENTS.md:147）；(iii) CI 的 host＝建置機；(iv) 舊 Kernel 設計：提供 runner 的一方加 main | 不當概念名 | (iv) 拆成兩個詞：「main」（組裝：建 World、列 plugin、接外部服務）與「runner」（跑迴圈；Bevy `app.rs:224` 的 `set_runner`）。不當型別後綴。(ii)(iii) 的一般語意照用 | 準則 2、4 |
| Shell | `WindowShellClient`、`ImGuiShellClient`（framework） | 不當概念名 | 不當型別後綴。CI 的 shell 是另一回事 | 準則 4 |
| Session | (i) 連線；(ii) 工具的編輯階段：`ModelSession`、`DocumentSession`；(iii) 實機場次（field-session）；(iv) AGENTS 的 session | 不當概念名 | Engine 不用它命名概念。產品內部與流程文件照用 | 準則 1 |
| Role | (i) 執行緒的角色：`RoleThread`（`RoleThread.hpp:67`）、舊 Kernel 的 `Affinity::role`；(ii) 舊 Kernel 設計：欄的角色 State／Input（`Declarations.hpp:41`）；(iii) 建置登記的 ROLE 欄 | 沿用、收窄 | 只指執行緒的角色（帶過去的 threads 已在用）。(ii) 屬決定性的設計，名字由 R1 定。(iii) 是建置登記的欄名，不動 | 準則 1、3 |
| Service | (i) 獨立行程的後端服務：`services/`、Go service；(ii) 舊 Kernel 設計：World 之外做 IO 的「外部服務」 | 沿用、收窄 | 只指 (i)。(ii) 先寫「外部工作者」，名字由 R1 定 | 準則 1、5 |
| Link | (i) 連結：CMake 的 link、AGENTS.md:526；(ii) 舊 Kernel 設計：和外部交換資料的 Resource，名字來自網路 | 沿用、收窄 | 一般語意照用。(ii) 的通用概念寫「交換 Resource」；`Link` 這個名字只留給網路 | 準則 1 |
| Domain | (i) `NativeErrorDomain`（`OsError.hpp:46`，在帶過去的 os 裡）；(ii) PvP 文件的 match_domain；(iii) 舊 Kernel 設計：clock domain，封閉列舉（`Declarations.hpp:54`） | 不當概念名 | (iii) 的概念寫「時鐘」：一個各自前進的 tick 計數。Loading 算不算時鐘是設計題，不在這裡定 | 準則 3 |
| Zone | (i) os 的 time zone；(ii) 舊 Kernel 設計：決定性的區 B／C | 暫用詞，R1 定 | (ii) 寫「決定性區」。它住在哪一層由登記表判 | 準則 5 |

### 4.4 層與產品的詞

| 詞 | 之前的用法（出處） | 處置 | gyo2 的用法 | 依據 |
|---|---|---|---|---|
| Engine | (i) 架構層；(ii) `engine/` 目錄；(iii) CMake target `GYO::Engine`（asset、io、runtime 的單體） | 沿用、收窄 | 指架構層與目錄。(iii) 在 R0b 之後不存在 | 準則 1 |
| Kernel | (i) `engine/kernel` 與它的設計；(ii) 「kernel 改造」這條計畫線；(iii) spike | 暫用詞，R1 定 | 先指「三類產品共用的通用機制」。決定性、回滾、複製不算在裡面。它要不要照 Bevy 拆成 ecs 與 app 兩塊、叫什麼，由 R1 定 | 準則 5 |
| Product、Tool | AGENTS：Product＝有 owner 的遊戲或工具；Tool＝設計支援工具 | 沿用 | 同 AGENTS。建置層的「app」在文件裡說「產品」 | 準則 3 |
| Target | (i) CMake 的 target；(ii) `docs/architecture/target/`：改造期間的目標條文 | 沿用、收窄 | 只指 CMake 的 target。gyo2 的設計文件就是現行文件，不再叫 target；資料夾怎麼命名交圖書管理員 | 準則 1 |
| Player、Peer、Match、Client、Gateway | PvP 產品的概念；舊 Kernel 的條文裡也出現（`MembershipRequests`、Player 與 Peer） | 沿用 | 產品的詞。通用機制的條文與程式不用它們，舉例時要標明是 PvP 的例子 | AGENTS §6 |

## 5. 和底稿（盤點 b）不同的地方

| 詞 | 底稿的建議 | 本表 | 原因 |
|---|---|---|---|
| Runtime | 禁用 | 不當概念名；「Runtime Error」與 AGENTS 的用法照用 | 錯誤契約已帶過去，它的核心詞就是 Runtime Error |
| Domain | 改名為 clock | 概念寫「時鐘」，但 Loading 算不算時鐘不在這裡定 | 封閉的 domain 列舉本身還沒判 |
| Scene | 照 Bevy 重新定義 | 暫用詞，R1 定 | 底稿引的 Bevy 出處查不到 |
| Handle | 底稿沒有 | 收窄成「資產或物件的參照」 | 舊程式的 `AssetHandle` 與 Bevy 的 Handle 同義；舊 Kernel 把 System 參數也叫 handle，會相撞 |

另外補了底稿沒有的詞：Entity、System、Schedule、Extract、Handle、Event、Snapshot、Registry、Module、Zone、Engine、Target，以及 Product／Tool 與 PvP 的產品詞。

## 6. 這張表不做的事

- 不改 master 的任何東西，也不改 gyo2 上帶過去的程式。型別改名跟著 R2 的程式進行。
- 標「R1 定」的 7 個詞（Event、Stage、Scene、Service 的 (ii)、Zone、Kernel、Role 的 (ii)）只給暫用說法。
- AGENTS 裡的 Component、Runtime、host、Link 是 AGENTS 自己的用法，要不要調整交風紀委員；本表不要求改。

## 7. 要使用者確認的事

1. 第 2 節的五條準則。
2. 第 4 節各詞的處置。特別是：
   - Host 拆成 main 與 runner。
   - Runtime 不當概念名、改用 App。
   - Handle 只指資產或物件的參照。
   - Tick 不照 Bevy（Bevy 的 Tick 是變更偵測）。
3. 7 個「R1 定」的詞先用暫用說法。
4. 「plugins 層」這個層名照舊（你 2026-11-11 定的）。

## 8. 範囲の限界

- 出處核對了 29 處型別定義與 6 處 AGENTS 的行，都對得上；底稿引用的其他行（建置腳本、workflow、產品文件）沒有逐一重查。
- Bevy 的對照只到型別名與檔:行，沒有比對語意細節。
- 詞是從型別名與文件裡找的，可能有漏；之後發現新的相撞，補進這張表並標日期。

# 同詞異義表（`v8-arch-rebuild` 步驟 1）

**版本：v3（2026-11-14）。** Owner：架構（架構師會話）。批次：`v8-arch-gyo2-vocabulary-v3`。**狀態：v3 照使用者 2026-11-14 的回饋改寫：正文每個詞是一般的技術定義，來歷移到附錄。v3 沒有經過別模型的檢查。**

v3 和 v2 的差別：
- 詞條不增不減，處置大多不變（變的見附錄 B）。
- 「意思」一欄改寫成技術定義：讀者不必知道這個專案的歷史，也看得懂這個詞在 GYO2 指什麼。
- 每個詞以前在哪個檔、指什麼、為什麼換，移到附錄 A（系譜）。
- `gyo2` 的 `engine/kernel` 已經實作的概念（World、schedule、stage、clock domain、mount 等），照現行程式定義，不再寫「X（舊）」。它們該不該留，仍由去留登記表判（第 1 節）。

## 0. 使用者的回饋（2026-11-14，主會話）

> 其上上面的技術名詞大多沒問題，有問題的是解釋，這個改掉就好。而關鍵在於角色詞「Runtime、Host」這 2 個可以替換。Shell 和 Session 可以暫留，遇到時可以解釋。而且 Module 這個概念很不錯，Engine 內部的第三方叫 Module，Engine 外部的第三份叫 Plugin

> tick 不用換呀。這個概念不光是網絡，單機遊戲也有用到的。

照這段話定的：
- Runtime、Host：處置是「替換」（4.4）。
- Shell、Session：處置是「暫留」，遇到時解釋（4.4）。
- Tick：照用。它是一般的概念，不是網路專用（4.2）。
- Module、Plugin：見 4.3 的 Module 列與 4.1 的 Plugin 列。

**同日追加**（回覆這張表的 App 列，以及 Module／Plugin 的提問）：

> 這個可以換掉，用 Game 就好，產品實際上有 2 種，一種是 Game，另一種是 Tool

> Module：例如：第三方 Json lib、SDL（引擎沒有，遊戲就不成立的）。Plugin：網絡插件、ImGUI（引擎沒有，遊戲依然成立，但是功能受限）

> 這個正好解決了我糾結的 SDL 的歸屬問題。

> 沒有 SDL，遊戲就沒有畫面。沒有 ImGUI，遊戲依然可以動，但是沒有工具了。

照這段話定的：
- **產品分兩種：Game 與 Tool。**
- **Module 與 Plugin 的分法是「少了它，完整的遊戲還成不成立」**，不是「接不接第三方」：
  - Module：在 Engine 裡。少了它，遊戲不成立。例：JSON 函式庫、SDL。
  - Plugin：在 Engine 外。少了它，遊戲仍然成立，只是功能受限。例：網路、ImGui。
- SDL 的接入歸 Engine，是 Module。

這個分法帶出的後續（都還沒做，列著等之後判）：
- 帶過來的 audio、之後的渲染與輸入，各自算 Module 還是 Plugin，要用同一個問題逐一問。
- SDL 與 JSON 函式庫帶回 gyo2 時歸 Module。
- AGENTS §6 的用詞要照這個分法改（見下）。

**同日再追加**（回覆上面三個後續）：

> Match 不算完整遊戲，而且這個應該屬於產品端。

> （AGENTS §6 的用詞）改，雖然 master 還在用，不過以 GYO2 優先。

> （舊設計裡定名為 App 的那個型別）這個不叫 App 了，太模糊 直接用 Product

照這段話定的：
- **通用機制裡那個「一個 World 加上跑它的 Executor」的型別叫 `Product`。** 「App」這個詞在 gyo2 的新文件裡不用。
- **「遊戲成不成立」看的是完整的遊戲。** Match 不算完整的遊戲，它屬於產品端；不拿它當 Module 分法的反例。
- **AGENTS §6 的用詞要改**，以 gyo2 為準；master 的 AGENTS 不動。條文的改法另給使用者看過再改。

**「有問題的是解釋」的讀法**（架構師提出、使用者 2026-11-14 回「這個方向沒錯」）：詞本身是通用的技術名詞，照用；v2 的解釋把它們說成「舊 Kernel 設計的…」「PvP 的…」，還加了「（舊）」這類限定，這才是問題。v3 照這個讀法改寫。

## 1. 這張表管什麼，不管什麼

- **只管名字**：一個詞有幾個意思、在 GYO2 不加限定時指哪一個、其他意思怎麼說。
- **定義寫的是現況**：一個詞在 gyo2 已有程式時，定義照程式現在的行為寫；還沒有程式時，寫一般的技術意思。
- **不判內容**：一個概念該不該存在、該怎麼設計，由去留登記表判。定義照現況寫，不代表它已經判定要留。
- **不判歸屬**：一件事由誰持有、住在哪一層，由去留登記表判。
- **適用範圍**：gyo2 上新寫的文件與之後的程式。AGENTS、舊文件、master 的程式不回頭改；引用它們時照原文的意思讀。

## 2. 準則與優先順序

衝突時，前面的贏後面的。

1. **一詞一義**：在新寫的文件與程式裡，一個詞不加限定時只指一件事。
2. **先占**：gyo2 上的程式的公開介面與 AGENTS 已經在用的詞義，優先保有原詞；別的意思加限定詞或換詞。
3. **Bevy 的詞只當參照**：GYO2 的詞和 Bevy 同名，不代表是同一個概念；是不是同一個，看定義。
4. **設計沒定的不硬定名**：還沒有程式、也還沒判的概念，不替它取名字。

## 3. 處置的種類

| 處置 | 意思 |
|---|---|
| 沿用 | 只有一個意思，照用 |
| 加限定詞 | 幾個意思都在用；寫出每個意思的說法，並標明不加限定時指哪一個 |
| 寫「X（舊）」 | 只在引用 master 或舊文件、而 gyo2 沒有對應的概念時用。v3 的表裡沒有詞條用這個處置 |
| 不用它命名新概念 | 這個詞意思太多或已被先占；一般語意與既有的型別名照用 |
| 替換 | 舊的角色詞，新文件改用另一個詞（使用者 2026-11-14） |
| 暫留 | 不特別處理；文件裡遇到時當場解釋它指什麼（使用者 2026-11-14） |

## 4. 表

「在 GYO2 的意思」是不加限定時的意思；「其他意思與寫法」是同一個詞的別的意思，各自怎麼寫才不會混淆。每個詞的來歷在附錄 A。

### 4.1 核心詞

| 詞 | 在 GYO2 的意思 | 處置 | 其他意思與寫法 |
|---|---|---|---|
| World | 一個 `Product` 的全部執行期資料：各 ECS 元件的欄、Resource、事件緩衝、延後的變更、變更 stamp、各 clock domain 的 tick 數，以及它自己的狀態（Ready、Running、Applying、Interrupted）。從凍結的 Registry 建出來；要換一代資料就建一個新的 World | 加限定詞 | 遊戲裡的場地與關卡資料寫「關卡內容」「地圖內容」，不叫 World |
| Entity | World 裡的一個實體：本身沒有資料，只是一個 id（`EntityId`，64 位元整數），資料都在掛在它上面的 ECS 元件裡。id 由通用機制的配發器（allotter）配發，也可以由產品指定 | 加限定詞 | 產品自己定義、寫進資料契約或 wire 的識別碼寫「契約 id」。它和 `EntityId` 是不是同一個值，由去留登記表判 |
| Component | **ECS 元件**：掛在實體上的一筆有型別的資料；一個實體每種元件最多一筆。預設只准一個 System 寫（獨占），要多個寫者得標 `Shared` | 加限定詞 | AGENTS 的 Component 是「部件」（一個有 owner、有 Design Invariants 的軟體組成單位），引用 AGENTS 時寫「部件」。檔案路徑的一段寫「路徑的一段」。建置系統的組成單位寫「建置元件」。「欄」只指元件的儲存，不拿來指元件本身 |
| Resource | World 裡不屬於任何實體的值；每個型別最多一個。例：設定、統計、和外部交換資料的緩衝 | 加限定詞 | 一律寫英文「Resource」。中文「資源」留給性能預算與 OS 資源（記憶體、檔案、執行緒）。GPU 上的物件叫什麼，另定 |
| System | 一個普通函式，World 在 schedule 裡呼叫它。參數的型別就是它的存取宣告（讀什麼、寫什麼）；回傳 `SystemResult`。詳見 [System 說明頁](system-contract-page.zh-Hant.md) | 沿用 | AGENTS 的「子系統」寫全稱「Subsystem」 |
| Schedule | 一組 System 與它們的執行計畫：有名字、屬於一個 clock domain（或不屬於）、分成依序執行的 stage、指定由哪個執行緒角色呼叫。System 的先後由 `before`／`after` 排出，不照登記的先後。名字由產品給，通用機制不內建任何 schedule | 沿用 | 動詞「排程」的一般語意照用 |
| Plugin | **Engine 外的可選部分**：少了它，完整的遊戲仍然成立，只是功能受限。例：網路、ImGui。以 `plugins/` 下的一個套件交付 | 加限定詞 | 通用機制的登記單位寫「`Plugin` 型別」：一個在啟動時向 Registry 登記元件、Resource、事件、System 與 schedule 的類別，只在啟動時被呼叫。兩者的關係見第 6 節第 2 點。AGENTS 的「Plugin System」照原文。和 Module 是一對，見 4.3 |
| Product | **`Product` 型別**：一個 World、建出它的凍結 Registry，以及依序執行 schedule 的 Executor，三者的組合。它本身不持有執行緒；`Run(schedule)` 由 runner 呼叫。System 回 `Err` 時，它把錯誤交給產品登記的 handler 一次，並讓 World 進入 Interrupted | 加限定詞 | AGENTS 的 Product（有 owner 的遊戲或工具）寫中文「產品」；產品分 Game 與 Tool 兩種。程式與文件裡寫成 `Product`（程式碼格式）時才指型別。「App」不用 |
| Extract | 從一個 World 單向讀資料到另一個 World 的步驟：在標成 extract 的 schedule 裡，System 用 `Extract<Read<T>>`、`ExtractRes<T>` 唯讀地看來源 World，寫進自己的 World。來源不能寫。典型用途是把模擬的資料交給渲染 | 沿用 | 同左 |
| Asset | 一份從檔案載入、在程式裡用 id 或 handle 參照的內容（模型、貼圖、聲音、關卡資料等）。gyo2 目前還沒有 asset 的程式 | 沿用 | `assets/` 是內容根目錄的名字 |
| Handle | 一個指向別處物件的不透明參照：持有者拿它去查，不直接持有物件。每一種 handle 都寫全名（例：audio 的 `SoundHandle` 是一次播放的 id），不單寫「handle」 | 加限定詞 | System 的參數寫「System 參數」，實體的 id 寫「實體 id」，不叫句柄。資產的參照要不要讓資產保持存活，另定 |

### 4.2 帶過來的程式已經在用、容易相撞的詞

| 詞 | 在 GYO2 的意思 | 處置 | 其他意思與寫法 |
|---|---|---|---|
| Frame | **畫面幀**：畫面畫一次 | 加限定詞 | audio 的取樣幀（每個聲道各一個取樣，48000 幀＝1 秒）寫「音訊幀」。名為 Frame 的 clock domain 寫「Frame domain」 |
| Tick | 一個 clock domain 前進一步，以及它的步數。每個 clock domain 有自己的 tick 數，由該 domain 的 clock schedule 每跑一次加一。固定步長的模擬（Fixed domain）最常用它，單機與連線都一樣 | 加限定詞 | 變更偵測用的計數寫「stamp」。其他的計數（批次數、重試數）寫「計數」，不叫 tick |
| Clock、Time | **時鐘**：真實時間的來源，例：time 模組的 `MonotonicClock`（單調、不會倒退）。命名空間 `GYO::Time` 已被 time 模組占用 | 加限定詞 | 「clock domain」是另一回事：一條獨立推進的時間線（Fixed、Frame、Loading），各有自己的 tick 與 schedule。World 裡的時間 Resource 叫什麼另定，要避開 `GYO::Time` |
| Domain | 不單用 | 不用它命名新概念 | 時間線寫「clock domain」（程式的型別名是 `Domain`）。錯誤來源的分類寫全名（os 的 `NativeErrorDomain`）。產品的領域照產品的說法 |
| Mount | **檔案系統的掛載**：把一個實體目錄或封包接到虛擬路徑底下，例：io 的 `MountTable`、`MountPoint` | 加限定詞 | System 在哪個 schedule 的哪個 stage 執行（連同先後、所屬的 set、run 條件）寫「System 的掛點（mount）」；一個 System 可以有多個掛點，每個 schedule 最多一個 |
| Command | 一般語意：一個要執行的指令。各模組內部的指令型別寫全名（例：audio 內部的 `Detail::Command`） | 加限定詞 | 延後到同步點才套用的結構變更（加減元件）寫「`Commands`」或「延後的變更」 |
| Sample | **音訊的取樣** | 加限定詞 | 從外部讀進 World 的一筆輸入，型別名以 `Sample` 或 `Input` 結尾（[命名規範](rebuild-naming.zh-Hant.md) N23）；文件裡寫「boundary 輸入」 |
| Role | **執行緒角色**：一條有名字、有固定職責的執行緒，例：threads 的 `RoleThread`；System 的執行緒親和也用角色名指定（`"Main"` 是平台的主執行緒） | 加限定詞 | ECS 元件的 State／Input 之分寫「欄角色」。建置登記表的 ROLE 欄照欄名 |

### 4.3 其他一詞多義的詞

| 詞 | 在 GYO2 的意思 | 處置 | 其他意思與寫法 |
|---|---|---|---|
| Event | **World 的事件**：System 用 `EventWriter<E>` 送出、用 `EventReader<E>` 讀的一筆訊息。每種事件屬於一個 clock domain，只能在那個 domain 的 schedule 裡讀寫；排在寫入者後面的 System，同一個 tick 就讀得到；每個 tick 的清理點（預設是最後一個 stage 結束時）丟掉這個 tick 以前的事件 | 加限定詞 | 輸入、檔案變更、動畫等其他領域的事件寫全名（`InputEvent`、`FileChangeEvent`）。之後若改叫 Message，會撞到日誌與 wire 的「訊息」，另定 |
| Snapshot | 不單用 | 加限定詞 | 某一刻狀態的完整拷貝，依用途寫「wire 快照」（網路上傳給對方的狀態）、「回滾快照」（倒回去重算用）、「驗證快照」（比對兩次執行是否一致用） |
| Registry、登記表 | **Registry**：收下產品給的 `Plugin` 清單，依序讓每個 plugin 登記，檢查全部登記，凍結，並替每個 schedule 排出執行計畫（Plan）。凍結後不能再登記 | 加限定詞 | 建置系統的產品清單寫「產品登記檔」。決定舊概念去留的那張表寫「去留登記表」 |
| Layer | 不單用 | 加限定詞 | 「架構層」：Engine、plugins、產品這種依賴方向固定的分層。「基盤／應用之分」：AGENTS §2 依「改了它上面要跟著改多少」的分法。「渲染層」：畫面合成的一層 |
| Module | **Engine 內的必要部分**：少了它，完整的遊戲不成立。以 `engine/<名字>/` 一個目錄、一個 CMake target 交付。接第三方函式庫的也算，例：JSON 函式庫、SDL | 沿用 | Go 的 module、C++20 的 module 照各自語言的意思，寫「Go module」「C++ module」。和 Plugin 是一對：差別是 Engine 內或外，判準是「少了它，完整的遊戲還成不成立」 |
| Stage | **schedule 的一段**：一個 schedule 由幾個 stage 依序組成；每個 stage 結束時有一個同步點，套用這段裡記下的延後變更。stage 的名字由產品給 | 加限定詞 | 打包的暫存目錄、shader 的階段（`ShaderStage`）照各自領域的詞。「stage」不當動詞用 |
| Scene | 不單用 | 不用它命名新概念 | 關卡的資料寫「關卡內容」，渲染的輸入寫「渲染輸入」。可以生成的實體範本（Bevy 的 Scene）要不要有，另判 |
| Input | **玩家的輸入**（鍵盤、滑鼠、手把），以及處理它的輸入模組 | 加限定詞 | 欄角色 Input（可以被入口 System 寫的決定性資料）寫「Input 欄角色」 |
| Boundary | **架構的邊界**：AGENTS 的 Architecture／Product／Module Boundary | 加限定詞 | 從外部進入 World 的資料寫「boundary 輸入」（欄角色是 Input 的資料）。存取驗證在 System 前後比對資料寫「boundary 比對」 |
| Zone | 不單用 | 加限定詞 | 「決定性區」：一筆資料或一個 System 承諾的決定性等級。C 區＝跨平台逐位元相同；B 區（預設）＝同一個執行檔在同一個平台上每次相同。「時區」：時間的時區 |
| Target | **CMake 的 target**：一個建置產物（函式庫或執行檔）與它的依賴 | 加限定詞 | 「render target」寫全稱。`docs/architecture/target/` 這個目錄寫「目標條文」 |

### 4.4 角色詞與產品的詞

| 詞 | 在 GYO2 的意思 | 處置 | 其他意思與寫法 |
|---|---|---|---|
| Runtime | 不拿來指元件或型別 | 替換 | 原本指「把一個 World 跑起來的那個型別」的，寫 `Product`。「Runtime Error」照用：執行時才會發生、程式應該處理的狀況（檔案不在、斷線），相對於 Programmer Error。AGENTS §8 的 Runtime（執行中的遊戲，相對於 Editor）照原文 |
| Host | 不拿來指元件或型別 | 替換 | 原本 Host 做的事分兩個詞：**組裝（setup）**＝產品啟動時列出要用的 `Plugin`、建 Registry 與 `Product` 的那一段；**runner**＝跑主迴圈、反覆呼叫 `Product::Run` 的函式（每個 `Product` 一個，由某個 `Plugin` 在啟動時裝上）。哪件事歸誰，由去留登記表判。AGENTS §2 的 host（搭載基盤的一個應用）與 CI 的 host（建置機）照原文 |
| Shell | 目前沒有對應的概念 | 暫留 | 遇到時解釋。命令列的 shell 照一般語意 |
| Session | 一段有開始與結束的期間 | 暫留 | 遇到時解釋它指哪一種：AGENTS 的 session、工具的編輯階段、實機場次、一條連線 |
| Service | **獨立行程的後端服務**：例：`services/` 下的 Go service | 加限定詞 | 在 World 之外替 System 做 IO 的部分寫「World 外的 IO」 |
| Link | 一般語意：連結、連結器的 link（例：CMake 的 link） | 加限定詞 | 和 `Product` 外部交換資料的 Resource 寫「external Resource」（登記時標 `external`，一律在 B 區） |
| Kernel | **通用機制**：World、Registry、System、schedule、Executor、`Product` 這些各產品共用的 ECS 機制。程式在 `engine/kernel`，命名空間 `GYO::Kernel`（[命名規範](rebuild-naming.zh-Hant.md) N17） | 沿用 | 文件裡解釋它是什麼時寫「通用機制」 |
| Engine | **各產品共用的那一層**：`engine/` 目錄，包含通用機制與各 Module。不依賴任何產品 | 沿用 | 同左 |
| Product、Tool | **產品**：有 owner 的遊戲或工具，分 Game 與 Tool 兩種；Tool 是設計支援工具（例：編輯器） | 沿用 | 寫成 `Product`（程式碼格式）時指 4.1 的型別 |
| Player、Peer、Match、Gateway；對戰的 Client | 產品的詞（例：PvP 產品），不是通用機制的詞 | 沿用 | 通用機制的文件要舉例時，標明是哪個產品的例子。「client」的一般語意不在此限 |

## 5. 這張表不做的事

- 不改 AGENTS。AGENTS 的 Component、Runtime、host、Plugin System、資源，新文件引用時照它的原文讀。（AGENTS §6 的用詞要照第 0 節改，那是另一件事，條文先給使用者看。）
- 不改 master，也不改 gyo2 上帶過來的程式。
- 不替還沒有程式、也還沒判的概念取名字。

## 6. 要使用者確認的事

1. **第 1 節的界線**：這張表只管名字；定義照現況寫，內容與歸屬留給去留登記表。
2. **Plugin 的兩個意思**：「Engine 外的可選部分」（第 0 節）與通用機制的「`Plugin` 型別」（登記單位）是兩件事。照現行程式，Module 也要透過一個 `Plugin` 型別向 Registry 登記它的元件與 System；這會讓「SDL 是 Module，卻用 `Plugin` 型別登記」。要不要替其中一個換名字，請你決定。
3. **Product 的兩個意思**：中文「產品」（Game 或 Tool）與 `Product` 型別（一個 World 加上 Executor）。v3 用中文與程式碼格式區分；若覺得仍會混淆，請你決定要不要另取型別名。
4. **Kernel 算不算 Module**：照 Module 的判準，少了通用機制，遊戲也不成立；但[命名規範](rebuild-naming.zh-Hant.md) N17 把通用機制（`GYO::Kernel::`）與 Module（`GYO::Engine::<Module>::`）分成兩個根。v3 照 N17 把兩者分開寫。

## 7. 範囲の限界

- 定義裡關於程式的事實，核對的是 `gyo2@051d686c` 的 `engine/kernel/include/engine/kernel/`（`Product.hpp`、`World.hpp`、`Registry.hpp`、`Declarations.hpp`、`Params.hpp`）與 `engine/entity_store`；決定性區的等級核對的是 `docs/architecture/target/determinism.md`。逐字比對的只有這些標頭；其他模組（audio、io、os、threads、time）的型別名只確認存在。
- 詞是從型別名與文件裡找的，可能有漏。之後發現新的相撞，補進這張表並標日期。
- [概念層重構稿](rebuild-concept.zh-Hant.md)與[命名規範](rebuild-naming.zh-Hant.md)用的是 v1、v2 的詞（「X（舊）」、「App」），還沒照 v3 改。

---

## 附錄 A. 系譜：每個詞以前在哪裡、指什麼

出處的寫法：舊程式與舊文件指 `master@08566260`；帶過來的程式指 gyo2（R0b 時在分支 `claude/gyo2-r0b-delete`）；Bevy 指本機 `~/Code/GitHome/bevy` 的 6a911653f，路徑從 `crates/` 起算。「舊 Kernel 設計」指 2026-11-11～13 的 kernel 規格與 `docs/architecture/target/` 的條文；其中一部分在 2026-11-14 以後成了 gyo2 的 `engine/kernel`。

### A.1 核心詞

| 詞 | 以前的意思與出處 | v3 怎麼處理 |
|---|---|---|
| World | (i) PvP 的遊戲世界狀態：`WorldSnapshot`（`PvpMatch.hpp:22`）；(ii) 舊 Kernel 設計：資料容器；Bevy 同詞（`bevy_ecs/src/world/mod.rs:96`）；(iii) 舊文件「世界內容」指關卡資料 | (ii) 成了 gyo2 的 `World`，照程式定義。(i)(iii) 改寫「關卡內容」等 |
| Entity | (i) 產品的契約 id：`EntityId`、`EntityKey`；(ii) 舊 Kernel 設計：World 的鍵；Bevy 同詞（`bevy_ecs/src/entity/mod.rs:447`） | (ii) 照程式定義。`EntityId` 現在是 `entity_store` 的 64 位元整數 |
| Component | (i) AGENTS 的 Component＝部件：State Authority、Design Invariants、外部 Component 的條文（AGENTS.md:249、:273-281、:386-393）；(ii) os 的 `SamePathComponent`（`engine/os/include/engine/os/Files.hpp:153`）；(iii) 建置的元件（`GyoComponents.cmake:5`）；(iv) 舊 Kernel 設計：ECS 的元件，中文也寫「欄」；Bevy 同詞（`bevy_ecs/src/component/mod.rs:530`） | 不加限定指 (iv)。「欄」預設了欄式儲存，所以不拿來指元件本身 |
| Resource | (i) AGENTS 的「資源」：性能與資源的預算、OS 資源；(ii) 舊程式的 `ModelResource`、`TextResource`、`ResourceHandle`（沒帶過來）；(iii) 舊 Kernel 設計：ECS 的 Resource；Bevy 同詞（`bevy_ecs/src/resource.rs:87`） | 不加限定指 (iii)，寫英文 |
| System | (i) AGENTS 的 Subsystem；(ii) 舊 Kernel 設計：一個 System 函式 | (ii) 照程式定義 |
| Schedule | 舊 Kernel 設計：一組排好順序的 System；Bevy 同詞（`bevy_ecs/src/schedule/schedule.rs:413`）。audio 的 `Scheduled` 是動詞 | 照程式定義 |
| Plugin | (i) 使用者 2026-11-11：「層與目錄叫 plugins，單個才叫 plugin」，指 `plugins/` 下的一個套件；(ii) 舊 Kernel 設計：登記的類別 `Plugin`（`Registry.hpp:60`）；Bevy 的 `Plugin` 是這個意思（`bevy_app/src/plugin.rs:57`）；(iii) AGENTS.md:659「Plugin System」指外掛機制；(iv) 使用者 2026-11-14：Engine 外、少了遊戲仍成立的部分 | 不加限定指 (iv)（它包含 (i) 的套件）。(ii) 寫「`Plugin` 型別」。v2 寫「plugin 套件」，v3 改成以 (iv) 定義，見附錄 B |
| Product（v2 的 App 列） | (i) AGENTS 的 Application＝產品（AGENTS.md:377）；(ii) 建置的 KIND app 與 `apps/` 目錄；(iii) 舊產品的類別 `EditorApp`、`PvpApplication`；(iv) 舊 Kernel 設計：原本叫 Runtime 的型別，2026-11-13 定名 `App`（`v8-arch-kernel-app-name`），2026-11-14 改叫 `Product`（`v8-arch-gyo2-product-type-name`）。Bevy 的 App 底下有多個 SubApp、每個 SubApp 一個 World，和 gyo2 的 `Product`（一個 World）不同 | 詞條名從 App 改成 Product，見附錄 B |
| Extract | 舊 Kernel 設計：把資料從主 World 單向取到渲染 World；Bevy 同詞 | 照程式定義 |
| Asset | (i) `assets/<產品>` 內容根目錄；(ii) 舊程式的 `AssetManager`、`AssetHandle`；(iii) 舊 Kernel 設計的 `Assets<T>`；Bevy 的 `Assets`（`bevy_asset/src/assets.rs:288`） | 寫一般定義 |
| Handle | (i) 舊程式的 `AssetHandle`：id 加世代的記號，不讓資產保持存活；(ii) audio 的 `SoundHandle`（`Mixer.hpp:50`）；(iii) OS 的 handle；(iv) 舊 Kernel 設計的「句柄」：System 參數的基底，也拿來說實體的鍵（gyo2 的 `engine/kernel` 裡是內部的 `Detail::Handle`）；(v) Bevy 的 `Handle<A>`（`bevy_asset/src/handle.rs:115`）：強參照會讓資產保持存活，和 (i) 不同 | 寫一般定義；(iv) 不叫句柄 |

### A.2 帶過來的程式已經在用、容易相撞的詞

| 詞 | 以前的意思與出處 | v3 怎麼處理 |
|---|---|---|
| Frame | (i) 畫面的一幀；(ii) audio：`kSampleRate = 48000; // frames per second`（`AudioFormat.hpp:10`）；(iii) 舊 Kernel 設計的 `Domain::Frame` 與 Frame World | (iii) 在 gyo2 是 `Domain::Frame`，寫「Frame domain」，不再加「（舊）」 |
| Tick | (i) 固定步的步數：`TickContext`（master 的 `FixedTickRuntime.hpp:17`，沒帶過來）、PvP 的 tick；(ii) 舊 Kernel 設計：domain tick，也拿來指 Loading 的批次計數；(iii) Bevy 的 `Tick`：變更偵測的計數（`bevy_ecs/src/change_detection/tick.rs:23`） | v2 把 (i) 寫成「PvP 的 tick」，使用者指出 tick 不只網路用。v3 照 gyo2 程式定義成 clock domain 的步數（`World::Tick(Domain)`、`CurrentTick`），見附錄 B |
| Clock、Time | (i) time 的 `MonotonicClock`、`TimePoint`（`MonotonicClock.hpp:18-19`）；(ii) 舊 Kernel 設計的 clock domain、`HostClockSample`；(iii) Bevy 的 `Time<T>` Resource | (ii) 在 gyo2 是 `Domain`，寫「clock domain」 |
| Domain | (i) os 的 `NativeErrorDomain`（`OsError.hpp:46`）；(ii) PvP 文件的 match_domain；(iii) 舊 Kernel 設計：clock domain，封閉列舉（`Declarations.hpp:54`） | (iii) 寫「clock domain」 |
| Mount | (i) io 的 `MountTable`、`MountPoint`（`engine/io/include/engine/io/fs/MountTable.hpp:88`）；(ii) 舊 scheduler 的 `Mount`：System 掛在哪個 schedule | (ii) 在 gyo2 是 `Registry.hpp` 的 `Mount`，寫「System 的掛點（mount）」 |
| Command | (i) audio 內部的 `Detail::Command`（`Mixer.hpp:17`）；(ii) 舊 Kernel 設計的 `Commands`；Bevy 的 `Commands` | 同 v2 |
| Sample | (i) audio 的取樣；(ii) 舊命名條文：boundary 輸入帶 `Sample` 後綴 | (ii) 已由命名規範 N23 定案（2026-11-14），不再「由登記表判」 |
| Role | (i) threads 的 `RoleThread`（`RoleThread.hpp:67`）；舊 Kernel 的 `Affinity::role` 同義；(ii) 舊 Kernel 設計：欄的角色 State／Input（`Declarations.hpp:41`）；(iii) 建置登記的 ROLE 欄 | (ii) 寫「欄角色」，不再加「（舊）」 |

### A.3 其他一詞多義的詞

| 詞 | 以前的意思與出處 | v3 怎麼處理 |
|---|---|---|
| Event | (i) `AnimationEvent`、`InputEvent`、`FileChangeEvent`；(ii) 舊 Kernel 設計：World 裡的事件緩衝；(iii) 這一版 Bevy：緩衝的叫 `Message`（`bevy_ecs/src/message/mod.rs:100`），`Event` 留給觸發式的（`bevy_ecs/src/event/mod.rs:89`） | 不加限定指 (ii)，照程式定義 |
| Snapshot | (i) PvP wire 的 `WorldSnapshot`、`SnapshotFrame`；(ii) 舊 Kernel 設計：回滾用、驗證用的快照 | 寫法同 v2，定義改成用途 |
| Registry、登記表 | (i) 舊 Kernel 設計的 `Registry.hpp`；(ii) 建置的 `GyoAppRegistry.cmake`、`projects.csv`；(iii) `LoaderRegistry`（沒帶過來）；(iv) [判斷機制](rebuild-judgment-rules.zh-Hant.md)第 5 節的登記表 | (i) 照程式定義 |
| Layer | (i) AGENTS §6 的架構層；(ii) AGENTS §2「層の区別」；(iii) `CompositeLayer`（`RenderTypes.hpp:98`） | 同 v2 |
| Module | (i) engine 的模組；(ii) Go module；(iii) C++20 module。使用者 2026-11-14 給了 Module 與 Plugin 的分法（第 0 節） | 照第 0 節定義 |
| Stage | (i) 打包的 stage 目錄；(ii) `ShaderStage`（`ShaderTypes.hpp:18`）；(iii) 舊 Kernel 設計：schedule 裡固定順序的一段；(iv) 舊設計的動詞：`StageMembershipRequests`；Bevy 沒有 stage，用 `SystemSet` | (iii) 在 gyo2 是 `ScheduleOptions::stages`，不再加「（舊）」 |
| Scene | (i) `SceneCapture`（`Renderer.hpp:12`）、`PreviewScene`；(ii) 舊 Kernel 設計：Loading 的「場景」階段、渲染的輸入；(iii) Bevy 的 `Scene`（`bevy_scene/src/scene.rs:49`）：可以生成的實體範本 | 同 v2 |
| Input | (i) 玩家的輸入、輸入模組；(ii) 舊 Kernel 設計：欄角色 `Input`、boundary 輸入 | (ii) 寫「Input 欄角色」，不再加「（舊）」 |
| Boundary | (i) AGENTS 的 Architecture／Product／Module Boundary；(ii) 舊 Kernel 設計的 boundary Resource | (ii) 在 gyo2 程式裡是欄角色 Input 的資料，寫「boundary 輸入」；另補程式裡的「boundary 比對」 |
| Zone | (i) os 的時區；(ii) 舊 Kernel 設計：決定性的區 | (ii) 照程式（`Zone::C`、`Zone::B`，預設 B）與 `determinism.md` 的等級定義 |
| Target | (i) CMake 的 target；(ii) render target；(iii) `docs/architecture/target/`：改造期間的目標條文 | 同 v2 |

### A.4 角色詞與產品的詞

| 詞 | 以前的意思與出處 | v3 怎麼處理 |
|---|---|---|
| Runtime | (i) 錯誤契約的 Runtime Error；(ii) AGENTS §8 的 Runtime（AGENTS.md:522）；(iii) 舊的 `engine/runtime` 模組與 `UiRuntime`（沒帶過來）；(iv) 舊 Kernel 設計：後來定名 App、再改叫 `Product` 的那個型別 | 替換（使用者 2026-11-14）。gyo2 的 `Product.hpp` 註解仍寫 "spec section 0 \"Runtime\""，那是出處的引用 |
| Host | (i) AGENTS.md:147 的 host；(ii) CI 的 host；(iii) 舊程式的 `MatchRuntimeHost`、`IpcHost`（沒帶過來）；(iv) 舊 Kernel 設計：提供 runner 的一方加 main | 替換（使用者 2026-11-14）。先前的提案「main」看起來像主執行緒（執行緒角色也有一個叫 Main），所以使用者定名「組裝（setup）」。runner 的說法照 Bevy 的 `set_runner`（`bevy_app/src/app.rs:224`）。gyo2 程式的註解仍寫 Host（例：`Registry.hpp` 的 "the Host's list"、`Declarations.hpp` 的 "Given by the Host"） |
| Shell | 舊 framework 的 `WindowShellClient`、`ImGuiShellClient`（沒帶過來） | 暫留 |
| Session | (i) AGENTS.md:276 的 session；(ii) 工具的編輯階段；(iii) 實機場次（field-session）；(iv) 連線 | 暫留 |
| Service | (i) `services/`、Go service；(ii) 舊 Kernel 設計：World 之外做 IO 的「外部服務」 | (ii) 寫「World 外的 IO」 |
| Link | (i) CMake 的 link、AGENTS.md:526；(ii) 舊 Kernel 設計：和外部交換資料的 Resource，舊 spec §0 另稱 external Resource | (ii) 在 gyo2 是 `ResourceOptions::external`，寫「external Resource」，不再加「（舊）」 |
| Kernel | (i) `engine/kernel` 與它的設計；(ii) 「kernel 改造」計畫線；(iii) spike | 2026-11-11 命名規範第 1 條；2026-11-14 轉入並由架構師訂（`v8-arch-gyo2-naming-1-to-4`）。v2 原寫「Kernel 只指舊的」，已改 |
| Engine | (i) 架構層與 `engine/` 目錄；(ii) CMake target `GYO::Engine`（R0b 之後不存在） | 指 (i) |
| Product、Tool | AGENTS：Product＝有 owner 的遊戲或工具；Tool＝設計支援工具 | 加上使用者 2026-11-14 的 Game／Tool 之分 |
| Player、Peer、Match、Gateway；PvP 意思的 Client | PvP 產品的概念；舊 Kernel 的條文裡也出現 | 同 v2 |

## 附錄 B. 各版的變動

### B.1 v3 相對 v2

| 詞 | v2 | v3 | 理由 |
|---|---|---|---|
| 全部 | 意思欄寫出處與舊設計 | 意思欄寫技術定義，出處移到附錄 A | 使用者 2026-11-14 回饋 |
| Frame、Clock、Domain、Mount、Role、Stage、Input、Boundary、Link | 寫「X（舊）」 | 照 gyo2 現行程式定義，寫限定詞（「Frame domain」「clock domain」「System 的掛點」「欄角色」等），不加「（舊）」 | 這些概念已在 gyo2 的 `engine/kernel` 實作；「（舊）」是使用者指出的解釋問題之一。去留仍由登記表判 |
| Tick | 不加限定指固定步的步數 | 不加限定指 clock domain 的步數（固定步是最常見的一種） | gyo2 的程式每個 clock domain 都有 tick（`World::Tick(Domain)`） |
| Sample | 第二個意思「隨 boundary（舊）由登記表判」 | 引用 N23 | N23 已定案 |
| Plugin | 不加限定寫「plugin 套件」 | 不加限定指「Engine 外的可選部分」 | 使用者 2026-11-14 的 Module／Plugin 分法 |
| App → Product | 詞條名 App，v2 已記「改叫 Product」 | 詞條名改成 Product（不算新增詞條，是同一列改名） | 使用者 2026-11-14：「直接用 Product」 |
| §3「寫 X（舊）」 | 多列使用 | 保留這個處置，但限於 gyo2 沒有對應概念時；v3 沒有列用它 | 同上 |

### B.2 v2 相對底稿（盤點 b）

| 詞 | 底稿的建議 | v2 |
|---|---|---|
| Runtime | 禁用 | 不用它命名新概念；Runtime Error 與 AGENTS 的用法照用（後由使用者改為替換） |
| Host | 改名為 runner 加 main | 改說「組裝（setup）」與「runner」；職責歸屬不在這裡分 |
| Domain | 改名為 clock | 寫「clock domain」；「時鐘」被 time 模組先占 |
| Scene | 照 Bevy 重新定義 | 不用它命名新概念：Bevy 的 Scene 是實體範本，是另一個概念 |
| Role | 執行緒的改叫 ThreadRole | 執行緒的保有原詞（帶過來的 threads 先占） |
| Service | IO 的概念改叫 IO worker | 寫「World 外的 IO」：worker 會和執行緒的角色相撞 |
| Frame | World F 照 Bevy 改名 | 寫「Frame domain」；另補音訊幀 |
| Layer | 渲染的叫 RenderLayers | 寫「渲染層」；另補 AGENTS §2 的意思 |
| App | 沿用 Bevy 的意思；建置的改名 game 或 product | 名字保留、不定義內容（後由使用者改為 Product） |
| Component | ECS 沿用，建置的改名 | 四個意思各加限定詞；AGENTS 的照原文 |
| Plugin | 只當登記單位 | 「plugin 套件」與「`Plugin` 型別」分開 |
| Kernel | 改名或拆開，交 R1 | 只指舊的（後改：通用機制的程式與目錄叫 Kernel） |
| Link | Kernel 層換中性的名字 | 寫「external Resource」 |
| Stage | 改名 | 寫「stage」 |
| Tick | 重新定義並寫明 | 只指固定步的步數；其他的寫 stamp 或計數 |

底稿沒有、v2 補的詞：Entity、System、Schedule、Extract、Handle、Event、Snapshot、Registry／登記表、Module、Zone、Engine、Target、Clock／Time、Mount、Command、Sample、Input、Boundary，以及 Product／Tool 與 PvP 的產品詞。

底稿有、表裡沒有單列的出處：AGENTS.md:276 的 session 已併進 Session 列；`KernelSettings` 是型別名，不是一詞多義。

### B.3 v1、v2 的經過

- 出處：[判斷舊設計的機制](rebuild-judgment-rules.zh-Hant.md) 第 2 題（名字）。底稿是盤點 (b) 第 4 節的 20 個詞。
- v1 由主會話（Fable）寫。別模型檢查（Opus／high，一次）：blocker 8、should 14、nit 8，全部採用。v1 被指出的主要錯誤：在名字這一層偷判了內容與歸屬（例：「決定性不算在 Kernel 裡」、替 main 與 runner 分職責、用還沒判過的舊設計定義 App）。v2 把這些都拿掉。
- v1 的準則 4「角色後綴不當型別名」在 v2 拿掉：它出自舊命名條文第 5 條（`docs/architecture/target/naming.zh-Hant.md:18`），來源是側邊會話轉述、沒有原話，理由也是內容主張。2026-11-14 使用者也只轉入那份條文的第 1～4 條。
- v2 的檢查：檢查者抽查舊程式與 AGENTS 18 處、Bevy 13 處；Bevy 的 Scene 一處 v1 寫錯，v2 已改。v2 新增的出處（audio、os、io、time）由主會話用 `git grep` 核對。
- 證據在主 checkout `build/evidence/rebuild-concept/`：`vocabulary-v1.md`、`vocabulary-checker-opus.md`。v3 沒有另存證據。

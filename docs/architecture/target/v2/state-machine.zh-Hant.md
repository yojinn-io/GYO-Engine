# 狀態機 Module（`engine/modules/state_machine`）

更新：2026-11-22（新頁，`v8-arch-gyo2-state-machine`）。Owner：架構。程式：`engine/modules/state_machine/`（`GYO::Engine::StateMachine`，target `GYO::StateMachine`，登記檔一列 `state_machine`，`library_only=yes`、`default=no`）。行為測試：`tests/common/state_machine/`（ctest `state_machine.behaviour`、`state_machine.combination`，標籤 `cpu`）；突變清單 [`scripts/state_machine_mutations.json`](scripts/state_machine_mutations.json)（`sm-`）。

由來：[事前宣告](state-machine-declaration.zh-Hant.md)修訂 8（2026-11-22 動工前核准：「照建議，三件都核准，開始」；交叉比對後「照建議，1 保留，2 選 A＋，開始」）。遊戲看得到的行為照修訂 7（2026-11-16 核准）。**Design Invariants 的最終文字在合併前核准時定稿**。和宣告文字不同的地方在條末標「實作」。

## 一個例子

單機遊戲：啟動後是主選單，按「開始」進關卡，關卡中可以暫停、開背包。

```cpp
enum class Flow : std::uint8_t { MainMenu, InLevel };
enum class Pause : std::uint8_t { Running, Paused };

template <>
struct GYO::Engine::StateMachine::StateMachineTraits<Flow> {
    static constexpr std::string_view name = "Flow";
    static constexpr std::array values{ValueName<Flow>{Flow::MainMenu, "MainMenu"},
                                       ValueName<Flow>{Flow::InLevel, "InLevel"}};
    static constexpr Flow initial = Flow::MainMenu;
    using SubStates = GYO::Kernel::TypeList<Pause>;
};
template <>
struct GYO::Engine::StateMachine::StateMachineTraits<Pause> {
    static constexpr std::string_view name = "Pause";
    static constexpr std::array values{ValueName<Pause>{Pause::Running, "Running"},
                                       ValueName<Pause>{Pause::Paused, "Paused"}};
    static constexpr Pause initial = Pause::Running;
    using Parent = Flow;
    static constexpr Flow parentValue = Flow::InLevel;
};

// The product's own build:
StateMachine::RegisterStateMachine<Flow>(setup);  // Main, right after PreUpdate
r.AddSystem<&SpawnMenuButtons>({.name = "SpawnMenuButtons", .mounts = {{.schedule = "Flow.Enter.MainMenu", .stage = "Update"}}});
r.AddSystem<&MovePlayer>({.name = "MovePlayer", .mounts = {{.schedule = "Main", .stage = "Update",
                          .conditions = {StateMachine::InState<Pause, Pause::Running>()}}}});
// A System that asks: request->Request(Flow::InLevel);
```

按「開始」的那一幀：`Main/PreUpdate` 之後的 `Flow.Apply` 跑 `Flow.Exit.MainMenu`、改值、`Flow.Enter.InLevel`，然後暫停（子狀態）出現成 `Running`、跑 `Pause.Enter.Running`。之後 `Update` 裡帶 `InState<Pause, Running>` 的 System 照跑；要求暫停後同一幀起不跑。

## 1. 位置與依賴

```text
產品 ──→ 狀態機 ──→ 編排層（WorldSetup、預設骨架的名字、準備函式）──→ Kernel
```

- 樣板函式庫，沒有登記單位：狀態的列舉是產品的，Module 不知道有哪些型別。產品在自己的 `build` 呼叫 `RegisterStateMachine<S>`。
- 不依賴時間、權威、回滾；它們也不依賴它（DI 8、權威 DI-A 9、回滾 R-3）。
- 標頭：`Traits.hpp`（名字表、`RootState`／`SubState` 約束）、`StateMachine.hpp`（兩種 Resource）、`Names.hpp`（名字）、`Conditions.hpp`（`InState`、`NotInState`）、`RegisterStateMachine.hpp`（登記與套用的 System）。

## 2. 介面

| 東西 | 意思 |
|---|---|
| `StateMachineTraits<S>` | 作者對自己的列舉特化（成員名照命名規範用小駝峰；宣告寫的 `kParentValue`、`KeepOnParentReenter` 在程式裡是 `parentValue`、`keepOnParentReenter`）：`name`、`values`、`initial`；父另列 `SubStates`（登記順序）；子另寫 `Parent`、`parentValue`、`keepOnParentReenter`（選填，預設 false）。只做一層；寫錯編不過 |
| `StateMachine<S>` | 現在的值：`Status()`（`NotEntered`／`Absent`／`Active`）、`Current()`；`SetInitial(值)` 只在第一次套用之前有用（之後斷言） |
| `StateMachineRequest<S>` | `Request(值)`：下一次套用換到那個值（換成同一個值也跑離開與進入）；`Pending()`：待處理的要求；`Overwritten()`：被蓋掉的要求的累計數。`==` 比全部欄位（回滾「相等就不寫回」靠它）；雜湊不收計數 |
| `RegisterStateMachine<S>(setup, options)` | 登記父與每個子的兩種 Resource、每個值一張進入與離開的 schedule（各一個 stage `Update`）、自己的 stage `<S>.Apply` 與套用的 System；加一個準備函式放第一個值。`options`：`schedule`／`anchor`／`place`（預設 `Main`、`PreUpdate`、之後）、套用的 System 的 `conditions` 與 `sets` |
| `InState<S, V>()`、`NotInState<S, V>()` | run 條件；還沒進入、不存在時都是否 |
| 名字 | `StateMachine.<S>`、`StateMachineRequest.<S>`、`ApplyStateMachine.<S>`、`<S>.Apply`、`<S>.Enter.<值>`、`<S>.Exit.<值>`、`InState.<S>.<值>`、`NotInState.<S>.<值>` |

## Design Invariants

<a id="sm-di-1"></a>
**DI 1 封閉的型別**：一種狀態進入之後，作者的程式只能經要求改它的值：在這個 Module 的 `Detail` 之外，`StateMachine<S>` 沒有從 `S` 的值建構、沒有公開的預設建構、沒有 `SetInitial` 以外的改值成員，`SetInitial` 只在第一次套用之前有用；複製、搬移、比較公開（回滾與 `CarryOver` 放回的是之前存下的值）。作者能放回存過的值，造不出沒存過的值。`Detail` 裡的東西（`Detail::Access`）是 Module 自己用的，作者用它是誤用（命名 N13；Bevy 靠 `pub(crate)`）。守衛：`behaviour: the closed type…`；突變 `sm-constructible-from-value`、`sm-default-constructible`。「沒有公開的預設建構」是宣告沒寫明、實作補的（實作）。

<a id="sm-di-2"></a>
**DI 2 一次套用的順序**：取走要求（父與每個子一起取、一起驗；值不在名字表是 Err SM-E1，什麼都不跑）→ 子照登記的順序離開、每個一離開就記成不存在 → 父的離開（現在的值仍是舊的）→ 改值 → 父的進入 → 子照登記的順序設初始值、進入 → 子各自處理自己的要求。巢狀的 Err 往上傳，已經改的值不還原。守衛：`behaviour: changing state…`、`…children appear…`、`…children leave…`、`…two children's requests…`、`…a value not in the table…`、`…an exit or an enter failing…`。

<a id="sm-di-3"></a>
**DI 3 一次一個要求**：一次套用對每種狀態最多處理一個要求，要求在套用一開始就取走；離開或進入裡提出的要求留到下一次。同一次套用前的兩個要求後寫蓋前寫，被蓋掉的累計計數（World 存在期間不歸零；回滾還原時跟著還原）。子因為父換了而離開、重設、進入，不算處理子的要求。守衛：`behaviour: one request per state per apply…`、`…a request for another child…`、`…two requests before one apply…`、`…parent and child requests in one apply…`。

<a id="sm-di-4"></a>
**DI 4 第一次套用**：套用的 System 第一次執行時，一律先進初始值（父的，再父的初始值底下的子），在遊戲初始化之後（O-1）；初始值是特性的 `initial`，或第一次套用之前用 `SetInitial` 給的；之前提出的要求留到下一次；在那之前兩種條件都是否。每個 World 各自、每次建 World 各自。套用的 System 帶的條件擋每一次套用，第一次也算。守衛：`behaviour: initial enter…`、`…a request made in game initialization…`、`…a child's request in game initialization…`、`…conditions are false before the first apply`、`…an initial value given with SetInitial…`、`…loading a save…`、`…the apply System's conditions…`、`…the first values are put by the registration`。

<a id="sm-di-5"></a>
**DI 5 子狀態**：子只在父的指定值底下存在；不存在時兩種條件都是否、對它的要求丟掉、不留之前的值（兩個經歷不同的不存在的子相等，World 摘要與 digest 相同；實作）；每次重新出現從初始值開始——例外是第一次套用時作者用 `SetInitial` 給的值，只用一次（父的初始值底下不存在時丟掉；宣告第 2.2 節 (i)）；父換成同一個值時默認重設，`keepOnParentReenter` 時保留（只對同一個值）。守衛：`behaviour: children leave…`、`…a request for an absent child…`、`…the parent entered again with the same value…`、`…keepOnParentReenter…`、`…loading a save…`、`…an absent child is the same whatever value it had`。

<a id="sm-di-7"></a>
**DI 7 同一種狀態只在一處套用**：登記函式呼叫兩次是 Kernel 的 R-E2。作者另外登記套用的函式是誤用，不守。守衛：`combination: registering one state twice…`。（DI 6 `RequestSilent` 不在這一批：宣告第 2.1 節 (d)，和網路 Module 那一批一起定；網路宣告 Q8 已定 (b)，由後合併的一方加。）

<a id="sm-di-8"></a>
**DI 8 不碰時間**：不讀寫 `TimeControl` 與任何時間的 Resource，不依賴時間 Module。守衛：行為測試的 target `gyo_state_machine_tests` 只連 `GYO::StateMachine` 而編得過；`gyo_state_machine` 的連結集（`tests/common/state_machine/CMakeLists.txt` 的 `gyo_expect_links`：只有 Kernel 與編排層）。

<a id="sm-di-9"></a>
**DI 9 Kernel 與編排層不知道狀態機**：這個 Module 只用它們的公開介面。守衛：`kernel_guard.py` K5、`orchestration_guard.py` O4 的准許清單（既有）。

<a id="sm-di-10"></a>
**DI 10 名字**：名字固定由特性組出，同一組特性在兩端組出相同的名字（第 2 節的表）。權威的一致性指紋用 Resource 的登記名與 System 名。守衛：`behaviour: names come from the traits`；突變 `sm-schedule-name-no-state`、`sm-notinstate-named-instate`。

### Replacement Criteria

符合任一條時，停下來交使用者「修還是重建」。

1. (c) 區的狀態因為這個 Module 的形狀，出現「兩端不一致而 digest 看不到」的情形。
2. 一個點名的 Feature 需要同一種狀態在每幀與固定步兩處換。
3. 為了狀態機要在 Kernel 或編排層加型別、分支或介面（DI 9 被打破）。
4. Engine 的另一個 Module 需要讀狀態（例：Loading 或生成改成綁在狀態機上），而這個 Module 的形狀接不上。
5. 作者為了繞過「一次只處理一個要求」「進入在套用處跑」「第一次一律進初始值」，在兩處以上寫了同樣的補償（例：自己再跑一次套用、自己存「上一個狀態」）。
6. 子狀態要第二層，或一個子要掛在兩個父值底下，而這個形狀接不上。
7. 作者為了訊息的保留（第 3 節的三個後果），在兩處以上寫「一直跑的 System 讀、抄進自己的 Resource」，或為了後果 2 自己記「上次讀到哪」（重評定案為作者的事之後，這是要回頭看 Kernel 契約的訊號）。
8. 網路 Module 那一批的伺服器覆蓋、或讀檔，要一條「放回值、不跑進入」的路，而這個 Module 的型別封閉接不上。

## 3. 寫法與要小心的

- **套用的 System**：它是獨占的，和同一個 stage 的每個 System 都衝突，所以自己佔一個 stage；它用 `ExclusiveWorld<Writes<…>>` 標註父與每個子的兩種 Resource（伺服器權威策略靠這個看見它寫了什麼）。多個寫要求的 System 在同一個 stage 照登記的先後。
- **進入裡生成的實體**：在那張進入的同步點就套用；排在套用的 System 後面的 System 同一幀看得到。
- **掛在哪**：預設 `Main` 的 `PreUpdate` 之後（Bevy 的位置）。同一個錨點插兩棵樹時，後登記的緊貼錨點、先套用；兩棵互不相干的樹之間的先後不是承諾，要同一幀生效就自己選插入位置。
- **遊戲初始化**：初始的進入在遊戲初始化之後，讀得到它放的設定；遊戲初始化裡 `InState`／`NotInState` 一律是否（任何 World 都還沒進狀態）。套用的 System 不要掛在遊戲初始化清單的 schedule 上。
- **生成中**：套用的 System 預設沒有條件，第一幀就進初始值，即使 World 還在生成中——這時只帶 `InState` 的遊戲邏輯在生成中就會跑。要等生成完才進，給 `options.conditions = {Orchestration::AfterGenerating()}`。條件擋的是每一次套用：World 內重新生成期間，要求延到生成結束。
- **讀檔**：一律在新的 World。讀檔的 System 在生成中讀到存檔的狀態，寫 `ResMut<StateMachine<Flow>>` 的 `SetInitial(存檔的值)`（子也可以），套用的 System 帶 `AfterGenerating()`：第一次套用直接進存檔的值，不先進預設的初始值（Bevy `insert_state`）。發替換之前就知道的，也可以在 `ReplaceRequest.extra` 裡 `SetInitial`。不要 `CarryOver` 狀態機的兩種 Resource，也不要用存下的副本蓋掉：新 World 會以為已經進入過，不跑進入。
- **進關卡用新 World 還是同一個 World**：兩種都行。同一個 World：「進入關卡中」裡把 `WorldGeneration` 寫回生成中；那一輪開始時不是生成中，幀尾不判定，下一幀開頭 `held` 清掉，要靠帶 `WhileGenerating()` 的 System 每幀 `Hold()` 撐到載完。新 World：`ReplaceWorld`，從初始值開始，要直接在「關卡中」就 `SetInitial`。
- **進入／離開裡的訊息（三個後果）**：(1) 帶狀態條件的讀者進了別的狀態不再跑，但它跑過，所以拉住保留；寫滿上限後每次寫入都丟最舊、記一筆 `DroppedMessages`。(2) 它回到原狀態的那一幀，把期間累積的舊訊息一次讀到。(3) 進入那張裡的讀者第一次讀不到之前寫的，之後每次讀到上次進入以來的全部。要帶進「進入」的資料（例：選了哪一關）放 Resource；讀訊息的 System 不要帶狀態條件。事件游標重評已定案：這是作者的問題，Engine 只負責講清楚，不改 Kernel（使用者 2026-11-22，`v8-arch-gyo2-event-cursor-reeval-author-side`）；通用寫法在 [System 契約說明頁](system-contract-page.zh-Hant.md)（由事件游標重評那張卡寫）。
- **暫停要不要停時間**：作者決定，狀態機不碰 `TimeControl`。要停：「進入暫停」寫 `TimeControl` 暫停、「離開暫停」設回。注意：把 (b) 區的暫停掛在固定步、又照這樣寫，會永遠離不開暫停（暫停時固定步不跑，套用的 System 也不跑）。
- **(c) 區「兩端同規則」**（例：天氣）：`RegisterStateMachine<Weather>(setup, {.schedule = "FixedUpdate", .anchor = "First"})`；選了伺服器權威的產品用 `AuthorityEntry::Mark<StateMachine<Weather>>()`、`Mark<StateMachineRequest<Weather>>()`（依型別，進 digest）與 `Runs(ApplySystemName<Weather>(), {EnterScheduleName(Weather::Clear), ExitScheduleName(Weather::Clear), …})`（名字用函式組，不手寫字串）；有回滾的 Client 用 `RollbackEntry::Resource` 登記兩種 Resource、`NestedSchedule(每張進入與離開, "FixedUpdate")`。回滾重跑那一步時轉換重跑：改世界狀態的照常重跑，只該發生一次的副作用（音效、粒子）讀 `Resimulation` 擋（別家的 `is_in_rollback`、`IsFirstTimeFullyPredictingTick`）。「權威端換、Client 採用」在網路 Module 那一批。
- **不擋的**：套件插的 stage 別人可以掛進來；作者自己的獨占 System 可以直接跑「進入」（現在的值不會變）；用了這些，套用的規則不保證。

## 4. 本批的限界

- 三個 (b) 區的 Feature 只在一個 World 的組合下驗；模擬與畫面分兩個 World 時，暫停的要求怎麼傳是交換的資料（O-12）。
- 「權威端換、Client 採用」、`RequestSilent`、`Change` 參數：網路 Module 那一批（網路宣告 Q8 (b)）。
- `StateMachineRequest` 被回滾還原時，被蓋掉的計數跟著還原（`==` 比全部欄位）。雜湊不收計數：它是診斷，不算遊戲的狀態（宣告 5.2 第 4 列）。網路那一批若給回滾自訂的比較，要一起看計數要不要移出判定資料。
- 效能沒有量。

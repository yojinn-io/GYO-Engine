# 事前宣告：System 讀得到 Resource 被寫過（`v8-arch-gyo2-kernel-res-changed`）

更新：2026-11-23（修訂 1）。Owner：Kernel（GYO2 主會話開出的任務卡）。**狀態：草稿，待使用者核准。**

根據：使用者 2026-11-22 決定補（原話「開」）；網路卡（`gyo2-net-module`，PR #275，未合併）第 2 步查狀態機卡交叉比對 F3 時發現。

經過：
- 修訂 0：照卡片寫。送換模型的敵對式檢查（Fable／high，唯讀）：blocker 1、should 5、nit 8。報告：主 checkout `build/evidence/gyo2-kernel-res-changed/declaration-check-r1-fable.md`。檢查者沒有建置、沒有跑測試；observer 那一項（S2）是照程式推的。
- **修訂 1（這一份）**：照報告改。最大的一項（B1）：點名的 Feature「階段一變就換 UI」光靠「被寫過」做不到，因為計畫中的寫者（網路的預設套用、狀態機的套用）同值也寫；寫成第 3.1 節的決定點。其他：observer 不在承諾內（S2）、換掉錯的單機例子（S1）、跟上 `origin/gyo2`（#278、#282，S3）、補列 set 條件與拿掉時的 stamp（S4）、登記跨部件規則（S5）、nit 全收。

---

## 1. 這一批做什麼

### 1.1 用遊戲講

單機或連線都一樣：作者有一個 Resource `GameSettings`（音量、畫質），設定畫面改了它；另一個 System 只在設定改了的那一幀重建音訊混音與畫質參數。

- **現在**：這個 System 讀得到值，卻不知道值是不是剛被寫過，只能自己在 `Local` 裡存上一次的值、每幀比一次。Kernel 已經在每個 Resource 上打了「最後被寫的 stamp」，作者等於在外面重做一份。
- **之後**：`Res<GameSettings>` 多一個 `Changed()`：這個 System 上次跑之後，`GameSettings` 被寫過就是 true。和元件的 `Query<…, Changed<T>>` 同一套判定。

**計畫中**的連線用法（網路卡 PR #275、狀態機卡 PR #281，都還沒併入 `gyo2`）：伺服器把 `StateMachine<RoundPhase>` 從「進行中」換成「結算」，網路照 Q8 (b) 複製到 Client，用狀態機提供的套用函式換掉 Client 的值（`RequestSilent`，不走轉換事件，還沒有程式）；Client 的 UI System 讀 `Res<StateMachine<RoundPhase>>::Changed()` 換畫面。這條路要另外一個條件才成立，見 3.1。

### 1.2 用機制的詞

| 現在已有的 | 位置 |
|---|---|
| 元件的變更過濾 `Changed<T>`／`Added<T>`：比「這個掛載上次跑」的 stamp | `Params.hpp:85-107`、`:352-361` |
| 每個 Resource 一個 `changed` stamp，寫入就打 | `ResMut::Get`（`Params.hpp:427-433`）、`ExclusiveWorld::Modify`（`:1000-1007`）、同步點的 resource 那一步（`World.cpp:900-909`，`Commands`、當場改、`Restore` 都經這裡）、Host 與 System 經 World API 的 `SetResource`／`ResourceMut`（`World.hpp:247-270`）、配發器（`Detail::Allot`，`World.cpp:473`） |
| 掛載的上次跑 `lastRun`：run 條件求值前放進呼叫；跑了才更新，被跳過不動 | `Executor.cpp:321`、`:355`；`MountState::lastRun` 預設 0（`WorldExecutor.hpp:127`） |

缺的只有讀法：`Res`／`ResMut` 不記 `lastRun`，也沒有公開的比較。

**前提**（`premise-engine-or-author`）：這是 Engine 的問題。stamp 只在 Kernel 裡、作者拿不到；作者自己存上一次的值來比，是在 Kernel 外面重做一份已有的機制，而且判定變成「值變了」，和元件的 `Changed<T>` 不一致。

**為什麼算基盤**（AGENTS §2）：Kernel 的公開介面；之後網路、狀態機、產品的 System 都會用它的意思，意思一旦有人用就難改。所以「被寫過」還是「值變了」、條件裡比誰，在這一批定清楚（第 3 節）。不是大的責務，不加 Replacement Criteria。

## 2. 要做的（最小範圍）

### 2.1 API

```cpp
template <class T> class Res {
    [[nodiscard]] bool Changed() const;   // 新增
};
template <class T> class ResMut {
    [[nodiscard]] bool Changed() const;   // 新增
};
```

- 判定：`Present() && slot.changed > lastRun`。`lastRun` 是這次呼叫的 `call.lastRun`，和 `Query` 的 `Changed<T>` 拿的是同一個值（`Params.hpp:269`）。
- 不改值、不打 stamp，和 `Present()` 一樣先做句柄的檢查（W-4）。
- **名字**：`Changed()`，不用卡片寫的 `IsChanged()`。理由：句柄這一組的判斷都不帶 `Is`（`Res::Present()`、`Query::Contains()`），N15 只定大小寫；和查詢的 `Changed<T>` 同一個詞、同一個意思。Kernel 別處有 `IsFrozen`，但不在句柄這一組。限制：`Res`／`ResMut` 類別裡之後不能不加限定地寫 `Changed<T>`（clang 探針確認；使用者程式裡兩者可以並存）。（自己判，照 `name-style-consistent`。）

### 2.2 各種情形的行為（System 掛載）

| 情形 | `Changed()` | 和現有機制的對照 |
|---|---|---|
| 掛載第一次跑 | `lastRun` 是 0：Resource 有值、被寫過（stamp ≥ 1）就是 true | 同 `Changed<T>`；Bevy 的 System 第一次跑也看到全部已有的 Resource 是 changed |
| 之後一輪才登記的 System（DI-KS 26） | 同第一次跑 | `WorldExecutor.hpp:182-185` |
| 換 World（`CarryOver`） | 新 World 的掛載全部是第一次跑：帶過去的值都是 true | `Setup.hpp:287-295`；`orchestration-skeleton` 宣告已寫明元件同樣 |
| Host 在 Ready 時 `SetResource`／`ResourceMut`（含兩次 Run 之間） | 下一次跑的每個掛載都 true（`WriteStamp()` 是目前的 stamp＋1） | 同元件的 `Insert`／`GetMut`；J-30 守 stamp 序列 |
| `KernelSettings` | World 建立時放進的值**改成也打 stamp**（`WriteStamp()`，即 1），第一次跑是 true | 現在建構子只放值、stamp 留 0（`World.cpp:61`）。改一行比在 DI 裡寫例外好。檢查者查過：摘要、指紋、邊界快照、回滾 Module、測試與突變都不依賴它是 0 |
| 拿 `ResMut` 寫入（`Get()`、`*`、`->`） | 寫入就算，不比值。`ResMut::Read()` 不算 | 同 `Mut<T>`（W-11）；同 Bevy、flecs、Unity |
| 同一次呼叫裡自己剛寫 | `ResMut::Changed()` 在 `Get()` 之後是 true | 同 Bevy |
| 自己上次跑寫的 | 下一次是 false（寫的 stamp 等於自己的 `lastRun`） | 同 `Changed<T>`、同 Bevy |
| 同一個 Run 裡排在後面的 System 寫 | 這次看不到，下一次跑才看到 | 同 `Changed<T>` |
| `Commands::SetResource`／`RemoveResource` | 在同步點的 resource 那一步打那個同步點的 stamp；之後才跑的掛載看得到 | 同元件的延後寫 |
| `ExclusiveWorld::Modify<T>` | 閉包被呼叫就打 stamp（不管改沒改值） | 同 `ExclusiveWorld::Modify(entity, f)` |
| `ExclusiveWorld::SetResource` | 當場打這個句柄的 stamp | 同當場的 insert |
| 掛載被 run 條件跳過 | `lastRun` 不動，寫入累積到它真的跑那次 | 同 `Changed<T>`（J-52「a skipped mount keeps its last-run stamp」）；和說明頁第 15 節訊息的「回來補讀」是同一類後果，只是只有一個 bool |
| 同一個 System 掛兩處 | 各自的 `lastRun` | 同 `Changed<T>`、`Local`（B-12） |
| Resource 沒有值（從沒放、或被拿掉） | false | 同 `Changed<T>`：拿掉的格不符合；Bevy 沒值時拿不到 `Res` |
| 拿掉後又放回 | true（放回打 stamp） | — |
| run 條件裡讀 | 比的是**這個掛載**的 `lastRun`（條件在掛載之下求值，B-5）；set 的條件逐成員掛載求值，各用各的 | 和 Bevy 不同，見 3.2 |
| `Restore` 放回 | 呼叫者的 eq 判為不同的打放回那次的 stamp → 之後才跑的掛載 true；判為相同的不寫 → 不變。回滾 Module 對沒有 `==` 也沒給 eq 的型別一律當不同（`History.hpp:203-210`），那種 Resource 每次還原都是 true | 同元件（DI-KS 12）。系譜 Bevy 讀檔標寫入當下 |
| 重跑（回滾後一步一步再跑） | stamp 與 `lastRun` 都只往前走：`Changed()` 反映**實際執行的歷史**，不是 tick 的歷史 | 同 `Changed<T>`。R-2「stamp 除外」，見 3.5 |
| 巢狀跑的 schedule | 裡面的掛載各有各的 `lastRun` | 同 `Changed<T>` |

**不在承諾內**：
- **hook**：沒有 `lastRun`（0），`Changed()` 等於「有值、被寫過」。和 hook 裡的 `Changed<T>` 一樣（`World.cpp:537`），寫明，不建議用。
- **observer**：同步點裡 `Add<T>`／`Remove<T>` 的 observer 在 resource 那一步之前跑、和它共用同一個 stamp（`World.cpp:613`、`:630`、`:908`），所以同一個同步點（含一次 `Restore`）後面才寫的 Resource 它這次讀到舊值、下次也是 false——永遠看不到；同一個同步點第二次被叫起時，自己剛寫的 `ResMut::Changed()` 也是 false。元件的 `Changed<T>` 有同樣的洞。DI-KS 10 已寫「同一個同步點第二次起讀不到較早的變更，不對作者承諾」，這一批補上「之後的寫入」這一半，同樣不承諾、不修（修要改同步點給 observer 的 stamp，是另一件事）。
- **`ExtractRes<R>`**：還沒實作；之後若要加 `Changed()`，來源 World 的 stamp 和目的 World 的 `lastRun` 不可比，不能照抄。

### 2.3 文件、規則、測試

- `kernel.zh-Hant.md`：加 **DI-KS 28**（Resource 的 `Changed()` 和元件的 `Changed<T>` 同一套：比 System 掛載的上次跑；寫入就算、不比值；沒有值是 false；第一次跑看到全部有值且寫過的；hook、observer 不承諾）；DI-KS 10 的 observer 那句補「之後的寫入」；表頭的更新行。最終文字在合併前核准定稿。
- `v2/system-contract-page.zh-Hant.md`：第 3 節 `Res<T>`、`ResMut<T>` 那一列補 `Changed()`；第 15 節加一句和 `Changed()` 累積的對照；列出 **Engine 自己每幀或每步都寫、`Changed()` 幾乎恆為 true** 的 Resource（`RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`Tick`、生成中的 `WorldGeneration`、配發器）。
- `docs/cross-component-rules.md`：登記 **CCR-8**（內容依 3.1 的決定；owner Kernel DI-KS 28）。
- 行為測試（`tests/common/kernel/`，案名帶「DI-KS 28」）：第一次跑；寫在前／讀在後、下一次 false；寫在後、讀在前、下一次才看到；Host 在兩次 Run 之間寫（引 J-30）；`Read()` 不算、`Get()` 同值也算；自己寫的下一次 false、同一次 `Get()` 後 true；`Commands::SetResource`、`ExclusiveWorld::Modify`／`SetResource`；跳過的掛載累積；兩個掛載各自；巢狀；沒值、拿掉、拿掉再放；`KernelSettings` 第一次跑；`Restore` 不同值 true、相同值 false；run 條件比掛載的 `lastRun`。
- 突變：只加每一個抓得到別的抓不到的誤判的（§14.5），預計：比較寫成 `>=`、不看 `Present()`、`ResMut` 拿 `stamp_` 當基準、`KernelSettings` 建構子不打 stamp。新加的程式文字不讓既有突變的 `find` 變成多處命中（例：`mutations.json` 裡含 `slot.changed = stamp_;` 的兩筆）。合併前交換模型的檢查（§14.2）；只跑相關突變（`mutation-rerun-scope`）。
- dev_log、`v2/README.md` 的列表、`v2/HANDOFF.md`。

## 3. 要你決定的，與和別家不同的地方

比過：Bevy（`bevy_ecs/src/change_detection/traits.rs` 的 `DetectChanges`／`DetectChangesMut`；`schedule/condition.rs` 的 `resource_changed`、`resource_removed`）、flecs（`docs/Queries.md`「Change Detection」）、Unity Entities（`ComponentSystemBase.LastSystemVersion`、chunk 的 `DidChange`、`ComponentLookup<T>.DidChange(Entity, version)`）。三家都是「寫入就算、不比值」（flecs 連帶 `out`／`inout` 的走訪都算）；flecs、Unity 把單例當成某個實體上的元件，沒有單例句柄上的讀法，要走查詢的變更過濾或 `ComponentLookup`。

### 3.1 【決定】同步同一個值的寫者——「被寫過」不等於「值變了」

**問題**（檢查 B1）：`Changed()` 是「被寫過」（三家都是）。可是計畫中寫遊戲狀態的 Engine 寫者，同值也會寫：
- 網路 Q8 (b) 的預設套用是 `SetResource<T>`（網路宣告 `:637`），每次都打 stamp；「值走不可靠、只送最新的」，同一個值可能套用多次；HANDOFF `:117` 的例子就是「每收到一筆快照寫一次同一個值」。
- 狀態機 r8 的套用只要有要求就 `Modify`，`from == to` 也一樣。

結果：1.1 計畫中那個 UI System 每收到一筆快照都拿到 true，作者還是得在 `Local` 存舊值比。（未驗證的部分：兩邊的 Resource 套用都還沒有程式，是從宣告與 HANDOFF 推的。）回滾跨過一次階段變更時，還原與重跑各寫一次，步外的 UI 也會拿到一次 true——這一點三個選項都一樣，和 Bevy＋lightyear 一樣。

| 選項 | 做法 | 代價 |
|---|---|---|
| **(a) 建議** | Kernel 照三家：`Changed()` 是「被寫過」。從 Kernel 的契約推出的規則登記成 CCR-8：「**反覆同步同一個值的寫者，值相同就不寫**（比完再寫）」，經主會話轉給網路卡（預設套用：先讀、不同才 `SetResource`）與狀態機卡（silent 套用：同值不 `Modify`）。兩邊都還沒實作，現在轉成本最低 | 每個這種寫者都要比一次；要求 `T` 能比（沒有 `==` 的型別由寫者給比法） |
| (b) | (a)＋Kernel 給「不同才寫」的入口（Bevy 的 `set_if_neq`）：`ResMut`、`ExclusiveWorld` 各一個 | Kernel 多 API；網路的預設套用是 `SetResource<T>`、不經 `ResMut`，入口要加在它用的那條路上才有用；作者用 `ResMut::Read()` 本來就一行寫得出來 |
| (c) | `Changed()` 比值：Kernel 每個 Resource 留一份上次的值 | 三家都不這樣；記憶體、每型別要 `==`；和元件的 `Changed<T>` 不一致 |

建議 (a) 的理由：修在源頭（同值寫入的那個寫者），不在讀的那一頭補（`fix-source-not-patch`）；和三家一致；Kernel 不多東西。

### 3.2 【告知】run 條件裡的 `Changed()` 比掛載的上次跑

- Bevy：run 條件本身是一個 System，有自己的 `last_run`；`resource_changed` 是「這個**條件**上次被檢查之後」。已知的缺點：條件為 true、但同一個掛載的另一個條件為 false，System 沒跑，變更就錯過了。set 的條件每次 schedule 跑只求值一次。
- GYO 現在：條件在掛載之下求值（B-5），呼叫裡的 `lastRun` 是掛載的；元件的 `Changed<T>` 在條件裡也是這樣。被別的條件擋下時，變更留到 System 真的跑那次。set 的條件每個成員掛載各求值一次、用各自的 `lastRun`（`Executor.cpp:344-348`，J-29），所以用 `Changed()` 當 set 的條件時，成員不保證同進同出。
- **建議：照現狀，寫明，不另做。** `MessageReader` 在條件裡有自己的游標（DI-KS 10、說明頁第 15 節、CCR-7），和 stamp 不同；這是現有的差別，這一批不動。

### 3.3 【確認】這幾個 Bevy 有、這一批不做

| Bevy 有的 | 不做的理由 | 之後要加 |
|---|---|---|
| `is_added` | Resource 只有 `changed` 一個 stamp；沒有點名的 Feature 要分辨「剛放進去」和「被改」，`Changed()` 已包含剛放進去 | 局部：`ResourceBase` 多一欄、三處放值的地方打 |
| `set_if_neq`、`replace_if_neq` | 見 3.1 (b) | 3.1 選 (b) 就做 |
| `bypass_change_detection`（寫但不打 stamp） | 網路的 `RequestSilent` 正是要讓讀者看到被換過，不能繞過 | — |
| `last_changed`、`World::is_resource_changed` | 一般 System 不需要 stamp 本身；`ExclusiveWorld::ResourceStamp<T>()` 已給回滾 Module；獨占 System 讀「被寫過」沒有點名的 Feature | — |
| `resource_removed` 條件 | 沒有點名的 Feature。Bevy 的做法是 `Local<bool>` 記上次有沒有值，作者照寫得出來；但 `Local` 在步的 schedule 上違反 R-2 前提 2。拿掉時的 stamp 目前不一致：同步點、當場改、`Restore` 都打，Host 的 `World::RemoveResource`（`World.hpp:273-279`）不打；`Changed()` 先看 `Present()`，所以不受影響，這一批不動 | 要做時先讓拿掉都打 stamp，再給 `Removed()` |

### 3.4 回滾的重跑（告知，不是這一批的決定）

`Changed()` 讀的是 stamp，stamp 不退回，所以步的 schedule 上「判定結果取決於 `Changed()`」的 System，第一次跑和重跑可能不同。R-2 的等價本來就「stamp 除外」、前提 1 只准讀可回滾的資料，`Changed<T>` 現在也是這樣，不破壞現有的契約。要不要在回滾頁 R-2 前提 1 明寫「`Changed<T>`、`Res::Changed()` 讀的是 stamp，不在前提內」，是回滾 Module 的事，這一批不改，回報給主會話。

### 3.5 既有文字（回報，不改）

`time-events` 宣告 5.1 第 3 項寫「讀 `VirtualTime.paused`，要抓變化用 `Changed<…>`」。`VirtualTime` 每幀被 `AdvanceTime` 寫（`WorldTimeModule.cpp:47-51`），`Changed()` 恆為 true，這句對 `VirtualTime` 不成立；能用的是作者寫的 `TimeControl`（作者也要不同才寫）。宣告是當時的紀錄，不改，回報主會話；說明頁的「恆為 true」清單會蓋到。

## 4. Architecture Delta（AGENTS §3）

### 4.1 七項

1. **Feature**：單機與連線的產品都用——設定改了才重建（1.1）、伺服器換掉的狀態讓 Client 的 UI、音效跟著變（計畫中，網路 Q8 (b)、狀態機，要 3.1 的條件）。
2. **問題**：stamp 已經在、寫入也打，但讀不到；作者只能在 Kernel 外重做。
3. **邊界**：Kernel 的公開介面多兩個唯讀的成員函式；DI-KS 加一條、DI-KS 10 補一句；跨部件規則多 CCR-8。
4. **影響**：`engine/ecs/kernel`（`Params.hpp`、`World.cpp` 一行）；測試、文件。不改任何 Module、編排層、Product 的程式；3.1 選 (a) 時，網路與狀態機的套用函式多一條要求（經主會話轉）。
5. **依賴方向**：不變。
6. **Ownership**：stamp 仍只由 Kernel 打、只在 Kernel 裡；讀法是 Kernel 的。不新增任何狀態的寫者或副本（State Authority 不變）。CCR-8 的 owner 是 Kernel（DI-KS 28）。
7. **為什麼不能更小**：已經是最小——只公開既有 stamp 與既有 `lastRun` 的比較。

### 4.2 選項的系譜（AGENTS §4）

| 選項 | 系譜 | 需要它的 Feature 與 Product | 結果 |
|---|---|---|---|
| A. `Res::Changed()`／`ResMut::Changed()`，比掛載的上次跑 | Bevy `Res::is_changed`；GYO 自己的 `Changed<T>` | 設定改了才重建（任何產品）；Client 跟著伺服器換的狀態變（連線產品） | **採用** |
| B. 參數層過濾（例如 `ResChanged<T>` 參數，沒變就不跑） | Bevy 的 `resource_changed` run 條件 | 同上，但只是寫法 | 不採：卡片說 run 條件等點名的 Feature；作者用 A 寫一個條件就有 |
| C. 作者自己存上一次的值來比 | — | — | 不採：在 Kernel 外重做 Kernel 的機制、判定變成「值變了」 |
| D. `KernelSettings` 留 stamp 0，在 DI 裡寫例外 | — | — | 不採：為一個例外多一句規則；改建構子一行就一致 |

決定的根據是 Feature 與「和元件同一套判定」，不是改動少。同值寫入的三個選項見 3.1。

## 5. 範圍外

- 3.3 表裡的各項；3.4 的回滾頁補句；observer 那個洞的修正（2.2）。
- 不為防誤用加守衛（`no-guards-against-misuse`）。

## 6. 檢查與核准

- 宣告交出前：換模型的敵對式檢查（Fable／high，唯讀）——已做一輪（修訂 0 → 1）。
- 動工：使用者說「開始」（含 3.1 的選擇）。
- 合併前：Fable／high 唯讀的設計審查（含 DI-KS 28 守衛測試的突變列表，§14.2）；PR base `gyo2`，等使用者明說「合併」。
- 證據：主 checkout `build/evidence/gyo2-kernel-res-changed/`。

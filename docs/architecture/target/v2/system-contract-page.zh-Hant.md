# 作者怎麼寫一個 System（一頁；現況）

更新：2026-11-20（第 1、3、4、5、7、9、10 節、第 11.1、11.4、11.6 節：`v8-arch-gyo2-time-events`——Kernel 沒有時鐘與 tick，事件分保留式 `Message` 與推送式 `Event`；只改被拿掉的東西）；2026-11-18（第 11.5 節、第 12 節、新第 13 節：`v8-arch-gyo2-orchestration-skeleton`——開局與登記單位搬到編排層，加編排層一節）；2026-11-18（第 11.6 節：同一批修正——配發器的 id 還活著時 `Spawn` 不回 id、例外離開當場改讓 World 中斷）；2026-11-18（第 3 節、第 5 節、第 6 節、第 9 節、第 11.2、11.4、11.6 節：`v8-arch-gyo2-kernel-subtract-structure` 第 3 段——`ExclusiveWorld` 當場改成員與增刪 Resource、`Commands` 增刪 Resource）；2026-11-18（第 3～6 節、第 9～11 節：同一批第 1 段——歧義不報錯、執行期不攔 untyped 寫入、`ExclusiveWorld` 讀寫任何值、`KernelSettings` 可讀寫、綁執行緒的 Resource）；2026-11-18（第 6 節末、第 12 節：同一批第 2 段——兩步登記、之後再登記、動態插入的宣告）；2026-11-16（第 3 節 `ExclusiveWorld` 那一列、第 11 節：K0 `v8-arch-gyo2-nested-schedule`）；2026-11-14。Owner：架構（GYO2 重建會話）。**狀態：給使用者確認 System 契約用的說明頁。寫的是 `gyo2` 上已經能跑的程式（機制的骨架在 PR #225 併入；第 8 節的初始值宣告在分支 `claude/gyo2-funnel`），不是新設計。** 例子取自 `tests/common/kernel/SkeletonTests.cpp`。

**本頁例子裡的型別（`Position`、`Velocity`、`Health`、`Arrival`、`Hit`、`Stats` 等）都是範例產品自己定義的，Engine 裡沒有。** Engine 提供的是 `Query`、`Res`、`Spawn`、`Registrar::InitialValue` 這些泛型的機制；元件叫什麼、有哪些、初始值怎麼算，都由產品決定。

## 1. 一個 System 長這樣

```cpp
// 每個有 Position 與 Velocity 的實體：位置加上速度。
SystemResult Move(Query<Write<Position>, Read<Velocity>> bodies) {
    bodies.ForEach([](EntityId, Mut<Position> position, const Velocity& velocity) {
        position->x += velocity.dx;
        position->y += velocity.dy;
    });
    return Ok();
}

// 讀上次之後送來的 Hit 訊息，扣血，順便記一筆統計。
SystemResult ApplyHits(MessageReader<Hit> hits, Query<Write<Health>> healths, ResMut<Stats> stats) {
    for (const Hit* hit : hits.Read()) {
        if (Mut<Health> health = healths.GetMut<Health>(hit->target)) {
            health->hp -= hit->amount;
            ++stats->hits;
        }
    }
    return Ok();
}
```

登記：

```cpp
r.AddSystem<&Move>({.name = "Move", .mounts = {At("Tick", "Simulate")}});
r.AddSystem<&ApplyHits>({.name = "ApplyHits", .mounts = {At("Tick", "Simulate", {}, {"HurtBelowFloor"})}});
```

**參數就是存取宣告。** 作者不另外寫「我讀什麼、寫什麼」；機制從參數的型別推出來。

## 2. 規則

- System 是普通函式，或沒有捕獲的 lambda。成員函式、有捕獲的 lambda、泛型 lambda 都編不過。
- 回傳 `SystemResult`（`Result<void, KernelError>`）。正常結束回 `Ok()`；回 `Err` 會讓這次 Run 停下。
- 參數只能是下表的型別，以值接收，最多 16 個。
- 同一個 System 不能對同一個東西又讀又寫，或寫兩次。
- 違反上面任何一條都是編譯錯誤，每一種有自己的訊息。

## 3. 參數一覽

| 參數 | 用來做什麼 | 算什麼存取 |
|---|---|---|
| `Query<Read<T>, Write<U>, With<V>, Without<W>, Changed<X>, Added<Y>>` | 依 id 由小到大走過符合的實體；`Read` 拿到 `const T&`，`Write` 拿到 `Mut<U>` | 讀 T、寫 U；其餘是篩選 |
| `Res<T>`、`ResMut<T>` | 讀、寫一個 Resource | 讀、寫 T |
| `MessageReader<M>`、`MessageWriter<M>` | 讀（照這個讀者自己的游標，上次之後寫的全部）、送保留式訊息 | 讀、寫 M |
| `Spawn<Bundle<A, B…>>` | 生成實體，當場拿到 id；下一個同步點才看得到 | 寫配發器；延後寫 A、B… |
| `SpawnWithId<Bundle<…>>` | 用指定的 id 生成（讀檔、連線） | 延後寫 |
| `Despawn` | 刪除任何實體，下一個同步點生效（`Entity(id)` 不回傳東西） | 不算寫者 |
| `CommandsFor<Inserts<…>, Removes<…>>` | 延後加上或移除指定的元件 | 延後寫 |
| `Commands` | 延後加減任何元件，型別在函式體裡才決定（`Insert`、`Remove` 不回傳東西）；延後放進或拿掉 Resource（`SetResource`、`RemoveResource`，在同步點的 resource 那一步套用） | 無：不進衝突、不算寫者，執行期也不檢查 |
| `Local<T>` | 這個 System 自己的狀態，跨次保留 | 無 |
| `ExclusiveWorld<>`、`ExclusiveWorld<Writes<R…>>` | 在閉包裡讀寫整個 World（`Read<T>`、`Modify<T>`）、當場生成／插入／拿掉／刪除實體與增刪 Resource（第 11.6 節）、跑別的 schedule（`RunSchedule`）；必須是唯一的參數；只能是 (b) 區（第 11 節） | 讀寫所有元件與 Resource 的值（`Message` 不在內；可以當場 `Trigger` 推送式事件）；`Writes<R…>` 是選填的標註，不算寫者 |
| `Extract<Read<T>>`、`ExtractRes<T>` | 在 extract 的 schedule 裡讀來源 World；沒有寫的版本 | 讀來源 |
| `SetRead<Tag>`、`DiagnosticsWriter<R>` | 讀一組具名的元件；寫診斷紀錄 | 讀；無 |

## 4. 登記時另外寫的

- **名字**。
- **掛在哪裡**：哪個 schedule 的哪個 stage。schedule 與 stage 的名字全由產品自己給。
- **先後**（可選）：`before`、`after` 寫別的 System 的名字。
- **run 條件**（可選）：一個回傳 `bool`、只讀不寫的函式。
- **外部副作用**（可選）。
- Resource 可以標 **`threadBound`**（綁執行緒）：值放進去的那條執行緒以外碰它就是斷言（第 5 節）。
- **不必寫**：決定性區、role、rollback。不填就是一般的資料。

資料的登記只要名字。例外：元件預設只准一個 System 寫，要多個寫者得標 `Shared`。（訊息不再寫它屬於哪個時鐘，`v8-arch-gyo2-time-events`；可以另給上限 `capacity`，預設 1024。）Kernel 沒有 tick：要知道第幾個固定步，讀時間 Module 的 `Res<Tick>`（[world-time](world-time.zh-Hant.md)）。

元件還可以多寫一行**初始值的宣告**（第 8 節）：`r.InitialValue<Position, &PositionOf>();`——「`Position` 的第一個值從 `Arrival` 這個請求、用 `PositionOf` 算」。生成者只生成請求，機制在同步點把元件補上。

## 5. 機制替作者做的

- **排順序**：照 `before`／`after`，不照登記的先後。兩個 System 寫同一個東西又沒寫先後：依登記的先後，並列在一張清單裡（`FrozenRegistry::TieBreaks`，每一對帶 schedule 名）；不論決定性區，都不是錯誤。結果要不受登記順序影響，就自己寫 `before`／`after`（使用者 2026-11-16：「偵測歧義其實是很麻煩的。我個人覺得沒必要。」；嚴格模式 `RequireExplicitOrder` 一起拿掉）。
- **綁執行緒的 Resource**：標 `threadBound` 的 Resource 記下值放進去的那條執行緒；別的執行緒用 `Res`／`ResMut`、`ExclusiveWorld`、World API 碰它，蓋寫、拿掉它，讀 World 摘要，或在值還在時銷毀 World，都是斷言。拿掉值之後換執行緒放回可以。哪條執行緒跑哪張 schedule 由組裝的人安排，啟動時不再檢查（使用者 2026-11-16：「別人掉過的坑，我沒必要在跳一次。」；照 Bevy 的 `NonSend`）。
- **同步點**：每個 stage 結束時套用延後的變更。在前一個 stage 生成的實體，下一個 stage 就看得到。
- **訊息**：每個讀者有自己的游標，讀到上次之後寫的全部；任何 schedule 都能讀寫，被跳過的讀者下次補讀（上限內不漏）。推送式事件在觸發時跑 observer。全文在 kernel [DI-KS 10](../kernel.zh-Hant.md#di-ks-10)。
- **hook**：元件被加上或移除時，在同步點呼叫登記的函式（`ExclusiveWorld` 當場改時當場呼叫，第 11.6 節）。hook 記的變更什麼時候生效，見第 9 節。
- **啟動檢查**：順序成環、獨占的元件有第二個寫者、用到沒登記的型別，啟動時一次全部報出來。
- **出錯**：見下一節。

## 6. 錯誤怎麼回報

機制裡的錯誤只有兩種，一個事件只屬於其中一種：

| 種類 | 用在哪裡 | 發生時 |
|---|---|---|
| `GYO_ASSERT` | 便宜的前置條件與內部不變量 | 每一種建置都檢查；失敗就終止程式 |
| `Result` 的 `Err` | 程式有 bug（Programmer Error），而且要留下它一路經過哪裡 | 帶著上下文一路回傳，由 Executor 處理一次 |

`Result<void, KernelError>` 是 System 的**回傳契約**，不是把錯誤送去別處的通道：每個 System 都回傳它，`Ok` 是正常的路。

生成被機制丟掉（往活著的編號生成等）不屬於這兩種：它是一種**結果**，和「插入的目標實體已經不在、丟掉並計數」同一類，`Run` 照常；怎麼查見第 10 節。

**什麼該回 `Err`，什麼不該**（使用者 2026-11-11 定的定位，2026-11-14 再次確認）：
- **回到 Executor 的 `Err` 代表 bug。** 用 `Result` 而不直接斷言，不是因為可以恢復，是為了留下清楚的路徑；退到最外層，它仍然是 Programmer Error。
- **執行時的狀況不回 `Err`。** 檔案不在、資料格式不對、對方斷線，這些是程式要處理的正常情形：System 自己處理（記成狀態、發事件、顯示訊息），然後回 `Ok`。讀檔失敗就回 `Err`，會讓整個 World 停下。

**System 裡怎麼寫**：呼叫的東西回了 `Err`，就包一層自己的上下文再回傳。System 自己不記錄、不印。

```cpp
SystemResult SpawnsWave(Spawn<Bundle<Position, Velocity, Health>> spawn) {
    for (int member = 0; member < 3; ++member) {
        auto made = spawn.New(Position{0, 0}, Velocity{0, 0}, Health{1});
        if (!made) {
            return GYO::Engine::Base::Err(std::move(made).error().Wrap("SpawnsWave: wave member " + std::to_string(member)));
        }
    }
    return Ok();
}
```

**Executor 怎麼處理**，只做一次：

1. 把完整的上下文鏈交給產品登記的 handler（`WorldExecutor::SetErrorHandler`）。要不要寫日誌、寫到哪裡，由 handler 決定。
2. World 進入中斷狀態（`Interrupted`），這次 Run 結束，回傳同一個 `Err`。
3. 這次呼叫已經記下、還沒套用的延後變更全部丟掉，不會套用一半。
4. 之後再對這個 World 呼叫 Run 是斷言，直到重建 World。理由：跑到一半的狀態，不能被下一個 tick 當成完整的狀態用。這對三類產品都成立。

**World 中斷之後仍然可以讀**，只是不能再寫、不能再跑。工具要救還沒存檔的內容，就在這時候把資料讀出來另存：存成另一個檔，不蓋掉原檔，因為讀到的可能是改到一半的狀態。

上面那個 System 在配發器只剩兩個 id 時，handler 收到的內容是（骨架測試實際印出的）：

```text
AllotterExhausted [W-9]: the entity allotter's range is used up world=test-world
  at System 'SpawnsWave' (schedule 'Tick', step 0)
  at SpawnsWave: wave member 2
  at Executor: step 0 'SpawnsWave' of schedule 'Tick'
```

**會產生 `Err` 的來源**：System 自己回傳的；hook 回傳的；機制自己查到的（配發器用完或沒有宣告、存取驗證抓到沒宣告的存取、巢狀跑被拒絕、當場改被拒絕）。執行期不再攔 untyped `Commands`、`Despawn`、`ExclusiveWorld` 對判定資料的寫入（使用者 2026-11-17：「拿掉執行期的攔截，伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。」；網路 Module 之前這段沒有東西抓）。

**兩個開關**，在 `KernelSettings` 這個 Resource 裡。產品、System（`Res`／`ResMut`、run 條件、hook、`ExclusiveWorld`）都可以讀寫；機制在每次 `Run` 開始時讀一次，所以改了從**下一次 `Run`** 生效（巢狀跑的 schedule 不重讀）。拿掉它就用預設值。
- **存取驗證**：開著時，機制比對每個 System 實際碰了什麼和它宣告了什麼，不合就是 `Err`。成本隨 World 的大小增加，所以可以關；關掉時這一類不會被查到。
- **`breakOnKernelError`**：`Err` 產生的當下呼叫一個會返回的掛鉤，讓除錯器停在現場；之後照常走 `Err`。

`KernelSettings` 還有一個不是開關的欄 `hookRoundLimit`：hook 連鎖的上限的產品預設（第 9 節）。

**啟動時的錯誤**是另一條路：`Registry::BuildPlans` 把所有問題收齊，一次回傳一份 `StartupReport`，每一條寫明是誰、在哪裡登記的；經編排層時再和編排層、登記單位的錯誤收成一份 `ProductStartupReport`（第 13 節），產品決定怎麼印、要不要啟動。遊戲跑起來之後再登記的那一輪失敗，只是那一輪丟掉、World 照跑（第 12 節）。

**這些是誰定的**（使用者 2026-11-11、11-12，經側邊會話轉述；規格 K.3 第 17 項、§讀法、E.5；決定表的 `v8-arch-kernel-result-errors`、`v8-arch-kernel-verification-not-assert`、`v8-arch-kernel-break-hook`）：
- 不採「常開的檢查巨集」，改採 Go 式的錯誤處理，原則是不靜默。理由：相對於斷言的直接終止，`Result` 給得出上下文鏈，也讓 World 停在已知的狀態。
- System 包一層上下文回傳、不自己記錄；Executor 最外層只處理一次。使用者接受每個 System 多一行回傳。
- 存取核對不叫斷言，叫「驗證」，是可以關的執行期開關。
- 一個事件只有一種性質：`Err` 的產生與回傳路上不呼叫斷言的 handler，不呼叫任何終止出口。

## 7. 寫樣本時實際碰到的

1. **生成實體的 System 也算那些元件的寫者。** 「一個 System 生成、另一個更新」的元件，在預設獨占之下是啟動錯誤。使用者 2026-11-14 定：數法不改，這種情形由作者用漏斗處理，Engine 提供漏斗的工具（第 8 節）。工具的形式也定了：登記元件時宣告「它的初始值從哪個請求、用哪個函式算」，機制在同一個同步點補上。工具已實作（`Registrar::InitialValue`）；樣本的 Position、Velocity、Health 回到預設的獨占。
2. ~~**事件一定要指定時鐘**，而且只能在那個時鐘的 schedule 裡讀寫。~~ 拿掉（2026-11-20，`v8-arch-gyo2-time-events`）：事件不屬於任何時鐘。
3. **System 沒有捕獲**，所以要記東西只有兩條路：`Local<T>`（自己的），或 Resource（共用的）。
4. **`Query` 的 `Count()` 數的是「符合這個查詢的實體有幾個」，會真的走一遍**，不是讀某個容器的大小。它只有這一種行為，任何查詢都一樣。（2026-11-14 曾改成「一個元件時直接讀大小」，使用者確認那是誤會後要求完全改回：「一個函數，不能有 2 種不同的行為。」）
5. **同一個同步點裡，同類的變更排在一起執行，不照記錄的先後。** 順序固定是：刪除實體→移除元件→生成實體→加上元件，每一類裡依 id。所以有兩個和直覺相反的結果（骨架測試驗過）：
   - 同一個 System 裡**先生成再刪除**同一個新實體：實體活下來。因為刪除先執行，那時實體還不存在。
   - 同一個 System 裡**先加上再移除**同一個元件：元件還在。因為移除先執行，之後才加上。

   這個順序是使用者 2026-11-11 問清楚後定的（`v8-arch-gyo2-sync-order-keep` 有原話）：兩個 System 之間的結果因此不隨登記的先後改變；刪除排在生成前面，是為了同一個同步點裡用同一個 id 重新加入。要「這個 tick 不要了」的作者，自己不要記那筆生成或加上。

6. **每個有 `Spawn` 參數的 System 都算配發器（allotter）那個 Resource 的寫者。** 兩個以上的 System 會生成實體時，配發器要標 `Shared`，否則啟動時報「獨占但有兩個寫者」。寫漏斗的測試時碰到的；這是現況，沒有改。

7. **打開嚴格模式之後，有三種情形一定要寫順序**：兩個生成實體的 System（都寫配發器）、兩個寫診斷的 System、`ExclusiveWorld` 的 System 和同一個 stage 的其他 System。hook 不在嚴格模式的範圍內（hook 沒有 `before`／`after`）。

## 8. 「生成者算寫者」：歷史與決定

**決定**（使用者 2026-11-14，`v8-arch-gyo2-spawn-writer-funnel`）：「Despawn 是機制，不能當成普通的實作參考。選 3 項，讓作者自己做處理，但是 Engine 提供漏斗工具支持。」
- 寫者的數法不改：生成的 bundle 照舊算寫者。
- 「一個 System 生成、另一個更新」的元件，由作者用漏斗：生成者只放一個請求，由那個元件唯一的寫者補上真正的元件。
- Engine 提供漏斗的工具。形式（使用者同日：「漏斗用選項 A」）：照 Bevy 的 required components 調整——登記元件時宣告它的初始值從哪個請求、用哪個函式算；生成者只生成請求，機制在請求出現的同一個同步點把元件補上。寫者的數法不動。作者想自己決定寫入順序時不用這個工具：多個 System 照指定的先後寫，就把元件標 Shared 並寫 before／after；維持一個寫者但自己排來源的先後，就由那個寫者在函式裡讀請求或事件自己套用。已實作，用法與規則見下。
- 「比照 Despawn，生成也不算寫者」被否決：Despawn 不算寫者是機制自己的規定，不能拿來類比一般的實作。

**用法**（`Registrar::InitialValue`，2026-11-14 實作；測試 `tests/common/kernel/InitialValueTests.cpp`）：

```cpp
struct Arrival { std::int32_t x, y, dx, dy, hp; };                 // 請求：生成者只生成這個
Position PositionOf(const Arrival& a) { return Position{a.x, a.y}; }  // 純函式：請求 → 初始值

r.Component<Position>({.name = "Position"});   // 預設獨占；唯一的寫者是 Move
r.Component<Arrival>({.name = "Arrival"});
r.InitialValue<Position, &PositionOf>();

SystemResult Spawner(Spawn<Bundle<Arrival>> spawn) { ... spawn.New(Arrival{0, 10, 1, 0, 2}); ... }
```

它不是作者在 System 裡呼叫的 helper，是登記時的宣告，和 hook 同一類：作者宣告規則，機制在同步點執行。規則：

- **什麼時候補**：請求的元件在同步點「新出現」在一個實體上時（生成的 bundle 裡有它，或加到原本沒有它的實體上）。補在加上元件之後、OnAdd 的 hook 之前，所以下一個 stage 的 System 看得到，`Added<T>` 與 OnAdd 也照常。
- **已經有的不蓋**：實體已經有那個元件（例如 bundle 裡自己放了），就不動它。
- **覆寫請求不重補**：對已經有請求的實體再寫一次請求，不會重算元件。
- **請求留在實體上**，機制不替作者移除。
- **可以串**：`Position` 從 `Arrival`、`Bounds` 從 `Position`，在同一個同步點補完。
- **多個請求給同一個元件**：一個實體在同一輪得到兩個請求時，用先登記的那個請求元件的值（ComponentId 較小者）。經串接才出現的請求排在下一輪，所以直接給的請求先於串接出來的（2026-11-14 檢查後更正：原本只寫「取 ComponentId 小的」，跨輪時不成立）。
- **只有元件的擁有者能宣告**：宣告的 plugin 必須是登記那個元件的 plugin；請求可以是別的 plugin 的元件。
- **決定性區**（PR #229，2026-11-15 併入）：只有一條原則——**插入或生成請求的 System 或 hook，在區的檢查裡當作也寫了它補出來的元件**（串接的一路往下算），然後照原本的區規則檢查。直接寫不行的，經請求也不行（啟動錯誤 D-E7：Z1、Z2、Z3、Z6）；錯誤會點名那個 System 或 hook，並寫出元件是從哪個請求補的。
  - 就地改請求的值（`Write<T>`）、或只是移除請求，不算：這兩種都不會觸發補值。
  - 寫者的計數不受影響：補出來的元件不算那個 System 的寫入。
  - 宣告本身另有一條：(c) 區的元件只能從 (c) 區的請求取值（Z1），對 role=Input 的元件也一樣。
  - 沒填區的產品不受影響。
- **指紋**（PR #229）：宣告算進 Registry 指紋（格式 2）；目標在 (c) 區的宣告算進 C 指紋（格式 3）。
- **(c) 區的原始檔清單**（PR #233）：(c) 區元件的初始值宣告所在的檔案會列進去，讓決定性的掃描看得到。
- **Host 直接寫 World 的 API 不補**（`World::Insert`），和 hook 一樣只在同步點。
- **寫者的數法沒有改**：補值的是機制，不是 System，不進寫者的計數；把元件直接放進 bundle 的 System 照舊算寫者。

**需要產品測試**（使用者 2026-11-15：先合併，但標記為需要產品測試；紀錄在 HANDOFF）：上面的區規則還沒有用真實的產品資料驗證過。gyo2 的底層機制還沒成形，舊架構的產品也不改，所以現在只能推定。

| | 內容 |
|---|---|
| 實測過的 | 補值的執行（同步點的第 5a 步）：單機樣本 `SkeletonTests` 跑 8 個 tick，和手算一致。區的檢查與指紋只在啟動時作用，不改變執行結果 |
| 推定的（待產品驗證） | 移除或就地寫請求不算寫了元件；(c) 區 System 的請求不能補 (b) 區的元件（Z1）；(c) 區的 hook 不能經漏斗補 Derived 的元件；宣告的 Z1 對 role=Input 的元件也適用 |
| 只記錄的 | 用不帶型別的 `Commands` 插入一個 `Shared` 的請求，可以讓機制補出單一寫者的元件。「不帶型別的寫入碰不到單一寫者的欄」字面上不再成立；這和「補值的是機制、不算寫者」一致 |
| 什麼時候回頭驗 | 第一個在 (c) 區、開回滾、用到漏斗的真實功能進 gyo2 時。正當的寫法被擋下，先懷疑規則。要放寬只需要改 `Registry.cpp` 的 `FunnelWrites` 一處 |

**歷史**：

- 元件預設獨占、寫者以 System 計、Startup 的 schedule 不計：使用者 2026-11-11 討論後定的（規格 K.3 第 3、9 項；B.3、B.3.1）。
- 「生成的 bundle 也算寫者」：不是那次討論定的，是規格對抗檢查的一條修訂（B3，架構師接受）。理由：生成改成型別寫在簽名上之後，bundle 裡的元件才進得了存取集合，否則函式體裡可以生成任何元件，繞過寫者的計數與區的規則。
- 「一個 System 在執行中生成、另一個更新同一個元件」該怎麼寫：規格、目標文件、決定表都沒有談到。規格舉的例子只有「Movement 寫 Velocity、Integrate 寫 Position」，沒有說誰生成它們。
- 同一份規格對刪除的處理是相反的：`Despawn` 不算寫者，理由是「它是成員關係結束，不是值的寫入」（使用者 2026-11-11，K.3 第 4 項）。

## 9. hook 記的變更什麼時候生效

更新：2026-11-15（`v8-arch-gyo2-hook-flush`；使用者 2026-11-15 定的做法與上限，HANDOFF 的 `v8-arch-gyo2-hook-queue-flush-at-run-end`、`v8-arch-gyo2-hook-flush-not-in-plan-bump-versions`、`v8-arch-gyo2-hook-flush-limit-two-levels`）。

用射擊遊戲的例子：子彈一出現，hook 幫它加上拖尾。

**規則：**

- **hook 記的變更，永遠比觸發它的那一輪晚一輪生效。**
  - 中段的同步點觸發的：在同一次執行的下一個同步點生效。
  - 最後一個同步點觸發的：在接下來的「清空輪」生效。清空輪由機制在最後一個同步點之後做，一輪一輪套用到沒有為止，所以這一次執行結束時都已生效。清空輪不是執行計畫的一步；這條規則改了執行計畫指紋與 C 指紋的版本號。
- 每一輪是一次完整的套用（刪除、移除、生成、加上、初始值、OnAdd、OnRemove），每一輪各取一個新的 stamp。
- 清空輪有上限。預設 8；產品可以在 `KernelSettings::hookRoundLimit` 改預設（下一次 `Run` 生效；0 在 `Run` 開始時斷言）；每張 schedule 可以在 `ScheduleOptions::hookRoundLimit` 另外指定（會進指紋）。最少 1，0 在啟動時擋下。
- 碰到上限是程式的錯誤：這次執行停下、World 中斷，錯誤會點名 schedule、上限來自哪裡、最後一輪還在記東西的 hook。
- **Engine 的上限是保險，不是遊戲規則；連鎖幾次該停由遊戲自己的 System 來數。**
- **例外：`ExclusiveWorld` 當場改成員觸發的 hook**（第 11.6 節）記的變更，在那次呼叫回傳前就一輪一輪套用到空，只套用這些（同步點還沒套用的紀錄與這張 schedule 的 hook 佇列不動），上限同上（使用者 2026-11-17 選「當場套用到空」；這條路才用，同步點的規則不變）。
- 提醒：每一輪清空是一次完整的套用。連鎖很長的玩法（例：棋子效果連動），更適合用自己的 System 加佇列來結算——這是作者的選擇，Engine 不擋。

**一個不固定的情形（每幀生成；巢狀執行做好之後才會出現）：**

- 每幀那張的某個 stage 生成子彈；hook 在那個 stage 的同步點記「加拖尾」；拖尾在下一個同步點才加上。
- 驅動固定步的 System 如果就排在下一個 stage，這一幀的固定步都看到「有子彈、沒拖尾」。這一幀跑幾步固定步不固定（看真實時間），所以「沒拖尾的固定步有幾步」也不固定。
- Kernel 不替你解決：每一幀和固定步怎麼對應，取決於遊戲的邏輯。

**要固定時的做法與代價：**

| 做法 | 結果 | 代價 |
|---|---|---|
| 用初始值的宣告（第 8 節），不用 hook | 拖尾在子彈出現的同一個同步點補上 | 拖尾的值只能從生成時的請求算出來；受漏斗的規則限制 |
| 生成者和驅動固定步的 System 之間多隔一個 stage | hook 記的在驅動之前的同步點生效 | 多一個 stage；連鎖每多一層就要再多隔一個 |
| 生成子彈的 System 直接把拖尾放進生成的內容 | 同一個同步點出現 | 生成者要知道拖尾；拖尾的寫者變成生成者 |

**訊息**（2026-11-20 改，`v8-arch-gyo2-time-events`）：清空輪裡 hook 寫的訊息，跑過的讀者下一次跑時照自己的游標讀到。還沒跑過的讀者不拉住保留，最外層 Run 結束的修剪可能先丟掉（例：遊戲初始化的清空輪寫的，第一幀的讀者還沒跑過時讀不到）；要傳給之後的 schedule，用元件或 Resource。

## 10. 生成被丟掉的時候

更新：2026-11-15（`v8-arch-gyo2-spawn-reject`；使用者 2026-11-15 定的方向與各件，HANDOFF 的 `v8-arch-gyo2-spawn-reject-drop-count-reason` 與之後的轉達；事前宣告 [spawn-reject-declaration.zh-Hant.md](spawn-reject-declaration.zh-Hant.md)）。規則的條文：規格 F-13、kernel DI-KS 9。

用 PvP 的例子：玩家 7 正在玩，網路那頭又送來一筆「玩家 7 加入」，同一幀還有一筆「玩家 8 加入」。負責加入的 System 照收到的編號記了兩筆生成。到了同步點，7 還活著，那一筆被丟掉；8 照常生成。這一幀照常跑完，`Run` 不回報錯誤，程式不終止。

**什麼時候會被丟（每筆取第一個成立的理由）：**

1. **落在配發器的範圍**：用指定編號生成（`SpawnWithId`），編號落在這個 World 的配發器的範圍裡。那一段編號是配發器的，外面給的不收（照收的話，配發器之後配到同一個編號就撞上）。
2. **這個同步點生成了兩次**：同一個編號在同一個同步點有兩筆以上的生成。
   - 全部是同一個記錄者記的（同一個 System 的一次呼叫，或同一個 hook）→ 留先記的那筆，其餘丟掉。例：同一幀收到兩筆「玩家 7 加入」，負責加入的只有一個 System，第一筆生效。
   - 來自兩個以上的記錄者 → 全部丟掉。兩個不同的 System（或 hook）撞同一個編號，一定是程式或資料的錯；丟掉全部，結果才不會隨 plugin 的登記順序變。
   - 上一窗的 hook 記的生成在同步點最先套用；這一窗的 System 又用同一個編號生成時，後者丟掉。
   - 同步點的兩次套用（先套用上一窗 hook 記的、再套用這一窗 System 記的）**各自判**：例如上一窗兩個 hook 撞同一個編號（兩筆都丟），這一窗只有一個 System 用那個編號生成，它照常生成。Engine 只給機制：撞了就丟、留明細讓你看得到，不替每一種排法訂規則。
3. **編號還活著**：生成時（同一次套用的刪除做完之後）那個編號還活著。用配發器生成（`Spawn`）撞上活著的實體也一樣丟掉，明細會標明「用配發器生成」——那只會是程式寫錯（配發器、或組裝與 runner 直接寫了 World）。

**被丟掉的那一筆**：不生成；它帶的元件一個都不寫，活著的那個實體原封不動；不過初始值的宣告（第 8 節）；不觸發 OnAdd；不取新的 stamp。同一步其餘的生成照常。

**重入的標準寫法**：玩家 7 要重新進來時，**在同一個 System 裡先記刪除 7、再用編號 7 生成**。同一個同步點固定先刪再生，這一對不會被丟。兩個例外：編號落在配發器的範圍時，刪除照做、生成被丟，7 就消失了；別的記錄者在同一個同步點也生成了 7 時，照第 2 點的規則處理。

**生成時的值放進 bundle**：同一個 System 寫成「用 7 生成玩家」接著「往 7 插入名字」，7 還活著時生成被丟掉，**插入卻照常落在原本的玩家 7 上，把他的名字改掉**（移除也一樣）。插入與移除是各自獨立的紀錄，看的是當時誰活著。生成時要給的值放進 bundle，或用初始值的宣告。

**怎麼查**：

- 每被丟一筆，World 加一筆 `RejectedSpawn` 明細，總數 `World::RejectedSpawns()` 加一。
- runner 在兩次 `Run` 之間讀：`World::Diagnostics<RejectedSpawn>()` 看，`World::TakeDiagnostics<RejectedSpawn>()` 取走並清空明細。總數只增不減，取走明細不歸零。
- **明細不設上限，runner 要定期取走**，不取就一直變長。
- World 中斷之後明細與總數都還在（中斷後仍然可讀）；World 重建時從 0 開始，要在銷毀前取走。
- System 讀不到明細；產品的 System 也不能用 `DiagnosticsWriter` 寫這個型別（編譯失敗）。

| 欄位 | 意思 |
|---|---|
| `entity` | 被丟的編號 |
| `reason` | `IdInAllotterRange`（落在配發器的範圍）、`IdSpawnedTwice`（這個同步點生成了兩次）、`IdAlive`（編號還活著） |
| `emitter`、`emitterIsHook`、`emitterZone` | 記錄者（System 或 hook）的名字、是不是 hook、它的區 |
| `fromAllotter` | 用 `Spawn`（配發器）記的 |
| `schedule` | 丟掉它的那張 schedule |
| `step` | 這筆紀錄的位置：System 在計畫裡的位置；hook 是觸發它的那個同步點 |
| `appliedAt` | 丟掉它的那個同步點（hook 記的紀錄在下一個同步點才套用，所以和 `step` 不同；清空輪裡是最後一個同步點） |
| `syncEpoch` | 丟掉當下那個 World 的同步點序號（`World::SyncEpoch()`，之前已完成的同步點數；Kernel 沒有 tick，`v8-arch-gyo2-time-events`） |
| `otherEmitter` | 只在 `IdSpawnedTwice`：另一個記錄者（留下的那筆的記錄者，或一起被丟的另一個記錄者） |

**System 要回應外面時自己先查**：遊戲要告訴客戶端「加入被拒」時，「編號還活著」「落在配發器的範圍」System 可以自己先查；「這個同步點生成了兩次」是別的記錄者造成的，System 查不到，只能由 runner 讀明細。

**編號與權威**（LIM-7）：

- **PvP（有權威端）的形式**：有 (c) 區資料的 World，編號只由權威的一方給。只在本機存在的東西不和權威共用編號空間（怎麼分開由產品決定）；(c) 區的狀態以權威為準。照這樣設計，(b) 區就擋不到 (c) 區的生成；Kernel 不另外擋。
- **單機**：只有一端，不會分岔；同一個 World 的生成都走配發器就好；用指定編號生成時避開配發器的那一段（撞到會被丟掉，明細查得到）。

**刪除與生成同一個編號**：(b) 區的 System 記了刪除某個編號、同一窗 (c) 區的 System 又用那個編號生成，照固定的順序先刪再生，新的留下，不是錯誤（區規則 Z5 隨 `v8-arch-gyo2-no-runtime-interception` 拿掉）。

## 11. 獨占的 System 跑另一張 schedule

（K0，`v8-arch-gyo2-nested-schedule`，使用者 2026-11-15～16 核准。條文在 [Kernel 文件](../kernel.zh-Hant.md#di-ks-16) DI-KS 16～23。）

用射擊遊戲當例子：每一幀依序「讀輸入 → 跑固定步（0 到數步）→ 算畫面」。遊戲開始時先跑一次遊戲初始化（編排層，第 13 節）：放進固定的設定，例如規則的參數、按鍵的對應；地圖這類世界內容不走遊戲初始化，走 Loading。

「跑固定步」可以由 World 裡的一個 System 來做：參數是 `ExclusiveWorld<…>` 的 System，在執行中呼叫 `RunSchedule`，跑幾次由它自己決定。

```cpp
// 驅動固定步的 System（示意：它本身是下一批的事；steps 從時間的 Resource 算出來）
SystemResult DriveFixedSteps(ExclusiveWorld<> world) {
    auto steps = world.Read<StepBudget>([](const StepBudget& budget) { return budget.steps; });
    if (!steps) return GYO::Engine::Base::Err(std::move(steps).error());
    for (int i = 0; i < steps->value_or(0); ++i) {
        if (auto ran = world.RunSchedule("FixedUpdate"); !ran) return GYO::Engine::Base::Err(std::move(ran).error());
    }
    return Ok();
}
```

### 11.1 誰能跑、能跑哪一張、什麼會被拒絕

- 只有 `ExclusiveWorld<…>` 的 System（只能是 (b) 區、而且是唯一的參數）。run 條件拿不到它；hook 拿得到 `ExclusiveWorld<>`，但在 hook 裡跑 schedule 會被拒絕。一般的 System 經全域指標呼叫 `WorldExecutor::Run` 是斷言。
- 能跑這個 World 登記過的任何一張，除了 extract。可以多層（A 跑 B，B 裡的獨占 System 再跑 C）；沒有層數上限，正在執行的那張（任何一層）不能再跑。
- 被拒絕的呼叫回 Err：跑正在執行中的（`ScheduleRunning`，X-11）、名字不存在（`UnknownSchedule`，X-12）、hook 裡（`HookRunsSchedule`，X-13）、`Read`／`Modify` 的閉包裡（`ClosureRunsSchedule`，X-14）、extract（`ExtractNotNestable`，X-15）。這些都是程式的 bug：**World 會中斷，錯誤只報一次；外層的 System 吞掉它回 Ok 也一樣**（和 Bevy 的 `try_run_schedule` 可以接住繼續跑不同）。
- 裡面那張照常完整執行：它的 System、同步點、hook、跑完時的清空輪，和從外面跑它一樣；訊息只在最外層 Run 結束時修剪，巢狀跑不修剪；Kernel 不推 tick（固定步的 tick 由時間 Module 的 `RunOneFixedStep` 推）。
- 裡面那張出錯：World 立刻中斷，`RunSchedule` 回 Err，外層的 System 應該把它往上回傳。錯誤處理只呼叫一次，拿到的錯誤包含裡面那一步與外層的 System；外層回傳別的 Err 時兩個都在清單裡；中斷之後再呼叫 `RunSchedule` 不執行、回傳留下的錯誤；裡面丟出例外、外層接住，執行器停下並補「巢狀執行被例外中止」（`NestedRunAborted`）。
- 跑完之後外層的 System 繼續執行：手上的 `ExclusiveWorld` 照常可用，之後的寫入用新取的變更記號（別的 System 的 `Changed` 看得到）。裡面出錯之後，經 `ExclusiveWorld` 的寫入照樣寫得進去（World 已經中斷，只供診斷）。

### 11.2 延後的變更在哪裡套用

規則只有一句：**記下的變更，在記它的那張 schedule 的下一個同步點套用。** 外層還沒套用的留在外層，裡面那張看不到；獨占的 System 自己沒有記延後變更的路徑，它的結構變更是當場改（第 11.6 節），當場改也不會把這些紀錄提前套用。

**每次都一樣的**

| 情形 | 結果 |
|---|---|
| 「讀輸入」排在跑固定步的前一個 stage，它記下「生成子彈」 | 下一個固定步就有子彈 |
| 固定步裡面自己記的變更 | 同一個固定步的下一個 stage 就看得到 |
| 排在跑固定步後面的 System 記的變更 | 下一幀的固定步看得到 |

**不保證每次一樣的，與避開的排法**

| 情形 | 會怎樣 | 怎麼排可以避開 | 代價 |
|---|---|---|---|
| 「讀輸入」和跑固定步放在同一個 stage、排在它前面 | 這一幀跑了幾步，子彈就晚幾步出現 | 跑固定步的 System 自己佔一個 stage，或在它前面宣告一個同步點。Engine 自己提供的那個 System 照這樣放 | 不用改 Kernel；作者要照這個排法 |
| 每幀那張生成的東西觸發的 hook 再記的變更 | 晚幾個固定步生效不固定；上一列的排法救不了 | 沒有不花成本的排法。要每次一樣，改由固定步裡的 System 來做這件事 | — |
| 一幀對應幾個固定步 | 取決於真實經過的時間 | 輸入帶著「在第幾個固定步生效」交給固定步（像網路遊戲的輸入緩衝） | 另一套機制，Engine 這一批不提供 |

**每次都一樣、但要先知道的規則**

和跑固定步的 System 同一個 stage、排在它前面的 System 記的變更，會在這一幀所有固定步之後才套用。所以：外層拿掉 C、裡面加上 C，最後沒有 C；外層寫 1、裡面寫 2，最後是 1；外層生成的實體，這一幀的固定步裡還不存在。照上面第一列的排法就不會碰到。

其他要知道的後果：

- 外層 (b) 區記了往實體 E 的插入，裡面那張刪了 E：外層的同步點丟掉那一筆，有計數（`DroppedMissingEntity`）。
- 不論區，都照上一條：裡面那張先套用；外層的紀錄在外層的同步點套用時，目標不在就丟掉並計數（Z5 的跨層重查已拿掉）。
- 同步點觸發的 hook 再記的變更，進的是「正在套用的那張 schedule」的 hook 佇列，在那張跑完時清空；外層 schedule 的 hook 佇列不被裡面那張動到。

**作者的負擔**：獨占的 System 和同一個 stage 的每個 System 都算衝突；沒寫先後的照登記順序平手（第 5 節），要固定先後就寫明。

### 11.3 玩家離開又用同一個編號加入

- **重入的標準寫法**：處理加入的 System 不管舊的角色在不在，先記刪除那個編號、再用同一個編號生成，兩筆配成一對。兩筆在同一個同步點，固定的順序是先刪再生，所以舊的被刪、新的留下；寫在固定步裡也一樣。只寫生成而舊的還在：那一筆生成會被機制丟掉並留下紀錄（第 10 節）。
- **要避開的排法**：外層（每幀那張）記了刪除某個編號，同一幀的固定步裡不要用同一個編號生成——外層那筆刪除在固定步之後才套用，新的會被它刪掉（舊的還活著時，裡面那筆生成先被丟掉並留明細，之後舊的照樣被外層刪掉）。把處理離開與處理加入放在同一張 schedule 就不會碰到。Engine 不為這件事另加機制（`v8-arch-gyo2-nested-same-id-respawn-not-engine`）。

### 11.4 讀寫資料：閉包

- 元件與 Resource 都在閉包裡讀寫：`world.Read<T>(實體, f)`、`world.Modify<T>(實體, f)`；Resource 不帶實體：`world.Read<R>(f)`、`world.Modify<R>(f)`。介面不交出指標（沒有 `Get`、`GetMut`）。
- 閉包可以回傳值：`auto hp = world.Read<Health>(player, [](const Health& h) { return h.value; });`。結果是 `Result<std::optional<值>, KernelError>`：沒有值是實體沒有那個元件（或 Resource 不存在），f 沒有被呼叫；f 回 `void` 時結果是「有沒有呼叫 f」。值的讀寫不會回 Err（回 Err 的是當場改，第 11.6 節）。
- **需要的值在閉包裡取出來回傳**；不要回傳指標、參考、`Mut`、`std::reference_wrapper`、`std::span`、`std::string_view` 這類檢視、迭代器（編譯不過，放在 `std::pair`／`std::tuple` 裡也一樣，所以 `return std::tie(...)` 編譯不過）；也不要把它們藏在自己的結構、`std::optional` 裡或存到外面的變數（編譯會過，但 `RunSchedule` 之後就是懸空的，屬於 LIM-3）。
- 閉包裡可以再開閉包；閉包裡不能跑 schedule（回 Err）。`Modify` 在呼叫 f 的當下記變更，f 沒有真的改值也算。
- **元件與 Resource**：讀寫任何元件、任何 Resource 的值，`KernelSettings` 也可以（改了下一次 `Run` 生效）；綁執行緒的 Resource 只在它的執行緒上（第 5 節）。存取驗證打開時，這些寫入不算未宣告；訊息緩衝不在內。放進或拿掉 Resource 用 `SetResource`／`RemoveResource`（第 11.6 節）。
- **`Writes<R…>` 是選填的標註**：`ExclusiveWorld<Writes<Score, Round>>` 只把 `Score`、`Round` 記進登記表（`SystemInfo::access.annotatedWrites`），給權威的策略看獨占 System 寫了什麼；不改變存取、不算寫者、不受區規則檢查（使用者：「嗯，可以留選填」；`v8-arch-gyo2-exclusive-writes-optional`）。hook 也可以帶。
- 獨占的 System 不能同時是 ingress（啟動錯誤，S-17，隨區規則那一批拿掉）。

### 11.5 開局（Startup）——已搬到編排層

Kernel 不再有開局的 schedule（`v8-arch-gyo2-orchestration-skeleton` 第 2 段拿掉；DI-KS 22 由編排層的 O-1 取代）。開局要做的事改寫成遊戲初始化清單裡的 schedule，見第 13 節。**之後要先空跑幾步（例如物理預熱）**：照舊自己寫一個 System——每幀那張裡的獨占 System，讀一個旗標 Resource，還沒做過就跑幾次固定步、把旗標設起來。

### 11.6 當場改成員與增刪 Resource

（`v8-arch-gyo2-kernel-subtract-structure` 第 3 段；使用者 2026-11-17：「沿用 ExclusiveWorld」。條文在 [Kernel 文件](../kernel.zh-Hant.md#di-ks-18) DI-KS 9、18、19、21。）

用讀檔當例子：讀檔的 System 把存檔裡的實體一個一個放回去、把 Resource 換掉，接著跑固定步讓畫面追上。

```cpp
SystemResult LoadSave(ExclusiveWorld<> world) {
    for (const SavedUnit& unit : saved.units) {
        auto spawned = world.SpawnWithId(unit.id, Position{unit.x, unit.y}, Health{unit.hp});
        if (!spawned) return GYO::Engine::Base::Err(std::move(spawned).error());
        if (*spawned) { /* 沒生成：理由在 *spawned，也記進了 RejectedSpawn */ }
    }
    if (auto set = world.SetResource(Round{saved.round}); !set) return GYO::Engine::Base::Err(std::move(set).error());
    return world.RunSchedule("FixedUpdate");   // 裡面看得到剛放回去的實體
}
```

| 函式 | 回傳 | 說明 |
|---|---|---|
| `Spawn<C…>(值…)` | `Result<EntityId>` | id 由配發器發（同 `Spawn<Bundle<…>>`）；配發器發的 id 還活著時不生成、回錯誤碼 `AllottedIdAlive`，並照第 10 節記一筆 `RejectedSpawn`（用配發器生成），手上不會拿到別人的 id；這個錯誤只交給你的 System，不算這次 Run 的錯誤、World 不中斷；沒有宣告配發器是 `NoAllotter`，用完是 `AllotterExhausted`（這次不生成、呼叫繼續，System 回傳後這次 Run 以它結束） |
| `SpawnWithId<C…>(id, 值…)` | `Result<std::optional<SpawnRejectReason>>` | 沒有東西表示生成了；不能生成（id 在這個 World 的配發範圍、或還活著）時不生成、回傳理由，並照第 10 節記一筆 `RejectedSpawn` |
| `Insert<T>(id, 值)` | `Result<bool>` | 加上或蓋過；實體不在時不插入、回 false，也計入 `DroppedMissingEntity` |
| `Remove<T>(id)` | `Result<bool>` | 先跑 OnRemove 再拿掉；沒有那個元件回 false |
| `Despawn(id)` | `Result<bool>` | 每個元件先跑 OnRemove，再依元件的編號拿掉（同同步點）；實體不在回 false |
| `SetResource<T>(值)`、`RemoveResource<T>()` | `Result<void>`、`Result<bool>` | 放進（蓋過）或拿掉 Resource；綁執行緒的記下這條執行緒；拿掉時不在回 false |

- **當場生效**：呼叫回來時就改好了；做法和同步點套用一筆紀錄一樣（新的格先填初始值的宣告、再跑 OnAdd；拿掉時先跑 OnRemove）。這些 hook 記的變更也在回來之前一輪一輪套用到空（第 9 節的例外），上限同清空輪，超過是 `HookRoundLimit`、World 中斷。
- **不動別人的紀錄**：這張 schedule 還沒到同步點的紀錄（同一個 stage 排在前面的 System 記的）和 hook 佇列都不會被提前套用。所以前面的 System 記了「用同一個 id 生成」、這裡又當場生成了，前面那一筆到同步點會被丟掉並留明細（`IdAlive`）。
- **變更記號**：變更用這次呼叫的記號；每一輪 hook 套用各取新的。回來之後手上的 `ExclusiveWorld` 照常可用，之後的寫入排在這些記號之後（和跑完 `RunSchedule` 一樣）。
- **不能做的**：在 `Read`／`Modify` 的閉包裡呼叫（`ClosureChangesMembership`）、hook 拿到的 `ExclusiveWorld<>` 呼叫（`HookChangesMembership`）——都是程式的 bug，World 會中斷、錯誤只報一次。
- **例外**：當場改觸發的 hook 或初始值函式丟出例外時，World 在例外離開那一刻中斷：還沒套用的紀錄丟掉，已經套用的留著。System 接住例外繼續跑也一樣：之後的當場改不做、回傳留下的錯誤，這次 Run 結束時錯誤處理收到一次 `ImmediateChangeAborted`（同巢狀跑的 `NestedRunAborted`）。不經這些函式、經 World API 或儲存本身改成員，不論在當場改之前或之後，都是斷言（使用者 2026-11-17 選「保留」）。只在序列執行。
- **存取驗證打開時**：當場改本身不算違規；但在它之前，這個 System 在宣告外的改動（例：經存下的位址寫訊息緩衝）會先被比對出來，這次變更就不做、回 `BoundaryChange`、World 中斷（同巢狀跑之前）。
- **一般的 System 要增刪 Resource**：用 `Commands` 的 `SetResource`／`RemoveResource`，在同步點的 resource 那一步依 Resource 的編號、Plan 的位置、記錄的先後套用；hook 也可以用（下一窗生效）。

## 12. 登記兩步、跑起來之後再登記

（`v8-arch-gyo2-kernel-subtract-structure` 第 2 段；規則全文在 [target/kernel](../kernel.zh-Hant.md) DI-KS 25、26。產品通常不直接呼叫，而是經編排層：第 13 節。）

- **兩步**：`registry.Register(owner, [](Registrar& r) { … })` 開一個 `Registrar`，只在回呼裡有效；`registry.BuildPlans()` 建 Plan。兩步可以重複：建好之後可以再開。`owner` 是誰登記的（名字＋序號），Kernel 只存、只比相等、只拿來印。（舊的 `Registry(plugins).Finish()` 與 Kernel 的 `Plugin` 已拿掉，登記單位在編排層。）
- **什麼時候看得到**：之後一輪登記的東西，`BuildPlans` 成功之前外面看不到（`FrozenRegistry` 與 World 都還是舊的）。成功時版本號加一；用這份 Registry 建的 World 下一次被用到時自己接上，所以建好之後、下一次 `Run` 之前，World API 就能用新的元件與 Resource。舊的 System 保留 `Changed` 的基準與 `Local`。
- **失敗**：建 Plan 失敗或呼叫 `DiscardRegistrations()`，那一輪整個丟掉，Registry 與 World 留在上一個版本，那一輪發出的 id 作廢、下一輪接著上一版發。
- **之後一輪准什麼**：新的元件、Resource、事件、具名集合、schedule、stage、System 的集合、effect、System，`RemoveSystem`，在既有的 schedule 上掛新 System。不准（R-E7，那一輪丟掉、World 照跑）：對已有的元件加 hook 或初始值宣告、對已有的具名集合加成員、配發器、`DigestSet`、`RequireExplicitZones`。
- **拿掉 System**：`r.RemoveSystem("名字")`，下一次 `BuildPlans` 生效；不在表裡回 false。`FrozenRegistry::Systems()` 照樣列出它（`removed` 為真）；同一個名字可以再登記；還有別的 System 的 before／after 指著它時建 Plan 失敗（D-E4）。
- **World 准不准**：`WorldExecutor(registry, "名字", WorldOptions{.dynamicInsert = false})`。只要有一個這樣的 World 活著，那份 Registry 的 `Register` 就回 `DynamicInsertRefused`、什麼都不改（同一份 Registry 上准的 World 也一起被擋）。要不要記警告由呼叫的人決定（Kernel 只回傳錯誤）。准的時候不保證兩端的指紋一致。
- **不能做的**（斷言）：在 `Register` 的回呼裡再呼叫 `Register`／`BuildPlans`／`DiscardRegistrations`；有登記待建時建 World；用這份 Registry 建的 World 在 `WorldExecutor::Run` 裡時呼叫那三個：它的 System、hook、初始值函式、它跑的別的 World 都算（別的 Registry 可以）。同一份 Registry 的登記與它的 World 的建立、銷毀、執行，要在同一條執行緒上做或由呼叫的人排好（Kernel 看不到，[共用規則頁](../../../cross-component-rules.md) CCR-5）。
- **讀登記表**：`FrozenRegistry` 另外交出 `Version()`、`Hooks()`、`InitialValues()`、`Schedules()`、`Sets()`、`OrderGraph(schedule)`（解析後的順序，System 對 System，每條標來源：stage、宣告、同步點、平手）、`SystemInfo::effects`。`FrozenRegistry` 物件一直有效，它交出的清單在版本號變了之後要重新讀（CCR-4）。

## 13. 編排層：把 System 接進 World、排先後之外的流程

（`v8-arch-gyo2-orchestration-skeleton`；規則全文在 [orchestration](orchestration.zh-Hant.md) 的 Design Invariants O-1～O-10。程式 `engine/orchestration`，`GYO::Engine::Orchestration`。）Kernel 決定一張 schedule 裡 System 的先後；編排層決定哪幾張、什麼時候、在哪個 World 跑，並把登記單位與產品自己的登記接進每個 World。

- **產品自己的登記**：`ProductDescription` 每個 World 一份 `WorldDescription`，`build` 是產品的登記回呼（拿到 `WorldSetup`：`Registrar()`、每幀清單 `Frame()`、`Prepare(fn)`、`CarryOver<T>()`、選項包）。它在所有登記單位的 `Build` 之後、一定經過一次 `Register`。`Product::Build(description, MakeRegisteredUnits())` 建出整個產品；啟動錯誤（Kernel 的、編排層的 O-E1～O-E7、登記單位回報的）一次收在 `ProductStartupReport`，有任何錯誤就不建任何 World。
- **登記單位**：`Module`（自己維護）與 `Plugin`（第三方），生命週期是產品一次 `Configure`，每個 World `Build` → Kernel `BuildPlans` → `Finish` → `Check`。產品帶哪些寫在 `config/modules.csv`、`plugins.csv` 與產品自己的 `gyo_product_units(...)`；依賴只寫在登記檔（缺、排在後面、Module 依賴 Plugin 是 O-E3）。單位沒有 `Name()`，名字是登記檔那一列的。
- **遊戲初始化**：`ProductDescription::gameInit`（schedule 名的清單；登記單位可以 `Append`／`InsertBefore`／`InsertAfter`），整個行程一次、在第一個 World、在任何一幀與任何 `RunSchedule` 之前。`RunFrame`、`RunSchedule` 在它還沒跑時先跑它；直接呼叫 `WorldExecutor::Run` 是繞過編排層，不會先跑。某一張回錯誤就停下回傳、記為已跑。
- **每一幀**：`RunFrame()` 照 World 的順序跑各 World 的每幀清單；某一張回錯誤就停下回傳（排在後面的 World 那一次不跑）。**一個 World 壞了（中斷，或替換失敗後位置是空的），`Product` 就結束**：之後每個入口都先檢查、什麼都不跑，回 `WorldInterrupted`／`WorldMissing`，錯誤上掛著 `WorldFailure`（哪個 World、怎麼壞的、壞在哪裡、之前最後跑的 schedule）；runner 決定關掉程式或另建一個 `Product`（回主選單）。編排層不寫日誌，要記就由你（或之後的日誌機制）拿 `WorldFailure` 去記（O-11）。runner（預設只跑一次 `RunFrame`）擁有迴圈；runner 與持有 `Product` 的人只呼叫 `RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`。只跑某一張（例：放玩家、讀檔）用 `RunSchedule(world, schedule)`。
- **準備**：每個 World 建好之後、第一次 Run 之前（啟動與每次替換），照「帶過去 → 登記單位的準備 → 描述的準備 → 替換帶的 `extra`」呼叫。準備放預設值時**不要蓋掉已經在的值**（帶過去的值先放進來了）。失敗時這個 World 不用。
- **替換 World（讀檔）**：`ReplaceWorld(name, {extra, beforeSwap, order})` 照描述重新登記那個 World（新的 Registry；之前動態插入的一律不在），帶過去、準備、在新 World 上跑 `beforeSwap`（例：讀檔的 schedule），全部成功才換上；遊戲初始化不重跑。`order` 預設 `BuildNewFirst`（失敗時保留舊的）；兩個 World 放不下時用 `DropOldFirst`（先銷毀舊的，失敗時位置是空的、`Product` 結束，不能再填）。壞掉的 World 不能拿替換來救。存檔的格式與版本是作者的，Engine 不認得：**發替換之前自己驗證存檔版本、素材、網路快照**，驗不過就不發（選 `DropOldFirst` 時尤其要先驗）。
- **帶過去**：`WorldSetup::CarryOver<T>()` 標記某個 Resource 換 World 時帶到新的（例：選項選單改過的按鍵）。**值裡不能存指向舊 World 的參照（`EntityId`、指標、句柄）**，這是作者的責任。
- **動態插入**：`InsertSystems(world, owner, [](Registrar& r) { … })` 走 Kernel 的兩步（第 12 節）；`owner` 不能是登記單位或產品自己的名字（`OwnerTaken`）。World 不准（`dynamicInsert = false`）或建 Plan 失敗時，編排層記**一次**警告、把錯誤回給你、World 照跑（CCR-3）。登記單位的 `Finish`、`Check` 不重跑。
- **`World(name)` 的參照**：`Product::World(name)` 給的 `WorldExecutor&` 在那個名字下一次被替換之前有效；替換之後要重新拿。
- **執行緒**：建、跑、替換、插入、銷毀 `Product` 都在呼叫的那條執行緒上（CCR-5）。
- **標頭的順序**：Engine 的單位都在 `GYO::Engine` 底下（2026-11-22 起，`v8-arch-gyo2-namespace-root`），舊根 `Engine::` 和 `GYO::Engine` 互相遮蔽的問題不再存在；當時守它的 include-order 編譯檢查同日拿掉（使用者決定）。

# 作者怎麼寫一個 System（一頁；現況）

更新：2026-11-14。Owner：架構（GYO2 重建會話）。**狀態：給使用者確認 System 契約用的說明頁。寫的是分支 `claude/gyo2-mechanism` 上已經能跑的程式，不是新設計。** 例子取自 `tests/common/kernel/SkeletonTests.cpp`（同分支）。

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

// 讀這個 tick 的 Hit 事件，扣血，順便記一筆統計。
SystemResult ApplyHits(EventReader<Hit> hits, Query<Write<Health>> healths, ResMut<Stats> stats) {
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
| `EventReader<E>`、`EventWriter<E>` | 讀、送事件 | 讀、寫 E |
| `Spawn<Bundle<A, B…>>` | 生成實體，當場拿到 id；下一個同步點才看得到 | 寫配發器；延後寫 A、B… |
| `SpawnWithId<Bundle<…>>` | 用指定的 id 生成（讀檔、連線） | 延後寫 |
| `Despawn` | 刪除實體，下一個同步點生效 | 不算寫者 |
| `CommandsFor<Inserts<…>, Removes<…>>` | 延後加上或移除指定的元件 | 延後寫 |
| `Commands` | 延後加減元件，型別在函式體裡才決定；只能動標成 Shared 的元件 | 執行時檢查 |
| `Local<T>` | 這個 System 自己的狀態，跨 tick 保留 | 無 |
| `CurrentTick` | 現在是第幾個 tick | 讀 tick |
| `ExclusiveWorld` | 看到並修改整個 World；必須是唯一的參數 | 整個 World |
| `Extract<Read<T>>`、`ExtractRes<T>` | 在 extract 的 schedule 裡讀來源 World；沒有寫的版本 | 讀來源 |
| `SetRead<Tag>`、`DiagnosticsWriter<R>` | 讀一組具名的元件；寫診斷紀錄 | 讀；無 |

## 4. 登記時另外寫的

- **名字**。
- **掛在哪裡**：哪個 schedule 的哪個 stage。schedule 與 stage 的名字全由產品自己給。
- **先後**（可選）：`before`、`after` 寫別的 System 的名字。
- **run 條件**（可選）：一個回傳 `bool`、只讀不寫的函式。
- **執行緒親和、外部副作用**（可選）。
- **不必寫**：決定性區、role、rollback。不填就是一般的資料。

資料的登記只要名字。兩個例外：事件要寫它屬於哪個時鐘（Fixed 或 Frame）；元件預設只准一個 System 寫，要多個寫者得標 `Shared`。

## 5. 機制替作者做的

- **排順序**：照 `before`／`after`，不照登記的先後。兩個 System 寫同一個東西又沒寫先後：依登記的先後，並列在一張清單裡。
- **同步點**：每個 stage 結束時套用延後的變更。在前一個 stage 生成的實體，下一個 stage 就看得到。
- **事件**：排在寫入者後面的 System，當個 tick 就讀得到。
- **hook**：元件被加上或移除時，在同步點呼叫登記的函式。
- **啟動檢查**：順序成環、獨占的元件有第二個寫者、用到沒登記的型別，啟動時一次全部報出來。
- **出錯**：見下一節。

## 6. 錯誤怎麼回報

機制裡的錯誤只有兩種，一個事件只屬於其中一種：

| 種類 | 用在哪裡 | 發生時 |
|---|---|---|
| `GYO_ASSERT` | 便宜的前置條件與內部不變量 | 每一種建置都檢查；失敗就終止程式 |
| `Result` 的 `Err` | System 這次執行沒有正常結束 | 帶著上下文一路回傳，由 Executor 處理一次 |

`Result<void, KernelError>` 是 System 的**回傳契約**，不是把錯誤送去別處的通道：每個 System 都回傳它，`Ok` 是正常的路。

**System 裡怎麼寫**：呼叫的東西回了 `Err`，就包一層自己的上下文再回傳。System 自己不記錄、不印。

```cpp
SystemResult SpawnsWave(Spawn<Bundle<Position, Velocity, Health>> spawn) {
    for (int member = 0; member < 3; ++member) {
        auto made = spawn.New(Position{0, 0}, Velocity{0, 0}, Health{1});
        if (!made) {
            return Engine::Base::Err(std::move(made).error().Wrap("SpawnsWave: wave member " + std::to_string(member)));
        }
    }
    return Ok();
}
```

**Executor 怎麼處理**，只做一次：

1. 把完整的上下文鏈交給產品登記的 handler（`Product::SetErrorHandler`）。要不要寫日誌、寫到哪裡，由 handler 決定。
2. World 進入中斷狀態（`Interrupted`），這次 Run 結束，回傳同一個 `Err`。
3. 這次呼叫已經記下、還沒套用的延後變更全部丟掉，不會套用一半。
4. 之後再對這個 World 呼叫 Run 是斷言，直到重建 World。

上面那個 System 在配發器只剩兩個 id 時，handler 收到的內容是（骨架測試實際印出的）：

```text
AllotterExhausted [W-9]: the entity allotter's range is used up world=test-world
  at System 'SpawnsWave' (schedule 'Tick', step 0)
  at SpawnsWave: wave member 2
  at Executor: step 0 'SpawnsWave' of schedule 'Tick'
```

**會產生 `Err` 的來源**：System 自己回傳的；hook 回傳的；機制自己查到的（配發器用完、延後變更寫到不該寫的元件、存取驗證抓到沒宣告的存取）。

**兩個開關**，都由產品在啟動時寫進 `KernelSettings` 這個 Resource：
- **存取驗證**：開著時，機制比對每個 System 實際碰了什麼和它宣告了什麼，不合就是 `Err`。成本隨 World 的大小增加，所以可以關；關掉時這一類不會被查到。
- **`breakOnKernelError`**：`Err` 產生的當下呼叫一個會返回的掛鉤，讓除錯器停在現場；之後照常走 `Err`。

**啟動時的錯誤**是另一條路：`Registry::Finish` 把所有問題收齊，一次回傳一份 `StartupReport`，每一條寫明是誰、在哪裡登記的；產品決定怎麼印、要不要啟動。

**這些是誰定的**（使用者 2026-11-11、11-12，經側邊會話轉述；規格 K.3 第 17 項、§讀法、E.5；決定表的 `v8-arch-kernel-result-errors`、`v8-arch-kernel-verification-not-assert`、`v8-arch-kernel-break-hook`）：
- 不採「常開的檢查巨集」，改採 Go 式的錯誤處理，原則是不靜默。理由：相對於斷言的直接終止，`Result` 給得出上下文鏈，也讓 World 停在已知的狀態。
- System 包一層上下文回傳、不自己記錄；Executor 最外層只處理一次。使用者接受每個 System 多一行回傳。
- 存取核對不叫斷言，叫「驗證」，是可以關的執行期開關。
- 一個事件只有一種性質：`Err` 的產生與回傳路上不呼叫斷言的 handler，不呼叫任何終止出口。

## 7. 寫樣本時實際碰到的

1. **生成實體的 System 也算那些元件的寫者。** 「一個 System 生成、另一個更新」的元件，在預設獨占之下是啟動錯誤；樣本裡把 Position、Velocity、Health 標成 Shared 才過。歷史見第 8 節。
2. **事件一定要指定時鐘**，而且只能在那個時鐘的 schedule 裡讀寫。
3. **System 沒有捕獲**，所以要記東西只有兩條路：`Local<T>`（自己的），或 Resource（共用的）。
4. **`Query` 的 `Count()` 會真的走一遍**，不是讀一個現成的數字。

## 8. 「生成者算寫者」的歷史

- 元件預設獨占、寫者以 System 計、Startup 的 schedule 不計：使用者 2026-11-11 討論後定的（規格 K.3 第 3、9 項；B.3、B.3.1）。
- 「生成的 bundle 也算寫者」：不是那次討論定的，是規格對抗檢查的一條修訂（B3，架構師接受）。理由：生成改成型別寫在簽名上之後，bundle 裡的元件才進得了存取集合，否則函式體裡可以生成任何元件，繞過寫者的計數與區的規則。
- 「一個 System 在執行中生成、另一個更新同一個元件」該怎麼寫：規格、目標文件、決定表都沒有談到。規格舉的例子只有「Movement 寫 Velocity、Integrate 寫 Position」，沒有說誰生成它們。
- 同一份規格對刪除的處理是相反的：`Despawn` 不算寫者，理由是「它是成員關係結束，不是值的寫入」（使用者 2026-11-11，K.3 第 4 項）。

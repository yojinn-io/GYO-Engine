# 混音器 Module（`engine/modules/audio`）

更新：2026-11-22（`v8-arch-gyo2-audio-review`：從 master 時代的英文頁 [`audio.md`](../../audio.md) 搬來、改寫；最終文字使用者 2026-11-22 合併前核准「照建議，全部核准」）；2026-11-23（`v8-arch-gyo2-audio-clip-loader`：clip 改成共用的不可變值、拿掉 `ClipBank` 與 clip 格、加音效 loader 與第 6 節的 Data Contract；照[事前宣告](audio-clip-loader-declaration.zh-Hant.md)，使用者 2026-11-23 動工前核准，**最終文字在合併前核准時定稿**）。

由來：[重審報告與事前宣告](audio-review-declaration.zh-Hant.md)（使用者 2026-11-22 動工前核准，HANDOFF `v8-arch-gyo2-audio-review-approved`）。改了的條文：DI 1、DI 2、DI 3、DI 4、DI 7、DI 10；Contract 的 `Submitter` 執行緒契約、時間點的限界、搬動與銷毀 `Mixer` 的責任（從後端換成混音器的持有者，取代 `audio.md` 的「the backend stops output first」）；新的第 4 節（後端要求的介面，取代 `audio.md` 的 SDL 段落）；RC 1、RC 2、RC 5、RC 9。其餘照 `audio.md` 譯過來、不改語意；條號沿用，方便突變的 `guards` 追蹤（`docs/architecture/plans/audio/scripts/mutations.json`）。master 時代的批次名（`v8-engine-audio-*`）是條文與守衛的出處，計畫在 `docs/architecture/plans/audio/`，只當歷史。

## 一個例子

FPS 的 Client 開槍：槍聲是 asset 載好的 clip（`Load<Clip>`，第 6 節的 `sound`／`wav`）。畫面 World 的 System 判定開了一槍，把「播哪個 clip、左右增益、時間點」交給混音器，混音器播它、不另存一份；音訊執行緒每個 block 把時間點換成第幾個取樣開始響。畫面卡了一下，聲音照原本的時刻響（`scheduleDelay` 吃掉這段延遲）。遊戲暫停時，模擬不跑就沒有新聲音，已在響的照響完——暫停不作用在混音器上（DI 3）。

World 那一側怎麼交（請求緩衝、套用點、時刻從哪裡來、提交口怎麼配給每個 World）是作者的寫法，見第 5 節。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::Time ◄── GYO::Audio ──► GYO::Asset（loader 介面）
```

- 連結恰好是 `GYO::Base`、`GYO::Time` 與 `GYO::Asset`（DI 2）；不認得 SDL、World、產品。認得 asset 的 loader 介面：`sound` 型別、它的格式與 loader 歸 audio（asset 頁第 5 節「擁有型別的 Module」）。
- 音效 loader（`WavLoader`）由產品登記：在 `Configure` 經 `AssetLoaderRegistry::Register<WavLoader>()`。忘了登記不是啟動錯誤，是請求時的 `Failed(NoLoader)`（asset 頁第 3 節 (B) 第 3 列）。SDL 那一批有建混音器的登記單位時，改由它登記（宣告第 3 題第 4 處）。
- 輸出後端（SDL 那一批）接在 `Mixer::Render` 上，在 audio 外面（第 4 節）。

## 2. 它是什麼

- **輸出格式**：只有一種——48 kHz 立體聲交錯 PCM16（`AudioFormat.hpp`）。轉成裝置的格式是後端的事。
- **clip（`Clip`）**：1 或 2 聲道、48 kHz 的 PCM16，**共用取樣的不可變值**：複製只加參照計數，asset 的 store、作者與混音器握的是同一份取樣。`Clip::Make` 有兩種：收 span 的複製一次，收 `std::vector&&` 的不複製（loader 用）。其他都是 `InvalidClip`（Runtime Error）：空的、聲道數不對、樣本數不是整數個 frame、取樣率不同（不重新取樣）、frame 數超過 2^32 − 1。空的 `Clip{}` 拿去播是 Programmer Error。`Clip` 合 CCR-9（沒有 `mutable`、參照計數是原子的、解構不綁執行緒）；「不在音訊執行緒放掉」由混音器守（第 3 節、DI 7）。catalog 的型別名是 `sound`（`AssetTypeName<Clip>`）。
- **voice（`VoiceMixer`，內部）**：固定數量（1～1024，否則 `InvalidConfig`），每個 voice 從一個起始 frame 播一個 clip 一次，左右增益在 [0, `kMaxGain`]。排好但還沒開始的 voice 佔著位子。全部佔滿時新的請求丟掉並記數（`noVoice`）。播完的 voice 空出來。
- **混音**：增益換成 Q16 定點；voice 用 int64 相加，乘主增益（Q16），四捨五入（一半往上）、飽和到 int16，每個飽和的輸出樣本記數（`clippedSamples`）。整數算術讓每個平台的輸出逐位元相同。
- **晚到**：起始 frame 在下一個要算的 frame 之前的請求，從那個 frame 開始（clip 從頭播），記成晚到並記晚了幾個 frame（`maxLateFrames`）。
- **提交口（`Mixer::CreateSubmitter`）**：每個是一個有上限的 SPSC 環的生產端。所有環（`maxSubmitters` 1～64、每個 `queueCapacity` 1～65536）在建立混音器時配好；建立提交口只是發下一個（任何執行緒、任何時候；沒有了就 `TooManySubmitters`）。提交口用掉不還。執行緒契約見第 3 節。環滿了請求丟掉並記數（`droppedQueueFull`）。提交口共用混音器的狀態，所以可以活過 `Mixer`。
- **混音器握的參照（參照表與回收環）**：每個播放帶它的 clip（`PlayRequest::clip`），任何提交口都能播任何 clip。每個提交口有一張 `queueCapacity + maxVoices + 1` 格的參照表與一個空格的堆疊，建立時配好。`Play` 先取空回收環（下一點），再從堆疊拿一格、把 clip 複製進去（參照計數加一，在使用提交口的一方），命令裡只放 clip 的 view（取樣指標、聲道數、frame 數）與格號；環滿時那一格當場清掉、放回堆疊。音訊執行緒手上只有 view 與格號、**不握參照**：沒有 voice 的播放、結束的 voice（播完、停止、還沒開始就被停），把格號推進那個提交口的回收環（`uint32` 的 SPSC 環，和參照表一樣大）。voice 帶的租約標記是「提交口加一 << 32 ｜ 格號」。一個格號同時只在一處（堆疊、命令環、voice、voice 的結束清單、回收環、使用提交口的一方手上），所以回收環不會滿；每次 `Play` 先取空，所以表不會用光（用光是混音器內部的不變量被破，生產端斷言）。
- **取空**：使用提交口的一方在每個成員呼叫（`Play`、`Stop`——含無效 handle、`StopAll`）的開頭、與 `~Submitter` 裡，把回收環取空：那幾格清掉（最後一份參照可能在這裡放掉）、格號放回堆疊。所以**混音器握的參照最晚在那個提交口下一次被呼叫時放掉**；不再被呼叫的提交口，它的格留到混音器的狀態解構（上限：表的大小）；沒有 `Render`（後端沒開）時，排著的請求的格留在環裡，直到混音器的狀態解構。`Play`、`Stop` 可能在呼叫的執行緒上釋放大塊記憶體。作者沒有不經 `Play`／`Stop`／`StopAll` 就讓混音器放掉的入口（需要時照 RC 10）。
- **handle 與停止（`Submitter::Stop`、`StopAll`）**：`Play` 回 `SoundHandle`——高 7 位元是提交口序號加一，下面是那個提交口的序號，只在播放排進環時前進，所以 handle 不重用；因為環滿而丟掉的播放回無效的 handle 0。停止和播放走同一個環，所以音訊執行緒取到停止時，它指的播放都已取過；提交口只能停自己的聲音。停止的時間點照起播的規則換成 frame。還沒開始的 voice，停止 frame 在它的起點或之前就立刻釋放（從不出聲）；否則在停止 frame 結束，或者它已開始、而那個 frame 已經算過時，在取到停止的那個 block 的第一個 frame 結束（記成晚）。多個停止時最早的 frame 勝。`StopAll` 對它的提交口在呼叫前送出的每個聲音做同樣的事。voice 那一層只保存每個 voice 的停止 frame、不透明的 cookie（handle）、不透明的租約標記與 clip 的 view；handle、參照表與回收環是 `Mixer` 的。
- **播放位置（`Mixer::PlayheadOf`）**：每次 `Render` 之後，音訊執行緒把每個 voice 播的 handle 與位置（已混的 clip frame 數、有沒有開始）發布到一塊建立時配好的原子看板；換了聲音的 voice 重新指派（先清 tag、再寫狀態、再寫 tag），閒下來的 voice 清掉。任何執行緒都可以無等待地讀一個 handle 的項：tag、狀態、再讀一次 tag，回 Scheduled、Playing（帶 frame）或 NotPlaying（還沒取、結束、停止、丟掉、無效 handle、被搬走的混音器）。值最多舊一個 block，比聽到的聲音超前輸出的延遲。只供顯示（DI 4）。
- **時間點的換算（`Mixer::Render(block, anchor)`）**：後端在每個 block 開頭讀一次時間基準當 `anchor`。每個排著的請求從 `blockStart + floor((at - anchor) × 48000 + 1/2) + delayFrames` 開始（一半往 +∞，負的偏移也一樣），`at - anchor` 夾在正負一小時內，`delayFrames` 是 `scheduleDelay` 照同樣規則取整。請求在音訊執行緒取到時記成準時或晚，所以記準時的請求可能還在未來（最多到延遲，未來的時間點最多一小時）。`scheduleDelay`（0～1 s，預設 60 ms）讓稍晚送出的請求仍準時開始，也是「現在」播放的最小延遲。預設值來自 master 在 SDL macOS 的量測，SDL 那一批重新推導。
- **計數**：`accepted`、`onTime`、`late`、`noVoice`、`maxLateFrames`、`renderedFrames`、`clippedSamples`；relaxed 原子，任何執行緒可讀，只供顯示與驗收。
- **音效 loader（`WavLoader`）**：型別名 `sound`、格式 `wav`，把 WAV PCM16 的位元組變成 `Clip`（只複製一次：位元組→取樣，交給 `Clip::Make(vector&&)`）。格式與規則在第 6 節。

## 3. Contracts

- `Mixer::Render`（和它裡面 `VoiceMixer` 的排程與混音）在一條執行緒上跑，就是後端的音訊執行緒。`CreateSubmitter`、`Stats`、`SetMasterGain` 可在任何執行緒。
- **`Submitter` 的執行緒契約**（`v8-arch-gyo2-audio-review` 第 2 題，取代 `audio.md` 的「owner thread」）：提交口**不綁執行緒**。同一時間只有一方呼叫它的成員（`Play`、`Stop`、`StopAll`，與解構子——它取空回收環〔`v8-arch-gyo2-audio-clip-loader`〕）；換到別的執行緒時，前一方最後一次呼叫要 happens-before 後一方第一次呼叫（例：`std::thread::join`、同一把 mutex 的 unlock／lock、runner 交出 World 時的同步）。只是時間上錯開、沒有這種先後關係的呼叫，是資料競爭；執行時不檢查（只有 ThreadSanitizer 抓得到，L1 沒有它）。
- 增益超出 [0, `kMaxGain`]、空的 clip、被搬走的 `Submitter`，是 Programmer Error，在生產端、在任何計數之前斷言（`GYO_ASSERT`）；音訊執行緒取請求不檢查（`VoiceMixer::ScheduleChecked` 與 `VoiceMixer::Stop`，`noexcept`）。
- **送進混音器的時間點**是同一行程的時間基準上的時刻（DI 3）。時間點和 `Render` 的 anchor 來自不同的時間線時（例：給了不是時間基準讀數的手動取樣），請求會被當成晚到，或排到最多一小時後、期間佔著 voice。
- `MixerStats`：沒有請求在飛時（每個排著的請求都已取、沒有生產者在播）`submitted == playedOnTime + playedLate + droppedQueueFull + droppedNoVoice`；計數一個一個讀，期間的快照兩邊都可能不同，不拿來相減算在飛的請求。
- `MixerStats`：同樣條件下 `stopsSubmitted == stopsMatched + stopsUnmatched + stopsDroppedQueueFull`；`stopsLate` 數的是 matched 的請求（不是 voice）。停止無效 handle 什麼都不做、不記數。別的提交口的 handle、或從未發出的 handle（含序號 0），是生產端的 Programmer Error。同一個 voice 多次停止，最早的停止 frame 勝。已開始、停止 frame 在取到停止的 block 的第一個 frame 或之前的 voice 立刻釋放（不再混它），所以同一個 block 裡之後的播放可以拿它的 voice，第二次停止找不到它（unmatched）。
- 參照表：使用提交口的一方在排一個播放之前把 clip 放進那一格，所以命令環的 release 與 acquire 把取樣發布給音訊執行緒（取樣本來就不可變）。音訊執行緒只在那個播放不會再讀取樣之後（沒有 voice，或 voice 已結束）才推它的格號，以 release 發布回收環的尾端；使用提交口的一方 acquire 讀到之後才清那一格，所以音訊執行緒的最後一次讀在清之前。音訊執行緒推之前 acquire 讀回收環的頭端，所以使用提交口的一方對那一個位置的讀在寫之前。
- 即時的上限：`Render` 在一個 block 裡取完所有排著的請求，每次起播線性找空的 voice，每次停止掃過所有 voice，每個 block 發布一次所有 voice 的位置；最壞情形一個 block 是 `maxSubmitters × queueCapacity × maxVoices` 次比較。預設（4 × 256 × 32）很小；設定的上限（64 × 65536 × 1024）不是即時的。
- 在音訊執行緒混音時、或別的執行緒呼叫 `PlayheadOf`、`Stats` 時搬動 `Mixer`，是資料競爭；混音器的持有者在後端停止之前不搬、不銷毀它（第 4 節）。`maxLateness` 是最晚的晚到，由 frame 換算。
- 被搬走的 `Mixer` 輸出靜音；奇數個樣本時最後一個樣本是靜音。
- `Render` 填任何長度的 block，`renderedFrames` 前進它的 frame 數。內部每次最多 256 frame，用建立時配好的累加器。
- 主增益每次內部處理讀一次；改了從下一次生效。

## 4. 後端要求的介面（SDL 那一批照這裡接）

1. **給**：`Mixer::Render(Span<int16_t>, TimePoint anchor) noexcept`，在後端的音訊執行緒上、一次一個 block；格式常數（`AudioFormat.hpp`）；`Stats()`。
2. **要求**：每個 block 用時間基準取一次 anchor（DI 3）；只有一條執行緒呼叫 `Render`；後端只持不擁有的參照（若它和別人共用所有權、而音訊執行緒的回呼握著最後一份，混音器的狀態會在音訊執行緒上解構，破 DI 7）；混音器放在 Product 的共用處，持有者在後端停之前不搬、不銷毀它；後端在自己登記的 `OnDestroy` 回呼裡停（編排層 O-15：回呼照登記的反序、在所有 World 之後、共用處之前），後端的登記排在建混音器的單位之後，它的 `OnDestroy` 才會先跑。
3. 後端的錯誤（例：裝置開不了）是後端自己的錯誤碼；`AudioErrorCode` 的 4（舊的 `DeviceUnavailable`）不再用、不重編。

## 5. 把 World 接到混音器：作者的寫法

〔`v8-arch-gyo2-world-audio-withdrawn`，使用者 2026-11-22「照建議」：不另做 `world_audio` 這一層；下面五件用現有的 audio、編排層（O-12、O-15）與 world_time 的 Resource 就做得到，是作者的寫法，不是 Engine 的規則。Kira、SoLoud、miniaudio 也都讓作者自己給起播的時間。〕片段是示意，名字是真的 API，但不是測試過的程式。

**例子**：FPS 的 Client。模擬是 64 Hz 的固定步；第 312 幀補了兩步（第 1001、1002 tick），兩步各開一槍。玩家之後讀第 2 個存檔，畫面 World 被替換。

### 5.1 tick 換成時刻

判定的 System 不讀時鐘（determinism 第 11 節），所以聲音從「第幾 tick 開了一槍」開始，到 `Main`／`Last` 的表現層 System 才配 world_time 的 `RealTime`、`Tick`、`FixedTime`、`VirtualTime::scale` 換成時間基準的時刻，再放進要交給混音器的請求（第 5.3 節）。tick 怎麼從固定步交到 `Main`，看這個 World 有沒有回滾：

- **沒有回滾的 World**：固定步裡的 System 用 `MessageWriter<ShotFired>` 寫 `{tick, shooter}`，`Main`／`Last` 的表現層 System 用 `MessageReader<ShotFired>` 讀。
- **用官方回滾策略的 World**：上面的寫法是啟動錯誤 RB-E8——重跑的 schedule 裡寫的 `Message` 不能有外面的讀者（[回滾頁](rollback.zh-Hant.md) R-2 第 4 點；`StateRollbackStrategy.cpp` 的 `CheckMessages`）。改成：在固定步裡另放一個**只給表現層**的 System，把 `{tick, shooter}` 加進一個**沒有登記成可回滾**的 Resource（例：`PendingShotSounds`），`Main`／`Last` 讀它、清它；重跑時不要再出聲，就在登記這個 System 時用回滾 Module 的 `SkipDuringResimulation`（重跑中不跑）。判定的 System 照常寫它的判定資料，不因為聲音少寫或多寫。

```cpp
struct ShotSound { std::uint64_t tick; GYO::Kernel::EntityId shooter; };
// 沒有回滾：MessageWriter<ShotSound>（固定步）→ MessageReader<ShotSound>（Main／Last）
// 有回滾：固定步裡只給表現層的 System 把 ShotSound 加進不回滾的 Resource，登記時 SkipDuringResimulation；
//         Main／Last 讀它、換成時刻、清掉
```

換成時刻的取捨（作者選；`overstep` 是 `FixedTime::accumulator.Overstep()`，最後一步之後還沒用掉的時間）：

| 做法 | 第 1001、1002 tick 的兩槍 | 代價 |
|---|---|---|
| (a) 全用這一幀的 `RealTime::now` | 同一刻響（落在幀格上） | 最簡單；快轉、補幀時聲音擠在一起 |
| (b) 照 tick 往回推：`now − ((目前的 Tick − tick) × 步長 + overstep) ÷ 倍率` | 隔一個步長（倍率 1 時 15.6 ms），各自落在它那一步的時刻 | 要讀倍率與餘數；推回過去的量會吃掉 `scheduleDelay` 的預算，超過就記成晚到；暫停時沒有新的 tick；慢動作下「一步對應多長的真實時間」是作者的定義 |
| (c) 只保留同一幀裡的相對間隔（SoLoud `playClocked` 的做法） | 隔一個步長，整批從 `now` 起算 | 不推回過去、不吃延遲的預算；整批晚一點點 |

去重的狀態（例：上一個出過聲的 tick）放在不回滾的地方。時鐘型別（`Time::TimePoint`）只出現在表現層 System 換成 `PlayRequest` 的那一步，不進判定資料（Unity Netcode 的 `IsFirstTimeFullyPredictingTick` 是同一個想法：只在第一次預測時出聲）。

### 5.2 提交口：照 World 名字建一次，放在 Product 一份的共用處

混音器與提交口都放在 Product 的共用處（O-15），不放進 World。建混音器的是誰（一個登記單位，或產品自己）由產品定；輸出後端要找到它，後端的登記要排在建混音器的單位之後（第 4 節），所以混音器通常由一個登記單位在它的 `Configure` 建，產品自己的 `configure` 在所有單位之後跑、建在那裡後端找不到。哪一種由 SDL 那一批定。

```cpp
struct GameAudio {                       // 產品自己的型別（示意）
    GYO::Engine::Audio::Mixer mixer;
    std::map<std::string, GYO::Engine::Audio::Submitter> submitters;   // 鍵：World 名
};
// 建混音器的單位的 Configure：setup.Buffer().Create<GameAudio>(...)
// 每個 World 的 build：auto* audio = world.ProductBuffer().Find<GameAudio>();
//   同名的提交口沒有才 CreateSubmitter()；有就沿用
// world.Prepare(...)：在 World 裡放一個指向那個提交口的 Resource（例：AudioOut）
```

- **為什麼照名字**：提交口用掉不還（RC 5）。每次建 World 都拿新的，讀檔換 World 幾次就 `TooManySubmitters`。同一個名字沿用同一個，提交口的數量不隨替換次數長。
- **clip 不綁提交口**〔`v8-arch-gyo2-audio-clip-loader` 改：原「clip 格不跟著沿用」〕：每個播放帶它的 clip，舊 World 送出的聲音照響完，之後混音器在那個提交口下一次被呼叫時放掉它握的參照；替換時不必卸載什麼。
- **替換時的注意**：預設的 `BuildNewFirst` 在新 World 上跑準備函式與 `beforeSwap`，這時舊 World 還在、而且和新 World 共用同一個提交口。在那裡 `Play`、`Stop`、`StopAll`，影響的是共用的提交口：替換若之後失敗、留下舊 World，新 World 做過的事不復原——舊聲音可能已停。要停舊聲音，在替換成功之後做。
- **World 交給別的執行緒時**：指標跟著 World 走；換手由 runner 的交出同步（第 3 節的執行緒契約），不必另拿提交口。交出中的 World 能不能替換、它的提交口怎麼辦，在 World 交出的入口那一批定（O-13）。
- **根據**：O-15 的共用處「規則同 O-2 的每 World 那一份……但**不凍結**」，而且「每個 World 都比共用處先銷毀」——它比每個 World 活得久。提交口是連到 Product 一份服務的連線，不是 World 的狀態，放在那裡、替換時沿用，合 O-15 的用意。O-2 寫明：「登記單位以 World 名為鍵記的資料在 `Build` 時重設（不含 O-15 共用處裡跨 World 的物件——它們比每個 World 活得久，例：照 World 名沿用的提交口）」（`orchestration.zh-Hant.md` O-2，使用者 2026-11-22「O-2 照建議補」，`v8-arch-gyo2-o2-shared-note`）。

### 5.3 一個 System 在 `Main`／`Last` 交給提交口

表現層的 System 把這一幀要播、要停、要卸的請求放進 World 裡的緩衝；`Main`／`Last` 只有**一個** System 把緩衝交給提交口（O-12 的套用點「出」）。理由：提交口是單一生產者；集中到一個 System，Kernel 怎麼排其他 System 都只有一方在用。混音器在 World 外、自己同步（CCR-6），World 的資料不上鎖。

### 5.4 依時序變的值不進判定

`Play` 回的 `SoundHandle`、`Stop` 回的 `false` 都取決於音訊執行緒跑到哪裡（環滿不滿）。所以：

- 放進 World 的這些值、指向提交口的 Resource（指標）、請求緩衝，各自的雜湊函式**什麼都不寫**（照 asset 宣告 5.9.1 的 `AssetView`：依完成時序變的值不進 World 摘要；沒有雜湊函式的 Resource 會讓 `World::Summary` 回錯誤）。
- 不拿 `false`、handle 有沒有效去分支判定（CCR-6）。`PlayheadOf` 的回報只供顯示（DI 4）。它們只給表現層用（例：環滿就這一幀不再送）。

### 5.5 clip 交出去就可以放掉

〔`v8-arch-gyo2-audio-clip-loader` 改：原「先送出緩衝，再卸載 clip」。〕緩衝裡的請求帶它自己的 clip；作者放掉 Handle 或手上的 clip，不影響已送出或還在緩衝裡的播放。clip 什麼時候真的釋放，看最後一個持有者：asset 的 store（維護時）、作者，或混音器（那個提交口下一次被呼叫時）。

## 6. 音效的 Data Contract（`sound`／`wav`）

〔`v8-arch-gyo2-audio-clip-loader`，[宣告](audio-clip-loader-declaration.zh-Hant.md)第 5 節；格式規則與碼是卡片判，使用者 2026-11-23 動工前核准。〕

**誰產生、誰讀**：audio_editor 寫（搬回時照這裡）、Script、手工（任何能輸出 48 kHz PCM16 WAV 的軟體），一樣有效；遊戲經 asset 讀，執行期讀、不需要 build 的產物。工具要驗證自己的輸出就呼叫同一個 loader（`WavLoader`，只連 `GYO::Audio`），並自己比大小（大小由 store 判）。

**catalog 的項目**：

```json
{"id": "object_fps_pvp.audio.gun_shoot", "type": "sound", "path": "audio/gun_shoot.wav"}
```

- `type`：`sound`。`format`：`wav`（副檔名推得出，可省）。`settings`：沒有；寫了照收、忽略。

**大小**（不是 loader 的規則）：經 store 載入時先判，預設上限 64 MiB（48 kHz 立體聲約 5.8 分鐘；卡片判，沒有 Feature 推得出），產品可用 asset 的 `byteLimits` 依（`sound`、`wav`）覆寫；超過是 asset 的 `TooLarge`，loader 不被呼叫。直接呼叫 loader 時不判。

**檔案格式（`wav` 第 1 版）**：RIFF/WAVE，小端序。loader 照下表的順序判，回第一條違反的碼（`WavLoaderCode`；同時違反多條時回編號最小的；規則 2 對整串 chunk 走完才判規則 3）：

| # | 規則 | 碼 |
|---|---|---|
| 1 | 至少 12 位元組，開頭是 `RIFF`、第 8～11 位元組是 `WAVE` | `NotWave` |
| 2 | 從第 12 位元組起是一串 chunk（4 位元組 id＋4 位元組長度＋內容，長度是奇數時補 1 位元組）。每個 chunk 的內容都在檔案內；最後一個 chunk 的補位可以缺；最後剩不到 8 位元組的尾巴忽略。RIFF 檔頭的長度欄不查 | `Truncated` |
| 3a | `fmt ` 與 `data` 都至多一個；其他 chunk（`LIST` 等）跳過 | `DuplicateChunk` |
| 3b | `fmt ` 與 `data` 都至少一個（先後不限） | `MissingChunk` |
| 4 | `fmt ` 至少 16 位元組 | `BadFormatChunk` |
| 5 | 格式碼 1（整數 PCM）、每樣本 16 位元 | `NotPcm16` |
| 6 | block align 等於 `fmt ` 的聲道數 × 2（只比，不除） | `BadFormatChunk` |
| 7 | `data` 的長度是 2 的倍數 | `PartialSample` |
| 8 | `Clip::Make` 的規則（聲道數 1 或 2、取樣率 48000、`data` 非空、樣本數是聲道數的整數倍、frame 數 ≤ 2^32 − 1），detail 是 `Clip::Make` 的訊息 | `InvalidClip` |

- 取樣是有號 16 位元、小端序、聲道交錯，照位元組原樣變成 clip（不抖動、不換算），逐樣本組合（不依賴主機的位元組序）。`fmt ` 的 byte rate 不查。
- **Version**：WAV 沒有 GYO 的版本欄，版本記在這一節（今天第 1 版）。放寬（多收，例 `WAVE_FORMAT_EXTENSIBLE` 裡的 PCM16）時版本加一、`format` 名不變：舊執行檔拒收用到新功能的檔，內容與執行檔一起出貨、不另設協商；工具預設只寫第 1 版的寫法。收窄、或改變已收內容的意思，用新的 `format` 名登記另一個 loader（AS-9）。
- **標準寫法**（工具、腳本照這個寫）：44 位元組檔頭（`RIFF`、`WAVE`、16 位元組的 `fmt `、`data`）。
- **範例**：master 的 `player_footstep.wav`（`tests/common/audio/contract/`，origin/master 3b411e21 的同一個 git 物件）原樣載得了；其餘的規則由 `WavLoaderTests.cpp` 逐條以產生的位元組驗。

## Design Invariants

1. **中立。** `GYO::Audio` 只認得 clip、時間、增益與樣本：沒有事件種類、武器、角色、產品名，也沒有以產品意義命名的 API。守衛：審查（沒有自動守衛）。
2. **不含 SDL。** `GYO::Audio` 不含 SDL：它的連結恰好是 `GYO::Base`、`GYO::Time` 與 `GYO::Asset`〔`v8-arch-gyo2-audio-clip-loader` 加 `GYO::Asset`：loader 介面〕（設定時的檢查 `gyo_expect_audio_links`，`tests/common/audio/CMakeLists.txt`；突變 `au-link-engine`）。後端的不變量歸 SDL 那一批。
3. **一條時間線。** 播放與停止的請求帶 `Time::MonotonicClock` 的時間點。換成取樣位置只在混音器裡、只從目前 block 的 anchor 換；後端每個 block 取一次 `Now()`（這一半是後端的，v8-engine-audio-sdl，隨後端歸 SDL 那一批）；不跨 block 帶固定的偏移。守衛「maps a request in a later block from that block's anchor」（v8-engine-audio-schedule）。停止的時間點照起播的同一條規則換（偏移、取整、夾住與 `scheduleDelay`），從取到它的 block 的 anchor（v8-engine-audio-control-stop；守衛「maps a stop like a play from the block's anchor」）。〔`v8-arch-gyo2-audio-review` 加〕送進混音器的時間點（`PlayRequest::at`、`Stop`／`StopAll` 的 `at`）是同一行程的時間基準上的時刻；可縮放的遊戲時間不是混音器的輸入，遊戲的暫停與時間倍率不作用在混音器上。守衛：審查。
4. **混音器是播放位置的權威。** 只有混音器擁有已算的 frame 數與每個聲音在 clip 裡的位置。它發布它們（`Stats().renderedFrames`、`Mixer::PlayheadOf`）只供顯示與驗收；使用者與後端不保存它們的副本，也不拿它們做決定（例：播放位置停了就播下一個、播放位置回報 NotPlaying 就跳過停止）；決定經命令走，由混音器解（AGENTS §3，State Authority）。發布的位置最多舊一個 block、比聽到的聲音超前輸出延遲、不屬於別的聲音、不改輸出。守衛：審查（使用者；音訊執行緒寫看板、自己保存發布過的 tag，不讀看板）；「playhead board never reports another sound's state, in any interleaving」；「output does not depend on someone reading playheads」；「reports a sound scheduled, playing, then not playing」；「playheads stay consistent while voices are reused, block by block」；ThreadSanitizer 的輪詢「playheads stay consistent under a polling thread」（自己的 CTest `gyo_audio_playhead_stress`；voice 一直在重用；三種排程——自由、讀者先、混音者先——每種在時限內（20 s，ThreadSanitizer 下 60 s）都要達到 10^5 次和混音重疊的輪詢與 10^3 次讀到播放中的聲音，否則失敗；它守資料競爭與「每次單獨的讀都在 clip 之內」，不守同一個聲音的多次讀的先後——在 ThreadSanitizer 下輪詢撐不過那麼久）；「音訊執行緒不讀看板」由設計審查守〔原本的原始碼掃描 `gyo_audio_board_readers` 拿掉，使用者 2026-11-22 合併前核准，`v8-arch-gyo2-audio-review`；它綁檔名與寫法，見合併前核准頁第 1 節〕。看板只有原子變數，放寬它的任何記憶體順序 ThreadSanitizer 與這些測試都看不到（它們交錯的是循序一致的步驟）；那一半靠 `PlayheadBoard.hpp` 的順序論證與審查。
5. **每次起播是它自己取整的偏移。** 在一個 anchor 下取到、時間點在那個 anchor 一小時內的請求，從它離 anchor 的偏移取整到最近的 frame（floor(x + 1/2)，一半往 +∞，負的也一樣）加上固定延遲開始；所以兩個都不晚、時間點差 Δ 的請求，開始的 frame 差和 Δ × 48000 相差不到一個 frame。超過一小時的偏移夾在一小時。（2026-10-30 經使用者同意改寫，v8-engine-audio-invariant5；一小時與取整方向在 xhigh 檢查後加，v8-engine-audio-invariant5-clamp。）守衛：「starts a clip exactly at its start frame」「starts a clip scheduled in a later block at its frame」「output does not depend on how blocks split it」「starts a request at its offset from the block's anchor」「rounds an offset to the nearest frame, halves up」「starts each request at its own rounded offset under one anchor」「maps a request in a later block from that block's anchor」。
6. **帳對得起來。** 每個請求恰好歸入準時、晚、因環滿丟掉、因沒有 voice 丟掉之一；submitted 等於它們的和。沒有不記數的丟棄。守衛：「drops a play when every voice is busy」「starts a late play at the next frame」「drops and counts a request when its queue is full」「balances every request from concurrent producers」。每個停止請求（Stop 或 StopAll）恰好歸入 matched、unmatched、因環滿丟掉之一；stopsSubmitted 等於它們的和。停止不改它的播放怎麼被記。守衛：「drops and counts a stop when its queue is full」「counts a stop of a finished or dropped play as unmatched」「balances plays and stops from concurrent producers」，以及「plays and lets go of clips while another thread renders」之後的帳。〔`v8-arch-gyo2-audio-clip-loader` 拿掉卸載的帳。〕
7. **音訊執行緒即時安全。** `Render` 不配置、不釋放、不上鎖、不等待、不記日誌、不丟例外（`noexcept`；每個容量在建立時配好，含每個提交口的環、參照表與回收環、播放位置的項）。〔`v8-arch-gyo2-audio-clip-loader` 改〕clip 的取樣不可變（`Clip` 沒有改它的操作）；混音器為每個排著或播放中的請求在提交口這一側的參照表裡握一份參照，**音訊執行緒不握參照**（只有 view 與格號）：那個播放不會再讀取樣之後（沒有 voice，或 voice 已結束），音訊執行緒以 release 把格號推進回收環；使用提交口的一方在下一次呼叫或解構時 acquire 讀到、才清那一格，或隨混音器的狀態放掉。（原文 2026-10-31 經使用者同意改寫，v8-engine-audio-control-invariants；2026-11-23 動工前核准改成參照表。）守衛：「lets go of a clip on the submitter's next call after its play ended, never in Render」（六種結局；播放中的呼叫不放掉、輸出逐樣本相同；結束後的下一次呼叫放掉恰好一個 clip）、「lets go of the clips handed back on each of its calls」（五種呼叫）、「lets go of a play the full queue dropped in that call, and keeps every entry」「takes its table and both rings around twice」「hands an entry above 65535 back to its own submitter」「Render allocates and frees nothing」（`gyo_audio_alloc_tests`，在每條執行緒上數配置與釋放）、「plays every queued play fully after every other holder let go」、ThreadSanitizer 與 AddressSanitizer 下的「plays and lets go of clips while another thread renders」；審查（上鎖、等待、日誌）、`noexcept` 的簽名。
8. **決定性。** (a) 給定每個 voice 的起始與停止 frame，輸出不論怎麼切 block 都逐位元相同。(b) 同樣的 clip、同樣的命令在同樣的 block 取到、block 的長度與 anchor 相同，輸出逐位元相同。守衛：「output does not depend on how blocks split it」（block 1～2200，跨過 256 frame 的內部處理；每個請求在第一次 render 之前送出且準時；v8-engine-audio-core）、「stops a voice exactly at its stop frame whatever the block split」（v8-engine-audio-control-stop）。（2026-10-31 經使用者同意改寫，v8-engine-audio-control-invariants：原本的「同樣的請求序列……不論怎麼切」對晚到的請求不成立，它的起點取決於取到它的 block。）一次性的行為由「renders a recorded one-shot sequence bit-identically」釘住（重建前錄下的雜湊，v8-engine-audio-control-baseline）。
9. **固定的輸出格式。** 混音器只輸出 48 kHz 立體聲 PCM16；其他取樣率的 clip 拒絕、從不重新取樣。守衛：「Clip rejects clips the mixer cannot play」。
10. **停止是一個 frame，有先後、有範圍。** 在開始前停止、停止 frame 在起點或之前的 voice 不出聲。停在之後的 frame 的 voice 恰好貢獻 [start, min(start + clip frames, stop)) 的 frame。已開始、停在已算過的 frame 的 voice，在取到停止的 block 的第一個 frame 結束，記成晚。停止只影響同一個提交口之前送出的聲音；提交口停不了別人的聲音。守衛：「stops a sound at the frame its time maps to」「stops a voice exactly at its stop frame whatever the block split」「silences a sound stopped before its start and frees its voice」「applies a late stop of a started sound at the block's first frame」「StopAll stops only its own submitter's sounds」。（`audio.md` 經 SDL 後端的守衛 `audio_sdl.disk` 隨後端移出。）
11. **handle 不混淆。** `SoundHandle` 在混音器的一生中不重用；停止或播放位置查詢不影響、不回報別的播放。守衛：「never lets a stale handle stop a newer sound in the same voice」「returns a unique handle per queued play and none for a dropped one」「gives the last of 64 submitters valid handles」。〔`v8-arch-gyo2-audio-clip-loader` 拿掉執行中 `ClipId` 的世代一句與它的兩個守衛：`ClipId` 不在了。〕
12. **音效的 Data Contract。** 〔`v8-arch-gyo2-audio-clip-loader` 新增〕`sound`／`wav` 的格式、Version 與 Validation Rule 在第 6 節；手寫、腳本、工具產生的一樣有效；loader 是純函式（沒有 World、時鐘、全域可變狀態），只連 `GYO::Audio` 就能呼叫。守衛：`WavLoaderTests.cpp` 的每條規則各一案（每個條件）、規則的先後五案、「loads master's content as it was」「gives the same clip for the same bytes, on any thread (AS-3)」「through the asset store: loaded, shared, played; and the size limit」。

### Replacement Criteria

下列任一成立時，不再修補，交使用者決定修還是重建。

1. 需要串流或壓縮格式；或 clip 的記憶體必須在音訊執行緒不跑時回收，或以位元組數設上限（含：混音器只借 asset 的記憶體、不共有它）。〔`v8-arch-gyo2-audio-clip-loader` 改：拿掉「clip 必須由擁有它的提交口以外的提交口載入、卸載或播放」——任何提交口都能播任何 clip。〕
2. 需要每個 voice 的增益與硬停止以外的訊號處理（bus、effect、HRTF、淡入淡出），或聲音送出後除了停止以外的改變（增益或音高、seek、暫停、loop、音樂）〔`v8-arch-gyo2-audio-review` 補：含聲音跟著遊戲的暫停或時間倍率走〕。
3. 需要第二個、非 SDL 的後端，而且換算要對每個後端重新驗證。
4. L2 量到的 anchor 抖動讓間隔誤差（超出 DI 5）對使用者不可接受，換算需要濾波，而濾波需要後端專屬的資訊。
5. 〔`v8-arch-gyo2-audio-review` 改〕生產者的**數量**在建立時定不下來（例：每次替換 World 都要新的提交口），或要多個生產者同時用一個提交口。**2026-11-22 判定已成立**（O-8 的替換：每次建 World 拿新的提交口會用光），使用者選修（宣告第 12 節第 1 題）。處理：混音器不改，由作者的慣例收掉（第 5.2 節：照 World 名字建一次、替換時沿用）；這個慣例碰到 O-2 字面與交出中的 World 的兩點見第 5.2 節，條件之後再成立（例：生產者的數量真的定不下來）時照本條交使用者。
6. 每個新的播放功能（含每種新的命令）都要重新驗證換算、晚到的規則、環的順序、clip 的回收或即時安全。
7. 使用者需要精確的播放位置（取樣精確或聽得到的、校正過裝置延遲的）、需要拿它做決定，或需要聲音結束時被通知。
8. 停止必須是立即的（不換算）或跨提交口，或需要混音器全體的停止。
9. 〔`v8-arch-gyo2-audio-review` 新增〕點名的 Feature 需要搶占（優先度、音量、距離或最舊）或虛擬 voice。（Unity、SoLoud 搶占，Kira 和這裡一樣不搶占；宣告第 0 節。）
10. 〔`v8-arch-gyo2-audio-clip-loader` 新增〕「混音器握的參照最晚在下一次呼叫時放掉」讓記憶體留得太久而超出預算，或作者需要不經 `Play`／`Stop`／`StopAll` 也能讓混音器放掉。

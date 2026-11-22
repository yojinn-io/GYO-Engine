# 重審報告與事前宣告：audio（`v8-arch-gyo2-audio-review`）

更新：2026-11-22（修訂 0，草稿）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-audio-review`）。**狀態：草稿，待敵對式檢查，之後交使用者動工前核准。使用者說「開始」之前不改程式。**

**性質**：批次表第 6 批「audio 重審」的重審報告＋事前宣告。`engine/modules/audio` 是 Engine 的公開 Interface（產品、工具、之後的 SDL 後端都接在上面），照 AGENTS §2 當**基盤**處理：積上去的是 SDL 的輸出後端、之後的 `world_audio`、音效的 loader、audio_editor 的預覽、每個產品的聲音。

**用詞**：標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。只把「推翻舊決定」或「和別家不同」的地方放進第 12 節交使用者。別家的事實標出處：本機原始碼寫路徑；網頁寫 URL；**〔知〕**＝憑記憶、這次沒讀到官方頁（FMOD、Wwise 的官方文件頁這次讀不到：FMOD 回空白頁、Wwise 回 403），不當決勝的唯一根據。

**核心一句**（每一項都拿來對照）：**混音器是 World 外、Product 一份的服務，只提供機制：在指定的時間點起播、停下、混音、計數；聲音什麼時候該響、遊戲暫停時聲音怎麼辦、滿了先丟誰，是作者的規則。**

---

## 0. 根據與讀的版本

- 程式：`origin/gyo2` eacab4d7（含 #270 交換點、#272 生成世界）的 `engine/modules/audio`（`Mixer`、`Submitter`、`ClipBank`、`VoiceMixer`、`PlayheadBoard`）與 `tests/common/audio`（82 個 TEST_CASE、3 個 CTest 執行檔與 1 個原始碼掃描）。
- 合約文件：`docs/architecture/audio.md`（master 時代的英文頁，R0 照帶，Design Invariants 1～11、Replacement Criteria 1～8）；突變清單 `docs/architecture/plans/audio/scripts/mutations*.json`（95＋1＋7 條）。
- 批次表 `rebuild-batch-plan.zh-Hant.md` 第 64 行「6. audio 重審」（文件分支 `claude/gyo2-time-design` 1fca1420）。
- R0 清單附錄 A.1「audio 的 core」列、第 9 節（未查證：Submitter 與交換點的相容性）。
- 概念層 `rebuild-concept.zh-Hant.md` 2.1（audio core：**不相合，要處理**）、3.7。
- 編排層 `orchestration.zh-Hant.md` O-2、O-3、O-12、O-13、O-15；交換點宣告 `exchange-declaration.zh-Hant.md` 5.1～5.3、5.6 第 372 行（「`Submitter` 綁執行緒、自己記；World 交出到別的執行緒時要在那條執行緒上另拿一個——audio 重審要對的點」）、E-16。
- HANDOFF `v8-arch-gyo2-thread-binding-by-runner`（使用者 11-16：「別人掉過的坑，我沒必要在跳一次。」→ World 不綁執行緒；需要綁執行緒的 Resource 自己記來源執行緒）。
- `engine/modules/world_time`：`TimeTypes.hpp`（`RealTime` 不受暫停、倍率、上限影響；`VirtualTime`；`Resimulation`）、`Sampling.hpp`（`TimeBase`／`ManualDelta`／`ManualInstant`）；`world-time.zh-Hant.md`。
- asset 宣告（分支 `claude/gyo2-asset-585196`，55a14825 起、1b120a86 動工前核准，未合併）：1、2.2（「音效（audio 重審那批）」）、5.4（`AssetStore` 每 Product 一份、World 只持 id 與 Handle）、5.16（loader 介面）、AS-10（值不可變）。
- master `engine/audio`：只看當時為什麼這樣做（`docs/architecture.md:357-359` 的兩段：遊戲要在事件發生的時刻出聲；工具要預覽、停、換 clip）。不拿 master 的數據當設計。
- 別家：
  - Bevy（本機 `~/Code/GitHome/bevy/crates/bevy_audio`）：`lib.rs:82-118`（播放的 System 全在 `PostUpdate`）、`audio_output.rs:16-31`（輸出裝置是 World 的 Resource）、`audio_output.rs:84-`（`play_queued_audio_system` 讀 `Assets<Source>`，載好才播）、`audio_source.rs:8-20`（`AudioSource { bytes: Arc<[u8]> }`，播放時共用、不複製）、`audio.rs:35-45`（`PlaybackSettings` 的 `speed`、`paused` 是每個 sink 的）。
  - Kira 0.12.6（docs.rs）：`StartTime::{Immediate, Delayed, ClockTime}`；時鐘可 `pause`、`set_speed`，聲音可以排在時鐘的 tick 上；`PlaySoundError::SoundLimitReached`（滿了回錯誤，沒有搶占）；`StaticSoundData.frames: Arc<[Frame]>`（「audio data is shared among all clones」）；`AudioManager` 在後端 `Send` 時是 `Send`，`play` 取 `&mut self`；`assert_no_alloc` 功能（音訊執行緒配置或釋放記憶體就 panic）。
  - Unity（docs.unity.com）：`AudioSource.PlayScheduled` 用 `AudioSettings.dspTime`（音訊系統自己的時間線）；`AudioListener.pause` 暫停全部、凍結 dspTime，`ignoreListenerPause` 可例外；`priority` 0～256，最低的先虛擬化；Max Real Voices／Max Virtual Voices。`Time.timeScale` 的頁沒說會影響聲音。
  - FMOD Core〔知〕：`ChannelControl::setDelay` 用 DSP 時鐘（取樣數）；虛擬 voice 依優先度與音量選真 voice；軟體 channel 數在 init 定。
  - Wwise〔知〕：`PostEvent` 可從任何執行緒呼叫、進佇列，`RenderAudio()` 每幀送出一次；Actor-Mixer 的播放上限與「優先度相同時丟最舊／最新」、虛擬 voice。

## 1. 先用遊戲講

**FPS 的 Client 開槍。** 第 312 幀，畫面 World 的 System 判定「玩家開了一槍」。它不直接碰混音器：它把「播 `sfx.shot`、左右增益 1.0、時間點＝這一幀的取樣時刻」放進 World 裡的請求。這一幀最後的套用點（`Main`／`Last`），一個 System 把這一幀所有的請求交給混音器。混音器在音訊執行緒上，把「時間點」換成第幾個取樣開始響；畫面卡了 20 ms 也照原本的時刻響（`scheduleDelay` 吃掉這段延遲）。

**單機遊戲按暫停。** World 的可縮放時間停了，模擬的 System 不跑，所以不會再有新的聲音。已經在響的爆炸聲照響到完——這和 Unity 預設（`timeScale` 不碰聲音）、Bevy（虛擬時間不碰 sink）相同。要「暫停時聲音也停」，是遊戲的規則，今天的混音器做不到（沒有暫停與變速），這個需求一出現就觸發 Replacement Criteria 2。

**讀檔換 World。** 玩家讀第 2 個存檔，World 被替換（O-8）。新的 World 要繼續出聲，但混音器只有 64 個提交口、用掉不還——每次換 World 拿一個新的，讀檔 64 次就用光。所以提交口要跟著「World 的名字」走，不跟著 World 的實例（第 3.2 節）。

**Client 的模擬 World 交給別的執行緒（O-13）。** 模擬 World 被 runner 整個交出去。它手上的提交口跟著它走；今天的合約說「提交口歸第一個呼叫它的執行緒」，交出去之後一呼叫就斷言。第 3.2 節判這個綁定不該存在。

**音效從哪來。** 進對局前 asset 在背景載好 `sfx.shot`（不可變、在 World 外的 `AssetStore`）。混音器今天另有一個「出聲前先登記好」的 `ClipBank`，可是混音器是 Product 一份、在 `Configure` 就建好，那時音效還沒載——`ClipBank` 在 GYO2 的流程裡填不進東西。第 3.3 節判它拿掉，音效一律在執行中載入，且不再複製一份。

**audio_editor 預覽。** 工具轉好一個 clip、播出來、拖動時停掉、畫出播放到哪裡。這條（停、執行中載入、播放位置）是 master 的 control 層為它做的，留下。

## 2. 重審結果總表

| 部件 | 判定 | 一句理由 | 別家 |
|---|---|---|---|
| `VoiceMixer`（取樣域的核心：固定數量的 voice、Q16 整數混音、飽和、晚到的規則） | **留** | 機制；整數混音讓三平台逐位元相同，成本為零（determinism.md 4.5） | Kira 固定容量；FMOD／Unity 有固定的真 voice 數〔知〕 |
| 時間點到取樣位置的換算（`Render(block, anchor)`、`scheduleDelay`） | **留**，合約補「World 那一側用哪個時間」 | 混音器在 World 外、輸出是真實時間；暫停與慢動作是 World 的可縮放時間（第 3.1 節） | Unity 用 dspTime、FMOD 用 DSP 時鐘：都是音訊那一側自己的時間線，不是遊戲時間 |
| `PlayRequest::at` 的型別（`Time::TimePoint`，時間基準） | **留** | 和 `RealTime::now` 是同一條時間基準（`Sampling.hpp`） | 同上 |
| `Submitter`（每個生產者一個 SPSC 環） | **改**：拿掉「歸第一個呼叫它的執行緒」，改成「同一時間只有一方用；換手要有先後關係」 | 綁定不是 SPSC 需要的；它擋住 World 交出（O-13）與執行器換執行緒（第 3.2 節） | Kira：`Send`、方法取 `&mut self`（可以搬、不能同時用） |
| `Submitter` 用掉不還 | **留**，`world_audio` 照 World 的名字各拿一個、替換時沿用 | 還回去要重新驗證環的順序與 clip 的回收（RC 6） | — |
| `ClipBank`（出聲前登記、混音器擁有） | **拿掉**（交使用者，第 12 節第 1 題） | GYO2 裡混音器先建、音效後載，填不進東西；和執行中載入是同一件事的兩條路 | Bevy、Kira、Unity 都沒有「出聲前的庫」，音效是一般的資產或資料 |
| `Submitter::LoadClip` 複製樣本 | **改**：吃一個不可變、共用樣本的 `Clip`，不複製（同第 1 題） | asset 已經持有不可變的值（AS-10）；混音器再複製一份是兩倍記憶體 | Bevy `Arc<[u8]>`、Kira `Arc<[Frame]>` 都共用 |
| clip 的租約與交還（音訊執行緒不釋放記憶體） | **留** | DI 7 的核心；改成共用之後，釋放仍只在擁有它的那一方的執行緒 | Kira `assert_no_alloc` 同一個原則 |
| `SoundHandle`、`Stop`、`StopAll` | **留** | audio_editor 的預覽要停；handle 不重用（DI 11） | Kira handle、FMOD Channel〔知〕 |
| `PlayheadBoard`、`PlayheadOf` | **留** | audio_editor 要畫播放位置；只供顯示（DI 4） | — |
| `CheckBoardReaders.cmake`（原始碼掃描：只准 `Mixer::PlayheadOf` 讀看板） | **拿掉**（交使用者，第 12 節第 3 題：DI 守衛照 §14.4 要核准） | 綁實作方式的守衛（AGENTS §3）；讀看板不影響即時安全，DI 4 改由審查守 | — |
| `MixerStats` 計數與「帳要對得起來」（DI 6） | **留** | 機制；丟了要記 | — |
| `AudioErrorCode::DeviceUnavailable` | **搬**到 SDL 那批的後端自己的錯誤碼 | audio 本身沒有地方用它；它是後端的錯誤 | — |
| 滿了就丟新來的（不搶占） | **留**（交使用者，第 12 節第 4 題：和 Unity／FMOD／Wwise 不同） | 搶占是規則（看優先度、音量、距離）；沒有點名的 Feature 要它 | Kira 同（回錯誤）；Unity、FMOD、Wwise 有優先度與虛擬 voice |
| 主增益 `SetMasterGain` | **留** | 機制（Bevy 的 `GlobalVolume` 也在引擎） | Bevy `GlobalVolume` |
| `scheduleDelay` 預設 60 ms | **留欄位**；預設值的來源是 master 在 SDL macOS 的量測，SDL 那批重新推導 | 沒有輸出後端就沒有量測的依據 | — |
| `AudioFormat`（48 kHz 立體聲 PCM16、`kMaxGain`） | **留** | 單一格式是 DI 9；轉格式歸後端與工具 | Bevy 交給 rodio 轉 |
| 突變清單裡 find 指向已刪檔案的 12 條（SDL 10、SoundLoader 1、SDL 的 render 1） | **退役** | 檔案不在 gyo2；SDL 那批重寫自己的 | — |
| `docs/architecture/audio.md` | **改寫**（英文，原地） | 拿掉 SDL 後端的段落、master 的批次與計畫連結；DI 與 RC 照第 7 節改 | — |

## 3. 四個重審點的判定

### 3.1 `PlayRequest::at` 與 World 的暫停、慢動作

**判定**：**留**時間基準（`Time::MonotonicClock`）當混音器的時間線；概念層 2.1 說的「不相合」，處理方式是**把「World 那一側拿哪個時間」寫進合約**，不是讓混音器認得 World 的時間。

**理由**：
1. 混音器在 World 外（O-15 的共用處），輸出是真實時間；音訊執行緒每個 block 用時間基準取一次 anchor（DI 3）。World 的可縮放時間（`VirtualTime`）暫停時不前進、慢動作時變慢，把它當起播時間點，暫停時一個請求會被排到永遠不到的未來。
2. World 裡本來就有同一條時間基準的值：`RealTime::now`＝這一幀的取樣時刻（`TimeTypes.hpp`：「Not affected by pause, scale or the caps」）。World 的 System 用它當 `at`，不自己讀 `Now()`——這正是 K8a「時間是可替換的 Resource」要的：時鐘只讀一次、在 world_time 讀。
3. 別家都這樣切：Unity 的 `PlayScheduled` 用音訊系統自己的 dspTime，`timeScale` 的頁沒提聲音；FMOD 的 `setDelay` 用 DSP 時鐘〔知〕；Bevy 的虛擬時間不碰 sink；Kira 的時鐘是**音訊那一側**的物件，可以暫停、變速，遊戲要讓聲音跟著遊戲時間走，就自己去暫停那個時鐘。沒有一家讓遊戲的時間倍率自動作用在混音器上。

**暫停與慢動作的處理**（卡片照 L17 判）：
- 暫停時，模擬的 System 不跑，就沒有新的聲音；已送出的聲音照響。這是預設，不是缺陷。
- 「暫停時聲音也停」「慢動作時聲音也變慢」是遊戲的規則。今天的混音器沒有暫停與變速（RC 2 已列）；點名的 Feature 一出現，照 RC 2 交使用者決定加在混音器（Unity 的 listener pause、Kira 的時鐘是兩種先例）。
- 一幀裡跑多個固定步時（快轉、補幀），這些步的聲音都拿到同一個 `RealTime::now`，會擠在同一刻。要分開的作者自己算偏移（`at` 可以是任何時間點）；`world_audio` 要不要給「相對這一幀的偏移」，在它那一批定。

**限界**（寫進合約）：`RealTime::now` 只在 `TimeBase` 取樣時和真實輸出的 anchor 是同一條時間線。`ManualDelta`／`ManualInstant`（測試）時，`Render` 的 anchor 也要用同一條手動的時間線（離線測試本來就自己傳 anchor）。真實輸出配手動取樣，請求會被當成晚到或排到一小時後（上限）；那是組合錯誤，不加守衛，寫在合約。

### 3.2 提交介面的執行緒契約與交換點

**判定**：
1. **混音器是 Product 一份的物件**，由之後的 `world_audio` 在 `Configure` 建進共用處（O-15）。同步是混音器自己的事（它本來就是 SPSC 環＋原子，CCR-6）。
2. **World 交給混音器的路線是套用點的「出」**（`Main`／`Last`）：作者的 System 把請求放進 World 裡的請求緩衝；`world_audio` 的一個套用 System 在最後把這一幀的請求交給這個 World 的 `Submitter`。不用交換函式、不用 runner 直接讀寫。
3. **`Submitter` 不綁執行緒**：拿掉「第一個呼叫它的執行緒擁有它、別的執行緒呼叫是 Programmer Error」。新契約：同一時間只有一方呼叫；換到別的執行緒時，換手本身要有先後關係（join、鎖、World 交出時 runner 的同步），和任何不保證執行緒安全的物件一樣。**這推翻交換點宣告 5.6 第 372 行的寫法**（交使用者，第 12 節第 2 題）。
4. **`Submitter` 照 World 的名字各一個**，`world_audio` 拿一次、存在 Product 一份的地方、替換 World 時沿用；World 裡的 Resource 放指向它的指標（O-15：「System 要用的由擁有的 Module 在準備函式放一個指向它的 World Resource」）。

**理由**：
- 為什麼是套用點：O-12 的規則——「Module 擁有的外部服務在幀裡進出 World 的預設：要和 System 排先後、要跟著生成的每一批走、或由很多 System 讀的，用它」。播放請求由很多 System 發、要排在它們之後，正是這一種。交換函式是給「要整個 World、在 Run 之外」的事。別家：Bevy 的播放 System 全在 `PostUpdate`（`bevy_audio/src/lib.rs:85-118`）；Wwise 的 `RenderAudio()` 每幀送一次〔知〕。
- 為什麼一個套用 System 交、作者的 System 不直接呼叫：SPSC 只能有一個生產者。一個 World 裡多個 System 直接呼叫，Kernel 解除 LIM-2（並行執行器）後就是多個生產者；集中到一個套用 System，Kernel 怎麼排都只有一方在用。
- 為什麼拿掉執行緒綁定：
  - SPSC 要的是「同一時間一個生產者，前後兩次呼叫有先後關係」，不是「永遠同一條執行緒」。綁定是比需要更強的檢查，用途是抓誤用（使用者原則：不為防誤用加守衛）。
  - 它和三件已定的事衝突：World 不綁執行緒（`-thread-binding-by-runner`）；World 整個交出時只有那條執行緒碰它（O-13 (2)）——提交口跟著 World 走，交出後第一次呼叫就斷言；用掉不還——「在那條執行緒上另拿一個」每交出一次耗一個，64 個用完就 `TooManySubmitters`。
  - `-thread-binding-by-runner` 說「需要綁執行緒的 Resource 自己記來源執行緒」。需要綁的是真的只能在某條執行緒用的東西（視窗、繪圖 context；Bevy 放 `thread_local!`）。SPSC 的生產端不在此列。
  - 先例：Kira 的 `AudioManager` 是 `Send`、`play` 取 `&mut self`——可以搬到別的執行緒，不能同時用。
- 為什麼照 World 的名字：讀檔換 World（O-8）每次重走 `Build`；每次拿新的，替換 64 次就用光。沿用同一個，`BuildNewFirst` 期間舊、新 World 短暫並存也不衝突（同一條 Product 執行緒、不同時 Run）。

**和 O-2 的一個待對齊處**（交 `world_audio` 那批）：O-2 寫「登記單位以 World 名為鍵記的資料在 `Build` 時重設」。提交口是 Product 一份的連線，不是那個 World 實例的資料；`world_audio` 的宣告要寫明它不屬於「重設」的範圍，或改由共用處裡混音器的持有者照 World 名字發放。

**重跑（回滾）**：回滾重跑時作者的 System 會再發一次同樣的聲音。要不要在重跑中發，是作者的規則：World 裡已有「重跑中」`Resimulation`（world_time 登記、回滾策略寫）。`world_audio` 不替作者過濾（Unity Netcode 的 `IsFirstTimeFullyPredictingTick` 也是交給作者判〔知〕）。

### 3.3 「Engine 只提供機制，會被替換的規則外移」適不適用

**判定**：適用，而且今天的 audio **大致已經只剩機制**；要外移的規則不在程式裡，只要寫清楚邊界。逐項分：

| 項目 | 機制還是規則 | 處置 |
|---|---|---|
| 混音（Q16 增益、int64 相加、主增益、四捨五入、飽和、計數） | 機制：誰用都一樣的算法 | 留 |
| voice 有上限（固定容量、建立時配好） | 機制：音訊執行緒不配置記憶體的前提 | 留 |
| 滿了丟新來的 | 機制的最小行為（不判誰重要） | 留；搶占（優先度、音量、距離、最舊）是規則，點名的 Feature 出現時加擴充點（新 RC 9） |
| 方向、距離換成左右增益 | 規則 | 已在外面（`PlayRequest` 只收增益） |
| 哪個事件播哪個聲音 | 規則 | 已在外面（master 的 event-sounds 契約在 framework／產品） |
| 暫停、慢動作要不要帶著聲音 | 規則 | 作者的事；混音器沒有暫停（RC 2） |
| 重跑中要不要發聲 | 規則 | 作者的事（3.2） |
| bus、effect、淡入淡出 | 會被替換的規則（11-13 的例子） | 沒有；RC 2 照舊 |
| 時間點換取樣位置、`scheduleDelay` | 機制（值可設定） | 留 |

**11-13 的方向和 11-16 的定義怎麼對**：11-13 使用者選「audio core 做成 plugin，規則不在 Engine 裡判斷」；11-16 改定義後 audio 是 Module（自己維護、介面是我們的），放 `engine/modules/audio`，產品可以不帶。11-13 要的「可替換」由「Module 本身可以不帶、可以換」滿足；Module 裡不放產品的規則（DI 1）。

### 3.4 SDL 的輸出後端：audio 這一側要的介面

後端不在本批（歸 SDL 那批）。audio 這一側給它、要求它的（寫進合約；卡片照 L17 判）：
1. **給**：`Mixer::Render(Span<int16_t>, TimePoint anchor) noexcept`，在後端的音訊執行緒上、一次一個 block；`AudioFormat` 的格式常數；`Mixer::Stats()`。
2. **要求**：每個 block 用時間基準取一次 anchor（DI 3）；只有一條執行緒呼叫 `Render`；混音器銷毀前，後端已停止呼叫 `Render`——在 GYO2 裡，後端在 `world_audio` 的 `OnDestroy` 回呼裡停（O-15：回呼在所有 World 之後、共用處之前，回呼返回時工作必須已停）。
3. **改變**：master 的 `SdlAudioOutput::Open(Mixer&&)` 拿走混音器的所有權；GYO2 的混音器在共用處，後端改成持有參照或共用所有權，形狀由 SDL 那批定。`DeviceUnavailable` 歸後端自己的錯誤碼。
4. **不改**：anchor log、callback 統計是後端的，跟著後端走。

## 4. 別家怎麼做（匯總）

| 決定 | Bevy | Kira | Unity | FMOD〔知〕 | Wwise〔知〕 | 本頁 |
|---|---|---|---|---|---|---|
| 起播的時間線 | 立刻播（沒有排程） | 立刻、延遲、或音訊那側的時鐘 | dspTime（音訊系統的時間） | DSP 時鐘 | 立刻（事件） | 時間基準＋每 block 的 anchor（留） |
| 遊戲暫停、倍率碰不碰聲音 | 不碰（sink 自己的 `paused`） | 不碰；作者暫停時鐘 | `timeScale` 不提聲音；`AudioListener.pause` 另開 | 不碰；作者暫停 channel group | 不碰；作者發暫停事件 | 不碰（留）；暫停是規則（RC 2） |
| 遊戲送請求的時點 | `PostUpdate` 的 System | 呼叫當下進命令佇列 | 主執行緒呼叫當下 | 呼叫當下 | `PostEvent` 進佇列、`RenderAudio` 每幀送 | 套用點的「出」一次送（新） |
| 提交端和執行緒 | System 在 World 裡 | `Send`、`&mut self` | 只限主執行緒 | 執行緒安全 | 執行緒安全 | 可搬、不同時用（改） |
| 聲音資料 | 資產 `Arc<[u8]>`，共用 | `Arc<[Frame]>`，共用 | `AudioClip` 物件 | `Sound` 物件 | SoundBank | 不可變的 `Clip`、共用（改） |
| voice 上限、滿了 | 沒有上限 | 有上限，滿了回錯誤 | 真 voice 上限，低優先度虛擬化 | 優先度＋音量選真 voice | 每個 Actor-Mixer 的上限與丟棄規則 | 有上限，滿了丟新的、記數（留） |

**共通**：起播的時間線都在音訊那一側；遊戲時間不自動作用在聲音上；聲音資料不可變、共用。**分歧**：搶占（Kira、本頁不做；Unity、FMOD、Wwise 做）、提交端的執行緒模型。**本頁的位置**：時間線與資料照共通；提交端照 Kira（GYO2 的 World 不上鎖、生產端單一）；搶占照 Kira（第 12 節第 4 題）。

## 5. 設計（Contract）

改完的公開介面（名字卡片照 N15 判；實作時可以微調，不改語意）：

```cpp
namespace GYO::Engine::Audio {

// 不可變的 48 kHz PCM16 clip，樣本共用：複製 Clip 不複製樣本。
// asset 的音效 loader（之後）產出它；離線測試與工具直接建。
class Clip final {
public:
    // InvalidClip：空的、聲道不是 1 或 2、樣本數不是整數個 frame、取樣率不是 kSampleRate。
    // 這裡複製一次樣本，之後的複製都共用。
    [[nodiscard]] static Base::Result<Clip, AudioError> Create(Base::ConstSpan<std::int16_t> interleaved,
                                                               std::uint16_t channels, std::uint32_t sampleRate);
    [[nodiscard]] std::uint16_t Channels() const noexcept;
    [[nodiscard]] std::uint32_t Frames() const noexcept;
    [[nodiscard]] Base::ConstSpan<std::int16_t> Samples() const noexcept;
private:
    std::shared_ptr<const std::vector<std::int16_t>> samples_;
    std::uint16_t channels_ = 0;
};

class Submitter final {
public:
    // 同一時間只有一方呼叫下面的成員；換到別的執行緒時，換手要有先後關係。
    SoundHandle Play(const PlayRequest& request);
    [[nodiscard]] bool Stop(SoundHandle sound, Time::TimePoint at);
    [[nodiscard]] bool StopAll(Time::TimePoint at);
    // 把 clip 放進這個提交口的一格（共用樣本，不複製）。NoClipSlot：每一格都載著或還沒交還。
    [[nodiscard]] Base::Result<ClipId, AudioError> LoadClip(const Clip& clip);
    [[nodiscard]] bool UnloadClip(ClipId clip);
};

class Mixer final {
public:
    [[nodiscard]] static Base::Result<Mixer, AudioError> Create(const MixerConfig& config);
    // 其餘不變：CreateSubmitter、SetMasterGain、Stats、PlayheadOf、Render。
};

}
```

- **拿掉**：`ClipBank`、`Mixer::Create` 的 `ClipBank&&` 參數、bank 的 id 範圍（`ClipId` 全部是執行中載入的 id）、`ClipValidation.hpp`（規則併進 `Clip::Create`，一個地方）、`Submitter::ClaimOwner` 與 `SubmitterSlot::owner`、`AudioErrorCode::DeviceUnavailable`。
- **共用與即時安全**：格子持有一份 `Clip`（`shared_ptr` 的一個參照）；音訊執行緒只經格子裡的 `ClipView`（裸指標與長度）讀，不碰參照計數。交還照舊：音訊執行緒取到卸載、沒有 voice 在讀時 release store 交還；擁有那個提交口的一方下次 `LoadClip` 時 acquire load 看到，才放掉那份 `Clip`。樣本的記憶體在最後一個持有者（asset 的 store 或提交口）放掉時釋放，永遠不在音訊執行緒（DI 7）。
- **不變**：取樣域的混音、換算的式子、晚到與停止的規則、handle、看板、計數、所有容量在建立時配好。輸出逐位元不變（第 10 節判定 1）。
- **給 `world_audio`（之後一批）的契約**（只寫形狀；本批不實作，理由見第 9 節）：`Configure` 建混音器進共用處；照 World 名字各一個 `Submitter`、替換時沿用；請求緩衝＋`Main`／`Last` 的一個套用 System；`at` 用 `RealTime::now`；不過濾重跑；不帶暫停；音效的 loader 產出 `Clip`、登記進 asset（asset 5.16），型別特性與 loader 放在只依賴 asset 的函式庫，混音器的 `GYO::Audio` 照舊只連 Base 與 Time。請求緩衝用保留式訊息、實體（Bevy 的 `AudioPlayer`）還是 Resource，在那一批照別家比過再定。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：批次表第 6 批（R0 標「帶過去並重審」）；概念層 2.1 判「不相合，要處理」；交換點宣告 E-16 與 5.6 要 audio 重審對齊。直接支撐的 Feature：①②③ 的遊戲聲音（FPS 的 Client 開槍、單機的音效）、③ audio_editor 的預覽（停、執行中載入、播放位置）。
2. **現在的問題**：(a) 提交口綁執行緒，和 World 不綁執行緒、World 整個交出、用掉不還三件事衝突；(b) `ClipBank` 在「混音器先建、音效後載」的流程裡填不進東西，和執行中載入重複；(c) 執行中載入複製樣本，和 asset 持有的不可變值重複；(d) World 那一側該用哪個時間沒寫，World 的 System 可能直接讀 `Now()` 或拿可縮放的時間。
3. **變的邊界**：`GYO::Audio` 的公開 Interface（`Clip` 新增、`ClipBank` 拿掉、`Mixer::Create`、`Submitter::LoadClip` 的參數、`Submitter` 的執行緒契約、錯誤碼少一個）。
4. **影響的 Module／Product／Tool**：audio 自己與它的測試。gyo2 上沒有別的使用者（沒有產品、沒有後端）。之後的 SDL 後端、`world_audio`、音效 loader、audio_editor 照新的 Interface 寫。
5. **依賴方向**：不變（`GYO::Audio` → `GYO::Base`、`GYO::Time`；`gyo_expect_audio_links` 照舊）。
6. **Ownership**：clip 的樣本從「混音器擁有的複本」變成「共用的不可變值」；釋放的執行緒規則不變（不在音訊執行緒）。提交口從「綁一條執行緒」變成「同一時間一個使用者」。
7. **為什麼不能更小**：(a) 只放寬斷言、不改合約，等於留一條寫著卻不守的規則；(b) 留 `ClipBank` 當「測試用的捷徑」就是同一件事兩條路，之後每加一種命令都要驗兩條（RC 6）；(c) 不改複製，asset 接上來時每個音效兩份記憶體、兩套有效期。

## 7. Design Invariants 與 Replacement Criteria 的改動（`docs/architecture/audio.md`）

**改寫的 Design Invariants**（英文頁，原地改；條號不變，方便 `guards` 追蹤）：
- **DI 2**：只留 audio 這一側——`GYO::Audio` 不含 SDL，連結恰好是 `GYO::Base` 與 `GYO::Time`（守衛 `gyo_expect_audio_links`）。後端的部分（platform-sdl 的不變量、G1～G3）移到 SDL 那批的合約。
- **DI 3**：加一句：World 裡發出的請求，時間點取自 world_time 的 `RealTime::now`（這一幀的取樣），不取可縮放的時間，也不自己讀時間基準；暫停與倍率不作用在混音器上（第 3.1 節）。
- **DI 4**：守衛拿掉「`gyo_audio_board_readers`」一項（第 12 節第 3 題核准後），改由審查；其餘守衛照舊。
- **DI 7**：「bank clips never change」改成「`Clip` 不可變；格子持有一份共用的 `Clip`，音訊執行緒只經 view 讀、不碰參照計數；格子交還之後，擁有那個提交口的一方才放掉它」。守衛加第 10 節判定 3、4。
- **DI 9**：守衛名改成 `Clip::Create` 的拒絕案例（原「ClipBank rejects clips the mixer cannot play」）。
- **DI 11**：「a runtime `ClipId` names one slot in one generation」照舊；拿掉 bank id 的部分。

**新增的 Contract 條文**（不是 DI）：`Submitter` 同一時間一方使用、換手要有先後關係；`Clip` 複製共用樣本；第 3.4 節的後端介面；第 3.1 節的取樣限界。

**改寫的 Replacement Criteria**：
- **RC 1**：拿掉「clips must be loaded, unloaded or played from a thread or submitter other than the one that owns them」裡的「thread」（執行緒不再綁）；留「別的提交口」。
- **RC 5**：改成「生產者的**數量**在建立時定不下來（例：每次替換 World 都要新的提交口，或要任意執行緒提交）」。
- **新增 RC 9**：點名的 Feature 需要搶占（優先度、音量、距離或最舊），或需要虛擬 voice。
- **新增 RC 10**：點名的 Feature 需要聲音跟著遊戲的暫停或時間倍率走（和 RC 2 的暫停、變速一起看）。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的選項與理由 | 要它的 Feature 與產品 |
|---|---|---|---|
| 時間線留在時間基準 | master DI 3（v8-engine-audio-schedule）；別家都在音訊那一側（第 4 節） | (a) 混音器認得 World 的可縮放時間：混音器是 Product 一份、對多個 World，沒有「哪一個 World 的時間」；暫停時請求排到永遠。(b) 混音器加自己的可暫停時鐘（Kira）：沒有點名的 Feature，進 RC 10 | ①②③ 的遊戲聲音 |
| 套用點的「出」一次交 | O-12 的規則；Bevy `PostUpdate`；Wwise `RenderAudio`〔知〕 | (a) 交換函式：給「要整個 World、在 Run 之外」的事，播放不需要；(b) 每個 System 直接呼叫：多個生產者；(c) runner 直接讀寫：Module 的資料不要求 runner 認得（RC-O1） | ①②③ |
| 提交口不綁執行緒 | Kira `Send`＋`&mut self`；`-thread-binding-by-runner` | (a) 照交換點宣告「交出時在那條執行緒另拿一個」：用掉不還，交出 64 次用光；(b) 讓提交口可以還：要重新驗證環的順序與 clip 的回收（RC 6）；(c) 改成多生產者佇列：沒有需要 | ① Client 的模擬 World 交出（O-13）；②③ 讀檔換 World |
| `ClipBank` 拿掉、`Clip` 共用 | Bevy `Arc<[u8]>`、Kira `Arc<[Frame]>`；asset AS-10 | (a) 留 `ClipBank` 給測試：同一件事兩條路；(b) 留複製：兩倍記憶體、兩套有效期；(c) 混音器直接讀 asset 的 store：store 的釋放不知道音訊執行緒還在讀，破 DI 7 | ①② 的音效走 asset；③ audio_editor 轉檔後預覽 |
| 不搶占 | master DI（v8-engine-audio-core）；Kira | 加優先度與搶占：沒有點名的 Feature（AGENTS §1）；照 RC 9 再加 | — |

**決勝的不是**：既有的測試與突變要不要改（第 10 節的改動是費用，不是理由；AGENTS §4）。

## 9. thin start 與實作的步驟（動工前核准、使用者說「開始」之後）

本批只改 `GYO::Audio` 與它的測試、文件。**`world_audio` 不在本批**（卡片照 L17 判）：它的形狀（請求緩衝用訊息、實體還是 Resource）有自己的取捨，要照別家比過；它的 loader 要等 asset 的實作合併；沒有輸出後端時聲音聽不到。建議主會話把它排在 asset 實作合併之後、SDL 那批之前或同一批。

步驟：
1. **`Clip`**：新增 `Clip`（`Create` 驗證並複製一次；複製共用）。驗證規則從 `ClipValidation.hpp` 搬進來。
2. **拿掉 `ClipBank`**：`Mixer::Create(config)`；`VoiceMixer` 只經 view 讀 clip（`views_` 拿掉）；`ClipId` 只剩執行中的 id。測試裡用 `ClipBank` 的 7 個檔改成 `LoadClip`。
3. **`LoadClip(const Clip&)`**：格子持有 `Clip`；交還後擁有者放掉。
4. **執行緒契約**：拿掉 `ClaimOwner`、`owner`；改寫 3 個「belongs to the thread」的測試成「換手之後在另一條執行緒照常使用」（ThreadSanitizer 下）。
5. **錯誤碼**：拿掉 `DeviceUnavailable`。
6. **守衛**：第 12 節第 3 題核准後拿掉 `gyo_audio_board_readers`。
7. **文件**：改寫 `audio.md`（第 7 節）；開頭寫明是 GYO2 的合約、master 的計畫只當歷史。
8. **突變**：照第 10 節退役、改指向、加新的。
9. dev_log、設計審查（Fable／high，唯讀）、PR（base `gyo2`）。

**時間估計**：實作與測試 4～6 h；突變與 ThreadSanitizer／AddressSanitizer 1～2 h；文件與 dev_log 1 h。超過 1.5 倍停下回報。

**模型／檔位**：本批是上段（公開 Interface、Design Invariants）。實作者 Opus 5.5／high（本卡）；設計審查與敵對式檢查 Fable／high（主會話的 brief 指定）；§14.4 的檢證者（第 12 節第 3 題）Opus／high。

## 10. 測試、判定（先宣告；結果出來之後不放寬）

1. **輸出逐位元不變**：`CharacterizationTests.cpp` 的錄好的雜湊（一次性序列）照舊，只把 clip 從 `ClipBank` 改成 `LoadClip(Clip)`，**雜湊常數不改**。不同就停下（§14）。
2. **既有行為不變**：其餘測試改成用 `Clip`／`LoadClip` 之後全部通過；判定（期待值、條件）不改。改的只有建 clip 的方式；每個改了判定的地方照 §14.4 寫在變更說明。
3. **共用不複製**：`LoadClip` 之後，格子讀到的樣本位址等於 `Clip::Samples().data()`（新）。
4. **有效期**：外面放掉最後一份 `Clip` 時，正在響的 voice 照響完、輸出和沒放掉時逐位元相同；交還之後樣本才釋放（AddressSanitizer 下）（新）。
5. **即時安全**：`gyo_audio_alloc_tests` 照舊——`Render` 不配置也不釋放，含格子持有共用 `Clip` 的情形。
6. **換手**：一個 `Submitter` 在執行緒 A 用、join 之後在執行緒 B 用，`Play`／`Stop`／`LoadClip`／`UnloadClip` 照常，帳對得起來（ThreadSanitizer 下）（新）。
7. **ThreadSanitizer 的壓力測試**（`gyo_audio_playhead_stress`、「loads, plays, stops and unloads while another thread renders」）照舊通過。

**突變**（AGENTS §14.5、只跑相關的）：
- **退役**：find 指向已刪檔案的 12 條；執行緒綁定的 4 條（`au-sched-owner-unchecked`、`au-ctl-stop-owner-unchecked`、`au-ctl-stopall-owner-unchecked`、`au-ctl-unloadclip-owner-unchecked`：機制拿掉）；第 3 題核准時連 `au-ctl-board-read-in-render`。
- **改指向**：`au-clip-rate-unchecked`（find 搬到 `Clip::Create`）；find 落在改動範圍內的其餘條目逐條重跑、不改判定。
- **新增**（每條寫 `guards` 與「只有它抓得到的誤」）：格子交還前就放掉 `Clip`（DI 7）；`LoadClip` 複製而不共用（判定 3）。
- 合併前在背景跑一次完整清單。結果寫 dev_log。

## 11. 本卡照 L17 判的（不是使用者的話）

- 時間線留在時間基準、World 那一側用 `RealTime::now`（3.1）——和別家一致，不推翻舊決定。
- 套用點的「出」、一個套用 System 交（3.2）——照 O-12 的規則。
- 照 World 名字各一個提交口、替換時沿用（3.2）。
- 3.3 的機制／規則分類。
- 第 3.4 節的後端介面；`DeviceUnavailable` 搬走。
- `scheduleDelay` 的預設值留、SDL 那批重新推導。
- `world_audio` 不在本批（第 9 節）。
- 名字：`Clip`、`LoadClip(const Clip&)`。
- 文件留在英文的 `audio.md` 原地改（文件照原語言）。
- 12 條死突變退役。

## 12. 要使用者定的

### 第 1 題：拿掉 `ClipBank`，音效一律執行中載入，且共用不複製（推翻 master 的 v8-engine-audio-core 與 DI 7 的寫法）

- **背景**：`ClipBank` 是 master 第一版的設計（clip 在出聲前登記、之後不變）；control 層（v8-engine-audio-control-clips，你 10-31 同意改 DI 7）加了執行中載入，兩條並存。GYO2 的混音器在 `Configure` 建、音效在生成世界才載好，`ClipBank` 在產品的流程裡填不進東西。
- **選項**：
  - **(A) 拿掉 `ClipBank`；`LoadClip` 吃共用的 `Clip`、不複製**（建議）。
  - (B) 拿掉 `ClipBank`；`LoadClip` 照舊複製。
  - (C) 兩條都留。
- **取捨**：(A) 一條路、記憶體一份，費用是改測試 7 個檔與 DI 7 的文字；有效期多一個持有者（asset 的 store），釋放規則不變。(B) 改動較小，但每個音效兩份、兩套有效期。(C) 同一件事兩條路，每加一種命令驗兩次（RC 6）。
- **別家**：Bevy、Kira 共用、不複製；沒有一家有「出聲前的庫」。

### 第 2 題：`Submitter` 不綁執行緒（推翻交換點宣告 5.6 第 372 行）

- **背景**：交換點宣告寫「`Submitter` 綁執行緒、自己記；World 交出到別的執行緒時要在那條執行緒上另拿一個」，並寫「audio 重審照它」。本卡查到：提交口用掉不還，交出一次耗一個，64 次用光；綁定本身不是 SPSC 需要的，是抓誤用的檢查。
- **選項**：
  - **(A) 不綁執行緒：同一時間一方使用、換手要有先後關係**（建議；提交口跟著 World 走）。
  - (B) 照交換點宣告：綁執行緒，交出時另拿；並讓提交口可以還（要重新驗證環與 clip 回收）。
  - (C) 照交換點宣告、不還：接受「交出／替換的次數有上限」。
- **取捨**：(A) 拿掉一個斷言與 4 條突變，契約變成「和一般不保證執行緒安全的物件一樣」；誤用（兩條執行緒同時用）不再當場斷言，交審查。(B) 守住當場斷言，費用是重新驗證 RC 6 列的四件事。(C) 讀檔、交出的次數有上限，遊戲玩久了會壞。
- **別家**：Kira `Send`＋`&mut self`（同 (A)）。
- **(A) 核准後**：交換點宣告第 372 行與 E-16 的「綁執行緒」要改，那份是主會話的文件，本卡請主會話照四樣記進 HANDOFF 並改。

### 第 3 題：拿掉 DI 4 的原始碼掃描守衛 `gyo_audio_board_readers`（Design Invariants 的守衛，照 §14.4 要核准）

1. **它守的性質**：音訊執行緒不讀播放位置的看板（看板只供顯示，混音器不拿它做決定）。
2. **緩的面**：以前，`Mixer.cpp` 以外的檔、或 `Mixer::PlayheadOf` 以外的地方出現 `board.Read(` 就失敗；之後不失敗。
3. **緊的面**：沒有。
4. **性質還有誰守**：「output does not depend on someone reading playheads」（讀不讀看板，輸出逐位元相同）守住「看板不改輸出」；音訊執行緒「不讀」本身改由每批的設計審查守。看板的寫入端沒有讀的介面以外的路（`PlayheadBoard::Read` 是唯一的讀）。
5. **根據**：AGENTS §3「實裝的方式に縛るガード」；記憶裡的原則「底層對誤用不加守衛，改由每批的 code review 看設計」。掃描綁的是函式名與檔名（改名就要改守衛），守的是寫法，不是行為。
- **建議**：拿掉。核准前照 §14.4 做突變（`au-ctl-board-read-in-render` 拿掉守衛後存活，證明「緩的面」如上）與別的檢證者（Opus／high）用實際原始碼確認 1～5。
- 若你要留：不動它，只隨改名改 find。

### 第 4 題：voice 滿了照舊丟新的、不搶占（和 Unity、FMOD、Wwise 不同）

- **背景**：Unity、FMOD、Wwise 都有優先度與虛擬 voice（低的先讓）；Kira 和 GYO 一樣，滿了回錯誤／丟掉並記數。
- **選項**：**(A) 照舊、加 RC 9**（建議）；(B) 現在加優先度與搶占的擴充點。
- **取捨**：(A) 沒有點名的 Feature（AGENTS §1）；搶占是規則（看什麼搶誰），擴充點的形狀要等第一個需求才知道（優先度？距離？最舊？）。(B) 先做的形狀可能不合第一個真正的需求，而且上面積了東西就難改。
- **別家**：見上；Kira 同 (A)。

## 13. 和其他卡的協調

- **asset（`gyo2-asset`）**：asset 宣告 2.2 寫「音效（audio 重審那批）」。本卡建議：本批只定 `Clip` 這個值型別；音效的 loader 與型別特性在 asset 實作合併之後、`world_audio` 那批做。不改 asset 的任何東西。經主會話轉告。
- **threads／io（`gyo2-threads-io-review`）**：本卡不動 threads。`Submitter` 不綁執行緒（第 2 題）和 `-thread-binding-by-runner`、O-13 一致；若 threads 那批定了「綁執行緒的物件」的共通寫法，`Submitter` 不在其列。經主會話轉告。
- **主會話**：第 2 題 (A) 核准後改交換點宣告 5.6 第 372 行與 E-16；`world_audio` 的排程；O-2「以 World 名為鍵的資料在 `Build` 時重設」和提交口沿用的對齊（3.2）。

## 14. 範囲の限界

- FMOD、Wwise 的官方頁這次讀不到，兩家的事實標〔知〕，不當決勝的唯一根據。Unity 的部分頁面是摘要讀的。
- 本批不實作 `world_audio`、loader、後端；第 3.2、3.4 節是給它們的契約，**整合的行為（套用點的順序、替換時沿用、交出時跟著走）要到 `world_audio` 那批才有測試**。
- 盤點是靜態的：本卡還沒在 gyo2 上建置、跑 audio 的測試；動工的第一步先跑一次全部 audio 測試當基準（結果記 dev_log）。
- `scheduleDelay` 的預設值沒有在 gyo2 上量過（沒有後端）。

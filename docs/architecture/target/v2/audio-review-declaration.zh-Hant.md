# 重審報告與事前宣告：audio（`v8-arch-gyo2-audio-review`）

更新：2026-11-22（修訂 2；動工前核准）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-audio-review`）。**狀態：動工前核准（使用者 2026-11-22）。使用者說「開始」之前不改程式。**

> **核准**（使用者，2026-11-22，本卡的對話）：原話「照建議，全部核准」。核准的是修訂 2（38ce73f0）第 12 節五題照建議：第 1 題 (A) 修（本批拿掉綁定，提交口數量交 `world_audio` 那批）；第 2 題 (A) `Submitter` 不綁執行緒；第 3 題 (A) 拿掉 `gyo_audio_board_readers` 的方向（合併前照 §14.4 正式核准）；第 4 題照第 7 節草案的方向（最終文字在合併前核准）；第 5 題 (A) 同意縮小，請主會話在批次表加 `world_audio` 一批，三件開著的事交它。

**經過**：
- 修訂 0（5c098265）送 Fable 5.2／high 敵對式檢查：blocker 2、should 17、nit 9（主 checkout `build/evidence/gyo2-audio-review/declaration-r0-adversarial.md`）。全部採用或照第 15 節處理成**修訂 1**。
- 修訂 1（d92b08f9）送 Fable 5.2／high 窄複查：blocker 0、should 10、nit 10（同目錄 `declaration-r1-narrow-check.md`）。全部採用成**修訂 2**（第 16 節）。
- 最大的改動：本批**縮小**。修訂 0 要在本批拿掉 `ClipBank`、改 `LoadClip`；檢查指出它的後果（clip 只能由載它的提交口播、格數預設 0、沒有 `Render` 就不回收、asset 的 Handle 也是一個選項）都取決於還沒設計的 `world_audio`，先改公開介面會猜錯使用者。這些改成「重審的判定＋留給 `world_audio` 那批的選項」（第 5 節），本批只做和使用者的流程無關、現在就能驗證的部分。
- 基準：gyo2 eacab4d7 上 audio 的 4 個 CTest 全部通過（本機 Intel Mac；主 checkout `build/evidence/gyo2-audio-review/baseline-*.log`）。

**性質**：批次表第 6 批「audio 重審」的重審報告＋事前宣告。`engine/modules/audio` 是 Engine 的公開 Interface，照 AGENTS §2 當**基盤**處理（第 3 節有根據表）。

**用詞**：標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。別家的事實標出處：本機原始碼寫路徑；網頁寫 URL；**〔知〕**＝憑記憶、這次沒讀到官方頁（FMOD 回空白頁、Wwise 回 403），不當決勝的根據。

**核心一句**（每一項都拿來對照）：**混音器是 World 外、Product 一份的服務，只提供機制：在指定的時間點起播、停下、混音、計數；聲音什麼時候該響、遊戲暫停時聲音怎麼辦、滿了先丟誰，是作者的規則。**

---

## 0. 根據與讀的版本

- 程式：`origin/gyo2` eacab4d7 的 `engine/modules/audio` 與 `tests/common/audio`（2 個執行檔、4 個 CTest，其一是原始碼掃描；82 個 TEST_CASE）。
- 合約：`docs/architecture/audio.md`（DI 1～11、RC 1～8；master 時代的英文頁，R0 照帶）；突變清單 `docs/architecture/plans/audio/scripts/mutations*.json`（95＋1＋7 條）。
- 批次表 `rebuild-batch-plan.zh-Hant.md` 第 64 行（文件分支 `claude/gyo2-time-design` 1fca1420）；R0 清單附錄 A.1、第 9 節；概念層 `rebuild-concept.zh-Hant.md` 2.1（audio：**不相合，要處理**）、3.7。
- 編排層 O-2、O-3、O-8、O-11～O-13、O-15；交換點宣告 5.1～5.6（E-16 第 110 行、第 372 行）；`docs/cross-component-rules.md` CCR-5、CCR-6。
- HANDOFF `v8-arch-gyo2-thread-binding-by-runner`（使用者 11-16）；kernel HANDOFF `v8-arch-adapter-plugins-review`（2026-11-13 結案：model 取樣、**audio core、ui 留 Engine**，`docs/architecture/plans/kernel/HANDOFF.md:92`）、`v8-arch-engine-or-plugin-criteria`、`v8-arch-docs-scope-of-target`（`docs/architecture/target/README.md:29`）。
- `engine/modules/world_time`（`TimeTypes.hpp`、`Sampling.hpp`）、`world-time.zh-Hant.md`；`docs/architecture/target/determinism.md` 第 11 節（判定程式只能從 tick 與記錄下來的邊界輸入拿時間）。
- asset 宣告（分支 `claude/gyo2-asset-585196`，1b120a86 動工前核准，未合併）：2.2、5.4、5.16、5.17、AS-10。
- master：`docs/architecture.md:357-359`（為什麼有時間點換算：30 FPS 時一幀晚 33 ms 處理事件，「現在」播會讓起播落在幀的格子上）；`apps/object_fps_pvp/src/Present/PvpSound.cpp` 的 `Play(cue, Time::TimePoint at, …)`（傳的是事件的時刻）。只看為什麼，不拿數據當設計。
- 別家：
  - Bevy（本機 `~/Code/GitHome/bevy/crates/bevy_audio/src`）：`lib.rs:82-118`（播放的 System 全在 `PostUpdate`）、`audio_output.rs:16-31`（輸出裝置是 World 的 Resource）、`audio_output.rs:84-`（載好才播）、`audio_source.rs:8-20`（`Arc<[u8]>` 共用）、`audio.rs:35-45`（`speed`、`paused` 是每個 sink 的）；整個 crate 不引用 `bevy_time`。
  - Kira 0.12.6（docs.rs）：`StartTime::{Immediate, Delayed, ClockTime}`；時鐘可 `pause`、`set_speed`；`PlaySoundError::SoundLimitReached`（回給該次呼叫者）；`StaticSoundData.frames: Arc<[Frame]>`；`AudioManager` 在後端 `Send` 時 `Send`，`play` 取 `&mut self`（編譯器擋同時使用）；`assert_no_alloc`。
  - Unity（docs.unity.com）：`PlayScheduled` 用 `AudioSettings.dspTime`；`AudioListener.pause` 暫停全部、凍結 dspTime；`priority` 0～256，低的先虛擬化；`Time.timeScale` 的頁沒提聲音（沒提，不等於確定不碰）。Unity Netcode（本機 `~/Code/GitHome/com.unity.netcode/Runtime/PredictionTicking/NetworkTime.cs:44`、`:164`）：`IsFirstTimeFullyPredictingTick`——「play sounds… only happens once」，由作者判。
  - FMOD〔知〕：`setDelay` 用 DSP 時鐘；虛擬 voice。Wwise〔知〕：`PostEvent` 進佇列、`RenderAudio` 每幀送；播放上限。Unreal、Godot：這次沒查（檢查者憑記憶：兩者遊戲暫停時預設帶到聲音）。

## 1. 先用遊戲講

**FPS 的 Client 開槍。** 第 312 幀，畫面 World 的 System 判定「玩家開了一槍」，把「播 `sfx.shot`、增益、時間點」放進 World 裡的請求；這一幀最後的套用點，一個 System 把請求交給混音器；音訊執行緒把時間點換成第幾個取樣開始響。**時間點是哪一刻**決定聲音準不準：master 傳的是事件的時刻，畫面卡 20 ms 也照原本的時刻響；如果只拿「這一幀的取樣時刻」，同一幀的聲音全擠在同一刻、起播落在幀的格子上——這正是 master 做時間換算要避開的事（第 4.1 節）。

**單機遊戲按暫停。** 模擬的 System 不跑，就沒有新的聲音；已在響的照響到完。Bevy、Kira 是這樣；Godot、Unreal 預設會帶著聲音暫停（未查證）。要「暫停時聲音也停」是遊戲的規則，今天的混音器沒有暫停（RC 2）。

**Client 的模擬 World 交給別的執行緒（O-13）。** 它的提交口跟著 World 走。今天的合約說「提交口歸第一個呼叫它的執行緒」，交出去之後再呼叫，斷言就會觸發（檢查是盡力而為的：已結束的執行緒的 id 可能被重用，`audio.md:110-111`）。

**讀檔換 World（O-8）。** 每次建 World 都拿一個新提交口的話，提交口用掉不還（上限 64、預設 4），讀檔幾次就用光。

**音效從哪來。** asset 在背景載好不可變的值（World 外的 `AssetStore`）。混音器有兩條收 clip 的路：出聲前登記的 `ClipBank`、執行中由提交口載入（複製一份）。哪一條留、要不要共用 asset 的記憶體，取決於 `world_audio` 怎麼把 asset 接到混音器（第 5 節）。

**audio_editor 預覽。** 播、停、執行中換 clip、畫播放位置——master 的 control 層為它做的，留下。

## 2. 重審結果總表

| 部件 | 判定 | 本批做不做 | 一句理由 |
|---|---|---|---|
| `VoiceMixer`（固定數量的 voice、Q16 整數混音、飽和、晚到） | 留 | — | 機制；整數混音三平台逐位元相同、成本為零（determinism.md 4.5） |
| 時間點換取樣位置（`Render(block, anchor)`、`scheduleDelay`） | 留；合約補「時間點必須是時間基準上的時刻」 | 做（DI 3 文字） | 第 4.1 節 |
| `PlayRequest::at`（`Time::TimePoint`） | 留 | — | 混音器在 World 外；時鐘型別不進 World（第 4.1 節） |
| `Submitter` 綁執行緒 | 拿掉（第 12 節第 2 題） | 做 | 第 4.2 節 |
| `Submitter` 用掉不還 | RC 5 已成立（第 12 節第 1 題）；處理在 `world_audio` 那批 | 不做 | 第 4.2 節 |
| `ClipBank`、`LoadClip` 的複製 | 該不該存在：**存疑**；選項與後果交 `world_audio` 那批 | 不做 | 第 5 節 |
| clip 的租約與交還（音訊執行緒不釋放） | 留 | — | DI 7 的核心 |
| `SoundHandle`、`Stop`、`StopAll`、`PlayheadOf` | 留 | — | audio_editor 的預覽 |
| `gyo_audio_board_readers`（原始碼掃描） | 拿掉（第 12 節第 3 題） | 合併前正式核准後做 | 綁實作方式的守衛；第 4.5 節 |
| `MixerStats` 與帳（DI 6） | 留 | — | 丟了要記 |
| `AudioErrorCode::DeviceUnavailable` | 拿掉，SDL 那批的後端自己定 | 做 | audio 自己沒有地方用它 |
| 滿了丟新的、不搶占 | 留，加 RC 9（第 12 節第 4 題） | 做（RC 文字） | 第 4.3 節 |
| 主增益、`AudioFormat`、`kMaxGain` | 留 | — | 機制 |
| `scheduleDelay` 預設 60 ms | 留欄位；SDL 那批重新推導預設值 | — | 預設值來自 master 在 SDL macOS 的量測 |
| 突變清單中 gyo2 上跑不起來的 14 條 | 退役 | 做 | 第 10 節 |
| 文件位置 | 照 `target/README.md:29`：開 `target/v2/audio.zh-Hant.md`，`audio.md` 加 Legacy 行（位置先經主會話確認，第 13 節） | 做 | 第 7 節 |

## 3. 基盤 Architecture 的根據（AGENTS §2）

**為什麼算基盤**：積在 `GYO::Audio` 上的有 SDL 的輸出後端、`world_audio`、音效 loader、audio_editor、每個產品的聲音；它的公開 Interface 一改，這些都要改。

**予見的根據 → 要求**：

| # | 根據（出處） | 種類 | 對 audio 的要求 | 本批 |
|---|---|---|---|---|
| A-1 | ① FPS Client 的槍聲、腳步（master `PvpSound.cpp`；R0 附錄） | 名指的 Feature | 起播在事件的時刻，不落在幀格上 | DI 3 的邊界（4.1）；預設在 `world_audio` |
| A-2 | ① Client 的模擬 World 交給別的執行緒（O-13、E-13） | 名指的 Feature | 提交口能跟著 World 換執行緒 | 拿掉綁定（4.2） |
| A-3 | ①②③ 讀檔換 World（O-8） | 名指的 Feature | 提交口的數量不隨替換次數長 | RC 5 成立；`world_audio`（4.2） |
| A-4 | ③ audio_editor 的預覽（master control 層） | 名指的 Feature | 停、執行中載入、播放位置 | 留 |
| A-5 | ①② 音效經 asset 載入（asset 宣告 2.2、5.16） | 名指的 Feature | clip 從 asset 的不可變值來，音訊執行緒不釋放 | 選項交 `world_audio`（5） |
| A-6 | SDL 的輸出後端（批次表 SDL 列；它是 Component，支撐 A-1、A-4 的「聽得到」） | 計畫中的 Component | 第 4.4 節的介面 | 只寫契約 |

**規模**：master 的 PvP 內容有 4 個 wav（asset 宣告 F1）；`maxVoices` 1～1024（預設 32）、提交口 1～64（預設 4）、每提交口 clip 格 0～256（預設 0）。本批不改任何上限。

**前提表**（誰定的、哪些判定掛在它上面）：

| 前提 | 誰定的 | 掛在它上面的 |
|---|---|---|
| World 不綁執行緒 | 使用者 11-16（`-thread-binding-by-runner`） | 4.2 拿掉綁定 |
| World 整個交出 | 使用者 11-22 合併前核准（O-13） | 4.2 |
| 外部服務自己同步、World 不上鎖 | O-12、CCR-6 | 4.2 套用點 |
| audio core 留 Engine、是 Module | 使用者 11-13（`-adapter-plugins-review`）、11-16（Module 定義） | 4.3 |
| 判定程式不讀時鐘 | 使用者 11-11（determinism 第 11 節） | 4.1 時鐘型別不進 World |
| 混音器是 Product 一份、放在共用處 | 交換點宣告 5.6 第 372 行（「混音器是 Product 一份的物件」）；「放在共用處」是卡片的假設 | 4.2、4.4 |
| 有一個 `world_audio` 登記單位接 World 與混音器 | **卡片的假設**（照 asset／world_asset、time／world_time 的拆法）；批次表還沒有這一批（第 12 節第 5 題） | 4.1、4.2、5 的「留給 `world_audio`」 |

## 4. 四個重審點的判定

### 4.1 `PlayRequest::at` 與 World 的暫停、慢動作

**判定**：混音器的時間線留在時間基準（`Time::MonotonicClock`）。概念層 2.1 的「不相合」，本批處理到**邊界**：時間點必須是時間基準上的時刻；**預設取哪一刻**（幀的取樣、事件的時刻）是一個**未解的缺口**，交 `world_audio` 那批與 world_time 一起定。本批不宣稱已經解掉「不相合」。

**本批寫進 DI 3 的邊界**（卡片照 L17 判；逐字草案在第 7 節）：只寫**混音器的輸入**——`PlayRequest::at` 與 `Stop` 的時間點必須是同一行程的時間基準（`Time::MonotonicClock`）上的時刻；可縮放的遊戲時間不是它的輸入。守衛：審查。

**留給 `world_audio` 那批的時間設計**（不進 audio 的 DI；那批照 determinism 第 11 節定）：World 那一側從哪裡拿時刻（`RealTime::now`、記錄下來的到達時刻、由它們算出的值）；判定資料的 System 不讀 `RealTime`（determinism 第 11 節只准 tick 與記錄下來的邊界輸入），所以讀 `RealTime` 的只能是表現層的 System；時鐘型別不進 World（請求帶整數奈秒，由套用 System 換成 `PlayRequest`）；請求緩衝不是判定資料、不進 World 摘要。這條「凡是送進 World 外服務的時刻都是時間基準」若要成為跨 Component 的規則，經主會話請圖書管理員對照 `cross-component-rules.md`（AGENTS §1）。

**理由**：
1. 混音器在 World 外（共用處），一個混音器對多個 World；可縮放的時間屬於某一個 World，暫停時不前進，拿它當起播時刻會排到永遠不到的未來。
2. 有根據的別家：Bevy 的 audio 完全不引用時間 crate；Kira 起播用音訊那一側的時鐘，遊戲要聲音跟著遊戲時間走就自己暫停那個時鐘；Unity 的 `PlayScheduled` 用 dspTime（音訊系統的時間）。FMOD、Wwise〔知〕不當根據。
3. 判定程式不讀時鐘（determinism 第 11 節）；`TimeTypes.hpp:3-6` 刻意不帶時鐘型別。所以時鐘型別留在 World 外。

**未解的缺口**（owner：`world_audio` 那批，和 world_time 對齊；卡片照 L17 判 owner）：
- **預設的時刻**：只用 `RealTime::now`，同一幀的聲音擠在同一刻、起播落在幀格上，A-1 做不到。
- **固定步第 i 步對應時間基準的哪一刻**：一幀跑多步（快轉、補幀）時要分得開；慢動作下這個對應怎麼定，world_time 今天沒有答案。
- **到達時刻**：輸入事件帶時間基準的到達時刻時，直接用它（SDL 那批的平台事件換算，`MonotonicClock.hpp` 的註解）。

**取樣的限界**（寫進合約）：時間點和 `Render` 的 anchor 要來自同一行程的時間基準。world_time 用 `ManualDelta` 或給的值不是時間基準時，請求會被當成晚到，或排到最多一小時後、期間佔著 voice（`audio.md:32-33`）。取樣移給 runner 後正式執行也是 `ManualInstant`（交換點宣告 5.5），那時 runner 給的值必須是時間基準的讀數。

**暫停、倍率**：不碰混音器（預設）。「暫停時聲音也停」「慢動作時聲音也變慢」是遊戲的規則，點名的 Feature 出現時照 RC 2（補一句「含跟著遊戲的暫停或時間倍率走」）交使用者。這和 Godot、Unreal 的預設不同（未查證），列在第 12 節第 4 題。

### 4.2 提交介面的執行緒契約與交換點

**判定**：
1. **混音器是 Product 一份的物件**（交換點宣告 5.6 已寫），同步是它自己的事（CCR-6）。
2. **World 交給混音器的路線是套用點的「出」**（`Main`／`Last`），一個 `world_audio` 的套用 System 把這一幀的請求交給這個 World 的提交口。理由：O-12 的規則（要排在發出請求的 System 之後、由很多 System 發）；SPSC 只能有一個生產者，集中到一個套用 System 後 Kernel 怎麼排（含解除 LIM-2）都只有一方在用。Bevy 的播放 System 在 `PostUpdate`（本機原始碼）。
3. **`Submitter` 不綁執行緒**（第 12 節第 2 題）：新契約——同一時間只有一方呼叫；換到別的執行緒時，前一方最後一次呼叫要 *happens-before* 後一方第一次呼叫（例：`std::thread::join`、同一把 mutex 的 unlock／lock、World 交出時 runner 的同步）；只是「時間上錯開」、沒有同步，不算。
4. **提交口的數量**（RC 5 已成立，第 12 節第 1 題）：不在本批處理，選項交 `world_audio` 那批。

**為什麼拿掉綁定**：
- 程式上：SPSC 要的是「一個生產者、前後兩次呼叫有先後關係」。`Enqueue` 用 relaxed 讀自己的 `tail`（`Mixer.cpp:245`），換手有 happens-before 就看得到；`lastSequence`、`ClipSlot` 的擁有者欄位同理。`ClaimOwner` 的 CAS 是 relaxed（`Mixer.cpp:239`），本來就不提供同步，只是檢查。
- 和已定的事衝突：World 不綁執行緒（11-16）；World 整個交出、交出期間只有那條執行緒碰它（O-13 (2)），提交口是 World 裡的東西，跟著走。交換點宣告寫的「交出時在那條執行緒上另拿一個」：提交口第一次被呼叫就認領那條執行緒，而 clip 格子與 handle 是提交口私有的——交出後另拿的提交口播不了原本載的 clip、停不了原本送出的聲音（`Mixer.cpp:257`、`287-296`、Stop 的前綴檢查）。
- `-thread-binding-by-runner` 說「需要綁執行緒的 Resource 自己記來源執行緒」。卡片的讀法：需要綁的是真的只能在某條執行緒用的東西（視窗、繪圖 context）；SPSC 的生產端不在此列。
- 綁定本身不可靠（執行緒 id 會重用，`audio.md:110-111`）。卡片的讀法：它是抓誤用的檢查；屬不屬於「不為防誤用加守衛」那條原則，由第 12 節第 2 題判。
- Kira 先例只能搬一半：Rust 的 `&mut self` 由編譯器擋同時使用，C++ 沒有。拿掉綁定後，兩條執行緒同時用是資料競爭；只有手動的 ThreadSanitizer 建置抓得到（gyo2 的 CI 與 preset 沒有它），執行時不斷言。

**留給 `world_audio` 那批的**（卡片列出、不在本批判）：
- 提交口照 World 名字沿用，還是讓提交口可以還（RC 6 列的重新驗證）。照名字沿用的已知問題：`BuildNewFirst` 的 `beforeSwap` 在新 World 上跑（`Product.hpp:79-82`），新 World 若對共用的提交口 `StopAll`／`UnloadClip` 而替換失敗，舊 World 的聲音已停、clip 已卸，舊的 `ClipId` 再 `Play` 會斷言（`Mixer.cpp:257`）；和 O-2「以 World 名為鍵的資料在 `Build` 時重設」的字面不合；交出中的 World 能不能替換未定（交換點宣告 5.3）。
- 帶回的值：`SoundHandle`、`Stop` 的 `false`、`NoClipSlot` 取決於音訊執行緒的進度；CCR-6 要求帶回的值不讓判定依時序而變。handle 怎麼回到作者手上、不進判定資料，要在那批寫。
- 請求還在緩衝裡時 clip 被卸載，交出時會斷言：先後要定。
- 重跑（回滾）：要不要在重跑中發聲是作者的規則（Unity Netcode 同），`world_audio` 不過濾；World 裡已有 `Resimulation`。

### 4.3 「Engine 只提供機制，會被替換的規則外移」適不適用

**先更正前提**：修訂 0 引的是 11-13 當天被改掉的那一版（「audio core 做成 plugin」）。同日的複核 `v8-arch-adapter-plugins-review` 結案：**audio core 留 Engine**；「會被替換的規則」讀作「產品會在多個實作之間選的規則」。11-16 起 audio 是 Module。

**照這個判準逐項分**（卡片照 L17 判）：

| 項目 | 產品會不會在多個實作之間選 | 處置 |
|---|---|---|
| 混音（Q16、int64、主增益、四捨五入、飽和、計數） | 不會：誰用都一樣 | 機制，留 |
| voice 有上限、建立時配好 | 不會：即時安全的前提 | 機制，留 |
| 晚到就從下一個 frame 開始（clip 從頭播） | 可能會（另一種是跳過已過去的部分，也不丟、不延後） | 今天的行為留；沒有點名的 Feature 要另一種 |
| 滿了丟新的 | 會（優先度、音量、距離、最舊） | 今天是最小行為；擴充點等點名的 Feature（RC 9） |
| `scheduleDelay` 的值 | 會（幀率、後端的 block 大小） | 已是設定；預設值 SDL 那批重推 |
| 方向、距離換成增益 | 會 | 已在外面 |
| 哪個事件播哪個聲音 | 會 | 已在外面 |
| 暫停、倍率帶不帶聲音 | 會 | 作者的事；混音器沒有暫停（RC 2） |
| 重跑中要不要發聲 | 會 | 作者的事 |
| bus、effect、淡入淡出 | 會 | 沒有；RC 2 |

**結論**：適用，而且 audio 今天只剩機制；要外移的規則不在程式裡。

### 4.4 SDL 的輸出後端：audio 這一側要的介面

後端不在本批。寫進合約（卡片照 L17 判）：
1. **給**：`Mixer::Render(Span<int16_t>, TimePoint anchor) noexcept`，在後端的音訊執行緒上、一次一個 block；格式常數；`Stats()`。
2. **要求**：每個 block 用時間基準取一次 anchor（DI 3）；只有一條執行緒呼叫 `Render`；**後端只持不擁有的參照**（若後端與別人共用所有權，而音訊執行緒的回呼握著最後一份，`MixerState` 會在音訊執行緒上解構，破 DI 7）；混音器放在共用處，持有者在後端停之前不移動、不銷毀它（取代 `audio.md:135-136`「backend stops output first」的責任分配）；後端在**自己**登記的 `OnDestroy` 回呼裡停——安全靠 O-15「回呼在所有 World 之後、共用處之前」（`orchestration.zh-Hant.md:72-73`）；後端的登記要排在建混音器的單位之後，所以它的 `OnDestroy` 先跑。
3. **改變**：master 的 `SdlAudioOutput::Open(Mixer&&)` 拿走所有權；GYO2 改成參照。`DeviceUnavailable` 歸後端。
4. **不變**：anchor log、callback 統計是後端的。

### 4.5 DI 4 的原始碼掃描

`CheckBoardReaders.cmake` 掃原始碼文字：`Mixer.cpp` 以外、或 `Mixer::PlayheadOf` 以外出現 `board.Read(`，或整個目錄裡的讀不是恰好 1 個（含 0 個），就失敗。它綁的是函式名、檔名與寫法（AGENTS §3「実装の方式に縛るガード」），守的是「音訊執行緒不讀看板」。讀看板不影響即時安全（都是原子操作）；違反的後果是 DI 4 說的「拿顯示用的值做決定」，這是設計審查看得出的事。判定見第 12 節第 3 題。

## 5. 留給 `world_audio` 那批：clip 的來源（`ClipBank` 該不該存在）

**重審的判定**：`ClipBank`「該不該存在」**存疑**，但答案取決於 `world_audio` 怎麼接 asset，本批不改。

**觀察到的事實**：
- 混音器有兩條收 clip 的路：`ClipBank`（出聲前登記、混音器擁有、id 1 起、沒有上限）與 `Submitter::LoadClip`（複製進提交口的格子、只有載它的提交口能播或卸、每提交口 0～256 格、**預設 0**、回收靠 `Render` 在跑且在擁有者下一次 `LoadClip` 時才釋放）（`ClipBank.hpp:48`、`Mixer.hpp:28`、`Mixer.cpp:139`、`257-258`、`287-310`；`audio.md:127-128`）。
- 混音器若在 `Configure` 建（卡片的假設；共用處不凍結，也可以晚建），出聲前 asset 還沒載好，`ClipBank` 填不進遊戲的音效。
- asset 持有不可變的值（AS-10）；`LoadClip` 再複製一份。

**選項**（給那一批比；後果照檢查補齊）：

| 選項 | 後果 |
|---|---|
| (A) 拿掉 `ClipBank`；`LoadClip` 吃共用樣本的 `Clip`（`shared_ptr`），不複製 | 一條路、記憶體一份；clip 只能由載它的提交口播，兩個 World 用同一音效要各佔一格；格數預設要重定；沒有 `Render`（gyo2 現在沒有後端）就不回收；釋放落在最後一個持有者的執行緒（asset 的 store 或提交口的擁有者），永遠不在音訊執行緒；`Clip` 會活過 asset 的 store 項目，要和 asset 卡對齊 |
| (B) 拿掉 `ClipBank`；`LoadClip` 照舊複製 | 同 (A) 的格子後果；每個音效兩份、兩套有效期 |
| (C) 兩條都留 | 同一件事兩條路；bank clip 給所有提交口、沒有上限，是 (A)(B) 沒有的 |
| (D) `world_audio` 持有 asset 的 Handle，混音器只借 view | 有效期只有 asset 一套、`GYO::Audio` 不必有共用所有權；音訊執行緒還在讀時 asset 不得釋放，要有和交還相同的協定 |

別家：Bevy、Kira 共用不複製；沒有一家有「出聲前的庫」。

**那一批寫判定時要照的**（修訂 0 檢查的 S-8、S-10、S-13 附帶、N-6；`build/evidence/gyo2-audio-review/declaration-r0-adversarial.md`）：格子在 `Mixer.cpp` 內部，測試讀不到位址，「不複製」改用「`LoadClip` 的配置數為 0」判；釋放在擁有者下一次 `LoadClip`，有效期的判定要寫觀測法；「外面先全部放掉」要有一案，並立「音訊執行緒在交還時放掉 `Clip`」的突變；「不碰參照計數」只能靠審查；`MixerTests.cpp` 直接用 `VoiceMixer`，不能改成 `LoadClip`；DI 8、DI 9、DI 11 的守衛測試要照 §14.4；被搬走的 `Clip` 傳給 `LoadClip` 的行為要定。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：批次表第 6 批；概念層 2.1；交換點宣告 E-16、5.6。支撐的 Feature：A-1～A-6（第 3 節）。
2. **現在的問題**：(a) 提交口綁執行緒，和 World 不綁執行緒、World 整個交出衝突；(b) World 那一側該送哪一種時間沒寫；(c) RC 1、RC 5 已成立（第 12 節第 1 題）；(d) `DeviceUnavailable` 在 audio 裡沒有使用者；(e) 突變清單有 14 條在 gyo2 跑不起來。
3. **變的邊界**：`Submitter` 的執行緒契約（公開合約）；`AudioErrorCode` 少一個值（`NoClipSlot` 留 5、不重編）；DI 1 的「and its backend」、DI 2、DI 3、DI 4 的守衛、DI 7 的「owner thread」、DI 10 的守衛；RC 1、RC 2、RC 5 改寫、RC 9 新增；守衛 `gyo_audio_board_readers`（第 3 題）。
4. **影響**：audio 與它的測試、突變清單；文件 `audio.md`（Legacy、被取代的條文照 `target/README.md:24` 標）、新頁 `target/v2/audio.zh-Hant.md`、`target/v2/README.md` 加一列（`docs/architecture/README.md:20` 指向它）、交換點宣告第 110、372 行（主會話）；引用 `audio.md` DI 編號的程式註解（`tests/common/audio/CMakeLists.txt:37`、`CheckBoardReaders.cmake:1`、`Mixer.cpp:40`、`Mixer.hpp:57`）改指新頁。gyo2 上沒有別的程式使用 audio。
5. **依賴方向**：不變（`GYO::Audio` → `GYO::Base`、`GYO::Time`）。
6. **Ownership**：提交口從「綁一條執行緒」變成「同一時間一個使用者、換手要同步」。其餘不變。
7. **為什麼不能更小**：只放寬斷言、不改合約，等於留一條寫著卻不守的規則；不寫時間的邊界，`world_audio` 會照「隨手拿 World 裡的時間」接上。

## 7. Design Invariants 與 Replacement Criteria 的改動

**位置**（交主會話確認，第 13 節）：照 `target/README.md:29`（audio 屬「改造會改變它」），新開 `target/v2/audio.zh-Hant.md` 收 audio 的 Design Invariants 與 Replacement Criteria（照 GYO2 各 Module 頁的寫法），`audio.md` 加 `> **Legacy**` 行。條號沿用 DI 1～11、RC 1～10，方便突變的 `guards` 追蹤。

**以下是草案**：現在請使用者核准方向（第 12 節第 4 題）；最終文字在合併前核准頁（第 9 節第 7 步）定。

**改的條文**（其餘照舊譯過去，不改語意）：
- **DI 1**：拿掉「and its backend」（後端不在 audio）。
- **DI 2**：只留「`GYO::Audio` 不含 SDL，連結恰好是 `GYO::Base`、`GYO::Time`（守衛 `gyo_expect_audio_links`）」；後端的部分（platform-sdl 的不變量、G1～G3）交 SDL 那批。
- **DI 3**：加一句，逐字草案：「送進混音器的時間點（`PlayRequest::at`、`Stop`／`StopAll` 的 `at`）是同一行程的時間基準（`Time::MonotonicClock`）上的時刻；可縮放的遊戲時間不是混音器的輸入，遊戲的暫停與時間倍率不作用在混音器上。守衛：審查。」拿掉指向 `disk` 驅動測試的守衛（gyo2 沒有）。
- **DI 4**：第 3 題核准時拿掉「`gyo_audio_board_readers`」一項，改寫「由設計審查守」。
- **DI 7**：「allocated only on that submitter's owner thread and freed there」改成「只由使用那個提交口的一方配置與釋放，那一方永遠不是音訊執行緒」；守的性質不變（音訊執行緒不配置、不釋放）。
- **DI 10**：拿掉 `audio_sdl.disk` 的守衛（gyo2 沒有）。
- **Contract**：`Submitter` 的新執行緒契約（4.2 第 3 點），取代 `audio.md:53`、`:107`、`:122` 的「owner thread」；「`Submitter` 的 Programmer Error 在計數之前斷言」（`audio.md:111`）照舊；4.4 的後端介面；4.1 的取樣限界。

**Replacement Criteria**：
- **RC 1**：「clips must be loaded, unloaded or played from a thread or submitter other than the one that owns them」拿掉「thread」，留「另一個提交口」。
- **RC 2**：補「含聲音跟著遊戲的暫停或時間倍率走」（修訂 1 的 RC 10 併進來：RC 2 已有 pause、pitch）。
- **RC 5**：改成「生產者的**數量**在建立時定不下來（例：每次替換 World 都要新的提交口），或要多個生產者同時用一個提交口」，並記「2026-11-22 判定已成立（O-8 的替換），使用者選修或重建（第 12 節第 1 題），處理在 `world_audio` 那批」。
- **新增 RC 9**：點名的 Feature 需要搶占（優先度、音量、距離或最舊）或虛擬 voice。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的選項與理由 | 要它的 Feature |
|---|---|---|---|
| 時間線留在時間基準；本批只寫邊界 | master DI 3；Bevy、Kira、Unity（第 0 節） | (a) 混音器認得 World 的可縮放時間：一個混音器對多個 World，暫停時排到永遠；(b) 混音器加可暫停的時鐘（Kira）：沒有點名的 Feature，進 RC 2；(c) 本批就定預設時刻：要和 world_time 的固定步對應一起定（第 12 節第 5 題） | A-1 |
| 套用點的「出」一次交（契約） | O-12；Bevy `PostUpdate` | 交換函式（給「要整個 World、在 Run 外」的事）；每個 System 直接呼叫（多個生產者）；runner 直接讀寫（RC-O1） | A-1、A-2 |
| 提交口不綁執行緒 | Kira（一半：Rust 編譯器擋同時使用，C++ 沒有）；`-thread-binding-by-runner` | 見第 12 節第 2 題的 (B)(C)(D) | A-2 |
| clip 的來源延到 `world_audio` | — | 本批就改（修訂 0）：後果取決於還沒設計的使用者 | A-5 |
| 不搶占 | master；Kira（但 Kira 回錯誤給該次呼叫者，GYO 只有總數） | 現在加擴充點：沒有點名的 Feature（AGENTS §1） | — |

**決勝的不是**：既有測試與突變要不要改（是費用，AGENTS §4）。

## 9. thin start 與實作的步驟（動工前核准、使用者說「開始」之後）

`world_audio`、loader、後端都不在本批。建議主會話把 `world_audio` 排在 asset 實作合併之後、SDL 那批之前或同一批；它依賴 world_time、asset、編排層。

0. **基準**：改之前，在本機 ThreadSanitizer 組態（`-fsanitize=thread` 的手動建置，`TSAN_OPTIONS=halt_on_error=1`）跑一次 audio 的 CTest，結果與組態記進 dev_log、log 放 `build/evidence/gyo2-audio-review/`。
1. **執行緒契約**：拿掉 `ClaimOwner`、`SubmitterSlot::owner` 與相關斷言；3 個「belongs to the thread」的測試照下表逐判定處理（不是 DI 守衛；照 §14.4 寫在變更說明）：

   | 測試 | 判定 | 守的性質 | 處理 |
   |---|---|---|---|
   | `ScheduleTests.cpp:385` | 別的執行緒 `Play` 斷言 | 綁定（拿掉） | 改寫成判定 2 的換手案 |
   | `StopTests.cpp:370` | 別的執行緒 `Stop`／`StopAll` 斷言 | 綁定（拿掉） | 同上 |
   | `StopTests.cpp:383` | 斷言後 `stopsSubmitted == 0` | 「Programmer Error 在計數之前斷言」（`audio.md:111`，照舊） | 搬到 `StopTests.cpp:350` 那一案（別的提交口的 handle 斷言後計數為 0） |
   | `ClipSlotTests.cpp:312` | 別的執行緒 `LoadClip`／`UnloadClip` 斷言 | 綁定（拿掉） | 改寫成判定 2 的換手案 |
   | `ClipSlotTests.cpp:323` | 斷言後 `unloadsSubmitted == 0` | 同 `StopTests.cpp:383` | 搬到 `ClipSlotTests.cpp:289` 那一案 |
2. **錯誤碼**：拿掉 `DeviceUnavailable`（`NoClipSlot` 留 5）。
3. **守衛**：第 3 題正式核准後拿掉 `gyo_audio_board_readers`。
4. **文件**：第 7 節（位置經主會話確認）；程式註解裡的 DI 引用改指新頁。
5. **突變**：第 10 節。
6. dev_log、設計審查（Fable／high，唯讀）。
7. 合併前核准頁 `audio-review-merge-approval.zh-Hant.md`（照同線慣例）：DI／RC 的最終文字、第 3 題的 §14.4 正式核准、測試判定的變更、突變的結果。之後開 PR（base `gyo2`），等使用者明確說「合併」。

**時間估計**：程式與測試 2～3 h；ThreadSanitizer（基準與之後）與突變 2～3 h；文件（新頁、Legacy 行）2～3 h；dev_log 與核准頁 1～2 h。共 7～11 h。超過 1.5 倍停下回報。

**模型／檔位**：上段（公開合約、Design Invariants）。實作者 Opus 5.5／high（本卡）；設計審查、突變清單的敵對式檢查 Fable／high（主會話的 brief 指定）；第 3 題的 §14.4 檢證者 Opus／high（作者以外的 Agent）。

## 10. 測試、判定（先宣告；結果出來之後不放寬）

1. **既有行為不變**：除了第 9 節第 1 步表裡的 3 個測試（其中兩個計數判定搬到別的案，不拿掉），其餘 audio 測試不改判定、全部通過；CharacterizationTests 的雜湊常數不改。CTest 從 4 個變 3 個（第 3 題核准、拿掉掃描時）。
2. **換手**（新；由第 9 節第 1 步改寫的 3 個測試承擔）：一個 `Submitter` 在執行緒 A 呼叫 `Play`／`Stop`／`StopAll`／`LoadClip`／`UnloadClip`，`join` 之後在執行緒 B 繼續呼叫，沒有斷言、帳（DI 6）對得起來。**先在未改的程式上跑，確認它紅**（綁定的斷言觸發），紅的輸出記 dev_log。
3. **ThreadSanitizer**（本機手動組態，同第 9 節第 0 步）：判定 2 的測試與既有壓力測試（`gyo_audio_playhead_stress`、「loads, plays, stops and unloads while another thread renders」）沒有報告；和第 0 步的基準比。
4. **連結**：`gyo_expect_audio_links` 照舊（只連 Base、Time）。

**突變**（AGENTS §14.5，只跑相關的）：
- **改之前**先跑一次本批會碰到的突變，當基準（結果記 dev_log）。
- **退役 14 條**（gyo2 跑不起來）：find 指向已刪檔案的 12 條——`au-soundloader-bits-unchecked`、`au-sdl-not-resumed`、`au-sdl-quit-twice`、`au-sdl-error-without-detail`、`au-sdl-header-includes-sdl`、`au-sdl-public-sdl`、`au-sdl-anchor-not-logged`、`au-sdl-anchor-wrong-frame`、`au-sdl-anchor-full-uncounted`、`au-sdl-anchor-head-not-published`、`au-sdl-render-anchor-shifted`、`au-sdl-anchor-logged-block-late`；target 是 gyo2 沒有的 `gyo_audio_sdl_tests` 的 2 條——`au-sdl-stop-one-block-late`、`au-sdl-late-stop-uncounted`（後者的 find／replace 和留下的 `au-ctl-late-stop-uncounted` 完全相同，README 第 1 條）。SDL 那批照自己的測試重立。
- **退役 4 條**（機制拿掉）：`au-sched-owner-unchecked`、`au-ctl-stop-owner-unchecked`、`au-ctl-stopall-owner-unchecked`、`au-ctl-unloadclip-owner-unchecked`。第 3 題核准時連 `au-ctl-board-read-in-render`。
- **改指向**：`au-link-engine` 連的 `GYO::Engine` 在 gyo2 不存在、`what` 已失真；它是 `gyo_expect_audio_links` 唯一的突變，不退役，改成連 gyo2 現有的目標（例 `GYO::Kernel`）並補 `guards`。`au-ctl-unload-marks-on-full`：find 在 `Mixer.cpp:337`、expect 對 `:257`，第 1 步不碰，實跑確認。
- **新增 1 條**（README 第 3 條：判定 2 是新的判定項目）：在 `Play` 插回一個執行緒檢查（第一次呼叫記下執行緒、之後不同就斷言），預期判定 2 的換手案以斷言失敗；`guards: ["audio-review-declaration 判定 2"]`。它是「實作退回去」，不是呼叫端的誤用。
- 碰到的條目補 `guards`（README 第 9 條）。清單說明裡的執行器路徑改成 `tests/common/mutation/run_mutations.py`。
- 碰到的突變清單受一次別的模型（Fable／high）的敵對式檢查（§14.2）；合併前在背景跑一次完整清單。

## 11. 本卡照 L17 判的（不是使用者的話）

- 時間的邊界（4.1）與「預設時刻、固定步對應」的 owner 交 `world_audio`＋world_time。
- 套用點的「出」、一個套用 System 交（4.2）——照 O-12。
- 4.3 的分類（照 `-adapter-plugins-review` 的判準）。
- 4.4 的後端介面（只持參照、後端自己的 `OnDestroy`）。
- `DeviceUnavailable` 拿掉；`scheduleDelay` 預設值留、SDL 那批重推。
- 本批縮小、`world_audio` 不在本批（第 9 節）。
- 突變的退役與重看（第 10 節）。

## 12. 要使用者定的

### 第 1 題：RC 1 與 RC 5 已經成立——修還是重建提交端（AGENTS §3：成立時交使用者）

- **成立的根據**：RC 1「clips must be … played from a thread … other than the one that owns them」——World 交出（O-13）後，提交口在另一條執行緒上播；RC 5「the number or threads of producers cannot be fixed at creation」——執行緒隨 World 交出而變，數量隨讀檔換 World 而長（O-8）。
- **選項**：
  - **(A) 修**（建議）：本批拿掉執行緒綁定（第 2 題）；數量的問題在 `world_audio` 那批定。RC 1、RC 5 照第 7 節改寫，RC 5 記「已成立、選修」。
  - (B) 重建提交端：例如改成多生產者佇列或可回收的提交口，重新驗證環的順序、clip 回收、即時安全（RC 6 列的四件事）。
- **取捨**：
  - (A)：本批的改動小（拿掉一個斷言、合約改寫，輸出不變）。數量的問題只是延後，兩個做法都有代價：「照 World 名字沿用」有 4.2 列的三個未解問題；「讓提交口可以還」要重驗 RC 6——正是 (B) 的費用。所以 (A) 可能在 `world_audio` 那批仍走到重驗。
  - (B)：一次解兩件，但 `world_audio` 的形狀還沒定，重建的需求可能猜錯。
- **和第 2 題的組合**：第 1 題 (A) 以第 2 題選 (A) 或 (D) 為前提；第 2 題選 (B) 等於第 1 題選 (B)；第 2 題選 (C) 時 RC 1 仍成立、和第 1 題 (A) 矛盾。

### 第 2 題：`Submitter` 不綁執行緒（推翻交換點宣告 5.6 第 372 行與 E-16）

- **背景**：交換點宣告寫「`Submitter` 綁執行緒、自己記；World 交出到別的執行緒時要在那條執行緒上另拿一個」，並寫「audio 重審照它」。本卡查到：綁定不是 SPSC 需要的（4.2）；交出後另拿的提交口播不了原本載的 clip、停不了原本送出的聲音（clip 格子與 handle 是提交口私有的）。
- **選項**：
  - **(A) 不綁執行緒**：同一時間一方使用、換手要 happens-before（建議）。
  - (D) 不綁執行緒，debug 建置偵測「兩方同時進入」（進出時原子計數）。
  - (B) 照交換點宣告綁，並讓提交口可以還（重驗 RC 6）。
  - (C) 照交換點宣告綁、不還：每條執行緒一個提交口。
- **取捨**：
  - (A)：拿掉一個斷言、退 4 條突變、立 1 條（第 10 節）；同時使用變成資料競爭，只有手動的 ThreadSanitizer 建置抓得到，CI 抓不到，執行時不斷言。
  - (D)：檢查的是新契約寫明的那一句（同時進入），能當場抓到 (A) 抓不到的錯；費用是自己的測試與突變。它算不算你說的「為防誤用加的守衛」，由你判。
  - (B)：守住當場斷言，費用是重驗 RC 6。
  - (C)：交出後的 World 拿新的提交口，原本載的 clip 與送出的聲音帶不過去，要在交出前卸載、停掉、在新的那條執行緒重載；RC 1 仍成立。
- **別家**：Kira `Send`＋`&mut self`（Rust 的編譯器擋同時使用；C++ 沒有對應物）。
- **(A) 核准後**：交換點宣告第 110、372 行要改，那份是主會話的文件，本卡照四樣轉給主會話。

### 第 3 題：拿掉 DI 4 的原始碼掃描守衛 `gyo_audio_board_readers`（現在定方向；合併前照 §14.4 正式核准）

1. **它守的性質**：音訊執行緒不讀播放位置的看板（看板只供顯示）。
2. **緩的面**：以前，`Mixer.cpp` 以外的檔或 `Mixer::PlayheadOf` 以外出現 `board.Read(`，或整個目錄的讀不是恰好 1 個（含 0 個），就失敗；之後都不失敗。
3. **緊的面**：沒有。
4. **性質還有誰守**：**沒有別的判定守「音訊執行緒不讀」**，拿掉後只剩設計審查。相鄰但不同的性質「使用者讀看板不改輸出」由「output does not depend on someone reading playheads」（`PlayheadTests.cpp:178-199`）守，它不守本條。
5. **根據**：AGENTS §3「実装の方式に縛るガード」；掃描綁函式名、檔名與寫法，改名就要改守衛；讀看板不影響即時安全。
- **選項**：**(A) 拿掉，改由審查**（建議）；(B) 留，只隨程式改 find；(C) 換成型別守：看板的寫入端與讀取端分成兩個型別，音訊執行緒那一側只拿得到寫入端（由編譯器守，不綁檔名）。
- **取捨**：(A) 少一個綁寫法的守衛，「音訊執行緒不讀」只剩審查。(B) 不動，改名時要改守衛。(C) 守住性質又不綁檔名，費用是改 `PlayheadBoard` 的形狀與它的測試（DI 4 的守衛群），本批範圍變大。
- **合併前的證明**：(A) 的話，把 `au-ctl-board-read-in-render` 對**留下來的**測試（`gyo_audio_tests`＋ThreadSanitizer 的壓力測試）跑，預期存活——這是第 4 項「沒有別的判定守」的實測（它原本的 `command` 只跑被拿掉的掃描，跑那個證明不了什麼）；再由別的檢證者（Opus／high）用實際原始碼確認 1～5，請你正式核准。

### 第 4 題：Design Invariants／Replacement Criteria 的改動方向（第 7 節的草案；最終文字在合併前核准），含兩處和別家不同

- 第 7 節的條文改動都是 DI 節的改動，要你同意（AGENTS §3）。現在問方向。
- **和別家不同的兩處**：
  - **不搶占**（新 RC 9）：Unity 有優先度與虛擬 voice；FMOD、Wwise〔知〕也有；Kira 和 GYO 一樣不搶占（但 Kira 把錯誤回給該次呼叫者，GYO 只記總數）。
  - **暫停、倍率不碰聲音**（DI 3 的新句、RC 2 的補句）：Bevy、Kira 不碰；Godot、Unreal 預設會帶著聲音暫停（未查證）。
- **建議**：照第 7 節；這兩件等點名的 Feature 出現再照 RC 9、RC 2 處理。

### 第 5 題：本批縮小，三件事收尾時仍開著

- **三件**（都寫「`world_audio` 那批」，但**批次表還沒有這一批**，`git grep world_audio -- docs` 只命中本頁）：
  1. **預設的起播時刻**：只拿這一幀的取樣時刻，起播落在幀格上；在它定之前，A-1（槍聲在事件的時刻響）做不到。概念層 2.1 的「不相合」只解了一半。
  2. **提交口的數量**（RC 5，第 1 題）。
  3. **clip 的來源**（第 5 節：`ClipBank` 該不該存在、共用還是複製、asset 的 Handle）。
- **選項**：
  - **(A) 同意縮小**，請主會話在批次表加 `world_audio` 一批（建議：asset 實作合併之後、SDL 那批之前或同一批），三件交它；本卡把三件連同出處轉給主會話。
  - (B) 第 1 件在本批定（要和 world_time 一起定固定步對應的時刻，範圍跨到 world_time）。
- **取捨**：(A) 三件都要等使用者流程（asset、World 的請求緩衝）定了才判得準，本批只做現在就能驗證的部分；代價是 `world_audio` 之前 audio 的時間契約只有邊界。(B) 早一點補上 A-1，但沒有使用者流程，固定步的時刻要先猜，而且動到 world_time 的 Design Invariants。

## 13. 和其他卡的協調（經主會話）

- **主會話**：(1) 第 7 節的文件位置（`target/v2/audio.zh-Hant.md`＋`audio.md` 的 Legacy 行＋`target/v2/README.md` 加一列；`target/README.md:31` 說定之前先問架構師）；(2) 第 2 題 (A) 核准後改交換點宣告第 110、372 行；(3) 第 5 題：`world_audio` 的排程與第 4.1、4.2、5 節留給它的事；(4) 「送進 World 外服務的時刻都是時間基準」要不要成為跨 Component 的規則，請圖書管理員對照 `cross-component-rules.md`。
- **asset（`gyo2-asset`）**：asset 宣告 2.2 寫「音效（audio 重審那批）」。本卡建議改到 `world_audio` 那批（loader、型別特性，和第 5 節的 clip 來源一起定）。不改 asset 的任何東西。
- **threads／io（`gyo2-threads-io-review`）**：不動 threads。若 threads 那批定了「綁執行緒的物件」的共通寫法，`Submitter`（第 2 題 (A)）不在其列。

## 14. 範囲の限界

- FMOD、Wwise 的官方頁讀不到，標〔知〕；Unity 的部分頁面是摘要讀的；Unreal、Godot 沒查。
- 本批不實作 `world_audio`、loader、後端；第 4.1、4.2、4.4、5 節是給它們的契約與選項，**整合的行為要到那幾批才有測試**。4.1 的「預設時刻」是未解的缺口，本批不宣稱解掉概念層 2.1 的「不相合」，只解掉「可以隨手拿 World 裡任何時間」這一半。
- 基準只在本機 Intel Mac 跑過（L1 三平台在 PR 上跑）。
- **ThreadSanitizer 不在 L1**：gyo2 的 CI 與 preset 都沒有它。判定 3 與第 2 題 (A) 的「資料競爭抓得到」只靠本機的手動組態，PR 上的 CI 不檢查。
- `scheduleDelay` 的預設值沒有在 gyo2 上量過（沒有後端）。

## 15. 修訂 0 的敵對式檢查：處理對照

| # | 處理 |
|---|---|
| B-1 | 採用。4.1 改成「邊界＋未解的缺口」，DI 3 新句改成邊界（任何時間基準上的時刻，不用可縮放的時間、System 不讀時鐘）；預設時刻與固定步對應列為缺口、owner `world_audio`＋world_time；第 11 節不再寫「不推翻舊決定」 |
| B-2 | 採用，並改範圍：`ClipBank`／`Clip` 不在本批；三個後果與第四個選項 (D) 寫進第 5 節給 `world_audio` 那批 |
| S-1 | 採用：第 12 節第 1 題 |
| S-2 | 採用：照名字沿用移出卡片自判，問題列進 4.2「留給 `world_audio`」 |
| S-3 | 採用 (a)：本批只做執行緒契約、錯誤碼、文件、突變清理、守衛 |
| S-4 | 採用：時鐘型別不進 World，請求帶整數奈秒，由套用 System 換；請求緩衝不是判定資料 |
| S-5 | 採用：限界改用「同一行程的時間基準」表述，寫症狀與 runner 取樣後的情形 |
| S-6 | 採用：改引 `-adapter-plugins-review`，照該判準重做 4.3 的表，補晚到、`scheduleDelay` |
| S-7 | 採用：第 3 題補第 2 項、第 4 項直述「沒有別的判定」；分兩段問 |
| S-8 | 因範圍縮小不適用（判定 3～5 隨 `Clip` 移出）；`world_audio` 那批要照這幾點寫判定（記在第 5 節 (A) 的後果） |
| S-9 | 採用：死的 14 條；`au-ctl-unload-marks-on-full`、`au-link-engine` 實跑確認；不新增與既有重複的；補 `guards`、改執行器路徑、改之前跑基準、清單受敵對式檢查 |
| S-10 | 因範圍縮小大半不適用；剩下的 3 個執行緒測試在第 9 節第 1 步列出、照 §14.4 寫說明 |
| S-11 | 採用：加選項 (D)；happens-before 寫明並舉不算的例子；「斷言就會觸發」加「盡力而為」的條件 |
| S-12 | 採用：別家逐家標根據；暫停的分歧列進第 12 節第 4 題 |
| S-13 | 採用：第 5 節選項 (D)；`Clip` 活過 store 項目、只依賴 asset 的說法不準，都移到第 5 節給那批 |
| S-14 | 採用：4.4 改成「只持參照、後端自己的 `OnDestroy`」並寫登記先後 |
| S-15 | 採用：位置照 `target/README.md:29`，經主會話確認（第 13 節） |
| S-16 | 採用：第 3 節加根據表、規模、前提表；合併前核准照 GYO2 慣例（設計審查後交使用者） |
| S-17 | 採用：第 6 節第 3、4、6、7 項補齊；(b) 的不準理由隨範圍縮小拿掉 |
| N-1 | 採用（第 0 節） |
| N-2 | 採用（第 1 節：上限 64、預設 4） |
| N-3 | 採用（第 12 節第 2 題的背景改寫） |
| N-4 | 採用：Unity Netcode 改引本機原始碼 |
| N-5 | 採用（第 8 節、第 12 節第 4 題） |
| N-6 | 隨 `Clip` 移出，記給 `world_audio` 那批 |
| N-7 | 採用（第 7 節 DI 1、DI 3、DI 10） |
| N-8 | 採用（開頭「經過」與第 14 節） |
| N-9 | 採用：估時改 6～9 h（修訂 2 再改 7～11 h）；`world_audio` 依賴 world_time 寫進第 9 節 |

（修訂 1 的對照裡 S-8、N-6 寫「記在第 5 節 (A)」、N-3 寫「採用」，實際沒做；修訂 2 補上，見第 16 節 R1-N2、R1-S5。）

## 16. 修訂 1 的窄複查：處理對照

| # | 處理 |
|---|---|
| R1-S1 | 採用：第 7 節加 DI 7 的改寫與 Contract 的「owner thread」三處；第 6 節第 3 項補 DI 7、DI 10 |
| R1-S2 | 採用：第 9 節第 1 步加逐判定表，兩個計數判定搬到別的案；判定 1 跟著改 |
| R1-S3 | 採用 (a)：立 1 條「插回執行緒檢查」的突變，守判定 2；判定 2 另要先在未改的程式上確認紅 |
| R1-S4 | 採用：第 9 節加第 0 步（TSan 基準、組態、證據位置）；判定 3 寫組態；第 14 節加「TSan 不在 L1」；第 2 題 (A) 的取捨改寫 |
| R1-S5 | 採用：4.2 與第 2 題換成「clip 與 handle 是提交口私有的」的論證；(C)、(D) 改成中性敘述；「使用者原則」標成卡片的讀法 |
| R1-S6 | 採用：第 1 題的取捨照實寫；RC 5 記「已成立、選修」；加和第 2 題的組合說明 |
| R1-S7 | 採用：第 7 節 DI 3 寫逐字草案，只放混音器的輸入、守衛：審查；`world_audio` 的時間設計移到 4.1「留給」一段，補「讀 `RealTime` 的只能是表現層」；第 13 節加圖書管理員對照 CCR |
| R1-S8 | 採用：第 9 節第 7 步合併前核准頁；第 7 節標草案；第 4 題寫明現在問方向 |
| R1-S9 | 採用：第 12 節第 5 題 |
| R1-S10 | 採用：第 3 題的突變改對留下來的測試跑，當第 4 項的實測 |
| R1-N1 | 採用（判定 1、2） |
| R1-N2 | 採用：第 5 節末加「那一批寫判定時要照的」 |
| R1-N3 | 採用（4.4 第 2 點） |
| R1-N4 | 採用（第 6 節第 4 項、第 13 節） |
| R1-N5 | 採用（4.3 表的「晚到」列） |
| R1-N6 | 採用：`au-link-engine` 改指向、不退役 |
| R1-N7 | 採用：RC 10 併進 RC 2 |
| R1-N8 | 採用：A-6 改成 Component；前提表加兩條卡片的假設 |
| R1-N9 | 採用：退役改列 id、寫 `au-sdl-late-stop-uncounted` 的重複；`NoClipSlot` 留 5 |
| R1-N10 | 採用：第 3 題加選項 (C)；「実装の方式に縛るガード」改正 |

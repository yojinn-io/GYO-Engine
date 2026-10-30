# 音效（GYO::Audio）：交接

更新：2026-10-30。**狀態：`v8-engine-audio-plan` 完成（2026-10-30 使用者核准，6 項待決事項照建議）；`v8-engine-audio-core` 完成（2026-10-30，本機 L1 與突變 9／9）；`v8-engine-audio-schedule` 完成（2026-10-30，本機 L1、xhigh 審查、突變 21／21＋TSan 4／4）；`v8-engine-audio-sdl` 完成（2026-10-30，本機 L1、突變 5／5）；`scheduleDelay` 的預設值定為 60 ms（使用者決定）；`v8-engine-audio-record` 停在決策點（2026-10-30）：量測與紀錄完成，但 disk 驅動計時的前提不成立，等使用者決定 `v8-engine-audio-l2-premise`。**
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。依 Engine 計畫夾的慣例，不點名消費端。

## 現況

- 2026-10-30：`v8-engine-audio-plan` 完成文件（本夾 README、PLAN、HANDOFF），同日使用者核准 6 項待決事項全部照建議（見決策紀錄）；dev_log：[2026_10_30_engine_audio_plan](../../../dev_logs/2026_10_30_engine_audio_plan.zh-Hant.md)。
- `docs/architecture.md` 的 Runtime boundaries 與 Architecture Delta 段落在 `v8-engine-audio-core` 加上（程式出現時才描述）。
- 2026-10-30：使用者說「開始吧」，開始 `v8-engine-audio-core`（high，估 3～4 h）。做法：樣本域的聲部混音做成模組內部類別 `VoiceMixer`（`engine/audio/src`，以樣本位置排程，不含時間換算與佇列），L1 直接測它；公開的 `Mixer`（提交端與時間戳換算）在 `v8-engine-audio-schedule` 包在它外面。這取代 PLAN 寫的「測試用的直接注入」，不在公開 API 留後門。
- 2026-10-30：`v8-engine-audio-core` 完成：`GYO::Audio`（`ClipBank`、內部 `VoiceMixer`）、`gyo_audio_tests` 13 案、SoundLoader 的第一批 L1、連結守衛、[audio.md](../../audio.md)（Design Invariants 的正本）；`ctest --preset core` 35／35，突變 9／9 被殺；dev_log：[2026_10_30_engine_audio_core](../../../dev_logs/2026_10_30_engine_audio_core.zh-Hant.md)。四平台 CI 未跑（PR 線一起跑）。
- 給 schedule 批：`VoiceMixer::Schedule` 含 `GYO_ASSERT`、不是 `noexcept`；PLAN 草案的 `Submitter::Play` 寫成 `noexcept`，要在生產者端驗證就不能 `noexcept`。開始時先定這兩點。
- 2026-10-30：使用者說「開始 v8-engine-audio-schedule。」開始（high，換算、遲到、SPSC 局部 xhigh，估 4～5 h）。開始時定下：①`Submitter::Play` 不是 `noexcept`，在生產者端以 `GYO_ASSERT` 驗證片段與增益；`Render` 改呼叫 `VoiceMixer` 不檢查的 `noexcept` 版本。②提交端的佇列在建立混音器時全部配置好，`CreateSubmitter` 以 atomic 領取一格，任何時候都可呼叫（不必限制在輸出開啟前）。③提交端與混音器以 `shared_ptr` 共用狀態，混音器先銷毀也安全（音效執行緒不持有參考）。④樣本位置改用有號整數，錨點之前的時間（例如第一個區塊之前的事件）才能判為遲到。
- 2026-10-30：**Design Invariant 5 的修訂（v8-engine-audio-invariant5，使用者同意）**：原文「同錨點下兩起點的差＝round(Δ×48000)」在三個以上請求時做不到（各自四捨五入，round(a)−round(b) 可和 round(a−b) 差 1）。使用者選「改成誤差小於 1 樣本」：每個起點是時間戳相對錨點的偏移四捨五入到最近的樣本，同錨點下兩起點的差與 Δ×48000 相差不到 1 個樣本。緩的面：原文要求恰好相等的情況現在容許差 1；締的面：明確規定每個起點本身的位置；守衛：逐一檢查起點等於各自偏移的四捨五入。
- 2026-10-30：`v8-engine-audio-schedule` 完成：公開 `Mixer`、`Submitter`、時間戳換算；`gyo_audio_tests` 32 案；xhigh 審查的確定問題（註解與契約）已修、測試缺口已補；`ctest --preset core` 35／35；dev_log：[2026_10_30_engine_audio_schedule](../../../dev_logs/2026_10_30_engine_audio_schedule.zh-Hant.md)。四平台 CI 未跑。
- 2026-10-30：使用者說「開始 sdl」，開始 `v8-engine-audio-sdl`（high，估 3～5 h）。途中使用者補充：「保持原則，sdl時不會直接被外部調用的。」做法：①後端不提供 `*Native` target，SDL 只在 `engine/audio/backend/sdl/src` 內；L1 也不 include SDL、不呼叫 SDL（驅動以 CTest 的環境變數選擇，子系統計數以行為檢查）。②照既有的 component 機制新增 `SDL_AUDIO`（產品在 project.json 宣告才建置與連結；不隱含 `SDL_PLATFORM`），full 測試設定檔加入它。③`SdlAudioOutput::Open` 接收 `Mixer` 的所有權（取代 PLAN「Mixer 必須比輸出活得久，以 GYO_ASSERT 檢查」：所有權讓順序不會錯，也不必在解構子 assert）；提交端與混音器共用狀態，開啟失敗時仍可送出（無聲）。
- 2026-10-30：`v8-engine-audio-sdl` 完成：`GYO::AudioBackendSDL`、component `SDL_AUDIO`、G1～G3 擴充、`audio_sdl.*` 3 個 CTest、量測程式；`ctest --preset test` 97／97、core 35／35；macOS 的區塊大小已量（dev_log 的表），Windows 未量；dev_log：[2026_10_30_engine_audio_sdl](../../../dev_logs/2026_10_30_engine_audio_sdl.zh-Hant.md)。
- 2026-10-30：使用者決定「預設 60 ms」：`MixerConfig::scheduleDelay` 預設改為 60 ms，加一案守住預設值；依賴預設 0 的兩案改為明寫 0。core 35／35、audio 相關 5／5。
- 2026-10-30：使用者說「開始 record」，開始 `v8-engine-audio-record`（medium；分析程式 high，估 2～4 h）。L2 只記錄，不是判定，所以不事先宣告門檻（AGENTS §14.1 的宣告留給消費端的判定）。做法：①程式 `gyo_audio_onset_record`（不是 CTest）以 `Time::Waiter` 跑 30 FPS 的幀迴圈，在 60 Hz Tick 格點上以 6、7、5 Tick 循環的間隔產生事件，在事件之後的下一幀才送出，時間戳用事件的 Tick 時間；每幀記下 `(Now, renderedFrames)` 估計裝置速率（不新增 API）。②驅動與 hint 由 Python 腳本以環境變數設定（程式與腳本都不呼叫 SDL）：`disk`、每塊 480、timescale 1；另跑一次預設裝置（只有計數與回呼統計，沒有檔案）。③分析：在檔案裡找起音，和事件逐一對應，報告間隔誤差（對 Δt×48000 與以估計速率修正後兩組）、對 Tick 格點與幀格點的距離、收支。
- 2026-10-30：`v8-engine-audio-record` 停止：30 秒紀錄的收支一致（299／299），真實裝置 299 個全部準時；但 disk 驅動在 macOS 上比 Engine 時鐘慢約 7% 而且會漂移，速率修正後的間隔誤差最大 10.9 ms，分不出 Tick 格點和幀格點（121／198）。PLAN「每塊 480 就能準確計時」的前提不成立（AGENTS §14：原因改變計畫的前提）。dev_log：[2026_10_30_engine_audio_record](../../../dev_logs/2026_10_30_engine_audio_record.zh-Hant.md)。
- 分支：`claude/v8-engine-audio-95a496`（切自消費端的 v8 計畫分支，未推送）。

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-engine-audio-plan` | 建本計畫夾、定介面與約束（本批） |
| `v8-engine-audio-core` | 片段庫、聲部與離線混音 |
| `v8-engine-audio-schedule` | 提交佇列與「時間戳 → 樣本位置」 |
| `v8-engine-audio-sdl` | SDL 輸出後端 |
| `v8-engine-audio-record` | disk 驅動的量測（只記錄）與收尾 |
| `v8-engine-audio-producers` 等 | 2026-10-30 核准的 6 項決策（決策紀錄） |

## 閱讀入口

1. [進度與執行規則](README.md)。
2. [批次計畫](PLAN.md)：範圍、公開介面草案、Design Invariants（草案）、Replacement Criteria、分批、驗收。
3. 本文件：等使用者決定的事、研究事實、消費端的壓力摘要。
4. 相關契約：[time.md](../../time.md)（時間基準）、[platform-sdl.md](../../platform-sdl.md)（SDL 隔離與 Invariant 1、2、8）、[error-handling.md](../../error-handling.md)。

## 等使用者決定

### `v8-engine-audio-l2-premise`：起音量測的做法（2026-10-30 提出）

- 現象：disk 驅動以每塊之後的 `SDL_Delay(10)` 模擬即時；macOS 上每次晚醒約 0.75 ms，裝置慢約 7%，而且會漂移。混音器以錨點和名目 48000 換算，起點因此偏差 7% ×（事件到錨點的距離＋delay），約 1～4 ms；加上漂移，最大 10.9 ms。
- 影響：消費端原本要以 disk 驅動判定「起音間隔對應射擊 Tick，而不是 33 ms 的幀格點」，這需要誤差小於約 8 ms，disk 驅動在 macOS 上做不到。
- 選項：
  - **A（建議）加一個診斷：輸出記錄每次回呼的錨點 `(T0, F0)`**（有上限的環，只供紀錄與驗收）。分析時用錨點重建每個請求「應該」落在的 frame，逐一比對起音，預期誤差 0 frame，和 disk 的計時無關；間隔也能以錨點換回 Engine 時間。代價：後端多一個中立的診斷 API（約 1～1.5 h），需要 audio.md 補契約。
  - B：保留現狀，消費端的門檻納入模型項（速率誤差 × 距離），並改在 Linux／Windows 的 disk 驅動上量（晚醒較小時誤差較小，未量）。
  - C：消費端的 L2 改為離線：把真實的事件時間戳與假錨點餵給 `Mixer::Render`，不經裝置；量得到換算，量不到裝置。
  - D：調 `SDL_AUDIO_DISK_TIMESCALE` 補償：只能以整數毫秒調，macOS 的晚醒又會變，不建議。
- 證據：[record 的 dev_log](../../../dev_logs/2026_10_30_engine_audio_record.zh-Hant.md)；主 checkout 的 `build/target/_build/test/logs/audio-onset-record-20261030/`。


### `v8-engine-audio-delay-default`：`scheduleDelay` 的預設值（2026-10-30 提出；同日決定 A，見決策紀錄）

- 機制：請求的時間戳是事件發生的時刻。消費端最晚在下一幀才送出（30 FPS 時最多 33.3 ms），音效執行緒最晚在下一次回呼才取出（macOS 預設區塊最多 21.5 ms，每塊 480 時 10.9 ms）。要準時，`scheduleDelay` 至少是兩者之和；不夠時聲音貼在區塊開頭播放，連射間隔就被打亂（計入 `playedLate`）。
- 取捨：`scheduleDelay` 也是本機射擊聽到聲音的延遲下限。
- 數據（macOS Intel、CoreAudio）：30 FPS＋預設區塊 ≈ 55 ms；30 FPS＋480 ≈ 44 ms；60 FPS＋預設 ≈ 38 ms；60 FPS＋480 ≈ 28 ms。Windows 未量。
- 選項：**A（建議）預設 60 ms**：30 FPS 與 SDL 預設區塊下不遲到；消費端可依自己的幀率與區塊調低。B：預設維持 0，要求消費端一定要設定：忘了設定時每個聲音都遲到（只表現在計數）。C：等 Windows 量完、或等 `v8-engine-audio-record` 的 L2 再定。
- 證據：[sdl 的 dev_log](../../../dev_logs/2026_10_30_engine_audio_sdl.zh-Hant.md) 的表；`tests/common/audio_sdl/BlockRecord.cpp`。

### 已決定（2026-10-30 核准前的原文）

下表是 2026-10-30 核准前的原文（每項附機制、取捨、證據位置；第一個選項是建議），保留作為決策的依據；結果見決策紀錄。

| 代號 | 問題 | 選項與取捨 | 證據 |
|---|---|---|---|
| `v8-engine-audio-producers` | 播放請求從幾個執行緒來、怎麼進音效執行緒 | **A（建議）每個生產者執行緒一個提交端**：建立混音器時固定上限（預設 4），每個提交端是 SPSC，混音器在回呼裡依序取出。無鎖、簡單、可證明；代價是生產者要在開始前登記，且同一時刻的兩個請求跨提交端時，取出順序不代表時間順序（起點仍由時間戳決定，不受影響）。<br>B 只有一個 SPSC：消費端要自己把各執行緒的事件匯到一個執行緒，等於把佇列做在產品裡。<br>C 無鎖 MPSC：任何執行緒都能送；正確性更難證明，需要更多 xhigh 時間（估 +2～3 h） | 消費端的 Client 有主執行緒、模擬、網路三個角色（`apps/object_fps_pvp/src/Pvp/ClientSimulationRole.cpp:150`、`ClientConnection.cpp`）；本機射擊與套用 snapshot 的事件可能出在不同角色 |
| `v8-engine-audio-clip-format` | 片段資料怎麼交給混音器、非 48 kHz 怎麼辦；`SoundLoader` 的測試誰補 | **A（建議）**：`GYO::Audio` 自己的 `ClipBank` 複製 PCM16；非 48 kHz 回錯；消費端把 `SoundAsset` 的欄位交進來；`SoundLoader` 的測試在 `v8-engine-audio-core` 順手補（約 0.5 h，因為這是第一次有人用它）。<br>B：`GYO::Audio` 依賴 `GYO::Engine` 直接收 `SoundAsset`：少一層轉交，但新增 Audio → Engine 的邊，離線測試要連整個 Engine。<br>C：加重新取樣：資產可以任意取樣率，但多一段要驗證的 DSP；消費端已決定以腳本產生 48 kHz WAV（消費端 v7 D38，待它確認） | `engine/asset/include/engine/asset/loaders/SoundLoader.hpp:16-21`、`engine/asset/src/loaders/SoundLoader.cpp`（沒有檢查取樣率）；repo 內沒有任何使用者或測試（`grep -rn SoundLoader apps tests engine`） |
| `v8-engine-audio-delay` | 排程延遲 `D`（加在每個起點上的常數）預設多少 | **A（建議）可設定，預設值在 `v8-engine-audio-sdl` 量完 macOS 與 Windows 的實際區塊大小後提案**（只記錄，例如兩個區塊長）。<br>B 現在固定（例如 30 ms）。<br>取捨：`D` 大，本機射擊聽起來延遲多；`D` 小，晚到的請求變多，遲到的聲音貼在區塊開頭，連射間隔就被破壞。`D` 必須至少涵蓋「事件發生 → 下一幀送出 → 下一次回呼」，30 FPS 時光是幀量化就有最多 33 ms | PLAN「時間戳換算成樣本位置」；區塊大小的預設見下面研究事實 2 |
| `v8-engine-audio-late` | 遲到的請求怎麼處理 | **A（建議）立刻在區塊開頭播放，計入 `playedLate` 與 `maxLateness`**：玩家仍聽得到，計數讓 L2 看得出來。<br>B 遲到超過某個上限就丟棄並計數：避免很舊的聲音（例如卡頓後一次湧入的腳步聲）疊在一起；上限要另外定。可以等 L2 或消費端的批次觀測到需要時再加 | — |
| `v8-engine-audio-l2` | L2 怎麼處理 `disk` 驅動的計時誤差；Engine 是否有自己的 L2 | **A（建議）**：Engine 有自己的 L2（只記錄），固定 `SDL_AUDIO_DEVICE_SAMPLE_FRAMES=480`（10 ms 整，不截斷），並以記下的錨點估計速率、報告修正前後兩組誤差；消費端的 L2 判定門檻在消費端的音效批開始前宣告。<br>B：只有消費端的 L2：少 2～3 h，但門檻沒有 Engine 端的來源，速率誤差會混在產品的量測裡。<br>這一項是 L2 的「構想」，不是判定；門檻依 AGENTS §14.1 在看結果前另外宣告 | 研究事實 3 |
| `v8-engine-audio-gears` | 分批、檔位與引擎 | 照 PLAN「分批」：四批都是 high（或 medium），schedule 局部 xhigh 用 `xhigh-reviewer`，引擎全部繼承（Opus 5.5）；沒有高於主對話的項目。是否需要 ultracode：不建議（介面與風險集中在 schedule 一批，xhigh 審查者 1 人已足夠） | `agents/effort-gearbox.md`；消費端計畫的 audio-line 列（已核准「局部 xhigh 用 `xhigh-reviewer`（引擎繼承）」） |

本計畫的範圍（PLAN「不做」）若要調整，例如 v8 就要迴圈或停止聲音，另提給使用者決定；那會改變 Replacement Criteria 2 與介面形狀。

## 決策紀錄

| 日期 | 決策 | 來源 |
|---|---|---|
| 2026-10-08 | （繼承）Engine 子系統的分配包含 Audio；每個 Engine 子系統與它的第一個消費端放在同一個 PR | 消費端 v7 D27；時間、執行緒與 Trace 計畫 |
| 2026-10-08 | （繼承）audio 計畫夾在音效批開始時建立；2026-10-14 改為在消費端 v8 的音效批開始時建立 | 消費端 v7 D28、D76 |
| 2026-10-30 | （繼承）消費端 v8 把音效排第一優先；Engine 部分分為 plan → 實作兩批，實作局部 xhigh 用 `xhigh-reviewer`（引擎繼承） | 消費端 v8 計畫的 audio-line |
| 2026-10-30 | `v8-engine-audio-producers`＝A：每個生產者執行緒一個 SPSC 提交端，建立混音器時固定上限（預設 4） | 使用者：「audio 計畫的 6 項待決事項都照建議核准」 |
| 2026-10-30 | `v8-engine-audio-clip-format`＝A：`ClipBank` 複製 PCM16，非 48 kHz 回錯，`GYO::Audio` 不依賴 `GYO::Engine`；`SoundLoader` 的測試在 `v8-engine-audio-core` 補 | 同上 |
| 2026-10-30 | `v8-engine-audio-delay`＝A：`scheduleDelay` 可設定，預設值在 `v8-engine-audio-sdl` 量完 macOS 與 Windows 的區塊大小後提案 | 同上 |
| 2026-10-30 | `v8-engine-audio-late`＝A：遲到的請求在區塊開頭立刻播放，計入 `playedLate` 與 `maxLateness`，不丟棄 | 同上 |
| 2026-10-30 | `v8-engine-audio-l2`＝A：Engine 有自己的 L2（只記錄），固定 `SDL_AUDIO_DEVICE_SAMPLE_FRAMES=480`，以錨點估計速率並報告修正前後的誤差；消費端 L2 的門檻在消費端音效批開始前宣告 | 同上 |
| 2026-10-30 | `v8-engine-audio-invariant5`：audio.md Design Invariant 5 改為「每個起點＝偏移四捨五入；同錨點兩起點的差與 Δ×48000 相差不到 1 樣本」 | 使用者（AskUserQuestion 選「改成誤差小於 1 樣本（建議）」） |
| 2026-10-30 | `v8-engine-audio-invariant5-clamp`：xhigh 審查後，Invariant 5 加上「對錨點前後 1 小時內的時間戳；超過的偏移夾在 1 小時」，捨入寫明 floor(x+½)（負數也往 +∞） | 使用者（AskUserQuestion 選「加限定語並寫明捨入（建議）」） |
| 2026-10-30 | `v8-engine-audio-far-ahead`：太遠未來的請求一取出就算準時並佔住聲部（可能讓混音器飢餓）；先不改行為，記為未結事項，等 sdl 批定 delay 或消費端觀測到時再決定是否加前瞻上限 | 使用者（AskUserQuestion 選「先記錄，等壓力（建議）」） |
| 2026-10-30 | `v8-engine-audio-delay-default`＝A：`MixerConfig::scheduleDelay` 的預設為 60 ms（30 FPS＋macOS 的 SDL 預設區塊不遲到；消費端可調低）。Windows 的區塊大小未量 | 使用者：「預設 60 ms」 |
| 2026-10-30 | `v8-engine-audio-gears`：照 PLAN「分批」的檔位與引擎（全部繼承，schedule 局部 xhigh 用 `xhigh-reviewer`），不用 ultracode | 使用者：「audio 計畫的 6 項待決事項都照建議核准」 |

## 研究事實（2026-10-30）

1. **SDL 版本**：`third_party/sdl3/CMakeLists.txt:10` 是 `release-3.4.0`；以下行號取自主 checkout 的 `build/target/_build/test/_deps/sdl3-src`（版本號 3.4.0，`include/SDL3/SDL_version.h:56,65`）。
2. **預設區塊大小**：`src/audio/SDL_audio.c:151-165` 的 `SDL_GetDefaultSampleFramesFromFreq`：有 `SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES`（`SDL_hints.h:345`）就用它，否則 48 kHz 時是 1024 樣本（21.33 ms）。實際裝置可能改變它（CoreAudio、WASAPI 各自決定），所以 `D` 的預設要實測。
3. **`disk` 驅動的計時**：`src/audio/disk/SDL_diskaudio.c:123` 的 `io_delay = sample_frames * 1000 / freq`（整數毫秒，截斷），`:125-131` 以 `SDL_AUDIO_DISK_TIMESCALE`（`SDL_hints.h:467`）縮放；`:35` 每塊之後 `SDL_Delay(io_delay)`（相對等待）。1024 樣本時延遲 21 ms 比實際 21.33 ms 短約 1.6%；`SDL_Delay` 的晚醒又讓它變慢。`SDL_AUDIO_DISK_TIMESCALE=0` 時不等待，適合只檢查結構的 L1。輸出檔名由 `SDL_AUDIO_DISK_OUTPUT_FILE`（`SDL_hints.h:454`）指定；驅動由 `SDL_AUDIO_DRIVER`（`:481`）選；也有 `dummy` 驅動（`src/audio/dummy`）。
4. **SDL 子系統的 owner**：Engine 內唯一的 `SDL_InitSubSystem` 是 `engine/platform/sdl/src/SdlPlatform.cpp:105`（VIDEO）。platform-sdl.md Invariant 8 規定各子系統由 owner 以參考計數初始化，所以 AUDIO 由音效後端擁有，不經 `SdlPlatform`。
5. **G3 的清單**：`tests/common/platform_sdl/CMakeLists.txt:82-86` 對 platform、main 與各後端呼叫 `gyo_expect_private_sdl`；新後端加進這份清單即可，不需要改 G3 的規則。
6. **`SoundLoader`**：`SoundLoader.cpp` 接受 PCM（格式 1）、16 位元、1 或 2 聲道，取樣率照讀不檢查；`engine/CMakeLists.txt:57` 編進 `GYO::Engine`。repo 內沒有程式或測試使用它。
7. **時間基準的型別**：`Engine::Time::TimePoint`／`Duration`（`engine/time/include/engine/time/MonotonicClock.hpp:19-22`），錯誤型別的寫法照 `Waiter.hpp:12-25`（`Base::Error<Code>` 加 `ToString`）。

## 消費端的壓力摘要

- 提出需求的消費端是一個 4 人連線 FPS，Client 以 30 FPS 以上繪製，模擬是 60 Hz 的固定步。它要的聲音：本機射擊（輸入事件的時間戳）、命中確認、受擊、換彈、遠端射擊、腳步。
- 它的原計畫（v7 交接「P6：音效」，2026-10-14 移到 v8）：中立混音器（PCM16、48 kHz、自己的樣本計數、即時安全的有界 SPSC、依時間戳換算樣本位置）＋SDL 後端（`SDL_OpenAudioDeviceStream` 的回呼）；L2 以 `SDL_AUDIO_DRIVER=disk` 在 30 FPS 連射，檢查起音間隔對應射擊 Tick，而不是 33 ms 的幀格點。本計畫照這個規格展開，沒有改變它。
- 音效資產的來源：消費端傾向以腳本程序產生 48 kHz WAV（可重現、授權單純），在它的音效批開始前確認。本計畫因此不做重新取樣（`v8-engine-audio-clip-format`）。
- 伺服器端程序沒有聲音；它的連結閉包不含音效 target 是消費端的檢查。

## 未結事項

- **太遠未來的請求佔住聲部**（`v8-engine-audio-far-ahead`，2026-10-30 使用者決定先記錄）：請求在取出時就計為準時；時間戳在未來（最多 1 小時，例如 `TimePoint::max()` 或錯誤的時間）時佔住一個聲部到那時。預設 32 個聲部，32 個這種請求就讓混音器只剩 `droppedNoVoice`。若要加前瞻上限，會在收支（Invariant 6）加第 5 種分類。
- **記憶體順序的守衛只在本機 TSan**：一般建置與 macOS 預設的 TSan 都抓不到環的 release／acquire 放寬；要 `TSAN_OPTIONS=ignore_interceptors_accesses=0`（`scripts/mutations-tsan.json`）。CI 沒有 TSan job。

- 真實裝置上的區塊大小、回呼抖動、裝置切換（拔耳機）時 SDL 的行為：在 `v8-engine-audio-sdl` 只記錄，Windows 在 CI 以外的實機只做 CI 做不到的項目。

## 時間紀錄

| 批次 | 估時 | 實際 | 備註 |
|---|---|---|---|
| `v8-engine-audio-plan` | 3～5 h（開始時重估：3 h，範圍明確、只寫文件） | 約 1.5 h（核准與記錄另約 0.2 h） | 閱讀 v8／v7 計畫、`SoundLoader`、SDL 隔離契約與 SDL 3.4 的音效驅動原始碼 |
| `v8-engine-audio-core` | 3～4 h | 約 1.5 h | 建置與突變在背景約 20 分鐘 |
| `v8-engine-audio-schedule` | 4～5 h | 約 2.5 h | 含 xhigh 審查約 18 分鐘、兩次 Invariant 5 的決定 |
| `v8-engine-audio-sdl` | 3～5 h | 約 2 h | 含完整 test preset 的建置與 CTest |
| `v8-engine-audio-record` | 2～4 h | 約 1.3 h（停止點） | 量測與 30 秒紀錄；停在 `v8-engine-audio-l2-premise` |

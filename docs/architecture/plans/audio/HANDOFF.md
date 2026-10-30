# 音效（GYO::Audio）：交接

更新：2026-10-30。**狀態：`v8-engine-audio-plan` 完成（2026-10-30 使用者核准，6 項待決事項照建議）；`v8-engine-audio-core` 完成（2026-10-30，本機 L1 與突變 9／9）；下一批 `v8-engine-audio-schedule` 等使用者指定。**
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。依 Engine 計畫夾的慣例，不點名消費端。

## 現況

- 2026-10-30：`v8-engine-audio-plan` 完成文件（本夾 README、PLAN、HANDOFF），同日使用者核准 6 項待決事項全部照建議（見決策紀錄）；dev_log：[2026_10_30_engine_audio_plan](../../../dev_logs/2026_10_30_engine_audio_plan.zh-Hant.md)。
- `docs/architecture.md` 的 Runtime boundaries 與 Architecture Delta 段落在 `v8-engine-audio-core` 加上（程式出現時才描述）。
- 2026-10-30：使用者說「開始吧」，開始 `v8-engine-audio-core`（high，估 3～4 h）。做法：樣本域的聲部混音做成模組內部類別 `VoiceMixer`（`engine/audio/src`，以樣本位置排程，不含時間換算與佇列），L1 直接測它；公開的 `Mixer`（提交端與時間戳換算）在 `v8-engine-audio-schedule` 包在它外面。這取代 PLAN 寫的「測試用的直接注入」，不在公開 API 留後門。
- 2026-10-30：`v8-engine-audio-core` 完成：`GYO::Audio`（`ClipBank`、內部 `VoiceMixer`）、`gyo_audio_tests` 13 案、SoundLoader 的第一批 L1、連結守衛、[audio.md](../../audio.md)（Design Invariants 的正本）；`ctest --preset core` 35／35，突變 9／9 被殺；dev_log：[2026_10_30_engine_audio_core](../../../dev_logs/2026_10_30_engine_audio_core.zh-Hant.md)。四平台 CI 未跑（PR 線一起跑）。
- 給 schedule 批：`VoiceMixer::Schedule` 含 `GYO_ASSERT`、不是 `noexcept`；PLAN 草案的 `Submitter::Play` 寫成 `noexcept`，要在生產者端驗證就不能 `noexcept`。開始時先定這兩點。
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

目前沒有。下表是 2026-10-30 核准前的原文（每項附機制、取捨、證據位置；第一個選項是建議），保留作為決策的依據；結果見決策紀錄。

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
| 2026-10-30 | `v8-engine-audio-gears`：照 PLAN「分批」的檔位與引擎（全部繼承，schedule 局部 xhigh 用 `xhigh-reviewer`），不用 ultracode | 同上 |

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

- 真實裝置上的區塊大小、回呼抖動、裝置切換（拔耳機）時 SDL 的行為：在 `v8-engine-audio-sdl` 只記錄，Windows 在 CI 以外的實機只做 CI 做不到的項目。

## 時間紀錄

| 批次 | 估時 | 實際 | 備註 |
|---|---|---|---|
| `v8-engine-audio-plan` | 3～5 h（開始時重估：3 h，範圍明確、只寫文件） | 約 1.5 h（核准與記錄另約 0.2 h） | 閱讀 v8／v7 計畫、`SoundLoader`、SDL 隔離契約與 SDL 3.4 的音效驅動原始碼 |
| `v8-engine-audio-core` | 3～4 h | 約 1.5 h | 建置與突變在背景約 20 分鐘 |

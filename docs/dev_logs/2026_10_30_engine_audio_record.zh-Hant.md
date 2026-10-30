# Engine v8-engine-audio-record：disk 驅動的起音量測（只記錄）

日期：2026-10-30。**停在決策點**：量測程式、分析腳本與 30 秒的紀錄都完成了，但結果推翻 PLAN 的一個前提：「disk 驅動固定每塊 480 樣本就能準確計時」。依 AGENTS §14 停下回報，不自行改做法。計畫見 [audio 計畫](../architecture/plans/audio/README.md)。

收支：一致（送出 299、起音 299、計數的丟棄 0；真實裝置送出 299、準時 299）

## 為什麼做

- `v8-engine-audio` 的最後一批（medium；分析程式 high，估 2～4 h）。使用者原話：「開始 record」。
- 這是 Engine 自己的 L2，只記錄、不當判定（`v8-engine-audio-l2`），用來提供消費端 L2 的門檻來源。

## 做了什麼

- `tests/common/audio_sdl/OnsetRecord.cpp` → `gyo_audio_onset_record`（不是 CTest）：
  - 以 `Time::Waiter` 跑 30 FPS 的幀迴圈，在 60 Hz Tick 上以 6、7、5 Tick 循環的間隔產生事件。
  - 每個事件在發生後的下一幀才送出，時間戳用它的 Tick 時間。
  - 每幀記下 `(Now, renderedFrames)`。
  - 程式不呼叫 SDL。
- `tests/common/audio_sdl/onset_record.py`（只用標準庫，有逾時）：
  - 以環境變數設定驅動。`disk` 模式：每塊 480、timescale 1、格式固定；`device` 模式：預設裝置。
  - 從輸出檔找起音，逐一對應事件，報告：間隔誤差（名目 48000 與以估計速率修正後兩組）、奇數 Tick 間隔是否比幀格點更接近 Tick 值、起音位置相對時間戳的散布、送出延遲。
  - 寫 meta.txt（commit、程式的 SHA-256、OS、負載）。

## 結果（macOS Intel x86_64，Kqueue，30 秒，負載 2.4）

證據：主 checkout 的 `build/target/_build/test/logs/audio-onset-record-20261030/`（report.txt、meta.txt、各模式的 events.csv、rate.csv、summary.txt、disk 的 audio.raw）。

| 項目 | disk（480／塊） | 真實裝置（CoreAudio，1024／塊） |
|---|---|---|
| 送出／準時／遲到／丟棄 | 299／299／0／0 | 299／299／0／0 |
| 起音數 | 299（振幅都是 1000） | 不適用（沒有檔案） |
| 回呼最長間隔 | 22.8 ms | 21.7 ms |
| 裝置速率（相對 Engine 時鐘） | 44664 frame／秒（−6.95%） | 不適用 |
| 間隔誤差，名目 48000 | 中位數 −6.74 ms，P99 11.9 ms，最大 18.2 ms | 不適用 |
| 間隔誤差，速率修正後 | 中位數 +0.21 ms，P99 4.25 ms，最大 10.9 ms | 不適用 |
| 奇數 Tick 間隔比幀格點更接近 Tick 值 | 121／198 | 不適用 |
| 起音位置的散布（去掉中位數） | P99 42 ms | 不適用 |
| 送出延遲（事件到送出） | 最大 16.8 ms | 同左 |

（8 秒的試跑：速率 −6.99%，速率修正後最大 4.06 ms，奇數 Tick 35／52。）

## 停止理由與經緯

- **前提不成立**：PLAN 以為每塊 480（`io_delay` 剛好 10 ms，不截斷）就能讓 disk 驅動和真實時間同步。實際上 macOS 的 `SDL_Delay(10)` 每次晚醒約 0.75 ms，disk 裝置慢約 7%，而且晚醒的量會變（30 秒裡速率不是常數，所以以一條直線修正後，起音位置的散布仍有 42 ms）。
- **誤差的來源**：混音器每塊以錨點和名目 48000 換算。disk 裝置的時間軸慢 7%，事件離錨點 `(at − anchor) + delay` 約 17～60 ms，所以每個起點有 7% × 該距離的偏差（約 1～4 ms）；加上速率本身漂移，最大到 10.9 ms。
- **對判定的影響**：區分「Tick 格點（16.7 ms）」與「幀格點（33.3 ms）」需要誤差小於約 8 ms。disk 驅動在 macOS 上做不到（121／198）；在真實裝置上速率誤差約 100 ppm 級，同樣的距離只有微秒級偏差，但真實裝置無法從檔案量起音。
- 這不是混音器的缺陷：L1 已以假錨點逐樣本驗證換算（Invariant 5）；這裡量到的是 disk 驅動的計時方式。
- 選項見 HANDOFF「等使用者決定」的 `v8-engine-audio-l2-premise`。

## 突變的完整一輪

- test 建置：`mutations.json` 全部 26／26 被殺（core 9、schedule 12、sdl 5）。
- TSan 建置（`TSAN_OPTIONS=ignore_interceptors_accesses=0`）：`mutations-tsan.json` 4／4 被殺。
- 執行後原始碼已還原（`git status` 的 engine 沒有變更）。

- 時間：估 2～4 h，到停止點約 1.3 h。

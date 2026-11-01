# Engine v8-engine-audio-windows-blocks：Windows 的輸出區塊與回呼間隔（只量測）

日期：2026-10-31。**完成**（只量測與記錄，沒有改程式）。計畫見 [audio 計畫](../architecture/plans/audio/README.md)，契約見 [audio.md](../architecture/audio.md)。

收支：一致（真實裝置兩輪：送出 299、準時 299、計數的丟棄 0、錨點丟棄 0）／disk 驅動第一輪不一致（送出 299、起音 299，但 297／299 落在重建的 frame；見「disk 驅動的觀察」）

## 為什麼做

- `v8-engine-audio-sdl` 只量了 macOS（[2026_10_30_engine_audio_sdl](2026_10_30_engine_audio_sdl.zh-Hant.md) 的表），Windows 未量。
- `MixerConfig::scheduleDelay` 的預設 60 ms 的根據（HANDOFF 的 `v8-engine-audio-delay-default`）：延遲至少要等於「事件到下一幀送出（30 FPS 最多 33.3 ms）」＋「到下一次回呼（回呼最長間隔）」。macOS 預設區塊 1024 樣本時是 33.3＋21.5＝54.8 ms。本批確認 Windows 上是否也在 60 ms 以內。

## 做了什麼

- master `d8cca12a`（含 PR #112、#113）。`cmake --preset test`（MinGW），建置 `gyo_audio_block_record`、`gyo_audio_onset_record`、`gyo_audio_sdl_tests`。
- L1：`ctest -R audio_sdl` 3／3 通過。
- `gyo_audio_block_record.exe 10 [frames]`：不指定、480、256，各 10 秒、各 2 次。
- `onset_record.py --seconds 30` 兩輪（第一輪 disk 的結果不如預期，再跑一輪看是否重現；量法不變）。

### 機器

- CPU：Intel Core i7-8750H（筆電）。OS：Windows 11 Pro 10.0.26300。
- 啟用中的播放端點（登錄的 MMDevices）：Realtek(R) Audio 的「スピーカー」（內建喇叭，共用模式格式 48000 Hz／16 位元／2 聲道）、Steam Streaming Speakers（48000 Hz）。耳機孔未插。預設裝置推定為 Realtek 內建喇叭（有線／內建），**未在 Windows 音效設定裡確認**。
- 沒有藍牙音效端點，藍牙未量。

## 結果

### 區塊（`gyo_audio_block_record`，10 秒，driver＝wasapi）

| 指定 | 次 | device_frames | 每次回呼 min／max | 最長間隔 | frame／秒 |
|---|---|---|---|---|---|
| 不指定（SDL 預設） | 1 | 480 | 480／480 | 10.348 ms | 48094.1 |
| 不指定（SDL 預設） | 2 | 480 | 480／480 | 10.373 ms | 48049.8 |
| 480 | 1 | 480 | 480／480 | 10.436 ms | 48049.9 |
| 480 | 2 | 480 | 480／480 | 10.586 ms | 48047.1 |
| 256 | 1 | 256 | 256／256 | 5.710 ms | 48050.5 |
| 256 | 2 | 256 | 256／256 | 5.945 ms | 48024.0 |

- Windows（WASAPI）的 SDL 預設就是 480（10 ms），不是 macOS 的 1024。指定 256 時回呼真的變成 256（約 5.3 ms），不像 macOS 停在約 10 ms。

### 起音紀錄（`onset_record.py`，30 秒，schedule_delay 60 ms）

| 項目 | 第一輪 | 第二輪 |
|---|---|---|
| device：driver／device_frames | wasapi／480 | wasapi／480 |
| device：準時／遲到 | 299／0 | 299／0 |
| device：callback_gap_max | 10.684 ms | 11.025 ms |
| disk：準時／遲到 | 297／2（最大遲到 78.3 ms） | 298／1（最大遲到 33.3 ms） |
| disk：callback_gap_max | 208.0 ms | 115.8 ms |
| disk：anchored match | **297／299**（未命中的最近距離 187、201 frame） | 299／299 |
| disk：Engine 時間下的間隔誤差（經錨點） | 報告沒有輸出這一行 | p99 0.019 ms、最大 33.295 ms |
| disk：奇數 Tick（經錨點） | 不適用 | 197／198 |
| disk：裝置速率 | −39630 ppm | −50890 ppm |

## 判斷

- 需要的延遲＝33.3 ms＋回呼最長間隔。SDL 預設（480）各次的最大值是 11.025 ms（起音紀錄第二輪的 device 行）→ **約 44.4 ms，在 60 ms 以內**（餘裕約 15.6 ms）。指定 256 時約 39.3 ms。
- 真實裝置兩輪都沒有遲到（played_late＝0）。
- 因此 Windows（這台、內建喇叭、WASAPI 共用模式）不需要調高預設，也不需要建議 480 區塊（預設已是 480）。待決的只有下面 disk 驅動的觀察。

### disk 驅動的觀察（未切分原因）

- 任務的預期是 disk 的 anchored match 全部命中。第一輪 297／299 未達；第二輪 299／299，但有一個間隔誤差 33.3 ms。
- 兩輪都有 disk 驅動的長停頓（回呼間隔 208 ms、116 ms），與 disk 的遲到同時出現；macOS 的紀錄（[2026_10_30_engine_audio_record](2026_10_30_engine_audio_record.zh-Hant.md)）沒有這種停頓。
- 原因未切分（AGENTS §14.1）：可能是環境（Windows 上 disk 驅動的 `SDL_Delay` 或寫檔被擋住，例如防毒掃描），也可能是分析器對遲到請求的錨點重建。`events.csv` 沒有逐筆的遲到標記，從現有紀錄分不出未命中的兩個是否就是遲到的兩個。只跑了 2 輪，沒有改量法。
- 影響：消費端的 L2（`sound_l2.py`）也以 disk 驅動與錨點重建判定，在 Windows 上可能偶發不成立。
- 使用者的決定（`v8-engine-audio-windows-disk`）：「A：照現狀記錄」。Windows 的 disk 結果不當判定，不加診斷、不改量法。

## 證據

- `build/target/_build/test/logs/audio-onset-record-20261031-windows/`（第一輪：report.txt、meta.txt、disk／device 的 anchors.csv、events.csv、rate.csv、summary.txt、disk/audio.raw）
- `build/target/_build/test/logs/audio-onset-record-20261031-windows-run2/`（第二輪）
- meta.txt：commit `d8cca12a`、`gyo_audio_onset_record.exe` sha256 `9c21e6c9…d412`。
- 區塊量測的輸出整行記在上表（沒有存檔）。
- 時間：約 30 分鐘。

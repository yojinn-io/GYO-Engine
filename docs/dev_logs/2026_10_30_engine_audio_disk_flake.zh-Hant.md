# Engine audio：`audio_sdl.disk` 在 macOS arm64 偶發失敗的修正

日期：2026-10-30。**完成**（本機 Red／Green、30 次重複；CI 等 PR）。計畫見 [audio 計畫](../architecture/plans/audio/README.md)。

## 為什麼做

- 工具線會話回報：PR #112 合併後，master 的 CI（run 41060166180，Quick acceptance／toolchain／macos-arm64）與 PR #111 的 L1／macos-arm64（run 41066070802）在 `SdlAudioOutputTests.cpp` 的 `CHECK(output.Stats().anchorsDropped == 0)` 失敗；其他平台與 master 的 L1 那一列通過，看起來是偶發。

## 原因（§14.1 的切分：Test 自身）

- disk 測試用 `SDL_AUDIO_DISK_TIMESCALE=0`，disk 驅動不等待，以最快速度回呼。本機（macOS Intel）量到每秒 12～17 萬次回呼（約每 ms 170 次）。
- 錨點記錄容量是 4096。測試的主執行緒只要停住約 24 ms（CI 機器的排程停頓），還沒取出就會填滿並丟棄；arm64 更快，更容易發生。
- 所以「不丟棄」這項檢查的結果取決於主執行緒有沒有被排程拖住，是測試條件的缺陷，不是混音器或後端的缺陷。

## 做了什麼

- §14.3 的順序：
  - 重現：本機直接重複 30 次 0 失敗（重現不了）。改從機制下手，在取出錨點前暫停 100 ms，模擬排程停頓：同一行、同一檢查失敗，`CHECK( 14825 == 0 )`（Red）。
  - 修正：測試環境改為 `SDL_AUDIO_DISK_TIMESCALE=0.1`（每塊 480 frame 之後等 1 ms），回呼頻率被限制在每秒約 1000 次以下，要 4 秒以上才會填滿 4096。測試只需要 48000 frame（約 0.1 s）。保留 100 ms 暫停：5 次通過（Green）。
  - 100 ms 暫停留在測試裡當回歸測試。`audio_sdl.*` 連跑 30 次 0 失敗。
- 判定（不丟棄、遲到的起音落在某個錨點的 frame、檔案裡恰好一個起音）不變，只改測試環境的驅動速度。

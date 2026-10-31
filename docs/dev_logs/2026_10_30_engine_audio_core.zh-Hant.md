# Engine v8-engine-audio-core：片段庫與樣本域混音

日期：2026-10-30。**完成**（本機 L1 與相關突變；四平台 CI 等 PR 線）。計畫見 [audio 計畫](../architecture/plans/audio/README.md)，契約見 [audio.md](../architecture/audio.md)。

## 為什麼做

- 消費端 v8 的音效最優先；本批是 `v8-engine-audio` 四批的第一批（high，估 3～4 h）。使用者原話：「開始吧」。

## 做了什麼

- 新模組 `engine/audio` → `GYO::Audio`（只連 `GYO::Base`、`GYO::Time`）：
  - `AudioFormat.hpp`：48 kHz、立體聲、增益上限 8。
  - `AudioError.hpp`：`InvalidClip`、`InvalidConfig`。
  - `ClipBank`：複製 PCM16，只收 1／2 聲道、48 kHz、整數個 frame；`ClipId` 從 1 開始。
  - `VoiceMixer`（模組內部，`engine/audio/src`）：固定數量的聲部（1～1024），以樣本位置排程；增益換成 Q16、int64 累加、主音量、四捨五入（半數進位）、飽和並計數；遲到的請求從下一個要輸出的 frame 開始並記錄遲到幾個 frame；計數 `accepted = onTime + late + noVoice`。
- 和 PLAN 的差異：PLAN 寫「此批的請求以測試用的直接注入進入」。實作時改成內部類別 `VoiceMixer` 由 L1 直接 include（`tests/common/audio/CMakeLists.txt` 加 `engine/audio/src` 的 PRIVATE include），公開 API 不留後門；公開的 `Mixer` 與 `MixerStats` 在 schedule 批包在它外面。累加從 PLAN 的 int32 改成 int64（增益上限 8、聲部上限 1024 時不溢位）。
- 守衛：`gyo_expect_audio_links`（configure 時檢查 `gyo_audio` 的連結恰好是 Base 與 Time）。
- `SoundLoader` 的 L1（`tests/common/core/asset/SoundLoaderTests.cpp`，第一次有測試）：單聲道、立體聲、奇數長度 chunk 的補位、非 RIFF、非 PCM、8 位元、6 聲道、沒有 data、空 data、data 長度超過檔案。
- 文件：`docs/architecture/audio.md`（契約、Design Invariants 9 條各標守衛與生效批次、Replacement Criteria）；`docs/architecture.md` 的 Runtime boundaries 加一列與一段說明；PLAN 加更正註記；HANDOFF、README。
- 突變目錄 `docs/architecture/plans/audio/scripts/mutations.json`（9 個），沿用 time-threads-trace 的 `run_mutations.py --catalog`。

## 驗證與經緯

- 本機（macOS Intel x86_64）core preset：configure、全部建置、`ctest --preset core` 35／35 通過（含新的 `gyo_audio_tests` 13 案 587 斷言，`engine_tests` 的 SoundLoader 2 案 26 斷言）。
- 突變：`run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/audio/scripts/mutations.json`，9／9 被殺（起點偏移、內部分段邊界漏 frame、聲部不釋放、遲到算成準時、捨入改截斷、飽和不計數、取樣率不檢查、`gyo_audio` 連 `GYO::Engine`（configure 失敗）、SoundLoader 不檢查位元數）。執行後原始碼已還原（每個 find 字串仍恰好出現一次）。
- 沒有跑 test preset（產品組態）與四平台：本批只加 Engine 模組與測試，產品沒有改；四平台 L1 等 PR 線的 CI。
- 時間：估 3～4 h，實際約 1.5 h。

## 留給下一批

- `VoiceMixer::Schedule` 有 `GYO_ASSERT`（不是 `noexcept`）。schedule 批從 `Mixer::Render`（`noexcept`）呼叫它之前，要讓生產者端（`Submitter::Play`）先驗證片段與增益，並決定 Render 內改呼叫不檢查的版本，或保留檢查（失敗即中止）。
- 公開介面草案的 `Submitter::Play` 寫成 `noexcept`；為了在生產者端 `GYO_ASSERT`，它不能是 `noexcept`（`Assert.hpp` 的規則）。

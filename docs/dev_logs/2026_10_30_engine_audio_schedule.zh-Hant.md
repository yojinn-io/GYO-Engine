# Engine v8-engine-audio-schedule：提交端與時間戳排程

日期：2026-10-30。**完成**（本機 L1、xhigh 審查與修正、相關突變；四平台 CI 等 PR 線）。計畫見 [audio 計畫](../architecture/plans/audio/README.md)，契約見 [audio.md](../architecture/audio.md)。

## 為什麼做

- `v8-engine-audio` 的第二批（high；換算、遲到、SPSC 局部 xhigh，估 4～5 h）。使用者原話：「開始 v8-engine-audio-schedule。」

## 做了什麼

- 公開的 `Engine::Audio::Mixer`（`Mixer.hpp`、`Mixer.cpp`）：
  - `MixerConfig`（聲部、提交端 1～64、佇列 1～65536、`scheduleDelay` 0～1 s；超出回 `InvalidConfig`）。
  - `Submitter`：有界 SPSC 環（tail／head 以 release／acquire 發布），第一個呼叫 `Play` 的執行緒擁有它；在生產者端以 `GYO_ASSERT` 驗證片段、增益、執行緒與移動後的物件；滿了丟棄並計數。所有環在建立混音器時配置；`CreateSubmitter` 以 CAS 領取（`TooManySubmitters`）；以 `shared_ptr` 共用狀態，可比 `Mixer` 活得久。
  - `Render(block, anchor)`：取出全部提交端的請求，起點＝區塊起點＋floor((at−anchor)×48000＋½)＋delay frame 數，偏移夾在 ±1 小時；早於區塊的從區塊開頭開始並計為遲到。
  - `MixerStats`：submitted、playedOnTime、playedLate、droppedQueueFull、droppedNoVoice、maxLateness、renderedFrames、clippedSamples。
- `VoiceMixer`：樣本位置改為有號（第一個區塊之前的時間也能判為遲到）；加 `ScheduleChecked`（`noexcept`，不檢查，給音效執行緒）；片段的 view 在建構時備好（`ClipBank::View` 會 assert）；奇數樣本數的最後一個樣本寫靜音。
- `AudioErrorCode::TooManySubmitters`；`IsValidGain` 移到 `AudioFormat.hpp` 共用。
- L1：`tests/common/audio/ScheduleTests.cpp` 19 案（合計 `gyo_audio_tests` 32 案、1348 斷言）。
- 突變：`mutations.json` 加 12 個（共 21 個）；新增 `mutations-tsan.json`（環的記憶體順序 4 個，只在 TSan 建置下有效）。

## 驗證與經緯

- 開始時的決定（記在 HANDOFF 現況）：`Submitter::Play` 不是 `noexcept`；`Render` 呼叫不檢查的版本；佇列預先配置、任何時候可建立提交端；有號樣本位置。
- **Design Invariant 5 的修訂（使用者同意兩次）**：
  1. 開始時發現原文「同錨點兩起點的差恰好＝round(Δ×48000)」在三個以上請求時做不到（各自四捨五入）。使用者選「改成誤差小於 1 樣本」（`v8-engine-audio-invariant5`）。
  2. xhigh 審查指出 ±1 小時夾限與負數的捨入方向沒有寫進新文。使用者選「加限定語並寫明捨入」（`v8-engine-audio-invariant5-clamp`）。
- **xhigh 審查**（`xhigh-reviewer`，引擎繼承）：功能上沒有確定的缺陷（SPSC、換算、`noexcept` 路徑、收支）；它另以 TSan、ASan＋UBSan 建置跑過原碼，乾淨。確定的問題都是註解或契約寫錯，已修：
  - `Stats` 的註解說在途時總和「可能少」，其實兩個方向都可能；改註解，讀取順序改為先讀結果再讀送出數。
  - `TooManySubmitters` 的條件：銷毀的提交端不歸還名額；寫進註解與契約。
  - ±1 小時夾限與捨入方向沒寫進契約：寫進 `Mixer.hpp` 與 audio.md（見上面第 2 點）。
  - `VoiceMixer` 註解說奇數樣本數「由呼叫端檢查」，其實沒人檢查：改註解。
  - 另加 `static_assert`：steady_clock 的 period 是 ns。
  - 測試缺口已補：−1.5 樣本、delay 的捨入、1 小時夾限、負錨點的溢位分支、每個提交端各有自己的佇列、Invariant 5 一案改為檢查振幅（同一 frame 的兩個請求）並拿掉自證的兩兩比較、併發測試加輸出總和的內容檢查。
  - 「可能」的項目：太遠未來的請求佔住聲部（使用者決定先記錄，`v8-engine-audio-far-ahead`）；owner 檢查只是盡力而為、單一區塊最壞工作量（寫進契約）。
- **收支**：一致。併發測試 4 生產者 × 5000：submitted＝四種結果之和，droppedNoVoice 0，飽和 0，輸出總和＝各生產者記下的期望總和。
- 本機（macOS Intel）：`ctest --preset core` 35／35；併發兩案重複 50 次 0 失敗；TSan 建置（`-fsanitize=thread`，`TSAN_OPTIONS=ignore_interceptors_accesses=0`）3 次 0 警告。
- 突變：一般建置 `mutations.json` 21／21 被殺；TSan 建置 `mutations-tsan.json` 4／4 被殺。審查者回報：記憶體順序的突變在一般建置與 macOS 預設 TSan 下全部存活（環的寫入被編成 memcpy，預設不檢查），所以另立 TSan 目錄；CI 沒有 TSan job，這部分只在本機守住。
- 時間：估 4～5 h，實際約 2.5 h（含 xhigh 審查約 18 分鐘）。

## 留給下一批

- sdl 批：每個回呼取一次 `Time::Now()` 傳給 `Render`；輸出必須先於 `Mixer` 停止（Render 中移動或銷毀 Mixer 是資料競爭）；量完 macOS／Windows 的區塊大小後提案 `scheduleDelay` 的預設。

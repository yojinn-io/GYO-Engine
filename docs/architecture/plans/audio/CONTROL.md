# 音效（GYO::Audio）：控制層重建計畫

更新：2026-10-31。**狀態：完成（2026-10-31）。** 9 項決策全部照建議核准（見 [HANDOFF](HANDOFF.md) 決策紀錄），6 批都已完成，[audio.md](../../audio.md) 的 Invariants 與 RC 都已成立。各批與計畫的差異記在各自的 dev_log 與 HANDOFF「現況」。

## 為什麼要做

- **要支持的 Feature**：資產工具（tool id `audio_editor`）的預覽。工具讀任意 WAV，轉成 48 kHz PCM16 後播放；使用者一邊調裁剪與增益一邊聽。需求由工具線轉來（`docs/tools/plans/v8/assets.md`，PR #117 的分支），2026-10-31 使用者決定由音效線做。工具需要三件事：
  - F1：停止已送出的聲音。至少能停掉自己送出的那一個，或停掉自己送出的全部。
  - F2：輸出執行中播放新轉出的片段。每次改設定都會重轉一次，但不重建輸出。
  - F3：讀出某個聲音目前播到片段的第幾個 frame，用來在波形上畫播放頭。只顯示，不拿來做判斷。
- **碰到的約束**（[audio.md](../../audio.md)）：
  - Replacement Criteria 1（輸出執行中載入片段）與 2（停止已送出的聲音）同時成立。
  - Design Invariant 7（輸出執行中片段不變）擋住 F2。
  - 2026-10-31 使用者在「修補現有 Mixer」與「重建控制層」之間選了**重建控制層**：保留樣本域（`VoiceMixer` 的混音）與時間換算，重新設計「命令怎麼進音效執行緒」與「片段怎麼活、怎麼回收」。
- **不在範圍內**：迴圈、音樂、淡出、改音量或音高、跳轉、暫停、結束通知、跨提交端的停止、串流與壓縮格式。這些都不是已點名的 Feature（AGENTS §1，不為預測的壓力一般化），只要求本設計不擋住它們。
- **PvP 的行為不變**：PvP 是目前唯一的消費端，只做一次性的、以時間戳排程的音效。它的程式不改；Invariants 3、5、9 的意思不變。

## 規劃的做法（ultracode）

- 使用者核准以 ultracode 規劃（2026-10-31）。workflow `v8-engine-audio-control-plan`（run `wf_38a2c8c8-6db`）的編組：
  - 3 個提案者，引擎與檔位都繼承（Opus 5.5），角度分別是：先參考成熟方案、最小改動、即時安全與決定性優先。
  - 1 個審查者（繼承），選定基底並綜合其他兩案。
  - 1 個對抗式檢查（`xhigh-reviewer`，xhigh，同引擎）。effort-gearbox 建議用另一個引擎；這次沒有換，原因是避免降低品質。這一點列為待決事項 `v8-engine-audio-control-gears`。
- 對抗式檢查找到 blocker 1、major 7、minor 12。本文已全部納入（見最後一節「對抗式檢查的處置」）。
- 原始輸出（英文的綜合計畫、中文的審查結果、workflow 的 journal）放在主 checkout（`/Users/karasu/Code/Source/GYO-Engine`，Intel Mac）的 `build/evidence/audio-control-20261031/`（git 不追蹤），本文是整理後的正本。

## 成熟方案的參考

API 名稱出自既有知識，寫進 audio.md 之前在實作批再核對一次。

| 來源 | 它的做法 | 採用 | 改裝或不採用 |
|---|---|---|---|
| Web Audio `AudioBufferSourceNode.start(when)`／`stop(when)` | 開始與停止用同一個時鐘；在開始之前（或同時）停止，等於從未發聲；停止時間已過，就立刻生效 | 停止的語意：停止前未開始＝靜音；遲到的停止落在取出它的區塊第一個 frame | 沒有 GC：片段槽由擁有它的生產者在下一次 `LoadClip` 回收 |
| miniaudio `ma_sound_set_stop_time_in_pcm_frames`、`ma_sound_get_cursor_in_pcm_frames`、resource manager | 以 PCM frame 指定停止點；其他執行緒可讀的 cursor；資料在工作執行緒釋放 | 停止點是絕對 frame，所以和區塊切法無關；播放頭＝已混入的片段 frame 數；音效執行緒從不釋放記憶體 | 不讓呼叫端持有聲部物件：GYO 的聲部由音效執行緒挑選 |
| FMOD `Channel::setDelay(start, end, stop)`、帶 generation 的 handle、`getPosition` | 以 DSP 時鐘精確到樣本的開始與結束；過期的 handle 無害 | 停止 frame 用混音器的時鐘，在樣本迴圈內套用；過期的 handle 絕不會碰到較新的聲音 | handle 不能帶聲部編號（聲部在之後才由音效執行緒選），所以改用每個提交端內永不重用的序號。不做聲部搶奪、不做虛擬聲部 |
| SoLoud（handle＝聲部編號＋播放序號） | 以 generation 驗證 handle | 以「唯一」保證 handle 有效 | 不用全域 mutex，也不做立即停止（會破壞 Invariant 8） |
| Wwise `PostEvent`／`StopPlayingID`／`GetSourcePlayPosition` | 送出端立刻拿到 id；位置給 UI 用 | `Play` 在生產者端就回傳 handle，不等音效執行緒；位置只供顯示 | 不做事件與 bank |
| Unity `PlayScheduled`／`SetScheduledEndTime` | 結束時間和開始時間用同一個時間基準排程 | 開始與停止共用一套換算 | — |
| Ross Bencina〈Real-time audio programming 101〉；Timur Doumler 的 ReleasePool | 音效執行緒上不配置、不釋放、不鎖、不等；垃圾交還給非即時執行緒 | 以 release store 的旗標交還；音效執行緒自己的使用計數只有它自己讀寫 | 不另開回收執行緒、不加回收環：由擁有者在 `LoadClip` 內回收 |

GYO 已經有的部分：frame 時鐘、錨點換算、固定大小的池、SPSC 環、有計數的丟棄。新增的部分：命令種類、handle、執行中的片段槽、公布的播放頭。

## 公開介面草案

形狀與責務定在這裡；名稱在實作批依當時的程式再核對。

```cpp
// ===== ClipBank.hpp =====
// Opaque to consumers. Bank clips keep today's numbering (generation field 0);
// runtime clips encode submitter, slot and a 32-bit generation (never 0).
struct ClipId final { std::uint64_t value = 0; friend constexpr bool operator==(ClipId, ClipId) = default; };
// ClipBank: Add, Count, Contains, View unchanged; validation moves to an
// internal ValidateClip shared with Submitter::LoadClip.

// ===== AudioError.hpp =====
// + NoClipSlot: every clip slot of this submitter is loaded or not yet handed back.

// ===== Mixer.hpp =====
struct MixerConfig final {
    std::uint32_t maxVoices = 32;
    std::uint32_t maxSubmitters = 4;
    std::uint32_t queueCapacity = 256;       // commands (play, stop, stop-all, unload) per submitter
    std::uint32_t clipSlotsPerSubmitter = 0; // 0 to 256; 0 = no runtime clips, nothing allocated
    Time::Duration scheduleDelay = 60ms;     // unchanged; maps plays and stops alike
};

// One play through one submitter, never reused for the mixer's life:
// (submitterIndex + 1) << 57 | sequence (>= 1, advanced only on a successful enqueue).
struct SoundHandle final {
    std::uint64_t value = 0; // 0 = not queued (the queue was full)
    [[nodiscard]] constexpr bool Valid() const noexcept { return value != 0; }
    constexpr explicit operator bool() const noexcept { return Valid(); }
    friend constexpr bool operator==(SoundHandle, SoundHandle) = default;
};

// Display only, never for decisions (Invariant 4).
struct Playhead final {
    enum class Status : std::uint8_t { NotPlaying, Scheduled, Playing }; // NotPlaying: not taken yet, ended, stopped or dropped
    Status status = Status::NotPlaying;
    std::uint32_t clipFrame = 0; // clip frames mixed as of the end of the last Render
};

struct MixerStats final {
    // plays, unchanged: submitted == playedOnTime + playedLate + droppedQueueFull + droppedNoVoice
    std::uint64_t submitted = 0, playedOnTime = 0, playedLate = 0, droppedQueueFull = 0, droppedNoVoice = 0;
    // stops (Stop and StopAll, one per request): stopsSubmitted == stopsMatched + stopsUnmatched + stopsDroppedQueueFull
    std::uint64_t stopsSubmitted = 0, stopsMatched = 0, stopsUnmatched = 0, stopsDroppedQueueFull = 0;
    std::uint64_t stopsLate = 0; // of stopsMatched: a request any of whose voices it stopped late
    // unloads: unloadsSubmitted == unloadsTaken + unloadsDroppedQueueFull
    std::uint64_t unloadsSubmitted = 0, unloadsTaken = 0, unloadsDroppedQueueFull = 0;
    Time::Duration maxLateness{}; std::uint64_t renderedFrames = 0, clippedSamples = 0; // unchanged
};

class Submitter final {
public:
    // Owner thread (the first to call any of these). Today's Programmer Errors, plus:
    // a runtime clip not loaded through this submitter, or already unloaded.
    SoundHandle Play(const PlayRequest& request);                  // was bool
    // Ends the sound at the frame `at` maps to (same mapping as Play, delay included).
    // An invalid handle does nothing and returns true; another submitter's handle,
    // or a sequence never issued, is a Programmer Error.
    // False: queue full (counted; the sound keeps playing; retry).
    [[nodiscard]] bool Stop(SoundHandle sound, Time::TimePoint at);
    // Same, for every sound this submitter sent before the call. One stop request.
    [[nodiscard]] bool StopAll(Time::TimePoint at);
    // Owner thread, also while output runs. Frees handed-back slots first, then
    // copies the samples on this thread. InvalidClip as ClipBank::Add; NoClipSlot.
    [[nodiscard]] Base::Result<ClipId, AudioError> LoadClip(Base::ConstSpan<std::int16_t> interleaved,
                                                            std::uint16_t channels, std::uint32_t sampleRate);
    // Queues the release behind every play already queued; sounds already sent
    // play to their end (stop them first to free sooner). Playing the clip
    // afterwards is a Programmer Error. False: queue full (counted; the clip
    // stays loaded and may be unloaded again).
    [[nodiscard]] bool UnloadClip(ClipId clip);
};

class Mixer final {
public:
    // Create (InvalidConfig also for clipSlotsPerSubmitter > 256), CreateSubmitter,
    // SetMasterGain, Stats, Render: unchanged signatures.
    // Any thread, wait-free, display only. An invalid handle or one whose
    // submitter index is outside this mixer gives NotPlaying. A handle from
    // another mixer is a Programmer Error it cannot always detect.
    [[nodiscard]] Playhead PlayheadOf(SoundHandle sound) const noexcept;
};
// engine/audio/backend/sdl/**: no change.
```

內部結構（`engine/audio/src`）：

- `Detail::Command`：`{kind: Play|Stop|StopAll|Unload; ClipId; TimePoint at; float gainL, gainR; uint64 handle}`，約 40 B，放在現有的每提交端 SPSC 環裡。
- `RuntimeSlot`（每提交端 `clipSlotsPerSubmitter` 個，建立時配置記錄、不配置樣本）：
  - `samples`、`channels`、`frames`、`generation`、`state`（Free／Loaded／Unloading）：只有擁有者生產者讀寫。
  - `std::atomic<bool> handedBack`：音效執行緒以 release 寫 true，擁有者以 acquire 讀。
  - `ClipLease{voices, unloadTaken}`：只有音效執行緒讀寫。
- `VoiceMixer` 只多兩個欄位：`stopFrame`（int64，預設最大值）和一個不透明的 `cookie`（uint64）。它在 `Render` 結束時把「本次結束的聲部的 cookie」寫進建立時配置、容量為 `maxVoices` 的陣列。片段租約、交還、tag、播放頭都由 `Mixer`（控制層）處理（對抗式檢查 M7：樣本域不擁有片段生命期）。
- `PlayheadBoard`：每個聲部 `{atomic<uint64> tag; atomic<uint64> state}`。寫端只經 `Assign`／`Publish`／`Clear` 三個方法，讀端只經 `Read`；兩端都以 Hook 範本參數（正式建置為空操作）讓測試在每個 store 與 load 之間插入步驟。

## 機制

**命令順序。**
- 每個提交端仍是一個 SPSC 環，記憶體順序照現狀：生產者以 release 公布 tail，音效執行緒以 acquire 讀，再以 release 公布 head。
- 停止與卸載和播放走同一個環，所以一定在它們指名的播放之後被取出。停止與卸載因此只限送出播放（或載入片段）的那個提交端。不同環之間沒有順序。

**每個區塊的 `Render`：**

1. 依索引順序，對每個已領取的環，依 FIFO 取出所有命令。
   - **Play**：照現狀換算 `blockStart + NsToFrames(ClampedOffset(at, anchor)) + delayFrames`，再呼叫 `ScheduleChecked`。
     - 執行中的片段從槽取得 `ClipView`。
     - 聲部拿到 `cookie = handle`，並在播放頭上 `Assign`。
     - 開始一個聲部時，`lease.voices++`；`noVoice` 丟棄時不加。
   - **Stop**：用同一個換算函式（獨立的呼叫點，讓突變能只打壞停止）。找到 cookie 相符的聲部後：
     - **已開始**（`startFrame < blockStart`）：只走遲到規則。`stopFrame < blockStart` 時夾到 `blockStart`，計 `stopsLate`；否則 `stopFrame = min(現值, stopFrame)`。
     - **未開始**：`stopFrame <= startFrame` 時立刻釋放聲部，不輸出任何樣本；否則同上取 min。
     - 找不到相符的聲部（已結束、已停止或被丟棄）時計 `stopsUnmatched`，找到時計 `stopsMatched`。
   - **StopAll**：對同一提交端的所有聲部套用同樣的規則。至少停了一個聲部就算 matched；任一聲部遲到就算 `stopsLate`。以請求計數，不以聲部計數。
   - **Unload**：`unloadsTaken++`，`lease.unloadTaken = true`。若 `voices == 0`，走下面的「交還」。
2. `VoiceMixer::Render` 混入 `[max(start, chunkStart), min(start + clipFrames, stopFrame, chunkEnd))` 的 frame。聲部在三種情況結束：到片段結尾、到 `stopFrame`、開始前被釋放。結束的聲部 cookie 寫進結束陣列。
3. `Mixer` 處理結束陣列：
   - 播放頭 `Clear`。
   - 若有租約，`voices--`；歸 0 且 `unloadTaken` 時走「交還」。
   - 對仍在播的聲部 `Publish` 位置。
4. **交還**：先把 `lease = {0, false}`，再 `handedBack.store(true, release)`。兩者都在同一個執行緒，release 把重設排在生產者重用之前（對抗式檢查 B1：不重設的話，重用的槽在下一代第一個聲部結束時就會被提早交還，造成音效執行緒讀到已釋放的記憶體）。

**Handle。**
- 格式是 `(submitterIndex + 1) << 57 | seq`。前綴 7 位，可表示 1～64 個提交端；`seq` 有 57 位。
- 對抗式檢查 M1：原本的 `<< 58` 在第 64 個提交端會溢位成 0，所以改成 7 位前綴。
- `seq` 由生產者私有，從 1 開始，只在入列成功時前進。57 位永不重用，所以沒有 ABA，也不需要 generation 表。
- `Stop` 在生產者端 assert「前綴是自己的、`seq` 不大於已發出的最大值」。

**執行中的片段（F2）。**

- **`LoadClip`**（擁有者執行緒）：
  1. 對自己每個 `handedBack.load(acquire)` 為 true 的槽：釋放 vector，清掉 `handedBack`，狀態改為 Free。
  2. 選一個 Free 槽，沒有就回 `NoClipSlot`。
  3. `ValidateClip`，配置、複製，狀態改為 Loaded，回傳編碼後的 id。
  - 它不經過環：樣本在之後任何 Play 入列之前就寫好，環的 release／acquire 讓音效執行緒看得到。
- **`UnloadClip`**：
  - 入列成功才把狀態改為 Unloading，並把 `generation` 加 1（跳過 0）。
  - 佇列滿時狀態不變，所以可以重試（對抗式檢查 m3）。
  - 之後再對舊 id 呼叫 Play，生產者端會 assert（generation 已不相符，對抗式檢查 m2）。
- **為什麼回收是安全的。**
  - 環是 FIFO，所以 Unload 被取出時，它之前的每個 Play 都已取出：每個 Play 不是佔住一個聲部（算在 `voices` 裡），就是被丟棄。
  - Unload 之後不會再有這個片段的 Play。
  - 所以 `voices` 歸 0 時，已經沒有讀者。
  - 音效執行緒在最後一次讀樣本之後才以 release 寫 `handedBack`，擁有者以 acquire 讀到它之後才釋放，因此讀一定發生在釋放之前。
- **提交端被銷毀時**：它的槽和環不會歸還（和現狀相同）。記憶體在後端停止輸出後，隨 `MixerState` 在最後持有者的執行緒上釋放。
- **沒有 `Render` 時**（輸出開不起來）：卸載的槽永遠不會交還，之後的 `LoadClip` 會回 `NoClipSlot`。寫進契約。

**播放頭（F3），兩端都 wait-free。**

- **寫端**（音效執行緒）：
  - `Assign(h, init)`：`tag.store(0, relaxed)`；`state.store(init, release)`；`tag.store(h, release)`。
  - `Publish(pos)`：每次 `Render` 後 `state.store(pos, release)`。
  - `Clear()`：`tag.store(0, release)`。
- **讀端** `PlayheadOf(h)`：
  1. 無效的 handle、或前綴超出本混音器的提交端數 → NotPlaying（對抗式檢查 M2）。
  2. 掃描每個聲部：`a = tag(acquire)`。若 `a == h`，讀 `s = state(acquire)`，再讀 `b = tag(acquire)`；若 `b == h`，依 `s` 回 Scheduled 或 Playing。
  3. 其他情況 → NotPlaying。
- **為什麼正確。** 讀到的 state 只有兩種可能：
  - h 的初始值或更新的值；
  - 前一個聲音的舊值。這時重讀的 tag 不可能仍是 h：tag 永不重用，而且 release 順序把 h 的 tag 排在 h 的 state 初始化之後。
- **刪掉的部分**：綜合計畫原本有 `takenSeq` 和「Queued」狀態，用來區分「還沒取出」與「已結束」。F3 只要顯示，兩者都顯示成沒有播放頭即可。依對抗式檢查 m10 刪掉，少一個排序論證和一個突變。
- **精度**：數字是「已混入的片段 frame」，最多晚一個區塊（SDL 上約 10～21 ms），而且比喇叭實際發出的聲音早一段輸出延遲。這些寫進契約。
- **不回流**：音效執行緒從不讀 board，所以播放頭不影響輸出。

**即時上限。**
- 每個區塊最壞是 `maxSubmitters × queueCapacity` 個命令，每個命令最多掃 `maxVoices` 個聲部，和 audio.md 現有的上限同一個數量級。
- 另外加上 `maxVoices` 次 board 寫入。
- `Render` 不配置、不釋放、不鎖、不等、不記錄、不拋出例外。

## Design Invariants 與 Replacement Criteria 的修改案（決策 `v8-engine-audio-control-invariants`）

每一項都需要使用者同意（AGENTS §3）。**不變**：1、2、5、9；RC3、RC4、RC5。

| 項 | 現文（要點） | 修改案（寫進 audio.md 時用英文） | 方向 |
|---|---|---|---|
| Inv 3 | 「Play requests carry … time points」 | 改為「Play and stop requests carry …」，並加一句：「A stop's timestamp is converted by the same rule as a start's (offset, rounding, clamp and `scheduleDelay`), from the anchor of the block that takes it.」守衛加「maps a stop like a play from the block's anchor」。 | 加嚴 |
| Inv 4 | 「Only the mixer owns the count of rendered frames; consumers … keep no copy of it to decide anything」 | 「Only the mixer owns the count of rendered frames and each sound's position in its clip. It publishes them (`Stats().renderedFrames`, `Mixer::PlayheadOf`) for display and acceptance only; consumers and the backend keep no copy of them and decide nothing from them (for example, playing the next sound when a playhead stops, or skipping a stop because a playhead reports NotPlaying); decisions go through commands, which the mixer resolves. A published position may be up to one block old, is ahead of the audible sound by the output latency, never belongs to another sound, and never changes the output.」守衛：review；「never reports another play's state for a reused voice」（交錯測試）；結構（音效執行緒不讀 board，以 grep 守住）。 | 加嚴（明定公布的範圍） |
| Inv 6 | 播放的四種結局之和＝送出數 | 播放不變。另加：「Every stop request (Stop or StopAll) ends in exactly one of matched, unmatched, dropped because the queue was full; stopsSubmitted equals their sum. Every unload request is taken or dropped because the queue was full; unloadsSubmitted equals their sum. A stop never changes how its play was counted.」守衛：「balances stops」「balances unloads」；並把「balances every request from concurrent producers」擴充到停止與卸載。 | 加嚴（播放的收支不變） |
| Inv 7 | 「clips do not change while output runs」 | 「A clip's samples never change while a voice or a queued play can read them: bank clips never change; a clip loaded through a submitter is allocated only on that submitter's owner thread and freed there, or with the mixer's state after output stops; the audio thread hands a slot back, with a release store and after resetting its own lease, only once it has taken the clip's unload and no voice reads it.」守衛：`noexcept` 簽章；「the audio thread allocates and frees nothing」（計數配置器）；「does not hand back a clip while a voice reads it」；「a reused slot is not handed back early」；ASan／TSan 壓力測試；鎖靠 review。 | **放寬一面**：輸出執行中可以載入。原文保護的性質（音效執行緒讀到的樣本不變、不被釋放）由上面的生命期規則與守衛接手。 |
| Inv 8 | 「same clip bank, request sequence and anchor sequence … however the output is split into blocks」 | 拆成兩句（對抗式檢查 M4）：(a)「Given each voice's start and stop frames, the output is bit-identical however it is split into blocks.」(b)「The same clips, the same commands taken in the same blocks, with the same block lengths and anchors, give a bit-identical output.」守衛：現有的「output does not depend on how blocks split it」擴充到停止（前提寫明：所有命令在第一次 `Render` 前送入且都準時）；PvP 形狀的特性測試。 | 換文字；(a) 的範圍比原文明確。依 AGENTS §14.4 寫出 1～5（下節） |
| 新 Inv 10 | — | 「**A stop is a frame, ordered and scoped.** A voice stopped before it has started, at or before its start frame, is silent. A voice stopped at a later frame contributes exactly the frames in [start, min(start + clip frames, stop)). A voice that has started and is stopped at a frame already rendered stops at the first frame of the block that takes the stop, counted late. A stop affects only sounds sent earlier through the same submitter; a submitter cannot stop another's sounds.」守衛：「stops a voice exactly at its stop frame whatever the block split」「a stop before the start silences the sound and frees its voice」「a late stop of a started voice applies at the block's first frame」「StopAll stops only its own submitter's sounds」。 | 新增 |
| 新 Inv 11 | — | 「**Handles never alias.** A `SoundHandle` is never reused for the mixer's life; a stop or playhead query never affects or reports another play. A runtime `ClipId` names one slot in one generation; an unloaded id never plays the slot's next clip.」守衛：「a stale handle never stops a newer sound in the same voice」「a reloaded slot does not answer to the old ClipId」「the last of 64 submitters gets valid handles」。 | 新增 |
| RC1 | 「Streaming, compressed formats, or loading or unloading clips while output runs …」 | 「Streaming or compressed formats are needed; or clips must be loaded, unloaded or played from a thread or submitter other than the one that owns them; or clip memory must be reclaimed without the audio thread running, or bounded by bytes.」 | 已滿足的條件移出，留下新設計的前提 |
| RC2 | 「… or stopping or changing a sound after it was sent (loops, music)」 | 「Signal processing beyond per-voice gains and a hard stop (buses, effects, HRTF, fades), or changing a sound after it was sent other than stopping it (gain or pitch, seek, pause, loops, music), is needed.」 | 同上 |
| RC6 | 「Each new playback feature requires re-verifying …」 | 加上「including each new command kind」以及「the ring ordering, clip reclamation」。 | 加嚴 |
| 新 RC7 | — | 「A consumer needs the playback position exactly (sample-accurate or audible, corrected for device latency), needs it for a decision, or needs to be notified when a sound ends.」 | 新增 |
| 新 RC8 | — | 「Stops must be immediate (unmapped) or reach across submitters, or a mixer-wide stop is needed.」 | 新增 |

### Invariant 6 與 8 的守衛擴充（AGENTS §14.4 的 1～5）

**Invariant 8 的切法守衛**（「output does not depend on how blocks split it」）

1. 守的性質：給定每個聲部的起止 frame，輸出和區塊切法無關。
2. 緩的面：原文的前提是「同樣的請求序列」，請求可以不準時。新文寫明前提「所有命令在第一次 `Render` 前送入且都準時」。遲到的請求取決於取出它的區塊，本來就不屬於這個性質。所以這不放寬原守衛實際測的範圍：現有測試的請求都在第一次 `Render` 前送入。
3. 締的面：同一組切法（1～2200）加上停止。停止點錯一個 frame、或隨切法改變，都會失敗。
4. 遲到行為仍由「starts a late play at the next frame」與新的「a late stop … applies at the block's first frame」守住。
5. 根據：本節與 `tests/common/audio/MixerTests.cpp` 現有的切法測試。實作批以突變檢查示範新判定的檢出力。

**Invariant 6 的併發收支守衛**（「balances every request from concurrent producers」）

1. 守的性質：沒有任何請求漏計。
2. 緩的面：無。播放的等式不變。
3. 締的面：同一次併發執行中，停止與卸載也要各自收支平衡。
4. 不適用（沒有緩的面）。
5. 根據：本節；實作批的突變 `au-ctl-stop-unmatched-uncounted`、`au-ctl-unload-uncounted`。

## Feature 對照

- **F1**：`Stop(h, Time::Now())` 與 `StopAll(Time::Now())`。延遲是 `scheduleDelay` 加上裝置的緩衝。工具可以用較小的 delay 建立自己的混音器（例如 10～20 ms）；這時它以 `Now()` 送出的播放可能被計為遲到，這是預期的。工具只有一個提交端，所以 `StopAll` 停的就是它播放的全部。
- **F2**：每次改設定時依序呼叫：
  1. `Stop(h1)`
  2. `UnloadClip(id1)`
  3. `id2 = LoadClip(pcm)`
  4. `h2 = Play({id2, now})`

  輸出不重建。回 `NoClipSlot` 時下一幀重試。轉換與工程檔的知識留在工具；Engine 只收 PCM16（Engine 不承擔工具職能）。槽數的建議值是 8，不是綜合計畫原本寫的 4：槽要等停止生效、再等下一次 `LoadClip` 才能回收，60 FPS 拖動滑桿時約有 2～3 個槽在途（對抗式檢查 m11）。live 批會實測。
- **F3**：UI 執行緒呼叫 `OwnedMixer().PlayheadOf(h)`，得到 Scheduled、Playing（`clipFrame`）或 NotPlaying。只供顯示。
- **PvP**：
  - 原始碼不改。`PvpSound.cpp:95` 丟棄 `Play` 的回傳值；它的 `ClipId` 是不透明的 bank id，不受寬度影響。
  - `clipSlotsPerSubmitter` 維持 0，不配置任何新東西。
  - 測試只有一處機械修改：`tests/common/audio_sdl/OnsetRecord.cpp:102` 的 `const bool accepted = …` 改成 `.Valid()`。這不改任何判定。doctest 的 `REQUIRE`／`CHECK_FALSE` 以 `static_cast<bool>` 判定，`explicit operator bool` 可以直接用。

## 分批

```text
v8-engine-audio-control                 「重建控制層：停止、執行中換片段、播放頭」
├── v8-engine-audio-control-plan        「high｜繼承｜本計畫；核准後把 Invariants／RC 寫進 audio.md」
├── v8-engine-audio-control-baseline    「high｜繼承｜先在 master 上固定 PvP 形狀的輸出與『不配置』守衛」
├── v8-engine-audio-control-stop        「high，換算與環順序局部 xhigh｜繼承｜命令環、SoundHandle、Stop／StopAll」
├── v8-engine-audio-control-clips       「high，回收順序局部 xhigh｜繼承｜執行中的片段槽、載入、卸載、交還」
├── v8-engine-audio-control-playhead    「xhigh｜繼承＋另一引擎的對抗式檢查｜播放頭與交錯測試」
└── v8-engine-audio-control-live        「medium，測試程式 high｜繼承｜SDL 的 dummy／disk 測試、完整突變、收尾」
```

| 批次 | 檔位｜引擎 | 估時 | 範圍 | 測試（先 Red） | 突變 | 驗收 |
|---|---|---|---|---|---|---|
| `-plan` | high｜繼承 | 2～3 h（本文約 1.5 h；核准後寫 audio.md 約 1 h） | 本文；核准後：audio.md 的 Invariants、RC 與 Status（Contracts 與即時上限描述現有程式，隨各實作批更新）、PLAN.md 的「做」與「不做」加註、README、HANDOFF。`docs/architecture.md` 的 Audio 列與段落（:74、:294）描述現有程式，移到 `-live` 批 | 無 | 無 | 使用者核准 Invariants 與 RC 的文字之後才寫程式；dev_log |
| `-baseline` | high｜繼承 | 1～1.5 h | 新增 `tests/common/audio/CharacterizationTests.cpp`、`RenderAllocGuard.cpp`。先確認 `gyo_audio_tests` 是獨立的執行檔，因為替換全域 `operator new` 會影響整個執行檔 | 「PvP-shaped sequence renders the recorded output」：對多個片段、跨錨點的播放、遲到、佇列滿、聲部滿的輸出取 FNV-1a 雜湊；在 master 上就是 Green（特性測試，不是 Red）。「Render allocates and frees nothing」 | `au-ctl-golden-gain-rounding`、`au-ctl-alloc-in-render` | 兩個突變都被殺。之後任何平台雜湊不同就停下回報（§14），不重錄 |
| `-stop` | high，局部 xhigh（`xhigh-reviewer`）｜繼承 | 4～5 h | `Mixer.*`、`VoiceMixer.*`（只加 `stopFrame`、cookie、結束陣列）、新增 `StopTests.cpp`、`OnsetRecord.cpp:102`、`mutations.json`（find 字串被改寫的既有突變同批更新並重跑） | 停在換算出的 frame；未開始前停止＝靜音並釋放聲部（`maxVoices=1`）；已開始的聲部遲到停止＝夾到區塊開頭，前段樣本仍在；跨區塊與播放同樣換算；含停止的切法無關（1～2200）；StopAll 只停自己的提交端；已結束或被丟棄的播放算 unmatched；佇列滿的停止有計數；handle 唯一、佇列滿時為 0；`Stop(SoundHandle{})` 無作用；別的提交端的 handle 是 Programmer Error；第 64 個提交端的 handle 有效；過期 handle 不會停到重用聲部上的新聲音；併發收支含停止（TSan）；雜湊不變 | `au-ctl-stop-off-by-one`、`-stop-unmapped`、`-stop-skips-delay`、`-stop-ignores-tag`、`-stopall-unscoped`、`-stop-before-start-ignored`、`-stop-branch-swapped`、`-late-stop-unclamped`、`-stop-voice-not-freed`、`-stop-unmatched-uncounted`、`-handle-reused`、`-handle-prefix-overflow` | core 與 test preset 全綠；相關突變以預期訊息被殺；xhigh 審查的問題已修；dev_log |
| `-clips` | high，局部 xhigh（`xhigh-reviewer`）｜繼承 | 4～6 h | `ClipBank.*`（64 位 `ClipId`、`ValidateClip`）、`AudioError.hpp`（`NoClipSlot`）、`Mixer.*`（槽、租約、交還）、新增 `ClipSlotTests.cpp`、`mutations.json`、`mutations-tsan.json` | 播放建立後載入的片段；`LoadClip` 拒絕 `ClipBank` 拒絕的；槽用完回 `NoClipSlot`；聲部還在讀時不交還（播一半回 `NoClipSlot`、播完成功）；已入列的播放在卸載後仍完整播出；停止的片段在停止 frame 交還；slots=1 時下一代播完一個聲部後仍不交還（B1）；卸載佇列滿可以重試；卸載後或別的提交端的片段是 Programmer Error；重載的槽不回應舊 `ClipId`；設定範圍檢查；不配置守衛涵蓋載入、播放、停止、卸載與結束路徑；TSan 與 ASan 壓力測試（每一代放不同的常數，輸出只能出現有效的「代 × 增益」；宣告 200 次 × 3 輪，只做結構斷言、不設時間上限） | `au-ctl-clip-handback-ignores-voices`、`-clip-lease-not-decremented`（自然結束、停止、開始前釋放，各一）、`-clip-lease-counts-novoice`、`-clip-unload-flag-not-reset`、`-clip-reuse-without-check`、`-clip-generation-not-bumped`、`-unload-uncounted`、`-unload-marks-on-full`；TSan：`au-ctl-clip-handback-relaxed` | 同 `-stop`，再加 TSan、ASan 的結果；回收的 happens-before 論證寫進 audio.md Contracts |
| `-playhead` | xhigh｜繼承＋另一引擎的對抗式檢查（見決策 `-gears`） | 3～4 h | 新增 `PlayheadBoard.hpp`（Hook 範本參數；寫端 `Assign`／`Publish`／`Clear`，讀端 `Read`）、`Mixer.*`、新增 `PlayheadTests.cpp` | 依序 Scheduled、Playing、NotPlaying；位置單調且不超過片段長度；直接驅動 board 的單執行緒窮舉交錯（寫端第 i 個 store × 讀端第 j 個 load）；無效或越界的 handle 回 NotPlaying；有輪詢執行緒時輸出位元相同；TSan 輪詢壓力（宣告 5 輪 × 10^5 次；v8-arch-audio-playhead-test-fix 改為：宣告 5 次執行，每次 3 種排程，各要在上限內達到 10^5 次與渲染重疊的輪詢與 10^3 次讀到 Playing，上限 20 s、TSan 建置 60 s；守競爭與單次讀取，同一音的讀取順序改由單執行緒逐區塊的重用檢查守） | `au-ctl-playhead-tag-before-state`、`-playhead-single-tag-read`、`-playhead-tag-not-cleared`、`-playhead-no-bounds-check`。`au-ctl-board-state-relaxed` 不列入「必須被殺」：board 全是 atomic，TSan 在結構上看不到，x86 的程式碼也相同。改以論證加 review，依 §14.2 照實記錄 | 突變被殺；xhigh 審查與對抗式檢查都有紀錄；殺不掉的照實記錄 |
| `-live` | medium，測試程式 high｜繼承 | 2～3 h | `tests/common/audio_sdl/SdlAudioOutputTests.cpp`、`mutations.json`（`au-sdl-*`）、README 與 HANDOFF 收尾、dev_log | dummy 驅動：載入、播放、等到 Playing、停止、等到 `stopsMatched`、卸載、等到 `LoadClip` 成功（只靠 CTest 的逾時，不設時間上限）。disk 驅動：停止的聲音只在停止 frame 之前非零，停止 frame 以 `TakeAnchors` 重算，接受兩個候選錨點（同 record 批的做法）。disk 驅動：Open 之後載入的片段聽得到。既有 6 個 SDL 測試不變。新測試先 Red | 新增一個只會被 `audio_sdl.disk` 殺掉的突變（例如 `au-ctl-stop-frame-ignored-in-render`）；控制層突變全部重跑；PR 線合併前背景完整跑一次（D184） | 報告以「収支：」行開頭，涵蓋播放、停止、卸載；CI 全綠；雜湊不變；PvP 建置與測試全綠；Architecture Fitness：`GYO::Audio` 只連 Base 與 Time（`gyo_expect_audio_links`），G1～G3 不變 |

- 合計 17～23.5 h。任何一批超過估時 1.5 倍，或同一個問題累計超過 2 h，就停下回報（AGENTS §14）。
- 每個程式批的事前宣告都要寫「範圍的限制」：CI 沒有 TSan job（HANDOFF「未結事項」）。release／relaxed 類突變只能用本機 TSan（`ignore_interceptors_accesses=0`）檢查。
- 突變執行器要求 find 字串在檔案中恰好出現一次（`run_mutations.py:110`）。所以停止的換算要有獨立的呼叫點字串；`au-sched-full-off-by-one`、`au-sched-block-start-ignored` 這類既有突變若被改寫，同批更新。

## Architecture Delta（AGENTS §3）

1. **支持的 Feature**：資產工具 `audio_editor` 的預覽（F1～F3）；使用者選擇由音效線重建控制層。
2. **現在的問題**：送出的聲音停不掉；片段只能在建立混音器前登錄（Invariant 7、RC1、RC2）。在工具裡每改一次就重建混音器與輸出，會重設 `renderedFrames`、切斷錨點紀錄、讓所有聲音中斷，並把交接的責任放進後端。
3. **變化的邊界**：`GYO::Audio` 的公開介面：
   - `Play` 回傳 `SoundHandle`；
   - 新增 `Stop`、`StopAll`、`LoadClip`、`UnloadClip`、`PlayheadOf`；
   - `ClipId` 改為 64 位；
   - 新增錯誤碼 `NoClipSlot`；
   - `MixerStats` 新增欄位。
4. **影響**：
   - `engine/audio`、`tests/common/audio`、`tests/common/audio_sdl`（一行機械修改）、audio.md、`docs/architecture.md`。
   - PvP 的原始碼不改。後端不改。
5. **依賴方向**：不變（Audio → Time → Base；後端 → Audio、SDL3 PRIVATE）。沒有新的邊。
6. **Ownership**：
   - 「每個聲音在片段中的位置」的權威屬於混音器（Invariant 4 擴充）。
   - 執行中片段的記憶體屬於載入它的提交端的擁有者執行緒；音效執行緒只持有使用計數。
   - `VoiceMixer` 的責務不擴大：它只多了停止點與一個不透明的 cookie。
7. **為什麼不能更小**：
   - 只加停止、不處理片段：F2 做不到。
   - 讓整個混音器熱抽換：第 2 點列的問題全部會發生，而且把交接放進後端（觸及 RC3）。
   - 修補的做法（在現有佇列旁邊另開停止與換庫路徑）：跨環的順序要另外證明，每加一個功能又要重新驗證（RC6）。使用者已選擇重建。

## 風險

1. **租約路徑**（最大的風險）：任何一條聲部結束路徑漏掉 `voices--`，槽就永遠不會交還；順序錯就是釋放後使用。對策：每條路徑一個突變、B1 的回歸測試、ASan 與 TSan 壓力測試、xhigh 審查。
2. **播放頭的順序是語意上的**：TSan 看不到，只能靠交錯測試（需要在正式程式裡留 Hook，決策 `-playhead-hook`）。弱記憶體模型下的論證靠 review。
3. **共用的環**：一陣大量播放可能讓停止或卸載被丟棄。有計數、也有 `[[nodiscard]]`，但工具若忽略回傳值，就會「停不掉」。工具線的測試要涵蓋重試。
4. **停止的延遲等於 `scheduleDelay`**：預設 60 ms 的話工具會覺得慢，除非它調低自己的 delay。調低後它的遲到計數是預期的，不是缺陷。
5. **硬切會有爆音**：淡出屬於 RC2，由使用者決定（`-stop-click`）。
6. **公開介面變更**：這是 Architecture Delta（見上節）。Engine 以外的消費端只有 PvP（以 grep 確認）。
7. **記憶體只以槽數限制**：片段大小由工具控制。

## 對抗式檢查的處置

| 代號 | 問題 | 處置（本文的位置） |
|---|---|---|
| B1 | `unloadTaken` 從不重設，重用的槽會被提早交還 | 交還前先重設租約（機制「交還」）；回歸測試與突變 `-clip-unload-flag-not-reset` |
| M1 | handle 前綴在第 64 個提交端溢位 | 改成 7 位前綴（Handle）；測試與突變 `-handle-prefix-overflow` |
| M2 | 無效或越界的 handle 讓 `PlayheadOf` 越界讀取 | 回 NotPlaying；來自別的混音器的 handle 寫成偵測不到的 Programmer Error（介面）；測試與突變 `-playhead-no-bounds-check` |
| M3 | 停止的分類順序錯 | 先判斷是否已開始（機制 Stop、Inv 10）；測試與突變 `-stop-branch-swapped` |
| M4 | Inv 8 的新文字自相矛盾 | 拆成 (a)、(b)，前提寫明（Inv 表、§14.4 的 1～5） |
| M5 | 交錯測試殺不掉寫端順序的突變 | 寫端集中在 board 的三個方法，兩端都有 Hook，直接驅動 board；`board-state-relaxed` 移出必殺清單 |
| M6 | 卸載沒有收支 | `unloadsSubmitted`、`unloadsTaken`、`unloadsDroppedQueueFull`（介面、Inv 6） |
| M7 | 租約與 board 寫進 `VoiceMixer`，隱藏的 Ownership 變更 | `VoiceMixer` 只加停止點、cookie、結束陣列；其餘在 `Mixer`（內部結構、Delta 第 6 點） |
| m1 | StopAll 的子集計數 | 以請求計數（機制 StopAll） |
| m2 | 槽的狀態機沒有定義、generation 會撞 0 | 狀態 Free／Loaded／Unloading；`UnloadClip` 時 generation 加 1，跳過 0 |
| m3 | 卸載佇列滿時不可標記 | 入列成功才標記（`UnloadClip`）；測試與突變 `-unload-marks-on-full` |
| m4 | `Stop(SoundHandle{0})` | 不做事、回 true（介面） |
| m5 | Inv 7 的釋放執行緒寫得不完整 | 加上「or with the mixer's state after output stops」 |
| m6 | `MixerStats` 少了 `= 0` | 介面保留預設初始化 |
| m7 | 突變的 find 字串要唯一 | 停止有獨立的呼叫點（分批的附註） |
| m8 | 文件範圍不全 | `-plan` 的範圍列出 PLAN.md 各行與 `docs/architecture.md` |
| m9 | Inv 6 守衛擴充的 §14.4 | 已寫出 1～5 |
| m10 | 超出 F1～F3 的部分 | 刪掉 `takenSeq`、Queued、`stopsBeforeStart`；保留 `stopsLate`（和 `playedLate` 對稱） |
| m11 | 「4 個槽夠用」沒有根據 | 工具建議 8 個；live 批實測 |
| m12 | live 批的 SDL 測試沒有 Red 也沒有突變 | 新測試先 Red，加只會被 disk 測試殺掉的突變，接受兩個候選錨點 |
| 測試缺口 6 | noVoice 路徑沒有突變 | `-clip-lease-counts-novoice` |
| 測試缺口 7 | 「播放頭不影響輸出」沒有突變能證明 | 以結構守住（音效執行緒不讀 board，grep）；位元比較測試照留，但檢出力不宣稱，依 §14.2 照實記錄 |

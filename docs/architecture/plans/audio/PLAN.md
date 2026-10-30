# 音效（GYO::Audio）：批次計畫

更新：2026-10-30。**狀態：已核准（2026-10-30）。** 標「待決定」的地方對應 [HANDOFF](HANDOFF.md) 的代號；6 項全部照建議核准（HANDOFF「決策紀錄」），本文依建議寫成，內容不變。

## 範圍

### 做

- **中立混音器**（`GYO::Audio`）：輸出格式固定為 PCM16、48 kHz、立體聲。混音器自己數已輸出的樣本（frame）數，這個計數是「播放進行到哪裡」的唯一權威。
- **片段庫**：輸出開始前登錄的不可變 PCM16 片段（單聲道或立體聲、48 kHz）。登錄時複製資料；輸出期間不增減（`v8-engine-audio-clip-format`，2026-10-30 核准）。
- **一次性的播放請求**：片段、Engine 時間戳、左右兩個增益。請求送出後不能停止或修改。
- **以 Engine 時間戳排程**：每個輸出區塊開始時，後端給一個錨點（這個區塊的 Engine 時間與已輸出的樣本數）；混音器把請求的時間戳換算成樣本位置，在區塊內精確到樣本開始播放。
- **提交端**：每個提交端是有界的 SPSC 佇列，綁定一個生產者執行緒；混音器在音效執行緒上取出。佇列滿時丟棄並計數（`v8-engine-audio-producers`，2026-10-30 核准）。
- **收支**：每個請求恰好落入「準時播放、遲到播放、佇列滿丟棄、聲部滿丟棄」之一；計數可隨時讀出，四者之和等於送出的數。
- **主音量**：一個可從任何執行緒設定的全域增益（atomic）。
- **SDL 後端**（`GYO::AudioBackendSDL`）：以 SDL 3.4 的音效串流（`SDL_OpenAudioDeviceStream`，預設播放裝置）加回呼輸出；以參考計數初始化與結束 AUDIO 子系統；裝置格式的轉換交給 SDL。開不了裝置時回傳 `Result` 錯誤，消費端可選擇無聲繼續。

### 不做（v8）

- 解碼壓縮格式、串流播放、執行中載入片段、重新取樣（片段一律 48 kHz，由資產工具或腳本產生）。
- 迴圈播放、停止或淡出已送出的聲音、音樂、匯流排、效果器、3D 定位（HRTF、距離衰減）。距離與方向換成左右增益是消費端的事。
- 錄音、裝置列舉與選擇 UI、第二個非 SDL 後端。
- 把 `SoundLoader` 搬家或改寫。`SoundLoader` 留在 `GYO::Engine`；消費端把 `SoundAsset` 交給片段庫（見「與 SoundLoader 的關係」）。

## Owner 與依賴方向

| Target | 目錄 | 依賴 | 說明 |
|---|---|---|---|
| `GYO::Audio` | `engine/audio` | Base、Time | 片段庫、混音器、提交端、統計。不含 SDL、不含資產與產品知識 |
| `GYO::AudioBackendSDL` | `engine/audio/backend/sdl` | Audio；SDL3 PRIVATE | 中立標頭不含 SDL；不需要 native 標頭，也不提供 `*Native` target（沒有消費端需要 SDL 的音效型別） |

- 依賴方向：消費端 → Audio（＋後端）→ Time → Base。Engine 不依賴任何消費端；`GYO::Audio` 不依賴 `GYO::Engine`、`GYO::PlatformSDL`。
- 後端不連結 `GYO::PlatformSDLNative`：音效不需要視窗，只需要 SDL 本身；直接 `SDL3::SDL3` PRIVATE。G3（`tests/common/platform_sdl/CMakeLists.txt` 的 `gyo_expect_private_sdl`）把新後端加進檢查清單；G1 的衛生翻譯單元與 G2 的掃描也涵蓋它的中立標頭（只加嚴，不放寬）。
- 音效執行緒是 SDL 的裝置執行緒，不是 `GYO::Threads` 的角色執行緒；Engine 不另開執行緒。
- 伺服器端程序（沒有聲音）不連結 Audio；這個檢查屬於消費端的批次。

### Architecture Delta（AGENTS §3）

1. 支持的 Feature：消費端的音效（射擊、命中確認、受擊、換彈、遠端射擊、腳步）。
2. 現在的問題：Engine 沒有輸出聲音的模組；在產品裡直接呼叫 SDL 音效會違反 SDL 隔離（platform-sdl.md Invariant 1、2），排程與即時安全也會變成產品知識。
3. 變化的邊界：`engine/` 下新增子系統 `audio`（不是 Top-level Directory）；新增兩個 target。
4. 影響：Engine（新目錄、`engine/CMakeLists.txt` 加子目錄）、`tests/common`（新測試、G1～G3 的清單）、`docs/architecture.md` 的 Runtime boundaries 表加兩列。既有模組的責務不變。
5. 依賴方向：只新增 Audio → Time、Base 與 AudioBackendSDL → Audio、SDL3 的邊；沒有逆向邊與循環。
6. Ownership：「播放位置」的權威是混音器；SDL AUDIO 子系統的初始化由後端擁有（與 `SdlPlatform` 擁有 VIDEO 對稱）。
7. 為什麼不能更小：把混音器放進產品，等於把即時安全與時間換算當成產品知識，下一個需要聲音的產品要重做一次；把它放進 `GYO::PlatformSDL`，則混音器的離線測試要連 SDL，且 platform 的責務擴大。

## 公開介面草案

名稱與型別在 `v8-engine-audio-core` 開始時依當時的程式再核對；這裡定的是形狀與責務。

```cpp
namespace Engine::Audio {

    inline constexpr std::uint32_t kSampleRate = 48000; // frames per second
    inline constexpr std::uint16_t kChannels = 2;        // interleaved stereo output

    enum class AudioErrorCode { InvalidClip, InvalidConfig, TooManySubmitters, DeviceUnavailable, AlreadyOpen };
    using AudioError = Base::Error<AudioErrorCode>; // shape follows error-handling.md

    struct ClipId { std::uint32_t value = 0; };

    // Immutable PCM16 clips registered before output starts. The bank copies the samples.
    class ClipBank final {
    public:
        // Rejects an empty clip, channels other than 1 or 2, and sampleRate != kSampleRate.
        Base::Result<ClipId, AudioError> Add(Base::ConstSpan<std::int16_t> interleaved,
                                             std::uint16_t channels, std::uint32_t sampleRate);
    };

    struct MixerConfig {
        std::uint32_t maxVoices = 32;        // playing plus scheduled voices
        std::uint32_t maxSubmitters = 4;     // fixed at creation
        std::uint32_t queueCapacity = 256;   // per submitter
        Time::Duration scheduleDelay{};      // constant added to every mapped start; default decided in v8-engine-audio-delay
    };

    struct PlayRequest {
        ClipId clip;
        Time::TimePoint at;      // Engine time the sound belongs to
        float gainLeft = 1.0f;
        float gainRight = 1.0f;  // the consumer computes pan and distance into these
    };

    // Producer end of one bounded SPSC queue. Bound to the first thread that calls Play (GYO_ASSERT afterwards).
    class Submitter final {
    public:
        bool Play(const PlayRequest& request) noexcept; // false: queue full, counted as dropped
    };

    struct MixerStats {
        std::uint64_t submitted, playedOnTime, playedLate, droppedQueueFull, droppedNoVoice;
        Time::Duration maxLateness;          // largest late start, for diagnostics
        std::uint64_t renderedFrames;
        std::uint64_t clippedSamples;        // samples saturated to int16
    };

    class Mixer final {
    public:
        static Base::Result<Mixer, AudioError> Create(const MixerConfig& config, ClipBank&& clips);
        Base::Result<Submitter, AudioError> CreateSubmitter(); // only before an output is opened
        void SetMasterGain(float gain) noexcept;               // any thread
        MixerStats Stats() const noexcept;                     // any thread; counters are atomics

        // Called by an output backend on its audio thread, once per block.
        // anchor: the Engine time sampled once at the start of this block.
        void Render(Base::Span<std::int16_t> interleavedOut, Time::TimePoint anchor) noexcept;
    };

} // namespace Engine::Audio

namespace Engine::Audio::Sdl {

    struct SdlAudioOutputConfig {
        std::uint32_t deviceSampleFrames = 0; // 0: SDL's default; otherwise SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES
    };

    // Opens the default playback device as an SDL audio stream whose callback calls Mixer::Render.
    // Owns one reference to SDL's AUDIO subsystem. The mixer must outlive the output (GYO_ASSERT on destruction order).
    class SdlAudioOutput final {
    public:
        static Base::Result<SdlAudioOutput, AudioError> Open(Mixer& mixer, const SdlAudioOutputConfig& config);
        std::string_view DriverName() const noexcept;  // diagnostics only, never for branching
        std::uint32_t DeviceSampleFrames() const noexcept;
    };

} // namespace Engine::Audio::Sdl
```

### 時間戳換算成樣本位置（`v8-engine-audio-schedule`）

- 區塊開始時，後端呼叫 `Time::Now()` 一次，連同區塊開始前已輸出的樣本數 `F0` 構成錨點 `(T0, F0)`。只有混音器持有錨點，消費端不持有它的副本。
- 取出的請求換算為 `start = F0 + round((at − T0) × 48000) + D`，其中 `D` 是 `scheduleDelay` 換成的樣本數（常數）。
- `start ≥ F0`：在區塊內第 `start − F0` 個樣本（或之後的區塊）開始，記為準時。`start < F0`：在區塊開頭開始，記為遲到，`maxLateness` 更新（`v8-engine-audio-late`，2026-10-30 核准）。
- 同一個錨點下取出的兩個請求，時間戳相差 Δ，起點恰好相差 `round(Δ × 48000)` 個樣本；不同錨點之間，誤差是錨點的抖動（回呼何時被叫醒）與裝置時鐘和 Engine 時鐘的速率差在一個區塊內的累積。每個區塊都重新取錨點，不跨區塊沿用固定偏移，所以速率差不累積。
- 是否需要濾波（以多個錨點擬合直線）要看 L2 量到的抖動；v8 不做，列在 Replacement Criteria 4。
- 換算、遲到判定、SPSC 的記憶體順序是本計畫最容易「看起來對、其實錯」的部分，局部用 xhigh（`xhigh-reviewer`）。

### 即時安全

- `Render` 不配置、不釋放記憶體，不上鎖，不等待，不寫日誌，不丟例外（`noexcept`）。所有容量（聲部、佇列、片段庫）在建立時配置好。
- 片段在輸出期間不可變；輸出關閉之後片段庫才能銷毀（`Mixer` 擁有片段庫，`SdlAudioOutput` 必須先於 `Mixer` 銷毀）。
- 統計是 relaxed atomic 計數；讀取端只拿來顯示與驗收，不做決定。

### 與 SoundLoader 的關係

- `SoundLoader`（`engine/asset/src/loaders/SoundLoader.cpp`）讀 RIFF/WAVE PCM16、單聲道或立體聲，產出 `SoundAsset{sampleRate, channels, pcm16}`。它不檢查取樣率，也沒有任何測試或使用者。
- 建議（`v8-engine-audio-clip-format`）：`GYO::Audio` 不依賴 `GYO::Engine`；消費端把 `SoundAsset` 的欄位交給 `ClipBank::Add`，取樣率不是 48 kHz 時由 `Add` 回錯。`SoundLoader` 的測試（格式錯誤、奇數長度、非 PCM）在消費端第一次用它的批次補上，或在 `v8-engine-audio-core` 順手補，由使用者決定（同一代號）。

## Design Invariants

核准後，`v8-engine-audio-core` 把本節搬到 `docs/architecture/audio.md`（標題保持 `Design Invariants`），並為每一條寫上守衛。
更正（2026-10-30，`v8-engine-audio-core`）：已搬到 [audio.md](../../audio.md)，那裡是正本（含守衛與生效的批次）；本節保留核准時的原文。

1. **中立。** `GYO::Audio` 與後端只知道片段、時間戳、增益與樣本；不知道事件種類、武器、角色或任何產品名稱，也不提供以產品語意命名的 API。
2. **SDL 只在後端。** `GYO::Audio` 不含 SDL；後端的中立標頭不含 SDL（platform-sdl.md Invariant 1、2，G1～G3）。Engine 不呼叫全域的 `SDL_Init`／`SDL_Quit`；AUDIO 子系統由後端以參考計數擁有（platform-sdl.md Invariant 8）。沒有例外穿過 SDL 的 C 堆疊。
3. **一個時間基準。** 請求的時間一律是 `Time::MonotonicClock` 的時間點。換算成樣本位置只在混音器內、只用當前區塊的錨點；後端每個區塊取一次 `Now()`；不跨區塊使用固定偏移。
4. **播放位置的權威是混音器。** 已輸出的樣本數只有混音器擁有；消費端與後端不保存它的副本，也不以它做決定（AGENTS §3「狀態的權威」）。
5. **同錨點下間隔不變。** 同一個錨點下取出、都沒有遲到的兩個請求，起點的樣本差等於時間戳差換算的樣本數（四捨五入）。
6. **收支相等。** 每個請求恰好落入準時、遲到、佇列滿丟棄、聲部滿丟棄之一；`submitted` 等於四者之和。沒有不計數的丟棄。
7. **音效執行緒即時安全。** `Render` 不配置、不鎖、不等待、不寫日誌、不丟例外；片段在輸出期間不可變。
8. **決定性。** 給定相同的片段庫、相同的請求序列與相同的錨點序列，`Render` 的輸出逐位元相同（離線測試的基礎）。
9. **輸出格式固定。** 混音器只輸出 48 kHz、PCM16、立體聲；轉成裝置格式是 SDL 的事。

### Replacement Criteria

符合任一條時，停止修補，交給使用者決定「修補還是重建」：

1. 需要串流、解碼壓縮格式，或在輸出期間載入／卸載片段（片段庫的生命週期前提不成立）。
2. 需要聲部之外的訊號處理圖（匯流排、效果器、HRTF），或需要在送出後停止、修改聲音（迴圈、音樂）。
3. 需要第二個非 SDL 後端，而換算規則必須依後端各自重新驗證。
4. L2 量到的錨點抖動讓 Invariant 5 以外的間隔誤差超過消費端可接受的範圍，必須加濾波，而濾波又需要後端特有的資訊。
5. 生產者的數量或執行緒在建立時無法固定（需要任意執行緒送出，SPSC 的前提不成立）。
6. 每加一種播放功能，就得重新驗證換算、遲到判定或即時安全。

## 分批

檔位依 `agents/effort-gearbox.md`；引擎沒寫的一律「繼承」（主對話的引擎，目前 Opus 5.5）。沒有任何一批的檔位或引擎高於主對話，`xhigh-reviewer` 已在消費端計畫核准（「局部 xhigh 用 `xhigh-reviewer`（引擎繼承）」）。

```text
v8-engine-audio
├── v8-engine-audio-core       「high｜片段庫、聲部、離線 Render、收支、audio.md」
├── v8-engine-audio-schedule   「high（換算、遲到、SPSC 局部 xhigh）｜提交端與時間戳排程」
├── v8-engine-audio-sdl        「high｜SDL 串流回呼、AUDIO 子系統、G1～G3、dummy／disk 的 L1」
└── v8-engine-audio-record     「medium（分析程式 high）｜disk 驅動的 L2（只記錄）、突變、dev_log」
```

| 批次 | 檔位｜引擎 | 估時 | 內容 | 完成條件 |
|---|---|---|---|---|
| `v8-engine-audio-core` | high｜繼承 | 3～4 h | `engine/audio` 骨架與 CMake；`ClipBank`；聲部（固定上限，滿了丟新請求並計數）；混音（int32 累加、主音量與增益、飽和並計數）；`Render`（此批的請求以測試用的直接注入進入，不經佇列）；`MixerStats`；（更正：實作時改為模組內部的 `VoiceMixer`，L1 直接測它；累加用 int64、增益用 Q16 定點；公開的 `MixerStats` 隨 schedule 批的 `Mixer` 出現。見 HANDOFF 現況）`audio.md`（契約、Design Invariants、守衛表） | L1：片段驗證、離線渲染逐樣本比對、飽和、聲部滿、收支、決定性；突變 3 個以上被殺 |
| `v8-engine-audio-schedule` | high；換算、遲到、記憶體順序局部 xhigh（`xhigh-reviewer`，引擎繼承） | 4～5 h | `Submitter`（有界 SPSC、綁定執行緒、滿了計數）；`Render` 開頭取出全部提交端；錨點換算、`scheduleDelay`、遲到規則；`maxLateness` | L1：假錨點序列下的起點位置（跨區塊、區塊邊界、剛好遲到一個樣本）、Invariant 5、併發生產者壓力（多次執行、結構條件不設時間上界）、佇列滿的收支；`xhigh-reviewer` 的審查與修正；突變（換算的四捨五入、錨點沿用、遲到判定、記憶體順序放寬）被殺 |
| `v8-engine-audio-sdl` | high｜繼承 | 3～5 h | `engine/audio/backend/sdl`；`SDL_OpenAudioDeviceStream`（48 kHz S16 立體聲）與回呼（每次回呼取一次 `Now()`）；AUDIO 子系統的參考計數；開啟失敗回錯；銷毀順序的 `GYO_ASSERT`；G1～G3 擴充 | L1（CI 無頭可跑）：`SDL_AUDIO_DRIVER=dummy` 開關、與 `SdlPlatform` 共存時子系統計數正確、`disk` 驅動加 `SDL_AUDIO_DISK_TIMESCALE=0` 寫出檔案並檢查結構（長度、有聲音的位置與收支）；G3 對新 target 生效 |
| `v8-engine-audio-record` | medium；分析程式 high｜繼承 | 2～4 h | Engine 自己的 L2 程式與分析腳本（見下）；相關突變的完整一輪；dev_log；README／HANDOFF 收尾 | L2 只記錄（不是判定）；收支一致；dev_log 列入本夾 |

- 合計 12～18 h；任一批超過估時 1.5 倍就停下回報（AGENTS §14 冒頭）。
- 每批的 dev_log 與該批同一變更提交。
- `v8-engine-audio` 的四批與消費端的第一個音效批同一條 PR 線，合併時一起生效。

## 驗收

### L1（CTest，四平台 CI）

- 混音器的核心全部離線測試：不開裝置，以 `Render` 加假錨點驅動，比對逐樣本的輸出與統計（Invariant 5、6、8 的守衛）。
- 時間相關的 L1 只檢查結構（位置、順序、計數），不設時間上界。
- 後端的 L1 用 `dummy` 與 `disk` 驅動，CI 沒有音效裝置也能跑；不以任何產品的存在為前提。
- 守衛的檢出力用突變檢查表示（AGENTS §14.2；只跑相關突變，PR 線合併前在背景跑完整一輪）。

### L2 構想（`SDL_AUDIO_DRIVER=disk`）

`disk` 驅動把裝置要的每一塊樣本寫進檔案，檔案的樣本位置就是「實際播放」的位置，因此可以離線量起音。

- **Engine 的 L2（`v8-engine-audio-record`，只記錄）**：一支不開視窗的程式，以 `Time::Waiter` 跑 30 FPS 的「幀」迴圈；在 60 Hz 的 Tick 格點上產生事件（不規則的間隔，例如 6、7、5 Tick），在事件之後的下一幀才送出（模擬幀量化），時間戳用事件的 Tick 時間。片段是起音陡峭的短脈衝。分析腳本在輸出檔裡找起音，和時間戳比較：起音間隔對 Tick 間隔的誤差分布，以及間隔是否落在 16.67 ms 的格點而不是 33.3 ms 的幀格點。程式同時記下每個區塊的錨點 `(T0, F0)`，用來估計 `disk` 裝置相對 Engine 時鐘的速率。
- **一個必須先處理的事實**：`disk` 驅動以「每塊之後 `SDL_Delay(整數毫秒)`」模擬即時（見 HANDOFF「研究事實」）。SDL 在 48 kHz 的預設是每塊 1024 樣本（21.33 ms），延遲被截成 21 ms，`disk` 裝置因此比真實時間快約 1.6%，加上等待的晚醒（macOS 約 1～2 ms）又變慢。依換算規則，這個速率差會讓檔案裡的起音間隔和時間戳間隔差同樣的比例。對策（`v8-engine-audio-l2`，2026-10-30 核准）：L2 固定 `SDL_AUDIO_DEVICE_SAMPLE_FRAMES=480`（剛好 10 ms，不截斷），並用錨點估計的速率報告修正前後兩組誤差。
- **消費端的 L2**（消費端的音效批）：30 FPS 連射，起音間隔對應射擊 Tick。判定門檻在那一批開始前宣告並由使用者核准；本計畫只提供 Engine 的 L2 結果作為門檻來源之一。
- 真實裝置無法從檔案量起音；在真實裝置上只記錄錨點、區塊大小與遲到計數（`DriverName()` 寫進紀錄）。

## 停止條件

- AGENTS §14 冒頭的三種情況（同一問題換兩次方法沒有結論、原因改變契約或計畫前提、回歸或超出範圍）。
- 需要新的第三方依賴，或需要 SDL 的音效型別出現在中立標頭。
- 符合任一條 Replacement Criteria。
- 估時超過 1.5 倍。

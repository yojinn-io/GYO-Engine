# 2026-11-23 SDL 線子批 A：音訊輸出 `sdl_audio`（`v8-arch-gyo2-sdl-a`）

Owner：架構（`plugins/sdl*`）、audio Module。GYO2 主會話開的任務卡（使用者「開 W、A、3D 物理、PvP 移植準備」）。分支 `claude/gyo2-sdl-a-audio-output-e9dfda`，draft PR #306（開向 `gyo2`）。作者 Opus 5.5／high。
根據：[SDL 總宣告](../architecture/target/v2/sdl-declaration.zh-Hant.md)修訂 21 第 7 節、第 10 節的 A（使用者 2026-11-23 動工前核准「SDL 照建議核准」）。照新 AGENTS 不另寫事前宣告；取捨寫在 PR 說明與 [sdl.zh-Hant.md](../architecture/target/v2/sdl.zh-Hant.md) 第 5 節。

## 做了什麼

1. **`plugins/sdl_audio`**：只做 `Configure` 的單位 `SDLAudioPlugin`（建混音器或用產品給的 `AudioOutputMixer`；有 asset 登記處時登記 `WavLoader`；開預設播放裝置；`AudioOutput` 放進共用處；`OnDestroy` 停）。裝置開不了不是啟動錯誤（`AudioOutputFailure`，E2）。SDL 只在 `src/adapters/sdl/AudioDevice.cpp`。照 master 修過的 `SdlAudioOutput.cpp`，差別是只借混音器、拿掉 anchor 紀錄。
2. **測試**：`sdl_audio.dummy`（建混音器、登記 loader、播；設定、設定錯誤、產品的混音器與 Product 結束時停、兩個 Product 並存、anchor 每個 block 讀）、`sdl_audio.disk`（檔案裡恰好一個 frame、增益對）、`sdl_audio.unavailable`（不存在的驅動：回報、Product 照跑）。都經 Product、不含 SDL，驅動用環境變數選。計測程式 `gyo_sdl_audio_block_record`。
3. **`scheduleDelay`**：判定先寫（sdl 頁第 5 節）再量。macOS Intel（coreaudio、預設 1024 frame）最長間隔 21.463／21.470 ms → 54.8 ms → 60 ms 不變。Windows、Linux 沒有實機，未量。
4. **命名**：N17 的 Plugin 根由這批定——SDL 是縮寫；`plugins/<名>` ↔ `GYO::Plugins::<推出的名>` ↔ `GYO::<推出的名>`；naming_guard 把 `plugins/` 每個目錄當單位；P 的 `GYO::Sdl` 改 `GYO::SDL`。已發訊息給 W 對齊。
5. **audio 的後續小事**（批次表）：`Mixer.cpp` 兩份手寫 SPSC 環收成 `SpscRing.hpp`；突變的 find 跟著改，TSan 清單的 `au-ctl-clip-handback-relaxed`、`au-ctl-clip-reclaim-relaxed` 和同一行的突變重複了、退役。`au-ctl-stress-reassign-every-block` 加「long sounds」案例（只讀鳴響中的聲音、緊迴圈）。

## 查證（本機 macOS Intel，`test` preset）

- 新與相關的 CTest 通過：`sdl_audio.*` 3、`naming.guard`／`_selftest`、`gyo_audio_tests`、`gyo_audio_playhead_stress`、`gyo_audio_alloc_tests`、`os.boundary_guard`、`build.ci` 的 `test_plugin_removal.py` 15。
- 全量 213 本：`gyo_os_tests`、`os.wake_fallback` 逾時（單獨重跑也逾時；當時 load average 110，其他會話在建置；這批沒改 os）。`os.boundary_guard` 第一次失敗：`plugins/` 的測試也受 R4，disk 測試的 `std::ifstream` 改成 `OS::Files::FileReader`。
- 突變：`sdla-*` 7/7；命名 3/3；audio 改了 find 或殺手測試改了的 13/13。第一輪 `sdla-not-resumed`、`sdla-loader-not-registered` 存活：doctest 的 `--test-case` 用逗號切，過濾到 0 個案例就「通過」；另外 loader 的測試自己先登記了 `WavLoader`。兩處都改了。
- `au-ctl-stress-reassign-every-block`：原本的案例本機 4/4 killed；新的案例在突變下每次 2312～3881 次 NotPlaying（5 次），未改的程式 0 次。
- TSan 版的突變沒有跑（要 TSan 建置）。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

沒有 blocker；should 6、nit 6。處理：
- S1 測試讀值的先後（回呼先 Render 再計數，先讀混音器）→ 改。S2 long sounds 的渲染端不等讀者 → 改成讀者讀滿 10^5 次才停（不超過 clip 長度與時限）。
- S3 `OnDestroy` 和共用處的反序解構今天給出同一個先後，測試分不出 → 留著（總宣告第 7 節、audio 第 4 節的核准做法；不依賴 Create 的順序），sdl 頁範囲の限界寫明沒有守衛；`AudioOutput::Stop` 改成私有（只有 Plugin 呼叫）。
- S4 `SpscRing` 一大一小 → `SPSCRing`，縮寫表加 `spsc`。S5 audio 第 4 節第 2 點、命名規範 N17 的 Plugin 列與縮寫表沒跟上 → 改。S6 SDL 的全域狀態（`SDL_Quit` 會收掉別人的 stream、第一個 Init 的執行緒是 SDL 的主執行緒、hint 全域）→ sdl 頁第 2 節加共用規則，轉 W。
- N1 SDL-3 的「分幾次算」改由審查守；N2 TSan 突變補 DI 7 標記；N3 `deviceSampleFrames` 註明行程全域、環境變數優先；N4 `test_acronym_table`、測試資料 `GYO::SDLWindow`（`SdlBuildTests.cpp` 是 P 的檔、W 正在改 `plugins/sdl`，轉 W）；N5 換 `wav` loader 做不到，記在範囲の限界；N6 不改。
- 修後：相關 CTest 10/10；`au-sched-full-off-by-one`、`au-sched-head-not-published`、`au-ctl-stress-reassign-every-block`、命名 3 個，6/6 killed。

## G1、G2：播放中改增益與 loop

- **決定**：G1、G2 觸發 audio RC 2、6（「交使用者決定修還是重建」）。經主會話問使用者，選 (A) 修（原話「這個照你建議就好。」，HANDOFF `v8-arch-gyo2-audio-repair-not-rebuild`）。比過 (B) 重建成 kira 式的每聲音參數（Tween、loop region、track/bus）：範圍遠大於 Object_Connect 要的。主會話記的注意：之後再要 bus、空間音效、ducking 或循環區段，會再碰到 RC 2，那時考慮 (B)。
- **做法**：混音器的結構不換。`PlayRequest::loop`（整段循環，直到停止；序奏接循環段是兩個播放）。`Submitter::SetGain(handle, 左, 右, at, ramp)`：走同一個環，`at` 照起播的規則換成 frame（偏移、取整、夾住、`scheduleDelay`），晚到從 block 的第一個 frame 開始並記晚；起點是音訊執行緒取到時的增益、到起始 frame 前維持，未完成的變化被取代——照 Kira 的 `Parameter::set`（`crates/kira/src/parameter.rs:71-79`）。每個 voice 只持一段增益（from、to、起點、長度），增益是輸出 frame 的函數，所以取到之後和怎麼切 block 無關（DI 8 (a) 補一句）。新的帳 `gains*`（DI 6）。
- **別家**：kira `set_volume(…, Tween)`（`track/main/handle.rs:31`）、`set_loop_region`（`sound/static_sound/handle.rs:282`）；Bevy `AudioSink::set_volume`（立即，`bevy_audio/src/sinks.rs:172`）、`PlaybackMode::Loop`（`audio.rs:21`）；SoLoud `fadeVolume`／`setLooping`（`soloud.h:368`、`:333`）；miniaudio `ma_sound_set_fade_in_milliseconds`／`ma_sound_set_looping`（`miniaudio.h:11465`、`:11478`）。和 Bevy 不同處：Bevy 的音量立即生效、沒有時刻；這裡的變化和播放、停止在同一條時間線上（DI 3），淡出照 kira 有長度。
- **RC 6 的重驗**：`GainLoopTests.cpp`（換算、delay、晚到、取代、切 block、帳、環滿、環的順序、無效與別人的 handle、併發的帳；loop 的循環、切 block、播放位置、序奏接循環）、`RenderAllocTests.cpp` 一案（漸變與 loop 時 Render 不配置、loop 的 clip 在停止之後的下一次呼叫放掉）。gyo_audio_tests 94、alloc 12 通過。新突變 `au-gain-*`、`au-loop-*` 13/13 killed（`au-gain-render-allocates` 第一版用 `std::vector` 被最佳化掉而存活，改成明寫的 `::operator new`，同 `au-ctl-alloc-in-render`）。
- 文件：audio 第 2、3 節、DI 4、6、7、8、RC 2、6（最終文字在合併前核准）。

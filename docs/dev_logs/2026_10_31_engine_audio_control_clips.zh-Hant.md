# 2026-10-31 輸出執行中載入與卸載片段（`v8-engine-audio-control-clips`）

Owner：Engine（`GYO::Audio`）。計畫：[CONTROL](../architecture/plans/audio/CONTROL.md)「分批」。契約：[audio.md](../architecture/audio.md)。

## 為什麼做

資產工具 `audio_editor` 每改一次裁剪或增益，就重轉一次片段，要在不重建輸出的情況下播放新片段（F2）。原本 Invariant 7 規定「輸出執行中片段不變」；使用者已同意改成生命期規則（決策 `v8-engine-audio-control-invariants`），做法是每個提交端自己的片段槽，卸載走同一個環（決策 `-clip-model`＝a）。

## 做了什麼

- **`ClipId` 改成 64 位**：
  - bank 片段維持 1～N。
  - 執行中的片段：高 32 位是槽的 generation（≥1），低 32 位是 `(提交端索引+1)<<16 | 槽`。所以執行中的 id 一定大於所有 bank id，bank 的 `Contains` 不會認它。
- **`MixerConfig::clipSlotsPerSubmitter`**：0～256，預設 0（PvP 不配置任何新東西）。新增錯誤碼 `NoClipSlot`。
- **`Submitter::LoadClip`**（擁有者執行緒，輸出執行中也可以）：
  1. 先回收音效執行緒已交還的卸載中槽（acquire 讀旗標後才釋放樣本）。
  2. 以共用的 `ValidateClip` 檢查（`ClipBank::Add` 也改用它，既有突變 `au-clip-rate-unchecked` 的字串不變）。
  3. 複製到空槽，回傳 id。
- **`Submitter::UnloadClip`**：
  - 以 `Unload` 命令走同一個環，排在已送出的播放之後。
  - 入列成功才把槽改成卸載中，並把 generation 加 1（跳過 0），舊 id 立刻失效。
  - 佇列滿時不改狀態，可以重試。
- **音效執行緒端**（`Mixer::Render`）：
  - 播放執行中片段時，把槽的 view 與租約標記交給 `VoiceMixer`；開始一個聲部就把租約加 1（`noVoice` 不加）。
  - 取出 Unload 時記下 `unloadTaken`；若沒有聲部在讀，就交還。
  - 每個停止命令之後、每次 `Render` 之後，取出 `VoiceMixer` 的結束陣列，把租約減 1；歸 0 且已取出卸載時交還。
  - 交還＝先重設租約，再以 release 寫旗標（計畫的 B1）。
- **`VoiceMixer`**：
  - `ScheduledPlay` 多了片段 view 與不透明的租約標記。
  - 所有聲部結束的路徑都經過 `End()`：自然結束、到停止 frame、開始前停止、在區塊開頭停止。`End()` 把標記寫進建立時配置、容量為 `maxVoices` 的陣列（計畫的「結束陣列」，原列在 `-stop`，這批才有讀者）。
- **`MixerStats`**：新增 `unloadsSubmitted`、`unloadsTaken`、`unloadsDroppedQueueFull`。
- **測試**：
  - 新增 `tests/common/audio/ClipSlotTests.cpp`（15 案），包含一個壓力測試：一個執行緒持續 `Render`，主執行緒載入、播放、停止、卸載 200 輪。
  - `gyo_audio_alloc_tests` 加一案「Render allocates and frees nothing for clips loaded while output runs」。
- **突變目錄**：
  - `mutations.json` 新增 16 個 `au-ctl-clip-*`、`au-ctl-unload*`、`au-ctl-ended-*` 等。
  - `mutations-tsan.json` 新增 2 個：交還與回收的記憶體順序。
  - 新增 `mutations-asan.json`（1 個）。
  - find 字串被改寫的既有突變 4 個同批更新：`au-core-voice-never-freed`、`au-ctl-alloc-in-render`、`au-ctl-stop-voice-not-freed`、`au-ctl-stop-free-at-block-end`。
- **ASan 建置**：新設 `build/target/_build/audio-asan`。設定是 `-fsanitize=address`、`BUILD_TESTING=ON`、`GYO_APPS=`、`GYO_RENDER_DEVICE=NONE`。不選產品是因為 PvP 需要 SDL_GPU，和 `NONE` 衝突。
- **文件**：audio.md 的「What it is」、Contracts（片段槽的 happens-before 論證、沒有 `Render` 時不交還、generation 回繞、跨 mixer 的 id 偵測不到），以及 Invariants 6、7、11 的片段部分，改成已成立。

## 驗證與經緯

- **Red**：先把交還的 release store 拿掉（槽永遠不交還）。名稱篩選到的 18 案中，7 案失敗，都是依賴回收的案；壓力測試在這個骨架下會一直等，所以排除。之後還原，全部 Green。
- **本機**（macOS x86_64）：
  - core 38／38。
  - test preset 109／109（`-j2`）；`audio` 相關與 `object_fps_pvp.cpu` 6／6。
  - 特性測試的雜湊沒變。
- **突變**：
  - 第一輪 51 個：49／51。
    - `au-ctl-alloc-in-render` 變成 stale：`Render` 結尾多了 `ReleaseEnded`。更新 find 字串。
    - `au-ctl-clip-unload-flag-not-reset`（B1）存活。原因是生產者只回收「卸載中」的槽，所以舊的交還旗標不會立刻被用掉；但下一次卸載時，舊旗標會讓生產者在音效執行緒取出卸載之前就釋放樣本，是真的釋放後使用。把這個序列加進「does not hand back a reused slot early」之後被殺。
  - 審查後（加了 6 個突變、改了測試與 `Play` 的順序）重跑相關的 57 個（`au-core-*`、`au-sched-*`、`au-clip-*`、`au-ctl-*`）：57／57。
- **TSan**（`audio-tsan`）：審查後 `gyo_audio_tests`（68 案，含壓力測試）3 次都沒有警告；`mutations-tsan.json` 6／6。
- **ASan**（`audio-asan`）：審查後 3 次都沒有報告；`mutations-asan.json` 1／1。CI 沒有 TSan、ASan 的 job，這是範圍的限制。
- **ASan 的檢出力**：把「回收不等交還」的突變套在 ASan 建置上，壓力測試 3／3 報 `heap-use-after-free`。這個突變記進 `mutations-asan.json`。
- 和計畫宣告的突變不同之處（AGENTS §14.2）：
  - 計畫的 `-clip-lease-not-decremented` 寫的是「自然結束、停止、開始前」各一個。實作分成 render（自然結束與停止 frame 在同一行）、開始前停止、區塊開頭停止三個。停止 frame 的結束沒有單獨的突變，由「hands back a stopped clip once its voice ends / at its stop frame」擋住。
  - 另外多了 TSan 的 `-reclaim-relaxed` 和 ASan 目錄。

### xhigh 審查（`xhigh-reviewer`，xhigh，引擎繼承）

沒有 blocker。審查者逐條追過所有聲部結束路徑、B1、結束陣列的上限、交還的記憶體順序，都成立。以下逐項處置。

- **Major 1**：同一個執行中片段同時有兩個以上聲部的情況沒有測試，「租約是計數」沒有守衛，有 3 個會造成釋放後使用的突變能存活。
  - 加「keeps a clip until the last of its voices ends」。
  - 加突變 `au-ctl-clip-lease-flag-not-count`、`-clip-handback-ignores-count`、`au-ctl-ended-not-cleared`。
- **Minor 2**：結束陣列的容量、以及「每個停止後都取出」沒有測試。
  - 「plays a clip loaded while output runs」最後加上卸載與重載（兩個聲部在同一次 `Render` 結束）。
  - 加「releases the lease of every voice a block frees」。
  - 加突變 `au-ctl-ended-capacity-one`、`au-ctl-stop-no-drain`。
- **Minor 3**：壓力測試的值檢查太弱。改成「聽到的值只會變大」：一個聲部，依輪次入列。比聽過的值小，就是讀到舊樣本。
  - 限制：讀到「被下一代重用的已釋放區塊」時值會變大，這個檢查抓不到；這部分靠 ASan。
- **Minor 4**：`Play` 在 `ClaimOwner` 之前讀擁有者私有的槽狀態（只在誤用時是資料競爭）。
  - 改成先 claim 再檢查。
  - 加 `UnloadClip` 的擁有者測試與突變 `au-ctl-unloadclip-owner-unchecked`。
- **Minor 5**：文件和程式不一致，已修正：
  - 擁有權的說法加上 `LoadClip`、`UnloadClip`。
  - Programmer Error 清單改成「不在 bank、也不是這個提交端載入中的片段」。
  - VoiceMixer 的說明加上租約標記與 view。
- **Minor 6**：記錄（本 dev_log、README、HANDOFF）在這批補上。
  - Invariants 7、11 的守衛名稱和核准文字不同：守衛清單改用實際的測試案名稱，並多列了幾個守衛。只有加嚴，沒有放寬，回報時向使用者列出。
- **Minor 7**：generation 32 位回繞（同一槽卸載 2^32−1 次）。在程式註解與 audio.md Contracts 寫明；實際上到不了。
- **nit**：
  - `au-ctl-unload-marks-on-full` 的 `expect` 改成比對實際的 `GYO_ASSERT failed: IsRuntimeClip(...)` 訊息（手動確認這個突變會在 `Play` 的 assert 中止）。
  - 壓力測試在 join 之前不再 `REQUIRE`：失敗時先跳出迴圈，join 之後再檢查。
  - 跨 mixer 的 id 寫進 Contracts。

## 證據

- 本機：`build/target/_build/core`、`test`、`audio-tsan`、`audio-asan`。
- 突變：`run_mutations.py --catalog docs/architecture/plans/audio/scripts/mutations.json`（core）、`mutations-tsan.json`（`audio-tsan`，`TSAN_OPTIONS=ignore_interceptors_accesses=0`）、`mutations-asan.json`（`audio-asan`）。

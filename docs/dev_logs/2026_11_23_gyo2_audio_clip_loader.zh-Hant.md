# 2026-11-23 GYO2：audio 那一側的小批（clip 的來源、音效 loader）

批次代號：`v8-arch-gyo2-audio-clip-loader`。分支 `claude/gyo2-audio-clip-loader-7e1559`（從 `gyo2` 641a1f26；開工時 merge 到 2d7db6c0）。任務卡 `gyo2-audio-clip-loader`，主會話「[GYO2][不良少年]新架構重構」開的卡。宣告：[audio-clip-loader-declaration.zh-Hant.md](../architecture/target/v2/audio-clip-loader-declaration.zh-Hant.md)（修訂 2，使用者 2026-11-23 動工前核准「照建議，全部核准」，HANDOFF `v8-arch-gyo2-audio-clip-loader-approved`）。頁：[audio.zh-Hant.md](../architecture/target/v2/audio.zh-Hant.md)。

## 宣告的經過

- 修訂 0（d97150df）→ Fable 5.2／high 敵對式檢查：blocker 2、should 11、nit 9。最大的兩件：(E) 的決勝理由說「任何 World 都能播只有 (E)」不成立（(A) 也能）；「(i) 和 Bevy 相同」說過頭（Bevy 的混音核心 rodio 不認得 asset），loader 由誰登記也和 Bevy 不同。
- 修訂 1（6e79de6f）：全部採用；(E) 的內部做法照檢查的 N-4 改成「參照留在提交口這一側的表，音訊執行緒只還格號」（DI 7 回到結構保證，回收環不會滿）。
- 窄複查：blocker 0、should 6、nit 8。主要是「格號還得太早」的判定、表用光改成斷言、租約標記的編碼放不下 66561 格、規則 8 的位置。修訂 2（3b45170b）全部採用。
- 證據：主 checkout `build/evidence/gyo2-audio-clip-loader/declaration-r0-adversarial.md`、`declaration-r1-narrow-check.md`。

## 做了什麼

### clip（宣告第 1 題 (E)）

- `engine/audio/Clip.hpp`、`src/Clip.cpp`（新）：`Clip` 是共用取樣的不可變值（`shared_ptr<const Detail::ClipData>`，`ClipData` 裝取樣與聲道數，`View()` 每次現算、傳值；第一版快取了 view，設計審查 S-1 指出搬動後懸空，改回宣告的形）。`Make(span)` 複製一次，`Make(vector&&)` 不複製。規則照 `ClipValidation`，加一條 frame 數 ≤ 2^32 − 1（`ClipView::frames` 與 voice 的位置是 32 位元）。`AssetTypeName<Clip>` 是 `sound`。
- 拿掉 `ClipBank.hpp`、`ClipBank.cpp`、`ClipId`、`Submitter::LoadClip`／`UnloadClip`、`MixerConfig::clipSlotsPerSubmitter`、`MixerStats` 的三個卸載計數、`AudioErrorCode::NoClipSlot`（5 不重編）。
- `PlayRequest::clip` 是 `Clip`。`Mixer::Create(config)` 不再收庫。
- `Mixer.cpp`：
  - 命令 `Command` 改成可平凡複製（`static_assert`），播放帶 clip 的 view 值與參照表的格號；拿掉 `Unload` 命令。
  - 每個提交口：參照表（`queueCapacity + maxVoices + 1` 格的 `Clip`）、空格堆疊、整數回收環（同大小）。`Play` 先取空、拿一格、複製 clip 進去；環滿時那一格當場清掉放回。堆疊空是內部不變量被破（`GYO_ASSERT`）。
  - `Reclaim()`：`Play`、`Stop`（含無效 handle）、`StopAll`、`~Submitter`（被搬走的跳過）的開頭取空回收環。
  - 音訊執行緒：沒有 voice 的、voice 結束的，把格號 `HandBack` 進回收環（release；推之前 acquire 讀頭端）。音訊執行緒不握參照。
  - 租約標記改成「提交口加一 << 32 ｜ 32 位元格號」（舊的 `<< 16` 放不下 66561 格）。
- `VoiceMixer`：拿掉庫；`ScheduledPlay` 帶 `ClipView` 值；`Schedule` 的 Programmer Error 改成「沒有 frame 的 clip」。

### loader 與 Data Contract（宣告第 2 題 (i)、第 5 節）

- `engine/audio/WavLoader.hpp`、`src/WavLoader.cpp`（新）：`sound`／`wav`，規則照宣告 5.2 的順序（規則 2 走完整串 chunk 才判 3；聲道數留給 `Clip::Make`；只比 block align、不除）；位元組逐樣本組合；預設上限 64 MiB。
- `config/modules.csv`：audio 列移到 asset 之後、`depends=asset`、description 改寫。`engine/modules/audio/CMakeLists.txt` 加 `Clip.cpp`、`WavLoader.cpp`、改註解。
- `tests/common/audio/CMakeLists.txt`：`gyo_expect_links(gyo_audio GYO::Base GYO::Time GYO::Asset)`；`gyo_audio_tests` 加 `ClipTests.cpp`、`WavLoaderTests.cpp`，以 `GYO_AUDIO_CONTRACT_DIR` 指 fixture。
- fixture：`tests/common/audio/contract/player_footstep.wav`（origin/master 3b411e21 的同一個 git 物件 5fa88357，不增加 repo 大小）。

### 測試

- 設定從 `ClipBank::Add` 換成 `Clip::Make`、判定不變：`ScheduleTests`、`StopTests`、`PlayheadTests`、`CharacterizationTests`（**雜湊常數未改而通過**：混音的輸出逐位元相同）、`MixerTests`（`VoiceMixer` 層的測試用 `std::vector<Clip>` 保住取樣，傳 view）。
- `MixerTests` 的兩案換成 `Clip` 的：「Clip rejects clips the mixer cannot play」（DI 9 的守衛改名，兩種 `Make` 都判）、「Clip copies samples given as a span once and shares them from then on」（`data()` 相等＝不複製）。「foreign clip」的 Programmer Error 案改成空的 clip。
- `ScheduleTests`：「treats an empty clip, a bad gain and a moved-from handle as Programmer Errors」（`submitted == 1` 照舊：在任何計數之前斷言）。
- `ClipSlotTests.cpp` 刪除，15 案逐判定的對照在[合併前核准頁](../architecture/target/v2/audio-clip-loader-merge-approval.zh-Hant.md)第 2 節。新的：
  - `ClipTests.cpp`（看輸出）：執行中做的 clip 能播；外面全部放掉後排著的播放照樣播完；一個 block 內兩次停止的格號都回來；換手（A 播、`join`、B 播並放掉 A 的 clip）；TSan／ASan 的壓力案「plays and lets go of clips while another thread renders」。
  - `RenderAllocTests.cpp`（數每條執行緒的配置與釋放）：第一案改成 `Render` 與提交口的呼叫分段計數；新增六種結局（(a) 播放中呼叫不放掉、輸出逐樣本相同；(b) 結束後下一次呼叫放掉恰好一個 clip）、五種呼叫各自取空、同一個 block 結束兩個 voice、重用的格號不提早回來、環滿當場放掉且表不縮小、表與兩個環繞過兩圈、格號 65536 以上回到自己的提交口。
  - `WavLoaderTests.cpp`：每條規則的每個條件、先後五案、master 的檔、AS-3（兩條執行緒各 1000 次）、經 `AssetStore` 載入並播出、大小上限 `TooLarge`。
- **我自己寫錯、已改的兩處**（不是產品程式的錯）：(1) `RenderAllocTests` 的「停在之後的 frame」每個 block 用同一個 anchor，停止的時刻沒前進——改成每個 block 前進 10 ms；(2) `MixerTests`「output does not depend on how blocks split it」把 `bank` 放在 `build` lambda 裡，lambda 回傳時解構、view 懸空（未定義行為，單跑時碰巧通過，突變執行器的基準跑時失敗）——`bank` 移到 lambda 外。第 (2) 處讓第一次完整突變的基準無效，log 保留為 `mut-after-full-INVALID-dangling-test.log`。

### 文件

- audio 頁：第 1 節（依賴與 loader 的登記）、第 2 節（`Clip`、參照表與回收環、取空、loader）、第 3 節（執行緒契約含解構子、Programmer Error、參照表的記憶體順序）、第 5.2 節（clip 不綁提交口、引 O-2 原文〔`v8-arch-gyo2-audio-o2-cite-deferred`〕）、5.4、5.5、新的第 6 節（Data Contract）、DI 2、6、7、9、11、新 DI 12、RC 1、新 RC 10。最終文字在合併前核准。
- asset 頁第 1、9 節（音效 loader 在 audio）；`target/v2/README.md` 加宣告與 dev_log；`docs/cross-component-rules.md` CCR-9 的實作欄加 `Clip`（主會話 2026-11-23 指示在本 PR 改）。

## 檢證

計測的件數突き合わせ：本批沒有計測（混音器的計數判定在單元測試裡，`CheckBalance`）。

- **基準**（乾淨的 checkout，2d7db6c0 未改，本機 macOS Intel，preset `test`）：audio 的 3 個 CTest 通過（`base-ctest.log`）；TSan（`-fsanitize=thread`，`TSAN_OPTIONS=halt_on_error=1:ignore_interceptors_accesses=0`）3/3、ASan 3/3（`base-thread-ctest.log`、`base-address-ctest.log`）。突變：完整清單 77/78，存活的是 `au-ctl-stress-reassign-every-block`（未改的程式上，見下）；TSan 7/7、ASan 1/1（`mut-base-*.log`）。
- **改過之後**：
  - audio 的 3 個 CTest 通過；`gyo_audio_tests`（含壓力案）連跑 5 次都通過。
  - 全量 `ctest -L "cpu|shader" -j4`：181/182，`gyo_os_tests` 逾時（`after-full-ctest.log`）。單獨重跑 0.74 s 通過（`after-os-rerun.log`）。切分：環境——當時乾淨 checkout 上的突變基準同時在建置與跑測試，os 不依賴 audio。（合併前再跑一次全量，見核准頁。）
  - 突變（`mut-after-full.log`、`mut-after-rerun.log`；後者跑在 2d7db6c0＋工作樹上，內容即之後 commit 的 03b6ea0c）：完整清單 86 條，第一輪 78/86；沒殺的 8 條逐條處理——`au-link-engine`（連結清單變長後 CMake 的訊息在 expected 之後換行，expect 改成專屬的 `gyo_audio links .*GYO::Kernel`）、3 條 `au-ctl-stop-*`（find 的 `request.at` 隨程式改名成 `command.at`，純改名）、3 條新突變的 `cases` 含逗號（doctest 把逗號當分隔，一案都沒跑；改成不含逗號的片段）、`au-ctl-ended-not-cleared`（真的存活：缺「重用的格號不提早回來」的案，補上）。重跑 8/8 killed。合計 85/86；存活的 `au-ctl-stress-reassign-every-block` 在未改的程式上也存活。
  - `au-ctl-stress-reassign-every-block`：DI 4 播放位置看板的並行突變，標「只在並行時抓得到」；上一批（audio 重審 dev_log 第 62 行）已記為時序偶發（同一 commit 單獨 3/3 killed、未改 2/2 killed）。這次基準與改過之後各存活一次（單獨跑那一次也存活）。和本批的改動無關（`PublishVoices` 未改）；報主會話，不在本批處理。
  - TSan、ASan（同基準的組態）：audio 的 3 個 CTest 各 3/3、沒有報告（`after-thread-ctest.log`、`after-address-ctest.log`）。突變 TSan 7/7（含改寫到回收環兩端的 `-handback-relaxed`、`-reclaim-relaxed`）、ASan 1/1（`au-clip-entry-back-on-start-asan`，`heap-use-after-free`）（`mut-after-tsan.log`、`mut-after-asan.log`）。

### 突變清單的增減（AGENTS §14.5）

`mutations.json` 78 → 86。

- **退役 10 條**（機制拿掉）：`au-review-unload-counts-before-assert`（`UnloadClip` 沒了；性質由新 `au-clip-empty-counts-before-assert` 接手）、`au-ctl-clip-handback-ignores-voices`（由 `au-clip-entry-back-on-start` 接手「還得太早」）、`au-ctl-clip-lease-counts-novoice`（由 `au-clip-novoice-not-returned` 接手）、`au-ctl-clip-lease-flag-not-count`、`au-ctl-clip-handback-ignores-count`（租約計數沒了：一個播放一格）、`au-ctl-clip-unload-flag-not-reset`、`au-ctl-clip-reuse-without-check`、`au-ctl-clip-generation-not-bumped`、`au-ctl-unload-uncounted`、`au-ctl-unload-marks-on-full`（卸載、世代沒了）。
- **改寫**：`au-clip-rate-unchecked`（檔改 `Clip.cpp`、案改名）、`au-review-handoff-rebound`（案名）、`au-ctl-clip-lease-not-ended-*` 三條（結束不還格號 → 下一次呼叫的釋放數判定）、`au-ctl-ended-capacity-one`、`au-ctl-ended-not-cleared`、`au-ctl-stop-no-drain`（改到新的案）、`au-link-engine`（expect）、`au-ctl-stop-*` 三條（改名）、`au-ctl-alloc-in-render`（補 `guards`）。
- **新增 18 條**，每條只捕自己那個誤（`what` 與 `guards` 欄）：`au-clip-empty-counts-before-assert`、`au-clip-entry-back-on-start`、`au-clip-entry-back-on-later-stop`、`au-clip-novoice-not-returned`、`au-clip-reclaim-keeps-clip`、`au-clip-play-no-reclaim`、`au-clip-full-not-returned`、`au-clip-lease-entry-16-bits`、`au-clip-play-copies-samples`、`au-clip-make-vector-copies`、loader 的 8 條（`au-wav-rate-fixed`、`-bits-unchecked`、`-tag-unchecked`、`-block-align-unchecked`、`-odd-data-accepted`、`-truncated-code`、`-duplicate-accepted`、`-pad-ignored`）。
- `mutations-asan.json`：`au-ctl-clip-reuse-without-check-asan` → `au-clip-entry-back-on-start-asan`（同一個「還得太早」在 ASan 下的讀已釋放）。`mutations-tsan.json`：`-handback-relaxed`、`-reclaim-relaxed` 改到回收環兩端。
- 宣告列了但沒立的：「`Clip` 的複製改成深複製」——`Clip` 的複製是 `shared_ptr` 的預設複製，沒有可改的一行；改成兩條各自可改的（`au-clip-play-copies-samples`、`au-clip-make-vector-copies`）。

## 合併前的設計審查與 §14.4 的檢證（03b6ea0c 之後）

- 設計審查（Fable 5.2／high，唯讀，`design-review.md`）：blocker 0、should 5、nit 10，牽動已核准決定的 4 條。處理對照在[合併前核准頁](../architecture/target/v2/audio-clip-loader-merge-approval.zh-Hant.md)第 4.1 節。改了的：S-1（`Clip` 改回宣告的形，加「被搬走的 clip 的 view 是空的」判定）、S-4（DI 7 只留本質三件）、S-5（audio 頁兩處舊說法）、N-1（`ClipValidation.hpp` 併進 `Clip.cpp`）、N-2（搬動賦值先取空，執行緒契約與 `Mixer.hpp` 的註解寫進去）、N-9、N-10（含測試 CMake 的過時註解）。A-1（無效 handle 的 `Stop` 就是只取空的入口）交使用者選。S-2（loader 的失敗訊息裡 id 由每個 loader 各拼一次，owner asset）、S-3（`Mixer.cpp` 兩份手寫的 SPSC 環）轉主會話排到之後。
- 不改、記下的：N-3（空格堆疊與回收環可合成一個環；但「重用的格號不提早回來」與 65536 那一案的檢出力靠堆疊的順序，換成先進先出時測試不會失敗、只會悄悄失去檢出力）、N-4（兩個測不到的靜默分支；`VoiceMixer` 的結束清單「每次 `Stop` 之後一定要取」是沒寫在型別上的約定，下次動 `VoiceMixer` 時考慮改成回呼）、N-5（釋放數的基準用 10 frame 的 clip，今天釋放次數和大小無關）、N-6、N-7（65536 那一案與 `au-clip-empty-counts-before-assert` 是日後退役的候選）、N-8（`InvalidClip` 收五種理由，audio_editor 搬回時若要分碼再定）。
- §14.4 的檢證（Opus 5.5／high，唯讀，`verifier-14-4.md`）：19 列 CONFIRMED 13、PARTLY 6、WRONG 0。補了兩案（「keeps a clip two plays share until the second ends」「lets go, on the thread it was handed to, of a clip played before the hand-over」）、換手案再用 5 次、突變 `au-clip-play-holds-nothing`（86 → 87），核准頁的表照它改寫。
- 審查修正之後的一輪（`r2-*`）：全量 `ctest -L "cpu|shader"` 182/182；完整突變 86/87（存活的同上：`au-ctl-stress-reassign-every-block`）；TSan、ASan 各 3/3、沒有報告；突變 TSan 7/7、ASan 1/1。
- 審查修正的檢證跑的時候，我改了 `Mixer.hpp` 的註解（執行緒契約多寫搬動賦值）；只是註解，突變清單沒有以 `Mixer.hpp` 為對象的條目。

## 和宣告的偏差

- 上面「宣告列了但沒立的」一條。
- 宣告 10.4 的「表與回收環繞過兩圈」用 12 輪（表 3 格），另加「重用的格號不提早回來」一案（窄複查之後才發現缺）。
- `Clip` 的內部形第一版和宣告不同，設計審查後改回（上一節）。

## 之後

- 設計審查（Fable／high，唯讀）、合併前核准頁、PR（base `gyo2`），等「合併」。
- 報主會話：`au-ctl-stress-reassign-every-block` 在 gyo2 上兩次存活（基準與改過之後），上一批記成時序偶發；要不要重看那條突變或那一案的判定，交主會話。

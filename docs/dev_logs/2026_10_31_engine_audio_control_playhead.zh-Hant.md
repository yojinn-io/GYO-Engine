# 2026-10-31 公布播放位置（`v8-engine-audio-control-playhead`）

Owner：Engine（`GYO::Audio`）。計畫：[CONTROL](../architecture/plans/audio/CONTROL.md)「分批」。契約：[audio.md](../architecture/audio.md) Design Invariant 4、「Playheads」。

## 為什麼做

資產工具 `audio_editor` 要在波形上畫播放頭（F3）。這個數字只拿來顯示；播放位置的權威仍在混音器（Invariant 4）。決策 `-playhead-hook`＝a 要求在 board 留 Hook，做確定性的交錯測試。

## 做了什麼

- **`PlayheadBoard<Hook>`**（新檔 `engine/audio/src/PlayheadBoard.hpp`）：
  - 每個聲部一對 atomic：tag（handle）與 state（已混入的 frame 數，加上「已開始」位元）。
  - 寫端只有三個操作：
    - `Assign`：tag 清 0（relaxed）、寫 state（release）、寫 tag（release）。
    - `Publish`：寫 state（release）。
    - `Clear`：tag 清 0（release）。
  - 讀端 `Read`：讀 tag、讀 state、再讀一次 tag，兩次都相符才回傳。
  - Hook 在每個 store 之後、每個 load 之前被呼叫，正式建置是空操作。
  - `static_assert`：64 位 atomic 必須是 lock-free。
- **`Mixer`**：
  - 每次 `Render` 結尾呼叫 `PublishVoices`：逐一看聲部（`VoiceMixer::StateOf`），和音效執行緒私有的「已公布 tag」陣列比對。換了聲音就 `Assign`，同一個聲音就 `Publish`，變成閒置就 `Clear`。
  - 音效執行緒從不讀 board。
  - 新增 `Mixer::PlayheadOf(SoundHandle)`：任何執行緒、wait-free，回傳 Scheduled、Playing（含 frame）或 NotPlaying。無效的 handle 與 moved-from 的 Mixer 都回 NotPlaying。
- **測試**：
  - 新增 `tests/common/audio/PlayheadTests.cpp`，共 10 案，其中包含：
    - 確定性的交錯測試：寫端照真正的轉換執行，可以先 `Clear` 再 `Assign`＋`Publish`（5 個 store），也可以直接 `Assign`＋`Publish`（4 個 store）。讀端在每次 load 之前，把 board 設成「寫端前 n 個 store 之後」的狀態，做法是從固定初始狀態重放前 n 個 store，用例外停在第 n 個。n 不遞減，窮舉全部 182 個組合。
    - 輸出位元比較：有沒有執行緒在讀播放頭，輸出都相同。
    - TSan 輪詢壓力測試：音效執行緒同時是提交端，持續播放、停止，聲部一直被重用。
  - 新增 CTest `gyo_audio_board_readers`（`tests/common/audio/CheckBoardReaders.cmake`）：原始碼檢查，board 只在 `Mixer::PlayheadOf` 被讀。
- **突變**：`mutations.json` 新增 12 個 `au-ctl-playhead-*` 與 `au-ctl-board-read-in-render`；find 字串被改寫的 `au-ctl-alloc-in-render` 同批更新。

## 驗證與經緯

- **Red**：先讓 `PlayheadOf` 永遠回 NotPlaying，篩選到的 7 案中 4 案失敗。另外 3 案（交錯、輸出位元、TSan 輪詢）不經過 `PlayheadOf` 的回傳值，它們的檢出力由突變證明。還原後 Green。
- 本機（macOS x86_64）的最終結果見下面「最終驗證」。
- 突變目錄的 `cases` 又踩到一次逗號：案名「…scheduled, playing, then not playing」被 doctest 的 `-tc` 拆開，兩個突變因此存活。改成不含逗號的篩選後被殺，並檢查三個目錄都沒有含逗號的篩選。這是 stop 批記過的同一個坑。
- `au-ctl-alloc-in-render` 又變成 stale（`Render` 結尾多一行）。find 字串改成不依賴後面的行。
- 範圍的限制：
  - board 只有 atomic，把它的任何記憶體順序改弱，TSan 和這些測試都看不出來（交錯測試是循序一致的步驟），這一半靠 `PlayheadBoard.hpp` 的順序論證與審查。所以計畫的 `au-ctl-board-state-relaxed` 沒有列入必殺（計畫 M5、AGENTS §14.2）。
  - CI 沒有 TSan 與 ASan 的 job。

### 審查（xhigh 審查：`xhigh-reviewer`，xhigh，Opus 5.5；對抗式檢查：general-purpose，Sonnet 5.5，依決策 `-gears`）

- **Sonnet 對抗式檢查**：要推翻的主張是「`PlayheadOf` 不會回報別的聲音、wait-free、不改變輸出，而且四個寫端或讀端的突變都會被測試抓到」。結果是推翻不了。對方以 happens-before 推導確認順序論證成立，也確認窮舉涵蓋了所有循序一致的交錯。另外指出一個測試缺口：同一區塊內聲部被重用、走直接 `Assign` 的路徑，在 Mixer 層沒有測試。和 xhigh 的 M1 是同一點。
- **xhigh 審查**：沒有 blocker，以下逐項處置。
  - **M1**：同一區塊內聲部被重用的路徑沒有 Mixer 層測試，「把 `!=` 改成 `== 0`」的突變會存活。加「reports a voice taken by another sound within one block」與突變 `-playhead-reuse-not-assigned`。
  - **M2**：沒有測試觀察第 0 個以外的聲部。加「reports each of several sounds in its own voice」與突變 `-playhead-first-voice-only`、`-playhead-last-voice-skipped`。
  - **M3**：已核准的 Invariant 4 守衛「音效執行緒不讀 board（grep）」沒有實作，被我改寫成 review。現在照計畫實作成 CTest `gyo_audio_board_readers`，並加突變 `au-ctl-board-read-in-render`（在發布時讀一次 board，這個檢查會失敗）。所以守衛沒有被拿掉，不需要走 §14.4 的放寬程序。
  - **m1**：TSan 輪詢測試原本幾乎沒有並行轉換。改成音效執行緒持續播放、停止（400 個聲音、3 種長度、3 個聲部），判定是「不超過自己的片段長度、只增加、結束後不再出現」。
  - **m2**：計畫寫的是「提交端索引越界 → NotPlaying」並宣告 `-playhead-no-bounds-check`。實作逐一掃描 tag，沒有用 handle 當索引，所以不存在越界讀取，突變改成 `-playhead-no-validity-check`。這項差異記在這裡，並加「reports a handle it never issued as not playing」（序號沒發出過、前綴是第 64 個提交端）。
  - **m3**：
    - board 的註解寫明第一個 store 是 relaxed 的理由。
    - `Mixer.hpp` 寫明 moved-from 的 `Stats`、`PlayheadOf` 有定義。
    - audio.md 改成「任何記憶體順序」都看不出來，刪掉 x86 那句。
  - **m4**：4 個突變的 `expect` 改成實際失敗的那一行。
  - **nit**：
    - 加「已開始」邊界的測試（第一個區塊恰好混入 1 frame）與突變 `-playhead-started-off-by-one`。
    - 加 lock-free 的 `static_assert`。
    - 註解寫明「一個 tag 只在一個聲部」。
    - Contracts 補上「`PlayheadOf`／`Stats` 進行中搬移 Mixer 是 data race」。
    - 測試的縮排改正。
    - 「每次都 Assign」的突變在單執行緒下等價，只在並行時短暫回報 NotPlaying，照 §14.2 記為存活，沒有列入目錄。

### 最終驗證

審查處置後重跑，macOS x86_64：
- 相關突變（`au-core-*`、`au-sched-*`、`au-clip-*`、`au-ctl-*`）70／70。
- TSan：`gyo_audio_tests`（78 案）宣告跑 5 次，都沒有警告；`mutations-tsan.json` 6／6。
- ASan：3 次都沒有報告；`mutations-asan.json` 1／1。
- core 39／39（多了 `gyo_audio_board_readers`）；test preset 110／110（`-j2`，負載約 3）。

## 證據

- 本機：`build/target/_build/core`、`test`、`audio-tsan`、`audio-asan`。
- 突變：`run_mutations.py --catalog docs/architecture/plans/audio/scripts/mutations.json`（core）、`mutations-tsan.json`（`audio-tsan`）、`mutations-asan.json`（`audio-asan`）。
- 證據檔案：主 checkout（`/Users/karasu/Code/Source/GYO-Engine`，Intel Mac）的 `build/evidence/audio-control-20261031/`的 `final-ph.txt`（審查前）、`final-ph2.txt`（審查後的突變、TSan、ASan、core、test preset）。

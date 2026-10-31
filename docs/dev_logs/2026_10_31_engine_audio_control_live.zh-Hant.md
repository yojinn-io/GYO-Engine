# 2026-10-31 控制層在 SDL 上的驗證與收尾（`v8-engine-audio-control-live`）

Owner：Engine（`GYO::Audio`、`GYO::AudioBackendSDL`）。計畫：[CONTROL](../architecture/plans/audio/CONTROL.md)。契約：[audio.md](../architecture/audio.md)。

## 為什麼做

前四個實作批只在離線的 `Mixer` 上驗證。這一批要確認：控制層在真正的 SDL 後端（`dummy`、`disk` 驅動）上，停止、輸出執行中換片段、播放頭照樣成立；同時收尾文件，並在 PR 線合併前把整個突變目錄完整跑一次（D184）。

## 做了什麼

- `tests/common/audio_sdl/SdlAudioOutputTests.cpp` 新增 2 案：
  - **dummy 驅動**「stops, swaps and reports sounds while it runs」：依序做
    1. 載入片段、播放，等到 Playing；
    2. 停止，等到 NotPlaying；
    3. 卸載，等到唯一的片段槽回來；
    4. 播新片段，等到 Playing。

    只用 5 秒的掛起守衛，不設時間上界。
  - **disk 驅動**「plays a clip loaded after opening and stops it at its mapped frame」：
    - 輸出開啟後才載入片段並播放，再送出停止。
    - 檢查檔案中聽得到的部分是一段連續的 frame。
    - 結尾 frame 要等於「以錨點紀錄重算的停止 frame」。候選有兩個：送出前最後一個錨點，和之後第一個錨點（和 record 批的做法相同）。
- `mutations.json` 新增 `au-sdl-render-anchor-shifted`：送進 `Render` 的錨點比記進紀錄的晚 1 ms。只有 disk 測試以錨點重算停止 frame，所以只有它殺得掉。
- 文件：
  - `docs/architecture.md`：Runtime boundaries 表的 Audio 列，以及新增「audio control layer」一段（為什麼重建、做法、介面的變化、依賴不變）。這兩處是 `-plan` 批延過來的。
  - audio.md：Status 改成已完成；「Planned」一段改成完成的說明；Invariant 10 加上 disk 測試這個守衛。
  - CONTROL 的狀態改成完成；README、HANDOFF 收尾。

## 驗證與經緯

- **Red**：在 `Render` 的停止分支放 `continue`（停止入列但不生效）。
  - dummy 案在「等到 NotPlaying」失敗。
  - disk 案在 `stopsMatched` 與停止 frame 的比對失敗。
  - 在這個骨架下，disk 案的「等到 NotPlaying」仍會通過：timescale 0.1 時，10 秒的片段約 1 秒就自然播完。
  - 還原後 Green。
- **穩定性**：`audio_sdl.disk` 與 `audio_sdl.dummy` 以 `--repeat until-fail:30` 連跑 30 次，全部通過（disk 驅動以前在 CI 上偶發失敗過，所以先確認）。
- **突變**：`au-sdl-render-anchor-shifted` 以預期的訊息被殺。
- 一個操作上的失誤：手動做 Red 時，第一次用了含逗號的篩選「*stops, swaps*」，0 案被執行。改用不含逗號的篩選重做。
- **完整的突變與 preset**：見下一節。
- 範圍的限制：CI 沒有 TSan、ASan 的 job；四平台 CI 要等 PR 線開出來才會跑。

### 完整驗證

macOS x86_64，負載約 3：
- `mutations.json` 整個目錄（`au-core`、`au-clip`、`au-link`、`au-soundloader`、`au-sched`、`au-sdl`、`au-ctl`），在 test 建置上跑：82／82。
- `mutations-tsan.json` 6／6；`mutations-asan.json` 1／1。
- test preset 110／110（`-j2`）；core 39／39。

## 證據

- 本機：`build/target/_build/test`（含 `SDL_AUDIO`）、`core`、`audio-tsan`、`audio-asan`。
- 突變：`run_mutations.py --build-dir build/target/_build/test --catalog docs/architecture/plans/audio/scripts/mutations.json`（完整目錄），以及 `mutations-tsan.json`、`mutations-asan.json`。

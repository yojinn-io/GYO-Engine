# Engine audio：`audio_sdl.disk` 停止 frame 偶發失敗的修正

日期：2026-11-04。批次代號 `v8-engine-audio-disk-stop-window`。**完成**（判定變更經使用者核准，AGENTS §14.4；CI 等 PR）。計畫見 [audio 計畫](../architecture/plans/audio/README.md)。

## 為什麼做

- #138 合併後的 master CI（run 41775642108，Quick acceptance／toolchain／linux-x64）在 `SdlAudioOutputTests.cpp:292` 失敗：停止 frame `end = 9678`，候選 `8771`、`9198`。之後三次 master run 通過，看起來是偶發。
- 失敗的是 audio.md Design Invariant 10 的守衛「plays a clip loaded after opening and stops it at its mapped frame」（從 `v8-engine-audio-control-live` 起）。
- 使用者：「在這裡切分」，切分後選「照選項 1 修」，查出主因後選「標記加放寬」。看過 1～5 與驗證者的結果後：「核准並開 PR」（接受 4 的「沒有守住的」一面）。

## 原因（§14.1 的切分：Test 自身，環境觸發；實作照契約）

兩條路徑，都是測試的前提錯，Engine 照 audio.md 的契約運作：

1. **檔案開頭的靜音（主因）**：SDL 開啟 device stream 時裝置是暫停的。裝置執行緒一啟動就先跑一輪，暫停時把整塊寫成靜音（`SDL_audio.c` 的 `SDL_PlaybackAudioThreadIterate`，logical device paused 時 `br = 0`，其餘補靜音），disk 驅動照樣寫進檔案。`SdlAudioOutput::Open` 在開啟（第 122 行）之後才 resume（第 135 行），被裝置執行緒搶先時，檔案比 mixer 的 frame 多出一塊（480 frame），整段聲音往後移且仍然連續，只有停止 frame 的比對失敗。測試把檔案的第 N 個 frame 當成 mixer 的第 N 個 frame。
2. **入列前被耽擱（次要）**：測試只接受「sentAt 之前最後一個」與「之後第一個」block 的映射。測試執行緒在 sentAt 與 `Stop()` 入列之間被耽擱時，期間開始的 block 看不到 stop，取走它的是更後面的 block；過了 stopAt 就照契約停在該 block 的第一個 frame 並計為遲到。timescale 0.1 下每塊約 1 ms，耽擱約 0.5 ms 以上就會發生。

## 證據

| 試驗 | 結果 |
|---|---|
| 本機自然重複（修正前）：閒置 200 次、CPU 滿載 300 次 | 0 失敗（重現不了） |
| sentAt 與 `Stop()` 之間插入 0／0.5／1.5／3 ms（探測，舊判定） | 0／3／5／5 次失敗（各 5 次）；取走 stop 的 block 晚於候選，1.5 ms 起多為遲到 |
| 只做選項 1（放寬範圍）後閒置連跑 | 第 19 次失敗：暫停 0、`doneAt` 在 sentAt 後 0 µs，停止 frame 比候選多 480 |
| 選項 1 版本、零暫停 150 次，記「檔案 frame 數 − 回呼 frame 數」 | 差 0：147 次全部通過；差 480：2 次通過、1 次失敗（失敗的都差 480） |
| `Open` 的開啟與 resume 之間插入 5 ms（探測，無標記） | 5／5 失敗；檔案多 2400～2880 frame，扣掉後停止 frame 等於候選的映射 |

- CI 的樣本（`end` 比後一個候選正好多 480）和第 1 條吻合，不需要巧合；CI 當時沒有印出錨點，所以是吻合，不是證明。
- 證據檔：本機（macOS）主 checkout 的 `build/evidence/audio-disk-stop-flake/`（git 不追蹤）：探測 patch、各次輸出、`prefix_probe_150.txt`、Red／Green 與重複的紀錄、突變紀錄。

## 做了什麼

- `tests/common/audio_sdl/SdlAudioOutputTests.cpp`：
  - 共用的 `PlayAndStopOnDisk(pause)` 與 `CheckDiskStopFrame`；原守衛（名稱不變）以暫停 0 呼叫。
  - **標記**：開啟輸出前先送出 48 frame、值 500 的 bank 片段，第一個 block 取走時已過時，從 mixer 的 frame 0 開始。檔案裡標記的第一個 frame 就是開頭靜音的長度，扣掉後檔案 frame 與 mixer frame 一一對應。標記必須恰好一段 48 frame。
  - **範圍**：候選從「sentAt 之前最後一個錨點」到「`Stop()` 返回後（`doneAt`）第一個錨點」的每個 block。每個 block 的預期：映射 frame；映射已渲染過（早於該 block 的第一個 frame）時，預期該 block 的第一個 frame 且 `stopsLate == 1`，否則 `stopsLate == 0`。
  - 失敗時印出開頭靜音、`stopAt`、`doneAt` 與每個候選 block（錨點相對 sentAt 的 µs、第一個 frame、映射、是否遲到）。
  - 新增一案「stops at the frame of the block that takes a stop queued blocks later」：暫停 1 ms 與 3 ms，固定第 2 條路徑。
- `docs/architecture/plans/audio/scripts/mutations.json`：`au-sdl-render-anchor-shifted` 的預期訊息改為 `CHECK( found )`；新增 `au-sdl-stop-one-block-late`、`au-sdl-anchor-logged-block-late`、`au-sdl-late-stop-uncounted`。
- Engine 的程式不變。探測（延遲、印出）都是暫時的，已撤回。

## 判定變更（AGENTS §14.4；Design Invariant 10 的守衛，需使用者核准）

1. **守的性質**：經過 SDL 後端，stop 停在「取走它的 block」以該 block 的錨點換算的 frame；換算已渲染過時停在該 block 的第一個 frame 並計為遲到（audio.md Invariants 3、10）。
   - 舊守衛實際上沒有走到遲到的那一句（從不遲到，也不讀 `stopsLate`）。
   - 舊守衛還隱含檢查了「檔案的第 N 個 frame 就是 mixer 的第 N 個 frame」，也就是後端沒有在串流開頭多加任何 frame。這不在 Invariants 3、10 的文字裡；新判定放棄這一項（見 2(a)）。
2. **緩的面**（以前失敗、之後通過）：
   - (a) 檔案在 mixer 的 frame 0 之前有一段固定長度的內容：以前直接失敗，之後量出長度並扣掉。**不只 SDL 在 resume 前寫的靜音**，後端自己在串流開頭多塞的 frame（整條串流多一段延遲）也一樣被扣掉：驗證者的突變 V1（第一次回呼多放 480 個零 frame）舊判定 5／5 失敗、新判定 5／5 通過（`verifier/V1_backend_prefix_*.txt`）。
   - (b) 測試執行緒在 sentAt 與入列之間被耽擱，取走 stop 的是「sentAt 之後第一個」以後、「`doneAt` 之後第一個」以前的 block：以前失敗，之後接受該 block 的映射。
   - (c) 取走 stop 的 block 已晚於 stopAt：以前失敗（映射 frame 不會出現），之後接受該 block 的第一個 frame（須 `stopsLate == 1`）。
3. **締的面**（以前通過、之後失敗）：
   - (a) 遲到計數須和結果一致：準時的結果要求 `stopsLate == 0`，遲到的結果要求 1；以前不看。
   - (b) 候選 block 若已遲到，以前仍接受它的映射 frame（一個已渲染過的 frame），之後不接受。
   - (c) 標記須恰好一段 48 frame；沒有或被切斷就失敗。
   - (d) 標記依賴「遲到的播放從 block 的第一個 frame 開始」：遲到的播放晚一個 block 開始（驗證者的突變 V2）舊判定 0／5 抓到、新判定 5／5 抓到。
4. **緩的面守的性質是否仍被守住**：
   - 每個候選仍是精確比對（frame 相等），沒有容差；範圍只在 `doneAt` 與 sentAt 之間跨過 block 時變大，平常入列只需數 µs，範圍與舊判定相同。
   - 一個 block 的偏移仍被抓到：新突變 `au-sdl-stop-one-block-late`（stop 晚 480 frame）與 `au-sdl-anchor-logged-block-late`（錨點紀錄晚 480 frame）都被殺。
   - 錨點的換算：`au-sdl-render-anchor-shifted`（舊判定殺得掉）新判定也殺得掉。
   - 遲到的規則與計數：離線的 `gyo_audio_tests`（「applies a late stop of a started sound at the block's first frame」等，見 audio.md Invariant 10 的守衛）不變；disk 的遲到計數由新突變 `au-sdl-late-stop-uncounted` 示範。
   - 開頭的扣除不依賴錨點，量的是 mixer 的 frame 0 在檔案的位置，所以不會掩蓋錨點或換算的錯（驗證者的 V6「以前一個 block 的錨點換算」被零暫停的一案抓到，5／5）。
   - 換算錯誤的偵測主要靠零暫停的一案：暫停 1 ms、3 ms 的兩案範圍較寬，V6 在這兩案 11 次裡被接受 9 次。這兩案主要守遲到的規則與計數（V4「遲到的 stop 晚一個 block」6／6 被殺、V5「準時的 stop 計為遲到」4／4 被殺）。零暫停時範圍是 2 個 block，和舊判定相同（60／60，`verifier/probe_window_clean_60.txt`）。
   - **沒有守住的**：後端在串流開頭加入的固定延遲（2(a) 的 V1）。延遲不在 Invariants 3、10 的契約裡（Invariant 4 只提到「比聽到的聲音早一個輸出延遲」），目前沒有其他判定守它；是否接受由使用者決定。
5. **根據與證據**：上面「原因」與「證據」兩節；本機主 checkout 的 `build/evidence/audio-disk-stop-flake/`（驗證者的輸出在其下 `verifier/`）。
   - 「檔案 frame 數 − 回呼 frame 數」是整個檔案的差，不只開頭；差 480 卻通過的 2 次，推定是多出的一塊在檔案結尾（關閉時）。

## 驗證

- Red：舊判定（只接受 sentAt 前後兩個 block、不看遲到）跑新案例，5／5 失敗（1 ms 與 3 ms 各失敗一次，原守衛通過）：`red_old_judgment_run*.txt`。開頭延遲 5 ms 的探測、無標記：5／5 失敗（`open_delay_5ms_run*.txt`）。
- Green：同一探測、有標記：5／5 通過，量到開頭靜音 2400／2880 frame（`green_marker_open_delay_5ms_run*.txt`）。
- 重複：`audio_sdl.disk` 閒置 200 次、CPU 滿載（2 × 核心數的忙迴圈）100 次，全部通過（`green_marker_idle_200.txt`、`green_marker_load_100.txt`）。
- 突變（D184：殺手測試所在檔案改了，`au-sdl-*` 全部重跑）：13／13 被殺（`mutations_au_sdl.txt`）。
- 另一位驗證者（Opus 5.5／high，general-purpose；和實作同一個模型，計畫沒有另外指派）以實際資料核對 1～5：因果主張與證據的數字確認；1、2、4 不完整（上面已補：舊守衛隱含的「沒有額外 frame」、2(a) 的範圍、V1 沒有被守住），3 少列一項締的面（已補 3(d)）。自訂突變 6 個：V1 舊殺新不殺；V2、V4、V5 新殺舊不殺；V6、C1（同 `au-sdl-stop-one-block-late`）兩者都殺。結束時工作樹與 `change.diff` 位元組相同。

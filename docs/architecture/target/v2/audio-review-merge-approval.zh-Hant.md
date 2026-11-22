# 合併前核准：audio 重審（`v8-arch-gyo2-audio-review`）

更新：2026-11-22。**狀態：待使用者合併前核准。合併要使用者另外說「合併」。**宣告：[audio-review-declaration.zh-Hant.md](audio-review-declaration.zh-Hant.md)（修訂 2，動工前核准）。dev_log：[2026_11_22_gyo2_audio_review.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_audio_review.zh-Hant.md)。

**這一頁請你核准四件**：
1. 第 1 節：拿掉 DI 4 的原始碼掃描守衛 `gyo_audio_board_readers`（宣告第 12 節第 3 題的正式核准；AGENTS §14.4：DI 的守衛不論有沒有緩的面都要你核准）。
2. 第 2 節：DI 11 的守衛測試加了一個判定（只有緊的面；事後報告，你看過就好）。
3. 第 3 節：和宣告的偏差。
4. 第 4 節：Design Invariants 與 Replacement Criteria 的最終文字（新頁 [audio.zh-Hant.md](audio.zh-Hant.md)）。

設計審查與檢證的結論在第 5 節，判定的結果在第 6 節。

## 1. 拿掉 `gyo_audio_board_readers`（§14.4 的 1～5，照檢證者的更正改寫）

檢證者（Opus 5.5／high，唯讀）逐項核對了宣告第 12 節第 3 題的 1～5：第 3 項 CONFIRMED，第 1、2、4、5 項 PARTLY（結論成立，措辭要更準）。下面是照它的更正改寫的版本。報告：主 checkout `build/evidence/gyo2-audio-review/q3-verifier.md`。

1. **守的性質**：
   - 意圖：音訊執行緒不讀播放位置的看板（看板只供顯示，混音器不拿它做決定；DI 4）。
   - 實際檢查的是文字代理：`engine/modules/audio/src` 頂層的 `*.cpp`／`*.hpp` 裡，`board\.Read\(` 這段文字恰好出現 1 次，而且位在 `Playhead Mixer::PlayheadOf(` 到它之後第一個 `\n}\n` 之間（`CheckBoardReaders.cmake:5-36`）。
   - 所以有些違反本來就抓不到：音訊執行緒直接呼叫 `PlayheadOf`（例：在 `Render` 或 `PublishVoices` 裡）；換一種拼法（`board_.Read(`、經別名呼叫）；檔案放在子目錄或 `include/`。
2. **緩的面**：以前失敗、之後通過的情形——
   - `Mixer.cpp` 以外的檔裡出現 `board.Read(`；
   - `Mixer.cpp` 裡 `PlayheadOf` 以外出現 `board.Read(`；
   - 整個目錄的 `board.Read(` 不是恰好 1 個（含 0 個，例：`SOURCE_DIR` 錯了而掃描對象為空）；
   - `PlayheadOf` 的簽名文字不符（改成 `auto`、簽名換行），或它之後找不到 `\n}\n`（CRLF 換行、`} // …` 結尾）；
   - 註解、字串或任何以 `board` 結尾的名字（例：`clipboard.Read(`）裡的 `board.Read(`。
   緩掉的都是「文字上的讀」；第 1 項列的那些違反本來就不在它的範圍內。
3. **緊的面**：沒有（純刪除）。
4. **性質還有誰守**：**沒有別的判定守「音訊執行緒不讀看板」，拿掉後只剩設計審查。**
   - 主要根據是靜態走查（檢證者）：程式側引用這個守衛的只有 `tests/common/audio/CMakeLists.txt:22-26`、腳本本身與突變 `au-ctl-board-read-in-render`；`authority_scan`、`naming_guard`、`tests/common/scenario` 都不涉及 audio 或看板；正例 `git grep -c "board\.Read(" -- engine` 命中 `Mixer.cpp` 1 處，證明走查有效。
   - 相鄰但不同的性質「外部讀者不擾動輸出」由「output does not depend on someone reading playheads」（`PlayheadTests.cpp:178-199`，判定在 :198）守，它不守本條。
   - 輔助：把 `au-ctl-board-read-in-render` 對留下來的 `gyo_audio_tests`（全部案例，含壓力案例）跑，存活（`q3-proof.log`，建置與命令在 `q3-proof-README.txt`）。限界：那是一般組態、不是 ThreadSanitizer（宣告寫「＋ThreadSanitizer 的壓力測試」是錯的）；`gyo_audio_alloc_tests` 沒跑；而且這條突變讀一個永遠不會發出的 handle、結果丟掉，按構造任何行為測試都抓不到，所以存活只當輔助，不當主要根據。
5. **根據**：
   - 分類：它是綁實作方式的守衛（AGENTS §3「実装の方式に縛るガード」）——綁檔名 `Mixer.cpp`、`PlayheadBoard.hpp`，綁簽名字串，綁成員名 `board` 與方法名 `Read`，綁 `\n}\n` 的格式；搬到子目錄時它不失敗，只是靜默地不掃。
   - 讀看板不影響即時安全：`noexcept`、只做 acquire load、預先配置、`static_assert` lock-free（`PlayheadBoard.hpp:10`、`:64-76`）。
   - 和 AGENTS §3 的連結：那一段管「何時立」（含這個機構的 scenario test 通過之前不立）。gyo2 上還沒有含混音器的 scenario test（`world_audio` 不在本批），照 §3 這個守衛現在不該立著。它是 2026-10-31 照當時核准的計畫立的（master 的 dev_log `2026_10_31_engine_audio_control_playhead.zh-Hant.md:48`），所以拿掉要你核准。

**核准後一起做**（同一個 PR）：刪 `tests/common/audio/CheckBoardReaders.cmake` 與 `CMakeLists.txt:22-26` 的 `add_test`；退役突變 `au-ctl-board-read-in-render`（唯一依賴它的突變；其他含看板的突變由 `gyo_audio_tests` 的案殺）；新頁 DI 4 刪那一項守衛與〔〕註；`audio.md` 的 Legacy 行把 DI 4 的「pending its approval」改成已拿掉。`docs/architecture/plans/kernel/guards-inventory.zh-Hant.md:161` 仍寫「留」、路徑是舊的 `engine/audio/src`——那是 kernel 研究線的盤點表，不是本卡的檔，轉主會話。

## 2. DI 11 的守衛測試加了一個判定（只有緊的面；事後報告）

設計審查指出：收下 `unloadsSubmitted == 1` 的「treats misused runtime clips as Programmer Errors」（`ClipSlotTests.cpp`）是 DI 11 的守衛測試。

1. **守的性質**：DI 11（執行中的 `ClipId` 指一格的一個世代、誤用是 Programmer Error）；新加的判定守 Contract「Programmer Error 在任何計數之前斷言」，原本由被拿掉的綁定測試 `ClipSlotTests.cpp:323` 守。
2. **緩的面**：沒有——原有的判定一個沒動。
3. **緊的面**：以前通過、之後失敗的情形：任何誤用的 `UnloadClip`（別的提交口的 clip、庫的 clip、卸載過的、卸兩次）在斷言之前先記了數。
4. **原性質由誰守**：原有的 `GYO_CHECK_ASSERTS` 全部照舊。
5. **根據與證據**：新突變 `au-review-unload-counts-before-assert`（`UnloadClip` 先計數後斷言）killed，`expect` 指這個判定（`mut-after-r2.log`）。「foreign or never-issued handle」（`StopTests.cpp`）同樣收下 `stopsSubmitted == 2`，那一案不在 DI 守衛清單（Contract），照一般測試處理。

## 3. 和宣告的偏差

| # | 宣告 | 實際 | 理由 |
|---|---|---|---|
| D-1 | 判定 2：換手案「A 呼叫五個成員，B 繼續」 | 第一版的測試只讓 A 呼叫 `Play` 或 `LoadClip`；設計審查指出只在 `Stop`／`UnloadClip` 插回綁定不會被抓。改成兩邊都呼叫全部相關成員（Stop 案：`Play`、`Stop`、`StopAll`；Clip 案：`LoadClip`、`Play`、`UnloadClip`），對未改的程式重做「先紅」（`handoff-red-before-change-r2.log`，三案都紅） | 判定照宣告的字面落實 |
| D-2 | 新增 1 條突變，插在 `Play` | 插入點移到四個成員共用的 `Submitter::Enqueue`，三個換手案都殺（`cases` 合併）；另新增 `au-review-unload-counts-before-assert`（第 2 節） | 設計審查 B-1、B-3；突變 95 → 79 |
| D-3 | 判定 3：TSan 用 `halt_on_error=1` | 加 `ignore_interceptors_accesses=0`（macOS 要它才看得到環的 memcpy 寫入，`mutations-tsan.json` 的說明）重跑未改／改過，並跑 `mutations-tsan.json` 的 7 條 | 設計審查 A-4；只用 `halt_on_error=1` 的那兩次對環的寫入沒有檢出力 |
| D-4 | 第 3 題的實測「＋ThreadSanitizer 的壓力測試」 | 只在一般組態跑；改以靜態走查為主要根據（第 1 節第 4 項） | 檢證者；這條突變按構造任何行為測試都抓不到，TSan 下再跑也證明不了更多 |
| D-5 | 宣告第 9 節第 1 步的表只列兩個搬走的計數判定 | 舊 `ScheduleTests` 那一案還有第三個計數判定 `submitted == 1`；它守的「斷言在計數之前」由「foreign clip, bad gain and moved-from handle」那一案的 `submitted == 1`（`ScheduleTests.cpp`）照舊守，不必搬 | 設計審查 A-6 |
| D-6 | 錯誤值「不重編」只寫在文件 | 加 `static_assert(NoClipSlot == 5)`（`ClipSlotTests.cpp`） | 設計審查 B-2；不立突變 |
| D-7 | — | TSan 基準第一次和改測試同時建置而無效，改在乾淨的 checkout 重跑 | dev_log「驗證與經過」 |

## 4. Design Invariants 與 Replacement Criteria 的最終文字（請你核准）

全文在新頁 [audio.zh-Hant.md](audio.zh-Hant.md)。和 `audio.md` 不同的地方：
- **DI 1**：拿掉「and its backend」。
- **DI 2**：只留 audio 這一側（連結恰好是 `GYO::Base`、`GYO::Time`）。
- **DI 3**：加「送進混音器的時間點（`PlayRequest::at`、`Stop`／`StopAll` 的 `at`）是同一行程的時間基準上的時刻；可縮放的遊戲時間不是混音器的輸入，遊戲的暫停與時間倍率不作用在混音器上。守衛：審查。」
- **DI 4**：第 1 節核准後刪 `gyo_audio_board_readers` 一項。
- **DI 7**：經提交口載入的 clip「只由使用那個提交口的一方配置與釋放，那一方永遠不是音訊執行緒」。
- **DI 10**：拿掉經 SDL 後端的守衛。
- **Contract**：`Submitter` 不綁執行緒（同時一方、換手要 happens-before）；時間點的限界；搬動與銷毀 `Mixer` 的責任在持有者；第 4 節後端要求的介面。
- **RC 1**：拿掉「執行緒」；**RC 2**：補「含跟著遊戲的暫停或時間倍率走」；**RC 5**：改寫並記「2026-11-22 判定已成立、選修、數量交 `world_audio`」；**RC 9**：新增搶占。

## 5. 設計審查與檢證

- **設計審查＋突變清單的敵對式檢查**（Fable 5.2／high，唯讀；報告 `build/evidence/gyo2-audio-review/design-review.md`）：blocker 0、should 7、nit 8；突變清單找到 3 處改了實作也沒有測試會失敗。處理：

  | # | 處理 |
  |---|---|
  | A-1 | 採用：`Mixer.hpp` 的「one per producer thread」改成「one per producer」 |
  | A-2 | 採用：新頁開頭與 `audio.md` 的 Legacy 行補 Contract 的兩處與第 4 節 |
  | A-3 | 採用：第 2 節 |
  | A-4 | 採用：D-3 |
  | A-5 | 採用：`au-ctl-board-read-in-render` 改 `command` 後的 killed 在 `mut-full.log`（489aadcb）與 `mut-after-r2.log`；q3 的建置記在 `q3-proof-README.txt` |
  | A-6 | 採用：D-5 |
  | A-7 | 採用：RC 5 的「已成立」只寫 O-8 |
  | A-8 | 採用：新頁補 `stopsLate`（matched）、DI 3 的後端那一半、DI 4 壓力守衛的條件與限界、DI 8 的條件與由來、例子的前提 |
  | A-9 | 採用：`Mixer.hpp` 的 `at`、`Render` 的 anchor 註解帶 DI 3 的邊界；類別註解拿掉 runner／World |
  | A-10 | 轉主會話（`target/README.md:29` 的 audio 歸類、`rebuild-concept` 2.1 的 audio 列、HANDOFF 代號） |
  | B-1 | 採用：D-1、D-2 |
  | B-2 | 採用：D-6 |
  | B-3 | 採用：第 2 節的新突變 |
  | B-4 | 採用：`guards` 改用頁上的節名（`audio 第 3 節`） |
  | B-5 | 採用：`mutations-asan.json`、`mutations-tsan.json` 的說明改執行器路徑 |

  §14.2 的「加上之後的結果」：見第 6 節的突變列。

- **§14.4 檢證者**（Opus 5.5／high，唯讀）：第 1 節。

## 6. 判定的結果

- 判定 1（既有行為不變）：audio 的 CTest 全部通過；CharacterizationTests 的雜湊常數沒動。
- 判定 2（換手，先紅）：三案在未改的程式上各自紅（`owner == self`，exit 134；`handoff-red-before-change-r2.log`），改過之後通過。
- 判定 3（TSan，`-fsanitize=thread -g`、`TSAN_OPTIONS=halt_on_error=1:ignore_interceptors_accesses=0`，本機 Intel Mac，乾淨的 checkout）：未改的程式 4/4（`tsan2-base-ctest.log`）、改過的 9d1e0d19 4/4（`tsan2-after-ctest.log`），都沒有報告；`mutations-tsan.json` 7/7 killed（`mut-tsan-after.log`），證明這個組態對環的記憶體順序有檢出力。先前只用 `halt_on_error=1` 的兩次（`tsan-baseline-ctest.log`、`tsan-after-ctest.log`）也通過，但對環的寫入沒有檢出力（D-3）。
- 判定 4（連結）：`au-link-engine` killed。
- 突變：改之前 3/3（`mut-baseline.log`）；改過之後相關 4/4（`mut-after-r2.log`：`au-review-handoff-rebound`、`au-review-unload-counts-before-assert`、`au-ctl-stop-sequence-zero`、`au-ctl-board-read-in-render`）；完整清單（489aadcb，78 條）77/78，存活的 `au-ctl-stress-reassign-every-block` 是時序偶發：同一個 commit 上單獨跑 3/3 killed、未改的程式上 2/2 killed（`stress-reassign-on-change.log`、`stress-reassign-on-base.log`），它標明「只在並行時抓得到」，當時機器同時跑審查 Agent。
- 全量 `ctest -L "cpu|shader"`（9d1e0d19＋未提交的 dev_log 與本頁，本機 Intel Mac，preset `test`）：169/169（`full-ctest.log`）。
- L1：PR 上的三平台結果，開 PR 後補。

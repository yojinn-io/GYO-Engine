# 2026-11-22 GYO2：第 6 批 audio 重審

批次代號：`v8-arch-gyo2-audio-review`。分支 `claude/gyo2-audio-review-71df0d`（從 `gyo2` eacab4d7；中途 merge 到 8e894776）。任務卡 `gyo2-audio-review`，主會話「[GYO2][不良少年]新架構重構」開的卡。宣告：[audio-review-declaration.zh-Hant.md](../architecture/target/v2/audio-review-declaration.zh-Hant.md)（修訂 2，使用者 2026-11-22 動工前核准「照建議，全部核准」，HANDOFF `v8-arch-gyo2-audio-review-approved`）。新頁：[audio.zh-Hant.md](../architecture/target/v2/audio.zh-Hant.md)。

## 重審的結論（宣告第 2～5 節的摘要）

- 混音器在 World 外、Product 一份、只提供機制；時間線留在時間基準，遊戲的暫停與倍率不作用在混音器上（Bevy、Kira、Unity、miniaudio 都在音訊那一側排程；SoLoud 的 `playClocked` 是之後「預設起播時刻」的參考）。
- `Submitter` 的執行緒綁定拿掉：SPSC 只需要「一個生產者、前後有先後關係」，綁定和 World 不綁執行緒、World 整個交出衝突。
- RC 1、RC 5 判定已成立，使用者選修；提交口的數量、預設起播時刻、clip 的來源（`ClipBank` 該不該存在）交新的 `world_audio` 一批（主會話已加進批次表）。〔2026-11-22 更正：`world_audio` 這一批撤掉（`v8-arch-gyo2-world-audio-withdrawn`），見下面「範圍擴大」。〕
- 「規則外移」適用，audio 今天只剩機制（11-13 的判準照 `v8-arch-adapter-plugins-review`：audio core 留 Engine）。
- 經過：宣告修訂 0 → Fable 5.2／high 敵對式檢查（blocker 2、should 17、nit 9）→ 修訂 1（範圍縮小）→ 窄複查（blocker 0、should 10、nit 10）→ 修訂 2 → 核准。使用者 clone 了 Kira、SoLoud、miniaudio，核對結果補進宣告第 0 節。

## 做了什麼

### 執行緒契約（310d25e2）

- `engine/modules/audio/src/Mixer.cpp`：拿掉 `Submitter::ClaimOwner` 與 `SubmitterSlot::owner`（和 `<thread>`）；註解的「owner」改成「使用提交口的一方」。
- `Mixer.hpp`：`Submitter` 的類別註解改寫成新契約（不綁執行緒；同時一方；換手要 happens-before，舉例與「只是時間上錯開不算」）；`Play` 的 Programmer Error 清單拿掉「別的執行緒」、補「在任何計數之前斷言」；`Stop` 拿掉「the call still claims the submitter for its thread」。
- 測試（宣告第 9 節第 1 步的表）：
  - `ScheduleTests.cpp`「belongs to the first thread…」→「plays on another thread after a hand-over」。
  - `StopTests.cpp`「stops belong to the thread…」→「stops on another thread after a hand-over」；原本的 `stopsSubmitted == 0` 搬到「foreign or never-issued handle」那一案，寫成 `stopsSubmitted == 2`（只有兩次合法的停止記數）。
  - `ClipSlotTests.cpp`「clip calls belong to the thread…」→「loads on one thread and plays and unloads on another after a hand-over」（一格，重載成功表示那一格已交還並在新執行緒上釋放）；原本的 `unloadsSubmitted == 0` 搬到「misused runtime clips」那一案，寫成 `unloadsSubmitted == 1`。
  - 判定變更的說明（§14.4，一般測試）：三個綁定的斷言判定隨機制拿掉（守的合約不再存在）；兩個計數判定守的「Programmer Error 在計數之前斷言」照舊，換到別的案守，期待值隨案例改成該案合法呼叫的次數。
- `AudioError.hpp`：拿掉 `DeviceUnavailable`（4），`NoClipSlot` 留 5、不重編。

### 突變清單（310d25e2、489aadcb、9d1e0d19）

`docs/architecture/plans/audio/scripts/mutations.json` 95 → 79：
- 退役 14 條（gyo2 跑不起來）：find 指向已刪檔案的 12 條（`au-soundloader-bits-unchecked`、11 條 `au-sdl-*`）＋ target 是 gyo2 沒有的 `gyo_audio_sdl_tests` 的 2 條（`au-sdl-stop-one-block-late`、`au-sdl-late-stop-uncounted`）。
- 退役 4 條（機制拿掉）：`au-sched-owner-unchecked`、`au-ctl-stop-owner-unchecked`、`au-ctl-stopall-owner-unchecked`、`au-ctl-unloadclip-owner-unchecked`。
- 改指向：`au-link-engine` 從 gyo2 不存在的 `GYO::Engine` 改成 `GYO::Kernel`，補 `guards: ["audio DI 2"]`。
- 新增 `au-review-handoff-rebound`：在 `Submitter::Enqueue`（`Play`、`Stop`、`StopAll`、`UnloadClip` 共用）插回綁定（`thread_local`＋函式內原子變數；執行器一條突變只改一處，`<thread>` 已拿掉，所以不用執行緒 id），三個換手案都以 `mine == slot_` 的斷言失敗；`guards` 判定 2 與 `audio 第 3 節`。只有它抓得到「實作退回綁定」。第一版插在 `Play`、只跑一案，設計審查指出只在 `Stop`／`UnloadClip` 插回不會被抓（B-1），9d1e0d19 移到 `Enqueue`。
- 新增 `au-review-unload-counts-before-assert`（9d1e0d19，設計審查 B-3）：`UnloadClip` 先計數後斷言，只有「misused runtime clips」搬過去的 `unloadsSubmitted == 1` 抓得到。
- `au-ctl-board-read-in-render` 的 `command` 原本寫死 `build/target/_build/core`，改成 `{cmake} -P tests/common/audio/CheckBoardReaders.cmake`（不依賴建置目錄）；補 `guards`。第 3 題正式核准後隨守衛退役。
- 碰到的 `au-ctl-stop-sequence-zero`、`au-ctl-unload-marks-on-full` 補 `guards`；三份清單說明的執行器路徑改成 `tests/common/mutation/run_mutations.py`。

### 設計審查之後（9d1e0d19）

- 換手的三案改成兩邊都呼叫全部相關成員（Stop 案 `Play`／`Stop`／`StopAll`，Clip 案 `LoadClip`／`Play`／`UnloadClip`），對未改的程式重做先紅。
- `ClipSlotTests.cpp` 加 `static_assert(NoClipSlot == 5)`（錯誤值不重編）。
- `Mixer.hpp` 註解：`CreateSubmitter` 不提 thread；`at` 與 `Render` 的 anchor 帶 DI 3 的邊界；類別註解不提 runner／World。
- 新頁與 `audio.md` 的 Legacy 行補漏列的改動與譯文漏處。

### 文件（310d25e2）

- 新頁 [target/v2/audio.zh-Hant.md](../architecture/target/v2/audio.zh-Hant.md)：Design Invariants 1～11、Replacement Criteria 1～9 的草案（改 DI 1、2、3、4、7、10，Contract 的執行緒契約，RC 1、2、5、9；其餘照 `audio.md` 譯過來），第 4 節是後端要求的介面。
- `docs/architecture/audio.md` 開頭加 Legacy 行，列出被取代的條文。
- `target/v2/README.md` 加一列。
- 交換點宣告第 110 行（E-16）、第 372 行旁加更正（主會話指定在本批改，原文不刪）。
- 測試的 CMake 註解改指新頁。

## 驗證與經過

- 基準（未改，gyo2 eacab4d7，本機 macOS Intel，preset `test`）：audio 的 4 個 CTest 全部通過（`baseline-ctest.log`）。
- **TSan 基準第一次無效**：它在背景建置時，我正在改測試，編進了改過的換手案配舊程式，`gyo_audio_tests` 因 `owner == self` 中止。不算數，log 保留為 `tsan-baseline-INVALID-*`。改在另一個乾淨的 checkout（78535950，未改）重跑：4/4 通過（`tsan-baseline-ctest.log`，`-fsanitize=thread`，`TSAN_OPTIONS=halt_on_error=1`）。之後跑突變與全量時都不在同一個工作樹改檔。
- 判定 2「先紅」：三個換手案在未改的程式上各自紅（`GYO_ASSERT failed: owner == self`，exit 134）；兩個搬過去的計數判定通過（`handoff-red-before-change.log`；改寫後的版本重做一次，`handoff-red-before-change-r2.log`，同樣三案紅、兩案過）。
- 判定 1、2：改過之後 audio 的 4 個 CTest 全部通過；CharacterizationTests 的雜湊常數沒動。
- 判定 3：改過之後 TSan 組態 4/4 通過、沒有報告（`tsan-after-ctest.log`）。設計審查指出 macOS 要 `ignore_interceptors_accesses=0` 才看得到環的 memcpy 寫入，加上之後未改 4/4、改過 4/4、`mutations-tsan.json` 7/7 killed（`tsan2-*`、`mut-tsan-after.log`）。
- 判定 4：`gyo_expect_audio_links` 照舊（`au-link-engine` killed）。
- 突變（只跑相關的，D184）：改之前 3/3 killed（`au-ctl-stop-sequence-zero`、`au-ctl-unload-marks-on-full`、`au-link-engine`，`mut-baseline.log`）；改過之後 4/4 killed（`mut-after.log`）；設計審查改完之後 4/4 killed（`au-review-handoff-rebound`、`au-review-unload-counts-before-assert`、`au-ctl-stop-sequence-zero`、`au-ctl-board-read-in-render`，`mut-after-r2.log`）。
- 第 3 題的實測：把 `au-ctl-board-read-in-render` 對留下來的測試（`gyo_audio_tests` 全部案例，一般組態）跑，存活（`q3-proof.log`、`q3-proof-README.txt`）。檢證者指出這條突變按構造任何行為測試都抓不到，所以只當輔助；主要根據是靜態走查（合併前核准頁第 1 節）。
- 完整清單（489aadcb，78 條）：77/78。存活的 `au-ctl-stress-reassign-every-block`（標明「只在並行時抓得到」）切分成時序偶發：同一個 commit 上單獨跑 3/3 killed、未改的程式上 2/2 killed（`stress-reassign-on-change.log`、`stress-reassign-on-base.log`）；那一次機器同時跑審查 Agent。原因歸環境（§14.1），不是本批的改動（它改的是 `PublishVoices` 的指派，本批沒碰）。
- 全量 `ctest -L "cpu|shader"`（9d1e0d19）：169/169（`full-ctest.log`）。
- 設計審查＋突變清單的敵對式檢查（Fable 5.2／high，唯讀）：blocker 0、should 7、nit 8，突變清單 3 處可存活；全部處理，對照在合併前核准頁第 5 節（`design-review.md`）。
- §14.4 檢證者（Opus 5.5／high，唯讀）：第 3 項 CONFIRMED，第 1、2、4、5 項 PARTLY（結論成立、措辭更正），照它改寫成合併前核准頁第 1 節（`q3-verifier.md`）。
- 主會話要求併進本批的三處文件（2026-11-22）：`docs/architecture/target/README.md:29` 把 audio 移出「還沒有 target 檔」並指向新頁；`rebuild-concept.zh-Hant.md` 的 audio core 列原文不改，表後加註（時間點的邊界已寫進 DI 3、預設起播時刻未解、執行緒綁定已拿掉）；`docs/architecture/plans/kernel/guards-inventory.zh-Hant.md` 的 board readers 列只更新路徑（`engine/modules/audio/src`），「留／已拿掉」等第 3 題的合併前核准再改。查會讀文件的測試：`git grep` `tests/`、`build/cmake/`、`.github/` 裡這三個檔名與 `docs` 的讀取，只命中註解、docstring 與突變清單的說明，沒有測試讀它們；照實再跑全量 `ctest -L "cpu|shader"` 169/169（`full-ctest-docs3.log`）。
- 證據：主 checkout `build/evidence/gyo2-audio-review/`（本機 Intel Mac，git 不追蹤）。

## 突變的基準（README 第 8 條）

- 新增 2 條：`au-review-handoff-rebound`（判定 2 是新的判定項目，第 3 條的下限；它抓的「實作退回綁定」沒有別的突變抓得到，退役的 4 條抓的是相反的誤）；`au-review-unload-counts-before-assert`（搬過去的計數判定原本沒有突變指到，設計審查 B-3）。數目在上限內（改的 DI 條文 6 × 5）。
- 退役 18 條：機能拿掉（第 7 條）與 gyo2 跑不起來；`au-sdl-late-stop-uncounted` 的 find／replace 和留下的 `au-ctl-late-stop-uncounted` 完全相同（第 1 條）。

## 範圍擴大（`v8-arch-gyo2-world-audio-withdrawn`）

主會話重查 `world_audio` 的前提（subagent），使用者在主會話說「照建議」：不另做 `world_audio`；作者的寫法、三處註解與預設值的坑併進本批（使用者同意擴大範圍）。做的：
- 新頁 [audio.zh-Hant.md](../architecture/target/v2/audio.zh-Hant.md) 第 5 節「把 World 接到混音器：作者的寫法」（5.1 tick 換成時刻的三種做法、5.2 提交口照 World 名字、5.3 一個 System 在 `Main`／`Last` 交、5.4 帶回的值不進判定、5.5 先送出再卸載）。片段用真的 API 名（`ProductSetup::Buffer`、`WorldSetup::ProductBuffer`／`Prepare`、`MessageWriter`／`MessageReader`、`ReplaceRequest::beforeSwap`），標明是示意。
- `PlayRequest::at` 的預設（紀元）：不改、不加守衛；註解寫明症狀，新測試「starts a play left without a time at once and counts it an hour late」釘住（block 的第一個 frame 起播、`playedLate == 1`、`maxLateness >= 59 min`）。
- `CreateSubmitter`、`ClipBank`、`clipSlotsPerSubmitter` 的註解。
- 宣告的「留給 `world_audio`」各處加更正註；`rebuild-concept` 的註改寫。

範圍擴大的設計審查（Fable 5.2／high，唯讀，`design-review-expansion.md`）：blocker 1、should 8、nit 10。blocker 是第 5.1 節「固定步寫 `Message`、`Main` 讀」在用官方回滾策略的 World 是啟動錯誤 RB-E8（`StateRollbackStrategy.cpp` 的 `CheckMessages`）；改成分有沒有回滾兩種寫法。其餘：(b) 的式子補累加器的餘數、clip 格不跟著沿用、建混音器的是誰交 SDL 那一批、依時序變的值一起列、標頭註解改回中性、測試改成恰好 1 h − 60 ms。處理對照在合併前核准頁第 4.2 節。全量 `ctest -L "cpu|shader"`：範圍擴大後 169/169（`full-ctest-expansion.log`），審查改完 169/169（`full-ctest-expansion-r2.log`）。

## 留給之後的

- audio 那一側的小批（asset 實作合併之後，或併進 SDL 那一批）：clip 的來源（宣告第 5 節的四個選項與寫判定要照的事）、音效 loader。預設起播時刻、提交口的數量是作者的寫法（新頁第 5 節）。（原寫「`world_audio` 那一批」，該批已撤掉。）
- 第 3 題的正式核准後：拿掉 `gyo_audio_board_readers` 與 `au-ctl-board-read-in-render`，DI 4 的守衛清單刪那一項。
- 「送進 World 外服務的時刻都是時間基準」要不要成為跨 Component 規則：主會話已排給圖書管理員。
- SDL 那一批：後端照新頁第 4 節接；`scheduleDelay` 的預設值重新推導；後端的突變照自己的測試重立。

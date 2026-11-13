# 架構 v8：v8-arch-audio-playhead-test-fix（播放頭輪詢測試的測試方法重訂）

日期：2026-11-13。Owner：架構（架構師會話開出的任務卡）；改動落在 audio 的測試與守衛文件（[audio.md](../architecture/audio.md) DI 4、[CONTROL](../architecture/plans/audio/CONTROL.md) `-playhead` 列）。
- 執行：本卡會話（Opus 5.5／high），工作樹 `.claude/worktrees/inspiring-herschel-4cc1de`，分支 `claude/arch-audio-playhead-test-fix`（自 origin/master `381c182d`）。
- 對抗式檢查與 §14.4 驗證：Fable／high，只讀，一次（使用者核准「核准含 Fable」）。
- 不改 `engine/audio` 的實作；不加重試；ctest TIMEOUT 只為新拆出的 stress 測試設定（見下）。

## 起因

L1 linux-x64 run 43365900431：TEST_CASE「Audio Mixer playheads stay consistent while another thread polls them」的 `CHECK(next > 0)` 失敗。讀取端可以在渲染端送出第一個音之前就做完 10^5 次輪詢，一個音都沒檢查就結束。

## 經過

1. 先修排程（讀取端等到有覆蓋才計數）。宣告的重複執行第 13 輪出現鏡像排程（渲染端先送完、讀取端什麼都沒看到），第一次停止。
2. 使用者判定：「這個是測試方法的問題。得改測試而不是單純的修。」擴大範圍重訂。重訂案（主 checkout 證據 `redesign-proposal.md`）經使用者核准。
3. 重訂後，宣告的 TSan 5 輪只有 4/5 通過（run 1 mode 1 在 20 s 內 overlap 99382）。照 §14.1 第二次停止。
4. 使用者：「先查一下。」調查結論（`investigation.md`）：
   - T2 是固定工作量（10^5 次輪詢 × 400 次讀取）對牆鐘 20 s 的守衛。TSan 下每次 `PlayheadOf` 約 250 ns（core 3～5 ns），一次輪詢約 131 µs（讀取 100、seen[] 複製 26、記錄 22），合計約 13 s，再加上本機每次執行之間約 ±25% 的起伏。
   - 渲染端爭用不是原因；整支執行和單獨執行沒有系統差異。
   - 「舊測試 TSan 下 5 s」的前提是錯的：原樣複製量測為 8.0～8.6 s，讀取 3.94×10^7 次。
5. 使用者選 (A)：TSan 建置下每輪守衛 60 s（最慢 19.4 s × 3），其餘 20 s；ctest TIMEOUT TSan 樹 200、其餘 90。
6. Fable 檢查找到 5 處，處置見下表。其中「TSan 下 T2 幾乎沒有同一音的序列比對」改變了設計前提，第三次停止。使用者選 (1)：寫清 T2 的職責，不改 T2 的設計。
7. 使用者核准 DI 4 守衛的變更（§14.4 1～5）。

## 做了什麼

- `tests/common/audio/PlayheadTests.cpp`：舊案換成兩案。
  - T1「…while voices are reused, block by block」：確定性、單執行緒。每區塊讀所有音；位置必須正好 64×區塊數（wrongFrame）；狀態必須在「未停止且未到片段尾」時正好是 Playing（wrongStatus，Fable 第 1 處後加）；不丟棄、每個未停止的音都讀到 Playing、最後都結束；加上舊守衛的三個條件（bad）。
  - T2「…under a polling thread (ThreadSanitizer stress)」：自己的 CTest `gyo_audio_playhead_stress`。三種排程（自由、讀取先、渲染先），各要在上限內達到 10^5 次與渲染重疊的輪詢與 10^3 次讀到 Playing。讀取端看最近 400 個音、新到舊；handle 放在 seq_cst 環，以 `writing` 計數跳過可能被重用的槽。上限 20 s，TSan 建置 60 s（編譯期 `__SANITIZE_THREAD__`／`__has_feature(thread_sanitizer)`）。輸出行印出達標時間、守衛值、sequence reads（只供參考）。職責：競爭與單次讀取；同一音的讀取順序由 T1 守，並行下靠 board 窮舉交錯測試與 review。
- `tests/common/audio/CMakeLists.txt`：`gyo_audio_tests` 排除 stress 案（TIMEOUT 60 不變）；`gyo_audio_playhead_stress` TIMEOUT 90，`CMAKE_CXX_FLAGS` 含 `-fsanitize=thread` 時 200。
- `mutations.json` 新增 10 個、`mutations-tsan.json` 新增 1 個。
- `audio.md` DI 4 守衛描述、`CONTROL.md` `-playhead` 列加註（使用者核准）。

## 件数の突き合わせ

一致：宣告驗收 core 100/100、TSan 5/5（0 警告）；突變 core 23/24＋TSan 1/1，存活 1 個（下表第 4 處）照實記錄。

## 受理判定（看結果前宣告）

| 判定 | 結果 |
|---|---|
| core：整支 100 次全過，stress 每行「bound 20 s」且達標 | 成立：100/100，達標 255～463 ms |
| TSan：整支 5 次全過、0 警告，每行「bound 60 s」且達標 | 成立：5/5，達標 14.6～16.1 s（約 3.7 倍餘裕），sequence reads 228～245 |
| 舊宣告的 TSan 5 輪（20 s） | 不成立 4/5（保留，見經過 3） |

## §14.4（DI 4 守衛換成 T1＋T2；使用者 2026-11-13 核准）

1. 守的性質：另一條執行緒在讀、聲部持續重用時，每個音的位置不超過片段、只增不減、結束後不再出現；加上 TSan 的競爭偵測。舊的沒有時間上限、沒有覆蓋條件，曾一個音都沒讀就通過。
2. 緩める面：檢查條件沒有放寬（同三個條件、最近 400 個音）。時間上限對舊守衛是新加的（締める）；60 s 是看過結果後改的門檻（§14.1，使用者核准 (A)）。並行下同一音的讀取順序新舊都沒有實質守住（舊：TSan 下讀到 Playing 約 470 次；新：sequence reads 約 230～300，mode 2 為 0），DI 4 照實寫出 T2 不守這一點。
3. 締める面：覆蓋條件、三種排程、T1 的精確幀數與狀態。突變：舊測試對同類突變 1/4，新的 23/24＋TSan 1/1。
4. 守住的地方：三個條件 → T1（各有突變證明）；單次讀取與競爭 → T2；並行下的讀取順序 → board 窮舉交錯測試與 review。CI 沒有 TSan job，TSan 部分只在本機。
5. 證據：見下。

## Fable 檢查（Fable／high，只讀）與處置

第一節（§14.2）找到 5 處：

| # | 內容 | 處置 |
|---|---|---|
| 1 | 提早回報 NotPlaying（Publish→Clear），兩案都抓不到 | T1 加 wrongStatus；`au-ctl-reuse-ends-early` 被殺 |
| 2 | bad 的「結束後又出現」分支無突變 | `au-ctl-reuse-ended-comes-back`，T1 殺 |
| 3 | bad 的「位置倒退」分支無突變 | `au-ctl-reuse-position-goes-back`，T1 殺 |
| 4 | 每區塊重新 Assign（只有並行看得到） | `au-ctl-stress-reassign-every-block` **存活**；屬並行下的讀取順序，靠 board 窮舉交錯測試與 review（§14.2 照實記錄） |
| 5 | T2 沒有 TSan 突變 | `au-ctl-stress-reader-reads-audio-state`（mutations-tsan.json），被殺 |

另：T2 空轉觀察（TSan 下每音最多讀一次）→ 第三次停止，使用者選 (1)。`written > n + kRing` 跳過與新到舊順序：檢查者推演為藏不住錯讀。

第二節（§14.4 驗證）：1、3、5 確認；2、4 部分確認，修正已併入上面的 §14.4（時間上限對舊守衛是締める面、60 s 屬 §14.1；並行下的序列性質不寫成「T2 同等守住」）。Fable 檢查之後的兩處改動（wrongStatus、T2 職責註解）沒有再送驗證：前者只締、後者只是註解。

## 突變

- 新增 core 10 個：`au-ctl-reuse-never-playing`、`-reuse-not-published`、`-reuse-not-cleared-on-end`、`-reuse-position-past-clip`、`-stress-never-playing`、`-stress-position-past-clip`、`-reuse-ends-early`、`-reuse-ended-comes-back`、`-reuse-position-goes-back`、`-stress-reassign-every-block`（存活）。TSan 1 個：`au-ctl-stress-reader-reads-audio-state`。
- 相關重跑（D184：新增、殺它的測試所在檔案被改）：core 24 個 23 killed、TSan 1/1。
- `au-ctl-board-read-in-render` 沒有重跑：它的命令寫死 `build/target/_build/core`，本工作樹沒有該目錄，基準失敗（`option-a/mutants-attempt-board-path.txt`）；它檢查原始碼讀取點，與本批改動無關。

## 給架構師

- T1 之前的「board 窮舉交錯測試」include 內部的 `PlayheadBoard.hpp`（既有，非本批新增）。本批 T1、T2 只用 `GYO::Audio` 的公開 API（不碰 SDL、不假設 Engine 或 plugin）。
- 工作量對照（一行）：T2 每次輪詢的讀取次數與舊測試相同（10^5×400），TSan 下慢 1.6 倍，原因已拆解（seen[] 複製與記錄），不是缺陷。
- 時間：第一份證據 15:42，到核准約 5 h 牆鐘（含等候使用者）；遠超過 2 h 時間框，已三次停止回報。

## 證據

主 checkout `build/evidence/audio-playhead-test-fix/`（git 管理外，本機 Intel macOS）：
- red.txt／red.diff、red2.txt／red2.diff（舊測試加覆蓋斷言、強制排程的確定性 Red）；mutants-old-test.txt；
- stop-report.md、stop-report-2.md、stop-report-3.md、redesign-proposal.md、investigation.md、probe/（探針原始碼與輸出）；
- option-a/（(A) 的宣告、驗收、mutants-2.txt、mutants-tsan.txt、doc-edits-draft.md）；
- final/（最終宣告 declaration.md、core-100.txt、tsan-5.txt、final.diff、section-14-4.md）。
- L1 三平台的達標時間：PR 的 CI 後補記。

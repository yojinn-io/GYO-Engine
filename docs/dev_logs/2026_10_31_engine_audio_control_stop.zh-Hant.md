# 2026-10-31 聲音代號與停止（`v8-engine-audio-control-stop`）

Owner：Engine（`GYO::Audio`）。計畫：[CONTROL](../architecture/plans/audio/CONTROL.md)「分批」。契約：[audio.md](../architecture/audio.md)。

## 為什麼做

資產工具 `audio_editor` 的預覽需要停止已送出的聲音（F1）。這是控制層重建的第一個功能批：
- 播放要能拿到一個代號；
- 停止要和播放用同一套時間換算，讓輸出保持決定性（決策 `-stop-timing`＝a）。

## 做了什麼

- **公開介面**（`Mixer.hpp`）：
  - `Submitter::Play` 改為回傳 `SoundHandle`（原本回傳 `bool`；`explicit operator bool` 讓既有的 `CHECK(Play(...))` 照常可用）。
  - 新增 `Stop(handle, at)`、`StopAll(at)`。
  - `MixerStats` 新增 `stopsSubmitted`、`stopsMatched`、`stopsUnmatched`、`stopsDroppedQueueFull`、`stopsLate`。
- **命令環**（`Mixer.cpp`）：
  - 每個提交端的 SPSC 環，元素從 `PlayRequest` 改成內部的 `Command`（播放、停止、全部停止）。
  - 入列共用 `Enqueue`，所以既有的佇列突變仍然各自唯一。
  - 停止的時間換算是獨立的一行，突變可以只打壞停止。
- **Handle**：
  - 格式是提交端索引加 1 放在最高 7 位，再加上該提交端的序號（從 1 開始，只在入列成功時前進）。
  - 序號由擁有者執行緒私有，放在 slot 裡，所以搬移 Submitter 後 handle 仍有效。
  - `Stop` 在生產者端 assert：前綴是自己的、序號不是 0、序號不超過已發出的最大值。
- **樣本域**（`VoiceMixer`）：
  - 只多了每個聲部的停止 frame 與不透明的 cookie（即 handle），以及 `Stop(cookie, mask, stopFrame)`。
  - handle 的知識留在 `Mixer`（計畫的 M7）。
  - 混音範圍改成 `[max(start, chunkStart), min(stopFrame, chunkEnd, start + clip))`，聲部在停止 frame 釋放。
- **停止的規則**（Invariant 10）：
  - 未開始（起點不早於本區塊第一個 frame）且停止點不晚於起點：立刻釋放，不發聲。
  - 已開始且停止點不晚於本區塊第一個 frame：立刻釋放；停止點早於第一個 frame 時計為遲到。
  - 其他情況：在停止 frame 結束。多次停止取最早的。
- **測試**：
  - 新增 `tests/common/audio/StopTests.cpp`，共 19 案，加進 `gyo_audio_tests`。
  - `gyo_audio_alloc_tests` 也走停止路徑（一個未命中的停止、一個遲到的全部停止）。
  - `tests/common/audio_sdl/OnsetRecord.cpp:102` 機械修改一行（`.Valid()`），判定不變。
- **突變**：
  - `mutations.json` 新增 20 個 `au-ctl-*`。
  - find 字串被改寫的既有突變同批更新：`au-core-chunk-edge-lost`、`au-core-voice-never-freed`、`au-sched-full-off-by-one`，語意不變。
- **文件**：
  - audio.md 的「What it is」、Contracts、即時上限、Invariants 3、6、7、8、10、11 改成已成立（停止與 handle 的部分）。
  - 擁有權的說法改成「第一個呼叫 `Play`、`Stop` 或 `StopAll` 的執行緒」。

## 驗證與經緯

- **Red**：先在 `Render` 的停止分支放 `continue`（停止入列但不生效）。以名稱篩選到的 15 案（14 個新案加 1 個既有案）中 10 案失敗。其餘 5 案只檢查生產者端（handle、佇列滿、assert），本來就不依賴停止生效。之後還原為實作，全部 Green。
- **本機結果**（macOS x86_64）：
  - core preset `ctest` 38／38。
  - test preset 109 案中有 3 案逾時：`object_fps_pvp.cpu`、`presentation_evidence`、`combat_gui_evidence_v7`。
    - 當時其他會話讓負載平均到 158。逐一重跑 3／3 通過（52 s、59 s、21 s）。
    - 判為環境（負載），不是實作的問題（AGENTS §14.1）。
  - 特性測試的雜湊沒變；不配置守衛通過。
- **TSan**（本機 `build/target/_build/audio-tsan`，`TSAN_OPTIONS=ignore_interceptors_accesses=0`）：
  - 審查前：`gyo_audio_tests` 跑 3 次，都沒有警告。
  - `mutations-tsan.json` 4／4 被殺。
  - CI 沒有 TSan job，這是範圍的限制。
- **突變**：
  - 第一輪（審查前）34／34 中有兩個問題：
    - `au-sched-full-off-by-one` 變成 stale：佇列滿的那一行改寫成 `return false;`。更新 find 字串。
    - `au-ctl-stopall-unscoped` 存活：測試的缺陷。環依索引順序取出，同一區塊裡 b 的播放在 a 的 StopAll 之後才排程，所以根本測不到「停到別人」。改成先 render 一塊，讓所有聲部都排程好，下一塊才取出 StopAll；64 提交端那一案也照此修改。之後被殺。
    - 執行上的失誤：第一次在 zsh 用未加引號的 `$ids` 傳 id，整串被當成一個參數，結果 0／0。改用 `xargs` 重跑。
  - 審查後（加了 8 個突變）重跑相關的 40 個（`au-core-*`、`au-sched-*`、`au-ctl-*`）：39／40，`au-ctl-late-stop-boundary` 存活。原因是突變目錄的 `cases`：doctest 的 `-tc` 以逗號分隔，案名「…first frame, on time」被拆成兩個篩選，結果一個案都沒跑。改成不含逗號的篩選後被殺，最終 40／40。
  - 審查後重跑 TSan：`gyo_audio_tests`（53 案）3 次都沒有警告，`mutations-tsan.json` 4／4；test preset 的 `audio` 相關 CTest 5／5。
- **計畫宣告的突變換掉一個**：CONTROL 列的 `au-ctl-late-stop-unclamped`（拿掉「夾到區塊開頭」）是等價突變。停止點早於本區塊時，迴圈一個 frame 都不會混；修正 D3 之後更是直接釋放，輸出與計數都不變。所以改成殺計數的 `au-ctl-late-stop-uncounted`，另加 `au-ctl-late-stop-boundary`（AGENTS §14.2）。

### xhigh 審查（`xhigh-reviewer`，xhigh，引擎繼承）

沒有 blocker。以下逐項處置。

- **T1（major）**：「已開始」的邊界（起點＝本區塊第一個 frame）沒有守衛。加「treats a late play clamped to the block start as not started」與突變 `au-ctl-stop-started-boundary`。
- **T2（major）**：「多次停止取最早」沒有守衛。加「keeps the earliest of several stops」與突變 `au-ctl-stop-latest-wins`。audio.md 的 Contracts 寫明「the earliest stop frame wins」。
  - 這一點我放在 Contracts，沒有放進 Invariant 10 的本文：它是把 Invariant 10 的 `min` 寫清楚，不改變它的意思。
- **T3**：停止點正好在本區塊第一個 frame 時，不能算遲到。加「frees a started sound stopped at the block's first frame, on time」與突變 `au-ctl-late-stop-boundary`。
- **T4**：停止點等於區塊結尾時要釋放聲部。加「frees a voice whose stop frame ends the block」與突變 `au-ctl-stop-free-at-block-end`。
- **T5**：Stop／StopAll 的擁有者檢查與 moved-from。加「Submitter's stops belong to the thread that owns it」、在 foreign 那一案加 moved-from，並加突變 `au-ctl-stop-owner-unchecked`、`au-ctl-stopall-owner-unchecked`。
- **T6**：不配置守衛原本沒有走停止路徑。改成第二個區塊後送出停止與全部停止，並加突變 `au-ctl-alloc-in-stop`。
- **T7**：換掉宣告突變的理由，記在本文（見上）。
- **D1**：序號 0 的 handle 會通過 assert。改成 assert `sequence != 0`，加測試與突變 `au-ctl-stop-sequence-zero`。
- **D2**：`Stats` 讀停止計數的順序與註解相反（只影響顯示）。改成先讀結局。
- **D3**：已開始、在本區塊第一個 frame 或更早停止的聲部，原本要等下一個 chunk 才釋放。這會讓同一區塊後面的播放被丟成 noVoice，也讓第二次停止被算成 matched。
  - 改成立刻釋放，輸出完全相同。
  - Contracts 寫明這個行為；T3 的測試同時檢查第二次停止是 unmatched。
- **D4**：Mixer.hpp 與 audio.md 的擁有權說法，改成「第一個呼叫 Play、Stop 或 StopAll 的執行緒」。
- **D5**：`lastSequence` 和環的唯讀欄位在同一條 cache line（偽共享）。移到生產者會寫的那一條。
- **nit**：
  - `stopFrame` 的預設值改成最大值。
  - `VoiceMixer` 的類別說明加上 Stop。
  - 測試在 `.front()` 之前先 `REQUIRE_FALSE(empty)`。
  - 測試補上 `#include <algorithm>`。
  - 序號溢位只在註解寫明（2^57 次播放實際上到不了）。
- **和核准文字的差異**（不放寬，只列出）：
  - Invariant 6、10、11 的守衛名稱改用實際的測試案名稱。
  - 核准文字說「擴充既有的併發收支測試」，實作是另加一案「balances plays and stops from concurrent producers」，舊案不動，所以舊判定沒有任何改變。
  - Invariant 7 的「Render allocates and frees nothing」守衛原本漏標，現在補標為已成立（由 baseline 批建立）。這批讓它也涵蓋停止路徑。
- **和計畫的差異**：CONTROL 的 `-stop` 範圍列了「結束陣列」，這批沒有做：現在沒有讀者，到 `-clips` 批（片段租約）才需要。

## 證據

- 本機：`build/target/_build/core`、`build/target/_build/test`、`build/target/_build/audio-tsan`。
- 突變：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/audio/scripts/mutations.json --only <id…>`；TSan 用 `--catalog …/mutations-tsan.json --build-dir build/target/_build/audio-tsan`。

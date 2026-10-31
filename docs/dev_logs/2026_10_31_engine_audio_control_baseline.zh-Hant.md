# 2026-10-31 控制層重建前的基準（`v8-engine-audio-control-baseline`）

Owner：Engine（`GYO::Audio`）。計畫：[CONTROL](../architecture/plans/audio/CONTROL.md)「分批」。

## 為什麼做

控制層重建會改寫 `Mixer::Render` 的命令路徑。動手之前，先用測試固定兩件事：
- 一次性播放的消費端（PvP 的用法）會看到的輸出。
- 音效執行緒不配置記憶體（Design Invariant 7）。

之後每一批都必須讓這兩個測試保持通過。

## 做了什麼

- **`tests/common/audio/CharacterizationTests.cpp`**（加進 `gyo_audio_tests`）：「Audio Mixer renders a recorded one-shot sequence bit-identically」。
  - 只用公開的 `Mixer`：3 個片段（單聲道與立體聲，以 xorshift32 產生）、2 個提交端、預設 60 ms 的 delay、6 個聲部、佇列容量 8。
  - 60 個區塊，長度輪流用 480、441、1024、7、256、300 frame；錨點比名目快 0.03%，另加抖動。
  - 過程中包含準時、遲到、佇列滿、聲部滿、飽和，以及第 30 個區塊的主音量變更。測試先確認每種結局都至少出現一次，再對輸出樣本與 `MixerStats` 取 FNV-1a 雜湊。
  - 雜湊值在現有程式上錄下（特性測試，不是 Red）：`12184034990701221815`。註解寫明：之後雜湊不同就停下回報，不重錄。
  - 錄製時調整過一次：第一次錄的序列沒有出現佇列滿（每 10 個區塊一次的 12 個播放分給兩個提交端，各 6 個，小於容量 8）。改成 20 個後才錄下雜湊。
- **`tests/common/audio/RenderAllocTests.cpp`**（新執行檔 `gyo_audio_alloc_tests`）：
  - 替換全域的 `operator new`／`new[]`／`delete`／`delete[]`（含 sized 版本），以 thread_local 旗標只計 `Render` 期間的配置與釋放。替換會影響整個程式，所以放在獨立的執行檔。
  - 「Audio allocation guard counts what the thread allocates and frees」：示範守衛數得到配置，作為檢出力的證據。
  - 「Audio Mixer Render allocates and frees nothing」：先送出準時、遲到、超過聲部數、會飽和的播放，再 `Render` 8 個區塊，讓聲部也走到結束；要求配置與釋放都是 0。
- `mutations.json` 新增兩個突變：
  - `au-ctl-golden-gain-rounding`：混音的四捨五入改成截斷。
  - `au-ctl-alloc-in-render`：`Render` 內明確呼叫 `::operator new`／`delete`。
- `engine/` 沒有任何改動。

## 驗證與經緯

- macOS x86_64（Intel）、core preset：`ctest` 38／38 通過（新增 `gyo_audio_alloc_tests` 一個 CTest，`gyo_audio_tests` 多一個案例）。
- 突變 2／2 被殺，各自以預期的訊息失敗。
- `au-ctl-alloc-in-render` 的第一版寫法是在 `Render` 內放一個不用的 `std::vector<int> scratch(1)`，結果存活。原因：最佳化建置會省略沒有用到的配置（C++14 起允許省略 new 運算式），所以這個突變其實沒有配置任何記憶體，不是守衛的漏洞。改成明確呼叫 `::operator new(16)`（這種呼叫不允許省略）之後就被殺。這次改的是突變的寫法，測試的判定沒有變。
- **守衛看不到的範圍**：
  - 對齊版的 `operator new(size, align_val_t)` 沒有替換，所以 `Render` 裡過度對齊的配置數不到。
  - 經 `malloc` 直接配置的也數不到。
  - 目前 `Render` 兩者都沒有用到；這兩點靠 review 守住。
- 雜湊只在 macOS x86_64 上錄製。運算全是整數，增益的換算用 `std::lround`（IEEE double），預期各平台相同，但要等 PR 線的四平台 CI 才算確認。CI 上若不同，照註解停下回報。

## 證據

- 本機：`build/target/_build/core`、`ctest --test-dir build/target/_build/core`。
- 突變：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/audio/scripts/mutations.json --only au-ctl-golden-gain-rounding au-ctl-alloc-in-render`。
- 證據檔案：這批的 CTest 與突變都在前景執行，輸出沒有保存（數值記在本文）。

# 第03批修正與已知問題

更新：2026-10-02。Owner：`object_fps_pvp`。範圍：v5第03批啟動相位對齊（方案A1）及其專用驗收器。
**第03批未結案**：計次的原生GUI可見延遲短測與25案真網路矩陣依使用者決定（MVP技術驗證）暫緩；
第04批暫停。A1本體經PR #2合併為`ff11ee3`；本目錄所述守門與驗收器修改經PR #3
（`claude/pvp-v5-start-phase-guard`）合併為`9cd7f26`。

## 用法

- 一個問題一份文件，固定段落：問題成因／影響／如何復現／解決方案／驗證，必要時加殘留風險與後續。
- 狀態只用：已解決／未解決（處理中）／未解決（暫緩）／已知問題。只有「處理中」代表已排程；
  暫緩與已知問題不代表已排程或承諾處理。
- 周邊文件（[v5總覽](../README.md)、[交接](../HANDOFF.md)、[第03批計畫](../03-v5-gameplay-and-delivery.md)、
  [v5契約](../../../protocol-v5.zh-Hant.md)）只留簡述與連結；細節以本目錄為準。
- 數字未標「實機」者皆為CPU模擬。不計次的冒煙只說明現象，不作為驗收證據。
- 時間順序、使用者決定與驗證總數見
  [守門dev_log](../../../../dev_logs/2026_10_02_pvp_v5_start_phase_guard.zh-Hant.md)；
  A1原始實作見[A1 dev_log](../../../../dev_logs/2026_10_01_pvp_v5_start_phase.zh-Hant.md)（含2026-10-02更正）。

## 索引

| 編號 | 問題 | 狀態 | 摘要（一句） | 文件 |
|---|---|---|---|---|
| 01 | A1 在低幀率（30／40 FPS）造成 Held 與重設 | 已解決 | 一幀含多個固定步時對齊量化成整幀，較舊命令受worker送出相位漂移影響；最新32幀平均>1.1 Tick（約54.5 FPS）時改為不對齊 | [01](01-a1-low-fps-regression.md) |
| 02 | stall reseed 使 A1 在整個 epoch 失效（開局卡頓） | 已解決 | 每epoch只量一次，觸發stall reseed的卡頓會取消其餘時間的對齊；本機三次GUI冒煙的移動方都被視窗啟動卡頓取消。與08同根，改以持續相位追蹤一起處理 | [02](02-a1-cancelled-by-stall-reseed.md) |
| 03 | A1 對偶發掉幀敏感（Starvation 重設；55–58 FPS vsync） | 已解決 | 切點以上A1把餘裕壓到約4ms，偶發長幀造成starvation重設；2026-10-02找到根因：worker送出期限以實際送出時刻重設，到達時間呈一Tick振幅的鋸齒，吃掉安全餘裕 | [03](03-a1-missed-frame-starvation.md) |
| 04 | macOS 上 Python 與 C++ 單調時鐘不一致 | 已解決 | `time.monotonic_ns`與Apple libc++ `steady_clock`不同域（實機差8176秒），會使25案矩陣中16案失敗或空等；跨行程時間戳記改用`run_network.steady_clock_ns` | [04](04-macos-clock-domain.md) |
| 05 | 驗收 runner 同時啟動 Match 與 Gateway 的競態 | 已解決 | Gateway只撥號Match一次，同時啟動時3次冒煙2次失敗；改為等Match ready日誌行再啟Gateway | [05](05-match-gateway-startup-race.md) |
| 06 | GUI 延遲測試的視窗干擾、視窗配置與補跑規則 | 已解決（規則已實作；計次輪未執行） | macOS兩視窗對角配置並記錄視窗事件；受干擾輪標無效並保留，只依視窗證據至多補跑一次 | [06](06-gui-window-interference-and-rerun-rule.md) |
| 07 | 啟動相位的觀測與記錄（w、調整量、撤回、取消） | 已解決 | 逐玩家／epoch／life記錄w、調整量、撤回、取消與量測窗口內的對齊秒數，依產品回報的`StartPhaseSkip`判讀 | [07](07-start-phase-diagnostics.md) |
| 08 | Client／Host 時鐘漂移使 A1 對齊隨時間失準（長局） | 已解決 | A1量一次後不再更新，兩端ppm級速率差使相位線性偏離；模擬中Client快20ppm、RTT 20時延遲P50約第10分鐘超過50ms且無重設修正；本機共用時鐘測不到 | [08](08-a1-clock-drift.md) |
| 09 | 涵蓋空檔後 Client 卡在「剛好遲到」（連續 Held 直到 Starvation 重設） | 已解決 | 長幀丟時間後Client落後，RTT使stall reseed永遠偵測不到；每Tick Held約0.5秒後Starvation重設；模擬中RTT 20時A1約94%的53ms卡頓以重設收場 | [09](09-covered-gap-stuck-late.md) |
| 10 | 高延遲／低幀率玩家的伺服器保護（連線品質移出） | 已解決 | 使用者要求；參考年齡＞160ms、被替代＞5%或有移動重設的10秒窗口連續3個即移出；須與08的修正同時上線，否則誤踢12% | [10](10-connection-quality-eviction.md) |

## 第03批狀態

- 已完成：A1（`ff11ee3`）、低幀率守門（01）、驗收器修正（04–07）及其自動測試。
- 2026-10-02已實作並通過CPU驗證（分支`claude/pvp-v5-phase-tracking`，PR送審中）：02、03、08、09以「輸入worker token bucket＋閉環的持續相位追蹤」處理，10為連線品質移出；01的低幀率守門與07的A1紀錄隨之由持續追蹤取代。實機GUI冒煙未執行。
- 未完成：計次GUI可見延遲短測、25案矩陣、第03批結案；第04批未啟動。
- 若恢復結案，先讀02與06：在閒置機器上一次一輪，事前宣告總輪數與補跑規則，
  記錄每輪w、調整量、撤回與取消；不改門檻、lead、插值或60Hz，不以重跑挑分數。

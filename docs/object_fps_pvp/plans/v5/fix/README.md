# 第03批修正與已知問題

更新：2026-10-02。Owner：`object_fps_pvp`。範圍：v5第03批啟動相位對齊（方案A1）及其專用驗收器。
**第03批未結案**：計次的原生GUI可見延遲短測與25案真網路矩陣依使用者決定（MVP技術驗證）暫緩；
第04批暫停。A1本體經PR #2合併為`ff11ee3`；本目錄所述守門與驗收器修改尚未commit／PR，
提交前的工作分支為`claude/pvp-v5-start-phase-guard`。

## 用法

- 一個問題一份文件，固定段落：問題成因／影響／如何復現／解決方案／驗證，必要時加殘留風險與後續。
- 狀態只用：已解決／未解決（暫緩）／已知問題。未解決與已知問題不代表已排程或承諾處理。
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
| 02 | stall reseed 使 A1 在整個 epoch 失效（開局卡頓） | 未解決（暫緩） | 每epoch只量一次，觸發stall reseed的卡頓會取消其餘時間的對齊；本機三次GUI冒煙的移動方都被視窗啟動卡頓取消，方案只評估未實作 | [02](02-a1-cancelled-by-stall-reseed.md) |
| 03 | A1 對偶發掉幀敏感（Starvation 重設；55–58 FPS vsync） | 已知問題 | 切點以上A1把餘裕壓到約4ms，偶發長幀造成starvation重設；55–58 FPS規律掉refresh時是否保留對齊取決於啟動時機 | [03](03-a1-missed-frame-starvation.md) |
| 04 | macOS 上 Python 與 C++ 單調時鐘不一致 | 已解決 | `time.monotonic_ns`與Apple libc++ `steady_clock`不同域（實機差8176秒），會使25案矩陣中16案失敗或空等；跨行程時間戳記改用`run_network.steady_clock_ns` | [04](04-macos-clock-domain.md) |
| 05 | 驗收 runner 同時啟動 Match 與 Gateway 的競態 | 已解決 | Gateway只撥號Match一次，同時啟動時3次冒煙2次失敗；改為等Match ready日誌行再啟Gateway | [05](05-match-gateway-startup-race.md) |
| 06 | GUI 延遲測試的視窗干擾、視窗配置與補跑規則 | 已解決（規則已實作；計次輪未執行） | macOS兩視窗對角配置並記錄視窗事件；受干擾輪標無效並保留，只依視窗證據至多補跑一次 | [06](06-gui-window-interference-and-rerun-rule.md) |
| 07 | 啟動相位的觀測與記錄（w、調整量、撤回、取消） | 已解決 | 逐玩家／epoch／life記錄w、調整量、撤回、取消與量測窗口內的對齊秒數，依產品回報的`StartPhaseSkip`判讀 | [07](07-start-phase-diagnostics.md) |

## 第03批狀態

- 已完成：A1（`ff11ee3`）、低幀率守門（01）、驗收器修正（04–07）及其自動測試。
- 未完成：計次GUI可見延遲短測、25案矩陣、第03批結案；第04批未啟動。
- 若恢復結案，先讀02與06：在閒置機器上一次一輪，事前宣告總輪數與補跑規則，
  記錄每輪w、調整量、撤回與取消；不改門檻、lead、插值或60Hz，不以重跑挑分數。

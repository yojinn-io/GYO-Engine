# PvP v5 第03批：A1低幀率守門與暫緩結案

日期：2026-10-01～10-02。Owner：object_fps_pvp。
狀態：守門與驗收器修正完成並通過自動測試，尚未commit／PR（工作分支`claude/pvp-v5-start-phase-guard`，自`ff11ee3`）；
計次結案驗收依使用者決定暫緩，**第03批未結案**，第04批暫停。
每個問題的成因／影響／復現／解決方案見[修正與已知問題](../object_fps_pvp/plans/v5/fix/README.md)，本文只記經過。
數字未標「實機」者皆為CPU模擬。

## 經過

1. 10-01：PR #2（A1，`d74830c`）在本機結案前由使用者合併為`ff11ee3`。本機Intel Mac以`d74830c`建置，
   CPU回歸119 cases／1,398,491 assertions與presentation 12 cases通過。
2. 驗收器先遇到macOS上Python與C++單調時鐘不同域（實機差8176秒），改為共用`steady_clock_ns`。（[fix/04](../object_fps_pvp/plans/v5/fix/04-macos-clock-domain.md)）
3. 改用產品式worker／幀時鐘漂移／抖動模擬後，發現A1使30 FPS對齊時Held 10–14%、40 FPS 1.7%，
   原相位矩陣的worker相位鎖定而漏掉。（[fix/01](../object_fps_pvp/plans/v5/fix/01-a1-low-fps-regression.md)）
4. 使用者決定「低於60 FPS時不對齊」，並確認切點為約54.5 FPS（1.1 tick）：本機GUI中位幀間隔
   17.18–17.6ms（約57–58 FPS），字面上的60會在驗收時關掉A1。
5. 守門三版：v1（8幀IQM＋鎖存）漏掉vsync量化的48–54 FPS、啟動卡頓鎖住整個epoch且在55 FPS頻繁切換
   → v2（32幀夾值平均、可逆、遲滯與駐留）→ v3（`CancelledByReseed`診斷、撤回時夾在追趕容量內、補測試）。
6. 三次report-only GUI冒煙（各一輪16秒）。第2次前兩次啟動因Match／Gateway競態失敗，加`wait_for_match_ready`
   （[fix/05](../object_fps_pvp/plans/v5/fix/05-match-gateway-startup-race.md)）；冒煙1紀錄為shift_armed，
   實際已被取消，紀錄器改為跟隨每次狀態變更（[fix/07](../object_fps_pvp/plans/v5/fix/07-start-phase-diagnostics.md)）。
7. 三次冒煙的移動方都在進入世界後0.1–0.7秒被macOS視窗啟動卡頓觸發的stall reseed取消對齊。
   評估三個方案（驗收暖機、出生後保持、O2W重新量測），只評估未實作。（[fix/02](../object_fps_pvp/plans/v5/fix/02-a1-cancelled-by-stall-reseed.md)）
8. 使用者批准計次GUI延遲輪的視窗干擾判定與補跑規則；macOS兩視窗改為對角配置。（[fix/06](../object_fps_pvp/plans/v5/fix/06-gui-window-interference-and-rerun-rule.md)）
9. 使用者決定切點以上的偶發掉幀重設為已知問題，暫不修。（[fix/03](../object_fps_pvp/plans/v5/fix/03-a1-missed-frame-starvation.md)）
10. 10-02：使用者決定以MVP／PvP技術驗證收尾：不做驗收暖機與計次跑次，先把經驗整理成文件，接著處理其他問題。

## 使用者決定

- 低於約54.5 FPS（32幀夾值平均>1.1 tick）不做啟動相位對齊。
- 切點以上A1對偶發掉幀敏感列為已知問題，不在本次修正。
- 計次GUI延遲輪：受視窗干擾或視窗證據缺失／無效的輪標為無效並保留；是否補跑只依視窗證據，
  每輪至多補跑一次，再受干擾則停止交使用者決定；總輪數與規則事前宣告。
- 第03批結案驗收（計次GUI可見延遲短測＋25案矩陣）暫緩；第04批暫停，需第03批結案、計畫複審與明確「開始」。
- 後續候選（未排程）：CS式開局／回合準備期（全員凍結、無敵、倒數），需另立計畫。

## 驗證

- C++ `gyo_object_fps_pvp_tests`：133 cases／1,418,939 assertions（約7.9秒，CTest上限30秒）；
  presentation 12 cases／4,189 assertions。突變檢查（8幀IQM、鎖存、無夾值、無遲滯、無駐留、恢復1.03、16幀窗口）皆被測試抓到。
- CTest `-L pvp` 16／16（新增`start_phase_record`、`timing_runner`、`steady_clock`、`service_startup`）；
  驗收器Python 165項；Go unit／race通過。
- 不計次、不作為驗收證據：三次report-only GUI冒煙皆PASS，實機可見P50 47.4／38.3／46.5ms，
  移動方全程未對齊；矩陣單案`upstream-250ms`冒煙PASS，Python／C++故障時戳相差在0.4ms內。
- 未執行：計次GUI可見延遲短測、25案矩陣、長測。

## Architecture Delta

- 需求：A1在低幀率退化（使用者批准切點）；驗收器在macOS無法正確量測與判讀。
- 變更只在object_fps_pvp：`Movement.hpp`的產品內常數（`MovementStartPhaseMaximumFrameSeconds`、
  `MovementStartPhaseRestoreFrameSeconds`）；`LocalPlayerPrediction`的守門與Client本機診斷`StartPhaseSkip`；
  `MovementRecoveryTests.cpp`模擬的`ClockOptions`；驗收紀錄器與測試（`start_phase_record.hpp`、`start_phase_evidence.py`、
  `StartPhaseRecordTests.cpp`、`test_steady_clock.py`、`test_service_startup.py`）；`run_network.py`的
  `steady_clock_ns`／`wait_for_match_ready`；新文件目錄`docs/object_fps_pvp/plans/v5/fix/`。
- 未改Engine、公共Gateway、proto／wire或Match；依賴方向與Ownership不變。
- Code smell：撤回時的追趕容量夾限重算了`FixedTickRuntime`的catch-up算式（`CatchUpSteps`＝5），
  日後可考慮由Engine提供存取器。

## 經驗沉澱

- worker與幀時鐘完全相位鎖定的CPU模擬會藏住相位對齊的錯誤；要模擬產品worker（喚醒晚到即overshoot、以送出時刻重設期限）、
  幀時鐘漂移、節拍抖動、vsync量化與啟動卡頓。
- 截尾估計（如IQM）正好會濾掉vsync顯示掉的refresh；改用夾值平均加遲滯與駐留。
- 診斷必須跟隨每次狀態變更（撤回／恢復／取消），否則證據會與實際相反。
- macOS跨行程時間戳記：Python monotonic不等於C++ `steady_clock`。
- 驗收器的行程啟動順序要明確（Gateway只撥號一次）。
- 在閒置機器上一次只跑一輪GUI；記錄幀間隔統計與重設原因，失敗才能歸因。
- 啟動即加入的GUI probe量到的是啟動雜訊，不是穩定遊玩。

## 證據（git忽略，只在本機）

- `build/target/_build/test/logs/pvp-v5-batch03-local-20261001-221441/`、`-integration-20261001-234746/`、
  `-integration2-20261002-004224/`、`-integration3-20261002-015053/`（前綴同為`pvp-v5-batch03`）。
- 彙整：`build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/`（冒煙、重分析、模擬輸出、patch、方案原型）。
  `build/`日後預計移出工作區，須保留這些目錄。

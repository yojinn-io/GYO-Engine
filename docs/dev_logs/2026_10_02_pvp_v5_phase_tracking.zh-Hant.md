# PvP v5 第03批：持續相位追蹤、token bucket worker與連線品質移出

日期：2026-10-02。Owner：object_fps_pvp。
狀態：實作與CPU驗證完成，第03批結案驗收通過；經分支`claude/pvp-v5-phase-tracking`（worktree `GYO-Engine-v5`，自`df195fa`，合併master `0bd5363`）提交PR #11送審（三平台CI通過），**尚未合併**；
實機report-only GUI冒煙1輪PASS（不計次）；第03批結案驗收通過（計次GUI可見延遲短測3輪有效輪皆通過（可見P50 37.3／36.6／35.9ms、P95 38.2／40.8／37.3ms）；25案真網路矩陣25／25通過（1075／1075動作）），**第03批已結案**，第04批暫停。
每個問題的成因／影響／復現／解決方案見[修正與已知問題](../object_fps_pvp/plans/v5/fix/README.md)，本文只記經過。
數字未標「實機」者皆為CPU模擬。

## 經過

1. 文件同步：v5文件仍寫守門與驗收器「尚未commit」，實際已由PR #3合併為`9cd7f26`，修正README、HANDOFF與fix/01–07。
   為避開另一個正在處理CI的會話，另建worktree `GYO-Engine-v5`。
2. 使用者要求處理fix/02。整理O2W＋O2P契約草案送審，使用者改為先討論根因，指出「量一次就用到底」的開環設計
   在CS式15分鐘對局中誤差只會變大。漂移模擬證實：Client快20ppm、RTT 20時A1延遲P50約第10分鐘超過50ms且無重設修正；
   本機三角色共用時鐘，結構上測不到（[fix/08](../object_fps_pvp/plans/v5/fix/08-a1-clock-drift.md)）。
3. 使用者決定不做兩台實機的漂移實測（邏輯缺陷不因實測數值而消失），直接修正。以模擬器覆蓋層做閉環追蹤原型：
   最小值統計追著到達時間的鋸齒跑，追查後發現輸入worker的送出期限以實際送出時刻重設，送出相位相對畫面幀繞圈，
   到達時間有一Tick振幅的鋸齒——這也是[fix/03](../object_fps_pvp/plans/v5/fix/03-a1-missed-frame-starvation.md)的根因；
   中位數延遲多約7ms；P90對準立即送出的命令，等同A1語意。
4. 健全性掃描：低幀率與掉包下Starvation重設暴增（Host保險絲遇上剛好準時的到達）。加晚端下限有延遲代價；
   改worker為立即送出後問題消失，但52 FPS時每秒98.6包，加動作30Hz會撞上Gateway每秒120包上限，改為60/s、容量2的token bucket。
5. 卡頓風暴中發現長幀丟時間後Client卡在「剛好遲到」、stall reseed因RTT永遠偵測不到
   （[fix/09](../object_fps_pvp/plans/v5/fix/09-covered-gap-stuck-late.md)），加入連續2個負值樣本立即修正。
6. 使用者在無人值守期間要求繼續；完成全套比較（19情境×3漂移×3 RTT×18相位×15分鐘）並寫成fix/08提案。
7. 使用者要求伺服器保護（高ping、低幀率直接移出）並交由本會話訂閾值。以模擬找出設計邊界：RTT約150ms塞滿12命令窗口、
   低於30 FPS幀長於2Tick lead；同一規則套在現行A1上會誤踢約12%正常局，因此必須與追蹤同時上線
   （[fix/10](../object_fps_pvp/plans/v5/fix/10-connection-quality-eviction.md)）。
8. 使用者核准fix/08提案（四項契約變更）、移出閾值與HUD警告；依分批實作（Host／wire、Client追蹤、worker、移出、驗收工具）。
   Python分析器測試的機械性改寫交由子代理完成。
9. 驗證中兩處修正：CombatHost測例要求每次發布只讀一次時鐘，改為每次Advance至多讀一次並與發布參考共用；
   寫測例時一度改成「slew期間不收樣本」，以產品碼重跑模擬發現卡頓重設退步，改回原型語意（見fix/08「實作」）。
10. 依建議順序commit並開PR #11（合併master `0bd5363`，無衝突），三平台CI通過。準備冒煙時發現另一會話殘留12個
    CPU壓力用忙迴圈佔滿CPU約78分鐘；本會話無權結束，由使用者結束後，在閒置機器上跑一輪report-only GUI冒煙。
11. 使用者恢復第03批結案驗收，通過後才合併PR。凍結來源`2cecd3a`與產物／分析器指紋，事前宣告3輪計次GUI＋1次矩陣與補跑規則。
    第3輪因量測中OS焦點變化無效，依fix/06補跑一次。第一次矩陣在第12案因工具會話結束而中斷：
    先在宣告追加補充條款，再核對指紋、於新目錄重跑完整25案，原目錄保留不計。

## 使用者決定

- 不採O2W草案，先討論根因；不做兩台實機漂移實測，直接修正。
- 核准fix/08提案：持續相位追蹤取代A1與低幀率守門、輸入worker token bucket、PlayerState欄位替換、刪除StartPhaseSkip。
- 要求並核准連線品質移出：10秒窗口、參考年齡>160ms／被替代>5%／移動重設任一即不合格、連續3窗口移出；加HUD警告。
- 依建議順序：先commit／PR讓CI跑三平台，CI期間跑report-only GUI冒煙，再決定是否恢復第03批結案驗收。
- 恢復第03批結案驗收；驗收通過後合併PR #11。

## 驗證

- C++：`gyo_object_fps_pvp_tests` 133 cases／9,020,866 assertions；紀錄器6 cases；完整建置（含Client、GUI與各驗收probe）。
- CTest `-L pvp` 16／16（含worker、wire、presentation、Python分析器）。全標籤只有`cmake.ProjectComposition`失敗：
  本機Darwin/x86_64不是封裝平台（`GyoPackaging.cmake`只支援Windows x64、Linux x64、macOS ARM64），與本次修改無關。
  `build.ci`以使用者shell的PATH（`~/.zshrc`加入cmake）執行通過。
  合併master `0bd5363`（新增macos-x64封裝平台與Go service測試）後重建，CTest全標籤40／40通過，Go `./...`的test、race、vet通過。
- Go：產品Gateway unit、race、vet通過；`services/gyo_gateway`未修改。
- Python：`test_run_timing.py` 18、`test_presentation_evidence.py` 36、`test_gameplay_evidence.py` 20。
- 模擬：以產品碼重跑171組設定，除卡頓風暴（產品約90秒依fix/10移出）外與核准原型逐位元相同；重設總數173（原型181）。
  與現行A1相比沒有任何一格Held多於0.05個百分點或重設多於2次。
- 實機report-only GUI冒煙（機器閒置，一輪16秒、20事件、60 FPS；不計次、不作為驗收證據）：PASS，
  可見P50／P95 36.754／38.811ms（Actual 28.831／36.448ms），20／20配對，視窗無干擾。兩方在量測開始前1.8–2.1秒決定相位，
  量測期間全程tracking、0次修正、0次移動重設；create方啟動時3次stall reseed都在量測前重新決定（fix/02的情境）。
  對照A1＋守門的三次冒煙：P50 47.4／38.3／46.5ms，移動方全程未對齊。只是一輪，不是統計證明。
- 第03批結案驗收（來源`2cecd3a`，指紋見證據目錄`fingerprints.json`，宣告見`declaration.md`）：
  | 輪 | 狀態 | 可見P50／P95 ms | Actual P50／P95 ms | 配對 | 量測中相位 |
  |---|---|---|---|---|---|
  | 1 | passed | 37.263／38.212 | 29.870／37.557 | 20／20 | 兩方tracking，0修正 |
  | 2 | passed | 36.561／40.826 | 29.266／36.658 | 20／20 | 兩方tracking，0修正 |
  | 3 | invalid_window_disturbed（門檻failed：配對率0.95） | 36.853／39.214 | 29.261／37.299 | 19／20 | 約122ms卡頓後重新取得 |
  | 3補跑 | passed | 35.893／37.279 | 29.273／36.660 | 20／20 | 兩方tracking，0修正 |
  有效輪量測期間Host移動重設皆為0、幀間隔中位約17.3ms。矩陣`matrix-run-2`：25／25、1075／1075動作，
  新操作恢復最慢1.110秒、穩定移動起點最慢0.293秒（門檻1.5／0.25秒維持）；`matrix`（中斷，前11案通過）不計。
- 未執行：長測、第04批。

## Architecture Delta

見[v5契約](../object_fps_pvp/protocol-v5.zh-Hant.md)§7第二段。摘要：只動`object_fps_pvp`的Client、Match、產品Go Gateway與專用驗收；
Engine與`services/gyo_gateway`不改；依賴方向不變（移出由Match決定，經既有IPC通知Gateway）；新增Match→Gateway的`PlayerEvicted`、
PlayerInput的`observed_authority_tick`，PlayerState以slack樣本取代`epoch_start_wait_us`；刪除A1、HostLate、低幀率守門與`StartPhaseSkip`。

## 經驗沉澱

- 三角色同機共用時鐘的測試，結構上看不到跨機器的時鐘漂移；開環量測要用模擬檢驗長時間行為。
- 延遲由Client的固定步相位決定，worker送出排程只影響安全餘裕；把兩者分開看才找得到fix/03的根因。
- 伺服器保護的閾值以設計本身的邊界（12命令窗口、2Tick lead）訂定，並用同一模擬器驗證誤踢與崩潰區。
- 實作中對原型的「小改進」也要用同一模擬器以產品碼回歸，否則會在個別情境悄悄退步。

## 證據（git忽略，只在本機）

- `build/target/_build/test/logs/pvp-v5-drift-sim-20261002/`：漂移現象（舊A1）。
- `build/target/_build/test/logs/pvp-v5-ct-prototype-20261002/`：`ct/`原型覆蓋層；`drift/`模擬器（`Sim.cpp`、`SimProduct.cpp`）、
  `build.py`、`sweep.py`、比較腳本與全部輸出（`out_final_bucket.txt`原型、`out_product_final.txt`產品、`out_kick_*.txt`移出閾值）。
- `build/target/_build/test/logs/pvp-v5-ct-gui-smoke-20261002-192738/`：report-only GUI冒煙（summary與`round-1`原始紀錄）。
- `build/target/_build/test/logs/pvp-v5-batch03-closure-20261002/`：結案驗收（`declaration.md`、`fingerprints.json`、`gui-round-1`～`3`、`gui-round-3-rerun`、`matrix`（中斷）、`matrix-run-2`）。

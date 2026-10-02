# PvP v5 交接

更新：2026-10-02。**第01–03批已完成（第03批2026-10-02結案驗收通過）。**
第04批暫停（需計畫複審及明確啟動），第05批未開始；未執行長測、未升格v5穩定基線。

第03批待結案守門：乾淨可見延遲短測曾有P50 **51.125ms >50ms**（啟動相位）。2026-10-01經使用者
批准實作方案A1啟動相位對齊（PR #2，合併為`ff11ee3`）；2026-10-02補上低幀率守門與驗收器修正（PR #3，合併為`9cd7f26`）。
其後一度依使用者決定以MVP技術驗證收尾、暫緩結案驗收。門檻、lead、插值與60Hz始終不變。各問題見[修正與已知問題](fix/README.md)。
2026-10-02：A1「每epoch量一次」在長局會因時鐘漂移失準（[fix/08](fix/08-a1-clock-drift.md)），與reseed取消（fix/02）
及fix/03、fix/09一起處理。使用者核准後已實作「輸入worker 60/s token bucket＋閉環的持續相位追蹤」與連線品質移出
（[fix/10](fix/10-connection-quality-eviction.md)），CPU驗證通過，經分支`claude/pvp-v5-phase-tracking`（PR #11，合併為`bec86b7`）；
A1、HostLate、低幀率守門與`StartPhaseSkip`已刪除。實機report-only GUI冒煙1輪PASS（可見P50／P95 36.8／38.8ms；不計次、不作為驗收證據），兩方量測全程tracking。
**結案驗收（2026-10-02，來源`2cecd3a`）：計次GUI可見延遲短測3輪有效輪皆通過（可見P50 37.3／36.6／35.9ms、P95 38.2／40.8／37.3ms）；25案真網路矩陣25／25通過（1075／1075動作）；第03批結案。**經過與驗證見
[追蹤dev_log](../../../dev_logs/2026_10_02_pvp_v5_phase_tracking.zh-Hant.md)；整體思考鏈見[延遲整改回顧](LATENCY_CASE_STUDY.md)。

## 閱讀入口

1. [進度與停止規則](README.md)、[v5唯一契約](../../protocol-v5.zh-Hant.md)。
2. [03完整v5玩法與交付](03-v5-gameplay-and-delivery.md)、[第03批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch03.zh-Hant.md)。
3. 先處理下述時序守門及[修正與已知問題](fix/README.md)（每個問題一份：成因／影響／復現／解決方案）；
   [04完整動作呈現](04-complete-action-presentation.md)尚不可視為依賴已驗收，須另外明確啟動。
4. 第02批人物校準、GUI證據與限制見[第02批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch02.zh-Hant.md)。
5. [原始v4基線與素材／測例清單](BASELINE.md)。v4完整認證只涵蓋原指紋，不自動涵蓋本候選。

## 已實作的第03批

- Client、產品Gateway、Match及兩份proto／Go bindings／HTTP join／Ready／Welcome
  一起使用v5，拒絕v1–v4；試跑需使用同批三角色並重啟舊程序。
- Match擁有跳躍規則、12發彈匣、10Tick單發、90Tick換彈、100HP／25傷害及180Tick重生。
  規則由Ready／Welcome下發，Client不以CSV或FBX時長決定玩法。
- 共用3D固定步、膠囊掃掠／支撐／頂頭、垂直預測／replay／鏡頭校正。
  Space沿只在第一個合法command消費；Held無jump，窗口滿／死亡／失焦／重同步清沿。
- LifeGeneration與movementEpoch獨立；重生保留PlayerId／Session及持續ActionId／帳本。
  舊生命請求仍得terminal裁決與ACK，晚到結果不改新生命HUD。舊移動取消另列觀測。
- Dead抑制操作／瞄準，保留中立命令與垂直落地；死亡取消換彈。
  到期先重生／補彈，然後全體移動，最後按穩定順序裁決動作，無新增同Tick互殺。
- 正式Space／R／左鍵及HUD已接入；R同幀優先，按住不連發，換彈／冷卻點擊不排隊。
  HUD使用最新權威HP／ammo／reload／death倒數；本機換彈請求有即時文字提示。
- 遠端位置／combat取同一呈現區間；生命或生死變更建立新段，人物相位重設。
  本批只有Idle／Jog及原槍模回饋，完整Jump／Reload／Death與素材快射重定時留第04批。

## 實作定位與下一批注意

- `Movement.*`／`PvpMatch.*`：產品權威、固定物理與生命狀態機。
  `Combat.hpp`：玩法唯一預設及ActionKind；`Arena`提供移動規則。
- `ClientConnection.*`：完整v5驗證、life／epoch窗口隔離、動作可靠交付與Drain。
  通用入口`SubmitAction(kind, life, observedTick, yaw, pitch)`；`SubmitShot`保留便利包裝。
- C++／proto保留歷史名稱`ShotRequest`／`ShotDecision`／`ShotRejection`與`shots`容器，
  實際支援Shot／Reload。wire enum的0是無效值；C++與wire拒絕值部分不同，必須明確映射。
  Shot角度要求presence，Reload不得帶角度；不要用static_cast替換映射。
- `LocalPlayerPrediction`含verticalVelocity／grounded／life；`SnapshotTimeline`回傳同段combat。
  `PvpApplication`以最新生命限制操作；切換所在幀丟棄轉換前收集的控制沿。
- `WeaponFeedbackObservation`提供life、HP／ammo、reload起訖／進度、death倒數與裁決kind；
  `RemoteMovementObservation`及人物frame帶life／dead。只讀觀測不是正式遊戲測試控制。
- 第04批沿用第02批女性／UAL／世界槍及Mark23，明確產品骨骼遮罩，不導入Enemy／Campaign。
  Reload原素材約3.733秒，Shoot約0.333秒；玩法時程分別1.5／約0.167秒，需做呈現映射。
  Jump Start／Land不可延遲物理；死亡動畫不能決定重生。人物命中仍是膠囊。
- 當前兩人產品Join前預備一個遠端GPU instance，Leave清識別但重用GPU；身高1.8、
  固定腳底anchor、Jog相位按路程。後退反向、側移近似，不宣稱全週期零滑步／IK。
- 第01–03批內容已存於`de87bb9`（wip）；啟動相位對齊經PR #2（分支`claude/project-thread-lv6bg5`）
  合併為`ff11ee3`；低幀率守門與驗收器修改經PR #3（`claude/pvp-v5-start-phase-guard`）合併為`9cd7f26`。
  不可把前批修改當無關內容刪掉。

## 驗證與證據

本批根目錄：`build/target/_build/test/logs/pvp-v5-batch03-20260928/`。
| 驗證 | 結果／入口 |
|---|---|
| 建置／CPU | 最後Client／Match／Gateway及probes成功；domain116 cases／1,395,489 assertions，人物12 cases／4,189 assertions |
| Go／wire | unit／race通過；完整datagram含頭仍≤1200；`go-validation.log`為原工具結果轉錄 |
| 證據工具 | 10項CTest短回歸、最後gameplay分析器17反例通過 |
| 真網路 | 25／25案、1075／1075動作逐生命交付退休；`gameplay-combined-25.json` |
| 恢復 | 新動作最慢1.046秒、穩定Actual起點0.550秒且維持250ms，原1.5秒門檻不變 |
| 主迴圈停頓 | 108／250／6000ms通過，worker存活；`main-stalls-1/recovery-results.json` |
| 生命清理 | 死亡等待中Leave／重入、Match斷線清空life／world／rules／帳本；`network-lifecycle-1/` |
| 真原生GUI | `gameplay-gui-3/`通過Space／快射／HP／R／死亡／180Tick重生／Esc；HUD圖已檢視 |
| 可見延遲 | `gui-timing-1`乾淨P50／P95 51.125／52.370ms未達標；`gui-timing-2`42.914／43.724ms通過，皆20／20配對 |
| 架構／指紋 | `architecture-fitness.json`靜態11項通過，未重跑完整remove build；`completion.json`為最後指紋且acceptance_complete=false |

原始失敗與退出碼見`execution-ledger-final.json`／`acceptance-bounded-summary.json`。
驗收器曾誤把故障時過期參考要求接受、把hold觀測算送包，已補反例並另寫重分析，原檔保留。
gui2有未排程mouse delta，來源未證實；不把受干擾結果或原GUI延遲失敗靜默剔除。
測試程序已全部退出，沒有要求使用者啟動服務或補跑長測。

## 第03批結案前的阻擋：啟動相位（已處理，見開頭）

`startup-phase-analysis.md`以相同command cursor分解交越：第02批本機→權威約24.504ms，
這次失敗輪30.698ms；權威→遠端約20.228／20.477ms，人物呈現並未增加約6ms。
首次輸入接納後等下一Authority Tick為9.073→14.781ms，首命令相位差6.675ms持續整場。
相關兩步lead／首次發布／獨立60Hz／一Tick插值公式本批未改。

此證據說明固定參數不保證每個啟動相位都達可見P50≤50ms：w（0～16.7ms）在啟動時抽定，
之後整個epoch固定加在延遲上。推估約18%的啟動會超過50ms。

**2026-10-01修正（方案A1，已獲使用者批准的契約補充）**：
- `MatchRuntimeHost`記錄每個epoch首窗口（含seq1）的收到時刻，到執行seq1的Tick時算出等待w，
  以可選`PlayerState.epochStartWaitMicros`／wire `epoch_start_wait_us`隨該epoch的Snapshot下發。
  Match不讀取，也不改Tick排程。
- `LocalPlayerPrediction`記錄首窗口發布時首個合法步已過時間；收到w後每epoch一次把固定步相位
  調整「w＋已過時間−4ms」，每幀最多移動經過時間25%，本機顯示不倒退。w>1Tick＋2ms
  （Host遲到）、epoch中途重新播種或首窗口後丟棄時間時不採用。
- 這是相位調整，不是延遲本機顯示：命令仍在產生幀取樣並立即顯示，只是改在離執行Tick
  較近的固定步邊界產生。lead仍是2個中立命令，序號→Tick對應、命令數、插值與門檻不變。
- 取捨：每個啟動的抖動餘裕固定在目前「w≈4ms的幸運啟動」水準，不再有w≈15ms時的額外餘裕。
- 驗證：C++ 118 cases／1,398,468 assertions（含既有486組恢復矩陣改為經過模擬Host回報w）、
  新增相位矩陣（30／60／144FPS×RTT0／20／40×worker×12個Authority相位）全部Actual、零重設；
  模擬的「命令產生→執行」中位數：144FPS各相位差≤1幀，最差相位改善≥1/3Tick
  （RTT0最差相位：60FPS 50→37.5ms、144FPS 45.8→36.1ms）。這是CPU模擬，不是可見延遲。Go unit／race及CTest `-L pvp` 11項通過
  （`presentation_cpu`因本環境無法下載shader工具未執行）。
  2026-10-02更正：該相位矩陣的模擬worker與幀時鐘相位鎖定；改用產品式worker／漂移／抖動後，
  30 FPS對齊時Held 10–14%，「全部Actual」不成立，見[fix/01](fix/01-a1-low-fps-regression.md)。
  PR head `d74830c`的本機CPU執行為119 cases／1,398,491 assertions（`ff11ee3`測試來源亦為119個TEST_CASE），
  與上列118不同。
- 未完成：原生雙GUI可見延遲短測、真網路25案矩陣未重跑。結案前須重跑GUI短測，
  並在結果記錄每輪的w與相位調整量；不可改門檻或重跑挑分數。
  更正（2026-10-02）：可見延遲短測`run_timing.py --gui --short`以`SDL_PushEvent`注入輸入，
  可在macOS／Metal執行，不需X11；只有`run_native_window.py`、`run_gameplay_gui.py`等原生視窗runner需X11／XTest。

### 2026-10-02：低幀率守門與暫緩結案

- 已做（細節各見fix文件）：
  - 低於約54.5 FPS不對齊：最新32個幀間隔（各最多計2Tick）平均>1.1Tick時不採用或撤回，
    ≤1.06Tick持續32幀才恢復。[fix/01](fix/01-a1-low-fps-regression.md)
  - Client本機診斷`StartPhaseSkip`（HostLate／FrameRateBelowTick／CancelledByReseed），非wire；
    驗收器逐epoch記錄w、調整量、撤回與取消。[fix/07](fix/07-start-phase-diagnostics.md)
  - 驗收器：跨行程時鐘域[fix/04](fix/04-macos-clock-domain.md)、
    等Match ready再啟Gateway[fix/05](fix/05-match-gateway-startup-race.md)、
    macOS視窗對角配置、視窗干擾判定與補跑規則[fix/06](fix/06-gui-window-interference-and-rerun-rule.md)。
  - 驗證：C++ 133 cases／1,418,939 assertions、CTest `-L pvp` 16／16、Python 165項通過。
    GUI冒煙3次與矩陣單案冒煙1次皆report-only，不計次、不作為驗收證據。
- 未做（當時）：計次GUI可見延遲短測、25案矩陣、第03批結案、第04批。其後處理見本文件開頭與追蹤dev_log。
- 未解決（暫緩，無承諾）：stall reseed使該epoch其餘時間失去對齊；本機三次GUI冒煙的移動方
  都被視窗啟動卡頓取消，冒煙未量到對齊後的可見延遲。[fix/02](fix/02-a1-cancelled-by-stall-reseed.md)
- 已知問題：切點以上偶發掉幀造成starvation重設；55–58 FPS規律掉refresh時對齊與否取決於啟動時機。
  [fix/03](fix/03-a1-missed-frame-starvation.md)
- 若恢復結案：先讀fix/02與fix/06；閒置機器、一次一輪，事前宣告總輪數與補跑規則。

### 本機驗證環境（Intel Mac）

- MacBook Pro 2019（x86_64）、macOS 26.7.1、Xcode 26.6＋Metal toolchain；GUI量測須機器閒置、一次一輪。
  完整平台、工具鏈與閒置條件以[fix/02](fix/02-a1-cancelled-by-stall-reseed.md)的「如何復現 A」為準。

## 架構與停止邊界

Architecture Delta限於PvP的wire／runtime狀態、Client操作／呈現及專用驗收。
Match仍不載Model／FBX／Renderer／SDL；Engine和公共Gateway不識別FPS政策。
不新增Top-level subsystem、跨v2／Editor依賴或通用可靠傳輸框架。
2026-10-02守門只新增產品內常數與Client本機診斷，未改Engine或wire；見[守門dev_log](../../../dev_logs/2026_10_02_pvp_v5_start_phase_guard.zh-Hant.md)。

v4的`MANUAL_ACCEPTANCE.md`、`ACCEPTANCE_STATUS.md`、`STABLE_BASELINE.md`維持原文及指紋。
證據位於git忽略的build目錄，提交文件不會保存原始trace／圖像，需另行備份。
第03批短驗證後停止；第04批與最終完整驗收須另外授權，不自動跑三輪GUI或30分鐘長測。

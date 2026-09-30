# PvP v5 交接

更新：2026-09-28。**第01–02批已完成；第03批完整功能與網路恢復已交付，整批驗收未結案。**
第04–05批未開始；未執行長測、未升格v5穩定基線、未自動commit。

唯一待結案守門：乾淨可見延遲短測曾有P50 **51.125ms >50ms**；最後另一次為42.914ms，
但啟動相位問題未修復，不能挑最後通過的跑次當作已完成。保持既定參數，停止在第03批。

## 閱讀入口

1. [進度與停止規則](README.md)、[v5唯一契約](../../protocol-v5.zh-Hant.md)。
2. [03完整v5玩法與交付](03-v5-gameplay-and-delivery.md)、[第03批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch03.zh-Hant.md)。
3. 先處理下述時序守門；[04完整動作呈現](04-complete-action-presentation.md)尚不可視為依賴已驗收，須另外明確啟動。
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
  舊生命請求仍得terminal裁決與ACK，晚到結果不改新生命HUD。旧移動取消另列觀測。
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
  Shot角度要求presence，Reload不得带角度；不要用static_cast替換映射。
- `LocalPlayerPrediction`含verticalVelocity／grounded／life；`SnapshotTimeline`回傳同段combat。
  `PvpApplication`以最新生命限制操作；切換所在幀丟棄轉換前收集的控制沿。
- `WeaponFeedbackObservation`提供life、HP／ammo、reload起訖／進度、death倒數與裁決kind；
  `RemoteMovementObservation`及人物frame帶life／dead。只讀觀測不是正式遊戲測試控制。
- 第04批沿用第02批女性／UAL／世界槍及Mark23，明確產品骨骼遮罩，不導入Enemy／Campaign。
  Reload原素材約3.733秒，Shoot約0.333秒；玩法時程分別1.5／約0.167秒，需做呈現映射。
  Jump Start／Land不可延遲物理；死亡動畫不能決定重生。人物命中仍是膠囊。
- 當前兩人產品Join前預备一個遠端GPU instance，Leave清識別但重用GPU；身高1.8、
  固定腳底anchor、Jog相位按路程。後退反向、側移近似，不宣稱全週期零滑步／IK。
- 起始HEAD仍為`edb6de4dfa179818ceeb9725712d29c288f18acd`；第01–03批均有未提交內容。
  不可把前批修改當無關內容刪掉，也不要自動commit。

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

## 第03批未結案：啟動相位

`startup-phase-analysis.md`以相同command cursor分解交越：第02批本機→權威約24.504ms，
這次失敗輪30.698ms；權威→遠端約20.228／20.477ms，人物呈現並未增加約6ms。
首次輸入接納後等下一Authority Tick為9.073→14.781ms，首命令相位差6.675ms持續整場。
相關兩步lead／首次發布／獨立60Hz／一Tick插值公式本批未改。

此證據說明固定參數不保證每個啟動相位都達可見P50≤50ms，尚未找到既定契約內
可直接消除此差異的本批程式缺陷。後續須釐清首次命令與Authority Tick的相位處理；
如需改時間契約，先提出具體取捨再確認。不可改門檻、縮lead、延遲本機顯示或重跑挑分數。
功能／網路證據可保留重用，但未修正並驗證此項前，第03批整體不得標完成或推進第04批。

## 架構與停止邊界

Architecture Delta限於PvP的wire／runtime狀態、Client操作／呈現及專用驗收。
Match仍不載Model／FBX／Renderer／SDL；Engine和公共Gateway不識別FPS政策。
不新增Top-level subsystem、跨v2／Editor依賴或通用可靠傳輸框架。

v4的`MANUAL_ACCEPTANCE.md`、`ACCEPTANCE_STATUS.md`、`STABLE_BASELINE.md`維持原文及指紋。
證據位於git忽略的build目錄，提交文件不會保存原始trace／圖像，需另行備份。
第03批短驗證後停止；第04批與最終完整驗收須另外授權，不自動跑三輪GUI或30分鐘長測。

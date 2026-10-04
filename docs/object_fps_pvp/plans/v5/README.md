# PvP v5 分批計畫與進度

更新：2026-10-04。Owner：`object_fps_pvp`。
第01–02批已完成；第03批功能完成，2026-10-02以持續相位追蹤＋token bucket worker取代A1與低幀率守門並加入連線品質移出
（PR #11，合併為`bec86b7`），**第03批2026-10-02結案驗收通過、已結案**；**第04批2026-10-02完成**（PR #14，合併為`cef1b39`）；
**第05批2026-10-03完成，完整驗收通過，v5升格為穩定基線**（[STABLE_BASELINE](STABLE_BASELINE.md)；PR #15合併為`f97beb5`，基線文件PR #16合併為`58346ca`）。
三角色已一起建成v5候選。乾淨GUI短測曾出現可見P50 51.13ms，高於50ms守門；
已定位既有首次命令／Authority Tick相位差。2026-10-01實作獲批准的啟動相位對齊（PR #2，合併為`ff11ee3`）；
2026-10-02以產品式worker的CPU模擬發現它使30／40 FPS退化（實機未重現），
補上低於約54.5 FPS不對齊的守門及驗收器修正（PR #3，合併為`9cd7f26`）。
原51.13ms失敗輪保留，不以後續通過覆蓋；結案驗收以持續相位追蹤版本在事前宣告下重跑：
計次GUI可見延遲短測3輪有效輪皆通過（可見P50 37.3／36.6／35.9ms、P95 38.2／40.8／37.3ms），25案真網路矩陣25／25通過（1075／1075動作）。2026-10-03完整驗收通過後升格（見下方「第05批結果與升格」與[STABLE_BASELINE](STABLE_BASELINE.md)）；保留固定兩步lead、獨立60Hz及一Tick插值，沒有擅改政策。

先讀 [交接](HANDOFF.md)、[v5契約](../../protocol-v5.zh-Hant.md) 和指定批次。
修正與已知問題：第03批啟動相位相關問題各一份文件（成因／影響／復現／解決方案），見 [fix/README.md](fix/README.md)。
延到v6處理的項目見 [v6交接](../v6/HANDOFF.md)；v6的分批與進度見 [v6進度](../v6/README.md)。
延遲整改從門檻失守到推翻重構的整體思考鏈，見 [延遲整改回顧](LATENCY_CASE_STUDY.md)（時間序）與 [v2](LATENCY_CASE_STUDY_v2.md)（系統模型、抽象層級與判準）。
原 [v4穩定基線](../v4/STABLE_BASELINE.md)、[手動指南](../v4/MANUAL_ACCEPTANCE.md)、
[驗收狀態](../v4/ACCEPTANCE_STATUS.md) 保留，不覆寫舊版證據。

## 進度

| 批次 | 文件 | 狀態 | 交付邊界 |
|---|---|---|---|
| 01 | [契約與基線](01-contract-and-baseline.md) | 已完成，2026-09-28 | v5契約、素材／測例清單、v4指紋、現有CPU短回歸 |
| 02 | [人物與移動動畫](02-player-model-and-locomotion.md) | 已完成，2026-09-28 | Client女性人物、掛槍、Idle／Jog與步頻；真雙GUI短測通過，仍為v4玩法 |
| 03 | [v5玩法與交付](03-v5-gameplay-and-delivery.md) | 已結案，2026-10-02 | 完整三角色v5、25案網路與生命恢復通過；持續相位追蹤＋token bucket worker與連線品質移出（PR #11）；計次可見50ms守門3輪與25案矩陣通過，修正與已知問題見[fix](fix/README.md) |
| 04 | [完整動作呈現](04-complete-action-presentation.md) | 已完成，2026-10-02 | 第一人稱換彈、遠端射擊／換彈／跳躍／死亡與生命隔離、Walk／Jog依速度混合；L1／L2／L3於macOS通過，見[第04批dev_log](../../../dev_logs/2026_10_02_pvp_v5_batch04.zh-Hant.md) |
| 05 | [整合短測與完整驗收交付](05-short-validation-and-acceptance.md) | 已完成，2026-10-03；完整驗收通過，**v5升格穩定基線**（[STABLE_BASELINE](STABLE_BASELINE.md)） | 長測與GUI戰鬥驗收器v5化、25案矩陣與整合短測、產品移除；[手動指南](MANUAL_ACCEPTANCE.md)、[驗收狀態](ACCEPTANCE_STATUS.md) |

```text
01 契約／基線 -> 02 人物Idle/Jog（v4）
                        -> 03 完整v5玩法／傳輸／操作
                                 -> 04 完整動畫
                                          -> 05 整合短測／驗收交付
                                                   -> 另行授權完整驗收
                                                            -> 通過才升格v5
```

## 執行規則

- 每次只執行使用者指定批次；依賴未完成不得以部分升版頂替。每批短測後更新本表、
  HANDOFF與dev_log，然後停止，不自動開始下一批或提交commit。
- 最初先落盤五份計畫與完成第01批；第02–03批依後續明確指示執行。第04批須另外啟動。
- 第03批把Client、Gateway、Match一起切為v5；試跑必須使用同批三角色，舊程序需重啟。
- 固定政策與資料語意只在v5契約維護。一般實作修正直接處理；若須改批准玩法、
  門檻、範圍或ownership，先報告具體證據，不以猜測改政策或放寬驗收。
- 長測不穿插開發。第05批只做短整合並交付完整驗收入口；需另外明確授權才執行
  三輪GUI及兩組30分鐘。先前對v4的長測授權不自動套用到v5。
- 先凍結來源／產物／分析器再量測；不要同時編譯與跑乾淨性能驗收。失敗保留原始
  跑次，先有限定位，再重驗受影響項目，不用大量重跑代替分析。
- Stage02改變Client呈現後應記錄新候選指紋；不因仍用v4 wire而宣稱原v4整份
  GPU／GUI性能認證自動涵蓋新人物。

## 第01批結果

- 起始HEAD：`edb6de4dfa179818ceeb9725712d29c288f18acd`，工作樹乾淨。
- v4 release manifest的232份來源／文件與7份產物指紋全部相符。
- 既有測試目標已是最新，Ninja無需重建；6／6 CTest短回歸通過，9.88秒。
  C++ domain含97 cases／1,390,416 assertions，全部通過。
- 只新增／修改文件與ignored基線證據，未修改產品、proto、bindings、資產、
  建置或測試程式。未啟動真socket矩陣、GPU、GUI或長測。
- 詳見 [基線清單](BASELINE.md) 及 [本批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch01.zh-Hant.md)。

## 接續處理文字

> v5已於2026-10-03升格穩定基線（macOS Intel／Metal、同機），先讀 STABLE_BASELINE.md 與 HANDOFF.md。
> 下一步是v6：2026-10-04已開始，分批與進度見 ../v6/README.md（延後項目的來源見 ../v6/HANDOFF.md）。
> Windows／Linux驗收與兩台機器的時鐘漂移實測須另行授權。

## 第02批結果

- 正式Client與GUI probe建置通過；原domain與新人物CPU／證據測試通過。
- 真Match／Gateway／雙GUI：30／60／144 FPS位移步頻及獨立GPU擷取4／4通過；
  約23秒原生操作V1–V8通過，包含射擊、HP、失焦、Tab、標題列拖曳、縮放及離開重入。
- 16秒移動＋射擊：20／20位移配對，呈現P50／P95為44.58／45.50ms；
  1920／1920 Actual，四次傷害令HP100→0；原門檻不變，沒有長測。
- Match／Gateway／Arena、v4 schema／bindings／玩法及三份歷史驗收文件未變。
  新Client呈現是第02批候選指紋，不替換v4完整認證，也未升格v5。
- 校準、GUI成本、失敗跑次、驗收器修正與限制見
  [第02批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch02.zh-Hant.md)。

## 第03批結果與結案

- 三角色一起升v5：共用3D跳躍／預測、12發彈匣／R換彈、死亡／重生、life隔離及HUD。
- 25個真網路案例、1075／1075動作、逐生命HP／彈藥與恢復通過；真主迴圈停頓
  108／250／6000ms及死亡等待Leave／重入／斷線清理通過。
- 正式三角色／probe建置、C++ domain與人物、Go unit／race、wire／證據分析器通過。
- 原生雙GUI的Space／快射／HP／R／180Tick重生及HUD通過；只做本批必要短驗證。
- 可見短測一輪51.125ms超50ms，另一輪42.914ms通過。首次命令的Authority相位
  差會持續成延遲差；不以第二輪取代第一輪或宣稱穩定基線。
- 2026-10-01：使用者批准方案A1，Host回報epoch首窗口等待w，Client每epoch一次對齊固定步相位。
  CPU／Go／CTest與三平台CI通過（PR #2，合併為`ff11ee3`）；見
  [A1 dev_log](../../../dev_logs/2026_10_01_pvp_v5_start_phase.zh-Hant.md)（含2026-10-02更正）。
- 2026-10-02：以產品式worker模型的CPU模擬（worker晚醒量取自本機Intel Mac）發現A1使30／40 FPS
  產生Held與重設（實機未重現），經使用者批准加守門：
  低於約54.5 FPS（1.1 Tick）不做啟動相位對齊；並修正驗收器的時鐘域、啟動競態、視窗配置／補跑規則
  與啟動相位紀錄。C++ 133 cases、CTest `-L pvp` 16／16、Python 165項通過；
  三次GUI冒煙與一次矩陣單案冒煙均為report-only，不計次、不作為驗收證據。見
  [修正與已知問題](fix/README.md)、[守門dev_log](../../../dev_logs/2026_10_02_pvp_v5_start_phase_guard.zh-Hant.md)。
- 結案驗收（計次GUI可見延遲短測＋25案矩陣）曾依使用者決定暫緩；
  stall reseed取消對齊（[fix/02](fix/02-a1-cancelled-by-stall-reseed.md)）與Client／Host時鐘漂移使對齊在長局失準
  （[fix/08](fix/08-a1-clock-drift.md)，模擬中Client快20ppm、RTT 20約第10分鐘超過50ms）同根於「每epoch量一次」，
  2026-10-02經使用者核准實作輸入worker 60/s token bucket＋閉環的持續相位追蹤，一併處理fix/03與新發現的
  [fix/09](fix/09-covered-gap-stuck-late.md)，並加入[連線品質移出](fix/10-connection-quality-eviction.md)；
  CPU驗證通過、經分支`claude/pvp-v5-phase-tracking`（PR #11，合併為`bec86b7`），實機report-only GUI冒煙1輪PASS（可見P50／P95 36.8／38.8ms；不計次、不作為驗收證據），見[追蹤dev_log](../../../dev_logs/2026_10_02_pvp_v5_phase_tracking.zh-Hant.md)；
  偶發掉幀重設的根因（worker鋸齒）一併解決（[fix/03](fix/03-a1-missed-frame-starvation.md)）。
- 2026-10-02結案驗收（使用者恢復；來源`2cecd3a`，事前宣告3輪GUI＋1次矩陣）：計次GUI可見延遲短測3輪有效輪皆通過（可見P50 37.3／36.6／35.9ms、P95 38.2／40.8／37.3ms），
  兩方量測全程tracking、0次移動重設；第3輪因量測中OS焦點變化判`invalid_window_disturbed`而依fix/06補跑一次，原輪保留。
  25案真網路矩陣25／25通過（1075／1075動作），新操作恢復最慢1.110秒（門檻1.5秒）；第一次矩陣因工具會話結束在第12案中斷，保留不計並重跑完整25案。
  證據：`build/target/_build/test/logs/pvp-v5-batch03-closure-20261002/`。
- 後續候選（未排程、無承諾）：CS式開局／回合準備期，全員凍結且無敵並倒數，讓啟動相位量測與
  首幀卡頓落在其中；需Match回合狀態、無敵規則、HUD倒數與契約變更，須另立計畫，見[fix/02](fix/02-a1-cancelled-by-stall-reseed.md)。
- [第03批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch03.zh-Hant.md)與[交接](HANDOFF.md)
  保存修復、失敗與最後指紋。第01–03批內容在`de87bb9`（wip），A1在`ff11ee3`；
  守門與驗收器修改經PR #3（`claude/pvp-v5-start-phase-guard`）合併為`9cd7f26`；持續相位追蹤與連線品質移出經PR #11，合併為`bec86b7`。第03批已結案；本批未執行長測（長測屬第05批）。

## 第04批結果

- 2026-10-02完成（計畫複審PR #13合併為`dcb19d1`；實作PR #14合併為`cef1b39`）。只改`object_fps_pvp`的Client呈現、產品資產與產品自有驗收器；wire、Match、Gateway與Engine不變。
- 第一人稱換彈；遠端射擊／換彈／跳躍三段／Death01，呈現為時間線同區間、同生命權威資料的純函數，不重播、不補播；
  驗證工具跨平台（平台指紋、視窗配置、SDL注入動作短測）；步態改為Walk／Jog依速度混合（使用者決定，契約§6同步）。
- 驗證：CTest全標籤41／41；突變檢查04-2 9／9、04-5 5／5；L1動作短測30／60／144 FPS與L2 Metal capture通過（第一次因實體滑鼠移動失敗，保留）；
  L3原生操作清單1–8與步態由使用者人工確認。只在macOS Intel／Metal；未執行長測與完整GUI三輪。
- 延到v6：遠端上半身俯仰瞄準、受擊反應、Engine呈現阻塞、本機冷卻閘約2 Tick落差等，見[v6交接](../v6/HANDOFF.md)。
- 經過與證據見[第04批dev_log](../../../dev_logs/2026_10_02_pvp_v5_batch04.zh-Hant.md)與[交接](HANDOFF.md)「第04批進度」。

## 第05批結果與升格

- 2026-10-02使用者啟動，並決定「v5結案」＝本批＋完整驗收＋全部通過即升格；平台只在本機macOS（Windows／Linux標「未執行」）。
  PR #15合併為`f97beb5`；穩定基線文件經PR #16合併為`58346ca`。
- 05-1／05-2：headless長測與GUI combat驗收器v5化（逐生命、換彈、死亡／重生）。05-3：25案矩陣25／25、雙GUI整合短測、長測短模式、產品移除通過。
  05-4：[手動指南](MANUAL_ACCEPTANCE.md)、[驗收狀態](ACCEPTANCE_STATUS.md)。
- 05-5完整驗收（2026-10-03）：GUI三輪×120秒皆通過（可見P50 36.6／35.8／34.9ms）；60 Hz與144 Hz各1808秒長測通過（4971／4971動作）。
  60 Hz原判定失敗，使用者選規則B（重生首幀改為結構條件）後以同一份原始資料重新分析為通過；144 Hz第一次被工具時限中止、
  第二次整機瞬間停頓判受干擾，第三次通過。失敗與中斷跑次全部保留。
- 2026-10-03升格穩定基線，範圍為macOS Intel／Metal、同機三角色；見[STABLE_BASELINE](STABLE_BASELINE.md)與[第05批dev_log](../../../dev_logs/2026_10_03_pvp_v5_batch05.zh-Hant.md)。

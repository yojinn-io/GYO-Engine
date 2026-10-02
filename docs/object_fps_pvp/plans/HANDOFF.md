# PvP v4 交接

後續開發入口：[v5 交接](v5/HANDOFF.md)、[五批進度](v5/README.md)。
v5 第01–02批完成；第03批已建成三角色v5候選，但可見延遲守門未結案；第04批未開始。
下文保留 v4 完整驗收的歷史交接，長測授權不自動延伸至 v5。

更新：2026-09-28。**第 05 批完整驗收已完成，依使用者追加授權將 v4 升格為穩定基線。**
**範圍為 Linux／X11／Vulkan、同機雙玩家。** 手動指南與回報表保留供未來測試案例參考。

## 現況與閱讀順序

1. [進度與執行規則](README.md)：只執行使用者這次指定的批次。
2. [v4 契約](../protocol-v4.zh-Hant.md)：政策、時間與資料語意的單一入口。
3. 本次 [05 整合短測與手動指南](05-short-validation-and-manual-test-guide.md)：
   [手動命令與門檻](MANUAL_ACCEPTANCE.md)、[驗收狀態／回報表](ACCEPTANCE_STATUS.md)。
4. [穩定基線](STABLE_BASELINE.md)：最終驗收、版本指紋及適用範圍。
5. 第 05 批最初交付見 [第 05 批 dev_log](../../dev_logs/2026_09_28_pvp_v4_batch05.zh-Hant.md)。
   正式遊戲實作與呈現證據見 [第 04 批 dev_log](../../dev_logs/2026_09_28_pvp_v4_batch04.zh-Hant.md)。
   網路共存／恢復證據保留於 [第 03 批 dev_log](../../dev_logs/2026_09_28_pvp_v4_batch03.zh-Hant.md)。

Client／Gateway／Match 的 wire、HTTP join、Ready／Welcome 已一起升為 **v4**，
拒絕 v1–v3。試跑必須使用一起重建的三個角色；舊程序須重啟。
射擊已能經正式 GUI → ClientConnection → Match 完成裁決與扣血。
**正式 Client 已有 Mark23、單發操作、命中提示與權威 HP；仍需 Match＋Gateway 執行。**

## 第 05 批最終驗收與停止狀態

- 三輪 GUI 各 120 秒／200 個移動事件／150 槍，全部配對、取得唯一接受裁決，
  下一成功 Presented 幀回饋及權威 HP／HUD 核對通過；每輪四次 25 HP 真實扣減。
  三輪可見延遲 P50／P95 為 37.82／38.76、39.81／40.76、38.54／39.55 ms，
  各自符合原門檻，零干擾與 epoch 重設；原始慢幀及全部事件均保留。
- 原生視窗 `native-window-6` 的 V1–V8 通過，包含九次原生射擊、實際 HUD、
  標題列拖曳、縮放、失焦、重入與 Lobby 操作。此功能跑次含 120.38 ms 長幀，
  不冒充乾淨效能證據，亦未證實歷史 OS 整機停頓根因。
- 60／144 Hz 各實時 1,800 秒 headless 移動＋合法射擊長測完整通過，分別
  8,802／8,938 次射擊全部接受、交付及退休；215,998／216,000 個移動命令
  全部 Actual，零重設、診斷遺失、模擬掉時或預測凍結，30 Tick 佇列和最大 60。
  每組兩玩家各實際扣 100 HP；歸零後 HP 本身無法辨認重複零傷害，另核對
  動作識別、唯一裁決、連續確認及退休完整性，不能只用最終血量宣稱去重成立。
- 正式產品來源及 Client／Match／Gateway 二進位未變。修正限於 owner 驗收器及
  原生視窗工具：持續射擊排程、雙玩家完整 HP／Tick／最終值及 GUI 乾淨跑次門檻。
  19 項分析器／runner 回歸通過；原始三輪 GUI 與既有短測另作補充核對，未覆寫原結果。
- 最終證據位於 `build/target/_build/test/logs/`：`pvp-v4-release-gui-1/`、
  `pvp-v4-release-evidence-audit-1/qualification.json`、
  `pvp-v4-release-soak60-1/result.json`、`pvp-v4-release-soak144-1/result.json`。
  原生視窗與版本指紋入口見 [穩定基線](STABLE_BASELINE.md)。
- 完整核對後已升格，不再等待使用者啟動長測或回報。保留
  [手動指南](MANUAL_ACCEPTANCE.md) 與 [驗收狀態／回報表](ACCEPTANCE_STATUS.md)。
  本輪未提交 commit，不自動開始下一功能或新一輪測試。

## 第 05 批最初工具交付（歷史證據）

- 正式產品政策／來源及 Client／Match／Gateway 二進位維持第 04 批；新增只在產品
  acceptance：合法射擊長測模式、有界背景資料輸出、GUI `--combat`、HP／裁決／
  下一 Presented 核對、失敗／not_run 總摘要。沒有正式遊戲 test 控制或新公共模組。
- GUI 整合 120 秒：200/200 配對，P50/P95 **42.29/43.48 ms**，150/150 真 SDL 單發
  及唯一裁決，四次25HP；全部150槍下一成功呈現幀回饋，Client裁決 P95 100.55 ms。
  Actual100%，零重設／診斷遺失，實際59.70FPS。當時只是一輪整合，沒有取代後來的三輪驗收。
- 純移動診斷首輪 P50 52.02 ms 超標，與 fitness build 重疊；第二輪指標通過，兩輪
  各有一次 pointer-release 記錄。原始失敗、外部負載及未證實根因均保留，不挑好的
  報告冒充完整驗收。GUI共存短／完整輪沒有該release記錄。
- Headless30／60／144 Hz每案10秒，148/148裁決均接受，各案兩玩家各實際扣100HP，
  Actual100%。RTT0／20／40＋jitter/loss及burst2四個新短案通過；20ms案有一次
  明確Cooldown拒絕且damage0。ACK／阻塞等03證據重用，原始分析器失敗保留。
- 隔離副本實際移除224項owner內容及registry，Engine／獨立fixture build/install/run
  成立；Match無SDL／Model／Renderer。既有其他FPS只configure，不聲稱完整重建。
- 當時交付 [MANUAL_ACCEPTANCE](MANUAL_ACCEPTANCE.md) 及
  [ACCEPTANCE_STATUS](ACCEPTANCE_STATUS.md)，安排由使用者啟動完整驗收。
  後續追加授權改由 Agent 執行；最終結果見上節，原手動命令及參考表完整保留。

## 已完成的 API 與責任

來源均在 `apps/object_fps_pvp/`，不新增 Engine／公共 Gateway 遊戲政策：

- `Combat.hpp`：唯一預設 `PvpCombatRules`，請求、裁決、戰鬥狀態型別；動作排程上限
  `ActionSendRate=30`。CombatRules 從 Match Ready 經產品 Adapter 到 Client Welcome。
- `PvpMatch`：先完成全部玩家移動再裁決；每玩家 pending＋未 ACK 裁決及 ID 距離均
  ≤32；連續 ACK 才退休。HP／冷卻／帳本不受 movementEpoch 影響。
- `MatchRuntimeHost::SubmitActionBatch(batch, acknowledgedThrough)`：shots＋ACK
  原子接納，支援純 ACK。格式／衝突失敗不能部分確認。有效待交付 ACK 可為新 ID
  預留窗口，真正退休與世界修改仍只在 Tick。`SubmitActions` 保持拒絕空 shot 列表。
- `PvpMatch::CanSubmitActions(batch, staged, acknowledgedThrough)`：驗證 prospective
  退休容量與 Host 暫存；內容衝突仍檢查退休前原紀錄。不要把 wire batch 拆成兩次
  非原子的 shots／ACK 呼叫。
- Host 最新 64 筆發布 Tick／steady-time 索引提供裁決年齡；只索引每次 Advance
  最終進入 handoff 的 snapshot。不是歷史命中盒，也不是送達證據。
- `IpcHost.cpp`：已開始的 TCP frame 完整寫出；未開始 snapshot 可替換；裁決從
  Match 帳本循環取最多八筆，讀取不退休。每玩家 lane 隨 Leave 清理。
- `gateway/action_delivery.go`、`runtime_link.go`：Session 映射 PlayerId，有界保存
  與循環重送，不能用「已轉送」冒充 Client ACK。寫完後重新錨定 deadline，
  避免阻塞解除後補送過期次數；公共 120 包／秒限制不變。
- `ClientConnection::SubmitShot(observedAuthorityTick, yaw, pitch)`：僅在有效連線及
  有界窗口可接納時配置下一個 ActionId，返回 optional；失敗不配置或跳號。
  worker 持有不可變請求、30 Hz 重送及 ACK；主執行緒不自行重送或補造射擊。
- `ClientConnection::Drain()`：原子取得狀態、快照接收歷史與 `decisions`。
  裁決只交付一次，只有已交付的連續 ID 才可 ACK。快照 history overflow／
  movementEpoch／主執行緒停頓不清裁決；Leave／斷線／換 Session 清理。
- `ClientConnectionState.combatRules` 為 Match 規則；`snapshot->combat` 為最新 HP／
  冷卻；`actionTransport` 為有界只讀診斷。戰鬥狀態不插值、不被移動 replay 覆蓋。

新的 schema 位於 `protocol/client_v4.proto`／`runtime_v4.proto`，Go bindings 在
`clientv4/`／`runtimev4/`，C++ 生成於 build tree。舊 v3 schema／bindings 已移除。
生成使用 build 的 protoc 36.2 與 module 固定的 protoc-gen-go v1.36.11。

## 已完成的 Client 整合與維護注意點

- 沿用 PvpApplication 的一次 Drain，同幀消費 `decisions`；不要另 Drain 一次後
  丟掉結果。取得結果後的遊戲呈現由主執行緒負責，worker 不重播已交付事件。
- 以最新快照 Tick 和本次絕對 yaw／pitch 呼叫 SubmitShot；不要帶 Client 時間、
  位置、命中或傷害。optional ID 成功才表示本機動作已交給傳輸，並非權威接受。
- 左鍵上升沿、捕捉、Mark23 資產準備、本機冷卻／動畫／後座、HUD 與命中標記
  都已於第 04 批接入。Match 規則不可在 Client 另維護一套常數。
- 同一動作只播一次回饋。逾時保持待確認；不能換新 ID 重開，也不能因失焦刪除
  已提交請求。窗口滿不配置 ID。接受且命中玩家才顯示命中標記。
- HP 只取最新 snapshot combat，晚到裁決只用於事件呈現；不能倒退 HP。
  HP=0 仍可操作，Leave／新 Join 才取得新身分及滿血。
- 保留既有兩步 lead、60 Hz 移動、恢復與延遲門檻。射擊額外最多 30 Hz，
  ACK 共用此預算；Hello、重送及非法已認證包均可能計入 120 包／秒限流。

## 第 03 批已保存的網路短驗證

- Client／Match／Gateway／worker、network、action probes 建置成功。
- CTest 8／8 通過（12.01 秒），包含 C++ 97 cases／1,390,416 assertions；
  Go race 通過；分析器最後補強回歸後另跑 6／6 通過。
- 同一組二進位 16 案真 socket 矩陣，每案六秒：4,724 筆唯一裁決完整交付並退休，
  23,040 筆快照 HP 核對符合唯一傷害。首包／結果／ACK 遺失、重複、亂序、衝突、
  未 Drain 1.3 秒，以及五處 250 ms／1 秒阻塞皆有證據。
- 乾淨射擊壓力下：移動 60 Hz、Actual 100%，首次送出 P95 16.21 ms；
  Actual 執行 P50 45.79 ms／P95 45.86 ms，零 epoch 重設。
- 最慢新裁決恢復 385.90 ms；連續 Actual 恢復起點 635.94 ms，至 902.58 ms
  已維持至少 250 ms；所有可恢復案例均符合 1.5 秒要求。
- Gateway 被刻意停頓一秒後有 118 包遭既有限流拒絕，接受窗口最高 120；正常
  共存零限流拒絕。原始報告曾因分析器將「所有故障零拒絕」視為要求而失敗，原報告
  保留；最終重核明列拒絕並驗證 cap 與完整恢復，沒有修改產品上限或恢復門檻。
- 原移動真網路短回歸通過：RTT 0／20／40 ms、最多 10 ms 設定抖動、5% 丟包、
  首包與連兩包遺失、六秒主執行緒停頓、Leave／重入／Lobby／IPC 失敗清場。

主要證據在 `build/target/_build/test/logs/`（本機 ignored 產物）：

| 入口 | 用途 |
|---|---|
| `pvp-v4-batch03-short.log`／`.xml` | 八項短回歸 |
| `pvp-v4-batch03-go-race.log` | Go race、真 UDP／TCP 與最大封包測試 |
| `pvp-v4-batch03-action-evidence.log` | 最終六個分析器回歸 |
| `pvp-v4-batch03-actions-final-1/action-matrix-final.json` | 最終 16 案含門檻、逐案恢復及原始失敗原因 |
| 同目錄各案 `artifacts.json`、trace、`result.json` | 產物 SHA、原始事件與原始結論 |
| `pvp-v4-batch03-network/result.json` | 既有移動及生命週期真網路短回歸 |

重跑命令、分析器重新核對方式及限制詳見 dev_log。不要用未完成的早期 matrix
或歷史 v3 release manifest 代替上列最終證據。

## 第 04 批呈現與操作

- `PvpApplication` 的同一幀先更新絕對視角，再提交最多一個合法左鍵上升沿，
  不在移動固定步或 replay 內建立動作。按住不連發；捕捉輸入的整幀不射擊。
- Tab 釋放、focus lost、window moved／resized／minimized 立即清當幀待射擊；
  Tab 釋放所在幀亦不能被後續 click 重新捕捉。既有 worker 裁決及保活繼續。
- 最新 `snapshot.combat` 決定 HUD HP／冷卻；Drain 裁決只觸發命中／拒絕提示，
  不修改 HP、不重播動畫。HP=0 仍可操作；Leave／新身分清呈現計數及效果。
- `WeaponViewModelFrame` 是產品內呈現專用值（Idle／Draw／Shoot、elapsed、duration、
  visual recoil），不需要 WeaponController／彈匣／Campaign。舊 snapshot overload
  仍保留。動畫時長讀已載入 clip；射擊冷卻讀 Match rules。
- `InitializeGraphics` 在 Join 前載入 Mark23 FBX、三張 PNG、GPU meshes／textures，
  並完成一次成功 Idle Render 暖機。槍模用獨立 ViewModel 深度層，後座不改世界鏡頭
  或已提交的瞄準角。必要 Model／ModelRenderer／SDL_IMAGE／UFBX 只接到 Client。
- `WeaponFeedback()` 提供只讀狀態、提交／動畫／裁決計數、準星／HP、同機時間及
  模型姿態資料；`PresentedMovement().weapon` 只在成功 Presented 後形成證據。
  production 沒有 probe 專用控制開關，測試仍注入 SDL events／使用現有連線 API。

已完成的證據位於 `build/target/_build/test/logs/`：

| 證據 | 結果 |
|---|---|
| `pvp-v4-batch04-core.log`／`.xml` | 6 項 CPU／模型／元件通過；初次 GPU 因 sandbox 無法開桌面失敗，原記錄保留 |
| `pvp-v4-batch04-gpu.log`／`.xml` | 取得桌面存取後，實際 Vulkan GPU smoke 通過（0.88 秒） |
| `pvp-v4-batch04-presentation-evidence.log` | 16 個分析器測試通過（6.687 秒） |
| `pvp-v4-batch04-gui-final-1/weapon-short-matrix.json` | 五項真雙 GUI 短測全通過 |
| 同目錄 `weapon-full-metrics-summary.json` | 原始 trace 離線補算所有呈現間隔及每一槍；不重跑，不覆寫原結果 |
| 同目錄 `latency/result.json` | 16 秒／20 事件全配對，P50 48.43 ms、P95 48.84 ms |
| 同目錄 `capture/` | Draw／Idle／Shoot／resize／wall-depth 五張 GPU BMP 及無損 PNG 副本 |

30／60／144 FPS 實際約 29.9／59.6／142.1。21 個 SDL 射擊均在下一個成功呈現幀
開始，提交至該幀返回 1.59–2.62 ms，不是實體點擊至螢幕發光的延遲。六次本機
射擊＋一個明確的權威冷卻測試請求不混算為七次本機動畫；對手 HP=0 後另實測移動／射擊。
所有原始 Update 幀、成功 Presented 與跳過幀均保留；離開重入跨 Lobby 的世界樣本
間距另列，不能將沒有世界樣本的 Lobby 幀誤稱作 GPU 卡頓。

腳本入口 `build/acceptance/object_fps_pvp/run_weapon_short.py` 預設只跑五項短測，
可用 `--case weapon60` 單獨重現；完整命令在 dev_log。`--latency-short` 顯式 16 秒／
20 事件，原 `--latency` 的 120 秒／200 事件與完整驗收門檻不變。
第 05 批重用相同正式產品來源的證據，不為整理文件重跑全部短矩陣；
完整 GUI 及長測均同時射擊與核對 HP，已依追加授權完成。

## 工作樹與限制

- 工作樹未提交。起始 HEAD 為 `bfeb047669d465ce2a9a43af43c85fb3e577e4bc`；
  第 01–05 批及更早架構／成本總結的未提交修改均保留。
- 第 04 批 Architecture Delta 是產品 Client 使用既有模型／圖片／動畫機制與呈現 API；
  第 03 批的產品契約、Client／Host／Gateway 交付保持不變。
  Engine／公共 Gateway 無變更；Match 動態依賴沒有 SDL。第 04 批完成靜態邊界
  檢查，第 05 批另於隔離副本移除 224 項 owner 內容及 registry 並完成獨立建置驗證。
- 第 04 批 SDL 注入及 UI 前 GPU captures 與本次原生視窗／實際 HUD 證據分開保留。
  兩組長測是 headless 網路／預測／射擊共存，不是 GUI 長測；原生視窗跑次是功能
  驗收，不是乾淨效能跑次。未執行實體 LAN、Windows 或跨主機時鐘漂移驗證。
  v4 查詢當前權威姿態，沒有命中回溯。舊 v3 manifest 未覆寫。
- OS 拖窗整機停頓根因仍未證實。本批無待決的契約變更。

第 05 批與升格已完成並停止。未提交 commit；後續功能須由使用者另行指定。

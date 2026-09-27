# 第 03 批：Protocol v4 與動作交付

依賴：第 02 批完成。先讀 [交接](HANDOFF.md) 及 [v4 契約](../protocol-v4.zh-Hant.md)。
**2026-09-28 已完成並停止。** 本批額外要求先證明網路共存與恢復，已完成；
第 04 批未開始，未執行長測。實作、原始證據及限制見
[本批 dev_log](../../dev_logs/2026_09_28_pvp_v4_batch03.zh-Hant.md)。以下保留本批要求。

## 目標

打通 Client → 產品 Gateway → Host → Match 的動作／裁決，以真 socket headless
probe 驗收。GUI 射擊操作與槍模留第 04 批。

## 協定與交接

- 兩份 Protobuf 一起升 v4，更新 C++／Go bindings、轉接器、建置引用、HTTP join、
  Ready／Welcome。三角色一起升級，明確拒絕 v1–v3，不留混合版本分支。
- 保留移動語意；新增 ActionBatch、ActionResults，各最多八筆。請求批次攜帶
  裁決連續確認下界並允許純確認；結果批次回報權威退休下界。
- Snapshot 加入獨立玩家戰鬥狀態集合；HP 不插值，不被移動 replay 覆蓋。
  CombatRules 由 Match 下發，Client 不複製權威預設值。
- Gateway 由 Session 映射 PlayerId；Client 不任選可信玩家身分。
- 各層按 ActionId 保留／合併，未確認窗口最多 32 個，分批循環重送。
  裁決不能僅存在於某一幀 Snapshot 或會丟失結果的 control queue。
- 連續確認不能跨洞；同 ID 內容衝突整批拒絕，原帳本及效果不變。退休下界前的
  舊資料不重新執行。逾時只顯示待確認，不能換 ID 補開同一次操作。
- 換 Session／Leave／斷線清理，主執行緒停頓與 movementEpoch 不清理。

## 排程與容量

- 移動排程不改；額外動作／確認共用最多 30 Hz，有資料才啟動，錯過 deadline
  不補送歷史次數。裁決也最多 30 Hz；立即傳送仍受相同預算限制。
- 驗證移動 60 Hz＋動作最多 30 Hz＋保活約 1 Hz 不超過現有 Session 120 包／秒。
  不提高公共限流，也不將 Hello 或重送排除於統計。
- 最大合法欄位序列化後，整包含 24-byte header 仍 ≤1,200 bytes。
- Host／Gateway 有界交接，I/O 不阻塞 Authority；部分寫出的 TCP frame 完成或
  連線失敗，不替換餘下 bytes。不建立通用可靠通道框架。

## 短測與完成條件

- codec／adapter、版本拒絕、最大長度、Go race、C++ 與移動短回歸通過。
- 真 socket：首包、裁決、確認遺失，重複、亂序、衝突，各層 250 ms／1 秒阻塞。
- 每 ID 最多一次效果，重送取得相同裁決；有效 Session 解除干擾後 1.5 秒內
  恢復新操作裁決，舊操作依法過期拒絕，窗口有界且不超限流。
- 交付 headless 操作 probe、診斷及短測證據；更新進度／交接後停止。

## 完成記錄

- v4 schema／bindings／HTTP／Ready／Welcome 全套切換，明確拒絕 v1–v3。
- Client、產品 Gateway 與 Host 保留不可變動作／裁決，循環八筆重送、最多 32 ID，
  連續消費確認；Snapshot／movementEpoch 不清除動作帳本。
- C++、worker／codec／分析器、Go race 及既有移動網路回歸通過。
- 16 案真 socket 六秒短測通過：首包／結果／ACK 遺失、重複／亂序／衝突、
  暫停 Drain 1.3 秒，以及 upstream／downstream／socket 路徑／Gateway／Host IPC
  各 250 ms／1 秒故障。4,724 筆裁決、23,040 筆快照 HP 核對均通過。
- 正常移動＋射擊＋Hello 實際最高 91 包／秒、零限流拒絕；移動 Actual 100%。
  Gateway 停頓一秒後的積包觸發 118 次既有限流拒絕，接受窗口仍 ≤120，
  新動作及移動均於 1.5 秒內恢復。故障跑次不冒充無丟包跑次。
- 路徑代理及 partial TCP frame 證據不等於 OS socket buffer 飽和測試。
  未驗收 GUI 開槍、GPU、實體 LAN 或長時間穩定性。

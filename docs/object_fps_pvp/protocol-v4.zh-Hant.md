# PvP Protocol v4：時間、動作與權威射擊契約

狀態：2026-09-28 第01–05批與追加完整驗收已完成，**v4升格為穩定基線**。
驗證範圍為Linux／X11／Vulkan、同機雙玩家及受控網路。Client／Gateway／Match
一起使用v4並拒絕v1–v3；Mark23單發、權威HP、動作確認與既有移動政策維持不變。
三輪GUI移動／射擊／HP、60／144Hz各30分鐘與原生桌面操作已通過。
基線指紋、證據、限制見 [穩定基線](plans/v4/STABLE_BASELINE.md)；
執行歷史見 [分批計畫](plans/v4/README.md)。實體LAN、Windows及射擊回溯不在本輪認證內。

## 1. 範圍與責任

沿用權威狀態同步：Client 送操作，Match 回狀態與裁決；本機只預判自身移動及武器
呈現。不改成 Lockstep，不加入完整世界 Rollback，不建立公共同步框架。

```text
Client input / predicted movement / speculative weapon animation
    | immutable movement window + independent action requests
    v
Go product Adapter: Session -> PlayerId, validation, routing, bounded transport
    | separate client / runtime contracts
    v
C++ Runtime Host: lifecycle, clock, bounded handoff, publication-time index
    | movement commands + actions + trusted age metadata
    v
Product Match: move all players -> adjudicate actions -> state / decisions
    | pure product rules using reusable GYO collision / fixed-step mechanisms
    v
Gateway -> Client: latest network state + retained action decisions
```

產品政策由 `object_fps_pvp` 擁有。Engine 提供固定步進、幾何、模型／呈現等機制；
公共 Gateway 不認識射擊、傷害、缺包替代或復原政策。Host 提供可信時間資訊，
Match 決定期限及玩法；Go 不做命中查詢。將來的接收者規則亦由權威決定，Gateway
只執行。本輪仍為兩位玩家，不加入 AOI、增量、壓縮、分區或持久交易。

## 2. 保持不變的 v3 基線

以下是本輪必須守住的產品政策，不升格成 Engine 全域規則：

| 項目 | 固定值／語意 |
|---|---|
| Authority／Client 固定模擬、worker 移動傳送、Snapshot | 名義各 60 Hz，仍為各自的排程責任 |
| 初始／重新同步 lead | 兩個中立命令；新 epoch 首個合法操作步形成後首次發布完整窗口 |
| 移動未確認窗口／Match 未來命令 | 最多 12／數量及距離游標皆最多 32 |
| 移動 ACK | `<tip` 重播，`==tip` 正常確認保留餘數，`>tip`／新 epoch 重新播種 |
| 缺移動輸入 | 最近實際執行的真實命令最多 Held 15 Tick，之後 Neutral；都推進游標 |
| 積欠／耗盡復原 | 原 30 Tick 統計、105 門檻、60 Tick 冷卻；不因射擊更改 |
| 遠端呈現 | 64 份接收歷史、一 Tick 延後、單調游標、不外推 |
| 本機校正 | 100 ms 收斂、1 世界單位硬校正、牆壁限制 |
| UDP／IPC | 完整 UDP ≤1,200 bytes（含 24-byte header）；IPC payload 1–65,536 bytes |

原本的首批積時保護、Advance 呼叫點計時、完整 ACK 插值端點及窗口滿保護也保留。
射擊不增加移動步數、不修改已有 MovementCommand，不把動作窗口佔用算成移動積欠。
詳細現行行為見 [聯網架構](network-architecture.zh-Hant.md)。

## 3. 時間與身分

| 概念 | 回答的問題 | 不能兼任 |
|---|---|---|
| SimulationTick | 世界完成哪個 Authority 步驟？ | 封包序號或某玩家的輸入序號 |
| InputSequence | 此玩家移動流的第幾個固定步？ | 動作是否接受的確認 |
| PacketSequence | 此 Session 傳輸流的第幾個包？ | 去重帳本或遊戲執行次數 |
| ActionId | 是不是同一次射擊請求？ | 固定一步模擬或 movementEpoch |
| movementEpoch | 此移動命令是否仍屬當前移動世代？ | 重生、交易或射擊帳本版本 |
| Session／PlayerId | 資料是否屬於這次有效加入？ | 玩家帳號或持久角色 |
| Client connection generation | 非同步結果是否屬於當前本機連線操作？ | Client 自訂的權威世代 |

ActionId 是正的 64-bit 遞增值，每次新 PlayerId 加入從 1 開始，不能回繞。
外部去重範圍是有效 Session／玩家加入生命週期＋ActionId；Runtime 不需要 Session
token，使用 Gateway 映射的 PlayerId 及現有 IPC 清場邊界隔離。movementEpoch 改變
不影響動作。耗盡 ID 時停止建立新動作並要求重新加入，不能悄悄重用 ID。

本輪沒有原地重生，不新增空的 EntityGeneration 欄位。未來加入重生時須另定實體
世代，不能借用 movementEpoch。封包可裝多個動作，重送保留 ActionId 並使用新
PacketSequence；現有 UDP 新舊丟棄仍可能丟失某次傳送，因此需要動作層重送。

## 4. 三種歷史的界線

| 資料 | 用途 | 本輪處理 |
|---|---|---|
| Network Snapshot | 告知 Client 權威狀態，將來可依接收者投影 | 保留完整兩玩家移動，新增獨立戰鬥狀態 |
| Hit-test History | 查過去姿態／幾何是否命中 | 不建立；v4 查目前權威姿態 |
| Rollback State | 完整恢復指定模擬範圍並重算 | 不建立；本機移動 replay 不是世界 Rollback |

Host 的 Tick／發布時間索引只是有效期限 metadata，既不是命中歷史也不是存檔。
將來可抽出經驗證的有界時間容器，但不強迫三種資料共用格式；現在不新增可選
同步模組或 Capture／Restore 公共介面。

## 5. CombatRules 與當前狀態裁決

以下數值是已選定的 PvP 第一版政策，集中於產品 CombatRules，由 Match 經
Ready／Welcome 提供，Client 不另外維護權威預設。既有 Mark23 是呈現資產，
不讓 Match 載入 FBX／動畫，也不自動接入 Campaign 的彈匣／換彈流程。

| 項目 | 值 |
|---|---|
| 武器／輸入 | 單一手槍，左鍵上升沿單發，無限彈藥 |
| 滿血／每次傷害 | 100／25 |
| 射擊冷卻 | 20 Authority Tick；在 Tick t 接受後，t+20 起才可再次接受 |
| 射程 | 100 世界單位 |
| 新動作有效期限 | 250 ms，含邊界；超過即過期 |
| 後座／散布／爆頭 | 後座只改武器呈現；無散布、無爆頭倍率 |
| HP=0 | 保留移動／射擊；後續命中實際傷害為 0；重入恢復滿血 |

每 Authority Tick 先處理加入／離開與所有玩家移動，再處理動作。新的動作依首次
交接到 Match 的 Authority Tick、PlayerId、ActionId 排序；相同 Tick 保持穩定順序。
亂序不要求等待缺號，缺號也不阻塞世界；不以封包數多走世界或補開歷史射擊。

射線起點為該 Tick 射手權威腳點＋Arena eyeHeight，方向來自本次請求的絕對
yaw／pitch。角度為 radians，有限值及合法範圍沿用產品視角驗證；不接受 Client
位置、傷害、命中結果或模擬時間。目標用 Arena 的直立身體膠囊，排除自己；
牆 AABB 及地板遮擋，同距離遮蔽物優先。命中查詢不回退任何人的位置。

新動作先通過時間及冷卻驗證，接受後消耗冷卻，即使未命中也一樣。拒絕不消耗
射擊效果或冷卻。傷害為 min(25, 目前 HP)，最低 0；HP=0 的玩家仍可被命中且實際
傷害為 0，不引入死亡／重生、副作用重播或累積補射。

## 6. 有效期限的可信時間來源

ShotRequest 的 `observedAuthorityTick` 表示 Client 最近觀察到的 Snapshot Tick，
不是要求在哪一 Tick 執行，也不是 Client 產生動作的可信時間。

「發布」明確指 Host 將完成的 owning Snapshot 放入供 IPC 取得的 publication
handoff。以該動作當時的 Host steady_clock 記錄 Tick／時間，最多保存最新 64 筆；
同一 Tick 的時間一旦建立，不能因 TakeSnapshot、編碼、重送或 socket 可寫而刷新。
catch-up 沒有發布的中間狀態不能另造可供參考的 Tick。這不聲稱 Client 實際收到。

對尚未裁決的新動作，Host 在交給 Match 裁決時提供參考是否有效及自發布起的年齡。
Match 使用 CombatRules 的 250 ms 政策判斷；沒有／已淘汰／尚未發布的 Tick 明確
拒絕。不能改用收到請求的時刻起算，也不能用 15 個模擬 Tick 代替 wall-clock 年齡，
否則主迴圈停頓／掉時會使過期操作重新有效。Domain 不自行讀時鐘。

已有帳本紀錄先驗證內容是否相同，再返回原裁決；重送不刷新或重算期限。對仍待
裁決者保持原請求及時間錨點，無法取得有效錨點時不能使用「現在」作替代。
此保守期限包含快照到達與請求返程時間，適用本輪 LAN 目標，不是完整時鐘同步、
反作弊時間證明或命中補償。未來改回溯模型時須另改契約與驗收。

## 7. 動作帳本、窗口與確認

每位玩家獨立保存 request／pending／decision。令 R 為權威已確認並退休的連續
ActionId 下界，初值 0；窗口同時限制 **數量最多 32，且 R < id ≤ R+32**，檢查加法
溢位。每次發布／重送最多八筆，與移動的 12／32 限制各自獨立。

```text
new immutable request -> reserved pending slot -> accepted / rejected decision
                                                   |
                           resend same decision <--+ (until confirmed)
                                                   |
                contiguous decision ACK -> retire entry, retain floor R
```

- Client 在可提交新動作時才分配連續 ID；冷卻預判、窗口滿或未捕捉輸入不產生缺號。
  已分配且提交的 ID 不因本機逾時／失焦刪除或換 ID，持續重送至取得裁決或生命週期結束。
- 批次先驗證格式、ID、窗口及不可變內容，再原子合併。相同 ID／相同內容為無操作；
  內容衝突整批拒絕，不能改原紀錄或使批內其他新請求部分生效。
- 在接受新請求前預留裁決空間；pending＋未確認裁決合計最多 32。超窗／容量不足
  是傳輸接納問題，無遊戲效果、保持原 ID 重試，不能偽裝成已完成的權威裁決。
- Match 不因缺 id1 而不能裁決 id2，但 Client 不能 ACK-through=2 跨過尚未取得
  裁決的 id1。過期與冷卻拒絕也是終局裁決，能參與連續 ACK。
- Client 只有在裁決已可靠保存在有界本機接收狀態、且能交給遊戲消費後才確認，
  不可 ACK 後讓主執行緒停頓／丟棄歷史導致結果遺失。主執行緒未消費的結果也須
  納入本機容量控制。Gateway 不可自行用「已轉送」代替 Client 確認。
- Match 只接受不跨過未裁決缺口的單調確認下界，退休前保留不可變原裁決。ACK 丟失
  時 Client 重送 ACK，Match 重送結果；舊／重複 ACK 無副作用。
- `id ≤ R` 永不重做，結果已回收時回報退休下界，不臆造新的接受／拒絕。未確認
  的結果不得按時間默默淘汰；正常回收只依確認或玩家生命週期結束。
- 拒絕原因至少可區分冷卻、過期、無效時間參考；未命中屬接受結果。非法 Session
  由邊界拒絕，不為未認證者建立帳本。格式／衝突錯誤不覆寫原裁決。

裁決記錄 ActionId、裁決 Tick、接受／拒絕、原因、命中種類、目標 ID 與實際傷害。
Client 逾時只能顯示待確認，不能認定未執行。不是持久交易：IPC／Session 失效後
依既有生命週期清場，不提供跨重啟恢復或保證舊動作成功。

## 8. 訊息與傳輸語意

第 03 批已建立 `client_v4.proto`／`runtime_v4.proto` 及 bindings，兩份仍獨立，
以產品 Adapter 轉換；不建立共用遊戲 schema 或通用可靠通道。
完整 wire 配置與生成方式見 [protocol README](../../apps/object_fps_pvp/protocol/README.md)。

| 訊息 | v4 語意 |
|---|---|
| Ready／Welcome | 延續 arena／cadence／身分，增加 Match 的 CombatRules |
| PlayerInput | 保持既有不可變移動窗口與 movementEpoch |
| ActionBatch | 最多八個 ShotRequest＋裁決連續確認下界；允許純確認 |
| ActionResults | 最多八個不可變裁決＋權威退休下界 |
| WorldSnapshot | 移動集合之外附獨立 CombatState 集合（玩家、HP、下一個可射擊 Tick） |

Client 射擊請求不帶任選可信 PlayerId，Runtime 由已驗證 Session 映射玩家。HP／
冷卻只用最新權威狀態，不做位置插值，不被本機移動 replay 覆蓋，晚到裁決不倒退 HUD。
血量可從新快照恢復，不表示裁決可以只放在一幀快照內。

可合併完整狀態、不可任意丟失動作、需確認裁決及 Join／Leave 有序控制各有交接
責任。Host／Gateway 依 ActionId 保留裁決，不能套用 snapshot-only latest-wins
或依賴目前可能丟失控制通知的佇列。部分寫出的 TCP frame 仍完成或連線失敗。
I/O、序列化與診斷寫檔不能阻塞世界 Tick，不因這個產品增加公共可靠通道框架。

動作／確認共用額外最多 30 Hz deadline；有資料才啟動，立即可送仍受同一預算
限制，錯過期限不補發。結果最多 30 Hz，每批八筆，循環覆蓋完整未確認窗口，不能
只重送最後一筆。每次重送新 PacketSequence。移動原排程不變。

現有公共 Session 限流是固定一秒窗口、合計最多 120 包；Hello、Input、Action 與
無效／重複的已認證流量皆可能計數。正常 60＋30＋約1 Hz 需在實際計數窗口驗證，
不能只用平均值宣稱安全，也不提高公共上限。最大合法欄位序列化必須驗證完整
UDP ≤1,200 bytes，不以平均大小或 protobuf payload 大小代替。

## 9. 本機回饋與生命週期

本節操作／槍模已於第 04 批實作；第 03 批的傳輸狀態與契約保持不變。
Client 在加入前準備模型／材質／GPU pipeline，權威裁決不重播本機動畫。

槍模使用現有 Mark23 的 Idle／Draw／Shoot，進入可操作世界前準備必要資產。
取得捕捉的首次點擊不射擊，後續左鍵上升沿產生一次動作；按住不連發，一幀多個
固定步不能重複消費操作或滑鼠 delta。

動畫／武器後座立即本機播放一次；送包、ACK 與移動重播不再播放。接受且命中
玩家的裁決才出命中標記；拒絕顯示短狀態，不倒放動畫。視覺槍口不是權威射線
來源，不能讓骨骼動畫、FOV 或 Renderer 參與 Match 命中計算。

| 邊界 | 動作／戰鬥狀態 |
|---|---|
| 失焦、Tab、拖窗 | 不產生新動作；既有待裁決／重送／確認繼續 |
| 主執行緒停頓 | worker 存活，保活與裁決保存繼續；不補造射擊 |
| movementEpoch 更新 | 保留動作、裁決、HP、冷卻，不跨移動世代復活舊命令 |
| Esc／Leave／斷線／換 Session | 依既有清理屏障結束玩家生命週期，清動作及呈現 |
| 新 PlayerId Join | 新 ActionId 流、滿血、乾淨帳本 |

## 10. Architecture Delta 與驗收狀態

功能壓力是射擊需要不可重複的終局裁決，而現有移動 ACK 包含替代步、epoch 會
重設，Snapshot 又可合併；只在既有移動封包加一個 fire bool 無法滿足契約。

受影響者為產品 domain、Client、Host、Gateway Adapter、兩份 schema 及 owner
驗收。新增射擊政策、帳本及戰鬥狀態歸 Match；即時回饋歸 Client；發布時間與
有界交接歸 Host。第 04 批 Client 已使用既有模型／圖片／動畫能力，Match 不連
Renderer。依賴方向不變，無新產品間依賴或 Top-level subsystem。

第 01 批只有文件與既有 CPU 回歸，**沒有實際 API／wire／build graph 變更**。
第 02 批加入普通 C++ 型別，第 03 批已切 wire v4，整套拒絕 v1–v3；第 04 批接入
槍模，第 05 批短測與人工指南。新增能力仍須可隨 owner 內容與 registration 移除。

第 02 批的實作入口為產品 `Combat.hpp`、`ShotQuery`、`PvpMatch` 與
`MatchRuntimeHost`。`WorldSnapshot.combat` 與移動分開，裁決另以
`GetActionResults` 取得 owning copy，讀取不確認、不回收。第 03 批 serializer 已
傳送戰鬥狀態與獨立裁決；Host 的 `SubmitActionBatch` 原子處理 shots＋ACK／純 ACK，
有效 ACK 可為同批新請求預留容量，真正退休及交付仍在 Tick 邊界生效。
Client Drain 只交付一次裁決，只有已交付的連續 ID 才可確認；snapshot history
溢位或 movementEpoch 不清帳本。來源與接續注意點見 [交接](plans/v4/HANDOFF.md)。

第 03 批 C++、Go race、worker／wire、16 案真 socket 共存與恢復，以及既有移動
網路短回歸已通過。射擊壓力下移動 Actual 100%，執行延遲 P95 45.86 ms；
故障解除後的新裁決及持續移動恢復均符合 1.5 秒。Gateway 停頓一秒案例有 118 次
既有限流拒絕並完整恢復，未提高 120 包／秒上限；正常共存零限流拒絕。
完整指標、原始失敗與重核依據見
[第 03 批 dev_log](../dev_logs/2026_09_28_pvp_v4_batch03.zh-Hant.md)。

驗收門檻、目視清單與手動長測規則集中在
[第 05 批](plans/v4/05-short-validation-and-manual-test-guide.md)。不降低原移動門檻；
第 04 批的 30／60／144 FPS、GPU 圖像與 16 秒／20 事件雙 GUI 短測已通過，
跨視窗 P50／P95 為 48.43／48.84 ms。詳見
[第 04 批 dev_log](../dev_logs/2026_09_28_pvp_v4_batch04.zh-Hant.md)。
第05批追加完整驗收已通過：三輪120秒GUI共600個移動事件／450槍，
60／144Hz各30分鐘共17740個唯一接受裁決，原生X11 V1–V8及HUD圖像核對通過。
依此升格v4；詳見 [穩定基線](plans/v4/STABLE_BASELINE.md) 與
[驗收狀態](plans/v4/ACCEPTANCE_STATUS.md)，不以歷史v3結果抵銷本次長測。

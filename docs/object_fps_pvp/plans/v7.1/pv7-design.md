# pv7-design：pv7 契約的設計

更新：2026-10-15。Owner：`object_fps_pvp`。**狀態：D-1～D-7 已決定（[交接](HANDOFF.md) D95）；草案已依 D94（Gateway 回到無狀態邊緣）修訂。下一步是「IPC 送達語意」的 xhigh 對抗式檢查。**
產出：[protocol-v7 草案](../../protocol-v7.zh-Hant.md)。本文件記錄這一批的過程、待決事項、需要新量測的設定、各批的測試清單與重估的工時。
決策編號 D-1～D-7 只在本批使用（決定後轉成交接的 Dxx）；和 v7 交接的 D1～D85 不同。

## 過程

- 檔位（D92）：ultracode。3 位設計者（和主對話同檔）、1 位評審、1 次對抗式檢查，另加 1 次 xhigh 對抗式檢查。約 84 分鐘。
- 三個設計方向與評審的分數：

| 方向 | 分數 | 評審的評語（摘要） |
|---|---|---|
| 最小改動 | 7 | 負擔最低，P3 搭在既有的 Hello／Welcome 往返；但漏掉「心跳回覆會刷新 runtime 活性」，`view_tick` 的解碼驗證會改變凍結 action_probe 的歸屬 |
| 語意分離 | 7.5 | 最守綁定決定、證據最多；但 P3 新增 GYOP 種類，time-echo 承擔不了；把傳輸中繼資料放進不可變請求 |
| 以可診斷為主 | 6 | 歸因最完整；但範圍超出檢測清單，負擔最重，有 Client 時間上 wire 的風險 |

- 合成：以「語意分離」的規則骨架為底（診斷沒有否決權、診斷訊框不刷新活性、跨時鐘域只傳照抄的時間戳或單域持續時間、DropReason 一值一鍵），P3 載體、對稱的 ClockSample、A1 不新增拒絕值改採「最小改動」，`maximum_shot_damage`、依 ActionId 裁決、丟棄樣本帶身分範圍取自「以可診斷為主」。

## 兩次檢查與處置

對抗式檢查：必須修正 3、應修正 5。xhigh 檢查：必須修正 2、應修正 7。全部已套用到草案，或轉成下面的待決事項。

| 檢查提出的問題 | 處置 |
|---|---|
| 「以剩餘時間呼叫 Read」在剩餘時間 ≤0 時會取消期限，心跳回覆讓 link 永遠不失效（必須修正） | 條文：剩餘 ≤0 不呼叫 Read，直接走逾時失效；加假時鐘案例與突變（§2） |
| Client 以 409 判定版本不符，會把 `room_full` 誤判（必須修正） | 改以 body 的 `error` 判斷，不看狀態碼；加反例測試（§1） |
| 丟棄回報的 L2 等式做不到：Gateway 被 SIGTERM 時 final 送不出去（必須修正） | 優雅關閉時先寫出所有串流的 final；Gateway 每寫一則回報記一行；L2 改為依 `report_sequence` 逐則相等（§4） |
| D32 的守衛放錯檔案、突變沒有 oracle（必須修正） | 結構隔離（估計器只在 I/O 層）、守衛擴大、差分測試當 oracle（§2） |
| 送出迴圈中途的 snapshot 取代被併進 Gateway 範圍的 coalesced（必須修正） | 拆成兩個值：45 只計讀取端；新增玩家範圍 31 `DROP_REPLACED_SNAPSHOT_PACKETS`；`gateway_hold_us` 每位玩家各自填（§4、§5） |
| dwell 的起點有兩種、終點在 `take()` 會少算寫出阻塞 | 命令與動作類一律以收包為起點；終點改為框架開始 Marshal 的時刻；各原因的起點列在表中（§2、§4） |
| 回報由誰組裝、鎖順序未定 | Server 擁有的診斷 goroutine，先 `s.mu` 再 `l.mu`，放進 link 的單槽診斷佇列（§2） |
| final 的數量沒有上界（未認證的 join／leave 可以推高） | 只有非空串流送 final；每次最多 MaxPlayers×2 條；延後佇列滿時併入 `merged_streams`（新欄位）（§4） |
| IPC 版本不符時 Match 只看得到 eof | 偵測方關閉前盡力寫出只含版本的 envelope（§1） |
| 凍結 pv7 工具「逐值一致」照字面做不到；`run_quad` mixed 依賴舊字串 | 改為「除事前列出的版本比對項之外逐值一致」；Ready 的錯誤保留既有前綴；`run_quad` mixed 在 version-errors 改寫或退役（§1） |
| 解碼層要求 E≥1 會在 A1 之前丟掉點擊 | 拿掉；E＝0 走 pv6 的到達判定（§6） |
| 「準時路徑下起點和 Client 一致」在命令 S 被替代時不成立 | 條文改為 T(S) 只由序號決定；影子紀錄加命令 S 的來源與是否晚到（§6） |
| 回溯公式用全域游標，v8 會取到目標出生前的姿勢 | 改成界限加政策：J−R ≤ τ ≤ J，可逐目標夾到目前生命的起點，跨生命的命中不得傷害目前生命（§7） |
| NACK 只到 Gateway，v7.1 沒有消費端 | 轉成 D-6 |
| 各批的內容超出 README 的工時 | 見下面的重估 |
| 其他小項：Leave 原因的說明、409 多餘的 `received_protocol_version` 鍵、FireGate 突變要一起退役、E 與 S 要取自同一個模擬步、`match_hold_us` 的時鐘來源、DropSample 的序號範圍不連續 | 都已改進草案（`received_protocol_version` 拿掉；DropSample 加 `item_count`） |

## 決定（原「等你決定」）

2026-10-16 依 D95 決定：D-1 不再需要（D94 讓 Gateway 的帳本整個移除，動作由 Match 判斷並依原因計數）；D-2～D-6 照建議；D-7 照建議，第 2 項改寫為「逐命令的完整丟棄身分」。下面保留決定前的說明，供日後回顧。

### D-1 A3／A5：只計數，還是轉送給 Match

- **機制**：A3 是 Gateway 用 session 帳本（Match 裁決的副本）驗證失敗，整批丟掉（`action_delivery.go:374-380`）；A5 是 link 的動作視窗拒收（`:381-390`）。兩者都是可重送的接納失敗，Match 對同一批也會做同樣的接納檢查（`PvpMatch.cpp:94-137`）。D49 的判斷規則是「這是執行，還是決策」。
- **選項**：(a) 只計數並回報（DropReason 16、17、20、21），條文寫成「依 Match 契約執行的守門，並且回報」；(b) 新增動作的拒絕線，原樣轉送給 Match 判斷。
- **取捨**：(b) 最符合「決定權在 Match」，但要新路徑、新守恆與突變，而 Match 得到的是同樣的拒絕結論；(a) Match 知情，不改路徑。
- **建議**：(a)。

### D-2 A1 的 E、S 在哪一批上 wire

- **機制**：E、S 在 `hitbox-fields` 就上 wire 的話，Client 照實填、PvpMatch 不讀，Host 對每個裁決寫影子紀錄（若採 A1 會走的路徑、需要的冷卻保留量）；判定與 digest 都不變。`pv7-checkpoint` 就能用凍結 pv7 工具量出退回率，正好是決定 K 與 S 取法需要的資料。
- **選項**：(a) `hitbox-fields` 一起加入 E、S 與影子紀錄（該批範圍改為「射擊請求欄位」）；(b) 照 README，E、S 與判定都在 `a1-shot-step`。
- **取捨**：(a) 在行為改變之前就有資料，`hitbox-fields` 多約 3 h；(b) K 只能憑推論宣告。
- **建議**：(a)。

### D-3 A1 退回路徑的參考年齡與冷卻保留

- **機制**：pv6 在第一個 Tick（A）呼叫 referenceAge（`PvpMatch.cpp:290-291`、`MatchRuntimeHost.cpp:421-427`）。A1 把判定延到 T(S)，若改在判定時量，保留時間會吃掉 250 ms 的一部分。上一發若走退回路徑、晚 d 個 Tick 判定，下一發的 T(S) 可能早於 nextAllowedShotTick，現行規則會直接回 Cooldown（`PvpMatch.cpp:310-311`）。
- **選項**：(a) 參考年齡在 A 量並存下來；冷卻差距 ≤K 時保留到 nextAllowedShotTick 再完整重判；K 在 `a1-shot-step` 依影子資料提案、由你決定；(b) 參考年齡在判定時量，冷卻同 (a)；(c) 參考年齡在 A 量，不保留（K＝0）。
- **取捨**：(a) Expired 的語意與 pv6 完全相同，退回後的連點不會被冷卻壓縮；(b) 保留期間會讓更多射擊 Expired；(c) 最簡單，但退回後的下一發可能被判 Cooldown。
- **建議**：(a)。

### D-4 端到端 snapshot 年齡：持續時間上 wire，還是只能離線

- **機制**：snapshot 年齡由四段組成：Match 停留、IPC、Gateway 停留、網路。P2／P3 只提供兩段傳輸，兩段停留只有在發生的地方量得到（原始需求見 v7 交接的第 10 批「snapshot 年齡」）。
- **選項**：(a) `WorldSnapshot.match_hold_us`、`gateway_hold_us` 上 wire，Client 可以線上逐份分段；(b) 不加欄位，用 snapshot tick 串接 trace、以 P2／P3 的 offset 離線換算，Gateway 段只有本機日誌。
- **取捨**：(a) 4 人 Snapshot 多 18 B（型別最大 1155／1200），`cause-records` 多約 3 h；(b) 只能離線，精度約兩段最小 RTT／2 的和。
- **建議**：(a)。

### D-5 診斷值違規時要不要關閉 link

- **機制**：既有 IPC 遇到解析失敗或未知訊息就關閉 link（`IpcHost.cpp:385`、`runtime_link.go:652-658`），房間隨之 unavailable。但另一個程序送來的資料屬 Runtime Error（`docs/architecture/error-handling.md`），而 v7.1 的前提是觀測不能改變行為。
- **選項**：(a) 診斷沒有否決權：語意違規只記錄、計數、忽略；結構違規（解析失敗、未知 oneof、版本不符、方向錯誤）照舊關閉；(b) 診斷的結構與語意違規一律關閉 link（Leave 除外）。
- **取捨**：(a) 觀測絕不讓房間斷線，但 Gateway 的計數 bug 可能只留下一行日誌，要靠驗收分析器把異常計數 >0 判為失敗；(b) bug 會立刻浮現，但觀測可能讓房間斷線。
- **建議**：(a)。

### D-6 動作拒收回覆的形狀

- **機制**：Match 整批拒收動作時目前不回覆，Client 與 link 會一直重送到 Leave。合成設計原本只把回覆送到 Gateway（新訊息 `ActionBatchRefused`），檢查指出：Match 本來就依原因計數，送到 Gateway 只多了 action id 範圍；真正需要 wire 的是「讓 Client 知道並停止重送」，若現在不定，之後幾乎一定要再改一次 wire。
- **選項**：(i) 回覆送到 Client：runtime 與 client 的 `ActionResults` 加 `refused_batches`、`last_refusal`（runtime 4、5；client 3、4），Gateway 原樣轉送；v7.1 只記錄，之後要讓 Client 停止重送只改行為、不改 wire；(ii) 不上 wire：Match 日誌補 action id 範圍；之後要讓 Client 停止重送，就是需要同意的 wire 變更；(iii) 照原設計，只送到 Gateway。
- **取捨**：(i) 你排的順序裡，這項的目的是「讓送方知道為什麼被拒」，(i) 直接做到，約 +2 h，ActionResults 每則多 ≤13 B；(ii) 最省，但送方仍不知道；(iii) 付了 wire 的成本，卻沒有消費端。
- **建議**：(i)。

### D-7 確認 pv7 的排除清單

- **機制**：pv7 是 v7.1 唯一一次 wire 變更。草案 §10 列出 8 項刻意不上 wire 的項目：B7／W11 的 slack 承載、D49⑤ 的品質免除與逐命令身分、Input 帶 Client 送出時刻、ShotDecision 帶判定路徑、v8 的武器／出生保護／回合／隊伍欄位、輸入防偽造、房間列表的版本鍵與 HTTP leave 的原因、DropReason 的新值。
- **選項**：(a) 確認排除：之後要做任一項，都是需要同意的 wire 變更；(b) 指定其中幾項現在加入。
- **取捨**：排除 B7 代表 re-evaluate 若選了「到達即發布」，就要徵求同意再改一次 wire；加入則要現在定語意，而 B7 的效果還沒評估。DropReason 的新值：Gateway 之後新增丟棄出口（例如 re-evaluate 後的修正）就要徵求同意；接收端已依 D-5 容忍未知值，風險低。
- **建議**：(a)。

## 需要新量測的設定（都不是 wire 欄位）

| 設定 | 為什麼現在定不了 | 在哪裡量 |
|---|---|---|
| K（A1 冷卻保留的上限） | 取決於退回遲到的分布與「退回之後下一發落在冷卻內」的頻率，目前只有推論 | `pv7-checkpoint` 的 L2（clean、network20／40、upstream、Gateway→Match 延遲注入）；在 `a1-shot-step` 的宣告時交你決定 |
| Client 的 S 取法（N 或 N＋1） | Client 的 33 ms 共用期限與 link 的 30 Hz 節拍可能讓射擊晚於 T(S)；W＝13 兩種都涵蓋 | 同一份影子資料：clean 的退回率（目標 0）與準時餘裕 |
| 時鐘跳變門檻、漂移回歸視窗、Gateway 時鐘在 macOS 睡眠時的行為 | 同機的抖動與睡眠後的跳變都沒有量過 | `heartbeat-sync` 的 L2（只記錄，中途做一次系統睡眠）；跨機器在 `lan15-windows` |

## 各批的測試清單

- **共通**：每個新訊息與欄位都有 Go↔C++ 的跨語言 golden；proto 改名後的往返測試；§2 的原始碼守衛與差分測試。
- **version-errors**：四處定義＝7；HTTP、UDP 標頭、IPC 三處都拒絕 1～6 與 8（集合由常數導出）；409 body；Client 的錯誤以 `error` 判斷（`room_full` 的 409 不得出現 protocol_version；400＋protocol_version 的 worker 案例）；IPC 的錯誤寫出期望值與收到值，並且和解析失敗分開；假 Gateway 送 version＝8 的空 envelope 時，Match 的 log 含 expected 7 received 8；既有 close 字串不變；`09-version-python` 突變不是 stale；凍結 pv7 工具在 pv6 語料上，除事前列出的版本比對項之外逐值一致。
- **heartbeat-sync**：四時間戳公式的單元測試（假時鐘：offset、delay、保留時間、負 delay、跳變）；origin 比對最近 8 個；Match 回覆單槽只回最新一則；回覆排在 snapshot 之後、不佔 controls；有無心跳時 action batch 的寫出時刻、順序與 `take()` 次數都相同；活性：診斷回覆恰好在 5 s 邊界前到達時，link 仍在 5 s 失效（假時鐘）；突變「剩餘 ≤0 仍呼叫 Read」「心跳走 `l.wake`」「心跳刷新活性」「offset 進參考年齡或品質」都要被殺，後兩者用差分測試當 oracle。
- **time-echo**：Hello／Welcome 樣本的比對；樣本不合法時 Hello 仍維持 liveness 與 Join、Welcome 仍設定 welcomed；Welcome 遺失不影響連線；`ReadyFitsWelcome` 計入 ClockSample；有回聲流量時，凍結 ingress 分析器的 IG、gateway_global、T3 仍然精確。
- **drop-report**：DropReason↔ingress 鍵的 golden（Go、C++、Python 讀同一份）；每個 reason 一個單元測試（出口恰好加一，且出現在下一則回報）；累計不倒退、只在變化時送、每秒 ≤1、final 恰好 1 則、樣本 ≤4、`samples_omitted` 正確、身分欄位符合 reason；10 秒歸零的 ingress 行與累計值由同一入口更新；下行新計數（注入寫出錯誤、讓 control 佇列滿）；送出迴圈中途注入新 snapshot：玩家 1 已寫出不計、玩家 2～4 各計 1、45 不變；1000 次 join／leave 之後每秒的回報數不超過上限；優雅關閉時所有非空串流都送出 final；鎖順序與 `-race` 測試；runtimeWrite 的替身讓第一則阻塞 500 ms，第二則的 dwell ≥500 ms；Match 端違規依 D-5 處置；最大組合 ≤MaxFrame；突變「拿掉任一出口的計數」「回報進 TrackSlack 或品質」都要被殺。L2 事前宣告：Match 收到的回報與 Gateway 寫出的回報依 `report_sequence` 逐則相等。
- **gateway-duties**：Go 測試與 race、digest 不變；A3／A5 的條文和程式一致。
- **cause-records**：dwell（假時鐘）：合併窗口取最舊、切段同值、拒絕線、換代移線、ActionBatch 只算第一次取走的請求、10 s 夾住、寫出阻塞；Leave 原因：三條路徑各帶正確的原因，UNSPECIFIED 時照常移除；動作拒收回覆（依 D-6）：每種拒收產生對應的原因（包括 Resetting）、累計正確、拒收會觸發結果送出；hold（依 D-4）：Match 段的起點＝發布時刻、被取代的 snapshot 不帶、Gateway 段每位玩家各自填、超界只記錄；突變「dwell 或 hold 進 slack／品質」「dwell 進 EqualShot」都要被殺。
- **hitbox-fields**：HitPart 的正反例（Gateway、Client、EqualDecision）；`last_hit_part` 的四欄規則；`maximum_shot_damage` 的三條規則與 damage 上限；Match 只產生 BODY；`view_tick_q16` 的紀錄性檢查（0、邊界、溢位、Reload≠0），永不拒絕，而且取的是全域游標（遠端剛重生的案例）；`EqualShot` 與 `operator==` 包含欄位 7～9；同一請求帶不同的 E、S、`view_tick_q16` 時，裁決逐欄相同；E 與 S 取自同一個模擬步；預測未啟用時點擊照樣送出（E＝0）；凍結 action_probe 的 duplicate-reorder-conflict 判定不變；依 D-2 的影子紀錄。
- **pv7-checkpoint**：四平台 L1；用凍結 pv7 工具和 v7 基準比較，有差異時在線內 commit 之間二分；產出 K 與 S 取法的影子資料（事前宣告＋D45 檢查）。
- **a1-shot-step**：準時 J＝T(S)、late／epoch／life 退回、上限 A＋13、epoch 未開始、S＝0、命令 S 被替代、冷卻保留 ≤K 與 >K、保留後重判、每位玩家依 ActionId 的順序（含 Reload 排在保留中的 Shot 之後）、同 Tick 跨玩家的順序、參考年齡在 A 量；不變式：同一 epoch 內照序號閘門送出時權威 Cooldown＝0；預測凍結時不送；新的 digest 情境；golden 依事前宣告重產並保留舊檔；`FireGateTests` 換成序號閘門的測試，`mutations.json` 中 FireGate 的突變一起退役或改寫；L2 事前宣告＋D45；L3 你的連點手感。

## 重估的工時

補充（2026-10-16）：D94 改為無狀態邊緣之後，pv7-line 改為約 170～179 h〔推論〕，各批見 [README](README.md)「pv7-line 各批」與 [gateway-direction](gateway-direction.md)。下表是 D94 之前的重估。

合成設計裝進各批的內容超出 README 的估計（對抗式檢查的應修正項）。依草案重估：

| 批次 | README | 重估 | 增加的原因 |
|---|---|---|---|
| pv7-design | 14 | 14 | 已用約 3 h，剩你的審核與草案修訂 |
| proto-rename-v7 | 3 | 3 | |
| version-errors | 10 | 12 | 版本不符的告知 envelope、分析器的例外清單、`run_quad` mixed 改寫 |
| heartbeat-sync | 5 | 9 | 活性規則與假時鐘測試、Match 診斷槽、估計器、D32 的結構隔離與差分測試 |
| time-echo | 3 | 4 | |
| drop-report | 10 | 16 | 37 個原因、累計值與 10 秒行同入口、診斷 goroutine 與鎖順序、final 與上限、優雅關閉、回報日誌、窗口收包時刻的追蹤 |
| gateway-duties | 6 | 6 | |
| cause-records | 6 | 9 | dwell 終點、每位玩家的 hold（D-4）、動作拒收回覆（D-6） |
| hitbox-fields | 6 | 9 | E、S 與影子紀錄（D-2） |
| pv7-checkpoint | 6 | 7 | 影子資料的分析 |
| a1-shot-step | 20 | 20 | |
| pv7-merge | 2 | 2 | |
| **合計** | **91** | **111** | |

若要壓回去：D-4 選 (b) 約省 3 h、D-6 選 (ii) 約省 2 h、D-2 選 (b) 約省 3 h（但 K 只能憑推論）。

## 風險

- A1 會改變手感：點擊到裁決多約 1～2 Tick〔推論〕。盲測已取消（D91），只能靠 L3。
- 退回率要到 `pv7-checkpoint` 才知道；D-2 (a) 能在行為改變之前量到。
- S 是 Client 的宣稱：作弊端可以把自己的動作延後到 [A, A＋13] 內任一點，但不會早於 pv6。
- 4 人 Snapshot 只剩約 45 B 的餘裕；v8 不改契約，預算到 pv8 才需要處理。
- 依 D-5 (a)，Gateway 的計數 bug 可能只留下一行日誌，要靠驗收分析器把異常計數判為失敗。
- proto 改名是數十個檔的機械變更，和同一條線上的其他批次有衝突風險。
- pv7 之後，凍結的 pv6 工具讀不了新跑次；v7final 與 c2-after 兩個 worktree 要保留到 `d44-regression-pv7`。
- `R` 的上限綁在 `maximum_reference_age_ms`，等於先替 v8 定下回溯上限的語意；v8 若需要更長的回溯，就要改契約。

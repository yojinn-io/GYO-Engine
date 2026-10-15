# Gateway 的方向評估

更新：2026-10-16。Owner：`object_fps_pvp`。**狀態：已決定。D-8：(a) 無狀態邊緣，順序守住 D90（[交接](HANDOFF.md) D94）；D-9～D-15 照建議（D95）。內容已寫進 [protocol-v7 草案](../../protocol-v7.zh-Hant.md) §2a 與 [README](README.md)。**
起因：[設計事故 2026-10-15](Incident/2026-10-15-gateway-state-drift.md) 行動項目 1；[交接](HANDOFF.md) D93。決定方向之後，再重評 [pv7-design](pv7-design.md) 的 D-1～D-7 與 pv7-line。
程式行號以 `95bac31` 為準。標〔推論〕的是沒有實測的估計。

## 過程

- 檔位（D93）：ultracode。3 位評估者各負責一個方向，1 位評審，1 次對抗式檢查；都和主對話同檔。約 46 分鐘。
- 主對話另做一次預查：v7 的 L2 語料（`pvp-v7-16-l2-20261015`、`pvp-v7-16-l2b-20261015`、`pvp-v7-08c-l2-20261011`，共 445 份 Gateway 日誌）中，有狀態 Gateway 才有的出口計數。

## 機制：問題出在哪裡

外部的 UDP 有損、會重複、會亂序；內部 IPC 原本假設每個命令與裁決恰好送一次。兩者之間一定要有人去重與重送，而去重要知道「Match 已經有什麼」。三個方向的差別，只在這份可靠送達的狀態放在哪裡。

- 評估者核對過：Match 本來就對輸入與動作冪等（輸入：`PvpMatch.cpp:59-77`、`MatchRuntimeHost.cpp:172-192`、`:431-449`；動作：`PvpMatch.cpp:94-137`）。Gateway 的去重原本只是減少 IPC 量的最佳化。
- 真正把 Gateway 綁成有狀態的，是**結果的可靠送達**：Match 每個裁決只送一次（`IpcHost.cpp:138-145`），靠 Gateway 的 session 帳本對 Client 重送（`action_delivery.go:279-340`）。

## 預查：有狀態的出口不是 0

| 出口 | 合計 | 主要出現在 | 意思 |
|---|---|---|---|
| `drop_rejected_lane_overflow_commands` | 1488（16-L2 1188、08c-L2 300） | backpressure、gateway-1000ms、08c session | 拒絕線溢出，命令被丟，Match 不知道 |
| `fwd_rejected_lane_input_packets`（＝`fwd_routed_epoch_mismatch_inputs`） | 4140 | backpressure、downstream-1000ms、respawn | 依副本判斷 epoch 不符，走拒絕線 |
| `drop_invalid_actions_*`（A3） | 42 | duplicate-reorder-conflict | Gateway 依帳本副本丟掉動作 |
| `drop_stale_sequence_actions_packets` | 436 | network0／20／40、08c session | 舊序號的 Actions 被丟 |
| `fwd_written_duplicate_commands` | 約 201 萬（每個 clean 跑次約 1.15 萬） | 所有案例 | Gateway 擋掉的重複命令（不是損失）。(a) 之後改由 Match 處理 |
| `drop_old_tick_snapshots`、`drop_regressed_snapshots`（D1、D2） | 0 | — | (a) 下可以移除的 snapshot 守衛 |

結論：有狀態的出口在實際跑次中會發生，其中一部分是 Match 不知道的丟棄。評審提出的「全部為 0 就改選 (c)」條件不成立。另一方面，(a) 會讓 Match 多處理每個 clean 跑次約 1.15 萬個重複命令，負載要在 `edge-input-passthrough` 的 L2 實測。

## 三個方向

| 方向 | 內容 | pv7-line 工時 | 評審分數 |
|---|---|---|---|
| (a) 無狀態邊緣（完整） | Gateway 只翻譯、逐則檢查、原樣轉送；去重、帳本、結果重送都在 Match（大多已存在，新增的是 IpcHost 的結果重送與補送）；Client、Engine、`services/gyo_gateway` 不改 | 155 h（評審）；對抗式檢查認為 163～171 h〔推論〕 | 7.5 |
| (a′) 無狀態邊緣，但結果帳本暫留 Gateway | 省約 15 h，但 Gateway 仍保存裁決並決定重送節拍，約束條文寫不出來 | 約 140 h | — |
| (b) 遊戲 UDP 直接到 Match | Gateway 只剩大廳與 session 發放；Match 直接面對公開 UDP；Gateway 約 2,500 行 Go 的語意要用 C++ 重做 | 約 171 h（156～186 h） | 5 |
| (c) 維持有狀態的 Gateway | 加封閉狀態清單（只准減少）與條文；有狀態的語意寫進 pv7；之後改方向要升 pv8，依 D87 最早在 v8 之後，估計多付 20～55 h〔推論〕 | 約 114 h | 5.5 |

### 取捨

- **(a)**
  - 好處：消除事故的結構根因；pv6 已知缺口 2、3、9 消失；pv7 的丟棄原因從約 37 個減到約 29～30 個；dwell 變成一對一；D-1 不用再決定；之後新增 Client 種類或 v8 的動作欄位，都不必碰 Gateway 的帳本。
  - 代價：工時增加約 50～60 h；三個 edge 行為批不經過權威 digest，只能靠 L2 前後對照歸因；pv7 的「只有 A1 改變權威結果」不再成立；Gateway 的 Go 測試與約 135 個突變要改寫。
- **(a′)**：省約 15 h，但根因只拿掉一半。
- **(b)**：結構保證最強，伺服器端只剩一個時鐘域；但成本最高、Match 直接暴露、切換時的行為改變拆不開，而且最不可逆。工時和 (a) 的差距在估計誤差內，所以排除它的理由是安全暴露、不可回退，以及檢測基準斷層，不是工時。
- **(c)**：交付最快、歸因最單純；但根因留著，有狀態的語意寫進唯一一次 wire，pv7 自己還會再加 Gateway 狀態（拒收狀態、丟棄串流）；v8 的動作規則要在 Go 與 C++ 兩處維護。選 (c) 時，原本「Gateway 無狀態」的意圖要你明確改成「取代」或「延後」。

### 長期成本的比較

- (c) 現在約 114 h，之後改走 (a)／(b) 時再多付 20～55 h，合計約 134～169 h，而且時點最早在 v8 之後。
- (a) 現在一次付約 163～171 h。
- 兩者的總量相近，差別在付的時點，以及這段期間 wire 上寫的是哪一種語意。

## 對抗式檢查的修正（選 (a) 時要套用）

1. **必須修正：結果的「最多 30 Hz」沒有執行點。** Match 改成在 ACK 之前每 33 ms 重送，Match→Gateway 一停頓，重送就會堆在 TCP 緩衝，放行後一次灌給 Client（host-ipc-1000ms 案例會違反「每秒 ≤31」〔推論〕）。修法：Gateway 對每位玩家只記「上次送出結果的時刻」（傳輸層的軟狀態），間隔內的結果丟棄、計數並回報；結果是冪等的，Match 下一個間隔會再送。條文改成「不保存裁決、不排程重送，只可依固定上限丟棄超額的外送並回報」。
2. **必須修正：評審建議的順序其實改了你的 D90。** 它把三個行為批排在丟棄回報（P4）之前。要守住 D90，P4 要先照現行出口實作，再做 edge 批，約 +5～8 h，好處是 edge 批的前後對照有丟棄回報可用。
3. 應修正：
   - edge 批的順序要對調（結果先、動作後），避免帳本在沒有 A3 時合併請求的中間狀態；
   - 從第一個 edge 批起，被刪路徑的 ingress v1 鍵照樣輸出 0，否則凍結分析器會停；
   - 丟棄原因實際約 29～30 個，逐出時 IPC 佇列裡的資料怎麼處理要定；
   - 結果直通批的 L2 要加射擊時序；
   - 白名單守衛要遞迴走訪欄位型別、掃描套件變數，依內容的判斷靠審查守；
   - D44 回歸改為 10～14 h。

## 等你決定：D-8 Gateway 的方向

- **選項**：(a) 無狀態邊緣（完整）；(a′) 結果帳本暫留 Gateway；(b) 遊戲 UDP 直接到 Match；(c) 維持現狀加棘輪。
- **建議**：(a)，順序守住 D90（P1 → P2 → P3 → P4 → edge 四批 → P5 → E2 → hitbox → checkpoint → A1 → merge），pv7-line 約 170～179 h〔推論〕。
- **會改變建議的證據**：
  - 改寫 IPC 送達語意時發現要改 Client 的行為或 wire 欄位（目前 Client 能冪等接受重送，`ClientConnection.cpp:360-396`）；
  - `edge-input-passthrough` 的 L2 顯示 Match 的負載明顯上升（每則 SubmitInput 都在 host mutex 內複製 snapshot，`MatchRuntimeHost.cpp:151`、`:166`）；
  - v7.1 有低於約 170 h 的硬性上限：改選 (c)（明確延後）或 (a′)；
  - 你認定原意只要求「不持有 Match 決策狀態」，並接受 Match 對外暴露：可以改選 (b)。
- **工時與停止規則**：(a) 的 pv7-line 約是 README 原估 91 h 的 1.9 倍，已超過 1.5 倍的停止規則，需要你明確接受。

### 選 (a) 之後的決定（2026-10-16 照建議，D95）

| 代號 | 內容 | 建議 |
|---|---|---|
| D-9 | pv7-line 的順序 | 守住 D90：P4 在 edge 批之前 |
| D-10 | IPC 的順序與背壓、結果 30 Hz 的執行點 | 同一玩家依到達順序；controls 不丟；資料每位玩家有界 FIFO，滿了丟最舊並回報；Gateway 只記上次送出時刻來守 30 Hz |
| D-11 | session 活性 | 解碼成功就提交，不看內容 |
| D-12 | snapshot 守衛 D1、D2 | 都移除（預查：兩者在 v7 語料中都是 0） |
| D-13 | 丟棄回報上 wire 或只寫 log | 上 wire（約 29～30 個原因） |
| D-14 | Gateway 約束的條文與守衛 | 允許的類別：連線與 session、生命週期、設定副本、有界佇列、診斷計數、逐則檢查；禁止 Match 決策狀態的副本與跨訊息、依內容的判斷 |
| D-15 | 兩份 proto 加 adapter | v7.1 保留，這就是「每種外部 Client 一個接口」 |

D-1～D-7 的變化：D-1 不用再決定（A3／A5 隨 Gateway 帳本一起刪除）；D-7 的排除清單第 2 項改寫；D-2～D-6 維持原建議。

## 證據位置

- 評估原文（三份評估、評審、對抗式檢查）：規劃會話的工作檔，不放進 repo；本文件是整理後的結論。
- 預查的腳本與結果：同上；計數來自 `build/target/_build/test/logs/` 下三個語料的 `gateway*.log` 中 `ingress statistics version=1` 行。
- 程式：`apps/object_fps_pvp/gateway/server.go:50-53`、`:574-607`、`:1000-1020`；`runtime_link.go:36-47`、`:223-340`、`:604-633`；`action_delivery.go:225-252`、`:279-340`、`:374-406`；`src/Pvp/IpcHost.cpp:138-145`；`PvpMatch.cpp:59-77`、`:94-137`；`MatchRuntimeHost.cpp:172-192`、`:431-449`。
- 契約：[protocol-v7 草案](../../protocol-v7.zh-Hant.md)；[protocol-v6](../../protocol-v6.zh-Hant.md) 已知缺口；[v7 交接](../v7/HANDOFF.md) D49（「Gateway 不一定要無狀態……但職責必須明確」）。

# rebind-on-new-port：換埠後的改綁時機與 Hello 間隔（2026-10-25 使用者決定，D187；審查後改為 D188：Hello 一律每 250 ms）

日期：2026-10-25。D181 新增的批次，處理 [silent-branch-asserts.md](silent-branch-asserts.md) 第 8 節的 K5、K6。PR #95，分支 `claude/pvp-v71-system-observe-test`。

## 1. 現況

**改綁的規則**（network-architecture.zh-Hant.md §4「Gateway」的「身分的重新綁定」）：Hello 從新 endpoint 到達時，token 正確、傳輸序號較新、原 endpoint 的最後驗證時刻已過 ≥ T_int（0.5 s）、session 未到期，才改綁。只有 Hello 帶 token。

**Design Invariants**（同一文件「連線中斷與重新對時」的表）：「身分的重新綁定只用 session 軟狀態」。要問的問題是「改綁的條件是否需要知道 Match 的狀態，或在原 endpoint 沒有靜默時也允許？」。

**Client 的 Hello 間隔**（`ClientConnection.cpp:415-417`）：Welcome 之前、以及中斷期間每 250 ms；其他時候每 1 s。

## 2. K5：換埠之後要等約 2 s 才改綁

追出來的原因：

- **上下行都斷的換埠**（典型的 NAT 映射過期）：舊埠收不到 snapshot，Client 在 T_int 後判中斷，Hello 改成每 250 ms，第一個從新埠到達的 Hello 就能改綁（原 endpoint 早已靜默）。這種情況已經很快，不需要改。
- **不對稱的換埠**（L2 的 C10 就是這種）：上行改從新埠出去，下行的 snapshot 仍然送得到舊埠。
  - 實際的例子：筆電在 Wi-Fi 上插入網路線，系統改由有線網路送出（來源位址變了），但送往 Wi-Fi 位址的封包仍收得到。`lan15-windows` 這類 LAN 場次可能遇到。
  - Gateway 拒收新埠的 Input（未綁定），但照舊把 snapshot 送往舊埠；Client 收得到 snapshot，所以不判中斷，Hello 維持每 1 s。
  - 改綁要等下一個 1 s 的 Hello；若它在原 endpoint 靜默滿 T_int 之前到達會被拒，要再等 1 s。這段期間 Client 的輸入全部被拒，Match 那邊等於沒有輸入。
  - L2（outage-resync，`pvp-v71-resync-l2-20261024`）的 C10 觀察到約 2 s。

## 3. K6：只送 Hello 的期間有改綁空窗

network-architecture §4 寫明的安全取捨：拿到 token、又能讓原 endpoint 靜默 T_int 以上的人可以搶走 session。只送 Hello 的期間（收到 Welcome 之後、第一份 snapshot 之前；或沒有送 Input 的時候），Hello 每 1 s 才刷新一次最後驗證時刻，所以有一半時間它已超過 T_int，搶的人不必先讓原 endpoint 靜默。

- 正常遊玩時 60 Hz 的 Input 一直刷新它，這段空窗只出現在握手後的短時間。
- 真正的修法是防竄改的 session 證明（要改 wire），已列入 v8 候選（D172 的 D-2 I-3）。
- outage-resync 時使用者要求保住 L2 的 Hello／Welcome 計數基線，所以當時只記錄。

## 4. 選項

### K5

| 代號 | 做法 | 優點 | 代價 |
|---|---|---|---|
| r1 | Client：輸入「已送出、但確認沒有前進」滿 T_int 時，Hello 也改成每 250 ms（和中斷時相同） | 只改 Client 的網路 worker，用的是它自己的狀態；Gateway、wire 都不變；正常遊玩時確認每個 Tick 都前進，不會觸發，L2 基線不變。改綁約在恢復後 T_int＋250 ms 內 | 要新增「確認最後一次前進的時刻」；Match 停頓時也會觸發（多送幾個 Hello，只換到 Welcome，無害） |
| r2 | Client：Hello 一律每 250 ms | 最簡單；同時解決 K6 | Hello 與 Welcome 都變 4 倍，L2 的計數基線全部要重取；速率預算 82＋4＋30＝116，離上限 120 只剩 4 |
| r3 | Gateway：記住「靜默不足而被拒的改綁」，靜默滿 T_int 時自動完成 | Client 不變 | Gateway 新增狀態與時間觸發的判斷，屬於 Architecture Delta；要重新檢查 Design Invariants 與安全取捨 |
| r4 | 保留現狀 | 不動 | 不對稱換埠時約 2 s 沒有輸入 |

### K6

| 代號 | 做法 | 優點 | 代價 |
|---|---|---|---|
| k1 | 保留每 1 s，安全取捨照現在的條文 | 不動；真正的修法在 v8 I-3 | 空窗留著（條文已寫明，事後查得到） |
| k2 | Hello 一律每 250 ms（同 r2） | 空窗消失 | 同 r2 |
| k3 | Client：最近 T_int／2（250 ms）內沒有送出 Input 或 Actions 時，Hello 每 250 ms | 只送 Hello 的期間最後驗證時刻一直在 T_int 內，空窗消失；正常遊玩時 60 Hz 的 Input 一直在送，不會觸發，L2 基線幾乎不變（只多握手後的一兩個 Hello） | 新增「最後一次送出 Input／Actions 的時刻」 |

## 5. 建議

**r1＋k3**：兩者都只改 Client 的 Hello 間隔規則，合成一條：

> Welcome 之前、中斷期間、最近 250 ms 沒有送出 Input 或 Actions、或輸入已送出但確認滿 T_int 沒有前進時，Hello 每 250 ms；其他時候每 1 s。

- Gateway、wire、通用層 session、Design Invariants 都不變。改綁的條件不變，只是 Client 更早送出能讓它成立的 Hello。
- 要改的契約條文：network-architecture §4 的 Hello 間隔與安全取捨那一段（空窗消失，取捨只剩「拿到 token 又能讓原 endpoint 靜默」）。這是契約的變更，所以放在這個關卡。
- 速率：最多每秒 4 個 Hello，和 Welcome 之前、中斷期間相同，在已核准的預算（116）內。

## 5a. 決定（D187）

r1＋k3，照第 5 節的規則；契約條文跟著改；實作後做 1 次 xhigh 審查；L2 宣告寫好後再交使用者核准。

## 6. 驗收（建議；L2 的判定要在執行前另行宣告並核准，§14.1）

- **Client 測試**：worker 驗收新增兩個情境：
  - snapshot 照常到、但確認不前進：T_int 之後 Hello 間隔變成約 250 ms，確認恢復前進後回到 1 s；
  - Welcome 之後、第一份 snapshot 之前，Hello 每 250 ms。
- **突變**：拿掉任一觸發條件，都要被上面的情境殺死。
- **L2**：outage-resync 的工具重跑 C10（換埠）3 輪，加 C0（乾淨對照）3 輪看 Hello／Welcome 計數有沒有變。改綁時刻在宣告裡事前定門檻（例如恢復後 ≤ T_int＋250 ms＋餘裕）。
- 四平台 L1。

## 7. 估時

實作與 Client 測試約 2～3 h；L2 宣告、執行與判定約 1～1.5 h。合計 3～4.5 h，在 D181 給的 5～8 h 內。
判定邏輯（觸發條件與時刻）屬於容易出錯的時序，實作後想用 1 次 xhigh 審查（要你同意，主會話以上的檔位）。

## 8. xhigh 審查（2026-10-25，`aed91dd`）與重新決定的關卡

執行緒安全、`NextHelloAt` 同時當送出條件與喚醒期限：沒有缺陷。發現：

1. **條件 (b) 的前提不成立**（中高）。「上行到不了，確認就不會前進」對真實的 Match 不成立：收不到命令時，Match 每個 Tick 用替代命令（Held，之後 Neutral，`PvpMatch.cpp:477-500`）解決下一個序號，snapshot 的確認照樣每個 Tick 前進。所以純不對稱換埠（事前沒有斷線）時，(b) 要等 Starvation 換 epoch（約 0.55 s）之後再過 T_int 才成立，約 t0＋1.1 s；舊行為的平均約 1.0 s、最壞約 1.5 s，D187 只改善到最壞約 1.1 s。L2 的 C10 是「先雙向斷 1 s 再換埠」，斷線期間已經換過 epoch，(b) 剛好成立，所以 worker 測試與計畫中的 L2 都看不出差別。第 2 節把 C10 當成純不對稱換埠，是誤認。
   - 另一個角度：「確認」的權威在 Match（命令是實際到達還是替代），(b) 是用它推論「上行有沒有到」，屬於 AGENTS.md §3「狀態的權威」要提出的判斷。
2. 契約條文的巢狀被改錯（已修正，`037e27b`）。
3. 測試缺口：(b) 的門檻值沒有釘住；(a) 只測了握手後；沒有測「確認恢復前進後回到 1 s」；每秒 4 個的上限沒有檢查；3 條 reset／更新沒有情境；`acknowledgementMoved` 的 epoch 條件是死碼（等價突變）。
4. Hello 計數改變的範圍比第 3、5 節寫的大：只有上行中斷（C4、C5）時每輪約多 7～8 個（現行 L2 判定不讀這個計數）。
5. `Movement.hpp:40-43` 與 `worker_main.cpp:556` 的預算註解過時（數字 116 仍成立）。

選項（等使用者決定）：

| 代號 | 做法 | 純不對稱換埠的改綁 | 代價 |
|---|---|---|---|
| P1 | (b) 改用只在命令實際到達時才變化的訊號（候選：snapshot 的 `contiguous_pending_commands` 長時間為 0，要先追約 30 分鐘確認語意） | 約 t0＋0.6～0.8 s | 仍是用 Match 發布的狀態推論上行，§3 的判斷點不變；測試要重寫 |
| P2 | 保留 (a)＋(b)，只修正條文與預期（C10 型很快，純不對稱最壞約 1.1 s），補測試缺口 | 最壞約 1.1 s | §3 的判斷點不變；改善有限 |
| P3 | 拿掉 (a)、(b)，Hello 一律每 250 ms（第 4 節的 r2） | 約 t0＋0.5～0.75 s（只看原 endpoint 的靜默） | Hello 與 Welcome 數量變 4 倍；速率預算 116 已包含每秒 4 個；邏輯與狀態都比現在少，沒有 §3 的判斷點；K6 的空窗一樣消失 |

建議 P3：它不推論任何別人的狀態，兩個問題都解決，實作比 D187 更小。代價是計數基線；審查確認現行 L2 判定不讀 Hello／Welcome 的計數，舊證據的比較要註明換了規則。

**D188**：選 P3。取代 D187 的規則（r1＋k3）。契約條文與測試跟著改；舊證據的 Hello／Welcome 計數比較，註明 D188 換了規則。

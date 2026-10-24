# rebind-on-new-port：換埠後的改綁時機與 Hello 間隔（2026-10-25 使用者決定，D187）

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

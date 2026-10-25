# quick-close：關閉 Client 時卡住約 3 秒（關卡，2026-10-25，等使用者決定）

發行線的第二批（INVENTORY `F1`）。唯讀調查，還沒有改任何程式。

## 成因

- 關閉 Client 時，網路 worker 在結束前呼叫 `DisconnectRemote`（`ClientConnection.cpp:895`），同步送 HTTP `POST /rooms/<room>/leave`（`:303`）。
- HTTP 客戶端的連線、讀取、寫入逾時都是 2 秒（`:80`）。Gateway 連不到或回得慢時，關閉就卡到逾時為止；INVENTORY 記的是「最多約 3 秒」。
- `/leave` 的作用是立刻釋放房間的位子；不送的話，要等 Gateway 的 session 到期（T_max＝5 s）。

## 選項

| 代號 | 做法 | 優點 | 代價 |
|---|---|---|---|
| q1 | 只有關閉時的 `/leave` 改用短逾時（例如連線與讀取各 300 ms） | 改動小；Gateway 正常時位子照樣立刻釋放 | Gateway 很慢時這次 `/leave` 放棄，位子要等 5 s 到期 |
| q2 | 關閉時不送 `/leave`，交給 session 到期 | 關閉最快 | 位子一律晚 5 s 才釋放（重新加入時可能遇到 `room_full`） |
| q3 | 視窗一關就先送 `/leave`、和畫面拆除並行，最多等一個上限 | 使用者感覺不到等待 | 結構改動較大（要動 App 的關閉順序） |

## 建議與驗收（門檻要事前宣告並核准，AGENTS §14.1）

- **建議 q1**：影響最小，正常情況的行為不變。
- **要你核准的判定草案**：從關閉要求到程序結束的時間，Gateway 可連時 ≤500 ms、Gateway 不可連（關掉 Gateway）時 ≤1 s；macOS 與 Windows 各 10 次，全部符合才算通過，不重試。數字在跑之前核准，跑完不放寬。
- **估時**：2～3 h。

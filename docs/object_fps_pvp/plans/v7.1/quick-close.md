# quick-close：關閉 Client 時卡住約 3 秒（2026-10-25：D202 選 q1、門檻核准；macOS 驗收通過，Windows 待跑）

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

## 決定與實作（D202）

使用者：「q1，門檻核准」。

- **實作**：`ClientConnection.cpp` 的網路 worker 結束時（Client 關閉）送的 `/leave` 改用短逾時：連線、讀取、寫入各 300 ms，總上限 600 ms（`ClosingLeaveTimeout`）。其他 HTTP 請求（加入、房間列表、離開房間回大廳）維持 2 秒。
- **量測**：worker probe 新增 `--close responding|down|stalled`：加入 MockGateway 進入 Playing 後印出 `close requested`，再解構產品的 `ClientConnection`（關閉時的 `/leave` 就在這裡送）。`build/acceptance/object_fps_pvp/close_timing.py` 量從那一行到程序結束的時間，每種情境 10 次，判定照核准的門檻：responding ≤500 ms、down ≤1000 ms，全部符合、不重試；stalled（Gateway 收下 `/leave` 但不回）只記錄。量的是產品的網路層，不是整個 Client 程序：延遲只來自這裡，視窗與 GPU 的拆除沒有改。
- **Red（探索，修正前，各 3 次）**：responding 2.7～2.9 ms、down 2.2～2.6 ms、stalled 2003～2004 ms。**核准的兩個判定在修正前也通過**：本機關掉 Gateway 時連線立刻被拒，不會等；會卡住的是 Gateway 收下請求卻不回（或主機不在、連線逾時）的情況，對應 stalled。
- **macOS 驗收（修正後，各 10 次，Intel Mac，背景同時在跑全量突變）**：通過。responding 最大 2.7 ms、down 最大 2.3 ms；stalled（記錄）303.5～304.1 ms。
- **Windows**：使用者在 D201 的 Windows 機器上跑同一個腳本。

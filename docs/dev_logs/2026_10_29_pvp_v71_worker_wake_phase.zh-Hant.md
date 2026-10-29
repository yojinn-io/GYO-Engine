# PvP v7.1 之後：worker 探針的 ACK 相位與 D226 的計時器喚醒（worker-wake-phase）

日期：2026-10-29。分支 `claude/pvp-worker-ack-phase-91261b`（從 master `62fd7ebf`）。Owner：`object_fps_pvp`。

**狀態：確認通過（D245「修正有效」），PR 待開。產品不變，只改驗收探針 `build/acceptance/object_fps_pvp/worker_main.cpp`。**

計畫、事前宣告與每次的結果在 [worker-wake-phase.md](../object_fps_pvp/plans/v7.1/worker-wake-phase.md)。

## 為什麼

v7.1 留下 worker 探針（CTest `object_fps_pvp.worker`）的兩個問題，記在 HANDOFF 的「範圍的限界」：

1. **ACK 檢查的相位沒有量**。「an advanced ACK waited for an unrelated wake」要確認 Drain 推進 ACK 時會喚醒網路執行緒。但 Client 每 250 ms 送一次 Hello，送 Hello 的那一輪也會順便送出 ACK。如果 Drain 剛好在 Hello 附近，突變 `v7-08-drain-no-wake` 可能會存活。另外，註解寫的「next Hello, up to a second away」是錯的。
2. **D226 的 2 次計時器喚醒來源不明**。在 D226 中，action 送出之前有 2 次 mock 看不到的計時器喚醒。

## 做了什麼

### 量測（D243）

量測前先宣告，只加觀測：`WakeTimeline` 會印出 Drain 或 SubmitShot 之前最後一次 Hello 的時間，也就是 `prior_hello_ms`。

- **量測 1**（O、M 各 20 次）：`prior_hello_ms` 全部在 131～136 ms，屬於判定 3「目前的流程避開了 Hello」。
  - 也就是說，相位只是靠前面步驟的耗時湊出來的：前面的步驟一改，突變就可能碰巧存活。
  - M 的 20 次全部 killed。其中 18 次在 ACK 之後 1.7～2.0 ms 就到了一個 Hello。
- **量測 2**：屬於判定 2，D226 的來源是 Hello 期限前 2 ms 的那一輪。
  - 機制：遠的期限，計時器會提前 2 ms 醒來先跑一輪（`CoarseWakeLead`，v7 第 08 批 `730984ec`），再等到期限跑第二輪。第一輪會送出已經可以送的東西，Hello 則在約 2 ms 後送出。
  - E-M 20／20 都是這個型態：action 在 7.86～8.33 ms 到達，Hello 在它之後 1.61～1.97 ms。
  - 這是產品的設計，不是缺陷。D227 的 100 ms 餘裕已經避開了它。
  - 判讀過程中修正了 analyzer 的一個錯誤：把 action 自己的封包算成「之間」的封包，記錄在計畫文件中。

### 修正（D244 的決定）

- 新增 `AwayFromHello`：等 mock 收到下一個 Hello，從它到達的時刻起再等 100 ms。內容就是 D227 原本寫在流程裡的那一段，搬成函式；逾時訊息仍會指出呼叫的那一行。
- `CheckActions` 的 ACK 檢查：閒置 50 ms 之後先呼叫 `AwayFromHello`，再 Drain。
- D227 的那一段：改成呼叫 `AwayFromHello`，行為不變。
- 註解：寫明 Hello 前 2 ms 的那一輪也會送出已經可以送的東西，拿掉「up to a second away」。
- 40 ms 的判定與兩個失敗訊息都沒有改。

### 判定的變更（AGENTS.md §14.4）

- 守的性質：Drain 推進 ACK 時會喚醒網路執行緒，立刻送出 ACK，不等無關的喚醒。
- 判定的式子不變，只改 Drain 的時刻，所以沒有放寬的面。
- 收緊的面：相位改由 ACK 檢查自己決定，不再依賴前面步驟的耗時。

## 驗證與經過

- **D244 的確認：不通過**。
  - FM（修正版加上 `v7-08-drain-no-wake`）只有 17／20 killed。另外 3 次在 ACK 檢查之前就失敗了，失敗的是別的檢查：1 次 `SendInput` 的 assert abort、2 次中斷行的 `gap_ms` 523／563。
  - FO 10／10 通過；FS 10／10 killed。
  - 第 4 項（47 個突變）沒有跑：我的命令把 id 清單放在一個 zsh 變數裡，zsh 不做字詞分割，runner 只收到一個參數。
  - 依 §14 停下。這個記錄保留，不改寫。
- **D245 的確認：通過**。使用者選擇把「在 ACK 檢查之前就失敗」另列一類，上限 2 次。D245 是另一次試行，判定是在看過 D244 的結果之後才訂的，計畫文件中寫有放寬的面。
  - FM：跑 20 次，20 次都到達 ACK 檢查，20 次都 killed；ACK 檢查之前的失敗 0 次。
    - `prior_hello_ms` 在 100.00～100.14。
    - ACK 在 Drain 之後 147.9～148.9 ms 才送出，也就是下一次 Hello 前 2 ms 的那一輪。
  - worker 的 47 個突變：baseline 通過，47／47 killed，沒有判錯失敗的，約 32 分鐘。這次 id 清單改由 Python 以參數清單傳給 runner，並先檢查個數是 47。
- **load average**：1.14～3.82。同一台 Mac 上其他會話的活動控制不了。
- **證據**：每次的完整輸出在本機 scratchpad，不進 git；摘要與數值在計畫文件。crash report 在 `~/Library/Logs/DiagnosticReports/worker_FM-2026-10-29-202503.ips`。

## 留下的事

- worker 探針在 `CheckActions` 之前的 3 種偶發失敗：D243、D244 的 130 次中出現 4 次。已記為 HANDOFF 的未結事項，不修也不追查。之後要改 `worker_main.cpp` 的 K10、WSAECONNRESET、token 等待的會話需要知道這件事。
- 範圍的限界：只在 macOS（Intel）確認；Windows 的 worker 驗收照 D231，不算發行條件。HANDOFF「範圍的限界」中的兩項（D226 的來源、ACK 的相位）由這次解消。

## 架構

沒有架構上的影響：產品、Contract、依賴關係都不變，只改驗收探針自身的時機與觀測。

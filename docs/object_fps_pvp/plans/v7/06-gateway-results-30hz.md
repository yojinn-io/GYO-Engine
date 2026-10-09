# 第 06 批：Gateway→Client 結果通道改為 30 Hz

狀態：**停在停止條件，等使用者決定**（2026-10-09）。實作、L1、開發跑次已完成；程式在本機的 WIP commit，未推送。
PR 線 P2。依賴 P2-log（`983e091`，本批的 before）。建議檔位 medium（資格容許局部可用 xhigh；本批沒有做 xhigh 審查）。

## 起因

- 結果通道只有約 15 Hz（P2-log 的開發跑次：間隔 P50 66.6 ms）。`ActionSendRate` 是 30 Hz。
- 原因在 `action_delivery.go:205` 的資格判定 `now.Before(w.nextSend)`：
  - 每次寫出完成後，期限重新錨定為「寫出完成＋I」（I＝1／30 秒）。
  - 寫出完成總是比選出它的 ticker 晚一點點，所以下一次 ticker 永遠「早一點點」，被跳過。
  - 結果是每隔一次 ticker 才送一次。
- 盤點在 relay 後量到的「約 18 Hz」，原因相同。

## 範圍

- 資格判定改為只在 `now + I/2 < nextSend` 時才跳過（`resultSendTolerance = actionSendInterval / 2`）。
- 保留寫出後的重新錨定。所以被擋住的寫出放行之後，不會補送，也不會爆量。
- runtime link 的 action batch 不改，只量測（P2-log 的 `link_actions`）。
- 既有測試 `TestCompletedBlockedWriteReanchorsActionDeadline` 的 UDP 部分，釘住的正是舊的資格（期限前 1 ns 不送）。這是本批要改的語意本身，所以改成：
  - 期限前 I/2 再早 1 ns：不送。
  - 期限前 I/2：送。
  - TCP（runtime link）部分不變。

## L1（完成）

- 新的 Go 測試 2 件：
  - 寫出完成落在 ticker 後 200 µs 時，30 次 ticker 送 30 次。舊的判定只送 15 次。
  - 測試條件：ticker 有 0～30 ms 的延遲，另有一次 250 ms 的寫出阻塞。
    - 兩次送出至少相隔 I/2。
    - 阻塞放行後不補送。
    - 任一秒不超過 31 次。
    - 至少每隔一次 ticker 送一次。
- 突變 2／2 killed：
  - `v7-06-results-exact-deadline`：退回舊的判定。
  - `v7-06-results-full-tolerance`：容許改成一整個 I。
- `go vet`、`go test`、`go test -race` 通過。

## 開發跑次（只記錄）

- before 是 P2-log 的 Gateway（`983e091`），after 是本批的 Gateway。Match 與 probe 相同，雜湊見 `artifacts.sha256`。
- `run_gameplay.py` 7 案，before／after 交錯，共 14 次，全部通過：
  - 案例：clean-60、clean-30、loss-result、loss-ack、burst2、gateway-250ms、downstream-250ms。
  - Gateway 的結果間隔 P50：before 66.5～66.7 ms，after 全部 33.3 ms。
  - relay 量到的「任一秒內的結果封包數」：before 17～21，after 全部 31。
- `action_probe.py` 自己的主模式共 19 次（after 16 次、before 3 次）：
  - before 與 after 都判定失敗，錯誤相同：「HP differs from sum of unique decision effects」2 筆、「Snapshot combat effect differs from unique original decisions」292～296 筆。
  - 這是既有的失敗：v6 交接記載這個主模式仍是 v4 的動作矩陣語意，v5 起的驗收不用它。它也在 v6 第 14b 批的凍結清單裡。

## 停止條件（2026-10-09 碰到）

- 交接寫的停止條件：「修正後若碰到分析器『每秒 ≤31』的窗口規則，停下由使用者決定」。
- 碰到的地方：`action_probe.py:412` 的「Gateway result schedule exceeds30Hz (one boundary packet allowed)」。
  - after 在 `upstream-250`、`host-ipc-1000` 兩案出現，兩位玩家都有。
  - relay 量到任一秒 32 個，最小間隔 2.4 ms（`host-ipc-1000`）與 9.1 ms（`upstream-250`）。
- 判斷材料：
  - **Gateway 端的保證**：新的判定要求下一次選出時刻不早於上一次寫出完成加 I/2，所以兩次寫出至少相隔約 16.7 ms。relay 量到的 2.4／9.1 ms 比這小，應該是 relay 收包時的排隊壓縮。但 P2-log 的統計只記 P50／P90／P99／最大，沒有最小間隔，所以 Gateway 端還不能直接證明。
  - **31 是結構上的上限**：30 Hz 的格點上，1 秒視窗最多 31 個，規則也註明「允許 1 個邊界封包」。before 只有 15 Hz，離上限很遠；after 每次都在 31。只要 relay 端有一次約 33 ms 的壓縮，就會量到 32。
  - **現行驗收不受影響**：這條規則所在的主模式不在現行驗收裡。現行矩陣用的 `gameplay_evidence.py` 只檢查上行（動作／ACK ≤31、認證封包 ≤120），不檢查結果的下行。

## 選項（等使用者決定）

- **(a) 建議**：接受本批的行為，不改凍結的 `action_probe.py`。
  - 「不爆量」改在 Gateway 端判定：結果間隔的統計加上最小值。
  - L2 的宣告以它判定：最小間隔 ≥ I/2，再扣掉寫出時間的容許。
- **(b)** 讓 Gateway 送得更保守，例如容許改成 I/4，或改以 ticker 時刻重新錨定。
  - 可以降低 relay 量到 32 的機率，但 relay 端的壓縮仍可能讓它出現，不能保證。
- **(c)** 做分析器 v7 的結果速率規則，保留 relay 端的檢查。
  - 例如改以 Gateway 的時刻判定，或明確容許 relay 的抖動。

## 之後

- 依決定修改。
- 寫 L2 的事前宣告，送使用者核准：before＝`983e091`，after＝本批的頭，以 P2-log 的統計比較結果通道。
- 推送，併進 PR #73。

## 證據

`build/target/_build/test/logs/pvp-v7-batch06-20261009/`（git 忽略）：
- `artifacts.sha256`、`gateway-before`、`gateway-after`。
- `mutations.json`。
- `dev-runs.py`、`dev/`、`dev-summary.json`、`dev-result-rates.json`。
- `action-runs.py`、`action/`、`action-summary.json`。

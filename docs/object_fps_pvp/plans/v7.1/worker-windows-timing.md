# worker-windows-timing：worker 驗收在 Windows MSVC 上的時間判定失敗（D216；第 3 步的事前宣告 2026-10-28 核准，D217）

## 現象

- OS 子系統的 os-windows-run 第 1 次（分支 `claude/os-windows-run`，`1b97f5d6`，MSVC，完整 `ctest`，沒有重跑）：`object_fps_pvp.worker` 失敗，訊息「worker acceptance: new window waited behind a fully acknowledged window's deadline」。
- 斷言：`build/acceptance/object_fps_pvp/worker_main.cpp` 要求 `restartMs<10 && completedWindowGapMs<14`。
  - `restartMs`：完整確認之後送新窗口，到 mock 看到它的時間。
  - `completedWindowGapMs`：舊窗口最後一次送出，到新窗口第一次送出的間隔。
- 這一項標 `network;pvp`，CI 的 windows-x64 只跑 `cpu|shader`（D204），所以這可能是它第一次在 Windows 上跑（未確認）。
- macOS（Intel）本機的值：`restart_ms` 0.15～0.20、`completed_window_gap_ms` 0.34～0.41（3 次）。

## 候選（都還沒排除）

1. **測試**：mock 在 `Pump` 時才蓋時間戳，`Pump` 之間是 `std::this_thread::sleep_for(100us)`。MSVC 的 `sleep_for` 走 `Sleep`；探針沒有初始化 SDL，也沒有調高計時器解析度。Windows 10 2004 起解析度是每個程序各自的，可能被捨入到約 15.6 ms 的 tick。
2. **環境**：同一件事從環境的角度看（機器的計時器設定）。
3. **實作**：Client 的網路執行緒對遠的期限用 asio 計時器，最後 2 ms 才交給 Waiter；在沒有 SDL 的 Windows 程序裡，asio 計時器也可能受 tick 影響。

## 切開的步驟

1. （OS 子系統的 Windows 續跑，唯讀）從 W3 的證據讀沒有 SDL（cli）與有 SDL（window）時，`sleep_until` 與 `condition_variable` 的 P50／P99。
2. （OS 子系統的 Windows 續跑）W5 在 `5733d361`（搬執行緒之前）與 B 上各跑 worker 2 次，逐次記錄通過與否。
3. （本會話）觀測已加：失敗訊息印出 `restart_ms` 與 `completed_window_gap_ms`（不改門檻，AGENTS.md §14.3）。下面的量測宣告要使用者核准後才跑。
4. 修測試（門檻或 mock 的輪詢方式）還是修產品（Client 的計時器），由使用者決定。

## 第 1、2 步的結果（2026-10-28，OS 子系統的 Windows 續跑，經「對接 OS 子系統的 Windows 實機結果」會話轉達）

來源：分支 `claude/os-windows-run` 的 `7306a142`，`docs/dev_logs/2026_10_28_engine_os_windows_run.zh-Hant.md` 的 W4、W5。

- **第 1 步（W4，A_wait 的建置，60 Hz 絕對期限，各 30 秒，晚醒 ms）**：
  - 沒有 SDL（cli）：`sleep_until` P50 15.630、P99 16.655、最大 16.893（1782 個樣本中 1504 個超過 4 ms）；`condition_variable` P50 5.280、P99 15.939；Waiter P50 0.617、P99 1.000。
  - 有 SDL（window）：`sleep_until` P50 1.077、P99 2.074；`condition_variable` P50 0.945、P99 1.949。
  - 沒有 SDL 時，Sleep 類的等待以約 15.6 ms 為主；有 SDL 時降到 1～2 ms（推測是 SDL 調高了解析度，未確認）。
- **第 2 步（W5）**：A_threads（`5733d361`，兩批搬移之前）與 B 各跑一次，worker 都以同一句失敗；加上 W1，這台 Windows 上 3 次全失敗。所以和 os-threads-backend、os-wait-backend 無關。
- **讀法（本會話，未驗證）**：worker 探針沒有 SDL，mock 的 `Pump` 之間是 `sleep_for(100us)`，照第 1 步的數據在這台 Windows 上約 15.6 ms，所以 mock 蓋的時間戳本身就有約 15.6 ms 的粒度，量不到 10 ms 以下。這指向候選 1（測試），但候選 3（Client 的 asio 計時器在沒有 SDL 的程序裡）還沒排除。
- 附帶（給 D179 的 `TransientSocketError`）：W6 在 Windows loopback 量到，未連線的 UDP socket 送到關閉的埠後，每個被拒的 datagram 各報一次 WSAECONNRESET（10054），之後 socket 仍可收送。用的是 Python 的 socket，asio 對應到哪個錯誤沒有量。

## 步驟 3 的事前宣告（2026-10-28 核准，D217）

使用者 2026-10-28：「核准 worker-windows-timing.md 的事前宣告，寫進文件，交給 Windows 續跑的 worker-10x。」以下內容在核准後不改；跑完之後不放寬（AGENTS.md §14.1）。

- **建置**：Windows 機器、MSVC（`cmake --preset ci-windows`），發行線 `claude/pvp-v71-release` 含觀測的 commit。
- **跑法**：`ctest --test-dir build/target/_build/ci-windows -C Release -R "^object_fps_pvp\.worker$" --output-on-failure`，串行跑 10 次，不重跑、不中途改。每次記錄：通過或失敗、`restart_ms`、`completed_window_gap_ms`（通過時取 stdout 的 JSON，失敗時取訊息），以及失敗時的其他訊息。
- **收支**：10 次中每一次都要有兩個值。沒有值的（其他斷言先失敗、逾時）照實記錄，計入「確認不了」。
- **分類（只記錄，不是驗收）**：
  - 「tick 捨入」：失敗的每一次，至少一個值超過門檻，而且落在 15.625 ms 的整數倍 ±2 ms 內。
  - 「不是 tick」：有失敗的值超過門檻，但不在任何整數倍 ±2 ms 內。
  - 「判定不了」：10 次全部通過，或者失敗的次數不足 2 次。
- **範圍的限界**：只有一台 Windows 機器；只看 worker 這一項，不代表其他網路測試；沒有 SDL 的程序才會受影響（Client 本體有 SDL）。

## 第 3 步的結果（2026-10-28，Windows 機器，D217 的宣告、D220）

收支：一致（10 次都跑完，每一次都有 `restart_ms` 與 `completed_window_gap_ms` 兩個值；沒有逾時，也沒有其他斷言先失敗）。

- **建置**：發行線 `ac922e498a2b4f5bfe8439293b771b292a06aa1d`（含 `d7e0af31`），另開的 worktree，`git ls-files --eol` 沒有 `w/crlf`。在 VS Community 2026 的 vcvars64 環境裡執行 `cmake --preset ci-windows`（exit 0）與 `cmake --build build/target/_build/ci-windows`（exit 0，1353 步，15:42～16:00）。MSVC 19.51.36260。
- **機器**：Windows 11 Pro 10.0.26300，i7-8750H（6 核 12 執行緒，筆電），16 GB，接著 AC（`PowerOnline=True`），電源計畫「平衡」。和 OS 子系統 os-windows-run 第 1 次是同一台。
- **跑法**：照宣告的指令 `ctest --test-dir build/target/_build/ci-windows -C Release -R "^object_fps_pvp\.worker$" --output-on-failure`，串行 10 次（16:00:24～16:03:33），沒有重跑，中途沒有改任何東西，期間沒有其他吃重的工作。
- **結果：10 次全部失敗**，都是同一句：`worker acceptance: new window waited behind a fully acknowledged window's deadline`。除了這一句與 ctest 的摘要，沒有其他訊息。

| 次 | 結果 | 耗時 s | `restart_ms` | 最近的 15.625 倍數（差） | `completed_window_gap_ms` | 最近的 15.625 倍數（差） |
|---|---|---|---|---|---|---|
| 1 | 失敗 | 18.32 | 16.9408 | ×1（+1.316） | 33.9042 | ×2（+2.654） |
| 2 | 失敗 | 18.26 | 16.8145 | ×1（+1.189） | 33.6171 | ×2（+2.367） |
| 3 | 失敗 | 18.25 | 16.2274 | ×1（+0.602） | 32.0213 | ×2（+0.771） |
| 4 | 失敗 | 18.20 | 17.3156 | ×1（+1.691） | 33.7354 | ×2（+2.485） |
| 5 | 失敗 | 18.29 | 16.6195 | ×1（+0.994） | 32.8000 | ×2（+1.550） |
| 6 | 失敗 | 18.22 | **1.7309** | —（未超過門檻 10） | **18.2151** | ×1（+2.590） |
| 7 | 失敗 | 18.27 | 16.1032 | ×1（+0.478） | 30.6155 | ×2（−0.634） |
| 8 | 失敗 | 18.27 | 16.3113 | ×1（+0.686） | 32.7947 | ×2（+1.545） |
| 9 | 失敗 | 18.26 | 16.3318 | ×1（+0.707） | 32.5778 | ×2（+1.328） |
| 10 | 失敗 | 18.19 | 17.1525 | ×1（+1.527） | 33.2471 | ×2（+1.997） |

門檻：`restart_ms` <10、`completed_window_gap_ms` <14。兩個值都是 ms。

**分類（照宣告的文字逐條套用，只記錄）**：

- 「tick 捨入」（失敗的每一次，至少一個值超過門檻而且落在整數倍 ±2 ms 內）：**不成立**。第 1～5、7～10 次的 `restart_ms` 都超過門檻，而且在 15.625 的 ±2 ms 內；但**第 6 次**唯一超過門檻的值是 `completed_window_gap_ms` 18.2151，離 15.625 差 +2.590，不在 ±2 ms 內。
- 「不是 tick」（有失敗的值超過門檻，但不在任何整數倍 ±2 ms 內）：**成立**。這樣的值有 4 個：第 6 次的 `completed_window_gap_ms` 18.2151（+2.590），以及第 1、2、4 次的 `completed_window_gap_ms` 33.9042、33.6171、33.7354（離 31.25 差 +2.654、+2.367、+2.485）。
- 「判定不了」（全部通過，或失敗不足 2 次）：不成立（失敗 10 次）。
- 套用時看到的事（不是判定，判讀交給 v7.1 會話）：
  - 這三類的文字沒有互斥。第 1、2、4 次各有一個值落在 ±2 ms 內（`restart_ms`），另一個不在（`completed_window_gap_ms`）。上面是照字面套用的結果。
  - 不在 ±2 ms 內的 4 個值，離最近倍數的差都是正的（+2.367～+2.654 ms）。
  - 第 6 次的 `restart_ms` 是 1.7309，是 10 次中唯一低於門檻的值。

**證據**（留在 Windows 機器上，不在 repo）：主 checkout 的 `build/target/_build/test/logs/worker-10x-20261028/`，內含 `build.log`、`build-exit.txt`、`msvc-version.txt`、`run1.log`～`run10.log`、`runs.txt`。`runs.txt` 裡的 `exit=` 欄是 Windows 端記錄指令的錯誤（取到的是 `date` 的結束碼，不是 ctest 的），不要用；每次的通過與否以 `run<n>.log` 的 ctest 摘要為準（10 份都是 `0% tests passed, 1 tests failed out of 1`）。

## 第 3 步的判讀與停止（2026-10-28，v7.1 會話）

收支：一致。照字面套用，分類是「不是 tick」。但宣告的三個分類不互斥（第 1、2、4 次各有一個值在 ±2 ms 內、一個不在），這是寫宣告時的疏失；結果照字面記錄，不改寫。值的形狀（restart 約 1 個 tick、間隔約 2 個 tick、一律偏多 0.5～2.6 ms）指向 mock 的 `sleep_for(100us)` 在沒有 SDL 的 Windows 程序裡睡滿約 15.6 ms，但這個讀法會改變判定基準，所以依 AGENTS.md §14 開頭停下。使用者選「重新檢視前提」，看過取捨後決定：「先做B，還有問題做C」（D221）。

## B：mock 改用 Engine 的 Waiter，再在 Windows 跑 10 次（D221）

- **改法**：worker 探針的 mock 在兩處輪詢之間的 `std::this_thread::sleep_for(100us)` 改成 Engine 的 `Time::Waiter::WaitUntil(Now()+100us)`（`MockGateway::Pause`）。Waiter 在 Windows 用高解析度的 waitable timer，不受程序的計時器解析度影響（W4：沒有 SDL 時 P50 0.617 ms）。門檻、斷言、Client 的程式都不改。
- **macOS（Intel）本機**：改後連跑 5 次通過，`restart_ms` 0.12～0.13、`completed_window_gap_ms` 0.25～0.30（改前 0.15～0.20、0.34～0.41）。
- **突變（D184：殺它的測試檔被改動，worker 為檢查的 47 個）**：46／47 killed；`sb-c3-welcome-unparsed` 第一次是 wrong_failure（worker 失敗在「unacknowledged window was not resent at approximately 60 Hz」），單獨重跑 2 次都 killed，歸為環境（同一台 Mac 上另一個會話同時在工作）。
- **Windows 的事前宣告（2026-10-28 核准，D222）**：使用者：「核准 worker-windows-timing.md 中 B 的 Windows 事前宣告，突變跑完後推送，我在 Windows 上跑。」以下內容核准後不改，跑完不放寬（AGENTS.md §14.1）。
  - 建置：Windows 機器、MSVC（vcvars64 環境，`cmake --preset ci-windows`、`cmake --build build/target/_build/ci-windows`），發行線含 B 的 commit。
  - 跑法：`ctest --test-dir build/target/_build/ci-windows -C Release -R "^object_fps_pvp\.worker$" --output-on-failure`，串行 10 次，不重跑、不中途改，期間不做其他吃重的工作。每次記錄通過或失敗、`restart_ms`、`completed_window_gap_ms`、失敗訊息。
  - 收支：每一次都要有通過或失敗的結果與兩個值；沒有值的計入「確認不了」。
  - 判定（互斥，依序取第一個成立的）：
    1. 「確認不了」：有任何一次沒有兩個值，或建置失敗。
    2. 「測試的原因」：10 次全部通過。結論：先前的失敗來自 mock 的時間戳粒度；產品在這條路徑上沒有觀察到問題。
    3. 「還有問題，做 C」：其他情況（至少一次失敗）。結論：原因不只 mock，進入 C（在 Client 端記錄實際送出的時刻）。
  - 範圍的限界：只有一台 Windows 機器；worker 探針沒有 SDL（和出貨的 Client 不同）；只看 worker 這一項。

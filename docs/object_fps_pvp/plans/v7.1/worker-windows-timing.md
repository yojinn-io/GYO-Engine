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

## B 的 Windows 結果（2026-10-28，Windows 機器，D222 的宣告）

收支：不一致（10 次都有結果，都是失敗；但 10 次都**沒有** `restart_ms` 與 `completed_window_gap_ms` 兩個值，依宣告計入「確認不了」）。

- **判定（照宣告的互斥順序，取第一個成立的）：1.「確認不了」**。理由：10 次都沒有兩個值（宣告：「有任何一次沒有兩個值」）。2、3 沒有往下套用。
- **建置**：發行線 `8966293ca0408e7804b19d4df3135df221fe2d21`（含 `f29001e0`），主 checkout，`git ls-files --eol` 沒有 `w/crlf`、工作樹乾淨。在 VS Community 2026 的 vcvars64 環境裡執行 `cmake --preset ci-windows`（exit 0）與 `cmake --build build/target/_build/ci-windows`（exit 0，增量 93 步，含 `worker_main.cpp`；17:04～17:09）。MSVC 19.51.36260。
  - 先前有一次建置跑錯目錄（Windows 端的腳本錯誤，在 `..\gyo-v71` 的 `e400f89`、不含 B）。configure 中途停掉，沒有拿來跑測試；log 保留在證據目錄的 `attempt1-wrong-dir/`。
- **機器**：和第 3 步同一台（Windows 11 Pro 10.0.26300，i7-8750H，16 GB），接著 AC（`PowerOnline=True`），電源計畫「平衡」。
- **跑法**：照宣告的指令 `ctest --test-dir build/target/_build/ci-windows -C Release -R "^object_fps_pvp\.worker$" --output-on-failure`，串行 10 次（17:09:33～17:12:46），沒有重跑，中途沒有改任何東西，期間沒有其他吃重的工作。

| 次 | 結果（ctest 摘要） | ctest exit | 耗時 s | `restart_ms` | `completed_window_gap_ms` | 失敗訊息 |
|---|---|---|---|---|---|---|
| 1 | 失敗 | 8 | 19.25 | 沒有 | 沒有 | `unacknowledged window was not resent at approximately 60 Hz` |
| 2 | 失敗 | 8 | 18.98 | 沒有 | 沒有 | 同上 |
| 3 | 失敗 | 8 | 18.96 | 沒有 | 沒有 | 同上 |
| 4 | 失敗 | 8 | 19.00 | 沒有 | 沒有 | 同上 |
| 5 | 失敗 | 8 | 19.01 | 沒有 | 沒有 | 同上 |
| 6 | 失敗 | 8 | 19.00 | 沒有 | 沒有 | 同上 |
| 7 | 失敗 | 8 | 19.02 | 沒有 | 沒有 | 同上 |
| 8 | 失敗 | 8 | 19.01 | 沒有 | 沒有 | 同上 |
| 9 | 失敗 | 8 | 18.97 | 沒有 | 沒有 | 同上 |
| 10 | 失敗 | 8 | 18.97 | 沒有 | 沒有 | 同上 |

除了這一句與 ctest 的摘要，`[ObjectFPS/PvP]` 以外沒有其他訊息。

**為什麼沒有值（讀程式碼，未另外驗證）**：

- 這次失敗的斷言是 `worker_main.cpp:847`（`resends>=55 && resends<=65`，1 秒內未確認窗口的重送次數），不是第 3 步失敗的 `:817`。
- 在同一個流程裡，`:817`（`restartMs<10 && completedWindowGapMs<14`）、`:835`、`:840`、`:842` 都在 `:847` 之前，而且 `Require` 失敗就會結束。所以 10 次都已經通過 `:817`，只是兩個值只在 `:817` 失敗的訊息與最後的 JSON（`:1140`）裡印出，這次沒有輸出的地方。
- `:847` 的重送次數本身也沒有印出。

**判讀交給 v7.1 會話**（Windows 端不做）。宣告、門檻、測試、產品程式都沒有改。

**證據**（留在 Windows 機器上，不在 repo）：主 checkout 的 `build/target/_build/test/logs/worker-B-20261028/`，內含 `build.log`、`build-exit.txt`、`build-commit.txt`、`msvc-version.txt`、`run1.log`～`run10.log`、`runs.txt`（這次的 `ctest_exit=` 是 ctest 的結束碼）、`attempt1-wrong-dir/`。

## B 的判讀與停止（2026-10-28，v7.1 會話）

收支：不一致（依宣告，判定是「確認不了」）。照 AGENTS.md §14 開頭，不定結論，等使用者決定。

- 宣告的判定照字面是「確認不了」：10 次都沒有兩個值。原因是值只在 `:817` 失敗的訊息與最後的 JSON 裡印出，而 10 次都在更後面的 `:847` 失敗。宣告沒有預想到「通過 `:817`、失敗在後面的斷言」這種情況，這是寫宣告時的缺漏。
- 照程式的順序（`Require` 失敗就結束），10 次都已通過 `:817`（`restartMs<10 && completedWindowGapMs<14`）。這表示第 3 步的失敗在 mock 改用 Waiter 後不再出現，和「mock 的時間戳粒度」的讀法一致（讀程式碼，未另外驗證）。
- 新的失敗是 `:847`：Client 在 1 秒內重送未確認窗口的次數不在 55～65（60 Hz）。這一項在 Windows 上之前從來沒有跑到過。重送的節奏由 Client 的網路執行緒決定：遠的期限用 asio 計時器，最後 2 ms 才交給 Waiter。在沒有 SDL 的 Windows 程序裡，asio 計時器可能受約 15.6 ms 的 tick 影響（W4：沒有 SDL 時 `condition_variable` P50 5.28 ms、`sleep_until` 15.63 ms），這指向候選 3（產品的計時器），但重送次數沒有印出，還不能判斷。
- 出貨的 Client 會初始化 SDL（W4：有 SDL 時 `sleep_until` P50 約 1.08 ms），這個失敗是在沒有 SDL 的探針裡量到的。

## C：印出重送的節奏，並用 Client 自己的送出時刻對照（D224）

使用者：「做 C：先在重送檢查的訊息印出重送次數與間隔，並在 Client 端記錄實際送出的時刻，重新宣告後在 Windows 再跑。」

- **改法（只加觀測，門檻與產品程式都不改）**：worker 探針在重送的那 1 秒裝上 Client 既有的 movement trace（`SetMovementTrace` 與記憶體內的 `MovementTrace`），收集 Client 每次送出的 `Sent` 事件（每次送出取一個 `startedNs`）。重送檢查（`resends` 在 55～65）失敗時，訊息附上 `mock_resends=N mock_gap_ms=最小/中位/最大 client_sends=N client_gap_ms=最小/中位/最大`。Client 的送出時刻來自產品本來就有的 trace，沒有新增產品的入口。
- **macOS（Intel）本機**：把門檻暫時改成必定失敗，確認訊息是 `mock_resends=59 mock_gap_ms=16.60/16.73/29.18 client_sends=59 client_gap_ms=16.70/16.72/29.19`，之後改回；正常跑通過。
- **Windows 的事前宣告（2026-10-28 核准，D228；依 D226、D227 更新後核准）**：使用者：「核准 worker-windows-timing.md 中 C 的 Windows 重新宣告，記錄後推送，我在 Windows 上跑。」以下內容核准後不改，跑完不放寬（AGENTS.md §14.1）。
  - 建置：Windows 機器、MSVC（vcvars64 環境，`cmake --preset ci-windows`、`cmake --build build/target/_build/ci-windows`）。發行線 `claude/pvp-v71-release`，必須含 `0d6916a8`（C、D226 的觀測、D227 的修正都在其中）。記錄建置的 commit 與 MSVC 版本；`git ls-files --eol` 沒有 `w/crlf`、工作樹乾淨。
  - 跑法：`ctest --test-dir build/target/_build/ci-windows -C Release -R "^object_fps_pvp\.worker$" --output-on-failure`，串行 10 次，不重跑、不中途改，期間不做其他吃重的工作。每次的輸出完整保存（D225 漏存 log 的教訓）。
  - 每次記錄：
    - ctest 摘要的通過或失敗、完整的失敗訊息（失敗的斷言以訊息辨認，不用行號）。
    - 重送檢查（「unacknowledged window was not resent at approximately 60 Hz」）失敗時，括號裡的四個值：`mock_resends`、`mock_gap_ms`、`client_sends`、`client_gap_ms`。
    - 輸出裡有「v7-08 wake timeline」這一行時，照抄（D226 的觀測；只在流程跑到 v7-08 的檢查時才有）。
  - 收支：每一次都要有 ctest 的結果。重送檢查失敗的那幾次，四個值都要有。
  - 判定（互斥，依序取第一個成立的）：
    1. 「確認不了」：建置失敗、沒有含 `0d6916a8`，或有任何一次沒有 ctest 的結果，或重送檢查失敗卻沒有四個值。
    2. 「全部通過」：10 次都通過。
    3. 「產品的節奏」：至少一次失敗在重送檢查，而且在所有失敗在重送檢查的次數裡，`client_sends` 都不在 55～65。結論：沒有 SDL 的 Windows 程序裡，Client 自己的重送節奏就不是 60 Hz（產品的計時器）。
    4. 「觀測側」：至少一次失敗在重送檢查，而且在所有失敗在重送檢查的次數裡，`client_sends` 都在 55～65（只有 `mock_resends` 超出）。結論：Client 照 60 Hz 送，問題在 mock 或傳輸。
    5. 「其他」：以上都不成立（例如有失敗在別的斷言，包括 v7-08 的檢查，或 3、4 混在一起）。照實記錄，交使用者判斷。
  - 流程的順序（讀程式碼）：重送檢查在 `main` 裡，v7-08 的檢查在之後的 `CheckActions`。重送檢查失敗的那幾次，v7-08 不會跑到，也就沒有時間線。
  - 範圍的限界：
    - 只有一台 Windows 機器；worker 探針沒有 SDL（和出貨的 Client 不同）；只看 worker 這一項。
    - Client 的送出時刻是 `Send` 開始時的 steady_clock，不是網卡實際送出的時刻。
    - D226 有 2 次是 mock 看不到的計時器喚醒，來源沒有找出來。
    - 同一段的 ACK 檢查（「an advanced ACK waited for an unrelated wake」）可能有和 v7-08 相同的相位問題，沒有量。
    - v7-08 的時機修正（D227）只在 macOS 用突變確認過；Windows 上的突變不在這次的範圍。

## v7-08 檢出力的比較（D225，事前宣告 2026-10-28 核准）

使用者：「核准 worker-windows-timing.md 中 v7-08 比較的事前宣告（D225），開始跑。」以下內容核准後不改，跑完不放寬。

**起因**：C 的突變（D184：worker 為檢查的 47 個）裡，`v7-08-submit-no-wake`（SubmitAction 不喚醒網路執行緒）存活。追加的單獨重跑：C 的版本 5 次中 2 次被殺；B 的版本（`f29001e0`）3 次中 2 次；B 之前的版本（`e400f896`，mock 用 `sleep_for`）4 次中 4 次。樣本少，不能下結論。依 AGENTS.md §14 開頭（可能是回歸）先停下；C 沒有推送。使用者：「先量清楚：事前宣告次數與判定，在 B 之前與 B 之後各跑 v7-08 的突變，比較殺死率，再決定。」

- **比較的兩個版本**（只換 worker 探針的兩個檔：`build/acceptance/object_fps_pvp/worker_main.cpp` 與同目錄的 `CMakeLists.txt`；產品與突變目錄都用發行線的現況）：
  - A（B 之前）：`e400f896` 的兩個檔。
  - B（B 之後）：`f29001e0` 的兩個檔。
- **跑法**：本機 macOS（Intel），`build/target/_build/test`，用產品的突變 runner `--only v7-08-submit-no-wake`。A、B 交錯（ABAB…），各 20 次，共 40 次，串行。每次前記錄 load average；跑的期間我不做其他工作（另一個會話的活動控制不了，照實記錄）。
- **每次的結果**：killed、survived，或其他（wrong_failure、build_failed、基準線失敗）。
- **收支**：40 次都要有結果；「其他」照實記錄。
- **判定**（互斥，依序取第一個成立的；A、B 的殺死數記為 kA、kB）：
  1. 「確認不了」：任一版本的「其他」超過 2 次。
  2. 「B 削弱了檢出」：kA ≥ 18 且 kB ≤ 14。
  3. 「沒有差別」：|kA − kB| ≤ 3。
  4. 「判定不了」：以上都不成立。
- **之後的處理（依判定，再交使用者決定）**：2 → B 的改法要修（例如這個檢查期間的 mock 輪詢方式，或讓檢查不依賴偶然的喚醒），C 跟著重做突變；3 → 這條突變的檢出本來就不穩，另外記錄，C 照常；1、4 → 交使用者。
- **範圍的限界**：只在 macOS（Intel）；同一台 Mac 上其他會話的活動不受控；20 次只能分辨相當大的差別。

### 結果（2026-10-28，本機 macOS Intel）

收支：40 次全部有結果。A 20 次：killed 20；B 20 次：killed 5、survived 14、其他 1（第 1 輪的 wrong_failure）。

- **判定：「B 削弱了檢出」**（判定 2：kA＝20 ≥ 18 且 kB＝5 ≤ 14；判定 1 不成立：「其他」A 0 次、B 1 次）。
- 每輪（ABAB 交錯）：A 每輪都 killed。B 在第 2、3、13、15、20 輪 killed，第 1 輪 wrong_failure，其他 14 輪 survived。存活沒有集中在某一段時間。
- load average（1 分鐘）在 1.68～3.52 之間；A 與 B 的範圍重疊（A 1.75～3.52，B 1.68～2.55），load 高的輪次 A 也全部 killed。負載不能解釋這個差別。
- B 的 wrong_failure 的失敗訊息沒有留下：跑法只記錄 runner 的結果行，沒有存 log。這是跑法的缺口，記錄下來，不補跑。
- 證據：`~/gyo-v708/results.txt`、`run.sh`（使用者的 Mac，本機保存、不在 repo；說明見 `~/gyo-v708/README.md`）。跑完後兩個檔已還原到發行線的版本（`git status` 乾淨）。
- 機制還沒有確認。檢查（`worker_main.cpp` 的「an eligible action waited for an unrelated wake」）只要求 SubmitShot 後 40 ms 內 mock 收到這個 action。拿掉喚醒後仍能在 40 ms 內送出，表示有別的事件喚醒了網路執行緒。B 讓 mock 的輪詢方式改變，可能改變了 mock 回應 Client 的時機，產生了這樣的「無關的喚醒」。這只是假設，沒有量。
- 依宣告的「之後的處理」：B 的改法要修，C 跟著重做突變。修法交使用者決定。

## v7-08 的喚醒機制（D226，觀測，2026-10-28）

使用者：「先量機制：在 B 的版本記錄 SubmitShot 前後是什麼喚醒了網路執行緒，只加觀測，不修。」這是探索用的觀測，不是受入判定，沒有事前宣告門檻。

收支：50 次全部有結果，每次都印出了時間線。A 突變 20 次（killed 18、survived 2），B 突變 20 次（killed 6、survived 14），A、B 原版各 5 次（全部通過）。

- **觀測方法**：只改 worker 探針。mock 記錄自己送出的每個封包（`sent`），並在 v7-08 的檢查前印一行時間線：SubmitShot 前 20 ms 到 action 到達之間，mock 收到（in）與送出（out）的封包，時刻以 SubmitShot 為 0（ms）。產品程式不改。A（`e400f896`）與 B（`f29001e0`）的探針加上相同的觀測，各建原版與突變版（`v7-08-submit-no-wake` 的替換），四個執行檔交錯跑。
- **結果（每次突變的時間線）**：
  - 被殺的 24 次：Hello 在 SubmitShot **之前** 0.2～3.1 ms 到達，mock 立刻回 Welcome 與 Snapshot。之後網路執行緒沒有其他喚醒，action 要等到下一次 Hello（約 245～249 ms 後）才送出。
  - 存活的 16 次：14 次是 Hello 在 SubmitShot **之後** 0.2～2.0 ms 到達（Client 送 Hello 的那一輪把 action 一起送出），或 Hello 在 0 ms 附近、而 mock 的回覆在 SubmitShot 之後才到（A 第 10 次，回覆喚醒的那一輪送出 action）；2 次（B 第 5、9 次）在 action 之前 mock 沒有收發任何封包，action 在 0.27 ms 與 1.06 ms 送出，是 mock 看不到的喚醒（計時器）。
  - 原版：action 在 0.12～1.15 ms 送出（SubmitShot 自己的喚醒）。
- **機制**：「無關的喚醒」是 Client 自己每 250 ms 的 Hello（`ClientConnection.cpp` 的 `HelloInterval`），包括送 Hello 的那一輪，以及 mock 回覆抵達時的那一輪。這一步的測試流程讓 SubmitShot 落在某次 Hello 的前後約 3 ms 之內（50 次中 48 次）。突變能不能被殺，取決於 SubmitShot 在那次 Hello 之前還是之後，差距不到 1 ms。
- **A 與 B 的差別**：A 的 Hello 大多落在 SubmitShot 之前（突變 20 次中 18 次），B 大多落在之後。B 讓測試流程的時間線和 Client 的 Hello 計時器之間的相位錯開了大約 1～2 ms。相位為什麼會錯開，沒有量；我的假設是 mock 的等待方式改變了每次 `UntilTime` 超過期限的量。
- **意義**：A 的檢出也是靠相位碰巧（這次 A 也存活了 2 次；D225 的 A 是 20／20）。這項檢查的 40 ms 判定本身沒有問題，問題在於 SubmitShot 的時刻沒有避開 Client 的 Hello。B 沒有改產品，只是讓這個弱點浮上來。
- **旁註（沒有處理）**：同一段的 ACK 檢查（「an advanced ACK waited for an unrelated wake」）註解寫「next Hello, up to a second away」，但 `HelloInterval` 是 250 ms。這項檢查可能也有同樣的相位問題，沒有量。
- 證據：`~/gyo-v708/obs/`（使用者的 Mac，本機保存、不在 repo；說明見 `~/gyo-v708/README.md`）的 `results.txt`、`logs/`（每次的完整輸出）、`run.sh`、`worker_A_obs.cpp`、`worker_B_obs.cpp`。發行線的探針保留這行時間線（只輸出到 stderr，不影響判定）。

## v7-08 檢查的時機修正與突變確認（D227，事前宣告 2026-10-28 核准）

使用者：「修檢查的時機：等 mock 收到 Hello 後再隔約 100 ms 才 SubmitShot；先寫好突變確認的事前宣告給我核准。」核准：「核准 worker-windows-timing.md 中 D227 的事前宣告，開始跑。」核准前沒有跑任何量測（只建置過）；以下內容核准後不改，跑完不放寬。

- **修正（只改 `build/acceptance/object_fps_pvp/worker_main.cpp`，產品不改）**：v7-08 的檢查原本在閒置 50 ms 後立刻 SubmitShot。改成閒置 50 ms 後，等 mock 收到下一個 Hello，從那個 Hello 的到達時刻起再等 100 ms 才 SubmitShot。前一次 Hello 在 100 ms 前，下一次大約在 150 ms 後，兩邊都離 40 ms 的判定很遠。40 ms 的判定與失敗訊息都不改。
- **判定的變更（AGENTS.md §14.4）**：
  1. 守的性質：可送出的 action 在 SubmitAction 時就送出（D36），不等無關的喚醒。
  2. 放寬的面：預期沒有。判定的式子不變，只改 SubmitShot 的時刻。舊判定殺死的突變（`v7-08-submit-no-wake`），新判定也必須殺死（下面第 1、2 項）。
  3. 收緊的面：SubmitShot 不再碰上 Client 的 Hello，突變不會因相位碰巧而存活。
  4. （確認沒有放寬面之後不適用）
  5. 證據：本節的結果，以及 D226 的時間線。
- **確認的項目與判定**（本機 macOS Intel，串行；每次保存完整輸出；每次前記錄 load average）：
  1. **發行線版本（B＋C＋觀測＋本修正）的突變 20 次**：`v7-08-submit-no-wake` 的突變版探針直接執行 20 次。killed＝結束碼非 0 且輸出含「an eligible action waited for an unrelated wake」；survived＝結束碼 0；其他＝其餘情況。**通過＝20／20 killed**。
  2. **A 的探針（`e400f896`＋觀測＋本修正）的突變 10 次**：確認檢出不再依賴 mock 的等待方式。分類同上，**通過＝10／10 killed**。
  3. **沒有突變的發行線版本 10 次**：worker 探針直接執行。**通過＝10／10 結束碼 0**（修正沒有讓正常的 action 判成失敗）。
  4. **D184 的 worker 突變**：被 worker 檢查殺死的 47 個突變（C 與本修正都改了 `worker_main.cpp`），用產品的突變 runner 跑一次。**通過＝47／47 killed**。wrong_failure 與 survived 依 D185 分類：wrong_failure 只單獨重跑 1 次，killed 就記為環境；survived 不重跑，直接列為不通過。
- **整體**：1～4 全部通過，才算「修正有效」。任何一項不通過，就不再重跑、不修改，停下來回報（附時間線與輸出）。
- **之後（依結果，再交使用者決定）**：通過的話，推送 C、觀測、本修正與文件，接著請你核准 C 的 Windows 重新宣告（另一份，還沒核准）。
- **範圍的限界**：
  - 只在 macOS（Intel）；Windows 上的 v7-08 不在這次的範圍。
  - D226 有 2 次是 mock 看不到的計時器喚醒，來源沒有找出來。這次的修正沒有處理它；如果它讓突變存活，第 1、2 項會不通過。
  - 同一段的 ACK 檢查（「an advanced ACK waited for an unrelated wake」）可能有同樣的相位問題，不在這次的範圍，沒有量。
  - 同一台 Mac 上其他會話的活動控制不了，照實記錄。
- **估計時間**：第 1～3 項約 20 分鐘，第 4 項約 70 分鐘。

### 結果（2026-10-28，本機 macOS Intel）

收支：一致。第 1～3 項 40 次都有結果，而且都印出了時間線；第 4 項 47 個突變都有結果。「其他」0 次。

- **判定：「修正有效」**（1～4 全部通過）。
  1. 發行線版本的突變：20／20 killed。action 在 SubmitShot 後 147.9～150.0 ms 才送出，也就是下一次 Hello。
  2. A 的探針（`e400f896`＋觀測＋本修正）的突變：10／10 killed，action 在 147.9～150.2 ms 送出。這表示檢出已經不依賴 mock 的等待方式。
  3. 沒有突變的發行線版本：10／10 結束碼 0，action 在 0.13～0.37 ms 送出。
  4. worker 的 47 個突變（產品的突變 runner）：47／47 killed，沒有重跑。
- load average（1 分鐘）：第 1～3 項在 1.02～3.56 之間；第 4 項開始時 2.33、結束時 2.19。
- §14.4 的確認：舊判定殺死的突變（`v7-08-submit-no-wake`），新判定在 A、B 兩種探針上都殺死；worker 的其他 46 個突變也都被殺。沒有發現放寬的面。
- D226 的 2 次計時器喚醒，這 30 次突變中沒有出現。來源仍然沒有找出來，範圍的限界不變。
- 證據：`~/gyo-v708/d227/`（使用者的 Mac，本機保存、不在 repo；說明見 `~/gyo-v708/README.md`）的 `results.txt`、`logs/`、`run.sh`、`worker_A_fix.cpp`、`worker47.json`、`worker47.log`、`worker47.load`。

## C 的 Windows 結果（2026-10-28，Windows 機器，D228 的宣告）

收支：一致（10 次都有 ctest 的結果；10 次都失敗在重送檢查，每次四個值都有）。

- **判定（照宣告的互斥順序，取第一個成立的）：3.「產品的節奏」**。
  - 1「確認不了」不成立：建置成功、含 `0d6916a8`、10 次都有 ctest 的結果、重送檢查失敗的 10 次四個值都有。
  - 2「全部通過」不成立：10 次都失敗。
  - 3 成立：10 次都失敗在重送檢查，這 10 次的 `client_sends` 都是 43，不在 55～65。
- **建置**：
  - 發行線 `021026858802e2fdab877e75b24c9ab34cd8feda`，主 checkout。`git merge-base --is-ancestor 0d6916a8 HEAD` 成立；`git ls-files --eol` 沒有 `w/crlf`，工作樹乾淨。
  - 在 VS Community 2026 的 vcvars64 環境裡：`cmake --preset ci-windows` exit 0、`cmake --build build/target/_build/ci-windows` exit 0（增量 31 步，含 `worker_main.cpp`；22:09～22:11）。MSVC 19.51.36260。
  - vcvars64 印了一行 `'vswhere.exe' is not recognized as an internal or external command`，但 `cl` 有找到，configure 與 build 都成功（只記錄）。
- **機器**：Windows 11 Pro 10.0.26300，Intel Core i7-8750H（6 核 12 執行緒，筆電），16 GB；接著 AC（`PowerOnline=True`），電源計畫「平衡」（`381b4222-…`）。和第 3 步、B 是同一台。
- **跑法**：照宣告的指令 `ctest --test-dir build/target/_build/ci-windows -C Release -R "^object_fps_pvp\.worker$" --output-on-failure`，串行 10 次（22:13:11～22:16:27），沒有重跑，中途沒有改任何東西，期間沒有其他吃重的工作。

| 次 | 結果（ctest 摘要） | ctest exit | 耗時 s | `mock_resends` | `mock_gap_ms`（最小/中位/最大） | `client_sends` | `client_gap_ms`（最小/中位/最大） |
|---|---|---|---|---|---|---|---|
| 1 | 失敗 | 8 | 19.09 | 43 | 16.74/28.32/29.67 | 43 | 16.80/28.48/29.66 |
| 2 | 失敗 | 8 | 18.81 | 43 | 16.80/27.85/29.92 | 43 | 16.78/27.85/29.66 |
| 3 | 失敗 | 8 | 18.88 | 43 | 16.72/28.35/29.55 | 43 | 16.74/28.38/29.49 |
| 4 | 失敗 | 8 | 18.87 | 43 | 16.76/28.60/29.67 | 43 | 16.80/28.62/29.67 |
| 5 | 失敗 | 8 | 18.91 | 43 | 16.75/28.69/29.72 | 43 | 16.77/28.71/29.74 |
| 6 | 失敗 | 8 | 18.89 | 43 | 16.79/27.14/30.01 | 43 | 16.80/27.23/29.97 |
| 7 | 失敗 | 8 | 18.95 | 43 | 16.27/27.36/29.94 | 43 | 16.96/27.39/29.73 |
| 8 | 失敗 | 8 | 18.95 | 43 | 16.57/28.13/29.99 | 43 | 16.77/28.52/29.97 |
| 9 | 失敗 | 8 | 18.94 | 43 | 16.79/27.22/30.08 | 43 | 16.82/27.21/30.02 |
| 10 | 失敗 | 8 | 18.94 | 43 | 16.71/27.24/30.30 | 43 | 16.92/27.68/30.13 |

- 失敗訊息（10 次相同，括號裡的值見上表）：`worker acceptance: unacknowledged window was not resent at approximately 60 Hz (mock_resends=… mock_gap_ms=… client_sends=… client_gap_ms=…)`。
- 除了這一句與 ctest 的摘要，`[ObjectFPS/PvP]` 以外沒有其他訊息。
- 「v7-08 wake timeline」：10 次都沒有出現（宣告：重送檢查失敗的那幾次，v7-08 不會跑到）。
- **判讀交給 v7.1 會話**（Windows 端不做）。宣告、門檻、測試、產品程式都沒有改。

**證據**（留在 Windows 機器上，不在 repo）：主 checkout 的 `build/target/_build/test/logs/worker-C-20261028/`，內含 `build.log`、`build-exit.txt`（建置的結束碼）、`commit.txt`、`build-commit.txt`、`msvc-version.txt`、`machine.txt`、`run1.log`～`run10.log`、`runs.txt`（`ctest_exit=` 是 ctest 的結束碼）。

## C 的判讀與停止（2026-10-28，v7.1 會話）

收支：一致（Windows 的紀錄：10 次都有結果，每次四個值都有）。判定照宣告是 3「產品的節奏」，我核對過表格，判定成立。依 AGENTS.md §14 開頭停下：原因的說明會改變計畫的前提（D216 原本假設是測試的問題，現在是產品的計時器），所以不修改，交給使用者決定。

- **觀測側沒有問題**：10 次的 `mock_resends` 和 `client_sends` 都是 43，兩邊的間隔也幾乎相同（差 0.3 ms 以內）。mock 收到的就是 Client 送出的，Client 在 1 秒內只送了 43 次。
- **間隔的分布符合 15.6 ms 的 tick**（讀程式碼與表格推得，沒有另外量）：
  - Client 的網路執行緒等下一次重送（每 16.67 ms）時，先用 asio 計時器等到期限前 2 ms（`ClientConnection.cpp` 的 `CoarseWakeLead`），最後 2 ms 才交給 Waiter。
  - 在沒有 SDL 的 Windows 程序裡，asio 計時器的 14.67 ms 等待會被進位到下一個約 15.6 ms 的 tick，實際醒來落在 14.67～30.3 ms 之間。醒得比 16.67 ms 早，Waiter 會等到 16.67 ms；醒得比它晚，就直接送。
  - 所以間隔應落在 16.67～30.3 ms 之間。表格的最小值 16.27～16.96 ms、最大值 29.49～30.30 ms，和這個範圍一致；平均約 23 ms，1 秒約 43 次。
- **出貨的 Client 會不會這樣（推論，沒有量）**：出貨的 Client 在同一個程序裡初始化 SDL。W4 量到有 SDL 時 `sleep_until` 的 P50 約 1.08 ms，表示 SDL 把程序的計時器解析度提高了。repo 裡沒有自己設定計時器解析度的程式，靠的是 SDL 的預設。如果這個推論成立，出貨的 Client 在 Windows 上照 60 Hz 重送；沒有 SDL 的程序（例如這個探針）則不會。
- **還沒量的**：有 SDL 時，worker 的這條路徑實際的重送次數。
- **範圍的限界**（之後的宣告照抄，直到解消）：
  - Client 網路執行緒的 60 Hz 重送，在 Windows 上依賴程序的計時器解析度（靠 SDL 提高）。沒有 SDL 的程序只有約 43 Hz。
  - `object_fps_pvp.worker` 在 Windows 上停在重送檢查，之後的檢查（包括 v7-08 的 `CheckActions`）在 Windows 上都沒有跑到。

## 出貨條件下的重送（D229，事前宣告 2026-10-28 核准）

使用者：「做 A：今晚做好開著 SDL 視窗的 worker 探針，寫好它和 lan15 的事前宣告；明天 Windows 先跑探針，通過才跑 lan15。」之後：「這個今晚就跑完。」核准：「核准 D229 的事前宣告，寫進文件後推送，我今晚在 Windows 上跑。」以下內容核准後不改，跑完不放寬（AGENTS.md §14.1）。

- **突變（D184）**：探針的變更（`1c0f892d`）之後，worker 檢查的 47 個突變在本機重跑中；推送時還沒跑完，結果記在下面。判定的程式沒有改。
  - 結果（2026-10-28，本機 macOS Intel，load average 1.8～2.0）：46／47 killed。`sb-c3-early-results` 是 wrong_failure（worker 失敗在「action worker emitted overdue catch-up packets」）；單獨重跑 1 次 killed。這個失敗訊息在 D185 的完整突變裡也出現過一次（當時 load 很高）。這次 load 不高，原因沒有切開（實作、測試本身或環境），照實記錄；在沒有突變的 worker 上沒有觀察到。證據：`~/gyo-v708/d229/`（本機保存，`worker47.json`、`worker47.log`、`rerun-sb-c3.json`）。

- **目的**：D228 量到沒有 SDL 的 Windows 程序裡，Client 1 秒只重送 43 次。出貨的 Client 會開 Engine 的 SDL 視窗，而 SDL 會提高程序的計時器解析度。這次在「開著 SDL 視窗」的條件下量重送次數，判斷 43 次是不是出貨版的缺陷。
- **探針（`1c0f892d`）**：
  - `gyo_object_fps_pvp_worker_window_probe`：和 worker 探針是同一份 `worker_main.cpp`，另外定義 `GYO_WORKER_PROBE_WINDOW`、連結 `GYO::PlatformSDL`。開頭用 `SdlPlatform::Create` 開一個 640×360 的視窗（和 Client、W4 的 window 組相同的初始化路徑），處理事件 1 秒讓視窗出現，印出「worker probe: Engine SDL window open」，之後不再處理事件，照常跑完整個 worker 驗收。
  - 兩支探針都在重送的那 1 秒結束後，每次印出「worker resend rhythm: …」（四個值），不再只在失敗時印。判定與門檻都不改。
  - 不是 CTest，也不是驗收的角色，只在這份宣告下手動執行。
  - macOS（Intel）本機試跑各 1 次（不算判定）：兩支都通過，`client_sends=59`。
- **建置**：Windows 機器、MSVC（vcvars64 環境，`cmake --preset ci-windows`、`cmake --build build/target/_build/ci-windows`）。發行線 `claude/pvp-v71-release`，必須含 `1c0f892d`。記錄建置的 commit 與 MSVC 版本；`git ls-files --eol` 沒有 `w/crlf`、工作樹乾淨。
- **跑法**：
  - 在有桌面的登入工作階段裡（視窗需要顯示器），直接執行兩個執行檔（不經 ctest），交錯 W、P、W、P…（W＝window 探針，P＝沒有視窗的探針），各 10 次，共 20 次，串行。
  - 執行檔在 `build/target/_build/ci-windows/acceptance/object_fps_pvp/` 之下（多組態時在 `Release/` 裡）。window 探針旁邊要有 SDL 的 DLL（建置會複製）。
  - 視窗出現後不要點它、不要移動或縮小；Windows 顯示「沒有回應」也照樣不碰。期間不做其他吃重的工作。
  - 不重跑、中途不改任何東西。每次的完整輸出各存一個檔。
- **每次記錄**：結束碼；有沒有「Engine SDL window open」（只限 W）；「worker resend rhythm」那一行的四個值；失敗時完整的失敗訊息（用訊息辨認，不用行號）；有「v7-08 wake timeline」時照抄。
- **收支**：20 次都要有結束碼。W 的 10 次都要有「window open」那一行。
- **判定**（互斥，依序取第一個成立的；只看 W，P 是對照）：
  1. 「確認不了」：建置失敗、沒有含 `1c0f892d`、有任何一次沒有結束碼，或 W 有任何一次沒有「window open」那一行。
  2. 「出貨條件下是 60 Hz」：W 的 10 次都有節奏行，`client_sends` 都在 55～65；而且 P 的 10 次都有節奏行，`client_sends` 都不在 55～65（對照重現了 D228）。
  3. 「出貨條件下也不是 60 Hz」：W 的 10 次都有節奏行，`client_sends` 都不在 55～65。
  4. 「其他」：以上都不成立（例如 W 有失敗在重送之前的斷言而沒有節奏行、W 混雜、P 沒有重現 D228）。照實記錄，交使用者判斷。
- **明天能不能接著跑 lan15**（使用者：「通過才跑 lan15」）：判定 2，而且 W 的 10 次結束碼都是 0（worker 驗收在出貨條件下整個通過，包括之前在 Windows 沒有跑到的後半段與 v7-08）。其他情況都不跑 lan15，停下回報。
- **之後（依結果，再交使用者決定）**：
  - 2 且 W 全部通過：43 次只發生在沒有 SDL 的程序，出貨版不受影響；記為已知限界（Client 的 60 Hz 在 Windows 上依賴 SDL 提高的計時器解析度），接著跑 lan15。沒有 SDL 的 worker 探針在 Windows 上的處理，另外交使用者決定。
  - 3：出貨版的缺陷。v7.1 要修，修法另外交使用者決定（D229 之前整理的兩個方向：Engine OS 層設定計時器解析度，與 D78 衝突；或 `ClientConnection` 改用高解析度的計時執行緒）。
  - 1、4：交使用者。
- **範圍的限界**：
  - 只有一台 Windows 機器；只看 worker 這一項。
  - window 探針只在開頭處理事件 1 秒，之後不處理；出貨的 Client 每幀都處理。這次只檢驗「SDL 對計時器解析度的影響」，不檢驗事件處理與畫面更新的負載。
  - Client 的送出時刻是 `Send` 開始時的 steady_clock，不是網卡實際送出的時刻。
  - D226 有 2 次是 mock 看不到的計時器喚醒，來源沒有找出來。
  - 同一段的 ACK 檢查（「an advanced ACK waited for an unrelated wake」）可能有和 v7-08 相同的相位問題，沒有量。
  - v7-08 的時機修正（D227）只在 macOS 用突變確認過。
  - 這支 window 探針沒有用突變確認過檢出力：判定看的是重送次數，不是新的斷言；worker 檢查的 47 個突變是在沒有視窗的探針上跑的（D184）。

## 出貨條件下的重送：Windows 結果（2026-10-28，Windows 機器，D229 的宣告）

收支：一致（20 次都有結束碼；W 的 10 次都有「worker probe: Engine SDL window open」）。另外，P7 沒有「worker resend rhythm」那一行（失敗在重送之前，見表），收支的條件不含節奏行，在判定裡處理。

- **判定（照宣告的互斥順序，取第一個成立的）：4.「其他」**。
  - 1「確認不了」不成立：建置成功、含 `1c0f892d`、20 次都有結束碼、W 的 10 次都有 window open。
  - 2「出貨條件下是 60 Hz」不成立：W 的 10 次都有節奏行，`client_sends` 都是 57（在 55～65）；但 P 的 10 次**不是都有節奏行**（P7 沒有），所以 P 那一半不成立。有節奏行的 9 次 P，`client_sends` 是 43～53，都不在 55～65。
  - 3「出貨條件下也不是 60 Hz」不成立：W 的 `client_sends` 都在 55～65。
  - 4 成立。照實記錄，交使用者判斷。
- **能不能接著跑 lan15：不能**。條件是判定 2，而且 W 的 10 次結束碼都是 0；這次是判定 4，W 也有 2 次結束碼 1（W5、W7）。今晚本來就不跑 lan15。
- **建置**：
  - 發行線 `8d32fb670cac01c5e92b5c0153e9ef585687f0e0`，主 checkout。`git merge-base --is-ancestor 1c0f892d HEAD` 成立；`git ls-files --eol` 沒有 `w/crlf`，工作樹乾淨。
  - 在 VS Community 2026 的 vcvars64 環境裡：`cmake --preset ci-windows` exit 0、`cmake --build build/target/_build/ci-windows` exit 0（增量 31 步，23:03～23:04）。MSVC 19.51.36260。vcvars64 照舊印了 `'vswhere.exe' is not recognized …`（cl 有找到）。
  - 執行檔（單一組態，不在 `Release/`）：`build/target/_build/ci-windows/acceptance/object_fps_pvp/` 的 `gyo_object_fps_pvp_worker_window_probe.exe`（SHA-256 `8b2181d2…78b6f`）、`gyo_object_fps_pvp_worker_probe.exe`（`a3060810…a62f9`），旁邊有 `SDL3.dll`（`38a1f37a…80eaec3`）。
- **機器**：Windows 11 Pro 10.0.26300，Intel Core i7-8750H（6 核 12 執行緒，筆電），16 GB；接著 AC（`PowerOnline=True`），電源計畫「平衡」（`381b4222-…`）。和第 3 步、B、C 是同一台。
- **跑法**：
  - 在登入的桌面工作階段裡，從 Claude Code 的 shell 直接執行兩個執行檔（不經 ctest）。沒有參數，工作目錄是執行檔所在的資料夾（和 ctest 對 worker 的設定相同）。
  - W、P 交錯，各 10 次，共 20 次，串行（23:04:27～23:11:41）。沒有重跑，中途沒有改任何東西，期間沒有其他吃重的工作。視窗出現後沒有碰它。
  - 每次外面包了 `timeout 300`（防止卡住；宣告沒有寫），20 次都沒有碰到（每次 11～26 秒）。
  - stdout 與 stderr 合併存成一個檔。

| 次序 | 探針 | 結束碼 | window open | `mock_resends` | `mock_gap_ms`（最小/中位/最大） | `client_sends` | `client_gap_ms`（最小/中位/最大） | 失敗訊息 |
|---|---|---|---|---|---|---|---|---|
| 1 | W1 | 0 | 有 | 57 | 16.77/17.14/29.17 | 57 | 16.77/17.17/29.16 | — |
| 2 | P1 | 1 | — | 45 | 16.85/17.56/30.28 | 45 | 16.85/17.41/30.31 | 重送（a） |
| 3 | W2 | 0 | 有 | 57 | 16.69/17.25/29.32 | 57 | 16.81/17.24/29.32 | — |
| 4 | P2 | 1 | — | 44 | 16.69/17.65/30.28 | 44 | 16.80/17.46/30.15 | 重送（a） |
| 5 | W3 | 0 | 有 | 57 | 16.61/17.16/28.60 | 57 | 16.80/17.18/28.59 | — |
| 6 | P3 | 1 | — | 46 | 16.85/17.62/30.23 | 46 | 16.90/17.52/29.91 | 重送（a） |
| 7 | W4 | 0 | 有 | 57 | 16.53/17.21/28.56 | 57 | 16.81/17.23/28.85 | — |
| 8 | P4 | 1 | — | 45 | 16.81/17.46/30.30 | 45 | 16.86/17.38/30.14 | 重送（a） |
| 9 | W5 | **1** | 有 | 57 | 16.49/17.22/28.42 | 57 | 16.78/17.20/28.43 | **（b）** |
| 10 | P5 | 1 | — | 53 | 16.63/17.25/31.93 | 53 | 16.76/17.20/31.90 | 重送（a） |
| 11 | W6 | 0 | 有 | 57 | 16.76/17.24/30.04 | 57 | 16.80/17.28/29.81 | — |
| 12 | P6 | 1 | — | 53 | 16.37/17.24/33.69 | 53 | 16.73/17.18/34.10 | 重送（a） |
| 13 | W7 | **1** | 有 | 57 | 16.80/17.27/29.10 | 57 | 16.79/17.31/29.46 | **（b）** |
| 14 | P7 | 1 | — | **沒有** | **沒有** | **沒有** | **沒有** | **（c）** |
| 15 | W8 | 0 | 有 | 57 | 16.80/17.25/29.97 | 57 | 16.79/17.26/29.70 | — |
| 16 | P8 | 1 | — | 52 | 16.76/17.25/33.34 | 52 | 16.76/17.21/33.66 | 重送（a） |
| 17 | W9 | 0 | 有 | 57 | 16.70/17.18/29.09 | 57 | 16.75/17.18/29.08 | — |
| 18 | P9 | 1 | — | 43 | 16.85/21.30/30.19 | 43 | 16.84/21.51/30.37 | 重送（a） |
| 19 | W10 | 0 | 有 | 57 | 16.81/17.24/29.33 | 57 | 16.81/17.23/29.36 | — |
| 20 | P10 | 1 | — | 43 | 16.81/19.53/30.24 | 43 | 16.77/19.37/30.17 | 重送（a） |

失敗訊息（照抄）：

- （a）`worker acceptance: unacknowledged window was not resent at approximately 60 Hz (mock_resends=… mock_gap_ms=… client_sends=… client_gap_ms=…)`，括號裡的值和同一次的節奏行相同（見表）。
- （b）W5、W7：`worker acceptance: a token wait outlived four commands`。這兩次的節奏行都有、`client_sends` 都是 57；失敗在重送檢查之後。
- （c）P7：`worker acceptance: 既存の接続はリモート ホストに強制的に切断されました。`（OS 的錯誤訊息，日文 UI；意思是「現有的連線被遠端主機強制關閉」）。只跑了 11 秒，沒有節奏行，失敗在重送檢查之前。log 全部 15 行，除了這一句都是 `[ObjectFPS/PvP]` 的行。

「v7-08 wake timeline」（只有通過的 8 次 W 跑到 v7-08）：

| 次 | 照抄 |
|---|---|
| W1 | `v7-08 wake timeline: action_ms=0.96 events=in:Actions@0.95` |
| W2 | `v7-08 wake timeline: action_ms=0.96 events=in:Actions@0.95` |
| W3 | `v7-08 wake timeline: action_ms=0.97 events=in:Actions@0.96` |
| W4 | `v7-08 wake timeline: action_ms=0.97 events=in:Actions@0.97` |
| W6 | `v7-08 wake timeline: action_ms=0.97 events=in:Actions@0.96` |
| W8 | `v7-08 wake timeline: action_ms=0.97 events=in:Actions@0.97` |
| W9 | `v7-08 wake timeline: action_ms=0.61 events=in:Actions@0.60` |
| W10 | `v7-08 wake timeline: action_ms=0.66 events=in:Actions@0.65` |

- 通過的 8 次 W 都在最後印出 `"passed":true` 的 JSON（全文在各自的 log）。
- **判讀交給 v7.1 會話**（Windows 端不做）。宣告、門檻、測試、產品程式都沒有改。

**證據**（留在 Windows 機器上，不在 repo）：主 checkout 的 `build/target/_build/test/logs/worker-D229-20261028/`，內含 `build.log`、`build-exit.txt`（建置的結束碼）、`commit.txt`、`build-commit.txt`、`msvc-version.txt`、`machine.txt`、`hashes.txt`、`W1.log`～`W10.log`、`P1.log`～`P10.log`、`runs.txt`（`exit=` 是探針的結束碼）。

## 出貨條件下的重送：判讀與停止（2026-10-28，v7.1 會話）

收支：一致（Windows 的紀錄：20 次都有結束碼，W 的 10 次都有 window open）。判定照宣告是 4「其他」，我核對過表格，判定成立；「能不能接著跑 lan15」照宣告是「不能」。依 AGENTS.md §14 開頭停下，不修改，交給使用者決定。

- **判定為什麼是 4**：P7 失敗在重送檢查之前，沒有節奏行，判定 2 的對照條件（P 的 10 次都有節奏行）不成立。宣告沒有預想到「P 失敗在重送之前」這種情況，這是寫宣告時的缺漏。照字面記錄，不重新解釋判定。
- **重送次數本身（只是觀察，不是判定）**：W 的 10 次 `client_sends` 都是 57，間隔中位 17.1～17.3 ms。有節奏行的 9 次 P 是 43～53，都不在 55～65。開著 SDL 視窗時，重送進了 55～65。
- **W5、W7 的新失敗**：「a token wait outlived four commands」。這項檢查在重送檢查之後，之前在 Windows 從來沒有跑到過（第 3 步、B、C 都停在更早的地方）。
  - 它要求排空 token 之後，送出的等待不超過 1.75 ms（`settledWaitMs`），再加上這次發佈比預定晚的部分。
  - 失敗時沒有印出等待的值，所以分不出是產品的等待在 Windows 上比較長，還是門檻對 Windows 太緊。原因還沒判定。
- **P7 的新失敗**：「既存の接続はリモート ホストに強制的に切断されました」（WSAECONNRESET）。讀程式碼（沒有另外驗證）：這句是 mock 的 `Pump` 在 `receive_from` 收到錯誤時丟出的例外（`worker_main.cpp` 的 `if(error)throw std::runtime_error(error.message())`）。Windows 的 UDP socket 在送出的封包收到 ICMP port unreachable 後，下一次接收會回報 WSAECONNRESET。所以這比較像 mock（測試端）在 Windows 上的問題，不是產品。沒有 SDL 的探針在 Windows 上，到目前約 30 次跑到這一段以後，這是第一次出現。
- **v7-08**：跑到的 8 次 W 都通過，action 在 SubmitShot 後 0.60～0.97 ms 送出。D227 的修正在 Windows 上沒有讓正常的 action 判成失敗。
- **範圍的限界**（之後的宣告照抄，直到解消）：
  - 「a token wait outlived four commands」在 Windows（開著 SDL 視窗）上 10 次失敗 2 次，原因未判定；失敗時沒有印出等待的值。
  - mock 的 `Pump` 遇到 WSAECONNRESET 會讓整個 worker 驗收中止（讀程式碼的推論）。
  - D228 已記的：沒有 SDL 的 Windows 程序裡，Client 的重送約 43～53 Hz。

## token 等待的觀測（D230，事前宣告 2026-10-29 核准）

使用者：「先觀測 token 等待：在 token 的兩項檢查失敗時印出每步的等待與間隔，只加觀測；寫好事前宣告給我核准，再在 Windows 重跑視窗版。」核准：「核准 D230 的事前宣告，寫進文件後推送，我在 Windows 上跑。」以下內容核准後不改，跑完不放寬（AGENTS.md §14.1）。

- **起因**：D229 的視窗版 W5、W7 失敗在「a token wait outlived four commands」，失敗時沒有值，分不出是產品的等待在 Windows 上比較長，還是門檻對 Windows 太緊。
- **觀測（`bff5bf36`，只改 `worker_main.cpp`，判定與門檻都不改）**：
  - token 的兩項檢查（「the waiting sends did not drain the token debt」與「a token wait outlived four commands」）失敗時，訊息括號裡附上一行值。
  - 同一行在每次執行時也印到 stderr：「worker token waits: …」，通過的那幾次也看得到。這是使用者所說「失敗時印出」之外多加的，目的是能和通過的執行比較。
  - 值的內容：
    - `waits_ms`：排空的 8 步，每一步的等待。
    - `intervals_ms`：相鄰兩次發佈的間隔。
    - `drained_ms`：第 1～3 步各排空了多少（目標是四分之一步，4.17 ms，容許 ±1.00 ms）。
    - `drain_excess_ms`：排空那項檢查最差的一步超過容許的量。
    - `settled_limits_ms`：第 4～7 步各自的上限（1.75 ms，加上這次發佈晚到的部分）。
    - `settled_excess_ms`：「outlived」那項檢查最差的一步超過上限的量。
    - 正值表示那項檢查失敗，負值表示還有餘裕。
  - macOS（Intel）本機試跑各 1 次（不算判定）：兩支都通過，`drain_excess_ms=-0.92`，`settled_excess_ms` 為 -1.60 與 -1.61。
- **突變（D184）**：改動後，worker 檢查的 47 個突變在本機重跑（2026-10-28，macOS Intel，load average 2.0～3.0）：47／47 killed，沒有重跑。證據：`~/gyo-v708/d230/`（本機保存，`worker47.json`、`worker47.log`）。
- **建置**：Windows 機器、MSVC（vcvars64 環境，`cmake --preset ci-windows`、`cmake --build build/target/_build/ci-windows`）。發行線 `claude/pvp-v71-release`，必須含 `bff5bf36`。記錄建置的 commit 與 MSVC 版本；`git ls-files --eol` 沒有 `w/crlf`、工作樹乾淨。
- **跑法**：
  - 在有桌面的登入工作階段裡，直接執行 `gyo_object_fps_pvp_worker_window_probe.exe`（不經 ctest），串行 20 次。這次不跑沒有視窗的版本。
  - 視窗出現後不碰它（同 D229）。期間不做其他吃重的工作。
  - 不重跑、中途不改任何東西。每次的完整輸出各存一個檔。
- **每次記錄**：結束碼；有沒有「Engine SDL window open」；「worker token waits」那一行全文；「worker resend rhythm」的四個值；失敗時完整的失敗訊息（用訊息辨認，不用行號）；有「v7-08 wake timeline」時照抄。
- **收支**：20 次都要有結束碼與「window open」。跑到 token 那段的每一次都要有「worker token waits」那一行。
- **判定**（互斥，依序取第一個成立的）：
  1. 「確認不了」：建置失敗、沒有含 `bff5bf36`、有任何一次沒有結束碼或沒有「window open」，或有任何一次失敗在 token 的兩項檢查之一、卻沒有那一行值。
  2. 「全部通過」：20 次結束碼都是 0。
  3. 「超過在 1 ms 以內」：至少一次失敗；所有失敗都在 token 的兩項檢查；每一次失敗，失敗那項的超過量（`drain_excess_ms` 或 `settled_excess_ms`）都在 0～1.00 ms 之間。
  4. 「超過多於 1 ms」：至少一次失敗；所有失敗都在 token 的兩項檢查；至少有一次，失敗那項的超過量大於 1.00 ms。
  5. 「其他」：以上都不成立（例如有失敗在別的斷言、像 D229 的 P7 那樣的 WSAECONNRESET，或第三項 token 檢查「the third new command did not wait for a token」）。照實記錄，交使用者判斷。
- **1 ms 的來源**：開著 SDL 視窗時，程序的計時器解析度約 1 ms（W4：有 SDL 時 `sleep_until` 的 P50 約 1.08 ms）。超過量在一個計時器刻度以內，和「門檻是在 macOS 上定的（v7 第 09a 批），對 1 ms 的計時器太緊」的讀法相容；超過一個刻度以上，這個讀法解釋不了，比較指向產品在 Windows 上的等待。兩種都只是讀法，判定本身只分類超過量。
- **之後（依結果，再交使用者決定）**：lan15 不會自動接著跑。D229 的 2 次失敗照 §14.1 保留，不被這次的結果覆蓋；能不能跑 lan15 由使用者看完結果決定。
  - 2：20 次都沒有重現，失敗率很低或和那次的環境有關。
  - 3：比較像門檻對 Windows 太緊。要不要改門檻，是 §14.4 的放寬，要另外經使用者核准（含突變檢查與獨立審查）。
  - 4：比較像產品在 Windows 上的等待，要另外排查。
  - 1、5：交使用者。
- **範圍的限界**：
  - 只有一台 Windows 機器；只看 worker 的視窗版。
  - 視窗版只在開頭處理事件 1 秒（同 D229）。
  - mock 的 `Pump` 遇到 WSAECONNRESET 會讓整個驗收中止（D229 P7，讀程式碼的推論）；這種情況會落在「其他」。
  - 沒有 SDL 的 Windows 程序裡，Client 的重送約 43～53 Hz（D228、D229）。
  - D226 有 2 次是 mock 看不到的計時器喚醒，來源沒有找出來。
  - ACK 檢查的相位問題沒有量；D227 只在 macOS 用突變確認過。
  - 視窗版本身沒有用突變確認過檢出力。
  - Client 的送出時刻是 `Send` 開始時的 steady_clock；token 等待量的是 mock 收到的時刻減去發佈的時刻（包含 loopback 的傳遞）。

## token 等待的觀測：Windows 結果（2026-10-29，Windows 機器，D230 的宣告）

收支：一致（20 次都有結束碼與「worker probe: Engine SDL window open」；跑到 token 那段的 19 次都有「worker token waits」那一行。W11 在 token 那段之前就失敗了，沒有這一行）。

- **判定（照宣告的互斥順序，取第一個成立的）：5.「其他」**。
  - 1「確認不了」不成立：建置成功、含 `bff5bf36`、20 次都有結束碼與 window open；失敗在 token 檢查的 8 次都有那一行值。
  - 2「全部通過」不成立：9 次結束碼是 1。
  - 3「超過在 1 ms 以內」與 4「超過多於 1 ms」都不成立：兩者都要求「所有失敗都在 token 的兩項檢查」，但 W11 失敗在 `既存の接続はリモート ホストに強制的に切断されました。`（宣告舉例的 WSAECONNRESET，同 D229 的 P7）。
  - 5 成立。照實記錄，交使用者判斷。
  - 照實附記（不是判定）：失敗在 token 檢查的 8 次都是「a token wait outlived four commands」，`settled_excess_ms` 是 0.03～0.16 ms；「the waiting sends did not drain the token debt」與「the third new command did not wait for a token」0 次。
- 不跑 lan15。
- **建置**：
  - 發行線 `ff252bd2e66b0a7afe861967a917ef217450809b`，主 checkout。`git merge-base --is-ancestor bff5bf36 HEAD` 成立；`git ls-files --eol` 沒有 `w/crlf`，工作樹乾淨。
  - 在 VS Community 2026 的 vcvars64 環境裡：`cmake --preset ci-windows` exit 0、`cmake --build build/target/_build/ci-windows` exit 0（增量 31 步，含 `worker_main.cpp`；2026-10-29 00:04～00:05）。MSVC 19.51.36260。vcvars64 照舊印了 `'vswhere.exe' is not recognized …`（cl 有找到）。
  - `build/target/_build/ci-windows/acceptance/object_fps_pvp/gyo_object_fps_pvp_worker_window_probe.exe` SHA-256 `eecc847f77d51c2baa68ea6037a072fb72da366ee3e266fe541298fcb2b39101`；旁邊的 `SDL3.dll` `38a1f37a793669689f90561bfec239cf9dc40342c13e5e25eea90b6ac80eaec3`（和 D229 相同）。
- **機器**：Windows 11 Pro 10.0.26300，Intel Core i7-8750H（6 核 12 執行緒，筆電），16 GB；接著 AC（`PowerOnline=True`），電源計畫「平衡」（`381b4222-…`）。和 D229 是同一台。
- **跑法**：
  - 在登入的桌面工作階段裡，從 Claude Code 的 shell 直接執行視窗探針（不經 ctest），沒有參數，工作目錄是執行檔所在的資料夾。串行 20 次（00:05:05～00:13:10）。沒有重跑，中途沒有改任何東西，期間沒有其他吃重的工作。視窗出現後沒有碰它。
  - 每次外面包了 `timeout 300`（防止卡住）。20 次都沒有碰到（每次 19～26 秒）。
  - stdout 與 stderr 合併存成一個檔。
- 20 次的「worker resend rhythm」：`client_sends` 與 `mock_resends` 都是 57（W11 沒有這一行），全文在各自的 log。
- 通過的 11 次（W2、W4、W9、W10、W13、W14、W16～W20）都在最後印出 `"passed":true` 的 JSON。

| 次序 | 執行 | 結束碼 | window open | `drain_excess_ms` | `settled_excess_ms` | `client_sends` | 失敗訊息 |
|---|---|---|---|---|---|---|---|
| 1 | W1 | 1 | 有 | -0.52 | 0.09 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 2 | W2 | 0 | 有 | -0.76 | -1.07 | 57 | —（通過） |
| 3 | W3 | 1 | 有 | -0.37 | 0.03 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 4 | W4 | 0 | 有 | -0.35 | -0.47 | 57 | —（通過） |
| 5 | W5 | 1 | 有 | -0.23 | 0.06 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 6 | W6 | 1 | 有 | -0.22 | 0.09 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 7 | W7 | 1 | 有 | -0.67 | 0.13 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 8 | W8 | 1 | 有 | -0.35 | 0.11 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 9 | W9 | 0 | 有 | -0.03 | -0.79 | 57 | —（通過） |
| 10 | W10 | 0 | 有 | -0.84 | -0.50 | 57 | —（通過） |
| 11 | W11 | 1 | 有 | 沒有 | 沒有 | 沒有 | `既存の接続はリモート ホストに強制的に切断されました。` |
| 12 | W12 | 1 | 有 | -0.31 | 0.16 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 13 | W13 | 0 | 有 | -0.62 | -0.09 | 57 | —（通過） |
| 14 | W14 | 0 | 有 | -0.16 | -0.76 | 57 | —（通過） |
| 15 | W15 | 1 | 有 | -0.74 | 0.10 | 57 | `a token wait outlived four commands`（括號內同下方的 token 行） |
| 16 | W16 | 0 | 有 | -0.73 | -0.99 | 57 | —（通過） |
| 17 | W17 | 0 | 有 | -0.83 | -0.36 | 57 | —（通過） |
| 18 | W18 | 0 | 有 | -0.35 | -0.79 | 57 | —（通過） |
| 19 | W19 | 0 | 有 | -0.30 | -1.03 | 57 | —（通過） |
| 20 | W20 | 0 | 有 | -0.60 | -0.29 | 57 | —（通過） |

W11 的 log 共 23 行，除了失敗訊息與「window open」都是 `[ObjectFPS/PvP]` 的行；沒有節奏行，也沒有 token 行（失敗在重送檢查之前）。

**20 次的「worker token waits」全文**：

- W1：`worker token waits: waits_ms=16.69/12.15/7.29/3.15/2.02/1.92/0.98/0.68 intervals_ms=16.70/16.88/16.89/16.41/16.59/16.80/17.01 drained_ms=4.50/4.65/3.92 drain_excess_ms=-0.52 settled_limits_ms=2.01/1.83/1.75/1.75 settled_excess_ms=0.09`
- W2：`worker token waits: waits_ms=16.31/12.08/7.59/3.60/1.14/0.63/0.61/0.60 intervals_ms=16.97/16.91/16.61/16.20/16.83/16.61/16.75 drained_ms=3.93/4.24/4.04 drain_excess_ms=-0.76 settled_limits_ms=2.22/1.75/1.81/1.75 settled_excess_ms=-1.07`
- W3：`worker token waits: waits_ms=16.44/10.88/7.30/2.90/0.99/0.97/1.83/1.94 intervals_ms=17.44/16.53/16.64/16.86/16.62/16.12/16.50 drained_ms=4.79/3.71/4.43 drain_excess_ms=-0.37 settled_limits_ms=1.75/1.79/2.29/1.91 settled_excess_ms=0.03`
- W4：`worker token waits: waits_ms=16.56/10.98/7.12/2.42/1.45/1.40/0.70/0.82 intervals_ms=17.51/16.65/16.55/16.35/16.55/17.36/16.02 drained_ms=4.73/3.87/4.82 drain_excess_ms=-0.35 settled_limits_ms=2.07/1.87/1.75/2.40 settled_excess_ms=-0.47`
- W5：`worker token waits: waits_ms=16.18/11.41/7.11/3.76/0.97/1.92/1.93/0.97 intervals_ms=17.14/16.61/16.61/17.21/15.98/16.55/17.48 drained_ms=4.30/4.36/3.40 drain_excess_ms=-0.23 settled_limits_ms=1.75/2.43/1.87/1.75 settled_excess_ms=0.06`
- W6：`worker token waits: waits_ms=16.73/11.71/6.97/2.68/0.96/0.98/0.97/1.94 intervals_ms=16.73/17.20/16.54/16.92/16.59/16.50/16.57 drained_ms=4.95/4.20/4.41 drain_excess_ms=-0.22 settled_limits_ms=1.75/1.82/1.91/1.84 settled_excess_ms=0.09`
- W7：`worker token waits: waits_ms=15.81/11.69/6.51/2.78/1.92/0.97/1.67/0.61 intervals_ms=16.77/17.36/16.45/16.62/16.72/16.11/17.18 drained_ms=4.02/4.49/3.94 drain_excess_ms=-0.67 settled_limits_ms=1.79/1.75/2.31/1.75 settled_excess_ms=0.13`
- W8：`worker token waits: waits_ms=16.54/12.20/8.18/3.88/0.95/1.91/1.89/1.84 intervals_ms=17.50/15.98/16.72/17.23/16.07/16.64/16.56 drained_ms=3.52/4.71/4.25 drain_excess_ms=-0.35 settled_limits_ms=1.75/2.35/1.78/1.86 settled_excess_ms=0.11`
- W9：`worker token waits: waits_ms=15.89/12.52/7.15/3.58/0.57/0.96/0.97/0.96 intervals_ms=16.84/16.97/16.39/16.72/17.14/16.58/16.60 drained_ms=3.20/5.07/3.85 drain_excess_ms=-0.03 settled_limits_ms=1.75/1.75/1.84/1.82 settled_excess_ms=-0.79`
- W10：`worker token waits: waits_ms=16.46/11.68/7.82/3.63/1.31/0.62/1.23/1.26 intervals_ms=17.12/16.48/16.78/16.45/16.86/16.41/16.66 drained_ms=4.33/4.04/4.08 drain_excess_ms=-0.84 settled_limits_ms=1.97/1.75/2.01/1.76 settled_excess_ms=-0.50`
- W11：沒有（失敗在 token 那段之前）
- W12：`worker token waits: waits_ms=16.65/12.05/7.06/2.75/1.99/0.71/1.96/1.85 intervals_ms=16.67/16.80/17.32/16.59/16.80/16.47/16.03 drained_ms=4.61/4.85/3.66 drain_excess_ms=-0.31 settled_limits_ms=1.83/1.75/1.95/2.39 settled_excess_ms=0.16`
- W13：`worker token waits: waits_ms=15.96/11.61/7.30/3.67/1.92/0.97/0.97/0.96 intervals_ms=16.91/16.89/16.51/16.41/17.37/16.63/16.43 drained_ms=4.11/4.09/3.79 drain_excess_ms=-0.62 settled_limits_ms=2.01/1.75/1.79/1.99 settled_excess_ms=-0.09`
- W14：`worker token waits: waits_ms=16.52/11.51/6.96/3.65/0.98/0.98/0.99/0.99 intervals_ms=17.46/16.22/16.59/17.15/16.39/16.56/16.95 drained_ms=4.22/5.00/3.38 drain_excess_ms=-0.16 settled_limits_ms=1.75/2.02/1.85/1.75 settled_excess_ms=-0.76`
- W15：`worker token waits: waits_ms=17.07/12.73/8.47/3.77/0.97/1.92/1.91/0.96 intervals_ms=17.08/16.50/17.17/16.82/16.46/16.60/16.75 drained_ms=3.93/4.43/4.20 drain_excess_ms=-0.74 settled_limits_ms=1.75/1.96/1.81/1.75 settled_excess_ms=0.10`
- W16：`worker token waits: waits_ms=16.35/11.94/7.68/4.01/0.66/0.62/0.62/0.76 intervals_ms=16.97/16.67/16.45/17.10/16.42/16.47/16.97 drained_ms=4.11/4.26/3.89 drain_excess_ms=-0.73 settled_limits_ms=1.75/2.00/1.94/1.75 settled_excess_ms=-0.99`
- W17：`worker token waits: waits_ms=16.26/11.72/7.62/2.84/1.67/0.50/1.81/0.94 intervals_ms=17.22/16.46/17.11/16.33/16.72/16.25/16.80 drained_ms=3.99/4.30/4.34 drain_excess_ms=-0.83 settled_limits_ms=2.08/1.75/2.17/1.75 settled_excess_ms=-0.36`
- W18：`worker token waits: waits_ms=16.92/12.61/7.26/4.21/0.96/0.97/0.96/0.98 intervals_ms=16.93/17.20/16.07/16.99/16.42/16.97/16.39 drained_ms=4.05/4.82/3.64 drain_excess_ms=-0.35 settled_limits_ms=1.75/2.00/1.75/2.03 settled_excess_ms=-0.79`
- W19：`worker token waits: waits_ms=16.25/11.15/8.03/3.24/0.65/0.74/0.66/0.72 intervals_ms=17.22/16.27/16.60/16.97/16.63/16.76/17.11 drained_ms=4.54/3.52/4.86 drain_excess_ms=-0.30 settled_limits_ms=1.75/1.79/1.75/1.75 settled_excess_ms=-1.03`
- W20：`worker token waits: waits_ms=16.54/11.38/7.56/3.27/0.98/1.94/0.96/0.59 intervals_ms=17.26/16.39/16.89/16.93/16.19/16.76/17.15 drained_ms=4.57/4.10/4.07 drain_excess_ms=-0.60 settled_limits_ms=1.75/2.23/1.75/1.75 settled_excess_ms=-0.29`

**「v7-08 wake timeline」**（只有通過的 11 次跑到 v7-08）：

| 次 | 照抄 |
|---|---|
| W2 | `v7-08 wake timeline: action_ms=0.75 events=in:Actions@0.74` |
| W4 | `v7-08 wake timeline: action_ms=0.98 events=in:Actions@0.97` |
| W9 | `v7-08 wake timeline: action_ms=0.97 events=in:Actions@0.97` |
| W10 | `v7-08 wake timeline: action_ms=0.67 events=in:Actions@0.66` |
| W13 | `v7-08 wake timeline: action_ms=0.98 events=in:Actions@0.97` |
| W14 | `v7-08 wake timeline: action_ms=0.99 events=in:Actions@0.98` |
| W16 | `v7-08 wake timeline: action_ms=0.99 events=in:Actions@0.98` |
| W17 | `v7-08 wake timeline: action_ms=0.58 events=in:Actions@0.58` |
| W18 | `v7-08 wake timeline: action_ms=0.98 events=in:Actions@0.98` |
| W19 | `v7-08 wake timeline: action_ms=0.76 events=in:Actions@0.76` |
| W20 | `v7-08 wake timeline: action_ms=0.99 events=in:Actions@0.98` |

- **判讀交給 v7.1 會話**（Windows 端不做）。宣告、門檻、測試、產品程式都沒有改。

**證據**（留在 Windows 機器上，不在 repo）：主 checkout 的 `build/target/_build/test/logs/worker-D230-20261029/`，內含 `build.log`、`build-exit.txt`（建置的結束碼）、`commit.txt`、`build-commit.txt`、`msvc-version.txt`、`machine.txt`、`hashes.txt`、`W1.log`～`W20.log`、`runs.txt`（`exit=` 是探針的結束碼）。

## token 等待的觀測：判讀與停止（2026-10-29，v7.1 會話）

收支：一致（Windows 的紀錄：20 次都有結束碼與 window open；跑到 token 那段的 19 次都有值）。判定照宣告是 5「其他」，我核對過表格，判定成立：W11 失敗在 WSAECONNRESET，不在 token 的兩項檢查。

依 AGENTS.md §14 開頭停下：同一個問題（worker 在 Windows 上的時間判定）從 D216 起，已經換了 4 次測量方式（第 3 步、B、C、D229／D230），每次都停在新的地方，判定沒有一次落在預期的類別。不再加新的測量，交給使用者決定。

- **token 的失敗（只是觀察，不是判定）**：
  - 19 次中 8 次失敗，全部是「a token wait outlived four commands」，`settled_excess_ms` 只有 0.03～0.16 ms。另外兩項 token 檢查 0 次失敗。
  - 排空之後（第 4～7 步）的等待集中在約 0.6、0.97、1.92 ms 這幾個值，看起來是以約 1 ms 為單位的刻度。macOS 上同樣的等待約 0.13 ms。
  - 這裡的等待是「mock 收到的時刻減去發佈的時刻」。mock 每次輪詢之間用 Waiter 等 100 µs，而 Windows 上 Waiter 的最小等待約 0.6 ms（W4：沒有 SDL 時 P50 0.617 ms）。所以 mock 自己的輪詢就可能讓收到的時刻晚 1～2 個刻度。失敗都是某一步落在約 1.92 ms（兩個刻度），超過 1.75 ms 的上限。
  - 讀法（沒有另外驗證）：超過量遠小於 1 ms，和「量測的刻度（mock 的輪詢與 Windows 的計時器）比門檻粗」相容，不像是產品的 token 規則在 Windows 上算錯。排空那項（每步排空四分之一步、容許 ±1 ms）在 19 次都通過。
- **WSAECONNRESET**：Windows 上 40 次中出現 2 次（D229 的 P7、這次的 W11），都在重送檢查之前，讓整個驗收中止。推論在 mock 端（D229 的判讀），還沒有找出是哪一步觸發的。
- **其他已經回答的**：開著 SDL 視窗時，重送每次都是 57（D229 的 10 次、這次的 19 次）；v7-08 在 Windows 上跑到的 19 次都通過，action 在 0.58～0.99 ms 內送出。
- **範圍的限界**（之後的宣告照抄，直到解消）：
  - Windows 上 worker 驗收裡的毫秒級判定，量的是 mock 輪詢到的時刻，刻度約 1 ms；「a token wait outlived four commands」的 1.75 ms 上限在 Windows（開著 SDL 視窗）上約四成失敗，超過量 0.03～0.16 ms。
  - mock 的 `Pump` 遇到 WSAECONNRESET 會讓整個驗收中止，Windows 上約 5%（40 次中 2 次），觸發的步驟還沒找出來。
  - 沒有 SDL 的 Windows 程序裡，Client 的重送約 43～53 Hz（D228、D229）。

## 決定（D231，2026-10-29）

使用者：「選 1：記為已知限界，Windows 的 worker 驗收不算 v7.1 的發行條件；限界寫進發行說明與 lan15 的宣告，mock 的兩個問題另開後續任務，接著規劃 lan15。」

- Windows 上的 worker 驗收不算 v7.1 的發行條件，上面各節的「範圍的限界」照記錄保留，寫進發行說明與 lan15 的宣告。D229 的「通過才跑 lan15」由此取代。
- mock 的兩個問題（WSAECONNRESET、token 等待的量測刻度）排在 v7.1 之後，見 HANDOFF 的未結事項。
- 本文件的調查到此結束。

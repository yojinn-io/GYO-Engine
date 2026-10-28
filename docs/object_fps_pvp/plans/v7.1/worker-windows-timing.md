# worker-windows-timing：worker 驗收在 Windows MSVC 上的時間判定失敗（D216，草案，等使用者核准）

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

## 步驟 3 的事前宣告（草案）

- **建置**：Windows 機器、MSVC（`cmake --preset ci-windows`），發行線 `claude/pvp-v71-release` 含觀測的 commit。
- **跑法**：`ctest --test-dir build/target/_build/ci-windows -C Release -R "^object_fps_pvp\.worker$" --output-on-failure`，串行跑 10 次，不重跑、不中途改。每次記錄：通過或失敗、`restart_ms`、`completed_window_gap_ms`（通過時取 stdout 的 JSON，失敗時取訊息），以及失敗時的其他訊息。
- **收支**：10 次中每一次都要有兩個值。沒有值的（其他斷言先失敗、逾時）照實記錄，計入「確認不了」。
- **分類（只記錄，不是驗收）**：
  - 「tick 捨入」：失敗的每一次，至少一個值超過門檻，而且落在 15.625 ms 的整數倍 ±2 ms 內。
  - 「不是 tick」：有失敗的值超過門檻，但不在任何整數倍 ±2 ms 內。
  - 「判定不了」：10 次全部通過，或者失敗的次數不足 2 次。
- **範圍的限界**：只有一台 Windows 機器；只看 worker 這一項，不代表其他網路測試；沒有 SDL 的程序才會受影響（Client 本體有 SDL）。

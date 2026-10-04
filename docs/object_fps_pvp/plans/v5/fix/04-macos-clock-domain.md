# 04 macOS 上 Python 與 C++ 單調時鐘不一致

狀態：**已解決**。2026-10-01發現並修正，2026-10-02以不計次單案確認。Owner：`object_fps_pvp`（驗收器`build/acceptance/object_fps_pvp`）。
相關：為PR #2（`ff11ee3`，A1啟動相位對齊，已合併）準備本機macOS結案驗證時發現；本修正不在PR #2內，
經PR #3（`claude/pvp-v5-start-phase-guard`，自`ff11ee3`分出）合併為`9cd7f26`。
只改驗收器；產品runtime、wire、v5契約、Engine與公共層都不變。索引見 [README](README.md)，
時間順序見 [守門dev_log](../../../../dev_logs/2026_10_02_pvp_v5_start_phase_guard.zh-Hant.md)。

## 問題成因

Python runner在故障注入、relay事件與等待迴圈寫下時間戳記，分析器直接拿它們和C++ probe／Match
trace的時間戳記比較。兩端在macOS讀的不是同一個時鐘：

| 端 | 讀法 | macOS實際來源 | 睡眠時 |
|---|---|---|---|
| C++ probe／Match trace | `std::chrono::steady_clock`：`MovementTraceNowNs()`（`apps/object_fps_pvp/include/RetroFPS/Pvp/MovementTrace.hpp:43-46`）；`ready.json`的`start_ns`（`action_main.cpp:124`、`gameplay_action.hpp:80`） | Apple libc++：`CLOCK_MONOTONIC_RAW` | 繼續計時 |
| Python runner（修正前） | `time.monotonic_ns()`／`time.monotonic()` | `mach_absolute_time()`（與`CLOCK_UPTIME_RAW`同值） | 停止 |

- 差值＝開機後累計睡眠時間，只增不減。實機（Intel MacBook Pro 2019、x86_64、Python 3.14）發現時量得
  **8176.5秒**（C++ 11593.9秒／Python 3417.3秒）；2026-10-02撰寫本文時已增為8864.8秒。
- Linux：libstdc++ `steady_clock`與Python `monotonic`都讀`CLOCK_MONOTONIC`，同一域。先前在Linux取得的
  第03批證據不受影響。剛開機且未睡眠的Mac差值≈0，問題會被掩蓋。
- 修正前用Python時鐘、卻與C++時間比較的位置：
  - `action_probe.py`：`ActionRelay.arm`／`_receive`／`_release`的故障起訖、`_send`的事件時間；
    `run_case`等待迴圈`while time.monotonic_ns() < ready['start_ns']+fault_at`及gateway `SIGSTOP`／`SIGCONT`起訖。
  - `backpressure_probe.py`：`IpcPause._accept`（host-ipc案例）、`DownstreamPause._receive`、`run_case`的起訖。
  - `run_player_short.py`：`SnapshotHold._receive`以`time.monotonic()`判斷是否落在由C++
    `host_steady_seconds`算出的300ms丟快照窗口。
- 比較發生在：`gameplay_evidence.py` `fault_expiry_exception`（:63-64，C++送出時間是否落在故障窗口）、
  恢復判定（:304起，`release_ns`對C++決策／frame時間）；`backpressure_probe.analyze`（:210）。

## 影響

- 只影響macOS上的驗收runner。產品只在單一行程內使用`steady_clock`，不受影響。
- 25案矩陣（`run_gameplay.py`）中16案受影響（依程式碼分析，修正前未在macOS實跑）：

| 類別 | 案數 | 修正前在macOS的行為 |
|---|---|---|
| network0／20／40 | 3 | relay以Python時鐘標故障窗口；釋放時間看起來比所有C++事件早8176秒，恢復判定失敗（預期訊息`Alive actor fresh decision failed1.5s recovery`） |
| upstream／downstream／socket-path／gateway／host-ipc × 250／1000ms；downstream-death／respawn-1000ms | 12 | 先卡在等待迴圈：Python時鐘要追上C++ `ready.start_ns`＋`fault_at`，約空等一個偏移量（約2.3小時）；16秒probe早已結束，故障不會落在執行期間 |
| cross-life | 1 | 故障起訖由relay以Python時鐘標記，與C++決策時間不可比 |

- 不受影響9案：clean-60／30／144、burst2、loss-shot／result／ack、duplicate-reorder-conflict、
  drain-stall（其故障窗口由C++ `ready.start_ns`推算）。
- runner遇第一個失敗就停（`run_gameplay.py`第51行），修正前會停在第4案network0。
- 其他runner：`run_player_short.py`的丟快照窗口永遠不命中，`dropped_observer_snapshots`為空而判失敗；
  獨立的`backpressure_probe.py`恢復分析同樣錯位。
- 錯位量是數千秒，不是「略差」：看起來像產品恢復失敗，重跑也不會改善。
- 不影響：產品runtime、wire、契約、CPU模擬、Linux證據，以及只在Python內部計算的間隔
  （截止時間、hold長度、送出率統計）。

## 如何復現

1. 量偏移（實機、只需Python、即時完成）。條件：macOS，且開機後曾睡眠；此命令只適用macOS。

   ```sh
   python3 -c "import time; print(time.get_clock_info('monotonic').implementation); print((time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)-time.monotonic_ns())/1e9)"
   ```

   印出`mach_absolute_time()`及以秒計的偏移（本機2026-10-02為8864.8）。數值會隨每次睡眠增加。

2. （選擇性）確認C++端讀`CLOCK_MONOTONIC_RAW`。存成`steady.cpp`，以`clang++ -std=c++17 steady.cpp -o steady && ./steady`執行：

   ```cpp
   #include <chrono>
   #include <cstdio>
   #include <time.h>
   int main() {
       const long long steady = std::chrono::duration_cast<std::chrono::nanoseconds>(
           std::chrono::steady_clock::now().time_since_epoch()).count();
       std::printf("steady_clock        %lld\n", steady);
       std::printf("CLOCK_MONOTONIC_RAW %llu\n", (unsigned long long)clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW));
       std::printf("CLOCK_UPTIME_RAW    %llu\n", (unsigned long long)clock_gettime_nsec_np(CLOCK_UPTIME_RAW));
   }
   ```

   前兩行只差呼叫間隔，第三行少一個偏移量。2026-10-02本機：55807.163／55807.163／46942.362秒。
   另以暫存工具（不在repo）跨行程取樣2000次：Python `CLOCK_MONOTONIC_RAW`前後包夾C++時間戳記
   2000／2000、中位差−0.5µs；`time.monotonic_ns`為0／2000。

3. 驗收器層級（任何平台、不需建置、無GUI、約1秒）：在repo根目錄把修正前的驗收器配上目前的守門測試。

   ```sh
   tmp=$(mktemp -d)
   git archive ff11ee3 build/acceptance/object_fps_pvp | tar -x -C "$tmp"
   cp build/acceptance/object_fps_pvp/run_network.py build/acceptance/object_fps_pvp/test_steady_clock.py \
      "$tmp/build/acceptance/object_fps_pvp/"
   python3 "$tmp/build/acceptance/object_fps_pvp/test_steady_clock.py"
   ```

   預期`FAILED (failures=1, errors=6)`，只剩三項平台選擇測試通過（errors含3個subtest）；目前工作樹同一測試為8／8 OK。

4. 不建議用完整runner復現：修正前的runner另有Match／Gateway啟動競態
   （[05](./05-match-gateway-startup-race.md)），可能先因啟動失敗而看不到本問題；fault案例還要空等約一個偏移量。

## 解決方案

- `run_network.py`新增`steady_clock_source(system=sys.platform)`（:68）與模組層`steady_clock_ns`（:82）：
  - `darwin` → `time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)`，與Apple libc++ `steady_clock`同域；
  - 其他平台 → 原本的`time.monotonic_ns`（Linux兩端皆`CLOCK_MONOTONIC`，行為不變）。
- 只在「Python時間與C++ trace時間相遇」處改用`steady_clock_ns`。純Python間隔（`block_until`、`network_until`、
  `cross_until`、啟動截止時間、GUI runner的事件標籤）維持`time.monotonic()`，規則寫在helper的docstring。
- 呼叫點：

| 檔案 | 函式（行） | 用途 |
|---|---|---|
| `action_probe.py` | `ActionRelay.arm`（:142）、`_send`（:150）、`_release`（:158／166／168）、`_receive`（:177） | relay故障起訖與事件時間 |
| `action_probe.py` | `run_case`（:568等待迴圈；:571／577 gateway `SIGSTOP`／`SIGCONT`） | 等到C++ `ready.start_ns`＋`fault_at`再注入 |
| `backpressure_probe.py` | `IpcPause._accept`（:95／101）、`DownstreamPause._receive`（:189／195）、`run_case`（:366／372） | host-ipc與下游阻塞起訖 |
| `run_player_short.py` | `SnapshotHold._receive`（:36） | `steady_clock_ns()/1e9`對C++ `host_steady_seconds` |
| `run_timing.py` | `run-manifest.json`（:338） | 新增`steady_clock_start_ns`，與原`monotonic_start_ns`並列，只作紀錄 |

- 各模組都從`run_network`匯入同一個`steady_clock_ns`，不各自判斷平台。這些模組原本就匯入
  `run_network.free_port`，沒有新的相依邊；全部位於product-owned驗收目錄，沒有Architecture Delta。
- 其他做法（未採用）：

| 做法 | 不採用原因 |
|---|---|
| 重開機並以`caffeinate`保持清醒，使偏移≈0 | 依賴機器狀態，任何一次睡眠就復發；程式仍錯 |
| 外部wrapper在執行前把`time.monotonic_ns`換成RAW時鐘 | 不在repo、證據看不出來；`run_player_short.py`用的是`time.monotonic()`，蓋不到 |
| 讓C++ probe／產品trace改讀`CLOCK_UPTIME_RAW` | 為驗收工具改產品時鐘，方向相反；產品trace不應配合驗收器 |
| 把所有Python時鐘都換成RAW | 不需要；純Python間隔與C++無關，只會擴大變更面 |

## 驗證

- `build/acceptance/object_fps_pvp/test_steady_clock.py`，8項：
  - darwin讀`CLOCK_MONOTONIC_RAW`（`monotonic_ns`被呼叫即失敗）；linux／win32維持`time.monotonic_ns`；
    模組層時鐘就是本平台的選擇且單調；
  - AST守門：`SITES`表列的每個跨域位置都讀`steady_clock_ns`，三個模組不得出現`time.monotonic_ns`，
    `SnapshotHold._receive`不得讀`time.monotonic()`；逐一把任一位置改回`time.monotonic_ns()`都會被抓到；
  - 三個模組匯入的是同一個`run_network.steady_clock_ns`；`SnapshotHold`丟快照窗口與`ActionRelay`事件時間的行為測試。
- 執行：`python3 build/acceptance/object_fps_pvp/test_steady_clock.py`；
  CTest `object_fps_pvp.steady_clock`（`tests/object_fps_pvp/CMakeLists.txt:97-99`，labels `cpu;pvp;acceptance`，TIMEOUT 10）：
  `ctest --test-dir build/target/_build/test -R '^object_fps_pvp\.steady_clock$' --output-on-failure`。
- 本機整合（2026-10-02）：`ctest -L pvp` 16／16（steady_clock 0.54秒）；Python驗收測試12檔165項全過。
- 不計次單案`upstream-250ms`（不計次、不作為驗收證據）：實機、PASS、約52秒，同時確認本修正與
  [05](./05-match-gateway-startup-race.md)的啟動helper。
  - 等待迴圈正確結束：Python故障開始距C++ `ready.start_ns` 1.505秒（`fault_at` 1.5秒），沒有空等。
  - 跨域順序一致：觸發故障的玩家1動作在C++端提交於Python故障開始前1.60ms；玩家2移動命令C++送出於+0.615ms、
    relay收到於+0.654ms；玩家1下一筆C++送出+9.586ms、relay收到+9.625ms。故障長250.46ms，3個被扣住的datagram
    在釋放後0.05ms內送出。
  - 證據（git忽略、只在本機）：`build/target/_build/test/logs/pvp-v5-batch03-integration3-20261002-015053/`
    `matrix-smoke-upstream-250ms-NOT-COUNTED.json`／`.log`；原始案例目錄
    `build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/matrix-smoke-upstream-250ms/`。
  - 重跑方式（需先`cmake --build --preset test`，Gateway另需`go`在PATH上：
    `cmake --build build/target/_build/test --target gyo_object_fps_pvp-gateway`；閒置機器、不要同時建置）：

    ```sh
    python3 build/acceptance/object_fps_pvp/run_gameplay.py \
      --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
      --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
      --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_action_probe \
      --arena assets/object_fps_pvp/pvp_arena.json \
      --output build/target/_build/test/logs/<新目錄>/matrix-upstream-250ms --case upstream-250ms
    ```

    `--output`不可已存在。

## 殘留風險與後續

- macOS上16個受影響案例只跑過`upstream-250ms`一案且不計次；25案矩陣依使用者決定（MVP技術驗證）暫緩，
  未在macOS完整執行，第03批未結案。日後恢復結案時，矩陣會一併覆蓋本修正。
  後續：2026-10-02第03批結案驗收的25案矩陣在macOS 25／25通過，已涵蓋本修正。
- AST守門只涵蓋`SITES`表列的三個模組。其他模組（如`run_gameplay_gui.py`、`run_native_window.py`、
  `recovery_probe.py`、`impaired_network.py`、`run_timing.py`）仍用Python時鐘做純Python用途（截止時間、延遲排程、
  事件標籤）；若日後要與C++時間比較，須改用`steady_clock_ns`並加入`SITES`。
- `run-manifest.json`同時有`monotonic_start_ns`與`steady_clock_start_ns`，在macOS屬不同域，不可混算。
- Windows維持`time.monotonic_ns`，未在Windows驗證它與MSVC `steady_clock`同域。
- 經驗：跨行程比較時間戳記前，先確認兩端的時鐘來源；macOS的Python monotonic ≠ C++ `steady_clock`。

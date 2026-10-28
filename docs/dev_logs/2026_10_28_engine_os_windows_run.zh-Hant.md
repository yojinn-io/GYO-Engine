# OS 子系統：Windows 自動項目 第 1 次（os-windows-run，D73）

日期：2026-10-28。分支 `claude/os-windows-run`。計畫見 [windows-run.md](../architecture/plans/os-subsystem/windows-run.md)。

**狀態：停在 W3（不通過）；使用者決定第 1 次停在這裡（2026-10-28）。** W0～W2 完成，W1 有失敗（使用者決定 W5 也照跑），W3 不通過（`window deadline p50`，不重跑），W4～W8 未跑。

收支：確認できない（W3 是比較兩側分布的計測，不是送收的計數；腳本照計畫各側跑完 4 對 × 2 情境，16 個結果檔與 16 個 sleeper 檔都在，沒有缺漏）。受け入れ判定（D71）的結果是不通過，照規則不重跑。

- 規則：只跑、只記錄；不改程式碼、測試、突變目錄、腳本與 CMake。判定與門檻照事前宣告（D68、D69、D71），不放寬。
- 證據目錄（不進 repo）：B 的 checkout 裡的 `build/target/_build/test/logs/os-windows-run-20261028/`。
- 路徑中的 Windows 使用者名稱寫成 `<user>`。

## 收支（中途）

計畫 9 項（W0～W8）：跑完 4 項（W0、W1、W2、W3），其中 W3 不通過；未跑 5 項（W4～W8：W3 不通過，照規則停下）。

## 環境

- Windows 11 Pro 10.0.26300（build 26300）
- CPU：Intel Core i7-8750H @ 2.20GHz，6 核 12 執行緒；記憶體 15.9 GB
- ACP＝65001、OEMCP＝65001（`HKLM\SYSTEM\CurrentControlSet\Control\Nls\CodePage`）
- 使用者字型資料夾 `%LOCALAPPDATA%\Microsoft\Windows\Fonts`：存在，直下沒有檔案
- 開發人員模式：關閉（`AllowDevelopmentWithoutDevLicense` 沒有值）
- 預設終端機：未設定（`HKCU\Console\%%Startup` 沒有 `DelegationConsole`）
- 電源計畫：平衡（`381b4222-…`）
- MSVC 19.51.36260（VS Community 2026，`vcvars64.bat`），CMake 4.0.1，Ninja，MinGW-Builds GCC 15.2.0（x86_64-posix-seh）
- 磁碟：D: 剩 312 GB、C:（`%TEMP%`）剩 34 GB
- `\\localhost\C$`：可連
- 行尾：系統 gitconfig 是 `core.autocrlf=true`，repo 的 local 已是 `false`（D210）。四份 checkout 的 `git ls-files --eol` 都沒有 `w/crlf`，工作樹乾淨

### 和文件不同的地方：Python

這台唯一的 Python 是 embedded 版（`D:\code\Runtime\python-3.13.3-embed-amd64`，3.13.3；`py --list` 沒有其他版本）。它的 `python313._pth` 讓腳本所在目錄不在 `sys.path`（`sys.flags.safe_path` 是 True），PYTHONPATH 也被忽略。所以 `wake_compare.py` 的 `import wake_baseline` 失敗（`ModuleNotFoundError`）。

處理（使用者同意）：repo 的腳本改用會話 scratchpad 的啟動器執行。啟動器先把腳本所在目錄加進 `sys.path`，再以 `runpy.run_path(..., run_name="__main__")` 執行原腳本，參數照文件。腳本本身不改；量測程式由腳本直接以 `subprocess` 啟動，不經過啟動器。啟動器全文（重現用）：

```python
import runpy, sys
from pathlib import Path
script = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(script.parent))
sys.argv = [str(script)] + sys.argv[2:]
runpy.run_path(str(script), run_name="__main__")
```

## W0：準備

| checkout | commit | 路徑 |
|---|---|---|
| B | `1b97f5d61a9883a1fa063110b4e290dfb1410c96`（和 `c54ef8b5` 只差文件與 `udp_error_probe.py`） | 主 checkout |
| A_wait | `4ffc7c65cb9b675ba91ac281d72d2e3b84ed6f0b` | `..\gyo-a-wait`（worktree） |
| A_threads | `5733d3619549abaea886b5d5c0c41fb689d8bb49` | `..\gyo-a-threads`（worktree） |
| B_mingw | 同 B | `..\gyo-b-mingw`（worktree） |

MSVC 建置（vcvars64 環境裡，`cmake --preset ci-windows`、`cmake --build build/target/_build/ci-windows`）：

| checkout | configure | build | 時間 |
|---|---|---|---|
| B | 0 | 0 | 10:51～10:56（增量，118 步） |
| A_wait | 0 | 0 | 10:56～11:14（1359 步） |
| A_threads | 0 | 0 | 11:14～11:32（1357 步） |

警告只看到 doctest 的 C5285（`std::tuple` 的特殊化）與 doctest 的 CMake Deprecation Warning。log：證據目錄的 `build/`。

## W1：B 的完整 ctest

`ctest --test-dir build/target/_build/ci-windows --output-on-failure`（11:32～11:38，從 Git Bash 執行，不在 vcvars64 環境裡）。

**exit 8：102 項中 70 通過、32 失敗、2 Skipped。** 照規則沒有重跑。

| 類別 | 項數 | 訊息 | 原因的判斷 |
|---|---|---|---|
| object_fps_pvp 的 Python 驗收（#64～#72、#74～#92） | 28 | `ModuleNotFoundError: No module named '<同目錄的模組>'` | 環境：上面的 embedded Python |
| `cmake.ProcessCodePage`（#46）、`cmake.ProjectComposition`（#43） | 2 | 子專案的編譯器檢查失敗：`RC Pass 1: command "rc /fo ..." failed ... no such file or directory` | 環境：ctest 在 vcvars64 外面執行，`rc` 不在 PATH。W1 的指令沒有指定 vcvars |
| `os.console_ctrl_c`（#26） | 1 | `console check: FAIL: interrupt once: expected exit 0 with 'requested reason=interrupt', got 4: ['ready', 'timeout']` | 推測是會話的執行環境沒有可送 Ctrl+C 的主控台（和 D201 的 console_close 同類），未確認 |
| **`object_fps_pvp.worker`**（#94） | 1 | `worker acceptance: new window waited behind a fully acknowledged window's deadline` | **未切開**：斷言失敗，可能是實作，也可能是時序（主機負載）。沒有重跑。W5 會用到這一項 |
| Skipped：`os.console_close`（#27） | — | — | 預期（開放決策 26） |
| Skipped：`object_fps_pvp.ingress_cross`（#73） | — | — | 原因未看 |

- W5 用到的 `object_fps_pvp.cpu`、`ipc`、`close_timing` 通過；`worker` 失敗。使用者決定 W5 照跑，worker 的失敗只記錄。
- 文件列的「預期」（symlink 提早返回、os-utf8-codepage 補的 UNC 建目錄與使用者字型目錄兩個案例）：沒有用 `-V` 跑，從這份輸出無法確認，記為未確認。
- 輸出：證據目錄的 `w1-ctest.log`。

## W2：複製量測程式

從 A_wait 與 B 的 `build/target/_build/ci-windows/tests/common/time/` 複製到證據目錄的 `programs/A`、`programs/B`（各 `gyo_time_wake_record.exe`、`gyo_time_wake_record_window.exe`、`SDL3.dll`）。

| 檔案 | SHA-256 |
|---|---|
| A `gyo_time_wake_record.exe` | `143c22794b2e3b64d9b10d6888a6ea38fe5417aa78e85ead24445124e3d1f290` |
| A `gyo_time_wake_record_window.exe` | `13183c518d61531dc4eb7f92defcdc8de3ca85b8821da42130f8d0f588222065` |
| B `gyo_time_wake_record.exe` | `ba332fcfcd4f4bef22d687da9f08e1f968aa1b88072481c03f5fb6350ebf9128` |
| B `gyo_time_wake_record_window.exe` | `2cffc3a69a82ca4d777d8766e79e896d4050a0e72b829ddd2355c02afcd258ac` |

## W3：os-wait-backend 的 L2

```
python <scratchpad>/run_script.py tests/common/time/wake_compare.py --a <證據目錄>/programs/A --b <證據目錄>/programs/B --output <證據目錄>/wait-AB --pairs 4 --modes cli window --a-label "A_wait 4ffc7c65" --b-label "B 1b97f5d"
```

12:39～13:21（約 42 分）。**exit 1：不通過。** 12 項中 11 項通過，`window deadline p50` 不通過。照規則（D71）**不重跑**，停下回報使用者。撤回（revert）與要不要另跑 A/A 診斷，由 OS 子系統的會話與使用者決定。

量測期間：12:41 在這台 commit 並推送了中途報告（`9c0318e`，只有文件，git 操作數秒；這時在 pair1 的 A-cli 中）；沒有建置，也沒有其他吃重的工作。`meta.txt` 的腳本來源 commit 是 `1b97f5d`（開始時）。Windows 上沒有 load average（`meta.txt`：`unavailable on this platform`），主機忙碌的程度只能看 sleeper。

腳本印出的 12 項（中位數＝各側 4 次的中位數，ms）：

| 檢查 | A | B | 上限 | 結果 |
|---|---|---|---|---|
| cli deadline p99 | 1.0025 | 0.9939 | 1.3031 | pass |
| cli deadline p50 | 0.6277 | 0.6216 | 0.6477 | pass |
| cli notify p99 | 0.0766 | 0.0764 | 0.1457 | pass |
| cli notify p50 | 0.0192 | 0.0204 | 0.0392 | pass |
| cli deadline never early | — | min 0.0291 | ≥0 | pass |
| cli same backend | HighResolutionWaitableTimer | 同左 | — | pass |
| window deadline p99 | 0.9085 | 0.9261 | 1.1856 | pass |
| **window deadline p50** | **0.4658** | **0.4890** | **0.4858** | **FAIL**（超出 0.0032） |
| window notify p99 | 0.0595 | 0.0571 | 0.1243 | pass |
| window notify p50 | 0.0173 | 0.0175 | 0.0373 | pass |
| window deadline never early | — | min 0.0214 | ≥0 | pass |
| window same backend | HighResolutionWaitableTimer | 同左 | — | pass |

每次的值（Waiter；deadline 的 P50／P99／min／max，notify 的 P50／P99，sleeper 的 P99；ms）：

| 情境 | 側 | 對 | deadline P50 | P99 | min | max | notify P50 | P99 | sleeper P99 |
|---|---|---|---|---|---|---|---|---|---|
| cli | A | 1 | 0.6725 | 1.1004 | 0.0294 | 1.687 | 0.0191 | 0.0839 | 1.051 |
| cli | A | 2 | 0.6983 | 1.0430 | 0.0515 | 1.097 | 0.0192 | 0.0763 | 1.088 |
| cli | A | 3 | 0.5830 | 0.9413 | 0.0273 | 1.441 | 0.0187 | 0.0769 | 1.000 |
| cli | A | 4 | 0.5662 | 0.9620 | 0.0424 | 1.124 | 0.0220 | 0.0756 | 0.993 |
| cli | B | 1 | 0.6638 | 0.9862 | 0.0513 | 1.056 | 0.0190 | 0.0750 | 0.958 |
| cli | B | 2 | 0.6383 | 0.9911 | 0.0419 | 6.644 | 0.0184 | 0.0778 | 1.052 |
| cli | B | 3 | 0.5890 | 0.9966 | 0.0291 | 1.026 | 0.0222 | 0.0732 | 1.024 |
| cli | B | 4 | 0.6048 | 1.0310 | 0.0515 | 1.545 | 0.0218 | 0.0788 | 1.073 |
| window | A | 1 | 0.4628 | 0.9013 | 0.0318 | 1.211 | 0.0203 | 0.0684 | 1.048 |
| window | A | 2 | 0.4555 | 0.9223 | 0.0173 | 1.102 | 0.0172 | 0.0523 | 0.982 |
| window | A | 3 | 0.4688 | 0.9157 | 0.0280 | 1.180 | 0.0168 | 0.0644 | 1.056 |
| window | A | 4 | 0.4857 | 0.8999 | 0.0340 | 1.000 | 0.0174 | 0.0545 | 0.968 |
| window | B | 1 | 0.4873 | 0.9397 | 0.0596 | 1.127 | 0.0176 | 0.0528 | 0.966 |
| window | B | 2 | 0.4908 | 0.9267 | 0.0214 | 1.048 | 0.0171 | 0.0707 | 1.042 |
| window | B | 3 | 0.4962 | 0.9073 | 0.0236 | 0.995 | 0.0175 | 0.0602 | 1.002 |
| window | B | 4 | 0.4703 | 0.9255 | 0.0267 | 1.121 | 0.0174 | 0.0541 | 0.988 |
觀察（只記錄，不是判定，也不據此放寬）：
- 兩側每次都回報 `HighResolutionWaitableTimer`。
- window 的 deadline P50：A 的 4 次在 0.4555～0.4857（極差 0.030），B 的 4 次在 0.4703～0.4962（極差 0.026）。兩側的範圍重疊；同一側 4 次的極差大於邊界 +0.02 ms。cli 的 deadline P50，A 的極差是 0.132。
- sleeper P99 在 0.958～1.088，兩側沒有明顯差別。
- 這次沒有跑 A/A 校準；D71 的校準是在 macOS Intel 上做的，Windows 上沒有 A/A 的資料。

證據：`wait-AB/verdict.json`、`wait-AB/meta.txt`、各次的 `pair<n>-<A|B>-<cli|window>[-sleeper].json`、`w3-stdout.log`。

## W4～W8

W3 不通過後先停下；使用者之後決定把 W4～W8 跑完（2026-10-28）。W3 的判定不因此改變。

## W4：os-wake-record 的 Windows 基準（只記錄）

```
python tests/common/time/wake_baseline.py --build-dir D:/code/Source/gyo-a-wait/build/target/_build/ci-windows --output <證據目錄>/wake-baseline
```

13:44～13:50，**exit 0**。程式是 A_wait 的建置（雜湊和 W2 的 A 相同）；`meta.txt` 的 source commit 是腳本所在的 B checkout（`4bc999e`，只差文件）。兩組的 Waiter 後端都是 `HighResolutionWaitableTimer`。

60 Hz 絕對期限的晚醒（ms；各 30 秒）：

| 情境 | 原語 | 樣本 | P50 | P99 | 最大 | >1 ms | >4 ms |
|---|---|---|---|---|---|---|---|
| cli | Waiter | 1800 | 0.617 | 1.000 | 1.068 | 19 | 0 |
| cli | condition_variable | 1797 | 5.280 | 15.939 | 16.965 | 1774 | 1590 |
| cli | sleep_until | 1782 | 15.630 | 16.655 | 16.893 | 1608 | 1504 |
| window | Waiter | 1800 | 0.450 | 0.928 | 1.231 | 7 | 0 |
| window | condition_variable | 1800 | 0.945 | 1.949 | 2.380 | 801 | 0 |
| window | sleep_until | 1800 | 1.077 | 2.074 | 2.738 | 1024 | 0 |

notify→wake（ms）：

| 情境 | 原語 | 樣本 | P50 | P99 | 最大 | 跳過的週期 |
|---|---|---|---|---|---|---|
| cli | Waiter | 1799 | 0.0193 | 0.0925 | 0.3252 | 0 |
| cli | condition_variable | 1740 | 0.0200 | 0.0796 | 0.1997 | 60 |
| window | Waiter | 1800 | 0.0173 | 0.0526 | 0.1261 | 0 |
| window | condition_variable | 1800 | 0.0184 | 0.0527 | 0.0885 | 0 |

sleeper（ms）：cli 前 P50 0.641／P99 1.053／最大 5.73，cli 後 0.481／0.934／0.966；window 前 0.580／0.961／1.516，window 後 0.368／0.904／0.987。

觀察（只記錄）：沒有視窗時，condition_variable 與 `sleep_until` 的晚醒以 15.6 ms 左右（系統預設計時器解析度）為主；有視窗時降到 1～2 ms（推測是 SDL 提高了計時器解析度，未確認）。Waiter 兩組都在 1 ms 左右。沒有視窗那組照 D68 由 OS 子系統的會話交給 v7.1 的 F5／F7。

證據：`wake-baseline/`（`cli.json`、`window.json`、`sleeper-*.json`、`meta.txt`、`*.log`）。

## W5：os-threads-backend 的 L2

A_threads（`5733d361`）與 B 的建置樹，各跑兩次 ctest（串行，`-V`）。13:50～13:54。

| 側 | `worker|ipc|close_timing` | `cpu` |
|---|---|---|
| A | exit 8（`worker` 失敗） | exit 0 |
| B | exit 8（`worker` 失敗） | exit 0 |

**判定：通過。** `A-roles.txt` 與 `B-roles.txt` 逐字相同（`cmp` 一致），四種角色：

```
role started version=1 role=gyo-client-net wait_backend=HighResolutionWaitableTimer name_applied=1 priority_applied=1
role started version=1 role=gyo-match-ipc wait_backend=HighResolutionWaitableTimer name_applied=1 priority_applied=1
role started version=1 role=gyo-match-sim wait_backend=HighResolutionWaitableTimer name_applied=1 priority_applied=1
role started version=1 role=gyo-test-sim wait_backend=HighResolutionWaitableTimer name_applied=1 priority_applied=1
```

`object_fps_pvp.worker`：A_threads 與 B 都以同一句失敗（`worker acceptance: new window waited behind a fully acknowledged window's deadline`）；W1 的 B 也是同一句。**A_threads 是 os-threads-backend 與 os-wait-backend 之前的程式**，所以這個失敗不是這兩批造成的。是實作還是這台的時序，仍未切開（交 v7.1／OS 子系統的會話）。

證據：`threads/`（`A-ctest-network.log`、`A-ctest-cpu.log`、B 同理、`A-roles.txt`、`B-roles.txt`、`exit.txt`）。

## W6：os-net-errors 的 UDP 錯誤量測（只記錄）

```
python docs/architecture/plans/os-subsystem/scripts/udp_error_probe.py --output <證據目錄>/udp/udp-errors.json
```

13:54，**exit 0**。Python 3.13.3 的 socket 模組，loopback，關閉的埠 53566，等待 1 秒。

| 情境 | 結果 |
|---|---|
| unconnected（未連線，`sendto` 到關閉的埠後 `recvfrom`） | 送出成功；接收得到 **`WSAECONNRESET`（10054，`ConnectionResetError`）** |
| usable-after（出錯之後同一個 socket 和 echo 往返） | 送出、echo 收到、echo 回送、收到 4 bytes，**正常** |
| repeated（連送 3 個再收） | 前 3 次接收各得到一次 `WSAECONNRESET`，第 4 次 1 秒內沒有東西：**每個被拒絕的 datagram 各報一次** |
| connected（已連線的對照） | 接收得到 `WSAECONNRESET`；之後再送成功 |

對照 macOS Intel（文件記載）：未連線時收不到任何東西，已連線時是 `ECONNREFUSED`。Windows 在**未連線**時也會報錯，這是兩者的差別。asio 對應到哪個 `asio::error` 不在這裡量。

證據：`udp/udp-errors.json`、`udp/stdout.log`。

## 留在這台機器上的東西

- worktree：`..\gyo-a-wait`、`..\gyo-a-threads`、`..\gyo-b-mingw`（各含 MSVC 建置樹；`gyo-b-mingw` 還沒建置）。之後接著跑 W4～W8 時可以直接用，沒有刪。
- 證據目錄：B 的 checkout 的 `build/target/_build/test/logs/os-windows-run-20261028/`。
- `%TEMP%` 的殘留：沒有檢查。

## 經驗（給下一次 Windows 執行）

- 這台的 `python` 是 embedded 版：會讓同目錄 import 的腳本失敗（W3 的 `wake_compare.py`、W1 的 28 項驗收）。下次先裝一般版 Python，或照上面的啟動器跑。
- ctest 也要在 vcvars64 環境裡跑，否則 `cmake.ProcessCodePage`、`cmake.ProjectComposition` 找不到 `rc`。
- 會話的執行環境沒有主控台：`os.console_ctrl_c` timeout、`os.console_close` Skipped。
- W3 的 +0.02 ms（P50）邊界，在這台 Windows 上比同側 4 次的極差還小；Windows 上沒有 A/A 校準的資料。

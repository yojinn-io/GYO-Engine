# OS 子系統：Windows 自動項目 第 1 次（os-windows-run，D73）

日期：2026-10-28。分支 `claude/os-windows-run`。計畫見 [windows-run.md](../architecture/plans/os-subsystem/windows-run.md)。

**狀態：進行中（中途報告）。** W0～W2 完成，W1 有失敗（使用者決定 W5 也照跑），W3 量測中，W4～W8 未跑。

- 規則：只跑、只記錄；不改程式碼、測試、突變目錄、腳本與 CMake。判定與門檻照事前宣告（D68、D69、D71），不放寬。
- 證據目錄（不進 repo）：B 的 checkout 裡的 `build/target/_build/test/logs/os-windows-run-20261028/`。
- 路徑中的 Windows 使用者名稱寫成 `<user>`。

## 收支（中途）

計畫 9 項（W0～W8）：跑完 3 項（W0、W1、W2），進行中 1 項（W3），未跑 5 項（W4～W8，排在 W3 之後）。

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

處理（使用者同意）：repo 的腳本改用會話 scratchpad 的啟動器執行。啟動器先把腳本所在目錄加進 `sys.path`，再以 `runpy.run_path(..., run_name="__main__")` 執行原腳本，參數照文件。腳本本身不改；量測程式由腳本直接以 `subprocess` 啟動，不經過啟動器。

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

12:39 開始，量測中。結果出來後補在這裡。

## W4～W8

未跑（排在 W3 之後）。

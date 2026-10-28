# OS 子系統：Windows 自動項目的執行（os-windows-run）

建立：2026-10-28。決定：D73、D74、D76（[交接](HANDOFF.md)的決策紀錄）。分支：`claude/os-windows-run`。
**狀態：第 1 次停在 W3（不通過，紀錄保留）；「第 1 次續跑」等 Windows 端執行（D74、D76）。**

> **2026-10-28 更新（v7.1 會話，D220；OS 收口 Engine D78）：現在只跑 worker-10x 這一項。** OS 子系統已依 D78 收口，本文件其餘的 OS 項目（W 系列、W8、第 2 次）都不再跑。worker-10x 的宣告已核准（v7.1 D217，發行線 `docs/object_fps_pvp/plans/v7.1/worker-windows-timing.md`），請直接跳到下面的「worker-10x」一節照做。結果寫進發行線那份文件的「## 第 3 步的結果」一節（在宣告之後新增，第一行寫收支），commit 訊息用日語，推送前先 `git pull`；不要改宣告、門檻或測試。本分支不必再推結果。

## 這份文件是什麼

- OS 子系統還沒在 Windows 實機上做的項目，凡是不需要人操作的，都由 Windows 機器上的 Claude 照這份文件跑。
- 目的：不等 lan15，把 OS 子系統做完（D73）。
- 分兩次：
  - 第 1 次（本文件）：計時核心三批（os-wake-record、os-threads-backend、os-wait-backend）與 os-utf8-codepage 留下的 Windows 項目，加上 os-net-errors 開工前的 UDP 錯誤量測。
  - 第 2 次：os-net-errors、os-close 完成後，跑它們的 Windows 專用突變與 os-close 的完整突變。內容到時由 OS 子系統的會話補在本文件最後。
- 每一項的來源：交接的「下一次實機的清單」、各批的紀錄、PLAN 第 7.13～7.16 節。

## 範圍

做（都不需要人操作；量測期間只需要機器開著、登入、不鎖定）：

| 代號 | 白話 | 判定 |
|---|---|---|
| W0 | 準備：四份 checkout、三份 MSVC 建置 | — |
| W1 | B 的完整 ctest（MSVC） | 記錄；失敗就停下回報 |
| W2 | 把量測程式從建置樹複製出來 | — |
| W3 | os-wait-backend 的 L2（A/B 交錯 4 對） | **事前凍結的判定**（D71） |
| W4 | os-wake-record 的 Windows 基準 | 只記錄（D68） |
| W5 | os-threads-backend 的 L2 | `role started` 行逐字相同（D69） |
| W6 | os-net-errors 的 UDP 錯誤量測 | 只記錄（D73） |
| W7 | Windows 專用突變 12 條 | killed／survived／未跑 |
| W8 | MinGW 的確認 | 只記錄（D66） |

不做：

- 需要人操作的：Ctrl+Break、在 conhost 手動關閉視窗、P5-6 的手動啟動、lan15 與它的 L3（拔網路線、關 Wi-Fi）。
- `os.console_close`、os-c4：開放決策 26 未決，在預設終端機是 Windows Terminal 的機器上會 Skipped。
- 強制結束時 log 是 0 B（開放決策 25）：要人按第二次 Ctrl+C，歸屬也還沒定。
- Linux 的項目（交接的「下一次實機的清單」的 Linux 部分）。
- os-net-errors、os-close 本身（還沒實作，第 2 次再跑）。

## 規則

- 只跑、只記錄。不改程式碼、測試、突變目錄、腳本與 CMake。發現問題就記下來、回報，修正由 OS 子系統的會話做。
- 停止條件（AGENTS.md §14 開頭與 §14.1）：
  - W3 不通過或無效：**不重跑**，記錄後停下回報使用者。撤回方式是 revert（D71），由 OS 子系統的會話決定。
  - W5 不相同：停下回報（D69 的撤回條件）。
  - W1 有失敗：先回報，問使用者要不要繼續；W3～W7 用到的測試失敗時，不繼續。
  - 突變存活或基準線不通過：不改測試、不改目錄，記錄原因後繼續下一條。
  - 同一個問題換了 2 次做法還沒有結論，或用時超過下面估計的 1.5 倍：停下回報。
- 判定與門檻都已事前宣告（D68、D69、D71）。看到結果之後不放寬。
- 回報的第一行是收支：計畫幾項、跑完幾項、無效或未跑幾項。
- 寫進 repo 的只有：本文件的「結果」一節、新的 dev_log（`docs/dev_logs/<日期>_engine_os_windows_run.zh-Hant.md`）。交接、README、PLAN、os.md、time.md 由 OS 子系統的會話合併時更新，這邊不動（避免兩邊衝突）。
- commit 訊息用日語；推送前先問使用者；只推 `claude/os-windows-run`，不 force push，不合併到其他分支，不通知消費端。
- 路徑中的 Windows 使用者名稱寫成 `<user>`。
- 測試殘留在 `%TEMP%` 的資料夾，刪不掉時不強刪，記錄後交使用者決定。

## W0：準備（約 1～1.5 h，多半是建置）

- 機器條件（照實記錄，不改系統設定）：Windows 版本、CPU、記憶體、系統 ACP（`GetACP` 或登錄檔）、有沒有使用者安裝的字型、開發人員模式、預設終端機、電源計畫。
- 行尾：`git config --local core.autocrlf false`（D210）。每份 checkout 都確認 `git ls-files --eol` 裡沒有 `w/crlf`。find 跨多行的突變在 CRLF 上會 stale。
- 四份 checkout（輸出根目錄在原始碼樹裡，MSVC 與 MinGW 也共用輸出目錄，所以每份建置要有自己的 checkout）：
  - B：`claude/os-windows-run` 的最新 commit。程式和 `c54ef8b5` 相同，多的只有文件。記下完整 SHA。
  - A_wait：`4ffc7c65`（os-wait-backend 之前；Waiter 和 os-wake-record 的 `f6997194` 相同）。
  - A_threads：`5733d361`（os-threads-backend 之前）。
  - B_mingw：和 B 同一個 commit，給 W8 用。
  - 例：`git worktree add ..\gyo-a-wait 4ffc7c65`、`git worktree add ..\gyo-a-threads 5733d361`、`git worktree add ..\gyo-b-mingw <B 的 SHA>`。
- MSVC 建置（B、A_wait、A_threads 各一次；在 VS 的 vcvars64 環境裡）：
  - `cmake --preset ci-windows`
  - `cmake --build build/target/_build/ci-windows`
  - 記錄 MSVC 與 CMake 的版本、每份的 exit code。
- 證據目錄：B 的 checkout 裡的 `build/target/_build/test/logs/os-windows-run-<YYYYMMDD>/`（不進 repo）。

## W1：B 的完整 ctest（約 10～20 min）

- `ctest --test-dir build/target/_build/ci-windows --output-on-failure`，輸出存進證據目錄。
- 預期（不是判定，只是方便判讀）：
  - `os.console_close` 在預設終端機是 Windows Terminal 時 Skipped（開放決策 26）。
  - symlink 的案例在沒開開發人員模式時提早返回（D201）。
  - os-utf8-codepage 補的兩個案例（UNC 底下建目錄、使用者字型目錄）在這台第一次實跑。
- 任何失敗：先回報，問使用者要不要繼續（見「規則」）。

## W2：複製量測程式（約 5 min）

- 從 A_wait 與 B 的 `build/target/_build/ci-windows/tests/common/time/` 複製 `gyo_time_wake_record.exe`、`gyo_time_wake_record_window.exe`，以及同一資料夾的所有 `.dll`（視窗版要 `SDL3.dll`），放到證據目錄的 `programs/A`、`programs/B`。
- 量測以副本為準。**從這裡到 W6 結束，這台機器不建置**，也不跑其他吃重的工作。

## W3：os-wait-backend 的 L2（約 45 min；事前凍結的判定）

- 在 B 的 checkout 執行：

```
python tests/common/time/wake_compare.py --a <證據目錄>/programs/A --b <證據目錄>/programs/B --output <證據目錄>/wait-AB --pairs 4 --modes cli window --a-label "A_wait 4ffc7c65" --b-label "B <B 的短 SHA>"
```

- 判定（開放決策 13 (a)，D71 凍結；腳本照這個規則判，不另外判）：
  - 每種情境（cli、window）與每種量（deadline、notify），取兩側各 4 次的中位數：B 的 P99 ≤ A 的 P99 × 1.25 ＋ 0.05 ms；B 的 P50 ≤ A 的 P50 ＋ 0.02 ms。
  - B 從不在期限前醒來（deadline 的 min_ms ≥ 0）。
  - A 與 B 回報相同的 Waiter 後端。
- exit 0＝通過，1＝不通過，2＝無效。不通過或無效都**不重跑**，停下回報。要不要另跑 A/A 當診斷，由使用者決定。
- 記錄：verdict 與 12 項的值（腳本最後印出的表）、每次的後端名稱、sleeper 的 P99（主機忙碌的程度）、meta.txt 的開始與結束負載。
- 這一項通過，是消費端合併 os-wait-backend 的前提（D71）。通過時只回報使用者；通知消費端由 OS 子系統的會話做。

## W4：os-wake-record 的 Windows 基準（約 6 min；只記錄）

```
python tests/common/time/wake_baseline.py --build-dir <A_wait 的 checkout>/build/target/_build/ci-windows --output <證據目錄>/wake-baseline
```

- 沒有視窗與有視窗兩組。記錄 Waiter、condition_variable、`sleep_until` 的 P50、P99、最大值，notify→wake 的 P50、P99 與跳過的週期，以及 sleeper。
- 沒有視窗那組之後由 OS 子系統的會話交給 v7.1 的 F5／F7。

## W5：os-threads-backend 的 L2（約 10～15 min）

- A_threads 與 B 的建置樹，各跑兩次 ctest（串行，`-V` 才看得到角色的啟動行）：

```
ctest --test-dir build/target/_build/ci-windows -V -R "^object_fps_pvp\.(worker|ipc|close_timing)$"
ctest --test-dir build/target/_build/ci-windows -V -R "^object_fps_pvp\.cpu$"
```

- 輸出存成 `threads/A-ctest-network.log`、`threads/A-ctest-cpu.log`（B 同理）。
- 從兩份輸出取出每一行裡從 `role started` 開始到行尾的文字，去掉行尾空白與 `\r`，去重、排序，存成 `A-roles.txt`、`B-roles.txt`，再比較。
- 判定：兩份**逐字相同**（D69，只記錄、附撤回條件）。macOS 上是四種角色：`gyo-match-sim`、`gyo-match-ipc`、`gyo-client-net`、`gyo-test-sim`。
- 不相同：停下回報。

## W6：os-net-errors 的 UDP 錯誤量測（約 1 min；只記錄）

```
python docs/architecture/plans/os-subsystem/scripts/udp_error_probe.py --output <證據目錄>/udp/udp-errors.json
```

- 量什麼：未連線的 UDP socket（消費端的 Client 就是這樣用：`send_to`、`receive_from`）送到 loopback 上關閉的埠後，下一次接收會不會出錯、出什麼錯；出錯之後 socket 還能不能用；每個被拒絕的 datagram 是否各報一次。另量已連線 socket 作對照。
- 為什麼：開放決策 7 原本建議等 Windows 實機確認 WSAECONNRESET 真的出現再搬分類（消費端 D179 也記著它未經實機確認）。這一項在 loopback 上確認，不需要網路與人。拔網路線時的 WSAENETDOWN 等錯誤仍屬 lan15 的 L3。
- 限界：用的是 Python 的 socket 模組，觀測的是 OS 回報的錯誤；asio 把它對應到哪個 `asio::error` 不在這裡量。
- 對照：macOS Intel（2026-10-28）未連線時收不到任何東西（之後和 echo 的往返正常，三次也都沒有錯誤）；已連線時收到 `ECONNREFUSED`（errno 61），之後再送仍成功。
- 腳本 exit 1（某個情境跑不起來）時，記錄原因，不改腳本。

## W7：Windows 專用突變（約 1～2 h）

- 在 B 的 checkout、vcvars64 環境裡，**每條單獨跑一次**。runner 只要有一條基準線失敗就整批停止（D201 的經驗）。

```
python docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/ci-windows --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json --only <id>
```

- exit 0＝killed；1＝沒有 killed，看印出的結果（`survived`、`stale`＝find 找不到、`wrong_failure`＝失敗但訊息不符、`timeout`、`build_failed`）；2＝基準線不通過（印出 `baseline …: failed` 或 `timeout`）。只有預期的檢查失敗、且訊息符合才算 killed。
- 12 條：

| id | 來源 | 備註 |
|---|---|---|
| pu-x3-windows-message-refuses-surrogate | os-path-utf8-sweep | D201 在 MinGW killed；這次用 MSVC 複跑 |
| pu-x4-windows-message-generic-separators | os-path-utf8-sweep | 同上 |
| pu-x9-windows-manifest-legacy | os-utf8-codepage | |
| pu-x10-windows-tests-without-manifest | os-utf8-codepage | |
| pu-x12-windows-code-page-query-fixed | os-utf8-codepage | |
| os-w10-windows-root-created | os-utf8-codepage 補了 UNC 案例（開放決策 24） | 需要 `\\localhost\C$` 可連；先確認並記錄 |
| os-g18-windows-extra-link | os-utf8-codepage 補了字型目錄（開放決策 24） | 跑全部 `gyo_os_tests` |
| th-x3-windows-name-byte-widened | os-threads-backend | |
| th-x5-windows-interactive-highest | os-threads-backend | |
| ow-x5-windows-timer-not-armed | os-wait-backend | |
| ow-x6-windows-signal-dropped | os-wait-backend | |
| ow-x10-windows-timeout-armed-for-nothing | os-wait-backend | |

- 全部跑完後，`git status` 必須乾淨（runner 每條都以 bytes 還原）。不乾淨就停下回報，不要自己還原。

## W8：MinGW 的確認（約 1 h；只記錄，可在時間不夠時略過並記錄）

- 來源：開放決策 23（D66：兩條路徑都寫，CI 驗 MSVC，MinGW 在 Windows 機器確認）。MinGW 的支援等級還沒定（build-baseline），結果不拿來改支援範圍。
- 在 B_mingw 的 checkout，MinGW 在 PATH 上：`cmake --preset test`、`cmake --build build/target/_build/test`。**不跑完整 ctest**：MinGW 上清理的 `remove_all` 會卡住（D201）。
- 要記錄的四件事：
  1. `.rc` 嵌入的 manifest 是否勝過 MinGW-w64 的預設 manifest：`ctest --test-dir build/target/_build/test -R "^cmake\.ProcessCodePage$"`，以及 `gyo_os_tests -s -tc="*ANSI code page*"`（它讀自己的 `RT_MANIFEST` 確認有 `activeCodePage`）。
  2. repo 路徑含非 ASCII 時，windres 找不找得到 manifest：在含非 ASCII 字元的路徑（例：`C:\gyo-檢查\b`）另開一份 checkout，`cmake --preset test`，只建 `gyo_os_tests`（`cmake --build build/target/_build/test --target gyo_os_tests`），再跑 1. 的那個案例。其他測試在非 ASCII 的 checkout 上會因巨集組成的路徑而壞（交接的未結事項），不跑。
  3. 目標的 compile definitions 經過 windres 是否出問題：1.、2. 的建置 log 裡 windres 的錯誤與警告。
  4. OEM code page 是否也變成 65001：repo 裡沒有現成的觀測程式，這次不做，記為「未做」。

## 第 1 次的時間

約 4～6 h，大部分是建置、量測與突變的等待時間。超過 9 h（1.5 倍）時停下回報。

## 第 1 次續跑（D74、D76）

第 1 次停在 W3（不通過）。使用者的決定（D74）：

- W3 的不通過保留，不重跑，判定不放寬。
- 在 Windows 跑一次 A/A 診斷：同一份 A 程式放在兩側，結果只記錄。同時記錄兩份 `SDL3.dll` 的雜湊。
- 看完診斷之後，由使用者在「撤回 os-wait-backend」與「Windows 另行事前宣告判定」之間決定。這不在 Windows 端決定。
- `object_fps_pvp.worker` 的失敗交給 v7.1 會話（v7.1 D216）。

2026-10-28 14:20 更新：W4～W7 已經照第 1 次的指令跑完（dev_log 的 W4～W7，`844e92b1`、`7306a142`），續跑不再跑這四項。W4 與 W5 也已經回答了 worker 切開的第 1、2 步：沒有 SDL 時 cli 的 `sleep_until` P50 是 15.63 ms；A_threads 上的 worker 也同樣失敗。

2026-10-28 D76：使用者的原則是「Windows 與 Linux 實機只處理 CI 無法處理或不好觀測的任務，驗收的主要標準是 CI」。下表每一項都寫出 CI 做不到的理由，說不出理由的移出：

- 拿掉 W1b（重跑 B 的完整 ctest）：它重複了 CI 的 windows-x64 L1。第 1 次 W1 的結果只當參考；其中 30 項環境類失敗（embedded Python、`rc` 不在 PATH）不是產品或 Engine 的缺陷。
- 拿掉 W1-ctrlc（`os.console_ctrl_c` 的診斷）：這一項標 cpu，CI 每次都跑而且通過。第 1 次的失敗記為會話的執行環境所致，不診斷。
- 拿掉 W0b（裝一般的 Python、B 重新 configure）：它只是為了 W1b。剩下的腳本照第 1 次的方式，用 scratchpad 的啟動器跑；W3-aa 用和 W3 相同的方式，條件也比較一致。D74 的「裝 Python」因此不做。

「規則」一節照舊適用：只跑、只記錄；不改程式碼、測試、突變目錄、腳本與 CMake。worker-10x 另建發行線是例外（D74）。

### 範圍與順序

| 代號 | 白話 | 判定 | 為什麼 CI 做不到 |
|---|---|---|---|
| W3-samples | 補記 W3 每次的樣本數與略過的週期（讀現有證據） | 只記錄 | W3 的證據只在這台機器上；補完實機量測的收支 |
| W3-dll | 兩份 `SDL3.dll` 的雜湊（讀現有副本） | 只記錄 | 同上，是 W3 的診斷 |
| host-state | 量測時段開始與結束時的電源與系統計時器解析度 | 只記錄 | W3-aa 的條件紀錄；CI 的 runner 是共用的虛擬機，沒有這台的條件 |
| W3-aa | Windows 的 A/A 診斷（同一份 A 程式當兩側） | 只記錄（D74） | 晚醒是亞毫秒級的時序，CI 的共用 runner 量不準；W3 的判定本來就定在實機上（D71） |
| worker-10x | v7.1 D216 的 worker 10 次量測（另建發行線）；**宣告有核准才跑** | 照 v7.1 的宣告，只記錄 | CI 在任何平台都不跑 network 標籤的測試（`CMakePresets.json` 的 test preset 只跑 `cpu\|shader`，D204），所以沒有 worker 的 CI 紀錄。要不要改成由 CI 跑，由 v7.1 會話與使用者決定 |
| W8 | MinGW 的確認（第 1 次還沒跑完時才跑） | 只記錄（D66） | CI 沒有 MinGW 的 job（D66：CI 只驗 MSVC） |

- 照表的順序執行。
- host-state、W3-aa、host-state（結束）是量測時段：期間不建置、不跑其他吃重的工作，也不 commit、不推送。第 1 次在 W3 的 pair1 中途推送過一次，這次避開。
- 量測時段接著電源（AC），不改電源計畫。
- 會建置的 worker-10x、W8 排在量測之後。
- 證據目錄沿用第 1 次的 `build/target/_build/test/logs/os-windows-run-20261028/`，檔名見各項。
- 第 1 次留下的 worktree（`..\gyo-a-wait`、`..\gyo-a-threads`、`..\gyo-b-mingw`）與建置樹直接沿用，不重新建立，也不重新建置（dev_log 的「留在這台機器上的東西」）。開始前確認每份的 HEAD 仍是 W0 表裡的 commit、工作樹乾淨。
- 下面的 `<run>` 指第 1 次的啟動器 `python <scratchpad>/run_script.py`（全文在 dev_log 的「和文件不同的地方：Python」）。`python -c` 不經過啟動器。

### W3-samples、W3-dll：讀現有證據（約 10 min；不跑程式）

- W3-samples：
  - 從 `wait-AB/pair<n>-<A|B>-<cli|window>.json` 的 Waiter 兩列，記錄每次的：
    - deadline 的 `samples`
    - notify 的 `samples` 與 `skipped_periods`
  - 共 16 次。補完 W3 樣本層級的收支（AGENTS.md §14 開頭）：deadline 晚醒時不補跑錯過的週期，所以少於 1800 不算丟棄。
- W3-dll：
  - 對 `programs/A/SDL3.dll`、`programs/B/SDL3.dll` 記錄 SHA-256、檔案大小、檔案版本（PowerShell 的 `(Get-Item <檔案>).VersionInfo.FileVersion`）。
  - 為什麼：W3 只有 window 情境不通過，而只有 window 程式載入 `SDL3.dll`。兩份 DLL 各自從自己的建置樹複製，第 1 次只記了 `.exe` 的雜湊。

### host-state：電源與系統計時器解析度（開始與結束各約 1 min；只記錄）

- 在 W3-aa 開始前與結束後各記錄一次，輸出存成 `host-state-start.txt`、`host-state-end.txt`：
  - 電源：PowerShell 的 `(Get-CimInstance -Namespace root/wmi -ClassName BatteryStatus).PowerOnline`（True＝接著 AC；查不到就寫「沒有電池資訊」），以及 `powercfg /getactivescheme`。
  - 系統計時器解析度（三個值，單位 100 ns，照印出的順序記錄）：

```
python -c "import ctypes; v=[ctypes.c_ulong() for _ in range(3)]; s=ctypes.windll.ntdll.NtQueryTimerResolution(*map(ctypes.byref, v)); print('status', s, 'values (100 ns)', [x.value for x in v])"
```

- 第 1 次 W3 時的電源狀態若不知道，就寫「不知道」，不推測。
- 為什麼：這台是筆電（i7-8750H），電源與計時器解析度都會影響晚醒的分布。A/A 要能和 W3 對照，就要知道兩次的條件是否相同。

### W3-aa：Windows 的 A/A 診斷（約 45 min；只記錄，D74）

- 在 B 的 checkout 執行。A 與 B 都給 W2 的同一份 A 副本：

```
<run> tests/common/time/wake_compare.py --a <證據目錄>/programs/A --b <證據目錄>/programs/A --output <證據目錄>/wait-AA --pairs 4 --modes cli window --a-label "A_wait 4ffc7c65" --b-label "A_wait 4ffc7c65 (A/A diagnostic)"
```

- 這是探索，不是驗收判定：
  - 不覆寫 W3 的不通過，不放寬已宣告的判定。
  - 腳本印出的 pass／FAIL 與 exit，只當成「同一份程式兩側的差距」的紀錄。
  - exit 1 或 2 都不重跑，也不停下，記錄後繼續。
- 記錄（和第 1 次的 W3 同一個格式）：
  - 腳本最後印出的 12 項。
  - 每次的 deadline、notify 的 P50／P99／min／max、sleeper 的 P99、樣本數與略過的週期。
  - `meta.txt` 的開始與結束。
- 解讀（撤回，或另行宣告）不在 Windows 端做，交給使用者與 OS 子系統的會話。

### worker-10x：v7.1 D216 的 worker 10 次量測（約 30 min；宣告已核准，v7.1 D217，2026-10-28；D220 起只跑這一項）

- 這是 v7.1 會話的量測，不是 OS 子系統的。這邊只負責在同一台機器上執行。
- 跑之前先確認條件：
  - `git fetch` 之後，讀發行線 `origin/claude/pvp-v71-release` 的 `docs/object_fps_pvp/plans/v7.1/worker-windows-timing.md`。
  - 只有在那份文件記錄了使用者核准時才跑（標題不再是「草案，等使用者核准」，並寫明核准的日期與原話）。
  - 沒有核准就不跑，記「未跑（宣告未核准）」，直接進 W8。
- 跑法照那份文件核准時的版本：建置、指令、次數、收支與分類都照它，這邊不另改，也不加判定。這邊只補機器上的做法：
  - 用發行線的 commit（必須含 `d7e0af31`）另開一份 worktree，例如 `git worktree add ..\gyo-v71 <commit>`。記下完整 SHA，確認 `git ls-files --eol` 沒有 `w/crlf`。
  - 在 vcvars64 環境裡執行 `cmake --preset ci-windows`，再執行 `cmake --build build/target/_build/ci-windows`。worker 是 C++ 的 CTest，不經過 Python。
  - 建置完才開始 10 次。10 次期間不建置、不跑其他吃重的工作。
- worker 失敗不停下，那正是要觀察的現象。建置失敗時記錄 log、不跑，繼續 W8。
- 每次的通過或失敗、`restart_ms`、`completed_window_gap_ms`、失敗訊息，寫進發行線 `worker-windows-timing.md` 的「## 第 3 步的結果」（D220；不再寫續跑的 dev_log）。判讀由 v7.1 會話做。

### W8（照第 1 次的指令）

- 第 1 次已經跑完 W8 的話，續跑不再跑。
- 還沒跑的話，照上面的 W8 跑。

### 續跑的時間

- 約 2.5 h：讀證據約 10 min，量測約 50 min，worker-10x 約 30 min（有核准時），W8 約 1 h（第 1 次還沒跑時）。
- 超過 3.75 h（1.5 倍）時停下回報。

### 續跑的回報

- 第一行是收支：
  - 續跑計畫 6 項（上表）、跑完幾項、未跑幾項（原因）。
  - W3-samples 補的樣本層級收支（一致／不一致／確認不了）。
- 寫進 repo 的只有兩處：本文件「結果」一節的「第 1 次續跑」、新的 dev_log `docs/dev_logs/<日期>_engine_os_windows_run_resume.zh-Hant.md`。
- 原始數值要以表格寫進 dev_log，不只寫摘要。對接的會話只能從 GitHub 讀結果，沒辦法直接問 Windows 端。至少要有：
  - W3-samples、W3-dll、host-state 的值。
  - W3-aa 的 12 項與每次的值（和第 1 次 W3 的表格同一個格式）。
  - worker-10x 每次的值（有跑時）。
- commit 用日語，推送前先問使用者，只推 `claude/os-windows-run`。

## 結果（Windows 端填）

### 第 1 次（2026-10-28，停在 W3；使用者決定停在這裡）

細節：[dev_log](../../../dev_logs/2026_10_28_engine_os_windows_run.zh-Hant.md)。

- 收支：計畫 9 項，跑完 4 項（W0～W3），其中 W3 不通過；未跑 5 項（W4～W8：W3 不通過，照規則停下）。W1 有失敗，使用者決定 W5 也照跑（2026-10-28）。
- 環境：Windows 11 Pro 10.0.26300，i7-8750H（6C/12T），16 GB，ACP＝OEMCP＝65001（登錄檔），開發人員模式關閉，預設終端機未設定，電源計畫「平衡」。B＝`1b97f5d61a9883a1fa063110b4e290dfb1410c96`。MSVC 19.51.36260（VS Community 2026），CMake 4.0.1，MinGW-Builds GCC 15.2.0。
- 和文件不同的地方：這台唯一的 Python 是 embedded 版，`._pth` 讓腳本所在目錄不在 `sys.path`（PYTHONPATH 也被忽略）。repo 的腳本改用會話 scratchpad 的啟動器執行（先把腳本目錄加進 `sys.path`，再以 `runpy` 當 `__main__` 執行，參數照文件）。腳本不改（使用者同意）。
- W0：三份 MSVC 建置的 configure 與 build 都是 exit 0（10:50～11:32）。四份 checkout 都沒有 `w/crlf`。
- W1：exit 8，102 項中 70 通過、32 失敗、2 Skipped。
  - 28 項 object_fps_pvp 的 Python 驗收：`ModuleNotFoundError`（上面的 embedded Python，環境）。
  - `cmake.ProcessCodePage`、`cmake.ProjectComposition`：子專案找不到 `rc`（ctest 在 vcvars64 外面跑，環境）。
  - `os.console_ctrl_c`：`got 4: ['ready', 'timeout']`（推測執行環境沒有可送 Ctrl+C 的主控台，未確認）。
  - **`object_fps_pvp.worker`**：`new window waited behind a fully acknowledged window's deadline`。斷言失敗，原因（實作或時序）未切開；沒有重跑。W5 會用到這一項。
  - Skipped：`os.console_close`（預期）、`object_fps_pvp.ingress_cross`。
- W2：A、B 的量測程式與 `SDL3.dll` 已複製到證據目錄的 `programs/`（雜湊見 dev_log）。
- W3：**exit 1，不通過**（12:39～13:21）。12 項中 11 項通過；`window deadline p50`：A 0.4658、B 0.4890、上限 0.4858 ms（超出 0.0032）。兩側後端都是 `HighResolutionWaitableTimer`；B 沒有早醒（min 0.0291／0.0214 ms）。照規則不重跑，停下回報。撤回與 A/A 診斷由 OS 子系統的會話與使用者決定。觀察（只記錄）：window deadline P50 的同側 4 次極差，A 0.030、B 0.026，大於 +0.02 ms 的邊界；Windows 上沒有 A/A 校準的資料。
- W4～W8：未跑。

### 第 1 次續跑（D74、D76）

（Windows 端填，格式同下。）

格式：

- 收支：計畫 9 項（W0～W8）、跑完幾項、未跑幾項（原因）；W7 是 12 條中 killed、survived、未跑各幾條。
- 環境：機器條件、B 的完整 SHA、MSVC／CMake／MinGW 的版本。
- 每一項：指令的 exit、判定或記錄的數值、證據檔名（證據本身留在 Windows 機器）。
- 停下的地方與原因（若有）。
- 細節寫進 dev_log，這裡只放摘要與 dev_log 的連結。

## 第 2 次（os-net-errors、os-close 之後）

（由 OS 子系統的會話在兩批完成後補。）

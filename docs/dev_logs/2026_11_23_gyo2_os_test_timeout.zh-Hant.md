# 2026-11-23 os 測試在本機偶爾跑很久或逾時（`v8-arch-gyo2-os-test-timeout`）

件數の突き合わせ：一致（負載掃描 18 次執行，每次都有輸出檔與 case-timing 檔；ctest 下的 os 兩支，慢的 4 次留下檔案，快的照設計刪掉；沒有其他丟棄）

Owner：OS 層的測試（`tests/common/os`）。GYO2 主會話開的任務卡。分支 `claude/keen-banach-4f8ae8`（開向 `gyo2`）。作者 Opus 5.5。

## 為什麼

`gyo_os_tests` 與 `os.wake_fallback` 在 CI 的 L1 三個平台一直通過，但本機（macOS Intel）跑全量 ctest、機器又忙時，四張卡碰過它們跑很久或 60 秒逾時（網路卡、狀態機卡、os 使用者資料目錄卡、audio 小批卡）。各卡都判「原因分不出來」，而且逾時的那幾次沒有逐案的輸出，看不出卡在哪一案（網路 Module 合併前核准頁第 9 節；主 checkout `build/evidence/gyo2-state-machine-r8/ctest-full-2.txt`、`ctest-full-3.txt`）。使用者的原則：量不到成因時，先建檢測再評估。

## 假設

1. 測試：某個等待時序的案例在負載下變慢。
2. 實作：WakeSignal（kqueue）或後備實作（mutex＋condition variable）在負載下喚醒慢、或漏掉訊號。
3. 環境：機器負載（同一台機器上其他卡的建置、ctest 自己的並行）。

## 做了什麼（觀測，不改判定）

- `tests/common/os/CaseTiming.{hpp,cpp}`（新）：doctest 的 listener，編進 `gyo_os_tests` 與 `gyo_os_wake_fallback_tests`。
  - 每個案例與 subcase 開始、結束時寫一行：wall、行程的 CPU 時間、context switch 次數、1 分鐘負載（Windows 寫 n/a）。
  - 寫到 stderr，同時寫到檔案，每行 flush。
  - 檔案放在 `$GYO_OS_CASE_TIMING_DIR`，沒設的話放在建置目錄的 `tests/common/os/case-timing/`。
  - 失敗、超過 5 秒、被殺掉的執行留下檔案；5 秒內通過的在結束時刪掉。5 秒只決定檔案留不留，不影響判定。
  - `Note()`：給案例自己寫進度。
- `WakeSignalTests.cpp` 的「loses no signal」案例：每 2000 回合與結束時用 `Note()` 寫回合數、進了 `Block` 的回合數、10 ms 以上的回合數、最長的回合、經過的時間。只讀時鐘、只寫字，沒有任何 CHECK 讀它。
- `tests/common/os/CMakeLists.txt`：兩個執行檔加 `CaseTiming.cpp` 與檔案目錄的編譯定義。後備的執行檔整個 target 都定義了 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`，所以 `CaseTiming.cpp` 先 `#undef`，不出現第二個 main。

確認觀測本身能用：
- 在 `gyo_os_tests` 跑到一半時送 SIGKILL：檔案留下，最後一行是正在跑的案例（`case start "OS wake signal: a signal that ended a block does not end the next one"`）。
- 突變讓案例失敗時，檔案留下，裡面有 `failed=yes`。
- 全量通過時檔案刪掉（最後的全量 ctest）。

## 查證

全部在這台 macOS Intel（i9-9980HK，8 核 16 執行緒）上，core preset、RelWithDebInfo。負載用 `yes` 行程製造。每次執行都有時間上限，跑之前在對話裡說明了「會拖慢其他卡的建置」。

### 1. 閒置時

整份 `gyo_os_tests` 約 0.8 s；最慢的案例 0.14 s。「loses no signal」0.05 s：20000 回合裡 525 回合進了 `Block`，其他回合 signaller 已經先設好旗標。

### 2. 加負載，各跑一次（`load-sweep/`）

| `yes` 數 | `os.wake_fallback` | `gyo_os_tests` | 慢的案例 |
|---|---|---|---|
| 0 | 0.59 s | 1.21 s | — |
| 8 | 0.59 s | 0.81 s | — |
| 16 | 0.62 s | 9.79 s | 「loses no signal」9.07 s（wall 9.07 s、CPU 0.29 s；20000 回合裡 15009 回合進了 `Block`；最長的回合 11 ms） |
| 24 | 0.67 s | 2.50 s | 「loses no signal」1.81 s |

變慢的只有「loses no signal」，其他案例和閒置時一樣。同樣的負載，時間差很多（16 個是 9 s，24 個是 1.8 s）。

### 3. 拆開一回合的時間（實驗，不 commit；`split-sweep/`、`ctest-loaded/`）

暫時在案例裡多記兩個時間點（waiter 開始一回合、signaller 送出訊號），把進了 `Block` 的回合拆成兩段：
- 反應延遲：waiter 開始一回合 → signaller 送出訊號。這段是測試自己的 signaller 執行緒（先自旋 4096 次，再 `yield`）拿到 CPU 的時間。
- 喚醒延遲：訊號送出 → waiter 的 `Block` 返回。這段是 WakeSignal 加上 waiter 被排程的時間。

實驗的差異在 `experiment-latency-split.diff`。

| 條件 | 實作 | 經過 | 進 `Block` 的回合 | 反應延遲 平均／最大 | 喚醒延遲 平均／最大 |
|---|---|---|---|---|---|
| 閒置 | kqueue | 0.065 s | 1097 | 1.7 µs／60 µs | 4.5 µs／43 µs |
| 閒置 | 後備 | 0.070 s | 3847 | 0.9 µs／30 µs | 8.5 µs／115 µs |
| `yes` 16 | 後備 | 5.49 s | 14759 | 363 µs／10.0 ms | 7.8 µs／0.5 ms |
| `yes` 32 | 後備 | 3.48 s | 13475 | 248 µs／10.7 ms | 8.8 µs／64 µs |
| `yes` 24＋兩支同時跑＋其他卡（load 24） | kqueue | 14.15 s | 17643 | 791 µs／10.2 ms | 9.6 µs／0.2 ms |
| 同上 | 後備 | 14.19 s | 17355 | 807 µs／10.3 ms | 10.2 µs／0.15 ms |
| 同上（load 升到 60） | kqueue | 27.91 s | 16363 | 1695 µs／16.7 ms | 9.7 µs／3.4 ms |
| 同上（load 升到 60） | 後備 | 27.68 s | 15400 | 1786 µs／16.7 ms | 10.7 µs／9.4 ms |

最後兩列：ctest `-j2` 只跑這兩支，兩支各 28.1 s、28.6 s，和狀態機卡的 47.38 s／49.03 s（兩支同時跑）是同一種樣子。

### 4. 照原條件跑全量 ctest（`ctest-full/`、`ctest-loaded/`、`final/`）

| 條件 | `gyo_os_tests` | `os.wake_fallback` |
|---|---|---|
| 全量 `-j8`，第 1 次（182 個，39.8 s） | 0.75 s | 0.59 s |
| 緊接著只跑這兩支 | 0.74 s | 0.60 s |
| 全量 `-j8`，第 2 次（25.6 s） | 0.76 s | 0.55 s |
| 緊接著只跑這兩支 | 0.72 s | 0.54 s |
| `yes` 24＋全量 `-j8` | 14.83 s | 14.65 s |
| 最後的全量 `-j4`（其他卡在跑，load 約 20） | 0.74 s | 0.93 s |

60 秒的逾時這次沒有重現。單靠全量 ctest 不夠；加上機器本身的負載（其他卡的建置）才會變慢。要逼到 60 秒得把機器壓得更重，會拖慢其他卡，所以沒有做。

## 對比與推斷

- **一回合的時間幾乎都是反應延遲。** 喚醒延遲在負載下平均仍是 8～11 µs，和閒置時同一個量級；反應延遲從 1～2 µs 漲到 0.25～1.8 ms，最大值約 10 ms，正好是 macOS 的排程時間片。
- **案例的時間 ≈ 進 `Block` 的回合數 × 反應延遲。** 16363 × 1.695 ms ≈ 27.7 s（實測 27.9 s）；17643 × 0.791 ms ≈ 14.0 s（實測 14.1 s）。照這個關係，反應延遲平均到 3.6 ms 左右（約 16500 回合進 `Block`）就會碰到 60 秒的逾時。
- **為什麼反應延遲會變大**：signaller 是自旋後 `yield` 的執行緒。CPU 滿載時，它讓出 CPU 後要排在其他可執行的行程後面，最久等一個時間片。剛被喚醒的 waiter 很快就能跑（喚醒延遲沒有變大），讓出 CPU 的 signaller 卻要等。
- **兩支同時跑時更慢**：兩份「loses no signal」各有兩條執行緒互相交手，ctest 並行時常常同時跑（狀態機卡 ctest-full-2 的 47 s／49 s、這次的 28 s）。
- **實作沒有漏掉訊號**：所有負載下 `lost == 0`，最長的回合 21 ms，離 1.5 s 的判定很遠。kqueue 與後備實作的數字一樣。
- **附帶發現：負載下這個案例的檢出力變弱。** 閒置時進 `Block` 的回合是 3～19%，負載下是 55～88%（每次執行的數字在第 2、3 節的檔案裡）。signaller 大多在 waiter 檢查旗標很久以後才送出訊號，掃過「檢查與 Block 之間」那個瞬間的回合變少。所以負載下就算通過，測到的也比較少。突變 ow-x1、ow-x19 是在閒置時抓到的（見下面）。

## 總結（§14.1 的切分）

| 類 | 判斷 | 依據 |
|---|---|---|
| 測試 | **時間花在這裡**。「loses no signal」是 20000 個前後相接的跨執行緒交手，沒有時間上限；每回合的時間由測試自己的 signaller 執行緒的排程延遲決定 | 第 3 節：反應延遲占一回合的大部分，經過時間 ≈ 回合數 × 反應延遲；其他 79 案在負載下不變 |
| 環境 | **觸發條件**。CPU 的負載超過核心數（同一台機器上的其他建置、ctest 讓兩份同時跑） | 第 2～4 節：閒置與只有 ctest 時不慢，加負載才慢 |
| 實作 | **沒有證據** | 喚醒延遲平均約 10 µs，沒有漏掉訊號；kqueue 與後備一樣 |

- `os.wake_fallback` 那一次「失敗」：當時的輸出沒存到，是逾時還是 `lost` 的判定失敗，現在仍分不出來。之後再出現，會留下 case-timing 檔。
- 網路卡的第 0 步（7.67 s），以及 os-wait 批第一次 L1 的 macos-arm64 `os.wake_fallback` 35.8 s，都符合同一個關係，但當時沒有逐案的紀錄，只能說「符合」。
- 這不是實作的缺陷，所以沒有走 §14.3 寫一個先失敗的測試。要動的是測試本身（回合數、時間上限、執行方式），交使用者決定，見下一節。

## 交使用者決定（§14.4）

這一批只加觀測，沒有改判定。要不要改、怎麼改，請使用者決定：

| 選項 | 內容 | 判定有沒有放寬 | 能消掉的 | 擋不住的 |
|---|---|---|---|---|
| (a) 改執行方式 | 「loses no signal」拆成獨立的 CTest 項目（kqueue 版、後備版各一），設 `RUN_SERIAL`；其餘案例留在原本的兩個項目 | 沒有放寬（同一個判定，只改什麼時候跑） | ctest 裡的並行，以及兩份互相競爭（目前看到的 28 s、47～49 s 都是這種）；逾時時看得出是哪一份 | 其他卡的建置帶來的負載 |
| (b) 時間上限 | 20000 回合或 N 秒先到就停，記錄實際跑了幾回合 | **放寬**：負載下掃描的回合變少。要照 §14.4 準備 1～5、突變、另一個驗證者 | 逾時 | 負載下檢出力變弱（反而更弱） |
| (c) 提高逾時 | 60 s → 例如 300 s | **放寬**（逾時是判定的一部分） | 逾時本身 | 不建議：只是把問題藏起來 |
| (d) 只留觀測 | 本 PR 到此為止，下次出現時看留下的檔案 | 沒有改 | — | 偶發照舊 |

建議 (a)＋(d)：判定不變，處理已經觀測到的情形（ctest 裡兩份同時跑），之後的偶發看 case-timing 檔。負載下檢出力變弱是另一個問題（測試的設計），要不要重新設計，也請使用者決定；這一批不碰。

## 突變（§14.5 與 `mutation-rerun-scope`）

- 這一批沒有加突變，也沒有拿掉（只有觀測，沒有新的判定）。
- 重跑的：殺它們的測試（「loses no signal」）有改的 ow-x1、ow-x19。各跑 5 次，10/10 killed，都是照預期的訊息算的（`mutants.log`）。
- 找出 find 落在改動檔案的突變（全部清單）：只有 `pu-x10-windows-tests-without-manifest`（`tests/common/os/CMakeLists.txt`，Windows 專用），find 仍命中一次。用 ow-x1 當正例，確認這個掃描有作用。

## 檢驗

- 建置：core preset 全部目標，沒有新的 warning 在這一批改的檔。
- `ctest -j4` 全量 182/182（`final/ctest-full.txt`）。
- L1：推上去後看，結果記在 PR。

## 證據

主 checkout `build/evidence/gyo2-os-test-timeout/`（git 不追蹤，這台 macOS Intel 機器）：
- `load_sweep.py`、`sample_load.sh`：重現用的腳本。
- `load-sweep/`、`split-sweep/`：第 2、3 節的每次輸出與 case-timing 檔。
- `experiment-latency-split.diff`：第 3 節暫時加的時間點（沒有 commit）。
- `ctest-full/`、`ctest-loaded/`：第 4 節的 ctest 輸出、`LastTest` 的複本、負載取樣（每 5 秒的 load 與前 8 個吃 CPU 的行程）。
- `final/`：最後的全量 ctest。
- `mutants.log`：ow-x1、ow-x19 各 5 次。

花的時間：約 1 小時。

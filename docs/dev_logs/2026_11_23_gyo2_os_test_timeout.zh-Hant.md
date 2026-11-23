# 2026-11-23 os 測試在本機偶爾跑很久或逾時（`v8-arch-gyo2-os-test-timeout`）

件數の突き合わせ：一致（負載掃描 `load-sweep` 8 次、`split-sweep` 10 次、`yield-sweep` 8 次，每次都有輸出檔；case-timing 檔照設計只留失敗、超過 5 秒、被殺的執行，其餘刪掉；沒有其他丟棄）

Owner：OS 層的測試（`tests/common/os`）。GYO2 主會話開的任務卡。分支 `claude/keen-banach-4f8ae8`（開向 `gyo2`，PR #289）。作者 Opus 5.5；合併前的設計審查 Fable 5.2／high（唯讀）。

## 為什麼

`gyo_os_tests` 與 `os.wake_fallback` 在 CI 的 L1 三個平台一直通過，但本機（macOS Intel）跑全量 ctest、機器又忙時，四張卡碰過它們跑很久或 60 秒逾時（網路卡、狀態機卡、os 使用者資料目錄卡、audio 小批卡）。各卡都判「原因分不出來」，而且逾時的那幾次沒有逐案的輸出，看不出卡在哪一案（網路 Module 合併前核准頁第 9 節；主 checkout `build/evidence/gyo2-state-machine-r8/ctest-full-2.txt`、`ctest-full-3.txt`、`os-tests-rerun.txt`）。使用者的原則：量不到成因時，先建檢測再評估。

## 假設

1. 測試：某個等待時序的案例在負載下變慢。
2. 實作：WakeSignal（kqueue）或後備實作（mutex＋condition variable）在負載下喚醒慢、或漏掉訊號。
3. 環境：機器負載（同一台機器上其他卡的建置、ctest 自己的並行）。

## 做了什麼（觀測，不改判定）

- `tests/common/os/CaseTiming.{hpp,cpp}`（新）：doctest 的 listener，編進 `gyo_os_tests` 與 `gyo_os_wake_fallback_tests`。
  - 每個案例與 subcase 開始、結束時，在檔案裡寫一行：wall、行程的 CPU 時間、context switch 次數、1 分鐘負載（Windows 寫 n/a）。每行 flush。
  - 檔案放在 `$GYO_OS_CASE_TIMING_DIR`（連上層一起建），沒設的話放在建置目錄的 `tests/common/os/case-timing/`（只建最後一層；執行檔拿到別台機器時不會照建置機的路徑建一串目錄）。
  - stderr 只寫檔案路徑，結尾留檔時再寫 `kept <path>`。逐案的行不寫 stderr：突變執行器拿 stdout＋stderr 比 expect，有的 expect 是案例名，原本只有那一案失敗時 doctest 才印出來（合併前審查 M1）。
  - 失敗、超過 5 秒、被殺掉的執行留下檔案；5 秒內通過的在結束時刪掉。5 秒只決定檔案留不留，不影響判定。
  - `Note()`：給案例自己寫進度（也只寫檔案）。
  - 什麼時候拿掉（審查 S7）：案例裡的進度 Note 在使用者對這個案例的決定落地後拿掉；listener 在批次表沒有未結的 os 測試本機逾時時拿掉。寫在 `CaseTiming.hpp`。
- `WakeSignalTests.cpp` 的「loses no signal」案例：每 2000 回合與結束時用 `Note()` 寫回合數、進了 `Block` 的回合數、10 ms 以上的回合數、最長的回合、經過的時間。回合時間沿用 `lost` 判定那一次讀的時鐘，迴圈裡沒有多讀時鐘（審查 S5：最初的版本在 `consumed.store` 之後多讀一次，落在掃描的相位上）。沒有任何 CHECK 讀這些值。
- `tests/common/os/CMakeLists.txt`：兩個執行檔加 `CaseTiming.cpp` 與檔案目錄的編譯定義。後備的執行檔整個 target 都定義了 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`，所以 `CaseTiming.cpp` 先 `#undef`，不出現第二個 main。

確認觀測本身能用：
- 在 `gyo_os_tests` 跑到一半時送 SIGKILL：檔案留下，最後一行是正在跑的案例（`case start "OS wake signal: a signal that ended a block does not end the next one"`）。審查者另外試過一次，結果相同。
- 突變讓案例失敗時，檔案留下，裡面有 `failed=yes`。
- 通過的 `gyo_os_tests -tc="*create directories*"` 的輸出不再含 os-w4、os-w5 的 expect 字串（修 M1 之前有 2 次）。

## 查證

全部在這台 macOS Intel（i9-9980HK，8 核 16 執行緒）上，core preset、RelWithDebInfo。負載用 `yes` 行程製造。每次執行都有時間上限，跑之前在對話裡說明了「會拖慢其他卡的建置」。

### 1. 閒置時

整份 `gyo_os_tests` 約 0.8 s，73 案；最慢的案例 0.14 s。「loses no signal」0.045～0.087 s；20000 回合裡進了 `Block` 的有 500～3847 回合（2.5～19%，`load-sweep`、`split-sweep`、`yield-idle.txt`），其他回合 signaller 已經先設好旗標。

### 2. 加負載，各跑一次（`load-sweep/`）

| `yes` 數 | `os.wake_fallback` | `gyo_os_tests` | 慢的案例 |
|---|---|---|---|
| 0 | 0.59 s | 1.21 s | — |
| 8 | 0.59 s | 0.81 s | — |
| 16 | 0.62 s | 9.79 s | 「loses no signal」9.07 s（wall 9.07 s、CPU 0.29 s；15009 回合進了 `Block`；最長的回合 11 ms） |
| 24 | 0.67 s | 2.50 s | 「loses no signal」1.81 s |

變慢的只有「loses no signal」，其餘 72 案的總和在各種負載下都是 0.65～0.69 s（審查者重算）。同樣的負載，時間差很多（16 個是 9 s，24 個是 1.8 s）。

### 3. 拆開一回合的時間（實驗，不 commit）

暫時在案例裡多記兩個時間點（waiter 開始一回合、signaller 送出訊號），把進了 `Block` 的回合拆成兩段：
- 反應延遲：waiter 開始一回合 → signaller 送出訊號。這段是測試自己的 signaller 執行緒（先自旋 4096 次，再 `yield`）注意到回合開始的時間。
- 喚醒延遲：訊號送出 → waiter 的 `Block` 返回。這段是 WakeSignal 加上 waiter 被排程的時間（也含 `Signal()` 本身，對「不在 WakeSignal」的結論是保守的）。

實驗的差異在 `experiment-latency-split.diff`。下表前 10 列是 `split-sweep/` 的全部執行，後 4 列是 `ctest-loaded/`（第 4 節）。後 4 列與第 4 節「`yes` 24＋全量」都是用這個實驗版跑的。

| 條件 | 實作 | 經過 | 進 `Block` 的回合 | 反應延遲 平均／最大 | 喚醒延遲 平均／最大 |
|---|---|---|---|---|---|
| 閒置 | kqueue | 0.065 s | 1097 | 1.7 µs／60 µs | 4.5 µs／43 µs |
| 閒置 | 後備 | 0.070 s | 3847 | 0.9 µs／30 µs | 8.5 µs／115 µs |
| `yes` 16 | kqueue | 0.990 s | 11087 | 79 µs／10.0 ms | 7.7 µs／1.47 ms |
| `yes` 16 | kqueue | 0.384 s | 11704 | 23 µs／3.5 ms | 7.9 µs／0.16 ms |
| `yes` 16 | 後備 | 0.894 s | 11532 | 68 µs／10.2 ms | 7.8 µs／0.72 ms |
| `yes` 16 | 後備 | 5.488 s | 14759 | 363 µs／10.0 ms | 7.8 µs／0.50 ms |
| `yes` 24 | kqueue | 1.931 s | 14211 | 126 µs／10.3 ms | 8.6 µs／0.40 ms |
| `yes` 24 | 後備 | 1.552 s | 14170 | 100 µs／9.8 ms | 9.0 µs／63 µs |
| `yes` 32 | kqueue | 1.005 s | 12869 | 67 µs／10.0 ms | 8.5 µs／41 µs |
| `yes` 32 | 後備 | 3.476 s | 13475 | 248 µs／10.7 ms | 8.8 µs／64 µs |
| `yes` 24，兩支同時跑（load 24） | kqueue | 14.15 s | 17643 | 791 µs／10.2 ms | 9.6 µs／0.21 ms |
| 同上 | 後備 | 14.19 s | 17355 | 807 µs／10.3 ms | 10.2 µs／0.15 ms |
| 同上，load 升到 60（來源不明） | kqueue | 27.91 s | 16363 | 1695 µs／16.7 ms | 9.7 µs／3.4 ms |
| 同上，load 升到 60 | 後備 | 27.68 s | 15400 | 1786 µs／16.7 ms | 10.7 µs／9.4 ms |

### 4. 照原條件跑 ctest（`ctest-full/`、`ctest-loaded/`、`final/`）

| 條件 | `gyo_os_tests` | `os.wake_fallback` |
|---|---|---|
| 全量 `-j8`，第 1 次（182 個，39.8 s） | 0.75 s | 0.59 s |
| 緊接著只跑這兩支 | 0.74 s | 0.60 s |
| 全量 `-j8`，第 2 次（25.6 s） | 0.76 s | 0.55 s |
| 緊接著只跑這兩支 | 0.72 s | 0.54 s |
| `yes` 24＋全量 `-j8`（實驗版） | 14.83 s | 14.65 s |
| `yes` 24＋只跑這兩支 `-j2`，第 1 次 | 2.82 s | 2.68 s |
| 同上，第 2 次（30 秒內 load 從 24 升到 60） | 28.60 s | 28.10 s |
| 同上，第 3 次 | 3.25 s | 3.20 s |
| 最後的全量 `-j4`（其他卡在跑，load 約 20） | 0.74 s | 0.93 s |

60 秒的逾時這次沒有重現。只有 ctest 自己的並行不會變慢；要加上機器本身的負載才會。要逼到 60 秒得把機器壓得更重，會拖慢其他卡，所以沒有做。load 升到 60 的那 30 秒，負載取樣（`load-samples.txt`，只留前 8 名）全是 `yes`，多出來的行程是誰沒有抓到。

### 5. signaller 的 yield 次數（實驗，不 commit；`yield-sweep/`、`yield-idle.txt`）

合併前審查 S2 推斷「signaller 每回合走到 `yield`，負載下一次 `yield` 就讓出最多一個時間片」，但沒有量。暫時數 signaller 兩個自旋迴圈的 `yield` 次數（差異在 `experiment-yield-count.diff`）：

| 條件 | 經過 | 進 `Block` 的回合 | 等 `consumed` 時的 yield | 等回合開始時的 yield |
|---|---|---|---|---|
| 閒置（兩支各 3 次） | 0.055～0.087 s | 1000～1402 | 19870～26301 | 527～1893 |
| `yes` 16，後備 | 5.68 s | 5517 | 5509 | 11 |
| `yes` 16，後備 | 6.02 s | 5993 | 5943 | 7 |
| `yes` 16，後備 | 7.50 s | 5306 | 5284 | 8 |
| `yes` 16，kqueue | 9.70 s | 15331 | 18330 | 25 |

閒置時 signaller 也是大約每回合 yield 一次，但沒有別的執行緒要跑，`yield` 立刻回來，整個案例 0.06 s。負載下 yield 的次數和進 `Block` 的回合數幾乎一樣，而每次 yield 都把 CPU 讓給其他行程，最多一個時間片（反應延遲的最大值約 10 ms）。

## 對比與推斷

- **一回合的時間幾乎都是反應延遲。** 喚醒延遲在負載下平均仍是 7.7～10.7 µs，和閒置時同一個量級；反應延遲從 1～2 µs 漲到 0.02～1.8 ms，最大值約 10 ms，正好是 macOS 的排程時間片。
- **案例的時間 ≈ 進 `Block` 的回合數 × 反應延遲。** 16363 × 1.695 ms ≈ 27.7 s（實測 27.9 s）；17643 × 0.791 ms ≈ 14.0 s（實測 14.1 s）。審查者把 `split-sweep` 10 次全算，差額固定在 0.11～0.14 s。照這個關係，反應延遲平均到 3.6 ms 左右（約 16500 回合進 `Block`）就會碰到 60 秒的逾時。
- **為什麼反應延遲會變大**（第 5 節量到的）：signaller 自旋 4096 次還等不到就 `yield`。閒置時 `yield` 不花時間；CPU 滿載時，`yield` 讓出 CPU 後要排在其他可執行的行程後面，最久等一個時間片。剛被喚醒的 waiter 很快就能跑（喚醒延遲沒有變大）。signaller 一旦讓出，下一回合 waiter 又會進 `Block`，所以一進這個狀態就會持續下去，同樣的負載時間差很多（第 2 節）。
- **只在 macOS 出現**：L1 只有 os-wait 批第一次 L1 的 macos-arm64 `os.wake_fallback` 35.8 s 出現過。Linux、Windows 的 `yield` 行為不同可能是原因，**沒有驗證**。
- **兩支同時跑不是原因**（審查 M2 指出，原本的版本寫反了）：`gyo_os_tests` 單獨跑也出現過 25 s 與 60 s 逾時（`gyo2-state-machine-r8/os-tests-rerun.txt`）；唯一有紀錄的全量逾時，`os.wake_fallback` 同時跑只花 0.50 s（`ctest-full-3.txt`）；這次 `yes` 24＋兩支同時跑三次，只有外來負載升到 60 的那一次慢（第 4 節）。差的是外來負載。
- **實作沒有證據**：kqueue 與後備的喚醒延遲一樣，所有負載下都沒有 `lost`。但負載下掃描很少落在「檢查與 `Block` 之間」（下一點），所以「負載下 `lost == 0`」是弱證據；強證據是喚醒延遲的分佈與閒置時的突變（審查 S4）。
- **附帶發現：負載下這個案例的檢出力變弱。** 閒置時進 `Block` 的回合是 2.5～19%，負載下是 26～88%（每次執行的數字在第 2、3、5 節的檔案裡）。signaller 大多在 waiter 檢查旗標很久以後才送出訊號，掃過那個瞬間的回合變少。所以負載下就算通過，測到的也比較少。突變 ow-x1、ow-x19 是在閒置時抓到的。
- **沒有查的假設**（審查 4）：thermal、QoS／task role、第一次啟動的掃描（XProtect）。它們解釋不了「只有這一案、最大值 10 ms」，優先度低。

## 總結（§14.1 的切分）

| 類 | 判斷 | 依據 |
|---|---|---|
| 測試 | **時間花在這裡**。「loses no signal」是 20000 個前後相接的跨執行緒交手，沒有時間上限；signaller 每回合大約 yield 一次，負載下每次 yield 最多讓出一個時間片 | 第 3 節：反應延遲占一回合的大部分，經過時間 ≈ 回合數 × 反應延遲；第 5 節：yield 次數 ≈ 進 `Block` 的回合數；其他 72 案在負載下不變 |
| 環境 | **觸發條件**。機器的 CPU 負載超過核心數（同一台機器上的其他建置）。只有 ctest 自己的並行不夠 | 第 2～4 節 |
| 實作 | **沒有證據** | 喚醒延遲平均約 10 µs；kqueue 與後備一樣；閒置時的突變都抓得到 |

- `os.wake_fallback` 那一次「失敗」：當時的輸出沒存到，是逾時還是 `lost` 的判定失敗，現在仍分不出來（§14 的「範圍の限界」，之後碰到這兩支的宣告要寫）。之後再出現，會留下 case-timing 檔。
- 網路卡的第 0 步（7.67 s），以及 os-wait 批第一次 L1 的 macos-arm64 `os.wake_fallback` 35.8 s，都符合同一個關係，但當時沒有逐案的紀錄，只能說「符合」。
- 這不是實作的缺陷，所以沒有走 §14.3 寫一個先失敗的測試。要動的是測試本身，交使用者決定，見下一節。

## 交使用者決定（§14.4）

這一批只加觀測，沒有改判定。

使用者 2026-11-23 先選了「(a)＋(d)，審查回來後照做」。審查 M2 指出我寫的 (a) 的效果（「能消掉 28 s、47～49 s」）沒有根據，選擇的前提錯了，所以 (a) 沒有做，修正後的選項重新交使用者：

| 選項 | 內容 | 判定有沒有放寬 | 對觀測到的逾時 | 其他 |
|---|---|---|---|---|
| (a) 拆項目＋`RUN_SERIAL` | 「loses no signal」用 `-tc`／`-tce` 拆成獨立的 CTest 項目，設 `RUN_SERIAL` | 判定不變，但把 Design Invariant 8 的守衛案例從原項目移出：名字寫錯時 doctest 跑 0 案也回 0，守衛會無聲消失，要加「恰好跑 1 案」的檢查；屬 §14.4 的「從執行對象排除」，要 1～5 與核准 | **沒有效果的證據**：單獨跑也逾時過 | `RUN_SERIAL` 讓機器忙時其他測試都等它 |
| (b) 時間上限 | 20000 回合或 N 秒先到就停 | **放寬**：負載下掃描的回合變少 | 消掉逾時 | 負載下檢出力更弱 |
| (c) 提高逾時 | 60 s → 例如 300 s（這一案的突變本來就用 300 s 跑） | **放寬** | 消掉逾時本身 | 不建議：把問題藏起來 |
| (d) 只留觀測 | 本 PR 到此為止，下次出現時看留下的檔案 | 沒有改 | 偶發照舊 | — |
| (e) 改測試自己的退避 | 例如多核時 signaller 只自旋不 `yield`，或調 `SpinsBeforeYield` | 改 Design Invariant 8 守衛測試的機制：要 1～5、ow-x1／ow-x19 在閒置與負載下都重驗、另一個驗證者 | 針對源頭（第 5 節） | 也可能處理負載下檢出力變弱；`yield` 的退路是為了單核也能前進，拿掉要另想 |
| (f) CTest 的資源宣告 | 兩個項目共用一個 `RESOURCE_LOCK`，或 `PROCESSORS 2` | 不改跑的內容，沒有過濾的風險 | **沒有效果的證據**（同 (a)） | 不擋其他測試 |

建議：(d) 先合這個 PR；(e) 另開一批，因為它改 Design Invariant 8 守衛測試的機制，要照 §14.4 走完整流程。(a)、(f) 對觀測到的逾時沒有證據，不建議。

**使用者的決定（2026-11-23）**：選「(d)＋另開 (e)（建議）」（本卡對話的提問卡片）。這個 PR 只留觀測；(e) 另開一批，開卡與 HANDOFF 由主會話處理（本卡把決定照原話、比過的選項、理由、出處轉給主會話）。(a) 不做。

## 合併前的設計審查

Fable 5.2／high（唯讀），報告：主 checkout `build/evidence/gyo2-os-test-timeout/merge-review-fable.md`。結論「改了再合併」：must 2、should 8、nit 7。審查時 L1（run 45095143633，d41b4c1d）三平台通過。

| 項 | 處理 |
|---|---|
| M1 案例名寫到 stderr，讓 expect＝案例名的突變恆命中（os-w4、os-w5 確定放寬） | 照建議 1：逐案的行只寫檔案，stderr 只寫路徑。os-w1、os-w4、os-w5、os-w7 重跑 4/4 killed（Windows 的 6 個在這台 skip）；`CaseTiming.hpp` 寫明理由 |
| M2 (a) 的效果與建議沒有根據 | 改寫「對比與推斷」與選項表；把 `os-tests-rerun.txt`、`ctest-full-3.txt`、pair 第 1、3 次補進來；使用者先前的選擇重新交使用者 |
| S1 第 3 節是選列；件數那一行；實驗版沒標 | 第 3 節全列；件數改寫；標明哪些是實驗版 |
| S2 yield 的機制是推斷 | 照建議量 yield 次數（第 5 節），量到的部分寫成結論；只在 macOS 的部分標「沒有驗證」 |
| S3 漏列 (e)(f) | 加進選項表；(c) 補 300 s |
| S4 `lost == 0` 是弱證據 | 加限定 |
| S5 多讀一次時鐘落在掃描相位 | 回合時間改用 `lost` 判定那次讀的時鐘，迴圈裡不再多讀；審查者的前後對照見「突變」 |
| S6 批次表第 122 列過時 | 交主會話改（決定還沒定） |
| S7 觀測留到什麼時候 | 寫進 `CaseTiming.hpp` 與本頁 |
| S8 編譯時的預設目錄會建一串目錄 | 預設只建最後一層，環境變數指定的才連上層一起建 |
| N1 `#undef` | 照現狀（審查者確認正確） |
| N2 欄位的意思 | 寫進 `CaseTiming.hpp` |
| N3 註解範例 | 改正 |
| N4 525、3%、79 案 | 改成留下的證據裡的數字（500～3847、2.5%、72 案） |
| N5 失敗的執行不清 | 照現狀：檔案很小 |
| N6 崩潰時在 signal handler 裡拿鎖 | 照現狀：和 doctest 自己的 reporter 同一類風險 |
| N7 Windows 只有 wall、MinGW | 照現狀 |

## 突變（§14.5 與 `mutation-rerun-scope`）

- 這一批沒有加突變，也沒有拿掉。
- 殺它們的測試（「loses no signal」）有改的 ow-x1、ow-x19：
  - 最初的版本各 5 次，10/10 killed（`mutants.log`）。
  - 審查者的前後對照（scratchpad，未保存；閒置，各 8 次交錯）：base 與最初的版本都 8/8 killed，「遺失 3 次時的回合數」中位數 ow-x1 1537→257、ow-x19 769→1634，n=8、變異大，不下結論。
  - 修 S5 之後各 3 次，6/6 killed（`mutants-after-review.log`）。
- 輸出改了的影響（M1）：expect 比對的是輸出，修 M1 後重跑 os-w1、os-w4、os-w5、os-w7，4/4 killed，都是照預期的訊息算的（`mutants-after-review-osw.log`）。
- find 落在改動檔案的突變（全部清單）：只有 `pu-x10-windows-tests-without-manifest`（`tests/common/os/CMakeLists.txt`，Windows 專用），find 仍命中一次。用 ow-x1 當正例，確認這個掃描有作用。

## 檢驗

- 建置：core preset 全部目標，沒有新的 warning 在這一批改的檔。
- 修審查之前：`ctest -j4` 全量 182/182（`final/ctest-full.txt`）；跟上 gyo2 後 os 與 naming 的 9 個通過。修審查之後：全量 182/182（`after-review/ctest-full.txt`）。
- L1：d41b4c1d 三平台通過（run 45095143633）；修審查後的 commit 推上去後再看，結果記在 PR。

## 證據

主 checkout `build/evidence/gyo2-os-test-timeout/`（git 不追蹤，這台 macOS Intel 機器）：
- `load_sweep.py`、`sample_load.sh`：重現用的腳本。
- `load-sweep/`、`split-sweep/`、`yield-sweep/`、`yield-idle.txt`：第 2、3、5 節的每次輸出與 case-timing 檔。
- `experiment-latency-split.diff`、`experiment-yield-count.diff`：第 3、5 節暫時加的實驗（沒有 commit）。
- `ctest-full/`、`ctest-loaded/`：第 4 節的 ctest 輸出、`LastTest` 的複本、負載取樣（每 5 秒的 load 與前 8 個吃 CPU 的行程）。
- `final/`、`after-review/`：修審查之前、之後的全量 ctest。
- `mutants.log`、`mutants-after-review.log`、`mutants-after-review-osw.log`：突變。
- `merge-review-fable.md`：合併前的設計審查。

花的時間：約 2 小時（含審查與修正）。

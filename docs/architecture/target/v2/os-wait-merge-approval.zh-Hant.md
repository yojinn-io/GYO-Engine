# 合併前核准：core/time 的等待改呼叫 core/os（`v8-arch-gyo2-os-wait`）

件數の突き合わせ：該当なし（本批沒有計測：沒有送出與收到的件數，計時驗收由宣告 5.4 取代）。

日期：2026-11-21。分支 `claude/gyo2-os-wait-4a65a6`（PR #262，base `gyo2`，已 merge 跟上 24956477）。事前宣告：[os-wait-declaration.zh-Hant.md](os-wait-declaration.zh-Hant.md)（修訂 3，2026-11-21 動工前核准「照建議，全部核准，開始」）。經過與證據：[dev_log](../../../dev_logs/2026_11_21_gyo2_os_wait.zh-Hant.md)。

**請你核准的**（兩段式的第二段）：第 2 節（os.md、time.md、architecture.md 條文的最終文字）、第 3 節（突變的數目）、第 4 節（和宣告不同的地方）、第 5 節（沒做、記下的事）。核准後你說「合併」才合併。

## 1. 判定的結果（宣告 §7）

| 項目 | 結果 | 證據 |
|---|---|---|
| macOS 本機全量 ctest | 157/157（第一次、merge #260 後、審查修正後，三次都是） | 主 checkout `build/evidence/gyo2-os-wait/ctest_*.log` |
| L1 三平台 | 第一次推送 c0becb21：linux-x64、windows-x64、macos-arm64 全部通過（run 44902893585）。審查修正後 fe6b472b：三平台全部通過（run 44905136580；windows-x64 159/159、macos-arm64 與 linux-x64 157/157）。`gyo_os_tests`：windows-x64 13.6 s → 1.3 s；`os.wake_fallback`：macos-arm64 35.8 s → 1.9 s | GitHub Actions |
| 平台呼叫的機械式對照 | 24 個記號新舊次數全部相同；diff 只有參數型別、錯誤建構（★）、cv 後備的 pending 旗標與外殼 | `mechanical/`；勾選表在 dev_log |
| 突變 | 全部 killed（數目見第 3 節）；ow-x1、ow-x19（競爭）各 5 次，5/5 | `mutants/` |
| 審查 | Fable 5.2／high：必修 2、建議 5、細節 9，全部處理（dev_log 的「審查」節）；兼 §14.4 的驗證者：5.2、5.3 已確認，5.4 第 4 點的一處更正（見第 4 節） | 審查報告在本卡的對話裡（未另存） |

## 2. 條文的最終文字

**os.md**
- Components 的 Wait 列：`WakeSignal`（`Create`、`Signal`、`Block`、`Mechanism`），landed；寫明使用者 2026-11-21 的 owner 決定解除 D78 的凍結，以 L1、守衛、突變取代計時 L2。表下說明加「或使用者的 owner 決定」。
- Contracts 加 Wait：一個 pending 訊號至多結束一次 `Block`，和逾時同時到的可能由這次或下一次取走；每個平台的機制；逾時上限 1 小時；`IoFailed` 帶 native error；moved-from 的行為。
- Invariant 3 與 Guard 段：`Threads::Threads` 也給等待的後備用。
- Invariant 6：加一個例外，`WakeSignal::Block` 讓後備的 `std::system_error` 穿過（同 D72）。
- Invariant 8：engine/os 不讀 Time 的 latch；pending 是等待原語自己的狀態；寫上守衛的測試名與突變（ow-x1、x3、x11、x12、x13、x16～x22）。
- Replacement Criterion 2：改成觸發條件「Wait 的後端改了計時行為，或 Feature 需要 L1 給不了的亞毫秒判定：提議把 Wait 拆成獨立元件，帶自己的計時驗收」。
- Replacement Criterion 9：加「使用者的 owner 決定也可以把凍結的元件帶回來；這樣的批次照它自己的宣告驗收，宣告要寫出 ★ 與 §14.4 的判定改變」，並記下 Wait 是這樣回來的。

**time.md**
- 開頭：平台機制是 engine/os 的 `WakeSignal`，Waiter 不呼叫 OS API。依賴圖加 PRIVATE 的 `GYO::Os`。
- Contracts：`Backend()` 是 engine/os 用的機制；`Create()` 的 detail 是 os 的錯誤（★）。Baselines 的 Windows 列改成「搬移前的 Waiter」。
- Invariant 3：加 `tt1-notify-without-signal`；記下「Notify 先設 latch 再 Signal 的順序沒有守衛」。
- Invariant 5：交給 `Os::WakeSignal::Block` 的一律是相對逾時；保留「No absolute time … handed to the OS」。
- Invariant 7：名字是 Time 的 `WaitBackend`，`static_assert` 值對值對應；選擇的守衛改指 `tests/common/os` 的案例（ow-x8、x14、x15），對應的守衛是「Time Waiter reports the os mechanism」（ow-x7）。
- Invariant 8：engine/os 不讀 latch；守衛是結構上的（API 沒有 latch 參數；os 連不到 time）。
- Invariant 10：連結 exactly `GYO::Base` 與 PRIVATE 的 `GYO::Os`，`GYO::Os` 不得在 INTERFACE；守衛 `ti-x1`、`ow-x2`。
- Replacement Criterion 3：刪掉，寫明它在 W3 觸發過、以凍結收場、原因沒查明，這次以宣告 5.4 取代計時驗收。

**docs/architecture.md**：`GYO::Os` 列加等待原語；`GYO::Time` 列的依賴改成「Base; Os privately, for the wait primitive」。

## 3. 突變的數目（README 基準 8）

動到的 Design Invariants 條文 8 條（os 3、6、8；time 3、5、7、8、10）× 5 ＝ 40。本批加 20：os-subsystem 的 ow-x 系列 19（x1～x4、x7～x9、x11～x22），time-threads-trace 的 `tt1-notify-without-signal` 1。沒有超過。

- 移植 `756073aa` 的 7 個（ow-x1～x4、x7～x9），改寫成新的形狀。
- 敵對式檢查（宣告修訂 2）之後新加的 5 個（ow-x11～x15）：後備的漏叫、不清旗標、kqueue 少了 `EV_CLEAR`、CMake 在 Apple 選錯檔、後備回報錯。
- 實作審查之後加的 8 個（ow-x16～x22、`tt1-notify-without-signal`）：每一個抓一個舊突變抓不到的錯（`what` 裡寫了「only it catches」）。
- 改寫、沒有新增的：`tt1-notify-dropped`、`tt1-no-recheck`（find）、`ti-x1`、`ti-x5`（expect）、`os-g6-stale`（改指 `RoleThreadTests.cpp R2 4`）、`th-x2`（`what`）；碰到的都補了 `guards`。
- 臨時的證據突變 2 個（`gyo_time` 多連 Math、多連 Threads）只當 5.2 第 5 點的證據，不進清單。

## 4. 和宣告不同的地方

| # | 宣告 | 實際 | 理由 |
|---|---|---|---|
| 1 | 5.4 第 4 點：「空轉由測試與突變守」 | 實作審查找到兩處沒守：後備只在進入時清旗標、無逾時的 `Block` 立刻返回。補了兩個案例與 ow-x16、x17、x18 之後才成立 | 審查（必修 1、2） |
| 2 | 5.4 第 4 點：不早醒由 time.md Invariant 5 守 | 是 Invariant 2（`tt1-waiter-early`） | 審查指出的筆誤 |
| 3 | 6.1 的測試清單 | 多了「a signal that ended a block does not end the next one」「a block without a timeout ends only on a signal」，以及看門狗 | 審查（必修 1、2，建議 1） |
| 4 | 6.1 的競爭測試：每一輪先取走剩下的訊號 | 只有一輪沒做任何 `Block` 時才取走（signaller 送出後發布 `signalled`） | 每輪都取太貴：第一次 L1 macos-arm64 的 `os.wake_fallback` 35.8 s（逾時 60 s）。改了之後本機 1.0 s，ow-x1、ow-x19 的檢出力 5/5 |
| 5 | 6.3 的突變表 | 多了 ow-x16～x22、`tt1-notify-without-signal`；ow-x4、x9 的 `guards` 是 os 9（逾時換算） | 審查 |

## 5. 沒做、記下的事

- Time 的原始碼現在能呼叫 os 的任何 API，不限 `WakeSignal`，沒有守衛（5.2 的附註，審查者）。目前 time 只 include `WakeSignal.hpp`。
- `Notify` 的順序（先 latch 再 Signal）沒有守衛：對調之後只在奈秒級的窗口裡出錯，測試抓不到；記在 time.md Invariant 3。
- ★ 的失敗注入測試（讓 `kqueue()` 失敗，檢查 `IoFailed` 與 `errno`）不加：舊版也沒有，沒有觀測到疏漏（§14.5）。
- Windows 上沒有跑突變；Windows 的機制選錯、旗標抄錯只靠記號清單（宣告 §7 的範圍限界）。
- `tests/common/os/CMakeLists.txt` 的 OS 定義守衛豁免清單寫的是舊路徑 `engine/os`，這一批沒改（範圍外）。
- 其餘後續見宣告第 10 節。

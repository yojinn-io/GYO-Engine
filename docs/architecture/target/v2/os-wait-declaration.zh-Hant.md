# 事前宣告：core/time 的等待改呼叫 core/os（`v8-arch-gyo2-os-wait`）

更新：2026-11-21（修訂 2）。Owner：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡）。
**狀態：草稿，待使用者核准（兩段式：先核准動工，突變與驗證做完後再定案條文，同 D71／D72）。核准之前不改 `engine/` 與 `tests/`。**

經過：

- 使用者 2026-11-21 定了 os 的定位與這張卡的順序：「os是底層兼容層，對外也只是提供一個介面（否則就沒有意義了）。需要調用OS能力的時候，就去調用os介面就好了。不需要碰底層OS api」；「開os卡，先查 W3 的原因。再做core/os，完畢之後，讓time直接調用core/os的api」。
- 第 1 步查 W3（第 2 節）。查的途中使用者說：「這個東西不用自己折騰，去查開源程序，或者github有沒有類似專案」「這個問題不會是我們第一個碰到，特別是這種兼容代碼」。查了成熟專案的寫法（第 2.4 節）後，使用者選驗收的方式：「選 (a)，照純搬移寫宣告」（第 2.5 節）。
- 修訂 0：第一份草稿。使用者指出 os 是內化的能力、不是 Module（「等等，os 不用登記module。為何會合Module有關？」「這是內化的能力。」），用詞改成「引擎單位」。
- 修訂 1：加上和 #260 的突變協調（6.3）。用 merge 跟上 gyo2 的 #261（2acf9cee，core、ecs、orchestration 不再登記在 `config/modules.csv`）。time 對 os 的連結改成手寫在 `engine/core/time/CMakeLists.txt`，連結期望改 `gyo_expect_links`，不再動登記檔（5.1、5.2、6.3）。
- 修訂 2（這一份）：照 Fable 5.2／high 的敵對式檢查（唯讀，必修 4、建議 8、細節 4）修訂。主要改動：
  - W3 的結論降到證據撐得住的程度，範圍限界照實寫（2.3、§7）；
  - 寫明這一批的根據是使用者 11-21 的 owner 決定，不是 os.md RC9 意義下的純搬移（2.5、§5、§9 的決定 2）；
  - cv 後備的 pending 約定寫死，突變改成移植 `756073aa` 的 ow-x 系列再補新的（3.1、6.3）；
  - §14.4 補上 Invariant 7 的守衛、以 L1 取代 D71 計時驗收這兩份 1～5（5.3、5.4）；
  - 條文清單補齊（5.1），檔案清單補 hygiene 測試（6.1），和 #260 的交集逐條列出（6.3）。
  檢查報告的全文在本卡的對話裡（未另存），逐條的處理見各節。

根據：`docs/architecture/os.md`（Components 的 Wait 列、Design Invariants 6、8、9，Replacement Criteria 2、7、9）、`docs/architecture/time.md`（Design Invariants 5、7、8、10，Replacement Criterion 3）、`docs/architecture/plans/os-subsystem/closeout.md`（第 2、3、5 節）、同目錄 `HANDOFF.md` 的 D71、D72、D74、D78，以及 `docs/dev_logs/2026_10_28_engine_os_windows_run.zh-Hant.md` 的 W3 節。舊的搬移在 commit `4041cf42`、`756073aa`（只在備份分支 `os-subsystem-75418486` 上）。

---

## 1. 這一批做什麼

### 1.1 用遊戲講

- Match 伺服器每 16.7 ms 跑一次 tick。跑完之後，用 `Time::Waiter::WaitUntil(下一次 tick 的時間點)` 睡到下一次。
- 寫日誌的執行緒平常睡著。主執行緒丟一筆日誌，就用 `Notify()` 叫醒它。

`Waiter` 管兩類東西：

- **規矩**：只有一條執行緒在等；「叫」會被記住（latch）；deadline 到了一定醒，絕不早醒；醒來後重讀時間。
- **平台機制**：怎麼讓執行緒睡著、怎麼被叫醒、怎麼計時。macOS 用 kqueue，Windows 用 event 加高解析度 waitable timer，其他平台用 condition variable。

今天兩類都寫在 `engine/core/time/src/Waiter.cpp` 裡。後者直接呼叫 OS API，所以被凍結在 `tests/common/os/os_boundary_allowlist.txt` 上（R1 3、R2 6、R3 10）。

### 1.2 改成什麼

- **平台機制**搬進 `engine/core/os`，做成一個介面 `Engine::Os::WakeSignal`（第 3 節）。平台實作放在 `src/<平台>/`。
- **規矩**留在 core/time。`Waiter` 的公開 API 不變，內部只呼叫 `Os::WakeSignal`。
- 准許清單上 `Waiter.cpp` 的三行刪掉。`tests/common/time/WaiterTests.cpp` 那一行，連同依 OS 分支的期望，搬進 `tests/common/os`（第 6 節）。

### 1.3 不做什麼

- 不改 `Waiter` 的公開 API，也不改 `WaitBackend` 的名字（consumer 的 log 帶著它們，time.md Invariant 7）。
- 不加新的平台後端。例如 Linux 用 futex、eventfd 或 timerfd 的專用後端，這批不做。Linux 照舊走 condition variable。
- 不改計時的數值行為：逾時換算（100 ns 進位、毫秒後備）、`NOTE_CRITICAL`、高解析度 timer 的選擇與退回，都照原樣（os.md Invariant 9）。
- 不碰 `engine/modules/world_time`，那是第 4 批（draft #259）的範圍。
- 不改 namespace 的根（`Engine::Os`），根的改名另有一批機械式處理。
- `src/thread/Thread.cpp` 也是「一個檔案裡用 `#if` 分支」的舊形狀，這批不動，列為後續（第 10 節）。

---

## 2. 第 1 步：W3 查到什麼

照「假設 → 查證 → 對比 → 推斷 → 總結」的順序。時間約 1.5 小時。

### 2.1 W3 是什麼

2026-10-28 Windows 實機第 1 次量測（os-windows-run）的第 3 項，是 os-wait-backend 的 Windows 計時 L2：

- A＝搬移前（`4ffc7c65`），B＝搬移後（`756073aa` 系列）。
- 交錯跑 4 對，每對的順序固定為 A-cli、A-window、B-cli、B-window。每次 30 秒，60 Hz。
- 判定（D71，看結果前凍結，`tests/common/time/wake_compare.py`，commit `beb14cf7`）：每側 4 次取中位數，P99 要 ≤ A × 1.25 + 0.05 ms，P50 要 ≤ A + 0.02 ms；另外要求不早醒、兩側後端相同。
- 結果：12 項中 11 項通過。window 情境的 deadline P50，A 0.4658、B 0.4890，上限 0.4858，超出 0.0032 ms。

### 2.2 假設與查證

| 假設 | 怎麼查 | 結果 |
|---|---|---|
| H1：搬移改了 Windows 的平台呼叫 | 逐行對照 `4ffc7c65:engine/time/src/Waiter.cpp` 與 `756073aa:engine/os/src/wait/WakeSignal.cpp` 的 Windows 分支 | **不成立**。`Block`、`Signal`、`Create` 的平台呼叫逐行相同，差別只有參數型別（`Time::Duration` → `std::chrono::nanoseconds`，兩者同為 `int64` 奈秒）與錯誤細節的格式。這次重驗了收口時說「沒有逐行重驗」的那份分析（D74）。 |
| H2：跨程式庫呼叫加一次間接存取，造成 0.023 ms | 看 `gyo_os` 的建置型別；估呼叫的成本 | **不成立**。`gyo_os` 是 `STATIC`（`756073aa:engine/os/CMakeLists.txt`），「跨程式庫」其實是同一個執行檔裡多一次沒有 inline 的函式呼叫，加一次 `unique_ptr` 的間接存取，成本是奈秒級。和 0.023 ms 差了三到四個數量級。而且同一條路徑在 cli 情境沒有變差（B 比 A 低 0.006 ms）。 |
| H3：純粹是量測雜訊 | 用 W3 的 16 次逐次數值，估「同一支程式比自己（A/A）」超出 +0.02 ms 的機率（`build/evidence/gyo2-os-wait/w3_stats.py`，常態近似、200,000 次模擬） | **只成立一半**。window 情境逐次的標準差約 0.012 ms，A/A 超出邊界的機率約 1.6%；4 對裡有 3 對是 B 較高（+0.025、+0.035、+0.027、−0.015 ms）。所以 window 的位移可能是真的，單靠雜訊不容易解釋。 |
| H4：判定本身不適合 Windows | 同上的模擬；對照 D71 的校準條件 | **成立**。+0.02 ms 只在 macOS Intel 上用 A/A 校準過（那裡 P50 約 0.077 ms，邊界約為 P50 的 26%）。到了 Windows，window 情境只剩 4.3%；cli 情境逐次的標準差約 0.052 ms，A/A 就有約 **31%** 的機率不通過，等於擲硬幣。Windows 上沒有 A/A 的資料。 |
| H5：位移來自搬移以外的差別 | 讀 W3 的紀錄與腳本 | **無法查證，但有三個具體的候選**：<br>① 只有 window 情境載入 `SDL3.dll`，A、B 是兩個 checkout 各自建置的，兩份 DLL 的雜湊沒有記錄（D74）；<br>② 每一對裡 B 一律排在 A 之後，對內的漂移會一直偏向 B；<br>③ 兩份執行檔的程式碼配置（對齊、位址）不同。<br>要分辨這三個，需要再量（D74 的 (B)）。 |

### 2.3 推斷與總結

- **原因未查明；沒有證據指向平台呼叫。** 平台呼叫逐行相同（H1）；已知的成本（多一次函式呼叫與間接存取）是奈秒級，解釋不了 0.023 ms，而且同一條路徑的 cli 情境沒有變差（H2）。
- window 的位移可能是真的（H3）。H5 的三個候選裡，③ 程式碼配置不同，本身就是搬移帶來的，所以不能說「和搬移無關」。D74 的 (B)（Windows A/A）被 D78 取消，原因一直沒查（`closeout.md` 第 2 節）。
- H3 的 1.6% 是用每側 4 筆估標準差、再以常態模擬得到的，只當量級看。審查者用同一組數字算：成對差 3 正 1 負，雙樣本 t 約 2.1，單側 p 約 0.04。
- 當時的判定在 Windows 上校準不足（H4）。
- 同一個形狀（兩次配置、兩層間接）在 macOS Intel 的 L2 是 12／12 通過（`HANDOFF.md` 的 w6-l2）；在 Windows 是 W3，11／12，window deadline P50 不通過。W3 是這個形狀在 Windows 上唯一的量測，不通過照原樣保留（§7 的範圍限界）。
- 就算位移是真的：0.023 ms 是 60 Hz 一個 tick（16.7 ms）的 0.14%，是 Windows 晚醒本身（約 0.47 ms）的 5%。
- **gyo2 上現在量不到 window 情境**：gyo2 還沒有 SDL（`tests/common/time/` 只有沒有視窗的 `gyo_time_wake_record`）。
- 使用 `Waiter` 的對象也和當年不同。gyo2 上現在只有 `core/threads` 的 `RoleThread`，和 `modules/logging` 寫日誌的執行緒；Client 的畫面節奏、Match 的 tick 還不在 gyo2 上。

### 2.4 成熟專案的寫法（使用者要求先查）

| 專案 | Windows | macOS | 能不能被叫醒 |
|---|---|---|---|
| Go 1.23 runtime | 高解析度 waitable timer；拿不到時退回 `timeBeginPeriod` | — | 能（semaphore 加 `WaitForMultipleObjects`） |
| Rust std（≥1.75）`thread::sleep` | 高解析度 waitable timer；失敗時退回 `Sleep` | — | 不能 |
| Python 3.11 `time.sleep` | waitable timer（bpo-21302），之後加上高解析度旗標 | — | 不能 |
| SDL3 `SDL_DelayNS` | 高解析度 waitable timer（原始碼已確認） | — | 不能 |
| .NET 的 Redplcs.HighResolutionTimer | 高解析度 waitable timer | kqueue `EVFILT_TIMER` 加 `NOTE_CRITICAL`，喚醒用 `EVFILT_USER` | 能 |
| **我們的 `Waiter`** | 高解析度 timer，失敗時退回一般 timer，加 auto-reset event | kqueue 加 `NOTE_CRITICAL` 加 `EVFILT_USER` | 能 |

- 我們的 Windows 寫法和 Go、Rust、SDL3 是同一套，macOS 寫法和 Redplcs 逐項相同。`Waiter` 本來就是照成熟方案寫的；這一批只改它所在的位置，不改寫法。
- 沒有能直接拿來用的庫。要「亞毫秒的計時、能被另一條執行緒叫醒、三個平台都有」，又能放進 core 的，沒有找到：
  - **SDL3** 是 Plugin，core 不能參照（AGENTS §6）。而且它只能二選一：`SDL_DelayNS` 準，但不能被叫醒；能被叫醒的 condition 等待，在 Windows 是 `SleepConditionVariableSRW`，逾時只到毫秒。
  - **libuv**：計時器以毫秒計。（修訂 1 也寫了 asio；審查者無法確認，這裡拿掉，不影響結論。）
  - **nanotime**：只有 sleep。
  - **libxev**：沒有 Windows。
- 證據：原始碼的副本放在這台 Mac（macOS Intel）主 checkout 的 `build/evidence/gyo2-os-wait/ref/`（Go `os_windows.go`、Rust `thread/windows.rs`、SDL3 `SDL_systimer.c`、`SDL_timer.c`、`SDL_syscond_cv.c`，2026-11-21 取自各專案的 GitHub 主線）。Python 與 Redplcs 兩列只看過、**未保存**，所以「macOS 寫法和 Redplcs 逐項相同」無法覆核，只當出處。
- 這張表回答「怎麼做」（AGENTS §2 的根據 2），不是這一批的根據。根據見 §5 第 1 項。

### 2.5 使用者的決定（照四樣記，轉主會話寫進 HANDOFF）

- **原話**：「選 (a)，照純搬移寫宣告」（本卡，2026-11-21）。
- **比過的選項**：
  - (a) 當作純搬移：用 L1 三平台、守衛、平台呼叫逐行相同的對照來驗收；Windows 實機計時不判定。
  - (b) 照卡片原樣：Windows 先跑 A/A，重新推出門檻，再量 A/B。Windows 約 3 小時，agent 約 4～6 小時。
  - (c) 換成第三方庫：沒有合適的。
- **理由**：
  - 機制已由成熟專案驗證過（2.4）；
  - 這次只換位置，平台呼叫逐行相同（2.2 的 H1、H2）；
  - 當時的判定在 Windows 上校準不足（H4）。
- **修訂 2 的補正（審查者指出）**：(a) 就是 `closeout.md` 決定 2 的 (d)：「計時這類 CI 量不準的搬移，只靠 L1 結構測試加守衛就收」，那裡已寫明這等於放寬 D71 的驗收前提，要另寫 §14.4 的 1～5 交你核准。1～5 寫在 5.4。修訂 1 寫的「和 os.md RC9 一致」不對：RC9 說需要 ★ 或改測試判定的就不是純搬移，這一批兩樣都有（見 §9 的決定 2）。
- **出處**：本卡的對話；本檔第 2 節；`build/evidence/gyo2-os-wait/`。
- **改變了什麼**：主會話開卡時寫的「Windows 實機計時照宣告」，改成「本批不做 Windows 實機計時」（第 7 節）。

---

## 3. os 的等待介面

### 3.1 形狀（照 `Waiter` 現在的需要推出來）

`Waiter` 向平台要的只有四件事。每一件都對到 `Waiter.cpp` 現有的程式：

| `Waiter` 需要 | 現在的程式 | os 的介面 |
|---|---|---|
| 建立平台的資源，失敗要知道原因 | `Waiter::Create` 的 `kqueue()`／`CreateEventW`／`CreateWaitableTimerExW` | `static Base::Result<WakeSignal, OsError> Create()` |
| 從任何執行緒叫醒 | `Impl::Signal` | `void Signal() noexcept` |
| 睡到被叫醒、相對逾時到期，或虛假喚醒 | `Impl::Block(optional<Duration>)` | `void Block(std::optional<std::chrono::nanoseconds> timeout)` |
| 說出用了哪個機制 | `Impl::backend` | `WaitMechanism Mechanism() const` |

```cpp
// engine/core/os/include/engine/os/WakeSignal.hpp（形狀；文字在實作時定）
namespace Engine::Os {

enum class WaitMechanism : std::uint8_t { Kqueue, HighResolutionWaitableTimer, WaitableTimer, ConditionVariable };

class WakeSignal final {
public:
    [[nodiscard]] static Base::Result<WakeSignal, OsError> Create();
    WakeSignal(WakeSignal&&) noexcept;  WakeSignal& operator=(WakeSignal&&) noexcept;
    WakeSignal(const WakeSignal&) = delete;  WakeSignal& operator=(const WakeSignal&) = delete;
    ~WakeSignal();

    // Any thread. A signal sent while no block is in progress stays pending and ends the next block at once;
    // several pending signals end one block. Moved-from: does nothing.
    void Signal() noexcept;
    // The single blocking thread. Returns when signalled, when the relative timeout has passed (none: only a
    // signal ends it), or spuriously. The caller re-checks its own state and its time base.
    void Block(std::optional<std::chrono::nanoseconds> timeout);
    [[nodiscard]] WaitMechanism Mechanism() const;

private:
    struct Impl;  std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Os
```

約定：

- **「待處理的叫醒」歸 os 的資源所有，三個平台都是「取走即清」。**
  - kqueue：`EVFILT_USER` 以 `EV_CLEAR` 註冊（`Waiter.cpp:136`），`kevent` 取走時清掉。
  - Windows：auto-reset event（`Waiter.cpp:141` 的第二個 `FALSE`），等待返回時清掉。
  - condition variable 後備（新的邏輯，不是搬移）：`Signal` 在 mutex 內設 pending 旗標，出了 mutex 再 `notify_one`。`Block` 在 mutex 內先看旗標，有就清掉並返回；沒有就等，**每一條返回路徑**（被叫醒、逾時、虛假喚醒）都在 mutex 內清掉旗標。
  - 所以 os 不讀 Time 的 latch（3.3 的採用列）。
- 移動後（moved-from）的物件：`Signal` 什麼都不做；`Block`、`Mechanism` 是 `GYO_ASSERT`（同 `756073aa`，有測試）。
- `Waiter` 的 latch（`notified`）照舊是 Time 的狀態。`Waiter::Notify` 先設 latch 再 `Signal()`；`WaitUntil` 的迴圈照舊在每次醒來後先看 deadline、再看 latch。新的設計下，三個平台都靠「先設 latch 再 Signal」這個順序才不丟通知。把順序對調的突變是既有的缺口（不是這一批造成的），照 §14.5 等觀測到疏漏再加，這裡記一筆。
- `WaitMechanism` 和 `WaitBackend` 的值一一對應，名字不變。對應關係由 time 的 `static_assert` 守住，同 `756073aa`。
- 失敗是 `OsErrorCode::IoFailed`，加 native error（`errno=<n>`／`win32=<n>`，os.md 的固定格式）。`Waiter::Create` 照舊回 `TimeErrorCode::WaitPrimitiveUnavailable`，message 不變；detail 改由 `Base::CauseDetail` 帶上 os 的錯誤（★，見 3.4）。
- `Block` 不是 `noexcept`：condition variable 的後備讓標準函式庫的 `std::system_error` 穿過，和今天的 `Waiter` 相同。這需要 os.md Invariant 6 加一句例外，同 D72（第 5 節）。

### 3.2 怎麼避開上次被懷疑的成本

上次被懷疑的是「跨程式庫呼叫加一次間接存取」。第 2 節的結論是：它不是 W3 的原因，成本是奈秒級。這次不特別為它改形狀：

- `gyo_os` 仍是 `STATIC`。同一個執行檔裡，`Waiter → WakeSignal` 多一次普通的函式呼叫。
- `WakeSignal` 由 `Waiter::Impl` 以值持有；`WakeSignal` 自己的 `Impl` 是另一次配置，在 `Create` 時做。所以形狀和 W3 的 B 一樣，是兩次配置、兩層間接（`Waiter::Impl` → `WakeSignal::Impl`），不是改善。
- **不放進標頭 inline**（退掉，3.3 的 R3）：inline 要把 `<windows.h>`、`<sys/event.h>` 放進公開標頭，違反 os.md Invariant 4。

### 3.3 系譜與退掉的選項（AGENTS §4）

| 選項 | 系譜 | 決定 | 理由（決定性的是哪個 Feature 或 Contract） |
|---|---|---|---|
| **採用：`WakeSignal` 四個操作，pending 由 os 的資源持有** | 從 `4041cf42` 的 `WakeSignal` 來，去掉 `Block` 的 latch 參數。去掉 latch 參數這點，是 `756073aa` 的審查（w5-review 的設計備忘）提過、當時沒做的做法。 | 採用 | ① 使用者的原則：os 只提供介面，time 只呼叫介面（四個操作就是 `Waiter` 用到的全部）。② 狀態權威（AGENTS §3；os.md Invariant 8、time.md Invariant 8）：latch 是 Time 的，os 不讀它。三個平台的約定一致，os.md Invariant 8 的跨邊界例外可以刪掉。 |
| R1：照 `4041cf42` 原樣，`Block(const atomic<bool>& latch, …)` | 舊的搬移 | 退掉 | os 要讀 Time 的狀態（condition variable 後備在 mutex 內讀 latch），需要 os.md Invariant 8 的跨邊界例外。三個平台的 `Block` 約定也不一致（只有後備讀 latch）。和這一批要守的狀態權威相反。 |
| R2：os 提供完整的 `Waiter`（latch、deadline 迴圈都搬進 os） | 新導出的 | 退掉 | latch、單一等待者、不早醒、先報 deadline，是 Time 的約定（time.md Invariants 2～5），不是 OS 的差異。全搬進 os，os 就有了政策（os.md Invariant 7）。 |
| R3：標頭 inline，平台碼放進公開標頭 | 卡片提示的方向 | 退掉 | 違反 os.md Invariant 4（公開標頭不洩漏 OS）。它要省的成本是奈秒級，不是 W3 的原因（第 2 節）。 |
| R4：用第三方庫（SDL3 等） | 2.4 | 退掉 | 沒有同時滿足「亞毫秒、能被叫醒、三個平台」的庫；SDL3 是 Plugin，core 不能參照。 |
| R5：一個檔案裡用 `#if` 分三支（`756073aa` 的 `src/wait/WakeSignal.cpp`） | 舊的搬移 | 退掉 | 使用者 2026-11-21 定的：core 的平台實作用 `src/<平台>/`。 |

「是不是在為舊形狀找位置」（`docs/architecture/target/README.md` 的檢查項）：
- 採用列：不是。舊形狀是「time 自己呼叫 OS」；新形狀是「os 提供介面、time 呼叫它」，介面從現有呼叫者用到的最小集合推出（3.1 的表），pending 改歸 os，和舊的「os 讀 Time 的 latch」不同。
- 檔案配置（§4）：照使用者的 `src/<平台>/`，不沿用 `756073aa` 的 `src/wait/` 一檔三支。

選這個形狀的費用（只當費用比較，不當決定的理由，AGENTS §4）：time.md 的突變要改兩個 find（第 6.3 節）；`WaiterTests.cpp` 有一個案例要改寫。

### 3.4 ★ 宣告的行為差異（只有一項）

- **`Waiter::Create` 失敗時的 detail 文字**：
  - 今天：macOS 是 `strerror(errno)`，Windows 是 `GetLastError=<n>`。
  - 改成：`IoFailed: errno=<n>`／`IoFailed: win32=<n>`（由 `Base::CauseDetail` 帶上 os 的錯誤）。
  - message 不變，`TimeErrorCode` 不變。
  - D72 核准過同一項差異。沒有程式讀這段 detail：審查者以 `git grep` 掃 detail 的文字（`GetLastError=`、`kqueue() failed`、`CreateEventW failed`），`Waiter.cpp` 以外 0 筆。實作時照 §14.2 重跑，先用 `Waiter.cpp` 本身當正例命中一次，並把命令和結果寫進 dev_log。（修訂 1 寫「`WaitPrimitiveUnavailable` 只出現在 `TimeError.hpp`」不對，`Waiter.cpp:133,138,143,152` 也有；而且掃錯誤碼的名字找不到讀 detail 的人。）
  - 這只在 OS 拒絕建立 kqueue、event、timer 時才會發生；正常執行看不到。
  - 和 os.md Replacement Criterion 9 的關係：RC9 說需要 ★ 的就不是純搬移。這一項連同 §14.4 的判定改變，一起在 §9 的決定 2 處理。

condition variable 後備的 pending 旗標（3.1）**不是** ★：`Waiter` 看到的結果相同。審查者確認：在「先設 latch 再 Signal」「醒來先看 deadline 再看 latch」的順序下，pending 旗標只會多出虛假喚醒，推不出丟通知或早醒。副作用：今天的 cv 後備在 Linux 上不留訊號（`WaiterTests.cpp:86-87` 的註解），改了以後會留，`tt1-no-recheck` 在 Linux 也殺得到。os.md Invariant 9 列的可觀測行為（`WaitBackend` 的字串、逾時換算、例外規格）都不變。

---

## 4. 檔案配置

| 檔案 | 內容 | 在哪些平台編譯 |
|---|---|---|
| `engine/core/os/include/engine/os/WakeSignal.hpp` | 3.1 的介面 | 全部 |
| `engine/core/os/src/windows/WakeSignal.cpp` | auto-reset event 加高解析度 timer，退回一般 timer | Windows |
| `engine/core/os/src/macos/WakeSignal.cpp`（新目錄） | kqueue：`EVFILT_USER` 加 `EVFILT_TIMER` 的 `NOTE_CRITICAL` | Apple |
| `engine/core/os/src/linux/WakeSignal.cpp`（新目錄） | condition variable 加 os 自己的 pending 旗標（只用標準函式庫） | 既不是 Windows、也不是 Apple 的平台（CMake 的 `else()`） |

- 選 `src/macos/`、`src/linux/` 的理由：使用者定的「`src/<平台>/`」。kqueue 是 macOS 的機制，不是 POSIX；Linux 是 Match 的主戰場。將來若要換成 futex、eventfd、timerfd，也落在同一個檔案。
- **`src/linux` 在 CMake 是 `else()`**：GYO 支援的三個平台裡，它只對應 Linux；內容目前只用標準函式庫，是通用的後備（`756073aa` 的註解寫「generic fallback, not a Linux one」）。這裡選擇用平台名，並在檔頭註解寫明「其他平台也落到這裡」，理由是使用者的規則以平台分目錄，而 Linux 專用後端之後也放這裡。
- **os.md RC7（「per-OS files are mostly duplicates」）**：三個檔各自重複建構、移動、解構，以及 `Signal`／`Block`／`Mechanism` 的外殼，每檔約 20～30 行。這一批接受這份重複：三個 `Impl` 的內容完全不同，共用外殼就得放一個條件式編譯的共用檔，或再加一層內部介面，都比重複的外殼更難讀。若之後第四個平台或第二個能力也要同樣的外殼，就是 RC7 成立，照 RC7 提議。
- 退掉 `src/posix/`（kqueue 不是 POSIX）。也退掉 `src/common/`：`common` 現在的意思是「每個平台都編譯、和 OS 無關」，條件式編譯的檔案放進去會改變它的意思。
- `src/linux/WakeSignal.cpp` 只用標準函式庫，所以也能在 macOS 編譯。測試利用這點，在本機跑它的競爭測試和突變（6.1）。這只是測試的建置方式，產品的選擇照上表。
- `engine/core/os/CMakeLists.txt` 的開頭註解（說明 `src/common`、`src/<os>`、`src/thread`）跟著更新。

---

## 5. Architecture Delta（AGENTS §3 的 7 項）

1. **哪個 Feature 或目的**：使用者 2026-11-21 指示的 Refactoring 目的（owner 決定）：os 是底層兼容層，只對外提供介面；需要 OS 能力時呼叫 os 的介面，不碰底層 OS API。這一批讓 core/time 符合它，**取代 D78 對 Wait 的凍結**。
   - 這不是 os.md RC9 意義下「觀測到的壓力」：准許清單上的 `Waiter.cpp` 在 D78 收口時就在，`closeout.md` 正是拿它當「有壓力時才搬」的例子，使用者當時仍選擇凍結。這次的根據是新的 owner 決定。
   - 層：**基盤 Architecture**（AGENTS §2）。上面疊的是 `Waiter` → `core/threads` 的 `RoleThread` → `modules/logging` 的寫入執行緒（`engine/modules/logging/src/JsonLinesFileSink.cpp`）；Client 的畫面節奏與 Match 的 tick 之後也會疊上來。
   - 支撐的 Feature 與 Product（AGENTS §4 的第三項）：gyo2 上目前沒有 Product。介面不是為預見的 Feature 設計的，而是從現有呼叫者（`Waiter`）用到的最小集合推出；它不加任何新能力，所以不需要預見的根據。
2. **現在的問題**：`Waiter.cpp` 直接 include OS 標頭、用 OS 巨集分支、呼叫 kqueue／Win32 API（准許清單 R1 3、R2 6、R3 10）；`WaiterTests.cpp` 依 OS 分支（R2 2）。
3. **變化的邊界**：os 多一個公開元件 `WakeSignal`；time 從「自己呼叫 OS」變成「呼叫 os」。
4. **影響的單位、Product、Tool**（AGENTS §3 的第 4 項；這裡的「單位」是 `engine/` 底下各層的程式庫，core 不是 Module）：`core/os`、`core/time`（含它的 CMakeLists）、`tests/common/os`、`tests/common/time`、兩份突變目錄、准許清單。`core/threads` 與 `modules/logging` 只透過 `Waiter` 間接受影響，API 不變。沒有 Product、Tool 受影響。
5. **依賴方向**：新增 `GYO::Time → GYO::Os`（PRIVATE）。同一層裡的 `GYO::Threads → GYO::Os` 已經是 PRIVATE，方向一致（time、threads 在 os 之上，os 在 base 之上）。不會造成循環：os 只連 base。
6. **Ownership**：平台機制的 owner 從 time 移到 os。latch 等規矩仍屬 time。
7. **為什麼沒有更小的做法**：要讓 time 不碰 OS API，平台呼叫就得放到 os；能放進 os 的最小單位，就是 `Waiter` 用到的四個操作（3.1）。

### 5.1 條文的修改

條文的文字照兩段式：核准動工時同意方向，突變與審查者的驗證做完後再定案（同 D71／D72）。

**os.md**

| 位置 | 現在 | 改成 |
|---|---|---|
| Components 的 Wait 列；Components 表下的說明（「A frozen or not-started component comes back only with observed pressure or a Feature」） | frozen（D78），後端留在 `Waiter.cpp` | Wait 列：`WakeSignal`，等待原語（`Create`、`Signal`、`Block`、`Mechanism`），`v8-arch-gyo2-os-wait`（landed）。表下的說明：加上「或使用者的 owner 決定」（§9 的決定 2） |
| Contracts | 沒有 Wait | 加 Wait 的約定（3.1 的「約定」各點，與每個平台的機制） |
| 開頭的依賴圖 | `engine/time ──→ engine/os` 等舊路徑 | 只改和 Wait 相關的一行；其他路徑的過時另列後續（第 10 節） |
| Invariant 3 與 Guard 段的連結檢查說明 | `Threads::Threads` 只為了 `pthread_setname_np`（D69） | 加上 cv 後備（`std::mutex`、`std::condition_variable`）。連帶改 `engine/core/os/CMakeLists.txt`、`tests/common/os/CMakeLists.txt` 的註解，以及 `th-x2` 的 `what`（`756073aa` 當時改過這幾處） |
| Invariant 6 | 沒有例外 | 加一句：`WakeSignal::Block` 在 cv 後備讓標準函式庫的 `std::system_error` 穿過，和搬移前的 `Waiter` 相同（同 D72） |
| Invariant 8 | 「Waiter's latch belongs to Time: engine/os reads it through a reference, under the condition variable's mutex in the fallback backend, and keeps no copy.」 | 「Waiter's latch belongs to Time; engine/os never reads it. A pending signal is the wake primitive's own state on every platform and is consumed when a block returns (kqueue's user event with `EV_CLEAR`, the Windows auto-reset event, the fallback's flag set and cleared under its mutex).」並寫上守衛名（6.3 的 ow-x1、ow-x11、ow-x12、ow-x13，與對應的測試） |
| Invariant 9 | 不變 | 不變 |
| Replacement Criterion 2 | 「動 os 任何元件都要重跑 Wait 的計時 L2 時，提議把 Wait 拆成獨立元件」 | 保持「置換的觸發條件」的寫法：「A change to the Wait backends changes timing behaviour (another mechanism, another timeout conversion), or a Feature needs a sub-millisecond verdict that CI L1 cannot give: propose splitting Wait into its own component with its own timing acceptance.」<br>理由：原文的前提「每次都要重跑計時 L2」在這一批的 5.4 之後不成立。 |
| Replacement Criterion 9 | 「A batch that moves a site into engine/os is accepted by CI L1 and the guards alone. If it needs … a ★ declaration or a changed test verdict … it is not a pure move: stop」 | 依 §9 的決定 2：加一句「使用者的 owner 決定可以把凍結的元件帶回來；這樣的批次照它自己的宣告驗收，宣告要寫出 ★ 與 §14.4 的判定改變」。修訂 1 寫的「不變」拿掉 |

**time.md**

| 位置 | 現在 | 改成 |
|---|---|---|
| 開頭的 Owner、Plan 段 | 說 os-wait-backend 凍結，後端留在 `Waiter.cpp` | 改成：平台機制由 `Os::WakeSignal` 提供（`v8-arch-gyo2-os-wait`），Waiter 持有規矩 |
| 依賴圖 | `GYO::Base ◄── GYO::Time` | 加 PRIVATE 的 `GYO::Os` |
| Contracts 的 `Create()` | detail 是平台的錯誤文字 | 改成 3.4 的 ★ 格式 |
| Baselines 的 Windows 列 | 「programs built from `4ffc7c65` (this Waiter)」 | 改成「(the Waiter before v8-arch-gyo2-os-wait)」；其他列不動 |
| Invariant 5 | 「Every backend arms a relative timeout…」 | 改寫成「`Waiter` 交給 `Os::WakeSignal::Block` 的一律是相對逾時，每次醒來重讀時間基準」，**保留**「No absolute time … handed to the OS」這一句。守衛 `tt1-no-recheck` 的 find 跟著改 |
| Invariant 7 | 守衛：「Time Waiter uses the platform backend」守選擇 | 加一句：名字由 Time 的 `WaitBackend` 持有，`Os::WaitMechanism` 值對值對應（`static_assert`）。選擇的守衛改指 `tests/common/os` 的「uses the platform mechanism」；Time 的案例改名為「Time Waiter reports the os mechanism」，守對應（5.3） |
| Invariant 8 | 「When the backends move into engine/os, engine/os reads the latch through a reference and keeps no copy」 | 改成現在式：「engine/os never reads the latch; the wake primitive keeps its own pending signal (os.md Invariant 8).」守衛寫結構上的那一份：`WakeSignal` 的 API 沒有 latch 參數；R5 加上 os 的連結檢查，讓 os 拿不到 `engine/time` |
| Invariant 10 | `GYO::Time` 連結 exactly `GYO::Base` 與 `Threads::Threads` | 改成：exactly `GYO::Base`，加 PRIVATE 的 `GYO::Os`；Os 不得出現在 INTERFACE（照 threads 的檢查方式）。拿掉 `Threads::Threads`（5.2） |
| Replacement Criterion 3 | 「搬進 engine/os 改變了 Wait 計時 L2 的結果：報告並決定」 | 刪掉，改指 os.md RC2。要寫給核准的人看：RC3 在 W3 觸發過（time.md 第 102～103 行），當時以凍結收場，**沒有查明原因**；這次刪掉是因為以 5.4 取代了計時驗收，不是因為原因已查明 |

**其他文件**

- `docs/architecture.md` 的 `GYO::Time` 列：依賴欄從「the operating system (kqueue, Win32 waitable timers, or a condition variable)」改成「Base; Os privately, for the wait primitive」。
- `docs/architecture/target/v2/world-time.zh-Hant.md` 第 17 行寫「只連 Base＋Threads（time.md DI 10）」：這份屬第 4 批（draft #259），這一批不改，交主會話轉給那張卡。
- `docs/architecture/target/v2/time-events-declaration.zh-Hant.md` 第 111 行記著使用者 2026-11-19 的「DI 10 … 都不動」。那個決定的理由是遊戲時間不放進 `engine/time`、time 不連 Kernel；這次改 DI 10 是另一個理由（11-21 的 os 原則），而且 time 不因此連到任何 ecs、orchestration 或 Module。這一點要你明示（§9 的決定 4）。

**CMake**

`engine/core/time/CMakeLists.txt`：
- `target_link_libraries(gyo_time PUBLIC GYO::Base)` 這一行**原樣保留**，另加一行 `target_link_libraries(gyo_time PRIVATE GYO::Os)`。
- 拿掉 `PRIVATE Threads::Threads` 與 `find_package(Threads)`，開頭的註解改成「平台機制經由 GYO::Os」。
- 保留第一行，是為了讓 #260 之後的 `ti-x1`、`os-g13`、`os-g21`（find 都是這一行）不必再改 find（6.3）。

不動 `config/modules.csv`：#261（2acf9cee）起，core、ecs、orchestration 不再登記，引擎內的連結手寫在各自的 CMakeLists，O-6 只管 Modules 與 Plugins；`threads` 的 `PRIVATE GYO::Os` 也是這樣寫的。

### 5.2 §14.4：time.md Invariant 10 的守衛（Design Invariants 的守衛，判定改變要你核准）

`tests/common/time/CMakeLists.txt` 的 `gyo_expect_time_and_thread_links`：改成 `gyo_expect_links(gyo_time GYO::Base GYO::Os)`，並照 `gyo_threads` 的寫法加「GYO::Os 不得在 INTERFACE」的檢查。

1. **守的性質**：time 是 base 之上的葉子，不連 SDL、不連 `GYO::Engine`，也不連別的引擎單位（core 的其他單位、ecs、orchestration、modules）。
2. **放寬面**：
   - 直接的：以前連到 `GYO::Os` 會失敗，之後允許，但只能 PRIVATE。
   - 間接的（審查者指出）：所有連 `GYO::Time` 的 target（audio、world_time、trace、logging）都會經 `$<LINK_ONLY:GYO::Os>` 連進 `gyo_os`；Windows 上還多帶 `shell32`。直接連結集的檢查照樣通過，沒有東西會報。標頭不外露（PRIVATE），所以這些 target 拿不到 os 的 API。
3. **收緊面**：以前允許的 `Threads::Threads`，之後失敗；`GYO::Os` 出現在 INTERFACE 也失敗。
4. **放寬的性質是否仍被守住**：
   - 「不連 SDL、不連其他引擎單位」仍由同一個 exactly 檢查守住，只多了 os 一項。
   - os 本身只連 base 和系統程式庫，由 `tests/common/os/CMakeLists.txt` 的連結檢查（os.md Invariant 3）守住。所以 time 經由 os，也拿不到 SDL 或其他引擎單位。
   - `gyo_time_tests`（`WaiterTests.cpp` 用 `<thread>`）和 `gyo_time_wake_record`（`WakeRecord.cpp` 用執行緒）今天靠 `gyo_time` 間接拿到 pthread。改成這兩個 target 自己直接連 `Threads::Threads`，不靠 `gyo_os` 的 PRIVATE 連結（隱性耦合）。
5. **證據**：動工後、定案條文前，在本機以突變證明：
   - 連一個別的引擎單位會失敗：`ti-x1`，照 #260 合併後的形狀；#260 沒合併時，對新的連結行臨時跑同一個改動當證據，結果寫進 dev_log。
   - 把 os 改成 PUBLIC 會失敗：`ow-x2`。
   - 連 `Threads::Threads` 會失敗：臨時跑一次，結果寫進 dev_log。
   - 由實作者以外的模型（審查者）用實際資料驗證 1～5。

### 5.3 §14.4：time.md Invariant 7 的守衛（選擇的那一半）

`time.md` 點名「Time Waiter uses the platform backend」（`WaiterTests.cpp:39-48`）是 Invariant 7「選擇」的守衛。這一批把依 OS 的期望搬到 `tests/common/os`，Time 的案例改成守對應。

1. **守的性質**：每個平台的 `Waiter` 用該平台預期的機制，並如實回報。
2. **放寬面**：平台選錯機制時，`gyo_time_tests` 不再失敗（它只比 `Waiter` 和 os 的回報是否一致）。
3. **收緊面**：`Waiter` 回報和 os 實際用的機制不一致時，以前不一定失敗（以前沒有 os 這一層），之後失敗。
4. **放寬的性質是否仍被守住**：選擇改由 `tests/common/os/WakeSignalTests.cpp` 的「uses the platform mechanism」守（每個平台一個期望，`tests/common/os` 不在 OS 邊界掃描的範圍）。突變：`ow-x8`（kqueue 回報錯）、`ow-x14`（CMake 在 Apple 選到後備檔）、`ow-x15`（後備回報錯）。Windows 的機制選錯：L1 的 windows-x64 會跑同一個測試，但這一批沒有在 Windows 跑突變證明（§7 的範圍限界）。
5. **證據**：同 5.2 的第 5 點，動工後、定案條文前。

### 5.4 §14.4：以 L1 加守衛取代 D71 的計時驗收（`closeout.md` 決定 2 的 (d)）

1. **守的性質**：搬移不讓晚醒明顯變差（D71：P50 ≤ A + 0.02 ms、P99 ≤ A × 1.25 + 0.05 ms，不早醒，兩側後端相同）。
2. **放寬面**：亞毫秒級的晚醒變差，不再讓這一批不通過。W3 那種 0.023 ms 的位移，這一批量不到，也不判。
3. **收緊面**：
   - 機械式的平台呼叫對照（§7）：正規化後的 diff 存進 evidence，並逐項勾一份記號清單（`NOTE_TRIGGER`、`EV_ADD | EV_CLEAR`、`EV_ADD | EV_ONESHOT`、`NOTE_NSECONDS | NOTE_CRITICAL`、`CreateEventW(nullptr, FALSE, FALSE, …)`、`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`、`TIMER_ALL_ACCESS`、`(+ 99) / 100`、`(+ 9999) / 10000`、`WaitForMultipleObjects(2, …)`）。以前沒有這一關。
   - 行為測試的下界：逾時不早於 10 ms、連續 `Signal` 後下一次 `Block` 仍等到逾時（不空轉），以及對應的突變（6.3）。
4. **放寬的性質是否仍被守住**：只守住一部分。
   - 守得住：不早醒（time.md Invariant 5 的測試）；粗的錯誤（timer 沒 arm、只 arm 1 ns、訊號掉了、空轉）由測試與突變守。
   - 守不住：旗標抄成 0、高解析度 timer 退回一般 timer 時，`Mechanism()` 仍可能回報對的值，L1 看不到，只有記號清單的人工勾選擋。亞毫秒的位移完全不守。
   - 這兩點寫進 §7 的範圍限界與 dev_log。之後由需要它的 Feature 排量測（新的 os.md RC2）。
5. **證據**：W3 的分析（第 2 節）；同形狀在 macOS Intel 12／12 通過、在 Windows 11／12（`HANDOFF.md` 的 w6-l2、D74）；`build/evidence/gyo2-os-wait/w3_stats.txt`（這台 Mac 的主 checkout）。

---

## 6. 測試、守衛與准許清單

### 6.1 測試

| 檔案 | 改什麼 |
|---|---|
| `tests/common/os/WakeSignalTests.cpp`（新） | 從 `756073aa` 的同名檔改寫：<br>· 每個平台用的機制（依 OS 分支，`tests/common/os` 不在掃描範圍；期望用 compile definition 傳入）；<br>· 叫醒前先 `Signal` 的會留著（案例名「a signal sent before the block stays pending」）；<br>· 多次 `Signal` 只結束一次 `Block`：之後的第二次 `Block(20 ms)` 三次中最長的一次至少 10 ms（案例名「a consumed signal does not end the next block」）；<br>· 被另一條執行緒叫醒之後，下一次 `Block(20 ms)` 同樣至少 10 ms；<br>· 逾時會醒（三次中最長的一次至少 10 ms）；<br>· 另一條執行緒在 `Block` 中叫醒；<br>· moved-from 的 `Signal` 什麼都不做，`Block`／`Mechanism` 會 assert；<br>· 競爭測試：掃描放在 waiter 端（`756073aa` 審查 M2 的改法），每一輪開始前先取走上一輪留下的 pending，免得被稀釋（審查者指出的新問題）。 |
| `tests/common/os/CMakeLists.txt` | 加上述測試。另加一個只在非 Windows 平台建置的測試執行檔：只放 `WakeSignalTests.cpp` 和 `src/linux/WakeSignal.cpp`，**不連 `gyo_os`**（只連 `GYO::Base`、os 的 include 目錄與 `Threads::Threads`），避免同一個建置裡有兩份 `WakeSignal::Impl` 的定義被連進同一個執行檔。機制的期望改為 `ConditionVariable`。cv 的突變一律以它為 `target`。 |
| `tests/common/os/OsHeaderHygieneTests.cpp` | include `engine/os/WakeSignal.hpp`（「every engine/os public header is included」會要求，os-g17） |
| `tests/common/time/WaiterTests.cpp` | 「Time Waiter uses the platform backend」改名改寫為「Time Waiter reports the os mechanism」：`waiter.Backend()` 等於 `Os::WakeSignal::Create()` 回報的機制對應到的 `WaitBackend`（5.3）。不分 OS。測試執行檔另外連 `GYO::Os` 與 `Threads::Threads`（測試，不影響 Invariant 10）。區域變數不叫 `signal`（R3 會誤報，`756073aa` 時踩過）。其餘案例不變。 |
| `tests/common/time/CMakeLists.txt` | 5.2 的連結期望；`gyo_time_wake_record` 直接連 `Threads::Threads` |

### 6.2 准許清單（`tests/common/os/os_boundary_allowlist.txt`）

- 刪掉 `engine/core/time/src/Waiter.cpp` 的 R1 3、R2 6、R3 10 三行，以及上面的註解「# Wait primitive backends (os-wait-backend).」。
- 刪掉 `tests/common/time/WaiterTests.cpp R2 2`：依 OS 的期望搬進 `tests/common/os`（6.1）。
- `RoleThreadTests.cpp R2 4` 不動（永久，不在範圍）。
- 守衛本身（`os_boundary_guard.py`）不改：計數變少會被它要求縮小清單，刪掉以後一致。審查者用掃描函式在記憶體裡跑過新路徑的三個檔，0 筆；新的 `src/macos`、`src/linux` 只受 R5／R6 掃描。

### 6.3 突變

規則：
- 只在預期的失敗出現時才算 killed（AGENTS §14.2）；預期的失敗寫成那個案例專用的訊息。
- 執行器加 `-B`、清 `__pycache__`；只跑相關的突變（D184）。
- 每一個加進來的突變，都寫出「只有它抓得到的錯」和它守的條文（§14.5）。

**既有突變的重新定位**

| 突變 | 處理 | 預期的失敗 |
|---|---|---|
| `tt1-notify-dropped`、`tt1-no-recheck`（time-threads-trace 的目錄） | 重新定位 find：`impl_->Signal()` → `impl_->primitive.Signal()`；`impl_->Block(…)` → `impl_->primitive.Block(…)`。其他四個 tt1 的 find 不變。 | 照原本的 expect |
| `os-g6-stale`（os-subsystem 的目錄） | find 指向的 `Waiter.cpp R1 3` 會被刪掉，改指仍在的 `RoleThreadTests.cpp R2 4`（改成 5） | `RoleThreadTests\.cpp R2 has 4 findings, allowlisted 5; shrink the allowlist` |
| `ti-x1-time-links-more` | find 照 #260 合併後的（`target_link_libraries(gyo_time PUBLIC GYO::Base)`，這一批保留這一行）；expect 的 exactly 改成 `GYO::Base;GYO::Os` | `gyo_time links … expected exactly GYO::Base;GYO::Os` |
| `ti-x5`（#260 分支上） | expect 含 `GYO::Base;Threads::Threads`，改成 `GYO::Base;GYO::Os` | 照 #260 的 expect，只換連結集 |
| `os-g13`、`os-g21`（#260 分支上） | find 是 `target_link_libraries(gyo_time PUBLIC GYO::Base)`，這一批保留這一行，**不必改**；動工時確認 | 照 #260 的 expect |
| `ti-x2`、`th-x1`、`os-g8-link` | 打的是 threads、os 的連結行，這一批不改那些行，**不必改**；動工時確認 | — |

**移植 `756073aa` 的 ow-x 系列（備份分支上的目錄，`git show 756073aa:docs/architecture/plans/os-subsystem/scripts/mutations.json`），加新的**

| 突變 | 平台 | 只有它抓得到的錯；守的條文 | 預期的失敗 |
|---|---|---|---|
| ow-x1 cv 在 mutex 外讀或設 pending 旗標 | 後備的測試執行檔（本機 macOS、CI 不跑突變） | 競爭下丟掉叫醒；os.md Invariant 8 | 競爭測試的專用訊息。照 §14.3 宣告：跑 5 次，至少 4 次出現預期訊息才算 killed；不到就是競爭測試的檢出力不夠，停下回報，不改判定 |
| ow-x2 `gyo_time` 以 PUBLIC 連 `GYO::Os` | 全部（configure） | Invariant 10 的新收緊面 | `gyo_time exposes GYO::Os to its users` |
| ow-x3 kqueue 的 `Signal` 不觸發 | darwin | macOS 的訊號掉了 | 「stays pending」案例的訊息 |
| ow-x4 kqueue 的 `Block` 不 arm timer | darwin | 逾時不結束 `Block` | 逾時案例的訊息 |
| ow-x7 `Waiter` 的對應平移兩格 | 全部 | 對應錯；平移兩格是為了讓 Windows 接受兩種 timer 的檢查也失敗（`756073aa` 審查 M1）。time.md Invariant 7 | 「reports the os mechanism」案例的訊息 |
| ow-x8 kqueue 回報 `ConditionVariable` | darwin | macOS 的機制回報錯；5.3 | 「uses the platform mechanism」案例的訊息 |
| ow-x9 kqueue 的 timer 只 arm 1 ns | darwin | `Waiter` 會空轉；Invariant 5 | 逾時下界案例的訊息 |
| ow-x11（新）cv 的 `Signal` 不設旗標 | 後備的測試執行檔 | 確定性的漏叫（和 ow-x1 的競爭不同）；os.md Invariant 8 | 「stays pending」案例的訊息 |
| ow-x12（新）cv 的 `Block` 不清旗標，或只在進入時清 | 後備的測試執行檔 | 空轉（`Waiter` 會吸收掉，`WaiterTests` 全過）；3.1 的「取走即清」 | 「a consumed signal does not end the next block」案例的訊息 |
| ow-x13（新）kqueue 註冊時拿掉 `EV_CLEAR` | darwin | macOS 的空轉；同上 | 同上 |
| ow-x14（新）CMake 在 Apple 選到後備檔 | darwin | 平台選錯；5.3 | 「uses the platform mechanism」案例的訊息（`Kqueue` 的期望） |
| ow-x15（新）後備回報 `Kqueue` | 後備的測試執行檔 | 後備的機制回報錯；5.3 | 同上（`ConditionVariable` 的期望） |

- 不加的：Windows 的 ow-x5、ow-x6、ow-x10，以及 Windows 機制回報錯、event 改成 manual-reset。這一批沒有 Windows 機器跑突變，加了也證明不了 killed。L1 的 windows-x64 會跑同一組行為測試，但「突變證明檢出力」這件事在 Windows 上沒做，寫進 §7 的範圍限界。
- 可選、這一批不加：`Notify` 先 `Signal` 再設 latch（3.1 已記）。
- **和 #260 的協調（主會話 2026-11-21 轉告）**：#260 那張卡正在改寫 os-subsystem 目錄的 `ti-x1`、`ti-x2`、`th-x1`、`os-g8-link` 等，改成打 core 各 CMakeLists 裡手寫的連結。兩張卡不改同一處，所以 #260 合併之前，這一批不碰 `docs/architecture/plans/os-subsystem/scripts/mutations.json`。#260 合併後用 merge 跟上，再一次做完上面兩張表裡 os-subsystem 目錄的部分。
- 審查者在審查裡找「改了實作，測試卻不失敗」的地方，找到的照 §14.2 加進目錄。

---

## 7. 驗收（看結果之前宣告）

照使用者的決定 (a)，以 5.4 取代 D71 的計時驗收：L1 加守衛加突變。

| 項目 | 判定 | 誰跑 |
|---|---|---|
| macOS 本機全量 ctest | 全部通過（含 `os.boundary_guard`、`os.header_hygiene`、configure 時的連結檢查、`gyo_os_tests`、`gyo_time_tests`、後備的測試執行檔） | 我（macOS Intel） |
| L1 三平台（linux-x64、windows-x64、macos-arm64） | `L1 /` 開頭的三個 job 都是 success。L1 跑 `cpu\|shader` 標籤；`gyo_os_tests`、`gyo_time_tests`、`os.boundary_guard` 都標 `cpu`，新的測試也標 `cpu`。 | CI（推送後手動派發；draft 的推送 L1 會 skipped） |
| 平台呼叫的機械式對照 | 每個平台各一份：舊 `Waiter.cpp` 的平台分支 vs. 新 `src/<平台>/WakeSignal.cpp`，正規化（空白、型別名）後的 diff 存進 `build/evidence/gyo2-os-wait/`（這台 Mac 的主 checkout）。差異只能是：參數型別、錯誤的建構（3.4 的 ★）、cv 後備的 pending 旗標（3.1）。5.4 的記號清單逐項勾，勾選表放進 dev_log。 | 我；審查者覆核 |
| 突變 | 6.3 的相關突變全部 killed（預期的失敗；ow-x1 照它自己宣告的 5 次 4 次） | 我（本機），dev_log 記收支 |
| 審查 | 實作者 Opus 5.5／high，審查者 Fable／high（不同模型，唯讀；兼 5.2～5.4 的驗證者，以及新突變清單的敵對式檢查） | — |

- **Windows 實機計時：本批不做**（使用者的決定 (a)）。
- **範圍的限界**（寫進 dev_log，影響後續的批次照 AGENTS §14 開頭寫進它們的事前宣告）：
  - macOS Intel：同一個形狀量過，12／12 通過（w6-l2）。
  - Windows：同一個形狀量過（W3），window deadline P50 +0.023 ms，超出當時邊界 0.0032 ms，**不通過照原樣保留，原因未查明**。
  - Linux：晚醒從來沒有量過（D78 取消）。
  - macOS arm64：從來沒有量過（沒有機器）。
  - Windows 的機制選錯、旗標抄錯：L1 抓不到，只靠 5.4 的記號清單；Windows 上沒有跑突變。
  - window 情境在 gyo2 上無法量（沒有 SDL）。
  - 之後在兩種情況下，由那個 Feature 另行宣告量測：SDL 那批進來時要 window 的基準，或 Client／Match 的節奏有產品壓力時（新的 os.md RC2）。這次的 W3 分析可以當起點：先跑 Windows A/A，再推門檻，並記錄 DLL 的雜湊、交替 A／B 的順序。
- **件數の突き合わせ**：這一批沒有計測，所以不適用。之後若使用者要求加跑「只記錄」的 Windows 晚醒，那一份報告以「件數の突き合わせ：…」那行開頭（§14 冒頭），並列出計畫次數、跑完次數、無效次數。
- 實機腳本：這一批沒有實機場次，不寫腳本。
- 停止條件：照 AGENTS §14 開頭。本宣告已列的判定改變（5.2 的 Invariant 10、5.3 的 Invariant 7、6.1 的 `WaiterTests` 改寫）以外，任何既有測試的判定要改時停下回報。

---

## 8. 批次、檔位與模型

上段（理由：動到 Design Invariants 與 os 的公開介面；基盤 Architecture）。

```text
v8-arch-gyo2-os-wait
├── 宣告          high ｜本檔；敵對式檢查 Fable／high（唯讀，修訂 2 已處理）→ 使用者核准動工
├── os-wake-signal high ｜core/os：介面、三個 src/<平台>/、WakeSignalTests、後備的測試執行檔、hygiene
├── time-calls-os high ｜core/time 改呼叫 os；time 的 CMakeLists 與連結期望；Waiter 測試；准許清單；tt1 的重新定位
├── mutants       high ｜#260 合併後 merge 跟上；os-subsystem 目錄的重新定位與 ow-x 系列；本機跑相關突變
├── docs          medium｜os.md、time.md、architecture.md 條文（5.1）、機械式對照、dev_log、v2 README 的列
└── review        high ｜Fable／high 審查＋5.2～5.4 的驗證 → 條文定案 → L1 三平台 → 合併前核准（要你明確說「合併」）
```

- 實作者：Opus 5.5（本會話）／high。審查者與敵對式檢查：Fable／high，唯讀，和實作者不同模型（navigator：上段的對抗要換模型）。
- 估時：實作約 4～5 小時（修訂 2 多了移植的突變與機械式對照）；審查與修正約 1～2 小時；加上 L1 與 #260 的等待。超過 1.5 倍就停下回報。
- 一個 PR 向 `gyo2`，標題 `refactor(os): core/time の待機を core/os の WakeSignal に移す（v8-arch-gyo2-os-wait）`。commit 用日語。
- #260 沒合併時：mutants 那一步等它；其他步驟照做，5.2 的證據照 5.2 第 5 點臨時跑。

---

## 9. 要你核准的

兩段式：現在核准動工；突變與審查者的驗證做完後，再把 5.1 的條文文字交你定案（同 D71／D72）。

1. **整份宣告**，含第 3 節的介面形狀與退掉的選項、第 4 節的檔案配置（含 `src/linux` 當 `else()`、RC7 接受重複）、第 7 節的驗收與範圍限界。
2. **這一批的根據是你 11-21 的 owner 決定，不是 os.md RC9 的純搬移。** RC9 的原文說：需要 ★ 或改測試判定的就不是純搬移，要停下。這一批有 ★（3.4）、三份 §14.4 的判定改變（5.2～5.4）、os.md 與 time.md 的條文改寫，以及 cv 後備的新邏輯。要你明示：
   - 這一批照你的 owner 決定進行，上面幾樣都在本宣告裡交你核准；
   - os.md 的 RC9 與 Components 表下的說明加一句「使用者的 owner 決定可以把凍結的元件帶回來」（5.1）。
3. **§14.4 的三份**：5.2（Invariant 10，有放寬面，含間接的）、5.3（Invariant 7，有放寬面）、5.4（以 L1 取代 D71 的計時驗收，有放寬面，亞毫秒的位移不再守）。
4. **time.md DI 10 的修改**：你 11-19 說過 DI 10 不動（理由是 time 不連 Kernel）。這次為 11-21 的 os 原則改它，time 只多連 os，不連 ecs、orchestration 或 Module。
5. **★ 錯誤細節**（3.4）。

---

## 10. 範圍外，列為後續

- `engine/core/os` 裡仍有三種形狀並存：`src/thread/Thread.cpp`（一檔三支）、`src/posix/Process.cpp`、`src/posix/Files.cpp`（POSIX 檔裡用 `#if` 分 macOS／Linux），加上這一批的 `src/macos`＋`src/linux`。照使用者 11-21 的 `src/<平台>/` 規則，要另開一批整理。
- os.md、time.md 裡多處仍寫 `engine/os`、`engine/time`、`engine/platform/sdl` 等舊路徑與 gyo2 已刪的元件（依賴圖、Out of scope）。這一批只改 Wait 相關的句子；整體的路徑同步交給圖書管理員或另一批。
- `docs/architecture/target/v2/world-time.zh-Hant.md` 第 17 行的「只連 Base＋Threads」：交第 4 批（draft #259）。
- `tests/common/threads/RoleThreadTests.cpp R2 4` 仍是永久條目。
- SDL 那批進來時，`gyo_time_wake_record_window` 與 Windows 的 window 晚醒基準要不要重建，由那批決定（2.3）。
- os.md 的 Replacement Criterion 7 在 `src/<平台>/` 的規則下要重讀一次，看措辭是否仍合用。這是規則的整理，交給風紀委員。

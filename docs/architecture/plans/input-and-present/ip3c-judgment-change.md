# ip3c 判定變更提案（AGENTS §14.4 的 1～5，第 3 版，D34；獨立核對 3 輪後定稿）

- 狀態：**已核准**（D35，2026-10-22）。

對象：事前宣告 `ip3-declaration.md` 第 2 節 ip3c 的整合判定「`p0 − β/2 ≤ ts ≤ p1 + β/2`」（測試 `A pumped event carries the time it was pushed`，`tests/common/platform_sdl/EventTimeTests.cpp`）。這個測試殺掉 m2-int、m3-int，是 `platform-sdl.md` Design Invariant 3 的守衛 Test，所以不論有沒有放寬面都要使用者核准。

新判定：push 之前由測試自己取一組配對 `P0 = SampleClockPair()`；pump 內讀該次 pump 的配對 `P1`（`CurrentClockPairForTesting`）。

- `drift = (P1.engine − P0.engine) − (P1.sdl − P0.sdl)`
- `τ = |drift| + P0.bracket + P1.bracket + 2 ns`
- 斷言 `p0 − τ ≤ ts ≤ p1 + τ`；「`ts ≤ 交付時刻`」與 5 ms 等待不變。

同時新增（只收緊，不需核准，但影響第 2、4 點的評估）：

- `SampleClockPairWith reads Engine then SDL then Engine and takes the midpoint`（D34 的接縫）：取樣函式的讀取改由參數傳入（`ClockReaders`），正式路徑 `SampleClockPair()` 傳真實時鐘；測試用假讀取，以精確等式檢查讀取順序 ESE、engine＝自己兩次讀取的中點、sdl＝自己的讀取、bracket＝自己兩次讀取的間隔。
- `MakeClockPair puts the engine time at the midpoint of its two reads`：配對組成的算術，精確等式。
- `SampleClockPair reads both clocks between the caller's reads`：100 輪，正式取樣的 engine 與 sdl 都落在呼叫者前後兩次讀取之間（抓單位與時域錯誤，例如 SDL 用 µs）。

## 1. 這個判定守的性質

pump 交出的事件時間等於事件被 push 的 Engine 時間；容許的誤差是配對的量測誤差，加上兩個時鐘在 push→pump 期間的走差（後者是 `platform-sdl.md` 時間戳契約的 Known limits，不是配對本身的誤差）。也就是：換算用了正確的配對、正確的正負號、沒有把 delta 歸零。

## 2. 放寬的面

舊判定失敗、新判定通過的情況：`ts` 落在 `[p0 − τ, p0 − ⌊β1/2⌋)` 或 `(p1 + ⌊β1/2⌋, p1 + τ]`。

- 正確實作會落在這裡的情況（依據：原始碼與模擬，**沒有實機量測**；本機 Intel 200 次中，舊判定失敗 0 次，落在放寬區間 0 筆）：
  - Linux：Engine 讀 `CLOCK_MONOTONIC`（受 NTP 調頻），SDL 讀 `CLOCK_MONOTONIC_RAW`。走差從 0（沒有 NTP 校頻）到數 µs，依 NTP 的頻率修正與等待長度而定；契約記為約 500 ppm。模擬：舊判定在 10／50／500 ppm 下約 16／43／57% 失敗，新判定 20 萬次 0 失敗。
  - Apple Silicon：兩邊都讀同一個 24 MHz 計數器，速率差 0；計數換成 ns 的取整與 β/2 的整數除法，使正確實作超出 β/2 達 1～2 ns。單看這一點只需要 +2 ns。
- 錯誤實作會落在這裡的情況：
  - 換算或取樣錯誤、誤差在 τ 以內者。本機 Intel 實測 τ 中位數約 1.25 µs、最大約 2.8 µs（200 次），約為舊容許 β/2 的 3.3 倍。
  - τ 由被測的 `SampleClockPair` 算出，有自我參照：取樣出錯時 τ 會跟著變大。前兩輪核對確認的錯誤（取樣用 e0 而非中點、偏 1 µs、SDL 用 µs、`SampleClockPair` 內部繞過中點、重複傳同一次讀取、讀取順序錯）在新判定下多數不會或不一定會被本判定殺掉（SDL 用 µs 仍被殺，但餘裕只剩數十 µs；`SampleClockPair` 傳兩次第一次讀取的錯誤 7／40 機率性）；它們現在都由上面新增的精確等式與「落在呼叫者讀取之間」測試確定殺掉（第 4 點）。其中「傳兩次第二次讀取」在舊判定下本來也殺不掉（0／40），不屬於這次的放寬。

## 3. 收緊的面

無。`τ ≥ β1 + 2 ns > ⌊β1/2⌋`（β1 = 0 時也成立），所以舊判定通過的輸入，新判定都通過。目標測試內，宣告版與提案版的差異只有三處（檔案另外新增 `#include <string>` 與 3 個只收緊的測試）：push 前取 `P0`、診斷用的 MESSAGE 行、上下界兩條斷言。

## 4. 放寬之後，原本的性質由哪些判定繼續守住

- 換算公式：純函式的精確等式 3 個（誤差 0）。m2-pure、m3-pure、m4-pure、m-zero 殺掉。
- 每次 pump 用自己的配對、巢狀還原：整合的精確等式 2 個（以讀出的配對計算，誤差 0）。m1、m-fixed、m-nest、m-nest-reuse 殺掉。
- 配對取樣：
  - `MakeClockPair` 的算術（精確等式）：x-edge、x-offset 殺掉。
  - `SampleClockPairWith` 的讀取順序與傳遞（精確等式，假讀取）：s-edge-bypass、s-before、s-after、x-order 殺掉。
  - 正式時鐘的單位與時域（落在呼叫者讀取之間）：x-us 殺掉。
- 本判定在新容許下仍殺掉 m2-int、m3-int（誤差約 5 ms，遠大於 τ）。
- 仍守不住的：正式路徑 `SampleClockPair()` 傳給 `SampleClockPairWith` 的兩個讀取函式本身（例如把 Engine 讀取換成另一個時鐘）。這一層只剩兩行 lambda，由「落在呼叫者讀取之間」守住時域與單位的大錯；偏差在一個 bracket 以內的錯誤守不住（核對者實測：Engine 讀取 −20 ns、SDL 讀取 +50 ns 都 0／10；SDL 讀取 +300 ns 10／10 被殺）。這一層在舊判定下同樣守不住（例：Engine 讀取 −100 ns 舊判定 0／40），不是這次放寬造成的。

突變目錄 22 個，以提案版測試全部執行：22／22 killed。第一次執行時 4 個取樣類突變顯示存活，原因是測試名稱含逗號，doctest 的 `-tc` 把逗號當分隔，篩選對到 0 個測試；改名後重跑 4／4 killed，並確認目錄中每個篩選都剛好對到 1 個測試。

## 5. 根據與證據

- Linux 兩時鐘的來源：`build/target/_build/test/_deps/sdl3-src/src/timer/unix/SDL_systimer.c:54-59`；Engine：`engine/time/include/engine/time/MonotonicClock.hpp`；契約的 Known limits：`docs/architecture/platform-sdl.md`。
- Apple 的 24 MHz 與 ns 換算：`SDL_systimer.c:121-133`、`SDL_timer.c:600-602`、`:624-636`；push 時由 SDL 補時間：`SDL_events.c:1802-1803`。
- 本機 Intel 200 次的量測、xhigh 審查的模擬 `sim.py`、核對者的 `verify/simlinux.py`、`verify/explore*.py`：都在本會話的 scratchpad（暫存，不在 repo）。核准後，數據摘要寫進 IP 的 HANDOFF。（2026-10-31 補註：這裡的 scratchpad 內容已經不在，未保留）
- 突變：`docs/architecture/plans/input-and-present/scripts/mutations.json`（22 個）；執行紀錄 `scratchpad/mutants-v3.log`。（2026-10-31 補註：這裡的 scratchpad 內容已經不在，未保留）

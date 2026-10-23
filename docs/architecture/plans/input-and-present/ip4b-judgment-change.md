# ip4b 判定變更提案（AGENTS §14.4 的 1～5，第 2 版）

- 狀態：**核准**（D38，2026-10-22，方案 B）。第 1 版經獨立核對，修正量測方法（不加 `-s`）、m22 的歸屬、「仍守不住」的範圍，並採納核對者提出的精確等式（方案 B）。
- 對象：[ip4-declaration.md](ip4-declaration.md) 第 2 節 ip4b 的時間戳判定。宣告的寫法是：經 Native push 一個 `timestamp = SDL_GetTicksNS() − 10 ms` 的本視窗事件，`timestamp` 在 `observedAt − 10 ms ± τ` 之內。
  - 測試：`tests/common/platform_sdl/EventWatchTests.cpp` 的 `An event watch stamps each event with its queue time`（函式 `CheckPastEventTime`）。
  - 這個測試守 [platform-sdl.md](../../platform-sdl.md) 的 Design Invariant 3（每次回呼取自己的配對），所以不論有沒有放寬面，都要使用者核准。
- 經過：
  - ip4b 實作時，在執行任何測試之前就改了判定（第 4 節補記）。
  - xhigh 審查（F4）指出這個更正有放寬面。
  - 本機量測與獨立核對，確認宣告版對正確的實作幾乎每次都失敗。

## 記號

- `P0`：測試在 push 之前自己取的配對。事件以 `t = P0.sdl − 10 ms` 推入。
- `after`：push 返回後，測試讀的 Engine 時間。
- `W`：回呼取的配對，由唯讀接縫 `LastWatchClockPairForTesting` 讀出。
- `τ = |(W.engine − P0.engine) − (W.sdl − P0.sdl)| + P0.bracket + W.bracket + 2 ns`，即 ip3c 的 τ（D35）。

## 判定

| | 判定 |
|---|---|
| 宣告版 | `observedAt − 10 ms − τ ≤ ts ≤ observedAt − 10 ms + τ` |
| 方案 A（第 1 版） | `P0.engine − 10 ms − τ ≤ ts ≤ after − 10 ms + τ` |
| 方案 B（建議） | 方案 A 的區間，再加兩條精確等式：`observedAt == W.engine + (W.bracket − W.bracket/2)`（回呼配對的第二次 Engine 讀取）與 `ts == ConvertSdlTimestamp(t, W, observedAt)` |

兩個方案都另外新增下面幾條斷言，都只收緊：

- `W.sdl` 落在 `[P0.sdl, push 後的 SDL 讀取]`；
- `W.engine` 落在 `[P0.engine, after]`；
- `observedAt` 落在 `[P0.engine, after]`；
- 兩次回呼的配對不同（新配對）；
- `ts ≤ observedAt`：這條被上界蘊含，獨立殺不掉任何突變，保留作為文件。

方案 B 的精確等式沿用 ip3c 的前例：`EventTimeTests.cpp` 的 `Each pump converts with its own clock pair`，在 pump 路徑上同樣以讀出的配對做精確等式。

採用方案 B（D38）。

## 1. 這個判定守的性質

watch 交出的事件時間，等於事件排入時的 Engine 時間。也就是說：

- 回呼用自己剛取的配對換算；
- 換算的正負號正確；
- 沒有把差值歸零，也沒有改用觀測時間；
- 觀測時間是回呼配對的第二次 Engine 讀取。

## 2. 放寬的面

**宣告版的錯誤。** 宣告版以 `observedAt` 為基準，但事件的真實排入時間約是 `P0.engine − 10 ms`。兩者相差 `observedAt − P0.engine`，這段時間包括：

- push 進入 watch 之前的耗時；
- 測試自己的事件建構。

這個差距大於 τ，所以正確的實作幾乎每次都失敗。核對者以 500 個樣本代數驗證，每一個都滿足 `誤差 = 走差 − (observedAt − P0.engine)`，誤差為 0。這表示失敗的原因是基準點，不是實作。

**方案 A 的放寬面。** 在 `P0.engine ≤ observedAt ≤ after` 時，方案 A 比宣告版多放過兩段：

- `[P0.engine − 10 ms − τ, observedAt − 10 ms − τ)`；
- `(observedAt − 10 ms + τ, after − 10 ms + τ]`。

這個前提由程式順序保證：observedAt 是回呼內的讀取，回呼在 push 之內、同一執行緒上同步執行（`SDL_events.c:1800-1815`），而且 Engine 時鐘是 `steady_clock`。

放寬面不對稱。正確的實作落在下界附近，所以：

- 「早」的錯誤：餘裕只有 τ 加走差（中位數 1.2 µs，最小 0.18 µs），超過就被殺。
- 「晚」的錯誤：要到約 2.7～5 µs（觀測到 `after` 的時間加 τ）才被殺。

核對者的突變中，在方案 A 下存活的有：

| 突變 | 內容 |
|---|---|
| v2 | 換算改以 e1 為基準 |
| v3 | 換算改用另讀的 SDL 時間 |
| v4 | 差值截成 µs |
| v7 | 記錄新配對，換算卻用第一個配對 |
| v8 | `observedAt` 改成中點 |
| v10 | `t + 2 µs` |
| v11 | `observedAt + 500 ns` |

**方案 B 的放寬面。** 兩條精確等式只放過一種輸入：`ts` 與 `observedAt` 恰好等於「以回呼自己記錄的配對換算」的結果。

- 正確的實作都在這裡面：100 次執行，0 失敗。
- 上面 7 個存活的突變全部殺掉：核對者 20～30 次中全數殺掉；本目錄收錄其中 4 個，見第 4 點。

剩下的放寬，只是「記錄的配對本身取錯」這一類。這一層由取樣的守衛處理，見第 4 點。

## 3. 收緊的面

- 主判定的區間在第 2 點的前提下包含宣告版，沒有收緊。
- 前提不成立的輸入（observedAt 在 push 之外），由新增的「`observedAt` 落在 push 之內」收緊。
- 方案 B 的精確等式是收緊：凡是不以自己的配對換算的實作，都會失敗。

收緊的斷言與殺掉的突變（目錄 `scripts/mutations.json`）：

| 斷言 | 殺掉的突變 |
|---|---|
| 上界 | m20 |
| SDL 時間的範圍 | m-watch-sdl |
| 新配對、兩個範圍、下界、`observedAt` 的範圍 | m21（五條都會殺） |
| `observedAt` 落在 push 之內 | m-watch-engine |
| 方案 B：`observedAt` 的精確等式 | m-watch-mid |
| 方案 B：換算的精確等式 | m-watch-first、m-watch-late、m-watch-e1 |

m22 只由宣告版本來就有的 future 測試殺掉：過去的事件換算後本來就在上限之下，改上限不影響。

## 4. 放寬之後，原本的性質由哪些判定繼續守住

- **換算公式**：ip3c 的純函式精確等式。殺掉的突變：m2-pure、m3-pure、m4-pure、m-zero。
- **取樣**：ip3c 的 `SampleClockPairWith` 精確等式，檢查讀取順序、中點與 bracket。watch 呼叫同一個 `SampleClockPair()`。
- **記錄的配對確實是這次回呼取的**：新配對，以及 SDL／Engine 兩個範圍斷言。
- **換算用的就是記錄的配對、觀測時間就是它的第二次讀取**：
  - 方案 B：兩條精確等式守住。
  - 方案 A：守不住，v7、v8、v11 存活。
- **仍守不住的**（兩個方案都一樣）：
  - 正式取樣路徑的兩個讀取函式，在一個 bracket 以內的偏差（ip3c 已知缺口，`2026_10_22_engine_ip3.zh-Hant.md`）。
  - Engine 讀取與 SDL 讀取同時偏同一方向、而且落在測試讀取之間的錯誤。

## 5. 根據與證據

**量測**（本機 Intel，正確實作，不加 `-s`；200 次執行，每次 2 個樣本）：

| 量 | 中位數 | 最大 |
|---|---|---|
| `observedAt − P0.engine` | 2.7 µs | 9.9 µs |
| `after − observedAt` | 1.5 µs | 23 µs |
| `after − P0.engine` | 4.0 µs | 31 µs |
| τ | 0.91 µs | 4.4 µs |
| 宣告版失敗 | 400／400 | |

- 第 1 版用了 `-s`。doctest 在 `after` 之前輸出成功的 REQUIRE，使後半段約大 10 倍；CTest 不加 `-s`（`tests/common/platform_sdl/CMakeLists.txt`）。
- 第 1 版的「最大約 117 µs」是兩個不同樣本的最大值相加，作廢。
- 核對者合計 1,700 個樣本，宣告版失敗 1,697 個。通過的 3 個，是 τ 被搶占撐大到約 31～51 µs 的樣本。

**突變**：

- 目錄共 73 個。
- 以方案 A 跑整個目錄時是 67／69：m-depth 的 find 過時；m-watch-engine 存活，補上「`observedAt` 落在 push 之內」後殺掉。
- 方案 B 加入 4 個突變，與時間戳相關的 9 個重跑，9／9 殺掉。
- m-watch-first（兩個時鐘不漂移時，第一個配對與新配對只差奈秒級）另外連跑 30 次，30／30 偵測到。

**獨立核對**：另一位審查者讀程式、自行量測，並寫了 11 個突變。結論：

| 點 | 結論 |
|---|---|
| 1 | 成立 |
| 2 | 部分成立 |
| 3 | 部分成立 |
| 4 | 部分成立 |
| 5 | 部分成立 |

指出的修正已在本版處理：

- 量測方法；
- m22 的歸屬；
- 「傳錯配對已守住」只對記錄下來的配對成立；
- 守不住的程式行有兩行，不是一行；
- 放寬面不對稱；
- 「舊判定可以殺 m-watch-engine」沒有效力，因為舊判定連正確的實作都拒絕。

**紀錄位置**：

- 本會話的 scratchpad：`ip4b-measure.py`、`ip4b-measure.log`、`mutants-ip4b.log`；
- 核對者的紀錄：`verify-ip4b/`。

**另外回報（不屬於本判定）**：`platform-sdl.md` DI 3 的守衛清單原本只列 pump 路徑（m1～m4、m-zero）。補上 watch 路徑屬於 Design Invariants 節的變更，已與本判定一起核准（D38）。

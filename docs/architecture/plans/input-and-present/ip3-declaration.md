# IP-3 事前宣告（ip3a）

- 狀態：**已核准**（D32，2026-10-21）。核准後，本文件的判定、突變與試行規則在 ip3b～ip3f 不再放寬（AGENTS §14.1）。
- 依據：[PLAN](PLAN.md) 文末的 IP-3／IP-4（D31）；設計的正式條文在 [platform-sdl.md](../../platform-sdl.md)、[input.md](../../input.md)。
- file:line 以 master `6fbb6a9` 為準。

## 1. 簽章定案時的更正

xhigh 審查的 F14 要求定義 `Sync()` 的 Err 與剪貼簿為空時的回傳。查 SDL 3.4.0 的原始碼後，兩者改成不回傳 `Result`：

| API | 草案 | 定案 | 根據 |
|---|---|---|---|
| `SdlPlatform::ClipboardText()` | `Result<std::string, SdlPlatformError>`，空回 `Ok("")` | `std::string`；沒有文字與失敗都回空字串 | SDL 失敗時也回空字串，分不出兩者（`SDL_clipboard.h:109-112`） |
| `SdlPlatform::Sync()`（IP-4） | `Result<bool, SdlPlatformError>` | `bool`；false＝逾時或失敗 | `SDL_SyncWindow` 只回 true／false（`SDL_video.c:3568-3574`） |

新增的錯誤碼：

| 列舉 | 新值 | 子批 |
|---|---|---|
| `SdlPlatformErrorCode` | `BaseDirectoryUnavailable` | ip3e |
| `SdlPlatformErrorCode` | `WindowQueryFailed`、`WindowOperationFailed`、`EventWatchFailed` | IP-4 |
| `SdlInputErrorCode` | `TextInputFailed` | ip3d |
| `SdlInputErrorCode` | `InjectionFailed` | IP-4 |

其餘簽章照 PLAN 的草圖與 [platform-sdl.md](../../platform-sdl.md)。

## 2. 判定

### ip3b：標頭不含 SDL

- G1：衛生 TU 只 include 所有非 Native 的後端公開標頭；`SDL_stdinc_h_`、`SDL_h_`、`SDL_platform_defines_h_` 任一被定義就 `#error`。先在現狀上確認失敗（Red），再改到通過（Green）。
- G2：去掉註解後，掃 `engine/**/include`（排除 `native/`）中的 `\bSDL_\w+`、`\bSDLK_\w+`、`\bSDLCALL\b`、`<SDL3/`。allowlist 只減不增；本子批結束時為空。先 Red 後 Green。
- IP-1 既有測試（`tests/common/input`）的判定不變；只做機械改寫（include、呼叫形式）。
- 同機兩樹的權威 digest 相同。

### ip3c：時間戳

- 純函式 `ConvertSdlTimestamp`：
  - `t < s`：結果 `engine − (s − t)`；`t > s`：`engine + (t − s)`，再夾到上限；
  - 上限之後的值等於上限；
  - `t == 0` 回上限（交付時刻）。
- 整合（dummy driver）：
  1. `p0=Now()`；`SDL_PushEvent`（timestamp 0，由 SDL 補上）；`p1=Now()`。
  2. `Waiter` 等 5 ms。
  3. pump；β 由 `SampleClockPairForTesting()` 讀出。
  4. 斷言 `p0 − β/2 ≤ ts ≤ p1 + β/2`；另一條斷言 `ts ≤ 交付時刻`。
- 假配對在兩次 pump 之間跳 10 s，第二次 pump 的結果跟著跳 10 s（容許 ±β）。
- 同一次 pump 內，SDL 時間不遞減的事件，結果不遞減。
- 判定不用常數門檻：上下界都來自測試自己讀到的時間。

### ip3d：文字輸入

- 文字輸入關閉時，`text` 恆為空；開啟期間才有內容。
- `Kind::Key` 只記按下（含 repeat），放開不記；repeat 不進 `events`。
- `text` 內的文字與按鍵依到達順序。
- `layoutKey`：以 Native 構造 `scancode=Q, key='a'` 的事件，`key==Key::Q`、`layoutKey==Key::A`。
- 剪貼簿：dummy driver 下先 `SDL_SetClipboardText` 再 `ClipboardText()`，相同。若 dummy driver 不支援，改成只在 L3 驗證，並寫明理由（不是放寬：本項原本就沒有 L1）。

### ip3e：進入點、執行檔目錄、日誌橋

- 等級對照：八個 SDL 等級各推一行，Base 收到的等級照表。
- tee 模式：Base 收到推入數；原本的輸出收到推入數。只轉送模式：Base 收到推入數；原本的輸出收到 0。
- 重入：Base handler 在同一執行緒呼叫 `SDL_LogMessage` 一次，原本的輸出恰好收到那 1 行，Base 不再收到它。
- 解構後 `SDL_GetLogOutputFunction` 回到建立前的函式；解構時若目前輸出不是橋，`GYO_ASSERT` 觸發（death test 或 assertion handler 計數）。
- `BaseDirectory()` 與 `SDL_GetBasePath()` 相同；含非 ASCII 的 UTF-8 字串轉成 path 再轉回 UTF-8，位元組相同（四平台 CI，含 windows）。
- `tests/common/runtime_sdl` 改用 `GyoMain`，CI 四平台的結束碼符合預期；`GyoMain` 丟例外時結束碼為 1。

## 3. 機率性測試的試行規則

| 測試 | 一次執行的內容 | 判定 | 重試 |
|---|---|---|---|
| ip3e 並行日誌橋 | 8 條執行緒持續 `SDL_Log`；主執行緒建立、解構橋 200 次 | 每一輪的行數守恆（每條執行緒推入數＝Base 收到數＋橋不存在時原輸出收到數）；整個測試 60 秒內結束（沒有死鎖） | 不重試：一次失敗就是失敗，照 §14.1 先切分原因 |
| ip4b 並行解構（IP-4，先宣告） | 另一條執行緒持續 `SDL_PushEvent`；主執行緒建立、解構 `EventWatch` 200 次 | 解構返回之後，回呼計數不再增加；60 秒內結束 | 同上 |

CI 上的試行就是每次 L1 跑一次；不另外重複。

## 4. 突變

執行工具沿用 `time-threads-trace/scripts/run_mutations.py`，目錄 `input-and-present/scripts/mutations.json`（ip3b 建立；`find` 字串在對應的程式寫好後才能定，判定與預期訊息現在定）。只有「檢查失敗，而且輸出符合預期訊息」才算殺掉。

| id | 子批 | 改壞的是什麼 | 由哪個判定殺掉 |
|---|---|---|---|
| m6a | ip3b | 在 `SdlPlatform.hpp` 放回 `#include <SDL3/SDL.h>` | G1 與 G2 都要失敗（兩次執行，各自的訊息） |
| m6b | ip3b | 在 `SdlInput.hpp` 放回 `#include <SDL3/SDL_events.h>` | G1 與 G2 |
| m1 | ip3c | 配對只在 `Create` 取一次 | 整合：假配對跳 10 s 的斷言 |
| m2 | ip3c | 負的 delta 歸零 | 純函式 `t < s`；整合的下界 |
| m3 | ip3c | delta 的正負號反轉 | 純函式；整合的上界 |
| m4 | ip3c | 拿掉上限 | 純函式「上限之後等於上限」；整合「`ts ≤ 交付時刻`」（合成未來的 SDL 時間） |
| m-zero | ip3c | `t == 0` 不特判 | 純函式 `t == 0` |
| m7 | ip3d | 文字輸入關閉時仍收集 `text` | 關閉時 `text` 為空 |
| m8 | ip3d | repeat 進 `events` | IP-1 的 repeat 測試與 ip3d 的 repeat 斷言 |
| m9 | ip3d | `text` 的順序反轉 | 到達順序的斷言 |
| m-up | ip3d | 放開也記進 `text` | 只記按下的斷言 |
| m-layout | ip3d | `layoutKey = key` | `scancode=Q, key='a'` 的斷言 |
| m10 | ip3e | 拿掉 thread_local 的防重入 | 重入「恰好 1 行」 |
| m11 | ip3e | 解構時不還原 | 解構後的輸出函式 |
| m12 | ip3e | `main` 丟掉 `GyoMain` 的回傳值 | runtime_sdl 的結束碼 |
| m-throw | ip3e | `main` 不攔例外 | `GyoMain` 丟例外時結束碼 1 |

IP-4 的 m13～m19 在 ip4a 開始前、介面審查（U10）之後宣告。

## 5. 消費端日誌契約的核對（PLAN ip3a 的最後一項）

產品的日誌改走 Base handler 之後，目前經 SDL 寫進日誌檔的行會換路徑。核對結果（依 D12 只寫摘要）：

- 提出需求的消費端的 A 級日誌行中，**只有 1 個行族**經 SDL 寫出（受擊回饋行，寫出端是 `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:377` 的 `SDL_Log`）。它的釘住測試從寫出端的原始碼讀鍵序列（`tests/object_fps_pvp/fixtures/log_v1/keys-v1.tsv:28`），目前沒有工具解析它。
- 其餘 A 級的 Client 行（worker statistics、role started、clock sync）走 `std::clog`，不經 SDL，不受影響。
- 經 SDL 的其他行都是 B 級。
- 結論：ip3e 的停止條件「日誌行觸及消費端日誌契約的 A 級」**會成立**。處理方式交消費端決定，寫在消費端的計畫；Engine 這一側沒有要改的。

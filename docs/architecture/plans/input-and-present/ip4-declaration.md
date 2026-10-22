# IP-4 事前宣告

- 狀態：宣告（2026-10-22，D37 之後；使用者：「其餘意見照你的處理，宣告後做 ip4a。」）。宣告後，這裡的判定、突變與試行規則不再放寬（AGENTS §14.1）。
- 依據：[PLAN](PLAN.md) 的 IP-4 節（D37 修正後的草圖）、[platform-sdl.md](../../platform-sdl.md) 的 Design Invariants 2、4、6、7、介面審查（U10）提出的突變。
- 執行：`time-threads-trace/scripts/run_mutations.py --catalog input-and-present/scripts/mutations.json`，環境 `SDL_VIDEODRIVER=dummy`。只有「失敗而且輸出符合預期訊息」才算殺掉。測試名稱不含逗號（doctest 的 `-tc` 把逗號當分隔）。

## 1. ip4a：查詢、操作、注入

### 判定（dummy driver）

- **注入只走 SDL 佇列**：注入之後、pump 之前，`Snapshot()` 不變；測試自己裝的 Native watcher 每次注入恰好看到 1 個事件。
- **每種注入在下一次 pump 進入幀**：
  - key：每個 `Key`（除了 `Count`）按下後，`events` 有一個 `KeyPressed`，`key` 等於注入的 `Key`；`Enter` 經 Native watcher 看到的 scancode 是 `RETURN`。
  - 滑鼠鍵：`MouseButtonPressed`／`Released`，按鍵相同。
  - focus：`WindowFocusGained`／`Lost`，方向相同。
  - motion：前一幀先注入 focus，下一幀注入 `(3, 7)`，`pointer.deltaX == 3`、`deltaY == 7`。
- **查詢每次讀 SDL**：經 Native 用 `SDL_SetWindowSize` 改大小，不 pump 直接 `QueryWindow`，讀到新大小；經 Native 切換相對滑鼠模式後，`flags.relativeMouseMode` 跟著變。
- **操作**：`SetWindowSize` 後 `QueryWindow` 讀回；`SetWindowPosition` 在下一次 pump 產生 `WindowMoved`（新位置）；`Sync()` 回傳不丟例外；`Raise()` 不丟例外。
- **查詢可在 live frame 內呼叫**：在 observer 內 push `EXPOSED data1=1` 觸發 live frame，handler 內呼叫 `QueryWindow`，不觸發斷言。
- **操作不可在 observer 內呼叫**：observer 內呼叫 `SetWindowSize`、`SetWindowPosition`、`Raise`、`Sync`，各自以 `GYO_CHECK_ASSERTS` 確認觸發斷言。
- **注入不可在 live frame 內呼叫**：live frame 的 handler 內呼叫注入，以 `GYO_CHECK_ASSERTS` 確認。
- **其他查詢**：`X11WindowNumber()` 在 dummy driver 下是 `nullopt`；`VideoDriverName() == "dummy"`；`QueryDisplay()` 成功且 `usableBounds` 的寬高大於 0。

### 突變

| id | 改壞的是什麼 | 由哪個判定殺掉 |
|---|---|---|
| m13 | 注入的 windowID 填 0 | key 的注入進入幀 |
| m14 | 注入直接寫進輸入幀，不經 SDL 佇列 | 注入只走 SDL 佇列 |
| m-enter | `Enter` 注入成 `KP_ENTER` | `Enter` 的 scancode 是 `RETURN` |
| m-table | 注入的按鍵表錯位一格 | 每個 `Key` 的注入 |
| m-motion | motion 的 dx、dy 對調 | motion 的注入 |
| m-focus | focus 的方向反了 | focus 的注入 |
| m26 | `QueryWindow` 讀 platform 自己的快取 | 經 Native 改大小後直接查 |
| m28 | 查詢也在 live frame 內斷言 | 查詢可在 live frame 內呼叫 |
| m29 | 操作沒有 observer 內的斷言 | 操作不可在 observer 內呼叫 |
| m-depth | 注入沒有分派深度的斷言 | 注入不可在 live frame 內呼叫 |
| m-x11 | 不是 X11 時回 0 而不是 `nullopt` | `X11WindowNumber` |

## 2. ip4b：觀測 hook、capture（先宣告，ip4b 開始時補完突變的 find）

- **時間戳**（U10 的 M6，避免重演 ip3c 的事後變更）：每次回呼取一個配對；`observedAt` 是該配對的第二次 Engine 讀取 e1；`timestamp = ConvertSdlTimestamp(t, pair, observedAt)`。判定：
  - 經 Native push 一個 `timestamp = SDL_GetTicksNS() − 10 ms` 的本視窗事件：`timestamp` 在 `observedAt − 10 ms ± τ` 之內，τ 照 ip3c 的定義（D35），以測試自己取的配對與 Native 的唯讀接縫 `LastWatchClockPairForTesting` 讀到的回呼配對計算。
  - push 一個未來 1 秒的事件：`timestamp == observedAt`。
- **injected**：注入的事件為 true；注入後立刻 `SetWindowSize`（dummy 同步送 RESIZED），該事件為 false；另一條執行緒以 Native push 本視窗 motion、主執行緒同時注入，前者全部為 false（1000 次，不重試）。
- **過濾**：其他視窗的事件、本視窗的 KEY_DOWN 與 USER 事件不回呼。
- **生命週期**：解構後 push，計數不變（決定性）；移動到 vector 並觸發重新配置後仍正確回呼；moved-from 解構不影響、moved-to 解構後停止。
- **並行解構**（U10 的 M6，只收緊）：另一條執行緒持續 push；回呼進入時設 inFlight、自旋約 100 µs、離開時清掉；主執行緒建立、解構 200 次，每次解構返回後 inFlight 為 false。60 秒，不重試。
- **在 live frame 或回呼內解構 watch**：觸發斷言（death test，D36 的平台邊界例外）。
- **platform 先解構**：watch 之後解構安全，之後 push 不回呼。
- **capture**：非對稱的測試圖（R≠B、alpha 非 255），輸出位元組與直接呼叫 SDL 的結果相同；hint 不設；Windows CI 用一個非 ASCII 檔名。
- 突變（ip4b 開始時補上 find）：m15、m15b、m15c、m16、m-kind、m17、m-order、m-move、m-moved-from、m20、m21、m22、m-depth-watch、m18、m-stride、m-flip、m-path。

## 3. ip4c：PRIVATE 連結（先宣告）

- G3（U10 的 M7 擴充）：四個後端與 `GYO::PlatformSDLMain` 的 `INTERFACE_LINK_LIBRARIES` 中，SDL3 只能以 `$<LINK_ONLY:>` 出現；任何非 `LINK_ONLY` 的 `*Native` 目標也不行；`INTERFACE_INCLUDE_DIRECTORIES` 不含任何 `native/include`。三個 Native 目標允許 INTERFACE。
- 突變：m19（SDL3 改回 PUBLIC）、m19b（PUBLIC 連 `PlatformSDLNative`）、m19c（PUBLIC 的 `native/include`）、m19d（`PlatformSDLMain` 的 SDL3 改 PUBLIC）。

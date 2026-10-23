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

## 4. 補記（ip4b 實作時，2026-10-22）

- **時間戳判定的更正（在執行任何測試之前）**：第 2 節寫的「`timestamp` 在 `observedAt − 10 ms ± τ` 之內」漏掉 push 本身的耗時（測試讀 SDL 時間到回呼取樣之間，約數 µs），正確的實作也會失敗；而且和 PLAN 的寫法「與 ip3c 相同的夾住 push 的窗口」不一致。改成照 PLAN：測試在 push 前取配對 P0、以 `t = P0.sdl − 10 ms` 推入、push 後讀 `after`，斷言 `P0.engine − 10 ms − τ ≤ timestamp ≤ after − 10 ms + τ`，τ 照 ip3c（D35），以 P0 與回呼配對（`LastWatchClockPairForTesting`）計算。另斷言兩次回呼的配對不同（新配對）。這是判定的更正，不是看了結果之後放寬。
- **突變**：
  - m-flip 不適用（本實作不翻轉，沒有可改壞的翻轉）。m-stride 原本以為只能把 pitch 加大（會讀超出範圍），審查後改成 pitch 少 1（SDL 拒絕），見下面「xhigh 審查之後」。
  - m-order 由該測試案例內的 SIGSEGV 偵測（另一條執行緒的回呼讀到已釋放的節點），預期訊息限定為該案例的 SIGSEGV。偵測依賴配置器在釋放後改寫或歸還記憶體；在 ASan 下會確定偵測（L1 沒有 ASan，記為已知限制）。
  - m21 在兩個時鐘不漂移的主機上換算結果相同，所以以「新配對」的斷言殺掉。
  - m-path（用 `path.string()`）只在 Windows 上殺得掉（其他平台 `string()` 就是 UTF-8），不列入本機目錄；Windows CI 由非 ASCII 檔名的測試涵蓋（mutants 不在 CI 跑）。

### xhigh 審查之後（ip4b，2026-10-22）

watch 的執行緒與生命週期做了局部 xhigh 審查（F1～F14）。照審查修改的地方：

- **回呼取樣改用 `SampleClockPair()`**：回呼原本自己讀三次時鐘再組配對，現在改用固定讀取順序的唯一取樣函式（ip3c 的 D34）；`observedAt` 是它的第二次 Engine 讀取。測試加上「回呼的配對落在測試前後兩次讀取之間」（SDL 與 Engine 各一條）。
- **injected 改成比對事件位址**：原本是執行緒區域的旗標，注入的 push 期間 SDL 在同一條執行緒排入的真實事件（例如 push 內跑的 live frame 造成的 RESIZED）也會被標成 injected。現在只有 `PushInjectedEvent` 正在推的那個事件本身才算。新增測試「只有注入的事件本身被標記」，以及「同一個事件物件之後再以一般 push 推入，不被標記」。
- **註冊計數的唯讀接縫** `RegisteredEventWatchesForTesting`：搬移指派要先移除被取代的 watch；以計數確定性地檢查，不再依賴配置器。
- **m-move、m-moved-from 其實適用**：前一版說「搬移不改變註冊的位址，所以沒有對應的突變」是錯的。搬移建構子沒清空來源、搬移指派沒移除被取代的 watch，都是可以寫出來的錯誤，現在分別由「moved-from 不再作用」與註冊計數殺掉。
- **watch 可以比 platform 活得久**：拆成兩個測試。宣告的版本（platform 解構後 SDL 的事件系統一起結束）照舊；另加「還有其他 platform 讓 SDL 繼續運作」的版本：watch 仍在 SDL 上註冊，但其他視窗的事件不會回呼，解構時從 SDL 移除。SDL 不重複使用視窗 ID（`SDL_GetNextObjectID` 單調遞增），所以舊 ID 只可能由手動 push 出現，這種情況不在保證之內。
- **capture 的測試移到 `tests/common/render/sdl_gpu`**（`render.sdl_gpu_capture_file`，cpu 標籤）：原本放在 platform 測試裡，讓 platform 測試連結了 GPU 後端。參考檔以 `u8string` 寫入；暫存目錄每次執行各自一個；`SceneCaptureFile.hpp` 註明位元組跟著 SDL 的 BMP 寫入器（`SDL_HINT_BMP_SAVE_LEGACY_FORMAT` 會改變它）。
- **併發測試獨立成 `platform_sdl_concurrency`**（逾時 60 秒），主測試維持 15 秒。
- **分派深度**：回呼內解構 watch 的 death test（`watch-in-callback`）。

新增的突變：m-inject-pointer、m-moved-from、m-move、m-dispatch、m-watch-sdl、m-watch-engine、m-button、m-delta、m-focus-watch、m-source、m-stride（pitch 少 1，SDL 拒絕建立 surface）。m15b 改為「推完之後沒有清掉標記」；m15c（標記不分執行緒）在位址比對之下不會改變可觀察的結果（其他執行緒推的不是同一個事件物件），刪除，「每條執行緒各自標記」的測試保留。

時間戳判定改成 [ip4b-judgment-change.md](ip4b-judgment-change.md) 的方案 B（D38）。方案 B 新增 m-watch-mid、m-watch-first、m-watch-late、m-watch-e1 四個突變。

Design Invariant 7 加「`EventWatch` 只在主執行緒解構」（D38），由 death test `watch-off-main` 與突變 m-main-thread 檢查。

### ip4c（2026-10-22）

照第 3 節做：

- **PRIVATE 化**：`gyo_platform_sdl`、`gyo_input_sdl` 的 SDL3 改成 PRIVATE；兩個 render 後端補上 PRIVATE 的 `SDL3::SDL3`。
- **G3**：寫在 `tests/common/platform_sdl/CMakeLists.txt` 的 `gyo_expect_private_sdl`。整個專案 configure 完之後才檢查，失敗時 configure 停下，訊息寫出目標名。
- **斷掉的目標**：只有 `tests/common/runtime_sdl`，因為它直接呼叫 `SDL_LogError`。改連 `GYO::PlatformSDLNative`，PLAN 已預告這一點。
- **突變工具**：TT 的 `run_mutations.py` 加 `expect_build_failure`。G3 在 configure 時讓建置失敗，原本的工具只會把它記成 build_failed；有這個欄位時，建置失敗而且訊息符合，才算殺掉。不設這個欄位的突變，判定方式不變。
- **m19c 的預期訊息**：CMake 會把長訊息折行，所以預期訊息改用 `[\s\S]*` 跨行比對。第一次執行時，它因為這個原因被判成 wrong_failure。


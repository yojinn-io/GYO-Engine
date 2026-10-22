# sdl-isolation 產品側 IP-4：分批與事前宣告（草案）

- 狀態：**核准**（v7.1 D168，2026-10-23）。P1～P5 照建議：P1 Waiter、P2 中立的 `RenderDeviceInfo` 存取點、P3 golden 加 Engine 補 s06（J1 的 §14.4 核准在 capture_golden_guard 完成後另交）、P4 gui-probe 放進 L2、P5 第 7 節第 2 版。L2 的第 3 版（工具雜湊、開發跑次、乾跑）在頭完成後另交。第 1 版的第 7 節經 D45 對抗式檢查後改寫（第 7.4 節）。
- 判定原則（使用者，D168）：**基準改變的量只記錄、不判定；和基準無關的量（產品行為、各樹各自的絕對門檻）照判。** 第 7 節的 G2～G6 依此分類：G3 的移動 Actual 和 probe 的節奏無關，照判；G4、G5 是基準改變的量，只記錄；G2、G6 是各樹自己的絕對判定。
- 依據：
  - [sdl-isolation.md](sdl-isolation.md) 的第 2～4 節，以及 S1、S2；
  - Engine 的 IP-4（已完成）：[platform-sdl.md](../../../architecture/platform-sdl.md)、[ip4-declaration.md](../../../architecture/plans/input-and-present/ip4-declaration.md)。
- 盤點：2026-10-22，在 HEAD `18b6c03` 上，由 3 個唯讀子 agent 完成：
  - probe 的 SDL 使用；
  - L2 門檻的來源；
  - pointer capture 的凍結 oracle。
  - 掃描腳本與結果放在本會話的 scratchpad（`scan.py`、`hits.txt`、`mut/`）。
- 編號說明：`sdl-isolation.md` 第 5 節的「U5、U6 的判定變更」沿用規劃草案的編號。
  - 草案的 U5 是「合成的 MOVED 改成真正移動視窗」，本文件記為 **J2**。
  - 草案的 U6 是「pointer capture 的凍結 oracle 改 golden」，本文件記為 **J1**。
  - Engine PLAN 定稿的 U5（`NativeEventObserver`）與這裡無關。

## 1. 盤點結果（摘要）

- 守衛 (a) 的五個樣式（去掉註解與字串後）命中 276 行，分在 9 個檔：probe 8 檔 215 行，`PointerCaptureCharacterizationTests.cpp` 61 行。`apps/object_fps_pvp` 為 0。
- 直接連 Native 的 target 有 3 個：
  - `build/acceptance/object_fps_pvp/CMakeLists.txt:38`：gui probe；
  - 同檔 `:47`：gui_quad probe；
  - `tests/object_fps_pvp/CMakeLists.txt:106`：pointer_capture_tests。
  - 沒有 target 直接連 `SDL3::SDL3`。
- 分類：
  - **A 機械替換**：約 186 行。
  - **B 證據語意改變**：約 21 行，見第 5 節。
  - **C 沒有中立對應**：約 67 行。其中 58 行是 pointer capture 的 oracle（J1），9 行是 `player_short.hpp` 的 SDL 時間戳與原始型別（改記 Engine 的時間與種類）。
- 注入器的語意：舊 probe 本來就經 `SDL_PushEvent` 和同一個 `SdlInput`，所以「不更新 SDL 內部狀態」「只在聚焦時累計 motion」對舊 probe 也成立，不是新的變化。實際差異只有兩項：
  - `PushKey` 會填 keycode，於是會產生 `layoutKey`；
  - 按鍵事件帶 `clicks=1`。
  - 產品只在 Ctrl+V 讀 `layoutKey`，對 probe 沒有影響。

## 2. 分批

```text
產品側 IP-4
├── probe_queries_entry   high                         ｜進入點、查詢與操作、capture、fingerprint、型別
├── probe_injector        high                         ｜注入改用 SdlInputInjector
├── probe_window_move     high（順序邏輯局部 xhigh）    ｜合成 MOVED 改成真正移動（J2）
├── probe_watch_pacing    high（watch 執行緒局部 xhigh）｜EventWatch、節奏（P1）、標籤、拿掉 Native
├── capture_golden_guard  high                         ｜pointer capture 改 golden（J1）、Engine 補 s06、守衛 (a)(b)
└── l2_interleaved        medium（宣告前做 D45 對抗式檢查）｜base 與頭交錯的 GUI 短測
```

| 代號 | 內容 | 驗收點 | 估時（h） |
|---|---|---|---|
| probe_queries_entry | 兩個 probe 的 `main` 改 `GyoMain`，連 `GYO::PlatformSDLMain`。`Raise`、`SetWindowSize`／`SetWindowPosition`、`Sync`、`QueryWindow`／`QueryDisplay`、`PlatformName`、`VideoDriverName`、`X11WindowNumber`、`SaveCaptureBmp`；`SDL_Rect` 等型別換成中立型別。fingerprint 的 GPU driver 名改由 P2 的存取點取得。`QueryDisplay` 的 usable 寬或高為 0 時，probe 記為 `unavailable`（和以前的空值相同） | 建置；`ctest -L pvp`；`test_presentation_evidence.py`、`test_native_window.py`；跑 1 次 `run_timing.py --gui --short`，`platform_*`、`window_*` 都能解析；capture 的 BMP 與 base 的逐位元組相同（同一張測試圖） | 3～4 |
| probe_injector | 4 個 `PushX` 改用 `SdlInputInjector` 與 `Key`；合成 MOVED 暫時保留 | 本機各跑 1 次：`run_action_short.py` 的 action60 與 capture、`run_player_short.py` 的 player60、`run_timing.py --gui --short --combat`（v7 HUD 規則）、`run_quad.py --case gui-bots`。都要 P，而且射擊數、延遲數與 base 相同 | 3 |
| probe_window_move | `action_short.hpp:366`、`gui_main.cpp:916-919` 的合成 MOVED，改成先 `SetWindowPosition`（真的位移），再 `Sync()`，最後注入點擊。`SetWindowPosition` 失敗（例如 Wayland）時，該檢查記為「無法執行」，不記為通過 | 本機 action60 的 `drag_suppressed`、`run_network.py --gui-probe` 的 `testedWindowMove` 都要成立；記錄 `Sync()` 的結果；見 J2 | 2～3 |
| probe_watch_pacing | `LatencyWindow` 與 `PlayerInputWatch` 改用 `EventWatch`：injected 改讀 `event.injected`（raise completion 的判定仍排除 injected，見 G6）；只保留舊的 8 種事件；Engine 時間戳取代 `sdl_timestamp_ns`。`SDL_DelayNS`／`SDL_Delay` 改用 P1 選定的睡法。計時基線的標籤改名（J3）。整數 typedef 一併換掉。兩個 probe 拿掉 Native 連結 | `test_presentation_evidence.py`（含讀原始碼文字的 `:722-734`）；跑 1 次 latency-short，synthetic 計數 20、disturbances 為空；player_short 的 watch 沒有 overflow、種類相符；phase-stalls 手動跑 1 次，6 次都恢復，並與 v6 的基線並列記錄 | 3～4 |
| capture_golden_guard | J1：pointer capture 改成 policy 層的 golden。Engine 的 `tests/common/input` 補 s06 守衛（下面第 6 節）。測試拿掉 Native。守衛 (a)(b) 與它的 3 個突變（第 4 節） | J1 的 §14.4：突變、他人驗證、你核准；守衛的 3 個突變都以預期訊息失敗；L1 四平台（draft PR，要手動觸發） | 4～6 |
| l2_interleaved | 第 7 節（含第 3 版、開發跑次） | 第 7 節 | 4～5 |

- 合計約 19～25 h，含 L2。原估 14～20 h 只到上限附近，超出的部分來自兩項：盤點新增的 B 類，以及 J1 要補 Engine 的 s06 守衛。
- 依「時間盒」規則，超過 1.5 倍（30 h）就停下回報。

## 3. 待你決定

| # | 問題 | 選項 | 建議 |
|---|---|---|---|
| P1 | probe 的節奏與停頓：`SDL_DelayNS`／`SDL_Delay` 換成什麼。共 10 處：`gui_main.cpp:532,690,730,775,1047`、`action_short.hpp:616,1015`、`player_short.hpp:309`、`native_window.hpp:107`、`platform_fingerprint.hpp:36` | **(a)** `Waiter`：v7 第 12 批的原文就是「SDL_Delay 改用 Waiter」。macOS 上 Waiter 用 kqueue 的 NOTE_CRITICAL，沒有計時器合併，probe 的幀節奏會更接近名目值，和 base 不同。**(b)** `std::this_thread::sleep_for`：macOS 與 Linux 上和 SDL 一樣是 nanosleep，Windows 則不同，節奏大致和 base 相同 | **(a)**。理由：第 7 節要判定的量（移動的 Actual、首次送出、逐筆判定、各樹各自的門檻）都不受 probe 相位影響；受影響的量（節奏、可見交越、回饋、裁決抵達、計時基線）在 pv7-checkpoint 起就只記錄（D132）。(b) 只能在兩個平台上和 base 等價，又違背第 12 批的原文。更正（D45）：可見交越、裁決抵達在各樹內**仍受絕對門檻判定**（G2），只有跨樹比較只記錄；17 輪的餘裕很大（可見 P95 38.4～41.5 ms，門檻 80；裁決抵達 P95 33～54 ms，門檻 150）。代價：(1) 計時基線（`platform_timer_*`）的 4／8 ms 主機狀態分類，兩樹之間不能比，主機狀態改用和兩樹無關的 session `sleeper.py` 判斷；(2) 節奏接近 60 時，幀與 60 Hz Tick 的拍頻降到約 0.1 Hz，20 個事件的相位抽樣變得相關，可見交越 P50 可能整體平移約 ±8 ms（仍遠低於 50 ms），而且 v7 的 HUD 規則沒在接近名目值的節奏下跑過，所以頭在 L2 前要先跑開發跑次（第 7.1 節） |
| P2 | S1(b) 的缺口：移除 `PvpApplication.hpp:129` 的 `SdlGpuRenderDevice&` 之後，`platform_fingerprint.hpp:24` 與 `gui_main.cpp:11` 還要讀 GPU driver 名 | **(a)** 產品加 `[[nodiscard]] Engine::Render::RenderDeviceInfo RenderDeviceInfo() const;`，這是 `IRenderDevice::GetInfo()` 的中立型別。**(b)** 回傳 `Engine::Render::IRenderDevice&`。**(c)** fingerprint 不再記 driver | **(a)**：只交出資訊，不交出裝置，範圍最小。這是產品公開介面的小改動，和 S1 的決定一起記錄 |
| P3 | J1（pointer capture）走哪個選項 | A：golden 加 Engine 的 s06 守衛／B：保留，列為符號守衛的例外／C：退役／D：保留 oracle，改用注入器驅動 | **A**，理由見第 6 節 |
| P4 | `run_network.py --gui-probe`（`Run --move`）要不要放進 L2。它的合成 MOVED 會改（J2），v7 與 v7.1 的 L2 項目清單都沒有它（v6 的 `MANUAL_ACCEPTANCE.md:119,124` 有） | 放進（每樹 1 次，判定見 G8）／不放，只在本機跑 | **放進**。J2 改的就是它的證據。但它現在「通過」不代表移動的檢查有做（`testedWindowMove` 在推出事件後就設為真），所以 G8 改判報告裡的三個子檢查 |
| P5 | L2 的門檻與停止規則（第 7 節） | 照草案／修改 | 照草案。D45 的對抗式檢查結果見第 7.4 節 |

## 4. 完成條件的守衛（sdl-isolation.md 第 3 節，細節定案）

- **(a) 原始碼**：
  - 掃描範圍：`apps/object_fps_pvp`、`build/acceptance/object_fps_pvp`、`tests/object_fps_pvp` 的 C／C++ 檔，先去掉註解與字串字面值。
  - 樣式：`\bSDL_\w+`、`\bSDLK_\w+`、`\bSDLCALL\b`、`#include <SDL3/`、`\b[US]int(8|16|32|64)\b`（最後一個只套用在 C++ 檔）。
  - 另外加一條：include 任何 `*Native.hpp`、或呼叫 `NativeWindow(`、`NativeEvent(`、`HandleNativeEvent(`，也算命中。理由：盤點發現 Native 的使用（`NativeWindow(...)` 共 39 處）不會被 (a) 的 SDL 樣式抓到，只能靠 (b)；(a) 也抓得到，訊息才會指出是哪一行。
  - 例外清單從空開始，維持空。
- **(b) 連結圖**：本產品的所有 target，自己的 `LINK_LIBRARIES` 都不含 `SDL3::*`、`SDL3_image::*`、任何 `*Native`。產品在 configure 結束時檢查，寫法比照 G3（`gyo_expect_private_sdl`）。這個檢查登記在 `tests/object_fps_pvp/CMakeLists.txt`，只在選擇本產品時啟用。
- **事前宣告的突變**（都要以預期的訊息失敗；(b) 用 `expect_build_failure`）：
  - g1：在 `PvpApplication.cpp` 加回一個 `SDL_GetTicks()`，由 (a) 抓到；
  - g2：產品 target 改連 `GYO::PlatformSDLNative`，由 (b) 抓到；
  - g3：probe 加回一個 `Uint32`，由 (a) 抓到；
  - g4（新增）：probe 加回 `NativeWindow(application.Platform())`，由 (a) 的 Native 規則抓到。

## 5. B 類（證據語意改變）的事前宣告

S2 的做法是「各樹用自己的 probe」，所以下面這些差異都列為**預期的差異**。它們只影響只記錄的量，或是第 7 節以「兩樹各自判定」處理的量：

1. **節奏與停頓**（P1）：幀間隔分布、實測 FPS、`skipped_frames`、player 85% 閘門的餘裕、hold 幀數、phase-stalls 的 `productElapsedLowerBound`／`recoverySeconds`、計時基線的全部欄位。
2. **合成 MOVED 改成真正移動**（J2）。
3. **LatencyWindow 的 injected 歸屬**：舊的做法是 probe 自己的時間窗旗標；新的是 Engine 以事件位址判定。預期計數相同（mover 20、join 0）。如果在舊旗標的時間窗內出現真實事件，以前算成 synthetic，現在算成 OS 事件；這種情況在既有的 11 輪裡沒有出現過。
4. **PlayerInputWatch 的欄位**：`event_type` 改成 `ObservedEventKind` 的值；`sdl_timestamp_ns` 改成 Engine 的時間戳，鍵名改成 `timestamp_ns`；semantics 字串改寫。目前沒有分析器讀這些欄位。
5. **計時基線的標籤**（J3）。

## 6. 判定變更草案

### J1：pointer capture 的凍結 oracle 改成 golden（P3＝A）

現況：`PointerCaptureCharacterizationTests.cpp:35-71` 的 `OracleHandle`，是第 06 批之前 `HandleNativeEvent` 遊戲中分支的原樣複製，是一個可執行的參考實作。

測試把同一個 `SDL_Event` 同時交給 Engine 的 `SdlInput`＋產品的政策，以及這個 oracle，逐幀比對 6 個欄位加上釋放次數。輸入有三組：手寫 14 幀、固定種子 5 組 × 3000 幀、宣告差異 2 的單獨測試。

**新的做法**：

- **golden 的內容**：
  - 輸入：同一批序列，但換成 policy 邊界上的 `InputEvent` 序列，加上每幀的 `focusedAtStart`；
  - 輸出：每幀的 6 個欄位、`releases`、`releasesWhileCaptured`。
- **錄製**：在 `18b6c03` 以後的 HEAD 上，用一次性的產生器錄。產生器在同一次執行裡確認 oracle 與現行實作 0 不一致。產生器要用 Native，所以放在證據夾，不留在樹裡。
- **存放**：`tests/object_fps_pvp/fixtures/pointer_capture_golden.txt`，檔頭比照 `authority_golden.txt`，寫明：
  - 來源 commit；
  - oracle 0 不一致；
  - 只在事前宣告的政策變更時重產，並保留舊檔。
- **比對**：純函式重播，逐幀逐欄完全相等，不需要 SDL。

**§14.4 的 1～5（草案；核准前會補上實測、突變與他人驗證）**：

1. **守的性質**：產品的指標擷取政策（`PointerCapture.cpp`），與第 06 批之前的行為相同。另外，經 Engine 的 `SdlInput` 轉出的事件語意（insideWindow、焦點、本視窗過濾、按鍵重複）和舊 oracle 的假設一致。
2. **放寬的面**：
   - L1：Engine 轉換層的錯誤（重複鍵、insideWindow 恆為真、其他視窗的按鍵或失焦）不再由本測試抓。
   - L2：insideWindow 用了過期的視窗大小（突變 s06）。**目前只有本測試抓得到。**
   - L3：`SdlInput` 逐事件焦點語意的改變。
   - L4：參考的獨立性下降，從「與第 06 批之前的程式相等」變成「與錄製時的實作相同」。
3. **收緊的面**：
   - T1：每一幀都比 `releasesWhileCaptured`（以前只在一個單獨的測試裡比）；
   - T2：部分幀開始時預設 `pendingReloadEdge=true`，可以殺掉目前存活的突變 p24。
4. **放寬之後由誰守**（子 agent 在 scratch 實測）：
   - L1：s01～s05 由 Engine 的 `input_sdl_tests` 殺掉（`tests/common/input/SdlInputTests.cpp:167-253`）。
   - L2：s06 在 Engine 的測試下**存活**。所以本批要在 `tests/common/input` 新增「點擊交給 `SdlInput` 時，用當下的視窗大小判定 insideWindow」，屬 Engine owner 的範圍，只收緊。這個語意目前沒有寫進任何契約，我會在 `input.md` 的 Contracts 補一句。這會改到契約條文，但不是 Design Invariants 節。
   - L3：Engine 側由 `SdlInputTests.cpp:216-253` 守；policy 側由 golden 殺掉 p29、p30。
   - L4：檔頭規則加上保留舊檔。重新錄製只在事前宣告的政策變更時才做。
5. **根據與證據**：
   - 子 agent 的突變結果：`PointerCapture.cpp` 30 個中殺 28 個。存活的是 p01（等價突變）與 p24（缺口，T2 處理）。`SdlInput` 的 s01～s06 結果如上。
   - 腳本：scratchpad 的 `mut/run.py`、`mut/mutants.json`。
   - 決定的由來：IP 的 U6（`PLAN.md:706`）、M7（IP HANDOFF）。
   - 本批要把 p02～p30（p01 除外）與 s06 登記到 `tests/object_fps_pvp/mutations.json`，新增一個 `pointer_capture` check。

**其他選項的取捨**：

- B（保留，列為例外）：守衛的例外清單就不再是空的，牴觸 D33 與 U7 的「維持空」。
- C（退役）：30 個 policy 突變中只殺得到 3 個。
- D（保留 oracle，改用注入器驅動）：注入器做不出其他視窗的事件、按鍵重複、視窗外的點擊、MINIMIZED，輸入範圍會縮小，而且 oracle 必須改寫。

### J2：合成 MOVED 改成真正移動視窗

- **對象**：兩項判定都**不變**，改的是驅動它們的事件來源。
  - `run_action_short.py` 的 `drag_suppressed`：拖動時點擊不射擊，而且不擷取。
  - `gui_main.cpp` 的 `Run --move`：移動視窗後放開 W；重新取得焦點不重新擷取指標。
- **差異**：
  - 舊的做法是 probe 自己推一個 `SDL_EVENT_WINDOW_MOVED`。
  - 新的做法是 `SetWindowPosition`（真的位移）加上 `Sync()`，事件由 OS 送來。它可能伴隨其他視窗事件，到達時間由 OS 決定。
- **風險**：`Sync()` 逾時、MOVED 沒有排在點擊之前時，點擊會變成射擊邊緣，判定失敗。這是失敗（F），不是放寬。
- **放寬的面**：沒有找到。判定的條件與門檻都沒變；新的來源更接近真實使用。
- **事前宣告的情況**：
  - 平台不能移動視窗時（Wayland）記為「無法執行」，不記為通過。
  - L2 只在 macOS 跑，所以這一點只影響 L1 與其他平台的手動驗收。
- **§14.4**：判定沒變，不屬於 §14.4 的判定變更；照 sdl-isolation.md 第 2 節的「證據語意改變，事前宣告」處理，也就是本節。

### J3：計時基線的標籤 `"SDL_DelayNS remainder"`

- 盤點發現，`test_presentation_evidence.py:526` 只是測試資料，標籤沒有被任何判定斷言；分析器只是原樣轉寫，並寫明「never a verdict」（`presentation_evidence.py:333-344`）。
- 所以改名**不是判定變更**，不需要 §14.4 的核准。這更正了 `sdl-isolation.md` 第 2 節「照 §14.4 報告 1～5」的寫法。
- 新標籤：P1 選 (a) 時是 `"Waiter remainder"`。測試資料跟著改，保持一致。

## 7. L2：base 與頭交錯的 GUI 短測（宣告草案，第 2 版）

這次核准的是**門檻與規則**。比照 pv7-checkpoint 的做法分兩段：

1. **本次核准**：門檻與規則（本節）。
2. **頭完成後再交一次（第 3 版）**：工具雜湊、開發跑次、乾跑，以及兩樹的執行檔與凍結清單逐項核對。第 3 版只能補事實，不能放寬本節的判定。

### 7.1 對象、做法、前置

- **base 樹**：PR 對 master 的 merge-base `2307a56`。它和舊稱的 `6fbb6a9` 只差 4 個文件檔，建置等價；scratchpad 的 worktree `wt-base` 要更新到 `2307a56`。
- **頭樹**：本分支完成 `capture_golden_guard` 之後的 HEAD。
- **舊證據只是旁證**：兩樹都是 movement trace schema 3 的樹。既有的 GUI timing 證據（v7-16、pv7-cp、a1，≤`43a8617`，schema 2）只能當跨建置的旁證；base 上從沒跑過 GUI timing。
- **probe 與分析器**：兩樹各自建置，各用自己的 probe 與分析器（S2）。timing 兩樹都用 v7 的 HUD 規則重判（D83、D84）。頭的分析器相對 base 的差異，只能是第 5 節宣告的項目（J3 標籤、鍵名）；第 3 版用 `git diff 2307a56 <頭> -- build/acceptance/object_fps_pvp/*.py` 列出。
- **前置（G10）**，每一項不成立都要停：
  - 兩樹的權威 digest 0 差分（`pv7-checkpoint-l2.md:20-24`）。
  - 記錄凍結清單、產物、工具的雜湊。
  - L1 的 capture BMP 與 base 逐位元組相同（probe_queries_entry 的證據）。
  - 用頭的分析器重判既有的 17 個 timing 跑次，分類和原本相同（比照先例的 corpus 檢查）。
- **開發跑次**（第 3 版前，只看分類，不作為判定）：
  - 頭：timing 3 次。看接近名目值的節奏下，v7 HUD 規則與可見交越的分類。
  - 頭：action short 3 次。看 J2 真正移動視窗時，有沒有游標下的 MOUSE_MOTION 造成 `unscheduled_yaw_frames`，以及 `Sync()` 的阻塞。
  - 兩樹：gui-probe 各 3 次。v7 第 04 批的開發跑次曾 3 次失敗 1 次（0 個配對）。
- **環境**：
  - 開勿擾、`caffeinate -dims`、接上電源；
  - 顯示器亮著、不碰滑鼠；關 Chrome；暫停 PR 的 Auto-fix（`pv7-checkpoint-l2.md:44,111`）；
  - 用專用 worktree，不在主 checkout 跑（D151）；
  - 閒置閘門：`sleeper.py` 跑 3 次、每次 5 秒，最大值 <10 ms。30 分鐘內不成立就停（`16-integration-acceptance.md:294`）。
- **涵蓋不到的**：X11 原生視窗（`native_window.hpp` 會改，`run_native_window.py` 需要 X11／XTest）在這台 Mac 跑不了。本次 L2 不涵蓋，只靠建置、L1 與日後 Linux 的手動驗收。

### 7.2 項目（G0）、單位與次數

- **單位**：runner 的一次呼叫，比照先例：action short、player short 都是一次呼叫跑 4 案。
- **每樹 10 次呼叫**：
  1. timing-gui-short；
  2. action-short（action30、action60、action144、capture）；
  3. player-short（player30、player60、player144、capture）；
  4. gui-bots；
  5. bots2-action60；
  6. bots2-player60；
  7. bots2-timing-gui-short；
  8. gui-probe（`run_network.py --gui-probe`，P4）；
  9. rapid-edge-60；
  10. rapid-edge-144。
- **順序與次數**：兩樹合計 20 次。每項每樹 1 次，B／A 逐項交替，第 1 項 B 先。不補跑、不重跑（v7 P4 的「只跑一次」；`pv7-checkpoint-l2.md:28`）。
- **sleeper**：每次呼叫前後各跑 1 次。

### 7.3 判定

| # | 判定 | 來源 |
|---|---|---|
| G1 | 每一項、每一案例，兩樹都要 P。player144 的 `invalid_capacity` 依 D11③ 接受，不算 P 也不算 F；規則照 `judge_16.py:313`，不附顯示器頻率的條件（這台 Mac 是 60 Hz，v7-16-L2 的 player144 也是 invalid）。其他任何 `invalid`，包括 timing 的 `invalid_window_*`，都算 F | `pv7-checkpoint-l2.md:50`（R1）；`16-integration-acceptance.md:143,146`；D11③ |
| G2 | 各樹照自己凍結的分析器判絕對門檻：<br>・可見交越 P50 ≤50 ms、P95 ≤80 ms，配對率 ≥99%（`presentation_evidence.py:601-607`）；<br>・裁決抵達 P95 ≤150 ms（`combat_gui_evidence.py:234-236`）；<br>・移動：首次送出 P95 ≤22 ms，產生→Actual P50 ≤50 ms、P95 ≤66.7 ms，Actual 比例 ≥99%（`command_evidence.py:413-416`）；<br>・quad：每位玩家 60 Hz 產量 ±2 步，每次經核對的重生另加 3 步，主機停頓（`gaps_100ms`）時不判（`quad_evidence.py:193-196`；`16-integration-acceptance.md:284`）；<br>・視窗閘門 clean。<br>player 的「實測 FPS ≥名目 85%」是 validity 閘門（`player_presentation_evidence.py:82-90`），不成立時依 G1 處理 | 同左；v6 `MANUAL_ACCEPTANCE.md:166-180`；`checks.json:16-17` |
| G3 | timing 兩項（timing-gui-short、bots2-timing-gui-short）的移動 Actual P50、P95：`A − B ≤ 1 ms`。另記雙側的差。任一樹的該項受干擾（D）時，不判，記為「無法判定」。不成立時不自動當成退步，交使用者判斷（比照 `pv7-checkpoint-l2.md:79`） | **套用到 GUI 是新的。** 規則形式出自 `pv7-checkpoint-l2.md:70`（非 GUI、每樹 3 次的 `max(range B, 1 ms)`），每樹只跑 1 次時只剩 1 ms。推導：<br>(1) Actual 和 probe 的節奏無關：模擬在自己的 RoleThread 上，以 `WaitUntil` 對齊步邊界（`ClientSimulationRole.cpp:168-187`），`generated` 在模擬執行緒上打時間戳（`LocalPlayerPrediction.cpp:356`）；同一樹內 clean-30／60／144 的主迴圈 FPS 差 4.8 倍，Actual P50 只差 ≤0.17 ms。<br>(2) 17 輪的分布：P50 37.42～37.62、P95 37.55～37.75 ms，標準差約 0.06 ms；同一 session 內 A−B 的絕對值 ≤0.11 ms。<br>(3) 每個 epoch 的第一次相位有 0.5 ms 的死區（`Movement.hpp:75`），兩次跑最多可差約 1 ms 而沒有退步，所以不能再往下收 |
| G4 | 可見交越、裁決抵達：跨樹比較只記錄，兩樹並列。各樹的絕對門檻仍由 G2 判定 | `pv7-checkpoint-l2.md:77`（C8，來源是 16-L2 的 L2-6） |
| G5 | 節奏只記錄。預測：timing 兩項的 create 與 join，頭的 `measured_mean_fps` 都 ≥59.0，而且都高於同一項、同一角色的 base。任何一格不滿足，就推翻這個預測，記下來交你判斷。不影響 G1～G3 | **新的**。機制：`Waiter.hpp:60-65`（NOTE_CRITICAL）。base 的參考值：既有 17 輪 create 56.46～57.97、join 56.39～57.97 FPS（不是 base 樹跑的） |
| G6 | mover 的 injected 視窗事件數：每一樹都 ＝實際觸發的事件數（`latency-events` 的列數）。join 為 0。不成立記為 A 類（工具），停下 | **新的**。40 輪（v5～v7.1）全部是 create 20、join 0。macOS 的 OS 視窗事件只在主執行緒 pump 時排入，舊的時間窗結構上進不來；新的事件位址判定也不會把真實事件算成 injected。頭的實作必須保留「raise completion 的判定排除 injected」（舊的是 `gui_main.cpp:333` 的 `!synthetic`），否則計數會變 19。G6 正好守住這一點 |
| G7 | action 的 REQUIRED_CHECKS，包括 `drag_suppressed`，兩樹都為真（包含在 G1 的 P 裡）。J2 的風險見開發跑次 | `run_action_short.py:38-46,80-111` |
| G8 | gui-probe：兩樹都要 P（網路階段照 `judge_16` 的分類）；而且 `gui/create-report.txt` 的 `window_move_releases_capture`、`window_move_stops_held_input`、`tab_toggles_capture` 三行都是 `true`。presentation 的部分（至少 3 個配對、中位數 ≤0.15 s）沒有視窗閘門；它不成立時先立旗標交你判斷，不直接停 | **新項目**（P4）。三個子檢查：`gui_main.cpp:948-978,1072-1074`。presentation 的門檻出自 v3 的 `bfeb047`（`presentation_evidence.py:15-16,162-165`）。fix06 第 47 行寫明 gui-probe 沒有閘門 |
| G9 | 受干擾：每次呼叫前後的 `sleeper.py`，任一次 ≥10 ms，該次記為 D。D 不重跑；它對 G3、G5、G6 不判，只記錄。兩樹合計 20 次呼叫中，累計 ≥3 次 D 就停 | 10 ms 與前後的 sleeper：`pv7-checkpoint-l2.md:28,62`。**≥3 是新的**，論證：v7.1 的 13 個 session、1512 個跑次中受干擾 23 次（1.52%），n＝20 時 P(≥3) 約 0.3%；環境差的時候（d44c 是 4.6%），P(≥3) 約 6%；GUI 項目歷來約 0.5%（1/191） |
| G10 | 前置（第 7.1 節） | `pv7-checkpoint-l2.md:20-24` |
| G11 | rapid-edge-60、rapid-edge-144：兩樹都以 `classify_rapid` 判 P。權威 Cooldown 只記錄 | `pv7-checkpoint-l2.md:40,50,52`。IP-4 換掉的就是每幀點擊的注入路徑，Waiter 也會改變 144 FPS 時的點擊數 |

**停止規則**：

- 任一樹出現 F（含 `invalid_window_*`）就停（`a1-shot-step.md:116`）。
- E 或 A 類記為工具或環境無效，也停（16-L2 的停止條件 1；先例第 4 節）。
- G9 累計 ≥3 次 D 就停；閒置閘門 30 分鐘內不成立就停。
- 逾時：quad、action、player、網路 900 s，timing 300 s（`16-integration-acceptance.md:146-147`）。逾時算 E。
- 收到 STOP-REQUEST 就停。
- 失敗保留，不以後來的成功覆蓋（AGENTS §14.1）。停下之後先回報，再決定下一步。

### 7.4 D45 對抗式檢查（第 1 版 → 第 2 版）

另一位唯讀審查者逐項核對了第 1 版的來源，並重算既有證據。指出並已修正：

- **G8 檢查不到 J2**：移動的三個子檢查沒有 Require；門檻來源引錯（應為 `bfeb047`，不是 fix06）；gui-probe 沒有閘門，而且有失敗前科。→ 改判三個子檢查，加開發跑次，presentation 部分改為立旗標。
- **G1 的 player144**：「120 Hz」的條件和這台 Mac（60 Hz）不符，也無法機械判定。→ 改照 `judge_16.py:313` 無條件接受 `invalid_capacity`。
- **G9 的比例論證是錯的**（照比例應該是 0.7），「同一情境同一樹 ≥2」在每樹 1 次的設計下永遠不會成立。→ 改用二項分布的論證，並定義單位為 runner 的一次呼叫。
- **停止規則不完整**：缺 B 的 F、E／A 要不要停、閒置閘門的期限、逾時。→ 補上。
- **G3 的數值成立**，但要補推導（節奏無關的直接證據、第一次相位的死區）、不成立時的處理、D 跑次的處理。→ 補上。
- **G4 引錯**：應是 C8，不是 D132；「回饋」沒有定義。→ 改引 C8，刪掉「回饋」。
- **G5 的推翻條件不是預測的否定**。→ 改成可判定的條件。
- **G6 應對實際觸發數**，不是常數 20；另指出頭的實作必須保留「raise completion 排除 injected」。→ 改寫，並寫進 probe_watch_pacing 的實作要求。
- **G11 在先例是判定項**。→ 改成兩項 rapid 判 P。
- **其他**：
  - merge-base 已是 `2307a56`；
  - 舊證據都不是 base 跑的；
  - 缺開發跑次與第 3 版的流程；
  - X11 涵蓋不到；
  - 缺環境條件；
  - 本文 P4 與 v7 的 P4 撞名。
  → 都已補上或更正。本文第 3 節的 P4 不改名，第 7 節提到 v7 的時一律寫「v7 P4」。

審查的一次性輸出在 scratchpad（`timing_summaries.txt`、`handoff.md`）。

## 8. sdl-isolation.md 第 2 節的更正（核准後與本批一起改）

- `PvpApplication.cpp:1205` 那兩列已完成，現在的位置是 `:1218`。
- typedef 清單不完整：
  - 漏了 `gui_main.cpp:532,775`、`action_short.hpp:616,1015`、`player_short.hpp:309`、`native_window.hpp:107`、`platform_fingerprint.hpp:36` 的 `Uint64`；
  - 漏了 `gui_input.hpp:15` 的 `Uint32` 與測試 `:147` 的 `Uint8`；
  - 已列的 `gui_main.cpp:263,287` 實際在 `:264,288`，兩個都是 `Uint32`。
- 行號漂移：`gui_main.cpp:915-918` 現在是 `:916-919`；pointer capture 測試的 `:92-104`，Native 現在在 `:13,86-87,102,268,270`。
- 標籤「由 `test_presentation_evidence.py:526` 斷言」不正確，見 J3。
- S1(b) 漏了 GPU driver 名的存取點，見 P2。
- B 類漏列的項目見第 5 節。
- 沒有寫到 probe 的 CMake 與進入點的改動。
- 沒有寫到 `test_presentation_evidence.py:722-734` 會讀 `gui_main.cpp` 的原始碼文字。
- 連到 `after-re-evaluate.md` 的連結在本分支是斷的：該檔只在記錄器分支 `claude/pvp-v71-after-re-evaluate`。兩條線都合併之後才會恢復，先記下來。

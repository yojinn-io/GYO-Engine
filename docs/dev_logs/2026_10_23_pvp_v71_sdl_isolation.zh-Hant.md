# PvP v7.1 sdl-isolation 產品側：產品、probe、產品測試不再直接呼叫 SDL

日期：2026-10-21～23。分支 `claude/pvp-v71-sdl-isolation`，和 Engine 的 IP-3／IP-4 在同一個 PR [#85](https://github.com/yojinn-io/GYO-Engine/pull/85)（D29）。PR 尚未合併。

**狀態：完成**（產品側 IP-4 的 6 批，含 L2）。PR #85 尚未合併。

相關文件：

- 計畫：[sdl-isolation.md](../object_fps_pvp/plans/v7.1/sdl-isolation.md)（D160）。
- 分批與事前宣告：[sdl-isolation-ip4-declaration.md](../object_fps_pvp/plans/v7.1/sdl-isolation-ip4-declaration.md)（D168；L2 第 3 版 D170）。
- J1 的判定變更：[sdl-isolation-j1-judgment-change.md](../object_fps_pvp/plans/v7.1/sdl-isolation-j1-judgment-change.md)（D169）。
- Engine 側的記錄：[IP-3](2026_10_22_engine_ip3.zh-Hant.md)、[IP-4](2026_10_22_engine_ip4.zh-Hant.md)。
- 決策 D158～D170：v7.1 記錄器分支 `claude/pvp-v71-after-re-evaluate` 的 HANDOFF。

## IP-3 時點的產品工作（已在 Engine IP-3 的 dev_log 記錄）

- 產品本體（`main.cpp`、`PvpApplication.cpp`）改用 Engine 的進入點、文字輸入、剪貼簿、日誌橋、`BaseDirectory`、`QueryWindow`。
- 產品本體的 SDL 符號歸零，不再連 Native；日誌檔的位元組不變（S3）。

## 產品側 IP-4 的分批

規劃（D168）：3 個唯讀子 agent 做盤點，L2 門檻另做 D45 的對抗式檢查。

盤點結果：probe 8 檔加 pointer capture 測試，共 276 行直接使用 SDL。其中機械替換約 186 行，會改變證據語意的約 21 行，沒有中立 API 可替換的約 67 行。

| 批次 | 檔位 | 做了什麼 | 驗收 |
|---|---|---|---|
| `probe_queries_entry`（`5ef5e69`） | high | 兩個 probe 的進入點改成 `GyoMain`；視窗查詢與操作改用 `SdlPlatform`；capture 改用 `SaveCaptureBmp`；產品公開介面以 `RenderDeviceInfo()` 取代 `SdlGpuRenderDevice&`（P2） | GUI 短測 P；L1 #573 |
| `probe_injector`（`d5ecfa3`） | high | 輸入注入改用 `SdlInputInjector`（`ProbeInput`）；按鍵改用 `Key` | action60、capture、player60、gui-bots 都 P；timing-combat 用 v7 HUD 規則判 P；action60 的事件與射擊數和 pv7-checkpoint 相同；L1 #575 |
| `probe_window_move`（`089c0b5`） | high，順序邏輯局部 xhigh | 合成的 MOVED 改成真的移動視窗（J2）：`SetWindowPosition`、`Sync()`，再確認位置確實改變 | action60 4 次、gui-probe 2 次都通過；L1 #577 |
| `probe_watch_pacing`（`bbc716a`） | high，watch 執行緒局部 xhigh | 兩個 watch 改用 `EventWatch`（injected 依事件位址判定）；13 處睡眠改用 `Waiter`（P1）；標籤改名（J3）；拿掉 Native | timing、player、action、gui-bots 都 P；phase-stalls 6／6 恢復；L1 #579 |
| `capture_golden_guard`（`8d7182e`、`0e977cf`、`e0de346`、`7f7b704`） | high | pointer capture 的凍結 oracle 改成 golden（J1）；Engine 補 3 項守衛；完成條件的符號守衛 (a)(b) | 突變：產品 34／34、Engine 5／5；J1 獨立核對；L1 #582 |
| `l2_interleaved` | medium | base `2307a56` 與頭 `7f7b704` 交錯跑 GUI 短測，共 20 次呼叫（第 7 節，第 3 版 D170） | **通過**：20／20 P，G1～G3、G6～G8、G10、G11 成立 |

## 遇到的問題

### 測試場地給錯，頭和 base 以相同方式失敗（`probe_injector`）

- **現象**：第一次本機跑 action60 時，射擊全部沒命中。
- **處理**：建置 base 的 probe 來對照，結果 base 也以完全相同的方式失敗。
- **原因**：我把場地誤給成 `pvp_corners.json`，以前的跑次用的是 `pvp_arena.json`。
- **歸類**：操作錯誤（E 類），不是缺陷，證據保留。

### 真正移動視窗的順序保證（`probe_window_move`，xhigh 審查）

- **順序有保證**：macOS 上 MOVED 在 `SetWindowPosition` 內部就已經排進佇列，實測 20／20。
- **註解要改**：`Sync()` 在 macOS 只 pump 一次，在 Windows 什麼都不做，所以把順序歸功於 Sync 是錯的。真正的守衛是「位置確實改變了」那條檢查，註解已改正。
- **另外補上**：移動紀錄（前後的 x、Sync 的耗時）、drag 之前必須在擷取中（避免空成立）、錯誤訊息帶上 detail。

### watch 的歸屬改成事件位址（`probe_watch_pacing`，xhigh 審查）

- **結論**：沒有 critical 或 major。新的歸屬方式等價於舊的時間窗旗標；raise completion 仍然排除 injected（G6）。
- **記下的測試缺口**：T1～T4，見宣告第 9 節。

### pointer capture 改 golden 的放寬面（`capture_golden_guard`，獨立核對）

- **錄製方式**：產生器在同一次執行裡跑舊 oracle 和現行政策，15014 幀 0 不一致。
- **L5（欄位投影）**：第一版 golden 只記了政策當時讀的欄位，「政策開始讀 windowX」這類改動會漏掉。重錄成除時間戳以外的全欄位後補上。
- **v-s07～s10**：Engine 側「取得焦點會設 windowFocused」「事件不去重」原本由舊 oracle 間接守著。現在由 Engine 測試直接守，並寫進 `input.md` 的 Contracts（D169 (a)）。
- **守衛 (b) 本身也被驗到**：重錄時暫時加的產生器 target 連了 Native，configure 階段就被守衛 (b) 擋下。

### 草案中照字面做不到的兩項（L2 第 3 版，D170）

- **語料重判**：17 個跑次裡有 6 個是 pv6 的證據，頭的分析器無法判讀。範圍改成 11 個 pv7 跑次，兩層都要相同，結果 11／11。
- **capture BMP 逐位元組相同**：兩次渲染的畫面本來就不同。改成以頭樹的 `render.sdl_gpu_capture_file` 通過為準。

### 範圍外的觀察

- **GUI 測試偶發地圖黑屏**（使用者觀察）：以前就有，兩樹都會發生，和本 PR 無關。已記為 `N-gui-black-map-intermittent`，觀測任務排在 L2 之後。
- **`object_fps_pvp.worker` 失敗**：已知的 `N-worker-probe-stale`，修正 `d2844a5` 在記錄器分支，還沒進本分支。

## Architecture Delta

- **產品公開介面**：拿掉 `SdlGpuRenderDevice& RenderDevice()`，加上 `RenderDeviceInfo()`（S1(b) 的補充，D168 P2）。
- **Engine 契約**（`docs/architecture/input.md` 的 Contracts，Design Invariants 不變）：
  - `insideWindow` 依處理當下的視窗大小判定；
  - 本視窗的焦點喪失、按鍵、滑鼠鍵事件不去重。
- **產品的完成條件守衛**：
  - (a) `tests/object_fps_pvp/sdl_symbol_guard.py`；
  - (b) configure 結束時的連結檢查。
  - 兩者都是產品擁有，只在選擇本產品時啟用。
- **產品的突變工具**：加 `expect_build_failure`，預設行為不變。

## 結果（L2 之前）

- 產品、probe、產品測試直接呼叫 SDL：0 處。守衛 (a)(b) 生效，突變 g1～g4 都殺掉。
- `test` preset 81／81；`ctest -L pvp` 42／43，唯一失敗的是已知的 worker。
- L1：#573、#575、#577、#579、#582，都是四平台成功。
- L2（2026-10-23，session `pvp-v71-sdl-isolation-l2-20261023b`）：**通過**。
  - 20 次呼叫全部 P，沒有受干擾，沒有旗標。
  - G3：A − B 的差最大 0.32 ms（門檻 1 ms）。
  - G5：頭的實測 FPS 59.75～59.87，base 56.90～57.88，預測成立。
  - 詳細見宣告第 7.7 節。
  - 第一次 session 依使用者要求，在第一項開始前停下，沒有任何項目跑過。
- 記錄上的錯誤：`7ae0588` 的腳本把宣告清空後提交（先以寫入模式打開檔案再讀取），D170 的修改也一起遺失。之後的 commit 從 `3106b5b` 還原，並重新套用 D170 的修改。

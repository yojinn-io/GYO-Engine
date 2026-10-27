# Engine 追記：platform_sdl_concurrency 的逾時、重現嘗試與看門狗

日期：2026-10-27。分支 `claude/engine-sdl-concurrency-watchdog`。檔位 high。

**狀態：** flood-repro 沒有重現（依 §14.3 停在觀測）；concurrency-watchdog 完成（J1～J4 通過，J5 見 PR）。**原本的逾時原因仍未查明，不是「已修正」。**

相關文件：

- 前一次的同組測試修正：[2026_10_24_engine_ip3_log_bridge_handshake](2026_10_24_engine_ip3_log_bridge_handshake.zh-Hant.md)。
- 經過：[HANDOFF](../architecture/plans/input-and-present/HANDOFF.md) 的「IP-3／IP-4 規劃紀錄」。

## 現象

- run 40510515336 attempt 1（分支 `claude/trusting-joliot-f13b78`，`908308b3`），只有「Quick acceptance / toolchain / linux-x64」失敗：`platform_sdl_concurrency` 60.07 s 逾時。
- 同一個 commit 的「L1 / linux-x64」跑同一個測試 2.29 s 通過。兩邊的 runner 映像（20261021.618）、preset（`ci-linux`）、逐一執行都相同。
- 逾時的輸出只有 SDL 的 3 行 `Couldn't open /dev/input/...: Permission denied`，看不出三個案例裡是哪個卡住。

## 已知 issue 與官方更新（2026-10-27 查）

- libsdl-org/SDL 沒有符合候選 A（佇列被灌滿時 `SDL_PollEvent` 的一輪不結束）或候選 B（監看鎖、日誌鎖的搶鎖飢餓）的 issue。相近的 #11319 是呼叫端 ABI 不一致，#13272 是 SDL2 Wayland。
- 3.4.0 到 3.4.18、main：`SDL_AddEvent`、`SDL_PumpEventsInternal`、`SDL_WaitEventTimeoutNS`、`SDL_PushEvent` 只有 #15485（`SDL_WaitEvent(NULL)`）的修改；監看鎖改為全域遞迴鎖只在 main（#15709）。升級 SDL 不會改變這段行為。

## flood-repro（候選 A 的重現，沒有重現）

環境：Docker 的 `ubuntu:24.04`、g++-14 14.2.0、CMake 3.31.6、`--cpus=4`；原始碼 `908308b3`；相依套件掛載本機的原始碼。事前宣告與判讀表經使用者核准。

收支：一致（試驗 1 的 40 次：試著推的數＝成功＋被拒；成功＝取出＋結束時留在佇列；有 watch 時回呼次數＝試著推的數）。

| 試驗 | 內容 | 結果 |
|---|---|---|
| 1 | 只連 SDL 的小程式：推送執行緒不停 `SDL_PushEvent`，主執行緒每 20 ms 跑一輪 `while (SDL_PollEvent)`，50 輪；有 watch、沒有 watch 各 20 次。單輪超過 10 s 算卡住 | 40 次都沒卡住。單輪中位數約 31 ms、最長 147 ms，每輪 49581～66483 個事件。每次約 1000 萬次推送、約 700 萬次被拒 |
| 2 | 三個案例各自以 `-tc=` 跑 20 次，60 s 逾時 | 60 次全部通過。EventWatch 案例 0.25～1.19 s，日誌橋兩個案例 0.01～0.03 s |

判讀：依宣告「兩個都不卡」，照 §14.3 記錄證據、補觀測手段後停下。

**範圍的限界**（之後相關的事前宣告要寫）：

- 容器跑在 Mac 的 Docker 虛擬機上，排程和 CI 不同。
- 容器沒有 `/dev/input`、udev 裝置。Linux 上 SDL 的 dummy 視訊驅動初始化時會開 evdev（`SDL_nullvideo.c`），CI runner 有這些裝置但沒有權限（那 3 行輸出），容器走不到這條路徑。dummy 驅動的 pump 不輪詢 evdev。
- 沒有照 CI 的順序先跑前面的測試。

## concurrency-watchdog（觀測手段）

只改 `tests/common/platform_sdl`：

- 新檔 `ConcurrencyWatchdog.hpp`／`.cpp`：doctest 的 listener 在每個案例開始、結束時寫一行到 stderr 並 flush；案例超過 10 s 時，看門狗執行緒每 10 s 回報最後的標記、輪次、距離標記多久、兩個計數，再另起一行印 SDL 佇列的事件數（讀佇列卡住時這一行不會出現）。不讓測試失敗，不呼叫 SDL 或 Engine 的日誌。
- 三個案例：主執行緒在各步驟加標記（一次原子寫入）；日誌執行緒計數寫出的行，建橋執行緒計數做好的橋，推送端計數推送與被拒。

原因：doctest 寫 stdout，被 CTest 接管時是整塊緩衝，逾時被殺就全部丟失（CI 的輸出只剩 SDL 寫到 stderr 的行）。

### 事前宣告的判定（使用者核准）

| 判定 | 結果 |
|---|---|
| J1 判定不變 | 通過。diff 裡刪除的只有 CMake 的原始碼清單、推送端的 `SDL_PushEvent`（改為計數失敗）、日誌執行緒的單行 lambda；沒有 CHECK、REQUIRE、時限、CTest 屬性的改動 |
| J2 觀測路徑能用（每個案例注入無限等待，經 CTest 60 s；要有開始行、和注入位置相同的最後標記、正好 5 次回報） | Mac 3／3。容器第 1 次 2／3（見下）；改注入點後重跑容器 twothreads 1／1 |
| J3 正常時不受影響（20 次全過；中位數不超過加之前的 1.5 倍） | Mac 2.21 → 2.38 s（1.08 倍）20／20；容器 0.55 → 0.74 s（1.35 倍）20／20 |
| J4 突變（D184：殺這三個案例的突變） | m-race、m-order 2／2 殺掉 |
| J5 四平台 L1 | 見 PR |

### J2 第 1 次不成立（照記，未覆蓋）

- 容器的 twothreads：36.98 s 被 CTest 記為「Subprocess killed」，只有 3 次回報（回報數與經過時間一致，標記正確）。
- 推斷原因（未驗證）：注入點在建橋執行緒持有橋的期間。橋存在時，4 條日誌執行緒的每一行都進測試的 `Capture`，存進 `received`，30 s 時已有 6700 萬行，8 GB 的 Docker 虛擬機記憶體不足而被殺。Mac 撐到 60 s。屬於注入點的設計錯誤，不是看門狗的問題。
- 使用者決定：把注入點移到建橋迴圈的開頭（這條執行緒不持有橋），只重跑容器這一組。重跑通過（60.01 s 逾時、5 次回報、最後標記「joining the makers」）。
- 附帶觀察：如果真的在橋存在時卡住，這個案例可能在 60 s 前因記憶體被殺；看門狗在那之前已 flush 的回報仍會留下。

注入腳本與紀錄在本會話 scratchpad（`wd/inject-v1.py`、`wd/inject.py`、`wd/j2-*.log`、`wd/j3-*.log`、`wd/j4-mutants.log`、`flood/`），不提交。

## Architecture Delta

無。Engine 程式不動；新增的是 Engine 測試的支援檔，owner 是 `tests/common/platform_sdl`。

## 下一步

下一次 CI 再出現這個逾時時，看門狗的回報會指出是哪個案例、哪一步、執行緒是否還在前進、SDL 佇列是否讀得到。依那時的證據再決定是否重排重現批。

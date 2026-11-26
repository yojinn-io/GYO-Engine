# 2026-11-26 Gateway 呼叫紀錄的 `took` 測試在 Windows 偶發失敗（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。大地圖卡發現、GYO2 主會話轉來，遊戲流程框架任務卡處理。作者 Opus 5.5／high。

## 現象

`tests/object_fps_pvp/client/ClientTests.cpp` 的「each exchange is kept for the log」（#355 加的）：`CHECK(calls[0].took > 0 && calls[1].took > 0 && calls[2].took > 0)`（`[pvp-gw-calls-took]`）在 #356 head 3de9dca0 的 L1（run 45719569576）只有 windows-x64 失敗；同一個 head 重跑通過。

## 原因

- `took` 不是讀時鐘得來的：`GatewayClient` 不自己讀時間，`took = now − going.started`，兩個值都是呼叫端傳給 `Update(now)` 的時間（`games/object_fps_pvp/src/client/Gateway.cpp` 的 `Note`）。Client 裡傳的是幀時間。
- 一次 `Update` 裡會先 `Start`（連線）再 `Poll`（送出、收回應）。本機 loopback 上 FakeGateway 的執行緒如果在這一次 `Poll` 結束前就回完，交換在開始它的那次 `Update` 裡結束，`took` 就是 0。這是「這次交換花不到一次 Update」的正確值，不是時鐘錯。
- 測試原本用 `steady_clock`（MSVC 是 QueryPerformanceCounter，100 ns 刻度）取 `now`；所以大地圖卡推測的「Windows 時鐘刻度粗」不是原因。兩次 `Update` 之間一定隔著 `sleep_for(1ms)`，不會拿到同一個 `now`；只會發生在同一次 `Update` 裡。
- 結論：測試的前提「每一次交換都 > 0」不成立；產品沒有錯。

## 改了什麼

- 測試改用自己給的時間：每次 `Update` 前進 1 ms。用 FakeGateway 的 `Hold` 扣住 `GET /v1/rooms`，看到它被扣住之後再跑 20 次 `Update` 才放開；判定改成 `calls[1].took >= 20 × 1 ms`。
- 判定的意圖不變（`took` 是這次交換從開始到收到回應的時間，且確實寫進紀錄），而且比原來嚴：原本只要 > 0，現在要大於一個已知的下限，結果是確定的。`[pvp-gw-calls-waited]` 也一起變嚴：record 至少等了被扣住的那段時間。
- session 和 record 兩筆的 `took` 不再判定：它們可能在一次 `Update` 裡結束，0 是正確值；寫入的程式是同一個 `Note`，list 那筆已經證明它有被寫入。
- 原本沒有突變守這個判定；加了 `pvp-gw-calls-no-took`（`took` 一律寫 0）。

## 驗證

- 本機（Intel Mac）：這個案例連跑 5 次都過；`gyo_object_fps_pvp_client_tests` 全部 39 案通過。
- 突變 `pvp-gw-calls-no-took`、`-no-wait`、`-asked-overwritten`、`-not-kept`、`-timeout-not-kept`：5/5 killed。
- L1：見 PR。

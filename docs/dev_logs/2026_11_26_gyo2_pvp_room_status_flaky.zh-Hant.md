# 2026-11-26 房間狀態列測試在 macos-arm64 偶發失敗（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話轉來，遊戲流程框架任務卡處理。作者 Opus 5.5／high。

## 現象

#358 合併後的 gyo2 全量 CI（run 45776468319）只有 macos-arm64 失敗：`client: gateway: two presses of ready before an answer end as the second asks; a refusal of an action stays`。`tests/object_fps_pvp/client/ClientTests.cpp:1749` 的 `[pvp-room-problem-shown]` 不成立，狀態列還是「全員の準備がまだです」，沒有換成「Gateway: busy」。那次 `object_fps_pvp.client` 跑了 81.5 s，和其他測試平行執行。上一次全量 CI（45760829973）這個測試是綠的。

## 原因

- 房間每 0.75 s 讀一次（`RoomScene.cpp` 的 `kPollEvery`）。這個時間是 Client 的時間：測試的 `Setup` 每幀前進一個 step（15.625 ms），所以 0.75 s ＝ 48 幀，和機器快慢無關。
- 測試的窗口卻是牆上時間：`run(1600 ms)`。機器一忙，1600 ms 內的幀數就不夠。窗口結束時，改成會失敗之後的那一次讀取還沒被問、或還沒被回應，狀態列自然還是上一句。
- 本機（Intel Mac）量測：每 1600 ms 約 180 幀（約 2.8 s 的 Client 時間）。#358 之前（abcede61）和之後（bc9045e5）一樣，都是 179～183 幀；**不是 #358 造成的回歸**。
- 重現：在窗口的每幀加上 40 ms 的等待（模擬忙碌的執行機器），得到和 CI 一樣的失敗值。

## 改了什麼

- 三個窗口都改成數 FakeGateway 實際收到的房間讀取次數，上限 2000 幀；條件沒達成就 `REQUIRE` 失敗，不會空過。房間只在前面的呼叫都回應、處理完之後才送下一次讀取，所以「收到第 N 次讀取」就表示前面的回應已經被房間看過。
  - ready：兩次 ready 都回應之後，再收到 2 次讀取。
  - 拒絕要留著：start 之後收到 3 次讀取（拒絕和至少兩次讀取都已回應），再判定狀態列。
  - 讀取失敗要換掉：改成失敗之後收到 2 次讀取（第一次的 503 已回應），再判定狀態列。
- 判定的內容不變（拒絕留過兩次讀取、之後失敗的讀取換掉它）。原本在慢的機器上可能讀取一次都還沒回應就判定（拒絕那條會空過、失敗那條會誤判），現在一定是在讀取真的被處理過之後才判定。

## 驗證

- 本機：每幀加 80 ms 等待的情況下跑 2 次都通過；正常速度連跑 5 次都通過；client tests 39/39。
- 突變 `pvp-room-ready-count-ignored`、`pvp-room-refusal-cleared`、`pvp-room-problem-hidden`：3/3 killed。
- L1：見 PR。

# 2026-11-25 PvP 移植實作第 4 段（Engine 部分）：World 交出、live frame、自己的執行緒的 World（`v8-product-pvp-port`）

Owner：Engine（編排層、`sdl_window`）。GYO2 主會話開的任務卡（PvP 移植實作）。作者 Opus 5.5／high。
根據：[事前宣告](../architecture/target/v2/live-frame-sim-thread-declaration.zh-Hant.md)（第 4.1～4.3 節、第 5 節 J1～J4）；SDL 總宣告 R3、R5；編排層 O-13。主會話 2026-11-24：宣告寫完就動工，只在要改總宣告已定的內容或動 AGENTS §1 時停下。遊戲的部分（Client 拆三個 World、J5～J7）疊在第 5 段上，另一個 PR。

## 做了什麼

1. **`v8-product-pvp-port-lend`**（編排層）：`Product::Lend` → `WorldLease`（`RunRound`、`World`、`Name`）→ `Product::Return`。`RunFrame` 跳過交出的 World；`RunSchedule`、`ReplaceWorld`、`InsertSystems` 指名它回新的碼 `WorldLent`；`World(name)`、`Generating(name)` 指名它是斷言；遊戲初始化沒跑、已交出、Extract 的來源或讀者回 `LendRefused`；`~Product` 時有交出的是斷言。O-11：`RunOn` 多一個 `lease` 參數，交出的 World 壞掉時先寫租約的 `WorldFailure` 再以 release 設原子旗標；`Ended()` 以 acquire 看旗標、設了就 `Break`；`RequireNotRunning` 與 `Ended()` 的狀態掃描跳過交出的 World（避免跨執行緒讀）。
2. **`v8-product-pvp-port-live-frame`**（`sdl_window` 的 adapter）：event watch 照宣告 4.1 的條件跑「一幀」；live frame 裡 `ApplyWindowRequests` 不呼叫 adapter。`SDLWindowOptions::liveFrame`、`SDLWindowPump::SetLiveFrame`。
3. **`v8-product-pvp-port-sim-thread`**（`sdl_window` 的 runner）：`SDLWindowOptions::ownThreadWorlds`、`ownThreadIdle`；`LentWorlds` 開 `RoleThread`、等下一個固定步、所有出口停止＋join＋`Return`。`sdl_window` 私下連 `GYO::Threads`、`GYO::WorldTime`。
4. 文件：`orchestration.zh-Hant.md` O-13 補入口的形、`sdl.zh-Hant.md` 第 5 節補 live frame 與自己的執行緒的 World（都標草稿，最終文字在合併前核准）。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 253/253。
- `orchestration.lend` 4 案（J1：`RunFrame` 跳過、租約推一輪的進／輪／出各一次、`Return` 後恢復；交出的 World 在別的執行緒跑 200 輪、同時主執行緒 200 幀；J2：`WorldLent`、`LendRefused`（初始化前、兩次、Extract 的來源與讀者）；J3：別的執行緒上壞掉 → Product 的下一個入口 `WorldInterrupted`、`Return` 後 `Failure()` 是那個 World）。
- `sdl_window` 3 案（J4：live frame 只在泵裡、`data1 = 1`、這個視窗、不重入；runner 的 live frame 回錯 → 泵完後 runner 回 Failure；`ownThreadWorlds` 的 World 在主執行緒停 250 ms 時跑了 122 輪、之後交還）。
- 突變：`or-lend-` 4/4、`sw-live-*`＋`sw-own-thread-ignored` 5/5 killed。過程：`or-lend-flag-not-set`、`or-lend-schedule-allowed`、`or-lend-extract-allowed` 第一次是 wrong_failure（REQUIRE 在帶標籤的檢查之前中止、或對成功的值取 `error()`），把判定改成帶標籤的 `CHECK_MESSAGE`、交還誤給的租約後 killed；`sw-live-outside-pump`、`sw-live-reentered` 的改法會讓程式當掉（空指標、無限遞迴），expect 加「test case CRASHED」；`sw-live-failure-ignored` 第一次存活（壞掉的 World 讓下一個 `RunFrame` 也失敗，兩者等價），測試改成 E2（交換函式失敗一次、World 健康）後 killed。
- **沒驗**：真的 OS modal 迴圈（macOS live resize、Windows 拖標題列）；TSan。實機在遊戲部分做完後交使用者。

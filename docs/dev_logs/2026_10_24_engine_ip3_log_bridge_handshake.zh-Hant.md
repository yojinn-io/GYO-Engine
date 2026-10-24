# Engine IP-3 追記：並行日誌橋測試的開始握手

日期：2026-10-24。分支 `claude/engine-log-bridge-handshake`。檔位 high。

**狀態：完成**（本機 Red／Green、突變；L1 見下面）。

相關文件：

- 測試：`tests/common/platform_sdl/LogBridgeTests.cpp` 的「SdlLogBridge keeps every line under concurrent logging」（CTest `platform_sdl_concurrency`）。
- 原本的宣告：[ip3-declaration.md](../architecture/plans/input-and-present/ip3-declaration.md) 的 ip3e「並行日誌橋」一列與第 6 節。
- 同類的前例：[IP-4 dev_log](2026_10_22_engine_ip4.zh-Hant.md) 的「並行解構的測試在 windows-x64 失敗（#561）」。

## 現象

- GitHub Actions run 40052105536，job「Quick acceptance / toolchain / windows-x64」，分支 `claude/pvp-v71-outage-resync`，commit `1713191`。
- 失敗的是 `CHECK( pushedBy[thread] > 0 )`，8 條執行緒全部 `0 > 0`。逐執行緒守恆的 `CHECK_MESSAGE` 沒有失敗（0＋0＝0）。
- 那條分支沒有改 Engine 的程式；這個測試屬於 Engine。

## 原因歸類：測試自身

測試缺少「等日誌執行緒開始寫」的握手。主執行緒建立 8 條執行緒後，馬上跑 200 輪建橋、解構（每輪只有 `std::this_thread::yield()`），然後設 `stop`。執行緒啟動慢時，200 輪在任何一條執行緒寫出第一行之前就跑完了，執行緒一進迴圈就看到 `stop`，一行也沒寫。

橋本身沒有遺失行：重現時守恆檢查照樣通過，下面的突變也顯示守恆判定仍然有效。所以不是 `SdlLogBridge` 的 Defect。

## 重現（Red）

本機 macOS（Intel），`test` preset。

1. 未修改的測試：`ctest -R ^platform_sdl_concurrency$ --repeat until-fail:5`，5／5 通過（本機執行緒啟動快，看不到現象）。
2. 暫時在日誌執行緒的開頭加 `sleep_for(50 ms)`（模擬執行緒啟動慢），只改測試：3／3 失敗，每次都是 8 條執行緒 `CHECK( 0 > 0 )`，守恆不失敗。和 CI 的現象相同。

## 修正（Green）

只改測試檔：

- 每條執行緒寫出第一行時，把 `started` 加 1。
- 主執行緒等到 `started == 8`（最多 5 秒）才開始 200 輪。
- 加 `CHECK_MESSAGE(started == Threads, "every thread logs before the rounds start")`。用 CHECK 而不是 REQUIRE：失敗時仍會設 `stop`、join 所有執行緒，不會因為 `std::thread` 沒 join 而 terminate。

驗證：

| 條件 | 結果 |
|---|---|
| 注入 50 ms，加握手 | 20／20 通過 |
| 注入 6 秒（超過等待上限） | 失敗：`started == Threads` 1 次、`pushedBy > 0` 8 次（判定仍會抓到「執行緒沒有跑起來」） |
| 移除注入 | 20／20 通過；`ctest -R ^platform_sdl` 3／3 通過 |
| 突變：橋每 1000 行丟 1 行（暫時改 `SdlLogBridge.cpp`，已還原） | 失敗：守恆 `CHECK_MESSAGE` 4 條執行緒（守恆判定在握手後仍有檢出力） |

## 判定的變更（AGENTS §14.4）

`pushedBy > 0` 與逐執行緒守恆都沒有改。改的是前置條件（等執行緒開始），另外加了一條 CHECK。這個測試不是 Design Invariants 的守衛（DI 5 的守衛是 m10、m11），所以照「只收緊」的程序事後報告：

1. **守的性質**：每一行只到一個地方，不遺失、不重複（逐執行緒守恆）；每條執行緒確實參與（`pushedBy > 0`）。
2. **放寬的面**：對橋的實作沒有。執行緒在 5 秒內、但在 200 輪之後才開始的情況，以前失敗、現在通過；那種情況下橋沒有和任何執行緒並行，以前的失敗不是在判定橋（假失敗）。
3. **收緊的面**：以前，執行緒在 200 輪結束後、`stop` 之前才寫出幾行，也能通過 `pushedBy > 0`，卻沒有和橋的建立、解構重疊。現在每條執行緒在第一輪之前都已經在寫。另加「5 秒內全部開始」的 CHECK。
4. **其他守住的判定**：不適用（橋的實作沒有放寬面）。執行緒在 5 秒內沒有開始，仍然失敗（注入 6 秒的結果）。
5. **根據**：CI run 40052105536 的現象、本機注入延遲的 Red 3／3 與 Green 20／20、上面的突變。執行的條件與結果寫在本文件，沒有另存紀錄檔。

## L1

draft PR 的推送不跑 L1，手動觸發四平台 L1。結果見 PR 的說明。

## 觀察（未處理）

- 同檔的「SdlLogBridge leaves SDL's previous output after bridges made on two threads」也是 4 條日誌執行緒沒有握手。它的判定（結束後輸出還原、`SdlLogBridgeInstalled()` 為 false）不依賴日誌執行緒有沒有寫，所以不會假失敗；執行緒啟動慢時只是並行的壓力變小。這次不改。

## Architecture Delta

無。只改 Engine 的測試檔與文件。

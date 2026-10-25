# Engine os-files-write：寫入原語，消費端的 arena 讀取與 trace 寫檔改走 engine/io

日期：2026-10-26。**完成**：四平台 L1 通過（[run 40213916104](https://github.com/yojinn-io/GYO-Engine/actions/runs/40213916104)）。
計畫見 [os-files-write](../architecture/plans/os-subsystem/PLAN.md)（第 7.8 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-files-write」；開工前的決定是 D52。

## 做了什麼

- engine/os：
  - `FileWriter`：建立缺少的檔案，清空或附加，和 C++ 的檔案串流開法相同；其他程序可以同時讀寫；不緩衝。
  - `LineEndings::Native`：Windows 上把 `\n` 換成 `\r\n`，和文字模式的 C++ 串流寫出的位元組相同；其他平台不變。
  - `CreateDirectories`：和 `std::filesystem::create_directories` 相同。
- engine/io：`NativeFileSystem` 開放寫入與建目錄；新增 `NativeFileUri`。
- engine/asset：`AssetWatcher` 改經 engine/io 查檔案狀態。
- 消費端：arena 清單與場地的讀取、執行檔 SHA 的讀取、log 目錄的建立、shot／ingress trace 的寫檔、trace 檔名的存在檢查，都改經產品內的 `NativeFiles`（再經 engine/io）。產品的 allowlist 清空。
- 換行（D52）：Windows 維持 CRLF。這是 os.md Replacement Criteria 5 觸發後交使用者的決定；把換行當成 Data Contract 統一，延到消費端發行之後。

## 驗證與經緯

- 寫出的位元組：Engine 測試把每種模式寫出的位元組，和 C++ 檔案串流（文字模式或二進位）寫出的比對；產品測試把 shot／ingress trace 檔的位元組，和文字模式 `std::ofstream` 寫出的比對。四平台的 L1 都通過，Windows 上的 CRLF 也一樣。
- xhigh 審查（1 位）：沒有 blocker、major。修了 Windows `CreateDirectories` 的根目錄處理、`NativeTextFile` 成功 Close 後的狀態、新錯誤訊息的路徑編碼，補了審查列出的測試缺口。審查發現的路徑差異（file:// 規則帶來的 5 項）補進事前宣告（D53），之後另開 io-native-paths 讓規則對齊 OS（D54）。
- 突變：Engine 新增 os-w1～os-w10、io-w1～io-w4，本機能跑的都 killed；消費端相關的 32 條沒有過期，加上新增的 4 條 osfw-*，全部 killed。
- 深路徑：Engine 測試在超過 300 字元的路徑下建目錄、寫檔、讀回；產品測試從超過 300 字元的資料夾讀 arena 清單與場地。Windows 上舊的 `std::ifstream` 在 260 字元就停（事故本身），這次沒有在 Windows 上重跑舊碼看 Red。
- 我自己寫的 NativeFileSystem 測試有一個未定義行為（取兩個不同暫時字串的 begin 與 end），40 次裡失敗 6 次；修正後 60 次 0 失敗。這是測試的錯，不是實作。

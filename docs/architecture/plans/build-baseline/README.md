# 建置基線：平台與工具鏈的支援範圍（提案）

更新：2026-10-27。Owner：Engine（平台支援範圍、CI 結構、`engine/os` Windows 後端的實作規則）。
**狀態：提案，未實作。** 已決定的只有「文字編碼：程序自己決定 UTF-8」一節；其餘未決定。 細化與基線的確立交給之後的規劃會話（使用者 2026-10-27）。決定之後會改動支援範圍、CI 與 `engine/os` 的責任，屬於 Architecture Delta（AGENTS.md §3）。

## 來源

- 2026-10-27，在 Windows 機器上跑提出需求的消費端的 Windows 專用驗證（突變、`-s` 案例確認、實機啟動、主控台）。機器用本機 MinGW-Builds GCC 15.2.0 建置（`test` preset），ACP＝65001。CI 的 windows-x64 用 MSVC（`ci-windows` preset），所以下面的差異 CI 都看不到。
- 使用者的想法：以 Windows 為基準，Linux 靠 Steam Proton（主機平台另算）；分「產品的執行層」與「Engine 的建置層」兩個方向考慮。

## 兩個前提

1. Proton 只涵蓋 Linux（SteamOS／Steam Deck），不涵蓋 macOS。macOS 要另外決定：放棄，或維持原生。
2. Proton 不代表不用測。Valve 的做法：有原生 Linux 版就先測原生，表現差才改測 Windows 版跑 Proton；Proton、反作弊、引擎更新都可能造成回歸。驗證對象從「Linux 原生程式碼」變成「Windows 包在 Proton 上的行為」。

## 其他引擎的做法（參考）

- Godot：官方 Windows 二進位全部用 MinGW 建；文件建議發給玩家的包用 MinGW，開發時用 MSVC。MinGW 是正式的發行工具鏈。
- Unreal（原始碼公開）、O3DE：Windows 以 MSVC 為主，Linux、macOS 原生，另有行動平台。
- 大型引擎都保留 OS 抽象層（主機平台需要）；不少獨立遊戲只發 Windows 版、靠 Proton 上 Steam Deck。

## 執行層（產品）

- 客戶端：只發 Windows 包、Linux 靠 Proton 是可行的。條件是「Windows 包在 Proton 上跑」列為正式驗收（Steam Deck 或 CI 的 Wine 煙霧測試）。
- 伺服器型的服務（專用伺服器、閘道類服務）維持原生 Linux。
- `engine/os` 抽象層保留：主機平台需要它；Wine 的長路徑、UNC、ACP 行為和真 Windows 不完全一樣（未驗證）。

## 建置層（Engine）：支援矩陣草案

| Tier | 內容 | 要求 |
|---|---|---|
| 1 | Windows／MSVC（客戶端）、Linux／GCC 或 Clang（伺服器） | CI 必過、擋合併 |
| 2 | Windows 包跑在 Proton 上 | 定期或發版前驗證 |
| 2 或放棄 | Windows／MinGW、macOS 原生 | 要嘛進 CI，要嘛文件寫「不保證」 |

- 若保留 MinGW，建議的規則：**Windows 後端不用 `std::filesystem` 的路徑語意，一律走原生 API**；測試的 setup 與清理也一樣。

## 文字編碼：程序自己決定 UTF-8（使用者決定，2026-10-27）

使用者：「就用這個方式來處理。程序不應該由環境參數來決定。」

- **原則**：程序的行為不由執行環境的設定決定。Windows 系統的 ANSI code page（ACP）是玩家機器上的設定（多數是 950、932 等，「Beta：使用 Unicode UTF-8」預設關閉），引擎與產品的行為不能隨它改變。
- **做法**：Windows 的執行檔以 application manifest 宣告 `<activeCodePage>UTF-8</activeCodePage>`（Windows 10 1903 起），程序內的 ACP 固定為 65001，不論系統設定。
- **與現有做法的關係**：`engine/os` 一律走寬字元 API 的做法維持為主線（不依賴 Windows 版本與 manifest 是否嵌入）；manifest 是第二道防線，讓第三方函式庫與漏網的窄字元呼叫也是 UTF-8。目前 repo 沒有 Windows application manifest（[os-subsystem PLAN](../os-subsystem/PLAN.md) 第 233 行）。
- **對驗證的影響**：ACP 不再是測試環境的前提，任何 Windows 機器都能判定。
  - 記錄 ANSI code page 的測試（現在斷言 `GetACP() != 65001`）改成斷言程序的 ACP 是 65001；
  - 「UTF-8 換成 ANSI」類的突變（os-f4、p4、os-c6、pu-x5～x7）重新設計，例如加上「拿掉 manifest 會被抓到」的突變。2026-10-27 在系統 ACP＝65001 的機器上，這 6 條無法判定（os-f4 基準線失敗、其餘 5 條必然存活）。
- **未確認**：Proton／Wine 是否支援 `activeCodePage`；manifest 與 `longPathAware` 是否一起加（長路徑目前靠 `\\?\` 加寬字元 API，與登錄值 LongPathsEnabled 無關）。
- **歸屬**：實作屬於 OS 子系統（Runtime 契約與測試判定的變更，Architecture Delta）；本節只記錄方向。
- **細化**（2026-10-27，OS 子系統的會話）：OS 子系統的決策 D64 與批次草案 os-utf8-codepage，見 [os-subsystem PLAN](../os-subsystem/PLAN.md) 第 3.10 節（責任與檢查）、第 5 節的 Invariant 11 草案、第 7.18 節（批次）、第 9.2 節（Architecture Delta）、第 11 節的開放決策 16～23。

## 觀測到的證據（2026-10-27，MinGW 15.2、ACP＝65001）

1. libstdc++ 的 `std::filesystem::remove_all` 對超過 260 字元的 `\\?\` 路徑無限迴圈。gdb 堆疊：`remove_all` → `recursive_directory_iterator::__erase` → `remove` → `symlink_status`。不依賴專案的最小重現：376 字元的資料夾樹卡住超過 10 s，68 字元正常。踩到的地方是測試的清理：`tests/common/os/FilesTests.cpp:462`、`tests/common/core/io/UncProbe.cpp:175`（相關的基準線逾時，ctest 的 `io.unc_localhost`、`io.unc_127.0.0.1` 也逾時）。
2. libstdc++ 不把 `//server` 當成 UNC 的 root-name。`engine/os/src/windows/Files.cpp:617` 的 `ResolvedPath` 用 `lexically_normal()`，把 `//localhost/C$/...` 變成 `/localhost/C$/...`（Engine 程式碼）。測試對照用的 `weakly_canonical` 在 MinGW 上也和 MSVC 不同：UNC 變成 `C:/localhost/...`，DefineDosDeviceW 的磁碟代號保留 `Z:`，結尾的 `/` 被去掉。
3. libstdc++ 的 `create_directory_symlink` 在 Windows 上回 "Function not implemented"，所以 symlink 的案例全部提早返回。
4. `cmake --install --component <product>` 的包沒有附 MinGW runtime（`libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll`），只在 PATH 上有 MinGW 的機器能跑。
5. ACP＝65001 時，記錄 ANSI code page 的基準線測試失敗（`CHECK(::GetACP() != 65001)`），「UTF-8 換成 ANSI」類的突變必然存活（ANSI 就是 UTF-8）。

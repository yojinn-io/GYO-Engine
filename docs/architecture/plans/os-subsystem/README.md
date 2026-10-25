# OS 子系統：分批計畫與進度

更新：2026-10-25。Owner：Engine（新增 `engine/os`；`engine/io` 新增原生後端與路徑契約；`engine/time`、`engine/threads`、`engine/trace`、`engine/platform/sdl` 之後改呼叫 `engine/os`）。
**狀態：計畫已核准**（2026-10-25，D46）；下一步是 os-skeleton 與 io-contracts（可並行），等使用者說開始。本文件與 [PLAN](PLAN.md)、[HANDOFF](HANDOFF.md) 於 2026-10-25 建立。

Engine 目前沒有處理 OS 差異的 owner：

- 檔案：engine/io 的 `IFileSystem` 預告由「platform 側」提供原生實作，但從沒寫過；engine/io 不含任何 OS 程式碼，代表它從未被使用，也沒有對 OS 做相容。asset 另有職責重複的 `IAssetSource`，asset 與 trace 各自用 fstream 開檔，都沒處理 Windows 的路徑長度上限與路徑編碼。
- 等待與執行緒：`Waiter.cpp`、`RoleThread.cpp` 各自在同一檔內用 `#if` 分 OS，沒有 Design Invariants 可以對照。
- 產品與工具自己判斷 OS：執行檔路徑、CPU 時間、本地時間、結束訊號、命令列、原子存檔、系統字型目錄；有幾件同一知識寫了兩次。
- 路徑字串沒有編碼契約；`ParseUri` 會把 POSIX 的絕對路徑變成相對路徑。

本計畫新增 `engine/os`，把所有 OS 差異收攏進來，讓 Engine 的其他模組、產品與工具都不必考慮 OS。先讀 [批次計畫](PLAN.md)（架構樹、部件、守衛、批次、Architecture Delta、開放決策）和 [交接](HANDOFF.md)（決策、壓力摘要、研究事實）。

## 來源

- 提出需求的消費端在 Windows 實機發現：發行包解壓到深資料夾時，shader 路徑 271 字元，超過 MAX_PATH 260，開不了。證據：主 checkout 的 `build/target/_build/test/logs/pvp-v71-windows-lan-20261016/logs/clientA/logs/client-20261016-104011.log` 第 2 行。
- 使用者決定不只修路徑，而是整理 OS 子系統（D40）。
- 本夾的文件依 D12 匿名：只寫「提出需求的消費端」，不連結消費端的文件；程式路徑與證據路徑當作資料保留。

## 已定案方針（2026-10-25，使用者）

1. **D40**：目標是整理 OS 子系統；長路徑只是 Files 部件第一個觀測到的壓力。
2. **D41**：分層是 asset → io → os → base。
   - 原生的系統相關實作放在 `engine/os`，engine/io 呼叫 engine/os。engine/os 只依賴 engine/base 與標準庫，API 只用中立型別。
   - engine/io 保留中立抽象，另外新增不含 OS 標頭的原生 `IFileSystem` 後端。
   - Waiter、RoleThread、產品與工具也照同一模式改呼叫 engine/os。
3. **D42**：asset 的讀檔改經 engine/io 的 `IFileSystem`；`NativeFileAssetSource` 暫時保留類別名與 API，當薄轉接；`IAssetSource` 的去留另行決定。
4. **D43**：Engine 字串裡的路徑現在就改成 UTF-8；產生 resolver 根目錄的呼叫端，和 asset 改走 IFileSystem 放在同一個變更。
5. **D44**：現在就修 `ParseUri` 的 file:// 規則，file:// 與原生絕對路徑只有一種對應。
6. **D45**：產品任務和架構分開。
   - 本計畫只交付結構與契約（A1～A3 與之後的部件）。
   - 長路徑的行為修正與驗收（P1～P5）由消費端的發行任務在 A1～A3 完成後進行。
7. **D46**：核准 PLAN 第 1 版。
   - 開放決策 1：A1～A3 在消費端發行前完成，其他全部排在發行之後。
   - 開放決策 14：UNC 根目錄這一輪暫不支援；io.md 定義 UNC 的 Uri 對應並寫明這個限制，P1 不寫 `\\?\UNC\` 分支。
   - 開放決策 15：A1～A3 併進消費端的發行線 PR（兩個任務，一個 PR）。
   - 同意 A1～A3 的 Delta：新 target `GYO::Os`、os.md 與 io.md 的 Design Invariants 節。

## 架構樹（目標狀態，詳見 PLAN 第 2 節）

```text
engine/base
engine/os  ［新，GYO::Os：OS 差異的唯一 owner］
├── OsError                    os-skeleton
├── Files（讀取／寫入）         os-files-read／os-files-write；Windows 長路徑＝消費端 P1
├── System（系統字型目錄）     os-tool-files
├── Process                    os-process-info
├── Console（命令列、輸出、結束要求）  os-console-shutdown
├── Thread                     os-threads-backend
├── Wait                       os-wait-backend
└── Net（條件式）              os-net-errors
engine/io（中立抽象＋NativeFileSystem 後端，不含 OS 程式碼）   io-contracts、os-files-read
engine/asset → io；engine/time、threads、trace → os；engine/platform/sdl 不動（PathFromUtf8 移到 os）
```

## 進度

| 批次 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|
| os-plan-docs 計畫 | medium | 完成（2026-10-25 核准，D46；[dev_log](../../../dev_logs/2026_10_25_engine_os_plan_docs.zh-Hant.md)） | 本夾三份文件與 dev_log |
| os-skeleton（A1） | high | 本機 L1 完成（2026-10-25；四平台 L1 待推送；[dev_log](../../../dev_logs/2026_10_25_engine_os_skeleton.zh-Hant.md)） | engine/os、GYO::Os、OsError、os.md、守衛（Engine 範圍）、突變 runner 擴充 |
| io-contracts（A2） | high；字串規則局部 xhigh；契約審查 ultracode（先問） | 未開始 | io.md、UTF-8 路徑、file:// 對應、ParseUri／ToString／MountTable 修正、IoError detail |
| os-files-read（A3） | high；UTF-8↔UTF-16 與錯誤對應局部 xhigh；公開介面 1 位 xhigh 審查（先問） | 未開始 | engine/os 的唯讀檔案原語、engine/io 的唯讀 NativeFileSystem |
| （消費端 P1～P5） | 由消費端決定 | 等 A1～A3 | 長路徑行為、錯誤記錄、asset 接線、UTF-8 呼叫端、驗收（不屬於本計畫） |
| os-guard-wide（A1b） | medium；規則調整 high | 未開始 | 守衛擴到產品、工具、產品測試、驗收程式 |
| os-process-info | high | 未開始 | 執行檔路徑、CPU 時間、本地時間 |
| os-console-shutdown | xhigh | 未開始 | UTF-8 命令列、主控台輸出、結束要求（★ Windows 行為變更） |
| os-files-write | high | 未開始 | 寫入原語、消費端的 log／arena／trace 寫檔 |
| os-tool-files | high | 未開始 | 編輯器的原子存檔、路徑比較、系統字型目錄 |
| os-trace-writers | high | 未開始 | TraceSink 經 engine/os（開放決策 2） |
| os-path-utf8-sweep | high | 未開始 | 剩下的窄字串路徑、PathFromUtf8 移到 engine/os |
| time-threads-invariants | medium | 未開始 | time.md、threads.md 的 Design Invariants |
| os-wake-record | medium | 未開始 | 晚醒量測程式合回、四平台基準 |
| os-threads-backend | high；Start 順序局部 xhigh | 未開始 | RoleThread 的 OS 呼叫搬進 engine/os |
| os-wait-backend | xhigh；1 位 xhigh 審查（先問） | 未開始 | Waiter 的 OS 後端搬進 engine/os，ABAB L2 |
| os-net-errors | high | 條件式 | socket 錯誤碼的中立分類（開放決策 7） |
| os-close | medium | 未開始 | allowlist 清空、文件同步、完整突變 |

```text
os-plan-docs ─┬─→ os-skeleton ─┐
              ├─→ io-contracts ┴─→ os-files-read ─→［消費端 P1～P5］─→ os-files-write ─→ os-tool-files ─┐
              │                                                         ├─→ os-trace-writers           │
              │   os-skeleton ─→ os-guard-wide ─→ os-process-info ─→ os-console-shutdown ──────────────┴─→ os-path-utf8-sweep
              ├─→ time-threads-invariants ─┐
              └─→ os-wake-record ──────────┴─→ os-threads-backend ─→ os-wait-backend
                                                                     （全部完成）─→ os-close
```

- 關鍵路徑（消費端的修正之前）：os-skeleton 與 io-contracts 並行，接 os-files-read，約 20～29 h 工時。
- 整條線：約 100～150 h。
- 依 D46：A1～A3 以外都排在消費端發行之後；A1～A3 併進消費端的發行線 PR。

## 尚待回答的開放決策

PLAN 第 11 節的 1、14、15 已由 D46 決定。其餘（2、4、6～13）在對應批次開始前回答。

## 執行規則

- 每次只執行使用者指定的批次；commit 與 PR 用日語。
- 每批開始、里程碑、停止時，更新本表、[HANDOFF](HANDOFF.md) 與 dev_log（`docs/dev_logs/YYYY_MM_DD_engine_os_<批次>.zh-Hant.md`），然後停止。
- 主對話檔位由使用者決定；ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意（D8）。
- Engine 的驗收只驗 Engine 的目標，不以特定產品的存在為前提（AGENTS §7）；消費端的檢查屬於消費端的批次。
- 計時相關的 L1 不設比結構條件更嚴的時間上界；精度只在 L2 量。
- 每批同步更新未啟用產品的遷移清單（[foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md)，加「OS」節）。
- 每批只重跑相關突變，PR 線合併前在背景完整跑一次（消費端 D184 的作法）。
- 出現非預期回歸、範圍擴大，或需要新的第三方依賴、SDL 邊、往上的依賴邊時，停下回報並重新規劃。

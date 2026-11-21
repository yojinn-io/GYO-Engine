# 事前宣告：core/os 的平台實作照 `src/<平台>/` 分檔（`v8-arch-gyo2-os-src-platform`）

更新：2026-11-21（修訂 0）。Owner：架構（os-wait 卡接手；使用者：「這個你要處理，你本身就是os的實作者。」）。
**狀態：草稿，待使用者核准。核准之前不改 `engine/` 與 `tests/`。**

## 1. 做什麼

使用者 2026-11-21 定的規則：core/os 每個平台的實作放在 `src/<平台>/`。`v8-arch-gyo2-os-wait` 的 `WakeSignal` 已照這個規則（`src/macos`、`src/windows`、`src/linux`）。core/os 裡還有三個檔是規則之前的寫法，在一個檔裡用 `#if` 分平台：

| 檔案 | `#if` 分的是什麼 |
|---|---|
| `src/thread/Thread.cpp`（71 行） | `SetCurrentName`、`SetCurrentPriority` 兩個函式，各分 macOS／Windows／Linux／其他四支，三個平台的呼叫完全不同 |
| `src/posix/Process.cpp`（112 行） | `ExecutablePath`：Linux 讀 `/proc/self/exe`，macOS 用 `_NSGetExecutablePath`＋`realpath`；其餘函式兩個平台共用 |
| `src/posix/Files.cpp`（440 行） | 三處：`stat` 的修改時間欄位名（`st_mtimespec`／`st_mtim`）、`FontDirectories` 的候選目錄、`DefaultCaseSensitivePaths`；其餘約 420 行兩個平台共用 |

這一批把這些 `#if` 拿掉，平台不同的部分搬進各平台的檔。只搬，不改行為。

## 2. 拆法

- **`Thread.cpp`**：拆成 `src/macos/Thread.cpp`、`src/windows/Thread.cpp`、`src/linux/Thread.cpp`，刪掉 `src/thread/`。三個平台沒有共用的程式，所以整個函式各放一份。
- **`src/posix/` 留著，當 macOS 和 Linux 共用的 POSIX 程式**；只把兩者不同的函式搬出去：
  - `ExecutablePath` → `src/macos/Process.cpp`、`src/linux/Process.cpp`；
  - `FontDirectories`、`DefaultCaseSensitivePaths`，以及修改時間的欄位（一個內部函式 `ModifiedTimespec(const struct stat&)`，宣告在 `src/posix/PosixPlatform.hpp`）→ `src/macos/Files.cpp`、`src/linux/Files.cpp`。
  - 之後 `src/posix/` 裡沒有 `#if`。
- CMake：Windows 選 `src/windows/*`；macOS 選 `src/posix/*`＋`src/macos/*`；其他選 `src/posix/*`＋`src/linux/*`（`src/linux` 照舊是 `else()`）。

**退掉的拆法**
- R1：`src/posix/` 也拆掉，整份複製進 `src/macos`、`src/linux`。約 420 行的共用程式變成兩份，改一處要記得改兩處；正是 os.md RC7「per-OS files are mostly duplicates」說的情況。
- R2：小差異（欄位名、一行常數）留 `#if`，只搬大函式。libuv 的 `src/unix/` 就是這樣（共用檔裡留小的 `#ifdef`）。退掉的理由：使用者的規則是平台實作放 `src/<平台>/`，留下 `#if` 就有兩種寫法並存；搬出去的代價只是一個三行的內部函式。
- 參考：libuv 的 `src/unix/`（共用）＋`linux.c`、`darwin.c`（各平台）是同一種分法；`src/posix` 對應它的 `src/unix`。

**拿掉的分支**：原本的 `#else`（既不是 macOS、Windows、也不是 Linux）在 `Thread.cpp` 回 `false`／只接受 Normal，在 `ExecutablePath` 回 `Unsupported`。CMake 只選三個平台的檔，這些分支沒有平台會走到，一起拿掉。GYO 支援的平台是 Windows、Linux、macOS。

## 3. Architecture Delta（AGENTS §3）

1. 目的：使用者 11-21 的規則（平台實作放 `src/<平台>/`）與使用者指派（「這個你要處理」）。
2. 問題：同一個元件裡兩種寫法並存（`WakeSignal` 分檔、`Thread`／`Process`／`Files` 用 `#if`）。
3. 邊界：只動 `engine/core/os/src/` 的內部檔案；公開標頭、API、連結都不變。
4. 影響：`engine/core/os`（src 與 CMakeLists）、os-subsystem 的突變清單、`docs/architecture/os.md` 的兩處路徑。沒有 Product、Tool、其他引擎單位受影響。
5. 依賴方向：不變。
6. Ownership：不變。
7. 為什麼沒有更小的做法：規則要的就是這個；R2 更小，但留兩種寫法。

這是 core/os 的內部構造，上面疊的東西（time、threads、io、logging）只看公開 API，不受影響，所以照應用 Architecture 處理（AGENTS §2）。

## 4. 測試、守衛、突變

- 行為不變，既有測試的判定都不改。
- `os.boundary_guard`：新檔都在 `engine/core/os` 裡，不在掃描範圍；准許清單不變。
- 突變（os-subsystem 清單）的 find 跟著搬，判定不變：
  - `ti-x4`、`th-x3`～`th-x7`（`Thread.cpp` → 各平台的 `Thread.cpp`）；
  - `os-p6-executable-path`（→ `src/macos/Process.cpp`）；
  - `os-x6-macos-font-system-dir-missing`（→ `src/macos/Files.cpp`）。
  - 碰到的補 `guards`。不加新突變：沒有新的出錯點（§14.5）。
- 驗收：macOS 本機全量 ctest；L1 三平台（Linux、Windows 的檔只在 CI 編得到）；相關突變在本機跑（只在 Linux、Windows 跑的照舊 skip）。

## 5. 文件

- `os.md` Contracts 的 Thread 段：路徑改成 `src/<平台>/Thread.cpp`，拿掉「One source with per-OS branches」。
- `engine/core/os/CMakeLists.txt` 開頭的註解。
- dev_log 同一個變更。

## 6. 批次、檔位與模型

一批。下段（不碰 Design Invariants、協議、資料形式、公開介面）。實作 Opus 5.5／high；審查 Fable／high（唯讀，不同模型）。估時約 1.5～2 小時，加上 L1 的等待。一個 PR 向 `gyo2`，標題 `refactor(os): core/os の平台実装を src/<平台>/ に分ける（v8-arch-gyo2-os-src-platform）`。

## 7. 要你核准的

1. 拆法（第 2 節）：`src/posix/` 留作 macOS／Linux 共用，只搬不同的部分；退掉 R1（整份複製）與 R2（小差異留 `#if`）。
2. 拿掉沒有平台會走到的 `#else` 分支。

# 事前宣告：core/os 的平台實作集中到 `sys/<平台>/`（`v8-arch-gyo2-os-src-platform`）

更新：2026-11-21（修訂 1）。Owner：架構（os-wait 卡接手；使用者：「這個你要處理，你本身就是os的實作者。」）。
**狀態：草稿，待使用者核准。核准之前不改 `engine/` 與 `tests/`。**

經過：
- 修訂 0：照「`src/<平台>/`」拆三個 `#if` 檔。
- 修訂 1（這一份）：使用者：「你建一個sys ，把3個平台的代碼放進去/core/os/sys/windows ，這樣的」。所有平台的程式集中到一個 `sys/` 目錄底下，每個平台一個子目錄（第 2 節）。和 Rust 標準庫的 `library/std/src/sys/`（平台層集中在 `sys/` 底下，再按平台分）是同一種分法。

## 1. 做什麼

使用者 2026-11-21 定的規則：core/os 每個平台的實作放在 `src/<平台>/`。`v8-arch-gyo2-os-wait` 的 `WakeSignal` 已照這個規則（`src/macos`、`src/windows`、`src/linux`）。core/os 裡還有三個檔是規則之前的寫法，在一個檔裡用 `#if` 分平台：

| 檔案 | `#if` 分的是什麼 |
|---|---|
| `src/thread/Thread.cpp`（71 行） | `SetCurrentName`、`SetCurrentPriority` 兩個函式，各分 macOS／Windows／Linux／其他四支，三個平台的呼叫完全不同 |
| `src/posix/Process.cpp`（112 行） | `ExecutablePath`：Linux 讀 `/proc/self/exe`，macOS 用 `_NSGetExecutablePath`＋`realpath`；其餘函式兩個平台共用 |
| `src/posix/Files.cpp`（440 行） | 三處：`stat` 的修改時間欄位名（`st_mtimespec`／`st_mtim`）、`FontDirectories` 的候選目錄、`DefaultCaseSensitivePaths`；其餘約 420 行兩個平台共用 |

這一批把這些 `#if` 拿掉，平台不同的部分搬進各平台的檔。只搬，不改行為。

## 2. 拆法

### 2.0 目錄（修訂 1）

```text
engine/core/os/
├── include/engine/os/   公開標頭（不變）
└── src/
    ├── common/          和 OS 無關、每個平台都編的程式（不變）
    └── sys/             平台的程式，全部在這裡
        ├── windows/     現在的 src/windows/（Console、Files、Process、Wide、WakeSignal）＋Thread
        ├── macos/       現在的 src/macos/（WakeSignal）＋Thread、Process 與 Files 的 macOS 部分
        ├── linux/       現在的 src/linux/（WakeSignal）＋Thread、Process 與 Files 的 Linux 部分；CMake 的 else()
        └── posix/       macOS 與 Linux 共用的 POSIX 程式（現在的 src/posix/，拿掉 #if 之後）
```

- **`sys/` 放在 `src/` 底下（建議）**，不放在 `engine/core/os/sys/`（和 `src/`、`include/` 並列）。理由：OS 邊界守衛（`tests/common/os/os_boundary_guard.py` 的 `OS_SOURCES = "engine/core/os/src/"`）只豁免 `src/` 底下的檔；放在外面要改守衛的豁免範圍（§14.4 的判定變更，範圍等同、沒有放寬面，但仍要你核准）。放在 `src/sys/` 守衛不用動。你說的是 `/core/os/sys/windows`；要放在 `src/` 外面的話，我照做並改守衛（第 7 節的決定 1）。
- `posix/` 不是一個平台，但它是只有 macOS 與 Linux 用的平台程式，所以也放在 `sys/` 底下（Rust 的 `sys/pal/unix` 同樣是 macOS、Linux 共用）。`common/` 和 OS 無關，留在 `sys/` 外面。
- 平台檔 include 共用標頭的相對路徑跟著改（`../common/` → `../../common/`）。

- **`Thread.cpp`**：拆成 `sys/macos/Thread.cpp`、`sys/windows/Thread.cpp`、`sys/linux/Thread.cpp`，刪掉 `src/thread/`。三個平台沒有共用的程式，所以整個函式各放一份。
- **`sys/posix/` 當 macOS 和 Linux 共用的 POSIX 程式**；只把兩者不同的函式搬出去：
  - `ExecutablePath` → `sys/macos/Process.cpp`、`sys/linux/Process.cpp`；
  - `FontDirectories`、`DefaultCaseSensitivePaths`，以及修改時間的欄位（一個內部函式 `ModifiedTimespec(const struct stat&)`，宣告在 `sys/posix/PosixPlatform.hpp`）→ `sys/macos/Files.cpp`、`sys/linux/Files.cpp`。
  - 之後 `sys/posix/` 裡沒有 `#if`。
- CMake：Windows 選 `sys/windows/*`；macOS 選 `sys/posix/*`＋`sys/macos/*`；其他選 `sys/posix/*`＋`sys/linux/*`（`sys/linux` 照舊是 `else()`）。

**退掉的拆法**
- R1：posix 也拆掉，整份複製進 `sys/macos`、`sys/linux`。約 420 行的共用程式變成兩份，改一處要記得改兩處；正是 os.md RC7「per-OS files are mostly duplicates」說的情況。
- R2：小差異（欄位名、一行常數）留 `#if`，只搬大函式。libuv 的 `src/unix/` 就是這樣（共用檔裡留小的 `#ifdef`）。退掉的理由：使用者的規則是平台實作放 `src/<平台>/`，留下 `#if` 就有兩種寫法並存；搬出去的代價只是一個三行的內部函式。
- 參考：Rust 標準庫 `library/std/src/sys/`（平台層集中，`pal/unix` 給 Unix 系共用、`pal/windows` 給 Windows）；libuv 的 `src/unix/`（共用）＋`linux.c`、`darwin.c`（各平台）。`sys/posix` 對應它們的 unix。

**拿掉的分支**：原本的 `#else`（既不是 macOS、Windows、也不是 Linux）在 `Thread.cpp` 回 `false`／只接受 Normal，在 `ExecutablePath` 回 `Unsupported`。CMake 只選三個平台的檔，這些分支沒有平台會走到，一起拿掉。GYO 支援的平台是 Windows、Linux、macOS。

## 3. Architecture Delta（AGENTS §3）

1. 目的：使用者 11-21 的規則（平台實作放 `src/<平台>/`）與使用者指派（「這個你要處理」）。
2. 問題：同一個元件裡兩種寫法並存（`WakeSignal` 分檔、`Thread`／`Process`／`Files` 用 `#if`）。
3. 邊界：只動 `engine/core/os/src/` 的內部檔案（搬進 `src/sys/`）；公開標頭、API、連結都不變。
4. 影響：`engine/core/os`（src 與 CMakeLists）、os-subsystem 的突變清單、`docs/architecture/os.md` 的兩處路徑。沒有 Product、Tool、其他引擎單位受影響。
5. 依賴方向：不變。
6. Ownership：不變。
7. 為什麼沒有更小的做法：規則要的就是這個；R2 更小，但留兩種寫法。

這是 core/os 的內部構造，上面疊的東西（time、threads、io、logging）只看公開 API，不受影響，所以照應用 Architecture 處理（AGENTS §2）。

## 4. 測試、守衛、突變

- 行為不變，既有測試的判定都不改。
- `os.boundary_guard`：新檔都在 `engine/core/os/src/` 底下，豁免範圍不變；准許清單不變。它的自我測試用合成的路徑（`src/posix/...`），不受影響。（若 `sys/` 放在 `src/` 外面，這一條要改，見 2.0。）
- 突變（os-subsystem 清單）：`file` 在 `src/windows`、`src/posix`、`src/macos`、`src/linux` 的約 70 個，路徑改成 `src/sys/...`（機械式改寫）。find 跟著函式搬家的，判定不變：
  - `ti-x4`、`th-x3`～`th-x7`（`Thread.cpp` → 各平台的 `Thread.cpp`）；
  - `os-p6-executable-path`（→ `src/macos/Process.cpp`）；
  - `os-x6-macos-font-system-dir-missing`（→ `src/macos/Files.cpp`）。
  - 碰到的補 `guards`。不加新突變：沒有新的出錯點（§14.5）。
- 驗收：macOS 本機全量 ctest；L1 三平台（Linux、Windows 的檔只在 CI 編得到）；相關突變在本機跑（只在 Linux、Windows 跑的照舊 skip）。

## 5. 文件

- `os.md` Contracts 的 Thread 段：路徑改成 `src/sys/<平台>/Thread.cpp`，拿掉「One source with per-OS branches」；os.md 其他提到 `src/<os>` 的地方一起改。
- `engine/core/os/CMakeLists.txt` 開頭的註解。
- dev_log 同一個變更。

## 6. 批次、檔位與模型

一批。下段（不碰 Design Invariants、協議、資料形式、公開介面）。實作 Opus 5.5／high；審查 Fable／high（唯讀，不同模型）。估時約 1.5～2 小時，加上 L1 的等待。一個 PR 向 `gyo2`，標題 `refactor(os): core/os の平台実装を src/sys/<平台>/ に集める（v8-arch-gyo2-os-src-platform）`。

## 7. 要你核准的

1. `sys/` 的位置（2.0）：放在 `src/sys/`（建議，守衛不用改），或照字面放在 `engine/core/os/sys/`（要改守衛的豁免範圍）。
2. 拆法（第 2 節）：`sys/posix/` 當 macOS／Linux 共用，只搬不同的部分；退掉 R1（整份複製）與 R2（小差異留 `#if`）。
3. 拿掉沒有平台會走到的 `#else` 分支。

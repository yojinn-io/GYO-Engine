# 事前宣告：core/os 的平台實作集中到 `sys/<平台>/`（`v8-arch-gyo2-os-src-platform`）

更新：2026-11-21（修訂 2）。Owner：架構（os-wait 卡接手；使用者：「這個你要處理，你本身就是os的實作者。」）。
**狀態：草稿，待使用者核准。核准之前不改 `engine/` 與 `tests/`。**

經過：
- 修訂 0：照「`src/<平台>/`」拆三個 `#if` 檔。
- 修訂 1：使用者：「你建一個sys ，把3個平台的代碼放進去/core/os/sys/windows ，這樣的」。所有平台的程式集中到一個 `sys/` 目錄底下，每個平台一個子目錄（第 2 節）。和 Rust 標準庫的 `library/std/src/sys/`（平台層集中在 `sys/` 底下，再按平台分）是同一種分法。
- 修訂 2（這一份）：使用者：「posix 能不能用一個設計模式放在hpp中。實現代碼依然放在對應的macos、linux中。嚴格來說抽象就是這樣一回事。」「posix 就不在需要了。」「你認為我為何和你提設計模式？」——macOS 與 Linux 共用的程式不再是一個 `posix/` 目錄，而是用 Template Method 寫成 hpp 裡的骨架（寫一次）；macOS、Linux 各自提供不同的步驟，並在自己的 `.cpp` 裡把骨架編進來（2.0、2.1）。

## 1. 做什麼

使用者 2026-11-21 定的規則：core/os 每個平台的實作放在 `src/<平台>/`。`v8-arch-gyo2-os-wait` 的 `WakeSignal` 已照這個規則（`src/macos`、`src/windows`、`src/linux`）。core/os 裡還有三個檔是規則之前的寫法，在一個檔裡用 `#if` 分平台：

| 檔案 | `#if` 分的是什麼 |
|---|---|
| `src/thread/Thread.cpp`（71 行） | `SetCurrentName`、`SetCurrentPriority` 兩個函式，各分 macOS／Windows／Linux／其他四支，三個平台的呼叫完全不同 |
| `src/posix/Process.cpp`（112 行） | `ExecutablePath`：Linux 讀 `/proc/self/exe`，macOS 用 `_NSGetExecutablePath`＋`realpath`；其餘函式兩個平台共用 |
| `src/posix/Files.cpp`（440 行） | 三處：`stat` 的修改時間欄位名（`st_mtimespec`／`st_mtim`）、`FontDirectories` 的候選目錄、`DefaultCaseSensitivePaths`；其餘約 420 行兩個平台共用 |

這一批把這些 `#if` 拿掉，平台不同的部分搬進各平台的檔。只搬，不改行為。

## 2. 拆法

### 2.0 目錄（修訂 2）

```text
engine/core/os/
├── include/engine/os/   公開標頭（不變）
└── src/
    ├── common/          和 OS 無關、每個平台都編的程式（不變）
    └── sys/             平台的程式，全部在這裡
        ├── UnixFiles.hpp、UnixProcess.hpp、UnixConsole.hpp
        │                macOS 與 Linux 共用的骨架（Template Method，2.1）；只是 hpp，不單獨編譯
        ├── windows/     Console、Files、Process、Wide、WakeSignal、Thread（Windows 自己的實作，不用骨架）
        ├── macos/       Files、Process、Console（提供 macOS 的步驟並編入骨架）、WakeSignal、Thread
        └── linux/       Files、Process、Console（提供 Linux 的步驟並編入骨架）、WakeSignal、Thread；CMake 的 else()
```

- 沒有 `posix/` 目錄。共用的程式只在三個骨架 hpp 裡，各一份。
- **`sys/` 放在 `src/` 底下（建議）**，不放在 `engine/core/os/sys/`（和 `src/`、`include/` 並列）。理由：OS 邊界守衛（`tests/common/os/os_boundary_guard.py` 的 `OS_SOURCES = "engine/core/os/src/"`）只豁免 `src/` 底下的檔；放在外面要改守衛的豁免範圍（§14.4 的判定變更，範圍等同、沒有放寬面，但仍要你核准）。
- 骨架 hpp 叫 `Unix*`：它們用的是 macOS 與 Linux 共有的 Unix 系統呼叫（`open`、`stat`、`rename`、`sigaction`…），名字說的是「這份骨架給 Unix 系的平台用」，不是一個平台。
- 平台檔 include 共用標頭的相對路徑跟著改（`../common/` → `../../common/`）。

### 2.1 Template Method：骨架在 hpp，不同的步驟在平台

```cpp
// sys/UnixFiles.hpp：骨架。Files 的公開函式在這裡定義一次，只呼叫 Platform 的步驟。
// 每個建置只有一個 .cpp（sys/macos/Files.cpp 或 sys/linux/Files.cpp）include 它。
namespace Engine::Os::Files {
Base::Result<FileStatus, OsError> Stat(const std::string_view path) {
    ...
    status.modified = FromTimespec(Detail::Platform::ModifiedTime(native));   // 平台的步驟
    ...
}
bool DefaultCaseSensitivePaths() noexcept { return Detail::Platform::CaseSensitivePaths; }
...
}

// sys/macos/Files.cpp：macOS 的步驟，然後把骨架編進來
namespace Engine::Os::Detail {
struct Platform final {
    static const timespec& ModifiedTime(const struct stat& status) noexcept { return status.st_mtimespec; }
    static std::vector<std::string> FontDirectoryCandidates();          // /System/Library/Fonts…
    static constexpr bool CaseSensitivePaths = false;
};
}
#include "../UnixFiles.hpp"

// sys/linux/Files.cpp：同樣的形狀，st_mtim、/usr/share/fonts…、true
```

- 共用的約 590 行只寫一次（骨架），公開 API 不用逐一轉呼叫，兩個平台之間沒有膠水。
- 平台的步驟在編譯期決定、可以 inline；共用程式裡沒有 `#if`，也不知道自己在哪個平台。
- 步驟的清單：`Files`：修改時間的欄位、字型目錄的候選、路徑是否分大小寫；`Process`：`ExecutablePath`（整個函式是平台的步驟）；`Console`：沒有不同的步驟，兩個平台的 `Console.cpp` 只 include 骨架。
- 骨架 hpp 裡有非 inline 的定義，所以約定「每個建置只由一個 `.cpp` include」，寫在檔頭。違反時是連結錯誤（重複定義），不會靜靜地出錯。
- 系譜：Template Method（GoF）；C++ 裡用編譯期的 policy（Alexandrescu 的 policy-based design）表達。退掉的 A（步驟宣告在 hpp、共用程式另成 `.cpp`、連結期決定）也是同一個模式，但會留下一個共用的 `.cpp` 目錄，就是 posix 換名字。

- **`Thread.cpp`**：拆成 `sys/macos/Thread.cpp`、`sys/windows/Thread.cpp`、`sys/linux/Thread.cpp`，刪掉 `src/thread/`。三個平台沒有共用的程式，所以整個函式各放一份，不用骨架。
- **`src/posix/` 的三個檔** → 骨架 `sys/UnixFiles.hpp`、`sys/UnixProcess.hpp`、`sys/UnixConsole.hpp`，加上 `sys/macos/`、`sys/linux/` 各三個 `.cpp`（2.1）。之後沒有 `#if`。
- CMake：Windows 選 `sys/windows/*`；macOS 選 `sys/macos/*`；其他選 `sys/linux/*`（照舊是 `else()`）。骨架 hpp 不列進 sources。

**退掉的拆法**
- R1：共用的程式整份複製進 `sys/macos`、`sys/linux`。約 420 行的共用程式變成兩份，改一處要記得改兩處；正是 os.md RC7「per-OS files are mostly duplicates」說的情況。
- R2：留一個 `posix/` 共用目錄，小差異留 `#if`，只搬大函式。libuv 的 `src/unix/` 就是這樣（共用檔裡留小的 `#ifdef`）。退掉的理由：使用者的規則是平台實作放 `src/<平台>/`，留下 `#if` 就有兩種寫法並存；搬出去的代價只是一個三行的內部函式。
- R3（修訂 1 的寫法）：`sys/posix/` 共用目錄，步驟宣告在 hpp、實作在平台（連結期）。使用者：「posix 就不在需要了」。
- R4：骨架寫成 class template，平台的 `.cpp` 逐一轉呼叫公開 API。約 30 個進入點加三個類別，兩個平台各寫一份轉呼叫，膠水比現在的 `#if` 多。
- 參考：Rust 標準庫 `library/std/src/sys/`（平台層集中在 `sys/`）；libuv 的 `src/unix/` 加 `linux.c`、`darwin.c`（共用＋平台，但共用的是 `.c`，不是骨架）。

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
- 突變（os-subsystem 清單）：`file` 在 `src/windows`、`src/posix`、`src/macos`、`src/linux` 的約 70 個，路徑改成 `src/sys/...`；posix 的改指骨架 hpp 或平台檔（機械式改寫）。find 跟著函式搬家的，判定不變：
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
2. 拆法（2.1）：macOS 與 Linux 共用的程式寫成 `sys/Unix*.hpp` 的骨架（Template Method），平台的 `.cpp` 提供步驟並編入骨架；沒有 `posix/`。退掉 R1～R4。骨架的名字 `Unix*` 也一起確認。
3. 拿掉沒有平台會走到的 `#else` 分支。

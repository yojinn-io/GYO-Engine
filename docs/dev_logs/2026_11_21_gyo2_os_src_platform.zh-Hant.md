# 2026-11-21 core/os 的平台程式集中到 `src/sys/`（v8-arch-gyo2-os-src-platform）

Owner：架構（os-wait 卡接手）。分支 `claude/gyo2-os-src-platform`，從 `gyo2` 963d06e0（#262 合併後）。
事前宣告：[`os-src-platform-declaration.zh-Hant.md`](../architecture/target/v2/os-src-platform-declaration.zh-Hant.md)（修訂 3）。

## 為什麼

- os-wait 的後續清單裡有一項：core/os 裡 `src/thread/Thread.cpp`、`src/posix/Process.cpp`、`src/posix/Files.cpp` 還是「一個檔案裡用 `#if` 分平台」的舊寫法，和 `WakeSignal` 的分檔寫法並存。
- 使用者：「這個你要處理，你本身就是os的實作者。」
- 形狀由使用者定（宣告的經過）：
  - 「你建一個sys ，把3個平台的代碼放進去/core/os/sys/windows ，這樣的」；
  - 「posix 能不能用一個設計模式放在hpp中。實現代碼依然放在對應的macos、linux中。嚴格來說抽象就是這樣一回事。」「posix 就不在需要了。」；
  - 「java都有interface 可以做部分實現，變的地方放implement」「Windows 也繼承，做一個空實現就可以了。」；
  - 「你已經有了現成的實作，你只需要改架構。實現完全不用改。」

## 做了什麼

- `engine/core/os/src/sys/`：
  - `Platform.hpp`：抽象類別 `Detail::Platform`，只放 macOS 與 Linux 不同的步驟，全是純虛函式：`ModifiedTimespec`（`stat` 的修改時間欄位）、`FontDirectoryCandidates`、`CaseSensitivePaths`、`NativeExecutablePath`。`CurrentPlatform()` 交出這個建置的平台。
  - `macos/Platform.cpp`、`linux/Platform.cpp`：實作這些步驟，內容是原本 `#if` 的各支，原樣搬來。`ExecutablePath` 的 UTF-8 檢查留在共用的 `Process.cpp`，平台只回 OS 給的路徑；錯誤的建構從 `FromErrno` 換成它展開後的 `MakeOsError`（同一個值）。
  - `windows/Platform.cpp`：繼承，空實作（Windows 自己實作檔案、行程、主控台，不呼叫這些步驟）。
  - `Files.cpp`、`Process.cpp`、`Console.cpp`：原本 `src/posix/` 的共用程式原樣搬來；`#if` 的三處（修改時間、字型目錄、大小寫）和 `ExecutablePath` 換成呼叫 `Detail::CurrentPlatform()`。`src/sys/` 底下沒有任何 `#if`。
  - `macos/Thread.cpp`、`windows/Thread.cpp`、`linux/Thread.cpp`：從 `src/thread/Thread.cpp` 的各支逐行切出（以程式從原檔取出各分支，不是手打）。原本的 `#else`（三個平台以外）拿掉：CMake 只選三個平台的檔。
  - `windows/` 的其餘檔、`WakeSignal` 三份：只搬位置；include 共用標頭的相對路徑改成 `../../common/`。
- `engine/core/os/CMakeLists.txt`：Windows 選 `src/sys/windows/*`；非 Windows 選 `src/sys/{Files,Process,Console}.cpp`，再依 Apple 與否選 `src/sys/macos/*` 或 `src/sys/linux/*`。
- `tests/common/os/CMakeLists.txt`：後備的測試執行檔改編 `src/sys/linux/WakeSignal.cpp`。
- `os.md`：「What it is」加內部的目錄說明；Contracts 的 Thread 段改成每個平台一份。
- 突變（os-subsystem 清單）：`file` 在舊 `src/windows`、`src/posix`、`src/macos`、`src/linux`、`src/thread` 的 77 個改到新路徑；`os-p6-executable-path`、`os-x6-macos-font-system-dir-missing` 跟著函式搬到 `src/sys/macos/Platform.cpp`（find 只多了一層縮排）；`ow-x14` 的 find 對到新的 CMake 選檔行；三個守衛指令的突變（`os-f4`、`os-c6`、`pu-x5`）expect 裡的路徑改成 `src/sys/windows/`。判定都沒變。

## 驗證

本機 macOS Intel（這台 Mac），preset `test`。

- 全量建置＋ctest：157/157。
- 突變：路徑或 find 有改的 78 個裡，本機能跑的 48 個全部 killed；30 個只在 Linux 或 Windows 跑，本機 skip（CI 不跑突變，見下）。第一次 45/48：`os-f4`、`os-c6`、`pu-x5` 是 wrong_failure，原因是 expect 裡寫死了舊路徑 `engine/core/os/src/windows/`（守衛印出的是新路徑）；改了 expect 的路徑後 3/3 killed（路徑的錯，不是判定放寬）。
- L1 三平台：ddeca8ad 全部通過（run 44912714512；linux-x64、windows-x64、macos-arm64）。Linux、Windows 的新檔在這裡第一次編過並跑過測試。
- 證據：主 checkout 的 `build/evidence/gyo2-os-src-platform/`（這台 Mac）：`build.log`、`ctest.log`、`mutations.log`、`mutations_run2.log`。

## 範圍的限界

- 只在 Linux 或 Windows 跑的 30 個突變（例如 `ti-x4`、`th-x6`、`os-f8`，以及 Windows 的檔案、主控台突變）這一批沒有在那兩個平台跑；它們的 find 都確認唯一命中新檔，Linux、Windows 的行為由 L1 的測試守。
- os-subsystem 清單裡另有 17 個突變指向 gyo2 已刪的 `engine/trace`、`engine/asset`（早就失效，和這一批無關），沒動。

## 時間

修訂 0 估 1.5～2 小時。實際：22:23 接手，22:38～23:02 宣告來回三版（改名 Unix、include 技巧、hook 改寫，都退掉了），23:02～23:08 照使用者最後的形狀改完並通過建置與測試，之後是等突變。改架構本身約 6 分鐘；多出的時間花在我的設計繞路上。

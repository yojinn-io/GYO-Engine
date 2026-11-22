# 2026-11-22 網路 Module 第 0 步：`core/os` 的 UDP socket（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`，從 `gyo2` eacab4d7（含 #270）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md)（修訂 2）。
  - 使用者 2026-11-22 動工前核准「照建議，全部核准」。
  - 追問 Q4 後回覆「維持，開始」。

## 為什麼

- 宣告 D-1、Q5（使用者核准）：UDP socket 放 `core/os`。
- 網路先做（`v8-arch-gyo2-net-is-mechanism-first`）是名指的 Feature 帶來的予見壓力，取代 D78 的「沒有觀測的壓力」。
- os.md 的 DI 7（哪些錯誤是暫時的由呼叫者決定）不變。

## 做了什麼

- **`engine/core/os`**：
  - 公開標頭 `include/engine/os/DatagramSocket.hpp`：
    - `DatagramAddress`：IPv4＋port；`Loopback`、`Any`、`Parse`、`ToString`、`==`。
    - `ReceivedDatagram`。
    - `DatagramSocket`：`Open`、`LocalAddress`、`Send`、`Receive`；非阻塞，不交給子行程。
  - `OSErrorCode` 多四個碼：`InvalidAddress`、`AddressInUse`、`Unreachable`、`WouldBlock`。
  - `src/common/DatagramAddress.cpp`：解析與文字，沒有 OS 呼叫。
  - `src/sys/DatagramSocket.cpp`：macOS 與 Linux 共用（BSD sockets、`fcntl`、`recvmsg` 看 `MSG_TRUNC`）。兩平台的呼叫相同，不經 `Detail::Platform`。
  - `src/sys/windows/DatagramSocket.cpp`：
    - Winsock 2，`FIONBIO`；每個 socket 持一個 `WSAStartup` 參照；
    - `SIO_UDP_CONNRESET`／`SIO_UDP_NETRESET` 關掉；
    - `recvfrom` 的 `WSAEMSGSIZE` 回 `TooLarge`。
  - `CMakeLists.txt`：依平台選來源；Windows 連 `ws2_32`（DI 3 允許的系統庫）。
- **文件**：`docs/architecture/os.md`
  - Components 的 Net（conditional）列換成 Datagram 列；
  - Contracts 加 Datagram 一條；
  - Out of scope 拿掉「socket 留在產品」。
- **測試**（`tests/common/os/`）：
  - `DatagramSocketTests.cpp` 7 案：解析、loopback 往返（帶寄件位址）、沒有就回「沒有」、太長是 `TooLarge` 且下一筆照收、`AddressInUse`、`InvalidAddress`、送到沒人聽的 port 之後收件不中斷。
  - `OSErrorTests` 補四個碼的名字。
  - `OSHeaderHygieneTests` 補 include 新標頭：守衛「每個公開標頭都被這個測試 include」抓到了漏的。

## 和宣告不同的地方

- **只有 IPv4**：宣告 D-1 寫「IPv4／IPv6 位址」。IPv6 沒有點名的功能要用（AGENTS §2：根據的不足的不做），先不做。`DatagramAddress` 之後要加 IPv6 時再改形狀。合併前核准時一起給使用者看。

## 驗證

- 本機（macOS x86_64，`core` preset）：
  - `gyo_os_tests` 74 案、1059 個斷言全過（8.5 s）；
  - `ctest -R "os\."` 6 個全過；
  - 守衛 `os.boundary_guard`、`os.header_hygiene` 通過。
- **突變**（`docs/architecture/plans/os-subsystem/scripts/mutations.json` 加 7 個，`od-` 開頭；`guards`：`net-module-declaration D-1`、`判定 G-7`）：
  - 本機 6／6 killed：`od-trunc-ignored`、`od-addr-in-use`、`od-addr-not-local`、`od-from-lost`、`od-parse-leading-zero`、`od-parse-trailing`。
  - `od-win-connreset-on` 只在 Windows 跑，本機 skipped。**CI 不跑突變，所以它還沒被證明會 killed**，合併前核准時揭露。
  - `od-parse-trailing` 第一次是 wrong_failure：清單裡第一個被它放過的是 `1.2.3.4.5`，不是 `1.2.3.4 `。expect 改成前者（它仍只認這個突變）。
  - `CAPTURE` 原本印出指標，迴圈改成 `std::string`。
- **§14.5 的確認**：7 個都寫了「只有它捕到的」。非阻塞（`O_NONBLOCK` 拿掉）的突變沒有加：它的失敗是逾時，不是可識別的訊息（§14.2）。
- **CI（G-7 三平台）**：PR-1 開了之後跑 L1，結果補在這裡。
- **證據**：主 checkout `build/evidence/gyo2-net-module/step0/ctest-os-last.log`。突變的輸出未保存，結果在上面。

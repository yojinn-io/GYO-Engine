# `OS::DatagramAddress` 改名的事前宣告（`v8-arch-gyo2-rename-address`）

日期：2026-11-25。改名的任務卡（GYO2 主會話開的任務卡）。作者 Opus 5.5／medium；設計審查 Opus／high。
根據：Gateway #339 設計審查 S5（主會話轉交）；使用者 2026-11-25「DatagramAddress 改名的卡先開」。

## 0. 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-gyo2-rename-address` | 這一批：`engine/core/os` 的位址型別 `DatagramAddress` 改名為 `SocketAddress`，搬到自己的標頭；行為不變 |

## 1. 要解決的問題

`DatagramAddress`（IPv4＋port）原本只給 UDP 的 `DatagramSocket`。#339 之後 TCP 的 `StreamSocket`、`http` Module 的 `HttpCall::Start` 也收它，產品拿它填 Gateway 的位址（`MatchSaves::service`）。名字說它屬於 datagram，實際上是「socket 綁定、送往、連線的位址」；型別也住在 `DatagramSocket.hpp`，`StreamSocket.hpp` 只為了位址去 include UDP 的標頭。

## 2. 新名字：`SocketAddress`

**為什麼選它**：它是兩種 socket 共用的位址，名字跟著 socket 走、不跟著傳輸走，和同一組的 `DatagramSocket`、`StreamSocket` 讀起來一致；也避開 net 的 `Endpoint`（Link 上的對端，net 自己的概念）。

| | 位址型別 | UDP 與 TCP 共用 |
|---|---|---|
| Rust std（lightyear 用的） | `core::net::SocketAddr`（V4／V6 的 enum；另有 `SocketAddrV4`） | 是（`UdpSocket`、`TcpStream` 都收它）；lightyear 在同一個檔裡同時用 OS 層的 `SocketAddr` 和 Link 層自己的 `Endpoint`（`crates/io/udp/src/endpoint.rs:32,36`），和 GYO 的 OS／net 分法一樣 |
| Godot | `IPAddress`＋另給的 port；底層 `NetSocket::Address{ip, port}`（`core/io/net_socket.h:63-87`） | 是（`PacketPeerUDP::bind`、`StreamPeerTCP::connect_to_host`，`core/io/packet_peer_udp.h:72`、`stream_peer_tcp.h:48`） |
| asio | `ip::address`＋`ip::udp::endpoint`／`ip::tcp::endpoint`（〔知〕，本機沒有原始碼） | 否（每種協定一個 endpoint 型別） |

- 不取 `Endpoint`：和 net 的 `Endpoint` 撞名，兩者在 `UdpLink::ToEndpoint`／`ToAddress` 互轉，同名會混淆。
- 不取 `IpAddress`：Godot 的 `IPAddress` 不含 port；我們的型別含 port，名字會說少了。
- 不加 `V4`：Rust 的 `SocketAddrV4` 是因為 `SocketAddr` 另有 V6；這裡只有一種。IPv6 沒有具名的 Feature（os 頁 Datagram 一節），現在不為它取名（日後要加 IPv6，`octets` 的形狀本來就得改）。
- 照 asio 每種協定各一個型別：兩種 socket 收的位址完全一樣，拆開只是多一個轉換。

**規則**：批次表那一列寫「改名要排前面、同時立規則」。這次照命名規範 N5（[`rebuild-naming.zh-Hant.md`](rebuild-naming.zh-Hant.md)，名字是概念的標籤）就夠：多個使用者共用的型別，名字取共同的概念。不另立條文，也不加守衛（名字由每批的設計審查看）。

## 3. 範圍

- 型別與方法不變（`octets`、`port`、`Loopback`、`Any`、`Parse`、`ToString`、`==`），只換名字。
- 搬到 `engine/os/SocketAddress.hpp`；`DatagramSocket.hpp`、`StreamSocket.hpp` include 它。`NetService.hpp`、產品標頭（大廳、`MatchSaves`）、`client_main.cpp`、測試的 listener 只用位址，改 include 新標頭；`UdpLink.hpp`、`Http.hpp` 用到 socket，維持原樣。`src/common/DatagramAddress.cpp` → `src/common/SocketAddress.cpp`。
- 使用處：`core/os`、net（`NetProductOptions::bind`、`NetConnectTarget::server`、`NetService::UdpAddress`、`UdpLink`）、`http`、`games/object_fps_pvp`（Client、Match、大廳、`MatchSaves`）、測試。
- 文件：os 頁 Datagram／Stream 兩節、http 頁、連線目標的宣告、PvP 的移植與 Gateway 宣告（Gateway 宣告第 3 段那句是當時的紀錄，保留原句、加註改名）。dev_log 與 HANDOFF 等歷史紀錄不改。
- 舊名不留別名。
- 突變：只更新 find／replace／file 受影響的條目，並重跑它們（D184：純改名批不做完整重跑）。

## 4. 不做

- 不改行為、不加 IPv6、不動 net 的 `Endpoint`。
- Gateway #340 審查 N9（`ServerHost::Respond` 的順序）只記錄，不改。
- 設計審查 N7 記給下一個碰 os socket 的批：IPv4 位址轉換（`ToNative`／`FromNative`）在 `src/sys/DatagramSocket.cpp`、`src/sys/StreamSocket.cpp` 與 windows 下的兩個檔各寫一份，可收成每個平台一份內部轉換；`SocketAddress::Parse` 的測試仍在 `DatagramSocketTests.cpp`。

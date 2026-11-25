# 2026-11-25 `OS::DatagramAddress` 改名為 `SocketAddress`（`v8-arch-gyo2-rename-address`）

Owner：Engine（core/os、net、http）。改名的任務卡（GYO2 主會話開的任務卡）做。作者 Opus 5.5。
根據：[事前宣告](../architecture/target/v2/rename-address-declaration.zh-Hant.md)；Gateway #339 設計審查 S5；使用者 2026-11-25「DatagramAddress 改名的卡先開」。

## 做了什麼

1. 型別改名為 `SocketAddress`，搬到 `engine/os/SocketAddress.hpp`；`src/common/DatagramAddress.cpp` → `SocketAddress.cpp`。理由：UDP 與 TCP 兩種 socket 共用的位址，名字跟著 socket 走，和 `DatagramSocket`、`StreamSocket` 一致，也避開 net 的 `Endpoint`（Rust `SocketAddr`、Godot `NetSocket::Address` 同樣共用；asio 每種協定一個型別，不採）。
2. 使用處：core/os、net（`NetProductOptions::bind`、`NetConnectTarget::server`、`NetService::UdpAddress`、`UdpLink`）、http、`object_fps_pvp`、測試。只用位址的標頭改 include 新標頭。舊名不留別名。行為不變。
3. 文件：os 頁 Datagram／Stream、http 頁、連線目標的宣告、PvP 移植宣告；Gateway 宣告第 3 段的紀錄保留原句、加註改名。
4. 突變：`os-subsystem/scripts/mutations.json` 的 `od-from-lost`（replace）、`od-parse-leading-zero`、`od-parse-trailing`（file）更新。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 256/256、`ctest -L network` 2/2；Go `go test ./...`（`games/object_fps_pvp`）全過，Go 端沒有改動。
- 突變（D184，只跑受影響的）：`od-from-lost`、`od-parse-leading-zero`、`od-parse-trailing`、`os-g17-hygiene-coverage` 4/4 killed。其他目錄的 find 全部仍唯一命中。

## 設計審查（Opus／high，唯讀）

blocker 0、should 1、nit 7。紀錄：`build/evidence/v8-arch-gyo2-rename-address/design-review.zh-Hant.md`（本機）。

- S1（批次表要「同時立規則」）→ 宣告寫明照命名規範 N5，不另立條文、不加守衛。
- N1–N6（殘留 include、「datagram 的型別」舊說法、os.md 欄位、宣告用字、Gateway 宣告重複、縮排）→ 照改。
- N7（IPv4 位址轉換在四個平台檔各寫一份；`Parse` 的測試仍在 `DatagramSocketTests.cpp`）→ 記給下一個碰 os socket 的批。

## 順手看的（不在範圍）

Gateway #340 審查 N9 現況屬實：`ServerHost::Respond`（`engine/modules/net/src/ServerHost.cpp:54-66`）在 challenge 通過、salt 較大時先 `End` 同一 endpoint 的舊連線，之後才比 facts、問 admission；被拒的重連會先斷掉自己還活著的連線。建議下一個碰 net 的批把 `End` 移到 facts 與 admission 之後（被拒時舊連線不動）；admission 若按 userId 計連線數，要先定它看不看得到舊連線。netcode.io 系（lightyear）對已連線的位址直接忽略新的連線請求，可當對照。

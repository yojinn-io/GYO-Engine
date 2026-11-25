# 2026-11-25 PvP Gateway 移植第 3 段：core/os 的 TCP 與 http Module（`v8-product-pvp-gateway`）

Owner：Engine（`engine/core/os`、新的 library-only Module `http`、`engine/core/base`）。GYO2 主會話開的任務卡（PvP Gateway 移植）。作者 Opus 5.5／high。疊在第 2 段（PR #337）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md)（第 1 節第 3 段、D8）。D78（`os.md` Constraints 9）：core/os 只在 Feature 有壓力時長；這裡的 Feature 是使用者 2026-11-25 放進 PvP 移植的 Gateway（`v8-arch-gyo2-gateway-in-pvp-port`）：Client 要講 Gateway、Match 要講 saves。

## 做了什麼

1. **`engine/core/os` 的 `StreamSocket`**（`include/engine/os/StreamSocket.hpp`）：非阻塞的 IPv4 TCP client。`Connect`（開始連）、`State`（`Connecting`／`Connected`；失敗是 `Unreachable` 或 `IOFailed`，之後一直是失敗）、`Send`（OS 收了幾個 byte，沒空間是 0）、`Receive`（收到幾個；0＝對方關了；nullopt＝現在沒有）。macOS 與 Linux 共用 `src/sys/StreamSocket.cpp`（`poll`＋`SO_ERROR`；SIGPIPE：C 函式庫有 `MSG_NOSIGNAL` 就用它（Linux、2023 年後的 macOS SDK），沒有才在 socket 設 `SO_NOSIGPIPE`，只用一種、靠 feature macro 選，不用 OS 巨集），Windows 是 `src/sys/windows/StreamSocket.cpp`（`select`：WSAPoll 在 Windows 10 2004 之前不回報連線失敗）。都設 `TCP_NODELAY`。位址沿用 `DatagramAddress`。沒有 listener（只有測試會用）。
2. **新的 library-only Module `http`**（`config/modules.csv`，`GYO::Http`）：`EncodeRequest`（請求行、Host、`Connection: close`、需要時的 `Content-Length`）、`ResponseParser`（一次餵任意片段；`Content-Length`、chunked、讀到連線關閉；1xx 跳過；204／304 沒有 body；head 與 body 的上限）、`HttpCall`（一個請求一條連線，呼叫者每幀 `Poll`；不阻塞；放棄的時機是呼叫者的）。錯誤 `HttpFailure`（`ConnectFailed`、`TransferFailed`、`MalformedResponse`、`Truncated`、`TooLarge`）。
3. **Base 的 `Sha256Digest`**（第 2 段審查 S5）：位元組版的 SHA-256；`Sha256`（hex）改成由它轉出。產品的 HMAC 改用它。net 的 `Handshake.cpp:95` 與 authority 的 `Registrations.cpp:79` 也在把 hex 解回位元組，不在本卡範圍，留作後續。
4. **測試的 listener**：`tests/common/os` 的測試函式庫 `gyo_test_loopback_listener`（中性的標頭 `listener_include/gyo_test/LoopbackListener.hpp`、原生 socket 只在 `LoopbackListener.cpp`；`Accept` 與收資料都有逾時，伺服器執行緒不會卡死、也不在執行緒裡中止測試），OS 與 http 的測試、第 4 段的存檔測試共用。
5. 文件：`os.md` 的 Components 加 Stream 一列、Contracts 加 Stream 一節、DI 3 加 `StreamSocket`；新頁 `docs/architecture/target/v2/http.zh-Hant.md`。
6. 第 2 段的 `match_process` 測試修正：「沒人來就結束」那條在失敗時也停掉 Match。第 2 段的突變 `match-never-idle`（Match 永不結束）跑完會留下一個占著 47813 的 Match，下一次測試就因 `AddressInUse` 失敗；那個孤兒行程（本 worktree 的、port 47813）已停掉，現在突變跑完不留行程。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 255/255、`-L "go|network"` 4/4。`gyo_os_tests` 加 3 案（連線雙向收發並看到對方關閉、沒資料不阻塞、沒人 listen 的 port 是 `Unreachable` 而且一直是）；`http.client` 8 案（請求的位元組、逐 byte 餵的 Content-Length、chunked（有一段 ≥ 10，守 16 進位）、讀到關閉、100＋204、拒絕的情形（不是 HTTP、截斷、body／head 超過上限、chunk 大小不是 16 進位）、對 loopback listener 的完整呼叫、連不上）；`gyo_base_tests` 加 SHA-256 的 FIPS 向量（位元組與 hex）。守衛 `os.header_hygiene` 抓到新的公開標頭沒被它的測試 include，已補。
- 突變（`docs/architecture/target/v2/scripts/http_mutations.json`）8/8 killed（`http-truncation-unseen`、`http-body-limit-ignored` 第一次 wrong_failure：測試先用 `REQUIRE_FALSE` 中止、帶標籤的訊息沒印，改成一個帶標籤的檢查後 killed）。OS 的兩項只在 macOS、Linux 跑（`platforms`）。
- **沒驗**：Windows 的 `StreamSocket` 只靠 CI L1；對真的 Go Gateway（net/http 的回應）在第 5 段的端到端走到。

## 設計審查（Opus／high，唯讀；作者同模型，Fable 暫停中）

blocker 1、should 6、nit 9、要使用者定的 0。報告：主 checkout `build/evidence/v8-product-pvp-gateway/segment3-design-review.zh-Hant.md`。審查者沒有建置、跑測試或突變；S1 的平台差異是照源碼推的。

- B1（http 的測試經 include 路徑直接含入帶 OS 標頭的 `tests/common/os/LoopbackListener.hpp`：守衛排除整個 `tests/common/os` 所以會過，但繞過 `os.md` DI 1 的精神、是第一個先例）→ 改成 `tests/common/os` 的測試函式庫：標頭中性（只有 `DatagramAddress` 與標準型別），原生程式在 .cpp，消費者只連 `gyo_test_loopback_listener`。
- S1（Send／Receive 在連線完成前三平台結果不同）→ 兩者先看連線自己的狀態：連線中 Send 回 0、Receive 回沒有；連線失敗回 `State` 的錯誤。加一案。
- S2（os.md 只有 Components 一列）→ Contracts 加 Stream 一節（含 OS 的連線逾時、0.0.0.0、Linux 的 `EADDRNOTAVAIL`）、DI 3、`OSError.hpp` 的註解。
- S3（http 只接 OS 錯誤的 message，丟掉碼與 detail）→ `Base::CauseDetail`；照 `UdpLink` 的先例拆出 `Unreachable`。
- S4（Engine 的註解指向產品文件、用產品的詞）→ 註解改成中性，設計依據寫進 Engine 文件（`os.md` 的 Stream、新頁 `http.zh-Hant.md`），產品的 D8 反過來引用。
- S5（TCP 與 http 的公開介面用 `DatagramAddress`，概念錯了；全 repo 約 89 處、25 檔的機械改名）→ **本卡不做**：改名會碰到網路 Module 的 `NetService`（#332 正在改），記成後續的獨立改名批交主會話；本卡的文件寫明「位址是 IPv4＋port，型別名沿用」。
- S6（SIGPIPE 與「沒空間回 0」兩條契約沒測試）→ 各加一案、各配突變（SIGPIPE 的突變讓測試行程被殺，expect 接受任何輸出。第一次在 macOS 存活：新的 macOS SDK 也有 `MSG_NOSIGNAL`，兩道防線同時在，拿掉一道不會被抓；改成只用一種機制、突變一項；測試也改成第一次失敗後再送三次——第一次失敗是 ECONNRESET，之後才是會帶 SIGPIPE 的 EPIPE）。
- N1（connect 遇 EINTR 重呼叫會得 EALREADY）→ 交給 `State`。N2 → 空 buffer 讀 0 寫進標頭與 os.md。N3 → 寫進 os.md。N4（Windows 在 SO_ERROR 為 0 時編造 `WSAECONNREFUSED`）→ 改成 `IOFailed`，detail 說 select 回報失敗但沒有錯誤。N5（HEAD）→ 寫進「不做」。N6（CR／LF）→ 寫明是呼叫者的事。N7（解析器出錯後不停）→ 出錯後每次都回同一個錯誤，加一案。N8（完成後每次 Poll 複製回應）→ 寫進文件。N9（測試執行緒裡的 REQUIRE 與沒 join）→ 執行緒裡不用 REQUIRE、listener 有逾時；PR 本文寫明 commit 的組成（主要程式在 `wip:` commit，`031a1ea8` 補了第 2 段的一個測試修正）。

# 2026-11-25 PvP Gateway 移植第 3 段：core/os 的 TCP 與 http Module（`v8-product-pvp-gateway`）

Owner：Engine（`engine/core/os`、新的 library-only Module `http`、`engine/core/base`）。GYO2 主會話開的任務卡（PvP Gateway 移植）。作者 Opus 5.5／high。疊在第 2 段（PR #337）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md)（第 1 節第 3 段、D8）。D78（`os.md` Constraints 9）：core/os 只在 Feature 有壓力時長；這裡的 Feature 是使用者 2026-11-25 放進 PvP 移植的 Gateway（`v8-arch-gyo2-gateway-in-pvp-port`）：Client 要講 Gateway、Match 要講 saves。

## 做了什麼

1. **`engine/core/os` 的 `StreamSocket`**（`include/engine/os/StreamSocket.hpp`）：非阻塞的 IPv4 TCP client。`Connect`（開始連）、`State`（`Connecting`／`Connected`；失敗是 `Unreachable` 或 `IOFailed`，之後一直是失敗）、`Send`（OS 收了幾個 byte，沒空間是 0）、`Receive`（收到幾個；0＝對方關了；nullopt＝現在沒有）。macOS 與 Linux 共用 `src/sys/StreamSocket.cpp`（`poll`＋`SO_ERROR`；SIGPIPE 用 feature macro 選 `MSG_NOSIGNAL` 或 `SO_NOSIGPIPE`，不用 OS 巨集），Windows 是 `src/sys/windows/StreamSocket.cpp`（`select`：WSAPoll 在 Windows 10 2004 之前不回報連線失敗）。都設 `TCP_NODELAY`。位址沿用 `DatagramAddress`。沒有 listener（只有測試會用）。
2. **新的 library-only Module `http`**（`config/modules.csv`，`GYO::Http`）：`EncodeRequest`（請求行、Host、`Connection: close`、需要時的 `Content-Length`）、`ResponseParser`（一次餵任意片段；`Content-Length`、chunked、讀到連線關閉；1xx 跳過；204／304 沒有 body；head 與 body 的上限）、`HttpCall`（一個請求一條連線，呼叫者每幀 `Poll`；不阻塞；放棄的時機是呼叫者的）。錯誤 `HttpFailure`（`ConnectFailed`、`TransferFailed`、`MalformedResponse`、`Truncated`、`TooLarge`）。
3. **Base 的 `Sha256Digest`**（第 2 段審查 S5）：位元組版的 SHA-256；`Sha256`（hex）改成由它轉出。產品的 HMAC 改用它。net 的 `Handshake.cpp:95` 與 authority 的 `Registrations.cpp:79` 也在把 hex 解回位元組，不在本卡範圍，留作後續。
4. **測試的 listener**：`tests/common/os/LoopbackListener.hpp`（原生 socket；OS 標頭只在 `tests/common/os`），OS 與 http 的測試共用。
5. `os.md` 的 Components 加 Stream 一列。
6. 第 2 段的 `match_process` 測試修正：「沒人來就結束」那條在失敗時也停掉 Match。第 2 段的突變 `match-never-idle`（Match 永不結束）跑完會留下一個占著 47813 的 Match，下一次測試就因 `AddressInUse` 失敗；那個孤兒行程（本 worktree 的、port 47813）已停掉，現在突變跑完不留行程。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 255/255、`-L "go|network"` 4/4。`gyo_os_tests` 加 3 案（連線雙向收發並看到對方關閉、沒資料不阻塞、沒人 listen 的 port 是 `Unreachable` 而且一直是）；`http.client` 8 案（請求的位元組、逐 byte 餵的 Content-Length、chunked（有一段 ≥ 10，守 16 進位）、讀到關閉、100＋204、拒絕的情形（不是 HTTP、截斷、body／head 超過上限、chunk 大小不是 16 進位）、對 loopback listener 的完整呼叫、連不上）；`gyo_base_tests` 加 SHA-256 的 FIPS 向量（位元組與 hex）。守衛 `os.header_hygiene` 抓到新的公開標頭沒被它的測試 include，已補。
- 突變（`docs/architecture/target/v2/scripts/http_mutations.json`）8/8 killed（`http-truncation-unseen`、`http-body-limit-ignored` 第一次 wrong_failure：測試先用 `REQUIRE_FALSE` 中止、帶標籤的訊息沒印，改成一個帶標籤的檢查後 killed）。OS 的兩項只在 macOS、Linux 跑（`platforms`）。
- **沒驗**：Windows 的 `StreamSocket` 只靠 CI L1；對真的 Go Gateway（net/http 的回應）在第 5 段的端到端走到。

## 設計審查

（審查後補）

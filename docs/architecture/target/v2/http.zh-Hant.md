# HTTP Module（`engine/modules/http`）

Owner：架構。來源：`v8-product-pvp-gateway` 第 3 段（遊戲的事前宣告 D8 引用本頁）。最終文字在該段的合併前核准定稿。

遊戲向自己的後端服務送 HTTP/1.1 請求的機制。library-only（`config/modules.csv` 的 `library_only=yes`，`GYO::Http`），不登記任何單位、不進 World；只依賴 Base 與 `engine/core/os` 的 `StreamSocket`（`os.md` 的 Stream）。

## 為什麼在 Engine

- 前提：socket 只能在 `engine/core/os`（`os.md` DI 1），作者自己包第三方 HTTP 函式庫要開 Plugin；請求的組裝與回應的解析每個要接後端的遊戲都會要，屬於機制。
- D78（`os.md` Constraints 9）：Stream 是由 Feature 的壓力帶進來的（例：Object_FPS_PVP 的 Client 要講它的 Gateway、Match 要講它的存檔服務；使用者 2026-11-25 把 Gateway 放進 PvP 移植）。
- 別家：Godot 的 `HTTPClient`（`~/Code/GitHome/godot/core/io/http_client.h:37`，`poll()` 在 `:192`）與 `StreamPeerTCP` 是同一個輪詢形狀；Unity 的 `UnityWebRequest`、Unreal 的 `FHttpModule` 也在 Engine 裡。Bevy 本體沒有 HTTP（生態的 `bevy_http_client` 包 ehttp）：GYO 偏離它，因為 OS 的 socket 只能在 core/os，產品自己包函式庫的膠水比一個小 Module 多。

## 形狀

- `EncodeRequest(HttpRequest)`：請求行、`Host`、`Connection: close`、需要時的 `Content-Length`（有 body，或方法是 POST、PUT、PATCH）、其他標頭、body。文字照收：CR、LF 不檢查，是呼叫者的事。
- `ResponseParser`：一次餵任意片段（`Feed`），連線結束時 `Close`。讀 `Content-Length`、chunked、或讀到連線關閉；1xx 跳過、204／304 沒有 body；head 與 body 有上限（`ResponseLimits`，預設 16 KiB／1 MiB）。出錯後每次 `Feed`、`Close` 都回同一個錯誤。純函式：沒有 socket、沒有時鐘。
- `HttpCall`：一個請求一條連線（`Connection: close`），呼叫者每幀 `Poll` 一步（連線、送出 OS 收得下的位元組、收進等著的位元組）。不阻塞。放棄的時機是呼叫者的：丟掉 `HttpCall` 就關掉連線；這裡唯一的逾時是 OS 自己的連線逾時（幾十秒）。完成後 `Poll` 每次給一份回應的複本。World 裡把它放在 Resource，由 System 輪詢。
- 錯誤 `HttpFailure`（E2：這個請求沒成，其他照舊）：`ConnectFailed`、`Unreachable`、`TransferFailed`、`MalformedResponse`、`Truncated`、`TooLarge`。從 OS 來的錯誤把 OS 的碼與 detail 帶在 detail 裡（`Base::CauseDetail`，`docs/architecture/error-handling.md` 規則 5）；連不到對方是 `Unreachable`（照網路 Module `UdpLink` 的先例）。

## 不做

DNS（位址是 IPv4 字面值，os 的 socket 位址 `SocketAddress`）、TLS、重導、cookie、keep-alive、HTTP/2、HEAD（HEAD 的回應有長度、沒有 body，解析器會等 body）。

## 測試與突變

`tests/common/http/`（ctest `http.client`，cpu）：請求的位元組、逐 byte 餵的回應、chunked、讀到關閉、1xx＋204、拒絕的情形、出錯後停住、對 loopback 的完整呼叫、連不上（`Unreachable` 帶 OS 的碼）。listener 是 `tests/common/os` 的測試函式庫 `gyo_test_loopback_listener`（原生 socket 只在那裡）。突變：`scripts/http_mutations.json`。

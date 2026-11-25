# Gateway 的 Client 端交接說明（`v8-product-pvp-gateway` → 遊戲流程框架卡）

日期：2026-11-25。寫的人：PvP Gateway 移植卡。主會話 2026-11-25 定：Client 改走 Gateway 的房間流程歸之後的遊戲流程框架卡，Gateway 卡做到 Match 端與 Go 服務，並交這份說明。
設計與理由在[事前宣告](gateway-declaration.zh-Hant.md)（D1～D9、第 4 節限制 L1～L10）；這頁只寫 Client 要用的介面與接法。

## 1. 流程（使用者的遊戲流程，宣告 D9）

```text
多人 → 大廳 World（房間列表；建房：模式、地圖）→ 房間 World（成員、準備；房主按開始）
     → 房間變成 started，GET 房間拿到自己的 connect → 遊戲 World（prepare 放 NetConnectTarget）
     → 離開或 Match 結束 → 回大廳（存檔由 Match 寫，Client 只讀）
```

- World＝場景：大廳與房間是沒有 net 的 Product（現在的 `LobbyWorld` 就是）；只有遊戲 World 帶 `NetConnectTarget`（#332：`engine/net/NetService.hpp`，準備函式或 `ReplaceRequest` 的 extra 放入；沒有就不連；同目標且連線活著時沿用，換目標就關舊開新）。
- 單人不碰 Gateway，也不帶 `NetConnectTarget`。

## 2. 外部協議 v1（HTTP/1.1＋JSON，Gateway 預設 `127.0.0.1:7700`）

所有回應都有 `Content-Length`。錯誤一律 `{"error":"<code>"}`＋HTTP 狀態；碼只說「發生了什麼」，怎麼處理由 Client 決定。

| 請求 | 標頭 | body | 成功的回應 |
|---|---|---|---|
| `POST /v1/session` | — | `{"deviceId":"<1..128 個可見 ASCII>"}` | `{"userId":7,"sessionToken":"…","expiresAt":<Unix 秒>}` |
| `GET /v1/rooms` | Bearer | — | `{"rooms":[{"id","mode","map","players","capacity","state":"open"}]}`（只列 open） |
| `POST /v1/rooms` | Bearer、Idempotency-Key | `{"mode":"coop"\|"survival","map":"<arena id>"}` | 房間（建的人自動加入、是房主） |
| `GET /v1/rooms/{id}` | Bearer | — | 房間：`{"id","mode","map","players","capacity","state","owner","members":[{"userId","ready"}],"connect"?}` |
| `POST /v1/rooms/{id}/join` | Bearer、Idempotency-Key | — | 房間（加入別的房會自動離開原本的 open 房） |
| `POST /v1/rooms/{id}/ready` | Bearer、Idempotency-Key | `{"ready":true\|false}` | 房間 |
| `POST /v1/rooms/{id}/start` | Bearer、Idempotency-Key | — | 房間（`state:"started"`、含呼叫者自己的 `connect`） |
| `POST /v1/rooms/{id}/leave` | Bearer、Idempotency-Key | — | `{"left":true}` |
| `GET /v1/saves/me` | Bearer | — | `{"userId","version","body"}`；body 是存檔（從沒寫過是 `null`） |

- `Bearer`＝`Authorization: Bearer <sessionToken>`（24 小時）。`Idempotency-Key`：Client 自己產生（每個操作一把，重試用同一把）；同一把鍵的成功結果會原樣再回一次，被拒絕的不記，可以用同一把鍵再試。
- `connect`＝`{"server":"A.B.C.D:P","userId":7,"token":"<114 個 hex>"}`。**只有 started 房間的成員、而且只有呼叫者自己的那一份**。token 2 分鐘到期；成員每次 GET started 房間都會拿到新的一張（Match 還在就重簽），所以**建遊戲 World 之前、以及重連之前，先 GET 一次房間**拿新的 connect。
- 房間的變化用輪詢 `GET /v1/rooms/{id}`（建議 0.5～1 秒一次）。
- 常見的錯誤碼：`no_session`、`invalid_session`、`session_expired`（401，重新 `POST /v1/session`）；`room_not_found`（404）；`room_full`、`room_started`、`room_starting`、`not_all_ready`（409）；`not_owner`、`not_member`（403）；`unknown_mode`、`unknown_map`、`invalid_room`（400）；`no_match_available`、`allocation_failed`（503，房間回到 open）；`match_ended`（410：Match 已結束，房間刪掉；之後是 404）；`unknown_route`、`unsupported_version`（404，後者附 `"versions":[1]`）；`service_unavailable`（502，Gateway 後面的服務連不上）。
- 開始：房主＋**所有成員（含房主）都 ready**。開始後房間不能加入、不在列表。

## 3. Client 這邊可以直接用的

| 要做的 | 用什麼 | 在哪 |
|---|---|---|
| 送 HTTP、每幀輪詢 | `GYO::Http` 的 `HttpCall::Start(address, request)`、`Poll()`；放在大廳／房間 World 的 Resource，由 System 輪詢；放棄＝丟掉物件 | `engine/modules/http/include/engine/http/Http.hpp`、`docs/architecture/target/v2/http.zh-Hant.md` |
| JSON | `GYO::Json`（`Parse`、`Writer`） | `engine/modules/json` |
| `"A.B.C.D:P"` → 位址 | `client::ParseMatchAddress` | `games/object_fps_pvp/include/object_fps_pvp/client/LobbyWorld.hpp` |
| token 的 hex → 位元組 | `data::DecodeHex` | `games/object_fps_pvp/include/object_fps_pvp/data/Hex.hpp` |
| 連線目標 | `NetConnectTarget{server, userId, token}` 放進遊戲 World 的 prepare | `engine/modules/net/include/engine/net/NetService.hpp`（#332） |
| 裝置 id 存哪 | 使用者資料夾（`OS::UserDataDirectory`）裡的一個檔；**用高熵亂數產生**（裝置 id 就是憑證，宣告 L7）；同一台 Mac 開兩個 Client 時用不同的檔（例如 `--profile NAME`） | `engine/core/os/include/engine/os/Files.hpp` |

- HTTP 的 `Host` 標頭填 Gateway 的 `A.B.C.D:P`；body 是 JSON 時加 `Content-Type: application/json`（Gateway 不檢查，但照慣例）。
- 只收 IPv4 字面值（沒有 DNS，宣告 L3）。

## 4. Match 端（已做好，Client 不用管，但要知道的行為）

- Match 驗 token：長度、版本、簽章、Match、user、到期；同一 userId **從別的 endpoint** 的第二條連線拒絕（「the user is already in the match」）。從同一個 endpoint 重連照收（#332：同目標沿用 socket，所以「同一個 Product 裡換新的遊戲 World 重連」是同一個 endpoint）。Client 換了 socket（例如整個 Product 重建）時，要等 Match 上舊的連線逾時（10 秒）才進得去（宣告 L5）。
- 拒絕的理由在網路的 `Refused` detail（`token expired`、`token user`、`the match is full`…）。
- Match 在沒人的狀態持續 60 秒後結束；之後 GET 房間是 410 `match_ended`。
- 存檔：玩家連上時 Match 向 saves 讀，離開時寫回（`matches + 1`、`deaths` 加上這場的）。格式 `{"saveVersion":1,"matches":N,"deaths":N}`（`games/object_fps_pvp/include/object_fps_pvp/match/PlayerSave.hpp`）。Client 用 `GET /v1/saves/me` 讀（例：大廳顯示戰績）。

## 5. 本機跑起來

```bash
build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-services all --gateway 127.0.0.1:7700 --data <建置輸出下的資料夾> --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match --content assets/object_fps_pvp
```

- 服務的執行檔由 `cmake --build <build> --target gyo_object_fps_pvp-services` 產生（要 Go）。
- `--content` 讓房間只收 Match 有的地圖（`pvp_training_v1`、`pvp_corners_v1`、`biome_trial`；檔名不是 id）。
- Match 由 matches 服務起在 7800～7899（`--first-port`／`--last-port`），advertise 預設 127.0.0.1（跨機時給 `--advertise`）。
- 一行一行試：`curl -s -X POST localhost:7700/v1/session -d '{"deviceId":"x"}'`，再帶 `Authorization: Bearer …` 建房、ready、start、GET 房間。

## 6. 還沒做的（框架卡的範圍或之後）

- 大廳與房間的 UI（U：`ui`、`world_ui`、`ui_render`，`docs/architecture/target/v2/ui.zh-Hant.md` 第 9 節）、錯誤的顯示。
- Client 帶 token 經 Gateway 連上真的 Match 的端到端測試（第 2、4 段只驗到 Match 端：token 向量、admission、對假 saves 的讀寫、真的 Match 行程起得來與 idle 結束）。
- 完成條件的實機部分：這台 Mac 兩個 Client 經 Gateway 建房、加入、準備、開始、連上同一個 Match 對戰；撤離後存檔寫回、下次讀得到。
- 快速配對、推送（取代輪詢）、商人、任務、合作／生存的規則與結算。

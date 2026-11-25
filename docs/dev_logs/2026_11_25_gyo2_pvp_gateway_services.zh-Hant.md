# 2026-11-25 PvP Gateway 移植第 1 段：Gateway 與後面的服務（`v8-product-pvp-gateway`）

Owner：Object_FPS_PVP（產品）。GYO2 主會話開的任務卡（PvP Gateway 移植）。作者 Opus 5.5／high。
根據：[事前宣告](../../games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md)（第 1 節第 1 段、第 2 節 D1～D6、第 5 節）；使用者 2026-11-25 的 `v8-arch-gyo2-game-flow-and-gateway`、`v8-arch-gyo2-gateway-in-pvp-port`。新 AGENTS：宣告寫完就動工。

## 做了什麼

1. **Go module** `games/object_fps_pvp/go.mod`（`gyo.local/object_fps_pvp`，go 1.24，只用標準庫）；`gyo_game_add_go_service(services …)`，CTest 加 `object_fps_pvp.go.vet`、`.go.test`（Linux 另有 `.go.race`）。go.mod 放在遊戲目錄根：`gyo_game_add_go_service` 讀目錄屬性 `GYO_GAME_CONTEXT`，子目錄拿不到。
2. **五個服務**（`games/object_fps_pvp/services/`）：
   - `gateway`：無狀態；驗 session token、把身分（`X-GYO-User`）與 Client 的 `Idempotency-Key` 放進內部請求、轉給後面的服務、回應原樣轉回；房間 id 只收數字（不能改內部路徑）。
   - `identity`：裝置 id → userId（`identity.json`，先寫暫存檔再改名）、session token。
   - `rooms`：列表、建房（模式 coop／survival、地圖）、加入（一人一房，加入別的房會離開原本的）、離開（房主走了交給下一位、空房刪除）、準備、開始（房主＋全員準備；`starting` 時什麼都不能改；分配失敗回 `open`；`started` 不列出、不能加入、成員拿自己的 `connect`）。
   - `matches`：provider 介面＋本機行程的 provider（port 範圍、金鑰經環境變數 `GYO_PVP_ADMISSION_KEY`、行程結束回收 port）；每個 Match 一把金鑰，給每位玩家簽 admission token。
   - `saves`：每位使用者一份、版本號樂觀鎖、冪等鍵；內容不解讀。
   - 共用：`internal/api`（JSON body 上限與拒絕未知欄位、錯誤、內部 client、`Replays`＝冪等）、`internal/store`（原子寫檔）、`token`（admission 61 bytes 二進位、session base64url，皆 HMAC-SHA256）、`stack`（同一行程起全部、各自 listener）。
   - 執行檔 `gyo_object_fps_pvp-services`：子命令 `all`、`gateway`、`identity`、`rooms`、`matches`、`saves`。
3. **動工後改的**：外部協議的版本改成路徑 `/v1`（宣告 D3 已改）；`Replays` 的進行中 channel 改成每個請求自己持有（突變暴露原本會雙重 close）；rooms 向 matches 要 Match 的冪等鍵每次隨機（room id 在 rooms 重啟後重來，matches 的紀錄不會）。

## 驗證

- `go vet`、`go test -race -count=1 ./...`（本機 Intel Mac，go 1.27.1）：token、api、identity、saves、matches、stack 全過。
- `ctest -L go`（CMake 的 test preset）：`object_fps_pvp.go.vet`、`.go.test` 2/2。
- 端到端（`stack`）：兩個使用者經 Gateway 建房→加入→準備→開始→各拿到自己的 `connect`，用 provider 收到的金鑰驗得過；開始後不能加入、不在列表、非成員看不到 ticket；滿房、分配失敗回 open、房主交接、換房、冪等鍵、沒有或偽造的 session、玩家經 Gateway 讀自己的存檔且沒有寫入的路徑。
- `matches` 的本機 provider：以測試執行檔當假 Match，檢查參數與金鑰、port 用完回 `ErrNoPort`、行程結束後 port 可再用。
- 冒煙：真的執行檔 `all` ＋ curl：session → 建房 → 準備 → 開始 → `connect`（provider 起了 `/usr/bin/true` 當 Match、結束後回收）。
- 突變 `pvp-gw-` 21/21 killed（`replays-no-wait` 第一次 wrong_failure：突變讓程式 panic，不是要守的行為；改 `Replays` 每個請求持有自己的 channel，突變改成「不等進行中的同鍵請求」後 killed）。
- **沒驗**：C++ 端（第 2 段起）；Windows、Linux 的 Go 由 CI（Linux 跑 vet、test、race，並交叉編譯四個平台）。

## 設計審查

（審查後補）

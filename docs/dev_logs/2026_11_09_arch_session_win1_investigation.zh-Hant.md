# 架構 v8：11/9 場次 win1「斷線」的調查（v8-arch-session-win1）

日期：2026-11-09。做的人：架構師會話（使用者 2026-11-09：「我的 win1 出現斷線，你把這個問題交給架構師」）。性質：**調查與分類，不修**（AGENTS §14.3：成因確定的部分交 owner 固定成測試再修；不確定的部分加觀測）。
證據：主 checkout `build/evidence/v8-lag-l2-session-20261109/session/`（`clients/win1/`、`mac/`、`marks.txt`）。程式 commit `24943706`（#167 pa-broadphase＋biome、#168 移動診斷）。win1＝i7-8750H、GTX 1060＋Intel UHD 630＋「Easy&Light Display HUB Virtual Display」、Windows 11 Pro；radius 0.25、cell 0.5 m。

## 使用者看到的（win1 的玩家）

忽然看到別人的移動是跳躍式的（像瞬間移動），接著自己無法移動，等了 30 秒沒有掉線、沒有提示，別的 Client 也看不到他動，只能 Esc 退出重進。發生三次。

## 從 log 能證明的

1. **三次「斷線」都是 Esc 離開，不是連線中斷。** 10:16:46、10:20:06、10:38:25 三次：UDP 無中斷（`connection summary interruptions=0`）、Match 記 `leave reason=client_request`、Gateway 當下收到 `/leave`。程式在對局中送 leave 的路徑只有 Esc（`PvpApplication.cpp:1147`）。整場 Match 對 win1 的輸入 600 個／10 秒、全部 accepted；snapshot 60/s；fps 60（1334 個秒窗只有 2 個長幀 >50 ms）；時鐘同步延遲與 win2 相同。**wire 與幀都正常。**
2. **第一次（player 5）的「無法移動」是碰撞的卡死，Match 與 Client 兩邊的移動診斷一致，持續 42 秒。** 位置 (39.747, 0.787, 11.252)，biome 的一個 0.5 m 高台階的內角（格子 y=1 層：x≤78 與 z≥23 為實心，玩家中心在 x=79.5、z=22.5 的格線交點）。每 tick：`grounded=false`、`desired_y`＝y−2.5 mm（重力想往下）、`sweep_hits=5`（全部疊代都撞）、`sweep_normal=(0.989, 0.148, 0)`（膠囊下半球壓在台階的上緣，法線幾乎水平、y 分量 0.148 < 0.5 不算支撐）、`sweep_fraction=0`（一步都動不了）、`vertical_velocity=0`（每 tick 歸零）。於是：掉不下去（每 tick 被歸零）、不算站著（跳不了）、水平也推不動——**懸在台階邊緣 0.29 m 的空中**，直到 Esc。這就是 movement-fix.md 列的機制（擦邊歸零＋邊緣支撐角度＋沒有跨階）的第一個真實重現，是 pm-detect 要的失敗案例。
3. **第二次（player 6）的「無法移動」不是碰撞。** 10:19:38 起 14 秒原地不動（`moved=0`、無 sweep hit、grounded），被殺死、重生在 (10.25, 1, 10.25) 後又 14 秒不動，然後 Esc。Match 收到的輸入數正常，但移動診斷顯示位移為零 → 輸入內容是「不動」。命令紀錄沒有記鍵盤內容，**分不出是鍵盤輸入沒進到遊戲（焦點、裝置）還是沒按**。
4. **離開後的 HTTP 失敗只有 win1 有。** 第二、三次離開後約 10 秒，房間列表每秒一次的刷新開始失敗（先「Could not establish connection」再 2 秒逾時），第二次持續到 10:23:00 重新加入才好；每次刷新都是新的 TCP 連線。同一時間 Mac 對 win2、win3 的 UDP 正常。兩次都在 win1 的 UDP 流停了約 10 秒之後開始。Gateway 不記 HTTP 請求、Client 不記 socket 錯誤碼，**成因分不出來**（候選：win1 網卡在流量停止後省電或 Wi-Fi 漫遊、Windows 網路設定檔、Mac 側對新連線的拒絕）。
5. 「別人跳躍式移動」：Client 的 `runtime_gap` 紀錄在 10:15:58（第一次死亡重生時）有 20 筆連續 `pending=12`、每筆丟一幀的追趕，其他時刻沒有；10:19:38 前沒有紀錶。只能說「有一次 200 ms 的追趕」，不能證明它就是使用者看到的那次。
6. 受擊回饋：win1 收到 87 筆 `hit feedback`，62 筆 `flash=1 reason=played`。程式認為有畫、有放；使用者說沒看到。顯示路徑沒有紀錄（GPU adapter、視窗在哪個螢幕）。

## 分類與歸屬

| 現象 | 性質 | owner | 下一步 |
|---|---|---|---|
| 第一次卡死（台階內角懸空） | 缺陷 #1，**已重現**（位置、地圖、每 tick 的紀錄） | 產品 movement-fix（pm-detect → pm-vertical） | pm-detect 直接用這筆資料固定成失敗的測試（§14.3），不必再找重現 |
| 第二次原地不動 | 未分類（輸入沒進遊戲？） | 產品 | 加觀測：每秒記輸入摘要（有無移動鍵）、視窗焦點得失事件 |
| 離開後 HTTP 失敗 | 未分類（網路） | 產品／Gateway | 加觀測：Gateway 記每個 HTTP 請求（來源、路徑、狀態、耗時）；Client 記 socket 錯誤碰與是連線或讀取失敗 |
| Esc 離開 | 使用者操作 | 產品 | Client 把 Esc 離開記成一行，之後不必推論 |
| 看不到受擊回饋、跳躍式移動 | 未分類（顯示路徑） | 產品／field-session C1、C5 | 記 GPU adapter 名稱、視窗所在螢幕、每秒 present 的 p95 |

## 給 field-session.md 的缺口（下次場次前補）

C1：GPU adapter 名稱與視窗所在螢幕、網卡種類（Wi-Fi／有線）。C4：Gateway 的 HTTP 請求紀錄。C5：每秒輸入摘要、視窗焦點事件、present 的 p95。C7：Client 的 socket 錯誤碼；Esc 離開的一行。

## 用時與會改變的判斷

約 2.5 h。會改變的判斷：場次的「斷線」回報要先分「連線層」與「玩法層」——這次三件都是玩法層（碰撞卡死、輸入不動）加一件連線層（HTTP），而連線層那件只在玩家離開後才出現；移動診斷證明了它的價值：沒有它，第一次卡死只會是一句「卡住了」。

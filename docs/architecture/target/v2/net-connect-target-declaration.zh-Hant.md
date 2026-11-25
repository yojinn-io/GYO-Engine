# 連線目標跟著 World、socket 延後開的事前宣告（`v8-arch-gyo2-net-connect-target`）

日期：2026-11-25。PvP 移植實作卡（GYO2 主會話開的任務卡）。作者 Opus 5.5／high；設計審查 Opus／high（Fable 暫停中，同模型，報告寫明）。
根據：[net 頁](net.zh-Hant.md)第 6、9 節（D-8）；[net 的事前宣告](net-module-declaration.zh-Hant.md) D-8、P8、P15；[runtime 的組合](../runtime-composition.zh-Hant.md)第 2 節；R1b 卡的發現（主會話 2026-11-25 轉達）。

**使用者的決定**（2026-11-25）：「選(1)，socket延後開。」核心要求：「遊戲開始就會連服務器這個是完全不能接受的，因為我有單人模式」；模型：「world 看成場景，單人模式也是一個 world，多人模式是另一個 world」。新 AGENTS：宣告寫完就動工。

## 0. 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-gyo2-net-connect-target` | 這一批：連誰（位址、userId、token）從 Product 的設定搬到 client World 的 Resource `NetConnectTarget`；沒有就不連；Client 的 UDP socket 第一次要連時才開 |

## 1. 要解決的問題（遊戲的例子）

- 有單人模式的遊戲：玩家只玩單人，程式卻一啟動就開 UDP socket（`NetModule::Configure`），而且只要有 client World 就立刻握手（World 準備時 `SetFacts(newWorld)`），連的位址是建 Product 時寫死的 `NetProductOptions::server`。
- PvP 的大廳：大廳時不能連；送出位址後才連那個位址。現在只能「大廳一個 Product、送出後關窗、用新位址另建一個 Product」。R1b 提供「同一個 Product 換前台 World」之後，要的是「換的時候帶這次連誰」。

## 2. 前提：Engine 要解決（net Module）

- 何時開 socket、何時握手、對誰握手，只有 net 決定：`NetService` 建構時固定 `serverEndpoint_`；userId、token 在 Product 層的 `NetProductOptions`；net 的準備函式一建 client World 就開 session。
- 作者唯一的繞法是自寫一個 Link 在執行中改寫目的地、大廳期間丟掉送出的封包讓握手逾時——靠逾時、一直送握手、多一筆 `PeerDisconnected`，是繞過機制，不採。

## 3. 別家

| | 一個程式的 World | 單人與多人 | 連誰在什麼時候給 |
|---|---|---|---|
| Unity Netcode for Entities | 多個 World | 單人／選單 `CreateLocalWorld`（不帶 netcode，`Runtime/ClientServerWorld/ClientServerBootstrap.cs:98`），多人才 `CreateClientWorld`（`:339`） | 在 client World 裡 `NetworkStreamDriver.Connect(endpoint)`（`Runtime/Connection/NetworkStreamDriver.cs:259`） |
| lightyear（＋bevy_replicon） | 一個 World | 同一個 World，用狀態切換 | 執行中插入帶 `server_addr` 的 `NetcodeClient` 再 `trigger(Connect)`（`examples/lobby/src/client.rs:488-513`） |
| Godot | 一棵 SceneTree | 同一棵樹 | 執行中 `create_client(address, port)`（`modules/enet/enet_multiplayer_peer.cpp:76`） |

結構上和 GYO（一個 Product 多個 World）最像的是 Unity：多人才有 client World。lightyear、Godot 只有一個 World，連線只能是 World 裡的動作。

**選 (1)（使用者）、退掉 (2)**：
- (1) 連誰跟著 client World（準備時給）：單人 World 沒有 net 的角色，不連也不開 socket；多人 World 帶目標才連；換連線＝換 World，舊的複製實體、回滾歷史跟著舊 World 丟掉（D-8 的理由不變）。
- (2) World 裡送 Connect／Disconnect 訊息（lightyear 式）：只多給「斷線超過逾時後不重進場景就重連」，沒有人點名；要改寫 D-8，並重新面對 D-8 退掉的「網路自己刪上一個連線的複製實體、和回滾的存活集合打架」。10 秒內的短暫斷線兩者都由「中斷」狀態保住（P8）。
- 「掉線回來角色還在原地」是 Match 的遊戲規則（P15），兩者都不管。

## 4. 設計

1. **`NetConnectTarget`**（`engine/net/NetService.hpp`，net 在每個帶角色的 World 登記的 Resource）：`server`（`std::variant<std::monostate, OS::DatagramAddress, Endpoint>`：UDP 時是位址、產品給的 Link 時是 Endpoint）、`userId`、`token`。由產品的準備函式（`WorldDescription::prepare`）或替換的 `ReplaceRequest::extra` 放入。不是判定資料（和 World 的其他 facts 一樣）。
2. **`NetProductOptions`** 拿掉 `server`、`serverEndpoint`、`userId`、`token`；留 `role`、`bind`、`link`、`session`、`admission`、`clock`、`seed`。
3. **讀的時機**：net 的準備函式在作者的準備函式與 `extra` 之前跑（O-7），所以不在那裡讀。client World 的第一幀，`ReceiveNet` 最前面讀一次：`NetService::SetTarget(target 或 null)`。
4. **`SetTarget` 的規則**：
   - 沒有目標（或位址種類和傳輸不合）：還活著的 session 關掉（Disconnect 封包當場送出），之後不連。種類不合計一次 link failure（`LinkFailed`，審查 N3：`Unreachable` 是 OS 的「不可達」），照 E2 由 `NetLinkFailed` 告訴 World。
   - 和還活著的 session 同一個目標（位址、userId、token 都相同）：沿用，新 World 照 R2 送 resync（原本在準備函式裡判，搬到第一幀，同一個條件：請求帶的 epoch 不是這個 World 的）。
   - 不同的目標：舊 session 關掉、Disconnect 送給舊的位址，下一次 `SetFacts` 開新的 session。
   - 已經斷掉的 session：還沒送完的 Disconnect 先送給舊的位址（產品 `Close` 後馬上替換、中間沒跑一幀時，Disconnect 還在 session 裡；審查 B1），再丟掉，下一次 `SetFacts` 開新的（D-8：重連是新的 client World）。
   - 新連線的請求帶的 epoch 是 `SetTarget` 時給的這個 World 的 epoch（審查 S1：第一幀之前又有一次失敗的替換時，`BeginClientWorld` 的計數已經往前，不能用「現在的計數減一」）。
   - 替換的 `beforeSwap` 跑了 `Main`（`ReceiveNet` 在裡面）之後替換失敗、舊 World 留下時，舊 World 的連線已被結束：舊 World 的 `ReceiveNet` 發現 session 沒了、而 `clientPeer` 還在時，照一般的斷線刪掉 Peer、發 `PeerDisconnected`（審查要使用者決定的第 3 項，作者照 (b) 做：機制內的正確性，不是新規則）。
   - 同一個 World 裡斷線之後不自己重連（D-8 不變）。
5. **socket 延後開**：
   - Client 角色：`NetModule::Configure` 不開 socket；`SetTarget` 第一次拿到 UDP 的位址時才 `UdpLink::Open(bind)`。開不了時計一次 link failure（`NetLinkFailed` 帶 OS 的原因），不連；下一個帶目標的 World 再試。原本的啟動錯誤 NT-E4 只剩 server 角色。
   - 回到沒有目標的 client World 時（`SetTarget` 拿到沒有目標）：送完剩下的 datagram 後關掉 socket，下一個帶目標的 World 再開（Client 的 port 每次不同）。使用者 2026-11-25 定（「單人不碰網路」：打完多人回到單人時不留著 port；Unity 的 driver 跟著 client World 銷毀）。
   - Server 角色（Match）：照舊在 `Configure` 開（要 listen；開不了是啟動錯誤 NT-E4）。
   - 產品給 Link 時不開 socket（照舊）。
   - `NetService::UdpAddress()` 在開之前回 nullopt。
6. **D-8 的補充**（net 頁第 6、9 節）：「新 World 準備時才開新的握手」改成「新 client World 的第一幀照它的 `NetConnectTarget` 開新的握手；沒有就不連」。「重連＝新的 client World」不變。net 宣告的 D-8 原文沒寫「準備時」，不用改（審查 S2 更正原寫的「net 宣告 D-8 也補」）。

## 5. 測試與突變（事前宣告的判定）

| # | 判定 |
|---|---|
| J1 | client World 沒有 `NetConnectTarget`：不握手、Match 沒有連線、UDP socket 沒開（`UdpAddress()` 是 nullopt） |
| J2 | 準備函式給目標：連上（行為和改之前相同，既有的 net 測試照新形改寫後全過） |
| J3 | 替換 World：同一個目標而連線還活著時沿用並 resync；換成另一個目標時關舊的、連新的；換成沒有目標時關掉、不連 |
| J4 | 目標的位址種類和傳輸不合：不連、計一次 link failure |
| J5 | UDP：socket 在第一次拿到目標時才開；server 角色照舊在啟動時開 |

突變：沒有目標時沿用上一個；同目標時不沿用（每次重連）；不保留的 session 不送 Disconnect（`nt-target-end-not-sent`，原列「換目標時不關舊的」：同一個 endpoint 帶更大的 salt 的新握手也會讓 Match 結束舊連線，從外面分不出，改由「沒有目標時關掉」與「Close 後馬上替換」兩案判；審查 S3）；socket 在 Configure 就開；種類不合時照樣連；`Close` 後馬上替換時 Disconnect 不送（`nt-target-closed-dropped`）；開不了 socket 時照樣當有連線（`nt-target-open-failure-ignored`）。

## 6. 不做

- Connect／Disconnect 訊息（(2)）、同一個 World 裡斷線後重連。
- 斷線時網路自己刪複製實體（D-8 退掉的 (a)）。
- 「掉線回來角色還在原地」（Match 的規則，P15）。
- PvP 改成「同一個 Product 換前台 World」：等 R1b 與這一批都合併後，在 PvP 卡接。這一批只把 PvP 的 Client 與測試照新形改寫，行為不變。

## 7. 模型與檔位

實作 Opus 5.5／high；設計審查 Opus／high（同模型，報告寫明）。

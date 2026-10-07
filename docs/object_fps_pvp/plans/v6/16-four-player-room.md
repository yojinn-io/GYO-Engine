# 第 16 批：房間上限 4 人

狀態：完成（2026-10-07 開始，分支 `claude/pvp-v6-batch16` 自 master `7886848`；規劃：ultracode workflow `wf_45d62ed8-68c`，對抗檢查的 major 3、minor 8 已處理）。程式（commit 1～7、9～11）完成，L1 本機通過，L2、L3 完成（[dev_log](../../../dev_logs/2026_10_07_pvp_v6_batch16.zh-Hant.md)）。**完成**，PR [#63](https://github.com/yojinn-io/GYO-Engine/pull/63) 待合併。
執行順序在第 13 批之後、第 14 批之前（D22、D23）。先讀 [進度](README.md)、[交接](HANDOFF.md)、[v6 契約](../../protocol-v6.zh-Hant.md)。

盤點的基準是 master `3607fe3`。開始時已在 `7886848` 重新核對（第 11～13 批之後）：Gateway、Match、Client 解碼與 arena 的位置不變；Client 的遠端呈現槽在 `PvpApplication.cpp:773`（容量 1），phase reanchor 的基準在 `:630`、`:654`（在迴圈內逐人更新，多個遠端時互相干擾）。

執行規則沿用 v5 與本計畫的 README：只做指定範圍、先凍結再量測、失敗的跑次保留、長測另外授權。

## 目標與範圍

房間最高人數由 2 人改為 4 人（使用者 2026-10-07 決定，D22）。

做：

- 人數上限在 Gateway、Match、Client 三個角色同時改為 4。C++ 與 Go 各有一個具名定義，加一致性測試。
- arena v1 的出生點數放寬為 2～64；Match 只主持出生點數不少於人數上限的 arena；產品 arena 追加 2 個出生點。
- Client 能同時呈現 3 個遠端玩家。
- 修正既有缺陷：玩家被逐出時，Gateway 沒有清除他在 runtime link 的狀態。
- 4 人驗收：quad 案例、1 GUI＋3 bot、版本混用時明確拒絕。

不做：

- 不升協議版本，proto 不變（決定 D23①）。
- 不改 digest 程式與 golden（pv6 §7）；不重新產生 golden。
- 不改第 07a 批凍結的 Python 分析器。2 個 Client 的工具中，GUI probe 與 3 個 GUI runner 依使用者 2026-10-07 的決定改為支援有 bot 的房間，2 人房的判定不變（見「事前宣告的修訂」）。
- 不修 Match→Gateway 的 IPC 寫出迴圈（D21，交給 v7 任務 3）。
- 不做正式的 4 人地圖（四角分散的出生點）、隊伍、外觀區分：留給玩法計畫 [v8](../v8/README.md)。

## 使用者決定（2026-10-07，D23）

| # | 決定 |
|---|---|
| ① | 維持 pv6。Snapshot 的人數上限與 `Ready.max_players` 由 2 改為 4，屬 pv6 §1 的「改語意」wire 變更，依 D11① 經使用者同意：三個角色同一個 commit 切換，舊程序重啟。pv6 契約加有日期的修訂 |
| ② | 同意本批作為「權威結果只在第 10 批改變」的例外，範圍只限 3 人以上才遇得到的規則（第 3、4 人加入、同一 Tick 多位射擊者、多位阻擋者時的重生選擇）。以 Match 單元測試鎖住；digest 程式與 golden 不動。2 人的 35 個 digest 情境預期不變 |
| ③ | 產品 arena 的新出生點放在現有兩點 (5,5)–(5,12) 的線段上：(5,7.5)、(5,9.5)。2 人時每個位置的出生選擇都不變。四角分散會改變 2 人的出生位置、需要改寫驗收幾何，已說明後由使用者改選線段；正式的 4 人地圖留給 [v8](../v8/README.md) |
| ④ | 4 人量測時若 IPC 寫出迴圈撐不住：約 8 ms 主機狀態下的結果標「未驗證」並記錄，作為 v7 任務 3 的壓力證據；約 4 ms 狀態必須通過。不在本批修迴圈（D21） |
| ⑤ | 第 14b 批加入 1 GUI＋3 bot 一輪，穩定基線涵蓋 4 人容量；長測維持 2 個 Client |
| ⑥ | 逐出時不清 runtime link 的既有缺陷，在本批以獨立 commit 修正 |
| ⑦ | 批次順序：12→11→13→16→14 |

## 盤點摘要

2 人的假設（基準 `3607fe3`）：

- Gateway：`adapter.MaxPlayers = 2`（`adapter.go:20`）。依賴它的有 readiness 的完全相等檢查（`runtime_link.go:50`）、Snapshot 人數檢查（`adapter.go:77`）、輸入槽（`runtime_link.go:115`）、動作窗口（`action_delivery.go:141`）。房間 JSON 與 `room_full`（`server.go:199`、`:248`）讀 `Ready.max_players`，不需要改。
- Match：`set_max_players(2)`（`IpcHost.cpp:140`）與 `match_full`（`PvpMatch.cpp:33`）是兩個獨立的字面值。
- Client：超過 2 人的 Snapshot 直接丟棄（`ClientConnection.cpp:428`）。遠端呈現只有一個槽（`PvpApplication.cpp:731`），phase reanchor 的基準在迴圈內（`:602-603`、`:623`）。
- arena：v1 規定恰好兩個出生點（`Arena.cpp:32`）。
- 其他：同一個事實在 C++ 有 5 處字面值、Go 有 1 處常數，彼此沒有一致性檢查。
- 既有缺陷：逐出時只移除預約（`server.go:558`），不清 runtime link 的動作窗口、epoch、生命與輸入。被逐出的玩家用過動作後，補進來的玩家在 2 人房間會撞到動作窗口上限，開槍與換彈默默送不到 Match。

大小（以 repo 的 Go binding 實算，契約最大編碼）：

| 玩家數 | 完整 Snapshot UDP |
|---|---|
| 2 | 545 bytes（與 §7 相同） |
| 3 | 800 bytes |
| 4 | 1055 bytes（型別最大值 1135） |
| 5 | 1310 bytes（超過 1200） |

- 公式：35＋255N bytes。4 人時剩 145 bytes。
- §7 寫的「worker probe 實測 587 bytes」是 ActionResults，不是 Snapshot，本批更正。Welcome 最壞 213 bytes，不變。

## 契約變更（pv6 修訂，加日期）

- §1：
  - Snapshot 的 players／combat 上限 2→4；`Ready.max_players` 的值為 4；readiness 維持完全相等；房間容量 4。超過上限沿用既有的無效路徑。
  - 新增容量定義表：C++ `fps::pvp::MaxPlayers`、Go `adapter.MaxPlayers`、驗收 C++ 與 Python 各一個，加一致性測試（比照 `ProtocolVersion`）。
- §2：
  - arena v1 的出生點數為 2～64；Match 只主持出生點數不少於 `MaxPlayers` 的 arena；本批之前的 pv6 二進位會拒絕不是 2 點的 arena。版本與正規化不變（先例 D11⑨，契約中註明）。
  - 同一受害者每 Tick 最多被命中 3 次；`last_attacker_id` 取裁決順序的最後一位；致命一擊之後的射擊穿透。
  - 日後修改 arena 時，要保持「新舊版本混用必定明確失敗」的性質。
- §5：出生點選擇（離存活對手最遠，同距時取內容順序較前者）寫成明文。
- §7：大小改為上表；更正 587 bytes 的說明。`network-architecture.zh-Hant.md` 的相關段落一併更正。

版本混用的結果（全部明確失敗）：

- 舊 Client 只能載入 2 點 arena；新 Match 拒絕少於 4 點的 arena。兩者的 arena digest 必然不同，連線時報 `arena_content_mismatch`。
- Gateway 與 Match 跨版本：readiness 要求人數上限完全相等，啟動失敗。
- 新 Match 用 `--arena` 載入舊的 2 點 arena：主持條件不成立，啟動失敗。

## 事前宣告（2026-10-07，commit 1 寫入，寫程式之前；之後不得放寬）

**權威：**

- 第 03 批的 35 個 digest 情境，在下列每個檢查點與 base（`7886848`）做同機兩樹比對，預期差異集合為空：容量常數化、arena 出生點、容量 2→4、head。golden（SHA-256 `f01f3a32…`）不變、不重新產生。
- 理由：沒有任何情境加入第 3 位玩家；產品 arena 的新出生點加在既有兩點之後，位於兩點之間的開線段上，且 `FindSpawn` 用嚴格比較，所以 2 人時每個位置的選擇都不變（規劃時以模型與讀碼確認）；digest 程式與合成 arena 都不動。
- 3 人以上才遇得到的規則（D23②）以 Match 單元測試鎖住：4 人加入、第 5 人 `match_full`、2 點 arena 的第 3 人 `spawn_blocked`、同 Tick 2 位與 3 位射擊者、致命後穿透、同時重生、出生點選擇、全部被擋時逐 Tick 重試。

**產品 arena：**

- 追加兩個出生點，放在既有兩點之後：(5, 0, 7.5) yaw 0、(5, 0, 9.5) yaw π（3.14159265）。yaw 沿用兩端的規則：朝向較近的那個端點的方向，也就是兩端點互相面對的方向。
- 內容測試鎖住：出生點數 4、`spawns[0..1]` 不變、新點在兩端點之間的開線段上、彼此間距 ≥4r、座標與 yaw。
- 已知限制（使用者 2026-10-07 已知）：新點在兩端出生點的交火線上，沒有出生保護，4 人時容易出生即被擊殺；正式的 4 人地圖留給 v8。

**驗收工具：**

- 2 個 Client 的工具與分析器的判定邏輯不改。容量常數放在新檔（C++ `AcceptanceMaxPlayers`、Python `MAX_PLAYERS`），不改 `acceptance_util.py`。
- 4 人只新增檔案：quad probe、`run_quad.py`、`quad_evidence.py` 與它們的測試；`quad_evidence` 以 2 個 Client 的語料和 `command_evidence` 逐值交叉驗證，交叉驗證放進 CTest。
- 完成後記錄新的凍結清單雜湊，並同步 v7 README。

**L2 的比較範圍（這裡固定；跑次、分母與 IPC 的門檻在 L2 的 `declare.txt` 中，於任何量測之前寫入）：**

- 2 個 Client，before（`7886848`）對 after：
  - 裁決逐筆 0 差異只用於 clean-30、clean-60、clean-144 與動作短測。
  - 故障與網路劣化案例：比較結構計數（Miss／World／Player 與各拒絕類別），並逐案比對重生位置。
- 4 人：quad 的功能判定、clean-60 判定、clean-30 只記錄（D21）、1 GUI＋3 bot、版本混用時的明確拒絕。
- IPC 寫出：以 Match trace 的 `transport` 事件數、合併計數與 snapshot age，依約 4 ms／8 ms 主機狀態分層報告；約 8 ms 狀態撐不住時依 D23④ 標「未驗證」。commit「容量 2→4」之後、正式量測之前，先做一次不計入的 4 Client 開發量測。

## 事前宣告的修訂（2026-10-07，使用者核准，commit 9～11）

起因：commit 7 之後，使用者要求「GUI probe 等寫死 2 人的部分也要改」。盤點 2 個 Client 的工具，寫死的 2 約 50 處，幾乎都是「這個測試有 2 個參與者」，不是房間容量（容量相關的已在 commit 6 改完）。使用者選擇「GUI probe 支援 4 人房」，並核准下列計畫。

- 修訂的範圍：只限 GUI probe（`gui_main.cpp` 與 `combat_latency.hpp`、`action_short.hpp`、`player_short.hpp`）和 3 個 GUI runner（`run_action_short.py`、`run_player_short.py`、`run_timing.py --gui`）。headless 的 2 Client 工具與第 07a 批的 Python 分析器不改。native window 模式（X11）只支援 2 人房。
- 條件（不得放寬）：2 人房的判定與原本相同。
  - 不加 bot 時，GUI probe 的命令列與輸出不變，不寫任何新檔。
  - 全員到齊的判定等於原本的「`players.size()==2`」；combat 證據取 Snapshot 順序的兩筆，與原本相同；診斷觀測的遠端也是唯一的那一位。
  - 以 `GuiRoomTests` 鎖住，並以 2 人房的 action short、player short、GUI timing short（combat）實跑確認。
- 有 bot 時（`--bots N`）：
  - 兩個 GUI 以各自公開的 player ID 互相辨識。
  - 產品的診斷用 `ObserveRemote` 只觀測對方 GUI。
  - bot 為被動模式：不開槍，走到 -X 的牆邊（避開兩個 GUI 之間的射線，新出生點正好在這條線上），到位後才算全員到齊。
  - `run_timing` 有 bot 時以 `quad_evidence.command_metrics`（N 人版，已交叉驗證）判定指令階段。
- 凍結清單：commit `c8b1276` 的驗收工具 61 檔，清單 `logs/pvp-v6-batch16-dev-20261007/frozen-tools.sha256`（SHA-256 `b8eabc7c…`）。Python 分析器與第 07a 批清單逐位元組相同；與 07a 相比有變動的檔案，是第 09～13 批與本批的 probe、runner 和測試。

## commit 拆分

| # | commit | 內容 | 檔位 |
|---|---|---|---|
| 1 | 批次文書、pv6 契約修訂、事前宣告 | 本文件定稿、契約 §1／2／5／7、README、HANDOFF、事前宣告 | medium（§1 的分類局部 xhigh） |
| 2 | 容量常數化（值仍為 2） | 四個定義與一致性測試；5 處字面值改用常數；Go 測試改引用常數、`room_full` 斷言錯誤碼、readiness 雙向不符、超過上限的 Snapshot 故障。兩樹 digest 不變 | high |
| 3 | 逐出時清除 runtime link 的玩家狀態 | 比照 Leave 清除四個 map，不送 Leave；測試補進來的玩家動作送達 | high |
| 4 | arena 出生點 2～64、主持條件、產品 arena 追加 2 點 | `Validate` 放寬；主持檢查做成純函式並測試；digest 對出生點數敏感；內容測試。兩樹 digest 不變 | high（出生點證明局部 xhigh） |
| 5 | Client 遠端槽改為 `MaxPlayers − 1`，phase reanchor 改為每幀一次 | 遠端移動觀測維持一份，註明只供診斷；第 11、13 批的遠端呈現擴充到 3 人 | high |
| 6 | 容量 2→4（三角色同一 commit） | 四個定義改為 4、大廳文字；Match 多人測試；Gateway 扇出 4 與第 5 人 `room_full`；Client 收 4 丟 5；大小測試釘 1055 與 1019；新增突變。兩樹 digest 不變 | high（多人決定性局部 xhigh） |
| 7 | 4 Client 驗收 | 新檔：quad probe、`run_quad.py`、`quad_evidence.py` 與測試；以 2 Client 語料與 `command_evidence` 逐值交叉驗證，交叉驗證放進 CTest；quad runner 每段開始前斷言 `GET /rooms` 的 players==0 | high（交叉驗證局部 xhigh） |
| 7b | 大廳的房間容量預設值改用 `MaxPlayers` | `LobbyRoom` 的預設值與房間列表缺 `capacity` 時的值原為字面值 2；字面值檢查加入這 2 處 | high |
| 9 | 診斷可指定觀測的遠端（修訂） | `PvpApplication::ObserveRemote` 與純函式 `ObservedRemotePlayer`；未指定時維持「Snapshot 順序最後一位」；單元測試與突變 | high |
| 10 | GUI probe 支援有 bot 的房間（修訂） | `gui_room.hpp`、`--room-players`；quad probe 的被動模式；`GuiRoomTests`；突變 | high（2 人房不變的證明局部 xhigh） |
| 11 | GUI runner 的 `--bots`（修訂） | `gui_bots.py`；3 個 GUI runner | high |
| 8 | L2、L3 結果與同步 | dev_log、HANDOFF、README、第 14 批、v7 README；同步分兩次：實作完成後（L2 之前）一次，L2／L3 之後一次 | medium |

commit 6 之後、正式量測之前，先做一次不計入正式跑次的 4 Client 開發量測，確認 IPC 指標。

## 驗收點

L1（CI 四平台＋本機）：

- CTest、`go test -race`；一致性測試確認四個定義都是 4（含自我檢查）；產品程式中沒有容量的字面值。
- Match：4 人加入、第 5 人 `match_full`、重複 id 先於上限判定、2 點 arena 的第 3 人 `spawn_blocked`、同 Tick 2 位與 3 位射擊者、致命後穿透、同時重生、出生點選擇、全部被擋時逐 Tick 重試。
- arena：`Validate` 接受 2／4／64 點、拒絕 0／1／65 點；digest 對出生點數敏感；fixture 的 golden 不變；內容與主持檢查測試。
- Gateway：readiness 拒絕 3 與 5；第 5 人 409 `room_full`；5 人 Snapshot 故障；扇出 4；逐出後補進的玩家動作送達。
- Client：收 4 人、丟 5 人且不更新活性；大小：worker 1055、自測 1019、Go 1055。
- 驗收工具：`quad_evidence` 與 `command_evidence` 在 2 人語料上逐值一致（CTest）；`GuiRoomTests`；`ObservedRemotePlayer` 單元測試。
- 權威：四個檢查點的兩樹比對 35／35；golden 不變；突變全部 killed，既有 12 個突變不 stale。

L2（macOS Intel／Metal，事前宣告、凍結、機器閒置、先徵求同意，依主機狀態分層）：

- 2 Client before／after：25 案矩陣、雙 GUI、動作與人物短測，比較範圍依事前宣告。
- 版本混用：舊 Client 被拒、跨版本 readiness 失敗、新 Match 載入舊 arena 時啟動失敗。
- 4 人：quad 功能判定；clean-60 五輪判定；clean-30 只記錄（D21）；1 GUI＋3 bot。IPC 合併造成失敗時依 D23④ 處理。
- 修訂追加：2 GUI＋2 bot 的 GUI 短測（action short、player short、GUI timing short＋combat）各一輪。

L3（人工）：

- 使用者以 1～2 個 GUI 加 bot 確認：人數顯示 n/4、3 個遠端角色、擊殺與重生、第 5 人 `room_full`。
- 已知限制（不判失敗）：遠端外觀相同、同 Tick 時 id 較小者優先、出生點排成一線（新點在兩端點的交火線上，沒有重生保護）、受擊方向只指向最後一位攻擊者。

## 平台

| 平台 | 本批 | 理由 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | L2、L3、兩樹 digest 比對 |
| CI 四平台 L1 | 預定執行 | L1 全部 |
| Windows、Linux、macOS arm64 實機 | 未執行 | 沒有實機（D11⑩） |

## 建議檔位

主體 high；文件 medium。局部升 xhigh：pv6 §1 的分類、出生點不變的證明、多人決定性、交叉驗證。

## 依賴與順序

- 依賴：第 09、10 批（已合併）；第 11、13 批（遠端呈現先完成，本批擴充到 3 人）。
- 第 14 批依賴本批：14a 加入 4 人案例，14b 加 1 GUI＋3 bot 一輪（D23⑤）。
- `PvpApplication.cpp` 的合併順序：05→06→12→11→13→16。

## 對 v7 的影響

- v7 的任務與人數無關。
- 30 FPS 對比：before 是含本批的 v6 最終 tree；headless 的 2 Client 工具與第 07a 批分析器不變，GUI probe 在 2 人房的判定也不變，F1～F6 與幀率對照仍可比較。
- 4 人的 IPC 寫出負載（每秒約 120→180 個 frame）的 L2 數據，作為 v7 任務 3 的壓力證據。
- Snapshot 剩 145 bytes，v7 若要加 Snapshot 欄位，空間有限。v7 的心跳走 runtime link，不受影響。

## Architecture Delta

1. 需求：D22（房間上限 4 人）。
2. 問題：容量散在 6 處、沒有單一定義，舊 Client 遇到 4 人會靜默丟棄；arena 寫死 2 個出生點；遠端呈現只有 1 個槽，且 reanchor 的基準有誤；逐出時的狀態洩漏。
3. 邊界：Client／Runtime 契約的值域、arena 規則與主持條件、Join 上限。proto、版本號、arena 正規化、Engine 都不變。
4. 影響：只有 `object_fps_pvp`（Match、Gateway、Client、驗收、測試）。
5. 依賴方向不變。
6. Ownership：Match 擁有上限與主持條件；Gateway 做相等檢查；Client 擁有解碼上限與呈現槽位。
7. 更小的變更不可行：只改字面值會留下靜默的版本混用；升 pv7 或在 Welcome 加欄位則超出需要。

修訂追加（commit 9～11）：

- 產品的公開介面多一個只供診斷用的方法 `PvpApplication::ObserveRemote`。表示與模擬不變；依賴方向不變（驗收工具 → 產品）；Engine 不受影響。
- GUI probe 的 2 個 role 之間多一個檔案契約：`<role>-gui-player.txt`、`bots-parked.txt`、`bots-stop.txt`，只在有 bot 時使用。Owner 是本產品的驗收工具。

將來 Refactoring：

- `quad_evidence.py` 與 `command_evidence.py` 有重複的職責，v7 收斂（AGENTS §12）。
- 遠端診斷只有一份（`RemoteMovement`）；需要同時觀測多個遠端時再擴充。

## 完成條件與停止條件

完成：

- 事前宣告寫入後才開始實作；L1 全過；四個檢查點的兩樹比對都在宣告內；L2 依宣告判定；L3 使用者確認。
- README、HANDOFF、dev_log、第 14 批文件、v7 README 更新後停止，不自動開始下一批。

停止：

- 任一 2 人 digest 情境或 golden 改變。
- 4 人 Snapshot 超過 1200 bytes，或需要改 proto。
- 約 4 ms 主機狀態下 4 人驗收失敗（8 ms 狀態依 D23④）。
- 需要改 Engine 或共通層。

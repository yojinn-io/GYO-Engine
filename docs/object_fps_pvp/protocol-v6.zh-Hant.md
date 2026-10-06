# PvP Protocol v6：受擊、arena 內容與 Collision 契約

更新：2026-10-06。Owner：`object_fps_pvp`。
本文件的版本條款（§1 的版本、拒絕 v1～v5、標頭與 envelope 的值）中，vN 指網路協議版本 pvN；計畫、交接、升格與玩法語意（例如 v5 測試名稱）中的 vN 指遊戲版本（[v6 交接](plans/v6/HANDOFF.md) D20）。程式名稱 `client_v6`／`runtime_v6`／`clientv6`／`runtimev6` 指 pv6。
**wire 條款由 [第 09 批](plans/v6/09-protocol-v6.md) 定稿。** 第 09 批合併起，Client、Gateway、Match 共同使用 6，拒絕 v1～v5 與其他值；合併前，現行程式仍是 [v5](protocol-v5.zh-Hant.md)。
第 09 批是 v6 唯一的 wire 變更（D11①），權威結果不變。權威判定的唯一變更在 [第 10 批](plans/v6/10-collision-authority.md)，不改 wire。
§4、§6 中屬於第 10～13 批的項目標「待定，由第 NN 批決定」，在各批開始時補上。
v6 在第 14b 批升格之前是候選。v5 契約中沒有在此改寫的條款，v6 原樣沿用；v5 的文件與證據保留，不被覆寫。進度見 [v6 計畫](plans/v6/README.md)。

## 1. 範圍、時間與預設

- 沿用 v5 §1 的全部政策與門檻：60 Hz 權威、移動 worker、lead、相位追蹤、插值、動作交付、恢復、連線品質移出，以及完整 UDP ≤1200 bytes（含 24-byte 標頭）。
  - 相位追蹤不修改。30 FPS 是設計範圍的邊界，見 v5 §1 的 2026-10-06 追記；根本處理在 v7（D21）。
- v6 新增（第 09 批）：
  - 受擊欄位：`CombatState` 的最後受擊 Tick、同一生命內的受擊計數與最後攻擊者（§2），只供呈現。
  - arena 內容 digest：Match 在 Ready 發布，Gateway 轉送，Client 在 HTTP join 與 Welcome 兩條路徑比對（§2）。
- v6 不包含：
  - 爆頭、骨骼命中、射擊回溯。
  - 回合準備期（第 9 項，維持候選）。
  - 文字輸入的協議變更。
  - 受擊呈現（第 13 批，不改 wire）。
  - runtime 連線（Gateway↔Match）的心跳對時（v7）。

### 版本

只接受 6。GYOP 標頭、IPC envelope、HTTP join JSON 的值一致；v1～v5 與其他值一律拒絕：

| 位置 | 版本不是 6 時 |
|---|---|
| HTTP join 請求（Gateway） | 409 `protocol_version`，在保留名額之前拒絕；缺少視為 0。不是 0～65535 的整數時照 v5 回 400 `invalid_join` |
| HTTP join 回覆（Client） | 連線失敗回 Lobby（`Client protocol mismatch`）；已取得的保留照 v5 在下一個動作以 Leave 釋放 |
| GYOP 24-byte 標頭（Client、Gateway） | 靜默丟棄：不回覆，不改序號與活性 |
| IPC Ready（Gateway） | readiness 不符，Gateway 啟動失敗 |
| 之後的 IPC envelope | Gateway 停止 runtime 連線，房間 unavailable（v5 既有路徑）；Match 關閉該連線 |
| `GET`／`POST /rooms`、leave | 不帶版本，也不保留名額；舊版 Client 可以列出或建立房間，但 join 以 409 拒絕，無法取得 session |

- HTTP join JSON 的 `protocol_version`（以及 §2 的 `arena_digest`）是 JSON 無號整數。Client 以完整值比較：不是無號整數即拒絕，不截斷。
- GYOP 標頭的位元組配置不變。編解碼在 Engine（FF-8），只檢查傳輸條件；版本與訊息種類由產品傳入並判斷。Engine 與 `services/gyo_gateway` 不改。
- 版本值每個 owner、每種語言只有一個定義；產品與驗收程式的其他位置一律引用它，不寫字面值，測試的拒絕集合也由它導出：

| Owner | 定義 | 說明 |
|---|---|---|
| 產品 C++ | `fps::pvp::wire::ProtocolVersion`（`Wire.hpp`） | GYOP 標頭、IPC envelope、HTTP join 請求與回覆都引用它 |
| 產品 Go | `adapter.ProtocolVersion` | `ClientVersion`（uint16）與 `RuntimeVersion`（uint32）由它導出，不另寫值 |
| 驗收 C++ | `AcceptanceProtocolVersion`（`acceptance_protocol.hpp`） | 第 02a 批刻意不引用產品常數，作為獨立的預期值 |
| 驗收 Python | `PROTOCOL_VERSION`（`acceptance_util.py`） | 同上 |

- 跨語言一致性測試（產品 CTest，只在選取本產品時建置；Go 與 Python 的定義以原始碼文字解析，不依賴 Go 工具鏈）讀取上表四個定義，確認四者相同且等於 `fps::pvp::wire::ProtocolVersion`。
- 候選期規則：從第 09 批合併到第 14b 批升格之前，版本保持 6，原則上不再改 wire。若必須改，需要使用者同意，並且三角色同一 PR、舊程序重啟、不混用不同 commit 的 v6 候選。
  - 「改 wire」包含：proto 欄位的新增、刪除、改號、改型別或改語意，以及 §2 arena digest 的正規化或演算法（含 `Arena` 新增成員）。
  - arena 內容的修改不是 wire 變更：內容不同的 Client 與 Match 以 §2 的錯誤碼明確拒絕。

## 2. 身分與資料語意

| 資料 | v6 增補與規則 |
|---|---|
| CombatState | `last_damage_tick`（11，uint64）、`damage_count`（12，uint32）、`last_attacker_id`（13，uint64）；Client／Runtime 兩份同編號、同型別。C++ 為 `lastDamageTick`／`damageCount`／`lastAttackerId` |
| Ready（Runtime） | `arena_digest`（9，fixed64） |
| Welcome（Client） | `arena_digest`（10，fixed64），Gateway 照 Ready 原值轉入 |
| HTTP join 回覆、房間狀態 JSON | `"arena_digest"`：JSON 無號整數（十進位，與 `session_id` 相同的表示） |

兩份 proto 改名為 `client_v6.proto`／`runtime_v6.proto`（package `object_fps_pvp.{client,runtime}.v6`，Go 套件 `clientv6`／`runtimev6`）。
HTTP join、Ready／Welcome、wire 版本與建置引用在同一 PR 切換，不交付混用三角色的可部署組合。
v5 §2 的其餘身分、生命世代、動作帳本與 latest-wins 規則不變。`arena_version` 仍是 arena 格式版本（目前只接受 1），不是內容修訂號。

### 受擊欄位（D2）

- 語意：該玩家目前生命內最後一次受擊的權威 Tick、該生命內的受擊次數、最後一次受擊的射擊者 PlayerId。
- 受擊＝合法 Shot 命中玩家（裁決的 `hit_kind` 為 `HIT_PLAYER`）。射偏、打牆、任何拒絕、重送與 Reload 都不寫入；射擊者自己的三欄不因射擊改變。
- 寫入：只有 Match 寫入。裁決命中時，在扣血的同一處對受害者寫入：`damage_count` 加 1、`last_damage_tick`＝裁決 Tick（即該裁決的 `resolved_tick`）、`last_attacker_id`＝射擊者；之後才依 v5 §5 判斷 HP 歸零轉 Dead。致命命中同樣寫入。
- 同 Tick 多次命中：依 v5 §5 的裁決順序（acceptedTick、PlayerId、ActionId）逐次寫入。每次命中計數加 1，Tick 與攻擊者取該順序中最後一次命中。致命命中後受害者不再是目標，所以死亡 Tick 的攻擊者就是擊殺者。
  - v6 規則下（每局最多 2 人、冷卻 10 Tick、命中查詢排除射擊者），同一受害者每 Tick 至多被命中 1 次。可達的同 Tick 情境是互射，以及致命命中後死者在同 Tick 的射擊（以 Dead 拒絕，不寫入）。
- 「沒有受擊」：三欄全為 0。權威 Tick 從 1 開始、PlayerId 0 無效，所以 0 不會與合法值混淆；三欄同時為 0 或同時非 0。
- 生命隔離：死亡不清除，死亡期間保留到重生；成功重生時與其他戰鬥欄位一起歸零，新加入的玩家也從 0 開始。計數不跨生命累計，舊生命的值不殘留。
- 溢位：每次受擊至少扣 1 HP（傷害＝min(shot_damage, hp)，目標必為存活），生命內不回血，所以 `damage_count` ≤ maximum_hp；uint32 不會溢位，不回繞也不飽和。
- 攻擊者是歷史身分：可能已離開而不在同一份 Snapshot，呈現必須容許。
- Match 裁決、Client 預測與校正都不讀這三欄；它們不進入第 03 批權威 digest 第 1 版的欄位清單。
- 驗證（產品 Go adapter 的 Snapshot 轉換與 Client 的 Snapshot 解碼相同）：
  1. 三欄同時為 0 或同時非 0。
  2. 非 0 時：同一份 Snapshot 該玩家的 `life_state_tick` ≤ `last_damage_tick` ≤ Snapshot tick，且 `last_attacker_id` ≠ 該玩家的 `player_id`。
  3. `damage_count` ≤ Ready／Welcome 的 `maximum_hp`。
  4. Dead 時三欄非 0，且 `last_damage_tick`＝`life_state_tick`（v6 只有命中會致死）。

  違反時整份 Snapshot 無效，沿用 v5 既有路徑：Gateway 視為 runtime 故障；Client 丟棄該 Snapshot，不更新序號與活性。

### arena 內容 digest（第 10 項）

- 目的：發現 id／version 相同、內容不同的 arena（例如只改了一方的 `pvp_arena.json`），避免 Client 預測與 Match 權威悄悄分歧。這是防意外不一致的完整性檢查，不是安全機制。
- 輸入：`Arena::Load`（含 `Validate`）成功後的值，不用檔案位元組。換行、空白、鍵順序、數字寫法（`20`／`20.0`／`2e1`）、省略選填鍵或明寫其預設值、未知鍵，都只透過解析後的值影響結果。
- 正規化位元組（整數與浮點都是 big-endian，與 GYOP 標頭相同）：

```text
u32 version
u32 id 的位元組數，接 id 的 UTF-8 位元組
f32 width depth cell_size movement_speed body_height radius eye_height
u32 牆數，接每面牆 f32 min.x min.y min.z max.x max.y max.z
u32 出生點數，接每個出生點 f32 position.x position.y position.z yaw
f32 jump_height gravity
```

- 順序就是 `Arena` 成員的宣告順序。牆與出生點保持內容順序，不排序：出生點順序決定同距離的選擇（v5 §5）。
- 正規化函式以 13 個名稱的 structured binding 取出 `Arena` 的全部成員；成員數或順序改變時編譯失敗，必須同時更新正規化（屬 §1 的 wire 變更）。
- f32 是解析後 binary32 值的位元樣式，不做正規化：−0 與 +0 視為不同內容（寧可誤拒，不可誤收）。JSON 的 `-0` 解析為整數 0（+0），只有 `-0.0` 得到 −0；L1 的數字寫法不變性不使用 0，−0 的敏感性以 `-0.0` 驗證。NaN 與 Inf 已被 `Validate` 拒絕。id 是 JSON 跳脫解碼後的 UTF-8 位元組，不做 Unicode 正規化。
- 演算法：Engine `Fnv1a64`（FF-3）對整個位元組串計算一次，64 bit。0 保留為「缺少」：結果為 0 的 arena 視為載入失敗，Match 不啟動，Client 啟動失敗（與 `Arena::Load` 失敗相同）；`SetArenaIdentity` 拒絕 digest 0。
- Owner：
  - 正規化與計算是產品 `Arena` 的一個函式，Match 與 Client 共用同一份實作。
  - Match 以模擬使用的同一份 Arena 計算一次，在 Ready 發布；這是唯一的權威值。計算與發布在 `match_main`／`IpcHost`，不改 `PvpMatch`、`MatchRuntimeHost`、`LocalPlayerPrediction` 的公開介面（第 03 批的 digest 程式依賴它們）。
  - Gateway 不讀 arena、不計算、不比對：在 readiness 檢查中要求 digest 非 0（與 `arena_id`、`arena_version` 同一處），原值轉入 HTTP join 回覆、房間狀態 JSON 與 Welcome。Welcome 的 1200 bytes 界限一律以非 0 的 digest 計算（任何非 0 的 fixed64 都是 9 bytes），與 PlayerId／MatchId 取最大值的做法相同；邊界測試的 Ready 帶非 0 digest。
  - Client 以本機載入的 arena 計算預期值，與 id、version 一起設定。
- Client 比對：HTTP join 回覆與每一個 Welcome 都比對，先比 id／version，再比 digest。

| 錯誤碼 | 條件 |
|---|---|
| `arena_identity_mismatch` | `arena_id` 或 `arena_version` 不同 |
| `arena_content_mismatch` | id、version 相同，`arena_digest` 缺少、不是無號整數、為 0，或與本機不同 |

- 錯誤碼只在 Client 本機產生，不上 wire。連線失敗原因（`ClientConnectionState.error` 與日誌的 `reason=`）以錯誤碼開頭，例如 `arena_content_mismatch: ...`，並列出 arena id 與雙方 digest（16 位十六進位）。
- 拒絕時照 v5 關閉連線回到 Lobby；已取得的保留由既有離開流程釋放。不重試，也不改用 Match 的內容。
- 照 v5，沒有設定 arena 身分的 Client（只有部分驗收 probe）不比對；正式 Client 一律設定。

## 3. 跳躍、命令與重播

沿用 v5 §3，不變。受擊欄位不進入預測、重播與校正；arena 不符的 Client 回到 Lobby，不以不同內容預測（§2）。

## 4. 動作裁決與彈匣

- 沿用 v5 §4 的裁決、彈匣、冷卻（權威 10 Tick）與拒絕語意。
- 合法 Shot 命中玩家時，Match 另外寫入受害者的受擊欄位（§2）；拒絕或重送的 Shot 不寫入。裁決結果仍只送給射擊者，受害者與其他玩家從 Snapshot 得知受擊。
- 射擊命中判定的 Collision 數值語意在第 10 批改變一次（使用者決定 D3：全部公開查詢統一為一套 double 實作與單一容差，並公開合法性檢查）。
  - 第 10 批的變更（2026-10-07）：結果會變的只有 `RaycastAabb`，以及 `VerticalCapsule` 版的 `RaycastCapsule`、`SweepSphereAgainstCapsule`（改為 `Math::Capsule` 版的薄包裝）。
    - 射擊判定的語意：`RaycastAabb` 只有方向分量恰為 0 才算平行，近平行射線不再視為平行；玩家膠囊改用 double，擦邊命中可能翻轉，距離有 ULP 等級的變化。容差維持 `1e-7`，移動與 Client 預測不變。
    - 第 03 批的 35 個權威 digest 情境全部不變（同機兩樹 0 不同；golden 不重新產生，`authority_golden.txt` SHA-256 `f01f3a32…`）：這些語意變化在現有情境中觀測不到。
    - arena 的合法性與契約不變：牆改用 Engine 的 `IsValid` 加上產品的「非空」規則；身體改用 `IsValid(VerticalCapsule)`，`body_height == 2·radius` 維持合法。
  - arena digest 只涵蓋解析後的內容值，Collision 實作與 `Validate` 合法性規則的改變（第 10 批）不影響它，也不是 wire 變更；新增或改變 `Arena` 的成員（含型別與順序），或改變正規化／演算法，屬 §1 的 wire 變更。
- 本機冷卻閘（Client 自己的閘門）：以相位追蹤估計的權威 Tick 補償約 2 Tick 的落差，Tick 閘與牆鐘閘保持一致；乾淨跑次的權威冷卻拒絕必須為 0（D11⑤）。
  - 補償公式與安全邊際：待定，由第 12 批決定。

## 5. 死亡／重生與 Tick 順序

沿用 v5 §5 的 Tick 順序；受擊欄位只在裁決步驟寫入，在重生時歸零：

```text
Authority Tick
  生命週期交接與ACK
    -> 到期重生／換彈完成（重生時受擊欄位隨 CombatState 歸零）
    -> 全部玩家固定移動（Dead只保留中立垂直物理）
    -> 按既有 acceptedTick、PlayerId、ActionId 逐一裁決動作
       （命中時扣血並寫入受害者的受擊欄位；每次致命傷害後立即轉Dead，供下一個動作判斷）
    -> 完整Snapshot（含本Tick的全部受擊）
```

- 死亡 Tick 的 Snapshot：hp＝0、Dead、`last_damage_tick`＝`life_state_tick`，`last_attacker_id` 是擊殺者。
- Dead 期間不是命中目標，受擊欄位保持死亡時的值，直到成功重生。
- 重生在裁決之前，所以重生 Tick 的命中屬於新生命；`last_damage_tick` 可以等於新生命的 `life_state_tick`。

## 6. 人物呈現與內容邊界

- 沿用 v5 §6：呈現是同區間、同生命權威資料的純函數，不重播、不補播。
- 遠端上半身依時間線內插後的 pitch，在 Pistol_Aim_Up／Neutral／Down 之間混合，作為持槍基底，再疊上射擊與換彈（第 11 批）。
  - 映射範圍、射擊與換彈時是否保留俯仰：待定，第 11 批開始時提案、使用者確認（D11⑥）。
- 受擊反應（第 13 批）：
  - 權威依據只有 §2 的受擊欄位：以（player_id、life_generation）為鍵，`damage_count` 相對同一生命上一次觀察值的增加就是新的受擊（增加量＝次數；新生命的基準為 0，所以重生 Tick 的命中或合併、遺失的 Snapshot 也不會漏掉），時點取 `last_damage_tick`，攻擊者取 `last_attacker_id`。不由 HP 下降推導；攻擊者可能不在同一份 Snapshot，呈現必須容許。
  - 遠端使用 Hit_Chest；Hit_Head 沒有權威依據，不使用。
  - 第一人稱有閃紅與 HUD，並依攻擊者位置顯示受擊方向。
  - 晃動只作呈現，不改變送出的 yaw／pitch 或射線。
  - 優先順序：死亡＞受擊＞射擊／換彈＞俯仰基底。細節待定，第 13 批開始時提案；需要 §2 以外的 wire 資料時，依 §1 的候選期規則處理。
- 死亡後的第一人稱武器（第 6 項，第 05 批）：自己死亡時不得呈現第一人稱持槍手臂。

## 7. 驗收、分母與 Architecture Delta

- 沿用 v5 §7 的全部門檻與分母；對玩家的門檻跨平台相同，不因平台放寬。
- 人物短測的達成 FPS 未達名義×0.85 時判 invalid：不計次，報告列出達成 FPS 與計時器分布，同一跑次的延遲門檻照判；有效輪不足時該項標「未驗證」，不算通過（D11③）。
- 權威不變的證明：第 03 批的同機兩樹 digest 比對。digest 程式（第 1 版與欄位清單）不修改；受擊欄位不進入此比對，由下列 Match 測試鎖住。只有第 10 批可以更新權威 golden；第 10 批確認不變，沒有更新。
- 量測：第 09 批的 before／after 都在本批 base commit 與 branch 上以凍結工具量；分析器除版本常數外逐位元組相同。B0＝第 04 批、B1＝第 07 批只作歷史參照。受擊欄位與 arena digest 只由 L1 驗證，本批不加入證據分析。
- 實機驗收只在 macOS Intel／Metal；Windows、Linux 實機與 macOS arm64 實機標「未執行」（D11⑩）。

第 09 批 L1（自動）：

- 版本：v6 wire golden 往返，標頭版本為 6；HTTP join、UDP 標頭、IPC envelope 三處拒絕 1～5 與 7（拒絕集合由常數導出）；`worker_main` 測「拒絕 5」；跨語言一致性測試（§1）。
- grep：版本字面 5 與 v5 名稱只留在下列事前宣告的類別，逐列理由寫進 dev_log：
  - （a）v5 指遊戲版本（D20），描述 v5 引入、v6 沿用之玩法語意者：測試名稱（`TEST_CASE("v5 ...")`、`TestV5...`、`adapter/v5_test.go` 及其中的訊息），驗收 probe／runner／GUI probe 的說明、docstring、註解與訊息，以及分析器的訊息。
  - （b）證據欄位 `gameplay_v5` 與旗標 `--gameplay-v5`：它是證據種類標記，不是協議版本；改名會違反上面的分析器規則。
  - （c）引用 v5 契約條款、而該條款由 v6 沿用的註解（例：`Movement.hpp:66`）。
  - （d）歷史文件與證據（v5 契約、`plans/v5/`、`docs/dev_logs/*`、基線依賴邊清單）。
- 指現行 wire 版本的名稱、註解與訊息（例：`Combat.hpp:33`、`worker_main.cpp:2,42,185,378`、`network_main.cpp:117-122,151,157`、`action_probe.py:283`、`server_test.go:770,773`）一律改為不含版本號或讀常數；這是本批的修改，不是允許殘留。
- 以同一組樣式的 6 版（`protocol_version\(6\)`、`!=\s*6`、`"protocol(_version)?",\s*6`、`\\x08\\x06`）確認字面 6 只出現在 §1 表列的四個定義與 wire golden 中。
- 受擊欄位：
  - Match 測試：未命中、打牆與拒絕不寫入；命中寫入計數、Tick 與攻擊者；同 Tick 互射；致命命中的死亡 Tick 與擊殺者；同 Tick 死者射擊不寫入；重送與 Reload 不寫入；射擊者自己的三欄不變；重生歸零；舊生命不殘留；hp＝maximum_hp−min(maximum_hp, damage_count×shot_damage)。
  - Go adapter 與 Client：§2 的四條驗證各有正反例。
  - 轉換：IpcHost、Go adapter、Client 解碼都保留三欄原值（以非 0 且各不相同的值往返）。
- arena：
  - 合成 arena fixture 的 golden digest，CI 四平台相同。
  - 不變性：CRLF／LF、空白、鍵順序、數字寫法、省略或明寫預設值。
  - 敏感性：每個成員、牆與出生點的順序、−0。
  - Gateway 拒絕 digest 為 0 的 Ready，並原值轉送。
  - id／version 相同而內容不同時，HTTP join 與 Welcome 兩條路徑都以 `arena_content_mismatch` 拒絕（Welcome 路徑由假 Gateway 讓兩者帶不同 digest）。
- 大小：最大玩家數 2，所有欄位取契約允許的最大編碼時，完整 Snapshot UDP 為 545 bytes（v5 為 487）；既有的最大 datagram 測試（worker probe，部分欄位取型別最大值）實測 587 bytes。Welcome 最壞為 213 bytes（arena_id 64 bytes）。都 ≤1200；既有的最大 datagram 測試加入新欄位。

Architecture Delta（第 09 批，依 AGENTS §3）：

1. 需求：交接第 2 項（受擊時點與攻擊者，D2）、第 10 項（arena 只比 id／version），以及 pv6 升版（D11①、D20）。
2. 問題：Snapshot 只有 hp，呈現無法以同區間權威資料的純函數決定受擊，也得不到攻擊者（v5 §6）；id／version 相同但內容不同的 arena，會讓預測與權威悄悄分歧；版本值散在多處字面值，漏改一處就會靜默拒絕或混版。
3. 邊界：產品 Client／Runtime 的 Data Contract 升 pv6（`CombatState` 三欄；Ready、Welcome、HTTP join 與房間狀態的 `arena_digest`；版本 6）；arena 內容的正規化成為產品內容契約的一部分。沒有新 Top-level domain、公共層概念或 Engine 變更。
4. 影響：只有 `object_fps_pvp` 的 Client、Match、IpcHost、產品 Go adapter 與 Gateway、專用驗收 probe（分析器只改版本常數）與產品測試。Engine、`services/gyo_gateway` 與共通 workflow 不改。
5. 依賴方向不變：產品→Engine。digest 用 `GYO::Base` 的 `Fnv1a64`（FF-3），domain 經既有的 `GYO::Math` 連結取得；GYOP framing 是 FF-8 已有的 `GYO::Net`。新增同一 owner 內的檔案依賴：`tests/object_fps_pvp` 的一致性測試讀取 `build/acceptance/object_fps_pvp` 與 `gateway/adapter` 的版本定義。Gateway 不取得 arena 內容的知識，只轉送不透明的 digest。
6. Ownership：受擊欄位由 Match 產生；arena 正規化與 digest 函式屬產品 `Arena`，Match 發布、Client 驗證、Gateway 只轉送；版本常數由產品擁有，Engine framing 只接收；驗收端保留自己的預期值。
7. 更小的變更不可行：不升版就無法拒絕舊端；以 HP 下降推導受擊違反純函數原則，且得不到攻擊者；只比 id／version 無法發現內容不同；雜湊檔案位元組會受換行轉換影響。

Architecture Delta（第 10 批，依 AGENTS §3；Engine 端的七點在 FF-9 計畫）：

1. 需求：交接第 10 項（Collision float／double 兩套演算法並存）與決定 D3。
2. 問題：同形狀的查詢有兩套精度與容差；本產品的 arena 合法性規則（`>=`）與 Engine 的前置條件（`>`）各自維護、不一致。
3. 邊界：本產品權威判定的數值語意（§4）；arena 合法性改由 Engine 公開檢查提供，產品只保留自有的「非空牆」規則。wire、arena 格式版本與內容契約都不變。
4. 影響：Match 的射擊判定（語意變化；現有 35 個情境觀測不到）、`Arena::Validate`、`QueryShot`、`tests/object_fps_pvp`。移動與 Client 預測不變。
5. 依賴方向不變：產品→Engine Collision 是既有方向，沒有新邊。
6. Ownership：幾何合法性歸 Engine Collision；arena 內容規則（數量、出生點、非空牆）仍歸本產品。
7. 更小的變更不可行：本產品自行保留一份合法性判定，會延續兩份規則的不一致。

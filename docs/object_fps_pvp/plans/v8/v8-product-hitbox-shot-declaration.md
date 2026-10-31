# v8-product-hitbox-shot 的事前宣告

更新：2026-11-01。Owner：`object_fps_pvp`。**狀態：等你決定**（下面「等你決定」的 5 項）。契約：[protocol-v9](../../protocol-v9.zh-Hant.md) §2、§3、§7、§8（定稿）。計畫：[hitbox-plan](hitbox-plan.md) 第 4 節 R1、R4、R5，第 8 節。形狀：[hitbox-shapes](../../hitbox-shapes.zh-Hant.md)。估時 10～14 h（和計畫相同；超過 21 h 停下回報）。

## 這批做什麼

Match 開始**依部位判定命中**：

- 用 Tick J 的權威狀態算出每個目標的骨架姿勢（D2 的共用函式），把 D1 的 7 個形狀放到世界座標，拿射線去打。
- 打中頭 ×2000‰、軀幹 ×1000‰、腿 ×750‰（傷害公式在 permille 批已完成）。手臂沒有形狀，射線會穿過去。
- 這批做完，pv9 照 `pv9-single-merge` 開成一個 PR，四平台 L1 一起看（含 D3 的 H6）。

不在這批：
- 回溯（Tick J 之前的姿勢）：rewind-line。
- Client 依部位播受擊動畫、驗收工具依部位推算傷害：`v8-product-hit-presentation`。
- 形狀大小是否合適：`v8-product-hitbox-acceptance` 的 L2 試玩。

## 現在的程式（開工前讀過的事實）

- 判定：`ShotQuery.cpp` 用一顆垂直膠囊（站或蹲的高度，半徑 `arena.radius`）；`PvpMatch.cpp:375`、`:381` 寫死 `HitPart::Body`。
- Tick 的順序（`PvpMatch::Tick`）：生命邊界 → 移動 → 射擊與換彈（`ResolveActions`）→ 最後才推進每位玩家的動畫（A2）。所以**判定時，動畫狀態還停在 J−1**。
- 姿勢的輸入：`ComposeBody` 讀的是相位、兩個權重、跳躍階段與零點、idle 時間、蹲姿（`PlayerPose.cpp:447-495`）。這些都只取決於 Tick J 的 frame；會帶歷史的只有 holding，而 holding 只在 Client 發生。所以 Match 不必保留呈現的歷史。
- Client 把 snapshot 換成 frame 的程式，寫在 `PvpApplication.cpp:804-836` 的迴圈裡，沒有單元測試（A4 的 G1）。身體到世界座標的換算，也寫在 Client 端：`Transform3D{position, {0, yaw, 0}, scale}` 與 `-anchor`（`PlayerPresentation.cpp:223`、`:263-268`）。
- Engine 已有任意方向膠囊的射線相交：`Engine::Collision::RaycastCapsule(Ray, 距離, Math::Capsule)`，起點在形狀內時回 0。球就是兩端相同的膠囊。**不需要改 Engine。**
- `match_domain` 不連結 `GYO::Model`；`player_content` 依賴 `match_domain`。所以 `PvpMatch` 不能直接呼叫 `SamplePlayerBodyPose`，否則會循環依賴。
- CI 裡用真的 Match 開槍的驗收：`network_main.cpp:190`、`action_main.cpp:185-189`。兩者都用 pitch 0，也就是從眼睛高度水平射出，**改成形狀之後很可能打中頭**，傷害變兩倍。見「會改判定的既有測試」。

## 分成三個子批

```text
v8-product-hitbox-shot
├── S1 v8-product-hitbox-shot-frame   「high｜snapshot→frame、身體→世界座標抽成共用函式，Client 行為不變」
├── S2 v8-product-hitbox-shot-judge   「high（同距與同 Tick 的順序交 xhigh 審查）｜判定模型的介面、合成模型、姿勢模型、依部位判定」
└── S3 v8-product-hitbox-shot-wire    「high｜Match 接上產品的模型、IPC 往返、CI 驗收、文件」
```

| 代號 | 估時 | 依賴 | 檢收點 |
|---|---|---|---|
| S1 | 2～3 h | 本宣告 | J1 |
| S2 | 5～7 h | S1 | J2～J5、J8 |
| S3 | 3～4 h | S2 | J6、J7 |

pv9 的 PR 在 S3 之後開。

## S1：Client 的兩段換算抽成共用函式（決定 `v8-product-hitbox-frame-sharing`）

放在 `player_content`，Client 與 Match 都呼叫：

- `PlayerFrameFromAuthority(PlayerState, CombatState?, PlayerAnimationSample?, presentationTick)`：
  - 照 `PvpApplication.cpp:804-836` 原樣搬出：位置、yaw、pitch、命、死亡、蹲姿、grounded、`lifeStateSeconds`，以及射擊、換彈、受擊的時刻。
  - Client 專用的欄位（`deltaSeconds`、`continuous`、`holding`）照舊由 `PvpApplication` 填。
- `PlayerBodyToWorld(definition, position, yaw)`：Client 畫面用的那個矩陣，`ComposeEulerXYZ(position, {0, yaw, 0}, {s, s, s})` 乘上 `-anchor`。

這一步**不改任何行為**，也順便讓 G1（取樣放進 frame 的那一行）有了單元測試。

### J1：Client 不變

- B3 的 H-pose 仍是 `dc7544253139d07e`（這台 Intel Mac）；呈現測試全部通過。
- 新的單元測試：snapshot 的每個欄位都進到 frame 的對應欄位（各改一個值，frame 只有那一欄變）。

## S2：依部位判定

### 介面（決定 `v8-product-hitbox-shot-boundary`）

- `match_domain` 定義一個小介面 `PlayerHitModel`：給它目標在 Tick J 的權威值（`PlayerState`、`CombatState`、`PlayerAnimation`、J），回傳世界座標的形狀（部位、兩端點、半徑）。
- 兩個實作：
  - **合成模型**（`match_domain`，預設）：一顆垂直膠囊，部位是 Body，高度與半徑和現在相同。也就是**現行的判定**，給單元測試與權威 digest 用。這是 A2 合成動畫規則（`SyntheticPlayerAnimationRules`）的先例。
  - **姿勢模型**（`player_content`）：持有 D2 的姿勢定義與 D1 的形狀。依序呼叫 `PlayerFrameFromAuthority` → `AdvancePlayerLocomotion`（新的 state，不帶歷史）→ `ResolvePlayerActions` → `SamplePlayerBodyPose` → `PlayerBodyToWorld`，再把形狀放到世界座標。
- `PvpMatch` 與 `MatchRuntimeHost` 多一個建構參數，預設是合成模型；Match 執行檔傳姿勢模型（S3）。
- 依賴方向不變：`player_content` → `match_domain`。新增的只是介面，沒有新的依賴邊。

### Tick J 用哪個動畫狀態

- 射擊時，Match 的動畫狀態還停在 J−1（見「現在的程式」）。判定時另外算一份**暫時的** J：用和 Tick 結尾相同的 `AdvancePlayerAnimation(規則, 上一筆, 上一個狀態, 這個 Tick 的狀態, J)`，但不寫回。
- 動畫狀態只取決於移動與生命，不讀戰鬥，所以活著的目標在 Tick 結尾寫進 snapshot J 的值會和它相同（J8 驗證）。
- 好處：不必調動 Tick 的順序，權威 digest 與 golden 都不變。

### 同一個 Tick 的多發子彈（決定 `v8-product-hitbox-tick-pose`，protocol-v9 §8 的未決項）

- 在這個 Tick 第一次需要判定時，算一次所有活著的目標的形狀，之後整個 Tick 都用它（Valorant「需要時才算」的作法）。
- 前一發打死的目標照舊從候選中拿掉（現行規則）。
- 成本：一個 Tick 最多 3 個目標的姿勢。B1 在 linux-x64 量到 12 次取樣 0.44 ms，3 次約 0.11 ms，不加新的門檻，只記錄。

### 同距的規則（決定 `v8-product-hitbox-tie`，交 xhigh 審查）

- 牆與玩家同距：牆（現行）。
- 兩個玩家同距：PlayerId 小的（現行）。
- **同一個玩家的兩個形狀同距**（關節處的形狀重疊、或眼睛在形狀裡的距離 0）：新的規則，見決定。

### J2：部位對了

用產品的內容，在站姿與蹲姿、yaw 0 與 yaw 1.3、idle 與跑步中的目標上：

- 瞄準頭球的中心 → Head，傷害 50000；
- 瞄準 `spine_02` 的位置 → Body，25000；
- 瞄準大腿的中點 → Leg，18750；
- 瞄準只有手臂的位置（`hand_r`，後面沒有東西）→ 不是 Player；
- 兩腿之間、膝蓋的高度 → 穿過。
- 瞄準點都由**判定用的同一個姿勢**算出，不是手寫的座標。

### J3：順序與同距

- 一條射線先後穿過腿和軀幹 → 取近的。
- 上面三條既有的同距規則照舊；同一個玩家兩個形狀同距時照決定的規則。
- 這部分交 xhigh 審查（計畫的「平手局部 xhigh」）。

### J4：同一個 Tick 用同一組形狀

- 兩位射手在同一個 Tick 打同一個目標：兩發看到的形狀相同。
- 第一發打死目標時，第二發不會再打中它（現行規則）。

### J5：合成模型＝現行的判定

- `compare_authority_trees.py`：base＝`eb28d26f`（D3），branch＝本批，同一台 Intel Mac，scale 1、3 的所有情境 **0 差分**。golden 不重產。
- 這證明預設的路徑沒有改變；改變只在姿勢模型。

### J8：暫時的動畫狀態＝snapshot J 的動畫狀態

- 在多個射擊情境（含起跳、落地、蹲下、跑步中被打）：每個有判定的 Tick，判定用的暫時值和 Tick 結尾寫進 snapshot 的值完全相同（每個活著的目標）。

## S3：接上 Match、CI 與文件

- `match_main.cpp`：D2 已經在啟動時載入姿勢定義（算 digest 用），D1 已經載入形狀；把兩者做成姿勢模型交給 `MatchRuntimeHost`。log 多一行模型的形狀數。
- 文件：
  - protocol-v9 §8 的未決項，寫入決定；
  - hitbox-shapes 的「誰讀它」改成實際的判定；
  - `ShotQuery.hpp` 的註解（膠囊 → 形狀）。

### J6：真的 Match 依部位判定，wire 帶得到

- IPC 測試：`MatchRuntimeHost` 用姿勢模型，瞄頭的一發 → runtime 的 `ShotDecision.hit_part`＝HEAD、傷害 50000、`last_hit_part`＝HEAD；瞄腿 → LEG、18750。
- 這是 LEG 第一次由真的判定產生，經 IPC 送出。Gateway 與 Client 的往返在 leg-wire 已經測過。

### J7：CI 的真 Match 驗收照常通過

- `network_main`、`action_main` 等用真的 Match 開槍的驗收，在新的判定下通過。
- 要改的地方見下表；改法以「瞄準點改成軀幹，保留原本的斷言」為先，因為它們要驗的是生命週期與動作傳送，不是部位。

## 突變（各子批）

只登記和本批有關的，只在預期失敗時才算 killed。D184 相關的既有突變全部要 killed（已知：`v71hf-match-head`、`v71hf-match-last-hit-part-unset`、`legwire-*` 的 find 會改到）。

- S1：frame 換算各漏一個欄位（射擊、換彈、受擊、蹲姿、生命各一個）；世界座標漏了 `-anchor`。
- S2：
  - 判定忽略形狀（固定回 Body）；部位對調（頭當腿）；
  - 同一個玩家多個形狀時取遠的；同距規則反過來；
  - 用 J−1 的動畫狀態（不算暫時的 J）；
  - 每發都重算（違反同一個 Tick 一組形狀）；第一發打死後仍可再被打中。
- S3：Match 沒有把姿勢模型交出去（仍用合成模型）。

## 會改判定的既有測試

| # | 測試 | 改法 | 寬鬆面 |
|---|---|---|---|
| W1 | `network_main.cpp` 的 life-cleanup 那一發（`:190-194`） | 瞄準點改成軀幹（pitch 往下），保留 `damage`、目標、命的斷言 | 無（斷言不變） |
| W2 | `action_main.cpp` 的射擊（`:185-189`） | 同上；如果它只看動作的傳送、不看傷害，就只改瞄準 | 無 |
| W3 | `PvpMatch`、`MatchRuntimeHost` 的建構多一個參數 | 用預設參數，既有的呼叫不必改，判定不變 | 無 |
| W4 | 既有突變的 find（`HitPart::Body` 那兩行） | find 跟著改，抓的東西不變 | 無 |

開工後如果發現本表以外的測試要改判定，停下回報。用真 Match 的 GUI 驗收工具（`combat_gui_evidence.py` 等，假設每發傷害固定）不在 CI，由 `v8-product-hit-presentation` 改。

## 等你決定

### `v8-product-hitbox-frame-sharing`：Match 怎麼得到和 Client 相同的 frame 與世界座標

- (a) 把 Client 的兩段換算（snapshot→frame、身體→世界）原樣抽成 `player_content` 的函式，兩端都呼叫（建議）。
  - 理由：D2 已經讓姿勢是同一個函式，剩下這兩段也同一個，才能說「畫面上的姿勢＝被判定的姿勢」。順便補上 G1 的測試。
- (b) Match 自己寫一份換算，用測試和 Client 的對照。
  - Client 不必動；但兩份程式會漂開，而且 Client 那份仍然沒有單元測試。

### `v8-product-hitbox-shot-boundary`：判定模型放哪

- (a) `match_domain` 定義小介面 `PlayerHitModel`，姿勢模型在 `player_content` 實作（建議）。
  - 理由：依賴方向不變（`player_content` → `match_domain`），不必搬 `PvpMatch`。
- (b) 把 `PvpMatch` 搬到一個連結 `player_content` 的新程式庫。
  - 沒有介面，但要搬 Match 的核心，`runtime_host`、`ipc` 和測試的連結都要改，範圍大很多。

### `v8-product-hitbox-test-model`：單元測試與權威 digest 用什麼判定

- (a) 合成模型：一顆垂直膠囊、部位 Body，等於現行的判定，作為預設（建議）。
  - 理由：80 處既有測試與 golden 都不變（J5 證明）；動畫規則已經有同樣的先例。
  - 代價：預設路徑不是產品的判定。產品路徑由 J2～J4、J6 守住，Match 有沒有接上由 J6 與 S3 的突變守住。
- (b) 全部改用產品的內容。
  - 測試更接近產品，但取樣跨平台不一致（B3），含命中的 golden 情境都要移出四平台的子集，既有測試的期望值也要逐一重推。

### `v8-product-hitbox-tick-pose`：同一個 Tick 的多發子彈（protocol-v9 §8）

- (a) 第一次判定時算一次，整個 Tick 共用（建議）。
  - 理由：結果不受同一個 Tick 內的處理順序影響；成本最多 3 次取樣。
  - 代價：snapshot J 會顯示這個 Tick 剛開始的射擊或受擊動作，判定的姿勢沒有。動作在第 0 秒的姿勢是否和不播相同，S2 開工時逐一確認（射擊、換彈、受擊），不同就寫進 dev_log 的限界。
- (b) 每次命中後重算被打的目標。
  - 更貼近 snapshot J，但結果取決於同一個 Tick 內的處理順序，成本也多。

### `v8-product-hitbox-tie`：同一個玩家的兩個形狀同距時算哪個部位

- (a) 倍率高的優先：頭 > 軀幹 > 腿（建議）。
  - 理由：偏袒射手，和成熟遊戲的取向一致（hitbox-plan §2 的 Overwatch，△）；規則不依賴 JSON 的順序，換形狀檔也不會改變結果。
- (b) 照 `hitbox.json` 裡的順序。
  - 規則簡單，但結果會隨內容的排列改變。

## 範圍的限界

- 判定用 Tick J 的現在姿勢，不回溯；射手看到的是比 J 早的畫面（內插延遲），這段落差由 rewind-line 處理。
- J2～J4 只在同一個平台上成立；跨平台的姿勢本來就不同（B3，`v8-product-hitbox-golden-policy`）。
- 合成模型的路徑不是產品的判定；只靠 J6 與突變確認 Match 有接上姿勢模型。
- GPU 實際畫出來的位置不比較：J1 比的是 Client 端算出的矩陣，GPU 的浮點運算不在範圍內。

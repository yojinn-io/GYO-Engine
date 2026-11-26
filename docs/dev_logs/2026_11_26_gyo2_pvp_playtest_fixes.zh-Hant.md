# 2026-11-26 LAN 實機場次的 8 項回饋（`v8-product-pvp-playtest-fixes`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡；決定與原話在 `docs/architecture/target/v2/HANDOFF.md` 的 `v8-arch-gyo2-lan-playtest-2026-11-26`，證據在主 checkout 的 `build/evidence/gyo2-lan-session-20261126/`（`REPORT.ja.md`）。作者 Opus 5.5／high（上段：arena／角色／sfx 的資料格式）。一條 PR 線，三段：A 聲音（1–3）、B 移動規則（4、5、7）、C HUD／流程（6、8）。

## A 聲音

### 2 蹲下不出腳步聲；別人的腳步聲

- 原狀（gyo2 `280c27a6`）：`PlaySounds` 只看自己的身體（`FootstepTracker` 一個），不看蹲姿。
- `FootstepTracker::Observe` 多吃 `crouched`：蹲著走不出聲，蹲著走的距離不算進下一步（站起來後重新數一步）；蹲著從台子掉下去照樣有落地聲。
- `ClientSound::steps` 改成每個玩家一個 tracker：自己的照舊置中；別人的照 `HeardGains`（空間化＋距離＋遮擋，見 3）在對方身體中段發聲。看不到的玩家（離開、不再複製）的 tracker 與開槍紀錄一起清掉，回來時重新開始、不補播。
- 對方的位置用 Frame World 的 `PlayerBody`（內插後顯示的位置），腳步跟畫面一致。

### 3 遮擋與最大可聽距離

- `audio/sfx.json` 升到 **version 4**：每個 cue 可帶 `maximum_distance`（沒有＝不限距離）、`spatial.occluded_gain`（必填，0～1）。只接受 version 4（照這個檔原來的規則：版本不同就拒絕）。
- 預設值：`remote_shot` 200 m（最大的地圖 250 m 見方，對角約 350 m：地圖另一頭的槍聲聽不到，半張地圖內聽得到）；`footstep` 30 m、`landing` 40 m（大約一條街；落地比腳步重，遠一點）；`occluded_gain` 0.25（約 −12 dB：牆後還聽得出方向，但明顯比沒擋時小）。都是依地圖尺度定的起點，未經實機調整。
- 遮擋：`world::Blocked(arena, a, b)`：從聽者的眼睛到聲源（對方身體中段）的線段被 arena 的牆（`InstalledArena::walls` 的 BoxSet）切到，就算擋住；只碰到線段終點（聲源貼著牆）不算。用 physics_3d 現有的 `CastRay(Shape, Pose, Ray, maxT)`，沒有改 Engine。只乘增益，不做濾波（卡片的指定）。
- `HeardGains(cue, ...)`：超過該 cue 的最大距離回 nullopt（不播），否則 `SpatialGains`×（擋住時）`occluded_gain`。

### 測試與突變（A）

- 邏輯：「a crouched walk is silent…」「heard gains: …maximum distance…occluded gain」，sfx 的解析加三個壞值（沒有 occluded_gain、大於 1、maximum_distance 0）。
- World：「the arena stands between two points…」（出貨的 pvp_arena 的柱子：擋住、上方／旁邊／停在前面不擋、貼著面不擋）。
- Client：「the other player's steps sound while it walks standing, not while it walks crouched」（另一個 Client 蹲著走兩秒：Mixer 送出的聲音數不變；站起來走一秒：增加）。
- 突變 8 個，8/8 killed：`pvp-crouch-steps-sound`、`pvp-crouch-distance-kept`、`pvp-sound-no-range`、`pvp-sound-no-occlusion`、`pvp-blocked-ignores-walls`、`pvp-blocked-at-touch`、`pvp-remote-steps-silent`、`pvp-remote-crouch-sounds`。
- 沒有外部測試的接線：`PlaySounds` 把 `Blocked` 的結果交給 `HeardGains` 那一行（Mixer 只數送出幾個，量不到增益）；兩端各自有測試。

## B 移動規則

都在 sim 共用的 `MovePlayers`／`StepBody`（Match 與 Client 預測同一份；Client 照舊預測自己的 `PlayerBody` 與 `PlayerCombat`）。

### 4 地圖邊界＝隱形牆＋底部死亡線（使用者已決定）

- arena 檔升到 **version 4**（只接受 4）。新成員：`floor`（可省：地板平面的高度；省略＝沒有地板平面，靠地圖自己的地面）、`kill_height`（必填：腳低於它就死；要低於地板與每個重生點）、`fall_damage`（見 5）。
- **隱形牆**：腳的 x、z 夾在 `[radius, width − radius] × [radius, depth − radius]`（`InsideBounds`，MoveAndSlide 之後）：撞上去只停那一軸，另一軸照走（沿牆滑）。跟重生點的檢查是同一個範圍。
- **地板平面變成 arena 資料**：原本「y = 0 的地板」是寫死在 `StepBody`／`Fall`／判定射擊的規則，而且不分 arena 範圍內外。地圖本身有地面的 arena（kiriyu 三張）在 y = 0 也有一層看不到的地板，所以從邊緣走出去、或掉進地面板（約 y 9～11.5）底下，會站在 y = 0（玩家回報 3 的「我在地下」很可能就是這裡）。有這層地板，死亡線永遠碰不到；所以：
  - `pvp_arena`、`pvp_corners`、`biome` 留 `"floor": 0`：它們靠這層地板（前兩張沒有地板方塊；biome 約 1/3 的格子底下沒有方塊）。
  - `kiriyu_extraction_250m`、`kiriyu_incident_250m`、`kiriyu_town_graybox` 不設 floor：每一格都有方塊（extraction 的地下室有自己的 0～0.5 地板、town 的地下室到 −3）；town 的 −3 地下室原本被 y = 0 的地板擋在上面，現在走得到。incident 有 1 格（0.5 m）沒有任何方塊，是地圖本身的洞；身體半徑 0.25 m，可能掉得下去（掉下去就死）。
  - `kill_height`：地面最低處再往下 10 m（training 三張 −10、extraction／incident −10、town −13）。
- 死於死亡線：HP 歸 0，`Die`（和被打死同一個函式：`respawnTick`、丟掉裝填中），之後照舊由 `RespawnPlayers` 重生。死後身體照舊只受重力，3 秒內重生。

### 5 墜落傷害

- `fall_damage: {"safe_height": 4.0, "points_per_metre": 15}`（點數；內部是 milli-points，HP 100 = 100000）。落地速度 v 換成落差 v²/2g，超過安全高度的公尺數 × 每公尺點數（無條件捨去）。不另存「從哪裡開始掉」：落地那一步的下落速度就決定落差（`Fall` 停在地板時把速度記在 `VerticalStep::impact`，撞上方塊時用那一步的下落速度），兩端算出來一樣。
- 預設值的依據：跳躍 0.6 m 永遠不痛；town 的屋頂離地約 5～8 m，跳下來扣 15～60；10 m 扣約 90；約 10.7 m 以上致死（例：extraction 從地面掉進地下室約 11 m）。安全高度 4 m ≈ 一層樓多一點。這是依地圖尺度定的起點，未經實機調整；之後改 arena 檔就好。
- 落地傷害讓 HP 歸 0 也走 `Die`。墜落不是命中：不寫 `HitRecord`，沒有紅閃與命中音。

### 7 按住空白鍵連跳

- `rules/movement_rules.json` 升到 **version 2**，加 `hold_jump`（必填 bool；出貨 true）。放在角色的移動規則（所有 arena 共用），之後技能改的是這個值（`StanceRules::holdJump`）。
- 規則：`sim::StepJumps(pressed, held, rules) = pressed || (holdJump && held)`。Client 的 Controls 多帶「空白鍵按著」（`StepInput::jumpHeld`，跟 crouch 一樣取最新），Sim／Solo World 寫每一步的輸入時套這條規則，答案放進原本的 `PlayerInput::jump`（一步一次的按下）。**網路的 input 格式不變**；Match 照舊「著地時有 jump 就跳」，所以按住時一著地就再跳。
- 控制過期（stale：Frame World 停住）時，按住的跳和按下一樣丟掉（不會在畫面停住時一直跳）。

### 測試與突變（B）

- 邏輯：arena v4 的解析（floor／kill_height／fall_damage 的正反例、v3 帶 v4 成員也拒絕）、`Fall` 的有無地板與 impact、`LandingDamage`／`BelowKillHeight`、`StepJumps`、movement_rules v2（缺 `hold_jump`、非 bool、v1 拒絕）、Controls 帶 `jumpHeld`。
- World：「the arena's bounds are an invisible wall…」「a body that falls below the kill height dies…」「a landing from above the safe height costs HP…」（沒有牆、只有邊界的 arena：直走停在半徑、斜走沿牆滑；沒有地板：掉下去在死亡線下一步內死；4 m 內不扣、10 m 扣約 90 點（±8%，最後一步的速度略大）、30 m 死）。
- Client：「holding Space jumps again on every landing; letting go stops」（按住 2 秒起跳 ≥ 3 次、放開不再跳、點一下跳一次）；J5（主執行緒停住）加上按住空白鍵：停住前每步都要跳，過期後不跳。
- 突變 12 個新的＋改 1 個舊的（`pvp-arena-version`：改成「讀了 version 3」），13/13 killed：`pvp-bounds-open`、`pvp-bounds-at-edge`、`pvp-kill-height-ignored`、`pvp-floor-everywhere`、`pvp-landing-unhurt`、`pvp-landing-speed-lost`、`pvp-fall-no-death`、`pvp-fall-safe-ignored`、`pvp-hold-jump-ignored`、`pvp-hold-jump-always`、`pvp-hold-not-carried`、`pvp-hold-kept-stale`。第一次跑 `pvp-arena-version` 存活：v3 的測試文字缺 `kill_height`，本來就會被拒；測試改成帶 v4 成員的 v3 後 killed。

### 1 命中音（等決定）

使用者 11-26 回「我沒看明白」，主會話再說明後轉決定過來再做。

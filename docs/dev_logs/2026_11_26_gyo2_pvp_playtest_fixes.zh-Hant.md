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

### 1 命中音（等決定）

使用者 11-26 回「我沒看明白」，主會話再說明後轉決定過來再做。

# 2026-11-24 PvP 移植實作第 1 段：模擬與 Match（`v8-product-pvp-port`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（PvP 移植實作）。作者 Opus 5.5／high。
根據：移植盤點（`games/object_fps_pvp/docs/porting-inventory.zh-Hant.md`）、事前宣告（`games/object_fps_pvp/docs/port-declaration.zh-Hant.md`，本段寫成；判斷 PV8、PV6、PV3、PV14、PV15 等在宣告第 2 節）。
依賴：同卡的 Engine 修正（K3 Client 的 Peer 實體被回滾刪掉，dev_log `2026_11_24_gyo2_pvp_port_k3_peer`），本段疊在它上面。

## 做了什麼

1. **`logic` 加**（不連 World）：
   - `data/VoxelMeshFile`：遊戲自己讀 voxel 地圖編輯器交出的 `gyo.voxel_mesh` v1（PV8：Engine 不承擔工具職能；格式在 master 已 deprecated）——只讀要的欄位，照契約的換算得到世界座標的 AABB，quads 展開成三角形與 palette 圖（第 2 段畫地圖用）。
   - `sim/Arena`（arena 檔 v2、清單）、`sim/MovementRules`（蹲姿、身高眼高、跳的初速、解析式落下）、`sim/CombatRules`（HP、部位倍率、彈匣、冷卻、換彈、重生；秒換成 World 的步數、視線方向、射擊的平手規則、選 spawn）。這些是遊戲規則與內容（master 的數值是 60 Hz tick 數，換成秒）；權威、回滾、預測的做法與常數一律不取 master 的（使用者 2026-11-20）。
2. **`world`**（`world/PvpWorld`）：玩家的元件（`PlayerBody`、`PlayerCombat`、`ShotRecord`、`HitRecord`：權威登記的判定資料、複製；`Owner`：複製）、輸入 `PlayerInput`（判定的輸入、網路的輸入；跳、開槍、換彈是單次觸發）、固定步的 System（兩端：`MovePlayers` 經 `physics_3d` 的 `MoveAndSlide`、`UseWeapons`；只在 Match：`JudgeShots`、`RespawnPlayers`）、每幀的 System（Match：`SeatPlayers` 連上時生成、斷線時刪；Client：`ControlOwnPlayer`、`FollowMatchArena`）。Client 選回滾策略（K3）預測自己的玩家。滿房由 `admission` 拒絕（`Seats`）。arena 的牆是一個固定編號的實體（`Collider{BoxSet}`），在準備函式放進去（第一步之前）。
3. **Match 執行檔**（`gyo_object_fps_pvp`，暫時是產品的 main；第 2 段由 Client 接手）：無畫面、UDP（`--port`、`--arena`、`--ticks`、`--content`、`--verbose`），4 ms 一幀。
4. **內容**：`assets/object_fps_pvp/` 的 maps、rules，`content.json`、`asset_catalog.json`。

## 驗證

- 本機（Intel Mac）：`object_fps_pvp.logic` 33 案、`object_fps_pvp.world` 13 案（獨自的 Match：走、撞牆、沒輸入不動、跳、蹲下站不起來、命中與冷卻與擊殺與重生、柱子擋子彈、彈匣與換彈；Match＋Client：連上與控制、走路的預測與收斂、命中傳到兩個 Client、滿房被拒、離開時刪玩家還座位、跟 Match 的 arena 與不一致）；`object_fps_pvp.match`（network 標籤，只在本機）。全量 `ctest -L "cpu|gpu"` 243/243（審查前；審查後跑的見 PR）。
- 突變：`games/object_fps_pvp/docs/mutations.json` 11/11 killed。宣告原列的 `pvp-damage-on-client` 換成 `pvp-respawn-early`（等價突變，宣告第 4 節寫了理由）。
- 手動突變：Client 不選回滾策略時，「the Client predicts its own player」失敗（0.703 vs 0.75）。
- **沒驗**：實際兩個行程經 UDP 對戰（第 2 段的 Client 才有）。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

blocker 0、should 6、nit 4。報告在主 checkout 的 `build/evidence/v8-product-pvp-port/segment1-design-review.zh-Hant.md`。

- S6（契約沒從外部驗）→ 加三案（離開、arena、預測領先）。
- S5、N1、N4 → 宣告的文字（遠端玩家沒有輸入元件、停在確認值；突變的殺手；部位只到規則層）。
- S2 → Engine 修正那一份（改名）。
- 交使用者：S1（回滾的存活集合只收回滾範圍內的實體）、S3（「誰控制哪個實體」與斷線的 lifetime 進網路 Module，lightyear 的 `ControlledBy`）、S4（連線上限進網路 Module，lightyear netcode 的 `max_clients`）、S5（第 2 段試玩前：先做內插或回溯其一，還是接受提前量）。
- N3（Match 自己的 4 ms 迴圈、沒有 headless runner）不改。

## 已知限制

- 遠端玩家在 Client 上停在確認值（約晚一個來回），判定用 Match 當 tick 的位置（PV3 不做），要提前量。
- 命中只有身體的膠囊（`HitPart::Body`）；頭、腿在第 3 段骨頭上的 hitbox。
- `Seats` 在 admission 時加、在 `PeerDisconnected` 時減；同一個 endpoint 重連（新的 salt）時的先後沒有測。

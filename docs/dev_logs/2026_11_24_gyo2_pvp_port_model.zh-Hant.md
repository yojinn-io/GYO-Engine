# 2026-11-24 PvP 移植實作第 3 段：角色、骨骼動畫、viewmodel、骨頭上的 hitbox（`v8-product-pvp-port`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（PvP 移植實作）。作者 Opus 5.5／high。疊在第 2 段（#325）上。
根據：事前宣告第 1、2 節（第 3 段、動畫事件、PV2、PV6）、盤點第 4 節。

## 做了什麼

1. **`logic` 加 `sim/PlayerAnimation`**（動畫的第 2 層，遊戲規則，純函式）：步態（走過的週期數、走↔跑的比例、移動權重照 transition 緩入、後退倒數、瞬移不算）、跳躍的四個階段、動作的選擇（死亡蓋過一切；這一命的受擊＞換彈＞開槍）、瞄準（上、中、下）。`presentation.json` 的讀取。
2. **新 `character` 函式庫**（Match 與 Client 共用）：讀 Player_V20 與 UAL1（`FbxLoader`，不經 render；路徑與設定照 catalog 的列，動畫庫不讀網格）、依名字 retarget 16 個片段（`translation_scale`、忽略 `Mannequin`）、上半身遮罩、身高縮放（rest 姿勢蒙皮後的高度 → arena 的身高）、`hitbox.json` 的 7 個形狀、第三人稱武器的掛點。`ComposeBody`（下半身＋上半身逐節點取一半，上半身＝瞄準＋動作的 additive）、`PlaceHitShapes`、`WeaponToWorld`。
3. **Match**：每個玩家一個 `PlayerMotion`（不是判定資料、不複製），`AnimatePlayers`（只在 Match）每步推進；`JudgeShots` 在目標這一步的姿勢上擺骨頭的形狀判部位（PV2）。`PlayerCombat` 加 `lifeTick`（codec 版本 2）。
4. **Client**：對手畫成 Player_V20（每個網格一個 `Mesh3d`＋`SkinPose`，材質色照 sRGB）、手上的武器（Nailer2）掛在 `hand_r`；自己的第一人稱武器（Nailer1：Idle、Shoot、Reload、Draw）在第二個 3D 相機（FOV 55°、near 0.01、far 5、`RenderLayers{2}`，深度自己清），HUD 相機改 `order 2`；換彈聲在 `magazine_in`。PNG 與 FBX 的 loader 加進 Product。方塊替身刪掉。
5. **內容**：Player_V20、UAL1（23.8 MB，CC0）、Nailer1／Nailer2 與貼圖、角色與武器的 JSON；catalog 的 FBX 都設 `"textures": false`（FBX 寫的是 JPG，catalog 是 PNG；盤點 4.1 的 model 修訂 1），viewmodel 的貼圖照 `nailer_v2_viewmodel.json` 的材質對照。

## 驗證

- 全量 `ctest -L "cpu|gpu"` 245/245 通過；`object_fps_pvp.*` 5 項（含 network 的 `match`）通過。

- 本機（Intel Mac）：`object_fps_pvp.logic` 39 案（加動畫規則 3 案）、`object_fps_pvp.world` 17 案（加：骨頭上的頭、身、腿各自的傷害，頭上方打不到；角色的骨架、16 個片段、7 個形狀、身高；上半身跟瞄準、下半身跟步態）、`object_fps_pvp.client` 4 案（對手的每個網格在它的位置、世界與 viewmodel 兩個 3D pass；第一人稱武器在相機前）。
- 數值檢查：viewmodel 的 idle 頂點在相機空間 z 0.34～0.91、重心 (0.16, −0.19, 0.77)（前方、右下；已寫成測試）；角色 rest 高 0.554 → 縮放 3.25、面向 +Z（暫時的探測程式，未提交）。
- 設計審查（Opus／high，唯讀；作者同模型，Fable 暫停中）：blocker 0、should 7、nit 9。處理見 `build/evidence/v8-product-pvp-port/segment3-design-review.zh-Hant.md`（本機）與宣告「第 3 段的其他判斷」。修正後加測試：走動的對手的大腿（瞄腳前 0.45、高 0.65 的射擊在步態的某一步打到往前跨的腿）、第一人稱武器 idle 的頂點在相機前（z 0.34～0.91）、重心在右下；開槍中頭仍跟著瞄準（S1）；「打到前方的玩家」改瞄胸口（膠囊退路拿掉後，所有玩家都在骨頭上判）。
- 突變：`games/object_fps_pvp/docs/mutations.json` 全份 21/21 killed（第 3 段 7 個：`pvp-bone-part-ignored`、`pvp-upper-mask-ignored`、`pvp-upper-action-replaces-aim`、`pvp-retarget-scale-ignored`、`pvp-character-not-shown`、`pvp-gait-not-judged`、`pvp-viewmodel-unturned`；「打到前方的玩家」改了，第 1 段的也一起重跑）。第一次跑時 `pvp-gait-not-judged` 存活：走路的對手從側面看，兩腿在射線上重疊，站姿也打得到腿；改成瞄腳前 0.45、高 0.65（站姿沒有東西、往前跨的腿會經過），探測過正常版腿 4 次、突變版 0 次。`pvp-upper-mask-ignored` 因 additive 改寫成 stale，改 find。
- **沒驗**：畫面的實際樣子（角色、動畫、viewmodel 的位置與動作、貼圖）。這個會話看不到畫面，交使用者試玩。

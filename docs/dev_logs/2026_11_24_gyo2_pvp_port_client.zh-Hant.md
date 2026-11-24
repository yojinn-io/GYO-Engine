# 2026-11-24 PvP 移植實作第 2 段：Client（`v8-product-pvp-port`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（PvP 移植實作）。作者 Opus 5.5／high。疊在第 1 段（#324）與 K3 Peer 修正（#323）上。
根據：事前宣告（`games/object_fps_pvp/docs/port-declaration.zh-Hant.md` 第 1 節第 2 段的列）、盤點第 2.1 節（present、App）。

## 做了什麼

1. **`logic` 加 `present/Controls`**：master `PointerCapture` 的規則（點一下或 Tab 取得游標，那一下不是開槍；Tab、Esc、失焦放開），照這一幀的狀態（input Module 留的）而不是事件序列；按鍵（開槍、跳、換彈）留到有一步取走（`Take()`）為止，只給那一步。
2. **`client` 函式庫**（`client/ClientWorld`）：
   - 兩個 World（render 頁第 7、8 節）：`Game`（第 1 段的 Client World＋Client 的 System；units 明寫，帶 `sdl_window`）與 `Render`（`extractFrom = "Game"`；`render_world`、`camera_render`、`sprite_render`、`mesh_render`、`sdl_gpu`）。
   - `ReadControls`（`Main`／`PreUpdate`，input、window 的 set 之後）：鍵盤、滑鼠 → 操作狀態；取得游標時要求視窗 `Locked`＋隱藏游標（PV9：W 已有）。
   - `WriteStepInput`（`FixedUpdate`／`First`、`ApplyInput:PlayerInput` 之前、重跑時不跑）：這一步的輸入寫進自己 Peer 的 `PeerInput` 這一步的格，每格只寫一次、在送出之前（設計審查 B1；lightyear 的寫法）。
   - `ShowArena`、`PlaceCamera`、`MoveBodies`、`AddBodies`：第一人稱相機（Client 自己的視角）、arena 的模型、每個活著的對手一個替身（`Mesh3d`）。arena 與替身都是 `gyo.voxel_mesh` 檔，經遊戲的 `VoxelModelLoader`（catalog 型別 `model`、格式 `json`）成為模型素材，palette 圖給 `Mesh3d::image`。替身是手寫的方塊（`characters/stand_in.voxel_mesh.json`），第 3 段換成角色模型。
   - `ShowHud`：第二個相機（2D、`order 1`、不清除、`RenderLayers{4}`）：準星、受擊的紅閃（`HitFeedback`）、HP、彈藥／裝填、狀態文字（連線中、點一下開始、倒下）；文字轉成圖的 `Sprite`（字變了才重做）。
   - `PlaySounds`：自己開槍、別人開槍（左右增益與距離）、命中、受擊、腳步、落地（`SoundCues`）。換彈聲在第 3 段（viewmodel 的動畫事件）。
   - Client 在步外做的實體（相機、HUD、arena 的圖、替身）都帶 `KeepThroughRollback`。
3. **執行檔**：Client 成為產品的 main（`gyo_object_fps_pvp`；`--connect A.B.C.D:P` 代替大廳 PV12、`--user`、`--frames`、`--content`、`--verbose`；關窗後 `NetService::Close()`＋`Flush`，Match 立刻拿掉玩家）；Match 改成第二個角色 `match`（`gyo_object_fps_pvp-match`，`gyo_install_product_target` 放在 Client 旁邊）。
4. **`world` 的小改**：Match 與 Client 的 World 明寫 units（帶了渲染單位的 Product 不會把它們放進遊戲 World）。
5. **內容**：audio（4 個 wav）、字型（BIZ UDPGothic，OFL 授權文字一起放）、替身、catalog 加模型、聲音、字型。

## 驗證

- 本機（Intel Mac）：`object_fps_pvp.logic` 37 案（加 controls 4 案）、`object_fps_pvp.client` 3 案（沒有視窗與 GPU：Match＋另一個 Client 經 MemoryLink，畫面在 Submit stage 讀：3D pass 有 arena 與對手、替身在對手的位置、HUD 的 2D pass 與文字圖；相機在眼睛、隨滑鼠轉、Match 的 yaw 跟著；點一下取得游標且不開槍、視窗被要求 Locked；按住 W 在 Match 走；再點一下開槍且有聲音；Esc 放開／一步四幀時每次點擊都到 Match／對手關閉後 Match 立刻拿掉、替身消失）、`object_fps_pvp.main`（gpu：Metal 開窗畫 30 幀）。
- 實機（這台 Mac，UDP）：Match＋兩個 Client（`--frames 400 --verbose`）：Match 記「user 11 joined」「user 12 joined」，兩個 Client 都「connected」、「playing」（之後一次）。**沒驗**：用滑鼠鍵盤實際對戰（這個會話沒有桌面操作的工具），交使用者。
- 突變：第 2 段 4 個（`pvp-press-lost`、`pvp-focus-loss-kept`、`pvp-cursor-not-locked`、`pvp-stand-in-not-shown`）4/4 killed。

## 已知限制

- 對手在 Client 上停在確認值（第 1 段審查 S5）；本機的來回短，差約 0.2 m 以內。
- 視窗不能改大小（PV10 live resize 在第 4 段）。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

blocker 1、should 5、nit 5。報告在主 checkout 的 `build/evidence/v8-product-pvp-port/segment2-design-review.zh-Hant.md`。

- B1（沒跑步的幀的按鍵到 Match 會丟：那一格在寫完之前就送出，Match 一格只收一次）→ 輸入改在固定步裡、`ApplyInput` 之前寫這一步的格（`WriteStepInput`），加一步四幀的測試。
- S4（關窗不送斷線）→ 程式結束時 `Close()`＋`Flush`，加測試。
- S5（測試沒驗的契約）→ 加 HUD 文字、聲音、相機轉向、替身的位置與消失。
- N5 → 資產來源記錄（`assets/object_fps_pvp/LICENSES/SOURCES.zh-Hant.md`）。
- 交使用者：S1（net 提供「本機輸入」的唯一寫入點，lightyear 的做法）、S2（`Text2d` 排進 U，兩個產品一起換）、S3（幀內插，和第 1 段 S5 一起決定）。

## 試玩後補（2026-11-24）

- 使用者在第 3 段試玩時指出：被擊中的方向沒有顯示。原因：這一段只把 `HitFeedback` 的紅閃接到畫面，盤點（`Present/HitFeedback` 列）要搬的方向弧、HP 高亮、鏡頭震動，`Sample` 已經算出，HUD 沒畫。
- 改：`ShowHud` 畫方向弧（`ui/hit_feedback.json` 的 `direction`：半徑 110 px、弧 30°、7 個點，往攻擊者、相對於視角）、HP 文字在 `hp_highlight` 的時間內變紅；`PlaceCamera` 排在 `ShowHud` 後，鏡頭照 `shake` 晃（只動相機，不動玩家控制的視角、輸入與射線）。
- 測試：`client: a hit shows where it came from…`（Match 記一次受擊後，弧的中點指向攻擊者、HP 變紅、相機有 roll；1.5 秒後都消失）。突變 `pvp-hit-direction-not-drawn`、`pvp-hit-hp-not-red`、`pvp-hit-no-shake` 3/3 killed。`ctest -L "cpu|gpu"` 全過。

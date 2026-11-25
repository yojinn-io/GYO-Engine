# Object_FPS_PVP 移植實作的事前宣告（`v8-product-pvp-port`）

日期：2026-11-24。GYO2 主會話開的任務卡（PvP 移植實作）。作者 Opus 5.5／high；每段的設計審查也是 Opus／high（Fable 暫停，作者與審查者同模型，報告寫明）。
根據：[移植盤點](porting-inventory.zh-Hant.md)（第 2.3 節、第 3 節 PV1～PV20、第 4、5、6 節）。基準 `gyo2` `9ab6cfe0`。
這份宣告屬於產品；刪掉 Object_FPS_PVP 時跟著刪（AGENTS §1-8）。新 AGENTS 不等核准，寫完就動手；碰到要推翻使用者決定或動 AGENTS §1 的事，停下問主會話。

**鐵則**（使用者 2026-11-20）：權威、回滾、預測的算法、常數、數據不參考 master；2026-09-25 的 dev_log 不引用；11-01 之後的數據只當比較基準。盤點標 ⚠A、⚠W 的不搬。master 的 proto 只取語意。下面引用 master 的地方都是**遊戲規則**（速度、跳躍高度、HP、彈匣、部位倍率、平手規則）或**內容**，不是權威、回滾的做法。

## 1. 分段（每段一個 draft PR，後一個疊在前一個上）

```text
v8-product-pvp-port
├─ 第 1 段 sim      high｜模擬與 Match：arena、移動、射擊、判定、重生、World 的登記、Match 執行檔
├─ 第 2 段 client   high｜Client：視窗、輸入、連線入口、3D 畫面（arena、角色先用方塊）、HUD、聲音 → 能實際對戰
├─ 第 3 段 model    high｜角色模型與骨骼動畫、viewmodel、第三人稱武器、骨頭上的 hitbox（PV2）、換彈事件聲
├─ 第 4 段 engine   high｜PV10 live resize、PV11 模擬在自己的執行緒（Engine：SDL 總宣告 R3、R5），單獨的 PR
├─ 第 5 段 lobby    high｜大廳（PV12）：輸入 Match 的位址、連上；用 U 卡的 ui、world_ui、ui_render（2026-11-24 主會話加）
└─ 第 6 段 present  high｜遠端玩家內插（PV17）、viewmodel 後座力（2026-11-25 使用者定「移植完成＝和 master 功能一樣」後加）
```

〔2026-11-24 主會話加第 5 段〕大廳在移植範圍內，原本四段沒排，用 `--connect host:port` 代替。加段是主會話判的，不是使用者的決定。**先後：第 5 段先、第 4 段後**（卡自己判）：第 5 段只等 #322、#326 合進 gyo2，接法已由 U 卡寫好、驗過，是產品的接線；第 4 段改編排層與 `sdl_window`（R3、R5、O-13），開工前另寫前提與別家的對照。主會話 2026-11-24 確認這個先後，並說第 4 段的前提與對照寫完就動工、不必先交使用者：R3、R5、O-13 都在已核准的 SDL 總宣告裡，照它實作不算推翻使用者的決定；只有「要改總宣告已定的內容」或「要動 AGENTS §1」時停下問主會話。

| 段 | 範圍 | 不做 | 測試 |
|---|---|---|---|
| 1 sim | `logic` 加：arena 內容（`pvp_arena.json` 等＋`gyo.voxel_mesh` 的 colliders）、移動規則（站姿、蹲、跳、重力，經 `physics_3d` 的 `MoveAndSlide`）、射擊規則（射線方向、射程、平手、部位；部位只到規則層，判定在第 3 段的骨頭 hitbox 之前只有身體的膠囊，設計審查 N4）、戰鬥規則（HP、傷害、部位倍率、彈匣、冷卻、換彈、重生、選 spawn）。新 `world`：元件、輸入、System、權威與網路的登記、玩家的生成與刪除。新 `match` 執行檔（無畫面、UDP）。資產：maps、rules | 畫面、聲音、模型、動畫 | `object_fps_pvp.logic` 加 arena、移動、射擊、戰鬥；新 `object_fps_pvp.world`（Match＋兩個 Client 經 MemoryLink：連上、生成、移動、開槍、命中、死亡、重生、Client 的預測收斂到 Match）；新 `object_fps_pvp.match`（執行檔 `--ticks N` 自己結束） |
| 2 client | 新 `client` 函式庫與執行檔：`sdl_window`、`input`（相對滑鼠、鎖游標；`PointerCapture` 的規則）、連線入口 `--connect host:port`（大廳 PV12 的代替）、arena 的網格（quads＋palette）、角色先畫方塊、第一人稱相機、HUD（準星、HP、彈藥、受擊）、聲音（`SoundCues`、`HitFeedback` 已搬的部分接上）。資產：audio、fonts。〔動工後補〕兩個 World（`Game`＋抽取它的 `Render`，render 頁第 7、8 節）；arena 與替身經遊戲的 voxel model loader（catalog 型別 `model`、格式 `json`）成為 `Mesh3d`；HUD 是第二個相機的 2D 層（`Shape2d`、文字轉成圖的 `Sprite`）；Client 成為產品的 main，Match 是第二個角色 `match` | 大廳的 UI（等 U） | 新 `object_fps_pvp.client`（沒有視窗與 GPU：渲染 World 不帶 `sdl_gpu`、在 Submit stage 讀畫面，注入 input／window 的 Message；連上本行程的 Match）、`object_fps_pvp.main`（gpu：開窗畫 30 幀）；本機實際對戰交使用者 |
| 3 model | 角色（`Player_V20.fbx`＋UAL1 retarget）、步態與跳躍的動畫規則、分層／additive／aim／附掛、viewmodel（`Nailer1_FPS.fbx`）、第三人稱武器、Match 上骨頭的 hitbox（PV2）、換彈的 `magazine_in` → 換彈聲 | 時間型 crossfade、IK、root motion（master 也沒有） | 動畫規則、姿勢合成、hitbox 的外部測試；Match 讀 FBX 不碰 render |
| 4 engine | `sdl_window` 的 live frame（R3）、模擬在自己的執行緒（R5） | — | SDL 的測試照總宣告；改 Engine 單獨 commit／PR |
| 5 lobby | 大廳畫面（`pvp_lobby.json`，`ui` 型別）：位址欄、送出、狀態與提示；送出後連上 Match、進入對局，大廳關掉（拿掉 `UiRoot` 實體或換 `canvas`）。接法照 `docs/architecture/target/v2/ui.zh-Hant.md` 第 9 節（下面「PV12」） | master 的 HTTP 房間與配對（PV18）；遠端玩家內插（PV17） | client：大廳畫出來、打位址（含過濾）與送出後連上 Match、錯的位址顯示狀態；`--connect` 照舊可用（測試與腳本用）；實機（`sdl_window` 的 IME、`sdl_gpu`）交使用者 |

PR 標題 `type(scope): 說明（v8-product-pvp-port）`；draft、base `gyo2`；每段手動 dispatch L1。

## 2. 卡裡自己判的

前提的寫法（使用者 2026-11-22）：先寫「Engine 要解決（只有 Engine 做得到）」還是「作者能處理」。

### PV8 voxel 地圖的讀取 → **遊戲自己讀**

- 前提：作者能處理。`gyo.voxel_mesh` v1 是工具（voxel 地圖編輯器）交出的資料契約；格式在 master 已標 deprecated（`docs/architecture/voxel-mesh.md` 的 Status：「Nothing new is added to it」），GYO2 沒有讀它的 Engine 單位。
- 原則：「工具與遊戲只用資料形式交接，Engine 不承擔工具職能；工具自己持有知識＋對齊測試」。把讀它的程式放進 Engine 等於在 GYO2 復活一個已退場的格式。
- 做法：遊戲讀它要的欄位（`origin`、`cell_size`、`palette`、`quads`、`colliders`），照契約的換算（`float(units + 16 × origin) × cell_size / 16`，`voxel-mesh.md` 的 Units）得到世界座標；colliders 進 `physics_3d` 的 `BoxSet`，quads 第 2 段做成網格。只做遊戲要的檢查（欄位在、型別對、`min < max`、`dir` 0～5、palette 索引）。對齊測試：`pvp_arena` 的 5 個 collider 換算出的 AABB 逐值比對。
- 換成分塊格式（`gyo.voxel_map`）時，同樣由遊戲或屆時有點名需求的 Engine 單位處理，不在本卡。

### PV6 動畫取樣的跨平台決定性 → **不要求逐位元相同，不比內容 digest**

- 前提：作者能處理（判定只在 Match）。伺服器權威下 Client 只呈現；命中（含第 3 段骨頭上的 hitbox）只在 Match 算，Match 是一個行程、一個平台，取樣在同平台重跑相同（盤點 4.4、4.6 的量測事實）。
- 所以：動畫狀態與姿勢**不登記成判定資料**（不進一致性指紋、不進 digest、不進 Client 的回滾）。兩端各自由複製來的移動狀態推動畫（盤點 5「Client 由移動狀態推」）。master 的 `PlayerAnimationDigest`（⚠W）不搬。
- 系譜：lightyear 的範例只在伺服器算命中（`examples/fps/src/server.rs` 的 `compute_hit_lag_compensation`），Client 不比動畫內容。

### PV3 命中回溯 → **這一卡不做，寫明**

- master 沒有（盤點 PV3）：判定用當 tick 的姿勢。GYO2 照做：Match 用當 tick 的玩家位置與姿勢判定。
- 後果：Client 看到的遠端玩家落後（見下面的「遠端玩家」）；要瞄準要提前量。網路已經帶每格的 `ViewTime`（`net.zh-Hant.md` 第 9 節），之後做回溯時不用改 wire。
- 系譜（之後做時的起點）：lightyear `crates/integration/avian/src/lag_compensation/history.rs:27-72`（伺服器每個實體留位置、旋轉、AABB 的歷史，依 Client 的內插延遲查）。

### 動畫事件 → **遊戲讀**（第 3 段）

- 前提：作者能處理。用到的只有一個：viewmodel 的 `Armature|Reload` 在 1.887 s 的 `magazine_in`（盤點 4.3）。遊戲讀 `viewmodel.anim_events.json`、自己判「這一幀的片段時刻有沒有越過事件」。有第二個產品要時再看要不要進 model。
- 〔第 3 段實作〕換彈聲在 `magazine_in` 的時刻減去聲音的同步點（`viewmodel.event_sounds.json` 的 `sync_seconds`）起播，一次換彈播一次。

### 第 3 段的其他判斷（動工後補）

- **共用的角色**（`character` 函式庫）：Match 與 Client 用同一份骨架、retarget 過的 16 個片段、上半身遮罩、模型到世界的放法、骨頭上的 hitbox。Match 不連 render、同步讀 FBX（盤點 4.5）。
- **分層照遊戲**：下半身（步態或跳躍）、上半身（瞄準或動作）逐節點取一半（盤點 4.3「上下半身分層：遊戲」）。上半身的動作是 additive：瞄準姿勢加上動作片段（或 idle）從第一幀起的變化（照 master 的組合方式，只取概念；設計審查 S1）。
- **Match 的動畫狀態**：每個玩家一個 `PlayerMotion`（不是判定資料、不複製；PV6），`AnimatePlayers`（只在 Match）每步推進；`JudgeShots` 用它的姿勢擺骨頭上的形狀（PV2），同一目標取最近的形狀、等距取高的部位。沒有 `PlayerMotion` 的實體不算在對局裡；沒有角色時 `JudgeShots` 回錯（不退回膠囊，設計審查 S3）。
- **判定依賴不登記的狀態**（設計審查 S2）：命中的部位依 Match 上的 `PlayerMotion`（步態的相位），它不在權威、回滾、一致性的登記裡。Match 只有一個、不回滾，所以判定仍只由 Match 決定；代價是 Match 重啟或換 Match 時步態從頭算，部位可能和原本不同。接受，理由同 PV6。
- **對手的時刻**（設計審查 S5）：Client 畫對手用最新確認的 tick（`NetReplicaState::latestConfirmed`），不用自己預測的 tick；對手的開槍、受擊、這一命的起點都是那個 tick 上的值。
- **viewmodel 掛在相機下**（設計審查 S6）：第一人稱武器是 viewmodel 相機的子實體（`ChildOf`），固定的局部 `Transform`（`ViewmodelPlacement`），每幀只寫 `SkinPose`；死亡時離開 viewmodel 的 layer。照 Bevy 的 `examples/camera/first_person_view_model.rs`。
- **手臂不判定**（試玩後確認）：試玩時提到手臂以前打得到（傷害同身體）。那是膠囊判定時期（master 11-01 前、本移植第 1、2 段）的行為；`v8-product-hit-rules-three-parts`（2026-10-31）定的是手臂不設判定。使用者 2026-11-24：「先不改了」，維持三部位。
- **沒搬**：viewmodel 的後座力（master 的 recoil 曲線；不影響判定，之後要時再加）。〔2026-11-25 改成第 6 段做，見下面「第 6 段」〕
- **替身拿掉**：第 2 段的方塊替身換成 Player_V20，替身檔與 catalog 列一起刪。

### PV10、PV11 → **第 4 段，單獨的 PR**

- 前提：Engine 要解決。OS 的 modal 迴圈（live resize）和執行緒的入口只有 `sdl_window` 與編排層做得到（SDL 總宣告 R3、R5、O-13）。
- 放最後：遊戲在單執行緒、不 live resize 下就能玩；先把能玩的版本交出（穩定版先）。開工時另寫這一段的前提與別家的對照（Bevy 的 `bevy_winit` 的 resize、Godot 的 `DisplayServer` 的 modal 迴圈）。
- 〔2026-11-24 主會話〕排在第 5 段之後；前提與別家對照寫完就動工（理由與停下的條件見第 1 節第 5 段的註）。

### PV12 大廳 → **第 5 段，用 U 卡的 UI**（2026-11-24 主會話加）

- 前提：作者能處理。UI 的機制（`ui`、`world_ui`、`ui_render`）由 U 卡做好（#322；組字游標改成位元組的 #326），大廳的畫面、動作、位址的規則是遊戲的。
- 接法照 `ui.zh-Hant.md` 第 9 節：
  1. Module 列：來源 World 加 `world_ui`、`window_camera`（`units` 明列，漏了 `window_camera` UI 會安靜地不畫），渲染 World 加 `ui_render`。
  2. 登記 `Ui::UiDocumentLoader`、`Text::FontLoader`；catalog 裡 `pvp_lobby.json` 型別 `ui`、字型 `object_fps_pvp.font.ui` 型別 `font`。
  3. 生一個實體掛 `UiRoot{document, canvas = "lobby"}` 與 `UiBindings{address, room, status, hint}`；3D 場景＋HUD 並存時用 `UiRoot::camera` 指 HUD 的相機。
  4. `Update`（`after` `WorldUi::kUiSet`）讀 `UiAction` 分派：`pvp.submit_address` 帶字串 → 連線；處理完改寫 `UiBindings`。
  5. 位址欄的字元過濾（master 的 `Pvp/AddressText` 的規則）放進 `UiRoot::filter`。
  6. 組字游標直接用位元組（`input` 的 `TextComposition.cursorByte`／`selectionBytes`，`sdl_window` 換好）。
  7. 錯誤讀 `UiRootState::failure`，由產品記 log 或顯示。
- 動工條件：#322、#326 合進 gyo2（等使用者說「合併」）。
- 連線的時機：現在 Client 在 Product 建好時就連（`NetProductOptions` 帶 server endpoint，`NetService` 建構時固定）。大廳要「送出才連」：大廳先跑不帶 net 的 Product，送出後再建帶位址的 Client（net 頁 D-8：重連＝新 client World 的新連線，產品的事），歸作者處理（主會話 2026-11-24 確認）。開工確認時若 Engine 缺入口，先問主會話。
- 〔第 5 段實作〕程式的流程：大廳（自己的 Product：`Game`＋`Render`，`world_ui`、`window_camera`、`ui_render`，沒有 net）→ 送出有效的位址就關窗、回到程式 → 用那個位址建 Client 的 Product → 連線結束（被拒、逾時、Match 關掉；net 的 `PeerDisconnected`）時 Client 自己關窗，程式帶著理由回到大廳；玩家關窗或按「終了」就結束程式。`--connect` 照舊（第一次不進大廳），`--frames N` 只跑一個 Product（測試用）。代價：大廳和 Client 是兩個 Product，各開一個視窗，換的時候視窗會關掉再開（D-8：連線的位址在 Product 建好時就定）。
- 〔第 5 段實作〕大廳的文件是產品自己的 `assets/object_fps_pvp/ui/pvp_lobby.json`（gyo.ui v2；位址欄、提示、狀態、「終了」），不帶 master 的房間按鈕（PV18 不做）；送出只有 Enter（文字框的動作）。位址只收 IPv4（`A.B.C.D:P`，`OS::SocketAddress::Parse`），和 `--connect` 同一個函式 `ParseMatchAddress`。

### PV14 內容比對 → **遊戲用複製的 Resource**

- 前提：作者能處理。Match 把 arena 的 id 與 `BoxSet` 的內容摘要放進複製的 Resource（`MatchInfo`）；Client 照 id 換成自己裝的 arena（master 的「Client 選 Match 的 arena」，盤點 2.1 `Arena` 列），摘要不同就斷線並記錄理由。不改握手（網路的握手只比協議）。
- 動畫內容不比（PV6）。

### PV15 斷線理由 → **不需要 Engine**

- 前提：作者能處理。這一卡的 Match 沒有產品自己的驅逐（master 的驅逐是連線品質，⚠A，不搬）。滿房用 `NetProductOptions::admission` 回錯，網路回 `Refused` 帶說明文字（`net.zh-Hant.md` 第 6 節）。之後要驅逐時，遊戲先送一則伺服器訊息再關。

### 其他（照別家、已定原則）

- **玩家的生成**：Match 在 `PeerConnected` 時生成玩家實體（`OnServer()`），斷線時刪掉。玩家實體帶複製的 `Owner{userId}`；Client 找自己 userId 的那個寫進 `PeerControls`。系譜：lightyear `examples/fps/src/server.rs:54-84`（連上時 spawn、`PlayerId`／`ControlledBy`）。
- **K3**：Client 選回滾策略（`StateRollbackStrategy`），預測自己的玩家。
- **遠端玩家**：網路不送別人的輸入（網路宣告 Q7）。Client 上遠端玩家沒有輸入元件（輸入元件只有 Client 自己控制的玩家有：`ControlOwnPlayer` 加上），`MovePlayers` 不跑它們，所以它們停在確認點的值（約晚一個來回，連重力也不算）。〔動工後改，設計審查 S5〕原寫「輸入是預設值、重力照算」，和程式不符。遊戲規則「這一步沒有輸入就不移動、不轉向」用在 Match 上還沒收到輸入的玩家。內插（PV17）是遊戲的事；lightyear 把遠端玩家當 interpolated（`InterpolationTarget`），GYO 的 K3 目前全部走回滾，這是和 lightyear 的差。判定用 Match 當 tick 的位置（PV3），所以 Client 要提前量；第 2 段實機試玩前，交使用者選「先做內插或回溯其中一個」還是「接受提前量」。
  〔2026-11-24 主會話〕**遠端玩家內插（PV17）這一卡不做**：對手停在最新確認的值（第 3 段起對手的動畫也用最新確認的 tick）。這是主會話的判斷；使用者試玩時覺得礙事，再另外排。〔2026-11-25 改成第 6 段做，見下面「第 6 段」〕
- **只在 Match 跑的**：命中判定與傷害、死亡、重生、生成與刪除玩家（`OnServer()`，兩端登記同一個 System，一致性指紋相同；`net.zh-Hant.md` 第 8 節 D-13）。Client 預測的是移動、彈藥、冷卻、換彈（開槍聲與彈藥數字不等伺服器）。
- **時間**：tick 照 `world_time`（網路預設 64 Hz）；master 的 60 Hz tick 數換成秒（冷卻 10 tick → 1/6 s，換彈 90 → 1.5 s，重生 180 → 3 s），每步的秒數讀 `FixedTime` 的步長。
- **地板**：arena 有隱含的地板 y＝0（master 的規則，盤點 PV1）；遊戲規則，不放 collider。
- **數學**：判定資料用 float（同 master）；Match 與 Client 不同平台時只是多修正，不會壞（伺服器權威）。

### 第 6 段：遠端玩家內插（PV17）與 viewmodel 後座力（2026-11-25 加）

**使用者的決定**（HANDOFF `v8-arch-gyo2-pvp-port-done-criteria`）：主會話問「(a) 和 master 功能一樣／(b) 核心玩法能打就算」，使用者「照建議 (a)，合併 #338」。所以上面寫「這一卡不做」的 PV17 與後座力改成這一段做。主會話轉達的範圍：內插只取概念、照 lightyear 對照，不照搬 master 的常數與算法；權威、回滾的鐵則照舊；內插是畫面上的事，不改判定。

**前提**
- 內插與後座力：**作者能處理**。兩者都只在 Client 的畫面（Frame World），不碰判定資料；網路宣告 Q7 定「網路不做內插」（`docs/architecture/target/v2/net-module-declaration.zh-Hant.md` 第 615 行「PvP Client 移植時再問」），這一段照它：歷史、取樣、混合都在遊戲。lightyear 把內插放在函式庫裡（`crates/replication/interpolation`）；GYO 先由遊戲做，第二個產品要時再看要不要搬進 Engine（交主會話記）。
- 「現在 Match 大概到哪個 tick」：**Engine 要解決**。這個估計只有 net 算得出來（`ClockSync` 用封包的到達時刻、RTT、jitter 平滑出的 offset，`engine/modules/net/src/Replication.cpp` 的 `SyncClock`），但算完沒存下來；作者只能照抄 net 的公式（`now / step + offset`）。改法：`ClockSync` 多存三個值——這一幀的估計 `remoteTick`、RTT `rttTicks`、jitter `jitterTicks`（lightyear 的 `RemoteTimeline::current_estimate()` 是公開的，`crates/core/sync/src/timeline/remote.rs`）。只多存、不改同步的行為。〔設計審查 B1 後加 `rttTicks`〕

**別家**
| | 時間軸 | 延遲 | 沒有更新的資料時 | 重生、瞬移 |
|---|---|---|---|---|
| lightyear | `InterpolationTimeline`：server 時間的估計減延遲，用和 input timeline 同一個調速控制器跟上（`crates/replication/interpolation/src/timeline.rs:189-215`、`crates/core/sync/src/timeline/sync.rs:73-135`） | `max(送出間隔 × 1.2, 5 ms)` ＋ jitter × 4 ＋ 1 tick（`timeline.rs:50-75`、`sync.rs:205-220`） | 停在最後一筆，不外插（`registry.rs:553-559`） | 不處理（作者用 `disabled()` 等規則） |
| master | 只看封包到達的時刻（到達時間的最小包絡線），游標只前進、誤差大就跳（`SnapshotTimeline.hpp`） | 一個 tick | 停在最後一筆 | 換了一命或瞬移就切段，不跨段混合 |

**設計**（只取概念；數字取 lightyear 的預設，或遊戲自己定，不用 master 的）
1. **時間軸**（Frame）：`Sim` 的「出」把 `ClockSync::remoteTick`、`rttTicks`、`jitterTicks` 放進 `ClientView`。Frame 每幀用自己的 `RealTime` 推進一個內插用的 tick（小數），目標＝`remoteTick − rttTicks / 2`（能拿到的最新狀態；加上快照到了之後過了多久）減延遲；〔設計審查 B1：原寫照 lightyear 用 `remoteTick` 本身，單程時間比延遲長（RTT 約 86 ms 以上）時時間軸跑到所有狀態前面、停在最新一筆，又變回一步一跳；lightyear 唯一的測試是 RTT 0。這裡偏離 lightyear，減去半個 RTT〕延遲＝`max(1.2 個送出間隔, 5 ms)` ＋ 4 × jitter ＋ 1 tick（Match 每一步送一次）。跟上目標用和 `ClockSync` 相同的規則：第一次或誤差超過 10 tick 就跳；同方向超過 1 tick 連續 3 次就以 1.05 倍加速或減速；否則照常速。還沒同步（沒有 `remoteTick`）時，對手照舊用最新確認的值。
2. **歷史**（Frame）：每個遠端玩家一份 `(確認的 tick, PlayerBody, PlayerCombat, HitRecord, ShotRecord)`，快照的 `latestConfirmed` 前進時加一筆（遠端玩家在 Client 不模擬，快照裡就是那個 tick 的值）。修剪照 lightyear：至少 3 筆、而且第二舊那筆已被越過時才丟最舊的。
3. **取樣**：找「≤ 內插 tick 的最後一筆」與下一筆；位置、pitch 線性，yaw 取最短角度；其他（戰鬥、受擊、開槍紀錄、蹲、著地）取前一筆。沒有下一筆就停在最後一筆，不外插。兩筆之間換了一命（`lifeTick` 不同）就不混合，越過後一筆的 tick 才換過去（master 的切段，只取概念）。
4. **寫到哪裡**：遠端玩家的複本元件改由內插的 System 每幀寫（Frame 的 PreUpdate，畫面、HUD、聲音之前）；自己的玩家照舊由快照寫（預測值）。對手的動畫時刻用內插 tick（不再用 `latestConfirmed`）；對手的開槍聲、受擊在內插 tick 越過那一刻才觸發，和畫面同一條時間軸（master 也是這樣排聲音）。Sim 沒有的玩家，等內插 tick 越過它消失的那個確認 tick 才刪（lightyear 的延遲刪除）。
5. **畫面的時間**（net 的 F2「玩家按下時畫面看到的 Match tick＋小數」）：Frame 把內插 tick 隨控制交給 Sim，`WriteStepInput` 寫進 `ViewTime`（小數一個 byte）。Match 還沒用它（PV3 不做，master 也只記錄），但欄位的定義是畫面的時間，這裡讓它照定義。
6. **後座力**：開槍的那一刻（Frame 第一次看到自己的 `lastShotTick` 變大，用 Frame 的時鐘；預測收回的一槍不踢），viewmodel 繞相機的 X 軸往上踢（左手系、+Z 朝前：繞 +X 正轉是往下，所以取負角；設計審查 B2）、指數回復：`踢的角度 × exp(−回復率 × 經過的秒數)`。只轉 viewmodel，不動世界相機、不動瞄準（master 的註解也是這樣）。兩個數字放進 `nailer_v2_viewmodel.json` 的 `recoil`（`kick_degrees`、`recovery_per_second`），值是遊戲自己定的。
7. **後果**：命中仍由 Match 用當 tick 的位置判定（PV3 不做，master 也沒有），對手在畫面上比之前多晚一點（約 2～3 tick），瞄準要多提前一點；和 master 相同。
8. **不處理**（設計審查 S3、N3、N4）：Sim 的回滾在權威領先或放棄（長時間卡頓）時，遠端玩家的值沒更新而 `latestConfirmed` 前進，歷史會記一筆「舊值標新 tick」，之後跳回正確的值；剛出現的玩家在時間軸追上它的第一筆前先顯示最新值；Match 的 World 被替換、tick 重來時同 id 的玩家會凍住（這個遊戲沒有替換 Match 的 World）。機率低或目前不會發生，寫明不處理。

**測試與突變**
- `logic`：時間軸的跟上規則（第一次跳、連續 3 次才調速、誤差小照常速）、取樣（混合、yaw 最短角度、停在最後一筆、換命不混合、修剪留兩筆）。
- `client`：對手在一步四幀時每幀都有不同的位置（不再一步一跳），而且落後最新確認值；Match 收到的 `ViewTime` 是畫面的時間（落後最新確認值、不超前）；開槍後 viewmodel 往上轉、之後回到原位；既有的案照樣過（對手離開後替身消失）。
- `net`：`ClockSync::remoteTick` 連上後接近 Match 的 tick。
- 突變：不混合（停在前一筆）、延遲為 0、換命照樣混合、後座力不套用、`ViewTime` 照舊用最新確認值、`remoteTick` 不存。

## 3. 測試

- 本機 `ctest -L "cpu|gpu"`；網路測試（UDP）只在本機。
- `object_fps_pvp.world` 用 MemoryLink（標籤 `cpu`，CI 會跑）：
  1. Match＋兩個 Client 連上，各有一個玩家實體，Client 找到自己的（`PeerControls`）。
  2. Client A 往前走 N tick：Match 上 A 的位置變了，兩個 Client 的確認值等於 Match 的；A 自己的預測在輸入停下後收斂到 Match 的值（逐值）。
  3. A 對著 B 開槍：彈藥減少、冷卻、B 的 HP 減少；打到第 4 發 B 死亡，重生時間後在離 A 最遠的 spawn 復活、HP 滿。
  4. 換彈：彈匣空時開槍被拒、換彈後滿。
  5. 第 5 個 Client 被拒（`Refused`）。
- 規則的外部測試：arena 的讀取與檢查、移動（走、撞牆滑、跳的最高點、蹲下時站不起來）、射線（牆擋、平手、部位）、戰鬥（傷害倍率、冷卻、彈匣、換彈、重生、選 spawn）。

## 4. 突變（只跑相關的）

第 1 段（`pvp-` 前綴，清單 `games/object_fps_pvp/docs/mutations.json`，照 `run_mutations` 的格式）。〔動工後改〕原列的 `pvp-damage-on-client`（傷害 System 拿掉 `OnServer()`）換成 `pvp-respawn-early`：Client 跟著預測傷害時，確認值一到就相同，結果觀察不出差別（等價突變），留著只會存活、不檢驗任何契約。

| id | 改法 | 誰要殺 |
|---|---|---|
| `pvp-jump-no-gravity` | 跳躍不減垂直速度 | 移動：跳的最高點 |
| `pvp-crouch-stands-anywhere` | 站起不查空間 | 移動：蹲下時站不起來 |
| `pvp-shot-wall-ignored` | 射線不看牆 | 射擊：牆擋 |
| `pvp-tie-higher-id` | 平手取大的 id | 射擊：平手 |
| `pvp-head-multiplier-ignored` | 部位倍率不用 | 戰鬥：傷害倍率 |
| `pvp-cooldown-ignored` | 不檢查冷卻 | 戰鬥：冷卻 |
| `pvp-spawn-nearest` | 選最近的 spawn | 戰鬥：選 spawn |
| `pvp-collider-origin-ignored` | 換算不加 origin | arena：對齊測試 |
| `pvp-no-input-moves` | 沒有輸入也照上一格移動 | world：沒有輸入的玩家不動不轉（Match 上，設計審查 N1） |
| `pvp-respawn-early` | 死了立刻復活 | world：重生時間 |
| `pvp-room-not-full` | admission 不數人 | world：第 5 個被拒 |

第 2 段（同一份清單）：

| id | 改法 | 誰要殺 |
|---|---|---|
| `pvp-press-lost` | 沒跑步的那一幀的按鍵丟掉 | controls：按鍵等到它那一步 |
| `pvp-focus-loss-kept` | 失焦不放開游標 | controls：Tab、Esc、失焦 |
| `pvp-cursor-not-locked` | 不要求視窗鎖游標 | client：點一下取得游標 |
| `pvp-hit-direction-not-drawn` | 不畫往攻擊者的方向弧 | client：被擊中時的方向弧、HP 變紅、鏡頭晃動（試玩後補：第 2 段只接了紅閃） |
| `pvp-hit-hp-not-red` | 被擊中後 HP 不變紅 | 同上 |
| `pvp-hit-no-shake` | 被擊中不晃鏡頭 | 同上 |
| `pvp-stand-in-not-shown` | 不替別的玩家做替身 | client：畫出地圖與對手（第 3 段替身換成角色，換成下面的 `pvp-character-not-shown`） |

第 3 段（同一份清單）：

| id | 改法 | 誰要殺 |
|---|---|---|
| `pvp-bone-part-ignored` | 骨頭上的形狀一律算身體 | world：頭、身、腿各自的傷害 |
| `pvp-upper-mask-ignored` | 上半身照下半身的片段 | world：頭跟著瞄準 |
| `pvp-retarget-scale-ignored` | retarget 的位移不縮放 | world：角色的骨架、片段、身高 |
| `pvp-character-not-shown` | 不替別的玩家做身體 | client：對手的每個網格 |
| `pvp-upper-action-replaces-aim` | 上半身的動作蓋掉瞄準（不是加上去） | world：開槍中頭跟著瞄準 |
| `pvp-gait-not-judged` | Match 的玩家不推進步態（射擊只碰站姿） | world：走動的對手往前跨的腿 |
| `pvp-viewmodel-unturned` | 第一人稱武器不轉向 | client：idle 姿勢在相機前、右下 |

第 5 段（同一份清單）：

| id | 改法 | 誰要殺 |
|---|---|---|
| ~~`pvp-lobby-filter-off`、`pvp-lobby-any-address`~~ | 遊戲流程框架第 3 段拿掉：直接輸入位址的大廳換成 Gateway 的大廳（`lobby/AddressText` 一起刪） | — |
| `pvp-lobby-quit-ignored` | 大廳的「戻る」不回主菜單（遊戲流程框架第 1 段改寫：大廳成為場景） | client：主菜單 → 大廳 → 主菜單 |
| `pvp-lobby-ui-not-drawn` | 大廳的渲染 World 沒有 `ui_render` | client：渲染 World 畫出 UI 的 pass |
| `pvp-session-end-ignored` | 連線結束沒記下（程式不會回大廳） | client：Match 拒絕時狀態寫明理由 |

第 4 段的清單在它的宣告（`docs/architecture/target/v2/live-frame-sim-thread-declaration.zh-Hant.md` 第 5 節）。

## 5. 完成條件

- 每段：draft PR 的 L1 三平台綠，回報主會話 PR、head、L1 run、審查結論。
- 全部：這台 Mac 上 Match＋一個以上的 Client 能實際對戰；實機試玩交使用者。

# Object_FPS_PVP 移植盤點表

來源：`origin/master` 的 `apps/object_fps_pvp/`（162 檔，最後改動 `5e65c4b1`，2026-11-12）、`tests/object_fps_pvp/`、`assets/object_fps_pvp/`（最後改動 `7b20b1f9`，2026-11-07）、`services/gyo_gateway/`，以及它用到的 master Engine（`engine/collision`、`model`、`render`、`net`、`voxel_mesh`、`ui`、`input`、`audio`、`text`、`platform`、`runtime`、`threads`、`entity_store`）。
移植目標：`games/object_fps_pvp/`（遊戲源碼）、`assets/object_fps_pvp/`（資產）、`tests/object_fps_pvp/`（遊戲自己的測試）。
GYO2 基準：`gyo2` `3ffb9e54`（2026-11-23）。這份表屬於產品；刪掉 Object_FPS_PVP 時跟著刪（AGENTS §1-8）。

讀法：每一項寫「原本是什麼 → 在 GYO2 對到什麼 → 狀態」。「Engine」指 Engine 的單位（Module、Kernel、core）與 Plugin；「遊戲」指 `games/object_fps_pvp/` 自己的程式（AGENTS §1-1：替わりうる規則は作者が選ぶ）。行號都以 `origin/master` 為準。

**master 只當需求來源。** 權威、回滾、預測的算法、常數、數據不參考（使用者 2026-11-20，網路宣告 P9）；11-01 之後的數據可以當比較基準。表裡標 **⚠A** 的是這一類；標 **⚠W** 的是 master 的 wire 或 digest 契約（GYO2 的網路 Module 已換成自己的 wire，這些不帶）。

## 1. 原專案的組成

| 層 | 原目錄 | 行數（約） | 依賴 |
|---|---|---|---|
| 模擬（Sim） | `include/RetroFPS/Sim/*`、`src/Sim/*` | 3,400 | `engine/collision`、`voxel_mesh`、`model`（ufbx）、`math`、`entity_store`、nlohmann |
| 權威與網路（Pvp） | `include/RetroFPS/Pvp/*`、`src/Pvp/*`、`match_main.cpp` | 11,000 | asio、httplib、protobuf、`engine/net`、`threads`、`time`、`trace`、`os` |
| 呈現（Present、App） | `include/RetroFPS/{Present,App}/*`、`src/{Present,App}/*`、`main.cpp`（含 `PvpApplication` 1,600 行） | 4,200 | `engine/render`、`model`、`ui`、`input`、`text`（SDL_ttf）、`audio`、`platform/sdl`、`runtime` |
| 舊 campaign 的殘留 | `World/GridMap.hpp`、`Data/GameData.hpp`、`Gameplay/Weapon/*` | 330 | PvP 的模擬沒有用到；`GridMap` 沒有 `.cpp` |
| 協議 | `protocol/client_v9.proto`、`runtime_v9.proto`、`README.md`（生成的 `.pb.go` 不算） | 830 | protobuf |
| Gateway（Go） | `gateway/`、`services/gyo_gateway/` | 14,500 | Go 1.23、protobuf |
| 測試 | `tests/object_fps_pvp/*`（doctest＋Python） | 28,000 | 上面全部 |

master 的結構：Client ↔ Gateway（Go 行程，外部 protobuf）↔ Match（C++ 行程，內部 protobuf 經 TCP IPC）。GYO2 的網路 Module 換成「Link 之上一套自訂 wire、帳本在 Match World 的 Peer 實體、Gateway 不另開行程」（網路宣告 D-3、Q2）。所以 master 的 wire、IPC、Gateway 這一層大多不搬（第 2.4 節）。

## 2. 逐項對照

### 2.1 遊戲自己的（搬進 `games/object_fps_pvp/`）

| 原本 | 內容 | 在 GYO2 | 狀態 |
|---|---|---|---|
| `Sim/PlanarMovement`、`Sim/GroundPoint` | 搖桿軸＋yaw → XZ 方向與位移（長度不超過 1，yaw 0 朝 +Z） | 遊戲（`sim/`），只用 `core/math` | **PR 2 搬** |
| `Present/HitFeedback`（含 `ClientView`） | 被擊中的紅閃、HP 高亮、方向弧、鏡頭震動；滑鼠 → yaw／pitch；讀 `ui/hit_feedback.json` | 遊戲（`present/`）。`Observe` 原本吃 master 的 `CombatState`（⚠A 混合的型別），只用到其中「這一命」與「受擊次數」兩欄，改成遊戲自己的小結構；JSON 改經 Engine 的 `json` Module | **PR 2 搬** |
| `Present/SoundCues` 的純部分 | `sfx.json` v3、立體聲 pan＋距離衰減（`SpatialGains`）、`FootstepTracker`、`HitTakenSounds` | 遊戲（`present/`）；JSON 改經 `json` Module。`RemoteShotTracker` 吃 master 的 `WorldSnapshot`，不搬（GYO2 沒有 snapshot，由複製元件取代，之後在 World 層重寫） | **PR 2 搬**（純部分） |
| `Pvp/AddressText` | 大廳位址欄的字元過濾（全形轉半形、只留 a-z A-Z 0-9 . : - [ ]） | 遊戲（`lobby/`） | **PR 2 搬** |
| `Sim/Movement` 的規則部分 | 一步移動：stance、蹲、跳（初速 `sqrt(2gh)`、解析式恆加速度）、重力；`StanceRules` 的驗證 | 遊戲。同檔的 `Movement.hpp:16-104` 幾乎全是 ⚠A（tick rate、輸入送件、修正、命令窗、相位追蹤、resync、連線品質），`PlayerState` 帶 epoch、lastResolvedCommand、slack（:152-175）也是 ⚠A：要逐段剝開、另定乾淨的狀態 | **等 3D 物理**（著地、站起、sweep 都是物理查詢，第 3 節 PV1） |
| `Sim/MovementRulesFile`、`rules/movement_rules.json` | 蹲姿身高、眼高、速度倍率 | 遊戲（資料契約）；驗證要 arena 的身高與半徑 | 和移動一起 |
| `Sim/Arena` 的內容模型 | arena 檔（id、尺寸、角色參數、spawns、walls）與驗證（spawn 2～64、eye ≤ body…） | 遊戲。`ArenaContentDigest`（⚠W，`Arena.cpp:175-199`）、`ArenaHostsRoom`／`MaxPlayers`（:241）不帶。schema 2 的牆來自 `*.voxel_mesh.json` 的 colliders，GYO2 沒有讀它的單位（第 3 節 PV8） | 等 PV8 與 3D 物理 |
| `Sim/ShotQuery` 的規則 | 射線方向 `(sin yaw·cos p, −sin p, cos yaw·cos p)`、pitch ±89°、射程、平手規則（等距世界優先、玩家取小的 id、同一目標頭＞身＞腿） | 遊戲；查詢本身是物理（PV1） | 等 3D 物理 |
| `Sim/Combat` 的規則子集 | HP、基礎傷害、部位倍率、彈匣、單發上限、`ShotDamage` | 遊戲。同檔的 action window、referenceAge、ShotStepLead／Hold、`ShotRequest` 的 epoch／sequence／viewTick 是 ⚠A；冷卻、裝填、重生以 master 的 60 Hz tick 計（`AuthorityTickRate` 是 ⚠A），要在 GYO2 的時間（`world_time`）下重新定單位 | 搬 World 層時（判定由權威 Module 的伺服器策略承接，規則重寫） |
| `PvpMatch` 裡的純規則 | 選 spawn（離最近敵人最遠、平手照內容順序，`PvpMatch.cpp:269-290`）、死亡與重生（:301-351）、`Decide` 裡的彈匣、裝填、傷害分支（:363-439） | 遊戲的 System；其餘整個類別是 ⚠A（join／leave、輸入收件、補位、backlog、resync、action ledger） | 搬 World 層時重寫；選 spawn 的「放得下」要物理 |
| `Sim/PlayerAnimation` | 下半身的步態相位、walk↔jog 權重、跳躍階段；由移動狀態推 | 遊戲的規則。Q16／permille 的量化與 `ValidPlayerAnimations` 是 master 的 snapshot 契約（⚠W），不帶；浮點版的規則要和第 4 節的動畫設計一起定 | 等骨骼動畫的設計 |
| `Sim/PlayerPose`、`PlayerHitbox`、`PlayerHitModel`、`PlayerAnimationContent` | 由動畫合成整副骨架的姿勢（分層、additive、aim blend、武器附掛）；把 hitbox 形狀擺上骨頭；Client 與 Match 共用 | 遊戲（規則與資料）＋Engine（model 匯入、取樣、混合、蒙皮；第 4 節）。`PlayerHitModel` 的介面與 synthetic 膠囊可以先搬 | 等 model 與骨骼動畫 |
| `Sim/PlayerAnimationDigest` | 動畫內容的 SHA-256，Match 送、Client 比對（⚠W） | 要不要有內容比對，看第 3 節 PV6、PV14 | 搬產品時判 |
| `Present/PointerCapture` | 視窗內左鍵或 Tab 取得滑鼠、失焦／Tab／resize 釋放、取得那一下不算開槍 | 遊戲的規則；輸入型別改成 W 的 `input` | 等 W（規則是純函式，W 合併後搬；golden `fixtures/pointer_capture_golden.tsv` 787 KB） |
| `Present/WeaponEventSounds` | viewmodel 換彈的 `magazine_in` 事件 → 換彈聲，依 clip 與動作時長比例排起播時刻 | 遊戲；動畫事件的資料（`gyo.anim_events`）與讀取在動畫那一側 | 等骨骼動畫 |
| `Present/PvpSound` | 依時刻排程播 cue、沒有裝置時靜音 | 遊戲的 System＋`audio`（`PlayRequest` 已有 `at` 與左右增益） | 等 A（裝置輸出） |
| `Present/PlayerPresentation`、`WeaponViewModel`、`WeaponPresentationDefinition`、`App/*` | 遠端玩家、第一人稱手臂＋槍、角色與武器的定義檔 | 遊戲的「每幀產生畫面資料」System；接 R2b 的 3D 管線與 model | 等 model、骨骼動畫、R2a、R2b |
| `Present/SnapshotTimeline` | 遠端玩家的內插時間線（收包時刻、延遲、hold） | 網路 Module 不做內插（網路宣告 Q7）；`DelaySeconds` 鄰接 ⚠A，要重新推 | 搬 World 層時判（第 3 節 PV17） |
| `Pvp/PvpApplication`、`main.cpp` | 整個 Client：大廳↔世界、HUD、相機、地板、arena mesh、viewmodel、音效觸發、live resize | 產品的 `main` 建 `Product`；拆成遊戲的 System 與 Resource | 等 W、R2a、R2b、U、A |
| `match_main.cpp` | Match 執行檔：`--arena`、`--listen`、載內容、算 digest | 遊戲的 Match Product 入口（網路 Module 的 `NetProductOptions`）；不帶 SDL 的列（SDL 總宣告 Q4：缺後端不算錯） | 搬 World 層時 |
| `Pvp/LobbyActions`、`ui/pvp_lobby.json` | 大廳的 `gyo.ui` 文件（panel、text、text_field、button、focus 順序）與 action 對照 | 遊戲；控件與文件格式是 U | 等 U |
| `Pvp/SessionSummary` | 射擊計數的每秒摘要（診斷） | 遊戲；吃 `CombatState` | 低優先，搬 World 層時看要不要 |

### 2.2 對到 GYO2 已有的單位

| 需要的能力 | 原本 | GYO2 | 狀態 |
|---|---|---|---|
| ECS、System、Resource、Message | `entity_store`＋手寫的 `PvpMatch` 欄 | Kernel、`orchestration` 的 `Product` | 已有 |
| 固定步、tick、時間 | `runtime/FixedTickRuntime`、`AuthorityTickRate`（⚠A） | `world_time`（`FixedUpdate`、Tick；網路預設 64 Hz） | 已有 |
| 權威判定 | `PvpMatch`（⚠A） | `authority` 的 `ServerAuthorityStrategy`：作者登記判定資料（`AuthorityEntry`） | 已有；判定規則是遊戲的 |
| 本機預測、校正 | `LocalPlayerPrediction`、`ClientSimulation*`（⚠A） | `rollback`（狀態觸發、逐元件歷史、平滑修正）＋網路 K3（`net.zh-Hant.md` 第 13 節） | 已有；**不搬**舊的 |
| 傳輸、握手、存活、重連 | `ClientConnection`、`ConnectionLiveness`、`Wire`、Gateway | `net` PR-1（#275）：UDP Link、確認與重送、兩個通道、分片、丟棄計數、握手、存活、重連 | 已有 |
| 複製、輸入、對時 | protobuf snapshot、輸入窗、`ClockSync` | `net` PR-1：元件與 Resource 的複製、伺服器訊息、輸入（以 Peer＋tick 為鍵、冗餘 5、hold-last）、`SyncClock` | 已有 |
| 私人狀態、相關性、頻寬 | 沒有（4 人全送） | `net` PR-2（#290）：`privateComponent`、`PeerVisibility`、值的預算與累積優先度；頻寬判定 ≤ 512 kbps | 已有；4 人 PvP 的實測在移植後（PV20） |
| 實體編號 | `EntityId` 分段 | `entity_ids`（`RangeAllotter`）；網路 NT-9：伺服器配、範圍由產品分開 | 已有 |
| 記錄、trace | `LogFile`、`ClogLine`、`engine/trace` 的 sink | `logging`、`trace` | 已有 |
| 讀資料檔 | `std::ifstream`、`NativeFiles` | `asset`（catalog、`TextLoader`）＋`world_asset`；資料契約的 JSON 經 `json` Module | 已有 |
| 數學 | `engine/math` | `core/math`（同一組標頭：Vec3、Quaternion、Matrix4、Aabb、Capsule、Ray、Plane、Intersection、Angle、Scalar；namespace 改成 `GYO::Engine::Math`） | 已有 |
| 文字 | SDL_ttf＋BIZ UDPGothic | `text`（FreeType＋HarfBuzz） | 已有 |
| 音效混音 | `audio` 的 `Mixer`、`ClipBank` | `audio`（`Mixer`、共用不可變的 `Clip`、`WavLoader`；`PlayRequest` 有起播時刻與左右增益） | 已有；裝置輸出在 A |
| PNG | SDL_image | `image`（PR #301，待合併） | 等 I 合併 |
| 相機、Transform、父子 | render 的相機 | `transform`、`camera`（X1）；`GlobalTransform` 的父子傳播（X2，PR #303 待合併） | 已有／待合併 |
| 狀態（大廳、載入、對局） | `PvpApplication` 裡的旗標 | `state_machine` | 已有 |
| 讀檔、存檔 | 沒有 | `save_version` | **不需要** |

### 2.3 要等的子批

| 子批 | Object_FPS_PVP 用到的 |
|---|---|
| W（window／input） | WASD、Space、LeftCtrl（按住）、左鍵、R、Tab、Esc；大廳的 Up／Down／Enter／Esc；**相對滑鼠模式**（取得滑鼠時隱藏並鎖住游標，每幀一次相對移動量）；焦點（失焦釋放滑鼠、清掉按下的邊緣）；大廳位址欄的**文字輸入與 IME 區域**；輸入事件的 OS 時刻（本機開槍聲的起播時刻） |
| R2a（render core） | 3D pass、深度、unlit 的 shader、半透明（紅閃、HUD）、貼圖（PNG）、CPU 更新頂點（master 是 CPU 蒙皮） |
| R2b（render pipelines） | 3D 管線：世界相機（near 0.05／far 150）、**viewmodel 用另一個相機（FOV 55°、near 0.01、far 5）並清深度**、不測深度的 world overlay、後處理（exposure／gamma）；2D 疊層：HUD（準星、文字、方向點）；1280×720 的縮放 |
| U（ui） | 大廳的 `gyo.ui` 文件（panel、text、text_field、button、focus 順序、binding） |
| A（audio output） | 混音器輸出到裝置；沒有裝置時遊戲照跑 |
| 3D 物理（同時開的卡） | 第 3 節 PV1、PV2 |
| model（ufbx；現在就開，`v8-arch-gyo2-model-now`） | FBX 匯入、骨架、片段、取樣、混合、retarget（第 4 節） |
| 骨骼動畫 | 第 1 層併在 model、第 2 層先不做、第 3 層蒙皮在 render（第 4 節） |
| SDL 總宣告留給 PvP Client 的 | R3（OS modal 迴圈中的 live frame，master 的 live resize）、R5（模擬在自己的執行緒，O-13） |

### 2.4 不搬

| 原本 | 為什麼 |
|---|---|
| `Pvp/LocalPlayerPrediction`、`ClientSimulation*`、`PredictionElapsedTime`、`Sim/ShotGate`、`Pvp/MatchSubstitutionRecords`、`Pvp/InputSendBudget` | ⚠A：預測、對帳、射擊估算、補位、送件的算法與常數；GYO2 由 `rollback`＋網路 K3 承接 |
| `Pvp/PvpMatch` 的權威骨架、`Pvp/MatchRuntimeHost` | ⚠A：收件、補位、backlog／starvation reset、resync、連線品質窗與驅逐門檻（10 s、3 次、160 ms、50‰，`Movement.hpp:101-104`）；GYO2 由 `authority`、`world_time`、`net` 承接，連線品質要的話遊戲重新推 |
| `Pvp/ClientConnection`、`IpcHost`、`Wire`、`WireCounter`、`ConnectionLiveness`、`ClockSync`、`protocol/*` | ⚠W：wire 換成網路 Module 的（`net.zh-Hant.md` 第 3 節）；大廳的 HTTP 那一半見 PV18。proto 的**語意**當遊戲需求的來源（第 5 節） |
| `gateway/`（Go）、`services/gyo_gateway/` | GYO2 已核准「Link 在行程內、帳本在 Match 的 Peer」（網路宣告 Q2）；Gateway 原意（外部多種 Client、內部一套協議、無狀態）落在 net：多種 Client＝多種 Link、一套協議＝NT-2、無狀態＝帳本在 Peer。之後要另開轉送行程時協議不用改（宣告 :567）。rate limit 與 rebinding 見 PV19 |
| `Pvp/IngressStatistics`、`MatchIngressLedger`、`MatchIngressTrace`、`MatchShotTrace`、`MovementDiag`、`MovementTrace*`、`NetworkStatistics`、`TracePaths`、gateway 的 `drop_report` 等 | 綁舊事故的診斷；教訓已吸收成網路的「每個丟棄都有理由」（NT-8）。要量時用 `trace`、`logging` 重做 |
| `World/GridMap`、`Data/GameData`、`Gameplay/Weapon/*` | 舊 campaign 的殘留，PvP 的模擬沒有用到 |
| `Sim/ArenaMesh` | render 端的轉接，R2b 與 PV8 定了再寫 |
| `Sim/EntityColumns`、`EntityId` | master `entity_store` 的轉接；GYO2 是 Kernel 的實體 |
| 測試：`PvpMatchTests`、`PredictionTests`、`CombatTests`、`CombatHostTests`、`ClientSimulation*Tests`、`MovementRecoveryTests`、`LifePredictionTests`、`ShotGateTests`、`PredictionElapsedTimeTests`、`MovementDiagTests`、`KernelPrepDetectTests`、`EntityModelTests`、`AuthorityDigest`、`fixtures/authority_golden*`、`WireTests`、`IpcHostTests`、`ClockSyncTests`、ingress／drop／log 的 fixture 與 Python 檢查、`mutations.json`（464 KB） | ⚠A／⚠W 或綁舊事故；`MathCharacterizationTests` 是 master 換 math 時的等價檢查，也不搬 |

## 3. 缺口（GYO2 沒有、Object_FPS_PVP 需要）

| # | 缺的能力 | Engine 的機制還是遊戲的事 | 原本怎麼用 | 影響的批 |
|---|---|---|---|---|
| PV1 | **3D 物理的查詢**：直立膠囊（`feet, height, radius`）對靜態 AABB 的 overlap＋contact（推出牆，最多 8 輪）、sweep＋slide（最多 5 輪）、著地判定（向下 probe 0.002、法線 y > 0.5，feet ≈ 0 算站在隱含地板）、放置檢查（膠囊對牆與其他膠囊）、射線對平面／AABB 格（最近命中＋平手）／球與線段膠囊；broadphase 是均勻格 | Engine（`physics_3d`）。「推出 → sweep → slide」是通用的 kinematic character controller（avian、bevy_rapier 的 KinematicCharacterController、Godot `move_and_slide`），迭代數、skin、probe、法線門檻由作者給。速度、蹲、跳、重力、身高、隱含地板、部位排序、射程是遊戲的 | `CharacterCollision.cpp:58-161`、`ShotQuery.cpp:72-129`、`Movement.cpp:63,100` | 3D 物理卡（需求已轉主會話，共享筆記 `gyo2-pvp-prep-physics-needs.md`） |
| — | **沒用到的**：移動中角色互推（`MoveCharacterBody` 的 actors 一律傳 `{}`，`Movement.cpp:100`）、台階、斜坡、三角網格碰撞、OBB、剛體 | — | — | 物理卡不必為 PvP 做 |
| PV2 | **骨頭上的命中形狀**：每 tick 依動畫姿勢把 7 個形狀（頭球 1、軀幹膠囊 2、腿膠囊 4；10 根骨：Head、spine_01、spine_03、pelvis、thigh_l/r、calf_l/r、foot_l/r）擺到骨頭上，射線判定部位 | Engine：射線對球／膠囊（PV1）＋骨架姿勢（第 4 節）；遊戲：`hitbox.json`、部位與倍率 | `PlayerHitModel.cpp:28-53`、`PvpMatch.cpp:632-656`（每 tick 第一次有人開槍時對每個活著的目標算一次） | 3D 物理、model、骨骼動畫 |
| PV3 | **命中的回溯（lag compensation）** | 未定：網路只帶「玩家看到的時間」（輸入格的 `ViewTime`，網路宣告 F2），歷史姿勢存哪、依時間插值怎麼查，網路宣告寫「產品或 collision 存」、不在網路本線 | **master 沒有**：判定用當 tick 的姿勢（`PvpMatch.cpp:404-415` 的 `TickShapes()`），view tick 只記錄（`client_v9.proto:235`「v7.1 only records them」），只用 `maximumReferenceAgeNs` 擋太舊的請求（⚠A）。GYO2 要做就是全新設計 | 3D 物理或 PvP 搬產品時的宣告 |
| PV4 | **FBX 匯入** | Engine（model 函式庫，ufbx 在它的 `src/adapters/ufbx/`，使用者已定 Q2；現在就開，`v8-arch-gyo2-model-now`） | 第 4 節 | model |
| PV5 | **骨骼動畫**：clip 取樣（Loop／Clamp）、混合、依名字一對一的 retarget、CPU 蒙皮、動畫事件 | Engine：取樣、混合、retarget 在 model 函式庫（第 1 層）、蒙皮在 render（第 3 層）；遊戲：播哪個片段、播到哪裡照 tick 推（第 2 層先不做），分層、additive、附掛（`v8-arch-gyo2-skeletal-animation-layers`） | 第 4 節 | 骨骼動畫 |
| PV6 | **動畫取樣的跨平台決定性** | 未定。master 要求 Client 與 Match 的載入＋retarget 結果逐 byte 相同（內容 digest 比對，不同就斷線）；量到的結果：載入的 hash 四平台相同，**取樣結果跨平台不同**（linux-x64＝macos-arm64，windows-x64、macos-x64 各自不同，原因是 Slerp 等浮點），同平台重跑相同（dev_log `2026_10_31_pvp_v8_anim_bottlenecks`） | `PlayerAnimationDigest.cpp`、`ClientConnection.cpp:308-316` | 骨骼動畫、網路（伺服器權威下 Client 只呈現，命中只在 Match 算；要不要比對內容，搬產品時判） |
| PV7 | **viewmodel pass**：另一個相機（FOV、near、far）＋在世界之後清深度 | Engine（R2b 的 3D 管線） | `RenderTypes.hpp` 的 `MeshLayer::ViewModel`、`WeaponViewModel.cpp:147-150` | R2b |
| PV8 | **voxel 地圖的讀取**：`*.voxel_mesh.json`（gyo.voxel_mesh v1 單檔格式，master 已標 deprecated）的 colliders（AABB，單位 1/16 cell）與畫面用的 quads＋palette；點名地圖 pvp_arena／pvp_corners 各 5 個 collider、biome 1,260 個（1.4 MB） | 未定：這是工具（voxel 地圖編輯器）交出的資料契約；讀它的程式放 Engine 的地圖單位、還是遊戲自己讀 AABB 列表，搬產品時判。master 的 `map_markers` PvP 沒用 | `Arena.cpp`、`engine/voxel_mesh` | 搬產品時（和 3D 物理的靜態幾何接法一起） |
| PV9 | **相對滑鼠模式**（取得滑鼠時鎖住游標、每幀相對移動量） | Engine（W 的 `window` 請求＋`input` 的 Message）；和 Object_Connect 的 G4（相對移動量）同一件，PvP 多一個「鎖住游標」的請求 | `PvpApplication.cpp:1200` | W |
| PV10 | **live resize**（OS modal 迴圈中照樣出幀） | Engine（SDL 總宣告 R3，留一個回呼點，PvP Client 移植時做） | `PvpApplication.cpp:1386-1400` | PvP Client 移植 |
| PV11 | **模擬在自己的執行緒** | Engine（SDL 總宣告 R5、O-13 的入口） | `ClientSimulationRole`（呼叫點屬於 ⚠A，執行緒的需求本身不是） | PvP Client 移植 |
| PV12 | 大廳的 UI 文件與控件 | Engine（U） | `ui/pvp_lobby.json` | U |
| PV13 | 依時刻播聲音、左右增益 | 已有（`PlayRequest` 的 `at`、`gainLeft`／`gainRight`）；聲音都是短的單發，播放中不改增益 | `PvpSound.cpp`、`SoundCues.cpp` | 不是缺口 |
| PV14 | **內容比對放進握手**（arena、動畫內容是否相同） | Engine 小擴充，或遊戲把 digest 放 token、由 `admission` 驗，或連上後用伺服器訊息比。網路的握手只比協議版本、協議雜湊、步長、id 範圍、一致性指紋 | `runtime_v9.proto:28`、`ClientConnection.cpp:308-316` | 搬產品時的宣告 |
| PV15 | **伺服器主動斷線帶產品的理由**（驅逐） | Engine 小事，或遊戲先送一則伺服器訊息再關。`ServerHost::Close` 只送 `Closed` | `Evicted` 等 | 搬產品時 |
| PV16 | **K8：父比子晚到**（lightyear `PendingChildOf`） | Engine（網路複製的後續，`net.zh-Hant.md` 第 8 節限界最後一條） | master PvP **沒有**複製的父子實體（武器是 Client 本機的 viewmodel；第三人稱武器是姿勢合成時掛在 `hand_r`，不是實體）。pv10 點名的寵物（「類似裝備，第一人稱、第三人稱都看得到」）與裝備附掛才會要 | 不擋 PvP 移植；pv10 時 |
| PV17 | 遠端玩家的內插 | 網路不做（Q7）；Engine 要不要給，未定，暫算遊戲的 | `SnapshotTimeline.hpp` | 搬 World 層時判 |
| PV18 | 大廳、房間、配對（HTTP `/rooms`、`/join`、`/leave`） | 遊戲或服務。GYO2 的建置已有 Go 服務的接法（`gyo_game_add_go_service`） | `gateway/server.go:168-171` | 搬產品時另立宣告 |
| PV19 | token 驗證、防偽造、rate limit（舊的 GCRA 120 pps）、endpoint rebinding | Engine（網路 Q7：加密與認證，未排） | `services/gyo_gateway/session` | 未排 |
| PV20 | 4 人 PvP 的實際頻寬 | 遊戲＋網路（RC-NT6）；網路的頻寬判定是合成情境 | — | 移植後量 |

**網路頁限界的對照**（`net.zh-Hant.md`）：最多 64 種複製元件（PvP 的玩家、戰鬥、動畫狀態遠低於此）；結構更新 ≤ 16 MiB（4 人遠低於此）；tick 截成 u32 不處理回繞（一局遠短於 2.1 年）；K3 上遠端玩家的輸入不送、實體約一個來回後才出現；可見性啟用時事件可能指向看不到的實體（M6，4 人競技場幾乎全看得到）。pv8 的教訓（假設 HitPart 夠用而漏了 hitbox）：本表的 PV2、PV3 就是 hitbox 的兩半，部位的資料（`HitPart`：頭、身、腿）是遊戲的複製元件或伺服器訊息，網路不必認得。

## 4. model 與骨骼動畫：PvP 的實際需求

「model Module 選 (A) 還是 (B)」與「骨骼動畫怎麼分三層」兩題，**使用者 2026-11-23 照主會話的建議定了**（主會話列出七件後，使用者「這個照你建議就好。」）：

- **model：(B)，現在就開**（HANDOFF `v8-arch-gyo2-model-now`）。另開一張卡，做成不依賴 render、GPU、SDL 的函式庫。
- **骨骼動畫**（HANDOFF `v8-arch-gyo2-skeletal-animation-layers`）：第 1 層放 model 函式庫（骨架、片段、時刻 t 的姿勢、混合、retarget）；第 2 層（每個實體的播放器 `AnimationModule`）**先不做**，PvP 由遊戲照 tick 推算片段時刻；第 3 層蒙皮放 render。

題目的原文（主會話 2026-11-23 提的，定之前的寫法）：

- **model 的前提**：model 放 Engine，做成 `engine/modules/model` 的函式庫（同 `audio`、`json`：不登記進 World），內容是節點階層、材質、蒙皮網格、動畫片段、取樣出 Pose，加 ufbx loader。**(A)** 照 11-19，model 跟著用它的產品（PvP、model_viewer）回來；**(B)** 現在就當基礎做，排在 SDL 之後或併進 SDL 的畫面子批。
- **三層**：1 資料與取樣（骨架、片段、「片段在時刻 t 的姿勢」的純函式；model 函式庫）；2 播放（每個實體播哪個片段、播到哪裡，照 World 的時間前進並取樣出 Pose；登記單位 `animation`，要能登記進回滾、取樣照 tick）；3 蒙皮顯示（網格照 Pose 變形，render 經 Extract 拿 Pose）。hitbox、何時切片段、混合規則是遊戲規則，產品讀第 2 層的 Pose。

4.1～4.4 是 PvP 實際用到什麼、誰做的；4.5、4.6 是定之前給使用者判斷用的對照（事實不變，留著當這兩個決定的根據）。

### 4.1 格式

- 只用 **FBX**，經 ufbx（master `engine/model/backend/ufbx/src/UfbxModelLoader.cpp`）。catalog 規定 model 只能是 `.fbx`（`docs/object_fps_pvp/assets.zh-Hant.md` §5）。執行時沒有 glTF、OBJ。
- 每個頂點只取最大的 4 個權重再正規化（`UfbxModelLoader.cpp:149-159`）；動畫用 `ufbx_bake_anim` 烘成 key（:179-190）。
- 貼圖只有 PNG；交付的 JPG 先轉 PNG。

### 4.2 各模型

| 模型 | 用途 | 蒙皮 | 骨骼數 | 材質／貼圖 | clip |
|---|---|---|---|---|---|
| `characters/player/Player_V20.fbx`（0.83 MB） | 第三人稱身體（所有玩家） | 是 | **77**（dev_log `2026_10_31_pvp_v8_anim_bottlenecks` 第 25 行「77 根骨頭」；是否含非骨骼節點未確認）；名稱與階層和 UAL1 的 65 根一致 | 15 個純色材質、沒有貼圖 | 自己沒有，16 個由 UAL1 retarget 過來 |
| `animations/ual_mannequin/UAL1_Standard.fbx`（23.8 MB） | clip 的來源 | 是 | 65 個骨骼節點 | — | 43 個（用到 16 個） |
| `weapons/nailer_v2/viewmodel/Nailer1_FPS.fbx`（2.8 MB） | 第一人稱手臂＋槍 | 是 | **53** | 4 個（3 個 4×4 單色 PNG、1 張 1024² PNG） | 自帶 5 個：Idle、Shoot、Reload、Draw、Hide |
| `weapons/nailer_v2/world/Nailer2_TPS.fbx`（0.37 MB） | 第三人稱手持武器 | 否（沒有骨架、沒有動畫） | 0 | 1 張 PNG | 0 |
| `tests/object_fps_pvp/fixtures/hair_buns/Hair_Buns.fbx` | 只在測試：單骨蒙皮的配件（掛 `Head`） | 單骨 | 1 | 1 張 PNG（1.7 MB） | 0 |

身體的 16 個 clip（`animations.animset.json`）：idle、walk、jog、shoot、reload、jump_start、jump_loop、jump_land、death、aim_up、aim_neutral、aim_down、hit、hit_head、crouch_idle、crouch_move。

### 4.3 用到的動畫功能與誰做

| 功能 | 有沒有用 | master 誰做 | 出處 |
|---|---|---|---|
| clip 取樣（Loop、Clamp） | 有 | Engine `SamplePose` | `PlayerPose.cpp:511-560`、`WeaponViewModel.cpp:81` |
| 兩個姿勢的混合（local TRS 插值） | 有：idle↔步態、walk↔jog、crouch_idle↔crouch_move、aim 的 neutral↔up／down（依 pitch） | Engine `BlendPoses`；權重是遊戲算的 | `PlayerPose.cpp:529-538,560-566` |
| 時間型 crossfade | **沒有**：換動作時直接切 clip；`transition_seconds` 只換成 Match 規則的 tick 數 | — | `PlayerPose.cpp:108,497-605` |
| 上下半身分層 | 有：從 `spine_01` 往下的子樹當上半身遮罩，逐節點覆寫 | **遊戲** | `PlayerPose.cpp:152-163,596-600`；`presentation.json` 的 `upper_body_root` |
| additive | 有：上半身的 aim 姿勢＋（動作 clip − 它的第 0 幀） | **遊戲** | `PlayerPose.cpp:46,570-588` |
| root motion | 沒有（位置來自模擬） | — | — |
| IK | 沒有 | — | — |
| 動畫事件 | 只有 viewmodel 的換彈：`Armature|Reload` 1.887 s 的 `magazine_in` → 換彈聲 | Engine 的 parse／bind／區間穿越；排程是遊戲 | `viewmodel.anim_events.json`、`PvpApplication.cpp:641-647` |
| retarget | 有：載入時依節點名一對一綁定、`translation_scale` 0.3125、忽略 `Mannequin` 節點 | Engine `TransferCompatibleAnimation`（不自動配對人形）；設定是遊戲的資料 | `CharacterPresentationDefinition.cpp:31-80`、`character.json` |
| 第一人稱與第三人稱 | 兩套獨立的模型與骨架；viewmodel 單一 clip、不混合、另加後座力旋轉 | 遊戲 | `WeaponViewModel.cpp` |
| 附掛 | 第三人稱武器以固定的 TRS 掛在 `hand_r`（`body.global[hand_r] × mount × 武器的參考姿勢`）；配件掛 `Head`（只在測試） | **遊戲** | `PlayerPose.cpp:166-191,618-625`、`presentation.json` 的 `weapon` |
| 蒙皮 | CPU 蒙皮，每幀重傳頂點 | Engine（master `ModelRenderer`） | `ModelRenderer.cpp:85,131` |

### 4.4 誰取樣、多頻繁

- **Match（伺服器）也取樣**：`match_main.cpp:125-151` 用 ufbx 讀 FBX（不解碼貼圖），每 tick 第一次有人開槍時，對每個活著的目標合成一次整副骨架的姿勢（一次 `SamplePose` 4～7 次），把 hitbox 擺上去（PV2）。Client 與 Match 共用 `ComposeBody`（`PlayerPose.cpp:497`）。
- 量測（同 dev_log）：Match 取 12 次樣 0.44 ms（linux）；載入 0.23 s、93 MB。
- 動畫狀態（步態相位、權重、跳躍階段）在 master 是 Match 的權威、量化後送給 Client（⚠W）。GYO2 的伺服器權威下這是遊戲的判定資料，要不要複製、Client 能不能自己推，搬產品時定（PV6）。
- **進了權威摘要，沒進預測**：master 的權威摘要從 v4 起把每位玩家的動畫狀態接在每個 tick 的紀錄後面（`tests/object_fps_pvp/AuthorityDigest.cpp:52-53,222-225`）；Client 的預測（`LocalPlayerPrediction.cpp`）完全不碰動畫——自己的身體在第一人稱看不到，遠端玩家的姿勢只是呈現，呈現不回流到判定（`docs/object_fps_pvp/player-presentation.zh-Hant.md` DI 4）。
- **內容摘要另一件**：`PlayerAnimationDigest`（SHA-256：影響姿勢的數值、16 個 clip 轉換後的 key、骨架）由 Match 送、Client 比對，不同就斷線（`runtime_v9.proto:28`、`ClientConnection.cpp:308-316`）。它比的是「載入＋retarget 的結果」，不是每幀的 Pose。

### 4.5 對上 model 的 (A)／(B)：PvP 需要什麼、什麼時候要（已定 (B)，`v8-arch-gyo2-model-now`）

| 事實 | 出處 |
|---|---|
| PvP 的模型全是 FBX（4 個執行時用的檔：身體、UAL1 片段來源、viewmodel、第三人稱武器），要 ufbx | 4.1、4.2 |
| **Match 沒有畫面也要 model**：載入骨架與片段（不解碼貼圖）、取樣整副骨架，給 hitbox 用。所以 model 函式庫要能不連 render、不連 GPU、不碰 SDL 地在伺服器上用 | `match_main.cpp:125-151`、`PlayerAnimationContent.cpp:74-98`（只登記 TextLoader 與 ufbx loader） |
| Client 畫第三人稱身體、手持武器、viewmodel 都要 model（＋R2b 的 3D 管線） | 2.1 的 `PlayerPresentation`、`WeaponViewModel` |
| 在 PvP 移植的路上，model 擋住的是：命中的部位（PV2）、Match 的判定、Client 的 3D 角色。不擋的是：移動（等物理）、HUD、聲音、大廳 | 第 2、3 節 |
| Object_Connect（另一個點名的移植）不用 model | `games/object_connect/docs/porting-inventory.zh-Hant.md`（PR #302） |
| master 的 model 在 Engine 提供的：`ModelAsset`（節點、材質、網格、片段）、`SamplePose`、`BlendPoses`、`MakeDefaultPose`、`TransferCompatibleAnimation`、`SkinMesh`、動畫事件的 parse／bind；載入用 nlohmann、和 asset 耦合（R0 帶不動的理由，`rebuild-r0-carry-list.zh-Hant.md:264`） | 4.3 |

### 4.6 對上骨骼動畫的三層：PvP 用到的每一項落在哪一層（已定，`v8-arch-gyo2-skeletal-animation-layers`：第 2 層先不做，「播哪個片段、播到哪裡」由遊戲照 tick 推）

| PvP 用到的 | 第 1 層（資料與取樣） | 第 2 層（播放） | 第 3 層（蒙皮顯示） | 遊戲規則 |
|---|---|---|---|---|
| 骨架：身體 77、viewmodel 53、UAL1 65（4.2） | ● | | | |
| 片段：身體 16（全部 retarget 自 UAL1）、viewmodel 5 | ● | | | 選哪些片段（animset 資料） |
| retarget（依名字一對一、`translation_scale`、忽略節點） | ● 載入時做 | | | 設定是遊戲的資料（`character.json`） |
| 取樣（Loop、Clamp）、兩個姿勢的混合 | ● 純函式（master 的 `SamplePose`、`BlendPoses`） | | | 權重怎麼算 |
| 播哪個片段、播到哪裡 | | master **沒有**每個實體的播放器：Match 每 tick 由移動狀態推出步態相位、權重、跳躍階段（`AdvancePlayerAnimation`），片段時刻由 tick 推（例：跳躍階段的零點 `jumpPhaseTick`），不讀真實時間 | | ● 推法是遊戲的（`PlayerAnimation.cpp`） |
| 遠端玩家的播放 | | Client 對兩份權威值內插（`InterpolatePlayerAnimation`），照呈現的時間 | | ● |
| viewmodel 的播放 | | 單一片段、依動作時長算時刻，只在本機 | | ● |
| 上下半身分層（`spine_01` 子樹）、additive、aim 混合、附掛到 `hand_r` | | | | ● master 全在遊戲（`PlayerPose.cpp`） |
| 動畫事件（換彈的 `magazine_in`） | ● 事件資料與區間穿越 | | | 事件 → 聲音的對照與排程 |
| hitbox 擺上骨頭（Match） | 讀整副骨架的全域變換 | | | ● 形狀、部位（`hitbox.json`） |
| 蒙皮：每頂點最多 4 權重；同時最多 4 個身體＋1 個 viewmodel | | | ● master 是 CPU 蒙皮、每幀重傳頂點；GPU 是 GYO2 的新設計 | |

**和「進回滾、進摘要」有關的事實**（給第 2 層定「要能登記進回滾、取樣照 tick」用）：

- master 的 Match 每 tick 推動畫狀態，那份狀態進權威摘要（4.4）；取樣只在 Match 判定時做、結果每 tick 快取（`PvpMatch.cpp:632-656`），姿勢本身不存、不進摘要。
- Client 不預測動畫、不用姿勢做任何判定；命中只在 Match 算。所以在 master 的設計裡，**Client 的回滾不需要動畫**。GYO2 的 PvP 要是改成 Client 也預測命中、或動畫狀態成為 K3 會回滾的複製元件，才會需要。
- 跨平台：載入的 hash 四平台相同，**取樣結果跨平台不同**（PV6）。Match（Linux）與 Client（Windows）的姿勢不會逐位元相同；master 因為只有 Match 判定，所以沒有出問題，含動畫的命中 golden 只做同平台比對。
- 量：Match 合成一次姿勢 `SamplePose` 4～7 次；12 次取樣 0.44 ms（linux）。

## 5. 從 master 的協議推出的遊戲需求（語意，不是 wire）

| 遊戲需求 | GYO2 怎麼承接 |
|---|---|
| 移動輸入（前後、左右、yaw、pitch、跳、蹲、衝刺） | 玩家實體上的輸入元件（網路第 9 節）；舊的 epoch、life、sequence、窗 12 不帶（⚠A） |
| 射擊、換彈（帶玩家看到的時間） | 輸入元件的單次觸發（C-IN3）＋每格的 `ViewTime`；舊的 action ledger、ACK 不需要 |
| 判定結果（接受、拒絕的理由、打到誰、傷害、部位） | 伺服器訊息（可指定收件者）或複製元件 |
| 玩家狀態、戰鬥狀態（HP、彈藥、冷卻、換彈） | 元件複製；彈藥、冷卻放私人元件 |
| 受擊紀錄（最後受擊的 tick、次數、攻擊者、部位） | 複製元件（`HitFeedback`、`SoundCues` 讀它） |
| 動畫狀態 | 複製元件，或 Client 由移動狀態推（第 4.4 節） |
| 對局規則（戰鬥、移動規則交給 Client） | 伺服器訊息或 `ReplicateResource` |
| 大廳、房間、join、leave | PV18；握手的 `admission` 決定收不收 |
| 斷線理由（離開、驅逐、錯誤） | `DisconnectReason`＋PV15 |

## 6. 資產

授權：使用者 2026-11-23「除了骨骼動畫，都是我自己做的。」（HANDOFF `v8-arch-gyo2-pvp-assets-license`）——master 記 `unrecorded` 的 8 個是使用者本人製作；骨骼動畫（UAL1）照 CC0。各檔跟著讀它的程式搬（model、A、asset 的接法），搬時把來源記錄改成這個決定。

規則（`docs/object_fps_pvp/assets.zh-Hant.md` §8）：非 text 的項目要有來源記錄；text 由 git 歷史記錄。來源記錄在 `origin/master:tests/object_fps_pvp/asset_sources.json`。

| 檔 | 大小 | 授權 | 狀態 |
|---|---|---|---|
| `animations/ual_mannequin/UAL1_Standard.fbx` | 23.8 MB | CC0（Universal Animation Library；`assets.zh-Hant.md` 附錄 A.3） | 可放；原樣進 git（主會話判，比照 Object_Connect 音訊的先例，`v8-arch-gyo2-pvp-assets-license`） |
| `characters/player/Player_V20.fbx` | 0.83 MB | 使用者在 Blender 做的（2026-10-29 交付），授權 `unrecorded` | 使用者本人製作，可放（`v8-arch-gyo2-pvp-assets-license`） |
| `weapons/nailer_v2/viewmodel/Nailer1_FPS.fbx` | 2.8 MB | 使用者交付（2026-10-29），`unrecorded` | 使用者本人製作，可放（`v8-arch-gyo2-pvp-assets-license`） |
| `weapons/nailer_v2/viewmodel/textures/Nailer1_D.png` | 1.3 MB | 由交付的 JPG 轉的，`unrecorded` | 使用者本人製作，可放（`v8-arch-gyo2-pvp-assets-license`） |
| `weapons/nailer_v2/viewmodel/textures/LowPoly_{Skin,Glove,Skin_Shadow}.png` | 各 74 B | 由交付的單色材質生成，`unrecorded` | 使用者本人製作，可放（`v8-arch-gyo2-pvp-assets-license`） |
| `weapons/nailer_v2/world/Nailer2_TPS.fbx` | 0.37 MB | 使用者在 Blender 做的（2026-10-31），`unrecorded` | 使用者本人製作，可放（`v8-arch-gyo2-pvp-assets-license`） |
| `weapons/nailer_v2/world/textures/Nailer2_D.png` | 1.25 MB | 由 FBX 內嵌的 JPG 轉的，`unrecorded` | 使用者本人製作，可放（`v8-arch-gyo2-pvp-assets-license`） |
| `audio/{gun_shoot,gun_reload,hit_flesh,player_footstep}.wav` | 共 350 KB（48 kHz、PCM16、stereo） | 使用者自己的混音（2026-10-30 使用者的話） | 可放（等 A 與 asset 的接法時搬） |
| `fonts/BIZUDPGothic-Regular.ttf` | 4.5 MB | SIL OFL 1.1 | 可放；要一起帶授權文字（Object_Connect 也用同一個字型，可共用一份，搬時判） |
| `maps/*.json`、`maps/*.voxel_mesh.json`（biome 1.4 MB、其餘各 23 KB） | 1.5 MB | 產品自己的資料（text） | 可放；讀法等 PV8 |
| `ui/hit_feedback.json`、`audio/sfx.json` | 0.4 KB、0.7 KB | 產品自己的資料（text） | **PR 2 搬**（`HitFeedback`、`SoundCues` 的測試讀它們） |
| 其餘 text：`characters/player/*.json`、`weapons/**/*.json`、`rules/movement_rules.json`、`ui/pvp_lobby.json`、`asset_catalog.json`、`content.json` | 15 KB | 產品自己的資料 | 跟著讀它們的程式搬；`content.json` 指向 master 的 shader bundle，GYO2 照 R2a 的形重寫 |
| `tests/object_fps_pvp/fixtures/hair_buns/*` | 2.0 MB | 測試用 | 等骨骼動畫時判 |

格式：FBX 經 model（PV4）、PNG 經 `image`、WAV 經 `audio` 的 `WavLoader`、JSON 經 `asset` 的 `TextLoader`＋`json`，都登記在 `assets/object_fps_pvp/` 的 catalog。

## 7. 搬法的取捨（卡片自己判的）

- **目錄與 namespace**：目錄 `games/object_fps_pvp`（和 master、`assets/object_fps_pvp`、`docs/object_fps_pvp` 同名）↔ namespace `object_fps_pvp` ↔ target `gyo_object_fps_pvp-logic`（命名規範 N17「產品用自己的根」；Object_Connect 同形）。master 的 `fps::pvp` 不沿用。決定性區的 System 與資料之後集中在 `object_fps_pvp::sim`（N24；N24 的例子寫 `fps::pvp::sim`，換成這個產品的根）。
- **只搬乾淨的部分**：master 的模擬與權威、預測、wire 混在同一個檔、同一個型別裡（`Movement.hpp`、`Combat.hpp`、`PvpMatch`）。整檔搬會把 ⚠A 帶進來，所以 PR 2 只搬輸入是單純的值、不碰 master 網路模型的單位；需要 master 型別的地方換成遊戲自己的最小結構（例：`HitFeedback` 只要「這一命」與「受擊次數」）。
- **JSON**：master 用 nlohmann；GYO2 的遊戲經 Engine 的 `json` Module 讀（遊戲依賴 Engine 是允許的方向，AGENTS §1-3）。錯誤的回報照原本（`std::optional`＋`std::string& error`）：遊戲內部的寫法是作者的（§1-1）。
- **測試框架**：沿用 master 的 doctest（GYO2 的 `third_party/doctest` 已有）；ctest 名 `object_fps_pvp.logic`，標籤 `cpu`。原測試照原樣搬，只改型別與 include；拿掉的 case 在第 8 節列出。
- **PR 的疊法**：PR 2 疊在 Object_Connect 的 PR #302 上——#302 讓「還沒有產品執行檔的遊戲」在 CI 的打包列公告後跳過，PvP 現在也沒有執行檔。合併順序 #302 → PR 2。

## 8. PR 2 搬的與拿掉的測試

| 測試 | 處置 |
|---|---|
| `HitFeedbackTests` | 全部搬；`CombatState` 換成遊戲的受擊結構 |
| `SoundCuesTests` 的設定、空間增益、腳步、受擊聲 | 搬 |
| `SoundCuesTests` 的 `RemoteShotTracker`、timeline 的顯示時間 | 不搬（吃 master 的 snapshot；PV17） |
| `AddressTextTests` | 搬 |
| `PlanarMovement` | master 只有 `MathCharacterizationTests` 的等價檢查（不搬）；照標頭寫的契約（yaw 0 朝 +Z、長度不超過 1、位移＝方向×速度×時間）補外部行為的測試 |

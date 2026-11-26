# 交互與 NPC 台本：成熟方案比較（`v8-arch-interaction-dialogue-research`）

更新：2026-11-26。Owner：研究卡（由 GYO2 重建主會話開出）。**狀態：研究，不實作。方向由使用者決定；這份文件不改任何已定的決定。**

查的版本：`gyo2` 7d06d987；Godot、Bevy、bevy_replicon、lightyear 用 `~/Code/GitHome` 裡的本地源碼（引用附檔名與行號，路徑從各 repo 的根算起）。inkcpp、Yarn Spinner、Godot Dialogue Manager、Unity、Unreal 不在本機，只讀網頁（附 URL），沒有下載或 clone。沒能在原文裡確認的寫「未確認」。

## 0. 問題與判斷依據

使用者想知道：之後要做「交互」（準心對著門 → 出現「[E] 開門」→ 按下就開門；多人時由 Match 判定）和「NPC 台本」（對話），還缺哪些系統。

判斷依據是 AGENTS.md §1：

- §1-1：Engine 只提供機制，替換得了的規則由作者選。
- §1-4：第三方元件的型別只出現在 adapter 裡。
- §1-6：名字是概念的 tag，產品的詞（門、NPC、任務）不進 Engine。
- §1-7：Tool 與 Game 只經資料格式交接。

每一項先寫前提「這是 Engine 要解決的（只有 Engine 做得到），還是作者能處理的」，再判斷放哪一層。

## 1. 交互

### 1.1 用門當例子拆成四步

| 步驟 | 要回答的問題 |
|---|---|
| A. 找目標 | 準心（或角色周圍）指著哪個「可以交互」的東西？ |
| B. 顯示提示 | 在畫面上顯示「[E] 開門」，文字從哪裡來？ |
| C. 按鍵 → 動作 | 按 E 之後，誰收到、誰執行「開門」？ |
| D. 多人判定 | Client 按 E，Match 怎麼知道、怎麼驗證、結果怎麼傳回？ |

### 1.2 Godot：`RayCast3D`／`Area3D`＋場景腳本

- **A. 找目標**：
  - `RayCast3D` 是一個節點，每個 physics frame 自己打一次射線：`NOTIFICATION_INTERNAL_PHYSICS_PROCESS` 呼叫 `_update_raycast_state()`（`scene/3d/physics/ray_cast_3d.cpp:206-223`），裡面用 `PhysicsDirectSpaceState3D::intersect_ray`，參數有 `collision_mask`、`exclude`、`collide_with_areas`（`ray_cast_3d.cpp:225-268`）。結果用 `is_colliding()`／`get_collider()` 讀（`ray_cast_3d.cpp:85-89`）。預設排除自己的父物體（`exclude_parent`，`ray_cast_3d.cpp:181-187`）。
  - `Area3D` 用於「靠近就能交互」：物理伺服器回呼 `_body_inout`（`scene/3d/physics/area_3d.cpp:225`），發 `body_entered`／`body_exited` 等 signal（`area_3d.cpp:756-762`）；也能直接問 `get_overlapping_bodies()`（`area_3d.cpp:516`）。
  - 篩選靠 collision layer／mask（32 個位元）。
- **B. 提示**：引擎沒有提供。常見寫法是玩家腳本讀 `get_collider()`，對方如果有某個方法或屬於某個 group，就把它的提示字串放進 HUD 的 `Label`。
- **C. 按鍵 → 動作**：`InputMap` 的 action（例：`"interact"` 綁 E），腳本用 `Input.is_action_just_pressed("interact")`（`core/input/input.h:380`），再呼叫目標的方法（duck typing 或 `has_method`）。**「可交互」這個概念不在引擎裡，是腳本的約定。**
- **D. 多人**：`@rpc` 標註；`MultiplayerAPI::RPC_MODE_ANY_PEER`／`RPC_MODE_AUTHORITY`（`scene/main/multiplayer_api.h:49-50`），伺服器用 `get_remote_sender_id()`（`multiplayer_api.h:69`）知道誰送的；節點的權威用 `Node::set_multiplayer_authority`（`scene/main/node.h:807`）。常見寫法（社群慣例，未找官方出處）：Client 呼叫 `rpc_id(1, "request_interact", path)`，伺服器驗證後改狀態，再由 `MultiplayerSynchronizer`（`modules/multiplayer/multiplayer_synchronizer.cpp`）同步。是否驗證距離、視線，完全是腳本的事。

### 1.3 Bevy：`bevy_picking`＋`MeshRayCast`＋生態的網路套件

- **A. 找目標**：
  - `bevy_picking` 是「指標（滑鼠、觸控、自訂指標）對著哪個實體」的通用機制。流程分成 `PickingSystems::Input → ProcessInput → Backend → Hover → PostHover`（`crates/bevy_picking/src/lib.rs:259-276`）：後端（backend）把命中寫成 `PointerHits{pointer, picks: Vec<(Entity, HitData)>, order}`（`backend.rs:93-117`），picking 合併成 `HoverMap`（`hover.rs:64`、`generate_hovermap` `hover.rs:141`）。實體上的 `Pickable{should_block_lower, is_hoverable}` 控制擋不擋後面、能不能被 hover（`lib.rs:199-254`）。
  - 指標可以是自訂的：`PointerId::Custom(Uuid)`（`pointer.rs:30-40`），FPS 可以放一個固定在畫面中心的虛擬指標。
  - 也可以不用 picking，直接用 system parameter `MeshRayCast::cast_ray`（`crates/bevy_picking/src/mesh_picking/ray_cast/mod.rs:175`、`:214`）打射線。
- **B. 提示**：picking 發 `PointerOver`／`PointerOut`（`events.rs:192`、`:256`）。作者在實體上掛 observer，收到 Over 就顯示提示、Out 就收起。提示的內容與 UI 是作者的。
- **C. 按鍵 → 動作**：picking 發 `PointerPress`／`PointerClick`（`events.rs:316`、`:353`），事件沿實體階層往上冒泡（`PointerTraversal`，`events.rs:115-121`）。「按 E 交互」不是指標按鈕，所以通常由作者自己讀 `ButtonInput<KeyCode>`，配合目前的 hover 目標。核心沒有 action map；生態用 leafwing-input-manager 或 bevy_enhanced_input（lightyear 也為這兩者各有一個輸入 crate：`crates/inputs/inputs_leafwing`、`crates/inputs/input_bei`）。
- **D. 多人**：Bevy 核心沒有網路。
  - bevy_replicon：Client 用 `client_trigger(event)` 送事件（`src/shared/message/client_event.rs:152-162`），伺服器收到 `FromClient<T>`（`client_message.rs:461`）。事件裡有 `Entity` 時用 `add_mapped_client_event`（`client_event.rs:43`），送出前把 Client 端的實體換成伺服器的實體。
  - lightyear：輸入走每 tick 的輸入緩衝（`crates/inputs/inputs_native/src/input_message.rs`），伺服器照 tick 套用。
  - bevy_picking 本身沒有任何網路或權威的概念（在 `crates/bevy_picking/src` 搜 `network`、`replicat` 沒有結果）。

### 1.4 Unity：`IInteractable` 慣例、XR Interaction Toolkit、Netcode

Unity 本體沒有「可交互」的概念；常見的有兩種：社群慣例，以及 XR Interaction Toolkit（XRI）這個官方套件。

- **A. 找目標**：
  - 慣例：每幀從相機打 `Physics.Raycast(origin, direction, out hit, maxDistance, layerMask, queryTriggerInteraction)`（<https://docs.unity.com/en-us/engine/current/script-reference/unityengine/physics/raycast>）。命中的物件如果實作 `IInteractable`（名字是社群慣例，不是 Unity 的型別），就是焦點。論壇的寫法有 `OnFocus`／`OnLoseFocus`／`OnInteract`，並提到一個常見的錯：目標換了也要發 `OnLoseFocus`，不能只在射線沒打中時發（<https://discussions.unity.com/t/raycasting-for-interactables/920097>）。沒找到 Unity Learn 上講這個模式的官方頁（未確認有沒有）。
  - XRI（3.x）：分成三個角色。Interactor 每幀列出有效目標，依優先順序排（預設近的先），可以掛 target filter 改順序。Interactable 可以否決（`IsHoverableBy`／`IsSelectableBy`）。`XRInteractionManager` 是中間人，真正改狀態的是它。另有 interaction layer，和物理 layer 分開，兩邊要有共同的一層才能交互（<https://docs.unity3d.com/Packages/com.unity.xr.interaction.toolkit@3.0/manual/architecture.html>、<https://docs.unity3d.com/Packages/com.unity.xr.interaction.toolkit@3.0/manual/interaction-layers.html>）。
- **B. 提示**：
  - 慣例：跟著焦點事件顯示或收起。
  - XRI：hover（意圖，通常只換外觀）→ select（輸入確認）→ activate（對已選的再做一次）。manager 每幀先結束失效的狀態，再進入新的，事件依 `…Entering` → `…Entered` 的順序發（同上 architecture 頁；API `XRBaseInteractable` 的 `hoverEntered`、`selectEntered`、`activated`）。
- **C. 按鍵 → 動作**：Input System 的 action 把「意思」和「實體按鍵」分開；action 上可掛 Press、Hold、Tap 等 interaction（Hold 要按滿 `duration` 才 Performed，提早放開就 Canceled）（<https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/Interactions.html>）。預設的 action 資產裡有沒有 `Interact` 未確認。
- **D. 多人**：
  - Netcode for GameObjects：`[Rpc(SendTo.Server)]` 的 RPC，參數用 `NetworkObjectReference` 指目標，伺服器用 `TryGet` 取回；對方已經被 despawn 時取不到（<https://docs.unity3d.com/Packages/com.unity.netcode.gameobjects@2.7/manual/advanced-topics/message-system/rpc.html>、<https://mp-docs.dl.it.unity3d.com/netcode/2.3.2/advanced-topics/serialization/networkobject-serialization/>）。`NetworkVariable` 預設只有伺服器能寫（<https://docs.unity3d.com/Packages/com.unity.netcode.gameobjects@2.8/manual/basics/networkvariable.html>）。伺服器要做哪些驗證（距離、視線），文件沒有寫，是常見做法（未在官方文件確認）。
  - Netcode for Entities：輸入是 `IInputComponentData`，烘成 `ICommandData` 緩衝，每 tick 送，並附最近 3 筆當冗餘；一次性的動作用 `InputEvent`（`Set()`／`IsSet`），掉包也保證伺服器恰好收到一次（「交互」這種按鍵適合用它，是我們的推論，文件沒有點名交互）（<https://docs.unity3d.com/Packages/com.unity.netcode@1.4/manual/command-stream.html>）。另有 `IRpcCommand`，文件沒有比較兩者在玩法上的用途（<https://docs.unity3d.com/Packages/com.unity.netcode@1.4/manual/rpcs.html>）。

**Unreal（Lyra 範例，僅供對照）**：

- 目標實作 `IInteractableTarget`，在 `GatherInteractionOptions` 裡加 `FInteractionOption`。
- `AbilityTask_WaitForInteractableTargets_SingleLineTrace` 用計時器反覆打線，直到打到實作這個介面的 actor。
- 按鍵觸發 `ULyraGameplayAbility_Interact`，再依選項的 `InteractionEventTag` 發 Gameplay Event 給對應的 ability。
- `AbilityTask_GrantNearbyInteraction` 先把附近的交互 ability 授權給玩家，讓它來得及複製到 Client。

這些全部在範例專案裡，不在引擎核心（<https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-interaction-system-in-unreal-engine>）。權威細節與提示 UI 那一頁沒寫（未確認）。

### 1.5 四家比較

| | Godot | Bevy | Unity（慣例／XRI） | Unreal Lyra |
|---|---|---|---|---|
| 「可交互」在哪一層 | 腳本約定（group、`has_method`） | 作者元件＋observer；picking 只管「指著誰」 | 慣例：作者的介面；XRI：官方套件，不在本體 | 範例專案的介面 `IInteractableTarget` |
| A. 找目標 | `RayCast3D` 節點每 physics frame 打一次；`Area3D` 進出 signal；layer／mask | backend → `PointerHits` → `HoverMap`；或直接 `MeshRayCast` | `Physics.Raycast`＋layerMask；XRI：Interactor 列有效目標、manager 決定 | 計時器反覆 line trace |
| B. 提示 | 作者的 HUD | `PointerOver`／`PointerOut` observer | 焦點事件；XRI 的 hover | 範例的 indicator（細節未確認） |
| C. 按鍵 → 動作 | `InputMap` action＋直接呼叫目標方法 | 作者讀按鍵；picking 的 Press／Click 會冒泡 | Input System action（Press／Hold）；XRI 的 select | Gameplay Ability＋Gameplay Event tag |
| D. 多人誰判定 | 伺服器（`rpc_id` 送請求、`get_remote_sender_id` 驗、Synchronizer 同步） | 核心沒有；replicon 用 mapped client event，lightyear 用每 tick 輸入 | NGO：RPC 帶 `NetworkObjectReference`，伺服器寫；NfE：每 tick 輸入的 `InputEvent` | ability 在伺服器執行（細節未確認） |
| 目標怎麼上 wire | 節點路徑 | 實體對照（Client→伺服器） | `NetworkObjectReference`；NfE 不送目標 | 未確認 |

共同點：**找目標的機制（射線、重疊）在引擎裡；「什麼可以交互、提示寫什麼、交互做什麼」都在引擎外。**多人時兩派：送請求加目標（Godot、NGO、replicon），或只在每 tick 的輸入裡送「按了」（NfE、lightyear）。

### 1.6 對照 gyo2 現有的機制

| 步驟 | gyo2 已有的 | 夠不夠 | 缺的、放哪 |
|---|---|---|---|
| A. 找目標（射線） | `Physics3d::CastRay(colliders, ray, maxT, filter)`（`engine/modules/physics_3d/include/engine/physics_3d/SpatialQuery.hpp:106-108`）：作者自己的 `Query` 可以帶 `With<Interactable>` 縮範圍，`filter` 是 `bool(EntityId)`（`SpatialQuery.hpp:1-18`）；結果 `EntityRayHit{entity, hit}`（`SpatialQuery.hpp:45-48`）。PvP 的射擊已經用同樣的方式從視線打射線（`games/object_fps_pvp/src/world/PvpWorld.cpp:416` 的 `ShotRay`） | 夠 | — |
| A. 找目標（靠近） | `Physics3d::Intersections(colliders, shape, pose, filter)`（`SpatialQuery.hpp:171-183`） | 夠（每 tick 問一次）。沒有 `Area3D` 那種進出事件 | 進出事件要的話，作者把這次與上次的集合比一下就有；不必放 Engine |
| A. 篩選 | 沒有 collision layer；用 `Query` 的條件（`With<…>`）與 `filter` | 夠。avian 也是 layer＋predicate 兩種都有，gyo2 只留 predicate（和 physics_3d 的決定一致） | — |
| A. 門是子物體 | 空間查詢的 `Query` 必須帶 `Without<Kernel::ChildOf>`，經 `ChildOf` 合成姿態是之後的批（`SpatialQuery.hpp:10-13`） | **不夠**：門掛在建築物底下、整棟一起移動時不能當 collider | **Engine（physics_3d）**，已排在 physics_3d 的後續批；門不動的話可以先不掛父物體 |
| A. 體素格本身可交互（拉桿方塊） | `CastRay` 對 `BoxSet` 只回 `t` 與法線，沒有方塊編號（`Queries.hpp:36-39`；`BoxContact` 才有 `box`，`Queries.hpp:52-55`） | 大致夠：作者用 `origin + t·dir − ε·normal` 算出格子座標 | 真的需要編號時，再在 physics_3d 加「射線對 BoxSet 回方塊編號」（Engine，小） |
| B. 提示文字 | `ui` 的綁定表 `UiBindingTable = unordered_map<string, UiBindingValue>`（`engine/modules/ui/include/engine/ui/UiTypes.hpp:128-129`），PvP 已用 `bindings.Set("title", …)`（`games/object_fps_pvp/src/client/SoloScene.cpp:125`）；準心已有（`ClientWorld.cpp:102` `kCrosshair`） | 夠（固定在畫面上的提示） | — |
| B. 提示跟著物體 | `Camera::WorldToViewport`（`engine/modules/camera/include/engine/camera/Camera.hpp:174-178`）；`ViewportToWorld`（`Camera.hpp:180`）可從畫面中心反推射線 | 夠 | — |
| B. 多語系 | Engine 沒有任何本地化機制（搜 `locali`／`i18n` 沒有結果）；PvP 的字串直接寫在程式裡（`SoloScene.cpp:125`） | 不夠（台本也會碰到，見第 2.6 節第 6 點） | 先不放 Engine；跟台本一起決定（第 5 節 Q4） |
| C. 按鍵 | `input` Module：`KeyboardInput`、`ButtonInput` 式的狀態（`engine/modules/input/include/engine/input/Input.hpp`）；決定遊戲的 System 讀作者每 tick 的輸入，不讀這些（同檔開頭註解 IN-5） | 夠 | **action map／改鍵**：Godot 放核心（`InputMap`），Bevy 放核心外（leafwing、BEI）。gyo2 現在由作者把按鍵翻成自己的 `PlayerInput`（按鍵在 `games/object_fps_pvp/src/client/ClientWorld.cpp:480-488`，組成一步的輸入在 `src/present/Controls.cpp:48-58`）。先留在遊戲層；第二個產品也要時再考慮做成 Module |
| C. 動作 | Kernel 的 `MessageWriter`／`MessageReader`（`engine/ecs/kernel/include/engine/kernel/Params.hpp:484`、`:516`）與推送式 `Trigger`＋observer（`Params.hpp:589`；Bevy 的 `Event`／`Observer`，HANDOFF `v8-arch-gyo2-events-two-kinds-like-bevy`） | 夠：「交互了」可以是作者的 Message 或 Event，門的 observer 收到就改自己的元件 | — |
| C. 門的狀態 | 元件（作者的 `DoorState`）；`state_machine` 是整個 World 一份的狀態（Resource，`engine/modules/state_machine/include/engine/state_machine/StateMachine.hpp:1-11`，同 Bevy 的 `States`，`crates/bevy_state/src/state/states.rs:64`），不是每個實體一份 | 夠（門用 enum 元件就好）；`state_machine` 不適合拿來做每扇門、每個 NPC 的狀態 | — |
| D. 多人 | 見 1.7 | 夠（用每 tick 的輸入） | 可靠的「Client → Match 的離散請求」通道現在沒有（見 1.7） |

**小結**：門的例子用現有機制就做得出來，Engine 不必新增 Module。「可交互」「提示文字」「距離」「按哪個鍵」「誰能開」都是規則，放遊戲層。四家裡也沒有一家把「Interactable」做進引擎核心（Godot、Bevy 沒有；Unity 只有 XR 套件有，見 1.4；Unreal 在 Lyra 範例裡，不在引擎）。

### 1.7 多人：input 加「交互＋目標」對網路格式的影響

**現況**：

- 每 tick 的輸入型別、它的 codec 與版本都是**遊戲自己的**：`PlayerInput`（`games/object_fps_pvp/include/object_fps_pvp/world/Components.hpp:106-121`），codec 在 `PvpWorld.cpp:187-213`（`PvpWorld.cpp:206` 的 `2` 是這個 codec 的版本）。
- Engine 的 net 只提供泛型 `InputOptions<T>{codec, redundancy, window, held}`（`engine/modules/net/include/engine/net/Replication.hpp:515-525`）；`held` 在缺一格輸入時清掉一次性的按鍵（C-IN3），PvP 用它清 `jump`、`fire`、`reload`（`PvpWorld.cpp:208-212`）。
- 伺服器配的實體在 Client 上用同一個 `EntityId`，元件裡的 `EntityId` 原樣上 wire、不用對照表（`docs/architecture/target/v2/net.zh-Hant.md` NT-9）。所以 gyo2 不需要 bevy_replicon `add_mapped_client_event` 那一步。
- Client → Match 只有每 tick 的輸入；Match → Client 有 `ServerMessage<M>`（可靠、有序，`Replication.hpp:7`）。**沒有** Client → Match 的可靠離散訊息（bevy_replicon 的 `client_trigger` 那種）。

**兩種做法**：

| | (a) 只送「按了交互」 | (b) 送「按了交互＋目標 `EntityId`」 |
|---|---|---|
| 輸入加什麼 | `std::uint8_t interact`（一次性，加進 `held` 清除的欄位） | `interact`＋`std::uint64_t target`（8 bytes；`redundancy` 5 → 每則最多多 45 bytes） |
| Match 怎麼判 | 用同一 tick 的身體姿態自己打射線（和 `JudgeShots` 一樣，用 `ShotRay` 的視線，`PvpWorld.cpp:416`、`:439`） | 驗證 Client 指的目標：存在、可交互、距離與視線（Match 仍要打一次射線或做距離檢查） |
| 優點 | 和射擊同一套；Client 不能指定 Match 看不到的東西；wire 最小 | 目標有歧義時（多個重疊的可交互物、清單選擇）用 Client 看到的那一個；提示與結果一致 |
| 缺點 | Client 與 Match 的姿態差一點就會指到不同的東西（提示寫「開門」，結果打到旁邊的箱子） | 要寫驗證；Client 可以送任意 id（Match 只信驗證過的） |
| 成熟方案 | Unity Netcode for Entities 的輸入元件、lightyear 的輸入（每 tick 送「做了什麼」）（見 1.4） | Unity Netcode for GameObjects 的 RPC 帶 `NetworkObjectReference`、Godot 的 `rpc_id`、bevy_replicon 的 mapped client event（見 1.2～1.4） |

**對網路格式的影響**：

- **Engine 的 wire 格式不變**：加的欄位在遊戲的 `PlayerInput` 與它的 codec 裡，`kNetProtocolVersion` 不用動。
- **遊戲的 codec 要升版**：`PvpWorld.cpp:206` 的 `2` → `3`，新舊 Client 不能互連。這是遊戲自己的事。
- 回滾：`interact` 是一次性按鍵，要加進 `held`，否則缺格時會重複觸發（C-IN3）。
- 台本的選項（第 2 節）在多人時也一樣：可以放在輸入裡（`choice` 一個 byte），不需要新通道。只有「低頻、要可靠、內容大」的請求（例：輸入名字、整包交易內容）才會需要 Client → Match 的可靠訊息；那是 **Engine（net）** 的缺口，等有點名的需求再做。

## 2. NPC 台本（對話）

四種都只讀網頁（inkcpp、Yarn Spinner、Godot Dialogue Manager 不在本機），沒有讀源碼。範例是照文件的語法寫的示意，不是從原文複製。

### 2.1 Ink（inkle）＋inkcpp

- **格式**：`.ink` 純文字。knot（`=== shop ===`）、stitch（`= greet`）、選項（`*` 一次性、`+` 可重複）、divert（`-> shop.greet`）、`VAR`、`LIST`、`EXTERNAL`、函式、tag（`#speaker:merchant`）。

  ```ink
  VAR gold = 10
  EXTERNAL playSound(name)
  === shop ===
  Merchant: Welcome. #speaker:merchant
  * {gold >= 5} [Buy a sword]
      ~ gold -= 5
      ~ playSound("coin")
      -> shop
  + [Leave] -> END
  ```

  編譯成中間格式 ink.json（頂層 `inkVersion`、`root`；容器是 JSON 陣列，控制指令是保留字串）。官方 runtime 讀 JSON（<https://github.com/inkle/ink/blob/master/Documentation/ink_JSON_runtime_format.md>、`RunningYourInk.md`）。
- **寫手工具**：
  - Inky：邊寫邊玩的編輯器，後面用的是 inklecate。
  - inklecate：命令列編譯器，`-p` 可以直接玩。
  - inkjs：社群的 JS 移植，Inky 的 web 匯出用它。
  - 出處：<https://github.com/inkle/ink>。
- **條件／變數／指令**：
  - 變數：`VAR`，遊戲用 `variablesState` 讀寫，`ObserveVariable` 收變動。
  - 給遊戲的指令有兩條路：
    - `EXTERNAL` 函式＋`BindExternalFunction`：型別 int、float、bool、string；預設當成有副作用的動作（`lookaheadSafe=false`）；沒綁時跑 ink 裡的 fallback 函式。
    - tag：每行、每個 knot、全域、每個選項都可以有，tag 裡可以放變數。
  - 文件沒說哪一條比較常用（未確認）。
  - 出處：`RunningYourInk.md`。
- **C++**：inkcpp（<https://github.com/JBenda/inkcpp>，社群維護，不是 inkle 的）。
  - 授權與版本：MIT；最新 0.1.12+UEPatch_3，2026-09-05。
  - 支援範圍：README 說「ink 1.1 的全部功能」，用 ink-proof 驗證。官方已到 1.2.1，inkcpp 跟上 1.2 沒有未確認。
  - 格式：runtime 讀自己的二進位格式（`story::from_file("x.bin")`）。`inkcpp_compiler`（依賴 nlohmann JSON）把 ink.json 轉成它；`.ink` 要靠同目錄或 PATH 上的 inklecate，它自己不解析 `.ink`。編譯器不必連進遊戲。
  - runtime 的性質：沒有外部依賴；執行中不配置 heap（「除非緊急」）。
  - 功能：snapshot（存讀檔）、observer、外部函式綁定（樣板產生膠水）。同一個 story 可以開多個 runner，可以共用全域變數。
  - 介面：有 C API（`clib`）、Unreal plugin、Python 綁定。
  - 已知缺口：runtime 還有一些 C 標準庫呼叫。snapshot 的 C++ API 在 README 沒寫（未確認）。
  - 出處：<https://raw.githubusercontent.com/JBenda/inkcpp/master/README.md>、<https://github.com/JBenda/inkcpp/releases>。
- **授權**：ink、Inky 都是 MIT（<https://www.inklestudios.com/ink/>）。
- **成熟度**：最新 v1.2.1，2026-05-05。inkle 自家的 80 Days、Sorcery! 用它；其他作品（Heaven's Vault、Sable 等）用不用 ink 未確認。
- **多語系**：官方沒有方案。ink 團隊在 issue #98 說他們自己沒做過本地化，所以沒有驗證過的做法（<https://github.com/inkle/ink/issues/98>）。社群做法是給每行加 tag 當 ID、另做字串表（<https://johnnemann.medium.com/localizing-ink-with-unity-42a4cf3590f3>）。
- **save**：官方 runtime 用 `story.state.ToJson()`／`LoadJson()`，存的是一整塊 JSON。文件只確定裡面有變數值；位置、拜訪次數、亂數種子、flow 也在裡面是憑記憶（未確認）。inkcpp 用 snapshot。

### 2.2 Yarn Spinner

- **格式**：`.yarn` 純文字。節點有 header（`title:`），`---` 開始、`===` 結束。台詞是 `Merchant: Welcome! #line:a1b2c3`，選項 `->`，還有 `<<if>>`、`<<set>>`、`<<jump>>`、自訂指令 `<<play_sound coin>>`、`{$gold}` 內插、函式。

  ```yarn
  title: Shop
  ---
  Merchant: Welcome! #line:a1b2c3
  -> Buy a sword <<if $gold >= 5>> #line:d4e5f6
      <<set $gold to $gold - 5>>
      <<play_sound coin>>
  -> Leave
  ===
  ```

  編譯成 protobuf 的 `Program`（`name`、`nodes`、`initial_values`、`language_version`）。程式裡只有台詞 ID，文字在字串表（CSV：language、id、text、file、node、line_number、lock、comment）（<https://raw.githubusercontent.com/YarnSpinnerTool/YarnSpinner/main/YarnSpinner/yarn_spinner.proto>、<https://docs.yarnspinner.dev/2.5/using-yarnspinner-with-rust/localisation>）。
- **寫手工具**：
  - VS Code 擴充（免費、開源）：節點圖、在編輯器裡預覽對話。
  - Try Yarn Spinner：線上版。
  - 出處：<https://docs.yarnspinner.dev/next/write-yarn-scripts/writing-yarn-in-vs-code>。
- **條件／變數／指令**：
  - 變數：所有讀寫都經 Variable Storage，可以換成遊戲自己的。
  - 指令：用名字分派給遊戲（字串＋參數），函式要先宣告。
  - 3.3.0 起 VM 改成非同步，函式與指令經 `IDialogueResponder` 處理，可以取消。
  - 出處：<https://docs.yarnspinner.dev/components/variable-storage/variable-storage>、<https://github.com/YarnSpinnerTool/YarnSpinner/releases>。
- **C++**：沒有和引擎無關的 C++ runtime。
  - 官方 Unreal 版：「純 C++」，有自己的 stack VM 與 protobuf 讀取，但用 Unreal 的型別（UFactory、USaveGame、FText）。狀態是 pre-release；授權 YSPL；匯入 `.yarn` 要跑 .NET 的 `ysc`（<https://github.com/YarnSpinnerTool/YarnSpinner-UnrealEngine>）。
  - 自己寫 VM：現行的 `Instruction` 有 23 種（同上 `.proto`）。內建函式（運算子）與 saliency 策略也要重寫，完整清單未確認。
  - Rust 版：`yarnspinner` 0.10.0（MIT OR Apache-2.0）含編譯器，Bevy 是可選的 feature，所以核心不依賴 Bevy；`bevy_yarnspinner` 是 Bevy 的整合。語言版本是 **2.5**，不是 3.x（<https://crates.io/api/v1/crates/yarnspinner>）。
- **授權**：核心、Unity 版 MIT；Godot、Unreal 整合用 YSPL，YSPL 不准把程式碼放進競爭的工具裡。Unity 另有付費版（<https://yarnspinner.dev/license/>、<https://www.yarnspinner.dev/yspl>）。
- **成熟度**：最新 v3.3.0，2026-11-17。出貨作品有 Night in the Woods、A Short Hike、DREDGE、Venba、Lost in Random（repo README）。
- **多語系**：四種裡最完整。每行一個 `#line:` ID（工具能自動加）→ 每種語言一份 CSV → 譯者改 text、comment 欄。語音檔也照 line ID 找（<https://docs.yarnspinner.dev/yarn-spinner-for-unity/assets-and-localization>）。
- **save**：變數在 Variable Storage（遊戲自己的），Unity 文件的例子是把全部變數存成 JSON。「現在跑到哪個節點」要不要存、怎麼存，文件沒寫（未確認）。

### 2.3 Godot Dialogue Manager（Nathan Hoad）

- **格式**：`.dialogue` 純文字。
  - cue：`~ start`。
  - 台詞與回應：台詞 `Character: text`，回應 `- text => cue`。
  - 跳轉：`=>` 跳轉，`=><` 跳過去再回來，`=> END`／`=> END!` 結束。
  - 條件：`if`／`elif`／`else`，回應上 `[if expr /]`，行內 `[if …]…[/if]`。
  - mutation：`$> SomeGlobal.animate(...)`（舊的 `set`／`do` 已不建議用）。
  - 其他：tag `[#mood=happy]`，隨機 `[[a|b]]`、`%`，靜態 ID `[ID:KEY]`。
  - 出處：<https://raw.githubusercontent.com/nathanhoad/godot_dialogue_manager/main/docs/Basic_Dialogue.md>、`docs/Conditions_Mutations.md`。

  ```
  ~ start
  Nathan: [[Hi|Hello]] there.
  if SomeGlobal.has_key
  	Nathan: You have the key.
  - Open the door [if SomeGlobal.has_key /] => open
  - Leave => END
  ~ open
  $> SomeGlobal.open_door()
  Nathan: [ID:DOOR_OPEN] It opens.
  => END
  ```

- **編譯**：匯入時由 GDScript 的編譯器（`DMCompiler.compile_string`）存成 Godot 的 `DialogueResource`（`.tres`）。runtime 是 GDScript，C# 只有薄薄一層包裝（<https://raw.githubusercontent.com/nathanhoad/godot_dialogue_manager/main/addons/dialogue_manager/import_plugin.gd>、`docs/CSharp.md`）。
- **寫手工具**：Godot 編輯器裡的對話編輯器（語法上色、即時檢查錯誤）、遊戲內的除錯分頁。刻意不做節點圖（`docs/FAQ.md`）。
- **條件／變數／指令**：**runtime 本身沒有狀態**，狀態全是遊戲的。名字從 autoload、`DialogueStateContext`、`extra_game_states` 找；mutation 可以呼叫任何暴露出來的物件的任何方法，文件沒提沙盒或白名單（`docs/State.md`）。這靠 Godot 的反射；C++ 沒有反射，要自己做名字到函式的登記表。
- **C++**：做不到。格式沒有獨立於 Godot 的規格，完整語法只由 GDScript 編譯器定義（推論，正式規格未確認），產物是 Godot 的 Resource。
- **授權**：MIT。
- **成熟度**：約 4.0k stars。最新 v4.2.0（「for Godot 4.7」，10/30，年份推測是 2026，未確認）。出貨作品沒有列出。
- **多語系**：走 Godot 的 `tr()`，可以匯出 POT 或 CSV。用 `[ID:KEY]` 時 ID 就是 msgid（`docs/Translations.md`）。
- **save**：沒有自己的 save，遊戲的狀態就是全部。

### 2.4 自訂 JSON 對話圖

- **格式**：自己定。例如 `{"nodes":[{"id":"greet","speaker":"merchant","text":"merchant.greet","next":"menu"}, {"id":"menu","options":[{"text":"merchant.buy","if":{"var":"gold","op":">=","value":5},"do":[{"cmd":"add","var":"gold","value":-5}],"next":"greet"}]}]}`。條件要嘛做成結構（如上），要嘛自己做一個小的運算式語言（等於重做 Ink、Yarn 已有的東西）。
- **寫手工具**：要自己做（§1-7：工具只經資料格式交接）。現成的通用工具，例如 articy:draft（商業）、Twine，能不能匯出合用的 JSON 未確認。
- **C++**：gyo2 已有 `json` Module（`engine/modules/json`）。
- **授權、成熟度**：自己的。成熟度從零開始。
- **多語系**：可以一開始就用「文字鍵 → 字串表」。
- **save**：狀態就是「目前節點＋變數」，可以直接是 ECS 的元件或 Resource，存檔、回滾、複製都走現有機制。

### 2.5 比較表

| | Ink（inkcpp） | Yarn Spinner | Godot Dialogue Manager | 自訂 JSON |
|---|---|---|---|---|
| 格式 | `.ink` → ink.json → inkcpp 的 `.bin` | `.yarn` → protobuf `Program`＋字串表 CSV | `.dialogue` → Godot `.tres` | 自己定 |
| 寫手工具 | Inky（免費），inklecate | VS Code 擴充（節點圖、預覽），線上版 | Godot 編輯器內 | 要自己做 |
| 條件／變數 | 語言內建（`VAR`、`LIST`、運算式） | 語言內建，Variable Storage 可換 | 運算式直接讀遊戲物件 | 要自己定 |
| 給遊戲的指令 | `EXTERNAL` 函式（有型別）、tag | `<<command>>` 用名字分派 | 直接呼叫遊戲方法（靠反射） | 自己的指令表 |
| C++ 能不能用 | **能**：inkcpp（MIT、無依賴、C API） | 沒有和引擎無關的；要自己寫 VM（23 種指令＋protobuf） | **不能** | 能（現有 `json` Module） |
| 授權 | MIT（ink、Inky、inkcpp） | 核心 MIT；Godot、Unreal 版 YSPL | MIT | — |
| 成熟度 | 高（inkle 自家多款）；inkcpp 是社群單一專案、跟到 ink 1.1 | 高（多款出貨）；C++ 這一側不成熟 | 中（Godot 生態常用；出貨作品未列） | 從零開始 |
| 多語系 | 官方沒有，靠 tag 當 ID 的社群做法 | **最完整**（line ID、CSV、語音對應） | Godot `tr()`、POT／CSV | 可以一開始就設計進去 |
| save | 整塊 state／snapshot | 變數歸遊戲；節點位置未確認 | 全歸遊戲 | 節點＋變數＝ECS 資料 |
| 接進 gyo2 時算 | **Plugin**（inkcpp 型別只在 adapter） | VM 自己寫 → **Module**（protobuf 讀取可以自己寫或另接 Plugin） | 不適用 | **Module** 或遊戲層 |

### 2.6 接進 gyo2 時的幾個共同問題

1. **Plugin 還是 Module**：照 gyo2 的用詞，Module 是自己維護、介面是我們的；Plugin 是第三方官方維護、型別只在 adapter（§1-4，例：SDL）。
   - inkcpp：第三方，屬 Plugin。adapter 對外的介面用遊戲的詞：「台詞」「選項」「選第幾個」「指令」「變數」「存讀」。
   - Yarn：要自己寫 VM，屬 Module。我們得跟著上游的語言版本走。
   - 自訂 JSON：Module，或乾脆放遊戲層。
2. **放 Engine 還是遊戲層**：「跑台本、給台詞與選項、收選擇、發指令」是機制；「用哪種台本語言、指令代表什麼、誰能說話」是規則（§1-1）。但只有一種後端時先做共用抽象，是在沒有差異的地方抽象。比較穩的順序：先做一個 Plugin，對外介面用遊戲的詞，給一個遊戲用；出現第二種後端時，再把共同的部分抽成 Engine 的介面。
3. **指令怎麼送到遊戲**：C++ 沒有反射，Godot Dialogue Manager「直接呼叫任何方法」的做法不適用。Ink 的外部函式、Yarn 的指令都可以在 adapter 裡翻成作者的 Kernel `Message` 或 `Event`（Params.hpp:516、:589），遊戲的 System 或 observer 收。
4. **多人**：
   - 台本只在權威端跑：Match 跑，單機時在本地 World 跑。Client 只拿「現在的台詞與選項」（複製的元件，或 `ServerMessage<M>`），選擇放進每 tick 的輸入（見 1.7）。
   - 這樣一來，台本 runtime 不必在各台機器上決定性相同，也不參加回滾，Ink 的亂數、浮點都不會變成同步的問題。
   - 要讓 Client 預測台本的話，四種都要重新評估。只有自訂 JSON 能直接放進回滾。
5. **save 與任務旗標**：
   - Ink：state 是一整塊。旗標要嘛留在 ink 變數裡（遊戲用 observer 鏡像出來），要嘛放遊戲那邊、經外部函式讀。
   - Yarn：Variable Storage 可以直接用遊戲的資料。
   - 自訂 JSON：本來就是遊戲的資料。
   - 不管哪一種，存檔格式都是作者的（`save_version` 只比版本號）。
6. **多語系**：Engine 現在完全沒有。台本選 Ink 的話，要從第一行就用 tag 當 line ID，否則之後補很痛。這件事和交互的提示文字一起決定（第 5 節 Q4）。

## 3. NPC 的周邊（只列，不深入）

| 項目 | 成熟方案 | gyo2 現況 | 缺什麼、放哪 |
|---|---|---|---|
| 行為：state machine | 每個實體一個 enum 元件＋System（Bevy、flecs 的常見寫法）；Godot 的 `AnimationNodeStateMachine`（`scene/animation/animation_node_state_machine.h:109`）只管動畫；Unreal 的 StateTree（Mass 用） | `state_machine` 是整個 World 一份（見 1.6），不是每個實體一份 | 每個 NPC 一份的 FSM：作者用 enum 元件就能做，不必放 Engine |
| 行為：行為樹／Utility AI | BehaviorTree.CPP（C++17，MIT；最新版號各來源不一致，4.9～4.11，未確認；編輯器 Groot2 另有 PRO 授權）[^bt]；Godot 核心沒有（常用外掛 LimboAI，未確認）；Bevy 核心沒有（生態有 big-brain、bevy_behave，未確認）；Unreal 內建 Behavior Tree | 沒有 | 第三方行為樹 → **Plugin**（型別只在 adapter）；或作者自己寫。等有 NPC 需求再選 |
| 尋路：navmesh | Recast & Detour（zlib）[^recast]：Recast 先把幾何體素化成 heightfield 再生成 navmesh；Godot 的 navigation 用它（`thirdparty/README.md:1014-1018`；`modules/navigation_3d/3d/nav_mesh_generator_3d.cpp:439`、`:457`），`NavigationAgent3D::set_target_position`／`get_next_path_position`（`scene/3d/navigation/navigation_agent_3d.h:206`、`:229`） | 沒有。`voxel_map` 只讀、驗證、展開工具烘焙的面、顏色、方塊（`engine/modules/voxel_map/include/engine/voxel_map/VoxelMap.hpp:1-9`），沒有「能走」的資料 | Recast → **Plugin**；navmesh 由工具烘焙、用資料交接（§1-7） |
| 尋路：格子 A* | voxel 地圖本來就是格子，格子 A* 最直接；Godot 的 `AStar3D`、`AStarGrid2D`（`core/math/a_star.h:41`、`core/math/a_star_grid_2d.h:37`） | 沒有。移動有 `MoveAndSlide`（physics_3d） | 格子 A* 演算法小，可以是 Engine 的純函式（像 physics_3d 的純查詢）或遊戲層；「哪些格子能走、跳多高」是規則，放作者 |
| 任務旗標 | Ink、Yarn 的變數（第 2 節）；Unreal 的 GameplayTags；多數引擎沒有通用的「任務」系統 | 沒有。存檔格式是作者的（`save_version` 只比一個數字，`engine/modules/save_version/include/engine/save_version/SaveVersion.hpp:1-16`）；PvP 的存檔是 `PlayerSave`（`games/object_fps_pvp/include/object_fps_pvp/match/PlayerSave.hpp:25-31`） | 遊戲層：旗標是作者的資料（元件或 Resource），和台本的變數接在一起（第 2.6 節第 5 點） |

[^bt]: BehaviorTree.CPP 授權與版本：<https://index.ros.org/p/behaviortree_cpp>、<https://github.com/BehaviorTree/Groot2>（只讀網頁搜尋結果，未讀源碼）。
[^recast]: <https://github.com/recastnavigation/recastnavigation>；Godot 內附的是 1.6.0（`thirdparty/README.md:1017`）。

## 4. 推薦與理由

### 4.1 交互：Engine 不加 Module，先在遊戲層做；多人用 (a)

- **推薦**：「可交互」元件、找焦點的 System、提示 UI、按鍵、交互的效果全部放遊戲層，用現有的 `CastRay`、`Intersections`、`WorldToViewport`、ui 綁定、Kernel 的 Message／Event。多人先用 1.7 的 (a)：輸入只加一個一次性的 `interact`，Match 用同一 tick 的視線自己判。
- **理由**：
  1. 四家都把「找目標」放引擎、「什麼能交互、做什麼」放引擎外（1.5）。gyo2 的找目標機制已經齊了（1.6）。
  2. (a) 和 PvP 現在的射擊是同一套（`ShotRay`＋`JudgeShots`），沒有新的信任面。Engine 的 wire 不動，只升遊戲自己的 codec 版本。Unity Netcode for Entities 與 lightyear 也是這一派。
  3. (b) 的好處（歧義時照 Client 看到的）要等真的出現「多個重疊的可交互物」才有感覺；到時候在同一個輸入加 `target` 就好，不用改 Engine。
- **Engine 的缺口**：照 `v8-arch-gyo2-solve-real-problems` 等點名再做，不先排。
  - `ChildOf` 下的 collider：已排在 physics_3d 後續批。
  - `CastRay` 對 `BoxSet` 回方塊編號：小。
  - Client → Match 的可靠離散訊息：net。
  - action map／改鍵：Module 候選。

### 4.2 台本：Ink＋inkcpp，當 Plugin 接；介面用遊戲的詞；多語系從第一行就用 line ID

- **推薦**：
  - 用 Ink 寫，inklecate＋inkcpp 編譯器在工具端產 `.bin`，遊戲端連 inkcpp runtime。
  - adapter 放 Plugin，對外只露「台詞（文字、說話者、tag）」「選項」「選第幾個」「外部函式 → 作者的 Message／Event」「存／讀」。
  - 台本只在權威端跑（2.6 第 4 點）。
  - 第一批先只給一個遊戲用，不做多後端抽象。
- **理由**：
  1. 四種裡只有 Ink 同時滿足「成熟的語言與免費寫手工具」「和引擎無關的 C++ runtime」「MIT」。
  2. inkcpp 的性質合 gyo2：無外部依賴、執行中不配置 heap、有 snapshot 可存讀、有 C API。
  3. Yarn 的多語系最好，但 C++ 這一側要自己寫並維護 VM、跟上游語言版本，Rust 核心也還停在 2.5。Godot Dialogue Manager 離不開 Godot。自訂 JSON 什麼都要自己做（尤其是寫手工具），違反「先參考成熟方案」。
- **風險**：
  - inkcpp 是社群的單一專案，跟到 ink 1.1，不是 1.2。
  - Ink 官方沒有多語系，要靠「每行 tag 當 ID」的約定，這個約定要在寫第一句台詞前定。
  - 寫手工具 inklecate 要不要 .NET 執行環境未確認。
- **下一步**（使用者同意方向後）：
  - 做一個 spike：在單機場景跑一段有選項、有外部函式、有存讀的 Ink。
  - 要 clone inkcpp 的源碼（<https://github.com/JBenda/inkcpp>，大小未確認），照規則先經主會話問使用者同意。

### 4.3 NPC 周邊：現在不做

- 行為用 enum 元件，有需要再接行為樹（BehaviorTree.CPP 一類，Plugin）。
- 尋路先看地圖形狀：voxel 格子用格子 A*，任意幾何才上 Recast（Plugin，navmesh 由工具烘焙）。
- 任務旗標是遊戲的資料，和台本變數一起設計。
- 這三項都等有具體的 NPC 需求再開卡。

## 5. 需要使用者決定的點

| # | 問題 | 選項 | 取捨 | 推薦 |
|---|---|---|---|---|
| Q1 | 交互在多人時送什麼 | (a) 只送「按了」，Match 自己打射線／(b) 送「按了＋目標 id」，Match 驗證／(c) 兩個都放 | (a) 最小、和射擊同一套，但姿態差一點會指到別的東西；(b) 照 Client 看到的，但要寫驗證；(c) 多 8 bytes，Match 先驗 target，失敗再自己找 | (a)（1.7、4.1） |
| Q2 | 交互與台本先在哪個遊戲試 | `object_fps_pvp`（多人，能驗 Match 判定）／新的單機範例（台本自然，不碰 PvP 的輸入版本）／兩邊各一小段 | PvP：直接驗多人，但 PvP 沒有 NPC，台本會很突兀；單機：台本自然，但多人路徑沒驗到 | 交互放 PvP（一扇門）、台本放單機 spike |
| Q3 | 台本用哪一種 | Ink＋inkcpp／Yarn Spinner（自寫 C++ VM）／自訂 JSON／Godot Dialogue Manager | 見 2.5：Ink 的 C++ 現成但多語系弱；Yarn 多語系強但 C++ 要自己維護；JSON 全控但工具全自己做；Godot DM 用不了 | Ink＋inkcpp（4.2） |
| Q4 | 多語系什麼時候做 | 現在就定（台詞 line ID＋字串表，UI 提示也走同一張表）／之後再補 | 現在定：多一條寫作約定與一張表；之後補：每句台詞都要回頭加 ID。這一題會影響 Q3（多語系很重要的話，Yarn 的分數會上升） | 現在先定 line ID 的約定，字串表等第二種語言再做 |
| Q5 | 台本放哪一層 | Plugin＋遊戲詞的介面，只給一個遊戲用／Engine 的台本 Module＋多後端抽象／完全放遊戲層 | Plugin：照 §1-4，不先抽象；Engine Module：作者可以換後端，但只有一種後端時是在沒有差異處抽象；遊戲層：最快，但第二個遊戲要重接 | Plugin（2.6 第 2 點） |
| Q6 | Engine 的四個缺口要不要先排 | 等點名再做／先排其中幾個 | 先排：保守的機制選項以後沒人改（`v8-arch-gyo2-mechanism-not-conservative`）；等點名：不做沒人用的東西 | 等點名；`ChildOf` 下的 collider 本來就排在 physics_3d 後續批 |

# 交互第一批：按 E 開關的門、交互的骨架、Map 與 Scene 分開（`v8-arch-gyo2-interact-door`）

任務卡，2026-11-27。這一階段只做研究與事前宣告，交使用者核准；**#367（模式配置）合併之前不寫程式**（#367 正在改同一個遊戲的檔案）。作者 Opus 5.5／high。

**修訂 1（2026-11-27）**：使用者把 D1 改了方向（第 10 節，HANDOFF `v8-arch-gyo2-map-scene-split`）：門放在 Scene（裝修）資料裡、狀態放在門的實體上；遊戲流程畫面的 `Scene` 改名。這一版照新方向重寫第 0、1.3、3～9 節；第 2 節（交互的骨架）與 D2 不變，D2 標「待定」。修訂前的版本是 f9c1e991。

出處：研究 #361（`interaction-dialogue-research.zh-Hant.md` 第 1 節、第 4.1 節、第 6 節的 Q1～Q6）；模式配置宣告 #367（`mode-config-declaration.zh-Hant.md`，以下簡稱「#367 宣告」，讀的是 `claude/gyo2-mode-config-declaration` 1baf9122）。程式的行號：Engine 照 `gyo2` 85df84da；遊戲照 #367 的分支（合併後才是 `gyo2` 的樣子）。參考實作照 `~/Code/GitHome` 的本地源碼，路徑從各 repo 的根算起。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| Q1 (a)：多人時輸入只送「按了交互」，Match 用同一 tick 的視線自己打射線找目標 | #361 第 6 節 |
| Q2：交互放 `object_fps_pvp`，先做一扇門 | #361 第 6 節 |
| Q6：Engine 的四個缺口（`ChildOf` 下的 collider、`BoxSet` 射線的方塊編號、Client → Match 的可靠離散訊息、action map）等點名再做；這張卡要用到就停下回報 | #361 第 6 節；卡片 |
| 路線 (A)＋(R1)：新功能加在 Object_PvP，由模式配置決定這一場掛哪些系統；玩法組在每個模式都固定登記，component 只在組開著時放到實體上；「核心 PvP」不開，結果逐位元一致 | #367 宣告第 0.1、2.3、4、5 節 |
| **Map 和 Scene 分開**：Map＝毛坯房（地形、牆、方塊，烘焙過、靜態）；Scene＝裝修（門、箱子、NPC 位置、可撿的東西等會互動、有狀態的）。Scene 是另一個配置檔，地位和地圖檔一樣；arena＝選哪張 map＋哪個 scene | 使用者 2026-11-27（第 10 節） |
| **門的開關狀態放在門的實體上**（不用 `DoorStates` 表） | 同上 |
| **「Scene」保留給裝修資料**；程式裡指遊戲流程畫面的 `Scene` 改名 | 同上 |
| 「商店 NPC 的對話，和商店內的商品是不同的系統」；「遊戲規則不綁定地圖」 | 使用者（後者另見 #367 宣告 11.7 U3） |
| arena 檔的格式只有一個寫者：`v8-arch-gyo2-arena-rules-split`（之後交模式配置卡）。門線不改 arena 的格式 | 主會話轉交（第 10 節） |
| Engine 只給機制；Kernel 的職責是 ECS；錯誤用 `Result` 只告知；不對誤用加守衛；ECS 相關以 Bevy 為主參考；改名排前面 | AGENTS §1、§3；批次表〈每批的設計審查〉第 5 項 |

### 0.2 結論（一段話）

1. **Engine 要加一個機制：兩端都已放好的實體（net 的 `Placed`）。**門是兩端各自從 scene 放好的實體，狀態 `DoorState` 在門上、由 Match 複製過去。GYO 的 net 現在只會複製伺服器配號的實體（第一次送是「spawn」，Client 有同號的實體就拒絕或計成異常，看不見了就刪掉），所以要補「附上」與「卸下」兩種 structure 操作。這是 Engine 的公開介面與 wire，屬上段（第 4 節）。它不是 Q6 的四個缺口之一，是使用者的方向帶出來的。
2. **Scene 檔是遊戲層的資料契約**（第 5 節）：`assets/object_fps_pvp/scenes/*.json`，每個物件有穩定的 `id`、屬於哪個玩法組、該組自己的資料。物件的 `EntityId`＝scene 編號範圍的起點＋`id`，兩端相同。解析後的值進 `MatchInfo::sceneDigest`，和 `wallsDigest` 同一種比法。
3. **arena 要多一個欄位 `scene`**（第 6 節）：門線只提出需求，由 arena 規則拆分那一批改格式。
4. **交互的骨架不變**（第 2 節）：`interaction` 組（`InteractInput`、`Interactable`、`JudgeInteractions`、`Interacted`）、`doors` 組（scene 的門、`DoorState`、`PoseDoors`、`OperateDoors`）。
5. **第 0 步先改名**（第 7 節）：遊戲流程的 `Scene` → `Screen`，加一個讓舊名不回來的檢查。
6. **核心 PvP 不開這兩組**：沒有 scene 的物件、沒有 `DoorState`、玩家身上沒有 `InteractInput`，權威 digest 每一步逐位元相同（用 #367 的 A/B 腳本證明）。`PlayerInput` 與它的 Codec（版本 2）不動。

---

## 1. 成熟方案

查證等級：**源碼**（附檔名與行號）、**官方文件**、**未確認**。#361 第 1.2～1.5 節已比過的（找目標、提示、按鍵、多人兩派）不重抄。

### 1.1 骨架的形狀：「找目標」在共用處、「做什麼」在各種類

| 家 | 找目標的一方（interactor） | 交互了 → 誰處理 | 距離放哪 |
|---|---|---|---|
| **Bevy**（主參考，`bevy` 6a911653f） | picking 的後端把命中寫成 `PointerHits`、合併成 `HoverMap`（#361 1.3） | `Pointer<E>` 同時是 `EntityEvent` 與 `Message`（`crates/bevy_picking/src/events.rs:170`）：可以用 observer 收，也可以用 `MessageReader`（`crates/bevy_ecs/src/message/message_reader.rs:38`）在 System 裡收；作者的 System 看目標身上有沒有自己的 component | 指標的射線（後端），不在目標上 |
| Unity XRI | Interactor 列有效目標、`XRInteractionManager` 決定（#361 1.4） | Interactable 的 `selectEntered` 等事件 | Interactor 那一側 |
| Godot | `RayCast3D`（`scene/3d/physics/ray_cast_3d.cpp:206-223`），`target_position` 決定長度 | 腳本 `get_collider()` 後呼叫目標的方法（#361 1.2） | 射線節點（玩家那一側） |
| Unreal Lyra | `AbilityTask_WaitForInteractableTargets_SingleLineTrace` 的 trace 長度 | 依 `FInteractionOption` 的 tag 發 Gameplay Event（#361 1.4） | ability task（玩家那一側） |

共同點：「打到誰」只有一處、「打到之後做什麼」各種類自己接；能伸手多遠是 interactor 那一側的規則；種類不是骨架裡的 enum（Bevy 看 component、Godot 看方法或 group、Lyra 看介面與 tag）。

### 1.2 多人：輸入型別可以有好幾種

| 家 | 做法 | 位置 |
|---|---|---|
| Unity Netcode for Entities | 輸入是 `IInputComponentData`（`Runtime/Command/IInputComponentData.cs:20`），一次性按鍵用 `InputEvent`（`:34-56`）；每種輸入元件各自一條 command 緩衝 | 源碼（`com.unity.netcode`） |
| lightyear | 泛型 `InputPlugin<A>`（`crates/inputs/inputs_native/src/plugin.rs:15`），每種 action 型別登記一個 | 源碼（`lightyear` 736e1695） |
| GYO net | `NetRegistry::inputs` 是清單（`engine/modules/net/include/engine/net/Replication.hpp:153-158`），每種各自的 `PeerInput<T>`（`:428`）與 `ApplyInput<T>`（`:543-544`），只寫進受控實體身上有 `T` 的（`:553`）；每種輸入每幀各送一則（`engine/modules/net/src/Replication.cpp:1179-1182`） | 源碼 |

一個 World 同時登記兩種輸入，GYO 現在沒有測試（`tests/common/net/InputTests.cpp:66`、`RollbackClientTests.cpp:239` 都只有一種）。第 1 步先補（只加測試）。

### 1.3 兩端都已放好的實體：網路怎麼認得「同一扇門」

| 家 | 怎麼認 | 誰給這個機制 | 看不見時 | 查證 |
|---|---|---|---|---|
| **bevy_replicon**（Bevy 生態，`bevy_replicon` d80c849） | `Signature` component：**兩端都插**，插入時算一個決定性的 hash（可以從 component、作者給的值，或直接給 hash）；Client 收到複製時用 hash 找已經生成的實體，找到就接上，否則照常 spawn（`src/shared/replication/signature.rs:12-35`） | 網路套件（Engine 那一側） | 照一般實體（未逐行確認） | 源碼 |
| lightyear | `PreSpawned{hash, user_salt}`：預設 hash＝生成的 tick＋archetype，也可以自己給；必須在 `FixedMain` 裡生成（`crates/replication/replication/src/prespawn.rs:185-215`） | 網路套件 | 未確認 | 源碼 |
| Unity Netcode for Entities | **pre-spawned ghost**：場景（subscene）裡放好的物件兩端都有，烘焙時照 subscene 的 hash（`SubSceneGhostComponentHash`）與排過序的 `PreSpawnedGhostIndex` 決定性地配 ghost id（`Runtime/Snapshot/Prespawn/PrespawnComponents.cs:9-33`） | netcode（Engine 那一側） | 未確認 | 源碼 |
| Godot | 不經 `MultiplayerSpawner` 的節點，兩端的場景裡都有，`MultiplayerSynchronizer` 照節點路徑同步（`modules/multiplayer/multiplayer_synchronizer.h:55` `root_path`；`scene_replication_interface.cpp:224-240`：只有 spawner 生成的才在 `:240` 拿 net id） | 引擎的多人模組 | 未確認 | 源碼；路徑怎麼上 wire 沒逐行讀 |
| **GYO net 現在** | 只有伺服器配號的實體走複製（NT-9：兩端同號，Client 用 `SpawnWithId`）。伺服器第一次送某個實體是 spawn（`Replication.cpp:381`）：沒有 rollback 的 Client，同號的實體活著就是 `SpawnRejected`（`:728`；G-5 要求它是 0）；有 rollback 的 Client 交給 inbox 的 `Entered`，活著只計數 `entriesWhileAlive`（`:655-665`）。Peer 看不見的實體，伺服器送 despawn（`:363-371`），Client 刪掉它（`:640-651`） | — | **刪掉** | 源碼 |

**共同點**：四家都由**網路那一側**提供「兩端各自生成、網路認得是同一個」的機制；作者只負責「兩端生成同樣的東西、給同樣的身分」。

**GYO 和它們的差別**：Bevy 的 `Entity` 兩端不同，所以 replicon、lightyear 要用 hash 找對應、Unity 要配 ghost id。GYO 的實體編號兩端本來就相同（NT-9，產品分配範圍），**編號本身就能當穩定的身分**：scene 物件的 `EntityId` 由 scene 的 `id` 決定，兩端一樣，不需要 hash 對照表。要補的只是「附上已經存在的實體」與「看不見時不要刪」。

### 1.4 Scene 檔

| 家 | 格式 | 物件的身分 | 和地圖的關係 | 查證 |
|---|---|---|---|---|
| **Bevy**（主參考） | `bevy_scene`：BSN（Bevy Scene Notation），可組合、可覆寫欄位、字串自動變成 asset handle；`.bsn` 檔「之後」才有（`crates/bevy_scene/src/lib.rs:2-80`）。序列化整個 World 的是 `bevy_world_serialization::DynamicWorld{resources, entities: Vec<DynamicEntity{entity, components}>}`（`crates/bevy_world_serialization/src/dynamic_world.rs:25-37`），靠 reflection 存每個 component | 檔裡的 `entity` 是檔內的編號；寫進 World 時用 `EntityHashMap` 換成新的實體（`dynamic_world.rs:92-111`） | scene 可以引用 asset（mesh、圖） | 源碼 |
| Unity | Scene／SubScene＋prefab；subscene 烘焙成 entity 資料 | pre-spawned ghost 的 index（1.3） | 地形、靜態物件與會互動的物件都可以在場景裡；常見的是靜態烘焙、會動的用 prefab | 官方慣例（未逐行查） |
| Godot | `.tscn` 場景＋資源（mesh、地形是資源） | 節點路徑 | 地形是資源，場景擺節點 | 慣例 |

GYO 沒有 reflection，不能像 Bevy 那樣「檔裡寫 component 的型別、照名字還原」。所以 scene 的物件寫成「屬於哪個玩法組＋那個組自己的資料」，由組自己的 reader 解析（和 #367 的 `SetReader` 同一種），Bevy 的 reflection 換成每組一個 reader。

---

## 2. 骨架：`interaction` 組（修訂 1 未改）

**前提：這是作者能處理的。**找目標用 `Physics3d::CastRay`（`engine/modules/physics_3d/include/engine/physics_3d/SpatialQuery.hpp:106-108`），事件用 Kernel 的 `Message`（`engine/ecs/kernel/include/engine/kernel/Params.hpp:484`、`:516`），輸入用 net 的 `Input<T>`。

### 2.1 資料

| 名字 | 種類 | 內容 | 決策資料？ | 複製？ |
|---|---|---|---|---|
| `InteractInput` | component（玩家）；net 的輸入型別（D2，待定） | `std::uint8_t interact`：這一步按了（一次性） | 是（`authority.Component<…>(…, {.input = true})`，照 `PlayerInput`，`PvpWorld.cpp:1171`） | 輸入，Codec 版本 1 |
| `Interactable` | component（marker） | 沒有欄位。種類由同一個實體上的種類 component 表示（門：`DoorState`） | 否 | 否 |
| `InteractionRules` | Resource | `float reach`（公尺），模式檔 `sets.interaction.reach` | 否（進 `rulesDigest`） | 否 |
| `Interacted` | Kernel `Message` | `actor`、`target`、`tick` | 是（`authority.Message`，不進 digest） | 否 |

不照主會話的「`Interactable`（種類、提示字、範圍）」：種類放骨架的 enum 每加一種都要改骨架（1.1）；提示字跟著種類的狀態變、只有 Client 要，由 Client 端各種類給（第 8 節）；範圍是 interactor 那一側的規則、不是地圖的資料，放模式檔。

### 2.2 System

| System | 掛點 | 哪一端 | 做什麼 |
|---|---|---|---|
| `GiveInteractInput` | Main/Update，`SeatPlayers` 之後 | Match（`OnServer`）、Local | 沒有 `InteractInput` 的玩家插一個 |
| `ControlOwnInteract` | Main/Update，`ControlOwnPlayer` 之後 | Client（`OnClient`） | 自己的玩家插 `InteractInput`；自己的 Peer 插 `PeerInput<InteractInput>`（照 `PrepareOwnInput`，`ClientWorld.cpp:576-579`） |
| `WriteStepInteract` | FixedUpdate/First，不在重算時 | Client；Local 另一支直接寫（照 `WriteLocalInput`，`ClientWorld.cpp:631-638`） | 從 `SimControls` 取出、清掉 `interact`，寫進這一步的槽 |
| `JudgeInteractions` | FixedUpdate/PostUpdate，`RespawnPlayers` 之後 | Match（`OnServer`）、Local | 見 2.3 |

全部進 `pvp.interaction.fixed`／`.frame`（#367 宣告 2.4），帶 `pvp.mode.interaction` 條件。

### 2.3 Match 怎麼判（Q1 (a)）

每個這一步按了、活著、比賽已開始（照 `Playable`，`PvpWorld.cpp:400-403`）的玩家，照 EntityId 由小到大：射線＝`ShotRay`（`PvpWorld.cpp:474-477`，和射擊同一條視線）；對所有 collider（`Walls`，`PvpWorld.cpp:268`，門也在裡面）打 `CastRay`，最遠 `reach`；打到的**第一個** collider 有 `Interactable` 就發 `Interacted`，否則不發；地板平面在 `reach` 內擋住時也不發（照 `JudgeShots`，`PvpWorld.cpp:499-506`）。門的狀態在第 n 步的 PostUpdate 改，碰撞形狀在第 n+1 步開頭擺好；兩端同一個順序，Client 重算時結果一樣。

### 2.4 組之間的依賴

`GameplaySet`（`games/object_fps_pvp/include/object_fps_pvp/world/GameplaySets.hpp:46-56`）加 `requires`；`LoadModes` 讀到「開了 `doors` 沒開 `interaction`」就讀取失敗、寫明原因。不採「自動幫它開」：模式檔開了什麼應該照字面看得到，`rulesDigest` 也只 hash 讀到的值。

---

## 3. 第一個種類：`doors` 組

**前提：這是作者能處理的**（Engine 的 `Placed` 由第 4 節補）。

### 3.1 資料放哪（「遊戲規則不綁定地圖」）

| 什麼 | 放哪 | 理由 |
|---|---|---|
| 牆、地形、方塊 | Map（不變） | 毛坯房 |
| 門在哪、多大、往哪邊開、開幾度、顏色；一開始開著還是關著 | **Scene 檔**的物件（第 5 節） | 裝修；換門的位置不必重烘焙地圖 |
| 能伸手多遠 | 模式檔 `sets.interaction.reach` | 規則 |
| 這一場有沒有門 | 模式檔有沒有開 `doors`；scene 有門的物件、模式沒開就不放 | 規則 |
| 上鎖、鑰匙 | 之後（背包） | — |

### 3.2 資料與 System

| 名字 | 種類 | 內容 | 決策資料？ | 複製？ |
|---|---|---|---|---|
| `DoorState` | component（門） | `std::uint8_t open` | 是（`authority.Component<DoorState>`，回滾照預設） | `Replicate`，Codec 版本 1（1 byte） |
| `DoorShape` | component（門） | scene 讀到的形狀：門軸、`yaw`、寬、高、厚、`swing`、顏色 | 否（進 `sceneDigest`） | 否（兩端從 scene 放） |

- **門的實體**：模式開著 `doors` 時，每一端在 `Prepare` 從 scene 放：`EntityId`＝`kSceneEntities` 的起點＋物件 `id`（第 5.3 節）；component：`DoorState`（scene 的初值）、`DoorShape`、`Interactable`、`Physics3d::Collider{Cuboid}`、`Transform`；有網路時另加 net 的 `Placed`（第 4 節）。
- **`PoseDoors`**（FixedUpdate/First，兩端）：照 `DoorState` 擺 `Transform`（關著 `yaw`、開著 `yaw + swing`，繞門軸）。`Transform` 不是決策資料，每步從會回滾的 `DoorState` 算出來。`MovePlayers`、`JudgeShots`、`SpawnFree` 讀的 `Walls` 收所有沒有 `ChildOf` 的 collider，門自動擋走路與子彈，**核心的 System 不用改**。門不掛父物體（不碰 Q6 的 `ChildOf` 缺口）。
- **`OperateDoors`**（FixedUpdate/PostUpdate，`JudgeInteractions` 之後；Match、Local）：讀 `Interacted`，目標有 `DoorState` 的切換它。新姿勢會壓到活著的玩家時不切換（`Physics3d::FindContact` 對站姿膠囊，照 `SpawnFree`，`PvpWorld.cpp:574-588`）。同一步多人按同一扇：照 `Interacted` 的順序各切一次。
- 物理的開關是瞬間的；擺動是 Client 的畫面（第 8 節）。
- **開賽前也看得到門**：開賽前 `ShowPlayers` 只讓 Peer 看到自己的玩家（`PvpWorld.cpp:759-779`）。改成「自己的玩家＋所有 `Placed` 的實體」（裝修不是要藏的資訊）。這是核心 System 的一行改動；核心 PvP 沒有 `Placed` 的實體，行為不變（A/B 證明）。

---

## 4. Engine：兩端都已放好的實體（net 的 `Placed`）

**前提：這是 Engine 要解決的。**「網路認得兩端各自生成的同一個實體」只有網路那一側做得到（1.3 四家都是），作者繞不過去：沒有這個機制，門只能由 Match spawn（修訂前的 (a)，Client 在門到之前會預測穿門），或把狀態放在 Resource（修訂前的 (b)，使用者否決）。**上段**（公開介面、wire、決定性）。

### 4.1 公開介面（`engine/modules/net/include/engine/net/Replication.hpp`）

| 加什麼 | 意思 |
|---|---|
| `struct Placed final {}`（component；`HashValue` 空；W-8 的 `static_assert`） | 「這個實體在每一端用同一個編號、在第一步之前放好」。由產品在兩端放（照 scene）。不是決策資料、不複製 |
| `ReplicaDrop` 不加新理由 | 附上的實體 Client 沒有：`MissingTarget`（已有，`Replication.cpp:730`） |
| `NetReplicaState` 加計數 `attaches`、`detaches` | 給測試與診斷 |

作者的用法只有一件事：兩端都在實體上放 `Placed`。編號範圍由產品定（照 `kArenaEntity`，`Components.hpp:173-175`，在兩個配號範圍之外）；Engine 不檢查範圍（不對誤用加守衛）。

### 4.2 wire：structure 操作多兩種

現在的操作（`Replication.cpp:363-389`、`:640`）：0＝改結構、1＝spawn、2＝despawn。加：

| 旗標 | 伺服器何時送 | Client 怎麼做（沒有 rollback） | Client 怎麼做（有 rollback，K3） |
|---|---|---|---|
| **3＝附上** | Peer 第一次看到一個 `Placed` 的實體（取代 spawn），或再次看到 | 實體要活著、有 `Placed`，否則 `MissingTarget`；照 spawn 的格式寫它所有複製的 component（`ApplyMode::Set`） | 不送 `Entered`（實體從第一步就在）；會回滾的 component 進 inbox、不回滾的直接寫（照 `:700-719` 現有的分法） |
| **4＝卸下** | `Placed` 的實體 Peer 看不見了（取代 despawn） | 拿掉它所有**複製來的** component，實體與本地的 component 留著 | 會回滾的 component 用 `removedToInbox`；不回滾的直接拿掉 |
| 2＝despawn | 伺服器真的刪了 `Placed` 的實體（門被打壞） | 照舊刪 | 照舊 `Exited` |

- `kNetProtocolVersion` 3 → 4（`engine/modules/net/include/engine/net/Handshake.hpp:28`）：旗標的意思變了，舊的 Client 讀到 3、4 會當成 Malformed。所有用 net 的產品一起升；握手比版本，不一致連不上（現有的路）。
- 「看不見時卸下、不刪」和 GYO 現在對一般實體的做法（刪掉，replicon 的 `ScopeLifetime::WhileVisible`，`Replication.hpp:453`）不同：放好的實體屬於兩端的 scene，不是伺服器給的，刪掉之後 Client 沒有辦法自己放回來。別家看不見時怎麼處理放好的實體，沒查到（未確認，第 11 節）。

### 4.3 對 protocol hash、權威 hash、#367「登記固定」的影響

| 項目 | 影響 |
|---|---|
| Engine 的 `kNetProtocolVersion` | 3 → 4，所有產品 |
| 產品的 protocol hash（名字、順序、Codec 版本） | `Placed` 不是複製的 component，**不進** protocol hash。`DoorState` 的 `Replicate` 會進（遊戲那一側，5.4） |
| 權威 digest | 不變：digest 不含實體編號（`engine/modules/authority/include/engine/authority/Registrations.hpp:244-264`），`Placed` 不是決策資料。附上之後 `DoorState` 照一般的格子算 |
| consistency fingerprint | 不變（Engine 沒有新的 System；Placed 的處理在現有的 `ReplicateOut`、`ApplyReplication` 裡） |
| #367 的 (R1) | 一致：`DoorState` 的 `Replicate` 與 authority 登記在**每個模式**都做；scene 物件只在組開著時放到 World，所以「沒開＝沒有格子＝不送、不進 digest」照舊成立。`Placed` 是 Engine 的型別，不分模式 |

### 4.4 測試（`tests/common/net/`，外部可驗）

- 兩端都放好的實體：伺服器改它的值，Client 收到、寫在同一個實體上；`SpawnRejected`、`entriesWhileAlive` 都是 0。
- Client 沒有那個實體：`MissingTarget` 計一次、不建立實體。
- 看不見 → 卸下（實體還在、複製來的 component 拿掉、本地的留著）；再看見 → 附上、值是伺服器現在的。
- 伺服器刪掉 → Client 刪掉。
- K3（有 rollback 的 Client）：附上的值進 inbox、重算後和伺服器一致；卸下走 `removedToInbox`。
- 一般實體的行為不變（既有的 net 測試全過、不改期望值）。
- 突變（預定）：附上當成 spawn、卸下當成 despawn、附上不檢查實體活著、K3 附上時送 `Entered`。

---

## 5. Scene 檔（遊戲層的資料契約）

**前提：這是作者能處理的。**格式與物件的種類都是遊戲的；Engine 只讀 JSON、算 hash。不做 Engine 的 scene Module：GYO 沒有 reflection，物件的內容只有遊戲的玩法組讀得懂；只有一個產品時抽象，是在沒有差異處抽象（第二個產品也要時再議）。

### 5.1 位置與格式

`assets/object_fps_pvp/scenes/<名字>.json`，由 arena 的 `scene` 欄引用（第 6 節）：

```json
{
  "version": 1,
  "id": "pvp_arena",
  "objects": [
    { "id": 1, "set": "doors",
      "data": { "hinge": [6.0, 0.0, 10.0], "yaw": 0.0, "width": 1.0, "height": 2.1, "thickness": 0.1,
                "swing": 1.5707963, "open": false, "color": [0.55, 0.35, 0.2] } }
  ]
}
```

| 欄位 | 意思 | 錯誤（讀取失敗，`error` 寫哪一個物件、哪一欄） |
|---|---|---|
| `id`（scene） | scene 的名字（1～64 字元，照 `sim::kMaxContentIdBytes`） | 缺、太長 |
| `objects[].id` | 物件的**穩定身分**：正整數、scene 內唯一、刪掉的不再重用（工具負責）。決定 `EntityId`（5.3） | 重複、0、超過範圍 |
| `objects[].set` | 屬於哪個玩法組 | 遊戲沒有這一組、或這一組不收 scene 物件 |
| `objects[].data` | 那一組自己的資料，由組的 `readScene` 解析 | 組的 reader 說的 |

- 門的 `data`：`hinge`（關著時門軸那一邊的底角）、`yaw`（關著時的朝向，照 `PlayerBody::yaw`）、`swing`（開的角度，正負決定方向）、`open`（初值）、`color`。錯誤：寬高厚不是正數、`swing` 為 0 或超過 π。
- 和 Bevy 的對應：`objects[]` ＝ `DynamicWorld::entities`；`id` ＝ `DynamicEntity::entity`（檔內的編號）；`set`＋`data` ＝ component 的型別與值（Bevy 靠 reflection，GYO 靠組的 reader）。差別：Bevy 寫進 World 時換新編號（`dynamic_world.rs:92-111`），GYO 照固定公式換（5.3），因為兩端要同號。

### 5.2 遊戲層的膠水

- `GameplaySet` 加 `readScene`（`SetReader` 同型：讀一個物件的 `data`，回傳 digest＋放進 World 的函式）。
- 讀內容（`LoadGameContent`）時讀每個 arena 引用的 scene；scene 讀不到或解析失敗，那個 arena 讀取失敗（照 arena 的 `map` 讀不到的處理）。
- `Prepare` 時，模式開著的組的物件才放（`put`），沒開的照樣解析、不放（同 #367 arena 區塊的做法）。

### 5.3 物件的 `EntityId`

`kSceneEntities`：一段固定的範圍（例 `3'100'000`～`3'199'999`），在 `kMatchIds`（1～1,000,000）、`kClientIds`（1,000,001～2,000,000）之外，也避開現有的固定編號（`3'000'001`～`3'000'032`，`Components.hpp:175`、`ClientWorld.cpp:121-138`、`SceneBuilds.hpp:26-27`）。物件的 `EntityId`＝起點＋`id`；`id` 超過範圍是讀取失敗。兩端用同一份 scene（5.4 比對），所以同號；這就是 1.3 的「編號本身就是穩定的身分」。

### 5.4 進內容比對

照 `wallsDigest`、`arenaDigest`（`Components.hpp:131-164`；#367 宣告 3.3）：

- `GameContent::SceneDigest(mode, arena)`：這一場的 scene 裡、**模式開著的組**的物件（`id`、`set`、`data` 解析後的值；用 Kernel 的 Hasher，不 hash 檔案位元組）。沒開的組的物件不算，改它們不會讓別的模式判成不一致（#367 第 2 輪審查 S1 的同一個理由）。
- `MatchInfo::sceneDigest`（`MatchInfoCodec` 版本加一）；`MismatchKind::Scene`，比對順序 Mode → Rules → Character → Arena → ArenaData → **Scene** → Walls；Client 看到不一致就帶原因回大廳（照 `ArenaData`）。
- 核心 PvP：沒有開著的組 → `sceneDigest` 是空集合的值，兩端相同。

### 5.5 Tool 怎麼產出（AGENTS §1.7）

- Game 只讀 scene 檔；Tool 只寫 scene 檔，兩邊只經這個格式交接。Game 沒有 Tool 也能跑（手寫的 JSON 就能玩）。
- 第一批：手寫（pvp_arena 一扇門、測試用的 scene）。格式寫成資料契約頁 `games/object_fps_pvp/docs/scene-format.zh-Hant.md`（欄位、錯誤、`id` 的規則、版本）。
- 之後的 scene 編輯工具（地圖編輯器照 Goxel；擺物件的工具是另一個決定）：自己持有格式的知識，加一個對齊測試（工具寫出的檔，遊戲讀得進來、digest 和手寫的一樣）。工具負責 `id` 不重用。

---

## 6. 和 arena 的介面（門線不改 arena 的格式）

門線需要 arena 提供：

| 需要 | 例 | 誰改 |
|---|---|---|
| 一個選填的欄位 **`scene`**：這張 arena 用哪個 scene 檔（相對 content 根的路徑，照模式檔 `rules` 的寫法）；沒有＝沒有裝修 | `"scene": "scenes/pvp_arena.json"` | **`v8-arch-gyo2-arena-rules-split`**（arena 格式的唯一寫者）在重定 arena 的形時加 |
| pvp_arena 的值 | 上一列的值 | 門線（只填值，不改格式） |

- 時間：門線的第 4 步（scene）要等 arena 規則拆分合併、而且有 `scene` 欄才動工；那時還沒有的話，停下回報主會話，不自己加。
- #367 的 arena `sets` 區塊（門原本預定放這裡，#367 宣告 6「門」那一列）：門改放 scene 之後，`sets` 要不要留、留給什麼，由 arena 規則拆分那一批判斷。門線不讀它。

---

## 7. 第 0 步：遊戲流程的 `Scene` → `Screen`

### 7.1 新名字

**`Screen`**：「主選單、大廳、房間、遊戲」這些一次換掉整組 World 的畫面。同組的 API 已有 `MenuPage`、`GamePage`（`games/object_fps_pvp/src/client/Pages.hpp:19-22`：一個畫面裡的頁），「一個 Screen 有幾個 Page」讀起來是一層包一層。對照：Bevy 的範例把流程寫成 `States`（`examples/state/states.rs:43` 的 `AppState`），Bevy 官方的遊戲範本把這一層叫 `Screen`（`bevy_new_2d`，本機沒有源碼，未確認）。

| 舊 | 新 |
|---|---|
| `enum class Scene`（`Menu`、`Splash`、`SoloRoom`、`SoloGame`、`Lobby`、`Room`、`MultiGame`） | `enum class Screen`（值不變） |
| `SceneChange`、`SceneRequest`、`SceneDirector`、`ChangeScene`、`SceneChangeWanted`、`SceneName` | `ScreenChange`、`ScreenRequest`、`ScreenDirector`、`ChangeScreen`、`ScreenChangeWanted`、`ScreenName` |
| `PlaceSceneDocument`、`PlaceSceneCamera`、`BuildSceneCommon`、`InSceneUpdate`、`kSceneCamera`、`kSceneRoot`、`ClientScene`、`GameScene`、`firstScene`、`documentScene`、欄位 `scene` | 同樣把 Scene 換成 Screen |
| 檔名 `client/Scenes.hpp`、`Scenes.cpp`、`SceneBuilds.hpp`、`GatewayScene.cpp`、`LobbyScene.cpp`、`RoomScene.cpp`、`SoloScene.cpp` | `Screens.hpp`、`Screens.cpp`、`ScreenBuilds.hpp`、`GatewayScreen.cpp`、`LobbyScreen.cpp`、`RoomScreen.cpp`、`SoloScreen.cpp` |

### 7.2 範圍

- 改：`games/object_fps_pvp/`（`client/`、`client_main.cpp`、`ClientWorld.*`、`SoloSave.hpp`、`CMakeLists.txt`）、`tests/object_fps_pvp/client/ClientTests.cpp`、`games/object_fps_pvp/docs/mutations.json` 的 find／expect、遊戲裡現行的說明文件（`game-flow-declaration`、`gateway-client-handoff` 開頭加一行「`Scene` 已改名 `Screen`」，正文不改寫）、程式註解。
- 不改：dev_log（當時的紀錄）；`engine/modules/model/src/adapters/ufbx/` 的 `SceneDeleter`、`ScenePtr`（ufbx 的 `ufbx_scene`，第三方的詞，只在 adapter 裡）；physics 測試的 `WallScene`、`RayScene`（`tests/common/physics_3d/MoveAndSlideTests.cpp:120`：一組 collider 的測試佈置，意思接近新的 Scene，不是流程畫面）。
- 行為不變：純改名。突變照「純改名只跑 find／expect 受影響的」（D184）。
- 時機：#367 合併之後（同一批 Client 檔），任何程式用到新的 Scene 之前。

### 7.3 守衛（讓舊名不回來）

- `tests/object_fps_pvp/naming/retired_names.py`，ctest `object_fps_pvp.retired_names`（label `cpu`）：掃 `games/object_fps_pvp/` 與 `tests/object_fps_pvp/` 的 C++／CMake，找 7.1 左欄的識別字（`Scene::Menu` 等用 `Scene::` 加值的寫法）與舊檔名；有就失敗，每個命中一行。只掃這張清單，不禁止 `Scene` 這個字（新的 Scene 要用它）。
- 照 `grep-p-macos` 的教訓用 Python，不依賴 BSD grep；附自我測試（有命中的正例、沒命中的反例）；突變：清單少一個名字 → 自我測試抓到。
- 歸屬：產品自己的檢查（Engine 的 `naming.guard` 只管 GYO 底下的 namespace，`tests/common/naming/naming_guard.py` 開頭）。

### 7.4 要架構師改的條文

同詞異義表 `docs/architecture/target/v2/rebuild-vocabulary.zh-Hant.md:154`：「Scene｜不單用｜不用它命名新概念｜……可以生成的實體範本（Bevy 的 Scene）要不要有，另判」。使用者的決定讓 Scene 成為一個新概念（裝修資料），這一列要改（例：「Scene｜**裝修資料**：Map 之上、會互動、有狀態的物件的擺放；另一個配置檔，地位同地圖檔｜遊戲流程的畫面寫 Screen」）。這張表的 Owner 是架構師會話；門線不自己改，交主會話轉。

---

## 8. Client（修訂 1 只改資料來源）

Client 是三個 World（`ClientWorld.hpp:3-19`）：Sim 跑網路與預測，Frame 畫畫面與讀按鍵，經 `ClientExchange` 交換。

- **按鍵**：Frame 的 `ReadControls` 讀 `E`（照 `R` 的 `JustPressed`，`ClientWorld.cpp:554`），`present::StepInput` 加 `interact`；照現有按鍵寫死，不做改鍵（不碰 Q6 的 action map）。模式沒開 `interaction` 時按了沒效果。
- **焦點與提示**：Frame 每幀從自己的眼睛沿視線打射線（牆用 `WorldSnapshot::walls`，`ClientExchange.hpp:46`；門用 scene 的形狀與 Sim 傳來的 `DoorState`），規則同 2.3；焦點是門時顯示「[E] 開ける」／「[E] 閉める」（照門的狀態）。提示由種類各自的 System 寫，HUD 只顯示目前焦點的提示。字串照 HUD 現有的寫法在程式裡（`ClientWorld.cpp:1022-1032`）。
- **門的畫面**：每扇門一個 `Mesh3d`（長方體，和地圖的 region 同一條路在程式裡做），狀態變了就照擺動秒數轉過去。擺動秒數、音量、距離放 Client 讀的表現檔（`audio/sfx.json` 加 `door_open`、`door_close`，版本號加一）。
- **音效**：0 → 1 播 `door_open`、1 → 0 播 `door_close`，用現有的空間化（`ClientWorld.cpp:1091`）。素材自製或 CC0，來源寫進 `assets/object_fps_pvp/LICENSES/SOURCES.zh-Hant.md`。
- `WorldSnapshot` 加門（編號、`DoorState`）；Frame World 從 scene 自己知道門的形狀。

---

## 9. wire、權威 hash、核心 PvP 的證明、測試

### 9.1 wire

| 項目 | 變化 | 核心 PvP 模式 | sandbox 模式 |
|---|---|---|---|
| Engine `kNetProtocolVersion` | 3 → 4（第 4 節） | 變（所有產品） | 同左 |
| `PlayerInput` 與 Codec（版本 2，`PvpWorld.cpp:198-224`） | 不變 | 不變 | 不變 |
| `InteractInput`（D2 (b)，待定；Codec 1） | 新登記 | 不送 | 每 Client 每幀多一則約 60 bytes 的輸入訊息（每格 9 bytes、重送 5 格，加訊息頭） |
| `DoorState`（`Replicate`，Codec 1） | 新登記 | 沒有門、不送 | 附上時送一次、狀態變時送 |
| `MatchInfo`（Codec 版本加一：`sceneDigest`） | 變 | 變（多 8 bytes） | 同左 |
| 產品的 protocol hash | 變（兩個新登記＋`MatchInfo` 的版本；兩端一致，正常的升版） | 變 | 變 |

### 9.2 權威 hash

| 項目 | 核心 PvP | sandbox |
|---|---|---|
| 權威 digest（有值的格子，沒值是 0，`Registrations.hpp:246-264`） | **逐位元相同**：沒有門的實體、玩家身上沒有 `InteractInput`；核心型別的 authority 登記序號不動（新的排在後面） | 多 `InteractInput`、`DoorState` |
| consistency fingerprint | 變（兩個組的 System 與 set；兩端一致）；`DatasetReport` 只在核心之後多列 | 同左 |
| `rulesDigest`、`arenaDigest`、`wallsDigest`、`characterDigest` | 不變 | `rulesDigest` 是 sandbox 自己的 |
| `sceneDigest` | 空集合的值 | 含門 |

### 9.3 A/B（照 #367 宣告第 5 節）

- 用 #367 的 `object_fps_pvp.core_script` 與主 checkout `build/evidence/gyo2-mode-config/ab.py`：改前（這條線分出的那一點）與改後，同一台機器、核心 PvP，**權威 digest 每一步逐位元組相同**；`--report` 的差異只能是 protocol hash（含 Engine 版本、`MatchInfo` 的 Codec）與 `DatasetReport` 在核心之後多出的列。
- macOS（本機）與 Linux 主機；Windows 只跑 L1。結果放主 checkout `build/evidence/gyo2-interact-door/`。
- 第 0 步（改名）之後與最後各跑一次。S5 不重量（不動讀圖與建牆；#367 11.8：程式碼變多造成的擺放效應）。

### 9.4 新的測試（本機＋L1 三平台；network 照檔案原本的標籤）

| 測試 | 驗什麼 |
|---|---|
| net（Engine）：兩種輸入 | 兩種都照步到、各自的 `held` 只清自己的、沒放緩衝的不送 |
| net（Engine）：`Placed` | 4.4 |
| world `[pvp-scene-read]` | scene 的各種錯誤：讀取失敗並寫明哪個物件哪一欄；`id` 決定 `EntityId` |
| world `[pvp-scene-digest]` | 開著的組的物件改一個值 → `sceneDigest` 變；沒開的組的物件改 → 不變；Client 的 scene 不同 → `ContentMismatch{Scene}` |
| world `[pvp-door-toggle]`、`[pvp-door-reach]`、`[pvp-door-wall-between]` | 看著 reach 內的門按 E → 那一步 `DoorState` 切換；太遠、中間有牆 → 不變 |
| world `[pvp-door-blocks]`、`[pvp-door-open-passes]` | 關著擋走路與子彈；開著不擋門洞 |
| world `[pvp-door-body-in-way]`、`[pvp-door-before-start]`、`[pvp-door-dead]` | 壓到人、開賽前、死了：不切換 |
| world `[pvp-door-match-client]` | Match＋2 Client 的記憶體連線：開賽前就看得到門（附上，`SpawnRejected` 0）；A 按 E → 兩個 Client 的門上的 `DoorState` 和 Match 相同；B 的預測被關著的門擋住、門開後能過；沒有 `ContentMismatch` |
| world `[pvp-interact-off]` | 核心 PvP：沒有門的實體（pvp_arena 有 scene 也一樣）、沒有 `InteractInput`、按 E 沒效果、沒有多送的訊息 |
| world `[pvp-door-needs-interaction]` | 開 `doors` 沒開 `interaction`：讀取失敗 |
| client `[pvp-door-prompt]`、`[pvp-door-swing]`、`[pvp-door-sound]` | 提示的字與有無；擺動到目標角度；開／關各播一次 |
| `object_fps_pvp.retired_names` | 7.3 |
| Go `./services/...` | 房間的模式清單多了 sandbox |

### 9.5 相關突變（D184）

預定的方向：reach 的比較反過來、射線不被牆擋、`held` 沒清 `interact`、開賽前／死了也判、`PoseDoors` 不擺、`OperateDoors` 不看擋到人、組沒開也放門的實體、`InteractInput` 在組沒開時也插、`requires` 不查、沒開的組的物件進了 `sceneDigest`、開著的沒進、`ShowPlayers` 開賽前不給 `Placed`、提示字開／關反了；Engine 的見 4.4。

---

## 10. 批次與要使用者決定的點

### 10.1 批次（一條線、一個 PR：這份宣告的 draft PR #375 接著放實作）

```
v8-arch-gyo2-interact-door（一個 PR，開向 gyo2；#367 合併後才動工）
├─ 第 0 步｜Opus/medium｜下段｜遊戲流程的 Scene → Screen、retired_names 檢查；A/B 一次
├─ 第 1 步｜Opus/high  ｜下段｜A/B 改前的輸出；Engine 測試「一個 World 兩種輸入」（只加測試，不過就停下回報）
├─ 第 2 步｜Opus/high  ｜上段｜Engine net：Placed、附上／卸下、kNetProtocolVersion 4、net 測試、net.zh-Hant 條文
├─ 第 3 步｜Opus/high  ｜上段｜interaction 組（第 2 節）、GameplaySet::requires
├─ 第 4 步｜Opus/high  ｜上段｜scene 檔：讀取、readScene、kSceneEntities、sceneDigest、MismatchKind::Scene、資料契約頁（要 arena 的 scene 欄）
├─ 第 5 步｜Opus/high  ｜上段｜doors 組：DoorState、PoseDoors、OperateDoors、ShowPlayers 給 Placed、sandbox 模式、pvp_arena 的 scene
├─ 第 6 步｜Opus/high  ｜下段｜Client：E 鍵、焦點與提示、門的網格與擺動、音效
└─ 第 7 步｜Opus/medium｜下段｜A/B（macOS、Linux）、設計審查、純淨 Code Review、相關突變、dev_log
```

| 步 | 上段／下段（根據） | 實作者 | 審查者 | 檢驗者 |
|---|---|---|---|---|
| 0 | 下段（純改名，行為不變；不是 Engine 的公開介面） | Opus／medium（多檔但照表改，CI 驗） | 併入第 7 步 | Opus／medium |
| 1 | 下段（只加測試與證據） | Opus／high | 併入第 7 步 | Opus／high |
| 2 | **上段**：Engine 的公開介面、wire、決定性（K3） | Opus／high | Opus／high | Opus／high |
| 3 | **上段**：新的輸入型別（protocol hash）、模式檔的新組、新的決策型別 | Opus／high | Opus／high | Opus／high |
| 4 | **上段**：新的資料契約（scene 檔）、`MatchInfo` 的 Codec | Opus／high | Opus／high | Opus／high |
| 5 | **上段**：新的複製 component 的 Codec、權威、核心 System（`ShowPlayers`）的改動 | Opus／high | Opus／high | Opus／high |
| 6 | 下段（Client 的表現，不寫決策資料、不動 wire） | Opus／high（多個 Client 檔） | Opus／high | Opus／high |
| 7 | 下段（執行與紀錄）；檢驗上段的成果，不用小模型 | Opus／medium | — | Opus／medium |

- 設計審查 Opus／high（唯讀）；純淨 Code Review Fable／high（只給 git 範圍；Fable 不能用時 Opus，報告寫明）。模型都不超過主會話（Opus）。
- **先後**：第 0 步在任何用到新 Scene 的程式之前；第 2、3 步互不依賴；第 4 步要 arena 的 `scene` 欄（第 6 節）；第 5 步要第 2、3、4 步。
- **時間**：第 0 步約半天；第 1 步約 2 小時；第 2 步約一天；第 3～6 步各約半天到一天；第 7 步的 Linux A/B 約 30 分鐘（要主機開著）。超過估計 1.5 倍就停下回報。
- **停下回報的條件**：第 1 步的 Engine 測試不過；第 4 步開工時 arena 還沒有 `scene` 欄；實作中發現要用 Q6 的缺口；物理對旋轉的 `Cuboid` 有缺陷（`tests/common/physics_3d/MoveAndSlideTests.cpp:100-104` 有帶旋轉的 helper，有沒有旋轉 `Cuboid` 的案沒逐案確認）。

### 10.2 要使用者決定的點（只列和別家不同、或會推翻已定的）

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **P1** | 放好的實體用什麼當網路身分（4、5.3） | (a) **`EntityId` 本身**：scene 的 `id` 照固定公式換成編號，兩端同號（GYO 的 NT-9 本來就兩端同號）(b) 照 replicon `Signature`／lightyear `PreSpawned`：兩端各算一個 hash，Client 用對照表接上 | **(a)**：別家用 hash 是因為兩端的實體編號不同；GYO 沒有這個問題，(b) 會多一張對照表與 hash 碰撞的處理。代價：scene 物件要有穩定的 `id`（工具負責不重用）、要一段固定的編號範圍 |
| **P2** | 放好的實體 Peer 看不見時（4.2） | (a) **卸下**：拿掉複製來的 component，實體留著 (b) 照一般實體刪掉 (c) 什麼都不做（留著最後的值） | **(a)**：放好的實體是兩端 scene 的，刪了 Client 放不回來；(c) 會讓看不見的東西留著舊值，和「看不見＝沒有它的資料」矛盾。別家怎麼處理沒查到（未確認） |
| **D2**（**待定**，使用者還在了解） | 交互的按鍵怎麼上 wire（主會話原本的建議是 `PlayerInput` 多一個位元） | (a) `PlayerInput` 加 `interact`（Codec 2 → 3，不多送訊息）(b) 另一個輸入型別 `InteractInput`，在 `interaction` 組裡登記、只在組開著時放到玩家身上 | **調查中**（主會話轉交，HANDOFF `v8-arch-gyo2-interact-d2-direction`）：結果見附錄 [`interact-d2-investigation.zh-Hant.md`](interact-d2-investigation.zh-Hant.md)，調查的推薦改成 (a)。使用者核准之前，本宣告的其他各節仍照 (b) 寫；核准後照附錄第 6 節末段改寫。原本推薦 (b) 的理由「核心 PvP 的 digest 會變」已由附錄第 3 節（#374 7.3 (a) 的比法）推翻 |

不是使用者的決定，但要別人做的：

- **同詞異義表的 Scene 那一列**（7.4）：Owner 是架構師會話，請主會話轉。
- **arena 的 `scene` 欄**（第 6 節）：請主會話轉給 arena 規則拆分那一批（模式配置卡）。

卡片自己判的（列出來讓你知道；要改就說）：

- 新名字 `Screen`（7.1）；舊名的檢查只掃清單、放在產品的測試（7.3）。
- Scene 檔是遊戲層的資料契約，不做 Engine 的 scene Module（第 5 節開頭）。
- 物件寫成「組＋組的資料」，由組的 reader 解析，代替 Bevy 的 reflection（1.4、5.1）。
- `sceneDigest` 只算模式開著的組的物件（5.4）。
- 開賽前讓 Peer 看到所有 `Placed` 的實體：核心 `ShowPlayers` 改一行（3.2）。
- `Interactable` 不帶種類、提示字、範圍；組的依賴用 `requires`、讀取時報錯；目標＝射線打到的第一個 collider（第 2 節）。
- 物理的開關是瞬間的、擋到人就不切換；`E` 寫死、不做改鍵；新模式叫 `sandbox`（`modes.json` 的第二個，預設仍是核心 PvP）；S5 不重量。

---

## 11. 使用者的決定（修訂 1 的由來）

主會話轉交，2026-11-27；出處 HANDOFF `v8-arch-gyo2-map-scene-split`。

**原話**：

1. 「首先門，之前的計劃是放在場景裡而不是Map，Map是毛坯房，Scene類似於裝修。一樣放在實體裡。但是場景數據是另一個配置文件（和地圖文件一樣）。你認為如何？」
2. 看完 side chat 的看法後：「嗯，轉回去。不過，我建議遊戲流程的畫面的Scene改名，Scene其實算是一個專有名詞。」

**決定**：Map 和 Scene 分開（Scene 是另一個配置檔；arena＝map＋scene，side chat 推論、使用者沒有反對）；門的開關狀態放在門的實體上；「Scene」保留給裝修資料，遊戲流程畫面的 `Scene` 改名。

**比過的選項**：修訂前的 D1 (a) Match spawn 門、(b) 狀態放 Resource、(c) 先補 pre-spawned。使用者的方向等於「有獨立 scene 檔的 (c)」。

**理由**（主會話轉述）：Map 和 Scene 的生命週期不同（地圖改了要重烘焙，換門的位置不該動到地圖）；合「規則不綁定地圖」（同一張毛坯房可以配不同的裝修）；成熟方案都這樣分（Godot 的資源與 `.tscn`、Unity 的 Scene／SubScene＋prefab、Bevy 的 `bevy_scene`）；Scene 是業界的專有名詞，流程畫面佔用它會一詞兩義。

D2：使用者還在了解，先維持 (b)、標待定。

---

## 12. 沒查到、未確認的

- bevy_replicon、lightyear、Unity、Godot 對「放好的實體在 Peer 看不見時」怎麼處理，沒查到（P2）。
- Godot 的節點路徑怎麼上 wire，沒逐行讀（1.3）。
- Bevy 官方範本把流程畫面叫 `Screen`：本機沒有 `bevy_new_2d` 的源碼（7.1）。
- 一個 World 兩種輸入沒有 Engine 的測試（1.2）：第 1 步補。
- 旋轉的 `Cuboid` 有沒有 `MoveAndSlide` 的測試案沒逐案確認（10.1）。

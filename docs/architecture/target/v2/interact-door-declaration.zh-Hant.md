# 交互第一批：按 E 開關的門與交互的骨架（`v8-arch-gyo2-interact-door`）

任務卡，2026-11-27。這一階段只做研究與事前宣告，交使用者核准；**#367（模式配置）合併之前不寫程式**（#367 正在改同一個遊戲的檔案）。作者 Opus 5.5／high。

出處：研究 #361（`interaction-dialogue-research.zh-Hant.md` 第 1 節、第 4.1 節、第 6 節的 Q1～Q6）；模式配置宣告 #367（`mode-config-declaration.zh-Hant.md`，以下簡稱「#367 宣告」，讀的是 `claude/gyo2-mode-config-declaration` 1baf9122，第 2.3 節 (R1)、第 2.4 節、第 4 節、第 5 節、第 6 節「門」那一列）。程式的行號：Engine 照 `gyo2` 85df84da；遊戲照 #367 的分支（合併後才是 `gyo2` 的樣子）。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| Q1 (a)：多人時輸入只送「按了交互」，Match 用同一 tick 的視線自己打射線找目標 | #361 第 6 節 |
| Q2：交互放 `object_fps_pvp`，先做一扇門 | #361 第 6 節 |
| Q6：Engine 的四個缺口（`ChildOf` 下的 collider、`BoxSet` 射線的方塊編號、Client → Match 的可靠離散訊息、action map）等點名再做；這張卡要用到就停下回報 | #361 第 6 節；卡片 |
| 路線 (A)：新功能加在 Object_PvP，由模式配置決定這一場掛哪些系統；「核心 PvP」模式不開，結果和現在逐位元一致 | #367 宣告第 0.1、2.3、5 節 |
| (R1)：玩法組在每個模式都固定登記，System 進帶模式條件的 set；component 只在組開著時放到實體上 | #367 宣告第 2.3、4 節 |
| 「商店 NPC 的對話，和商店內的商品是不同的系統」；「遊戲規則不綁定地圖」 | 使用者（卡片轉述；後者另見 #367 宣告 11.7 U3） |
| Engine 只給機制；Kernel 的職責是 ECS；錯誤用 `Result` 只告知；不對誤用加守衛；ECS 相關以 Bevy 為主參考 | AGENTS §1、§3；批次表〈每批的設計審查〉第 5 項 |

### 0.2 結論（一段話）

**Engine 不改程式，也用不到 Q6 的四個缺口。** 交互與門都是遊戲層的兩個玩法組：

1. **`interaction` 組（骨架）**：玩家身上多一個輸入型別 `InteractInput{interact}`（一次性按鍵，和 `PlayerInput` 分開登記）；可交互的東西掛 `Interactable`；Match（與單機的 Local World）在固定步裡用同一 tick 的視線打一條射線，打到的第一個 collider 如果是 `Interactable`、而且在模式規定的距離內，就發一則 Kernel `Message` `Interacted{actor, target, tick}`。骨架不知道「門」。
2. **`doors` 組（第一個種類）**：門的位置與形狀放在 arena 檔的 `sets.doors`（地圖的資料），能伸手多遠放在模式檔（規則）。門是兩端各自放好的實體（固定編號，collider＋transform＋`Interactable`＋`Door{index}`）；開／關狀態是一個 authority Resource `DoorStates`，由 Match 寫、複製給 Client。`OperateDoors` 讀 `Interacted`，只處理目標有 `Door` 的；`PoseDoors` 每步照狀態擺 collider。Client 照狀態播擺動與音效，提示字由 Client 自己找焦點後顯示。
3. **核心 PvP 不開這兩組**：玩家身上沒有 `InteractInput`、沒有 `DoorStates`、沒有門的實體，權威 digest 每一步逐位元相同（用 #367 的 A/B 腳本證明）。`PlayerInput` 與它的 Codec（版本 2）**不動**。

和主會話建議不同的兩處（第 8 節 D1、D2）：輸入不是在 `PlayerInput` 多一個位元，而是另一個輸入型別；門的狀態不是門實體上的 component，而是一個 Resource。

---

## 1. 成熟方案

查證等級：**源碼**（本機 `~/Code/GitHome`，附檔名與行號，路徑從各 repo 的根算起）、**官方文件**、**未確認**。#361 第 1.2～1.5 節已比過的（找目標、提示、按鍵、多人兩派）不重抄，只補這一批多出來的問題。

### 1.1 骨架的形狀：「找目標」在共用處、「做什麼」在各種類

| 家 | 找目標的一方（interactor） | 交互了 → 誰處理 | 距離放哪 |
|---|---|---|---|
| **Bevy**（主參考，`bevy` 6a911653f） | picking 的後端把命中寫成 `PointerHits`、合併成 `HoverMap`（#361 1.3） | `Pointer<E>` 同時是 `EntityEvent` 與 `Message`（`crates/bevy_picking/src/events.rs:170` 的 `#[derive(EntityEvent, Message, …)]`）：可以用 observer 收，也可以用 `MessageReader`（`crates/bevy_ecs/src/message/message_reader.rs:38`）在 System 裡收；作者的 System／observer 看目標身上有沒有自己的 component | 指標的射線（後端），不在目標上 |
| Unity XRI | Interactor 列有效目標、`XRInteractionManager` 決定（#361 1.4） | Interactable 的 `selectEntered` 等事件，各自處理 | Interactor 那一側（射線長度） |
| Godot | `RayCast3D` 節點（`scene/3d/physics/ray_cast_3d.cpp:206-223`），`target_position` 決定長度 | 腳本 `get_collider()` 後呼叫目標的方法（#361 1.2） | 射線節點（玩家那一側） |
| Unreal Lyra | `AbilityTask_WaitForInteractableTargets_SingleLineTrace` 的 trace 長度 | 依 `FInteractionOption` 的 tag 發 Gameplay Event 給對應的 ability（#361 1.4） | ability task（玩家那一側） |

共同點：

1. **「打到誰」只有一處，「打到之後做什麼」由各種類自己接。**骨架只回答「誰對誰做了交互」。
2. **能伸手多遠是 interactor（玩家）那一側的規則**，不寫在每個目標上。
3. 「這是門還是箱子」不是骨架裡的一個 enum：Bevy 用目標身上有沒有某個 component，Godot 用方法或 group，Lyra 用介面與 tag。

### 1.2 多人：輸入型別可以有好幾種

| 家 | 做法 | 位置 |
|---|---|---|
| Unity Netcode for Entities | 輸入是 `IInputComponentData`（`Runtime/Command/IInputComponentData.cs:20`），一次性的按鍵用 `InputEvent`（`:34-56`，「保證伺服器恰好偵測到一次」）；每種輸入元件各自烘成一條 command 緩衝 | 源碼（`com.unity.netcode`） |
| lightyear | 輸入外掛是泛型 `InputPlugin<A>`（`crates/inputs/inputs_native/src/plugin.rs:15`），每種 action 型別登記一個 | 源碼（`lightyear` 736e1695） |
| GYO net | `NetRegistry::inputs` 是一個清單（`engine/modules/net/include/engine/net/Replication.hpp:153-158`），每種輸入各自的緩衝 `PeerInput<T>`（`:428`）、各自的 entry `ApplyInput<T>`（`:543-544`），只寫進「受控實體身上有 `T`」的（`:553`）；每種輸入每幀各送一則訊息（`engine/modules/net/src/Replication.cpp:1179-1182`） | 源碼 |

GYO 的 net 本來就是「一種輸入一條路」，和 Unity、lightyear 一樣。**但一個 World 同時登記兩種輸入，現在沒有測試**（`tests/common/net/` 裡每個 World 都只有一種：`InputTests.cpp:66`、`RollbackClientTests.cpp:239`）。第 0 步先補這個 Engine 的測試（只加測試）；不通過就是 Engine 的缺陷，停下回報。

### 1.3 場景裡放好的物件，狀態怎麼同步

| 家 | 做法 | 查證 |
|---|---|---|
| Unity Netcode for Entities | **pre-spawned ghost**：場景裡放好的物件兩端都有，烘焙時照 subscene 的 hash 與 `PreSpawnedGhostIndex` 決定性地配 ghost id（`Runtime/Snapshot/Prespawn/PrespawnComponents.cs:9-33`） | 源碼；這是 **Engine（netcode）的機制** |
| Godot | 不經 `MultiplayerSpawner` 生成的節點，兩端的場景裡都有，`MultiplayerSynchronizer` 照節點路徑同步（`modules/multiplayer/multiplayer_synchronizer.h:55` `root_path`；`scene_replication_interface.cpp:224-240`：只有 spawner 生成的才在 `:240` 拿 net id） | 源碼；路徑怎麼上 wire 沒逐行讀（未確認） |
| bevy_replicon、lightyear | 伺服器 spawn、複製到 Client 是主路；lightyear 另有 `PreSpawned`（Client 先生成、再和伺服器的對上） | 沒讀這一段的源碼（未確認） |
| GYO net | 只有伺服器配的實體走複製（NT-9：Client 用 `SpawnWithId`）；伺服器第一次送某個實體是「spawn」，Client 有 rollback 時交給 rollback 的 inbox 在確認點放回（`Replication.cpp:653-667`；已經活著的實體只計數 `entriesWhileAlive`）。**沒有「兩端都已放好的實體」這條路** | 源碼 |

GYO 沒有 Unity 的 pre-spawned ghost。門有兩種做法（第 3.2 節）：(a) Match spawn 門、複製；(b) 兩端照地圖各自放好門的實體，狀態放進一個複製的 Resource。這張卡**不補 pre-spawned 的 Engine 機制**（它不在 Q6 的四項裡，而且 (b) 用不到它；見第 9 節）。

---

## 2. 骨架：`interaction` 組

**前提：這是作者能處理的。**找目標用現成的 `Physics3d::CastRay`（`engine/modules/physics_3d/include/engine/physics_3d/SpatialQuery.hpp:106-108`），事件用 Kernel 的 `Message`（`engine/ecs/kernel/include/engine/kernel/Params.hpp:484`、`:516`），輸入用 net 的 `Input<T>`。

### 2.1 資料

| 名字 | 種類 | 內容 | 決策資料？ | 複製？ |
|---|---|---|---|---|
| `InteractInput` | component（玩家身上）；net 的輸入型別 | `std::uint8_t interact`：這一步按了交互（一次性） | 是（`authority.Component<InteractInput>(…, {.input = true})`，照 `PlayerInput`，`PvpWorld.cpp:1171`） | 輸入（Client → Match），Codec 版本 1 |
| `Interactable` | component（可交互的東西身上） | 沒有欄位（marker）。種類由同一個實體上的種類 component 表示（門：`Door`） | 否（兩端照資料放，比對值保證兩端相同） | 否 |
| `InteractionRules` | Resource | `float reach`：能伸手多遠（公尺），模式檔 `sets.interaction.reach` | 否（設定；進 `rulesDigest`，#367 宣告 11.2「`put` 在每一端都執行」） | 否 |
| `Interacted` | Kernel `Message` | `actor`（玩家）、`target`、`tick` | 是（`authority.Message<Interacted>`，不進 digest） | 否 |

**為什麼不照主會話的「`Interactable`（種類、提示字、範圍）」**：

- **種類**：骨架裡放一個種類的 enum，每加一種（箱子、NPC、拉桿）都要改骨架。Bevy、Godot、Lyra 都不這樣做（1.1 第 3 點）；ECS 的做法是目標身上掛種類自己的 component，種類的 System 用 `With<Door>` 一類的條件挑。
- **提示字**：會跟著種類的狀態變（門開著時是「閉める」），只有種類自己知道；也只有 Client 要顯示。所以由 Client 端各種類自己的 System 給（第 4.2 節），不放在兩端共用的 component。
- **範圍**：是 interactor 那一側的規則（1.1 第 2 點），而且是規則、不是地圖的資料（「遊戲規則不綁定地圖」），放模式檔。之後真的有「某一種要更遠」的需求，再讓種類的設定蓋過它。

### 2.2 System

| System | 掛點 | 哪一端 | 做什麼 |
|---|---|---|---|
| `GiveInteractInput` | Main/Update，`SeatPlayers` 之後 | Match（`OnServer`）、Local | 沒有 `InteractInput` 的玩家，插一個 |
| `ControlOwnInteract` | Main/Update，`ControlOwnPlayer` 之後 | Client（`OnClient`） | 自己的玩家插 `InteractInput`；自己的 Peer 插 `PeerInput<InteractInput>`（照 `PrepareOwnInput`，`ClientWorld.cpp:576-579`） |
| `WriteStepInteract` | FixedUpdate/First，不在重算時 | Client（Sim World）；Local 另一支直接寫玩家的 component（照 `WriteLocalInput`，`ClientWorld.cpp:631-638`） | 從 `SimControls` 取出、清掉 `interact` 的按下，寫進這一步的槽 |
| `JudgeInteractions` | FixedUpdate/PostUpdate，`RespawnPlayers` 之後 | Match（`OnServer`）、Local | 見 2.3 |

全部進 `pvp.interaction.fixed`／`pvp.interaction.frame`（#367 宣告 2.4），帶 `pvp.mode.interaction` 條件。

### 2.3 Match 怎麼判（Q1 (a)）

每個這一步按了交互、活著、比賽已開始（照 `Playable`，`PvpWorld.cpp:400-403`）的玩家，照 EntityId 由小到大：

1. 射線＝`ShotRay(rules, body)`（`PvpWorld.cpp:474-477`，和射擊同一條視線、同一步的姿勢）。
2. 對所有 collider（`Walls` 這個 Query，`PvpWorld.cpp:268`；門的 collider 也在裡面）打 `CastRay`，最遠 `reach`。
3. 打到的**第一個** collider 如果有 `Interactable`，發 `Interacted{player, target, tick}`；打到牆、或什麼都沒打到，就不發。地板平面（`arena.floor`）在 `reach` 內擋住時也不發（照 `JudgeShots` 對地板的算法，`PvpWorld.cpp:499-506`）。

「打到的第一個 collider」＝Godot 的 `get_collider()`、準心對著的那個東西。這一批的種類只有門，門本身就是 collider；之後沒有 collider 的種類（NPC 的身體、地上的小東西）要另給「交互用的形狀」，見第 6 節。

**順序**：`JudgeInteractions` 在 `RespawnPlayers` 之後；門的狀態在第 n 步的 PostUpdate 改，碰撞形狀在第 n+1 步開頭擺好（2.4、3.3）。兩端都照這個順序，Client 重算時結果一樣。

### 2.4 組之間的依賴

`doors` 沒有 `interaction` 就永遠收不到 `Interacted`。#367 的 `GameplaySet`（`games/object_fps_pvp/include/object_fps_pvp/world/GameplaySets.hpp:46-56`）沒有依賴的欄位。

- 做法：`GameplaySet` 加 `requires`（組名的清單）；`LoadModes` 讀到「開了 `doors` 卻沒開 `interaction`」的模式就讀取失敗（`error` 寫哪一組要哪一組），和 #367 的「不認得的組名」同一種錯誤。
- 不採「自動幫它開」（Bevy 的 Plugin 常在 `build` 裡 `add_plugins` 自己的依賴）：模式檔是寫給人讀的資料，開了什麼應該照字面就看得到；自動開會讓 `rulesDigest`（只 hash 模式檔讀到的值）與實際開的組不一致，要另外處理。

---

## 3. 第一個種類：`doors` 組

**前提：這是作者能處理的。**

### 3.1 資料放哪（「遊戲規則不綁定地圖」）

| 什麼 | 放哪 | 理由 |
|---|---|---|
| 門在哪、多大、往哪邊開、開幾度、什麼顏色 | arena 檔 `sets.doors`（#367 宣告 3.2 末段：arena 檔可以帶組的一塊資料） | 地圖的形狀 |
| 能伸手多遠 | 模式檔 `sets.interaction.reach` | 規則 |
| 這一場有沒有門 | 模式檔有沒有開 `doors` | 規則；地圖有門的資料，模式沒開就不放 |
| 上鎖、鑰匙 | 之後（背包） | — |

arena 檔（例）：

```json
"sets": {
  "doors": {
    "doors": [
      { "hinge": [6.0, 0.0, 10.0], "yaw": 0.0, "width": 1.0, "height": 2.1, "thickness": 0.1,
        "swing": 1.5707963, "color": [0.55, 0.35, 0.2] }
    ]
  }
}
```

- `hinge`：關著時門軸那一邊的底角（世界座標）；`yaw`：關著時門板朝向（照 `PlayerBody::yaw` 的轉法）；`swing`：開的角度（弧度，正負決定往哪邊開）。
- 錯誤（讀取失敗，`error` 寫哪一扇、哪一欄）：欄位缺、型別不對、`width`／`height`／`thickness` 不是正數、`swing` 為 0 或超過 π、門超過 65535 扇。
- 模式檔（新的 `modes/sandbox.json`，`modes.json` 的第二個，預設仍是核心 PvP）：`"sets": {"interaction": {"reach": 2.0}, "doors": {}}`，`"arenas": []`（全部 arena；有門資料的才有門）。

### 3.2 門的狀態：Resource，不是門實體上的 component（D1）

| | (a) Match spawn 門、`DoorState` 是門上的 component、複製 | **(b) 兩端照 arena 資料各自放好門的實體（固定編號）；狀態是 authority Resource `DoorStates`、複製** |
|---|---|---|
| 和 Bevy 的關係 | 照 Bevy 的 ECS 習慣（每個實體自己的狀態） | **偏離**：每扇門的狀態放在一個 Resource 的陣列裡，用 `Door{index}` 對上 |
| Client 何時有門 | Match 的 structure 更新到了才有；開賽前 `ShowPlayers` 只讓 Peer 看到自己的玩家（`PvpWorld.cpp:759-779`），所以開賽前看不到門，要改核心的 `ShowPlayers` | 從第一幀就有（和牆一樣，地圖讀完就在）；開賽前、重連後都不必等 |
| Client 預測走路 | 門到之前，預測會穿過關著的門、再被 rollback 拉回 | 一開始就擋 |
| 碰撞形狀 | 門到了之後，Client 要在複製來的實體上插 collider、transform、`Interactable`（照 `ControlOwnPlayer` 插 `PlayerInput`） | 兩端在 `Prepare` 時一起放 |
| 會不會碰到 Engine 的缺口 | 不會（走一般的複製） | 不會（`ReplicateResource` 走 rollback inbox，`Replication.cpp:771-772`；`tests/common/net/RollbackClientTests.cpp:237` 有會回滾的複製 Resource） |
| 什麼時候要改回 (a) | — | 門要在比賽中生成或被打壞（實體會增減）；或門多到「改一扇就送整個陣列」變貴（現在一扇 1 byte） |

**推薦 (b)。**理由：門是地圖的一部分，兩端本來就各自讀同一張地圖（`arenaDigest` 保證相同）；(b) 不必改核心的 `ShowPlayers`、沒有「門還沒到」的空窗。和 Bevy 的衝突點一個（狀態不在實體上）；膠水是 `Door{index}` 到陣列位置的對照（約 10 行）。成熟方案裡最接近的是 Unity 的 pre-spawned ghost 與 Godot 的場景節點（1.3），它們能把狀態留在實體上，是因為 Engine 有「兩端都已放好的實體」的機制；GYO 沒有，這一批不補（第 9 節）。

### 3.3 資料與 System

| 名字 | 種類 | 內容 | 決策資料？ | 複製？ |
|---|---|---|---|---|
| `Door` | component（門的實體） | `std::uint16_t index`：arena 的第幾扇 | 否 | 否 |
| `DoorLayout` | Resource | arena 讀到的每扇門的形狀（3.1） | 否（進 `arenaDigest`） | 否 |
| `DoorStates` | Resource | `std::vector<std::uint8_t> open`（第 i 扇開著＝1） | 是（`authority.Resource<DoorStates>`，回滾照預設） | `ReplicateResource`，Codec 版本 1：扇數 u16＋每扇 1 byte |

- **門的實體**：`doors` 的 arena reader 回傳的 `SetData::put`（#367 的 `GameplaySets.hpp:40-43`，在每一端的 `Prepare` 執行）放進 `DoorLayout`、`DoorStates`（全部關著），並為每扇門放一個實體：固定編號（在 `kMatchIds`、`kClientIds` 兩個配號範圍之外，也避開現有的固定編號，例 `3'100'000 + index`；照 `kArenaEntity`，`Components.hpp:173-175`）、`Door`、`Interactable`、`Physics3d::Collider{Cuboid}`、`Transform`。
- **`PoseDoors`**（FixedUpdate/First，兩端）：照 `DoorStates` 擺每扇門的 `Transform`（關著：`yaw`；開著：`yaw + swing`，繞門軸轉）。`Transform` 不是決策資料；它每步從會回滾的 `DoorStates` 算出來，所以 Client 重算時跟著對。`MovePlayers`、`JudgeShots`、`SpawnFree` 讀的 `Walls` Query 本來就收所有沒有 `ChildOf` 的 collider，門自動會擋走路與子彈，**核心的 System 不用改**。門不掛父物體（不碰 Q6 的 `ChildOf` 缺口）。
- **`OperateDoors`**（FixedUpdate/PostUpdate，`JudgeInteractions` 之後；Match、Local）：讀 `Interacted`，目標有 `Door` 的，切換那一扇。**新的姿勢會壓到活著的玩家時不切換**（用 `Physics3d::FindContact` 對每個活著的玩家的站姿膠囊，照 `SpawnFree`，`PvpWorld.cpp:574-588`）。同一步多人按同一扇門：照 `Interacted` 的順序（EntityId 由小到大），每一則各切一次。
- 物理的開關是**瞬間的**（一步之內換姿勢）；擺動是 Client 的畫面（4.2）。不做「推門時推動玩家」「被擋住時半開」。

### 3.4 「做什麼」不綁在骨架上的例子

`OperateDoors` 是 `doors` 組自己的 System；骨架不知道門。之後加種類時，各自讀 `Interacted`、挑自己的目標（第 6 節）。

---

## 4. Client

**前提：這是作者能處理的**（畫面、聲音、按鍵都是 Client 的表現，不寫決策資料）。Client 是三個 World（`ClientWorld.hpp:3-19`）：Sim 跑網路與預測，Frame 畫畫面與讀按鍵，兩者經 `ClientExchange` 交換。

### 4.1 按鍵

- Frame World 的 `ReadControls` 讀 `E`（`I::KeyCode::E`，照 `R` 的 `JustPressed`，`ClientWorld.cpp:554`），`present::StepInput` 加 `interact`（按下，照 `reload`）；`SimControls` 照現在的「按下會累加、取走時清掉」帶到 Sim World。
- 這是照現有按鍵的寫法寫死 `E`，**不做改鍵**（Q6 的 action map 缺口不碰）。
- 模式沒開 `interaction` 時，按 `E` 沒有任何效果（Sim World 沒有 `PeerInput<InteractInput>`，也不送）。

### 4.2 焦點、提示、門的畫面與聲音

- **焦點**（Frame World，每幀）：從自己的眼睛沿著畫面的視線打射線（牆用 `WorldSnapshot::walls`，`ClientExchange.hpp:46`；門用 `DoorLayout` 與 Sim 傳來的狀態），規則和 2.3 相同。這是 Client 自己算的，和 Match 的判定可能差一點（Q1 (a) 已知的缺點，#361 1.7）。
- **提示**：焦點是門時，HUD 顯示「[E] 開ける」或「[E] 閉める」（照門的狀態）。Frame World 的提示由種類各自的 System 寫（門寫門的字），HUD 只顯示「目前焦點的提示」。字串照 PvP 現有的 HUD 寫在程式裡（`ClientWorld.cpp:1022-1032`）；Q4 的 line ID 是台本的約定，UI 的字串表等第二種語言再做（#361 Q4）。
- **門的畫面**：每扇門一個 `Mesh3d`，網格用和地圖的 region 同一條路在程式裡做（一個長方體，顏色照 `color`）；狀態變了就照擺動秒數轉到目標角度。擺動秒數、音效的音量與距離放 Client 讀的表現檔（`audio/sfx.json` 加 `door_open`、`door_close` 兩個 cue，版本號加一），不寫死在程式。
- **音效**：狀態從 0 → 1 播 `door_open`、1 → 0 播 `door_close`，在門的位置用現有的空間化（`PlaySounds` 的 `spatial`，`ClientWorld.cpp:1091`）。素材：自製或 CC0，來源寫進 `assets/object_fps_pvp/LICENSES/SOURCES.zh-Hant.md`。
- **單人（Solo）**：同一套，`DoorStates` 從 Local World 直接拿。

---

## 5. wire 與權威 hash 的影響

### 5.1 wire

| 項目 | 變化 | 核心 PvP 模式 | sandbox 模式 |
|---|---|---|---|
| `PlayerInput` 與 Codec（版本 2，`PvpWorld.cpp:198-224`） | **不變** | 不變 | 不變 |
| 新的輸入型別 `InteractInput`（Codec 版本 1：`interact` 1 byte；`held` 清掉 `interact`，C-IN3） | 新登記（所有模式） | 不送（Client 沒有它的緩衝，`encodeRecent` 回 false，`Replication.hpp:857-881`） | 每幀多一則輸入訊息：每格 1＋4＋1＋2＋1＝9 bytes、重送 5 格＝45 bytes，加上訊息頭（`Head`＋wire 2＋end 4＋count 2）；量級每 Client 每幀約 60 bytes |
| 新的複製 Resource `DoorStates`（Codec 版本 1：扇數 u16＋每扇 1 byte） | 新登記（所有模式） | 不送（沒有這個 Resource；#367 `[pvp-set-off-sent]` 已確認不存在的 Resource 不送） | 狀態變的那一步送一次（現在一扇門：3 bytes） |
| protocol hash（名字、順序、Codec 版本，`Replication.hpp:13-14`） | 所有模式都變（多兩個登記，排在核心之後） | 變（兩端一致；舊 Client 連不上新 Match，正常的升版，#367 宣告 2.3） | 同左 |
| `MatchInfo` 與它的 Codec | 不變 | 不變 | 不變 |
| Engine 的 `kNetProtocolVersion` | 不變 | — | — |

### 5.2 權威 hash

| 項目 | 核心 PvP 模式 | sandbox 模式 |
|---|---|---|
| 權威 digest（每個決策型別每個有值的格子，`engine/modules/authority/include/engine/authority/Registrations.hpp:246-264`；沒有值是 0） | **逐位元相同**：`InteractInput` 不在任何實體上、`DoorStates` 不存在、`Interacted` 不進 digest；核心型別的 authority 登記序號不動（新的排在後面，#367 宣告 2.4） | 多 `InteractInput`（每個玩家）與 `DoorStates` |
| consistency fingerprint（寫決策資料的 System、條件、set） | 變（多兩個組的 System 與 set；兩端一致）；`DatasetReport` 的差異只在核心之後多出的列 | 同左 |
| `rulesDigest` | 不變（核心模式的檔案不動） | sandbox 自己的 |
| `arenaDigest`（模式開著的組的 arena 區塊） | 不變：pvp_arena 多了 `sets.doors`，但核心模式沒開 `doors`，區塊不算（第 2 步用測試確認） | 含門的資料 |
| `wallsDigest` | 不變（門不是 `BoxSet` 的一部分） | 不變 |

---

## 6. 之後的種類怎麼掛上來（點名過的功能）

只列「怎麼掛」，不是那些批的設計。要用到 Q6 缺口的，標出來。

| 功能 | 找目標 | 觸發 | 收什麼 | 狀態／wire | 用到的缺口 |
|---|---|---|---|---|---|
| 門（這一批） | 射線打到的 collider | 按 E | `Interacted` | `DoorStates` | 無 |
| 互動物品（拉桿、按鈕） | 同門（實體有 collider） | 按 E | `Interacted` | 自己的 Resource 或 component | 無；拉桿是**體素格本身**時要 `BoxSet` 射線的方塊編號（或作者用 `t`、法線推格子，#361 1.6） |
| 開箱 | 同門 | 按 E（之後可能要長按：`InteractInput` 加 `held`，Codec 升版本） | `Interacted` → 開箱 UI | 箱子的內容是私有資料（`PeerVisibility::ShowPrivate`，`Replication.hpp:473`）；「拿哪一件」放輸入（像台本的選項一個 byte） | 「整包交易」才需要 Client → Match 的可靠訊息 |
| NPC 對話 | **NPC 沒有 collider**：骨架加「交互用的形狀」（另一個 component，和 collider 分開；`JudgeInteractions` 的候選多一種） | 按 E | 對話的 System 收 `Interacted`；Ink 跑在權威端（#361 2.6） | 選項放輸入 | 無 |
| 商店 | 不自己收 `Interacted`：**對話的指令**（Ink 的 `EXTERNAL`）發「開店」的 Message，商店的 System 收（「對話與商品是不同的系統」） | — | 商店自己的 Message | 商品清單、買賣放輸入 | 大量內容才需要可靠訊息 |
| 自動拾取 | **不經骨架**：靠近就撿，用 `Physics3d::Intersections`（`SpatialQuery.hpp:171-183`）每步問一次 | 走過去 | 拾取自己的 System | 背包 | 無 |
| 解謎 | 由多個互動物品組成 | 按 E | 各物品的 `Interacted` → 解謎的 System 看全部的狀態 | 解謎的 Resource | 同互動物品 |
| 小遊戲 | 同門 | 按 E 進入 | `Interacted` → 小遊戲的 System；進入後的操作放小遊戲自己的輸入型別（像 `InteractInput` 一樣另登記） | 小遊戲自己的 | 改鍵才需要 action map |

骨架要長的地方只有兩個（之後的批才做）：**交互用的形狀**（NPC）、**長按**（開箱）。兩者都是遊戲層，不改 Engine。

---

## 7. 核心 PvP 怎麼證明沒變、外部可驗的測試

### 7.1 A/B（照 #367 宣告第 5 節）

- 用 #367 的核心 PvP 腳本（ctest `object_fps_pvp.core_script`）與主 checkout 的 `build/evidence/gyo2-mode-config/ab.py`：改前（這條線分出的那一點）與改後各建一次，同一台機器、核心 PvP 模式，**權威 digest 每一步逐位元組相同**；`--report` 的差異只能是 protocol hash 與 `DatasetReport` 在核心之後多出的列。
- 在 macOS（本機）與 Linux 主機跑；Windows 只跑 L1（建兩次太重，照 #367）。結果放主 checkout `build/evidence/gyo2-interact-door/`。
- **S5 不重量**：這條線不動讀圖與建牆；#367 11.8 已查出「程式碼變多會讓讀圖慢幾十 ms」是擺放位置的效應，重量只會量到這個。要量的話再說。

### 7.2 新的測試（外部可驗；本機＋L1 三平台，network 的照檔案原本的標籤）

| 測試 | 驗什麼 |
|---|---|
| net（Engine，只加測試）：一個 World 兩種輸入 | 兩種輸入都照步到 Match、各自的 `held` 只清自己的按下、只登記沒放緩衝的那種不送 |
| world `[pvp-door-toggle]` | sandbox 的 Local World：看著 reach 內的門按 E → 那一步 `DoorStates` 變 1；再按 → 0 |
| world `[pvp-door-reach]`、`[pvp-door-wall-between]` | 超過 reach、中間有牆：不變 |
| world `[pvp-door-blocks]`、`[pvp-door-open-passes]` | 關著的門擋走路與子彈（門後的玩家打不到）；開著的不擋原本的門洞 |
| world `[pvp-door-body-in-way]` | 新姿勢壓到活著的玩家：不切換 |
| world `[pvp-door-before-start]`、`[pvp-door-dead]` | 開賽前、死了：按 E 沒有效果 |
| world `[pvp-door-match-client]` | Match＋2 Client 的記憶體連線：A 按 E → Match 切換；兩個 Client 的 `DoorStates` 和 Match 相同；B 的預測被關著的門擋住、門開後能過；沒有 `ContentMismatch` |
| world `[pvp-interact-off]` | 核心 PvP：玩家身上沒有 `InteractInput`、沒有 `DoorStates`、沒有門的實體（pvp_arena 有門資料也一樣）、按 E 沒有效果、沒有多送的輸入訊息 |
| world `[pvp-door-needs-interaction]` | 開 `doors` 沒開 `interaction` 的模式：讀取失敗，原因寫明 |
| world `[pvp-door-arena-data]` | 門資料各種錯：讀取失敗並寫明哪一扇哪一欄；`arenaDigest` 在開 `doors` 時隨門的資料變、沒開時不變 |
| client `[pvp-door-prompt]` | 看著 reach 內的關門／開門：提示「[E] 開ける」／「[E] 閉める」；看別處或太遠：沒有提示 |
| client `[pvp-door-swing]`、`[pvp-door-sound]` | 狀態變了之後門的角度朝目標走、擺動秒數後到達；開／關各播一次對應的 cue |
| Go `./services/...` | 房間的模式清單多了 sandbox（#367 M1 (a) 照 `modes.json` 讀），既有測試若數模式要跟著改 |

### 7.3 相關突變（D184：新的、find 在改動範圍裡的）

預定的方向（實際在各步定）：reach 的比較反過來、射線不被牆擋、`held` 沒清 `interact`、比賽開始前也判、死了也判、`PoseDoors` 不擺（門開了還擋）、`OperateDoors` 不看擋到人、組沒開也放 `DoorStates`／門的實體、`InteractInput` 在組沒開時也插、`requires` 不查、門的區塊沒進 `arenaDigest`、提示字的開／關反了。

---

## 8. 批次與要使用者決定的點

### 8.1 批次（一條線、一個 PR：這份宣告的 draft PR 接著放實作）

```
v8-arch-gyo2-interact-door（一個 PR，開向 gyo2；#367 合併後才動工）
├─ 第 0 步｜Opus/high｜下段｜A/B 改前的輸出存下；Engine 測試「一個 World 兩種輸入」（只加測試，不過就停下回報）
├─ 第 1 步｜Opus/high｜上段｜interaction 組：InteractInput（輸入型別）、InteractionRules、Interactable、JudgeInteractions、Interacted、GameplaySet::requires
├─ 第 2 步｜Opus/high｜上段｜doors 組：arena 的門區塊、門的實體、DoorLayout、DoorStates（authority＋ReplicateResource）、PoseDoors、OperateDoors；sandbox 模式檔；pvp_arena 放一扇門
├─ 第 3 步｜Opus/high｜下段｜Client：E 鍵、焦點與提示、門的網格與擺動、音效（sfx.json、素材、LICENSES）
└─ 第 4 步｜Opus/medium｜下段｜A/B（macOS、Linux）、每批的設計審查、純淨 Code Review、相關突變、dev_log
```

| 步 | 上段／下段（根據） | 實作者 | 審查者 | 檢驗者 |
|---|---|---|---|---|
| 0 | 下段（只加測試與證據，不動行為） | Opus／high | 併入第 4 步 | Opus／high |
| 1 | **上段**：wire（新的輸入型別、protocol hash）、資料契約（模式檔的新組）、權威（新的決策型別） | Opus／high | Opus／high | Opus／high |
| 2 | **上段**：wire（新的複製 Resource 的 Codec）、資料契約（arena 檔的門區塊）、權威 | Opus／high | Opus／high | Opus／high |
| 3 | 下段（Client 的表現，不寫決策資料、不動 wire） | Opus／high（多個 Client 檔） | Opus／high | Opus／high |
| 4 | 下段（執行與紀錄）；檢驗的是上段的成果，檢驗者不用小模型 | Opus／medium | — | Opus／medium |

- 設計審查 Opus／high（唯讀，報告放 `build/evidence/gyo2-interact-door/`）；純淨 Code Review Fable／high（只給 git 範圍；Fable 不能用時 Opus，報告寫明）。模型都不超過主會話（Opus）。
- **時間**：第 0 步約 2 小時；第 1、2、3 步各約半天到一天；第 4 步的 Linux A/B 約 30 分鐘（要主機開著）。超過估計 1.5 倍就停下回報。
- **停下回報的條件**：第 0 步的 Engine 測試不過；實作中發現一定要用 Q6 的缺口或 pre-spawned 實體；物理對旋轉的 `Cuboid`（門）和膠囊的 `MoveAndSlide`、`CastRay` 有缺陷（`tests/common/physics_3d/MoveAndSlideTests.cpp:100-104` 有帶旋轉的 helper，有沒有旋轉 `Cuboid` 的案沒逐案確認）。

### 8.2 要使用者決定的點（只列和別家不同、或和主會話建議不同的）

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **D1** | 門的開／關狀態放哪（3.2） | (a) Match spawn 門、`DoorState` 是門實體上的 component、複製（Bevy 的 ECS 習慣；要改核心的 `ShowPlayers`；Client 在門到之前預測會穿門）(b) 兩端照 arena 資料各自放好門的實體，狀態是一個 authority Resource `DoorStates`、複製（偏離 Bevy：狀態不在實體上；膠水約 10 行）(c) 先補 Engine 的 pre-spawned 實體（Unity 的做法），狀態留在門上 | **(b)**：門是地圖的一部分，兩端本來就各讀一份；不碰核心 System、沒有空窗。門要在比賽中生成或被打壞時再改 (a) 或 (c) |
| **D2** | 交互的按鍵怎麼上 wire（5.1；主會話的建議是 `PlayerInput` 多一個位元） | (a) `PlayerInput` 加 `interact`（Codec 2 → 3；不多送訊息）(b) 另一個輸入型別 `InteractInput`，在 `interaction` 組裡登記、只在組開著時放到玩家身上 | **(b)**：(a) 會讓 `PlayerInput` 的 hash 多一個欄位，核心 PvP 的權威 digest 每一步都變，「核心 PvP 逐位元一致」就做不到；(b) 照 #367 的 (R1)（component 只在組開著時出現），也是 Unity、lightyear 的做法（一種輸入一條）。代價：sandbox 模式每 Client 每幀多一則約 60 bytes 的輸入訊息；一個 World 兩種輸入要先補 Engine 的測試（第 0 步） |

卡片自己判的（不是決策點，列出來讓你知道；要改就說）：

- `Interactable` 不帶種類、提示字、範圍（2.1）；範圍放模式檔、提示字由 Client 各種類給。
- 組的依賴用 `requires`、讀取時報錯，不自動開（2.4）。
- 目標＝射線打到的第一個 collider（2.3）。
- 物理的開關是瞬間的；擺動只在畫面；擋到人就不切換（3.3）。
- `E` 照現有按鍵寫死，不做改鍵（4.1）。
- 新模式叫 `sandbox`（顯示名「サンドボックス」），`modes.json` 的第二個；預設仍是核心 PvP（3.1）。
- S5 不重量（7.1）。

---

## 9. 沒查到、未確認的、注意到但不做的

- 一個 World 兩種輸入沒有 Engine 的測試（1.2）：第 0 步補。
- Godot 的場景節點路徑怎麼上 wire，沒逐行讀（1.3）。bevy_replicon、lightyear 對「地圖上放好的物件」的做法沒讀源碼（1.3）。
- 旋轉的 `Cuboid` 有沒有 `MoveAndSlide` 的測試案沒逐案確認（8.1）。
- `arenaDigest` 在核心模式下不收 `sets.doors`：從 #367 宣告 3.3 第 5 項推，第 2 步用測試確認。
- **注意到但不做**：GYO 的 net 沒有 Unity 那種 pre-spawned 實體（兩端都已放好的實體走複製）。不在 Q6 的四項裡，這一批也用不到（D1 (b)）。門要在比賽中生成或被打壞時才會被點名；到時候是 Engine（net）的批。

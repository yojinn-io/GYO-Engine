# 背包與裝備系統怎麼做（`v8-arch-gyo2-inventory-research`）

更新：2026-11-28（新建）。Owner：研究卡（由 GYO2 重建主會話開出，`v8-arch-gyo2-inventory-research-start`）。**狀態：只是研究，不實作。第 10 節的 U1～U6 等使用者決定。**

查的版本：`gyo2` 65988d9f；門與交互骨架（#375，還沒合併）看 `origin/claude/gyo2-interact-door-e11d33` ce63d6fd 的 `interact-door-declaration.zh-Hant.md`（以下簡稱「門宣告」）；技能研究（#374，還沒合併）看 `origin/claude/gyo2-skill-research-c63667`。本地源碼（路徑從各 repo 的根算起）：Bevy 6a911653f、bevy_replicon d80c849、lightyear 736e1695、flecs 09790b5f7、Godot 94fc09c、Unity Netcode for Entities 7.2.0（c4cabf4）。

查證等級：**源碼**（附檔名與行號）、**官方文件**、**公開資料**（社群文件、媒體報導）、**未確認**。Lyra 本機沒有源碼，只讀了 Epic 的官方文件與社群文件（由子 agent 讀網頁，Opus／high）；塔科夫的兩個 wiki（fandom、wiki.gg）擋自動讀取（HTTP 402、401），只有媒體與攻略站的摘要，**細節未確認**。

---

## 0. 問題、已定的前提、結論

### 0.1 問題（用遊戲的例子）

撤離射擊的一局：

1. 地上有一把步槍。玩家走過去按 E 撿起來，步槍出現在背上（第二武器欄）。按 2 換成步槍，按 1 換回射釘槍。按 G 把手上的槍丟在地上，別人可以撿。
2. 玩家打開背包，把三盒子彈拖進胸掛；彈匣裡剩 12 發。別人看得到你背著步槍、穿著護甲，**看不到**你背包裡有什麼、彈匣剩幾發。
3. 玩家被打死，屍體變成可以搜的容器；另一個玩家打開屍體，這時才看得到裡面的東西。
4. 玩家撤離成功，身上的東西帶出去；死了就全丟（安全箱除外）。

要回答的：物品在資料裡長什麼樣、三種位置（地上、背包裡、身上）怎麼轉、網路誰看得到什麼、回滾包不包含、哪些是 Engine 的機制、哪些是 Object_PvP 的規則、怎麼分批。

### 0.2 已定的前提（這份研究不推翻）

| 前提 | 誰定的、出處 | 掛在它上面的 |
|---|---|---|
| Engine 只提供機制；Kernel 的職責是 ECS；Engine 不放產品的詞（「背包」「撤離」） | AGENTS §1.1、1.2、1.6 | 第 6 節 |
| 路線 (A)：新功能加在 Object_PvP，由模式配置決定開不開；核心 PvP 的結果逐位元一致 | `v8-arch-gyo2-object-pvp-route-a`（`HANDOFF.md:417`） | 第 3、7 節 |
| 最終產品是有怪物的撤離射擊，塔科夫只是參考之一 | `v8-arch-gyo2-tarkov-is-reference`（`HANDOFF.md:421`） | 第 2.3 節 |
| (R1)：玩法組固定登記（component、`Replicate`、authority、System），component 只在組開著時放到實體上 | 模式配置宣告第 2.3 節（`mode-config-declaration.zh-Hant.md:120-124`） | 第 3、5 節 |
| 武器系統放遊戲層；武器 entity 掛玩家下、自帶彈藥與冷卻；切換＝`Hide`→`Draw`；撿丟接交互 | `v8-arch-gyo2-weapon-system-split`（`HANDOFF.md:415`） | 第 2、3 節 |
| W1：`CombatRules` 的武器欄位搬進武器定義 `weapons/<weapon>/weapon.json`，由模式規則表的 `weapons` 列出 | 批次表 W1 列（`rebuild-batch-plan.zh-Hant.md:90`）；模式配置宣告第 6 節（`:265`） | 第 2.1、3.1 節 |
| W3 的背包切換另成玩法組 `loadout_switch`；核心 PvP 不開＝只有一把、不出現切換的 component | 模式配置宣告第 6 節（`mode-config-declaration.zh-Hant.md:265`） | 第 3.1、7 節 |
| 武器模型 U1 (a)：武器檔帶骨架與片段，手臂照骨名複製姿勢；拔槍 `Draw`、收槍 `Hide` 跟著武器；換槍＝舊 `Hide`＋新 `Draw` | `v8-arch-gyo2-weapon-model-u1-folders`、`-draw-hide-emptyhand`（`HANDOFF.md:475-476`） | 第 3.4 節 |
| 部位全部資料化 (1c)：部位有穩定的名字；傷害來源統一成「(部位, 數值)」；護甲在哪一步減傷「等武器與背包線再定」 | `v8-arch-gyo2-limb-damage-decisions`（`HANDOFF.md:422`）；`limb-damage-research.zh-Hant.md:274-278` | 第 3.5 節 |
| 遊戲規則不綁定地圖；Map＝毛坯房、Scene＝裝修（「可撿的東西」屬 Scene） | `v8-arch-gyo2-mode-config-u3`、`-map-scene-split`（`HANDOFF.md:437`、`:452`） | 第 3.6 節 |
| 交互骨架：E 是 `PlayerInput` 的 `presses` 位元；Match 用同一步的視線打射線，發 `Interacted{actor, target, tick}`；種類由目標身上的 component 表示 | 門宣告第 2 節；`v8-arch-gyo2-interact-door-decisions`（`HANDOFF.md:465`） | 第 3.3 節 |
| 通道照性質分：綁某一步、要預測的（切武器、快捷用藥）走每 tick 的輸入；不綁 tick、不預測的（拖曳整理、丟棄、購買）走可靠訊息；Client → Match 的可靠訊息現在沒有（#361 Q6，等點名） | D2 調查（門宣告附錄 `interact-d2-investigation.zh-Hant.md` 第 5 節）；`interaction-dialogue-research.zh-Hant.md:117`、`:391` | 第 4.3 節 |
| 錯誤用 `Result` 只告知；不對誤用加守衛；ECS 以 Bevy 為主參考 | AGENTS §1.9、§3；批次表〈每批的設計審查〉第 5 項 | 全篇 |

### 0.3 結論（先講）

1. **每個物品是一個實體**（推薦，U1）。物品定義是資料檔（像 W1 的 `weapon.json`），實例是實體上的 component（耐久、彈匣、堆疊數）。地上、背包裡、身上三種狀態是**同一個實體換 component**，編號不變、實例狀態不必搬。flecs 的背包範例、Unity Netcode 的 FPS 撿槍例子都是這樣；Lyra 也是「定義＋實例物件」，只是用 UObject。
2. **物品在哪裡用一個「來源存目標」的 component 表示**：`ItemIn{holder, slot}`（名字暫定）。角色也是一種 holder，它的格子就是裝備欄。這正是 Bevy 的 Relationship 的形（來源側存一個目標、一個實體同一種關係只有一個＝物品只能在一個地方）。Kernel 現在只有 `ChildOf`／`Children` 一對，通用的關係入口是 RC 27 定了形、沒有實作。**不要拿 `ChildOf` 表示「在背包裡」**：它帶 Transform 的傳播與連帶刪除（死了東西會跟著被刪），意思不對（U2）。
3. **背包是 Server 權威，可見性用 net 現有的機制就做得到**：別人的背包裡的物品整個實體不給看（`PeerVisibility` 以實體為單位）；身上的裝備大家看得到，但彈匣剩幾發這種只給擁有者（C-VIS2 的私人 component，`Replication.hpp:457-461`、`:638-664`）。這和 Lyra（背包只給擁有者、裝備給所有人）、Unity（`SendToOwner`）、replicon（可見性範圍到 component）同一個分法。
4. **回滾只包每 tick 輸入會改的值**（推薦，U4）：目前拿哪一欄、換槍開始的 tick、彈匣剩幾發。撿起、丟下、搬動只在 Match 改，Client 不預測、照收到的直接寫。理由：預測裡增刪 component 或實體很難回滾正確（Unity 文件點名），而撿東西慢一個 RTT 是各家都接受的。
5. **最小可玩版本（I1＋I2）不用改 Engine**：撿起（E）、丟下（G）、兩把槍切換（1／2）。容器、背包整理那一批（I4）才需要 Engine 補兩件已點名的機制：Kernel 的通用關係（RC 27 (b)）＋網路複製關係時的先後（K8），以及 Client → Match 的可靠訊息（#361 Q6）。
6. **核心 PvP 也把那一把射釘槍變成武器實體**（推薦，U3）：開槍只有一條路。核心 PvP 的結果（位置、命中、血量）逐位元相同；權威 digest 的組成會變（彈藥從 `PlayerCombat` 搬到武器實體），照 #374 7.3 (a)／門宣告 9.3 的比法證明。

---

## 1. 現況（讀程式得到的）

| 項目 | 現況 | 出處 |
|---|---|---|
| 武器狀態 | 在玩家的 `PlayerCombat` 裡：`ammo`、`nextShotTick`、`reloadEndTick`、`lastShotTick`、`shots`。沒有武器實體 | `games/object_fps_pvp/include/object_fps_pvp/world/Components.hpp:54-69` |
| 開槍、換彈 | `UseWeapons` 讀 `PlayerInput`、寫 `PlayerCombat`；彈匣容量從 `GameRules` 讀 | `games/object_fps_pvp/src/world/PvpWorld.cpp:459-480` |
| 權威與複製的登記 | `PlayerBody`、`PlayerCombat`、`ShotRecord`、`HitRecord` 進 authority；`PlayerInput` 是輸入；全部 `Replicate`，沒有私人 component | `PvpWorld.cpp:1181-1218` |
| Client 的回滾 | Client 用 `StateRollbackStrategy` | `PvpWorld.cpp:1347-1351` |
| Kernel 的關係 | 只有 `ChildOf`（來源側）／`Children`（目標側、連帶刪除），Kernel 自己登記；通用入口 `Registrar::Relationship<R>`「只是形狀」（RC 27） | `engine/ecs/kernel/include/engine/kernel/Relationship.hpp:3-17`、`:34-42`；`docs/architecture/target/kernel.zh-Hant.md:122`、`:230` |
| 關係的網路複製 | net 不複製 `ChildOf`；父比子晚到要網路自己等（lightyear `PendingChildOf`），記成 K8 待辦 | `net.zh-Hant.md:276`；`kernel-relationship-declaration.zh-Hant.md:252-254` |
| 可見性 | `PeerVisibility`：以實體為單位（看不見就在 Client 刪）＋私人 component 以（實體, 型別）為單位；由遊戲每 tick 填 | `engine/modules/net/include/engine/net/Replication.hpp:450-485` |
| 複製的 component 數 | 最多 64 種 | `Replication.hpp:658` |
| 物理查詢 | 吃作者自己的 `Query` 與過濾函式；`Walls` 是「所有沒有 `ChildOf` 的 collider」 | `engine/modules/physics_3d/include/engine/physics_3d/SpatialQuery.hpp:16`、`:85-90`；`PvpWorld.cpp:281` |
| 第三人稱的槍 | 不是 `ChildOf`：骨頭不是實體，每幀算「身體 × `hand_r` × 掛點」寫進槍的 `Transform` | `weapon-model-research.zh-Hant.md:71`（`CharacterRig.cpp:349-351`） |
| 規模的估算 | 一場的物品約 200 個（master 的 11-01 需求，只當規模）；私人資料「背包 40 件」約 0.6 KB | `net-module-declaration.zh-Hant.md:100`；`authority-rollback-declaration.zh-Hant.md:249` |

---

## 2. 成熟實作怎麼拆（第 1 題）

### 2.1 例子

「撿起步槍」在各家是什麼：

- **flecs**：步槍是一個實體，`IsA` 一個步槍的 prefab（定義），撿起＝把 `ContainedBy` 指到玩家的背包實體。
- **Lyra**：地上的是一個 actor；撿起時 Server 在玩家的 InventoryManager 加一筆 entry（新建一個 ItemInstance），地上的 actor 刪掉；按 2 時 QuickBar 叫 EquipmentManager 生成步槍的 actor 掛到手上。
- **Unity Netcode 的範例**：步槍本身就是一個 ghost；撿起時把它加進玩家的 `GhostGroup`，讓它跟玩家一起送。
- **Godot**：步槍是一個節點，撿起＝`reparent` 到玩家的某個節點下。

### 2.2 各家怎麼做

| 家 | 物品是什麼 | 定義 vs 實例 | 物品在哪裡 | 裝備（身上） | 網路 | 查證 |
|---|---|---|---|---|---|---|
| **Bevy**（主參考） | 核心沒有背包。能用的機制：自訂 Relationship | — | 來源側 component 存一個目標，目標側存來源集合，由 hook 維護（`examples/ecs/relationships.rs:20-34`）；可以帶資料欄位（`crates/bevy_ecs/src/entity/clone_entities.rs:2401-2415` 的 `Source{#[relationship] target, data}`）；連帶刪除是選的（`linked_spawn`，`crates/bevy_ecs/src/relationship/mod.rs:82-94`）；也能一對一（`:66-79`） | — | 生態：replicon 的可見性範圍可以是整個實體、幾個 component、或「除了這幾個」（`bevy_replicon` `src/shared/replication/visibility.rs:331-338`），「只給擁有者」的例子 `:313-325`、「除了擁有者」的例子 `:186-205`；lightyear 的 `ControlledBy{#[relationship] owner, lifetime}` 是帶資料的關係（`lightyear` `crates/replication/replication/src/control.rs:50-60`），`ReplicateLike` 讓子實體照根實體的方式複製（`hierarchy.rs:405-430`） | 源碼 |
| **flecs** | 實體 | 定義＝prefab（`IsA`）：`Attack` 共用、`Health` 用 `set_auto_override` 複製到實例（每把劍自己的耐久）（`examples/cpp/game_mechanics/inventory_system/src/main.cpp:288-302`） | `ContainedBy` 關係，登記成 `Exclusive`：「一個物品只能被一個容器裝」（`:279-282`）；搬＝換目標（`:153-154`）；堆疊：同種且有 `Amount` 就加到目的地那一個、刪掉來源（`:133-150`）；玩家的 `Inventory` 關係指到它的容器實體（`:90-96`） | `Active` tag＝穿著／拿著（`:14`、`:113-120`） | — | 源碼 |
| **Lyra**（Unreal 範例專案） | `ULyraInventoryItemInstance`（UObject），每次加入都新建 | `ItemDefinition`＝顯示名＋一串 Fragment（`EquippableItem`→EquipmentDefinition、`SetStats`→初始 tag 數值如彈匣 30、`PickupIcon`、`QuickBarIcon`、`ReticleConfig`）；Instance 記「stat tag 的堆疊」（剩餘彈藥等） | `InventoryManagerComponent` 在 **Controller** 上，`FLyraInventoryList`（`FFastArraySerializer`）的一串 `FLyraInventoryEntry{Instance, StackCount}`；**沒有格子、沒有欄位、加入不會失敗**（大小限制只靠 QuickBar） | `EquipmentManagerComponent` 在 **Pawn** 上；EquipmentDefinition＝要生成的 actor＋掛點＋ability set；Instance 在裝上時建、卸下時毀，「裝卸之間不留狀態」；一次只能裝一把武器；`QuickBarComponent`（Controller）用 `SetActiveSlotIndex` 先卸再裝 | 背包只給 Server 與擁有者（Autonomous Proxy）；裝備給所有人 | 官方文件 [E2]、社群 [X-Inv][X-Eq][X-Int][G]；**源碼未確認** |
| **Unity Netcode for Entities** | 範例：FPS 玩家撿、丟、帶三把槍，**每把槍是一個 ghost** | ghost prefab＝定義 | 撿起＝把槍加進玩家的 `GhostGroup` buffer、槍加 `GhostChildEntity`；丟下＝反過來（`Documentation~/ghost-groups.md:38-80`）。理由：不分組時別人會看到「手的動畫先到、槍還在地上」、遠處的人看起來空手、`HoldingPlayer` 指到已刪的實體（`:55-63`） | 「假的父子」：槍照手的位置擺（`:42`） | 每個 component 可設 `SendToOwner`／`SendToNonOwner`／`All`（`Runtime/Authoring/GhostModifiers.cs:82-93`、`GhostComponentAttribute.cs:30`）；`DynamicBuffer` 可複製（`Documentation~/ghostfield-synchronize.md:49-60`）；預測中的結構變更（增刪 component）可能讓修正套不上（`Documentation~/prediction-smoothing.md:43`） | 源碼＋官方文件 |
| **Godot** | 核心沒有背包。慣例：物品是節點或 `Resource` | 定義＝`Resource`（`.tres`），實例＝`duplicate` 或 `resource_local_to_scene`（`core/io/resource.h:88`、`:154-158`） | 節點樹：`reparent`（`scene/main/node.h:547`）。一個節點只有一個父，父子同時表示位置與所屬 | 同左（掛到骨頭節點） | `MultiplayerSynchronizer` 照 Peer 設可見（`modules/multiplayer/multiplayer_synchronizer.h:110-115`） | 源碼；「慣例」是推論 |
| **塔科夫**（格子背包的參考之一） | 物品各有 W×H 大小，可以轉 90°（R） | — | 背包、胸掛各有自己的格子，大小依裝備而不同；安全箱有自己的欄位，死了也保留 | 固定的裝備欄（主武器、手槍、護甲、胸掛、背包、安全箱…） | — | **公開資料**：媒體與攻略站摘要（[PC Gamer](https://www.pcgamer.com/escape-from-tarkov-is-a-compelling-game-about-gear-and-how-we-store-it)、[Twinfinite](https://twinfinite.net/guides/escape-from-tarkov-rotate-items/)、[HandWiki](https://handwiki.org/wiki/Software:Escape_from_Tarkov)）；欄位清單、格子能不能套娃、重量的門檻**未確認** |

Lyra 的出處：
- [E2] Epic 官方：<https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-inventory-and-equipment-in-unreal-engine>
- [X-Inv]／[X-Eq]／[X-Int]：<https://x157.github.io/UE5/LyraStarterGame/Inventory/>、`/Equipment/`、`/Interactions/`
- [G] Garashka「Fixing Lyra's Inventory System」：<https://garashka.github.io/LyraDocs/lyra/fixing-inventory-system>（Lyra 的背包原型有幾處沒做完：撿起時拿錯對象、listen server 上 UI 不更新、堆疊不會合併）

### 2.3 為什麼這樣設計：取能力，不取技術債

| 能力（取） | 誰這樣做、為什麼 | GYO 怎麼用 |
|---|---|---|
| **定義與實例分開** | 每一家都分。定義是共用的資料（傷害、大小、圖示），實例是這一把自己的狀態（耐久、彈匣） | 定義＝資料檔，進內容比對；實例＝實體上的 component |
| **定義由「片段」組成** | Lyra 的 Fragment、flecs 的 prefab 上的 component：一件物品是不是武器、是不是容器、能不能穿，由有沒有那個片段決定，不是由一個 enum | 和 scene 檔「物件屬於哪個玩法組＋那個組自己的資料」、#367 的 `SetReader` 同一種：定義檔裡每個區塊由對應玩法組的 reader 讀（第 3.1 節） |
| **物品只能在一個地方** | flecs `Exclusive`；Bevy 一個實體同一種關係只有一個來源側 component；Godot 一個父 | 用「物品身上的一個 component 指到 holder」的形，天生只有一個位置 |
| **「擁有什麼」和「身上拿著什麼」分開** | Lyra 把背包（Controller、只給擁有者）、裝備（Pawn、給所有人）、QuickBar（哪一欄在用）分三個：可見性、生命週期、表現都不同（[E2]） | 不分三個 component，分的是**可見性與表現**：同一個實體，在背包裡就藏起來，在裝備欄就公開，彈匣數私人（第 4 節） |
| **撿起的東西跟著人一起送** | Unity 的 GhostGroup 解決「手先到、槍後到」 | GYO 的 net 每個 tick 照實體送，同一 tick 的結構變更依序套用；撿起是 Match 在同一步改玩家與物品，Client 在同一個 tick 收到兩邊（第 4.1 節） |

| 技術債（不取） | 誰 | 為什麼不取 |
|---|---|---|
| 背包放 Controller、裝備放 Pawn | Lyra | x157 指出 AI 沒有 Controller 的背包可看、他改成放 Pawn（[X-Inv]）。GYO 沒有 Controller；怪物也會帶東西，放在角色實體上 |
| 加入不會失敗、沒有容量 | Lyra | 撤離射擊的核心就是容量取捨；容量規則放遊戲（第 3.2 節） |
| 每次裝上都新建 Instance、「裝卸之間不留狀態」 | Lyra EquipmentInstance | 換槍不能讓彈匣數歸零；GYO 的武器實體一直在，狀態留在它身上 |
| 用父子（`ChildOf`／節點樹）表示「在背包裡」 | Godot 慣例 | 父子帶 Transform 傳播與連帶刪除；死了要掉東西，不能跟著刪（Kernel 的關係宣告已點名「掉落武器：先拆再刪」，`kernel-relationship-declaration.zh-Hant.md:24`、`:59`） |
| 「假的父子」：每幀照手擺槍 | Unity 範例 | GYO 第三人稱已經這樣做（骨頭不是實體）。這是表現，不是所屬；所屬另外記 |

---

## 3. 資料模型（第 2 題）

### 3.1 物品定義（資料）與物品實例（實體）

**例子**：步槍的定義寫「大小 4×2、重 3.4 kg、是武器（數值在 `weapons/rifle/weapon.json`）、彈匣 30」；地上那一把步槍的實例寫「耐久 87%、彈匣剩 12」。

**前提：這是作者能處理的。**定義檔的格式、欄位、哪幾種片段都是遊戲的；Engine 只讀 JSON、算 hash。

推薦的形（名字都是暫定，實作批照命名規範定）：

```text
items/<item>/item.json          物品定義（新）
{
  "version": 1,
  "id": "rifle_ak",
  "size": [4, 2],               grid 批（I5）才讀
  "weight": 3.4,                weight 批（I5）才讀
  "slots": ["primary"],         能放哪種裝備欄
  "weapon": "weapons/rifle_ak/weapon.json",   W1 的武器定義，原樣引用
  "container": { … },           容器批（I4）：這件物品本身是容器（背包、胸掛）
  "armor": { … },               護甲批（I6）：護哪些部位
  "stack": 60                   可堆疊的上限（彈藥、錢）
}
```

- 模式規則表多一個 `items`（定義檔的清單），和 W1 的 `weapons` 並列；進 `rulesDigest`（模式配置宣告第 3.3 節的同一條路）。
- **W1 不用改**：`weapon.json` 仍是武器的規則數值與兩個視角的索引（`weapon-model-research.zh-Hant.md:177-200`）；物品定義用路徑引用它，和 Lyra 的 `EquippableItem` 片段引用 EquipmentDefinition 同形。核心 PvP 沒開背包時，`weapons` 照 W1 的樣子用。
- 每個區塊（`weapon`、`container`、`armor`…）由那個玩法組的 reader 讀；組沒開，區塊照樣解析、不放（scene 檔第 5.2 節的同一個做法）。

實例：

| component（暫定名） | 內容 | 誰有 | 決策資料？ | 複製 |
|---|---|---|---|---|
| `Item` | 定義的索引（模式表裡的第幾個，像 L1 (1c) 部位用表索引上 wire） | 每個物品 | 是 | 公開 |
| `Magazine` | 剩幾發 | 武器 | 是（回滾，U4） | **私人**（擁有者） |
| `Stack` | 數量 | 可堆疊的物品 | 是 | 跟著物品的可見性 |
| `Durability` | 耐久 | 有耐久的（護甲、武器） | 是 | 跟著物品的可見性 |
| `WeaponTiming` | `nextShotTick`、`reloadEndTick`（從 `PlayerCombat` 搬來） | 武器 | 是（回滾） | 私人 |

### 3.2 物品是實體，還是元件裡的資料（U1）

**例子**：背包裡 30 件東西，每件一個實體，還是背包身上一個 component 裝 30 列？

| | (a) 每個物品一個實體（**推薦**） | (b) 容器裡的是資料列；地上與身上的才是實體 |
|---|---|---|
| 誰這樣做 | flecs 範例、Unity 的 FPS 例子（槍是 ghost）、Bevy 慣例（一切都是實體）、Godot（節點） | Lyra（背包是一串 entry）、Unity 的 `DynamicBuffer` |
| 撿起、丟下、裝上 | 同一個實體換 component、編號不變，實例狀態不搬 | 在「實體」和「資料列」之間轉，每次轉都要把耐久、彈匣抄過去；轉的那一步要 spawn 或 despawn |
| 套娃（胸掛裝在背包裡、彈匣裡有子彈、槍上有配件） | 自然：holder 也是物品 | 資料列裡要再放資料列，或改成實體 |
| 可見性 | net 現有的實體可見性＋私人 component 就夠 | 一個 component 整列給或不給；部分給（搜屍體時看到別人背包的某一格）做不到 |
| 回滾 | 值的變化照 component 回滾 | 一個大 component，任何一格變了整個比對與回滾 |
| 代價 | 實體數多（一場約 200 個物品，第 1 節）；可見性規則要逐個物品算 | 列的格式固定，每加一種實例狀態就要改列的格式 |

推薦 (a)：撤離射擊一定會有套娃與「搜別人的容器」，(b) 兩件都要繞路。200 個實體對 GYO 不是負擔（權威的預算估算裡私人資料只有約 0.6 KB／40 件，`authority-rollback-declaration.zh-Hant.md:249`）。

### 3.3 物品在哪裡：一個「來源存目標」的 component（U2）

**例子**：步槍在玩家的第二武器欄 → `ItemIn{holder = 玩家, slot = 第二武器}`；子彈在背包的第 7 格 → `ItemIn{holder = 背包, slot = 7}`；地上的 → 沒有 `ItemIn`，有 `OnGround`（位置、朝向）。

- **角色也是 holder**：角色的格子就是裝備欄（主武器、副武器、手槍、護甲、頭盔、胸掛、背包…）。「背包本身也是裝備」就自然成立：背包是一件 `ItemIn{玩家, 背包欄}` 的物品，它自己又是 holder。
- **裝備欄的清單放角色資料**（怪物和人不同：怪物可能沒有手槍欄），**哪些欄能放什麼放物品定義的 `slots`**。這一場開不開背包由模式配置決定。
- **「目前拿哪一把」不是位置**：`ActiveItem{slot, since}` 在玩家身上（換槍開始的 tick，G2 的 `Hide`→`Draw` 從它推算）。

這個形就是 Bevy 的 Relationship（來源側存一個目標＋資料，像 lightyear 的 `ControlledBy{owner, lifetime}`）。差別只在**誰維護反向的集合**（「這個背包裡有哪些東西」）：

| | (a) Kernel 通用關係（RC 27 (b)）現在就開 | (b) 遊戲的一般 component，形和 (a) 相同；(a) 排在容器批 I4 之前（**推薦**） | (c) 分兩套：角色身上一個 `Loadout` 陣列＋容器身上一個內容清單 |
|---|---|---|---|
| 反向集合 | Kernel 維護（同 `Children`）；目標不在時那一筆不成立、留 `RejectedRelationship` | 沒有；要「背包裡有什麼」時 Query 全部 `ItemIn` 再篩（200 個、8 個玩家，便宜） | 兩邊都是作者維護 |
| 「只在一個地方」 | 天生 | 天生 | **不天生**：物品可能同時在 `Loadout` 和某個清單裡，要靠作者寫對 |
| Engine 的工作 | Kernel 一批（通用入口、回滾與讀檔照 DI-KS 33）＋net 一批（複製關係、目標晚到時等，K8） | 現在不用；I4 前做 (a) | 不用 |
| 和別家 | Bevy、flecs | 同 (a) 的資料形 | Lyra、Unity |
| 換到 (a) 的成本 | — | 資料與 wire 不變，只把型別改登記成關係、刪掉遊戲自己的篩選 | 要改資料模型 |

推薦 (b)：最小可玩版本只有「兩把槍在兩個欄位」，反向集合用不到；而 (a) 要先等 Kernel 與 net 兩批，會擋住 I1、I2。**這不是「之後可以放寬」的保守選項**（`v8-arch-gyo2-mechanism-not-conservative`）：資料形一開始就是關係的形，(a) 直接排進批次表、在容器批之前做，觸發條件是「有套娃的容器」（RC 27 (b) 的第一個點名使用者）。

不拿 `ChildOf` 的理由（三條都會壞）：
1. `Children` 帶連帶刪除（`Relationship.hpp:8-9`）：玩家實體被刪時身上的東西跟著刪，屍體掉落就做不到。
2. `ChildOf` 是 Transform 的階層（`engine/modules/transform/`）：背包裡的東西沒有位置，不該進傳播。
3. 遊戲的 `Walls` 用 `Without<ChildOf>` 篩牆（`PvpWorld.cpp:281`），`ChildOf` 已經被用來表示「掛在別的東西上」；再加一個意思就是一詞兩義。

### 3.4 背包的形狀：欄位、格子、重量

**例子**：第一版背包只有「8 格、每格一件」；格子版是「6×8 的格子，步槍佔 4×2、可以轉」；重量是「超過 30 kg 跑得慢」。

**前提：這全部是 Object_PvP 的規則（作者能處理）。**Engine 不知道「格子」「重量」。

| 形狀 | 規則 | 取捨 | 誰 |
|---|---|---|---|
| 欄位（slot） | 每格一件；裝備欄也是欄位 | 最簡單；放不放得下只看「有沒有空格＋這格收不收這種」 | Lyra QuickBar、多數 FPS 的武器欄 |
| 格子（grid） | 物品有 W×H、可轉；容器有一個或多個格子；`slot` 換成 (x, y, 轉了沒) | 撤離射擊的手感核心（空間取捨）；判定要做矩形放置，UI 要能拖、能轉 | 塔科夫（公開資料） |
| 重量 | 不擋撿起，只改移動 | 一個加總值；接技能研究的 `EffectiveMovement(基準, 裝備, 修飾, now)`（技能研究第 4 節，`skill-research.zh-Hant.md:200-210`）——移動速度只有一個入口 | 塔科夫（公開資料：超重時體力掉得快、走得慢） |

推薦（U5）：I4 先做欄位（容量＝格數），I5 再做格子＋轉向＋重量。欄位版的 `ItemIn.slot` 是一個整數，格子版是 (x, y, r)；格子在 I5 改 `slot` 的編碼（Codec 升版），規則 System 換一個。核心 PvP 都不開。

### 3.5 裝備欄位與護甲

**例子**：玩家穿一件「護胸、護腹」的護甲，被打到胸時先扣護甲耐久再扣血。

- 裝備欄＝角色的 holder 格子（3.3）。武器欄的物品是武器實體；護甲欄的物品有 `Durability`。
- 護甲定義的 `armor` 區塊寫「護哪些規則部位」：部位的名字來自 L1 (1c) 的模式表，所以護甲直接用名字引用（`limb-damage-research.zh-Hant.md:276`）。
- 減傷插在「(部位, 數值) → 血池」的哪一步、穿透怎麼算，**這份研究不定**，和 G6（肢體傷害）、L4 一起定（`HANDOFF.md:422` 的待定）。這裡只確定資料能接上：護甲是物品實體，減傷 System 讀「這個角色的護甲欄裡的物品」。

### 3.6 三種狀態的轉換

| 從 → 到 | 例子 | 誰發起 | Match 做什麼（同一個 System，第 3.7 節） |
|---|---|---|---|
| 地上 → 身上／背包 | 按 E 撿步槍 | 每 tick 輸入：E（交互骨架發 `Interacted`） | 目標有 `Item`＋`OnGround` → 照規則找空位 → 拿掉 `OnGround`、collider、`Transform`、`Interactable`；放 `ItemIn{玩家, 位}` |
| 身上 → 地上 | 按 G 丟手上的槍 | 每 tick 輸入：G（`presses` 一個位元） | 拿掉 `ItemIn`；放 `OnGround`（腳前的固定位置，兩端同算）、collider、`Interactable` |
| 背包 ↔ 身上、容器 ↔ 容器 | 拖子彈進胸掛；把步槍從背包換到主武器欄 | 可靠訊息（I4 起；不綁 tick、不預測） | 驗證「來源還在、目的地收這種、放得下」→ 改 `ItemIn` |
| 身上 → 屍體 | 被打死 | Match 的死亡 System | 玩家實體變成可搜的 holder（或另生成一個屍體實體），物品的 `ItemIn` 不用動或改指屍體；**不刪** |
| 背包 → 帶出局 | 撤離成功 | 撤離規則 | 遊戲把「這個玩家能帶走的」寫進局外的資料（存檔或帳號服務），不在這份研究 |

**地上的物品不擋人**：撿起要打得到（交互用射線），但不能當牆。physics 的查詢吃作者自己的 `Query`（`SpatialQuery.hpp:85-90`），所以 `Walls` 加 `Without<OnGround>`、交互的 Query 另外收 `OnGround`——遊戲層的改動，Engine 不用加碰撞圖層。

**scene 裡的物品**（U6）：門宣告把「可撿的東西」放進 Scene（`HANDOFF.md:452`）。但撤離射擊的戰利品通常是**隨機**的，Client 不該事先知道哪裡有什麼。推薦：scene 裡放的是「戰利品點」（位置＋戰利品表的名字），**Match 開局擲骰、生成物品實體**（Server 配號、照一般實體複製）；固定的物品（任務道具）才用門宣告的 `Placed`。對照：Unity 的 pre-spawned ghost 是「場景裡的東西兩端都有」，適合門、不適合隨機戰利品。

### 3.7 同一步兩個人撿同一把（Kernel 的延後套用）

**例子**：A、B 同一步對同一把步槍按 E。

Kernel 的結構變更延後到同步點套用（DI-KS 9）：`PickUpItems` 處理 A 的 `Interacted` 時記下「拿掉 `OnGround`」，處理 B 時步槍**看起來還在地上**。所以所有改位置的 System 合成一個（撿、丟、搬都經它），它自己記「這一步已經被拿走的」；`Interacted` 照 EntityId 由小到大（門宣告 2.3），A 先拿到、B 什麼都沒發生。這是遊戲的漏斗，不是 Engine 的事。

---

## 4. 網路與權威（第 4 題）

### 4.1 Server 權威、誰看得到什麼

**例子**：A 的背包裡有手雷。A 看得到、B 看不到；A 背著的步槍 B 看得到；步槍剩幾發只有 A 知道；B 打開 A 的屍體時才看得到屍體裡的東西。

| 物品在哪 | 擁有者 | 其他人 | GYO 的機制 |
|---|---|---|---|
| 地上 | 看得到 | 看得到（照一般的視野規則） | 一般實體 |
| 裝備欄（身上） | 看得到 | 看得到實體與 `Item`（才能畫出背的是哪把） | 一般實體 |
| 武器的 `Magazine`、`WeaponTiming` | 看得到 | **看不到** | 私人 component：`Replicate<T>(…, {.privateComponent = true})`＋`ShowPrivate<T>(擁有者)`（`Replication.hpp:638-664`、`:474-482`） |
| 背包、胸掛裡的東西 | 看得到 | **整個實體看不到** | `PeerVisibility::Hide`（`Replication.hpp:466-469`） |
| 屍體或箱子裡的東西 | — | 正在搜的人看得到 | 遊戲的可見性 System 加「正在搜這個 holder」 |

- 誰看得到由遊戲每 tick 填 `PeerVisibility`（現有的 `ShowPlayers` 就是這樣，門宣告 3.2 最後一點）。規則是「物品的最外層 holder 是誰、這個 Peer 和它的關係」，要沿 `ItemIn` 往上走到最外層。
- 對照：Lyra 背包只給擁有者、裝備給所有人（[E2]）；Unity `SendToOwner`（`GhostModifiers.cs:86-87`）；replicon 的 `VisibilityScope::Components`（`visibility.rs:331-338`）。GYO 現有的機制已經涵蓋這三種，**這一題 Engine 不用加東西**。
- 看不見再看見：一般實體在 Client 被刪、回到視野時整個重送（`Replication.hpp:450-453`）。物品從背包拿到手上，對 B 來說就是「一個新實體出現」。

**撿起時「手先到、槍後到」**（Unity GhostGroup 要解決的）：Match 在同一步改玩家的 `ActiveItem` 和物品的 `ItemIn`，net 同一個 tick 的結構與值一起送、依序套用。只有超出每 tick 的位元組預算時（`ValueBytesPerTick`，`Replication.hpp:654`），值可能分到兩個 tick。到時候要讓 holder 和它的物品同優先度（lightyear 的 `ReplicateLike` 就是做這個）——等量到再說，現在不加。

### 4.2 回滾要不要包含背包（U4）

**例子**：A 按 2 換步槍，A 的畫面要馬上開始收槍動畫（預測）；A 按 E 撿東西，等 Match 回覆再出現在手上可以接受嗎？

| | (a) 只回滾每 tick 輸入會改的值（**推薦**） | (b) 全部回滾，包括撿、丟、搬 | (c) 都不預測 |
|---|---|---|---|
| 回滾的 | `ActiveItem`、`Magazine`、`WeaponTiming`（以後：快捷用藥改的 `Stack`） | (a)＋`ItemIn`、`OnGround` 的增刪 | 沒有 |
| Client 的感覺 | 換槍、開槍、換彈馬上；撿起、丟下慢一個 RTT | 全部馬上 | 換槍也慢一個 RTT |
| 難處 | 小：都是值的變化，和現在的 `PlayerCombat` 同一種 | 預測中增刪 component：Unity 寫明會讓修正套不上（`prediction-smoothing.md:43`）；兩人搶同一把時 Client 預測錯要把實體「還回去」 | 換槍的延遲在 PvP 不可接受 |
| 別家 | Lyra 撿起只在 Server 加（`HasAuthority()`，[G]） | Unity 的 predicted spawning 要另一套配對（`ghost-spawning.md:40-50`） | — |

推薦 (a)。撿起、丟下、搬動在 Client 照收到的直接寫（K3 的「不回滾的直接寫」，門宣告 4.3）。對照權威回滾宣告：私人資料的預算估算已經含背包（`authority-rollback-declaration.zh-Hant.md:249`），(a) 只回滾其中一小部分。

**換彈要扣背包裡的子彈時**（塔科夫式；核心 PvP 是無限備彈）：扣 `Stack` 的值可以預測；扣到 0 要刪實體，那一步留給 Match（預測裡不刪）。這條等有「備彈來自背包」的模式配置再寫進那一批。

### 4.3 輸入走哪條通道

照 D2 調查的判準「綁不綁某一步、要不要預測、內容大不大」（`interact-d2-investigation.zh-Hant.md:74`）：

| 動作 | 通道 | 這一批要做的 |
|---|---|---|
| 撿起（E） | 每 tick 輸入（已有：`kPressInteract`） | 不用 |
| 丟下（G） | 每 tick 輸入：`presses` 加一個位元 | I2 |
| 換槍（1／2／滾輪） | 每 tick 輸入：要一個值（第幾欄），`PlayerInput` 加一個 `std::uint8_t select`（0＝沒按） | I2；Codec 和 G2、G6 同一段一起升（`animation-layers-ik-stance-research.zh-Hant.md:324`） |
| 拖曳整理、在容器之間搬、拆堆疊 | **Client → Match 的可靠訊息**（現在沒有，#361 Q6） | I3（Engine）、I4 用 |

---

## 5. 和既有決定的關係（第 3 題）

| 既有決定 | 關係 | 要改它嗎 |
|---|---|---|
| **W1** 武器欄位搬進武器定義 | 物品定義的 `weapon` 引用 `weapon.json`；W1 照原樣做 | 不改。W1 做完，I1 再把「武器狀態」從 `PlayerCombat` 搬到武器實體 |
| **W3 `loadout_switch`**（模式配置宣告第 6 節） | I2 的「兩把槍切換」就是它：`ActiveItem`＋`select` | 不改名字與方向。W3 原本寫「武器實體、切換的 component」，和本研究同形 |
| **W4 撿丟** | I2 的撿起、丟下 | 併進 I2（和 W3 同一批：撿到第二把才有得切） |
| **武器模型 U1 (a)、`Draw`／`Hide`** | `ActiveItem.since` 是換槍開始的 tick，G2 從它推算「舊 `Hide` 播完 → 換網格與片段來源 → 新 `Draw`」 | 不改。第一人稱畫哪把槍＝`ActiveItem` 指的那一欄的武器定義 |
| **部位傷害 L1 (1c)、(部位, 數值)** | 護甲用部位的名字；減傷的位置等 G6／L4 | 不改。I6 和 G6 同時定 |
| **模式配置 (R1)** | 新的 component 全部固定登記、只在組開著時放；核心 PvP 不開背包的組 | 不改。新組：`pickups`、`loadout_switch`、`containers`、`grid`、`weight`、`armor`、`loot` |
| **arena 規則拆出（#379）** | 背包的規則（容量、重量門檻、死亡掉落）都放模式配置，不放 arena；戰利品點放 scene | 不改 |
| **門與交互骨架（#375）** | 撿起＝交互的一種種類（目標有 `Item`＋`OnGround`）；`Interactable` 是 marker | 不改。I2 只新增一個讀 `Interacted` 的 System，交互骨架不動 |
| **權威回滾宣告** | 私人資料的預算已含背包；回滾只包每 tick 的值（U4 (a)） | 不改 |
| **技能研究 K4**（第一批技能由模式固定，「玩家自選要接存檔或背包」） | 背包可以成為技能的來源（裝備帶的被動），走同一個 `EffectiveMovement` | 不改；技能線停在研究（`HANDOFF.md:457`） |

---

## 6. 歸屬：Engine 的機制 vs Object_PvP 的規則（第 5 題）

| 需求 | 前提 | 歸屬 | 現況 |
|---|---|---|---|
| 物品是實體、實例狀態是 component | 作者能處理（ECS 本來就給） | Object_PvP | 有 |
| 「物品在哪」的來源側 component | 作者能處理（一般 component） | Object_PvP（I1、I2） | 有 |
| 反向集合由 Kernel 維護、一個實體一種關係只有一個（通用關係） | **Engine 要解決**：反向集合要和每一次套用、回滾、讀檔一致（DI-KS 30、33），作者做的話每個產品各寫一份、還跟不上 `Restore` | Kernel（RC 27 (b)） | 形定了、沒實作 |
| 複製關係時目標晚到要等（K8） | **Engine 要解決**：只有 net 知道兩筆的先後 | net | 待辦 |
| 每個 Peer 看哪些實體、哪些私人 component | Engine 已有 | net（C-VIS1、C-VIS2） | 有 |
| 「誰看得到哪個物品」的規則 | 作者能處理 | Object_PvP | — |
| Client → Match 的可靠離散訊息 | **Engine 要解決**：遊戲不能自己開通道（`NetModule.cpp:170-171` 只開兩條自用） | net（#361 Q6） | 沒有 |
| 兩端放好的實體（固定物品） | Engine 已有（#375 的 `Placed`，合併後） | net | #375 進行中 |
| 物理查詢排除地上的物品 | 作者能處理（自己的 `Query` 與過濾） | Object_PvP | 有 |
| 物品定義的格式、片段、讀檔 | 作者能處理 | Object_PvP | — |
| 欄位、格子、轉向、重量、堆疊、裝備欄清單、死亡掉落、撤離保留、戰利品表 | 作者能處理（各家都是遊戲的規則） | Object_PvP | — |
| 背包 UI（拖、轉、提示） | 作者能處理（用 ui 的機制） | Object_PvP 的 Client | — |

Engine 這一側出現的詞只有「關係（relationship）」「私人 component」「可見性」「可靠訊息」，沒有「背包」「裝備」「撤離」「物品」。

---

## 7. 分批建議（第 6 題）

最小可玩版本＝**I1＋I2**：撿起、丟下、兩把槍切換。不用改 Engine。

```text
背包與裝備
├─ I1 high｜武器實體化：核心 PvP 的射釘槍也變成武器實體（物品定義、Item、Magazine、WeaponTiming）
├─ I2 high｜撿起、丟下、兩把槍切換（=W3 的 sim＋W4）：ItemIn、OnGround、ActiveItem、select、G 鍵
├─ I3 high｜Engine：通用關係（RC 27 (b)）＋複製關係（K8）＋Client→Match 可靠訊息（#361 Q6）
├─ I4 high｜容器：背包與胸掛是可裝東西的裝備（欄位版）、私人可見性、搜屍體、背包 UI
├─ I5 high｜格子＋轉向＋重量（接 EffectiveMovement）
├─ I6 high｜護甲（和 G6／L4 一起定減傷的位置）
└─ I7 high｜戰利品點（scene）＋Match 擲骰、死亡掉落、撤離保留
```

| 批 | 內容 | 上段／下段（根據） | 模型／檔位（實作者、審查者、檢證者；純淨 Code Review） | 前置 |
|---|---|---|---|---|
| I1 `v8-product-pvp-item-entity` | 物品定義檔與模式表 `items`；武器實體（玩家生成時一起生成）；`PlayerCombat` 的彈藥與時序搬到武器實體；`UseWeapons` 改讀武器實體；核心 PvP 的證明照 #374 7.3 (a)：扣掉搬動的項目，權威 digest 每步相同，`PlayerCombat.ammo` ↔ `Magazine` 逐步對照 | **上段**：資料格式（定義檔、模式表）、交換內容（新 component 的 Codec）、核心 PvP 的 digest 組成 | Opus／high；Opus／high；Opus／high；Fable／high | W1 |
| I2 `v8-product-pvp-pickup-drop-switch` | `ItemIn`、`OnGround`、`ActiveItem`；`PlayerInput` 加 `select` 與丟下的位元；`PickUpItems` 讀 `Interacted`；丟下；兩把槍切換的 sim（G2 的動畫接它）；`Walls` 排除地上的物品；死亡時的掉落規則（模式配置）；可見性：`Magazine` 私人 | **上段**：wire（`PlayerInput` Codec）、決定性（同一步搶同一把）、權威 | 同上 | I1、#375 合併、G2 同段 |
| I3 `v8-arch-gyo2-relationship-general`＋net 兩件 | Kernel 的通用關係入口（RC 27 (b)：Bevy 的 Relationship 帶資料、連帶刪除可選、一對一可選）；net 複製關係、目標晚到時等（K8）；遊戲能登記自己的 Client → Match 可靠訊息 | **上段**：Kernel 的公開介面、wire、DI-KS 改條文 | 同上 | 可以和 I2 並行 |
| I4 `v8-product-pvp-containers` | 背包、胸掛（物品定義的 `container`）；欄位版容量；`ItemIn` 改登記成 I3 的關係；可見性 System（背包裡的整個藏、正在搜的人看得到）；搜屍體；搬動走可靠訊息；背包 UI | **上段**：資料格式、可見性規則、wire（UI 的部分是下段，可以拆成子批由 Sonnet 以上做，但 UI 讀的資料先在上段定） | 同上 | I2、I3 |
| I5 `v8-product-pvp-inventory-grid` | `slot` 改成 (x, y, r)（Codec 升版）；矩形放置規則；轉向；重量加總接 `EffectiveMovement` | **上段**：資料格式、Codec | 同上 | I4；重量要等技能研究的 `EffectiveMovement` 定案或在這一批先做只有重量一個來源的版本 |
| I6 `v8-product-pvp-armor` | 護甲定義的 `armor`；減傷的位置；耐久 | **上段**：判定 | 同上 | I4、G6、L4 |
| I7 `v8-product-pvp-loot` | scene 的戰利品點、戰利品表（模式配置）、Match 擲骰（只在 Match）；撤離保留（局外資料的介面另開研究） | **上段**：scene 格式、內容比對（戰利品表進 `rulesDigest`、擲骰結果不進） | 同上 | I4、#375 |

- 模型與檔位照 `agents/navigator.md` 與 `agents/effort-gearbox.md`：上段不用小模型；ultracode 與純淨 Code Review 以外的實作、審查用 Opus；檔位 high（主力）。決定性的局部（I2 的搶同一把、I3 的關係維護）要不要升 xhigh，開那一批時照使用者 2026-11-19 的用法（xhigh 只在需要調查時用）判斷。
- **I1 要先確認一件事**：武器實體在玩家之後生成，玩家的 `EntityId` 才不會變（`ShotRecord.target`、`HitRecord.attacker` 存的是玩家編號）。權威 digest 不含實體編號（門宣告 4.4），但命中紀錄的值含。
- Codec 的升版照 G2、G6「交換內容在同一段一起改」（`animation-layers-ik-stance-research.zh-Hant.md:324`）：I2 的 `select` 和 G2 的切換狀態同一段。

---

## 8. 推薦總結

1. 物品＝實體；定義＝資料檔（片段由各玩法組讀）；實例狀態＝component。
2. 位置＝物品身上一個「來源存目標」的 component，角色的格子就是裝備欄；先用遊戲的一般 component，容器批之前換成 Kernel 的通用關係。不用 `ChildOf`。
3. 可見性用現有的 net 機制；Engine 這一題不加東西。
4. 回滾只包每 tick 輸入會改的值；撿、丟、搬由 Match 改、Client 不預測。
5. 最小可玩版本 I1＋I2 不改 Engine；I3 是 Engine 的三件已點名機制，在 I4 之前。
6. 核心 PvP 的射釘槍也變成武器實體，開槍只有一條路。

---

## 9. 對主會話初步看法的驗證

| 主會話的看法（brief） | 查到的 | 結論 |
|---|---|---|
| 「通用的容器／關係可能早就有了，先查 Kernel」 | 只有 `ChildOf`／`Children`；通用入口只定了形（RC 27） | 部分成立：形有了，實作沒有；而且 `ChildOf` 的意思不能拿來用（3.3） |
| 「每個玩家只複製自己的背包，別人只看得到身上的裝備」 | net 的實體可見性＋私人 component 已經做得到 | 成立，不用改 Engine |
| 「撿起物品＝一種交互」 | 交互骨架的種類由目標的 component 表示，加一個讀 `Interacted` 的 System 就好 | 成立，交互骨架不動 |
| 「背包由模式配置決定開不開，核心 PvP 模式不開」 | 背包、容器、格子的組都不開；但武器實體（I1）在核心 PvP 也有 | 部分修正：U3 |

---

## 10. 要使用者決定的點

| 代號 | 問題 | 比過的選項 | 推薦 | 出處 |
|---|---|---|---|---|
| **U1** | 物品是不是實體？ | (a) 每個物品一個實體；(b) 容器裡的是資料列、地上與身上的才是實體 | **(a)**：撤離射擊一定有套娃與搜別人的容器，(b) 兩件都要繞路；三種狀態轉換時編號與實例狀態不變 | 第 3.2 節；flecs `inventory_system/src/main.cpp:279-302`；Unity `ghost-groups.md:38-80`；Lyra [E2] |
| **U2** | 「物品在哪」用什麼表示？ | (a) Kernel 通用關係現在就開；(b) 遊戲的一般 component、形和 (a) 相同，(a) 排在容器批 I4 之前；(c) `Loadout` 陣列＋容器清單兩套 | **(b)**：最小版本用不到反向集合，(a) 會擋住 I1、I2；資料形一開始就是關係，(a) 直接排進批次表（不是「以後再說」）；(c) 不保證「只在一個地方」 | 第 3.3 節；Bevy `relationship/mod.rs:66-94`、`clone_entities.rs:2401-2415`；Kernel RC 27 |
| **U3** | 核心 PvP 的射釘槍也變成武器實體嗎？ | (a) 是，開槍只有一條路，核心 PvP 的 digest 組成變、結果不變（照 #374 7.3 (a) 證明）；(b) 否，背包開著才有武器實體，`PlayerCombat` 的彈藥留給核心 PvP（兩條開槍的路） | **(a)**：兩條路以後每改一次開槍都要改兩份；`v8-arch-gyo2-weapon-system-split` 已寫「武器 entity 掛玩家下自帶彈藥冷卻」；門宣告改 `PlayerInput` 也是同一種證明 | 第 7 節 I1；`HANDOFF.md:415`；門宣告 9.3 |
| **U4** | 回滾包不包含背包？ | (a) 只回滾每 tick 輸入改的值（`ActiveItem`、`Magazine`、`WeaponTiming`）；(b) 全部回滾、撿起也預測；(c) 都不預測 | **(a)**：換槍、開槍要馬上；撿起慢一個 RTT 各家都接受；預測裡增刪 component 難回滾正確 | 第 4.2 節；Unity `prediction-smoothing.md:43`、`ghost-spawning.md:40-50`；Lyra [G] |
| **U5** | 第一個容器的形狀？ | (a) 先欄位（I4），格子＋轉向＋重量在 I5；(b) 一開始就做格子 | **(a)**：欄位版先把容器、可見性、搬動、UI 的骨架跑通；格子只改 `slot` 的編碼與一個規則 System | 第 3.4 節；塔科夫（公開資料，未確認細節） |
| **U6** | scene 裡的物品怎麼生成？ | (a) scene 放「戰利品點」，Match 開局擲骰生成（Server 配號），固定物品才用 `Placed`；(b) 全部照門的做法用 `Placed`，兩端都從 scene 放 | **(a)**：隨機戰利品不能讓 Client 事先知道；`Placed` 適合門那種兩端都該有、沒有秘密的東西 | 第 3.6 節；門宣告 1.3、5；Unity pre-spawned ghost（`PrespawnComponents.cs:9-33`，門宣告引用） |

卡片自己判的（照別家與已定規則，和別家不同時才問）：
- 物品定義另成 `items/<item>/item.json`、引用 W1 的 `weapon.json`（Lyra Fragment 的形；W1 不改）。
- 裝備欄的清單放角色資料，哪欄收什麼放物品定義。
- 改位置的 System 合成一個漏斗（Kernel 的延後套用，3.7）。
- 地上的物品從 `Walls` 排除（遊戲的 Query，不加碰撞圖層）。
- 撿起與手上的槍同一個 tick 送；超出預算時的同優先度等量到再說。

---

## 11. 沒查到、未確認的

- **Lyra 的源碼**：只讀了官方文件與社群文件。`FLyraInventoryEntry` 有 `StackCount`、`FFastArraySerializer` 出自 Garashka 引的程式片段；QuickBar 的欄數、EquipmentList 是不是 fast array、ItemInstance 是不是以 subobject 複製，**未確認**。
- **塔科夫**：兩個 wiki 擋自動讀取（fandom 402、wiki.gg 401），只有媒體與攻略站的摘要。裝備欄的完整清單、格子能不能套娃、重量門檻、保險的規則，**未確認**；要細節的話請使用者提供能讀的公開頁面，或在 I4、I5 開卡時再查。
- **Bevy 生態的背包 crate**：本機沒有，沒查。
- **Godot**：「物品用 `Resource`＋節點」是慣例的推論，沒有讀到官方的背包範例。
- I1 的「武器實體在玩家之後生成，玩家編號不變」是推論，要在 I1 用核心 PvP 的腳本確認。
- 撤離成功後的資料怎麼帶出局（存檔或帳號服務）不在這份研究。

## 12. 使用者的決定

（待填）

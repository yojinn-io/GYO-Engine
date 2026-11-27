# 模式配置：研究與事前宣告（`v8-arch-gyo2-mode-config`）

任務卡，2026-11-27。這一階段只做研究與事前宣告，交使用者核准；核准前不寫實作程式。作者 Opus 5.5／high。

出處：HANDOFF `v8-arch-gyo2-object-pvp-route-a`、`v8-arch-gyo2-mode-config-start`、`v8-arch-gyo2-limb-damage-decisions`、`v8-arch-gyo2-tarkov-is-reference`；研究 #365（`limb-damage-research.zh-Hant.md` 第 1.4 節、第 9 節）；批次表〈每批的設計審查〉〈後續小事〉。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| 路線 (A)：新功能都加在 Object_PvP，由 arena／模式資料決定開哪些；現行規則留成「核心 PvP」模式，當回歸基準 | `v8-arch-gyo2-object-pvp-route-a` |
| 最終產品是有怪物的撤離射擊，塔科夫只是參考之一 | `v8-arch-gyo2-tarkov-is-reference` |
| 模式配置的內容：同一個執行檔，用資料決定這一場掛哪些系統、用哪張規則表；沒列的系統不跑、不同步；模式 id＋規則 digest 進內容比對 | `v8-arch-gyo2-mode-config-start` |
| 部位全部資料化 (1c)、欄位模型 (H3)、擋／不擋欄位 (d)；PV3 在 G6 之後，回溯開關與上限由模式資料帶 | `v8-arch-gyo2-limb-damage-decisions` |
| #365 第 1.4 節的四點需求（本宣告第 3.4 節逐條對上） | `limb-damage-research.zh-Hant.md:116-125` |
| Engine 只給機制；Kernel 的職責是 ECS；Product 的詞不進 Engine；ECS 相關以 Bevy 為主參考 | AGENTS §1、批次表〈每批的設計審查〉第 5 項 |

### 0.2 結論（一段話）

**Engine 不新增機制。** 要的東西 Engine 都有：Kernel 的 run condition 與帶條件的 System set（Bevy 的 `run_if`、`configure_sets(...).run_if`），編排層的 `WorldDescription::build`／`units`（Bevy 的 Plugin 與 PluginGroup），網路的「兩端同樣登記、用條件決定跑不跑」（`OnServer`／`OnClient` 已經是這個做法）。模式配置是**遊戲層的資料與一小段膠水**：

1. `assets/object_fps_pvp/modes/` 放模式檔（另一個檔，不併進 arena）；模式檔列出「開哪些玩法組」與「用哪些規則表」。
2. 遊戲的每個玩法組（武器切換、門、肢體傷害的後果、延遲補償……）**在所有模式下都登記**（component、`Replicate`、authority、System），每個 System 進一個帶條件的 Kernel set，條件讀「這一場的模式」。沒開的玩法組：System 不跑、它的 component 不放到任何實體上，所以不同步、不進權威 digest。
3. 模式 id＋規則 digest 放進 `MatchInfo`，和 `wallsDigest` 同一種比法；不一致就是 `ContentMismatch`，走 arena 不一致的同一條路（Client 換一個 World 重來）。
4. 核心 PvP 模式＝現在的規則。證明沒變：同一台機器上，改前與改後的程式跑同一段腳本，每一步的權威 digest 逐位元相同；S5 的載入量測在基準範圍內。

---

## 1. 成熟方案（Engine 的機制 vs 遊戲的資料）

查證等級：**源碼**（本機 `~/Code/GitHome`，附檔名與行號）、**官方文件**、**△ 二手資料**、**未確認**（憑記憶、沒查到出處）。

### 1.1 Bevy（主參考，源碼 `bevy` 6a911653f）

| 什麼 | 位置 | 機制或資料 |
|---|---|---|
| `Plugin`：`build(&self, app)`，另有 `ready`／`finish`／`is_unique` | `crates/bevy_app/src/plugin.rs:57-89` | 機制 |
| 任何 `Fn(&mut App)` 都是 Plugin：遊戲的功能單位可以只是一個函式 | `plugin.rs:96-99` | 機制 |
| `PluginGroup`：一組 Plugin，可 `set`／`add_before`／`enable`／`disable` | `crates/bevy_app/src/plugin_group.rs:253,362,437,537,551` | 機制（哪幾個開關是遊戲的選擇） |
| run condition：`resource_exists`、`resource_equals`、`resource_exists_and_equals`、`not` | `crates/bevy_ecs/src/schedule/condition.rs:688,728,770,1258` | 機制 |
| 把條件掛在 set 上：`configure_sets(...).run_if(...)`；`add_systems` | `crates/bevy_app/src/app.rs:321,402`；`crates/bevy_ecs/src/schedule/config.rs:542,670` | 機制 |
| State：`in_state`、`OnEnter`、`DespawnOnExit` | `crates/bevy_state/src/condition.rs:103`、`state/transitions.rs:19`、`state_scoped.rs:145` | 機制（狀態的種類是遊戲的） |

Bevy 自己沒有「模式」：哪個模式開什麼，是遊戲用上面的機制寫的。兩種寫法都常見：(i) 依模式決定 `add_plugins` 哪些（登記本身就不同）；(ii) 全部 `add_plugins`，用 `run_if(resource_equals(...))` 或 `in_state(...)` 決定跑不跑。

**Bevy 生態在有網路時選 (ii)**，因為兩端的登記必須相同：

- bevy_replicon：component、event、message 的**登記順序兩端必須相同**，建議「把登記都放在共用的 crate」（`bevy_replicon/src/lib.rs:545-546`）；不同就是 protocol mismatch（`src/client.rs:246-253`）。
- lightyear：「只有 P2P 的 Client 會跑」的協議，仍放在共用的 Plugin 裡登記，因為 id 依登記順序配，一般的 Client 與伺服器也要佔住同樣的 id（`crates/core/lightyear/src/shared.rs:26-30`）。**跑不跑和登不登記分開**，正是這裡要的。

### 1.2 Unity Entities／Netcode for Entities（源碼 `com.unity.entities` 1.5.0、`com.unity.netcode`）

| 什麼 | 位置 | 機制或資料 |
|---|---|---|
| `RequireForUpdate<T>()`：某個 component（常是一個 singleton）不存在時，這個 System 不更新 | `Unity.Entities/SystemState.cs:1115-1120`（說明 1100-1112） | 機制 |
| `[WorldSystemFilter]`、`[DisableAutoCreation]`、`WorldSystemFilterFlags`：哪些 System 在哪種 World（伺服器、Client、單機）被**建立** | `Unity.Entities/WorldSystemFiltering.cs:37,47,125`；`DefaultWorldInitialization.cs:375` | 機制（用程式的 attribute 分 World 種類，不是分模式） |
| 協議版本＝RPC 的集合版本＋ghost component 的集合版本（整個 build 裡所有登記的型別，不分場景） | `com.unity.netcode/Runtime/Connection/NetworkProtocolVersion.cs:65-80` | 機制 |

Unity 的分法：**World 的種類**（伺服器／Client）用 attribute 決定建立哪些 System；**這一場玩什麼**由場景資料（subscene 烘出的 singleton）加 `RequireForUpdate` 決定跑不跑；協議涵蓋所有型別、不隨場景變。官方 Netcode 範例用「場景裡放一個 `EnableXxx` singleton，System 在 `OnCreate` 裡 `RequireForUpdate<EnableXxx>()`」來開關範例的玩法——**未確認**（範例 repo 不在本機，憑記憶）。

### 1.3 flecs（源碼 `flecs` 09790b5f7，對照）

- module 是一個匯入函式：`ecs_import`／`ECS_MODULE`／`ECS_IMPORT`（`include/flecs/addons/module.h:46-49,114,125`）。每個 World 自己決定匯入哪些 module（＝依登記區分，(i) 的做法）。
- System 是實體，`ecs_enable(world, system, false)` 加上 `EcsDisabled` 就不再被排到（`include/flecs.h:2950-2966`），可以用資料在執行期關掉。
- flecs 本身沒有複製協議，所以 (i) 沒有「兩端登記不同就連不上」的代價。這一點和 GYO 不同，只當對照。

### 1.4 Unreal Lyra（本機沒有源碼）

| 什麼 | 層 | 查證 |
|---|---|---|
| Game Features 外掛系統（`UGameFeaturesSubsystem`、`UGameFeatureAction`，外掛在執行期被註冊、載入、啟用） | **Engine 的機制**（GameFeatures plugin） | 官方 API 頁只有類別清單：<https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/GameFeatures> |
| `ULyraExperienceDefinition`：`UPrimaryDataAsset`，欄位有要啟用的 Game Features（`GameFeaturesToEnable`）、預設的 pawn 資料、要執行的 action 與 action set | **Lyra 範例遊戲的程式與資料** | △ <https://x157.github.io/UE5/LyraStarterGame/Experience>、△ <https://velog.io/@imeamangryang/Unreal-Engine-Data-Driven-Architecture> |
| 流程：決定 Experience → 非同步載入資產 → 啟用 Game Feature 外掛 → 執行 action | Lyra | △（同上 velog） |
| Experience 由伺服器決定、複製給 Client，Client 收到後載入同一組 | Lyra | **未確認**（憑記憶） |

Lyra 的分法：**Engine 給「執行期啟用一包程式與內容」的機制，Experience（哪個模式開什麼）是遊戲的資料資產。** GYO 只有一個執行檔、程式全部連結在內，不需要「執行期載入一包程式」；要借的只有「Experience＝模式資料資產」這個概念。

### 1.5 共同點與 GYO 的取捨

1. **四家都把「哪個模式開什麼」放在遊戲的資料**；Engine 只給「依資料決定跑不跑／建不建」的機制。GYO 照做：模式檔在 `assets/object_fps_pvp/`，膠水在 `games/object_fps_pvp/`，Engine 不動。
2. **有複製協議的框架（replicon、lightyear、Unity Netcode）都讓登記固定、用條件或資料決定跑不跑**；沒有協議的（flecs、單機 Bevy）才常用「依登記區分」。GYO 有 protocol hash 與 consistency fingerprint（第 2.2 節），所以照前者。
3. **和 Bevy 的接合點**：玩法組的登記函式＝Bevy 的 `Fn(&mut App)` Plugin；條件＝`resource_equals`；帶條件的 set＝`configure_sets(...).run_if`。沒有偏離 Bevy，沒有衝突點；膠水只在遊戲層（第 2.4 節，約一個表格加一個條件模板）。

---

## 2. 第 1 題：Engine 要不要新增機制

**前提：這是作者能處理的。** 「哪個模式開什麼、用哪張表」是遊戲規則；Engine 只要提供「依資料決定跑不跑」與「兩端登記一致」的機制，而這兩個都已經有了。

### 2.1 現有的機制夠不夠

| 需要 | 現有的機制 | 夠不夠 |
|---|---|---|
| System 依資料跑或不跑 | Kernel 的 run condition：函式或不帶捕獲的 lambda，回傳 bool、參數唯讀，讀的東西併進 System 的讀（`engine/ecs/kernel/include/engine/kernel/Registry.hpp:53-97`）；`Mount::conditions`（`Registry.hpp:103-111`） | 夠 |
| 一組 System 共用一個條件 | `Registrar::Set(SetOptions, conditions)`（`Registry.hpp:250`；`SetOptions` 是名字、schedule、先後，`Declarations.hpp:64-69`）；`Mount::sets` | 夠。set 屬於一個 schedule，跨 Fixed 與 Frame 的玩法組要兩個 set，共用一個條件 |
| 同一個 World 描述在不同模式下帶不同的東西 | `WorldDescription::build`（遊戲自己的登記，`engine/orchestration/include/engine/orchestration/Product.hpp:34-44`）、`OptionBag options` | 夠。模式是 `WorldOptions` 的一部分，`BuildWorld` 讀它 |
| Engine 的 Module 依 World 選 | `WorldDescription::units`（`Product.hpp:39-40`）＝PluginGroup 的 enable／disable | 夠（這次用不到：模式不決定 Engine Module） |
| 兩端登記一樣、只在一端跑 | `OnServer()`／`OnClient()`：「兩端登記同一個 System、帶條件，consistency fingerprint 就相同」（`engine/modules/net/include/engine/net/Replication.hpp:633-636`） | 夠，而且是**先例**：模式條件照同一個做法 |
| 內容不一致時告訴外面 | 遊戲的 `ContentMismatch` 與 `FollowMatchArena`（`games/object_fps_pvp/src/world/PvpWorld.cpp:810-825`） | 夠（遊戲層，加比模式） |

### 2.2 為什麼「依模式登記」在 GYO 會卡住

握手時比兩樣東西，兩樣都由登記組成：

- **protocol hash**：登記的名字、順序、Codec 版本（`Replication.hpp:13-14`；組法 `engine/modules/net/src/Replication.cpp:23-27`）。
- **consistency fingerprint**：寫決策資料的 System 的名字、存取、掛點、**run condition 與所屬的 set**（`engine/modules/authority/src/ServerAuthorityStrategy.cpp:313-345`；收集 `engine/modules/authority/src/Consistency.cpp:105-121`）。不同就 `FingerprintMismatch`（`engine/modules/net/src/Handshake.cpp:45-51`）。

如果依模式登記，**模式不同的兩端在握手就被拒**，Client 拿不到 `MatchInfo`，也就不知道 Match 玩的是哪個模式。現在 arena 不一致是連上之後從 `MatchInfo` 得知、換一個 World 重來（`PvpWorld.cpp:810-825`、`games/object_fps_pvp/src/client/ClientWorld.cpp:1408-1423`）；依模式登記會讓模式走另一條、更差的路（只拿到一行協議不符）。

固定登記之後，模式不在協議裡：兩端都能連上，模式在 `MatchInfo` 裡比（第 3.3 節）。

### 2.3 選項

| | 做法 | 好處 | 代價 |
|---|---|---|---|
| (R1) | **全部玩法組固定登記**（component、`Replicate`、authority、System），System 進帶模式條件的 set；component 只在開啟時放到實體上 | 協議與指紋不隨模式變；模式不一致走 arena 的同一條路；照 replicon／lightyear／Unity Netcode | 每加一個玩法組，**所有模式**（含核心 PvP）的 protocol hash 與指紋都會變（兩端仍一致）；關著的 System 每步算一次條件 |
| (R2) | **依模式登記**（只登記開的玩法組） | 核心 PvP 的登記永遠＝今天；Plan 裡沒有關著的 System | 模式不同在握手被拒、拿不到模式；Client 連線前一定要先知道模式（直連 `--connect` 時要自己帶）；#365 L1 的「登記固定」不成立 |
| (R3) | 協議（`Replicate`、authority）固定、System 依模式登記 | 協議不變 | 指紋收 System，仍在握手被拒；兩邊的缺點都有 |

**判定：(R1)。** 理由：第 2.2 節；#365 第 1.4 節第 3 點已定「登記固定、掛不掛由模式定」，這裡把它落到 System 上；(R1) 是 Bevy 生態有網路時的做法（1.1）。(R1) 的代價「加玩法組就換協議」是正常的升版：舊 Client 本來就該連不上新 Match。

### 2.4 遊戲層的膠水（不在 Engine）

- **玩法組（gameplay set）**：遊戲程式裡的一張固定表。每一列是名字（例 `doors`）、一個登記函式 `void(O::WorldSetup&)`（＝Bevy 的 `Fn(&mut App)` Plugin）、模式檔裡它那一塊設定的解析函式。名字照命名規範、只用 ASCII。
  - 不叫 Feature（GYO 文件裡 Feature 是行為需求的用語）、不叫 Plugin／Module（GYO 的 Plugin＝第三方、Module＝我們維護的 Engine 單位）。
- **條件**：`template <std::size_t Slot> bool GameplaySetOn(K::Res<ActiveMode>)`，名字 `pvp.mode.<組名>`。條件不能捕獲（`Registry.hpp:53`），所以用表裡的固定位置當模板參數。
- **`ActiveMode`**：一個 Resource，`Prepare` 時放好、之後不寫；不是決策資料（不進權威 digest）。不併進 `GameRules`：`LoadArena` 在生成期間寫 `GameRules`（`PvpWorld.cpp:909-913`），條件讀它會給所有被條件管的 System 加上不必要的衝突。
- **每個玩法組的 System**：在 Fixed 與 Frame 各進一個 set（`pvp.<組名>.fixed`、`pvp.<組名>.frame`），兩個 set 都帶同一個條件。
- **登記順序**：玩法組的登記一律排在核心的登記之後，讓核心型別的 authority 登記序號與網路的 wire id 不動（權威 digest 用型別的 authority 登記序號，`engine/modules/authority/include/engine/authority/Consistency.hpp:7-13`）。

### 2.5 Engine 這邊有沒有缺口

- 沒有一定要補的。Bevy 內建 `resource_equals` 這類常用條件（`condition.rs:728`），GYO 的 Kernel 沒有；遊戲自己寫一個條件模板只要幾行。**不在這條線補**：只有一個使用者，照 AGENTS §1 不抽；第二個產品也寫同樣的東西時再議。
- `state_machine` 的 `InState`（`engine/modules/state_machine/include/engine/state_machine/Conditions.hpp:27`）不拿來當模式：模式在一個 World 的一生裡不變（和 arena 一樣，換模式＝換 World），不需要狀態轉移。

---

## 3. 第 2 題：模式資料的形狀

**前提：這是作者能處理的。** 檔案的位置與欄位是遊戲的資料契約；Engine 只讀 JSON（JSON Module）、算 hash（Kernel 的 Hasher）。

### 3.1 放在哪裡：另一個檔

| | 做法 | 判定 |
|---|---|---|
| (D1) | arena 檔加 `mode` 欄 | 不採：同一張地圖要能玩不同模式（核心 PvP 與撤離都在同一張圖上），arena 與模式是兩個獨立的選擇 |
| (D2) | **`modes/modes.json` 列模式檔，每個模式一個檔** | **採用**：和 `maps/arenas.json` 同一種寫法；Lyra 的 Experience 也是獨立於地圖的資料資產 |
| (D3) | 模式寫在 `arenas.json` 的每一列 | 不採：同 (D1) |

### 3.2 欄位

`assets/object_fps_pvp/modes/modes.json`（照 `maps/arenas.json`；第一個是預設）：

```json
{ "version": 1, "modes": ["core_pvp.json"] }
```

`assets/object_fps_pvp/modes/core_pvp.json`：

```json
{
  "version": 1,
  "id": "core_pvp",
  "name": "核心 PvP",
  "rules": {
    "movement": "rules/movement_rules.json"
  },
  "sets": {},
  "arenas": []
}
```

| 欄位 | 意思 | 錯誤 |
|---|---|---|
| `id` | 模式的 id，進 `MatchInfo`；在 `modes.json` 裡唯一 | 重複：讀取失敗 |
| `name` | 顯示的名字（大廳、房間） | — |
| `rules` | 規則表的種類 → 檔案（相對 content 根）。種類由遊戲程式認得：第一批只有 `movement`；之後 W1 加 `weapons`、G6 加 `damage` | 不認得的種類、檔案讀不到或解析失敗：讀取失敗 |
| `sets` | 開啟的玩法組 → 它那一塊設定（物件；沒有設定就是 `{}`）。**列在這裡＝開，沒列＝關** | 遊戲不認得的組名、設定解析失敗：讀取失敗 |
| `arenas` | 這個模式能玩的 arena id；空＝全部 | 列了沒安裝的 arena：讀取失敗；Match 的 `--arena` 不在清單裡：用法錯誤 |

**arena 檔也可以帶玩法組的一塊資料**（門的位置、撤離點、怪物的出生區）：arena 檔加選填的 `sets` 物件，組名 → 那個組自己解析的資料。模式沒開那個組時，資料照樣解析、不用。這樣同一張圖能在不同模式下玩。

**為什麼 `CombatRules` 這一批不搬進資料**：現在它是程式裡的預設值（`games/object_fps_pvp/include/object_fps_pvp/sim/CombatRules.hpp:20-32`）。W1 會把武器那幾個欄位（傷害、彈匣、冷卻、換彈、射程）搬到武器定義，G6 會把部位倍率搬到傷害表（(H3)）；這一批先搬，兩批又要再拆一次。所以這一批只讓 `movement`（本來就是檔案）走模式的 `rules`，證明「模式選規則表」的路；`CombatRules` 的值照樣進規則 digest（3.3），兩端不同照樣擋得住。

### 3.3 模式 id＋規則 digest 進內容比對

照 `wallsDigest` 的做法（`games/object_fps_pvp/include/object_fps_pvp/world/Components.hpp:131-155`、`PvpWorld.cpp:810-825`）：

- `MatchInfo` 加 `modeId`（字串）與 `rulesDigest`（u64）；`MatchInfoCodec` 版本號升一號。Match 在 `Prepare` 就放好（不必等地圖）；Client 照樣從 Match 收。
- **規則 digest 的內容**：解析後的值，用 Kernel 的 Hasher 逐型別算（和 `World::Summary` 同一種，`engine/ecs/kernel/include/engine/kernel/World.hpp:361-368`），**不 hash 檔案的原始位元組**——換行（CRLF／LF）、空白、欄位順序不同不該算不一致。涵蓋：
  1. 模式本身（id、開的組與各組的設定、`arenas`）；
  2. `rules` 裡的每一張表（第一批：`StanceRules`）；
  3. 程式裡的 `CombatRules`（搬進資料之前也要比）；
  4. 角色的判定形狀（`hitbox.json` 解析後的值；#365 第 1.4 節第 1 點，(1c) 之後標籤名由它宣告）。
- **Client 的判斷**（`FollowMatchArena` 改成也比模式）：`modeId` 不同、或 `rulesDigest` 不同 → `ContentMismatch`，`reason` 寫哪一項不同，並帶 `matchMode`。模式的比對不等地圖讀完（和「另一個 arena 當場就知道」同一個時機）。
- **程式怎麼處理**（不是 Engine 的事，照現在 arena 的做法）：Client 有那個模式、而且只是模式不同 → 用 Match 的模式重建 World（`ClientWorld.cpp:1408-1423` 加上模式）；digest 不同（同名模式、內容不同）→ 不能玩，回大廳並顯示原因。

### 3.4 #365 第 1.4 節的四點

| # | #365 的需求（`limb-damage-research.zh-Hant.md:118-123`） | 本宣告 |
|---|---|---|
| 1 | 模式要有身分、兩端一致：模式 id＋規則 digest 進 `MatchInfo`，和 `wallsDigest` 同一種；`hitbox.json` 也要進 digest | 3.3：`modeId`、`rulesDigest`；digest 涵蓋 `hitbox.json` 解析後的值；不一致是 `ContentMismatch` |
| 2 | 模式帶一張傷害規則表；現在 `CombatRules` 的數字就是核心模式的表 | 3.2：`rules` 是「種類 → 檔案」，G6 加 `damage` 這一種；在那之前 `CombatRules` 的值進 digest。核心 PvP 的傷害表＝現在的數字，由 G6 搬並用第 5 節的 A/B 證明 |
| 3 | 模式決定掛哪些 component；登記固定（protocol hash） | 2.3 (R1)、第 4 節：component、`Replicate`、authority、System 都固定登記；「掛」＝放不放到實體上、System 跑不跑 |
| 4 | 核心 PvP 的規則表＝現在的數字，S5 的結果要一致 | 第 5 節 |

### 3.5 誰決定這一場的模式

照 arena 的現況：

- **Match**：命令列 `--mode ID`（照 `--arena`，`games/object_fps_pvp/src/match_main.cpp:150-151,256-263`）；沒有就用 `modes.json` 的第一個。matches 服務起 Match 時帶 `--mode`（現在只帶 `--arena`，`games/object_fps_pvp/services/matches/local.go:57-62`）。
- **Client**：`SceneChange` 加 `mode`（照 `map`，`games/object_fps_pvp/include/object_fps_pvp/client/Scenes.hpp:53-55`）；沒有或沒安裝就用自己的預設，由 Match 的 `MatchInfo` 糾正（3.3）。
- **Local（單機）**：單機房間選的模式，經 `SceneChange` 帶進 `LocalWorld`。

**現存的 `coop`／`survival`**：房間服務寫死 `Modes = {"coop", "survival"}`，註解說「背後的規則是之後的功能，服務只帶這個選擇」（`games/object_fps_pvp/services/rooms/rooms.go:33-35,236-237`）；大廳切換（`games/object_fps_pvp/src/client/LobbyScene.cpp:31`）、顯示名「協力」「生存」（`GatewayScene.cpp:43-44`）、單機固定「生存」（`SoloScene.cpp:3`）。這是 game-flow 宣告（`games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md:36,110,225`）留的位置。模式配置做完之後，「模式」要有真的模式檔，這兩個佔位要怎麼處理是**決策點 M1**（第 8 節）。

---

## 4. 第 3 題：網路——登記固定、掛不掛由模式決定

**前提：這是作者能處理的。** 協議的組法是 Engine 的（不改）；遊戲照它的規則登記。

1. **所有玩法組的 `Replicate`、`ReplicateResource`、`Input`、`ServerMessage` 與 authority 登記，在每個模式、每一端都做**，順序固定：核心的在前（照現在 `PvpWorld.cpp:1091-1134`），玩法組照表的順序在後。protocol hash 只由程式決定，不由模式決定。
2. **「不同步」靠「沒有值」**：component 只在開啟的玩法組裡被放到實體上（生成實體、插入都在那個組的 System 裡），關著的組沒有任何格子，複製就沒有東西送；權威 digest 只加總有值的格子，也不變。
   - 玩法組的 Resource 若要複製（`ReplicateResource`），只在組開啟時 `SetResource`。不存在的 Resource 會不會被送：從 `Replication.cpp:299-305`（`encode` 回傳能不能送）推是不送，**未確認**，第 2 步用測試確認。
3. **System**：每個 System 帶組的條件；只在一端跑的照舊再加 `OnServer`／`OnClient`。條件和 set 都進指紋（`ServerAuthorityStrategy.cpp:333-345`），所有模式相同。
4. **Codec 版本號**：玩法組改 wire 時照舊升自己的 Codec 版本（例：G6 的 `HitRecord::part` 改成傳模式表的索引）。
5. **Kernel 的排程**：Serial 執行器用 Kahn，ready 的 System 取登記序號最小的；沒有先後的衝突對，從序號小的連到大的（`engine/ecs/kernel/src/Scheduler.cpp:10-24`）。所以後登記、沒有寫 `before` 的玩法組 System，不會插到核心 System 的前面；**寫了 `before` 指向核心 System 的，可能改變核心 System 之間的先後**——由第 5 節的 A/B 檢出，不另加守衛。

---

## 5. 第 4 題：怎麼證明核心 PvP 沒變

**前提：這是作者能處理的**（遊戲的回歸測試）。

| 項目 | 方法 | 判定 | 在哪裡跑 |
|---|---|---|---|
| **權威 digest** | **A/B**：一支「核心 PvP 腳本」程式（固定的輸入序列：走、跳、蹲、開槍、換彈、命中三種部位、擊殺、重生、墜落；Local World 與 Match＋2 Client 的記憶體連線各一份），每一步印 `Authority::Digest`（決策資料：`PlayerBody`、`PlayerCombat`、`ShotRecord`、`HitRecord`、`PlayerInput`）與 Client 端收到的值。用**改前的 `gyo2`（這條線分出的那一點）與改後**各建一次，在同一台機器上跑，輸出逐位元組比 | 完全相同 | macOS（本機）與 Linux 主機；腳本與輸出放主 checkout `build/evidence/gyo2-mode-config/` |
| 協議與指紋 | 同一支程式另印 protocol hash 的文字與 `DatasetReport` | 第 1 步：指紋的文字相同；protocol hash 只差 `MatchInfo` 的 Codec 版本。第 2 步（只加測試用的玩法組時）：見下 | 同上 |
| 既有測試 | `object_fps_pvp.*` 全部（`tests/object_fps_pvp/world/WorldTests.cpp` 已逐值驗傷害、冷卻、重生、墜落等），**不改期望值** | 全部通過 | 本機＋L1 三平台 |
| 新的測試 | 模式讀取（預設模式、錯誤的組名與種類、重複 id、`arenas` 限制）；`MatchInfo` 的模式不一致與 digest 不一致都是 `ContentMismatch`；**關著的玩法組不跑、不同步**（測試用的組：關著時 System 一次都沒跑、沒有實體有它的 component、每一步的權威 digest 和「沒有這個組」的 World 相同；開著時會跑、Client 收得到） | 通過 | 本機＋L1 |
| **S5** | 用既有的 `s5_linux.sh load`（主 checkout `build/evidence/gyo2-big-map/s5/`）在 Linux 主機重量 Match 讀圖＋建牆、峰值 RSS、Client 讀圖；另比 `BoxSet` digest | digest 相同；時間與記憶體不比基準差：Match 3,674～3,686 ms、峰值 RSS 597～602 MB、Client 3,950～3,963 ms（`big-map-declaration.zh-Hant.md:244,763-765`）。**時間是量測、不能逐位元**；判讀照 S5 的做法（各兩次取較差的） | Linux 主機，整條線最後一次（第 7 節第 4 步）。量之前在報告寫預計時間、負荷與中止方式 |

說明：

- **為什麼用 A/B、不存一份 golden 進 repo**：權威 digest 裡有浮點（位置、速度），跨平台、跨編譯器版本不保證逐位元相同（PvP 本來就不要求，`games/object_fps_pvp/docs/port-declaration.zh-Hant.md:44-49`）。存在 repo 裡的 golden 會因為換編譯器就紅；同一台機器上改前改後比，才是在比「這次的改動」。
- **Windows 不做 A/B**（建兩次太重），只跑 L1；寫明是未跑。
- **之後的批**（W1、G6、PV3）用同一支腳本當它們的驗收：「核心 PvP 模式下 A/B 相同」。

---

## 6. 第 5 題：之後的 W1、門、肢體傷害（G6）、PV3 怎麼掛上來

**前提：都是作者能處理的**（遊戲層）。這一節只舉例，不是那些批的設計。

兩種掛法：**玩法組**決定「哪些 System 跑、哪些 component 出現」；**規則表**決定「共用的 System 用哪組數字」。

| 批 | 怎麼掛 | 核心 PvP 模式 |
|---|---|---|
| **W1 武器資料化** | 規則表：模式的 `rules` 加 `weapons`（武器定義檔的清單，例 `weapons/nailer_v2/weapon.json`）；`CombatRules` 裡武器的欄位搬進武器定義，`UseWeapons`／`JudgeShots` 讀它 | `weapons` 只列 nailer_v2，數值＝現在的 `CombatRules`；A/B 相同。W3 的背包切換另成一個玩法組 `loadout_switch`（武器實體、切換的 component 固定登記；核心 PvP 不開＝只有一把、不出現切換的 component） |
| **門** | 玩法組 `doors`：`DoorState` 固定登記（authority＋`Replicate`）；System：從 arena 檔的 `sets.doors` 生成門、Match 收「按了」自打射線（`v8-arch-gyo2-interaction-dialogue-decisions` Q1）、改 `DoorState` | 不開：沒有門的實體、沒有 `DoorState` 的格子；arena 檔可以照樣帶門的資料 |
| **肢體傷害 G6** | 規則表 `damage`：(H3) 的欄位、(d) 的擋／不擋、形狀標籤 → 規則部位（(1c)），進 `rulesDigest`；`LimbHealth` 固定登記，只在表有部位血池時放到玩家身上；後果 (Z1)～(Z3) 是玩法組 `limb_consequences`；`HitRecord::part` 改成傳表的索引（Codec 升版本） | 傷害表＝頭 2000‰、軀幹 1000‰、腿 750‰、沒有部位血池；不放 `LimbHealth`、不開後果；A/B 相同是 G6 的驗收 |
| **PV3 延遲補償** | 玩法組 `lag_compensation`，設定 `{"max_rewind_ms": …}`（L2'）：Match 端記錄判定形狀的歷史（`OnServer`＋組的條件；歷史的 component 兩端都登記）；`JudgeShots` 有這組的設定時照 Client 看到的時間回溯，沒有時用當步的姿勢（今天的做法） | 不開：`JudgeShots` 走今天的路；A/B 相同 |

---

## 7. 第 6 題：批次怎麼切

**一條線、一個 draft PR**（PR 盡量少開；文件同步併進同一個 PR）。每一步開頭先跑 A/B（第 0 步之後）與相關測試。

```
v8-arch-gyo2-mode-config（一個 PR，開向 gyo2）
├─ 第 0 步｜Opus/high｜下段｜核心 PvP 腳本程式＋A/B 腳本；改前的輸出先存
├─ 第 1 步｜Opus/high｜上段｜模式檔、讀取、ActiveMode、MatchInfo 的 modeId＋rulesDigest、ContentMismatch、--mode、SceneChange::mode
├─ 第 2 步｜Opus/high｜上段｜玩法組的表與條件、arena 的 sets 區塊、「關著不跑不同步」的測試
├─ 第 3 步｜Opus/high｜下段｜房間／matches 服務與大廳、單機的模式（照 M1）；client 測試拆成幾個 ctest 項
└─ 第 4 步｜Opus/medium｜下段｜A/B（macOS、Linux）、S5 重量、設計審查、純淨 Code Review、dev_log
```

| 步 | 內容 | 上段／下段（根據） | 實作者 | 審查者 | 檢驗者 |
|---|---|---|---|---|---|
| 0 | `tests/object_fps_pvp/` 下加核心 PvP 腳本程式（印每步的 digest、協議文字、`DatasetReport`）；A/B 腳本放主 checkout `build/evidence/gyo2-mode-config/`；先用改前的程式跑一次存下 | 下段（測試工具，不動行為與資料契約） | Opus／high（要先懂權威與網路的測試支援） | 併入第 4 步的設計審查 | Opus／high（navigator：上段計畫的輸入不用小模型） |
| 1 | 第 3 節：`modes/` 兩個檔、讀取與錯誤、`ActiveMode`、`MatchInfo` 加兩欄（Codec 升版本）、規則 digest、`FollowMatchArena` 比模式、Match `--mode`、`SceneChange::mode`、Client 的重建路徑、Local | **上段**：資料契約（新的內容檔）、wire（`MatchInfo` 的 Codec）、交換給 Client 的判斷 | Opus／high | Opus／high（第 4 步一起） | Opus／high |
| 2 | 第 2.4、4 節：玩法組的表、條件模板、兩個 set、arena 檔的 `sets`；測試用的玩法組只在測試裡登記 | **上段**：登記與協議的規則（protocol hash、指紋） | Opus／high | Opus／high | Opus／high |
| 3 | 房間服務的模式清單照 M1（照 `installedMaps` 讀 `maps/arenas.json` 的做法讀 `modes/modes.json`，`games/object_fps_pvp/services/cmd/services/main.go:181-187`）；matches 帶 `--mode`；大廳與單機房間顯示模式檔的 `name`。另收批次表〈後續小事〉指給「下一次動 object_fps_pvp 測試的批」的一件：`object_fps_pvp.client` 拆成幾個 ctest 項目平行跑 | 下段（服務的值的來源、UI 顯示；不動協議） | Opus／high（Go＋C++ 多檔） | Opus／high | Opus／high |
| 4 | A/B 在 macOS 與 Linux；S5 的 `load` 在 Linux；每批的設計審查（Opus／high，唯讀，報告放 `build/evidence/gyo2-mode-config/`）；純淨 Code Review（Fable／high，只給 git 範圍）；相關突變（D184：新的、find 在改動範圍的）；dev_log | 下段（執行與紀錄）；但檢驗的是上段的成果，檢驗者不用小模型 | Opus／medium | — | Opus／medium |

- 模型都不超過主會話（Opus）；Fable 只在純淨 Code Review（navigator 規則，不另問）。
- **突變**（只列預定的方向，實際在各步定）：模式不一致沒被偵測、digest 不一致沒被偵測、不認得的組名被接受、條件恆真、關著的組仍放 component、玩法組排到核心登記之前。
- **時間**：第 0～3 步各約半天到一天；第 4 步的 S5 約 20 分鐘（S5 的 `load` 模式各兩次）。超過估計 1.5 倍就停下回報。

---

## 8. 要使用者決定的點

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **M1** | 房間的 `coop`／`survival` 佔位（game-flow 宣告留的）怎麼處理 | (a) **房間的模式清單改讀已安裝的模式檔**（和地圖讀 `arenas.json` 同一種）；現在只有「核心 PvP」；`coop`／`survival` 拿掉，等它們有自己的模式檔再出現；單機用模式清單的第一個 (b) 先建 `coop.json`、`survival.json` 兩個模式檔，內容暫時和核心 PvP 相同 (c) 房間的「模式」和模式配置分開，兩個概念並存 | **(a)**：(b) 的兩個檔現在沒有自己的規則，會是三份一樣的內容、日後分歧；(c) 讓「模式」一詞有兩個意思。(a) 會改 game-flow 宣告的兩處（大廳的模式切換、單機固定「生存」），所以要你決定 |

卡片自己判的（不是決策點，列出來讓你知道；要改就說）：

- 登記方式 (R1)（2.3）：#365 第 1.4 節第 3 點已定方向，這裡落到 System 上。
- 模式放另一個檔 (D2)（3.1）。
- `CombatRules` 這一批不搬進資料，留給 W1／G6（3.2 末段）。
- digest 算解析後的值、不算檔案位元組（3.3）。
- 第 3 步順便把 `object_fps_pvp.client` 測試拆開（批次表〈後續小事〉指定給下一次動 object_fps_pvp 測試的批）。
- S5 在第 4 步要在 Linux 主機重量一次（需要主機開著）。

## 9. 沒查到、未確認的

- Lyra 的 Experience 欄位、載入流程是二手資料（△）；Experience 由伺服器決定再複製給 Client 是憑記憶。
- Unity Netcode 範例用 `EnableXxx` singleton＋`RequireForUpdate` 開關玩法：範例 repo 不在本機，憑記憶。
- 不存在的 Resource 不會被 `ReplicateResource` 送出：從程式推，第 2 步用測試確認。
- A/B 只在 macOS 與 Linux；Windows 只有 L1 的既有測試。

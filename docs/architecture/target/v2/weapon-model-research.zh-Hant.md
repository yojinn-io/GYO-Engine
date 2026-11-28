# Object_PvP 的武器模型怎麼組成（`v8-arch-gyo2-weapon-model-research`）

更新：2026-11-28（同日加第 14 節：素材資料夾結構，使用者已定、排進 W1；同日加 14.4 節：敵人與其他武器的擴充，以及第 15 節：使用者的決定）。Owner：研究卡（由 GYO2 重建主會話開出）。**狀態：只是研究，不實作。決策點已全部收完：U1 使用者選 (a)（第 15 節）；素材資料夾結構與它的擴充使用者已定（第 14 節）。改寫了第 5.2 節的資料夾形狀與第 11 節 G1 的內容；其他已定的決定沒有改。**

查的版本：`gyo2` 9eea5968；E1、E2 看的是 `origin/claude/gyo2-model-pose-ik` 170c610f（#377，還沒合併）。本地源碼：Bevy 6a911653f、Godot 94fc09c、Blender 4.5 內附的 FBX 匯出器（`/Applications/Blender.app/Contents/Resources/4.5/scripts/addons_core/io_scene_fbx/`。素材是用 Blender 5.2.3 匯出的，**版本不同**，5.2 的選項是不是一樣，未確認）。Unreal 只讀官方文件（附 URL）。Source（CS:GO、Garry's Mod）、塔科夫、Valorant 沒有原始碼，Valve Developer Community 的頁面也抓不到（HTTP 403），所以只能用搜尋摘要，全部標「未確認」。

FBX 的內容是本卡用一支只讀的 Python 腳本（自己解析 FBX binary 7400 的節點、連線和動畫層）直接讀檔案得到的，**沒有經過 gyo2 的 ufbx adapter 跑過**。單位換算後的數值是推論。

## 0. 問題、已定的前提、判斷依據

### 0.1 問題（用遊戲的例子）

使用者的問題：「我的槍和手現在混在一起，我要拆開嗎？」

W2 要加第二把武器，例如步槍。打開第一人稱時：

1. 畫面上是手臂（皮膚、手套）加上步槍。手臂應該和射釘槍是同一雙手，換手套時兩把槍都要跟著換。
2. 步槍的 Reload 要動手，也要動彈匣（彈匣拔出、插回），而且動作和射釘槍不一樣。
3. 第三人稱：別人看到你手上拿步槍，左手要握在護木上（G3 的左手 IK），而且 Match 也要算這一步（D5 (b)，手臂進判定）。

要回答的是：這三件事在模型檔裡怎麼分，資料怎麼寫，Blender 怎麼匯出，Engine 要不要加東西。

### 0.2 已定的前提（這份研究不推翻它們）

| 前提 | 誰定的、出處 | 掛在它上面的 |
|---|---|---|
| 武器系統放遊戲層 `object_fps_pvp/weapons/`。Engine 只提供機制（ECS、ChildOf、asset、model 的取樣與混合、音效） | 使用者 2026-11-26，HANDOFF `v8-arch-gyo2-weapon-system-split`（`HANDOFF.md:415`） | 第 7 節 |
| 第 2 層（每個實體的播放器）不進 Engine，遊戲從 tick 推算（D1）；two-bone IK、逐骨權重混合、additive、`CrossedEvents` 進 model（D2、D3）；第三人稱是基本姿勢組＋每把武器的覆寫表（D4）；Match 也跑 IK（D5 (b)） | 使用者 2026-11-27，`v8-arch-gyo2-anim-layers-decisions`（`HANDOFF.md:419`），研究 #363 第 12 節 | 第 3、5、7 節 |
| `SolveTwoBoneIK` 不轉末端骨（手）；G3 要手的朝向時，在 IK 之後由遊戲補（U2 (a)） | 使用者 2026-11-28，`v8-arch-gyo2-model-pose-ik-u1-u2`（`HANDOFF.md:463`） | 第 4、5 節的握點 |
| 最終產品做肢體傷害 (3)：手臂進判定，左手握點是影響判定的資料 | 使用者 2026-11-27，`v8-arch-gyo2-limb-damage`、`-limb-damage-decisions`（`HANDOFF.md:422`） | 第 4、5 節 |
| 路線 (A)：新功能都加在 Object_PvP、由模式配置開關；核心 PvP 逐位元一致 | 使用者 2026-11-27，`v8-arch-gyo2-object-pvp-route-a` | 第 11 節的驗收 |
| W1 的武器定義檔是 `weapons/nailer_v2/weapon.json`，由模式規則表的 `weapons` 列出 | 模式配置宣告第 6 節（`mode-config-declaration.zh-Hant.md:265`；#367 已合併） | 第 5 節 |
| 素材都是使用者自己在 Blender 做的（除了骨骼動畫庫） | `assets/object_fps_pvp/LICENSES/SOURCES.zh-Hant.md:11-13`；HANDOFF `v8-arch-gyo2-pvp-assets-license` | 第 6 節 |
| 使用者不要空殼（「空殼刪掉」） | 使用者 2026-11-27，`v8-arch-gyo2-mode-config-m1` | 第 11 節：W1 不先放沒有素材的欄位 |

### 0.3 判斷依據

- AGENTS §1-1（Engine 只提供機制）、§1-6（產品的詞不進 Engine）、§1-7（Tool 與 Game 只靠資料形式交接）、§1-10（預設方便、不綁作者）、§2-2（先問「該不該存在」）。
- 每一項先寫前提：「這是 Engine 要解決的（只有 Engine 做得到），還是作者能處理的」。

## 1. 現況（讀檔案與程式得到的）

### 1.1 第一人稱 `weapons/nailer_v2/viewmodel/Nailer1_FPS.fbx`

| 項目 | 內容 |
|---|---|
| 匯出器 | Blender (stable FBX IO) 5.2.3；`UnitScaleFactor` 100（檔案單位＝公尺）；30 fps |
| 骨架 | 一副 `Armature`（FBX 的 Null 節點，**縮放 0.01、偏航 15°**）。下面是 `Mark23_Root`，它有兩個子節點：`Spine2_1`（雙臂、48 根手指骨）與 **`main_j`**；`main_j` 下有 `side_j`、`Mag_j`。共 54 根骨 |
| `main_j` 的位置 | **不是手的子節點**，是 `Mark23_Root` 的直接子節點，和手臂平行。槍的移動由 `main_j` 自己的動畫帶，手是另外配合著動 |
| 網格 | 4 個，每個各有一個 skin：`Hand_Mesh`（2,534 三角面，綁 39 根手臂與手指骨）、`main`（槍身，5,866 面，只綁 `main_j`）、`mag`（彈匣，4,171 面，只綁 `Mag_j`）、`Side`（132 面，只綁 `side_j`） |
| 材質 | `LowPoly_Skin`、`LowPoly_Glove`、`LowPoly_Skin_Shadow`（手臂）、`Nailer1_Material`（槍） |
| 片段 | `Armature|Idle` 1.000 s、`Shoot` 0.333 s、`Reload` 3.733 s、`Draw` 0.833 s、`Hide` 0.367 s（由 `LocalStop` 換算）。**每個片段 54 根骨都有 key**（Blender 的「Key All Bones」預設是開的，`io_scene_fbx/__init__.py:494-498`） |
| 尺寸 | 槍身的頂點範圍約 9×32×26，彈匣約 7×21×12。骨頭的局部座標是 `Armature` 的 0.01 縮放之前的值，所以**實際上是公分**（推論：槍身約 32 cm） |

槍的零件（槍身、彈匣、`Side`）每個都是「一個網格只綁一根骨、權重全是 1」，也就是剛體。這種做法讓「槍上會動的零件」等於「`main_j` 底下的骨頭」。

### 1.2 第三人稱 `weapons/nailer_v2/world/Nailer2_TPS.fbx`

- 只有一個網格節點 `Nailer2_World_LOD0`（1,388 三角面，**沒有骨頭、沒有 skin、沒有其他節點**：沒有槍口、沒有握點）。節點縮放 100、檔案單位是公分（`UnitScaleFactor` 1）、24 fps。頂點範圍約 0.29×1.0×0.89。
- 掛在角色的 `hand_r` 上：`characters/player/presentation.json:22-27` 的 `weapon {node, translation, rotation_xyzw, scale: 0.107491}`。這個 `scale` 是在補這把槍模型的大小（推論：網格做成約 1 單位大小，再用掛點縮小），**掛點的數字是為這把槍調的，卻放在角色的檔案裡**。
- 和第一人稱的槍是兩個不同的模型：名字是 `Nailer1` 對 `Nailer2`，面數 10,169 對 1,388。外觀是不是同一個設計，本卡沒有看圖，未確認。

### 1.3 程式怎麼用

| 用途 | 位置 | 做法 |
|---|---|---|
| 讀第一人稱 | `ClientWorld.cpp:2021-2109`（`LoadViewmodel`） | 一個 FBX、`FbxSettings::textures = false`；材質名 → 貼圖 id 的表（`nailer_v2_viewmodel.json` 的 `materials`）；片段用 animset 的名字找；`anchor_node`（`main_j`）在 Idle 第 0 幀的位置當放置的錨點。檔名 `nailer_v2_viewmodel.json`、`Nailer1_FPS.fbx` **寫死在程式裡**（`:2023`、`:2027`） |
| 第一人稱的姿勢 | `ClientWorld.cpp:859-938`（`ViewmodelClip`、`PoseViewmodel`） | 一次 `SampleClip`＋`ComputeModelSpace`，**同一份姿勢給 4 個網格的 entity**（`SkinPose.nodes`，`:909`）。**`Hide` 沒有用到**；換彈聲只看 `magazine_in` 一個事件（`:920-934`） |
| 第一人稱的掛法 | `ClientWorld.cpp:940-963`（`AddViewmodel`） | 每個網格一個 entity，`ChildOf` viewmodel 相機，照 Bevy 的 `examples/camera/first_person_view_model.rs` |
| `muzzle` | `nailer_v2_viewmodel.json` 的 `muzzle {node: main_j, local_position_meters}` | **程式沒有讀**（`git grep -i muzzle` 在 `games/` 只有 JSON）。數值 (0.0016, 13.57, -20.22) 是 `main_j` 的局部座標，照 1.1 節是公分，欄位名的 `meters` 不對（推論） |
| 第三人稱的掛法 | `ClientWorld.cpp:813`、`CharacterRig.cpp:349-351`（`WeaponToWorld`） | **不是 `ChildOf`**：gyo2 的骨頭不是 entity，姿勢是 `SkinPose` 裡的矩陣。每幀算「身體到世界 × `hand_r` 的模型空間矩陣 × 掛點」，寫進槍的 `Transform` |
| 第三人稱片段的來源 | `CharacterRig.cpp:163-184` | 片段從動畫庫（UAL mannequin）用 `BindByName`＋`Retarget` 搬到角色身上。**gyo2 已經有「依骨頭名字把一個模型的動畫用到另一個模型」的先例** |
| 射擊判定 | `PvpWorld.cpp:486-489`（`ShotRay`） | 射線從眼睛出發，**槍的模型不進判定**。G3 之後只有左手握點進判定 |

### 1.4 ufbx adapter 讀得到什麼（`engine/modules/model/src/adapters/ufbx/UfbxImport.cpp`）

- **所有節點都會進 `ModelData::nodes`**：骨頭、Null、網格節點都一樣，從根開始深度優先（`:110-122`）。所以 Blender 的 Empty（例如 `muzzle`、`grip_l`）匯出後就是一個有名字的節點，`FindNode` 找得到。
- 轉成公尺、+Y 上、+Z 前、左手系（`:60-87`）。換算放進幾何與變換，所以骨頭的局部座標會帶著 `Armature` 的 0.01 縮放（`nailer_v2_viewmodel.json` 的 `muzzle` 就是這樣），這一點沒在引擎裡跑過確認。
- 一個網格最多一個 skin（`:155-157`）；一個網格的多個材質會拆成多個 `Mesh`（`:165-171`）。
- 每個 anim stack 變成一個片段，以 60 Hz 烘焙，**對那個檔案自己的節點清單**（`:309-338`；`AnimationClip::nodeCount`，`ModelData.hpp:143-145`）。一個片段不能直接拿去取樣另一個檔案的節點清單；要用 `BindByName`（＋`Retarget`）搬，或者兩邊各自帶同名片段。
- **讀不到的**：自訂屬性（Blender 的 Custom Properties）、FBX 的 user properties、相機、燈。要用的話得改 adapter（目前沒有需求）。

### 1.5 現況和美術規格草稿對不上的地方

美術規格草稿（主 checkout `build/evidence/art-spec-draft/art-asset-spec.zh-Hant.md`）第 7 節寫「FP：槍掛在手臂的 `main_j`」「手臂一個檔、每把槍一個 FP 檔」。這是「手臂帶動畫、槍掛上去」的想法；但現在的檔案裡，**動畫是手臂與槍一起做的，`main_j` 自己有動畫**，手臂沒有帶槍的骨頭。拆的時候動畫要放哪一邊，是第 3 節的主題。

## 2. 別家怎麼組武器模型

| | 第一人稱的手臂與槍 | 動畫放哪 | 第一／第三人稱 | 掛點 | 出處 |
|---|---|---|---|---|---|
| **Bevy** | 範例把手臂當相機的子 entity，用 `RenderLayers` 分開畫、固定 FOV（`examples/camera/first_person_view_model.rs:4-35`） | glTF 的每個節點都 spawn 成 entity（`crates/bevy_gltf/src/loader/mod.rs:1638-1641`）；片段的目標是「骨頭名字路徑的 hash」，**任何骨頭名字對得上的骨架都能播同一個片段**（`crates/bevy_animation/src/lib.rs:170-175`、`:1344-1349`） | 範例只有第一人稱 | 掛件是骨頭 entity 的子 entity（`ChildOf`），沒有 socket 型別 | 源碼 |
| **Godot** | 網格可以綁到別的檔案的 `Skeleton3D`：`MeshInstance3D` 的 `skeleton` 路徑（`scene/3d/mesh_instance_3d.h:52,87`），`Skin` 的 bind 可以用**骨頭名字**（`add_named_bind`，`scene/resources/3d/skin.h:62,73`），綁的時候照名字找骨（`scene/3d/skeleton_3d.cpp:404-420`，找不到就報錯並用第 0 根） | 動畫在 `Skeleton3D` 那一邊，網格只跟著 | — | `BoneAttachment3D`：骨頭名字＋在 `skeleton_updated` 之後跟著動（`scene/3d/bone_attachment_3d.h:35-46`、`.cpp:134-141`） | 源碼 |
| **Unreal（模組化角色）** | 三種：**Leader Pose**（子網格不跑動畫、直接用 leader 的骨頭；子骨架必須是 leader 的子集、結構完全一致，多的或少的骨用參考姿勢）、**Copy Pose From Mesh**（子網格自己有 AnimGraph，只複製名字對得上的骨；source 要先 tick，否則拿到上一幀）、**Skeletal Mesh Merge**（執行時合成一個網格，只能跑一個動畫） | 在 leader／source 那一邊 | — | Skeletal Mesh Socket（骨頭上的具名變換） | https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-modular-characters-in-unreal-engine （5.9 頁面；成本表：Leader Pose 設定最省、render 成本高；Copy Pose 兩條 thread 都最貴） |
| **Unreal（第一人稱）** | 5.x 有 First Person Rendering：元件標 `FirstPerson`（用相機的第一人稱 FOV 畫）或 `WorldSpaceRepresentation`（給世界與其他玩家看、投影子）。**也就是第一人稱與世界各一份** | — | 分開兩份 | — | https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-rendering ；`EFirstPersonPrimitiveType`（https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/EFirstPersonPrimitiveType ） |
| **Lyra** | 只有第三人稱 | 武器的 equipment instance 帶「裝備／卸下時的動畫組」 | — | equipment definition 決定「掛在 pawn 的哪裡」；屬性名（`ActorsToSpawn`、`AttachSocket`）只在社群頁面看到 | △ 社群 https://x157.github.io/UE5/LyraStarterGame/Equipment 、https://x157.github.io/UE5/LyraStarterGame/Weapons ；官方未確認 |
| **Source（CS:GO、Garry's Mod）** | 手臂（`c_arms`）和武器是不同模型，用 **bonemerge**（照骨頭名字複製變換）合起來；武器模型用 `$bonemerge` 而不是 attachment。**誰 merge 到誰，兩份二手資料說法相反**（一說手臂 merge 到武器、一說 Source 1 是武器 merge 到共用的手臂，s&box 相反） | 未確認 | `v_`／`w_` 分開（記憶） | 未確認 | △ 搜尋摘要：https://developer.valvesoftware.com/wiki/$bonemerge 、https://developer.valvesoftware.com/wiki/Rootbone 、https://wiki.facepunch.com/gmod/Using_Viewmodel_Hands （以上都 HTTP 403 讀不到原文）；s&box 的頁面 404。**全部未確認** |
| **塔科夫** | 第一人稱看到的是角色本身的手臂（true first person）——記憶，未確認 | — | — | — | 未確認。塔科夫只是參考之一 |

### 2.1 共同點

1. **網格分開、姿勢只算一次**：Unreal 的 Leader Pose／Copy Pose、Godot 的 named bind、Bevy 的「名字路徑對得上就能播」、Source 的 bonemerge，做法都是「一個骨架算姿勢，其他網格照骨頭名字跟著」。沒有一家要求「兩個檔各帶一份同名片段，再對齊時間同時播」。
2. **名字是連結**：跟著的那一邊靠骨頭名字找到骨頭；名字對不上的骨，退回參考姿勢（Unreal）或報錯（Godot）。
3. **第一人稱與第三人稱多半是兩份**：Unreal 5 的 First Person Rendering 明寫兩種元件；Source 的 `v_`／`w_`（未確認）。只有 true first person（塔科夫，未確認）用同一份身體。
4. **掛點是「節點名＋局部偏移」**：Unreal socket、Godot `BoneAttachment3D`／`Marker3D`、Bevy 的子 entity。和研究 #363 第 2.2 節的結論一樣。

## 3. 第 1 題：第一人稱要不要把手臂和槍拆開

### 3.1 例子

現在要做步槍時，有兩條路：

- 不拆：做一個 `Rifle_FPS.fbx`，裡面也放一份手臂網格。之後要把手套換成黑色的長手套（網格不同），射釘槍和步槍兩個檔都要重新匯出；有十把槍就是十個檔。
- 拆：手臂做一個檔，每把槍一個檔。換手套只動手臂那一個檔。

問題在於：**拆的是網格，動畫跟著哪一邊？** 步槍的 Reload 是「手＋步槍＋彈匣」一起做的動作，射釘槍的 Reload 是另一套。

### 3.2 選項

| | 做法 | 好處 | 代價 |
|---|---|---|---|
| **(a)** | **武器檔帶姿勢**：每把武器的 FBX 帶完整骨架（手臂骨＋槍的骨）、槍的網格、這把武器的片段。手臂的 FBX 只帶同名的手臂骨與手臂網格，沒有片段。執行時用武器檔取樣一次，手臂照骨頭名字複製局部姿勢（Unreal Copy Pose From Mesh 的「只複製名字對得上的骨」） | 一個片段一個時間來源，不會兩邊對不齊；手臂、手套只有一份；每把槍的骨（彈匣、槍機）各自不同也沒關係；`main_j` 的現有做法（槍由自己的骨帶、手配合）原樣保留 | 每把武器的檔都帶一份手臂骨（只有骨，沒有網格；54 根骨很小）；手臂骨的名字與層級要全遊戲固定（2.1 第 2 點） |
| (b) | **手臂檔帶姿勢**：手臂的 FBX 帶所有片段；槍是剛體，掛在手臂的 `main_j`；槍上會動的零件自帶小骨架與**同名片段**，用同一個時間各自取樣 | 槍的檔很單純 | 手臂檔要裝下每一把槍的片段（十把槍就五十個片段在同一檔）；兩個檔的同名片段要分開匯出還要長度一致，重新匯出一邊就可能錯開；別家都沒這樣做（2.1 第 1 點） |
| (c) | **不拆**（現況）：每把武器一個 FBX，手臂網格每把各一份 | 不用改程式；換手套的**顏色**已經可以：材質名 → 貼圖的表（`nailer_v2_viewmodel.json` 的 `materials`）每把槍各自指定 | 換手套的**形狀**做不到；手臂網格改了要重新匯出每一把槍 |

### 3.3 推薦：(a)，時機在 W2

1. **要拆，而且拆的是網格，不是動畫**。這修正主會話初步看法的一部分：「槍掛在 `main_j`」在 (a) 裡不需要做成掛點，因為 `main_j` 本來就是骨架裡有動畫的一根骨，槍的網格就綁在它上面，和現在一樣。
2. **`main_j` 能直接用**，當「武器的根骨」。建議改名（例：`weapon_root`），因為程式只讀 JSON 裡寫的名字（`anchor_node`），改名只動資料，不動程式。`Mark23_Root` 也是舊武器（Mark23）留下的名字，同樣建議改成通用的 `root`（第 6 節）。
3. **槍上會動的零件**：照現在的做法，做成武器根骨底下的骨頭（`Mag_j`、`side_j`），網格剛體綁一根骨。每把槍可以有不同的零件骨，因為這些骨只在那把槍的檔裡；手臂檔不用知道。不用「自帶骨架＋同名片段同時播」（那是 (b)）。
4. **手套與皮膚怎麼換**：
   - 顏色、花紋：換貼圖。現在的材質表已經做得到，不用拆（`ClientWorld.cpp:2071-2076`）。
   - 形狀（長手套、袖子）：換手臂的 FBX。這需要 (a) 的拆法。
   - 換手套屬於玩家的外觀，不屬於武器：手臂的檔建議放在 `characters/player/` 底下（例 `characters/player/arms/`），不放在 `weapons/<weapon>/`（照 `docs/object_fps_pvp/assets.zh-Hant.md` 第 2 節「依對象分資料夾」）。
5. **時機**：W2 做（要有第二把槍的素材才有意義）。W1 不先加手臂欄位，因為沒有素材時只是一個空殼（0.2 節）。W1 只要把資料整理成「之後加一個選填欄位就能拆」的形狀（第 5 節）。

### 3.4 (a) 在程式裡長什麼樣（遊戲層，W2 那一批）

用的都是 model 已有的函式：

1. 讀檔時：`BindByName(武器.nodes, 手臂.nodes)`（`Retarget.hpp:65-66`）得到對照；檢查「手臂的每一根骨，武器都有」（`unmatchedTarget` 只能是網格節點），否則報錯。和 `CharacterRig.cpp:172-177` 對 UAL 動畫庫的寫法一樣。
2. 每幀：武器照現在的 `PoseViewmodel` 取樣一次得到局部姿勢；手臂的局部姿勢＝對照表上的骨用武器的值，其他用 rest；兩邊各自 `ComputeModelSpace`，各自寫進自己網格 entity 的 `SkinPose`。
3. 不需要 `Retarget`：兩個檔是同一副 Blender 骨架匯出的，rest 一樣。

## 4. 第 2 題：第一人稱和第三人稱的槍，一個模型還是兩個

### 4.1 例子

你在第一人稱看到的射釘槍，貼著鏡頭、FOV 55°、near clip 1 cm，面數一萬；別人在 30 公尺外看到你手上的射釘槍，只有幾十個像素。第三人稱的槍還要讓 Match 讀左手握點（G3、D5 (b)）。

### 4.2 推薦：兩個模型，靠「同名節點」對齊，不靠數字對齊

| 項目 | 推薦 | 理由 |
|---|---|---|
| 模型 | **維持兩個**：第一人稱（高模，含零件骨、片段）、第三人稱（低模、剛體） | 2.1 第 3 點；兩邊要的東西不同（第一人稱要零件動畫與近距離細節，第三人稱要面數少、Match 要讀得快）。true first person（塔科夫，未確認）是另一種產品設計，會連帶改相機與身體，不在這張卡 |
| 槍口 | 兩個模型**各自**有一個 Empty `muzzle`（第一人稱掛在武器根骨下，第三人稱掛在網格根下） | 射擊判定用眼睛的射線（`PvpWorld.cpp:486-489`），槍口只給畫面（火光、彈道）。自己的畫面用第一人稱的、別人的畫面用第三人稱的。現在的 `muzzle {node, local_position_meters}` 沒人讀，可以改成只寫節點名 |
| 右手握點 | 第三人稱槍的**原點放在右手握把**（美術規格草稿第 1 節已經這樣寫），角色那一邊的 `hand_r` 掛點只記「手掌握把的位置」，每把槍的檔可再加一個微調 | 現在 `presentation.json` 的掛點縮放 0.107491 是在補這把槍的大小；照「原點在握把、真實尺寸」做，掛點會接近單位矩陣，換槍不用重調角色 |
| 左手握點 | 第三人稱槍上一個 Empty `grip_l`（位置＋朝向） | G3：IK 的目標＝`hand_r` 的模型空間 × 掛點 × `grip_l`；位置給 `SolveTwoBoneIK`，朝向給 IK 之後的手（U2 (a)，`Animation.hpp`（#377）`SolveTwoBoneIK` 的註解）。右手不受左手 IK 影響（兩條鏈只共用 `spine_03` 以上），所以順序是「先放槍、再解左手」 |
| 第一人稱要不要 `grip_l` | **不要** | 第一人稱的手是動畫師對著槍做的，不跑 IK |
| 高低模、LOD | 現在不做 LOD。要做時：同一個 FBX 放 `_LOD0`、`_LOD1` 網格，共用同一組節點（`muzzle`、`grip_l` 只有一份）；切換用 render 已有的 `VisibilityRange`（`engine/modules/render/include/engine/render/Drawables.hpp:198-206`，照 Bevy） | Engine 已經有切換的機制；同一檔放多個 LOD，掛點就不會因為 LOD 不同而對不上 |
| 兩邊的對齊 | **不對齊數字**。兩個模型各自用同一組節點名；第一人稱的大小、位置只看 viewmodel 相機（`offset_meters`、FOV） | 兩邊用不同的相機與 FOV（Bevy 範例的理由），數字對齊沒有意義。要一致的只有「節點名」與「槍的外觀」 |

### 4.3 Match 要讀第三人稱的槍

D5 (b) 之後，Match 要知道 `grip_l`。兩種寫法：

- 節點寫在 FBX（推薦）：Match 讀第三人稱槍的 FBX 時用 `FbxSettings{meshes = false}`（`FbxLoader.hpp:54-61`），只要節點。Match 已經會讀角色的 FBX（`CharacterRig.cpp:150`）。和 Unreal socket、Godot `Marker3D` 一樣，在 Blender 裡看得到、拖得動。
- 數字寫在 JSON：不用讀 FBX，但要有工具才知道數字對不對。

這是 G3 那一批的細節，本卡只記推薦。**要提醒 G3**：握點進了判定，模式配置的內容比對（`rulesDigest`）是否要涵蓋它，開 G3 時查（本卡沒查內容比對的範圍）。

## 5. 第 3 題：每把武器的資料

### 5.1 例子

W3 背包裡有射釘槍和步槍。按 2 切到步槍：Client 要知道步槍第一人稱用哪個 FBX、哪組片段、Reload 的哪個時刻放哪個聲音；別人的 Client 要知道第三人稱用哪個 FBX、掛在手上的哪裡、上半身換哪組姿勢、左手握哪裡；Match 要知道步槍的數值、上半身姿勢組、左手握點。

### 5.2 推薦的形狀（W1、W2、G3 各加一部分）

`weapon.json` 當**索引**，兩個視角各自的定義檔原樣留在自己的資料夾（`weapons/<weapon>/{viewmodel,world}/`）。這樣 W1 不用改現有兩份定義檔的欄位。資料夾裡依種類分 `models/`、`animations/`、`textures/`，這是使用者 2026-11-28 定的結構（第 14 節）。

```text
weapons/<weapon>/
├── weapon.json                 W1：規則數值（CombatRules 搬來的）、兩個視角檔的 id、第三人稱的姿勢組覆寫（D4）
├── viewmodel/
│   ├── <weapon>_viewmodel.json 現有：模型、材質表、animset／anim_events／event_sounds 的 id、錨點、放置、FOV、後座力
│   │                           W2 加：arms（選填；沒寫＝手臂在武器的模型裡，就是現況）
│   ├── viewmodel.animset.json  role → 片段（Idle、Shoot、Reload、Draw、Hide）
│   ├── viewmodel.anim_events.json、viewmodel.event_sounds.json
│   ├── models/                 <Weapon>_FPS.fbx：骨架＋槍網格（W1 時仍是現在的一體檔；W2 之後不含手臂網格與片段）
│   ├── animations/             W2：<Weapon>_FPS_Anims.fbx（骨架＋這把槍的片段，無網格）
│   └── textures/
└── world/
    ├── character.json          現有：模型、材質覆寫
    │                           G3 加：left_grip_node（例 "grip_l"）；W1 或 G3 加：mount（掛點微調，預設單位矩陣）
    ├── models/                 <Weapon>_TPS.fbx：網格＋ Empty（muzzle、grip_l）
    └── textures/

characters/player/
├── presentation.json           現有的 weapon{node, …} 留下「手掌握把在哪」（每副骨架一份）
├── models/                     Player_V20.fbx
├── textures/
└── arms/                       W2：第一人稱手臂（arms.json＝手套的材質表；models/ 手臂骨＋手臂網格；textures/）
```

| 欄位 | 誰讀 | 對齊的已定事項 | 哪一批加 |
|---|---|---|---|
| 規則數值 | Match、Client | W1（`mode-config-declaration.zh-Hant.md:265`） | W1 |
| 第一人稱模型、材質表、片段組、事件、聲音、錨點、放置、FOV、後座力 | Client | 現有欄位 | W1：只把寫死的檔名（`ClientWorld.cpp:2023`、`:2027`）改成從 `weapon.json` 找 |
| `arms`（選填） | Client | 本卡第 3 節 (a) | W2 |
| 第三人稱模型、材質覆寫 | Client | 現有欄位 | 不變 |
| 第三人稱姿勢組覆寫 | Match、Client | D4：基本組＋每把武器的覆寫表 | W1 的 G1（研究 #363 第 8 節） |
| 掛點微調 `mount` | Client（G3 之後 Match 也讀） | R4：右手掛點搬進武器 | W1 的 G1 或 G3 |
| `left_grip_node` | Match、Client | D5 (b)、G3 | G3 |
| `muzzle`（節點名） | Client（火光、彈道） | — | 第一次用到槍口時（現在沒人讀） |

**W1 的逐位元一致**：`mount` 從角色的檔搬到武器的檔，角色那一邊留單位矩陣時，`Multiply(單位矩陣, mount)` 在 float 裡和 `mount` 完全相同（乘 1、加 0），第三人稱的槍位置不變。槍的位置也不進判定（1.3 節）。

## 6. 第 4 題：Blender 的製作與匯出

### 6.1 例子

你在 Blender 裡打開 `nailer.blend`，一副 Armature 下面有手臂、槍、彈匣，五個 action（Idle、Shoot…）。現在要匯出「射釘槍檔」和「手臂檔」兩個 FBX。

### 6.2 匯出器的行為（Blender 4.5 的源碼；5.2 未確認）

| 事實 | 出處（`io_scene_fbx/`） | 對拆分的意思 |
|---|---|---|
| 「Selected Objects」只匯出選取且可見的物件 | `__init__.py:305-308` | 骨架**必須一起選**：只有選到的 Armature 物件才會被包成骨架（`export_fbx_bin.py:2825-2829`） |
| 骨架被包進去時，**它所有的骨都會匯出**；「Only Deform Bones」才會只留有 deform 的骨（和底下有 deform 子骨的骨） | `export_fbx_bin.py:2139-2155`；`__init__.py:473-476` | 手臂檔與武器檔都會帶完整的 54 根骨（3.2 (a) 本來就要這樣）；不要開「Only Deform Bones」，否則手臂檔會少掉 `main_j`（它在手臂檔裡沒有 deform），對照時沒差，但層級會不一樣 |
| 「All Actions」：每個 action 變成一個 AnimStack，「動畫物件會拿到所有和它相容的 action」 | `__init__.py:506-511` | **同一個 .blend 放了兩把槍的 action，匯出射釘槍時也會帶步槍的 Reload**。建議一把武器一個 .blend；手臂檔匯出時關「Baked Animation」（`__init__.py:489-492`） |
| 「Key All Bones」預設開：每根骨至少一個 key | `__init__.py:494-498` | 現況每個片段 54 根骨都有 key 就是這個 |
| 「Add Leaf Bones」預設開：每條鏈尾巴加一根骨 | `__init__.py:445-448` | 現在的檔沒有 `_end` 骨，表示是關的；兩個檔要同一個設定，否則骨頭清單不一樣 |
| 物件可以用「Bone」當父（`parent_type == 'BONE'`），匯出時它的父就是那根骨 | `fbx_utils.py:1661-1666` | `muzzle`、`grip_l` 用 Empty、父設成骨頭，匯出後就是那根骨底下的節點（1.4 節：adapter 讀得到） |
| 「Apply Scalings」有 All Local／FBX Units Scale／FBX Custom Scale／FBX All；「Apply Unit」看 Blender 的單位設定；「Apply Transform」標明實驗性、和骨架或動畫一起用已知會壞 | `__init__.py:332-345`、`:86-90` | 不要開「Apply Transform」。`Armature` 的 0.01 縮放，建議在 Blender 裡先對 Armature 套用縮放（Ctrl+A），讓骨頭的局部座標變成公尺。**本卡沒有在 Blender 裡試**；改完要用第 6.4 節的檢查確認 |

### 6.3 推薦的流程

```text
characters_arms.blend（手臂、手套，全遊戲一份）
  └─ 匯出 arms FBX：選 Armature＋手臂網格；Baked Animation 關
nailer.blend（一把武器一個檔；手臂用 Link／Append 從上面帶進來，只當動畫時的參考）
  ├─ 匯出 Nailer_FPS.fbx：選 Armature＋槍的網格＋ muzzle Empty；All Actions 開
  └─ 匯出 Nailer_TPS.fbx：選第三人稱的槍＋ muzzle、grip_l 的 Empty；原點在右手握把；真實尺寸
```

限制與注意：

1. 手臂骨的**名字與層級**全遊戲固定。改一根骨的名字，所有武器的 .blend 都要重新匯出。
2. 武器根骨（現在的 `main_j`）以下的骨，每把武器可以不同。
3. 手臂檔與武器檔要用同一副 Armature（同一個物件變換，包含現在的 15° 偏航），否則兩邊的 rest 不同，3.4 節「不用 Retarget」就不成立。
4. Link 的手臂只當參考：匯出武器檔時不要選到手臂網格。
5. Blender 改骨頭名字時，非目前 action 的 F-Curve 會不會跟著改名，本卡沒查（未確認）。要改名的話，先在一份複本上試。

### 6.4 骨頭命名

- **程式只讀 JSON 裡寫的節點名**（`anchor_node`、之後的 `muzzle`、`grip_l`），不看命名風格。所以命名規則是美術的約定，不是程式的契約。
- 建議：通用、英文、小寫加底線，不放產品名（`Mark23_Root` → `root`、`main_j` → `weapon_root`；零件骨 `weapon_mag`、`weapon_slide`；Empty `muzzle`、`grip_l`）。
- 第一人稱的手臂現在是 Mixamo 風格（`LeftForeArm1`、`_1` 後綴是 Blender 重名時加的），第三人稱是 UE mannequin 風格（`lowerarm_l`、`hand_r`）。兩者不會互相 retarget，所以**不統一也能用**；要統一的話，在 W2 重新匯出時一起做最省事（第 6.3 節限制 1）。這是本卡的建議，不是決策點。
- 重新匯出後的檢查（本卡用的腳本可以做，或之後的 model 工具）：`Armature` 縮放是 1、武器根骨的局部位置是公尺級（約 1.4，不是 140）、手臂檔的骨都在武器檔裡、每個片段的長度與現在一樣。

## 7. 第 5 題：Engine 要不要新增機制

先寫前提：「只有 Engine 做得到」的才放 Engine。

| 需要的 | 前提 | 各家 | gyo2 已經有的 | 結論 |
|---|---|---|---|---|
| 把一個模型掛到另一個模型的骨頭上 | **作者能處理**：一個矩陣乘法 | Bevy：骨頭是 entity，`ChildOf` 就好；Godot `BoneAttachment3D`；Unreal socket | `WeaponToWorld`（`CharacterRig.cpp:349-351`）＋`FindNode`＋`ComputeModelSpace`；第三人稱已經在用 | **不加**。gyo2 的骨頭不是 entity（姿勢在 `SkinPose` 裡），「掛件元件」要一個排在取樣之後、transform 傳播之前的 system，等於把第 2 層的一部分搬進 Engine，和 D1 不合 |
| 多個模型同一時刻用同一個姿勢 | **作者能處理**：取樣一次、照名字複製 | Unreal Copy Pose From Mesh／Leader Pose；Godot named bind；Bevy 名字路徑 | `BindByName`（`Retarget.hpp:65-66`）、`SampleClip`、`ComputeModelSpace`；第三人稱的動畫庫已經這樣搬（`CharacterRig.cpp:172-184`） | **不加** |
| 多個模型各自取樣同名片段 | 作者能處理：呼叫兩次 `SampleClip` | 別家都不這樣做（2.1 第 1 點） | `FindClip`、`SampleClip` | 不加；也不推薦這種做法（3.2 (b)） |
| 換貼圖、換手套 | 作者能處理：材質表是遊戲的資料 | — | `Mesh3d::image`（`Drawables.hpp:145-151`）、材質表 | 不加 |
| LOD | 機制已經在 Engine | Bevy `VisibilityRange` | `VisibilityRange`（`Drawables.hpp:198-206`） | 不加 |
| 每把武器的事件 → 聲音 | 機制在 E1 | Bevy 事件在 Engine | `CrossedEvents`（#377） | 已在 E1，G4 用它 |
| 握點（IK 目標） | 作者能處理：節點名＋偏移 | 同上 | `FindNode`、E2 的 `SolveTwoBoneIK` | 不加 socket 型別（#363 R4 已這樣判） |

**結論：Engine 0 批**。主會話初步看法的這一點成立。

**之後要重看的條件**（不是現在）：出現第二個產品，或者同一個角色身上要掛很多東西（配件、背包、頭盔），每一件都要手寫一次 `WeaponToWorld` 那種乘法，而且這些東西不進判定。那時再照 Bevy 的做法比較「骨頭 entity＋`ChildOf`」，和 D1 的重開條件（「等有產品要 Bevy `AnimationPlayer` 那種用法」）一起看。

## 8. 第 6 題：之後的擴充（只列，不設計）

| 項目 | 會碰到什麼 | 現在要不要預留 |
|---|---|---|
| 瞄具（換鏡） | 第一人稱的槍上要有一個 Empty（例 `sight_socket`），瞄具模型掛上去；開鏡時相機要對準瞄具的某個點（ADS），那是放置／相機的規則 | 不預留。到時候加一個 Empty 就好（Blender 端的工作） |
| 換彈匣（擴容） | 彈匣是武器根骨下的一根骨＋剛體網格，換一個網格綁同一根骨就好；換了數值是規則 | 不預留 |
| 第三人稱的零件動畫（別人換彈時彈匣也拔出來） | 第三人稱的槍要有零件骨與片段，時間跟著角色的 Reload；照 3.2 (a) 的想法，就是「角色的片段帶槍的零件骨」或「槍的同名片段」，要另外比 | 不預留；現在第三人稱的槍沒有骨頭 |
| 換槍皮 | 第一人稱與第三人稱的材質表各換一組貼圖；現有的材質表就是這個形狀 | 不預留 |
| 火光、彈道的起點 | 用 `muzzle` 節點；自己的畫面用第一人稱的、別人的用第三人稱的。兩個位置在世界裡不一樣，別家怎麼讓彈道對得上，本卡沒查 | 不預留 |
| 撿起與丟掉（W4） | 地上的槍用第三人稱的模型（美術規格草稿第 3 節已這樣寫）；放在地上時的原點與握把原點不同，要不要另一個節點，到 W4 再看 | 不預留 |
| true first person（看得到自己的身體） | 相機、身體、手臂都要改，是產品設計的決定 | 不預留 |

## 9. 推薦總結、對主會話初步看法的驗證

主會話的初步看法：「要拆，時機排在 W2；Engine 不用加機制。」

| 部分 | 驗證結果 |
|---|---|
| 要拆 | **成立，但要說清楚拆的是什麼**：拆網格（手臂、手套一份，每把槍一份），**不拆動畫**。片段留在每把武器的檔裡，手臂照骨頭名字跟著（3.2 (a)） |
| 時機 W2 | **成立**。W1 不先加 `arms` 欄位（沒有素材就是空殼），只把寫死的檔名改成從 `weapon.json` 找 |
| 「rig 已有 `main_j`，主要是匯出方式不同」 | **大致成立，補一點**：`main_j` 不是手的子節點，是和手臂平行、自己有動畫的武器根骨。(a) 的做法裡它不需要當掛點，原樣保留就好。匯出時另外要注意「All Actions 會把同檔的別把槍的 action 也帶進來」「骨架要一起選」「縮放 0.01」（第 6 節） |
| Engine 不用加機制 | **成立**（第 7 節）。用到的 `BindByName`、`SampleClip`、`ComputeModelSpace`、`FindNode`、`VisibilityRange` 都已經在 Engine；E1、E2 是已排的批 |

另外查到、主會話沒提的：

1. `muzzle` 欄位程式沒讀，單位是公分不是公尺（1.3 節）。
2. 第三人稱槍的掛點縮放 0.107491 放在角色的檔裡，是在補這把槍的大小（1.2 節）。
3. 第三人稱的槍沒有任何節點，G3 之前要加 `grip_l`（4.2 節）。
4. `LoadViewmodel` 寫死了檔名（1.3 節），W1 要改。

## 10. 要使用者決定的點

只列和別家不同、或會影響使用者的 Blender 工作方式的。

| # | 問題 | 機制與取捨 | 推薦 | 證據 |
|---|---|---|---|---|
| **U1** | 拆開之後，**動畫跟著哪一邊**？ | **(a) 武器檔帶骨架與片段，手臂照骨頭名字複製姿勢**：一個時間來源；手臂一份；你的 Blender 流程是「一把武器一個 .blend、手臂用 Link 帶進來」。別家的共同做法（Unreal Copy Pose From Mesh／Leader Pose、Godot named bind、Bevy 名字路徑）。代價：手臂骨的名字與層級全遊戲固定，改了要重新匯出所有武器。<br>**(b) 手臂檔帶片段，槍掛在 `main_j`，零件自帶同名片段**：槍檔單純；但手臂檔會裝下每把槍的片段，兩個檔的同名片段要長度一致，別家沒這樣做。這是美術規格草稿第 7 節原本的想法。<br>**(c) 不拆**：程式不用改；換手套只能換顏色，手臂改了要重新匯出每把槍 | **(a)**；**使用者 2026-11-28 選 (a)**（第 15 節） | 第 2 節表、2.1；第 3.2、3.4 節；匯出器的行為第 6.2 節 |

不列成決策點、本卡自己判的（照別家與已定事項）：第一／第三人稱兩個模型（4.2，別家共同做法）；握點用 Empty 節點（4.2，和 #363 R4 一致）；`weapon.json` 當索引（5.2）；Engine 0 批（第 7 節）；骨頭命名（6.4，程式不依賴）。

## 11. 批次

模型／檔位照 `agents/navigator.md`、`agents/effort-gearbox.md`。Engine 沒有批。

```text
W 線 × 武器模型（本卡新增或補充的部分）
├─ Engine：0 批
├─ G1  high｜（既有，W1）＋本卡補充：LoadViewmodel 的檔名改從 weapon.json 找；mount 搬進 world/character.json；不加 arms 欄；素材搬進 models／textures（第 14 節）
├─ A1  美術｜（使用者，W2 素材）Blender 重新匯出：手臂檔＋武器檔、Armature 套用縮放、muzzle／grip_l 的 Empty、第三人稱真實尺寸
├─ G7  high｜（新，W2）第一人稱手臂分檔：arms 選填欄、依名字複製姿勢、手臂的材質表（需 A1、G1）
└─ G3  high｜（既有）＋本卡補充：第三人稱槍的 grip_l 節點；Match 用 meshes=false 讀槍的節點（需 A1 的第三人稱部分、E2）
```

| 代號 | 內容 | 上段／下段（根據） | 實作者／審查者／檢證者（模型／檔位） |
|---|---|---|---|
| G1 `v8-product-pvp-weapon-data`（W1，既有） | 研究 #363 第 8 節的內容，加：`LoadViewmodel` 不再寫死 `nailer_v2_viewmodel.json`、`Nailer1_FPS.fbx`（`ClientWorld.cpp:2023`、`:2027`），改從 `weapon.json` 找；右手掛點 `mount` 搬進 `world/character.json`，角色留單位矩陣（5.2 節，逐位元一致）；`muzzle` 留著或刪掉由 G1 判（現在沒人讀）；**素材資料夾照第 14 節重排**（只搬既有的檔，`models/`、`textures/`；`animations/` 等 W2 有檔才建）：catalog 的 `path` 跟著改、AssetId 不改，用路徑讀檔的程式一起改，`docs/object_fps_pvp/assets.zh-Hant.md` 第 2、3 節同一批改 | 上段（資料格式） | 照 #363：Opus／high；審查 Opus／high |
| A1 `v8-product-pvp-weapon-art-reexport`（使用者的工作） | 第 6.3 節的流程；檢查照 6.4 節最後一條 | 不是程式批 | 使用者。檢查可由 G7 的卡用腳本做 |
| G7 `v8-product-pvp-viewmodel-arms-split`（W2，新） | `<weapon>_viewmodel.json` 加選填 `arms`（沒寫＝現況）；第一人稱的片段改從 `viewmodel/animations/` 的檔拿，用 `BindByName` 對到模型檔的骨（照 `CharacterRig.cpp:172-184`，第 14.3 節）；讀檔時 `BindByName` 並檢查手臂的骨都在武器檔裡；`PoseViewmodel` 對手臂複製局部姿勢、各自 `ComputeModelSpace`；手臂的材質表；`characters/player/arms/` 的 catalog 項；外部測試：沒寫 `arms` 時姿勢與現在逐位元相同、少一根骨時報錯、手臂跟著武器的片段動 | 上段（資料格式） | Opus／high；純粹的 Code Review Fable／high；檢證者 Opus／high |
| G3 `v8-product-pvp-left-hand-ik`（既有） | #363 第 8 節的內容，加：`world/character.json` 的 `left_grip_node`；Match 讀第三人稱槍的 FBX（`meshes = false`）取 `grip_l`；IK 目標＝`hand_r` × `mount` × `grip_l`，手的朝向取 `grip_l` 的朝向；查握點要不要進內容比對（4.3 節） | 上段（判定） | 照 #363：Opus／high；審查 Opus／high；檢證者 Opus／high |

順序：G1（W1）→ A1（W2 的素材）→ G7（W2）；G3 需要 A1 的第三人稱部分（`grip_l`）與 E2，可以在 G7 之前或之後。G7 不改交換內容，不必和 G2、G6 同一段升 Codec 版本。

## 12. 美術規格草稿要補的條目（建議文字，不直接改那個檔）

對象：主會話 checkout 的 `build/evidence/art-spec-draft/art-asset-spec.zh-Hant.md`。

**第 1 節 共通規則**，「單位」那一列的備註改成：

> 檔案單位是公尺（`UnitScaleFactor` 100），但 `Armature` 帶 0.01 的縮放，骨頭的局部座標實際是公分（`muzzle` 的 13.5／-20.2 就是這樣）。重新匯出前對 Armature 套用縮放（Ctrl+A），不要開匯出器的「Apply Transform」（匯出器標明和骨架一起用會壞）。

**第 2 節 骨架**，「第一人稱手臂」那一列的必要節點改成：

> 左右手、手指、武器根骨（現在叫 `main_j`，建議改 `weapon_root`；它是 root 的直接子節點，不是手的子節點）。手臂骨的名字與層級全遊戲固定，改了要重新匯出所有武器。

「武器自己的骨頭」那一列改成：

> 都在武器根骨底下，每把武器可以不同（例 `weapon_mag`、`weapon_slide`）；每個零件網格只綁一根骨、權重 1（剛體）。槍口用 Empty `muzzle`（父設成武器根骨），不再用「骨頭＋偏移」的數字。

**第 6 節 動畫**加一列：

> | 片段放哪 | 第一人稱的片段放在**每把武器的檔裡**（手臂＋槍一起做），手臂檔不帶片段。一把武器一個 .blend，避免「All Actions」把別把槍的 action 一起匯出 |

**第 7 節 武器**整節改成：

> | 項目 | 規定 |
> |---|---|
> | 檔案 | 手臂一個檔（全遊戲共用，放 `characters/player/arms/`）：Armature＋手臂網格，關「Baked Animation」。每把槍兩個檔：第一人稱（Armature＋槍的網格＋ Empty＋這把槍的片段）、第三人稱（剛體網格＋ Empty） |
> | 第一人稱怎麼組 | 遊戲用武器檔取樣姿勢，手臂照骨頭名字跟著；所以手臂檔的每根骨，武器檔都要有（兩個檔用同一副 Armature 匯出，不開「Only Deform Bones」） |
> | 掛點 | 第一人稱：不需要掛點（槍綁在武器根骨上）。第三人稱：原點放在右手握把、真實尺寸；角色的 `hand_r` 掛點只記手掌的位置 |
> | 槍口 | 兩個檔各有一個 Empty `muzzle` |
> | 握點（IK 用） | 只有第三人稱：Empty `grip_l`（位置＋手掌朝向）。第一人稱不跑 IK，不需要 |
> | LOD | 要做時同一個檔放 `_LOD0`、`_LOD1`，共用同一組 Empty |
> | stance | 每把槍標用哪組持槍姿勢：〈pistol／rifle／…〉（D4：基本組＋覆寫表） |

**第 8 節 交件前檢查**加：

> - [ ] Armature 的縮放是 1，武器根骨的位置是公尺級
> - [ ] 手臂檔的骨都在武器檔裡；兩個檔的「Add Leaf Bones」設定一樣
> - [ ] 武器檔只帶這把槍的片段，片段長度與上一版一樣（沒有刻意改的話）
> - [ ] 第三人稱的槍有 `muzzle`、`grip_l`，原點在右手握把

**第 9 節 現況對照**，「`Nailer2_TPS.fbx`」那一列的差距改成：

> 1,388 三角面；沒有任何節點（缺 `muzzle`、`grip_l`）；網格約 1 單位大小，靠 `presentation.json` 的掛點縮放 0.107491 縮小，要改成真實尺寸、原點在握把

## 13. 未確認、沒做的

- 沒有在 gyo2 的 ufbx adapter 或 Blender 裡實際跑過任何東西；FBX 的結構是自己的解析腳本讀的，單位換算是推論。
- Blender 的行為讀的是 4.5 的匯出器源碼，素材是 5.2.3 匯出的；5.2 是否相同，未確認。Blender 改骨頭名字時其他 action 會不會跟著改，未確認。
- Source、CS:GO、Garry's Mod 的做法只有搜尋摘要（原頁 403、s&box 404），而且兩份說法相反；塔科夫、Valorant 是記憶；Lyra 的屬性名只在社群頁面。都不當推薦的根據，推薦的根據是 Unreal 官方文件、Godot 與 Bevy 的源碼、gyo2 自己的先例。
- 第一人稱與第三人稱的槍外觀是不是同一個設計，沒看圖。
- G3 的握點要不要進內容比對，沒查。
- 沒做重的本機量測。

## 14. 素材資料夾結構（使用者 2026-11-28 定，排進 W1）

### 14.1 決定

**原話**：「現在，我要做美術資源和3D資源的整理，請給我一個資料夾結構。我想把動畫、模型、貼圖都分開」；看過本卡的提案後：「好，補進研究文件，轉給主會話排進 W1」。

**決定**：第一層照舊依對象分（一個角色、一把武器的一個視角各一個資料夾）；對象資料夾裡依種類分 `models/`、`animations/`、`textures/`，定義 JSON 放對象資料夾的最上層。好幾個對象共用的動畫庫照舊放 `animations/<library>/`。

**比過的**：第一層就依種類分（`models/`、`animations/`、`textures/` 各一棵樹）。不採用的理由：一把武器的檔會散在三棵樹，刪掉或換掉一個對象要到三處找。

**理由**：使用者要的是種類分開；現行規範（`docs/object_fps_pvp/assets.zh-Hant.md` 第 2 節「依對象分資料夾」）要的是同一個對象的檔放一起。兩層分開排，兩邊都滿足。對象底下再分種類，在 Unreal 專案裡常見（Meshes、Animations、Textures），這是本卡的印象，沒有查證，所以不當根據。

### 14.2 結構

repo 裡的匯出檔（`assets/object_fps_pvp/`）：

```text
assets/object_fps_pvp/
├── content.json, asset_catalog.json
├── characters/
│   └── player/
│       ├── character.json, presentation.json, hitbox.json, animations.animset.json
│       ├── models/                     Player_V20.fbx
│       ├── animations/                 只有這個角色用的片段（有才建）
│       ├── textures/                   （有才建）
│       └── arms/                       W2：第一人稱手臂
│           ├── arms.json               手套的材質表
│           ├── models/                 完整骨架＋手臂網格，無片段
│           └── textures/
├── weapons/
│   └── <weapon>/
│       ├── weapon.json                 W1
│       ├── viewmodel/
│       │   ├── <weapon>_viewmodel.json, viewmodel.animset.json, viewmodel.anim_events.json, viewmodel.event_sounds.json
│       │   ├── models/                 完整骨架＋槍的網格
│       │   ├── animations/             W2：完整骨架＋這把槍的片段，無網格
│       │   └── textures/
│       └── world/
│           ├── character.json
│           ├── models/                 網格＋ muzzle、grip_l
│           └── textures/
└── animations/
    └── ual_mannequin/                  共用的動畫庫（照舊）
```

原始檔（主 checkout 的 `build/art-source/object_fps_pvp/`，git 不追蹤）的路徑和上面相同，底下再加交付版本：`<對象>/<交付版本>/{blend,textures,export}/`。例：`weapons/nailer_v2/viewmodel/version03/blend/nailer.blend`。

| 種類 | 放哪 | 匯出 |
|---|---|---|
| 定義 JSON | 對象資料夾最上層 | — |
| 模型 | `<對象>/models/` | 骨架＋網格；關「Baked Animation」 |
| 動畫 | 一個對象用：`<對象>/animations/`；多個對象共用：`animations/<library>/` | 骨架＋片段、無網格；只選 Armature、開「All Actions」 |
| 貼圖 | `<對象>/textures/`；第二個對象也要用時才搬到 `textures/<group>/` | 照舊 |
| 資料夾名 | 小寫 snake_case；交付的檔名保留原樣 | 照現行規範第 4 節 |

### 14.3 對批次的影響

1. **W1（G1）只搬既有的檔**，不建空資料夾（「空殼刪掉」）：
   - `characters/player/Player_V20.fbx` → `characters/player/models/`
   - `weapons/nailer_v2/viewmodel/Nailer1_FPS.fbx` → `weapons/nailer_v2/viewmodel/models/`。現在仍是手臂＋槍＋片段的一體檔，W2 重新匯出時才拆。
   - `weapons/nailer_v2/world/Nailer2_TPS.fbx` → `weapons/nailer_v2/world/models/`
   - 貼圖已經在 `textures/`，不動。
2. **catalog 只改 `path`，AssetId 不改**（現行規範第 5 節）：`asset_catalog.json:75`、`:81`、`:87`。走 catalog 讀的程式不用改。
3. **用路徑讀的程式要一起改**：`ClientWorld.cpp:2027`（`dir + "Nailer1_FPS.fbx"`）。G1 本來就要把 `LoadViewmodel` 寫死的檔名改成從 `weapon.json` 找（第 11 節），兩件事同一批做。搬檔前再 `git grep` 一次 `.fbx`。
4. **規範文件同一批改**：`docs/object_fps_pvp/assets.zh-Hant.md` 第 2 節的樹與第 3 節「3D 模型｜對象資料夾」，改成 `<對象>/models/`，並加 `<對象>/animations/`。這份文件寫在 master 時期，它提到的守衛 `tests/object_fps_pvp/test_asset_catalog.py` 在 `gyo2` 上沒有，所以不會撞到檢查；文件裡提到這個守衛的段落要不要一起整理，由 G1 判。
5. **逐位元一致不受影響**：檔案內容不變，只改路徑。
6. **W2（G7）多一件事**：第一人稱的片段改放 `viewmodel/animations/` 的獨立檔。模型檔多了網格節點，兩個檔的節點清單不同，所以要用 `BindByName` 把動畫檔的骨對到模型檔（第三人稱已經這樣做，`CharacterRig.cpp:172-184`）。研究的 (a) 不變：動畫還是每把武器一份，只是獨立成一個檔。Engine 不用改。

### 14.4 有敵人、有其他武器時（使用者 2026-11-28 照提案）

同一套規則往下長：**第一層依對象分，對象裡依種類分**。

```text
assets/object_fps_pvp/
├── characters/
│   ├── player/                  玩家
│   ├── crawler/                 怪物（例：爬行怪；名字照「它是什麼」取）
│   │   ├── character.json, presentation.json, hitbox.json, animations.animset.json
│   │   ├── models/
│   │   ├── animations/          只有牠用的片段
│   │   └── textures/
│   └── raider/                  人形敵人（例：拾荒者）
│       ├── character.json, hitbox.json, animations.animset.json
│       ├── models/              自己的外觀
│       └── textures/            片段直接用 animations/ual_mannequin/
├── weapons/
│   ├── nailer_v2/
│   └── rifle_a/                 第二把武器：結構和 nailer_v2 一樣
│       ├── weapon.json
│       ├── viewmodel/{models,animations,textures}/
│       └── world/
│           ├── models/, textures/
│           └── animations/      這把槍專屬的第三人稱持槍片段（D4 的覆寫表指到這裡）
├── animations/
│   ├── ual_mannequin/           人形共用：玩家、人形敵人
│   └── crawler_family/          同一副骨架的怪物共用（第二隻用到時才建）
└── textures/<group>/            兩個以上對象共用時才建
```

規則：

1. **敵人放 `characters/`，不另開 `enemies/`**。怪物和玩家要的是同一組東西：模型、骨架、片段、hitbox（肢體傷害 L1 (1c) 的形狀標籤也在角色的 `hitbox.json`）。「是不是敵人」由模式配置決定（掛不掛 AI），不是素材的屬性；人形敵人也可以用玩家的模型。資料夾名照「它是什麼」取，不照「它扮演什麼角色」取。這和使用者的原則「地圖適應規則，不由地圖決定規則」（`v8-arch-gyo2-mode-config-u3`）同一個方向：素材不綁玩法。
2. **共用的放上層，專屬的放對象裡**：片段只有一個對象用就放 `<對象>/animations/`，第二個對象也要用時才搬到 `animations/<library>/`；貼圖同一條規則（第二個對象要用時才建 `textures/<group>/`）。
3. **敵人拿槍用武器的 `world/`**：人形敵人拿步槍，掛 `weapons/rifle_a/world/` 的第三人稱模型，和別人看玩家拿槍是同一份；敵人不需要 `viewmodel/`。怪物的爪子、酸液是怪物模型的一部分，不放 `weapons/`。
4. **每把槍的第三人稱持槍片段放武器的 `world/animations/`**：誰拿這把槍誰就用（D4：基本組＋覆寫表，覆寫表在 `weapon.json`）。片段直接取自共用動畫庫時，留在 `animations/<library>/`，不複製。

**對批次的影響**：W1 不建上面任何新資料夾（只搬既有的檔，14.3 節）；這一節是之後加怪物、加武器時照著放的規則，寫進 `docs/object_fps_pvp/assets.zh-Hant.md` 時和 14.3 節第 4 條同一批。

**比過的**：(i) 另開 `enemies/`：把「敵人」這個玩法上的角色寫進資料夾，同一個模型換模式當友軍時位置就不對；(ii) 持槍片段放角色的 `animations/`：每個會拿槍的角色都要各放一份。

## 15. 使用者的決定（2026-11-28）

**原話**：「照建議，U1 選 (a)，敵人和武器也照提案」。

| # | 決定 | 和推薦的關係 |
|---|---|---|
| U1 | **(a)**：武器檔帶完整骨架與這把武器的片段，手臂檔只帶骨架與手臂網格、沒有片段；執行時用武器檔取樣一次，手臂照骨頭名字複製姿勢 | 照推薦 |
| 資料夾 | 第一層依對象、對象裡分 `models/`、`animations/`、`textures/`（14.1～14.3 節），排進 W1 | 照提案；主會話已記成 `v8-arch-gyo2-asset-folder-structure`（`gyo2` a092c86a） |
| 擴充 | 敵人放 `characters/`、共用的放上層、敵人拿槍用 `world/`、持槍片段放武器的 `world/animations/`（14.4 節） | 照提案 |

**比過的**：U1 的 (b)（手臂檔帶片段、槍的零件帶同名片段）與 (c)（不拆），見第 10 節；擴充的比較見 14.4 節。

**理由**：使用者沒有另外說。本卡的理由：U1 (a) 只有一個時間來源，手臂與手套只有一份，和別家的共同做法相同（2.1 節）；擴充的理由見 14.4 節。

**照這些決定，第 11 節的批次不必再改**：G7 照 (a) 寫；A1 照第 6.3 節的流程，素材放 14.2 節的位置。合併 #380 仍要等使用者說「合併」。

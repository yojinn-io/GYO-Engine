# 武器模型分檔的套用：事前宣告（`v8-arch-gyo2-weapon-model-apply`）

任務卡「武器模型分檔的套用」，2026-11-28。這一階段只做事前宣告，交使用者核准；核准前不寫實作程式。作者 Opus 5.5／high。

出處：HANDOFF `v8-arch-gyo2-asset-folder-structure`、`v8-arch-gyo2-weapon-model-u1-folders`、`v8-arch-gyo2-weapon-model-draw-hide-emptyhand`、`v8-arch-gyo2-weapon-model-apply-start`、`v8-arch-gyo2-object-pvp-route-a`；研究 `weapon-model-research.zh-Hant.md`（#380）第 1、3.4、4.2、5.2、6.3、11、14 節；批次表 `rebuild-batch-plan.zh-Hant.md` 第 90 行（W1／W2）；`arena-rules-split-declaration.zh-Hant.md`（#379）第 0.1 節與它的 A/B 腳本。

來源壓縮包：`~/Documents/3DModel/gyo_object_pvp-20261128T094842Z-1-001.zip`（69 個檔），根目錄 `gyo_object_pvp/` 對應 `assets/object_fps_pvp/`。逐檔讀到的結果（probe 的原始碼與輸出）放在主 checkout 的 `build/evidence/gyo2-weapon-model-apply/probe/`。

---

## 0. 前提與結論先講

### 0.1 用遊戲的例子

你在第一人稱拿著射釘槍：畫面上的手臂、手套和槍，現在是**同一個 FBX**（`Nailer1_FPS.fbx`），五個動作（待機、開槍、換彈、拔槍、收槍）也塞在裡面。使用者照研究卡的結構重新匯出後，變成：

- 手臂一個檔（`characters/player/arms/models/Arms.fbx`），只有骨架和手臂網格。
- 槍一個檔（`weapons/nailer_v2/viewmodel/models/Nailer_FPS.fbx`），只有骨架和槍的網格。
- 每個動作一個檔（`weapons/nailer_v2/viewmodel/animations/Nailer_FPS_Idle.fbx` 等五個），只有骨架和那一個片段。

遊戲每一幀照舊「取樣一次」：取樣的來源從「一體檔」換成「這個動作的片段檔」，再照骨頭名字把姿勢複製給槍和手臂（U1 (a)）。玩家看到的畫面要和現在一樣。

### 0.2 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| 第一層依對象分，對象裡分 `models/`、`animations/`、`textures/`；定義 JSON 放對象資料夾最上層 | `v8-arch-gyo2-asset-folder-structure` |
| U1 (a)：武器檔帶骨架與片段，手臂檔只帶骨架與網格；執行時取樣一次，手臂照骨名複製姿勢；Engine 不改 | `v8-arch-gyo2-weapon-model-u1-folders` |
| 資料夾擴充四條；只給一個對象用的放對象裡，第二個對象要用時才搬上層 | 同上；研究 14.4 節 |
| 拔槍 `Draw`、收槍 `Hide` 跟著武器；換槍＝舊 `Hide`＋新 `Draw`；空手片段放 `characters/player/arms/animations/` | `v8-arch-gyo2-weapon-model-draw-hide-emptyhand` |
| AssetId 是契約，整理結構只改 `path` | `docs/object_fps_pvp/assets.zh-Hant.md` 第 5 節 |
| 路線 (A)：核心 PvP 的權威結果逐位元一致，用權威 digest 的 A/B 證明 | `v8-arch-gyo2-object-pvp-route-a`；#379 第 0.1 節 |
| 這張卡＝W1 的資料夾部分＋W2（G7）的片段拆分與手臂對骨；W1 的「`CombatRules` 武器欄位搬進武器定義」不在這張卡 | `v8-arch-gyo2-weapon-model-apply-start` |

### 0.3 結論

**前提：這是作者能處理的**（遊戲的素材、資料與 Client 的載入程式；Engine 不改。要用的 `BindByName`、`SampleClip`、`ComputeModelSpace`、FBX 依路徑引用貼圖，Engine 都已經有）。

1. **Nailer 照新結構套用，畫面不變**：新的片段檔、槍檔、手臂檔的骨架（名字、父子、rest）和舊的一體檔完全相同。用「取樣片段檔一次 → 照名字複製給槍與手臂」算出的節點矩陣，和舊檔直接取樣的結果，五個片段 × 241 個時刻、137,370 個矩陣**逐位元相同**（第 2.2 節）。
2. **核心 PvP 的權威結果不變是預期**：Match 只讀 `Player_V20.fbx` 的骨架、網格高度和 hitbox。新舊兩版的 77 個節點（名字、父子、rest）、20 個網格、rest 高度完全相同，只有材質改了（第 2.1 節）。權威 digest 雜湊的是解析後的值，不是檔案位元組。實作後用 #379 的 A/B 腳本證明。
3. **`Player_V20` 一定要接上貼圖**：新版的 15 個材質底色變成白色，改成引用 `../textures/V08_*.png`。現在的 Client 只用材質底色畫角色（`ClientWorld.cpp:844`），不接上的話角色會全白。V08 的 15 張 4×4 色塊，每一張都正好是舊底色的 sRGB 值（第 2.1 節）。
4. **Patchwood AK 現在用不了**：它的片段套到 `Arms.fbx` 時，手臂變成約 34 公尺寬（Nailer 的片段是 0.37 公尺）。原因是 AK 的片段匯出時套用了 Armature 的縮放，`Arms.fbx` 沒有套用，違反研究第 6.3 節限制 3「手臂檔與武器檔要用同一副 Armature」（第 2.3 節）。決策點 A。
5. 要使用者決定的有 **A～E 五點**（第 3 節），另有兩件來源要確認（第 3.6 節）。

---

## 1. 成熟方案（源碼）

| 對照 | 看到的 | 位置 |
|---|---|---|
| **Bevy**（`~/Code/GitHome/bevy`） | 動畫的目標用「從根到骨頭的名字路徑」雜湊成 `AnimationTargetId`，同名字路徑的骨架都能播同一個片段（文件說這讓 retarget 變簡單） | `crates/bevy_animation/src/lib.rs:165-187`（說明與型別）、`:1344-1351`（`from_names`）；glTF loader 照路徑配 id：`crates/bevy_gltf/src/loader/mod.rs:631-634` |
| | glTF 的材質直接帶檔案裡的 base color 貼圖（變成 loader 的依賴）；`load_materials` 可以關 | `crates/bevy_gltf/src/loader/mod.rs:1360-1380`（`load_material` 取 `base_color_texture`）、`:254`、`:745-746`（`load_materials`） |
| | 事件掛在片段本身（`AnimationClip::add_event`），不掛在模型上 | `crates/bevy_animation/src/lib.rs:326` |
| | 第一人稱：手臂與槍是相機的子實體 | `examples/camera/first_person_view_model.rs`（現有程式已照它，`ClientWorld.cpp:957-958` 的註解） |
| **Godot**（`~/Code/GitHome/godot`） | 動畫軌用 `節點路徑:骨頭名` 找骨頭（`find_bone`） | `scene/animation/animation_mixer.cpp:812-817` |
| | `RetargetModifier3D`：照骨頭名字把來源骨架的姿勢複製給目標骨架 | `scene/3d/retarget_modifier_3d.h:36-60`、`.cpp:99-111` |
| **Unreal**（本機沒有源碼，**未確認**） | Copy Pose From Mesh／Leader Pose：一個骨架取樣，其他網格照骨名跟著動 | 研究 #380 第 2 節的記憶，未確認 |
| **GYO 現況** | 第三人稱已經用 `BindByName` 把動畫庫的片段搬到角色身上 | `games/object_fps_pvp/src/character/CharacterRig.cpp:176-189` |

**共同點**：片段和網格分開，照名字（Bevy、Godot 是名字路徑或「路徑＋骨名」）對到目標骨架；材質貼圖跟著模型檔走。

**和 Bevy 不同的一點**：Bevy 比的是名字路徑，GYO 的 `BindByName` 只比名字。為了得到同樣的保證，讀檔時多檢查一條：「對上的節點，它的父節點也要對到來源那一邊的父節點」，不合就報錯（第 4.3 節）。這只是讀檔時的資料檢查，不改 Engine。

---

## 2. 讀檔得到的現況

### 2.1 角色 `Player_V20.fbx`

| 項目 | 舊（repo） | 新（壓縮包 `characters/player/models/`） |
|---|---|---|
| 節點 | 77 | 77，名字、父子、rest **逐值相同** |
| 網格 | 20（9 個臉部＋11 個身體材質） | 20，名字、頂點數、面數、skin 相同 |
| rest 高度（`RestBounds`，`CharacterRig.cpp:108-134`） | −0.00147031224 .. 0.553222597 | **相同** |
| 材質 | 15 個 `V08 *`，底色是顏色，沒有貼圖 | 15 個，底色**白色**，貼圖 `../textures/V08_<名>.png` |

V08 的 15 張 PNG 都是 4×4、單一顏色（86～87 bytes），顏色＝舊底色編成 sRGB 的值（例：`V08 skin` 舊底色 linear (0.76, 0.385, 0.155) → `#e2a76e`，貼圖就是 `#e2a76e`；15 張全部相同）。也就是說，使用者把「材質顏色」換成了「色塊貼圖」。

**對權威的影響**：`CharacterDigest`（`games/object_fps_pvp/src/world/ContentHash.cpp:162-177`）雜湊的是 `bodyAssetId`、節點、片段、上半身遮罩、縮放、錨點、命中形狀。這些都不變：節點相同；片段是從 UAL 動畫庫搬到這副骨架，骨架相同所以結果相同；縮放和錨點由 rest 高度算出，高度相同。材質不進 digest。

### 2.2 Nailer

| 檔 | 節點 | 網格 | 片段 |
|---|---|---|---|
| 舊 `viewmodel/Nailer1_FPS.fbx` | 59 | 5（手臂 2＋槍 3） | 5 個，`Armature|Idle` 等 |
| 新 `viewmodel/models/Nailer_FPS.fbx` | 58（骨架 55＋網格節點 3） | 3（`main`、`mag`、`Side`，各綁 53 根骨） | 0 |
| 新 `characters/player/arms/models/Arms.fbx` | 56（骨架 55＋`Hand_Mesh`） | 2（`LowPoly_Skin`、`LowPoly_Glove`，綁 53 根骨） | 0 |
| 新 `viewmodel/animations/Nailer_FPS_{Idle,Shoot,Reload,Draw,Hide}.fbx` | 各 55 | 0 | 各 1 個，名字照舊（`Armature|Idle`…），長度照舊（1.000、0.333、3.733、0.833、0.367 s） |
| 新 `world/models/Nailer_TPS.fbx` | 2 | 1 | 0（節點與網格和舊 `Nailer2_TPS.fbx` 相同；只有貼圖路徑從 `.fbm/…jpg` 改成 `../textures/Nailer2_D.png`） |

- 三種檔共有的 55 個節點，名字、父子、rest **逐值相同**；片段檔之間也相同。
- `BindByName(片段檔, 槍檔)` 沒對上的只有 3 個網格節點，`BindByName(片段檔, 手臂檔)` 只有 `Hand_Mesh`。
- **姿勢比對**（`build/evidence/gyo2-weapon-model-apply/probe/PoseCompare.cpp`）：每個片段取 241 個時刻，比較「舊一體檔取樣」與「新片段檔取樣 → 照名字複製給槍與手臂 → `ComputeModelSpace`」的節點矩陣，共 137,370 個，**全部逐位元相同**。
- 材質：槍的 `Nailer1_Material` 現在引用 `../textures/Nailer1_D.png`（舊檔引用的是 `.jpg`，所以 Client 才用 JSON 表對應，`ClientWorld.cpp:2026` 的註解）。手臂的三個材質引用 `../textures/LowPoly_*.png`。三張 LowPoly 也是 4×4 色塊，等於舊底色，內容和 repo 現在的檔相同，只是搬到 `characters/player/arms/textures/`。

### 2.3 Patchwood AK（新武器）

| 檔 | 內容 |
|---|---|
| `viewmodel/models/Patchwood_AK_FPS.fbx`、`world/models/Patchwood_AK_TPS.fbx` | 兩個檔只差 FBX 標頭的匯出時間（offset 369 的 1 個位元組，`Millisecond` 欄），**內容相同**。53 個節點：`Patchwood AK` > `Fore-end`／`Magazine`／`Receiver`（> `ChargingHandle`）／`Stock` > 51 個網格。**沒有骨架、沒有 skin**：槍是物件的階層，零件是剛體網格 |
| `viewmodel/animations/Patchwood_AK_{Idle,Draw,Hide,Shoot,Reload}.fbx` | 各 61 個節點：手臂的骨架（和 Nailer 同名，含 `main_j`、`side_j`、`Mag_j`）＋槍的 6 個物件節點。片段名沒有 `Armature|` 前綴（`Idle`、`Draw`…）。Idle 4.958 s、Draw 0.792 s、Hide 0.792 s、Shoot 0.458 s、Reload 3.417 s |
| `viewmodel/animations/Patchwood_AK_Charge.fbx` | 7 個節點，**只有槍**（沒有手臂），0.533 s。從名字推測是拉機柄，什麼時候播**未確認** |
| `viewmodel/textures/`、`world/textures/` | 各 15 張，兩份逐檔 hash 相同。11 張是 768×384 的木頭、膠帶、金屬貼圖，4 張是 4×4 色塊 |

**對不上的地方**（`build/evidence/gyo2-weapon-model-apply/probe/ArmsBounds.cpp`）：

- AK 片段裡的 `Armature` 縮放是 1，手臂骨的局部座標是公尺級；`Arms.fbx` 和 Nailer 的 `Armature` 縮放是 0.01、骨頭是公分級。兩邊的手臂骨 rest 全部不同（54 根）。
- 照名字把 AK 的 Idle 複製給 `Arms.fbx` 再蒙皮：手臂的包圍盒約 **33.9 × 19.3 × 32.8 公尺**（Nailer 的 Idle 是 0.37 × 0.23 × 0.48 公尺）。蒙皮用的是 `Arms.fbx` 綁定時的逆矩陣（含 0.01 縮放），所以差了 100 倍。
- 槍的物件節點，在片段檔裡的 rest 和模型檔有微小差異（例 `Fore-end` 的平移 0.00248559983 對 0.00248560007），這是匯出時的浮點誤差，複製局部值不受影響（取樣的值來自片段）。
- 推論：AK 的片段是在「套用了 Armature 縮放」的那份 Blender 檔匯出的，手臂檔不是。要讓 AK 能用，要嘛重新匯出 AK 的片段（不套用縮放），要嘛手臂檔與 Nailer 都改成套用縮放後重新匯出（研究第 6.3、6.4 節建議的是後者：「`Armature` 縮放是 1、武器根骨的局部位置是公尺級」）。**這是美術端的工作**（研究第 11 節 A1），不是程式能補的。

### 2.4 程式現況

| 位置 | 現在 | 這張卡改成 |
|---|---|---|
| `ClientWorld.cpp:2021-2111`（`LoadViewmodel`） | 用路徑讀 `nailer_v2_viewmodel.json`、`Nailer1_FPS.fbx`、`viewmodel.animset.json`、`viewmodel.anim_events.json`、`viewmodel.event_sounds.json`、`../world/character.json`（`:2023`、`:2027`、`:2036`、`:2078`、`:2092`、`:2101`）；片段只從一個模型找，忽略 animset 每一列的 `model_asset_id` | 全部經 catalog 用 AssetId 讀（第 4.3 節） |
| `ClientWorld.cpp:2156` | `LoadViewmodel(fs, root + "/weapons/nailer_v2/viewmodel/")` | 傳武器定義的 AssetId |
| `ClientWorld.cpp:508` | `requests->Load(A::AssetId("object_fps_pvp.model.nailer_v2"))` 寫死 | 用定義裡的 `model_asset_id` |
| `ClientWorld.cpp:876-937`（`PoseViewmodel`） | 一次取樣，同一份姿勢給全部網格 | 一次取樣，照名字複製給槍與手臂，各自 `ComputeModelSpace` |
| `ClientWorld.cpp:940-965`（`AddViewmodel`）、`:825-850`（`AddCharacters`） | 材質用 JSON 表（viewmodel）或底色（角色）；第三人稱的槍只取一張圖（`heldImage`，`:2104-2108` 最後一個覆寫贏） | 決策點 E |
| `CharacterRig.cpp` | 經 catalog 讀 `Player_V20.fbx`（路徑來自 catalog） | **不改**；catalog 的 `path` 改了它就跟著讀新位置 |

JSON 裡**沒有程式讀的欄位**：`nailer_v2_viewmodel.json` 的 `muzzle`、`sampler`；`world/character.json` 的 `base_color_linear`、`sampler`；`viewmodel.anim_events.json` 的 `model_asset_id`（`git grep` 在 `games/`、`tests/` 只有 JSON）。

---

## 3. 決策點

### 3.1 A：Patchwood AK 這次怎麼處理

| 選項 | 內容 | 代價 |
|---|---|---|
| **(A2) 推薦：這次不進 repo** | 壓縮包裡的 AK 原樣放進主 checkout 的 `build/art-source/object_fps_pvp/weapons/patchwood_ak/<交付版本>/export/`（git 不追蹤，`assets.zh-Hant.md` 第 8 節），等重新匯出、而且有地方拿它（W1 武器資料化、W3 換槍）時一起進 | AK 暫時不在版本控制裡 |
| (A1) 進 repo＋catalog，不進遊戲 | 搬進 `weapons/patchwood_ak/`，列 catalog，加一個「讀得到、槍的片段對得上」的測試 | 違反 `assets.zh-Hant.md` 第 7 節「出貨的素材都要被產品用到」，要在第 9 節加例外；手臂對不上，測試只能測一半；重新匯出後約 10 MB 的檔整批換掉，git 歷史留兩份 |
| (A3) 能在遊戲裡拿 | 要先重新匯出（第 2.3 節），再加 viewmodel 定義（擺放、FOV、錨點）、聲音、規則數值、選武器的方法 | 擺放數值要對著畫面調，規則數值和換槍是 W1、W3 的內容；這張卡做不完 |

**推薦 (A2)** 的理由：(1) 手臂的縮放對不上，現在進來是一個已知會壞的素材；(2) 照「空殼刪掉」與第 7 節，沒有人用的素材不出貨；(3) 重新匯出後檔案內容會換掉。

不論選哪個，都要請使用者處理：

- AK 片段的 Armature 縮放：重新匯出時和 `Arms.fbx` 一致（兩種做法見第 2.3 節最後一條）。
- `Charge` 什麼時候播（只有槍的 7 個節點、0.533 s）。這會影響播放方式：疊在待機上播，還是換彈的一部分。

### 3.2 B：Patchwood viewmodel／world 的重複貼圖怎麼放

前提：兩份貼圖逐檔相同；兩個 FBX 的內容也相同（第 2.3 節）。它們是**同一把武器的兩個視角**。

| 選項 | 內容 |
|---|---|
| **(B1) 推薦：一份，放 `weapons/patchwood_ak/textures/`** | 規則「第二個對象要用時才搬上層」裡的「上層」，對同一把武器的兩個視角來說是武器資料夾本身；`textures/<group>/` 留給不同對象（例：兩把武器）共用。放置規則第 2 條補一句「同一把武器的兩個視角共用的貼圖，放 `weapons/<weapon>/textures/`」 |
| (B2) 兩份照交付 | 不用改規則；git 多一份約 3.6 MB，catalog 兩個 id 是同一張圖 |
| (B3) 一份，放 `textures/patchwood_ak/` | 照第 2 條字面；一把武器的檔散到兩處 |

和決策點 E 的關係：E 選 (E2)（FBX 依路徑引用貼圖）時，FBX 裡寫的是 `../textures/X.png`（相對 `models/`），搬到武器資料夾要讓 Blender 匯出時的貼圖路徑指到 `../../textures/X.png`。A 選 (A2) 時，這件事在重新匯出時一起做，這張卡不碰檔案。

另外提醒（不是決策點）：研究第 4.2 節建議第三人稱用低模、原點在右手握把、帶 `muzzle`／`grip_l` 的 Empty。現在的 TPS 和 FPS 是同一個模型，重新匯出時可以一起考慮。

### 3.3 C：片段檔的命名與 animset 的對應

| 項目 | **推薦 (C1)** | 比過的 |
|---|---|---|
| 檔名 | 照交付：`<Weapon>_FPS_<Role>.fbx`（`Nailer_FPS_Idle.fbx`），`Role` 和 animset 的鍵一致（`Idle`、`Shoot`、`Reload`、`Draw`、`Hide`） | (C2) 一把武器一個 `<Weapon>_FPS_Anims.fbx` 放全部片段（研究 5.2 節的草圖）：要使用者重新匯出，沒有好處 |
| catalog | 一個片段檔一列，type `model`，`settings {"meshes": false, "textures": false}`（同 UAL 動畫庫），id `object_fps_pvp.model.animation.nailer_v2.viewmodel.<role 小寫>` | — |
| animset | 格式不變：每個 role `{model_asset_id: 那個片段檔, clip: 檔裡的片段名}`（`Armature|Idle`）。現有格式本來就是一列一個模型（玩家的 animset 也是），只是現在每列指不同的檔 | — |
| anim_events | **version 2**：`clips` 改成陣列，每一項 `{model_asset_id, clip, events}`，和 animset 的寫法相同；拿掉頂層的 `model_asset_id` | (C3) 格式不變，頂層的 `model_asset_id` 改指 Reload 的片段檔：一個檔只能描述一個片段檔的事件，下一個有事件的片段（例 Shoot 的拋殼）就放不下。(C4) 用 role 當鍵：事件屬於片段，不屬於 role（同一個片段換了 role，事件時間還是那個片段的）；Bevy 也把事件掛在片段上（`lib.rs:326`） |

`gyo.anim_events` 只有這個遊戲讀（`git grep`：`ClientWorld.cpp:2078` 一處），改版不影響別的程式。

### 3.4 D：V08 的 86 bytes 貼圖

**是單色色塊**：15 張都是 4×4、單一顏色，等於舊的材質底色（第 2.1 節）。

| 選項 | 內容 |
|---|---|
| **(D1) 推薦：進 catalog，照交付放 `characters/player/textures/`** | id `object_fps_pvp.texture.player.v08_<名>`（例 `v08_skin`、`v08_eye_white`）。新版 FBX 的底色是白色，不接上貼圖角色會全白 |
| (D2) 不進 catalog，顏色寫進 JSON | FBX 說白色、JSON 說別的顏色，同一件事寫兩處 |
| (D3) 請使用者改回材質顏色、重新匯出 | 使用者改成貼圖可能有原因（之後換成真的貼圖），不推薦 |

### 3.5 E：貼圖怎麼接到材質上（新增的決策點）

起因：新的 FBX 全部改成引用正確的 PNG 路徑（`../textures/X.png`）。Client 現在不用檔案裡的貼圖（`FbxSettings::textures = false`），改用 JSON 表對應；原因寫在 `ClientWorld.cpp:2026`：「FBX 寫的是 JPG，catalog 裡是 PNG」。這個原因已經不存在了。

| 選項 | 內容 | 改的資料 |
|---|---|---|
| (E1) JSON 表（照研究 14.2 節的樹） | 角色在 `character.json` 加 `material_overrides`（15 項，和武器 `world/character.json` 同格式）；手臂另開 `arms.json`（3 項材質表）；viewmodel 的 `materials` 照舊 | 多 18 項表格；FBX 已經寫了的對應，JSON 再寫一次 |
| **(E2) 推薦：用 Engine 已有的「依路徑引用」** | catalog 的模型列不再關 `textures`；`FbxLoader` 照 FBX 的相對路徑宣告貼圖引用，asset store 對到 catalog 的列（`engine/modules/model/src/FbxLoader.cpp:83-91`、`AssetRequests::Load(AssetRef)`）；Client 給每個網格的圖＝它材質的引用，沒有貼圖的材質照舊用底色。拿掉 viewmodel 的 `materials`、`world/character.json` 的 `material_overrides`（連同沒人讀的 `sampler`、`base_color_linear`）。手臂不需要 `arms.json`，viewmodel 定義只加一個 `arms_model_asset_id` | 少兩張表；檔案是唯一的來源 |

**推薦 (E2)** 的理由：

1. 繞開機制的原因已經消失，「用機制，不繞過機制」。
2. Bevy 的 glTF loader 預設也是材質帶檔案裡的貼圖（`bevy_gltf/src/loader/mod.rs:1360-1380`）；GYO 的 `FbxSettings::textures` 本來就照 Bevy 的 `load_materials` 設計（`FbxLoader.hpp:55-61` 的註解）。
3. 「該不該存在」：JSON 表重複了 FBX 已有的資訊，兩邊要一起改。

**代價**：

- 這會推翻研究 14.2 節樹裡的 `arms.json 手套的材質表`。研究 3.3 節第 4 條說換手套的顏色「換貼圖」用材質表做；改成 (E2) 之後，換手套是換 PNG，或者等換裝的 Feature 出來時再加覆寫表。
- Client 畫角色、手臂、槍的路徑會改，畫面要確認。這是畫面上的事，不進權威。
- Match 讀角色時照舊關掉貼圖（`CharacterRig.cpp:84`），不受影響。

### 3.6 要確認的來源（`LICENSES/SOURCES.zh-Hant.md`）

現有紀錄：「除了骨骼動畫，都是我自己做的」（`v8-arch-gyo2-pvp-assets-license`）；`Nailer1_FPS.fbx` 記成「使用者本人製作」。

1. **第一人稱的手臂與五個片段**（`Arms.fbx`、`Nailer_FPS_*.fbx`）：也是使用者自己做的嗎？骨架的根叫 `Mark23_Root`（研究 3.3 節說是舊武器 Mark23 留下的名字），所以想確認手臂的骨架與動畫是不是來自某個素材包。
2. **Patchwood AK**（模型、片段、貼圖）：來源與授權。A 選 (A2) 時這張卡不寫進 `SOURCES`，但 art-source 的 `README.md` 會記。

---

## 4. 做法（照推薦 A2、B1、C1、D1、E2 寫；核准的選項不同時照它改）

### 4.1 檔案

| 動作 | 檔 |
|---|---|
| 搬（內容換成新版） | `characters/player/Player_V20.fbx` → `characters/player/models/Player_V20.fbx`；`weapons/nailer_v2/viewmodel/Nailer1_FPS.fbx` → `weapons/nailer_v2/viewmodel/models/Nailer_FPS.fbx`；`weapons/nailer_v2/world/Nailer2_TPS.fbx` → `weapons/nailer_v2/world/models/Nailer_TPS.fbx` |
| 搬（內容不變） | `weapons/nailer_v2/viewmodel/textures/LowPoly_{Glove,Skin,Skin_Shadow}.png` → `characters/player/arms/textures/` |
| 新增 | `characters/player/arms/models/Arms.fbx`；`weapons/nailer_v2/viewmodel/animations/Nailer_FPS_{Idle,Shoot,Reload,Draw,Hide}.fbx`；`characters/player/textures/V08_*.png`（15 張） |
| 不動 | `animations/ual_mannequin/UAL1_Standard.fbx`、`Nailer1_D.png`、`Nailer2_D.png`（內容與位置都相同） |
| 不建 | `characters/player/animations/`（壓縮包裡是空資料夾；「有才建」，git 也不收空資料夾） |
| repo 外 | Patchwood AK 全部 → `build/art-source/object_fps_pvp/weapons/patchwood_ak/version01/export/`，並在該處的 `README.md` 記交付日期與第 2.3 節的問題 |

### 4.2 catalog 與 JSON

**AssetId**：

- 不改：`model.character.player_v20`、`model.nailer_v2`、`model.weapon.nailer_v2.world`、`texture.nailer_v2.gun`、`texture.weapon.nailer_v2.world`，以及全部 text 列。只改 `path`，拿掉模型列的 `"textures": false`（E2）。
- **改名**（卡片自己判的）：三張手臂貼圖 `texture.nailer_v2.{skin,glove,skin_shadow}` → `texture.player.arms.{skin,glove,skin_shadow}`。第 5 節「整理結構只改 path」說的是同一個對象搬位置；這三張現在屬於另一個對象（手臂），id 裡留著 `nailer_v2` 會讓人以為是槍的。引用它們的只有 `nailer_v2_viewmodel.json` 的 `materials`，E2 之後那張表也拿掉了。
- **新增**：`model.player.arms`；`texture.player.v08_<名>` × 15；`model.animation.nailer_v2.viewmodel.{idle,shoot,reload,draw,hide}`。
- id 換版本的規則（第 5 節「換版本時新增 id」）：使用者保留了 `Player_V20`、`nailer_v2` 的版本名，這次是重新匯出，不是新設計，所以 id 不換版本。

**JSON**：

| 檔 | 改 |
|---|---|
| `nailer_v2_viewmodel.json` | 加 `arms_model_asset_id: "object_fps_pvp.model.player.arms"`（選填，沒寫＝手臂在武器的模型裡）；拿掉 `materials`、`sampler`；`muzzle` 留著（研究 5.2 節：等第一次用到槍口時再改成只寫節點名，這張卡不碰） |
| `viewmodel.animset.json` | 五列的 `model_asset_id` 各指自己的片段檔 |
| `viewmodel.anim_events.json` | version 2（C1） |
| `world/character.json` | 拿掉 `material_overrides`（E2） |
| `characters/player/character.json` | 不改（E2 不需要表）。#379 剛改過它，這張卡不碰，也就不會撞到 |
| `characters/player/presentation.json` | 不改（第三人稱的掛點 `mount` 搬家是 W1 的另一件事） |

### 4.3 程式（只改 `games/object_fps_pvp/src/client/ClientWorld.cpp` 與它的標頭，Engine 不改）

1. **`LoadViewmodel(fs, root, definitionId)`**：用 Engine 的 `ParseCatalog`（`engine/asset/Catalog.hpp:101`）讀 catalog，從定義的 AssetId 開始，一路照 JSON 裡的 id 找檔。槍的模型、手臂模型、每個片段檔各讀一次（同一個檔只讀一次）。武器定義的 id 暫時是一個常數 `object_fps_pvp.weapon.nailer_v2`，等 W1 的 `weapon.json` 或模式配置來給。第三人稱的槍從 `presentation.json` 的 `weapon.character_asset_id` 找，不再用 `../world/` 的相對路徑。
2. **對骨**：每個片段檔對槍、對手臂各做一次 `BindByName`，讀檔時檢查兩件事：
   - 目標的每一根蒙皮骨（skin 的 joint）都要對上；
   - 對上的節點，它的父節點也要對到來源那一邊的父節點（第 1 節，和 Bevy 的名字路徑同等的保證）。
   
   不合就報錯，錯誤訊息寫檔名和節點名。這是載入失敗（`LoadPresentation` 回傳錯誤），不是 assert。
3. **錨點**：`anchor_node` 在 Idle 片段第 0 幀、片段檔自己的節點清單裡的位置（現在是在一體檔裡取，值相同：第 2.2 節）。
4. **`PoseViewmodel`**：片段檔取樣一次 → 對槍、對手臂各自「rest，再把對上的節點換成片段的值」→ 各自 `ComputeModelSpace` → 寫進各自網格實體的 `SkinPose`。`ViewmodelPart` 記它屬於槍還是手臂。片段的長度從片段檔取（`ViewmodelClip` 現在讀 `viewmodel.model->clips`）。
5. **`AddViewmodel`、`AddCharacters`**（E2）：網格的圖＝`AssetRequests::Load(材質的 baseColorImage)`；沒有貼圖的材質照舊用底色（`MaterialColor`）。拿掉 `heldImage`、`materialImages`。進度條把這些圖的 Handle 也算進去。
6. **`RequestAssets`**：viewmodel 的模型 id 從定義來，不再寫死；多要手臂的模型。

### 4.4 文件

- `docs/object_fps_pvp/assets.zh-Hant.md`：
  - 第 2 節：樹換成研究 14.2 節的形狀，含 `characters/player/arms/{models,animations（有才建）,textures}/`。照 E2，不畫 `arms.json`。
  - 第 3 節：「3D 模型｜對象資料夾」→「`<對象>/models/`」，加「片段｜`<對象>/animations/`，多個對象共用的放 `animations/<library>/`」。
  - 放置規則：寫進資料夾擴充四條（研究 14.4 節第 1～4 條）、拔槍收槍跟武器、換槍＝舊 `Hide`＋新 `Draw`、空手片段放手臂資料夾（第 5 條），以及 B1 的一句。
  - 第 6 節提到的守衛 `test_asset_catalog.py` 在 gyo2 上沒有：這張卡不整理那一段（不在範圍），只在 dev_log 記一筆。
- `LICENSES/SOURCES.zh-Hant.md`：檔名與路徑照新位置；`Arms.fbx`、五個片段檔、V08 貼圖的來源照第 3.6 節使用者的回答寫。
- `docs/architecture/target/v2/README.md` 加這份宣告的連結；dev_log `docs/dev_logs/2026_11_28_gyo2_weapon_model_apply.zh-Hant.md`。
- HANDOFF 與批次表不改：使用者的決定由主會話記。

---

## 5. 測試（外部確認）

| 測試 | 確認的事 |
|---|---|
| 讀 viewmodel 成功 | `LoadPresentation` 讀出槍、手臂、五個片段；每個片段對槍、對手臂的蒙皮骨全部對上 |
| 手臂跟著武器的片段動 | 五個片段、多個時刻：槍與手臂共有的骨頭，兩邊算出的模型空間矩陣逐位元相同 |
| 少一根骨就報錯 | 用造出來的 `ModelData`（不用 FBX）：目標的一根蒙皮骨在片段裡沒有 → 錯誤訊息有那根骨的名字；父節點對不上 → 報錯 |
| 片段播得出來 | 每個片段取樣後，槍的根骨（`main_j`）在開始與中間的位置不同（現有的「待機姿勢在 viewmodel 相機前」測試照舊要過，`tests/object_fps_pvp/client/ClientTests.cpp:1283`） |
| 貼圖都對得到（E2） | 角色、手臂、兩把槍的每個材質：FBX 裡的相對路徑解析後都是 catalog 裡的一列，檔案存在 |
| 現有測試 | `object_fps_pvp` 的 client、world、logic 測試與 core script 全部照舊通過 |

**突變測試**：只跑和改動相關的——新加的兩條讀檔檢查（蒙皮骨、父節點）、`PoseViewmodel` 的複製、E2 的圖。

---

## 6. 逐位元一致（A/B）

- 工具：#379 的 `build/evidence/gyo2-arena-rules-split/ab.py`（複製到這張卡的證據資料夾）。base＝`origin/gyo2` 的 `9f7201da`，head＝這張卡的建置；`gyo_object_fps_pvp_core_script --report --check` 的每一幀權威 digest 逐行比較，`report` 行（網路登記與 decision dataset，含 `characterDigest`）印差異。
- 預期：**digest 全部相同，`report` 也沒有差異**（第 2.1 節：角色的骨架、高度、hitbox 都沒變；Client 的改動不進權威）。
- 不同時：停下來回報第一個不同的行，不改測試、不調整資料去湊。

---

## 7. 不做的

- W1 的武器規則數值搬家（`CombatRules` → `weapon.json`）、`weapon.json`、第三人稱掛點 `mount` 搬家：W1 另一批。
- 換槍（`Hide` 接 `Draw`）、空手片段：沒有素材，也沒有背包（W3）。
- `muzzle` 的改寫、第三人稱的 `grip_l`（G3）。
- Patchwood AK 進遊戲（A2）。

## 8. 分批、模型與檔位

上段（改資料格式：catalog、animset、anim_events v2、viewmodel 定義；讀檔程式）。照研究第 11 節 G7 的指派：

| 步驟 | 內容 | 模型／檔位 |
|---|---|---|
| 1 | 搬檔、catalog、JSON | 實作 Opus／high（這張卡） |
| 2 | 程式（第 4.3 節）＋測試（第 5 節） | 實作 Opus／high |
| 3 | 文件、SOURCES、dev_log | 實作 Opus／high |
| 4 | A/B（第 6 節）、相關突變 | 檢證者 Opus／high |
| 5 | 設計審查；純粹的 Code Review | 審查 Opus／high；純粹的 Code Review Fable／high |
| 6 | draft PR（目標 `gyo2`），手動 dispatch L1 | — |

一個 PR、一條功能線。CPU 建置與 core script 在本機跑；沒有重 GPU 或大記憶體的量測。要在本機看畫面確認（E2 的貼圖）時，先問使用者。

## 9. 未確認

- 新的 Nailer 片段的 key 是否和舊檔逐值相同：沒有直接比 key，比的是取樣結果（137,370 個矩陣逐位元相同，第 2.2 節）。
- E2 之後畫面的顏色：4×4 色塊貼圖經過取樣，和直接用 `Color8` 底色畫，在 GPU 上是否逐像素相同沒有確認。兩條路的 sRGB 解碼點可能不同，顏色值本身相同。
- AK 的 `Charge` 的用途、TPS 是否另做低模：要問使用者（第 3.1、3.2 節）。
- Blender 的匯出設定（是否套用縮放）只從結果推論，沒有看 .blend 檔。

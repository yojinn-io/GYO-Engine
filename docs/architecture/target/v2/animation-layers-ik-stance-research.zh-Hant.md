# 骨骼動畫的三層、IK、姿勢組：成熟方案比較（`v8-arch-anim-layers-research`）

更新：2026-11-27（加第 10 節：手臂的 hitbox；同日照使用者「最終產品做肢體傷害」的決定，重看 R3、R4、R5、D5、D8、D9 與第 8 節，改了什麼見第 11 節）。Owner：研究卡（由 GYO2 重建主會話開出）。**狀態：研究，不實作。D1～D8 使用者已定（第 12 節）；D9、D10 待定。這份文件不改任何已定的決定。**

查的版本：`gyo2` 4e370a25。本地源碼（引用附檔名與行號，路徑從各 repo 的根算起）：Bevy 6a911653f（0.20.0-dev）、Godot 94fc09c、lightyear 736e1695、bevy_replicon d80c849、Unity Netcode for Entities 7.2.0（`~/Code/GitHome/com.unity.netcode`）。Unreal、Unity 引擎本體不在本機，只讀官方文件網頁（附 URL），沒有下載或 clone；少數只在社群頁面找到的，標「社群」。沒能在原文確認的寫「未確認」。

讀源碼與網頁的偵察由四個子 agent 做（Opus／high：Godot、Bevy＋網路、Unreal 文件、Unity 文件），本卡抽查了引用的行號（Godot 一處行號偏了，已照源碼改正：`skeleton_3d.cpp:452`）。

## 0. 問題、已定的前提、判斷依據

### 0.1 問題（用遊戲的例子）

PvP 之後要有多把武器（W1 武器資料化、W2 第二把、W3 背包與切換、W4 撿起與丟掉）。按 2 從步槍切到手槍時：

1. 第一人稱：viewmodel 播步槍的 Hide，接著播手槍的 Draw。
2. 第三人稱：上半身換一組持槍姿勢（手槍的 aim、shoot、reload）。
3. 第三人稱：左手用 IK 對到護木上（手槍可能是雙手握把）。
4. Match：命中判定用骨架姿勢擺 hitbox（`games/object_fps_pvp/src/world/PvpWorld.cpp:513` 的 `PlaceHitShapes`），所以姿勢會影響判定。

要回答的是：這些東西在成熟引擎裡各放在哪一層，GYO2 還缺什麼，缺的放 Engine 還是遊戲。

### 0.2 已定的前提（這份研究不推翻它們）

| 前提 | 誰定的、出處 | 掛在它上面的 |
|---|---|---|
| **三層的分法已定**：第 1 層（骨架、片段、時刻 t 的姿勢、混合、retarget，都是純函式）放 model 函式庫；第 2 層（每個實體的播放器 `AnimationModule`）**先不做**，PvP 由遊戲照 tick 推片段時刻；第 3 層蒙皮放 render。hitbox、切片段、分層、additive、瞄準、附掛是遊戲規則。重開的條件：「等有產品要 Bevy `AnimationPlayer` 那種用法再加」 | 使用者 2026-11-23（「這個照你建議就好。」），HANDOFF `v8-arch-gyo2-skeletal-animation-layers`（`docs/architecture/target/v2/HANDOFF.md:412`） | model #313、#317；`PlayerAnimation`；render 的 `SkinPose` |
| model 不依賴 render／GPU／SDL，Match 可以用 | 使用者 2026-11-23，HANDOFF `v8-arch-gyo2-model-now`（`HANDOFF.md:335`） | Match 擺 hitbox |
| 取樣結果跨平台不保證逐位元相同；只用在「每台機器自己判」的地方 | model 頁 4.5 節（`model.zh-Hant.md`）；PvP 盤點 PV6（`games/object_fps_pvp/docs/porting-inventory.zh-Hant.md:114`） | 第 5 節 |
| 命中只在 Match 算；命中回溯（PV3）這一輪不做，`ViewTime` 已經在 wire 上 | PvP 移植宣告 PV3（`games/object_fps_pvp/docs/port-declaration.zh-Hant.md:50-54`） | 第 5 節 |
| ECS 相關的機制以 Bevy 為主參考，偏離要寫原因 | 使用者 2026-11-23（批次表〈每批的設計審查〉第 5 項） | 全文 |
| **最終產品做肢體傷害，採 (3)**：手臂是獨立部位，有自己的倍率和血量（2026-11-27 加入本表） | 使用者 2026-11-27（「照建議。之前本來也是要的。」），HANDOFF `v8-arch-gyo2-limb-damage`（`gyo2` 275dcfd0 的 `HANDOFF.md:418`） | 第 10、11 節；R3、R4、R5；D5、D8、D9、D10 |
| **路線 (A)**：最終產品是塔科夫型撤離射擊＋怪物；所有功能直接加在 Object_PvP，各系統由 arena／模式資料開關（2026-11-27 加入本表） | 使用者 2026-11-27，HANDOFF `v8-arch-gyo2-object-pvp-route-a`（`HANDOFF.md:417`） | 第 11 節 |

**記錄沒同步**：批次表 `rebuild-batch-plan.zh-Hant.md` 第 112、113 列（model 的時機、骨骼動畫的三層）還寫「待使用者定（未決）」，但兩件都已在 11-23 定了（上表）。這張卡只改本文件，所以沒改那兩列，列在第 9 節交給主會話（主會話已在 `gyo2` 18d35c90 改正）。

所以這份研究真正要問的是：**W3 是不是觸發了「有產品要 `AnimationPlayer` 那種用法」的條件？如果沒有，W3 還缺哪些第 1 層的機制？**

### 0.3 判斷依據

- AGENTS.md §1-1：Engine 只提供機制，替換得了的規則由作者選。
- §1-2：Kernel 的職責是 ECS。
- §1-6：產品的詞（步槍、手槍、護木）不進 Engine。
- §1-10：預設方便，不綁作者。
- 每一項先寫前提：「這是 Engine 要解決的（只有 Engine 做得到），還是作者能處理的」，再判斷放哪一層。

## 1. 三層：各家把什麼放在哪

### 1.1 Bevy（`crates/bevy_animation`，主參考）

這個 checkout 已把 `AnimationGraph` 改名為 `BlendGraph`（commit 4dacf9d6a，#26367），下文用新名字。

- **第 1 層（資料與取樣）**：
  - `AnimationClip { curves, events, duration }` 是 asset（`src/lib.rs:105-111`）；曲線依 `AnimationTargetId` 分組（`lib.rs:161`）。`AnimationTargetId` 是骨頭名字路徑的 hash（`lib.rs:1356-1370`），所以「骨頭名字對得上就能播」（`lib.rs:174-175`）。
  - 單條曲線的取樣是純函式（`AnimationCurve::sample_clamped`，`src/animation_curves.rs:393`）。
  - **整副姿勢的求值綁在 World 上**：圖的走訪、混合、寫回都在 `animate_targets` 這個 system 裡，最後把結果覆寫到骨頭 entity 的元件上（`lib.rs:1096-1300`，commit 在 `lib.rs:1296`）。沒有找到「把時刻 t 的整副姿勢算進一個 buffer」的公開 API。
  - 骨頭是 entity：`SkinnedMesh { inverse_bindposes, joints: Vec<Entity> }`（`crates/bevy_mesh/src/skinning.rs:16-21`）。
- **第 2 層（播放）全在 Engine**：
  - `AnimationPlayer { active_animations: HashMap<BlendNodeIndex, ActiveAnimation> }` 是元件（`lib.rs:748`）；`ActiveAnimation` 有 weight、repeat、speed、elapsed、seek_time、last_seek_time、paused 等欄位（`lib.rs:522-543`）；`update(delta, duration)` 用 `seek_time += delta * speed` 往前推（`lib.rs:576-610`）。
  - `BlendGraph` 是 asset（`src/graph.rs:115-132`），節點只有 `Clip`、`Blend`（子節點權重正規化）、`Add` 三種（`graph.rs:214-240`）；`BlendGraphHandle` 是元件。
  - **遮罩**：`AnimationMask = u64`，一張圖最多 64 組（`graph.rs:486-489`）；`add_target_to_mask_group` 把骨頭加進第 N 組（`graph.rs:731`）；節點的 mask 第 N 位是 1，代表這個節點與它的子樹不動第 N 組（`graph.rs:180-185`；取樣時跳過在 `lib.rs:1221-1227`）。上下半身就是「上半身的骨頭放進 U 組，locomotion 的節點遮掉 U」。範例 `examples/animation/animation_masks.rs` 用 6 組。
  - **轉場**：`AnimationTransitions`（`src/transition.rs:33`）在 Engine 裡；`play(player, new, duration)` 把舊的主動畫以每秒 1/duration 的速度淡出（`transition.rs:78-100`，`:126`）。
  - **排程**：`advance_transitions`、`advance_animations`、`animate_targets` 等串在 `PostUpdate`、`AnimationSystems` 集合、在 `TransformSystems::Propagate` 之前（`lib.rs:1316-1335`）；時間用 `Res<Time>` 的 `delta_secs()`（`lib.rs:1045-1051`），在 `PostUpdate` 裡就是虛擬時鐘。排程是寫死的；要搬到 `FixedUpdate`，有幾個 system 不是 `pub`（`lib.rs:1003`、`graph.rs:913`），得複製外掛（實際可不可行，未確認）。
- **第 3 層（遊戲的動畫狀態）**：沒有狀態機。範例用遊戲的 system 決定播哪個、何時 `AnimationTransitions::play`（`examples/animation/animated_mesh_control.rs:149-156,213`）。
- **事件**：在 Engine。`TriggeredEvents::from_animation` 用 `last_seek_time` 到 `seek_time` 的區間判斷穿過了哪些事件，包含倒播與跨 loop（`lib.rs:1448-1527`）；根事件在 `lib.rs:1003-1042` 觸發。
- **決定性、網路**：`bevy_animation` 裡沒有。播放頭是浮點 delta 的累積，不是 tick 的函式（`lib.rs:586-587`）。

### 1.2 Godot（`scene/animation`、`scene/3d`）

- **第 1 層**：`Animation` 是 Resource（`scene/resources/animation.h:40`），取樣函式是 `const` 的，例：`try_position_track_interpolate(track, time, *out, backward) const`（`animation.cpp:1183`）。`AnimationLibrary` 只是「名字 → `Animation`」的表（`animation_library.h:48`）。
- **第 2 層全在 Engine**，拆成兩件：
  - 混合核心 `AnimationMixer`：每幀從一串 `AnimationInstance{Animation, PlaybackInfo}`（`animation_mixer.h:84-99`）重算，`_process_animation` 依序 init → 算總權重 → 取樣累加 → 寫進 `Skeleton3D`（`animation_mixer.cpp:1026-1040`）。
  - 播放器：`AnimationPlayer` 的播放頭在 `PlaybackData.pos`（`animation_player.h:69`）；`AnimationTree`＋`AnimationNode*`（BlendTree、StateMachine、BlendSpace1D／2D、OneShot、Transition、Add2、Blend2、TimeScale 等）。
  - **圖和狀態分開放**：`AnimationNode` 是共用、無狀態的 Resource（`animation_tree.h:66`）；每棵樹的狀態（播放頭、淡入剩餘、狀態機的目前狀態）放在樹自己的 `property_map`（`animation_tree.h:483`），節點實例的註解明說它不能存持久資料（`animation_tree.h:276-277`）。這和 GYO2「資料是純的、狀態另放」的分法最接近。
  - **遮罩**：沒有獨立的遮罩資產；每個節點有 `filter`（骨頭路徑集合）與 `filter_enabled`（`animation_tree.h:85-86`），動作有 PASS／STOP／BLEND（`animation_tree.cpp:174-249`）。例：Blend2 對輸入 0 用 BLEND、輸入 1 用 PASS（`animation_blend_tree.cpp:946-948`）。
  - **轉場**：狀態機的 transition 有 `xfade_time`、switch mode（Immediate／Sync／AtEnd）、advance condition／expression（`animation_node_state_machine.h:37-63`）；播放狀態在 `AnimationNodeStateMachinePlayback`，用 A* 找路徑（`animation_node_state_machine.h:234-290`）。
  - **時間**：callback mode 有 Physics／Idle／Manual（`animation_mixer.h:55-57`）；Manual 時由呼叫者 `advance(dt)`（`animation_mixer.cpp:2123-2125`）。
- **第 3 層**：遊戲腳本寫 `parameters/...`、呼叫 `travel()`。

### 1.3 Unity（官方文件）

URL 前綴 `E` = `https://docs.unity.com/en-us/engine/current/`（Unity 6000.6；舊的 `docs.unity3d.com/Manual` 會轉到這裡）。

- **第 1 層**：`AnimationClip`、Avatar（Humanoid／Generic）、Avatar Mask，都在 Engine。
- **第 2 層全在 Engine**，有兩條路：
  - **Animator Controller**（Mecanim）：狀態機、transition（Exit Time、Transition Duration、Interruption Source、Conditions，`E`manual/animation-section/animation-mecanim/animation-animator-controller/animation-state-machines/class-transition）、Blend Tree、Layers。Layer 可以是 Override 或 Additive，各自配一個 Avatar Mask；Sync layer 沿用另一層的狀態與轉場、只換片段（`E`manual/.../animation-state-machines/animation-layers）。update mode 有 Normal／Fixed／UnscaledTime（`E`script-reference/unityengine/animatorupdatemode），也能手動 `Animator.Update(dt)`（`E`script-reference/unityengine/animator/update）。
  - **Playables API**：`PlayableGraph`、`AnimationMixerPlayable`、`AnimationLayerMixerPlayable`（有 `SetLayerMaskFromAvatarMask`、`SetLayerAdditive`，`E`script-reference/unityengine/animations/animationlayermixerplayable）、`AnimationScriptPlayable`＋`IAnimationJob`（自訂的姿勢運算）。Engine 給圖與混合器；狀態機、轉場、播放隊列要使用者自己組（`E`manual/animation-section/animation-mecanim/playables/examples）。`DirectorUpdateMode.Manual` 時呼叫者 `PlayableGraph.Evaluate(dt)`。
- **第 3 層**：遊戲腳本設 Animator 的參數；`StateMachineBehaviour` 掛在狀態上收回呼（`E`script-reference/unityengine/statemachinebehaviour）。

### 1.4 Unreal 與 Lyra（官方文件）

- **第 1 層**：Skeleton＋Animation Sequence（每根骨的 key、壓縮設定、notify 與 curve 存在 sequence 上）。https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-sequences-in-unreal-engine
- **第 2 層全在 Engine**：Animation Blueprint 的 **AnimGraph** 每幀求值到 Output Pose，預設在 worker thread 上跑。https://dev.epicgames.com/documentation/en-us/unreal-engine/graphing-in-animation-blueprints-in-unreal-engine
  - State Machine（https://dev.epicgames.com/documentation/en-us/unreal-engine/state-machines-in-unreal-engine）、Blend Space、Aim Offset（mesh space 的 additive）。
  - 上下半身：**Layered blend per bone**，例：Bone Name `spine_01`、Blend Depth 1、開 Mesh Space Rotation Blend，上半身由 Slot 接入。https://dev.epicgames.com/documentation/en-us/unreal-engine/using-layered-animations-in-unreal-engine
  - 一次性動作（Draw、Hide、Reload）：**Montage＋Slot**。Montage 的播放**不會自動複製**，要自己發 RPC（只有 root motion 會複製）。https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-montage-in-unreal-engine
  - 事件：Anim Notify；Montage 的 tick type 分 Queued（非同步、略不準）與 Branching Point（逐幀準，用在 gameplay 判斷）；有「Trigger on Dedicated Server」旗標。https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine
- **第 3 層**：EventGraph（game thread）或 Blueprint Thread Safe Update Animation（worker thread）＋Property Access 讀 gameplay 的值。
- **Lyra**（https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-in-lyra-sample-game-in-unreal-engine）：主 ABP `AnimBP_Mannequin_Base` 放 locomotion 狀態機與上下半身的 Layered blend per bone；Gameplay Tag Property Map 把 GAS 的 tag 綁成變數。武器相關的部分走 Linked Anim Layer（見第 3 節）。

### 1.5 比較表：每家把什麼放在 Engine

| 項目 | Bevy | Godot | Unity | Unreal | gyo2 現況 |
|---|---|---|---|---|---|
| 骨架、片段、時刻 t 的取樣 | Engine（曲線取樣是純的，整副姿勢綁 World） | Engine（`const` 取樣） | Engine | Engine | **Engine：model 純函式**（`SampleClip`） |
| 兩個姿勢的混合 | Engine（`Blend` 節點） | Engine（Mixer） | Engine | Engine | **Engine：`Model::Blend`** |
| additive | Engine（`Add` 節點） | Engine（Add2／Add3） | Engine（Additive layer） | Engine（Aim Offset、Apply Additive） | 遊戲（`CharacterRig.cpp:304-315` 手寫） |
| 上下半身遮罩 | Engine（mask group，u64） | Engine（節點的 filter） | Engine（Avatar Mask） | Engine（Layered blend per bone） | 遊戲（`CharacterRig.cpp:193-198` 算 `upperMask`，`:304-315` 逐節點選） |
| 每個實體播哪個、播到哪 | **Engine**（`AnimationPlayer`） | **Engine**（Player／Tree） | **Engine**（Animator／Playables） | **Engine**（AnimInstance） | **遊戲**（`PlayerAnimation`，照 tick 推；11-23 決定） |
| 轉場（crossfade） | Engine（`AnimationTransitions`） | Engine（`xfade_time`） | Engine（Transition Duration） | Engine（State Machine 的 blend） | 遊戲（`Locomotion::moveWeight` 的 ease，`PlayerAnimation.cpp:110-111`） |
| 狀態機 | 無（遊戲的 system） | Engine（StateMachine） | Engine（Animator Controller） | Engine（State Machine） | 遊戲（`ResolveActions`，`PlayerAnimation.cpp:139-182`） |
| 動畫事件 | Engine（區間穿越含 loop） | Engine（method track 等，未細查） | Engine（Animation Event） | Engine（Anim Notify） | 遊戲（`ClientWorld.cpp:890` 手算一個事件） |
| 播放頭的時間來源 | `Time` 的 delta（`PostUpdate`） | Idle／Physics／Manual | Normal／Fixed／Manual | 每幀（可在 server 上 tick，見第 5 節） | sim 的 tick（Match 與 Client 同一推導） |

**觀察**：四家都把第 2 層放在 Engine；但**沒有一家的第 2 層是「tick 的純函式」**——播放頭都是「上一幀的值＋delta」的狀態。Unity Netcode 為此另做了一層，把動畫狀態變成 ghost data（第 5.2 節）。gyo2 現在的做法（片段時刻由 sim 的時間戳推）比較像 Unity Netcode 那一層，而不是 Bevy 的 `AnimationPlayer`。

## 2. IK

### 2.1 各家

| | 有沒有 | 放哪一層 | 和取樣、混合的先後 | 握點怎麼表達 |
|---|---|---|---|---|
| **Bevy** | **Engine 沒有 IK**（`crates/`、`examples/` 搜 IK、inverse kinematic、two_bone、fabrik 都沒有）；只有 `Transform::looking_at` 等朝向工具（`crates/bevy_transform/src/components/transform.rs:187,463`） | — | 使用者自己的 system，排在 `animate_targets` 之後、transform 傳播之前或之後自己決定（未確認有官方範例） | 沒有 socket 概念；掛件是骨頭 entity 的子 entity（使用者的寫法） |
| **Godot** | Engine：`SkeletonModifier3D` 系列——`TwoBoneIK3D`（`scene/3d/two_bone_ik_3d.h:35`）、`FABRIK3D`（`fabr_ik_3d.h:35`）、`CCDIK3D`（`ccd_ik_3d.h:35`）、`JacobianIK3D`、`SplineIK3D`、`LookAtModifier3D`、`AimModifier3D`；舊的 `SkeletonIK3D` 已標 deprecated（`doc/classes/SkeletonIK3D.xml:2`） | 第 2 層之後的後處理 | **Mixer 先寫局部姿勢，modifier 後跑**：`NOTIFICATION_UPDATE_SKELETON` 先備份姿勢（`skeleton_3d.cpp:369`），跑 modifier（`:377`），發 `skeleton_updated`（`:388`）、上傳蒙皮，**最後把姿勢還原成 modifier 之前的**（`:452`）。所以 IK 的結果進畫面，但不回流到下一幀的動畫。modifier 照子節點順序跑、各有 influence（`skeleton_3d.cpp:1253-1270`） | IK 目標是任一 `Node3D` 的 `NodePath`（`two_bone_ik_3d.h:52-55`），例：武器上的 `Marker3D`；`BoneAttachment3D` 是 socket（`bone_attachment_3d.h:35`），跟著 modifier 之後的姿勢走 |
| **Unity** | 內建：Humanoid 的 IK Pass、`OnAnimatorIK`、`SetIKPosition`（`E`manual/animation-section/animation-mecanim/avatar-creationand-setup/inverse-kinematics）。套件 Animation Rigging：Two Bone IK、Chain IK（**FABRIK**）、Multi-Aim、Damped Transform 等 12 種 constraint（https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/ConstraintComponents.html） | Animator 之後 | Rig Builder 在 Animator 的狀態機之後接一段 PlayableGraph，constraint 以 Animation Job 跑；順序是 rig 子階層的深度優先（https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/RiggingWorkflow.html） | 文件建議在接觸點放一個空的子物件當把手；Two Bone IK 的 Target 是一個 Transform（https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/constraints/TwoBoneIKConstraint.html） |
| **Unreal** | Engine：AnimGraph 的 Two Bone IK、FABRIK、CCDIK、Hand IK Retargeting；Control Rig 的 Full Body IK | AnimGraph 裡、混合之後 | Virtual Bones 文件的例子：基礎姿勢 → Layered blend per bone → Two Bone IK 對到 virtual bone（https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-bones-in-unreal-engine）。「蒙皮之前」是從 Output Pose 是最終姿勢推的，沒有頁面明寫 | Skeletal Mesh Socket：骨頭上的一個具名變換（https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletal-mesh-sockets-in-unreal-engine）；Hand IK Retargeting 讓共用的武器握法適應不同體型（https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-hand-ik-retargeting-in-unreal-engine）。UE mannequin 的 `ik_hand_gun` 等骨頭只在社群頁面有解釋；Lyra 的左手 IK 怎麼做，官方 Lyra 頁沒寫（未確認） |

### 2.2 共同點

- 三家（Godot、Unity、Unreal）都在 Engine 提供 two-bone IK，也都提供 FABRIK 或 CCD 這類鏈式解；Bevy 都沒有。
- 順序一致：**取樣 → 混合（含遮罩、additive）→ IK → 蒙皮**。IK 讀的是模型空間（或世界空間）的姿勢，改的是鏈上幾根骨的局部旋轉。
- 握點都是「某個節點＋局部偏移」：Unreal 的 socket、Godot 的 `Marker3D`／`BoneAttachment3D`、Unity 的子 Transform。目標點在**武器模型**上，IK 鏈在**角色**上。
- Godot 明確把 IK 當成「只給畫面的後處理」（每次還原）。另外三家沒有這個規定。

## 3. 姿勢組（依武器換一組片段）

| | 做法 | 資料長什麼樣 | 是 Engine 還是遊戲 |
|---|---|---|---|
| **Unreal／Lyra** | Animation Layer Interface 宣告一組 layer；`Link Anim Class Layers` 在執行時把介面換成另一個 ABP class 的實作；文件建議播一個 Montage 遮住切換（https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-linking-in-unreal-engine）。Lyra：介面 `ALI_ItemAnimLayers`，基底 `ABP_ItemAnimLayersBase`，每把武器是它的**子 ABP**（例 `ABP_PistolAnimLayers`），**子 ABP 只指定動畫資產與變數**（Lyra 動畫頁） | 「一個子 class，裡面的變數指向 sequence」。依 cosmetic tag 挑哪個子 class（`PickBestAnimLayer`、`FLyraAnimBodyStyleSelectionSet`）只在社群頁面有（https://x157.github.io/UE5/LyraStarterGame/CharacterParts/CharacterPart），官方未確認 | 連結機制是 Engine；介面、基底、每把武器的子 ABP、挑選規則是專案 |
| **Unity** | `AnimatorOverrideController`：沿用同一個 controller 的狀態、參數、轉場，只換片段。`ApplyOverrides` 一次換多個；文件的例子本身就是換武器（`E`script-reference/unityengine/animatoroverridecontroller/applyoverrides）。換成同一 controller 的 override 時不重設目前狀態（`E`script-reference/unityengine/animator/runtimeanimatorcontroller）。限制：結構不能不同；exit time 要用正規化時間，否則短的片段會跳過（`E`manual/.../animator-override-controller） | **「原片段 → 替代片段」的對照表** | 對照與重新綁定是 Engine；每把武器的表是資產 |
| **Godot** | 沒有專用機制。可行的路：`remove_animation_library`＋`add_animation_library` 換同名的 library（`animation_mixer.cpp:295-353`；片段鍵是 `"lib/anim"`，`:162`）；節點用名字找片段（`animation_blend_tree.cpp:283,301`），所以換庫後取到新片段。**注意（讀碼推論，沒跑）**：節點實例快取的長度與 loop 只在節點自己的版本變時更新（`animation_blend_tree.cpp:417-429`），換庫不觸發，長度可能是舊的。其他路：Transition 節點在每把武器的子圖之間選 | 「名字 → `Animation`」的庫 | 庫是 Engine 的資料；怎麼換是遊戲腳本 |
| **Bevy** | 沒有 override 或 layer 交換的功能。可行的路：(a) 換 entity 的 `BlendGraphHandle`；`active_animations` 的鍵是節點 index，只在一張圖內有意義；(b) 改 asset 的 `Clip(handle)`，但 asset 共用，會影響所有用它的玩家（範例 `animation_masks.rs:313-341` 就是改 asset）；(c) 所有姿勢組放進同一張圖、靠權重或 `AnimationTransitions` 切 | — | 全是使用者用 Engine 原語組出來的 |

**共同點**：姿勢組都是**資料**——「角色（role）→ 片段」的對照。Unity 與 Lyra 的形狀最接近：一份共用的結構（controller／基底 ABP），每把武器只給一張「這個 role 用哪個片段」的表。切換時都用一段一次性動畫（Montage、Draw）遮住，結構不變，所以不需要重設播放狀態。

## 4. 對照 gyo2 現況

### 4.1 model（Engine 第 1 層）已經有的

`engine/modules/model/include/engine/model/Animation.hpp`：

| 函式 | 行 | 用途 |
|---|---|---|
| `RestPose` | `:38` | 每個節點的 rest |
| `SampleClip(nodes, clip, seconds, wrap, out)` | `:45-46` | 時刻 t 的局部姿勢；`Wrap::Clamp`／`Loop` |
| `Blend(a, b, weight, out)` | `:51-52` | 兩個姿勢以**單一權重**混合（平移、縮放 lerp，旋轉 slerp） |
| `ComputeModelSpace` | `:56-57` | 模型空間矩陣 |

另外有 `BindByName`、`Retarget`（`Retarget.hpp`）與 FBX loader。檔頭註解明寫：純函式、不保留狀態；跨平台最後幾位元可能不同（`Animation.hpp:20-23`）。

### 4.2 object_fps_pvp 在遊戲層自己做的

- **`sim/PlayerAnimation`**（第 2、3 層，純函式、不用 model 的型別）：
  - `Advance`（`PlayerAnimation.cpp:91-137`）：由腳的位移推步態週期 `cycles`、`moveWeight`（以 `transitionSeconds` ease）、`jogWeight`、跳躍階段。**這是有狀態的**（`Locomotion`，`PlayerAnimation.hpp:81-92`），每步累積。
  - `ResolveActions`（`PlayerAnimation.cpp:139-182`）：由 sim 的時間戳（`shotSeconds`、`reloadStart`、`hitSeconds`、`lifeSeconds`）算「上半身播哪個、進度多少」。**這部分是無狀態的**：只看時間戳與現在的時刻。
  - Match 每步跑 `Advance`（`PvpWorld.cpp:548` 的 `AnimatePlayers`）；Client 對遠端玩家在內插時間軸上各自跑一份（`ClientWorld.cpp:739` 起的 `PoseCharacters`）。
- **`character/CharacterRig`**（姿勢合成）：
  - 讀角色：一個身體、16 個 role 的片段、**全部來自同一個片段庫**（`CharacterRig.cpp:163-170`，多於一個模型就報錯）。
  - `ComposeBody`（`CharacterRig.cpp:244-316`）：下半身＝ idle／walk／jog 的混合（或跳躍、死亡）；上半身＝ aim（neutral 對 up／down 混合）**加上**動作片段「相對第 0 幀的動作量」（additive，`:304-315`）；`upperMask` 由 `upper_body_root`（`spine_01`）往下算（`:193-198`）。
  - 武器掛點：`presentation.json` 的 `weapon {node: hand_r, translation, rotation_xyzw, scale}`（`CharacterRig.cpp:199-202`），就是一個 socket 的資料。
  - `PlaceHitShapes`（`CharacterRig.cpp:329-347`）：把 `hitbox.json` 的 7 個形狀擺到骨頭上。
- **第一人稱**：`ViewmodelClip`（`ClientWorld.cpp:826-839`）依序判 reload、shoot、Draw（新的一條命的開頭）、idle；`PoseViewmodel`（`ClientWorld.cpp:843`）取樣、算模型空間，並在 `magazine_in` 的時刻減 `sync_seconds` 播換彈聲（`ClientWorld.cpp:890`）。**Hide 片段有資料但沒有用到**（`viewmodel.animset.json` 有 `Hide`，`ViewmodelClip` 不選它）。
- **資料**（都是遊戲的格式）：
  - `assets/object_fps_pvp/weapons/nailer_v2/viewmodel/viewmodel.animset.json`：role（Idle、Shoot、Reload、Draw、Hide）→（模型、片段名）。
  - `viewmodel.anim_events.json`（`gyo.anim_events`）：片段 → 事件時刻的清單，例 `Armature|Reload` 1.887 s 的 `magazine_in`。
  - `viewmodel.event_sounds.json`（`gyo.event_sounds`）：事件 → 聲音、`sync_seconds`、`gain`。
  - `characters/player/animations.animset.json`：第三人稱 16 個 role → 片段（**只有一組**，沒有依武器分）。

### 4.3 批次表的「動畫事件做成 model 的純函式」那一列

`rebuild-batch-plan.zh-Hant.md:115`：model 的純函式「區間內穿過了哪些時刻（含跨 loop）」＋它的資料；JSON 的讀法與事件 → 聲音是遊戲的。PvP 移植宣告當時判「作者能處理，有第二個產品要時再看」（`port-declaration.zh-Hant.md:58`）。

多把武器之後，每把武器的 viewmodel 都可能有自己的事件（拉槍機、上彈匣、Draw 的出槍聲），現在 `ClientWorld.cpp:890` 那種「只判一個事件」的寫法會變成 N 份。Bevy 把這件事放在 Engine（`lib.rs:1448-1527`）。

### 4.4 把 W3 的例子逐步拆：缺什麼、放哪

| 需要的 | 前提：Engine 要解決，還是作者能處理 | 各家 | 建議放哪 |
|---|---|---|---|
| A. 每把武器一組 viewmodel 片段（Draw、Hide、Idle、Shoot、Reload） | 作者能處理：資料格式已有（`viewmodel.animset.json`） | 全是資料 | 遊戲（W1 武器資料化） |
| B. 切換時 Hide → Draw | 作者能處理：由 sim 的「切換開始 tick」推片段與時刻，和現在的 `ViewmodelClip` 同一種寫法 | Unreal：Montage；Unity：狀態＋transition；Bevy：`AnimationTransitions` | 遊戲（sim 加切換狀態） |
| C. 第三人稱換一組上半身片段 | 作者能處理：姿勢組是資料（第 3 節的共同點） | Unity override 表、Lyra 子 ABP、Godot library | 遊戲；資料形狀見第 7 節 D4 |
| D. 換組時上半身的 crossfade（舊組淡到新組） | 一半一半：「何時、多久」是規則（遊戲）；「兩個姿勢依每根骨的權重混」是機制 | 四家的遮罩都在 Engine | 規則在遊戲；**逐骨權重的混合**補進 model（見 E） |
| E. 遮罩混合、additive | **機制**：四家都在 Engine；gyo2 現在由 `CharacterRig.cpp:304-315` 手寫，多一組姿勢就多一份迴圈 | 1.5 表 | **model 補純函式**（第 6 節 R2） |
| F. 左手 IK 到護木 | 解法是機制（純數學，沒有規則）；「哪條鏈、目標在哪、權重多少、何時開」是規則 | Godot／Unity／Unreal 在 Engine；Bevy 沒有 | **two-bone IK 的純函式放 model**（第 6 節 R3，偏離 Bevy）；套用在遊戲 |
| G. 握點（武器上的左手目標、右手掛點） | 作者能處理：「節點名＋局部偏移」的資料＋現有的 `FindNode`、`ComputeModelSpace` | Unreal socket、Godot Marker3D、Unity 子 Transform | 遊戲的資料（武器檔）；Engine 不加 socket 型別 |
| H. 動畫事件（每把武器的換彈、Draw 聲） | 機制：區間穿越（含 loop）是固定的算法；事件 → 聲音是規則 | Bevy、Unity、Unreal 都在 Engine | **model 補純函式**（批次表第 115 列，第 6 節 R2） |
| I. 片段來自多個片段庫 | 作者能處理：`CharacterRig.cpp:163-170` 的限制是遊戲自己的 | — | 遊戲（手槍的片段可能在另一個 FBX） |
| J. Match 用同一組姿勢擺 hitbox | 作者能處理：Match 已經跑同一份 `ComposeBody`；要補的是「目前的武器、切換開始 tick」進 sim | 第 5 節 | 遊戲（sim 狀態） |

## 5. 決定性與權威

### 5.1 gyo2 的事實

- hitbox 只有頭（`Head` 的球）、軀幹（`spine_01→spine_03`、`pelvis→spine_01` 兩個膠囊）、腿（4 個膠囊），**沒有手臂**（`assets/object_fps_pvp/characters/player/hitbox.json`）。
  - 所以**左手 IK 不影響現在的判定**。但使用者已定做肢體傷害（0.2 節），手臂形狀加進來之後，左手 IK 就會進判定（第 11 節）。
  - 但上半身的根是 `spine_01`（`presentation.json` 的 `upper_body_root`）：**換姿勢組、換組的 crossfade、瞄準，都會動到頭與軀幹的 hitbox**。這些必須由 Match 也算得出來的 sim 狀態推。
- Match 在開槍那一步現算姿勢（`PvpWorld.cpp:508-513`），不存姿勢、不存歷史；`Locomotion` 每步累積（`PvpWorld.cpp:548`）。
- 取樣結果跨平台不同（PV6）：linux-x64＝macos-arm64，windows-x64、macos-x64 各自不同（`porting-inventory.zh-Hant.md:114`）。Match 是 Linux、Client 多是 Windows，兩邊的姿勢不會逐位元相同；因為只有 Match 判，所以沒有出過問題。

### 5.2 各家怎麼處理「姿勢影響判定」

| | 判定在哪 | 回溯什麼 | 姿勢參不參與 |
|---|---|---|---|
| **lightyear**（avian 整合） | 伺服器（`examples/fps/src/server.rs:138-141` 讀射手的 `InterpolationDelay`） | 每個碰撞體的 `(Position, Rotation, ColliderAabb)` 歷史，預設 35 tick（`crates/integration/avian/src/lag_compensation/history.rs:43,72`）；查詢時在兩個 tick 間 lerp／slerp | **不參與**：源碼明寫「collider 形狀假設不變，只取樣位姿」（`lag_compensation/query.rs:300`）；範例沒有動畫 |
| **bevy_replicon** | — | 沒有（只在 README 指向第三方 `bevy_rewind`，`README.md:60`） | — |
| **Unity Netcode for Entities** | 伺服器 | 整個 Unity Physics 的 `CollisionWorld`，每 tick 一份，環形 32 份（`Runtime/Physics/PhysicsWorldHistory.cs:118`），取 `tick − CommandDataInterpolationDelay`（`PhysicsWorldHistory.cs:41-57`）；官方文件：只有碰撞世界會回溯，其他狀態要自己追（https://docs.unity3d.com/Packages/com.unity.netcode@1.8/manual/server-rewind.html） | **間接**：`GhostAnimationController` 把動畫狀態放進 ghost 的元件、可在預測裡求值（`Runtime/Hybrid/GhostAnimationController.cs:63-70,263-310`）；但套件裡沒有「骨頭帶動碰撞體」的程式，要使用者自己做（未確認有官方範例）。伺服器建置的文件警告：有 hitbox 的骨頭不要剝掉（https://docs.unity3d.com/Packages/com.unity.multiplayer.tools@2.2/manual/porting-to-dgs/optimizing-server-builds.html） |
| **Unity NGO** | — | — | `NetworkAnimator` 同步的是 Animator 的狀態、轉場、參數，**不是骨頭姿勢**；每台機器自己算姿勢（https://docs.unity3d.com/Packages/com.unity.netcode.gameobjects@2.4/manual/components/networkanimator.html） |
| **Unreal** | 預設由作者決定；dedicated server 上骨架要不要更新看 `VisibilityBasedAnimTickOption`（https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/VisibilityBasedAnimTickOption）。伺服器「沒有在畫」，用 `AlwaysTickPose` 時骨頭不刷新、hitbox 不跟著動（社群頁面，官方未確認） | **官方沒有回溯**（CMC 的網路頁只講自己的移動預測，https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine；回溯是第三方插件） | 由作者選擇讓伺服器 tick 姿勢 |
| **Lyra** | **Client 先判、伺服器確認**：`ULyraGameplayAbility_RangedWeapon` 在本機 Client 打射線，target data 送到伺服器，由 `ULyraWeaponStateComponent` 確認後才扣血（https://dev.epicgames.com/documentation/en-us/unreal-engine/abilities-in-lyra-in-unreal-engine）。確認時檢查什麼，文件沒寫（未確認） | 沒有 | Client 用自己畫面上的姿勢 |

**共同點**：

1. 沒有一家回溯「骨架姿勢」；有回溯的都只回溯「碰撞體的位姿」。要姿勢準的 hitbox，做法是**伺服器每 tick 把骨頭帶動的形狀寫進碰撞狀態**，讓它們一起進歷史。
2. 動畫狀態在網路上傳的是「參數、狀態、時間」（NGO、Unity ghost data、Unreal 要自己發 RPC），不是姿勢本身。
3. 沒有一家要求跨平台的姿勢逐位元相同。

### 5.3 選項

| | 做法 | 需要什麼 | 代價 |
|---|---|---|---|
| (a) | 只在 Match 判，用當 tick 的姿勢（現況、master 的做法） | 姿勢由 sim 狀態推得出來（第 4.4 節 J） | Client 看到的對手落後，要提前量（PV3 已寫明） |
| (b) | Match 判＋回溯：每 tick 存**擺好的 hit shape**（現在 7 個形狀，肢體傷害之後 11 個；每個兩點＋半徑），照 `ViewTime` 在兩 tick 間插值 | Match 每 tick 都要算所有玩家的姿勢（現在只在有人開槍時算；肢體傷害之後還要加 IK）；一份歷史 | 記憶體小（7×7 個 float／人／tick；肢體傷害之後 11×7）；每 tick 的取樣量：盤點量到 12 次取樣 0.44 ms（`porting-inventory.zh-Hant.md:185,222`）。和 lightyear、Unity 同一種 |
| (c) | Match 判＋回溯：存**動畫輸入的歷史**（`Locomotion`、時間戳），回溯時重算姿勢 | 每 tick 存輸入；查的時候重算 | 存得更少、查得更貴；`Locomotion` 是累積的，要存它而不是只存移動 |
| (d) | Client 判、伺服器確認（Lyra） | 伺服器要能驗證 Client 說的命中 | 伺服器拿自己的姿勢去驗，會因 PV6 的跨平台差異而有邊緣誤判；要嚴格驗就要 (e) |
| (e) | 跨平台決定性的取樣（固定運算順序、自己寫 slerp／nlerp、不用 libm 的 `sin`／`acos`） | 改 model 的數學、加跨平台的 golden | 只有 (d) 或「Client 也預測命中」才需要；四家都沒有做 |

## 6. 推薦與理由

### R1. 第 2 層維持在遊戲（不推翻 11-23）；W3 沒有觸發重開的條件

- W3 要的 Hide→Draw、換姿勢組、換組的 crossfade，都能寫成「sim 的時間戳 → 片段與時刻」的推導，和 `ResolveActions`、`ViewmodelClip` 同一種寫法。Match 用同一份推導擺 hitbox，Client 對遠端玩家在內插時間軸上跑同一份。
- **偏離 Bevy 的原因**（Bevy 把 `AnimationPlayer`、`BlendGraph`、`AnimationTransitions` 放在 Engine）：Bevy 的播放頭是 `Time` 的 delta 累積、排在 `PostUpdate`（`lib.rs:1045-1051,1316-1335`），不是 tick 的函式，進不了 Match 的判定與回溯。要用它就得像 Unity Netcode 那樣另包一層，把播放狀態變成複製資料（`GhostAnimationController.cs:100-121`）。今天只有 PvP 一個產品，而它已經用 tick 推導，Engine 加一個播放器只會多一層沒人用的狀態（AGENTS §2.2「該不該存在」）。
- 重開的條件照 11-23 不變：等出現「不進判定、只要播」的產品（NPC、model_viewer、過場），再照 Bevy 的 `AnimationPlayer` 做。

### R2. model 補三個第 1 層的純函式：逐骨權重的混合、additive、事件的區間穿越

- 四家都把遮罩與 additive 放在 Engine（1.5 表），Bevy 也是（`Add` 節點、mask group）。它們沒有規則，只是固定的算法；W3 的換組 crossfade 會讓 `CharacterRig.cpp:304-315` 那種手寫迴圈變成好幾份。
- 形狀照 model 現有的寫法：呼叫者給 span，不保留狀態。
  - 逐骨權重的混合：`Blend(a, b, weights, out)`，`weights` 每個節點一個。遮罩就是 0／1 的權重，「哪些骨算上半身」仍由呼叫者給。和 Bevy 的不同：Bevy 用 u64 的 mask group（最多 64 組，`graph.rs:486-489`），這裡每個節點一個 float，因為沒有圖可以掛 mask；也能表達 Unreal 的 Blend Depth 那種漸變。
  - additive：「把 `motion` 相對 `reference` 的動作量加到 `base` 上，乘以權重」，就是 `CharacterRig.cpp:309-314` 的式子。
  - 事件：`CrossedEvents(times, from, to, duration, wrap)` → 穿過了哪些事件（含跨 loop），照 Bevy `TriggeredEvents::from_animation`（`lib.rs:1448-1527`）的區間規則；事件的資料（`gyo.anim_events`）的讀法仍是遊戲的（批次表第 115 列原本就這樣分）。

### R3. two-bone IK 做成 model 的純函式；FABRIK、CCD 現在不做

- 解法是純數學，沒有規則（§1-1 的機制）；Godot、Unity、Unreal 都在 Engine 提供，順序也一致（混合之後、蒙皮之前）。
- **偏離 Bevy 的原因**：Bevy 的 Engine 沒有 IK。這裡放進 model 是因為 model 本來就是「姿勢的純函式」那一層；放進去的只有一個函式，不加 modifier stack、不加元件，膠水很少。要照 Bevy 的話，就是由遊戲自己寫（第 7 節 D2 的選項 (b)）。
- 形狀：給局部姿勢與模型空間矩陣、三個節點（root、mid、tip）、模型空間的目標點與 pole、權重，寫回 root 與 mid 的局部旋轉。FABRIK、CCD 沒有點名的需求，不做（沒觀測到需求就不擴大）。
- 第三人稱的左手 IK **Match 和 Client 都跑**（2026-11-27 改。原本推薦只在 Client，前提是 hitbox 沒有手臂；肢體傷害定了之後不成立，見第 11 節）。
  - IK 放在 `ComposeBody`→`ModelSpace` 這條兩端共用的路徑上，不放在 Client 才有的 `PoseCharacters`（`ClientWorld.cpp:739`）。Match 只在要擺 hit shape 的那一步跑（現在是有人開槍的那一步，`PvpWorld.cpp:508-513`）。
  - Godot 把 IK 當成只給畫面的後處理（`skeleton_3d.cpp:452`）；這裡不照它，因為手臂進了判定。
  - IK 的數學（常見的解法用餘弦定理，要 `acos`）和取樣落在同一區：有動畫的命中判定是 DET-1 的中間區（`docs/architecture/determinism.md:22`）。只有 Match 判，所以不會分歧；E2 不必另做跨平台決定性。

### R4. 姿勢組與握點都是遊戲的資料；形狀照 Unity override／Lyra 子 ABP

- 一份共用的第三人稱 role 清單（現在的 16 個），每把武器只寫「這些 role 換成哪個片段」的表。沒寫的沿用基本組。
- 握點寫在武器檔：右手掛點（現在 `presentation.json` 的 `weapon`，搬進武器）、左手目標（武器模型上的節點名＋偏移，或直接是偏移）。
- Engine 不加 socket 型別：Bevy 沒有（掛件是子 entity），現有的 `FindNode`＋`ComputeModelSpace` 就夠。
- 肢體傷害之後，左手握點是**影響判定的資料**：Match 要讀武器檔的握點（2026-11-27 補，第 11 節）。

### R5. 權威：現在照 (a)；做 PV3 時用 (b)；(e) 不做

- (a) 是現況，換組要補的只是把「目前的武器、切換開始 tick」放進 sim，讓 Match 推得出同一組姿勢。
- 做回溯時照 lightyear、Unity 的共同做法：存擺好的形狀，不存姿勢（(b)）。
- 四家都沒有要求跨平台的姿勢逐位元相同；只有改成 (d)（Client 判）時才要 (e)。
- 肢體傷害之後（2026-11-27 補）：(a) 在開槍那一步多跑 IK、多擺 4 個手臂形狀；(b) 每 tick 要跑所有玩家的取樣＋IK，存 11 個形狀。選擇本身不變。

## 7. 需要使用者決定的點

| # | 問題 | 選項與取捨 | 推薦 |
|---|---|---|---|
| D1 | 第 2 層（每個實體的播放器）要不要因為 W3 重開？ | (a) **不重開**，W3 照 tick 推導寫在遊戲（11-23 的決定不變）。(b) Engine 做「tick 驅動的播放描述」：元件記（片段、開始 tick、速度、wrap），時刻＝tick 的函式，可以複製、可以回滾——Unity Netcode ghost 動畫的路，和 Bevy 不同、沒有直接的參考實作。(c) 照 Bevy 做 `AnimationPlayer`＋`BlendGraph`：有參考，但進不了判定，PvP 用不到 | (a) |
| D2 | IK 放哪？ | (a) **two-bone 的純函式放 model**（偏離 Bevy；Godot、Unity、Unreal 都在 Engine）。(b) 遊戲自己寫（照 Bevy；第二個產品要時再移進 model）。(c) Engine 做 modifier stack（Godot `SkeletonModifier3D` 的 ECS 版）：要排程、元件、順序規則，現在沒有需求 | (a) |
| D3 | 遮罩混合與 additive 要不要補進 model？ | (a) **補**（四家都在 Engine，含 Bevy）。(b) 維持遊戲的手寫迴圈：省一批，但每加一組姿勢就多一份 | (a) |
| D4 | 每把武器的第三人稱姿勢組，資料長什麼樣？ | (a) **基本組＋每把武器的替換表**（Unity override、Lyra 子 ABP 只填資產）：武器檔小，但換組時要知道「哪些 role 被換了」。(b) 每把武器一份完整的 animset：好讀，16 個 role 每把都抄一遍 | (a) |
| D5 | 第三人稱的 IK 在 Match 跑不跑？ | (a) 不跑：只在 hitbox 沒有手臂時成立。(b) **跑**：放在兩端共用的姿勢路徑上，Match 讀武器的握點資料；Match 只在要擺 hit shape 的那一步跑 | **(b)**（2026-11-27 改；原本 (a)，前提隨肢體傷害的決定不成立） |
| D8 | 手臂要不要判？（第 10 節） | **使用者已定**：(3)，手臂是獨立部位、有自己的倍率和血量（`v8-arch-gyo2-limb-damage`）。不再是決策點；原本列的 (A)～(D) 留在 10.4 節 | — |
| D9 | 手臂的精度驗證，量什麼、門檻多少？（2026-11-27 改：原本是「決定要不要加手臂」的量測） | 量兩件：(i) 同一組輸入下，Client 畫的手臂和 Match 擺的手臂差多少（Windows 對 Linux，PV6）；(ii) 手臂膠囊包住手臂網格的程度（漏的、多的）。門檻要在量之前定。這張卡不提數字 | 使用者定 |
| D10 | 同一目標內，手臂擋不擋後面的部位？（2026-11-27 新增） | (a) **擋**：照現行規則取最近的形狀（`PvpWorld.cpp:513-519`），打在手臂上就扣手臂的血。判定規則不用改。塔科夫類的遊戲裡手臂會替胸口擋子彈（△ 社群說法，未確認）。(b) 不擋（CS 2016 的做法）：手臂後面有部位就算那個部位，手臂只在「只中手臂」時扣血，部位血量的意義變小。(c) 穿透：先扣手臂，再照穿透量進後面的部位；要武器有穿透數值，現在沒有 | (a)；(c) 等武器有穿透數值再議 |
| D6 | 換組的 crossfade 期間，Match 的 hitbox 用什麼姿勢？ | (a) **用 crossfade 中的姿勢**（和 Client 畫面一致，Match 推得出來）。(b) 在切換 tick 直接換成新組：簡單，但畫面上頭的位置和判定會差一段 crossfade | (a) |
| D7 | 命中回溯（PV3）什麼時候做、存什麼？ | 這張卡只列選項（5.3 的 (b)／(c)），時機照 PV3「這一輪不做」。要做時推薦 (b) | 不在 W1～W4 |

## 8. 如果採用推薦：Engine 和遊戲各要做的批次

批次代號照 v8 的「版本-範圍-主題」寫法。模型／檔位照 `agents/navigator.md`、`agents/effort-gearbox.md`：model 的公開介面是上段，不用小的模型。

```text
W 線（武器）
├─ E1 high｜model 補逐骨權重混合、additive、事件區間穿越（Engine）
├─ E2 high｜model 補 two-bone IK（Engine）
├─ G1 high｜W1 武器資料化：武器檔帶 viewmodel 片段、姿勢組替換表、握點、事件（遊戲）
├─ G2 high｜W3 切換：sim 加「目前武器、切換開始 tick」，Hide→Draw、換組 crossfade、Match 的 hitbox 同一推導（遊戲；需 E1、G1）
├─ G3 high｜左手 IK，放在 Match 與 Client 共用的姿勢路徑（遊戲；需 E2、G1）
├─ G4 medium｜各武器的事件 → 聲音改用 CrossedEvents（遊戲；需 E1、G1）
├─ G6 high｜肢體傷害 (3) 的動畫這一側：手臂形狀、`HitPart::Arm`、Match 擺手臂（遊戲；需 E2、G3、模式資料開關）
└─ G5 medium｜手臂的精度驗證（遊戲；需 G6；D9 先定門檻）
```

| 代號 | 內容 | 上段／下段（根據） | 實作者／審查者／檢證者（模型／檔位） |
|---|---|---|---|
| E1 `v8-arch-gyo2-model-pose-ops` | `Animation.hpp` 加逐骨權重的 `Blend`、additive、`CrossedEvents`（含跨 loop、倒播）；外部測試；只跑相關的突變；`model.zh-Hant.md` 第 4、5 節更新 | 上段（公開介面） | Opus／high；純粹的 Code Review Fable／high；檢證者 Opus／high |
| E2 `v8-arch-gyo2-model-two-bone-ik` | two-bone IK 的純函式（目標、pole、權重、鏈長度不夠時的伸直）；參考 Godot `two_bone_ik_3d.cpp`、Unity Two Bone IK、Unreal Two Bone IK 的參數 | 上段（公開介面） | Opus／high；純粹的 Code Review Fable／high；檢證者 Opus／high |
| G1 `v8-product-pvp-weapon-data`（W1） | 武器檔：viewmodel 的 animset、anim_events、event_sounds、第三人稱替換表、右手掛點、左手握點；`CharacterRig` 允許片段來自多個庫（`CharacterRig.cpp:163-170`） | 上段（資料格式） | Opus／high（資料格式，不降到 medium 以下）；審查 Opus／high |
| G2 `v8-product-pvp-weapon-switch`（W3） | sim 的切換狀態；`ViewmodelClip` 加 Hide→Draw；`ComposeBody` 依武器取組、換組 crossfade（用 E1）；Match 的 hitbox 測試：換組中頭的位置跟著姿勢 | 上段（判定） | Opus／high；審查 Opus／high；檢證者 Opus／high |
| G3 `v8-product-pvp-left-hand-ik`（W3） | 在 `ComposeBody`→`ModelSpace` 這條兩端共用的路徑上，混合之後、算模型空間之前套 E2；Client 每幀跑，Match 在擺 hit shape 的那一步跑（2026-11-27 改：原本只在 Client） | 上段（判定：手臂姿勢進命中） | Opus／high；審查 Opus／high；檢證者 Opus／high |
| G4 `v8-product-pvp-weapon-events` | `PoseViewmodel` 的換彈聲改成通用的事件 → 聲音（用 E1） | 下段 | Opus／medium；審查 Opus／high |
| G6 `v8-product-pvp-limb-damage`（肢體傷害 (3)，批次表「後續小事」那一列） | 動畫這一側：`hitbox.json` 加 `arm` 部位與每隻手臂兩個膠囊（`upperarm→lowerarm`、`lowerarm→hand`），載入（`CharacterRig.cpp:219-220`）跟著擴充；`HitPart::Arm`；同目標規則照 D10；Client 的受擊片段（`PlayerAnimation.cpp:171`）與命中標記（`ClientWorld.cpp:972`）對 `Arm` 的處理。傷害這一側（倍率、`LimbHealth`、Codec 升版本號、模式開關）照 HANDOFF `v8-arch-gyo2-limb-damage`，不在這張卡 | 上段（判定、資料格式、交換內容） | Opus／high；審查 Opus／high；檢證者 Opus／high |
| G5 `v8-product-pvp-arm-accuracy`（原本的 `-arm-exposure-survey`） | 精度驗證：同一組輸入下 Client 與 Match 的手臂形狀差、膠囊對網格；每組姿勢分開報；門檻（D9）在量之前寫下；只報告，不改判定 | 下段（不改判定；但結果是上段決定的輸入，所以不用小的模型） | Opus／medium；審查 Opus／high |

順序：E1、E2 可以和 G1 同時做；G2 等 E1、G1；G3 等 E2、G1；G4 等 E1、G1；G6 等 E2、G3 與模式資料開關（批次表的排法：E2 與模式資料開關之後、W3 前後）；G5 等 G6 與 D9（有步槍姿勢時再量一次）。

肢體傷害帶來的順序變化（2026-11-27）：

- **E2 進了判定的路徑**：它是 G3、G6 的前置。內容不變，測試要加 Match 會碰到的情形（鏈長度不夠、目標在身體裡、權重 0）。E1、E2 之間的先後不必動（兩者都在 W3 之前）。
- **G3 從下段變上段**，而且要在 G6 之前（G6 的手臂形狀要擺在 IK 之後的姿勢上）。
- **G2 和 G6 都改交換內容**：G2 把「目前的武器、切換開始 tick」放進 sim，G6 加 `HitPart::Arm`。批次表寫「交換內容在同一段一起改」，所以兩批的 Codec 版本號建議在同一段一起升。W2（第二把武器的資產）、W4（撿起與丟掉）不碰動畫的 Engine 部分，不在這張表。

## 9. 交給主會話的（不在這張卡）

- 批次表 `rebuild-batch-plan.zh-Hant.md` 第 112、113 列原本寫「待使用者定（未決）」；主會話已在 `gyo2` 18d35c90 改成「已定」，不必再處理。
- `viewmodel.animset.json` 有 `Hide`，程式沒有用（`ClientWorld.cpp:826-839`）。W3 會用到，現在不用處理。

## 10. 手臂的 hitbox（使用者 2026-11-27 追加的分析）

前提：手臂要不要判、怎麼判，是**作者（遊戲）能處理的**：形狀是 `hitbox.json` 的資料，部位與倍率是遊戲的規則；任意方向膠囊的射線相交 Engine 已經有（`PvpWorld.cpp:514` 的 `P::CastRay`），Match 要跑 IK 時用的也是 E2 的 model 純函式（model 可以給 Match 用，`HANDOFF.md:335`）。所以這一節**不產生 Engine 的批次**。

### 10.1 現況與由來

- **決定**：使用者 2026-10-31（`v8-product-hit-rules-three-parts`）：部位只有頭、軀幹、腿，**手臂不設判定**；理由是「沒有塔科夫級的肢體傷害」（master 的 `docs/dev_logs/2026_10_31_pvp_v8_hitbox_survey.zh-Hant.md:48`、`docs/object_fps_pvp/plans/v8/hitbox-plan.md:9`）。這裡只引用決定與理由，不引用 master 的算法或數據。
- **gyo2 的程式照這個決定**：
  - `HitPart` 只有 `None`、`Body`、`Head`、`Leg`（`games/object_fps_pvp/include/object_fps_pvp/sim/CombatRules.hpp:16`）；載入時其他部位名直接報錯（`CharacterRig.cpp:219-220`）。
  - 倍率：頭 2000‰、軀幹 1000‰、腿 750‰（`CombatRules.hpp:25-27`）。
  - 判定：對每個目標，取射線上**最近**的形狀；同距時頭＞軀幹＞腿（`PvpWorld.cpp:513-519`、`CombatRules.cpp:39-46`）。沒有形狀的地方，射線直接穿過去。
- **骨架裡有手臂**：`Player_V20.fbx` 有 `clavicle_l/r`、`upperarm_l/r`、`lowerarm_l/r`、`hand_l/r`；第三人稱的槍掛在 `hand_r`（`presentation.json:22-24`）。
- **部位會往外傳**：`HitPart` 存在 `ShotRecord`、`HitRecord`（`games/object_fps_pvp/include/object_fps_pvp/world/Components.hpp:70-87`），進權威的 hash（`Components.hpp:124-125`）、送給 Client（`games/object_fps_pvp/src/client/ClientExchange.hpp:36-37`）；Client 用它選受擊片段（`PlayerAnimation.cpp:171`）和命中標記的顏色（`ClientWorld.cpp:972`）。**所以多一個 `HitPart` 的值＝改資料格式與交換內容**（上段）。
- 舊線的測試曾明寫「…let a shot through the arm」：站姿 idle 時，右手握槍在胸前約 0.3 m，不在任何形狀裡（master 的 `docs/dev_logs/2026_11_01_pvp_v8_hitbox_shot_judge.zh-Hant.md:35`）。gyo2 的 `tests/object_fps_pvp` 裡搜不到手臂相關的測試字樣；有沒有等價的測試，**未確認**。

### 10.2 「手臂問題」其實是三件事（用遊戲的例子）

| # | 場面 | 現在的結果 | 是不是問題 |
|---|---|---|---|
| 1 | 從側面打敵人伸出去、握著槍的手臂，手臂後面沒有身體 | **沒打中**（射線穿過去，打到後面的牆或什麼都沒有） | 是手感問題：射手看到準星在手臂上，卻沒有命中標記。發生得多不多，**沒量過** |
| 2 | 從正面打，手臂擋在頭或胸前（舉槍瞄準時常見） | 手臂不擋，算**後面的頭或軀幹** | 對射手有利。CS:GO 2016 的更新刻意做成同樣的結果：打到手臂而射線繼續進頭，算頭的倍率（△，PCGamesN 的報導，見 10.3） |
| 3 | 加了手臂形狀之後 | — | 手臂的姿勢就會進判定：左手 IK、握點資料、瞄準時手臂的位置，都要 Match 也算得出來（D5 要改成 (b)） |

第 3 件的代價（推論，沒有量）：

- **Match 要跑 IK、讀握點**：G1 的武器檔裡的左手握點，會從「畫面資料」變成「影響判定的資料」。
- **跨平台差距在手臂上最大**：手是 `spine_03→clavicle→upperarm→lowerarm→hand` 這條鏈的末端，每一節的角度誤差都會往外累積；Client 畫的手臂和 Match 擺的手臂，差距會比頭大（PV6，5.1）。只有 Match 判，所以不會分歧，但「看到打中卻沒算」的距離會變大。
- **形狀數**：每隻手臂兩個膠囊（`upperarm→lowerarm`、`lowerarm→hand`），7 個變 11 個：每個目標的射線測試多 57%；做 PV3 時（5.3 的 (b)）每 tick 的歷史也從 7 個變 11 個。

還有一個時機的因素：現在第三人稱用的是手槍片段（`animations.animset.json:4` 的 `Pistol_Idle_Loop`），雙手靠近胸前；W2 的第二把武器如果是步槍那種前握的姿勢，左手臂會伸到軀幹外面，第 1 件的面積會變大。所以**要不要加手臂，應該照每一組姿勢量**。

### 10.3 別的遊戲怎麼做（都是二手來源）

| 遊戲 | 手臂 | 查證 |
|---|---|---|
| CS:GO／CS2 | 有手臂形狀，倍率和胸口同為 1×，Kevlar 也護手臂；2016 年的更新：打到手臂或腿、射線繼續進頭或腹部時，用較高的倍率 | △ 社群 wiki（https://counterstrikewiki.com/Armor_and_Damage_System）、PCGamesN（https://pcgamesn.com/counterstrike/counter-strike-global-offensive-update-enables-headshots-through-arms）；CS2 是否保留 2016 的規則，**未確認** |
| Valorant | 早期的武器數值表把「腿與手臂」列為同一級 | △ Sportskeeda 1.03 版的整理（https://sportskeeda.com/esports/valorant-what-damage-ratio-weapon-version-1-03）；現在的版本**未確認** |
| Overwatch | 形狀是簡化的；有的英雄手臂有形狀，有的手臂下有空隙 | △ 玩家分析（https://www.dexerto.com/overwatch/overwatch-streamer-reveals-true-hitboxes-for-every-hero-in-the-game-1365648） |
| Unity NfE、lightyear、Unreal | 框架不定義部位；hitbox 是作者的資料（第 5.2 節） | 源碼與官方文件 |

共同點：**有手臂形狀的遊戲，手臂多半用軀幹或腿的倍率，而且（CS）不擋後面的頭**。gyo2 和 CS 只差在第 1 件：只打到手臂時，CS 算 1×，gyo2 算沒打中。

### 10.4 選項

| | 做法 | 好處 | 代價 |
|---|---|---|---|
| (A) | **維持：手臂不判**（10-31 的決定） | 不動判定、資料格式、交換內容；IK 只在 Client（D5 (a)） | 第 1 件的手感問題留著，頻率未知 |
| (B) | 加手臂形狀，**不擋**：同一目標的其他形狀有中，就照現行規則算其他部位；**只中手臂**才算手臂。手臂的倍率先用軀幹的（像 CS 的 1×），不加 `HitPart` 的值 | 第 1 件解決；第 2 件的結果不變；不改交換內容 | Match 要跑 IK、讀握點（D5 改 (b)，G3 變上段）；形狀 7→11；`hitbox.json` 要能標「不擋」（資料格式，上段） |
| (B') | (B)＋手臂自己的部位與倍率（`HitPart::Arm`） | 可以給手臂獨立的倍率與受擊表現 | 再加：`HitPart` 多一個值＝交換內容、權威 hash、Client 的受擊片段與標記都要跟著改 |
| (C) | 加手臂形狀，**照最近的算**（手臂擋住後面的頭） | 規則最簡單（現在的判定不用改） | 舉槍時手臂擋頭，對射手不利；和 CS 2016 以後的方向相反 |
| (D) | 不加手臂，把軀幹膠囊加粗包住手臂 | 不加形狀、不要 IK | 沒有手臂的姿勢（例如手垂下時）身體旁邊的空氣也會中，換成「沒打到卻算中」的抱怨（Overwatch 那種） |

**2026-11-27：使用者選了 (3)**（＝(B')＋部位血量，`v8-arch-gyo2-limb-damage`）。下面 10.5、10.6 是 10-31 的前提還成立時寫的，留著當紀錄；現在的推薦看第 11 節。

### 10.5 推薦（原本的；已被第 11 節取代）

**先 (A)，同時量第 1 件有多常發生；量到夠大再換 (B)。**

1. 10-31 的前提（「沒有塔科夫級的肢體傷害」）沒有變；第 2 件的結果已經和 CS 一樣。
2. 第 1 件的頻率沒有量過，先量再評估（量不到成因就修，容易修錯方向）。量法：在 Match 用的同一組姿勢上，先暫時擺上手臂膠囊（半徑由 gyo2 模型的頂點量），從正面、側面打一張射線網格，算「**只中手臂**的面積 ÷ 目標的總面積」，每一組姿勢（idle、瞄準、跑步、換彈；W2 有步槍姿勢時也量）分開報。**門檻要在量之前由使用者定（D9）**。
3. 換 (B) 時，倍率先對到軀幹，不加 `HitPart` 的值（避開交換內容的改動）；要不要 (B') 另外決定。
4. (C)、(D) 不推薦：(C) 和成熟方案的方向相反；(D) 換來另一種抱怨。

### 10.6 對前面各節的影響（原本的；已被第 11 節取代）

- D5（第 7 節）：(A) 時維持 (a)；採 (B)／(B') 時改成 (b)，G3 的左手 IK 也要在 Match 跑，變成上段。
- G1（第 8 節）：武器檔的左手握點，在 (B) 之後是影響判定的資料。
- 5.3 的 (b)（存擺好的 hit shape）：(B) 之後每 tick 存 11 個形狀。

## 11. 肢體傷害 (3) 定了之後重看（2026-11-27）

前提換了：使用者定了最終產品做肢體傷害，採 (3)，手臂是獨立部位、有自己的倍率和血量（`v8-arch-gyo2-limb-damage`）；所有功能加在 Object_PvP、由模式資料開關（`v8-arch-gyo2-object-pvp-route-a`）。10-31「手臂不設判定」的理由（沒有塔科夫級的肢體傷害）只對 Object_PvP 的舊範圍成立。所以第 10.5 節的 (A) 不再推薦。D1～D7 的其他項，使用者還沒定。

### 11.1 改了哪些推薦

| 項目 | 原本 | 現在 | 理由 |
|---|---|---|---|
| 第 10.5 節 | (A) 維持不判，先量「只中手臂的面積」 | 不再推薦；量測改成精度驗證（D9、G5） | 要不要加手臂已經定了；剩下要知道的是 Match 擺的手臂準不準 |
| R3、D5 | 左手 IK 只在 Client（D5 (a)） | Match 和 Client 都跑，放在兩端共用的姿勢路徑（D5 (b)） | 手臂形狀擺在 IK 之後的姿勢上；Match 不跑 IK，Client 看到左手在護木上、Match 的左前臂卻在片段原本的位置 |
| R4 | 握點是畫面資料 | 握點是影響判定的資料，Match 要讀 | 同上 |
| R5、5.3 (b) | 7 個形狀 | 11 個形狀；(a) 開槍那一步多跑 IK，(b) 每 tick 跑取樣＋IK | 形狀多了 4 個；選擇本身不變（現在 (a)，做 PV3 時 (b)） |
| D8 | 決策點 | 使用者已定 | — |
| D10 | 沒有 | 新增：手臂擋不擋後面的部位，推薦 (a) 擋 | 手臂有自己的血量，「不擋」會讓手臂很少扣血，部位血量的意義變小 |
| 第 8 節 | G3 下段、只在 Client；G5 是曝光量測；G6 是 (B) 的條件批 | G3 上段、兩端都跑、在 G6 之前；G5 是精度驗證；G6 是 (3) 動畫這一側的本體 | 同上；交換內容的版本號和 G2 同一段升 |

**沒改的**：R1（第 2 層留在遊戲）、R2（model 補三個純函式）、D1～D4、D6、D7、E1／E2 的內容。

- 肢體傷害不需要每個實體的播放器：手臂的姿勢仍然是「sim 狀態 → 片段與時刻 → 混合 → IK」的推導，Match 推得出來，和 R1 的理由相同。
- D6（換組 crossfade 期間用 crossfade 中的姿勢）現在也涵蓋手臂。

### 11.2 (3) 在動畫這一側要的東西

1. **手臂形狀**：每隻手臂兩個膠囊（`upperarm→lowerarm`、`lowerarm→hand`），骨頭在 `Player_V20.fbx` 裡都有；半徑照現有形狀的量法，由 gyo2 模型的頂點量。
2. **IK 在共用路徑**（G3）：Match 擺手臂之前要先跑。
3. **鏈末端的誤差最大**：手在 `spine_03→clavicle→upperarm→lowerarm→hand` 的末端，Client（多是 Windows）和 Match（Linux）的取樣差距在這裡累積最多（PV6，推論、沒量）。只有 Match 判，不會分歧，但「看到打中卻沒算」的距離會比頭大。這就是 D9 (i) 要量的。
4. **Client 對 `Arm` 的處理**：受擊片段現在只分頭和其他（`PlayerAnimation.cpp:171`），`Arm` 先走「其他」不必改；命中標記只看是不是頭（`ClientWorld.cpp:972`），也不必改。要不要給手臂專用的受擊片段，是之後的表現選擇。

### 11.3 不在這張卡的

下面這些是傷害規則，不是動畫，開 G6 那一批（或主會話排的肢體傷害批）時再定：

- 手臂的倍率與血量。
- 手臂血量歸零之後的後果。塔科夫類的做法是傷害轉到其他部位（△ 社群說法，未確認）。
- 左右手臂分不分開。
- 軀幹與腿要不要跟著拆成更多部位。
- `LimbHealth` 的形狀、Codec 的版本號、模式開關（照 HANDOFF `v8-arch-gyo2-limb-damage`）。

## 12. 使用者的決定（2026-11-27）

出處：HANDOFF `v8-arch-gyo2-anim-layers-decisions`（`gyo2` d80050c8 的 `HANDOFF.md:419`），由 side chat 轉交主會話、再轉本卡。

**原話**：「改成（b）。」（D5）、「了解了，D1–D4、D6、D7 照舊就好」

| # | 決定 | 和推薦的關係 |
|---|---|---|
| D1 | 第 2 層（每個實體的播放器）不進 Engine，遊戲從 tick 推算 | 照推薦 (a) |
| D2 | two-bone IK 做成 model 的純函式 | 照推薦 (a) |
| D3 | 逐骨權重的混合、additive、`CrossedEvents` 放進 model | 照推薦 (a) |
| D4 | 基本姿勢組＋每把武器的替換表 | 照推薦 (a) |
| D5 | **(b)**：Match 也跑 IK | 照第 11 節改過的推薦（原本推薦 (a)，只在 Client） |
| D6 | Match 用 crossfade 中的姿勢擺判定形狀 | 照推薦 (a) |
| D7 | PV3 之後再做；做時保存擺好的判定形狀（7→11 個） | 照推薦 |
| D8 | 最終產品做肢體傷害，採 (3)：手臂是獨立部位，有自己的倍率和血量 | 另一個決定（`v8-arch-gyo2-limb-damage`），見 0.2 節 |
| D9 | **待定**：手臂精度驗證量什麼、門檻多少（side chat 詢問中） | — |
| D10 | **待定**：同一目標內，手臂擋不擋後面的部位（還沒問；本卡推薦 (a) 擋） | — |

**理由**（使用者與主會話記下的）：

- D5 改 (b)：同日定了肢體傷害 (3)，手臂進判定，Match 要知道手的位置；IK 是純函式，Match 能直接用。
- D1：遊戲已在 `PlayerCombat` 記開始的 tick，rollback 只有一份狀態，Match 推得出來（使用者已理解）。

**照這些決定，第 8 節的批次不必改**：E1、E2、G1～G4、G6 的內容就是 D1～D7 都照推薦、D5 取 (b) 的寫法。G5 等 D9 定了再寫門檻；G6 的同目標規則等 D10。合併 #363 仍要等使用者說「合併」。

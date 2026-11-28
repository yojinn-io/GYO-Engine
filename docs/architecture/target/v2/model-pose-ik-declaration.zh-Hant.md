# model 的姿勢運算與 two-bone IK：事前宣告（`v8-arch-gyo2-model-pose-ops`、`v8-arch-gyo2-model-two-bone-ik`）

任務卡，2026-11-28。這一階段只寫事前宣告，交使用者核准；核准前不寫實作程式。作者 Opus 5.5／high。

出處：研究 #363（`animation-layers-ik-stance-research.zh-Hant.md` 的 R2、R3、第 5、8、11、12 節）；HANDOFF `v8-arch-gyo2-anim-layers-decisions`、`v8-arch-gyo2-limb-damage`、`v8-arch-gyo2-limb-damage-decisions`；`model.zh-Hant.md` 第 4、5 節；現有的 `engine/modules/model/include/engine/model/Animation.hpp` 與 `tests/common/model/AnimationTests.cpp`。

---

## 0. 前提與結論

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| D1：第 2 層（每個實體的播放器）不進 Engine，遊戲從 tick 推算片段時刻 | `v8-arch-gyo2-anim-layers-decisions` |
| D2：two-bone IK 做成 model 的純函式；FABRIK、CCD 不做 | 同上；研究 R3 |
| D3：逐骨權重的混合、additive、`CrossedEvents` 放進 model | 同上；研究 R2 |
| D5 (b)：Match 也跑 IK（手臂是獨立部位，Match 要知道手的位置） | 同上；`v8-arch-gyo2-limb-damage` |
| IK 結果只在 Match 判，不另做跨平台決定性（取樣也一樣，`model.zh-Hant.md` 4.5 節） | 研究 R3 最後一點；RC-MD3 不觸發 |
| model 不依賴 render／GPU／SDL／World，Match 可以用；呼叫者給 span、不保留狀態 | `v8-arch-gyo2-model-now`；MD-3、MD-5 |
| ECS 相關以 Bevy 為主參考，偏離寫衝突點與膠水量 | 批次表〈每批的設計審查〉第 5 項 |

### 0.2 結論

兩批都只改 `engine/modules/model`（`Animation.hpp`／`Animation.cpp`）、它的測試、突變表與文件。**遊戲層不改**：`CharacterRig.cpp:304-315` 的手寫 additive 留到 G2 換成新函式，這一批的行為零變化（K-15）。

- E1：`Blend` 加逐節點權重的多載、新增 `ApplyAdditive`、新增 `CrossedEvents`。
- E2：新增 `TwoBoneChain` 與 `SolveTwoBoneIK`。

需要使用者決定的只有兩點（第 5 節）：U1 一次跨過不只一圈時事件報幾次、U2 IK 要不要也轉末端骨。

---

## 1. 參考實作

查證等級：**源碼**（本機 `~/Code/GitHome`，附檔名與行號）、**官方文件**（只讀網頁，沒有原始碼）。

### 1.1 Bevy（主參考，`bevy` 6a911653f）

| 什麼 | 位置 | 用在哪 |
|---|---|---|
| `Add` 節點：子節點不正規化，各乘權重後依序加上 | `crates/bevy_animation/src/graph.rs:226-238` | additive |
| `Transform` 的 additive：translation `+= w·v`、scale `+= w·v`、rotation `= slerp(I, v, w) * rotation`（**左乘**） | `crates/bevy_animation/src/blendable.rs:142-169`（`:148-153`） | additive（K-3、K-4） |
| additive 的輸入本身是差值（`Add` 節點收的是 clip 取樣的值，Bevy 沒有「相對 reference 算差」） | `blendable.rs:148-153`；`animation_curves.rs:489-504` | K-2 |
| 遮罩：`AnimationMask = u64`，最多 64 組 | `graph.rs:486-489`（研究 1.1 節） | 逐骨權重（K-1） |
| 事件的區間：前進 `last <= t < this`、倒播 `this < t <= last`；跨 loop 時拆成上下兩段；播完時把剩下的全給 | `crates/bevy_animation/src/lib.rs:1455-1528`（`:1482`、`:1484-1487`、`:1493`、`:1496-1498`、`:1505-1524`） | `CrossedEvents`（K-5） |
| 播放頭每 tick 至多跨一圈（`seek_time %= duration`，註解「assumes delta is never lower than -clip_duration」） | `lib.rs:576-610`（`:599-605`） | U1 |
| duration 0 的片段：每 tick 都 `just_completed`、走 ForwardLooping，事件每 tick 觸發 | `lib.rs:587-596`、`:1470-1471` | K-7 |
| IK：Engine 沒有（研究 2.1 節） | — | E2 偏離 Bevy（研究 R3 已寫原因） |

### 1.2 Godot（`godot` 94fc09c）

| 什麼 | 位置 |
|---|---|
| `TwoBoneIK3D::_process_joints`：目標與根重合就不動 | `scene/3d/two_bone_ik_3d.cpp:780-787` |
| 伸不到：沿根→目標的方向伸直 | `two_bone_ik_3d.cpp:789-796` |
| 太近（距離 < \|l1−l2\|）：把目標推到半徑 \|l1−l2\| 的球面上 | `two_bone_ik_3d.cpp:797-805` |
| 中間：兩圓交點，取離 pole 近的那一個；pole 先投影成垂直於根→末端直線的方向 | `two_bone_ik_3d.cpp:807-831`；`skeleton_modifier_3d.cpp:387-398` |
| 骨長取 **rest**（全域 rest 的差） | `two_bone_ik_3d.cpp:620-681`（`:663-681`） |
| 旋轉：rest 前方向量轉到新方向的 swing（最短弧） | `two_bone_ik_3d.h:146-176`（`:163`、`:176`） |
| 另有 pole direction 的 roll 對齊（可關） | `two_bone_ik_3d.h:181-215` |
| 只寫 root 與 mid 的旋轉，末端骨不動 | `two_bone_ik_3d.cpp:840-842` |
| 權重（influence）：modifier 前後的局部姿勢逐骨插值 | `scene/3d/skeleton_3d.cpp:1254-1267` |
| 祖先的旋轉取 `basis.get_rotation_quaternion()`（不看縮放） | `two_bone_ik_3d.h:121-141`、`:150-160` |

### 1.3 Unity、Unreal（**只讀官方文件，沒有讀到原始碼**）

| | 參數（文件列的） | 目標 | pole | 權重 | 伸不到 | 動哪些骨 |
|---|---|---|---|---|---|---|
| Unity Animation Rigging 1.3 Two Bone IK（https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/constraints/TwoBoneIKConstraint.html） | Weight、Root、Mid、Tip、Target、Hint、Maintain Target Offset、Target Position Weight、Target Rotation Weight、Hint Weight | Target 的位置；另有 Target Rotation Weight，1 時 Tip 取 Target 的旋轉 | Hint 是物件，用它的**位置**定彎曲方向 | 四個權重，0～1 之間線性插值 | 只寫「會盡量伸到」，沒寫伸直或拉長 | 沒明寫；Mid 必須是 Root 的子、Tip 是 Mid 的子 |
| Unreal Two Bone IK（https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-two-bone-ik-in-unreal-engine） | IK Bone、Effector Location（＋Space、Target）、Joint Target Location（＋Space、Target）、Allow Stretching、Start Stretch Ratio、Max Stretch Scale、Take Rotation from Effector Space、Maintain Effector Rel Rot、Allow Twist、Twist Axis | Effector 的位置；旋轉只在 bone space 時可取 | Joint Target 是**點** | 這一頁沒有列 Alpha | 只寫可選的拉長 | 鏈＝IK Bone、它的父（joint）、祖父（root） |

**共同點**：目標是位置、pole 是點、三根骨（root、mid、tip）。末端骨的旋轉：Godot 不動，Unity、Unreal 各有一個選項（U2）。

---

## 2. 公開介面（全部加在 `Animation.hpp`，namespace `GYO::Engine::Model`）

```cpp
// ---- E1 ----

// Per node: out[i] = a[i] mixed toward b[i] by weights[i], as the Blend above
// (translation and scale linearly, rotation by shortest-path slerp; weights
// not clamped). A mask is weights of 0 and 1. out may be a or b.
void Blend(std::span<const LocalTransform> a, std::span<const LocalTransform> b,
           std::span<const float> weights, std::span<LocalTransform> out);

// out[i] = base[i] plus weight times the motion of motion[i] relative to
// reference[i]: translation base + w (motion - reference); scale
// base + w (motion - reference); rotation base * slerp(identity,
// inverse(reference) * motion, w), the change in the node's own frame.
// weight 1 with base == reference gives motion. out may be any input.
void ApplyAdditive(std::span<const LocalTransform> base, std::span<const LocalTransform> motion,
                   std::span<const LocalTransform> reference, float weight,
                   std::span<LocalTransform> out);

// The instants of a clip crossed when its time moves from `from` to `to`
// (seconds, the same times SampleClip takes). times: ascending, within
// [0, duration]. Writes the indices into out in the order they are crossed
// and returns how many; each index at most once (U1). out.size() must be at
// least times.size().
std::size_t CrossedEvents(std::span<const float> times, float from, float to, float duration,
                          Wrap wrap, std::span<std::uint32_t> out);

// ---- E2 ----

// Three nodes of one chain: mid below root, tip below mid (not necessarily
// direct children).
struct TwoBoneChain final {
    std::uint32_t root = 0;
    std::uint32_t mid = 0;
    std::uint32_t tip = 0;
};

// Turns root and mid in pose so tip reaches target, bending toward pole
// (both points in model space, the space of ComputeModelSpace). Only the
// rotations of root and mid change. weight 0 leaves pose as it is, 1 is the
// solved pose, between them each of the two rotations by slerp.
void SolveTwoBoneIK(std::span<const Node> nodes, const TwoBoneChain& chain, Math::Vec3 target,
                    Math::Vec3 pole, float weight, std::span<LocalTransform> pose);
```

Programmer Error（assert，和現有函式同一套）：span 大小不合、`out` 小於 `times`、`from`／`to`／`duration`／`weight`／`target`／`pole` 不是有限值、`duration` < 0、chain 的索引超出範圍或不是祖先關係。`times` 的遞增與範圍是呼叫者的資料規則（遊戲在讀 `gyo.anim_events` 時檢查），每次呼叫不檢查（K-6）。

---

## 3. 語意

### 3.1 逐節點的 `Blend`

和單一權重的 `Blend` 同一條式子，權重換成 `weights[i]`。單一權重的 `Blend` 等於每個節點同一個權重（測試 T4 比位元）。實作共用一個逐節點的內部函式。

### 3.2 `ApplyAdditive`

```text
delta_r       = inverse(reference.rotation) * motion.rotation
out.rotation  = base.rotation * slerp(identity, delta_r, w)
out.translation = base.translation + w (motion.translation − reference.translation)
out.scale       = base.scale       + w (motion.scale       − reference.scale)
```

rotation 都先 `NormalizeOrIdentity`（同 `CharacterRig.cpp:311-312`）。上下半身的用法：`ApplyAdditive(aim, action, action 的第 0 幀, 1, upper)`，再 `Blend(lower, upper, mask, out)`。

### 3.3 `CrossedEvents`

以 `from < to` 為前進、`from > to` 為倒播、相等時沒有事件。D＝`duration`。

| | 前進 | 倒播 |
|---|---|---|
| `Clamp` | f＝夾到 [0, D] 的 from，g＝夾住的 to：`f <= t < g`；另外 **g＝D 且 f < D 時，t＝D 也算**（走到底的那一次） | `g < t <= f`；另外 **g＝0 且 f > 0 時，t＝0 也算** |
| `Loop`，跨距 < D | p＝from 對 D 取餘數（同 `SampleClip` 的 `WrapTime`），q＝p＋跨距：先 `p <= t < q`，q > D 時再 `t < q − D` | p 同上，q＝p − 跨距：先 `q < t <= p`（遞減），q < 0 時再 `t > q + D`（遞減） |
| `Loop`，跨距 >= D | 每個事件一次，從 p 開始：`t >= p` 遞增，再 `t < p` 遞增（U1 (A)） | `t <= p` 遞減，再 `t > p` 遞減 |
| D＝0 | 沒有事件（K-7） | 同左 |

連續的區間 [a, b)、[b, c) 不重複、不漏（測試 T11）；在 `Loop` 裡 t＝0 與 t＝D 是同一個時刻，跨圈時兩個都報（同 Bevy `lib.rs:1505-1513`）。

### 3.4 `SolveTwoBoneIK`

1. weight＝0：直接返回，pose 逐位元不變（Match 沒有握點時走這條）。
2. 由 `nodes` 與 `pose` 沿祖先算 root、mid、tip 的模型空間位置（矩陣，同 `ComputeModelSpace`）與旋轉（四元數連乘、不看縮放；同 Godot `get_rotation_quaternion`，K-12）。只走這條鏈的祖先，不算整副。
3. 骨長 a＝|mid − root|、b＝|tip − mid|，取**目前的姿勢**（K-9）。a 或 b 近 0：不動（鏈退化）。
4. d＝target − root。|d| 近 0：不動（Godot `:785-787`）。
5. 新的 mid'、tip'：
   - |d| >= a + b（**伸不到**）：沿 d 伸直，mid'＝root＋â·a、tip'＝root＋â·(a+b)（Godot `:789-796`）。
   - |d| < |a − b|（**目標在身體裡**、太近）：目標推到沿 d、距離 |a − b| 的點，鏈完全折起（Godot `:797-805`）。
   - 其他：兩圓交點，彎向 pole 的那一側：pole 投影到垂直 d 的方向 n（Godot `skeleton_modifier_3d.cpp:387-398`）。**pole 落在根→目標的直線上**時 n 改取目前 mid 的那一側；目前的鏈也是直的，就取與 d 垂直的固定方向（K-11）。
6. 旋轉（最短弧，同 Godot 的 swing，K-10）：S1 把 (mid − root) 轉到 (mid' − root)，root 的局部旋轉＝inverse(Q_root 的父) · S1 · Q_root；S2 把 S1·(tip − mid) 轉到 (tip' − mid')，mid 的局部旋轉同理（父的旋轉已含 S1）。平移、縮放、其他節點不動，所以骨長不變。
7. 0 < weight：root、mid 的局部旋轉各自 `slerp(原本, 解出的, weight)`（Godot `skeleton_3d.cpp:1267`、Unity 文件「線性插值」）。不夾住（同 `Blend`）。

---

## 4. 卡片自己判的（不是使用者的話；可以推翻）

| # | 判的 | 根據 | 比過的 |
|---|---|---|---|
| K-1 | 逐骨權重用每節點一個 float | 研究 R2 已寫：沒有圖可以掛 mask；也能表達 Unreal Blend Depth 那種漸變。**偏離 Bevy**：Bevy 是 u64 mask group（`graph.rs:486-489`），衝突點是「組」的概念；膠水：呼叫者從節點的組算 0／1 的權重，一個迴圈 | Bevy 的 u64 mask |
| K-2 | additive 收 (motion, reference) 現場算差，不收差值姿勢 | 遊戲與 master 的用法就是「動作片段相對第 0 幀」（`CharacterRig.cpp:304-315`）；model 沒有「差值片段」的資料。**偏離 Bevy**：Bevy 的 Add 收已經是差值的值（`blendable.rs:148-153`）；照 Bevy 的話呼叫者要多一趟算差，或 loader 烘差值片段 | `ApplyAdditive(base, difference, w)`（Bevy 形）；另加 `Difference(motion, reference, out)` |
| K-3 | rotation 的差在節點自己的框架：`base * delta`（右乘） | 這是遊戲與 master 的式子（`CharacterRig.cpp:310-314`；它的註解 `:280-282` 寫明照 master 的 `AddMotion`，master 本身這次沒有重讀）；換成左乘會改變現有的上半身動作。**偏離 Bevy**：Bevy 左乘（`blendable.rs:152`，差在父的框架）。base＝reference 時兩者相同，base 與 reference 不同時（瞄準＋開槍）結果不同。膠水：無（遊戲沿用） | 照 Bevy 左乘；做成參數 |
| K-4 | scale 用線性差 `base + w (motion − reference)` | 和 Bevy 的加法同一種算術（`blendable.rs:150`）；遊戲現在保留 base 的 scale，片段不動 scale 時結果相同 | 比例 `base · (motion / reference)`（Retarget 的做法，要除以 0 的規則）；保留 base（遊戲現況） |
| K-5 | `CrossedEvents` 收未取餘的時間（和 `SampleClip` 同一組秒數），不收 Bevy 的 seek＋`just_completed` | D1：沒有播放器狀態，時間是 tick 推出來的。區間的開閉與跨圈的拆法照 Bevy（`lib.rs:1482-1524`）；Clamp 走到底時補 t＝D 照 Bevy 的「播完給剩下的」（`:1484-1487`） | 照 Bevy 收 (last, this, looped, reversed) |
| K-6 | 輸出寫進呼叫者的 span、回數量；`times` 是遞增的 float span，每次不驗 | model 現有函式都寫進呼叫者的 span、不配置；U1 (A) 讓數量 <= `times.size()`，呼叫者一個固定陣列就夠。遞增是資料規則，同 key 的順序由 `Validate` 管（MD-2） | 回 `std::vector`；callback；每次驗遞增 |
| K-7 | duration 0 沒有事件 | 長度 0 沒有可以「穿過」的區間；`SampleClip` 也把它當成永遠在 0。**偏離 Bevy**：Bevy 在這種片段每 tick 觸發（`lib.rs:587-596`），是 `just_completed` 的副作用，不是刻意的規則 | 照 Bevy 每次都報；當成 Clamp 在 0 報一次 |
| K-8 | IK 自己沿祖先算需要的位置與旋轉，不收模型空間矩陣 | 研究 R3 寫「給模型空間矩陣」；但那份矩陣要在 IK 之前另算一次整副，且和 pose 不同步時是看不出的錯。只走這條鏈的祖先（手：約 6 個節點）。呼叫者 IK 之後照常 `ComputeModelSpace` 一次 | 收 `std::span<const Math::Matrix4>`（研究 R3 的形） |
| K-9 | 骨長取目前的姿勢 | 呼叫者給的就是要畫、要判的姿勢；片段若動了平移，rest 的長度會讓末端偏掉。**偏離 Godot**：Godot 取 rest（`two_bone_ik_3d.cpp:663-681`），它還要處理 virtual end 的設定。人形的手臂片段不動平移時兩者相同 | 取 rest（Godot） |
| K-10 | 旋轉取最短弧，不做 roll 對齊 | 和 Godot 關掉 pole direction 時相同（`two_bone_ik_3d.h:163,176`）；上臂的扭轉保留片段的。roll 對齊要每根骨一個「哪個軸朝 pole」的資料，現在沒有需求 | Godot 的 roll 對齊（`:181-215`） |
| K-11 | pole 在直線上時取目前 mid 那一側；鏈也是直的就取固定的垂直方向 | **偏離 Godot**：Godot 的投影方向為 0 時，mid 落在直線上、骨長不保（`two_bone_ik_3d.cpp:814-831` 直接用 0 向量）。Match 的握點與 pole 都是資料，直線不是不可能 | 照 Godot；當成 Programmer Error |
| K-12 | 祖先的旋轉用四元數連乘，不看縮放 | 同 Godot（`two_bone_ik_3d.h:121-141`）；縮放均勻時是精確的；PvP 的骨架縮放均勻（Retarget 的規則 3 也要求參考姿勢均勻） | 從矩陣分解旋轉（非均勻縮放時也沒有單一解） |
| K-13 | 權重＝兩根骨的局部旋轉各自 slerp；0 直接返回 | Godot influence（`skeleton_3d.cpp:1267`）、Unity 文件同義；0 逐位元不變，Match 在不開 IK 的模式下結果和現在相同 | 夾住到 [0, 1]；位置插值後再解 |
| K-14 | 名字 `SolveTwoBoneIK`：縮寫全大寫 | `rebuild-naming.zh-Hant.md` N17 的縮寫規則（不准一大一小）。**`ik`→`IK` 由本卡在 E2 的 commit 寫進縮寫表**（`rebuild-naming.zh-Hant.md` 第 65 行，括號附批次代號；先例：`sdl`、`spsc` 是 `v8-arch-gyo2-sdl-a`、`gpu` 是 `v8-arch-gyo2-sdl-r2a` 在各自的 PR 加的；主會話 2026-11-28 指示）。`ik` 不出現在目錄名，守衛的 `ACRONYMS` 不改；函式名由審查看 | `SolveTwoBoneIk` |
| K-15 | 遊戲層不改 | E1、E2 是 Engine 批；換用新函式在 G2（`ComposeBody`）、G3（IK）、G4（事件）。這一批 PvP 的姿勢、權威 digest 都不變 | 這一批順手改 `CharacterRig` |
| K-16 | 最短弧的四元數（from→to）放在 `Animation.cpp` 的匿名 namespace，不加進 core Math | 只有這裡用；core 的公開介面多一個函式要它自己的測試與審查。有第二個使用者再搬 | 加 `Math::MakeQuaternionFromTo` |

---

## 5. 需要使用者決定的點

### U1：一次跨過不只一圈時，事件報幾次？

- **機制**：`Loop` 的片段，`to − from >= duration`（例：0.4 s 的腳步循環，Client 卡了 0.5 s）。
- **(A) 每個事件至多一次**（推薦）：照跨越的順序報一次。照 Bevy：播放頭每 tick 至多跨一圈（`lib.rs:599-605` 的註解），結果至多兩段（`lib.rs:1505-1524`）。數量有上限（<= 事件數），呼叫者用固定陣列。代價：卡頓時腳步聲少報（2 次變 1 次）；反過來說，卡完不會一口氣疊好幾個聲音。
- **(B) 每圈都報**：數學上精確。數量沒有上限，要 callback 或 vector；卡頓時聲音疊在一起。和 Bevy 不同。
- **證據**：Bevy `crates/bevy_animation/src/lib.rs:576-610`、`:1455-1528`；遊戲的用法 `ClientWorld.cpp:890`（現在一幀只判一個事件）。

### U2：IK 要不要也轉末端骨（手）？

- **機制**：`SolveTwoBoneIK` 只轉 root、mid（上臂、前臂）；手的方向沿用片段的局部旋轉。
- **(a) 不轉**（推薦）：照 Godot（`two_bone_ik_3d.cpp:840-842`）。命中形狀只看手的位置（`lowerarm→hand` 膠囊，研究 11.2 節），Match 用不到手的方向。G3 要讓手對齊護木時，呼叫者在 IK 之後設手的局部旋轉，約 5 行。沒觀測到需求就不擴大。
- **(b) 加可選的末端旋轉＋權重**：Unity 有 Target Rotation Weight、Unreal 有 Take Rotation from Effector Space（**只讀文件**）。介面多兩個參數（目標旋轉、它的權重）。
- **證據**：第 1.2、1.3 節的表。

---

## 6. 測試清單（外部測試，`tests/common/model/AnimationTests.cpp`）

全部經公開介面、用測試裡寫的節點表與姿勢，每案寫出預期的值。既有的測試不改。

### 6.1 E1

| # | 案 |
|---|---|
| T1 | 逐節點 `Blend`：三個節點各用 0、1、0.25，各自得到 a、b、混合值 |
| T2 | 逐節點 `Blend`：out 可以是 a 或 b |
| T3 | 逐節點 `Blend`：`weights` 大小不合是 Programmer Error |
| T4 | 逐節點 `Blend`：每個節點同一個權重時，和單一權重的 `Blend` 逐位元相同 |
| T5 | `ApplyAdditive`：base＝reference、weight 1 得到 motion（三個分量） |
| T6 | `ApplyAdditive`：motion＝reference 時 base 不變（任何 weight） |
| T7 | `ApplyAdditive`：rotation 的差在節點自己的框架：base 繞 Y 90°、差是繞 X 90°，結果是 base·delta，不是 delta·base |
| T8 | `ApplyAdditive`：weight 0.5 是一半的差（平移一半、旋轉半角、縮放一半） |
| T9 | `ApplyAdditive`：out 可以是 base；大小不合是 Programmer Error |
| T10 | `CrossedEvents` 前進：from 上的事件算、to 上的不算；切成連續的幾段，每個事件剛好一次 |
| T11 | `CrossedEvents` 倒播：`(to, from]`，遞減的順序 |
| T12 | `Loop` 前進跨過尾端：先尾段、後頭段；from、to 是未取餘的時間（例 2.9→3.1、負的時間） |
| T13 | `Loop` 倒播跨過開頭 |
| T14 | `Loop` 跨距 >= duration：每個事件一次，從 from 的相位開始（U1 (A)；選 (B) 時改成每圈都報） |
| T15 | `Loop` 的 t＝0 與 t＝D 跨圈時都報 |
| T16 | `Clamp`：走到 D（to＝D 與 to > D 兩種）時報 t＝D，下一段不再報；D 之後、0 之前不重複 |
| T17 | `Clamp` 倒播：走到 0 時報 t＝0 |
| T18 | from＝to、duration 0：沒有事件 |
| T19 | Programmer Error：out 小於 times、NaN 的 from／to、負的 duration |

### 6.2 E2（含 Match 會碰到的情形）

| # | 案 | Match 的情形 |
|---|---|---|
| I1 | 伸得到：tip 落在 target（誤差 1e-4）、兩段骨長不變、平移不變、鏈外的節點不變 | 一般的握點 |
| I2 | pole 決定彎的方向：相反的兩個 pole，mid 各在 pole 那一側，且 root、mid、target、pole 共面 | 肘朝外 |
| I3 | **伸不到**：鏈沿 root→target 伸直，tip 在 root＋â·(a+b)、mid 在直線上 | 握點離肩膀太遠（武器資料錯、換組 crossfade 中） |
| I4 | 剛好伸到（|d|＝a＋b）：與伸直的結果連續 | 邊界 |
| I5 | **目標在身體裡**（|d| < |a−b|）：tip 在沿 d、距離 |a−b| 的點，結果有限 | 握點在軀幹內、在肩膀上 |
| I6 | 目標就在 root：pose 不變 | 退化 |
| I7 | **權重 0**：pose 逐位元不變 | 沒有握點／模式不開 IK |
| I8 | 權重 0.5：root、mid 的局部旋轉＝原本與權重 1 的解的 slerp（逐位元同 `Math::Slerp`） | 開關 IK 的過渡 |
| I9 | pole 在直線上：彎向目前 mid 的一側；鏈是直的時結果有限、tip 仍落在 target | 資料造成的退化 |
| I10 | 祖先有旋轉與平移、root 與 mid 之間隔一個節點：tip 在模型空間落在 target | 肩膀在 spine 下、twist 骨 |
| I11 | 骨長 0：pose 不變 | 壞的骨架 |
| I12 | Programmer Error：chain 索引超出、mid 不在 root 之下、tip 不在 mid 之下、pose 大小不合、非有限的 target／pole／weight | — |

---

## 7. 相關突變（D184 的範圍）

只跑：新的、`find` 在這次改動範圍內的、殺它的測試被改了的。

- **新增**（`tests/common/model/mutations.json`，id 用 `ma-`）：
  - E1：逐節點權重都取第 0 個、權重反過來（1 − w）；additive 左乘、忽略 weight、忽略 reference（差＝motion）、scale 保留 base；事件前進改成閉區間、倒播的順序反了、Loop 當 Clamp、跨尾端的第二段沒有、Clamp 走到底不補 t＝D、Clamp 走到底後下一段又補一次、跨距 >= D 的起點不看相位。
  - E2：伸不到的分支拿掉、太近時不推出、pole 的側邊反了、pole 在直線上不退回、權重被忽略、權重 0 不提前返回（I7 的逐位元檢查殺它）、root 的父旋轉被忽略、mid 沒有帶上 S1、骨長取成 rest。
  - 每個寫 `expect`（正規式）對到殺它的那一行檢查；實作時若某個寫不出不 UB 的 find／replace，在描述的「不加的」裡寫原因（照檔頭現有的寫法）。
- **既有、要重跑的**：`ma-blend-rotation-kept`（`Blend` 改成共用內部函式，`find` 會照新程式改寫）。其他 `ma-*` 的 `find` 不在改動範圍、殺它們的測試沒改，不跑。
- 實際跑哪幾個、結果，寫進 dev_log。

---

## 8. `model.zh-Hant.md` 第 4、5 節的改法

- 檔頭：加一行更新（兩個批次代號、這份宣告、使用者決定的出處）。
- **4.2 混合**：加逐節點的 `Blend` 與 `ApplyAdditive`（第 3.1、3.2 節的式子、K-1～K-4 的 Bevy 對照）；刪掉「分層、additive 是遊戲的規則，可以用這個與逐節點的覆寫組出來」，改成「哪些節點算上半身、權重多少、何時加 additive 是遊戲的規則；混合本身在這裡」。
- **新 4.6 事件：`CrossedEvents`**：第 3.3 節的表、U1 的決定、K-5～K-7；事件資料（`gyo.anim_events`）的讀法與事件→聲音仍是遊戲的。
- **`rebuild-naming.zh-Hant.md` 第 65 行**（E2）：縮寫表加 `ik`→`IK`（K-14）。
- **新 4.7 IK：`SolveTwoBoneIK`**：第 3.4 節的步驟、U2 的決定、K-8～K-13；Godot 的對照；Match 也跑（D5 (b)）。
- **4.5 跨平台**：加一句 IK 用 `sqrt`、`acos`（在 `Slerp` 裡），跨平台同樣不保證逐位元；IK 只在 Match 判（研究 R3），所以沒有新的 RC。
- **第 5 節**：刪掉「動畫事件：這張卡沒做……列在後續」那一條；「命中形狀、何時切片段、分層、additive、瞄準、附掛：遊戲規則」改成「命中形狀、何時切片段、哪些骨是哪一層、權重與時機、瞄準、附掛、握點、IK 的鏈與何時開：遊戲規則」；加「FABRIK、CCD、roll 對齊、末端旋轉（U2 選 (a) 時）：沒有點名的需求，不做」。
- **Design Invariants MD-3**：函式清單加上新的四個，守衛加 T1～T19、I1～I12。第 6 節的表加 K-1～K-16（或指到這份宣告）。

---

## 9. 不做的

- 遊戲層（`CharacterRig`、`ClientWorld` 的事件、左手 IK 的套用）：G2、G3、G4。
- FABRIK、CCD、roll 對齊、拉長（Unreal Allow Stretching）：沒有點名的需求。
- 跨平台決定性的 IK（研究 R3、R5 (e)）。
- core Math 的新函式（K-16）。

---

## 10. 步驟、驗收、模型／檔位

| 步 | 內容 | 上段／下段 | 實作／審查／檢證（模型／檔位） |
|---|---|---|---|
| E1 | 第 2、3.1～3.3 節的程式、T1～T19、E1 的突變、`model.zh-Hant.md` 4.2、4.6、第 5 節、dev_log（同一個 commit） | 上段（公開介面） | 實作 Opus／high（本卡）；檢證 Opus／high |
| E2 | 第 2、3.4 節的程式、I1～I12、E2 的突變、`model.zh-Hant.md` 4.5、4.7、MD-3、dev_log（同一個 commit） | 上段（公開介面） | 實作 Opus／high（本卡）；檢證 Opus／high |
| 收尾 | 純粹的 Code Review（`.claude/agents/pure-code-reviewer.md`，prompt 只給 git 範圍）；處置後 dispatch 三平台 L1 | — | Fable／high（沒有 Fable 時 Opus，寫在報告） |

- 本機（macOS arm64）：`gyo_model_animation_tests`、`gyo_model_fbx_tests`、用到 model 的 `object_fps_pvp` 測試全過；相關突變全部 killed。
- 檢證者：拿實際的輸出確認第 3 節的每一條（尤其 I3、I5、I7 與 U1 的結果）。
- L1：三平台（linux-x64、windows-x64、macos-arm64）。不需要 GPU 或重的量測。
- PR 不合併；等使用者說「合併」。

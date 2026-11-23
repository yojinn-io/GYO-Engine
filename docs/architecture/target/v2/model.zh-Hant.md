# 模型 Module（`engine/modules/model`）

更新：2026-11-24（`v8-arch-gyo2-model`：新建；照使用者 2026-11-23 的兩個決定——model 現在就開、做成不依賴 render／GPU／SDL 的函式庫（HANDOFF `v8-arch-gyo2-model-now`），骨骼動畫第 1 層放 model、第 2 層先不做、第 3 層歸 render（HANDOFF `v8-arch-gyo2-skeletal-animation-layers`）；ufbx 走 Module 這一側（`v8-arch-gyo2-sdl-q2-ufbx-stb-module`）。需求來源：PvP 移植盤點 `games/object_fps_pvp/docs/porting-inventory.zh-Hant.md` 第 4 節。新 AGENTS（#296）下不另寫事前宣告；卡片自己判的取捨在第 6 節，**最終文字在合併前核准時定稿**）。

## 一個例子

一個 PvP 的 Match（伺服器，沒有視窗）開局：catalog 裡兩個 `type` 為 `model` 的項目——身體（`settings: {"meshes": false}`，只要骨架）與片段來源（同樣不要網格）。Match 經 asset 請求 `Handle<ModelData>`；worker 讀檔、`FbxLoader` 經 ufbx 解成 `ModelData`。遊戲用 `BindByName` 把片段來源的骨頭對上身體的骨頭、拿掉不要的、以 `Retarget` 把要用的片段搬到身體上（載入時一次）。之後每個 tick，遊戲自己算片段時刻與權重，`SampleClip` 取兩個姿勢、`Blend` 混合、`ComputeModelSpace` 得到每根骨頭在模型空間的矩陣，再把命中判定用的形狀擺上去。Client 用同一份資料畫角色：網格與蒙皮資料交給 render（R2b），貼圖由產品照材質名或 `baseColorTexture` 對到 catalog 的 `image` 項目。整個 model 不需要 SDL、GPU 或 World。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math ◄── GYO::Model ──► GYO::Asset（loader 介面）
                            │  └─(私有)─► GYO::Json（settings）
                            └─(私有)─► gyo_ufbx（只在 src/adapters/ufbx/）
```

- 函式庫（`library_only`、`default=no`），沒有 `ModelModule`、不進 World：連結恰好是 `GYO::Base`、`GYO::Math`、`GYO::Asset`、`GYO::Json`、`gyo_ufbx`（MD-5）。不認得 SDL、渲染、GPU、World、產品。
- 數學型別用 core 的 `GYO::Math`（`Vec3`、`Quaternion`、`Matrix4`），不依賴 transform Module（它連 Kernel、是 World 的登記單位）；姿勢用自己的 `LocalTransform`（和 transform 的 `Transform` 同一組欄位，沒有 hash 與回滾的語意）。
- loader（`FbxLoader`）由產品登記：在 `Configure` 經 `AssetLoaderRegistry::Register<FbxLoader>()`，同 image 的 `PngLoader`、audio 的 `WavLoader`。
- 一個 Module、不拆 `mesh`／`animation`：使用者定了第 1 層放 model 函式庫（`-skeletal-animation-layers`）。Bevy 拆成 `bevy_mesh`、`bevy_animation`、`bevy_gltf` 是因為網格與動畫各自是 ECS 的元件與系統（`bevy_mesh/src/skinning.rs:16` 的 `SkinnedMesh` 是 Component、`bevy_animation/src/lib.rs:748` 的 `AnimationPlayer` 是 Component）；這裡沒有 World 的部分，拆開只多一層依賴。真的出現「只要網格、不要動畫」的使用者（或第 2 層 `AnimationModule`）時再拆。

## 2. 資料（`ModelData`）

### 2.1 型別

一個檔載成一個 `ModelData`，擁有全部的值（照 Bevy 的 `Gltf` 資產，`bevy_gltf/src/assets.rs:18` 收場景、網格、材質、節點、蒙皮、動畫；不同處見 M-3）：

| 型別 | 內容 |
|---|---|
| `Node` | `name`、`parent`（**在它之前的節點**，或 `kNoParent`）、`rest`（`LocalTransform`：檔案裡沒有動畫時的姿勢）。骨架就是節點表本身：片段與蒙皮都用節點的索引 |
| `Material` | `name`、`baseColor`（linear RGB＋straight alpha，沒有就白）、`baseColorTexture`（檔裡寫的貼圖檔名，沒有就空）。只是資料：不開檔、不解碼 |
| `Mesh` | 一個材質的三角形表：`node`、`material`（或 `kNoMaterial`）、`positions`、`normals`、`uvs`（空或每頂點一個；(0, 0) 是貼圖左上角，和 image IM-2 一致）、`indices`（每三角形三個）、`skin` |
| `Skin` | `joints`（節點索引）、`inverseBindMatrices`（每 joint 一個：bind 時網格空間→joint 空間）、每頂點 4 個 `vertexJoints`（`uint16`，指 `joints`）與 `vertexWeights`（由大到小，和為 1 或全為 0）。照 Bevy 的 `SkinnedMesh`＋`SkinnedMeshInverseBindposes`（`skinning.rs:16,34`）與 `Uint16x4`／`Float32x4` 屬性（`bevy_mesh/src/mesh.rs:416-424`） |
| `AnimationClip` | `name`、`duration`（秒）、`nodeCount`（為哪個大小的節點表做的）、`tracks`（每節點至多一條） |
| `Track` | `node` 與三個 channel：`translation`、`rotation`、`scale`，各是 `Keys`（`times` 嚴格遞增、在 [0, duration] 內，`values` 一對一）。空的 channel＝那個節點的 rest 值 |

**模型空間**：照 Math 的慣例——公尺、+Y 上、+Z 前、左手系、column vector（`Math::Multiply(parent, local)`）。

**蒙皮後的位置**（第 3 層的 render 照這個算）：模型空間的位置＝Σ（4 個影響）weight × global(joint) × inverseBind(joint) × position。沒有蒙皮的網格放在它的節點上（global(node) × position）。

`ModelData` 是不可變的值：任何執行緒都能讀、在任何執行緒解構（asset AS-10 (c)）。

### 2.2 規則（`Validate`）

loader 回的每個 `ModelData` 都守這些規則；工具或測試手做的先用 `Validate` 查，`Retarget` 做出的片段用 `ValidateClip` 查。先查節點，再逐個網格（網格的規則、再它的蒙皮），最後查片段；回第一條違反的碼（`ModelDataCode`）：

| 碼 | 規則 |
|---|---|
| `BadNode` | 節點的 parent 在它之前（或沒有）；rest 值有限 |
| `BadMesh` | 網格的節點在範圍內；材質在範圍內或 `kNoMaterial`；`normals` 每頂點一個、`uvs` 空或每頂點一個；索引數是 3 的倍數、每個在範圍內；頂點值有限 |
| `BadSkin` | 沒有 joint 就沒有其他蒙皮資料；joint 在範圍內；inverse bind 每 joint 一個、有限；`vertexJoints`／`vertexWeights` 每頂點一個；指的 joint 在 `joints` 裡；權重有限、不為負、和為 1（容許 1e-4）或全為 0 |
| `BadClip` | `nodeCount` 等於節點數；track 的節點在範圍內、不重複；channel 的 times 與 values 一樣多、times 有限、嚴格遞增、在 [0, duration] 內；值與 duration 有限、duration 不為負 |

「父在子之前」照 Jolt（`Skeleton.h:57`：多數演算法要求父關節在子之前）；ozz-animation 也這樣要求。好處是 `ComputeModelSpace` 一趟由前往後就好。

## 3. 模型的 Data Contract（`model`／`fbx`）

**誰產生、誰讀**：任何能輸出 FBX 的軟體（Blender、Maya、Script），一樣有效；遊戲經 asset 讀原始檔。工具要驗證自己的輸出就呼叫同一個 loader（只連 `GYO::Model`）。

**catalog 的項目**：

```json
{"id": "game.hero", "type": "model", "path": "characters/hero.fbx"}
{"id": "game.hero.skeleton", "type": "model", "path": "characters/hero.fbx", "settings": {"meshes": false}}
```

- `type`：`model`。`format`：`fbx`（副檔名推得出，可省）。
- `settings`：可以不寫（＝`meshes: true`）。寫的話只能有 `meshes`，值是 `true` 或 `false`；`false` 不讀幾何（`meshes` 空，節點、材質、片段照樣），給只要骨架與動畫的使用者（Match）。其他成員、其他值都是 `BadSettings`。不認得的成員不忽略：之後加設定是放寬（加版本）。

**大小**（不是 loader 的規則）：預設上限 64 MiB，產品可用 asset 的 `byteLimits` 依（`model`、`fbx`）覆寫；超過是 asset 的 `TooLarge`。ufbx 本身的工作記憶體與結果各限 256 MiB（超過是 `Undecodable`）。

**檔案格式（`fbx` 第 1 版）**：FBX（binary 與 ASCII，ufbx 收的版本）。loader 照下表判，回第一條違反的碼（`FbxLoaderCode`）：

| # | 規則 | 碼 |
|---|---|---|
| 1 | 是 FBX 檔（空的、或 ufbx 認不出格式的不是） | `NotFbx` |
| 2 | ufbx 讀得完（資料完整、在記憶體上限內）、每個動畫 stack 烘得出 key | `Undecodable` |
| 3 | 每個網格至多一個 skin；skin 至多 65536 個 joint；網格少於 2^32 個頂點 | `Unsupported` |
| 4 | 結果守第 2.2 節的規則（例：沒有 NaN） | `BadData` |

**怎麼轉成 `ModelData`**（都在 adapter 裡，ufbx 的選項見 `UfbxImport.cpp` 的 `LoadOptions`）：

- **空間**：轉到 Math 的模型空間（公尺、+Y 上、左手系，鏡像軸 Z），換算放進幾何與變換，根節點不帶換算。幾何變換（FBX 的 geometric transform）放進頂點；非「parent × local」的繼承（例：Maya 的 segment scale compensate）由 ufbx 加 helper 節點表示，所以 `ComputeModelSpace` 的單純連乘就是對的。
- **節點**：從 ufbx 的根節點（名字空）深度優先、子節點照檔案順序，所以父在子之前。helper 節點也是節點。
- **材質**：`baseColor` 取 FBX 的 diffuse color 乘 diffuse factor（RGB），alpha 照原樣；`baseColorTexture` 取那張貼圖的 relative filename（沒有就 absolute filename）。**不宣告 reference**：檔裡的路徑常是作者機器上的路徑、也常和 catalog 的格式不同（交付 JPG、catalog 裡是 PNG），宣告了反而讓模型載入失敗；對到 catalog 的 id 是產品的事。
- **網格**：每個材質分區一個 `Mesh`（名字是節點名）；多邊形照 ufbx 三角化；完全相同的頂點（位置、法線、UV、joint、權重的位元組）合併（`ufbx_generate_indices`）。法線缺的由 ufbx 產生。UV 的 v 翻成 `1 − v`（FBX 的原點在左下）。
- **蒙皮**：每頂點取權重最大的 4 個（同權重時 joint 小的先），再縮放成和為 1；沒有正權重的頂點四個都是 0。inverse bind＝ufbx 的 `geometry_to_bone`。dual-quaternion 蒙皮照 linear 讀、blend shape（morph）與 cache deformer 不讀（基本形照常）——見 M-7。
- **片段**：每個 animation stack 一個片段（名字是 stack 名），`ufbx_bake_anim` 以每秒 60 個 key 烘（`kBakeRate`；ufbx 保留原本就是 24／30 Hz 的 key），開頭對齊 0。key 時間轉成 float 後若相同，留後面那個（一個 step 烘成的兩個 key）。`duration`＝stack 的播放長度與最後一個 key 的較大者。

- **Version**：FBX 沒有 GYO 的版本欄，版本記在這一節（今天第 1 版）。放寬（多收：例 morph、多一個設定）時版本加一、`format` 名不變；收窄或改變已收內容的意思，用新的 `format` 名登記另一個 loader（AS-9）。同 image、audio。
- **範例**：`tests/common/model/contract/skinned_bar.fbx`（`make_skinned_bar.py` 用 Blender 4.5 從零做的：兩根骨頭、蒙皮的長方柱、一個材質、兩個片段，場景寫在腳本開頭）。PvP 的 4 個 FBX 在本機載過一次（dev_log），**不進 Engine 的測試**：Engine 不讀產品的資產（AGENTS §1-8）。

## 4. 骨骼動畫第 1 層（`Animation.hpp`、`Retarget.hpp`）

都是純函式：沒有時鐘、沒有 World、呼叫之間不留狀態。「播哪個片段、播到哪裡、權重多少」是呼叫者的（第 2 層先不做，`-skeletal-animation-layers`）。輸出是呼叫者給的 span、每節點一個；大小不合、或拿別的節點數做的片段來取樣，是 Programmer Error（assert）。

### 4.1 取樣：`RestPose`、`SampleClip`

- `RestPose(nodes, out)`：每個節點的 rest。
- `SampleClip(nodes, clip, seconds, wrap, out)`：先填 rest，再逐 track 取樣。channel 在第一個 key 之前是第一個 key 的值、最後一個之後是最後的值；兩個 key 之間 translation 與 scale 線性、rotation 走最短路徑的 slerp。key 用二分搜尋。照 Jolt（`SkeletalAnimation.cpp:62-110`：二分搜尋、首尾夾住、translation 線性、rotation SLERP）與 Bevy（`bevy_animation/src/blendable.rs:172-179`：`Quat` 用 slerp）。
- `Wrap::Clamp`：時間夾到 [0, duration]。`Wrap::Loop`：對 duration 取餘數（負的從尾端倒數；剛好等於 duration 就是 0）；duration 為 0 的片段永遠在 0。NaN 的時間是 Programmer Error。

### 4.2 混合：`Blend`

`Blend(a, b, weight, out)`：逐節點 translation、scale 線性，rotation 最短路徑 slerp；weight 0 是 a、1 是 b，不夾住；`out` 可以就是 `a` 或 `b`。照 Bevy 的 `Transform` 插值（`blendable.rs:133-139`）。多個姿勢的混合、分層（上下半身）、additive、時間型 crossfade 是遊戲的規則，可以用這個與逐節點的覆寫組出來（Bevy 的混合圖把累積權重化成兩兩插值，`animation_curves.rs:486-510`）。

### 4.3 模型空間：`ComputeModelSpace`

`out[i]` 是根節點為自己、其他為 `out[parent] × ToMatrix(local[i])`。一趟由前往後（父在子之前，第 2.2 節）。照 Jolt 的 `SkeletonPose::CalculateJointMatrices`、ozz 的 `LocalToModelJob`。命中形狀擺到骨頭上、附掛、蒙皮矩陣都從這裡出發。

### 4.4 Retarget：`BindByName`、`Retarget`

- `BindByName(source, target)`：名字（位元組）相同的節點綁在一起，回 `bindings`（source 的順序）與兩邊沒對上的節點。同一個名字在一邊出現兩次、而另一邊有這個名字，是 `AmbiguousName`。它只是做 binding 的一種方法：要拿掉的節點（例：網格節點、`Mannequin` 這種不是骨頭的節點）由呼叫者從 `bindings` 刪掉再交給 `Retarget`。
- `Retarget(source, target, clip, bindings, settings)`：搬的是每個節點**相對 rest 的動作**，不是絕對值，所以兩邊的 rest 可以方向、比例不同；source 的 key 等於 source 的 rest 時，結果就是 target 的 rest。設 S、T 是兩邊父節點在模型空間的 rest 旋轉（根節點為單位）：

  ```text
  C            = conj(T) * S
  rotation'    = C * rotation * conj(rest_s.rotation) * conj(C) * rest_t.rotation
  translation' = rest_t.translation + C (translation − rest_s.translation) * k
  scale'       = rest_t.scale * scale / rest_s.scale   （逐分量）
  ```

  k＝`translationScale` ×（source 父節點的 rest 均勻縮放 ÷ target 父節點的）。也就是：動作先換到模型空間，再換回 target 父節點的座標。沒綁的 source track 丟掉；沒有 source 的 target 節點保持 rest。key 時間與 duration 照抄，track 照 target 節點排序。
- 規則（照順序）：`translationScale` 有限且大於 0（`BadTranslationScale`）；binding 的節點在範圍內、每個節點至多綁一次（`BadBinding`）；被綁節點的父鏈上 rest scale 都是均勻且大於 0、source 節點自己的 rest scale 沒有 0 的分量（`BadRestScale`：否則 rest 框架沒有單一的旋轉與縮放）。
- 參考：Bevy 靠名字路徑的 hash 對 target（`bevy_animation/src/lib.rs:163-186`：「骨頭名字相同就能播」，等於只做到同 rest 的 retarget）；Jolt 的片段也以 joint 名字對上（`SkeletalAnimation.cpp:82`），`ScaleJoints` 就是平移比例（`:55-60`），兩副骨架之間用 `SkeletonMapper`（`SkeletonMapper.h:81,107`，靠中立姿勢對齊）。這裡多做「rest 不同」的修正，因為 PvP 的身體與片段來源是不同的檔、比例不同（盤點 4.3 的 `translation_scale` 0.3125）。

### 4.5 跨平台

同一個建置、同樣的輸入，結果相同。不同平台、不同編譯器之間最後幾個位元可能不同（`sin`、`acos` 與 float 的運算順序沒有固定）：用在每台機器自己判的地方（Match 的命中、Client 的畫面），不要用在兩台機器必須逐位元一致的地方。PvP 在 master 也是這樣（盤點 4.6：取樣結果跨平台不同，命中只在 Match 算）。

## 5. 不在本 Module

- **第 2 層**（每個實體播哪個片段、播到哪裡，`AnimationModule`）：先不做，等有產品要 Bevy `AnimationPlayer`（`lib.rs:748`）那種用法（`-skeletal-animation-layers`）。
- **第 3 層**（蒙皮顯示：網格照姿勢變形、上傳 GPU）：render（R2b，GPU 或 CPU 在那裡判）。第 2.1 節的公式是 render 要照的。
- **動畫事件**（盤點 4.6 放在第 1 層的「事件資料與區間穿越」）：這張卡沒做。PvP 的事件來自另一份 JSON（`viewmodel.anim_events.json`），不在 FBX 裡；要時加一個純函式（區間內穿過了哪些時刻，含跨 loop）與它的資料，列在後續。
- 命中形狀、何時切片段、分層、additive、瞄準、附掛：遊戲規則。
- FBX 以外的格式（glTF、OBJ）：有點名的 Feature 時另加 loader。
- morph（blend shape）、dual-quaternion 蒙皮、頂點色、第二組 UV、切線：今天不讀（M-7）。
- 第三方授權告示：打包時附，在批次表後續（K 的 tools 線）。

## 6. 卡片照 AGENTS §2.6 判的（不是使用者的話；可以推翻）

| # | 判的 | 根據 | 比過的 |
|---|---|---|---|
| M-1 | 一個 Module，不拆 `mesh`／`animation` | 使用者定第 1 層放 model；Bevy 拆是因為各自是 ECS 元件（第 1 節） | 照 Bevy 拆三個 crate |
| M-2 | 姿勢型別 `LocalTransform` 自己定，不用 transform 的 `Transform` | transform 連 Kernel、是 World 的登記單位，Match 用 model 不該把它帶進來；欄位相同，產品換一行就好 | 依賴 transform Module |
| M-3 | 一個檔＝一個擁有全部值的 `ModelData`，片段、網格用索引 | GYO2 的 asset 沒有子資產標籤（Bevy 的 `Gltf` 是一組 `Handle`，`assets.rs:18-38`）；Match 取樣是熱路徑，索引比名字快 | 照 Bevy 拆成子資產；片段照 Bevy 用名字 hash 對 target |
| M-4 | 片段帶 `nodeCount`，和節點表不合是 Programmer Error | 每 tick 的取樣不該做資料檢查；片段與骨架誰配誰是程式寫的 | 每次取樣回 `Result` |
| M-5 | key 只有線性（rotation slerp），沒有 step、cubic | FBX 經 `ufbx_bake_anim` 烘成線性的 key；PvP 只用這個 | 照 Bevy 帶插值方式（`gltf_curves.rs` 的 stepped、cubic） |
| M-6 | retarget 搬「相對 rest 的動作」，binding 由呼叫者給、`BindByName` 只是輔助 | AGENTS §1-1（對法是作者選的）；PvP 的兩個檔 rest 比例不同；Jolt 也把對應交給呼叫者（`SkeletonMapper::Initialize` 的 `CanMapJoint`） | 只做同名同 rest（Bevy）；自動人形配對 |
| M-7 | morph、dual-quaternion、cache deformer 不讀但不拒收；拒收的只有存不下的（第 3 節規則 3） | 基本形仍然正確，拒收會讓有無害 shape key 的檔載不了（AGENTS §1-10 預設方便）；PvP 的檔沒有這些 | 照 master 全部拒收 |
| M-8 | 幾何變換放進頂點（`MODIFY_GEOMETRY`），`Mesh` 沒有 geometry-to-node 矩陣 | 少一個欄位、render 少一個乘法；蒙皮的 `geometry_to_bone` 已含它 | 照 master 保留 `geometryToNode`（`PRESERVE`） |
| M-9 | settings 只有 `meshes`（預設 true） | Match 只要骨架與片段（盤點 4.4：載入 0.23 s、93 MB）；不認得的成員拒收讓之後加設定是放寬 | 不設 settings |
| M-10 | joint 索引 `uint16`（至多 65536） | 照 Bevy 的 `Uint16x4`；Bevy 的渲染上限 256（`bevy_pbr/src/render/skin.rs:28`）是渲染的事，資料不先限制 | `uint8`（256） |
| M-11 | ufbx 抓 `ufbx.h`、`ufbx.c` 兩個檔（FetchContent 的 URL＋SHA-256），不 clone | 照 stb_image（I-6）；釘 commit 的網址加 SHA-256 | 照 master 用 git tag（tag 會動） |

## Design Invariants

條號用 MD-。

1. **MD-1 ufbx 只在一個 adapter。** `ufbx.h` 只被 `engine/modules/model/src/adapters/ufbx/` 與 `third_party/ufbx/` 的實作檔 include；`gyo_ufbx` 只被 `gyo_model` **私有**連結，標頭不出現在任何 `include/` 下（AGENTS §1-4）。adapter 對外只有 `Detail::ImportFbx`，用 model 自己的型別。`UFBX_NO_STDIO` 放在 `gyo_ufbx` 的 PUBLIC 定義。守衛：設定時的檢查 `gyo_expect_model_links`（`tests/common/model/CMakeLists.txt`：連結恰好那五個、`gyo_ufbx` 不在 `gyo_model` 的 INTERFACE 連結裡）；別的 target 沒有 ufbx 的 include 路徑；「沒有別的 target 連 `gyo_ufbx`」靠審查。
2. **MD-2 模型是 CPU 的資料。** `ModelData` 是第 2 節的型別與規則：擁有全部的值，沒有渲染的 handle、解碼的貼圖、匯入器的指標；父在子之前；權重和為 1 或全 0；片段帶 `nodeCount`。守衛：`AnimationTests.cpp` 的 `Validate` 各規則各條件、規則的先後。
3. **MD-3 第 1 層是純函式。** `RestPose`、`SampleClip`、`Blend`、`ComputeModelSpace` 沒有時鐘、World、呼叫之間的狀態；照第 4.1～4.3 節的定義。守衛：`AnimationTests.cpp` 的取樣（key 之間、key 上、channel 外、Clamp、Loop、duration 0、最短路徑）、混合、模型空間、Programmer Error 各案；`FbxLoaderTests.cpp` 用範例檔的骨頭轉 90 度驗證整條路（載入→取樣→模型空間→蒙皮）。
4. **MD-4 retarget 搬相對 rest 的動作。** 第 4.4 節的公式與規則；binding 是呼叫者的資料。守衛：`AnimationTests.cpp` 的 retarget 各案（相同節點表、rest 的 key 給 target 的 rest、父 rest 不同時模型空間的旋轉變化相同、平移換座標與比例、scale 比例、丟掉沒綁的、三條規則與先後）。
5. **MD-5 中立、沒有裝置。** `GYO::Model` 的連結恰好是 `GYO::Base`、`GYO::Math`、`GYO::Asset`、`GYO::Json`、`gyo_ufbx`：沒有 SDL、渲染、GPU、World、產品名。守衛：`gyo_expect_model_links`；中立靠審查。
6. **MD-6 模型的 Data Contract。** `model`／`fbx` 的規則、先後、轉法、settings 與 Version 在第 3 節；loader 是純函式（沒有 World、時鐘、全域可變狀態；不開檔），只連 `GYO::Model` 就能呼叫。守衛：`FbxLoaderTests.cpp` 的 settings、規則 1、2、範例檔的節點／材質／網格／蒙皮／UV／片段、`meshes: false`、「same model for the same bytes, on any thread (AS-3)」「through the asset store」兩案。規則 3（`Unsupported`）與規則 4（`BadData`）沒有檔案能觸發的測試（要多 skin 的網格或 NaN 的檔，Blender 做不出），靠審查。

### Replacement Criteria

下列任一成立時，不再修補，交使用者決定修還是重建。

1. **RC-MD1**：不可信來源的模型成為 Feature（玩家上傳），或 ufbx 的缺陷影響我們的內容，就重新評估 adapter。
2. **RC-MD2**：點名的 Feature 需要第 2.1 節存不下的東西（morph、多組 UV、頂點色、cubic key、每頂點超過 4 個權重），`ModelData` 要改形——讀它的每一處（render、遊戲）都要改。
3. **RC-MD3**：兩台機器必須對同一個姿勢逐位元一致（例：Client 也預測命中、姿勢進回滾或摘要）。第 4.5 節不保證這件事，要換成固定運算順序與自己的三角函數。
4. **RC-MD4**：整檔讀進來再整個轉換放不下（串流、分塊），或載入時間超出預算。

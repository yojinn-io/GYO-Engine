# Object_FPS v2：3D 敵人與碰撞檢視

v2 使用 Superhero Male 作為近戰敵人、Superhero Female 作為遠程敵人，
保留人物原有的身體、眉毛與眼睛貼圖，並分別配上側分短髮與髮髻。各模型以自己的 reference pose
計算固定腳底 anchor，再等比例縮放至 1.6 公尺。近戰仍使用拳頭，
遠程在右手持有 Ultimate Pistol 1，從槍口發射。

動作來自 UAL1，載入時依相容骨架的 reference pose 轉移到人物模型。
目前使用 unlit 材質與 CPU 蒙皮；沒有加入任意人形骨架自動配對、
GPU 蒙皮、坡地控制器、root-motion 位移或剛體物理。

## F3 開關

按 F3 開啟或關閉透視碰撞線框。預設關閉；按住不會連續切換。
暫停時仍可操作，切換不推進模擬。設定在當次程序內跨關卡保留，
重新啟動後恢復關閉。HUD 顯示狀態及圖例。

| 顏色 | 實際使用的形狀 |
| --- | --- |
| 綠色 | 玩家與活敵人的移動膠囊 |
| 黃色 | 骨架頭部、軀幹及四肢的受擊膠囊 |
| 紅色 | 有效近戰窗口內的拳部掃掠形狀 |
| 青色 | 格子牆轉成的 AABB |

線框來自模擬快照的世界座標幾何，不從繪製模型重新估算。
沒有啟用的碰撞體不顯示，因此死亡角色仍在播放倒地時，不保留受擊線框。
地板是 y=0 平面，不會虛構一個地板碰撞箱；角色膠囊也不代表部位受擊範圍。
線框透過 WorldOverlay pass 顯示在場景前，武器及 UI 仍在其後繪製。
關閉時不建立、更新或提交除錯網格。

## 職責與資料流

```text
v2 AI／格子尋路 → 角色膠囊移動 → AnimationInstance 更新／混合
    → CPU Pose → 骨架受擊區、攻擊掛點 → 戰鬥結算
    → 擁有獨立 Pose 的快照 → ModelRenderer／碰撞線框
```

移動膠囊不隨骨架擺動。玩家、牆壁、活敵人使用同一組 Engine 幾何查詢；
滑動與穩定順序解算屬於 v2。Engine 不知道 GridMap、EnemyId、部位名稱或傷害。
尋路的格子可通行判定仍是遊戲政策，不是另一個實際位移求解器。

權威動畫實例在 gameplay 中。一般動作以短過渡切換，中斷時從當前混合姿勢開始；
遠程進入射擊時的過渡為 0 秒，保留 Pistol_Shoot 第一幀開始的後座動作。
渲染僅使用快照，不重新決定動畫時間。精確出手時刻可從當次更新前的動畫實例
取樣，避免用下個渲染畫面的手部位置代替事件位置。

近戰對有效窗口分段取樣拳部軌跡，檢查玩家膠囊與牆壁，一次出招最多命中一次。
遠程在 release 時刻，以該時刻右手姿勢與武器掛點算出的槍口世界座標發射。
槍身繪製與發射位置使用同一組骨架姿勢、掛點變換、角色 anchor、縮放與 yaw。
近戰／遠程攻擊間隔仍為 0.90／1.25 秒。死亡會取消尚未消費的攻擊，
停用移動與受擊碰撞，播放 Death01 後回收。出生點可在屍體播放期間保持保留，
但這不會讓屍體重新阻擋玩家。

實際 UAL 資料的校正結果如下。事件不是從 FBX 內嵌標記讀取，
而是 v2 對已取樣動作所設定的遊戲時間；可用驗收工具的 `--inspect-rig` 重現取樣。

| 動作 | clip 長度 | v2 使用方式 |
| --- | --- | --- |
| Idle_Loop | 2.5 秒 | 近戰待機，循環 |
| Pistol_Idle_Loop | 50 / 30 秒 | 遠程持槍待機，循環 |
| Jog_Fwd_Loop | 28 / 30 秒 | 兩類敵人移動，循環；遠程移動時槍身仍跟隨右手 |
| Punch_Jab | 26 / 30 秒 | 左拳，有效窗口 4 / 30 至 8 / 30 秒；前伸峰值在 6 / 30 秒 |
| Pistol_Shoot | 19 / 30 秒 | 右手持槍，單次；0 秒發射，進入動作的過渡為 0 秒 |
| Death01 | 2.4 秒 | 單次，結束後回收 |

Pistol_Shoot 在 0 至 1 / 30 秒間已產生明顯後座；發射事件取 0 秒姿勢，
避免子彈延遲到槍身抬起後才出現。UAL1 沒有專用持槍跑步動作，
目前保留一般 Jog_Fwd_Loop，不另加上半身混合或 IK。

男女模型的 reference 高度與腳底不同；anchor 與等比例縮放由載入模型計算，
模型朝向為 +Z，不另維護一套渲染或碰撞校正值。

頭部節點名稱是區分大小寫的 `Head`。依主要受該節點影響的蒙皮頂點校正，
男性的骨骼局部中心為 `(0, 0.094935, -0.019193)`，
女性為 `(0, 0.103493, -0.008712)`。兩者都使用 0.16 公尺半徑的頭部球形膠囊，
再套用角色縮放，避免把頭部關節位置誤當成頭顱中心。

射擊以牆壁與所有部位形狀的最近命中為準，同一敵人的重疊部位不重複扣血。
準星先決定瞄準點，再從槍口檢查實際命中；結算使用最後命中的 region，
因此槍口被牆壁或其他部位擋住時，不會誤套準星所指部位的倍率。

### 部位傷害

每個受擊區自行設定 `damage_multiplier`，男女敵人目前採用相同初始值，
但分別保存在各自的 enemy 資料中，可獨立調整。

| 受擊區 | 傷害倍率 |
| --- | --- |
| `head` | 2.0 |
| `torso`、`pelvis` | 1.0 |
| 左右 `upper_arm`、`forearm`、`thigh`、`calf` | 0.75 |

結算順序為 `max(1, 武器基礎傷害 × 部位倍率 − 敵人防禦)`，最後按剩餘生命值截斷。
例如基礎傷害 25、防禦 5 時，頭部扣 45、身體扣 20、四肢扣 13.75。
角色仍共用一條生命值與整體防禦，沒有部位生命值、部位護甲、肢解或免疫。

`EnemySystem::ApplyDamage(id, rawDamage, region)` 從目標自身的不可變 rig 查詢倍率；
呼叫端只傳命中部位 ID，不能自行提供倍率。省略 region 的直接傷害使用倍率 1；
指定未知部位、未知或已死亡敵人，以及非有限或非正的原始傷害均不套用。
中間乘法與減傷使用 double，扣血後再轉回 float，避免兩個有限 float 相乘溢位。
結果的 `rawDamage` 保留武器原始傷害，`appliedDamage` 表示實際扣除的生命值。

## v2 資料契約

`assets/object_fps_v2/data/enemies.csv` 保留敵人 ID、種類、傷害、冷卻、HP、
防禦及 body 膠囊尺寸，最後一欄為 `presentation_asset_id`。
不再讀取 sprite 圖集尺寸、像素槍口或 `enemy_animation_clips.csv`。

`characters/enemies/*.enemy.json` 使用整數 `version: 1`：

- `character_asset_id` 指向既有 character 定義，該定義提供模型、材質及動畫集。
- 動畫集必須包含 `idle`、`move`、`attack`、`dead` 四個語義；跨來源動畫須由
  character 定義明示啟用相容 reference pose 綁定，組裝後均指向同一份人物模型快照。
- `hurt_regions` 每項有唯一 `id`、`start`／`end` 骨骼點及 `radius`。
  骨骼點包含 `node` 及可選三維 `offset`，均在模型座標中表達。
  可選 `damage_multiplier` 必須是可表示為有限正 float 的數字；省略時為 1.0，
  因此沿用 `version: 1`。正式敵人資料明確填入每個部位的倍率。
  載入器與 domain 出生驗證均拒絕無效倍率或重複 ID，診斷包含敵人與部位。
- 沒有武器掛點時，`attack.point` 是出招骨骼點，`radius` 是近戰球體半徑。
  近戰使用 `begin_seconds`／`end_seconds`；遠程使用 `release_seconds`。
  可選的 `attack.transition_seconds` 指定進入攻擊動作的過渡時間，預設沿用一般過渡；
  遠程設定 `release_seconds: 0` 與 `transition_seconds: 0`。
- 可選的 `weapon` 描述持有武器：`character_asset_id` 指向武器模型／材質定義，
  `node` 指定人物骨骼，`translation`、`rotation_xyzw`、正值 `scale` 指定骨骼局部變換，
  `muzzle` 是完成匯入變換後的武器模型座標。四元數會正規化。
  有 `weapon` 時，`attackPoint` 由武器掛點與槍口推導，禁止再填 `attack.point`。
- 骨骼偏移與半徑使用匯入後的模型公尺，再與模型一起套用同一 uniform scale。
  攻擊與死亡時長取自 FBX，沒有第二份手工維護的 clip 長度。

人物的 `*.character.json` 保留 `model_asset_id`、`material_overrides` 與
`animation_set_asset_id`，並可選擇加入：

```json
"animation_binding": {
  "mode": "compatible_reference_pose",
  "translation_scale": 1.0,
  "ignored_source_nodes": ["Mannequin"]
}
```

未提供此設定時維持同來源綁定限制。啟用後由 v2 App 按唯一節點名稱建立明確索引配對，
包括祖先節點；Engine 檢查父子關係與 reference pose，再轉移各動作的相對運動。
`translation_scale` 是來源模型空間位移的正值倍率，人物設定為 1，
不取代最後縮放至 1.6 公尺的角色變換。`ignored_source_nodes` 明示排除來源 mannequin 網格節點，
不允許藉此略過骨骼動畫。人物自己的網格、蒙皮綁定與 reference pose 保留，
轉移後動作組裝到新的不可變模型快照，不改寫 AssetManager 中的原始來源模型。

手槍使用 `weapons/ultimate_pistol_1/world/Pistol_1.fbx`；匯入後槍管朝 +X，
`muzzle` 約為 `(1.479757, 0.56304, 0)`，掛點縮放為 0.14，
再套用人物本身的縮放。這些完整模型座標已包含 FBX 節點變換，
不能直接以原始網格頂點座標取代。掛點配置屬 v2 資料，Engine 不知道右手或槍口語義。

### 頭髮與外觀附屬物

男性使用 `Hair_SimpleParted`，女性使用 `Hair_Buns`，來自同一套人物資產。
兩者沿用已登錄的 hair texture；原本人物的 Hair 材質槽只負責眉毛，並沒有頭髮網格。

敵人的 character 定義使用可選 `accessories` 陣列：

```json
"accessories": [
  {
    "character_asset_id": "object_fps_v2.hair.simple_parted",
    "node": "Head",
    "source_node": "Head"
  }
]
```

`node` 是人物掛點；`source_node` 是髮型原始蒙皮所綁定的節點。
這個契約只接受所有頂點完整綁在單一節點上的靜態外觀，不支援巢狀附屬物或獨立動畫。
可選 `translation` 與正值等比 `scale` 用於局部校正，這兩款髮型使用預設值即可。
它們的 Head bind frame 與對應人物相同，因此保留來源 geometry-to-joint 矩陣，
將該節點的 global transform 替換成權威人物快照的 Head transform 即可正確蒙皮。

外觀配置、載入及 GPU 實例歸 v2 App 呈現層；不加入 EnemyRig 的戰鬥資料，
也不參與身高校正、移動膠囊或受擊區。ModelRenderer 沿用既有資源管理與蒙皮能力，
頭髮隨同人物待機、跑動、攻擊及倒地，並與人物實例一起回收。
此變更只擴充 v2 character 呈現契約，沒有新增 Engine 模組、依賴邊或修改 v1 契約。

缺失／錯誤版本、找不到或重複的骨骼名稱、未授權的跨來源綁定、
不相容的父子關係、未映射的動畫節點、非均勻或鏡像的動畫／reference 縮放、
非有限數值、非法碰撞尺寸、重複部位、超出 clip 的事件以及小於動作長度的冷卻，
均視為載入錯誤。
本契約由 v2 App 載入後轉成不可變 CPU EnemyRig；Runtime 不依賴 Editor 或來源美術目錄。

## 架構邊界與依賴

本文件維護 v2 敵人的遊戲架構、資料契約及呈現政策；
Engine 的整體依賴方向與共用規則見[總體架構](../architecture.md)。

| v2 責任 | 使用的 Engine 能力 | 留在遊戲內的政策 |
| --- | --- | --- |
| domain／EnemySystem | Model 的 AnimationInstance、Collision 的幾何查詢 | 敵人狀態、動畫時間、骨骼部位、攻擊窗口、傷害及死亡回收 |
| 世界 collision adapter | Collision 的 Capsule／AABB overlap 與 sweep | GridMap 轉換、滑牆、角色阻擋、平面敵人移動及出生點保留 |
| App 內容載入 | Asset、Model | CSV／JSON 解讀、動作語義、骨骼綁定及模型校正 |
| 武器與敵人呈現 | ModelRenderer、Render | 快照轉成繪製提交、實例生命週期及遊戲材質配置 |
| App display settings | Input、Render 的 WorldOverlay 與線框產生器 | F3、預設值、跨關卡保留、顏色圖例及生效形狀選擇 |

首次 3D 敵人遷移來自三個實際壓力：sprite 幀／像素無法表達骨架受擊與出招掛點；
命中計算需要與動畫共用時間與姿勢；武器和敵人的蒙皮、上傳及回收流程重複。
因此 v2 domain 新增 `GYO::Model` 依賴，以模擬端的 Pose 決定受擊區與事件位置，
再交由快照傳給呈現端。v2 敵人契約改成角色、動作與骨架配置，取代 sprite 契約。

武器與敵人的共用呈現流程沉澱到獨立的 `GYO::ModelRenderer`，
其依賴為 Model／Render，兩個底層模組不互相依賴。
格子地圖、EnemyId、部位名稱、傷害與狀態語義不進入 Engine。
詳細的[渲染契約](../rendering_architecture.zh-Hant.md)仍由 Engine 文件維護。

### 人物與手槍的 Architecture Delta

男女人物沒有內建動作，且與 UAL1 雖然具有相同骨骼階層，骨長、reference 旋轉及
蒙皮綁定仍不同；單改資產 ID 或沿用來源節點索引，無法正確呈現人物。
因此在既有 `GYO::Model` 追加 `TransferCompatibleAnimation`，接受明確索引配對，
在父節點 reference 座標之間轉換相對運動，輸出獨立擁有 keyframes 的動作。
它不辨識骨骼名稱或人形部位，不建立新的 subsystem。

名稱配對、允許的來源排除與綁定模式由 v2 App 的 character 契約擁有；
App 負責組裝不可變人物模型，domain 與呈現端繼續使用同一份模型及 Pose。
enemy 契約追加可選武器掛點，由單一槍口配置同時決定呈現和攻擊位置，
已有 ModelRenderer 負責人物與槍身的資源生命週期。
影響範圍為 Model 的新增介面與測試、v2 的資料載入、enemy 契約、呈現及驗收；
依賴仍為 v2 → Model／ModelRenderer／Render，沒有新增反向依賴或產品間依賴。
v1 的契約與載入路徑不遷移。

### 部位傷害的 Architecture Delta

實作壓力來自射擊已辨識骨架部位，結算卻只接收敵人 ID 與武器傷害，導致所有部位
扣血一致。僅新增 JSON 欄位無法使倍率生效；因此擴充既有 `EnemyHurtRegion` CPU
契約與 `ApplyDamage` 的可選 region 參數，讓 GameSession 傳遞最後命中的部位。

倍率的配置及解讀歸 v2 App／enemy 資料，計算與驗證歸 v2 domain／EnemySystem。
碰撞快照仍只有部位 ID 與幾何，Engine Collision 不知道傷害規則；不增加 subsystem、
依賴邊或產品間依賴。影響範圍限於 v2 資料、載入、結算、測試及文件；
Engine、v1、碰撞形狀與呈現責任維持原有邊界。

### Engine 介面與 v1 相容性

`GYO::Model` 增加 AnimationInstance／BlendPoses，但保留 SamplePose／SkinMesh。
`GYO::Collision` 保留直立膠囊舊 API，新增任意軸 Capsule 與 Contact 查詢。
`GYO::ModelRenderer` 獨立依賴 Model／Render，v2 武器與敵人共用它。
模型、材質與貼圖資源可共用，每隻角色的動態網格及姿勢獨立。
Render 的線框工具只接受幾何數值，不依賴 Collision。

v1 不遷移資料或程式；新增能力不要求 v1 連結 ModelRenderer。
v2 的專屬契約與設定也不改變 v1 原有的 sprite 敵人路徑。
驗證 v1 時使用忽略的 build 目錄內 registry 副本及 `GYO_REGISTRY_FILE`，
不要為測試更改 repository 的預設產品選擇。

## 驗證

共通測試使用合成幾何與模型；UAL、男女人物、手槍／Mark23 和遊戲規則歸 v2 owner。
測試涵蓋膠囊角落掃掠、初始重疊、動畫混合、中斷、多實例隔離、
骨架命中、攻擊事件、死亡取消、F3 圖例及 overlay pass 順序。
部位傷害另驗證資料預設及拒絕規則、先乘倍率再扣防禦、最小傷害與生命值截斷、
極大有限數值、未知部位，以及槍口最後命中區域的單次結算。

建置矩陣包括 Engine-only、v1-only、v2-only 與 v1＋v2。
GPU 驗收分別指定 D3D12、Vulkan，檢查 idle、move、attack、dead、
線框對位、遮擋與原有武器呈現。實際執行結果與限制記錄於本次驗證報告，
尚未執行的檢查不可當成已通過的證據。

人物／手槍變更另需驗證 reference pose 轉移保留目標骨長與綁定、來源模型不被修改、
男女材質正確、右手持槍與槍口事件對位、射擊首次後座與 0 秒事件、
近戰維持左拳窗口，以及死亡取消尚未發生的射擊。此處列出驗證範圍，不代表已完成驗收。

選取 v2 並啟用 testing／packaging 建置後，驗收工具從自己的已組裝資產讀取資料。
以下以本次雙產品 build tree 為例，在 repository 根目錄執行：

```powershell
$build = 'build/target/_build/enemy3d'
ctest --test-dir $build -L cpu --output-on-failure
$probe = "$build/acceptance/object_fps_v2/bin/gyo_object_fps_v2_acceptance.exe"
& $probe --inspect-rig --output build/target/validation/rig
& $probe --headless-smoke-test
& $probe --smoke-test --gpu-driver d3d12 --output build/target/validation/v2-d3d12
& $probe --smoke-test --gpu-driver vulkan --output build/target/validation/v2-vulkan
```

CPU smoke 透過 GameSession 的正常輸入清完第一關整波敵人；GPU probe 另取樣
idle／move／death 各三個時刻及 attack 四個時刻，比較碰撞顯示關閉／開啟，
另擷取握槍側面，並執行同一整波流程。
這是 v2 專屬支援程式，不會把測試模式或測試資產加入正式遊戲。
初次 mannequin 遷移的歷史結果見[實作與驗證記錄](../dev_logs/2026_09_22_v2_skeletal_enemies.md)，
該記錄不作為後續人物與手槍變更已通過驗證的證據。
人物與手槍的實際結果見[2026-09-23 驗證紀錄](human-enemies-validation.zh-Hant.md)。
部位倍率的實際結果見[傷害資料化驗證紀錄](region-damage-validation.zh-Hant.md)。

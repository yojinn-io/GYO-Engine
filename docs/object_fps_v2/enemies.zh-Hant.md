# Object_FPS v2：3D 敵人與碰撞檢視

v2 使用 UAL mannequin 的同檔骨架動畫。近戰為紅色、遠程為藍色，
reference pose 以固定腳底 anchor 等比例縮放至 1.6 公尺。
目前使用 unlit 材質與 CPU 蒙皮；沒有加入跨模型重定向、GPU 蒙皮、
坡地控制器、root-motion 位移或剛體物理。

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

權威動畫實例在 gameplay 中。一般動作以短過渡切換，中斷時從當前混合姿勢開始。
渲染僅使用快照，不重新決定動畫時間。精確出手時刻可從當次更新前的動畫實例
取樣，避免用下個渲染畫面的手部位置代替事件位置。

近戰對有效窗口分段取樣拳部軌跡，檢查玩家膠囊與牆壁，一次出招最多命中一次。
遠程在 release 時刻以施法手的世界座標發射。死亡會取消尚未消費的攻擊，
停用移動與受擊碰撞，播放 Death01 後回收。出生點可在屍體播放期間保持保留，
但這不會讓屍體重新阻擋玩家。

實際 UAL 資料的校正結果如下。事件不是從 FBX 內嵌標記讀取，
而是 v2 對已取樣動作所設定的遊戲時間；可用驗收工具的 `--inspect-rig` 重現取樣。

| 動作 | clip 長度 | v2 使用方式 |
| --- | --- | --- |
| Idle_Loop | 2.5 秒 | 循環 |
| Jog_Fwd_Loop | 28 / 30 秒 | 循環 |
| Punch_Jab | 26 / 30 秒 | 左拳，有效窗口 4 / 30 至 8 / 30 秒；前伸峰值在 6 / 30 秒 |
| Spell_Simple_Shoot | 0.5 秒 | 左手，0.10 秒過渡結束時發射；此動作是伸手持續施法與後座，不是右手揮擊 |
| Death01 | 2.4 秒 | 單次，結束後回收 |

匯入後 reference 高度約 1.828717 公尺，腳底 anchor 約 0.000461 公尺，
套用約 0.874930 的等比例縮放；模型朝向為 +Z。這些數值由載入模型計算，
不作為另一組硬編碼的渲染或碰撞校正。

頭部節點名稱是區分大小寫的 `Head`。以主要受該節點影響的 746 個頂點校正，
其包圍中心轉回骨骼局部座標為 `(0, 0.121383, 0.005830)`，包圍半徑約 0.157188 公尺。
v2 使用此局部偏移與 0.16 公尺半徑的頭部球形膠囊，避免把頭部關節位置誤當成頭顱中心。

射擊以牆壁與所有部位形狀的最近命中為準，同一敵人的重疊部位不重複扣血。
結果保留 region 資訊；第一版各部位倍率皆為 1，不加入爆頭獎勵。

## v2 資料契約

`assets/object_fps_v2/data/enemies.csv` 保留敵人 ID、種類、傷害、冷卻、HP、
防禦及 body 膠囊尺寸，最後一欄為 `presentation_asset_id`。
不再讀取 sprite 圖集尺寸、像素槍口或 `enemy_animation_clips.csv`。

`characters/enemies/*.enemy.json` 使用整數 `version: 1`：

- `character_asset_id` 指向既有 character 定義，該定義提供模型、材質及動畫集。
- 動畫集必須包含 `idle`、`move`、`attack`、`dead` 四個語義，且均來自角色同一模型。
- `hurt_regions` 每項有唯一 `id`、`start`／`end` 骨骼點及 `radius`。
  骨骼點包含 `node` 及可選三維 `offset`，均在模型座標中表達。
- `attack.point` 是出招掛點，`radius` 是近戰球體半徑。
  近戰使用 `begin_seconds`／`end_seconds`；遠程使用 `release_seconds`。
- 骨骼偏移與半徑使用匯入後的模型公尺，再與模型一起套用同一 uniform scale。
  攻擊與死亡時長取自 FBX，沒有第二份手工維護的 clip 長度。

缺失／錯誤版本、找不到骨骼或動畫、不同來源動畫綁定、非有限數值、
非法碰撞尺寸、重複部位、超出 clip 的事件以及小於動作長度的冷卻均視為載入錯誤。
本契約由 v2 App 載入後轉成不可變 CPU EnemyRig；Runtime 不依賴 Editor 或來源美術目錄。

## Engine 介面與相容性

`GYO::Model` 增加 AnimationInstance／BlendPoses，但保留 SamplePose／SkinMesh。
`GYO::Collision` 保留直立膠囊舊 API，新增任意軸 Capsule 與 Contact 查詢。
`GYO::ModelRenderer` 獨立依賴 Model／Render，v2 武器與敵人共用它。
模型、材質與貼圖資源可共用，每隻角色的動態網格及姿勢獨立。
Render 的線框工具只接受幾何數值，不依賴 Collision。

v1 不遷移資料或程式；新增能力不要求 v1 連結 ModelRenderer。
驗證 v1 時使用忽略的 build 目錄內 registry 副本及 `GYO_REGISTRY_FILE`，
不要為測試更改 repository 的預設產品選擇。

## 驗證

共通測試使用合成幾何與模型；UAL／Mark23 和遊戲規則歸 v2 owner。
測試涵蓋膠囊角落掃掠、初始重疊、動畫混合、中斷、多實例隔離、
骨架命中、攻擊事件、死亡取消、F3 圖例及 overlay pass 順序。

建置矩陣包括 Engine-only、v1-only、v2-only 與 v1＋v2。
GPU 驗收分別指定 D3D12、Vulkan，檢查 idle、move、attack、dead、
線框對位、遮擋與原有武器呈現。實際執行結果與限制記錄於本次驗證報告，
尚未執行的檢查不可當成已通過的證據。

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
idle／move／attack／death 各三個時刻，比較碰撞顯示關閉／開啟，並執行同一整波流程。
這是 v2 專屬支援程式，不會把測試模式或測試資產加入正式遊戲。
本次結果見[實作與驗證記錄](../dev_logs/2026_09_22_v2_skeletal_enemies.md)。

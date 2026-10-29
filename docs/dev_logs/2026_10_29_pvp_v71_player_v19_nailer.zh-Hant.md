# PvP v7.1：人物換成 Player_V19、第一人稱換成 nailer_v2

日期：2026-10-28～29。分支 `claude/pvp-player-v19`（從 master `5d688aaf`）。要併進 v7.1 發行線、一起進 lan15 測試；合併與 L1 派發由 v7.1 主會話處理。

**狀態：完成**（使用者確認模型驗收，兩項判定變更已核准）。

相關文件：

- 判定變更：[player-v19-judgment-change.md](../object_fps_pvp/plans/v7.1/player-v19-judgment-change.md)（J-muzzle、J-accessory）。

## 做了什麼

### 人物 Player_V19（只改資料）

- `asset_catalog.json` 加 `object_fps_pvp.model.character.player_v19`；`players/character.json` 改用它，`translation_scale` 0.3125（＝舊女性 scale ÷ V19 scale，讓骨盆位移在世界中的幅度不變），拿掉材質覆寫與髮髻配件（V19 有 15 個純色材質、自帶頭髮）。
- `players/presentation.json` 的手上武器掛點依新骨架重算：世界中的朝向、大小、相對手部的偏移（0.0899 m）都與舊的一致；四元數 z 調 3e-8，讓 legacy 正規化逐位元一致。
- 刪除不再使用的女性 FBX、眼睛與身體貼圖（catalog 檢查要求）；頭髮貼圖與 buns 因測試 fixture 仍在用而保留。

### 第一人稱 nailer_v2（資料＋2 行 id）

- 新增 `weapons/nailer_v2/viewmodel/`：FBX、`nailer_v2_viewmodel.json`、`viewmodel.animset.json`（clip 名 `Armature|*`）、貼圖。
  - 引擎沒開 JPG 解碼 → `Nailer1_D.jpg`（4096²）轉成 1024² PNG。
  - 三個純色材質（皮膚、手套、皮膚陰影）用 4×4 單色 PNG，顏色由 FBX 的線性色轉 sRGB（與人物純色材質的呈現一致）。
- `PvpApplication.cpp` 與測試的 viewmodel id 改成 `object_fps_pvp.weapon.nailer_v2`；刪除 PvP 的 Mark23（object_fps／object_fps_v2 各自的副本不動）。
- 槍口：`main_j` 局部 `(0.001603444, 13.567352295, -20.221311927)`（`main_j` 在 0.01 縮放的 Armature 下，所以是公分數值）。用引擎驗證：距前端網格最近頂點 6 mm。
- 擺放：`offset [0.173333, -0.226667, 0.733333]`、`rotation [8.2115, 180, 0]`、`scale 1`。保留模型 15° 偏航展示槍身；仰角由引擎解出，讓槍身延長線穿過準星。縮小用推遠代替（位置除以 0.75，畫面等同 0.75 倍）。

### 判定變更（各自 §14.4，已核准）

- J-muzzle：槍口特性測試的逐值相等只要求 yaw-only 單位縮放的擺放，另對正式擺放的 yaw-only 投影保留逐值相等；其餘由 32 u M 上限把關。
- J-accessory：GUI 驗收的頭髮判定改成依定義宣告的配件數（新增觀測欄位 `declaredAccessories`）。

## 驗證與經過

- 模型來回 6 版（使用者在 Blender 透過 MCP 修正）：
  - ×100 骨架縮放造成精度失敗 → 改用 FBX All 匯出。
  - 半邊臉消失：L 側臉部零件法線朝內，背面剔除下看不到 → Flip。
  - 重新匯入的 `.001` 副本讓 `Armature` 綁定失敗 → 改用原物件。
  - 朝向差 180° → 頂層物件繞 Z 轉 180°。
- 骨架：65 根骨頭名稱與階層和 UAL1 一致；原生速度實測 walk 0.91、jog 6.009（10% 判定內）。
- nailer：53 根骨頭與 Mark23 一致。只繞 Y 的擺放撞不到測試；加仰角時撞到 J-muzzle，使用者先實跑確認角度、動畫與手臂後才走核准流程。
- 突變檢查與 Sonnet 子 Agent 的獨立審查：見判定變更文件。
- 最終：ctest 84/84；GUI `capture`、`player60` 通過；兩個 Client 實跑由使用者確認。

## 留下的事

- 渲染只有 unlit，看不出光影（使用者：後續再做）。已提了「人物光影：受光著色的調查與計畫」的會話卡片。
- HUD 上的武器名仍是 `MARK23`（`PvpApplication.cpp` 的 HUD 文字），未改。
- 遠端玩家手上仍是 `ultimate_pistol_1`；nailer 只換了第一人稱。
- 欄位名 `hair_meshes`／`hairSubmittedMeshes` 實際是「配件網格」，未改名。
- 判定變更文件記錄的既有缺口：配件「實際送出」沒有觀測；`std::max({a,b,c})` 吞掉 NaN。

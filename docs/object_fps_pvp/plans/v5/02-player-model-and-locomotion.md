# 第 02 批：玩家模型與位移驅動步頻

狀態：已完成，2026-09-28；只執行本批，短驗證後已停止。第 03 批未開始。
先讀 [進度與執行規則](README.md)、[交接](HANDOFF.md) 及
[v5 契約](../../protocol-v5.zh-Hant.md)。本文件是實作批次，不另外維護玩法或協定規則表。

## 目標與範圍

以產品已有的女性骨架人物、髮髻及右手手槍替換遠端玩家的箱體，完成 Idle／Jog、
實際位移驅動步頻及上半身持槍。本批只改 Client 呈現，仍使用 v4 wire 與玩法；
不提前增加跳躍、彈匣、換彈、死亡／重生或 life 欄位。v4 的 HP 歸零行為也保持不變。
Client／Gateway／Match 的 v5 原子切換由第 03 批負責。

側移／後退沿用現有 Jog 的近似已獲批准。其方向表現不等同專用 strafe／backward
動作；驗收需記錄這項限制，不為補齊素材引入新動畫包、IK 或通用動畫圖。

## 既有資產與責任

所有下列來源均由 `object_fps_pvp` 擁有，無需讀取、連結或保留 `object_fps_v2`：

- `assets/object_fps_pvp/characters/superhero_female/`：
  `Superhero_Female_FullBody.fbx`、身體／眼睛／頭髮材質。
- `assets/object_fps_pvp/characters/hairstyles/buns/`：髮髻模型與 `Head` 掛點定義。
- `assets/object_fps_pvp/animations/ual_mannequin/UAL1_Standard.fbx`：
  `Armature|Pistol_Idle_Loop` 與 `Armature|Jog_Fwd_Loop`。
- `assets/object_fps_pvp/weapons/ultimate_pistol_1/world/`：第三人稱手槍。
  同產品 `characters/enemies/ranged.character.json` 及 `ranged.enemy.json` 提供已校正的
  女性材質、髮飾、reference-pose 綁定與 `hand_r` 掛點參考；不把玩家註冊成敵人。
- `apps/object_fps_pvp/src/App/CharacterPresentationDefinition.cpp` 已有相容骨架動畫轉移、
  材質與髮飾載入；`AnimationSetDefinition.cpp` 已有語意名稱至 clip 的明確綁定。
  前者目前未編入 PvP Client target，僅接入本批確實需要的產品來源。
- Engine 既有 `Model`／`ModelRenderer` 提供姿勢取樣、局部 TRS、矩陣與 CPU 蒙皮。
  `AnimationInstance` 可顯式推進時間，但不提供人物步幅標定或上／下半身政策。

第一人稱繼續使用既有 Mark23。第三人稱 Ultimate Pistol 與 Mark23 是現有資產組合，
不在本批增加武器選擇、角色選擇或另製 Mark23 世界模型。

## 實作切片

1. 在產品資產內建立玩家用 character／animset 與必要的呈現設定，註冊於產品的
   `asset_catalog.json`。沿用已存在的版本化 JSON、相容 reference-pose 綁定與材質契約，
   不經 `EnemyCatalog`、`GameSession` 或 Campaign 取得玩家人物。
2. 在產品 Client 內建立玩家呈現器。共享不可變模型、動畫及 GPU 資源，每個遠端玩家
   持有自己的姿勢、步頻相位與 instance；加入前完成資產準備，避免首見玩家同步載入。
3. 從 reference pose 求固定腳底 anchor，依 PvP 身高等比例縮放。套用既有呈現位置與
   yaw；人物、髮髻與右手槍使用同一個合成姿勢及世界變換。不要隨動畫重算腳底 anchor。
4. 在 `PvpApplication::PrepareWorld` 現有遠端 timeline 取樣點替換箱體提交。
   沿用插值後位置；不建立第二套移動預測、時間線或 authoritative 位置。
5. 以相鄰有效呈現樣本的實際水平位移推進 Jog 相位，將一次循環的步幅／參考速度校正
   留在產品資料。轉向本身不算位移，撞牆、停止或 timeline hold 不讓腳步原地持續走；
   恢復時不補播停頓期間不存在的路程。新玩家、離開、重入及不連續位移清除舊相位。
6. Idle／Jog 過渡和上半身持槍組合留在 Client。以明確的產品骨骼選擇及既有姿勢操作
   保留腿部步行、上身持槍和正確手槍掛點；必要的 globals 由合成後 local TRS 重建。
   不把整個 `Pistol_Idle_Loop` 覆蓋到腿部，也不只調整槍的世界位置掩蓋手部偏離。
7. 保留既有 Mark23、射擊回饋、HUD 與滑鼠消費路徑。本批不以人物姿勢取代 Match
   的命中盒，不增加遠端動作傳輸；第 04 批再接完整動作呈現。

步幅需對實際縮放後的人物校正，不能把 v2 每幀 `Advance(deltaSeconds)` 原樣接上，
也不能僅以按鍵狀態或未經碰撞修正的指令速度宣稱已符合實際移動速度。

## 短驗證與完成條件

- 建置 PvP Client；必要 CPU 測試覆蓋資產／clip 綁定、相位與實際位移比例、停止／
  撞牆／hold、不連續位移清理及每玩家狀態隔離。只測有意義的呈現政策。
- 真雙 GUI／GPU 短測核對女性身體材質、髮髻、腳底、縮放、右手握槍、Idle／Jog 與
  上身持槍；保存成功 Presented 的姿勢資料及代表性圖像，不能以 headless 輸出替代。
- 在 30／60／144 FPS 短片段中核對相同路程的步頻一致性；前進、斜走、側移／後退、
  轉向、撞牆、停止及開始／結束插值 hold 均有明確結果。側／後退近似限制保留於報告。
- 短回歸既有移動／射擊共存、失焦／Tab／拖窗及離開重入；新增呈現不能造成重複射擊、
  相機角度倒退、模型殘留或首見人物長幀。記錄實測成本，不提高既有通過門檻。
- 靜態核對 schema／Go bindings／Gateway／Match 玩法未改；Match 不新增 Model、
  Renderer、SDL 或 Campaign 依賴。測試與 acceptance 僅於本 owner 選擇時編入。

## Architecture Delta 與停止點

觀測到的缺口是 Client 只畫箱體，且既有敵人呈現器要求 Campaign snapshot，不能直接
作為 PvP 玩家介面。最小改動是啟用已複製的產品 character loader、增加產品玩家
呈現資料／呈現器及 Client source 清單；不移動敵人責任、不新增公共 subsystem。
依賴方向維持 PvP Client → Engine Model／ModelRenderer，兩者本來已由 Client 使用；
所有角色、骨骼選擇、步幅與掛點仍屬 PvP，無新 product 依賴或 Editor／Runtime 契約。

短驗證通過後更新 [README](README.md)、[HANDOFF](HANDOFF.md) 與本批 dev_log，
記錄實際校正、GPU 證據、成本及限制，然後停止。不自動開始第 03 批、不提交 commit，
不執行長測，也不將本批短測稱為 v5 穩定基線。

## 實際交付

PlayerPresentation、玩家character／animset／presentation資料與Join前暖機完成。
同路程步頻在30／60／144 FPS一致；三FPS雙GUI＋獨立GPU圖像4／4通過。
原生操作V1–V8通過，16秒移動／射擊／HP共存通過，可見延遲P50／P95
44.58／45.50ms。詳見[本批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch02.zh-Hant.md)
及[交接](HANDOFF.md)。保留側移／後退近似、當前膠囊命中及短測範圍限制。

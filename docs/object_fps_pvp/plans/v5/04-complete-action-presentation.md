# 第 04 批：完整動作呈現

狀態：**計畫複審完成（2026-10-02），待使用者決定驗證平台並明確啟動。**依賴已滿足：第 02 批完成，
第 03 批 2026-10-02 結案（PR #11，`bec86b7`）。複審核對與待決事項見下方「複審核對（2026-10-02）」。
先讀 [進度與執行規則](README.md)、[交接](HANDOFF.md) 及
[v5 契約](../../protocol-v5.zh-Hant.md)。動作、life 與時間語意只在契約維護；
本文件描述 Client 如何呈現已完成的權威玩法。

## 目標與範圍

將第 03 批的射擊、彈匣／換彈、跳躍、死亡／重生接入第一人稱 Mark23、遠端骨架
人物及 HUD。保留第 02 批的位移驅動步頻和上半身持槍，移動與武器動作能同時顯示。
本批不重新設計 Match 規則、動作交付或協定，不加入近戰、收槍操作、動畫命中盒、
ragdoll、IK 或通用動畫狀態圖。

## 可用產品資產與支援

- 第一人稱沿用 `assets/object_fps_pvp/weapons/mark23/viewmodel/` 與
  `apps/object_fps_pvp/src/App/WeaponViewModel.cpp`。Mark23 已有 Idle／Shoot／Reload／
  Draw／Hide；目前 Client 專用 `WeaponViewModelAction` 僅有 Idle／Draw／Shoot，
  將必要的 Reload 接入該產品呈現介面，不導入 Campaign `WeaponController`。
- 遠端沿用第 02 批女性人物、髮髻、Ultimate Pistol 掛點及玩家呈現器。
  `assets/object_fps_pvp/animations/ual_mannequin/UAL1_Standard.fbx` 已有
  `Armature|Pistol_Shoot`、`Armature|Pistol_Reload`、`Armature|Jump_Start`、
  `Armature|Jump_Loop`、`Armature|Jump_Land`、`Armature|Death01`。
- 以產品玩家 animset 宣告所需語意，沿用 `CharacterPresentationDefinition` 的
  reference-pose 轉移、`AnimationSetDefinition`、Engine 姿勢取樣及 ModelRenderer。
  不讀取 `object_fps_v2` 內容，也不透過敵人／Campaign 流程觸發玩家動作。

## 複審核對（2026-10-02）

對照 `74c08ee` 的程式碼與資產逐項核對本計畫的前提：

| 項目 | 核對結果 |
|---|---|
| Mark23 clip 原長 | Shoot 0.333 秒（20 Tick）、Reload 3.733 秒、Draw 0.833 秒、Hide 0.367 秒、Idle 0.033 秒（以 ufbx 讀取 FBX animation stack） |
| UAL clip 原長 | Pistol_Shoot 0.633、Pistol_Reload 1.667、Jump_Start 1.333、Jump_Loop 2.500、Jump_Land 1.267、Death01 2.400 秒；六個 clip 都在 `UAL1_Standard.fbx`，與 [BASELINE](BASELINE.md) 一致 |
| 第一人稱射擊 | 第 03 批已實作：每次合法射擊重新觸發 Shoot，冷卻取自 `combatRules.cooldownTicks`；連射時 0.333 秒的 clip 在下一發被截斷。切片 1 只需補驗證，不需縮放 Shoot |
| 第一人稱換彈 | `WeaponViewModelAction` 只有 Idle／Draw／Shoot；按 R 只送出動作，畫面沒有 Reload。Mark23 Reload 需由 3.733 秒映射到 1.5 秒（約 2.49 倍速），須以畫面確認不失真 |
| 遠端資料來源 | Snapshot 已有 `CombatState.lastShotActionId／lastShotTick`、`reloadStartTick／reloadEndTick`、`lifeGeneration`，`PlayerState.grounded／verticalVelocity`；**不需改 wire**。只需擴充 Client 的 `PlayerPresentationFrame`（目前只有位置、yaw、hold、life、dead） |
| 玩家 animset | `players/locomotion.animset.json` 只有 idle（Pistol_Idle_Loop）／jog；需在產品玩家 animset 宣告 Shoot／Reload／Jump 三段／Death01 |
| 第 03 批新增的 HUD 與移出 | HUD 已有「CONNECTION POOR」移出警告行；被移出時 Client 回到 Lobby 並在狀態列顯示原因。本批改 HUD 時須保留兩者，並在 800×600 下確認不與新行重疊（第 03 批曾在該尺寸出現 HUD 越界） |
| 驗證工具的平台 | 原生操作 runner（`run_gameplay_gui.py`、`run_native_window.py`）只支援 Linux X11/XTest；第 02／03 批的原生操作與 GPU capture 都在 Linux／Vulkan 取得。目前的 Intel Mac 只用過 GUI timing probe（SDL 層注入）；probe 的 player／weapon capture 走 SDL GPU readback，理論上可用於 Metal，但**尚未在 macOS 驗證** |

### 待使用者決定

**D1 驗證平台。**本批的雙 GUI 短片段、原生操作與 GPU 圖像核對要在哪裡取得：

| 選項 | 內容 | 代價 |
|---|---|---|
| A（建議，若沒有 Linux 環境） | 在本機 Mac：SDL 注入的 probe 跑 30／60／144 FPS 短片段與 GPU capture（先做一次 capture 冒煙確認 Metal readback 可用）；原生鍵鼠清單由使用者手動操作約 5 分鐘並記錄 | 原生操作證據是人工紀錄，不是 XTest 自動化；需在報告中明示 |
| B | 回到 Linux X11／Vulkan 環境，沿用既有 XTest runner | 需要可用的 Linux 顯示環境 |
| C | 為 macOS 新增原生輸入工具（CGEvent） | 擴大範圍，新增驗收工具；本批不建議 |

## 實作切片

1. 第一人稱射擊依 v5 契約的 **10 tick** 快射節奏呈現（第 03 批已實作重新觸發與冷卻，本切片以驗證為主）。沿用一次左鍵上升沿一次動作、
   成功提交後即時回饋及純視覺後座；每次合法快射可重新觸發 Shoot，不能等原 clip
   自然播放完才准許下一次輸入。動作時長來自 Match 規則及契約，不在 Client 複製預設表。
2. 將 Reload 接入 `WeaponViewModelFrame` 路徑。以契約 **90 tick** 的換彈時間映射
   clip 進度；動畫播放完不自行補彈。可預判的本機回饋、接受／拒絕與中斷處理都服從
   v5 的權威狀態及裁決，快照才決定 HUD 的彈匣、備彈與 HP。
3. 跳躍的畫面位置與鏡頭沿用第 03 批的預測／校正及權威移動資料；人物動畫不再產生
   第二份垂直位移。依契約接入 Jump_Start／Jump_Loop／Jump_Land，開始與落地過渡
   各 **100 ms**；不能要求玩法等待 UAL 原始起跳／落地長 clip 播完。
4. 遠端角色沿用同一呈現時間線的移動、姿態與 v5 動作資訊。走動時保持腿部步頻，
   上半身組合持槍、射擊及換彈；武器、髮飾從最終合成 pose 求掛點。沿用已批准的
   側移／後退 Jog 近似，不在本批偷偷擴充為方向動畫系統。
5. 依契約辨識死亡並播放 `Death01`（既有 clip 約 **2.4 秒**），禁止把死亡動畫的
   自然結束當作權威重生條件。死亡、中斷既有動作、新 life 的畫面切換及回收皆由
   v5 狀態驅動；不讓舊生命的屍體、武器動畫或提示附著到新生命。
6. 所有動作呈現狀態具備契約要求的 session／玩家／life 隔離。相同 ActionId 的
   重送、裁決 ACK、移動 replay 或相同 snapshot 不重播；舊 life 晚到結果不能重啟
   射擊／換彈／跳躍、改寫新 life HUD 或保留死亡姿勢。換 life 清除舊相位與掛點狀態。
7. HUD 顯示契約規定的武器／彈藥、生命與死亡／重生狀態；操作提示補上 R 與 Space。
   保留第 03 批的「CONNECTION POOR」移出警告與 Lobby 的移出原因顯示。
   延續既有捕捉、失焦、Tab、拖窗與離開行為，不從畫面回呼另送玩法動作；禁止把
   測試控制或網路實作細節加入一般使用者流程。

## 短驗證與完成條件

- 建置 Client 與必要 owner 測試；CPU 驗證新增 clip 能轉移至女性骨架、動作進度映射、
  合成姿勢與掛點一致，以及重複／晚到動作和 life 切換的呈現隔離。
- 依 D1 決定的平台，真雙 GUI 在 30／60／144 FPS 各跑有界短片段：連續點擊快射、按住不連發、空彈匣、
  換彈、移動中換彈、跳躍中射擊、著地、死亡中斷及重生。輸入、唯一動作、裁決、
  權威狀態、HUD 與成功 Presented frame 必須能對照，不能只看最終數字。
- 在 macOS 取得 GPU 圖像前，先以一次 capture 冒煙確認 Metal readback 與既有 Vulkan 結果的格式一致。
- GPU 圖像核對第一人稱 Reload／快射、遠端腿部與持槍上身同時動作、手槍握持、
  起跳／空中／落地、Death01 及新 life 姿勢；核對地面 anchor、深度層與視窗縮放。
  壓縮跳躍過渡及射擊動作需要實際畫面確認，不只證明 clip 名稱存在。
- 短故障片段涵蓋動作重送／重複結果、timeline hold、life 切換後舊結果、失焦／Tab／
  拖窗與離開重入；確認無重播、舊彈藥回填、屍體跟隨新玩家或待送的幽靈動作。
- 保存首個成功 Presented 的本機回饋證據與短時移動／射擊共存結果，沿用既有門檻；
  明確區分 GUI 功能核對、GPU captures 與延遲量測，不宣稱 input-to-photon 或完整長測。
- 確認 Client-only：Match／Gateway 不增加模型、SDL、Renderer、素材或 Campaign
  依賴；本批不更改 v5 wire／authority 語意。前批缺陷只作既定契約內的最小修正並留記錄。

## Architecture Delta 與停止點

本批觀測到的缺口是 Client 呈現介面只支援原有射擊，無法顯示已由 v5 確立的完整
動作及生命週期。最小改動是擴充產品 `WeaponViewModelFrame`、玩家呈現器與資產語意，
保留 Client → Engine 的取樣／繪製方向；權威 gameplay、動作交付及 ownership 不移動。
不把人物政策抽到 Engine，也不為讀取模型增加 product 或 Editor 依賴。

短驗證通過後更新 [README](README.md)、[HANDOFF](HANDOFF.md) 及本批 dev_log，
交付可試玩的 Client、對照證據與已知近似限制，然後停止。不自動開始第 05 批、
不提交 commit、不執行長測，也不將本批短測稱為 v5 穩定基線。

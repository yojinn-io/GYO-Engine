# 第 04 批：完整動作呈現

狀態：待使用者啟動。依賴第 02 批人物呈現及第 03 批全角色 v5 原子切換完成。
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

## 實作切片

1. 第一人稱射擊依 v5 契約的 **10 tick** 快射節奏呈現。沿用一次左鍵上升沿一次動作、
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
   延續既有捕捉、失焦、Tab、拖窗與離開行為，不從畫面回呼另送玩法動作；禁止把
   測試控制或網路實作細節加入一般使用者流程。

## 短驗證與完成條件

- 建置 Client 與必要 owner 測試；CPU 驗證新增 clip 能轉移至女性骨架、動作進度映射、
  合成姿勢與掛點一致，以及重複／晚到動作和 life 切換的呈現隔離。
- 真雙 GUI 在 30／60／144 FPS 各跑有界短片段：連續點擊快射、按住不連發、空彈匣、
  換彈、移動中換彈、跳躍中射擊、著地、死亡中斷及重生。輸入、唯一動作、裁決、
  權威狀態、HUD 與成功 Presented frame 必須能對照，不能只看最終數字。
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

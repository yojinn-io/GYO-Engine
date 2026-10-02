# v5 第 01 批：v4 基線、素材與測例清單

日期：2026-09-28。Owner：`object_fps_pvp`。這是 v5 開發起點，**不是 v5 驗收**。
現行 wire／玩法仍為 v4；v5 政策以 [契約](../../protocol-v5.zh-Hant.md) 為單一入口。

## 指紋與證據範圍

- 起始 HEAD：`edb6de4dfa179818ceeb9725712d29c288f18acd`，工作樹乾淨。
- v4 manifest：`build/target/_build/test/logs/pvp-v4-release-manifest.json`。
  SHA-256：`32267ea93ec8682addf13aafe9bbe08eea878f8e8508f953abe2fac403b47f9e`。
- 開始前逐一核對 manifest 的 **232 份來源／文件及 7 份產物，全部相符**。
  本批文件變更另行記錄，不回寫 v4 manifest，也不以新文件指紋冒充歷史跑次。
- 本批 ignored 證據：`build/target/_build/test/logs/pvp-v5-batch01-20260928/`。
  `baseline.json` 保存起始全部指紋；`cpu.log`／`cpu.xml` 保存短回歸；
  `completion.json` 保存結束時範圍、文件指紋及未改程式／產物的核對。
  提交 Markdown 不會保存 build 內證據，需另行保留。

| 產物 | SHA-256 |
|---|---|
| Client | `8cb197fdc5fd686a2ecb848b91ef3d6c1797ffd9b814fba89709f0a754fc8f29` |
| Match | `29f64b3cf1957cffbc6cda6ee3c324fa5eb0a0a741c148161f50554b8ee70ac5` |
| Gateway | `ff4e4c7d162caa59cb10c72c935e1566ec874ce2225396dd69c60859bd5db554` |
| action probe | `10763eb6a091f60ec7d08958fa238cc089303f03f867e07605c841bac1da89ef` |
| GUI probe | `1fd3563996852fb1c7eb820ded3f5c08c4e99dcea517842efdbe67a706179bf7` |
| 部署 asset_catalog.json | `3a047f84928df2aafaa207c0bdc37af68cb0918fe74cceddf144e32ce3b41fc4` |
| 部署 pvp_arena.json | `6f716a4efa68a173d26d916ccfd072fa735ee2c69453c63a68575440581b16ce` |

完整產物路徑記於 `baseline.json`。Client／Match 位於
`build/target/object_fps_pvp/bin/`，Gateway 位於
`build/target/_services/object_fps_pvp/bin/`，probes 位於
`build/target/_build/test/acceptance/object_fps_pvp/`。

[v4 穩定基線](../v4/STABLE_BASELINE.md)、[手動指南](../v4/MANUAL_ACCEPTANCE.md) 與
[驗收狀態](../v4/ACCEPTANCE_STATUS.md) 保持原文。歷史基線中的舊 HEAD／當時工作樹
描述屬於原跑次來源，不改寫成本批 HEAD。適用範圍仍是 Linux／X11／Vulkan、
同機雙玩家；新增人物後須取得新呈現證據，不能自動承接舊 GPU／效能認證。

歷史結果入口均在 `build/target/_build/test/logs/`：

| 路徑 | 用途 |
|---|---|
| `pvp-v4-release-gui-1/results.json` | 三輪完整 GUI 移動／射擊／HP；各輪原始指紋 |
| `pvp-v4-release-evidence-audit-1/qualification.json` | GUI 零干擾／重設及完整事件資格核對 |
| `pvp-v4-release-soak60-1/result.json` | 60 Hz、1,800 秒原始長測 |
| `pvp-v4-release-soak144-1/result.json` | 144 Hz、1,800 秒原始長測 |
| `pvp-v4-release-environment.json` | 原環境識別 |

原生視窗及失敗歷史入口見 v4 穩定基線。本批沒有重跑或重新宣稱上述驗收。
歷史整機停頓原因仍未證實，不能歸因於尚未加入的新人物。

## 現有程式與下一批接點

以下路徑相對 repository root：

| 位置 | 已核對的現況／交接 |
|---|---|
| `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` | 遠端以兩個方塊表示；本機已有 Mark23、滑鼠上升沿與 Presented 觀測 |
| `apps/object_fps_pvp/src/Pvp/Movement.cpp` | Client／Match 共用水平步驟，現有 constrainToFloor；第 03 批加入 Y 軸與跳躍 |
| `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp` | 固定步進、兩步 lead、ACK／epoch 恢復；加入垂直與生命狀態時保持原契約 |
| `apps/object_fps_pvp/include/RetroFPS/Pvp/SnapshotTimeline.hpp` | 遠端位置時間線；第 03–04 批增加生命區段與同時間線動作狀態 |
| `apps/object_fps_pvp/src/Pvp/PvpMatch.cpp` | 權威順序、射擊與帳本；第 03 批統一生命與武器狀態機 |
| `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp` | 獨立 worker、移動／動作窗口與可靠裁決；不能因重生清未 ACK 動作 |
| `apps/object_fps_pvp/src/App/CharacterPresentationDefinition.cpp` | PvP 自有人物 loader，目前尚未選入 Client 支援來源；第 02 批只接必要部分 |
| `apps/object_fps_pvp/src/App/WeaponViewModel.cpp` | 既有第一人稱資產／GPU 管線及產品 presentation frame 接口 |
| `apps/object_fps_pvp/CMakeLists.txt` | 只選 PvP domain／必要 App 支援；勿整包啟用複製的 Campaign 程式 |
| `apps/object_fps_pvp/protocol/`、`gateway/adapter/` | 目前兩份 v4 schema；第 03 批連同三角色版本／建置引用一次切換 |

Tick／Frame 的既有分離是固定步進累積器及獨立網路 worker；本機模擬與畫面仍共用
主執行緒。沒有把「固定 60 Hz」誤稱為獨立 Client 模擬執行緒。

## 素材清單與限制

所有正式載入路徑留在 `assets/object_fps_pvp/`。v2 僅供讀取參考，不形成 build、
runtime、test 或 packaging 依賴。第 01 批沒有新增或修改資產。

| PvP 內相對路徑 | 用途／限制 |
|---|---|
| `characters/superhero_female/Superhero_Female_FullBody.fbx` 及同目錄 textures | 已有女性人物、身高與腳底須對齊 1.8 膠囊 |
| `characters/hairstyles/buns/character.json`、`Hair_Buns.fbx` | 既有人物附件 |
| `characters/enemies/ranged.character.json`、`ranged.enemy.json` | 已校準 female／hand_r 掛槍的參考；玩家另建自有定義，不接 Enemy 生命周期 |
| `animations/ual_mannequin/UAL1_Standard.fbx` | Idle／Jog／Aim／Shoot／Reload／Jump／Death01 的來源骨架 |
| `weapons/ultimate_pistol_1/world/character.json`、`Pistol_1.fbx` | 第三人稱世界手槍 |
| `weapons/mark23/viewmodel/Mark23.fbx`、`mark23_viewmodel.json`、`viewmodel.animset.json` | 第一人稱手臂／槍模及動畫映射 |

UAL 使用 `Armature|` clip 前綴。Pistol_Idle_Loop 約 1.667 秒、Jog_Fwd_Loop
約 0.933 秒、Pistol_Shoot 約 0.633 秒、Pistol_Reload 約 1.667 秒、Death01
約 2.4 秒；Jump Start／Land 原長約 1.333／1.267 秒。Mark23 原始 Reload
約 3.733 秒。這些是素材時長，**不是玩法參數**；裁決與補彈由 Match Tick 決定。

UAL 沒有專用側移／後退持槍循環；後退反向 Jog、側移近似已列契約。v2 參考實作
按時間前進動畫，未提供要求中的位移步頻匹配，不能整段照搬。現有 Engine Model
可 SamplePose／BlendPoses 與取得 local/global transforms；骨骼遮罩組合留產品內，
不為此增加通用動畫框架或 IK。詳細資產與新建內容見 [第 02 批](02-player-model-and-locomotion.md)。

## 測例清單與有界短回歸

第 01 批只執行現有回歸：目標 `gyo_object_fps_pvp_tests` 無需重建；6／6 CTest
成功，共 9.88 秒。命令見 [第 01 批](01-contract-and-baseline.md)。

| 測試 | 本批結果 |
|---|---|
| `object_fps_pvp.cpu` | 97 cases／1,390,416 assertions 通過，約 1.33 秒 |
| `object_fps_pvp.action_evidence` | 12 tests 通過，約 0.21 秒 |
| `object_fps_pvp.presentation_evidence` | 16 tests 通過，約 7.04 秒 |
| `object_fps_pvp.command_evidence` | 31 tests 通過，約 0.99 秒 |
| `object_fps_pvp.recovery_relay` | 2 tests 通過，約 0.10 秒 |
| `object_fps_pvp.backpressure_evidence` | 2 tests 通過，約 0.19 秒 |

待各批實作的驗收責任：

| 批次 | 必要案例與不應誤用的舊假設 |
|---|---|
| 02 | 真雙 GUI 人物比例／握槍／步頻／貼牆／時間線保持；30／60／144 FPS，不提前改 v4 HP 與射速 |
| 03 | 共享 3D 步驟、跳躍邊沿與頂頭／落地、窗口／重播、10 Tick／12 發／90 Tick、四槍死亡／180 Tick 重生、出生點、跨生命裁決與真網路恢復 |
| 04 | 下一成功 Presented 射擊、上下身移動瞄準、Jump／Reload／Death 按權威時間、舊結果不污染新生命、恢復不補播動作 |
| 05 | 射擊／換彈／死亡快照遺失及生命切換停頓、HP／彈藥／裁決守恆、原生 Space／R／Tab／拖窗／Esc／Lobby、版本指紋與完整驗收入口 |

`tests/object_fps_pvp/CombatTests.cpp` 明確測試 **HP=0 仍可移動／射擊／被命中**，
並測 20 Tick 冷卻；v4 的無限彈藥長測也持續射擊。第 03 批須用新生命／彈藥／
冷卻狀態機案例明確取代這些斷言；不能刪測例後假稱 v5 已通過。既有移動、
背壓、ACK／epoch 故障與 latency 門檻保留。第 05 批另建 v5 手動指南／回報表，
不改舊 v4 指南來迎合新結果。

本批未跑 Go race、shader、GPU、真 socket 矩陣、GUI 或長測；它們依對應批次
執行，最終完整驗收另行授權。沒有重建或啟動試玩服務。

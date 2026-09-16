# 2026-09-15：由 Mark-23 模型校準槍口

## 目的與狀態

讓射擊起點跟隨實際槍管位置，並讓射擊當幀的曳光從畫面中的槍口開始。
準星決定瞄準點；物理射線決定傷害；曳光呈現這次已解析的射擊。

Windows Debug 整合建置、三種比例的 GPU 投影驗證、獨立 gameplay 測試
與可移除模組建置已通過。整合 18 項檢查均已完成；包含 headless 重建
後的重跑驗證。完整證據列於本文末尾。

## 從模型取得槍口

取槍管內側 18 個頂點形成的圓環中心，換算為 `main_j` 局部座標：

| 項目 | 值 |
|---|---|
| 槍口節點 | `main_j` |
| 局部 x | `0.003179880` m |
| 局部 y | `0.089686641` m |
| 局部 z | `-0.213802223` m |
| 射擊起點取樣 | `Shoot` 片段局部時間 `0` |
| 固定定位基準 | `Idle` 的定位節點 |

槍管屬於 `main_j`；`side_j` 是會往復移動的滑套節點。把槍口綁在
`side_j` 會讓它跟著滑套後退。圓環中心在指定節點的局部座標中保存，
套用節點姿勢後才進入整體模型空間，不能直接當作相機或世界座標。

## 資料與責任邊界

共用的 `LoadWeaponPresentationDefinition` 同時供模型呈現與槍口校準使用。
它從 `mark23_viewmodel.json` 讀取模型、材質／片段映射、槍口節點與
局部點、固定 Idle anchor，以及 offset、rotation、scale、武器 FOV。

| 責任 | 所屬 |
|---|---|
| 節點階層、片段取樣與矩陣變換 | `GYO::Model` |
| 模型／JSON 資產解析與生命週期 | GYO Asset 與遊戲的共用載入流程 |
| 槍口節點、局部點、武器構圖與 FOV 對應 | Object_FPS presentation content |
| 每把武器的純數值 `WeaponShotGeometry` | Object_FPS `CampaignContent` |
| 瞄準、遮擋、後座力、傷害與曳光時序 | Object_FPS gameplay |
| 世界／ViewModel 的提交與深度合成 | GYO Render |

載入階段完成模型取樣及座標轉換後，將每把武器的 `WeaponShotGeometry`
交給遊戲。Gameplay 只消費數值，不持有模型資產、pose、GPU handles，
也不從 renderer 查詢槍口。通用 Model／Render 不認識 Mark-23、準星或
扣血規則。

## 校準的座標流程

以 `p_local` 表示 JSON 中的槍口點，`N_shoot0` 表示 Shoot 起點的
槍口節點模型矩陣，`a_idle` 表示固定 Idle anchor 的模型空間位置：

```text
p_model = N_shoot0 * p_local
p_view  = offset + rotation * (scale * (p_model - a_idle))
```

這和武器頂點呈現使用相同的定位基準及 placement。Anchor 只由 Idle
求一次，不隨每個動畫姿勢重新置中；槍口則明確取 Shoot 零點，符合
本遊戲在射擊動作開始解析命中的規則。

世界相機的垂直 FOV 是 60°，武器相機是 55°。直接把 `p_view` 當作
世界相機座標，投影後會偏離畫面中的槍口。兩相機使用相同長寬比時：

```text
ratio   = tan(worldFov / 2) / tan(viewModelFov / 2)
p_world_camera = (p_view.x * ratio, p_view.y * ratio, p_view.z)
```

x／y 使用正切比例轉換，z 保持不變；這使兩組投影得到相同的螢幕位置。
計算三角函數時 FOV 轉為弧度。此步在 placement 之後執行，不對點做
方向正規化，也不額外調整深度。最後再用玩家相機的 right／up／forward
基底與位置轉成世界座標；俯仰、轉向及跳躍高度都沿用同一套相機資料。

## 射擊當幀的時序

物理命中與曳光的相機取樣時間有明確分工：

1. 接受一次射擊後，使用尚未加上「本次新增後座力」的相機射線取得
   準星瞄準點。此前已存在的後座力仍屬於玩家當下的瞄準姿態。
2. 從校準後的物理槍口執行槍口到瞄準點查詢。相機到槍口先做世界
   線段限制，遇牆／地板時退至碰撞前，保留正面牆、薄牆及轉角遮擋。
   本次傷害在此解析一次，扣彈仍由原本武器規則管理。
3. 本次後座力更新後，以實際用來呈現該幀的相機重新求曳光槍口位置。
   同時確保已存在的 projectiles 完成該幀更新，才建立新的 cosmetic
   tracer；新曳光的出生幀留在槍口，不立刻多走一個 delta time。
4. 曳光朝已解析的命中點呈現，起點的牆面限制與視覺路徑裁切仍生效。
   它不能穿過真正遮擋路徑的牆面，也不重新查詢／結算目標傷害。

如此可避免新後座力改變本次準星命中，以及曳光出生後立即前進造成
「子彈從槍口前方出現」的視覺斷裂。武器使用獨立深度呈現，並不授予
物理射擊或世界曳光穿牆能力。

## 調整與部署

- 調整 `assets/object_fps/data/mark23_viewmodel.json` 的 offset、rotation、
  scale 或武器 FOV，再建置以同步部署資產並重新啟動。共用載入流程
  會重新計算槍口，無須另改 gameplay 中一組手填偏移值。
- 更換模型或槍管幾何時，重新測量槍口的節點局部點並核對節點名稱。
  只有構圖改變時，原有局部點仍有效。
- Shoot／Reload／Draw／Hide 的 CSV 時序與傷害、彈藥設定維持各自責任；
  改槍口位置不改變動作長度或射速。
- 本設計延續先前的相機到槍口遮擋修正，取代其手填虛擬槍口偏移。
  [換彈與貼牆修復紀錄](2026_09_15_reload_muzzle_fix.md) 保留當時的診斷
  與驗收；目前槍口來源及曳光時序以本紀錄為準。

## GPU 診斷與重現

`--muzzle-smoke-test` 在 16:9、4:3、21:9 各執行七組相機條件：正前方、
水平轉向、仰視、俯視、跳躍、射擊後座力首幀及世界 FOV 75°。
每組分別繪製世界空間槍口標記與模型槍口標記，再量測白色像素的重心。

模型側使用原始骨骼槍口點減去固定 anchor，再讓真實 GPU placement
矩陣處理 scale／rotation／offset；世界側使用 gameplay 的純數值幾何。
因此可獨立比較 CPU 與 GPU placement，不只是重複呼叫同一個 CPU 公式。
兩組 GPU 重心及各自與理論投影點的距離，都必須小於 1 pixel。

在 repository 根目錄使用目前的 Debug 建置，可執行：

```powershell
$muzzleExe = '.\build\object-fps-ui-vs18\apps\object_fps\Debug\gyo_object_fps.exe'
& $muzzleExe --muzzle-smoke-test --capture-dir '.\build\muzzle-calibration\16x9'
& $muzzleExe --muzzle-smoke-test --preview-4x3 --capture-dir '.\build\muzzle-calibration\4x3'
& $muzzleExe --muzzle-smoke-test --preview-21x9 --capture-dir '.\build\muzzle-calibration\21x9'
```

CTest 對應 `object_fps.muzzle_smoke`、`object_fps.muzzle_smoke_4x3`、
`object_fps.muzzle_smoke_21x9`。`--capture-dir` 可省略；指定時每種比例
輸出 14 張白色標記 BMP 和一張 `muzzle_model_review.bmp`。
青色模型標記維持相同投影、暫時移近相機以便辨認，僅用於診斷。
一般遊戲不會顯示標記，也不會等待這些 GPU readback。

## 驗收結果

| 項目 | 結果 |
|---|---|
| Windows／VS 18 Debug 所有 target 建置 | 通過 |
| 可移除模組建置 | `model-neutral-check` 停用 Object_FPS 與 FBX adapter；CTest **5/5**，4.79 s |
| 獨立 gameplay/domain 測試 | g++ 執行 **862 assertions，0 failures** |
| 三種比例 GPU 槍口驗證 | 三個 CTest 均通過；每種比例七組相機條件 |
| GPU 標記重心差距 | 最大 **0.0000 pixel** |
| GPU 重心與理論投影差距 | 最大 **0.3823 pixel**，小於 1 pixel 門檻 |
| 模型影格檢查 | 青色標記位於槍管前端開口，已離線檢查 |
| 不同工作目錄啟動 | 從 `D:/code/Source` 執行部署版 muzzle smoke、使用絕對 capture 路徑，通過 |
| 部署 JSON | 已包含新的 `muzzle.node` 與 `muzzle.local_position_meters` |
| 整合 CTest | **18 項檢查均已完成**；全量執行通過的 17 項，加上 headless 重建後重跑通過 |
| 整合 headless 重跑 | **1/1 通過，1,169 assertions**，1.84 s |

本次使用 Visual Studio 18 隨附的 CMake／CTest，對已配置的目錄執行：

```powershell
cmake --build build/object-fps-ui-vs18 --config Debug --parallel 1 -- /nr:false /v:quiet
ctest --test-dir build/object-fps-ui-vs18 -C Debug --output-on-failure
cmake --build build/object-fps-ui-vs18 --config Debug --target object_fps_headless_tests --parallel 1 -- /nr:false /v:quiet
ctest --test-dir build/object-fps-ui-vs18 -C Debug --rerun-failed --output-on-failure
cmake --build build/model-neutral-check --config Debug --parallel 1 -- /nr:false /v:quiet
ctest --test-dir build/model-neutral-check -C Debug --output-on-failure
```

18 項整合結果由全量執行與 headless 重跑合併確認；headless 重跑前
重建測試 target，其餘 production code 維持同一版本。

獨立 domain 測試涵蓋相機旋轉／俯仰／跳躍／FOV 下的出生槍口、新後座力
前的命中、曳光出生幀不前進、單發傷害與曳光消失後不重複傷害，以及
既有貼牆、正面牆、薄牆及轉角規則。GPU 標記驗證專注於模型與世界的
投影對齊，不取代這些實際 GameSession 時序與傷害測試。

整合 headless 另核對真實模型的 18 個槍管內環頂點、共用校準載入、
JSON placement／FOV 變更、無效槍口設定拒絕，以及非預設世界 FOV
由 snapshot 傳入 presentation 相機。

本次輸出的 [16:9 模型與青色槍口標記](../../build/muzzle-calibration/16x9/muzzle_model_review.png)
供離線複核；影格位於本機 build 目錄，重新建置／清除產物後可用以上
指令重新產生 BMP。影像是 scene readback，尚未套用曝光／Gamma 或 HUD。

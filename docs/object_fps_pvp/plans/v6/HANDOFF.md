# PvP v6 交接

更新：2026-10-02。Owner：`object_fps_pvp`。**v6尚未開始，沒有計畫、契約或分批。**
本文件收集v5期間使用者決定「延到v6處理」的項目，作為v6規劃的起點；開始v6時先讀本文件，
再依[v5穩定基線](../v5/STABLE_BASELINE.md)（2026-10-03升格）與[v5交接](../v5/HANDOFF.md)確認v5的最終狀態。這些項目都不是v5的未完成工作，v5不會等它們。

每項記錄：來源、現況與證據、已知的資料或素材、當時的判斷。數字未標「實機」者為CPU模擬或靜態分析。

## 延後項目

### 1. 遠端人物上半身的俯仰瞄準

- 來源：v5第04批L3人工驗收（2026-10-02，使用者）：A玩家抬頭看天時，B玩家畫面中的A沒有任何變化。
- 現況：`PlayerPresentation`的上半身只依yaw轉身，持槍姿勢固定；`PlayerState.pitch`已在Snapshot與時間線取樣中
  （`SnapshotTimeline`會內插pitch），所以**不需改wire**。
- 這是v5契約§6「上半身：明確產品骨骼遮罩組合持槍／瞄準」的缺口，在v5第04批未實作。
- 素材：`UAL1_Standard.fbx`有`Armature|Pistol_Aim_Up`／`Pistol_Aim_Neutral`／`Pistol_Aim_Down`（各0.167秒），
  可依pitch在三個姿勢間混合後作為上半身基底，再疊射擊／換彈。
- 需決定：pitch到姿勢的映射範圍、射擊／換彈時是否保留俯仰、死亡時忽略。

### 2. 受擊反應

- 來源：同上（使用者）：被擊中沒有反應。
- 現況：v5契約與第04批計畫都不含受擊反應。Snapshot只有`CombatState.hp`，**沒有受擊時點**；裁決結果只送給射擊者。
- 素材：`Armature|Hit_Chest`（0.333秒）、`Armature|Hit_Head`（0.433秒）。
- 需設計：受擊時點的來源（新增wire欄位，或由同一生命的HP下降在時間線上推導）、遠端人物反應、第一人稱回饋
  （畫面閃紅、鏡頭晃動、HUD），以及與死亡動畫的優先順序。若改wire，屬契約變更與Architecture Delta。

### 3. Engine渲染阻塞主迴圈（macOS）

- 來源：v5第04批L3人工驗收中使用者看到`CONNECTION POOR`。
- 證據：`build/target/_build/test/logs/pvp-v5-batch04-20261002-manual-2/client-2.log`（git忽略）：
  三次render約1199ms、兩次約700–785ms，每次緊接視窗互動（釋放指標）。第一組`-manual/`另有一次1134ms。
- 原因：`engine/render/backend/sdl_gpu/src/SdlGpuRenderDevice.cpp`以阻塞的`SDL_WaitAndAcquireGPUSwapchainTexture`
  取得swapchain；Metal在視窗拖動／縮放時取得drawable最多等約1秒。主迴圈停住期間Client不產生移動命令，
  1.2秒約72 Tick被Host以Held替代，占10秒判定窗口12%（門檻5%），該窗口不合格而顯示警告。
  警告判定正確；移出需要連續3個不合格窗口，單次不會移出。
- 方向（未評估）：改為不阻塞的`SDL_AcquireGPUSwapchainTexture`（拿不到就跳過該幀呈現），或讓固定步模擬不受
  呈現阻塞。屬Engine層，影響所有產品，須以Architecture Delta提出並跨平台驗證。
- v5的決定（2026-10-02）：先記錄為已知問題。

### 4. 本機射擊冷卻閘的落差

- 來源：v5第04批04-3開發實跑。
- 現況：Client以最新Snapshot的Tick比對`nextAllowedShotTick`，該Tick約比權威晚2 Tick；間隔剛好12 Tick的點擊在
  幀抖動下會在本機被擋（第03批「冷卻點擊不排隊」設計）。權威的10 Tick冷卻本身正確。
- 需決定：本機閘門是否以估計的權威Tick補償落差，或維持保守；屬操作手感，與防止送出必被拒絕的請求之間取捨。

### 5. 驗收工具清理：`weapon_short`與v4合法射擊長測

- 現況：`build/acceptance/object_fps_pvp/weapon_short.hpp`的觀察方仍檢查v4「HP=0仍可移動與射擊」，在v5必然失敗；
  其v5涵蓋已由第04批的動作短測（`run_action_short.py`）取代。
- 同類（v5第05批05-1發現）：`run_action_legal.py`與probe的`--legal-shots`模式、`action_evidence.analyze_legal`假設
  「全部接受、無限彈藥」，v5第13發起即為空彈拒絕；v5長測已改用`run_gameplay_soak.py`。
- 需決定：刪除這些模式或改寫為v5語意。

### 6. 自己死亡時的持槍手臂（待使用者釐清）

- 來源：v5第04批L3（使用者）：「自己死亡時，持槍手臂還是在。」
- 現況：Client在死亡時不提交第一人稱武器（`PvpApplication.cpp`的`SubmitWeapon`在`!Alive()`時跳過）；
  遠端畫面中屍體依Death01全身動畫倒地，世界手槍仍掛在手上（v5第04批的設計）。
- 待釐清：使用者看到的是第一人稱手臂（若是則為缺陷，須重現）或對方畫面中屍體手上的槍（若希望掉落或隱藏則為新需求）。

### 7. 驗收紀錄計時器基線（使用者2026-10-03決定延到v6）

- 來源：v5第05批05-5的60 Hz長測失敗。headless probe以`sleep_until`跑60 Hz迴圈，macOS常晚醒3～8 ms
  （空迴圈實測：P99 20.5 ms、超過18 ms約22–25%；probe加上工作後4.6%的幀超過21.7 ms）。
  重生首幀時間重設的配對規則原本假設「幀長≤1/60秒＋5 ms」，113次重生撞到3次而誤判。
- v5的處理：配對改為結構條件（同epoch、緊跟LifeRespawn、掉時恰為幀長−1 Tick、幀長<100 ms），不再依賴計時精度。
- v6待做：probe開始時量一次計時器抖動（例如數秒的空60 Hz迴圈分布），寫進平台指紋與報告，**只用來解讀結果，不當門檻**；
  讓不同平台的時間抖動可以直接和產品問題區分。原則：不依OS名稱分支，以實測能力解讀。

### 8. 測試與驗收器的跨平台相容性稽核（2026-10-03，使用者要求的全面檢查）

- 原則（使用者2026-10-03討論定案）：對玩家的門檻跨平台相同、不因平台放寬；測量工具對主機計時精度或視窗／GPU行為的假設，
  優先改成結構條件，非得用時間常數時以實測基線解讀（第7項），不依OS名稱分支；平台對玩家的影響（如第3項）屬產品問題。
- v5已處理：重生首幀時間重設的配對規則（改為結構條件）。其餘項目在macOS的v5驗收中都通過，未有失敗證據，
  依「實際壓力才改」不在v5變更（改了也會使已取得的驗收指紋失效）。下表依風險排序，數字為子代理稽核所列位置（`build/acceptance/object_fps_pvp/`為BA）。
- 已有失敗紀錄：
  - BA/`player_presentation_evidence.py:101-103`：人物短測達成FPS須≥名義×0.85；144 FPS需≥122.4。v5第04批join只到120 FPS而失敗（當時判為GPU容量並保留）。
    同檔`:122-139`的腳本時窗也依賴事件準時。
- 高風險（依主機喚醒精度或以接收端時間量間隔）：
  - BA/`worker_main.cpp:336-352`：1秒內重送55–65次、60–70次嘗試；`:219,340,366`以接收端`Pump()`時間要求間隔≥30／≥14 ms（只剩2.7 ms餘裕）。
  - BA/`worker_main.cpp:230-234`、`action_probe.py:402-412`、`gameplay_evidence.py:202-206`：以relay接收時間做1秒滑動窗口（≤120包、≤31動作／結果），relay執行緒停頓會把封包擠在一起。
  - BA/`action_short.hpp:150-158`：射擊間隔0.25秒對10 Tick冷卻、換彈只留200 ms；某幀晚83 ms以上即失敗。
  - BA/`gui_main.cpp:657-662`（產生命令數只容1 ms搶占）、`:932,1004`（穩定幀≤40 ms）、`:571-686`（`SDL_Delay(83)`注入停頓逼近補步上限）。
  - BA/`presentation_evidence.py:16,130`：遠端交越不得早於本機20 ms以上；60 Hz加晚醒的幀可達約25 ms。
  - BA/`run_timing.py:69-105`：計次GUI輪任一OS視窗事件即無效；macOS對`SDL_RaiseWindow`的非同步啟用事件若晚到，會被計入。
  - `tests/common/render/sdl_gpu/MeshUpdateSmoke.cpp:237-249`：16幀都須Presented；視窗被遮蔽或headless GPU會回Skipped。
  - `tests/common/ci/test_package_checks.py:75-80`（0.2秒內Python子程序須啟動）、`tests/object_fps/package_tools/test_gpu_smoke.py:55-60`（0.5秒）。
  - `apps/object_fps_pvp/gateway/backpressure_test.go:102-158`：60 Hz `time.Ticker`在250 ms內須≥12次，並依100 ms間隔判定恢復。
  - `tests/common/core/asset/AssetWatcherTests.cpp:39-66`：20 ms後改寫同大小檔案，依賴檔案系統mtime精度（FAT 2秒）。
- 說明（非缺陷，但解讀時要知道）：各分析器的「≥100 ms停頓」實際約83 ms即可觸發，因為Client每幀最多補5步（`LocalPlayerPrediction.hpp:73`），
  剩餘時間加上該幀達100 ms就開始捨棄，`dropped_seconds>0`也算停頓。v5長測最長幀87 ms而未掉時，屬接近邊界。
- 產品門檻（50／66.7／80／100／150 ms、≥99%、恢復1.5秒、窗口與包率上限）不在此列，維持跨平台相同。

### 9. 未排程的既有候選（v5文件已記載）

- CS式開局／回合準備期：全員凍結、無敵、倒數；需Match回合狀態、無敵規則、HUD倒數與契約變更。見
  [v5 fix/02](../v5/fix/02-a1-cancelled-by-stall-reseed.md)。
- 相位估計器在WAN雜訊下的穩定性：非對稱抖動、相關突發、佇列、亂序、Wi-Fi競爭下P90估計是否穩定；
  先收集真實餘裕樣本分布再決定。見[延遲整改回顧v2](../v5/LATENCY_CASE_STUDY_v2.md)第6節。

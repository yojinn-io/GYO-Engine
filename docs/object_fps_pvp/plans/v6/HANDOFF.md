# PvP v6 交接

更新：2026-10-02。Owner：`object_fps_pvp`。**v6尚未開始，沒有計畫、契約或分批。**
本文件收集v5期間使用者決定「延到v6處理」的項目，作為v6規劃的起點；開始v6時先讀本文件，
再依[v5交接](../v5/HANDOFF.md)確認v5的最終狀態。這些項目都不是v5的未完成工作，v5不會等它們。

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

### 7. 未排程的既有候選（v5文件已記載）

- CS式開局／回合準備期：全員凍結、無敵、倒數；需Match回合狀態、無敵規則、HUD倒數與契約變更。見
  [v5 fix/02](../v5/fix/02-a1-cancelled-by-stall-reseed.md)。
- 相位估計器在WAN雜訊下的穩定性：非對稱抖動、相關突發、佇列、亂序、Wi-Fi競爭下P90估計是否穩定；
  先收集真實餘裕樣本分布再決定。見[延遲整改回顧v2](../v5/LATENCY_CASE_STUDY_v2.md)第6節。

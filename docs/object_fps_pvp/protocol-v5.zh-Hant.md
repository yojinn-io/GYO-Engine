# PvP Protocol v5：人物、跳躍與生命循環契約

更新：2026-09-28。Owner：`object_fps_pvp`。
**Client／Gateway／Match 已一起實作 v5 候選；第03批功能及網路恢復通過，整批驗收未結案。**
可見延遲50ms守門尚有啟動相位問題，詳見交接；本契約參數及門檻沒有因此放寬。
第01批建立契約，第02批完成女性人物Idle／Jog／掛槍；第03批接入v5 wire、
跳躍、彈匣、換彈、死亡／重生及操作／HUD。完整動作動畫仍留第04批。
實作／停止點見 [v5進度](plans/v5/README.md)。本候選尚未完成v5完整驗收，
不取代原 [v4穩定基線](plans/STABLE_BASELINE.md) 的歷史指紋與認證範圍。

## 1. 範圍、時間與預設

本輪完成移動／跳躍、單發手槍／換彈、死亡／重生與人物骨骼呈現。
不包含近戰、收槍、蹲伏、衝刺、多武器、拾取、爆頭、骨骼命中、射擊回溯、
布娃娃、持久屍體、觀戰、角色選擇或回合勝負系統。

| 政策 | v5預設／來源 |
|---|---|
| Authority／本機固定模擬 | 60Hz；固定1/60秒，不受FPS或封包數驅動 |
| 移動worker／Snapshot | 各60Hz；保持既有獨立傳輸排程 |
| 初始／重同步lead | 2個中立命令，第一個合法固定步形成後才首次發布完整窗口 |
| 移動窗口／Match未來命令 | Client≤12；Match數量與距游標皆≤32 |
| 動作交付 | 每批≤8，未退休窗口與ID距離≤32，動作／ACK合計排程30Hz |
| 遠端插值／本機校正 | 落後1Tick；小校正100ms消除，誤差≥1世界單位直接定位 |
| 封包／限流 | 完整UDP≤1200 bytes，含24-byte頭；既有120包／秒限制不變 |
| 水平／身體 | 沿用Arena：速度3、身高1.8、半徑0.25、眼高1.6 |
| 跳躍 | 高度0.6、重力18、初速度sqrt(2gh)；Space上升沿 |
| 生命 | 滿血100，每槍25，180Tick／3秒自動重生；無重生無敵 |
| 手槍 | 半自動，10Tick／約167ms間隔，射程100 |
| 彈藥／換彈 | 彈匣12、無限備彈，R換彈90Tick／1.5秒；不自動換彈 |
| 動作有效期 | 保留250ms權威發布參考年齡上限，不接受Client時間／位置／傷害 |

由Match擁有並經Ready／Welcome發布規則：移動規則包含jumpHeight／gravity，
戰鬥規則包含HP、傷害、射速、彈匣、reloadTicks、respawnTicks與既有射擊限制。
Client／Match共用產品純移動步驟；Client使用已驗證的權威規則。CSV、FBX時長、
動畫播放率與UI不能成為第二份玩法規則。Arena既有設定與認證繼續有效。

`SimulationTick`是世界步數；`FrameId`是Client迴圈／呈現識別；
`InputSequence`是某movementEpoch內的單步輸入；`PacketSequence`是傳輸識別；
`ActionId`是Session內動作識別；`lifeGeneration`是同一玩家的生命世代。
這些量不互相替代。固定Tick／Frame語意分離不要求新增Client模擬執行緒。

## 2. 身分與資料語意

| 資料 | v5增補與規則 |
|---|---|
| MovementCommand | 新增單次jumpRequested；仍只代表固定一步，沒有任意dt或Client位置 |
| PlayerInput | 完整未確認窗口帶player身分、movementEpoch及單一lifeGeneration |
| PlayerState | 既有位置／角度／ACK，加verticalVelocity、grounded、lifeGeneration、Alive／Dead及生命轉換Tick |
| CombatState | 既有HP／冷卻，加magazineAmmo、reloadActionId／起訖Tick、最近接受射擊ActionId／Tick；同份Snapshot按PlayerId關聯生命 |
| ActionRequest | ActionId、lifeGeneration、observedAuthorityTick與Shot／Reload種類；只有Shot包含絕對yaw／pitch |
| ActionDecision | 動作種類、原請求生命世代、ActionId、裁決Tick、接受／拒絕；Shot結果另含命中kind／targetId／targetLifeGeneration／damage |
| WorldSnapshot | 同一Authority Tick的完整玩家、戰鬥與生命狀態；不傳骨骼矩陣或完整模型pose |

以上是產品契約語意。實作保留歷史C++／proto名稱`ShotRequest`、`ShotDecision`、
`ShotRejection`及`ActionBatch.shots`，以明確`ActionKind`區分Shot／Reload；名稱不代表
只有射擊，也不保留v4 wire相容。wire kind／life enum的0為無效sentinel，與C++值
採明確轉換；Shot的yaw／pitch必須存在，Reload不得攜帶這兩欄。
兩份Client／Runtime協定獨立，經產品Adapter轉譯。HTTP join、Ready／Welcome、
wire版本與建置引用同批切換，明確拒絕v1–v4，不交付混用三角色的可部署組合。

lifeGeneration從1開始，只由Match在成功重生時遞增；PlayerId／Session不變。
重生同時提升movementEpoch並歸零其命令序號，兩種世代各自有意義且不回繞。
一般積欠／耗盡恢復只改movementEpoch，不改生命、HP、彈匣或動作帳本。
世代／序號耗盡必須明確失敗，不透過回繞復用身分。

每個ActionRequest各帶lifeGeneration，**不能只放在ActionBatch頭**：同一重送
窗口可同時包含舊生命未確認與新生命動作。ActionId於同Session持續遞增，
重生不歸零；只有Leave／斷線／新Session按既有生命週期清理整條動作流。

可替換的Snapshot繼續latest-wins；動作裁決不可因此丟失。已開始的TCP frame
必須寫完或失敗，未知動作结果不得因超時或重生靜默取消。動畫提示是Snapshot
中可替換的短暫呈現狀態，不是對每個遠端動作逐一播放的可靠事件通道。

## 3. 跳躍、命令與重播

- Space上升沿可等待本幀之後第一個合法固定步；該步消費一次，同幀其餘步不帶
  邊沿。按住不連跳；空中請求在該步消費並無效果，不保存到落地。
- 窗口滿、死亡、失焦、身分／世代變更或重同步清除尚未分配的跳躍請求。
  已發布命令保持不可變，不能為了清按鍵修改重送內容。
- Client／Match以固定1/60秒解析積分重力，掃掠完整膠囊、處理頂頭與向下支撐。
  保留現有pitch不影響水平速度的行為、靜態牆碰撞及不做玩家互撞的範圍。
- Actual按收到的命令執行；Held沿用最近實際執行命令的持續軸／角度，但清除
  jumpRequested；Neutral亦無跳躍。重送不刷新Held期限或增加世界步数。
- 延續既有Running／AwaitingFirstCommand／ResetBoundary命令游標政策；
  Running每Tick恰好一步。缺命令或死亡不把重力當作可重複的輸入事件；死亡的
  正常Running步仍執行垂直中立物理。重設邊界保持原先不執行移動的規則。
- 垂直速度、grounded與Y位置一同還原／重播；不能只校正腳底高度而保留舊速度。
  校正與呈現碰撞解除原本的平面限制，保護地板、牆、牆角與天花板。
- 最新生命狀態是權威限制。收到Dead後，舊移動歷史不能讓重播重新啟用水平移動
  或跳躍；死亡期間正常生成中立命令，保持游標、保活及重送流程。

## 4. 動作裁決與彈匣

沿用Host原子批次接納、不可變請求、重複無效果、有界帳本、可靠裁決與連續ACK。
格式／衝突／容量錯誤不能部分接納shots或ACK。合法格式的生命不符、Dead、
Reloading、EmptyMagazine、MagazineFull等玩法失敗，必須產生可ACK的終局拒絕，
不能只在Gateway／Host丟掉後讓Client永久重送。

拒絕判斷按：生命不符、死亡、既有參考Tick／有效期驗證，然後各動作玩法限制。
過去生命明確標為StaleLife，未來生命標為InvalidLife；它們都沒有遊戲副作用。
Shot先判換彈中、空彈匣，再判10Tick冷卻；Reload先判已換彈，再判滿彈匣。
身分無效／格式錯誤仍走原接納錯誤路徑，不創造可偽裝他人身分的裁決。

| 狀態／操作 | 結果 |
|---|---|
| 合法Shot | 扣1發、記錄冷卻與射擊呈現識別，再做當前權威膠囊命中；打牆／射偏亦扣彈 |
| 拒絕／重送Shot | 零扣彈、零傷害、零新增動畫事件 |
| 合法Reload | 當Tick記錄開始與完成Tick，不立即補彈；缺少的彈數不影響固定時長 |
| Reload進行中 | 可移動／跳躍；射擊拒絕、重複換彈不延長、不補彈 |
| Reload完成 | 在start+90的Tick開始補滿一次並結束換彈；當Tick之後的合法Shot可執行 |
| 死亡 | 取消未完成Reload，不補彈；生命狀態及到期重生Tick進入Snapshot |

Client每幀先更新一次絕對視角，最多建立一個Shot／Reload動作；R與左鍵同幀優先R。
窗口不可接納時不配置ActionId。冷卻／換彈中的點擊丟棄，不形成稍後自動射擊。
Space與R／Shot為不同輸入語意，可以同幀跳躍加射擊或換彈。

本機Shot即時播一次預期回饋；Reload即時開始預期呈現，權威接受後對齊起訖Tick，
拒絕則取消。Client可限制已提交但未確認Shot以避免預期彈藥超支；正式彈藥／HP
顯示仍以最新Snapshot為準，不能把晚到裁決當作直接覆寫新快照的狀態。

## 5. 死亡／重生與Tick順序

```text
Authority Tick
  生命周期交接與ACK
    -> 到期重生／換彈完成
    -> 全部玩家固定移動（Dead只保留中立垂直物理）
    -> 按既有 acceptedTick、PlayerId、ActionId 逐一裁決動作
       （每次致命傷害後立即轉Dead，供下一個動作判斷）
    -> 完整Snapshot
```

- HP歸零當下立即轉Dead，記錄deathTick與respawnTick=deathTick+180，取消Reload。
  同Tick後續動作已看見此狀態：死者不能再射擊，死者亦不再是命中目標。
  不以Tick開始時的一份可命中目標快取允許額外死後命中／互殺。
- 死亡後不能控制位置／視角／武器；本機保留最後視角，顯示權威倒數，Esc／Tab
  仍有效。若屍體在空中，重力使其落地；人物死亡動畫不改權威膠囊。
- 到期後先過濾不能放置膠囊的出生點，再取距存活對手最遠者，距離相同按Arena
  配置順序；Dead不阻塞出生點。無存活對手時取第一個可用點。
- 全部受阻則保持Dead與倒數零，顯示等待出生點，每Tick重試；只在成功時提升
  lifeGeneration／movementEpoch。同Tick多人重生按PlayerId順序，後者看見前者。
- 成功重生設出生位置／朝向、grounded=true、垂直速度0、滿血滿彈、清冷卻與
  Reload，清舊移動／替代輸入／積欠統計。Client以新權威狀態重建兩步lead。
- **不清動作帳本、待ACK結果或ActionId。** 已完成結果保持不可變；未完成舊生命
  動作在裁決時明確拒絕。Client照常Drain／ACK旧結果，但只允許當前自身生命的
  結果影響當前武器提示；傷害歸屬以targetId＋targetLifeGeneration核對。
- 死亡本身不重新Join、不改Lobby占用人數；同一玩家重生不增加玩家數。

## 6. 人物呈現與內容邊界

只使用PvP自有資產：女性人物與buns、UAL1骨骼動畫、既有hand_r持槍配置及
Ultimate Pistol世界模型。第一人稱保持Mark23；兩種槍模差異沿用v2現有素材慣例。
不新增角色選擇、不要求取得新素材。

Client專用玩家Presenter使用已存在的模型載入、相容骨骼轉移、SamplePose／
BlendPoses及蒙皮機制；建立玩家專用character／animset與骨骼遮罩內容，不透過
EnemyCatalog／GameSession等campaign接口。Match只依賴膠囊與數值規則。

| 呈現 | 時間／組合規則 |
|---|---|
| Idle／Jog | 下半身取實際呈現位移，Jog參考速度3；相位依路程累積，貼牆／hold停步 |
| 後退／側移 | 後退反向Jog近似，側移沿用Jog；人物yaw仍依瞄準，不宣稱有專用方向動畫 |
| 上半身 | 明確產品骨骼遮罩組合持槍／瞄準／Shoot／Reload，不新增通用動畫層框架或IK |
| Jump | Start／Land各約100ms，中間Loop；控制／碰撞不等待動畫 |
| Shot | 約167ms；本機下一成功Presented開始，遠端以同位置時間線的接受Tick定位進度 |
| Reload | 1.5秒權威進度；不直接使用Mark23原始Reload約3.733秒 |
| Dead | Death01約2.4秒，之後保持末姿態到重生；不保留獨立屍體 |

當前資產的原始Jump Start／Land較長，必須由產品規定的呈現時程縮放；
不得將FBX播放完成當成起跳、落地、補彈或重生的權威觸發器。
校正／瞬移／長幀恢復重設位移取樣，不把大跳變累積成步程；新生命重設完整動畫。
普通時間線保持只停步，不重新播放已見Shot／Reload／Dead。

遠端位置、生命與動作取同一Snapshot呈現區間，不把最新CombatState與較舊位置
任意拼接；不跨生命世代或Alive／Dead邊界插值，也不因此清除其他玩家的時間線。
本機HUD則始終讀最新权威Snapshot。短暫遠端Shot可被新狀態取代，恢復後不補播
過時動畫；這不影響原請求者取得每個動作的可靠裁決。

## 7. 驗收、分母與Architecture Delta

各批工作與停止點見 [進度](plans/v5/README.md)。v4原始測試／報告保留；v5需
明確替換「零血仍可操作、無限彈藥、20Tick冷卻」的舊斷言，不能直接把它們刪掉
而漏驗新行為。

- 延用移動送出P95≤22ms、Actual P50≤50／P95≤66.7ms、可見交越P50≤50／
  P95≤80ms、配對率≥99%。未知／不明移動事件按無限延遲納入，所有慢幀保留。
- 乾淨合法動作延續Match裁決P95≤100ms、Client取得P95≤150ms；拒絕與接受
  分別記錄，正常可接受操作須完整接受。故意死亡／空彈／冷卻壓力不能用來
  掩蓋合法操作失敗，也不能因新版增加合理拒絕就要求所有測例都100%接受。
- 生還且Session有效的可恢復故障維持1.5秒內恢復並穩定至少250ms。死亡3秒是
  玩法等待，另列生命驗收；不得延長所有網路恢復門檻。
- 乾淨跑次要求零積欠／耗盡恢復重設；重生的epoch切換需有唯一LifeRespawn原因、
  生命增量及位置重建證據。不得把未知reset重命名成重生。
- 動作、傷害、彈藥、重生取消按生命世代核對。舊生命未執行移動的失效命令
  單列生命周期取消，不算Actual，也不能靜默消失；純移動基準另保留≥99%Actual。
- 原生操作及GPU圖像與乾淨效能量測分離。新模型接受膠囊命中範圍，不宣稱
  骨骼級命中、input-to-photon、實體LAN、Windows或射擊回溯已驗證。
- 每批只短測後停止；三輪GUI及60／144Hz各30分鐘需另外明確授權。v5完整驗收
  未完成前仍候選，不能覆寫v4穩定基線或沿用v4的完整認證標記。

Architecture Delta（第01批記錄；第02–03批實作對應部分，第04批完整動畫仍待啟動）：

1. Feature壓力：v4缺乏垂直移動、彈匣與生命世代，遠端只有方塊；不能靠Client
   單獨播動畫得到權威死亡／重生或去重換彈。
2. 改變邊界：產品Client／Runtime契約、PvP domain狀態、Client人物呈現與產品
   CMake內容選取；沒有新Top-level domain或公共遊戲政策。
3. 影響owner：object_fps_pvp的Client、Match、產品Go Adapter、資產與專用驗收。
4. 依賴方向：Client→既有Engine Model／Renderer；Match→既有Runtime／Collision；
   Engine及公共Gateway不依賴PvP，PvP不依賴v2或Editor。
5. Ownership：生命、動作帳本、跳躍、動畫遮罩與參數留在產品；不移入Engine。
6. 較小替代不足：只換模型無法完成玩法；直接啟用複製的Campaign／Enemy
   controllers會帶入不必要生命周期，且v2沒有網路重生契約或步頻校準。
7. Fitness：專用code／assets／tests由owner選取；只讓Client增加角色支援，
   Match無SDL／Model／Renderer；刪除／複製產品不要求公共層新增名稱分支。

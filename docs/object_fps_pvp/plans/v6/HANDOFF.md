# PvP v6 交接

更新：2026-10-04。Owner：`object_fps_pvp`。**第 01 批完成**（計畫、分批與基線；PR [#39](https://github.com/yojinn-io/GYO-Engine/pull/39)，CI 通過，等使用者合併；工作分支 `claude/pvp-v6-batch01`，自 master `05042fa`）。
其餘批次都未開始；現行 wire 與玩法仍是 v5（[v5 穩定基線](../v5/STABLE_BASELINE.md)、[v5 交接](../v5/HANDOFF.md)）。

本文件原本收集 v5 期間使用者決定「延到 v6」的項目。2026-10-04 第 01 批開始後，它也是 v6 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。
數字未標「實機」的，是 CPU 模擬或靜態分析的結果。

## 閱讀入口

1. [進度與執行規則](README.md)：全部批次、依賴、檔位。
2. 本文件：決策、延後項目的現況與對應批次、各批紀錄、未結事項。
3. [基線](BASELINE.md)：基準 commit、CTest、產物指紋、v5 證據、wire 版本號檢查表。
4. 指定批次的文件，以及 [v6 契約骨架](../../protocol-v6.zh-Hant.md)。
5. Engine 部分的正式來源：
   - [輸入與呈現](../../../architecture/plans/input-and-present/HANDOFF.md)：第 3、11 項的 Engine 部分。
   - [基礎後續整理](../../../architecture/plans/foundation-followups/HANDOFF.md)：第 10 項的 Engine、工具、測試、登錄列，以及第 8 項的共通測試列。

## 決策紀錄

全部由使用者在 2026-10-04 決定。

| # | 決策 |
|---|---|
| D0 | v6 納入四群：A 玩法／呈現（1、2、4、6）；B Engine 平台（3、11）；C 驗收工具（5、7、8）；D Math 範圍外事項（10）。第 9 項維持候選 |
| D0b | 第 6 項是「自己死亡後仍看到第一人稱持槍手臂」，屬缺陷，須先重現 |
| D0c | 規劃用 ultracode：3 個分批方案、1 位評審、1 次對抗式檢查 |
| D1 | Engine 工作的計畫與紀錄放在兩個 Engine 計畫夾（IP、FF），正式清單移過去；本文件只保留產品列 |
| D2 | 受擊時點新增 wire 欄位：`last_damage_tick`、同一生命內遞增的受擊計數，**並帶最後攻擊者 id**；第 13 批做受擊方向指示 |
| D3 | Collision（使用者原話：「這個地方不想再埋坑，用徹底的方案」）：全部公開查詢（含 `RaycastAabb`）共用一套 double 實作與單一容差；`VerticalCapsule` 多載改為薄包裝；公開合法性檢查，並讓產品的 `Arena::Validate`、`ShotQuery` 改用；退化膠囊一併拒絕（FF-9＋第 10 批） |
| D4 | GYOP 24-byte 標頭的 C++ 編解碼收進 Engine，位元組不變，排在第 09 批之前（FF-8） |
| D5 | 刪除未編譯的 29 檔，並清理只被它們使用的孤兒資產（第 08 批） |
| D6 | 第 8 項的共通測試列全部改為結構條件（FF-2） |
| D7 | 第 3 項先量測再選修法：先拆開量 fence 等待與 `nextDrawable`；停頓在 `nextDrawable` 時停下，帶證據請使用者選擇「視窗移動／縮放期間暫停 acquire」或「分執行緒」（IP-2） |
| D8 | ultracode 與高於主對話的檔位：逐批開始時說明並徵求同意 |
| D9 | v5 原始證據：複製需要的目錄並記錄雜湊（第 01 批已完成） |
| D10 | include 路徑風格另立 FF 批徹底統一（FF-7） |
| D11① | wire 只在第 09 批升一次；之後到升格前原則上不再改 wire |
| D11② | 第 9 項、文字輸入與剪貼簿、AssetManager `LoadShared`、產品登錄資料搬家維持候選 |
| D11③ | 人物短測的達成 FPS 未達名義×0.85 時判 invalid：不計次，報告照列；有效輪不足時標「未驗證」，不算通過 |
| D11④ | 第 5 項：附涵蓋對照表後刪除 v4 語意模式 |
| D11⑤ | 第 4 項：以相位追蹤補償本機冷卻閘；乾淨跑次的權威冷卻拒絕必須為 0 |
| D11⑥ | 俯仰與受擊的呈現細節，在第 11、13 批開始時提案、確認 |
| D11⑦ | `Render::Color` 與 `UiColor` 保留兩個型別，只收斂有限性檢查 |
| D11⑧ | `object_fps_preview` 在 `tools.csv` 停用，並寫進遷移清單 |
| D11⑨ | `item_step` 加有限性驗證，不升 gyo.ui 版本，但在契約註明超出 float 範圍的值也會被拒絕 |
| D11⑩ | 驗收平台比照 v5：實機只有 macOS Intel／Metal，其餘標「未執行」 |
| D11⑪ | 第 14a 批的產品移除檢查把本產品的 dev_log 視為歷史紀錄保留 |
| D12 | Engine 計畫文件完全匿名、不連結：只寫「提出需求的消費端」，不連到本產品的文件 |
| D13 | IP-2 的 L2 必須在合併前完成；產品的分析器需要修改時，先做驗收工具的修正，再回到 IP-2 |
| D14 | 第 02a 批：`weapon_short` 的多數斷言別處沒有涵蓋，所以先把獨有斷言移進 `action_short`，再刪除它（取代 D11④ 的「直接刪除」） |
| D15 | 第 03 批：CI 的 -O0 比對只對角度為 0 的情境強制（Apple arm64 的 sincos 合併會讓非零角度差 1 ULP，本來就允許）；產品不改；加保險測試證明 ULP 差異不會在 Client 校正中累積 |

## 第 01 批進度（記錄器）

- 2026-10-04：使用者開始 v6，決定 D0～D0c。兩個 Explore agent 盤點現況，結果見下方各項目的「更正」。
- 2026-10-04：使用者要求先看全部批次、檔位與理由的表，核准後執行規劃。
- 2026-10-04：ultracode 規劃完成。三個方案分別是 23、18、12 批；評審以「以協議為中心」案為主幹，合成推薦案。對抗式檢查找到 6 個 major、12 個 minor，全部處理。經過見 [dev_log](../../../dev_logs/2026_10_04_pvp_v6_batch01.zh-Hant.md)。
- 2026-10-04：使用者決定 D1～D11。批次重新編號為本產品 01～14，Engine 為 IP-1～2、FF-1～9。
- 2026-10-04：基線：test preset 建置成功，CTest 54／54 通過（97.64 秒）。v5 證據 10 個目錄、1,325 個檔案 clone 到本 worktree，雜湊與原檔相同。
- 2026-10-04：批次文件與 Engine 計畫並行撰寫，經兩位審查者檢查；使用者另決定 D12、D13；修正後再驗證一次，剩下的小問題由主對話修正。
- 2026-10-04：最終檢查：315 個相對連結全部存在；Engine 文件沒有連到本產品文件；沒有簡體字；變更只有 docs。commit 後開 PR [#39](https://github.com/yojinn-io/GYO-Engine/pull/39)。CI 通過：只改 docs，CI scope 判定略過 L1 四平台與 Quick acceptance，CI gate 通過。

## 第 02 批進度（記錄器）

- 2026-10-04：使用者開始第 02 批：一個 PR、三個 commit，high 為主、02c 局部 xhigh；分支 `claude/pvp-v6-batch02` 疊在 #39 上。
- 2026-10-04：02a 開工盤點觸發停止條件（`weapon_short` 的涵蓋對照表有斷言找不到替代），使用者決定 D14。
- 2026-10-04：02a 完成：獨有斷言移進 `action_short`，刪除 `weapon_short`、legal 模式；`run_native_window.py` 改為 v5 死亡語意；
  驗收端版本號收成單一常數。本機雙 GUI 開發實跑 `action30`／`60`／`144`／`capture` 通過（不計次）。對照表見 [dev_log](../../../dev_logs/2026_10_04_pvp_v6_batch02.zh-Hant.md)。
- 2026-10-04：02b 完成：GUI 與 headless probe 都記錄 3 秒空 60 Hz 迴圈的晚醒分布（睡法與各 probe 相同），只供解讀；判定不讀它（突變與原始碼守衛測試）。
- 2026-10-04：02c 開工盤點，`worker_main` 的節奏判定與每秒 ≤31 動作需要產品帶出送出端資料；停下回報，使用者決定「做完可行的，其餘記錄」。
- 2026-10-04：02c 完成：人物短測（FPS invalid、事件錨定時窗）、`action_short`（等閘門）、`gui_main`（括號式上界、連續幀、依實際停頓分類）、
  交越順序、視窗 raise 完成事件、Gateway 背壓測試確定化、停頓規則說明。工具凍結後重新分析 v5 原始資料，8 個跑次的新判定與事前宣告全部一致。
- 2026-10-04：開 PR [#40](https://github.com/yojinn-io/GYO-Engine/pull/40)（base 為 #39 的分支，#39 合併後改指 master）。
- 待決：`action_probe.py` 自己的 `main` 仍是 v4 動作矩陣語意，v5 驗收不用它。
- 待決：`worker_main` 的重送節奏與每秒 ≤31 動作／結果仍以接收時間判定（未放寬）；需要產品帶出送出端序號或時間，建議第 09 批評估。

## 第 03 批進度（記錄器）

- 2026-10-05：使用者指示「按計劃來」，開始第 03 批；檔位照計畫（high，golden 策略局部 xhigh）。分支 `claude/pvp-v6-batch03` 疊在 #40 上。
- 2026-10-05：完成 runner（35 個情境，30 個屬 golden 子集）、-O0 runner 與 CTest、golden 檔、兩樹比對腳本；記錄 `05042fa` 的 digest。
  靈敏度第 1 項（x 加 1 ULP）未達宣告字面：只擾動 x，只沿 z 移動的情境沒被擾動。停下回報後，使用者決定追加 z 的改動（第 5 項），結果符合宣告。
  經過見 [dev_log](../../../dev_logs/2026_10_05_pvp_v6_batch03.zh-Hant.md)。
- 2026-10-05：開 PR [#41](https://github.com/yojinn-io/GYO-Engine/pull/41)（base 為 #40 的分支）。CI 四平台會驗證 golden 子集。
- 2026-10-05：CI 的 macos-arm64 出現 -O0 不一致（只有非零角度的 `move-turning/contract`，原因是 sincos 合併），依停止條件回報；使用者決定 D15，已實作並加保險測試。
- 2026-10-05：修正後 CI 四平台全部通過；golden 子集在四平台逐位元相同。

## Engine 批次對本產品的影響（記錄器）

- 2026-10-05：FF-8（Engine 的 GYOP 標頭編解碼）改了本產品的 `apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp`：標頭編解碼改用 `Engine::Net`，wire 版本收成 `wire::ProtocolVersion` 一個常數（值仍為 5），Type 範圍與 TCP frame 留在產品。
  [基線](BASELINE.md) 的 wire 版本號檢查表中 `Wire.hpp:26`、`:36` 兩處，因此變成這一個常數（第 09 批改值時以它為準）。
  位元組與拒絕集合以 `tests/object_fps_pvp/WireTests.cpp` 鎖住；權威 digest 兩樹比對相同。見 [基礎後續整理交接](../../../architecture/plans/foundation-followups/HANDOFF.md)。

## 延後項目：現況與對應批次

### 1. 遠端人物上半身的俯仰瞄準 → 第 11 批

- 來源：v5 第 04 批 L3 人工驗收（2026-10-02，使用者）：A 抬頭看天時，B 畫面中的 A 沒有任何變化。
- 現況：
  - `PlayerPresentationFrame`（`PlayerPresentation.hpp:18-21`）只有 yaw，填值在 `PvpApplication.cpp:599-605`。
  - `PlayerState.pitch` 已在 Snapshot 裡，並由 `SnapshotTimeline` 內插，所以**不需要改 wire**。
- 這是 v5 契約 §6「上半身：明確產品骨骼遮罩組合持槍／瞄準」的缺口。
- 素材：`UAL1_Standard.fbx` 有 `Armature|Pistol_Aim_Up`／`Pistol_Aim_Neutral`／`Pistol_Aim_Down`（各 0.167 秒），目前沒有任何地方引用。

### 2. 受擊反應 → 第 09 批（wire 欄位）、第 13 批（呈現）

- 來源：同上（使用者）：被擊中沒有反應。
- 現況：
  - Snapshot 只有 `CombatState.hp`，沒有受擊時點。
  - 裁決結果只送給射擊者（`gateway/action_delivery.go:139,218-236`）。
  - 既有的 `last_shot_action_id`／`last_shot_tick` 是「呈現取權威時點」的模式，受擊欄位比照它（D2）。
  - 重生時 `player.combat = CombatState{id}`（`PvpMatch.cpp:241`），新欄位自然歸零。
- 素材：`Armature|Hit_Chest`（0.333 秒）、`Armature|Hit_Head`（0.433 秒）。v6 沒有爆頭，Hit_Head 沒有權威依據，不使用。

### 3. Engine 渲染阻塞主迴圈（macOS）→ 第 04 批（重現）、IP-2（修正）、第 07 批（B1）

Engine 部分的正式來源是 [輸入與呈現](../../../architecture/plans/input-and-present/HANDOFF.md)。本產品這一側的事實：

- 來源：v5 第 04 批 L3 人工驗收，使用者看到 `CONNECTION POOR`。
- 證據（已 clone 到本 worktree）：`build/target/_build/test/logs/pvp-v5-batch04-20261002-manual-2/client-*.log`。
  - render_ms ≥250 ms 的停頓，兩個 client 合計 12 筆，範圍 362.8～1199.5 ms（原記 5 次）。全部是 `presented=1`。
  - 停頓完全落在 render_ms 內；同一幀的 frame_gap_ms 約 16 ms，下一幀約 1202 ms。
  - 第一組 `-manual/` 另有一次 1134 ms。
- 影響：主迴圈是單執行緒（`RuntimeLoop.cpp:18-44`）。停頓期間 Client 不產生移動命令（`LocalPlayerPrediction.hpp:72-76` 每幀最多補 5 步），1.2 秒約 72 Tick 被 Host 以 Held 替代，占 10 秒窗口的 12%（門檻 5%），警告判定正確。
- 更正：
  - Engine 已經有 `PresentStatus::Skipped` 路徑，本產品在 `PvpApplication.cpp:943` 依此分支。
  - SDL 3.4.0 的 Metal 後端即使不阻塞，仍會呼叫最多阻塞約 1 秒的 `nextDrawable`（詳見 IP HANDOFF）。
  - client-2 的 5 筆停頓中有 4 筆，「釋放指標」在停頓幀之後約 3～4 ms 才記錄到，也就是視窗事件在停頓之後才被處理。

### 4. 本機射擊冷卻閘的落差 → 第 12 批

- 來源：v5 第 04 批 04-3 開發實跑。
- 現況：
  - Tick 閘：Client 以最新 Snapshot 的 Tick 比對 `nextAllowedShotTick`（`PvpApplication.cpp:445`），該 Tick 約比權威晚 2 Tick。
  - **更正**：另有一道牆鐘閘 `localCooldownUntil`（`:442,455`），HANDOFF 原本沒有記錄。
  - 權威的 10 Tick 冷卻（`PvpMatch.cpp:296-312`）本身正確。
- 決定：D11⑤。

### 5. 驗收工具清理 → 第 02a 批

- 現況：`weapon_short.hpp` 的觀察方仍檢查 v4「HP=0 仍可移動與射擊」；`run_action_legal.py`、probe 的 `--legal-shots`、`action_evidence.analyze_legal` 假設「全部接受、無限彈藥」。
- **補列**（對抗式檢查）：`run_native_window.py` 的 HP 歸零段（:428-437）是同類的 v4 斷言；該 runner 只支援 Linux X11。
- **刪除前的前提**：`weapon_short` 是唯一逐幀記錄第一人稱 `submitted_meshes` 的 probe，第 04 批重現第 6 項要用到。第 02a 批要先把這個欄位移進存續的 probe，再刪除。共用 helper 的依賴見第 02 批文件。
- 決定：D11④。

### 6. 自己死亡時的第一人稱持槍手臂 → 第 04 批（重現）、第 05 批（修正）

- 來源：v5 第 04 批 L3（使用者）：「自己死亡時，持槍手臂還是在。」
- 使用者 2026-10-04 釐清：看到的是**第一人稱手臂**，屬缺陷（D0b）。
- 現況：
  - Client 在 `!Alive()` 時跳過 `SubmitWeapon`，並把 `submittedMeshes` 設為 0（`PvpApplication.cpp:515-516`）。這個欄位和 `weaponFeedback.dead`（`:341`）讀同一份 Snapshot，所以不能用它抓到缺陷，第 05 批要另訂觀測量。
  - 本機人物不會被 `players->Submit` 提交（`:567` 跳過自己），可以排除「自己的人物被畫在鏡頭前」。
  - 待查：Skipped 幀殘留上一張畫面、warmup（`:740-750`）、ViewModel camera 狀態、`Alive()` 讀取的時點。
- 候選（新需求，不在 v6 範圍）：對方畫面中，屍體手上的世界手槍仍掛著（`PlayerPresentation.cpp:464-468`，提交在 `:512`）。若希望掉落或隱藏，另行決定。

### 7. 驗收紀錄的計時器基線 → 第 02b 批

- 來源：v5 第 05 批 05-5 的 60 Hz 長測失敗。headless probe 以 `sleep_until` 跑 60 Hz 迴圈，macOS 常晚醒 3～8 ms（空迴圈 P99 20.5 ms）。
- v5 的處理：重生首幀時間重設的配對規則改為結構條件。
- v6：probe 開始時量一次計時器抖動，寫進平台指紋與報告，**只用來解讀結果，不當門檻**；不依 OS 名稱分支。

### 8. 測試與驗收器的跨平台相容性 → 第 02c 批（產品列）、FF-2（共通列）

- 原則（使用者 2026-10-03 定案）：對玩家的門檻跨平台相同，不因平台放寬；測量工具對主機計時精度或視窗／GPU 行為的假設，優先改成結構條件；非得用時間常數時以實測基線解讀；不依 OS 名稱分支。
- 共通測試列（`MeshUpdateSmoke`、package checks、AssetWatcher、未啟用產品的 `test_gpu_smoke`）的正式來源是 [基礎後續整理](../../../architecture/plans/foundation-followups/HANDOFF.md) B 節。
- 本產品的列（`build/acceptance/object_fps_pvp/` 簡稱 BA）：
  - 已有失敗紀錄：BA/`player_presentation_evidence.py:101-103`，達成 FPS 須 ≥ 名義×0.85，v5 第 04 批 144 FPS 案只到 120 FPS 而失敗；同檔 `:122-139` 的腳本時窗也依賴事件準時。
  - BA/`worker_main.cpp:336-352`：1 秒內重送 55～65 次、60～70 次嘗試；`:219,340,366` 以接收端 `Pump()` 時間要求間隔 ≥30／≥14 ms。
  - BA/`worker_main.cpp:230-234`、`action_probe.py:402-412`、`gameplay_evidence.py:202-206`：以 relay 接收時間做 1 秒滑動窗口。
  - BA/`action_short.hpp:150-158`：射擊間隔 0.25 秒對 10 Tick 冷卻、換彈只留 200 ms。
  - BA/`gui_main.cpp:657-662`、`:932,1004`、`:571-686`。
  - BA/`presentation_evidence.py:16,130`：遠端交越不得早於本機 20 ms 以上。
  - BA/`run_timing.py:69-105`：計次 GUI 輪任一 OS 視窗事件即無效。
  - `apps/object_fps_pvp/gateway/backpressure_test.go:102-158`：60 Hz `time.Ticker` 在 250 ms 內須 ≥12 次。
  - 說明（非缺陷）：各分析器的「≥100 ms 停頓」約 83 ms 即可觸發（`LocalPlayerPrediction.hpp:73` 每幀最多補 5 步）。
- 決定：D6、D11③。

### 9. 未排程的候選（維持候選）

- CS 式開局／回合準備期：全員凍結、無敵、倒數；需 Match 回合狀態與契約變更。見 [v5 fix/02](../v5/fix/02-a1-cancelled-by-stall-reseed.md)。
- 相位估計器在 WAN 雜訊下的穩定性：先收集真實餘裕樣本分布再決定。見 [延遲整改回顧 v2](../v5/LATENCY_CASE_STUDY_v2.md) 第 6 節。

### 10. Math 基礎統一的範圍外事項

Engine、工具、測試、登錄各列的正式來源是 [基礎後續整理](../../../architecture/plans/foundation-followups/HANDOFF.md) A 節（FF-1～FF-9）。本產品自己的列：

| 項目 | 批次 |
|---|---|
| Client 與 server 只比對 arena 的 id 與 version，不比對內容；Client 以自己的 arena 檔做 prediction（`ClientConnection.cpp:180-184` 的 `CheckArena`，由 HTTP join `:276` 與 Welcome `:388` 呼叫；Gateway 轉送點 `server.go:276,440`） | 第 09 批（arena 內容 digest） |
| `GroundPoint` 的有限性檢查三份、格子線段檢查兩份（屬未編譯的 29 檔） | 第 08 批（隨刪除結案） |
| `EnemyPresentationDefinition` 與 `EnemySystem` 的攻擊時間容差不一致（屬未編譯的 29 檔） | 第 08 批（隨刪除結案） |
| Collision 統一對本產品權威判定的影響、權威 golden 更新、`Arena::Validate`／`ShotQuery` 改用公開合法性檢查 | 第 10 批（與 FF-9 同一 PR） |

### 11. Engine 輸入層的缺口 → IP-1（Engine）、第 06 批（本產品）

Engine 部分的正式來源是 [輸入與呈現](../../../architecture/plans/input-and-present/HANDOFF.md)。本產品這一側：

- `PvpApplication.cpp` 的 `HandleNativeEvent`（:188-237）繞過 Engine 直接處理 `SDL_Event`：
  - 縮放時更新寬高，並釋放指標。
  - windowID 過濾；失焦、移動、縮小時釋放指標。
  - Tab 切換指標擷取。
  - 視窗內的點擊會擷取指標，或保留同一幀的射擊上升沿（`pendingShotEdge`，:209-217）。
- 文字輸入與剪貼簿（:157-160、:223-236）不在 v6 範圍，第 06 批保留原生處理，並列為例外。
- 驗收 GUI probe 以 `SDL_PushEvent` 注入合成事件；注入就是被量測的 SDL 路徑，所以注入本身保留 SDL。
- `tools/object_fps_preview` 依 D11⑧ 停用，它的輸入遷移只寫進遷移清單。

## 未結事項

- 第 01 批：PR #39 等使用者合併。
- 第 02 批之後都未開始，由使用者逐批指定。
- 已知要在批次開始時決定的事：
  - FF-7 的 include 統一方向。
  - 第 11、13 批的呈現細節（D11⑥）。
  - 第 10 批與 FF-9 的容差選擇與事前宣告。
  - IP-2 量測後的修法選擇（D7）。

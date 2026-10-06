# PvP v6 交接

更新：2026-10-05。Owner：`object_fps_pvp`。**第 01～03 批完成並合併**：PR [#39](https://github.com/yojinn-io/GYO-Engine/pull/39)（`a687663`）、[#40](https://github.com/yojinn-io/GYO-Engine/pull/40)（`a36b319`）、[#41](https://github.com/yojinn-io/GYO-Engine/pull/41)（`6381e9d`），2026-10-05 由使用者依序合併到 master。**第 04 批完成並合併**（分支 `claude/pvp-v6-batch04`，自 master `6381e9d`；PR [#42](https://github.com/yojinn-io/GYO-Engine/pull/42)，`30f87d5`）。**第 05 批不執行**（第 04 批未重現）。
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
| D16 | （Engine，2026-10-05）IP-2 同時處理拖動與縮放；D17：xhigh 審查 agent 1 個；D18：PR 以功能線為單位、盡量少開。詳見 [輸入與呈現交接](../../../architecture/plans/input-and-present/HANDOFF.md) |
| D19 | （2026-10-05）IP-2 的 L2 之後，D7 選「分執行緒」，但延到 **v7**：本產品的主執行緒（事件＋畫面）、模擬、網路三個角色分離（網路不應和畫面、主執行緒在一起）。IP-2 只交付縮放拖動中的 live frame |
| D20 | （2026-10-05）版本號分兩層：**vN＝遊戲版本**，**pvN＝網路協議版本**。程式碼中的協議目前是 pv5（`Wire.hpp` 的 `wire::ProtocolVersion`、`ClientConnection.cpp` 的 join、Gateway `adapter.go` 的 `ClientVersion`／`RuntimeVersion`、`runtime_v5.proto`）。本計畫舊文中的「協議 v6」「protocol-v6」指的是 **pv6**（第 09 批） |
| D21 | （2026-10-06）30 FPS 的相位餘裕缺口（第 07 批後續的 A/B 調查）**不在 v6 修正**：已規劃的第 15 批（完整觀測＋等待守門）不執行。根因是 Client 的移動命令在主迴圈依畫面幀產生，30 FPS 時一幀含多個固定步，較舊的步要等整幀才送出；v7 的主執行緒／模擬／網路分離（D19）會讓命令在固定步邊界產生，原因就不存在。v6 只更正文件，把 30 FPS 寫成設計範圍的邊界（[protocol-v5](../../protocol-v5.zh-Hant.md) §1 的追記）。網路路徑上以短休眠輪詢、實際長度依 OS 計時粒度而異的問題，也交給 v7：Match 與 Gateway 間的 IPC 迴圈每次休眠 1 ms（`IpcHost.cpp:267`），Client 的網路 worker 休眠 2 ms（`ClientConnection.cpp:567-570`） |

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

## 第 04 批進度（記錄器）

- 2026-10-05：第 01～03 批的 PR 由使用者合併（#39→#40→#41，各自改指 master 後合併）。
- 2026-10-05：使用者決定先執行第 04 批（Engine 的 IP-2 需要本批的重現）。檔位照計畫：medium。
  量測來源固定為 master `6381e9d`，在獨立 worktree `GYO-Engine-v6b04`（detached）以 test preset 從零建置。
- 2026-10-05：L1：第 02 批凍結的 52 個工具檔與清單（`b5f4bf79…`）逐一相同。`05042fa..6381e9d` 沒有合併任何 Engine 批次；`engine`、`apps`、`services`、`tools` 之下只改了 `apps/object_fps_pvp/gateway/backpressure_test.go`（測試），產品執行期來源與 `05042fa` 相同。
- 2026-10-05：跑次、分母與第 3、6 項的重現步驟已在量測前寫入 [dev_log](../../../dev_logs/2026_10_05_pvp_v6_batch04.zh-Hant.md) 並 commit。
- 2026-10-05：使用者開放螢幕錄製權限；第 6 項改為 Claude 以 probe 自動重現與連續擷取，未重現時才用正式 Client（量測前修改宣告）。
- 2026-10-05：L1：CTest 55／55；權威 digest 規模 1、10 與 `05042fa` 逐位元相同。
- 2026-10-05：跑次 1 動作短測 4／4 通過；跑次 2 人物短測 3 案通過、player144 `invalid_capacity`（join 120.01 FPS，與宣告一致，標未驗證）；跑次 3 雙 GUI 整合短測通過。
- 2026-10-05：跑次 4 矩陣在 clean-30 失敗並停止（probe 一幀 57.7 ms 造成丟時，5 筆未配對）；保留，有限定位見 dev_log；停下回報。
- 2026-10-05：使用者決定以新目錄重跑矩陣：25／25 通過（`4b-matrix/`）；clean-30 最長幀 38.3 ms，與 v5 同級。
- 2026-10-05：第 6 項 probe 自動重現（不計次）：重疊的視窗、以及單視窗擷取時 probe 跑次失敗（保留）後，改用雙 GUI 戰鬥短測＋全螢幕擷取；join 兩次死亡共 18 張截圖都沒有第一人稱手臂。
  使用者看過畫面後決定不再以正式 Client 重現：**第 6 項未重現，第 05 批依停止條件不執行**。
- 2026-10-05：第 3 項正式 Client 重現（使用者操作）：**拖動標題列 `render_ms` 約 1197 ms（3 次中 2 次），重現**。
  **縮放的停頓在事件處理（0.4／3.0／1.0 秒），不在 render**，是新發現，已以文字摘要寫進 [輸入與呈現交接](../../../architecture/plans/input-and-present/HANDOFF.md)。使用者沒有看到 `CONNECTION POOR`。
- 2026-10-05：B0 寫入 [基線](BASELINE.md)；完成，停止。

## 第 06 批進度（記錄器）

- 2026-10-05：開始。
  - 分支 `claude/pvp-v6-batch06`，疊在 IP-2 分支上（開始時 `98beb78`，IP-2 審查修正後 rebase 到 `2912232`）：兩者都改 `PvpApplication.cpp`，IP-2 合併後 rebase。
  - 檔位：計畫建議 high，使用者已把主對話調為 xhigh，照 xhigh 進行。
- 2026-10-05：probe 盤點（使用者確認）：4 檔全部保留 SDL，沒有適用 IP-1 的部分。
  - `gui_main.cpp`：注入；event watch 計數 OS 視窗事件，需要 occluded／exposed／hidden 與推送當下的時點，IP-1 沒有。
  - `player_short.hpp`：注入；event watch 記錄 timestamp、裝置 id、xrel／yrel，IP-1 的事件沒有這些欄位。
  - `action_short.hpp`：注入、視窗大小與位置操作。
  - `combat_latency.hpp`：只有注入。
- 2026-10-05：事前宣告（使用者確認，實作前寫入）。遊戲中的擷取狀態機，從 `HandleNativeEvent` 的逐一原生事件，改為 pump 結束後依 IP-1 的 `PhysicalInputFrame::events` 依序處理。大廳的位址文字輸入維持原生處理（例外）。差異只有下列三條：
  1. 處理時機：同一幀內事件順序相同，仍在 `Update` 之前。可觀察的差異：
     - 釋放指標時的 `SetRelativeMouseMode(false)` 與 `ClearJumpRequest`，由事件當下改為 pump 結束後；
     - 「PvP releasing pointer」日誌的時間稍晚；
     - IP-2 的 live frame 看到的擷取狀態，尚未套用該次 pump 的事件。live frame 不消費輸入，命令不受影響。
  2. 點擊是否在視窗內：由「產品依已處理的縮放事件更新的寬高」改為 IP-1 的 `insideWindow`（SDL 處理該事件當下的視窗大小）。只有在「點擊之後、同一次 pump 還有尚未處理的縮放」時不同。
  3. 每個事件當下的焦點，由 pump 開始時的焦點加上 events 中的取得／失去焦點重建。這不是差異：以 characterization 證明與舊的逐事件 `Snapshot().windowFocused` 相同。

  其餘都必須與舊行為相同：
  - 其他視窗的事件；
  - 按鍵重複；
  - Tab；
  - 左鍵的擷取與射擊上升沿；
  - 失焦／移動／縮放／縮小時釋放指標；
  - 縮放時更新寬高（`Max(1, ·)`）。

  驗證：把舊邏輯原樣複製到測試當基準，對同一串合成 SDL 事件（手寫案例＋固定種子的隨機序列）比對擷取狀態與釋放次數；每一條差異都附突變測試。
- 2026-10-05：實作（commit `382232b`，rebase 後為 `ddc1d44`）：
  - 擷取政策抽成產品內的純函式 `ApplyPointerCaptureEvents`（`PointerCapture.hpp`／`.cpp`）。
  - `ProcessEvents` 在 pump 後依序套用 `events`。
  - `HandleNativeEvent` 刪除，只留 `HandleTextEditing`（例外）。
- 2026-10-05：L1：
  - characterization（`object_fps_pvp.pointer_capture`）：
    - 手寫 14 幀、固定種子 5 組各 3000 幀：與舊邏輯 0 不一致。每組約 500 次擷取、100 次以上的射擊上升沿、1000 次釋放。
    - 宣告差異 2 有單獨測試（點擊之後、處理之前 SDL 已縮小視窗）；釋放日誌的計數也有測試。
  - 突變 10 個，抓到 9 個。沒被抓到的「失焦時不把焦點設為 false」是等價突變：同一幀內失焦後 `windowInteraction` 已是 true，之後的 Tab 與點擊本來就被擋下。
  - CTest 58／58。
  - grep：pvp 的 `src` 只剩 `HandleTextEditing` 與 `PumpEvents` 的觀察者簽名處理 SDL 事件。
  - 權威 digest 同機兩樹（base `2912232`，暫時的 worktree `GYO-Engine-b06base`；rebase 到審查修正後的 IP-2 後重跑）35 個情境 0 不同。
  - 消費端接線本身（漏呼叫 `ApplyInputEvents` 之類）不是單元測試的範圍，由 L2 的 GUI 短測（注入點擊、Tab、視窗事件）驗證。
- 2026-10-05：L2 事前宣告寫入證據夾的 `declare.txt`：
  - before／after 各跑動作短測 4 案、人物短測 4 案；
  - player144 預期為 `invalid_capacity`（D11③）。
- 2026-10-05：L2（macOS Intel／Metal，Claude 執行，機器閒置；證據 `build/target/_build/test/logs/pvp-v6-batch06-20261005/`）：

  | 跑次 | before（`2912232`） | after（`693ea5a`，程式同 `ddc1d44`） |
  |---|---|---|
  | 動作短測 | action30／60／144／capture 全部通過 | 全部通過 |
  | 人物短測 | player30／60、capture 通過；player144 為 `invalid_capacity`（join 只到 120.00 FPS） | 同 before（120.01 FPS） |

  - 動作短測涵蓋注入的擷取點擊、射擊、Tab、失焦、移動與縮放視窗、ESC 後重新加入，都通過。
  - 人物短測的 runner 結束碼為 1，只因 player144 不計次；依 D11③ 標「未驗證」，不算失敗。
- 2026-10-05：L3（macOS Intel／Metal，使用者依 v5 原生操作清單 1～8 操作；Claude 啟動 Match、Gateway 與兩個 Client，關窗後另開一個 Client 重新加入；證據 `.../pvp-v6-batch06-20261005/l3/`）：**全部正常**。
  - 使用者回報：快速連點時，射速有時會「加速」，鎖在一個較快的固定頻率。
    - 判斷：不是第 06 批引起。第 06 批沒有改射擊判定，L2 的 before／after 也相同。
    - 原因是已知的第 4 項：本機的牆鐘閘與 Snapshot Tick 閘不一致。
      - 確認回來之前只有 10 Tick 的牆鐘閘；回來之後，落後約 2 Tick 的 Tick 閘再多擋一些。
      - 被擋下的點擊不排隊，射速因此隨點擊相位跳動。
    - 上限仍是權威的 10 Tick。由 [第 12 批](12-local-fire-gate.md) 處理，並作為第 12 批 L3 的重現項目。
- 2026-10-05：完成。完成條件依序：
  - characterization 與突變；
  - grep 只剩例外；
  - 兩樹 digest 相同；
  - L2 before／after；
  - L3。
  CI 在 PR 上確認。暫時的 worktree `GYO-Engine-b06base` 已移除。

## 第 08 批進度（記錄器）

- 2026-10-05：開始。
  - 使用者先問了這 29 檔的功能，以及「單機劇情＋聯機選項」能否沿用。
  - 結論：
    - 這些是從單機版複製過來、從未編譯的舊單機架構（Client 自己判定）。
    - 原版在 `apps/object_fps_v2`：28 檔有同名原版，多數相同或只差 Engine 遷移；`GridWorldCollision` 只在本產品。
    - 本產品刪除前的版本可由 commit `ccbd279`（或 master `ef12037`）取回。
  - 使用者決定照計畫刪除。
  - 分支 `claude/pvp-v6-batch08`，疊在第 06 批（#54）上：兩者都改 v6 文件。
  - 檔位：計畫 medium，主對話 xhigh。
- 2026-10-05：盤點與處置：
  - 程式：
    - 29 個 `.cpp`。
    - 29 個 header：從所有已編譯來源（產品、測試、驗收 probe）沿 include 走不到的。
    - `GameData.hpp`、`GridMap.hpp`、`GroundPoint.hpp` 仍被武器相關 header 引用，保留。
    - `sources.cmake` 刪除 `APP_DOMAIN_SOURCES`、`APP_SUPPORT_SOURCES`，`PVP_DOMAIN_SOURCES` 不變。
  - 資產：從程式引用的 id 出發、沿資產內容的 id 引用遞移，走不到的有 30 個。
    - 29 個刪除（catalog 條目與檔案）：
      - 敵人 6 個；CSV 3 個；`ui.screens`；地圖 3 個；天空；地板與牆貼圖；`common.texture.white` 的本產品副本；
      - 男性角色（定義、模型、3 張貼圖）；女性角色定義（模型與貼圖仍被玩家使用，保留）；
      - 簡單分線髮型 2 個；UAL1 的角色定義與 locomotion 動畫集（模型保留）；
      - 動畫手槍第一人稱 2 個。
    - 保留 1 個，附理由：`object_fps_pvp.arena`（Client 與 Match 以路徑 `pvp_arena.json` 載入）。
    - 不在 catalog 的孤兒檔案 2 個，刪除：`fonts/PressStart2P-Regular.ttf`、`textures/weapons/gun.png`。
    - 刪除的美術資產在 `assets/object_fps_v2` 都有逐位元相同的原版。
    - catalog 從 50 個條目減為 21 個；沒有空目錄。
- 2026-10-05：L1：
  - 新增產品自有的 `object_fps_pvp.asset_catalog`（Python）：
    - 每個條目的檔案存在；
    - 資產夾的每個檔案都在 catalog（或是根目錄清單）；
    - id 不重複；
    - 每個 id 都被使用，或列在保留清單並附理由；
    - 檢查本身會抓到孤兒。
    - 突變 3 個（孤兒 id、路徑不存在、未登錄檔案）都被抓到。
  - 已編譯來源不變：重新 configure 前後的 `compile_commands.json` 同為 957 個檔案、清單相同；`apps` 的差異只有刪除與 `sources.cmake`。
  - CTest 59／59。
  - grep：刪除的檔名、header、`APP_*_SOURCES`、catalog id 與資產路徑，在現行程式與資料中都沒有殘留。只剩健檢報告與 dev_log 等歷史紀錄，依計畫不改寫。
  - 權威 digest 同機兩樹（base `ccbd279`，暫時的 worktree，用完已移除）：35 個情境 0 不同。
- 2026-10-05：完成。CI 在 PR 上確認。L2、L3 依計畫不適用（沒有行為變更）。
- 單機劇情＋聯機選項（v7 之後的產品方向候選，使用者 2026-10-05 提出）：
  - Match 只處理命令與權威狀態，不知道 UDP／HTTP／房間，傳輸由 Gateway 負責。
  - 單機時可以在本機執行同一套權威模擬，跳過 Gateway；劇情、敵人、關卡只寫一份。
  - 與 D19 的主執行緒／模擬／網路分離同方向。
  - 單機內容應從 `object_fps_v2` 有意識地移植到權威模擬上，而不是沿用已刪除的舊副本。

## 第 07 批進度（記錄器）

- 2026-10-05：開始（使用者指定）。
  - 分支 `claude/pvp-v6-batch07`（只有文件；同時同步 #53～#55 與先前各批 dev_log 的合併狀態）。
  - 檔位：計畫 medium，主對話 xhigh。
- 2026-10-05：事前宣告（量測前寫入，全文在證據夾的 `declare.txt`）：
  - 來源：master `5b0553a`，獨立 worktree `GYO-Engine-b07`，從零建置。
  - 工具：52 檔中 51 檔與第 02 批凍結清單相同；`gui_main.cpp` 只差 FF-7 的 3 行 include 路徑，Python 分析器全部相同。
  - 量測前的檢查（不算量測）：
    - 全部 CTest 一次；
    - 權威 digest 規模 1、10 的輸出雜湊與 B0 比對，不同就停下。
  - 跑次與 B0 相同，各一次、依序、機器閒置：
    1. 動作短測 4 案；
    2. 人物短測 4 案（player144 預期 `invalid_capacity`）；
    3. 雙 GUI 整合短測 1 輪；
    4. 25 案矩陣。
  - 每項與 B0 並列：結果、達成 FPS、`skipped_frames`（probe 有記錄者）、與 B0 相同的延遲數字。
  - 差異分類為幀節奏、Skipped、延遲、其他；門檻只對 v5 STABLE_BASELINE。
- 2026-10-05：量測前的檢查：
  - B1 tree 從零建置；CTest 59／59，112.81 秒。
  - 權威 digest 規模 1、10 與 B0 逐位元相同（`34d0ca7f…`、`f9e0ef41…`）。
  - 產物與工具的雜湊在證據夾的 `artifacts.sha256`、`tools.sha256`。
- 2026-10-05：第一次執行是**呼叫錯誤，不是量測**：
  - zsh 不會切開未加引號的參數變數，4 個 runner 都在解析參數時以結束碼 2 退出（約 1 秒），沒有啟動任何 probe。
  - 輸出保留在 `attempt1-invocation-error/`；改用 bash 依同一份宣告執行。
- 2026-10-06：量測結果（證據 `build/target/_build/test/logs/pvp-v6-batch07-20261005/`）：

  | 跑次 | B1 | B0 |
  |---|---|---|
  | 1 動作短測 | 4／4 通過；8 個 probe 報告的 `skipped_frames` 都是 0 | 4／4 通過 |
  | 2 人物短測 | player30、player60、capture 通過；player144 `invalid_capacity`（join 120.01 FPS） | 同 |
  | 3 雙 GUI 整合短測 | 通過，視窗未受干擾；移動 Actual 99.58%，P50／P95 27.1／36.3 ms；可見交越 20／20，P50／P95 33.6／37.3 ms；`skipped_frames` 0 | 通過；Actual 1.0，P50／P95 30.1／37.3 ms；交越 P50／P95 37.7／39.8 ms |
  | 4 25 案矩陣 | **失敗**：停在 clean-30（執行 2／25） | 第 1 次同樣在 clean-30 失敗；重跑 25／25 |

  - 矩陣失敗的有限定位：
    - clean-30 的兩個 Client 在同一瞬間（相差 24 ms）各有一次 `runtime_gap`：一個 41.1 ms 幀、丟時 24.4 ms，另一個 24.0 ms 幀、丟時 7.3 ms。都不是重生造成的，所以觸發嚴格的停頓規則；同時 Actual 低於 99%。
    - 同一個 probe 程序內的兩個 Client 同時卡住，指向主機層級的短暫停頓。
    - **更正（2026-10-06，A/B 調查）**：24.0 ms 那一筆是 player 2 重生後的 seed 夾住，分析器已配對為 LifeRespawn；未配對的只有 41.1 ms 那一筆。「兩個 Client 都不是重生、同時卡住」與「指向主機停頓」的推論不成立。
    - 這一案記錄的計時器分布比 B0 差：晚醒 P50／P99 3.7／8.1 ms、間隔 >18 ms 的比例 34%（B0：2.3～3.0／3.8～4.2 ms、9～22%）。
    - 與 B0 第一次的失敗同一案、同一類型（B0 是 57.7 ms 幀）。
    - 依停止條件保留全部跑次，停下回報，不自行重跑。
- 2026-10-06：使用者選擇「重跑一次矩陣＋記為待查」。重跑（4b）**也失敗**：
  - 第 1 案 clean-60 就停下（執行 1／25，約 20 秒）。
  - 兩個 Client 同一瞬間各有一個 19.8 ms 的幀，丟時 3.1 ms；player 2 另有一個 19.1 ms 的幀，丟時 2.4 ms。
  - 這次計時器的晚醒 P50／P99 為 1.6／4.1 ms，>18 ms 的比例 21%，與 B0 相當。
  - 有限定位：
    - 矩陣用的無頭 action probe 不經過 `RuntimeLoop`／`PvpApplication`，IP-2 與第 06 批不在它的路徑上。
    - 從 B0 到 B1，這條路徑上的產品改動只有 FF-7 的 include 路徑與 FF-8 的 GYOP 標頭，兩者都證明位元組不變。
    - **可疑點**：一個 19.8 ms 的幀就丟掉 3.1 ms，等於「超過 1 Tick 的部分」全丟。但 `LocalPlayerPrediction` 的追趕上限是 5 步（`LocalPlayerPrediction.hpp:73`），照理不該丟。這可能是產品的丟時計算問題（實戰中也會造成替代），也可能是工具的判定方式；需要調查才能分辨。
  - B0 的兩次矩陣是 1 敗 1 勝；B1 的兩次都失敗（clean-30、clean-60）。
- 2026-10-06：調查（使用者指定；只讀程式與既有 trace，不改程式、不再量測）。
  - 所有被判失敗的丟時，都與一次 seed（`LocalPlayerPrediction::SeedLead`）同時發生：seed 之後的第一幀若超過 1 Tick，就被夾到 1 Tick（`freshSeed_`，`LocalPlayerPrediction.cpp:254-256`），差額計為丟時。這是設計上的行為。
  - 分析器（`gameplay_evidence.py` 的 `client_disturbance`，經 `command_evidence.py` 的 `match_life_seed_clamps`）只豁免「與 Match 的 LifeRespawn 配對」的夾住。檢查的是整份 trace，不只量測窗。
  - 失敗分兩類：
    - **A：連線時的初始 seed**（B1 4b 的 clean-60，t+0.037 秒）。
      - 連線瞬間的 seed 之後，第一幀 19.8 ms 超過 1 Tick，丟時 3.1 ms。
      - 對遊戲無影響，但分析器不豁免；是否發生取決於第一幀的長短，屬時好時壞的工具誤判。
      - B0 唯一沒有 gap 的 clean-60，正是第一幀剛好短於 1 Tick。
    - **B：主機卡頓後的停頓重設**（B1 的 clean-30，t+9.807 秒；B0 第一次的 clean-30 也是這類）。
      - 30 FPS 下主機卡了一幀（B1 41.1 ms，B0 57.7 ms），權威解析超過 Client 的命令尖端。
      - Client 依設計以中立命令重設（`LocalPlayerPrediction.cpp:180-183`），之後第一幀被夾而丟時。
      - Match 對該玩家沒有任何 reset，所以分析器判為未配對。這是真的小停頓，分析器抓它正確，但是否發生取決於主機的計時抖動。
      - **更正（2026-10-06，A/B 調查）**：B1 與 A/B 的觸發幀是 37～43 ms 的普通長幀（主機處於約 8 ms 晚醒狀態），不是卡頓。只有 B0 第一次是真正的 57.7 ms 卡頓：先走 covered gap，之後在取得相位期間又因 37 ms 的幀連續兩次停頓重設。根因見「第 07 批後續：交錯 A/B 調查」。
  - 與 IP-2、第 06、08 批無關：矩陣路徑的程式與 B0 相同，B0 也出現過兩類中的 B。
    - **更正（2026-10-06，A/B 調查）**：「程式相同」字面上不精確。Match 的機器碼相同，Gateway 只差 build ID；probe 只有 GYOP 編解碼改經 `Engine::Net`（FF-8，線上位元組相同）。結論不變。
- 2026-10-06：使用者決定「先修工具，再重取矩陣」。工具修正記為**第 07a 批**，與第 07 批同一個 PR（D18：同一條功能線），commit 與章節分開。

## 第 07a 批進度（記錄器）：驗收工具修正

- 2026-10-06：範圍只有 A 類。
  - `command_evidence.py` 新增 `is_session_start_seed_clamp`：豁免每位玩家一次「連線時初始 seed 的夾住」。
  - 判定條件是結構性的：
    - epoch 1、生命 1；
    - 序號與 pending 都等於初始命令數＋1（＝3，產品的 `InitialCommandLead`＝2）；
    - 沒有被擋的步；
    - 幀短於 100 ms；
    - 丟時恰好等於幀長減 1 Tick。
  - `match_life_seed_clamps` 先配對重生，其次接受一次初始 seed；`gameplay_evidence.py` 的錯誤訊息同步。
  - B 類（遊戲中途的停頓重設，序號遠大於 3）照樣判為干擾，不放寬。
- 2026-10-06：驗證：
  - 新測試（`test_command_evidence`、`test_gameplay_evidence`）：
    - 合法案例：幀 16.8／19.8／33.3／99 ms；
    - 不合法案例 9 種：停頓重設、pending 不同、第二條生命、第二個 epoch、有被擋的步、100 ms 幀、丟時不精確、沒有丟時、沒有生命欄位；
    - 每位玩家只豁免一次；
    - 停頓重設在 `client_disturbance` 中仍是錯誤。
  - 62 個測試通過；突變 5 個（拿掉序號、生命、每人一次、丟時精確、pending 的檢查）全部被抓到。
  - 以新分析器重新分析既有資料：
    - B1 4b 的 clean-60（A 類）改判通過；
    - B1 與 B0 第一次的 clean-30（B 類）仍判失敗；
    - 其餘原本通過的案例不變。
- 2026-10-06：新凍結工具清單 `build/target/_build/test/logs/pvp-v6-batch07a-20261006/frozen-tools.sha256`（52 檔，清單本身 SHA-256 `9e086f1d…`）。
  - 與第 02 批清單的差異：本批的 2 個分析器與 2 個測試檔，以及 FF-7 的 `gui_main.cpp`。
  - 之後重取 B1 的矩陣，以這份清單為準。
- 2026-10-06：以新凍結工具重取 B1 的矩陣（4c，同一棵 B1 tree，事前宣告追加在 `declare.txt`）：**25／25 通過**。
  - 第 07 批完成，B1 寫入 [基線](BASELINE.md)。
  - B1 tree 的 scratch worktree `GYO-Engine-b07` 移除；產物雜湊已記錄。
- 未結事項（B 類）：30 FPS 下主機卡了一幀（41～58 ms）後，Client 落在權威後面而停頓重設，clean 案判失敗。
  - 是否發生取決於主機的計時抖動，B0、B1 各出現一次。
  - 是否要加大 30 FPS 的領先餘裕，或接受為環境因素，由使用者之後決定。
  - 可與候選「相位估計器在 WAN 雜訊下的穩定性」一起看。
  - **更正（2026-10-06，A/B 調查）**：不是主機卡頓造成的環境因素，而是 30 FPS 相位追蹤的餘裕缺口的尾端，觸發條件是主機約 8 ms 的晚醒狀態，兩樹相同。見下一節。

## 第 07 批後續：交錯 A/B 調查（記錄器）

- 2026-10-06：起因：B1 的 clean-30 移動 Actual P95 為 41.0 ms（B0 26.9）。使用者認為降幅太大，指定以交錯 A/B 排除隱患。
  - 事前宣告：`build/target/_build/test/logs/pvp-v6-batch07-ab-20261006/declare.txt`。
  - A＝`6381e9d`（B0 來源）、B＝`5b0553a`（B1 來源），各自從零建置；分析器為第 07a 批的凍結工具。
  - clean-60、clean-30 各 6 輪，A、B 交替先後，共 24 次，機器閒置。
  - 第一次啟動因腳本預先建立輸出目錄，在建目錄時全部失敗，沒有執行任何 probe；紀錄保留在 `attempt1-invocation-error/`。
- 結果（移動 Actual，6 次的中位數 [最小..最大]，ms）：

  | case | tree | 通過 | P50 | P95 |
  |---|---|---|---|---|
  | clean-60 | A | 6／6 | 22.9 [22.5..26.5] | 37.3 [25.2..40.3] |
  | clean-60 | B | 6／6 | 26.8 [23.4..33.8] | 29.1 [25.6..37.9] |
  | clean-30 | A | 5／6 | 22.0 [19.9..26.2] | 37.9 [24.9..42.1] |
  | clean-30 | B | 4／6 | 21.7 [20.4..28.2] | 37.9 [25.6..40.3] |

  - 事前規則只在 clean-60 P50 機械性觸發（B 的中位數 26.8 大於 A 的最大值 26.5；差 3.9 ms，未達 5 ms）。依規則記為「疑似退化」，由下列調查推翻；規則本身不改。
- 2026-10-06：調查（使用者指定 ultracode：3 位調查者、1 位評審、1 次對抗檢查，全部 xhigh；唯讀，不再量測）。以下是經對抗檢查修正後的結論。
  1. **延遲差不是回歸。**
     - 矩陣路徑上 Match 的反組譯逐位元組相同，Gateway 只差 build ID。probe 只有 `wire::Encode`／`Decode` 改經 `Engine::Net`（語意與線上位元組相同）；預測、連線、幀迴圈的機器碼相同。
     - 矩陣的無頭 probe 不經過 `RuntimeLoop`／`PvpApplication`／SDL（符號表確認），所以矩陣也測不到 IP-2 與第 06 批。
     - 單次 P50／P95 以 1 Tick 量化，落點由開局時 probe 幀與 Match Tick 的相位決定。clean-30 的單次 P95 幾乎只會落在約 25 或 37～42 ms；A 樹自己就有 24.9 與 42.1，B0 的 26.9 與 B1 的 41.0 都在其中。
     - 事前規則在兩樹無差異時的觸發機率：P50 3～7%、P95 約 29%。單次的 before／after 比較沒有鑑別力。
     - 未解：clean-60 P50 在 6 對中 B 都較高（單尾 p＝0.021，四個統計量之一）。程式中找不到機制，只影響測試工具的開局相位，不影響每個命令的延遲。
  2. **clean-30 的失敗是真實的產品問題，不是標準造成的；與樹無關。**
     - 分析器正確：24 次從原始 trace 重算一致，非 Actual 全部是 Match 的 Held。
     - 主機的 sleep 晚醒在約 4 ms 與約 8 ms 兩種狀態間切換，隨時間而不隨樹：4 ms 狀態 clean-30 8／8 通過，8 ms 狀態 1／4。歷史上 B0 來源 2／8、B1 來源 3／8 失敗，所以矩陣 25／25 不是穩定性質。
     - 標準並不嚴：≥99% 比產品自己的「Held ≤0.5%」（[protocol-v5](../../protocol-v5.zh-Hant.md) §1 的已知取捨）寬鬆。4 ms 狀態 8 次中 4 次超過 0.5%；8 ms 狀態收斂後每位玩家 1.5～9.1% 被替代，8 個值中 4 個超過產品的連線品質門檻（每 10 秒 >5%）。8 ms 晚醒是 macOS 的常態（`command_evidence.py:45` 的註解）。
     - 標準唯一的問題是標籤：停頓規則的理由假設約 83 ms 的幀，實際觸發的是產品在 37～43 ms 普通長幀後自行重新播種。只改變一案（B 的 run-6）的判定，而那次確實丟了玩家輸入。
  3. **核心：30 FPS 下的相位追蹤**（兩樹機器碼相同）。
     - 餘裕太薄：30 FPS 一幀＝2 Tick，控制器把到達餘裕的 P90 對準「2 Tick lead＋4 ms」。固定步邊界落在幀喚醒的抖動帶內時，會出現 1 步幀接 3 步幀；3 步幀最舊的一步只剩約 4 ms，8 ms 晚醒時錯過自己的 Tick 而被 Held（最舊一步的遺失率：4 ms 狀態 4.7%，8 ms 狀態 72%）。
     - 控制器看不到：P90 只看餘裕大的一側；Client 每幀只用最新的 snapshot 做 `Reconcile`（`PvpApplication.cpp:391-395`，probe 相同），Host 每次發布後重設最小餘裕樣本（`MatchRuntimeHost.cpp:274`），30 FPS 約一半樣本到不了控制器。失敗跑次只有 38% 的晚到樣本被看到，「連續 2 個負樣本」的修正幾乎不觸發。
     - 失敗都從 8.37 秒開始：8 ms 狀態下首次誤差高約 1 Tick，超過 ±2 Tick 的上限而被夾住，殘差反而保護了前段；30 FPS 下 240 個樣本約 8 秒才滿（`Movement.hpp` 註解的「約 4 秒」只對 60 FPS 成立）。
     - 停頓重設是同一機制的尾端：Match 先 Held 掉 Client 尚未產生的序號，snapshot 又早於下一幀到達。
     - protocol-v5 的「CPU 模擬 ≤0.5%」只模擬了 2 步幀與 ±0.5 ms 抖動，沒有分析 3 步幀、8 ms 晚醒與晚到樣本遺失，是未分析的設計缺口。
     - 相位、首次修正被夾、8 ms 狀態三者在資料中共線，因果尚未以實驗拆開。
  4. **累積**：延遲不累積。累加器都有界；24 次中 18 次前後段的中位數差在 ±1 ms 內；v5 的 60 FPS 30 分鐘長測各窗口 P95 為 35.7～39.0 ms。
     - 但 30 FPS 的 Held 比率收斂後是平穩值，控制器看不見，重設後又回到同一相位；長局中可能連續 3 個窗口 >5% 而移出。這是推論：沒有 30 FPS 長測，`run_gameplay_soak.py` 的 `--soak` 也不接受 30 FPS。
     - 兩台機器的時鐘漂移（[v5 fix/08](../v5/fix/08-a1-clock-drift.md)）在同機測不到。30 FPS 下窗口約 8 秒、死區 ±2 ms，相位可能先往餘裕較小的方向漂移；列為殘留風險。
  - 證據：A/B 目錄的 `summary.txt`、`investigation.json`（workflow 結果）與 `scripts/`（分析與調查腳本）。
- 2026-10-06：使用者決定先修正 PR #56 的文件（本節與上述更正、[基線](BASELINE.md)、dev_log），門檻與分析器不動。待使用者決定：
  - 30 FPS 的缺口：(a) 接受為設計範圍的邊界，只更正 protocol-v5 與 `Movement.hpp` 的說明；(b) 另開一批修正相位追蹤，先以 CPU 模擬確認機制，以 xhigh 或 ultracode 規劃。調查建議 (b)。
  - 30 FPS 長局實驗（確認移出風險；長測需另外授權）。
  - A、B 兩個 worktree（`../GYO-Engine-abA`、`../GYO-Engine-abB`）保留到上述決定後再清理。
- 2026-10-06：使用者先選 (b)，以 ultracode 規劃第 15 批（完整觀測＋等待守門，先做 CPU 模擬）並核准計畫、寫下事前宣告（`build/target/_build/test/logs/pvp-v6-batch15-sim-20261006/`，未執行任何模擬）。
  - 規劃的主要發現：只讓控制器看到所有晚到樣本修不好；最舊一步晚到是因為等幀，換算成誤差和一般樣本相近，P90 不會移動。必須把固定步邊界提前。
- 2026-10-06：使用者改判為 **D21**：根因是命令放在主迴圈依畫面幀產生，交給 v7 的執行緒分離；第 15 批不執行，v6 只更正文件，30 FPS 寫成設計範圍的邊界。
  - 已更正：protocol-v5 §1 加有日期的追記（原文不改）、protocol-v6 §1、`Movement.hpp` 的兩段註解（只改註解）。
  - 30 FPS 長局實驗未排程，v7 分執行緒後的驗收再考慮。
- 2026-10-06：A、B 兩個 worktree 移除（使用者指示）；兩樹的產物雜湊已記在 A/B 目錄的 `artifacts.sha256`。

## Engine 批次對本產品的影響（記錄器）

- 2026-10-05：FF-8（Engine 的 GYOP 標頭編解碼）改了本產品的 `apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp`：標頭編解碼改用 `Engine::Net`，wire 版本收成 `wire::ProtocolVersion` 一個常數（值仍為 5），Type 範圍與 TCP frame 留在產品。
  [基線](BASELINE.md) 的 wire 版本號檢查表中 `Wire.hpp:26`、`:36` 兩處，因此變成這一個常數（第 09 批改值時以它為準）。
  位元組與拒絕集合以 `tests/object_fps_pvp/WireTests.cpp` 鎖住；權威 digest 兩樹比對相同。見 [基礎後續整理交接](../../../architecture/plans/foundation-followups/HANDOFF.md)。
- 2026-10-05：IP-1（Engine 輸入層）完成，PR [#50](https://github.com/yojinn-io/GYO-Engine/pull/50) 已合併（`56e0033`）：`Key` 擴充為完整鍵盤（含 `Tab`、數字列），`PhysicalInputFrame::events` 依序提供本視窗的按鍵、滑鍵（含座標與是否在視窗內）與視窗事件（失焦、移動、縮放含新寬高、縮小、還原）。
  [第 06 批](06-input-migration.md) 可以用它取代 `PvpApplication.cpp` 的 `HandleNativeEvent`（文字輸入與剪貼簿除外）；同一幀的點擊上升沿（`pendingShotEdge`）可由 `events` 的 `MouseButtonPressed` 取得。
- 2026-10-05：Engine 的 FF-1～FF-6、FF-8、IP-1 依序合併（PR #43～#50，master `56e0033`）。本產品批次的 Engine 依賴：第 06 批（IP-1）、第 09 批（FF-3、FF-8）已滿足；第 07 批仍等 IP-2，第 10 批與 FF-9 同一個 PR。
- 2026-10-05：FF-7（Engine 的 include 路徑統一）改了本產品的 include：公開 Engine header 一律在 `engine/<m>/` 之下（例如 `render/Renderer.hpp` → `engine/render/Renderer.hpp`、`platform/sdl/SdlPlatform.hpp` → `engine/platform/sdl/SdlPlatform.hpp`、`model_renderer/ModelRenderer.hpp` → `engine/render/model/ModelRenderer.hpp`）。
  本產品的程式、驗收、測試共 54 行一併改好（含第 08 批要刪除的未編譯檔）；之後新寫的 include 依這個規則。行為不變：權威 digest 兩樹比對 35 個情境相同。
- 2026-10-05：IP-2（Engine）在本產品的部分：
  - `PvpApplication::Run` 接上 live frame：縮放拖動中畫面與命令持續。live frame 不消費輸入，使用空的輸入幀，點擊與 R 的邊緣留給正規更新。
  - 慢事件處理的日誌多了 `observer_ms`、`live_frames`；sdl_gpu 在取得 ≥50 ms 時記錄 `fence_wait_ms`、`drawable_ms`。
  - L2：最差窗口由 30.5%／46.5% 降到 7.7%／15.9%，但 `CONNECTION POOR` 仍會出現。原因是按下縮放角到開始拖動的空窗（macOS），以及原因未明的 `nextDrawable` 停頓，依 D19 由 v7 的執行緒分離處理。
  - [第 07 批](07-measurement-baseline-b1.md) 的 B1 以這個狀態為準。

## 第 09 批進度（記錄器）：協議 pv6

- 2026-10-06：開始（使用者指示「繼續 09 批」）。前置都已合併：第 02、03 批，FF-3、FF-8；第 07、07a 批（#56，`c1ee2b2`）。分支 `claude/pvp-v6-batch09` 自 master `c1ee2b2`，同一 PR 帶上 #56 的合併狀態同步與 v7 任務清單的追加。
  - 檔位：使用者已開啟 ultracode；commit 0（契約定稿）以 ultracode 規劃（3 案、1 位評審、1 次對抗檢查；設計者 high，評審與對抗 xhigh）。之後主體 high，版本閘、解碼與 arena 拒絕路徑局部 xhigh。
  - 開始時以 grep 重新產生版本與名稱的檢查表（185 列，涵蓋 `apps/object_fps_pvp`、`build/acceptance/object_fps_pvp`、`tests/object_fps_pvp`，不含產生的 `.pb.go`）；逐列結果寫進 dev_log。
- 2026-10-06：commit 0 契約定稿（[protocol-v6](../../protocol-v6.zh-Hant.md)），以 ultracode 規劃（workflow `wf_315d6c56-0c0`；對抗檢查第一次因 API 529 失敗，續跑後完成）。對抗檢查判定成立：major 1（grep 允許殘留的類別太窄）、minor 12，全部套入。使用者核准。
  - 受擊：`CombatState` 11～13（`last_damage_tick`、`damage_count`、`last_attacker_id`），三欄全 0＝沒有受擊，解碼驗證 4 條（Go 與 Client 相同）。
  - arena：13 個成員依宣告順序的 big-endian 位元組，`Fnv1a64`；Ready 9、Welcome 10（fixed64），JSON 十進位；錯誤碼 `arena_identity_mismatch`、`arena_content_mismatch`（只在 Client 本機）。
  - 版本：只接受 6；產品 Go 由 `adapter.ProtocolVersion` 導出兩種型別；跨語言一致性測試為產品 CTest，以原始碼文字解析 Go／Python 定義。
  - 命名：`gameplay_v5`／`--gameplay-v5` 與 v5 測試名稱保留（v5 指遊戲版本）；指現行 wire 版本者改為中性或讀常數。
  - 大小：完整 Snapshot 545 bytes（契約值域內最大；型別最大值 585），v5 為 487／527。
  - 使用者決定：L1 的「同 Tick 多次命中」改測可達情境（同 Tick 互射、致命命中後死者同 Tick 的射擊被拒且不寫入），契約保留通用規則。
  - 計畫與程式的差異（行號偏移、漏列的 `WireTests.cpp:79-81` 與 `network_main.cpp:116-122` 位元組版本測試、HTTP join 回覆版本的截斷）寫進 dev_log，在對應 commit 處理。
- 2026-10-06：commit 1～4 完成（`708e877` 改名、`f255bf1` 版本 6、`fb6ec0a` 受擊欄位、`96ff97a` arena digest），每個 commit 都通過 `go test -race` 與完整 CTest（60／60）。突變 12 個全部被抓到。詳見 [dev_log](../../../dev_logs/2026_10_06_pvp_v6_batch09.zh-Hant.md)。
  - 權威不變：同機兩樹（base `c1ee2b2`，暫時的 worktree，用完已移除）35 個情境 0 不同，輸出逐位元相同。
  - 未以 L1 覆蓋：IpcHost 的 Snapshot 轉換（內部函式）；以程式審查與 L2 確認（Go 的規則 4 會讓漏寫在第一次死亡時暴露）。
  - commit 5（文件）：產品 `protocol/README.md` 改寫為 pv6、聯網架構文件的現行 wire 描述、dev_log。下一步：開 PR 取得四平台 CI，再以同一個 commit 做 L2（需要機器閒置，先徵求使用者同意）與 L3（使用者操作）。
- 2026-10-06：PR [#57](https://github.com/yojinn-io/GYO-Engine/pull/57) 開啟，四平台 CI 通過。使用者要求把手動的突變檢查做成產品自有工具：`tests/object_fps_pvp/run_mutations.py`、`mutations.json`、說明書 `run_mutations.zh-Hant.md`（commit 6 `8fbc2e5`），第 09 批 12／12 killed。
- 2026-10-06：L2（事前宣告 `e6fa12bf…`）：雙 GUI 短測 before／after 都通過；25 案矩陣 before 25／25、after 25／25；沒有疑似回歸。主機全程在約 4 ms 狀態。詳見 dev_log。下一步：L3（使用者）。
- 2026-10-06：L3（使用者）全部正常；arena 內容不一致的 Client 以 `arena_content_mismatch` 被拒（第一次因我的副本漏了 `lib` 沒有執行到，修正後重做）。取得視窗時的 `CONNECTION POOR` 對上已知的 `nextDrawable` 停頓（D19，v7）。**第 09 批完成**，PR #57 已合併（`5da939f`，2026-10-07）；暫時的 worktree（before／after）移除。下一批由使用者指定。

## 第 10 批進度（記錄器）：Collision 權威變更（與 FF-9 同一 PR）

- 2026-10-07：開始（使用者指示）。前置都已合併：第 03、09 批（#57，`5da939f`），FF-1。分支 `claude/pvp-v6-batch10` 自 master `5da939f`；Engine 端的 FF-9 在同一個 PR，紀錄寫在 Engine 計畫的記錄器（D12）。
  - 檔位：使用者同意以 ultracode 規劃事前宣告（3 案、1 位評審、1 次對抗檢查；評審與對抗 xhigh），容差、`IsValid` 規則、差異歸因局部 xhigh，其餘 high。
  - 事前宣告在寫程式之前完成，經使用者確認後寫進本批文件、dev_log 與 Engine 記錄器；之後不得事後放寬。
- 2026-10-07：事前宣告完成（ultracode，workflow `wf_28494fb3-383`；對抗檢查 major 1、minor 10 全部套入），使用者核准：`RaycastAabb` 用 double slab；新增 commit 0（分析器先凍結）、退化膠囊拒絕移到 commit 1、golden 不更新；arena v1 不升版（比照 D11⑨）。
  - 權威 digest 預期變化集合為空（35 個情境逐位元不變）；Engine 端 FF-1 語料恰好 435 筆不同。詳見本批文件的「事前宣告」與 [dev_log](../../../dev_logs/2026_10_07_pvp_v6_batch10.zh-Hant.md)。
  - 下一步：commit 0（擴充並凍結分析器），之後才寫 Collision。

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

- **第 04 批（2026-10-05）**：重現。拖動標題列時 render 停頓約 1.2 秒；縮放時停頓在事件處理（0.4～3.0 秒），不在 render。見 [基線 B0](BASELINE.md)。

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

- **第 04 批（2026-10-05）**：未重現（probe 路徑 18 張死亡期間截圖都沒有手臂；使用者決定不再以正式 Client 重現）。第 05 批不執行；若再看到，以正式 Client 並記錄時間重新開啟。

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
| `GroundPoint` 的有限性檢查三份、格子線段檢查兩份（屬未編譯的 29 檔） | 第 08 批：**已結案**（2026-10-05 隨刪除；`GroundPoint.hpp` 只剩型別定義） |
| `EnemyPresentationDefinition` 與 `EnemySystem` 的攻擊時間容差不一致（屬未編譯的 29 檔） | 第 08 批：**已結案**（2026-10-05 隨刪除） |
| Collision 統一對本產品權威判定的影響、權威 digest 不變的證明、`Arena::Validate`／`ShotQuery` 改用公開合法性檢查 | 第 10 批（與 FF-9 同一 PR） |

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

- 第 09～14 批未開始（第 05 批不執行），由使用者逐批指定。
- 30 FPS 相位追蹤的餘裕缺口（矩陣 B 類的停頓重設與 Held 替代）：依 D21 列為設計範圍的邊界，v7 處理；矩陣的 clean-30 在主機約 8 ms 晚醒狀態下仍可能判失敗。
- 已知要在批次開始時決定的事：
  - ~~FF-7 的 include 統一方向~~：2026-10-05 使用者決定 `engine/<m>/`，FF-7 已完成。
  - 第 11、13 批的呈現細節（D11⑥）。
  - 第 10 批與 FF-9 的容差選擇與事前宣告。
  - ~~IP-2 量測後的修法選擇（D7）~~：D19，分執行緒延到 v7。
- v7 的任務與規劃輸入（D19、D21 與 2026-10-06 的決定）已移到 [v7 任務清單](../v7/README.md)，以那裡為準。

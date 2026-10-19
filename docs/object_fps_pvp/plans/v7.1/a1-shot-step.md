# a1-shot-step：射擊照命令步判定（計畫與事前宣告，第 3 版）

- 狀態：計畫與第 3 節的宣告已核准（D137），實作完成（第 10 節）。**第 6 節（L2）第 3 版等核准**：工具、開發跑次與乾跑在第 11 節；核准後不放寬（AGENTS.md §14.1）。第 1 版經 D45 的門檻來源對抗式檢查（1 位 xhigh）：MUST-FIX 2、SHOULD-FIX 7、NOTE 7，第 2 版逐項修正（對應表在第 9 節）。
- 依據：`protocol-v7.zh-Hant.md` §6（A1 的判定語意、Client 規則）、`pv7-design.md` D-3 與「各批的測試清單」的 `a1-shot-step` 列、`network-architecture.zh-Hant.md` §7「射擊判定的冷卻保留與命令步」、HANDOFF D136（K＝min(K_max＝2, cooldownTicks)、S＝N）、`pv7-checkpoint` 的影子資料（`pv7-checkpoint-l2.md` 第 6 節、HANDOFF 2026-10-19）。
- 檔位：high；判定的到期、保留與順序（第 2 節 `a1-match`）局部 xhigh。估 20 h，超過約 30 h 停下回報。
- 數字沒有標「實測」的，是推導或推論。「模擬」指 D45 檢查者用 A1 規則重放 `pv7-checkpoint` 的 `match-shots.jsonl`（近似：假設冷卻以外的檢查結果不變、沿用 B 的送出時刻；`sim_a1.py` 在本 session 的 scratchpad）。

## 1. 改什麼、不改什麼

**改**

| 位置 | 現在（pv6 語意） | A1 之後 |
|---|---|---|
| `PvpMatch::ResolveActions`（`PvpMatch.cpp:268`） | 動作在交接 Tick A 就判定；順序 (A, PlayerId, ActionId) | 每個 Tick 先算每個未決動作是否到期（§6 的 due）；到期的在該 Tick 移動之後判定。只差冷卻且 gap≤K 時保留到 nextAllowedShotTick 重判一次（**不分路徑**：E＝0、late、epoch、life 的射擊也適用）。同一玩家較大的 ActionId 不早於較小的判定；跨玩家依 (A, PlayerId, ActionId) |
| 參考年齡（`PvpMatch.cpp:290`） | 判定時呼叫 `referenceAge(observedAuthorityTick)` | 在 `PvpMatch::Tick` 第一次看到該動作的 Tick（＝A，此時 `tick_`＝A）用同一個 callback 量一次，存在 `ActionEntry`，判定時用存下的值（D-3 (a)）。Host 不經手（D45 N3） |
| 冷卻保留上限 | 沒有 | `ShotHoldLimit(cooldownTicks)`＝min(K_max, cooldownTicks)；K_max 與 W 定義在 `Combat.hpp`（判定程式不依賴診斷用的 `MatchShotTrace.hpp`，D45 N4） |
| `PvpMatch` 的規則來源 | 全域常數 `PvpCombatRules` | 建構子加一個 `CombatRules` 參數，預設 `PvpCombatRules`；只給測試（冷卻 1、2 的情境）與將來的武器資料用（**介面擴充，要你同意**，第 8 節） |
| `MatchShotTrace.hpp`（影子紀錄 v1） | 記「若採 A1 會走的路徑」 | 版本 2：記 A、路徑、第一個到期的 Tick D、實際判定 Tick J、保留量、是否保留後重判。`ClassifyShotStep` 只做紀錄用的 A 時分類，判定一律由 `PvpMatch` 的 due 決定（它把 S＝0、L＝0 歸成 hold_limit，和 due 的「L≥S」不同，D45 S4a） |
| Client 射擊閘門（`PvpApplication.cpp:460-483`、`FireGate.*`） | `LocalFireGate`：由相位追蹤估最早 Tick，加 guard 2 ms、spread 7 Tick | 序號閘門（§6 Client 規則）：同一 epoch 內 T̂(S)＝snapshot.tick＋(S−snapshot 的 last_resolved_command)；放行條件 T̂(S) ≥ max(next_allowed_shot_tick, 已收到的接受裁決的 resolved_tick＋cooldown, 未決射擊的 T̂＋cooldown)；跨 epoch 保守退回用 snapshot 的 Tick。預測未啟用或凍結時不送、不排隊 |
| `WeaponFeedback.cooldownRemainingSeconds`（`PvpApplication.cpp:528-529`） | 來自 `fireGate.RemainingTicks` | 改成序號閘門的剩餘量；預測凍結時不得為 0（`action_short.hpp:270`、`:436`、`:890`，`gui_input.hpp:41` 依賴它，D45 S5） |
| `FireGate.cpp`／`.hpp`、`FireGateTests.cpp`、`ShotTiming` 的傳遞（`ClientSimulationRole`、`LocalPlayerPrediction`） | 存在 | 退役。`mutations.json` 裡 FireGate 的突變一起退役或改寫成序號閘門的，在變更說明逐條列出（AGENTS.md §14.4） |
| `CombatHostTests.cpp:343`（「E、S 不影響判定」） | 判定不讀 E、S | 改寫成 A1 的判定測試，列入變更說明（§14.4；D45 N5） |
| `test_diagnostic_isolation.py` | 守「S 與 view tick 不進判定檔」 | S、E 變成判定輸入，從守衛名單移除；view tick 仍是診斷，守衛保留 |
| 文件 | — | `network-architecture.zh-Hant.md` §1 的流程圖與 Ownership 表（`LocalFireGate`）、§7 修改入口表的 FireGate 列；README、HANDOFF |

**不改**：wire（E、S 已在 `hitbox-fields` 上 wire）、Gateway、IPC、Client 的移動預測與相位追蹤、`next_allowed_shot_tick` 的語意、拒絕原因的列舉（§6：不新增拒絕原因）、凍結的分析器（第 5 節）。

## 2. 實作切片

每片做完都跑本機全量 CTest；一起推送後觸發四平台 L1。

| 代號 | 內容 | 檔位 |
|---|---|---|
| `a1-match` | `ActionEntry` 加 A、在 A 量的參考年齡、保留狀態；`ResolveActions` 改成到期判定、保留重判、同玩家依 ActionId；`ShotHoldLimit`；`PvpMatch` 接受 `CombatRules` | xhigh（局部） |
| `a1-trace` | 影子紀錄版本 2；`shadow_cp.py` 的讀法另做（不在凍結工具內） | high |
| `a1-client-gate` | 序號閘門取代 `LocalFireGate`；預測未啟用或凍結時不送；`local_cooldown_blocks` 與 `cooldownRemainingSeconds` 的語意 | high |
| `a1-retire-firegate` | 刪 `FireGate.*` 與 `ShotTiming` 的傳遞；測試與突變改寫；`test_diagnostic_isolation.py` | medium |
| `a1-digest` | 新 digest 情境、`combat-magazine` 的 golden 重產（第 3 節）、兩樹比較 | high |
| `a1-docs` | 第 1 節的文件 | medium |

## 3. 事前宣告：權威 digest 與 golden

比較範圍：`compare_authority_trees.py`（base＝`7a5e0ea`）的 35 個共同情境 × scale 1、3；新情境在 base 不存在，會列為 `missing_in base`，不算差異（D45 N6）。

**預期差異：`combat-magazine/corridor`（scale 1、3）**，其餘 34 個情境 0 差分，golden 其餘 29 個 0 不符。

- 理由：冷卻保留不分路徑（`protocol-v7.zh-Hant.md` §6、D136）。`AuthorityDigest.cpp:418` 在每個 400 Tick 週期的 phase 0～156 每 4 Tick 開一槍（E＝0，所以 D＝A），冷卻 10、彈匣 12。不可以為了維持 0 差異把保留限制成只對 on_time（那違反 §6 與 D136）。
- 契約手算（每個週期；phase 是 `t % 400`；接受的射擊各自讓 nextAllowed＝J＋10）：

| | 接受（J） | Cooldown | EmptyMagazine |
|---|---|---|---|
| pv6（現在） | 0、12、24、…、132（每 12，共 12 發） | 4、8、16、20、…、128（gap 6、2 交替） | 136～156 |
| A1（K＝2） | 0、10（phase 8 保留 2）、20、30（28 保留 2）、40、50（48）、60、70（68）、80、90（88）、100、110（108），共 12 發 | 4、12、16、24、32、36、44、52、56、64、72、76、84、92、96、104（gap 3 以上） | 112～156 |

  phase 170 的 Reload、175 的 Reloading、180 的「已在換彈」、300 的 MagazineFull 不變。
- 判定：兩樹比較只在 `combat-magazine/corridor` 有差分；新 golden 與上表逐筆相同（以 digest 的逐 Tick 紀錄對照接受的 J 與拒絕原因）之後才重產，舊檔保留為 `authority_golden.pv7-checkpoint.txt`（`pv7-design.md:139`）。其他任何差異都停下回報，不改 golden。
- 其他情境已逐一核對沒有 gap∈{1, 2} 的射擊（D45 M1）：duel 每 15 Tick、cross-life 每 12 Tick（120／130 的兩發 gap＝0）、graze 每 12、host 每 25、rejoin 只有一發。

**新增的 digest 情境**（填 E、S，腳本化輸入）。每個情境先由契約手算預期的 D、J 與結果寫成表（證據來自契約，不來自實作，AGENTS.md §14.2），golden 只在實作的輸出與手算表逐筆相同時才寫入：

| 情境 | 預期 |
|---|---|
| on_time（S＝L(A)＋1、＋2、＋12） | D＝J＝T(S)＝A＋(S−L(A))（同一 epoch 每 Tick 恰好推進一個序號） |
| late（S≤L(A)，含 S＝0 且 L(A)＝0） | D＝A（L≥S 已成立；不可沿用 `ClassifyShotStep` 的 hold_limit 分類，D45 S4a） |
| epoch 不符（E＝0、E 為舊 epoch） | D＝A |
| life 不符 | D＝A |
| A 時 epoch 尚未解析任何命令，A＋W 前開始 | D＝T(S)（§6 的 due 定義優先於「上限」的描述，D45 S4b） |
| A 時 epoch 尚未開始，A＋W 前沒開始 | D＝A＋13 |
| S−L(A)≥13 | D＝A＋13 |
| A 之後、T(S) 之前 epoch 重設 | D＝重設那一 Tick（E≠Ecur），D45 S4c |
| 命令 S 被 Held／Neutral 替代 | D＝T(S)，射線起點取該 Tick 的權威狀態 |
| 冷卻保留 gap＝1、2（≤K） | 保留到 nextAllowedShotTick 重判並接受；J＝nextAllowedShotTick |
| 冷卻保留 gap＝3（>K）；冷卻 1、2 的規則下 gap＝2、3 | D 時回 Cooldown（`ShotHoldLimit` 取 min 的端到端證據，D45 S6） |
| 保留後重判時已死亡 | Dead，不再保留第二次 |
| 保留後重判時彈匣已空 | 構造：同一玩家先送較大 id 的射擊（將被保留），較小 id 的射擊之後才到並先判定、打掉最後一發；EmptyMagazine（D45 S4e） |
| Reload 排在保留中的 Shot 之後 | Reload 不早於該 Shot 判定 |
| 跨玩家：同一個 D、A 的順序和 PlayerId 的順序相反，而且結果看得出差別（兩人互射致死） | 依 (A, PlayerId, ActionId)；搭配突變「排序忽略 A」（D45 S4d） |
| 參考年齡：A 時 ≤250 ms、J 時 >250 ms | 接受（不是 Expired） |
| 參考年齡：A 時已 >250 ms | Expired，resolved_tick＝D（D45 S4f） |
| 射手在 T(S) 之前死亡 | Dead |

## 4. 測試清單（L1）與突變

- `pv7-design.md` 的 `a1-shot-step` 列逐項；加上：
  - `ShotHoldLimit(c)`＝min(2, c)：c＝0、1、2、3、10。
  - D∈[A, A＋W]、J∈[D, D＋K] 或等待同玩家較小 id、J−A≤W＋K（含連鎖的已知限制只發生在 S 不隨 id 單調的情況）。
  - 保留或等待期間 Client 與 link 重送的同一請求視為重複，不重判。
  - 不變式：同一 epoch 內照序號閘門送出時權威 Cooldown＝0（模擬 Client 依閘門連點 1000 發，到達延遲 0～2 Tick）；另加邊界案例：前一發 late 且 d＝K＋1 時下一發得到 Cooldown（D45 N7）。
  - 預測未啟用或凍結時，點擊不送也不排隊，`cooldownRemainingSeconds`＞0。
  - Client 序號閘門的三個上界各自起作用（next_allowed、已接受裁決、未決射擊），跨 epoch 的保守退回。
  - action short 的冷卻注入（`action_short.hpp:445-452`，E＝0）：靠「同玩家依 ActionId」仍在第一發之後判定並得 Cooldown（凍結分析器 `authority_cooldown_wall_and_hit` 的前提）。
- 突變（每個都要被殺）：到期改回「一律在 A」；保留不限 K；保留第二次；參考年齡改在 J 量；忽略同玩家的 ActionId 順序；排序忽略 A；hold_limit 不加 W；S＝0、L＝0 走 hold_limit；序號閘門少任一個上界；預測凍結時仍送。

- 等價突變（實作時發現，第 10 節）：「K 不取 min、固定 2」在行為上和 min(2, 冷卻) 相同，殺不到，不列入。

## 5. 凍結分析器的影響（盤點）

| 分析器 | 和 A1 的關係 | 結論 |
|---|---|---|
| `gameplay_evidence.py:64-74` 故障期間的 Expired 例外 | 以 resolved_tick 算年齡 >250 ms；A1 在 A 量、J≥A，A 時 >250 必定 J 時也 >250 | 方向安全 |
| `gameplay_evidence.py:77` 合法性重播 | 冷卻以 resolved_tick＋10 重播；A1 的 nextAllowed＝J＋cooldown | 一致 |
| `gameplay_evidence.py:178` 故障模式的 Cooldown | 只接受 at_seconds≤5.6 秒的 Cooldown | 模擬：A1 在故障情境造成的 Cooldown 0；開發跑次確認 |
| `gameplay_evidence.py:192-193`、`gameplay_soak_evidence.py:265-266` 合法射擊延遲 P95 ≤100（Match）／150 ms（Client） | A1 多 S−L(A) 個 Tick（clean 實測多為 1） | B 實測 Match 約 7.5～16 ms、Client 約 17～42 ms，加 1～2 Tick 後仍遠低於門檻 |
| `quad_evidence.py:33` 只允許 Dead／StaleLife | 任何 Cooldown 都判 F | 序號閘門＋clean：預期 0；開發跑次確認 |
| `combat_gui_evidence.py:234-236` 裁決抵達 P95 ≤150 ms | 實測 A 的 P95 約 33 ms（timing-gui-short-r1-A），推論 A1 後約 50～67 ms | 餘裕足夠 |
| `run_action_short.py` 的連點計數 | 每次點擊的 local block 要等於產品計數器 `local_cooldown_blocks` | 序號閘門保留這個計數的語意；開發跑次確認 |
| `action_short.hpp` 的冷卻注入與 `cooldownRemainingSeconds` 的等待 | 見第 1、4 節 | 開發跑次確認 |
| 影子紀錄的判讀（`shadow_cp.py`） | 版本 2 的欄位 | 新工具，不是凍結工具 |

凍結工具清單（`frozen-pv7-cp.sha256`）預期**不需要重新凍結**；若開發跑次顯示需要，停下回報再決定。

## 6. L2 事前宣告（第 3 版，等核准）

- 兩棵樹（都是 pv7）：B＝`../GYO-Engine-a1-before` 的 `7a5e0ea`（`pv7-checkpoint` 的 A 程式；從零建置，Gateway 建置資訊 `7a5e0ea`、modified＝false，D45 S7）；A'＝主 checkout 的 `43a8617`（之後只能改文件；Gateway 建置資訊 `43a8617`、false）。兩棵樹的 Match、Gateway、Client、6 個 probe 與 arena 寫在一份 `artifacts.sha256`（82 行；兩棵樹的 31 個資產檔逐檔相同）。兩棵樹的 `build/acceptance` 是同一份檔案，都以 `frozen-pv7-cp.sha256`（`f036b2af…`）核對。
- 交錯、閒置門檻、sleeper、受干擾規則沿用 `pv7-checkpoint-l2.md`（「同一情境同一樹 2 次或累計 6 次」）。
- 判定工具：`pv7-checkpoint` 的 `judge_cp.py` 修掉 `schedule()` 的標籤錯誤（D135）後改成兩棵 pv7 樹的版本（`judge_a1.py`）。兩棵樹都用 pv7 的檢查（R4 的結果間隔、K1a、R8、T1、H1、執行檔身分）；凍結判定的 F 兩棵樹都停（沒有「B 基準的已知限制」）；timing 兩棵樹都用 v7 HUD 規則（`timing_v7_a.py`）。延遲注入兩棵樹都跑。
- 每棵樹 119 項，共 238 項；時間約 2.5 小時（`run_a1.py --list-only`：152.0 分加閒置門檻）。

| 部分 | 項目 | 每棵樹的次數 |
|---|---|---|
| I | 25 案（同 `pv7-checkpoint`；含 loss-*、duplicate-reorder-conflict、cross-life、downstream-death／respawn：等待或保留期間的重送去重、life／epoch 退回都在這裡，D45 M2） | 3 |
| I | 背壓、網路一般、網路 impairments | 各 3 |
| I | quad clean-60（5 輪） | 1 |
| I | 長測短模式 | 3 |
| S | Gateway→Match 延遲注入 T＝2.0、11.5 | 各 3 |
| G | 7 個 GUI 項目（同 `pv7-checkpoint`） | 各 1 |
| G | 連點 6 格（interval12、edge × 30／60／144） | 各 3 |

**每項的停止條件**：`pv7-checkpoint` 的 R1、R1s、R3、R3f、R4、R8、K1a、T1、H1、D 照舊，兩棵樹都判（R8 判的 11 個情境都在清單內）；R2（clean 與連點的權威 Cooldown＝0）只判 B，A' 改判 A1-1；B 的影子紀錄（版本 1）照 `pv7-checkpoint` 判 H2、H3，A' 的紀錄（版本 2）判 A1-2；紀錄版本不符是工具無效。另加：

| 代號 | 條件 | 來源 |
|---|---|---|
| A1-1 | A' 的 clean 項目（clean-30／60／144、長測短模式、quad clean-60、連點 6 格、timing 兩項、quad-gui-bots；action short 與 bots2-action60 的冷卻注入不在清單）：每筆 `rejection`＝cooldown 的射擊紀錄都要有解釋，否則停。解釋（機械定義，依 J 排序的同一玩家）：(i) 前一發射擊不是 on_time 路徑；(ii) 這一發是 on_time 但 D 不等於 A 時預計的 step_tick（A 到 T(S) 之間 epoch 重設）。有解釋的記為 A1-3 | 實測：`pv7-checkpoint` 的這些項目 late＋epoch＝0（注入除外）；契約本身允許 d＞K 的 Cooldown（`network-architecture.zh-Hant.md` §7） |
| A1-2 | A' 的紀錄（版本 2，`shadow_a1.analyze_v2`）：D∈[A, A＋W]；on_time 的 D 是 T(S)：同一 session（tick 回落切分，同 H2）內 (player, E, life, S) 的 resolved 事件的 `authority_tick` 等於 D；不等時要有解釋（同一玩家在 [A, D] 內有 epoch 大於 E 的 reset、同鍵的 `lifecycle_cancelled`、或 trace 在 D 之前結束），否則停；J∈[D, D＋K]，或 J＞D＋K 時同一玩家有較小 id 在 J 判定；J−A≤W＋K | `protocol-v7.zh-Hant.md` §6 的上界 |
| A1-3 | A' 的保留次數、「gap＞K 的 Cooldown」次數只記錄 | D136 |

**結束時的判定**（只在完整 session 上判）：

| 代號 | 指標 | 規則 |
|---|---|---|
| C1、C2 | 移動「產生→Actual」P95、P50 | 照 `pv7-checkpoint`：median A' − median B ≤ max(range B, 1 ms)（A1 不改移動） |
| C3 | 故障情境 Actual 比例 | 照 `pv7-checkpoint` |
| C6 | 每案第一筆裁決的拒絕原因分布 | 照 `pv7-checkpoint`（A' 的每跑次分布等於 B 某一跑次），允許的差異事前列出、只記錄：保留後改判接受（Cooldown 少、接受多）、A1-1 (i)～(iii) 的 Cooldown；預測來源：模擬的故障情境 A1 Cooldown 0、保留只有 upstream-1000ms 3 次（gap 2） |
| C9 | A' 的 clean 項目（A1-1 的清單）的退回率（late＋epoch）／全部紀錄 | 每個有紀錄的 clean 情境都是 0（`pv7-checkpoint` 的 A 實測 0）；沒有任何 clean 紀錄時不成立 |
| C10 | 每發的 J−A（影子紀錄 v2）；合法射擊 Match 端延遲 P95 | 只記錄；預測 clean 的 J−A 多為 1 Tick（`pv7-checkpoint`：clean-60、144 的 S−L(A) 全為 1，clean-30 有 88% 為 1）。合法射擊 P95 不能跨建置比較（`pv7-checkpoint-l2.md` 第 2 節），只列兩棵樹的值（D45 N1） |
| C11 | 連點的本機擋下數、送出數 | 只記錄（FireGate 退役後的手感資料） |

L3：使用者實際連點的手感（30／60／144 FPS），和 v7 的 L3-3 同一份腳本。

## 7. 預測

| 代號 | 預測 | 推翻條件 |
|---|---|---|
| P1 | 兩樹比較只有 `combat-magazine/corridor` 有差分，而且和第 3 節的手算表相同 | 其他差異，或和表不同 |
| P2 | A' 的 clean 項目權威 Cooldown 只有注入的那幾發，保留次數 0 | 出現其他 Cooldown，或 clean 有保留 |
| P3 | A' 的 clean J−A：on_time 的 1 Tick 佔多數 | 1 Tick 不到一半 |
| P4 | 凍結分析器不需要重新凍結 | 開發跑次需要改分析器 |

## 8. 等你決定

1. 核准本計畫與第 3 節的宣告（`combat-magazine/corridor` 的預期差異與 golden 重產；其他 0 差異是停止條件）。
2. `PvpMatch` 的建構子加 `CombatRules` 參數（預設 `PvpCombatRules`）：讓「K 取 min」可以用冷卻 1、2 的規則端到端驗證，也是武器資料驅動的入口。這是 `PvpMatch` 公開介面的小擴充（Architecture Delta 的「Existing Interface Extension」）。不做的話，「K 固定 2」的突變只能在 `ShotHoldLimit` 的單元測試裡被殺，在 `PvpMatch` 的呼叫處寫錯會漏掉（事前宣告它存活）。
3. 第 6 節是草稿：工具寫好、乾跑之後出第 3 版，門檻的字面值在那時交你核准。

## 10. 實作紀錄（2026-10-19，D137 之後）

- `a1-match`：`Combat.hpp` 加 `MaxShotStepLead`（W，從診斷檔移來）、`MaxShotHoldTicks`、`ShotHoldLimit`；`PvpMatch` 加 `CombatRules` 參數（並檢查 ValidShotDamageBounds、彈匣 >0、射程、參考年齡上限）、`ActionDue`、`Decide`、`GetActionTiming`；`ResolveActions` 依 (A, PlayerId, ActionId) 排序，一位玩家的動作依 ActionId 填進自己的位置，等待中（未到期或保留中）的動作讓較大的 id 一起等。參考年齡在 `Tick` 第一次看到動作時（A）用既有的 callback 量一次（D45 N3）。
- **等價突變**：gap＝nextAllowed−D，nextAllowed＝上一發的 J＋冷卻、D≥J，所以 gap≤冷卻，K＝min(2, 冷卻) 和 K＝2 的判定永遠相同；D45 S6 的「K 固定 2」突變殺不到，不列入。`CombatRules` 參數改用來端到端測冷卻 2、4（快武器）的保留與拒絕、一發致死的跨玩家順序、彈匣 2 的保留中打空。設計文件與契約同步寫明（`network-architecture.zh-Hant.md` §7）。
- 改寫的既有測試（AGENTS.md §14.4）：`CombatTests`「exact cooldown boundary」原本在冷卻內 1 Tick 開槍並預期 Cooldown，A1 會保留到邊界接受，改成在冷卻內 3 Tick（>K）預期 Cooldown，保留另有新測試；`CombatHostTests`「E、S 不影響判定」改成「view tick 不影響判定」（E、S 已是判定輸入；原寫法在 A1 下會因 E 不符而空洞地通過，D45 N5）；「影子紀錄的冷卻保留量」改成「保留的射擊在判定時記錄」；`test_diagnostic_isolation.py` 移除 S 的檢查（view tick 保留）。
- `a1-trace`：`match-shots.jsonl` 版本 2（`protocol-v7.zh-Hant.md` §6）。
- `a1-client-gate`、`a1-retire-firegate`：`ShotGate.hpp`／`.cpp`（`EstimatedShotTick`、`LocalShotGate`）取代 `FireGate.*`；`LocalPlayerPrediction::ShotGate()` 在預測未啟用或凍結時回 nullopt；`ClientPresented` 的 `shotTiming` 改為 `shotGate`；`WeaponFeedback.cooldownRemainingSeconds` 改讀序號閘門，沒有 view 時至少 1 Tick。`ShotGateTests.cpp` 取代 `FireGateTests.cpp`，含與真實 `PvpMatch` 的閉環測試：每 Tick 點擊、Client 看到前一 Tick 的 snapshot，到達晚 0～2 Tick 時 113 發權威 Cooldown 0；冷卻 4、裁決晚 6 Tick 時，一發晚 2 Tick 讓下一發保留、晚 3 Tick 讓下一發 Cooldown（N7 的邊界）。
- 突變：`v71-a1-shot-step` 19 個（新增 18 個，加上由 `v71hf-shadow-cooldown-ignored` 改寫的 1 個），另外更新 `v71hf-shadow-time-unset` 的定位、更新定位 5 個（縮排）；退役 FireGate 的 10 個（`12-earliest-plus-one`、`12-no-guard`、`12-shift-ignored`、`12-no-pending-spread`、`12-decision-ignored`、`v7-09-guard-minus-half-ms`、`v7-09-guard-plus-2ms`、`v7-09-spread-6`、`v7-09-spread-10`、`v7-09-guard-plus-50us`：所測的常數與估計已不存在，對應的序號閘門突變取代其中能對應的）。新增與改寫的全部殺死。
- `a1-digest`：A1 下 `combat-magazine/corridor` 的逐筆判定（`--dump`，scale 1 與 3 的每個週期）和第 3 節的手算表完全相同；golden 只改這一行，舊檔為 `authority_golden.pv7-checkpoint.txt`。第 3 節的新情境表由 `CombatTests` 的 A1 案例逐項涵蓋（契約手算的 D、J）；另加 digest 情境 `combat-step/corridor`（零角度、進 golden，四平台逐位元比對）：準時 lead 3、13、超過 W、S＝L(A)、gap 6 拒絕、gap 2 保留、Reload 排在保留中的射擊後、E＝0 的保留、同 Tick 在保留的較小 id 之後判定，11 個判定和情境旁的手算表逐筆相同（scale 3 每個週期相同）。
- 同機兩樹比較（base＝`7a5e0ea`，新 worktree `../GYO-Engine-a1-before`）：scale 1、3 各 36 個情境，只有 `combat-magazine/corridor` 不同；`combat-step/corridor` 只在 branch（base 沒有）。其餘 34 個 0 差分，P1 成立。證據在 `build/target/_build/test/logs/pvp-v71-a1-shot-step-20261019/`。
- 突變的完整重跑：19 個中 15 個殺死、4 個 `wrong_failure`，原因都是 cpu 測試超過 CTest 的 60 秒逾時（當時 base 樹在同機完整建置）；機器空閒時重跑這 4 個全部殺死。

## 9. D45 檢查的修正對應（第 1 版 → 第 2 版）

| 檢查項 | 修正 |
|---|---|
| M1 保留不分路徑，`combat-magazine` 一定有差 | 第 3 節改成預期差異＋手算表＋golden 重產；其餘 34／29 維持 0 |
| M2 Part I 拿掉動作路徑的故障情境 | 恢復 25 案、背壓、網路；沿用 C6 並列出允許的差異與預測來源 |
| S1 A1-1 不是契約不變式 | 來源改成實測；有解釋的 Cooldown 只記錄；列出 clean 項目並排除注入 |
| S2 C9 沒有範圍 | 排除 `cooldown_injection_action_id` |
| S3 A1-2 把 D 和 J 混在一起 | 分開判 D、J、J−A，H3 的解釋照舊 |
| S4 新情境缺項 | 補 S＝0、epoch 晚開始、A 到 T(S) 之間重設、跨玩家順序、保留中彈匣打空的構造、A 時已過期 |
| S5 盤點漏項 | `cooldownRemainingSeconds`、兩個延遲門檻（附 B 的實測）、`quad_evidence.py:33` |
| S6 「K 固定 2」的突變殺不到 | 第 8 節第 2 項 |
| S7 B 的二進位會被覆蓋 | B 在另一個 worktree 重建並宣告新雜湊 |
| N1 C10／P3 的量不能跨建置比 | 改用影子紀錄 v2 的逐發 J−A |
| N2 150 ms 的推論 | 成立，保留 |
| N3 參考年齡的量測點 | 留在 `PvpMatch::Tick(A)` 的 callback，Host 不經手 |
| N4 W 定義在診斷檔 | W、K_max 移到 `Combat.hpp` |
| N5 `CombatHostTests.cpp:343` 會空洞地通過 | 改寫並列入變更說明 |
| N6 新情境在兩樹比較裡是 missing | 寫明比較範圍是 35 個共同情境 |
| N7 不變式只證明 d≤K | 補 d＝K＋1 的邊界案例 |

## 11. 工具、開發跑次與乾跑（第 3 版）

工具在 `build/target/_build/test/logs/pvp-v71-a1-l2-prep-20261019/`（不進版本庫）。session 的命令（核准後照這行跑）：

```
caffeinate -dims python3 -I -B run_a1.py --output ../pvp-v71-a1-l2-<日期> \
  --driver-sha dbd7e999123495c062e38ad6e659a0354bc25505003f3a468781068f42b9a73a \
  --judge-sha f967e6513450c82e6564c6caf144158f1ab438f1e8081f316475f34a1303e925 \
  --artifacts-sha 034a16ee5f22c86de30a862e181fc8b42bebe29ca9d61f30fcf3f8812671651e
```

| 檔案 | sha256 | 內容 |
|---|---|---|
| `run_a1.py` | `dbd7e999…` | driver（`pv7-checkpoint` 的 `run_cp.py` 改兩棵 pv7 樹） |
| `judge_a1.py` | `f967e651…` | 排程（238 項）、逐項判定（A1-1 在內）、結束判定（C9 在內） |
| `shadow_a1.py` | `be6af78f…` | 版本 1 的 H2、H3（附版本檢查）與版本 2 的 A1-2（`judge_a1.py` 釘住） |
| `timing_v7_a.py` | `f48d3783…` | 同 `pv7-checkpoint`（兩棵樹的驗收模組是同一份） |
| `artifacts.sha256` | `034a16ee…` | 兩棵樹的程式與資產 |
| `commands.txt` | `2326f5ac…` | 90 種項目的完整命令 |
| `prep-files.sha256` | `7a6f27fb…` | 以上與 `make_fixtures.py`、`dryrun.py`、`sleeper.py`、`dryrun-stdout.json` |

匯入與 `pv7-checkpoint` 相同（`judge_16b.py`、`judge_16.py`、`judge_c2.py`、edge-rate-limit 的 `judge_l2.py`、`sleeper.py`，各自核對雜湊）。

**開發跑次**（只看分類、停止、旗標，不看第 6 節的指標）：A' 45 種項目各 1 次（25 案、背壓、網路兩種、長測短模式、quad 5 輪、延遲注入兩個 T、G 7 項、連點 6 格），B 5 種（clean-60、延遲注入 2.0、quad、連點 edge-60、timing）。全部分類 P（延遲注入為 R），沒有停止；只有 `duplicate-reorder-conflict` 的 conflict 拒收旗標（情境設計）。A' 的 clean-60：版本 2 的紀錄 43 筆，on_time 41 筆的 D 全部等於 resolved 事件的 Tick。

**乾跑**（`dryrun.py`，`dryrun-stdout.json`）：

| 部分 | 列數 | 結果 |
|---|---|---|
| 逐項：開發跑次 50、B 的夾具 30（`pv7-checkpoint` 的同一批，改成 B 樹的項目）、A' 的夾具 9（A1-2：D 超出範圍、on_time 的 D 不是 S 的 Tick、有重設解釋、J 超過 D＋K、J 等較小 id；A1-1：沒有解釋、前一發 late 的解釋；版本 1 的紀錄放在 A'；只改 `actions.jsonl` 的 Cooldown 不影響 A'）、交叉 2（B 的紀錄判成 A' → 版本不符；A' 的紀錄判成 B → 版本不符）、受干擾一次、執行檔身分 | 93 | 全部如預期 |
| 突變：H2 不分 session、T1 忽略 dropped、H1 不判、A1-2 沒有 D 的範圍、A1-2 用任意 Tick 對接、A1-2 的 J 不認較小 id、A1-2 沒有 J 的範圍、A1-1 一律有解釋、B 的 R2 不判、A' 用版本 1 判、K1a 不判、clean 不判 R3 | 12 | 全部殺死 |
| driver：每項命令只用自己那棵樹的路徑、延遲注入／連點／quad 的參數、拒絕重寫 artifacts、錯的 driver 字面值預檢不過、正確的預檢通過、開發跑次拒絕已存在的輸出 | 8 | 全部如預期 |
| 模擬 session（連到兩棵樹的開發跑次）：完整、停下、不完整、順序錯、受干擾 2 次、累計 6 次、B 的 R3f 第 3 項 | 7 | 全部如預期 |
| 第 5 節（合成數字）：C1、C3、C6 同 `pv7-checkpoint`；C9 的成立、clean 有 late 時不成立、沒有 clean 紀錄時不成立 | 14 | 全部如預期 |

## 12. L2 結果（2026-10-19，D138）

- session：`build/target/_build/test/logs/pvp-v71-a1-l2-20261019/`，238／238 完成、0 停止、0 受干擾、完整性問題 0，145.4 分（估 152 分）。判定（`judgement.json`）**`passed`＝true**：C1、C2、C3、C9 全部成立。
- C1／C2：移動「產生→Actual」A'−B 最大 +0.213 ms／+0.149 ms（host-ipc-1000ms），下限 1 ms。C3：16 個故障情境都成立。C9：A' 的 14 個 clean 情境退回率都是 0。
- A1-1：A' 的 clean 項目權威 Cooldown 0、保留 0（P2 成立）。保留只出現在 upstream-1000ms（3 次，gap 2），和 D45 檢查者用影子資料模擬的預測相同。action short 4 次、bots2-action60 1 次 Cooldown 是冷卻注入（不在 A1-1 的清單）。
- A1-2：所有 A' 的紀錄 D∈[A, A＋W]、on_time 的 D＝T(S)、J 的上界都成立。J−A：clean-60、144 幾乎全為 1 Tick，clean-30、連點、quad 有 2 Tick；clean 項目 1376／1780 筆為 1（P3 成立）。
- C6 旗標（只記錄）：downstream-1000ms、socket-path-1000ms 各有 1 個 A' 跑次的第一筆裁決少 1 個 Expired、多 1 個接受（`pv7-checkpoint` 的 A 在 socket-path-1000ms 也有同樣的旗標）。
- C10（只記錄；合法射擊 P95 的中位數，不能跨建置比較）：Match 端 B 約 13.2～14.5 ms、A' 約 30.6～31.0 ms（約 +17 ms＝1 Tick）；Client 端 clean-60 19.6→52.7、clean-30 36.7→66.7、clean-144 22.3→36.2、長測短模式 22.9→38.6 ms。和計畫前的推論（命中回饋慢約 20～37 ms）一致。
- C11（只記錄）：連點每次送出 31～32 發，interval12 三格都是 44／44；edge 格 A' 比 B 多送 0～1 發。
- 下一步：L3（使用者的連點手感，30／60／144 FPS）。


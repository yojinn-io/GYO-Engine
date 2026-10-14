# 第 09b 批：相位追蹤的既有缺陷與延遲位移

狀態：**規劃完成（草案），待使用者確認「需要決定的事」**（2026-10-10）。PR 線 P2（PR #73）。排在第 09a 批之後、第 09 批的 M0 與常數凍結之前（D47⑤ 的更正）。
檔位：規劃 ultracode（使用者 2026-10-10「09b 的規劃開 ultracode，檔位照建議」）：方案 3 位 high（缺陷、延遲位移、交互與風險）、評審 high、對抗式檢查 xhigh。

這份草案依使用者的要求（memory「decision points for learning」），每一項都寫出機制、為什麼重要、選項與代價、決定它的證據在哪裡，以及途中被推翻的說法。

## 先讀這一段：最重要的發現在 09a，不在 09b

- **機制**：Client 相位追蹤的 late 修正，在遲到來自傳輸路徑時會被夾在 2 Tick（−33.3 ms，`LocalPlayerPrediction.cpp:97-98`、`Movement.hpp:59`），命令領先從 2 變成 4。v7 的穩態每 Tick 排隊 2 個命令，30 Tick 合計 60（v6 是每 Tick 1 個、合計 30），所以 backlog 門檻 105 的餘裕只有 45（v6 是 75）。領先 4 時約 23 Tick 後合計 ≥105，epoch 重設，這就是 D44 的推論鏈。
- **新的證據**（對抗式檢查重新分類 `defects/late-outcomes.jsonl`）：真正的 late 修正有 5 次（另外 4 筆是停頓 reseed 之後的首次決定，原本被誤算進來）。送出被釘住的 4 次，30 Tick 合計停在 89～99，沒有重設；沒被釘住的 1 次就是 D44（合計 108，Tick 622 重設）。被釘住時命令晚送 3～10 ms，吃掉了一部分多出來的領先。
- **為什麼重要**：第 09a 批要修的正是「送出被釘住」。修好之後，這個「意外的緩衝」就沒了，傳輸型的 late 修正很可能每次都變成 backlog 重設。這符合 D44 的提前條件（`09-firegate-c2.md`：「決定 1 選了修正，而修正確認會改變遲到修正或 backlog 的行為」）。樣本只有 5 次，結論是方向一致的小樣本，不是證明。
  - 加註（2026-10-10，D51 ⑨，使用者核准）：「意外的緩衝」是在截斷的資訊流上看到的現象。被釘住能吸收多出來的領先這件事仍是 09a 的機制（保留），但「傳輸型 late 修正」有多常發生，要在修好資訊流之後重新量；見[事故紀錄](Incident/2026-10-10-gateway-silent-drop.md) 3.4（R12）。
- **所以 09a 的實作先停下**，等你決定 D44 要不要提前（見「需要決定的事」1）。
- **證據位置**：`build/target/_build/test/logs/pvp-v7-09b-plan-20261010/adversarial/step_context*.txt`、`queue_recount.txt`；`defects/late-outcomes.jsonl`（原始 9 列）。D44 的完整 trace 在 `pvp-v7-batch08-20261009/dev-backpressure/host-ipc-250ms/`。

## 09R-eval：在修好的資料上的狀態（補充，2026-10-11）

第 08c 批（Gateway 丟棄回報）的 L2 通過之後，09R-eval 在修好的資料上重新檢查了[事故紀錄](Incident/2026-10-10-gateway-silent-drop.md) 3.4 列為「重做」「判斷不了」的項目。L2 是 `build/target/_build/test/logs/pvp-v7-08c-l2-20261011/session/`；分析是 `build/target/_build/test/logs/pvp-v7-09r-eval-20261011/`（`report-from-agent.md` 第 1-b 節；「flow」是只修資訊流的樹，「flow+09a」是加上 09a 原型的樹）。狀態分四種：**成立**、**加限定**、**要重量**（等 09R-map，D63 ①）、**不再需要**。

| 項目 | 狀態 | 依據（L2） |
|---|---|---|
| R1 第①步普查的次數 | 不再需要（以 L2 取代，不重做舊普查） | 修好的樹 clean 16 次、network 32 次，late 修正都是 0；注入以外，非 respawn 的重設 0 |
| R2 D44 的頻率與歸因 | 成立（clean／network 型）；短延遲型要重量 | clean／network 型的流量上 0；Client→Gateway 短延遲（單次 40～80 ms、持續 100～300 ms）L2 沒有案例 |
| R5 backlog 餘裕需要多少 | 加限定 | network40 故障解除後 30 Tick 合計 90（餘裕剩 15），三棵樹相同，第①步第 5 點成立；「需要多少」仍取決於短延遲的區域（要重量） |
| R7「領先＝2」的前提 | 成立（閒置主機的穩態） | clean 100％ q＝2；network 83～94％，network40 有 6.5％ q＝3；注入以外沒有領先 4（`lead_share.txt`） |
| R8 Cooldown 拒絕的發生率 | 加限定 | clean 0；network before 4、flow 1、flow+09a 1（各 16 跑次），全是玩家 1、故障中段，那些跑次 late 修正 0；推論是 network 抖動壓縮了射擊間隔，和 late 修正無關，屬 FireGate「常數涵蓋不了的失敗」，作為 M0 的輸入。network 的遺失是固定種子，跑次不是獨立樣本 |
| R9 08b 的「late 修正 0 次」 | 成立（clean／network 型） | 修好的樹 clean、network 的 late 修正 0 |
| R10 有損傷網路下的延遲位移 | 不再需要 | 決定 4 只改文件，結果不會改變任何決定 |
| R11 注入的位置與方式 | 不再需要 | L2P-injector 已提供 Client→Gateway 的純延遲注入 |
| U1「脆弱度以 Held／starvation 的形式看不見」 | 加限定（持續延遲成立）；短延遲要重量 | x1 的替代 before 84.8 對 flow 84.3‰，重設時刻相同：持續延遲的區域裡，修好資訊流沒有改變 Match 的結果 |
| U4 閒置主機的「0 筆」 | 成立 | clean 的替代三棵樹都是 0 |
| U7 network 的故障解除事件 | 成立（同 R5） | network40 每次都是 90，約在故障解除後 0.47 s（`q30.txt`） |

- 另外兩項（不在上表）：缺陷 B 的影響大小（R3）是新證據，已觀察到串接，見「缺陷 B」一節的更正；09a 的 R12「修好之後 D44 變多」要重量，見 [09a](09a-input-send-pinning.md)「為什麼重要」3 的補充。缺陷 A（R3）成立、不必重量：clean 的修好樹 late_first 0、替代 0，≤0.27 ms 仍適用（推論）。
- D48 第③步的 J3（延遲比重新擷取長時擋不住）歸「保留（條件力學）」（D63 ②）。
- 之後 09a／09b 的 L2 必須包含 Client→Gateway 的短延遲注入；late 修正用凍結 D48 偵測器的計數器增量，不用 `judge_l2.py` 的最大值（D63 ④）。

## D48 第①步：既有證據的普查（2026-10-10，只讀）

分析 1 位（high）＋對抗式核對 1 位（high）。證據：`build/target/_build/test/logs/pvp-v7-d48-step1-20261010/`（偵測器 `detector.py` 在普查前凍結，sha256 `445271ce…`；核對在 `adversarial/`）。

**結論：既有證據已經用盡，「被釘住擋住 D44」的 4 對 1 無法檢定；但它背後的機制有了更精確、可以被推翻的形式。**

1. **樣本沒有擴大**：498 個有 trace 的 v7 跑次中，tracking 型、約 −33 ms 的 late 修正只有 09b 已知的那 5 筆，而且 M1／M2 是同一跑次、同一次主機停頓，實際只有 4 個獨立事件。閒置主機的 111 個跑次（3,550 玩家·秒）是 0 筆。另有 23 個 `run_network` 跑次沒有命令 trace，其中包括 D44 的第 1 次（08 `dev-network`），無法分類。
2. **「被釘住」用錯了變數**（被推翻的說法）：09b 的 4 對 1 用的是修正**之後**的送出等待。依 D48 凍結的定義（修正**前** 1 秒的中位數 >2 ms），5 筆都沒有被釘住，2×2 表的「被釘住」那一列是空的。而修正後的等待是 slew 本身造成的：slew 期間命令以 75 Hz 產生、token 以 60 Hz 補充，等待每個命令增加約 3.3 ms，到約 16.7 ms 繞回；slew 結束時的值就凍結下來（09a 的中性穩定）。所以「修正後被釘住」不是獨立的暴露變數。
3. **真正的變數是到達 Match 的領先**：Match 端領先（resolved − host_accepted）≈ 修正前的領先＋33.3 − W_post（W_post＝slew 結束時凍結的送出等待）。它超過約 66.7 ms（4 Tick）時，每 Tick 排隊 4 個，約 25 Tick 內 30 Tick 合計 ≥105。5 筆和這條邊界單調一致：M1／M2 57.6／57.8 ms→合計 90；M5 60.7→97；M4 63.5→66.1→89（trace 1.24 秒後結束，右設限）；M3（D44）69.7→108、重設。模型對 M3 的預測 69.7、實測 69.7，其餘差 1～3 ms。
4. **M5 不是乾淨的反例**（被推翻的說法）：分析原本把 M5（network20，修正後等待 4.38 ms 仍然重設）當成反例；核對發現它的重設發生在 network20 故障解除後 246 ms：單程延遲約 13 ms 消失，Match 端領先從約 60 跳到 73 ms。是「殘留的領先＋外部觸發」的複合事件。
5. **網路延遲下降或釘住解除，也會把領先推高 16～24 ms**（network40 的故障解除、08b L2 gateway-250ms r2 的釘住解除，都在沒有 late 修正時讓合計到 90）。v7 的 backlog 餘裕 45 只容得下約 29 ms 的到達提前。
6. **5 筆在修正前 1 秒內都有 27～76 ms 的產生空檔**（這類間隔在 v7 只占 0.006～0.17％），所以它們也無法支持「只有傳輸型遲到才夾在 2 Tick」。M1／M2 的修正量也被輔助偵測器高估，實際約 32.0／30.6 ms，仍是 2 Tick 的上限。
7. **對 09a 的意涵**（仍是假說）：09a 會讓 W_post 在 4 個命令內歸零。若上面的模型成立，之後每次 tracking 型 late 修正都會越過 66.7 ms。現在的頭若 W_post 在 0～16.7 ms 近似均勻，越線機率約 20～25％，實測 1／5。W_post 的分布只有 5 個值，修正當下的 token 水位也沒有記錄在 trace 裡，這兩點都沒有驗證。
8. **檢定力**：(4,1) 的 Fisher 雙尾最小 p 是 0.2，任何效果都無法在 α＝0.05 下檢出。比例比較（第③步）每棵 tree 需要約 8 次（若真實是 0.25 對 1.0）到 18 次（0.25 對 0.75）late 修正；自然發生在閒置主機是 0，只能靠注入。用連續量（W_post、Match 端領先）做預測檢查，比 2×2 有效。
9. **事後的成分**（如實記錄）：5 筆在凍結前都已知；延伸觀察視窗是在已知 network20 的時間線之後設計的；MAIN 的規則（tracking 且 ≤−20 ms）寫在普查之後。凍結時的表不受這兩點影響。

**第②步若要做，必須量的東西**（核對的建議）：注入要能穩定產生 tracking 型 −33 ms 的 late 修正，並記錄注入有沒有造成產生空檔；每次修正記錄 W_pre（1 秒與 0.25 秒）、slew 期間每個命令的等待與 W_post、修正前後的 Match 端領先、每 Tick 的排隊與 30 Tick 合計的軌跡；修正後 2～3 秒內不能有排程中的故障解除或延遲改變，並標記右設限；事前宣告可被推翻的預測：「重設 ⇔ Match 端領先 >66.7 ms ⇔ W_post 小於（修正前領先 − 33.4），約 3.6～3.9 ms」。

## D48 第②步：試跑時碰到停止條件（2026-10-10）

工具與試跑 1 位（high）。證據：`build/target/_build/test/logs/pvp-v7-d48-step2-dev-20261010/`（12 次開發跑次全部保留，沒有重跑；主機沒有閒置閘門，只供開發）。新的 runner `build/acceptance/object_fps_pvp/run_short_stall.py` 與它的測試還沒有 commit；凍結檔沒有改動（S1 的 `analyzers.sha256` 試跑前後都吻合）。

**停止的理由：原本的注入（relay 擋住 client→Gateway 的封包 30～150 ms 再放出）在現在的頭，機制上就產生不了「成對遲到 → late 修正」。** 試跑 0／5 個跑次（每位玩家擋下 1～7 個命令，都落在同一次發布裡）。

- **機制**（主對話已核對程式）：
  1. Gateway 收到 Match 的 snapshot 就更新 `lastResolved`（`gateway/server.go:519-531`），之後封包裡 ≤`lastResolved` 的命令直接略過，不轉給 Match（`:420-422`）。所以 client→Gateway 路徑上晚到的命令，只有落在「Match 替代它」到「Gateway 處理那個 snapshot」之間的那一小段才會到 Match、產生負的 slack 樣本；晚得更多的會被 Gateway 吞掉，連樣本都沒有。
  2. 擋住再放出的封包只會落在一次發布裡，而 late 修正需要連續 2 次發布都帶負樣本（`MovementPhaseLateSamples`＝2，`Movement.hpp:58`、`LocalPlayerPrediction.cpp:134`）。即使改成「連續 4 個命令各晚 12～15 ms、分在 4 次發布」（原型 1），仍然 0 次，因為被 Gateway 略過。
  3. 第①步的 M1～M5 都帶有產生空檔，和這兩點不矛盾。推論（未驗證）：host-ipc 跑次的 relay 讓 Match→Gateway 的 snapshot 變慢，拉長了那一小段，這可能是 D44（M3）出現在 host-ipc 跑次的原因。
- **意涵**（未結，要寫進 D44）：client 端的網路遲到幾乎不會觸發 late 修正；真正的觸發來源是伺服器端的輸入延遲（Gateway→Match）或邊界巧合。
  - 加註（2026-10-10，D51 ⑨，使用者核准）：這段「意涵」是在截斷的資訊流上寫的，把 Gateway 過濾的效果當成了系統性質。Client 端的遲到觸發不了 late 修正，是因為晚到的命令在 Gateway 被吞掉（`gateway/server.go:421-423`），而不是遲到本身不重要；見[事故紀錄](Incident/2026-10-10-gateway-silent-drop.md) 3.2、P2。
  - 更正（2026-10-11，09R-eval）：修好資訊流之後，這句對 network 型損傷（遺失加 ≤30 ms 抖動）又成立了（第 08c 批 L2 的 network0／20／40 late 修正 0），但機制不同，不是 Gateway 的過濾：Match 每次發布只帶最小樣本（`MatchRuntimeHost.cpp:129-134`、`:466-479`），遺失造成的遲到由下一個倖存的包一次帶到，只產生 1 次負樣本的發布，達不到連續 2 次的規則（`LocalPlayerPrediction.cpp:128`、`:133-137`）。對持續延遲（x1、ramp）不成立：L2 的修好的樹有 late 修正。證據：`build/target/_build/test/logs/pvp-v7-09r-eval-20261011/`的 `p4_late.txt`、`report-from-agent.md` 第 2 節；L2 是 `build/target/_build/test/logs/pvp-v7-08c-l2-20261011/session/`。
- **新的注入可以重現**：在 Gateway→Match 的 IPC 輸入路徑加延遲（原型 2，δ 40～50 ms、視窗 100 ms），tracking 型、−33.3 ms 的 late 修正 8／8 事件；故障前後沒有 >20 ms 的產生空檔、沒有 runtime_gap；注入在 slew 結束前就結束。模型預覽：8 個單次修正的 Match 端領先，實測與預測（修正前＋33.33−W_post）相差 ≤0.3 ms，全部沒有重設（領先 58.6～64.3 ms）。
- **但脆弱窗沒有碰到**：W_post 6.5～12.0 ms，0／4 個跑次落在約 <3.6～3.9 ms。同一跑次兩位玩家的 W_post 相差 ≤0.2 ms，所以獨立單位是跑次。經驗擬合（不是推導）：W_post ≈ (26.8−16.667·x_before) mod 16.667 ms，x_before 約 1.39～1.60 時才會碰到脆弱窗；既有現頭跑次估計每個跑次的機率約 0.24。
- **第二條重設途徑**（1 個跑次，δ 80 ms、視窗 150 ms）：兩次 late 修正串接（缺陷 B），共 −66.7 ms，Match 端領先約 94 ms，兩位玩家都 backlog 重設。不管 W_post 多少都會重設，09a 管不到。
- **量測腳本**（草稿，未凍結）：`measure_draft.py`，定義寫在 docstring；用第①步的 M3 驗證：前／後／預測領先 36.89／69.71／69.72，與第①步一致。

## D48 第②步的結果（2026-10-10，**passed**）

- 證據：`build/target/_build/test/logs/pvp-v7-d48-step2-20261010/`（`declaration.md`／`declaration.sha256`、`artifacts.sha256`、`preflight.txt`、`build.log`、`session/`、`measure.jsonl`、`sha256.txt`）。session 頭 H＝`6a298f4`。
- 執行：preflight 全部通過（P1：Gateway `vcs.revision=H`、`vcs.modified=false`；P1'：建置資訊和試跑的 Gateway 只差 revision 與時間；P1''：`a954aa3..H` 在 `docs/` 以外只有 runner 與它的測試；P2：Match `c9cc6617…`、probe `d45a1f0b…`、arena `0026013c…` 與凍結工具都等於宣告值）。量測期間暫停 PR #73 的 auto-fix；閒置閘門通過；8 回全部完成；開跑前與結束時 `artifacts.sha256` 16／16 OK、`git status` 空。
- **判定（凍結的 `measure.py` 機械地給出）：`step2`＝passed。**
  - V1 注入乾淨：8／8。
  - V2 重現：8／8 跑次，16 個合格事件（每個跑次兩位玩家各一次單次、約 −33.3 ms 的 tracking 型 late 修正）。
  - V3 模型精度：16 個合格事件、8 個跑次，|model_diff| 最大 0.645 ms、中位 0.256 ms（容許 1.0 ms）。
  - V4：不越線那一側 12 個事件全部沒有重設（「一致」）；越線那一側沒有帶外的事件（「無法判定，交給第③步」）；帶內（66.667±1.0 ms）4 個事件，只列出、不檢定。
- 逐事件（ms；同一跑次的兩位玩家不獨立，獨立單位是跑次）：

  | 跑次 | W_post | 修正前領先 | 修正後領先（實測） | 預測 | 差 | 30 Tick 合計最大 | 重設 |
  |---|---|---|---|---|---|---|---|
  | 2.0-r1 | 14.0／14.0 | 37.1 | 56.7／56.6 | 56.4／56.4 | +0.23／+0.20 | 90 | 否 |
  | 11.5-r2 | 8.1／8.3 | 36.8 | 62.7／62.5 | 62.0／61.9 | +0.65／+0.62 | 90 | 否 |
  | 2.0-r3 | 7.8／7.9 | 37.1 | 62.9／62.8 | 62.6／62.6 | +0.29／+0.28 | 90 | 否 |
  | 11.5-r4 | **3.75／3.85** | 36.8 | **66.9／66.8** | 66.4／66.2 | +0.54／+0.57 | 105 | **是**（t_c 後 512 ms） |
  | 2.0-r5 | 7.5／7.4 | 37.3 | 63.3／63.3 | 63.2／63.3 | +0.11／+0.04 | 90 | 否 |
  | 11.5-r6 | 8.8／8.7 | 37.0 | 62.0／61.9 | 61.6／61.6 | +0.40／+0.34 | 90 | 否 |
  | 2.0-r7 | **3.12／3.37** | 37.2 | **67.4／67.4** | 67.4／67.2 | −0.06／+0.12 | 107 | **是**（t_c 後 513 ms） |
  | 11.5-r8 | 10.2／10.0 | 37.0 | 60.4／60.4 | 60.2／60.3 | +0.23／+0.13 | 90 | 否 |

- 讀法（只記錄，不改判定）：
  - **脆弱窗自然碰到了 2／8 個跑次**（r4、r7：W_post 3.1～3.9 ms），兩次都重設；W_post ≥7.4 ms 的 6 個跑次領先 ≤63.3 ms、合計 90、都沒有重設。這和模型的邊界一致，但 r4、r7 都落在 ±1.0 ms 的帶內，依宣告不檢定；越線那一側仍交給第③步。
  - **模型有系統性的正偏差**：16 個事件中 15 個實測比預測大（+0.04～+0.65 ms；宣告第 14 節第 4 點已記錄，部分是沒扣 W_pre）。在邊界附近這個偏差有影響：r4 的預測 66.4／66.2 ms 在門檻下，實測 66.9／66.8 ms 在門檻上，而且重設了。所以「用預測代替實測」（link B）在邊界附近不可靠，第③步要用實測的領先。
  - 30 Tick 合計在 r4 剛好到 105、r7 到 107，重設都在 t_c 後約 0.51 秒，和開發跑次 z1（530 ms）、第①步 M3（488 ms）一致。
- **意涵**：這個注入可以當第③步的工具（V1、V2 成立）；第①步的模型在不越線那一側精度 ≤0.65 ms（V3）。現在的頭在 8 個跑次中有 2 個走進脆弱窗並重設：也就是說，現在的頭上「被釘住吸收額外領先」並不可靠，大約四分之一的 tracking 型 late 修正仍會走到 backlog 重設（樣本小，只供規劃）。

## D48 第②步的事前宣告（2026-10-10 使用者核准：「宣告核准，CTest 不登記，開始跑」）

下面是證據目錄 `build/target/_build/test/logs/pvp-v7-d48-step2-dev-20261010/declaration-draft.md`（SHA-256 `34df9e43…`）的全文。核准時複製到 session 的證據目錄，雜湊寫進 `declaration.sha256`。準備：runner 依 D50 (b1) 定稿（測試 12 個通過）、量測腳本 `measure.py` 凍結候選（`045714f1…`）；D45 的 xhigh 對抗式檢查判定「修正後可用」，修正已套用，主對話再依檢查原文逐段核對替換文字。

### D48 第②步 宣告（草稿）：Gateway→Match IPC 輸入延遲的注入與模型驗證

- 狀態：**草稿，未執行**。D45 的對抗式檢查（xhigh，專門核對每個門檻的來源）已完成，判定「修正後可用」；本版已套用它的修正（證據在 `adversarial/`）。下一步送使用者核准。主對話依對抗式檢查的原文逐段比對過替換文字，補正了 P1'／P1''、fault_at 的射擊延遲、V2 的措辭、`relay_error` 的註、停止條件、第 12 節三列與 D50② 的措辭。核准前不跑 session。
- 依據的頭：`claude/pvp-v7-p2` 的 `cdd3a59`。產品程式碼＝`a954aa3`（`git diff --name-only a954aa3 cdd3a59` 在 `docs/` 以外是空的）。
- 依據的決定：D48（第②步）、D50（注入改為 Gateway→Match 的 IPC 延遲、實作 b1、四個條件）、D48 第②步的決定 3＝(iii)。
- 本草稿的工具、試跑與數字都在 `build/target/_build/test/logs/pvp-v7-d48-step2-dev-20261010/`（`commands.txt`、`sha256.txt`）。

#### 1. 目的（決定 3＝(iii)）

第②步只驗證三件事，**不**驗證脆弱窗那一側：

1. **注入乾淨**（V1）：注入不造成產生空檔、runtime_gap 或重新播種，而且注入在 slew 結束之前就結束。
2. **穩定重現**（V2）：每次注入都產生 tracking 型、約 −33.3 ms（2 Tick）的單次 late 修正。
3. **模型在不越線那一側的精度**（V3、V4）：Match 端領先 after ≈ 修正前的領先＋33.33−W_post；每個合格事件也照「重設 ⇔ 領先 >66.7 ms」檢驗。偶然越線的事件同樣照預測檢驗，但**不要求**一定碰到越線。

越線那一側（W_post 落在脆弱窗、領先 >66.7 ms）交給第③步：09a 原型在設計上會讓 W_post≈0。本步不檢定 09a／B2，不判斷 D44 是否提前，不做三棵 tree 的對照。

#### 2. 產物與建置

- 產品程式碼 `a954aa3`。session 開始前在 `docs/` 以外只能多出 runner 與它的測試兩個檔（見下；若核准時決定把測試登記進 CTest，`tests/object_fps_pvp/CMakeLists.txt` 的那幾行也算，Match 與 probe 的雜湊仍必須不變）。
- **建置方式（選定）：先 commit，再在主 checkout 原地建置。**
  1. 核准後，把 `build/acceptance/object_fps_pvp/run_short_stall.py` 與 `test_run_short_stall.py` 和本宣告相關的文件更新一起 commit 到 `claude/pvp-v7-p2`（是否推送由使用者決定，與 session 無關）。這個 commit 記為 session 頭 H。
  2. **從 commit H 到 session 結束，主 checkout 不能有任何變動**：HEAD＝H，`git status --porcelain` 必須是空的（追蹤檔沒有變更，也沒有未被 ignore 的 untracked 檔）。HANDOFF 的「現況」更新併進 H，或等 session 結束後再寫。理由：Go 在 repository 根目錄跑 `git status --porcelain` 來決定 `vcs.modified`（Go 1.27.1 `src/cmd/go/internal/vcs/vcs.go:231`），任何位置的 untracked、未 ignore 的檔都會讓它變成 true。建置前、開跑前、結束時各核對一次；runner 每回之前另核對 HEAD 與追蹤檔（`--untracked-files=no`）。
  3. `cmake --build build/target/_build/test --target gyo_object_fps_pvp_action_probe gyo_object_fps_pvp-match gyo_object_fps_pvp-gateway`，log 寫進證據目錄。
  4. **建置後的兩個 preflight**（任一不成立就停下：不寫 `artifacts.sha256`，不開跑）：
     - P1：`go version -m` 的 Gateway 顯示 `vcs.revision=H`、`vcs.modified=false`。
     - P1'：新 Gateway 的 `go version -m` 與試跑時的 Gateway（`ef5b9779…`）比較，除了 `vcs.revision`、`vcs.time` 以外全部相同。
     - P1''：`git diff --name-only a954aa3 H` 在 `docs/` 以外只有 runner 與它的測試（登記 CTest 時再加 `tests/object_fps_pvp/CMakeLists.txt`）。
     - P2：Match、probe、arena 的雜湊和試跑相同（Match `c9cc6617…`＝第 08b 批 L2 的 match-after，probe `d45a1f0b…`，arena `0026013c…`；預期 probe 與 Match 不重新編譯），凍結工具的雜湊等於下面列出的值。
  5. P1、P1'、P1''、P2 都成立後才寫 `artifacts.sha256`（Gateway 的雜湊要在 H 上建置完才知道），開跑前與結束時用 `shasum -a 256 -c` 核對。
- **不選 detached worktree 的理由**：從零建置會換掉路徑，Match 與 probe 不再和試跑、第 08b 批 L2 逐位元組相同（S1 為此需要另做路徑替換的核對並請使用者核准）。原地建置讓試跑與本 session 只差 Gateway 的 VCS 戳記；試跑時 Gateway 是在 `fe25ba6`（`vcs.modified=false`）建置的，`fe25ba6..cdd3a59` 只改文件。
- **風險**：主 checkout 的建置目錄是共用的。核准到 session 之間若有其他建置，雜湊就會不同；由 P2、開跑前的 `shasum -a 256 -c artifacts.sha256`、每個跑次的 `artifacts.json` 對 `plan.json`（量測腳本的停止條件）攔下。
- **`artifacts.sha256`（建置與 P1、P2 之後寫入，開跑前與結束時核對）**：Match、Gateway、probe、arena；`run_short_stall.py`、`action_probe.py`（`59002b43…`）、`backpressure_probe.py`（`c88ceb89…`）、`gameplay_evidence.py`（`01a45228…`）、`command_evidence.py`（`5fa9779b…`）、`acceptance_util.py`（`227d021f…`）、`run_network.py`（`494b0987…`）、`impaired_network.py`（`abe49647…`）；`sleeper.py`（C1 版 `98105dbf…`）；`measure.py`；第①步的 `detector.py`（`445271ce…`）；本宣告。

#### 3. 工具

- **runner**：`build/acceptance/object_fps_pvp/run_short_stall.py`（sha256 `b5a6d33c…`；測試 `test_run_short_stall.py` `835cdbd3…`，12 個測試通過，含對抗式檢查補上的測試：window_ms 與 release 的核對、只改追蹤檔也會停、所有延遲片段送出後才 release、視窗結束後的位元組立即轉送；對抗式檢查的 7 個突變全部被殺掉）。依 D50 (b1)：
  - 在自己的行程裡把 `action_probe.IpcPause` 換成子類別 `IpcDelay`，每回結束就還原；呼叫不變的 `run_case(options, 'host-ipc', window_ms)`。凍結檔不改。
  - 開頭核對實際匯入的 `action_probe.py` 與 `backpressure_probe.py` 的 sha256，不符就拒絕執行，什麼都不寫（D50 條件③）。
  - 每回結束後讀該跑次 `result.json` 的 fault，必須有 `mechanism=ipc-input-delay`、`delay_ms`、`window_ms`（等於宣告值）、`delayed_chunks ≥1`、`start_ns`、`release_ns > start_ns`，證明替換真的生效；缺了就記為 `injection_unproven` 並停下（D50 條件③）。
  - `run_case` 的判定只記錄（`run_case.recorded_only`），判定來自凍結的量測腳本（D50 條件④）。
  - 輸出目錄必須是新的或空的；寫 `plan.json`（參數、排程、工具與產物雜湊、sleeper、repository 狀態）、每回一行的 `results.jsonl`、`idle_gate.jsonl`、`summary.json`（含結束時的 `repository_end`）。失敗的跑次保留，不重跑。
  - 停下（其餘回記為 `not_run`）：runner 錯誤（`run_case` 例外，沒有 artifacts）、`injection_unproven`、HEAD 或追蹤檔改變（每回之前核對）、sleeper 失敗、閒置閘門 30 分鐘不成立、SIGINT／SIGTERM。
  - **中斷的注意事項**：SIGINT／SIGTERM 只有送到 runner 自己的 PID（例如 `kill -TERM <pid>`）時才會做完當回再停；在終端機按 Ctrl-C 時，SIGINT 會送到整個前景行程群組，Match、Gateway、probe 也會收到，當回會變成 runner 錯誤。不論哪一種，中斷都照停止條件處理（`summary.json` 的 `interrupted`），保留證據、不重跑。
- **量測腳本**：`measure.py`（sha256 `045714f12dd5020d88b4d34e0df07d85a6e6607d17651f76ee08263fc6fadf14`；以對抗式檢查的修正版為底，另在 docstring 補上新欄位的說明）。完整定義寫在它的 docstring；核准時以這個 sha256 凍結，複製到證據目錄。判定寫在最後一行 `judgement`，其中 `step2` 欄位是第②步的結論（第 8 節）。
  - 開頭核對第①步偵測器的 sha256（`445271ce…`），不符就拒絕（結束碼 2）；偵測器以唯讀方式載入，不寫 bytecode。
  - 判定模式讀 runner 的 `plan.json`，參數必須等於宣告值（δ 50 ms、視窗 100 ms、fault_at［2.0, 11.5］、8 回、60 FPS），否則拒絕。
- **sleeper**：C1 的 `sleeper.py`（`98105dbf…`，與 S1 相同），複製到證據目錄。

#### 4. 主機

- 閒置：session 期間不做開發；runner 以 `caffeinate -dims` 執行；前景沒有其他使用者程式（照 C1 第 2 次：關掉 Chrome、ChatGPT／Codex）；**暫停 PR #73 的 auto-fix**；背景的其他分析也先停下，跑完再恢復。
- 閒置閘門（C1 第 2 次 session 的附註，與 S1 相同；runner 的 `--idle-gate`）：第 1 回之前連續 3 次 5 秒 sleeper，每次最大值都 <10 ms。不成立就每 30 秒重試，最多 30 分鐘，仍不成立就停下。
- 每回前後各一次 5 秒 sleeper，並記錄 CPU 最高的 5 個程序（runner 的 `--sleeper`）。
- probe 的 TimerBaseline（`result.json` 的 `timer_baseline`）只記錄，不分層、不判定。

#### 5. 注入

- 機制 `ipc-input-delay`：δ＝50 ms，視窗＝100 ms；fault_at 交錯 2.0／11.5 秒（第 k 回：k 奇數 2.0、偶數 11.5），每個跑次只注入一次；v5 gameplay probe、60 FPS。
- **D50 條件①：延遲的是 Gateway→Match 方向的全部流量。** 移動輸入、動作、ACK、控制都在同一條 IPC 串流上，一起延遲，不只是輸入。視窗內 relay 讀到的每個位元組在讀到後 50 ms 才送出；位元組順序不變，所以視窗之後讀到的位元組排在被延遲的後面（TCP 串流不能超車），整條串流實際上要到約「視窗＋δ」才恢復（試跑 `release_ns − start_ns`＝140～147 ms）。**Match→Gateway（snapshot、結果）永遠不延遲、不暫停**（`IpcDelay.arm` 不設定基底類別的 `duration`，`backpressure_probe.py:79-106` 的下行迴圈照常轉送）。
- **D50 條件②：Python relay 整場都在 Gateway 與 Match 之間。** `run_case` 的 host-ipc 模式在 Gateway 啟動之前就建立 relay，Gateway 連到 relay 的埠（`action_probe.py:531-533`），每個跑次從開始到結束都經過它。這**不是正式的部署拓撲**；視窗之外，每個 Gateway→Match 的位元組也經過 relay 的讀取執行緒與送出執行緒的交接，Match→Gateway 也整場經過 relay 的逐框讀取（標頭與內容分兩次讀，並檢查協議版本）。第③步必須讓三棵 tree 都用同一份 runner（同一個 sha256）、經過同一個 relay，三者才彼此可比（D50②）；這要寫進第③步的宣告。數字不能當成正式拓撲的值。
- 選 δ 50／視窗 100 的理由（試跑）：δ 40～50 ms、視窗 100 ms 的 5 個跑次、10 個事件全部是單次、約 −33.3 ms 的 tracking 型 late 修正；δ 80／視窗 150 的 1 個跑次產生兩次串接的 late 修正（缺陷 B，共 −66.7 ms，領先約 94 ms，兩位玩家都 backlog 重設）。串接途徑不管 W_post 都會重設，09a 管不到；本步不跑它，只作為第③步的背景。
- 選 fault_at 2.0／11.5 的理由與限制：
  - gameplay 計畫是確定的（`gameplay-plan.json` `65bad3b1…`）：玩家 2 在約 9.77 秒 life_respawn（試跑全部相同），之後 60 Tick（約到 10.77 秒）不能再重設（`Movement.hpp:40`）。
  - 2.0 秒：事件視窗［t_c−1 秒, t_c＋3 秒］約為［1.1, 5.1］秒，離 respawn 很遠。
  - 11.5 秒：修正前的視窗從約 10.6 秒開始，和重設冷卻的尾巴重疊。冷卻只管能不能重設，而結果視窗［t_c, t_c＋3 秒］從約 11.6 秒開始，在冷卻之後；life 設限看的［t_c−1 秒, 結果時刻）也不含 9.77 秒的 respawn。玩家 2 這時是新的 epoch，修正前的領先由新 epoch 的命令算出。
  - late 修正之後約 4.1 秒會有下一次視窗修正，若是正向，產生間隔是 22.2 ms（>20 ms）：試跑中在約 6.2 秒與約 15.7 秒，都比 V1 與事件視窗的結束晚約 1.1 秒；trace 在約 16.0 秒結束（t_c＋3 秒約 14.6 秒）。
  - 兩個時刻的注入視窗內都有動作的裁決（試跑約 2.02 秒、約 11.58 秒），所以條件①的「動作也一起延遲」實際會發生：2.0 秒那一個裁決的 Tick 在 δ 50 時是 305，在 δ 40／80 時變成 308／310。玩家 1 的計畫射擊剛好在 2.0 與 11.5 秒，這一發是否落進視窗取決於競爭：y1／y3／z1 沒有被延遲，y2 被延遲約 70 ms（解析 Tick 間隔 18／11，權威冷卻 10 Tick）。`run_case` 的判定因此可能受影響，只記錄。
  - 兩個時刻的自然 token 水位分布都量過（`natural_x.txt`）。

#### 6. 跑次

- **8 次**（4 次 2.0 秒、4 次 11.5 秒），約 16 個玩家事件。不隨機化，不補跑。
- 理由：
  - 重現率：8 次中至少 7 次「重現」（V2）。不乾淨的跑次算不重現；越線而重設的跑次仍算重現。V2 容許的是 1 次不伴隨 V1 問題的未重現（修正量、串接）；伴隨 >20 ms 產生空檔、seed 或 runtime_gap 的主機雜訊由 V1（8／8 乾淨）判為不成立。真實的每跑次重現率若是 0.95，P(≥7/8)＝0.94；0.80 時 0.50；0.70 時 0.26。
  - 模型精度：約 16 個事件、至少 12 個合格，而且至少來自 6 個跑次（V3）。同一跑次的兩位玩家 W_post 相差 ≤0.6 ms，**獨立單位是跑次**。
  - 自然越線（只記錄）：`natural_x.txt` 的 10／42（兩個時刻各一次）混合了 clean-30（30 FPS）、250 ms 故障案例與 S1 的 p2 建置；條件和本 session 最接近的 clean-60（60 FPS）是兩個時刻各 2／6。所以每跑次約 0.24～0.33，8 次至少碰到一次約 0.89～0.96。樣本小，W_hat 的擬合也沒有驗證，只記錄、不要求碰到。開發跑次 δ 40～50 的 5 個跑次碰到 1 次（z1，見第 13 節）。
  - 再多跑也不會改變第②步的結論：越線那一側本來就交給第③步。
- 時間：每回約 30 秒（`run_case` 約 20 秒＋兩次 sleeper），8 回約 4～5 分鐘，另加閒置閘門。

#### 7. 定義（摘要；完整定義與常數在 `measure.py` 的 docstring）

- **事件**：第①步凍結偵測器的 P 事件（`action-frames.jsonl` 的 `phase_late_corrections` 增加），tracking 型（前一幀的 `phase_state` 不是 Acquiring），配對到偵測器的負向產生相位步（`first_seq`～`last_seq`＝被 slew 的命令，`size_ms`＝淨位移）。
  - `in_span`：幀時刻落在［注入開始, 開始＋1 秒］，而且不是重設之後的事件。
  - `post_reset`：該玩家在事件前 3 秒內有非 respawn 的重設（例如重設後的重新擷取）；只列出。
  - `chained`：increment >1，或同一玩家 0.5 秒內有另一個（非 post_reset 的）P 事件（缺陷 B 的途徑）。
  - `qualifies`：tracking、increment＝1、不是 chained、|size−(−33.33)| ≤1.0 ms。
- **等待**：w(s)＝第一次 `sent`（在 `generated` 之後）−`generated`，真實（非 seed）命令。W_pre 是修正前 1 秒與 0.25 秒的中位數；slew 期間逐命令列出（`first_seq−2`～`last_seq+2`）。
- **W_post**：t_post＝`last_seq` 的產生時刻；W_post＝［t_post, t_post＋0.25 秒）內命令等待的中位數（至少 8 個）。
- **Match 端領先**：lead(s)＝Match `resolved`（source actual）−`host_accepted`。lead_before＝［t_c−1 秒, t_c）內產生的命令的中位數；lead_after＝［t_post, t_post＋0.25 秒）內產生、而且算得出領先的命令的中位數，**至少 8 個**（`n_lead_after`；不足時為 null，事件記為「lead_after／lead_pred 無法計算」而不合格），另列 P10／P90 與 1 秒版。lead_pred＝lead_before＋33.333−W_post；model_diff＝lead_after−lead_pred。
- **排隊**：照 `PvpMatch.cpp:427-451` 重播每 Tick 的 `queued` 與 30 Tick 合計（換 epoch 或重設時清空），列出軌跡、最大值、第一次 ≥105 的 Tick。
- **結果**：reset＝［t_c, t_c＋3 秒］內該玩家的 backlog 重設。結果在第一個非 respawn 重設時就決定，所以下面的設限只看［t_c−1 秒, 結果時刻）：重設本身造成的重新播種與重新擷取不會把它設限掉（這一點是開發跑次 z1 找到的，見第 13 節）。
- **設限**（任一成立就不合格，列出）：trace 提早結束（沒有重設時）、注入邊界落在 t_post 之後、產生間隔 >20 ms、Client runtime_gap（dropped >1 ms 或 frame ≥50 ms）、任何 Match runtime_gap、死亡或 respawn、**結果時刻的那個重設（第一個非 respawn 重設）是 starvation／sequence_exhausted**、另一次 phase 修正。
- **合格事件**（V3、V4 的對象）：乾淨跑次中 `in_span` 且 `qualifies`、沒有設限、lead_before／lead_after／W_post 都算得出來的事件。
- **token 水位**：從 `sent` 封包重建（`ClientConnection.cpp:595-605` 的規則），**標記為重建值**（欄位名以 `reconstructed_` 開頭），只記錄。

#### 8. 判定

- **V1 注入乾淨**（每個跑次）。乾淨＝以下都成立：
  - delay 欄位齊全且等於宣告值；`relay_error` 為 null；frames 有相位計數器；
  - 每位玩家在［注入開始−1 秒, end_p）內：沒有 >20 ms 的產生間隔、沒有 `seeded_neutral` 的產生、沒有 Client runtime_gap（同上的門檻）；end_p＝該玩家在［開始, release＋3 秒］內第一個非 respawn 重設，沒有就是 release＋3 秒；
  - ［開始−1 秒, 最晚的 end_p）內沒有 Match runtime_gap；
  - 每個 `in_span` 事件的 `release_ns < t_post`（注入在 slew 結束前就結束）。
  - **V1 成立 ⇔ 8 個跑次都完成而且都乾淨。**
  - 註：這裡的 `relay_error` 是 `result.json` fault 中 IPC relay（IpcDelay）的 socket 錯誤，只記到 release 為止（`run_case` 在 release 時取 evidence）；之後的 relay 錯誤會讓 probe 失敗，歸到 runner 錯誤（E）。UDP relay（ActionRelay）的錯誤在 `relay.json`，只進 `run_case` 的判定（只記錄）。
- **V2 重現**：跑次「重現」＝乾淨，而且兩位玩家各有剛好一個 `in_span` 的 P 事件，且它 `qualifies`。**V2 成立 ⇔ 重現的跑次 ≥7／8。**
- **V3 模型精度**（合格事件）：|model_diff| ≤1.0 ms。
  - 成立 ⇔ 合格事件 ≥12、來自 ≥6 個跑次，而且全部在容許內。
  - 不成立 ⇔ 任一合格事件超出 1.0 ms。
  - 無法判定 ⇔ 合格事件 <12 或跑次 <6，而且沒有超出的。
- **V4 邊界**（合格事件）：門檻 66.667 ms（4 Tick），帶寬 ±1.0 ms。兩側分開判定：
  - 帶內（|lead_after−66.667| ≤1.0）：只列出、不檢定。
  - 不越線那一側（lead_after <65.667）：有合格事件重設了就是「推翻」；否則至少有一個這一側的合格事件就是「一致」，一個都沒有就是「無法判定」。
  - 越線那一側（lead_after >67.667）：有合格事件沒有 backlog 重設就是「推翻」；否則有重設的合格事件就是「一致」，沒有就是「無法判定（交給第③步）」。
  - V4 整體：任一側推翻就是「推翻」，否則「一致」。
- **「推翻」「不成立」「無法判定」的意義**：
  - 推翻（V4）：出現與預測相反的決定性觀測。模型或邊界錯了，D44 的推論鏈要重看；停在第②步回報，不進第③步。
  - 不成立（V1、V2、V3）：注入不符合前提（V1、V2），或模型精度不如宣告（V3）。注入不能照設計當第③步的工具，或第③步不能用預測值代替實測；回報，由使用者決定。
  - 無法判定：越線側沒有碰到、合格事件或跑次不足、帶內事件、被設限的事件。不視為支持，也不視為反證；不補跑。
- **結論欄位 `step2`**（`judgement` 行，依序）：任一停止條件 →「stopped」；否則 V4 推翻 →「refuted (V4)」；否則 V1、V2、V3 成立**而且不越線那一側「一致」**→「passed」；其餘 →「not passed」。
- **第②步通過 ⇔ `step2`＝passed**（越線那一側「一致」或「無法判定」都可以）。通過代表這個注入可以當第③步的工具。
- 判定由凍結的 `measure.py` 的 `judgement` 行機械地給出；`run_case` 的判定不參與。

#### 9. 只記錄

- `run_case` 的判定與錯誤（凍結分析器的 host-ipc 規則）；TimerBaseline；每回前後的 sleeper 與 CPU 最高的程序。
- W_pre（1 秒、0.25 秒）、slew 期間逐命令的等待、W_post_first、lead_after 的 P10／P90、`n_lead_after` 與 1 秒版。
- 排隊軌跡、30 Tick 合計的最大值、第一次 ≥105 的 Tick、重設發生在 t_c 之後幾 ms。
- link B：「重設 ⇔ lead_pred >66.667」（以預測代替實測的版本）。
- 範圍之外的自然 late 修正、post_reset 事件、chained 事件、Acquiring 型事件、被設限的事件與原因；被設限的越線事件另列（`censored_crossing`）。
- token 水位重建：x（slew 前一個命令、最後一個 slew 命令）、重建的漂移，以及經驗擬合 W_hat＝(26.8−16.667·x) mod 16.667 對 W_post 的差（擬合是試跑得出的，不是推導）。
- 注入本身：`delayed_chunks`、`delayed_bytes`、`last_delayed_sent_ns`、`maximum_send_lateness_ms`（送出比預定晚多少；z1 為 2.15 ms）。
- 越線事件（若有）的完整時間線：這是第③步規劃的輸入。

#### 10. 停止條件（立即停下，保留全部證據，不重跑，交使用者決定）

1. 範圍需要擴大：要改權威（authority digest）、wire、FireGate 常數、late 修正上限、target、lead、backlog 門檻，或要改凍結檔，或要把注入升級成凍結工具的正式模式（D50 的 (c)）。
2. runner 錯誤（E）：`run_case` 例外（沒有 artifacts）、`injection_unproven`、sleeper 失敗、閒置閘門 30 分鐘不成立。
3. 分析錯誤（A）：`measure.py` 對任一跑次丟出例外，或 frames 沒有相位計數器。凍結分析器在結構上拒絕 trace 時（`command_evidence.read_trace_events` 丟出 ValueError，`gameplay_evidence.analyze` 不攔），`run_case` 會在內部捕捉、回傳沒有 artifacts 的結果，runner 記為 `runner_error`，歸到 E。
4. 雜湊不符：runner 開頭的凍結檔核對（拒絕執行）；開跑前與結束時的 `artifacts.sha256`；`measure.py` 的偵測器核對；任一跑次的 `artifacts.json` 與 `plan.json` 不同；`plan.json` 的凍結檔雜湊與凍結值不同。
5. 主 checkout 改變：runner 每回之前核對的 HEAD 或追蹤檔改變；`summary.json` 的 `repository_end` 與 `plan.json` 的 `repository` 不同（`measure.py` 的停止）；開跑前或結束時 `git status --porcelain` 不是空的（含未 ignore 的 untracked 檔，第 2 節）。
6. 建置後的 preflight（P1、P1'、P1''、P2，第 2 節）不成立；或 session 被中斷（SIGINT／SIGTERM，`summary.json` 的 `interrupted`，第 3 節）；或 runner 異常結束、沒有 `summary.json`（`measure.py` 拒絕，結束碼 2）。
7. 任一跑次出現權威 Cooldown 拒絕（`actions.jsonl` 中 rejection＝3，`Combat.hpp:51-53`；P2 的停止條件，與 S1 相同）。回報時標明那一發是否在注入視窗內被延遲。
- 失敗或被設限的跑次照樣保留並列入彙總；不補跑。

#### 11. 證據

`build/target/_build/test/logs/pvp-v7-d48-step2-<YYYYMMDD>/`（session 當天的日期）：

- `declaration.md`（本宣告的核准版）與 `declaration.sha256`；`artifacts.sha256`；`measure.py`、`sleeper.py`（複本）；`build.log`；`commands.txt`；`preflight.txt`（建置前的 `git status --porcelain`、P1、P2、開跑前的 `shasum -c` 與 `git status --porcelain`）。
- `session/`：runner 的輸出（`plan.json`、`idle_gate.jsonl`、`results.jsonl`、`summary.json`、8 個跑次目錄）；`run.stdout.txt`。
- `measure.jsonl`：`python3 -I measure.py <第①步 detector.py> session` 的輸出（每個跑次、每個事件、最後一行 judgement，含 `step2`）。
- 結束時的 `sha256.txt`（全部檔案）。

預定的命令（核准並 commit H 之後；`E`＝證據目錄，`L`＝`build/target/_build/test/logs`）：

```
git status --porcelain                                    # 必須是空的
/Users/karasu/Code/Runtime/Builder/cmake-4.4.3/CMake.app/Contents/bin/cmake --build build/target/_build/test \
  --target gyo_object_fps_pvp_action_probe gyo_object_fps_pvp-match gyo_object_fps_pvp-gateway > $E/build.log
/usr/local/go/bin/go version -m build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway   # P1
shasum -a 256 <Match> <probe> <arena> <凍結工具>                                                        # P2
#（P1、P2 成立後寫 $E/artifacts.sha256）
shasum -a 256 -c $E/artifacts.sha256 && git status --porcelain
caffeinate -dims python3 build/acceptance/object_fps_pvp/run_short_stall.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_action_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output $E/session --delay-ms 50 --window-ms 100 --fault-at 2.0 --fault-at 11.5 --rounds 8 \
  --sleeper $E/sleeper.py --idle-gate > $E/run.stdout.txt
shasum -a 256 -c $E/artifacts.sha256 && git status --porcelain
python3 -I $E/measure.py $L/pvp-v7-d48-step1-20261010/detector.py $E/session > $E/measure.jsonl
```

#### 12. 門檻的來源（D45）

建置欄的「a954aa3」指：Match `c9cc6617…`、Gateway `ef5b9779…`（`fe25ba6`，`vcs.modified=false`）、probe `d45a1f0b…`、arena `0026013c…`。「試跑」＝`pvp-v7-d48-step2-dev-20261010/` 的 y1（2 次）、y2、y3、y4 與 z1；主機沒有閒置閘門（z1 的 sleeper 最大 4.07／4.24 ms）。

| 門檻 | 值 | 來源 | 建置 | 指標 | 流量 |
|---|---|---|---|---|---|
| δ／視窗 | 50／100 ms | 試跑：δ 40～50、視窗 100 的 5 個跑次、10 個事件都是單次 −33.3 ms；δ 80／視窗 150 串接（缺陷 B） | a954aa3 | `action-frames.jsonl` 的 `phase_late_corrections`＋偵測器的產生相位步 | v5 gameplay probe 60 FPS；relay 整場在 Gateway 與 Match 之間；注入只動 Gateway→Match |
| fault_at | 2.0／11.5 秒 | 玩家 2 的 life_respawn 約 9.77 秒（試跑全部相同，`gameplay-plan.json` `65bad3b1…`）；重設冷卻 60 Tick（`Movement.hpp:40`），11.5 秒時只和修正前視窗重疊（第 5 節）；下一次視窗修正約在 late 修正後 4.1 秒；trace 約 16.0 秒結束 | a954aa3 | Match `reset`（life_respawn）、最後一筆 `resolved`、Client `generated` 間隔 | 同上 |
| 閒置閘門 | 3 次 sleeper 最大 <10 ms，30 秒重試、30 分鐘上限 | C1 第 2 次 session（使用者核准的附註），S1 沿用 | 獨立的 sleeper 程序 | sleeper 的 1/60 秒絕對期限晚醒 | 無 session 流量 |
| 產生空檔 | >20 ms | Client 的步長只有 16.67／13.33（T/1.25）／22.22 ms（T/0.75）；>20 ms 是主機停頓或正向 slew。D48 第①步的 0.006～0.17％ 是 27～76 ms 空檔的比例（5 筆自然事件修正前都有），不是這個門檻的頻率。閒置主機的基準：第 08b 批 L2 的 clean-60（12 個跑次，before／after 兩種 Match）在 2.0／11.5 秒的兩個 V1 視窗內，0／24 有任何 V1 問題（`adversarial/analysis/gaprate.txt`）。注意正向 slew 的產生間隔是 22.2 ms，也會超過門檻：試跑中出現在開始後約 0.17 秒（首次決定）、respawn 後的首次決定（約 9.9 秒）、late 修正後約 4.1 秒的視窗修正（約 6.2 秒、約 15.7 秒；y1 玩家 1 再過 4 秒於 10.3 秒），都在 V1 與事件視窗之外（最近的差約 1.1 秒）；y2 在 5.7 秒有 30～38 ms 的主機停頓（非閒置主機，在視窗外） | a954aa3 的 Client（模擬角色在步邊界產生） | 每位玩家真實命令 `generated` 的相鄰間隔（穩態 16.7 ms；負向 slew 13.3 ms；正向 slew 22.2 ms） | 每位玩家，不分 epoch |
| Client runtime_gap | dropped >1 ms 或 frame ≥50 ms | 試跑與 z1 共 13 個跑次的 21 筆 Client runtime_gap 都是 epoch 開始的 seed clamp（sequence 3，dropped ≤0.093 ms，frame ≤16.8 ms；初稿寫的 24 筆把 synthetic-session 的 symlink 重複算了）；模擬角色在 frame ≥100 ms、dropped >0、frame 被夾（frame > elapsed）或擋住步時才寫（`LocalPlayerPrediction.cpp:332-338`）；分析器 v7 對 <100 ms 的 seed clamp 也豁免 | a954aa3 | Client `runtime_gap` | 每位玩家 |
| Match runtime_gap | 任何一筆 | Match 只在 elapsed ≥100 ms 或 dropped >0 時寫（`MatchRuntimeHost.cpp:314-316`）；試跑與 z1 的 13 個跑次 0 筆 | a954aa3 | Match `runtime_gap` | Match 全體 |
| 修正量 | −33.333±1.0 ms | 上限 2 Tick（`LocalPlayerPrediction.cpp:97-98`、`Movement.hpp:59`）；試跑 10 個事件 −33.29～−33.39 ms | a954aa3 | 偵測器的產生相位步淨位移（前後各 9 個命令的中位數） | 每位玩家、事件的 epoch |
| V2 重現 | ≥7／8 跑次 | 試跑 5／5 跑次（10／10 事件；δ 50／視窗 100 為 4／4 跑次、8／8 事件）。5／5 的單側 95％ 下界只有 0.55，所以 7／8 不是由試跑推出的，是宣告的要求：第③步需要可預期的 late 修正產量，只容許 1 次不伴隨 V1 問題的未重現 | a954aa3 | P 事件（同上） | 同上 |
| V3 容許 | model_diff 的絕對值 ≤1.0 ms | 試跑 10 個事件最大 0.312 ms（全部為正，+0.11～+0.31，中位約 +0.2），約 3 倍餘裕。第①步 5 筆自然事件的差是 0.01～2.94 ms（M3 0.01，其餘 1.0～2.9），但 5 筆都有產生空檔（本定義下全部被 gen_gap 設限），不用來定容許 | a954aa3 | Match `resolved`−`host_accepted` 的中位數（after：t_post 之後 0.25 秒產生的命令，至少 8 個）對 lead_before＋33.333−W_post | 只算 actual 解析的移動命令；不含動作 |
| V3 最少事件與跑次 | 12 個事件，來自 ≥6 個跑次 | 8 跑次×2 位玩家＝16，容許 4 個不合格；同一跑次的兩位玩家不獨立（W_post 相差 ≤0.6 ms），所以另要求跑次數 | — | — | — |
| V4 門檻 | 66.667 ms（4 Tick） | 每 Tick 的 `queued`≈floor(領先÷16.667 ms)：領先在 3～4 Tick 時排隊 3 個（30 Tick 合計 90）、超過 4 Tick 時 4 個（120）（09b 第①步第 3 點）；backlog 門檻 30 Tick 合計 ≥105（`Movement.hpp:38-39`、`PvpMatch.cpp:427-451`）。第①步 5 筆單調一致；試跑：64.3 ms 合計 90 不重設，z1 68.9／69.5 ms 合計 108 重設 | a954aa3 | Match `resolved` 的 `queued` 重播與 `reset` | 每位玩家 |
| V4 帶寬 | ±1.0 ms | 試跑 after 視窗內逐命令領先的 P10～P90 寬度 ≤0.63 ms（半寬 ≤0.31）；模型誤差 ≤0.31；第①步 M4（66.1 ms，右設限）會落在帶內 | a954aa3 | 同 V3 | 同 V3 |
| 視窗長度 | 修正前 1 秒；after 0.25 秒；結果 3 秒；串接 0.5 秒；範圍 1 秒 | 修正前 1 秒＝第①步凍結的「被釘住」定義；結果 3 秒：單次修正的越線約在 t_c 後 0.5 秒重設（z1 530 ms、M3 488 ms）；上界是下一次視窗決定（約 4.1 秒，它的 22.2 ms 正向 slew 會被當成產生空檔），3 秒在它之前約 1 秒；串接：缺陷 B 的第二次修正在第一次之後 133 ms（y4）；範圍：試跑的修正都在注入開始後 96～136 ms。after 0.25 秒（約 15 個命令）的註：試跑中 after 視窗最後一個命令在 t_c 之後 426～476 ms 解析，單次修正的最早重設在 t_c 之後 509～530 ms（y4 第一次、z1），餘裕 33～100 ms；串接的第二次修正（y4）在 379 ms 重設，after 視窗只剩 9 個命令（原本 15～16 個，`adversarial/analysis/lead_window_vs_reset.txt`）。所以 lead_after 要求至少 8 個命令（`n_lead_after`），不足時不合格 | a954aa3 | 時刻都是 probe 的 C++ steady clock | — |
| W_post 與 lead_after 最少命令 | 8 | 試跑每個事件 15～18 個（串接的第二次修正除外：lead_after 9 個） | a954aa3 | Client `sent`−`generated`；Match `resolved`−`host_accepted` | 每位玩家、事件的 epoch |

#### 13. 開發跑次的觀察（只供參考，不判定、不計入）

- **最終 runner 的 1 次開發跑次 z1**（δ 50、視窗 100、fault_at 2.0；主機沒有閒置閘門）碰到了越線：兩位玩家 W_post 1.57／2.14 ms，lead_after 69.47／68.89 ms（預測 69.36／68.75，差 +0.11／+0.14），t_c 之後 530 ms 兩位都 backlog 重設（30 Tick 合計 108，第 30 Tick 首次 ≥105）。「重設 ⇔ 領先 >66.7 ms」與模型在這個事件都成立。同一跑次的兩位玩家不獨立，所以這是 1 個單位。
- 量測腳本的初版把這個事件設限掉了（重設本身造成的重新播種與重新擷取落在設限視窗內），也讓 V1 判它不乾淨，等於越線那一側在結構上永遠無法判定。凍結版改為「結果決定之前」才設限（第 7 節），這是從開發跑次修正的定義，如實記錄。
- 試跑（原型 runner）與 z1 的 10 個事件：W_post 1.57～11.99 ms；model_diff +0.11～+0.31 ms；注入在 slew 結束前 64～93 ms 就結束；token 重建的 W_hat 與 W_post 的差 0.01～0.08 ms（y2 玩家 1 有重建漂移，差 3.79 ms）。
- 對第①步證據的乾跑：M3（host-ipc-250ms）前／後／預測 36.89／69.71／69.72 ms，與第①步相同。這個跑次的 frames 沒有相位計數器，所以 M3 是偵測器的 S 候選（`validation_only`，不會被判定計入）。5 筆（含 M3）都被 gen_gap 設限，M4 另有 trace_end。
- 對抗式檢查後的版本在同樣的三組乾跑中，每個跑次與事件的值都和初版相同，只多了 `n_lead_after` 欄位；試跑的預覽結論是 `step2`＝not passed（只有 4 個符合宣告參數的跑次，V1、V2 達不到 8 次的要求；這是預期的，不是判定）。

#### 14. 核准時要決定的事與未決事項

1. 本宣告的核准，以及 commit runner 與測試（session 頭 H）後原地建置的做法。
2. `test_run_short_stall.py` 要不要登記進 CTest（`tests/object_fps_pvp/CMakeLists.txt`，照 `action_runner` 的寫法）。目前沒有登記，本草稿不改 CMake。登記會讓 L1 執行它，其中 2 個測試用 loopback socket 與實際時間（下界 49 ms、上界 40 ms 的寬容值）。
3. runner 放在 `build/acceptance/object_fps_pvp/`（產品 owner 的 acceptance 工具），但它只服務 D48；D50 已寫明之後若要成為常設回歸再走 (c)。第③步結束後是否保留，屆時決定。
4. V3 的系統性偏差：10 個事件的 model_diff 全部為正（+0.11～+0.31 ms）。模型沒有扣掉 W_pre（0.08～0.12 ms），扣掉之後還剩 +0.01～+0.19 ms，其餘原因沒有查（hypothesis）。不改模型（決定 3 照第①步的形式寫），只記錄；1.0 ms 的容許已涵蓋這個偏差。
5. relay 送出比預定晚的值（`maximum_send_lateness_ms`，z1 為 2.15 ms）在非閒置主機上量到；只記錄。若閒置主機上仍明顯，第③步要把它列進可比性的前提。

## D48 第③步的結果（2026-10-10，**stopped**）

- 證據：`build/target/_build/test/logs/pvp-v7-d48-step3-20261010/`（`declaration.md`／`declaration.sha256`、`artifacts.sha256`、`preflight.txt`、`post-session.txt`、`session/`、`measure.jsonl`、`commands.txt`、`sha256.txt`）。session 頭 H＝`eac65bb`（相對 `8fabfa6` 只改 `docs/`）。
- 執行：preflight P1～P3 全部通過，跑前 `shasum -c` 24／24 OK；量測期間暫停 PR #73 的 auto-fix，結束後恢復；閒置閘門第一次就通過（3 次 sleeper 最大 3.7／4.2／8.7 ms）；18:33～19:15，84 個項目全部 completed；結束時 `shasum -c` 24／24 OK，主 checkout 與三棵 worktree 都沒變。
- **判定（凍結的 `measure3.py` 機械地給出）：`step3`＝stopped**。停止條件 7：兩個 chain 項目各有 1 次權威 Cooldown 拒絕（`actions.jsonl` rejection＝3）。依宣告第 8 節，停止條件優先；下面 J1～J3 的值是同一行 `judgement` 算出來的，**只記錄，不構成判定**。不補跑。
- **兩次 Cooldown 拒絕**（宣告要求標明那一發是否在注入視窗內被延遲）：

  | 項目 | 被拒的那一發 | 前一發 | 權威的間隔 |
  |---|---|---|---|
  | chain-r2-base（11.5 秒） | 玩家 1 的 31 號：release 之後 19 ms 才送出，**本身沒有被延遲**；observed 889 → resolved 890 | 30 號：注入開始時送出（−0.04 ms），**在視窗內被延遲**；observed 874 → resolved 881（晚 7 Tick） | 9 Tick＜`cooldownTicks` 10（`Combat.hpp:22`） |
  | chain-r5-09a-b2（2.0 秒） | 玩家 1 的 8 號：release 之後 31 ms 送出，本身沒有被延遲；observed 319 → resolved 320 | 7 號：注入開始後 3.9 ms 送出，在視窗內被延遲；observed 304 → resolved 311（晚 7 Tick） | 9 Tick |

  - 兩次的共同點：Client 在送出被拒的那一發之前約 139 ms，就已經收到前一發的裁決（resolved 881／311）；被拒的那一發都在 late 修正（t_c）之後約 114～115 ms 送出，在任何撤回之前。
  - 推論（未驗證）：late 修正之後，FireGate 對「這一發在哪個 Tick 解析」的預測偏了，guard 擋不住被注入壓縮的間隔。trace 沒有記錄 FireGate 的 R 預測，無法直接確認（觀測缺口）。
  - 次數：single 30 個跑次 0 次、chain 18 個跑次 2 次；第②步 8 個跑次 0 次。不是 clean 量測，不觸發第 09 批的重推條件 10，但和 FireGate 的前提（「領先＝目標」）直接相關。
- **只記錄的計算值**（同一行 `judgement`；第 8 節的讀法照套只供參考）：
  - **V4X**：一致。帶內 3 個事件：single-r1-base 66.1／66.2 ms 沒有重設，single-r8-base 67.46 ms 重設。
  - **J1**（算出來是成立）：base 2／10 對 09a 10／10，單側 Fisher p＝0.00036。P1 符合預測：09a 的 W_post 最大 0.156 ms，領先 70.35～70.76 ms，30 Tick 合計 105～108。base 的 W_post 散在 1.6～15.8 ms，重設的 2 個跑次就是領先 >67.4 ms 的那 2 個。
  - **J2**（算出來是成立）：20 個事件、10 個跑次碰到脆弱窗（`lead_after_max` 70.05～70.83 ms），全部撤回 +29.85～+32.75 ms，殘餘 −3.46～−0.60 ms，30 Tick 合計 71～74，沒有重設、沒有來回修正、`held_post` 0。`J2c_jitter`＝無法判定（09a-b2 的 network20／40 共 6 個跑次，late 修正 0 次）。
  - **J3**（算出來是不成立）：串接單位／重設單位：base 9／12、9／12；09a 10／12、12／12；09a-b2 2／10、4／10。09a-b2 重設的是 chain-r4、chain-r6（都是 11.5 秒，兩位玩家）：
    - r4：第一次 late 修正後 151 ms、重新擷取之前，又來一次 late 修正（共 −73.3 ms，缺陷 B 的串接）；重新擷取只撤回 33.3 ms，淨 −40 ms，領先 103.8 ms，t_c 後 529 ms 重設。
    - r6：t_c 後 168 ms 有一次重新擷取的修正，但找不到撤回的正向步（撤回量算不出來），領先停在約 67 ms，t_c 後 579 ms 重設。
    - r2 只撤回 +16.0 ms（殘餘 −17.3 ms），合計 92、沒有重設；r1、r5 完整撤回（+30.2、+29.8 ms）。
    - 時間線（相對注入開始；release 都在 220～230 ms）：完整撤回的 r1、r5，late 修正在 136～137 ms；部分撤回、沒有撤回或串接的 r2、r3、r4、r6，late 修正都較早（113～119 ms）。撤回的決定都在 release 之後（281～319 ms，r4 是 414 ms）。推論（未驗證）：late 修正越早，重新擷取用的 8 個樣本中，注入期間產生、仍帶著延遲的命令就越多，撤回量就越小。和開發跑次的「部分撤回」一致。
  - 只記錄的 36 個項目（network20／40、host-ipc／gateway-250ms）：late 修正 0 次、重設 0 次。
  - 主機（宣告第 4 節）：每棵 tree 的 sleeper 最大值中位數 5.6～7.1 ms、最大 9.6 ms，≥10 ms 的項目 0 個；relay 送出晚醒中位數 5.9～7.0 ms（最大 10.1 ms），比第②步閒置 session 的 3.1～4.9 ms 高，三棵 tree 相近。
  - （補充，2026-10-11，D63 ②）J3（延遲比重新擷取長時擋不住）歸「保留（條件力學）」：它是給定串接或長延遲之後的力學，被延遲的命令已過 Gateway 的過濾，不依賴截斷的頻率。見[事故紀錄](Incident/2026-10-10-gateway-silent-drop.md) 3.4。
- **意涵**（只記錄，交使用者決定）：照第 8 節的讀法套到計算值，是「J1 成立、J3 不成立：09a 不能單獨做，B2 也不夠；停下重新規劃」。也就是說，不管停止條件怎麼處理，下一步都是重新規劃 B2：單次短延遲（J2）擋得住，延遲比重新擷取長時（J3）擋不住。另外，Cooldown 拒絕顯示 FireGate 在 late 修正之後可能失去保證，這是第 09 批的新輸入。

## D48 第③步的準備：原型、審查與修正（2026-10-10）

workflow 5 個階段：原型 1 位（high）、審查 1 位（xhigh）、修正 1 位（high）、宣告 1 位（high）、D45 對抗式檢查 1 位（xhigh）。證據：`build/target/_build/test/logs/pvp-v7-d48-step3-prep-20261010/`（`proto/`、`review/`、`fix/`、`declare/`、`adversarial/`，各有 `commands.txt` 與 `sha256.txt`）。原型只在 worktree，不合併、不推送；主 checkout 全程乾淨。

- **三棵 tree**（detached worktree，都從 `8fabfa6` 開）：base `8fabfa6`（不改）；09a `e6885b7`（比例折讓、`InputSendDrainRate`＝80）；09a-b2 `c2705be`（parent＝`e6885b7`；B2：補償換算＋late 修正後以首次決定規則重新擷取，從不清 `phaseDecided_`）。三棵都從零建置，全量 CTest 70／70，既有斷言一個都沒改；wire、權威 digest、FireGate 常數都沒動。
- **09a 的實測和推導相符**：worker probe（每棵 09a 樹 10＋20 次）的等待 W0～W3＝16.4～16.7／12.2～12.5／8.0～8.2／3.8～4.1 ms，k≥4 為 0.13～0.40 ms；每步減 4.1～4.3 ms（推導 Δ＝4.167、N＝4）。worker probe 有 1 次失敗是既有的負載敏感斷言（「fully acknowledged 60 FPS publication caused excessive sends」，`04-client-roles.md:164`）；沒改動的 base 重跑 20 次也出現 1 次，不是原型造成的。
- **審查（xhigh）推翻或修正的說法**：
  1. **09a 的折讓把晚醒算進等待**（major，已修）：詳見 [09a](09a-input-send-pinning.md)「C 的兩種寫法」的更正。修正比審查者的式子多加了 bucket 剩餘空間的上限，因為修正者用模型找到審查者沒涵蓋的情境（工作執行緒一次性停頓 ≥20 ms）。
  2. **B2 的撤回會讓第②步的量測在結構上判不了 B2**（major，改宣告）：撤回是 late slew 之後的第二次相位修正，凍結的 `measure.py` 會把它設限成 `other_phase`、把它的 22.2 ms slew 間隔當成主機停頓，偵測器的修正量也會混進撤回。開發跑次證實了這一點（`declare/dev/measure2-dryrun.jsonl`）。所以第③步另寫 `measure3.py`，唯讀重用 `measure.py`，再追加 B2 的定義。
  3. **B2 的測試沒有釘住 4 個核心性質**（major，已補）：late 判斷用原始 slack、`reacquiring_` 永不清除、換算少了還沒 slew 的部分、重新擷取用錯死區，這 4 個突變原本都存活（T-B1／T-B2 的 lag＝2 讓 settle 時的樣本幾乎已帶著全部修正）。審查者的 X1～X4 加入後都被殺（`fix/mutate_b2.txt`）。
  4. **09a 的 worker 測試沒有釘住「只有等待過的送出才打折」與清除點**（major，已補）：詳見 09a 的「測試與突變」更正。
  5. minor：SeedLead 只清其中一項的突變存活；B2＝A3＋重新擷取（換算也改變了缺陷 A 與正向 slew 中的 late 判斷），第③步的差異無法分開歸因；重新擷取沒有遲滯與次數上限，間歇性成對遲到下的來回修正沒有量過。
- **開發跑次**（每棵 2 次，共 6 次；超過任務文字「每棵 1 次」，記錄、不判定）：三棵都照預測的方向行為。base W_post 12 ms、不越線；09a W_post 約 0.1 ms、領先 70.7 ms、重設；09a-b2 在 t_c 後 181 ms 撤回 +30.2 ms、合計 73、不重設。chain 案例的 B2 只撤回 +13.4 ms（延遲還沒結束就以 8 個樣本決定），殘餘 −19.9 ms、領先 57 ms 約 4 秒，是 B2 正式實作的設計輸入。
- **D45 對抗式檢查（xhigh）**：草稿「不能送核准」，套用修正後可以。推翻 2 項：產生空檔的固定帶（正向 slew 的最後一步是部分步，20.06～21.78 ms 的間隔有 17 個，草稿全算成主機停頓，B2 會在結構上判不了）；J2d 的比較型門檻（09a 與 B2 的 Held 觀測視窗約 0.3 秒對 2.8 秒，不可比）。需修正：撤回與來回修正要在重設處截止（草稿對第②步 session 乾跑就把 r4／r7 重設後的 reseed 首次決定記成「撤回」）、J2 的事件母體不能因 B2 自己的反應而排除、J1 的天花板、J3 的可計算性、量測腳本沒檢查宣告寫的 4 個停止條件。成立：循環性、J1 的檢定力、獨立單位、三棵 tree 的可比性、`session3.py`、REACHED（偏保守）。修正 C1～C7（`measure3.py`）與 R1～R13（宣告）都已套用；主對話以同一份證據重跑三組乾跑，輸出與檢查者的逐位元組相同。

## D48 第③步的事前宣告（2026-10-10 使用者核准：「宣告核准，四件事照建議，檔位照建議，開始跑」）

下面是證據目錄 `build/target/_build/test/logs/pvp-v7-d48-step3-prep-20261010/declare/declaration.md`（核准版 SHA-256 `55d5ef06…`；送核准時是 `757e1f4c…`，核准版只多了核准記錄與使用者要求的範圍限制）的全文。核准時複製到 session 的證據目錄，雜湊寫進 `declaration.sha256`。準備：三棵 tree 的原型與建置（上一節）、driver `session3.py`（`a3acafea…`）、量測腳本 `measure3.py` 凍結候選（`18ffde15…`，唯讀重用第②步的 `measure.py`）；D45 的 xhigh 對抗式檢查判定「修正後可以送核准」，修正已套用，主對話再依檢查原文逐項核對替換文字。

### D48 第③步 宣告：三棵 tree 的原型對照（base／09a／09a＋B2）

- 狀態：**2026-10-10 使用者核准**（原話：「宣告核准，四件事照建議，檔位照建議，開始跑」）：決定 1＝三棵 tree 共用 base 的 Match、Gateway 與 arena；決定 2＝84 個項目；J2d＝絕對門檻（每個 B2 J2 事件 `held_post`＝0）；開發跑次每棵 2 次（共 6 次）已確認、不計入判定。核准前另依使用者的要求補上第 1 節的範圍限制。
- D45 的對抗式檢查（xhigh，專門核對每個門檻的來源）已做完：2 項推翻（產生空檔的固定帶、J2d 的比較型門檻）、多項需修正，`measure3.py` 的修正 C1～C7 與本宣告的替換文字 R1～R13 都已套用（`adversarial/`；主對話以同一份證據重跑三組乾跑，輸出與檢查者的逐位元組相同）。
- 草稿（修正前）：`declare/declaration-draft.md`（`3e3b71da…`）、`declare/measure3-draft.py`（`01402a06…`），保留作記錄。
- 依據的頭：主 checkout `claude/pvp-v7-p2` 的 `8fabfa6`（任務文字寫的 `a5f3ce4` 之後多了一個只改 HANDOFF 的 docs commit；`git diff --name-only a5f3ce4 8fabfa6` 只有 `docs/`）。產品程式碼＝`a954aa3` 加上第②步的 runner 與它的測試（`6a298f4`）。
- 三棵 tree（detached worktree，原型不合併、不推送、不 commit 進 `claude/pvp-v7-p2`）：
  - base：`/Users/karasu/Code/Source/GYO-Engine-d48-base`，`8fabfa62967781449e765c1a0e1a426fd66e00c1`（沒有改動）。
  - 09a：`/Users/karasu/Code/Source/GYO-Engine-d48-09a`，`e6885b75a4255c6d6fb22ccc5dd4fd6767014801`（比例折讓、`InputSendDrainRate`＝80、送出後不超過 1 個 token 的上限）。
  - 09a-b2：`/Users/karasu/Code/Source/GYO-Engine-d48-09a-b2`，`c2705bed0f2c546e08829da50bbf186ebcea5c5d`（parent＝`e6885b7`；B2：補償換算＋late 修正後以首次決定規則重新擷取，不清 `phaseDecided_`）。
- 依據的決定：D48（第③步：三棵 tree 的原型對照，要宣告並經核准；測試設計成推論為真時會失敗，碰不到脆弱窗判為無法判定）、D50（Gateway→Match 的 IPC 延遲、b1、四個條件；②三棵 tree 用同一份 runner、同一個 relay）、D44、D47。
- 本草稿的工具、開發跑次與數字都在 `build/target/_build/test/logs/pvp-v7-d48-step3-prep-20261010/declare/`（`commands.txt`、`sha256.txt`）。原型的建置、測試、審查與修正在同目錄的 `proto/`、`review/`、`fix/`。

#### 1. 目的：D48 的兩個問題

第②步證實了注入乾淨、可以穩定重現單次 −33.3 ms 的 tracking 型 late 修正，而且「Match 端領先 ≈ 修正前＋33.33−W_post」在不越線那一側精度 ≤0.65 ms；現在的頭 8 個跑次中有 2 個（W_post 3.1～3.9 ms）走進脆弱窗並重設。第③步回答：

1. **問題 1（J1）**：09a 會不會提高「tracking 型 late 修正 → 30 Tick 合計 ≥105／backlog 重設」的比例？
2. **問題 2（J2、J3）**：B2 能不能讓它維持在 105 以下，而且在抖動下不來回修正、不增加 Held？

**可被推翻的預測**（第①、②步的模型；開發跑次只供參考，見第 13 節）：

| tree | 單次注入（δ 50／視窗 100）的預測 | 依據 |
|---|---|---|
| base | W_post 散在約 3～14 ms，約 1/4 的跑次越線並重設 | 第②步 2／8 跑次（W_post 3.1～3.9 ms 越線），其餘 6 個 W_post ≥7.4 ms、合計 90 |
| 09a | 每個事件 W_post ≤1.0 ms（約 0.1），Match 端領先約 70.3～71.3 ms（>67.667），**每個跑次都重設**（約 t_c＋0.5 秒） | 恢復上界 N＝4（`09a-input-send-pinning.md`）；worker probe 第 4 個以後的等待 0.13～0.40 ms（`fix/`）；領先＝修正前約 37.3＋33.33−W_post，加上第②步的 +0.04～+0.65 ms 偏差 |
| 09a-b2 | 同一個 late 修正（−33.3）先讓領先升到約 70 ms，約 0.18 秒後重新擷取撤回約 +30 ms，30 Tick 合計約 73（<105），不重設；撤回後 1 秒內沒有別的修正；注入結束 0.1 秒後沒有 Held | 09b 規劃的模型（`judge/sim_judge.txt`：短停頓 60～250 ms 時 120→74）；開發跑次 1 次 |

- 推論為真時，J1 會「成立」（09a 的重設比例顯著高於 base）。這正是 D48 要的「推論為真時會失敗」：若把 J1 當成 09a 的驗收條件「09a 不提高比例」，它會失敗。
- **無法判定**：base 與 09a 都沒有任何合格事件碰到脆弱窗（實測領先 >67.667 ms），或合格跑次不足；B2 沒有足夠的合格事件碰到脆弱窗；串接途徑（缺陷 B）在 base 與 09a 都沒有出現。無法判定不視為支持，也不視為反證；不補跑。
- 邊界附近一律用**實測的領先**，不用預測值（第②步：模型有 +0.04～+0.65 ms 的正偏差，r4 的預測 66.4 ms 在門檻下、實測 66.9 ms 在門檻上而且重設）。

本步不改權威、wire、FireGate 常數、late 修正上限、target、lead、backlog 門檻；原型也都沒有改這些（`proto/`、`fix/` 的說明）。

**範圍限制**（2026-10-10 使用者要求補上）：結論只在現在的 Gateway 過濾下成立；Client→Gateway 的遲到被過濾，不在本步範圍（D49）。Gateway 會略過 ≤`lastResolved` 的命令（`gateway/server.go:420-423`），所以 Client 端網路遲到現在幾乎不會產生 late 修正（第②步的試跑 0／5）；本步只注入 Gateway→Match 的 IPC 延遲。D49 第二階段（丟棄回報給 Match）實作後，Client→Gateway 的遲到會開始觸發 late 修正，屆時要依第 09 批的重推條件 11 重跑 M0，並用本步的方式加跑 Client→Gateway 短停頓的注入。補充（2026-10-14，D73）：D49 第二階段中已解析命令的轉送由 08c 提前完成；剩下的 `GatewayDropReport` 移到 v7.1，重推條件 11 在 v7.1 觸發。

#### 2. 產物與建置

- 三棵 tree 都在 commit 之後從零建置（`proto/build.sh`、`fix/build.sh`：先 `rm -rf build/target`，`FETCHCONTENT_SOURCE_DIR_*` 指向主 checkout 的 `_deps/*-src`），全量 CTest 70／70（不加 label 過濾）。證據：`proto/build-status.txt`、`proto/ctest-base.log`、`fix/ctest-09a.log`、`fix/ctest-09a-b2.log`。
- **決定 1（建議）：三棵 tree 共用 base 的 Match、Gateway 與 arena，只有 probe 依 tree 不同。**
  - 理由：
    - 09a 與 B2 只改 Client（`ClientConnection.cpp`、`LocalPlayerPrediction.{hpp,cpp}`）、`Movement.hpp`（多一個 Client 才用的常數與註解）、測試與 `worker_main.cpp`。`git diff --stat 8fabfa6 c2705be` 沒有任何 Match、Gateway（Go）或權威的檔案。
    - Match 執行檔裡沒有 Client 的程式：`nm -C` 在三棵 tree 的 Match 都找不到 `LocalPlayerPrediction` 與 `ClientConnection` 的符號（`PvpMatch` 有 40 個），action probe 有（20／135 個）。所以 tree 之間 Match 的差別只會來自嵌入的 worktree 路徑。
    - 三棵 tree 各自的 Match 與 Gateway 雜湊都不同（Match 嵌了 9 處 worktree 路徑；Gateway 的 `vcs.revision` 跟著各自的 commit），無法用雜湊證明相同。共用同一個檔案，就不必用原始碼差異或路徑映射去論證，也解決了 09b 規劃要求的「Gateway 逐位元組相同」。
    - 被比較的變數因此只剩 probe 內的 Client（`ClientConnection`、`LocalPlayerPrediction`、模擬角色），正是 09a 與 B2 改動的地方。
  - 代價：09a 與 09a-b2 自己建出的 Match 與 Gateway 不會被執行。它們的程式和 base 相同，限制寫在這裡。
  - 替代案：每棵 tree 用自己的 Match 與 Gateway，Gateway 以 P1' 形式（`go version -m` 去掉 `vcs.revision`、`vcs.time` 與路徑後相同，`proto/gateway-buildinfo-*.txt` 已確認）比較，Match 以上面的原始碼差異與符號論證。
- **產物（session 用；完整值寫進 `plan.json` 與 `artifacts.sha256`）**：

  | 產物 | 來源 | SHA-256 |
  |---|---|---|
  | Match（共用） | base `build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match` | `97abe4fcb0cb7fa6…` |
  | Gateway（共用） | base `build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway`（go1.27.1，`vcs.revision=8fabfa6`，`vcs.modified=false`） | `acfe7d316c3c36da…` |
  | arena（共用） | base `…/assets/object_fps_pvp/pvp_arena.json`（三棵相同） | `0026013c731a7d73…` |
  | action probe | base／09a／09a-b2 | `1fd80fd997005fd8…`／`30d294410307180f…`／`5281efc84cad0358…` |
  | timing probe | base／09a／09a-b2 | `5e770a3f798016f5…`／`e8e0c0397c25666e…`／`b0ed399754d6b0a7…` |

  完整清單：`proto/artifacts-base.sha256`、`fix/artifacts-09a.sha256`、`fix/artifacts-09a-b2.sha256`。
- **主 checkout**：只提供凍結的工具（雜湊核對）。核准時若把宣告與 HANDOFF 的更新 commit 成 H，H 只能改 `docs/`：driver 開頭檢查 `git diff --name-only 8fabfa6 HEAD` 在 `docs/` 以外是空的、`git status --porcelain` 是空的，不成立就拒絕執行；session 期間主 checkout 與三棵 worktree 的 HEAD 與 `git status --porcelain` 都不能變（每個項目之前核對）。
- **開跑前的 preflight**（任一不成立就停下，不開跑）：
  - P1：三棵 worktree 的 HEAD 等於上面的值，`git status --porcelain` 是空的。
  - P2：共用的 Match、Gateway、arena 與三棵 tree 的 probe 雜湊等於上表；凍結工具的雜湊等於第 3 節。
  - P3：`go version -m` 的共用 Gateway 顯示 `vcs.revision=8fabfa6…`、`vcs.modified=false`。
  - P1～P3 成立後寫 `artifacts.sha256`，開跑前與結束時用 `shasum -a 256 -c` 核對。

#### 3. 工具

- **runner（D50 條件②：三棵 tree 用同一份）**：主 checkout 的 `build/acceptance/object_fps_pvp/run_short_stall.py`（`b5a6d33c…`，`6a298f4` commit 的版本），不改。它在開頭核對 `action_probe.py`（`59002b43…`）與 `backpressure_probe.py`（`c88ceb89…`）的 sha256；每回結束確認 `result.json` 的 delay 欄位（D50 條件③）；`run_case` 的判定只記錄（D50 條件④）。每個注入項目以 `--rounds 1` 呼叫一次，輸出到該項目自己的新目錄。
- **只記錄的案例**用凍結的 `run_gameplay.py`（`0a2bc490…`；network20、network40，action probe）與 `backpressure_probe.py`（host-ipc-250ms、gateway-250ms，timing probe），都不改。
- **driver**：`session3.py`（草稿 sha256 `a3acafea4158e0f5ad9ca7676f1e49cff09b93cc0a5ec7a4b7523c8a827ddc83`；核准時凍結，複製到證據目錄）。
  - 依第 6 節的排程交錯執行三棵 tree，每個項目之前核對主 checkout、三棵 worktree 與所有產物的雜湊。
  - 寫 `plan.json`（宣告值、排程、凍結工具與產物的雜湊、主 checkout 與 worktree 的狀態）、`idle_gate.jsonl`、每個項目一行的 `progress.jsonl`、`summary.json`。
  - 停下（其餘項目記為 `not_run`）：runner 錯誤、產物或 repository 改變、sleeper 失敗、閒置閘門 30 分鐘不成立、SIGINT／SIGTERM。
  - 中斷的注意事項同第②步：SIGINT／SIGTERM 只送到 driver 自己的 PID 時才會做完當前項目再停；在終端機按 Ctrl-C 會讓子行程一起收到，當前項目變成 runner 錯誤。
  - 只記錄的案例在雙方的 trace 都寫出時算完成；工具自己的判定（`result.json`、`backpressure.json` 或 `failure.json`）只記錄。D44 型的重設在這些案例本來就可能出現，不算 runner 錯誤。
- **量測腳本**：`measure3.py`（D45 修正後的候選 sha256 `18ffde156b59bf822f0fcc15a50d7d6f2a57745daa2d2ba92694850845278d79`＝`adversarial/measure3-adv.py`；內含 `SESSION3_SHA256`，核准時若 `session3.py` 再改，兩者一起重算後凍結）。
  - **以唯讀方式重用第②步凍結的 `measure.py`**（`045714f12dd5020d88b4d34e0df07d85a6e6607d17651f76ee08263fc6fadf14`）：開頭核對它的 sha256，不符就拒絕（結束碼 2）；以唯讀方式載入，不寫 bytecode。事件偵測、W_pre、W_post、領先、排隊重播、q30_max、重設與設限都由它的 `measure_run()` 算。呼叫時把它的模組變數 `DECLARED` 暫時設成該案例的 δ／視窗（`measure_run` 用它核對參數），檔案本身不改。
  - 第①步的偵測器（`445271ce…`）經由 `measure.py` 載入並核對。
  - `measure3.py` 只在上面**追加**定義（第 7 節）；完整定義與常數寫在它的 docstring。
  - 判定模式讀 driver 的 `plan.json`，`declared` 必須等於 `measure3.py` 的 `DECLARED3`，否則拒絕。判定寫在最後一行 `judgement`，其中 `step3` 欄位是結論。
- **sleeper**：C1 的 `sleeper.py`（`98105dbf…`，與第②步相同），複製到證據目錄。
- 工具的整合測試（不執行任何程式）：`mock_session.py`（`9ea52750…`）把開發跑次與第 08b 批的 dev 案例以符號連結放進 driver 的目錄結構，再以判定模式跑 `measure3.py`。結果：84 個項目的排程與目錄結構、判定流程都能運作；只記錄案例的 `artifacts.json` 與計畫不同時，18 個項目都被判為停止條件（預期的，`mock-measure3.jsonl`）。

#### 4. 主機

- 閒置：session 期間不做開發；driver 以 `caffeinate -dims` 執行；前景沒有其他使用者程式（照 C1 第 2 次：關掉 Chrome、ChatGPT／Codex、Claude 桌面版的其他工作）；**暫停 PR #73 的 auto-fix**；背景的其他分析也先停下，跑完再恢復。
- 閒置閘門（C1 第 2 次的附註，與 S1、第②步相同；driver 的 `--idle-gate`）：第 1 個項目之前連續 3 次 5 秒 sleeper，每次最大值都 <10 ms。不成立就每 30 秒重試，最多 30 分鐘，仍不成立就停下。
- 每個項目前後各一次 5 秒 sleeper，並記錄 CPU 最高的 5 個程序（注入項目由 runner 的 `--sleeper` 做，只記錄的項目由 driver 做）。
- probe 的 TimerBaseline 只記錄，不分層、不判定。
- 可比性的前提：三棵 tree 在同一個 session 內交錯執行（第 6 節），主機狀態的漂移平均分給三棵 tree。relay 送出比預定晚的值（`maximum_send_lateness_ms`）每個項目都記錄：第②步閒置 session 為 3.1～4.9 ms，本次開發跑次（非閒置主機）為 8.1～9.6 ms。
- 每棵 tree 的 `maximum_send_lateness_ms` 與前後 sleeper 的最大值，以中位數與最大值列表（只記錄、不判定）；sleeper 最大值 ≥10 ms 的項目逐一標出。

#### 5. 注入與案例

- **D50 條件①**：注入延遲的是 Gateway→Match 方向的**全部**流量（移動輸入、動作、ACK、控制）。Match→Gateway 永遠不延遲、不暫停。位元組順序不變，串流實際上要到約「視窗＋δ」才恢復。
- **D50 條件②**：Python relay 整場都在 Gateway 與 Match 之間（`action_probe.py:531-533`），不是正式的部署拓撲。三棵 tree 都用同一份 runner（同一個 sha256 `b5a6d33c…`）、經過同一個 relay，三者因此可比；數字不能當成正式拓撲的值。
- 案例：
  - **single**（判定 J1、J2）：δ 50 ms、視窗 100 ms，fault_at 交錯 2.0／11.5 秒（與第②步相同的注入；選擇理由與 11.5 秒的限制見第②步宣告第 5 節）。
  - **chain**（判定 J3，缺陷 B 的途徑）：δ 80 ms、視窗 150 ms，fault_at 交錯 2.0／11.5 秒。第②步的試跑 y4 在這組參數下產生兩次串接的 late 修正（共 −66.7 ms，領先約 94 ms，兩位玩家都重設）；本次開發跑次在 base 與 09a 各有 1 位玩家串接（第 13 節）。串接後的重設與 W_post 無關，09a 管不到；這組參數檢驗 B2 能不能擋住這條途徑。
  - **只記錄**（J2c 的抖動部分除外，見第 8 節）：network20、network40（`run_gameplay.py`，action probe，有相位計數器）；backpressure 的 host-ipc-250ms、gateway-250ms（`backpressure_probe.py`，timing probe，沒有相位計數器，只能用偵測器的 S 候選與產生相位步）。D44 的 M3 就出現在 backpressure host-ipc-250ms。
- 兩個注入時刻的視窗內都有動作的裁決（第②步宣告第 5 節），`run_case` 的判定可能受影響，只記錄。

#### 6. 跑次與排程

- **12 輪，84 個項目**（每個項目一次 probe）：
  - single：第 1～10 輪，每棵 tree 10 次（2.0 秒、11.5 秒各 5 次：奇數輪 2.0、偶數輪 11.5）。
  - chain：第 1～6 輪，每棵 tree 6 次（各 3 次）。
  - 只記錄：第 k 輪跑 `[network20, network40, host-ipc-250ms, gateway-250ms]` 的第 (k−1) mod 4 個，每個案例每棵 tree 3 次。
- **交錯**：每一輪依序跑 single、chain、只記錄，每個案例內三棵 tree 的順序為 `TREES[(k−1)%3:]＋TREES[:(k−1)%3]`（base、09a、09a-b2 輪流先跑）。不隨機化，不補跑。
- **跑次的理由**：
  - 獨立單位是跑次：同一跑次的兩位玩家 W_post 相差 ≤0.6 ms（第②步），結果也一起重設。
  - J1 用比例比較：第①步的檢定力估計是每棵 tree 約 8 次（真實 0.25 對 1.0）到 18 次（0.25 對 0.75）。每棵 10 次時，若 09a 10／10 重設，base 只要 ≤6／10，單側 Fisher 的 p 就 ≤0.043（base 2／10 時 0.0004、5／10 時 0.016）。若 09a 的真實比例只有 0.75，10 次的檢定力就不夠，結果會落在「無法判定」，這一點寫進讀法。
  - 比例之外，以連續量做預測檢查：09a 的每個事件 W_post ≤1.0 ms（P1），以及 base／09a 的「重設 ⇔ 實測領先 >66.667 ms」（V4X）。
  - 最少合格跑次 8／10：第②步 8／8 重現；容許 2 次因主機雜訊或設限而不合格。
  - chain 6 次：開發跑次中 base 與 09a 各 1／2 位玩家串接、第②步試跑 y4 為 2／2。若每跑次串接機率為 0.5，base 與 09a 共 12 次都沒有串接的機率約 0.0002。B2 最少要有 4／6 個乾淨跑次。
  - 只記錄的案例每棵 3 次：只用來看 B2 在抖動與故障下會不會觸發撤回、會不會出現來回修正，不做比例比較。
- **時間**：開發跑次每個注入項目約 31 秒（含前後 sleeper），只記錄的項目約 25～30 秒；84 個項目約 45 分鐘，另加閒置閘門。
- 獨立性的限制：三棵 tree 共用同一份 gameplay 計畫（`gameplay-plan.json` `65bad3b1…`），fault_at 只有兩個值。W_post 的分布由注入當下的 token 水位決定（第②步的經驗擬合），不是隨機抽樣；只記錄，不作為判定。

#### 7. 定義（摘要；完整定義與常數在 `measure3.py` 與 `measure.py` 的 docstring）

沿用第②步（`measure.py`，不改）：事件（P 事件，tracking 型）、`in_span`、`post_reset`、`chained`、W_pre、W_post、lead_before、lead_after（t_post 之後 0.25 秒產生的命令的中位數，至少 8 個）、lead_pred、model_diff、排隊重播與 q30_max、`first_ge105`、結果（［t_c, t_c＋3 秒］內的 backlog 重設）、設限（只看結果決定之前）。

`measure3.py` 追加（所有 tree 用同一套定義）：

- **產生空檔 v3**：真實命令的產生間隔 >20 ms 算空檔，**但凍結偵測器正向產生相位步內、而且 ≤T/0.75＋0.4＝22.62 ms 的間隔不算**。
  - 「步內」的條件：間隔的結尾命令與前一個命令同 epoch、序號相連，並落在某個 sign>0 的步的 first_seq～last_seq 內。這就是第①步偵測器自己的 episode 定義。
  - 理由：B2 的撤回是正向 slew，發生在 V1 視窗內；第②步的「>20 ms」會把它當成主機停頓，讓 09a-b2 的跑次在結構上永遠不乾淨（開發跑次就是這樣，第 13 節）。正向 slew 每步 22.22 ms，但最後一步是部分步，間隔＝T＋（修正量 mod T/3），可能落在 17.67～22.22 ms 的任何位置。
  - 實例：第②步 session 與開發跑次的自然視窗修正，就有 20.06～21.78 ms 的部分步間隔（`adversarial/gap_rule_check.out.jsonl`）。
  - 為什麼不用固定帶：草稿的固定帶［21.82, 22.62］會把撤回量約 +31.1～+32.9 ms 的 B2 撤回當成主機停頓（`adversarial/synthetic-*.jsonl`）。
  - 步內的間隔照樣列出（`slew_band_intervals`）。
  - 這條規則取代第②步的規則，用在 V1 與事件的設限，三棵 tree 相同。
- **修正量 v3**：`size_v3`＝d(after) 的中位數 − d(before) 的中位數，d(s)＝產生時刻 − s·T。before＝first_seq−9～first_seq−1（同偵測器），after＝last_seq～min(last_seq＋8, 下一個產生相位步的 first_seq−1)。理由：B2 的撤回在 late slew 結束後 2～3 個命令就開始，偵測器取的 9 個命令會混進撤回（開發跑次的淨值是 −22.2 ms，不是 −33.3）。之後沒有別的步時，和偵測器的值相同。`qualifies_v3`＝tracking、increment 1、不是 chained、|size_v3＋33.333| ≤1.0 ms。
- **撤回（withdrawal）**：late 修正之後 1 秒內、而且在該玩家下一次非 respawn 重設之前，第一個「`phase_corrections` 增加、`phase_late_corrections` 不變」的幀。這是 B2 的重新擷取；`phase_state` 仍是 Tracking，所以只能看計數器。
  - 為什麼在重設處截止：重設之後 reseed 的首次決定，計數器變化也一樣。第②步 session 的 r4／r7 在重設後 187 ms 就有一次，草稿把它記成 t_c 後 699／702 ms 的「撤回」（`adversarial/compare-validate.txt`）。
  - 它的產生相位步＝之後第一個正向步。
  - `withdraw_ms` 是撤回的淨位移；`residual_ms` 是撤回後相對 late 修正之前的淨位移。
  - 三棵 tree 都計算（base 與 09a 預期沒有）。
- **設限 v3**：第②步的設限，把 `gen_gap` 換成產生空檔 v3，把 `other_phase` 換成「［t_P, t_end）內**撤回以外**的其他相位修正」。
- **領先**（都是實測）：`lead_after_max`＝t_post 之後 0.25 秒產生的命令中，逐命令領先的最大值（撤回到達之前的暴露；base 與 09a 約等於 lead_after 加上分布寬度）；`lead_min_post`＝［t_post, t_end）的最小值；`lead_settled`＝［t_c＋1 秒, min(t_c＋2 秒, t_end)）的中位數（至少 8 個）。
- **碰到脆弱窗（REACHED）**：`lead_after_max` >66.667＋1.0＝67.667 ms。
- **Held**：`held_post`＝該玩家在［release＋0.1 秒, t_end）內 source 不是 actual 的 `resolved`。注入本身造成的 held 在 release 之後 1 Tick 內結束（開發跑次最晚在 release＋0.3 ms；第②步 session 8 跑次 × 2 位玩家，在［start, release＋0.1 秒］以外 0 筆）。
- **來回修正的特徵（OSC）**：撤回之後 1 秒內又有任何相位修正，或 3 秒內又有第二次撤回。
  - 兩個視窗都在該玩家下一次非 respawn 重設處截止。重設本身由 J2a／J3 判定，它的 reseed 首次決定不算來回修正。
  - 理由：撤回的修正會清掉視窗，之後的正常決定要 240 個樣本（約 4 秒；開發跑次撤回後的下一次修正在 4.09～4.11 秒）。所以 1 秒內的修正只可能是另一次 late 修正，3 秒內的第二次撤回也只可能跟在另一次 late 修正之後。
  - timing probe 的兩個案例沒有計數器，算不出 OSC，只記錄 late 步之後的正向步。
- **合格**：
  - single：乾淨（V1 v3）跑次中，`in_span`、`qualifies_v3`、沒有設限（v3）、W_post 與 lead_after 都算得出來的事件。跑次「合格」＝至少一個合格事件；`run_reset`＝有合格事件 backlog 重設；`run_reached`＝有合格事件碰到脆弱窗。
  - **J2 的事件**（`eligible_j2`，只用在 09a-b2 的 J2）：取乾淨（V1 v3）single 跑次中，每位玩家**第一個** `in_span` 事件。條件：
    - tracking、increment 1、|size_v3＋33.333| ≤1.0 ms；
    - 沒有主機或邊界類的設限（gen_gap_v3、client_gap、match_gap、life、trace_end、fault_boundary）；
    - `lead_after_max` 算得出來。
    - **不**因為 B2 自己的反應而排除：之後又串接的 late 修正、其他相位修正（other_phase_v3）與 starvation／sequence_exhausted 重設（other_reset），都是 J2 要判定的結果，不是要去掉的干擾。
  - chain：乾淨（V1 v3，但**不要求**「注入在 slew 結束前就結束」：缺陷 B 的途徑需要延遲持續到第一次 slew 之後）跑次中，每位玩家的第一個 `in_span`、tracking 型 P 事件是一個單位，記下是否串接、修正量合計、q30_max、是否重設。

#### 8. 判定

判定由凍結的 `measure3.py` 的 `judgement` 行機械地給出；`run_case`、`run_gameplay.py`、`backpressure_probe.py` 的判定不參與。

- **V4X（模型檢查，base 與 09a 的合格 single 事件）**：沿用第②步的 V4（門檻 66.667 ms，帶寬 ±1.0 ms，用實測的 lead_after）。lead_after >67.667 而沒有 backlog 重設，或 <65.667 而重設了，就是「推翻」。B2 不在這裡檢驗（它的 lead_after 混進了撤回）。第②步留下的越線那一側在 09a 上可以檢驗。
- **J1（問題 1）**：r_tree＝重設的合格跑次／合格跑次。
  - 可判定 ⇔ 以下三項都成立：
    - base 與 09a 都有 ≥8 個合格跑次；
    - base 或 09a 至少有一個合格事件碰到脆弱窗；
    - base 不是全部重設（r_base＜1）。在天花板上 09a 不可能更高，`r_09a ≤ r_base` 不能讀成「09a 沒有提高比例」。
  - 成立（09a 提高比例）⇔ r_09a > r_base，而且單側 Fisher 精確檢定（超幾何，09a > base）p ≤0.05。
  - 不成立 ⇔ r_09a ≤ r_base。
  - 其餘（較高但 p >0.05、合格跑次不足、沒碰到脆弱窗）⇔ 無法判定。
  - 隨判定一起記錄預測 P1：09a 每個合格事件 W_post ≤1.0 ms（符合／不符合）。P1 不符合時，代表 09a 的折讓在實際系統中沒有讓等待在 4 個命令內歸零，結果要另外說明。
- **J2（問題 2，single，09a-b2）**：對象是碰到脆弱窗的 B2 `eligible_j2` 事件（第 7 節）。
  - **J2a**：沒有 backlog 重設、q30_max <105，而且撤回之後到 t_c＋3 秒沒有任何非 respawn 重設（`resets_after_withdrawal`，例如撤回過頭造成的 Starvation）。
  - **J2b**：每一個都有撤回。
  - **J2c**：09a-b2 的 single、chain 與 network20／40 跑次都沒有來回修正的特徵。
    - timing probe 的兩個案例沒有計數器，不參與。
    - 結果依案例分列。
    - network20／40 的部分另外寫成 `J2c_jitter`：B2 在那裡從未撤回時是「無法判定（抖動下沒有觸發撤回）」，這不影響 J2 本身。
    - 只在 chain 出現的來回修正，代表延遲比重新擷取長時 B2 會再修一次，照樣算 J2 不成立。
  - **J2d**：每一個的 `held_post`＝0。
    - 來源：第②步 session（a954aa3，閒置主機）8 個跑次 × 2 位玩家，在［start_ns, release_ns＋0.1 秒］以外的 Held 都是 0；開發跑次 6 次也都是 0（`adversarial/post_reset_corrections.out.jsonl`）。
    - 三棵 tree 的合計只記錄。草稿的比較型門檻（B2 ≤ 09a）不可比：09a 的事件約在 t_c＋0.5 秒重設，Held 只觀測約 0.3 秒；B2 不重設，約 2.8 秒；事件數也不同。
  - **成立** ⇔ J2a～J2d 都成立，而且有 ≥8 個 B2 single 跑次含碰到脆弱窗的 `eligible_j2` 事件。
  - **不成立** ⇔ 任一項失敗（失敗不受數量限制）。
  - **其餘** ⇔ 無法判定。
- **J3（缺陷 B 的途徑，chain）**：
  - **可判定** ⇔ 以下兩項都成立：
    - base 或 09a 至少有 1 個串接單位（這條途徑真的被走到）；
    - 09a-b2 有 ≥4 個乾淨的 chain 跑次，而且其中每個單位的 reset 與 q30_max 都算得出來。第一個事件沒有對上產生相位步時兩者都是空的，要列出（`b2_units_not_computable`），不算通過。
  - **成立** ⇔ 09a-b2 每個 chain 單位都沒有 backlog 重設、q30_max <105，而且撤回之後到 t_c＋3 秒沒有任何非 respawn 重設。
  - **不成立** ⇔ 任一單位違反。
  - **其餘** ⇔ 無法判定。
- **結論欄位 `step3`**：任一停止條件 →「stopped」；否則 V4X 推翻 →「refuted (V4X)」；否則列出 J1、J2、J3 的結果。
- **讀法**（給 D48 的 (a)／(b) 決定；決定由使用者做）：
  - J1 成立、J2 成立（J3 成立或無法判定）：09a 單獨做會讓傳輸型 late 修正幾乎都走到 backlog 重設，B2 能擋住。支持 (b)：09a 與 B2 一起處理（09b 重新規劃 B2 的正式實作與 L1 閉環）。
  - J1 成立、J2 或 J3 不成立：09a 不能單獨做，B2 也不夠；停下重新規劃。
  - J1 不成立：09a 沒有提高比例，支持 (a)：09a 照原計畫做。
  - **「成立」的強度**：J2 與 J3 是單臂、零失敗的判定。全部跑次都沒有失敗時，在 95% 單側信賴下只能排除以下比例以上的「每個跑次失敗機率」：
    - J2：≥8 個跑次 >31%，10 個 >26%；
    - J3：6 個跑次 >39%，最少 4 個時只到 >53%。
    - 所以 J2／J3 成立的意思是「B2 沒有出現大比例的失敗」，不是「B2 不會失敗」。剩下的部分要在 (b) 之後由 L1 閉環與 L2 補上。
  - **J1 的檢定力**（假設 09a 全部重設、base 的真實比例 0.25）：
    - 每棵 tree 只有 8 個合格跑次時，09a 8／8 需要 base ≤4／8（p＝0.039）；09a 只有 9／10 重設時，base 要 ≤4／10（p＝0.029）。
    - J1 成立的機率：8、9、10 個合格跑次時分別約為 0.973、0.990、0.997。
  - 推翻或無法判定：回報，由使用者決定；不補跑。
  - 「推翻」（V4X）代表模型或邊界錯了，D44 的推論鏈要重看；停在第③步回報。
  - 本步的 base 與 09a 的 backlog 重設是預期的觀測結果，**不是**停止條件（與第 08b 批 L2 的 D44 停止條件不同）。

#### 9. 只記錄

- `run_case`、`run_gameplay.py`、`backpressure_probe.py` 的判定與錯誤；TimerBaseline；每個項目前後的 sleeper 與 CPU 最高的程序；relay 的 `maximum_send_lateness_ms`、`delayed_chunks`。
- 第②步的所有欄位（W_pre、slew 期間的等待、W_post_first、lead_after 的 P10／P90 與 1 秒版、`n_lead_after`、排隊軌跡、`first_ge105`、重設在 t_c 之後幾 ms、link B、token 水位重建）。
- `slew_band_intervals`、`size_v3` 與偵測器 `size_ms` 的差、撤回的時刻／`withdraw_ms`／`residual_ms`、`lead_after_max`、`lead_min_post`、`lead_settled`、`held_post`、`corrections_after`（t_c 之後 6 秒內的每次修正）。
- 每棵 tree 的 W_post、lead_after、lead_after_max、q30_max 的分布（`rates`）。
- chain：每棵 tree 的串接單位數、重設單位數、B2 撤回後的殘餘（開發跑次：撤回只有 +13.4 ms，殘餘 −19.9 ms，領先 57 ms，維持到約 4 秒後的視窗修正）。
- 只記錄的案例，每位玩家：late 修正（有計數器時用 P 事件，否則用 S 候選）與各自的 q30_max、撤回次數、相位修正總數、非 respawn 的重設與原因、Held 總數；沒有計數器的跑次另列 late 步之後 1 秒內的正向步（撤回的候選）。
- 範圍之外的自然 late 修正、post_reset 事件、被設限的事件與原因。

#### 10. 停止條件（立即停下，保留全部證據，不重跑，交使用者決定）

1. 範圍需要擴大：要改權威（authority digest）、wire、FireGate 常數、late 修正上限、target、lead、backlog 門檻，或要改凍結檔、runner、原型的程式。
2. runner 錯誤（E）：注入項目的 runner 沒有完成（`run_case` 例外、`injection_unproven`）、只記錄的項目沒有寫出雙方的 trace、sleeper 失敗、閒置閘門 30 分鐘不成立、項目逾時（900 秒）。
3. 分析錯誤（A）：`measure3.py` 對任一項目丟出例外，或 single／chain 的 frames 沒有相位計數器。
4. 雜湊不符：driver 開頭的凍結工具核對；開跑前與結束時的 `artifacts.sha256`；`measure3.py` 對 `measure.py` 與偵測器的核對；每個項目之前的產物核對；任一項目的 `artifacts.json` 與該 tree 的計畫產物不同；`plan.json` 的 driver 與 sleeper 的 sha256，不等於 `measure3.py` 凍結的 `SESSION3_SHA256`／`SLEEPER_SHA256`；`summary.json` 的 `artifacts_end` 與 `plan.json` 的 artifacts 不同。
5. 主 checkout 或 worktree 改變：每個項目之前核對的 HEAD 或 `git status --porcelain` 改變；`summary.json` 的 `repository_end`／`worktrees_end` 與 `plan.json` 不同；開跑前或結束時 `git status --porcelain` 不是空的。
6. preflight（P1～P3）不成立；session 被中斷（`summary.json` 的 `interrupted`；`measure3.py` 會判為停止）；driver 異常結束、沒有 `summary.json`（`measure3.py` 拒絕，結束碼 2）。
7. 任一注入項目出現權威 Cooldown 拒絕（`actions.jsonl` 中 rejection＝3；P2 的停止條件）。回報時標明那一發是否在注入視窗內被延遲。
- 失敗或被設限的項目照樣保留並列入彙總；不補跑。

#### 11. 證據

`build/target/_build/test/logs/pvp-v7-d48-step3-<YYYYMMDD>/`（session 當天的日期，主 checkout 內，git ignore）：

- `declaration.md`（本宣告的核准版）與 `declaration.sha256`；`artifacts.sha256`；`session3.py`、`measure3.py`、`sleeper.py`（複本）；`commands.txt`；`preflight.txt`（P1～P3、開跑前的 `shasum -c`、主 checkout 與 worktree 的 `git status --porcelain`、`git diff --name-only 8fabfa6 HEAD`）。
- `session/`：driver 的輸出（`plan.json`、`idle_gate.jsonl`、`progress.jsonl`、`summary.json`、`runs/<案例>-r<k>-<tree>/` 與各自的 stdout）；`run.stdout.txt`。
- `measure.jsonl`：`python3 -I measure3.py <第②步 measure.py> <第①步 detector.py> session` 的輸出（每個項目、每個事件、最後一行 judgement，含 `step3`）。
- 結束時的 `sha256.txt`（全部檔案）。

預定的命令（核准之後；`E`＝證據目錄，`L`＝`build/target/_build/test/logs`，`B`＝`/Users/karasu/Code/Source/GYO-Engine-d48-base/build/target`）：

```
git status --porcelain && git diff --name-only 8fabfa6 HEAD          # 空；只有 docs/
for t in base 09a 09a-b2; do git -C ../GYO-Engine-d48-$t rev-parse HEAD; git -C ../GYO-Engine-d48-$t status --porcelain; done   # P1
/usr/local/go/bin/go version -m $B/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway                                      # P3
shasum -a 256 <共用 Match、Gateway、arena> <三棵 tree 的 action／timing probe> <凍結工具>                                          # P2
#（P1～P3 成立後寫 $E/artifacts.sha256）
shasum -a 256 -c $E/artifacts.sha256 && git status --porcelain
caffeinate -dims python3 $E/session3.py --output $E/session --sleeper $E/sleeper.py --idle-gate > $E/run.stdout.txt
shasum -a 256 -c $E/artifacts.sha256 && git status --porcelain
python3 -I $E/measure3.py $L/pvp-v7-d48-step2-20261010/measure.py $L/pvp-v7-d48-step1-20261010/detector.py $E/session > $E/measure.jsonl
```

#### 12. 門檻的來源（D45）

建置欄：「三棵 tree」＝第 2 節的共用 Match `97abe4fc…`、Gateway `acfe7d31…`、arena `0026013c…` 與各 tree 的 probe；「a954aa3」＝第②步宣告第 12 節的建置（Match `c9cc6617…`、Gateway `ef5b9779…`、probe `d45a1f0b…`）。「開發跑次」＝`declare/dev/` 的 6 個跑次（每棵 tree δ50／100 與 δ80／150 各 1 次，fault_at 2.0，主機沒有閒置閘門，sleeper 最大 7.2～8.9 ms），設定與 session 相同（共用 base 的 Match 與 Gateway，各 tree 的 action probe）。

| 門檻 | 值 | 來源 | 建置 | 指標 | 流量 |
|---|---|---|---|---|---|
| single 的 δ／視窗／fault_at | 50／100 ms；2.0／11.5 秒 | 第②步的 session（8／8 重現，`step2`＝passed）；選擇理由見第②步宣告第 5、12 節 | a954aa3 | P 事件＋產生相位步 | v5 gameplay probe 60 FPS；relay 整場在 Gateway 與 Match 之間；只延遲 Gateway→Match |
| chain 的 δ／視窗 | 80／150 ms | 第②步試跑 y4（串接 −66.7 ms，兩位都重設）；開發跑次 base、09a 各 1／2 位玩家串接、09a-b2 0／2 | a954aa3；三棵 tree | 同上 | 同上 |
| 閒置閘門 | 3 次 sleeper 最大 <10 ms，30 秒重試、30 分鐘上限 | C1 第 2 次、S1、第②步沿用 | 獨立的 sleeper | sleeper 的 1/60 秒絕對期限晚醒 | 無 session 流量 |
| 產生空檔 v3 | >20 ms，扣除凍結偵測器正向產生相位步內 ≤22.62 ms 的間隔 | Client 的步長只有 16.67／13.33／22.22 ms（第②步第 12 節），加上正向 slew 最後一個部分步（T＋修正量 mod T/3，即 17.67～22.22 ms）；22.62＝第①步凍結偵測器的 HI，步的範圍＝它的 episode；第②步 session 與開發跑次整條 trace 中 >20 ms 的間隔共 17 個，全部是正向步的部分步（20.06～21.78 ms），步外 0 個（`adversarial/gap_rule_check.out.jsonl`）；B2 開發跑次撤回的部分步是 19.02～19.26 ms。草稿的固定帶［21.82, 22.62］只看了整步，被 D45 推翻 | 三棵 tree 的 Client；a954aa3 | 每位玩家真實命令 `generated` 的相鄰間隔；偵測器的正向步 | 每位玩家；間隔要同 epoch、序號相連才算步內 |
| Client／Match runtime_gap | 同第②步 | 第②步第 12 節 | a954aa3 | 同第②步 | 同第②步 |
| 修正量 | −33.333±1.0 ms（size_v3） | 上限 2 Tick（`LocalPlayerPrediction.cpp:97-98`、`Movement.hpp:59`）；開發跑次 size_v3 −33.29～−33.35 ms（B2 的偵測器淨值為 −22.2／−19.9，原因見第 7 節） | 三棵 tree | 產生相位步，after 截到下一個步之前 | 每位玩家、事件的 epoch |
| 脆弱窗門檻與帶寬 | 66.667 ms，±1.0 ms | 第②步 V4 的來源（`queued`≈floor(領先÷16.667)，backlog 門檻 105，`Movement.hpp:38-39`、`PvpMatch.cpp:427-451`）；第②步 session：66.8～67.4 ms 的 4 個帶內事件都重設（合計 105／107），≤63.3 ms 的 12 個都沒有 | a954aa3 | Match `resolved`−`host_accepted` | 只算 actual 解析的移動命令 |
| REACHED 用 lead_after_max | >67.667 ms | 撤回在 late slew 結束後約 33 ms 就開始（開發跑次 B2：late slew 結束 209 ms、撤回步開始 242 ms），after 視窗的中位數混進撤回（45.8 ms），最大值才是撤回到達前的暴露（70.6 ms，和 09a 的 70.9 相同）。base／09a 的最大值比中位數大 0.14～0.27 ms（開發跑次）。第②步 session 16 個合格事件，最大值比中位數大 0.07～0.28 ms；4 個帶內事件（66.8～67.4 ms）都重設，但最大值 67.06～67.61 ms 沒有超過 67.667，所以 REACHED 比「會重設」保守 | 三棵 tree；a954aa3 | 同上 | 同上 |
| P1：09a 的 W_post 上限 | 1.0 ms | worker probe 第 4 個以後的等待 0.13～0.40 ms（`fix/worker-*.json`，09a 與 09a-b2 各 10 次）；恢復最多 4 個命令（推導）；W_post 是約 15 個命令的中位數；開發跑次 0.09～0.12 ms | 09a、09a-b2 | Client `sent`−`generated` | 每位玩家、事件的 epoch |
| J1 的最少合格跑次 | 8／10（每棵） | 第②步 8／8 重現；V2 的要求 ≥7／8 | a954aa3 | 合格（v3） | — |
| J1 的檢定 | 單側 Fisher，α＝0.05 | 第①步第 8 點的檢定力估計；第 6 節的數值 | — | 跑次的重設比例 | — |
| 撤回的時間窗 | late 修正後 1 秒，在下一次非 respawn 重設處截止 | 開發跑次的撤回在 t_c 後 169～181 ms；下一次正常的視窗決定在約 4 秒後（240 個樣本；開發跑次 4.09～4.11 秒）；第②步 r4／r7 重設後 187 ms 的 reseed 首次決定（計數器變化與撤回相同） | 09a-b2；a954aa3 | 幀的 `phase_corrections`／`phase_late_corrections` | 每位玩家 |
| 來回修正（OSC） | 撤回後 1 秒內任何修正，或 3 秒內第二次撤回；在下一次非 respawn 重設處截止 | 同上：撤回會清掉視窗，之後的正常決定要約 4 秒；第②步 r4／r7 重設後 187 ms 的 reseed 首次決定 | 09a-b2；a954aa3 | 同上 | 每位玩家 |
| Held 的寬限 | release＋0.1 秒 | 開發跑次中注入造成的 held 最晚在 release＋0.3 ms（09a chain），之後 0 筆；第②步 session 中注入造成的 held 最晚在 release −0.05 ms，之外 0 筆；0.1 秒＝6 Tick 的餘裕 | 三棵 tree；a954aa3 | Match `resolved` source≠actual | 每位玩家 |
| J2d | 每個 B2 J2 事件 held_post＝0 | 第②步 session 注入之外 0 筆 Held（8 跑次 × 2 位玩家，整條 trace）；開發跑次 0 筆；預測表「注入結束 0.1 秒後沒有 Held」。草稿的比較型門檻（B2 ≤ 09a）因觀測視窗不同被 D45 推翻 | a954aa3；三棵 tree | `held_post`（［release＋0.1 秒, t_end）） | 每位玩家 |
| J2 的最少跑次 | 碰到脆弱窗的 B2 合格跑次 ≥8 | 同 J1 | — | — | — |
| J3 的最少跑次 | 乾淨的 B2 chain 跑次 ≥4／6；base 或 09a ≥1 個串接單位 | 開發跑次串接 1／2（base、09a）；第 6 節的機率；零失敗時，6／4 個跑次只能排除 >39%／>53% 的每跑次失敗機率 | 三棵 tree | 同第 7 節 | 同上 |
| 結果視窗 | t_c＋3 秒；修正記錄到 t_c＋6 秒 | 第②步：單次修正的越線在 t_c 後約 0.5 秒重設（開發跑次 09a 529 ms，chain 311～563 ms）；下一次視窗修正約 4.1 秒（開發跑次 base 4.10 秒）；trace 約 16.0 秒結束（11.5 秒的事件 t_c＋6 秒會超出，`corrections_after` 只到 trace 結束為止） | 三棵 tree | probe 的 C++ steady clock | — |
| lead_after、lead_settled 的最少命令 | 8 | 第②步第 12 節 | a954aa3 | 同第②步 | 同第②步 |

#### 13. 開發跑次的觀察（只供參考，不判定、不計入）

證據：`declare/dev/`（`dev_rounds.sh`、`dev_rounds.log`、每個跑次的 runner 輸出、`measure2-dryrun.jsonl`、`measure3-dryrun-dev.jsonl`、`explore_dev.txt`）。主機沒有閒置閘門（sleeper 最大 7.2～8.9 ms，relay 送出晚 8.1～9.6 ms）。每個跑次都是 1 次、fault_at 2.0，**定性**檢查三棵 tree 是否照預測行為；同一跑次的兩位玩家不獨立。

| tree | 案例 | W_post（ms） | lead_after／lead_after_max（ms） | 撤回（t_c 後、淨位移） | q30_max | 重設 |
|---|---|---|---|---|---|---|
| base | single | 12.2／12.4 | 58.5／58.8 | 無 | 90 | 否 |
| 09a | single | 0.11／0.09 | 70.7／70.9 | 無 | 108 | 是（t_c 後 529 ms） |
| 09a-b2 | single | 0.10／0.11 | 45.8（混入撤回）／70.6 | 181 ms，+30.2／+30.3，殘餘 −3.2／−3.0 | 73 | 否 |
| base | chain | 10.7／10.9 | 玩家 1：59.9；玩家 2 串接（−33.3 再 −40.0）：93.0 | 無 | 90／105 | 玩家 2（494 ms） |
| 09a | chain | 3.4／0.09 | 玩家 1 串接：90.8；玩家 2：70.7 | 無 | 105／107 | 兩位（447／563 ms） |
| 09a-b2 | chain | 0.12／0.12 | 57.1（混入撤回）／70.4 | 169 ms，+13.4（部分），殘餘 −19.9 | 92 | 否 |

- 三棵 tree 都照預測的方向行為：base 不越線（W_post 12 ms），09a 的 W_post 歸零、越線並重設，09a-b2 撤回、合計 73。
- **用第②步的 `measure.py` 看 09a-b2 會得到錯的結論**（`dev/measure2-dryrun.jsonl`）：撤回的正向 slew（22.2 ms）被當成 >20 ms 的產生空檔，跑次不乾淨；偵測器的修正量混進撤回（−22.2 ms），事件不合格；撤回本身又被 `other_phase` 設限。也就是說，第②步的定義在結構上不可能讓 B2 的事件合格。這是第 7 節追加產生空檔 v3、修正量 v3 與設限 v3 的理由，三棵 tree 用同一套定義。審查的發現 2（`fix/` 的 declined）指的就是這件事。
- chain 的 09a-b2：重新擷取在延遲還沒結束時（release 約在開始後 222 ms，撤回決定在 t_c 後 169 ms）就以 8 個樣本決定，只撤回 +13.4 ms，殘餘 −19.9 ms，領先 57 ms，要等約 4 秒後的視窗修正才回到 37 ms。合計 92，沒有重設；但這段期間 backlog 的餘裕只剩約 10 ms 的領先（66.7−57）。這是 B2 的一個弱點，記錄在第 9 節，不另立判定。
- 撤回之後的下一次修正在 4.09～4.11 秒（+3.1～+3.2 ms 與 +20.0 ms，是把殘餘拉回目標的正常視窗修正，同號），1 秒內沒有修正，沒有來回修正的特徵。
- 注入造成的 held：single 6～7 筆、chain 11～12 筆，三棵 tree 幾乎相同（注入期間命令到不了 Match）；release＋0.1 秒之後 0 筆。
- 用第②步 session 的 8 個跑次（a954aa3，當成 base）以 `measure3.py` 乾跑：16 個合格事件，lead_after、W_post、重設與第②步的 `measure.jsonl` 完全一致，V4X「一致」，帶內 4 個事件（`dev/measure3-dryrun-validate.jsonl`）。只記錄案例的路徑用第 08b 批 dev 的 network20／40（有計數器）、host-ipc／gateway-250ms（timing probe）與 D44 的 M3（S 候選 −32.98 ms，q30 108，backlog 重設）驗證過（`dev/measure3-dryrun-m3.jsonl`）。第 08b 批 dev 的 network20／40 沒有任何 late 修正，所以 J2c 的抖動部分在本 session 很可能是「無法判定」。
- 草稿的 `measure3.py` 對第②步 session 乾跑時，把 r4／r7 重設後的 reseed 首次決定記成 base 的「撤回」（t_c 後 699／702 ms）。修正後這筆消失，其餘欄位不變（`adversarial/compare-validate.txt`；修正版的乾跑 `dev/measure3-dryrun-*-final.jsonl`）。
- 開發跑次共 6 次（每棵 tree 2 次：δ50／100 與 δ80／150 各 1 次），超過任務文字「每棵 1 次」的字面。已記錄，不計入判定，由使用者確認。

#### 14. 核准時要決定的事與未決事項

1. **決定 1：Match 與 Gateway 的來源**（第 2 節；2026-10-10 核准：照建議）。建議三棵 tree 共用 base 的 Match 與 Gateway，只有 probe 不同；替代案是各用自己的，以 P1' 形式比較 Gateway、以原始碼差異與符號論證 Match。
2. **決定 2：跑次**（2026-10-10 核准：照建議，84 個項目）。建議 84 個項目（single 10、chain 6、只記錄 4 案例×3，每棵 tree），約 45 分鐘。若要縮短，可把只記錄的案例只排在第 1～8 輪（每個案例每棵 2 次，共 72 個項目），約 37 分鐘；single 不建議少於 10 次（J1 的檢定力）。
3. 本宣告、`session3.py` 與 `measure3.py` 的凍結（核准時記錄 sha256 並複製到證據目錄），以及 D45 對抗式檢查的修正（C1～C7、R1～R13，已套用）。其中要使用者確認的兩點：
   - **J2d 改為絕對門檻**：每個 B2 J2 事件 `held_post`＝0（取代草稿的「B2 ≤ 09a」）。
   - **開發跑次每棵 2 次**（共 6 次），超過「每棵 1 次」的字面；不計入判定。
4. **抖動下的來回修正大概率無法判定**：B2 的重新擷取只在 late 修正之後才啟動，而 network20／40 在閒置主機幾乎沒有 late 修正（第 08b 批 dev 0 次）。要真正檢驗「抖動下不來回修正」，需要「抖動＋注入」的組合案例；runner 目前不支援（要改 acceptance 工具或另做 (c)），不在本步範圍，另外決定。
5. **chain 的部分撤回**（第 13 節）：B2 在延遲未結束時就決定，殘餘領先可能維持約 4 秒。它不影響本步的判定（J3 只看合計 <105），但會是 B2 正式實作的設計輸入（例如重新擷取的樣本要在 release 之後才收）。
6. 第②步的模型偏差（+0.04～+0.65 ms）與 relay 的送出晚醒：本步一律用實測值，不改模型。
7. 原型的程式不在本步修改。若 session 發現原型本身的錯誤（例如 09a 的 P1 不符合），回報，不修改後重跑。
8. 主 checkout 的 runner 只服務 D48（第②步宣告第 14 節第 3 點）；第③步結束後是否保留，屆時決定。

## 09R-map 的事前宣告（2026-10-11 使用者核准，D64；依 D45 做了 1 次門檻來源的對抗式檢查（xhigh），修正已併入）

09R-map：在修好資訊流的樹上，用 Client→Gateway 的短延遲格點量 late 修正、領先與重設，回答「09a 能不能單獨先上線」與「B 怎麼重新規劃」需要的資料。草稿原檔 `build/target/_build/test/logs/pvp-v7-09r-map-prep-20261011/declaration-draft.md`（sha256 `23ae410d…`），`run_map.py` `cc8be0ed…`、`measure_map.py` `f263cb2d…`、`sleeper.py` `98105dbf…`、凍結清單 `frozen-map.sha256` `908f2a5a…`（47 檔）。第 14 節的 7 點全部照建議核准。下文的 `prep/` 指那個目錄。

09R-map 的意思是：在修好資訊流（08c）的樹上，用 Client→Gateway 的短延遲（45～80 ms、持續 100～300 ms）做格點量測，取得「09a（Client 送命令的節奏）能不能單獨先上線」與「B（缺陷 B：late 修正串接；B2：修正後重新擷取）要怎麼重新規劃」需要的資料。

- 狀態：**草稿・修正輪**（2026-10-11）。
  - 第 1 版：18:34～18:58，1 位 high；保留在 `prep/scratch/declaration-draft.v1.md`（腳本 `prep/scratch/run_map.v1.py`、`measure_map.v1.py`）。
  - D45 的門檻來源對抗式檢查：1 位 xhigh，`prep/xhigh-check.md`。
  - 本版（19:16 起）依該檢查的「必須在送核准前修正」1～7 與「順手修」各項修正；對應表在第 16 節。第 14 節的新預設都標「建議，待使用者確認」。
  - 下一步：主對話整合後送使用者核准。
- 依據的頭：主 checkout `claude/pvp-v7-p2` 的 `6fb18f2`（乾淨）。工具都以這個 commit 的版本為準（記為 MAIN_BASE）。宣告提交後的 commit 記為 **H**；`MAIN_BASE..H` 只能改 `docs/`，主 checkout 從 H 到 session 結束不得改動（driver 要求 `--main-head H`，每個跑次之前核對）。
- 依據的決定：D44（偶發重設的嚴重度）、D45（宣告前的門檻來源對抗式檢查）、D48（原型對照的做法；「推論為真時會失敗、碰不到脆弱窗就判無法判定」）、D50（b1：在 runner 自己的行程裡替換，不改凍結檔）、D58（記錄 Gateway 拿不到內容的丟棄）、D60（注入工具的無效門檻、凍結清單）、D61（08c L2 的格式）、D62、D63（先做 09R-map；late 修正改用凍結偵測器的增量）。
- 大綱：09R-eval 回報 `pvp-v7-09r-eval-20261011/report-from-agent.md` 第 5 節「09R-map 宣告的大綱」。和大綱不同的地方在第 13 節。
- 本草稿的工具、開發跑次與數字都在 `build/target/_build/test/logs/pvp-v7-09r-map-prep-20261011/`（以下簡稱 `prep/`）。指令見 `prep/commands.txt`，雜湊見 `prep/prep-files.sha256`。
- 先例：第 08c 批「L2 的事前宣告」（`08c-drop-report.md`；`pvp-v7-08c-l2-prep-20261011/`）；D48 第②步、第③步的宣告與 `measure.py`、`measure3.py`。
- 數字沒有標「實測」的，是程式推導或推論。

### 1. 目的

只取得兩個決定需要的資料，**不判定 09a／09b 的好壞**：

1. **09a 的時機**（U6：09a 和 B 的實作順序）：上行路徑（Client→Gateway）的短延遲，在 flow 與 flow+09a 上，各會產生多少單次的 tracking 型 late 修正、修正後的 Match 端領先多少、會不會走到 backlog 重設。
2. **B 的重新規劃**（09b-plan：B0 或 B2）：在哪些（延遲、持續時間）組合下，late 修正會串接（缺陷 B）；串接時重設的情況；B2 原型在同樣的刺激下撤回多少、會不會來回修正（只記錄）。

判定 Jm1～Jm3 只確認「D48 的模型可以搬到上行路徑」，讓上面的資料能用同一套模型讀。Jm2 的結果本身就是第 1 點的答案，不是通過條件。

不在本批範圍：權威、wire、FireGate 常數、late 修正上限、target、lead、backlog 門檻都不改；不做 09a／09b 的實作；不量 network 型損傷（network 案例的遺失是固定種子，09R-eval 已記錄，不適用於本批）。

### 2. 三棵樹

| 樹 | worktree | commit | 內容 | 角色 |
|---|---|---|---|---|
| flow | `/Users/karasu/Code/Source/GYO-Engine-l2-flow` | `eabb107e610a0423aa7c6903e37a9e3be424524b` | 08c（修好資訊流） | 判定 |
| flow+09a | `/Users/karasu/Code/Source/GYO-Engine-l2-flow09a` | `cb921e013a51205965fd3779aac78088b7049d1b` | `eabb107`＋09a 原型（`git diff 8fabfa6 e6885b7`，L2P-trees） | 判定 |
| flow+09a+B2 | `/Users/karasu/Code/Source/GYO-Engine-l2-flow09a-b2` | `0e1fabb33e8a2a15db757ba2e6f130ad17ddab2e` | `cb921e0`＋B2 原型相對 09a 原型的差分（`git diff e6885b7 c2705be`） | **只記錄** |

- flow、flow+09a 沿用 08c L2 的 worktree 與建置（L2P-trees；`pvp-v7-08c-impl-20261011/l2p-trees/artifacts.sha256`），本批不修改。
- flow+09a+B2 是本批新建（`prep/tree/`）：
  - `git worktree add --detach ../GYO-Engine-l2-flow09a-b2 cb921e0`；差分存為 `prep/tree/b2-e6885b7-c2705be.diff`（sha256 `2551c0ec…`），`git apply --index` 無衝突：4 個檔在 `e6885b7` 與 `cb921e0` 的 blob 相同，套用後 4 個檔的 blob 和 `c2705be` 逐一相同，`git diff cb921e0 0e1fabb` 的 sha256 等於差分檔。
  - 本地 commit `0e1fabb`，本地分支 `l2-flow09a-b2-0e1fabb`，不推送。
  - 差分的 4 個檔：`LocalPlayerPrediction.{hpp,cpp}`（Client 的相位追蹤）、`Movement.hpp`（只改註解）、`PredictionTests.cpp`（測試）。Match 與 Gateway 不讀這些檔（D48 第③步：Match 執行檔裡沒有 `LocalPlayerPrediction` 的符號）。
  - 乾淨建置：`prep/tree/build-trees.sh`（`l2p-trees/build-trees.sh` 的複本，只改輸出目錄），Match、Gateway、action probe、timing／network／worker probe，18:36～18:51（14 分），錯誤 0（`prep/tree/build-flow09a-b2.log`）。Gateway 的 `vcs.revision=0e1fabb…`、`vcs.modified=false`（`prep/tree/gateway-vcs.txt`）。worktree 建置後仍乾淨。CTest 本批沒有跑（B2 原型的 L1 在 D48 第③步 `fix/ctest-09a-b2.log` 已通過；本批只用它的 probe）。
- 執行檔 sha256（完整值寫在 `run_map.py` 的 `ARTIFACTS`；flow+09a+B2 的完整清單在 `prep/tree/artifacts.sha256`；driver 每個跑次之前核對）：

| 樹 | Match | Gateway | action probe |
|---|---|---|---|
| flow | `8d154a3a…` | `4f0caaca…` | `4949302f…` |
| flow+09a | `b83f3548…` | `e5041519…` | `adf1d13f…` |
| flow+09a+B2 | `2f6dc26c…` | `277d50eb…` | `76b0d154…` |

- 每棵樹用自己的 Match、Gateway 與 probe（和 08c L2 相同）。三棵樹的 Match 與 Gateway 程式相同（差分只在 Client），雜湊不同只來自建置路徑與 Gateway 的 `vcs.revision`。本批不重跑權威 digest（08c L2 的 J1 已證明 flow 與 flow+09a 的 Match 等價；B2 的差分不碰 Match）。
- arena：主 checkout 的 `assets/object_fps_pvp/pvp_arena.json`，`0026013c731a7d7317ce6be4f84f897b91fa1b807c9621d60fb608570f1775a5`。

### 3. 工具與凍結清單

- 所有 runner 與分析器都從主 checkout 執行；三棵樹用同一份工具。
- **凍結清單 `prep/frozen-map.sha256`**：47 檔，清單本身的 sha256 `908f2a5a68f64c542520cf3c2c2b5fa7d7f5f326c02161c6db6aa42acb635c7f`。內容＝08c L2 的 44 檔（`pvp-v7-08c-l2-prep-20261011/frozen-l2.sha256`，`d2a8699b…`）＋下面 3 個既有的凍結量測腳本：

  | 檔 | sha256 | 用途 |
  |---|---|---|
  | `pvp-v7-d48-step1-20261010/detector.py` | `445271ced691b25e1c87a38bb70ea9b3754d32b741a1b0331f1e00c0578194cd` | late 修正的 P 事件（計數器增量） |
  | `pvp-v7-d48-step2-20261010/measure.py` | `045714f12dd5020d88b4d34e0df07d85a6e6607d17651f76ee08263fc6fadf14` | W_post、修正前後的領先、模型差、排隊重播、重設、設限 |
  | `pvp-v7-d48-step3-20261010/measure3.py` | `18ffde156b59bf822f0fcc15a50d7d6f2a57745daa2d2ba92694850845278d79` | v3 的產生空檔規則、撤回、來回修正、串接單位 |

  （路徑在 `build/target/_build/test/logs/` 之下。）
- 核對時點：2026-10-11 在 `6fb18f2` 上 `shasum -a 256 -c` 47／47 OK；session 開頭不符就拒絕；每個跑次之前核對；結尾記在 `summary.json` 的 `frozen_ok_end`。
- `run_client_delay.py` 以 `--frozen-list prep/frozen-map.sha256` 執行，自己再核對清單中的每個檔（`run_client_delay.py:409-427`）。`run_gameplay.py` 由 driver 核對。`run_network.py` 不在清單內（同 08c L2），只受 git 狀態核對保護。
- **本批新寫、核准時一起凍結的檔**（雜湊在 `prep/prep-files.sha256`；修正輪的版本：`run_map.py` `cc8be0eda4e69023f3e0f6fa504f6936191919f7706258b9ba35cff45a278589`（＝`measure_map.DRIVER_SHA`）、`measure_map.py` `f263cb2d13d78d05f58f41a2aec94e8d3a0d7940d8f08333a406ac436582dc69`、`sleeper.py` `98105dbf…`；第 1 版是 `d643fced…`／`39221640…`。核准前再修改時，先重算 `run_map.py` 的 sha、寫進 `DRIVER_SHA`，再重算 `measure_map.py`）：
  - `run_map.py`：session 驅動。
  - `measure_map.py`：量測與判定。**量測路徑上唯一的新程式是一個轉接**（見下）；其餘的量都由上面 3 個凍結腳本算出。
  - `sleeper.py`：照抄 08c L2 的 `sleeper.py`（`98105dbf…`）。
- **轉接（adapter）**：凍結的 `measure.py` 從 `result.json` 讀故障欄位，而且要求 `delayed_chunks`（它原本的 Gateway→Match runner 的欄位）。`run_client_delay.py` 寫的故障欄位是 `start_ns`、`release_ns`、`delay_ms`、`window_ms`，延遲的資料包數記在 `relay.json` 的 `client_delay.delayed`。`measure_map.py` 在自己的行程裡把 `measure.py` 的模組函式 `load_json` 換成包裝：只對故障機制是 `client-gateway-pure-delay` 的 `result.json` 補上 `delayed_chunks`＝`client_delay.delayed`，其餘原樣傳回。檔案不改（D50 b1 的作法）。`measure.DECLARED` 由 `measure3.analyze_inject` 在每次呼叫時設成該格的延遲與持續時間（和 D48 第③步相同）。
- **判定腳本核對 session 是不是宣告的設計**（xhigh M3）：`measure_map.py --session` 要求 `plan.json` 的 `declared` 等於 `measure_map.DECLARED`、`schedule` 等於它自己重算的 `schedule()`、`plan.state.driver` 等於寫死的 `DRIVER_SHA`（核准版 `run_map.py` 的 sha256）、`plan.state.judge` 等於它自己的 sha256（每個跑次的 `evaluate` 和最後的彙整是同一個版本）。任一不符，`plan_check` 不成立、`passed` 為 false。排程與宣告值只定義在 `measure_map.py`，`run_map.py` 直接使用（同一件事只維護一處）。
- driver 的核對：正式執行強制 `--output`、`--idle-gate`、`--main-head`、`--judge-sha`、`--driver-sha`，和實際檔案不符就拒絕；`sleeper.py` 的 sha256 寫死；三者與 3 個凍結量測腳本、`ingress_evidence.py` 都在每個跑次之前的狀態核對之內。

### 4. 主機

- 照 08c L2：session 期間不做開發；以 `caffeinate -dims` 執行；前景沒有其他使用者程式；**暫停 PR #73 的 auto-fix**；背景的其他 agent 與分析先停。
- **閒置閘門**（`--idle-gate`，正式執行必須帶）：第一個項目之前連續 3 次 5 秒 sleeper，每次最大值 <10 ms；不成立就每 30 秒重試，最多 30 分鐘。
- 每輪（前置、第 1～3 輪）前後各記 1 次 5 秒 sleeper 與前 5 名 CPU 程序（`host.jsonl`）。結尾那一次失敗時照樣寫 `summary.json`（記成停止）。
- 子程序：每個都跑在自己的 session（`start_new_session`），終端機的 Ctrl-C 只打到 driver，driver 跑完目前的項目後停下；閒置閘門期間也會在 0.5 秒內回應並停下。
- 逾時 300 秒時先送 SIGTERM：`run_gameplay.py` 會執行清理；`run_client_delay.py` 只把它當成「這一輪跑完再停」（`run_client_delay.py:552-556`），卡住的那一輪不會被清理。兩種情況都在 15 秒後對整個 process group 送 SIGKILL 收拾（Match、Gateway 由 runner 直接啟動，在同一個 group）。每個項目結束後，group 裡留下的程序一律殺掉並記錄。
- 項目開始前的 sleeper 或狀態核對失敗時，那一項記為 `not_run`（它沒有跑），session 停下。

### 5. 格點、次數、順序與時間

- **注入**：`run_client_delay.py --profile x1`（固定延遲型），δ∈{45, 60, 80} ms × 視窗∈{100, 150, 300} ms，每格每棵樹 2 次；**核心的單次格 45／100、60／100 每棵樹 3 次**（建議，待使用者確認；xhigh 第 5 節第 6 點：合格事件幾乎只來自這兩格，2 次時 Jm2 的 ≥6 個跑次沒有餘裕）：9×3×2＋2×3＝60 次。
- **前置**：每棵樹 1 次 clean-60（`run_gameplay.py --case clean-60`），在第 1 輪之前。合計 **63 個項目**。
- **起點**：每個注入都在 probe 時鐘的 **12.0 s** 開始（`--fault-at 12.0`，固定偏移）。理由（gameplay 計畫 `gameplay-plan.json` 與 08c L2 的 clean 跑次實測）：
  - **Cooldown 的機制**（D48 第③步的停止原因，`09b-phase-tracking.md:319-334`（行號以 `ad2c1c4` 為準））：被延遲的那一發晚約 7 Tick 才解析，250 ms 後沒有被延遲的下一發照常解析，兩者在權威上只隔 9 Tick，小於 `cooldownTicks`＝10（`Combat.hpp:22`），被拒（rejection＝3）。所以危險的是「被延遲的一發，後面 250 ms 內緊跟著一發沒有被延遲的射擊」。
  - 預排的射擊（`gameplay-plan.json`）：玩家 1 在 10.25～12.0 s 每 0.25 s 一發，12.5 s 換彈，**下一發射擊是 14.25 s**；玩家 2 是 13.1 s。
  - 12.0 s 起點：注入期間（12.0～12.38 s，含放出）只有 12.0 s 那一發可能被延遲，它後面 2.25 s 都沒有射擊；延遲只會拉長它和 11.75 s 那一發的間隔（250＋δ ms）。所以上面的機制在結構上不會發生。
  - 2.0 s 起點（D48 第②③步用過）與 11.0～11.75 s：被延遲的那一發之後 250 ms 都有一發沒被延遲的射擊（2.0～3.25 s、10.25～12.0 s 每 0.25 s 一發），正是上面的機制。在允許的範圍（respawn 之後、≤12.0 s）內，12.0 s 是唯一沒有這個風險的起點。
  - 生命事件：玩家 2 在約 6.75 s 被擊殺、約 9.78 s respawn（`life_respawn` 重設與 `lifecycle_cancelled` 約 9.79 s，08c L2 的 clean-60-r1-flow 與 x1-r2-flow 實測）。之後到 16 s 兩位玩家都沒有生命事件。凍結 `measure.py` 的 `life` 設限看 ［t_c−1 s, t_end），12.0 s 起點讓它落在 ［11.0 s, 15.4 s］，碰不到 respawn。
  - 08c L2 用的 7.5 s 不適用：玩家 2 在 7.5～9.78 s 是死亡狀態，而且 respawn 落在結果視窗（t_c＋3 s）內，會讓沒有重設的事件被 `life` 設限、有重設的事件不受影響，造成偏向重設的偏差。
  - `run_client_delay.py` 的界限：fault_at 1.0～12.0 s（`:85`），注入加放出要在 13 s 之前結束（`:82`、`:155-161`）；12.0＋0.300＋0.080＝12.38 s。結果視窗到 t_c＋3 s ≈ 15.4 s，probe 跑到 16 s（`trace_end` 設限不會觸發）。
- **順序**：
  - 前置：flow、flow+09a、flow+09a+B2。
  - 第 1 輪：9 格依 (45,100)、(60,100)、(80,100)、(45,150)、(60,150)、(80,150)、(45,300)、(60,300)、(80,300)；第 2 輪反序。
  - 第 3 輪：核心格 45／100、60／100 的第 3 次（依 CELLS 的順序）。
  - 每格三棵樹連續跑，起頭的樹輪替：TREES 依（格的序號＋輪次−1）mod 3 旋轉。起頭次數 flow 7、flow+09a 6、flow+09a+B2 7（`run_map.py --list-only`）。
  - 不隨機化，不補跑。
- **獨立單位是跑次**：同一跑次的兩位玩家共用同一個注入與主機狀態（D48 第②步：同跑次兩位玩家的 W_post 相差 ≤0.6 ms）。但固定起點下，跑次之間在相位上也不完全獨立（開發跑次的 token 水位重建 x_before 都在 1.16～1.48；xhigh 第 3 節 H3），比例類的說法只對這個起點與這台主機成立。
- **時間**：開發跑次每項 20～21 秒（實測，`prep/dev/*/timing.txt`），每項的判定（`ingress_evidence.py`＋量測）約 2 秒。63 項約 24 分，加閒置閘門（最少 15 秒）與每輪前後的 sleeper（8×5 秒），**約 27 分**；閒置閘門重試時最多再加 30 分。

### 6. 注入的參數與有效性（D60）

- 所有格都通過 `run_client_delay.py` 的事前檢查（`profile_problems`，`:133-163`）：最大延遲 80 ms×240 包／秒＝19.2 <256；視窗 100～300 ms 在 100～8000 之內；起點與結束時刻如第 5 節。也就是 **9 格都做得到**；開發跑次的實測見第 11 節。
- 每個跑次由 `run_client_delay.py` 自己核對（`round_problems`，`:429-470`），任一不成立就記為 `injection_invalid`，session 停下：
  - 延遲欄位與參數相符、延遲的資料包 ≥1；
  - 注入開始與結束時，兩個方向的收＝送、佇列清空；沒有無法路由的 Gateway 資料包、沒有 relay 錯誤；
  - 佇列深度 <192（`QUEUE_NEAR`，`:78`）；
  - 送出延遲的 p99 ≤10 ms（`LATENESS_P99_MS`，`:80`）；
  - Gateway 的速率拒絕＝0（`rate_limited_packets`，`:467-469`）。
- 注入延遲的是 Client→Gateway 方向的全部資料包（輸入、動作、ACK），Gateway→Client 照收照轉；沒有丟包、不超車（x1 結束時排在延遲包後面一起放出，相當於斜率 −1 的放出）。
- `run_case` 的遊戲判定只記錄（D60）。

### 7. 量的定義（摘要；完整定義在 `measure_map.py`、`measure.py`、`measure3.py` 的 docstring）

- **late 修正**：凍結偵測器的 P 事件＝`action-frames.jsonl` 裡 `phase_late_corrections_{a,b}` 的每一次**增量**（不用最大值；計數器在 epoch 重設時歸零，歸零後的值照算，D63 ④、09R-eval「J5b 少算」）。每個跑次記全部與注入後 1 秒內（`in_span`）的增量總和。
- **事件的量**（`measure.py`）：t_c、W_post（slew 結束後 0.25 s 內產生的命令的送出等待中位數，至少 8 個）、lead_before（t_c 前 1 s 的 Match 端領先中位數）、lead_after（slew 結束後 0.25 s 內產生的命令的 Match 端領先中位數，至少 8 個）、lead_pred＝lead_before＋33.333−W_post、model_diff＝lead_after−lead_pred、30 Tick 合計的重播與最大值（q30_max）、［t_c, t_c＋3 s］內的 backlog 重設與其他非 respawn 重設、設限。Match 端領先＝`resolved`（actual）−`host_accepted`。
- **v3 的追加**（`measure3.py`）：產生空檔的 v3 規則（正向 slew 的間隔不算主機停頓，B2 的撤回因此不會讓跑次不乾淨）、size_v3、撤回（late 修正後 1 秒內、下一次非 respawn 重設之前，`phase_corrections` 增加而 `phase_late_corrections` 不變的幀）、撤回量與殘餘、來回修正的特徵、lead_after_max、lead_settled。
- **合格事件 E(樹)**（`eligible_v3`，Jm1～Jm3 的母體）：
  - 跑次乾淨（v3）：延遲欄位與參數相符、有相位計數器、注入視窗內沒有 >20 ms 的產生空檔（v3 規則）、沒有 seeded、沒有 Client／Match 的 runtime_gap，而且**注入在 slew 結束之前已放出**（`release_before_slew_end`）；
  - 事件在注入後 1 秒內、不是重設後的事件；tracking、增量 1、沒有串接、|size_v3＋33.333| ≤1.0 ms；沒有設限（v3）；W_post 與 lead_after 算得出來。
  - 「放出在 slew 結束之前」這一條讓視窗 300 ms 的全部格在結構上沒有合格事件：放出之前 Match 收到的命令仍帶著注入的延遲，lead_after 量到的不是修正後的穩態（凍結 `measure.py` 的 V1 與 `fault_boundary` 設限也是同樣的理由）。**δ 80／視窗 150 沒有合格事件的原因是串接**，不是放出條件（開發跑次 flow 與 B2 在這一格都是放出早於 slew 結束，`dev-summary.txt`；第 1 版的寫法是錯的）。這些格只提供只記錄的串接資料。
  - **放出到 slew 結束的餘裕和第②步不同**：第②步的放出都在 slew 結束前 64～93 ms（`09b-phase-tracking.md:307`）。本批開發跑次：45／100 是 58～70 ms，60／150 只有 17 ms；推算 45／150 約 7～20 ms（xhigh 第 3 節 M1）。小餘裕的區間是第②步沒有驗證過的；每個事件的 `t_post` 與 `release_to_t_post_ms` 都記下來，供事後核對。
  - **沒有合格、但可能藏住違反的事件**（xhigh M4）：有重設的事件只看到重設為止（約 0.5 s），沒有重設的要看滿 3 s，所以「越線卻沒重設」的事件比一致的事件更容易被設限排除。注入後 1 秒內、qualifies_v3、算得出 lead_after、但被設限或所在跑次不乾淨的事件，只要在帶外，就列成使用者判斷旗標（`not_eligible_outside_band_flags`，比照第②步的 `censored_crossing`）。
- **串接單位**（`measure3.py` 的 chain 單位）：每位玩家在注入後 1 秒內的第一個 tracking 型 P 事件；是否串接（增量 >1，或 0.5 秒內同一玩家又有 P 事件）、後續 late 修正數、size_v3 合計、q30_max、backlog 重設、撤回、來回修正。這裡不要求注入在 slew 結束前放出。

### 8. 判定（plan 核對、session 完整、Jm1 成立、Jm3 沒有被推翻，才算「這張圖可以用模型讀」；Jm2 是答案，不是通過條件）

由凍結的 `measure_map.py --session` 機械地給出（`judgement.json`）。

| 判定 | 內容 | 成立 | 不成立／推翻 | 其餘 |
|---|---|---|---|---|
| plan 核對 | `declared`、重算的 schedule、driver sha、judge sha 都等於宣告值（第 3 節） | 全部相等 | — | 不相等（`passed` 為 false） |
| session | 排程的 63 項全部完成；沒有中斷、沒有停下；凍結清單開頭與結尾都 OK；結尾的狀態等於開頭 | 全部滿足 | — | 不完整（`passed` 為 false） |
| **Jm1**（模型能不能搬到上行路徑） | E(flow)＋E(flow+09a) 的 \|model_diff\| ≤1.0 ms | 合計 ≥12 個事件、≥6 個跑次，**而且每棵判定樹各自 ≥6 個事件、≥3 個跑次**，全部 ≤1.0 | 任一 >1.0 | 無法判定 |
| **Jm2**（flow+09a：單次 tracking 修正越過脆弱窗並重設） | E(flow+09a) 的 lead_after 對 66.667±1.0 ms，加上 ［t_c, t_c＋3 s］ 的 backlog 重設 | 沒有違反，而且 ≥6 個跑次有「lead_after >67.667 而且重設」的事件 | 違反＝lead_after <65.667，或 >67.667 而沒有重設 | 無法判定（帶內的事件列出、不檢定） |
| **Jm3**（flow：重設 ⇔ 領先 >66.667 ms） | E(flow) 在帶外的事件，**分兩側**：越線側（>67.667，必須重設）、不越線側（<65.667，必須不重設） | 沒有推翻、帶外 ≥8 個事件，而且**兩側都有事件**：「一致」；只有一側：「越線側一致（不越線側無法判定）」或反過來 | 帶外的事件和規則相反（「推翻」） | 帶外不到 8 個：無法判定 |
| passed | plan 核對、session、Jm1 成立、Jm3 沒有被推翻、沒有任何停止旗標 | | | |

- **讀法**（只供 09a-時機與 09b-plan 參考；決定由使用者做）：
  - Jm1 成立、Jm2 成立：在本起點、本主機、本格點的條件下，flow+09a 上的上行短延遲單次 late 修正**沒有例外地**走到 backlog 重設，和 D48 第③步的 IPC 延遲記錄值一致。這不是「09a 每次都會重設」的證明（Jm2 的統計強度見門檻表）。
  - **四點限定**（xhigh M2）：
    1. **flow 的基準頻率不能用本批的比例**：12.0 s 起點下 flow 的 W_post 集中在 2.5～4.7 ms（開發跑次），落在帶的上緣附近，flow 的重設比例偏高，拿「flow 對 flow+09a 的重設比例」讀 09a 的增量會低估。flow 的基準頻率參考 D48 第③步 base 的 2／10（W_post 1.6～15.8 ms；stopped session 的記錄值）。
    2. **串接的格在三棵樹都重設**（開發跑次 9 個串接單位全部重設，含 B2），不能用來區分 09a；它們是 B 的資料。
    3. **格點只表示條件暴露**：δ 與視窗是人工設定的，推不出實際網路下單次 late 修正的頻率；只能說「在這個延遲與持續時間下會發生什麼」。
    4. **Jm1 只檢驗放出之後的穩態**：合格事件都是放出早於 slew 結束的事件；延遲持續期間（長視窗）的領先不在 Jm1 的範圍內。
  - 09a 單獨上線時暴露的大小，由「哪些格會產生單次 late 修正」決定（第 10 節的圖），讀的時候套上面四點。
  - Jm2 不成立：09a 在上行路徑上沒有把 W_post 壓到 0，或模型的邊界在上行路徑上不同；列出違反的事件，停下交使用者判斷。
  - Jm1 不成立或 Jm3 被推翻：模型不能直接搬到上行路徑，第 10 節的圖只能當描述，不能用 D48 的推論鏈讀。
  - 無法判定不視為支持，也不視為反證；不補跑。

#### 判定門檻與來源（D45 的核對對象）

| 門檻 | 值 | 來源（建置、指標、範圍） |
|---|---|---|
| 模型容許（Jm1） | \|model_diff\| ≤1.0 ms | **來源是 D48 第②步的宣告**（`pvp-v7-d48-step2-20261010/declaration.md:190`：試跑 10 個事件最大 0.312 ms，約 3 倍餘裕；`a954aa3`，Gateway→Match IPC 延遲 δ 50／視窗 100）；常數 `measure.py:114` `MODEL_TOL`。之後的實測：V3 16 個事件、8 個跑次，最大 **0.645**、中位 0.256 ms（`measure.jsonl:25`），餘裕降到約 1.55 倍。指標：Match `resolved`−`host_accepted` 的中位數，只算 actual 的移動命令；W_post 是 Client 端的等待，兩條路徑相同。注意：第②步的放出到 slew 結束有 64～93 ms 的餘裕，本批的部分格只有約 8～20 ms（第 7 節），是第②步沒驗證過的區間 |
| Jm1 最少事件與跑次 | 合計 12 個、6 個跑次；每棵判定樹各 6 個、3 個跑次 | 合計值沿用 V3 的 `V3_MIN_EVENTS`、`V3_MIN_RUNS`（`measure.py:122-123`；第②步為「8 個跑次×2 位玩家」設計的提議值，不是推導）。每棵樹的最少數是本版新增（xhigh H1）：flow+09a 的 W_post≈0.1 ms，模型的「−W_post」那一項在它身上等於沒有被檢驗，所以 flow 必須自己達到最少數。可達性（推論，依開發跑次）：合格事件只來自單次修正、而且放出在 slew 結束前的格；45／100、60／100 每棵樹各 3 次＝6 個跑次、約 12 個事件，45／150、60／150 是邊界 |
| 脆弱窗的門檻（Jm2、Jm3） | 4T＝66.667 ms | 每 Tick 排隊數 ≈ floor(領先÷16.667 ms)；領先 >4 Tick 時每 Tick 排 4 個，30 Tick 合計 120 ≥105（`Movement.hpp:38-39` `MovementBacklogSampleTicks`＝30、`MovementBacklogCommandSum`＝105，行號以 flow 樹與主機 checkout 為準，flow+09a 樹是 `:51-52`；`PvpMatch.cpp:452-459`，三棵樹相同）；`measure.py:112` `THRESHOLD`。這是 Match 端的量，和注入路徑無關 |
| 帶寬（Jm2、Jm3） | ±1.0 ms | D48 第②步 V4 的帶寬（`measure.py:113` `BAND`）：試跑的逐命令領先 P10～P90 寬度 ≤0.63 ms；第②步的帶內 4 個事件（66.8～67.4 ms）全部重設（`d48-step2/declaration.md:193`、`measure.jsonl:25`）；第③步帶內 3 個事件（66.1、66.2 ms 不重設，67.46 ms 重設）是 **stopped session 的記錄值**（`pvp-v7-d48-step3-20261010/measure.jsonl:203` V4X；`09b-phase-tracking.md:319-335`），不是判定 |
| Jm2 的預期與來源 | 09a 的單次事件越線並重設 | D48 第③步 J1 的**記錄值**：09a 10／10 個跑次重設，W_post 最大 0.156 ms，領先 70.35～70.76 ms，30 Tick 合計 105～108（`pvp-v7-d48-step3-20261010/measure.jsonl:203`）。那一行的 `step3` 是 **stopped**（chain 跑次出現 Cooldown），所以 J1 只是計算出來的記錄值，不是判定。那是 Gateway→Match IPC 延遲；本批檢驗上行路徑 |
| Jm2 最少跑次 | 6 | 提議。單臂、零失敗：(1−p)^6＝0.05 → p＝0.39；6 個跑次在 95% 單側下排除「每跑次失敗機率 >39%」。這個算法要求跑次獨立，而固定起點下跑次在相位上不完全獨立（第 5 節），所以「排除 >39%」**只對本起點、本主機成立**；Jm2 成立的意思是「本條件下沒有例外」。可達性（xhigh M1，依開發跑次的時刻重寫）：t_post−start 在 δ45 約 204～208 ms、δ60 約 222～227 ms、δ80 約 238～244 ms；放出約在 視窗＋δ−7～＋2 ms。所以 45／100、60／100 是合格的主要來源（各 3 次＝6 個跑次），45／150 屬邊界（餘裕約 7～20 ms），60／150 一半串接，80／100 推論串接。flow+09a 的合格跑次**約 6～8 個**，剛好在門檻上，沒有餘裕：一個跑次不乾淨就會變成無法判定 |
| Jm3 的規則來源 | 重設 ⇔ lead_after >66.667 ms，兩側分開判 | D48 第②步 V4 只驗了**不越線那一側**（below 12、crossing 0，`d48-step2/measure.jsonl:25`）；越線那一側的證據只有第③步 V4X 的記錄值（stopped session）與開發跑次。所以本批要求兩側都有事件才叫「一致」（xhigh H2；比照 `measure.py:503-506` 的兩側判定） |
| Jm3 最少帶外事件 | 8 | 提議。**可達性（xhigh H3，依開發跑次重寫；第 1 版「W_post 在 0～16.7 ms 均勻」借自別的設定與未驗證的假說 `09b-phase-tracking.md:51`，撤回）**：12.0 s 起點下 flow 的 W_post 實測是 2.48、2.50、2.68、2.82、4.38、4.73 ms（token 水位 x_before 1.16～1.48，經驗擬合的 W_hat 約 2.1～7.5 ms），換算 lead_after 約 63～68.5 ms，剛好橫跨帶的上緣（W_post 在 2.93～4.93 ms 時落在帶內）。開發的 6 個值裡 2 個在帶內，另外 4 個只比帶的上緣高 0.2～0.35 ms。所以 **Jm3 很可能「無法判定」，或只有越線那一側**；不越線那一側可能接近 0 個事件。flow 的合格跑次約 6～8 個、12～16 個事件，帶外約一半 |
| late 修正的幅度 | −2T＝−33.333 ms，容許 1.0 ms | `LocalPlayerPrediction.cpp:98` 的夾限（flow 與 flow+09a 樹；B2 樹是 `:100`）；`Movement.hpp:59` `MovementPhaseMaximumCorrectionSeconds`＝2 Tick（flow 樹；flow+09a 樹是 `:72`）；`measure.py:115-116` |
| 注入無效（停止） | 佇列 ≥192、送出延遲 p99 >10 ms、速率拒絕 >0、收發不等 | 工具門檻（D60；`run_client_delay.py:78-80`、`:429-470`）。p99 10 ms 與佇列 192 是 L2P-injector 的提議，不是推導 |
| Cooldown（clean 前置停止） | rejection＝3 出現 1 次 | `Combat.hpp:22`（`cooldownTicks`＝10）、`:52`（`ShotRejection::Cooldown`）；08c L2 的 clean-60 在修好的樹上是 0（09R-eval R8）；和 08c L2 一樣只對 clean 停 |
| ingress `failed`（修好的兩棵樹停止） | `ingress_evidence.py` 的 status＝failed | 建議，待使用者確認（第 14 節第 4 點）。D61 的前例在修好的樹上停止；本批的前提是「修好資訊流」，而上行短延遲是 08c L2 沒驗證過的刺激。`undetermined` 只記錄 |

### 9. 預測（可被推翻；只記錄，不是判定）

依開發跑次（第 11 節，每格最多 1 次）與機制推論，**全部是推論**：

| 格（δ／視窗） | 預測 | 依據 |
|---|---|---|
| 45／100、60／100（45／150 是邊界） | 每位玩家 1 次單次的 tracking 型 late 修正（−33.3 ms），放出在 slew 結束前，是合格事件；45／150 的放出餘裕只有約 7～20 ms | 開發跑次 45／100 三棵樹 6／6 位玩家單次；晚約 7.7～22.7 ms <33.3 ms |
| 60／150、80／100 | 邊界：單次與串接混在一起 | 開發跑次 60／150（flow+09a）1 位單次、1 位串接（第二次 −40 ms，注入後 249 ms） |
| 80／150、45／300、60／300、80／300 | 串接（δ 45／300 推論單次，但放出在 slew 之後，不合格）；80／150 不合格的原因是串接 | 開發跑次 80／150（flow、B2）、60／300（flow）、80／300（flow+09a）全部串接；串接的機制：slew 期間仍收到 late 樣本，slew 結束後立刻再修正（`LocalPlayerPrediction.cpp:114-116` 的註解、`:128`、`:134-135`） |
| flow 的單次事件 | W_post 集中在約 2～7.5 ms（本起點的 token 相位），lead_after 約 63～68.5 ms，橫跨帶的上緣；重設 ⇔ lead_after >66.667 | 開發跑次 45／100：W_post 2.68、2.82 ms，lead_after 68.00、67.95 ms，兩位都重設；W_post 實測全部 2.48～4.73 ms（第 8 節 Jm3 的可達性） |
| flow+09a 的單次事件 | W_post ≤1 ms，lead_after 約 70.5～70.8 ms，全部重設（約 t_c＋0.5 s） | D48 第③步 J1；開發跑次 3 個事件：W_post 0.08～0.11，lead_after 70.68～70.71，全部重設 |
| 串接（所有樹） | 領先 100 ms 以上，全部重設（不能區分 09a） | 開發跑次所有串接單位（9 個）都重設，lead_after_max 99.7～123.5 ms |
| B2 的單次事件（只記錄） | 約 t_c＋150 ms 撤回約 +30 ms，q30 約 69，不重設 | 開發跑次 45／100：撤回 +29.9／+29.8 ms，殘餘 −3.4／−3.5 ms，q30 69，不重設 |

推論的依據：
- late 樣本要命令晚於它的 Tick 被替代，也就是單程延遲超過目標餘裕 37.3 ms（`InitialCommandLead`＝2 Tick＋`MovementPhaseTargetSeconds`＝4 ms，`Movement.hpp:36`、`:45`）；要連續 2 次發布帶負樣本才修正（`Movement.hpp:58`、`LocalPlayerPrediction.cpp:128`、`:134-135`）。
- δ＝45 ms：晚約 7.7 ms；δ＝60：約 22.7 ms；δ＝80：約 42.7 ms。一次修正只移 33.3 ms，所以 δ＝80 在延遲持續到修正之後時，推論會再修一次（串接）；δ＝45、60 推論只修一次。
- x1 結束時以斜率 −1 放出：視窗結束後 δ−37.3 ms 內送出的命令仍會晚到。

### 10. 記錄項（只記錄）

- **圖**：每個（樹、格）：跑次數、每跑次 late 修正的增量（全部與注入後 1 秒內）、串接單位的比例（B 隨視窗的變化）、backlog 重設的單位數、注入後 3.5 s 內非 respawn 重設的原因分布、size_v3 合計、q30_max、lead_after_max、合格事件的 W_post、lead_after、model_diff。
- **B2 樹**：撤回的有無、撤回量、殘餘、來回修正的跑次；它的合格事件也照列，但不進 Jm1～Jm3（lead_after 混入撤回）。
- **D58：Gateway 拿不到內容的丟棄**：每個跑次的總數與依原因（速率限制、解碼失敗、未授權、舊序號的 Actions／其他、未知種類、非 active），由凍結的 `ingress_evidence.py`（`gateway_contentless_drops`）給出。
- **沒有合格、但在帶外的候選事件**（第 7 節，`not_eligible_outside_band_flags`）：逐筆列出交使用者判斷。
- 每個事件的 `t_post` 與放出到 slew 結束的餘裕（`release_to_t_post_ms`）。
- 每個跑次的 `ingress_evidence.py` 狀態（`undetermined` 只記錄；`failed` 見第 12 節）、權威 Cooldown 拒絕數、`run_case` 的判定、注入的實測（延遲的資料包、佇列最大、送出延遲分布、放出時刻）。
- **D44 的觀察旗標**：注入跑次在 ［start, release＋3 s］ 之外、或 clean 前置跑次的任何非 respawn 重設，逐筆列出交使用者判斷（D44 的累計）。在 ［start, release＋3 s］ 之內的 D44 型 backlog 重設，會被當成注入的結果（低機率；xhigh 第 5 節第 5 點）。
- 主機：閒置閘門、每輪前後的 sleeper 與 CPU。

### 11. 開發跑次（只記錄；不計入判定）

10 次（每棵判定樹 3 格各 1 次；B2 樹 clean＋3 格），2026-10-11 18:52～18:56，主機沒有閒置閘門。腳本 `prep/dev.sh`，輸出 `prep/dev/<樹>/`，逐事件的摘要 `prep/dev-summary.txt`（`prep/summarize_dev.py`）。

| 樹 | 格 | 注入（延遲包、佇列最大、送出延遲 p99、速率拒絕、放出） | late 修正（增量，兩位合計） | 單次／串接 | 合格事件 | 結果 |
|---|---|---|---|---|---|---|
| flow | 45／100 | 22、10、2.69 ms、0、138 ms | 2 | 單次 2 | 2（W_post 2.68／2.82，lead_after 68.00／67.95，model_diff +0.07／+0.18） | 兩位 backlog 重設（612 ms） |
| flow | 80／150 | 34、15、2.06、0、227 | 4 | 串接 2 | 0（串接） | 兩位重設（576 ms），lead_after_max 101.5 |
| flow | 60／300 | 48、12、2.65、0、361 | 4 | 串接 2 | 0（串接、放出在 slew 後） | 兩位重設（715 ms） |
| flow+09a | 45／100 | 23、10、2.36、0、145 | 2 | 單次 2 | 2（W_post 0.11／0.08，lead_after 70.71／70.70，model_diff +0.21／+0.18） | 兩位重設（608 ms），q30 105 |
| flow+09a | 60／150 | 31、11、2.03、0、205 | 3 | 單次 1、串接 1 | 1（W_post 0.09，lead_after 70.68，+0.13） | 兩位重設（560、660 ms） |
| flow+09a | 80／300 | 55、14、2.60、0、371 | 6 | 串接 2（各 3 次） | 0 | 兩位重設（632 ms），size 合計 −110 ms |
| B2 | clean-60 | — | 0 | — | — | 重設 0、Cooldown 0 |
| B2 | 45／100 | 24、10、2.79、0、146 | 2 | 單次 2 | 2（只記錄） | 撤回 +29.9／+29.8（t_c＋150 ms），q30 69，不重設 |
| B2 | 80／150 | 36、15、1.81、0、230 | 4 | 串接 2 | 0 | 撤回 +18.3 後又 late 修正，兩位重設（646 ms） |
| B2 | 60／300 | 53、11、2.76、0、357 | 2 | 單次 2 | 0（放出在 slew 後） | 撤回的幀在 t_c＋150 ms，但找不到正向步；放出後兩位重設（711 ms），lead_after_max 97.4 |

- **注入在必要的格點做得到**：10／10 次 `run_client_delay.py` 的 `completed`，佇列最大 10～15（<192）、送出延遲 p99 1.8～2.8 ms（≤10）、Gateway 速率拒絕 0。9 格的參數都通過事前檢查（第 6 節），沒有做不到的格。
- **凍結偵測器能產生 late 修正的 P 事件**：每個注入跑次都有，計數器增量 2～6（兩位合計）。Jm1～Jm3 需要的量（W_post、lead_before、lead_after、lead_pred、model_diff、q30_max、backlog 重設、撤回）在合格事件上全部算得出來。
- 全部 10 次：`ingress_evidence.py` passed、`run_case` passed、權威 Cooldown 0、D58 的拿不到內容的丟棄 0、D44 旗標 0。
- 合格事件的 model_diff：+0.07～+0.21 ms（5 個事件），在 1.0 ms 之內，正偏差和 D48 第②步（+0.04～+0.65）同向。
- 讀法上的注意（只記錄）：**flow 在 45／100 也重設了**（W_post 2.7 ms 讓領先 68.0 ms，剛好越過 67.667）。flow 的 W_post 實測全部在 2.48～4.73 ms，集中在帶的上緣附近（第 8 節 Jm3 的可達性、第 8 節讀法的限定 1）。
- 放出到 slew 結束的餘裕（`release_to_t_post_ms`）：flow 45／100 70 ms、flow+09a 45／100 58 ms、flow+09a 60／150 17 ms（合格事件）；300 ms 的格是負值（放出在 slew 之後）。
- 開發跑次不計入判定，也不用來調整門檻，只用來確認可達性（Jm2 的最少跑次由 4 提高到 6、核心格改成 3 次，都是依可達性的推論，不是依結果）。
- 修正輪：開發跑次的判定紀錄以本版 `measure_map.py` 重算（`dev/*/*.judge/record.json`；新增 `t_post`、餘裕與候選事件，其他值不變）。

模擬 session 的測試（`prep/mock_session.py`、`prep/mock_tamper.py`、`prep/scratch/ingress_stop_check.py`；輸出 `prep/mock/`，結果 `prep/mock/results.txt`）。模擬 session 用宣告的 63 項排程與 `plan.json`（declared、driver／judge 的 sha），每一項套用同一棵樹的開發跑次紀錄（只測判定的邏輯與數量，不是量測）：

| 測試 | 預期 | 結果 |
|---|---|---|
| full（宣告的 63 項） | plan 核對成立、session 完整、Jm1 成立、Jm2 成立、Jm3「越線側一致（不越線側無法判定）」、`passed` true | 符合 |
| twoside（flow 加 1 個帶下的不重設事件） | Jm3「一致」、`passed` true | 符合 |
| incomplete（最後 1 項沒跑、中斷） | session false、`passed` false | 符合 |
| diff（flow+09a 的一個 model_diff 改成 1.2） | Jm1 不成立、`passed` false | 符合 |
| jm2（flow+09a 的一個越線事件改成不重設） | Jm2 不成立；`passed` 仍 true（Jm2 不是通過條件） | 符合 |
| jm3（flow 的一個越線事件改成不重設） | Jm3 推翻、`passed` false | 符合 |
| stop（clean 前置的 Cooldown 停止） | session false、`passed` false | 符合 |
| stopflag（紀錄上有停止旗標，summary 宣稱完整） | `passed` false | 符合 |
| **noflow**（flow 沒有任何合格事件；xhigh H1） | Jm1 無法判定、`passed` false | 符合 |
| **plantamper**（schedule 只剩 45／100、declared 改掉；xhigh M3） | plan 核對不成立、`passed` false | 符合 |
| driversha（plan 的 driver sha 不符） | plan 核對不成立、`passed` false | 符合 |
| `evaluate`：clean 跑次加 1 筆 rejection＝3 | cooldown 停止 | 符合（`prep/scratch/neg-cooldown.judge`） |
| `evaluate`：ingress 換成 stub（failed／undetermined／passed × flow／B2） | 只有「flow、failed」停止 | 符合（`prep/mock/ingress-stop-check.txt`） |

driver 的測試：`--list-only`（63 項，每棵樹 21 項，各輪 3／27／27／6，起頭次數 flow 7、flow+09a 6、B2 7）；`--dry-run`（preflight 問題 0，exit 0，`prep/dryrun.txt`；主 checkout 已到 `ad2c1c4`，相對 `6fb18f2` 只改 `docs/`）；正式執行缺 `--main-head`／`--judge-sha`／`--driver-sha`／`--idle-gate` 時拒絕、什麼都不寫；值錯誤時拒絕；閒置閘門期間的 Ctrl-C（1 秒後送出 → 1.5 秒內回傳 false）。`spawn()` 的逾時與殘留子程序的測試在第 1 版做過，本版沒有改 `spawn()`。

### 12. 停止條件（立即停下，保留全部證據，不重跑，交使用者決定）

- 注入無效（`injection_invalid`：第 6 節任一項）。
- 狀態改變（「digest 改變」）：主 checkout 的 HEAD 或 `git status --porcelain`、三棵 worktree 的 HEAD 或狀態、任何執行檔、arena、凍結清單的任何檔、`measure_map.py`、`run_map.py`、`sleeper.py` 與開頭不同。
- clean 前置跑次出現權威 Cooldown 拒絕（任何一棵樹）。
- runner 錯誤（沒有 case 目錄、缺 `gateway.log`／`match.log`／`match-commands.jsonl`／`clients-commands.jsonl`／`action-frames.jsonl`、`result.json` 沒有 `artifacts`）、逾時、量測的例外（分析錯誤）。
- sleeper 失敗、閒置閘門 30 分鐘不成立、SIGINT／SIGTERM（跑完目前的項目後；閒置閘門期間也會停）。
- **修好的兩棵樹（flow、flow+09a）上 `ingress_evidence.py` 的 status＝failed**（建議，待使用者確認；第 14 節第 4 點）。`undetermined` 只記錄。B2 樹只記錄。

注入跑次的 Cooldown、D44 的旗標、沒有合格的帶外候選事件都只記錄（第 14 節的決定點 3、5）。clean 前置的 Cooldown 在**任何一棵樹**（包括只記錄的 B2 樹）都會停下整個 session，這是待確認的設計選擇（第 14 節第 7 點）。

### 13. 和大綱不同的地方

1. **起點**：大綱寫「固定 seed 偏移」；本草稿用固定的 12.0 s（第 5 節）。不用 08c L2 的 7.5 s（玩家 2 的死亡與 respawn）。
2. **前置跑次**：大綱只在停止條件寫「clean 前置跑次」；本草稿定為每棵樹 1 次 clean-60，在第 1 輪之前。
3. **Jm1 的母體**：只含「注入在 slew 結束前放出」的事件（第 7 節），長視窗的格在結構上不參與；Jm1 合併 flow 與 flow+09a，另要求每棵樹各自達到最少數。
4. **Jm2、Jm3 的成立條件**：大綱只寫規則；本草稿加了違反的定義、帶內事件不檢定、最少跑次／事件，Jm3 分兩側。
8. **次數**：大綱是每格每樹 2 次；本草稿把核心單次格 45／100、60／100 提高到 3 次（建議，待使用者確認）。
9. **plan 核對**：判定腳本核對 session 的宣告值、排程與兩個腳本的 sha（第 3 節）。
5. **「digest 改變」**：解讀成狀態與雜湊的改變（第 12 節）。本批不重跑權威 digest（第 2 節的理由）。
6. **量測的轉接**：大綱沒提；凍結的 `measure.py` 需要 `delayed_chunks`，以行程內的包裝補上（第 3 節）。這是量測路徑上唯一的新程式，xhigh 檢查請特別看。
7. 三棵樹各用自己的 Match 與 Gateway（同 08c L2；D48 第③步是共用 base 的）。

### 14. 需要使用者決定的地方（附建議；本版依 xhigh 第 5 節改了預設，標「建議，待使用者確認」）

1. **起點 12.0 s**（建議，待使用者確認）：在允許的範圍內，它是唯一沒有第③步 Cooldown 機制的起點（第 5 節）。代價是 flow 的 W_post 集中在帶的上緣附近，所以讀法禁止拿 flow 的比例當 09a 的效果，以第③步 base 的 2／10 為基準頻率（第 8 節限定 1）。替代案：flow 與 flow+09a 交錯用 11.0 s 讓 W_post 分散（注入跑次的 Cooldown 本來就只記錄，不影響移動的量測），代價是 Cooldown 的機制會出現。
2. **門檻**（建議，待使用者確認）：Jm1 合計 12 個事件／6 個跑次、每棵判定樹 6 個／3 個；Jm2 至少 6 個跑次；Jm3 至少 8 個帶外事件、兩側都有才「一致」；Jm3 無法判定或只有一側時仍可 `passed`（只要求沒被推翻；理由同 D48 第②步，而且 Jm1 已要求 flow 自己達到最少數）。Jm2 很可能落在門檻邊上（約 6～8 個合格跑次），Jm3 很可能無法判定或只有一側。
3. **注入跑次的 Cooldown**：只記錄（建議；和 08c L2 一致；12.0 s 起點下結構上幾乎不會發生，萬一出現，照第③步的格式標明那一發是否被延遲）。
4. **`ingress_evidence.py` 的 `failed`**（建議，待使用者確認）：在修好的兩棵樹上**停止**；`undetermined` 只記錄；B2 樹只記錄。理由：D61 的前例在修好的樹上停止，本批的前提是「修好資訊流」，而上行短延遲是 08c L2 沒驗證過的刺激。替代案：只把那個跑次排除在 Jm 的母體之外，不停止。
5. **D44 的旗標**：只記錄、逐筆交使用者判斷（建議；D44 的累計）。
6. **次數**（建議，待使用者確認）：核心格 45／100、60／100 每格每樹 3 次，其他 2 次；合計 63 項、約 27 分。替代案：全部 2 次（57 項、約 25 分），但 Jm2 幾乎一定無法判定。
7. **B2 樹的 clean 前置出現 Cooldown 也停下整個 session**（待確認的設計選擇）：B2 只記錄，但 clean 出現 Cooldown 在閒置主機上是新現象（08c L2 修好的樹是 0），先停下交你判斷。替代案：B2 的 clean Cooldown 只記錄。

### 15. 證據目錄、腳本與執行步驟

- 準備：`prep/`＝`build/target/_build/test/logs/pvp-v7-09r-map-prep-20261011/`。
  - 樹：`prep/tree/`（差分、建置腳本與 log、`artifacts.sha256`、`gateway-vcs.txt`）。
  - 腳本：`run_map.py`、`measure_map.py`、`sleeper.py`、`frozen-map.sha256`；開發用 `dev.sh`、`summarize_dev.py`、`mock_session.py`、`mock_tamper.py`、`scratch/ingress_stop_check.py`。雜湊 `prep-files.sha256`，指令 `commands.txt`。
- 正式 session（核准後；不在本準備中執行）：

```
E=build/target/_build/test/logs/pvp-v7-09r-map-<YYYYMMDD>
P=build/target/_build/test/logs/pvp-v7-09r-map-prep-20261011
shasum -a 256 -c $P/frozen-map.sha256                       # 47/47 OK
python3 -I -B $P/run_map.py --dry-run --main-head <H> --judge-sha <..> --driver-sha <..>   # exit 0, preflight 問題 0
caffeinate -dims python3 -I -B $P/run_map.py --output $E/session --idle-gate --main-head <H> \
  --judge-sha <measure_map.py の sha256> --driver-sha <run_map.py の sha256（＝ measure_map.DRIVER_SHA）>
python3 -I -B $P/measure_map.py --session $E/session         # judgement.json
```

- 核准時把本宣告、`run_map.py`、`measure_map.py`、`sleeper.py`、`frozen-map.sha256` 複製到 `$E/`，雜湊寫進 `$E/declaration.sha256`。

### 16. 本版對 xhigh 檢查的修正（對照）

| xhigh 的項目 | 修正 |
|---|---|
| 必修 1（H1：flow 沒有合格事件也能 `passed`） | Jm1 加每棵判定樹的最少數（6 個事件、3 個跑次；`measure_map.py` `JM1_MIN_*_PER_TREE`）；負向 mock `noflow` 確認 `passed` false |
| 必修 2（H2：Jm3 只靠一側就「一致」） | Jm3 輸出 `crossing_side`、`non_crossing_side`；兩側都有帶外事件才叫「一致」，只有一側時叫「越線側一致（不越線側無法判定）」或反過來；正向 mock `twoside` |
| 必修 3（H3：Jm3 的可達性借了別的分布） | 第 8 節門檻表以開發跑次的 W_post（2.48～4.73 ms）重寫，撤回「0～16.7 ms 均勻」；寫明很可能無法判定或只有一側。第 9 節的預測同步改寫 |
| 必修 4（M1：Jm2 的可達性與統計說法） | 可達性改為約 6～8 個合格跑次（附 t_post 與放出的實測時刻）；「排除 >39%」限定為本起點、本主機，並寫明跑次在相位上不完全獨立；讀法的「每次都走到」改成「本條件下沒有例外」 |
| 必修 5（M2：讀法） | 第 8 節讀法加四點限定：flow 的基準頻率參考第③步 base 2／10；串接的格不能區分 09a；格點只是條件暴露；Jm1 只檢驗放出之後的穩態 |
| 必修 6（M3：判定腳本不核對 plan） | `--session` 核對 declared、重算的 schedule、driver sha（`DRIVER_SHA`）、judge sha；排程與宣告值移到 `measure_map.py`，`run_map.py` 直接使用；負向 mock `plantamper`、`driversha` |
| 必修 7（引用與事實） | Jm1 容許改引 `d48-step2/declaration.md:190`（試跑 0.312），並寫明實測 0.645、餘裕 1.55 倍；第③步的 J1、V4X 標成 stopped session 的記錄值；第 7 節 80／150 的原因改為串接；註明放出到 slew 結束的餘裕和第②步不同，事件記錄 `t_post` 與 `release_to_t_post_ms` |
| 第 14 節的新預設 | ingress `failed` 在修好的兩棵樹停止、`undetermined` 只記錄；核心格 3 次（63 項）；起點 12.0 s 保留並寫進 Cooldown 機制；B2 的 clean Cooldown 停止列為待確認的設計選擇。全部標「建議，待使用者確認」 |
| M4 | 沒有合格、但在帶外的候選事件列成使用者判斷旗標（`not_eligible_outside_band_flags`）；EVENT_KEYS 加 `t_post_after_start_ms`、`release_to_t_post_ms` |
| `run_map.py` 的低項 | 結尾的 host sample 失敗照樣寫 `summary.json`；逾時 SIGTERM 的描述改正（`run_client_delay.py` 不清理，靠 group SIGKILL）；閒置閘門期間回應 Ctrl-C；項目開始前失敗記成 `not_run`，不再記成 runner_error |
| 引用的行號 | `Movement.hpp`、`LocalPlayerPrediction.cpp` 的行號標明是哪一棵樹（flow 與主 checkout 相同；flow+09a 的 `Movement.hpp` 位移 13 行；B2 的 `LocalPlayerPrediction.cpp` 位移 2 行）；`09b-phase-tracking.md` 的行號以 `ad2c1c4` 為準 |

## 09R-map 的結果（2026-10-11 19:27～19:51，commit H `6a0e4d5`）

證據：`build/target/_build/test/logs/pvp-v7-09r-map-20261011/`（`declaration.md` 與腳本的副本、`session/`、`session/judgement.json`、`summary-main.txt`、事後計算 `posthoc-jm1-deltaW.txt`）。63／63 項完成，沒有停止；凍結清單 47 檔開頭與結尾都吻合；ingress 63 次 passed；Cooldown、D44 旗標、D58 的丟棄都是 0。

**判定（依宣告；`passed`＝false）**

| 判定 | 結果 |
|---|---|
| plan 核對、session | 成立 |
| **Jm1** | **不成立**：43 個合格事件（flow 22／12 跑次、flow+09a 21／12 跑次），中位差 0.118 ms，只有 1 個事件超出 1.0 ms：`x1-60-100-r3-flow` 玩家 1，差 7.307 ms（flow+09a 最大 0.411） |
| Jm2（答案） | 成立：flow+09a 的單次 tracking 修正 12／12 個跑次越線並重設（修正後領先 70.15～70.9 ms，W_post ≤0.22 ms），沒有例外 |
| Jm3 | 一致（兩側）：不越線側 18 個事件都沒重設，越線側 3 個都重設；帶內 1 個（66.425 ms，沒重設） |

**Jm1 不成立的那個事件（事後的分析，只記錄；不改宣告的判定）**：
- 那位玩家在修正**之前**就已經有 7.444 ms 的送出等待（`W_pre_1s`），其他 42 個事件都在 0.07～0.22 ms。
- 宣告的模型是「修正後領先＝修正前＋33.3−W_post」，隱含 W_pre≈0（xhigh 檢查也這樣假設）。
- 改用等待的**變化量**（W_post−W_pre）時，43 個事件的差全部 ≤0.332 ms，這個事件是 −0.154 ms。
- 推論：模型本身在上行路徑成立，失敗來自「W_pre≈0」這個沒有寫進判定的前提；這個事件的玩家在注入之前就處於送出被釘住的狀態（09a 要修的現象，出現在 flow 樹）。

**格點（只記錄；單位＝每位玩家的修正單位：總數／串接／backlog 重設）**

| 格 | flow | flow+09a | flow+09a+B2 |
|---|---|---|---|
| 45 ms／100 ms | 6／0／2 | 6／0／6 | 6／0／0 |
| 60 ms／100 ms | 6／0／1 | 6／0／6 | 6／0／0 |
| 80 ms／100 ms | 4／0／0 | 4／0／4 | 4／0／0 |
| 45 ms／150 ms | 4／2／2 | 4／1／4 | 4／0／2 |
| 60 ms／150 ms | 4／2／2 | 4／2／4 | 4／0／3 |
| 80 ms／150 ms | 4／2／2 | 4／4／4 | 4／0／2 |
| 45～80 ms／300 ms | 12／12／12 | 12／8／12 | 12／4／12 |

- B2 在 100 ms 的格撤回約 +29.5～30.5 ms，殘餘約 −2.9～−3.8 ms，沒有來回修正；150 ms 以上的格沒有撤回紀錄。
- 讀法（宣告第 8 節的限定照用）：起點固定 12.0 s，flow 的重設比例只代表這個條件；格點是條件暴露，不是實際頻率。

**意涵（推論，給 09b-plan）**：
1. 09a 單獨上線時，短延遲造成的每一次單次修正都走到 backlog 重設（Jm2；和 D48 第③步的條件力學一致）。所以 09a 不能單獨先上，要和 B 一起規劃。
2. B2 原型在 100 ms 的短延遲上把重設從 16／16 降到 0／16；150 ms 時部分有效（7／12）；300 ms 的持續延遲三棵樹都重設。
3. 串接（缺陷 B）在 150 ms 以上的格普遍出現，B2 原型讓串接在 150 ms 的格消失。

## 09b-plan：09a 與 09b 同批實作、合併一次 L2（2026-10-13 使用者核准，D66）

規劃用 ultracode（3 個方案、1 位評審、1 次 xhigh 對抗式檢查，D63 ③），之後 1 輪修正。原檔與各方案：`build/target/_build/test/logs/pvp-v7-09b-plan-20261011/`（`plan-final.md`、`judge.md`、`adversarial.md`、`design-*.md`）。

狀態：**核准**（2026-10-13，D66：「照建議，繼續」；第 7 節 13 項全部照建議）。本文件以 `judge.md` 第三節為骨架，併入 `adversarial.md` 第五節的 9 項修正與第六節可直接改的項目。全程只讀：沒有改追蹤的檔案、沒有 commit、沒有建置、沒有量測、沒有留下背景程序。下文 `L/` 指 `/Users/karasu/Code/Source/GYO-Engine/build/target/_build/test/logs/`。依據標記：〔實測〕〔程式〕〔推論〕〔提議〕（提議＝沒有直接來源、要交 D45 對抗式檢查與使用者核准的值）。

### 白話摘要

1. 要修兩件事：Client 停頓後送輸入會一直「被釘住」（09a）；短暫網路延遲觸發的追趕修正之後，Client 不會把多推的 2 Tick 收回來，容易連帶觸發重設（09b，修法叫 B2，順便消掉舊樣本混進視窗的小缺陷 A）。
2. 兩者放在同一條 PR #73：先 commit 09a，再 commit 09b；只有 09a 的中間版本不量測、不合併，因為單獨上 09a 會讓 100 ms 延遲的重設從 3／16 變成 16／16。
3. 程式照已量過的原型正式化，不改設計；150 ms 延遲只部分改善、300 ms 延遲不處理，都寫成已知限制。
4. 分 8 批：09a 實作、09b 實作、對抗式審查、本機測試與突變、新寫並凍結 L2 工具、L2 準備與宣告、L2 執行、文件同步。
5. L2：修之前（`eabb107`）對修之後兩棵樹，每棵 66 次、共 132 次，約 60 分鐘機器時間；案例＝100／150／300 ms 短延遲格點＋第 08c 批 L2 的全部案例＋clean-30。
6. 判定的門檻改為取自「同一條注入路徑、修好的樹」的 09R-map 原型量測並寫明餘裕；沒有資料的部分（clean-30、150 ms）只記錄或和同一 session 的修之前比較。
7. 總時間約 18～23 h（含新寫 L2 工具 3.5～4.5 h）；前置核對失敗、要重新宣告時再加 1.5～2.5 h。超過 1.5 倍或單一問題累計超過 2 h 就停下回報。

---

### 代號對照（本文件用到的）

| 代號 | 白話 |
|---|---|
| `09a-impl` | 把 09a 原型（送輸入不再被釘住）正式寫進產品，加測試與突變 |
| `09b-impl` | 把 B2 原型（late 修正後換算樣本並重新擷取一次）正式寫進產品，加測試與突變 |
| `09ab-rev` | 1 位審查者對兩者做對抗式實作審查，之後修 1 輪 |
| `09ab-L1` | 本機的全量測試、權威 digest、突變、TSan |
| `09ab-L2T` | 新寫合併 L2 要用的 driver 與判定腳本，乾跑重現既有結果，凍結並交 D45 檢查（本版新增） |
| `09ab-L2P` | 建置修之後的樹、跑開發跑次、寫 L2 宣告、做 D45 門檻來源檢查 |
| `09ab-L2` | 執行合併 L2 並記錄結果 |
| `09ab-doc` | 契約註解、架構文件、批次文件的同步 |
| `Pab1`～`Pab8` | 對修之後的樹事先寫下、可以被量測推翻的預測（第 2 節各列有白話） |
| `Jab1`～`Jab8` | L2 的判定條件（第 3 節各列有白話） |
| D45 | 「每個 L2 判定門檻送核准前，先由 1 位 xhigh 檢查它取自哪個建置、哪個指標」的規則 |
| D61 | 第 08c 批 L2 核准的規則；本文件只用其中「D44 事後規則」：同一案例在 ≥2 個跑次出現新的重設原因，或平均每跑次多 ≥1 個重設事件，就停下交使用者 |
| M0 | 第 09 批凍結 FireGate 常數之前的影子量測（`09-firegate-c2.md` 的 S3） |
| t_frame | 量測工具在 Client 幀上看到 late 修正的時刻（`events.jsonl` 的 `t_frame_after_start_ms`）；本文件的時間一律用它 |

---

### 0. 本版對對抗式檢查的處理

#### 0-1. 第五節「必須修正」1～9

| # | 對抗式檢查的要求 | 本版的處理 | 位置 |
|---|---|---|---|
| 1 | Pab2／Jab4 只用一個量、改用 09R-map B2 的來源並寫出餘裕、補 `held_post`＝0、標提議 | 只判「撤回量」，殘餘量改只記錄（殘餘＝撤回＋修正量，不獨立）。門檻 [28.5, 31.5] ms 取自 09R-map B2 的 16 個事件（29.54～30.45），兩側餘裕 1.04／1.05 ms；D48③ 只當旁證並寫出它會越過上界。補 `held_post`＝0（16／16 實測為 0）、0 次重設、沒有來回修正。整條標〔提議〕交 D45；另加最少 6 個跑次有合格事件（D64 的 Jm2 先例） | 2 節 Pab2；3-4 節 Jab4 |
| 2 | Pab6／Jab6：clean-30 沒有資料；改為同 session 比較或只記錄；G2E 寫腳本與視窗；閒置閘門與 TimerBaseline 分層 | Pab6 拆成 clean-60（判定，來源 08c L2）與 clean-30（修好的樹上 0 筆資料，只記錄，和同一 session 的 before 並列）。G2E 用 `L/pvp-v7-09b-plan-20261010/latency/decompose.py` 的凍結副本，視窗＝`action-client.json` 的 [start_ns, end_ns)，只取 actual 解析的非 seeded 命令；1.0 ms 標〔提議〕，比較對象是同一 session 的 before。寫明閒置閘門（3 次 5 秒 sleeper 都 <10 ms）與 TimerBaseline 分層（`timer.late_p99_ms` ≥6.0 ms＝8 ms 狀態，只用於分層與解讀） | 2 節 Pab6；3-4 節主機與 Jab6 |
| 3 | Pab7 支持度改「中」，加一個 L1 情境 | 改「中」，寫出機制：往後修的 slew 期間，換算後的 slack＝原始值－已位移量，原始值是小正值的樣本會變成負值。加 L1 特性情境 T-B9（正向 slew 期間連續收到 2 個小正值原始樣本，記下會不會觸發 late）；檔名更正為 `LocalPlayerPrediction.cpp:128`、`:134` 與 `MatchRuntimeHost.cpp:129` | 2 節 Pab7；3-2 節 |
| 4 | M0 銜接：刪「重推條件 11 由本次格點滿足」；late 修正窗口改為 M0 規劃的待決事項 | 刪除。改寫為：重推條件 11 的觸發條件（D49 第二階段）尚未發生（補充，2026-10-14，D73：移到 v7.1）；本批的格點不記錄 FireGate 的 earliest，不能代替它的 FireGate 那一半；「短延遲注入是否併入 M0」交使用者決定。「late 修正窗口」不當排除區，列為 M0 規劃的待決事項，並寫明至少保留 Cooldown＝0 的檢查與 D48③ 兩次 Cooldown 發生在撤回之前 | 4-3 節；7 節決定 10 |
| 5 | Pab4 與已知限制：統一時間基準；邊界約 70～87 ms（t_frame）；降成敘述或凍結新定義 | 時間一律用 t_frame，並寫出換成 measure.py 的 t_c 時的數字。邊界改為約 70～87 ms，附 D48③ r1／r2／r5 的值與 09R-map 的範圍。Pab4 降成敘述（不判定、不當預測），支持度「低」；新定義與凍結列進「之後再評估」 | 1-2 節已知限制；2 節 Pab4 |
| 6 | Jab8：寫明 D61 門檻是借來的與它的靈敏度 | 寫明：D61 在 08c 的故障案例（三棵樹逐案相同）核准，套到 150 ms 格是借用；靈敏度：before 每跑次 1.0 次重設，規則要 after 到 2.0 次（每個單位都重設，等於只有 09a 沒有 B2）才會停，或出現新的重設原因。決定 7 依此改寫 | 3-4 節 Jab8；7 節決定 7 |
| 7 | 新工具：列出新寫並凍結的 driver 與判定腳本，交 D45；重估時間 | 新增批次 `09ab-L2T`：`run_ab.py`（driver，涵蓋 run_gameplay、run_network、backpressure_probe、run_client_delay 四種 runner）、`judge_ab.py`（判定）、`sleeper.py`（照抄），以及要加進凍結清單的既有腳本副本。乾跑必須重現 09R-map 與 08c L2 的已發表數字。交 D45。時間 +3.5～4.5 h，總計改為 18～23 h | 3-3 節；3-1 節 |
| 8 | 刪停止條件 6 的 q30 >95 | 刪除。停止條件 6 只留「開發跑次的 100 ms 格出現重設或來回修正」（Pab2 預測為 0，來源 09R-map 0／16） | 4-1 節 |
| 9 | 開發跑次包含 after 樹的 x1、ramp、100 ms 格；寫出前置核對失敗時的重新宣告流程 | 開發跑次（after 樹）：100 ms 三格各 1、x1 ×1、ramp ×1、clean-30 ×1、clean-60 ×1，ramp 記錄來回修正與 late 數；只記錄，不當門檻來源。前置核對在 L2P 開始與 session 前各做一次；失敗時依差分分三類處理（3-5 節） | 3-5 節 |

#### 0-2. 第六節「可以只記錄」：本版直接改了哪些

| 項目 | 處理 |
|---|---|
| 「一律夾在 2 Tick」→「最多 2 Tick」；`:97`→`:98` | 已改（1-2 節） |
| 「8 個樣本的 P90＝最大值」→「n≤10 時取最大值，決定時的樣本數可能超過 8」 | 已改（1-2 節） |
| Pab7 引用的檔名 | 已改（2 節） |
| 09a 清除點有四個（連線重設處也清），補 `wait-not-cleared-at-reset` | 已改（1-1 節、3-2 節） |
| 「要修 300 ms 就得改 105」→「或新增計畫外的規則」 | 已改（1-6 節） |
| `PredictionTests.cpp:1241-1257` 的測試名稱只對非 late 修正成立，要更新註解 | 已列進 09b-impl（只改註解，不改斷言） |
| HANDOFF 每批開始與結束都要更新 | 已寫進 3-1 節（每批的完成條件之一） |
| Fisher 單側 p 的單位、0／9 上界的獨立性 | 列進第 6 節「已知、只記錄」 |
| `send_wait.txt` 只涵蓋 7 個案例 | 寫進 Pab1 的來源，並列進第 6 節 |
| 格點注入期間碰到 D44 型自發重設會被算成注入造成 | 列進第 6 節 |
| b2-extend「docs 以外多 1 個檔」應為 2 個 | 列進第 6 節 |

#### 0-3. 本版另外順手改的（第二節裡沒有列進第五節，但會影響判定）

- **Pab1 的口徑與範圍**：被釘住只用「每秒送出等待中位數 >2 ms 的玩家·秒」一個口徑（和 `send_wait.txt`、M0 相同），拿掉逐命令的「每個 epoch 的前 2 個命令」排除；W_post ≤1.0 ms 只套用在 100／150 ms 格的 late 事件，因為 08c L2 的 x1／ramp 上，flow+09a 每個 late 事件的修正後等待中位數是 3.47～4.92 ms〔實測，`L/pvp-v7-09r-eval-20261011/send_wait.txt`〕，套上去會誤判失敗。1.0 ms 標〔提議〕（它出自 D48③ 宣告的預測，不是實測）。
- **Pab3 支持度改「中」**：80／150 不串接靠的是「放出早於 slew 結束」，餘裕約 40 ms（adversarial 第二節 Jab5）。

---

### 1. 要修的東西與設計

#### 1-1. 09a：照 `09a-input-send-pinning.md` 的修正版與原型 `e6885b7`

- 設計：`charge −= Min(1−60/R, Max(0, Min(wait/P−(tokens−1), Burst−tokens)))`，R＝80。
- 清除點**四個**〔程式，`e6885b7`〕：送出時；連線重設（diff 後約 `ClientConnection.cpp:187`，和 `haveSentInput`、`nextActionSendAt` 一起清）；epoch 變更（約 `:549`）；完整 ACK（約 `:559`）。
- 實測證據：D48③ 的 W_post 最大 0.156 ms（Gateway→Match IPC 注入）；09R-map 的 flow+09a 21 個事件 W_post ≤0.217 ms（`L/pvp-v7-09r-map-20261011/summary-main.txt`，同一條 Client→Gateway 路徑）；08c L2 的 flow+09a 被釘住 0 玩家·秒（`send_wait.txt`，涵蓋 7 個案例，沒有 downstream-1000ms 與 gateway-1000ms）。
- 程式證據：補充率等於產生率（`ClientConnection.cpp:593-628`；`Movement.hpp:22`、`:28`）。
- 推論：恢復步數 N＝ceil(R/(R−60))＝4；R 的可選範圍 75<R≤85。

#### 1-2. 缺陷 B：B2 照原型 `c2705be` 正式化，不改重新擷取用哪些樣本（P）

- 設計三部分：
  - 換算：`phaseAppliedSeconds_`，每個命令記下它產生時的 `appliedSeconds`。
  - late 判定改用換算後的 slack。
  - late 修正後的下一次決定改用首次決定的規則（`MovementPhaseFirstSamples`＝8 個樣本、`MovementPhaseFirstDeadbandSeconds`＝0.5 ms 死區，`Movement.hpp:53`、`:56`），`phaseDecided_` 不清。
- 程式證據：
  - late 修正**最多** 2 Tick（`LocalPlayerPrediction.cpp:98` 是 Clamp；`Movement.hpp:59`；09b 文件 `:1077` 已更正為「並非一律 2 Tick」）。
  - 往回修要等 240 個樣本的視窗（`Movement.hpp:54`）。
  - backlog 的條件是 30 Tick 合計 ≥105（`PvpMatch.cpp:452-458`、`Movement.hpp:38-39`）。
  - 串接的來源是 `LocalPlayerPrediction.cpp:128` 在 settling 中照樣累加、`:134-137` 立即再修。
- 實測證據：
  - 09R-map 的 B2（`0e1fabb`），100 ms 三格 0／16 重設；撤回 +29.54～+30.45 ms；`held_post` 16／16 為 0；q30 69～72。
  - D48③ 的 20 個事件撤回 +29.85～+32.75 ms（Gateway→Match IPC 注入，旁證）。
  - 100 與 150 ms 格串接 0／28（`judgement.json` 的 `map_recorded.flow09a-b2/*`）。
- **已知限制（寫進文件；時間基準＝t_frame）**：
  - 程式證據：已執行命令的 slack 樣本到它被解析的那個 Tick 才發布（`MatchRuntimeHost.cpp:605-611`）；遲到樣本在到達時發布（`:228-232`）；每次發布只留最小樣本（`:129-134`）。百分位索引是 min(n−1, ⌊0.9n⌋)，n≤10 時 P90 就是最大值，n＝11 起是第二大（`LocalPlayerPrediction.cpp:24-30`）；決定在 slew 結束時做，當時的樣本數可能超過 8。
  - 實測證據（放出時刻－t_frame）：
    - 09R-map：100 ms 格 31～48 ms，16 個單位全部完整撤回；150 ms 格 87～101 ms，12 個單位中 7 個重設、5 個只部分撤回（size_v3 −22.1～−28.5，淨撤回 5～11 ms；重設的單位 size_v3 −34.2～−39.9，即重新擷取反而再往前修）。
    - D48③（IPC 注入）：r1、r5 約 70 ms 仍完整撤回（29.73～30.23）；r2 約 81 ms 部分撤回（16.0）；r3、r4、r6 約 93～97 ms 沒有撤回或重設。
    - **邊界約在 70～87 ms 之間**（t_frame 基準）。
    - 換成凍結 `measure.py` 的 t_c（比 t_frame 早 13.7～24.1 ms）時：100 ms 是 49～70 ms、150 ms 是 103～115 ms。兩種基準不得混用。
  - 推論：放出得晚時，決定那一刻的樣本裡還沒有反映延遲已經消失的樣本，撤回量變小，甚至反向。
- 為什麼現在不改：改了就要重新取得 100 ms 的證據；而改用「只收 late 修正之後的樣本」（M）的涵蓋範圍估計也到不了 150 ms 的 87～101 ms。

#### 1-3. 缺陷 A：由換算取代 A1

- 程式證據：病因是 settle 只前移 `settledAfter_`、不清視窗（`LocalPlayerPrediction.cpp:323-328`）。換算之後，settling 期間收到的樣本描述的就是修正後的相位。
- 實測證據：缺陷 A 的量級 ≤0.27 ms（09b 文件「缺陷 A」）。
- 用一個 L1 測試釘住：現在的頭要失敗，修改後通過。

#### 1-4. 不採用的路線：A1＋B0′

- 09a 上線後，100 ms 的重設全是單次修正（串接 0、重設 16／16），B0′ 碰不到（minimal-a-b0 第 0 節）。
- 它和 B2 互相取代、不能疊加。

#### 1-5. 順序：同一條 PR #73，先 commit 09a，再 commit 09b

- 只有 09a 的中間頭禁止任何量測或合併；L2 只在兩者合併後的頭上做。
- 依據〔實測〕：09a 單獨上線時 Jm2 是 12／12，100 ms 格從 3／16 變成 16／16。
- 依據〔推論〕：被釘住時，換算的前提「slack 隨產生時刻 1:1 移動」不成立，所以 B2 也需要 09a。

#### 1-6. 範圍外：300 ms 型的「延遲消失型」重設

- 實測證據：B2 在 45／60 ms 的領先約等於 37.3＋δ；三棵樹都是 12／12 重設。
- 要修它，得改 backlog 門檻 105 或 late 修正的上限（停止條件），**或新增計畫外的 Client 規則**（例如延遲消失時快速正向回收，列在「之後再評估」）。
- 每次重設會讓 1 個連線品質視窗判失敗，連續 3 個就踢人（`Movement.hpp:72-73`）；實際網路上的頻率未知〔推論〕。

#### 1-7. 已知限制（補充，2026-10-14，09ab-doc：彙整 09ab-rev、09ab-L2 與 D71）

09a＋09b 合併後的頭（`b205615`）上已知、本批不修的行為。產品程式的對應註解在 `LocalPlayerPrediction.cpp` 的 `TrackPhase`、`LocalPlayerPrediction.hpp` 的 `reacquiring_`、`Movement.hpp` 的 `InputSendBurst`／`InputSendDrainRate`；契約面見 `protocol-v5.zh-Hant.md` §1 的 2026-10-14 追記。

1. **撤回少 3.3 ms，殘餘是結構性的（Pab2 的機制）。** 往前修的 late slew 期間，命令以 75 Hz 產生、token 以 60 Hz 補充，09a 的折讓式讓每個命令停在平衡等待 P·(1−60/75)＝3.33 ms（和 R 無關；`ClientConnection.cpp:607-623`、`LocalPlayerPrediction.cpp:291-313`）。重新擷取決定那一刻最大的樣本正是這些命令，所以**撤回量≈|late|−3.33 ms**（100 ms 格：33.33−3.33＝30.0），撤回後相位偏早約 3.3 ms（`residual_ms`≈−3.3），要到下一次視窗決定（約 4 秒）才修回。方向安全（不會造成 late、重設或串接）。實測：09R-map 原型 +29.54～+30.45、09ab-L2 after 29.71～30.08 ms。證據：09ab-rev §1-a（`build/target/_build/test/logs/pvp-v7-09ab-impl-20261013/rev/xhigh-review.md`）、`build/target/_build/test/logs/pvp-v7-09ab-l2-20261014/summary-main.txt`。
2. **撤回後約 4 秒內，相位誤差 3.3 ms 超過 FireGate 的 guard 2 ms**（guard 的前提是「決定後的相位誤差 ≤ 死區 2 ms」，`FireGate.hpp:24-27`）。這段期間 `earliest` 可能比實際晚 1 Tick，只影響壓著冷卻邊界的射擊（約 8%）。交 M0（4-3 節；宣告第 12 節第 3 點）。修法候選：第 5 節第 1 項的 S（只收 slew 結束之後產生的命令），可以直接消掉這 3.3 ms。
3. **只撤回決定那一刻樣本看得到的部分。** 延遲消失得晚（放出晚於 late 修正的 slew 結束；t_frame 基準的邊界約 70～87 ms）時只部分撤回或反向，150 ms 格部分單位重設（1-2 節、Pab4）。
4. **δ＞70.6 ms 時，B2 要放出比 slew 結束早超過約 11～19 ms 才不串接**（界線約 11 ms、跑次間會變動；D71）。09ab-L2 的 80/150：r1 放出到 t_post 22.6、22.7 ms 不串接；r2、r3 是 11.4～11.5 ms 仍串接（4 個單位，Jab5 不成立）；09R-map 原型是 18.8、10.9 ms 不串接。宣告原本只把「放出到 t_post ≤0」的串接當成已知限制，D71 把界線改寫成這一條。不另開修正批；重新擷取的樣本邊界（S／M）仍是第 5 節第 1 項，留到 v7 完成後統一評估。
5. **300 ms 型的延遲消失照舊重設**（1-6 節，範圍外）。
6. **送出的平均率只在 60 Hz 產生時是 60／秒。** 往前 slew 期間送出跟著 75 Hz 產生率，每個命令固定等約 3.3 ms；任何 1 秒最多 82 個輸入封包（09a 文件；`Movement.hpp`）。

---

### 2. 可被推翻的預測（after＝09a＋09b 的頭）

| 代號 | 白話 | 預測 | 推翻條件 | 支持 |
|---|---|---|---|---|
| Pab1 | 送輸入不再被釘住 | (a) 全部案例被釘住的玩家·秒＝0（口徑：每秒送出等待中位數 >2 ms）；(b) 100／150 ms 格的每個 late 事件 W_post ≤1.0 ms〔提議〕 | 任一不成立 | (a) 強：08c L2 flow+09a 7 個案例為 0（`send_wait.txt`）。(b) 強：09R-map 21 個事件最大 0.217（餘裕 0.78）、D48③ 0.156 旁證。x1／ramp 不套 (b)（flow+09a 為 3.47～4.92 ms），只記錄 |
| Pab2 | 100 ms 延遲後完整撤回、不重設 | 100 ms 三格的每個合格 late 事件：撤回量 ∈ [28.5, 31.5] ms〔提議〕、`held_post`＝0、0 次非 respawn 重設、沒有來回修正 | 任一不成立 | 強：09R-map B2 16 個事件 29.54～30.45（下界餘裕 1.04、上界餘裕 1.05）、重設 0／16、`held_post` 0／16、來回修正 0。D48③ 旁證 29.85～32.75（IPC 路徑；32.75 會越過上界，見決定 6） |
| Pab3 | 100／150 ms 不再串接修正 | 100 和 150 ms 格的串接單位＝0 | ≥1 | 中：09R-map B2 0／28；80／150 靠「放出早於 slew 結束」，餘裕約 40 ms |
| Pab4 | （敘述，不是預測）150 ms 只部分有效 | 150 ms 格：部分單位重設、部分只部分撤回；放出－t_frame 越接近 70 ms 越可能完整撤回 | 不判定 | 低：凍結的 `measure3` 在 150 ms 給 `withdraw_ms`＝None（late slew 與重新擷取併成一步），「第一個乾淨樣本」也沒有定義；09R-map 裡「完整撤回」那一類是空的 |
| Pab5 | 300 ms 照舊重設 | 兩棵樹全部重設；45／60 ms 的單次修正領先＝37.3＋δ ±3 ms（排除 80／300 的串接單位） | 有單位沒重設，或偏差 >3 | 強：12／12；實測偏差 +0.10～+1.95。只記錄 |
| Pab6 | 閒置主機上沒有退步 | clean-60：late 修正、非 respawn 重設、Cooldown、被釘住都是 0；G2E（產生→執行）每位玩家 P50 的合併中位數和同一 session 的 before 相差 ≤1.0 ms〔提議〕。clean-30：不預測（修好的樹上 0 筆資料），只記錄 | clean-60 任一不成立 | clean-60 的 0：中～強（08c L2 flow／flow+09a 各 4 次為 0；09R-map B2 clean-60 前置 1 次；B2 只有這 1 次）。G2E：中（clean 也有首次決定與視窗修正，slew 期間換算不是恆等；1.0 ms 出自舊版 09b-3 草案，沒有來源） |
| Pab7 | 抖動網路下不觸發 late 修正 | network0／20／40 的 late 修正＝0 | ≥1 | 中：08c 32／32 是 flow 與 flow+09a，沒有 B2。〔程式〕每次發布只帶最小樣本（`MatchRuntimeHost.cpp:129`），late 要連續 2 次（`LocalPlayerPrediction.cpp:128`、`:134`）；但往後修的 slew 期間，換算後 slack＝原始值－已位移量，原始值在 (0, 位移量) 的樣本會變負，network40 的遺失加抖動下可能湊成連續 2 次〔推論，機率低〕。L1 用 T-B9 釘住行為 |
| Pab8 | 持續延遲（x1、ramp）不預測 | 不做預測，照 D61 的事後規則比較兩棵樹 | D61 的旗標 | B2 在這兩個案例沒有資料；開發跑次先看（3-5 節）。〔推論〕ramp 上升期間重新擷取取最大值，可能再往前修，下降時領先更大 |

（補充，2026-10-14，09ab-doc；來源 09ab-rev §1-a、§7（`build/target/_build/test/logs/pvp-v7-09ab-impl-20261013/rev/xhigh-review.md`），L2 宣告第 11 節已採用）：
- **Pab2 的機制**：撤回≈|late|−3.33 ms，殘餘 +3.3 ms（相位偏早）是結構性的，見 1-7 節第 1 點。預期中心值 30.0 ms 有機制來源（33.33−P·(1−60/75)），不只是分布。
- **Jab4 上界對 role 晚醒的敏感度**：撤回量≈33.33−（決定那一刻最大樣本的發布→送出等待）；role 晚醒 j ms 時那個命令的等待是 max(0, 3.33−j)，所以 **j＞1.5 ms 就可能讓撤回超過 31.5**（09ab-rev 原寫 1.8，L2 宣告的 D45 xhigh M1 依同一條式子更正）。j 是單一命令相對 75 Hz 節拍的晚醒抖動，整體固定偏移不影響。已量到的 j≈0～0.45 ms；09ab-L2 的 18 個事件 29.71～30.08，沒有往上偏。
- **W_post 的口徑**：Pab1(b) 一律用 slew 結束後的視窗 [t_post, t_post＋0.25 s)（和 09R-map 相同）；把 slew 期間的命令算進去會得到結構性的 3.33 ms 而誤判失敗（x1／ramp 的 3.47～4.92 ms 就是這個原因）。
- **Pab3 的「餘裕約 40 ms」**：和實測不符，L2 宣告已撤回（09R-map B2 的放出到 t_post 是 10.9～18.8 ms）；結果見 1-7 節第 4 點。

---

### 3. 實作分批、L1、L2

#### 3-1. 分批、檔位、時間

| 代號 | 白話 | 檔位 | 時間 |
|---|---|---|---|
| 09a-impl | 09a 原型正式化，加 worker 測試與突變 | high；token 折讓式與四個清除點局部 xhigh | 2.5～3.5 h |
| 09b-impl | B2 正式化，加測試（含 T-B9）與突變 | high；換算的正負號、settle／late／重新擷取的交界局部 xhigh | 3.25～4.25 h |
| 09ab-rev | 1 位對抗式審查，之後 1 輪修正 | xhigh 審查＋high 修正 | 1.5～2 h |
| 09ab-L1 | 全量 CTest、權威 digest、全部突變、TSan、probe 30 次 | medium | 約 1.5 h |
| 09ab-L2T | 新寫 driver 與判定腳本、乾跑重現、凍結、D45 檢查工具 | high 撰寫＋xhigh 檢查 | 3.5～4.5 h |
| 09ab-L2P | 建置 after 樹、前置核對、開發跑次、宣告、D45 門檻來源檢查 | 宣告 high＋xhigh 檢查；開發跑次 medium | 3.5～4.5 h |
| 09ab-L2 | 執行，記錄結果 | medium | 約 1.5 h（機器約 60 分） |
| 09ab-doc | 文件同步 | medium | 約 1 h |

- 合計 **18.25～22.75 h（約 18～23 h）**。前置核對失敗而要重新宣告（3-5 節的 (c) 類）時 +1.5～2.5 h。超過 1.5 倍，或單一問題累計超過 2 h，就停下回報。
- 每批開始、里程碑、結束時，HANDOFF 的「現況」與 P2 記錄器和工作同一個變更更新（不是只在 09ab-doc 才做）。
- 09ab-L2T 和 09a-impl／09b-impl 互不依賴，可以並行；09ab-L2T 的 D45 檢查要在 09ab-L2P 寫宣告之前完成。
- 09ab-doc 要同步：`Movement.hpp` 的契約註解、`network-architecture.zh-Hant.md:81`、`:222-223`、`08-client-worker-asio.md:20`、第 09 批的 S2／S3、09a 與 09b 文件、HANDOFF。

#### 3-2. L1 大綱

- **09a**：照 09a 文件。
  - `worker_main` 的測試放在 `:429` 之後、序號 ≥78；容許值由 probe 重複 30 次推導。
  - 補「第 8 步之後連發 2 個」和「等待中遇到 ack／epoch 變更」兩段檢查。
  - 加 Input(78,78) 或 `static_assert` 的總預算。
  - `MovementRecoveryTests` 的打折只加在 `Path::Product`。
  - 突變 `v7-09a`：`charge-off`、`charge-every-send`、`waited-sticky`、`wait-not-cleared-at-epoch`、`wait-not-cleared-at-full-ack`、**`wait-not-cleared-at-reset`**、`drain-70`、`drain-75` 必須被殺；`drain-85` 預期存活，寫明理由。
- **09b 的測試**：
  - 原型已有的 T-B1、T-B2，以及原型審查補的 X1～X4。
  - 恆等式：沒有修正時，與現在的頭逐值相同，≥80 個種子。
  - 缺陷 A：修正後第一個滿窗的 P90 沒有舊相位的偏移；現在的頭要失敗。
  - 每串 late 修正只觸發一次重新擷取。（更正，2026-10-14，09ab-doc：原寫「每次」；連在一起的多次 late 修正共用 1 次重新擷取，見 `tests/object_fps_pvp/PredictionTests.cpp` 的串接測試與 09ab-rev §3。）
  - SeedLead 清掉 `phaseAppliedSeconds_` 和 `commandAges_`。
  - 持續延遲 >70.6 ms 時仍會串接。
  - 特性測試：放出晚於 slew 結束時，P 只部分撤回。這是把已知限制釘住，不是改善的證明；測試註解寫明「之後若改善這個限制，這個測試會失敗，屬預期」。
  - **T-B9（Pab7 的特性情境）**：往後修（撤回方向）的 slew 期間，連續收到 2 個原始 slack 為小正值（小於已位移量）的樣本。斷言只記錄現在的行為（會不會觸發 late 修正），不新增規則；結果寫進 L2 宣告的 Pab7 說明。
  - far 測試（`PredictionTests.cpp:1229-1238`）和 `:1241-1257` 的斷言都不改；`:1241-1257` 的測試名稱註解改成「只對非 late 的修正成立」。
- **09b 的突變 `v7-09b`**：`rebase-off`、`rebase-sign-flip`、`late-raw-slack`、`reacquire-never-clears`、`reacquire-window-deadband`（改用 240 個樣本、2 ms 死區）、`conversion-missing-unslewed`、`seedlead-partial-clear`、`reacquire-on-window-correction`。每一條都只在預期的訊息出現時算 killed。
- **閘門**：全量 CTest、權威 digest scale 1 和 3 各 35／35 為 0 差分（只是閘門，它不保護相位追蹤）、TSan `object_fps_pvp.ipc`、沒有 stale 的突變。

#### 3-3. 新寫並凍結的 L2 工具（09ab-L2T）

09R-map 的 `run_map.py`／`measure_map.py` 把宣告值、排程與三棵樹寫死，只有 run_gameplay 與 run_client_delay；08c L2 的 `run_l2.py`／`judge_l2.py` 也寫死三棵樹，沒有格點的量測。合併 L2 要新寫：

| 檔 | 內容 | 起點 |
|---|---|---|
| `run_ab.py`（driver） | 兩棵樹、DECLARED、排程、執行檔 sha、四種 runner（`run_gameplay.py`：clean-30／60；`run_network.py`：network0／20／40；`backpressure_probe.py`：downstream-1000ms、downstream-respawn-1000ms、gateway-1000ms；`run_client_delay.py`：x1、ramp、格點）、閒置閘門、每輪前後 sleeper、`--main-head`／`--judge-sha`／`--driver-sha`、每個跑次之前的狀態核對、逾時與 process group 的收拾 | 08c `run_l2.py`（`f95839db…`）與 09R-map `run_map.py`（`cc8be0ed…`） |
| `judge_ab.py`（判定） | plan 核對（宣告值、排程、driver sha、自己的 sha，同一件事只定義在它裡面）；只呼叫凍結的分析器：`ingress_evidence.py`（Jab2）、D48 偵測器的計數器增量（late 數）、`measure.py`＋`measure3.py`（經 09R-map 的轉接，格點事件）、G2E 的 `decompose.py` 副本、被釘住的 `send_wait.py` 副本；D61 事後規則照 `judge_l2.py` 的實作搬過來 | 09R-map `measure_map.py`（`f263cb2d…`）與 08c `judge_l2.py`（`e6a8916f…`） |
| `sleeper.py` | 照抄（`98105dbf…`） | 08c L2 |

- **凍結清單**：09R-map 的 47 檔（08c 的 44 檔＋D48 偵測器、`measure.py`、`measure3.py`）＋`L/pvp-v7-09b-plan-20261010/latency/decompose.py`＋`L/pvp-v7-09r-eval-20261011/scripts/send_wait.py`，共 49 檔。後兩者以副本的 sha256 凍結；`run_network.py` 與往例相同只受 git 狀態核對保護。
- **乾跑（凍結前必須通過）**：
  - `judge_ab.py` 對 09R-map session 重算的 B2 撤回量、重設數、串接數、`held_post`，和 `summary-main.txt` 逐值相同。
  - 對 08c L2 session 重算的 late 數、被釘住、D61 旗標，和 `summary-main.txt`、`send_wait.txt` 相同。
  - 對 08c L2 的 clean-60（flow 對 flow+09a）算出 G2E 的差，作為 Jab6 提議值的參考（記錄來源與數字）。
  - mock session（不完整、plan 不符）要被拒。
- **D45**：xhigh 檢查兩支新腳本的每個判定是否照宣告、門檻取自哪個建置與指標；修正後重算 sha，核准時一起凍結。

#### 3-4. L2 宣告大綱

**兩棵樹**
- before：`../GYO-Engine-l2-flow`（`eabb107`），沿用 08c L2 的建置，開跑前核對 `artifacts.sha256`。
- after：09a＋09b 的頭，在 detached worktree 從零建置。

**案例**（每棵樹 66 次，共 132 次）
- Client→Gateway 短延遲格點：x1，起點 12.0 s（D64 的理由）；δ∈{45, 60, 80} ms × {100, 150} ms 各 3 次；× 300 ms 各 2 次，只記錄。
- 08c L2 的全部案例，照原次數與 x1、ramp 的 7.5 s 起點：clean-60 ×4、network0 ×4、network20 ×6、network40 ×6、downstream-1000ms ×4、downstream-respawn-1000ms ×2、gateway-1000ms ×2、x1 ×6、ramp ×4。
- clean-30 ×4。

**主機**
- 照 08c L2 與 09R-map：session 期間不做開發；`caffeinate -dims`；前景沒有其他使用者程式；暫停 PR #73 的 auto-fix；背景 agent 與分析先停。
- 閒置閘門（`--idle-gate`，正式執行必須帶）：第一個項目之前連續 3 次 5 秒 sleeper，每次最大值 <10 ms；不成立每 30 秒重試，最多 30 分鐘，仍不成立就停下。每輪前後各記 1 次 sleeper 與前 5 名 CPU 程序。
- **TimerBaseline 分層**：每個跑次記 probe 的 `timer_baseline`；`late_p99_ms` ≥6.0 ms 記為 8 ms 狀態（第 05 批的定義），同時記 `interval_p99_ms`（`cross.py` 的 ≥22 ms）與兩者的一致率。分層只用於解讀與並列（clean-30／60、G2E 依狀態分欄），不是判定、門檻或分母。

**判定與門檻來源**

| 代號 | 白話 | 條件 | 來源 |
|---|---|---|---|
| Jab1 | session 是宣告的那一個 | plan 核對、session 完整（132 項）、腳本 sha 等於宣告值 | 09R-map 的作法（D64） |
| Jab2 | 資訊流守恆沒有壞 | 每個跑次 ingress passed；修好的樹上 failed 或未判定就停止 | D61、D64 |
| Jab3 | 送輸入不再被釘住（Pab1） | after 全部案例被釘住玩家·秒＝0；100／150 ms 格的 late 事件 W_post ≤1.0 ms〔提議〕 | 被釘住：08c L2 flow+09a（7 個案例）。W_post：09R-map flow+09a 21 個事件最大 0.217（同路徑、原型）；D48③ 0.156 旁證 |
| Jab4 | 100 ms 完整撤回（Pab2） | 100 ms 三格每個合格事件：撤回量 ∈ [28.5, 31.5] ms〔提議〕、`held_post`＝0、0 次非 respawn 重設、沒有來回修正；而且 9 個跑次中 ≥6 個有合格事件，否則記「無法判定」並交使用者 | 09R-map B2 16 個事件（`0e1fabb`，同一條路徑、修好的樹），經前置核對；最少跑次照 D64 的 Jm2；`held_post` 照 D48③ 已核准的 J2d。殘餘量只記錄 |
| Jab5 | 100／150 ms 不串接（Pab3） | 串接單位＝0 | 09R-map B2 0／28，經前置核對 |
| Jab6 | 閒置主機沒有退步（Pab6） | clean-60：after 的 late 修正（偵測器增量）、非 respawn 重設、Cooldown、被釘住都是 0；G2E 合併中位數 \|after−before\| ≤1.0 ms〔提議〕，before 取同一個 session、同一支凍結腳本、同一個視窗。clean-30：只記錄，和 before 依 TimerBaseline 分層並列 | clean-60 的 0：08c L2；G2E：同 session 的 before，1.0 ms 的參考值在 09ab-L2T 由 08c L2 重算 |
| Jab7 | 抖動網路下沒有 late 修正（Pab7） | network0／20／40 的 late 修正＝0；不成立就停下交使用者，附 T-B9 的行為 | 08c L2（flow、flow+09a 32／32；沒有 B2，支持度中） |
| Jab8 | 故障案例與 150 ms 沒有整體惡化 | 08c 的故障案例（x1、ramp、downstream、gateway、network）和 150 ms 格，套用 D61 的 D44 事後規則（同一案例新原因 ≥2 個跑次，或平均每跑次多 ≥1 個事件），成立就停下交使用者 | 故障案例：D61 原本的適用範圍。**150 ms 格是借用**：D61 在三棵樹逐案相同的 08c 故障案例上核准，150 ms 格預期兩棵樹不同。靈敏度：before（flow）150 ms 每跑次 1.0 次重設（09R-map 6／12 單位），規則要 after ≥2.0 次（每個單位都重設，等於只有 09a 沒有 B2；B2 原型是 7／12＝1.17）才會停；另一條是出現 backlog 以外的新重設原因 |

**計數方式**：late 修正一律用凍結 D48 偵測器的計數器增量（D63④），不用 `judge_l2.py` 的最大值。

**只記錄**
- Pab4（含 150 ms 每個單位的 size_v3、放出－t_frame、重設、q30）、Pab5、Pab8。
- clean-30 的全部量（late、重設、G2E），依 TimerBaseline 分層，和同 session 的 before 並列。
- 殘餘量；x1／ramp 的 W_post；ramp 的來回修正數與 late 數。
- 以等待變化量（ΔW）表示的模型，寫明「事後得出、本次才預先宣告」（D65）。
- q30_max、撤回的時刻與量、D58 的丟棄、注入跑次的 Cooldown、D44 旗標、主機狀態。

#### 3-5. 前置核對、開發跑次與重新宣告

**前置核對**（L2P 開始時一次、session 開跑前一次）
- `git diff 0e1fabb <after> -- apps/object_fps_pvp/src apps/object_fps_pvp/include`（`0e1fabb` 同時含 09a 與 B2 原型）只能有註解。

**開發跑次**（after 樹；只記錄，不計入判定，不當門檻來源）
- 100 ms 三格各 1 次、x1 ×1、ramp ×1、clean-30 ×1、clean-60 ×1；ramp 記錄來回修正與 late 數。機器約 10～15 分。
- 用途：確認新工具在 after 樹上能跑完；提早看到 x1／ramp 會不會碰到 D61 停止、clean-30 在修好的樹上的行為。
- 開發跑次的 100 ms 格出現重設或來回修正 → 停止條件 6。

**前置核對失敗時的重新宣告流程**
1. 停下，不跑開發跑次（或不開 session）。把差分逐段分類並記進 HANDOFF：
   - (a) 只有改名、搬移、`static_assert`、常數的寫法改變，產生的行為不變；
   - (b) 只動測試或註解以外的非執行路徑（例如 log 文字）；
   - (c) 會改變行為的程式修正（例如審查之後的修正）。
2. 只有 (a)(b)：宣告寫明每一段的分類與理由，借自原型的門檻保留；D45 的 xhigh 檢查要確認分類。
3. 有 (c)：Jab3 的 W_post、Jab4、Jab5 與 Pab1(b)、Pab2、Pab3 全部降為〔提議〕；after 樹的 100 ms 三格開發跑次加到各 2 次（只記錄）；xhigh 只重新檢查受影響的判定；改版的宣告送使用者核准。時間 +1.5～2.5 h。
4. 不論哪一類，宣告裡的前置核對改成對「核准時的 after commit」比對，session 前再核一次。

---

### 4. 停止條件、風險、M0 銜接

#### 4-1. 停止條件

1. 需要改權威、wire、FireGate 常數、late 修正上限、target、lead 或 backlog 門檻。
2. 既有斷言需要放寬：`MovementRecoveryTests` 的 Held／Actual、`worker_main`、far 測試、`PredictionTests.cpp:1241-1257`。
3. 有突變沒被殺，或既有突變變成 stale。
4. 權威 digest 不是 0 差分。
5. 前置核對失敗（照 3-5 節處理）。
6. 開發跑次的 100 ms 格出現重設或來回修正。
7. clean 出現 Cooldown，或 Jab1、Jab2、Jab6 不成立。
8. 有人要在只有 09a 的頭上量測。
9. 超出 time-box。
10. 新工具的乾跑不能重現 09R-map 或 08c L2 的已發表數字。

#### 4-2. 風險

1. 150 ms 只部分有效（已知限制，寫進文件）。
2. 抖動下會不會來回修正量不到：network 的 late 修正若為 0，B2 根本不會被觸發。
3. Pab7 支持度只有「中」：往後修的 slew 期間換算會讓 slack 變小（T-B9 釘住）。
4. x1／ramp 上 B2 沒有資料，ramp 可能觸發 D61 停止；開發跑次先看。
5. network40 故障解除時合計已到 90，餘裕只剩 15。
6. 固定起點下，跑次之間不完全獨立。
7. `worker_main` 的既有斷言偶發失敗（base 也有 1／20）。
8. token 規則有三份拷貝（code smell）。
9. Jab4 的上界：after 若像 D48③ 的 IPC 路徑那樣出現 32～33 ms 的撤回，會誤判失敗（見決定 6）。

#### 4-3. 和 M0、FireGate 凍結、C2 的銜接

- 前置條件：09ab-L2 通過，PR #73 的頭＝09a＋09b，之後才做 S3（M0）→ S4（凍結）→ C2。
  - （補充，2026-10-14，09ab-doc）：09ab-L2 依宣告 `passed`＝false（只有 Jab5 不成立）；D71 把它記為已知限制（1-7 節第 4 點），照建議繼續第 09 批。
- 重推條件 1、2 發生在凍結之前，不算重推（`09-firegate-c2.md:91`、`:92`）。
- **重推條件 11**（`09-firegate-c2.md:101`）的觸發是「D49 第二階段實作後」，目前沒有發生（補充，2026-10-14，D73：D49 第二階段移到 v7.1，所以在 v7 內不會發生）；08c 建議的改寫（`08c-drop-report.md:755`）尚未核准，本批不動它。本批的格點不記錄 FireGate 的 earliest、也不量 R 對 earliest，**不能代替 11 的 FireGate 那一半**。「M0 要不要加跑 Client→Gateway 短延遲注入」交使用者決定（決定 10）。
- **交給 M0 規劃的待決事項**（不在本批定案）：
  - late 修正之後的射擊怎麼處理。不當排除區；建議當成另行判定的類別，至少保留「Cooldown＝0」的檢查。
  - 依據：D48③ 的 2 次 Cooldown（base 的 chain-r2 與 09a-b2 的 chain-r5 各 1 次）都在 t_c 之後約 114～115 ms、late slew 期間、撤回之前（09b 文件「D48 第③步的結果」）。撤回期間 phaseShift 是正值，FireGate 偏保守（`FireGate.cpp:18-19` 的 `Max(0, phaseShift)`）；late slew 期間沒有這個保守，由 M0 確認。
  - 這 2 次 Cooldown 和 R8 一起作為 M0 的輸入。
  - （更正，2026-10-14，09ab-doc）：上面「撤回期間 phaseShift 是正值，FireGate 偏保守」不準確。09ab-rev §2-a：撤回期間 FireGate 扣掉的剩餘量正好等於相位實際多出的餘裕，是**準確**的，不是偏保守；問題在撤回結束後。
  - **（補充，2026-10-14，09ab-doc）撤回後的殘餘**：撤回後約 4 秒內（到下一次視窗決定）相位誤差約 3.3 ms，超過 guard 的 2 ms（`FireGate.hpp:24-27`），`earliest` 可能比實際晚 1 Tick（1-7 節第 2 點）。M0 要確認這段期間的影響；L2 宣告第 12 節第 3 點已列為 M0 的待決事項。
- C2 要記錄被釘住的秒數和 late 修正數。

---

### 5. 之後再評估（計畫外，本批不做）

1. 重新擷取用哪些樣本：M（只收 late 修正當下 tip 之後的命令）或 S（只收 slew 結束之後產生的命令），附第 1-2 節的機制證據與 70～87 ms 的邊界。
2. Match 改成在命令到達時就發布 slack 樣本。
3. late 修正的幅度改成依遲到量決定，或改變「連續 N 次」。
4. 依 D51④ 重推 backlog 門檻 105；延遲消失時的快速正向回收。
5. 短延遲加抖動的複合注入（量來回修正）。
6. network 案例改用每次不同的遺失種子。
7. token 規則抽成純函式。
8. 加跑 200 ms 的視窗，量出撤回的邊界。
9. 之後的模型宣告一律改用 ΔW 形式。
10. 為 150 ms 型事件定義並凍結「第一個反映延遲消失的樣本」的量測（讓 Pab4 可以變成預測）。
11. 各量測工具的時間基準統一（t_frame 與 measure.py 的 t_c 並存）。
12. Jab4 的上界改成結構上界（撤回量 < 該事件的修正量，即不過度撤回），取代分布上界。

---

### 6. 已知、只記錄

- Fisher 單側 p＝0.113 把同一跑次的兩位玩家當成獨立單位；改用跑次（flow 2／8 對 B2 0／8）是 0.233。0／9 的 95% 上界 28.3% 假設跑次獨立，但起點固定。
- `send_wait.txt` 沒有 downstream-1000ms 與 gateway-1000ms，「08c 被釘住 0」只涵蓋 7 個案例。
- 格點注入期間若碰到 D44 型的自發重設，會被算成注入造成的重設（機率低）。
- b2-extend 的「小問題」：`eabb107..HEAD` 在 docs 以外多出的是 `run_client_delay.py` 和它的測試 2 個檔。
- clean-30 在主機 8 ms 狀態下，修好的樹（08c 之後）沒有資料；08b L2（舊樹）有 8 ms 狀態的 clean-30，late 0（`L/pvp-v7-09b-plan-20261010/latency/aggregate.txt`），但不能借用。memory「v7 clock cross comparison」的 Client 端比較，若本次 L2 出現 8 ms 狀態的 clean-30，可以順便補做。
- D48③ 的撤回量 29.85～32.75 來自 Gateway→Match IPC 注入、08c 之前的樹、stopped session，只當旁證。

---

### 7. 需要使用者決定的地方

每一項都附建議、替代方案與代價。

1. **09a 和 B2 要不要放在同一批、只做一次合併的實機量測？**
   - 建議：同一批、同一次 L2（也就是 D48 的 (b) 路線，D44 的提前條件）。
   - 替代甲：先修 B2、09a 延後。代價：多一次 L2；而且 B2 的前提（送輸入不被釘住）不成立。
   - 替代乙：兩者都延後到 M0 之後。代價：M0、凍結、C2 都要做兩次，違反 D47⑤。
2. **late 修正後重新擷取，要用哪些樣本？**
   - 建議：照原型（收重新擷取開始以來的全部換算後樣本）。
   - 替代：只收 late 修正之後的命令。代價：+1～1.5 h 開發格點量測；100 ms 借來的門檻全部降為提議值；150 ms 的改善估計到不了實測的 87～101 ms，理由比上一版更弱。
3. **缺陷 A（舊樣本混進視窗）怎麼修？**
   - 建議：由換算一併取代，加一個 L1 測試釘住。
   - 替代：另外再做 A1。代價：效果重複，多一組突變。
4. **09a 的待決 2～5**：建議全部照 09a 文件的建議（比例折讓修正版、R＝80、Input(78,78)、這次不抽純函式）。
5. **commit 粒度**：
   - 建議：09a、09b 各一個 commit，只有 09a 的中間頭禁止量測。
   - 替代：壓成一個 commit。代價：中間頭和原型不再一一對應，審查時比對不方便。
6. **100 ms 撤回量的門檻**（Jab4）：
   - 建議：[28.5, 31.5] ms，兩側都取自 09R-map 原型（同一條注入路徑、修好的樹），餘裕各約 1 ms，標提議值交 D45；9 個跑次中至少 6 個有合格事件。
   - 替代：下界照建議，上界改成「撤回量小於該事件自己的修正量」（不過度撤回）。好處：不借分布，不會因為 32～33 ms 這種正常撤回誤判失敗（D48③ 的 IPC 路徑有 32.75）；代價：上界變寬，擋不住 31.5～33 ms 的偏移。
7. **150 ms 延遲怎麼判？**（Jab8）
   - 建議：沿用第 08c 批核准的事後規則，但明說它是借來的，只擋得住「每個單位都重設」這種整體惡化，或出現新的重設原因。
   - 替代甲：只記錄。代價：退步到「只有 09a」的程度也不會停下。
   - 替代乙：以原型的 7／12 為基準另定門檻。代價：需要新提議值與 D45 檢查，樣本只有 12 個單位。
8. **clean-30（30 FPS 閒置）怎麼判？**
   - 建議：只記錄，和同一次 session 的修之前並列、依主機計時器狀態分層；Cooldown 和故障窗外的非 respawn 重設仍是停止條件。
   - 替代：和同一次 session 的修之前比較，after 多於 before 就判失敗。代價：每棵樹只有 4 次，主機 8 ms 狀態下的行為未知，容易誤判。
9. **產生→執行延遲（G2E）的容許差 1.0 ms**：
   - 建議：保留 1.0 ms 作為提議值，只和同一次 session 的修之前比較；在 09ab-L2T 用同一支腳本對 08c L2 的 clean-60 重算兩棵樹的差，作為 D45 檢查的參考。
   - 替代：只記錄。代價：Client 端的延遲退步不會被擋下。
10. **M0 要不要加跑短延遲注入？late 修正之後的射擊怎麼判？**
    - 建議：交給 M0 規劃決定，本批只把它列為 M0 的待決事項；傾向加跑（因為本批量不到 FireGate 在 late 修正前後的行為），而且 late 修正之後的射擊當成另行判定的類別，至少保留「Cooldown＝0」。
    - 替代：M0 不加跑，等重推條件 11 觸發時再做。代價：FireGate 在 late 修正期間的行為，在凍結前沒有任何量測；D48③ 看到的 2 次 Cooldown 沒有後續驗證。
11. **合併 L2 的工具**：
    - 建議：新寫一份 driver 和判定腳本（`09ab-L2T`，+3.5～4.5 h），乾跑重現既有數字後凍結，交 D45。
    - 替代：把 09R-map 和 08c 的既有腳本各複製一份，只改樹與排程，分兩次 session 跑。代價：省約 1 h，但有兩份判定、兩次 D45、兩次閒置閘門，結果要人工合併。
12. **前置核對失敗時的處理**（3-5 節）：建議照本版的三類流程；有行為變更時，借自原型的門檻全部降為提議值並重新送核准。
13. **檔位**：照 3-1 節（D47⑪、09b 文件決定 9 的形式）：主力 high；09a 的 token 與清除點、09b 的換算交界、審查與兩次 D45 檢查用 xhigh；文件與執行 medium。xhigh 高於主對話檔位的部分需要你同意。

## 09ab-L2 的結果（2026-10-14 04:54～05:41，commit H `02c755c`；**依宣告 `passed`＝false：Jab5 不成立**）

證據：`build/target/_build/test/logs/pvp-v7-09ab-l2-20261014/`（`declaration.md` 與腳本的副本、`precheck-2.txt`（和第 1 次逐位元組相同）、`session/`、`session/judgement.json`、`summary-main.txt`）。132／132 completed，沒有停止、沒有中斷；凍結清單 49 檔開頭與結尾都吻合；binding 和宣告相同（after head `b205615`、main_base `b205615`、主 checkout `02c755c`、TREES_SHA `e3b0a9f0…`）。

| 判定 | 結果 |
|---|---|
| Jab1 | 成立（plan 核對、session 完整、sha） |
| Jab2 | 成立：ingress 132／132 passed；I1 130 passed、2 未判定（1.5％，非 EOF），每個（樹、案例）都有判定 |
| Jab3 | 成立：(a) after 被釘住 17 個案例全部 0（before 在 x1、ramp 與格點有 1～37）；(b) 32 個事件 W_post 最大 0.833 ms（≤1.0） |
| Jab4 | 成立：100 ms 三格 18 個事件撤回 29.71～30.08 ms，9／9 跑次有事件，不重設、不來回修正 |
| **Jab5** | **不成立**：80/150 的 r2、r3 兩個跑次，兩位玩家都串接（4 個單位）；其餘格點 0 串接 |
| Jab6 | 成立：clean-60 的 late、非 respawn 重設、Cooldown、被釘住都是 0；G2E 差 −0.012 ms |
| Jab7 | 成立：network0／20／40 after 的 late 修正 16／16 為 0 |
| Jab8 | 成立：停止與檢視旗標都沒有 |

**Jab5 不成立的那幾個單位**：
- 80/150 的 after：r1 不串接（放出到 t_post 22.6、22.7 ms）；r2、r3 串接，放出到 t_post 都是 **11.4～11.5 ms**（放出仍早於 slew 結束），串接後領先 84～87 ms，backlog 重設。
- 宣告的讀法只把「放出到 t_post ≤0」的串接寫成「被放出時刻觸發的已知限制」；這次是 >0 仍串接，**不在宣告的讀法之內**。
- 參考（只記錄）：09R-map 的 B2 原型（程式相同）在 80/150 是 18.8、10.9 ms 都沒有串接；before（flow，沒有 B2）在 80/150 是 6／6 串接。推論：δ＝80 ms 超過 70.6 ms 時，B2 只有在放出早於 slew 結束「足夠多」時才不串接，界線落在約 11 ms 附近、跑次間會變動。
- **使用者決定（D71，2026-10-14）**：照宣告記為不成立，寫成已知限制「δ＞70.6 ms 時，B2 要放出比 slew 結束早超過約 11～19 ms 才不串接（界線約 11 ms、跑次間會變動）」（plan 1-7 節第 4 點）；不另開修正批，S／M 留到 v7 完成後統一評估；繼續 09ab-doc 與第 09 批。

**格點（只記錄；單位＝總數／串接／backlog 重設）**

| 格 | before（flow） | after（09a＋09b） |
|---|---|---|
| 45、60、80 ms／100 ms | 18／0／7 | 18／0／0 |
| 45、60 ms／150 ms | 12／7／7 | 12／0／8 |
| 80 ms／150 ms | 6／6／6 | 6／4／5 |
| 45～80 ms／300 ms | 12／10／12 | 12／4／12 |

**其他（只記錄）**：x1 與 ramp 的 after 每跑次都有撤回（4～6 次），4／4 跑次有來回修正；非 respawn 重設兩棵樹逐案相同（x1：starvation 24＋backlog 6；ramp：16＋4）；after 的 x1 修正後送出等待中位數 3.54 ms（slew 期間的結構性等待，09ab-rev）；使用者旗標只有 before 的 x1-r2（run_case）。

## 09ab-L2 的事前宣告（2026-10-14 核准，D70：依 D69 照建議；依 D45 做了工具與宣告各 1 次 xhigh 檢查，修正已併入）

草稿原檔 `build/target/_build/test/logs/pvp-v7-09ab-l2-prep-20261014/declaration-draft.md`（sha256 `eb8282f5f2489d14871d8c439fb34e5386d77f1d5c32107486e7e85a6d799b00`）。字面值：JUDGE_SHA `ac5cdb5dbc6a457af80fc2078d75eb14e175a93afe0e163f0a3e404cc1881ded`、DRIVER_SHA `4d2ec0813f8368740ec5ea76924367d888797f8c2b0ed28d4476e5134a3ba7ef`、TREES_SHA `e3b0a9f0ec2fc7c8a5fff5eb16020f751123fa5f0421544d4e4e60eb89d86560`、凍結清單 `frozen-ab.sha256` `e695e0e6f85bf4a31dee42f03fca7cfc8c236607b7d86e0c6aa00828c53be668`（49 檔）；H＝本宣告的 commit。第 16 節 9 點全部照建議核准（第 9 點依 D45 檢查寫死條件）。下文的 `P/` 指那個目錄。

09ab-L2：09a（停頓後送輸入不再被釘住）＋09b（B2：late 修正後換算樣本、重新擷取一次）的合併實機量測。修之前（`eabb107`，08c 的 flow）對修之後（09a＋09b 的頭 `b205615`）兩棵樹，每棵 66 次、共 132 次。

- 狀態：**草稿・修正輪**（2026-10-14，09ab-L2P，1 位 high）。
  - 第 1 版：04:04～04:28；保留在 scratchpad 的 `declaration-draft.v1.md`（sha256 `74831f39…`）。
  - D45 的門檻來源對抗式檢查：1 位 xhigh，`P/xhigh-check-declaration.md`（腳本 `P/xhigh-check-declaration-scripts/`）。
  - 本版依該檢查第 6 節「送正式量測前必須修正」H1～H3、M1～M3 與「只記錄」L1～L6 修正，只改宣告文字；凍結工具、`trees-ab.json` 都沒動。對照表在第 17 節末。
  - 下一步：主對話整合 → 依 D69 第 16 節照建議核准。
- 下文 `L/`＝`build/target/_build/test/logs/`，`P/`＝`L/pvp-v7-09ab-l2-prep-20261014/`，`plan`＝`docs/object_fps_pvp/plans/v7/09b-phase-tracking.md`「09b-plan」。判定章節的全文（定義、程式位置）是 `P/judge-spec.md`（`bb387a84…`）；本宣告引用它，不重複全部的程式位置。
- 依據的頭：
  - 主 checkout `claude/pvp-v7-p2`，寫宣告時是 `82458399f9ada4283281538a28cde164398370d8`（乾淨）。修正輪時是 `26883b3`（主對話的 commit，只改 `docs/`）。
  - 工具的 commit（`main_base`）＝`b205615cf6b77dfd6d2024d1de9654aecc3025b3`；`b205615..8245839` 只改 `docs/`（已核對）。
  - 宣告提交後的 commit 記為 **H**。`main_base..H` 只能改 `docs/`；主 checkout 從 H 到 session 結束不得改動（driver 要求 `--main-head H`，每個跑次之前核對）。
- 依據的決定：D44、D45、D48（第②③步的做法與 `measure.py`、`measure3.py`）、D58、D60、D61（08c L2 的格式與 D44 事後規則）、D63④（late 用偵測器增量）、D64（09R-map 的格點與 plan 核對）、D65（ΔW 模型）、D66（09b-plan 核准）、D68（`Correct()` 只加註解）、D69（照建議）。
- 大綱：plan 3-4、3-5、4-1。先例：08c「L2 的事前宣告」（`08c-drop-report.md`）、09R-map 的事前宣告（`09b-phase-tracking.md`）。
- 數字沒有標「實測」的，是程式推導或推論。〔提議〕＝沒有直接來源、要交 D45 與使用者核准的值。

#### 1. 目的

確認 09a＋09b 的頭在實機上做到計畫的兩件事，而且沒有讓其他地方退步：

1. **09a**：Client 送輸入不再被釘住（Jab3）。
2. **09b（B2）**：100 ms 的短延遲之後完整撤回多推的 2 Tick、不重設、不來回修正（Jab4）；100／150 ms 不再串接（Jab5）。
3. **沒有退步**：資訊流守恆（Jab2）、閒置主機（Jab6）、抖動網路（Jab7）、08c 的故障案例與 150 ms 格（Jab8）。

通過的意思是「在這台主機、這些刺激與起點下，沒有例外」，不是一般網路下的頻率。

不在範圍：權威、wire、FireGate 常數、late 修正上限、target、lead、backlog 門檻都不改。150 ms 只部分有效、300 ms 不處理，是已知限制（plan 1-6），只記錄。

#### 2. 兩棵樹

| 樹 | worktree | commit | 內容 | 角色 |
|---|---|---|---|---|
| before | `/Users/karasu/Code/Source/GYO-Engine-l2-flow` | `eabb107e610a0423aa7c6903e37a9e3be424524b` | 08c（修好資訊流） | 判定（比較基準；每跑次的停止也適用，D61） |
| after | `/Users/karasu/Code/Source/GYO-Engine-l2-ab` | `b205615cf6b77dfd6d2024d1de9654aecc3025b3` | 09a＋09b 的頭（PR #73） | 判定 |

- before 沿用 08c L2 的 worktree 與建置，不修改；執行檔 sha 寫死在 `judge_ab.BEFORE`（＝`L/pvp-v7-08c-impl-20261011/l2p-trees/artifacts.sha256` 的 flow 列）。
- after 是本批新建（`P/tree/`）：
  - `git worktree add --detach ../GYO-Engine-l2-ab b205615`，worktree 乾淨。
  - 乾淨建置：`P/tree/build-trees.sh ab`（09R-map `tree/build-trees.sh` 的複本，只改輸出目錄與註解），Match、Gateway、action／timing／network／worker probe。2026-10-14 04:05:09～04:19:55（約 15 分），exit 0，編譯錯誤 0（`P/tree/build-ab.log`；configure log `build-ab-configure.log`）。和 09R-map 一樣，host-tools 的 dxc 在建置中 clone（先例的 log 也有）。
  - Gateway：`vcs.revision=b205615cf6b77dfd6d2024d1de9654aecc3025b3`、`vcs.modified=false`（go1.27.1）（`P/tree/gateway-vcs.txt`）。
  - 建置後 worktree 仍乾淨（`git status --porcelain` 0 行），HEAD＝`b205615`。CTest 本批沒有跑（09ab-L1 已在主 checkout 的 `b205615` 同內容上通過）。
- `eabb107..b205615` 在 `docs/`、`tests/` 以外的差分只有 Client 端（`ClientConnection.cpp`＝09a、`LocalPlayerPrediction.{hpp,cpp}`、`Movement.hpp`＝09b）與驗收的 `worker_main.cpp`、`run_client_delay.py`（＋測試）。Match 與 Gateway 的程式相同（09ab-rev 2-d：Match 執行檔沒有 `LocalPlayerPrediction::`、`ClientConnection` 的符號）；執行檔雜湊不同只來自建置路徑與 Gateway 的 `vcs.revision`。`run_client_delay.py` 一律用主 checkout 的（它在凍結清單裡）。
- 執行檔 sha256（完整值寫在 `P/trees-ab.json`；driver 每個跑次之前核對；after 的完整清單 `P/tree/artifacts.sha256`）：

| 樹 | Match | Gateway | action probe | timing probe |
|---|---|---|---|---|
| before | `8d154a3a117ddf76746c8d8feed4beecc8a7d375f61ea8c5c16f1c3cfa4e98c0` | `4f0caaca8eae0c533e6a0f8d00cb5475a287d81bf25b76fd728b5c9d377ffb8f` | `4949302f1283b71332d31f54165a1ecec6d9f19d479a5ca6557c7a862aaf470c` | `ea5c569eacc8cc27f87e21e0dffec3a64a68b10df8ca6e876276a475ea9ece21` |
| after | `02b1b09f6f73d2402fb1fc1534c47671261c64593a1a0617f5f106e199d2ae3e` | `bbe29889ed3fa5f9f6616a0879edabdbd99778d08feef741e850e9b870bb74d6` | `fe3fd799aff8a25cd5162a9628d19657d6ba198dc0e0b9016bb1f795f7dd3e3d` | `25eac43a56708bbc00fd0a14f3f5415c886e02003c79c7caad71d9464bbd0d7d` |

（after 的 network probe `8e66db4e…`、worker probe `c5068866…` 也建了，L2 不用，只列在 `P/tree/artifacts.sha256`。）

- **`P/trees-ab.json`**（sha256＝TREES_SHA，第 3 節）：`after.worktree`＝`/Users/karasu/Code/Source/GYO-Engine-l2-ab`、`after.head`＝`b205615cf6b77dfd6d2024d1de9654aecc3025b3`、上表 after 的 4 個 sha、`main_base`＝`b205615cf6b77dfd6d2024d1de9654aecc3025b3`。09ab-L2T 的草稿（after head `bb9ffa1`、執行檔 null、worktree `…-l2-after`）已被覆寫。

- arena：主 checkout 的 `assets/object_fps_pvp/pvp_arena.json`，`0026013c731a7d7317ce6be4f84f897b91fa1b807c9621d60fb608570f1775a5`。
- 本批不重跑權威 digest：09ab-L1 已對 `03e99e2` 比過 scale 1、3 各 35 情境 0 差分；B2 與 09a 的差分不碰 Match。

#### 3. 工具與凍結清單（字面值）

| 檔 | sha256 | 用途 |
|---|---|---|
| `P/run_ab.py` | `4d2ec0813f8368740ec5ea76924367d888797f8c2b0ed28d4476e5134a3ba7ef` | driver（＝`judge_ab.DRIVER_SHA`）＝**DRIVER_SHA** |
| `P/judge_ab.py` | `ac5cdb5dbc6a457af80fc2078d75eb14e175a93afe0e163f0a3e404cc1881ded` | 判定＝**JUDGE_SHA** |
| `P/trees-ab.json` | `e3b0a9f0ec2fc7c8a5fff5eb16020f751123fa5f0421544d4e4e60eb89d86560` | after 樹的宣告值＝**TREES_SHA** |
| `P/sleeper.py` | `98105dbf71a5818d20bb2c82083f7a3cbbdb9c236b43514537c45845442a9567` | 08c L2 的照抄（`judge_ab.SLEEPER_SHA`） |
| `P/frozen-ab.sha256` | `e695e0e6f85bf4a31dee42f03fca7cfc8c236607b7d86e0c6aa00828c53be668` | 凍結清單 49 檔（`judge_ab.FROZEN_LIST_SHA`） |
| `P/frozen/decompose.py` | `aa69db2976af84a4e3a5ba71505689f26c110a55e20f41c997680857d4ffc981` | G2E（`L/pvp-v7-09b-plan-20261010/latency/decompose.py` 的副本） |
| `P/frozen/send_wait.py` | `d12cee323f631d5aa5dedf65fc258e9f89518aa78561f5dbcf37d94d9ffefaca` | 被釘住（`L/pvp-v7-09r-eval-20261011/scripts/send_wait.py` 的副本） |
| `P/judge-spec.md` | `bb387a84f671caff29433503885cfe562b3273bf7945d80cb7a1029743461a01` | 判定章節 |

- **凍結清單**（49 檔）＝09R-map 的 47 檔（08c L2 的 44 檔＋D48 偵測器 `445271ce…`、`measure.py` `045714f1…`、`measure3.py` `18ffde15…`）＋上表的兩份副本。2026-10-14 在 `8245839` 上 `shasum -a 256 -c` 49／49 OK。`run_network.py` 和往例相同只受 git 狀態核對保護。
- 和以前的 L2 相同：所有 runner 與分析器都從主 checkout 執行，兩棵樹用同一份工具；每棵樹用自己的 Match、Gateway 與 probe。
- **執行時一律用本表的字面值**（`--judge-sha`、`--driver-sha`、`--trees-sha`），不從當下的檔案計算（09ab-L2T xhigh D：從當下檔案算會讓核對變成循環）。
- 09ab-L2T 的乾跑（凍結前）：09R-map 469 項、08c L2 45 項逐值相同；正向 mock 通過；負向 mock 15 種全部被拒（`P/report-l2t-fix.md`）。

#### 4. 主機

- 照 08c L2 與 09R-map：session 期間不做開發；`caffeinate -dims`；前景沒有其他使用者程式；**暫停 PR #73 的 auto-fix**；背景的其他 agent 與分析先停。
- **閒置閘門**（`--idle-gate`，正式執行必須帶）：第一個項目之前連續 3 次 5 秒 sleeper，每次最大值 <10 ms；不成立每 30 秒重試，最多 30 分鐘，仍不成立就停下。
- 每輪（6 輪）前後各記 1 次 5 秒 sleeper 與前 5 名 CPU 程序（`host.jsonl`）。
- 子程序：每個都跑在自己的 session，終端機的 Ctrl-C 只打到 driver；第一次停止訊號：跑完目前的項目後停下；第二次：0.5 秒內結束目前的項目。逾時（300 秒）先 SIGTERM，15 秒後對整個 process group 送 SIGKILL；項目結束後 group 裡留下的程序一律殺掉並記錄。
- **TimerBaseline 分層（只記錄）**：每個跑次記 probe 的 `timer_baseline`；`late_p99_ms` ≥6.0 ms 記為 8 ms 狀態（第 05 批的定義，`L/pvp-v7-09b-plan-20261010/latency/aggregate.py:7`），同時記 `interval_p99_ms` ≥22 ms（`L/pvp-v7-cross-20261009/cross.py:44`）與兩者的一致率。分層只用於解讀與並列（clean-30／60、G2E、Jab4 的事件依狀態分欄），**不是判定、門檻或分母**。它是 probe 的代理值，不是 role 自己的晚醒。
- **整個 session 可能全部落在 8 ms 狀態**（只記錄）：開發跑次有 TimerBaseline 的 8 次全部是 8 ms 狀態，sleeper 7.1～8.0 ms 也低於閒置閘門的 10 ms，所以分層可能只有一側。

#### 5. 案例、次數、順序與時間

每棵樹 66 次、合計 **132 次**（`judge_ab.CASES`；`run_ab.py --list-only`）：

| 案例 | 每樹次數 | runner | probe | 參數／判定上的角色 |
|---|---|---|---|---|
| clean-60 | 4 | `run_gameplay.py --case clean-60` | action | Jab6 |
| clean-30 | 4 | `run_gameplay.py --case clean-30` | action | 只記錄（Jab3(a) 的範圍內） |
| x1-δ-100（δ＝45、60、80 ms） | 各 3 | `run_client_delay.py --profile x1 --delay-ms δ --window-ms 100 --fault-at 12.0` | action | Jab3(b)、Jab4、Jab5 |
| x1-δ-150 | 各 3 | 同上，`--window-ms 150` | action | Jab3(b)、Jab5、Jab8（grid-150） |
| x1-δ-300 | 各 2 | 同上，`--window-ms 300` | action | 只記錄（Pab5；Jab3(a) 的範圍內） |
| network0 | 4 | `run_gameplay.py --case network0` | action | Jab7、Jab8 |
| network20 | 6 | `run_gameplay.py --case network20` | action | Jab7、Jab8 |
| network40 | 6 | `run_gameplay.py --case network40` | action | Jab7、Jab8 |
| downstream-1000ms | 4 | `backpressure_probe.py --layers downstream --durations-ms 1000` | timing | Jab8 |
| downstream-respawn-1000ms | 2 | `run_gameplay.py --case downstream-respawn-1000ms` | action | Jab8 |
| gateway-1000ms | 2 | `backpressure_probe.py --layers gateway --durations-ms 1000` | timing | Jab8 |
| x1 | 6 | `run_client_delay.py --profile x1 --delay-ms 200 --window-ms 2000 --fault-at 7.5` | action | Jab8 |
| ramp | 4 | `run_client_delay.py --profile ramp --peak-ms 250 --rise-ms 1500 --fall-ms 500 --fault-at 7.5` | action | Jab8 |

- `run_client_delay.py` 一律帶 `--frozen-list P/frozen-ab.sha256`（自己再核對 49 檔）。
- 格點的起點 12.0 s 沿用 D64 的理由（被延遲的那一發之後 2.25 s 沒有射擊，D48③ 的 Cooldown 機制在結構上不會發生；12.0～15.4 s 碰不到 respawn）。x1、ramp 照 08c L2 的 7.5 s。
- **順序**：6 輪，每輪 22 項。第 k 輪依 `CASE_ORDER`（clean-60、clean-30、9 個格、network0／20／40、downstream-1000ms、downstream-respawn-1000ms、gateway-1000ms、x1、ramp）跑「輪次含 k」的案例；每個案例兩棵樹連續跑，起頭的樹依 (rep−1＋案例序號) mod 2 輪替。
  - 起頭次數：合計 before 33／after 33；偶數次的案例 1:1；3 次的格點每格是 2:1 或 1:2，**100 ms 三格合計 before 先 5、after 先 4，150 ms 三格反過來 4:5**（Jab4 的 9 個 after 跑次裡有 5 個是第二個跑的；只記錄）。
  - 輪次：clean-60、clean-30、ramp 在第 1、3、4、6 輪；network0 第 1、2、4、5 輪；network20／40、x1 每輪；downstream-1000ms 第 2、3、5、6 輪；downstream-respawn、gateway 第 2、5 輪；100／150 ms 的格交替在 {1,3,5} 或 {2,4,6}；300 ms 的格各 2 輪。
  - 不隨機化、不補跑。完整清單：`python3 -I -B P/run_ab.py --list-only`（`P/dryrun/list-only.txt`）。
- **時間**：依開發跑次的實測（`P/dev/progress.jsonl`）：action probe 的跑次每次 19.8～21.8 秒、backpressure 13.4 秒；每次 `judge_ab.evaluate()` 約 1～2 秒，狀態核對約 0.5 秒。
  - 120 個 action probe 跑次 ×約 22.5 秒＋12 個 backpressure ×約 15.5 秒 ≈ 48 分；每輪前後的 sleeper 12 次 ×約 5.2 秒 ≈ 1 分；閒置閘門 ≥15 秒；結尾的 send_wait、decompose 與 `--session` 判定約 1～3 分。
  - **合計約 50～55 分**（plan 的「約 60 分」之內）。閒置閘門要等時最多再加 30 分。準備與收尾（步驟 1～4、6、7）約 10 分。

#### 6. 注入的有效性（D60）

- Client→Gateway 的格點、x1、ramp：`run_client_delay.py` 自己的事前檢查（`profile_problems`）與每輪核對（`round_problems`：延遲欄位相符、延遲包 ≥1、開始與結束時收＝送、佇列清空、沒有無法路由的包與 relay 錯誤、佇列 <192、送出延遲 p99 ≤10 ms、Gateway 速率拒絕＝0）。任一不成立記 `injection_invalid`，session 停下。和 09R-map 相同（09b 文件「09R-map 的事前宣告」第 6 節）。
- backpressure 兩案與 network 三案：照 08c L2。`run_case` 的遊戲判定只記錄（D60）；`passed` 為 False 列成使用者旗標（第 10 節）。

#### 7. 量的定義（摘要；全文 `P/judge-spec.md` 第 1 節）

- **late 修正**：凍結 D48 偵測器的 P 事件，計數器**增量的總和**（D63④；不是 `judge_l2.py` 的最大值）。P 讀產品自己的計數器，只有 `Correct(.., late=true)` 會加（`LocalPlayerPrediction.cpp:114`）；B2 的重新擷取走 `Correct(error, false)`（after 樹 `:167`，`0e1fabb` 是 `:160`），不會被算成 late。
- **合格事件**：`measure3.py` 的 `eligible_v3`（跑次 clean_v3、放出早於 slew 結束、in_span、qualifies_v3、沒有設限、W_post 與 lead_after 算得出），和 09R-map 相同。
- **W_post（slew 結束後的口徑）**：該玩家在 [t_post, t_post＋0.25 s) 產生的命令，送出等待的中位數；t_post＝late slew 最後一個命令的產生時刻（`measure.py:29`、`:357`）。slew 期間的平衡等待 3.33 ms 不在視窗裡（09ab-rev §7-2）。
- **撤回量**：late 修正後 1 秒內、下一次非 respawn 重設之前，`phase_corrections` 增加而 `phase_late_corrections` 不變的那一步的位移（`measure3.py:42`、`:353-367`）；**殘餘量**只記錄。
- **串接單位**：每位玩家在 span 裡的第一個 tracking P 事件；增量 >1、0.5 s 內同玩家還有 P 事件、或 span 裡有 >1 個事件，就是串接。
- **被釘住**：`send_wait.py`，每位玩家每秒（≥10 個命令）送出等待的中位數 >2 ms，算 1 個玩家·秒；只涵蓋 action probe 的 runner。
- **G2E**：`decompose.py`，action-client.json 的 [start_ns, end_ns) 內產生、以 actual 解析的非 seeded 命令，resolved−generated；每位玩家的 P50，取 (跑次, 玩家) 的中位數。

#### 8. 判定（plan 核對、session 完整、Jab1～Jab8 全部成立、而且沒有任何跑次帶停止旗標，才通過）

由凍結的 `judge_ab.py --session DIR --trees-sha TREES_SHA` 機械地給出（`judgement.json`）。任何一條「無法判定」都讓 `passed`＝false，交使用者；不補跑。

| 代號 | 白話 | 條件（`judge_ab.py` 的位置） | 門檻 | 門檻的來源（建置、指標、範圍） | 提議？ | 開發跑次（只記錄、不是來源） |
|---|---|---|---|---|---|---|
| Jab1 | session 是宣告的那一個 | plan.json 的 declared＝`DECLARED`、schedule＝重算值（132 項）、`state.driver`＝DRIVER_SHA、`state.judge`＝本檔 sha、trees 文字的 sha＝`--trees-sha`（**第 3 節的字面值**），而且 after 的 head／執行檔和 state 一致、凍結清單 sha＝`FROZEN_LIST_SHA`；session 完整（132 項全部完成、沒有中斷或停下、凍結清單頭尾 OK、結尾狀態＝開頭）。`judgement.json` 的 `binding` 印出 trees.after.head、main_base、state.main.head，**判完由人和本宣告對照**（after head＝`b205615…`、main_base＝`b205615…`、main head＝H） | — | 09R-map 的 plan 核對（`L/pvp-v7-09r-map-prep-20261011/measure_map.py:343-354`、D64）；08c xhigh B／C；09R-map xhigh M3；09ab-L2T xhigh D | 否 | — |
| Jab2 | 資訊流守恆沒有壞 | 每個完成的跑次 ingress＝passed、沒有 ingress／d59／j5a／relay 的停止；**D61 的 J2**：沒有 I1 failed，每個（樹、案例）至少 1 次 I1 有判定；輸出 I1 未判定的比例與原因（D58(7)）（`# Jab2`，`:760`） | 0 次；每組 ≥1 次 | D61、D64；`L/pvp-v7-08c-l2-prep-20261011/judge_l2.py:373-378`。乾跑：08c 修好的兩棵樹 74 passed／2 undetermined（0.026），09R-map 63 次 0 undetermined | 否 | 9 次全部 ingress passed、I1 passed |
| Jab3 (a) | 送輸入不再被釘住 | `send_wait.py` 的每個 (案例, after) 列，被釘住玩家·秒＝0；after 的每個 action probe 案例都要有列，缺列＝無法判定。**範圍**：action probe 的 17 個案例，**包括** clean-30 與 300 ms 格（這兩者其他地方只記錄）；**不含** backpressure 2 案 | 0 | 實測：08c L2 flow+09a 7 個案例為 0（`L/pvp-v7-09r-eval-20261011/send_wait.txt`）；09R-map 的 B2 原型 9 個格點為 0（09R-map session `L/pvp-v7-09r-map-20261011/session/` 的 B2 跑次，經 `send_wait.py` 副本重算；09ab-L2T 的 `mock/full` 以這些跑次為輸入）。**clean-30、x1、ramp 在 B2 上，除了開發跑次之外沒有資料**；0 是 09a 的設計要求（送出不被釘住），不是從分布推出來的值。結構上的餘裕（D45 xhigh L5）：被釘住要某位玩家某一秒 >50% 的命令等待 >2 ms，已量到的最大比例是 09a-only 的 ramp 0.32～0.41、B2 的 300 ms ≤0.29、150 ms ≤0.18 | 否 | after 的 7 個 action probe 跑次（100 ms 三格、x1、ramp、clean-30、clean-60）與 before clean-30 全部 0 |
| Jab3 (b) | late 事件之後送出不再等待 | after 的 100／150 ms 格，**in_span、沒有串接、W_post 算得出的 P 事件**（合格事件是它的子集；串接由 Jab5 判），每個 W_post ≤1.0 ms；沒有事件＝無法判定（`# Jab3 b`，`:796`） | 1.0 ms（`:206`） | 09R-map B2 原型、同一母體：28 個事件最大 **0.192** ms（合格 18 個最大 0.184；`P/dryrun/r09map/report.txt`），餘裕約 5 倍；09R-map flow+09a 合格 21 個最大 0.217（`L/pvp-v7-09r-map-20261011/summary-main.txt:1`）；D48③ 0.156 旁證。1.0 本身出自 D48③ 宣告的預測。**口徑**：slew 結束後（第 7 節），slew 期間的 3.33 ms 不在內 | **是** | after 100 ms 三格 6 個事件 0.062～0.127 |
| Jab4 | 100 ms 完整撤回 | after 的 100 ms 三格，每個合格事件：撤回量 ∈ [28.5, 31.5] ms、`held_post`＝0、沒有 backlog 重設、[t_c, t_c＋3 s] 與撤回後沒有非 respawn 重設、沒有來回修正；這 9 個跑次每一個在 [start, release＋3 s] 沒有非 respawn 重設、沒有來回修正；9 個跑次中 ≥6 個有合格事件，否則「無法判定」（`# Jab4`，`:811`）。撤回量 >31.5 但小於該事件自己的修正量（結構上界）時**仍判不成立**，交使用者，結構上界的結果並列 | [28.5, 31.5] ms（`:207`）；≥6 跑次（`:208`） | 撤回量：09R-map B2 16 個事件 29.54～30.45 ms（`summary-main.txt:22-24`，乾跑逐值重現），兩側餘裕 1.04／1.05；同一跑次的兩位玩家幾乎相同，**獨立的單位是 8 個跑次**。**中心值 30.0 有機制來源**：33.33−P·(1−60/75)＝33.33−3.33（09ab-rev §1-a、§7-1，`L/pvp-v7-09ab-impl-20261013/rev/xhigh-review.md`）。依 TimerBaseline：8 ms 狀態 5 跑次 10 個事件 29.54～30.26，4 ms 狀態 3 跑次 6 個事件 29.62～30.45（上偏沒有出現；見第 11 節 Pab2 的敏感度）。`held_post`：`measure3.py:56`、`:377`，D48③ 的 J2d。最少跑次：沿用 D64 的 Jm2（`measure_map.py:114`），不是推導 | **是**（撤回範圍）；**沿用 D64／提議**（≥6 跑次） | after 100 ms 三格 6 個事件 29.76～30.10，全部 8 ms 狀態 |
| Jab5 | 100／150 ms 不串接 | after 的 100／150 ms 格，串接單位＝0；沒有任何單位＝無法判定（`# Jab5`，`:848`） | 0 | 09R-map B2 0／28 單位（`summary-main.txt:13-18`，乾跑重現）。不要求 clean_chain（比來源保守）。**放出到 t_post 的餘裕**：09R-map B2 150 ms 三格是 10.9～18.8 ms（80/150：18.8、10.9；`P/dryrun/r09map/x1-*-150-r*-flow09a-b2.judge/record.json` 的 `release_to_t_post_ms`），不到 1 Tick（plan Pab3 的「約 40 ms」出自 `L/pvp-v7-09b-plan-20261011/adversarial.md:50`，和實測不符，撤回） | 否 | after 100 ms 三格 0／6 單位串接（150 ms 沒有跑） |
| Jab6 | 閒置主機沒有退步 | clean-60 after：每個跑次 late＝0、非 respawn 重設＝0、Cooldown＝0；send_wait 的 (clean-60, after) 被釘住＝0；G2E 合併中位數 \|after−before\| ≤1.0 ms，兩邊都來自本 session、每個跑次 2 列（`# Jab6`，`:855`） | 0；1.0 ms（`:209`） | 0：08c L2 的 flow、flow+09a 各 4 次（乾跑重現）；09R-map 的 B2 clean-60 前置 1 次。G2E：08c L2 clean-60 重算 flow+09a−flow＝−0.110 ms、flow−before＝+0.097 ms，樹內範圍 flow 37.692～37.978、flow+09a 37.531～37.979（`P/dryrun/g2e/report.txt`）；1.0 約是樹間差的 9 倍、樹內寬度的 2～3 倍。參考值只有 09a，B2 在 clean 沒有資料 | **是**（1.0 ms） | after clean-60 1 次：四個 0 |
| Jab7 | 抖動網路下沒有 late 修正 | after 的 network0／20／40 每個跑次 late＝0（`# Jab7`，`:890`）；同時是每跑次的立即停止，停下交使用者並附 T-B9 的行為（第 11 節 Pab7） | 0 | 08c L2 flow、flow+09a 32／32 跑次為 0（乾跑重現）。B2 沒有資料，支持度「中」 | 否 | 沒有跑 |
| Jab8 | 故障案例與 150 ms 沒有整體惡化 | (1) D61 的 D44 事後規則（`judge_l2.py:341-449` 原樣搬到 `d61_rule()`，`:633`），單位＝每位玩家的非 respawn 重設事件，after 對 before：before 沒有的原因在 after ≥2 個跑次出現、或每跑次平均多 ≥1 個事件 → 停止旗標；只在 1 個跑次 → 給使用者的旗標。組＝network0／20／40、downstream-1000ms、downstream-respawn-1000ms、gateway-1000ms、x1、ramp 各一組，150 ms 三格合併為 `grid-150`（`:195`）。(2) **上限（grid-150）**：after 的 150 ms「全部單位都重設」，或每跑次事件 ≥2.0 → 停止旗標，**不看 before**（`# CEILING`，`:900`）。(3) 每格各自的 D61 比較：只記錄 | (1) 2 個跑次；1 個事件／跑次。(2) 2.0 事件／跑次（`:210`） | (1) D61（08c 的故障案例是原本的適用範圍；**150 ms 是借用**：D61 是在三棵樹逐案相同的案例上核准的，150 ms 預期兩棵樹不同）。(2) 為什麼要上限：150 ms 每位玩家最多重設 1 次，after 每跑次的上限約 2.0，「多 ≥1」只有在 before ≤1.0 時才可能觸發；before 9 跑次 18 單位、每單位 p＝0.5（09R-map flow 6／12）時，before >1.0 的機率＝P(Bin(18, 0.5) ≥10)≈**0.41**，這時連「只有 09a」型的 18／18 全部重設也擋不住（09ab-L2T xhigh A，mock `jab8ceiling` 重現並修正）。2.0＝plan 3-4 自己寫的靈敏度（「每個單位都重設，等於只有 09a 沒有 B2」）。B2 原型 7 個事件／6 跑次＝1.17（7／12 個單位）、09R-map flow 6 個事件／6 跑次＝1.0（6／12 個單位），都在上限之下 | (1) 否；**(2) 是** | x1、ramp 的重設和 08c flow／flow+09a 逐項相同；downstream-1000ms 相同 |

- **Jab4 的讀法**：上界 31.5 對 role 的晚醒抖動敏感（j >1.5 ms；第 11 節第 2 點）。撤回量 >31.5 而且小於該事件的修正量時，判定照樣是「不成立」，但交使用者時並列「結構上界（不過度撤回）成立」、**該跑次 slew 期間的產生間隔偏差**（`clients-commands.jsonl` 的 generated 間隔對 13.333 ms 的偏差；用它取代或補充計時器狀態）與計時器狀態，由使用者判斷是退步還是晚醒。
- **Jab4 的不合格候選**（D45 xhigh L6）：合格事件若被 1～3 s 內的其他修正設限，就不再受判（最多可藏 3 個跑次）。100 ms 的不合格候選事件（`not_eligible_candidates`）中，撤回量落在 [28.5, 31.5] 之外的，列為使用者旗標（讀法；判定不改）。
- **Jab5 的讀法（80/150）**：80/150 的串接單位若 `release_to_t_post_ms` ≤0（放出晚於 slew 結束），判定照樣是「不成立」；交使用者時標明這是「>70.6 ms 的已知限制，被放出時刻觸發」，並附上該單位的 `release_to_t_post_ms`（每跑次 `events` 裡已有）。
- **Jab3(a) 的範圍**：只宣稱 action probe 的 17 個案例。backpressure 兩案其實有 `clients-commands.jsonl`，缺口來自凍結的 `send_wait.py` 只看 gameplay 與 client_delay（和 08c 的缺口相同），不是資料不存在。Jab3(a) 只輸出被釘住的秒數，等待 >2 ms 的比例從 0.3 升到 0.49 這種退化看不出來（D45 xhigh 的測試缺口；每玩家·秒的最大比例建議之後列為只記錄，本次判定不改）。
- **讀法的限定**：起點固定（格點 12.0 s、x1／ramp 7.5 s），跑次在相位上不完全獨立；格點只是條件暴露，推不出實際網路下的頻率；0 違反只代表「本條件下沒有例外」。**network 的遺失是固定種子**（`build/acceptance/object_fps_pvp/impaired_network.py:156-157`；`08c-drop-report.md:728`），Jab7 的 0 只代表這個遺失樣式；08c 的 32／32 與本次的 16 次都是同一個情境的重複量測，不是獨立樣本。

#### 9. 每個跑次的立即停止（停下，保留全部證據，不重跑，剩下的記 not_run，交使用者決定）

`judge_ab.evaluate()` 在每個項目結束後執行，`stops` 不是空的就停下 session（`P/judge-spec.md` 第 2 節）。**兩棵樹都是修好資訊流的樹，每跑次的停止對兩棵樹都適用**（D61；08c 的只記錄樹是 `c2b76cd`，不在本批）。

| 代號 | 條件 | 適用 | 來源 |
|---|---|---|---|
| ingress | `ingress_evidence.py` 的 status 不是 passed（failed、undetermined、stopped、missing） | 兩棵樹、全部案例 | D61；`judge_l2.py:294-296` |
| d59 | link_shots 不是 passed、carried≠lines 或 lines＝0；action_drops 任一側不完整 | 兩棵樹、全部案例 | D61；`judge_l2.py:297-302` |
| j5a、relay | x1、ramp：never_arrived＞0、送出卻沒到＞0 或判不出、late_first＝0；relay 的上行轉出≠Gateway 收到 | 兩棵樹、x1／ramp | D61；`judge_l2.py:303-309` |
| d44 | 格點以外：故障窗 (start, release＋1.5 s] 之外的非 respawn 重設；clean 沒有故障窗，出現就停 | 兩棵樹、格點以外 | D61；`judge_l2.py:310-315`。格點照 D64 只記錄（窗 [start, release＋3 s]） |
| cooldown | clean-60／clean-30 出現 Cooldown 拒絕（rejection＝3） | 兩棵樹 | plan 4-1 第 7 點；`Combat.hpp:52` |
| jab6 | after 的 clean-60 有 late 修正 | after | plan 4-1 第 7 點 |
| jab7 | after 的 network0／20／40 有 late 修正 | after | plan 3-4 Jab7（停下交使用者，附 T-B9） |
| analysis | 偵測器或 measure3 丟出例外；action probe 的跑次缺 phase 計數器（否則 late 會讀成 0）；格點跑次沒有故障窗（否則 Jab4 的跑次層級檢查會變空） | 兩棵樹 | 09R-map 的作法；後兩條是 09ab-L2T 新增（xhigh E） |
| driver | runner 錯誤（缺 case 目錄或必要的 log、`result.json` 沒有 artifacts、backpressure 的 failure.json）、注入無效（第 6 節）、逾時、第二次 Ctrl-C、狀態改變（主 checkout 的 HEAD 或 porcelain、兩棵 worktree、任何執行檔、arena、凍結清單的任何檔、judge、driver、sleeper、trees 檔與開頭不同）、sleeper 失敗、閒置閘門 30 分鐘不過 | — | `run_ab.py` 的 docstring |

- 第一次 SIGINT／SIGTERM：目前的項目跑完後停下（記為中斷，session 不完整）。
- 注入跑次（格點、x1、ramp、network、backpressure）的 Cooldown 只記錄（和 08c L2、09R-map 相同）。
- 停下之後不補跑；`judge_ab.py --session` 照樣執行，結果是 `passed`＝false，交使用者。

#### 10. 只記錄

`judgement.json` 的 `*_recorded` 等欄位（`P/judge-spec.md` 第 4 節）：

- **TimerBaseline**：每個跑次的兩種判法與一致率；clean-30／clean-60 的 late、重設、Cooldown、被釘住、G2E，依樹與狀態並列（`clean_recorded`）。**clean-30 只記錄**（修好的樹上目前只有本批開發跑次的資料；Cooldown 與故障窗外的非 respawn 重設仍是停止條件）。若出現 8 ms 狀態的 clean-30，順便補 memory「v7 clock cross comparison」的 Client 端比較。
- **格點地圖**（兩棵樹 × 9 格）：每跑次 late（全部與 in_span）、單位、串接、依原因的重設、size_v3 合計、q30_max、lead_after_max、W_post、`held_post`、撤回量與殘餘量、放出－t_frame（Pab4）、來回修正的跑次、放出時刻、佇列、計時器狀態（`grid_recorded`）。300 ms 格（Pab5）只在這裡。
- **殘餘量**：撤回後的殘餘。`measure3` 的 `residual_ms` 預期約 −3.3（09R-map −2.9～−3.8、開發跑次 −3.22～−3.55）；**`residual_ms`≈−3.3 就是「相位偏早 3.3 ms」**（第 11 節第 1 點）。
- **ΔW 模型**（D65；事後得出、本次才預先宣告）：每個合格事件 `lead_after−(lead_before＋2T−(W_post−W_pre_1s))`（`dW_model_diff`）。**指定 W_pre_1s**。09R-map 事後的 0.332 ms 重現不了，同樣 43 個事件重算為 **0.319** ms（09ab-L2T 與其 xhigh 都只得到 0.319）。
- **Jab4 的附帶紀錄**：結構上界替代（撤回量 < −size_v3）的違反清單、每個事件所在跑次的計時器狀態、不合格的候選事件。
- **Jab8 的附帶紀錄**：`grid_150_ceiling` 的數字（單位、重設單位、事件／跑次）與每個 150 ms 格各自的 D61 比較（補償三格合併的稀釋）。
- **Jab8 上限的讀法**（D45 xhigh L4）：以原型 p＝7／12 計，18／18 全部重設的機率約 6e−5、D61 的「多 ≥1」誤停約 0.8%；若真實的 p 接近 7／12 的 95% 上界 0.85，兩者合計約 24%（主要來自 D61，不是上限）。「≥2.0 事件／跑次」那條算的是**整個跑次**的非 respawn 重設，不限格點視窗；觸發時列出多出來那些重設的原因與時刻。
- **x1／ramp／network**：每跑次 late、撤回次數、來回修正、依原因的重設、send_wait 的修正後等待（x1／ramp 的 W_post；**這裡的口徑含 slew 期間**，所以 flow+09a 是 3.47～4.92 ms，不套 Jab3(b)）；ramp 的來回修正數與 late 數（Pab8）。
- **Jab2**：I1 的狀態分佈、未判定比例與原因（D58(7)）。
- **使用者旗標**：完成的跑次中 `run_case` 的 passed 為 False（frames 可能提早結束，late 會少算；08c 與 09R-map 修好的樹全部是 True）。**本批開發跑次的 after x1 是 False**，原因是換彈在 250 ms 邊界被判 Expired 的連鎖（08c 的 before 也有 2／6），讀法見第 14 節與第 16 節第 9 點（條件寫死）；**判完後另記**：依樹的 x1 action 24 是 13 Tick 或 15 Tick 的次數（判定程式不輸出，由主對話從 `actions.jsonl`、`match-commands.jsonl` 計算）；Jab8 只在 1 個跑次出現的新原因。
- **綁定資訊**：`binding`（trees.after.head、worktree、main_base、state.main.head、state 的 after head、給的 trees sha）。
- D58 的 Gateway 拿不到內容的丟棄（每跑次總數與依原因）、每跑次 Cooldown、`run_case` 的判定、D64 的格點 D44 旗標、各案例 before 的被釘住、主機（閒置閘門、每輪前後的 sleeper 與 CPU）。

#### 11. 預測（可被推翻；Pab1～Pab8）

plan 第 2 節的預測，加上 09ab-rev 與本批的資料。判定在第 8 節；預測不成立時照判定處理，Pab4、Pab5、Pab8 只記錄。

| 代號 | 白話 | 預測（after） | 推翻條件 | 支持與依據 | 開發跑次（只記錄、不是來源） |
|---|---|---|---|---|---|
| Pab1 | 送輸入不再被釘住 | (a) action probe 的 17 個案例被釘住玩家·秒＝0；(b) 100／150 ms 格 in_span、沒串接的 P 事件 W_post ≤1.0 ms〔提議〕（slew 結束後的口徑） | 任一不成立（Jab3） | (a) 強：08c L2 flow+09a 7 案為 0；09R-map B2 9 格為 0；clean-30、x1、ramp 在 B2 上沒有開發跑次以外的資料，0 是設計要求。(b) 強：09R-map B2 28 個事件最大 0.192。x1／ramp 不套 (b)（含 slew 期間的口徑是 3.47～4.92 ms，09ab-rev §7-2） | (a) after 7 個 action probe 跑次與 before clean-30 全部 0；(b) 6 個事件 0.062～0.127 |
| Pab2 | 100 ms 延遲後完整撤回、不重設 | 100 ms 三格的每個合格事件：撤回量 ∈ [28.5, 31.5] ms〔提議〕（預期約 30.0）、`held_post`＝0、0 次非 respawn 重設、沒有來回修正 | 任一不成立（Jab4） | 強：09R-map B2 16 個事件 29.54～30.45、重設 0／16、`held_post` 0／16、來回修正 0 | after 100 ms 三格各 1 次（全部 8 ms 狀態）：6 個合格事件 29.76～30.10、`residual_ms` −3.22～−3.55、`held_post` 0、重設 0、來回修正 0 |
| Pab3 | 100／150 ms 不串接 | 串接單位＝0 | ≥1（Jab5） | 中：09R-map B2 0／28。150 ms 靠「放出早於 slew 結束」，09R-map B2 的放出到 t_post 是 10.9～18.8 ms（80/150：18.8、10.9），不到 1 Tick；δ＝80 ms 超過 70.6 ms，**只要放出晚於 slew 結束，80/150 就會串接**（plan 3-2「持續延遲 >70.6 ms 時仍會串接」；09R-map B2 的 80/300 是 4／4 串接）。風險低～中，B2 每格只有 2 次資料，無法精算 | 150 ms 沒有跑 |
| Pab4 | （敘述，不是預測）150 ms 只部分有效 | 部分單位重設、部分只部分撤回；放出－t_frame 越接近 70 ms 越可能完整撤回 | 不判定 | 低：凍結的 `measure3` 在 150 ms 給 `withdraw_ms`＝None；09R-map B2 7／12 單位重設 | — |
| Pab5 | 300 ms 照舊重設 | 兩棵樹全部重設；45／60 ms 的單次修正領先＝37.3＋δ ±3 ms（排除 80／300 的串接單位） | 有單位沒重設，或偏差 >3 | 強：09R-map 12／12。只記錄 | — |
| Pab6 | 閒置主機沒有退步 | clean-60：late、非 respawn 重設、Cooldown、被釘住都是 0；G2E 和同 session 的 before 相差 ≤1.0 ms〔提議〕。clean-30：不預測，只記錄 | clean-60 任一不成立（Jab6） | 0：中～強（08c L2 各 4 次；09R-map B2 1 次）。G2E：中（參考 −0.110 只有 09a） | after clean-60 1 次四個 0；clean-30 兩棵樹各 1 次 late 0、重設 0 |
| Pab7 | 抖動網路下不觸發 late 修正 | network0／20／40 的 late＝0（固定種子的這個遺失樣式） | ≥1（Jab7；立即停止） | 中：08c 32／32 是 flow 與 flow+09a，沒有 B2；固定種子，是同一情境的重複。〔程式〕每次發布只帶最小樣本（`MatchRuntimeHost.cpp:129`），late 要連續 2 次（`LocalPlayerPrediction.cpp:128`、`:134`）。**T-B9**（下面） | 沒有跑 |
| Pab8 | 持續延遲（x1、ramp）不預測 | 不做預測，照 D61 的事後規則比較兩棵樹（Jab8） | D61 的旗標 | B2 在這兩案沒有開發跑次以外的資料。〔推論〕ramp 上升期間重新擷取取最大值，可能再往前修，下降時領先更大 | x1 late 10、ramp 8（08c 的 x1：flow 9～14、flow+09a 10～12；ramp：flow 8～10、flow+09a 10）；非 respawn 重設的原因與數量和 08c 的 flow、flow+09a 逐跑次相同（玩家 1 backlog 1、兩位各 starvation 2）；撤回每位玩家 x1 2 次、ramp 3 次；來回修正的特徵 x1 1 位、ramp 2 位 |

**09ab-rev 的發現（寫進預測的讀法；`L/pvp-v7-09ab-impl-20261013/rev/xhigh-review.md` §1-a、§2-a、§7）**

1. **撤回≈|late|−3.33 ms，殘餘（相位偏早 3.3 ms，`residual_ms`≈−3.3）是結構性的。** 往前修的 late slew 期間，命令以 75 Hz 產生、token 以 60 Hz 補充，09a 的折讓式讓每個命令停在平衡等待 0.2P＝3.33 ms（＝P·(1−60/75)，和 R 無關；`LocalPlayerPrediction.cpp:291-313`、`ClientConnection.cpp:607-623`）。100 ms 格決定那一刻最大的樣本正是這些命令，所以撤回≈33.33−3.33＝30.0 ms，撤回後相位偏早約 3.3 ms（`residual_ms`≈−3.3），要到下一次視窗決定（約 4 秒）才修回。這是**原型本身的行為**（09R-map 的 +29.5～30.5 正是它），本批不改（改了屬 (c) 類）。方向是安全的（不會造成 late、重設或串接）；殘餘超過 FireGate 的 2 ms guard，交 M0（第 12 節）。
2. **Jab4 的上界對 role 晚醒敏感。** 撤回量≈33.33−（決定那一刻最大樣本的發布→送出等待）；role 晚醒 j ms 時那個命令的等待是 max(0, 3.33−j)，要超過 31.5，等待必須 <1.83 ms，所以 **j >1.5 ms 就可能讓撤回超過 31.5**（推論，未實測；09ab-rev 原寫 1.8，D45 xhigh M1 依同一條式子更正）。**j 是單一命令相對於 75 Hz 節拍的晚醒抖動**；整體固定的偏移不會降低平衡等待。開發跑次三個 clean（8 ms 狀態）的產生間隔最大偏差 0.13 ms（D45 xhigh M1），比 1.5 ms 小一個數量級以上；probe 的 TimerBaseline 反映不了這個抖動。資料：
   - 09R-map 的 B2 100 ms 跑次：8 ms 狀態 5 跑次、10 個事件 29.54～30.26；4 ms 狀態 3 跑次、6 個事件 29.62～30.45。**已量到的 8 ms 狀態下沒有往上偏**；最大 30.45 對應 j≈0.45 ms（xhigh 實測 j≈0～0.45 ms）。
   - 獨立的單位只有 8 個跑次；計時器狀態是 probe 的代理值，不是 role 自己的晚醒。
   - D48③（Gateway→Match IPC、08c 之前的樹、stopped session）的 32.75 可以用同一個機制解釋，只是旁證。
   - 本批開發跑次：after 100 ms 三格各 1 次，**3 次都是 8 ms 狀態**（late_p99 7.40～7.98 ms），6 個事件 29.76～30.10，也沒有往上偏。合計 8 ms 狀態 16 個事件（09R-map 10＋開發 6）最大 30.26。
   - 處理：上界保持 31.5〔提議〕，結構上界（撤回量 < 該事件的修正量）只記錄並在違反時並列（第 8 節 Jab4 的讀法；第 16 節第 2 點）。
3. **W_post 的口徑**：一律用 slew 結束後的視窗 [t_post, t_post＋0.25 s)。把 slew 期間的命令算進去會得到結構性的 3.33 ms 而誤判失敗（09ab-rev §7-2；x1／ramp 的 3.47～4.92 ms 就是這個原因）。Jab3(b) 用 `measure.py` 的 W_post，符合這個口徑。
4. 偵測器：持續延遲時重新擷取得出的負向修正若 ≥20 ms，可能被偵測器的「size」類事件看到；**P 事件只讀產品計數器，不受影響**（09ab-L2T 確認）。

**Pab7 與 T-B9**（`tests/object_fps_pvp/PredictionTests.cpp:1791-1838`，09b-impl）：樣本的換算是「原始值－（該命令產生時尚未吸收的決定量）」（`LocalPlayerPrediction.cpp` 的 rebase：`raw − (phaseAppliedSeconds_ + phaseShiftSeconds_ − age.appliedSeconds)`）；修正開始前產生的命令，扣的是整個修正量（T-B9 的註解：rebased by all of it）。這對**任何往後的修正**都適用，包括視窗決定與首次決定，不只限於 late 之後的撤回；所以在 network 案例裡，不需要先有 late 修正，前提也可能成立。若 settle 前最後兩個樣本是**「≥修正後餘裕的成對延遲尖峰」**（原始 slack 是小正值、小於該命令產生時尚未吸收的決定量，換算後變負），而 settle 那一幀又沒有樣本，會立刻觸發一次 late 修正，接著重新擷取、再撤回一次（多一對修正）；同樣的兩個樣本若在 slew 中段，會被下一個準時樣本清掉，不觸發。這是原型本身的行為，測試只釘住現在的行為，沒有新增規則。實際風險仍低：以 Match 端的近似 slack（resolved−host_accepted）看 08c network40 flow+09a，11487 個命令中 <5 ms 的 14 個、<2 ms 的 4 個；每個 client 每跑次只有約 1 次額外的修正（D45 xhigh H2）。撤回後殘餘（`residual_ms`≈−3.3，相位偏早）也讓樣本更不容易被判 late（09ab-rev §7-5）。**Jab7 觸發時**，停下並把那個 late 事件的時刻、所在跑次的修正與撤回紀錄、以 `match-commands.jsonl` 重建（近似）的事件前兩次發布的樣本交使用者，判斷是不是 T-B9 的情境（slack 樣本本身不在任何 log 裡，只能近似重建）。

#### 12. M0 的待決事項（交給第 09 批，不在本批定案）

plan 4-3 與 09ab-rev 的整理。本批的格點**不記錄 FireGate 的 earliest、也不量 R 對 earliest，不能代替重推條件 11 的 FireGate 那一半**（`09-firegate-c2.md:101`）。

1. **M0 要不要加跑 Client→Gateway 短延遲注入**：交 M0 規劃決定（D66 決定 10；傾向加跑）。
2. **late 修正之後的射擊怎麼判**：不當排除區；建議當成另行判定的類別，至少保留「Cooldown＝0」。依據：D48③ 的 2 次 Cooldown（base 的 chain-r2、09a-b2 的 chain-r5）都在 t_c 之後約 114～115 ms、late slew 期間、撤回之前；late slew 期間 `FireGate.cpp:18-19` 的 `Max(0, phaseShift)` 不提供保守（09ab-rev 2-b：B2 沒有改變這個風險）。這 2 次 Cooldown 和 R8 一起作為 M0 的輸入。
3. **撤回後的殘餘：相位偏早 3.3 ms（`residual_ms`≈−3.3）**（09ab-rev §2-a）：撤回期間 FireGate 扣掉的剩餘量正好等於實際多出的餘裕（準確）；撤回結束後，殘餘超過 guard 的前提「決定後的相位誤差 ≤ 死區 2 ms」（`FireGate.hpp:24-27`），到下一次視窗決定（約 4 秒）前 `earliest` 可能比實際晚 1 Tick（只落在壓著冷卻邊界的射擊，約 8%）。M0 要確認這段期間的影響；之後的修法候選是 plan 第 5 節第 1 項的 S（只收 slew 結束之後產生的命令）。
4. **C2** 要記錄被釘住的秒數和 late 修正數（plan 4-3）。
5. 前置條件：09ab-L2 通過、PR #73 的頭＝09a＋09b，之後才做 S3（M0）→ S4（凍結）→ C2。

#### 13. 前置核對（plan 3-5；L2P 開始時與 session 開跑前各一次）

`0e1fabb`（09R-map 量過的 09a＋B2 原型，門檻的來源）對**核准時的 after commit** `b205615`，產品的 Client 端只能有註解：

```
git diff 0e1fabb33e8a2a15db757ba2e6f130ad17ddab2e b205615cf6b77dfd6d2024d1de9654aecc3025b3 -- apps/object_fps_pvp/src apps/object_fps_pvp/include
```

- **第 1 次**（09ab-L2P 開始時，2026-10-14 04:05，主 checkout `8245839`）：輸出 `P/precheck/precheck-1.txt`（`db32a709…`）。
  - 唯一的差分：`apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp` 的 1 段，`Correct()` 前面加 7 行不變式註解（D68，09ab-rev 修正輪）。沒有刪除行，沒有程式碼的改動。
  - 分類：**(a) 類**（行為不變；D68 已記為 (a) 類）。沒有 (b)、(c) 類。照 plan 3-5 第 2 點，借自原型的門檻（Jab3(b)、Jab4、Jab5）保留原本的來源與標記，不走第 3 點（(c) 類）的重新宣告流程。
  - 參考（不在核對範圍）：`0e1fabb..b205615` 在 `docs/` 以外另改了 `tests/object_fps_pvp/PredictionTests.cpp`、`mutations.json`（測試）、`build/acceptance/object_fps_pvp/worker_main.cpp`（worker probe 的測試）、`run_client_delay.py`（＋測試；L2 用主 checkout 的版本，在凍結清單裡）。Match 與 Gateway 的來源在 `eabb107..b205615` 沒有改動。
- **第 2 次**（session 開跑前，執行步驟 3）：同一個指令，輸出必須和 `precheck-1.txt` 的差分部分逐位元組相同（`tail -n +3` 之後的 sha256＝`bee77d5b13a66320cafab6bf6a5a4d864d7a7705ed79220146e8a93b6743e355`）。不同就不開 session，照 plan 3-5 的流程重新分類。

#### 14. 開發跑次（只記錄；不計入判定，不當門檻來源，不調整門檻）

**9 次**（after 的 action probe 7 次、before clean-30 1 次、after downstream-1000ms 1 次），2026-10-14 04:20～04:24，主機沒有閒置閘門（每項之前 1 次 5 秒 sleeper，最大 7.1～8.0 ms）。每一項用**正式 session 的同一個指令**（`run_ab.command_for` 加填好的 `trees-ab.json`、`run_ab.spawn`、`run_ab.runner_error`），之後用凍結的 `judge_ab.py --one` 判（`P/dev_ab.py`，不凍結）。輸出 `P/dev/`（`progress.jsonl`、`runs/<名稱>/`、`runs/<名稱>.judge/record.json`、`send_wait.txt`）。**9 次全部 completed、`--one` 的 stops 全部是空的**；ingress 全部 passed（I1 也是 passed）；D58 的丟棄 0；Cooldown 0；**有 TimerBaseline 的 8 次全部是 8 ms 狀態**（late_p99 6.71～8.12 ms，cross 判法也一致；downstream-1000ms 用 timing probe，沒有 `timer_baseline`）。

| 樹 | 案例 | 秒 | late（兩位合計） | 非 respawn 重設 | 撤回量（ms）／殘餘 | W_post（ms） | 其他 |
|---|---|---|---|---|---|---|---|
| after | x1-45-100 | 21.8 | 2（單次 2） | 0 | 29.76、29.83／−3.55、−3.46 | 0.092、0.080 | 合格 2；`held_post` 0；來回修正 0；q30 69／71；延遲包 23、佇列最大 10、送出延遲 p99 2.83、速率拒絕 0；放出到 t_post 60.9 ms |
| after | x1-60-100 | 20.2 | 2（單次 2） | 0 | 30.10、30.01／−3.22、−3.27 | 0.095、0.062 | 合格 2；`held_post` 0；來回修正 0；q30 70／70；延遲包 25、佇列 12、p99 2.82；放出到 t_post 70.5／70.7 ms |
| after | x1-80-100 | 20.1 | 2（單次 2） | 0 | 29.87、29.80／−3.42、−3.47 | 0.102、0.127 | 合格 2；`held_post` 0；來回修正 0；q30 69／69；延遲包 28、佇列 14、p99 2.82；放出到 t_post 57.5 ms |
| after | x1 | 20.2 | 10（5／5） | 玩家 1 backlog 1、兩位各 starvation 2（全部在故障窗內） | 撤回每位 2 次 | 修正後等待 3.44～3.49（偵測器口徑，含 slew 期間，n＝2） | J5a 成立（never_arrived 0、送出沒到 0、late_first 142）；relay 2072＝Gateway 2072；來回修正特徵 玩家 2 1 次；**`run_case` passed＝False**（下面） |
| after | ramp | 20.2 | 8（4／4） | 同上 | 撤回每位 3 次 | 0.067～0.102（n＝6） | J5a 成立（late_first 112）；relay 2064＝2064；來回修正特徵 兩位各 1 次 |
| after | clean-30 | 19.9 | 0 | 0 | — | — | 被釘住 0 |
| after | clean-60 | 19.9 | 0 | 0 | — | — | 被釘住 0 |
| before | clean-30 | 19.8 | 0 | 0 | — | — | 被釘住 0 |
| after | downstream-1000ms | 13.4 | （timing probe） | starvation 2（窗內） | — | — | 和 08c L2 的 flow／flow+09a 各 4 次相同（starvation 2） |

- **停止條件 6（plan 4-1）沒有觸發**：after 的 100 ms 三格沒有重設、沒有來回修正。
- **Jab3(a) 的預覽**（`P/dev/send_wait.txt`）：after 的 7 個 action probe 跑次與 before clean-30（`send_wait.txt` 的 8 列），被釘住玩家·秒全部 0；送出等待 >2 ms 的比例 x1 0.022、ramp 0.022、100 ms 格 0.006～0.009、clean 0。**x1／ramp 是 B2 第一次有被釘住的資料**。
- **Jab4 的預覽**：6 個事件 29.76～30.10，都在 [28.5, 31.5]；全部是 8 ms 狀態，沒有往上偏（第 11 節第 2 點）。
- **x1 的 `run_case` passed＝False**（使用者旗標，不是停止）：錯誤只有 `Unexpected verdict for post-life-recovery-shot: 8`。機制（`gameplay_evidence.py:64-75`、`:170-175`）：故障窗內 8.4 s 的換彈（玩家 1 的 action 24）在 resolved−observed＝15 Tick（703−688）、參照年齡 250.0057 ms 時被判 Expired（rejection 2，判定允許，`fault_expiry_reasons` 只有它），彈匣沒有補滿，12.0 s 那一發就變成 EmptyMagazine（8），而這一發不在允許範圍內。**這是 250 ms 邊界的擲硬幣**：08c L2 的 x1 18 次裡，action 24 是 15 Tick 的有 5 次，其餘 13 次是 13 Tick（216.7 ms）。15 Tick 的 5 次：before（`c2b76cd`）r2／r4 年齡 250.044 ms，兩次都 Expired（就是 08c 的那 2 個 False，錯誤字串相同）；flow r4 249.89 ms、flow+09a r1 250.0004 ms、r5 249.94 ms 都被接受。flow+09a r1 已超過 250 ms 仍被接受，邊界在 µs 等級（D45 xhigh 逐值核對）。所以不是 after 特有的行為，正式 session 的 x1 兩棵樹都可能出現這個使用者旗標；late 計數不受影響（frames 沒有提早結束：late 10 在 08c 的範圍內）。讀法的條件寫死在第 16 節第 9 點。
- 重設的原因與數量：x1、ramp 和 08c L2 的 flow、flow+09a 每個跑次逐項相同（玩家 1 backlog 1、兩位各 starvation 2），Jab8 的 D61 規則在這兩組預期不會觸發。
- clean-30：兩棵修好的樹第一次有資料，兩者都是 8 ms 狀態、late 0、重設 0、Cooldown 0。
- 開發跑次不計入判定，也不用來調整門檻。

#### 15. 和計畫不同的地方

09ab-L2T 的 8 點（`P/report-l2t.md` 第 4 節）經 D45 xhigh 修正後的版本，加上本批的：

1. **network0／20／40 的 runner**：用 `run_gameplay.py --case networkN`（和 08c L2 的 `run_l2.py:96-98`、`:165` 相同）。plan 3-3 寫 `run_network.py`，但它是每個 runner 都 import 的模組，只受 git 狀態核對保護。
2. **Jab3(a) 的範圍**：action probe 的 17 個案例（含 clean-30、300 ms 格），不含 backpressure 2 案。plan 寫「全部案例」。
3. **Jab3(b) 的母體**：in_span、沒有串接、W_post 算得出的 P 事件（plan 寫「每個 late 事件」；串接事件的 W_post 會混進下一次 slew 的等待，由 Jab5 判）。
4. **Jab4 加了跑次層級的條件**（每個跑次沒有重設、沒有來回修正；來回修正會讓事件被設限而變成不合格，等於被藏起來）。
5. **停止的範圍擴大**：ingress 為 undetermined 也停、d59 也套到格點與 clean-30（D61）；缺 phase 計數器、格點缺故障窗也停。
6. **Jab8**：組的切法（08c 的 8 案各一組、150 ms 三格合併）與 **grid-150 的上限**〔提議〕；每格各自的 D61 比較只記錄。
7. **ΔW** 指定 W_pre_1s；09R-map 的 0.332 重現不了，重算為 0.319。
8. **after 樹的宣告方式**：after 的宣告值放在 `P/trees-ab.json`，它的 sha 當必要參數；判定印出 `binding`，after 是哪個 commit 由本宣告與前置核對保證（第 13 節）。
9. **after 樹的 worktree 名稱**：09ab-L2T 的草稿寫 `../GYO-Engine-l2-after`，本批建在 `../GYO-Engine-l2-ab`（判定與 driver 只讀 trees 檔的值，不影響工具）。
10. **開發跑次**：plan 3-5 的 7 次（after 的 100 ms 三格、x1、ramp、clean-30、clean-60）之外，加了 **before clean-30 ×1**（09ab-L2T xhigh：兩棵修好的樹在 clean-30 都沒有資料），x1／ramp 先算一次 send_wait（第 14 節）。
11. **開發跑次另加 after 的 downstream-1000ms ×1**（plan 3-5 沒有列）：確認 after 樹的 timing probe 在新 driver 的指令下能跑完（Client 端的改動可能連進 timing probe；08c L2 只在 flow 樹跑過）。只記錄。

#### 16. 需要使用者決定的地方（附建議；依 D69 照建議核准，仍列出）

每一項附建議、替代方案與代價。依 D69，主對話照建議核准；這裡仍全部列出，供使用者事後查看。

1. **Jab3(b) 的 W_post 上限 1.0 ms**〔提議〕。
   - 建議：1.0 ms，母體＝in_span、沒串接、W_post 算得出的 P 事件。來源 09R-map B2 28 個事件最大 0.192（約 5 倍餘裕）。
   - 替代：0.5 ms（仍有 2.6 倍餘裕）。代價：沒有直接來源，主機 8 ms 狀態下的 W_post 分布只有 09R-map 的資料。
2. **Jab4 的撤回範圍 [28.5, 31.5] ms**〔提議〕與 ≥6 跑次〔沿用 D64〕。
   - 建議：照 D66 已核准的 [28.5, 31.5]；違反上界但符合結構上界時照樣判不成立、並列結構上界與計時器狀態交使用者。理由：中心值 30.0 有機制來源，上偏只在 role 相對 75 Hz 節拍的晚醒抖動 >1.5 ms 時出現，09R-map 的 8 ms 狀態沒有看到。
   - 替代甲：上界改成結構上界（撤回量 < 該事件的修正量）。好處：不會因晚醒誤判；代價：擋不住 31.5～33 ms 的偏移，而 D45 xhigh 沒有檢查過這個寫法的可達性。
   - 替代乙：依 TimerBaseline 分層定兩個上界。代價：8 ms 狀態下的上偏沒有資料，第二個上界沒有來源。
3. **Jab6 的 G2E 容許差 1.0 ms**〔提議〕。
   - 建議：1.0 ms，只和同 session 的 before 比較（參考 −0.110 ms 的約 9 倍）。
   - 替代：只記錄。代價：Client 端的延遲退步不會被擋下。
4. **Jab8 grid-150 的上限 2.0 事件／跑次（或全部單位重設）**〔提議〕。
   - 建議：加（09ab-L2T xhigh A；沒有它時 before >1.0 的機率約 0.41，連 18／18 全部重設也擋不住）。
   - 替代：只用 D61 的比較規則。代價：如上。
5. **clean-30**：建議只記錄（D66 決定 8），Cooldown 與故障窗外的非 respawn 重設仍是停止條件。本批開發跑次兩棵樹各 1 次（兩次都是 8 ms 狀態）：late 0、重設 0、Cooldown 0、被釘住 0。
6. **Jab7 觸發時的處理**：建議立即停下，附 T-B9 的資料交使用者（plan 3-4）。替代：只記錄、跑完 session。代價：若是 B2 引起的新行為，後面的資料無法用同一份宣告解讀。
7. **開發跑次的結果是否改變任何宣告值**：建議不改。開發跑次沒有觸發任何停止，數字都在既有的範圍內（第 14 節）；開發跑次本來就不用來調整門檻。
8. **前置核對的分類**：D68 的 7 行 `Correct()` 註解屬 (a) 類（行為不變），借自原型的門檻保留，不降為提議值。D45 xhigh 請確認這個分類。
9. **x1 的 `run_case` 使用者旗標怎麼讀**（開發跑次新看到的；D45 xhigh H3 把條件寫死）：
   - 建議：**以下全部成立時**，讀成 250 ms 邊界的連鎖（第 14 節），不算退步：
     - `errors` 只有 `Unexpected verdict for post-life-recovery-shot: 8` 這一條；
     - `fault_expiry_reasons` 只有玩家 1 的 action 24；
     - 那一筆的 resolved−observed＝15 Tick；
     - 250 < `original_reference_age_ms` ≤250.1。
   - 任一不成立（例如 17 Tick、283 ms 才 Expired，或還有別的錯誤），照原定「frames 可能提早結束、late 可能少算，或延遲有變化」交使用者。判定程式不改（只是讀法）。
   - 另外依樹記錄 x1 的 action 24 是 13 Tick 還是 15 Tick 的次數（08c：15 Tick 合計 5／18）；判定程式不輸出，判完由主對話從 `actions.jsonl` 與 `match-commands.jsonl` 計算（第 10 節）。
   - 替代：把它當成 Jab8 之外的退步訊號，after 的次數多於 before 就停下。代價：08c 的資料顯示這是 15 Tick 邊界的擲硬幣（before 2／2、flow 0／1、flow+09a 0／2），每棵樹 6 次的樣本分不出樹的差異。

#### 17. 證據目錄、腳本與執行步驟

- **準備**：`P/`。
  - 樹：`P/tree/`（`build-trees.sh`、建置 log、`artifacts.sha256`、`gateway-vcs.txt`）。
  - 前置核對：`P/precheck/precheck-1.txt`。
  - 凍結（核准時一起凍結，sha 見第 3 節）：`run_ab.py`、`judge_ab.py`、`sleeper.py`、`frozen-ab.sha256`、`frozen/decompose.py`、`frozen/send_wait.py`、`trees-ab.json`、`judge-spec.md`。
  - 開發用、不凍結：`dev_ab.py`、`dev/`（開發跑次）、`dryrun-l2p.txt`（本批的 dry-run）、09ab-L2T 的 `recompute.py`、`mock_ab.py`、`dryrun/`、`mock/`。雜湊：`P/prep-files.sha256`（09ab-L2T）＋`P/prep-files-l2p.sha256`（本批）。
- **證據目錄**（session）：`L/pvp-v7-09ab-l2-<YYYYMMDD>/`：`declaration.md` 與 `declaration.sha256`、`commands.txt`、`precheck-2.txt`、`session/`（`plan.json`、`idle_gate.jsonl`、`host.jsonl`、`progress.jsonl`、`runs/<名稱>/`、`runs/<名稱>.judge/`、`summary.json`、`judge-ab/`、`judgement.json`）。
- **步驟**（核准後；字面值一律抄本節，不從當下的檔案算）：

```
# 0. 宣告提交為 H（只改 docs/）。H 寫進 HANDOFF 的進度記錄器。
R=/Users/karasu/Code/Source/GYO-Engine; cd $R
P=build/target/_build/test/logs/pvp-v7-09ab-l2-prep-20261014
E=build/target/_build/test/logs/pvp-v7-09ab-l2-<YYYYMMDD>
JUDGE_SHA=ac5cdb5dbc6a457af80fc2078d75eb14e175a93afe0e163f0a3e404cc1881ded
DRIVER_SHA=4d2ec0813f8368740ec5ea76924367d888797f8c2b0ed28d4476e5134a3ba7ef
TREES_SHA=e3b0a9f0ec2fc7c8a5fff5eb16020f751123fa5f0421544d4e4e60eb89d86560
H=<宣告 commit 的完整 sha（字面值）>
# 1. 凍結檔的核對
shasum -a 256 -c $P/frozen-ab.sha256                                  # 49/49 OK
shasum -a 256 $P/judge_ab.py $P/run_ab.py $P/trees-ab.json $P/sleeper.py $P/frozen-ab.sha256   # = 第 3 節
git diff --name-only b205615cf6b77dfd6d2024d1de9654aecc3025b3 $H | grep -v '^docs/'          # 空
# 2. 暫停 PR #73 的 auto-fix、背景 agent 與分析；前景不開其他程式
# 3. 前置核對（第 2 次）
mkdir -p $E; git diff 0e1fabb33e8a2a15db757ba2e6f130ad17ddab2e b205615cf6b77dfd6d2024d1de9654aecc3025b3 \
  -- apps/object_fps_pvp/src apps/object_fps_pvp/include > $E/precheck-2.txt
shasum -a 256 $E/precheck-2.txt     # = bee77d5b13a66320cafab6bf6a5a4d864d7a7705ed79220146e8a93b6743e355
# 4. dry-run：exit 0、preflight_problems []
python3 -I -B $P/run_ab.py --dry-run --main-head $H --judge-sha $JUDGE_SHA --driver-sha $DRIVER_SHA --trees-sha $TREES_SHA
# 5. 正式執行（約 1 h；閒置閘門要等時最多再加 30 分）
caffeinate -dims python3 -I -B $P/run_ab.py --output $E/session --idle-gate --main-head $H \
  --judge-sha $JUDGE_SHA --driver-sha $DRIVER_SHA --trees-sha $TREES_SHA
# 6. 判定
python3 -I -B $P/judge_ab.py --session $E/session --trees-sha $TREES_SHA
# 7. judgement.json 的 binding 和本宣告對照：trees.after.head = b205615c…、main_base = b205615c…、state.main.head = H
```

- 第 3 步的輸出和第 1 次的差分逐位元組相同（`precheck-1.txt` 開頭兩行是說明，`precheck-2.txt` 沒有，所以兩者的 sha 比的是差分本體：`precheck-1.txt` 的 `tail -n +3`＝`bee77d5b…`）。
- 核准時把本宣告與第 3 節的檔複製到 `$E/`，雜湊寫進 `$E/declaration.sha256`。
- 結果寫進 `09b-phase-tracking.md` 與 HANDOFF（09ab-doc 之前，照 HANDOFF 記錄器的規則同步更新）。

##### 對 D45 檢查（`P/xhigh-check-declaration.md`）的處理對照

| 項目 | 處理 | 位置 |
|---|---|---|
| H1 Jab5／Pab3 的餘裕「約 40 ms」不成立 | 改為 09R-map B2 150 ms 放出到 t_post 10.9～18.8 ms（80/150：18.8、10.9），撤回「約 40 ms」並註明出處；加讀法：80/150 的串接單位若 `release_to_t_post_ms` ≤0 照樣判不成立，交使用者時標明「>70.6 ms 的已知限制，被放出時刻觸發」並附該值 | 第 8 節 Jab5 與讀法；第 11 節 Pab3 |
| H2 Pab7／T-B9 的機制、固定種子、交付 | 換算改為「原始值－（該命令產生時尚未吸收的決定量）」，寫明任何往後的修正都適用（視窗決定、首次決定也是）；讀法加「network 是固定種子，0 只代表這個遺失樣式」；Jab7 交付的「事件前兩次發布的樣本」改為以 `match-commands.jsonl` 重建（近似） | 第 8 節讀法的限定；第 11 節 Pab7 列與「Pab7 與 T-B9」 |
| H3 x1 使用者旗標的讀法太寬 | 條件寫死（只有那一條錯誤、`fault_expiry_reasons` 只有玩家 1 的 action 24、15 Tick、250 < 年齡 ≤250.1 ms）；依樹記錄 action 24 的 13／15 Tick 次數；第 14 節改成逐值的年齡 | 第 16 節第 9 點；第 10 節使用者旗標；第 14 節 |
| M1 j 的門檻 1.8 → 1.5 ms | 改為 j >1.5 ms，寫明 j 是相對 75 Hz 節拍的晚醒抖動；Jab4 交付時附該跑次 slew 期間的產生間隔偏差 | 第 8 節 Jab4 的讀法；第 11 節第 2 點；第 16 節第 2 點 |
| M2 開發跑次的次數 | 9 次（after action probe 7 次、before clean-30 1 次、after downstream 1 次）；有 TimerBaseline 的 8 次全是 8 ms 狀態；HANDOFF 由主對話改 | 第 14 節；第 8 節 Jab3(a)；第 11 節 Pab1 |
| M3 開發跑次混進來源欄 | 第 8 節與第 11 節的表另立「開發跑次（只記錄、不是來源）」欄，來源欄拿掉開發跑次；寫明 clean-30、x1、ramp 在 B2 上沒有開發跑次以外的資料，0 是設計要求 | 第 8 節表、Jab3(a)、Jab6；第 11 節表 Pab1、Pab2、Pab6、Pab8 |
| L1「7／12＝1.17」 | 改為「7 個事件／6 跑次＝1.17（7／12 個單位）」，flow 同樣寫法 | 第 8 節 Jab8 |
| L2 殘餘量的正負號 | 寫明 `residual_ms`≈−3.3 就是相位偏早 3.3 ms | 第 10 節、第 11 節第 1 點、第 12 節第 3 點、Pab7 |
| L3 mock 引用 | 改引 09R-map session 的 B2 跑次（`mock/full` 以它為輸入） | 第 8 節 Jab3(a) |
| L4 上限的機率與整跑次計數 | 寫明 p＝7／12 與 0.85 時的機率；「≥2.0」算整跑次的非 respawn 重設，觸發時列出多出來的重設 | 第 8 節 Jab8 上限的讀法 |
| L5 被釘住的餘裕；backpressure 其實有資料 | 寫明結構餘裕（最大比例 0.32～0.41 等）；backpressure 的缺口來自凍結腳本的範圍；最大比例看不到的退化列為測試缺口 | 第 8 節 Jab3(a) 與其範圍的讀法 |
| L6 100 ms 不合格候選 | 撤回量落在 [28.5, 31.5] 之外的列為使用者旗標（讀法） | 第 8 節 Jab4 的不合格候選 |
| 整個 session 可能全是 8 ms 狀態 | 只記錄 | 第 4 節 |
| 測試缺口（80/150 放出時刻串接、合格事件被設限消失、被釘住比例的退化） | 只記錄；判定與 mock 不改 | 本表；第 8 節 |
| 第 2 次前置核對在結構上必然相同 | 只記錄；照 plan 3-5 保留，真正綁住 after 的是 TREES_SHA | 本表 |
| 第 16 節 1～8 | xhigh 全部同意；第 2、6、7 點已照 M1、H2、M2／M3 修正文字 | 第 16 節 |

## 09b-impl（2026-10-13～14，D66）

- 實作：`LocalPlayerPrediction.hpp/.cpp`、`Movement.hpp` 和 B2 原型 `c2705be`（09R-map 量過的 `0e1fabb`）逐字相同，前置核對（3-5）是空差分。`PredictionTests.cpp` 加原型的 LaggedHost、T-B1、T-B2、reseed、X1～X4，以及缺陷 A（在 `c26f9c4` 上失敗，偏移 66.7 µs）、每串 late 修正只重新擷取一次、持續延遲 >70.6 ms 仍串接、特性測試（放出晚於 slew 結束時只部分撤回；之後若改善這個限制，這個測試會失敗）、T-B9、SeedLead、reseed 後的 late 修正不計舊 seed 的命令、重新擷取計入 settle 前產生的命令。`:1241` 的測試名稱改成只對非 late 修正成立，斷言不變。
- 恆等式：沒有修正進行時，和 `c26f9c4` 逐值相同（差分 harness，100／100 種子；對照模式 0／20 相同），證據放在證據目錄（不寫成 commit 的單元測試：逐位元的參考值在 arm64／Windows 可能不穩定）。
- 突變 batch `v7-09b`：11 個，全部 killed（`mutations.json`）。**預期存活**：`seedlead-keeps-applied`（留下的常數位移在換算時相減抵消，只差捨入 1.2e-15 s）。`seedlead-keeps-ages` 原本誤判為等價，xhigh 審查以探針推翻後補測試殺掉；`seedlead-keeps-reacquiring` 原型審查曾記為存活，其實不等價，也已補測試殺掉。
- 驗證：全量 CTest 69／69（cpu 248）、probe 29／30（1 次是既有的負載敏感斷言，probe 不含 `LocalPlayerPrediction`）、權威 digest 0 差分（scale 1、3 各 35）。換算交界的局部 xhigh 審查：產品沒有缺陷。證據：`build/target/_build/test/logs/pvp-v7-09ab-impl-20261013/09b/`。
- 給 L2 宣告（Pab7）：往後修的 slew 期間，若 settle 前最後兩個樣本是「≥修正後餘裕的成對延遲尖峰」、settle 那一幀又沒有樣本，會觸發一次 late 修正、接著重新擷取再撤回（T-B9；原型本身的行為）。推論：network／抖動型下增量接近 0。
- 交 09ab-rev：A、B、A 的重複計數（`c26f9c4` 也有；Host 每個序號只發布一次、Client 依 tick 去重，實際到不了）；「Correct 只在剩餘量為 0 時被呼叫」的不變式要寫明或加 assert；「每串 late 修正」的用語；恆等式 harness 的兩點限制；時鐘丟棄的時間不進 `phaseAppliedSeconds_`（既有近似）。

## 缺陷 A：settling 期間收到的舊樣本留在下一個視窗

- **機制**：`Correct()` 只在修正開始時清掉樣本（`LocalPlayerPrediction.cpp:97-101`）。settling 期間 `:117-130` 照常收樣本，用的是舊的 `settledAfter_`；settle 時（`:323-328`）只前移 `settledAfter_`，不清視窗。所以修正後滿 240 個樣本時，視窗裡還有 m 個「修正前相位」的樣本。`Percentile` 取 `sorted[216]`（`:24-30`）。
  - 正向修正時舊樣本落在頂端，P90 實際約 P93.1（m＝8）、P93.5（m＝9）；負向修正時落在底端，約 P89.7，量級只有前者的約 1/9。主要往「相位更晚、slack 更少」的方向偏。
  - 影響的不只是滿窗的那一次決策：滿窗之後每次 TrackPhase 都重算，所以影響前 m＋1 次決策。
- **為什麼重要**：P90 是 FireGate guard 推導的前提（相位追蹤控制到達餘裕的 P90）。偏了，guard 的餘裕就跟著偏。
- **量級（v7 很小）**：兩種獨立方法都在同一量級。開環重播：1049 個修正後的視窗，clean 的 P90 偏移最大 0.27 ms，決策改變 clean 0／379、全部 1／1049（開發 network40）；Match 端的 slack 代理值：08b L2 的 P93−P90 中位 0.02～0.03、最大 0.08 ms。原因是 v7 的 slack 很集中（08b L2 clean after 的 P10／50／90＝36.9／37.3／37.6 ms）。限制：重播的視窗決策只吻合 72／131（55％），0／379 是開環近似，不是實機的決策。
- **這是 v6 就有的程式**：同一段 settle 程式從 `a4ccaa5`（v6）起就存在，`fee92ff`、`03b2ea7`、`805bd11`、`a954aa3` 都相同。所以它不可能是任何新舊兩棵 tree 差異的原因。
- **選項**：
  - A1【建議】settle 時刪掉視窗前段，只保留結尾 `min(lateSamples_, size)` 個樣本（約 2 行）。late 的判斷只讀結尾 `lateSamples_` 個樣本（`:134-135`），A1 正好保留它們，所以在第一次視窗決策分歧之前，late 決策與現行相同（開環性質檢查：3000 條串流中 0／1251 的第一次分歧是 late 決策）。分歧之後的 late 修正可能不同（開環：clean 0／379，network40 1／1049）。另外 `lateSamples_==1` 時會殘留 1 個舊樣本在底端，偏向保守。
  - A2 settle 時連 `lateSamples_` 一起清空：同時改變缺陷 B（見下），不建議。
  - A3 依每個命令「還沒吸收的修正量」換算：精確，但單獨用來修 A 是過度設計；它是 B2 的基礎。
- **途中被推翻的說法**：
  - 「約 P93 是最壞情況」：百分位的口徑混用（`217/(240−m)` 是 (idx＋1)/n，P89.7 是 idx/n）。統一後是「m≈8 時約 P93.1」。
  - 「A1 由構造保證 late 路徑不變」說得太大：只在第一次視窗分歧之前成立。

## 缺陷 B：settling 期間累積的 late 樣本，在 settle 時觸發第二次 late 修正

- **機制**：`:128` 在 settling 中照樣累加 `lateSamples_`（`slack<0 ? +1 : 0`，任何一個準時樣本就歸零）；settle 不重設；下一次 TrackPhase 走 `:134-137`，用沒有換算的舊相位樣本再修正一次，同樣夾在 2 Tick。觸發條件是 settle 前最後 ≥2 個樣本連續遲到，持續延遲或間歇性的成對遲到都可能。`:113-116` 的註解與 `PredictionTests.cpp:1229-1238`（far 測試，−150 ms → 2 次）把它寫成設計意圖。
- **為什麼重要**：多修一次就是多 2 Tick 的領先，可能直接推向 backlog。
- **證據**：v7 建置中有相位觀測的 late 修正只有 8 次，其中 0 次串接成第二次；唯一有完整 trace 的 D44 是單次修正。模型（`judge/sim_judge.txt`）在 60～250 ms 的短停頓下也只修 1 次。但模型每個命令送 1 個樣本，實機是「每次發布取最小值」（`MatchRuntimeHost.cpp:110`、`:312`、`:403`），模型高估了遲到樣本數，所以「短停頓不發生」在實機只會更成立。檢定力很低，寫成「未觀察到」，不寫「排除」。
  - 更正（2026-10-11，09R-eval）：「0 次串接」「未觀察到」是在截斷的資訊流上的結論。L2 的 x1／ramp 已觀察到串接：結算（settle）之後隔一個 Tracking 幀、128～155 ms 就再修正一次；非首次的修正中串接的比例 ramp flow 21／21、ramp flow+09a 24／24、x1 flow 29／39、x1 flow+09a 25／37（late 修正合計 ramp 37、40，x1 69、63；依凍結 D48 偵測器的計數器增量）。這些案例的串接不影響 Match 的重設（重設完全由注入決定，三棵樹逐案相同）。所以 B0 的前提（未觀察到）不成立，B 要在 09R-map 之後於 09b-plan 重新規劃（D63 ①③）。證據：`build/target/_build/test/logs/pvp-v7-09r-eval-20261011/`的 `late_chain.txt`、`detector/`、`report-from-agent.md` 第 1-b 節。
- **持續延遲時，第二次修正其實有用**（模型，hypothesis）：持續 +60／+70 ms 的延遲，現行碼修正 2 次，最後 slack 44.0／34.0 ms（目標 37.3）；「清空」或「只換算」的修法只修 1 次，slack 只剩 10.7／0.7 ms，維持到約 4 秒後的視窗修正。也就是說，直接把 B 修掉會更差。L1 雖然執行了 94 次缺陷 B，但只有 far 測試有斷言，所以「持續延遲下有用」只有模型支持。
- **選項**：
  - B0【建議】不改程式，把限制寫進文件。
  - B′（只換算 late 旗標）、A2（清空）：持續延遲下更差，不建議。
  - B2（補償換算＋late 修正後以首次決定規則重新擷取，8 個樣本、0.5 ms 死區，不清 `phaseDecided_`）：模型中短停頓後 1 次 slew 撤回 2 Tick，30 Tick 合計 120→74，持續延遲收斂到 37.3。但它會改變 late 修正與 backlog 的行為，屬 D44 的提前條件；只有模型證據，抖動與間歇性遲到下會不會來回修正沒量過。它的前提「slack 隨產生時刻 1:1 移動」在被釘住時不成立，所以只能排在 09a 之後。

## 延遲位移：不是缺陷，是設計造成的結果

- **分解**（08b L2 clean，Client＝`a954aa3`，actual 解析的移動命令）：產生→第一次送出 0.1 ms，送出→Match 收下 0.4 ms，Match 端 slack 37.3 ms，產生→執行（G2E）37.8 ms。兩種主機狀態相同。37.3＝領先 2 步（33.33 ms）＋目標 4 ms（`Movement.hpp:37`、`:45`；`LocalPlayerPrediction.cpp:123-124`）。
- **為什麼 v6 是 21～25 ms**：兩棵 tree 的樣本公式相同（v6final `:121-122`＝現行 `:123-124`），追蹤保持的都是「步邊界→執行」≈37.3 ms。但 v6 的 generated 是畫面幀發布的時刻（邊界＋age），v7 由角色在步邊界醒來時記錄（`ClientSimulationRole.cpp:125`、`:185`）。所以 13～17 ms 的差＝v6 的幀 age 抵扣，兩棵 tree 的「產生→執行」原點不同、不能直接比。v6 的實際餘裕是 37.33−age，30 FPS 時可以低到約 4 ms，這也是 v6 在 clean 會出現 late 修正、30 FPS 有 Held 的原因。
- **使用者實際感受到的**：「輸入取樣→執行」v7 多約 1 幀（C1 s2 4 ms 狀態：clean-60 v6 25.0 對 v7 41.4 ms，clean-30 21.1 對 54.0 ms；受 probe 錨定影響，只記錄）。
- **代價與收益**：v7 把餘裕固定在 37.3 ms，C1 clean-30 的 Client 替代比率因此 0.010％（v6 0.570％）；代價是多 1 幀的輸入延遲，以及 backlog 餘裕減半（75→45），這正是 D44 在 v7 只要一次 late 修正就閉合的結構原因。
- **能動的槓桿**：只有「追蹤的 lead」與 target。`InitialCommandLead` 不能直接改：SeedLead 的中立命令會改變權威 digest，凍結分析器也寫死 2（`command_evidence.py` 的 `INITIAL_COMMAND_LEAD = 2`、`start_phase_evidence.py`）。
- **選項**：
  - 只修文件【建議】：寫明跨樹不可比、I2E 多約 1 幀是步邊界產生的代價、backlog 餘裕 75→45。
  - 拆出「追蹤 lead」＝1（seed 仍為 2，X 37.3→20.7 ms，60 FPS 的 I2E 約 −16.7 ms，backlog 穩態回到 30）：Held 餘裕降到約 v6 60 FPS 的水準（抖動網路下暴露明顯上升），Windows 的角色晚醒未知（第 15 批），`FireGate.cpp:22` 必須同步改（否則 earliest 早 1 Tick），改變 backlog 穩態，觸發 D44 停下詢問。不建議放進 09b。

## 其他發現（列進未結事項，不在 09b 處理）

- **停頓 reseed 依據過時的 snapshot 播種，領先實際上歸零**：reseed 之後新命令的 G2E 只有約 0.2～6 ms（`adversarial/step_context_06l2_gw250_r6.txt`），直到首次決定或 Acquiring 的 late 修正才把領先補回。（規劃時把它描述成「舊命令遲到」，被對抗式檢查推翻。）
- **權威 digest 不保護相位追蹤**：RunLink 不經 TrackSlack，35 個場景的相位樣本都是 0。
- **late 修正並非一律 2 Tick**：角色自己停頓、補步時 age 很大，修正很小（C1 s1 r4 約 −4.5 ms）。只有命令準時產生、遲到在傳輸路徑時才夾在 2 Tick，而 D44 的危險情境正是後者。
- **clean 的 late 修正**：閒置主機的 session（C1 s2、08b L2）是 0；非閒置主機的 C1 s1 有 4 次（r1 因此延遲門檻失敗）。所以 09b 與之後的 L2 都要沿用 C1 s2 的閒置檢查作為前提。

## 實作與驗收（決定確認後）

- **09b-1：A1 與 L1**（high；settle 與 late 交界的局部 xhigh，加 1 位 xhigh 實作審查）：
  - `:323-328` 的刪除與 `:109-116` 的註解。
  - T-A1 視窗衛生（HEAD 要失敗、修改後通過；PhaseTwins 在 settle 那一幀的樣本可能在 Advance 之後被過濾，實際收進的舊樣本可能是 4 個，實作者要照 `:24-30` 的索引核對）；T-A2 late 路徑不變（far 測試不改斷言，另加「settle 時結尾 1 個 late，再來 1 個新 late → 第二次修正」）；T-A3 負向修正與「`lateSamples_==1` 時只殘留 1 個舊樣本，決策不變」。
  - 突變（batch `v7-09b`）：`settle-keeps-window`、`settle-clears-all`、`settle-erases-tail`（寫清楚由哪一個測試殺掉；`lateSamples_=0` 時等於 HEAD）。
  - 全量 CTest、權威兩樹比對 35／35（只當 L1 閘門，它不保護相位追蹤）、TSan。
- **09b-2：開發跑次**（clean-30／60、network20／40、backpressure host-ipc-250ms），用開跑前凍結的偵測器記錄 late 修正（大於 20 ms 的負向步之前 30 Tick 內有 held 或負 slack，而且同一 epoch 之前沒有 seeded_neutral），先在 08b L2、C1 s1／s2 上重算基準並記錄 sha256。
- **09b-3：L2**（宣告 high＋xhigh 對抗式檢查，使用者核准）。要點（草案，對抗式檢查已修正）：
  - before＝09a 的頭，after＝09a＋09b 的頭，各在 detached worktree 從零建置；Gateway 逐位元組相同。
  - 判定：clean-30 ×12、clean-60 ×6 交錯；只記錄：network20、network40、backpressure host-ipc-250ms 各 3 輪。
  - 前提：沿用 C1 s2 的閒置檢查。
  - J1：通過率 after ≥ before−1/N（C1 規則 3 的形式）；「after 全部通過」改為停下回報的條件。
  - J2：G2E 每位玩家 P50 的合併中位數，|after−before| ≤1.0 ms（不退步的檢查，不是 A1 的證明）；來源值要用宣告的同一個腳本、同一個視窗在 08b L2 上重算；任一位玩家每輪的 P50 落在 [36.8, 38.8] ms 之外就停下。
  - J3／J4：領先跳升與非 respawn 重設，after 出現就停下；before 與 after 並列計數；歸因規則事先寫明（before 也出現，或同一跑次沒有視窗決策分歧時，歸到 09a／D44）。
  - J5：Client 替代比率，0.05％ 依本批的跑次長度換算成每輪筆數；排除 Match 停頓的根據是「A1 只改 Client 的私有狀態，Match 行為相同」。
  - 限制：A1 的效果（≤0.3 ms）低於 L2 的解析度，L2 只證明沒有退步，A1 的證明在 L1。
- **之後**：第 09 批的 M0 在 09a＋09b 的頭上量。

## 本批的停止條件

1. 範圍需要擴大到 late 路徑、late 修正上限、target、lead、backlog 門檻或 FireGate 常數，但使用者沒有核准（D44 提前條件）。
2. A1 實作後 far 測試不是 2 次，或 `PredictionTests.cpp:1241-1257` 出現 late 或修正。
3. MovementRecoveryTests 或任何 Held 斷言需要放寬；`worker_main` 的斷言需要放寬。
4. 權威兩樹比對不是 35／35；需要改 wire。
5. 突變沒有被殺，或既有突變變成 stale。
6. L2 的 after 出現 J3、J4 或權威 Cooldown 拒絕；J2 超出門檻或出現雙峰。
7. 凍結分析器拒絕 trace。
8. 開發跑次或 L2 中，late 修正後 30 Tick 合計 ≥105 的比例高於 before。
9. FireGate 常數有任何需要大於 v6 的跡象。
10. 09a 尚未完成，或 09a 的 L2 發現與相位追蹤有關的退步。

## 需要決定的事

1. **（2026-10-10 使用者決定 D48：先用測試確認，再選 (a) 或 (b)；分三步，見 HANDOFF 的 D48）（D44 提前條件）09a 修好送出被釘住之後，D44 可能變多。** 現有 5 次真正的 late 修正：被釘住的 4 次沒有重設，沒被釘住的 1 次就是 D44。你可以從 `adversarial/step_context*.txt` 看到每一次的前後命令時間線。選項：
   - (a) 照原計畫做 09a，但在 09a 的開發跑次與 L2 用凍結的偵測器記錄「每次 late 修正後的 30 Tick 合計」，比例高於 09a 之前就停下；D44 仍在第 16 批。代價：可能在 09a 的 L2 才碰到重設。
   - (b) 把 D44 的一部分提前：09a 與 09b 一起處理，在 09b 加入 B2（late 修正後重新擷取，讓短停頓後撤回 2 Tick）或其他讓 late 修正不閉合 backlog 的方式，再規劃一次。代價：範圍擴大、B2 只有模型證據、L2 要加故障案例與短停頓注入（要改 acceptance 工具）。
   - (c) 09a 暫緩，先查清楚 D44（第 16 批的工作提前到 P2）。代價：P2 的順序大幅改變。
   - 我的傾向是 (a)：先用量測確認「被釘住在擋 D44」是不是真的，樣本只有 5 次，不足以支撐 (b) 的範圍擴大；若 09a 的開發跑次就出現，再停下改走 (b)。
2. **09b 的範圍**：只做 A1 加文件更正（建議）。
3. **缺陷 B**：B0，不改程式、寫明限制（建議）。若決定 1 選 (b)，B2 在那時重新規劃。
4. **延遲位移**：只修文件，常數不變（建議）。拆出追蹤 lead＝1 是替代方案，會觸發 D44 停下，不建議放進 09b。
5. **文件更正**（建議同意）：
   - HANDOFF「C1 之後要決定的事」：「約 P93」改成「m≈8 時約 P93.1；v7 clean 偏移 ≤0.3 ms」；缺陷 B 寫成「未觀察到、hypothesis 層級」，不寫「排除」。（更正，2026-10-11，09R-eval：缺陷 B 已在第 08c 批 L2 的 x1／ramp 觀察到，不再是「未觀察到」；數字見「缺陷 B」一節的證據。）
   - 03a「晚約 1 幀、方向相同」不成立：主要理由是兩棵 tree 的 settle 程式完全相同，缺陷不可能造成兩者的差；「方向相反」只是輔助；「probe 幀相位與 age 繞回」標為 hypothesis。
6. **D44 未結事項補上**（建議）：v7 穩態 30 Tick 合計 60 對 v6 的 30；傳輸型的 late 修正才夾在 2 Tick；一次傳輸型 late 修正就會 ≥105（模型 120，實測 1／1 是 108）；送出被釘住時是 89～99（4／4）。
7. **其他發現列進未結事項**（建議）：停頓 reseed 後領先歸零；權威 digest 不保護相位追蹤。
8. **09b 的 L2**：單獨一個 session，讓 M0 的頭明確等於 09a＋09b（建議）；或併進 M0 的前段（同一份建置，加上凍結的 J2～J4），少一次 session。
9. **檔位**：09b-1 主力 high、局部 xhigh，加 1 位 xhigh 實作審查；L2 宣告 high 加 xhigh 對抗式檢查；文件與執行 medium。

## 規劃的過程與證據

- 三份方案的主要結論一致：A1、B0、延遲只修文件、D44 維持第 16 批。評審否決了 risk 方案「短停頓下領先 2→6、合計 180」（沒有證據，與模型矛盾）與 latency 方案把「追蹤 lead＝1」列為建議。
- 對抗式檢查的結論：主要建議可以採用，但多項事實描述要先更正；**最重要的遺漏是 09a 的交互**（上面第一段），應在 09a 實作前問你。已併入的更正：late 修正並非一律 2 Tick；clean 的 late 修正只在閒置主機為 0；late-outcomes 的分類（真正的 late 修正 5 次）；Acquiring late 的機制；「構造保證」的範圍；百分位口徑；J1、J2、J5 的來源與缺少的 late 偵測基準；03a 更正的主要理由。
- 證據：`build/target/_build/test/logs/pvp-v7-09b-plan-20261010/`（`defects/`、`latency/`、`risk/`、`judge/`、`adversarial/`，各有 `commands.txt` 與 `sha256.txt`）。

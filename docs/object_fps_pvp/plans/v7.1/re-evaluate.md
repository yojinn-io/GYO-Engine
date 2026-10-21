# re-evaluate：用 pv7 之後的數據重新評估（建議清單，第 1 版）

- 狀態：**建議第 1 版，等你排序**（第 3 節）。
- 依據：README 的 `re-evaluate` 列（「用 pv7 之後的數據重新評估：相位追蹤（B6～B9）、延遲位移（A4）、偶發 epoch 重設（A5）、連線品質的參考年齡等；交出建議，由使用者排序」）。INVENTORY 處置為 re-evaluate 的 12 項，加上今天交過來的 D44 歸因、09b 重做的剩餘（R4、U6）。
- 做法：分三組只讀評估（相位追蹤與延遲位移、Host 與 wire、A5），讀程式與現有證據，不跑新的 L2。主要的證據是 `d44-regression-pv7` 第 3 次 session（`build/target/_build/test/logs/pvp-v71-d44-l2-20261021c/`，197 項）。分析腳本在本會話的 scratchpad（`re-eval-phase/`、`re-eval-host/`、`re-eval-a5/`），沒有進 repo。
- 限制：schema 3 的 `phase_decision` 還沒有 L2 證據（只有開發跑次的 3 筆 first），所以相位追蹤的結論是從 schema 2 的 generated／resolved 時間戳推的。
- file:line 以 `3ca4e6a` 為準。

## 1. 總覽

```
re-evaluate 的建議
├── 現在結案（只改文件）
│   ├── N-respawn-reject-volume         「0.5 h｜828 是重送窗口的副本，計數點不同」
│   ├── N-conflict-window-permanent-loss「0.5～1 h｜pv7 原樣轉送後不再永久」
│   ├── B9                              「0.5 h｜領先不足都在 0.25 s 內自己恢復」
│   ├── B8 的「連續 N 次」              「—｜N＝2 沒有誤觸發」
│   ├── N-phase-1-7 第 6 點             「—｜92/s 低於 120/s」
│   └── N-09b-followup-measures         「0.5 h｜拆到各項後結案」
├── 小修（不動 digest、不動 wire）
│   └── N-analyzer-literal-105          「1～1.5 h｜6 處寫死的 105，改成共用常數＋核對測試」
├── 保留為已知限制（寫文件）
│   ├── A5                              「1～2 h｜設計內的回收；300 ms 型是已知限制」
│   ├── B6                              「—｜殘餘約 4 s、+3 ms（12/12）」
│   └── N-quality-reference-age-sampling「0.5 h｜低估方向、每視窗最多 8.7%」
├── 選做的量測（只在要做下面的延後項時才有用）
│   ├── 300 ms 型格點在 pv7 重量          「約 2 h｜A5、門檻 105 的前提」
│   └── B6 的 Gateway→Match 延遲格點      「約 1.5 h 機器＋分析｜B6 的前提」
└── 延後（有前置條件）
    ├── A4                              「v8，13～15 h｜唯一玩家感受得到（−16.7 ms），手感取捨」
    ├── B7                              「pv8，20～30 h｜改 wire，要等 B6 的結論」
    ├── 門檻 105 重推                    「6～10 h｜動 golden digest；對 x1 型沒幫助」
    ├── B8 的幅度＋slew                  「和 A5 一起｜slew 速率也是瓶頸」
    ├── 快速正向回收                     「v8 之後，10～20 h｜沒有它支持的 Feature（AGENTS §2）」
    └── N-phase-estimator-wan-noise      「有 WAN 測試時｜真實 WAN 樣本是 0；也是 A4 的前置」
```

## 2. 各項

| 項目 | pv7 之後的狀況 | 證據 | 建議 | 工時 | wire／digest |
|---|---|---|---|---|---|
| N-respawn-reject-volume（828 對 147） | 已解釋。Client 的 pending 到上限 12 就凍結，每幀重送同一個 12 命令的舊 epoch 窗口，直到新 epoch 的 snapshot 到達；pv7 原樣轉送，Match 算每一份副本。147 是 pv6 在 Gateway 端算的（當時 Gateway 不重送寫出過的命令），計數點不同，不是錯誤 | downstream-respawn-1000ms 三次 816／804／804 個命令，每個被拒的窗口正好 12 個（`match-ingress.jsonl`）；`PvpMatch.cpp:71-73`；`Movement.hpp:51`；`LocalPlayerPrediction.cpp:350`、`:454` | 結案，在 08c 與 INVENTORY 寫上原因 | 0.5 h | 都不動 |
| N-conflict-window-permanent-loss（pv6 缺口 9） | 前提不成立：「永久」來自 Gateway 的併入與去重，pv7 已刪除（Design Invariants 禁止）。剩下的是 Match 整則拒收衝突窗口、游標越過後恢復，損失是暫時的 | 197 項的 `conflict_*` 計數全是 0；`network-architecture.zh-Hant.md:745-760`；`PvpMatchTests.cpp:1113-1117` | 結案；protocol-v6 缺口 9 註明「pv7 起不再永久」，記下「衝突窗口整則拒收，到游標越過為止」 | 0.5～1 h | 都不動 |
| B9（reseed 用過時的 snapshot） | 前提成立，但影響小 | 遊戲矩陣 24/24 領先完整；背壓 120 次中 12 次領先不足（8 次剩 1 步、4 次剩 0），都在 0.19～0.25 s 內回到 ≥33 ms，替代 0 | 結案，記為已知限制。要修就得估計 Host 時鐘，違反「不假設兩端時鐘有關」（`LocalPlayerPrediction.cpp:241`） | 0.5 h | 修的話可能動 digest |
| B8（late 修正的幅度、連續 N 次） | N＝2：沒有誤觸發（network 系 9 次，負樣本 1～2 個都沒觸發；唯一觸發的 network40-r3 同時有 held）。幅度：x1 起始遲到約 −166 ms，修正上限 2 Tick；推論就算一次修 165 ms，slew 每幀 25% 也要約 0.66 s，比 starvation 的 0.5 s 長 | `Movement.hpp:79`；`LocalPlayerPrediction.cpp:17`；`PvpMatch.cpp:511-512` | N 的部分結案；幅度和 A5 一起延後 | — | 幅度：改 late 與 backlog 行為 |
| N-phase-1-7-items-3-6 | 第 6 點：每秒輸入封包最多 92，低於 session 上限 120，限速拒絕 0。第 3 點：沒有資料 | `session.go:44`；197 項的 `rate_limit_refusals` | 第 6 點結案；第 3 點併入 B6 | — | — |
| N-09b-followup-measures | 第 6 項（換遺失種子）已排進下一個 L2（D153）；第 11 項（時間基準）屬 re-evaluate 之後；第 5、8、10 項只有做 B6 才需要；第 7、9、12 項是方法上的事 | 09b §5 | 拆到各項後結案 | 0.5 h | — |
| N-analyzer-literal-105 | 寫死的 105 有 4 個檔 6 處，視窗長度 30 也寫死 5 處 | `command_evidence.py:214`、`:436`；`gameplay_evidence.py:298`、`:323`；`gameplay_soak_evidence.py:365`；`quad_evidence.py:228` | 改成共用常數，加一個從 `Movement.hpp:55-56` 解析並核對的測試；做任何門檻調整之前都要先有它 | 1～1.5 h＋L1 | 都不動（改凍結工具，下一個 L2 重新凍結） |
| A5（backlog 105、300 ms 型、快速正向回收） | x1／ramp 的 backlog 重設是延遲消失後留下的常駐領先（queued 停在 9～11 Tick）被回收，觸發時刻由 cooldown 決定，不是由 105 決定；要避開只能用 150～183 ms 的常駐延遲去換。300 ms 型（v7 的 09ab L2：合計 105～107、queued 4～5）的判定邏輯在 pv7 沒變，推論仍會出現，沒在 pv7 量過 | x1 15/15 在放出後 +366～370 ms、ramp 15/15 在 +674～677 ms 重設；`PvpMatch.cpp:505-506`；`pvp-v7-09ab-l2-20261014/summary-main.txt:20-22` | 照設計保留並寫文件：x1／ramp 型是「starvation 重對齊後的設計內回收」，300 ms 型是已知限制。要寫進 Design Invariants 的話屬 Architecture Delta，要你同意 | 1～2 h | 只寫文件時都不動 |
| B6（重新擷取的樣本邊界 S／M） | 1-7 第 1 點的殘餘在 pv7 重現 12/12；FireGate 退役之後，它只代表約 4 秒、+2～3 ms 的延遲，期間沒有 held | stall-2.0／11.5：撤回後 G2E 多 +2.01～+3.35 ms，3.97～4.14 s 回到基準 | 保留為已知限制；要修先做 phase-tracking-golden | 修約 4 h（不含 golden） | 都不動 |
| N-quality-reference-age-sampling | 前提成立：只對帶新命令的窗口取參考年齡。被排除的 late-only 窗口在 x1／ramp 約 4.7～4.9%，單一 10 s 視窗最多 8.7%；被排除的偏高，所以偏差方向是低估。L2 沒有任何踢出 | `MatchRuntimeHost.cpp:242-247`；`refage.py` | 保留為已知限制，記下方向與上界；要修先補檢測（ingress 行尾追加中位數、樣本數、排除數，2～3 h），再決定要不要改取樣（3～4 h） | 記錄 0.5 h | 改取樣會動 digest（踢出在 digest 裡） |
| A4（延遲位移：產生→執行約 38 ms） | 前提成立：pv7 的 33 個母體 P50 都在 37.5～38.0 ms，分解和 08b 相同，所以 Gateway 直通沒有改變它。舊成本「FireGate 要同步改」已經不存在 | `g2e_d44.txt`；`LocalPlayerPrediction.cpp:144-145`；`Movement.hpp:53`、`:62`。反事實（推論）：lead 1 時收下→執行 <16.7 ms 的比例，clean 0.16%、network20／40 0.9%、impaired 2.5%（品質門檻 5%） | 延到 v8，當作手感取捨。好處是移動與射擊都少約 16.7 ms（A1 之後射擊照命令步） | 程式 2 h＋golden 約 7 h＋L2 4～6 h，合計 13～15 h | 不改 wire；digest 不涵蓋相位追蹤，擋不住它；會改 backlog 穩態（60→30），碰到 D44 的停止條件 |
| B7（slack 到達即發布） | 前提成立：已執行的樣本要到解析那個 Tick 才發布。Match 端看不出收益：樣本流失可忽略，合併時保留的是最負的那個 | `MatchRuntimeHost.cpp:640-667`、`:233-239`、`:133-138`；`slack.py` | 延到 pv8，等 B6 的結論 | 20～30 h（推論） | 改 wire；不動 digest |
| 門檻 105 重推（D51④） | 往下調會撞到 network40 的 90，往上調是用延遲去換；對 x1 型沒有幫助 | clean 的 30 Tick 合計最大 60、network40 90 | 延後；前提是 300 ms 型格點的 pv7 重量 | 6～10 h | 動 golden digest（`move-backlog/contract`） |
| 快速正向回收 | 只對 300 ms 型（常駐 33～50 ms）有用，對 x1／ramp 型沒有幫助；現有的 B2 只在放出早於 slew 結束時撤回 | `09b-phase-tracking.md:1167`、`:1176`、`:1372` | 延到 v8 之後：說不出它支持的 Feature（AGENTS §2） | 10～20 h，時序邏輯要 xhigh | 改行為 |
| N-phase-estimator-wan-noise | 模擬抖動下，epoch 1 的修正次數從 clean 的每位玩家 1 次，增加到 network20／40 的 2 次；真實 WAN 樣本是 0 | `phase_summary.py` | 延到有 WAN 測試時；也是 A4 的前置資料 | — | — |
| D44 本身 | 290 個 case-run 加長測 0 事件，「未再現」。剩下的只有 A5 這條看得到、解釋得了的 Match 機制 | `d44-regression-pv7.md` 第 15 節 | 已由 `d44-regression-pv7` 處理；A5 見上 | — | — |

## 3. 等你排序

- 建議的順序（成本低、風險低、能解除其他項目前提的排前面）：
  1. **現在結案的 6 項**（只改文件，合計約 2～2.5 h）：讓 INVENTORY 只剩真正待決的事。
  2. **N-analyzer-literal-105**（1～1.5 h＋L1）：任何門檻調整之前都要有它。
  3. **A5、B6、N-quality-reference-age-sampling 寫成已知限制**（約 2～3 h）：其中 A5 若要寫進 Design Invariants，要你另外同意。
  4. **延後項的歸屬**：A4 → v8（先做 phase-tracking-golden 與 WAN 資料）；B7 → pv8；門檻 105、B8 幅度、快速正向回收 → 和 A5 一起，等有支持的 Feature 再排。
  5. **選做的量測**：只在你要做第 4 項裡的某一項時才跑。
- 1～3 加起來約 5～7 h，都不動 wire、不動 digest。要不要在 v7.1 內做完、還是只做 1，交你決定。

# re-evaluate：這些項目還需不需要繼續（判斷清單，第 2 版）

- 狀態：**判斷第 2 版已確認（D154）；B6、B7、A5 補量中**（第 4 節）。
- 依據：README 的 `re-evaluate` 列。目的是用 pv7 之後的數據，判斷每個項目**還需不需要繼續**，不是設計怎麼做。範圍是 INVENTORY 處置為 re-evaluate 的 12 項，加上從今天的 `d44-regression-pv7` 衍生出來的 3 項（門檻 105 重推、快速正向回收、分析器寫死的 105）。
- 做法：分三組只讀評估（相位追蹤與延遲位移、Host 與 wire、A5），讀程式與現有證據，不跑新的 L2。主要證據是 `d44-regression-pv7` 第 3 次 session（`build/target/_build/test/logs/pvp-v71-d44-l2-20261021c/`，197 項）。分析腳本留在本會話的 scratchpad，沒有進 repo。
- 更正：第 1 版寫了修法、工時與順序，方向不對（使用者：「re-evaluate 就是評估，這些case還是否需要繼續的，而不是評估要如何做的」）。第 2 版只留「需不需要」與根據。
- file:line 以 `3ca4e6a` 為準。

## 1. 判斷

```
re-evaluate
├── 不需要繼續（結案或取消）
│   ├── N-respawn-reject-volume          「828 已解釋：重送窗口的副本，計數點不同」
│   ├── N-conflict-window-permanent-loss 「前提消失：pv7 原樣轉送，不再永久」
│   ├── B9                               「領先不足都在 0.25 s 內自己恢復，替代 0」
│   ├── B8                               「N＝2 沒有誤觸發；幅度不是瓶頸」
│   ├── B6                               「殘餘只剩約 4 s、+3 ms」
│   ├── B7                               「它要解的殘餘已經很小，Match 端看不出收益」
│   ├── A5                               「看得到、解釋得了的設計內回收」
│   ├── 門檻 105 重推                     「隨 A5：沒有要改的理由」
│   ├── 快速正向回收                      「隨 A5：說不出支持哪個 Feature」
│   ├── N-quality-reference-age-sampling「低估方向、每視窗最多 8.7%，沒有任何踢出」
│   ├── N-phase-1-7-items-3-6            「第 6 點已滿足；第 3 點隨 B6」
│   └── N-09b-followup-measures          「已拆到其他地方，沒有獨立的事」
├── 還需要（等你決定排到哪裡）
│   ├── A4                               「延遲位移 38 ms 還在，是玩家感受得到的手感」
│   └── N-analyzer-literal-105           「同一個常數在 6 處手動維持（AGENTS §9）」
└── 有條件才需要
    └── N-phase-estimator-wan-noise      「真實 WAN 樣本是 0，有 WAN 測試時再看」
```

「不需要繼續」的項目，每項都附上重開條件：條件出現時再回來，不必重新盤點。

## 2. 各項的根據

### 不需要繼續

| 項目 | 判斷 | 根據（pv7 的證據） | 重開條件 |
|---|---|---|---|
| N-respawn-reject-volume（828 對 147） | 結案：不是缺陷 | Client 的 pending 到上限 12 就凍結，每幀重送同一個 12 命令的舊 epoch 窗口，直到新 epoch 的 snapshot 到達。downstream-respawn-1000ms 三次是 816／804／804 個命令，每個被拒的窗口正好 12 個（`match-ingress.jsonl`；`Movement.hpp:51`；`LocalPlayerPrediction.cpp:350`、`:454`）。147 是 pv6 在 Gateway 端算的（當時 Gateway 不重送寫出過的命令），pv7 原樣轉送，Match 算每一份副本，計數點不同 | — |
| N-conflict-window-permanent-loss（pv6 缺口 9） | 結案：前提消失 | 「永久」來自 Gateway 的併入與去重，pv7 已刪除，Design Invariants 也禁止（`network-architecture.zh-Hant.md:745-760`）。Match 整則拒收衝突窗口，游標越過後就恢復（`PvpMatchTests.cpp:1113-1117`）。197 項的 `conflict_*` 計數全是 0 | 有 Client 會刻意送衝突窗口時 |
| B9（reseed 用過時的 snapshot） | 結案 | 遊戲矩陣 24/24 領先完整；背壓 120 次中 12 次領先不足，都在 0.19～0.25 s 內回到 ≥33 ms，替代 0 | 抖動網路下，領先不足的那 0.25 s 出現 held |
| B8（late 修正的幅度、連續 N 次） | 取消 | N＝2 沒有誤觸發：network 系 9 次都有 1～2 個負樣本但沒觸發，唯一觸發的 network40-r3 同時真的有 held。幅度：x1 型的回收由 backlog 重設處理（見 A5），不是幅度的問題 | 出現 late 修正追不上、又沒有 backlog 重設接手的情況 |
| B6（重新擷取的樣本邊界 S／M） | 取消 | 1-7 第 1 點的殘餘在 pv7 重現 12/12，但 FireGate 退役之後，它只代表約 4 秒、+2～3 ms 的延遲，期間沒有 held（stall-2.0／11.5，撤回後 3.97～4.14 s 回到基準） | Gateway→Match 延遲 80～150 ms 的格點（pv7 沒量過）量出殘餘遠大於 +3 ms，或 `phase_decision` trace 顯示撤回失敗 |
| B7（slack 到達即發布） | 取消 | 它要解的是撤回太慢（`09b-phase-tracking.md:1138-1147`），那個殘餘就是 B6，已經很小。Match 端也看不出收益：樣本流失可忽略，合併時保留的是最負、late 修正要用的那個（`MatchRuntimeHost.cpp:133-138`）。要做就得改 wire（pv8） | 同 B6 |
| A5（backlog 105、300 ms 型的延遲消失重設） | 取消（照設計保留） | x1／ramp 的 backlog 重設是延遲消失後留下的常駐領先（queued 停在 9～11 Tick）被回收，觸發時刻由 cooldown 決定（`PvpMatch.cpp:505-506`）；要避開只能用 150～183 ms 的常駐延遲去換。300 ms 型在 v7 的 09ab L2 出現過（合計 105～107），判定邏輯在 pv7 沒變，推論仍會出現，但它是看得到、解釋得了的 Match 機制，不是 D44（`d44-regression-pv7.md` 第 15 節） | 玩家在真實網路下感受到這種重設，或它造成踢出 |
| 門檻 105 重推（D51④） | 取消 | 隨 A5：沒有要改的理由。往下調會撞到 network40 的 90，往上調是用延遲去換 | 同 A5 |
| 快速正向回收 | 取消 | 隨 A5：只對 300 ms 型有用，說不出它支持哪個 Feature（AGENTS §2） | 同 A5 |
| N-quality-reference-age-sampling | 結案：記為已知限制 | 只對帶新命令的窗口取參考年齡（`MatchRuntimeHost.cpp:242-247`）。被排除的窗口在 x1／ramp 約 4.7～4.9%，單一 10 s 視窗最多 8.7%；被排除的偏高，所以偏差方向是低估。197 項沒有任何踢出 | 真實網路下出現該踢沒踢、或不該踢卻踢的情況 |
| N-phase-1-7-items-3-6 | 結案 | 第 6 點：每秒輸入封包最多 92，低於 session 上限 120，限速拒絕 0（`session.go:44`）。第 3 點沒有資料，隨 B6 | 同 B6 |
| N-09b-followup-measures | 結案 | 第 6 項（換遺失種子）已排進下一個 L2（D153）；第 11 項（時間基準）是 E3，已在 re-evaluate 之後；第 5、8、10 項只有做 B6 才需要；第 7、9、12 項是方法上的事 | — |

### 還需要

| 項目 | 為什麼還需要 | 根據 |
|---|---|---|
| A4（延遲位移：產生→執行約 38 ms） | 位移還在，而且是玩家感受得到的手感；A1 之後射擊也照命令步判定，所以移動與射擊都受影響。這是手感上的取捨，不是缺陷 | pv7 的 33 個母體 P50 都在 37.5～38.0 ms，分解和 08b 相同，Gateway 直通沒有改變它 |
| N-analyzer-literal-105 | 同一個常數（backlog 門檻 105、視窗 30）在 Match 與 4 個分析器的 6 處手動維持，違反「同じ事実を手動で維持する場所は、一か所に限定する」（AGENTS.md §9）。不改門檻也一樣存在 | `command_evidence.py:214`、`:436`；`gameplay_evidence.py:298`、`:323`；`gameplay_soak_evidence.py:365`；`quad_evidence.py:228` |

### 有條件才需要

| 項目 | 條件 | 根據 |
|---|---|---|
| N-phase-estimator-wan-noise | 有真實 WAN 測試時 | 模擬抖動下，epoch 1 的修正次數從 clean 的每位玩家 1 次增加到 2 次；真實 WAN 樣本是 0 |

## 3. 等你確認

- 第 1 節的判斷是否同意，特別是 B6、B7、A5 的取消：這三項的重開條件裡，有 pv7 還沒量過的範圍（80～150 ms 的格點）。
- 還需要的 2 項（A4、N-analyzer-literal-105）要排到哪裡：v7.1 之內，或交給 v8。

## 4. B6、B7、A5 的補量（D154：「B6、B7、A5先補量再定」）

量測只為了判斷這三項還需不需要繼續，不是驗收：只記錄，不設門檻、不做判定，結果交你決定。量什麼在跑之前寫在這裡。

### 4.1 格點與跑法

- 沿用 v7 的 09R-map（`plans/v7/09b-phase-tracking.md` 第 5 節）：Client→Gateway 的固定延遲 x1，δ∈{45, 60, 80} ms × 持續∈{100, 150, 300} ms，9 格；起點 `--fault-at 12.0`（同 09R-map 的理由：避開射擊冷卻與生命事件）。
- 每格 2 次：第 1 輪照 (45,100)、(60,100)、(80,100)、(45,150)…(80,300) 的順序，第 2 輪反序。共 18 次，每次約 25 秒。
- 樹：主 checkout 的目前建置（`3346379` 的程式；movement trace 是 schema 3，帶 `phase_decision`）。凍結清單：本次產生的 `build/acceptance` 全部 `.py` 的清單（不是 L2，不沿用 cp 清單）。
- 證據：`build/target/_build/test/logs/pvp-v71-reeval-map-20261021/`。

### 4.2 記錄的量（每次、每位玩家）

- **A5（300 ms 的 3 格）**：非 respawn 重設的原因與發生時刻（相對放出）；重設前 30 Tick 的排隊合計。對照 v7 的 09ab L2（三格都是 backlog 重設，合計 105～107）。
- **B6、B7（全部 9 格）**：
  - 放出前後的 `phase_decision`：late 修正的次數與修正量、之後第一個 first 決定（重新擷取）的修正量、兩者相加的淨殘餘（09b 1-7 第 1 點預期約 −3.3 ms）。
  - 放出後的產生→執行（G2E）回到注入前基準的時間（09b 1-7 第 1 點：約 4 秒）。
  - 放出後 3 秒內的 held／neutral 數。
  - 150 ms 格有沒有部分撤回、反向或重設（1-7 第 3 點）；80 ms 格有沒有串接（第二次 late 修正或重設，1-7 第 4 點）。
- 全部以表格列出，和 09ab L2、09R-map 的數字並列；不從結果推門檻。

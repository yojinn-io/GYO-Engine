# rebind-on-new-port 的 L2 事前宣告（第 2 版，2026-10-25，等使用者核准；第 1 版經 D45 檢查後改寫，見第 7 節）

批次：`rebind-on-new-port`（D181、D187→D188）。被驗收的變更：Client 的 Hello 一律每 250 ms（`206d967`、`126edbe`）。關卡與經過見 [rebind-on-new-port.md](rebind-on-new-port.md)。

## 1. 範圍與工具

- **注入**：outage-resync 的 `build/acceptance/object_fps_pvp/run_outage.py`，不改。注入種類與每一輪的流程照 [resync-l2-l3 宣告](resync-l2-l3-declaration.md)第 1 節（每輪重開 Match、Gateway、兩位玩家的 probe；遊戲 16 s，probe 開始後 6 s 布設注入）。
- **驅動**：`run_rebind_l2.py`。由 outage-resync 的 `run_resync_l2.py` 改出：只換項目清單與判定工具，停止規則與 sha256 檢查不變。
- **判定**：`judge_rebind.py`。先執行凍結的 `judge_outage.py`（J-R1～J-R9，原樣），再加上第 3 節的 N1、N2、N9。凍結的判定會把 `ingress-evidence.json` 寫回輪次目錄；本 session 的輪次是新產生的，沒有問題。舊證據要重判時一律用副本（第 7 節）。
- **凍結**（sha256，換行正規化後；開跑時以參數傳入，不符就拒絕執行）：
  - `run_rebind_l2.py`：`9d065eb60049ab1a0abf91e2bcd140b8d63d323cc6e627c0668f93f642a9f56c`（4 個字面值都必須傳入，否則拒絕執行）
  - `judge_rebind.py`：`a77140cfd1f3c710fb70c3671cda3e44486d85bda67e8e26940672aa047a581f`（輸出保留凍結判定的完整結果，放在 `frozen` 鍵）
  - `judge_outage.py`（outage-resync 的凍結版，不改）：`0bfe5d9b3ecf29e3045c55c8cd5f288259b3c592ea2ebc59831412b8196bb8ba`
  - `frozen-resync.sha256`（outage-resync 的凍結清單，不改）：`b53dd372f6fa57098572ee6223ce5d613405d21bf5709caf0c60f27c1c798300`
  - 新的兩個檔案在 `build/target/_build/test/logs/pvp-v71-rebind-l2-prep-20261025/`；舊的兩個在 `…/pvp-v71-resync-l2-prep-20261024/`（都是 git 忽略的證據目錄）。
  - Match、Gateway、probe 在開跑前由 `206d967` 之後的樹重建，sha256 記錄在 `plan.json`。Gateway 必須重建：它不在預設的建置目標裡，舊的執行檔早於 D180。

## 2. 跑次（4 項 × 3 輪＝12 輪，約 12 分鐘；照這個順序）

| 代號 | 注入 | 為什麼放進來 |
|---|---|---|
| C0 | 不注入（對照組） | Hello／Welcome 的計數在正常遊玩時是新規則（N2） |
| C2 | 雙向斷 1000 ms | 中斷與恢復不受影響（凍結的判定） |
| C4 | 只斷上行 1000 ms | 不中斷、上行不到的情境，Hello 計數改變最大（只記錄） |
| C10 | 雙向斷 1000 ms 後換來源埠 | 改綁的時刻（N1）；恢復的 epoch（N9） |

純不對稱換埠（事前沒有斷線）這次的工具造不出來（`run_outage.py` 的 `port` 一定先雙向斷）。D188 不依賴任何狀態判斷，改綁只看原 endpoint 的靜默，所以 C10 量到的「窗口結束後到改綁」就是 Hello 間隔加上傳遞時間；純不對稱的情況，改綁發生在原 endpoint 靜默滿 T_int 之後的下一個 Hello，這個推論記在 rebind-on-new-port.md，不另做 L2。

## 3. 判定（每一輪、每一位玩家）

- **凍結的 J-R1～J-R9**：原樣，見 resync-l2-l3 宣告第 3 節。唯一的例外是 C10 的「J-R9 epoch」，由 N9 取代（理由見下）。
- **N1（C10）**：每位玩家剛好一行 `session rebound`，而且 `silent_ms − window_ms ≤ 400`。
  - `silent_ms` 是 Gateway 收到改綁 Hello 的時刻減去原 endpoint 最後一個通過驗證的資料包的時刻（`session.go:107-113`、`server.go:513`），所以 `silent_ms − window_ms`＝（窗口開始 − 最後一個上行，60 Hz Input 下最多約 17 ms，實測 6.0～10.5）＋（改綁 Hello − 窗口結束，最多一個 Hello 間隔 250 ms）＋傳遞時間。
  - 來源：雙向斷線期間原 endpoint 靜默；窗口結束後第一個從新埠到達的 Hello 就能改綁（靜默早已超過 T_int＝500 ms；新埠的 Input 在 endpoint 比對時就被拒，不耗速率額度）。250＋17＝267 ms，餘裕約 133 ms。
  - 舊行為（D188 之前，`pvp-v71-resync-l2-20261024` 的 C10）：`silent_ms` 2004～2005，相減後約 1004，會判 F。
  - 探索性試跑（第 6 節）：230.1、242.1（判定的算法，減 `window_ms` 1000.884）。舊的每秒一次規則不可能碰巧通過：恢復在窗口結束後約 10 ms，Hello 回到每 1 s，下一個至少在窗口結束後約 750 ms。
- **N2（C0）**：中繼收到的、該玩家送出的 Hello，從第一個到最後一個：
  - 平均每秒 3.6～4.4 個（每 250 ms，±10 %）；
  - 每一個間隔在 200～350 ms（和 worker 驗收的 `CheckHelloCadence` 相同；只看平均時，偶爾回到 1 s 的間隔抓不到）；
  - 中繼收到的、送給該玩家的 Welcome 數等於 Hello 數，或少 1 個（join 期間收到的 Hello 不回 Welcome，`server.go:516-527`；實測 join 只花 3.7～15.5 ms）。
  - 舊行為：每秒 1.00 個、間隔約 1000 ms，會判 F。探索性試跑：4.0，間隔 249.9～250.2 ms，Welcome 77＝Hello 77。
- **N9（C10，取代凍結的「J-R9 epoch」）**：
  - `connection resumed` 行的 `epoch_after` 等於 Client 在窗口結束後收到第一份 snapshot 的時刻，Match 最後一次換 epoch 的 epoch；
  - 從那個時刻到窗口結束後 1 s，Match 對這位玩家換 epoch 最多 1 次，原因必須是 Backlog，而且不早於中繼在窗口結束後收到的第一個 Hello（改綁的那個 Hello）。
  - **為什麼要換**：凍結的規則比的是「窗口結束後 1 s 時 Match 的 epoch」。C10 的下行仍走舊埠，Client 在窗口結束後約 10 ms 就恢復；到改綁之前，上行被 Gateway 拒收，這段不對稱期間 Client 累積了領先量，改綁後約 0.5 s（30 Tick）由 Backlog 清掉、換一次 epoch。這是 outage-resync 已知的 P2 例外（和只有上行中斷的 C4 同一型；C2 沒有）。舊行為改綁在窗口結束後約 1 s，那次 Backlog 落在 1 s 之外（舊 3 輪：窗口開始後 2510～2529 ms，計數窗到約 2001 ms）。D188 讓改綁提早約 0.8 s，同一次 Backlog 落進計數窗（探索性試跑：1728、1745 ms），凍結的規則就判 F。
  - **AGENTS §14.4**（這是受入判定的變更）：
    1. 守護的性質：恢復時 Client 用的是 Match 當時的 epoch，不是過時的。
    2. 放寬的面：恢復之後、窗口結束後 1 s 之內，Match 因 Backlog 換了一次 epoch、而且在改綁的 Hello 之後，以前判 F，現在判 P。其他的換 epoch（次數多於 1、原因不是 Backlog、或在改綁之前）照樣判 F（N9 的第二條）。
    3. 收緊的面：恢復發生在窗口結束 1 s 之後時，(1 s, 恢復] 之間的換 epoch 以前不算，現在算。
    4. 被放寬的情況由什麼守：放寬的只有「改綁後的一次 Backlog」；改綁之前上行到不了 Match，正確的程式不會在那之前觸發 Backlog（Backlog 要待處理命令總和 ≥105，`PvpMatch.cpp:505-530`），所以 N9 的第二條守住時刻與原因。恢復之後命令是否被接受、相位是否重新擷取，由凍結的 J-R9（3 s 內被接受的命令數、`first` 決策）照舊判定。
    5. 證據：探索性試跑 C10（`explore-C10`）在凍結規則下 F、在 N9 下 P；舊的 3 輪 C10 在兩種規則下都 P（Backlog 在窗口開始後 2529.4、2514.8、2510.1 ms，都在計數窗外）；突變「Backlog 移到窗口結束後 100 ms（改綁之前）」在 N9 下判 F。
- **只記錄**：每一輪、每位玩家在窗口內中繼收到的 Hello 數（C4 預期約每秒 4 個，被中繼丟掉）；凍結判定被 N9 取代的那些問題原文。

## 4. 前提（不成立時判 E）

照 resync-l2-l3 宣告第 4 節（注入成立、兩位玩家、證據檔齊全、窗口與 start_ns 可讀）。

## 5. 結果的分類與停止規則

照 resync-l2-l3 宣告第 6 節：某一項出現 F 或 E 就停止整個 session（已跑的輪次照判、保留）；結果出來後不放寬判定（§14.1）。

## 6. 探索性試跑（不作判定）

2026-10-25，`206d967` 之後的樹，`pvp-v71-rebind-l2-prep-20261025/explore-*`，各 1 輪：

| 項 | judge_rebind | 凍結的 judge_outage | 記錄 |
|---|---|---|---|
| C0 | P | P | Hello 每秒 4.0（兩位都是 77 個），Welcome＝Hello |
| C2 | P | P | 窗口內 Hello 每秒 4.0 |
| C4 | P | P | 窗口內 Hello 每秒 4.0 |
| C10 | P | F（只有 J-R9 epoch） | `silent_ms` 1231、1243（窗口 1001）；Backlog 換 epoch 在 1728、1745 ms |

判定工具對舊證據（`pvp-v71-resync-l2-20261024` 的 C0、C10 第 1 輪）判 F：N2（每秒 1.00）、N1（`silent_ms` 2005）。

## 7. 門檻來源的對抗式檢查（D45，2026-10-25）

一位審查者（唯讀，在副本上重跑判定）。發現與處置：

1. **（中）N9 第 1 版放寬得太多**：凍結的 J-R2 只把 Starvation／Resync 算成保險絲，所以第 1 版的 N9 放過「恢復後 1 s 內任何非保險絲的換 epoch」，次數、時刻都不限；突變「Backlog 移到改綁之前」判 P。→ 加上 N9 的第二條（最多 1 次、Backlog、不早於改綁的 Hello），突變改判 F；§14.4 的第 2、4 點照實改寫。
2. **（中低）Backlog 成因寫錯**：不是「中斷期間的領先量」，是恢復後到改綁之前的不對稱期間累積的（C2 沒有 Backlog，C4 有）。→ 第 3 節改正。數字都正確。
3. **（中低）N2 只看平均**：每位玩家刪掉 2 段各 3 個 Hello 仍判 P。→ 加上每個間隔 200～350 ms，突變改判 F。
4. **（低）N1 的來源少算一項**（窗口開始到最後一個上行，約 6～17 ms），探索性的數字是減 1000 而不是減 `window_ms`。→ 第 3 節改正；門檻 400 與「窗口結束後第一個 Hello 就改綁」駁不倒。
5. **（低）Welcome＝Hello 在正確程式下理論上可能差 1**（join 期間的 Hello）。→ 允許少 1。
6. **（低）工具**：判定輸出丟掉了凍結判定的 checks／recorded → 保留在 `frozen` 鍵；驅動的 sha256 參數原本可以不傳 → 改成必須全部傳入。
- **駁不倒的部分**：§14.4 的第 1、3、5 點；舊 C10 3 輪在兩種規則下都 P、新的 1 輪凍結 F／新判定 P；舊 C0 判 F（N2）、舊 C10 判 F（N1）；N9 以 `player_id` 過濾正確；「窗口後第一份 snapshot_received」可當恢復時刻（中繼轉發後約 0.1 ms）；突變「恢復的 epoch 改成過時的 1」被 N9 抓到。
- **證據的完整性**：審查者指出凍結的 `judge_outage.py` 會寫回 `ingress-evidence.json`。本會話之前曾直接對舊 session（`pvp-v71-resync-l2-20261024`）的 C0、C10 第 1 輪跑過判定；以該 session 的 `files.sha256` 核對，638 個檔案全部相符（被重寫的兩個檔案內容逐位元相同，只有修改時間變了）。之後的重判都用副本。

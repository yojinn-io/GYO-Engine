# outage-resync：resync-l2-l3 的 L2 宣告（第 2 版；2026-10-24 使用者核准，D178）

日期：2026-10-24。分支 `claude/pvp-v71-outage-resync`。

這份宣告對應計畫第 7 節的 J-R1～J-R8，另外新增 J-R9（恢復後真的回到正常遊玩）。依 AGENTS.md §14.1，在看結果之前宣告，交使用者核准後才開跑。第 1 版經 D45 對抗式檢查（第 7 節）後改寫成本版。

## 1. 範圍與工具

- **注入**：`build/acceptance/object_fps_pvp/run_outage.py`（`11ed8a6` 及之後的修正）。
  - 凍結的 `action_probe.py`、`impaired_network.py` 原樣匯入。每一輪只在這個程序裡把 `ActionRelay` 換成 `OutageRelay`，所以既有情境的位元組不變。
  - 注入種類：`both`（雙向）、`uplink`（只上行）、`downlink`（只下行）、`partial`（部分送達）、`port`（換來源埠）、`none`（對照組）。
  - 每一輪都是一次新的 `run_case`：Match、Gateway、兩位玩家的 probe 全部重開；遊戲 16 s，在 probe 開始後 6 s 布設注入。
  - 注入是否成立由 `round_problems` 讀 `relay.json` 判斷。
- **驅動**：`run_resync_l2.py`。固定順序執行 11 項，每項是一次 `run_outage.py`，連續跑 3 輪。
- **判定**：`judge_outage.py`。每項的 3 輪跑完後逐輪判定，只讀既有的證據檔：
  - `probe.log`、`gateway.log`、`match.log`；
  - `match-commands.jsonl`、`clients-commands.jsonl`、`match-ingress.jsonl`；
  - `relay.json`。
- **時間基準**：`relay.json` 的 `fault.start_ns` 和各紀錄檔共用 steady clock。窗口結束 ＝ `start_ns + window_ms`（中繼時鐘量的長度）。
- **凍結**：驅動、判定工具與凍結清單以 sha256 字面值鎖定，開跑時以參數傳入，不符就拒絕執行。三個檔案都在 `build/target/_build/test/logs/pvp-v71-resync-l2-prep-20261024/`（git 忽略的證據目錄）：
  - `run_resync_l2.py`：`ba4666abff195f52d2e4d7be7ab39a01566a6a77f0579f24ce0e72846704e231`
  - `judge_outage.py`：`0bfe5d9b3ecf29e3045c55c8cd5f288259b3c592ea2ebc59831412b8196bb8ba`
  - `frozen-resync.sha256`（60 個檔案，含 `run_outage.py` `0ac266a5…`）：`b53dd372f6fa57098572ee6223ce5d613405d21bf5709caf0c60f27c1c798300`
  - 清單包含這一批改過的 `command_evidence.py`（`resume_reset`），以及 `run_outage.py`。
  - Match、Gateway、probe 在開跑前重建，sha256 記錄在 `plan.json`。

## 2. 跑次（11 項 × 3 輪＝33 輪，約 30 分鐘；照這個順序）

| 順序 | 代號 | 注入 | 用途 | 時長的取捨 |
|---|---|---|---|---|
| 1 | C0 | `none` 1000 ms | J-R6、J-R7 | 對照組 |
| 2 | C1 | `both` 250 ms | J-R1 | T_int／2 |
| 3 | C2 | `both` 1000 ms | J-R2、J-R9 | 2×T_int |
| 4 | C3 | `both` 3000 ms | J-R2、J-R9、Hello 探測 | T_max−2 s |
| 5 | C4 | `uplink` 1000 ms | J-R2（只上行）、J-R9 | 2×T_int |
| 6 | C5 | `uplink` 3000 ms | 同上 | T_max−2 s |
| 7 | C6 | `downlink` 550 ms | J-R2d（短）、J-R9 | 見下 |
| 8 | C7 | `downlink` 1500 ms | J-R2d（長）、J-R9 | 遠離 Starvation 的邊界 |
| 9 | C8 | `partial` 1000 ms，6 個中 1 個準時、其他晚 3000 ms | J-R4（晚到過久）、J-R9 | 晚到 3 s ≫ N_o |
| 10 | C10 | `port` 1000 ms | J-R8、J-R9 | 2×T_int |
| 11 | C9 | `both` 7000 ms | J-R5 | T_max＋2 s；會結束 session，排最後 |

**C6 的邊界**：

- 下限是 T_int：恢復時的間隔 ＝ 窗口長度＋（布設 − 最後一份 snapshot）≥ 550 ms，所以離 500 ms 還有 50 ms 以上。
- 上限是 Starvation：Match 用完 Client 窗口裡的 12 個命令之後，再 30 Tick 才會判定。在 localhost 上約是布設後 650～683 ms，而恢復後的第一個命令約在布設後 552～590 ms 到達 Match，所以還有約 60～130 ms。
- 第 1 版用的 600 ms 在這一側只剩約 10～70 ms（D45 Major），所以改成 550 ms。

## 3. 判定（每一輪、每一位玩家）

**計數範圍**：重設事件指 `match-commands.jsonl` 的 `reset`，只算布設之後到窗口結束後 1 s 為止的；C8 再延長 3 s（晚到的副本）。「fuse 重設」指原因為 starvation 或 resync 的重設。

**全部適用**：

- 沒有 `connection failed`；Gateway 日誌在第一行 `player left` 之前沒有 `player expired player=N reason=session_timeout`。C9 除外。
- 沒有 `session rebound` 行。C10 除外。

**各條判定**：

- **J-R6（C0）、J-R1（C1）**：`connection interrupted` 0 行；摘要行 `interruptions=0`；fuse 重設 0 次。
  - 來源：T_int＝0.5 s。v7.1 語料中，沒有注入停頓的跑次 snapshot 最大間隔 0.119 s；注入 250 ms 停頓時最大 0.283 s。
- **J-R2（C2、C3、C10）**：
  - `connection interrupted` 與 `connection resumed` 各恰好 1 行；
  - 中斷行的 `gap_ms` 在 [500, 520]；
  - fuse 重設恰好 1 次。
  - 來源：§4。T_int 從最後一份有效 snapshot 起算；520 是 500 加上一次 worker 喚醒的上限。
- **J-R2 Hello 探測（C3）**：從布設後 0.6 s 到窗口結束，中繼收到（並丟掉）的 Hello 每秒 3～5 個。
  - 來源：§4，中斷期間 Hello 改成每 250 ms 一個。
- **J-R2 只上行（C4、C5）**：`connection interrupted` 0 行（§7 已知項目）；fuse 重設恰好 1 次。Backlog 只記錄（P2 已知例外）。
- **J-R2d（C6、C7）**：中斷與恢復各 1 行；中斷行 `gap_ms` 在 [500, 520]。
  - C6：恢復行的 `epoch_before == epoch_after`；fuse 重設 0 次。
  - C7：`epoch_after == epoch_before + 1`；fuse 重設恰好 1 次，原因是 starvation。
- **J-R9（C2～C8、C10）**：恢復後真的回到正常遊玩。
  - 窗口結束後 3 s 內，Match 接受該玩家的命令（`host_accepted`）至少 90 個（1.5 s 的量）；
  - Client 至少做 1 次 `first` 相位決定；
  - 恢復行的 `epoch_after` 等於 Match 當時的 epoch（窗口結束後 1 s 內最後一次重設的 epoch，沒有重設時就是 `epoch_before`）。
  - 來源：探索輪中，恢復後 1 s 內接受 60～62 個；第一次決定在 175～190 ms，部分送達約 2.2 s，換埠約 1.2 s（第 8 節）。
- **J-R4（全部）**：
  - **只算有第一份副本到達的替代紀錄**（`match-ingress.jsonl`）：
    - `late_us > 333,333` 的筆數 ＝ 該玩家 `resync statistics` 的 `late_beyond_no_commands` 合計；
    - `late_us ≤ 333,333` 的筆數 ＝ ingress 行的 `slack_late_samples` 合計。
  - 合計涵蓋所有 `final=0` 與 `final=1` 的行。
  - `slack_clamped_samples` 合計 ＝ 0。被 ±1 s 夾限改變的樣本，只會來自晚到超過 1 s 的副本，所以這條在 L2 守住「晚到過久不產生樣本」裡超過 1 s 的那一段。
  - 這兩條等式只證明紀錄與計數器一致，本身不證明「不產生樣本」。333 ms～1 s 這一段由 `match-resync` 的守衛測試負責。
- **J-R4 C8**：fuse 重設恰好 1 次，原因是 resync；`resyncs == 1`；`late_beyond_no_commands ≥ 1`。
- **J-R5（C9）**：
  - 至少一位玩家有 `connection failed ... reason=Gateway connection timed out`，而且之前有 1 行 `connection interrupted`（`gap_ms` 在 [500, 520]）。
  - 這位玩家的 T_max 在 [4980, 5060] ms，量法：中繼收到該 session 最後一個上行封包的時刻，減去布設前轉給它的最後一個下行封包的時刻。
    - 下限 4980：Client 以 60 Hz 重送，最後一個上行封包最多早於斷線一個重送間隔（16.7 ms）。
    - 上限 5060：計時器的準確喚醒與 loopback 的延遲。
    - 這個範圍抓得到「T_max 從進入中斷才起算」這種錯法（約 5500 ms）。
  - `relay.json` 的 `ended_by == "run"`。
  - 另一位玩家沒有斷線行，而且最後一個上行封包和前者相差超過 17 ms 時，判 E 而不是 F：凍結的 probe 在第一個 Client 失敗時就結束兩個連線，第二行出不出現是競態。
- **J-R7（全部，C9 除外）**：
  - `ingress_evidence.py` 的 S1、S2、J5a 都是 `passed`。C0 不帶 `--injection`，其他都帶（J5a 因此要求 `never_arrived == 0`）。
  - S3（由 `judge_outage.py` 計算，用 retention-600 的寫法）：每位玩家恰好一行 `final=1` 的 ingress 行與一行 `final=1` 的 resync 行，而且 `late_first == slack_late + late_beyond_no`。
  - Match 的 end 行 `reason=signal`、`exit_code=0`。
- **J-R8（C10）**：每位玩家恰好 1 行 `session rebound`，`old_port` 是中繼原本的埠，`new_port` 是 `relay.json` 記錄的該 session 新埠。

## 4. 前提（不成立時判 E，不是 F）

- 證據檔齊全：`relay.json`、三個日誌與三個紀錄檔。C9 不要求 `action-client.json`。
- 注入的種類、時長、參數和第 2 節一致，`round_problems` 沒有問題。
- 兩位玩家（player 1 與 2）都加入。
- C0、C1、C9 以外：該玩家在 [布設 − 1 s, 它第一次 fuse 重設的時刻] 內沒有 life_respawn；沒有 fuse 重設時，終點是窗口結束（C8 再加 3 s）。重生會設定冷卻，讓 Starvation 不觸發（§4「不換 epoch 的情況」），新的 life 也會讓晚到的副本變成舊 life。fuse 之後的重生不影響判定（乾跑的 C8 中，player 2 在 Resync 之後 0.75 s 重生；第 1 版的前提把它誤判成 E）。
- `match-ingress.jsonl` 的 `trace_end` 恰好一行，而且 `dropped == 0`。`suppressed` 只發生在拒收紀錄上，所以不影響。
- C8：窗口內至少有一筆替代紀錄。

## 5. 只記錄、不判定

- J-R3：恢復後同一個 epoch 內的相位決定，最新樣本是否都是恢復之後產生的命令。
  - 改成只記錄的原因：中繼造出的中斷都會讓 Match 持續替代，所以「epoch 相同、權威 < tip」的 `resume_reset` 路徑不會出現（探索 13 輪中 0 次）。這一條判定在 L2 不可能失敗。
  - `resume_reset` 由 `client-interrupted` 的單元測試與突變守住（§7 守衛清單）。
- 其他只記錄的量：
  - `phase_samples_unmatched`、Late 決定次數；
  - Backlog 與 life_respawn 重設、`reason=resume` 的時鐘行數；
  - 品質視窗的失敗次數、`transient_socket_errors`；
  - `run_case` 的判定（中斷會打亂預先安排的命中，預期失敗）；
  - 恢復的 `gap_ms`、改綁的 `silent_ms`；
  - C9 的 `player expired`：probe 自己在結束時送出 Leave（`player left`，Match 端是 `reason=client_request`），通常比 Gateway 的到期早，所以看不到。這部分由 Go 測試守住（`Expired`，本線未改）。
- **L2 不涵蓋**：Welcome 不延長 T_max、Match 活著卻不出 snapshot、`resume_reset`。這三項由單元測試與 worker 測試（`RunOutage`）負責。

## 6. 結果的分類與停止規則

- **P**：該輪所有適用的判定都成立，前提也都成立。
- **F**：前提成立，但任一判定不成立。
- **E**：任一前提不成立。
- **停止**：每一項的 3 輪跑完後判定。只要有一輪是 F 或 E，就停止，之後的項目不跑，回報使用者。同一項中已經跑完的輪照樣判定並保留，不被之後的結果覆蓋（§14.1）。
- **通過**：33 輪全部是 P。
- **不重試**。

## 7. 門檻來源的對抗式檢查（D45，2026-10-24）

小規模 ultracode：程式審查 1 位、證據審查 1 位、對抗式反駁 1 位（`wf_603e73b4-cb9`）。反駁者的結論是，第 1 版在正確的程式上幾乎一定會停（C8），而且有幾條判定抓不到它要守的缺陷。處置如下：

| 審查項目 | 處置 |
|---|---|
| Critical：C8（3 個中 1 個準時）造不出晚到過久。每個 Input 都帶著整個未確認的窗口，所以幾乎每個命令都有準時的副本 | 改成 6 個中 1 個準時。探索 2 輪（期數 6）與 2 輪（期數 8）都是每位玩家 Resync 1 次、`late_beyond_no` 1、Starvation 0。加前提「窗口內至少一筆替代」 |
| Major：J-R4 的等式在結構上一定成立，抓不到洩漏的樣本 | 加判 `slack_clamped == 0`；`trace_end` 的 `dropped == 0` 改成前提；改寫結論句（第 3 節） |
| Major：沒有判定檢查恢復後真的回到遊玩；J-R3 在集合為空時也通過 | 新增 J-R9 |
| Major：J-R3 在宣告的情境中不可能失敗 | 改成只記錄，寫明 L2 不涵蓋 `resume_reset` |
| Major：J-R5 的上限 5600 會放過「T_max 從中斷起算」（約 5.5 s） | 改用每個 session 的中繼事件量 T_max，範圍 [4980, 5060] |
| Major：C6 離 Starvation 的實際餘裕約 70 ms，不是 100 ms | 改成 550 ms，寫明上限是 Match 停止 12 個命令後的 30 Tick |
| Major：J-R7 要的 S3，凍結的分析器不算 | S3 由 `judge_outage.py` 計算；寫明 `--injection` 的使用 |
| Major：停止規則和工具不符，沒有執行順序 | 固定順序；每項 3 輪後判定；規定各情境必要的證據檔 |
| Minor：Gateway 到期沒有寫出是哪一行；也沒查意外的改綁 | 寫明 `player expired ... reason=session_timeout`，掃描到第一行 `player left` 為止；C10 以外要求 `session rebound` 0 行 |
| Minor：J-R5 第二位玩家的斷線行是競態 | 至少一位；另一位不符時判 E |
| Minor：沒有判定釘住 T_int 的值與 Hello 探測 | 加判 `gap_ms` 在 [500, 520]、C3 的 Hello 每秒 3～5 個；Welcome 與 Match 不出 snapshot 兩項列為 L2 不涵蓋 |
| Minor：冷卻的例外寫在錯的時段（布設前 1 s 內不會發生） | 改成前提：[布設 − 1 s, 第一次 fuse 重設] 內沒有 life_respawn（乾跑後修正終點，見第 4 節） |
| Minor：第 8 節的探索輪數與工具版本不準 | 改正第 8 節；開跑前以最終工具乾跑全部 11 項各 1 輪 |
| 程式審查者說 Gateway 的到期通常先於 SIGTERM | 反駁者以證據推翻：probe 自己的 Leave 先到。理由改寫（第 5 節） |

## 8. 探索性試跑（不作判定）

- **工具開發期間**（`pvp-v71-resync-l2-prep-20261024/dev/`，17 輪）：用了 5 個不同版本的 `run_outage.py`，而且都在工作樹有未提交修改時跑的。這些只用來確認注入有效，不作為判定的根據。主要觀察：
  - 雙向 1 s 與 3 s：每位玩家中斷 1 次、恢復 1 次、Starvation 1 次；
  - 只下行 0.6 s：恢復時 epoch 不變；1 s 時 Starvation；
  - 只上行：Client 不中斷，Starvation 1 次、Backlog 1 次；
  - 換來源埠：每位玩家改綁 1 次，`silent_ms` 約 2 s。Gateway 在改綁前仍把 snapshot 送往舊埠，中繼照樣轉給 Client，所以 Client 先恢復，改綁要等下一個每秒一次的 Hello；
  - 雙向 7 s：兩位玩家在 5 s 時斷線；
  - 雙向 0.25 s 與對照組：沒有中斷。
- **最終工具的乾跑**（11 項各 1 輪，不作判定）：
  - `dryrun-0-relative-path/`：驅動把相對路徑交給在另一個目錄執行的 runner，輸出寫錯位置，判 E。改成絕對路徑。
  - `dryrun-1/`：C0～C7 都是 P；C8 判 E（重生的前提太寬，見第 4 節），修正後用同一份證據重判為 P。
  - `dryrun-2/`：C8、C10、C9 都是 P。
- **判定工具的鑑別力**（`judge-tamper-2/`）：從通過的乾跑輪複製出 10 份竄改版，每份只改一處，模擬一種錯誤的行為。10 份全部判出預期的結果：
  - 少一行恢復、T_int 變成 700 ms、恢復後 Match 沒有再接受命令、session 到期、意外的改綁：F；
  - 被夾限的樣本 1 個、短下行中斷卻換了 epoch、T_max 從中斷才起算（5496 ms）、晚到紀錄與計數不符：F；
  - ingress 紀錄被丟棄：E。

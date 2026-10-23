# outage-resync：斷線恢復＝重新對時（C13 (b)＋UDP 中斷狀態）

- 狀態：**核准**（v7.1 D172，2026-10-23：第 8 節的 D-1～D-14 全部照建議）。`gate-characterize`（D173）：T_int＝0.5 s、N_o＝20 Tick、D-15＝(a)。`gate-contract`（D174）：條文核准。D-13 在 L2 前另行宣告核准。
- 分支：`claude/pvp-v71-outage-resync`，從 master `cdb9b07` 開出。這是新的 PR 線，與記錄器分支 `claude/pvp-v71-after-re-evaluate` 分開。
- 規劃方式（D171）：ultracode。2 位偵察、3 份計畫（最小契約、契約先行、風險優先）、1 位評審綜合、1 次對抗式審查。審查判定「必須修訂」，本版已套用；對照見第 11 節。
- 引用的 file:line 以 master `cdb9b07` 為準。

## 代號對照

| 代號 | 白話 |
|---|---|
| `plan-commit` | 提交本計畫 |
| `outage-characterize` | 用測試把現在的斷線與晚到行為釘住 |
| `resync-contract` | 把狀態與規則寫進契約文件 |
| `drop-counters` | 把看不到的丟棄補上計數 |
| `single-store-refactor` | 被替代的序號只存一份，行為不變 |
| `match-resync` | Match 看到中斷就換新的移動 epoch |
| `retention-600` | 保留範圍改成 600 個序號（宣告過的行為改變） |
| `client-interrupted` | Client 的「連線中斷」狀態與恢復流程 |
| `identity-rebind` | 換了位址的同一個 session 能接回來 |
| `resync-l2-l3` | 注入中斷的 L2，以及你在場的 L3 |
| `line-close` | 收尾 |
| `gate-characterize` | 關卡：依特性報告決定 T_int 與 N_o（舊稱 G1） |
| `gate-contract` | 關卡：你核准契約條文，之後才改程式（舊稱 G2） |

關卡原本叫 G1、G2，和 INVENTORY 的項目代號 G1、G2（例如 D159「G2 先不做」的「時鐘到網路路徑」系列紀錄）撞名，2026-10-23 改名。舊的 commit 訊息與 HANDOFF 決策紀錄裡的 G1、G2 指的是這兩個關卡時，照這張表讀。

## 現況

- 2026-10-23：草案第 2 版提交，D172 全部照建議核准。
- 2026-10-23：`outage-characterize` 開始（high，構造時序的部分局部 xhigh）。
- 2026-10-23：`outage-characterize` 完成（[特性報告](outage-characterize-report.md)）。`gate-characterize` 的決定見下一行。
- 2026-10-23：`gate-characterize` 決定（D173）：T_int＝0.5 s、N_o＝20 Tick、D-15＝(a)（只記錄，交給 B6）。L1 確認後開始 `resync-contract`。
- 2026-10-23：`outage-characterize` 的 L1（run 39944311104）四平台成功。`resync-contract` 開始（high）。
- 2026-10-23：`resync-contract` 的條文寫完，xhigh 對抗式審查（1 次）判定「必須修訂」，已全部處理（第 13 節）。
- 2026-10-23：依你的要求做第二輪 xhigh 複核：判定「必須修訂」（Critical 1、Major 2、Minor 8），已全部處理（第 13 節第 8 點）。
- 2026-10-23：第三輪 xhigh 複核：判定「可送核准」（Minor 7 項，已全部處理，第 13 節第 9 點）。
- 2026-10-23：`gate-contract` 核准（D174）：條文照現狀（含三輪審查的修訂）。`drop-counters` 開始（high）。
- 2026-10-23：`drop-counters` 完成（`3a3e1b1`）：`resync statistics` 行（`slack_clamped_samples`）、`phase summary` 行（`phase_samples_unmatched`）；突變 7／7；L1 run 39956082177 待確認。下一批 `single-store-refactor`（high，紀錄身分與關閉順序局部 xhigh）。
- 2026-10-23：`drop-counters` 的 L1（run 39956082177）四平台成功。`single-store-refactor` 完成（`832eca4` 等）：被替代序號的紀錄只存一份（`MatchSubstitutionRecords`），行為逐位元不變（xhigh 等價性審查、差分 fuzz 0 差異）；突變 79／79＋4／4。§7 條文的 owner 名稱照實作改正。L1 待確認。下一批 `match-resync`（xhigh，開始前問你）。
- 2026-10-23：§7 的 owner 改名核准（D175）。`match-resync` 在 L1 通過後以 xhigh 開始（使用者同意 xhigh）。
- 2026-10-24：`single-store-refactor` 的 L1（run 39967478342）四平台成功。`match-resync` 完成（`426aa4b` 等）：晚到超過 N_o 改成 Resync，發布的 slack 不再低於 −333,333 µs；特性測試升格為 §7 守衛；突變 10／10；L1 run 39974643016 待確認。下一批 `retention-600`（xhigh，開始前問你；§14.4 的材料交你核准）。
- 2026-10-24：`match-resync` 的 L1（run 39974643016）四平台成功。`retention-600` 的程式完成（`d1bcb47` 等），突變 84／84。**等你核准**：[判定變更 J-A～J-C 與 L2 不退步紀錄的宣告草案](retention-600-judgment-change.md)。L1 與 L2 在核准後進行。
- 本線的現況寫在這一節，每批與工作同一個變更更新。開發紀錄是 `docs/dev_logs/2026_10_23_pvp_v71_outage_resync.zh-Hant.md`，從 `outage-characterize` 起每批同步寫。
- v7.1 的 HANDOFF、README、after-re-evaluate 只在記錄器分支更新：分線時、你做決定時、收尾時各更新一次。

---

## 0. 固定輸入（你的決定）

**D171（2026-10-23）**：
> 「選（b），合併後：遲到 1～10 秒的命令也會產生 slack 樣本，斷線恢復時，Client 會收到很大的負值的這個問題，用斷線重連的方式（重新對時？）來做。新開PR，用Ultracode來跑這個case的規劃」

**設計原則**（2026-10-23 的補充，記錄器分支 `6c473af`）。重連後要重新建立的是「時間上的對齊」：

- **P1 時鐘**：時間回聲重新取樣，不沿用斷線期間的時鐘視窗。
- **P2 相位**：重新擷取送命令的時機，不沿用斷線前的領先量（B6 那一區的機制）。
- **P3 命令序列**：用新的移動 epoch 重新起步，斷線期間的舊命令直接丟掉，不補算。
- **P4 身分**：同一位玩家、同一個 session 要能接回來。這是 Gateway 無狀態原則要特別設計的部分。
- **P5 形式**：UDP 加應用層的存活判斷，重連時重新對時。要寫成明確的狀態與規則：
  - 偵測門檻；
  - 中斷期間的行為；
  - 恢復流程；
  - 真正斷線的上限。

**旁支會話轉達**：斷線偵測與重連只走 UDP，不新增 TCP 心跳或控制通道。

## 1. 已核實的事實

以下由偵察與審查對照程式確認：

- **完整中斷時，Match 已經做了一部分重新對時。**
  - Starvation 保險絲：連續 30 Tick 沒有領先量、有替代、佇列是空的，就換 epoch（`PvpMatch.cpp:511-519`）。
  - 換 epoch 時游標歸零（`:416-431`），舊 epoch 的命令被拒，回 `EpochOld`（`:72-73`）。
  - Client 會清掉送出窗口（`ClientConnection.cpp:608-611`），並執行 Reset＋SeedLead（`LocalPlayerPrediction.cpp:217-231`）。
- **缺口一：冷卻。**
  - 保險絲受 60 Tick 冷卻限制（`PvpMatch.cpp:505-506`）。
  - 每次重生也會設定這個冷卻（LifeRespawn 同樣寫 `lastMovementResetTick`）。PvP 重生很頻繁，所以落在冷卻期內的中斷並不罕見。
- **缺口二：部分送達。** 有命令陸續送到時佇列不空，保險絲不觸發（`PvpMatchTests.cpp:1337-1373`）。
- **缺口三：Client 的過濾不夠。**
  - Client 只要求 `seq > settledAfter_`（`LocalPlayerPrediction.cpp:132-134`）。序號還在 64 格環裡的晚到樣本就會被採用。
  - 連續兩個負樣本會立刻觸發 Late 修正（`:150-157`）。
  - stallReseed 只在 `authority.lastResolvedCommand > oldTip` 時發生（`:241-244`）。只有上行中斷時，Client 照常收到 snapshot，tip 跟著前進，不會 reseed。
- **結論（審查 C2）**：下列三種情況，大負值樣本**現在就會進入 Client 的相位追蹤**：
  - 只有上行中斷，而且在保險絲觸發前就恢復；
  - 中斷落在冷卻期內；
  - 部分送達。

  C13 (b) 把保留範圍改成 600 之後，「1～10 秒的晚到」也會產生樣本。這就是 D171 要處理的問題。
- **夾限**：slack 在發布時夾在 ±1 s（`MatchRuntimeHost.cpp:663-667`），所以最大的負值是 −1,000,000 µs。
- **兩份紀錄**：`track.substituted` 保留最新 64 筆（`MatchRuntimeHost.cpp:17`、`:662`）；帳本保留 600 個序號，容量 1024（`MatchIngressLedger.hpp:38-39`）。同一個常數兼作回看保險（`MatchRuntimeHost.cpp:639-640`）。
- **Client 的 64 格環**（`LocalPlayerPrediction.hpp:160`）：對不上時直接丟棄，不計數，但會更新 `lastSlackSequence_`（`.cpp:139-141`）。這違反死信原則（`log-contract.zh-Hant.md:85`）。
- **兩端的上限**：
  - Client：Playing 時從最後一個有效封包起算 5 s 就逾時（`ClientConnection.cpp:654`）。Welcome 也算有效封包，而 Welcome 每 1000 ms 一次（`:478`），所以 Gateway 還活著時 Client 不會逾時。
  - Gateway：session 期限 5 s（`apps/object_fps_pvp/gateway/server.go:30`），只有更新的傳輸序號才會刷新（`services/gyo_gateway/session/session.go:78-80`）。
- **身分**：
  - Hello 只接受原本綁定的 endpoint（`session.go:70-72`，註解寫明「Endpoint rebinding is not in v1」）。
  - 契約原文：「UDP endpoint 不可用舊 session 靜默切換」（`network-architecture.zh-Hant.md:540`）。
  - 但 endpoint 屬於允許的 session 軟狀態（`network-architecture.zh-Hant.md:137`）；依傳輸序號與 session 生命週期做跨訊息判斷也是允許的。所以「換 endpoint 接回」可以在無狀態原則內設計，不需要 Gateway 持有 Match 的判定狀態。
- **socket 錯誤**：收送時只要有錯誤就丟例外，接著 Failure（`ClientConnection.cpp:389`、`:492`）。
- **品質**：中斷 ≥0.5 s 時，光是替代率超過 5% 就會讓該品質視窗失敗（`MatchRuntimeHost.cpp:695-697`），和是否換 epoch 無關。連續 3 個失敗視窗就逐出，約 30 s。
- **時鐘域**：macOS 上 C++ 的 `steady_clock` 跨睡眠繼續走，Go 的單調時鐘會停（HANDOFF.md:359 的實測）。
- **語料**：既有語料裡 `late_first == slack_late` 全部相等；`late_us` 最大 221,871 µs，沒有一筆 ≥250 ms。行為改變只能靠構造的測試證明。
- **權威 digest 不涵蓋 slack**（`AuthorityDigest.cpp:46`）。

---

## 2. Feature（玩家看得到的部分）

- **F1 短暫抖動**（< T_int）：和現在一樣，畫面沒有新提示。
- **F2 連線中斷**（T_int ≤ 中斷 < T_max）：
  - HUD 顯示「連線中斷，重新連線中…」；
  - 玩家留在對局，名額保留；
  - 中斷期間的操作見 D-9。
- **F3 恢復＝重新對時**：
  - 角色從權威位置重新起步，不快轉補走；
  - 不會因為中斷而被 late 修正多加領先；
  - 時鐘、相位、命令序列都重新對齊（P1～P3）。
- **F4 真正斷線**（≥ T_max）：回 Lobby 並顯示原因。T_max 在 Client 與 Gateway 是同一個值。
- **F5 換了位址也能接回**（P4，範圍依 D-2）：中斷期間 Wi-Fi 重連、NAT 對外埠改變時，同一個 session 能接回來，不必重新加入。
- **F6 宣告的行為改變**（C13 (b)）：
  - 被替代序號的紀錄只有一份，保留 600 個序號；
  - 搭配 F3 的「晚到太久就重新對時」，1～10 秒的晚到不再變成 −1 s 樣本，改成觸發換 epoch（第 3.2 節）。

**不在本線範圍**：

- 超過 T_max 後自動重新加入（wake-reconnect 的 w2，見 D-11）；
- v8 B6 的撤回、B10 的相位追蹤 golden；
- 加密或防竄改的 session 證明（需要改 wire，pv8 等級，見 D-2 的 I-3）。

---

## 3. 狀態與規則（P5）

### 3.1 Client：連線狀態（Playing 期間）

| 狀態 | 進入條件 | 期間的行為 | 離開條件 |
|---|---|---|---|
| **連線** | 進入 Playing；或恢復流程完成 | 現行行為 | 最後一份有效 snapshot 起 ≥ T_int → **中斷**；非暫時性錯誤 → **斷線** |
| **中斷** | 同上 | 見下方「中斷期間的行為」 | 收到有效 snapshot → 恢復流程 → **連線**；最後一份 snapshot 起 ≥ T_max → **斷線**；非暫時性錯誤 → **斷線** |
| **斷線** | 同上；或 Gateway 的 Leave、Error | 既有的 Failure：關閉傳輸、回 Lobby、顯示原因 | 終點。重新加入走新的 join |

**偵測門檻**：

- **T_int**（進入中斷）：只看 snapshot 的到達時刻，因為「snapshot 串流就是心跳」。
  - 候選 0.5、0.75、1 s。
  - 來源：`outage-characterize` 量既有 clean 語料的 snapshot 間隔分布。已知 Gateway 會停頓約 1.3 s（`network-architecture.zh-Hant.md:799`），所以 0.5 s 可能在 clean 跑次裡誤報。
  - 在 `gate-characterize` 決定（D-4）。
- **T_max**（真正斷線）：Playing 期間**從最後一份 snapshot 起算**。
  - 現在是從任何有效封包起算，而 Welcome 每秒都會到。如果 Gateway 活著、Match 卻不出 snapshot，「中斷」會沒有上限（審查 minor 6）。
  - 值見 D-11（建議維持 5 s）。
- 到達時刻只用收包當下的 `steady_clock`。ClockSync 的 offset、delay、回聲都不得進入判定（`protocol-v7.zh-Hant.md:51-53`）。

**中斷期間的行為**：

- **保留**：socket、session 憑證、傳輸序號（`sentSequence` 不歸零）、動作帳本。不呼叫 CloseTransport 或 ResetActions，不換連線 generation（`ClientConnection.cpp:866-868`）。
- Hello 間隔從 1000 ms 改成 250 ms，作為探測。速率上限的註解（`Movement.hpp:39-40`）要重算：Input 82、Hello 4，加起來仍在 120 包／秒以內。
- Input 與動作照常產生、照常送出（D-9）。
- 暫時性 socket 錯誤只計數，當作「這次沒有資料」，不走 Failure。範圍：connection_reset、refused、network_unreachable、host_unreachable、network_down（D-7）。
- 相位追蹤沒有新的 snapshot，所以不會有新樣本。
- Client 不推進 epoch；中斷狀態只供顯示，不進任何權威判定。

**恢復流程**（收到第一份有效 snapshot 時，依序執行）：

1. **時鐘（P1）**：
   - Client 結束目前的 ClockSync 視窗，寫 final 行（行尾加 `reason=resume`，D-8），再開新視窗。
   - 時鐘值只供診斷，不進第 2、3 步。
2. **命令序列（P3）**：比較 snapshot 的 movement epoch 和中斷前的 epoch。
   - **不同**：Match 已經判定命令序列中斷（第 3.2 節）。走既有的換 epoch 路徑：清掉送出窗口、Reset＋SeedLead。舊命令已被 Match 以 `EpochOld` 丟掉。
   - **相同**：Match 的命令序列沒有中斷（例如只有下行中斷，命令照常到達），沒有需要丟的舊命令。
3. **相位（P2）**：不論 epoch 是否不同，都**明確**呼叫既有的 SeedLead。
   - SeedLead 會把 `settledAfter_` 設成權威游標（`LocalPlayerPrediction.cpp:91`），中斷期間產生的序號不再成為樣本；
   - 下一次相位決定照「第一次決定」處理。
   - 現在相同 epoch 時要靠 stallReseed 的條件剛好成立；本線改成恢復時一定做。
   - 不新增相位邏輯，只多一個呼叫點。
4. **顯示**：HUD 的中斷提示消失，寫 A 級的 `connection resumed` 行（第 4 節 K5）。

### 3.2 Match：命令序列中斷的判定（epoch 的權威）

- **偵測**（Match 看得到的中斷）：
  - (a) Starvation 保險絲：現行規則，30 Tick。
  - (b) **新增：晚到過久**。命令到達時，它的序號已被替代，而且「游標 − 序號 > N_o Tick」。
- **處置**：
  - 換新的 movement epoch，原因記為 Starvation 或 Resync（schema 3 的列舉要加 Resync，A 級）；
  - 觸發 Resync 的那筆命令不產生 slack 樣本；
  - 之後到達的舊 epoch 命令一律 `EpochOld`。
- **冷卻**：
  - Resync 不受 60 Tick 冷卻限制，否則落在重生後冷卻期內的中斷就漏掉了；
  - 同一個 epoch 內最多一次 Resync：換 epoch 後，舊命令都是 `EpochOld`，不會再觸發。
- **N_o 的來源**：既有語料的晚到分布，目前最大約 13 Tick（221,871 µs）。候選 30 Tick（0.5 s），在 `gate-characterize` 依特性報告決定（D-3）。
- **效果**：
  - 所有發布的 slack 都 ≥ −N_o Tick 對應的 µs，大負值在源頭消失；
  - 這也涵蓋部分送達、只有上行中斷、冷卻期內這三個缺口。
- **品質**：維持現行，中斷造成的替代照算（D-10）。
- **上限**：Match 自己不判斷斷線，玩家離開仍由 Gateway 的 Leave 決定。

**只有下行中斷時**，Match 收得到命令，不會換 epoch，Client 恢復時走第 3.1 節第 2 步的「相同」分支。這不違反 P3：命令序列本身沒有中斷，也沒有舊命令要丟。

**pv7 的限制**（審查 M1）：epoch 只由 Match 推進，Match 看不到 Client 的下行中斷。不加 wire 訊息（pv8 等級），就只能保證「Match 看得到的中斷會換 epoch」。Client 的相位一律重新擷取（第 3 步），不受這個限制。

### 3.3 Gateway：session、時鐘、身分

- **活性**：現行規則，只有更新的傳輸序號才刷新 `lastSeen`。
- **上限**：sessionTimeout（5 s）到期 → expired → Leave(SESSION_TIMEOUT)，維持現行。T_max 與 Client 同值，寫在同一條契約。
  - 時鐘域不同：Gateway 從最後一個上行封包起算，Client 從最後一份 snapshot 起算。
  - Gateway 和 Client 在同一台睡眠的筆電上時，Go 的時鐘會停，Gateway 可能不讓 session 到期。這寫進契約。
- **時鐘（P1，審查 M5）**：
  - 上行靜默 ≥ T_int 後收到第一個封包時，結束並重開該 Client 的時鐘視窗（`client clock sync player=`）。
  - 這是依 session 生命週期的判斷，屬於允許的類別（`network-architecture.zh-Hant.md:137`、`:758`）；狀態放在診斷狀態，白名單不變。
  - Match 的時鐘視窗量的是 Gateway↔Match，不受單一 Client 中斷影響，不動。
- **身分（P4）**：依 D-2，建議 I-2：
  - **重新綁定的條件**（全部成立才允許）：
    - Hello 的 token 正確；
    - 傳輸序號比 session 記錄的新；
    - 原 endpoint 已靜默 ≥ T_int；
    - session 還沒到期。
  - **之後**：原 endpoint 的封包回 `ErrUnauthorized`。
  - **紀錄**：每次重新綁定寫一行 A 級 `session rebound player= ...`（舊、新 endpoint 只記雜湊或埠，不記 token）。
  - **只用 session 軟狀態**（token、endpoint、序號、lastSeen），不需要知道 Match 的任何狀態。
  - Client 不換 token、不重設序號（第 3.1 節「保留」），所以這條規則在 Client 端不需要新邏輯。

---

## 4. Contract（Data 與 Wire）

原則：**wire 不變，維持 pv7**（D90），ingress 維持 v1。commands.jsonl 維持 schema 3，只在 movement epoch 的原因列舉加上 Resync，凍結讀者先查。

- **K1 Client 連線生命週期**：改寫 `network-architecture.zh-Hant.md` §4（`:538-547`），內容是第 3.1 節。
- **K2 Match 的命令序列中斷**：寫進 `network-architecture.zh-Hant.md` §7，內容是第 3.2 節。
  - 新增 Design Invariant「連線中斷與重新對時」：
    - epoch 只由 Match 推進；
    - Match 看得到的中斷一定換 epoch；
    - 發布的 slack ≥ −N_o；
    - 恢復時 Client 一定重新擷取相位。
  - 守衛測試的位置寫在條文裡。
  - 附 Replacement Criteria（第 5 節）。
- **K3 session 與身分**：
  - `network-architecture.zh-Hant.md:540` 的「不可靜默切換」改成第 3.3 節的重新綁定條件（I-2 時）；
  - `session.go:66` 的註解同步改；
  - 寫明 T_max 兩端同值，以及兩端的時鐘域。
- **K4 被替代序號的單一紀錄**（程式內的契約註解，加上 ingress_v1 README 的 `:58`、`:61`、`:95`）：
  - 權威是 MatchRuntimeHost 的 SlackTrack，記錄 (player, epoch, life, seq) → {substitutedAt, substitutedTick, firstArrival}。
  - 保留：游標 − seq ≤ 600，容量 1024。超出時照帳本既有的 overflow 或 aged 規則關閉並計數，不得無聲丟。
  - 每個序號只在第一次晚到時產生一個樣本；夾限仍在發布點。
  - 帳本只讀，或接收事件，自己只保留不參與判定的附欄。`match-ingress.jsonl` 的位元組不變。
  - 回看保險拆成獨立常數，值仍是 64。
  - `protocol-v7.zh-Hant.md:55` 的差分 oracle 加上「帳本讀 SlackTrack」這條路徑（審查 minor 8）。
- **K5 日誌**（審查 M3、M4）：
  - **L2 要讀的行一律是 A 級**，用新的事件名加 `version=1`，附釘住測試，並確認錨點不和既有前綴衝突（`log-contract.zh-Hant.md:121`）：
    - Client：`connection interrupted version=1`、`connection resumed version=1`（含 gap_ms、epoch 是否改變），以及每次斷線前一行 `connection summary version=1`（中斷次數、暫時性錯誤次數、`phase_samples_unmatched`）；
    - Match：`resync statistics version=1`（每個統計視窗一行：Resync 次數、`slack_clamped`）；
    - Gateway：`session rebound version=1`（I-2 時）。
  - **不追加到 ingress 行**：ingress 已有 8 組，再加會觸發 log-contract 的 Replacement Criteria 1（`log-contract.zh-Hant.md:128`）。
  - clock sync 行（A 級，`:37`）的行尾追加 `reason=`（R1，D-8）。
  - S3（`slack_late == late_first`）加在 C++ 的守恆檢查，不動凍結的 `ingress_evidence.py`（D-12）。
- **K6 診斷隔離**：
  - `protocol-v7.zh-Hant.md:53` 的「5 秒的 session／runtime／Client 逾時」改列 T_int、T_max、N_o 與重新綁定的條件。
  - `ConnectionLiveness` 列入 `test_diagnostic_isolation.py` 的 DECIDING 清單。

## 5. Architecture 與 Architecture Delta

不新增 subsystem。

### Delta-1：被替代序號的 State Authority 移到 SlackTrack（C13 (b)）

1. **為什麼需要**：F6，以及 C13。AGENTS §9 規定同一個事實只在一處維持。
2. **現在的問題**：
   - 同一個事實存兩份，規則不同（64 筆與 600 個序號）；
   - 其中一份參與 late 修正的輸入；
   - `slack_late ≤ late_first` 只在語料上相等，結構上沒有保證。
3. **變化的邊界**：Host 內部從「host 同時餵兩邊」變成「帳本讀 SlackTrack」的單向關係。
4. **影響範圍**：
   - `MatchRuntimeHost.{hpp,cpp}`、`MatchIngressLedger.hpp`、`IngressStatistics.hpp`、`PvpMatchTests.cpp`；
   - 突變目錄中目標在這三個檔的共 73 個（53＋16＋4），全部重新判定。
   - Engine、Gateway、Client、wire 不受影響。
5. **依賴方向**：只允許「帳本 → SlackTrack」。
   - 由新突變 `v7-flow-ledger-drives-sample` 守住；
   - `test_diagnostic_isolation.py` 新增一條檢查，也守這個方向。
6. **Ownership**：「已替代、第一次是否已到」由 SlackTrack 唯一持有。帳本只讀。
7. **為什麼不能更小**：
   - 只把 64 改成 600：仍是兩份；
   - 讓 SlackTrack 讀帳本：等於診斷驅動判定。

**Replacement Criteria**：出現下列任一項時，停下來交你決定修還是重建：

- 帳本為了分類，需要回寫紀錄；
- 第二次需要依診斷值改變保留規則；
- 保留範圍需要依情境設定。

### Delta-2：連線中斷狀態，以及重新對時的契約化

1. **為什麼需要**：F2～F4，以及 P1～P3、P5。
2. **現在的問題**：
   - Client 只有 5 s 一個出口，暫時性錯誤也直接 Failure；
   - 重新對時只是 Starvation 的副作用，沒有寫成契約，也沒有守衛；
   - 有冷卻與部分送達兩個缺口。
3. **變化的邊界**：
   - Client 新增中斷子狀態，ConnectionPhase 的公開值不變，CopyState 輸出旗標與時長；
   - 新增 header-only 的 `ConnectionLiveness`；
   - Match 新增「晚到過久」這個換 epoch 原因（Resync）；
   - `LocalPlayerPrediction` 新增「只重設相位擷取」的入口；網路 worker 交給模擬端的批次帶一次性的「恢復」旗標；
   - 契約在 §4 與 §7（第 13 節修訂）。
4. **影響範圍**：
   - `ClientConnection.{hpp,cpp}`（突變 20 個）、`LocalPlayerPrediction.cpp`（17 個）、`PvpApplication.cpp`（HUD）；
   - `PvpMatch.cpp`、`MovementTrace.hpp:19`、`command_evidence.py`；
   - Gateway 的 per-client 時鐘視窗；
   - `ClientSimulation`／`ClientSimulationRole`（消費恢復旗標、寫 `phase summary`）、`MatchRuntimeHost`（量晚到時間）、`PvpMatch.hpp`（新的公開排程介面）；
   - relay 工具。
5. **依賴方向**：新增一條邊，其餘不變。
   - **新增**：MatchRuntimeHost（SlackTrack 量晚到時間）→ PvpMatch 的公開排程介面（要求在下一個 Tick 換 epoch）。方向和既有的 Host → PvpMatch 相同，epoch 仍只由 PvpMatch 改變。替代紀錄的保留範圍因此成為遊戲規則的一部分。
   - `ConnectionLiveness` 只收 `steady_clock` 的到達時刻與設定，不依賴 ClockSync；
   - 中斷狀態不被 LocalPlayerPrediction 或 Match 讀取；相位追蹤只接收一次性的「恢復」旗標。
6. **Ownership**：
   - epoch 歸 Match；
   - 中斷狀態歸 Client，是 Client 自己的觀測，不是 Match 狀態的副本；
   - Gateway 只持有 session 軟狀態。
7. **為什麼不能更小**：
   - 只延長 5 s：做不到不離場的中斷；
   - 讓 Client 自己換 epoch：違反 epoch 的權威；
   - 加 wire 的 resume 訊息：要升 pv8。

**守得住「不讀 ClockSync」的方法**（審查 M8）：只靠 DECIDING 清單不夠，因為呼叫端 `ClientConnection.cpp` 可以把 ClockSync 推導出的時刻傳進去。所以另外做兩件事：

- 突變「用時鐘回聲的時刻餵 Liveness」，必須 killed；
- 差分 oracle：診斷值為極端值時，中斷判定必須逐位元相同。

**Replacement Criteria**：出現下列任一項時，停下來交你決定修還是重建：

- 需要 Gateway 記住中斷階段（重新綁定所需的 session 軟狀態除外）；
- 第二次需要在 Client 狀態機加中斷特例，才能通過驗收；
- 需要第四種和中斷有關的換 epoch 原因；
- 品質判定需要依「是否重連」分支。

### Delta-3：session 的 endpoint 重新綁定（只有 D-2 選 I-2 時）

1. **為什麼需要**：F5、P4。
2. **現在的問題**：endpoint 一改，Hello 就回 `ErrUnauthorized`，只能等到期後回 Lobby。
3. **變化的邊界**：
   - 通用 session 層（`services/gyo_gateway/session`）的 Hello 規則，以及契約 `:540`；
   - session 新增欄位「原 endpoint 最後一個通過 session 驗證的資料包的時刻」：Hello 的 token 比對或 `Admit` 的 endpoint 比對都刷新它，不論序號。產品 Gateway 的時鐘視窗重開也讀它；
   - T_int 由產品以參數傳給通用層。
4. **影響範圍**：`session.go`、`session_test.go`（現有的「endpoint hijack」測試改成「靜默不足時仍然拒絕」，並新增允許的情況），以及產品 Gateway 的日誌行與時鐘視窗。
5. **依賴方向**：不變。這是通用層，不認識產品。
6. **Ownership**：不變，endpoint 本來就屬於 session。
7. **為什麼不能更小**：P4 要求接回；不改通用層就做不到。

**安全取捨**：token 在 Hello 裡以原文傳送（`client_v7.proto:8`）。拿到 token、又能讓原 endpoint 的所有封包停止 T_int 以上的人，可以搶走 session。偽造原 endpoint 的封包只能阻止改綁，不能促成改綁。重新綁定的 A 級行讓事後查得到。真正的證明（HMAC 等）需要改 wire，屬於 I-3。

---

## 6. 分批

```
outage-resync（約 59～85 h）
├── plan-commit             medium｜本計畫、dev_log、記錄器分支的分線紀錄
├── outage-characterize     high，構造時序局部 xhigh｜在 HEAD 上用測試釘住現況
│   └── [gate-characterize] D-3（N_o）、D-4（T_int）：依特性報告決定數值
├── resync-contract         high，寫完做 1 次 xhigh 對抗式審查｜K1～K6 的條文
│   └── [gate-contract]     你核准條文，之後才改程式
├── drop-counters           high｜看不到的丟棄補計數，新的 A 級行族
├── single-store-refactor   high，紀錄身分與關閉順序局部 xhigh｜單一紀錄，行為逐位元不變
├── match-resync            xhigh｜晚到過久就換 epoch，Resync 不受冷卻限制
├── retention-600           xhigh｜保留範圍改成 600（§14.4）
├── client-interrupted      high，計時與恢復路徑局部 xhigh｜中斷狀態、恢復流程、HUD、Gateway 時鐘視窗
├── identity-rebind         xhigh｜session 重新綁定（D-2 選 I-2 時）
├── resync-l2-l3            medium，工具 high；門檻來源的對抗式檢查用小規模 ultracode｜注入中斷的 L2、你在場的 L3
└── line-close              medium｜收尾
```

- 檔位由我設定，每批開始前說明。標 xhigh 的批次，開始前再問你。
- 小規模 ultracode 的規模：約 3 個方案、1 位審查者、1 次對抗式檢查（D45）。
- `match-resync` 排在 `retention-600` 之前：保留範圍改成 600 時，晚到過久的處置已經在位，分支上不會出現「1～10 秒的晚到變成 −1 s 樣本」的中間狀態。

| 代號 | 內容 | 驗收 | 估時（h） | 決策點 |
|---|---|---|---|---|
| plan-commit | 本文件、dev_log；記錄器分支的 HANDOFF 記分線 | 只改文件 | 1～2 | D-1 |
| outage-characterize | 只改測試，不改產品。用注入的時鐘構造：(a) 雙向完整中斷 0.3／0.6／1／2／5／10 s，分冷卻期內外；(b) 只有上行、只有下行；(c) 部分送達，加一個晚到 3 s 的版本；(d) 稀疏替代跨過 600 序號；(e) Client：stallReseed、64 格環丟棄、連續負樣本；(f) 夾限。另外唯讀掃描 clean 語料的 snapshot 間隔分布與晚到分布。新增突變：夾限移除、保險絲恆為 false | L1 四平台；新突變 killed（只在預期的測試失敗時算）。特性報告附 file:line：哪些情況大負值進入 TrackPhase、保險絲的實際觸發 Tick、snapshot 間隔分布、晚到分布 | 6～8 | `gate-characterize`：D-3、D-4；報告若推翻前提，停下回報 |
| resync-contract | K1～K6 的文件條文；§7 的 Invariant 與 Replacement Criteria | 只改文件；1 次 xhigh 對抗式審查；你核准（`gate-contract`） | 4～5 | D-5～D-12 |
| drop-counters | Client 的 `phase_samples_unmatched`、Host 的 `slack_clamped`；新的 A 級行族與釘住測試 | L1；突變「不計數」「計錯分支」killed；權威 digest 不變 | 3～4 | D-12 |
| single-store-refactor | 照 Delta-1。暫時規則是聯集：600 序號內，或屬於最新 64 筆就保留，樣本只給最新 64 筆。每筆紀錄要有 eligible 旗標，才能逐位元模擬現在的裁切（審查 minor 1）。SubmitInput 端的重建與帳本關閉時刻要對齊 | L1；既有判定原樣通過（包括 `:1363`）；`match-ingress.jsonl` 位元組相同；73 個突變重新判定並登記 | 8～12 | 動手前你同意 Delta-1 |
| match-resync | 照 §14.3，先 Red 後 Green：晚到 > N_o 換 epoch（Resync）、不受冷卻限制、同 epoch 至多一次、觸發的命令不產生樣本。schema 3 列舉、`command_evidence.py`、log-contract 同步 | L1；突變（N_o 差一、雙重換 epoch、受冷卻限制、觸發命令仍產生樣本）killed；權威 digest 的 35 個情境不變 | 7～10 | D-3 |
| retention-600 | 拿掉「最新 64 筆」的條件；`:1363` 的判定反轉（§14.4 的 1～5、突變、另一位審查者核對）；文件與註解的 64 改成 600；C++ 守恆檢查加 S3 | §14.4 的流程與你的核准；L1；L2 的不退步紀錄（x1、ramp、gateway-250ms 子集） | 4～6（另加 L2 2～3） | D-6 |
| client-interrupted | `ConnectionLiveness.hpp`；第 3.1 節的狀態與恢復流程；HUD；暫時性錯誤；Hello 250 ms；T_max 改從 snapshot 起算；Gateway 的 per-client 時鐘視窗；速率註解重算 | L1（worker CTest＋Go）；突變（中斷時 CloseTransport、ResetActions、sentSequence 歸零；拿掉 T_int；暫時性錯誤仍丟例外；Liveness 吃時鐘回聲；恢復時不 SeedLead；Welcome 延長 T_max）killed；差分 oracle；37 個既有突變重新判定 | 10～14 | D-4、D-5、D-7～D-9 |
| identity-rebind | 第 3.3 節的重新綁定；`session_test.go`；契約 `:540`；A 級行 | Go 測試；突變（不檢查靜默、不檢查序號、不檢查 token、到期後仍允許）killed；`state_whitelist_test.go` 不變 | 6～9 | D-2 |
| resync-l2-l3 | relay 工具加中斷視窗（雙向、只下行、只上行、部分送達、換來源埠）；既有情境的位元組不變；判定在看結果前宣告並經你核准。L3：Windows 與 macOS 各拔網路線或關 Wi-Fi，2 s 與 7 s 各一次，並記錄 endpoint 有沒有改變 | 宣告的判定成立；失敗的跑次不被之後的成功覆蓋（§14.1）；你確認 L3 | 7～10（不含你在場 1～2） | D-13 |
| line-close | 記錄器分支的 HANDOFF、README、after-re-evaluate（substitution-single-store 指向本線、wake-reconnect 的處置）；v8 README 的 B6 交接；Fitness Check（§12）；Architecture Report（§15）；PR 本文用日語 | L1 全綠 | 1～2 | 合併時機 |

- **估時**：原本 after-re-evaluate 給 substitution-single-store 4～6 h、wake-reconnect 4～8 h。本線大很多，原因是：
  - 特性測試、計數、結構與行為分兩批；
  - 中斷狀態與 L2；
  - 身分接回；
  - 突變重新判定約 110 個（審查 M6：原估的「約 45 個」偏低）。
- **縮小的選項**（D-1）：D-2 選 I-1（不做 `identity-rebind`）時約 53～76 h。
- **時間框**：任一批超過估時 1.5 倍，或單一問題累計超過 2 小時，就停下回報。

## 7. L2 判定候選

都在看結果前宣告、經你核准；宣告前先做門檻來源的對抗式檢查。注入的時長避開 T_int、保險絲與 T_max 的邊界，各留餘裕（審查 M2）。

| 代號 | 判定（候選） | 適用的注入 | 來源 |
|---|---|---|---|
| J-R1 | 中斷 ≤ T_int／2：中斷狀態 0 次，Starvation 與 Resync 換 epoch 0 次 | 全部 | T_int；J-R6 |
| J-R2 | 中斷 2×T_int 與 T_max−2 s：Client Failure 0、Gateway expired 0、玩家留在對局；每次注入「中斷→恢復」恰好 1 次；原因為 Starvation 或 Resync 的換 epoch 恰好 1 次（LifeRespawn 不計） | 雙向、只上行 | K1、K2 |
| J-R2d | 只有下行：短於約 0.7 s 時換 epoch 0 次、相位重新擷取 1 次；超過約 0.7 s（冷卻期外）時 Starvation 換 epoch 恰好 1 次（第 13 節修訂） | 只下行 | network-architecture §4 |
| J-R3 | 中斷期間產生的序號沒有被 TrackPhase 採用，0 次容許 | 全部 | K2；`LocalPlayerPrediction.cpp:132-161` |
| J-R4 | 發布的 slack 全部 ≥ −333,333 µs（N_o，時間；第 13 節修訂）。因為 N_o 小於 1 s 的夾限，這條有鑑別力 | 全部，含部分送達 | network-architecture §4 |
| J-R5 | 中斷 T_max＋2 s：Client 在最後一份 snapshot 後 [T_max, T_max＋0.5] s 內 Failure；Gateway 在最後一個上行封包後同樣的區間內 Leave；Match 沒有殘留玩家 | 雙向 | `server.go:30`；K1、K3 |
| J-R6 | clean 對照組：中斷 0 次 | 無 | snapshot 間隔分布 |
| J-R7 | 所有跑次 S1、S2、J5a、S3 成立，runtimeFailed 為 0 | 全部 | `IngressStatistics.hpp:152-189` |
| J-R8 | 中斷期間換來源埠：session 接回，`session rebound` 恰好 1 次，Leave 0 次（I-2 時） | 換來源埠 | K3 |

- **只記錄、不判定的量**（D168：基準改變的量只記錄）：
  - `slack_clamped`、`phase_samples_unmatched`、Late 決定次數；
  - 與 STABLE_BASELINE 的比較；
  - 恢復時的時鐘視窗紀錄；
  - 品質視窗的失敗次數。
- **試行次數與重試規則**：D-13 由你決定。

---

## 8. 交給你的決策點

照建議核准時，第 6 節的分批就是執行順序。`gate-characterize`、`gate-contract` 兩個關卡會再停下來。

1. **D-1 核准本計畫、分批、檔位與範圍。**
   - 完整版約 59～85 h；縮小版（D-2 選 I-1）約 53～76 h。
   - **建議**：完整版。

2. **D-2 身分接回（P4）。**
   - (I-1) 維持 v1：只有同一個 endpoint 才能接回；換位址的中斷就是斷線，回 Lobby。需要你同意縮小 P4。
   - (I-2) 第 3.3 節的重新綁定：wire 不變，只用 session 軟狀態，風險限縮在中斷期間，有 A 級紀錄。
   - (I-3) 防竄改的證明（HMAC 等）：要改 wire，排到 v8 的 pv8。
   - **建議**：I-2，並把 I-3 記到 v8 的候選。

3. **D-3 Match 端：晚到過久就換 epoch（第 3.2 節），N_o 在 `gate-characterize` 決定。**
   - (M-B′) 照第 3.2 節：Starvation 加上「晚到 > N_o」，Resync 不受冷卻限制。
   - (M-A) 只把現有的 Starvation 升格為 Invariant。冷卻期內、部分送達、只有上行的大負值會留下，交給 B6。這偏離 P3，需要你修改原則。
   - **建議**：M-B′。N_o 候選 30 Tick，由晚到分布確認。

4. **D-4 T_int：0.5、0.75 或 1 s。**
   - **建議**：在 `gate-characterize` 看 snapshot 間隔分布再定；如果 1.3 s 的 Gateway 停頓在 clean 語料裡常見，取 1 s。

5. **D-5 T_max 的起算點：最後一份 snapshot，還是任何有效封包。**
   - **建議**：Playing 期間從最後一份 snapshot 起算，讓「中斷」一定有上限。

6. **D-6 C13 (b) 的行為改變宣告。**
   - 新增的樣本：600 序號內、已被最新 64 筆擠出去、但晚到不超過 N_o 的命令。在 M-B′ 下，這只剩很少的情況。
   - 消失的樣本：稀疏替代時，舊於 600 序號的晚到；以及晚到 > N_o 的命令（改成觸發換 epoch）。
   - `:1363` 的判定反轉有放寬的一面，照 §14.4 處理。
   - **建議**：兩面一起核准，§14.4 的材料在 retention-600 批交給你。

7. **D-7 哪些 socket 錯誤算暫時性。**
   - 範圍：connection_reset、refused、network_unreachable、host_unreachable、network_down。Windows 上 UDP 的 WSAECONNRESET 是〔推論〕，在 L3 確認。
   - **建議**：照列表。

8. **D-8 恢復時時鐘視窗的格式。**
   - (a) 只多一行 final；
   - (b) 行尾追加 `reason=resume`。
   - **建議**：(b)，之後的分析才分得出正常結束與恢復。

9. **D-9 中斷期間的操作**（審查 minor 7）。
   - (a) 照常產生並送出 Input 與動作：開槍可能因參考過舊而 Expired，被擊中照算。
   - (b) 中斷期間本地停止產生動作，移動照送。
   - **建議**：(a)，不改 ClientSimulation。UX 寫進契約：中斷期間的操作不保證生效。

10. **D-10 中斷造成的品質視窗失敗照算嗎。**
    - 照算時，30 s 內連續 3 次各約 1 s 的中斷，會以 UnstableInput 被逐出。
    - **建議**：照算，不為重連另開分支（已寫進 Replacement Criteria）。L2 只記錄。

11. **D-11 T_max 與 wake-reconnect。**
    - T_max：維持 5 s；10 s；或 ≥30 s（撐過實測 26.8 s 的睡眠）。
    - wake：
      - (w1) 只併入契約：短於 T_max 的睡眠就是中斷，其餘留在原批；
      - (w2) 加上斷線後自動重新加入；
      - (w3) 用很大的 T_max 撐過睡眠。
    - **建議**：T_max 維持 5 s，wake 選 (w1)。T_max 越長，斷線的玩家佔房間名額越久。

12. **D-12 日誌。**
    - 新的 A 級行族照 K5；`slack_clamped` 不放 ingress 行；S3 只加在 C++，不動凍結的 `ingress_evidence.py`。
    - **建議**：照 K5。

13. **D-13 L2 的判定、門檻、試行次數與重試規則**（第 7 節，包括 retention-600 的不退步紀錄）。依 §14.1，在看結果前核准。

14. **D-14 其他要你同意的項目。**
    - Delta-1～3 的七點說明；
    - §7 新增的 Design Invariant 與 Replacement Criteria；
    - 新元件 `ConnectionLiveness`；
    - wire 留在 pv7；
    - after-re-evaluate 的決策點 A：建議的 (a)「批內先寫特性測試」還沒被核准過（審查 minor 9）。本計畫的 `outage-characterize` 就是 (a)，請一起核准。

## 9. 風險

1. **前提可能不準**：`gate-characterize` 是關卡，避免為了量不到的問題排修正（v7.1 估到 400 h 的教訓）。
2. **夾限是整個房間的單點故障**：夾限一壞，Gateway adapter 回錯誤（`adapter.go:91-93`），接著 runtimeFailed，整個房間停住。所以順序是先加突變（outage-characterize）、再加計數（drop-counters）、最後才改行為。
3. **準時樣本被擠掉**：晚到樣本會贏得那次發布的最小值（`MatchRuntimeHost.cpp:133-138`）。在 M-B′ 下，晚到量上限是 N_o，影響變小。本線只記錄，屬於 B6 的輸入。
4. **Host 有送、Client 丟掉**：Client 的環只有 64 格。本線只加計數，不擴大環，因為那會改到 TrackPhase 的規則（B6 的範圍）。
5. **沒有相位追蹤的 golden**：權威 digest 不涵蓋 slack，B10 在 v8。守衛只到本線碰到的路徑；範圍以外改壞了仍然測不到。
6. **只有上行中斷時**，Client 照樣收到 snapshot，不顯示中斷；Match 換 epoch 之後，玩家會看到角色停住再起步。記為已知項目。
7. **重新綁定的劫持風險**：見 Delta-3。
8. **跨平台**：Windows 的 WSAECONNRESET、Wi-Fi 重連後的來源埠，都靠 L3 證實。macos-x64（CI 的 Rosetta）的偶發問題只記錄。
9. **與 quick-close 重疊**：中斷時按離開，同步的 /leave 會卡 2～3 秒（`ClientConnection.cpp:73-77`）。本線只記錄。
10. **驗收成本**：既有語料看不出差別，行為改變只能用構造的測試與注入的 L2 證明。這會寫進宣告。

## 10. 和其他計畫的關係

- **v8 B6**：
  - 本線不改 `Correct()`、`reacquiring_`、deadband、p90 視窗，也不改 `LocalPlayerPrediction.cpp:150-178` 的決定邏輯。
  - 只多一個 SeedLead 的呼叫點，並加上計數。
  - line-close 會在 v8 README 寫明：B6 的基準要在本線合併後重新取得。
- **after-re-evaluate 的 substitution-single-store**：整批移到本線。
- **wake-reconnect**：依 D-11。
- **記錄器分支**：不併入本線；HANDOFF 只記分線與決定。

## 11. 規劃過程與審查的處置

- **評分**：3 份計畫，滿分 70。
  - 風險優先版 58：先釘現況、分開結構與行為、Match 的做法交關卡。
  - 契約先行版 53。
  - 最小契約版 52。

  以風險優先版為底，移植其他兩版的優點：Starvation 升格為 Invariant、Gateway 的守衛測試、`ConnectionLiveness`、T_max 兩端同值、wake 的選項。
- **對抗式審查**：判定「必須修訂」。本版的處置如下：

| 審查項目 | 處置 |
|---|---|
| C1 身分接回被縮成同一 endpoint，並移出範圍 | P1～P5 列為固定輸入；F5、第 3.3 節、Delta-3、D-2（首要決策點） |
| C2 部分送達與只有上行的大負值沒有處理 | 第 1 節列為已核實的缺口；第 3.2 節「晚到過久就換 epoch」；J-R3、J-R4 寫明適用的注入 |
| M1 M-A 偏離 P3 | D-3 把 M-A 標為需要修改原則；寫明 pv7 的限制 |
| M2 J-R4 沒有鑑別力，J-R1 落在邊界 | J-R4 改用 N_o（小於夾限）；注入時長避開邊界 |
| M3 `slack_clamped` 會觸發 ingress 的 Replacement Criteria | 改用新的 A 級行族（K5） |
| M4 L2 要讀 B 級行 | L2 要讀的行一律 A 級，附釘住測試（K5） |
| M5 時鐘只在 Client 重新取樣 | Gateway 的 per-client 視窗也重開；Match 的視窗不受影響（第 3.3 節） |
| M6 突變數與估時偏低 | 改成約 110 個；估時改成 59～85 h，並列出縮小版 |
| M7 分支基底與文件位置，記錄沒有落實到每批 | 「現況」一節與 dev_log 每批更新；記錄器分支的文件只在分線、決定、收尾時更新 |
| M8 ConnectionLiveness 不足以單獨守住 | 只收 `steady_clock` 到達時刻；回聲突變；差分 oracle |
| minor 1～10 | 1：single-store-refactor。2：D-10。3：D-4。4：第 1、3.3 節。5：J-R2、J-R2d、J-R5。6：D-5。7：D-9。8：K4。9：D-14。10：第 3.1 節 |

- **ultracode 的原始輸出**（git 忽略的 scratchpad，只作出處）：`wf_3c8b9349-036` 的 journal。

## 13. resync-contract 的修訂（`gate-contract` 之前，2026-10-23）

契約條文寫在 [network-architecture](../../network-architecture.zh-Hant.md) §4「連線中斷與恢復」與 §7 的兩節 Design Invariants；條文和本計畫第 3～5 節不同時，以條文為準。xhigh 對抗式審查（1 次）判定「必須修訂」：Critical 2、Major 8、Minor 12，已全部處理。和本計畫不同的地方：

1. **恢復時的相位重新擷取**（審查 C1）：
   - epoch 相同、而且權威還沒追上 tip 時，不呼叫 SeedLead，只重設相位的擷取（樣本、命令年齡環、晚到計數、`settledAfter_`＝Client 的 tip）。
   - 原因：SeedLead 會清空未確認的命令，再從權威游標重新產生，等於用不同內容改寫還在途中的序號，違反 §7「相同命令識別不可修改」。
   - 第 3.1 節說的「只多一個呼叫點、不新增相位邏輯」撤回；`LocalPlayerPrediction` 會多一個「只重設擷取」的入口。
   - 恢復事件經由網路 worker 交給模擬端那一批資料上的一次性旗標，在同一批的相位樣本之前生效（審查 M2）。
2. **N_o 改用時間判斷**（審查 C2、M8）：
   - 「晚到過久」＝第一次到達時「到達時刻 − 替代時刻 > 20 Tick（333,333 µs）」。值不變（D173），判斷方式從序號差改成時間，所以 Match 停頓時樣本的下限仍然成立。
   - 晚到過久的第一次到達一律不產生樣本，計 `late_beyond_no`；每個 epoch 的第一次排程 Resync（下一個 Tick 邊界，和其他排定的換 epoch 合併，resets 只計一次）。
   - S3 改成 `late_first == slack_late + late_beyond_no`，`retention-600` 之後才是等式。
3. **Resync 的歸屬**（審查 M4）：Host（SlackTrack）量晚到時間，透過 PvpMatch 新增的公開介面要求換 epoch。這條 Host → PvpMatch 的依賴邊加進 Delta-2。保留範圍（600 個序號）因此成為遊戲規則的一部分。
4. **只有下行中斷**（審查 M1）：未確認的窗口滿 12 就停，所以超過約 0.7 s 時 Starvation 會換 epoch。J-R2d 已改。
5. **「一定換 epoch」改成可檢查的定義**（審查 M3）：冷卻期外的 Starvation，以及晚到過久的第一次到達。不換 epoch 的三種情況列為已知項目，由相位重新擷取處理。
6. **日誌**（審查 M6、M7、minor 5、minor 9）：
   - 鍵照批次順序排；
   - `phase_samples_unmatched` 改由模擬端寫新的 `phase summary` 行；
   - `late_beyond_no_commands` 加在 `resync statistics`；
   - clock sync 的 `reason=` 加在每一行（`window`／`final`／`resume`），恢復時不重設漂移回歸，`drift_ppm` 的意義不變；
   - schema 3 的 `reset_reason` 新增 `resync`。
7. **重新綁定的「靜默」**（審查 minor 2、3、4）：
   - 改用「原 endpoint 最後一個通過 token 驗證的資料包」的時刻，不論序號，避免以跳號製造靜默。這是通用層 session 的新欄位，加進 Delta-3。
   - T_int 由產品以參數傳給通用層。
   - Gateway 的時鐘視窗重開也用這個時刻判斷。
8. **第二輪審查（xhigh 複核，使用者要求）**：判定「必須修訂」，Critical 1、Major 2，已全部處理：
   - 「最後驗證時刻」改成 Hello 的 token 比對或 `Admit` 的 endpoint 比對都刷新。只看 token 的話，正常遊玩中每秒約有一半時間會被判成靜默，可以被改綁，時鐘視窗也會一直重開；
   - S3 在 `retention-600` 之前兩個方向都不保證（稀疏替代超過 600 序號時 0 對 1），只在之後檢查；
   - epoch 相同、權威 < tip 時，領先量留到 Backlog 清掉，列為 P2 的第二個已知例外，交給 B6；
   - 游標＝tip 時 SeedLead；只重設擷取時取消未 slew 完的修正，清除 `settling_`、`reacquiring_`；旗標的 mutex 區段與換代清除；和 LifeRespawn 合併時的計數；過渡批次的判斷範圍；`phase summary` 的寫出端；Delta-2、Delta-3 就地修正。
9. **第三輪審查（xhigh 複核，使用者要求）**：判定「可送核准」，只剩 Minor 7 項，送 `gate-contract` 之前全部處理：時鐘視窗重開只由通過驗證的資料包觸發（m1）；只有已綁定 endpoint 的 Hello 刷新最後驗證時刻（m2）；第一個 Welcome（m3）；`phase_path=resume_reset`（m4）；`phase summary` 只在有本機玩家的那一代寫（m5）；1 Hz Hello 的空窗寫進安全取捨，Hello 間隔在 `identity-rebind` 決定（m6）；有效 snapshot 的定義、Delta-2 的影響範圍（m7）。
10. **`single-store-refactor` 的實作（2026-10-23）**：權威的實體是新元件 `MatchSubstitutionRecords`（`substitutions_`），不是 SlackTrack 這個 struct 本身；SlackTrack 的 `substituted` 已刪除。§7 條文的 owner 名稱照實作改正（D175 核准），意圖不變。帳本的附欄另外包含看著解析的範圍與溢出的序號（分類用，不參與判定）。
11. **其他**：
   - 暫時性錯誤在連線與中斷狀態都適用。代價是 Gateway 程序死掉時，Client 要等 5 s 才回 Lobby。
   - T_int、T_max 兩端的值由共用 fixture 綁定。
   - 握手期的逾時寫法改正。
   - 特性測試升格為守衛之後，改判定照 §14.4。
   - 睡眠時鐘只寫 macOS 的實測。

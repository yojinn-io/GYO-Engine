# 合併前核准：網路 Module PR-1（`v8-arch-gyo2-net-module`）

- PR：#275（base `gyo2`）。
- 範圍：第 0～3 步、3c、Q8 (b)、設計審查的修正。
- 宣告：[net-module-declaration.zh-Hant.md](net-module-declaration.zh-Hant.md)（修訂 2＋第 17～20 節的決定）。
- 證據：主 checkout `build/evidence/gyo2-net-module/`。
- **要你核准的**：第 6 節的四件，與合併。合併要你明說「合併」。

---

## 1. 這個 PR 做了什麼（一句一步）

| 步 | 做了什麼 | dev_log |
|---|---|---|
| 0 | `core/os` 的非阻塞 UDP socket（IPv4），Windows 關掉 `SIO_UDP_CONNRESET`；os.md 照 Q5 改 | `2026_11_22_gyo2_net_module_step0` |
| 1a | 傳輸層：Link（UDP、記憶體）、封包標頭、ack、兩種通道、分片、丟棄計數 | `…_step1a` |
| 1b | 握手（無狀態的挑戰）、存活、斷線理由、`NetModule`、Peer 實體、不選角色就沒有 | `…_step1b` |
| 2 | 元件的複製（結構可靠、值不可靠、應用層確認、帳本、同號）、伺服器訊息綁 tick、`OnServer()`、World 世代 | `…_step2` |
| 3a | 輸入（實體上、兩端同名的入口、冗餘、hold-last、單次觸發）、重連、CCR-8；回滾的元件輸入紀錄、權威的代為登記 | `…_step3a` |
| 3b | 對時：時間 Module 的 `TimeSyncScale`、`JumpFixedSteps`、可登記的 `TickWriters`；網路的 `SyncClock` | `…_step3b` |
| 3c | 送件的效能（配置減少），G-12 | `…_step3c` |
| Q8 (b) | Resource 的複製；狀態機的 `RequestSilent` 與兩個套用函式 | `…_q8_resources` |
| 審查 | 設計審查與窄複查的修正（B1、B2、S1～S4、R1、R2、小事） | `…_review_fixes` |

## 2. 判定的結果（事前宣告；結果出來後沒有放寬）

| 判定 | 結果 | 數字（本機 macOS x86_64） |
|---|---|---|
| G-1 單機零成本 | 成立 | 沒有角色時，System、Resource、元件、訊息逐項和不選網路相同 |
| G-2 傳輸 | 成立 | Ordered Reliable 10 000／2 000 則恰好一次、照順序；件數兩式逐項相等（讀法見第 6 節 2） |
| G-3 握手 | 成立 | 五項各自只改一項，各回對應的理由；K1 不比指紋 |
| G-4 存活 | 成立 | 門檻前一刻／後一刻各看一次 |
| G-5 複製 | 成立 | 觀測 164／391 tick（門檻 10%），不一致 0；件數一致 |
| G-6 輸入 | 成立 | 逐 tick 比，等於 Client 送的或沿用上一格，兩者都不是的 0；件數一致 |
| G-7 UDP | 成立 | loopback 上握手、縮小版複製；L1 三平台 |
| G-8 大小 | 成立 | 兩次執行逐位元組相同；44 078 bytes／130 幀 |
| G-9 重連 | 成立 | 觀測 42、不一致 0；**舊連線的包因連線識別不符被丟 5**（宣告的那條路真的被觀測到） |
| G-12 送件的效能 | 成立 | 改後 3 次平均 1.12／1.12／1.09 ms ≤ 3.9 ms（改前約 5.49 ms） |
| G-10、G-11 | — | 第 4、5 步（PR-2） |

**CI**：L1 三平台，每一步都觸發。最後一次是 283120db 的 run 45065330263（結果補在這裡）。前一次 bb2201ad 的 run 45062054028 三平台都過。

## 3. 審查

- **設計審查**（Fable／high）：擋合併 2、應改 4、小事 8、應交使用者 5。
  - 原文：`design-review-pr1-fable.md`。
  - 處理：`review_fixes` 的 dev_log。U2～U5 由你在 2026-11-23 定了（宣告第 20 節）。
- **窄複查**：仍擋合併 1（B2 的殘留）、應改 1（epoch 的根）、小事 4。
  - 原文：`design-recheck-pr1-fable.md`。
  - 處理：283120db。
- **第二次確認**：283120db 的唯讀確認，結果補在這裡。
- **修法**：照 §14.3，先寫會失敗的測試（Red），修到 Green，測試留下。

## 4. 和宣告不同的地方（已在各 dev_log 揭露）

| 不同 | 狀態 |
|---|---|
| 只做 IPv4（D-1） | 你已定（U3） |
| socket 壞掉不回給 runner，記在 `NetService`＋World 訊息 `NetLinkFailed`（D-12） | 你已定（U2） |
| 一幀多步只送最後一步的值（C-RP4） | 你已定（U4）；第 4 步的宣告寫明 |
| K2 不保證收斂 | 你已定（U5 (a)），寫成限界 |
| 錯誤型別叫 `NetFailure`（命名規範 N25） | 照規範 |
| 握手多了 salt（遞增）、`Rejected` 種類、網路協議版本欄；`Accepted` 帶回 salt；請求帶 Client 的 epoch | 細化；審查確認成立 |
| 結構更新只在有變化時送；值仍每 tick 送 | 細化 |
| resync 與 epoch（Client 換 World 時整份重送） | 細化；B1、R2 修正後審查確認 |
| `ReplicateResource<R, V>` 多一個 `read`（`StateMachine<S>` 型別封閉，wire 上送 `S`） | 細化 |
| 帳本記確認過的 World 戳記，不記 tick | 細化 |
| 通道固定 0 號可靠、1 號不可靠；最多 64 種複製元件 | 細化 |
| 超過 `maxMessageBytes` 的結構更新每 tick 被拒、只計數 | 寫成限界＋RC-NT8 |

## 5. 突變（§14.5）

| 清單 | 新加 | 退役 | 結果 |
|---|---|---|---|
| os（`plans/os-subsystem/scripts/mutations.json`） | 7（`od-`） | 0 | 本機 6／6；`od-win-connreset-on` 只在 Windows，**CI 不跑突變，還沒被證明 killed** |
| net（新清單） | 74 | 2（實作期間退的，不在清單內） | 74／74 |
| rollback | 3 | 0 | 輸入相關 10／10 |
| authority | 3 | 0 | 輸入、Tick 相關 killed |
| world_time | 4 | 0 | 96／96 |
| state_machine | 8 | 0 | 全清單 70／70 時加的；之後新加的 `sm-adopt-*` 3／3 |

- 合計新加 99 個。這個 PR 改的 Design Invariants 條文約 20 條：net NT-1～10、os DI 3、WT-1／3／5／6、rollback R-6 與輸入列、authority 第 4 節限界／AU-E5／AU-E6、state machine DI 6。乘 5 約 100，99 沒有超過（README 第 8 點）。
- 每個都寫了 `guards`，以及「只有它捕到的誤差」。
- 退役的兩個（`nt-window-not-power-of-two`、`nt-link-failure-retried`）守的是誤用或不是條文，照「誤用不加守衛」退掉。

## 6. 要你核准的

1. **Design Invariants 的最終文字**（各頁標「實作，合併前核准定稿」的地方）：
   - [net.zh-Hant.md](net.zh-Hant.md)：新頁。NT-1～NT-10、RC-NT1～RC-NT8、wire 格式、限界。
   - [world-time.zh-Hant.md](world-time.zh-Hant.md)：WT-1、WT-3、WT-5、WT-6（`TimeSyncScale`、`JumpFixedSteps`、可登記的 `TickWriters`；WT-6 沒有多一項）。
   - [rollback.zh-Hant.md](rollback.zh-Hant.md)：`Input<T>` 收元件、R-6 的登記多一種。
   - [authority.zh-Hant.md](authority.zh-Hant.md)：第 4 節限界那一條（元件輸入的代為登記）、AU-E5（讀可登記的 `TickWriters`）、AU-E6。
   - state machine 的頁：DI 6（silent 與兩個套用函式）。
   - [os.md](../../os.md)：Datagram 一條、DI 3 加 `ws2_32`（判定變更你已在 2026-11-22 核准）。
2. **G-2 的件數讀法**：判定的文字是一個式子。實作分成 datagram 一式、訊息的「片」一式，各自逐項相等。原因是一個 datagram 帶多則訊息、可靠的又會重送，一式對不起來。兩式比一式嚴（審查判斷是合理的細化）。
3. **突變 99 個**（第 5 節）與 `od-win-connreset-on` 還沒在 Windows 上被證明 killed。
4. **合併**。

## 7. 留給 PR-2 的

- 第 4 步：K3（模擬又回滾的 Client）。`RollbackInbox`、確認點、回滾實作 `Entered`／`Exited`，要改 R-8 文字，開工前給你看。確認點要能接受缺 tick（U4）。K3 的 Resource 權威值經回滾的入口。
- 第 5 步：相關性（每 Peer 的可見性、元件範圍、優先度）、回滾的 `NotSent`。**頻寬預算（Q7）在第 5 步動工前定**，連同典型情境，量之前先定。
- 效能只在這台 macOS x86_64 量過，Windows Client、Linux Match 沒量。
- 時間的 Resource 每幀被寫、`Changed()` 恆為 true，網路不拿它判斷；只寫說明。

# 2026-11-23 網路 Module 第 3c 步：送件的效能（判定 G-12）（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（事前宣告 fa403bc0 之後）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md) 第 19 節（使用者 2026-11-23「照建議，加 3c，預算選 (b)」；判定 G-12）。
- **實作者**：Opus／high。

件數の突き合わせ：一致（G-12 的情境與 G-8 的情境，改前改後 Match 送出的 datagram 逐位元相同：摘要值相同；G-8 每 tick 的位元組數列相同）

## 1. 做了什麼

- **`ReplicateOut`（`engine/modules/net/src/Replication.cpp`）**：
  - **快照改成連續陣列**：原本是 `std::map<EntityId, mask>` 與 `std::map<(EntityId, wire), ChangeStamp>`。改成「實體依 id 排序的 vector（id、mask、第一格的位置）」＋「每個元件一格的 change stamp 的 vector」。一格的位置＝第一格＋mask 裡比這個 wire 小的位元數。每 tick 建一次（所有元件的 `visit` 收集後排序），所有 Peer 共用。
  - **值的編碼每 tick 只做一次**：某個實體某個元件的值，第一次有 Peer 要送時編碼進快照的共用緩衝，之後的 Peer 直接用這些位元組。`ReplicateOut` 執行中 World 不變，所以和每個 Peer 各編一次的位元組相同。
  - **緩衝重用**：結構、ops、值的 part、一個實體的暫存、送出的 body，在同一 tick 的 Peer 之間重用容量；`restructured` 從 `std::set` 改成排序的 vector（依 id 遞增放入）。
  - 協議、wire 上的位元組、判定都沒改。
- **量測程式 `gyo_net_budget`（`tests/common/net/NetBudget.cpp`）**：改成 G-12 的情境（見第 2 節），加上每 Peer 下行位元組、Match 送出 datagram 的摘要值，以及重放 G-8 情境的 `wire` 模式。不是 ctest、沒有 `cpu` 標籤。
- **突變**：`nt-empty-tick-not-sent` 的 find 跟著改（第 4 節）。

## 2. 量法（G-12）

- **情境**：Match 1、Client 8（K1），MemoryLink 不掉包；Match 400 個複製的實體（Position、Health），Position 每 tick 都改值（另有地圖的門 1 個、玩家 2 個）；64 Hz，一幀一個 tick（牆鐘每幀前進 15.625 ms）。
- **量什麼**：trace receiver 用單調時鐘計 Kernel 每個 System 的段，每幀加總 Match World 上網路 Module 的全部 System：`ReceiveNet`、`ReceiveInputs`、`CollectServerMessages:DoorOpened`、`ReplicateOut`、`SendNet`（再加上名字以 `ApplyInput:`、`DeliverServerMessages:` 開頭的；這個遊戲在 Match 沒有）。其他 System 的名字列在輸出的最後一行（不計）。
- **建置**：`core` preset（RelWithDebInfo）。
- **次數**：暖機 300 tick，量 2 000 tick，跑 3 次。
- **機器**：本機 macOS 26.7.1，Intel Core i9-9980HK 2.40 GHz（16 執行緒），64 GB。
- **重跑方法**：
  ```
  cmake --build build/target/_build/core --target gyo_net_budget
  build/target/_build/core/tests/common/net/gyo_net_budget 8 2000   # G-12：8 Peer、2 000 tick
  build/target/_build/core/tests/common/net/gyo_net_budget wire     # G-8 的情境：每 tick 位元組與摘要值
  ```
- **摘要值**：Client 的 `seed` 在量測程式裡固定（G-12 的 8 個 Client 1000～1007；`wire` 模式 7），否則 Client 的 salt 每次不同、摘要值不會重複。位元組數不受影響（G-8 的數列和測試印的一樣）。

## 3. 結果

**G-12：成立。** 3 次的平均都 ≤ 3.9 ms／tick。

| | 第 1 次 | 第 2 次 | 第 3 次 |
|---|---|---|---|
| 改前 平均 | 5 504.5 µs | 5 482.6 µs | 5 478.8 µs |
| 改前 p50／p99 | 5 581.4／5 967.7 | 5 581.8／6 393.9 | 5 584.2／6 126.3 |
| **改後 平均** | **1 122.1 µs** | **1 117.9 µs** | **1 092.4 µs** |
| 改後 p50／p99 | 1 117.3／1 449.0 | 1 118.8／1 422.5 | 1 112.5／1 423.6 |

（第 1、2、3 次是各自的檔案順序；p50、p99 記錄、不判定。最大值改前 16.9～49.6 ms、改後 12.3～18.2 ms，是偶發的長幀，兩邊都有。）

**分項**（每 tick 平均，3 次的範圍）：

| System | 改前 | 改後 |
|---|---|---|
| `ReplicateOut` | 5 311～5 338 µs | 937～959 µs |
| `SendNet` | 147～153 µs | 138～146 µs |
| `ReceiveNet` | 14.1～14.8 µs | 13.1～13.8 µs |
| `ReceiveInputs` | 2.6～2.7 µs | 2.6 µs |
| `CollectServerMessages:DoorOpened` | 1.0～1.1 µs | 0.9～1.0 µs |

- Client 一個（不在判定內，記錄）：改前改後都約 0.23 ms／tick，大多是 `ApplyReplication`（約 0.22 ms）。
- 改後的取樣（macOS `sample`）：`ReplicateOut` 本身的時間最多，其次是配置與釋放（每個 part 的實體清單搬進 `pending`、`Connection::Send` 的佇列）、Kernel 的 `Read<T>`。沒有再往下改（門檻已過，範圍是 G-12）。
- **和 3b 的數字不一致**：3b 的 dev_log 記 4 個 Peer 時 Match 平均 8.19 ms（`ReplicateOut` 8.03 ms）。這次用改前的執行檔量 4 個 Peer 一次：平均 2.70 ms（`ReplicateOut` 2.62 ms，`g12.before.4peers.txt`），約是 8 個 Peer（5.49 ms）的一半。3b 的數字偏高，可能是量的時候機器上有別的工作（同一時段在跑 ctest 或突變），沒有查證。G-12 的比較基準是這次的改前 3 次。

**wire 上的位元組不變**（G-8 證明）：
- G-8 的情境（1 個 Client、忙碌的遊戲到 tick 120、130 幀）：每 tick 位元組數列改前改後相同（合計 43 950，每 tick 最大 625），`gyo_net_tests` 的 G-8 印出的數列也和改前相同；Match 送出的 datagram 摘要值改前改後都是 `ab83028c36ff4f78`。
- G-12 的情境：Match 送給 8 個 Client 的所有 datagram，摘要值改前 3 次、改後 3 次都是 `e8a90f14e6e0ca2b`。

**判定 G-2～G-9**：`gyo_net_tests` 全過；網路測試印出的數字（G-5、G-6、G-9 等，RTT 的那行除外）和 3b 的證據逐行相同。

### 本機的影響（宣告第 19 節）

- 每次約 50 秒、CPU 一核，沒有 GPU；改前改後共 6 次，另有試跑與取樣幾次。

## 4. 突變

- **stale 的 find**：`nt-empty-tick-not-sent` 的 find 是 `parts.empty()`，改成 `partCountSoFar == 0`（parts 的容量在 Peer 之間重用，已用的數量另記）。replace 照舊拿掉這個條件，守的性質（C-RP4：每 tick、每 Peer 都送一則值）不變。
- **跑的範圍**（D184：find 在這次改動的範圍、或同一檔案）：`Replication.cpp` 的 13 個（`nt-value-before-structure`、`nt-ledger-at-send`、`nt-empty-tick-not-sent`、`nt-ledger-not-advanced`、`nt-generation-unchecked`、`nt-removed-not-encoded`、`nt-stale-value-applied`、`nt-resync-ignored`、`nt-onserver-everywhere`、`nt-clock-no-jump`、`nt-clock-speed-reversed`、`nt-clock-speed-kept`、`nt-clock-unsmoothed`）：13／13 killed（`mutations-replication.log`）。
- **沒加新的**：這次沒有新的決定（Design Invariants 的條文、判定都沒變），共用編碼與格的位置是實作的方式（`tests/common/mutation/README.md` 的 3、5）。探索性地試了兩個（不進清單）：格的位置多算自己的位元、格的位置不看 wire。兩個都被抓到（`mutations-explore.log`、`mutations-explore-detail.txt`）：不看 wire 的那個 G-5 (1)(2)(4)、無變化的 tick、World 換掉、CCR-8 的狀態比對都失敗；多算自己位元的那個讀到格外、測試卡住（執行器以逾時算失敗）。只是確認現有測試的偵測力，不是清單的 killed。

## 5. 頻寬的數字（給 Q7 用；不判定）

每 Peer 每 tick 的下行（Match → Client），算的是 datagram 的位元組（UDP 的 payload，不含 IP／UDP 標頭）；kbps＝位元組 × 8 × 64 ÷ 1000。

| 情境 | 平均 | 最大 | 64 Hz 換算 |
|---|---|---|---|
| G-12（8 Peer、400 個實體每 tick 都動） | 9 742.6 bytes | 9 791 bytes | 平均 4 988 kbps、最大 5 013 kbps |
| G-8（1 Peer、範例遊戲的忙碌情境，130 tick，step2） | 338.1 bytes | 625 bytes | 平均 173 kbps、最大 320 kbps |
| G-8 規則停止之後（一則空的值、確認、keepalive） | 55 bytes | — | 28 kbps |

- G-12 每 tick 每 Peer 9 個 datagram；加上 IPv4＋UDP 標頭（每個 28 bytes）約 9 995 bytes，平均約 5 117 kbps。
- G-12 的一個實體約 23 bytes：實體 id 8＋元件數 1＋wire 2＋長度 4＋Position 的值 8（兩個 int32）。值本身以外的部分是 15 bytes。這是現在的 wire 格式的實際數字，不是設計目標。
- 兩個情境都是「沒有掉包、沒有量化、沒有差分」的數字。

## 6. 範圍的限界

- **只在這台 macOS x86_64 量**。Windows Client、Linux Match 沒量；擴到它們之前另量（AGENTS §2，宣告第 19 節）。
- G-12 的情境只有值的更新（沒有生成、刪除），結構更新的費用不在這次的數字裡。

## 7. 驗證

- 本機（同上的機器，`core` preset）：
  - 全部的 ctest 181 個全過；
  - `gyo_net_tests` 全過（G-2～G-9 照舊）。
- **證據**（主 checkout `build/evidence/gyo2-net-module/step3c/`，git 不追蹤）：
  - `g12.before.{1,2,3}.txt`、`g12.after.{1,2,3}.txt`：G-12 的輸出（平均、p50、p99、最大、分項、頻寬、摘要值）；
  - `wire.before.txt`、`wire.after.txt`：G-8 情境的數列與摘要值；`g8.after.txt`：改後 `gyo_net_tests` 的 G-8 輸出；
  - `net_messages.after.txt`：改後網路測試印出的數字；
  - `gyo_net_budget.before`、`gyo_net_budget.after`：量測用的執行檔；`NetBudget.cpp.used`；`before.commit`；`machine.txt`；
  - `sample_before.txt`、`sample_after.txt`：取樣；
  - `g12.before.4peers.txt`：改前 4 個 Peer（和 3b 對照）；
  - `mutations-replication.log`、`mutations-explore.log`、`mutations-explore-detail.txt`、`explore_mutations.json`；
  - `ctest-all.log`。

## 之後要接的

- 第 4 步：K3（`RollbackInbox`、確認點）。
- 規模點名之後照 AGENTS §2 重訂預算（宣告第 19 節）。
- Client 的 `ApplyReplication`（8 Peer 的情境每 tick 約 0.22 ms）沒有預算，這次沒動。

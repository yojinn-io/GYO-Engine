# v8-product-hitbox-rewind 事前宣告（草稿，待使用者核准）

更新：2026-11-09。Owner：`object_fps_pvp`。rewind-line 的最後一批（[hitbox-plan](hitbox-plan.md) §8；契約 [protocol-v9](../../protocol-v9.zh-Hant.md) §5、§7）。
**狀態：草稿。** 判定與閾值在看到任何結果之前寫下，核准後不放寬（AGENTS §14.1）。

## 0. 為什麼（Feature → Contract → Architecture）

- **Feature**：射手照自己畫面上看到的時刻 τ 判定命中（lag compensation），不再一律用裁決 Tick J 的姿勢。
- **Contract**：protocol-v9 §5 已定，不加 wire 欄位：τ＝clamp(view_tick_q16/65536, J−R, J)，0 或無效時 τ＝J；**R＝7 Tick**（`v8-product-hitbox-rewind-r-7`，使用者 2026-11-09 採用 11/9 場次的探索值）；逐目標再夾到該目標目前區段的起點；跨生命、或 τ 活著到 J 已死（同一條命）判 Miss；射手不回溯。
- **Architecture**：Match 內部新增姿勢歷史（實體欄位）與 `ShotRewind` 單一入口；`view_tick_q16` 從「只記錄」變成有界的判定輸入（Design Invariants 的變更已在 pv9 核准意圖，守衛在本批第二次核准：`v8-product-hitbox-view-invariant` (a)）。

## 1. 範圍

- **姿勢歷史**：每個實體一個欄位（entity-model 的欄位規則：依 EntityId 升冪、整列重建、沒有舊列），存最近 R＋1＝8 個 Tick 的判定輸入：`PlayerState`（位置、yaw、pitch、crouched、grounded、生命代、`life_state`、`movement_epoch`）、`CombatState` 的動作起點、該 Tick 的 `PlayerAnimation`。只在 Match；不上 wire、不進 snapshot。
- **`ShotRewind`**（單一入口）：輸入 J、`view_tick_q16`、R、射手與歷史；輸出每個目標在 τ 的形狀（或排除）。只有它讀 `view_tick_q16` 做判定。
  - τ 是小數 Tick：在相鄰兩個歷史 Tick 之間用 protocol-v9 §4.4 的內插重建姿勢（和 Client 呈現用同一個函式）。
  - 逐目標：τ 早於該目標目前區段（`movement_epoch`、生命代、`life_state`）的起點時，夾到起點；τ 落在前一條命，或 τ 活著而到 J 已死（同一條命）→ 這個目標不參與這一發（射線穿過它）。
- **判定**：`PvpMatch` 的射擊判定改用 `ShotRewind` 的形狀；射手的位置照舊（解析完 S 之後的狀態）。`view_tick_q16` 為 0 或無效（`ValidViewTick`）時 τ＝J，形狀和現在的 `TickShapes` 逐位元相同。
- **守衛第二段**：`test_no_decision_reads_the_view_tick`（`tests/object_fps_pvp/test_diagnostic_isolation.py`）改成全掃產品原始碼＋白名單（`ShotRewind.*`、`MatchShotTrace.hpp`、wire 與記錄的位置）。這是 Design Invariants 的守衛，依 §14.4 用 1～5 說明並取得使用者核准。
- **影子紀錄**：`match-shots.jsonl` 加 τ（`rewind_tick_q16`）與「被排除的目標數」，version 3→4（產品內的 Data Contract；`lag_l2_summary.py` 只讀 version 3 的，照它的規則一起升）。

## 2. 不做

- R 的再量測（之後的場次；`s1_end` 自動化在下一次場次的 kit 做）。
- 回溯世界幾何（牆不動，不回溯）。
- 漏收、snapshot 合併時 Client 與 Match 的姿勢落差（protocol-v9 §5 接受的限制）。

## 3. 待決（使用者）

| 代號 | 問題 | (a) | (b) | 推薦 |
|---|---|---|---|---|
| `-rewind-shape-cache` | 同一 Tick 多發子彈、各自的 τ 不同時，形狀怎麼放 | 每一發各自放（不快取） | 依 (目標, τ) 快取，同一 Tick 內共用 | (a)：最簡單、沒有快取失效的問題；成本由 W8 的預算擋，超過再做 (b) |
| `-rewind-verifier` | §14.4 守衛變更的驗證者 | Fable／xhigh（比主會話強，要你同意） | Opus／high（和實作者同模型） | (a)：守衛是 Design Invariants，領航員規則要求驗證者與提案者不同、不弱 |

## 4. 判定（先宣告）

| 代號 | 判定 | 通過條件 |
|---|---|---|
| W1 τ＝J 不變 | `view_tick_q16` 為 0 或無效的所有既有測試、AuthorityDigest 的 golden 不改；隨機交叉（200 組 × 200 發）：τ＝J 時 `ShotRewind` 的形狀與 `TickShapes` 逐位元相同 | 全部通過；比較數＝產生數 |
| W2 回溯生效 | 合成場景：目標橫移，射手瞄 τ 時的位置 → 命中；瞄 J 時的位置且 τ＜J → 不中；τ＝J 時兩者相反 | 四個 case 都成立 |
| W3 夾取 | view 早於 J−7 → τ＝J−7；剛好 J−7、J−7＋1/65536、晚於 J → τ＝J | 邊界 case 全部成立 |
| W4 區段與生命 | τ 早於目標的重生、新 epoch → 夾到區段起點；τ 在前一條命 → 不參與；τ 活著到 J 已死 → 不參與（射線打到後面的玩家或牆） | 全部成立 |
| W5 內插 | 小數 τ 的姿勢＝§4.4 內插（和 Client 呈現用同一函式，同輸入逐位元相同） | 成立 |
| W6 守衛第二段 | 全掃＋白名單；舊守衛抓到的寫法新守衛也抓到；新守衛的突變 | §14.4 1～5、驗證者、使用者核准 |
| W7 影子紀錄 version 4 | τ 與排除數照實寫出；`lag_l2_summary.py` 的測試照新版本 | 成立 |
| W8 預算（AGENTS §2） | Intel Mac、RelWithDebInfo、biome、4 人：每發的回溯＋判定 p99 ≤ 0.2 ms；歷史的記憶體 ≤ 64 KB（MaxPlayers 4 × 8 Tick） | 兩者都成立 |
| W9 突變 | τ 不夾、夾錯邊界、不夾區段、跨生命仍命中、射手也回溯、τ＝J 時走不同路徑、守衛白名單過寬 | 全部 killed |
| W10 L1 | 三平台 | 通過 |

- 件數的突き合わせ：W1 記錄產生數＝比較數；W7 的影子紀錄 `trace_end` 計數照舊對帳。
- 實機的效果（射手體感、被打的一方「躲進牆後仍中彈」的程度）是探索，留給下一次場次（field-session），不在本批判定。

## 5. 分批、模型與檔位

```text
v8-product-hitbox-rewind
├── rw-history   上段｜Opus 5.5／high｜姿勢歷史欄位、W8 的記憶體
├── rw-judge     上段｜Opus 5.5／high（τ、夾取局部 xhigh）｜ShotRewind、判定接上、W1～W5、W7、W8
└── rw-guard     上段｜Opus 5.5／high｜守衛第二段（§14.4）、W6、W9、W10
```

- 上段理由：Match 判定、Design Invariants 的守衛、產品內 Data Contract 的版本。
- 實作者：主會話 Opus 5.5。審查者：Opus 5.5／high（general-purpose 子 agent，唯讀）。驗證者：見 `-rewind-verifier`。
- 估時：12～18 h（rewind-line 第 1 版估 23～35 h，lag-shadow 與 lag-l2 已用掉一部分）。超過 27 h 停下回報。
- 前提：PR #173（R＝7 的記錄）合併後開分支。

## 6. 範圍的限界

- R＝7 是 11/9 場次的探索值（區間事後定，LAN、3 人）；網際網路的延遲沒有量。
- 一度記錄「判定不成立」的 lag-l2（S1、R2）不影響本批的判定，但 R 的來源是探索值這件事要寫進 dev_log 與 PR。

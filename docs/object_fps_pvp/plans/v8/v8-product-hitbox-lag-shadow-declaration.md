# v8-product-hitbox-lag-shadow 的事前宣告

更新：2026-11-05。Owner：`object_fps_pvp`。**狀態：已核准**（`v8-product-hitbox-lag-shadow-declaration-approve`，2026-11-05，使用者：「Approve as recommended」；`-a-field` (a)）。
估時 1～2 h（超過 3 h 停下回報）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | 主會話 Opus 5.5／medium | 只改一個產品內紀錄檔的欄位與版本 |
| 審查者 | Opus 5.5／high（子 agent，和實作者同一個模型） | 改動小，沒有對抗式檢查；沒有其他模型可用時照領航員規則報告 |
| §14.4 檢證者 | 不需要（見「Test 判定的變更」：沒有緩める面） | — |

上段／下段：**上段**。它改的是產品內的 Data Contract（`match-shots.jsonl` 的格式與版本），屬於領航員規則的「資料格式」。不碰 wire、不碰權威結果、不碰 Design Invariants。

## rewind-line 的位置

rewind-line 的三批（[hitbox-plan](hitbox-plan.md) §8）照領航員規則補上上段／下段：

| 批次 | 上段／下段 | 實作者 | 審查者 | 估時（重估） |
|---|---|---|---|---|
| `v8-product-hitbox-lag-shadow`（本批） | 上段（資料格式） | 主會話 Opus 5.5／medium | Opus 5.5／high | 1～2 h |
| `v8-product-hitbox-lag-l2` | 上段（R 決定回溯的權威範圍） | 主會話 Opus 5.5／medium；收資料的場次要你的機器 | Opus 5.5／high | 2～3 h＋一次 L2 場次 |
| `v8-product-hitbox-rewind` | 上段（Design Invariants 第 8 列、守衛第二段核准、權威結果） | 主會話 Opus 5.5／high（τ、夾取、跨生命局部 xhigh，用 `xhigh-reviewer`） | `xhigh-reviewer` Opus 5.5／xhigh；§14.4 檢證者 Opus 5.5 | 20～30 h |

舊計畫寫的 lag-l2 由 Sonnet 5.5 執行，照現在的規則不適用（上段不用小模型），改成 Opus 5.5。

## 這批做什麼

pv7 §7 的文字把 `view_lag` 定義成「裁決 Tick J 減掉射手看到的時刻」，但程式（`MatchShotTrace.hpp:230`）用的是交給 Match 的 Tick A。兩者在射擊被冷卻擋住時才不同（J＝D＋冷卻）。使用者已決定以 J 為準（`v8-product-hitbox-view-lag-base`；protocol-v9 第 121 行），因為回溯的界限 τ 是在 J 判定時用的。

1. `MatchShotTraceVersion` 2 → 3。
2. `view_lag_q16` 改成 `J×65536 − view_tick_q16`。
3. 新增 `view_lag_a_q16`＝`A×65536 − view_tick_q16`（原本的值，供比較；見選項 `-a-field`）。兩者都只在 view tick 有效且非 0 時有值，否則是 `null`。
4. 文件：`log-contract.zh-Hant.md` 的 `match-shots.jsonl` 那一列改成 version 3；protocol-v9 第 121 行標上已實作；`MatchShotTrace.hpp` 開頭的註解改成 version 3。

不在這批：讀這個檔的分析器（`lag-l2` 做）；回溯本身（`rewind` 做）。

## 開工前的事實

1. 寫出端只有 `MatchShotTrace.hpp`（`MatchShotRecordLine`、`MatchShotTraceEndLine`）。
2. 讀者：
   - `build/acceptance/object_fps_pvp/lan15_judge.py` 只讀 `kind`、`player_id`、`rejection` 和 `trace_end` 的 `dropped`，不檢查版本，也不讀 `view_lag`；它的測試 `test_lan15_judge.py` 用手寫的 version 2 資料。
   - `run_action_short.py` 只負責讓 Match 寫出這個檔。
   - log-contract 寫的讀者「各 L2 的 `shadow_*.py`」在 repo 裡找不到（v7.1 的 L2 腳本沒有進 repo，或已經改名）。本批把那一欄照實改寫，不追查。
3. 現有測試：
   - `CombatTests.cpp:1100-1112` 逐字比對一行。
   - `CombatHostTests.cpp:390-407` 比對 `view_lag` 等於 A 的算法。那個情境沒有冷卻，A＝J。
   - 另一個測試（`:408` 起）有被冷卻擋住的射擊，A≠J，但 view tick 是 0，`view_lag` 是 `null`。
   所以**現在沒有任何測試能分辨 A 和 J**。

## 判定（結果出來前宣告）

- **L1（行為）**：新增測試「被冷卻擋住、view tick 有效的射擊」：`view_lag_q16` 等於 `J×65536 − view`，`view_lag_a_q16` 等於 `A×65536 − view`，且兩者差 `cooldown_hold_ticks×65536`（此情境為 2×65536）。
- **L2（格式）**：`CombatTests.cpp` 的逐字比對改成 version 3 的一行（含 `view_lag_a_q16`）；`trace_end` 也是 version 3；`null` 的情況兩個欄位都是 `null`。
- **L3（本機）**：PvP `cpu|shader` 全部通過（目前 50 個，新增測試後的數字照實記錄），失敗 0。
- **L4（突變）**：照 D184 只跑相關的。新增 3 個，必須全部被抓到：
  - `lag-shadow-view-lag-from-a`：`view_lag_q16` 改回用 A。
  - `lag-shadow-version-2`：版本留在 2。
  - `lag-shadow-a-field-from-j`：`view_lag_a_q16` 用 J。
  現有突變中，`find` 落在 `MatchShotTrace.hpp` 的也一起重跑，必須維持 killed。
- **L5（CI）**：PR 的 L1 三平台通過。
- 不重試：任何一項不成立就停下回報（§14.1）。

## Test 判定的變更（§14.4，事後報告）

1. `CombatTests.cpp` 的逐字期望：守的性質是「一行的格式固定」。改成新版本的一行，仍然逐字比對，沒有緩める面。
2. `CombatHostTests.cpp:390-407` 的 `view_lag` 期望從 A 改成 J：那個情境裡 A＝J，舊判定失敗的輸入新判定一樣失敗，是同等。真正分辨 A 和 J 的是 L1 的新測試，以 L4 的第一個突變證明它的檢出力。

## 證據

主 checkout 的 `build/evidence/v8-hitbox-lag-shadow/`（Intel Mac）：ctest 輸出、突變工具的報告。

## 選項（附建議）

| 代號 | 問題 | 選項 | 建議 |
|---|---|---|---|
| `-a-field` | 要不要保留 A 算的值 | (a) 保留為 `view_lag_a_q16`；(b) 不保留，只有 J | (a)：計畫寫了要「保留 A 的值供比較」；lag-l2 可以看冷卻讓兩者差多少，回溯做完後再決定要不要刪 |

## 修訂紀錄

1. 2026-11-05 草稿。
2. 2026-11-05 核准，`-a-field` (a)。

# PvP v8：v8-product-hitbox-lag-shadow（影子紀錄 v3：view_lag 以 J 計算）

日期：2026-11-05。分支 `claude/pvp-v8-rewind-line`。主會話 Opus 5.5／medium；審查者子 agent Opus 5.5／high（general-purpose，唯讀；和實作者同一個模型，沒有用別的模型）。估時 1～2 h，實際約 1 h。

**狀態：L1～L5 全部通過。** PR #154 已合併（2026-11-05，merge commit `318b4e66`）。 [事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-lag-shadow-declaration.md)已核准（`-a-field` (a)）。

## 做了什麼

- `MatchShotTrace.hpp`：
  - `MatchShotTraceVersion` 2 → 3；
  - `view_lag_q16` 改成 `J×65536 − view_tick_q16`（J＝裁決的 Tick，和 `resolved_tick` 相同）；
  - 新增 `view_lag_a_q16`＝`A×65536 − view_tick_q16`（版本 2 的值）。view tick 無效或為 0 時兩者都是 `null`。
- 測試：
  - `CombatHostTests.cpp` 新增「被冷卻擋住、view tick 有效的射擊」：兩個欄位各自的算法，以及兩者差 `cooldown_hold_ticks×65536`（2 Tick）；
  - 沒有冷卻的舊情境（A＝J）期望改成 J；無效 view tick 的情境多檢查 `view_lag_a_q16` 也是空的；
  - `CombatTests.cpp` 的逐字比對改成 version 3 的一行。
- 突變：新增 3 個（`lag-shadow-view-lag-from-a`、`lag-shadow-version-2`、`lag-shadow-a-field-from-j`）。
- 文件：`log-contract.zh-Hant.md` 的 `match-shots.jsonl` 列改成 version 3，讀者照實改寫；protocol-v9 第 121 行標上已實作。

## 判定

| 判定 | 結果 |
|---|---|
| L1 行為 | 通過：新測試與相關的 5 個 case、115 個斷言全過 |
| L2 格式 | 通過：逐字比對與 `trace_end` 都是 version 3，`null` 情況兩欄都是 `null` |
| L3 本機 | PvP `cpu\|shader` 50／50 通過（測試數沒變：新 case 在既有的 `object_fps_pvp.cpu` 執行檔內）；審查後改了註解與文件，重建後再跑一次也是 50／50 |
| L4 突變 | 10／10 killed：新的 3 個，加上 `find` 在 `MatchShotTrace.hpp` 的既有 7 個（D184） |
| L5 CI | 通過：PR #154 head `a354b861` 的 L1（run `42013454125`）linux-x64、macos-arm64、windows-x64 三平台 |

## 經過

- 開工前查到：現有測試都分不出 A 和 J。沒有冷卻的情境 A＝J；有冷卻的情境 view tick 是 0，`view_lag` 是 `null`。新測試補上了這個缺口，`lag-shadow-view-lag-from-a` 證明它抓得到。
- log-contract 舊記載的讀者「各 L2 的 `shadow_*.py`」在 repo 裡找不到，只照實改寫，不追查。
- 第一次加突變時用 `json.dumps` 重寫整個 `mutations.json`，縮排變了（約 9000 行差異），立刻還原，改成只在檔尾追加。

## Test 判定的變更（§14.4，事後報告）

1. 逐字比對：守的是「一行的格式固定」。改成新版本的一行，仍然逐字比對，沒有放寬的地方。
2. `view_lag` 的期望從 A 改成 J：那個情境裡 A＝J，舊判定會失敗的輸入，新判定一樣會失敗，兩者等價。分辨 A 和 J 的是新測試，檢出力由突變 `lag-shadow-view-lag-from-a` 證明。
3. 無效 view tick 的情境多一個檢查：只收緊。

## 證據

主 checkout 的 `build/evidence/v8-hitbox-lag-shadow/`（Intel Mac）：`build_1.txt`、`ctest_pvp_cpu_shader_1.txt`、`l1_l2_cases.txt`、`mutations.txt`、`mutations.json`、`load_*.txt`。

## 審查

Opus 5.5／high（和實作者同一個模型）。沒有 blocker；程式、新測試（確認 A≠J 是被斷言的，不是假設的）、§14.4（沒有放寬）、突變的 expect、讀寫端都確認過。採納的指正：

- should：README 的 lag-shadow 還寫「草稿，等核准」，lag-l2 還寫 Sonnet 5.5（README 與 hitbox-plan）→ 改成已完成與 Opus 5.5。
- should：dev_log 與 README 的開發紀錄列（審查者看的是 diff，未追蹤的 dev_log 不在內）→ 同一變更提交。
- nit：protocol-v9 第 121 行原句「`MatchShotTrace.hpp:230` 現在用 A」與新加的「已實作」矛盾 → 改成「版本 2 的程式用 A」。
- nit：protocol-v7 §6 的實作紀錄仍說 view_lag 以 A 計算 → 照檔內既有的「pv9 起改寫」寫法加註。
- nit：`MatchShotTrace.hpp` 開頭註解行過長 → 重新折行（只動改過的那幾行）。

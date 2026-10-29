# PvP v7.1：連線品質窗口失敗時寫出原因（quality-window-log）

日期：2026-10-29。分支 `claude/pvp-quality-window-log-831143`（從 master `62fd7ebf`）。Owner：`object_fps_pvp`。

**狀態：本機驗證完成，PR 待開。判定門檻與 UI 不變。**

## 為什麼

- lan15 第 1 次場次（2026-10-29），KLYU（player 2）在 11:06:31 收到 `connection_quality_failures`＝1，畫面顯示「CONNECTION POOR - improve within 20s or you will be removed」，10 秒後恢復，沒有被逐出。
- 從 log 推算：替代約 35‰（< 50‰）、重設只有 `life_respawn`（不算進品質判定），三個失敗條件都對不上。Match 判定失敗時不寫 log，成因判定不了（[lan15-declaration](../object_fps_pvp/plans/v7.1/lan15-declaration.md) 最後一節）。
- 照「量不到成因時先建檢測再評估」：先補這一行，之後才評估要不要改判定或 UI。
- 推論（沒有驗證）：35‰ 是由 ingress 統計的 `late_first_commands`（被替代、之後才到的序號）推的。被替代、一直沒到的序號不在裡面，且 10 秒的統計視窗和品質窗口的邊界不對齊，所以真正的替代比例可能高於 35‰。新的一行會直接寫出分母與分子，下次能確認。

## 做了什麼

- `MatchRuntimeHost`：`JudgeConnectionQuality` 每個判定過的窗口失敗時（`failed` 為真），留一筆 `QualityWindowFailure`。內容：player、關閉窗口的權威 Tick、連續失敗數（含這一個）、參考年齡中位數、替代千分比、重設數、三個條件各自是否成立、替代的分母（resolved）與分子（substituted）、參考年齡的樣本數。
  - 判定式本身不變：原本寫在一行的 `permille > …` 拆成具名的 `substitutedTooMany`，`failed` 的值相同。
  - `TakeQualityWindowFailures()` 取出（只作診斷）；`ClearState`（重設）時清掉，和 `evictions_` 相同。
- `IpcHost`：每次處理 host 的結果時，先取出失敗的窗口寫成 log，再處理 eviction。所以逐出時是先寫 `quality window failed`，再寫 `evicted player=`。
- 行的格式（`QualityWindowFailedLine`，`IpcHost.hpp`）：

  ```text
  [ObjectFPS/PvP Match] quality window failed version=1 player=5 consecutive_failures=2 reference_age_ms=171 substituted_permille=35 movement_resets=4 reference_age_failed=1 substituted_failed=0 movement_resets_failed=1 tick=1201 resolved_commands=600 substituted_commands=21 reference_age_samples=598
  ```

  - `movement_resets` 只計 Starvation、Backlog、Resync 三種 epoch 重設（`MovementQuality::resets`，`PvpMatch.cpp` 的 `ResetMovementEpoch`），不含重生與 exhaustion。
  - 三個 `*_failed` 是判定時的結果，不是讀者用門檻重算的。
  - `reference_age_samples`＝0 時 `reference_age_ms` 寫 0（沒有樣本）。

## 日誌契約：A 級

照 [log-contract](../object_fps_pvp/log-contract.zh-Hant.md) 第 1 節，B 級是「只給人讀」。這一行是之後評估判定的輸入，會由工具或判定讀，所以一開始就登記為 A 級（Design Invariant 1；同類的先例：`connection interrupted`、`phase summary` 在沒有讀者時就是 A 級）。新的事件名、帶 `version=1`（R2）。

- `log-contract.zh-Hant.md` 1.1 的表加一列（寫出端、錨點、各鍵的意思）。
- `keys-v1.tsv` 加 `match_quality_window_failed`（全部 13 個鍵）；`prefixes.txt` 加錨點。
- 既有的突變 `lc-pin-evicted-drops-key` 的 `find`（`" reference_age_ms="`）在新的行裡也出現，變成兩處，`test_run_mutations.py` 判它過時。把 `find` 補上 evicted 行的上下文（`std::to_string(eviction.playerId)+" reference_age_ms="`），突變的意思不變。

## 測試（§14.3：先 Red 再 Green）

- 既有的「PvP host evicts a player after three failed connection-quality windows」：只**加**斷言（判定沒有放寬）。三個失敗的窗口都留了紀錄（連續 1、2、3，Tick 1200／1800／2400，參考年齡 199～201 ms、`reference_age_failed` 成立、其他兩個不成立，分母 600、分子 0、樣本 600），最後一筆的參考年齡和 eviction 的相同；player 6 沒有紀錄。
- 新的「PvP host records which conditions failed a connection-quality window」：一個照 snapshot 的 epoch 每 Tick 送下一個命令的 Client。
  - 第 2 窗口：一次多送 5 個命令，Backlog 熔斷重設、沒有替代 → 只有 `movement_resets_failed`。
  - 第 3 窗口乾淨 → 沒有紀錄，所以第 4 窗口的連續失敗數回到 1。
  - 第 4 窗口：保持 2 個命令的領先，每 10 個丟 1 個 → 替代 59／600＝98‰，只有 `substituted_failed`。窗口最後一 Tick 丟的那個要到下一窗口才輪到，所以是 59 不是 60。
- 新的「PvP IPC writes a failed connection-quality window as its log line」：逐字比對。
- Red：先放空殼（不留紀錄、行回傳空字串），三個測試與 `test_log_contract.py` 都失敗（紀錄 0 筆、行是空的、keys 的釘住找不到敘述）。之後實作，全部 Green。
- 新測試第一次跑時，有 3 處是測試自己的假設錯了（不是實作的錯），改了測試：
  - 窗口在 Tick 1200／2400 關閉，不是 +1。
  - 原本第 4 窗口用「沉默 90 Tick」做替代：即時送出（沒有領先）的 Client 只要缺 1 個命令，Starvation 就立刻重設，之後沒有命令就不再解決，所以只替代了 1 步。改成保持領先、丟命令。
  - 59 和 60 的差，原因同上。

## 驗證（macOS Intel，test preset）

- 建置成功；`ctest -R '^object_fps_pvp\.'` 51／51 通過（第一次跑時 `mutation_tool` 失敗，就是上面的過時突變，修好後重跑全部通過）。
- D184 的相關突變 8／8 killed：
  - 新增 4 個：`qwl-failure-not-recorded`（不留紀錄）、`qwl-conditions-swapped`（參考年齡與替代的條件對調）、`qwl-line-wrong-field`（`substituted_permille` 寫成參考年齡）、`qwl-pin-renames-key`（改鍵名）。
  - `find` 改過的 `lc-pin-evicted-drops-key`；`keys-v1.tsv` 改過，所以其他 `log_contract` 檢查的 `lc-pin-gateway-transport-reorder`、`lc-pin-match-start-rename`；`find` 在 `JudgeConnectionQuality` 附近的 `v7-flow-ledger-evicted-executed-discarded`。
- 證據：會話的 scratchpad 裡的 `mutations-qwl.json`、`ctest2.log`。

## 範圍的限界

- `IpcHost` 那一行呼叫（取出後寫 log）沒有自動測試：要在 IPC 測試裡讓一個窗口失敗，得實際跑 20 秒以上的 Tick（第一個窗口不判定）。刪掉這一行不會被任何測試抓到。紀錄的內容（host 測試）、行的格式（逐字測試）、鍵序列（`keys-v1.tsv`）各自有守衛。`evicted player=` 那一行呼叫也是同樣的情況。
- 實機上還沒看過這一行，要等下一次場次。

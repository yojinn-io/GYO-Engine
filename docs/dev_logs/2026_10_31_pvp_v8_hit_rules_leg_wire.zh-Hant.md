# PvP v8：v8-product-hit-rules-leg-wire（wire 與兩端的檢查接受「腿」）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 3～5 h，實際約 1.5 h。

收支：一致（L1 的兩樹比對 scale 1、3 各 42 個情境，0 差分）

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hit-rules-leg-wire-declaration.md)（`v8-product-hit-rules-leg-wire-declaration-approve`）判定，沒有觸發停止。Match 仍然只產生 Body。四平台 L1 照 `pv9-single-merge`，在整條 hitbox-line 的 PR 上跑。

## 做了什麼

- **proto**：兩份 proto 的 `HitPart` 加 `HIT_PART_LEG = 3`，註解改成 pv9 §2。Go 綁定用 protoc 36.2 與 protoc-gen-go v1.36.11 重產；差異只有新的列舉值，以及跟著變的描述子位元組。
- **C++**（`Combat.hpp`）：
  - `HitPart::Leg = 3`。
  - 新增 `PlayerHitPart`（Body、Head、Leg），`ValidDamageRecord` 改用它。
  - `ValidDecisionHitPart` 加上 Leg。
  - `ShotDamage` 對 Leg 用 `legPermille`。
- **對照**：`IpcHost.cpp` 的 `static_assert` 加上 `Leg == HIT_PART_LEG`，`HitPart_ARRAYSIZE == 4`。
- **Gateway**：`playerPart` 加上 `HIT_PART_LEG`。
- **Client**：解碼程式不改，跟著兩個檢查函式一起接受腿。
- **文件**：`protocol/README.md` 的 `HitPart` 段落。

## 判定

- **L1：通過。** base `e54a74b9`（`GYO-Engine-v8-base`）、branch＝本批，`compare_authority_trees.py` 一般模式：scale 1、3 都是「42 scenarios, 0 differing」；`authority_golden.txt` 和 base 位元組相同。
- **L2：通過。** 宣告那張表，C++ 與 Go 各自實作同一組檢查：
  - C++（`CombatTests.cpp`）：
    - 命中玩家的 decision，部位 {1,2,3} 接受，{0,4,99,-1} 拒絕。
    - 打空、打到世界的 decision，只接受 0；{1,2,3,4,99} 都拒絕。
    - 受擊紀錄接受 Leg，拒絕 4 與 99。
    - `ShotDamage(Leg)` 在血量 100000 時是 18750，在血量 10000 時被截成 10000。
  - Go：
    - `TestResultsCheckTheHitPart`：Leg 的命中照抄過去；Leg、4、99 的 miss 與 4 的命中都拒絕；`EqualDecision` 能分辨 Body 和 Leg。
    - `TestHitRecordIsValidatedAndKeptVerbatim`：Leg 照抄過去，4 被拒絕。
- **L3：通過。**
  - C++ → runtime proto：靠 `static_assert` 守住（編譯通過）。
  - runtime proto → client proto：Go 的兩個測試，經過轉換之後的值仍是 `HIT_PART_LEG`。
  - client proto → Client：worker probe（`v8-product-leg-wire-worker-probe` (a)）實際送過兩次，Client 都讀成 `HitPart::Leg`：
    - mock Gateway 送出受擊紀錄是腿的 snapshot。
    - action 1 的結果改成「命中玩家 2 的腿」。重送的那份內容相同。
  - 原本 Body 的受擊紀錄檢查保留。
- **L4：通過。** 大小的釘都沒動（706 B、748 B、Go 的 capacity 測試、Welcome 1200 B），測試照舊通過。
- **L5：** 見「突變」。
- **L6：通過。** `test` preset 的 CTest 111 項、`go test ./...` 都通過（CTest 的經過見下）。

## 經過（AGENTS §14.1）

- 第一次跑全部 CTest（`-j8`）時，`object_fps_pvp.cpu`、`presentation_evidence`、`combat_gui_evidence_v7` 逾時。當時主機的負載平均是 79（其他會話正在建置），所以判斷是環境造成的。這三項沒有改動，序列重跑之後都在時間內通過（cpu 21 s、presentation_evidence 17 s、combat_gui_evidence_v7 7 s）。這次執行只是在確認既有測試，不是事前宣告的受入判定試行。
- 同一次執行裡，`mutation_tool` 失敗是真的：既有突變 `v71hf-decode-hit-record-part-unchecked` 的 find 落在這批改寫的那一行（`ValidDamageRecord` 改用 `PlayerHitPart`）。我把 find 更新成新的那一行；突變的意思（拿掉部位檢查）沒有變。

## 改了判定的既有測試（AGENTS §14.4）

V1～V3 照宣告：值 3 從拒絕變成接受（寬鬆面）；4 從沒測變成明確拒絕（收緊面）。1～5 的揭露見宣告。commit 之前做的兩項檢查：

- 突變：見下。補上 3 個「只放寬未知值」的突變之後，4 與 99 的測試有檢出力的證據才齊全。
- 審查：交給 Sonnet 5.5 子 agent（檔位 medium，只讀；讀 `git diff HEAD` 與 HEAD 版本）。它逐項核對 1～5：
  - 1、2、3、5：成立。值 3 在 HEAD 的每個檢查都被拒絕；變更之後只有在命中玩家的 decision 與受擊紀錄上接受 3；打空、打到世界、裝填時仍然拒絕。HEAD 的每一個拒絕案例都還在，或者被新的案例涵蓋；唯一移到「接受」那一組的就是 3。
  - 4：**部分成立。** Go 端在 HEAD 就有 99 的案例。C++ 端在 HEAD 只有 3（受擊紀錄）與 `{0,3,-1}`（decision），99 是這批**新加**的。宣告寫「以 4 與 99 繼續守住」，對 C++ 端來說不精確：守住的測試是同一組，但 C++ 的 99 不是「繼續」，是新增。結論不變。
  - 另外指出兩點，下面兩項處理：
    1. **缺少只抓未知值的突變。** 舊突變（`v71hf-*`）是把部位檢查整個拿掉，Unspecified 的案例也會失敗，所以證明不了 4、99 的案例本身有檢出力。補了 `legwire-decision-accepts-unknown`、`legwire-record-accepts-unknown`、`legwire-gateway-accepts-unknown`：讓檢查接受任何非零值，只有 4、99 的案例會失敗。3／3 killed。
    2. **worker probe 改了既有步驟的輸入。** action 1 的兩次 `Results({1})` 原本是 Cooldown 的拒絕，現在是命中腿。這一步原本在檢查「ACK 推進到 32」與「收到時間取第一次到達」，這兩項都照舊檢查。Cooldown 拒絕的解碼由 id 2～9 的 `Results` 與 `Drain` 檢查繼續涵蓋（`client fabricated a successful result`）。所以沒有寬鬆面，只是這一步多驗了腿。
  - 小事：`gofmt` 順便把 `hit_record_test.go:16` 的對齊改了一格空白。

## 突變（L5）

**24／24 killed**（只在預期的訊息失敗時才算 killed），在未提交的工作樹上用 `--allow-dirty` 跑。照宣告，commit 要在突變與審查之後。

- 本批新增 9 個（`batch` 為 `v8-product-hit-rules-leg-wire`）：
  - 宣告列出的 6 個：`legwire-gateway-drops-leg`、`legwire-decision-drops-leg`、`legwire-record-drops-leg`、`legwire-damage-leg-as-body`、`legwire-enum-off-wire`、`legwire-client-reads-leg-as-body`。
  - 審查之後補的 3 個：`legwire-*-accepts-unknown`。
- `legwire-enum-off-wire` 第一次跑是 `build_failed`：我登記時漏了 `expect_build_failure`。`static_assert` 是編譯期的守衛，工具本來就支援這種突變，補上標記後單獨重跑，killed（預期的訊息是 `HitPart differs from the wire`）。
- D184 相關的既有突變 15 個，全部 killed：
  - find 落在改動的行上：`09-hit-dead-rule`、`v71hf-decode-hit-record-part-unchecked`（find 已更新）、`v71hf-client-hit-part-unchecked`。
  - 殺它的測試被改：`09-hit-*`、`v71hf-gateway-*`、`permille-*`。
  - worker probe 改到的檢查：`v7-08-receipt-overwritten`、`wpr-client-body-hit-read-as-head`。

## 範圍的限界

- Match 不產生 Leg，所以「Match 判出腿 → Client 看到腿」的完整路徑，要到 `v8-product-hitbox-shot` 才有證據。
- L1 只在這台 Intel Mac 上比。

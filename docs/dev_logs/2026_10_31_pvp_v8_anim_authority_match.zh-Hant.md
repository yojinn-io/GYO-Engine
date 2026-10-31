# PvP v8：v8-product-anim-authority-match（A2：Match 每 Tick 推進動畫狀態）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 4～6 h，實際約 1.5 h。

收支：一致（E3：scale 1、3 各 42 個情境，19636＋58774 筆紀錄全部比過，0 差分；新增的 9 個欄位每一個都出現過）

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-anim-authority-declaration.md)的 A2。

## 做了什麼

- **`PvpMatch`**：
  - 建構多一個參數 `PlayerAnimationRules`，預設是 `SyntheticPlayerAnimationRules`。
  - 每位參與者記住上一個 Tick 的動畫值，以及推進到的那個狀態。
  - `Join` 時先算一次（在目前的 Tick 重設），所以從加入的那一刻起，snapshot 裡每位玩家都有一個值。
  - `Tick` 的最後，等移動、生命邊界、射擊都處理完，才對**每一位**玩家推進一次。這樣不會在重生或跳過移動的那個 Tick 漏推進，也就是 A1 審查的 S1。
- **`WorldSnapshot.animation`**：和 `players` 一對一。`Snapshot()` 送出前呼叫 `RequireValidAnimation`，用 `GYO_ASSERT` 檢查 `ValidPlayerAnimations`（`pv9-gateway-check-failure` (a)）。
- **`ValidPlayerAnimations`**（`match_domain`）：protocol-v9 §4.5 的單則訊息檢查，A3 的各個讀者共用。
  - 一位玩家剛好一個值，順序不限，而且屬於那位玩家目前的命。
  - 權重 ≤ 1000，sprint 為 0。
  - 階段 ∈ {1..4}，零點 ≤ tick。
  - 活著的玩家：Start／Airborne ⇔ 不 `grounded`。
- **Match 執行檔**：啟動時用 `LoadPlayerAnimationRules(Assets 資料夾)` 從內容載入規則（只登記文字與 FBX 的載入器，不讀貼圖），交給 `MatchRuntimeHost`，並寫一行 `animation rules` 的 log。內容不合就停止。Match 連結 `player_content`；`player_content` 私下連結 `GYO::AssetUfbx`。
- **`MatchRuntimeHost`**：建構多一個參數，預設同樣是合成的規則。
- **權威 digest version 4**：每個 Tick 紀錄的最後，加上每位玩家的動畫欄位。用合成的規則，所以 golden 不會因為玩家的 clip 改了就跟著變。
- **比對工具**：`--added` 可以單獨用，不必搭配 `--scaled`。工具自己的測試加了 2 項。

## 判定

- **E3：通過。** base `f2974e74`（A1 完成，`GYO-Engine-v8-base`）、branch＝本批，同一台 Intel Mac，`--added animations,animPlayer,animLife,gaitPhaseQ16,moveWeight,jogWeight,jumpPhase,jumpPhaseTick,sprintWeight`：
  - scale 1：42 個情境、19636 筆紀錄，0 差分。
  - scale 3：42 個情境、58774 筆紀錄，0 差分。
  - 新增欄位每一個都出現過。抽查 scale 1：
    - 相位有 2052 種值；
    - 跳躍四個階段都出現過（1：19907、2：1015、3：2168、4：450）；
    - `move_weight` 只出現 0、166、333、500、666、833、1000 這幾個值。
  - golden 重產 37 行，舊檔留為 `authority_golden.pv9-permille-checkpoint.txt`。
- **E4：通過。** 「PvP Match refuses to publish an animation the Gateway would refuse」：§4.5 的 14 種違反情況，各自讓 `ValidPlayerAnimations` 回傳 false，`RequireValidAnimation` 也各自觸發 Assert（`GYO_CHECK_ASSERTS`）；順序互換、死亡玩家不做交叉檢查，這兩種照舊接受。
- **Match 整合**（2 個 TEST_CASE）：
  - 加入時就有值；
  - 走路之後相位前進，`move_weight` 到 1000；
  - 起跳的 Tick 是 Start，零點＝這個 Tick；
  - 死亡期間值凍結，重生後第 2 條命從 0 開始，`jog_weight` 0（審查 C1），零點＝`life_state_tick`。
- **W3**（`PvpMatch` 的建構多一個參數）：用預設參數，既有的 80 處呼叫不必改，判定也沒有改。

## 突變

**11／11 killed**（只在預期的訊息失敗時才算 killed），在未提交的工作樹上用 `--allow-dirty` 跑。

- 本批新增 7 個（`batch` 為 `v8-product-anim-authority-match`）。突變清單也新增了檢查項 `authority_digest`。
  - `a2-no-advance`：Match 加入之後就不再推進動畫。
  - `a2-join-without-value`：加入時沒有先算一個值。
  - `a2-check-life`／`a2-check-duplicate`／`a2-check-sprint`／`a2-check-grounded`：§4.5 的檢查各拿掉一條。
  - `a2-digest-without-animation`：digest 不編入動畫欄位。
- `a2-no-advance` 第一次跑是 `wrong_failure`：先失敗的不是我預期的整合測試，而是另一個測試在 Match 自己的 `GYO_ASSERT failed: ValidPlayerAnimations` 停下。不推進的話，跳躍或重生之後，動畫值和 `grounded`、命會對不上。這正是寫入前的守衛應該抓到的情況，所以把預期的訊息加上這條 Assert，單獨重跑，killed。
- D184 相關的既有突變 4 個，全部 killed：
  - `v71a1-rules-ignored`：find 落在建構子那一行，已更新；
  - `anim-death-not-frozen`：殺它的測試裡新增了 Match 的死亡情境；
  - `ac-room-named-clients`、`ac-frames-first-column`：比對名稱時寬鬆比到的，照樣跑了。

## 其他測試

- `test` preset 的 CTest 111／111 通過。

## 範圍的限界

- `Snapshot()` 呼叫 `RequireValidAnimation` 的那一行沒有測試能抓到。正常的 Match 本來就產生不出違反的值；E4 直接測檢查函式與 Assert。
- Match 是否真的把載入的規則（而不是合成的規則）交給 `MatchRuntimeHost`，目前只有啟動時的 log 行可以看，沒有測試。A3 的 Ready／Welcome 會帶 `animation_digest`（在 `v8-product-hitbox-data`），到時候 Client 端就會發現兩邊不一致。
- 只在這台 Intel Mac 上比。零角度的 golden 子集要在四平台 L1 上檢查逐位元一致：狀態機只用 sqrt、除法、floor、llround，加上 yaw＝0 時的 sin／cos，而且建置有 `-ffp-contract=off`。

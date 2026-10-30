# PvP v8：v8-product-crouch 的 wire 接入（pv8 版本值 8）

日期：2026-10-30。分支 `claude/pvp-v8-pv8-line`。檔位 high（預測與 wire 轉換局部 xhigh）｜Opus 5.5。

**狀態：** 完成。pv8-line 的 A 層（`pv8-rename`、`pv8-seq-ceiling`、`pv8-seq-narrow`、本批）全部做完，蹲下在真的連線中可以用。版本值是 8。PR 還沒開。

相關文件：[事前宣告](../object_fps_pvp/plans/v8/v8-product-crouch-wire-declaration.md)（`v8-product-crouch-wire-declaration-approve`）、[protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md) §1～§6、§8、[不碰 wire 部分的 dev_log](2026_10_30_pvp_v8_crouch_nowire.zh-Hant.md)、[v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md)。

## 做了什麼

- **版本值 7→8**：`Wire.hpp`、`adapter.go`、`acceptance_protocol.hpp`、`acceptance_util.py`；突變 `09-version-python` 的 find／replace 改成 8／7。
- **proto**（兩份同號）：`MovementCommand.crouch_held`＝7、`sprint_held`＝8；`PlayerState.crouched`＝20、`sprinting`＝21；Ready 10～13／Welcome 12～15 的 4 個 float。
- **Gateway**：逐欄複製（input、snapshot、Ready→Welcome）；`EqualCommand` 也比較兩個新 bool；`ReadyFitsWelcome` 把 4 個 float 算進去。沒有新狀態、新判斷、新 DropReason。
- **C++ 轉換**：IpcHost（命令解碼、snapshot 編碼、Ready）、ClientConnection（命令編碼、snapshot 解碼、Welcome）。新欄位都接在 struct 最後。
- **移動規則資料**（`v8-product-crouch-rules-file` (a)）：`assets/object_fps_pvp/movement_rules.json`（`version` 1，4 個值，鍵要剛好這 5 個），`asset_catalog.json` 登記。新的 `MovementRulesFile.hpp/.cpp`（domain）讀檔並套 `ValidStanceRules`，違規就報錯、不退回預設值。`match_main` 加 `--movement-rules`（預設正式版的檔），`MatchRuntimeHost` 多一個 `StanceRules` 參數交給 `PvpMatch`，IpcHost 從 host 取來放進 Ready。
- **Client**：`MovementRules` 多一個 `stance`（`StanceRules` 的定義移到它前面）。Welcome 的 4 個值先做不需要地圖的檢查（有限、正、眼高 ≤ 蹲高、蹲速 ≤1、衝刺 ≤3），地圖相關的部分由預測採用時的 `ValidStanceRules` 檢查；同一 session 內不同就報 `Match movement rules changed within a session`。`ClientSimulation::Observe` 把 `stance` 交給預測。
- **按鍵**（`v8-product-crouch-key` (a)）：按住 Left Ctrl。`ClientIntent.crouch` → `ClientSimulationRole` 只在「自己的、新鮮的、有 controls」的 intent 時更新蹲的意圖，否則保留上一次（§5）→ `ClientInputSample.crouch` → 預測的 `Advance`。操作說明加上「Ctrl crouch」。`sprint_held` 一律 false。
- **呈現**：
  - 本機相機的眼高依預測的姿態取 `StanceEyeHeight`，只在呈現層以 0.08 s 時間常數平滑；`LocalMovementObservation` 多一個 `crouched`。
  - 遠端：animset 加 `crouch_idle`（`Crouch_Idle_Loop`）、`crouch_move`（`Crouch_Fwd_Loop`）；`crouched` 時下半身用兩者依移動權重混合，上半身照舊瞄準。模型縮放在載入時決定，姿態碰不到它。
- **驗收工具**：`run_quad` 的 mixed 模式保留（`v8-product-crouch-run-quad-mixed` (a)），只改註解；它以 `PROTOCOL_VERSION - 1` 寫成通用形式，給它 pv7 的 base 執行檔就能用。

## 檢驗與經過

- **W1**（事前宣告）：base `58af2861`、branch＝本批，同一台 Intel Mac，`compare_authority_trees.py` 各 1 次：scale 1、scale 3 都是「42 scenarios, 0 differing」。既有的 38 個情境 0 差分；多出的 4 個是本批新增、base 沒有的情境。
- **W2**：`move-crouch-sprint-held/contract`（`move-crouch/contract` 加上一直按住衝刺）的 digest 和 `move-crouch/contract` 相同；`CrouchTests.cpp` 的 `PvP sprint_held changes nothing in pv8` 逐 tick 檢查 `sprinting` 為 false，最終狀態相同。
- **W3**：Go（handshake 測試：Welcome 帶 4 個值、input 帶 `crouch_held`／`sprint_held`；snapshot mapping 帶 `crouched`／`sprinting`）、C++ IpcHost（`PvP IPC carries the stance rules in Ready, the crouch in and the stance out`）、worker 探針（`CheckStance`：按住蹲送出 `crouch_held`、收回 `crouched`、Client 採用 Welcome 的值）。
- **W4**：`PvP movement rules data loads the release file and refuses every violation`（正式版的檔對兩張正式地圖都能讀；14 種違規都拒收：每一條界線、非有限、非數字、版本、缺鍵、多鍵、不是 JSON、檔案不存在）；worker 的 `CheckStance`（4 種違規的 Welcome、同一 session 內改變）；`PvP ClientSimulation predicts with the stance rules Welcome delivered`（蹲速 0.25 的預測位移是 0.5 的一半）。
- **W5**：`proto.Size` 實測，寫進 protocol-v8 §6 並釘成完全相等：Snapshot 契約最大 748 B（40＋177N）、型別最大 822 B（42＋195N，餘裕 378 B）、合法值 698 B、PlayerInput 450 B；Action 不變；Welcome 邊界仍是 1200 B。全部和推論一致。
- **W6**：`PvP crouched remote lower body plays the crouch clips on the standing model`。
- **W7**：登記的突變 23／23 killed（新 17 個＋判定測試被改到或 find 落在改動行上的既有 6 個）。
  - 既有突變 `v7-02-observe-ignores-rules`、`v71te-welcome-bound-without-echo` 的 find 因本批改寫而失效，改成語意相同的 find（前者拿掉 `SetMovementRules` 的呼叫，後者把最大的時間回聲換成空的 `ClockSample`）；兩者 killed。
  - `v8cw-client-rules-change-unchecked` 第 1 次是 build_failed：replace 多了一個右括號（登記的錯），修正後 killed。
  - 宣告 W7 列的「模型縮放改用蹲姿高度」沒有登記：縮放在載入時由定義決定，姿態沒有路徑能到它，突變打不進去。「大小的等式改成 ≤」也沒有登記：那是改測試本身，不是改實作。
- **W8**：`test` preset 的 CTest 100／100，`go test ./...` 通過。
- **§14.4**：U1（版本位元組的 golden：`WireTests.cpp` bytes 4～5、`heartbeat_test.go` 與 `IpcHostTests.cpp` 的 `\x08\x07`→`\x08\x08`）與 U2（大小釘：`capacity_test.go`、`worker_main.cpp`、`network_main.cpp`、`adapter_test.go`；`maximumSnapshot` 與探針的最大值訊息把新 bool 設為 true；Welcome 邊界測試的 Ready 帶 4 個 float）照宣告改，都沒有寬鬆面（同等或完全相等的釘），事後回報。另外，worker 探針的假 Gateway 補上 Welcome 的 4 個值：沒有它們，Client 照新規則拒收，這是測試輸入的補齊，不是判定的變更。
- **§14.1**：突變工具第 2 次單跑 `v8cw-client-rules-change-unchecked` 時，前置檢查的 worker 失敗 1 次，沒有留下 log，原因無法切分（實作、測試或環境都可能）。之後同一個單跑的前置檢查通過，全部 CTest 通過，worker 單獨重複 10 次都通過。記錄為「未切分、之後未重現」。

## protocol-v8 §8 的 12 個情境的對照（`v8-product-crouch-scenarios` (a)）

| §8 的名字 | 由誰涵蓋 |
|---|---|
| `crouch-hold` | digest `move-crouch/contract`；`PvP crouch holds the stance, lowers speed and stands only with room` |
| `crouch-stand-blocked` | digest `move-crouch-slab/slab`；同上的測試 |
| `crouch-walk-under` | digest `move-crouch-walk-under/slab`（本批新增） |
| `crouch-shot` | `PvP crouch changes the shot eye and the target capsule` |
| `crouch-held-neutral-gap` | `PvP crouch survives held and neutral substitution and an epoch rotation` |
| `crouch-epoch-reseed` | digest `move-crouch-epoch-reseed/contract`（本批新增）；同上的測試 |
| `crouch-respawn` | `PvP a crouched death respawns standing`（`CombatTests.cpp`） |
| `crouch-dead-release` | `PvP crouch in the air stands up only with room and freezes while dead` |
| `crouch-airborne` | 同上的測試；digest `move-crouch-jump-hold/contract` |
| `crouch-jump` | digest `move-crouch/contract`（t＝200、260 的跳）；`PvP crouch jump: a grounded jump stands up first and never jumps from under a slab` |
| `crouch-jump-blocked` | digest `move-crouch-slab/slab`；同上的測試 |
| `crouch-jump-hold` | digest `move-crouch-jump-hold/contract`（本批新增） |

新增的 3 個情境以 dump 確認走到了預期的路徑：walk-under 蹲著走到 x＝9.5（slab 在 x 6～8）才站起；jump-hold 跳到 0.6、空中蹲姿 116 個 tick；epoch-reseed 在蹲姿中 epoch 輪替、輪替時仍是蹲姿。

## 範圍的限界

- 蹲下的手感與遠端蹲姿的觀感 CI 看不到，這批不做 L3；之後在 Windows 實機確認時另排。
- 本機只有 Intel Mac；W1 只在這台比，跨平台靠 golden 子集與 L1。

## 證據

- 兩樹比對、突變報告、dump 在會話的工作檔（`crouch-w1-s1`、`crouch-w1-s3`、`crouch-mutants.json`、`crouch-mutants-2.json`、`dump-move-crouch-*.txt`、`worker-repeat-*.log`），不放進 repo；結果行照抄在上面。

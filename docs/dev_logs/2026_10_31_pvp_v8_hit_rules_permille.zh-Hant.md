# PvP v8：v8-product-hit-rules-permille（血量與傷害改成千分之一點，版本值 9）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 10～16 h，實際約 3 h。

收支：一致（P1 的兩樹比對 scale 1、3 各 42 個情境，19636＋58774 筆紀錄全部比過，0 差分；新增欄位每一個都出現過）

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hit-rules-permille-declaration.md)（`v8-product-hit-rules-permille-declaration-approve`，3 項都照建議 (a)）判定，沒有觸發停止。四平台 L1 照 `pv9-single-merge` 在整條 hitbox-line 的 PR 上跑。

## 做了什麼

- **規則**（`Combat.hpp`）：`maximumHp` 100000、`shotDamage` 25000、`maximumShotDamage` 50000；部位倍率 `headPermille` 2000、`bodyPermille` 1000、`legPermille` 750 加進 `CombatRules`（只在 Match，不上 wire）。
- **傷害函式**：`PartDamage`（shotDamage×permille/1000，乘積用 uint64）、`ValidPartDamage`（能整除、≥1、≤ `maximumShotDamage`，三個部位都檢查）、`ShotDamage(rules, part, hp)`（再以 hp 為上限）。`PvpMatch` 的建構檢查加上 `ValidPartDamage`，命中時改用 `ShotDamage(rules_, decision.hitPart, hp)`；part 仍然只有 Body。
- **版本值 8→9**：`Wire.hpp`、`adapter.go`、`acceptance_protocol.hpp`、`acceptance_util.py`；突變 `09-version-python` 的 find／replace 改成 9／8。
- **HUD**：`DisplayPoints`＝`x/1000 + (x%1000 != 0)`，hp 與上限都用它顯示。`WeaponFeedbackObservation` 的 hp、maximumHp、lastDamage 照存千分之一點（註解標明）。
- **權威 digest version 3**：combat 紀錄最後加上 `lastDamageTick`、`damageCount`、`lastAttackerId`、`lastHitPart`，decision 紀錄最後加上 `hitPart`。另外每筆 combat 紀錄都檢查 `damageCount` ≤ 6（P7）。
- **golden**：`authority_golden.txt` 重產，舊檔留為 `authority_golden.pv8-checkpoint.txt`，檔頭補一段說明。
- **比對工具**：`compare_authority_trees.py` 加 `--scaled NAME[,NAME]=FACTOR` 與 `--added NAME[,NAME]`。這兩個模式下，每個情境都會兩樹各 dump 一次，逐筆、逐欄比對：被指定倍數的欄位必須恰好是 base×倍數，新增欄位先拿掉再比、但至少要出現一次，其他欄位逐字相同。工具的測試 `test_compare_authority_trees.py`（CTest `object_fps_pvp.compare_authority_trees`，10 項）。
- **文件**：protocol-v9 §6 補實測值；`protocol/README.md` 改成版本 9。

## 判定（宣告的 P1～P9）

- **P1：通過。** base `c1963051`（`GYO-Engine-v8-base` worktree）、branch＝本批，同一台 Intel Mac，`--scaled hp,damage=1000 --added lastDamageTick,damageCount,lastAttackerId,lastHitPart,hitPart`：
  - scale 1：42 個情境、19636 筆紀錄，0 差分；`hitPart` 出現 246 次，受擊紀錄四欄各出現 23539 次。
  - scale 3：42 個情境、58774 筆紀錄，0 差分；`hitPart` 695 次，四欄各 65278 次。
  - 實際有命中被比到：scale 1 的 decision 紀錄裡，`damage=25000` 有 65 筆（base 是 25）。
- **P2：通過。** 新 golden 是 P1 通過之後 branch 在 scale 1 的輸出；`object_fps_pvp.authority_digest` 通過。跨平台的一致性在 L1 檢查。
- **P3：通過。** `PvP part damage follows the permille multipliers`：頭 50000、軀幹 25000、腿 18750；hp 不足時取 hp；`PartDamage(3000000, 2000)`＝6000000（超過 32 位元）；整除、0、超過上限各被拒。`PvP Match rejects rules whose part damage is invalid`：`shotDamage` 20001（腿 15000.75）、`legPermille` 0、`headPermille` 2001 在建構時被拒。
- **P4：通過。** `PvP HUD shows whole points rounded up`：0→0、1→1、999→1、1000→1、1001→2、100000→100。
- **P5：通過。** `WireTests` 的 7 種訊息位元組 golden，只有版本欄位 0x0008→0x0009。
- **P6：通過。** 型別最大以 MaxUint32 取 hp、damage，數值不變。合法值的 4 人 Snapshot（`network_main.cpp:159`）由 698 B 變成 706 B：每人的 hp 由 1 B 的 varint 變成 3 B。錯誤訊息改成會印出實際大小。Welcome 的邊界（`actions_test.go`）是由 arena id 動態長到 1200 B 的，照舊等於 1200。
- **P7：通過。** `PvP one life takes at most six hits`：ceil(100000／18750)＝6。digest 的 42 個情境、兩種 scale，每筆 combat 紀錄都通過 `damageCount` ≤ 6 的檢查（runner 以 `Require` 檢查，違反就失敗）。
- **P8：** 見下面的「突變」。
- **P9：通過。** `test` preset 的 CTest 111／111、`go test ./...` 通過。

## 和宣告不同的地方

- **golden 是 37 行，不是 42 行。** 42 個情境中，只有 37 個屬於零角度子集（golden）。宣告的「42」把情境數當成了 golden 的行數；判定沒有因此改變（P1 比的是 42 個情境，P2 重產的是全部 37 行）。
- **規則不合法時用 `std::invalid_argument`，不是 Assert。** `PvpMatch` 的建構本來就用這個方式檢查規則（`shotDamage` 為 0 等），所以這批照既有的寫法加上 `ValidPartDamage`。P3 照宣告的另一種寫法，用檢查函式的回傳值和建構時拋出例外來測。
- **腿只經 `PartDamage` 測。** C++ 的 `HitPart` 還沒有 Leg（`v8-product-hit-rules-leg-wire`），所以 `ShotDamage` 只分 Head 和其他。腿的倍率仍然進 `CombatRules`，也在建構時驗證。
- **`lastDamage` 沒有地方顯示。** Client 只存、不畫，所以沒有東西要換算。

## 改了判定的既有測試（AGENTS §14.4）

照宣告的 U1～U6，全部是 ×1000 或 8→9，沒有寬鬆面。下面兩項在宣告的寫法之外，另外說明：

1. `CombatTests.cpp` 的 `CHECK(PvpCombatRules.maximumShotDamage == PvpCombatRules.shotDamage)`（守的性質：「v7.1 沒有部位倍率」）改成 `maximumShotDamage == PartDamage(shotDamage, headPermille)`。這是 `v8-product-permille-max-shot-damage` (a) 核准的規則變更，不是放寬：新判定要求上限剛好是頭部傷害。
2. `OneHitRules`（一發就死的測試規則）加上 `headPermille = 1000`。原本的 `shotDamage` 100000 配上預設的頭部倍率，會是 200000，超過 `maximumHp`，建構會被新的檢查拒絕。這組規則只用 Body，頭部倍率不影響任何測試的結果。

其餘：Go 的 `testRules` 照 ×1000 是 `MaximumShotDamage` 25000（不是產品的 50000），`hitbox_test.go` 的「上限高於基本傷害」的情況改成 50000／50001。`authority_golden.txt` 的變化範圍由 P1 證明。

`damage_count ≤ maximum_hp`（產品的驗證式）的寬鬆面照宣告的 1～5 揭露，由 P7 在 Match 端補強。

## 突變（P8）

**24／24 killed**（只在預期的訊息失敗時算 killed），commit `f6bc3114`。

- 本批新增 6 個（`batch` 為 `v8-product-hit-rules-permille`）：`permille-damage-unscaled`（傷害仍是 25）、`permille-product-32-bit`（乘積只用 32 位元）、`permille-divisibility-unchecked`（拿掉整除檢查）、`permille-hud-floor`（HUD 無條件捨去）、`permille-version-cpp`（C++ 的版本值留在 8；Python 那一處由既有的 `09-version-python` 守）、`permille-compare-any-scale`（比對工具不管倍數就放行）。
- D184 相關的既有突變 18 個：find 落在改動的行上的 2 個（`09-version-python`、`v71hf-match-head`），以及殺它的測試被改過的 16 個（以突變的 `expect` 對照改過的 TEST_CASE／Go 測試函式／Python 測試名，條件放寬，寧可多跑）。
- 第一次執行時，zsh 把 id 清單當成一個參數傳了進去，工具回報 unknown id，沒有任何突變被套用。之後改用 Python 傳參數重跑，得到上面的結果。

## 範圍的限界

- P1 只在這台 Intel Mac 上比；跨平台靠 golden 子集與 L1。
- 千分之一點對手感沒有影響（顯示照舊是整數 HP），這批沒有做 L3。

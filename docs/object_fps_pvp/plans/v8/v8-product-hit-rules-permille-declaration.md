# v8-product-hit-rules-permille 的事前宣告

更新：2026-10-31。Owner：`object_fps_pvp`。**狀態：已核准**（`v8-product-hit-rules-permille-declaration-approve`，3 項都照建議 (a)）。 契約：[protocol-v9](../../protocol-v9.zh-Hant.md) §1、§3、§6（定稿，`pv9-final-approve`）。計畫：[hitbox-plan](hitbox-plan.md) 第 8、9 節。檔位 high｜Opus 5.5（繼承主會話），估時 10～16 h（超過 24 h 停下回報）。

## 範圍

這批之後，血量與傷害全部以千分之一點計，版本值變成 9。**判定的行為不變**：Match 仍然只判 BODY、只打一顆膠囊（部位判定在 `v8-product-hitbox-shot`），所以每一發的結果只是數值乘上 1000。

1. **規則**（`Combat.hpp`）：`maximumHp` 100000、`shotDamage` 25000；部位倍率（頭 2000、軀幹 1000、腿 750，千分比）加進 `CombatRules`，`maximumShotDamage` 的值見「等你決定」第 1 項。
2. **傷害函式**：`ShotDamage(rules, part, hp)`＝min(hp, shotDamage×permille(part)/1000)，中間值用 uint64。建構時驗證：每個部位的結果能整除、≥1、≤ `maximumShotDamage`；不通過就是 Programmer Error（Assert）。`PvpMatch.cpp:370` 改用它（part 仍是 Body）。
3. **版本值 8→9**：`Wire.hpp`、`adapter.go`、`acceptance_protocol.hpp`、`acceptance_util.py` 四處；突變 `09-version-python` 的 find／replace 同步改。
4. **Client 的顯示**：HUD 的 hp 與上限顯示 ceil(x/1000)，寫成 `x/1000 + (x%1000 != 0)`；`weaponFeedback.lastDamage`（`PvpApplication.cpp:477`）照存千分之一點、顯示時換算。
5. **權威 digest 升 version 3**：見「等你決定」第 2 項。
6. **golden 重產**：`authority_golden.txt` 全部 42 行（每個 Tick 的 combat 紀錄都編 hp，所以每一行都會變）。
7. **測試與驗收工具的字面值**：見下面 §14.4 的表。
8. **大小**：以 `proto.Size` 實測，數值寫進 protocol-v9 §6，各處重釘成完全相等。

不做：`HIT_PART_LEG`（`v8-product-hit-rules-leg-wire`）、部位判定（`v8-product-hitbox-shot`）、動畫狀態（`v8-product-anim-authority`）、`animation_digest`（`v8-product-hitbox-data`）。

## 受入判定（看結果之前宣告）

### P1：只有單位變了（兩樹的換算比對）

- base＝`c1963051`（本批之前），branch＝本批，同一台 Intel Mac，scale 1、3 各 1 次，42 個情境。
- `compare_authority_trees.py` 加兩個選項（見「等你決定」第 3 項）：`--scaled hp,damage=1000`（這些欄位 branch 必須恰好等於 base×1000）、`--added <欄位>`（version 3 新增、base 沒有的欄位，只檢查存在）。
- 判定：**每個情境、每筆紀錄，除了 hp、damage 以外的欄位逐位元相同；hp、damage 恰好是 1000 倍；紀錄數與順序相同**。任何例外就停下回報，不重跑。
- 這就是「沒有寬鬆面」的證據：舊 golden 的每一行，在新判定下都對應到唯一的、可由舊值算出的新值。

### P2：golden 重產

- 新 `authority_golden.txt`＝branch 在 P1 通過之後的輸出；檔頭說明照既有格式補一段（本批、DigestVersion 3、P1 的證據位置）。
- 舊檔保留為 `authority_golden.pv8-checkpoint.txt`（pv7 的先例）。
- 跨平台：零角度子集的 golden 在四平台 L1 上逐位元一致（現行規則不變）。

### P3：傷害函式與規則驗證

- 三個部位的值：頭 50000、軀幹 25000、腿 18750；hp 不足時等於 hp。
- 建構時驗證：不能整除（例如倍率 333）、結果為 0、超過 `maximumShotDamage` 各自被拒（Assert，用 death test 或建構檢查函式的回傳值測）。
- uint64 中間值：shotDamage×permille 超過 uint32 時不溢位（例如 shotDamage 3000000、倍率 2000）。

### P4：顯示

- ceil：0→0、1→1、999→1、1000→1、1001→2、100000→100。活著（hp ≥ 1）時不顯示 0。

### P5：版本值

- 每種訊息的位元組 golden 只有版本欄位 8→9（pv7→pv8 的先例），其餘位元組不變。

### P6：大小

- `proto.Size` 實測；契約最大與型別最大照舊以 MaxUint32 取 hp、damage，所以應該不變；變的是「合法值」的釘（`network_main.cpp:136`、`:148`、`:159`）與 Welcome 的邊界（`actions_test.go:135`，arena id 上限縮短）。各處完全相等，每項加 24 B 標頭 ≤1200 B。

### P7：`damage_count` 的 Match 端收緊

- Match 的戰鬥測試加一條：任何情境中 `damageCount` ≤ 6（一條命最多被打 6 次：前 5 次每次至少 18750，100000−5×18750＝6250 > 0，第 6 次必死）。用 `CombatTests` 的模擬與 digest 的 42 個情境都檢查。

### P8：突變

只登記和本批有關的，只在預期失敗時算 killed：傷害沒乘 1000（`shotDamage` 回到 25）；uint64 改成 uint32；整除檢查拿掉；HUD 的 ceil 改成 floor；版本值有一處留在 8；比對工具的 `--scaled` 接受非 1000 倍的差。D184 的相關突變（find 落在改動的行上，或殺它的測試被改的）全部 killed。

### P9：既有測試

`test` preset 的全部 CTest 與 `go test ./...` 通過。四平台 L1 在 PR 時跑（`pv9-single-merge`：整條 hitbox-line 一個 PR）。

## 會改判定的既有測試（AGENTS §14.4，先列出）

| # | 測試 | 守的性質 | 改法 | 寬鬆面 | 收緊面 |
|---|---|---|---|---|---|
| U1 | C++ 的戰鬥測試字面值：`CombatTests.cpp`（約 36 處）、`CombatHostTests.cpp`、`HitFeedbackTests.cpp`、`LifePredictionTests.cpp`、`IpcHostTests.cpp` | 每一發的扣血、死亡、重生、受擊紀錄 | 期望值改成舊值×1000，或改由 `PvpCombatRules` 算出 | 無 | 無（同等） |
| U2 | Go 的字面值：`adapter_test.go`／`server_test.go` 的 `testRules`、`hitbox_test.go`、`hit_record_test.go`、`v5_test.go`、`actions_test.go`、`forwarding_test.go`、`lifecycle_test.go` | Gateway 的單則訊息檢查（hp ≤ 上限、damage ≤ 上限、hp＝0 ⇔ 死亡） | 數值×1000；檢查式不變 | 無 | 無 |
| U3 | `authority_golden.txt` 全 42 行 | 權威結果不變、跨平台逐位元一致 | 重產（P2）；變化範圍由 P1 證明 | 無（P1） | 無 |
| U4 | 驗收工具的期望值：`combat_gui_evidence.py`、`gameplay_evidence.py`、`gameplay_soak_evidence.py`、`run_gameplay_gui.py`、`run_native_window.py` 與它們的 `test_*.py` | 實機證據裡的扣血與擊殺 | 數值×1000 | 無 | 無 |
| U5 | 大小的釘：`network_main.cpp`、`worker_main.cpp`、`capacity_test.go`、`actions_test.go` | 最壞情況 ≤1200 B，任何欄位增大都會被抓到 | 改成 P6 的實測值，仍是完全相等 | 無 | 無 |
| U6 | 版本位元組的 golden：`WireTests.cpp`、`heartbeat_test.go`、`IpcHostTests.cpp`；突變 `09-version-python` | 每種訊息的位元組固定，只有版本欄位隨版本改 | 8→9 | 無 | 無 |

不改：v7 遊戲版本的驗收腳本（`*_v7.py`）、其他產品（`object_fps`、`object_fps_v2`）的測試。開工後若發現本表以外的測試要改判定，停下回報。

### `damage_count ≤ maximum_hp`（產品的驗證式，不是測試；照 §14.4 的 1～5 揭露）

1. **守的性質**：snapshot 不能宣稱比血量允許的更多次受擊。
2. **寬鬆面**：驗證式不變，但上限從 100 變成 100000。現在（25 傷害、100 HP）實際最多 4 次，5～100 本來就會通過；改後 7～100000 都會通過。
3. **收緊面**：無。
4. **由誰守住**：P7 在 Match 端檢查 ≤ 6；Gateway 與 Client 只做單則訊息的界限檢查（不知道最小傷害）。
5. **根據**：`v8-product-hit-rules-damage-count` (a)（沿用到第 2 版，hitbox-plan §9）；`adapter.go:152`、`Combat.hpp:169`。

## 等你決定

### `v8-product-permille-max-shot-damage`：`maximum_shot_damage` 現在就用 50000 嗎

部位倍率放進 `CombatRules` 並在建構時驗證「每個部位 ≤ `maximumShotDamage`」，頭（50000）就必須 ≤ 上限。

- (a) 現在就是 50000，倍率三個欄位一起進 `CombatRules`（建議：就是 protocol-v9 §3 的終值；倍率的驗證在這批就能測；pv9 一次合併，中間狀態不會出貨）；
- (b) 這批維持 25000、不放倍率，`v8-product-hitbox-shot` 再加。這批的變更更純，但傷害函式與驗證要在 hitbox-shot 再測一次。

### `v8-product-permille-digest-v3`：DigestVersion 3 加哪些欄位

- (a) `decision.hitPart` 加上 combat 的受擊紀錄四欄（`lastDamageTick`、`damageCount`、`lastAttackerId`、`lastHitPart`）（建議：`pv9-upper-body-in-hitbox` (a) 之後，受擊紀錄成為判定姿勢的輸入，應該被權威 digest 守住）；
- (b) 只加 `decision.hitPart`（第 1 版計畫的寫法）。

兩者都在 `--added` 下比對，P1 的判定不變。

### `v8-product-permille-compare-tool`：換算比對放在哪

- (a) 擴充 `compare_authority_trees.py`（`--scaled`、`--added`），加它自己的測試與突變（建議：之後 rewind 等批還會用到「只有指定欄位變」的比對）；
- (b) 本批另寫一支一次性的腳本。

## 範圍的限界

- P1 只在這台 Intel Mac 上比；跨平台靠 golden 子集與 L1。
- 千分之一點對手感沒有影響（顯示照舊是整數 HP），這批不做 L3。

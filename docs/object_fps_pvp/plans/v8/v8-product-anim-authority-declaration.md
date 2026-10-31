# v8-product-anim-authority 的事前宣告

更新：2026-10-31。Owner：`object_fps_pvp`。**狀態：已核准**（`v8-product-anim-authority-declaration-approve`，5 項都照建議 (a)，附錄 A 的文字照草稿）。 契約：[protocol-v9](../../protocol-v9.zh-Hant.md) §4、§7（定稿，`pv9-final-approve`）。計畫：[hitbox-plan](hitbox-plan.md) 第 7、8 節。估時 21～31 h（超過 45 h 停下回報）。

## 這批做什麼

步伐相位、移動權重、jog 權重、跳躍階段與它的零點，從 Client 自己累積（`PlayerPresentation.cpp:257-335` 的 `AdvancePlayerLocomotion`）改成：
- Match 每個 Tick 推進，寫進 snapshot；
- Client 照 snapshot 播放。

判定還不用它，部位判定在 `v8-product-hitbox-shot`。這批之後，畫面上遠端玩家的下半身動作就是 Match 決定的那一組。

只有遠端玩家會畫角色（`PvpApplication.cpp:756`，自己的那位 `continue`），所以 Client 不需要預測自己的動畫。

## 分成四個子批

```text
v8-product-anim-authority
├── A1 v8-product-anim-authority-domain   「high（狀態機交 xhigh 審查）｜整數狀態機、規則載入、和舊 Client 狀態機對照」
├── A2 v8-product-anim-authority-match    「high｜Match 每 Tick 推進、寫入前 Assert、digest version 4、golden 重產」
├── A3 v8-product-anim-authority-wire     「high｜proto、IpcHost、Gateway 與 Client 的檢查、驗收工具的解碼、大小」
└── A4 v8-product-anim-authority-client   「high（內插交 xhigh 審查）｜Client 照 snapshot 播放、呈現測試改寫、新的 Design Invariants 節」
```

| 代號 | 估時 | 依賴 | 檢收點 |
|---|---|---|---|
| A1 | 6～9 h | 本宣告 | E1、E2 |
| A2 | 4～6 h | A1 | E3、E4 |
| A3 | 5～7 h | A2 | E5～E7 |
| A4 | 6～9 h | A3 | E8～E10，以及 A4 的 §14.4 表（另外核准） |

- 每個子批各自寫 dev_log、更新 HANDOFF、各自 commit。
- 四個子批都在同一個 PR 線上（`pv9-single-merge`），只有 A3 會動 wire。
- 子批之間的中間狀態不出貨：A2 之後 snapshot 裡有動畫狀態，但 wire 還沒有；A3 之後 wire 有了，但 Client 還沒用。

## A1：共用的整數狀態機

### 放在哪

- `PlayerAnimation.hpp/.cpp` 放進 `match_domain`，只用整數與 `PlayerState`，不依賴 `GYO::Model`。Match 與 Client 共用。
- 規則 `PlayerAnimationRules`（整數 Tick 與 q16）：
  - walk 一圈的距離、jog 一圈的距離（＝native speed × clip 長度）；
  - walk 與 jog 的 native speed；
  - `transition`、`jump_start`、`jump_land` 的 Tick 數；
  - 判斷瞬移的距離。
- 規則的來源見決定 `v8-product-anim-rules-source`。

### 每 Tick 的推進（照 protocol-v9 §4.4；和舊 Client 狀態機一一對應）

輸入是同一位玩家前一 Tick 與這一 Tick 的權威 `PlayerState`。

| 狀態 | 規則 | 舊 Client 對應 |
|---|---|---|
| 重設 | 第一次、換人、換命或生死改變、換 epoch（決定 `v8-product-anim-epoch-rule`）、一個 Tick 的水平位移 > 判斷瞬移的距離。重設時：相位 0、`move_weight` 0、`jog_weight` 由這個 Tick 的速度重算、跳躍階段由 `grounded` 決定（GROUNDED 或 AIRBORNE，零點＝這個 Tick） | `:262-288` 的 spawn／player／life／epoch／teleport。`long_frame`、`time`、`discontinuity` 是 Client 呈現才有的，Match 沒有 |
| 相位 | Δq16＝round(有號的水平位移 ÷ 一圈距離(jog_weight) × 65536)，累加後 mod 2^32。往後走（位移和 yaw 的前方內積 < 0）時取負號 | `:298-301`。backward 的判斷見決定 `v8-product-anim-backward` |
| `jog_weight` | 速度 > 0 時＝floor(1000 × clamp((速度 − walk) ÷ (jog − walk), 0, 1))；速度 0 時保留 | `:303` |
| `move_weight` | 這個 Tick 有位移就往 1000 走一步，沒有就往 0 走一步。步數 k ∈ [0, transition]，weight＝1000 × k ÷ transition（整數除法） | `:309-312`（holding 是 Client 呈現才有的，Match 沒有） |
| 跳躍階段 | 照 §4.4：離地且上升→START，零點＝這個 Tick；離地且下落→AIRBORNE，零點＝這個 Tick；著地→LAND；START 滿 6 Tick→AIRBORNE，零點＋6；LAND 滿 6 Tick→GROUNDED，零點＋6 | `:314-330` |
| 死亡 | 值凍結在死前最後一個 Tick | `:265` 的 life 重設 |
| `sprint_weight` | 恆為 0（② 之前，`pv9-sprint-animation` (a)） | 無 |

- 規則載入時檢查：`transition`、`jump_start`、`jump_land` 的秒數都是 Tick 的整數倍（現在都是 0.1 s＝6 Tick）。
- 一圈距離 > 0，jog 的 native speed > walk 的 native speed。
- 不合就是載入錯誤（Runtime Error）。

### E1：和舊 Client 狀態機對照（看結果之前宣告）

- **路線：** 合成路線 12 條，每條 10 s，以 60 Hz 取樣，每個 Tick 一個 frame：
  - 定速 walk、定速 jog；
  - 加速、減速、停下再走；
  - 往後走、橫移；
  - 原地跳、跑跳、從台階落下；
  - 蹲走、蹲下停住。
- **做法：** 同一組權威狀態分別餵給舊的 `AdvancePlayerLocomotion` 與新的整數狀態機。舊狀態機的 `planarSpeed` 是這個 Tick 的速度，`presentationSeconds`＝tick ÷ 60。
- **門檻：** 全部由量化的精度推出來，不是調出來的。

| 量 | 門檻 | 根據 |
|---|---|---|
| 相位 | \|新 q16 ÷ 65536 − 舊 `unwrappedPhaseCycles`\| ≤ T ÷ 131072 + 10⁻⁶ 圈（T＝重設後經過的 Tick 數） | 每個 Tick 四捨五入最多差 0.5 q16 |
| `move_weight` | \|新 ÷ 1000 − 舊\| ≤ 0.001 + 10⁻⁶ | 1000 × k ÷ 6 的整數除法最多差 0.67‰，加上 float 的累加誤差 |
| `jog_weight` | \|新 ÷ 1000 − 舊\| ≤ 0.001 + 10⁻⁹ | floor 到千分比 |
| 跳躍階段 | 每個 Tick 完全相同；零點＝round(舊 `jumpPhaseSeconds` × 60)，完全相同 | 兩邊都在 Tick 上偵測 |
| 重設的時機 | 完全相同（只有 Match 也有的那幾種重設原因） | 規則一一對應 |

- 不比較的（契約改了，不是誤差）：
  - idle 的時鐘：改成 (t − `life_state_tick`) ÷ 60，§4.1；
  - holding；
  - Client 呈現才有的三種重設原因。
- 任何一條超出門檻就停下回報，不改門檻、不重跑。

### E1 的跳躍判定修正（`v8-product-anim-authority-e1-jump`，試行 1 不成立之後；2026-10-31 使用者核准：「核准並重跑」）

試行 1（[證據](evidence/anim-authority-e1-run1.txt)）：跳躍有 19 個 Tick 不同。每一個都是舊 Client 的 double 比較 `t/60 − s/60 ≥ 0.1` 在剛好 6 Tick 時算出 0.0999999999999996，晚一個 Tick 才換階段。試行 1 的失敗留在紀錄裡，不覆寫。

新判定（`JudgeJump`，`PlayerAnimationEquivalenceTests.cpp`）照 AGENTS §14.4 的 1～5 揭露：

1. **守的性質**：每個 Tick，Match 的跳躍階段與零點都等於舊 Client 的。
2. **寬鬆面**：只多通過一種情況（Start→Airborne 與 Land→Grounded 兩種轉換各算一次）。條件是以下全部成立：
   - Client 在 Start（或 Land），Match 已經在 Airborne（或 Grounded）；
   - Tick＝Client 的零點＋跨度，Match 的零點也＝Client 的零點＋跨度；
   - Client 自己的 double 比較 `tick/60 − 零點秒數 ≥ 跨度秒數` 不成立。

   每次只容許一個 Tick；下一個 Tick 仍然逐 Tick 比對，要回到完全相同。
3. **收緊面**：無。
4. **其他由誰守住**：
   - 其他 Tick 照舊完全相同。
   - 新狀態機剛好在 6 Tick 換階段，由 E2 的「PvP animation jump phases start from their clip time zero points」守住。
   - `JudgeJump` 自己的單元測試，會拒絕這些情況：提早一個 Tick、延後一個 Tick、零點錯、階段的組合錯、浮點比較其實成立。
5. **根據**：試行 1 的證據；protocol-v9 §4.4（跨度是 Tick 的整數倍，0.1 s＝6 Tick）；`PlayerPresentation.cpp:322-330` 的 double 比較。

核准之前做的檢查：
- **突變**：只跑 `JudgeJump` 的單元測試，沒有跑 E1，共 4 個，4／4 killed。
  - 第一輪是 3／4：「不釘住 Tick」的突變存活，因為少了「Match 提早一個 Tick」的情況。補上之後 4／4。
- **審查**：交給 Sonnet 5.5，只讀，不跑測試。1、3、4、5 成立；2 成立，用詞已照它的指正改成「兩種轉換」。它另外重算了證據：
  - 19 個 Tick 都是 `t − s = 6`，double 的差是 0.09999999999999998 或 0.09999999999999964；下一個 Tick 都會跟上。
  - Start 與 Land 的零點都是 `s/60` 本身，累加 `+= 0.1` 只發生在轉換的那一刻，所以不影響比較。
  - 在 s＝1～1999 的範圍裡，約一半的起點會晚一個 Tick。舊 Client 的這個延遲很常見。
  - 延後一個 Tick 的錯誤會在下一個 Tick 被抓到，連續兩個 Tick 都落後的情況也過不了。

**重跑的判定（看結果之前宣告）**：用新判定重跑 E1 一次。
- 相位、兩個權重的門檻不變。
- 跳躍判定為 `Differs` 的 Tick 數為 0。
- 判定為 `ClientFloatLag` 的 Tick，必須**恰好是試行 1 的那 19 個 Tick**：37, 43, 262, 271, 277, 307, 337, 367, 382, 391, 412, 457（兩條路線）, 487, 493, 532, 547, 562, 583（同一台機器，同樣的輸入；試行 1 的路線名稱印成指標，所以以 Tick 的多重集合比對）。不多也不少；有差異就停下回報。為此 E1 會在結尾斷言這個集合（審查指出原本只印出、沒有斷言）。

### E2：狀態機本身

- 每一條規則都有單元測試：
  - 重設的五種原因；
  - 往後走時相位減少；
  - 相位溢位：跨過 2^32 之後，有號差仍然正確；
  - `move_weight` 只取 0, 166, 333, 500, 666, 833, 1000 這幾個值；
  - `jog_weight` 速度 0 時保留；
  - 跳躍的四個階段與零點；
  - 死亡凍結；
  - `sprint_weight` 恆為 0。
- 規則載入：秒數不是 Tick 的整數倍、一圈距離不是正數、jog ≤ walk，各自被拒。
- **xhigh 審查（`v8-product-anim-xhigh-review` 的範圍）：** 交給 `xhigh-reviewer` 子 agent，只讀狀態機與它的測試，找邊界錯誤（換階段的那個 Tick、相位溢位、重設與跳躍在同一個 Tick）。

## A2：Match 推進與權威 digest

- `PvpMatch` 每 Tick 移動之後推進每位玩家的動畫狀態。`WorldSnapshot` 加 `std::vector<PlayerAnimation> animation`，和 `players` 一對一。
- 寫進 snapshot 之前，以 Assert 檢查 §4.5 的同一組條件（`pv9-gateway-check-failure` (a)）：
  - 一一對應、生命相同；
  - 權重 ≤ 1000、`sprint` = 0；
  - 階段 ∈ {1..4}、零點 ≤ tick；
  - 和 `grounded` 交叉檢查。
- **權威 digest version 4：** 每個 snapshot 紀錄最後加上每位玩家的動畫欄位。

### E3：只多了動畫欄位（兩樹比對）

- base＝A1 完成的 commit、branch＝A2，同一台 Intel Mac，scale 1、3 各 1 次，42 個情境。
- 比對工具：`compare_authority_trees.py --scaled` 要求至少一個倍數欄位，這裡不適用。所以加一個 `--added` 單獨使用的模式（不改 `--scaled` 的行為，工具自己的測試跟著加）。
- 判定：除了新增的動畫欄位以外，每個欄位逐位元相同、紀錄數與順序相同；新增欄位每一個都出現過。
- golden 重產（37 行，舊檔留為 `authority_golden.pv9-permille-checkpoint.txt`）。

### E4：Assert 守衛

- 每一條 Assert 條件各寫一個 death test（`AssertTestSupport.hpp` 的既有寫法）。

## A3：wire

- proto 加 `PlayerAnimation`、`JumpPhase`、`WorldSnapshot.animation`（client 8／runtime 7），重產 Go 綁定。
- IpcHost 編碼；Gateway 的 `SnapshotForClient` 檢查並照抄；Client 解碼並檢查（§4.5）；驗收工具的解碼端（`acceptance_protocol.hpp`、`network_main.cpp`、`worker_main.cpp`）跟上。

### E5：白名單（兩端對稱）

- C++ 與 Go 各自實作同一張表，§4.5 的每一條各有接受與拒絕的情況：
  - 數量不合、`player_id` 重複或缺少、生命不同；
  - 權重 1001、`sprint` 1；
  - 階段 0 與 5、零點 > tick；
  - START 卻 `grounded`、LAND 卻不 `grounded`；
  - 死亡玩家不檢查 `grounded` 的交叉。

### E6：往返

- C++ → runtime proto → Gateway → client proto → Client，每一段都有測試。worker probe 送一組非零的動畫狀態，Client 讀到的值相同。

### E7：大小

- 以 `proto.Size` 實測，數值寫進 protocol-v9 §6，各處重釘成完全相等。
- 必要條件：4 人的型別最大加 24 B 標頭 ≤ 1200 B（推論 1022 B）。
- 契約最大照實測記錄（推論約 896 B）。超過 1200 B 就停下回報。

## A4：Client 照 snapshot 播放

- `SnapshotTimeline::Sample` 帶出兩筆 snapshot 的動畫狀態，以共用的純函式 `InterpolatePlayerAnimation` 內插（§4.4）：
  - 權重線性內插；
  - 相位用 int32 的有號差；
  - 跳躍階段取前一筆。
- `PlayerPresentationFrame` 改帶動畫狀態，不再帶 `planarSpeed`。
- `AdvancePlayerLocomotion` 不再累積相位與權重，只保留：
  - holding 凍結：最後一個移動中的姿勢凍結，不讓腿在位置停住時還在動；
  - idle 時鐘：從 `life_state_tick` 起算。
- 呈現用的 `PlayerLocomotionState` 縮小。量測與 survey 工具（`AnimBottlenecks.cpp`、`HitboxSurvey.cpp`）跟著改。

### E8：Client 是 snapshot 的純函式

- 同一組 snapshot、同一個呈現 Tick：30／60／144 FPS 的姿勢輸入（相位、權重、跳躍、clip 時間）完全相同。
- alpha＝0 時，Client 用的相位等於 snapshot 的 q16 ÷ 65536，完全相同。
- 漏收一筆 snapshot 時，內插跨過兩個 Tick，結果等於兩端的內插。

### E9：holding 與重生

- holding 時姿勢凍結；重生之後不沿用舊命的相位（`life_generation` 配對）。

### E10：Design Invariants 的文件（protocol-v9 §7 第 2～5 項的落地）

- 新文件 `docs/object_fps_pvp/player-presentation.zh-Hant.md`，含 `Design Invariants` 節（草稿見下面的附錄 A，要你核准文字）。
- network-architecture 的表加一列；`Combat.hpp:134-137` 與 protocol-v7 §2、§7 的文字照 §7 改寫。

### A4 的 §14.4（先列範圍，A4 開工前另外核准逐項的表）

`PlayerPresentationTests.cpp` 有 12 個 TEST_CASE 直接測 Client 的累積，`:320-660` 的 11 個加上 `:849`。它們守的性質會變成三種：

| 類 | TEST_CASE | 去向 |
|---|---|---|
| 搬到 domain | `:346` 速度混合、`:557` 往後走、`:573` epoch／瞬移重設、`:605` 生死邊界、`:638` 不同玩家不共用、`:849` 跳躍階段 | 同一個性質改由 A1 的狀態機測試守住。判定照舊，或者照門檻 E1 等價 |
| 性質變強 | `:381` 相位用權威速度、`:503` 30／60／144 FPS | 換成 E8 的「完全相同」：比原本的容許差更嚴 |
| 性質不再存在 | `:624` 100 ms／108 ms 的呈現間隔重設、`:526` 停住與 holding 不累積步伐 | 前者 Client 已不累積，所以拿掉；後者的 holding 部分留在 E9 |

「性質不再存在」那一類有寬鬆面。A4 開工前，照 §14.4 的 1～5 逐項列出，跑突變，並交給另一位審查者（Sonnet 5.5）核對，之後才請你核准。

## 會改判定的既有測試（A1～A3）

| # | 測試 | 改法 | 寬鬆面 |
|---|---|---|---|
| W1 | `authority_golden.txt` | A2 重產；變化範圍由 E3 證明 | 無 |
| W2 | 大小的釘（`network_main.cpp:159` 706 B、`worker_main.cpp` 748 B、`capacity_test.go`、`actions_test.go`） | A3 改成實測值，仍然完全相等 | 無 |
| W3 | `PvpMatch` 的建構（測試裡的 `PvpMatch(arena, rules, stance)`） | 多一個動畫規則參數；測試用合成的規則 | 無 |

其他測試照舊。開工後如果發現本表以外的測試要改判定，就停下回報。

## 等你決定

### `v8-product-anim-rules-source`：Match 從哪裡拿一圈距離

一圈距離要用到 walk、jog 這兩個 clip 的長度，而 clip 的長度在 FBX 裡。

- (a) **把 Client 的 CPU 載入器（`LoadPlayerPresentationDefinition`、`CharacterPresentationDefinition`、`AnimationSetDefinition`）搬到一個新的程式庫 `player_content`**，依賴 `GYO::Model` 與 UFBX，Match 與 `app_support` 都連結它。Match 啟動時載入（B2：0.23 s、93 MB）。（建議）
  - 好處：這就是 hitbox-plan §7 第 3 項核准的方向。`v8-product-hitbox-data` 只剩形狀契約。一圈距離只有一個來源。
  - 代價：這是 CMake 模組結構的 Architecture Delta，Match 執行檔會多依賴 UFBX。
- (b) 在 `presentation.json` 加上 walk、jog 的 clip 長度，Match 只讀 JSON，另寫一個測試確認它和 FBX 一致。
  - 這批較小，但同一個事實有兩個手動維護的地方（AGENTS §9），而且 hitbox-data 遲早還是要讓 Match 讀 FBX。

### `v8-product-anim-backward`：往後走的判斷用什麼 sin／cos

- (a) **和移動一樣用 `std::sin`／`std::cos`**（`PlanarMovement.cpp:17`）。（建議）
  - 權威的移動本來就用它，跨平台的 golden 也本來就只收零角度的情境（sin 0、cos 0 在每個平台都精確）。不會多出新的決定性限制。
- (b) 改用多項式或整數的等價式，讓含角度的情境也能跨平台。
  - B3 已經量到取樣在平台間不一致，含動畫命中的情境本來就只做同平台比對，所以這裡的收益很小。

### `v8-product-anim-epoch-rule`：換 movement epoch 時

- (a) **照舊 Client 的做法整個重設**：相位 0、`move_weight` 0、跳躍階段由 `grounded` 決定。（建議）
  - E1 可以完全對照；換 epoch（Resync）很少發生。代價是那個 Tick 腳步會跳一下，但舊 Client 本來就這樣。
- (b) 保留相位與權重，只不把那個 Tick 的位移算進去。
  - 腳步不會跳，但這是新的規則：E1 在換 epoch 之後就無法對照，要另外定門檻。

### `v8-product-anim-xhigh-review`：狀態機與內插交給 xhigh 審查

計畫給這批的檔位是「high（狀態機局部 xhigh）」。

- (a) **A1 的狀態機與 A4 的內插，各交給一次 `xhigh-reviewer` 子 agent**（只讀，Opus 5.5／xhigh）。（建議）
  - 這兩處是「換階段的那個 Tick」「相位溢位」這類不容易察覺的錯。其他部分照 high。
- (b) 不另外審查，靠 E1 的對照與 E2 的單元測試。

### `v8-product-anim-presentation-invariants`：附錄 A 的文字

核准附錄 A 的 Design Invariants 與 Replacement Criteria 的文字（protocol-v9 §7 第 4 項：`PlayerPresentation` 目前沒有這一節）。

## 範圍的限界

- 判定還不用動畫狀態，所以「畫面上的姿勢＝被判定的姿勢」要到 `v8-product-hitbox-shot` 才成立。
- E1 比的是合成路線，不是實際對局的紀錄。實際對局的觀感，在 `v8-product-hitbox-acceptance` 的 L2 試玩裡看。
- E3 只在這台 Intel Mac 上比。跨平台靠零角度的 golden 子集與 L1。

## 附錄 A：`player-presentation.zh-Hant.md` 的 Design Invariants（草稿）

1. 遠端玩家的步伐相位、移動權重、jog 權重、跳躍階段與它的零點，只來自 snapshot 的 `animation`（Match 的權威）。Client 不由位移、速度或呈現的時間累積它們。
2. Client 對它們只做 protocol-v9 §4.4 的內插（`InterpolatePlayerAnimation`，和 Match 的回溯共用），以及 holding 時的凍結。
3. 其他影響姿勢的輸入（瞄準、蹲姿、死亡、射擊、換彈、受擊、idle）都是 snapshot 權威欄位與呈現時間的純函式。同一組 snapshot 在同一個呈現 Tick，不論 frame rate 都得到同一組姿勢輸入。
4. 呈現的狀態不回流到判定或任何權威狀態。
5. **Replacement Criteria：**
   - 為了讓畫面看起來對，Client 必須自己修正或推測權威的動畫狀態（例如漏收時另外外插相位）；
   - 或者每加一種動作，都要同時改 Match 的狀態機與 Client 的呈現規則才能一致。

   出現其中一種時，停下來提出「改契約（加欄位或改內插規則）還是重建呈現層」，交給你決定。

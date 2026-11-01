# v8-product-hit-presentation 的事前宣告

更新：2026-11-01。Owner：`object_fps_pvp`。**狀態：草稿，等你決定 4 項與模型表**。計畫：[hitbox-plan](hitbox-plan.md) 第 8 節（「受擊動畫依部位、驗收工具依部位推算傷害」）。契約：[protocol-v9](../../protocol-v9.zh-Hant.md) §4.6、§7（`pv9-upper-body-in-hitbox` (a)）。估時 6～10 h（計畫估 3～5 h，見「為什麼比計畫大」；超過 15 h 停下回報）。

## 這批做什麼

- **受擊動畫依部位**：被打中頭時，遠端看到的是頭部受擊的動作，打中軀幹或腿時是胸口受擊。
- **GUI 驗收工具**：照 `v8-product-real-match-aim` 的做法改成瞄軀幹，「每發 25000、第 4 發致死」的預先宣告照舊。

不在這批：
- 射手端的爆頭提示（準心、音效）：沒有排進計畫，要的話另外決定。
- 形狀與手感：`v8-product-hitbox-acceptance` 的 L2 試玩。

## 開工前查到的事實（2026-11-01）

1. **受擊動作是判定姿勢的一部分。** pv9 核准 `pv9-upper-body-in-hitbox` (a)：上半身的射擊、換彈、受擊 clip 都進判定姿勢（「畫面＝判定」）。
   - 所以如果只改 Client 播的 clip，畫面就和判定不同。要依部位換 clip，Match 的姿勢模型也得一起換。
   - 換 clip 也會改變 `AnimationContentDigest`（D3：15 個 clip 依固定順序都在 digest 裡）。新舊 Match 與 Client 混用時，會在 Welcome 被擋下，這是 D3 本來的用途。
2. **部位已經在兩端。** `CombatState.lastHitPart`（wire 的 `last_hit_part`，pv9 之前就有）由 Match 寫入，snapshot 帶給 Client。**不需要改 wire、不升版本。**
3. **動畫檔只有兩個受擊 clip。** `UAL1_Standard.fbx` 有 `Hit_Chest`（現在用的 `hit`）和 `Hit_Head`，沒有腿部受擊。
4. **姿勢的程式**：`PlayerPose.cpp:98` 讀 `hit` clip；`:456-461` 依最近一次受傷的時間播受擊；`:569` 選上半身的 clip。frame 裡目前沒有部位。
5. **GUI 驗收工具假設 Body 傷害**：
   - `combat_gui_evidence.py:25`、`:223`：每發 25000、4 發致死的 17 格排程（`combat_latency.hpp`）。
   - `run_native_window.py:418-423`：hp 路徑 75000→50000→25000→0；瞄準 `aim(yaw, pitch=0.)`（`:374`），出生點上 yaw 0 正對目標，現在會打中頭。
   - 這些都要螢幕，不在 CI。

## 為什麼比計畫大

計畫估 3～5 h 時，把受擊動畫當成只改呈現。事實第 1 點讓它變成 Match 與 Client 兩端都要改，加上 digest 改變與對照測試。GUI 工具的瞄準也比無頭的 probe 多一步：要把瞄準點交給滑鼠注入。

## 分成兩個子批

```text
v8-product-hit-presentation
├── P1 v8-product-hit-clip-by-part   「上段｜受擊 clip 依部位：Match 的姿勢模型與 Client 一起換」
└── P2 v8-product-hit-gui-aim        「上段｜GUI 驗收工具瞄軀幹」
```

| 代號 | 估時 | 依賴 | 檢收點 |
|---|---|---|---|
| P1 | 4～6 h | 本宣告 | G1～G4 |
| P2 | 2～4 h | 本宣告（可和 P1 分開做） | G5、G6 |

做完後，和已完成的 `v8-product-real-match-acceptance-repair` 一起開一個 PR（`v8-product-real-match-pr` (a)）。

## 模型與檔位（navigator.md）

| 子批 | 段 | 根據 | 実装者 | 審査者 | 検証者 |
|---|---|---|---|---|---|
| P1 | 上段 | 改判定姿勢（Design Invariants「畫面＝判定」）與 `animation_digest` 的內容（資料契約） | Opus 5.5／high | Opus 5.5／xhigh（`xhigh-reviewer`，唯讀；看 clip 選擇的決定性與兩端是否一致） | Opus 5.5／high（照實装者；只有改到判定時才需要） |
| P2 | 上段 | 修正回歸（#128 讓 GUI 驗收打中頭） | Opus 5.5／high | Opus 5.5／high | Opus 5.5／high（照實装者） |

- 理由：兩批都屬上段，不用 Sonnet 以下。審查者和實装者同一個模型是規則允許的，盲點不重疊的要求只針對敵對檢查；這批不跑 ultracode，沒有敵對檢查。
- 想要更強的審查，P1 的審查者可以改用 Fable 5.1（比主會話強，要你同意）。

## P1：受擊 clip 依部位（決定 `v8-product-hit-clip-by-part`、`v8-product-hit-leg-clip`）

- `animations.animset.json` 加一個 clip `hit_head`（`Armature|Hit_Head`）。`presentation.json` 的 `hit_seconds` 兩個受擊 clip 共用。
- frame 多一個欄位：最近一次受傷的部位，由 `PlayerFrameFromAuthority` 從 `CombatState.lastHitPart` 取。
- 姿勢選擇：頭 → `hit_head`；軀幹、腿 → `hit`（`Hit_Chest`）。Match 的姿勢模型與 Client 的呈現走同一個函式（S1、D2 的共用），所以兩端自動一致。
- `AnimationContentDigest` 的 clip 列表多一個 `hit_head`，位置固定在最後。

### 判定

- **G1 選對 clip**：同一個 frame，部位是頭時上半身播 `hit_head`，軀幹、腿時播 `hit`；沒有受過傷（部位未定）時和現在相同。
- **G2 兩端一致**：S2 的「every kind of input」測試加上「頭部受擊」與「腿部受擊」兩種輸入。姿勢模型的形狀，和 Client 算出的姿勢逐位元相同。
- **G3 digest**：
  - Client 與 Match 兩條路徑的 digest 仍然相同。
  - 拿掉 `hit_head` 的 key 或交換順序，digest 會改變。
  - 新的值四平台一致：本機先記值，pv9 之後第一個 PR 的 L1 比對，不同就停下回報（H6 的作法）。
- **G4 預設路徑不變**：
  - 兩樹比對（合成模型，scale 1、3，S2 的 42 個情境）0 差分。合成模型不看姿勢，權威 digest 與 golden 都不該變。
  - `hit_part_cross` 仍然通過。

## P2：GUI 驗收工具瞄軀幹（決定 `v8-product-hit-gui-aim`）

- `combat_latency.hpp`（GUI probe 的 17 格排程）：開火前用 `hit_aim.hpp` 算出目標軀幹的 pitch，經 SDL 滑鼠注入轉過去。
- `run_native_window.py`：GUI probe 在它的狀態檔裡寫出「目標軀幹的 yaw／pitch」，`aim()` 改成轉向它。
- 分析器（`combat_gui_evidence.py`、`run_native_window.py` 的 hp 路徑）都不動。

### 判定（螢幕上跑，Intel Mac，各一次）

- **G5** `run_gameplay_gui.py` 預設案例通過（`combat_gui_evidence` 的 17 格、hp 照每發 25000）。
- **G6** `run_native_window.py` 通過（hp 路徑 75000→50000→25000→0）。
- 只在時間檢查失敗時可重跑一次，兩次都記錄；傷害、生命相關的錯誤不重跑，直接停下回報。

## 會改判定的既有測試

| # | 測試 | 改法 | 寬鬆面 |
|---|---|---|---|
| W1 | `PlayerAnimationDigestTests` 的 clip 列表與「改一項就變」的清單 | 加上 `hit_head` | 無（多守一項） |
| W2 | `PlayerHitModelTests`「every kind of input」 | 加兩種輸入 | 無（多比兩種） |
| W3 | 依 clip 數目寫死 15 的地方（若有） | 改成 16 | 無 |

開工後如果發現本表以外的測試要改判定，停下回報。

## 突變

- 部位被忽略（永遠播 `hit`）：G1 失敗。
- Match 和 Client 用不同的選擇（只改其中一端）：G2 失敗。
- digest 漏掉 `hit_head`：G3 失敗。
- GUI 瞄準改回 pitch 0：G5 失敗在傷害（手動，GUI runner 不在 CTest）。

## 等你決定

### `v8-product-hit-clip-by-part`：受擊動畫依部位怎麼做

- (a) Match 的姿勢模型和 Client 一起換 clip（建議）。
  - 理由：守住「畫面＝判定」（`pv9-upper-body-in-hitbox` (a)）；部位已在兩端，不改 wire。
  - 代價：判定姿勢改變（頭部受擊的姿勢不同）、digest 改變。
- (b) 只改 Client 播的 clip，Match 仍用胸口受擊判定。
  - 改動小，但畫面和判定不同，違反已核准的設計，要當 Architecture Delta 另行同意。
- (c) 這批不做受擊動畫，只做 P2。
  - 最快；受擊動畫留到有腿部 clip 時一起做。

### `v8-product-hit-leg-clip`：腿部受擊用什麼

- (a) 先用胸口受擊（`Hit_Chest`）（建議）。
  - 理由：現有的動畫檔沒有腿部受擊；之後有了 clip，只要改內容和 digest。
- (b) 先找一個腿部受擊的 clip 再做。
  - 要決定資產來源（和 `v8-product-sfx` 的資產問題同類），這批會延後。

### `v8-product-hit-gui-aim`：GUI 驗收工具怎麼處理

- (a) 和無頭的 probe 一樣瞄軀幹，分析器不動（建議）。
  - 理由：和 `v8-product-real-match-aim` (a) 一致，預先宣告的排程照舊。
- (b) 分析器改成依部位推算傷害與致死。
  - 計畫原本這樣寫，但 4 發致死的排程會依結果變動，屬於寬鬆面，要做 §14.4 的揭露與核准。

### 模型表

- 照上面的表（兩批都 Opus 5.5，P1 的審查用 xhigh）（建議）。
- 或 P1 的審查者改用 Fable 5.1。

## 範圍的限界

- G5、G6 要螢幕，只在 Intel Mac 跑；不在 CI。
- 受擊動畫的觀感（頭部受擊看起來對不對）要 L2 試玩看，歸 `v8-product-hitbox-acceptance`。
- K2 試行 1 的時間檢查失敗原因未分清（`v8-product-real-match-acceptance-repair` 的限界）。GUI runner 也有類似的時間檢查，再出現時照那批的作法記錄。
- CI 只跑 `cpu|shader`：G1～G4 的單元測試在 CI；`hit_part_cross`、G5、G6 只在本機。

# PvP v8：v8-product-anim-authority-domain（A1：Match 的整數動畫狀態機）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5（狀態機交一次 xhigh 審查）。估時 8～12 h（含 (a-1) 多出的 2～3 h），實際約 6 h。

收支：一致（E1 試行 2：12 條路線 × 600 Tick＝7200 個 Tick 全部比對，判定為不同的 0 個，容許的 Client 浮點延遲 19 個＝事前宣告的 19 個）

**狀態：完成。** 文件決定 `v8-product-anim-phase-definition` 選 (a)：protocol-v9 §4.3 與宣告的表改成實作的定義。 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-anim-authority-declaration.md)的 A1。途中停了兩次（AGENTS §14），經過見下。

## 做了什麼

- **狀態機**（`match_domain`）：`PlayerAnimation.hpp/.cpp`。
  - `PlayerAnimation`：protocol-v9 §4.3 的 8 個欄位。`JumpPhase` 的 0 無效。
  - `PlayerAnimationSource` → `MakePlayerAnimationRules`：「clip 長度→一圈距離」與「秒→Tick」唯一的推導。
  - `AdvancePlayerAnimation`：每位玩家、每個權威 Tick 推進一次；只用整數與前後兩個 `PlayerState`。
- **`player_content` 程式庫**（`v8-product-anim-rules-source-texture-free` (a-1)）：
  - `CharacterPresentationDefinition`、`AnimationSetDefinition` 從 `app_support` 搬過來。
  - 新的 `PlayerAnimationContent`：`ReadPlayerAnimationSource` 是規則輸入唯一的讀者；`LoadPlayerAnimationRules` 是 Match 的路徑。
  - `LoadCharacterAnimationDefinition` 只載入模型與動畫集，不綁材質、不載配件，所以不解碼任何貼圖。
  - 依賴：`match_domain`、`GYO::Render`（核心，沒有 SDL）、`GYO::Model`。`app_support` 改成連結它。Match 執行檔在 A2 才連結。
- **Client**：`PlayerPresentationDefinition` 多一個 `animationRules`，由同一個讀者與推導產生。呈現的行為不變（A4 才改）。
- **測試**：
  - `PlayerAnimationTests.cpp`（cpu，18 個 TEST_CASE）：E2，加上 xhigh 審查後補的 9 個。
  - `PlayerAnimationEquivalenceTests.cpp`（presentation_cpu，3 個 TEST_CASE）：
    - E1，新舊狀態機對照；
    - E1 跳躍判定函式的單元測試；
    - (a-1) 的對齊測試：讓角色綁上貼圖、而且不登記貼圖解碼器，Match 的路徑照樣載入成功，完整路徑則失敗；兩條路徑算出的規則相同。

## 判定

- **E1：試行 1 不成立 → 判定修正核准 → 試行 2 通過。**
  - 試行 1（證據：使用者的 Intel Mac，主 checkout 的 `build/evidence/v8-anim-authority/anim-authority-e1-run1.txt`）：相位、`move_weight`、`jog_weight` 都在門檻內，最壞分別是門檻的 0.59、0.67、0.996 倍。跳躍有 19 個 Tick 不同，全部出在階段開始後剛好第 6 個 Tick：舊 Client 的 double 比較 `t/60 − s/60 ≥ 0.1` 算出 0.0999999999999996，所以晚一個 Tick 才換。
  - 判定修正 `v8-product-anim-authority-e1-jump`：照 §14.4，核准之前先做了兩件事，之後經使用者核准（「核准並重跑」）。
    - 突變：只跑判定函式的單元測試，4／4 killed。第一輪是 3／4，補上「Match 提早一個 Tick」的情況之後才全部 killed。
    - Sonnet 5.5 審查：1～5 成立，「兩種轉換」的用詞已照它的指正修改。
  - 試行 2（證據：使用者的 Intel Mac，主 checkout 的 `build/evidence/v8-anim-authority/anim-authority-e1-run2.txt`）：判定為不同的 0 個；容許的延遲 Tick 恰好是試行 1 的 19 個（斷言通過）；三個量的最壞值和試行 1 相同。
  - 試行 3（證據：使用者的 Intel Mac，主 checkout 的 `build/evidence/v8-anim-authority/anim-authority-e1-run3.txt`）：xhigh 審查之後程式改過，所以用同一個判定再跑一次確認。結果和試行 2 完全相同，36038 個斷言全部通過。
- **E2：通過。**
  - 宣告列的規則各有測試：重設的五種原因、往後走、相位溢位、`move_weight` 的值、`jog_weight` 靜止時保留、跳躍四個階段與零點、死亡凍結、`sprint` 恆為 0、規則載入的拒絕。
  - xhigh 審查之後，再加上找到的缺陷與漏洞的測試（下一節）。

## xhigh 審查（`v8-product-anim-xhigh-review` (a)）與修正（AGENTS §14.3）

交給 `xhigh-reviewer`（Opus 5.5／xhigh，只讀）。它的結論是：照正確的方式呼叫時，輸出不會違反 §4.5。它找到下面這些問題，我每一個都先寫成會失敗的測試，確認是紅的，再修程式。

| # | 問題 | 修正 |
|---|---|---|
| C1 | 重設時，`jog_weight` 用的是跨過邊界的位移（死亡點→重生點、兩位玩家之間）：在別處重生、站著不動，也會得到 1000。舊 Client 的時間軸不會跨區段算速度（`SnapshotTimeline.hpp:110-118`），契約寫的是「由當下速度重算」 | 換人、換命、生死改變、換 epoch：速度當 0。只有同一區段裡的瞬移，才用這個 Tick 的位移（和舊時間軸相同） |
| C2 | transition ≥ 1001 Tick 時，步數還原 `(w×span+999)/1000` 會錯，`move_weight` 永遠是 0；跨度也沒有上界 | 載入時限制：transition 1～1000、跳躍 1～3600 Tick |
| C3 | 位置不是有限值時，重設路徑會把 NaN 轉成 uint32（UB；在某些 CPU 上會超過 1000）。規則的乘積可能是 0 或 inf | 位移不是有限值時，當作靜止重設。一圈距離與瞬移距離要是有限值，而且至少 1 µm |
| S1 | 函式不檢查 `previous` 和 `before` 是不是同一條命。A2 如果在重生的那個 Tick 漏推進一次，就會把凍結的「空中」帶進新命，違反 `grounded` 的交叉檢查，讓 Gateway 失效 | `previous` 的命 ≠ `before` 的命時重設 |
| S2 | Tick 倒退時（例如 `PvpMatch::Reset`），`tick − 零點` 會下溢，輸出零點 > tick | 零點 > tick 時重設 |
| S3 | protocol-v9 §4.3 寫相位＝floor(累積圈數×65536)，實作是每個 Tick 先四捨五入再累加。宣告的表寫一圈距離用「jog_weight」，實作用的是這個 Tick 速度的精確比例 | 程式不改，文件要不要改見「等你決定」 |

補上的漏洞測試，在修正前就通過，目的是讓突變能被抓到：
- 走下台階的那個 Tick，垂直速度剛好是 0（`Movement.cpp`）→ 直接 Airborne；
- 三個跨度各不相同（4／8／12 Tick）；
- 低於靜止門檻的微小位移、和 yaw 垂直的位移；
- 只有生死狀態改變時的重設。

## 經過（AGENTS §14）

1. **開工前停下：** 決定 (a) 的前提被我說成「搬過去的 CPU 載入器會讀貼圖，Match 會依賴 SDL」。使用者改選 (a-1)。
   - **更正（之後查明）：** 現在的玩家角色與武器都沒有綁貼圖，只有頭髮配件有。所以照原案 (a) 搬過去，用現在的內容並不會讀到貼圖；只有內容加上材質覆寫或配件時才會。
   - (a-1) 的保證不依賴內容，所以仍然比原案嚴。這是第一輪突變 `anim-match-reads-textures` 存活才發現的：用現在的內容，完整路徑和 Match 的路徑沒有差別。對齊測試因此改成讓角色真的綁上一張貼圖。
2. **E1 試行 1 不成立：** 經過見「判定」。
3. 全部 CTest 平行跑時，`object_fps_pvp.cpu` 與 `presentation_evidence` 逾時；當時主機負載 66，有其他會話在建置。序列重跑都通過（cpu 50 s、presentation_evidence 55 s），但都接近 60 s 的逾時上限。這不是受入判定的試行。

## 突變

**17／17 killed**（只在預期的訊息失敗時才算 killed），在未提交的工作樹上用 `--allow-dirty` 跑，`batch` 為 `v8-product-anim-authority-domain`。

- 第一輪 11 個，10／11：`anim-match-reads-textures` 存活。原因見「經過」第 1 項，修正是讓對齊測試真的綁上貼圖。
- xhigh 審查的修正之後，因為改寫了那幾行，`anim-epoch-not-reset`、`anim-teleport-walked`、`anim-span-not-whole` 的 find 跟著更新。另外每一項修正各加一個突變：
  - `anim-reset-carries-speed`（C1）、`anim-transition-unbounded`（C2）、`anim-previous-life-unpaired`（S1）、`anim-zero-ahead-accepted`（S2）；
  - `anim-walkoff-starts`、`anim-spans-swapped`（漏洞）。
- 17 個全部重跑，17／17。
- D184 相關的既有突變：0 個。find 沒有落在這批改到的行上，殺它的既有測試也沒有被改。
- 另外，E1 跳躍判定函式的 4 個突變沒有登記進清單，是只跑那個單元測試的一次性檢查，4／4（`judge_mutants.py`，見「判定」）。

## 其他測試

- `test` preset 的 CTest 111／111、`go test ./...` 都通過。

## 範圍的限界

- E1 的 12 條路線中途都沒有重生、換人、換 epoch 或瞬移，所以 E1 只比到第 1 個 Tick 的重設。中途的重設由 E2 的單元測試守住（C1 正是 E1 看不到的）。
- E1 比的是合成路線，不是實際對局。
- 只在這台 Intel Mac 上跑過。跨平台由 L1 檢查（`pv9-single-merge`）。

## 等你決定

- `v8-product-anim-phase-definition`：protocol-v9 §4.3 的相位定義，以及宣告表裡「一圈距離」的寫法，要不要改成實作的樣子？
  - (a) 改成「每個 Tick Δq16＝round(有號位移 ÷ 一圈距離 × 65536)，一圈距離用這個 Tick 速度的精確比例（不是量化後的 `jog_weight`），累加 mod 2^32」。（建議）只有 Match 會算相位，Client 只讀，所以這只是讓文件和唯一的實作一致。
  - (b) 改程式，照現在的文字：累積 double 再 floor，一圈距離用 `jog_weight`。這等於 Match 要多留一個高精度狀態，違反 §4.4 的「量化值就是權威值」；用量化的比例，E1 的相位門檻也不成立（推導見宣告）。

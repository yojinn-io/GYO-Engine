# PvP v8：v8-product-hitbox-shot-judge（S2：依部位判定）

日期：2026-11-01（夜間，使用者睡眠中進行）。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5；同距規則、同 Tick 的放置與暫時動畫狀態交 xhigh 審查（宣告指定）。估時 5～7 h，實際約 3 h。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-shot-declaration.md)的 S2，5 項決定都照 (a)。Match 執行檔還沒有接上姿勢模型（S3）。

## 做了什麼

- **介面**（`match_domain` 的新標頭 `PlayerHitModel.hpp`，`v8-product-hitbox-shot-boundary` (a)）：
  - `PlayerWorldHitShape`：部位與體積。體積是垂直膠囊或任意方向的膠囊；球就是兩端相同的膠囊。
  - `PlayerHitInput`：目標在這個 Tick 的權威值（狀態、戰鬥狀態、動畫、Tick），以及場地與蹲姿規則。
  - `PlayerHitModel::Shapes`：放出目標的形狀。
  - `SyntheticPlayerHitModel()`：一顆站／蹲高度的垂直膠囊，部位 Body，也就是原本的判定。`PvpMatch` 與 `MatchRuntimeHost` 的預設（`v8-product-hitbox-test-model` (a)）。
- **`QueryShot`**（`ShotQuery.cpp`）：改成吃「目標＋形狀」。
  - 每個目標取射線上最近的形狀；同一個目標兩個形狀同距時，頭 > 軀幹 > 腿（`v8-product-hitbox-tie` (a)），與形狀的順序無關。
  - 目標之間、牆與目標之間的比較，和原本逐字相同。
  - 原本吃 `PlayerState` 的版本保留，內部用合成模型的形狀呼叫新版本。
  - `ShotHit` 多一欄 `part`。為了避開 `Combat.hpp` ↔ `ShotQuery.hpp` 的循環 include，`HitPart` 與形狀在這裡只做前置宣告。
- **`PvpMatch`**：
  - 建構多一個參數 `hitModel`（預設合成模型；null 拋出例外）。
  - `TickShapes()`：這個 Tick 第一次判定射擊時，替每位活著的玩家放一次形狀，之後整個 Tick 共用（`v8-product-hitbox-tick-pose` (a)）。下一個 Tick 重新放。
  - 放置用的動畫：用和 Tick 結尾相同的 `AdvancePlayerAnimation` 算出這個 Tick 的值，但不寫回。
  - 放置用的戰鬥狀態：`ResolveActions` 開始時保留的副本（xhigh 審查的發現 1，見下）。
  - 決定與受擊紀錄的部位改成 `hit.part`（原本寫死 Body）。
- **姿勢模型**（`player_content` 的新檔 `PlayerHitModel.cpp`，`MakePosedPlayerHitModel`）：依序呼叫 Client 的函式：
  - `PlayerFrameFromAuthority`（S1）→ `AdvancePlayerLocomotion`（新的 state，不帶歷史）→ `ResolvePlayerActions` → `SamplePlayerBodyPose`；
  - 形狀放到骨頭上（球：`Head` 的全域變換乘 `offset`；膠囊：兩根骨頭的原點），再用 `PlayerBodyPointToWorld`（S1）換到世界座標。
- **依賴方向不變**：`player_content` → `match_domain`。新增的只有介面，沒有新的依賴邊；`Engine` 不必改（任意方向膠囊的射線相交本來就有）。

## 判定

- **J2：通過。** 「PvP posed shapes judge the head, the body and the legs」：
  - 四種姿勢：idle（yaw 0）、idle（yaw 1.3）、蹲下、跑步中（相位 1/4 圈、兩個權重 1000）。
  - 瞄準頭球中心 → Head、50000；軀幹膠囊的中點 → Body、25000；左大腿的中點 → Leg、18750。瞄準點取自判定用的同一個姿勢。
  - 「…let a shot through the arm and between the legs」：站姿 idle 時，右手（握槍、在胸前約 0.3 m）不在任何形狀裡，從右側射向它會穿過去；從正面射向兩膝之間也會穿過。
  - 「…follow the target's place and facing」：同一個姿勢換位置與 yaw，形狀跟著平移與轉動。
- **J3：通過。** `ShotQueryTests.cpp` 新增 4 個：
  - 取近的形狀（兩種順序）、射線外的形狀不算、沒有形狀在射線上就是 Miss；
  - 同一個目標同距：頭 > 軀幹 > 腿，三對 × 兩種順序；眼睛在形狀裡（距離 0）也照這個規則；
  - 兩個目標同距：id 小的勝，即使它的部位比較低（部位只在同一個目標內比）；牆在前面時是牆；射手自己的形狀不算；
  - 合成模型＝站／蹲的垂直膠囊、Body。
- **J4：通過。** 「PvP a tick places every living target's shapes once at its first shot」（記錄呼叫的模型包住合成模型）：
  - 沒有射擊的 Tick 不放置；三發同 Tick 只放一次（4 位玩家各 1 次）；
  - 目標移開後的下一個 Tick 重新放置，在原地的射擊打不到它；
  - 同 Tick 前一發打死目標，後面的射擊穿過；死掉的玩家不放置。
- **J5：通過。** `compare_authority_trees.py`，base＝`d7708c71`（S1）、branch＝本批，同一台 Intel Mac：scale 1、3 各 42 個情境 **0 差分**。golden 不重產。（發現 1 的修正之後重跑，見「其他測試」。）
- **J8：通過。** 「PvP shots are judged with the animation the tick's snapshot carries」：
  - 120 個 Tick，玩家 2 走、跳、落地、蹲；三位射手錯開時間開槍。
  - 每次放置用的暫時動畫，都和 Tick 結尾寫進 snapshot 的值完全相同：共比對 144 次。
  - 覆蓋：四個跳躍階段（Grounded、Start、Airborne、Land）都出現；6 次放置剛好落在跳躍階段的第一個 Tick；有蹲下時的放置。
- **開工時的確認**（宣告要求）：動作在第 0 秒的形狀和不播相比，射擊 0 m、換彈 0 m、受擊 6×10⁻⁹ m（浮點的雜訊）。寫成斷言：「PvP an action at its first instant barely moves the shapes」。

## xhigh 審查

審查者 `xhigh-reviewer`（Opus 5.5，xhigh，唯讀）。問題 1（同距）、2（同 Tick 放置）、3（暫時動畫）、4（合成路徑逐位元等價）的核心都「沒有發現」，並列出它檢查了什麼。指出的問題：

1. **（should-fix，已修，先紅後綠）放置讀到的戰鬥狀態，含同一個 Tick 稍早決定的換彈。**
   - 形狀在第一發射擊時才放置，而照 (A, PlayerId, ActionId) 的順序，排在前面的換彈已經寫進戰鬥狀態。判定姿勢因此取決於同一個 Tick 內的處理順序，違反 `v8-product-hitbox-tick-pose` (a) 的理由。
   - 照 §14.3：先寫「PvP a tick places its shapes with the combat state from before its actions」，確認是紅的（放置看到 `reloadStartTick`＝這個 Tick）；再改成在 `ResolveActions` 開始時保留每位玩家戰鬥狀態的副本，放置時讀它，轉綠。
2. **（should-fix，已修）`MakePosedPlayerHitModel` 的註解說角色不符會拋出例外，實作沒有。** 形狀只存骨頭的索引，查不出它是對哪個骨架讀的。註解改成實際的契約：只檢查索引在骨架之內；形狀與定義要來自同一個內容根。
3. **測試的缺口（都已補）：**
   - 3a：姿勢模型的輸入接線沒有守衛（拿掉戰鬥狀態、動畫或 Tick，都可能活過測試）。新增「PvP posed shapes are the Client's pose for every kind of input」：9 種輸入（跑步、空中、落地、蹲著移動、受擊 5 Tick 前、換彈中、射擊 3 Tick 前、抬頭、時間不同），每個形狀的端點都和 Client 路徑獨立算出的值逐位元相同，而且每一種都讓形狀離開 idle（最小 2 mm，換彈中）。
   - 3b：J8 沒有覆蓋起跳與落地。射手改成錯開時間開槍，斷言四個階段都出現、至少 2 次放置落在階段的第一個 Tick（實際 6 次）。
   - 3c：跨目標同距的測試分不出「id 小的勝」和「部位高的勝」。對調部位：id 小的那個是腿，結果仍是它。
   - 3d：Match 寫進決定的部位沒有守衛（合成模型一律是 Body）。新增「PvP the Match writes the hit shape's part into the decision and the hit record」：用只換部位的模型，頭 50000、腿 18750、軀幹 25000，決定與受擊紀錄的部位都對。
   - 3e：刪掉 `shapes_.clear()` 可能活下來。J4 加上「目標移開後，原地的射擊打不到」。
4. **（note）代價的大小比宣告所估的大。** 見「範圍的限界」第 1 條。

## 突變

**26／26 killed**（只在預期的訊息失敗時才算 killed），在未提交的工作樹上用 `--allow-dirty` 跑。

- 本批新增 16 個（`batch` 為 `v8-product-hitbox-shot-judge`）：
  - `QueryShot`：一律判 Body、取最遠的形狀、同距規則反過來、合成模型判成別的部位；
  - `PvpMatch`：用 J−1 的動畫、每發都重新放置、保留前一個 Tick 的形狀、讀已被這個 Tick 的動作改過的戰鬥狀態、前一發打死的目標仍可被打中、決定一律寫 Body；
  - 姿勢模型：頭球判成腿、不依 yaw 轉動、膠囊兩端是同一根骨頭、忽略戰鬥狀態、忽略動畫、在 Tick 0 取樣。
- D184 相關的既有突變 10 個，全部 killed：`09-hit-count`、`09-hit-attacker`、`16-attacker-first`、`v71hf-match-head`、`v71hf-match-last-hit-part-unset`、`v71a1-rules-ignored`（這三個的 find 跟著新的那一行改，抓的東西不變），以及新測試旁邊的 `a2-check-*` 4 個。
- 第一輪 24／26，兩個是 `wrong_failure`（測試失敗了，但不是預期的訊息）：
  1. `s2-synthetic-leg`：合成模型改判 Leg 時，有一個既有測試不會結束，跑到逾時，所以輸出裡沒有預期的訊息。
     - 卡住之前最後印出的是 `CombatHostTests` 的「PvP combat host only exposes final catch-up publications as valid references」；我推測是某個測試在等目標死亡，用 Leg 的傷害就等不到。沒有再追（不在範圍），留給 S3：真的 Match 會產生 Leg。
     - 突變改成判 Head（`s2-synthetic-head`），一樣檢查「合成模型必須是 Body」，killed。
  2. `s2-posed-capsule-one-bone`：被 3a 新增的「every kind of input」抓到，可是登記預期訊息時還沒有那個測試。預期改成它，killed。
- 手動檢查突變時，有一次測試執行檔卡住、被移到背景，那段時間原始碼停在突變後的狀態；停掉之後已還原，並用 `grep` 確認。

## 其他測試

- `test` preset 的 CTest 111／111 通過（`-j3`，完整輸出另存）。
- 發現 1 修正之後，J5 重跑：scale 1、3 各 42 個情境 0 差分。
- **worker 又偶發失敗一次**（`-j4` 全跑時），單獨重跑通過。這是第三次（A3、D3 各一次）。這一次的失敗輸出被重跑蓋掉，沒有留下。之後的全跑都把完整輸出存檔。原因分不出來（實作、測試本身還是環境），L1 時一起看。

## 範圍的限界

- **動作在判定的 Tick 重新開始時，判定的形狀和 snapshot J 不同。**
  - 宣告估的代價是「snapshot J 會顯示這個 Tick 剛開始的動作，判定的姿勢沒有」，並在開工時確認第 0 秒和不播幾乎相同（上面的 0 m／6×10⁻⁹ m）。
  - 審查指出，常見的情況是「同一個動作重新開始」：目標在 20 Tick 內再次中彈時，snapshot J 顯示受擊的第 0 秒，判定的形狀卻是前一次受擊播到一半。量測（「PvP an action restarted in the judged tick against the run it replaces」，1～40 Tick 前開始的那一次對第 0 秒）：**受擊最多差 0.14 m，射擊 9 mm**。沒有宣告門檻，只記錄。
  - 這個差異在 (a)、(b) 兩個選項下都存在：這個 Tick 的第一發，在 (b) 也是用打中之前的姿勢判定。射手看到的是更早的畫面（內插延遲），播到一半的受擊本來就比 snapshot J 的第 0 秒更接近射手看到的姿勢。所以它不改變已核准的選擇；畫面上的實際感受在 L2 試玩時看。
- 預設的合成模型不是產品的判定；產品路徑由 J2、3a、3d 守住。Match 執行檔有沒有接上姿勢模型，要到 S3（J6）才有守衛。
- J2～J4、3a 只在同一個平台上成立；跨平台的姿勢本來就不同（B3）。
- `MakePosedPlayerHitModel` 查不出形狀是對哪個骨架讀的（見審查第 2 條）；S3 的 Match 從同一個內容根載入兩者。

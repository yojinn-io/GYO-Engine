# PvP v8：v8-product-hitbox-data-shapes（D1：三部位的命中形狀）

日期：2026-10-31～11-01。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 3～4 h，實際約 2.5 h。

**狀態：完成。** H2 的判定修正經使用者核准（`v8-product-hitbox-data-h2-legs`，2026-11-01「核准並 commit」）。

## 做了什麼

- **形狀檔**：`assets/object_fps_pvp/players/hitbox.json`（7 個形狀），在 asset catalog 登錄為 `object_fps_pvp.player.hitbox`。契約寫在 [hitbox-shapes](../object_fps_pvp/hitbox-shapes.zh-Hant.md)。
- **嚴格的讀者**（`player_content`，`PlayerHitbox.hpp/.cpp`）：
  - 鍵不能多也不能少；
  - 三個部位都要有；
  - 骨頭名稱剛好出現一次，膠囊的兩根骨頭不同；
  - 半徑在 [0.01, 0.5]，`offset` 是三個有限值。
- **共用的資產載入**：`WithContentAssets` 只登記文字與 FBX 的載入器，動畫規則與形狀都用它。
- **Match**：啟動時以不讀貼圖的路徑載入形狀，內容不合就停止，並寫一行 `hitbox shapes=7` 的 log。這批還不判定。
- **頭球的 `offset`**：照宣告由 survey 的方法推出。
  - 推法：在 `HitboxSurvey.cpp` 暫時加一行印出 `headLocal`（頭部頂點重心換到 `Head` 骨頭的局部座標），跑一次後還原，沒有 commit。
  - 結果：站姿 (0.000786, 0.023344, −0.005024)，蹲姿相差 1e-6 以內，所以是剛性綁在骨頭上。

## 判定

- **H1：通過。**
  - 產品的檔案載入成功，每個形狀的部位、種類、骨頭（含腿的四段與骨盆的兩端）、半徑都符合；
  - 不登記貼圖解碼器時，Match 的路徑也載入成功，結果和 Client 的相同；
  - 16 種錯誤寫法都被拒；缺少鍵、`offset` 只有兩個值時，拒絕的原因會指名那條規則。
- **H2：照修正後的判定通過。** 經過見下。
  - 所有形狀的頂端 ≤ 1.8 m；
  - 頭球中心在頸部以上，高度 1.5543 m，和 survey 的 1.554 相同；
  - 大腿的起點離 `pelvis` 關節 0.150／0.132 m，都 ≤ 0.179；
  - 膝與腳都在 `pelvis` 關節以下。

## 經過（AGENTS §14.1）

1. H2 的「腿的形狀在骨盆以下」不成立：大腿的起點（`thigh_l`／`thigh_r` 關節 y＝0.9108／0.9107）比 `pelvis` 關節（0.8962）高 1.5 cm。原因是判定把 `pelvis` 關節的原點當成骨盆的高度。停下回報，記在 HANDOFF。
2. 使用者選 (a)：大腿的起點要在骨盆膠囊的半徑以內，膝與腳要在 `pelvis` 關節以下。
3. 照 §14.4 做了兩件事：
   - **突變：** 腿綁到 spine、腿往上接到 spine_03、頭球綁到 neck，3 個都 killed。
   - **Sonnet 審查：** 跑了測試、並在暫存的副本上逐一套用綁錯的情況。結論是 2、4、5 成立；1、3 照原本的寫法不成立：新判定抓不到「腿綁錯成另一段腿」（左右接反、少了大腿、跳過膝蓋、兩條大腿都從左邊開始），我原本宣稱的「大腿從遠處開始也會被抓到」，在綁錯骨頭時並不成立。
4. **處理：**
   - H1 加上腿的四段與骨盆的骨頭名稱必須完全符合（收緊），對應的 4 個突變都 killed；
   - 1、3、4 的用詞改成實際的情況。
   - 修正後的 1～5 寫在宣告的「H2 的修正」一節。

## 突變

**16／16 killed**（`batch` 為 `v8-product-hitbox-data-shapes`，16 個），加上 D184 相關的既有突變 2 個，在未提交的工作樹上跑。

- 讀者的檢查 7 個。第一輪有 2 個存活：拿掉「缺少鍵」「`offset` 的長度」這兩條檢查後，之後讀取時 JSON 函式庫照樣丟出例外，結果一樣是被拒。改成檢查拒絕的原因必須指名那條規則，之後 killed。
- 綁錯骨頭的資料突變 7 個：H2 的 3 個，H1 骨頭名稱的 4 個。
- 既有的：`anim-match-reads-textures`、`anim-walk-reads-jog`。

## 其他測試

- `test` preset 的 CTest 111／111 通過。

## 範圍的限界

- 形狀的大小是否合適，要等 `v8-product-hitbox-shot` 之後的 L2 試玩才看得出來。
- 檔案位置：asset-layout 線（PR #125）的新標準預定把它放在 `characters/player/`。目前它和 `players/` 的另外 3 個定義檔放在一起，由那條線的 player-folder 批一起搬；asset id 不變。

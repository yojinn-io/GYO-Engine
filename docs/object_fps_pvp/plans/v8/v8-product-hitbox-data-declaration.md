# v8-product-hitbox-data 的事前宣告

更新：2026-10-31。Owner：`object_fps_pvp`。**狀態：已核准**（`v8-product-hitbox-data-declaration-approve`，3 項都照建議 (a)）。 契約：[protocol-v9](../../protocol-v9.zh-Hant.md) §4.6（定稿）。計畫：[hitbox-plan](hitbox-plan.md) 第 4 節 R1、R4，第 8 節。估時 12～16 h（計畫原本估 6～10 h，多出的部分見「為什麼比計畫大」；超過 24 h 停下回報）。

## 這批做什麼

判定要用的資料準備好，但**還不判定**。部位判定在 `v8-product-hitbox-shot`。

1. **三部位的形狀**：新的產品內 Data Contract 與嚴格的載入器，Match 啟動時載入。
2. **Match 端的姿勢**：Match 能用和 Client **同一個函式**，由權威狀態算出玩家的骨架姿勢（畫面上的姿勢＝被判定的姿勢）。
3. **`animation_digest`**：§4.6 的動畫內容校準。Ready 18／Welcome 20，Client 和自己載入的內容比對，不合就明確報錯，不進入對局。

## 為什麼比計畫大

- 計畫的 6～10 h 只算了「形狀契約與載入器、Match 載入骨架與 clip」。
- 載入的部分，A1 已經做完：`player_content` 不讀貼圖，就能載入模型與全部 clip。
- 多出來的有兩件：
  - **姿勢組合要共用**：Client 的 `SamplePlayerPresentationPose`、`ResolvePlayerActions` 在 `app_support` 裡，Match 不能連結；
  - **`animation_digest` 的 wire 變更**：protocol-v9 §4.6 把它指定在這一批。

## 分成三個子批

```text
v8-product-hitbox-data
├── D1 v8-product-hitbox-data-shapes   「high｜形狀契約、嚴格載入器、Match 載入」
├── D2 v8-product-hitbox-data-pose     「high｜姿勢組合搬到 player_content，Client 與 Match 共用」
└── D3 v8-product-hitbox-data-digest   「high｜animation_digest：計算、Ready／Welcome、Gateway、Client 比對、大小」
```

| 代號 | 估時 | 依賴 | 檢收點 |
|---|---|---|---|
| D1 | 3～4 h | 本宣告 | H1、H2 |
| D2 | 4～6 h | 本宣告 | H3、H4 |
| D3 | 5～6 h | D2（digest 的範圍和姿勢用到的內容相同） | H5～H8 |

## D1：形狀的 Data Contract

### 檔案（決定 `v8-product-hitbox-shape-format`）

`assets/object_fps_pvp/players/hitbox.json`（2026-11-01 照 asset-layout 線的標準搬到 `characters/player/hitbox.json`，asset id 不變），在 asset catalog 登錄為 `object_fps_pvp.player.hitbox`（text）。

```json
{
  "version": 1,
  "character_asset_id": "object_fps_pvp.player.character",
  "shapes": [
    {"part": "head", "kind": "sphere",  "bone": "Head", "offset": [0, 0.08, 0], "radius": 0.184},
    {"part": "body", "kind": "capsule", "from": "spine_01", "to": "spine_03", "radius": 0.181},
    {"part": "body", "kind": "capsule", "from": "pelvis",   "to": "spine_01", "radius": 0.179},
    {"part": "leg",  "kind": "capsule", "from": "thigh_l",  "to": "calf_l",   "radius": 0.15},
    {"part": "leg",  "kind": "capsule", "from": "calf_l",   "to": "foot_l",   "radius": 0.105},
    {"part": "leg",  "kind": "capsule", "from": "thigh_r",  "to": "calf_r",   "radius": 0.15},
    {"part": "leg",  "kind": "capsule", "from": "calf_r",   "to": "foot_r",   "radius": 0.105}
  ]
}
```

- **骨頭與半徑：** 照 hitbox-plan R4 與 hitbox-survey 的量測值。
  - 大腿取 0.14～0.164 的中間值 0.15，小腿取 0.102～0.113 的中間值 0.105。
  - 頭球的 `offset`（相對 `Head` 骨頭）在 D1 開工時，由 survey 的頭部頂點重心推出，寫進 dev_log。
- **單位與座標：** 公尺，模型的參考尺度乘上 `presentation` 的 `scale`，和呈現同一個縮放。這一點在 D1 的測試裡固定下來。
- **手臂不給形狀：** 射線穿過手臂，契約 §2。
- 這份契約寫成 `docs/object_fps_pvp/hitbox-shapes.zh-Hant.md`：欄位、單位、驗證規則、版本。

### 嚴格的載入器（`player_content`）

- `version` 必須是 1。未知的鍵、少了必要的鍵，都是錯誤。
- `part` ∈ {head, body, leg}，三種都至少有一個形狀；`kind` 是 sphere（`bone`）或 capsule（`from`、`to`）。
- 骨頭名稱必須在角色的骨架裡**剛好出現一次**；capsule 的 `from` ≠ `to`。
- 半徑是有限值，範圍 [0.01, 0.5] m；`offset` 是三個有限值。
- 不合就是載入錯誤（Runtime Error），Match 啟動失敗。

### H1：載入器

- 產品的檔案載入成功，形狀和上面列的一致。
- 下面每一種錯各自被拒：
  - 版本不是 1、多一個鍵、少一個鍵；
  - 少一個部位、不認識的 `part` 或 `kind`；
  - 骨頭不存在或名稱重複、`from` == `to`；
  - 半徑 0、0.6、NaN；`offset` 只有兩個值。
- Match 用不讀貼圖的路徑載入（和 A1 的對齊測試一樣，不登記貼圖解碼器）。

### H2：形狀在參考姿勢上的位置

- 在站姿的參考幀上，每個形狀的頂端（y＋半徑）≤ 1.8 m；頭球在頸部以上；腿的形狀在骨盆以下。
- 這是用 survey 的量法核對形狀放對了骨頭，不是判定的門檻。

### H2 的修正（`v8-product-hitbox-data-h2-legs`，2026-11-01 使用者選 (a)；修正後的 1～5 經使用者核准：「核准並 commit」）

D1 實作後，H2 的「腿的形狀在骨盆以下」不成立：大腿膠囊的上端（`thigh_l`／`thigh_r` 關節 y＝0.9108／0.9107 m）比 `pelvis` 關節（0.8962 m）高 1.5 cm。原因是這條判定把 `pelvis` 關節的原點當成骨盆的高度，而髖關節本來就在那附近。

新判定，照 AGENTS §14.4 揭露 1～5：

1. **守的性質**：腿的形狀綁在腿上，從骨盆往下長，沒有綁到軀幹或手臂上。H2 只守住「沒有綁到軀幹或手臂」與「從骨盆往下」；「綁在正確的那一段腿上」由 H1 的骨頭名稱檢查守住（見第 3、4 點）。
2. **寬鬆面**：大腿膠囊的上端，不再要求低於 `pelvis` 關節；改成要求它離 `pelvis` 關節不超過骨盆膠囊的半徑（0.179 m）。這一點最多可以高出 `pelvis` 關節 0.179 m。
3. **收緊面**：
   - 以 `thigh_` 命名的骨頭為起點時，必須在 `pelvis` 關節附近，舊判定沒有這個要求。不過這只在骨架或姿勢改變、讓大腿關節遠離骨盆時才會抓到；綁錯骨頭（例如起點換成 `calf_l`）會走「非大腿」的分支，不會被這一條抓到（Sonnet 審查）。
   - 膝與腳（各段的下端、小腿的上端）照舊要低於 `pelvis` 關節。
   - 審查之後 H1 另外加上：腿的四段骨頭必須依序剛好是 `thigh_l→calf_l`、`calf_l→foot_l`、`thigh_r→calf_r`、`calf_r→foot_r`，骨盆是 `pelvis→spine_01`。這是收緊。
4. **由誰守住**：
   - 腿綁到軀幹或手臂：由新判定抓到。突變 `hb-thigh-on-spine`（大腿從 `spine_02` 開始）、`hb-leg-up-the-body`（腿往上接到 `spine_03`）都 killed。
   - 腿綁錯成另一段腿（左右接反、少了大腿、跳過膝蓋、兩條大腿都從左邊開始）：新判定抓不到，改由 H1 的骨頭名稱檢查抓到，突變 `hb-leg-crossed`、`hb-thigh-missing`、`hb-knee-skipped`、`hb-thigh-swapped` 守住。
   - 頭的位置由「頭球中心 1.554 m（survey）」守住，突變 `hb-head-on-neck` killed。
5. **根據**：實測的關節高度（上面）；hitbox-plan R4 的形狀定義；hitbox-survey 的量法。

## D2：共用的姿勢組合（決定 `v8-product-hitbox-pose-sharing`）

- `ResolvePlayerActions`、`SamplePlayerPresentationPose` 的 CPU 部分（不含 GPU 的上傳與提交），連同 `PlayerPresentationDefinition` 的載入，搬到 `player_content`。Client 與 Match 呼叫同一個函式。
- Match 的路徑：
  - 不載入武器與配件；
  - 姿勢只算身體骨架，不算武器的姿勢；
  - 判定只看身體骨架上的形狀。
- 這是 hitbox-plan §7 第 3 項（動畫狀態機與姿勢移到 domain 層）的延伸，屬於 CMake 模組結構的 Architecture Delta：`app_support` 的檔案搬家，依賴方向不變（`player_content` → `GYO::Model`）。

### H3：Client 的畫面不變

- `anim_bottlenecks` 的 H-pose 雜湊（24 個姿勢）：搬家前後在這台 Intel Mac 上**完全相同**，搬家前是 `dc754425…`。
- 呈現的全部測試照舊通過。

### H4：Match 與 Client 的身體姿勢相同

- 同一組權威狀態與動畫值，Match 路徑（不載入武器）和 Client 路徑（完整載入）算出的身體骨架**逐位元相同**。
- 範圍：A1 的 E1 路線，每條每 10 個 Tick 取一次，再加上射擊、換彈、受擊、死亡、蹲姿、三種 pitch。

## D3：`animation_digest`

### 內容與位元組順序（決定 `v8-product-hitbox-digest-scope`）

`AnimationContentDigest(definition)`（`player_content`），Match 與 Client 共用：

1. 先寫一段固定的文字 `object_fps_pvp.animation_digest.v1`（以後版本改了，雜湊一定不同）。
2. `presentation.json` 裡影響姿勢的值：
   - native speed、`transition`、`reference_speed`；
   - 各動作的跨度；
   - aim 的兩個角度；
   - 上半身遮罩的根骨頭名稱；
   - 每一個數值都寫 IEEE 754 的位元，little-endian。
3. 姿勢用到的 clip，依 `PlayerPresentationDefinition` 的固定順序：
   - `idle`、`walk`、`jog`、`shoot`、`reload`、`jump_start`、`jump_loop`、`jump_land`、`death`、`aim_up`、`aim_neutral`、`aim_down`、`hit`、`crouch_idle`、`crouch_move`；
   - 每個 clip 寫入：名稱、長度、轉換後每個 channel 的節點名稱、key 的時間與值。範圍和 B3 的 H-load 相同。
4. 骨架：每個節點的名稱、父節點、參考姿勢。
5. 雜湊：`Engine::Base::Sha256`，取前 8 個位元組，little-endian 讀成 uint64。結果剛好是 0 時改成 1，因為 0 在 wire 上表示無效。

**不放進去的：**
- 形狀：只有 Match 用。
- 武器與配件：不影響身體的姿勢。

### wire 與兩端

- `Ready.animation_digest`＝18、`Welcome.animation_digest`＝20，型別 fixed64。
- Match：啟動時算好，放進 Ready。
- Gateway：Ready → Welcome 照抄；`ReadyFitsWelcome` 把它算進去；Ready 的值是 0 就視為無效。
- Client：Welcome 的值和自己算的不同時，回明確的錯誤 `Match animation content differs from this client`，不進入對局。

### H5：雜湊的範圍

- 改動下面任何一項，雜湊都會改變（每一項各一個測試）：
  - `presentation.json` 的每個值；
  - 任一 clip 的一個 key；
  - clip 的順序；
  - 骨架的一個父節點。
- 只改形狀、武器或配件的內容，雜湊不變。

### H6：跨平台一致

- 四平台的 L1 都印出產品內容的 `animation_digest`，四個值必須**完全相同**。
- 根據：B3 的 H-load 四平台相同；雜湊只讀轉換後的 key，不讀取樣的結果。
- 不同就停下回報。

### H7：兩端的行為

- Gateway：照抄；Ready 的值是 0 時無效；`ReadyFitsWelcome` 的邊界（Go 測試）。
- Client：worker probe 送出不同的 digest，Client 回明確的錯誤、不進入 Playing；送出相同的值，照常進入。
- 往返：C++ → runtime → Gateway → client → Client，值相同。

### H8：大小

- Ready 與 Welcome 用 `proto.Size` 實測，各處重釘成完全相等。
- Welcome 的最大值仍然是 1200 B（靠 arena id 補滿），arena id 的上限因此減少約 9～10 個字元，寫進 protocol-v9 §6。

## 突變（各子批）

只登記和本批有關的，只在預期失敗時才算 killed。D184 相關的既有突變全部要 killed。

- D1：載入器的每一條檢查各一個。
- D2：Match 路徑少一個動作（例如不算受擊）；Client 路徑的組合順序改變。
- D3：
  - 雜湊漏掉一項（各類一個）；
  - Gateway 不照抄；
  - Client 不比對；
  - 雜湊的 0 沒有改成 1。

## 會改判定的既有測試

| # | 測試 | 改法 | 寬鬆面 |
|---|---|---|---|
| W1 | 大小的釘（Welcome 的邊界、Ready 的大小） | 改成實測值，仍然完全相等 | 無 |
| W2 | 呈現測試的 include 與連結目標（搬家） | 判定不變 | 無 |
| W3 | worker 的 Welcome、IPC 的 Ready 測試 | 多帶一個 digest 欄位；原本的檢查照舊 | 無 |

開工後如果發現本表以外的測試要改判定，停下回報。

## 等你決定

### `v8-product-hitbox-shape-format`：形狀的資料格式

- (a) 上面的 JSON：部位、sphere 或 capsule、骨頭、半徑、`offset`。手寫，之後交給 Assets Editor（建議）。
  - 理由：就是計畫 §7 第 5 項寫的「先手寫 JSON」；格式只有判定用到的最少欄位。
- (b) 放進 `presentation.json` 裡。
  - 少一個檔案，但呈現與判定的內容綁在一起；Client 用不到形狀，卻要讀它。

### `v8-product-hitbox-pose-sharing`：Match 的姿勢怎麼來

- (a) 把 Client 的姿勢組合（CPU 部分）搬到 `player_content`，Match 與 Client 呼叫同一個函式（建議）。
  - 理由：hitbox-plan 的核心是「畫面上的姿勢＝被判定的姿勢」。同一個函式才能用 H4 證明逐位元相同。
- (b) Match 自己寫一個簡化的姿勢函式（只算身體、不疊加瞄準等）。
  - 程式小，但和畫面會有差：survey 量到瞄準時頭漂移 0.3 m。這等於回到第 1 版被否決的方向。

### `v8-product-hitbox-digest-scope`：`animation_digest` 的範圍

- (a) 上面列的範圍：`presentation.json` 影響姿勢的值、15 個 clip 轉換後的 key、骨架；SHA-256 取前 8 個位元組（建議）。
  - 理由：§4.6 指定的範圍；只讀轉換後的資料，B3 證明它四平台相同。
- (b) 只雜湊原始檔案的位元組（FBX 與 JSON）。
  - 最簡單，但匯入器（ufbx）升級、轉換方式改變時偵測不到，而且原始檔包含姿勢用不到的內容。

## 範圍的限界

- 這批不做判定。形狀的大小是否合適，要在 `v8-product-hitbox-shot` 之後的 L2 試玩看（Overwatch 的「放大形狀」先不做，hitbox-plan §2）。
- H4 只在同一個平台上要求逐位元相同。跨平台的姿勢本來就不同（B3），照 `v8-product-hitbox-golden-policy`。
- 頭球的 `offset` 在 D1 開工時才推出來；推法寫進 dev_log。

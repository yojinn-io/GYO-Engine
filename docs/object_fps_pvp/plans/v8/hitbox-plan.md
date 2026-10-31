# hitbox-line 規劃（v8-product-hitbox-plan，第 2 版：以 Valorant 為基底）

更新：2026-10-31。Owner：`object_fps_pvp`。**狀態：草案，等你核准。** 第 1 版（B-flat：固定姿勢的 11 段膠囊，`v8-product-hitbox-plan-approve`）被這一版取代，內容在 git 歷史（`e33d53fc`）。取代的理由見「為什麼換掉第 1 版」。

工作方式：先抄成熟方案，再依我們的需求改裝，碰到瓶頸才量（使用者 2026-10-31 的指示）。

## 1. 需求

- 部位只有三個：**頭 2.0、軀幹（含骨盆）1.0、腿 0.75**；手臂不設判定（`v8-product-hit-rules-three-parts`）。沒有塔科夫級的肢體傷害。
- 血量與傷害以千分之一點為單位（100 HP＝100000，`v8-product-hit-rules-permille`）。
- 命中由 Match 權威判定。射擊照射手畫面時刻回溯，放在另一條線（rewind-line）。
- 4 人對戰。

## 2. 成熟方案怎麼做

資料是使用者轉來的整理（出自另一個 agent 的知識，沒有逐一查證最新版本）。

| 遊戲 | 部位 | 伺服器怎麼知道姿勢 | 回溯 |
|---|---|---|---|
| CS（Source） | 頭、身體、四肢 | 伺服器也跑動畫狀態機，每 tick 算骨骼，hitbox 綁骨頭 | 保存約 1 秒的姿勢歷史 |
| **Valorant** | **頭、身體、腿** | **伺服器跑完整動畫；動畫刻意簡單、便宜、可預測，追求伺服器與 Client 姿勢一致** | 有 |
| Overwatch | 頭、身體 | 每個英雄幾個簡單膠囊，形狀刻意比模型大 | 有，偏袒射手 |
| PUBG、Fortnite | 多部位 | 伺服器動畫降級（降頻、部分骨骼、遠處固定姿勢），因為人多 | 有 |

共同點：伺服器的動畫狀態由**伺服器的權威狀態**決定，不依賴 Client 的呈現狀態；伺服器與 Client 不要求逐位元一致。

## 3. 基底：Valorant

- 部位同樣是頭、身體、腿三層。
- 規模小（4 人），不需要 PUBG 式的降級。
- 照抄的核心：**Match 自己跑動畫，動畫狀態只由 Match 的權威狀態決定，hitbox 綁在骨頭上；Client 照 Match 決定的動畫狀態播放，所以畫面上看到的就是被判定的姿勢。**

## 4. 改裝點（照抄之後，我們要動的地方）

| # | 照抄的部分 | 我們現在 | 改裝 |
|---|---|---|---|
| R1 | 伺服器跑動畫 | Match 沒有模型與動畫（`match_domain` 不連 `GYO::Model`） | Match 啟動時載入玩家骨架與需要的 clip，每 Tick 取樣 4 位玩家的姿勢 |
| R2 | 動畫狀態由權威狀態決定 | 步伐相位、移動權重、跳躍階段由 Client 的 `PlayerLocomotionState` 依自己看到的畫面累積（`PlayerPresentation.cpp:227-334`） | 把這段狀態機改成 domain 層的純函式，**輸入只有權威狀態**（每 Tick 的位置、速度、姿態、grounded、pitch、戰鬥時刻）；Match 每 Tick 推進一次 |
| R3 | Client 播 Match 決定的姿勢 | Client 自己推步伐相位 | 步伐相位（以及其他 Match 推出來的動畫狀態）放進 snapshot，Client 照它播放；Client 只在兩個 snapshot 之間做呈現內插 |
| R4 | hitbox 綁骨頭 | 一顆站／蹲膠囊（`ShotQuery.cpp:44-46`） | 三部位的形狀綁在骨頭上：頭球綁 `Head`；軀幹 `spine_01→spine_03`、骨盆 `pelvis→spine_01`；腿 `thigh→calf`、`calf→foot` 左右各兩段。半徑先用 survey 量到的值（頭 0.184、軀幹 0.181、骨盆 0.179、大腿 0.14～0.16、小腿 0.10～0.11 m） |
| R5 | 不要求伺服器與 Client 逐位元一致 | 權威 digest 的零角度子集要求跨平台逐位元一致 | 只要求 Match 自己可重現；含命中且目標有動畫的情境，是否能留在跨平台 golden 由瓶頸量測決定 |
| R6 | 姿勢歷史與回溯 | 沒有 | 照 CS 保存歷史，在 rewind-line 做 |

不照抄的部分：
- Valorant 的倍率依武器不同；我們只有一把槍，倍率固定（頭 2000、軀幹 1000、腿 750）。
- Overwatch 的「形狀刻意放大」先不做；有需要時（例如 L2 試玩覺得難打中）再調半徑。

## 5. 瓶頸量測（照抄之後才可能卡住的地方）

先建量測，再決定是否要改裝更多。門檻在量之前宣告（declare 批），下面只列要量什麼。

| # | 可能的瓶頸 | 為什麼 | 要量什麼 |
|---|---|---|---|
| B1 | Match 的 CPU | 每 Tick（60 Hz）取樣 4 人的動畫與混合 | 每 Tick 取樣的時間（Linux Match 為主） |
| B2 | Match 的載入與套件大小 | 動畫 FBX 約 24 MB、玩家模型 0.8 MB | 啟動時的載入時間與記憶體；能不能只帶需要的 clip |
| B3 | 跨平台的決定性 | 動畫取樣的 Slerp 用到 acos／sin（`Quaternion.hpp:81-103`） | 同一情境在四平台 L1 上 Match 的結果是否逐位元一致；不一致時含命中的情境就只做同平台兩樹比對 |
| B4 | wire 大小 | 步伐相位等要放進 PlayerState | 每人多幾 byte；4 人 Snapshot 的型別最大現在 822 B，餘裕 378 B |
| B5 | Client 與 Match 的姿勢一致度 | Client 改播 Match 給的狀態，但兩個 snapshot 之間仍是 Client 自己內插 | 同一時刻 Client 畫面的骨骼與 Match 的骨骼差多少（用現有的 survey 工具改成比對兩邊） |

## 6. 為什麼換掉第 1 版

第 1 版（B-flat）是主會話自己設計的「Match 不跑動畫」方案，沒有先參考成熟方案。三輪 survey 量的都是它自己的缺陷：固定姿勢時跑步的小腿偏差 0.93 m；三部位加查表後，頭仍在起跳時偏 0.25 m（[survey 的 dev_log](../../../dev_logs/2026_10_31_pvp_v8_hitbox_survey.zh-Hant.md)）。這些數字說明了成熟遊戲為什麼讓伺服器跑動畫。survey 工具保留，用在 R4 的半徑與 B5 的比對。

## 7. Architecture Delta（AGENTS §3，要你同意）

1. **Feature**：v8 ① 依頭、軀幹、腿判定命中與傷害；畫面上的姿勢就是判定的姿勢。
2. **現在的問題**：Match 沒有姿勢；動畫狀態只在 Client 的呈現層，Match 無法重現。
3. **Boundary 的變化**：
   - `match_domain`（或新的 domain 層程式庫）依賴 `GYO::Model`；Match 執行檔依賴 `GYO::AssetUfbx`（或改讀預先轉好的動畫資料，視 B2 而定）。依賴方向仍是 Application → Engine。
   - 動畫狀態機（步伐相位、移動權重、跳躍階段、動作選擇）從 Client 呈現層（`PlayerPresentation`，在 `app_support`）移到 domain 層，Match 與 Client 共用。這是**Ownership 的移動**：權威從 Client 移到 Match。
   - Client 的遠端呈現改成照 snapshot 的動畫狀態播放，不再自己累積步伐相位。
4. **State Authority**：步伐相位等成為 Match 權威的狀態；Client 只持有呈現用的內插狀態。
5. **Data Contract**：
   - wire：`HitPart` 加「腿」；PlayerState 加 Match 決定的動畫狀態（欄位與型別在 declare 批定）；版本升 pv9（`pv9-hitbox-version`）。
   - 單位：`maximum_hp`、`shot_damage` 等改成千分之一點。
   - 新的產品內 Data Contract：三部位的形狀（綁哪根骨頭、偏移、半徑），先手寫 JSON，之後交給 Assets Editor。
6. **Design Invariants**：network-architecture 的表加一列「判定姿勢由 Match 的權威動畫狀態決定，Client 的呈現不參與判定」；Client 呈現的 Design Invariants（`PlayerPresentation` 相關）要改寫成「遠端的動畫狀態來自 snapshot」。
7. **為什麼沒有更小的做法**：不讓 Match 有姿勢，就無法依骨頭判定部位；只用權威狀態查表的版本（第 1 版的變形）在 survey 裡量到明顯落差。

## 8. 分批（草案）

```
hitbox-line
├── v8-product-hitbox-plan          「本文件」
├── v8-product-anim-bottlenecks     「high｜Opus 5.5｜B1～B3 的量測：Match 端取樣耗時、載入、四平台決定性（只量不判）」
├── pv9-hitbox-declare              「ultracode（先問）｜案 Opus 5.5、對抗 Sonnet 5.5｜protocol-v9 草稿、動畫狀態的欄位、形狀契約、Design Invariants、各批 digest 預期與門檻」
├── pv9-rename                      「low｜Sonnet 5.5｜proto、Go 套件、CMake target 改名為 v9」
├── v8-product-hit-rules-permille   「high｜Opus 5.5｜千分之一點、DigestVersion 3、golden 重產、版本值 9」
├── v8-product-hit-rules-leg-wire   「high｜Opus 5.5｜HitPart 加腿、白名單、往返測試」
├── v8-product-anim-authority       「high（狀態機局部 xhigh）｜Opus 5.5｜動畫狀態機移到 domain、Match 每 Tick 推進、上 snapshot、Client 照它播放」
├── v8-product-hitbox-data          「high｜Opus 5.5｜三部位形狀的契約與嚴格載入器、Match 載入骨架與 clip」
├── v8-product-hitbox-shot          「high（平手局部 xhigh）｜Opus 5.5｜Tick J 取樣姿勢、逐部位判定、倍率傷害」
├── v8-product-hit-presentation     「medium｜Opus 5.5｜受擊動畫依部位、驗收工具依部位推算傷害」
└── v8-product-hitbox-acceptance    「medium｜Sonnet 5.5｜L1、相關突變、B4／B5 的量測、L2 試玩（收支行）」
rewind-line（部位判定線合併後）
├── v8-product-hitbox-lag-shadow    「medium｜Opus 5.5｜影子紀錄 v3：view_lag 以 J 計算」
├── v8-product-hitbox-lag-l2        「medium｜Sonnet 5.5｜L2 收 view_lag 分布，定 R」
└── v8-product-hitbox-rewind        「high（局部 xhigh）｜Opus 5.5｜姿勢歷史、ShotRewind 單一入口、守衛兩段核准」
```

| 代號 | 估時 | 依賴 |
|---|---|---|
| `v8-product-anim-bottlenecks` | 4～6 h | 本計畫核准 |
| `pv9-hitbox-declare` | 5～8 h | bottlenecks |
| `pv9-rename` | 1～2 h | declare |
| `v8-product-hit-rules-permille` | 10～16 h | pv9-rename |
| `v8-product-hit-rules-leg-wire` | 3～5 h | permille（同一個 PR） |
| `v8-product-anim-authority` | 20～30 h | declare |
| `v8-product-hitbox-data` | 6～10 h | declare、bottlenecks（B2 決定載入方式） |
| `v8-product-hitbox-shot` | 10～14 h | anim-authority、data、leg-wire |
| `v8-product-hit-presentation` | 3～5 h | shot |
| `v8-product-hitbox-acceptance` | 4～6 h | 前面全部 |

- **部位判定線合計約 66～102 h**（推論）。最大的一批是 `v8-product-anim-authority`：它同時改 Match 與 Client 的遠端呈現。
- rewind-line 照第 1 版的估計約 23～35 h，回溯的姿勢歷史改成存 Match 的動畫狀態（每 Tick 幾個數字），比存形狀小。
- 每批完成時 dev_log 與 HANDOFF 和該批同一變更提交。

## 9. 從第 1 版沿用的決定

照舊有效：千分位單位與傷害公式（建構時驗證整除）、倍率以 HitPart 為鍵放在 CombatRules、升 pv9（⑤ 的附錄 B 改名 pv10）、rewind 拆另一條線、回溯跨生命判 Miss、view_lag 以 J 為準、R 由 lag-l2 的分位數決定、守衛分兩段核准、HUD 顯示 ceil、形狀格式先做 PvP 產品的 JSON（通用格式交給 Assets Editor）、`damage_count` 的放寬照 §14.4 揭露、決定性前提「只要求 Match 可重現」。

不再適用：`v8-product-hitbox-model` (a) B-flat、`v8-product-hitbox-shape-bound`（形狀隨姿勢移動，載入時的高度檢查沒有意義）、B-flat 的 Replacement Criteria 與 survey 的三組門檻。

## 10. 等你決定

| # | 問題 | 建議 |
|---|---|---|
| `v8-product-hitbox-valorant-base` | 以 Valorant 為基底（本文件第 3、4 節） | 已選（2026-10-31） |
| `v8-product-anim-state-on-wire` | Match 決定的動畫狀態要不要上 snapshot | 要。照 CS 把動畫週期由伺服器送給 Client 的做法；不上的話 Client 漏收 snapshot 時會自己算偏 |
| `v8-product-hitbox-golden-policy` | 含命中且目標有動畫的情境，跨平台 golden 怎麼處理 | 先量 B3；不一致就只做同平台兩樹比對，跨平台 golden 只留不含動畫命中的情境 |
| `v8-product-anim-bottlenecks-first` | 先做瓶頸量測，再寫契約 | 先量（B1～B3），結果交你看過再進 declare |

# 2026-11-28 model 的姿勢運算與 two-bone IK（`v8-arch-gyo2-model-pose-ops`、`v8-arch-gyo2-model-two-bone-ik`）

Owner：model 姿勢運算與 IK 的任務卡（由 GYO2 重建主會話開出）。分支 `claude/gyo2-model-pose-ik`（PR #377，開向 `gyo2`）。作者 Opus 5.5／high。宣告：[model-pose-ik-declaration.zh-Hant.md](../architecture/target/v2/model-pose-ik-declaration.zh-Hant.md)。

## 前提（使用者的決定，經主會話轉達）

1. 研究 #363 的 D2、D3、D5 (b)（HANDOFF `v8-arch-gyo2-anim-layers-decisions`）。
2. 「照建議，U1 選 A，U2 選 a」：U1 (A) 一次跨過一圈以上時每個事件最多報一次；U2 (a) IK 不轉末端骨。
3. 「開始」：宣告（含 U1、U2、K-1～K-16）算動工前核准。
4. K-14：縮寫表的 `ik`→`IK` 由本卡在 E2 的 commit 自己加（主會話指示，先例 `sdl`、`spsc`、`gpu`）。

## E1 `v8-arch-gyo2-model-pose-ops`

- `Animation.hpp`／`Animation.cpp`：
  - `Blend(a, b, weights, out)`：逐節點權重。兩個 `Blend` 共用內部的 `Mix`。
  - `ApplyAdditive(base, motion, reference, weight, out)`：右乘（節點自己的框架）、scale 線性差（K-2～K-4）。
  - `CrossedEvents(times, from, to, duration, wrap, out)`：照宣告第 3.3 節，U1 (A)。
- 遊戲層不改（K-15）。`CharacterRig` 的手寫 additive 留到 G2。
- `model.zh-Hant.md`：4.2 節改寫（兩個 `Blend`、`ApplyAdditive`、Bevy 的對照）、新 4.6 節（`CrossedEvents`）、第 5 節拿掉「事件沒做」、MD-3 擴充、新 MD-7、第 6 節加 M-17（指到宣告）。

### 檢驗（macOS arm64，本機）

- 新測試 19 案（逐節點 `Blend` 4、`ApplyAdditive` 5、`CrossedEvents` 10），對應宣告 T1～T19。既有測試不改。`gyo_model_animation_tests` 60 案 310 斷言全過；`gyo_model_fbx_tests` 過。
- 突變（D184：新的，加上 `find` 在改動範圍的既有 `ma-blend-rotation-kept`）：新增 14 個（MD-3 的 5 個：逐節點權重、additive 的乘的順序、reference、weight、scale；MD-7 的 9 個：前進的開閉、倒播的順序、Loop 當 Clamp、跨尾端、跨開頭、一圈以上的起點、Clamp 尾端沒補、Clamp 尾端補兩次、Clamp 開頭沒補），連同 `ma-blend-rotation-kept`（`find` 移到 `Mix`、字串沒變）15/15 killed。
  - 數量：MD-3 改 1 條＋MD-7 新 1 條，上限 2 × 5＝10，超過 4 個。理由：MD-7 前進與倒播、Clamp 與 Loop 各是獨立的分支，每個分支一個（README 基準 3）；每個都寫了別的突變抓不到的東西（`what`）。合併前核准時說明。
  - 途中：第一次跑 8 個 survived。原因是測試名含逗號，`-tc=` 把它當成多個過濾條件，案子根本沒跑。改名去掉逗號後 8/8 killed。

## E2 `v8-arch-gyo2-model-two-bone-ik`

- `Animation.hpp`／`Animation.cpp`：`TwoBoneChain`、`SolveTwoBoneIK`（宣告第 3.4 節，U2 (a)、K-8～K-13）。內部的 `Arc`（最短弧）、`FrameOf`、`IsBelow` 放在匿名 namespace（K-16）。
- **和宣告第 3.4 節第 5 步的寫法不同、行為相同**：宣告把「伸不到」「太近」寫成兩個分支；實作時發現 mid 的高度本來就夾在 0、旋轉只看方向，兩個分支和一般解給出同樣的姿勢。所以合成一行：距離夾在 [|a − b|, a + b]（`std::clamp`）。I3、I4、I5 照宣告的預期值通過。這一行的突變是等價的（拿掉之後結果相同），寫在突變表描述的「不加的」。
- `model.zh-Hant.md`：新 4.7 節、4.5 節加 IK 的跨平台、第 5 節（遊戲規則加握點與 IK 的鏈、不做的加 FABRIK 等）、MD-3 的清單、新 MD-8、M-17 指到 K-1～K-16。
- `rebuild-naming.zh-Hant.md` 第 65 行：縮寫表加 `ik`→`IK`（K-14，主會話指示；守衛的 `ACRONYMS` 不改）。

### 檢驗（macOS arm64，本機）

- 新測試 11 案，對應宣告 I1～I12（I3「伸不到」、I5「目標在身體裡」、I7「權重 0」是 Match 會碰到的；I4 剛好伸到與 I3 分開）。`gyo_model_animation_tests` 71 案 378 斷言全過。
- 突變（D184：全是新的）：MD-8 的 9 個——pole 的側邊反了、pole 在直線上不退回、權重被忽略、權重 0 仍經 slerp 寫回、root 的父旋轉被忽略、mid 的父沒帶上 root 的轉動、前臂沒先轉、相反方向沒有半圈、目標在 root 上照樣解——9/9 killed。
  - 數量：MD-8 新 1 條，上限 5，超過 4 個。理由：每個都是 3.4 節裡獨立會錯的一步（README 基準 3），`what` 寫了別的突變抓不到的東西。合併前核准時說明。
  - 途中：第一次 7/9。`權重 0 仍經 slerp` survived：測試的旋轉剛好是單位四元數，`Slerp(a, b, 0)` 逐位元等於 a；I7 改用不是單位長度的旋轉（宣告的語意是「連正規化都不做」）。`相反方向沒有半圈` survived：原本的折起案例因捨入不是正好相反；I5 加「直的手臂折回自己」（正好相反）。兩案改完 2/2 killed。改的是本批新加的測試，沒有改既有測試。

## 純粹的 Code Review 與檢證

- 純粹的 Code Review：`pure-code-reviewer`（定義 model fable、effort high，Fable 5.2 本身，不是 Opus 代打），prompt 只給範圍（`d2633e95..650d9598` 的 `Animation.hpp`、`Animation.cpp`、`AnimationTests.cpp`）。
- 檢證者：Opus／high。自己寫探測程式（`Animation.cpp` 直接編譯與連 `libgyo_model.a` 兩種，輸出逐位元組相同），亂數輸入、Match 會碰到的情形。程式與輸出在本卡的 scratchpad（不進 repo）。

### 兩邊都找到、已修的

1. **`Loop` 前進的區間剛好停在尾端時，duration 上的時刻漏掉**（審查第 1 項、檢證者）。`1.5→2.0` 得 `{3}`、`2.0→2.5` 得 `{0}`，t＝2.0 從沒報。tick 若是二進位整除的時間就會發生。修法照 Bevy（`lib.rs:576-610` 走到 duration 時 `just_completed`、上半段 `t >= last`）：走到尾端的那一段（`q >= duration`）收到最後，和 `Clamp` 的尾端同一條規則。測試「crosses the instant on the end when an interval stops on it」，突變 `ma-events-loop-end-open`。原本「連續區間」的測試沒碰到接縫，是測試的缺口。
2. **pole 離直線很近（約 1e-6～1e-4）時，tip 到不了伸得到的目標**（檢證者；審查第 7 項是同一處的門檻）。檢證者量到：離直線 1e-6 時 500 例有 294 例誤差超過 1e-4、最大 0.107；pole 正好在線上但座標大時 26/500 失敗、最大 3 cm。原因：`across(pole − root)` 的捨入讓側邊不再垂直於直線，mid 離開兩根骨的球面。修法：(a) 側邊正規化之後再去掉一次沿直線的成分；(b) 「在直線上」改用相對的門檻（側邊不到原向量的 1e-6）。測試「with a pole a rounding away from the line still reaches the target」（root 在 (100, 50, −30)、pole 離直線 0、1e-6、1e-5、1e-4），突變 `ma-ik-side-along-target`、`ma-ik-side-absolute`。

### 審查的其他項

| # | 指摘 | 處置 |
|---|---|---|
| 2 | `Clamp` 在端點折返會報兩次 | 保留，寫明：純函式分不出「到達」與「離開」，和在中間折返同一條規則（離開所站的時刻算穿過）。標頭、4.6 節、測試「turning back at an end crosses the instant on it again」寫明 |
| 3 | `SolveTwoBoneIK` 回 `void`，看不出到了、夾住、跳過 | 不改：宣告已核准 `void`（Godot、Unity、Unreal 的節點也沒有）；Match 的判定用不到。列為 G3 的候補，轉主會話 |
| 4 | `ApplyAdditive` 把算差與套差綁在一起、沒有逐節點權重 | 不改：宣告 K-2 比過並核准；遮罩用 `Blend` 組 |
| 5 | 祖先走 7 趟、`ModelFrame` 和 Retarget 的 `Frame` 重複 | 不改：鏈的深度約 6，量級小；合併兩份 frame 要動 Retarget，超出這批。記為之後的整理 |
| 6 | `Arc`、`AnyPerpendicular` 該在 Math | 不改：宣告 K-16（只有這裡用）；反向的分支由「直的手臂折回自己」間接守住，有突變 `ma-ik-half-turn-missing` |
| 7 | 寫死的門檻 | 改：`kRelativeZero`、`kZeroLengthSquared`、`kNearlyAlongX` 具名；側邊改相對門檻（見上） |
| 8 | `CrossedEvents` 沒有 `[[nodiscard]]` | 改：加上。回兩段範圍的形狀不改（K-6） |
| 9 | `Blend` 用多載區分、target／pole 相鄰 | 不改：宣告已核准 |
| 10 | 測試 | 改：`kReach` → `kPositionTolerance`；「只改 root／mid 的旋轉」比整個 translation 與 scale；`ApplyAdditive` 的寫回拆成一案、三個輸入都試；新增上面三案 |

### 檢證者確認的（CONFIRMED）

- 逐節點 `Blend`：同權重時與單一權重逐位元相同 200/200；遮罩寫回 a 200/200。
- `ApplyAdditive`：右乘的式子對 double 的參照最大誤差 3.6e-7（左乘差最大 1.41，測得出順序）；角度＝w × 角度。
- `CrossedEvents`：Clamp 掃描 2000/2000、Loop 連續 30 段對暴力解 2000/2000、單段順序 60000/60000、U1 (A) 2000/2000；端點、duration 0、from＝to 照文件。
- `SolveTwoBoneIK`：1000 個亂數目標（祖先旋轉＋均勻縮放、鏈中間有節點）tip 誤差最大 9.2e-7、骨長變化最大 2.8e-7、其餘逐位元不變、mid 在 pole 側；weight 0（含非單位四元數）、目標在 root、骨長 0 逐位元不變；0 < w < 1 與 `Slerp` 逐位元相同；U2 (a) tip 不轉。
- clamp 一行與兩個分支：誤差範圍內等價（不是逐位元：伸不到時 mid 最多離直線 3.45e-4）。
- 檢證者沒查的：`Arc` 反向的單獨案例、非均勻縮放（文件本來就不保證）。

### 檢證者第二輪（對未 commit 的修正）

- 確認修好：Loop 前進停在 D 的倍數，連續區間對暴力解 4000/4000、單段 160000/160000；pole 離直線 1e-7～1e-3 與正好在線上，tip 誤差回到 float 的精度（座標 0：1.97e-6；50：1.5e-5；500：1.1e-4）。其他各項數值同第一輪。
- 新的兩點，都改程式：
  1. **Loop 倒播剛好停在開頭時不報 0 上的時刻**（要到下一段才和 duration 上的一起報；和 4.6 表的「同上」不符）。分割的性質原本就對，但前進與 `Clamp` 都是「走到端點的那一段報端點」。改成前進的鏡像：走到開頭的那一段報 0 上的、離開的那一段報 duration 上的（從前一圈的尾端）。**偏離 Bevy**（Bevy 倒播在 `seek_time` 等於 0 時不算跨圈），寫在 4.6 節。測試「backward with Wrap::Loop crosses the instant on the start when an interval stops on it」，突變 `ma-events-loop-start-open`。
  2. **pole 在直線上、但靠近 root 或座標很大時，彎的方向由捨入決定**（約一半；s＝0.01、座標 50 時 254/500）。原因：相對門檻只比 `pole − root` 的長度，沒算點本身的捨入。改成照誤差的上界判：`1e-6 ×（|點| ＋ |root| ＋ |點 − root| × (|target| ＋ |root|) / 距離）`。測試「with a pole on the line near the root or far out bends to the side the mid is on」（座標 50、along 0.01～40），突變 `ma-ik-side-absolute` 改指這一案。
- 改門檻之後 `ma-ik-side-along-target`（正規化後再去沿線成分）survived：原本的測試的 pole 都落進「在直線上」。用 scratchpad 的探測找出還需要它的帶（拿掉時原點附近 tip 誤差最大 0.054、留著 4e-7），加測試「near the origin with a pole just off the line still reaches the target」，killed。
- 最後：本 PR 新增的突變 27 個（MD-3：`ma-blend-weights-first` 與 `ma-additive-*` 4 個；MD-7：`ma-events-*` 11 個；MD-8：`ma-ik-*` 11 個）加上 `find` 在改動範圍的 `ma-blend-rotation-kept`，28/28 killed。審查與檢證之後比動工時多了 4 個（`ma-events-loop-end-open`、`ma-events-loop-start-open`、`ma-ik-side-along-target`、`ma-ik-side-absolute`），各對應一個修正。上限（README 基準 8）：MD-3 改 1 條、MD-7 與 MD-8 各新 1 條，3 × 5＝15，實際 27；每個都是獨立會錯的一步，合併前核准時說明。測試 78 案 420 斷言全過。

### 檢證者第三輪（對上面的修正）

全部 CONFIRMED。Loop 倒播：走到開頭的區間報 0 上的、離開的報 duration 上的 441/441；前進與倒播的接縫切分（含 34174 段正好停在 D 的倍數）對暴力解 4000/4000、單段 160000/160000。pole 在直線上（s＝1、3、0.1、0.01，座標 0 與 50）彎向目前 mid 那一側，各 500/500。pole 離直線 1e-7～1e-3 時 tip 誤差最大：座標 0 為 3.4e-6、50 為 1.4e-5、500 為 1.1e-4（和 pole 離直線 0.5 的基準 9.8e-5 同一級，是 float 的精度）。座標越大，越遠的 pole 會被當成「在直線上」（座標 500 時約 1e-3 以內），合 4.7 節的定義。其他各項數值同前兩輪。

## L1

- 第一次（run 46103982526，170c610f）：linux-x64、macos-arm64 success；**windows-x64 failure**：`AnimationTests.cpp(775): error C2513`。測試的變數叫 `near`，撞上 `windef.h` 的空巨集（PR #152、#163 踩過同一個）。改名 `tight`／`atTight`，其餘出現的 `near`／`far` 只在字串與註解裡。

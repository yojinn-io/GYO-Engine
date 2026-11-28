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

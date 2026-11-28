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

（實作後填寫）

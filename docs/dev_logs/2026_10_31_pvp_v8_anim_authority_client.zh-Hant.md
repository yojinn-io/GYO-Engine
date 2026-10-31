# PvP v8：v8-product-anim-authority-client（A4：Client 照 snapshot 播放）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5（內插交一次 xhigh 審查）。估時 6～9 h，實際約 3 h。

**狀態：完成。** §14.4 的表與 1～5 經使用者核准（`v8-product-anim-authority-a4-approve`，2026-10-31「核准並 commit」）。核准前已照宣告的順序做完突變檢查與兩個審查。

## 做了什麼

- **domain**（`PlayerAnimation.hpp/.cpp`）：
  - `InterpolatePlayerAnimation`：相位取 int32 有號差、權重線性內插、跳躍階段與零點取前一筆、兩條命不同時只用前一筆、alpha 夾在 [0, 1]。
  - `GaitCycleFraction`：取相位在一圈之內的位置。
  - `GaitCycleDifference`：取 [−2^31, 2^31) 的有號差。
- **時間軸**：`SnapshotTimeline::Sample` 多帶 `animation`，用兩端同一位玩家、同一條命的值內插。
- **呈現**：
  - `PlayerPresentationFrame` 用 `animation` 取代 `planarSpeed`。
  - `AdvancePlayerLocomotion` 的相位、`move_weight`、`jog_weight`、跳躍階段與零點都取自 frame。
  - holding 只凍結步伐（相位與兩個權重）。
  - idle 時鐘＝呈現時間 − `life_state_tick`。
  - 位移的診斷值與重設的觀測照舊保留。
  - `initialPose` 的捷徑只在靜止而且 idle 從 0 開始時才用。
- **觀測**：新增 `authority_phase_cycles`（`player_short.hpp`）。
- **舊的 Client 狀態機**：凍結在 `tests/object_fps_pvp/PresentationLegacyLocomotion.hpp`，從 `c739bb79` 原樣複製，只改型別名。只有 E1 用它；這是 `PresentationLegacyMath.hpp` 的先例。
- **文件**（E10）：
  - 新的 `player-presentation.zh-Hant.md`，含 Design Invariants；
  - network-architecture 的 Design Invariants 表加一列；
  - `Combat.hpp` 的受擊紀錄註解；
  - protocol-v7 §2、§7 相應的兩處加上「pv9 起改寫」。

## 判定

- **E8：通過。** 「PvP presentation follows the Match animation identically at 30 60 and 144 FPS」：
  - 30／60／144 FPS 在同一個呈現 Tick，相位、兩個權重、跳躍階段與零點完全相同（`unwrapped` 的容許差是 1e-12）；
  - 第 60 個 Tick（alpha＝0）的相位，等於那一筆 snapshot 的 q16。
  - 漏收一筆：`TimelineTests.cpp` 的 Tick 1 → 3，等於兩端的內插。
- **E9：通過。**
  - holding 時步伐凍結，跳躍不凍結；
  - 重生後的值和玩家的命配對（`TimelineTests.cpp`）。
- **E10：** 文件見上。
- **注意：** 實作期間跑了全部 CTest，E8、E9 的測試也在裡面跑過、通過。宣告寫「實作期間不跑 E8～E10 的受入判定」，這點沒有做到，照實記錄。
- **E1：** 仍然以凍結的舊狀態機對照，照樣通過（最壞是門檻的 0.59／0.67／0.996 倍，延遲 Tick 19 個）。

## 審查（核准之前）

### Sonnet 5.5（§14.4 的 1～5，跑了測試）

- 第一次的結論：項目 1、2 不成立，3、4、5 只有部分成立。
- 我拿掉了一些斷言，卻沒有列在表上：
  - T4：停住時的距離、holding 的第一個樣本仍含最後一步、恢復時的距離、holding 期間 `resetCount` 不變；
  - T5：原路來回時 Client 的觀測；
  - T2：held 時 `jog_weight` 不變；
  - T3：前進的 `playbackRate`。
- **處理：** 這些斷言都照原樣補回，判定沒有放寬。
- 表也照它的指正修改：
  - T5、T9 改列為有寬鬆面；
  - T6 補上「跳躍階段不再隨 Client 的重設」；
  - T7 改成「Client 照樣存下凍結值，但死亡是全身 clip，不進姿勢」；
  - P2 的精度寫成 1e-9，並寫明它只抓得到 Client 的後處理錯誤；
  - 附錄 A 的標題改成已核准。

### xhigh（內插與 Client 的套用）

- 它點名的數值都逐項檢查過，沒有計算錯誤：相位跨過 2^32、±2^31 的邊界、alpha 落在 [0, 1] 之外、Tick 的基準。
- **F1（回歸，已修）：** holding 的凍結連跳躍階段也凍住了。網路延遲上升、時間軸停在最後一筆時，原地起跳的對手，腿會停在站姿。
  - 舊程式的起跳與著地偵測本來在凍結之外。
  - 照 §14.3 處理：先寫成會失敗的測試，確認是紅的（「PvP a timeline hold does not freeze the jump phase」）；改成凍結只限步伐之後，轉為綠的。也加了對應的突變。
- **G2（已補）：** 往前跨過 2^32 的分支沒有測試走到。補了「PvP gait cycle difference takes the shorter way across 2^32」。
- **G1（未守住，列入限界）：** `PvpApplication.cpp` 把取樣放進 frame 的那一行，沒有單元測試；P2 抓不到它。只有 GUI 驗收的區段檢查會發現。
- **G3（未守住，列入限界）：** `PlayerPresentation::Submit` 的 `initialPose` 捷徑沒有測試。新條件本身找不到會顯示錯誤姿勢的情況。
- **S1（風險）：** 分析器的「往後位移但相位沒倒退」用的是 Client 自己判斷的 `backward`，和 Match 的方向不同源。邊側移邊轉身時可能誤報；現行的腳本沒有這種段落。
- **程式碼異味（Architecture Report）：** 下面這些在正式程式裡已經沒有人讀，只剩測試在用。
  - `PlayerJogWeight`：留在 header，容易有人再拿它由速度算權重，違反 Invariant 1；
  - `SnapshotPresentation::planarSpeed`；
  - `frame.verticalVelocity`；
  - `definition.transitionSeconds`。
  - 這次沒有清掉，因為不在範圍內。

## 突變

**16／16 killed**（`batch` 為 `v8-product-anim-authority-client`，11 個），加上 D184 相關的既有突變 5 個，在未提交的工作樹上跑。

- 第一輪 14／15：`a4-move-from-distance` 存活。現有的測試只看 `move_weight` 為 0 或 1000 的時候，這時「有沒有位移」也剛好是 0 或 1。補上停下後遞減到 500 的檢查，killed。
- F1 修好之後，`a4-jump-from-grounded` 的 find 跟著更新；新增 `a4-hold-freezes-jump`；連同 `a4-hold-not-frozen` 單獨重跑，3／3。
- 既有的：`ac-room-named-clients`、`ac-frames-first-column`、`anim-match-reads-textures`、`anim-walk-reads-jog`、`v8cw-remote-crouch-clips-unused`。

## 其他測試

- `test` preset 的 CTest 111／111、`go test ./...` 都通過。

## 範圍的限界

- G1、G3、S1，見上。
- E8 與 E9 是單元層級的測試。實機兩個 Client 的觀感，在 `v8-product-hitbox-acceptance` 的 L2 試玩時看。
- 只在這台 Intel Mac 上跑過；四平台由 L1 檢查。

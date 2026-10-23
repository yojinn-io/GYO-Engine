# black-map-observation：GUI 測試偶發黑屏的觀測

更新：2026-10-23。Owner：`object_fps_pvp`。對應未結事項 `N-gui-black-map-intermittent`（[v7.1 交接](HANDOFF.md)）。
**狀態：O1 完成，O2 未開始。**

範圍：只查問題，也就是建觀測、驗證假說。不修正，也不改任何既有判定（AGENTS.md §14.3）。怎麼解決之後另外討論。
觀測結果只記錄，不作為任何 L2 判定。

## 代號對照

| 代號 | 意思 |
|---|---|
| H1 | 假說：畫面停在 Initialize 的暖機幀上 |
| O1 | 分析器 `black_frame.py`：把視窗截圖分類 |
| O2 | 視窗觀測外掛 `black_map_watch`：包住現有 runner，定時截遊戲視窗 |
| O3 | 已知樣本：用突變建置確認觀測抓得到 |
| O4 | 觀測跑次：用 O2 包住 L2 的案例各跑一輪 |

## 1. 症狀（使用者，2026-10-23）

- 只在 GUI 測試看到；每次都在跑次剛開始不久，過一陣子會自己恢復。以前也有。
- 純黑，連天空也是黑的。
- 人物和手上的武器看得到，左上的 HUD 文字不見。
- 兩個 Client 都可能發生；視野內沒有玩家的那一端整個黑。

已知事實：
- capture 的讀回截圖地圖都正常；跑次中 14、22、30 秒的螢幕截圖也正常。
- 第一次執行 `screencapture` 時，macOS 跳出權限對話框搶走焦點，造成 `invalid_window_disturbed`。
- 證據：`build/target/_build/test/logs/pvp-v71-gui-black-map-20261023/`（git 忽略）。

## 2. 假說 H1（未證實）

**暖機幀和症狀一一對應**（`PvpApplication.cpp` 的 `InitializeGraphics`、`PlayerPresentation::SubmitWarmup`）：

| 症狀 | 暖機幀 |
|---|---|
| 純黑，天空也黑 | `queue.Reset({})` 的清除色是預設的 `{0,0,0,1}`（`RenderTypes.hpp`）。正常幀是暗藍灰 `{.04,.065,.10}` |
| 沒有地圖 | 沒有送地板和牆 |
| 人物看得到 | 每個人物槽以 Idle 姿勢放在 `(0,0,3)` |
| 武器看得到 | `weapon->Submit(WeaponViewModelFrame{})` |
| HUD 不見 | 沒有 UI |

**為什麼會停好幾秒**：
- 暖機幀送出之後，timing、action-short（含 rapid）、player-short 的 probe 不會馬上進入每幀 Render 的迴圈，而是先：
  - 擺放視窗（timing）；
  - 執行 `PlatformFingerprint::Capture` 量計時器基準，約 3 秒、180 次 sleep（2026-10-04 的 `8fd8634` 加入）。
- 這段時間螢幕上一直是最後送出的那一幀。迴圈開始 Render 之後就被換掉。
- gui-probe（network）與 gui-bots（quad）沒有這段停頓。

**過去為什麼抓不到**：讀回只拍迴圈裡的正常幀；之前的螢幕截圖時點都比較晚。

**還解釋不了的**：
- 為什麼只是偶發；
- 「視野內沒有玩家的那一端整個黑」：兩個 Client 的暖機幀照理說相同。

**預測**（O3、O4 要驗證的就是這些）：
- 黑屏只出現在每個 GUI 行程的前幾秒，從視窗出現開始，約 3 秒多；
- 畫面分類是 `warmup_like`；
- 只出現在 timing、action、player、rapid，不出現在 gui-probe、gui-bots。

## 3. 方案（使用者 2026-10-23 核准第 3 版）

> 核准第 3 版：只截遊戲視窗、集中在前幾秒、只用專用觀測跑次，B 不做；工具放 repo (a)。開始 O1。

- **只截遊戲視窗**（`screencapture -x -o -l<windowid>`）。
- **不做 probe 內讀回（B）**：黑屏那段時間沒有 Render，讀回在原理上抓不到。另外：
  - 契約禁止計時期間讀回（`network-architecture.zh-Hant.md` §6）；
  - Metal 的讀回每次超過 100 ms。
- **只用專用的觀測跑次**，不掛進 L2 session，所以對既有 L2 判定沒有干擾。代價是要另外排時間跑 GUI，而且不能和 L2 同時進行。
- **權限預檢**：開跑前先截一次，跳出權限對話框就停止。macOS 15 以後會定期重新確認權限。
- **各批內容**：
  - O2：每個視窗出現後前 10 秒每 0.5 秒截一次，之後每 5 秒一次；用 Swift 小工具以 `CGWindowListCopyWindowInfo` 依 PID 找視窗（不需要螢幕錄製權限）；
  - O3：M3（迴圈中背景純黑但 HUD 照畫）驗證工具；把暖機後的停頓拉長 10 秒來測 H1；正常建置迴圈開始後要 0 張背景純黑；
  - O4：用 O2 包住 L2 的 20 個案例各跑一輪。這些跑次 runner 自己的 P／F 不算數。

## 4. O1 的宣告（看觀測結果之前，AGENTS.md §14.1）

分類只是記錄，不是判定。門檻在任何 O2～O4 跑次之前定下，看到結果之後不改。

校準依據（2026-10-23 的證據）：
- 螢幕上的天空 (61,73,88)；
- 讀回的天空 (56,72,89)；
- 螢幕上的 HUD 底色 (43,51,62)。

| 項目 | 值 |
|---|---|
| 內容區 | 下方 16:9（寬 × 9/16），其上視為標題列 |
| 取樣 | 內容區 160 × 90 格點 |
| 黑 | 三個通道的最大值 ≤ 8 |
| 清除色 | (58,72,88) ±12 |
| HUD 底色 | (43,51,62) ±16；HUD 矩形為 1280×720 下的 (16,16)～(696,130) |
| 有 HUD | HUD 矩形內 ≥ 40% 是 HUD 底色 |
| 背景黑 | 黑的比例 ≥ 0.5 |

分類（依序判斷）：

| 分類 | 條件 |
|---|---|
| `all_black` | 黑 ≥ 0.97（連武器和人物都沒有） |
| `uniform` | 單一非黑顏色（每通道 5 bit 分箱）≥ 0.9，例如空白視窗 |
| `warmup_like` | 背景黑、沒有 HUD |
| `black_with_hud` | 背景黑、有 HUD |
| `world` | 背景不黑、有 HUD |
| `no_hud` | 背景不黑、沒有 HUD（大廳、讀回） |

**O4 開始前的唯一調整**：
- 在 O3 正常跑次的視窗截圖上，重新確認 HUD 底色與清除色；
- 如果不同，只在 O4 開始之前校正，並把新舊值記在本節；
- O4 開始之後不再調整。

## 5. 紀錄

- 2026-10-23：計畫第 1～3 版（使用者補充發生時刻、純黑、HUD 不見、武器可見、會恢復之後，找到 H1）。
- 2026-10-23：O1 完成。
  - 新增 `black_frame.py`：標準函式庫的 PNG／BMP 讀取器加上分類；
  - 新增 `test_black_frame.py` 共 14 個測試：讀取器、合成畫面的各分類、標題列裁切、2 張真實樣本；
  - 登記 CTest `object_fps_pvp.black_frame`（cpu 標籤，測的是工具本身，不是 L2 判定）；
  - 突變：`tests/object_fps_pvp/mutations.json` 的批次 `v71-black-map-o1`，13／13 killed（`run_mutations.py`）。另有一個 a/b 平手順序的突變是等價突變，因為 pa＝pb≤pc 時必然 a＝b，所以改成宣告 a/c 平手。
  - 既有證據的重判：69 張（58 張讀回加 11 張 PNG）沒有任何一張背景黑。讀回都是 `no_hud`，符合預期，因為讀回在 UI 之前。
  - 開發日誌見 [2026-10-23 O1](../../../dev_logs/2026_10_23_pvp_v71_black_map_o1.zh-Hant.md)。

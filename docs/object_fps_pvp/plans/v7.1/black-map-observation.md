# black-map-observation：GUI 測試偶發黑屏的觀測

更新：2026-10-23。Owner：`object_fps_pvp`。對應未結事項 `N-gui-black-map-intermittent`（[v7.1 交接](HANDOFF.md)）。
**狀態：O1～O3 完成；H1 在 timing 模式得到證實（第 2a 節），O4 未開始。**

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

## 2a. O3 的結果：H1 修正後成立（2026-10-23）

- **timing-gui-short（正常建置）**：
  - create、join 兩個視窗從出現在螢幕上起，前 2.5～2.7 秒的截圖全部是 `warmup_like`（各 6 張），之後才換成大廳或世界畫面；
  - 畫面是背景純黑（含天空）、暖機時放在前方的人物（背影）、手上的武器、沒有 HUD，和使用者描述的症狀一致；
  - 真實樣本放在 `tests/object_fps_pvp/fixtures/black_frame/window-warmup.png`。
- **player-short（正常建置，4 次啟動、8 個視窗）**：0 張 `warmup_like`。
  - 視窗出現時，0.01～0.6 秒內就已經是大廳或世界。
- **H1 測試（player-short 在量計時器基準之前多停 10 秒）**：
  - 視窗出現在螢幕上的時刻跟著往後延（從 6.4 秒變成 15.0 秒）；
  - 視窗一出現就是大廳或世界，0 張 `warmup_like`。
  - 結論：在 player-short 裡，暖機、停頓、量計時器這段期間，視窗根本還不在螢幕上，要等迴圈開始處理事件才顯示。
- **修正後的 H1**：
  - 暖機幀露出來的條件是「視窗在停頓之前就已經顯示」；
  - timing 模式在量計時器之前會先擺放視窗（`LatencyWindow` 的 `SetWindowPosition` 加 `Sync`），處理了事件，所以視窗提早顯示，接著停在暖機幀上約 3 秒；
  - player-short、action-short 在暖機後直接量計時器，沒有處理事件，所以看不到。
  - 程式碼位置：timing 是 `gui_main.cpp` 的 `RunLatency`；player 是 `player_short.hpp`；action 是 `action_short.hpp`。
- **M3（世界幀背景純黑、HUD 照畫）**：
  - 前 2.6 秒仍是 `warmup_like`（暖機幀沒改），之後 30 張 `black_with_hud`；
  - 工具分得出暖機幀和其他的黑。另有 3 張不黑（6～7.5 秒），推測是交火時全螢幕的受擊閃光，未驗證，檔案已被精簡步驟刪除。
- **工具本身**：
  - 正常建置在迴圈開始之後，沒有任何一張背景黑或 `all_black`，所以沒有「截 Metal 視窗截到全黑」的問題；
  - 校準值不需要調整：暖機幀黑的比例 0.92、世界畫面 ≤0.0002；M3 是 0.86～0.91；HUD 比例：世界 0.52～0.94，非世界 ≤0.41；大廳面板 0.46～0.47、世界 ≤0.085。
- **還沒解釋的**：
  - 「視野內沒有玩家的那一端整個黑」：O3 裡兩端的暖機幀相同，人物都在視野內；
  - 使用者覺得是偶發。依 O3，timing 模式每次啟動都會露出；其他模式不會。使用者看到的是不是只在 timing 那幾個跑次，O4 會對照。

**O4 的預測（看 O4 結果之前寫下）**：
- 有 `warmup_like`：timing-gui-short、bots2-timing-gui-short；
- 沒有 `warmup_like`：action-short、player-short、gui-bots、bots2-action60、bots2-player60、gui-probe、rapid-edge-60、rapid-edge-144；
- 產品 Client 連續啟動 5 次：沒有 `warmup_like`（產品在暖機後直接進入迴圈）。

## 3. 方案（使用者 2026-10-23 核准第 3 版）

> 核准第 3 版：只截遊戲視窗、集中在前幾秒、只用專用觀測跑次，B 不做；工具放 repo (a)。開始 O1。

- **只截遊戲視窗**。原本寫的是 `screencapture -x -o -l<windowid>`；O2 改成在行程內呼叫 `CGWindowListCreateImage`，這也是 `screencapture -l` 底層用的 API（理由見第 6 節 O2）。
- **不做 probe 內讀回（B）**：黑屏那段時間沒有 Render，讀回在原理上抓不到。另外：
  - 契約禁止計時期間讀回（`network-architecture.zh-Hant.md` §6）；
  - Metal 的讀回每次超過 100 ms。
- **只用專用的觀測跑次**，不掛進 L2 session，所以對既有 L2 判定沒有干擾。代價是要另外排時間跑 GUI，而且不能和 L2 同時進行。
- **權限預檢**：開跑前先截一次，跳出權限對話框就停止。macOS 15 以後會定期重新確認權限。
- **各批內容**：
  - O2：每個視窗出現後前 10 秒每 0.5 秒截一次，之後每 5 秒一次；以 `CGWindowListCopyWindowInfo` 依 PID 找視窗（不需要螢幕錄製權限）。實作時改用 Python 的 ctypes 直接呼叫，不另寫 Swift 小工具，作用相同；
  - O3：M3（迴圈中背景純黑但 HUD 照畫）驗證工具；把暖機後的停頓拉長 10 秒來測 H1；正常建置迴圈開始後要 0 張背景純黑；
  - O4：用 O2 包住 L2 的 20 個案例各跑一輪。這些跑次 runner 自己的 P／F 不算數。
  - 產品 Client（使用者 2026-10-23 同意加入）：用 O2 包住真正的產品 Client，連續啟動幾次，截前 10 秒。用來回答「在真正的遊戲裡看不看得到預熱幀、看得到多久」。照程式碼推論，產品在 `InitializeGraphics` 之後直接進入 `Run()` 的迴圈，只會露出約 1 幀，這點還沒量過。

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
| 大廳 | 整個內容區 ≥ 25% 是 HUD 底色（O2 加入，見下） |

分類（依序判斷）：

| 分類 | 條件 |
|---|---|
| `all_black` | 黑 ≥ 0.97（連武器和人物都沒有） |
| `uniform` | 單一非黑顏色（每通道 5 bit 分箱）≥ 0.9，例如空白視窗 |
| `warmup_like` | 背景黑、沒有 HUD |
| `black_with_hud` | 背景黑、有 HUD |
| `lobby` | 背景不黑、大廳的面板佔 ≥ 25%（O2 加入） |
| `world` | 背景不黑、有 HUD |
| `no_hud` | 背景不黑、沒有 HUD（大廳、讀回） |

**O2 加入 `lobby`（2026-10-23，任何 O3／O4 跑次之前）**：
- 冒煙時，產品 Client 的大廳被分成 `world`：大廳中央大面板的顏色和 HUD 底色相近，HUD 矩形又落在面板裡。
- 量到的面板色比例：大廳 0.47、世界（有 HUD）0.08、讀回 0.007。門檻定為 0.25。
- 這只是記錄用的分類，不是驗收判定。改動發生在任何觀測跑次之前，沒有違反「看結果之前宣告」。

**O4 開始前的唯一調整**：
- 在 O3 正常跑次的視窗截圖上，重新確認 HUD 底色與清除色；
- 如果不同，只在 O4 開始之前校正，並把新舊值記在本節；
- O4 開始之後不再調整。

## 5. 正常現象還是 bug（使用者 2026-10-23 的問題；以 H1 成立為前提）

- 使用者的看法：預熱是需要的，問題在於它暴露給了使用者。如果這是正常現象，要考慮的是怎麼隱藏這一段，而不是把該做的事搬到別的地方。怎麼解決之後另外討論。
- 已確認：
  - 暖機用一般的 `Renderer::Render`，這個 API 一次完成取得、繪製、送上螢幕；
  - 暖機迴圈要等到真的送上螢幕才算成功（`weaponPresented`），所以暖機幀上螢幕是設計本身帶來的結果；
  - 程式停止 Render 時，視窗顯示最後送出的那一幀，這是作業系統的正常行為。
- 判斷：機制正常，沒有違反契約。目前缺的是一條需求，也就是「預熱幀能不能讓使用者看到」沒有寫明。依 §14.3，這不算 Defect。
- O3 補充：露出的時間是 timing probe 自己造成的。它在暖機後先擺放視窗、讓視窗顯示，然後停約 3 秒不 Render。產品在暖機後直接進入迴圈，O4 會量產品看不看得到。
- 還沒排除、可能是真 bug 的部分：為什麼只是偶發；為什麼只有視野內沒有玩家的那一端整個黑。

## 6. 紀錄

- 2026-10-23：計畫第 1～3 版（使用者補充發生時刻、純黑、HUD 不見、武器可見、會恢復之後，找到 H1）。
- 2026-10-23：O1 完成。
  - 新增 `black_frame.py`：標準函式庫的 PNG／BMP 讀取器加上分類；
  - 新增 `test_black_frame.py` 共 14 個測試：讀取器、合成畫面的各分類、標題列裁切、2 張真實樣本；
  - 登記 CTest `object_fps_pvp.black_frame`（cpu 標籤，測的是工具本身，不是 L2 判定）；
  - 突變：`tests/object_fps_pvp/mutations.json` 的批次 `v71-black-map-o1`，13／13 killed（`run_mutations.py`）。另有一個 a/b 平手順序的突變是等價突變，因為 pa＝pb≤pc 時必然 a＝b，所以改成宣告 a/c 平手。
  - 既有證據的重判：69 張（58 張讀回加 11 張 PNG）沒有任何一張背景黑。讀回都是 `no_hud`，符合預期，因為讀回在 UI 之前。
  - 開發日誌見 [2026-10-23 O1](../../../dev_logs/2026_10_23_pvp_v71_black_map_o1.zh-Hant.md)。
- 2026-10-23：O2 完成。
  - 新增 `black_map_watch.py`：原樣執行命令；依 PID 樹找命令的視窗；視窗出現後前 10 秒每 0.5 秒截一次，之後每 5 秒一次；跑完後分類並寫出 `watch.jsonl`、`black-map-summary.json`。
  - 預檢：先用 `CGPreflightScreenCaptureAccess` 查權限（不會跳對話框），再截一張丟棄，接著等 1.5 秒看有沒有新的大視窗出現。
  - 其他程序出現的大視窗都記成 `foreign_window`（只記錄）。
  - 冒煙用主 checkout 的產品 Client（`a8a61ec8…`，sdl-isolation 的建置），`--gateway 127.0.0.1:9`，開 12～14 秒。證據在 `pvp-v71-black-map-o2-smoke-20261023/`。
  - **冒煙的發現與調整**：
    1. 預檢被選單列右上角 28×28 的螢幕錄影指示器擋下。它的 owner 是 Window Server，不是對話框。預檢和 `foreign_window` 改成只看 ≥200×200 的視窗。
    2. 啟動 `screencapture` 每張要 600～1400 ms，跟不上 0.5 秒的節奏；同時遊戲每次都出現約 220 ms 的 `slow swapchain acquire`。當時背景建置佔滿 CPU（負載約 20），數字偏高，但改成行程內截圖之後，每張中位數降到 136 ms、最長 198 ms，同樣負載下 `slow swapchain acquire` 也沒有再出現。
    3. 截圖改成 1 倍解析度，寫成 BMP 時不逐像素轉換。跑完後 `world`／`lobby`／`no_hud` 只保留每個視窗的第一張和最後一張，其他類別全部保留，量測值都留在 `watch.jsonl`。
    4. 大廳被誤分成 `world`，所以新增 `lobby` 分類（第 4 節）。
  - 產品 Client 冒煙 2 次（product-1 用 `screencapture`、product-2 用行程內截圖）：視窗出現後 0.1～0.3 秒的第一張已經是大廳，沒有 `warmup_like`。這只是冒煙，不算 O4 的觀測。另外記一筆：product-1 的 log 在 `graphics ready` 之後有 345 ms、741 ms 的幀間隔（`slow event processing`）。
  - 測試：`test_black_map_watch.py` 17 個（用假的系統跑完整主迴圈，不開 GUI），登記 CTest `object_fps_pvp.black_map_watch`；`test_black_frame.py` 加上 `lobby` 之後是 16 個。
  - 突變：批次 `v71-black-map-o2` 共 16 個（外掛 15 個、`lobby` 規則 1 個），16／16 killed；O1 的 13 個在改動之後重跑，仍然 13／13 killed。
  - 開發日誌見 [2026-10-23 O2](../../../dev_logs/2026_10_23_pvp_v71_black_map_o2.zh-Hant.md)。
- 2026-10-23：O3 完成。
  - 證據在 `pvp-v71-black-map-o3-20261023/`：`normal`、`h1-hold`、`timing-normal`、`m3`；每個案例都有 `patch.diff`、`head.txt`、`binaries.sha256`。
  - 暫時改動都在本 worktree 改完、重建、跑完之後還原，沒有提交；最後一次完整重建之後，原始碼和 HEAD 相同。
  - 一次無效的跑次：H1 測試第一次建置失敗（master 的 probe 沒有 `SleepFor`），但腳本照樣用舊的 probe 跑了。二進位的雜湊和 `normal` 相同，所以改名為 `h1-hold-invalid-build-failed-old-binary`，不計入。之後用 `SDL_Delay(10000)` 建置成功再跑。
  - 同一份原始碼重建出來的 probe，雜湊和第一次不同（`ed1115eb…`、`f223a7df…`），建置不是逐位元組可重現的。
  - 觀測跑次裡 runner 自己的結果不算數：player-short 全部 P；timing-gui-short 判 F，原因是 `combat-gui-evidence` 的 HUD HP／彈藥不符，和 sdl-isolation L2 裡凍結分析器的已知不符同類，與黑屏無關。
  - fixtures 加上 `window-warmup.png`、`window-m3-black-with-hud.png`，測試加 2 個（共 18 個）。
  - 開發日誌見 [2026-10-23 O3](../../../dev_logs/2026_10_23_pvp_v71_black_map_o3.zh-Hant.md)。

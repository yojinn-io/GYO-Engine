# PvP v7.1 black-map-observation O3：用已知樣本確認觀測，並驗證 H1

日期：2026-10-23。**完成**（O3）。
計畫見 [black-map-observation](../object_fps_pvp/plans/v7.1/black-map-observation.md) 第 2a 節；前一批見 [O2](2026_10_23_pvp_v71_black_map_o2.zh-Hant.md)。

## 做了什麼

- 用 O2 的觀測外掛包住 runner，在本 worktree 的建置（master 加上觀測工具，不含 sdl-isolation）上跑四組：
  - `normal`：正常建置的 player-short；
  - `h1-hold`：player-short 在量計時器基準之前多停 10 秒（暫時改動）；
  - `timing-normal`：正常建置的 timing-gui-short（`--gui --combat --short --duration 16 --events 20 --fps 60`）；
  - `m3`：世界幀的清除色改為 0、不送地板和牆，HUD 照畫（暫時改動），跑 timing-gui-short。
- 暫時改動在本 worktree 改、重建、跑完就還原，沒有提交。每組都留了 `patch.diff` 與二進位的雜湊。

## 驗證與經過

- **暖機幀第一次被拍到**：timing-gui-short 的兩個視窗從出現起，前 2.5～2.7 秒都是 `warmup_like`。內容是背景純黑、暖機時放在前方的人物背影、手上的武器、沒有 HUD，和使用者描述的症狀一致。
- **player-short 拍不到**：
  - 正常建置 8 個視窗 0 張；
  - 多停 10 秒之後，視窗出現在螢幕上的時刻跟著延後約 8.5 秒，出現時就已經是大廳或世界。
  - 所以在 player-short 裡，暖機到迴圈開始之間，視窗根本還不在螢幕上。
- **H1 修正**：暖機幀要露出來，視窗必須在停頓之前就已經顯示。timing 模式在暖機後先擺放視窗（`LatencyWindow`，含 `Sync`），讓視窗提早顯示，然後停約 3 秒量計時器基準；player-short 與 action-short 沒有這一步。
- **M3** 證明工具分得出暖機幀（沒有 HUD）和背景黑但有 HUD 的畫面。正常建置在迴圈開始之後沒有任何背景黑的截圖，所以截圖本身不會造成假的全黑。
- **意外**：
  - H1 測試第一次建置失敗（master 的 probe 沒有 `SleepFor`，那是 sdl-isolation 才加的），腳本卻照樣用舊的 probe 跑了。從雜湊和 `normal` 相同發現，改名標成無效、不計入。之後改用 `SDL_Delay(10000)`，腳本也加上 `pipefail`。
  - 同一份原始碼重建出來的 probe，雜湊和第一次不同，建置不是逐位元組可重現的。
- runner 自己的結果不算數：player-short 全部 P；timing-gui-short 判 F，原因是 `combat-gui-evidence` 的 HUD HP／彈藥不符，和 sdl-isolation L2 的已知不符同類。
- 還沒解釋的：「視野內沒有玩家的那一端整個黑」（O3 裡兩端的暖機幀相同）；使用者感覺到的偶發（依 O3，timing 每次都會露出、其他模式不會）。
- O4 的預測在看結果之前寫進計畫第 2a 節。

## 證據

- `build/target/_build/test/logs/pvp-v71-black-map-o3-20261023/`（`normal`、`h1-hold`、`timing-normal`、`m3`；無效的那次在 `h1-hold-invalid-build-failed-old-binary`；git 忽略）。
- 真實樣本：`tests/object_fps_pvp/fixtures/black_frame/window-warmup.png`、`window-m3-black-with-hud.png`。

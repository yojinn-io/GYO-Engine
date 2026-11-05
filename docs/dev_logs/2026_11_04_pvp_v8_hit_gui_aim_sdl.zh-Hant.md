# PvP v8：v8-product-hit-gui-aim-sdl（SDL 排程瞄軀幹的正式驗收）

日期：2026-11-04。分支 `claude/pvp-v8-after-pv8`。主會話 Opus 5.5／medium，沒有子 agent。估時 1～2 h，實際約 0.5 h（不含等待負載下降）。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hit-gui-aim-sdl-declaration.md)（`v8-product-hit-gui-aim-sdl-declaration-approve`，S1～S3，3 輪＋負面對照）。

## 前因

P2（`v8-product-hit-gui-aim`）讓 SDL 的 17 格排程（`combat_latency.hpp`）每幀轉向對手軀幹，但唯一用到它的 `run_timing.py --gui --combat` 一直報 HUD 不符，所以記成未結事項。`v8-product-hud-tick-offset` 查明那是 v7 起已知的凍結分析器誤判（D83／D84）：v7 第 04 批後 `hud_tick` 來自模擬角色的 snapshot，HUD 讀主執行緒最新的 snapshot，可差 1 tick；HUD 要用 `combat_gui_evidence_v7.py` 判定。P2 的探索跑次只看了凍結分析器，是我漏查。

## 判定

Intel Mac，執行檔來自本分支（master `d1d0db1e` 之後）的 `test` 建置，Gateway 由同一份程式重新編譯。命令：`run_timing.py --gui --combat --short --duration 16 --events 20`，每輪之後跑 `combat_gui_evidence_v7.py`。

- **S1、S2：通過（3／3）。**

  | 輪 | 開跑時 1 分鐘負載 | 移動 | 畫面延遲 | v7 | 其他錯誤 | 旗標 | 凍結分析器的 HUD 不符畫面 |
  |---|---|---|---|---|---|---|---|
  | s1 | 3.00 | 通過 | 通過 | 通過 | 0 | 0 | 21 |
  | s2 | 2.68 | 通過 | 通過 | 通過 | 0 | 0 | 25 |
  | s3 | 2.42 | 通過 | 通過 | 通過 | 0 | 0 | 7 |

  凍結分析器的錯誤全部是 HUD 不符的訊息；生命與重生驗證通過。`run_timing.py` 自己的結果因 HUD 判 false（已知行為，宣告的事實 2）。
- **S3（負面對照）：成立。** 把第 99 行的 `aim->pitch` 換成 `0.0f`（P2 之前的瞄法）跑 1 輪：凍結分析器的非 HUD 錯誤 1381 個，其中 1362 個是「snapshot combat state differs from the per-life replay」，另有「wrong v5 verdict」與「hit landed outside its predeclared target life」；v7 判 false。之後還原並重建。這支驗收抓得到「打錯部位」。

## 經過

- 開跑前兩次在 `run_timing.py` 的參數檢查就被拒，沒有開始量測：宣告的命令先寫成 `--duration 20`（`--combat` 只接受 16 或 120），再漏了 `--events 20`（`--short` 要求每個事件至少 600 ms）。兩次都照探索跑次的命令更正，判定條件不變，記在宣告的修訂紀錄。
- 等負載：Model Viewer 的會話建置時負載平均約 20，腳本等到 1 分鐘負載低於 4 才開跑。

## 證據

主 checkout 的 `build/evidence/v8-hit-gui-aim-sdl/`（Intel Mac）：`s1`～`s3`、`neg1` 的資料夾與輸出、`*-v7.json`、`load.log`、`run_rounds.sh`、被拒的兩次（`rejected-*`）。

## 範圍的限界

- 只在 macOS（Intel）跑；這支不在 CI。
- HUD 的判定沿用 v7 規則，允許 HUD 比主執行緒的 snapshot 落後不超過區間寬度（本次每個畫面最多 1 tick）。

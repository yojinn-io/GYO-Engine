# 工具 v8：ve-feel-survey（手感調查）

日期：2026-11-07。分支 `claude/v8-tool-voxel-editor-measure`。下段（只量不改）。實作：主會話（Opus 5.5）。
事前宣告：[voxel-editor-measure.md](../tools/plans/v8/voxel-editor-measure.md) 的 `ve-feel-survey`（2026-11-07 核准）。調查表：[feel-survey.zh-Hant.md](../voxel_map_editor/feel-survey.zh-Hant.md)。

**狀態：** FS1～FS4 成立（`cpu|shader` 138/138；L1 三平台通過（PR #170，2026-11-07，run 42377813964））。

## 做了什麼

- **量測命令**：
  - `measure-preview --scenario stroke [--brush-size n]`：每一輪照視窗的做法畫一筆 Brush，依序計時按下、拖曳一步、放開、undo；300 輪，丟前 30 輪。
  - `--orbit-distance <cells>`：近距離相機。
  - 失敗的輪會寫進 CSV（含失敗的階段），stderr 會印出按下時的拾取結果與射線。
  - CLI 參數測試 8 條。
- **差異表**：D1～D15，每列附 Goxel 的檔名與我們的出處（只比行為）。
- **你的回答**：H1～H8、Q10、最痛的三個，逐條記進調查表第 4 節。

## 量測結果（只描述）

本機 Intel Mac、RelWithDebInfo，Chrome 關閉。

| 圖 | 筆刷 | 按下 p95 | 拖曳 p95 | 放開 p95 | undo p95 | 件數 |
|---|---|---|---|---|---|---|
| building | 1 | 11.8 ms | 10.3 ms | 5.6 ms | 10.4 ms | 300／300／0 |
| building | 4 | 13.4 ms | 11.6 ms | 5.5 ms | 10.8 ms | 300／300／0 |
| km2-ground，近距離 330 格 | 1 | 101.2 ms | 12.4 ms | 7.0 ms | 12.5 ms | 300／300／0 |
| km2-ground，近距離 330 格 | 4 | 97.8 ms | 12.9 ms | 7.0 ms | 12.7 ms | 300／300／0 |

- km2-ground 按下的時間，主要是拖曳開始時的整份複製（`Drag::base`）。
- 俯瞰相機下，km2-ground 的 300 輪全部失敗，量到 0 件：
  - 相機離地圖中心約 2,546 格，超過拾取距離 1,024 格。
  - 你判斷遠距離編輯 km² 地圖不是合理的用法（HANDOFF `v8-tool-voxel-feel-km2-overview`），改用近距離量。
- 近距離量第一次確認時，40 輪有 4 輪失敗：
  - 診斷出 `Pick` 的缺陷，另開 `ve-pick-tie` 修好（[dev_log](2026_11_07_tool_v8_voxel_pick_tie.zh-Hant.md)）。
  - 修好後才量上表。

## 你的回答（摘要；全文在調查表）

- **不是問題（目前）**：
  - H1 延遲；
  - H2 命中；
  - H4 undo；
  - H6、H7：正式製作地圖時再看。
- **是問題**：
  - H3：朝向看不出來，也找不到改朝向的地方。
  - H5 相機：
    - 縮放要有上下限，上限依地圖大小、拉遠時繪製距離跟著擴大；km² 要等 ②。
    - 縮放要有滑桿與鎖定。
    - 多層建築裡相機穿牆。
  - H8：要顯示相機的朝向，照 Goxel 角落的 X／Y／Z 軸箭頭。
- **決定**：
  - Q10：Line 照 Goxel 改成拖曳。
  - Q12：方向是自由相機，用來處理大地圖與多層建築的內部。
- **最痛的三個**：
  1. 縮放；
  2. 左邊面板的功能塊混在一起；
  3. 多層建築的編輯。

## 和宣告的差異

1. **情境的跑法**：不分四次跑，改成 `--scenario stroke` 每一輪依序計時四段。這是看結果之前定的，次數與丟棄規則照宣告。
2. **km2-ground**：
   - 俯瞰量不到，記為不是目標（你的判斷）。
   - 改用近距離 330 格（你選「近距離で測る」）。這是跑之前補進宣告的。
3. **Q10、Q12**：照核准的試法 (a)，本批不做第二種。Q12 你直接給了方向（自由相機）。

## 證據

主 checkout `build/evidence/tool-v8-voxel-map/editor-next/feel/`：
- `building-b*.{csv,out}`；
- `km2-ground-b*.*`：俯瞰，0 件；
- `km2-near-check*.*`：診斷；
- `km2-near-after-fix-check.*`；
- `km2-near-b*.*`；
- `trial-ui/`：你試用時的副本；
- `environment.txt`。

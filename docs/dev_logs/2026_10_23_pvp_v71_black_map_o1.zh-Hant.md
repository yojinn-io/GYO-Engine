# PvP v7.1 black-map-observation O1：把遊戲視窗截圖分類的分析器

日期：2026-10-23。**完成**（O1）。
計畫見 [black-map-observation](../object_fps_pvp/plans/v7.1/black-map-observation.md)；未結事項 `N-gui-black-map-intermittent` 見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)。

## 做了什麼

- 目的：為 GUI 測試偶發的黑屏建觀測，只記錄。不修正，也不改任何既有判定。
- 新增 `build/acceptance/object_fps_pvp/black_frame.py`：
  - 讀 PNG（8 bit RGB／RGBA、非交錯）與未壓縮的 BMP，只用標準函式庫，因為環境沒有 PIL、numpy；
  - BMP 按需解碼取樣點，給 O2 的大量截圖用；
  - 每張圖在內容區取 160×90 個點，算黑、清除色、HUD 底色的比例，分成 `all_black`、`uniform`、`warmup_like`、`black_with_hud`、`world`、`no_hud`。
- 分類規則與門檻在任何觀測跑次之前寫進計畫第 4 節。

## 驗證與經過

- 方案經過三版。
  - 第 1 版比較「只截視窗」與「probe 內讀回」兩條路。
  - 使用者補充「純黑、剛開始、會恢復、HUD 不見、武器可見」之後，發現症狀和 Initialize 的暖機幀一一對應（假說 H1，未證實）。黑屏那段時間沒有 Render，所以讀回在原理上抓不到，第 3 版只截視窗。
- 校準：從既有證據量出螢幕上的天空 (61,73,88)、讀回的天空 (56,72,89)、螢幕上的 HUD 底色 (43,51,62)。
  - 從螢幕截圖切出 create 視窗：`world`，HUD 底色比例約 0.9；
  - 既有 69 張圖都沒有背景黑。
- 測試 14 個：
  - 讀取器：PNG 的五種濾波、RGBA、Paeth 的平手；BMP 的上下方向、32 bit bitfields、列補齊；
  - 合成畫面的各分類與標題列裁切；
  - 2 張真實樣本，最近鄰縮小，放在 `tests/object_fps_pvp/fixtures/black_frame/`。
- 突變：
  - 第一次 12／13，存活的是 Paeth 的 a/b 平手順序。推導後確定是等價突變：pa＝pb≤pc 時必然 a＝b；
  - 改成 a/c 平手的突變之後，偽隨機資料仍然沒有碰到這種情況，所以補了一個專門的 2×2 測試；
  - 最後用 `run_mutations.py --batch v71-black-map-o1` 跑，13／13 killed，`test_run_mutations.py` 也通過。
- 測試登記到 CTest `object_fps_pvp.black_frame`（cpu、pvp、acceptance 標籤）。測的是工具本身，不是 L2 判定。

## 證據

- 既有證據：`build/target/_build/test/logs/pvp-v71-gui-black-map-20261023/`、`pvp-v71-sdl-isolation-l2-20261023b/`（git 忽略）。
- 突變報告：只在會話的 scratchpad，結果記在本文。

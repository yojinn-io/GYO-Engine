# 第 06 批：pvp 改經 Engine 取得輸入（第11項的產品端）

狀態：完成（2026-10-05；見 [dev_log](../../../dev_logs/2026_10_05_pvp_v6_batch06.zh-Hant.md)）。依賴 Engine IP-1 與第 04 批完成。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md)，以及 Engine 的
[input-and-present 計畫](../../../architecture/plans/input-and-present/PLAN.md)（IP-1 的介面）。

## 目標與範圍

pvp Client 目前在 `HandleNativeEvent` 直接處理 `SDL_Event`。本批改用 IP-1 提供的 Engine 輸入介面
（IP-1 涵蓋同幀上升沿、事件順序、點擊座標、縮放事件（含新寬高）；釋放指標仍是本產品的擷取政策，見 IP 計畫），
**擷取政策的行為不變**。何時擷取或釋放指標仍由 pvp 決定，不進 Engine。

做：

- `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` 的 `HandleNativeEvent`（:188-237）改用 IP-1：
  - windowID 過濾（:193-198）。
  - 失焦／移動／縮放／縮小時釋放指標（:199-204）。
  - Tab 切換擷取（:205-208）。
  - 左鍵點擊擷取、視窗內範圍檢查與射擊上升沿（:209-219）。
  - 縮放時更新寬高（:188-191）。
- GUI probe 4 檔改用 IP-1 中適用的部分：`build/acceptance/object_fps_pvp/gui_main.cpp`、`player_short.hpp`、
  `action_short.hpp`、`combat_latency.hpp`。`weapon_short.hpp` 已在 02a 刪除，略過。
  - probe 以 `SDL_PushEvent` 注入合成事件（例：`gui_main.cpp:148-179`），並以 SDL event watch 觀察
    （`player_short.hpp:17-40`）。注入本身是被量測的輸入路徑，保留在 SDL 層。
  - 哪些部分改用 IP-1、哪些保留 SDL，開始時逐檔列出，經確認後再改。
- 刪除被取代的 `SDL_Event` 直接處理。

例外（保留原生處理，完成條件中明列）：

- 文字輸入與剪貼簿：`PvpApplication.cpp:157-160`（`SDL_StartTextInput`／`SDL_StopTextInput`）與
  `:223-236`（`SDL_EVENT_TEXT_INPUT`、Backspace、Ctrl+A、Ctrl+V、Return／Esc 結束編輯）。這是 Engine 候選項目（D11②），不在 IP-1。

不做：

- 不改 wire、Match、權威邏輯。
- 不改其他產品。`object_fps_preview`（`SDL_SCANCODE_1..6`）只寫進 Engine 的遷移清單；FF-6 會停用它。
- 不改 Engine。IP-1 介面不足時停下（見停止條件）。
- 不改 Linux X11 runner `run_native_window.py`（其 HP 歸零段已在 02a 處理）。

## 交付

- pvp Client 與 GUI probe 的改用。
- L1 characterization 測試與事前宣告的差異表。
- 改用後的 grep 紀錄：除了文字輸入與剪貼簿，pvp `src` 不再處理被取代的 SDL 事件。
- 本批 dev_log（`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch06.zh-Hant.md`），README／HANDOFF 更新。
- 消費端結果以文字摘要寫回 Engine 的 [input-and-present HANDOFF](../../../architecture/plans/input-and-present/HANDOFF.md)。

## 驗收點

- L1 自動：
  - characterization：同一串合成 SDL 事件，在改用前後產生相同的 `PhysicalInputFrame`、擷取狀態與命令序列。
  - 逐條比對 Engine 端既有行為：`engine/input/backend/sdl/src/SdlInput.cpp:94-107` 的 FOCUS_GAINED 壓制相對位移、
    FOCUS_LOST 清除 delta 並 `ReleaseAll`。
  - 前後差異逐條事前宣告，每條附突變測試；未宣告的差異即失敗。
  - pvp CPU 測試全部通過。
  - 權威 digest：以 [第 03 批](03-authority-digest.md) 的 runner 同機兩樹比對，預期相同。
    該 runner 不經 SDL 輸入，只作附帶檢查；本批的主要證明是 characterization。
- L2 實機：在本批 base commit 與 branch 上以凍結工具量 before／after（動作短測、人物短測，跑次事前宣告）。
  不與 IP-2 的實機量測同時進行，以免無法歸因。
  基線世代：一律以本批 base commit 為 before；B0（第 04 批）只作歷史參照；門檻只對 v5 STABLE_BASELINE 的數字。
- L3 人工：macOS 由使用者執行 [v5 第04批原生操作清單](../v5/04-complete-action-presentation.md) 1–8，
  重點是第1項（擷取／釋放／重新擷取）與第6項（Tab、失焦、拖動標題列、縮放、關窗後重新加入）。

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（L2、L3 原生操作清單） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 未執行：pvp 沒有宣告 GPU 檢查（checks.json 的 gpu 為 false）；CI toolchain 列的共通 render.* 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行（v6 實機只有 macOS Intel，D11⑩） |
| Linux 實機 | 未執行（同上；X11 的 `run_native_window.py` 可選，未授權即標未執行） |
| macOS arm64 實機 | 未執行（同上） |

## 建議檔位

建議 high。跨檔案的產品改用，要先讀懂既有擷取狀態機與 probe 的注入方式；有 L1／L2／L3 做外部驗證。
不升 xhigh。依 D8，開始時說明檔位；高於主對話的檔位需使用者同意。

## 依賴與合併順序

- 依賴 IP-1（Engine 輸入介面）與第 04 批。
- `PvpApplication.cpp` 的建議合併順序：05→06→12→11→13。這只是建議，用來減少衝突與方便歸因，不是依賴；
  順序不同時由後合併的一方 rebase。

## Architecture Delta

無新增。產品改用 Engine 既有方向（Application→Engine）的介面，不新增依賴邊、不改 Ownership。
pvp 對 SDL 的直接依賴減少；仍保留的直接依賴（文字輸入與剪貼簿、probe 的事件注入）在 dev_log 明列。
這項變化記錄在 IP-1 的 Fitness，不在本批另立 Delta。

## 執行規則（沿用 v5）

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

- 只做使用者指定的本批；一批一個 PR，commit 與 PR 用日語。
- 開始、里程碑、停止時更新 README／HANDOFF／dev_log。
- 先凍結來源／產物／分析器再量測；開發與乾淨量測不同時進行。
- 長測另外授權；本批不跑長測。

## 完成與停止

完成條件：

- characterization 通過，差異都在事前宣告內並有突變。
- grep 紀錄只剩明列的例外。
- 同機兩樹 digest 相同；CI 通過；before／after 結果寫入 dev_log。
- L3 由使用者完成並記錄。
- 更新 README／HANDOFF／dev_log 與 Engine HANDOFF 的消費端摘要後停止，不自動開始下一批。

停止條件：

- IP-1 介面實際上無法保持現有行為（IP-1 已宣告涵蓋同幀上升沿、事件順序、點擊座標、縮放事件（含新寬高）；
  實作與宣告不符，或出現上述以外的缺口）：停下，回報缺口，交回 IP 計畫；
  不在 pvp 端用 SDL 補洞，也不在本批改 Engine。
- characterization 出現未宣告的差異：停下回報，不事後放寬。
- 失敗跑次保留，有限定位後停下；不覆寫、不無限重跑。

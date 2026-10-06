# PvP v7 任務清單（未開始）

更新：2026-10-06。Owner：`object_fps_pvp`。**v7 尚未開始**：v6 完結後開始。本文件只收集已決定放進 v7 的任務與規劃輸入，還沒有分批、契約或檔位。
v7 開始時，依變速箱規則以 ultracode 規劃：先做一份唯讀盤點（所有用到時間、執行緒、sleep、socket 輪詢的地方，含 Engine、產品、工具），作為下列子系統的「觀測到的壓力」證據，再分批。

版本號：v7＝遊戲版本，pvN＝網路協議版本（見 [v6 交接](../v6/HANDOFF.md) D20）。

## 已決定的任務

| # | 任務 | 來源 |
|---|---|---|
| 1 | 主執行緒（事件＋畫面）、模擬、網路三個角色分離 | v6 D19（IP-2 的縮放停頓、`nextDrawable` 停頓） |
| 2 | 移動命令在固定步邊界產生，不再等畫面幀；相位追蹤處理每一份 snapshot 的樣本（目前每幀只用最新一份，30 FPS 約一半樣本到不了）。根本處理 30 FPS 的相位餘裕缺口 | v6 D21、第 07 批後續的 A/B 調查 |
| 3 | 網路路徑的短休眠輪詢改為事件驅動，不依各 OS 的計時粒度：Match IPC 每輪 1 ms（`IpcHost.cpp:267`）、Client 網路 worker 每輪 2 ms（`ClientConnection.cpp:567-570`） | v6 D21 |
| 4 | runtime link（Gateway↔Match）的心跳對時：偏移、RTT、漂移。只作量測與診斷，遊戲邏輯不依賴對時（權威仍以 Tick 與序號運作）。屬 wire 變更 | 2026-10-06 使用者決定 |
| 5 | 音效：Engine 新增 audio 模組，本產品加入射擊、受擊等聲音 | 2026-10-06 使用者決定（「有聲音遊戲才算完整」） |
| 6 | 解析度與視窗模式切換，設定可保存。Engine 提供機制（顯示器與模式列舉、視窗／無邊框全螢幕／全螢幕、執行中改變大小、像素密度、實際繪製大小，以及每位使用者的可寫目錄與原子寫入）；本產品決定政策（提供哪些選項、預設值、設定選單、套用後未確認就還原、設定檔的格式與版本、UI 依解析度縮放；目前寫死 1280×720，`PvpApplication.cpp:121`、`:876`） | 2026-10-06 使用者決定 |
| 7 | 產品不再直接依賴 SDL：產品只呼叫 Engine，由 Engine 呼叫 SDL。現存的直接使用（2026-10-06 盤點）：日誌 `SDL_Log*`；文字輸入與剪貼簿（`SDL_StartTextInput`／`StopTextInput`、`SDL_GetClipboardText`、文字事件與修飾鍵，`PvpApplication.cpp:165`、`:220`）；等待 `SDL_Delay`（`:742`、`:990`）；視窗大小 `SDL_GetWindowSize`（`:1006`）；執行檔目錄 `SDL_GetBasePath` 與進入點 `SDL_main`（`main.cpp:6`、`:37`）。驗收 GUI probe（`gui_main.cpp` 等 7 檔）直接連結 `GYO::PlatformSDL` 並以 `SDL_PushEvent` 注入事件，需要 Engine 提供測試用的事件注入介面。完成條件：產品與其驗收 probe 的原始碼不 include SDL 標頭（CMake 的後端元件選擇除外） | 2026-10-06 使用者決定 |

## 需要的 Engine 子系統（Engine 另立計畫）

依賴由下而上。都屬 Architecture Delta，規劃時逐一說明 AGENTS.md §3 的七點；Engine 計畫文件不連結本產品文件（D12）。

| 子系統 | 負責 | 目前散落的地方（壓力） |
|---|---|---|
| Time | 單調時鐘、等待原語（等到期限或事件發生就醒）、固定步時鐘（絕對格點、補步與丟步規則）、晚醒量測、各平台高精度計時 | `IpcHost`、`ClientConnection`、`RuntimeLoop`／`FixedTickRuntime`、驗收 probe 的 `sleep_until`、渲染取得畫面的計時 |
| Threads／Channels | 執行緒生命週期與停止、各角色的迴圈、跨執行緒佇列與狀態交接 | `ClientConnection` worker、`IpcHost`、`MatchRuntimeHost`、`MovementTraceWriter` 各自開執行緒；任務 1 的三個角色 |
| Net transport | 事件驅動 socket（包裝 asio）、framing、心跳與對時 | Engine `net` 目前只有 `GyopDatagram`；兩條傳輸都在產品內且為輪詢 |
| Trace | 結構化、帶統一時間戳的事件記錄與背景寫檔；日誌、量測、錄製重播共用 | 產品的 `MovementTraceWriter`、`PvpApplication` 與渲染的慢事件日誌 |
| Audio | 音效播放與混音、即時執行緒。音效事件帶時間戳（Time 的單調時鐘），經佇列送給音訊執行緒，由它排程到對應的樣本位置播放；不由幀迴圈觸發（使用者 2026-10-06 指定） | 尚無（任務 5）。若由幀迴圈觸發，30 FPS 時連射的聲音會被量化成 33 ms 一格；與 30 FPS 移動命令（任務 2）是同一類問題 |
| Display（擴充 platform） | 顯示器與模式列舉、視窗模式切換、執行中改變大小、像素密度；render 端依新大小重建繪製目標 | `SdlPlatformOptions` 只有建立時的寬高（`SdlPlatform.hpp:35-38`），沒有切換；產品不得直接呼叫 SDL（第 06 批的方向）；工具（ui_editor、preview）也有視窗，是第二個使用者（任務 6） |
| SDL 隔離層（擴充 platform、input） | 日誌、文字輸入與剪貼簿、執行檔與使用者目錄、進入點、測試用事件注入；與 Time（等待）、Display（視窗大小）分工 | 任務 7 的盤點 |
| 使用者設定的保存（擴充 io） | 每位使用者的可寫目錄、原子寫入、讀取失敗時退回預設。鍵與值的意義、版本與驗證規則屬各產品的 Data Contract，Engine 不知道有哪些設定 | 目前沒有任何保存機制（任務 6） |

- 錄製與重播成立的前提：時間、輸入、網路都經過 Engine（Time、Input、Net transport），由 Trace 記錄。
- Job（工作分割、平行運算）目前沒有壓力，不建；出現平行運算需求時建在 Threads 之上。

## 驗收與方法

- 跨平台實機驗證：Windows、Linux，以及兩台機器之間的時鐘漂移（v5 起從未實測）。Time 子系統以此為驗收條件。
- 30 FPS 與主機約 8 ms 晚醒的情況要納入驗收（v6 D21）；30 FPS 長局實驗在分執行緒後再考慮。
- 可重複使用的效能比較方法：多次跑次、交錯順序、依主機狀態分層、事先宣告判定規則（v6 第 07 批的教訓：單次比較會誤判）。

## 候選（未定案，有需求時再開）

- 非同步資產載入（Job 的第一個實際需求）。
- 當機回報：當機時收集最後的 trace、版本、平台。
- 單機劇情＋聯機選項：本機執行同一套權威模擬（見 v6 交接）。
- PvP 骨骼 hitbox（2026-10-07 使用者提出）：部位判定、爆頭倍率。
  - 前置條件：Match 端的權威姿勢；跨平台決定性的姿勢取樣；射擊回溯；wire 新增命中部位欄位（屬於協議變更）；v6 第 11 批的俯仰瞄準；修改契約 §6 的 Hit_Head 規則。
  - 不在 v6 第 10 批做的原因：會違反 D11①（不改 wire）；Match 目前沒有模型與動畫的依賴；會破壞第 10 批「差異只來自數值統一」的歸因證明。
  - Engine 端已鋪路：FF-9 之後，球形 `VerticalCapsule` 與長度 0 的 `Math::Capsule` 都合法，射線與球掃掠的 `Math::Capsule` 版不變。

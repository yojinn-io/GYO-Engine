# 2026-11-25 遊戲流程框架第 5 段：設置（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（遊戲流程框架）。作者 Opus 5.5／high。分支疊在第 4 段（#351）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md)第 3.6 節與第 4 節第 5 段；使用者 `v8-arch-gyo2-settings-resolution-fullscreen`、`v8-arch-gyo2-window-mode-vsync-start`。

## 做了什麼

1. **設定的資料**（`include/object_fps_pvp/client/Settings.hpp`）：`ClientSettings{resolution, window, vsync}`；解析度只有四種（1280×720、1600×900、1920×1080、2560×1440）；檔案 `{"version":1,"resolution":[w,h],"windowMode":"windowed"|"borderless","vsync":bool}`（JSON Module）；讀不到或不是設定檔時用預設值並說明原因。
2. **主菜單的「設定」**（`pvp_menu.json` 的 `settings` canvas，同一個場景換 canvas）：解像度（按一下換下一種）、画面（ウィンドウ／全画面（枠なし））、VSync（オン／オフ）、適用、戻る。適用：尺寸不同時送 window 的 `ResizeWindow`，存檔，顯示結果。在用的設定是 Frame World 的 Resource，`CarryOver` 帶過場景。
3. **程式**：啟動時讀 `<使用者資料目錄>/GYO/object_fps_pvp/settings.json`（一台機器一份，不分 profile），視窗用它的尺寸開；讀不到就記一行警告、用預設值。
4. **視窗模式與 VSync**（「視窗模式＋VSync」卡 #344 與「內插＋後座力」卡 #345 合併進 gyo2 之後接上）：設定直接用 window 的 `WindowMode`（檔案照舊寫 `"windowed"`／`"borderless"`）；啟動時給 `WindowSettings::mode`；適用時送 `SetWindowMode` 與 `ResizeWindow`（每次都送）；Render World 的 Extract 把 VSync 抄進 render 的 `Presentation`（`CopyPresentation`）。
5. **頁面改用 `state_machine`**（宣告 3.7，主會話轉達使用者「master 頁面跳轉都是一堆 if else」）：`client/Pages.hpp` 的 `MenuPage{Main, Settings}`、`GamePage{Loading, Playing, Paused}`。頁面的 enter／exit schedule 放與拿 UI 根；頁面的 System 用 `InState`；按鈕與條件（loading 就緒且開始、指標被拿走或還回）送 `StateMachineRequest::Request`。原本設定是換 canvas、loading 與暫停是每幀照條件挑 canvas（`ShowPause`），都拿掉。場景之間仍是 `ReplaceWorld`。
6. **房間的地圖帶進遊戲**（大地圖卡要的接點）：`SceneChange::map` 帶房間的 `map`（arena 的 id）；Sim、Solo、Frame 照它找 arena 的 index；沒有那張圖或沒帶時用 Client 自己的。`kiriyu_extraction_250m` 放進 `arenas.json` 後會自動出現在大廳的「マップ」切換裡。
7. **使用者的兩個決定**（2026-11-26 夜，`v8-arch-gyo2-decisions-2026-11-26-night`，宣告修訂 6）：
   - 解析度改實體像素：`WindowSizeFor(physical, scale)` 換成邏輯像素再送 `ResizeWindow`；`FitWindow`（每個場景，`WindowFit` 用 `CarryOver` 帶）在視窗說出 scale 或 scale 改變時送一次；適用照現在的 scale 換算。
   - 單人房間：和多人用同一份房間文件（`pvp_room.json` 的 `solo_room` canvas），按鈕「開始」「マップ」「戻る」；模式固定「生存」；選的地圖經 `SceneChange::map` 進 Solo World；中止回房間時帶回那張圖。多人房間的模式與地圖移到兩列。`pvp_solo_room.json` 刪除。

## 驗證

- 第一版（審查前）：本機（Intel Mac）`ctest -L "cpu|gpu"` 256/256、`-L "go|network"` 5/5。
- client：編碼往返、不提供的尺寸與別的版本讀不進、下一種尺寸繞回、壞檔回預設並說明；主菜單 → 設定 → 尺寸換兩次（1920×1080）、全画面、VSync オフ → 適用：送出一次 `ResizeWindow{1920,1080}`、存檔、讀回相同；換場景後設定還在。
- 突變：`pvp-settings-no-resize`、`pvp-settings-not-saved`、`pvp-settings-not-carried`、`pvp-settings-any-size`，加 `pvp-menu-multi-ignored`：5/5 killed。

## 設計審查（Opus／high，作者同模型，審查者只拿成果物與原始要求）

範圍 `b8561486..2abd5b48`。blocker 0、should 4、nit 9。

- S1（啟動時讀設定、套到第一個 World 沒守）→ `Setup` 加 `adjust`（改 `ClientStart`）；新案「啟動的設定是第一個場景的；沒適用的改動離開設定頁就丟」：非預設設定 → 第一個 Frame World 的 `ClientSettings` 與 Render 的 `Presentation`、設定頁顯示那個值。突變 `pvp-settings-start-ignored`。`client_main.cpp` 從資料目錄讀檔那幾行仍由實機守。
- S2（解像度是邏輯像素，Windows 顯示縮放 125% 時 1920×1080 比 1080p 螢幕大）→ 不自己改：使用者定的是「送現有的 `ResizeWindow`」；宣告修訂 5 寫明，回報主會話問使用者要不要改成實體像素（產品照 `WindowState::scaleFactor` 換算即可，不改 Engine）。
- S3（宣告寫左右切換，實作按一下換下一種）→ 宣告修訂 5 改成「按一下換下一種（繞回）」：只有四種。
- S4（接上 #344 時的三件）→ (a) 拿掉 `WindowChoice`，直接用 `WindowMode`；(b) 進設定頁時畫面一列照視窗實際的 `WindowState::mode`（玩家用 OS 的按鈕切過時）；(c) 無邊框時解像度一列標「（ウィンドウ時）」。宣告修訂 5。
- N1（`MenuActions` 沒用的 `UiBindings` 寫入）→ 拿掉。N2（尺寸沒變不送）→ 每次適用都送。N3（「進設定從生效值重來」沒守）→ S1 的新案後半；突變 `pvp-settings-not-dropped`。N4（`ClientStart` 一處斷言一處三元）→ 統一成斷言。N7（註解說有 mode 的文字）→ 改註解。N9 → `Setup` 的 `adjust` 取代繞一次 Menu 的寫法之外，測試同時記 `SetWindowMode`。
- 不改：N5（標籤字串在 C++：產品現有的文字也在 C++，換語言時一起搬）、N6（UI 檔的排版：已合在同一個 commit，不拆）、N8（一個欄位錯整檔回預設：有 `version`，記成取捨）。
- 新突變：`pvp-settings-no-mode`、`pvp-settings-no-vsync`、`pvp-settings-not-dropped`、`pvp-settings-start-ignored`、`pvp-page-loading-stays`；頁面改寫後 find 變了的突變重跑：20/20 killed（`pvp-pause-not-shown`、`pvp-loading-not-shown` 第一次是 wrong_failure：頁面的請求下一幀才套用，最先失敗的是沒有 tag 的 `REQUIRE`；把那兩個 `REQUIRE` 加上 tag 後 killed）。

- 審查與頁面改寫之後：本機（Intel Mac）`ctest -L "cpu|gpu"` 258/258、`-L "go|network"` 5/5；client 29 案全過。
- 地圖：Gateway 的案在大廳選第二張圖 → 房間 → 遊戲：`SceneChange::map`、Sim 與 Frame 的 `GameRules` 都是那張圖（`[pvp-gw-map*]`）；突變 `pvp-map-not-carried`、`pvp-map-sim-ignored`、`pvp-map-frame-ignored` 3/3 killed。
- 修訂 6：新案兩個——「解析度是實體像素」（`WindowSizeFor` 的換算；視窗說 125% 時送一次 1536×864；適用也照 scale；測試的游標原本停在 1280×720 視窗的中央，換了大小會停到別的按鈕上，所以移到角落）；「單人房間是多人的房間」（`solo_room` canvas、模式固定、選第二張圖 → Solo World 玩那張、中止回房間仍是那張）。Gateway 的案多檢查房間顯示建房時的地圖。新突變 `pvp-solo-map-ignored`、`pvp-solo-map-not-cycled`、`pvp-solo-map-not-kept`、`pvp-room-map-not-shown`、`pvp-settings-physical-ignored`、`pvp-settings-apply-logical`；find 變了的 `pvp-solo-start-ignored`、`pvp-settings-no-resize` 改 find 重跑。

## 沒驗

- 真的視窗換尺寸、比例（UI 的 fit 與相機）、視窗模式與 VSync 的實際套用：交使用者試玩。
- 程式啟動時讀設定（`client_main.cpp`）：沒有自動測試，實機看。

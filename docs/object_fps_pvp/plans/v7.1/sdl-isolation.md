# sdl-isolation：產品、probe、產品測試不直接呼叫 SDL（消費端計畫，草案）

- 狀態：**核准**（D160；Engine 側為 IP 的 D31）。ip3a 核准（S3 選 (a)，D161）；進行中：ip3b。
- 依據：
  - [after-re-evaluate](after-re-evaluate.md) 第 9 節的 `sdl-isolation-plan`、`sdl-isolation`（D158、D159）；
  - v7 的 P4 與第 12 批（[v7 HANDOFF](../v7/HANDOFF.md) D33、P4 的原文；[v7 README](../v7/README.md) 的 PR 線表）。
- Engine 側：[輸入與呈現計畫](../../../architecture/plans/input-and-present/PLAN.md) 文末的 IP-3／IP-4 草案。Engine 的 API、契約、Design Invariants 與待決項（U1～U10）都在那裡；本文件只寫產品這一側。
- 規劃方式：2026-10-21 用 ultracode（D159 的決策點 B），過程見 Engine 的 [HANDOFF](../../../architecture/plans/input-and-present/HANDOFF.md)「IP-3／IP-4 規劃紀錄」。對抗式檢查指出，產品的工作清單原本寫在 Engine 的 PLAN 裡，屬 Ownership 漂移，所以移到這裡。
- file:line 以 master `6fbb6a9` 為準。

## 1. PR 與順序

- 一個 PR：IP-3、IP-4 與本計畫的產品遷移一起合併。這沿用 v7 的 P4「標頭去 SDL、PRIVATE 連結與產品遷移必須一起合併」，也符合 D159 的 PR 線。分支：`claude/pvp-v71-sdl-isolation`。
- 順序：Engine 的每個里程碑完成後，再做本計畫對應的產品部分。下表的「時點」就是那個里程碑。

## 2. 產品的工作

| 時點 | 檔案 | 工作 |
|---|---|---|
| ip3b | `PvpApplication.cpp:940-945` | pump 改用 `PlatformEvent`、`HandleEvent(const PlatformEvent&)` |
| ip3b（暫時） | `PvpApplication.cpp:1205`；probe 的 `.NativeWindow()`（`gui_main.cpp` 27 處等）；`PointerCaptureCharacterizationTests.cpp:92-104` | 機械修改：改連 Native 目標，IP-4 再改掉 |
| ip3d | `PvpApplication.cpp:186-190`、`:233-250` | 改用 `SetTextInput`；`HandleTextEditing` 改在 pump 之後讀 `text`；剪貼簿用 `ClipboardText` |
| ip3e | `main.cpp:9`、`:67` | 改成 `GyoMain`，連結 `GYO::PlatformSDLMain` |
| ip3e | `main.cpp:95`、`:120` | `BaseDirectory`、`PlatformName` |
| ip3e | `main.cpp:24-50`、`:68`、`:103-116` | 刪掉 Engine→SDL 的 `ForwardEngineLog`，改成 Base 的 handler 寫進 LogFile，再建立 `SdlLogBridge`（預設 tee）。**刪除 `ForwardEngineLog` 是建立橋的前提**（Engine Design Invariants 第 5 條） |
| ip3e | `PvpApplication.cpp:224`、`:914`、`:1189` 的 `SDL_Log*`、`SDL_GetTicks`、`SDL_Delay` | 改用 `Base::Log`、`Time::NowNs`、`Waiter` |
| ip3e | `tests/object_fps_pvp/PlayerPresentationTests.cpp:20,44`、`CMakeLists.txt:82-83` | 改用 `BaseDirectory`，拿掉直連 SDL3 |
| IP-4 | `PvpApplication.cpp:1205` | 改用 `QueryWindow` |
| IP-4 | probe 8 檔：`gui_main.cpp`、`gui_quad_main.cpp`、`gui_input.hpp`、`action_short.hpp`、`player_short.hpp`、`native_window.hpp`、`platform_fingerprint.hpp`、`combat_latency.hpp` | 改用注入器、`AddEventWatch`、查詢與操作、capture 存檔、`Waiter`；SDL 的整數 typedef（`player_short.hpp:5-6`、`gui_main.cpp:263,287` 的 `Uint32`／`Uint64`）一起換掉 |
| IP-4 | `gui_main.cpp:915-918`、`action_short.hpp:366` | 合成的 MOVED 改成真正移動視窗（Engine 不提供合成；證據語意改變，事前宣告） |
| IP-4 | 計時基線的標籤 `"SDL_DelayNS remainder"`（`platform_fingerprint.hpp:31-36`），由 `test_presentation_evidence.py:526` 斷言 | 改名照 AGENTS §14.4 報告 1～5 |
| IP-4 | `PointerCaptureCharacterizationTests.cpp` 的凍結 oracle | 依 Engine 的 U6（建議改 golden，預設視為有放寬的一面，走 §14.4 的核准流程） |
| IP-4 | `PvpApplication.hpp:129-130` 公開的 `SdlPlatform&`／`SdlGpuRenderDevice&` | 見 S1 |
| IP-4 | 完成條件的守衛（第 3 節） | 新增，在 `tests/object_fps_pvp/CMakeLists.txt` 登記，只在選擇本產品時啟用 |

`SDL_VIDEODRIVER=dummy`（`tests/object_fps_pvp/CMakeLists.txt:99`、`:116`）是環境設定，照 v7.1 的 D87 保留（不是移除 SDL）。

## 3. 完成條件的守衛（v7 D33：以符號判定，例外清單從空開始）

對抗式檢查（H3）指出，原本「掃 app、probe、測試三個目錄」的寫法會誤報合法內容（字串 `"SDL_DelayNS remainder"`、`SDL_VIDEODRIVER`、元件名 `SDL_GPU`），又會漏掉 SDL 的整數 typedef，而且守衛自己沒有突變。所以改成兩層：

- **(a) 原始碼**：只掃 C／C++ 原始碼與標頭（`apps/object_fps_pvp`、`build/acceptance/object_fps_pvp`、`tests/object_fps_pvp`），先去掉註解與字串字面值；比對 `\bSDL_\w+`、`\bSDLK_\w+`、`\bSDLCALL\b`、`<SDL3/`、`\b[US]int(8|16|32|64)\b`。整數 typedef 的規則只套用在 C++ 檔，避免碰到 Go 的 `atomic.Uint64`。
- **(b) 連結圖**：本產品的所有 target，自己的 `LINK_LIBRARIES` 都不含 `SDL3::SDL3`、`SDL3_image` 與任何 `*Native`。檢查的是目標自己的連結，不是連結閉包：Engine 後端的 `$<LINK_ONLY:>` 會出現在閉包裡。
- **事前宣告的突變**（三個都要以預期的訊息失敗）：
  - 在 `PvpApplication.cpp` 加回一個 `SDL_GetTicks`；
  - 產品 target 改連 `GYO::PlatformSDLNative`；
  - probe 加回 `Uint32`。

## 4. 驗收

- **L1**：本機 `test` preset 全量；四平台 CI；第 3 節的守衛與它的突變；Engine 各里程碑的突變。
- **L2**：整個 PR 的 base 與頭，在同一場次交錯跑 GUI 短測，只跑一次（v7 P4 的原文）。門檻在開跑前宣告並取得同意，宣告前做門檻來源的對抗式檢查（D45）。
- **L3**：大廳的文字輸入與剪貼簿；live resize；日誌檔裡經 SDL 的行與以前相同（日誌契約的 B 級；核對在 Engine 的 ip3a）。

## 5. 估時

| 部分 | 估時（h） | 依據 |
|---|---|---|
| IP-3 時點的產品工作 | 5～6 | 評審的合成草案 |
| IP-4 時點的產品工作 | 14～20 | 對抗式檢查（M4）：probe 8 檔約 300 處，加上 U5、U6 的判定變更、守衛與它的突變、L2 宣告與交錯量測。原草案 7～9 h 偏樂觀 |
| 合計 | 19～26 | Engine 部分另計 37～46.5 h（Engine 的 U1） |

after-re-evaluate 第 9 節原本估 `sdl-isolation-plan` 4～6 h、`sdl-isolation` 22～28 h，共 26～34 h。新的合計約 56～73 h，約 2 倍。這是 Engine 的 U1，由你決定。

## 6. 本計畫的待決項

| # | 問題 | 選項 | 建議 |
|---|---|---|---|
| S1 | v7 第 12 批決定「移除 `SdlPlatform&`／`SdlGpuRenderDevice&` 的公開暴露」（`PvpApplication.hpp:129-130`），但 probe 要透過 `application.Platform()` 拿到查詢與注入器，兩者不能同時成立（對抗式檢查 M2） | (a) 維持移除：產品改公開一個只給 probe 用的窄介面（例如 `ProbeAccess`，只交出 `SdlPlatform&`）／(b) 修訂該決定：保留 `Platform()`，只移除 `SdlGpuRenderDevice&` 的公開暴露／(c) probe 自己建立 platform（不經產品） | (b)：`SdlPlatform` 去 SDL 之後已經是中立 API，公開它不再違反「不直接呼叫 SDL」；(c) 會讓 probe 和產品的初始化分岔。**決定：(b)（D160）** |
| S2 | 凍結的 probe 與分析器：IP-4 會改 probe 的事件來源（注入、觀測）與標籤字串 | L2 的 base 用舊 probe、頭用新 probe，各自凍結／兩邊都用新 probe（base 要 cherry-pick） | 前者：base 與頭各用自己的樹，比較的是產品行為；差異由事前宣告列出。**決定：前者（D160）** |
| S3 | Engine 的 ip3a 核對（[ip3-declaration.md](../../../architecture/plans/input-and-present/ip3-declaration.md) 第 5 節）：A 級的受擊回饋行（`PvpApplication.cpp:377`，`PvP hit feedback version=1`）現在經 `SDL_Log`→`ForwardSdlLog` 以 `[info] ` 開頭寫進日誌檔；釘住測試從寫出端原始碼讀鍵序列（`keys-v1.tsv:28`），沒有工具解析它。產品改走 Base 之後，這一行的寫出端與路徑都會改 | (a) 改成 `Base::Log`，產品的 Base handler 以和 `ForwardSdlLog` 相同的格式（`[info] ` 加內容）寫進日誌檔，日誌檔的位元組不變；釘住測試的讀取位置跟著寫出端改，鍵序列不變（判定等價，照 §14.4 事後報告 1～5，並以突變證明）／(b) 改用 `std::clog` 寫，和其他 A 級的 Client 行相同（但 `std::osyncstream` 正是 cxx-flag-removal 要去掉的）／(c) 維持 `SDL_Log`（違反完成條件，不可行） | (a)：位元組不變，路徑和其他產品日誌一致；這樣 ip3e 的停止條件不會在途中成立。**決定：(a)（D161）** |

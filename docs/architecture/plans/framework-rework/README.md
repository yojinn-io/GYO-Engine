# framework 整改：分批計畫與進度

更新：2026-11-01。Owner：Engine（`engine/platform/sdl`、`engine/io`）與 framework（`framework/`）。
**狀態：執行中。** 使用者 2026-11-01 核准 PLAN 第 1 版與待決事項 D-1～D-9（全部採建議）。先讀 [批次計畫](PLAN.md)，再讀 [交接](HANDOFF.md)。

## 來源

- 2026-11-01 架構健檢：`docs/checkup/architecture/2026_11_01_architecture.zh-Hant.md`（PR #126 併入 master）。
- 架構會話整理的修正方向第 2 版（含一次對抗式審查），本計畫需要的結論已寫進本資料夾。
  - 原稿保存在使用者的 Mac（本 repo 的主 checkout）`build/evidence/framework-rework/2026_11_01_framework-fix-direction-v2.md`，git 不追蹤；SHA-256 `cdef6a07617dcd0c375c88672895f3a7b9e20d400c278a73c5eac9924a17e374`。
  - 那次對抗式審查（Sonnet）的原始輸出未保存；它的發現只以原稿中標「（審查）」的段落留下。
- 基準：origin/master `6e78090b`（與 `ebfe3225` 只差 `docs/checkup/`）。framework 由 PR #123（`97b0461d`）建立。
- 規劃：2026-11-01 用 ultracode 規劃，結論與證據位置見 PLAN 開頭。

## 使用者的原則（原話，2026-10-31～11-01）

1. 「Engine和SDL是可插拔的模組化，和IMGUI和JSON一樣，數據和結構分離是算法抽象應該依靠結構，而不是數據。」
2. 「tool相當於UI，而framework相當於controller。tool整理好數據之後，調用framework，得到返回的結果。」
3. 「framework在GYO中就是一個通用建置，他可以處理UI的東西，但不是直接嵌入，而是抽象出來。否則某一天我不用IMGUI了，或則特定的json解析工具了，就需要代碼一通改。」
4. 「SDL的問題你可以直接用Engine的能力」「Engine 沒有就直接增強 Engine 的能力，架構設計要保持一致性。」
5. 「ImGui的問題和Json一樣，他不是Engine的職責…引擎提供能力，而遊戲使用這些能力。framework則更偏向通用功能，但是不會直接和組件綁定。」
6. 「彈窗的 OS 那一半是視窗系統的能力，和剪貼簿同級，屬 Engine；流程那一半是工具的，留在工具。」
7. 「framework 可以遊戲和tool的公用層…遊戲就是一個有娛樂效果的工具…只不過方向不同。」
8. 分工：工具側的工作交 v8 tool-line 會話；framework 與下沉到 Engine 的部分屬本計畫。先處理已知問題，再做功能。

## 目標分層

| 層 | 負責 | 例子 |
|---|---|---|
| Engine | 能力 | 視窗、渲染、事件、剪貼簿、檔案（含使用者路徑上的讀寫與根目錄內判斷）、log、選擇窗的 OS 半、catalog 的格式 |
| framework 核心＝產品外殼（controller） | 把 Engine 的能力組起來、把控制交給產品；只依靠結構與自己定義的介面 | 有視窗的外殼、無視窗的外殼、內容掛載、登錄政策 |
| 綁定模組（framework 底下，獨立 target） | 把具體元件接到核心介面上，可替換 | ImGui 宿主、catalog 的 JSON 文字讀寫 |
| 產品（遊戲與工具） | 自己的部分 | pvp 的三角色、ImGui 畫面、工程檔、資產預覽 |

規則（草案，條文在 PLAN 經核准後才寫進各 owner 的 Design Invariants）：

- SDL 只經 Engine：核心與綁定模組都不直接具名 `SDL3::SDL3`、不呼叫 `SDL_*`、不在標頭前置宣告 SDL 型別。只有 ImGui 綁定可連 `GYO::*Native`。ImGui 的 SDL 後端對外 PUBLIC 連 SDL3，所以規則寫「不直接具名」。
- 核心不 include `SDL3/`、`imgui*`、`backends/`、`nlohmann/`；不連結 `SDL3::SDL3`、`imgui::*`、`nlohmann_json::*`、`GYO::*Native`。
- ImGui 綁定的建置條件是 `IMGUI` 且 `SDL_RENDERER`（`IMGUI` 不隱含另外兩個，`build/cmake/GyoComponents.cmake:38-46`）。JSON 綁定隨核心一起建，不新增 component；Engine 的 nlohmann 是 PRIVATE（`engine/CMakeLists.txt:76`），所以綁定要自己具名連結。
- 「換掉 JSON 函式庫只換綁定模組」只對 framework 成立：Engine 自己的 `CatalogParser`、`ContentManifest`、ui codec 都內建 nlohmann。
- 工具的畫面程式、工程檔、資產預覽留在工具；不做 UI 元件庫。
- 選擇窗：OS 半進 Engine 平台層（`engine/platform/sdl`，不新開中立目錄），介面比照剪貼簿；流程半（用途對照、每幀套用）留在工具。跨執行緒的信箱歸 Engine（PLAN 決策 D-3，待確認）。framework 不再持有選擇窗的任何部分。
- 登錄政策（id 已用另一個 type、路徑已被另一個 id 占）留在核心，由呼叫者的選項決定是否拒絕；audio_editor 的 CLI 測試 `convert_taken_path` 期待結束碼 4（`tests/audio_editor/AudioEditorCliTests.cmake:31`），行為不變。

## 批次（草案，待 PLAN 核准）

```text
framework-rework
├── v8-framework-rework-plan         medium｜定計畫
├── v8-framework-guard-ratchet       high｜先把守衛的規則裝好，舊違規列進只減不增的清單
├── v8-engine-platform-path-dialog   high＋xhigh 審查｜選擇窗的 OS 半進 Engine
├── v8-engine-io-user-files          high＋xhigh 審查｜檔案戳記、原子寫建父目錄、路徑包含判定進 engine/io
├── v8-framework-shell               high（草圖先核准）｜產品外殼與 ImGui 綁定
├── v8-framework-catalog-structure   high｜catalog 登錄的結構與 JSON 文字分開
├── v8-framework-catalog-extra-fields  high｜修更新條目時丟掉額外欄位的缺陷（D-6）
└── v8-framework-invariants          high｜工具切換後收尾：刪舊件、文件定稿
```

內容、依賴、估時、驗收見 [PLAN](PLAN.md) 第 1、2 節。收尾前各批只做加法（PR A），工具線切換後才刪舊件（PR B）。

## 完成

| 批 | 日期 | dev_log |
|---|---|---|
| v8-framework-rework-plan | 2026-11-01 | [規劃](../../../dev_logs/2026_11_01_framework_rework_plan.zh-Hant.md) |

## 決策（2026-11-01 提出，同日全部照建議核准）

| 代號 | 問題 | 決定（採建議） | 證據 |
|---|---|---|---|
| D-1 | catalog 規則的來源：(a) 規則留 framework、只用 cases.json 子集對齊；(a)＋b-lite 再把 Engine 的 `IsRelativeContentPath` 公開讓 framework 呼叫；(b) Engine 公開整套檢查 | (a)＋b-lite。已觀察到分歧（控制字元），b-lite 用一個公開函式消掉；(b) 要整套內容，會改 audio_editor 的 CLI 契約，屬工具的 Feature | `framework/src/CatalogRegistration.cpp:136-149`；`engine/asset/src/ContentManifest.cpp:13,35-38` |
| D-2 | `AbsolutePath`（絕對化＋詞法正規化）的唯一擁有者 | 留在 framework 核心，和內容掛載放一起；ui_editor 用核心這一份。行為不變、Engine 不動、只有一份實作 | `framework/src/ReadOnlyAssetCatalog.cpp:29,46,64`；`DocumentSession.cpp:169,208` |
| D-3 | 選擇窗的跨執行緒信箱歸 Engine（原則 6 說「inbox 留在工具」） | 歸 Engine。回呼在 Engine 註冊，例外不穿 C stack 與平台銷毀後的回呼只有 Engine 處理得了；工具留用途對照與每幀套用 | `framework/src/SdlPathDialogs.cpp:19-31`；`SDL_dialog.h:125-126` |
| D-4 | T1 的時點：與 T3a 一起做，或給舊宿主加 `SdlPlatform*` 橋接讓 T1 先做 | 一起做。橋接要改即將刪除的公開標頭，多一次核准 | `ImGuiSdlHost.hpp:20-23` |
| D-5 | 守衛：ratchet（第 1 批裝好、清單只減不增）或最後才收緊 | ratchet。新程式碼一出生就受保護 | `tests/framework/framework_dependency_guard.py:25,29,70-71` |
| D-6 | 已知缺陷（更新條目時丟掉額外欄位）誰修 | 本線在 catalog-structure 之後另開一小批，依 §14.3 先 Red 後 Green。程式碼屬 framework；不修的話新綁定會照抄缺陷 | `CatalogRegistration.cpp:84,189,208`；cases.json `valid-custom-type-and-metadata` |
| D-7 | PR 切法：2 條（前六批、收尾）或 3 條 | 2 條 | 記憶「PR 盡量少」 |
| D-8 | 有視窗外殼目前只有 ui_editor 一個使用者，依新 DI 5 進核心 | 接受，DI 5 註明是使用者接受的預測壓力 | `docs/framework/architecture.md:61-65` |
| D-9 | ultracode 的使用 | 外殼草圖用（約 3 案＋1 次對抗式檢查）；選擇窗只做 xhigh 審查＋別的引擎做一次對抗式檢查 | effort-gearbox |

各批的 Design Invariants 條文與守衛變更，在該批開工前逐批核准（PLAN 第 4、5 節）。

## 與其他會話的交接

- v8 tool-line 會話：工具側 T1（ui_editor 改用 Engine 選擇窗）、T2（兩個工具改用 engine/io）、T3a（ui_editor 接新外殼與 ImGui 綁定）、T3b（audio_editor 接結構 API 與 JSON 綁定）。都等 PR A 合併；預設 T1 與 T3a 一起做。收尾批等 T1～T3b 都合併。每批合併後通知：合併 commit、改了的公開介面、工具側要改的地方。工具線的分支 `claude/v8-tool-audio-editor-edit` 停在核准關卡等本計畫。
- 外殼介面草圖定稿後，通知 pvp v8 的 owner 會話，把「pvp Client 搬上外殼」列為候選。
- 整條線完成後，通知架構會話，下次健檢核對。

## 不做

- SDL renderer 的貼圖 API（健檢記為 Engine 缺口，另案）。ui_editor 的資產預覽在那之前仍經 `*Native` 直連 SDL。
- 把 pvp 搬上外殼（產品批次）。
- 遊戲 framework 的其他部分（角色編排等），照 v7 README 的觸發條件。
- Gateway、未啟用產品、素材結構。

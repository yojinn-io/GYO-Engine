# v8 工具線交接

更新：2026-10-30。Owner：三個工具各自的 owner（見 [README](README.md)）。**狀態：`v8-tool-plan` 完成，等你核准；沒有實作，沒有推送。**
本文件是工具線的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)。v8 產品計畫的同步由產品會話做，本線不改 `docs/object_fps_pvp/plans/v8/`。

## 代號對照

代號規則照 [docs/architecture.md](../../../architecture.md)「Plan records and identifiers」（`v8-docs-id-scheme`）。

| 代號 | 意思 |
|---|---|
| ui-widgets-line | PR 線：文字輸入框元件、輸入法組字、ui_editor |
| voxel-line | PR 線：3D 格子地圖編輯器 |
| assets-line | PR 線：資產編輯器（v8 只做 hitbox） |
| `gyo.voxel_map` | 地圖編輯器的原始檔格式（草案名） |
| 烘焙 | 把 `gyo.voxel_map` 轉成 arena v1 的產品 Script |

## 現況（每次停下來時更新）

- **完成**：`v8-tool-plan`（本計畫、本 HANDOFF、[ultracode prompt 草稿](ultracode-prompts.md)）。
- **進行中**：無。
- **等你決定**：下一節。
- **發現（不在本線的範圍，只報告）**：
  - `GYO::Ui` 與 ui_editor 沒有 `Design Invariants` 節（[ui_toolchain.md](../../../ui_toolchain.md)、[tool_projects.md](../../../tool_projects.md)）。本線加元件前要補（`v8-tool-ui-invariants`）。
  - PvP 有一份從舊產品複製的 `RetroFPS/World/GridMap.hpp`，經 `GameData.hpp` 被武器標頭間接包含，但 PvP 沒有它的載入器。是不是遺留物沒有查證；交給架構健檢。

## 等你決定

每項：機制、取捨、證據位置、建議。「會改變哪個決定」寫在最後一欄。

| 代號 | 問題 | 選項與取捨 | 建議 | 會改變 |
|---|---|---|---|---|
| `v8-tool-plan-location` | 計畫文件放哪 | `docs/tools/plans/v8/`（三個工具共用一份線計畫；`docs/` 底下，不是新的 Top-level 目錄）／各工具 owner 的 `docs/<tool_id>/plans/v8/`（分三份，跨工具的順序要寫三次） | 前者；各工具的格式與 Design Invariants 之後放 `docs/<tool_id>/` | 文件位置；圖書管理員可再調 |
| `v8-tool-start-timing` | 實作何時開始 | 照 v8-product-order 在 ③ 之後／ui-widgets-line 的檢測（`v8-tool-ui-ime-observe`）先做，其他照順序 | 檢測先做：它不改行為，結果決定輸入法要不要修 | 何時排實機 |
| `v8-tool-order` | 三個工具的順序 | ui → voxel → assets（依賴最少的先做；assets 要等 hitbox-data）／voxel 先（地圖編輯的價值最大） | ui → voxel → assets | PR 線的排程 |
| `v8-tool-ui-widget-scope` | 「組件化這些按鈕和輸入框」的範圍 | (a) 只加 `text_field`；按鈕已經是 `gyo.ui` 的 `button`（`pvp_lobby.json`）／(b) 再加按鈕的共用樣式（同一組顏色、字級只寫一次），也是 schema 變更／(c) 可重用的元件範本（prefab），接近 ui_toolchain.md 明說不做的「custom widgets」 | (a)；(b) 等第二個畫面也重複時再做 | `v8-tool-ui-button-style` 要不要排 |
| `v8-tool-ui-schema-version` | `text_field` 怎麼進 schema | v1 的集合寫明是封閉的。升 `version: 2`（讀取端接受 1、2；舊讀取端明確拒絕 v2）／留在 v1 加元素（舊讀取端以「未知元素」拒絕，但版本號沒告訴人為什麼） | 升 v2 | 契約文件與 codec |
| `v8-tool-ui-edit-policy-owner` | 文字編輯政策放哪 | 通用的部分（游標、刪除、全選、貼上、組字顯示）放 `GYO::Ui`（Architecture Delta：Ui 的責任變大）／全部留在產品，Ui 只畫（輸入框就不算元件）。字元過濾：(i) `text_field` 宣告 `max_length`，過濾由產品在提交時檢查／(ii) 資料裡宣告允許的字元集合（簡單、封閉，但開始往設定語言走，AGENTS §9）／(iii) 產品在執行時提供過濾函式 | 通用部分在 Ui；過濾用 (iii)：規則留在產品，資料不帶產品語意 | Ui 的 Design Invariants 怎麼寫 |
| `v8-tool-ui-invariants` | 在 ui_toolchain.md 新增 `Design Invariants` 節 | AGENTS §3：節的新增要使用者同意 | 同意後在 `v8-tool-ui-invariants` 寫，內容另給你看 | 能不能開始加元件 |
| `v8-tool-ime-input-delta` | 組字事件由誰做 | Engine 的 Input（計畫夾 input-and-present；只加不改，不變式 1）在工具線做／交給 Engine 的會話做 | 工具線做，記到 input-and-present 的 HANDOFF；先等 `v8-tool-ui-ime-observe` 的結果 | 有沒有 Engine 批 |
| `v8-tool-voxel-dimension` | v8 README 未決事項 b：2D 還是 3D | 工具名稱是 voxel，推論你選了 3D；舊的 2D 是 ASCII 文字地圖（`assets/object_fps/maps/*.txt`），產品已停用 | 3D；請確認 | 整條 voxel-line |
| `v8-tool-voxel-runtime-path` | 執行端讀什麼 | 烘焙到 arena v1（不改 wire、不改 Client／Match；多一份產生的檔，用 CI 檢查一致）／執行端讀格子（arena v2，新 pvN，改碰撞與繪製） | 烘焙 | 有沒有協議變更 |
| `v8-tool-voxel-cell-model` | 格子裡放什麼 | 只有方塊＋標記／網格模型（mesh library，像 Godot；需要 arena v2） | 只有方塊＋標記 | 格式與繪製 |
| `v8-tool-voxel-contract-owner` | `gyo.voxel_map` 的讀寫放哪 | 工具＋產品 Script（Engine 不加模組）／一開始就放 Engine（新 Subsystem，現在只有一個使用者） | 工具＋產品 Script | 有沒有 Engine 的 Architecture Delta |
| `v8-tool-gpu-imgui-component` | ImGui 的 SDL_GPU 後端 | 共通 Build 加一個元件（Architecture Delta：`GyoComponents.cmake`、部署規則）／voxel 工具自己編譯後端原始碼（不改共通 Build，但部署規則與元件宣告會散在工具裡，第二個 3D 工具再複製一次） | 加元件，影響範圍在規劃批寫七項說明 | 共通 Build |
| `v8-tool-assets-scope` | 資產編輯器在 v8 做到哪 | 只有 hitbox／再加材質覆寫、動畫集等 | 只有 hitbox；其他等有需求 | assets-line 的批數 |
| `v8-tool-assets-format-owner` | hitbox 格式在產品時，通用工具讀不到 | 由 `v8-product-hitbox-data` 決定；工具線希望通用部分在 Engine。要和產品會話雙向對齊 | 對齊後再開 `v8-tool-assets-editor-plan` | 工具是通用還是產品專用 |
| `v8-tool-release` | 新工具要不要進 release | 本機工具（`release=0`）／release 工具（每平台都要驗收） | 先本機，穩定後再議 | 打包與 CI |
| `v8-tool-ultracode` | voxel 與 assets 的規劃批用 ultracode | prompt 草稿在 [ultracode-prompts.md](ultracode-prompts.md)；看過、同意才啟動 | 看過 prompt 再定 | 規劃批的做法 |

## 決定紀錄

（尚無）

## 紀錄

- 2026-10-30：`v8-tool-plan` 開始（檔位 high，引擎 Opus 5.5，估 3 h）。從主會話開的工具線會話，分支 `claude/v8-tool-line-3a892c`（切自 `claude/pvp-v8-pv8-line` 的 `c7ca2fd7`）。盤點 `tools/`、`engine/ui`、`engine/input` 的 SDL 後端、產品的大廳與 arena、舊產品的格子地圖、v2 的 hurt_regions、共通 Build 的 ImGui 元件。主要發現：SDL 後端不收 `SDL_EVENT_TEXT_EDITING`；`gyo.ui` v1 沒有輸入框；`Arena` 新增成員是 wire 變更（pv6 §2），所以建議地圖編輯器烘焙到 arena v1。
- 2026-10-30：計畫、HANDOFF、ultracode prompt 草稿寫完；實際用時約 1 h（估 3 h 以內）。沒有實作、沒有推送。停下等你核准。

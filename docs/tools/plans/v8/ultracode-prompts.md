# ultracode 規劃批的 prompt 草稿

更新：2026-10-30（`v8-tool-plan`）。**草稿，未啟動。** 使用者看過、同意後才啟動（`v8-tool-ultracode`）。
規模照 [變速箱規則](../../../../agents/effort-gearbox.md)：案約 3 個、審查 1 人、對抗式檢查 1 次。

## 編成（兩批相同）

| Agent | 引擎｜檔位 | 理由 |
|---|---|---|
| 提案者 ×3 | Opus 5.5｜high（繼承主會話） | 每人從不同的前提出發，避免三案相同 |
| 審查者 ×1 | Opus 5.5｜high | 比較三案，對照 AGENTS §3、§7、§8、§9 |
| 對抗式檢查 ×1 | Sonnet 5.5｜high | 和提案者不同的引擎（變速箱規則）；只找能推翻建議的證據 |

都在主會話的引擎與檔位以內，不需要另外同意。

## 交給 agent 的資料（兩批共通）

- 交給：`AGENTS.md`、`agents/effort-gearbox.md`、本計畫的 [README](README.md)、[tool_projects.md](../../../tool_projects.md)、[ui_toolchain.md](../../../ui_toolchain.md)、[protocol-v6](../../../object_fps_pvp/protocol-v6.zh-Hant.md) §2。
- 不交給：v7／v7.1 的 HANDOFF 全文（太長、和工具無關）；dev_log（是當時的紀錄，會過時）。

## `v8-tool-voxel-map-plan`

```text
你是 GYO-Engine 的規劃者，只寫計畫，不改程式碼、不 commit。
目標：為 3D 格子地圖編輯器（v8-tool-voxel-map-editor）寫一份分批計畫的提案。

讀：docs/tools/plans/v8/README.md「工具 2」、apps/object_fps_pvp/include/RetroFPS/Pvp/Arena.hpp、
assets/object_fps_pvp/pvp_arena.json、docs/object_fps_pvp/protocol-v6.zh-Hant.md §2（arena digest）、
apps/object_fps/src/World/GridMapLoader.cpp、apps/object_fps/src/Rendering/MapGeometryGenerator.cpp、
docs/tool_projects.md、build/cmake/GyoComponents.cmake、build/cmake/GyoAppProject.cmake。

你的前提（三位提案者各拿一個）：
  A. 烘焙到既有的 arena v1，不改 wire（README 的建議）。
  B. 執行端直接讀格子（arena v2＋新的 pvN），一開始就為大地圖準備。
  C. 最小：只做烘焙 Script＋手寫格子檔，GUI 延到 v9；說明這樣夠不夠。

每案要回答：
  1. gyo.voxel_map 的欄位、版本、驗證規則；誰讀、誰寫。
  2. 執行端的路徑；是不是 wire 變更（依 protocol-v6 §2 的定義），引用行號。
  3. owner 與依賴方向；Product 刪除後還成不成立（AGENTS §7）。
  4. 共通 Build 的變更（ImGui SDL_GPU 後端）與它的 Architecture Delta 七項說明（AGENTS §3）。
  5. 分批、每批的檔位與引擎、估時、驗收（CI 為主）。
  6. 要使用者決定的事（附機制、取捨、證據位置）。
  7. 這案最弱的地方。
每個事實都附檔名與行號；讀不到的寫「未確認」。輸出用正體中文。
```

審查者：比較三案，指出各案違反 AGENTS 哪一條（附條號），給一個建議案與理由。
對抗式檢查：只針對建議案，找出能讓它失效的證據（例如烘焙後 digest 不穩定、合併演算法改變碰撞結果、SDL_GPU 與 ImGui 的版本限制）。找不到就寫「找不到」，不要編。

## `v8-tool-assets-editor-plan`

前提：`v8-product-hitbox-data` 已完成，格式與 owner 已定。開始前把它的結果（格式文件的路徑、owner）填進 prompt。

```text
你是 GYO-Engine 的規劃者，只寫計畫，不改程式碼、不 commit。
目標：為資產編輯器（v8-tool-assets-editor，v8 只做 hitbox）寫一份分批計畫的提案。

讀：docs/tools/plans/v8/README.md「工具 3」、<hitbox-data 的格式文件>、
docs/object_fps_v2/enemies.zh-Hant.md（hurt_regions 先例）、docs/architecture/3d-assets.md、
engine/model/include/engine/model/、assets/object_fps_pvp/players/character.json、
tools/ui_editor（唯讀掛載資產的做法）、<voxel-line 的 3D 視窗程式的位置>。

你的前提（三位提案者各拿一個）：
  A. 通用工具：hitbox 的通用格式在 Engine，工具不連結遊戲。
  B. 產品專用工具：requires_apps 綁 object_fps_pvp（像 object_fps_preview）。
  C. 先只做驗證 CLI＋唯讀預覽，編輯仍手寫；說明這樣夠不夠。

每案要回答：
  1. 讀寫哪些檔、哪些唯讀；部位語意（倍率）放哪裡。
  2. owner 與依賴方向；Product 刪除後還成不成立（AGENTS §7）。
  3. 3D 視窗和 voxel 編輯器的重複：要不要抽共用、抽到哪裡、根據哪個觀測到的重複（AGENTS §1）。
  4. 挑骨骼的方法、蒙皮模型的姿勢來源。
  5. 分批、每批的檔位與引擎、估時、驗收（CI 為主）。
  6. 要使用者決定的事（附機制、取捨、證據位置）。
  7. 這案最弱的地方。
每個事實都附檔名與行號；讀不到的寫「未確認」。輸出用正體中文。
```

審查者與對抗式檢查同上。

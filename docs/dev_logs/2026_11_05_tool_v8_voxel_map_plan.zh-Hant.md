# 工具 v8：v8-tool-voxel-map-plan（voxel 地圖編輯器的方案與分批）

日期：2026-11-05。分支 `claude/v8-tool-voxel-map-plan`（切自 origin/master `16710503`）。檔位 medium，模型 Opus 5.5（主會話）；統合者 Fable 5.1／high，共兩輪。

**狀態：** 完成，方案與分批已由使用者確認。只改文件，沒有程式變更。

相關文件：[voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md)、[工具線 README](../tools/plans/v8/README.md)、[工具線 HANDOFF](../tools/plans/v8/HANDOFF.md)。

## 做了什麼

- 寫 [voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md)：參考專案借了什麼、Feature → Contract → Architecture、15 項已定案的決定、8 批（檔位、模型、估時、驗收、依賴）、風險、候選與延後。
- README：樹狀總覽加 voxel-map-line；「待討論的兩個工具」的 voxel 段改成指向新文件；dev_log 一覽加本批。
- HANDOFF：現況、決定紀錄（`v8-tool-voxel-*`，附使用者原話）、紀錄。

## 經過

1. **先盤點，不先提案。** 照 `v8-tool-scope-ui-first`，第一版自己寫的地圖建議已經撤下，這次不沿用。盤點了：
   - arena v1（`Arena.hpp`）與 `walls` 的使用者：碰撞、`ShotQuery.cpp:99`、`PvpMatch.cpp:275`、`PvpApplication.cpp:712-731`；
   - pv6 §2 的 digest 條款，以及 pv7、pv8 的沿用；
   - 舊產品的 `GridMap`；
   - 產品 v8 計畫「大地圖的預計壓力」；
   - 工具和 framework 的現況。
2. **使用者給了自己的報告**（PDF，6 頁），要我重點讀兩個參考專案。PDF 用 macOS 的 PDFKit 轉成文字。之後讀了：
   - Goxel 的原始碼：稀疏 16³ tile、copy-on-write 的 undo、逐面剔除＋AO、沒有 greedy、GPL-3.0；
   - MagicaVoxel 的 `.vox` 規格：1 byte 調色盤索引、以索引為 id 的 MATL。
3. **使用者指定參考 Godot 的 GridMap。** 讀了 master `bf194eb5` 的 `modules/gridmap/` 和 `mesh_library.h`：
   - 每格存 item 16 bit＋rot 5 bit；`layer` 8 bit 有宣告，但 `.cpp` 沒有用到，4.3 和 4.5 stable 也一樣；
   - octant 8；每個 octant 每種 item 一個 MultiMesh；
   - 編輯器用「編輯平面」求交點，只經 `set_cell_item` 改資料。
   - 查證時發現 GYO 的 Render 沒有 instancing。
4. **使用者說明方向：** 格子用來編輯，build 成 mesh；Goxel／MagicaVoxel 參考互動，GridMap 參考儲存。
5. **第一輪 Fable 統合**（D1～D12），之後逐點討論。過程中幾個主要的轉折：
   - **D2**：使用者決定離線 build、產物存檔，理由是「可以反查」。因此 D1 的格子 owner 從 Engine 改成工具；D3、D6～D10、D12 的前提都變了，對照表在證據的 `plan-discussion.md`。
   - **framework**：使用者提醒「還有 framework」。讀了 `docs/architecture.md` 的分層表、framework 的 DI 3 和 DI 5，以及 PR #146 分支上的 `PreviewRenderer.hpp`，結論是 3D 預覽元件屬於 framework（model_viewer 之後的第二個組合者）。
   - **D7**：重讀 `protocol-v6.zh-Hant.md:51-52`（arena 內容的修改不是 wire 變更），提出 (b′)。之後查到 `IpcHost.cpp:507` 會把 `arena.version` 送上 wire，而 pv6:112 寫「只接受 1」，所以把「檔案版本」和「`arena_version`」分開（Q4 (ii)）。
6. **第二輪 Fable 統合**，問題重編成 Q1～Q11，使用者逐題回答。過程中的幾個插曲：
   - 旁支會話（「btw: 這個問題時,我要是要去二樓要如何處理?」）回報了二樓、樓梯、B1／B2 的結論。回到本會話確認時，使用者修正為「類似 minecraft 的半磚」，並說明形狀和朝向是給編輯器用的。所以 Q1 從整格改成 1/16 格，朝向採 24 種，形狀用資料定義。
   - Q10：使用者選原始檔放 `build/art-source/`。D2 原本的「CI 從原始檔重新 build 並逐位元比較」因此拿掉，改成「編輯器存檔時一定同時寫出產物＋`source_sha256`」。

## 查證過的事實（判斷用到的）

| 事實 | 出處 |
|---|---|
| Engine 的模型讀取器固定讀 FBX，repo 沒有 FBX 寫出器 | `engine/model/backend/ufbx/src/UfbxModelLoader.cpp:228` |
| 頂點只有 position＋uv；材質只有 texture＋tint；著色器只有 unlit；取樣只有 Linear（沒有 nearest） | `engine/render/include/engine/render/RenderTypes.hpp:53-56, 84-87, 124-137`；`engine/render/shaders/builtin/` |
| `walls≤1024`、arena 讀檔上限 256 KiB、出生點必須 `y==0`、`version==1` | `apps/object_fps_pvp/src/Pvp/Arena.cpp:28, 35, 51, 67` |
| Match 用 `--arena` 路徑載入 arena，不掛 catalog | `apps/object_fps_pvp/match_main.cpp:91` |
| `arena_version` 會上 wire，pv6 寫「目前只接受 1」 | `IpcHost.cpp:507`、`protocol-v6.zh-Hant.md:112` |
| catalog 的 type 是字串的雜湊 | `engine/asset/include/engine/asset/AssetType.hpp:30` |
| 產品資產規則：原始檔不進 repo | `docs/object_fps_pvp/assets.zh-Hant.md:122` |
| model_viewer 的 (B′) 預覽與 P1（p95 ≤ 6.0 ms）只在 PR #146 的分支上 | `tools/model_viewer/include/gyo/model_viewer/PreviewRenderer.hpp`、`docs/tools/plans/v8/model-viewer.md:183`（分支 `claude/v8-tool-model-viewer-77f310`） |

## 偏離與限制

- README 原本把這一批的檔位寫成「ultracode 規劃」。實際只用了 Fable 統合者（兩輪），沒有三位提案者，也沒有敵對式檢查者；使用者沒有要求補做敵對檢查。
- 兩輪統合都是唯讀的 Plan agent；主會話抽查了它們引用的事實（上表）。

## 證據

主會話所在機器（macOS）的主 checkout：`build/evidence/tool-v8-voxel-map/plan/`（git 不追蹤）。

- `Voxel_Map_Technical_Report_zh-TW.pdf`、`report.txt`：使用者的報告
- `reference/goxel/`、`reference/vox/`、`reference/godot/`、`reference/SOURCES.txt`：參考原始檔與出處
- `plan-discussion.md`：兩輪統合與逐點討論的計畫檔

## 核對

- 新文件和改過的 README、HANDOFF、本 dev_log 的相對連結，用腳本確認都存在。
- 用時約 4 h（估 4～6 h，在範圍內）。

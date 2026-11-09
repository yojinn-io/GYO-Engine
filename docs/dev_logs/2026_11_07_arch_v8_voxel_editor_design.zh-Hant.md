# 架構 v8：voxel 地圖編輯器下一階段的設計稿（架構師支援卡）

日期：2026-11-07。分支 `claude/v8-arch-voxel-editor-design`（切自 origin/master `b2781ab7`）。docs-only。
- 執行：架構師開出的任務卡會話（Opus 5.5／high）。沒有開子 Agent。
- 文件 owner：voxel 線（`voxel_map_editor`）；本卡只起草，實作時由 voxel 線維護。

**狀態：** 設計稿完成；2026-11-07 使用者核准 Q1～Q12 全部照建議（「照建議全部核准」，`v8-tool-voxel-design-approve`），已記進設計稿頁首與 HANDOFF 的決定表。沒有改程式、沒有改既有契約。

相關文件：
- [voxel_map_editor/design.zh-Hant.md](../voxel_map_editor/design.zh-Hant.md)（本批的產出）
- [voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md)（現況行指向設計稿）

## 做了什麼

- 寫 `docs/voxel_map_editor/design.zh-Hant.md`：
  - 頁首：「要決定的事」Q1～Q12，各附建議。
  - §1：現在程式裡的上限，以及推到 1 km² 的估計（標明未量測）。
  - §2 文件模型：選取集合、prefab（v1 格子檔、方塊對應、24 旋轉表、鏡像規則 (c) 的可計算判定、`.vox` 匯入、放置模式）、Layers（v2 的問題表與「先剖切」的替代）、undo 的儲存方式、大地圖的格子檔。
  - §3 操作模型：Goxel 工具逐一的採／改裝／不採、snap、拖曳與預覽、相機的差異表、快捷鍵約定、對稱、選取、大面積填充、邊界延伸。
  - §4 手感：H1～H8，每項有可量的判定或要問使用者的問題；不推斷使用者的意思。
  - §5 契約影響：分類、DI 1～7 與 RC 1～6 逐條、Architecture Delta 草稿兩件。
  - §6 分批建議：`ve-*` 批次樹、上段／下段與模型、各批的宣告項目與「不做」、順序的取捨。
- HANDOFF 的現況加一行。

## 經過

1. 先讀 AGENTS.md、`agents/effort-gearbox.md`、`agents/navigator.md`，工具的 architecture.md、voxel 線的 HANDOFF、window-goxel 的宣告、window-1 與 tool-chunked 的 dev_log、big-map 的 todo 與 `v8-arch-map-scene-split`。
2. 讀 Goxel 的原始碼（使用者在會話中補充了位置 `~/Code/GitHome/goxel-master`）。只讀行為，設計稿不含它的程式碼、圖示、著色器或 UI 字串。讀過的檔案：
   - `src/tools.h`、`src/tools.c`、`src/gesture3d.h`、`src/gesture3d.c`
   - `src/tools/` 的 brush、shape、line、laser、extrude、plane、move、selection、rect_select、fuzzy_select
   - `src/layer.h`、`src/layer.c`、`src/image.h`、`src/image.c`
   - `src/goxel.h`、`src/goxel.c`、`src/volume_utils.h`、`src/volume_utils.c`
   - `src/gui/` 的 layers_panel、symmetry_panel、snap_panel；`topbar.c`、`src/inputs.h` 只查快捷鍵
3. 讀本 repo 的工具核心標頭（`VoxelMap.hpp`、`Editing.hpp`、`Tools.hpp`）與視窗（`EditorWindow.cpp` 的輸入、拾取、拖曳、疊加線），找出和 Goxel 不同的地方，以及現有的上限。
4. 起草，最後對照 Goxel 原始碼修正兩處描述（Ctrl＋3 是左視、Delete 在沒有選取時清整個圖層）。

## 查證過的事實（設計稿用到的）

| 事實 | 出處 |
|---|---|
| 格子檔超過 16 MiB 就拒讀 | `tools/voxel_map_editor/include/gyo/voxel_map_editor/VoxelMap.hpp`（`MaxGridTextBytes`）、`src/GridDocument.cpp` |
| 像建築的合成圖（768 區塊）的格子檔 3,830,161 bytes；整張烘焙 578 ms | 主 checkout `build/evidence/tool-v8-voxel-map/tool-chunked/measure/`（`summary.txt` 與 `building.voxel_grid.json` 的大小） |
| 每區塊在記憶體中是 4096 個 `Cell`（`uint16`＋`uint8`） | `VoxelMap.hpp`（`ChunkCellsArray`） |
| 按下滑鼠時複製整份文件給拾取用 | `src/window/EditorWindow.cpp`（`Drag::base`） |
| 調色盤與形狀的 undo 存前後兩份完整文件 | `Editing.hpp`（`DocumentStep`） |
| 一次編輯最多 2^20 格；筆刷最大 16 | `Editing.hpp`（`MaxBoxCells`）、`Tools.hpp`（`MaxBrushSize`） |
| 拾取的光線最多 1024 格 | `Editing.hpp`（`Pick` 的 `maxCells`） |
| 縮放朝固定中心；每像素 0.01 rad 旋轉 | `EditorWindow.cpp`（`Navigate`） |
| Goxel 縮放時把中心移到游標下的體素；旋轉以畫面寬為一圈；左鍵在空白處拖曳旋轉 | Goxel `src/goxel.c`（縮放、旋轉、左鍵旋轉的手勢） |
| Goxel 的 Line 是拖曳；我們是點兩下 | Goxel `src/tools/line.c`；`EditorWindow.cpp`（`Tool::Line`） |

1 km² 的數字（約 230 MB 的格子檔、730 MiB 的記憶體、35 s 的整張重烘）都是由上表推算，**沒有量測**；設計稿把它們列為 `ve-scale-measure` 要量的項目。

## 偏離與限制

- 卡片要求「需要讀 Goxel 行為時先讀 src/tools/、src/gui/、src/goxel.c、src/gesture3d.c、src/layer.c 的相關部分」。`src/gui/` 只讀了和 layers、對稱、snap 有關的三個面板（其餘面板是材質、光照、算圖、匯出等，和本稿無關）。
- 沒有開子 Agent，也沒有敵對式檢查；設計稿的建議都待使用者回答 Q1～Q12 後，在各批的宣告中再檢查。

## 證據

沒有產生新的證據檔。引用的量測在主 checkout `build/evidence/tool-v8-voxel-map/tool-chunked/measure/`（Intel Mac，tool-chunked 批留下的）。

## 時間

約 3 h（卡片的時間盒 8～12 h）。

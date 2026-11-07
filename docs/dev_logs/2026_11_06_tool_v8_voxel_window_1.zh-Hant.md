# 工具 v8：v8-tool-voxel-window-1（Goxel 式的視窗：區塊化預覽與存檔、模式×工具、標記）

日期：2026-11-06。分支 `claude/v8-tool-voxel-map-window`（PR #158；master 已 merge 進來，`9c2a2479`）。上段。
- 實作：主會話（Opus 5.5／high）。
- 審查：子 Agent（Opus／high，唯讀）。
- §14.4 驗證：另一個子 Agent（Opus／high，唯讀）。
- 事前宣告：[voxel-map-window-goxel.md](../tools/plans/v8/voxel-map-window-goxel.md)，2026-11-06 使用者核准（「推奨どおり承認」）。

**狀態：** 完成：判定 W1～W11、P1、L3 全部成立（2026-11-07）。原本還沒做的項目都已處理：
- L3（使用者試用）：已合格。

相關文件：
- [voxel_map_editor/architecture.md](../voxel_map_editor/architecture.md)：「The window」節、命令表、DI 5 擴充、DI 7 改寫
- [voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md)

## 做了什麼

### vw1-preview：區塊化的預覽與存檔

- `EditSession` 會記下改過的格子（含 undo、redo）以及整份文件的變更（`TakeChanges`）。新增 `CancelStroke`，給 Esc 用。
- `MapPreview`（核心）：只對碰到的區塊做 `RebakeChunks`，再交出改過的區塊的 `ExpandVoxelChunk`，以及有變時的調色盤圖。
- `PreviewScene`：每個區塊一個 mesh，只重傳改過的區塊。
- 存檔順序：grid → 區塊化地圖 → 標記。外部修改由 `FileChanged` 偵測（grid、`map.json`、標記檔），偵測到時開對話框：從磁碟重新載入／覆寫／取消（解決上一版的 S4）。
- 命令改為 `edit <grid> <map-dir> [--markers --marker-kinds]`；`measure-preview` 加 `--no-edit`。

### vw1-tools：Goxel 的功能與互動（程式碼自己寫）

- 讀過的 Goxel 檔案（只讀行為、不搬程式碼；原始碼在使用者本機 `~/Code/GitHome/goxel-master`）：
  - `src/tools/` 的 brush、shape、laser、line、extrude、plane、color_picker、selection、move；
  - `src/gui/` 的 tools_panel、snap_panel、topbar、symmetry_panel；
  - `src/goxel.c`、`src/gesture3d.c`、`src/tools.c`；
  - README 的操作說明。
- 核心（`Tools.hpp`，CPU 可測）：
  - `EditMode`（Add、Sub、Paint）；
  - `BrushCells`、`StrokeCells`、`LineCells`、`ShapeCells`（箱、球、圓柱）、`LaserCells`；
  - `ExtrudeFace`、`Extrude`、`ApplyCells`、`PlaneThrough`；
  - 任意軸的 `Pick`，可指定 snap。
- 視窗：
  - 模式鍵：T、R、G；工具鍵：B、S、L、N、F、P、C、K；
  - 朝向用 Q、E（加 Shift 換上方軸），筆刷大小用 `[`、`]`，編輯平面用 PgUp、PgDn；
  - 拖曳中直接套用並顯示，放開算一步 undo，Esc 取消；拖曳時 pick 看的是拖曳開始時的地圖；
  - Shift＋點擊從上一點畫直線，Ctrl＋點擊吸取；
  - 相機：中鍵旋轉、右鍵平移、滾輪縮放、方向鍵旋轉、1／3／7／0 切換視角；
  - 畫面上有提示。

### vw1-markers：標記的放置

- 工具 Marker（K）：
  - 點：yaw 取相機的方向；
  - 箱：拖曳；
  - 路徑：點擊加點，Enter 完成。
- 面板可以：
  - 從產品的清單選 kind；
  - 改 id；
  - 改 yaw；
  - 編輯 tags（依鍵順序）；
  - 刪除標記。
- 標記不進 undo。
- 清單外的 kind 會擋下整次存檔（`UnknownKinds`）。

## 判定

件數の突き合わせ：
- P1：一致。(a)、(b) 都是要求 600、量到 600、失敗 0，統計用 540 幀。
- 突變：一致。最後一輪要求 87、結果 87。

| 代號 | 結果 |
|---|---|
| W1 | 成立：各工具與模式（含上限）、標記的放置輔助 |
| W2 | 成立：拖曳是一步；取消後文件不變，且會回報改過的格子 |
| W3 | 成立：舊的 W3 測試；任意軸的平面；snap 取先碰到的（審查 B1 之後加的測試） |
| W4 | 成立：拾取的 GPU 煙霧測試（本機） |
| W5 | 成立：存檔順序、FileChanged 與覆寫、grid 寫不出、地圖寫不出、舊 region 刪不掉、標記寫不出、清單外的 kind、空格子 |
| W6 | 成立：每個區塊畫的頂點，等於存檔文字讀回後展開的結果（逐位元） |
| W7 | 成立：200 組隨機編輯（含 undo、redo、調色盤變更），畫的區塊等於全部重烘的結果；另有區塊邊界的固定案例 |
| W8 | 成立：`edit` 的參數（標記選項要成對）、`measure-preview --no-edit` |
| W9 | 成立：驗證者確認 1～5，並找出 3 個沒列出的放寬，已全部改嚴（見下）；2026-11-07 使用者核准 |
| W10 | 成立：最後一輪 **87/87** 被殺（`mutations-round3.txt`）；第一輪的不足見下面的「突變」 |
| W11 | 成立：本機全量建置成功，`cpu|shader` 135/135、`gpu` 5/5；拿掉標記之後再跑一次，`cpu|shader` 仍是 135/135。L1（PR #158，head `44a06c82`，run 42342459066）：windows-x64、linux-x64、macos-arm64 全部通過 |
| P1 | **成立**。(a) 不編輯：p95 **7.48 ms** ≤ 16.7 ms。(b) 每幀編輯：p95 **8.82 ms** ≤ 33 ms。用像建築的合成圖（tool-chunked 的 `measure-map` 產生的同一張，768 區塊，grid sha `60e3aa3c…`），`--grid` 給的；不是 `--synthetic` 的小圖 |
| L3 | **成立**（2026-11-07，使用者「合格」）。使用者在視窗裡編輯 `pvp_corners`（加了一種方塊，311 格），`verify-map` 一致；遊戲用啟動腳本重新 build 的 v1（906 quads、24 colliders）跑 60 fps，有走動與落地。腳本與 log 在主 checkout `build/evidence/tool-v8-voxel-map/window-1/l3/` |

### 突變

- 第一輪 77/80：
  - `vw1-preview-neighbours-stale` 存活：隨機編輯太稀疏，碰不到「邊界格子讓隔壁區塊變」。
  - `vw1-cancel-unreported` 存活：測試在取消前沒有先取走變更，所以看不出取消有沒有回報。
  - `vw1-save-markers-unchecked` 是 wrong_failure：存檔成功時，測試去讀失敗結果而崩潰。
  - 三個都是測試不足，不是等價突變；補測試後被殺（`mutations-round2.txt`）。
- 舊視窗批的 5 條 `vmw-*` 因為程式碼改寫而失效。2 條改指向新程式碼，3 條以 `vw1-*` 取代（守同一性質）。
- 審查與驗證修正後加了 7 條：守衛 4 條、snap 順序、Extrude 上限、`map.json` 的 known bytes。全目錄 87 條重跑，結果見 `mutations-round3.txt`。

## 審查（Opus／high）

**blocker（已修）**
- B1：snap 不是取先碰到的。編輯平面在相機和既有方塊之間時，選中的是後面的方塊，會擋住 L3 要做的二樓。改成先算光線碰到平面的位置，只有在那之前進入的方塊才算；並加測試。

**should-fix（都修了）**
- S1：拖曳中按 Ctrl＋S 或重新載入，拖曳會壞掉。存檔、重新載入、undo 前先取消拖曳。另外，拖曳中按 undo 只取消拖曳，不再多 undo 一步。
- S2：預覽上傳失敗後，畫面和存檔的內容不一致。改為失敗時全部重烘、重傳。
- S3：拖曳每幀都重新套用，計算量沒有上限。
  - 改為只有在角點、距離或滑鼠位置變了才套用；
  - Extrude 與 Laser 加上 2^20 格的上限；
  - 滑鼠懸停時算 Extrude 的面，結果有快取。
- S4：標記箱的起點會殘留，之後多出一個箱。切換工具、Esc、在區域外放開時都丟掉起點。
- S5：舊 region 刪不掉時，`map.json` 的 known bytes 沒有更新，下次存檔誤報 FileChanged。改為換完索引就更新 known bytes，並加測試。

**宣告裡寫了卻沒做的（已補）**
- Shape 的「留在起點的平面」。
- 標記 id 可改。

**nit（已修）**
- yaw 正規化到 (-π, π]；
- 標記座標超過 ±2^20 單位時不放；
- `ShapeCells` 的乘積溢位；
- Extrude 的錯誤訊息。

**交給使用者**
- 空格子存檔：只寫 grid、不寫地圖，所以磁碟上舊的 `map.json` 和 region 會留著，和 grid 對不上。見下面「要使用者決定的事」。

## §14.4（依賴守衛，DI 1）

驗證者用舊（master `24f111eb`）、新兩版守衛跑了 66 個合成樹與實際的樹。

1. **守的性質**：DI 1。驗證者確認。
2. **放寬的面**：
   - 視窗檔可以 include `imgui.h`、`gyo/framework/imgui/…`、`window/…`；
   - 核心可以 include 剛好一個 `window/EditorWindow.hpp`；
   - 視窗 library 可以連六個視窗 target；
   - gui 的存在檢查裡可以出現這些名字；
   - 核心多連 `GYO::Math`。
   - 驗證者確認，並指出：`engine/render/…`、`engine/platform/…`、`gyo/framework/Preview*` 舊版本來就允許，所以不算放寬。
3. **收緊的面**：
   - 核心不能 include `gyo/framework/imgui/*`、render、platform、Preview、WindowShell；
   - 核心不能連視窗 library；
   - include 路徑不能有 `.`、`..` 或反斜線；
   - 不能用 `link_libraries`、`INTERFACE_LINK_LIBRARIES`、`link_options`、`TARGET_OBJECTS`、生成式或變數來連結；
   - native 的檢查不分大小寫；
   - 不能為視窗 library 建 ALIAS。
4. **緩的面以外仍受守護**：驗證者確認，但找出 3 個沒列出的放寬，本批都改嚴了：
   - A：入口標頭 `EditorWindow.hpp` 套的是視窗規則。它被核心 include，改套核心規則。
   - B：`src/window/` 的規則看目錄、不看 target。改為只有視窗 library 可以編譯那些檔案；代替檔 `EditorWindowUnavailable.cpp` 也套核心規則。
   - C：沒檢查「只在 gui 版」。視窗 library 與視窗 target 只能出現在 `if(GYO_TOOL_VARIANT STREQUAL "gui")` 的分支裡。
   - 舊版本來就有的兩個漏洞也一起補：沒有目錄的 `SDL.h`、大寫的 `gyo/framework/IMGUI/`。
5. **證據**：
   - 自我測試 17 條通過；
   - 實際的樹通過；
   - 突變 `vmw-guard-*`、`tc-guard-*`、`vw1-guard-*` 全部被殺。
   - 另外：自我測試 `test_other_engine_modules_still_fail` 在 merge 時改成逐名檢查。驗證者指出那比數件數鬆，已改回精確比對（6 筆）並加入 `GYO::MapMarkers`。

**使用者核准**：上面 2 的放寬（含舊視窗批沒核准完的 W9），2026-11-07 核准（原話「承認する」）。

## 和宣告的差異

- **標記不進 undo**：宣告沒寫，實作上先不放，面板上有標示。
- **正交投影延後**：照決定 5。

## 要使用者決定的事

- **空格子存檔時的舊地圖**：Engine 的格式不能表示空地圖（至少一個 region）。選項：
  - (a) 拒絕存檔，說明原因；
  - (b) 存 grid，並刪掉舊的 `map.json` 與它列的 region（刪除是破壞性的動作）；
  - (c) 維持現狀（舊地圖留著），只在狀態列警告。

## 證據

主 checkout `build/evidence/tool-v8-voxel-map/window-1/`（Intel Mac，RelWithDebInfo）：

| 位置 | 內容 |
|---|---|
| `p1/` | 判定試行 |
| `explore/` | 探索：第一版每幀編輯 p95 152 ms，原因是每次編輯都複製整張地圖、把所有區塊標成髒；修正後 11.4 ms。不當判定 |
| `mutations-round*.txt` | 突變的各輪結果 |

審查者與驗證者的 probe 在本會話的 scratchpad，未保存。

## 2026-11-07：拿掉標記的放置（使用者決定）

- **依據**：使用者的決定 `v8-arch-map-scene-split`，地圖只管幾何，標記屬於另一個場景編輯器（經架構師轉達）。使用者在本會話確認（「外す」）。
- **拿掉的**：
  - vw1-markers 的 UI、`edit` 的 `--markers`／`--marker-kinds`、存檔寫標記；
  - `MarkersFile` 的放置輔助（還原成 master 的狀態）；
  - 對應的 3 個測試案例與 3 條突變；
  - CLI 測試改成「`--markers` 是用法錯誤」。
- **出過的錯**：一次刪除時，把存檔裡產生地圖文字的那段也刪了。建置與測試之前用 grep 發現並補回，沒有提交過那個狀態。
- **判定**：留下的判定沒有放寬，被拿掉的只有隨功能一起移除的部分。重跑全目錄的突變：**84/84** 被殺（件數の突き合わせ：一致，要求 84、結果 84；`mutations-round4-no-markers.txt`）。本機 `cpu|shader` 135/135（`ctest-cpu-shader-no-markers.txt`）。

## 2026-11-07：空格子的存檔（使用者決定 (a)）

- **決定**：使用者選 (a)，原話「(a) 保存を断る」：沒有填任何格子的格子不存檔，回 `EmptyMap`，什麼都不寫。這樣磁碟上的 grid 不會和它的地圖對不上（Engine 的格式不能表示空地圖）。
- **測試**：把「空格子只存 grid」的 W5 測試改成「拒絕，且什麼都不寫；把存過的地圖清空後，存檔被拒，舊的 grid 和地圖原樣、彼此一致」。
- **突變**：加了 `vw1-empty-grid-saved`。
  - 第一次寫成「只拿掉檢查」，結果是 wrong_failure：拿掉檢查後程式讀到空的地圖就崩潰，到不了斷言。
  - 改成「重現舊行為：只存 grid、回成功」之後被殺。
  - 存檔相關的 5 條全部被殺（`mutations-round5-empty.txt`）。


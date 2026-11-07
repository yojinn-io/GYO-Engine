# v8 工具：voxel 地圖編輯器的視窗，照 Goxel 的功能與互動重做（`v8-tool-voxel-window-goxel`）

更新：2026-11-06。Owner：voxel 地圖編輯器線（本會話）。
- 上一版的視窗：[voxel-map-editor.md](voxel-map-editor.md)「v8-tool-voxel-map-editor-window」（PR #158；P1 不成立後凍結）。
- 區塊化：[voxel-map-tool-chunked.md](voxel-map-tool-chunked.md)（PR #163 已合併）。
- 紀錄：[HANDOFF](voxel-map-editor-HANDOFF.md)。

**狀態：2026-11-06 使用者核准 window-1 的宣告（原話「推奨どおり承認」：決定 1～5 全部照建議）。實作、審查修正、P1 完成（[dev_log](../../../dev_logs/2026_11_06_tool_v8_voxel_window_1.zh-Hant.md)）；等 §14.4 核准、空格子存檔的決定、L1、L3。2026-11-07 照使用者的決定拿掉標記放置 UI（見文末「實作後的變更」）。**

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-tool-voxel-window-goxel` | 整件事：視窗的功能與互動照 Goxel，外觀用我們自己的，程式碼自己寫 |
| `v8-tool-voxel-window-1` | 本批：區塊化的預覽與存檔、Goxel 的模式與核心工具、snap、拖曳中預覽、相機與快捷鍵、標記的放置 |
| `v8-tool-voxel-window-2` | 之後：選取（矩形、依方塊種類）、Move（旋轉、翻轉）、snap 的 Selection 與 Box、Symmetry（鏡像照 (c)） |
| `v8-tool-voxel-window-3` | 之後：Layers（格子格式 v2，Architecture Delta） |

## 已定的前提（使用者，2026-11-06）

- 功能與互動照 Goxel；外觀用我們自己的。
- 只取功能，不搬程式碼：我們的編輯邏輯（方塊種類、形狀、24 種朝向、區塊重烘）不同，要自己客製。
  - Goxel（GPL-3.0）只讀行為；不複製、不翻譯它的程式碼、圖示、著色器或 UI 字串。
  - 原始碼在使用者本機 `~/Code/GitHome/goxel-master`；本批讀過的檔案記在 dev_log。
- 分三批：window-1、window-2、window-3。
- 鏡像照 (c)：只有對稱的形狀可以鏡像，其餘拒絕；格式不變。這一點在 window-2 用到。
- Layers 在 window-3，會改格子格式（v2），到時另行宣告。

## 和上一版視窗的關係

PR #158 的分支已 merge master（`9c2a2479`）。本批在同一個分支、同一個 PR 上改寫。

| 上一版的內容 | 本批的處置 |
|---|---|
| 核心的 `EditSession`、拾取（DDA）、`CameraPose` | 保留並擴充：新工具的邏輯也放在核心，不碰 ImGui，可以在 CPU 上測 |
| 工具 Place／Erase／Paint／Pick／BoxFill／BoxClear | 改成 Goxel 的「模式×工具」 |
| 預覽：每次編輯整張 build、經文字往返，畫 v1（P1 不成立） | 改成區塊化：只重烘碰到的區塊，只重傳那些區塊的 mesh |
| 存檔：grid＋v1 單檔 | 存 grid＋區塊化地圖（＋標記檔）。見決定 1 |
| 上一版待決：W9 的 §14.4、DI 1 與 DI 5 的字句、S4、P1 | W9 併入本批的 W9（重新驗證）；S4 在本批解決；P1 改成本批的 P1 |

## 範圍（window-1）

### A. 區塊化的預覽與存檔

- **命令**：`edit <grid> <map-dir> [--markers <file> --marker-kinds <file>] [--gpu-driver <driver>]`。
  - `<grid>` 不存在時從新格子開始，和現在一樣。
  - 標記的兩個選項要一起給。
- **預覽**：
  - 開檔時 `BakeMap`；每次編輯後 `RebakeChunks(ChunksTouchedBy(...))`；
  - 每個區塊一個 Render mesh，只重建、重傳有變動的區塊；
  - 調色盤貼圖用 `ExpandVoxelPalette`，調色盤改了才重傳。
- **存檔**（Ctrl+S）：
  - 順序：grid → 地圖（`WriteMap`＋`SaveMap`）→ 標記（`WriteMarkers`）；
  - 每份都先檢查磁碟上的檔案是否被外部改過（FileChanged），包括 `map.json` 與標記檔；
  - 改過的話，開一個對話框（解決上一版的 S4）：
    - 從磁碟重新載入（放棄編輯）；
    - 覆寫；
    - 取消。
- **`measure-preview`**：改走區塊化的路徑，P1 用它量。

### B. Goxel 的功能與互動（自己寫，依我們的模型客製）

- **模式**：Add（T）、Sub（R）、Paint（G），在頂端列。
  - Add 放「選中的方塊＋朝向」；
  - Sub 清空；
  - Paint 換方塊種類、保留朝向（和現在的 Paint 相同）。
- **工具**：

  | 工具 | 鍵 | 我們的版本 |
  |---|---|---|
  | Brush | B | 大小 1～16 格；形狀：立方、球、圓柱（以格子取樣）。拖曳時沿路徑蓋章；Shift＋點擊，從上一個點畫直線 |
  | Shape | S | 從起點拖到終點：箱、球、圓柱（內切於那個箱）。可勾「留在起點的平面」 |
  | Laser | L | 沿著滑鼠的視線，清掉穿過的格子（只在 Sub 模式有意義；其他模式時當 Sub） |
  | Line | N | 點兩下畫一條直線，粗細用 Brush 的大小 |
  | Extrude | F | 拖曳一個面：把同一方向、在同一平面上相連的那片面，往外推出或往內縮 N 格 |
  | Plane | P | 點一個面，編輯平面移到那個面上，平面可以沿 X、Y、Z 任一軸（現在只有 Y） |
  | Picker | C | 點一格，選中它的方塊與朝向；任何工具下 Ctrl＋點擊也一樣 |

- **Snap**：
  - Volume：表面。Add 放在被點到的那一面外側的格子；Sub 和 Paint 作用在被點到的格子。
  - Plane：編輯平面。
  - 兩者可同時開，取先碰到的。
- **拖曳中預覽**：
  - 滑鼠懸停時，用線框顯示會受影響的格子；
  - 拖曳時，把結果套在一份暫存的格子上，即時重烘並畫出那些區塊；
  - 放開才確定，算一步 undo；Esc 取消。
- **朝向**：R 改給 Sub 用，朝向改用 Q／E（繞上方軸轉）、Shift＋Q／E（換上方）。
- **相機**（照 Goxel）：

  | 操作 | 動作 |
  |---|---|
  | 中鍵拖曳 | 旋轉 |
  | 右鍵拖曳 | 平移 |
  | 滾輪 | 縮放 |
  | 方向鍵 | 旋轉 |
  | 數字鍵 1／3／7／0 | 前視、右視、頂視、預設 |

  正交投影（Goxel 的 5）延後：framework 的 `OrbitCamera` 只有透視，要加正交得改 framework 或 Engine，不在本批。
- **提示**：視窗裡顯示一行操作提示（例如「拖曳一個面來擠出」），文字自己寫。
- **不變的**：Ctrl＋Z／Y、Ctrl＋S、調色盤與形狀庫的面板、離開時的未存檔提示。

### C. 標記的放置（`tc-markers` 的 UI 部分）

- **標記面板**：列出標記；kind 從 `--marker-kinds` 的清單選（不解釋名字）；id 自動給、可改；tags 可編輯。
- **放置**：
  - 點：點在表面上，yaw 預設取相機的水平方向，可在面板改；
  - 箱：像 Shape 一樣拖曳；
  - 路徑：逐點點擊，Enter 結束。
- **單位與檢查**：座標換成以 origin 為原點、1/16 格的整數。存檔前用 `UnknownMarkerKinds` 檢查，清單外的 kind 擋下存檔並指出是哪個標記。
- **顯示**：標記以線框疊在預覽上。

### D. T-junction 目視（tool-chunked 決定 5 移過來）

L3 時由使用者看。

### 不做（之後或不適用）

- window-2：選取、Move、Selection 與 Box 的 snap、Symmetry。
- window-3：Layers。
- 正交投影、Marching Cubes、材質、光照、算圖、匯出面板（匯出就是我們的 `build-map`）。

## Architecture Delta（AGENTS §3）

1. **Feature**：大地圖的編輯（big-map 線），以及使用者要的 Goxel 互動。
2. **現在的問題**：
   - 視窗的預覽與存檔走 v1 單檔，P1 在大地圖上差兩個數量級；
   - 工具與互動不是使用者要的形式。
3. **變化的邊界**：
   - `edit` 命令的參數：`<mesh>` 改成 `<map-dir>`，加上標記的兩個選項；
   - 視窗寫的檔案：區塊化地圖與標記檔。
4. **受影響**：
   - `tools/voxel_map_editor`（核心的編輯模型、視窗）；
   - `tests/voxel_map_editor`；
   - `docs/voxel_map_editor/architecture.md`；
   - 本線的計畫文件。
   Engine、framework、產品不改。
5. **依賴方向**：不變。視窗的連結清單不加新 target；`GYO::MapMarkers` 已由核心帶入。
6. **Ownership**：不變。
7. **為何不能更小**：預覽要能撐大地圖，就得用區塊化的路徑；互動的形式是使用者指定的。

## Design Invariants 的變更（請一併核准；寫進 `docs/voxel_map_editor/architecture.md`）

- **DI 5（只寫自己做的）**：視窗寫命令列指定的 grid、地圖目錄（和 `build-map` 相同的規則）、標記檔；三者都不覆寫被外部改過的檔，除非使用者在 FileChanged 對話框選了覆寫。
- **DI 7（看到的就是遊戲讀的）改寫**：
  > The preview draws, per chunk, the geometry Engine's `ExpandVoxelChunk` gives for the chunks `BakeMap` and `RebakeChunks` keep (each accepted by `ValidateVoxelChunk`), with the palette image of `ExpandVoxelPalette`; saving writes those same chunks (`WriteMap` reads them back equal). The editor makes no triangles of its own from the grid.
  - 守衛：W6、W7 與它們的突變。
  - 拖曳中的暫存預覽也走同一條路徑。
- **DI 1**：字句不變，但上一版視窗批的 W9（守衛的 §14.4）沒有核准完，本批重新驗證後再請核准。

## 判定（現在固定）

| 代號 | 判定 |
|---|---|
| W1 | 編輯模型（CPU）：每個工具與模式的結果和預期的格子集合相等。<br>- Brush：三種形狀、大小 1／2／5，含沿路徑蓋章與 Shift 直線；<br>- Shape：箱、球、圓柱；<br>- Laser、Line、Extrude：往外、往內、邊界與相連範圍；<br>- Plane：三個軸；<br>- Picker 與 Ctrl＋點擊；<br>- Snap：Volume 與 Plane 的規則。 |
| W2 | undo：一次拖曳是一步；Esc 取消後文件完全不變；拖曳中的預覽不進 undo |
| W3 | 拾取：上一版的 W3 照舊，並加上任意軸的編輯平面 |
| W4 | 拾取的 GPU 煙霧測試：上一版照舊（本機，gpu 標籤） |
| W5 | 存檔：<br>- grid、地圖、標記三份都寫對，讀回相等；<br>- FileChanged 對話框的三個選項各自的結果；<br>- 中途失敗時，已寫的與沒寫的檔案都符合 DI 5 |
| W6 | 看到的就是遊戲讀的：預覽每個區塊的頂點，等於 `WriteMap` 的文字讀回後 `ExpandVoxelChunk` 的結果（逐位元） |
| W7 | 差分預覽：200 組隨機編輯序列，預覽持有的每個區塊 mesh，等於全部重烘後展開的結果 |
| W8 | CLI：`edit` 與 `measure-preview` 的參數檢查；沒有視窗的版本說明後結束，不寫檔 |
| W9 | 依賴守衛：自我測試；§14.4 的 1～5 由另一位驗證者用實際資料確認（含上一版沒核准完的部分） |
| W10 | ミューテーションテスト：新突變（`vw1-*`）全部被殺；相關的既有突變重跑 |
| W11 | 全量建置成功；本機 `cpu|shader` 全部通過；L1 三平台通過 |
| P1 | **影格時間**：本機 Intel Mac，RelWithDebInfo，1280×720；像建築的合成圖（tool-chunked 的 `measure-map` 產生的同一張，768 區塊）。600 幀，丟掉前 60 幀。<br>- (a) 不編輯：p95 ≤ 16.7 ms；<br>- (b) 每幀用 Brush（大小 1）改一格，位置依固定序列輪流落在區塊內部與邊界：p95 ≤ 33 ms。<br>件數：要求的幀數＝量到的幀數；編輯失敗為 0。不一致時不判定，交使用者。 |
| L3 | 使用者用 Goxel 式工具做一張有二樓與階梯的地圖，放幾個標記，存檔；`verify-map` 與 `check-markers` 通過；目視 T-junction |

## 分批、模型、ギア、估時

```text
v8-tool-voxel-window-1（上段：Data Contract 的寫出、DI 5／7）
├── vw1-declare   Opus／medium｜1.5～2 h ：本宣告
├── vw1-preview   Opus／high  ｜6～9 h   ：區塊化預覽、存檔、FileChanged 對話框、measure-preview
├── vw1-tools     Opus／high  ｜10～15 h ：模式×工具、snap、拖曳中預覽、相機、快捷鍵、提示（邏輯在核心）
├── vw1-markers   Opus／high  ｜4～6 h   ：標記面板與放置
└── vw1-measure   Opus／medium｜2～3 h   ：P1，接著 L3（使用者）
```

| 角色 | 模型／ギア | 理由 |
|---|---|---|
| 實作者 | Opus 5.5／high（本會話） | 上段 |
| 審查者 | Opus／high（另一個子 Agent） | 上段 |
| §14.4 驗證者 | Opus／high（與提案者不同的子 Agent） | 要確認第 4 點 |

- 合計 23～35 h。超過 53 h，或單一問題累計超過 2 h，停下回報。
- 證據：主 checkout `build/evidence/tool-v8-voxel-map/window-1/`。

## 要使用者決定的事

1. **視窗存什麼**（建議：grid＋區塊化地圖＋標記）。
   - 產品到 arena-v3 之前讀的是 v1。視窗不再寫 v1，要 v1 時用 CLI 的 `build`。
   - 另一個選項：視窗也同時寫 v1。但大地圖的 v1 會超過 16 MiB，被 Engine 拒收。
2. **P1 的門檻**（建議：不編輯 p95 ≤ 16.7 ms、每幀編輯 p95 ≤ 33 ms）。
3. **Extrude 的範圍**（建議：同一方向、在同一平面上、相連的那片面）。另一個選項是只推一格的面。
4. **Laser 在 Add／Paint 模式**（建議：一律當 Sub）。另一個選項是沿視線塗色。
5. **正交投影延後**（建議延後；要做的話是 framework 或 Engine 的變更，另案）。

## 實作後的變更（2026-11-07，使用者決定）

- **拿掉 C「標記的放置」**。依據是使用者的決定 `v8-arch-map-scene-split`（經架構師轉達，PR #164 `bcea47de`）：地圖是幾何，場景是放在幾何上的東西，兩者分開；場景編輯器是另一個工具，voxel 地圖編輯器只編幾何。使用者在本會話確認（原話「外す」）。
  - 拿掉的：
    - 工具 Marker（K）與標記面板；
    - `edit` 的 `--markers`、`--marker-kinds`；
    - 存檔時寫標記；
    - vw1 加進 `MarkersFile` 的放置輔助（還原成 master 的狀態）；
    - 對應的測試，以及 3 條突變（`vw1-save-unknown-kinds`、`vw1-save-markers-unchecked`、`vw1-marker-order`）。
  - 留下的：tc-markers 的讀寫核心（`MarkersFile`、`check-markers`、Engine 的兩個函式），記為待搬到場景編輯器（HANDOFF 的 TODO #8），不再往上加功能。
  - 判定的影響：
    - W1 少了標記的部分；
    - W5 少了標記檔的寫入、清單外的 kind、標記寫不出等三條；
    - W8 改成「`--markers` 是用法錯誤」。
    - 這些都是隨功能一起移除，留下的判定沒有放寬。


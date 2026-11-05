# 工具 v8：v8-tool-voxel-map-editor-core（格子格式、build、CLI）

日期：2026-11-05。分支 `claude/v8-tool-voxel-map-plan`（PR #152）。上段；實作是主會話（Opus 5.5／high），審查是子 Agent（Opus 5.5／high）。事前宣告在 [voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md)「v8-tool-voxel-map-editor-core 的事前宣告」，2026-11-05 使用者核准（「核准」）；實作和宣告的差異在審查前回報，使用者確認（「確認，送審查」）。

**狀態：** 本機判定 E1～E9 通過；E10（L1）待 CI。

相關文件：
- [voxel_map_editor/architecture.md](../voxel_map_editor/architecture.md)：格式、朝向表、build、CLI、Design Invariants 6 條、Replacement Criteria 4 條
- [voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md)

## 做了什麼

- **新的工具 owner `voxel_map_editor`**：
  - `tools.csv` 一列（enabled、default 是 true，release 是 false，三平台）；
  - `project.json` 只有 `cli` 一個 variant；
  - 用 `gyo_tool_install_target` 登錄執行檔。
- **核心程式庫 `gyo_voxel_map_editor_core`**：
  - `gyo.voxel_grid` 的讀寫：嚴格驗證，寫出的格式固定；
  - 24 種朝向，以及以格子中心為軸心的旋轉；
  - build：cullface、方向亮度、碰撞盒合併（沿 y 逐層）、Engine 的正規順序，寫出前用 `ParseVoxelMesh` 讀回；
  - 盒子匯入（用 JSON pointer 指出位置）；
  - 經 `engine/io` 讀檔與原子替換寫檔。
- **CLI `gyo_voxel_map_editor`**：`build`、`verify`、`inspect`、`import-boxes`，經 framework 的 headless 外殼執行。
- **測試**：
  - 核心測試：規則、朝向的字面表、cullface 的完整清單、200 組隨機合併、黃金檔、sha、匯入；
  - CLI 測試（`cmake -P`）；
  - 依賴守衛和它的自我測試；
  - 黃金檔 `fixtures/sample.voxel_grid.json` 與 `sample.voxel_mesh.json`，用 `.gitattributes` 固定位元組。
- **突變目錄** `docs/tools/plans/v8/scripts/voxel_map_editor_mutations.json`，20 條。

## 判定

| 判定 | 結果 |
|---|---|
| E1 格子格式的規則 | 通過（審查後補齊各規則的測試） |
| E2 朝向 | 通過：24 個矩陣兩兩不同、正交、行列式 +1；字面表；「上方」的那一欄；樓梯轉 90°、180°、倒放的手算值 |
| E3 cullface | 通過：1 個立方體、2 個立方體、立方體旁的半磚，比對完整清單；樓梯兩側不對稱的情況 |
| E4 碰撞 | 通過：200 組隨機（固定的 LCG），聯集相同、互不重疊、有序；2×1×1 合成 1 個盒子、L 形 2 個、三格高的柱 1 個 |
| E5 確定性 | 通過：黃金產物逐位元組相同（核心測試和 CLI 測試都比對）；同一輸入 build 兩次相同 |
| E6 一個驗證者 | 通過：空的格子 build 出來沒有面，Engine 拒絕，什麼都不回傳；CLI 的 `build_refused` 也沒有寫檔；突變 `vme-readback-ignored` |
| E7 CLI | 通過 |
| E8 依賴守衛 | 通過 |
| E9 突變 | 第一輪 15／16；修正突變目錄後 16／16。審查後 20 條，合計 20／20 killed（見「經過」6） |
| E10 L1 | 待 CI |

## 經過

1. **冒煙測試**：用現行的 `pvp_arena.json`，`import-boxes … /walls` → 260 格 → `build` 得到 696 個四邊形、5 個碰撞盒；`verify` 判定相同；Engine 讀得回。
2. **突變第一輪 15／16**。倖存的 `vme-repeated-key-kept`，是突變目錄的 `cases` 含逗號（`*fields, schema and version*`），doctest 在逗號處切開，一個測試都沒選到。屬於 Test 自身的錯誤，不是實作的 Defect。改成 `*schema and version*` 後重跑，被殺死。
3. **和宣告的差異**：照承諾，在審查前回報，使用者確認：
   - `convert-arena-v1` 改成中立的 `import-boxes`；
   - 結束碼對齊 model_viewer；
   - 碰撞盒合併改成沿 y 逐層做；
   - 格子格式的範圍規則；
   - E6 的做法；
   - fixtures 的 `.gitattributes`。
4. **審查（Opus／high）**：沒有 blocker。審查者手算核對了黃金產物約 40 筆四邊形和全部 12 個碰撞盒，都相符，所以黃金檔不是自我參照。修正如下：
   - **C1**：`import-boxes` 會寫出 `ParseGrid` 讀不回來的格子（origin 沒對齊 16 時，區塊整塊的範圍檢查會失敗）。範圍規則改成只檢查非空格，和文件一致；匯入寫出前先用 `ParseGrid` 讀回。
   - **C2**：一個特定的 JSON pointer（`/w/18446744073709551616`）讓程式 abort，結束碼 134。改成在 try 裡追蹤 pointer，任何例外都轉成輸入錯誤。
   - **C3**：`inspect` 原本用子字串判斷格式；改成讀 `schema` 欄位。
   - **C4**：超過 16 MiB 的格子，結束碼不一致；改成讀檔上限放寬，由 `ParseGrid` 拒絕。
   - **C7**：`GridDocument::Set` 把空格的朝向正規化成 0。
   - **T1**：24 種朝向的字面表寫進測試和文件。朝向編號會存進格子檔，所以這張表屬於格式契約。
   - **T2**：補齊規則測試：方塊為空、重複、超過 256；形狀超過 256；巢狀的未知欄位與重複 key；小數的整數欄位；±2^30；16 MiB；origin 的下界；格子範圍的上下界與沒對齊的 origin；runs 為空、長度為 0、形狀不對。
   - **T3**：sha 用非正規的原始文字測，證明算的是原始位元組。
   - **T4**：E3 改成比對完整清單。
   - **A1**：補上 `gyo_tool_install_target`。
   - **A2**：拿掉沒用到的 `GYO::Math` 連結，以及守衛裡對應的允許項。
   - **A3**：DI 4 的守衛描述改成實際的測試。
   - 文件：C5（「非正規的順序」，不是「非正規的文字」）、C6（`cell_size` 寫成 double 的最短十進位，並同步改 `voxel-mesh.md` 對寫出端的描述）、C8（匯入的寬鬆之處）、亮度的進位方式。
   - 新增 4 條突變：`vme-block-name-repeat`、`vme-cell-range-unchecked`、`vme-up-renumbered`（只有字面表擋得住）、`vme-import-pointer-throws`。
5. **又踩到 Windows 的 `far` 巨集**：審查後新增的測試變數 `far`，在推送前就改名了（上一批 CI 的教訓）。

6. **審查後的突變 19／20**：倖存的 `vme-verify-always-same` 第一輪是被殺死的。原因是審查後加了 `gyo_tool_install_target`，執行檔改輸出到 `build/target/_tools/voxel_map_editor/bin/`；runner 的 `{exe:}` 只在建置目錄裡找，找到的是先前留下、沒被突變的舊執行檔（19:15）。屬於計測路徑的問題，不是實作或測試的錯。刪掉舊執行檔，並讓突變的指令直接指向新位置（Windows 也找 `.exe`）之後重跑，被殺死。CLI 測試本身用的是 `$<TARGET_FILE>`，不受影響。

## 審查者量到的效能（參考，不是判定）

| 輸入 | 結果 |
|---|---|
| 256×64×256 實心（4.2M 格） | build 3.0 s、328 MB，產物 7 MB，1 個盒子 |
| 128×64×128 三維棋盤 | 3.3 s、549 MB；產物超過 16 MiB，Engine 拒絕（什麼都沒寫） |

病態的格子（16 MiB 塞滿最小的區塊）可能吃到約 7 GB 記憶體。這是推論，沒有實測；目前不處理，列為候選。

## 證據

主 checkout（macOS Intel）的 `build/evidence/tool-v8-voxel-map/core/`：

- `configure.txt`、`build.txt`、`ctest.txt`、`ctest-tool.txt`
- `ctest-cpu-shader.txt`：整套 128／128
- `mutations.txt`、`mutations-rerun.txt`
- `build-after-review.txt`、`ctest-tool-after-review.txt`、`mutations-after-review.txt`

審查者的探測輸入在會話的暫存區，未存證；判定沒有用到它們。

## 用時

約 5 h（估 14～20 h）。

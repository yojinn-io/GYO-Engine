# v8 voxel 地圖編輯器交接

更新：2026-11-05。Owner：工具 `voxel_map_editor`、Engine（`GYO::VoxelMesh`）、framework（3D 預覽元件，owner 是架構會話）、`object_fps_pvp`（arena v2、step-up）。分工見 [voxel-map-editor.md](voxel-map-editor.md)。

本文件是 `v8-tool-voxel-map-editor` 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [voxel-map-editor.md](voxel-map-editor.md)。這條線 2026-11-05 移出工具線（工具線 [HANDOFF](HANDOFF.md) 的 `v8-tool-voxel-map-editor-moved-out`）。2026-11-05 以前的決定與紀錄原本寫在工具線的 HANDOFF，同日搬到這裡（`v8-tool-voxel-handoff-split`）。

## 現況（每次停下來時更新）

- **PR**：
  - PR #152（plan、contract、core、arena-v2）已合併（`a9ccad7d`）。
  - window 批在分支 `claude/v8-tool-voxel-map-window`，有它自己的 PR。
- **完成**：
  - plan
  - `v8-engine-voxel-mesh-contract`
  - `v8-tool-voxel-map-editor-core`
  - `v8-product-arena-v2`（L3 合格）
  - `v8-framework-preview-component`（架構會話，PR #157）
- **進行中**：`v8-tool-voxel-map-editor-window`
  - 功能、審查修正、測試都完成。
  - dev_log：[2026_11_06_tool_v8_voxel_map_editor_window](../../../dev_logs/2026_11_06_tool_v8_voxel_map_editor_window.zh-Hant.md)。
- **等你決定**：
  - 守衛判定變更（W9）的 §14.4 核准；DI 1 和 DI 5 這次的文字修改，也一起請你核准。
  - S4：檔案被外部修改之後，視窗要不要加「重新載入」或「另存」。
- **等架構會話**：P1（`v8-tool-voxel-window-p1-to-architect`）。
- **之後**：`v8-tool-voxel-map-editor-trial`（P1 照架構師的結論重新宣告；L3：用 GUI 建一張有二樓和階梯的地圖）。`v8-product-step-up` 由產品會話做。

## 決定紀錄

| 代號 | 決定 | 使用者原話 |
|---|---|---|
| `v8-product-arena-v2-maps-move` | （2026-11-05）把延後的 `v8-product-asset-path-loaded-move` 併進 arena-v2 批：arena 一組（含烘焙地圖）搬進 `maps/`，`movement_rules.json` 搬進 `rules/`，根目錄的例外全部刪除；不加新例外 | 「这2个问题就能放到pvp的assets的config下么」→ 選「今 maps/ に移す」 |
| `v8-product-arena-v2-p3-regression` | （2026-11-05）審查實測：P3 的軌跡對「集合相同、分法不同」沒有檢出力（AGENTS §14 停下回報）。P3 降為回歸檢查，退役證據只靠 P2（集合相同）；P3 的腳本加上繞柱、沿柱滑、斜進四角（只收緊） | 選「回帰チェックに下げる（推奨）」 |
| `v8-framework-preview-component-owner` | （2026-11-05）framework 的 3D 預覽元件批交給架構會話（framework 的 owner）做；本線等它的分支與 PR，再接 `v8-tool-voxel-map-editor-window` | 「架構師【アーキテクト】」 |
| `v8-product-arena-v2-maps-guard-approved` | （2026-11-05）核准資產守衛 DI-2 的變更（§14.4，驗證者確認後的 1～5）：`maps/` 為 text 專用的第一層資料夾、RETAINED 加 2 個烘焙地圖 id、刪除根目錄的 4 筆例外 | 「承認する」 |
| `v8-product-arena-v2-maps-guard` | （2026-11-05）資產標準 DI-2 的守衛加 `maps` 資料夾種類：照 §14.4 先做突變與另一個驗證者的確認，再請你核准 | 選「手続きを進める（推奨）」 |
| `v8-product-arena-v2-p10` | （2026-11-05）P10（L3）的判定事前定為：形狀、位置、顏色與碰撞的手感一致即通過（**已被 `-p10-rewrite` 取代**） | 沒有偏好，採建議的定義 |
| `v8-product-arena-v2-p10-rewrite` | （2026-11-05）P10 改成「看起來能玩」（沒有破洞、沒有閃爍、牆和地板分得出來、看得到對手）；加 CI 判定 P11：碰撞外露的面都被畫出的面蓋住、畫出的面背後都有實心 | 旁支會話「btw: 這個是什麼意思。」轉來：「如果烘培出的mesh，是沒有辦法判斷的呢？方塊地圖可能和都會有差別。」；本會話選「この案で書き直す（推奨）」 |
| `v8-product-arena-v2-declare` | （2026-11-05）核准 arena-v2 批的事前宣告（P1～P10） | 「承認」 |
| `v8-product-arena-v2-render` | （2026-11-05）arena-v2 批由 Client 畫烘好的 mesh（R2） | 選「R2 描く（推奨）」 |
| `v8-product-arena-v2-merge` | （2026-11-05）碰撞盒合併改成條帶式：每一行取沿 z 的最長段，完全相同的段沿 x 合併，相同的矩形沿 y 往上長 | 選「帯状に変える（推奨）」 |
| `v8-tool-voxel-map-editor-core-deviations` | （2026-11-05）確認 core 批實作和宣告的差異：`convert-arena-v1` 改成中立的 `import-boxes <json> <pointer> <grid>`；結束碼對齊 model_viewer（1 輸入、2 用法、3 讀寫、4 verify 不同）；碰撞盒合併改成沿 y 逐層做；格子格式的範圍規則；E6 的做法；fixtures 的 `.gitattributes` | 「確認，送審查」 |
| `v8-tool-voxel-map-editor-core-declare` | （2026-11-05）核准 core 批的事前宣告，包括「`walls≤1024` 工具不檢查，由產品擋下」的解讀 | 「核准」 |
| `v8-tool-voxel-handoff-split` | （2026-11-05）voxel 這條線另開專用的 HANDOFF，把工具線 HANDOFF 裡的 voxel 決定與紀錄搬過來 | 「另開 HANDOFF」 |
| `v8-engine-voxel-mesh-contract-deviations` | （2026-11-05）確認實作和事前宣告的差異：`cell_size` 在 [2^-16, 2^16]、整數在 ±2^30、錯誤細節的路徑、`ExpandVoxelMesh` 的 assert（都是收緊，見 dev_log） | 「確認」 |
| `v8-engine-voxel-mesh-contract-declare` | （2026-11-05）核准事前宣告與名稱（`gyo.voxel_mesh`、`GYO::VoxelMesh`、`engine/voxel_mesh/`、`docs/architecture/voxel-mesh.md`） | 「核准」 |
| `v8-tool-voxel-contract-split` | （2026-11-05）契約拆兩層：格子原始檔（工具持有）＋建置產物（Engine 持有）；之後另外做場景編輯器，接手 arena v2 和玩法標記 | 「D1 契約：照建議，之後我會分出另一個場景編輯器。」 |
| `v8-tool-voxel-offline-build` | （2026-11-05）離線 build，產物存檔，遊戲只讀產物；碰撞也烘進產物；產物記原始檔的 SHA-256 和 build 版本 | 「我認為要存檔，否則後期載入時都會有細微差別，容易出bug，還不好查，有檔我至少可以反查。」 |
| `v8-tool-voxel-layers` | （2026-11-05）產物格式與讀取器＝Engine；格子格式、build、拾取＝工具；3D 預覽元件＝framework（第二個組合者，framework DI 5）；arena v2＝產品 | 「我們不光有工具和engine 還有framework。」→ 分配表：「可以」 |
| `v8-tool-voxel-units` | （2026-11-05）產物和格子用整數，1/16 格的子單位（原本建議整格，因為要有樓梯而重開） | 選「1/16 格」 |
| `v8-tool-voxel-artifact-encoding` | （2026-11-05）產物用 JSON 四邊形清單，讀取器展開成三角形；不依區塊切、不附空間索引 | 選「(d) JSON 四邊形（建議）」 |
| `v8-tool-voxel-render` | （2026-11-05）面剔除＋6 個方向的亮度；繪製的 greedy、AO 是候選 | 選「面剔除＋方向亮度（建議）」 |
| `v8-tool-voxel-arena-v2` | （2026-11-05）只有檔案的 schema 是 v2，`Arena.version` 維持 1；相鄰檔名引用產物；產物的碰撞填進 `Arena::walls`；wire 不變；`walls≤1024` 不放寬 | 選「(ii) 只有檔案是 v2（建議）」 |
| `v8-tool-voxel-grid-format` | （2026-11-05）格子檔：JSON、16³ 區塊分段、區塊內 RLE；形狀用資料定義（1/16 單位的盒子）；24 種直角朝向 | 「這個半轉是個例子，首先這個轉塊是給編輯器用的。後續還要轉成mesh。其次這個帶方向可以在編輯時可以更加方便做地形和裝飾。」；選「24 種直角朝向（建議）」「資料定義（建議）」「依區塊分段」 |
| `v8-tool-voxel-coords` | （2026-11-05）origin 的 y 固定 0，地板維持隱含平面；`width`／`depth` 在 arena v2 手寫 | 沒有偏好，採建議 |
| `v8-tool-voxel-spawn` | （2026-11-05）出生點手寫在 arena v2；`Arena::Validate` 改成「腳下要有支撐」，arena 契約變更，wire 不變 | 選「照這樣做（建議）」 |
| `v8-tool-voxel-stairs` | （2026-11-05）B1 用形狀和朝向做階梯（碰撞全是 AABB），step-up 列入本線、由產品會話做；B2 斜坡另立 Feature | 旁支會話「btw: 這個問題時,我要是要去二樓要如何處理?」的結論，回到本會話後修正為：「這個概念類似minecraft的半轉，利用這個機制我能做樓梯。人也可以走上去。」；step-up 選「列入本線，由產品會話做（建議）」 |
| `v8-tool-voxel-preview-framework` | （2026-11-05）PR #146 合併後抽出 framework 的 3D 預覽元件，model_viewer 同一批改用 | 選「#146 合併後抽（建議）」 |
| `v8-tool-voxel-first-tools` | （2026-11-05）第一版做基本工具；擠出、圖層、形狀筆刷延後 | 選「基本工具（建議）」 |
| `v8-tool-voxel-source-location` | （2026-11-05）原始檔放 `build/art-source/`，位置由編輯器決定；存檔時一定同時寫出產物；CI 不重新 build（修改 `v8-tool-voxel-offline-build` 原本的「CI 逐位元比較」） | 「讓編輯器自己找地方存。」→ 選「放 build/art-source」 |
| `v8-tool-voxel-retire-v1` | （2026-11-05）v1 退役：先比 AABB 集合，再比軌跡，最後 L3 | 選「先比集合，再比軌跡（建議）」 |

## 紀錄

- 2026-11-05：`v8-tool-voxel-map-plan`（medium，Opus 5.5）。主會話交來 `v8-tool-voxel-map-editor` 的規劃卡；先盤點現況，不先提案。你給了自己寫的報告，要我讀兩個參考專案（Goxel、MagicaVoxel），再指定參考 Godot 的 GridMap。兩輪 Fable／high 統合（唯讀，只有統合者，沒有敵對式檢查者）後逐點定案（決定紀錄的 `v8-tool-voxel-*`），方案與分批寫進 [voxel-map-editor.md](voxel-map-editor.md)。證據在主 checkout 的 `build/evidence/tool-v8-voxel-map/plan/`。dev_log：[2026_11_05_tool_v8_voxel_map_plan](../../../dev_logs/2026_11_05_tool_v8_voxel_map_plan.zh-Hant.md)。
- 2026-11-05：你說「開始」，開工 `v8-engine-voxel-mesh-contract`（high，Opus 5.5）。讀了先例：`animation-events.md` 的契約與 DI、`engine/model` 的 CMake 與連結守衛、`anim_events_mutations.json`、Render 的正面方向（`RenderDeviceTypes.hpp:28-29`，順時針）、`Aabb` 在 Math。重估 8～12 h，和計畫相同。事前宣告寫進 [voxel-map-editor.md](voxel-map-editor.md)「v8-engine-voxel-mesh-contract 的事前宣告」，等你核准；核准前不實作。
- 2026-11-05：你核准事前宣告，實作 `v8-engine-voxel-mesh-contract`。本機 14／14 通過，突變 16／16 killed；Opus 審查無 blocker，修 C1（`cell_size` 上限算錯）、C2（沒有下限）與測試缺口後，329 個斷言通過、突變 20／20 killed。和宣告的差異列在 dev_log，等你確認。證據在主 checkout 的 `build/evidence/tool-v8-voxel-map/engine-contract/`。
- 2026-11-05：PR #152 的 CI 處理。
  - Windows L1 編譯失敗：測試變數 `far` 和 `windef.h` 的巨集衝突，改名為 `atEdge`（`3e6a46f6`），判定不變。
  - 修正推送後 CI 沒有跑：PR 和 master 衝突（#146 合併時，工具線把 voxel 移出，改到 README／HANDOFF 的同一段）。合入 master（`62b6f0cc`），保留「移出」的說法，加上指向 voxel-map-editor.md 的連結。
  - 之後 L1 三平台通過，記錄在 dev_log（`6ca245b1`）。
- 2026-11-05：你說「另開 HANDOFF」：建立本文件，搬移 17 筆決定與 3 筆紀錄（`v8-tool-voxel-handoff-split`）。
- 2026-11-05：你說「開始」，開工 `v8-tool-voxel-map-editor-core`（high，Opus 5.5）。讀了 model_viewer 的登錄、CMake、測試結構和 DI，以及 `tool_projects.md`、`Sha256`、`WriteFileAtomically`。重估 14～20 h。事前宣告寫進 voxel-map-editor.md，等你核准。
- 2026-11-05：你核准 core 批的宣告，實作完成。本機：工具 ctest 4／4，核心測試 10037 個斷言；整套 cpu|shader 128／128；突變第一輪 15／16。倖存的 `vme-repeated-key-kept` 是突變目錄的篩選字串含逗號，doctest 切開後一個測試都沒選到，屬於 Test 自身的錯誤；修正後重跑 killed，合計 16／16。實作和宣告有差異，審查前先請你確認。證據在主 checkout 的 `build/evidence/tool-v8-voxel-map/core/`。
- 2026-11-05：core 批的審查（Opus／high）沒有 blocker。修 C1（匯入的格子讀不回來）、C2（pointer 讓程式 abort）、T1（朝向的字面表）、T2（補齊規則測試）、A1（`gyo_tool_install_target`）和其他 nit。突變審查後 20 條：19／20，倖存的一條是 runner 找到了舊位置的執行檔（計測路徑），修正後 20／20。提交並推送，等 L1。
- 2026-11-05：core 批的 L1 三平台通過（run 42010316153，`5f410c60`），批次完成。
- 2026-11-05：你選 `v8-product-arena-v2`，由這個會話做。讀了 `Arena.cpp` 的載入與 digest、`Arena::Load` 的 20 多個呼叫端、`Movement.cpp` 的站立判定、資產契約測試的 RETAINED 與 root-file 例外。發現：現在的合併得到的盒子集合和 v1 不同（盒子數相同、分法相反）。重估 10～14 h。事前宣告寫進 voxel-map-editor.md，等你核准。
- 2026-11-05：arena-v2 批審查（Opus／high）：沒有 blocker。實測發現 P3 對分法不同的分解沒有檢出力，照 AGENTS §14 停下回報；你選 P3 降為回歸檢查、`maps` 守衛照 §14.4 走程序，P10 採建議的定義。其餘 should-fix 照審查修。
- 2026-11-05：arena-v2 批合入 master（`0c5db246`，`mutations.json` 依 id 合併兩邊的突變）。本機 cpu|shader 129／129；L1 三平台綠（run 42034025482）。剩 L3（P10）。
- 2026-11-05：arena-v2 批的 L3（P10）你在 macOS 判定「問題なし（合格）」。啟動腳本與 log 在主 checkout 的 `build/evidence/tool-v8-voxel-map/arena-v2/l3/`（`pvp_corners` 兩人同房、命中 26）。arena-v2 批完成。下一批：`v8-framework-preview-component`（要和架構會話協調）；`v8-product-step-up` 由產品會話做。
- 2026-11-05：你問編輯器做好了沒：只完成資料格式、build、CLI 與遊戲端讀取，GUI 視窗還沒做。framework 批交給架構會話（「[Job][架構師]例行架構掃描」），已送出委託（範圍、留在 model_viewer 的部分、客戶端建立元件、voxel 編輯器的需求）。等它完成後接 window 批。
- 2026-11-06：PR #152 已合併（`a9ccad7d`）。架構會話完成 `v8-framework-preview-component`（PR #157，合併為 `2cd57244`）。
- 2026-11-06：你說「開始」，開工 `v8-tool-voxel-map-editor-window`（high，Opus 5.5）。從 master `2cd57244` 切出分支 `claude/v8-tool-voxel-map-window`。
  - 讀了 `docs/framework/preview.md` 的 DI 與 Replacement Criteria、工具的 DI，以及 model_viewer 的 gui 版本（CMake、`PreviewMeasure`、依賴守衛）。
  - 重估為 16～22 h。
  - 事前宣告寫進 voxel-map-editor.md，裡面包括：改 DI 1 和 DI 5、新增 DI 7，以及判定 W1～W11 和 P1。等你核准，核准前不實作。
- 2026-11-06：你核准 window 批的宣告（「承認する」），開始實作。
  - 核心的編輯模型、拾取、存檔（`Editing.hpp`／`Editing.cpp`）寫完；W1～W3、W5～W7 的測試共 34 個 test case，本機全部通過。
  - 視窗、預覽、`measure-preview` 都能建置。
  - **停下（AGENTS §14：原因改變計畫的前提）**：用 `measure-preview` 試跑 30 幀（探索用，不是 P1 的正式量測）。
    - 合成地圖（64×16×64、填 30%）：每幀約 2.5 s。
      - 產物有 124,236 個四邊形、20,691 個碰撞盒，文字 5.3 MB；
      - build 約 1.36 s，其中 Engine 讀回約 1.2 s；
      - 預覽又再讀一次，約 1.2 s。
    - `pvp_corners`：p95 13.1 ms。
    - 結論：「每次編輯都整張 build，並經過文字往返」在合成地圖上差了兩個數量級，宣告中的 P1 (b) 必然不過（Replacement Criteria 3）。
  - 程式留在工作樹，沒有提交。
  - 證據在主 checkout 的 `build/evidence/tool-v8-voxel-map/window/explore-p1/`。
  - 等你決定做法。
- 2026-11-06：你決定（經旁支會話轉達，`v8-tool-voxel-window-p1-to-architect`）：
  - 原話：「現在為何會有地圖編輯器，就是在為大地圖做準備。要不然就更本不需要編輯器這個東西。」「把這個問題轉給架構師，讓他來考慮。現在 map editor 這邊就想像如何做出editor，性能問題由架構師來處理。」
  - 效能問題（P1）交給架構會話，旁支已把觀測轉過去。
  - 縮小測試地圖或範圍的做法（A／D）不採用，因為大地圖正是編輯器存在的理由。
  - 這一批繼續完成編輯器的功能與互動。P1 的宣告怎麼改，等架構師的結論；要改宣告時先回報你。
- 2026-11-06：window 批的實作與審查。
  - 審查（Opus／high）：
    - B1：undo 之後索引越界；
    - S1～S3：框選上限、拖曳中存檔、核心可以 include render 的標頭；
    - nit：N1～N9。
    - 以上都修了。S4 交你決定。
  - §14.4 驗證者（Opus／high）做了四輪，前三輪都找到守衛隱藏的放寬。每次都把守衛改嚴，沒有放寬文字。
  - 第四輪剩下的罕見寫法，修好後由提案者自己重跑確認。
  - 突變：視窗 22 條、相關的既有突變都 killed。
  - 本機結果：cpu|shader 130／130；編輯器與 GPU 6／6。
  - 證據在主 checkout 的 `build/evidence/tool-v8-voxel-map/window/`。

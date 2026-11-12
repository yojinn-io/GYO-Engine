# 事前宣告：`v8-arch-remove-object-fps`（移除 object_fps、object_fps_v2、object_fps_preview，兼驗 §7 removability）

更新：2026-11-12。Owner：架構（架構師會話）。**狀態：使用者 2026-11-12「A 刪、B 搬 pvp、C 照宣告，開始」；執行中。** 三項的決定：A owner 文件刪；B `3d-assets.md` 的授權段搬進 `docs/object_fps_pvp/assets.zh-Hant.md` 當附錄、刪檔；C 判定 2 不成立時停下記 Delta、不修。**宣告錯誤的修正（使用者 2026-11-12 選 (a)）**：宣告「不碰」清單寫 pvp 零引用，實為架構師掃描錯誤（BSD grep 無 `-P`）；`tests/object_fps_pvp/` 的 8 行文字（`HitboxSurvey.cpp` 2 行註解、`test_asset_catalog.py` 3 行自測假路徑、`fixtures/hair_buns/README.zh-Hant.md` 2 行、`asset_sources.json` 1 行出處）由主會話以第四個 commit 改寫，判定 3 的 grep 範圍與零命中要求不變。

決定來源：HANDOFF `v8-arch-remove-object-fps`（使用者 2026-11-11，側邊會話轉述，spec K.3 第 8 項：PvP 已有自己的生態，不需要這個「錨」；照 AGENTS §7 完整刪除、不留相容層；排成改造前的獨立批）；`v8-arch-remove-runtimeport-animinstance`（使用者 2026-11-11，audit 待決第 3 項：刪 `IRuntimePort` 與 `AnimationInstance` 含各自測試，併入本批；取樣函式與 ufbx loader 不動）。

## 目的

兩件事，一次做完：

1. **刪除**兩個停用產品（`object_fps`、`object_fps_v2`）、依賴它們的工具（`object_fps_preview`），以及只有它們在用的兩個 Engine 型別（`Engine::Runtime::IRuntimePort`、`Engine::Model::AnimationInstance`）。改造批不必再為這三個 owner 規劃遷移或保留相容層。
2. **驗證 AGENTS §7 的性質**：「刪除一個 Product，共通層不需要改」。這是改造前最便宜的一次 removability 實測——如果刪除要動到 `build/cmake`、`build/ci`、`.github`、頂層 CMake、`tests/common`（宣告列出的兩處除外），就是 §7 不成立的證據，照 §3 記 Architecture Delta 回報，**不在本批修**。

## 範圍

### 刪除清單（§7 六類；檔數以 master `56b8dda3` 的 `git ls-files` 計）

| §7 類別 | 路徑 | 檔數 |
|---|---|---|
| Content（程式） | `apps/object_fps/`、`apps/object_fps_v2/` | 71、78 |
| Content（工具） | `tools/object_fps_preview/`（`project.json` 宣告 `requires_apps.game = object_fps`） | 5 |
| Content（資產） | `assets/object_fps/`、`assets/object_fps_v2/`（約 55 MB、46 MB；不走 LFS） | 41、49 |
| Content（owner 文件） | `docs/object_fps/`、`docs/object_fps_v2/`；`docs/architecture/3d-assets.md`（授權段先搬進 pvp 的 assets 文件；使用者決定 B） | 2、3、1 |
| Registration | `engine/config/projects.csv` 的 `object_fps`、`object_fps_v2` 兩列；`engine/config/tools.csv` 的 `object_fps_preview` 一列 | 修改 2 檔 |
| Owner-specific Test | `tests/object_fps/`、`tests/object_fps_v2/` | 26、11 |
| Owner-specific Validation | `build/acceptance/object_fps/`、`build/acceptance/object_fps_v2/` | 14、5 |
| Engine 型別（使用者決定併入） | `engine/runtime/include/engine/runtime/IRuntimePort.hpp`（刪檔）；`engine/model/include/engine/model/Animation.hpp` 第 72～97 行 `AnimationInstance` 類別與 `engine/model/src/Animation.cpp` 第 192～241 行的定義（刪段；`PlaybackInterval`、`SamplePose`、`BlendPoses`、`TransferCompatibleAnimation`、`SkinMesh`、ufbx loader 全部保留——`PlaybackInterval` 仍被 `tools/model_viewer` 用） | 刪 1 檔、修 2 檔 |
| Engine 型別的測試 | `tests/common/core/runtime/RuntimePortTests.cpp`（刪檔，1 個 TEST_CASE）＋ `tests/common/core/CMakeLists.txt` 第 16 行；`tests/common/model/ModelTests.cpp` 用 `AnimationInstance` 的 3 個 TEST_CASE（第 139、170、213 行起；其餘 14 個不動） | 刪 1 檔、修 2 檔 |
| Reference（現行文件） | 見下「文件的處置」 | 修約 20 檔 |

預計：**刪除 308 檔**（上表刪檔合計，含 `3d-assets.md`）、**修改約 26 檔**（2 登記＋4 共通測試／Engine＋約 20 文件）。實際數字由報告的件数の突き合わせ回填。

### 不碰

- `apps/object_fps_pvp/`、`tests/object_fps_pvp/`、`build/acceptance/object_fps_pvp/`、`assets/object_fps_pvp/`、`docs/object_fps_pvp/`（文件只改「現行引用」兩處，見下）。經負向前瞻 `object_fps(?!_pvp)|ObjectFps(?!Pvp)|IRuntimePort|AnimationInstance` 掃描，pvp 的程式、測試、驗收、資產**零引用**待刪項目；pvp 與舊產品共用的只有 namespace 名 `RetroFPS`（名字，不是依賴）。
- `build/cmake/`、`build/ci/`、`build/acceptance/common/`、`.github/`、`CMakeLists.txt`、`CMakePresets.json`、`engine/`（上列兩型別除外）、`framework/`、`plugins/`、其餘 `tools/`、`tests/common/`（上列兩處除外）、`tests/framework/`。盤點結果：這些地方**沒有一處寫死 `object_fps`**——產品由 `projects.csv`／`tools.csv` 列舉，`tests/<app>`、`build/acceptance/<app>`、`assets/<app>`、`tools/<tool>` 由 `GyoBuild.cmake`／`GyoAcceptance.cmake`／`GyoAppProject.cmake`／`GyoTools.cmake` 依名字慣例發現。所以本批**預期共通層零修改**；若實作時發現不是，照判定 2 處置。
- `tests/common/entity_store/entity_store_guard.py` 的禁字表 W1 含 `object_fps`、`RetroFPS`：那是「Engine 程式不得出現產品名」的守衛，`object_fps_pvp` 仍在，禁字不改。
- git 歷史：約 100 MB 資產留在歷史裡，不重寫歷史。
- `docs/dev_logs/`、`docs/checkup/`、`docs/architecture/plans/<舊計畫>/`、`docs/object_fps_pvp/plans/`：歷史紀錄，一律不改內容（下方「文件的處置」只在三份 `inactive_products.md` 加一行註記）。

### 文件的處置

- **現行文件改措辭**（把「停用、待遷移」改成「已移除（本批 PR）」，或把 object_fps 的例子換成 pvp 的）：`README.md`／`README.en.md`／`README.zh-Hant.md` 第 76 行；`docs/architecture.md` 第 138 行（`IRuntimePort` 段）；`docs/architecture/animation-events.md` 第 70 行（`AnimationInstance` 改為 `PlaybackInterval::Crossed` 的呼叫者）；`docs/creating_apps.md` 第 142 行；`docs/tool_projects.md` 第 12、25～26、64、74 行（`requires_apps` 的例子換成假想名或 pvp）；`docs/releasing.zh-Hant.md`／`.ja.md` 第 45 行附近；`docs/rendering_architecture.zh-Hant.md`／`.ja.md` 第 204 行附近；`docs/ui_toolchain.md` 第 18 行的例子字串；`docs/object_fps_pvp/assets.zh-Hant.md` 第 9、12 行；`docs/object_fps_pvp/network-architecture.zh-Hant.md` 第 238～239、448 行；`docs/object_fps_pvp/entity-model.zh-Hant.md` 第 59 行（指向已刪檔，改成「已移除；見 git 歷史 `56b8dda3`」）。
- **計畫文件標記完成**：`docs/architecture/target/README.md` 第 64 行（第 8 項）；`docs/architecture/plans/kernel/README.md`、`HANDOFF.md`、`spec.zh-Hant.md` K.3 第 8 項與 L.1 表、`audit.zh-Hant.md`／三份 audit-part 的對應列（加「已移除」）；`docs/tools/plans/v8/README.md` 第 19、28、30 行、`model-viewer.md` 第 25 行（刪 `AnimationInstance`）。
- **三份 `inactive_products.md`**（foundation-followups、math-foundation、result-unification）：檔頭加一行「2026-11-12：三個產品已移除（PR #N），本清單只作歷史紀錄」，內容不改。
- **`docs/architecture/3d-assets.md`**：第 146～212 行的素材包授權記錄搬進 `docs/object_fps_pvp/assets.zh-Hant.md` 當附錄（翻成正體中文、保留原文的授權事實與日期），刪檔；`docs/object_fps_pvp/assets.zh-Hant.md` 第 12 行改指附錄（使用者決定 B）。

## 做法

一個 PR、三個 commit，讓判定 2 可以逐 commit 讀 diff：

1. `chore(apps)`：刪三個 owner 的 Content、Test、Validation、資產、owner 文件，改兩份登記表。這個 commit 之外**不應**有其他檔。
2. `refactor(engine)`：刪 `IRuntimePort`、`AnimationInstance` 與其測試（4 個共通檔）。
3. `docs`：上節的文件處置＋dev_log＋HANDOFF／README。

實作者在獨立 worktree 只 commit 不 push；主會話 cherry-pick、獨立重跑判定 3～5、再開 PR。

## 判定（看結果前宣告）

1. **清單逐項**：PR 的 `git diff --diff-filter=D --name-only origin/master` 集合**恰等於**上表的刪除清單（含 `RuntimePortTests.cpp`、`IRuntimePort.hpp`）；`--diff-filter=M` 的集合 ⊆ 上表的修改清單＋文件處置清單。多刪、少刪、多改任一＝不成立，報告，不補。
2. **共通層零修改（§7 的實測）**：commit 1 的 diff 只含刪除清單與兩份登記表；整個 PR 在 `build/`（`build/acceptance/object_fps*` 以外）、`.github/`、`CMakeLists.txt`、`CMakePresets.json`、`engine/`（`IRuntimePort.hpp`、`Animation.hpp`、`Animation.cpp` 以外）、`framework/`、`plugins/`、`tools/`（`object_fps_preview` 以外）、`tests/common/`（`core/CMakeLists.txt`、`core/runtime/RuntimePortTests.cpp`、`model/ModelTests.cpp` 以外）、`tests/framework/`、`apps/object_fps_pvp/` **零變更**。出現任何一處＝§7 不成立：停下、把那一處當 Architecture Delta 寫進報告（§3 七項），**不在本批修**，由使用者決定。
3. **零殘留**：合併候選上 `git grep -P 'object_fps(?!_pvp)|ObjectFps(?!Pvp)|IRuntimePort|RuntimePort|AnimationInstance'` 在 `apps/ tools/ engine/ framework/ plugins/ build/ tests/ assets/ .github/ CMakeLists.txt CMakePresets.json` 為 **0 行**（`entity_store_guard.py` 與其測試的禁字表除外，事先列明）；`docs/` 的命中只在 `dev_logs/`、`checkup/`、`plans/`（kernel 計畫文件與三份 inactive_products 已標「已移除」）與明寫「已移除」的句子裡。無空目錄（`git ls-files` 不追蹤空目錄，另以 `find apps tools tests assets build/acceptance docs -type d -empty` 在 clean checkout 為 0）。`cmake --preset dev -DGYO_APPS=object_fps` 回 `GyoAppRegistry.cmake` 的「not registered」錯誤而不是靜默跳過（無 Compatibility Branch、無 Dummy Target）。
4. **建置與測試**：本機 Intel Mac preset `test`（`GYO_APPS=AUTO`、`GYO_TOOLS=AUTO`）configure／build 零錯誤、ctest 全過；ctest 的測試**數**前後相同（兩個產品 `enabled=0`，`tests/object_fps*` 本來就不在 AUTO 建置裡），doctest 案例數：`engine_tests` 減 1、model 測試減 3、其餘每個測試執行檔的案例數前後相同。PR 的 L1 三平台（linux-x64、windows-x64、macos-arm64）與 Package trial 全綠；數字從 L1 artifact 的 `tests.xml` 讀，不從日誌摘要。
5. **pvp 不受影響**：`object_fps_pvp` 的建置目標集合（`gyo-build.json` 或 `cmake --build --target help` 的 pvp 目標）、ctest 數、每個 pvp 測試執行檔的 doctest 案例數，前後逐一相同。

件数の突き合わせ：報告以「刪除檔數（宣告 308／diff D）、修改檔數（宣告 ≤ 26／diff M）、ctest 數（前／後）、doctest 案例數（前／後／差＝4）、三平台 tests.xml 的 tests／failures」開頭。不一致＝不確定受理，等使用者。

## 範囲の限界（本批不解、報告要列）

- 不驗證「Product 的**追加**／複製只改登記與內容」（§12 Product Add／Clone）：那要一個新產品當樣本，留給改造後的第一個新 host。
- 不重寫 git 歷史：資產留在歷史裡，clone 大小不變。
- `RetroFPS` namespace 留在 pvp：命名歸屬由 target/naming 第 1 條在遷移批處理，不在本批改名。
- 刪除的是**停用**產品，CI 從未建置它們；判定 4 驗的是「刪了以後其他東西還成立」，不是「刪前它們能建」。
- `docs/architecture/plans/<舊計畫>/inactive_products.md` 記的遷移需求隨產品一起作廢，只標註不刪：它們也是當時 Engine 改了什麼公開介面的紀錄。
- 本批不評估 object_fps／v2 有沒有值得搬進 pvp 的設計（hit rules 的部位倍率已在 `v8-product-hit-rules-v2-values` 搬過；`MapGeometryGenerator`、`GridMap` 的參考價值由 voxel 線在需要時從 git 歷史讀）。

## 模型／檔位

上段：改 Engine 公開介面（刪 `IRuntimePort`、`AnimationInstance`）、改登記資料與 Build Graph 的輸入，並且判定 2 是 Architecture 的判斷。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 主會話（宣告、cherry-pick、獨立重跑判定 3～5、報告） | Fable／使用者設定 | 控制權在主會話（`v8-arch-kernel-control`） |
| 實作者（子代理，獨立 worktree，只 commit 不 push） | **Opus／high** | 跨 300 多檔、三個 commit 的機械性刪除＋約 20 份文件措辭；需要讀懂每處引用的上下文才能改對措辭，但沒有時序／協定邏輯，不到 xhigh |
| 審查者（只讀） | **Fable／high** | 與實作者不同模型；獨立重做判定 1～3 的集合比對與 grep，並讀三個 commit 的 diff 找「順手改了共通層」 |
| 本機 configure／build／ctest、grep、計數 | Haiku 或主會話／low | 定型作業 |

估時：實作 3 小時（上限 4.5 小時）、審查 1 小時、主會話驗證與報告 1 小時。單一問題累計超過 2 小時（例如某份文件的措辭爭議、某平台的 L1 紅）就停下報告。證據：主 checkout `build/evidence/remove-object-fps/`（前後 ctest／doctest 計數原始輸出、grep 輸出、L1 artifact 三份 `tests.xml`）。

## 檢查：這是不是在為舊形狀找位置？

不是。本批不新增任何結構，只刪除與驗證既有的 §7 性質；刪除的兩個 Engine 型別都是只剩一個（已停用的）使用者的介面，不替代、不改名、不留 shim。若判定 2 不成立，本批也不修——那是改造批的輸入。

## 待使用者確認

- **A. owner 文件**：`docs/object_fps/`（2 檔：acceptance）與 `docs/object_fps_v2/`（3 檔：enemies、human-enemies-validation、region-damage-validation）照 §7 當 Content 刪除（**建議刪**；需要時從 git 歷史 `56b8dda3` 讀，現行文件引用處改寫成這句）。另一選項：留著當歷史文件，但那會違反「owner 刪除後不留 Content」。
- **B. `docs/architecture/3d-assets.md`**：內容是 object_fps（v1）的素材組裝說明，但第 146～212 行記了素材包的授權（UAL mannequin 等），而 pvp 的 `assets/object_fps_pvp/animations/ual_mannequin/` 仍用同一個包、`docs/object_fps_pvp/assets.zh-Hant.md` 第 12 行引用它記的授權。**建議**：把授權段搬進 `docs/object_fps_pvp/assets.zh-Hant.md` 當附錄（歸屬改成實際使用者 pvp），刪 `3d-assets.md`。另一選項：留檔、只在檔頭標「歷史：產品已移除；授權段仍有效」。
- **C. 判定 2 不成立時的處置**：宣告寫「停下、記 Delta、不修」。若你希望小的共通層修正（例如一行寫死的路徑）就地修掉而不中斷，請在核准時說明；否則照宣告。

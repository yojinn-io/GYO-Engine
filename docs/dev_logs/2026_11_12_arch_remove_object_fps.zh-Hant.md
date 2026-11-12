# 架構 v8：v8-arch-remove-object-fps（移除 object_fps、object_fps_v2、object_fps_preview，兼驗 §7 removability）

日期：2026-11-12。Owner：架構（kernel 線）。計畫：[kernel README](../architecture/plans/kernel/README.md) 批次表的 `-remove-object-fps` 列、事前宣告 `docs/architecture/plans/kernel/remove-object-fps-declaration.zh-Hant.md`（使用者 2026-11-12「A 刪、B 搬 pvp、C 照宣告，開始」）。決定來源：spec K.3 第 8 項、audit 待決第 3 項（`v8-arch-remove-runtimeport-animinstance`）。
- 執行：實作者＝架構師會話開的子代理（Opus／high，獨立工作樹 `.claude/worktrees/agent-a124d6091b90f19c2`，從 master `56b8dda3`，只 commit 不 push）；主會話 cherry-pick、獨立重跑判定 3～5、開 PR。
- 性質：刪除與驗證。不新增結構。

## 起因

兩個停用產品（`object_fps`、`object_fps_v2`）與依賴它們的工具（`object_fps_preview`）在改造批之前移除，改造不必為它們規劃遷移或相容層。只有它們在用的 Engine 型別 `Engine::Runtime::IRuntimePort`、`Engine::Model::AnimationInstance` 一起刪。同時實測 AGENTS §7「刪除一個 Product，共通層不需要改」。

## 做了什麼

三個 commit，讓判定 2 可以逐 commit 讀 diff：

1. `chore(apps)`：`git rm -r` 三個 owner 的 Content（`apps/`、`tools/`、`assets/`、owner 文件 `docs/object_fps*`）、Test（`tests/object_fps*`）、Validation（`build/acceptance/object_fps*`），共 305 檔；`engine/config/projects.csv` 刪兩列、`tools.csv` 刪一列。
2. `refactor(engine)`：刪 `IRuntimePort.hpp`、`RuntimePortTests.cpp`（1 個 TEST_CASE）與 `tests/common/core/CMakeLists.txt` 的那一行；從 `Animation.hpp`／`Animation.cpp` 刪 `AnimationInstance` 類別與成員定義；`ModelTests.cpp` 刪用到它的 3 個 TEST_CASE。`PlaybackInterval`、`SamplePose`、`MakeDefaultPose`、`BlendPoses`、`TransferCompatibleAnimation`、`SkinMesh`、ufbx loader 不動。`Animation.hpp` 的 `<memory>` 保留：包含它的 pvp 檔中有 3 個用 `shared_ptr` 卻沒有自己 include `<memory>`，可能靠這個標頭間接取得；沒有驗證拿掉後是否仍能建置，為了不冒「pvp 需要修改」（判定 2）的風險而保留。`Animation.cpp` 的 `Scalar.hpp` 也保留（`Math::Clamp` 仍在用）。
3. `docs`：照宣告「文件的處置」改現行文件的措辭（停用→已移除，或把例子換成 pvp／假想名）；kernel 計畫文件（spec K.3 第 8 項、L.1／L.4 表、audit 與三份 audit-part 的對應列）與 `docs/architecture/target/README.md` 第 8 項、`docs/tools/plans/v8/README.md` 三列標「已移除」；三份 `inactive_products.md` 檔頭加一行；`docs/architecture/3d-assets.md` 第 146～212 行（素材包清單與授權）翻成正體中文，搬進 `docs/object_fps_pvp/assets.zh-Hant.md` 附錄 A 後刪檔（使用者決定 B）；本 dev_log。

## 受理判定（實作者自驗；主會話另行重跑判定 3～5）

件数の突き合わせ：刪除 308（宣告 308／diff D 308，集合一致）、修改 32（宣告「約 26」／diff M 32；全部在宣告列名的檔案內，見下）、本機 ctest 138 過／0 敗、doctest `engine_tests` 139、`gyo_model_tests` 21；三平台 tests.xml 待 L1。

- 判定 1 清單：刪除集合與宣告逐檔相同（308）。修改 32 檔 ⊆ 宣告的修改清單＋文件處置清單；數目超過宣告的估計「約 26」，因為宣告列名的文件實際是 26 份（估成約 20），另 kernel 的 README、HANDOFF 由主會話改，合計會是 34。
- 判定 2 共通層零修改：commit 1 只含刪除清單與兩份登記表；整個變更在宣告列的共通層路徑中，扣掉宣告允許的檔後為空。**§7 在這次刪除上成立。**
- 判定 3 零殘留：**不成立**。程式、建置、測試、資產、workflow 範圍除了 `entity_store_guard.py` 與其測試的禁字表（3 行），還有 `tests/object_fps_pvp/` 的 6 行：`HitboxSurvey.cpp` 第 2、46 行（註解，說明 11 個區域取自 v2 的 `hurt_regions`；值已複製成常數，執行期不讀 v2 的檔）、`test_asset_catalog.py` 第 450、451、453 行（守衛自測用的合成路徑字串，不需要實檔）、`fixtures/hair_buns/README.zh-Hant.md` 第 10 行（說明文字）。都不是依賴，pvp 的建置與測試照常通過；但宣告的前提「pvp 零引用」不對，而 `tests/object_fps_pvp/` 在不碰清單裡，本批沒有改，交使用者決定。`docs/` 的命中只在 dev_logs、checkup、計畫文件與明寫「已移除」的句子。無空目錄。`cmake --preset dev -DGYO_APPS=object_fps`（以及 `object_fps_v2`、`-DGYO_TOOLS=object_fps_preview`）以「is not registered」失敗（`GyoAppRegistry.cmake:239`、`GyoToolRegistry.cmake:79`）。
- 判定 4 建置與測試：本機 Intel Mac preset `test`（`GYO_APPS=AUTO`、`GYO_TOOLS=AUTO`）configure 成功、全建 0 error（1512 步）、`ctest --preset test` 138／138 過；`engine_tests` doctest 139（原 140，減 1）、`gyo_model_tests` 21（原 24，減 3）；`gyo_model_viewer` 建成，`gyo_model_viewer.*` 測試通過。三平台待 L1。
- 判定 5 pvp 不受影響：主會話重跑。

## 經緯

- 宣告寫「`ModelTests.cpp` 其餘 14 個 TEST_CASE 不動」；實際原檔有 15 個 TEST_CASE，刪 3 個後剩 12 個。差 3 的判定不受影響（`gyo_model_tests` 另含 `AnimationEventsTests.cpp` 的 9 個）。
- 刪掉的第三個 TEST_CASE（`playback intervals report an event once ...`）主要測 `PlaybackInterval::Crossed`，只有最後 3 行用 `AnimationInstance`；照宣告整個刪。`Crossed` 之後只剩 `tests/model_viewer/ModelViewerEventEditingTests.cpp` 經 `CrossedEvents` 間接涵蓋，`tests/common` 沒有直接測它。
- 宣告沒有列、但刪 `3d-assets.md` 後會變成死連結而一併改的：三份根 README 第 73 行的連結（刪掉該條目）、`docs/architecture.md` 第 301 行（改指 animation-events）；`docs/releasing.*` 第 436 行與 `docs/rendering_architecture.*` 第 204 行指向已刪的 `docs/object_fps/acceptance.*`（刪掉連結）。`docs/architecture.md` 第 170、270 行講 v1／v2 與 preview 的現況句也改了。
- 留下未改（不在本批可改的範圍）：`tests/object_fps_pvp/asset_sources.json` 第 62 行與 `tests/object_fps_pvp/fixtures/hair_buns/README.zh-Hant.md` 第 9 行的 `record` 仍寫 `docs/architecture/3d-assets.md`（自由文字，守衛不檢查路徑，測試不會失敗）；應改指 `docs/object_fps_pvp/assets.zh-Hant.md` 附錄 A，由 pvp 決定。
- 證據：主 checkout `build/evidence/remove-object-fps/`（刪改清單、grep 輸出、configure／build／ctest 輸出、probe 的錯誤訊息；實作者工作樹的原始輸出在 `build/evidence-impl/`，由主會話搬入）。三平台 `tests.xml` 待 L1。本機 Intel Mac。

## 架構報告

- Architecture Delta：無。判定 2 成立，刪除不需要改共通層。刪 `IRuntimePort`、`AnimationInstance` 是 Engine 公開介面的刪除（使用者 2026-11-11 已決定）。
- Data Contract：`projects.csv`／`tools.csv` 只刪列，欄位不變。
- 範圍の限界（宣告原文）：不驗 Product 的追加／複製；不重寫 git 歷史；`RetroFPS` namespace 留在 pvp；刪的是 CI 從未建置的停用產品。
- 觀察到但不在本批處理：現行文件仍有不帶 `_pvp` 的「Object_FPS」字樣（`docs/rendering_architecture.*` 第 55、66、129、248 行，`docs/creating_apps.md` 第 87 行，`docs/architecture.md` 第 136、293 行，`docs/ui_toolchain.md` 第 203 行等），判定 3 的正規式（區分大小寫）抓不到；有的指產品族、有的指 v1，交圖書管理員點檢。

## 追記（主會話，實作者停止之後）

- 使用者 2026-11-12 選 (a)：`tests/object_fps_pvp/` 的 8 行文字由主會話以第四個 commit `4bb944cb` 改寫（`HitboxSurvey.cpp` 兩行註解、`test_asset_catalog.py` 三行自測假路徑改 `other_game`、`fixtures/hair_buns/README.zh-Hant.md` 兩行、`asset_sources.json` 一行出處改指 `docs/object_fps_pvp/assets.zh-Hant.md` 附錄 A）。判定 3 在此之後**成立**：非 docs 的 `git grep -P` 只剩 `entity_store_guard` 的禁字表 3 行。上文「三個 commit」「判定 3 不成立」「本批沒改 pvp 測試目錄」是停止當時的紀錄，保留不改。
- 最終數字：刪 308、修 38（實作者 32＋commit 4 的 4＋主會話後續 2）、新增 2（宣告、本 dev_log）；審查後第五個 commit 把 `PlaybackInterval::Crossed` 的 5 個 CHECK 以同名 TEST_CASE 留在 `ModelTests.cpp`（審查 S1：那是保留型別的直接測試，刪掉是 §14.4 的「緩める面」，宣告沒寫明；留下沒有緩める面），所以 `gyo_model_tests` 24→22、合計少 3。
- 主會話的判定 4、5（基準環境：主 checkout preset `test`，含 network 測試）：ctest 152→152 名單相同、全過；doctest 只差 `engine_tests` 140→139、`gyo_model_tests` 24→21（第五個 commit 後 22）；pvp 57 個測試項目的案例數逐一相同；空目錄 0；`test_asset_catalog` unittest 28/28。證據：主 checkout `build/evidence/remove-object-fps/`（`before-*`、`after-*` 為主會話；`impl/` 為實作者原本寫在 worktree `build/evidence-impl/` 的檔，已搬入；`review-report.md` 為審查者）。
- 審查（Fable／high，只讀）：0 blocker／5 should／6 nit。S1 如上；S2～S4 文件過時已補；S5 `rendering_architecture.*` 第 204 行泛化後描述了不存在的 `gyo_<game>_acceptance`，改成原則句；N2、N3、N4 已改；N1（`<memory>`）保留；N6 大小寫不同的「Object_FPS」字樣交圖書管理員。
- 宣告的前提錯誤（pvp 零引用）出在架構師的掃描：`xargs grep -P` 在 macOS 的 BSD grep 是無效選項，錯誤被 `2>/dev/null` 吞掉。已記入 HANDOFF 與記憶。
- 三平台：PR #185 的 L1，數字待回填。


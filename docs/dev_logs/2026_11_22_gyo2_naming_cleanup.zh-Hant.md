# 2026-11-22 命名收尾（`v8-arch-gyo2-naming-cleanup`）

Owner：架構（GYO2 主會話開的任務卡）。分支 `claude/gyo2-naming-cleanup`（PR 開向 `gyo2`）。實作者 Opus 5.5／high；設計審查 Fable 5.2／high（唯讀）。

## 為什麼

namespace 改名（#266，45f13757）與第 4 批後續（#265，96a38061）的「後續」與設計審查留下的命名與測試共用設施。使用者在兩張卡上都說「後續轉給主會話，開卡處理」；照「改名類工作排前面，以免後面越改越多」，這張先做。出處：`docs/dev_logs/2026_11_22_gyo2_namespace_root.zh-Hant.md`、`docs/dev_logs/2026_11_22_gyo2_batch4_followups.zh-Hant.md` 的「後續」與「設計審查」，主 checkout 的 `build/evidence/gyo2-namespace-root/reviews/README.md`、`build/evidence/gyo2-batch4-followups/design-review/REPORT.md`。

## 做了什麼（七件）

1. **naming_guard 換到中立的 owner**
   - `tests/common/kernel/{naming_guard,test_naming_guard}.py` → `tests/common/naming/`（git mv）；ctest `kernel.naming_guard`／`kernel.naming_guard_selftest` → `naming.guard`／`naming.guard_selftest`（`tests/common/naming/CMakeLists.txt`，`tests/common/CMakeLists.txt` 加一行）。
   - 讀 C/C++ 文字的共用函式從 `kernel_guard.py` 抽到 `tests/common/support/source_text.py`：`SUFFIXES`、`DIRECTIVE`、`TARGET`、`STANDARD_HEADERS`、`strip_comments_and_strings`、`line_of`、`camel_case`。kernel_guard、orchestration_guard、naming_guard 都從這裡匯入；orchestration_guard 不再依賴 kernel 的目錄。
   - `kernel_guard.camel_case` 與 `naming_guard.unit_name` 收斂成 `source_text.camel_case(identifier, acronyms=None)`：K1 不帶縮寫表（產品的名字由產品定），naming_guard 帶 N17 的 `ACRONYMS`。`unit_name` 拿掉。
   - 突變收回一份：Kernel 清單的 `ks-n1-*` 8 個與 `ks-real-old-namespace`、orchestration 的 `or-guard-naming-root-dropped`、`or-guard-real-n1-other-namespace`、world_time 的 `wt-guard-real-n1-other-namespace` 搬進 `naming_mutations.json`（16 → 28）。id 不改（dev_log、宣告裡的引用還找得到）；路徑跟著改；沒有 `guards` 的補上（README 第 9 條）。`nm-unit-name-underscore` 改指 `source_text.py`，`nm-override-dropped`、`nm-scan-roots-examples` 的 find 跟著改。Kernel 清單的 `ks-adv-standard-headers-widened`、`ks-k5-directive-include-only` 改指 `source_text.py`（那兩行搬過去了）。
   - 搬檔對照守衛 glob：掃描範圍沒有縮小——naming_guard 讀的是 `--root` 底下的 `SCAN_ROOTS`，和它自己放在哪裡無關；預設 `--root` 是 `parents[3]`，新位置深度相同。自測 53 案的名字前後一樣。
2. **`gyo_expect_links` 換到中立的位置**：定義從 `tests/common/time/CMakeLists.txt` 搬到 `tests/common/support/ExpectLinks.cmake`（本體逐字相同，加 `include_guard(GLOBAL)`）。用它的 14 個目錄（audio、core、entity_store、json、kernel、logging、math、orchestration、os、rollback、time、trace、world_logging、world_time）各自 `include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")`，不再靠 `tests/common/CMakeLists.txt` 的先後（那行註解拿掉）。放 `tests/common/support` 而不放 `build/cmake`：它是測試的檢查，不是建置的一部分；`gyo_test_support`、`SilentProbe.cmake` 已在這裡。
3. **`engine/` 的 `::GYO::Engine::<單位>::` 改短名**：234 處中 232 處。在 `GYO::Engine` 裡寫 `<單位>::`（例 `Base::`、`Time::`、`IO::Path::`），在 `GYO::Kernel` 裡寫 `Engine::<單位>::`（`GYO::Kernel` 看不到兄弟單位，只看得到 `GYO::Engine`）。`Assert.hpp` 的兩個巨集（`GYO_ASSERT`、`GYO_UNREACHABLE`）保留全限定：巨集在任何 namespace 展開。腳本依所在 namespace 決定寫法，存在 `build/evidence/gyo2-naming-cleanup/short_names.py`。突變的 find／replace 含 `::GYO::Engine::` 的 28 個照同一張對照表改寫（find 在檔案裡的次數前後都是 1）。
4. **測試輔助的 namespace 收斂成一套**：`KernelTest`、`WorldTest` → `GYO::Test::Kernel`；`OrchTest`（含 `OrchTest::Save`）→ `GYO::Test::Orchestration`（`::Save`）；`RollbackTest` → `GYO::Test::Rollback`。判斷：N17「`GYO::Test` 的根」那一列本來就寫「各單位的測試輔助放 `GYO::Test::<單位>`」，頂層的四個是漏網的舊寫法，不是另一套規則；寫明「兩套都可以」等於留兩個出口。Kernel 的兩份標頭同在 `GYO::Test::Kernel`：`WorldTest` 本來就 `using namespace KernelTest`，兩份宣告的名字不重疊；若拆成 `GYO::Test::Kernel::World`，namespace 名 `World` 會遮住 `GYO::Kernel::World`。巨集（`KERNEL_REQUIRE_OK`、`KERNEL_REQUIRE_RUN_OK`、`ORCH_REQUIRE_BUILT`）寫 `::GYO::Test::…` 全限定。命名規範「N17 的根」補一條說明（`rebuild-naming.zh-Hant.md`）。守衛不查頂層的根（和產品的根分不出來），由審查看。
5. **`detail` 對齊 N13 的 `Detail`**：`engine/core/io` 的 `MountTable.hpp`、`Vfs.hpp`，`engine/ecs/entity_store` 的 `EntityColumns.hpp`（`namespace detail`／`detail::` → `Detail`）；`engine/core/math` 的 `Frustum.hpp`（`FrustumDetail` → `Detail`，math 裡沒有別的 `Detail`）。gyo2 的突變清單沒有指到這些字（master 時代的 `voxel_map_editor_mutations.json` 有一個 `FrustumDetail`，屬範圍外的 15 份）。
6. **`SCAN_ROOTS` 只拿掉 `framework`，留 `apps`、`tools`**（和 brief 不同，理由）：framework 層 2026-11-11 退役，`build/` 已不讀它；`apps/`、`tools/` 雖然現在不存在，`build/cmake/GyoBuild.cmake` 仍照登記檔把 `apps/<app>`、`tools/<tool>` 加進建置（第 68、76 行），是產品的家。拿掉它們，之後加進來的產品打開 `GYO::Engine::…`、`GYO::Test` 時就不會報。不存在的掃描根照舊跳過。若主會話或使用者要照 brief 一起拿掉，改這一行與 pin 的自測即可。
7. **authority、rollback 在範圍內**：#266 之後單位從目錄推出，`engine/modules/authority`→`GYO::Engine::Authority`、`engine/modules/rollback`→`GYO::Engine::Rollback` 已自動涵蓋（第 4 批宣告 9.5 延下來的那條，不用補）。證據：在 `engine/` 的複本上把兩個單位各一個原始檔改成打開 `GYO::Engine::Orchestration`，守衛各報一條（`item7-probe.log`）。

文件跟著改的：`rebuild-naming.zh-Hant.md`（守衛位置、測試輔助一條）、`target/naming.zh-Hant.md`（守衛路徑）、`world-time.zh-Hant.md`（守衛欄的 ctest 名；WT-4 守衛欄原本寫已退役的 `wt-guard-naming-root-dropped`，改成現行的 `wt-guard-real-n1-other-namespace`；條文本身不變）、`entity-store.md`（`gyo_expect_links` 的位置）、`target/v2/README.md`（本 dev_log）。歷史紀錄（dev_log、HANDOFF、已核准的宣告）不改寫。

## AGENTS §14.4

驗證者（Opus 5.5／high，和實作者不是同一個 agent；照 navigator「計畫沒指派的 §14.4 驗證者跟實作者的指派」）用實際資料確認下面 1～5：判定的變更本身沒問題；它指出 1、2 原本列得不完整（漏了舊根、讀不懂的宣告、`.m`／`.mm`、經目錄連結才讀到的檔）、5 漏了行號，已照改。探針：在新樹的複本加 `framework/` 的檔與連結，舊守衛報 12 條、新守衛報 1 條，少掉的 11 條都在 `framework/` 底下；新守衛的輸出是舊守衛的子集。反向：把 `framework` 加回清單，`test_n1_scan_roots_are_pinned` 失敗。

### 只搬位置、判定不變（第 1、2 件）

- naming_guard：程式碼和搬之前比，只差匯入、`SUFFIXES` 的來源名、`unit_name`→`camel_case(…, ACRONYMS)`、`SCAN_ROOTS`（第 6 件，見下）。`camel_case` 加了 `if part`，對空段兩者都產生空字串，輸出相同。自測 53 案名字相同、全過。
- 共用讀法：`source_text.py` 的六個定義和 `kernel_guard.py` 原本的逐字相同；kernel_guard 自測 91 案、orchestration_guard 自測 13 案全過。
- `gyo_expect_links`：本體逐字相同；三個 DEFER 的突變（`os-g19-links-from-later-directory`、`ti-x5-links-from-later-directory`、`rb-r3-authority-from-later-directory`）搬完仍 killed。
- 證據：`judgement-unchanged.log`（和 `origin/gyo2` 逐段 diff）、`mutations/run-naming.log`、`run-kernel-py.log`、`run-orch-py.log`、`run-links-*.log`。

### 改了判定：`SCAN_ROOTS` 拿掉 `framework`（第 6 件；naming DI 1 的守衛，不論鬆緊都要使用者核准）

1. **守的性質**：N17 的根只在自己的位置打開——掃描範圍內、不屬於任何單位的 C/C++／Objective-C++ 檔（`.m`、`.mm` 也讀），不打開單位的根、舊根 `Engine`（過渡規則）、`GYO` 單獨、表外的 `GYO::<X>`，`GYO::Plugins`／`GYO::Test` 只在自己的目錄；讀不懂的 namespace 宣告也報（fail closed）。
2. **緩める面**：`framework/` 底下的任何發現，以前報，之後不報——上面每一種（單位的根、舊根、`GYO` 單獨、表外的根、`GYO::Plugins`／`GYO::Test` 的位置、讀不懂的宣告），`.m`／`.mm` 也一樣；別的掃描根裡經目錄連結才讀到 `framework/` 的檔也不再讀。放在掃描根裡、指向 `framework/` 的檔案連結仍會報。gyo2 現在沒有 `framework/`、也沒有符號連結，實樹上沒有差別（新守衛跑新樹、舊守衛跑舊樹、舊守衛跑新樹都 clean）。
3. **締める面**：無。
4. **緩的面是否由別處守著**：`framework/` 不會進建置——`build/cmake/GyoBuild.cmake` 只加 `third_party`、`engine`、`plugins/<plugin>`、`apps/<app>`、`tools/<tool>`、`tests/common`、`examples`、`tests/<app>`；不在建置裡的檔案不會成為 Engine 或產品的一部分。「`SCAN_ROOTS` 以外的頂層目錄不讀」本來就寫在守衛的 Limits。要再有 framework 層，得先改架構與建置，那時再加回掃描根。
5. **根據與證據**：`GyoBuild.cmake` 第 34、57、63、68～70、76、82、86、93、97、105、109 行（`build/acceptance` 讀進建置但現在沒有 C/C++ 檔，它不在掃描根是本批以前就有的缺口，列後續）；自測 `test_n1_scan_roots_are_pinned` 改成新的清單，`test_n1_every_scan_root_is_scanned` 對留下的每個根仍報；突變 `nm-scan-roots-examples`（find 改成新清單）killed。

## 突變（README：新加或搬的、find 在改動範圍的、殺它的檢查被改的）

| 範圍 | 結果 | 紀錄 |
|---|---|---|
| `naming_mutations.json` 全部（原 16＋搬來 12） | 28/28 killed | `mutations/run-naming.log` |
| Kernel 清單裡 file 是 `kernel_guard.py`／`source_text.py` 的（import 改了、兩個改指新檔） | 49/49 killed | `mutations/run-kernel-py.log` |
| orchestration_guard 的（import 改了） | 11/11 killed | `mutations/run-orch-py.log` |
| 靠 `gyo_expect_links` 抓的連結突變（6 份清單，含 3 個 DEFER） | 15/15 killed，`os-g18-windows-extra-link` 只在 Windows、本機 skip | `mutations/run-links-*.log` |
| 第 3 件改寫 find 的 28 個（Kernel 3、orchestration 14、rollback 3、logging 7、trace 1） | 第一輪 27/28：`lg-file-writer-timer` wrong_failure——它的 expect 比對 `GYO_ASSERT` 印出的原始碼字串，那行改成 `Time::WakeReason::Notified` 後訊息跟著變；expect 改成新字串後 killed（README「改名造成的訊息變化」，不問）。gyo2 清單裡 expect 帶 `GYO::Engine` 的只有它受影響（其餘 6 個是符號名或守衛訊息）→ 28/28 | `mutations/run-item3-*.log`、`run-item3-lg-file-writer-timer-expect.log` |

- 加的突變：0。退的：0。搬的 12 個 id 不變（README 第 8 條：本批沒有新加，不用比「條數 × 5」）。
- 靜態的 find 次數檢查（不建置）：gyo2 在用的清單 1784 個，本批沒有造成任何 find 失效；36 個在基準上就找不到（`plans/audio` 12、`plans/time-threads-trace` 6，指 master 的路徑；`tests/common/authority_scan/mutations.json` 18，路徑寫法另一套），和本批無關，列後續。
- 第 4 件（測試輔助 namespace）只改了測試檔的 namespace，判定不變；殺 Kernel、編排層、回滾突變的測試檔因此「被改過」。照 D184，這些測試所在檔案上的既有突變（改動檔案上共 752 個）在 PR 線合併前背景完整跑一次，結果記在下面。

## 驗證

本機 macOS Intel，AppleClang，preset `test`；`run_mutations.py` 加 `-B`、先清 `__pycache__`。

- configure、build 通過（`configure-2.log`、`build-1.log`）。
- 全量 ctest 161/161（`ctest-full-1.log`；`naming.guard`、`naming.guard_selftest` 在新位置跑）。
- 第 3 件的掃描（`item3-scan.log`）：正例——掃描式在 `origin/gyo2` 的 `engine/` 命中 234 處；正向——之後只剩 `Assert.hpp` 的兩個巨集；反向——40 個改動檔案的每一行，把短名換回 `::GYO::Engine::<單位>::`（第 5 件的檔案再換回 `detail`）就等於原本那行，沒有別的改動。
- L1：PR 上跑。

## 設計審查（每批固定的一步）

（審查完填）

## 證據

主 checkout 的 `build/evidence/gyo2-naming-cleanup/`（使用者的 Intel Mac，git 不追蹤）：`configure-1.log`、`configure-2.log`、`build-1.log`、`ctest-full-1.log`、`short_names.py`、`item3-scan.log`、`item7-probe.log`、`judgement-unchanged.log`、`mutations/`、`design-review/REPORT.md`。

## 後續（範圍外，沒有做）

- naming_guard 的 `SCAN_ROOTS` 不含 `build/acceptance`（建置會讀，現在沒有 C/C++ 檔）。本批以前就有的缺口，驗證者發現。
- `tests/common/entity_store/entity_store_guard.py`、`tests/common/os/os_boundary_guard.py` 各自還有一份 `strip_comments_and_strings`（和 `STANDARD_HEADERS`），可以改用 `source_text.py`。本批只處理 brief 指名的兩支守衛。
- 突變清單 `plans/audio`、`plans/time-threads-trace` 有 18 個指向 master 路徑的突變，不在 brief 列的「15 份 master 時代清單」裡；`authority_scan/mutations.json` 的路徑寫法要確認執行器怎麼解讀。
- brief 原列的範圍外照舊：15 份 master 時代的突變清單、文件路徑與 master 時代文件的去留、entity_store／trace／logging 三個 include 守衛的 `..`、Kernel「拿掉的名字不回來」的守衛、權威 Module 的 `gyo_expect_links`、K6 標頭分支的去留。

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
4. **測試輔助的 namespace 收斂成一套**：`KernelTest`、`WorldTest` → `GYO::Test::Kernel`；`AuthorityTest` → `GYO::Test::Authority`（brief 沒點名，設計審查找到，同批補）；`OrchTest`（含 `OrchTest::Save`）→ `GYO::Test::Orchestration`（`::Save`）；`RollbackTest` → `GYO::Test::Rollback`。判斷：N17「`GYO::Test` 的根」那一列本來就寫「各單位的測試輔助放 `GYO::Test::<單位>`」，頂層的四個是漏網的舊寫法，不是另一套規則；寫明「兩套都可以」等於留兩個出口。Kernel 的兩份標頭同在 `GYO::Test::Kernel`：`WorldTest` 本來就 `using namespace KernelTest`，兩份宣告的名字不重疊；若拆成 `GYO::Test::Kernel::World`，namespace 名 `World` 會遮住 `GYO::Kernel::World`。巨集（`KERNEL_REQUIRE_OK`、`KERNEL_REQUIRE_RUN_OK`、`ORCH_REQUIRE_BUILT`）寫 `::GYO::Test::…` 全限定。命名規範「N17 的根」補一條說明（`rebuild-naming.zh-Hant.md`）。守衛不查頂層的根（和產品的根分不出來），由審查看。`tests/` 裡剩下的頂層 namespace（`Legacy`：凍結的 master 程式，作特性比對；`Sample`：合成的錯誤型別；`OrchUnits`：合成的登記單位；`Walker`：scenario 的遊戲）都是在測試裡扮演別人的程式或產品的替身，不是測試輔助，不動。
5. **`detail` 對齊 N13 的 `Detail`**：`engine/core/io` 的 `MountTable.hpp`、`Vfs.hpp`，`engine/ecs/entity_store` 的 `EntityColumns.hpp`（`namespace detail`／`detail::` → `Detail`）；`engine/core/math` 的 `Frustum.hpp`（`FrustumDetail` → `Detail`，math 裡沒有別的 `Detail`）。gyo2 的突變清單沒有指到這些字（master 時代的 `voxel_map_editor_mutations.json` 有一個 `FrustumDetail`，屬範圍外的 15 份）。
6. **`SCAN_ROOTS` 只拿掉 `framework`，留 `apps`、`tools`**（和 brief 不同，理由）：framework 層 2026-11-11 退役，`build/` 已不讀它；`apps/`、`tools/` 雖然現在不存在，`build/cmake/GyoBuild.cmake` 仍照登記檔把 `apps/<app>`、`tools/<tool>` 加進建置（第 68、76 行），是產品的家。拿掉它們，之後加進來的產品打開 `GYO::Engine::…`、`GYO::Test` 時就不會報。不存在的掃描根照舊跳過。若主會話或使用者要照 brief 一起拿掉，改這一行與 pin 的自測即可。
7. **authority、rollback 在範圍內**：#266 之後單位從目錄推出，`engine/modules/authority`→`GYO::Engine::Authority`、`engine/modules/rollback`→`GYO::Engine::Rollback` 已自動涵蓋（第 4 批宣告 9.5 延下來的那條，不用補）。證據：在 `engine/` 的複本上把兩個單位各一個原始檔改成打開 `GYO::Engine::Orchestration`，守衛各報一條（`item7-probe.log`）。

文件跟著改的：`rebuild-naming.zh-Hant.md`（守衛位置、測試輔助一條）、`target/naming.zh-Hant.md`（守衛路徑）、`world-time.zh-Hant.md`（守衛欄的 ctest 名；WT-4 守衛欄原本寫已退役的 `wt-guard-naming-root-dropped`，改成現行的 `wt-guard-real-n1-other-namespace`；條文本身不變）、`entity-store.md`（`gyo_expect_links` 的位置）、`target/v2/README.md`（本 dev_log）。歷史紀錄（dev_log、HANDOFF、已核准的宣告）不改寫。

## 使用者的決定（照四樣，轉主會話記 HANDOFF；不是合併同意）

1. **合併前核准**（2026-11-22）
   - 原話：「核准，N13 選 (a)」。
   - 範圍：本卡回報時列的待決四件裡的前三件，也就是：
     - §14.4 `SCAN_ROOTS` 拿掉 framework（naming DI 1 守衛的放寬）；
     - §14.4 `test_n1_every_scan_root_is_scanned` 的收緊；
     - 第 6 件留 `apps`、`tools`（偏離 brief）。
   - 比過的選項：第 6 件是「照 brief 一起拿掉 apps、tools」對「只拿掉 framework」。
   - 理由：見各節的 1～5、第 6 件與設計審查第 7 項。
   - 出處：本卡對話。
2. **N13 補「引用別的單位」，選 (a)**
   - 原話：同上。
   - 比過的選項：
     - (a) 補這一句，殘留另開一批；
     - (b) 補這一句，殘留本批改完；
     - (c) 不補。
   - 理由：見「交使用者決定」與設計審查第 4 項。
   - 出處：本卡對話。
   - 處置：`rebuild-naming.zh-Hant.md` N13 補「Engine 的單位裡寫 `<單位>::`，Kernel 裡寫 `Engine::<單位>::`，巨集全限定」，「N17 的根」末記來源。殘留（不帶前導 `::` 的約 267 處、`::GYO::Kernel::` 16 處）交主會話開卡。

## AGENTS §14.4

### 自測收緊：`test_n1_every_scan_root_is_scanned` 的 engine 分支（naming DI 1 的守衛測試，只收緊；照 §14.4 仍要使用者核准）

1. **守的性質**：每個掃描根都被讀。
2. **緩める面**：無（只加斷言，原本的斷言不變）。
3. **締める面**：roots 檢查漏讀 `engine/` 底下不屬任何單位的檔時，自測失敗（以前通過）。
4. —
5. **根據與證據**：設計審查 B 的探針；`design-review/hole-closed.log`（兩種改法都 FAIL）；自測 53/53。

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

- 審查者：Fable 5.2／high，唯讀（實作者 Opus 5.5）。第一次啟動因 Fable 用量上限中斷；主會話通知額度恢復後，從中斷處續跑（中斷期間照 navigator 用 Opus 代替的審查已停掉，沒有採用）。報告：主 checkout `build/evidence/gyo2-naming-cleanup/design-review/REPORT.md`。同一個審查者也做了 §14.2 的敵對式檢查（`SCAN_ROOTS` 的判定變更）。
- A：8 項，保留 6、改 2。

| # | 項目 | 審查的判斷 | 處置 |
|---|---|---|---|
| 1 | naming_guard 搬家、突變收回 | 守衛讀六個根、不屬任何單位；orchestration_guard 不再依賴 kernel 目錄 | 保留 |
| 2 | `source_text.py` | 「一個出口、各自注入」的形狀，但還不是唯一出口：entity_store_guard、os_boundary_guard 各有一份逐字相同的 `strip_comments_and_strings`，`SUFFIXES` 已漂開（entity_store 少 `.ipp`、`.tpp`、`.inc`、`.ixx`）；json／logging／trace 的 `strip_block_comments` 是另一種讀法，不硬併 | 保留；兩份重複列後續 |
| 3 | `ExpectLinks.cmake` | `include_guard(GLOBAL)`＋使用方各自 include 是慣用法 | 保留 |
| 4 | `engine/` 短名 | 只換了帶前導 `::` 的 232 處；不帶前導 `::` 的全名約 267 處、`::GYO::Kernel::` 16 處仍在，同一型別兩種寫法；命名規範沒寫單位之間怎麼引用，下一批會繼續混寫。遮蔽的結果是編譯錯誤、不是靜默換義，可接受 | 規則一句交使用者（見下）；殘留列後續（brief 只點名 234 處） |
| 5 | 測試輔助收斂 | 方向對；`AuthorityTest` 漏網 | 改：同批補 |
| 6 | `Detail` | 純對齊 N13 | 保留 |
| 7 | `SCAN_ROOTS` 只拿掉 framework | 偏離 brief 的理由成立：brief 把「現在沒有」當成「gyo2 不再有」；拿掉 apps／tools 會讓「產品不打開 `GYO::`」與舊根的過渡規則在產品移植回來時失效 | 保留；請使用者知悉 |
| 8 | authority、rollback 在範圍內 | 靠目錄推出 | 保留 |

- 小點（不擋合併）：N1 forward 有四個實樹陽性對照，`wt-guard-real-n1-other-namespace` 與 `nm-real-acronym` 看起來重疊，下次碰到時可併（列後續）。
- B（敵對式檢查）：22 種改法，9 killed、13 存活。存活的 11 種不補：多掃（只會更嚴）2、讀法的深度 2（README 第 4 條：見到漏網才加）、某條規則在某個根靜默的交叉組合 6（守衛沒有按根分支）、等價 1。**該補的 1 個洞**（2 種改法）：roots 檢查跳過 `engine/`（迴圈改成 `SCAN_ROOTS[1:]`、或跳過 `engine/` 底下所有檔），自測與實樹都過——`test_n1_every_scan_root_is_scanned` 的 engine 分支只斷言「不是層」。本批以前就有。處置：那個測試的 engine 分支加兩個斷言（`engine/sample/Leak.cpp` 與層目錄下的散檔 `engine/core/Loose.hpp` 都要報 roots 的發現）；兩種改法現在都讓自測失敗（`design-review/hole-closed.log`）。照 README 第 4 條（清單的每個項目由自測逐一確認，清單只要一個突變，已有 `nm-scan-roots-examples`），不加突變。
- 拿掉 framework 本身：加回清單會被 `test_n1_scan_roots_are_pinned` 殺；§14.4 的 1～5 和程式一致。

### 交使用者決定：N13 補一句「單位之間怎麼引用」——已決定 (a)，見「使用者的決定」

- 現況：`engine/` 裡同一個型別有兩種寫法（例：`World.hpp` 的 `Engine::Base::Result` 與 `Executor.cpp` 的 `GYO::Engine::Base::Result`），命名規範沒有寫該用哪一種。
- 審查的建議：N13 補「Engine 單位內寫 `<單位>::`，Kernel 內寫 `Engine::<單位>::`，巨集全限定」，由審查看、不立守衛。
- 選項：(a) 補這一句；剩下約 267 處全名與 16 處 `::GYO::Kernel::` 另開一批改完（推薦：先有規則，下一批才不會繼續混寫；改名越晚影響越大）／(b) 補這一句，殘留在本批一起改完／(c) 不補，維持「兩種都可以」。
- 本卡沒有改規則（規則的變更要使用者同意）。

## 證據

主 checkout 的 `build/evidence/gyo2-naming-cleanup/`（使用者的 Intel Mac，git 不追蹤）：`configure-1.log`、`configure-2.log`、`build-1.log`、`ctest-full-1.log`、`short_names.py`、`item3-scan.log`、`item7-probe.log`、`judgement-unchanged.log`、`mutations/`（`full/` 是合併前的完整重跑）、`design-review/`（`REPORT.md`、敵對式檢查的腳本 `adv.py`、`adv2.py`、`count.py`、`hole-closed.log`）。

## 後續（範圍外，沒有做）

- `engine/` 裡不帶前導 `::` 的全名約 267 處（IO 最多）、`::GYO::Kernel::` 16 處（world_time 的 `HashValue`）：照 N13 新加的那一句改成短名，另開一批（使用者選 (a)）。
- 「切段找 `..`」的同一個運算式有 5 份（kernel_guard K5、orchestration_guard O4、entity_store_guard，以及 #267 新加的 trace_guard、logging_guard），各綁一個突變；能共用的只有「切段找 `..`」（gyo2-stale-cleanup 卡轉來，出處 `build/evidence/gyo2-stale-cleanup/design-review/REPORT.md` B 節第 3 項）。可併進 `source_text.py`，誰後合併誰對齊。
- N1 forward 的四個實樹陽性對照裡，`wt-guard-real-n1-other-namespace` 與 `nm-real-acronym` 看起來重疊（設計審查小點）。
- naming_guard 的 `SCAN_ROOTS` 不含 `build/acceptance`（建置會讀，現在沒有 C/C++ 檔）。本批以前就有的缺口，驗證者發現。
- `tests/common/entity_store/entity_store_guard.py`、`tests/common/os/os_boundary_guard.py` 各自還有一份 `strip_comments_and_strings`（和 `STANDARD_HEADERS`），可以改用 `source_text.py`。本批只處理 brief 指名的兩支守衛。
- 突變清單 `plans/audio`、`plans/time-threads-trace` 有 18 個指向 master 路徑的突變，不在 brief 列的「15 份 master 時代清單」裡；`authority_scan/mutations.json` 的路徑寫法要確認執行器怎麼解讀。
- brief 原列的範圍外照舊：15 份 master 時代的突變清單、文件路徑與 master 時代文件的去留、entity_store／trace／logging 三個 include 守衛的 `..`、Kernel「拿掉的名字不回來」的守衛、權威 Module 的 `gyo_expect_links`、K6 標頭分支的去留。

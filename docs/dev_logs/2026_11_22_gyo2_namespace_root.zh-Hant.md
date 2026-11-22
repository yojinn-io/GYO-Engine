# 2026-11-22 namespace 的根統一＋守衛（`v8-arch-gyo2-namespace-root`）

Owner：架構（GYO2 主會話開的任務卡）。分支 `claude/gyo2-namespace-root-c22a63`（draft PR #266，base `gyo2`）。實作者 Opus 5.5／high；敵對式檢查 Fable 5.2／high（唯讀）。

## 為什麼

- 2026-11-14 定了：帶過來的單位（base、math、os、time、threads、io、audio、entity_store）的 namespace 根從 `Engine::<X>` 統一成 N17 的 `GYO::Engine::<X>`，在「System 契約確認之後、移植任何產品之前」做（[命名規範](../architecture/target/v2/rebuild-naming.zh-Hant.md)第 6 節）。一直沒排。
- 使用者 2026-11-22：「先開這個，其他先不動。另外這類改名卡最好排前面，否則後面影響範圍會越來越大。不光是改名，還要建立規則，讓後面任務都遵守」。

## 做了什麼

1. **改名**（腳本＋逐一核對；腳本存在 `build/evidence/gyo2-namespace-root/rename_roots.py`）：
   - `Engine::<X>` → `GYO::Engine::<X>`（`::Engine::Base::` 這類全限定、`using namespace`、別名一起）；`Engine::Os` → `GYO::Engine::OS`，`Engine::IO` → `GYO::Engine::IO`（縮寫全大寫，見下）。
   - target 別名 `GYO::Io` → `GYO::IO`、`GYO::Os` → `GYO::OS`（`gyo_io`、`gyo_os` 與目錄不變）。
   - 單位內部用相對寫法指 OS 單位的 `Os::…` → `OS::…`（型別名 `OsError`、`IoError` 不動）。
   - 測試輔助的 `Engine::Test` → `GYO::Test`；logging、world_logging 測試裡打開單位 namespace 的 `GYO::Engine::Logging::Test`、`GYO::Engine::WorldLogging::Test` → `GYO::Test::Logging`、`GYO::Test::WorldLogging`。
   - entity_store 測試在測試檔裡打開 `namespace Engine::EntityStore { … }` 特化 `EntityKey`，改成全限定特化 `template <> struct GYO::Engine::EntityStore::EntityKey<T>`。
   - 改名後多餘的別名拿掉（logging 的 `Os`、`Threads`，trace 的 `Base`：在 `GYO::Engine::<單位>` 裡直接寫得到）。io 標頭三處註解的 `Engine::Error`／`Engine::Result` 改成 `Base::`。
   - 範圍：`engine/`、`tests/`、`examples/`、CMake、還在用的突變清單（find、replace、expect）、現行文件的程式片段與名字。歷史紀錄（dev_log、HANDOFF、已核准宣告與合併前核准頁）不改寫。master 時代、指向的檔案在 gyo2 已不存在的 15 份突變清單不改。
   - 統計：程式範圍 R1（舊根）1,406、R2（`Engine::Test`）31、R3（`Os::`）90、R4（target 別名）53、R5（mangled 名）5；文件 16 份。
2. **規則**（[命名規範](../architecture/target/v2/rebuild-naming.zh-Hant.md) N13、N14、N17 與第 3 節「N17 的根」）：`GYO::` 底下第二層的根是一張封閉的表（`Kernel`、`Engine::<單位>`、`Plugins::<plugin>`、`Test`），單位的名字從目錄名推，縮寫表 `io`→`IO`、`os`→`OS`；N17 的「Engine 的 Module」改寫成「Engine 的單位」（#261 之後 core、ecs、orchestration 不登記成 Module，但根照 11-14 的決定）。
3. **守衛**：重寫 `tests/common/kernel/naming_guard.py`，不另起新的。從原本三列固定的表，改成從目錄推出每個 Engine 單位的根（例外表 `UNIT_OVERRIDES` 只有 kernel）；加「根」的檢查（舊根 `Engine`——過渡規則；`GYO` 單獨；不在表裡的 `GYO::<X>`；在別的目錄打開某個單位的根；`GYO::Plugins` 在 `plugins/` 以外；`GYO::Test` 在 `tests/` 以外；單位外讀不懂的 namespace 宣告）。掃描範圍加 `examples/`。N3（角色後綴）照舊只查原本三個目錄。新突變清單 `docs/architecture/target/v2/scripts/naming_mutations.json`（16 個）。
4. include-order 編譯物件（`tests/common/orchestration/CMakeLists.txt`）的註解改成符合現況（它守的相撞已不存在；去留交使用者）。

## 使用者的決定（照四樣，轉主會話記 HANDOFF）

1. **縮寫全大寫，`Json` 不動**。原話：「IO、OS 這些就不要一個大一個小了。看著很難受。」比過的選項：縮寫全大寫（`Json` 也改 `JSON`）／縮寫全大寫、`Json` 不動／都只首字母大寫（`Io`、`Os`）。選第二個。理由：看著很難受；`Json` 不動的理由沒有說。出處：本卡的對話，2026-11-22。
2. **規則管 `GYO::` 底下全部的根**。原話：「另外GYO::Engine 既然有，那麼就會有其他的 例如非Engine 層的。」比過的選項：只管 `GYO::Engine::<單位>`（卡片原本的打算）／`GYO::` 底下的根做成封閉的表（卡片照這句話提出、使用者沒有反對）。理由：`GYO::Engine` 之外還有別的根。出處同上。
3. 開工前使用者看了要改的根的表，說「開始」。

## 本卡自己判的（名字照命名規範）

- `Engine::Test` 改 `GYO::Test`（不放進 `GYO::Engine`：它是整個 repo 的測試輔助，不是 Engine 的單位）；各單位的測試輔助放 `GYO::Test::<單位>`。
- 打開別人的 namespace 不准，特化用全限定寫法（N17 表下）。
- 守衛從目錄推出根，而不是逐列登記：加一個單位不用改守衛（AGENTS §7 的精神），拿掉一個 Module 也不用改守衛。

## 合併前要給使用者看的（Design Invariants 層級；名字和措辭不交使用者）

### 條文

- 命名規範 N13、N14、N17 與第 3 節新增的「N17 的根」（`rebuild-naming.zh-Hant.md`）。
- 現行文件裡 Design Invariants 的條文只跟著改名字（意思不變）：`time.md` Invariant 10、`threads.md` 9、`os.md` 第 3 點的 `GYO::Os` → `GYO::OS`；`world-time.zh-Hant.md` WT-4 的「程式裡的 `Engine::` 一律全限定」改寫成「Engine 的單位寫 `GYO::Engine::<單位>`」（全限定的理由——兩個根互相遮蔽——已不存在）。
- 正式的 `docs/architecture/target/naming.zh-Hant.md` DI 1（「對應以一張表給定，不要求兩邊拼字相同」）沒有改。守衛對 Engine 的單位要求從目錄名推出的拼字，是 N17「三者的名字要對得上」比 DI 1 更嚴的那一半；產品的拼字仍由產品定。

### 守衛判定的變更（AGENTS §14.4；Design Invariants 的守衛，不論鬆緊都要核准）

1. **守住的性質**：namespace 與目錄一一對應（DI 1、N17）；守衛不會因為什麼都沒掃到而通過。
2. **放寬的面**：以前 `engine/modules/world_time` 不見了會報（它是 ROOTS 的固定一列）；現在 Module 在 modules 層裡，拿掉或改名不報（自測 `test_n1_removing_a_unit_needs_no_guard_change` 正是釘這一點）。world_time 拿掉之後，N3 也不再查那裡。
3. **收緊的面**：所有 Engine 的單位（以前只有 3 個）都要用推出的根；`tests/`、`examples/`、`plugins/` 打開任何單位的 namespace、打開舊根 `Engine`、`GYO` 單獨、不在表裡的 `GYO::<X>`、`GYO::Plugins` 在 `plugins/` 以外、`GYO::Test` 在 `tests/` 以外，都報；`engine/` 底下不在層表裡的目錄報；單位外讀不懂的 namespace 宣告報（以前不報）；掃描範圍加 `examples/`。
4. **放寬的面是否由別的判定守著**：「什麼都沒掃到」仍由三處報——層表裡的層不見或是空的、`UNIT_OVERRIDES`（kernel）不見、單位目錄沒有原始碼（`naming_guard.py` 的 `units()` 與 `check_forward()`；自測 `test_n1_missing_orchestration_directory_is_reported`、`test_missing_mapped_root_is_reported`、`test_n1_layer_of_units_missing_or_empty_is_reported`、`test_n1_unit_without_source_is_reported`）。拿掉一個 Module 不再需要改守衛，這是 AGENTS §7（產品與 Module 的增減不改共通層）要的。
5. **根據與證據**：自測 61 案；敵對式檢查的報告與突變紀錄在主 checkout 的 `build/evidence/gyo2-namespace-root/`。

自測其他的改動都是同等或收緊：`test_n1_kernel_namespace_outside_kernel_is_reported` 的訊息改成 forward 的（time 現在是單位）；`test_n1_every_source_type_is_scanned` 的位置從 `engine/core/time` 移到 `tests/common/time`；`test_n1_every_scan_root_is_scanned` 的 engine、plugins 改用各自的訊息確認；`ks-n1-mapped-root-silent` 原本合併了兩個自測的 expect，拆成它（kernel 不見）和 `nm-layer-gone-silent`（層不見）。

### 要使用者決定的（設計審查提出、本卡判斷是使用者的事）

- **DI 1 的收緊**：正式的 DI 1 寫「不要求兩邊拼字相同」；N17 與守衛對 Engine 的單位改成「預設從目錄名推，例外寫進 `UNIT_OVERRIDES` 並附理由」。產品不受影響。推薦同意（設計審查也建議）。
- **N3（角色後綴，舊命名條文第 5 條）**：2026-11-14 沒有轉入 gyo2，守衛仍在 kernel、orchestration、world_time 三個目錄查（本卡沒有擴大、沒有拿掉）。選項：(a) gyo2 上拿掉（設計審查推薦）；(b) 留三個目錄；(c) 擴到全部單位。它是 DI 3 的守衛，可以不在這一批定。
- **縮寫表的範圍**：現在只管 namespace 與 target（`GYO::Engine::IO`、`GYO::IO`）；型別名與檔名照 N15、N16 的駝峰（`IoError`、`OsError.cpp`）。要不要也改成 `IOError`、`OSError`？推薦先只管 namespace 與 target。

- **include-order 的編譯物件與它的兩個突變**（`ur-include-order-kernel-unqualified`、`wt-include-order-unqualified`）：`gyo_orchestration_include_order` 存在的理由是舊根 `Engine::` 和 `GYO::Engine` 互相遮蔽（標頭裡沒全限定就編不過）。改名之後只有一個根，兩個突變的改法變成等價、存活。選項：(a) 連同編譯物件一起退掉（推薦：理由已不存在，不留空的檢查）；(b) 留著編譯物件當「標頭自足」的檢查，突變改寫成拿掉一個 include；(c) 留著不動（兩個突變存活）。退掉會讓那組編譯物件失去唯一的突變，照 README「尋ねるもの」要問。

## 驗證

- 本機 macOS（x86_64，Intel i9-9980HK）`ctest --preset test`：改名前 161/161（基準），改名後 161/161，守衛補強後 161/161。
- 舊根掃描（`git grep -P`，先用改名前的 `HEAD:engine/core/base/include/engine/base/Assert.hpp` 證明會命中）：程式範圍的命中只剩守衛自測裡刻意寫錯的範例與 `ks-real-old-namespace` 的 replace。
- `naming_guard.py` 在真實樹上 clean；自測 74/74。
- L1（手動 dispatch，三平台）：見 PR。
- 敵對式檢查（Fable 5.2／high，唯讀，守衛與突變清單的快照）：試了 94 種改法，41 種存活（約 19 種落在可修的 9 項，其餘是等價、崩潰即 fail closed、或刻意構造）。處理：改寫的 3 個舊突變 replace 沒跟著改縮排（語法錯誤、不是 killed）→ 修；N3 的範圍、空的層、其他層的 target、target 正規式、單位裡的 `GYO::Test`、單位自己 namespace 裡的別名、`engine/`／`plugins/` 直下的原始檔、`namespace Engine = …` 別名、外掛與縮寫的逐段推法 → 補自測 10 案、補突變 7 個，守衛補兩處（直下的原始檔、舊根別名）；其中直下原始檔、舊根別名、target、plugin 推法、N3 範圍的部分在設計審查後拿掉（見下節）。只記錄、不做的：掃描範圍外的頂層目錄、兩個單位推出同名（CMake 的重複 target 會擋）、巨集或 `#if` 拆開的 namespace、`.cppm`／`*.h.in`、CMake 註解塊裡的別名（寫進守衛的 Limits）。
- 設計審查：見下節。審查後的守衛：自測 61/61、真實樹 clean、全量 ctest 161/161（見下）。

## 突變

重跑的範圍（D184）：entry 改過的、find 在改動範圍的、`naming_guard` 的，加上非機械地改了測試的三份（entity_store、base、logging）整份。

| 清單 | 結果 | 備註 |
|---|---|---|
| `naming_mutations.json`（新） | 16/16 killed | 第一輪 21/21；敵對式檢查後 26/26；設計審查拿掉 target、直下原始檔、別名、plugin 推法、N3 範圍的 10 個後 16/16 |
| Kernel `mutations.json` | 47/50 → 修後 naming 相關 14/14 | 沒 killed 的 3 個是本卡改寫時漏改 replace 的縮排（敵對式檢查第 1 項），已修；`ks-n1-*` 5 個改寫並補 `guards` |
| `orchestration_mutations.json` | 25/27；naming 相關 2/2 | `ur-include-order-kernel-unqualified` 存活（見「要使用者決定的」）；`or-runner-logs-back` build_failed，改名前就壞（`Product.cpp` 沒 include `Log.hpp`，本卡沒有改 include） |
| `world_time_mutations.json` | 1/2；`wt-guard-real-n1-other-namespace` 1/1 | `wt-include-order-unqualified` 存活（同上）；`wt-guard-naming-root-dropped` 退役（ROOTS 的列不在了，名字的推法改由 `nm-unit-name-underscore` 守） |
| os-subsystem | 16/16（2 個此平台跳過） | |
| entity-store | 15/15 | |
| base | 10/10 | |
| logging | 92/92 killed | 第一輪的紀錄被 trace 的覆蓋（本卡產生紀錄檔名的寫法讓兩份撞名），重跑一次 |
| rollback、trace、io_user_files | 3/3、3/3、2/2（1 個跳過） | |

加的突變（README 第 8 條）：`naming_mutations.json` 16 個；退 1 個（`wt-guard-naming-root-dropped`）。參考值「條數 × 5」以 N17 一條計是 5。超過的理由：守衛從固定的三列改成推導＋兩條規則（forward、根）＋舊根的過渡規則。README 第 4 點要求每條規則三個（關掉、真實樹的陽性對照、fail closed），再加每個獨立的分支或清單一個：推導 4（縮寫表、`_` 分段、例外表、真實樹）、層的 fail closed 4（未知層、空層、層不見、單位沒有原始碼）、根 6（`GYO` 單獨、不在表裡、非單位的根的位置、讀不懂、掃描範圍清單、真實樹）、舊根 2（關掉、真實樹）。

## 設計審查（Fable 5.2／high，唯讀）

報告摘要在主 checkout 的 `build/evidence/gyo2-namespace-root/reviews/README.md`。總評：核心合理，也就是單位的 namespace 由目錄決定、namespace 只在自己的目錄打開、`GYO` 的根是封閉表，這是 N17 的本質；外圍有五塊在擋誤用或提前釘死。五問的結論：

1. 機制有沒有分散：合理，推導只在一處。
2. 責任與依賴方向：有疑問，守衛的 owner 還是 Kernel 的測試（列為後續）。
3. 能拿掉的：有幾處，處理如下。
4. 本質還是過度治理：核心合理，外圍不合理。
5. 和別家比：核心合理。沒有哪家 C++ 引擎機械地強制 namespace↔目錄；Rust、Go 由語言或工具鏈強制這個性質，GYO 用文字守衛換到同樣的性質。

本卡照使用者的原則（守衛只守設計本質、不擋誤用、能拿掉的就拿掉）自己判、這一批做了：

- 拿掉名為 `Engine` 的別名檢查：別名不是打開，屬於擋誤用。
- 拿掉「不屬於任何單位的原始檔」：打開單位 namespace 的情況本來就會報，剩下的只是在管建置結構。
- 拿掉 target 檢查：這是本卡自己加的，不是 brief 要的。建置與登記檔負責 target，Unreal 也是在建置工具裡強制。
- Plugin 只守根：`GYO::Plugins` 只能在 `plugins/` 打開。每個 plugin 的拼字等第一個 plugin 的那批定；依據 AGENTS §3「守衛立的時機」，而且照現在的推法 `sdl` 會變成 `Sdl`，和使用者對 IO、OS 的要求相反。
- 拿掉 N3 範圍的突變和 pin。
- N17 補兩句：`UNIT_OVERRIDES` 就是 DI 1 的例外表；舊根檢查是過渡規則，並寫明拿掉的條件。
- 改正 include-order 的註解。

要使用者決定的，見「合併前要給使用者看的」。

## 後續（範圍外，沒有做）

- 設計審查提出的：`naming_guard` 搬到中立的 owner（例 `tests/common/naming/`，ctest `naming.guard`），共用的讀法函式從 `kernel_guard` 抽出，四份清單裡的突變歸位；`engine/` 裡 234 處 `::GYO::Engine::` 是相撞時期的繞法，現在可以寫短名；測試輔助的 namespace 有兩套（`GYO::Test::<單位>` 與 `KernelTest`／`OrchTest`／`RollbackTest`／`WorldTest`），要收斂或寫明兩者都可；單位裡的巢狀名會遮住同名的兄弟單位（`OS::Files`、`IO::Path`），加單位時由審查看；`SCAN_ROOTS` 的 framework、apps、tools 在 gyo2 上不存在，清單可以縮；`kernel_guard.camel_case` 和 `unit_name` 是同一個轉換。

- io 的 `IO::FS` 是全大寫縮寫，合現在的縮寫表；io、entity_store 內部用小寫 `detail`，math 用 `FrustumDetail`，和 N13 的 `Detail` 不一致。
- 守衛的 N3（舊命名條文第 5 條的角色後綴）2026-11-14 沒有轉入 gyo2，但守衛仍在三個目錄上查。
- 15 份 master 時代的突變清單（`docs/architecture/plans/{framework-*,input-and-present,event-sounds,big-map/engine_grid}`、`docs/tools/plans/v8/scripts/*`）指向的檔案在 gyo2 已不存在。
- 現行清單裡 find 已找不到的突變（改名前就是這樣，不是本卡造成）：Kernel 清單 `ks-k6-calls-off`、`ks-k6-every-file-exempt`、`rs-iv-fc3-step0-skipped`、`hf-round-skips-initial-values`、`ks-s2-interrupted-stops-refusing`、`ks-s5-change-deferred`、`ks-s5-change-stamp`、`obs-host-err-not-failed`、`obs-exclusive-err-not-left`、`obs-host-no-snapshot-after`、`el-kernel-print-back`、`ks-restore-record-by-record`；權威 `au-input-rolled`、`au-digest-resource-dropped`；回滾 `rb-input-capacity-unchecked`。可能是 `gyo2-batch4-followups` 卡正在處理的。
- `docs/architecture/entity-store.md`、`math.md`、`error-handling.md` 的路徑還是 `engine/<模組>`（沒有層）；`docs/architecture/trace.md`、`input.md` 是 master 時代的文件。

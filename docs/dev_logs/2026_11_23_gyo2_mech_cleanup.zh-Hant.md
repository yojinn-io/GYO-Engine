# 2026-11-23 GYO2：累積的機械式清理（突變清單、日誌測試的暫存檔、dev_log 狀態行）

批次代號：`v8-arch-gyo2-mech-cleanup`。分支 `claude/gyo2-mech-cleanup-44273a`（從 `gyo2` 0bb64a28）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的（使用者「開 1、2、3」的第 3 張）。實作者 Opus 5.5／medium；突變退役判斷的審查者 Opus 5.5／high（唯讀）。

## 起因

批次表〈後續小事〉累積的五件機械式清理（不涉及設計變更）：

1. 9 個本來就壞的突變退役或重寫。
2. 日誌測試把 `logging-test-files/` 寫進工作目錄，改寫到建置目錄。
3. 三份突變清單的路徑還指向 master（big-map engine_format、audio、time-threads-trace）；authority_scan 的路徑寫法待確認。
4. `orchestration_mutations.json` 裡 `or-exch-round-after-failed-in` 的舊副本。
5. 已合併批次的 dev_log 開頭補「已合併」。

## 先查現況：五件裡有三件已被前面的批次做掉

動手前在 `gyo2` 0bb64a28 上逐件查（靜態掃描用 Python 讀全部 `*mutations*.json`，逐條數 `find` 在 `file` 裡的命中次數；先用已知存在的 id 命中驗證走查有效）。

| 件 | 現況 | 由誰做掉 |
|---|---|---|
| 1 | Kernel 的 3 個：`ks-k6-every-file-exempt`、`ks-k8-skips-executor-files` 已退役，`ks-k6-calls-off` 已改寫（打 K6 的迴圈本身）。os-subsystem 的 6 個（`os-g8-link`、`os-g13-definition-name`、`os-g14-definition-platform-id`、`ti-x1-time-links-more`、`ti-x2-threads-links-more`、`th-x1-threads-os-public`）已全部改寫、沒有退役 | Kernel：第 4 批後續 PR #265（dev_log `2026_11_22_gyo2_batch4_followups`）；os：過時突變 PR #260（dev_log `2026_11_21_gyo2_stale_mutants`） |
| 2 | `AppendTests.cpp`、`FileSinkTests.cpp` 已寫到 `GYO_LOGGING_TEST_BINARY_DIR`（`tests/common/logging/CMakeLists.txt` 給 `${CMAKE_CURRENT_BINARY_DIR}`）；`FormatTests.cpp` 用 `temp_directory_path()` | trace 批第 4 步 db83c6ef（trace 宣告判定 8） |
| 4 | 舊副本已不在；清單 288 條、沒有重複的鍵、沒有重複的 id，`or-exch-round-after-failed-in` 只剩現行那一份（find 對 `RunRound`） | 讀檔實作 d7c0dd74（dev_log `2026_11_22_gyo2_restore_impl` 第 41 行） |

這三件本批不改程式，只跑相關突變與測試確認（下面「驗證」）。批次表那幾列改成完成，寫上由誰做掉。

## 第 3 件：master 路徑的突變清單

掃描結果（`find` 所在的 `file` 不存在）：

| 清單 | 總數 | `file` 不存在 | 處置 |
|---|---|---|---|
| `docs/architecture/plans/big-map/scripts/engine_format_mutations.json` | 68 | 67（`engine/voxel_mesh/src/VoxelMesh.cpp` 41、`engine/map_markers/src/MapMarkers.cpp` 24、`engine/map_markers/CMakeLists.txt` 1、`engine/core/base/include/engine/base/StrictJson.hpp` 1） | **整份刪除（退役 68）** |
| `docs/architecture/plans/time-threads-trace/scripts/mutations.json` | 20 | 6（`engine/trace/src/TraceSink.cpp`） | **退役 6**，其餘 14 條留 |
| `docs/architecture/plans/audio/scripts/mutations.json` | 87 | 0 | 不動：87 條的 `find` 都恰好命中一次；這份清單在批次表記下之後由 audio 重審（`v8-arch-gyo2-audio-review`）與 audio 小批 #288（03b6ea0c 改了 30 個 `file`）跟上 |
| `tests/common/authority_scan/mutations.json` | 18 | 掃描時 18 條「不存在」是假陽性 | 不動：`file` 照執行器 `run_mutations.py` 的寫法以**清單所在目錄**為基準（`DIRECTORY / mutant["file"]`），兩個檔都在 `tests/common/authority_scan/`。實跑 18/18 killed |

### 退役的理由（AGENTS §14.5、`tests/common/mutation/README.md` 第 7 點）

判準：守的對象還在、又沒有別的突變捕同一個誤，就重寫；否則退役。

- **engine_format 68 條**：守的是 master 的 `GYO::VoxelMesh`（`gyo.voxel_map`、`gyo.voxel_region`）、`GYO::MapMarkers`（`gyo.map_markers`、`gyo.marker_kinds`）與 `StrictJson`。`gyo2` 的 `engine/`、`tests/`、`plugins/`、`config/`、`build/cmake/` 裡 `VoxelMesh`、`MapMarkers`、`voxel_region`、`map_markers`、`marker_kinds`、`StrictJson` 都是 0 命中（`voxel_map` 唯一的 1 處是 Kernel 守衛測試假樹裡的產品名字串，與格式無關）。唯一 `file` 還在的 `vm-guard-gameplay-word` 改的是 `docs/architecture/voxel-mesh.md`，但它跑的 CTest `gyo_voxel_mesh.neutral_names` 在 `gyo2` 不存在。守的對象全部不在 → 整份退役。和 stale-cleanup（`v8-arch-gyo2-stale-cleanup`）刪 15 份 master 清單同一個處理；那一批把這份列為「部分 `file` 不存在、不在 15 份裡」，本批查完確定沒有一條能留。
- **tt2 6 條**：守的是 master `engine/trace` 的 `TraceSink`。`gyo2` 的 trace 重建時不再有自己的傳輸（trace 宣告第 186 行，選項 (b)「和 `JsonLinesFileSink` 幾乎一樣的東西寫兩份」被退掉），`TraceSink` 只剩 `engine/modules/logging` 註解裡的「照 master TraceSink」。`TraceSink.cpp` 不在，6 條都從這份清單退役；但同樣的傳輸決定搬到了 logging 的 `JsonLinesFileSink`（LG-6），所以逐條看 logging 清單有沒有突變捕同一個誤（審查者指出第一版只寫「已有自己的突變」、有 3 條對錯）：

| 退役的 | logging 那邊 | 處置 |
|---|---|---|
| `tt2-drop-uncounted`（滿佇列不計數） | `lg-file-full-not-counted` | 已有，不加 |
| `tt2-timer-wake`（寫者靠計時器醒） | `lg-file-writer-timer` | 已有，不加 |
| `tt2-accept-after-close`（Close 後仍收） | `lg-file-closed-not-counted`；`gyo2` 的 `Write` 是 `void`，沒有「回 false」這個決定；整段拿掉 `if (closed)` 會被同一案例的 `droppedClosed == 1` 與守恆式抓到 | 已有，不加（README 第 1 點） |
| `tt2-flush-no-wait`（Flush 不等寫者） | 沒有：`lg-file-flush-ignores-failure` 只拿掉失敗判斷，不是同一個誤 | **改寫成 `lg-file-flush-no-wait`**（拿掉 `JsonLinesFileSink.cpp` 的 `wait_until`；案例「Flush waits for the lines accepted before it」的 `now() - start >= 50ms` 失敗）。killed |
| `tt2-no-wake`（推進空佇列不喚醒寫者） | 沒有 | **列後續、不加**：試過改寫成 `lg-file-push-no-wake`（拿掉 `Push` 的 `if (wake) writer->Notify();`），結果 **timeout**，不算 killed——用 `WaitForWriteStarted` 的案例都先 `Hold()` 了 output，`REQUIRE` 失敗後解構的 `Close` 喚醒寫者、寫者卡在被 Hold 的 output。其他案例都靠 Flush 或 Close 喚醒，捕不到。要捕到得改測試，超出本批 |
| `tt2-final-batch-unwritten`（停止後那一批沒寫、結尾行太早） | 沒有：`lg-file-no-end-line` 是整行不寫，不是同一個誤；`gyo2` 也沒有 master「busy writer」那種決定性的案例 | **列後續、不加**：要先補決定性的測試（Hold 住 output、排幾行、在寫者被擋時 Close） |
- 退役的 id 在其他清單、Python、CMake、workflow 裡沒有引用（`git grep` 0 命中；先用留下的 `tt2-role-backend-fixed` 命中 1 次驗證走查有效）。

### 留著的引用（不改，列後續）

- 現行文件 `docs/architecture/voxel-mesh.md:246`、`docs/architecture/map-markers.md:77`、`:133` 用文字指著刪掉的 engine_format 清單當守衛出處；兩份文件描述的都是 `gyo2` 上不存在的 master Module。和 stale-cleanup「後續」列的 6 份文件同類，交 Legacy 文件整理。
- 計畫資料夾（`plans/big-map/HANDOFF.md`、`engine-format.zh-Hant.md`、`plans/kernel/guards-inventory.zh-Hant.md`）是當時紀錄，不改寫。

## 第 5 件：dev_log 狀態行

- `2026_11_22_gyo2_threads_io_review.zh-Hant.md`：開頭加「狀態：已合併（PR #277，merge 0f3be2cc）」。
- `2026_11_22_gyo2_state_machine.zh-Hant.md`：開頭加「狀態：已合併（PR #281，merge 1df88fe7；L1 run 45042604998）」。
- 寫法照 time_events dev_log 的先例（狀態行＋「本行由哪一批補上」）。

## 驗證

（本機 macOS Intel，preset `test`，全新 configure＋全量建置。）

| 項目 | 結果 | 證據 |
|---|---|---|
| 全新 configure＋全量建置 | 364/364，exit 0 | `configure.log`、`build.log` |
| ctest | 196/196 通過 | `ctest.log` |
| 第 1 件：os-subsystem 的 6 條 | 6/6 killed | `os-nine.log` |
| 第 1 件：Kernel 的 `ks-k6-calls-off`，與 #265 合進去的 `ks-adv-executor-exempt-all`、補的 `ks-k6-log-header-off` | 3/3 killed | `kernel-k6.log` |
| 第 2 件：從 repo 根直接跑 `gyo_logging_tests`（8628 個斷言通過）後 `git status --ignored` 沒有 `build/` 以外的未追蹤檔；`logging-test-files/` 出現在 `build/target/_build/test/tests/common/logging/`（正例） | 根目錄無殘留 | `logging-root.txt` |
| 第 3 件：time-threads-trace 剩下的 14 條（改過的清單） | 14/14 killed | `ttt.log` |
| 第 3 件：authority_scan 全部 | 18/18 killed | `authority_scan.log` |
| tt2 改寫：`lg-file-flush-no-wait` | killed；試過的 `lg-file-push-no-wake` timeout（沒放進清單） | `logging-new.log` |
| 靜態掃描：全部 `*mutations*.json` 的 `find` 在 `file` 裡恰好命中一次 | 全部符合（authority_scan 以清單所在目錄為基準） | 未保存（掃描指令的輸出，判定以上面的實跑為準） |

## 突變的增減（README 第 8 點）

- 退役 74：engine_format 68（整份刪除）、time-threads-trace 6。理由見上，都是「守的對象（檔案）在 `gyo2` 不存在」（README 第 7 點）。
- 新增 1：`lg-file-flush-no-wait`（`tt2-flush-no-wait` 的改寫，落在 logging 清單）。它只捕「Flush 不等寫者」，logging 清單裡沒有別的突變捕這個誤（README 第 3 點）；`guards` 寫 `logging LG-6`。
- 第 1 件的 9 條在 #260、#265 已處理，本批沒動。

## 證據

本機 macOS Intel，主 checkout（`/Users/karasu/Code/Source/GYO-Engine`）的 `build/evidence/gyo2-mech-cleanup/`（git 不追蹤）：`authority_scan.log`、`kernel-k6.log`、`os-nine.log`、`ttt.log`、`logging-new.log`、`logging-root.txt`、`configure.log`、`build.log`、`ctest.log`。

## 審查

突變退役／改寫的判斷交審查者 Opus 5.5／high（唯讀）看一次。結論：退役都成立；指出 tt2 有 3 條在 `JsonLinesFileSink` 還有同一個決定卻沒有突變（照上表處理：1 條改寫、2 條列後續），以及 StrictJson 的重複鍵性質由 GYO::Json 的 J-2 承接（`js-repeated-small-unchecked`、`js-repeated-large-unchecked` 守），所以 engine_format 裡那一條退役也不留缺口。

## 後續（交主會話）

- logging LG-6 的兩個決定沒有突變守（owner：logging）：「推進空佇列才喚醒寫者」（`JsonLinesFileSink.cpp` 的 `if (wake) writer->Notify();`；現有案例都 Hold 住 output，突變只會 timeout）與「Close 寫完停止後取到的那一批、再寫結尾行」（沒有決定性的案例）。兩者都要先動測試，再加突變。

- `voxel-mesh.md`、`map-markers.md` 指向已刪清單的文字（見上）→ Legacy 文件整理。
- 清單 `description` 裡的舊執行器路徑 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py`（entity-store、framework-rework、os-subsystem 三份）：#260 已列後續，本批不碰。

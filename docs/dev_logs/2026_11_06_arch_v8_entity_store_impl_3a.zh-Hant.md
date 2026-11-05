# Engine 架構：v8-arch-entity-store-impl-3a（Engine 的 entity store 欄的部分＋pvp 換上）

日期：2026-11-05～11-06。分支 `claude/v8-arch-entity-store`（PR #156，草稿）。主會話 Fable 5.1，途中由使用者改為 Opus 5.5（已核准計畫的模型指派照計畫）。實作者 Opus 5.5／high（第一個在程序結束時中斷，由第二個接手）；審查者 Opus 5.5／high（唯讀）；§14.4 檢證者 Fable 5.1／high（xhigh-reviewer，唯讀）。估時 9～13 h，實際約 4 h（中斷前的實作者時間未精確計入）。

**狀態：事前宣告的判定 1～7 全部通過。** 上段任務。計畫見 [README](../architecture/plans/entity-store/README.md)，[事前宣告](../architecture/plans/entity-store/impl-3a-declaration.zh-Hant.md)，經過見 [HANDOFF](../architecture/plans/entity-store/HANDOFF.md)。

## 做了什麼

- 新 Engine 模組 `engine/entity_store` → `GYO::EntityStore`（header-only、只連 `GYO::Base`，`838e7f8e`）：`EntityId`（`std::uint64_t`）、`EntityKey<T>` trait 與 `EntityOf`（預設讀成員 `id`，所有者的特化優先）、`FindEntity`／`InsertEntity`／`EraseEntity`／`StrictlyAscending`（演算法與 pvp 原本相同：lower_bound 插入、重複是 Programmer Error、保序刪除、二分查找）。`DeferredChanges` 不在本批（3b）。
- common 守衛 `tests/common/entity_store/`：契約 Guard 表第 1～3、7、9、10 列的測試（合成型別，含比較次數上限，抓線性搜尋）；configure 時的連結檢查（只連 `GYO::Base`）；字詞守衛 `entity_store_guard.py`（產品名、unordered、hash、clock、random、thread、atomic、static 資料；include 白名單）與自我測試。
- Engine 突變目錄 `docs/architecture/plans/entity-store/scripts/mutations.json`（15 個），以 time-threads-trace 的 runner 執行。
- pvp 換上（`2aa9c9df`）：`EntityId.hpp` 改為 Engine 型別的別名（保留 `PlayerId`、`NoEntity`）；`EntityColumns.hpp` 變成轉接（using 宣告＋`playerId` 過渡型別的 `EntityKey` 部分特化，同時有 `id` 的型別以 static_assert 擋下）；刪 `HasEntityId` 多載；`match_domain` 與 -O0 digest runner 連結 `GYO::EntityStore`。`PvpMatch.cpp`、`MatchRuntimeHost.cpp`、`IpcHost.cpp` 一行都沒改。
- 產品測試：`Fresh` 型別的欄測試搬到 common，`Legacy`（`playerId` 轉接）留在產品。產品突變：M1、M2 退役（使用者決定，見下），新增轉接突變 `entity-store-adapter-key-not-player-id`。
- 文件：entity-model.zh-Hant.md 的 §0 第 2 項、§3.1、§11 第 4～5 項、E-5 守衛欄、G1 列與 M1／M2 項加日期註記（不改寫歷史；E-12 留到 3b）；契約 entity-store.md 的狀態句改為「欄的部分已實作、`DeferredChanges` 在 3b」。

## 判定

件數的對照：一致（情境 42＝42；records 合計兩樹在 scale 1 皆 19,636、scale 10 皆 195,757，逐情境相同；修正後再以 runner 比對 scale 1，42 情境 digest 與 records 全同）。

| # | 判定 | 結果 |
|---|---|---|
| 1 | 搬移 0 差分 | 通過：`compare_authority_trees.py` `--scale 1`、`--scale 10` 皆 exit 0、42 情境 0 差分、digest_version 4；base `2c959fbc`（程式同宣告的 `ffd64346`）、branch `2aa9c9df`。審查修正後只多一個 static_assert，以修正後 runner 對照前次 branch 的輸出，42 情境全同 |
| 2 | golden 不變 | 通過：`authority_digest` 通過；golden 與 `AuthorityDigest.cpp` 無 diff |
| 3 | 範圍不外溢 | 通過：protocol、gateway、services、`apps/object_fps_pvp/src` 無 diff |
| 4 | Engine 守衛的檢出力 | 通過：15/15 killed，只靠 common（測試、字詞守衛、連結檢查）；審查修正後重跑仍 15/15 |
| 5 | 產品守衛不退化 | 通過：初次 M1～M6 6/6（M1、M2 由 common 殺）；退役後 M3～M6＋轉接突變 5/5；G1、G2、G3、G7、G13 Green |
| 6 | 依賴與字詞 | 通過：連結檢查（`es-row9-links-math` 證明會失敗）、字詞守衛與自我測試（18 案例）通過；產品無 `HasEntityId` |
| 7 | L1 | 通過：[run 42056321286](https://github.com/yojinn-io/GYO-Engine/actions/runs/42056321286)（`fd52f8a7`，手動觸發 `cross-platform.yml`）：L1 macos-arm64、linux-x64、windows-x64 與 Quick acceptance 全部 success |
| — | ctest cpu\|shader（本機 Mac） | 126/126；本機沒有 shader 標籤的測試 |

## Test 判定的變更（§14.4）

**M1、M2（`entity-split-insert-appends`、`entity-split-erase-swap-and-pop`）：先改指 Engine 標頭並由 common 測試殺，之後退役。** 使用者 2026-11-06 承認並決定退役；說明採檢證者收窄後的版本：

1. 守的性質：插入在 lower_bound 的位置、不是尾端附加；刪除保留其餘元素的順序（E-5 的機制、契約 DI 2）。
2. 放寬的面：舊判定要求產品整套 cpu 失敗（產品單元測試、G1 整合、`RequireEntityOrder` 三個偵測器任一）。對這兩段固定的突變文字，沒有舊判定殺得掉、新判定放過的輸入：以舊判定重跑搬到 Engine 標頭的 M1、M2 也都 killed。放寬的是驗證冗餘：不再證明產品整合路徑也能抓到這種錯。
3. 收緊的面：舊 expect 接受任何 `GYO_ASSERT failed`，同文的 Engine 突變只接受 common 測試的案例名。
4. 性質仍被守：Engine 目錄同文的 `es-row2-insert-appends`、`es-row2-erase-swap-and-pop`，由 common 測試殺；產品 G1 與 `RequireEntityOrder` 仍在 L1 cpu，並由 M4 的 expect 持續驗證。產品側 `playerId` 路徑的刪除保序只剩 G1、G2 的整合覆蓋。
5. 證據：`product_mutations.txt`、`engine_mutations.txt`、`m1m2_old_check_on_engine_header.txt`、`verifier-1444-report-20261106.md`。

**新增轉接突變的判定**（新判定，不是放寬）：`expect` 為 `Entity columns insert at the ordered position|GYO_ASSERT failed: row`。鍵一錯，PvpMatch 的每一欄都受影響，按檔名順序先跑的測試在 `PvpMatch.cpp:27` 的 `Row()` 就觸發 assert、程序結束，Legacy 案例跑不到；要求只由 Legacy 案例殺需要帶案例篩選的 check，目錄目前沒有這種形式。

## 經過

- 第一個實作者在 Claude Code 程序結束時中斷：Engine 部分已 commit；pvp 換上未提交；Engine 突變的第一次執行被中斷，留下突變 `es-row10-entityof-reads-id` 的置換文字在 `EntityKey.hpp`。主會話還原並掃描兩個目錄無其他殘留，由第二個實作者核對後接手。之後每次 runner 跑完都檢查 `git status`。
- 全量建置時 -O0 digest runner 編譯失敗（找不到 Engine 標頭）：它直接編產品 domain 原始碼、連結清單手抄自 `match_domain`。在產品自己的 CMake 加 `GYO::EntityStore`；兩處維護是既有設計，審查者不要求重構。
- 檢證者發現產品轉接標頭沒有任何登錄的突變，補上 `entity-store-adapter-key-not-player-id`。
- 審查 must 0／should 3／note 8：S1 同時有 `id` 與 `playerId` 的型別改成編譯失敗（以前會默默用 `playerId` 當鍵，下一批的投射物正可能長這樣）；S2 字詞守衛改成 include 白名單（`<pthread.h>`、`<stdatomic.h>` 之前擋不到）；S3 測試加 assert 攔截（兩個突變原本靠 abort 前輸出已刷出才算 killed，改後 returncode 1）。N2（`id` 型別的隱式轉換）照契約不改。
- 主會話給實作者的指示讓 M1、M2 留在產品目錄、改指 Engine 標頭，與 delta §1 第 4 項「搬到 Engine 目錄」不一致；交使用者決定，使用者選退役。

## 證據

主 checkout `build/evidence/entity-store/impl-3a/`（git 管理外，本機 Mac）：`compare-scale1/`、`compare-scale10/`、`count_reconciliation.txt`、`compare_console.txt`、`engine_mutations.txt`、`engine_mutations-fix.txt`、`product_mutations.txt`／`.json`、`product_mutations-fix.txt`／`.json`、`product_mutations-fix-attempt1.txt`、`m1m2_old_check_on_engine_header.txt`、`old_check_on_relocated.py`、`engine_assert_mutants_returncode-fix.txt`、`adapter-static-assert-fix.txt`、`branch-fix-vs-branch-scale1.txt`、`ctest_cpu_shader.txt`、`ctest_cpu_shader-fix.txt`、`ctest_entity_pvp-fix.txt`、`implementer-report-20261106.md`、`review-20261106.md`、`verifier-1444-report-20261106.md`。`engine_mutations_run1.txt` 是中斷留下的空檔，不算數。

## 留給之後

- 3b（`DeferredChanges`、G12、E-12 措辭的改寫）與第一個非玩家實體批同時做。
- 圖書管理員已在本 PR 以 3a 版改 `docs/architecture.md` 與 `docs/creating_apps.md:41`（`c7878e47`）；3b 合併時再通知它補成完整版。
- 檢證者列的缺口 2：產品側 `playerId` 路徑的 `EraseEntity` 沒有單元測試，只靠 G1、G2 整合覆蓋；記錄，不在本批補。

# PvP v8：v8-product-match-entity-split（Match 內部拆成實體元件欄與輸入來源欄）

日期：2026-11-03。分支 `claude/gifted-khorana-d38ac5`（entity-line）。檔位 high、S3／S5 局部 xhigh；上段（Architecture 判斷、DI 守衛測試）；實作者主會話 Fable 5.1、審查者 `xhigh-reviewer` Opus 5.5 xhigh、§14.4 檢證者未用到（沒有判定變更）。事前宣告：[v8-product-match-entity-split-declaration](../object_fps_pvp/plans/v8/v8-product-match-entity-split-declaration.md)（已同意）。估 14～20 h，實際約 7 h（含審查、33 個突變重跑與一次停下的等待）。

**狀態：** 完成。entity-line 三批（requirements、contract、split）與缺陷批都完成，等開 PR。

件數の突き合わせ：一致（base `edbd827f` 與 branch 都是 42 個情境；每個情境的 record 數相等，由兩樹 `--run --ticks` 的 json `records` 欄位逐一比對，見 `s6_compare_scale1/`）。

## 做了什麼（AGENTS §3 Architecture Delta (B)：Match 內部 Ownership 重整，七點在 [entity-model](../object_fps_pvp/entity-model.zh-Hant.md) §11）

- S1（`78778d00`）：`EntityId.hpp`（`EntityId`、`PlayerId` 別名、`NoEntity`）、`EntityColumns.hpp`（`FindEntity`／`InsertEntity`／`EraseEntity`／`StrictlyAscending`，升冪 vector 的三個自由函數，不 append、不 swap-and-pop）、4 個單元測試。
- S2（`bee1c4e5`）：`Participant` 的 retiredActionThrough／actions → `ActionLedger` 欄；命令、替補樣本、重設排程、品質 → `Connection` 欄；連線與帳本相關的公開 API 查對應的欄。
- S3（`f676e42e`）：`std::map<PlayerId, Participant> players_` → `bodies_`、`combats_`、`animations_`（`AnimationTrack{id, value, before}`）三欄；`PvpMatch::EraseEntity` 成為唯一刪除入口（Leave 經它）；`Decide`、`Kill`、`AdvanceAnimation` 改簽名；G1 `EntityColumnsStayOrdered`、G2 `EraseEntityRemovesEveryColumn`（friend `PvpMatchTestAccess`）。
- S4（`88c74abb`）：`shapes_`／`actionsCombat_` → 升冪 vector `tickShapes_`（`EntityShapes`）／`actionStartCombats_`（`combats_` 整欄複本）；`EraseEntity` 也清快取。
- S5（`bb7f88d7`）：`RequireEntityOrder` 進 `Snapshot()` 與 `RequireValidSnapshot`；entity-model「實作現況」更新。
- S6：突變 M1～M5；M5 存活→停下報告→使用者決定加 G13 `ActionStartCombatIsTickStart`（記錄型假命中模型）並改用 cpu 檢查（`72f80667`、`f83fc8db`）；Opus xhigh 審查的 must 2、should 5、note 5 全部處置（`a95c5023`）：must-1 跨平台 iterator 錯誤、must-2 補 G7 `TieBreakByEntityId` 與 M6、should-1～4 的 E-4 判定與 Assert、既有 26 個突變的 find 重對應。
- 公開介面（`PvpMatch.hpp` 的 public 區）簽名不變；新增的只有 `RequireEntityOrder`、`PvpMatchTestAccess` 的宣告。不改 wire、Gateway、Engine、Client 專用原始碼、`AuthorityDigest.cpp`、golden。

## 驗收（宣告 8 項，結果出來後填）

1. 兩樹 `compare_authority_trees.py` base `edbd827f`：scale 1 與 10 都「42 scenarios, 0 differing」（每步 S2～S6 各一次 scale 1，S5／S6 加 scale 10；`s2_compare*`～`s6_compare*`）。成立。
2. `authority_digest` CTest 通過；`git diff edbd827f -- fixtures/authority_golden.txt` 為空。成立。
3. `DigestVersion` 4、`DigestFields` 不變；42 個情境兩樹 `--dump` 逐一 cmp 相同（`s5_dump_diff.txt`、`s6_dump_diff.txt`）。成立。
4. `git diff --stat edbd827f -- protocol gateway engine services` 為空；Client 專用原始碼為空；`MatchRuntimeHost`、`IpcHost`、`AuthorityDigest.cpp` 無改動。成立（本批連簽名都沒改到 host）。
5. G1、G2、G7 Green（加 G3、G13）：5243 斷言（`s6_guards.txt`）。突變：本批 M1～M4、M5b（`actionStartCombats_` 只複製一次；由 G13 與 `GYO_ASSERT(actionStart)` 殺）、M6（反向重生迴圈；由 G7 殺）6／6 killed；宣告的 M5「姿勢用動作後的 combat」的可觀測故障就是既有的 `s2-combat-after-actions`（killed）；曾登錄的 `action-start-after-actions`（動作後再複製一次）對本 Tick 無效，是等價突變，已刪並記錄。重對應的既有 26 個全部 killed（`mutations_run_s6_all.txt` 30／33，其中未殺的 3 個：`a2-join-without-value` 與 `s2-stale-shapes` 改 expect 並加 `TickShapes` 的升冪 Assert 後重跑 2／2 killed（`mutations_run_s6_rerun.txt`），第 3 個即刪除的等價突變）。
6. 產品單元測試 13,694,063 斷言通過（`s6_tests.txt`）；L1 cpu|shader 在 PR 開了之後看（含 must-1 的 linux-x64／windows-x64 編譖）。
7. 範圍的限界：效能未量（審查者估無數量級問題：FindEntity 最多 4 個元素、整欄複製比 map 重建便宜）；network 標籤只在本機；split 的 0 差分只有 2 人情境，3 人以上的順序由 G1、G7 的單元測試守；移動迴圈反向只改 TraceMovement 的輸出順序、沒有守衛（審查者指出的缺口，記錄）。
8. 公開介面簽名不變。成立。

## 經緯與停止

- M5 存活：digest runner 的合成命中模型不讀 combat，宣告寫錯了偵測路徑；照 §14.1 停下，使用者決定 `v8-product-match-entity-split-m5-g13`。工具第一次把 M5 判 wrong_failure（整批 cpu 跑時更早的測試先觸發 `GYO_ASSERT(actionStart)`），手動只跑 G13 證明 2 個斷言失敗後把 `GYO_ASSERT failed` 加進 expect。
- 審查 must-1 是本批最重要的發現：macOS 的 libc++ 讓 `std::span` 與 `std::vector` 的 iterator 同型別，單元測試全過，CI 的主戰場 linux-x64（GCC 14）與 windows-x64（MSVC）會編譯失敗。本機以 `-D_LIBCPP_ABI_BOUNDED_ITERATORS` 模擬：舊標頭 7 個錯誤、新標頭通過（`s6_must1_iterator_check.txt`）。真正的 Linux／Windows 建置要在 L1 看。
- 既有 26 個突變的 find 因改寫對不上（`mutation_tool` 守衛 Red），逐一對應到新本文，故障語意不變；全部重跑（D184：find 在改動範圍內的突變要重跑）。
- `s2-stale-shapes` 在 vector 化後存活：舊版 map 累積會留下過期形狀，新版不清就 append、二分搜尋恰好找到新的那份。照 E-8「快取也是欄」在 `TickShapes` 結尾 Assert 嚴格升冪（`68be4730`），突變改由 Assert 殺。
- 實作與契約的一處差異：entity-model §3.5 原寫「以 FindEntity 取 combats_、animations_」，實作用同索引讀並 Assert（三欄恰等長、同進同出）；契約文字已改成和實作一致，並寫明理由。

## 證據

主 checkout `build/evidence/v8-product-match-entity-split/`（Intel Mac，git 管理外）：`base_list.txt`、`s1_*`～`s6_*`（測試、ctest、兩樹比對的 json 與輸出、42 個 dump）、`s6_must1_iterator_check.txt`、`s6_m5_manual_g13.txt`、`mutations_run*.txt` 與報告 json。

## 下一批

entity-line 三批完成後開 PR（三批同一個 PR）；L1 要看 linux-x64／windows-x64 的編譯與 H6 類的四平台 digest。之後 rewind-line 的姿勢歷史照 entity-model §10 以欄的形式接。

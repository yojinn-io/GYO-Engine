# Engine 架構：v8-arch-entity-store-delta（實體容器的 Architecture Delta 與 Engine 契約）

日期：2026-11-05。分支 `claude/v8-arch-entity-store`。主會話 Fable 5.1；實作者子 agent Opus 5.5／high（general-purpose）；審查者子 agent Opus 5.5／high（general-purpose，唯讀；與實作者同模型，照任務卡）。估時 2.5～3 h，實際約 1.5 h（實作者約 16 分、審查者約 35 分，其餘為主會話的偵察、核對與文件）。

**狀態：batch 2 完成，停下等使用者核准（容器版或更多、Design Invariants 與 Replacement Criteria、batch 3 的拆法與時機等）。** 不寫程式。上段任務。計畫見 [README](../architecture/plans/entity-store/README.md)，經過見 [HANDOFF](../architecture/plans/entity-store/HANDOFF.md)。

## 做了什麼

- 新文件 [entity-store.md](../architecture/entity-store.md)（英文，225 行）：Engine 契約，照 os.md／trace.md 體例。`Design Invariants` 10 條、`Replacement Criteria` 15 條，每條附 survey 與產品契約的根據，但不引產品檔案路徑（可刪除性，AGENTS §7）。
- 新文件 [delta.zh-Hant.md](../architecture/plans/entity-store/delta.zh-Hant.md)（237 行）：AGENTS §3 七點（壓力逐項標觀測／預測）、搬移 vs 新建＋改裝、容器版與「更多」（B1～B7）、E-1～E-12／RC-1～RC-5 的歸屬表、給圖書管理員的 `docs/architecture.md` 修改稿（第 129 行、模組表列、「Architecture changes」段）、產品契約要跟著改的地方、使用者要決定的事、batch 3 估時。
- 建議：新建 `GYO::EntityStore`（header-only 葉，只連 `GYO::Base`），pvp 改裝而非搬檔（現行 `EntityColumns.hpp`／`EntityId.hpp` 含 `HasPlayerId`、`PlayerId`、`NoEntity` 的產品知識）；容器版；batch 3 拆 3a（欄，現在做）／3b（`DeferredChanges`，與第一個非玩家實體批同時）。
- 誠實的壓力分類：消費端只有 pvp 一個；獨立於使用者決定的觀測事實只有「容器內容已與產品無關、位置卻在產品」，其餘（第一個非玩家實體、pv10 分送、第二個產品）是預測。

## 判定

| 判定 | 結果 |
|---|---|
| 審查 | must 1／should 12／note 11，全部處置（delta「審查的處置」節）；總評「修正後可交核准、不需重做」，設計方向無反對根據 |
| 現況核對 | 審查者以 grep 核對 CMake 目標、測試、mutation 條目、architecture.md 行號；錯的 3 處（33→30 行、MatchRuntimeHost.cpp:760 非欄函數、mutation 條目起行）已改 |
| 語言 | entity-store.md 英文合 Engine 文件體例，標題 `Design Invariants` 正確；delta 正體中文、無簡體與假名 |
| 建置／測試 | 不適用（只有文件） |

## 經過

- 實作者 10 分鐘完成，遠低於估時：survey §4.1／§4.2／§5 與 entity-model 已含大部分根據。
- 審查的 must 是 C++ 規則：「所有者的 `EntityOf` 多載優先、否則成員 `id`」以兩個獨立 concept 的同名多載會 ambiguous，且 ADL 看不到 using-directive（產品的 Legacy 測試在匿名命名空間）。契約改成所有者提供的 hook，Engine 的 `EntityOf` 分派。這在 batch 3 編譯前就被抓到。
- 審查指出 Engine 契約引用產品檔案與行號會在產品刪除時留下 Invalid Reference（既有 Engine 契約產品路徑 0 件），已把產品細節移到 delta、Engine 文件只留一條 plan 連結。
- 實作者修正時自行在契約定了「callback 擲出時清空待辦並還原執行中狀態」（N6），已在報告中提醒使用者可改列為決定。
- 審查指出 survey 留給 batch 2 的選擇（型別分離 vs 執行期 Assert）被默默採了 Assert，以及 Apply「生成先」的取捨（同一同步點刪舊 id 再以同 id 重生不成立）沒寫；兩者改成 §7 的使用者決定，附代案。
- 產品已核准的 DI（E-5 的 guard 欄、E-12 的字面「Tick 內」）要跟著變，是產品 Design Invariants 的變更，AGENTS §3 要使用者同意，已列入 §6 與 §7。

## 證據

- 主 checkout `build/evidence/entity-store/`（git 管理外）：`delta-implementer-report-20261105.md`、`delta-review-20261105.md`。

## 給使用者決定的事

見 delta §7。核准後：通知圖書管理員會話本線的分支與 Delta（architecture.md 的修改由它在同一 PR 做）；batch 3 開始時先事前宣告受理判定（42 情境 `--scale 1`／`10` 差分 0、golden 不變、mutants 由 common 測試殺）並取得承認。

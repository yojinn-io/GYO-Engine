# 合併前核准：Extract 的來源被替換（`v8-arch-gyo2-sdl-r1b`，SDL 線子批 R1b）

日期：2026-11-25。分支 `claude/gyo2-sdl-r1b-extract-replace-8886f8`（PR #334，base `gyo2`）。事前宣告：[sdl-extract-replace-declaration.zh-Hant.md](sdl-extract-replace-declaration.zh-Hant.md)（修訂 1；使用者 2026-11-25 動工前核准「選 B，開始」）。

**請你核准的**：第 2 節（條文的最終文字，每條「改前／改後／白話」）、第 3 節（和宣告不同的地方）、第 4 節（新的公開介面）。核准後各頁的〔`v8-arch-gyo2-sdl-r1b`，草案〕標記改成定稿。

## 1. 判定的結果

| # | 結果 | 證據 |
|---|---|---|
| 1 | 本機 macOS x86_64（Apple clang，preset `test`）merge gyo2 76311f5b 之後 `ctest -L "cpu|gpu"` 253/254；`cmake.PluginSelection` 是本機離線設定（`FETCHCONTENT_FULLY_DISCONNECTED`），gyo2 基準上同樣失敗 | `build/local-cache/ctest5.log`（工作樹內，不進 git） |
| 2 | Kernel「DI-KS 38: …」5 案、編排層「extract: replacing the source: …」7 案全過；R1a 的三個「替換來源是斷言」案拿掉 | `tests/common/kernel/ExtractTests.cpp`、`tests/common/orchestration/ExtractTests.cpp` |
| 3 | 相關突變：Kernel 11/11（新 `ks-rx-` 4＋本批改到 find 的 `ks-s2-`／`ks-s3-` 7）、編排層 13/13（新 `or-rx-` 5＋既有 `or-ex-` 3＋本批改到 find 的 5）killed；拿掉 `or-ex-replace-source-allowed` | dev_log「驗證」 |
| 4 | 宣告的設計審查（Opus／high，同模型）B1 S5 N4 全採用；實作的設計審查（Opus／high，同模型）B1 S2 N4，N-1 以外採用 | 主 checkout `build/evidence/gyo2-sdl-r1b/declaration-r0-review.md`、`impl-r0-review.md` |
| 5 | L1 三平台全綠（0d007573，merge gyo2 76311f5b 之後；linux-x64、windows-x64、macos-arm64 與 CI gate）；之後的 commit 只改文件 | PR #334，run 45545924167 |

所有「同模型」：Fable 暫停期間一律 Opus。

## 2. 條文的最終文字

### 2.1 Kernel DI-KS 13（第二段）

- **改前**：……`Extract<Write<…>>` 不存在（編譯失敗測試）。補守衛：D-E14 的案；「DI-KS 13: an extract source registers again …」（`RegistrationRoundTests.cpp`）。
- **改後**：……`Extract<Write<…>>` 不存在（編譯失敗測試）。讀者的 Registry 可以在之後換來源（DI-KS 38），新來源同樣要建好過 Plan。補守衛：D-E14 的案（含「DI-KS 38: a new source without Plans is D-E14 …」）；「DI-KS 13: an extract source registers again …」（`RegistrationRoundTests.cpp`）。
- **白話**：渲染 World 原本一輩子只讀一個來源，現在可以換讀另一個；唯讀不變，新來源一樣要先建好。

### 2.2 Kernel DI-KS 26（兩處）

(1) 「之後一輪只准……配發器是 R-E7（這一輪丟掉、World 照跑）。」之後
- **改前**：（無）
- **改後**：換 extract 來源（`ReplaceExtractSource`，DI-KS 38）也是之後一輪的一種，照樣受 `dynamicInsert` 擋；之後一輪可以先 `StagePlans` 建好、留著不換進，再 `CommitPlans` 換進（只搬移），或 `DiscardRegistrations` 丟掉——`BuildPlans` 就是兩者連著做。
- **白話**：換來源走既有的「之後一輪」；宣告不准動態插入的 World 也不准換。另外把「建好」和「換進」拆開，讓編排層可以幾個讀者一起成功或一起放棄。

(2) 斷言那一句
- **改前**：……有暫存副本時建 World；用這份 Registry 建的 World 在 `WorldExecutor::Run`／`RunExtract` 裡時呼叫那三個——它的 System、hook、同步點上的初始值函式、它跑的別的 World 都算；
- **改後**：……有暫存副本時建 World；有暫存的一輪時呼叫 `Register`／`ReplaceExtractSource`／`BuildPlans`／`StagePlans`；用這份 Registry 建的 World 在 `WorldExecutor::Run`／`RunExtract` 裡時呼叫那三個或 `ReplaceExtractSource`／`StagePlans`／`CommitPlans`——它的 System、hook、同步點上的初始值函式、它跑的別的 World 都算；
- **白話**：新的三個函式和舊的一樣，不能在 World 跑到一半時呼叫；建好還沒換進的時候不能再開新的一輪。

### 2.3 Kernel DI-KS 37（呼叫的人的規則）

- **改前**：讀者的 Registry 在登記、建 Plan 與 `RunExtract` 期間，來源的 Registry 要活著；讀者的 Registry 不比來源的晚銷毀；
- **改後**：讀者的 Registry 在登記、建 Plan 與 `RunExtract` 期間，它**目前的**來源的 Registry 要活著；換進新來源（DI-KS 38）之後舊來源才可以走；除此之外讀者的 Registry 不比來源的晚銷毀（`DropOldFirst` 的替換期間來源先走，期間不經舊來源的指標存取讀者——換來源只覆寫指標的值，O-8）；
- **白話**：「來源要活著」改成指「現在讀的那個」；舊來源要等讀者換好才能銷毀。先丟舊的那種替換是唯一例外，那段時間不去碰讀者。

### 2.4 Kernel DI-KS 38（新增）

- **改前**：（無）
- **改後**：**讀者的 Registry 可以換 extract 來源。** `Registry::ReplaceExtractSource(source)` 在第一次 `BuildPlans` 成功之後開一個之後一輪，那一輪的 extract 來源是 `source`（之前用 `SetExtractSource`）；這一輪建起來時（`BuildPlans`，或 `StagePlans` 再 `CommitPlans`）Extract 參數重新對 `source` 解析：`source` 沒建過 Plan 是 D-E14，沒登記讀者讀的型別是 R-E4，失敗時這一輪丟掉、Registry 與 World 留在舊來源與舊版本。成功時版本加一，World 下一次被用到時接上（DI-KS 26：保留 `lastRun` 與 `Local`），之後 `RunExtract` 只收由新來源建的 World。有 World 宣告 `dynamicInsert = false` 時回 `DynamicInsertRefused`（同 `Register`），什麼都不變。`StagePlans` 把之後一輪建好、留著不換進（World 照讀舊來源；失敗或丟例外時這一輪丟掉），`CommitPlans` 只搬移、不丟例外（失敗的斷言除外；測試裡斷言以例外實作，所以不標 `noexcept`），讓呼叫的人把幾份 Registry 的一輪全部建好才一起換進（編排層替換有多個讀者的來源，O-8）。不加錯誤碼。斷言：第一次 `BuildPlans` 之前、`source` 就是自己、有暫存的一輪時再開一輪或再建、沒有暫存時 `CommitPlans`，以及 `Register` 的那一組。Kernel 不發「來源換了」的通知（決定換的是呼叫的人；編排層的通知在 O-12）。守衛與系譜見 Kernel 頁。
- **白話**：這是 (B) 的 Kernel 原語。換不成（新來源缺型別、沒建好、World 不准動態插入）就整個不變；換成了，渲染 World 的 System 狀態都留著，只是改讀新的來源。

### 2.5 Kernel RC 29（新增）

- **改前**：（無）
- **改後**：**換來源要同時讀兩個來源，或通知要搬進 Kernel**：(a) 某個點名的 Feature 需要在換來源時同時讀到舊來源與新來源（兩個來源的比對）；(b) 兩個以上直接用 Kernel 的 host 各自寫「來源換了」的通知——重評 DI-KS 38 與把通知搬進 Kernel。
- **白話**：出現這兩種需求時，回頭重看這批的設計。

### 2.6 編排層 O-8（末句與守衛）

- **改前**：Extract 的來源被替換後，讀它的 World 不得再讀舊來源（O-12）；R1b 定做法之前，替換有讀者的來源（鏈的中間那個也算）是斷言，在丟掉任何東西之前（`v8-arch-gyo2-sdl-extract`）。……守衛：`orchestration.O-8` 各案、讀檔案例、scenario 第一段。
- **改後**：Extract 的來源被替換後，讀它的 World 不得再讀舊來源（O-12）。替換一個 World 時，直接讀它的 World 留著，在換上的同一步改讀新的 Registry（Kernel DI-KS 38）：新 World 就緒（`BuildNewFirst` 在 `beforeSwap` 成功之後、銷毀舊的之前；`DropOldFirst` 在新的就緒後）時對每個直接讀者 `ReplaceExtractSource`＋`StagePlans`，全部成功才在換上的那一步一起 `CommitPlans`；任一失敗（含讀者宣告 `dynamicInsert = false` 的拒絕）或丟例外就丟掉所有暫存，`BuildNewFirst` 回 E2 `ReplaceRegistrationFailed`、舊的照跑，`DropOldFirst` 的 Product 結束（O-11）。暫存全部成功後、換上之前，每個讀者的 `ExtractSourceSwaps` 加一（O-12；讀者那時還在原來的版本，不配置）。讀者的讀者不動（它的來源沒換）；讀者要跟著重建，作者在之後自己替換它。讀者不會被借出（`Lend` 拒絕有 Extract 關係的 World，O-13）；O-13 放寬這一點時重看本條的步驟（讀者的 Registry 屬借走它的執行緒）。……守衛：`orchestration.O-8` 各案、讀檔案例、scenario 第一段；來源的替換：`orchestration.extract`「replacing the source: …」各案與突變 `or-rx-`。
- **白話**：讀檔或重連換掉模擬 World 時，渲染 World 留著改讀新的；幾個讀者要嘛一起換成，要嘛都不換。換不成時，「先建新的」那種照舊跑舊的，「先丟舊的」那種 Product 結束（和其他替換失敗一樣）。

### 2.7 編排層 O-12（Extract 那一項，加一句）

- **改前**：（無）
- **改後**：編排層在每個有 `extractFrom` 的 World 登記 `ExtractSourceSwaps`（建好時 `count` 是 0），替換來源時加一（O-8）；讀者以來源實體為鍵、跨幀留著的東西，由擁有它的 System 讀 `Res<ExtractSourceSwaps>::Changed()` 整批丟掉（用到它的每個 System 各自看，或擁有者在最早用到它的那張 schedule 清掉）；編排層只發通知、不認得那些東西。
- **白話**：新 World 的實體 7 可能是別的東西，所以渲染 World 記著「實體 7 的網格」這類東西時要聽這個通知丟掉。編排層只告訴它「來源換了」，不替它決定丟什麼。

### 2.8 編排層 O-15（誰比誰活得久）

- **改前**：……讀者的 Registry 也先於來源的 Registry，`Build` 失敗時還沒成 World 的登記也照反序丟，DI-KS 37）。
- **改後**：……讀者的 Registry 也先於來源的 Registry，`Build` 失敗時還沒成 World 的登記也照反序丟，DI-KS 37；例外：`DropOldFirst` 替換來源的期間與失敗之後舊來源先走，期間不經舊來源的指標存取讀者、失敗之後讀者不再被用到）。
- **白話**：和 DI-KS 37 同一個例外，在編排層這邊也寫明。

### 2.9 編排層 RC-O13（改寫）

- **改前**：Extract 的來源替換（O-8）讓讀者的重建成為替換時間的主要部分，或讀者有不能重建的狀態（和 RC-O9 第二句一起看）。
- **改後**：換來源（O-8）之後讀者有以舊來源為鍵、經 `ExtractSourceSwaps` 也丟不乾淨的狀態，或讀者換來源的那一輪成為替換時間的主要部分（和 RC-O9 第二句一起看）。
- **白話**：讀者不再重建，所以要盯的風險改成「通知丟不乾淨」和「換來源本身變慢」。

## 3. 和宣告不同的地方

1. 宣告第 3.2 節寫「stage 一次做開輪與建」，實作拆成 `ReplaceExtractSource`（開輪＋改來源）與 `StagePlans`（建）兩步：沿用 `Register`、`BuildPlans` 既有的兩種錯誤型別，不新增錯誤碼。實作審查判定合理（比原寫法正交）。
2. 通知的時點：宣告修訂 1 寫「換進之後加一」，實作審查 S-1 指出那時讀者 World 接新版本會配置、可能丟例外，改成「暫存全部成功後、換進之前」。宣告已同步。
3. `CommitPlans` 不標 `noexcept`（宣告寫「不丟例外」）：測試裡的斷言以例外實作，標了會讓斷言案 terminate；條文寫明。

## 4. 新的公開介面

- Kernel：`Registry::ReplaceExtractSource(const Registry&) -> Result<void, KernelError>`、`Registry::StagePlans() -> Result<void, StartupReport>`、`Registry::CommitPlans()`。沒有新的錯誤碼。
- 編排層：`engine/orchestration/ExtractSourceSwaps.hpp` 的 `ExtractSourceSwaps { std::uint64_t count; }`（`Orchestration.hpp` 會 include）。沒有新的 `OrchestrationErrorCode`（讀者換不成是既有的 `ReplaceRegistrationFailed`）。

## 5. 留給之後

- 以來源實體為鍵的保留式對應（R6）是通知的第一個使用者；做的時候把「聽 `ExtractSourceSwaps`」寫進那個 Module 的契約。
- 另一種通知（Kernel 給 World 唯一 id，像 Bevy 的 `WorldId`，由 Extract 參數自己偵測）：Kernel 要加概念，目前不採（RC 29 (b) 時重評）。
- PvP 在同一個 Product 換前台 World 還需要 net 的連線目標每次替換時給（主會話已記錄「接続先を client World ごとに渡す」）。
- gyo2 上原本就 find 不到的 15 個突變（列在 dev_log），留給整理突變的卡。

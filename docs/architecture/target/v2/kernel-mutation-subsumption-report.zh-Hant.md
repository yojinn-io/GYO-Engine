# Kernel 突變清單的 subsumption 檢查（`v8-arch-gyo2-mutation-prune` 第 3 步）

日期：2026-11-19。分支 `claude/gyo2-mutation-prune-19c247`（清單是第 2 步之後的 714 個，`801e2d64`；PR #246）。
本頁只是候選表：**這一步不改清單、測試、守衛**。使用者核准之後才改（另一個變更）。

**核准與結果（2026-11-19）**：使用者「照你建議的做」，第 3 節的 51 個已合併進 39 個接手，清單 714 → **663**；其餘照建議留（第 8 節）。改動與驗證在 [dev_log](../../../dev_logs/2026_11_19_gyo2_mutation_prune.zh-Hant.md)。
用詞照第 1 步的報告（[kernel-mutation-prune-report.zh-Hant.md](kernel-mutation-prune-report.zh-Hant.md)）：**緩的面**＝退役之後，以前會被某個突變發現、之後不會被任何突變發現的情形；**接手**＝之後由哪個突變證明。

## 0. 結論（先看這裡）

- 714 個突變每個都不篩案例、跑全部 Kernel 測試，記下讓哪些斷言失敗。
  - **最小集合 469 個**：每個都抓得到別人抓不到的測試弱化，留。
  - **候選 245 個**：失敗集合包含某個留下的突變的集合。
- 照「X 的 expect 指的那句斷言，能不能由留下的突變在**同一個位置**證明」分三類：A 56、B 19（可以併進去）、C 170（最小集合裡沒有突變在同一位置證明它）。
- **建議退役 51 個**（第 3 節）：A、B 裡不推翻已定決定、試跑合併殺得掉、失敗的值和接手沒有實質差別的。做法照第 1 步的合併，刪除型的弱化沒有緩的面（第 3 節開頭）。714 → **663**。
- **要你決定 17 個**（第 3b 節）：位置相同、但 X 與接手錯成的值不同。退了，斷言被改鬆時會少一個發現的突變。我建議留；如果照退，714 → 646。
- **要你另外決定 7 個**（第 4 節）：第 1 步第 5 節「留」的守衛突變 6 個，加上 `bs-thread-serial-is-tls-address`（S10 事故的重現突變）。我建議都留。
- **C 170 個建議留**（第 5 節）。
- **敵對式檢查（Fable 5.2／high）推翻了初稿**（第 9 節）。初稿建議退 96 個，其中 31 個有問題：
  - 23 個：A/B 判定只看 expect 的文字有沒有出現在接手的輸出裡，沒看對上的是不是同一句斷言。13 個退了之後，那句斷言就沒有任何突變的 expect 指到。
  - 7 個：失敗集合的鍵沒分 SUBCASE，包含的方向看反了。
  - 1 個：上面那個 S10 重現突變。
  - 本頁是照檢查結果重算後的版本：判定改成看位置，鍵加上 SUBCASE。
  - 窄複查確認重算後的 68 個位置條件全部成立，另外指出 17 個的值不同（第 3b 節）。
- 草案方向的對照（第 6 節）：照第 1 步第 7 節第 7 題草案的方向，會有 276 個候選，其中 188 個屬於最小集合。使用者 11-19 決定照一般定義的方向。

## 1. 怎麼做的

**跑法**（`run.py`）：714 個突變逐個套用、重建，然後**不篩 `cases`**，跑整個測試執行檔（`gyo_kernel_tests`、`gyo_base_tests`、執行緒探針），或跑突變自己的指令（守衛的自測、符號表、匯出檢查）。
- GYO_ASSERT 讓行程中止時，把中止的那個案例排除再跑，直到跑完，所以中止之後的案例也記得到。56 個突變要多跑幾輪，最多 29 輪。
- 每輪逾時 60 秒，只有 `hf-round-count-kept` 逾時（第 7 節）。
- 2026-11-19 01:58～04:07，約 130 分，macOS arm64。跑完原始碼已還原。

**失敗集合的鍵**（`parse.py`；doctest 的部分以 `analyze2.py` 的 `keys2` 為準）：
- doctest：每句失敗的斷言記成「案例名｜SUBCASE 路徑｜檔案:行號｜種類」（ERROR、FATAL ERROR、CRASHED、THREW）。中止另外記一筆「ABORT｜案例｜SUBCASE｜GYO_ASSERT 的式子」；沒有 GYO_ASSERT 訊息的中止記訊號名（例如 SIGSEGV）。
- unittest（守衛的自測）：失敗的測試名，含 subTest 的參數。
- 其他指令與探針：輸出裡比沒改的程式多出來的行（基準在 `baseline/`，數字換成 `#`）。
- 預期編譯失敗的突變：錯誤訊息的行。

**比較的方向**（使用者 2026-11-19「照一般定義的方向做」；第 1 步報告第 7 節第 7 題）：只在同一種執行對象之間比。F(Y) 非空、而且 F(Y) ⊆ F(X) 時，X 是候選；集合相同的，留清單裡排在前面的那個。
- 理由：凡是**刪掉** F(X) 裡斷言、讓 X 活下來的測試弱化，一定也刪掉了 F(Y) 的全部，Y 也會活下來而被發現。這個關係只對「刪掉」成立；「改鬆」（例如讓一句斷言只在更極端的情況才失敗）不一定（第 7 節）。
- 留下的叫**最小集合**。

**A／B／C**：這份清單「殺掉」的判準是 expect 要對上指定的那一句（AGENTS §14.2）。只照集合退 X，X 的 expect 指的那句斷言可能就沒有突變證明了。所以每個候選再看一次（`analyze2.py`）：
- **判定的條件（位置）**：把 X 的 expect 拆成各句。每一句在接手 V 的原始輸出裡（只看合併後的 `cases` 範圍：V 的加上 X 的）對上的位置，必須符合三點：
  - 非空；
  - 不是只對到每次都會印出的案例標題（有 8 個案例無條件印 MESSAGE，標題永遠出現）；
  - 是 X 自己（在 X 的 `cases` 範圍）對上的位置的子集。
- 位置是（案例，檔:行），中止則是（ABORT，案例）。守衛的自測與預期編譯失敗只比文字（第 7 節）。
- **A**：包含它的、留下的 Y 就滿足位置條件。X 的 `cases` 與 expect 併進 Y。
- **B**：包含它的突變都不滿足，但另一個留下的 Z 滿足。改法由 Y 涵蓋，那句斷言由 Z 在同一位置證明，X 併進 Z。
- **C**：最小集合裡沒有突變滿足位置條件。

**合併試跑**（`merge_trial.py`）：用臨時清單（正式清單不動），把建議的 51 個併進接手（位置條件成立的 68 個也另跑了一次），再用 `run_mutations.py` 跑收到合併的接手（照 `cases` 篩，和平常一樣）。合併時：
- 每句 expect 包成 `(?:…)`，避免頂層的 `|` 跨到別句。
- 相同的句子只寫一次。

## 2. 數字

| | 數 |
|---|---|
| 突變 | 714（全部跑完，沒有跳過或對不上） |
| 失敗集合大小的中位數／最大 | 2／95 |
| 最小集合（留） | 469 |
| 候選 | 245（集合相同的 30、真包含的 215） |
| A：併進包含它的突變 | 56 |
| B：併進另一個在同一位置證明的突變 | 19 |
| C：最小集合裡沒有突變在同一位置證明 | 170 |
| 要你另外決定（第 4 節） | 7（守衛 6、S10 重現 1） |
| 位置條件成立、不推翻已定決定 | 68（類別：A 51 個、B 17 個）；收到合併的接手 51 個，試跑 51/51 |
| 其中失敗的值不同、要你決定（第 3b 節） | 17 |
| **建議退役（第 3 節）** | **51**；收到合併的接手 39 個，試跑 39/39 |
| 退役後 | 663（第 3b 節也退則 646） |

失敗集合大小的中位數是 2：Kernel 的測試很窄，大多數突變只讓一兩句斷言失敗。所以候選大多不是「大突變吞掉小突變」，而是同一句斷言被好幾個改法打到。

依執行對象：

| 執行對象 | 最小集合 | A | B | C |
|---|---|---|---|---|
| `gyo_kernel_tests`（doctest） | 376 | 47 | 17 | 153 |
| 守衛的自測（unittest） | 52 | 4 | 2 | 13 |
| 其他指令（符號表、匯出檢查、守衛本體） | 20 | 0 | 0 | 0 |
| 預期編譯失敗（try_compile 樣本） | 14 | 3 | 0 | 1 |
| `gyo_base_tests`（`bs-`） | 6 | 2 | 0 | 3 |
| 執行緒探針 | 1 | 0 | 0 | 0 |

位置條件成立的 68 個：`gyo_kernel_tests` 64、預期編譯失敗 3、`gyo_base_tests` 1。其中 16 個和接手的鍵（案例｜SUBCASE｜檔:行）完全相同；輸出內容也逐字相同的只有 8 個 doctest 加 3 個預期編譯失敗（其餘 4 個值不同、1 個次數不同，`ks-s10-check-ge` 是接手較難殺，無損）。守衛的 A、B 都在第 4 節。

## 3. 建議退役：A、B 裡不推翻已定決定、試跑殺得掉、失敗的值和接手沒有實質差別的（51 個）

每個都是「併進接手」：接手的 `cases` 加上 X 的案例，expect 加上 X 的那句（每句都要出現）。

- **緩的面**：刪除型的弱化沒有。X 那句斷言被刪掉時，接手在同一位置失敗的那句也被刪掉，接手活下來而被發現。
- **締める面**：接手以後要每句都失敗才算殺掉。
- 改鬆型的弱化見第 7 節。
- 其中有些合併對接手沒有任何實質改動：expect 字串相同、X 的 `cases` 已在接手裡（全部 68 個裡有 33 對，其中 10 個在第 3b 節）。對這些，「expect 加上 X 的那句」不成立，試跑殺掉也沒有新資訊，退役等於直接刪掉 X；正式合併時只剩把 X 的 `what` 寫進接手。刪除型沒有緩的面的理由不變：接手在同一位置失敗。

51 個（「關係」：集合相同＝失敗集合的鍵（案例｜SUBCASE｜檔:行）完全一樣，失敗的值不一定相同；被它包含＝接手的失敗集合是 X 的真子集；B＝改法由前者涵蓋，X 那句由併進的那個在同一位置證明）。

| id | 類 | 併進 | 關係 | 試跑（併進的那個） |
|---|---|---|---|---|
| `rs-j3-4-z1-removed` | B | `rs-j12-condition-reads-dropped` | 被 `rs-iv-fx16-one-declaration-per-request` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `rs-j3-5-changed-allowed` | A | `rs-adv-condition-change-filter-dropped` | 被它包含 | killed |
| `rs-j12-write-term-dropped` | A | `rs-j11-self-conflict-off` | 集合相同 | killed |
| `rs-j24-de16-removed` | A | `rs-adv-derived-deferred-writer-counts` | 被它包含 | killed |
| `rs-adv-assert-self-conflict-removed` | A | `rs-j11-self-conflict-off` | 集合相同 | killed |
| `rs-adv-assert-condition-write-removed` | A | `rs-j13-condition-write-allowed` | 集合相同 | killed |
| `rs-b5-literal-counts-deferred` | A | `rs-b5-literal-counts-membership` | 被它包含 | killed |
| `rs-b3-trim-ignores-declared` | A | `rs-b3-trim-not-pinned` | 被它包含 | killed |
| `rs-iv-ff3-C-section-dropped` | A | `rs-iv-fb8-C-request-not-hashed` | 被它包含 | killed |
| `rs-iv-fn1-funnel-empty` | A | `rs-iv-fn4-system-Z6-without-funnel` | 被它包含 | killed |
| `rs-iv-fn2-funnel-not-transitive` | A | `rs-iv-fx6-note-dropped-Z3-site` | 被它包含 | killed |
| `rs-iv-fn3-system-zone-without-funnel` | A | `rs-iv-fx6-note-dropped-Z3-site` | 被它包含 | killed |
| `rs-iv-fn7-zone-access-unchanged` | A | `rs-iv-fn4-system-Z6-without-funnel` | 被它包含 | killed |
| `rs-iv-fn8-funnel-note-empty` | A | `rs-iv-fx6-note-dropped-Z3-site` | 被它包含 | killed |
| `rs-iv-fx15-funnel-zone-B-systems-only` | A | `rs-iv-fn4-system-Z6-without-funnel` | 被它包含 | killed |
| `rs-iv-fd4-resolver-insert-not-recorded` | A | `rs-iv-fn4-system-Z6-without-funnel` | 被它包含 | killed |
| `rs-iv-fd5-commandsfor-inserts-as-writes` | A | `rs-iv-fx14-funnel-zone-C-systems-only` | 被它包含 | killed |
| `hf-no-rounds` | B | `hf-verification-only` | 被 `hf-rounds-follow-trim` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `hf-limit-off-by-one` | A | `hf-default-limit-nine` | 被它包含 | killed |
| `hf-schedule-limit-ignored` | B | `hf-limit-check-before-empty` | 被 `hf-limit-larger-of-two` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `hf-only-with-trim` | A | `hf-startup-skipped` | 被它包含 | killed |
| `hf-first-queue` | B | `hf-verification-only` | 被 `hf-rounds-follow-trim` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `hf-limit-no-names` | A | `hf-limit-names-not-unique` | 被它包含 | killed |
| `we-j30-stamp-after-call` | B | `we-adv-tickstamp-never-recorded` | 被 `we-j30-skip-no-stamp` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `we-j1-insert-append` | A | `we-j1-erase-swap-pop` | 被它包含 | killed |
| `we-j2-alive-append` | A | `we-j1-erase-swap-pop` | 被它包含 | killed |
| `we-j7-column-dtor-descending` | A | `we-j7-world-dtor-reverse` | 集合相同 | killed |
| `we-j5-layer1-off` | A | `we-j5-id-not-mode` | 被它包含 | killed |
| `we-j3-16-violations-dropped` | A | `we-j5-id-not-mode` | 被它包含 | killed |
| `hf-round-skips-despawn` | A | `hf-round-skips-onremove` | 被它包含 | killed |
| `sr-dropped-onadd-only` | B | `sr-dropped-cells-added` | 被 `ks-s5-spawn-alive-id-returned` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `sr-range-removed` | B | `sr-dropped-made-alive` | 被 `sr-range-recorded-first` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `sr-emitter-by-planpos` | A | `sr-emitter-by-name` | 被它包含 | killed |
| `sr-tick-cached-for-world` | A | `sr-tick-one` | 集合相同 | killed |
| `sr-discard-clears-records` | A | `we-j35-diagnostics-dropped` | 集合相同 | killed |
| `ns-entry-violation-enters` | A | `ns-entry-retake-only` | 集合相同 | killed |
| `ns-left-error-not-listed` | B | `we-adv-dropped-err-beside-own-err-lost` | 被 `ns-self-outermost-only` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `ns-handle-epoch-not-renewed` | B | `ns-entry-no-compare-no-retake` | 被 `ns-handle-not-renewed-on-throw` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `ns-stamp-not-renewed` | A | `ns-modify-no-changed` | 被它包含 | killed |
| `ks-s10-bind-skipped` | A | `ks-s10-handles-unchecked` | 被它包含 | killed |
| `ks-s2-refusal-leaves-pending` | B | `ks-s2-empty-build-bumps` | 被 `ks-s2-re7-old-hook` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `ks-s2-own-world-unchecked` | A | `ks-s2-register-while-running` | 被它包含 | killed |
| `ks-s6-set-not-recorded` | A | `ks-s6-plan-order-reversed` | 被它包含 | killed |
| `ks-s5-interrupted-still-changes` | B | `ks-s5-closure-refusal-not-interrupting` | 被 `ks-s5-throw-not-interrupting` 包含；X 那句在併進的那個裡、同一位置失敗 | killed |
| `ks-s4-layer2-events-declared` | A | `ns-entry-retake-only` | 被它包含 | killed |
| `ks-s7-drop-every-run` | A | `we-adv-ready-window-compare-off` | 集合相同 | killed |
| `ks-s10-one-token-for-all-threads` | A | `ks-s10-handles-unchecked` | 被它包含 | killed |
| `bs-thread-serial-not-thread-local` | A | `bs-thread-serial-issuer-per-thread` | 集合相同 | killed |
| `ks-s10-check-ge` | A | `ks-s10-handles-unchecked` | 集合相同 | killed |
| `ns-rebase-no-retake-names-ping` | A | `ns-rebase-no-retake-on-throw` | 集合相同 | killed |
| `ks-s5-caller-error-recorded` | A | `ks-s5-spawn-alive-ends-run` | 集合相同 | killed |

## 3b. 要你決定的 17 個：位置相同、失敗的值不同

窄複查（第 9 節）指出：位置條件成立的 68 個裡，有 27 個是 X 與接手在同一行失敗、但失敗的值不同。其中 17 個（檢查者讀值後的判讀），X 與接手打壞的是同一句比對裡的不同部分，或者 X 是邊界情形。
- 刪除型的弱化：沒有緩的面（同一行被刪，接手也活）。
- 改鬆型的弱化：**有**。例如斷言被改成只查另一半，或邊界從 `>` 放寬成 `>=`：以前 X 會活下來而被發現，之後接手仍被殺。和第 4 節的 `bs-thread-serial-is-tls-address` 同一類，只是差在「值」而不是「次數」。
- 合併救不了：接手產生不出 X 的值。檢查者另查過，留下的 646 個裡沒有別的突變在那一行以 X 的錯法失敗、而且 expect 指到它（`checker/recheck/c8.out`）。
- `we-j36-*` 三個彼此的值也不同，至少留一個，early@2 那段才有突變證明。

| X | 接手 | 位置 | X 的值｜接手的值 |
|---|---|---|---|
| `ns-stamp-reused` | `ns-modify-no-changed` | `NestedRunTests.cpp:603` | `7 > 7`｜`1 > 7`（X 是邊界；`>` 放寬成 `>=` 只有 X 活） |
| `hf-tostring-de20` | `hf-schedule-zero-allowed` | `HookFlushTests.cpp:953` | 報成 D-E19｜完全沒報錯 |
| `ks-s5-spawn-alive-other-code` | `ks-s5-spawn-alive-id-returned` | `ImmediateChangeTests.cpp:660` | refused AllotterExhausted｜spawned 100 |
| `ks-s5-round-limit-off-by-one` | `hf-tostring-hookroundlimit` | `ImmediateChangeTests.cpp:625` | 多跑一輪｜結尾變 StructureBroken |
| `we-adv-host-write-current-stamp` | `we-adv-added-is-changed` | `ExecutorTests.cpp:488` | `added:0 added:0`｜`added:1 added:1`（期望 `added:1 added:0`） |
| `we-j32-despawn-record-order` | `we-j32-onadd-record-order` | `WorldTests.cpp:529` | remove 的順序錯｜add 的順序錯 |
| `hf-round-skips-remove` | `hf-round-skips-onremove` | `HookFlushTests.cpp:702` | 只有 `removed:3`｜只有 `removed:4` |
| `ks-s2-local-lost` | `ks-s2-plans-not-cleared` | `RegistrationRoundTests.cpp:185` | 第三段變 `old run1 changed:1`｜少了 `new` |
| `ks-s5-reject-not-read` | `sr-dropped-cells-added` | `ImmediateChangeTests.cpp:499` | 回傳值都是 spawned｜多一個 `add Hp 7` |
| `ks-s5-changed-remove` | `ks-s5-changed-insert` | `ImmediateChangeTests.cpp:544` | `remove 2 again true`｜`insert 99 true`、`despawn 3 again true` |
| `we-j36-hook-writerpos-zero` | `we-j36-no-same-tick` | `WorldTests.cpp:1011` | early@2 少 99｜late@1 那段空 |
| `we-j36-trim-early` | `we-j36-no-same-tick` | `WorldTests.cpp:1011` | early@2 全空｜late@1 那段空 |
| `we-j36-swap-compare` | `we-j36-no-same-tick` | `WorldTests.cpp:1011` | 內容移到 late@2｜late@1 那段空 |
| `we-j29-set-once` | `we-j29-condition-cached` | `ExecutorTests.cpp:466` | `guarded@1 condition`｜中間多出 `guarded@2` |
| `ns-read-resource-non-const` | `ns-read-component-non-const` | `NestedRunTests.cpp:1469` | `overloads 12`｜`overloads 21` |
| `ks-s3-version-not-raised` | `ks-s3-failed-raises-version` | `RegistrationRoundTests.cpp:880` | `1 == 2`｜`3 == 2`（方向相反） |
| `rs-j27-guard-unchecked` | `rs-fix-c2-xe1-per-system` | `CoverageTests.cpp:412` | `0 == 1`｜`3 == 1`（方向相反） |

我的建議：**17 個都留**（第 3 節只退 51 個）。理由和第 5 節「集合相同但 expect 不同」一致：這份清單的 expect 對的是「錯成什麼樣」，不同的錯法是不同的證明。其餘 10 個值不同、檢查者判斷無實質差別的（同一個布林檢查只是診斷文字不同，或 X 是同方向更粗的錯），留在第 3 節：`hf-limit-no-names`、`ns-stamp-not-renewed`、`ns-handle-epoch-not-renewed`、`we-j1-insert-append`、`we-j2-alive-append`、`ks-s5-interrupted-still-changes`、`rs-iv-fn2-funnel-not-transitive`、`rs-iv-fn3-system-zone-without-funnel`、`ks-s6-set-not-recorded`、`rs-b3-trim-ignores-declared`。

## 4. 要你另外決定的 7 個

**第 1 步第 5 節「留」的守衛突變（6 個）**：
- 第 5 節照「規則整個關掉、實樹正控制、fail-closed、子規則唯一、抓到過漏報」留了 89 個，使用者選了選項 2。
- 下面這幾個在 subsumption 看來可以併進去：檢查者確認 X 與接手失敗的是同一個測試方法、同一行 assert。
- 但退它們等於推翻第 5 節的理由。

| id | 併進 | 試跑（初稿） | 第 5 節留它的理由 |
|---|---|---|---|
| `ks-k1-cmake-unscanned` | `ks-k1-roles-dropped` | killed | 子規則唯一（K1 讀 CMakeLists） |
| `ks-k4-off` | `ks-k4-third-symbol` | killed | 規則整個關掉 |
| `ks-k4-attribute-parameter` | `ks-k4-constexpr-after-only` | killed | 抓到過漏報（審查 A5） |
| `rs-sym-foreign-wide` | `ks-s10-sym-serial-nested-namespace` | killed | 子規則唯一（FOREIGN 的寬度） |
| `ks-k8-serial-state-unnamed` | `ks-k8-singular-dropped` | killed | K8 後半「Base 的執行緒序號」唯一（S10 決定 3） |
| `ks-k8-needs-detail-qualifier` | `ks-k8-singular-dropped` | killed | 抓到過漏報（K8 的敵對式檢查） |

如果你要退這 6 個：理論上沒有刪除型的緩的面。實際上少的是「這條規則或子規則有一個專門的突變」，以後接手被改寫或退役時，要記得這句 expect 也在裡面。

**`bs-thread-serial-is-tls-address`**：
- 它是重現 S10 真實 bug 的突變（macOS 把已結束執行緒的儲存區交給新執行緒）。
- 失敗集合和接手相同，但迴圈 200 輪裡它只失敗 186 輪，接手失敗 200 輪。失敗集合是集合、不記次數，所以看不出來它比較難殺。
- 測試若被改成不再觸發 TLS 位址重用（輪數、執行緒起訖方式），以前它會活下來而被發現；併掉之後接手仍被殺，就不會被發現。
- 只有 macOS 的資料。

我的建議：**7 個都留**。
- 守衛那 6 個守的是「規則或子規則整個壞掉」，和 subsumption 衡量的東西不同：subsumption 看現在這組自測下誰比較容易殺，它們守的是自測本身被改鬆的那一天。
- `bs-thread-serial-is-tls-address` 守的正是一種改鬆型的弱化。

## 5. C：建議留（170 個）

C 的意思是：最小集合裡沒有突變能在同一個位置證明 X 的那句斷言。退了，那句被刪掉或改鬆時，不會有最小集合裡的突變發現。程式改壞的情形仍由包含它的突變抓到（它們的失敗集合是 X 的子集）。

- 檢查者指出：C 裡有一部分，那句斷言在另一個也是 C 的突變裡、同一位置失敗。doctest 的 C 156 個裡有 81 個（用 `analyze2.py` 的判準重算；初稿時是 148 個裡約 67 個）。所以 C 不全是「唯一的證明」，而是「最小集合裡沒有證明」。
- 不建議因此再多退：那些接手多半是失敗很多的大突變，和第 9 節第 3 點（方向看反）同一類風險。
- 值得一提的幾類：
  - **集合相同但 expect 不同**：同一句斷言失敗，但 expect 對的是「錯成什麼值」，例如 `field := "?hook access` 和 `field := "?rollbackEnabled`。各自證明的是不同的錯。
  - **規則整個關掉的守衛突變**（`ks-k3-headers-off`、`ks-k5-off`、`ks-n1-forward-off`、`ks-n3-off` 等）：子規則的突變包含它們，但它們的 expect 是規則專屬的自測。
  - **初稿誤列 A/B 的 23 個**（第 9 節清單 1a、1b）：現在都在 C，或照位置條件找到了真正同一位置的接手。

170 個（括號是失敗集合的大小；「被誰包含」最多列兩個）。

| id | 執行對象 | 被誰包含（失敗集合大小） | 失去唯一突變證明的那句（expect） |
|---|---|---|---|
| `ks-k3-headers-off`（29） | guard | `ks-k5-splitlines`（1） | `FAIL: test_k3_clock_header_is_reported` |
| `ks-k5-off`（21） | guard | `ks-k5-bom`（1）, `ks-k5-directive-include-only`（1） | `FAIL: test_k5_capability_module_include_is_reported` |
| `ks-k6-every-file-exempt`（21） | guard | `ks-k6-calls-off`（16） | `FAIL: test_k6_log_header_is_reported` |
| `ks-n1-forward-off`（9） | guard | `ks-n1-attribute-unread`（1）, `ks-n1-inline-substring`（1） | `FAIL: test_n1_old_engine_namespace_in_kernel_is_reported` |
| `ks-n3-off`（14） | guard | `ks-n3-using-dropped`（1）, `ks-n3-qualified-dropped`（1） | `FAIL: test_n3_host_suffix_is_reported` |
| `ks-adv-standard-headers-widened`（3） | guard | `ks-k5-directive-include-only`（1） | `FAIL: test_k5_platform_and_third_party_headers_are_reported` |
| `ks-n1-linkage-block-named`（2） | guard | `ks-n1-unreadable-silent`（1） | `FAIL: test_n1_namespace_inside_a_linkage_block_is_checked` |
| `rs-j3-3-event-writes-not-counted`（16） | gyo_kernel_tests | `ks-s5-handle-epoch-kept`（1）, `rs-j37-nonclock-allowed`（2） | `J-22 tie: two writers of one event are one tie` |
| `rs-j3-4-z2-removed`（6） | gyo_kernel_tests | `rs-j39-ingress-for-all`（1）, `rs-iv-fx14-funnel-zone-C-systems-only`（2） | `"Z2"` |
| `rs-j3-11-skip-deferred`（3） | gyo_kernel_tests | `rs-j3-11-skip-hooks`（2） | `c.what := "?deferred insert` |
| `rs-j3-11-skip-bundle`（22） | gyo_kernel_tests | `rs-iv-fx16-one-declaration-per-request`（1）, `rs-j3-11-skip-hooks`（2） | `c.what := "?Spawn bundle` |
| `rs-j3-14-z1-reads-only`（9） | gyo_kernel_tests | `rs-iv-fx16-one-declaration-per-request`（1）, `rs-iv-fn5-hook-zone-without-funnel`（2） | `FilterWithB\|FilterWithoutB\|WriteModeFromC` |
| `rs-j24-de4-removed`（8） | gyo_kernel_tests | `rs-j24-stop-at-first`（3） | `ErrorCount(.*StartupCode::DE4)` |
| `rs-j39-boundary-c-write`（6） | gyo_kernel_tests | `rs-iv-fx6-note-dropped-Z3-site`（2）, `rs-iv-fd9-written-block-the-walk`（2） | `"Z3"` |
| `rs-j41-writes-unchecked`（7） | gyo_kernel_tests | `rs-iv-fx16-one-declaration-per-request`（1）, `rs-iv-fn5-hook-zone-without-funnel`（2） | `"Z7"` |
| `rs-sym-bss-unread`（6） | guard | `rs-adv-sym-findings-exit-zero`（1） | `FAIL: test_inline_variable_without_static_is_found` |
| `rs-sym-relro-counted`（6） | guard | `ks-s10-sym-serial-pattern-dead`（1） | `FAIL: test_sysv_read_only_and_text_and_undefined_are_not_writable` |
| `rs-adv-set-order-dropped`（4） | gyo_kernel_tests | `ks-s9-set-before-dropped`（1） | `(?=[sS]*setPlan)[sS]*J-23 declared: a set's after leaves no tie` |
| `rs-adv-z6-system-removed`（9） | gyo_kernel_tests | `rs-iv-fn4-system-Z6-without-funnel`（5） | `"Z6"` |
| `rs-adv-z6-hook-removed`（3） | gyo_kernel_tests | `rs-iv-fn6-hook-Z6-without-funnel`（2） | `hookNoRollback` |
| `rs-adv-hook-zone-rules-off`（3） | gyo_kernel_tests | `rs-iv-fn5-hook-zone-without-funnel`（2） | `hookWritesC` |
| `rs-adv-domain-tick-is-zone-b`（15） | gyo_kernel_tests | `rs-iv-fd2-seed-writes-in-place`（1）, `rs-iv-fd1-seed-all-deferred-writes`（2） | `readsTick` |
| `rs-adv-cfp-header-only`（10） | gyo_kernel_tests | `rs-b6-cfp-hooks-dropped`（1）, `hf-c-fp-eight-as-unset`（1） | `withZoneCChange` |
| `rs-adv-fp-hook-access-dropped`（1） | gyo_kernel_tests | `rs-adv-fp-rollback-enabled-dropped`（1） | `field := "?hook access` |
| `rs-adv-fp-ingress-dropped`（1） | gyo_kernel_tests | `rs-adv-fp-rollback-enabled-dropped`（1） | `field := "?ingress` |
| `rs-fix-b4-finish-before-resolve`（71） | gyo_kernel_tests | `ks-s3-hooks-unlisted`（1） | `systems == 1` |
| `rs-b6-cfp-schedule-dropped`（3） | gyo_kernel_tests | `rs-b6-cfp-hooks-dropped`（1）, `hf-c-fp-value-shifted`（1） | `change := "?rollback` |
| `rs-b6-cfp-sync-dropped`（3） | gyo_kernel_tests | `rs-b6-cfp-hooks-dropped`（1）, `hf-c-fp-value-shifted`（1） | `change := "?sync` |
| `rs-b6-cfp-version-1`（6） | gyo_kernel_tests | `hf-c-fp-value-shifted`（1） | `(?=[sS]*CFingerprint().version == 8U)(?=[sS]*none.registry->Frozen().C…` |
| `rs-iv-fc3-step0-skipped`（2） | gyo_kernel_tests | `hf-round-skips-initial-values`（1） | `TEST CASE:s+initial value: a request recorded by a hook is filled at t…` |
| `rs-iv-fc6-append-round-not-next`（10） | gyo_kernel_tests | `sr-same-emitter-cells-added`（1）, `ks-s5-changed-insert`（1） | `TEST CASE:s+initial value: (a component the entity already has is left…` |
| `rs-iv-ff1-registry-section-dropped`（9） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1）, `rs-iv-ff2-registry-request-not-hashed`（2） | `TEST CASE:s+(initial value: a declaration changes the Registry fingerp…` |
| `rs-iv-fb9-C-filter-by-request-zone`（1） | gyo_kernel_tests | `rs-iv-ff4-C-takes-zone-B`（1） | `TEST CASE:s+initial value: (a declaration changes the Registry fingerp…` |
| `rs-iv-fb11-C-count-dropped`（2） | gyo_kernel_tests | `hf-c-fp-value-shifted`（1） | `TEST CASE:s+initial value: (a declaration changes the Registry fingerp…` |
| `rs-iv-registry-version-1`（8） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1） | `RegistryFingerprint().version == 4U` |
| `rs-iv-fx8-note-dropped-system-Z6-site`（3） | gyo_kernel_tests | `rs-iv-fd10-inserts-not-sorted`（1） | `TEST CASE:s+initial value: the error names the System that writes the …` |
| `rs-iv-fx11-visit-lowest-first`（2） | gyo_kernel_tests | `rs-iv-fd10-inserts-not-sorted`（1） | `TEST CASE:s+initial value: with two written requests of one component …` |
| `rs-iv-fx12-declaration-Z1-dropped`（2） | gyo_kernel_tests | `rs-iv-fx13-declaration-Z1-exempts-input`（1） | `a zone C component takes its value from zone C data only` |
| `rs-iv-fd6-spawn-bundle-as-writes`（14） | gyo_kernel_tests | `rs-iv-fx16-one-declaration-per-request`（1）, `rs-iv-fx6-note-dropped-Z3-site`（2） | `TEST CASE:s+(J-12: every parameter type's Contribution\|initial value: …` |
| `hf-one-round`（34） | gyo_kernel_tests | `hf-rounds-follow-trim`（2）, `hf-limit-not-err`（20） | `GYO_ASSERT failed: world.window_.size() == world.runStack_[level].wind…` |
| `hf-limit-hardcoded`（12） | gyo_kernel_tests | `hf-product-limit-static`（1）, `hf-limit-larger-of-two`（2） | `J-54: KernelSettings::hookRoundLimit not read` |
| `hf-product-limit-not-read`（12） | gyo_kernel_tests | `hf-product-limit-static`（1）, `ks-s5-limit-default-only`（1） | `J-54: KernelSettings::hookRoundLimit not read` |
| `hf-rounds-every-sync`（15） | gyo_kernel_tests | `ks-s5-applies-hook-queue`（1）, `sr-cross-phase-as-alive`（3） | `JoinLog() == "B:0 C:1"` |
| `hf-mid-records-wait-for-rounds`（19） | gyo_kernel_tests | `ks-s5-hook-target-not-reset`（1）, `sr-cross-phase-as-alive`（3） | `JoinLog() == "B:0 C:1"` |
| `hf-first-sync-point`（64） | gyo_kernel_tests | `ks-s5-applies-hook-queue`（1）, `hf-rounds-follow-trim`（2） | `JoinLog() == "B:0 C:1"` |
| `hf-round-error-unwrapped`（2） | gyo_kernel_tests | `hf-round-err-layer-always-one`（1） | `result.error().Text().find("hook round 1") != std::string::npos` |
| `hf-limit-no-break-hook`（2） | gyo_kernel_tests | `hf-limit-break-hook-twice`（1） | `(breaks > 0) == breakOn` |
| `hf-product-zero-allowed`（2） | gyo_kernel_tests | `ks-s7-zero-limit-first-run-only`（1） | `a product default of 0 fails a GYO_ASSERT at the first Run[sS]*expecte…` |
| `hf-plan-fp-no-limit`（12） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1） | `planOf(t5, "Tick") != planOf(none, "Tick")` |
| `hf-plan-fp-presence-only`（3） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1） | `planOf(t5, "Tick") != planOf(t6, "Tick")` |
| `hf-plan-fp-unset-as-default`（7） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1） | `planOf(t8, "Tick") != planOf(none, "Tick")` |
| `hf-c-fp-no-limit`（5） | gyo_kernel_tests | `hf-c-fp-eight-as-unset`（1）, `hf-c-fp-value-shifted`（1） | `t5.registry->Frozen().CFingerprint() != none.registry->Frozen().CFinge…` |
| `hf-c-fp-presence-only`（2） | gyo_kernel_tests | `hf-c-fp-value-shifted`（1） | `t5.registry->Frozen().CFingerprint() != t6.registry->Frozen().CFingerp…` |
| `hf-registry-schedule-field`（4） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1） | `frozen.RegistryFingerprint().Text() == kJ18RegistryGolden` |
| `hf-export-no-limit`（3） | gyo_kernel_tests | `hf-export-limit-line-moved`（1） | `report.find("  hook-round-limit 5\n") != std::string::npos` |
| `we-j34-hook-immediate`（33） | gyo_kernel_tests | `hf-product-limit-static`（1）, `hf-round-no-epoch`（1） | `Join(Log()) == "first:4 second:4 modes@1:0"` |
| `we-j34-hook-reverse`（6） | gyo_kernel_tests | `td-adv-despawn-onremove-never-for-highest-column`（2） | `Join(Log()) == "first:4 second:4 modes@1:0"` |
| `we-j34-f5-off`（2） | gyo_kernel_tests | `hf-not-applying`（1） | `a record on the System path while Applying fails a GYO_ASSERT` |
| `we-j35-err-not-interrupted`（47） | gyo_kernel_tests | `we-adv-interrupted-write-allowed`（1）, `ns-left-error-overwritten`（1） | `a System returns Err` |
| `we-f6-empty-sync-no-epoch`（5） | gyo_kernel_tests | `hf-round-no-epoch`（1）, `sr-drop-takes-stamp`（1） | `rig->world().SyncEpoch() == epochBefore + 4` |
| `we-adv-hook-err-swallowed`（21） | gyo_kernel_tests | `td-adv-despawn-onremove-error-swallowed`（5）, `ks-s5-hook-err-own-phase-ignored`（5） | `a hook returns its own Err` |
| `we-j33-apply-after-each-system`（77） | gyo_kernel_tests | `hf-product-limit-static`（1）, `hf-round-err-layer-always-one`（1） | `changed outside the World API before step 1 'SeeBeforeMid'` |
| `td-despawn-onremove-descending`（4） | gyo_kernel_tests | `td-adv-despawn-all-entities-onremove-before-any-destroy`（1）, `td-adv-despawn-onremove-never-for-highest-column`（2） | `values: CHECK( badge-removed:1/7 mode-removed:1/3 tag-removed:1/2 tag-…` |
| `td-despawn-onremove-absent-components`（3） | gyo_kernel_tests | `we-j32-onadd-record-order`（1）, `td-adv-despawn-onremove-never-for-highest-column`（2） | `values: CHECK( tag-removed:1/2 tag-removed-second:1/2 mode-removed:1/3…` |
| `td-despawn-cells-kept`（16） | gyo_kernel_tests | `td-adv-despawn-dead-id-check-weakened`（1） | `TEST CASE:  J-32: a despawn runs every OnRemove in ascending Component…` |
| `td-adv-despawn-highest-column-not-destroyed`（2） | gyo_kernel_tests | `td-adv-despawn-dead-id-check-weakened`（1） | `TEST CASE:  J-32: a despawn runs every OnRemove in ascending Component…` |
| `td-adv-onremove-hooks-of-one-component-reversed`（2） | gyo_kernel_tests | `td-adv-despawn-onremove-never-for-highest-column`（2） | `values: CHECK( tag-removed-second:1/2 tag-removed:1/2 mode-removed:1/3…` |
| `td-adv-onremove-only-first-hook-of-a-component`（2） | gyo_kernel_tests | `td-adv-despawn-onremove-never-for-highest-column`（2） | `values: CHECK( tag-removed:1/2 mode-removed:1/3 badge-removed:1/7 == ` |
| `td-adv-onremove-hook-context-component-wrong`（3） | gyo_kernel_tests | `td-adv-despawn-onremove-never-for-highest-column`（2） | `values: CHECK( tag-removed:1/0 tag-removed-second:1/0 mode-removed:1/0…` |
| `td-adv-despawn-onremove-component-major-across-entities`（2） | gyo_kernel_tests | `td-adv-despawn-all-entities-onremove-before-any-destroy`（1） | `values: CHECK( tag-removed:1 tag-removed:2 mode-removed:1 mode-removed…` |
| `we-j32-winner-first`（6） | gyo_kernel_tests | `we-adv-insert-key-recseq-first`（1） | `rig->world().Get<Tag>(3)->t == 2` |
| `we-adv-overwrite-fires-onadd`（2） | gyo_kernel_tests | `ks-s5-changed-insert`（1） | `Join(Log()) == "remove-tag:2=2"` |
| `we-adv-remove-skips-onremove`（4） | gyo_kernel_tests | `hf-round-skips-onremove`（1）, `ks-s5-changed-insert`（1） | `Join(Log()) == "remove-tag:2=2"` |
| `we-j26-false-condition-runs`（4） | gyo_kernel_tests | `we-j29-condition-cached`（1）, `we-adv-skip-updates-lastrun`（1） | `Log() == expected` |
| `we-j3-16-executor-swallows`（95） | gyo_kernel_tests | `we-j5-id-not-mode`（1）, `ns-entry-retake-only`（1） | `expected Err AllotterExhausted` |
| `we-j50-settings-not-read`（37） | gyo_kernel_tests | `we-j5-id-not-mode`（1）, `ns-entry-retake-only`（1） | `HasCode(result, KernelErrorCode::AccessViolation)` |
| `we-adv-break-hook-not-read`（7） | gyo_kernel_tests | `hf-limit-break-hook-twice`（1） | `(breaks > 0) == breakOn` |
| `we-adv-epoch-assert-removed`（3） | gyo_kernel_tests | `ns-epoch-check-skipped-for-exclusive`（2） | `J-3: a membership change inside a System call fails the epoch check` |
| `we-w11-mut-no-mark`（3） | gyo_kernel_tests | `we-j30-skip-no-stamp`（1） | `Mirror() == full` |
| `we-adv-event-trim-never`（2） | gyo_kernel_tests | `ns-nested-events-not-trimmed`（1） | `rig->world().EventPayloads<Ping>().empty()` |
| `we-j4-boundary-off`（14） | gyo_kernel_tests | `ns-entry-retake-only`（1）, `ks-s5-no-compare-first`（1） | `(?=[sS]*HasCode(second, KernelErrorCode::BoundaryChange))[sS]*HasCode(…` |
| `we-j32-spawn-before-despawn`（10） | gyo_kernel_tests | `hf-round-skips-onremove`（1）, `sr-cross-phase-not-alive`（3） | `despawn then spawn with the same id: the new entity lives[^=]*CHECK( w…` |
| `we-j32b-dropped-err-lost`（29） | gyo_kernel_tests | `ns-self-outermost-only`（1）, `ns-no-abort-error`（1） | `static_cast<bool>(result)` |
| `we-j53-after-diff-dropped`（12） | gyo_kernel_tests | `we-adv-hook-layer2-off`（2）, `ns-exclusive-no-boundary-compare`（2） | `HasCode(result, KernelErrorCode::BoundaryChange)` |
| `we-adv-j4-serial-half-dropped`（3） | gyo_kernel_tests | `ns-renew-adopts-call`（1） | `HasCode(result, KernelErrorCode::StaleHandle)` |
| `hf-recorders-every-called-hook`（2） | gyo_kernel_tests | `hf-limit-names-first-call-order`（1） | `J-54: a hook that recorded nothing is named` |
| `hf-plan-fp-value-shifted`（2） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1） | `J-54: Plan fingerprint of a declared limit of 5` |
| `hf-plan-limit-not-carried`（10） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1）, `hf-export-limit-line-moved`（1） | `planOf(t5, "Tick") != planOf(none, "Tick")` |
| `sr-alive-applied`（39） | gyo_kernel_tests | `ks-s5-spawn-alive-id-returned`（1）, `sr-zone-c-own-cells`（2） | `(?=[sS]*J-55 player-7: 7 keeps Body 1)[sS]*J-32: the dropped spawn wro…` |
| `sr-no-record`（36） | gyo_kernel_tests | `sr-records-capped`（1） | `(?=[sS]*J-55 player-7: one record)(?=[sS]*J-8: the dropped spawn is re…` |
| `sr-no-count`（13） | gyo_kernel_tests | `sr-take-resets-count`（1）, `sr-discard-resets-count`（1） | `(?=[sS]*J-55 player-7: one dropped spawn counted)(?=[sS]*J-8: the drop…` |
| `sr-break-after-drop`（8） | gyo_kernel_tests | `sr-key-no-recseq-no-assert`（1）, `sr-records-capped`（1） | `J-55 player-7: 8 spawns with Body 80` |
| `sr-reason-alive-as-twice`（15） | gyo_kernel_tests | `sr-alive-second-as-twice`（1）, `sr-range-recorded-first`（1） | `(?=[sS]*J-55 player-7: reason IdAlive)[sS]*J-32: the reason is IdAlive` |
| `sr-reason-range-as-alive`（7） | gyo_kernel_tests | `sr-range-recorded-first`（1） | `(?=[sS]*J-55 range-first: reason IdInAllotterRange)[sS]*J-8: the reaso…` |
| `sr-range-after-alive`（2） | gyo_kernel_tests | `sr-range-recorded-first`（1） | `J-55 range-first: reason IdInAllotterRange` |
| `sr-range-after-twice`（3） | gyo_kernel_tests | `sr-range-recorded-first`（1） | `J-55 range-before-twice: 100 has the allotter's spawn` |
| `sr-range-on-spawn`（24） | gyo_kernel_tests | `sr-range-recorded-first`（1）, `ks-s5-spawn-alive-id-returned`（1） | `J-55 range-spawn: 100 and 101 spawn` |
| `sr-alive-before-despawn`（9） | gyo_kernel_tests | `sr-cross-phase-not-alive`（3） | `J-55 rejoin: the new 7 lives` |
| `sr-spawned-here-kept`（18） | gyo_kernel_tests | `sr-alive-second-as-twice`（1）, `sr-range-recorded-first`（1） | `J-55 previous-sync: reason IdAlive` |
| `sr-key-id-only`（28） | gyo_kernel_tests | `sr-key-no-recseq`（16） | `GYO_ASSERT failed: spawnKey` |
| `sr-keep-last`（5） | gyo_kernel_tests | `sr-key-no-recseq-no-assert`（1） | `J-55 one-call-twice: 9 keeps the first value` |
| `sr-one-emitter-all-dropped`（8） | gyo_kernel_tests | `sr-same-emitter-cells-added`（1）, `sr-key-no-recseq-no-assert`（1） | `J-55 one-call-twice: 9 keeps the first value` |
| `sr-two-emitters-keep-first`（17） | gyo_kernel_tests | `sr-two-emitters-cells-added`（1）, `sr-one-emitter-first-two-only`（2） | `J-55 registration-order-alone: the World is the same in both orders` |
| `sr-both-applied`（5） | gyo_kernel_tests | `sr-key-no-recseq-no-assert`（1） | `J-55 one-call-twice: 9 keeps the first value` |
| `sr-count-capped`（2） | gyo_kernel_tests | `sr-take-resets-count`（1） | `J-55 many: 64 counted` |
| `sr-drop-is-err`（41） | gyo_kernel_tests | `ks-s5-spawn-alive-id-returned`（1）, `ks-s5-spawn-alive-ends-run`（3） | `J-55 player-7: Run ok` |
| `sr-records-unsorted`（7） | gyo_kernel_tests | `sr-key-no-recseq-no-assert`（1）, `sr-records-capped`（1） | `J-55 order: 7 before 9` |
| `sr-plan-version-not-bumped`（10） | gyo_kernel_tests | `hf-registry-fp-schedule-field-when-declared`（1）, `hf-export-limit-line-moved`（1） | `(?=[sS]*J-55 versions: Plan fingerprint version 6)[sS]*planOf(none, "T…` |
| `ns-entry-baseline-not-renewed`（18） | gyo_kernel_tests | `ks-s5-baseline-read-outermost`（2） | `GYO_ASSERT failed: world.MembershipEpoch() == world.runStack_[level].s…` |
| `ns-closure-allowed`（2） | gyo_kernel_tests | `ns-closure-depth-flag`（1） | `[ns-closure-run]` |
| `ns-self-allowed`（4） | gyo_kernel_tests | `ns-self-outermost-only`（1）, `ns-self-innermost-only`（2） | `[ns-self]` |
| `ns-interrupted-runs`（7） | gyo_kernel_tests | `ns-left-error-overwritten`（1） | `[ns-again]` |
| `ns-nested-calls-handler`（38） | gyo_kernel_tests | `ns-self-outermost-only`（1）, `ns-left-error-overwritten`（1） | `[ns-err-once]` |
| `ns-left-error-not-kept`（15） | gyo_kernel_tests | `ns-self-outermost-only`（1）, `ns-left-error-overwritten`（1） | `[ns-again]` |
| `ns-refusal-does-not-interrupt`（3） | gyo_kernel_tests | `ns-left-error-listed-twice`（1） | `[ns-refusal-interrupts]` |
| `ns-nested-tick-not-advanced`（7） | gyo_kernel_tests | `ns-nested-events-not-trimmed`（1） | `[ns-run-k]` |
| `ns-handle-epoch-success-only`（2） | gyo_kernel_tests | `ns-handle-not-renewed-on-throw`（1） | `[ns-stamp-after-error]` |
| `ns-absent-component-present`（3） | gyo_kernel_tests | `ks-s5-changed-resource`（1） | `(?=[sS]*[ns-absent])[sS]*[ns-res-absent]` |
| `ns-absent-void-called`（2） | gyo_kernel_tests | `ks-s5-applies-hook-queue`（1） | `[ns-absent]` |
| `ns-closure-depth-not-decremented`（12） | gyo_kernel_tests | `ns-renew-adopts-call`（1）, `ks-s5-handle-epoch-kept`（1） | `ClosureRunsSchedule [X-14]` |
| `ns-closure-pair`（3） | gyo_kernel_tests | `ns-closure-not-recursive`（1）, `ns-closure-const-pair`（1） | `[ns-closure-pair-tuple]` |
| `ks-s9-zone-c-tie-unlisted`（7） | gyo_kernel_tests | `rs-adv-exclusive-no-conflict`（1）, `ks-s9-same-filters-no-conflict`（1） | `J-22 tie: an immediate write against a read is one tie` |
| `ks-s9-tie-without-schedule`（9） | gyo_kernel_tests | `rs-adv-exclusive-no-conflict`（1）, `ks-s9-same-filters-no-conflict`（1） | `J-23 tie: TieBreak::schedule names the schedule` |
| `ks-s4-writes-counted`（5） | gyo_kernel_tests | `ks-s4-annotation-counted-as-writer`（1） | `J-12 annotation: the Contribution of ExclusiveWorld<Writes<TestRes>>` |
| `ks-s4-annotation-not-recorded`（3） | gyo_kernel_tests | `ks-s11-annotated-writes-not-hashed`（1） | `J-12 annotation: annotatedWrites holds the listed resource` |
| `ks-s4-layer2-shared-only`（11） | gyo_kernel_tests | `ks-s5-handle-epoch-kept`（1）, `ks-s4-layer1-not-declared`（1） | `[ns-ver-quiet] .*'resource Flag'` |
| `ks-s7-first-run-only`（16） | gyo_kernel_tests | `ks-s7-zero-limit-first-run-only`（1）, `ks-s7-exclusive-settings-modify-hidden`（1） | `(?=[sS]*J-50 next Run)[sS]*J-54 next Run` |
| `ks-s10-thread-unchecked`（3） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1）, `ks-s10-check-le`（2） | `J-6 thread-bound: World::Resource on another thread did not assert` |
| `ks-s10-summary-unchecked`（1） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1） | `J-6 thread-bound: World::Summary on another thread did not assert` |
| `ks-s2-pending-written-directly`（39） | gyo_kernel_tests | `ks-s2-orders-not-cleared`（1）, `ks-s2-removed-state-kept`（1） | `pending round visible in the Registry view` |
| `ks-s2-re7-unchecked`（15） | gyo_kernel_tests | `ks-s2-re7-old-hook`（3）, `ks-s2-re7-old-initial-value`（3） | `R-E7 count (all kinds) = 0` |
| `ks-s2-remove-marks-nothing`（7） | gyo_kernel_tests | `ks-s2-orders-not-cleared`（1）, `ks-s2-removed-state-kept`（1） | `a removed System ran` |
| `ks-s2-refusal-ignored`（2） | gyo_kernel_tests | `ks-s2-interrupted-stops-refusing`（1） | `a World that declares no dynamic insert did not refuse` |
| `ks-s3-declared-edge-as-stage`（3） | gyo_kernel_tests | `ks-s3-sync-path-as-stage`（1）, `ks-s9-set-before-dropped`（1） | `declared System edge` |
| `ks-s3-order-not-expanded`（2） | gyo_kernel_tests | `ks-s3-sync-path-as-stage`（1） | `stage edge` |
| `ks-s5-change-deferred`（27） | gyo_kernel_tests | `ks-s5-changed-resource`（1）, `ks-s5-applies-hook-queue`（1） | `(?=[sS]*[ks-s5-spawn])[sS]*J-45 spawn` |
| `ks-s5-change-stamp`（3） | gyo_kernel_tests | `ks-s5-round-no-stamp`（1） | `[ks-s5-spawn-stamp]` |
| `ks-s5-level-spawn-applied`（3） | gyo_kernel_tests | `ks-s5-applies-hook-queue`（1） | `[ks-s5-level-spawn-dropped]` |
| `ks-s5-baseline-not-updated`（34） | gyo_kernel_tests | `ks-s5-baseline-outermost-level`（2） | `GYO_ASSERT failed: world.MembershipEpoch() == baseline` |
| `ks-s5-nested-baseline-not-updated`（34） | gyo_kernel_tests | `ks-s5-baseline-outermost-level`（2） | `GYO_ASSERT failed: world.MembershipEpoch() == world.runStack_.back().s…` |
| `ks-s5-hook-records-not-applied`（23） | gyo_kernel_tests | `ks-s5-applies-hook-queue`（1）, `ks-s5-round-no-epoch`（1） | `[ks-s5-hook-rounds-now] .*driver sees tag 5 no, 80 no, 1 no` |
| `ks-s5-hook-target-ignored`（9） | gyo_kernel_tests | `ks-s5-applies-hook-queue`（1）, `ks-s5-round-no-epoch`（1） | `[ks-s5-hook-rounds-now] .*driver sees tag 5 no, 80 no, 1 no` |
| `ks-s5-closure-not-refused`（11） | gyo_kernel_tests | `ks-s5-closure-refusal-not-interrupting`（3） | `[ks-s5-closure-refused]` |
| `ks-s5-compare-before-not-own-account`（5） | gyo_kernel_tests | `ks-s5-handle-epoch-kept`（1） | `[ks-s5-verification-quiet].*'component Hp'` |
| `ks-s6-remove-not-recorded`（4） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1） | `[ks-s6-record-order]` |
| `ks-s10-sync-touch-unchecked`（1） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1） | `J-6 thread-bound: (Commands\|ExclusiveWorld)::(Set\|Remove)Resource.* on…` |
| `ks-s4-annotation-also-a-write`（3） | gyo_kernel_tests | `ks-s4-annotation-counted-as-writer`（1） | `J-12 annotation: a listed resource is not a write` |
| `ks-s4-annotation-in-written-keys`（2） | gyo_kernel_tests | `ks-s4-annotation-counted-as-writer`（1） | `[ns-decl-zone-c] D-E7 Z2` |
| `ks-s4-layer2-components-only`（7） | gyo_kernel_tests | `ks-s4-layer1-not-declared`（1） | `[ns-ver-quiet] .*'resource Flag'` |
| `ks-s4-layer2-resources-only`（13） | gyo_kernel_tests | `ks-s5-handle-epoch-kept`（1）, `ks-s4-layer1-not-declared`（1） | `[ns-ver-quiet] .*'component Tag'` |
| `ks-s4-layer2-zone-b-single-writer-components-undeclared`（7） | gyo_kernel_tests | `ks-s5-handle-epoch-kept`（1）, `ks-s4-layer1-not-declared`（1） | `[ks-s5-verification-quiet] .*'component Hp'` |
| `ks-s9-mixed-zone-tie-unlisted`（2） | gyo_kernel_tests | `rs-adv-exclusive-no-conflict`（1） | `J-22 tie: a (b) read of (c) data against its (c) writer is one tie` |
| `ks-s9-tie-sides-swapped`（13） | gyo_kernel_tests | `rs-adv-exclusive-no-conflict`（1）, `ks-s9-same-filters-no-conflict`（1） | `ties[0].first == "WriteFirst"` |
| `ks-s10-world-resource-unchecked`（3） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1）, `ks-s10-check-le`（2） | `J-6 thread-bound: World::Resource on another thread did not assert` |
| `ks-s10-world-resource-mut-unchecked`（1） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1） | `J-6 thread-bound: World::ResourceMut on another thread did not assert` |
| `ks-s10-world-set-overwrite-unchecked`（1） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1） | `J-6 thread-bound: an overwrite with World::SetResource on another thre…` |
| `ks-s10-world-remove-unchecked`（1） | gyo_kernel_tests | `ks-s10-handles-unchecked`（1） | `J-6 thread-bound: World::RemoveResource on another thread did not asse…` |
| `ks-s7-verification-read-live`（3） | gyo_kernel_tests | `ks-s7-nested-reread`（1） | `J-50 same Run` |
| `ks-s4-layer2-zone-c-resources-undeclared`（2） | gyo_kernel_tests | `ks-s4-layer1-not-declared`（1） | `[ns-written] the Run reported` |
| `ks-s4-layer1-zone-c-resources-undeclared`（1） | gyo_kernel_tests | `ks-s4-layer1-not-declared`（1） | `[ks-s4-layer1-declared] .*'CRes'` |
| `ks-s4-layer1-zone-b-single-writer-components-undeclared`（1） | gyo_kernel_tests | `ks-s4-layer1-not-declared`（1） | `[ks-s4-layer1-declared] .*'Mode'` |
| `ks-s7-break-first-run-only`（3） | gyo_kernel_tests | `ks-s7-removed-keeps-break`（1） | `J-50 next Run: breakOnKernelError` |
| `ks-diks15-code-added-without-text`（3） | build:gyo_kernel_tests | `ks-links-extra`（1）, `ks-diks15-code-without-text`（2） | `enumeration value 'DE8' not handled in switch\|DE8.*not handled\|C4062` |
| `bs-id-issuer-no-advance`（5） | gyo_base_tests | `bs-id-issuer-not-atomic`（2）, `bs-thread-serial-issuer-per-thread`（2） | `[id-issuer-repeat]` |
| `bs-id-issuer-starts-at-zero`（8） | gyo_base_tests | `bs-thread-serial-first-zero`（3）, `bs-id-issuer-one-source-for-all`（4） | `[id-issuer-zero]` |
| `bs-id-issuer-starts-at-two`（5） | gyo_base_tests | `bs-id-issuer-one-source-for-all`（4） | `[id-issuer-first]` |
| `ks-k8-rule-dropped`（10） | guard | `ks-k8-skips-preprocessor-lines`（1）, `ks-k8-skips-executor-files`（1） | `FAIL: test_k8_id_issuer_is_reported` |
| `ks-k8-current-serial-refused`（2） | guard | `ks-k8-include-line-read`（1） | `FAIL: test_k8_current_thread_serial_and_the_include_are_clean` |
| `ks-k8-needs-base-qualifier`（3） | guard | `ks-k8-line-always-1`（1）, `ks-k8-cmake-unread`（1） | `FAIL: test_k8_id_issuer_after_using_is_reported` |
| `ks-k8-skips-source-files`（2） | guard | `ks-k8-skips-executor-files`（1） | `FAIL: test_k8_in_a_source_file_is_reported` |

## 6. 草案方向的對照

照第 1 步第 7 節第 7 題草案的方向（A 的失敗集合包含 B 的就退 B），候選是 **276 個，其中 188 個屬於一般定義下必須留的最小集合**（用初稿的鍵算；鍵加上 SUBCASE 後最小集合多 6 個，結論不變）。退了它們，各自唯一能發現的測試弱化就沒人發現。這是使用者決定更正方向的根據。

## 7. 限制

- **包含關係只對「刪掉」型的弱化成立**：「改鬆」型（讓一句斷言只在更極端的情況才失敗、迴圈輪數減少）不一定被包含它的突變發現。第 9 節第 3、4 點就是這一類；第 4 節的 `bs-thread-serial-is-tls-address` 因此留下。失敗集合只記「有沒有失敗」，不記失敗幾次，這一類看不出來。
- **守衛的自測與預期編譯失敗只比文字**：unittest 的粒度是一個測試方法，同一個方法裡不同 assert 的失敗分不開。第 3 節沒有守衛突變（6 個都在第 4 節）；3 個預期編譯失敗的，檢查者確認 expect 與位置都和接手相同。
- **中止會讓集合少記**：排除中止的案例再跑時，那個案例裡中止之後的斷言沒跑到。檢查者確認這沒有造成假的包含：X 必須有同案例、同式子的 ABORT 鍵，而中止之後的斷言對 Y 在實際跑法裡本來就到不了。合併後的接手照 `cases` 篩跑時，第一個中止就結束行程，後面併進來的案例不會跑。這一點靠試跑把關：第一版有 1 個（`rs-iv-fc6-append-round-not-next`）因此合併不成，現在它在 C。
- **`hf-round-count-kept` 逾時**：不篩案例時某個案例卡住，沒有輸出可以判斷是哪個。它的集合只有「TIMEOUT」，自然留在最小集合。照 `cases` 篩的平常跑法會被 GYO_ASSERT 殺掉，不受影響。
- **只在 macOS arm64 跑過**：全部的失敗集合都只有 macOS 的資料。符號表類（`rs-sym-*`、`ks-s10-sym-*`）走的是 Mach-O 路徑，Linux 的 ELF 路徑沒有資料。
- **關係是對現在這組測試說的**：測試改了，包含關係可能跟著變。合併的做法讓每句斷言都留在 expect 裡，以後要重算，跑 `run.py`、`parse.py`、`analyze2.py` 即可。

## 8. 要你決定的事

**已定（使用者 2026-11-19）：「照你建議的做」**——第 1 題退 51 個（合併進接手）；第 2 題 17 個留；第 3 題 7 個留；第 4 題 C 170 個留；第 5 題 7 個空 expect 列為後續、不在這次改。比過的選項：各題的「建議」與「也退／也併」。理由見各節。清單的改動與驗證在 dev_log。

1. **第 3 節的 51 個**：照建議退役（合併進接手）嗎？核准後另一個變更改清單，照第 2 步的做法：
   - 重跑收到合併的接手、ctest、dev_log。
   - 合併時保留 X 的 `what`（寫進接手的 `what`）。
   - dev_log 記下第 2 步的接手又換人的那幾個（鏈式合併，例如 `sr-range-removed`）。
2. **第 3b 節的 17 個**：留（建議），還是也退？
3. **第 4 節的 7 個**：留（建議），還是也併進去？
4. **第 5 節 C 的 170 個**：留（建議）？要退的話要逐個核准緩的面。
5. **既有問題，另案處理（不在這次退役裡）**：7 個突變的 expect 是案例標題，而那幾個案例無條件印 MESSAGE，所以 expect 實際上是空的，只剩「`cases` 裡有東西失敗」。這 7 個是 `rs-iv-ff1`、`rs-iv-fb5`、`rs-iv-fb6`、`rs-iv-fb7`、`rs-iv-fb8`、`rs-iv-fb9`、`rs-iv-fb11`（第 9 節第 2 點）。照 AGENTS §14.2 要改寫 expect，建議列為後續。其中 `rs-iv-fb8` 是 `rs-iv-ff3-C-section-dropped` 的接手，第 3 節的合併會讓它的 expect 不再是空的（變嚴）。

## 9. 敵對式檢查

檢查者：Fable 5.2／high，general-purpose、唯讀（沒有改檔、沒有重跑突變或建置；讀報告、證據、清單、原始碼，跑只讀的分析）。作者是 Opus 5.5，模型不同。檢查的是初稿（建議退 96 個、A 76／B 27／C 148）。

**結果：「96 個沒有緩的面」不成立，96 個裡 31 個有問題。** 數字本身全部對得上，合併的實作沒有造成緩的面。全部照下面處理，本頁已是處理後的版本。

| # | 指出的事 | 會失去檢出力？ | 處理 |
|---|---|---|---|
| 1 | A/B 判定只看「X 的 expect 正規式在接手不篩案例的整份輸出裡對得上」，沒看對上的是哪一句。23 個的接手對上的是別句斷言（通用文字、同案例的另一行、別的案例的同一句 GYO_ASSERT）。其中 13 個退了之後，那句斷言沒有任何突變的 expect 指到。試跑殺掉也證明不了，因為接手自己失敗的那句剛好含同樣的文字 | 是 | 判定改成看位置（第 1 節）：X 的 expect 每一句在接手（合併後的 `cases` 範圍）對上的位置必須非空、不是空標題，而且是 X 自己位置的子集（`analyze2.py`，沿用檢查者的 `sites.py`）。13 個全部改列 C；另外 10 個裡 6 個改列 C（加上合併不成的 `rs-iv-fc6-append-round-not-next`，從 A/B 改列 C 的共 21 個，含下一列的 `rs-iv-fb9`），4 個（`rs-j3-4-z1-removed`、`hf-no-rounds`、`hf-first-queue`、`hf-only-with-trim`）找到位置也對得上的接手 |
| 2 | 案例標題型的 expect，遇到無條件印 MESSAGE 的案例永遠對得上（8 個案例）。清單裡 7 個突變的 expect 因此實際上是空的 | 是（`rs-iv-fb11-C-count-dropped`，含在 #1） | 位置條件排除空標題（`rs-iv-fb9-C-filter-by-request-zone` 因此從 A/B 改列 C）；7 個的 expect 改寫列為後續（第 8 節第 5 題） |
| 3 | 失敗集合的鍵不含 SUBCASE。`ks-s2-re7-unchecked` 在 7 個 SUBCASE 各失敗一次，7 個 `ks-s2-re7-*` 各只在自己那個 SUBCASE 失敗，初稿判成「集合相同」，退的是小的、留的是大的，方向反了 | 是（改鬆型） | 鍵加上 SUBCASE（`keys2`）。重算：最小集合 463 → 469、候選 251 → 245；這 7 個變成最小集合 |
| 4 | `bs-thread-serial-is-tls-address` 被標成「集合相同」，但迴圈 200 輪它只失敗 186 輪、接手 200 輪。它是 S10 真實 bug 的重現突變 | 是（改鬆型） | 個別留下，移到第 4 節；第 7 節補「集合不記次數」的限制 |
| 5 | 第 1 步退 `rs-j12-bundle-dropped` 的理由是「J-12 同一行還有 `rs-iv-fd6-spawn-bundle-as-writes`」；初稿把 fd6 併進 fd7，而 fd7 失敗的是下一行 | 是（含在 #1） | fd6 現在是 C（留），第 1 步的理由仍成立 |
| 6 | B 的邏輯要多一個條件：Z 對上的位置要 ⊆ X 的位置。初稿的 B 裡 10 個不滿足 | 是（含在 #1） | 位置條件同樣套用在 B（第 1 節） |
| 7 | C 的定義（「留下的突變輸出裡都沒有」）和實作（只查最小集合）不一致；doctest 的 C 裡約 67 個，那句在另一個 C 裡、同一位置有 | 否（說法；留著是保守的） | 第 0、5 節改成「最小集合裡沒有突變證明」，不再多退 |
| 8 | `rs-b3-trim-not-pinned` 的 expect 有頂層 `\|`，包進 lookahead 後語意改變（這一次是變嚴；如果排在最後一句會變鬆） | 否（變嚴） | 合併時每句包成 `(?:…)`（`merge_trial.py`） |
| 9 | 41 對的 X 與接手 expect 字串完全相同，合併後同一句寫兩三次 | 見 #1 | 相同的句子只寫一次 |
| 10 | 沒有 GYO_ASSERT 訊息的中止（SIGSEGV）都記成「ABORT｜案例｜None」，不同原因分不開 | 推測：影響小 | 鍵加上訊號名 |
| 11 | 說法不準：包含關係只對「刪掉」成立、「沒有緩的面」要限定為刪除型、中止型靠「expect 出現」把關不成立、全部資料只有 macOS、「集合相同」的粒度 | 否（說法） | 第 1、3、7 節改寫 |
| 12 | 第 2 步的接手在這一步又被退（鏈式合併，例如 `sr-range-removed`） | 否 | 第 8 節第 1 題：正式合併時保留 X 的 `what`，dev_log 記下接手換人 |

檢查者也確認了：
- 數字全部自洽（獨立重算）。
- 初稿建議的 96 個，子集合併的定義和全部合併時逐位元組相同，所以第一次試跑的結果直接適用。
- 103 對的 `platforms`、`expect_build_failure`、`command`、`target`、有沒有 `cases` 全部相同。
- 中止排除的做法沒有造成假的包含（#10 除外）。
- 只在同一種執行對象之間比，是保守的方向。
- 第 4 節的 6 個守衛：X 與接手失敗的是同一個測試方法、同一行 assert，建議留合理。
- 3 個預期編譯失敗的與 `bs-thread-serial-not-thread-local`：expect、位置都和接手相同，是真的重複。

**窄複查**（同一位檢查者 Fable 5.2／high，唯讀；檢查重算後的版本，當時建議 68 個）：

結論：**68 個的位置條件全部成立**（檢查者另寫一份更細的實作：案例＋SUBCASE＋檔:行，`cases` 用合併後實際的範圍）。合併清單（每句包 `(?:…)`、去重、646 ＝ 714 − 68、51 個接手只改 expect 與 `cases`）與第 0、2 節的數字也都對得上。

| # | 指出的事 | 會失去檢出力？ | 處理 |
|---|---|---|---|
| 1 | 68 個裡 27 個，X 與接手在同一行失敗但值不同；其中 17 個退了之後，清單裡沒有突變在那一行以 X 的錯法失敗 | 刪除型：否；改鬆型：是 | 17 個移到第 3b 節交使用者，建議留；第 3 節改成 51 個 |
| 2 | 「16 個和接手的失敗集合完全相同」容易讀成內容相同；實際內容相同的只有 8＋3 個 | 否（說法） | 第 2 節改寫 |
| 3 | 33 對的合併對接手沒有實質改動（expect 相同、`cases` 已包含），試跑沒有新資訊 | 否 | 第 3 節開頭補說明 |
| 4 | 第 5 節「67／59」是初稿的數字 | 否 | 改成 81／156 |
| 5 | 從 A/B 改列 C 的實際是 21 個（多 `rs-iv-fc6`、`rs-iv-fb9`） | 否 | 第 9 節第 1、2 列補上 |
| 6 | 報告留有佔位字串 | 否 | 換成本段 |
| 7 | `rs-iv-fb8`（空 expect 之一）是第 3 節某個 X 的接手，合併後不再是空的 | 否（變嚴） | 第 8 節第 5 題註記 |

檢查者也確認了：
- 68 個裡沒有 X 比接手難殺的：每一輪內的 ERROR 次數都是 X ≥ 接手。
- 初稿清單 1a 的 13 個都在 C；1b 的 10 個裡 6 個在 C、4 個換了接手後位置條件成立；7 個 `ks-s2-re7-*` 都在最小集合；`bs-thread-serial-is-tls-address` 在第 4 節。
- 腳本搬到 `checker/recheck/`。

## 10. 證據

主 checkout `build/evidence/gyo2-mutation-prune/subsumption/`（git 不追蹤；本機 macOS arm64）：
- `run.py`：資料收集。`full/raw/<id>.txt` 是每個突變的原始輸出（714 個，12 MB），`full.log` 是進度與時間，`full.base` 是當時的 commit。
- `baseline.py`、`baseline/`：指令與探針在沒改的程式上的輸出（parse 時扣掉）。
- `parse.py` → `full/failsets.json`；`analyze.py` → `full/analysis.json`（**初稿**，被第 9 節推翻，留作紀錄）；`analyze2.py` → `full/analysis2.json`（本頁的數字）。`analyze2.py` 要在 `checker/` 裡執行，沿用檢查者的 `lib.py`、`sites.py`（位置對應）。
- `checker/recheck/`：窄複查的腳本與輸出（`v.py` 位置對應的獨立實作、`c1.py` 68 個的位置條件、`c4.py`／`c4.out` 失敗區塊比較、`c6.py` 合併清單檢查、`c8.py`／`c8.out` 17 個那一行還有哪些突變）。
- `checker/`：檢查者的腳本（從暫存區搬來）：`a6.py`（96 個的位置涵蓋）、`a11.py`（鍵加 SUBCASE 的重算）、`a13.py`（C 的檢查）、`a2.py`（合併重建與屬性比對）。
- `merge_trial.py`：合併的臨時清單。
  - 初稿：`trial_catalog.json`、`trial.log`（103 個全部併，73 個接手殺 72）；`rec_catalog.json`、`rec_trial.log`（初稿建議的 96 個，67/67）。
  - 本頁：`rec2_catalog.json`、`rec2_vias.txt`、`rec2_trial.log`（位置條件成立的 68 個全部併進去，51/51）；`rec3_catalog.json`、`rec3_vias.txt`、`rec3_trial.log`（第 3 節的 51 個，663 個，39 個接手 39/39）。
- `guard_keep.json`：第 5 節「留」的守衛突變（現在清單裡的 90 個守衛突變，含改寫的 `ks-real-product-name`）。
- `tables.py`、`tables.md`：本頁表格的產生器與輸出。
- `pilot/`：開跑前用 4 個突變做的試跑。

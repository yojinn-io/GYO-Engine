# 2026-11-22 GYO2：第 5 批 多個 World、交換點、Product 層 Resource

批次代號：`v8-arch-gyo2-exchange`。分支 `claude/gyo2-exchange-81ea9f`（從 `gyo2` 45f13757；開工時 merge 了 `gyo2` 0f0ab8a5，含第 0 步的 #268）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high；敵對式檢查與設計審查是 Fable 5.2／high（唯讀）。

## 起因與核准

- 使用者 2026-11-22：「第5批可以開始了。」任務卡先寫事前宣告 [`exchange-declaration.zh-Hant.md`](../architecture/target/v2/exchange-declaration.zh-Hant.md)。
- 動工前核准：使用者 2026-11-22「照建議，選共用處，核准」（宣告第 12 節的決定紀錄；`v8-arch-gyo2-exchange-declaration-approved`）。之後使用者問「郵箱？」「Host？」，宣告更正出處、做一次用詞對齊（宣告第 18、19 節），設計不變。
- 開工：使用者 2026-11-22「開始」。
- 第 0 步（兩條守衛）已由 #268 合併，紀錄在 [`2026_11_22_gyo2_exchange_guards.zh-Hant.md`](2026_11_22_gyo2_exchange_guards.zh-Hant.md)。

## 做了什麼（宣告第 9 節的 thin start）

### 第 1 步：Product 一份的共用處（宣告 5.2）

- `Product` 另持有一份 `ModuleBuffer`（`buffer_`），不凍結、活到 Product 結束。拿得到的地方：`ProductSetup::Buffer()`、`WorldSetup`／`WorldFinish`／`WorldCheck::ProductBuffer()`（都可寫）、`Product::Buffer()`。
- `ModuleBuffer` 的物件照 `Create` 的反序解構（解構子逐個 `pop_back`；`vector` 自己的解構順序標準沒規定）。型別註解改寫成兩種用法。
- `ProductDescription::options`（`OptionBag`）＋`ProductSetup::Options<T>()`：產品與測試給單位的東西，在第一個單位的 `Configure` 就在。
- `ProductSetup::Report`：收進 `ProductStartupReport::unitErrors`；有任何 `Configure` 回報就不跑任何 World 的 `Build`～`Check`（每個 World 標沒檢查）。
- `ProductSetup::OnDestroy`：所有 World 之後、共用處之前，登記的反序，各恰好一次；`Build` 失敗（含 `Configure` 丟例外）時已登記的也跑。
- `~Product` 改成明確的六步：World（描述的反序；每個 World：交換函式 → World → Registry）→ `OnDestroy` → runner（O-E4 時多裝的也留到這一步：`extraRunners_`）→ 共用處 → 描述與選項 → 登記單位。

### 第 2 步：交換函式的登記（宣告 5.1）

- `WorldSetup::ExchangeIn`／`ExchangeOut`（`ExchangeFn`＝`Result<void, OrchestrationError>(Kernel::World&)`），在 `Build` 登記、存在 World 槽，替換時隨新的 `Build` 換上（`BuildNewFirst` 失敗時舊的照舊；`DropOldFirst` 丟掉舊 World 時一起放掉）。
- `RunFrame`：每個 World「進 → 一輪 → 出」；遊戲初始化在第一個 World 的「進」之前；`RunGameInit`、`RunSchedule` 不呼叫。一輪中途壞掉時「出」與之後的 World 都不呼叫。
- 失敗：`OrchestrationErrorCode::ExchangeFailed`（E2；訊息帶進或出、單位名、World 名，函式的碼在 `reporterCode`），World 不算壞、Product 不結束；同一個 World 的下一個函式不呼叫。例外原樣往外傳。預設 runner 收到它回 `Failure`。
- 生成世界（另一張卡）合併後，一輪的定義（生成的那一批）由後合併的那一批補測試（宣告第 10 節末、生成世界宣告的 C19）。

### 第 3 步：條文

- 編排層 `orchestration.zh-Hant.md`：O-2（共用處、選項、`Configure` 回報、`OnDestroy`、`~Product` 的順序）、O-3（runner 直接讀寫、可呼叫 Module 給的函式）、O-5、O-7、O-8 加句；新增 O-12「交換點」（四種形與規則、交換函式、Extract 的契約）、O-13「多個 World 的執行緒」（只寫不變量）；RC-O10 改寫、RC-O11～RC-O13 新增。都標〔草稿〕，合併前核准時定稿。
- Kernel DI-KS 13：Extract 的執行原語拿來源當 `const`、來源與時機由編排層給（實作在 SDL 那批）。
- `docs/cross-component-rules.md`：新增 CCR-6（World 外的服務自己同步；套用點與交換函式持鎖只做表的操作、不等另一條執行緒）；CCR-5 的負責欄加 O-13。
- world-time：第 3 節與 WT-6 第 (2) 項的退出條件改成「第一個需要 runner 取樣的 runner 那批」（方向不變；合併前核准時給使用者看）。
- 加註：runtime-composition 第 2 節（Mailbox 那句的出處）、日誌宣告 5.4 末句（交換點已排）、時間設計 5.3 與 6.4。

## 驗證

（本機 macOS Intel，AppleClang，preset `test`）

- 新測試：`orchestration.product-buffer`（P-1～P-9）、`orchestration.exchange`（X-1～X-8）18 案全過；編排層既有 84 案照過。
- 測試寫的過程中抓到的（都是測試自己的錯，實作沒改）：P-5 同一型別 `Probe` 建兩次（斷言正確擋下，改成兩個型別）；P-5 的 runner 探針被 configure 閉包多抓一份（改成只讓 runner 持有）；P-8 被移走的那一份也記錄、檢查恆真（改成比「World 的 Resource 先死、共用處的物件後死」的先後）；X-2、X-8 的紀錄比對寫錯分隔與起點。
- 全量建置與 ctest：163/163（`impl-ctest.log`；161 加新的兩組）。新的建置警告 0（16 個都是既有的）。敵對式檢查後編排層 103 案、663 個斷言全過。
- 突變：見下。
- L1：PR 上跑。

## 突變（README 第 3、8 點）

- 宣告時的 25 個（`or-pbuf-` 12、`or-exch-` 13）：25/25 killed（`run-impl-mutants.log`）。
- **敵對式檢查**（Fable 5.2／high，唯讀；作者 Opus 5.5；`mutants-adversarial-check.md`）：必修 6、提案 6、輕微 5。
  - M1：本批改了 `Product.cpp` 的文字，讓 9 個既有突變的 `find` 對不上（`or-runframe-no-gameinit`、`g8-product-build-before-units`、`g8-swap-keeps-old-lists`、`g8-check-report-unit`、`g8-check-inside-finish-loop`、`g8-check-sees-true`、`or-skeleton-after-units`、`or-buffer-finish-other`、`or-buffer-check-other`）。原因：我每步只跑新的突變、沒做靜態的 find 次數掃描（第 4 批後續卡提過的同一個漏洞）。復原 9 個；其中 6 個的 `guards` 是檢查者照案名填的，我核對過條文。
  - M2：`or-exch-init-after-in` 實際是刪掉遊戲初始化（和 `or-runframe-no-gameinit` 同一個改動），改寫成真的搬到第一個「進」之後。
  - M3～M6：替換時的「出」、錯誤訊息的單位名與「出」、選項的銷毀時點、「不凍結」各補測試或突變。
  - 提案採了檢查者標「建議採」的 4 個（`or-pbuf-ondestroy-before-worlds`、`or-pbuf-buffer-before-ondestroy`、`or-pbuf-report-stops-configure`、`or-exch-round-after-failed-in`）與 10 個 `expect` 的收窄；逐欄細項等 10 個不採（README「尋ねずに行う」的分類，理由在合併前核准頁第 4 節）。P6：替換時 4 行沒有條文與測試的 `clear()` 拿掉。
- 檢查後加的 9 個與復原、改寫、改過 `find` 的：45 個裡 43/45 → 兩個是我之後改程式沒對齊（`or-exch-out-side-label` 的 `replace` 還寫舊參數、建置失敗；`or-exch-replace-out-kept` 的 `expect` 照拿掉 `clear()` 之後的訊息改），改後 45/45 killed（`run-impl-mutants-2.log` 與之後的單獨重跑）。
- 本批加 34 個（宣告 25＋檢查後 9），基準是改的條文 9 條 × 5＝45。各自只捕自己的錯，寫在 `what`，都有 `guards`。
- 靜態掃描（不建置，數每個突變的 `find` 出現幾次）：所有清單都恰好一次。
- 合併前的完整跑（宣告判定 3）：第一次從 0ae8a8fa 開始後，`gyo2` 合進了 #269（命名收尾），PR 有衝突，所以停掉、merge 之後從 0bca189d 重跑（臨時的 detached 工作樹 `.claude/worktrees/gyo2-exchange-fullmut`，腳本 `fullmut.sh`）。結果見下方補記。

## 跟上 `gyo2`（#269）

- `Product.hpp` 一處衝突（#269 把私有入口的回傳改成 `Base::Result`，我加了 `Exchange`）：照 #269 的寫法合起來。
- 我新寫的 engine 程式照 N13 改成短名（`Base::Result`、`Base::Err`）；兩個突變（`or-exch-failure-ignored`、`or-exch-next-called`）的 `find` 跟著改。
- #269 把測試支援的 namespace 改成 `GYO::Test::Orchestration`：新的兩個測試檔跟上（第一次建置失敗，ctest 跑到的是舊執行檔，不算數；重建後 163/163，`merge-ctest.log`）。
- 和命名收尾卡約好的兩處（`kernel_guard.py` 的 K9 那一行用本批的、`tests/common/authority/CMakeLists.txt` 的 `include(ExpectLinks.cmake)`），#269 合併時已照約定處理。
- 全部清單的靜態 find 掃描：0 問題。

## 設計審查（每批固定的一步）

- 審查者：Fable 5.2／high，唯讀（實作者 Opus 5.5）。報告：主 checkout `build/evidence/gyo2-exchange/design-review/REPORT.md`。結論：整體合理，沒有新的要使用者定的設計題。
- 保留：`ModuleBuffer` 一個型別兩種範圍、描述的選項、`OnDestroy` 兩段式（系譜補 Unity Entities 的「停 → `OnDestroy` → 銷毀」、照建立的反序）、交換函式的登記（近例 Bevy `SubApp::set_extract`）。
- 改了：`~Product` 的條文改寫成「誰比誰活得久」；`runner_` 與 `extraRunners_` 合併；O-2 的 Product 層那一組拆成 O-15（和生成世界卡協調編號）；O-3 補「做了一部分的 E2」；O-12 的 Extract 縮成本質一句；O-13 拿掉重複與待辦；DI-KS 13、WT-6 拿掉進度與沿革；宣告兩處引用改正確；日期改回 2026-11-22；測試字串的 mailbox 拿掉；`Exchange` 直接收 `World&`。
- 可選提案（`~Product` 的兩個細項降成實作細節，會放寬判定 P-5）：交使用者，合併前核准頁第 8 節。
- 後續（轉主會話排批）：world_logging 給接 `WorldSetup` 的登記函式，加一個走真的 `TakeKernelDiagnostics` 的測試；本批 Engine 裡沒有交換函式的呼叫者。

## 證據

主 checkout 的 `build/evidence/gyo2-exchange/`（使用者的 Intel Mac，git 不追蹤）：`impl-build.log`、`impl-ctest.log`、`run-impl-mutants.log`、`run-impl-mutants-2.log`、`mutants-adversarial-check.md`（與 `-proposed.json`、`-tests.patch`、`-run.log`）、`design-review/REPORT.md`；宣告階段的 `survey-bevy-lightyear.md`、`survey-unity-flecs-mass.md`、`declaration-r0-adversarial-check.md`、`declaration-r1-recheck.md`。

合併前核准頁：[`exchange-merge-approval.zh-Hant.md`](../architecture/target/v2/exchange-merge-approval.zh-Hant.md)。

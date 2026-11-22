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
- 全量建置與 ctest：163/163（`impl-ctest.log`；161 加新的兩組）。新的建置警告 0（16 個都是既有的）。
- 突變：見下。
- L1：PR 上跑。

## 突變（README 第 3 點；本批加 25 個）

（結果見下方補記）

## 證據

主 checkout 的 `build/evidence/gyo2-exchange/`（使用者的 Intel Mac，git 不追蹤）：`impl-build.log`、`impl-ctest.log`、`run-impl-mutants.log`；宣告階段的 `survey-bevy-lightyear.md`、`survey-unity-flecs-mass.md`、`declaration-r0-adversarial-check.md`、`declaration-r1-recheck.md`。

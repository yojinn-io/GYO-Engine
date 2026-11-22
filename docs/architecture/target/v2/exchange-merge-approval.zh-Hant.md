# 合併前核准：多個 World、交換點、Product 層 Resource（`v8-arch-gyo2-exchange`）

件數の突き合わせ：該当なし（本批的判定沒有送出與收到的件數）。

日期：2026-11-22。分支 `claude/gyo2-exchange-81ea9f`（PR #270，base `gyo2`）。事前宣告：[exchange-declaration.zh-Hant.md](exchange-declaration.zh-Hant.md)（修訂 2；使用者 2026-11-22 動工前核准）。dev_log：[`2026_11_22_gyo2_exchange.zh-Hant.md`](../../../dev_logs/2026_11_22_gyo2_exchange.zh-Hant.md)。

**請你核准的**：第 2 節（各頁 Design Invariants 的最終文字）、第 3 節（§14.4：新的守衛測試）、第 4 節（突變的數目）、第 5 節（和宣告不同的地方）、第 6 節（新型別與新碼）、第 7 節（沒做、記下的事）。第 8 節是設計審查的一個可選提案，選「照現狀」也可以合併。

## 1. 判定的結果（宣告第 10 節）

| # | 結果 | 證據 |
|---|---|---|
| 1 | 本機 macOS x86_64 全量 ctest 163/163（7ca34378 之後；161 加新的兩組）；編排層 103 案、663 個斷言全過（91f652f0）。L1 三平台：PR #270 上跑（結果補記） | `impl-ctest.log` |
| 2 | P-1～P-9、X-1～X-8 全過（敵對式檢查後 P-1、P-5、X-3、X-6、X-7 加了案或斷言，見第 3 節） | `orchestration.product-buffer`、`orchestration.exchange` |
| 3 | 本批的突變經 Fable 5.2／high 敵對式檢查補齊；相關突變的結果與合併前的完整跑見第 4 節 | `run-impl-mutants.log`、`run-impl-mutants-2.log`、`mutants-adversarial-check.md` |
| 4 | 設計審查（Fable 5.2／high）：整體合理，沒有新的要你定的設計題；8 處小修都已改（第 5 節），一個可選提案在第 8 節 | `design-review/REPORT.md` |

## 2. Design Invariants 的最終文字（各頁標〔草稿，`v8-arch-gyo2-exchange`〕的段落）

- **編排層** [orchestration.zh-Hant.md](orchestration.zh-Hant.md)：
  - O-2：Product 層那一組移到 O-15，O-2 只留一句指過去。
  - O-3：runner 可以呼叫 Module 給的函式、「runner 直接讀寫」；`ExchangeFailed` 是第一個「做了一部分」的 E2。
  - O-5：`Configure` 的回報，有它就不建 World；`ExchangeFailed`。
  - O-7：交換函式和準備函式同時登記、但存在 World 槽。
  - O-8：Extract 的來源被替換後不讀舊來源。
  - **O-12 交換點**（新）：四種形——套用點、交換函式、runner 直接讀寫、Extract——與規則。
  - **O-13 多個 World 的執行緒**（新，只寫不變量）。
  - **O-15 Product 一份的共用處**（新）：共用處、選項、`Configure` 的回報、`OnDestroy`、「誰比誰活得久」。
  - RC-O10 改寫；RC-O11～RC-O13 新增。
  - 編號和生成世界卡協調過：本批 O-12、O-13、O-15、RC-O11～13；生成世界 O-14、RC-O14～19。
- **Kernel** [kernel.zh-Hant.md](../kernel.zh-Hant.md)：DI-KS 13 加一句（Extract 的原語拿來源當 `const`，來源與時機由編排層給）。
- **時間 Module** [world-time.zh-Hant.md](world-time.zh-Hant.md)：**第 3 節與 WT-6 第 (2) 項的退出條件改寫**——原文（你 2026-11-19 核准）是「交換點做出來後這一項移給 runner」；交換點這一批做出來了，但取樣不移，改成「到第一個需要 runner 取樣的 runner 那批」（有視窗的是 SDL 那批；沒畫面的 Match 若先來就是它那批）。方向不變。理由：宣告 5.5——沒有點名的 Feature 需要現在移，Bevy 的取樣點跟著 present、要等有視窗的 runner。
- **跨元件規則** [cross-component-rules.md](../../../cross-component-rules.md)：新增 CCR-6（World 外的服務自己同步；套用點與交換函式持鎖只做表的操作、不等另一條執行緒）；CCR-5 的負責欄加 O-13。
- **加註**（指向本批，不改當時的紀錄）：runtime-composition 第 2 節（Mailbox 那句出自舊規格）、日誌宣告 5.4 末句、時間設計 5.3 與 6.4。

## 3. §14.4：新的守衛測試（都是新判定或只收緊，沒有緩める面）

P-1～P-9、X-1～X-8 守 O-2、O-12、O-15，屬 Design Invariants 的守衛，照 §14.4 交你核准。敵對式檢查後的測試改動都只收緊：

| 改動 | 1 守的性質 | 2 緩める面 | 3 締める面 | 5 證據 |
|---|---|---|---|---|
| P-1 加「`Finish`、`Check` 拿到的是同一個 Product 共用處」 | O-15：一份、各處拿到同一份 | 無 | 交出別的 buffer 時失敗 | `mutants-adversarial-check-tests.patch` |
| P-5 加選項的探針 | O-15：描述與選項在共用處之後才放 | 無 | 選項太早放時失敗 | 突變 `or-pbuf-options-early` |
| P-7 拆標記 | O-15：`Configure` 回報後其餘單位照跑 | 無 | 後面的單位沒跑時失敗 | `or-pbuf-report-stops-configure` |
| X-3 加「單位登記的『出』失敗」一案 | O-12：錯誤帶進或出、單位名 | 無 | 名字或進出寫錯時失敗 | `or-exch-unit-name`、`or-exch-out-side-label` |
| X-6 加「出」 | O-12：替換時換上新的進、出 | 無 | 留著舊的「出」時失敗 | `or-exch-replace-out-kept` |
| X-7 加一行正向確認 | O-12：一輪中途壞掉時不呼叫「出」 | 無（原本兩個 `find()==npos` 在紀錄是空的時也會過；現在先確認有紀錄） | 紀錄是空的時失敗 | `mutants-adversarial-check.md` 輕微第 3 點 |

第 4 項（緩める面是否由別的判定守）：沒有緩める面，不適用。

## 4. 突變的數目（README 基準 8）

| 清單 | 本批加 | 復原或改寫 | 只收窄 expect | 退 | 現在 |
|---|---|---|---|---|---|
| `orchestration_mutations.json` | 34（宣告時 25＋敵對式檢查後 9） | 復原 9（本批改文字讓它們的 `find` 對不上）、改寫 1 | 10 | 0 | 275 |
| Kernel `mutations.json`（第 0 步，#268 已合併） | 1 | — | — | 0 | — |
| `authority_mutations.json`（第 0 步，#268 已合併） | 1 | — | — | 0 | — |

- 基準：改的條文 O-2、O-3、O-5、O-7、O-8、O-12、O-13、O-15、DI-KS 13 共 9 條 × 5＝45。本批加 34，在基準內。
- 不採的提案（README「尋ねずに行う」的分類）：
  - 逐欄細項 3 個（`ProductBuffer()` 在產品自己的 `build`、`Finish`、`Check` 各一個）。
  - `~Product` 相鄰步驟中檢查者沒標「建議採」的 3 個。
  - `Configure` 回報的 `unit` 欄、沒檢查的旗標各 1 個。
  - 預設 runner 的 E2、`reporterCode` 各 1 個。
  - 這些錯誤，測試今天已經會失敗，只是沒有突變專門指著它。
- 相關突變：本批的 34 個與改過 `find` 的 11 個（復原 9、`or-runner-not-called`、`or-buffer-create-after-freeze`）：45/45 killed（第一次 43/45，兩個是我改程式後沒對齊的 `replace` 與 `expect`，修正後重跑殺掉；dev_log「突變」）。
- 合併前的完整跑（宣告判定 3：Kernel＋編排層＋`world_time`＋權威＋回滾＋base＋naming）：背景跑，結果補記。
- 靜態掃描（不建置，數每個突變的 `find` 出現幾次）：所有清單都恰好一次（91f652f0）。

## 5. 和宣告不同的地方

| 宣告 | 實作 | 理由 |
|---|---|---|
| Product 層那一組寫進 O-2 | 拆成 O-15 | 設計審查：O-2 原本一段約 3,700 字、七個主題 |
| `~Product` 的六步寫成條文 | 條文寫「誰比誰活得久」，步驟留在 `Product.cpp` 的註解 | 設計審查：條文該寫關係，不寫程序 |
| `runner_` 與多裝時的 `extraRunners_` 兩個欄位 | 合成一個 `runners_` | 設計審查：名實不符；行為不變 |
| 替換時先清掉舊的交換函式（`DropOldFirst` 與換上時共 4 行） | 拿掉 | 敵對式檢查 P6：沒有條文、沒有測試，拿掉也沒有可觀察的差別（交換函式只拿參數 `World&`） |
| O-12 的 Extract 寫了「extract schedule 寫在每幀清單裡、『進』在它之前」 | 縮成本質一句（唯讀、不寫回、兩個都停著、替換後不讀舊來源），做法指向宣告 5.4 | 設計審查：還沒做的機制的做法寫成不變量，SDL 那批一改就是 Architecture Delta |
| 宣告寫「沒有一家有交換函式的登記」 | 改成「沒有一家有多個單位依序登記的清單；Bevy 的 `SubApp::set_extract` 是近例」 | 設計審查查到 Bevy 的近例（`sub_app.rs:171-209`） |
| 宣告寫日誌宣告 5.4「已寫明等交換點」 | 改成「寫了可以移到交換點」 | 設計審查：原文是「可以」 |
| 宣告、條文的日期寫 2026-11-24 | 改成 2026-11-22 | 我把額度訊息的「11-24 重置」誤當成當天；機器的時鐘與 commit 都是 11-22 |

## 6. 新型別與新碼

- `ProductDescription::options`（`OptionBag`）、`Product::Buffer()`。
- `ProductSetup::Buffer()`、`ProductSetup::Options<T>()`、`ProductSetup::Report(code, message)`、`ProductSetup::OnDestroy(fn)`。
- `WorldSetup::ProductBuffer()`、`WorldFinish::ProductBuffer()`、`WorldCheck::ProductBuffer()`。
- `ExchangeFn`；`WorldSetup::ExchangeIn(fn)`、`WorldSetup::ExchangeOut(fn)`；`Detail::ExchangeEntry`。
- `OrchestrationErrorCode::ExchangeFailed`。
- 名字照同組的寫法：`Prepare(fn)`、`InstallRunner`、`Buffer()`。

## 7. 沒做、記下的事

- **交換函式在 Engine 裡還沒有呼叫者**（設計審查）。點名的使用者是 Kernel 的診斷紀錄；接線要由 world_logging 給一個接 `WorldSetup` 的登記函式，加一個走真的 `TakeKernelDiagnostics` 的測試。已轉主會話排批。
- World 交出到別的執行緒（O-13 的入口）：PvP 的 Client 移植那批。交出那批要把「進 → 一輪 → 出」做成單一入口，`RunFrame` 改成呼叫它。
- Extract 的實作與來源替換的做法（「讀者一起重建」或「准許換來源」）：SDL 那批。
- 時間取樣移給 runner：第一個需要的 runner 那批（第 2 節的 world-time）。
- 生成世界合併後，「一輪」含生成那一批的合起來案例，由後合併的那一批補（生成世界宣告的 C19）。
- 只在 macOS（AppleClang、libc++）跑過突變；銷毀順序靠明確的步驟，不依賴標準庫，L1 的三平台會跑 P-5。

## 8. 設計審查的可選提案：`~Product` 的兩個細項要不要降成實作細節

- **背景**：設計審查認為這兩條沒有站得住的理由，卻進了 P-5 的整串比對與突變：
  - 同一個 World 裡「交換函式先於 World 放掉」：交換函式只拿參數 `World&`，先後沒有可觀察的差別。對應突變 `or-pbuf-exchange-after-world`。
  - `Build` 失敗時，多裝的 runner 也照 runner 那一步才放掉（今天由 `runners_` 一起持有）。對應突變 `or-pbuf-failed-build-runners`。
- **照現狀**（不改）：兩條留在 P-5 與突變裡。代價是多守兩個沒有使用者的細節。
- **降成實作細節**：P-5 的整串比對拿掉這兩段，兩個突變退役。這是**放寬先宣告的判定 P-5**，照 §14.4 要你核准：
  - 1 守的性質：`~Product` 的銷毀順序。
  - 2 緩める面：同一個 World 裡「World 先於交換函式放掉」、失敗路徑上多裝的 runner 早於 `OnDestroy` 放掉——兩種今天會失敗，改了之後通過。
  - 3 締める面：無。
  - 4：「每個 World 比共用處先放」「回呼在 World 之後」仍由 P-5 其餘部分與 P-8 守。
  - 5 證據：`design-review/REPORT.md` 第 4 項。
- **本卡的建議**：照現狀。兩條都便宜，而且失敗路徑和成功路徑走同一個順序，比較好講。若你覺得多守了，選降級。

**核准紀錄**：（待使用者）

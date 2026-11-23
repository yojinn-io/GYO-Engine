# 合併前核准：Extract 的執行（`v8-arch-gyo2-sdl-extract`，SDL 線子批 R1a）

件數の突き合わせ：該当なし（本批的判定沒有送出與收到的件數）。

日期：2026-11-23。分支 `claude/quirky-sinoussi-6065a7`（PR #299，base `gyo2`）。事前宣告：[sdl-extract-declaration.zh-Hant.md](sdl-extract-declaration.zh-Hant.md)（修訂 1；使用者 2026-11-23 動工前核准「Q1、Q2 照建議，開始」）。dev_log：[`2026_11_23_gyo2_sdl_extract`](../../../dev_logs/2026_11_23_gyo2_sdl_extract.zh-Hant.md)。流程照開卡時的舊規則（主會話 2026-11-23 轉達 `v8-arch-gyo2-core-agents-transition`）。

**合併前核准（2026-11-23）**：使用者在本卡「核准」——第 2～6 節照本頁（Design Invariants 的最終文字、§14.4 的判定含 DI-KS 13 守衛欄、突變的數目、和宣告不同的地方、新的公開介面與碼）。合併等使用者明說「合併」。

**請你核准的**（已核准）：第 2 節（Design Invariants 的最終文字）、第 3 節（§14.4：守衛的判定）、第 4 節（突變的數目）、第 5 節（和宣告不同的地方）、第 6 節（新的公開介面與碼）。核准後由你說「合併」才合併。

## 1. 判定的結果（宣告第 2.6 節）

| # | 結果 | 證據 |
|---|---|---|
| 1 | 本機 macOS x86_64（Apple clang，preset `test`）全量建置成功、`ctest -L cpu` 197/197（實作完成時）；merge `origin/gyo2` a3cb107e 之後（077cc127）重新 configure、全量建置、`ctest -L cpu` 197/197；再 merge KR（#295，`origin/gyo2` dd09651a）之後（45c7b132）198/198，相關突變 Kernel 19/19、編排層 15/15 killed。和 KR 只有文件、突變清單與 CMakeLists 的文字衝突，兩邊都保留；同步點那一段沒有語意上的交互（KR 改關係的維護、當場改與 `Restore`，R1a 改 `Run` 的入口與步驟的 context）`gyo_kernel_tests` 410 案、`gyo_orchestration_tests` 134 案 | 本機 |
| 2 | Kernel XK-1～XK-8 與斷言各案、編排層 `orchestration.extract` 14 案全過 | `tests/common/kernel/ExtractTests.cpp`、`tests/common/orchestration/ExtractTests.cpp` |
| 3 | 相關突變（D184）：Kernel 19/19、編排層 15/15 killed | dev_log「檢驗」 |
| 4 | 實作審查（Opus／high，同模型）：可合併，B0 S4 N8，S 全採用、N 採用 5 | `build/evidence/gyo2-sdl-extract/review-r0.md` |
| 5 | 突變清單的敵對式檢查（Opus／high，同模型）：值得加 8、清單要修 1、不值得 12；採用 7＋合併 1，不採 M-3（理由在第 4 節） | `build/evidence/gyo2-sdl-extract/mutant-list-check-r0.md` |
| 6 | L1 三平台全綠（b116760c，KR 合併之後；linux-x64、windows-x64、macos-arm64 與 CI gate） | PR #299，run 45146771727 |

所有「同模型」：Fable 用量不足期間，主會話轉達使用者「暫時用Opus」。

## 2. Design Invariants 的最終文字（各頁標〔`v8-arch-gyo2-sdl-extract`，草案〕的段落）

- **Kernel** [kernel.zh-Hant.md](../kernel.zh-Hant.md)：
  - **DI-KS 35 Extract 的執行**（新）：`RunExtract` 是讀者 World 的一次最外層 Run、和 `Run` 同一段；Plan 步驟上的 System 與 run 條件的 Extract 參數讀來源，巢狀的內層拿不到；四條斷言；D-E7 也擋 run 條件；不加錯誤碼。
  - **DI-KS 36 Extract 參數讀呼叫當下的來源狀態**（新）：讀法清單；不寫來源（接版本的簿記除外）；沒有 `Changed`／`Added`、不讀來源的 `Message` 與移除紀錄。
  - **DI-KS 37 Kernel 不保存來源 World**（新）：只記來源的 Registry；呼叫的人的規則（Registry 的壽命與銷毀先後、`RunExtract` 期間不寫來源）。
  - **RC 28**（新）：Extract 要寫回、要動兩個 World，或參數不夠。
  - DI-KS 13 的守衛欄（見第 3 節）；DI-KS 26 的三處「`Run`」改成「`Run`／`RunExtract`」。
- **編排層** [orchestration.zh-Hant.md](orchestration.zh-Hant.md)：
  - O-5：O-E9（O-E8 不用）；來源沒登記成的讀者標沒檢查、只報一次（你選的 Q1＝(a)，和 O-15 的 `Configure` 回報同類，是「一次收齊」的例外）。
  - O-8：R1b 之前替換有讀者的來源是斷言（鏈的中間也算），在丟掉任何東西之前；替換讀者照常。
  - O-12 的 Extract 那一項：`extractFrom`、O-E9 的條件、接來源的時點、三個入口走 `RunExtract`、讀到的時點、一個來源多個讀者與鏈。
  - O-15「誰比誰活得久」：讀者的 Registry 也先於來源的；`Build` 失敗時照反序丟。
- **說明頁** [system-contract-page.zh-Hant.md](system-contract-page.zh-Hant.md)：第 3 節表的 Extract 列、新第 20 節「從別的 World 讀（Extract）」（KR 先佔了第 19 節）；第 13 節的「O-E1～O-E7」補 O-E9。

## 3. §14.4：守衛的判定

| 改動 | 1 守的性質 | 2 緩める面 | 3 締める面 | 4 | 5 證據 |
|---|---|---|---|---|---|
| D-E7 也擋 run 條件（XK-7） | H-6：Extract 參數只在 extract schedule | 無 | 以前啟動通過、執行時才撞斷言的「非 extract schedule 上帶 Extract 參數的條件」，現在啟動時失敗 | 不適用 | 突變 `ks-de7-condition-skipped`、`ks-de7-set-condition-skipped`、`ks-de7-extract-condition-reported` |
| DI-KS 13 的守衛欄：「J-43」→「XK-2、D-E14 的案、J-10」 | DI-KS 13：Extract 唯讀、無反饋 | **字面上有**：規格的 J-43 寫了「寫入斷言」，新守衛不做。**實際沒有**：gyo2 上沒有叫 J-43 的測試，也沒有寫入斷言的實作，沒有一個以前失敗、現在通過的輸入 | 新增 XK-2（來源的摘要與 stamp 不變） | 寫來源只能繞過 `const` 型別；型別在編譯時擋，繞過型別的誤用由審查抓（你選的 Q2＝(a)） | 宣告第 10 節 Q2；`SignatureTests.cpp` J-10 |
| 既有判定 | — | 沒有改任何既有測試的期望值；既有突變的 `find`、`expect` 只有 `or-pbuf-worlds-forward` 合併了一案（expect 改成兩案都要失敗，只收緊） | — | — | 突變清單 |

## 4. 突變的數目（README 基準 8）

| 清單 | 本批加 | 合併 | 退 |
|---|---|---|---|
| Kernel `docs/architecture/plans/kernel/scripts/mutations.json` | 16（`ex-` 13、`ks-de7-` 3，含檢查後加的 4） | — | 0 |
| `docs/architecture/target/v2/scripts/orchestration_mutations.json` | 14（`or-ex-`，含檢查後加的 4） | `or-pbuf-worlds-forward` 加 extract 的銷毀案 | 0 |

- 基準：改的條文 DI-KS 13、26、35、36、37 與 O-5、O-8、O-12、O-15 共 9 條 × 5＝45。本批加 30，在基準內。每個的 `what` 寫了只它捕得到的錯、`guards` 都填了。
- 不加的（README「尋ねずに行う」）：來源的 `CheckThread`（M-3：O-12 的前提是同一條執行緒，和其他讀法同一個檢查；要另寫跨執行緒的工作者，等觀察到漏掉再加）；`Get`／`Contains`／`Count`／`ForEach` 各自（逐項細節，`ex-reads-reader` 與 `ex-entities-unsorted` 代表）；W-4 的 `Check`（J-4 用 `Res` 代表）；觀察不到的（巢狀拿不到來源、`SourceScope`、`Build` 的反序丟、第 5 步的 `unchecked`）。理由逐條在 dev_log。

## 5. 和宣告不同的地方

1. `rs-adv-extract-outside-extract-schedule` 的 `find` 沒有改（宣告 2.1 第 5 點說會改）：條件的檢查另走一段，System 那一行不動；條件由新突變守。
2. 「extract schedule 卻沒有來源」的斷言放在 `RunOn` 的 try 之前（放在裡面會被當成例外、記成 Product 結束）；`beforeSwap` 那一處仍在替換的 try 裡（`DropOldFirst` 時舊 World 已丟；Programmer Error 正式版直接 abort）。
3. `Build` 第 5 步加「有沒檢查的 World 就不建」：正常路徑上不改行為（沒檢查一定帶著擋住它的錯）；沒有它，突變下 Build 會成功、在第 6 步對空的 Registry 解參考。
4. 審查後：const 版的讀取函式改名 `Detail::SourceColumnAt`／`SourceResourceAt`（只給 Extract 用、沒有 W-6 的檢查）；`Build` 的登記結果在例外的路上也反序丟；拿掉語意上多餘的 `extractFromRefused` 欄位。
5. 測試代號：宣告的 XK-1～XK-7、XO-1～XO-7 之外，加了 XK-8（讀者反序登記，型別照來源的 Registry 查）與「被略過的 World 之後的讀者」一案。

## 6. 新的公開介面與碼

- Kernel：`WorldExecutor::RunExtract(std::string_view, const World&)`；`Extract<Read<T>>` 的 `Entities`／`Count`／`Contains`／`Get`／`ForEach`；`ExtractRes<R>` 的 `Present`／`operator*`／`operator->`。`StartupCode`、`KernelErrorCode` 不加。
- 編排層：`WorldDescription::extractFrom`；`OrchestrationCode::OE9 = 9`。
- 指紋與 golden 不變（`ScheduleOptions::extract` 本來就在 Plan 指紋裡）。

## 7. 留給之後

- R1b：來源替換（讀者一起重建／准許換來源，交你），屆時拿掉替換的斷言。
- R2a：`render_world` 把 extract schedule 插在 `Main` 之前。
- R2b：「上次 Extract 之後改了什麼」（宣告 2.4 的候選）、實體對應；審查 N-5（`ForEach` 每個元素一次二分搜尋）大量 sprite 時再看。

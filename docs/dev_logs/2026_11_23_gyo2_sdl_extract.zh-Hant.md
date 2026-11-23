# 2026-11-23 Extract 的執行（`v8-arch-gyo2-sdl-extract`，SDL 線子批 R1a）

Owner：Kernel＋編排層（GYO2 主會話開的任務卡）。分支 `claude/quirky-sinoussi-6065a7`（開向 `gyo2`）。作者 Opus 5.5。

## 為什麼

SDL 總宣告（`sdl-declaration.zh-Hant.md` 修訂 21，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 6.1 節把 Extract 的執行排成子批 R1a：渲染一類的 World 只經 Extract 唯讀拿來源 World 的資料（K.3-6、DI-KS 13）。gyo2 上只有 Extract 的「宣告那一半」（參數、`ScheduleOptions::extract`、`SetExtractSource`、D-E14），沒有執行；編排層沒有接來源的地方（交換點宣告 5.4「留給 SDL 那批的」）。點名的 Feature：KeepTool 的 ImGui draw list 經 Extract（子批 K）、2D 遊戲的渲染（R2b）。

事前宣告：[`sdl-extract-declaration.zh-Hant.md`](../architecture/target/v2/sdl-extract-declaration.zh-Hant.md)。修訂 0 敵對式檢查（Opus／high，同模型）B1 S10 N8 全採用 → 修訂 1；主會話確認 RC 28 與和 KR 的先後；動工前核准（使用者在本卡「Q1、Q2 照建議，開始」：Q1＝(a) 來源沒登記成時讀者不登記、標沒檢查；Q2＝(a) 不加 extract 期間的寫入偵測）。

## 做了什麼

- **Kernel**（`engine/ecs/kernel`）：
  - `WorldExecutor::RunExtract(schedule, const World& source)`：和 `Run` 共用最外層的一段（`RunOutermost`：正在 Run 的計數、接版本、`KernelSettings`、`DroppedMessages` 的重設、Plan、修剪）；這次 Run 的 Plan 步驟（含 run 條件）經 `SystemContext::extractSource` 讀來源，巢狀跑給空。`extractSource_` 由 scope 在每條離開的路上清掉。斷言：`Run` 指名 extract schedule、`RunExtract` 指名非 extract、來源的 Registry 身分不符或就是讀者、來源不是 Ready。
  - Extract 句柄的讀法：`Extract<Read<T>>` 的 `Entities`（升冪）、`Count`、`Contains`、`Get`、`ForEach`；`ExtractRes<R>` 的 `Present`、取值。經 const 版的 `Detail::SourceColumnAt`／`SourceResourceAt` 讀（沒有 W-6 的檢查，名字標明只給來源用；審查 N-6）。
  - D-E7 也擋 run 條件（掛載點的、set 的）帶 Extract 參數掛在非 extract schedule（修訂 0 的檢查 B-1：以前啟動通過、執行時撞斷言）。
  - X-15 的訊息改成指向 `RunExtract`（碼與規則不變）。
- **編排層**（`engine/orchestration`）：
  - `WorldDescription::extractFrom`；`OrchestrationCode::OE9 = 9`（O-E8 不用）。
  - O-E9 (a) 在 `Build` 第 1 步對描述本身查（名字先複製一份：迴圈會把描述搬進槽）。被拒的讀者在登記時自然找不到排在前面的來源（槽保留描述的先後），不另記旗標（突變清單檢查 M-14：先寫了一個 `extractFromRefused` 欄位，語意上多餘，拿掉）；(b) 在登記後的清單檢查（每幀清單有 extract schedule 而沒有 `extractFrom`、開局清單有 extract schedule）。
  - 讀者在 `BuildPlans` 之前 `SetExtractSource(來源的 Registry)`；替換讀者時用來源槽裡目前的 Registry。來源沒登記成（Kernel 的錯、被 O-E6 丟掉、讀者自己的 O-E9）時讀者標沒檢查，不另報 D-E14（Q1＝(a)）；第 5 步把「有沒檢查的 World」也算成不建（正常路徑上不改行為：沒檢查一定帶著擋住它的錯）。
  - 編排層跑 schedule 的三處（清單、`RunSchedule`、`beforeSwap`）經 `RunWith`：extract schedule 走 `RunExtract`。「extract schedule 卻沒有來源」的斷言放在 `RunOn` 的 try 之前（放在裡面會被當成例外、記成 Product 結束）。
  - 替換有讀者的來源（鏈的中間那個也算）是斷言，在丟掉任何東西之前；R1b 定做法時拿掉。
  - `Build` 的登記結果在每條離開的路上照反序丟（scope；含單位的 `Build` 丟例外），讀者的 Registry 先於來源的（DI-KS 37、O-15）。
- **條文**（Design Invariants；最終文字在合併前核准）：Kernel DI-KS 35～37、RC 28 新增；DI-KS 13 的守衛欄（J-43 → 本批的案與 J-10，Q2）；DI-KS 26 的「`Run`／`RunExtract`」。編排層 O-5（O-E9、沒檢查的讀者）、O-8、O-12 的 Extract 那一項、O-15。說明頁第 3 節表與新第 20 節（KR 先佔了第 19 節）。

## 和宣告不同的地方

- 宣告 2.1 第 5 點說既有突變 `rs-adv-extract-outside-extract-schedule` 的 `find` 會跟著改：沒有改。條件的檢查另走一個 lambda（`Registry.cpp` 的 `ResolveSystems`），System 自己那一行不動；條件的洞由新突變 `ks-de7-condition-skipped`、`ks-de7-set-condition-skipped` 守。沒有任何既有判定改變（§14.4 不適用）。
- 「extract schedule 卻沒有來源」的斷言從 `RunWith` 裡挪到 `RunOn` 的 try 之前（理由見上）。`beforeSwap` 那一處仍在替換的 try 裡（`DropOldFirst` 時舊 World 已丟，斷言是 Programmer Error，正式版直接 abort；審查 N-3），沒有另加突變（改回 `fresh->Run` 由 XO-5 捕到）。
- 第 5 步加「有沒檢查的 World 就不建」：宣告沒寫。正常路徑上不改行為；沒有它，O-E9 (a) 的突變下 Build 會成功、在第 6 步對空的 Registry 解參考。

## 檢驗

- 本機（macOS Intel，Apple clang，preset `test`）：整個建置成功；`ctest -L cpu` 197/197 通過（實作完成後一次）；之後依審查與突變清單檢查改了測試與 `Product.cpp` 的小處，`gyo_kernel_tests` 410 案、`gyo_orchestration_tests` 134 案重跑全過，merge `origin/gyo2` a3cb107e 之後（077cc127）重新 configure、全量建置、`ctest -L cpu` 197/197；再 merge KR（#295）之後（45c7b132）198/198、相關突變 19/19、15/15 killed。和 KR 只有文字衝突（兩邊都保留；說明頁 KR 先佔第 19 節，本批改第 20 節），同步點沒有語意上的交互。合併前核准頁：[`sdl-extract-merge-approval.zh-Hant.md`](../architecture/target/v2/sdl-extract-merge-approval.zh-Hant.md)。
- 新測試：
  - Kernel `tests/common/kernel/ExtractTests.cpp`（`kernel.others`）：XK-1 讀到來源的值（兩邊放不同值）、XK-2 來源不被動（摘要與 stamp）、XK-3 讀法（逐項 CHECK）、XK-4 run 條件讀來源、XK-5 來源接上新版本、XK-6 修剪／`KernelSettings`／`DroppedMessages` 每次一份、XK-7 D-E7 擋條件（與在 extract schedule 上不報）、XK-8 型別照來源的 Registry 查（讀者反序登記）、斷言（`GYO_CHECK_ASSERTS`）。
  - 編排層 `tests/common/orchestration/ExtractTests.cpp`（新 ctest 組 `orchestration.extract`）：每幀（含來源的「出」之後、讀者的「進」在前）、O-E9 五種、讀者沒檢查（Kernel 錯、O-E6）、被略過的 World 之後的讀者仍拿到自己的來源、`RunSchedule`、替換讀者（兩種先後、`beforeSwap`）、銷毀先後、兩個讀者與鏈、斷言四案（各自一個 TEST_CASE）。
- 突變（D184：新的全跑；`find` 在改動範圍的既有突變也跑）：Kernel 19/19 killed（新 16＋既有 `ns-extract-nestable`、`rs-adv-extract-outside-extract-schedule`、`ks-s2-extract-source-after-build`）；編排層 15/15 killed（新 14＋合併了 extract 銷毀案的既有 `or-pbuf-worlds-forward`）。
  - 途中三個清單或執行的錯（不是測試或實作）：zsh 不拆未加引號的變數，`--only $K` 只當成一個 id，第一次重跑漏跑新的那批（已用 `${=K}` 重跑）；`cases` 用了含逗號的 test case 名字，doctest 的 `-tc=` 把逗號當分隔、一案都沒跑，突變「存活」；`CHECK_MESSAGE` 串 `const char*` 被印成位址，標籤對不上（`wrong_failure`）。都改了再跑。

## 突變（AGENTS §14.5）

新加 30 個（Kernel 16、編排層 14）。改動的條文 DI-KS 13、26、35、36、37 與 O-5、O-8、O-12、O-15 共 9 條 × 5 = 45 以內（README 第 8 點）。每個只捕別的捕不到的誤（各自的 `what`），`guards` 都填了。和宣告 2.6 的草案比：
- 拿掉：`ex-trims-source`（修剪只作用在讀者，來源是 const，寫不出能編譯又真實的「修剪來源」）；`ex-source-after-conditions`（來源在建 call 時一次設好，沒有「條件之後才設」的形；改成 `ex-step-no-source`）；`ex-condition-no-source`（同上）；`or-ex-replace-chain-middle`（斷言掃全部 World，沒有「只擋末端」的形）；`or-ex-failed-source-registers-reader` 改名 `or-ex-reader-registered-anyway`。
- 新加：`ex-step-no-source`、`ex-runextract-accepts-plain`、`ex-source-not-ready-unchecked`、`ex-extract-dropped-shared`（實作審查 S-3）、`ks-de7-set-condition-skipped`、`or-ex-assert-inside-run`；O-E9 拆成五個（README 第 3 點）；突變清單檢查加的 `ex-res-reads-reader`（M-1）、`ex-component-id-from-reader`、`ex-resource-id-from-reader`（M-2）、`ks-de7-extract-condition-reported`（M-11）、`or-ex-source-index-wrong`（M-13）、`or-ex-registrations-misaligned`（M-15）、`or-ex-replace-reader-source-unset`（M-16）、`or-ex-beforeswap-plain-run`（M-17）。
- 合併：`or-pbuf-worlds-forward` 加 extract 的銷毀案（M-21；`guards` 補 O-15、DI-KS 37）。
- 不加：來源的 `CheckThread`（M-3：同一條執行緒是 O-12 的前提，和其他讀法同一個檢查；要測得另寫跨執行緒的工作者，§14.5 等觀察到漏掉再加）；XK-2 沒有突變（寫來源只能繞過 `const`，DI-KS 13 守衛欄寫明只靠型別與審查，M-10）。觀察不到的：巢狀步驟拿不到來源（D-E7 讓任何能被巢狀跑的 schedule 都沒有 Extract 參數）；「來源就是讀者」的斷言（被 Registry 身分的斷言涵蓋，除非 Registry 是自己的來源）；`Build` 的反序丟（`~Registry` 不碰來源指標）；第 5 步的 `unchecked`（正常路徑上一定有別的錯一起來）。

## 審查與檢查

- 宣告的敵對式檢查：Opus／high，同模型；B1 S10 N8 全採用（宣告第 11 節）。證據 `build/evidence/gyo2-sdl-extract/declaration-r0-adversarial.md`。
- 實作審查：Opus／high，同模型；可合併，B0 S4 N8。採用：S-1 斷言案拆開（免得突變下跑進懸空指標）、S-2 XK-3 逐項、S-3 XK-6 補兩項（XK-2 的游標與 XO-6 的失敗路徑觀察不到，見上）、S-4 本節、N-1 例外路徑也反序丟、N-2、N-6 改名、N-8 文件。未改：N-3（見上）、N-4／N-5（量小；R2b 大量 sprite 時再看 `ForEach`）、N-7（set 條件的 D-E7 每個成員各報一次，照現行登記錯誤的慣例）。證據 `build/evidence/gyo2-sdl-extract/review-r0.md`。
- 突變清單的敵對式檢查（§14.2）：Opus／high，同模型；值得加 8、清單要修 1、不值得 12。採用 M-1、M-2、M-11、M-13、M-15、M-16、M-17、M-21、M-14（拿掉多餘欄位）、M-10（文件）；不採 M-3（理由見上）、M-22（可選合併：各自已 killed，合併只省執行時間）。證據 `build/evidence/gyo2-sdl-extract/mutant-list-check-r0.md`。

證據都在主 checkout `build/evidence/gyo2-sdl-extract/`（git 不追蹤，這台 macOS Intel）。建置、測試、突變的輸出未保存（數字已記在上面）。

## 沒做、留給之後

- R1b（來源替換：讀者一起重建／准許換來源，交使用者）；屆時拿掉替換的斷言。
- R2b：「上次 Extract 之後改了什麼」（宣告 2.4 的候選）、實體對應。
- R2a：`render_world` 把 extract schedule 插在 `Main` 之前。

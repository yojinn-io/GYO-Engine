# 獨占 World 的 System 可以跑一張 schedule（`v8-arch-gyo2-nested-schedule`，K0）

日期：2026-11-16。分支 `claude/gyo2-nested-schedule-381f0f`（從 `gyo2` `3743eaa8` 開出）。PR [#239](https://github.com/yojinn-io/GYO-Engine/pull/239)（目標 `gyo2`）。

這一批由任務卡 gyo2-nested-schedule 做（GYO2 重建主會話開出）。事前宣告：[K0 事前宣告修訂 6](../architecture/target/v2/kernel-nested-schedule-declaration.zh-Hant.md)（#237 併進 `gyo2`；使用者 2026-11-16「核准 K0」，`v8-arch-gyo2-k0-judgement-approved`）。實作者 Opus；宣告第 7 節的五處另由 Opus／xhigh 逐行複核；突變清單的對抗檢查 Fable／high。

## 做了什麼

用射擊遊戲當例子：每一幀「讀輸入 → 跑固定步（0 到數步）→ 算畫面」。這一批讓「跑固定步」可以由 World 裡的一個 System 來做，並讓 Kernel 自己跑開局（Startup）。

1. **巢狀執行**（宣告第 4 節第 1～9、15 項）：
   - `ExclusiveWorld::RunSchedule(名字)` 經 `Detail::RunNested` 進執行器（呼叫的上下文帶執行器的指標；hook 的上下文也帶，才走得到拒絕）。`Product::Run`（從外面）照舊斷言 World 閒置；兩者共用 `Product::RunPlan(名字, nested)`。
   - 執行中的 schedule 記成 World 的成員 `runStack_`（每層一筆：schedule、這一層的延後紀錄在 `window_` 的起點、成員的基準）。每層用 RAII 的 `LevelGuard` 推入／彈出；例外穿過時設中斷、丟掉延後紀錄，巢狀那一層另外留下 `NestedRunAborted`。裡面那張跑完回到 Running，最外層跑完才回 Ready。
   - 拒絕（回 Err、World 立刻中斷、留下錯誤）：hook 裡（X-13）、閉包裡（X-14）、名字不存在（X-12）、Startup（H-12）、extract（X-15）、Startup 執行中（H-12）、正在執行中（任何一層，X-11）。全部通過之後才看「World 已中斷」（中斷了就不執行、回傳留下的錯誤）——這個先後宣告沒定，回報了主會話（見「和宣告不一致或待定」）。
   - 成員檢查：進入巢狀前比對一次（常開的 `GYO_ASSERT`），回來後（含例外的路徑，`Rebase`）以現在的值為外層這一步的新基準。
   - 存取驗證：進入前把外層 System 到這裡為止的改動照它自己的邊界比對（有違規就記在它頭上、不進入），再重拍；巢狀那一層沒有 Ready 窗口的比對；回來後（含例外的路徑）重拍。
   - 每層的紀錄：`ApplySync` 只拿 `window_[windowBase..]`、套用後切掉；跑完時的兩條斷言改成「這一層的那一段是空的」。Z5 的重查另外看外面每一層留著的紀錄與那幾張的 hook 佇列（(c) 區的插入與指定編號的生成），訊息寫出在哪一層、哪張 schedule。
   - 出錯：`Fail(error, nested)`——巢狀時中斷、丟掉全部延後紀錄、留下錯誤（只留最先的）、不呼叫錯誤處理。執行器在每個 System 回來後，World 已中斷就把留下的錯誤併進這次呼叫的清單（已有就不重列），交給最外層呼叫一次。
   - 句柄：`RunSchedule` 回來後（成功、失敗、被拒絕都一樣）句柄的同步點記號更新成 World 現在的，寫入的 stamp 換成新取的一個。
2. **`ExclusiveWorld` 的新介面**（第 10、11 項）：一個名字的樣板，`ExclusiveWorld<>` 與 `ExclusiveWorld<Writes<R…>>`；別的寫法（例如 `ExclusiveWorld<Write<R>>`）由帶標記的 `static_assert` 擋。`Get`／`GetMut` 拿掉，換成閉包：`Read<T>(實體, f)`、`Modify<T>(實體, f)`、`Read<R>(f)`、`Modify<R>(f)`；結果是 `Result<std::optional<值>, KernelError>`（f 回 `void` 時是 `Result<bool, KernelError>`：有沒有呼叫 f）。f 的回傳型別用 `requires` 擋 `Detail::PointsIntoWorld`（指標、參考、`Mut`、`std::reference_wrapper`、任何長度的 `std::span`、`std::ranges::view`、迭代器；`pair`／`tuple` 遞迴看每一項；const 也看）。Resource 的規則：讀擋 `KernelSettings` 與親和不合（S-15，新代碼 `UnreadableResource`）；寫 (b) 區 `Shared` 的與列出來的，(c) 區照 S-13，(b) 區沒列的單一寫者是 S-14（新代碼 `UndeclaredResourceWrite`）。列出來的是 System 的「寫 Resource」存取，D-E2、Z2、D-E18、親和、Registry 指紋都自動成立。兩條新的啟動檢查（D-E7，S-14）：獨占的 System 設了 ingress 的旗標；hook 用帶宣告的寫法。
3. **開局由 Kernel 跑**（第 12 項）：World 記 `startupRan_`、`startupRunning_`。`Product::Run(X)`：X 是 Startup → 斷言還沒跑過，記成跑過，跑；不是 → 有 Startup 而且還沒跑過時先完整跑一次（失敗就回傳、X 不跑、tick 不推進），再跑 X（tick 的推進在 `RunPlan` 的開頭，所以字面不用改）。新的啟動錯誤 D-E21（一個 World 第二張 Startup，規則 H-12）。
4. **錯誤碼**（第 13 項）：`KernelErrorCode` 最後加十個（`ScheduleRunning`、`UnknownSchedule`、`StartupNotNestable`、`StartupRunning`、`ExtractNotNestable`、`HookRunsSchedule`、`ClosureRunsSchedule`、`UndeclaredResourceWrite`、`UnreadableResource`、`NestedRunAborted`）；規則代號照現有的往下排，每一種拒絕一個：X-11～X-16、H-12（作者的 System 跑 Startup）、H-13（Startup 執行中跑別的）、S-14（沒列的單一寫者 Resource）、S-15（讀 `KernelSettings`）、S-16（讀親和不合的 Resource）、S-17（獨占的 System 兼 ingress）、S-18（hook 用帶宣告的寫法）。最初 H-13、S-16 併在 H-12、S-15，S-17、S-18 併在 S-14；Opus／xhigh 的複核指出和宣告第 4 節第 13 項「每一種拒絕的規則代號」不合，拆開（主會話確認可以）。
5. **指紋**（第 14 項）：執行計畫的格式 3→4、C 的 5→6；Registry 的版本不動、值跟著變。第 5.4 節的固定值換新值。
6. **測試**：新檔 `NestedRunTests.cpp`（J-56 巢狀執行、J-57 `ExclusiveWorld` 的讀寫、J-58 開局；含 J-10、J-15、J-50 各加的一案），`CMakeLists.txt` 加來源與 J-56～J-58 的 ctest 項。既有測試只動宣告第 5 節列的：J-45 的五個函式改寫成 `Read`／`Modify`；`SignatureTests`、`CoverageTests`、`ZoneRuleTests` 的簽名改成 `ExclusiveWorld<>`；第 5.4 節的指紋固定值（J-55 的兩句訊息文字跟著改成新版本號）。
7. **文件**：target `kernel.zh-Hant.md`（DI-KS 1、2、6、9、15 改寫；DI-KS 16～23 與 RC 15 新增）、`world.zh-Hant.md`、`limits.zh-Hant.md`（LIM-3）、舊規格 `spec.zh-Hant.md`（第 5.3 節列的各處，加 S-14、S-15、X-11～X-16、H-12、D-E21、J-56～J-58）、概念稿 E5 標拿掉、說明頁 `system-contract-page.zh-Hant.md` 第 3 節一列與第 11 節（三張表、重入與要避開的排法、閉包、開局）、`docs/cross-component-rules.md` 登記 CCR-2、`target/v2/README.md` 的索引。

## 和宣告不一致或待定

- **巢狀入口的「World 已中斷」檢查排在哪裡（待使用者判）**：宣告第 2 節第 8 點寫「中斷之後再呼叫 `RunSchedule`：不執行，直接回傳留下的那個錯誤」；判定 1「裡面丟出例外」的第二案要用「接住例外之後同一個 System 再呼叫，回傳的是留下的錯誤，而不是『正在執行中』」驗堆疊有沒有還原，這只有在「正在執行中」的檢查排在「已中斷」之前才分得出來。宣告沒定先後。現在的做法：先做各種拒絕，全部通過才看「已中斷」——第 2 節第 8 點只對合法的呼叫成立（已經中斷、又寫錯的呼叫拿到新的拒絕錯誤；錯誤處理仍然只呼叫一次）。已回報主會話，主會話列給使用者判；選「已中斷排第一」的話，要改的是入口那幾行、那一案的說法與 `ns-guard-not-popped-on-throw`（記成從外面看不到）。
- **宣告第 1 節的三段合成兩個提交**：`RunSchedule` 是新的 `ExclusiveWorld<…>` 的成員，所以「巢狀執行」與「新介面」在同一個提交（ad1045f7），開局在下一個（6b23e941）。PR 說明列了每一段在哪些檔、哪些函式。

## 宣告第 5.3 節、第 8 節與文件的對照

| 宣告的列 | 改在哪裡 |
|---|---|
| 概念稿 E5 拿掉 | `target/v2/rebuild-concept.zh-Hant.md` E5 那一列、§3.9 |
| 規格 E.2 第 1 步、規格 A 的狀態 | `plans/kernel/spec.zh-Hant.md` A.1 的 state（Running 變成一疊）、E.2 第 1 步（「或由獨占 System 發起」） |
| DI-KS 1、S-10、S-13（讀全部的例外、`Writes`、S-13 的理由句） | `target/kernel.zh-Hant.md` DI-KS 1；spec §讀法的 `KernelSettings` 那一句、S-10、S-13（理由句只對元件成立）、新 S-14、S-15、S-16 |
| DI-KS 2 | `target/kernel.zh-Hant.md` DI-KS 2 |
| DI-KS 6、規格 B.3、`Declarations.hpp` 的註解、spec `:261` | `target/kernel.zh-Hant.md` DI-KS 6；spec B.3「不算寫者」那一段；`engine/kernel/include/engine/kernel/Declarations.hpp` 的 `ScheduleOptions::startup` 註解 |
| 規格 D.2 第二張 Startup | spec D.2 的 8l（D-E21）、D-5 與 J-24 的範圍（D-E1～D-E21）；另加 8m（S-17、S-18） |
| 規格 E.2（`Run` 的步驟）、DI-KS 8、A 的 clock | spec E.2 第 0 步；A.2「推進 domain tick」那一列；DI-KS 8 的字面不變 |
| DI-KS 15 | `target/kernel.zh-Hant.md` DI-KS 15（啟停的例外；角色詞換成 runner） |
| DI-KS 9、規格 W-3、target `world` 第 41 行 | `target/kernel.zh-Hant.md` DI-KS 9；spec W-3；`target/world.zh-Hant.md` §3 那一條 |
| 規格 W-4、A.4、J-4 | spec W-4、A.4 新的一條、J-4 那一列；案例 J-56「句柄與 stamp」兩案、「裡面丟出例外」 |
| LIM-3、規格 B.5 | `target/limits.zh-Hant.md` LIM-3；spec B.5 新的一列 |
| target `world` DI 3、規格 E.2 的 stamp | `target/world.zh-Hant.md` §2 與 DI 3；spec A.1 的 stamp 與 X-5 |
| 規格 Z5 | spec Z5 那一列（重查的範圍加上外面每一層） |
| 規格 E.2（步驟的迴圈） | spec E.2 第 3 步 |
| 規格 E.5 | spec E.5 新的一條；`target/kernel.zh-Hant.md` DI-KS 17 |
| 規格 X-9 | spec X-9；新 X-11～X-16 |
| 守衛 J-35、J-50、J-10、J-15 | spec J.2 各列加一句；案例在 `NestedRunTests.cpp` |
| 守衛 J-54 與寫死指紋的測試 | 第 5.4 節的固定值（`RegistryTests`、`SchedulerTests`、`HookFlushTests`、`InitialValueTests`、`CoverageTests`、`SpawnRejectTests`） |
| 規格 D.2「同步點只在 stage 結尾與宣告的位置」 | 不變 |
| 規格 `:1308`（編輯器檢視器） | spec S-13 那一列的最後一句 |
| H-A1、H-9、DI-KS 12 | 不變 |
| 第 8 節八條 Design Invariants | `target/kernel.zh-Hant.md` DI-KS 16～23（新增的一節「巢狀執行」），各條寫了守衛的案例 |
| 第 8 節 Replacement Criteria | `target/kernel.zh-Hant.md` RC 15 |
| 第 11 節說明頁 | `target/v2/system-contract-page.zh-Hant.md` 第 3 節 `ExclusiveWorld` 那一列、第 11 節（11.1 誰能跑與拒絕、11.2 三張表與後果、11.3 重入與要避開的排法、11.4 閉包、11.5 開局） |
| 第 4 節「文件」：cross-component-rules | `docs/cross-component-rules.md` CCR-2 |
| 新的 H-13（宣告寫在 H-12 裡的那一條） | spec H.8 新的一列；`target/kernel.zh-Hant.md` DI-KS 22 |

## 開工前的兩個確認

- **只改兩個版本常數**（執行計畫 3→4、C 指紋 5→6）跑完整的 Kernel 測試：234 個案例 9 個失敗，失敗的斷言正好是宣告第 5.4 節的表（`CoverageTests:504`、`HookFlushTests:483-484`、`:883/885/887/890`、`InitialValueTests:717`、`:904`、`:906`、`RegistryTests:64`（固定值在 `:41`）、`SchedulerTests:171-172` ×3（固定值在 `:132-137`）、`:636`、`SpawnRejectTests:795-796`），沒有多也沒有少；反向連結的執行檔同樣。表上的行號差一行，是因為 doctest 報的是多行巨集的最後一行。紀錄：`build/evidence/gyo2-nested-schedule/precheck-kernel-tests.log`。
- **命名**（`docs/architecture/README.md`〈Naming〉→ `target/naming.zh-Hant.md`）：`Read<T>`／`Modify<T>` 是 Kernel 的概念（讀、改一個元件或 Resource），和查詢項 `Read<T>` 指的是同一件事（讀 T），沒有產品的詞，詞性是動詞；`Mut::Read()`、`ResMut::Read()` 已有同名的成員。`Writes<…>` 和 `Write<T>` 只差一個字，但打錯的兩個方向都變成帶標記的編譯錯誤（`ExclusiveWorld<Write<R>>` → `GYO_KERNEL_REJECT_EXCLUSIVE_DECLARATION`；`Query<Writes<T>>` → 既有的查詢項 `static_assert`），不會靜默誤用。結論：不改名（沒有需要使用者判的選項）。

## 檢證

（突變與 L1 的結果在跑完後補。）

- **指紋判定**（判定 1 最後一列）：把兩個版本常數換回 3、5 重算，第 5.4 節每一處都和舊的固定值相同（完整的 Kernel 測試在舊常數與舊固定值下全過，`impl/fingerprint-old-constants.log`）。舊的固定值裡沒有獨占 System 的 schedule，所以另外做了一個探針：一張 schedule 掛一個不帶宣告的獨占 System 與一個 (c) 區 System，用 `3743eaa8` 的 Kernel 與這一批（常數換回舊值）各編一次，三種指紋逐字相同（`fingerprint-exclusive/`）。

## 證據

全部在這台 Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-nested-schedule/`（git 不追蹤）：

- `precheck-kernel-tests.log`：只改兩個常數時的測試結果。
- `mutant-list-r0.md`：實作前定案的突變清單；`mutant-list-check-fable.md`：Fable／high 的對抗檢查。
- `fingerprint-exclusive/`：獨占 System 的指紋探針。
- `impl/`：建置、測試、ctest、突變的紀錄；`make_ns_mutants.py`、`rewrite_existing_mutants.py`（寫進 `mutations.json` 的腳本）。

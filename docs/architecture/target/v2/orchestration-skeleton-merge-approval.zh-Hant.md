# 合併前核准：編排層的骨架（`v8-arch-gyo2-orchestration-skeleton`）

日期：2026-11-18。分支 `claude/gyo2-orchestration-skeleton-67744c`。事前宣告：[orchestration-skeleton-declaration.zh-Hant.md](orchestration-skeleton-declaration.zh-Hant.md) 修訂 7（2026-11-18 動工前核准）。經過與證據：[dev_log](../../../dev_logs/2026_11_18_gyo2_orchestration_skeleton.zh-Hant.md)。

用詞：宣告正文的「段 1～段 4」＝「第 1～4 步」；「第二段核准」＝本頁的合併前核准。

**請你核准的**（宣告第 12 節「兩段核准」）：第 7.1 節的五組（下面第 2 節，已照 G-7 改寫）、第 7.2 節的 DI 守衛各列（第 3 節）、第 10 節 Design Invariants 的最終文字（第 4 節）、和宣告不同的地方（第 5 節）。被退回的組，合併前把那一組的改動撤回。**PR 在你核准之後才開。**

## 1. 判定的結果（宣告第 11 節）

| # | 結果 | 證據（主 checkout `build/evidence/gyo2-orchestration-skeleton/`） |
|---|---|---|
| G-1 | 本機 `ctest --preset test -L "cpu|shader"` 全過（macOS）；數字見第 6 節 | `segment4/ctest-segment4.log`、最終的 `ctest-final.log` |
| G-2 | 新案各有專屬突變；Red 的輸出存證據 | `segment1/red-stub.log`、`g8-defects-red.log`、`segment3/header-order-red.log` |
| G-3 | Kernel 的 19 個式子 0 命中；引擎 CMakeLists 的 `GYO::` link 0 命中；link 的集合逐一相同，只有 `gyo_threads` 的順序變了（第 5 節） | `segment2/g3_scan.log`、`segment3/g3_links.log` |
| G-4 | 編排層清單與 Kernel 清單的完整突變：見第 6 節 | `mutants-full-*.log` |
| G-5 | 守衛腳本的 Python 測試在 ctest 裡跑、全過 | 同 G-1 |
| G-6 | CI 的 L1 三平台：PR 開了之後跑（核准後） | — |
| G-7 | Fable 5.2／high 檢證：C、D、G、H、J 屬實；A、B、E、F 有出入，已照它改寫表、補了三個案（第 2 節） | `g7-verification-fable-high.md` |
| G-8 | 四次（第 1 步、第 3 步、第 4 步各一次；第 1 步之後的 D1～D4 修正）；每次找到的處數與處理在 dev_log | `segment1/`、`segment3/`、`segment4/` 的 `g8-check-fable-high.md` |

## 2. 第 7.1 節：有緩的面的五組（照 G-7 改寫後）

緩的面＝以前判失敗、之後判通過；緊的面＝以前判通過、之後判失敗。

### A 開局（DI-KS 22 的守衛）

1. **守的性質**：一個 World 至多一張 Startup；Kernel 在第一次跑別的 schedule 前、同一次 Run 裡、推 tick 之前跑完它一次；System 不能跑它、它執行中不能跑別的；主程式明確呼叫只准一次。
2. **緩的面**：一個 World 可以有多張「只跑一次」的 schedule；System 可以巢狀跑遊戲初始化的 schedule、遊戲初始化中可以巢狀跑別的；直接呼叫 `WorldExecutor::Run` 不會先跑遊戲初始化；第二個以後的 World 不再各自自動跑一張。**G-7 補的三項**：(a) 清單裡的某一張可以用 `RunSchedule` 或 `WorldExecutor::Run` 指名再跑、不斷言（「只准一次」守的是整份清單）；(b) 遊戲初始化的 schedule 可以帶 clock、可以推 tick（以前 Startup 帶 clock 是錯誤）；(c) 失敗時「tick 沒前進」與「傳回的錯誤內容」（`[ns-st-error-tick]`）不再有判定。舊 :1393 的 stamp 性質仍由 `ExecutorTests` J-30 守。
3. **緊的面**：清單的順序（含插入）；錨點不在、名字不是第一個 World 的 schedule 是 O-E5；遊戲初始化的 schedule 要屬於一個 domain——**這一項 G-7 指出沒有判定，已補案** `O-1: until batch 3 a game initialization schedule belongs to a domain (D-E12)…`（`CoverageTests.cpp`，突變 `or-gi-domain-unchecked`）。
4. **之後由誰守**：「整份一次、在第一幀與 `RunSchedule` 之前、只在第一個 World、明確呼叫只准一次、錯誤時不跑每幀」由編排層 O-1 的行為案例與 scenario 第一段（0cb54e05 提交）守。巢狀與 tick：不由機制守。
5. **根據**：`game-init-list`、`two-layers-like-bevy`；統合 7.2 J-58 列（HANDOFF 找得到）。
- **處置和表不同**：`NestedRunTests` :1486 是「拆」（表寫「改寫搬」）；`tests/common/kernel/CMakeLists.txt` 的 ctest `kernel.J-58` 隨 J-58 的案拿掉（表沒列）。

### B 登記單位的清單（J-19 的登記單位那半、J-24 的 R-E1／R-E3／R-E6；含 DI-KS 25、26 的守衛）

1. **守的性質**：清單裡名字重複是 R-E1、重複的不 build；`Requires` 缺或在後是 R-E3；兩個 runner 是 R-E6；build 每個一次、全部 build 完才 finish；`Plugin::Finish` 讀到版本 0；`Finish()` 只能是第一輪。
2. **緩的面**：不經編排層、直接用 Kernel 兩步登記的程式不受這些檢查；「`Plugin::Finish` 讀到版本 0」「`Finish()` 只能是第一輪」不再成立。**G-7 補的兩項**：R-E6 的報告點名兩個安裝者、之後一輪不再呼叫單位的 `Build`（`plugin.builds == 1`），這兩條不再有判定（O-E4 的報告點名安裝者的寫法不同）。
3. **緊的面**：依賴的來源改成登記檔，Module 依賴 Plugin 是 O-E3；`Finish` 讀得到 Plan 與版本 1；插入後不重跑 `Finish`——**G-7 指出原本的判定只比對兩個字串，已改成「插入之後到下一幀，紀錄是空的」**（`[or-insert-no-lifecycle]`，突變 `or-insert-reruns-finish`）；`Finish` 在 `BuildPlans` 之前跑的突變 `or-finish-before-plans` 也加了。
4. **之後由誰守**：編排層 O-2 的行為案例（O-E1、O-E3、O-E4、順序、一份報告收齊）。
5. **根據**：`module-deps-in-registers`、`module-plugin-registers`；統合 L5、L20、7.2 J-19、J-24 列。
- **處置和表不同**（G-7）：`CoverageTests` :130 留在 Kernel 就地改寫（表寫「搬」；編排層沒有在 `Finish` 裡讀集合的案，組合由 Kernel 的案守）；`RegistrationRoundTests` :836 改寫留著、只拿掉 builds／finishes 兩條（表寫「拿掉」）；`RegistryTests` :142 拆，lookup 那半留在 Kernel（表寫「搬」）；`RegistryTests` :251 留下的 R-E2／R-E4 段的 `errors.size() >= 4` 改成 `>= 2`（拿掉 R-E1／R-E3 兩條之後的計數）。

### C R-E7 的開局兩支（DI-KS 26 的守衛）——G-7：屬實

1. 之後一輪登記開局的 schedule、或在它上面掛 System，是 R-E7。2. 緩：沒有「開局的 schedule」，之後一輪登記任何 schedule 都照一般規則。3. 緊：無。4. 不由機制守（概念沒有了）。5. 同 A。

### E 開局的寫者豁免（DI-KS 6 的守衛）

1. 只掛在 Startup 的 System 不算寫者；Startup 列的 `Writes<…>` 不算。
2. **緩的面**（對實作）：把開局的 System 算成寫者的實作（突變 `rs-j15-startup-counted`，已退役）以前判失敗、之後判通過。
3. **緊的面**（對作者）：遊戲初始化與每幀都寫同一個 Exclusive 的欄，以前通過、之後是 D-E2（到第 4 條拿掉 D-E2 為止）——**G-7 指出沒有判定，已補案**（同 A 第 3 點那一案的後半，突變 `or-gi-writer-uncounted`）。
4. 不由機制守（Kernel 不認得開局；寫者數量本身在第 4 條拿掉）。
5. `two-layers-like-bevy`；統合 3.2 第 4 列（G-7 建議另加 `init-values-like-bevy`）。

### F 開局裡改 `KernelSettings`（DI-KS 24 的守衛）

1. 開局在同一次 Run 裡先跑時，它改的 `KernelSettings` 從下一次 Run 才生效。
2. 緩：把「開局改的值在同一次 Run 就生效」的實作（突變 `ks-s7-startup-change-same-run`、`ns-st-settings-after-startup`，已退役）之後判通過。
3. 緊：無。
4. **之後由誰守（改寫）**：「下一次最外層 Run 生效」由 J-50 的一般案守（`NestedRunTests` :1236、:1267，本批沒動）。編排層那一案（`[or-gi-settings]`）**只判「遊戲初始化每張是獨立的 Run，下一張讀得到上一張寫的值」**，G-7 指出它不判設定生效；案名與註解已照這個意思改，不再當作 F 的守衛。
5. `kernel-settings-read-write`。

## 3. 第 7.2 節：Design Invariants 的守衛各列（不論有沒有緩的面都要你核准）

| 組 | G-7 | 改寫 |
|---|---|---|
| D DI-KS 15 的碼清單（`RegistryTests`） | 屬實 | 理由改成「期待值隨定義變（等式比對，清單變短）」 |
| G hook 清空輪跑一次的 schedule（`HookFlushTests` :337、:754） | 屬實：只改 setup 一行與案名，斷言不變 | — |
| H 測試輔助改兩步 | 屬實：owner `{name, i}` 與舊 `Registry::Finish()` 逐欄相同，指紋只雜湊名字 | 補列 `SchedulerTests` 的 `BuildGolden` 與 D-E14 案、`SignatureTests` :260（同樣改兩步、斷言不變） |
| I 指紋與匯出（DI-KS 3） | 內容屬實，行號有出入 | 行號改成 `HookFlushTests` :510、:918；`SpawnRejectTests` :799；`CoverageTests` :519；`SchedulerTests` :137、:694（:618 沒有被改的判定） |
| J 符號掃描的自測試 | 屬實 | 「換成留下的名字」改成「換成合成的名字 `GYO::Kernel::Synthetic`」 |

## 4. 第 10 節：Design Invariants 的最終文字

- Kernel：[target/kernel.zh-Hant.md](../kernel.zh-Hant.md) 的 DI-KS 3、6、15、16～22 的前言、18（補「直接放實體不看配發器」一句，使用者已定，見第 5 節第 7 點）、22（拿掉，由 O-1 取代）、24、25、26、RC 7（移到編排層 RC-O1）、RC 13（c4098ab1 起的文字）。
- 編排層：新頁 [orchestration.zh-Hant.md](orchestration.zh-Hant.md) 的 O-1～O-10 與 RC-O1～RC-O9。第 3 步之後 O-6 的守衛欄寫上 `cmake.UnitRegistry`、`orchestration.units`。

## 5. 和宣告不同的地方

1. **D2 的讀法**：`RunFrame` 碰到空位或中斷的 World 就停下回傳，排在後面的 World 那一次不跑；我讀成「之後仍能用 `RunSchedule` 跑別的 World」（宣告 7.3 寫「`RunFrame` 回 `WorldMissing` 且別的 World 照跑」），實作沒改，案 `O-8: RunSchedule on an interrupted or empty World; another World still runs…` 守這個讀法。若你的意思是同一次 `RunFrame` 裡別的 World 也要跑，要改實作。
2. **G-3 的 link 順序**：十個引擎 target 的 link 集合和改之前相同；`gyo_threads` 的順序從 `GYO::Time;Threads::Threads;GYO::Os` 變成 `GYO::Time;GYO::Os;Threads::Threads`（登記的依賴一次 link 完，第三方的排到後面）。宣告寫「逐一相同」。
3. **Kernel 標頭改寫 `::Engine::`**（第 3 步找到的缺陷）：編排層是第一個 `GYO::Engine` 的 Module，先 include 它再 include Kernel 標頭會編不過；Kernel 公開標頭 49 處改寫 `::Engine::`（行為不變），加 include 順序的編譯守衛。之後的 `GYO::Engine` Module 在命名空間改名批之前都會碰到同一件事。
4. **`docs/architecture/target/README.md` 的 DI 清單沒改**：那張表是 v2 之前的（只有 DI-KS 1～15），照 `pre-v2-target-docs-later` 先不動。
5. **小處**：ctest 名照既有命名寫成 `cmake.UnitRegistry`（判定 1 寫 `cmake.unit_registry`）；突變執行器加 `{cmake}` 佔位符；`RunGameInit`／`RunFrame`／`RunSchedule` 回 `OrchestrationError`（宣告草圖寫 `KernelError`）；帶過去的值從中斷的新 World 取回時經讀取的檢視移出。
6. **第 2 步漏做、這次補上的**：Kernel 突變清單照第 9 節處理（退役 16、搬到編排層 12、改寫 16；清單的 `description` 寫明每一個去了哪裡）。G-7 順帶查到的。

7. **已定（第 4 步 G-8 提出的一件）**：scenario S1-7b 的佈置靠「外部的 `World::Insert` 不檢查配發器的範圍」，原本沒有條文。使用者 2026-11-18「照建議」（主會話轉達，`v8-arch-gyo2-direct-insert-no-allotter-range`）：在 DI-KS 18 補一句——直接放實體時不檢查、也不移動配發器的範圍，由作者負責。已補（[target/kernel](../kernel.zh-Hant.md#di-ks-18) DI-KS 18 的「直接放實體不看配發器」），S1-7b 改成引用它，它也列進 DI-KS 18 的守衛。

## 6. 數字

證據都在主 checkout 的 `build/evidence/gyo2-orchestration-skeleton/`（git 不追蹤）。

| 項目 | 結果 | 證據 |
|---|---|---|
| ctest（`test` preset，本機 macos-arm64） | 113 項全過 | `ctest-final2.log` |
| 編排層突變清單（`target/v2/scripts/orchestration_mutations.json`） | **179/179 殺** | `mutants-full-orchestration-cc5fe0e1.log` |
| Kernel 突變清單（`plans/kernel/scripts/mutations.json`） | 777 個：1 個這個平台跳過（`rs-sym-elf-bss-unread` 只在 Linux）；完整跑 769 殺、7 沒殺；本批造成的 4 個改寫後殺掉（`mutants-kernel-rewritten-4.log`）→ **773/776 殺** | `mutants-full-kernel-cc5fe0e1.log` |
| Kernel 清單沒殺的 3 個 | 基準就沒殺掉、不是本批造成（上一批 `v8-arch-gyo2-kernel-subtract-structure` 已記錄、建議另開）：`ks-real-product-name`（`engine/config/projects.csv` 只剩表頭，K1 已沒有產品名可比）、`rs-sample-capture-accepted`、`rs-sample-return-accepted`（J-10 的樣本變體因別的編譯錯誤失敗，不是預期的那一句） | 上一批 `gyo2-kernel-subtract-structure/base-a08c16f6-unkilled-11.log` |
| 本批造成、已改寫的 4 個 | `hf-startup-skipped`（Kernel 沒有 startup 旗標，改成「非時鐘的單次 schedule 不跑 hook 輪」）、`we-j1-insert-append`（Kernel 標頭改用 `::Engine::` 後 `find` 從 `::` 起）、`ks-s3-version-not-raised`、`ks-s3-failed-raises-version`（DI-KS 25 的案在第 2 步改名，`cases` 跟著改） | `mutants-kernel-rewritten-4.log` |
| 敵對式檢查 | 宣告修訂 0（Fable／high）：必改 10、建議改 16、小問題 8；G-8 第 1 步（Fable／high）：突變候補 56、測試弱點 13、疑似缺陷 5（D1、D3、D4 是缺陷，已先 Red 後修）；第 3 步 G-8：33 處；第 4 步 G-8：23 處；G-7（Fable／high）：A～J 中 C、D、G、H、J 屬實，A、B、E、F 有出入，已照第 2 節改寫 | 各步的 `g8-check-fable-high.md`、`g7-verification-fable-high.md` |

L1 三平台與 Windows／Linux 本機沒跑（要開 PR 才跑 L1；合併前核准之後才開 PR）。

## 7. 轉交（不在本批）

- **Plugin 的目錄一律 `add_subdirectory`**（第 3 步 G-8 的 D6）：第一個 Plugin 列（SDL、ImGui）進來的那一批要先決定「不用它的產品要不要 configure 它」。
- **`GYO::Engine` 與 `::Engine` 並存**：見第 5 節第 3 點；權威 Module 等下一個 `GYO::Engine` Module 動工前要知道。
- **第 7.4 節的延後守衛**：第 3 批開頭立（宣告的安排不變）。

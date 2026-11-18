# Kernel 減法・結構段（`v8-arch-gyo2-kernel-subtract-structure`）

日期：2026-11-17～18。分支 `claude/eloquent-joliot-aedd90`（從 `gyo2` a08c16f6 快轉）。第二段核准：使用者 2026-11-18 核准（宣告第 15 節開頭）；PR 開往 `gyo2`，合併等使用者。

這一批由任務卡做（GYO2 主會話開出）。事前宣告：[kernel-subtract-structure-declaration.zh-Hant.md](../architecture/target/v2/kernel-subtract-structure-declaration.zh-Hant.md) 修訂 2（cd553b2d）；使用者 2026-11-17 在主會話「照建議」核准第一段（HANDOFF `v8-arch-gyo2-kernel-subtract-structure-declaration-approved`）。宣告的修訂 0、1 各送 Fable／high 敵對式檢查與窄複查。實作者 Opus／high（三段＋兩次修正）；逐行複核 Fable／xhigh（使用者同意）；§14.4 檢證與 §14.2 檢查 Fable／high。

## 做了什麼

批次表第二段第 1 條：Kernel 減法裡「不需要新家」的部分。用遊戲講：兩個 System 都寫主角的血量，沒寫先後就照登記順序排、不報錯；除錯指令的 System 可以改 `KernelSettings`、下一次 Run 生效；讀檔與回滾可以讓 `ExclusiveWorld` 當場生成、插入、拿掉實體與增刪 Resource；將來的指令集可以在遊戲跑起來之後再登記 System。

| 段 | 提交 | 內容 |
|---|---|---|
| 段 1 拿掉 | 491bb528～c4e20705 | S1 `Product` 改名 `WorldExecutor`；S4 `ExclusiveWorld` 讀寫任何元件與 Resource 的值、`Writes<…>` 變成不計寫者的標註；S7 `KernelSettings` 每次 Run 讀一次、可讀可寫、驗證開關翻轉時重取快照、拿掉 D-E18；S8 拿掉執行期的攔截（F-10、Z5、W-6b），`Commands`／`Despawn` 改回傳 `void`；S9 拿掉 D-E8 與 `RequireExplicitOrder`；S10 拿掉執行緒的啟動檢查、改成綁執行緒的 Resource（記號後來改用 Base 的執行緒序號，e4074799）；S11 指紋改版（Registry 2→3、Plan 4→5、C 6→7、匯出標頭 2→3） |
| 段 2 登記 | 8730e948～3f721dda | S2 `Registry::Register(owner, build)`／`BuildPlans`／`DiscardRegistrations`；第一輪直接寫表、之後一輪寫暫存副本、成功時換內容（表物件位址不變）；World 在入口自己接上新版本；拿掉 System 留墓碑；之後一輪只准新東西（其餘 R-E7）；World 宣告准不准動態插入（`DynamicInsertRefused`）。S3 唯讀登記表加版本號、hook、`InitialValue`、schedule、集合、順序圖、effects、`annotatedWrites` |
| 段 3 當場改 | 1ef7432f～98e1bc38 | S5 `ExclusiveWorld` 的 `Spawn`、`SpawnWithId`、`Insert`、`Remove`、`Despawn`、`SetResource`、`RemoveResource`；觸發的 hook 當場跑、記的變更回傳前套用到空（使用者選 (a)）；繞過句柄改成員照樣斷言（使用者選 (a)，先比對再更新基準）；閉包與 hook 裡拒絕。S6 `Commands::SetResource`／`RemoveResource` 在同步點套用 |
| 第一次修正 | e172fa86～3bf480a8 | 段 2 逐行複核：前提斷言改成「這份 Registry 有 World 在跑就斷言」、R-E7 補「舊 Request」、補測試 |
| 第二次修正 | ac55aa12 | 段 3 逐行複核、G-7、G-8：例外離開當場改時 World 中斷（`ImmediateChangeAborted`）、當場 `Spawn` 拿到活著的 id 時回傳空、補齊守衛與突變 |
| 文件 | d8a72cb2 等 | `target/kernel.zh-Hant.md` 的 DI-KS 1～6、9、13～15、18～21、23～26 與 RC 3、14、16～18；`system-contract-page`；`docs/cross-component-rules.md` 三列；宣告第 5.6、14 節 |

## 驗證與經過

- 基準（0404888e，改程式之前）：`ctest --preset test -L "cpu|shader"` 99 項全過。
- 最後（ac55aa12）：98 項全過（拿掉 `kernel.strict_order`、`kernel.J-28`，加 `kernel.J-6_thread_bound_destroy`）；Kernel 的 doctest 377 案；守衛腳本 129 項通過；G-3 的 27 個掃描式全部 0 命中（每個式子先在 a08c16f6 命中過）。只在 macOS 上跑；L1 三平台要等 PR。
- 突變：清單 624 → 759。在 ac55aa12 完整跑一次：758 個中殺掉 747 個、1 個只在 Linux 跑（`full-mutation-run-ac55aa12.log`）。沒殺掉的 11 個在基準 a08c16f6 上逐個重跑來分類（`base-a08c16f6-unkilled-11.log`）：
  - 基準上也沒殺掉（不是這批造成，沒動）：`ks-real-product-name`（K1 的產品名清單已沒有 `ObjectFpsPvp`）、`rs-sample-capture-accepted`、`rs-sample-return-accepted`（J-10 樣本的變體以別的編譯錯誤失敗，不是預期的那一句）。
  - 這批的改名讓突變編不過，改 find／replace 後都殺掉（`fixed-4-mutants.log`、`fixed-2-mutants.log`）：`we-sym-tlcurrent-renamed`（S10 加了 `CurrentThreadToken`）、`ns-modify-resource-entry-stamp`（`Modify` 不再拿 `call`）、`rs-iv-fc3-step0-skipped`（phase 沒有 `pending` 了，改用「記錄來自 hook」認出第 0 步）、`sr-drop-is-err`（`kernelError` 輔助函式拿掉了）；這批自己加的 `ks-diks15-code-without-text` 與 `ks-s2-re7-without-text` 在 ac55aa12 把 `-Wswitch` 設成錯誤後編不過：前者改成預期編譯器拒絕（守住那個 pragma），後者改成文字變空、由 DI-KS 15 的測試抓。
  - **這批的設計讓突變活下來**（使用者 2026-11-18 定「都補測試」，aa6e65c0 補了，兩個都在預期的失敗下殺掉，`survivor-tests-2-mutants.log`）：`hf-product-limit-static`（S7 讓每次 Run 開頭都重讀 `KernelSettings`，靜態與每個 World 一份在現有測試裡分不出來；只有另一個 World 在這個 World 的 Run 裡面跑時才分得出）、`ns-rebase-no-retake`（S4 讓 `ExclusiveWorld` 宣告寫全部、事件除外，內層排程改的東西不再算到外層 System 頭上；只有內層寫事件時才分得出）。
- S10 修正（使用者 2026-11-18 選 (b)、改語義成 Base 的通用發號器；宣告第 14.1、14.5 節）：`engine/base/include/engine/base/IdIssuer.hpp`（`IdIssuer`、`CurrentThreadSerial`），Kernel 綁執行緒的記號改用執行緒序號。探針壓力跑 300 次、漏 0（修正前漏 20；`s10-fix/probe-stress-300.log`，檢查者另跑 300 次也是 0）。敵對式檢查（Fable 5.2／high）候選 28 個、殺 14，建議補的 11 處照做（Base 第 5 案查不為 0、J-6 加三案、J-56 新案加正例、記號守衛自測試加長得像的名字與兩種格式的正例），取 21 個候選進清單。最後：ctest 98/98、Kernel doctest 382 案、守衛腳本 130 項；S10／Base／記號守衛／兩個補測的突變 57 個中殺 56、1 個只在 Linux 跑、1 個因標頭註解變成 stale，改 find 後殺掉（`s10-fix/after-check-mutants.log`、`after-check-stale-fixed.log`）。突變清單 759 → 786。
- 檢查：段 2 逐行複核（必改 1）、段 3 逐行複核（必改 1）、G-7（沒有不成立的組，6 處文字更正）、G-8（找到 45 處，補齊）。處理都在宣告第 14.4 節。

## 停下的與待定的

- **S10 三件使用者 2026-11-18「照建議」**：核准 FOREIGN 加 Base 的兩個名字、核准 DI-KS 14 改寫、Kernel 只准用 `CurrentThreadSerial`。S10 提交 e4074799；K8（`kernel_guard.py`）與 DI-KS 14 的條文在下一個提交。K8 的 §14.2 檢查（Fable 5.2／high）找到 19 處，建議的已補；kernel_guard 的突變 71 個全部殺掉；最後 ctest 98/98、守衛腳本 144 項、G-3 27 式 0 命中（`k8/`）。突變清單 → 802。
- **當場 `Spawn<C…>` 遇到還活著的 id**：使用者 2026-11-18「參考Go的成功和錯誤回傳，不要回空」。回傳型別回到 `Result<EntityId, KernelError>`，回錯誤碼 `AllottedIdAlive`（新碼；只交給呼叫的 System，`Detail::CallerError` 不記進這次呼叫的錯誤，所以 Run 不以它結束、World 不中斷），`RejectedSpawn` 照舊記（比照同步點丟掉的生成，F-13）。DI-KS 18 的那一句、System 契約的說明頁、宣告第 3.5、14.2、15.5 節跟著改。測試改寫 1 案、加 3 個突變、更新 1 個，相關突變 52 個全部殺掉（`spawn-alive-err/`）。
- **第二段核准**（2026-11-18 使用者「核准」）：材料整理在宣告第 15 節（判定 G-1～G-8 的狀態、九組的緩的面與 G-7、DI 守衛、DI 最終文字、實作中新出現的）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-kernel-subtract-structure/`（git 不追蹤）：偵察清單 `test-inventory.md`、`mutant-guard-inventory.md`；宣告的檢查 `declaration-r0-adversarial-check.md`、`declaration-r1-narrow-check.md`；基準 `baseline-*.log`；各段的 Red／Green 與 ctest 在 `segment1/`、`segment2/`、`segment3/`、`segment2-fix/`、`fix2/`；逐行複核 `segment2-line-review-fable-xhigh.md`、`segment3-line-review-fable-xhigh.md`；`g7-verification-fable-high.md`；`g8-segment1-inspection-fable-high.md`；突變 `full-mutation-run-ac55aa12.log`、`base-a08c16f6-unkilled-11.log`、`fixed-4-mutants.log`、`fixed-2-mutants.log`、`survivor-tests-2-mutants.log`；S10 修正 `s10-fix/`（探針壓力、敵對式檢查 `adversarial-check-fable-high.md` 與候選、突變、ctest、新舊記號守衛的比較 `symbols-old-vs-new.log`）；K8 `k8/`（檢查報告、候選、突變、ctest、G-3）。

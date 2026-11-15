# 同步點丟掉不能生成的那一筆（`v8-arch-gyo2-spawn-reject`）

日期：2026-11-15。分支 `claude/gyo2-spawn-reject-da0c9b`（從 `gyo2` `bd11575a` 開出）。PR [#235](https://github.com/yojinn-io/GYO-Engine/pull/235)（目標 `gyo2`）。

這一批由任務卡「v8-arch-gyo2-spawn-reject」做（GYO2 重建主會話開出）。事前宣告：[spawn-reject-declaration.zh-Hant.md](../architecture/target/v2/spawn-reject-declaration.zh-Hant.md)（修訂 2，使用者 2026-11-15「核准」）。

## 做了什麼

1. **三條斷言改成丟掉**：同步點第 4 步原本的三條 `GYO_ASSERT`（一個 id 在一次套用被生成兩次；id 還活著；`SpawnWithId` 的 id 落在配發器的範圍）改成「丟掉那一筆、留 `RejectedSpawn` 明細與總數，不是錯誤，`Run` 照常」（規格 F-13）。每筆依「落在配發器的範圍（只對 `SpawnWithId`）→ 這個同步點生成了兩次 → id 還活著」取第一個成立的理由。
2. **重複的規則**（使用者重判）：同一次套用裡同一個 id 的紀錄扣掉落在範圍的之後，來自同一個記錄者（同一個 System 的一次呼叫，或同一個 hook）時留第一筆，來自兩個以上的記錄者時全部丟掉——結果不隨互不衝突的 System 的登記順序變（DI-KS 5 不用改）。記錄者用 `Record::emitterKey`（System 與 hook 各自的存取描述的指標，只比相等）判斷，因為 hook 的名字沒有唯一性。第 0 步（上一窗 hook 記的）生成過、第 1～7 步又生成而且仍活著時，後者丟掉、理由「生成了兩次」。
3. **排序鍵**：第 4 步改成依 `(EntityId, planPos, recSeq)` 排序，並照規格 F.6 斷言相鄰鍵不相等（原本只照 id 排序、不穩定）。
4. **明細與總數**：`RejectedSpawn`（entity、reason、emitter、emitterIsHook、emitterZone、fromAllotter、schedule、step、appliedAt、tick、otherEmitter）放進 World 現有的診斷紀錄，runner 在兩次 `Run` 之間以 `Diagnostics<RejectedSpawn>()`／`TakeDiagnostics<RejectedSpawn>()` 讀；`World::RejectedSpawns()` 一生只增不減。不設上限；World 中斷後保留；不叫 break 掛鉤。`DiagnosticsWriter<RejectedSpawn>` 由 `static_assert` 擋下（產品的紀錄不混進 Kernel 的明細）。
5. **指紋**：執行計畫指紋版本 2→3、C 指紋 4→5（慣例 `v8-arch-gyo2-hook-flush-not-in-plan-bump-versions`）；Registry 指紋版本不變（值因含執行計畫指紋的文字而變）。
6. **測試**：新檔 `SpawnRejectTests.cpp`（J-55，36 個案例、含子案例；其中 14 個是突變清單的對抗檢查之後補的，1 個是主會話審查之後補的）；`CMakeLists.txt` 加來源與 `GYO_KERNEL_J_ENTRIES` 的 J-55（不加的話 `kernel.others` 排除 `J-*`，J-55 不會在 ctest 跑）；`compile_fail` 加 `KERNEL_DIAGNOSTICS` 變體。
7. **守衛的判定變更**（AGENTS §14.4，使用者核准）：J-32 子案例「往活著的編號生成」與 J-8 裡「配發器範圍」那一段，從「期待斷言」改成「期待丟掉、舊實體不動、明細與總數」；J-8 改用配發器 `[149, 151]`，之後配發器依序配到 149、150、151。指紋的 8 個固定值換新值（判定不變）。
8. **突變**：`mutations.json` 加 `sr-*` 81 個（照宣告寫的與實作時補的 55 個，對抗檢查之後補的 25 個，審查之後補的 1 個）；拿掉 `we-adv-spawn-alive-assert-removed`、`we-adv-spawnwithid-allotter-range-assert-removed`（`find` 是被拿掉的斷言，由 `sr-assert-alive`、`sr-assert-range` 取代）；`rs-b6-cfp-version-1`、`hf-plan-version-kept`、`hf-c-version-kept` 的 `find`／`expect` 照新版本重寫。
9. **文件**：規格（A.1、H.2b 第 4 點、F.2 第 4 步與「結果」、F.6、F-13（新）、J-49 的突變說明、J-55（新））；target `kernel.zh-Hant.md` DI-KS 9 加一句、`world.zh-Hant.md`、`limits.zh-Hant.md` LIM-7（新）；說明頁第 10 節「生成被丟掉的時候」與第 6 節一句；`target/v2/README.md` 的索引。

## 使用者在卡片裡的決定（都已轉給主會話記進 HANDOFF）

| 件 | 定的 |
|---|---|
| 同一個同步點生成兩次（重判；原決定「留排在前面的」會讓結果隨登記順序變，碰到 DI-KS 5） | 同一個記錄者留先記的，不同記錄者全部丟掉 |
| 被丟掉的分類 | 結果，不是錯誤（system-contract §2b「兩種機制」不變） |
| 用配發器生成撞上 | 照樣丟掉，明細加標記 |
| 同一次呼叫的插入與移除 | 照常落在活著的那個（說明頁寫明「生成時的值放進 bundle」） |
| 明細的上限 | 不設上限，runner 取走 |
| (b) 區擋住 (c) 區 | 照樣丟掉，寫明限制；說明頁分 PvP（有權威端）與單機兩段；設計思想帶過來、不照搬舊架構的規格 |
| 限制要不要登記 | 登記成 LIM-7 |
| 明細的形狀 | 照宣告 3.1 |
| 條文與 §14.4 的五點、指紋固定值 | 核准 |

另外：使用者指正「Host 是什麼不是廢棄了麼。」——宣告與新寫的文件、註解照用詞表 v3 用「組裝（setup）」與「runner」。

## 檢證

- **本機（macOS；`uname -m` 是 x86_64）**：完整 `ctest` 96 個全過；`gyo_kernel_tests` 234 個案例、`gyo_kernel_tests_reversed` 全過；`compile_fail` 的樣本在設定時跑（`KERNEL_DIAGNOSTICS rejected with GYO_KERNEL_REJECT_KERNEL_DIAGNOSTICS`）。
- **指紋**：只改兩個版本常數時失敗的位置與宣告第 4 節的表一致（宣告的檢查者實測；實作後第一次跑也是同樣 8 個案例失敗、其他全過）。
- **突變**（照 `mutation-rerun-scope`）：新的 `sr-*` 55 個；`World.cpp`、`Executor.cpp`、`Fingerprint.cpp` 的全部（三條斷言以前是別的突變的隱性保險，照 `find` 的位置挑會漏）；被改到的測試抓的（J-8、J-18、J-20、J-32、J-54、初始值的指紋、`ExportPlans`）；`rs-iv-fb5-sort-tiebreak-dropped`。共 198 個：197 killed、1 wrong_failure（`sr-range-on-spawn`：前一個子案例的 `REQUIRE` 失敗中止了整個案例，預期那一句沒跑到）。把那個子案例拆成獨立的案例（判定不變）後，它與同一組 6 個重跑全部 killed。
- **突變清單的對抗檢查**：Fable／xhigh（唯讀子代理，在自己的 worktree 跑實驗；報告 `impl/mutation-check-fable.md`）。存活的缺口 7 項（10 個突變存活）、expect 問題 6 項、漏做或和宣告不同 6 項、測試的錯 2 項。照報告處理：
  - **實作不合宣告（G-7）**：同一個 id 上，「落在範圍」的那筆明細會排在計畫位置更前面的那筆之前；宣告 3.1 寫的是依 `(id, planPos, recSeq)`。改實作：每個 id 的每筆先定結果，再依排序鍵寫出明細。順帶拿掉檢查者指出的死碼（`spawnedHere` 對同一個 id 的更新，之後沒有人讀）。
  - **存活的缺口**：補案例——「第 0 步被丟成『還活著』的那筆不算『生成過』」（宣告列了、我先前說等價的那個，跨兩次套用時不等價）、「兩個 System 三筆」（兩種登記順序）、「hook 依 A、B、A 的順序被叫」、「第 0 步生成過較大的 id 時，活著的較小 id 仍是『還活著』」、「不是 Tick、不是 Fixed 的 schedule 的 tick 與 schedule」、「同一個 id 的明細順序」、四種理由各一個「不過初始值、不觸發 OnAdd」、「兩個 (c) 區 System 的 bundle 不相交時不衝突、相交時衝突」、「登記順序那一句單獨一個案例」、「第 0 步生成過、窗裡先刪再生，另一個 System 也生成」（宣告判定 1 原本那一列的後半，照最後的規則兩筆都丟、11 消失）——並補對應的突變。
  - **expect 問題**：斷言型突變的 expect 加上案例名；`sr-spawned-here-kept` 的 cases 縮到那一個案例（不再靠讀已銷毀的 Registry）；`sr-two-emitters-keep-first` 改指「登記順序」那一句；「配發器的範圍」那一組子案例拆成各自獨立的案例（一個 `REQUIRE` 失敗不再擋住後面）。
  - **漏做**：「被丟時觸發 OnAdd（只加 OnAdd）」併進 `sr-dropped-cells-added` 的說法不成立，另加 `sr-dropped-onadd-only`；「被丟的那筆算進『生成過』」加 `sr-dropped-counts-as-spawned`；改寫後的 J-32、J-8 守衛自己的句子補了 8 個突變（`sr-j32-*`、`sr-j8-*`）。
  - 檢查者也確認了：兩個 System 的案例真的互不衝突、計畫順序是登記順序、「跨兩次套用」確實走第 0 步。
  - 改實作與加案例之後的突變重跑：`sr-*` 80 個加 `World.cpp` 的既有突變，共 113 個，113 killed（`mutants-run3.log`；`Executor.cpp`、`Fingerprint.cpp` 與其他相關的既有突變在第一次重跑時已全部 killed，這次的改動只在第 4 步）。
- **L1 三平台**：（補）

- **主會話的審查（PR #235）**：沒有擋合併的問題。兩點已處理：(1) 效能——`ApplySync` 不再每個同步點查一次 schedule，tick 只在第一次要記明細時才查；(2) 「第 0 步兩個 hook 撞號（兩筆都丟）、窗裡一個 System 又生成同一個編號」——現在窗裡那筆照常生成。列給使用者兩個選項（依套用分開判／整個同步點一起判），使用者問「現實的遊戲實作中，會有人寫這種代碼而不會發現嗎？Engine提供的是機制。」之後定：「對，就這樣。如果規則訂太死，Engine就失去了活力。遊戲類型很多，而且方向也多，我不能保證限制太死了，我在做其他遊戲時能否撐住。」→ 依套用分開判；DI-KS 9 的「同一個同步點裡」改「同一次套用裡」並寫明兩次套用各自判；補案例；說明頁寫明。之後 `sr-*` 81 個加 `World.cpp` 的既有突變共 114 個重跑，114 killed（`mutants-run4.log`）。

## 經過

- 宣告：初稿（修訂 0）→ 核准前的試做（不提交，§14.4 要求的突變對照）→ 對抗檢查（Fable／xhigh：blocker 3／should 13／nit 8）→ 修訂 1 → 使用者逐件判（八件）→ 修訂 2 → 核准。
- 對抗檢查的三個 blocker：「留排在前面的那筆」在互不衝突的 System 之間依登記順序（碰到 DI-KS 5）；第 4 步漏了規格 F.6 的相鄰鍵斷言（沒有它，「排序鍵漏 `recSeq`」「只看編號」兩個突變抓不到）；§14.4 的放寬面漏列四類。
- **和宣告不一致的地方**（照實記）：
  1. 判定 1「跨兩次套用，第二次先刪」原本另有一個「在它後面的 System 再用 11 生成」，期待留先刪再生的那筆。使用者在 2.2 選了「不同記錄者全部丟掉」，那樣排兩筆都會被丟。拆成兩個案例：先刪再生的那筆單獨生效；加上另一個 System 時兩筆都丟、11 消失、互相記對方（對抗檢查指出後補的）。
  2. 判定 2 的「先刪再生之後『留下的那筆』不更新」在實作的寫法下確實等價（那個值之後沒有人讀；對抗檢查確認），那一行改實作時拿掉了。我原本也說「被丟的那筆算進『生成過』」等價，**是錯的**：同一次套用裡等價，跨第 0 步與窗不等價（對抗檢查實測存活），補了案例與 `sr-dropped-counts-as-spawned`。
  3. 「被丟時觸發 OnAdd（只加 OnAdd、不加初始值）」我原本說併進 `sr-dropped-cells-added`，**說法不成立**（兩者預期失敗的句子不同）；補了 `sr-dropped-onadd-only`。
  4. 「排序鍵只看編號」宣告事前寫了「存活就記為等價」的豁免；補上相鄰鍵斷言後它被斷言抓到（`sr-key-id-only`），豁免沒有用到。
  5. 「兩張 schedule」案例併進「玩家 7 與玩家 8」：每個 rig 先登記 `Early` 再登記 `Tick`，所以「寫成第一張登記的 schedule」由那個案例的 schedule 那一句抓（`sr-schedule-first` killed）。
  6. 「兩個不同的 hook」案例讓兩個 hook 用同一個名字（hook 的名字沒有唯一性），才抓得到「用名字判斷記錄者」（`sr-emitter-by-name`）。
  7. 「兩個不同的 System」案例多檢查明細依計畫的順序，才抓得到「`recSeq` 排在 `planPos` 前面」（在「不同記錄者全部丟掉」之下，留哪一筆不再分得出順序）。
  8. 判定 1 的「範圍只查 `SpawnWithId`」原本是「配發器的範圍」案例的子案例，突變跑出 wrong_failure 後拆成獨立的案例（判定不變）。

## 卡片留下、交給主會話的事

1. 突變清單裡 `ks-links-extra`、`ks-adv-include-allowlist-widened`、`ks-k5-parent-directory`、`rs-j17-zone-b-listed` 的 `find` 在這一批之前（`bd11575a`）就已經對不上（查過 base 上的檔），不是這一批造成的，沒有修。
2. 回滾那一批要重看兩件（宣告第 9 節、LIM-7）：重放的生成撞上帶 (b) 區元件的活實體；明細的 `rollbackActive` 這一批固定 `false`，重放時明細與總數會重複。
3. 讀檔還沒設計：用 `SpawnWithId` 把單機存檔的實體放回去會整批被丟（編號都在配發器的範圍），讀檔設計時要定怎麼放回去。
4. K0 宣告第 6 節「外層刪除後裡面用同一個編號生成」那個案例的期待，照這一批的行為寫（主會話已知）。

## 證據

全部在這台 Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-spawn-reject/`（git 不追蹤）：

- `spike/`：核准前的試做（`spike.patch`、`spike_mutants.py`、`spike-mutants.log`）。
- `declaration-check-r1-fable.md`、`check-r1-experiments/`：宣告的對抗檢查。
- `impl/`：建置與測試（`configure1.log`、`build1.log`～`build6.log`、`kernel-tests1.log`～`kernel-tests3.log`、`reversed2.log`～`reversed4.log`、`ctest1.log`～`ctest3.log`）；突變（`make_sr_mutants.py`、`rerun-ids.txt`、`rerun-ids-2.txt`、`rerun-ids-3.txt`、`mutants-run1.log`～`mutants-run4.log`）；突變清單的對抗檢查（`mutation-check-fable.md`、`mutation-check-fable-experiments/`）。

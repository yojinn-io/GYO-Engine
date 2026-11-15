# hook 的紀錄在 schedule 跑完時套用（`v8-arch-gyo2-hook-flush`）

日期：2026-11-15。分支 `claude/gyo2-hook-flush-fa1569`（從 `gyo2` 3a06056b 開出）。PR [#234](https://github.com/yojinn-io/GYO-Engine/pull/234)（目標 `gyo2`）。

這一批由任務卡「hook 的紀錄在 schedule 跑完時套用」做（GYO2 重建主會話開出）。事前宣告：[hook-flush-declaration.zh-Hant.md](../architecture/target/v2/hook-flush-declaration.zh-Hant.md)（修訂 3，使用者 2026-11-15「三組都核准，開始實作」）。

## 做了什麼

1. **清空輪**：計畫的最後一個同步點之後（清舊事件在預設位置時，排在它之前），執行器把這張 schedule 的 hook 佇列一輪一輪套用到空。每一輪是一次完整的套用（走同步點的同一段 `phase`：Z5 的重查、刪除、移除、生成、插入、漏斗的補值、OnAdd、OnRemove），取一個新的 stamp，`syncEpoch` +1。清空輪不是計畫的一步。實作在 `Executor.cpp` 的 SyncPoint 分支（`position == lastSync`）。
2. **上限兩層**：`KernelSettings::hookRoundLimit`（產品的預設，每個 World 一份，預設 8，第一次 `Run` 快取，0 是 GYO_ASSERT）；`ScheduleOptions::hookRoundLimit`（每張 schedule 另外指定，`std::optional`，0 是啟動錯誤 D-E20）。碰到上限回 Err `HookRoundLimit`（規則 F-12）：World 中斷、剩下的丟掉、已套用的輪不退回、這次 `Run` 還沒做的步驟不做。訊息點名 schedule、上限與它來自哪一層、剩下的筆數、最後一輪記紀錄的 hook（依登記順序、去重；`World` 的 `roundRecorders_`）。
3. **兩個新的不變量的斷言**：`Run` 開頭「這張的 hook 佇列是空的」；`Run` 結尾「`window_` 與這張的 hook 佇列都是空的」。
4. **指紋**：執行計畫指紋版本 1→2、C 指紋 3→4，兩者雜湊 schedule 指定的上限（有沒有指定、值）；Registry 指紋版本不變（它含每張計畫指紋的文字、`KernelSettings` 的大小，所以值會變）；`ExportPlans` 格式 1→2，每張 schedule 多一行 `hook-round-limit`。`Plan` 多一個成員 `hookRoundLimit`。
5. **測試**：新檔 `HookFlushTests.cpp`（J-54，26 個案例：21 個照宣告寫的，5 個是突變清單的對抗檢查之後補的）；改寫 J-34 第 2 子案例、J-52 第 2 與第 5 子案例，新增 J-52「單一 stage 的刪除在同一次 `Run` 的清空輪套用」子案例。
6. **守衛的固定值**（AGENTS §14.4，使用者核准；判定不變、只有期待值變）：`RegistryTests.cpp:41`、`SchedulerTests.cpp:129-134`（計畫指紋 `1:`→`2:`、Registry 指紋）、`:632`（`gyo.kernel.plans 2`）、`:633`（`fingerprint 2:`）、`InitialValueTests.cpp:904`、`:906`（C 指紋 `3:`→`4:`）、`:717`、`CoverageTests.cpp:504`（C 版本 `== 4U`）。
7. **突變**：`mutations.json` 加 `hf-*` 69 個（照宣告寫的 41 個、對抗檢查後補的 29 個，扣掉被換掉的 `hf-limit-names-call-order`）；從 `claude/arch-kernel-world-exec`（`50cb2050`）搬 `we-*` 39 個（原文落在這一批改到的函式裡的 38 個，加 `we-j50-settings-writable`；`we-j34-hook-immediate` 的 replace 照 gyo2 的 `queue.records` 重寫，`we-j7-despawn-reverse` 的 find 改到銷毀的迴圈）；`rs-b6-cfp-version-1` 照 C 指紋 4 重寫。
8. **文件**：規格（§讀法、A.1、A.2、A.3、W-1、D.1、D.2 第 8i／8k 步、D-5、E.1、E.2、E.5、X-5、F.1、F.2 第 0 步、F.3、F.4、F-1、F-4、F-6、F-9、F-12（新）、F.7 兩列、F.8 取捨 2、J-24、J-33、J-34、J-36、J-54（新））；target `kernel.zh-Hant.md`（DI-KS 9、RC 14 新）、`world.zh-Hant.md`（本文 stamp 與套用順序、DI 3、DI 5）、`limits.zh-Hant.md`（LIM-6）、`system-contract.zh-Hant.md`、`scheduler.zh-Hant.md`、`target/README.md`；`plans/kernel/mechanism.zh-Hant.md`；說明頁新增第 9 節「hook 記的變更什麼時候生效」（每幀生成這一種不固定、有代價的三個做法）；`Product.hpp`、`Plan.hpp` 的註解。

## 檢證

- **本機（macOS；`uname -m` 是 x86_64）**：完整 `ctest` 全過；`gyo_kernel_tests` 198 個案例、`gyo_kernel_tests_reversed` 全過。數字與 log 見〈證據〉。
- **突變**：這一批相關的 117 個（`hf-*` 41、`we-*` 38、`Fingerprint.cpp` 的 24、漏斗的 5、`rs-iv-fb5-sort-tiebreak-dropped`、被匯出案例抓的）跑了一次：114 killed。沒抓到的三個——
  - `hf-schedule-limit-ignored`：wrong_failure。原因是測試寫法：前面的 `REQUIRE` 先停掉子案例。把那一句改成帶同一個專用訊息的 `CHECK_MESSAGE` 後重跑，killed（同時重跑 `hf-limit-larger-of-two`，killed）。
  - `we-j7-despawn-reverse`：存活。對抗檢查查出原因：在 gyo2 上它的 `find` 命中的是 despawn 時「先對每個元件叫 OnRemove」的迴圈，不是銷毀的迴圈。把 `find` 改到銷毀的迴圈之後被 J-7 抓到。原本打到的那個性質（despawn 的 OnRemove 依 ComponentId 遞增）在 gyo2 上沒有測試守，是既有的缺口，交主會話。
  - `rs-j17-zone-b-listed`：stale。它的 `find` 在這一批之前（`3a06056b`）的 `Fingerprint.cpp` 就已經找不到，不是這一批造成的。
- **突變清單的對抗檢查**：Fable／high（唯讀子代理，在自己的工作樹跑實驗）。存活的缺口 12 項（17 個突變）、expect 問題 8 項、漏做 5 個（報告 `impl/mutation-check-fable.md`）。照報告處理：
  - 存活的缺口：補 5 個案例（schedule 指定值碰上限的訊息與輪數、只列最後一輪有記紀錄的 hook、依登記順序、break 掛鉤恰好一次、錯誤碼的文字；第 2 輪出錯的上下文層；`KernelSettings` 被移除時的預設 8；有指定值時三種指紋的固定值與 C 指紋的「指定 8 ≠ 沒指定」、匯出那一行的位置；D-E20 的文字與點名），加 17 個對應的突變。
  - 漏做：檢查者把宣告列了、清單沒有的 5 個寫出來並跑過，全部被現有的案例抓到，加進清單（含「清空排到清舊事件之後」`hf-trim-before-rounds` 與「輪的套用略過某一步」`hf-round-skips-*`——初稿的 dev_log 說寫不出來，是錯的）。另加檢查者寫的幾個（`hf-plan-limit-not-carried`、`hf-default-limit-nine`、`hf-limit-check-before-empty`）。
  - expect 問題：`we-j52-*` 三個改用專用訊息（J-52 的 SpawnWithId 子案例加一句訊息，判定不變）；七個 `we-*` 改用子案例或案例的名字；兩個改用失敗的那一句的文字；`hf-schedule-limit-ignored` 改成只對應一句；`hf-limit-names-call-order` 換成真的測「順序」的 `hf-limit-names-first-call-order`。
  - 被 `Run` 結尾的斷言抓到的 7 個 `hf-*`：清單的 `what` 註明；檢查者把兩個斷言拿掉重跑，7 個都被案例自己的判定抓到（`noasserts.log`）。
  - 新增與改了字串的 44 個重跑：結果見〈證據〉的 `mutants-run3.log`。
- **L1 三平台**：開 PR 後跑，結果寫在 PR。

## 經過

- 宣告：初稿（修訂 0）→ 第一輪對抗檢查（Fable／high：blocker 4／should 11／nit 9）→ 修訂 1 → 使用者在分支會話定了「放在哪裡」與「上限兩層」→ 修訂 2 → 第二輪對抗檢查（Fable／high：blocker 1／should 9／nit 8）→ 使用者「照建議」（不送第三輪）→ 修訂 3 → 使用者核准。
- 宣告之前做了試做（不在本卡的分支），找出既有測試只有 3 個子案例會變（`build/evidence/gyo2-hook-flush/spike/`）。
- **和宣告不一致的地方**（照實記）：
  1. 宣告說突變清單在實作前定案；實際是先寫測試與實作，之後整理清單。宣告第 6 節預先列的「至少包含」都做了，另外加了宣告沒列的幾個（`hf-mid-records-wait-for-rounds`、`hf-round-position-plan-size` 等）。
  2. 宣告判定 1「一輪是完整的套用」第 (5) 項「Resource 的插入」寫不出來：hook 沒有能記 Resource 延後操作的參數（`Params.hpp` 裡沒有記 `ResourceInsert` 的介面）。Resource 的操作走同一段 `phase` 的第 7 步，沒有另寫的程式。
  3. 宣告判定 2 列的突變裡，「跨輪合併成一次套用」寫不出單一處的改動，沒有做。其餘都做了（初稿漏了 5 個，對抗檢查後補上）。
  4. 新的三個指紋固定值（有指定上限時的執行計畫、C、Registry 指紋）是在這台機器算的，要由 L1 三平台確認相同。
- 測試在寫的過程中修了三處，讓對應的突變認得出來：「每個 World 一份」的案例先跑另一個 World（否則抓不到「預設存成行程共用」）；「輪裡出錯」的案例檢查 hook 那一層的步驟號（抓「位置改用計畫的長度」）；`Run` 結尾的斷言和開頭的寫法不同（失敗時分得出是哪一個）。

## 卡片留下、交給主會話的事

1. K0 要補「裡面那張的執行不套用、也不丟掉外層 schedule 的 hook 佇列」的案例與兩個突變（宣告 3.5 第 6 點）；在那之前這個性質沒有測試守（使用者核准時知道）。
2. HANDOFF `v8-arch-gyo2-hook-flush-limit-two-levels` 裡「`KernelSettings` 整個行程共用一個值」要訂正為每個 World 一份（已轉給主會話）。
3. `we-*` 突變裡不在這一批範圍的 52 個沒有搬進 gyo2；despawn 的 OnRemove 依 ComponentId 遞增在 gyo2 上沒有測試守（既有缺口，對抗檢查發現）；`rs-j17-zone-b-listed` 在 gyo2 是 stale（既有）。要不要處理交主會話判斷。
4. 每幀生成這一種（K0 之後才會出現）要不要登記成 LIM，建議由 K0 決定。

## 證據

全部在這台 Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-hook-flush/`（git 不追蹤）：

- `spike/`：宣告前的試做（diff、測試與突變的 log、腳本）。
- `declaration-check-r1-fable.md`、`check-r1-experiments/`；`declaration-check-r2-fable.md`、`check-r2-experiments/`：宣告的兩輪對抗檢查。
- `grep-cross-run-dependency.log`：「跨 `Run` 保留」的依賴搜尋。
- `impl/`：實作後的建置與 ctest（`build.log`、`ctest.log`、`build2.log`、`ctest2.log`、`reversed.log`、收尾的 `ctest3.log`）、突變（`mutants-run1.log`、`mutants-run2.log`、`mutants-run3.log`）、突變清單的對抗檢查（`mutation-check-fable.md` 與 `mutation-check-fable-experiments/`）。

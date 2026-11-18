# Kernel 突變清單的盤點與退役（`v8-arch-gyo2-mutation-prune`）

日期：2026-11-18～19。分支 `claude/gyo2-mutation-prune-19c247`（從 `8b912a1e`；第 2 步前合併 `origin/gyo2` `f48af791`，即第 2 批 PR #244 合併後）。**狀態：第 2 步完成（22/22、ctest 113/113），PR 待 L1 與合併；之後做 subsumption 檢查（不改清單）。**

這一批由任務卡做（GYO2 主會話開出，使用者原話「開一個任務卡處理一下，master都沒到這個級別吧。」）。上段（觸及守衛與突變的判定）：實作者 Opus 5.5／high，敵對式檢查 Fable 5.2／high（使用者事先同意）。盤點與退役表：[kernel-mutation-prune-report.zh-Hant.md](../architecture/target/v2/kernel-mutation-prune-report.zh-Hant.md)。

## 做了什麼

| 步 | 提交 | 內容 |
|---|---|---|
| 第 1 步 盤點與退役表（唯讀） | 5d91175c | 777 個逐個分類；五類候選（重複、等價、對象不在或已涵蓋、守衛、活著或訊息不符）與各自的緩的面；增長判準草案；Fable／high 敵對式檢查 14 處全部改進報告 |
| 使用者決定 | d8ec86be、b262e477、331a9ef3 | 「那 31 個都退，照合併的做法」「舊產品別名不用擋了，那 5 個也退」「其他的照你建議」「要，第 2 步做完之後跑 subsumption 檢查」（報告第 7 節） |
| 第 2 步 改清單 | 本提交 | 777 → 714：直接退 41、合併進接手 22（試跑合併不成的 1 個留下）、改寫 3（下述） |

第 2 步的改動（腳本 `apply_step2.py`，見證據）：
- **直接退 41**：
  - 重複 3：`ns-rebase-no-retake`、`we-j4-handle-check-off`、`ns-c-version-not-bumped`。
  - 被拿掉的那句斷言仍有別的突變 3：`we-w11-mut-no-mark-j30`、`ks-s5-applies-level-records`、`rs-j12-bundle-dropped`。
  - 判定是指令、和接手的指令不同、合併不了 2：`we-k5-entity-store-dropped-selftest`、`rs-sym-elf-bss-unread`。
  - 舊產品別名 4：`ks-k1-aliases-dropped`、`ks-adv-alias-dropped-fps-pvp`、`-pvp`、`-gyo-tools`。
  - 守衛的細節 29（報告第 5 節選項 2）。
- **合併進接手 22**（find 與 replace 相同；接手的 `cases` 改成逗號清單、expect 改成每句都要出現）：3.2 的 20 個裡的 19 個，加上版本常數 3 個（`hf-c-version-kept`、`sr-c-version-not-bumped` 併進 `rs-b6-cfp-version-1`；`hf-plan-version-kept` 併進 `sr-plan-version-not-bumped`；三者只差換成的數字，斷言是 `== 常數`，換成哪個數字都失敗）。接手 18 個，其中 `sr-no-record`、`sr-no-count`、`sr-range-removed`、`rs-b6-cfp-version-1` 各收兩個。
- **改寫 3**：
  - `ks-real-product-name`：改成在 `Plan.hpp` 放 `PlayerState`，expect `engine/kernel/Plan\.hpp:\d+: K1 Player`（K1 的實樹正控制；gyo2 沒有登記的產品，原來的產品名找不到）。
  - `rs-sample-capture-accepted`、`rs-sample-return-accepted`：expect 改成 `variant <X> failed without\s+GYO_KERNEL_REJECT_<X>`（§14.4 的 1～5 在報告 3.4 表下；使用者「其他的照你建議」核准）。
- **合併不成、留下 1**：`ks-s5-nested-baseline-not-updated`（下述）。
- `hf-startup-skipped` 留下（第 2 批已改寫，報告 3.3）。
- 清單的 `description` 末尾加一段，列出退役、合併、改寫的 id。

## 驗證與經過

- 基底：合併 `origin/gyo2` `f48af791` 後重建（`build/target/_build/test`，macOS arm64）。`8b912a1e` 到 `f48af791` 清單只改了第 2 批 `b4b80832` 的 4 個改寫（盤點時已知），程式只多一行註解（`World.hpp`），退役表不受影響。
- D184 的範圍：本批不改程式與測試，只改清單；所以「相關突變」＝合併後的接手與改寫的突變，共 22 個。
- **第一次試跑（23 個全部合併）：22 個中 21 殺**（`step2_mutants.log`）。沒殺掉的是 `ks-s5-baseline-not-updated`（wrong_failure）：它收了 `ks-s5-nested-baseline-not-updated`，兩邊的 expect 都是 GYO_ASSERT 失敗，第一個案例的 GYO_ASSERT 讓行程中止，第二個案例沒跑到，第二句訊息不會出現。報告 3.2 事先列為可能卡住的一個。照決定（報告第 7 節第 1～2 題「合併試跑殺不掉的，原突變留下」），用 `apply_step2.py --keep ks-s5-nested-baseline-not-updated` 重套：接手不收它、它本身留在清單。第一次試跑的清單存為 `step2_mutations_first_try.json`。
- 另一個事先列為可能卡住的 `ks-s5-detour-after-unchecked`（接手 `ns-epoch-check-skipped-for-exclusive` 的 expect 是 `expected GYO_ASSERT to fail`）合併後殺掉：接手的案例用 doctest 檢查 GYO_ASSERT 會失敗，行程不中止，第二個案例照跑。
- 重套後：`ks-s5-baseline-not-updated`、`ks-s5-nested-baseline-not-updated` 2/2（`step2_mutants_keep.log`）。**第 2 步的相關突變 22/22。**
- ctest：`ctest --preset test -L "cpu|shader"` 113 項全過（`step2_ctest.log`，macOS arm64）。本批不改程式與測試，ctest 只確認合併後的基底。
- 沒有跑整份清單：D184 只要求相關突變；整份留給後續的 subsumption 檢查（它每個突變都跑全部 Kernel 測試，涵蓋面比完整跑大）。

## 判定變更（AGENTS §14.4）

- 退役 41 個的緩的面逐個寫在報告第 3、5 節，使用者 2026-11-18 核准（第 7 節）。
- 合併 23 個：沒有緩的面（每句斷言仍由一個突變證明）；締める面是接手以後要每句都失敗才算殺掉。
- rs-sample 兩個：有緩める面（「failed without 標記」以前不算殺、現在算），1～5 在報告 3.4，使用者核准。
- `ks-real-product-name`：守的性質從「登記的產品名進 Kernel」改成「角色詞進 Kernel」。舊的在 gyo2 本來就活著（沒有登記的產品），實際上沒有證明力；登記 id 那一半仍由自測 `test_k1_registered_product_id_is_reported` 與 `ks-k1-registry-ignored` 守。

## 後續（不在本批）

- subsumption 檢查（使用者決定；報告第 7 節第 7 題）：本批合併後做，產出新的候選表交使用者核准，不改清單。
- 把 `PRODUCT_ALIASES` 從 `kernel_guard.py` 拿掉（守衛的改動）。
- `bs-` 開頭的突變搬到 Base 的清單。
- 第 6 節的增長判準交風紀委員看要不要寫進規則文件；GYO2 線先照做。

## 證據

主 checkout `build/evidence/gyo2-mutation-prune/`（git 不追蹤；本機 macOS）：
- 第 1 步的盤點檔與腳本：見報告第 9 節。
- `apply_step2.py`：第 2 步改清單的腳本（`--keep` 是給合併不成時留下原突變用的）。
- `step2_mutants.log`：第一次試跑（接手 19、改寫 3，21/22）；`step2_mutations_first_try.json`：那次的清單。
- `step2_mutants_keep.log`：重套後的兩個；`step2_ctest.log`：ctest。

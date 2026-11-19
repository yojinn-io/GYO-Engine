# Kernel 突變清單的盤點與退役（`v8-arch-gyo2-mutation-prune`）

日期：2026-11-18～19。分支 `claude/gyo2-mutation-prune-19c247`（從 `8b912a1e`；第 2 步前合併 `origin/gyo2` `f48af791`，即第 2 批 PR #244 合併後）。**狀態：第 2 步、第 3 步完成（清單 777 → 714 → 663），PR #246 已合併（27e2404b）；後續「空 expect」另開分支 `claude/gyo2-empty-expect`。**

這一批由任務卡做（GYO2 主會話開出，使用者原話「開一個任務卡處理一下，master都沒到這個級別吧。」）。上段（觸及守衛與突變的判定）：實作者 Opus 5.5／high，敵對式檢查 Fable 5.2／high（使用者事先同意）。盤點與退役表：[kernel-mutation-prune-report.zh-Hant.md](../architecture/target/v2/kernel-mutation-prune-report.zh-Hant.md)。

## 做了什麼

| 步 | 提交 | 內容 |
|---|---|---|
| 第 1 步 盤點與退役表（唯讀） | 5d91175c | 777 個逐個分類；五類候選（重複、等價、對象不在或已涵蓋、守衛、活著或訊息不符）與各自的緩的面；增長判準草案；Fable／high 敵對式檢查 14 處全部改進報告 |
| 使用者決定 | d8ec86be、b262e477、331a9ef3 | 「那 31 個都退，照合併的做法」「舊產品別名不用擋了，那 5 個也退」「其他的照你建議」「要，第 2 步做完之後跑 subsumption 檢查」（報告第 7 節） |
| 第 2 步 改清單 | 801e2d64 | 777 → 714：直接退 41、合併進接手 22（試跑合併不成的 1 個留下）、改寫 3（下述） |
| 第 3 步 subsumption 檢查（不改清單） | f62c70b3 | 714 個逐個跑全部 Kernel 測試、記失敗集合；最小集合 469、候選 245（A 56、B 19、C 170），建議退 51 個；候選表 [kernel-mutation-subsumption-report.zh-Hant.md](../architecture/target/v2/kernel-mutation-subsumption-report.zh-Hant.md)，交使用者核准 |
| 第 3 步 改清單（使用者核准後） | 本提交 | 714 → 663：候選表第 3 節的 51 個合併進 39 個接手（使用者 2026-11-19「照你建議的做」） |

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

### 第 3 步 subsumption 檢查

- 使用者決定：「要，第 2 步做完之後跑 subsumption 檢查」（11-18）、「照一般定義的方向做」（11-19，更正報告草案的方向，2a7cd404）。使用者睡前交代「今晚把突變清單跑完吧」，主會話轉達：照順序做完、不改正式清單、不合併 #246。
- 資料收集：`run.py`，2026-11-19 01:58～04:07（7801 秒），714 個全部跑完；跑完工作樹乾淨。中止的案例排除再跑（56 個要多輪，最多 29 輪）；`hf-round-count-kept` 不篩案例時逾時。
- 中途修的兩件事（都在分析腳本，不影響收集到的資料）：
  - 合併過的 expect 以 `(?=[\s\S]*…)` 開頭，用 `re.search` 在幾百 KB 的輸出上是平方時間，改成 `re.match`（這種式子從開頭試一次就夠了）。
  - 符號表、匯出檢查與執行緒探針的輸出不是 doctest／unittest 格式，改成「比沒改的基準多出來的行」（`baseline.py`）。
- 初稿（`analyze.py`）：最小集合 463、候選 251（A 76、B 27、C 148），建議退 96 個。A/B 的判定是「X 的 expect 文字出現在接手的完整輸出裡」。合併試跑：103 個全部併時 73 個接手殺 72（`rs-iv-fc6-append-round-not-next` 併進 `we-j3-spawn-immediate` 不成，原因和第 2 步那個一樣，接手的 GYO_ASSERT 中止讓後面的案例跑不到）；初稿建議的 96 個 67/67（`rec_trial.log`）。
- 敵對式檢查（Fable 5.2／high）推翻初稿：96 個裡 31 個有問題。
  - 23 個：接手對上的是別句斷言，其中 13 個退了之後那句沒有任何突變。
  - 7 個 `ks-s2-re7-*`：鍵沒分 SUBCASE，方向看反。
  - 1 個：`bs-thread-serial-is-tls-address`，失敗次數 186 對 200，是 S10 的重現突變。
  - 處理：判定改成看位置、鍵加上 SUBCASE 與訊號名（`analyze2.py`，沿用檢查者的位置對應腳本，搬到 `checker/`）；合併時每句包 `(?:…)`、相同的句子只寫一次。12 點與處理在候選表第 9 節。
- 重算：最小集合 469、候選 245（A 56、B 19、C 170）；建議 68 個，收到合併的接手 51 個試跑 51/51（`rec2_trial.log`）。
- 候選表與建議：退 51 個（714 → 663）；第 3b 節的 17 個（值不同）另交使用者（建議留）；第 1 步第 5 節「留」的守衛 6 個與 `bs-thread-serial-is-tls-address` 另交使用者（建議留）；C 170 個建議留；7 個 expect 實際為空的突變（案例無條件印 MESSAGE）列為後續。
- 窄複查（同一位檢查者 Fable 5.2／high）：68 個的位置條件全部成立；另外指出其中 17 個 X 與接手在同一行失敗、錯成的值不同，退了會少一個發現改鬆型弱化的突變。和 C 類「集合相同但 expect 對的值不同」同一類，為一致建議留，移到候選表第 3b 節交使用者。其餘 6 點是說法與過時數字，照改。
- 最後建議：退 51 個（714 → 663），收到合併的接手 39 個試跑 39/39（`rec3_trial.log`）。

### 第 3 步 改清單

- 使用者決定（2026-11-19）：「照你建議的做」——候選表第 8 節：退 51 個（合併進接手）；第 3b 節的 17 個、第 4 節的 7 個、第 5 節 C 的 170 個留；7 個空 expect 列為後續。
- 改法：`subsumption/apply_step3.py`，合併的邏輯和試跑的 `merge_trial.py` 相同。另外加兩件事：接手的 `what` 寫進每個併進來的 X 的 id 與原本的 `what`；清單的 `description` 加一段。
- 和試跑用的 `rec3_catalog.json` 比：id 相同。有 16 個接手的 expect 不同，原因是去重後只剩一句時沒有改寫原本的 expect，試跑則包成 `[\s\S]*(?:…)`，兩者對 `re.search` 等價（逐一確認）。其餘欄位除 `what` 外完全相同。
- 鏈式合併（接手又被退）：
  - `sr-range-removed` 併進 `sr-dropped-made-alive`：它在第 2 步收的兩句（J-55 range-respawn／range-insert）隨整段 expect 一起移過去。
  - `ns-rebase-no-retake-names-ping` 併進 `ns-rebase-no-retake-on-throw`：它是第 2 步直接退掉的 `ns-rebase-no-retake` 的接手。
- 驗證：用正式清單跑 39 個接手 39/39（`step3_mutants.log`；09:35 開始，跑到最後一個時機器休眠，14:03 醒來後跑完，原始碼已還原）；`ctest --preset test -L "cpu|shader"` 113 項全過（`step3_ctest.log`，14:06 重跑存完整輸出）。

### 後續：7 個空 expect（`v8-arch-gyo2-empty-expect`）

- 使用者 2026-11-19：「把 7 個空 expect 你直接處理」（候選表第 8 節第 5 題）。分支 `claude/gyo2-empty-expect`（從 `gyo2` 27e2404b）。
- 問題：`rs-iv-ff1-registry-section-dropped`、`rs-iv-fb5-sort-tiebreak-dropped`、`rs-iv-fb6-C-target-not-hashed`、`rs-iv-fb7-registry-target-not-hashed`、`rs-iv-fb8-C-request-not-hashed`、`rs-iv-fb9-C-filter-by-request-zone`、`rs-iv-fb11-C-count-dropped` 的 expect 是案例標題。那幾個案例每次都無條件印 MESSAGE，標題永遠出現，所以 expect 只剩「`cases` 裡有東西失敗」。
- 改法：從 subsumption 完整跑的原始輸出，找每個突變在自己的 `cases` 範圍裡實際失敗的 CHECK，expect 改成指名它設計上要打壞的那一句（doctest 印的 `CHECK( 式子 ) is NOT correct`；每個式子在測試原始碼裡只出現一次）：

| 突變 | 打壞的性質 | 新 expect 指名的 CHECK（`InitialValueTests.cpp`） |
|---|---|---|
| ff1 | Registry 指紋漏掉宣告 | 708、709（兩句都要）：有宣告時 Registry 指紋要變 |
| fb5 | 宣告的排序不再比後段 | 893、894（兩句都要）：順序不同，指紋要相同 |
| fb6 | C 指紋漏掉元件 | 891 |
| fb7 | Registry 指紋漏掉元件 | 890 |
| fb8 | C 指紋漏掉 request | 715（第 3 步併進來的 `rs-iv-ff3-C-section-dropped`，那句的位置也是 715） |
| fb9 | C 指紋照 request 的區挑宣告 | 713：只有 zone B 時 C 指紋不變 |
| fb11 | C 指紋不計宣告數 | 906：C 的 golden |

- 判定變更（AGENTS §14.4，只有締める面，由我改、事後報告）：
  1. 守的性質：每個突變證明「指紋的那一句 CHECK 抓得到這個改法」。
  2. 緩める面：無。新 expect 對上時，那句 CHECK 所在的案例一定跑過也失敗了，標題一定印出，舊 expect 也對得上；所以新判定算殺掉的，舊判定也算。舊判定算殺掉的 7 個突變，新判定也全部殺掉（7/7）。
  3. 締める面：`cases` 裡別的斷言失敗、但指名的那句沒失敗時，以前算殺掉，現在不算。用 subsumption 的 714 份原始輸出比：舊 expect 對得上 592 份（fb8 是 10 份），新 expect 只對得上 1～11 份，都是真的打壞那一句的突變。
  4. 不適用（沒有緩める面）。
  5. 證據：`build/evidence/gyo2-mutation-prune/empty_expect_old.json`（舊 expect）、`empty_expect_mutants.log`（7/7 killed，macOS arm64）。
- 程式與測試沒有改動，所以沒跑 ctest。

## 後續：拿掉 PRODUCT_ALIASES（v8-arch-gyo2-drop-product-aliases）

- 來由：報告第 7 節第 3 題，使用者 2026-11-18 定「舊產品別名不用擋了」，當時只退了突變、清單留在守衛裡，列為後續。2026-11-19 使用者：「拿掉 PRODUCT_ALIASES 你直接處理」。
- 改動：
  - `tests/common/kernel/kernel_guard.py`：刪 `PRODUCT_ALIASES`（`RetroFPS`、`fps::pvp`、`\b[Pp]vp`、`Gyo::Tools`）；K1 剩兩組：(i) 登記 id、(ii) 角色詞 `ROLE_WORDS`（原本的 (iii)）。docstring 寫明別名已拿掉與出處。
  - `tests/common/kernel/test_kernel_guard.py`：刪 `test_k1_product_alias_is_reported`、`test_k1_every_product_alias_is_reported`。
  - `mutations.json`：`ks-k1-roles-dropped` 的 find／replace 原本寫著 `PRODUCT_ALIASES + ROLE_WORDS`，改成只寫 `ROLE_WORDS`（拿掉角色詞的改法不變）；description 加一段。
  - `kernel-scaffold-declaration.zh-Hant.md` 第 3.1 節 K1 那一行加註：(ii) 已拿掉。
- 驗證（macOS arm64）：守衛自測 85 個全過；守衛跑實樹乾淨；檔案是 `kernel_guard.py` 的 48 個突變全部殺掉（48/48）。Kernel 的 C++ 沒有改，沒跑 ctest。
- 判定變更（AGENTS §14.4；這是 Design Invariants 的守衛，有緩める面，使用者 2026-11-18 的決定與 2026-11-19 的指示即核准）：
  1. 守的性質：Kernel 的程式與註解不出現舊產品的名字 `RetroFPS`、`fps::pvp`、`Pvp`／`pvp`、`Gyo::Tools`（登記表推不出的那幾個）。
  2. 緩める面：Kernel 裡出現這四個名字，以前 K1 報、現在不報（除非同時碰到登記 id 或角色詞，例：`PvpMatch` 不會被 `\bMatch\b` 抓到，所以現在通過）。
  3. 締める面：無。
  4. 這個性質不再由別的判定守，是使用者的決定：gyo2 沒有這些產品。登記 id 那一組照舊：產品重新登記到 gyo2 時，它的 id（原樣與 CamelCase）自動被禁，但 `RetroFPS`、`fps::pvp` 這類推不出的名字不會。
  5. 根據：報告第 7 節第 3 題；證據 `build/evidence/gyo2-mutation-prune/drop_aliases_mutants.log`（48/48 killed）。
- 沒做（範圍外）：`tests/common/entity_store/entity_store_guard.py` 的 W1 也禁同樣的舊名字（`object_fps`、`RetroFPS`、`fps::pvp`、`Pvp`）。它是 Kernel 角色詞的來源，要不要一起拿掉由使用者決定。

## 判定變更（AGENTS §14.4）

- 退役 41 個的緩的面逐個寫在報告第 3、5 節，使用者 2026-11-18 核准（第 7 節）。
- 第 3 步退役 51 個（合併）：刪除型的弱化沒有緩的面（每句斷言仍由接手在同一位置證明）；締める面是接手要每句都失敗才算殺掉。改鬆型的情形（值不同 17 個、次數不同 1 個）照使用者決定留下，見候選表第 3b、4 節。
- 合併 23 個：沒有緩的面（每句斷言仍由一個突變證明）；締める面是接手以後要每句都失敗才算殺掉。
- rs-sample 兩個：有緩める面（「failed without 標記」以前不算殺、現在算），1～5 在報告 3.4，使用者核准。
- `ks-real-product-name`：守的性質從「登記的產品名進 Kernel」改成「角色詞進 Kernel」。舊的在 gyo2 本來就活著（沒有登記的產品），實際上沒有證明力；登記 id 那一半仍由自測 `test_k1_registered_product_id_is_reported` 與 `ks-k1-registry-ignored` 守。

## 後續（不在本批）

- ~~把 `PRODUCT_ALIASES` 從 `kernel_guard.py` 拿掉（守衛的改動）。~~ 已做，見上「後續：拿掉 PRODUCT_ALIASES」。
- `entity_store_guard.py` 的 W1 裡的舊產品名要不要一起拿掉（待使用者決定）。
- `bs-` 開頭的突變搬到 Base 的清單。
- 第 6 節的增長判準交風紀委員看要不要寫進規則文件；GYO2 線先照做。

## 證據

主 checkout `build/evidence/gyo2-mutation-prune/`（git 不追蹤；本機 macOS）：
- 第 1 步的盤點檔與腳本：見報告第 9 節。
- `apply_step2.py`：第 2 步改清單的腳本（`--keep` 是給合併不成時留下原突變用的）。
- `step2_mutants.log`：第一次試跑（接手 19、改寫 3，21/22）；`step2_mutations_first_try.json`：那次的清單。
- `step2_mutants_keep.log`：重套後的兩個；`step2_ctest.log`：ctest。
- `subsumption/`：第 3 步的資料收集、分析、合併試跑與檢查者的腳本（清單見候選表第 10 節）。

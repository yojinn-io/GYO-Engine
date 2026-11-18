# Kernel 突變清單的盤點與退役候選（`v8-arch-gyo2-mutation-prune` 第 1 步）

日期：2026-11-18。分支 `claude/gyo2-mutation-prune-19c247`（基於 `8b912a1e`，第 2 批分支的最後一個 commit）。
對象：`docs/architecture/plans/kernel/scripts/mutations.json`（777 個）。本頁只是提案：**第 1 步不改清單、測試、守衛**；你核准退役表之後，第 2 步（第 2 批合併進 `gyo2` 之後）才改。

用詞：**緩的面**＝退役之後，以前會被某個突變發現、之後不會被任何突變發現的情形；**接手**＝同一個改法或同一個性質之後由哪個突變證明。

完整跑：第 2 批卡片在 `cc5fe0e1` 跑的整份清單（2026-11-18 19:57～22:57，約 180 分）：776 個執行、769 killed、1 個只在 Linux 跑而跳過、7 個不是 killed（第 3.4 節）。`cc5fe0e1` 到本分支基底 `8b912a1e` 只改了文件，程式與清單相同。之後第 2 批在 `b4b80832` 改寫了其中 4 個、4/4 殺掉（Kernel 773/776）；剩下 3 個歸本批（第 3.4 節）。

## 0. 結論（先看這裡）

- 照任務的五類，**退役候選 36 個**：(a) 3、(c) 33（同一改法 28、對象不在 5）；(e) 的 7 個沒有另外要退的；(b) 0。退役後 741 個，完整跑一次約省 8 分（第 4 節）。
- (c) 同一改法的 28 個裡，**20 個不是單純拿掉，而是「合併進接手」**：接手的 `cases` 與 expect 加上被拿掉的那句斷言，兩句仍由一個突變證明，沒有緩的面。合併還沒試跑；第 2 步合併不成的就留下，所以 36 是上限（第 3.2 節）。
- 另外 **(d) 守衛腳本的突變 125 個**交你判斷（第 5 節）。敵對式檢查後我改了建議：只退 29 個「沒抓到過真實漏報、也不是子規則唯一證明」的；抓到過漏報的（審查 A 系列、K8 與 S10 的敵對式檢查）、fail-closed、子規則唯一的、實樹的正控制都留。照建議做的話總數是 712 個。
- 777 → 七百出頭這個量級，代表**清單的大半是不同的改法**（不同的條件、順序、錯誤碼、預設值），不是重複。master 的 209 個只涵蓋 11-13 的兩批（足場＋登記排程，其中 197 個 gyo2 也有）；gyo2 之後的七批又加了 580 個（第 1.2 節）。要回到幾百個的量級，靠的不是這次退役，而是第 6 節的增長規則。
- (e) 的 7 個都不是等價突變。4 個（`hf-startup-skipped`、`we-j1-insert-append`、`ks-s3-version-not-raised`、`ks-s3-failed-raises-version`）是第 2 批段 2、段 3 改程式時沒跟上的，**第 2 批已在 `b4b80832` 自己改寫、4/4 殺掉**；剩下 3 個在第 2 批開工前就殺不掉，歸本批：`ks-real-product-name` 退役（你已定），`rs-sample-capture-accepted`、`rs-sample-return-accepted` 改寫 expect、不退役（第 3.4 節）。
- 敵對式檢查（Fable 5.2／high）：找到 14 處，其中 8 處會失去檢出力；全部照它的意見改了報告（第 8 節）。最大的改動是 3.2 改用斷言的粒度算緩的面（多數改成合併），和 (d) 的建議從「退 78」改成「退 29」。

## 1. 組成

### 1.1 依改的檔案

| 類 | 數 | 說明 |
|---|---|---|
| 改 `engine/` 的 C++（含 CMakeLists） | 665 | 其中 15 個的判定是守衛腳本（例：把產品名放進 `Plan.hpp`，看 `kernel_guard.py` 報不報） |
| 改守衛腳本（`.py`） | 110 | `kernel_guard.py` 69、`naming_guard.py` 22、`kernel_symbols.py` 18、`store_stamp_guard.py` 1 |
| 改測試 | 2 | J-10 編譯失敗樣本的兩個自我檢查 |

判定方式：`gyo_kernel_tests` 640、指令（守衛腳本或掃描器）116、`gyo_base_tests` 11、`gyo_kernel` 9、`gyo_kernel_thread_bound_probe` 1。只在某些平台跑的 9 個（其中 `rs-sym-elf-bss-unread` 只在 Linux，本機的完整跑永遠略過它）。

### 1.2 依批次（id 前綴）

| 批次 | 前綴 | 數 | master 也有 |
|---|---|---|---|
| kernel-scaffold（11-13） | `ks-k*`、`ks-n*`、`ks-real-*`、`ks-adv-*` | 74 | 74 |
| registry-schedule（11-13） | `rs-`（不含 `rs-iv-`） | 122 | 122 |
| funnel-check | `rs-iv-` | 54 | — |
| hook-flush | `hf-` | 68 | — |
| spawn-reject | `sr-` | 80 | — |
| nested-schedule | `ns-` | 67 | — |
| world-exec 搬入 | `we-` | 77 | — |
| test-debts | `td-` | 13 | — |
| kernel-subtract-structure | `ks-s*`、`ks-diks*`、`ks-k8-*`、`ks-links-*`、`bs-` | 211＋11 | 1 |

kernel-subtract-structure 一批就加了兩百多個，其中執行緒序號（S10）一件就有 `ks-s10-*` 26、`ks-k8-*` 16、`bs-*` 11，共 53 個。

### 1.3 依守的性質

逐個的盤點（id、守的條文、判定的測試、expect、完整跑的結果）在證據檔 `inventory.tsv`（第 9 節）。依條文的分布（只算改 `engine/` 的）：

| 條文 | 數 | 條文 | 數 |
|---|---|---|---|
| J-54（hook 清空輪） | 71 | DI-KS 9 | 17 |
| J-55（丟掉不能生成的那一筆） | 70 | DI-KS 25 | 15 |
| 初始值（`InitialValue`、漏斗） | 50 | J-18（指紋） | 14 |
| J-56（巢狀跑） | 49 | S10（執行緒序號） | 14 |
| DI-KS 26（登記輪） | 36 | J-12、J-50 | 各 12 |
| DI-KS 18（當場改） | 32 | J-40 | 10 |
| J-32（同步點的套用） | 30 | 其他 56 種（含讀不出條文的 6 個） | 各 1～9 |
| J-57（巢狀跑的閉包、宣告） | 27 | | |
| J-24（啟動錯誤） | 23 | | |
| J-6（綁執行緒的 Resource） | 20 | | |

## 2. 怎麼找的

- (a)／(c) 的重複：把 777 個依「檔案＋find」分組。97 組有兩個以上（269 個），逐組讀 replace 與判定。其中 **find 與 replace 完全相同的 26 組**是同一個改法；其餘多數是同一行的不同改法（不同條件、不同錯誤），不算重複。26 組裡 24 組進了候選；另外 2 組（`ks-s10-kernel-own-thread-serial`／`ks-s10-k4-kernel-own-thread-serial`、`ks-s10-sym-serial-pattern-dead`／`-real`）判定的守衛或對象不同（一個是 K8、一個是 K4；一個是自測、一個是實際函式庫），不是重複。
- (b) 等價：只看完整跑活下來的；活下來的 3 個，原因都不是等價（第 3.4 節）。
- (c) 對象不在：讀每個突變的 replace 是否引用已拿掉的東西，並對照完整跑的 `build_failed`。
- (d)：所有改守衛腳本、或判定是守衛腳本的突變，依守衛規則分組（第 5 節）。
- (e)：讀第 2 批在 `cc5fe0e1` 的完整跑（776 個執行、769 killed、1 跳過、3 survived、2 wrong_failure、2 build_failed）。
- 「緩的面」：每個候選都查兩件事——接手的突變是否是同一個改法、被拿掉的突變的 expect 指到的那句斷言退役後還有沒有別的突變。初稿用 `cases` 字串相等算第二件事，敵對式檢查指出粒度太粗，改用斷言（CHECK）的粒度（第 3.2 節）。

## 3. 退役候選表

### 3.1 (a) 重複：同一處、同一種改法、同一個案例、同一句斷言（3 個）

| id | 理由與證據 | 接手 | 緩的面 |
|---|---|---|---|
| `ns-rebase-no-retake` | 和接手的 find／replace 完全相同、同一個案例；接手的 expect 是它的加長版（多要求訊息點名 `'event Ping'`） | `ns-rebase-no-retake-names-ping` | 無：同一個改法、同一句斷言、更嚴的 expect |
| `we-j4-handle-check-off` | 同一行；它的改法（整個檢查換成 `return;`）比接手的（只留 `syncEpoch` 那一半）更寬，同一個案例、同一句 expect（`HasCode(result, StaleHandle)`；J-4 只有 `AccessCheckTests.cpp:296` 這一句）。能殺接手的情境裡它的行為和接手相同 | `we-adv-j4-serial-half-dropped` | 無：同一句斷言 |
| `ns-c-version-not-bumped` | C 指紋版本常數改成 7；和接手同一個案例 J-18、同一句 expect（`CFingerprint().version == 8U`） | `rs-b6-cfp-version-1` | 無 |

初稿這裡有 7 個；敵對式檢查指出另外 4 個（`sr-j8-allots-later`、`hf-c-version-kept`、`sr-c-version-not-bumped`、`hf-plan-version-kept`）的接手在另一句斷言或另一個案例，不符合這一節的標題，已移到 3.2。

### 3.2 (c) 同一個改法已在清單裡，只是由另一個案例或另一句斷言殺（28 個）

這一組被拿掉的突變，程式改壞了仍由接手抓到。差別在**斷言**：被拿掉的突變的 expect 指到某一句 CHECK，退役後那一句 CHECK 有沒有別的突變證明。初稿用 `cases` 字串相等來算「案例的其他突變」，粒度太粗（同一個 `J-32:*` 底下的別的突變，指的是別的 SUBCASE、別句 CHECK）；敵對式檢查改用斷言的粒度重算，下表照重算的結果分兩種處理。

**直接退（8 個）**：那一句斷言仍有別的突變，或那一句只是重複檢查同一個常數。

| id | 接手 | 被拿掉的那句斷言 | 緩的面 |
|---|---|---|---|
| `we-w11-mut-no-mark-j30` | `we-w11-mut-no-mark`（J-11） | J-30 的 stamp 序列 | 無：那一句仍由 `we-j30-skip-no-stamp` 證明 |
| `ks-s5-applies-level-records` | `ks-s5-level-spawn-applied` | DI-KS 9「`[ks-s5-hook-rounds-now]`」 | 無：那一句仍有別的突變 |
| `rs-j12-bundle-dropped` | `rs-j3-11-skip-bundle`（J-15） | J-12 的 `Spawn<Bundle<…>>` | 小：J-12 同一行還有 `rs-iv-fd6-spawn-bundle-as-writes`（大致同一句） |
| `hf-c-version-kept` | `rs-b6-cfp-version-1`（J-18） | J-54 案例裡「C 指紋版本等於 8」 | 小：J-54 那一句只是重複 J-18 已證明的常數；那一句被刪不會被發現 |
| `sr-c-version-not-bumped` | `rs-b6-cfp-version-1`（J-18） | J-55「versions: C fingerprint version 8」 | 小：同上（J-55 的重複句） |
| `hf-plan-version-kept` | `sr-plan-version-not-bumped`（J-55） | J-54 案例裡「Plan 指紋版本等於 6」 | 小：同上（Plan 版本由 J-55 那一句證明） |
| `we-k5-entity-store-dropped-selftest` | `we-k5-entity-store-dropped`（實樹） | 自測 `test_k5_entity_store_include_passes` | 小：這個自測失去唯一的突變；同一個改法由實樹的突變抓到 |
| `rs-sym-elf-bss-unread` | `rs-sym-bss-unread`（自測） | 實際函式庫的 `.bss`（只在 Linux 跑） | 小：Linux 上「實際函式庫」那一層失去證明，自測仍抓同一個改法。接手 `rs-sym-bss-unread` 在第 5 節改列「留」（ELF 這條路徑只有它），兩邊不會一起退 |

**合併進接手（20 個）**：find 與 replace 和接手完全相同，但被拿掉的那句斷言退役後沒有別的突變。做法是把被拿掉的突變的案例與那句訊息**併進接手**：接手的 `cases` 改成逗號清單（兩個案例都跑），expect 改成 `(?=[\s\S]*<接手原本那句>)[\s\S]*<被拿掉的那句>`（兩句都要出現，順序不拘）。清單裡已有前例（`rs-iv-fd6-spawn-bundle-as-writes` 的 `J-12:*,initial value:*`、`ns-rebase-no-retake-names-ping` 的串接）。這樣兩句斷言仍由一個突變證明，**沒有緩的面**；締める面是接手以後要兩句都失敗才算殺掉。省下的是一次建置。

合併還沒試跑。第 2 步逐個試跑；**合併後殺不掉的（例如第一個案例的 GYO_ASSERT 讓程式中止、第二個案例沒跑到），原突變就留下，不退役**。所以這一組的 20 是上限。可能卡住的：`ks-s5-nested-baseline-not-updated`（兩邊的 expect 都是 GYO_ASSERT 失敗）、`ks-s5-detour-after-unchecked`（接手的 expect 是 `expected GYO_ASSERT to fail`）。

| id | 併進的接手 | 被拿掉的那句斷言（現在只有它證明） |
|---|---|---|
| `sr-j8-allots-later` | `sr-dropped-made-alive` | J-8「the allotter later allots 149, 150 and 151」（`WorldTests.cpp:344-363` 的區塊裡，只有它證明被丟掉的 id 之後配得出來） |
| `sr-j8-no-record` | `sr-no-record` | J-8「the dropped spawn is recorded」 |
| `sr-j8-no-count` | `sr-no-count` | J-8「the dropped spawn is counted」 |
| `sr-j8-reason` | `sr-reason-range-as-alive` | J-8「the reason is IdInAllotterRange」 |
| `sr-j32-alive-applied` | `sr-alive-applied` | J-32「the dropped spawn wrote no bundle component」（`WorldTests.cpp:609-629` 的 SUBCASE） |
| `sr-j32-no-record` | `sr-no-record` | J-32「the dropped spawn is recorded」 |
| `sr-j32-no-count` | `sr-no-count` | J-32「the dropped spawn is counted」 |
| `sr-j32-reason` | `sr-reason-alive-as-twice` | J-32「the reason is IdAlive」 |
| `sr-range-removed-respawn` | `sr-range-removed` | J-55「range-respawn: 150 is gone」 |
| `sr-range-removed-insert` | `sr-range-removed` | J-55「range-insert: the insert is dropped and counted」 |
| `we-j5-boundary-off` | `we-j4-boundary-off` | J-5 (a)(b) 的 `HasCode(on, KernelErrorCode::BoundaryChange)`（`AccessCheckTests.cpp:167`、`:183`） |
| `ks-s9-set-order-dropped` | `rs-adv-set-order-dropped` | J-23「a set's after leaves no tie」 |
| `ks-s7-removed-limit-one` | `hf-settings-absent-limit-one` | J-54「removed」（兩次 Run 之間拿掉 KernelSettings） |
| `ks-s7-first-run-only-limit` | `ks-s7-first-run-only` | J-54「next Run」 |
| `ns-nested-end-ready-overwrite` | `ns-nested-end-ready` | J-56 `[ns-back-overwrite]` |
| `ns-absent-resource-present` | `ns-absent-component-present` | J-57 `[ns-res-absent]`（`NestedRunTests.cpp:1151-1157`） |
| `ks-s5-j45-spawn-deferred` | `ks-s5-change-deferred` | J-45「J-45 spawn」（`ExecutorTests.cpp:825-836`） |
| `ks-s5-detour-after-unchecked` | `ns-epoch-check-skipped-for-exclusive` | DI-KS 18 `[ks-s5-detour-after]` |
| `ks-s5-nested-baseline-not-updated` | `ks-s5-baseline-not-updated` | DI-KS 18 的巢狀跑那一句 GYO_ASSERT |
| `ks-s6-hook-set-at-record-time` | `ks-s6-set-at-record-time` | DI-KS 9 `[ks-s6-hook-next-window]` |

初稿對其中 4 個（`ks-s7-removed-limit-one`、`ks-s5-detour-after-unchecked`、`ks-s5-nested-baseline-not-updated`、`ks-s6-hook-set-at-record-time`）的傾向是「保留」；合併之後它們守的那句斷言仍有證明，這個傾向就不需要了（合併不成的照上面留下）。

### 3.3 (c) 保護的對象已經不在（5 個）

舊產品別名那 5 個（`ks-real-product-name`、`ks-k1-aliases-dropped`、`ks-adv-alias-dropped-*`）：使用者 2026-11-18 決定不再擋舊產品的名字、退役（第 7 節第 3 題）。

**`hf-startup-skipped` 從候選拿掉（留下）。** 初稿把它列在這裡，理由是 replace 用到已拿掉的 `record->options.startup`（完整跑 `build_failed`）。第 2 批在 `b4b80832` 把它改寫成 `if (position == lastSync && record->options.clock)`，也就是「非時鐘的單次 schedule 不跑 hook 輪次」，案例 `J-54: run once*`，4/4 裡已殺掉。重新判斷的結果如下。
- 測試上已有涵蓋：`hf-only-with-trim`（`if (position == lastSync && position + 1 < plan->steps.size())`）用同一個案例、同一個 expect。那個案例的 schedule 只有一個 stage，最後一個同步點後面沒有別的步驟，所以兩個改法的效果相同，都是「這個單次 schedule 的 hook 記錄沒有套用」。照 (c) 的標準可以退。
- 仍然留下的理由：
  - 它是唯一直接寫出第 2 批新契約的突變：Kernel 沒有開局了，編排層用非時鐘的單次 schedule 開局，這種 schedule 也必須跑 hook 輪次。
  - 第 2 批今天才刻意改寫它。
  - `hf-only-with-trim` 要接手，前提是「測試的 schedule 在最後同步點之後沒有步驟」。測試一改，接手就斷。
- 如果你要退：緩的面是「改成只有時鐘 schedule 才跑 hook 輪次」這種錯，之後只靠 `hf-only-with-trim` 間接證明案例抓得到；上面那個前提一變就沒有證明。

| id | 理由與證據 | 接手 | 緩的面 |
|---|---|---|---|
| `ks-real-product-name` | 把 `ObjectFpsPvpState` 放進 `Plan.hpp`，看 K1 報不報。gyo2 的 `engine/config/projects.csv` 只剩表頭，沒有任何登記的產品，所以 K1 的「登記 id」部分在實樹上不找任何名字，突變活著（完整跑：`survived`）。也算 (e) | 無（K1 的登記 id 規則仍由自測 `test_k1_registered_product_id_is_reported` 用合成的登記檔守，突變 `ks-k1-registry-ignored`） | 實樹上「產品名進 Kernel」本來就抓不到（沒有產品）。這個突變測的是 K1 的「登記 id」那一半，不是別名；退役後 K1 沒有實樹的正控制——但它現在就是 survived，實際上已經沒有了，不是退役造成的。另一個做法：改寫成角色詞（例：在 `Plan.hpp` 放 `PlayerState`，K1 的 `ROLE_WORDS` 仍在守），留作 K1 的實樹正控制（第 7 節第 3 題） |
| `ks-k1-aliases-dropped` | `PRODUCT_ALIASES` 的四個別名（`RetroFPS`、`fps::pvp`、`Pvp`、`Gyo::Tools`）都是 master 上舊產品的名字，gyo2 已經沒有這些產品 | 無 | 別名清單整個失去突變證明；**若你想讓 K1 繼續擋「舊遊戲的程式搬回 Kernel」，這 4 個應保留**（第 7 節） |
| `ks-adv-alias-dropped-fps-pvp` | 同上（逐一拿掉一個別名） | 無 | 同上 |
| `ks-adv-alias-dropped-pvp` | 同上 | 無 | 同上 |
| `ks-adv-alias-dropped-gyo-tools` | 同上 | 無 | 同上 |

### 3.4 (e) 完整跑時活著或失敗訊息不符（7 個）

完整跑 7 個不是 killed（另 1 個 `rs-sym-elf-bss-unread` 只在 Linux 跑，已列在 3.2）。改寫的 5 個已在本分支用臨時清單（`rewrite_check.json`，不是正式清單）跑過，全部 killed（第 9 節）。

| id | 完整跑 | 原因（證據） | 處理 | 緩的面 |
|---|---|---|---|---|
| `ks-real-product-name` | survived | 沒有登記的產品（3.3） | 退役（已算在 3.3） | 見 3.3 |
| `hf-startup-skipped` | build_failed | 引用已拿掉的 `record->options.startup` | **第 2 批已改寫（`b4b80832`）**、已殺掉；本批不退（3.3 的說明） | — |
| `we-j1-insert-append` | build_failed | 第 2 批段 3（`a8065853`）把呼叫改成 `::Engine::EntityStore::InsertEntity(cells, …)`；find 只對到 `Engine::…` 那一段，換掉後前面留下 `::cells.push_back`，編譯不過 | **第 2 批已改寫（`b4b80832`）**：find 前面加 `::`（和本批的試跑同一個改法）→ killed | 無（同一個改法、同一個 expect） |
| `ks-s3-version-not-raised` | survived | 第 2 批段 2（`81e1d231`）把測試名從「DI-KS 25: the version is 0 …」改成「… is 1 …」；`cases` 還是 `the version is 0*`，一個測試也沒跑，所以活著 | **第 2 批已改寫（`b4b80832`）**：`cases` 改成 `DI-KS 25: the version is 1*`（和本批的試跑相同）→ killed | 無 |
| `ks-s3-failed-raises-version` | survived | 同上 | 同上（第 2 批已改寫）→ killed | 無 |
| `rs-sample-capture-accepted` | wrong_failure | 拿掉「拒絕有捕獲的 lambda」之後，變體在更後面編譯失敗（`no matching constructor`），所以 try_compile 報的是「variant CAPTURE failed without GYO_KERNEL_REJECT_CAPTURE」（CMake 把這句折成兩行），不是 expect 寫的「variant CAPTURE compiled」 | 改寫 expect 成 `variant CAPTURE failed without\s+GYO_KERNEL_REJECT_CAPTURE` → killed | 見表下的說明 |
| `rs-sample-return-accepted` | wrong_failure | 同上（RETURN 變體） | 改寫 expect 成 `variant RETURN failed without\s+GYO_KERNEL_REJECT_RETURN` → killed | 同上 |

rs-sample 兩個的 expect 改寫（照 AGENTS §14.4 的 1～5）：
1. 守的性質：J-10 樣本 (a) 的「有捕獲的 lambda／回傳型別不對的 System 會被拒絕，而且是用設計的那個標記拒絕」。拿掉那個檢查之後，捕獲的 lambda 仍會在更後面編譯失敗（`no matching constructor`，`rs-sample-capture-output.log:63`），所以這個突變能證明的是「拒絕訊息帶設計的標記」，不是「會被接受」。
2. 緩める面：以前不算殺掉的「failed without 標記」現在算殺掉。
3. 締める面：變體真的編過（`tests/common/kernel/CMakeLists.txt:218` 的「compiled」）之後不再算殺掉。
4. 「compiled」那條分支是所有變體共用的，仍由 `rs-j11-self-conflict-off`、`rs-j13-condition-write-allowed`、`rs-adv-assert-member-removed`、`we-j4-handle-copyable`、`sr-static-assert-removed` 等證明。
5. 證據：`rewrite_check.log`（2/2 killed）。前例：world-exec 分支 `50cb2050` 曾改成 `variant CAPTURE (compiled|failed without)`，gyo2 沒帶過來，所以 11-18 的完整跑又是 wrong_failure。本頁用指名標記的寫法，比前例嚴：前例連「失敗但訊息不對」的別的原因也算殺掉。限制：只在 macOS 驗過；`\s+` 依賴 CMake 把那句折行，別的平台若折法不同仍對得上，但沒驗過。

第 2 批在 `b4b80832` 自己改寫了 `hf-startup-skipped`、`we-j1-insert-append`、`ks-s3-*` 這 4 個，4/4 殺掉（第 2 批的 commit 訊息：Kernel 773/776）。本批第 2 步以第 2 批合併後的 `gyo2` 為底，只處理剩下 3 個：`ks-real-product-name` 退役，rs-sample 兩個改寫 expect。

### 3.5 (b) 等價突變：0 個

完整跑活下來的 3 個：`ks-real-product-name` 是保護的對象不在（3.3）；`ks-s3-*` 兩個是根本沒跑到測試（`cases` 篩不到）。都不是等價。其餘都被殺掉（以預期那一句），所以沒有「改了之後行為不變」的證據。

## 4. 退役後的總數與省下的時間

| | 數 |
|---|---|
| 現在 | 777 |
| (a) | −3 |
| (c) 同一改法 | −28（其中 20 個是合併，合併不成的留下） |
| (c) 對象不在 | −5（`ks-real-product-name` 已算在內；`hf-startup-skipped` 留下） |
| (e) 另外的 | 0（`ks-real-product-name` 已算在 (c)，其餘 6 個改寫、不退役） |
| **退役後** | **741**（最少；合併不成的每個加回 1） |
| 再加 (d) 的 29 個（你選的話） | 712 |

時間：完整跑 776 個用了約 180 分，平均每個約 14 秒（含重建）。退役 36 個約省 8 分（`rs-sym-elf-bss-unread` 在 macOS 本來就略過，不省時間；合併的 20 個省下一次建置，接手多跑一個案例幾乎不加時間）；再加 (d) 的 29 個，最多再省約 7 分。守衛突變只改 `.py`，重建幾乎不花時間，所以 (d) 實際省得比平均少。這只是平均估計，log 沒有每個突變的時間。

## 5. (d) 守衛腳本的突變（125 個，交你判斷）

AGENTS §3「ガードを立てる時期」（`0f400dd3`，已在 `origin/gyo2`）：綁實作方式的守衛（掃描名字、記號、結構的腳本）和守它們的突變，要等涵蓋那個機制的 scenario test 通過才立。第 2 批的宣告第 7 節已定「第 1 批已立的守衛不拆、照舊跑」；所以這裡**只問突變**，守衛本身不動。

scenario 第一段（S1-1～S1-10，走路計分）涵蓋的是登記與建 Plan、`Commands` 增刪 Resource、動態插入、當場生成與 `AllottedIdAlive`、換 World。下面的守衛守的都是**靜態性質**（名字、include、符號表），scenario 不會碰；嚴格照條文讀，**這 125 個都沒有 scenario 涵蓋**。

初稿把突變分成「核心」40／「細節」78，建議退細節。敵對式檢查指出三件事，我照它改了分法：
- 細節裡有 **fail-closed** 的（`ks-k5-unreadable-silent`、`ks-n1-unreadable-silent`），照初稿自己的定義該算核心。
- 細節裡有**子規則唯一的突變**：K3 的裸呼叫（`ks-adv-k3-srand-call`）、K5 的標準 header 清單（`ks-adv-standard-headers-widened`）、K1 與 K8 讀 CMakeLists（`ks-k1-cmake-unscanned`、`ks-k8-cmake-unread`）、K8 對 `.cpp` 與 Executor 檔、N1 的 namespace alias、N3 的 using、符號表的 FOREIGN 寬度與 ELF 路徑（`rs-sym-bss-unread`、`rs-sym-relro-counted`；Linux L1 跑的就是 ELF），以及 K8 的後半「Base 的執行緒序號」（`ks-k8-serial-state-unnamed`，S10 決定 3）。
- 細節裡有**真的抓到過漏報的**：審查 A1～A9 修掉的守衛缺陷（18 個，`docs/dev_logs/2026_11_13_arch_kernel_scaffold.zh-Hant.md:49`、`:53`）、K8 的 §14.2 檢查（`kernel-subtract-structure-declaration.zh-Hant.md:637`「找到 19 處」）、S10 修正的敵對式檢查（8 個）、target/world 第 6 節的 Fable 檢查（`we-adv-stamp-guard-camel-suffix`）。初稿要退它們，和第 6.2 節第 4 條「讀法的突變只在真的漏報過之後才加」矛盾。另有幾個是實樹或實際函式庫的正控制（`ks-s10-kernel-own-issuer`、`ks-k8-cmake-definition`、`rs-sym-darwin-data-unread`、`ks-s10-sym-*-pattern-dead-real`）。

修訂後的分法：**留**＝規則整個關掉、實樹正控制、fail-closed、子規則唯一的、真的抓到過漏報的；**可退**＝其餘（審查 B 系列等「沒漏報過的讀法與清單逐項」）。

| 規則 | 守的 DI | 綁實作方式的程度 | 留 | 可退 | 已在 (c) |
|---|---|---|---|---|---|
| K1 產品名、角色詞 | naming DI 4；AGENTS §6 產品邊界 | 低（邊界，不是機制） | 5 | 3 | 5 |
| K2 unordered 容器 | DI-KS 3（順序只來自 id 與 Plan） | 中 | 2 | 0 | — |
| K3 時鐘、亂數、執行緒 | DI-KS 15 | 中 | 3 | 2 | — |
| K4 static 變數（允許清單恰好兩個名字） | DI-KS 15、spec A.6 | 高（兩個具名符號） | 9 | 2 | — |
| K5 include 允許清單 | DI-KS 15；AGENTS §6 依賴方向 | 低（依賴邊界） | 10 | 3 | 1 |
| K6 log 呼叫 | error-handling DI 2 | 中 | 3 | 2 | — |
| K7 `far`／`near` | 無（Windows 的巨集，編譯衛生） | 低 | 2 | 0 | — |
| K8 `IdIssuer`、Base 的執行緒序號 | DI-KS 14（S10 的決定） | 高（名字） | 16 | 0 | — |
| K0 與掃描器本身 | J-9 | — | 3 | 4 | — |
| N1 namespace ↔ 目錄 | naming DI 1、3 | 中 | 9 | 7 | — |
| N3 角色後綴 | naming 規則 5 | 中 | 5 | 3 | — |
| 符號表（`kernel_symbols.py`，判定 9） | DI-KS 15（沒有可寫的全域資料，允許的兩個除外） | 高（符號名、段名） | 19 | 3 | 1 |
| `store_stamp_guard.py` | target/world 第 6 節 | 高（名字） | 3 | 0 | — |
| **合計** | | | **89** | **29** | **7** |

可退的 29 個與各自的緩的面（「自測失去唯一的突變」＝那個自測被刪或改鬆，不會有突變發現；守衛本身照跑）：

| id | 改壞的是 | 緩的面 |
|---|---|---|
| `ks-adv-line-comment-to-eof`、`ks-adv-block-comment-off`、`ks-adv-string-to-eof` | 掃描器處理 `//`、`/* */`、未結束字串（review B1、B20；block comment 那個只會多誤報） | 各自的自測失去唯一的突變 |
| `ks-adv-kernel-suffixes-shrunk`、`ks-adv-naming-suffixes-shrunk` | 只讀 `.cpp`／`.hpp`（review B7、B8） | 同上 |
| `ks-k1-camel-dropped`、`ks-adv-registry-case-sensitive` | K1 登記 id 的 CamelCase、大小寫（B9） | 同上；gyo2 沒有登記的產品，這兩個自測目前只守合成的登記檔 |
| `ks-adv-role-player-case` | K1 角色詞只認 `Player`（B13） | 同上 |
| `ks-adv-k3-headers-shrunk`、`ks-adv-k3-names-shrunk` | K3 的 header、名字清單少一項（B14、B15） | 同上（清單逐項由自測檢查，突變只證明自測會跑） |
| `ks-k4-type-unchecked`、`rs-b8-k4-inline-const-pointee` | K4 允許清單只比名字不比型別；const 指向物算 const | 前者的自測失去唯一的突變；後者同一自測還有 1 個 |
| `ks-adv-standard-header-in-quotes`、`we-k5-entity-store-prefix-unbounded`、`we-k5-backslash-parent` | K5：引號裡的標準 header（B19）、`entity_store` 少了結尾斜線、反斜線的 `..` | 第一個的自測失去唯一的突變；後兩個同一自測各還有 1 個（`..` 本身由 `ks-k5-parent-directory` 守） |
| `ks-adv-log-headers-cstdio`、`ks-adv-k6-set-log-handler` | K6 清單少一項（B17、B18） | 各自的自測失去唯一的突變 |
| `ks-n1-prefix-loose`、`ks-n1-nested-lost`、`ks-n1-missing-reverse-root-fails`、`ks-n1-plugins-dropped`、`ks-adv-stack-never-pops`、`ks-adv-reverse-roots-shrunk` | N1：前綴寬鬆、忘了外層 namespace、缺反向根時誤報、少掃 `plugins/`、括號不關（B3）、反向根少幾個（B6） | 各自的自測失去唯一的突變 |
| `ks-n3-suffix-dropped`、`ks-adv-n3-keywords-shrunk`、`ks-adv-n3-attributes-dropped` | N3：少 `Controller` 後綴、宣告關鍵字少幾個（B21）、不跳過屬性（B22） | 第一個的自測失去唯一的突變；後兩個同一自測還有 2 個 |
| `rs-sym-common-unread`、`rs-sym-tlv-kept`、`rs-b8-toolchain-any-format` | 符號表：Mach-O `__common`、`$tlv$init` 的名字、工具鏈名字套到所有格式 | 第一個同一自測還有 1 個；後兩個的自測失去唯一的突變 |

合計：退役後 107 種守衛 expect 裡，有 27 種沒有任何突變（上表 23 種，加上 3.2／3.3 的別名、`ks-real-product-name`、`we-k5-entity-store-dropped-selftest`）。逐個的分法與這張對照在證據檔 `guards_v2.py` 的輸出（第 9 節）。

**選項**：
1. **全留**（現狀）。
2. **退 29 個、留 89 個**（建議）。緩的面見上表：都是「某個自測被刪或改鬆不會被發現」，守衛的每條規則與子規則仍有突變。理由：這 29 個守的是掃描器的寫法與清單逐項，沒有一個抓到過真實的漏報；照「ガードを立てる時期」，這正是設計一改就要跟著改的那一層。
3. **綁實作程度「高」的四條（K4、K8、符號表、`store_stamp_guard`）連留下的都退**，守衛照跑但沒有突變證明，等 scenario 涵蓋執行緒或多個 World 同時存在時再立。比選項 2 再少 47 個（9＋16＋19＋3）；緩的面：這四條守衛整個壞掉也不會有突變發現，其中包括抓到過漏報的 K8 與 S10 那些。我不建議。

## 6. 以後怎麼控制增長（提案，不改 AGENTS 或規則文件）

### 6.1 增長從哪來（照 `git log -- mutations.json` 與 `what` 欄）

| 來源 | 例 | 本次盤點裡的量 |
|---|---|---|
| 突變清單的敵對式檢查後「同一個改法換一個案例再立一個」 | `sr-j32-*`、`sr-j8-*`（「added after the adversarial check of the mutant list」） | 3.2 的 28 個多數 |
| 每批各自為版本常數立一個 | C 指紋版本 4 個、Plan 版本 2 個 | 4 |
| 守衛的掃描器細節與清單逐項 | `ks-adv-*`（review A／B 系列）、`ks-k8-*` 13、符號式樣 `ks-s10-sym-*` | 78（其中 49 個抓到過漏報或是子規則唯一的，29 個不是） |
| 一個決定牽動好幾個 DI（S10 執行緒序號） | `ks-s10-*`、`ks-k8-*`、`bs-*` | 53 |
| 拿掉功能時沒跟著改寫 | `hf-startup-skipped`（第 2 批已改寫） | 1 |

### 6.2 加突變的判準（草案）

1. **一個改法一個突變**：find＋replace 已在清單裡就不再加。想證明另一個案例或另一句斷言也抓得到同一個改法，把那個案例加進原突變的 `cases`（逗號清單），那句訊息加進 expect（`(?=[\s\S]*A)[\s\S]*B`），不另立。只寫進 `what` 不夠：`what` 不會被執行（敵對式檢查指出）。前一個案例會讓程式中止（GYO_ASSERT）時合併不了，才另立。
2. **版本與格式常數**：每個常數一個突變；升版時改寫它的 find／expect，不每批再加。
3. **每條 DI 條文或 J 條目的下限**：條文裡每個可以獨立做錯的決定（一個條件、一個順序、一個預設值、一個錯誤碼、一個「不做」）至少一個突變，expect 指到守那一句的斷言。不設上限；但新增時要寫得出「它抓到的錯，清單裡哪一個都抓不到」，寫不出就不加。
4. **守衛腳本**：每條規則兩個突變——規則整個關掉、實樹上的正控制；fail-closed 各一個。每個子規則（規則裡會獨立做錯的一段分支或清單）一個。掃描器的讀法（註解、BOM、換行、屬性）只在**真的漏報過**之後才加（觀測到的壓力，AGENTS §2），加了就不因為「只是讀法」而退；清單的逐項由自測一條逐一檢查，突變只要一個。
5. **綁實作方式的守衛**：照 AGENTS §3「ガードを立てる時期」，守衛和突變一起延到 scenario 涵蓋之後。
6. **敵對式檢查的產出**：檢查者找到「改壞了沒有突變發現」時，先分辨是「沒有測試抓得到」（補測試＋突變），還是「另一個案例也該抓」（不另立，照第 1 條併進原突變）。
7. **拿掉功能時同一變更退役它的突變**，並做一次靜態檢查：每個突變的 find 只出現一次、套上 replace 之後編得過（可以在 `run_mutations.py` 加一個只套用不跑測試的模式；這是另一件事，本批不做）。
8. **每批自查**：合併前，本批新增的突變照 1～4 自查一次，數量與理由寫進 dev_log；一批新增超過「本批改動的 DI 條文數 × 5」時，在合併前核准裡說明。

### 6.3 哪些不必再問

照上面的判準，下面這些以後不必再問你：
- 同一個改法換案例（改成併進原突變）、版本常數再立一個、掃描器細節（沒有漏報過）、清單逐項——**不另立**，不用問。
- 拿掉功能時退役它的突變——**照做**，在 dev_log 寫一行。
- 改寫 find／expect 讓突變跟上改名或改訊息（守的性質不變）——照做，事後報告（AGENTS §14.4「沒有緩的面」）。

仍要問的：
- 退役會讓某句斷言或某個自測失去唯一的突變，而且不能合併（像第 5 節那 29 個、3.2 裡合併不成的）。
- 守衛規則本身的立、拆、延後（§3「ガードを立てる時期」）。
- 判準本身要不要寫進規則文件（風紀委員與你決定；本頁不改）。

## 7. 要你決定的事

1.～2. **已定（使用者 2026-11-18）：「那 31 個都退，照合併的做法」**——3.1 的 3 個＋3.2 的 28 個都退役，照合併的做法。比過的選項：只直接拿掉／照本頁的分法（直接退 8＋合併 20）／全部照合併。第 2 步的做法：
   - 被拿掉的那句斷言退役後沒有別的突變的，併進接手（3.2 的 20 個，加上版本常數那 3 個：`hf-c-version-kept`、`sr-c-version-not-bumped`、`hf-plan-version-kept` 的 J-54／J-55 案例也併進接手）。
   - 那句斷言仍有別的突變的（3.1 的 3 個、`we-w11-mut-no-mark-j30`、`ks-s5-applies-level-records`、`rs-j12-bundle-dropped`），併進去不加任何證明，直接退。
   - 判定是指令、和接手的指令不同的（`we-k5-entity-store-dropped-selftest`、`rs-sym-elf-bss-unread`），合併不了，直接退；緩的面照 3.2。
   - 合併試跑殺不掉的，原突變留下，不退役，在 dev_log 記下。
3. ~~3.3 的舊產品別名：K1 的 `PRODUCT_ALIASES` 要不要繼續擋舊產品的名字？~~ **已定（使用者 2026-11-18）：「舊產品別名不用擋了，那 5 個也退」**——退役 4 個別名突變＋`ks-real-product-name`。比過的選項：不擋（退 5 個）／擋（留 4 個、`ks-real-product-name` 改寫成用別名）。別名清單本身還留在守衛裡；把它從 `kernel_guard.py` 拿掉是守衛的改動，不在本批，列為後續。
   **補問（敵對式檢查指出當時漏了一個選項）**：`ks-real-product-name` 測的是 K1 的「登記 id」，不是別名。除了退役，也可以改寫成角色詞（例：在 `Plan.hpp` 放 `PlayerState`），留作 K1 唯一的實樹正控制。維持退役，還是改寫？（我的傾向：改寫，因為 K1 退役後沒有任何實樹的正控制；但你上次的決定是退，照你的決定也可以。）
4. 3.4 (e)：第 2 批已改寫 4 個；本批的 3 個是 `ks-real-product-name`（第 3 題），rs-sample 兩個改寫 expect。rs-sample 的改寫有緩める面（「failed without 標記」以前不算殺掉、現在算），1～5 寫在 3.4 表下；要你核准。`hf-startup-skipped` 我的判斷是留下（3.3 的說明）；你要退的話也可以，緩的面寫在那裡。
5. 第 5 節 (d)：選項 1、2（退 29，建議）、3？
6. 第 6 節的判準：交給風紀委員看，或先只在 GYO2 線的批次裡照做？

## 8. 敵對式檢查

檢查者：Fable 5.2／high，general-purpose、只讀（沒有改檔、沒有重跑突變；讀清單、原始碼、既有 log 與盤點檔）。作者是 Opus 5.5，模型不同。
結果：**找到 14 處，其中 8 處會失去檢出力**（其中 4 處失去的是「某一段測試被刪或改鬆不會被發現」，程式改壞仍由接手抓到）。全部照下面處理，已改進本頁。

| # | 指出的事 | 會失去檢出力？ | 處理 |
|---|---|---|---|
| 1 | `rs-sym-elf-bss-unread` 的接手 `rs-sym-bss-unread` 被初稿分在「細節」；兩邊一起退，ELF 的 `.bss` 改法就沒有突變，而 Linux L1 跑的就是 ELF | 是 | `rs-sym-bss-unread`、`rs-sym-relro-counted` 改列「留」（第 5 節） |
| 2 | `sr-j32-*` 四個：「案例的其他突變 22」是用 `cases` 字串相等算的，那 22 個在別的 SUBCASE；四個一起退，那個 SUBCASE 的四句 CHECK 全部沒有突變 | 是（測試層） | 改成合併進接手（3.2） |
| 3 | `we-j5-boundary-off`：J-5 的 `BoundaryChange` 斷言只有它 | 是（測試層） | 合併（3.2） |
| 4 | `sr-j8-*` 四個一起退，J-8 範圍區塊五句只剩一句有突變 | 是（測試層） | 合併；`sr-j8-allots-later` 也是（3.2） |
| 5 | `ks-s5-j45-spawn-deferred`、`ns-absent-resource-present` 的「1」是別的 ASSERT 突變；共同原因是粒度不到斷言。用斷言粒度重算，31 個同改法候選裡只有約 6 個那句斷言還有別的突變。建議合併（`cases` 逗號清單＋expect 串接），比「寫進 `what`」強 | 是（測試層） | 3.2 全部改用斷言粒度；20 個改成合併；第 6.2 節第 1 條照改 |
| 6 | 3.1 的 7 個裡 4 個不符合「同一個案例、同一句」 | 否（說法） | 4 個移到 3.2；(a) 3、(c) 33，總數不變 |
| 7 | K8 的後半「Base 的執行緒序號」只剩細節突變 | 是 | `ks-k8-serial-state-unnamed` 改列「留」（K8 全留） |
| 8 | `ks-k5-unreadable-silent`、`ks-n1-unreadable-silent` 是 fail-closed，卻在細節；前者是審查 A3 的實際缺陷 | 是 | 改列「留」 |
| 9 | K3 的裸呼叫、K5 的標準 header 清單與 `..`、FOREIGN 寬度、K8 讀 CMakeLists／`.cpp`／Executor 檔、N1 alias、N3 using／typedef：子規則唯一的突變在細節 | 是 | 改列「留」（`ks-k1-cmake-unscanned` 同理一併） |
| 10 | 細節裡有真的抓到過漏報的（審查 A 系列 18、K8 檢查 12、S10 檢查 8），和第 6.2 節第 4 條矛盾 | 否（實質損失已算在 7～9） | 選項 2 改成只退沒漏報過的 29 個；第 6.2 節第 4 條補「加了就不因為只是讀法而退」 |
| 11 | 選項 2 的敘述「每條規則的整個關掉與實樹正控制仍有突變」不成立；細節裡有實樹正控制；守衛 expect 有 69 種會沒有突變，應附對照 | 否（說法） | 實樹正控制改列「留」；附上 29 個的緩的面表與「27 種 expect 沒有突變」（第 5 節、`guards_v2.py`） |
| 12 | rs-sample 改寫「緩的面：無」不準：有締める面與緩める面；沒提 `50cb2050` 的前例；只在 macOS 驗過 | 否（說法） | 3.4 表下補 §14.4 的 1～5、前例與限制；第 7 節第 4 題改成要你核准 |
| 13 | 第 7 節第 3 題漏了「`ks-real-product-name` 改寫成角色詞」這個選項；退役後 K1 沒有實樹正控制（但它現在就已經 survived） | 否（說法） | 3.3 說明；第 7 節第 3 題補問 |
| 14 | 小出入：1.3「其他 47 條」應是 56；26 組 vs 24 組沒說明；`rs-sym-elf-bss-unread` 在 macOS 省 0 秒；`0f400dd3` 已在 `origin/gyo2` | 否（說法） | 都改了（1.3、第 2 節、第 4 節、第 5 節） |

檢查者也確認了：24＋2 對 find／replace 相同的接手在完整跑都是 killed；3.2 初稿標「0」的 8 列是對的；`hf-startup-skipped` 留下的描述與 `b4b80832` 一致；主要數字自洽。

## 9. 證據

主 checkout `build/evidence/gyo2-mutation-prune/`（git 不追蹤；本機 macOS）：
- `inventory.py`、`inventory.tsv`：777 個的逐個盤點（id、批次、類、檔案、守的條文、判定、expect、完整跑的結果）。
- `candidates.py`：初稿的退役候選與「案例的其他突變」（`cases` 字串相等）的計算。3.2 的斷言粒度重算是敵對式檢查者做的，沒有另存腳本；檢查者的完整回報也未另存，要點在第 8 節。
- `guards.py`：守衛突變的分組與初稿的核心／細節分法；`guards_v2.py`：敵對式檢查後修訂的「可退 29 個」與「每種 expect 退役前後剩幾個」。
- 完整跑：主 checkout `build/evidence/gyo2-orchestration-skeleton/mutants-full-kernel-cc5fe0e1.log`（第 2 批卡片跑的，`cc5fe0e1`）。
- `rewrite_check.json`、`rewrite_check.log`：(e) 的 5 個改寫，在本分支（`8b912a1e`）用臨時清單跑的結果（5/5 killed；rs-sample 兩個第一次用沒有 `\s+` 的 expect 是 wrong_failure，log 一起留著）。
- `rs-sample-capture-output.log`：手動套用 `rs-sample-capture-accepted` 時的建置輸出（看實際失敗訊息用）。

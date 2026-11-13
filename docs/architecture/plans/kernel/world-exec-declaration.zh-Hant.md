# 事前宣告：`v8-arch-kernel-world-exec`（World 與 Serial Executor；存取驗證兩層）

更新：2026-11-13。Owner：架構（架構師會話寫宣告；實作建議由架構師開的任務卡執行）。**狀態：已核准（使用者 2026-11-13「核准」；含第 4 節第 2 點的實作檔位 Opus／xhigh）。**

出處：遷移計畫 `migration-plan.zh-Hant.md` §3.7（本批）、§3 共通、§4 Delta-1、Delta-3、§6 BU-2；`target/kernel.zh-Hant.md`（DI-KS 2、3、4、6、9、10、11、12、14 與 RC 1、6、9、10）；`target/world.zh-Hant.md`、`target/system-contract.zh-Hant.md` §2、§2b、`target/error-handling.md`、`target/limits.zh-Hant.md`（LIM-3、LIM-6）；spec §A（World）、§E（Executor）、§F（延後佇列、hook、事件）、J.2、J.3；kernel `HANDOFF.md` 現況「registry-schedule 交給 world-exec 的事」、決定行 `v8-arch-migration-plan-undecided` 的 U1。

## 1. 目的（Feature → Contract → Architecture）

- **Feature**：跨產品能力「Match 與 Client 的預測共用同一份模擬核心」（object_fps_pvp 的 Match 與 Client）；「進 digest 的狀態只有一個寫者」成為可執行的檢查（AGENTS §3 State Authority 的機制版）；下一版的 loading 分批載入需要的延後佇列與同步點。開發者可見：`gyo_kernel_tests` 真正執行 System、World 的守衛三平台綠；驗證開與關兩個 World 在同一個執行檔裡比較。
- **Contract**：World（spec A.1～A.5：鍵 u64、欄依 id 升冪、同步點後參考失效、SoA 的 keys／值／stamps）；延後佇列的套用順序（spec F.2：hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource；每階段依 id，同鍵依 (planPos, recSeq)）；事件（F-7～F-9）；hook（F.4）；Serial Executor（E.2，含第 ⓪ 步呼叫前比對與第 ⑤ 步呼叫後比對）、`Result<void, KernelError>`、`Interrupted`（E.5）；存取驗證兩層與句柄 epoch，違規以 Err 回傳、帶 World 的身分；`KernelSettings` 的兩個參數（驗證開關、`breakOnKernelError`）與四條件（LIM-6）；`DiagnosticsWriter<T>` 的 World 端緩衝，`Interrupted` 時不丟。
- **Architecture**：遷移計畫 §4 Delta-1 續（使用者已同意）；Delta-3（World 的欄儲存重用 `engine/entity_store` 的演算法；使用者已同意）。欄配置走一個小介面（一般配置／可換獨立頁），本批只走一般配置（U1：頁保護延到 G2a 後決定）。

## 2. 範圍

| 範圍 | 內容 |
|---|---|
| `engine/kernel/`（新增與修改） | World、欄儲存、延後佇列與同步點套用、事件緩衝、hook、Serial Executor、存取驗證兩層、句柄 epoch、`KernelSettings`、`DiagnosticsWriter<T>` 的緩衝、欄配置介面；`CMakeLists.txt` 加連結 `GYO::EntityStore` |
| `engine/entity_store/` | 只有在選「回傳位置的查找版本」時加一個小介面（Delta-3 的差分寫在報告）；選 `Cell<T>` 包法時不動 |
| `tests/common/kernel/` | 第 6 節的 J 條目、J-32 的三平台 golden、BU-2 的記錄執行檔（不進 ctest）、符號層檢查收回成「恰等於兩個」、突變目錄 |
| spike 測試的搬改 | `spikes/kernel/tests/AccessCheckTests.cpp` 搬改（斷言改成 Err；兩個執行檔改成一個執行檔兩個 World）；`RegistryWorldTests.cpp` 的 J-7 部分搬改，F-2／F-7 部分重寫（spike 的語意與規格相反）。`spikes/kernel/` 本身不刪 |
| target 文件 | `target/world.zh-Hant.md` §6 的欄佈局（兩種之一）寫成定案；不改已核准的 Design Invariants，要改的停下交使用者 |
| 其他 | `docs/architecture/plans/kernel/scripts/mutations.json`；dev_log；kernel README 與 HANDOFF |

不動：`build/`、`.github/`、`plugins/`、`apps/`、`tools/`、`framework/`。範圍外還要改檔才能完成＝停止條件。

## 3. 從前一批接手的事（kernel HANDOFF 現況已記）

1. **符號層檢查收回成「恰等於兩個」**：本批加入 `tlCurrent` 後，`kernel.symbols` 的 ctest 加 `--require tlCurrent`（締める面）。
2. **三個執行半條列進本批判定**：J-11 的「全量對增量重建」等價（W-11）、J-12 的「兩種參數順序下 `Fetch` 沒有順序相依」（B.4 第 7 點）、J-44 的「World 摘要不變」（D-9）。
3. **配發器的獨占（registry-schedule 的 C5）不在本批定**：本批沿用前一批較嚴的現狀（配發器預設 Exclusive，兩個會生成實體的 System＝第二寫者錯誤），合成測試照前一批的做法把配發器設 Shared 並加註；PvP 的 `ApplyMembership` 用 `SpawnWithId`，不經配發器。這一題我會在下一次改 spec 時交使用者（見第 4 節）。

## 4. 要使用者定的事

1. **核准本宣告。**
2. **實作者檔位 Opus／xhigh**（遷移計畫 §3.7 的指定：F.2 的套用順序、stamp 取值、驗證比對屬於決定性與時序類）。navigator 規定檔位超過主會話時要同意。
3. **不在本批、但會碰到的一題（不擋開工）**：配發器在獨占計數裡怎麼算——兩個會生成實體的 System，算「有衝突、要排序」還是「第二寫者錯誤」（spec W-9「由衝突序列化」與 DI-KS 4、6 的語義）。本批照第 3 節第 3 點沿用較嚴的現狀。

## 5. 守衛與突變的去處（計畫 §3 共通）

| 對象 | 去處 | 說明 |
|---|---|---|
| `entity_store_guard.py`（W1～W5） | **留** | 性質對 store 仍成立；World 不放進 `engine/entity_store` 目錄；Delta-3 加的小介面照舊受掃描 |
| `gyo_expect_links(gyo_entity_store GYO::Base)` | **留** | — |
| `gyo_expect_links(gyo_kernel GYO::Base)` | **改寫**（加 `GYO::EntityStore`） | Delta-3 的新邊；締める面以外的改變照實寫在報告 |
| `kernel_guard.py`（J-9）、`naming_guard.py` | **留** | 新程式要零命中；K4 白名單已含 `tlCurrent` |
| `kernel.symbols` | **改寫**（收緊） | 第 3 節第 1 點 |
| spike 的兩個測試 | **改寫** | 第 2 節 |
| 新守衛：第 6 節的 J 條目、J-32 golden | **新增** | 突變一覽受一次敵對檢查（第 8 節） |
| `es-row*` 突變 15 條 | **重列**（若選回傳位置版本而改動 store 的 API） | `find` 文字會過期；選 `Cell<T>` 時不動 |

## 6. 判定（看結果前宣告）

| # | 判定 | 門檻 |
|---|---|---|
| 1 | 範圍 | `git diff --name-only origin/master...HEAD` 全部落在第 2 節（Python 比對，先以一個已知範圍外的路徑當正例）；範圍外 0 檔 |
| 2 | L1 三平台 | linux-x64（含 core preset）、windows-x64、macos-arm64 全綠；下列 J 條目在三份 `tests.xml` 各出現且通過：J-1～J-5、J-6 的 Running 半（ownerThread 半在 threading）、J-7、J-8、J-26、J-29～J-37、J-32b、J-45、J-50、J-52、J-53，以及第 3 節第 2 點的三個執行半條；件数の突き合わせ寫在報告第一行 |
| 3 | 存取驗證 | J-5：spec 的三種繞路在驗證開著時以 Err 攔下、報告帶「驗證開著」欄位；關著時回 Ok；同一執行檔兩個 World；旁邊印 `IsDebuggerPresent()`（ci-readiness §3.2 的推斷變證據） |
| 4 | 套用順序的決定性 | J-32 的 golden 三平台逐位元相同；J-44 的 20 種排列下 World 摘要不變 |
| 5 | `Interrupted` 與 release | J-45：release 目標也看得到 Err 與 `Interrupted`；`DiagnosticsWriter<T>` 的 World 端緩衝在 `Interrupted` 後仍可讀 |
| 6 | 符號層 | `kernel.symbols` 加 `--require tlCurrent` 後通過：可寫資料段的符號恰等於兩個（Linux、macOS） |
| 7 | 突變 | 本批突變目錄在本機全殺（預期失敗可識別才算）；必含 spec J.3 第 1、7、8、9、12、13、16 條；敵對檢查加入的處數與加入後的檢出數寫進報告 |
| 8 | BU-2 | Kernel 自身開銷照遷移計畫 §6 的量法（合成 Registry、記錄執行檔、不進 ctest）量並記錄；**本批只記錄、不判**——量測機器的規則已由 `v8-arch-u8-gate-in-linux-container` 改為 Linux 實機上的容器，預算本身待 U12 重核，本批在本機量的數字只當參考 |
| 9 | 既有守衛不變紅 | 由判定 2 的 L1 涵蓋 |

掃描類判定照 AGENTS §14.2：`git grep -P` 或 Python、不丟 stderr、以結束碼區分「無」與「失敗」、先以已知正例命中。

## 7. 範囲の限界

- 共通三條（AGENTS §14、單一問題累計超過 2 h、總時數超過估時上限 1.5 倍）。
- 第一層在 spike 是斷言，本批直接照規格以 Err 回傳。
- L7（其他執行緒的存取當成 host）到 threading 才評估。
- 回滾（H.5）與 Extract（H.6）不實作，契約已在 spec。
- 頁保護不在本批（LIM-3 的 L1、L6 仍不解除；U1 延到 G2a 後）。
- 配發器的獨占沿用較嚴的現狀（第 3 節第 3 點）。
- BU-2 的數字只當參考（第 6 節判定 8）。

## 8. 模型／檔位

上段（Design Invariants、資料佈局、錯誤契約）。

- 實作者：**Opus／xhigh**（待使用者同意）。建議由架構師開任務卡執行。
- 審查者兼突變一覽的敵對檢查者：**Fable／xhigh，只讀，一次**（與作者 Opus 不同模型；照使用者同意的臨時做法，Fable 每批只用一次，審查與敵對檢查合併，報告分兩節）。
- 驗證者：只在改 golden、守衛判定或 DI 守衛時出場，Opus／high 另一代理。

## 9. 估時與停止

- 估時：**25～50 h**（遷移計畫 §2.2，推定）＋守衛去處判斷 1～2 h＋接手的三個執行半條 2～4 h（推定）。總時數上限 56 h×1.5＝84 h。
- 停止：MSVC 的 `std::launder`／`static inline char` 在 RelWithDebInfo 第一次 L1 紅→先切分是實作、測試還是環境（AGENTS §14.1），不重跑湊綠；J-2 或 J-32 要改 World 的資料模型才能過→停（spec K.2 第 1 條），交使用者「修還是重建」；核對 W-1～W-11 時發現 EntityStore 撐不住 SoA、要 archetype→停（spec M、K.2 第 6 條）；範圍外還要改檔；放寬既有守衛；AGENTS §14 冒頭三條。

## 10. 若失敗

loading、rollback-extract、四個 host 都假設 World 與 Executor 照契約運作，會白做；plan-dryrun（閘門 G1）只用到 Registry 與 Plan，不受影響。

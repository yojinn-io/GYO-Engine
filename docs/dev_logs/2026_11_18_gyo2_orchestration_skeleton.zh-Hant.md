# 編排層的骨架（`v8-arch-gyo2-orchestration-skeleton`）

日期：2026-11-18～。分支 `claude/gyo2-orchestration-skeleton-67744c`（從 `gyo2` 0f1f2d21）。**狀態：進行中——第 1～4 步完成；待完整突變、G-7、第 3、4 步的 G-8 與合併前核准。**

這一批由任務卡做（GYO2 主會話開出）。事前宣告：[orchestration-skeleton-declaration.zh-Hant.md](../architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md) 修訂 7；使用者 2026-11-18「核准」（動工前核准；主會話轉達）。修訂 0 送 Fable／high 敵對式檢查（必改 10、建議改 16、小問題 8，報告 `declaration-r0-adversarial-check.md`）。實作者 Opus／high。

用詞（2026-11-18 主會話轉達的更正）：宣告正文的「段 1～段 4」＝本頁的「第 1～4 步」；「第一段／第二段核准」＝「動工前核准／合併前核准」。scenario test 的「第一段」是 scenario 的段名（ctest `scenario.segment1`），不是批內的步。

## 做了什麼（到目前為止）

| 步 | 提交 | 內容 |
|---|---|---|
| 第 1 步 編排層本體 | bb9a5811 | `engine/orchestration`：`Module`／`Plugin`、`Product`（World 群、每個 World 一份 Registry、build → BuildPlans → Finish → Check、`ProductStartupReport` 收齊 O-E1～O-E7）、遊戲初始化與每幀清單、只跑一次的 runner、準備時機、`ReplaceWorld`（重新登記、BuildNewFirst／DropOldFirst、`beforeSwap`、`CarryOver<T>`）、`InsertSystems`（拒絕或建 Plan 失敗記一次 `Engine::Base::Log` 警告）。行為測試 `tests/common/orchestration/`（判定 2 的讀檔案例在 `RestoreFlowTests.cpp`） |
| 第 1 步 的測試補強與突變清單 | 2c0ab9c7 | 失敗訊息加專屬標記、加「遊戲初始化只在第一個 World」一案、插入丟例外後先在同一份 Registry 再插一次；突變清單 `docs/architecture/target/v2/scripts/orchestration_mutations.json`（40 個，只給行為測試；7.4 的掃描守衛延後） |
| 第 1 步 沒殺掉的 7 個 | d57356b2 | 測試加標記、先檢查非空、看規則專屬的訊息；40/40 |
| 第 2 步 Kernel 減法 | 81e1d231 | 拿掉開局排程（`startup`、`RunStartup`、J-58 等）與登記單位（`Plugin`、`FinishContext`、`Registry` 的 vector 與 `Finish()`、R-E1／R-E3／R-E6）；測試輔助改兩步（`RegisterAll`）；指紋 Registry 4、Plan 6、C 8，匯出 `gyo.kernel.plans 4`；golden 更新 |
| G-8 找到的缺陷 D1、D3、D4 | 01ce5137 | 各先寫失敗的案（Red，`segment1/g8-defects-red.log`）再修（Green，`g8-defects-green.log`）：D1 替換中斷的 World 失敗時不寫回它（值留在暫存處）；D3 `extra` 丟例外時記一次警告再往外傳；D4 第一個 World 的位置是空的時遊戲初始化回 `WorldMissing`、不記為已跑。D5（`UnitSlot::isModule` 只寫不讀）拿掉 |
| G-8 的測試補強與突變 | 本提交 | 新 `CoverageTests.cpp`（約 20 案：O-E6 三種、只有 Kernel 錯誤、`privateDepends`、Plugin 依賴 Plugin、產品的 build 最後、每幀清單的插入／錨點／錯誤、`RunSchedule` 的中斷與空位、`UnknownWorld`、預設 runner 的失敗等）；W1～W13 的處理（標記、非空檢查、四幀、比對碼與警告訊息）；突變清單加 52 個 G-8 候補與 D1／D3／D4 各一個，共 95；Kernel 與編排層的 DI 文字草稿（`target/kernel.zh-Hant.md`、新頁 `target/v2/orchestration.zh-Hant.md`；最終文字在合併前核准） |

| 第 3 步 登記檔 | 本提交 | `engine/config/modules.csv`（十列，全部 `library_only=yes`）、`plugins.csv`（只有表頭）；`build/cmake/GyoUnitRegistry.cmake`（讀取與驗證、`gyo_unit_selection`、`gyo_link_registered_dependencies`、`gyo_product_units`）；`engine/CMakeLists.txt` 照登記檔的列序加子目錄，各 Module 的引擎內 link 換成 `gyo_link_registered_dependencies`（第三方的 link 照舊）；`GyoBuild.cmake` 讀登記檔、Plugin 從 `plugins/<name>` 加；測試 `cmake.UnitRegistry`（`tests/common/cmake/UnitRegistryTests.cmake`）與 `orchestration.units`（合成登記檔 `tests/common/orchestration/units/`）；Kernel 標頭改寫 `::Engine::`（下述缺陷）與 include 順序的編譯守衛 |
| 第 4 步 scenario 第一段與文件 | 本提交 | `tests/common/scenario/`（`WalkerGame.hpp`：「走路計分」，產品自己的登記；`ScenarioTests.cpp`：S1-1～S1-10、S1-6c、S1-7b；ctest `scenario.segment1`）；說明頁第 13 節「編排層」、第 11.5 節與其他寫著 Kernel 開局的地方改掉；`docs/cross-component-rules.md` CCR-3 寫明編排層是第一個實作者；v2 README 的索引 |

## 驗證與經過

- 基準（0f1f2d21 的程式；第一次跑時我的新標頭已在，`kernel.naming_guard` 因它在 `GYO::Kernel` 做前向宣告而失敗——改成 include 後通過）。
- 第 1 步 之後：`ctest --preset test -L "cpu|shader"` 110 項全過（原 98 項＋編排層 12 組）；編排層 doctest 37 案、273 個斷言全過（macOS）。
- Red（G-2）：`Product.cpp` 換成空實作時 6 案失敗、`World()` 的斷言讓行程中止（`segment1/red-stub.log`）；換回後全過（`segment1/green.log`）。
- 實作中照第 8.4 節的「不會用」改了兩處測試（機制不動）：合成遊戲的 `SpawnRock` 與 `SpawnBadId` 都生成帶 `Pos` 的實體，被 D-E2 算成兩個寫者（DI-KS 6）——`SpawnBadId` 改用 `Junk`；`SaveData` 含 `std::vector`，World 摘要雜湊 Resource 的位元組會帶堆積位址——改成固定大小的陣列。
- 實作細節（合併前核准時看）：帶過去的值要從**已中斷**的新 World 取回時（讀檔的 schedule 失敗），Kernel 只准讀（E.5），所以經讀取的檢視把值移出、不寫回那個 World（它隨即銷毀）；`RunGameInit`／`RunFrame`／`RunSchedule` 回傳 `OrchestrationError`（宣告草圖寫 `KernelError`），Kernel 的錯誤放在 `OrchestrationError::kernel`。
- **第 1 步 的突變：40 個殺 33**（`segment1/mutants-segment1.log`）。沒殺掉的 7 個還沒處理（停在這裡）：
  - 活下來 1：`or-oe3-module-on-plugin`。推測：`RegisteredUnits` 裡 Module 一定排在 Plugin 前，拿掉「Module 依賴 Plugin」的檢查後，同一處仍以「依賴在後」報 O-E3；測試只看有沒有 O-E3。要改測試看訊息。
  - 預期之外的失敗 6：`or-oe2-file-and-library`、`or-oe5-anchor-silent`、`or-oe5-gameinit-unchecked`、`or-oe7-silent`、`or-carry-not-given-back`、`or-owner-not-checked`。推測：失敗出現在沒有標記的 `REQUIRE_FALSE`，或空指標當掉（帶過去的值沒移回時 `Resource<KeyBindings>()` 是空的），在專屬標記印出之前就結束。這一輪的日誌沒有存每個突變的輸出，原因待下一次用 `--only` 重跑確認。
- 7 個重跑並存下輸出（`segment1/unkilled/`）：6 個是測試寫法——失敗出在沒有標記的 `REQUIRE_FALSE`（5 個），或帶過去的值沒移回時解參照空指標當掉（1 個）；活下來的 1 個是測試只看 O-E3、分不出是哪一條。照第 8.4 節的「不會用」改測試（加標記、先檢查非空、看規則專屬的訊息），機制不動。重跑 7/7 在預期的失敗下殺掉（`segment1/unkilled-7-after-fix.log`）。**第 1 步 的突變 40/40。**
- **第 2 步**：`ctest --preset test -L "cpu|shader"` 全過（`segment2/ctest-segment2.log`）；G-3 的 Kernel 掃描 19 個式子都 0 命中，每個式子先在 0f1f2d21 用已知例命中一次（`segment2/g3_scan.py`、`g3_scan.log`）。
- **G-8（Fable 5.2／high，唯讀，讀 d57356b2）**：找到突變候補 56 個（預期活下來 39）、測試弱點 13 處、疑似缺陷 5 處（報告 `segment1/g8-check-fable-high.md`）。處理：
  - 候補 56 個加進清單 52 個；4 個不加，理由寫在清單各自的 `what`（與既有突變等價，或只會崩潰、沒有可認的失敗）。
  - 弱點 W1～W13 全部改測試（機制不動）。
  - 缺陷 D1、D3、D4 照 §14.3 修（01ce5137）；D5 拿掉。
  - **D2 不改，合併前核准時請使用者確認讀法**：`RunFrame` 碰到空位或中斷的 World 就停下回傳，排在後面的 World 那一次不跑。宣告 3.4 寫「有錯就停下回傳」，7.3 寫「`RunFrame` 回 `WorldMissing` 且別的 World 照跑」；我讀成「別的 World 之後照樣能用 `RunSchedule` 跑、不被拖垮」，案 `O-8: RunSchedule on an interrupted or empty World; another World still runs…`（`CoverageTests.cpp`）守這個讀法。
- **完整突變 95 個：94 殺、1 活**（`segment1/mutants-after-g8.log`）。活的 `g8-anchor-last-occurrence`：重複錨點只測了 `InsertAfter`，補 `InsertBefore` 的那幾行後殺掉（`mutants-after-g8-anchor-fix.log`）。**95/95。**

- **第 3 步**：
  - `ctest --preset test -L "cpu|shader"` 112 項全過（原 110＋`orchestration.units`、`cmake.UnitRegistry`；`segment3/ctest-segment3.log`）。
  - **G-3 的 link 比對**（`segment3/g3_links.py`、`g3_links.log`）：引擎各 CMakeLists 的 `target_link_libraries(` 裡的 `GYO::` 別名在 0f1f2d21 命中 13 個（8 個檔），現在 0；configure 後十個引擎 target 的 `LINK_LIBRARIES`／`INTERFACE_LINK_LIBRARIES`（`link_dump.cmake` 在 configure 結束時傾印，`links-before.txt` 取自 01ce5137 的副本、`links-after.txt`）**集合逐一相同；順序只有 `gyo_threads` 不同**（`GYO::Time;Threads::Threads;GYO::Os` → `GYO::Time;GYO::Os;Threads::Threads`）：登記的依賴一次 link 完，第三方的 `Threads::Threads` 排到後面。宣告寫「逐一相同」，這一點合併前核准時報告（link 的先後不改變連結器拿到的東西：CMake 依依賴排靜態函式庫）。
  - **缺陷（§14.3）：Kernel 標頭在先開了 `GYO::Engine` 的翻譯單位裡編不過**。產生的 `<target>_units.cpp` 先 include 編排層的標頭，`namespace GYO::Engine::Orchestration` 一開，Kernel 標頭在 `GYO::Kernel` 裡未限定的 `Engine::Base`、`Engine::EntityStore` 就先找到 `GYO::Engine`、編譯失敗（Red：`segment3/header-order-red.log`）。編排層是第一個 `GYO::Engine` 的 Module（N17；帶過來的模組根仍是 `::Engine`，架構師排的機械改名在之後），所以第一次出現。修法：Kernel 公開標頭的 49 處改寫 `::Engine::`（行為不變；`src/` 不改，因為它不會先開 `GYO::Engine`）；Kernel 突變清單裡 find 含這幾行的 3 個跟著改（改前改後都恰好命中一次）。守衛：`gyo_orchestration_include_order`（每個編排層公開標頭先 include、再 include 全部 Kernel 公開標頭，各一個翻譯單位，隨測試一起建）；把 `WorldExecutor.hpp` 的一處改回未限定時它編不過（7 處錯誤，`include-order-mutant.log`），還原後通過（`include-order-green.log`）。之後的 `GYO::Engine` Module（例：權威 Module）在改名批之前都會碰到同一件事，轉交主會話。
  - **第 3 步 的突變 19 個：19 殺**（`segment3/mutants-segment3.log`、`mutants-segment3-expect-fix.log`）。登記檔的讀取與驗證 10、帶哪些單位 4、產生的表與 link 4（`cmake -P` 經突變執行器的 `command` 跑），include 順序 1（建置失敗才算殺）。第一輪 3 個是 `expect` 寫錯（拿掉欄數檢查後是讀欄時另一處失敗、`maybe` 讀成 `no` 後以「進 World 卻沒有單位」失敗、CMake 把長訊息換行），改成各自專屬的式子後殺掉；測試不動。
  - 和宣告不同的小處：測試名照 `tests/common/cmake` 既有的命名寫成 `cmake.UnitRegistry`（宣告判定 1 寫 `cmake.unit_registry`）；突變執行器 `run_mutations.py` 的 `command` 加 `{cmake}` 佔位符（向後相容），讓登記檔的突變能跑 `cmake -P`。

- **第 4 步**：`ctest --preset test -L "cpu|shader"` 113 項全過（`segment4/ctest-segment4.log`）。scenario 第一段第一次跑就全過（2 案、128 個斷言；`BuildNewFirst` 與 `DropOldFirst` 兩個子案 S1-7～S1-9 的結果相同）。第 8.4 節的三種失敗都沒有出現。實作時的細處（照 L17 自己判）：
  - 「第一幀之後 `BestScore` 存在」另外用 `Peek`（每幀清單裡、`CountScore` 之後、同步點之前）記下每一幀看不看得到，第一幀看不到、之後看得到，讓「同步點前就看得到」的突變能被抓到。
  - `KeyBindings` 第 3 分時由每幀的 `OptionsMenu` 改成 7（模擬選項選單）；描述的準備只在沒有時放預設值 0。
  - S1-7b 的讀檔：`extra` 自己用 World API 放回玩家，交給 `LoadSave` 的存檔標「沒有玩家」，所以 `LoadSave` 只放分數、生成游標——游標拿到 id 1（活著）→ `AllottedIdAlive`，`LoadSave` 處理、World 照跑、`RejectedSpawns` 是 1。
  - 「舊 World 已銷毀」用描述的準備放的 `WorldLife`（最後一份被銷毀時計數）。
- **第 4 步 的突變**（判定 3 的十類，S1-10 分「沒帶過去」與「被準備蓋掉」兩個，共 12）：**12/12 殺**（`segment4/mutants-segment4.log`）。「被準備蓋掉」改的是小遊戲自己的準備（O-7 是作者的責任），其餘改機制。
- **和宣告第 4 節不同**：`docs/architecture/target/README.md` 的目標 Design Invariants 清單沒有改。那張表是 v2 之前的（只有 DI-KS 1～15，DI-KS 15 那一列還寫「Host 保留多時鐘」），本批改的 DI-KS 15、拿掉的 DI-KS 22 在它裡面不是同一個樣子；照 `pre-v2-target-docs-later` 先不動，合併前核准時一併報告。
- **別家對照**：第 8.2 節的表（宣告時做，出處在 `scenario-cross-engine.md`）；scenario 第一段沒有新增步驟，沒有新的分歧。

## 下一步

1. 完整突變（清單全部）；G-7（Fable／high）；第 3 步 的 G-8（背景進行中）、第 4 步 的 G-8。
2. 合併前核准的資料：第 7.1 節各組、第 7.2 節的 DI 守衛各列、第 10 節的最終文字、D2 的讀法、G-3 的 `gyo_threads` 順序、Kernel 標頭的 `::Engine::`。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-orchestration-skeleton/`（git 不追蹤）：`inventory-startup-plugin.md`、`declaration-r0-adversarial-check.md`、`scenario-cross-engine.md`；`segment1/`：`baseline-build.log`、`baseline-ctest.log`、`ctest-segment1.log`、`red-stub.log`、`green.log`、`mutants-segment1.log`、`unkilled/`、`unkilled-7-after-fix.log`、`g8-check-fable-high.md`、`g8-defects-red.log`、`g8-defects-green.log`、`mutants-after-g8.log`、`mutants-after-g8-anchor-fix.log`；`segment2/`：`ctest-segment2.log`、`g3_scan.py`、`g3_scan.log`；`segment3/`：`link_dump.cmake`、`links-before.txt`、`links-after.txt`、`g3_links.py`、`g3_links.log`、`header-order-red.log`、`include-order-mutant.log`、`include-order-green.log`、`ctest-segment3.log`、`mutants-segment3.log`、`mutants-segment3-expect-fix.log`；`segment4/`：`mutants-segment4.log`、`ctest-segment4.log`。

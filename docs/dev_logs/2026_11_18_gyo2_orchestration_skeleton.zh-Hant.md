# 編排層的骨架（`v8-arch-gyo2-orchestration-skeleton`）

日期：2026-11-18～。分支 `claude/gyo2-orchestration-skeleton-67744c`（從 `gyo2` 0f1f2d21）。**狀態：進行中——段 1、段 2 完成；段 1 的 G-8 處理完（突變 95/95）；進行段 3（登記檔）。**

這一批由任務卡做（GYO2 主會話開出）。事前宣告：[orchestration-skeleton-declaration.zh-Hant.md](../architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md) 修訂 7；使用者 2026-11-18「核准」第一段（主會話轉達）。修訂 0 送 Fable／high 敵對式檢查（必改 10、建議改 16、小問題 8，報告 `declaration-r0-adversarial-check.md`）。實作者 Opus／high。

## 做了什麼（到目前為止）

| 段 | 提交 | 內容 |
|---|---|---|
| 段 1 編排層本體 | bb9a5811 | `engine/orchestration`：`Module`／`Plugin`、`Product`（World 群、每個 World 一份 Registry、build → BuildPlans → Finish → Check、`ProductStartupReport` 收齊 O-E1～O-E7）、遊戲初始化與每幀清單、只跑一次的 runner、準備時機、`ReplaceWorld`（重新登記、BuildNewFirst／DropOldFirst、`beforeSwap`、`CarryOver<T>`）、`InsertSystems`（拒絕或建 Plan 失敗記一次 `Engine::Base::Log` 警告）。行為測試 `tests/common/orchestration/`（判定 2 的讀檔案例在 `RestoreFlowTests.cpp`） |
| 段 1 的測試補強與突變清單 | 2c0ab9c7 | 失敗訊息加專屬標記、加「遊戲初始化只在第一個 World」一案、插入丟例外後先在同一份 Registry 再插一次；突變清單 `docs/architecture/target/v2/scripts/orchestration_mutations.json`（40 個，只給行為測試；7.4 的掃描守衛延後） |
| 段 1 沒殺掉的 7 個 | d57356b2 | 測試加標記、先檢查非空、看規則專屬的訊息；40/40 |
| 段 2 Kernel 減法 | 81e1d231 | 拿掉開局排程（`startup`、`RunStartup`、J-58 等）與登記單位（`Plugin`、`FinishContext`、`Registry` 的 vector 與 `Finish()`、R-E1／R-E3／R-E6）；測試輔助改兩步（`RegisterAll`）；指紋 Registry 4、Plan 6、C 8，匯出 `gyo.kernel.plans 4`；golden 更新 |
| G-8 找到的缺陷 D1、D3、D4 | 01ce5137 | 各先寫失敗的案（Red，`segment1/g8-defects-red.log`）再修（Green，`g8-defects-green.log`）：D1 替換中斷的 World 失敗時不寫回它（值留在暫存處）；D3 `extra` 丟例外時記一次警告再往外傳；D4 第一個 World 的位置是空的時遊戲初始化回 `WorldMissing`、不記為已跑。D5（`UnitSlot::isModule` 只寫不讀）拿掉 |
| G-8 的測試補強與突變 | 本提交 | 新 `CoverageTests.cpp`（約 20 案：O-E6 三種、只有 Kernel 錯誤、`privateDepends`、Plugin 依賴 Plugin、產品的 build 最後、每幀清單的插入／錨點／錯誤、`RunSchedule` 的中斷與空位、`UnknownWorld`、預設 runner 的失敗等）；W1～W13 的處理（標記、非空檢查、四幀、比對碼與警告訊息）；突變清單加 52 個 G-8 候補與 D1／D3／D4 各一個，共 95；Kernel 與編排層的 DI 文字草稿（`target/kernel.zh-Hant.md`、新頁 `target/v2/orchestration.zh-Hant.md`；最終文字在第二段核准） |

## 驗證與經過

- 基準（0f1f2d21 的程式；第一次跑時我的新標頭已在，`kernel.naming_guard` 因它在 `GYO::Kernel` 做前向宣告而失敗——改成 include 後通過）。
- 段 1 之後：`ctest --preset test -L "cpu|shader"` 110 項全過（原 98 項＋編排層 12 組）；編排層 doctest 37 案、273 個斷言全過（macOS）。
- Red（G-2）：`Product.cpp` 換成空實作時 6 案失敗、`World()` 的斷言讓行程中止（`segment1/red-stub.log`）；換回後全過（`segment1/green.log`）。
- 實作中照第 8.4 節的「不會用」改了兩處測試（機制不動）：合成遊戲的 `SpawnRock` 與 `SpawnBadId` 都生成帶 `Pos` 的實體，被 D-E2 算成兩個寫者（DI-KS 6）——`SpawnBadId` 改用 `Junk`；`SaveData` 含 `std::vector`，World 摘要雜湊 Resource 的位元組會帶堆積位址——改成固定大小的陣列。
- 實作細節（第二段核准時看）：帶過去的值要從**已中斷**的新 World 取回時（讀檔的 schedule 失敗），Kernel 只准讀（E.5），所以經讀取的檢視把值移出、不寫回那個 World（它隨即銷毀）；`RunGameInit`／`RunFrame`／`RunSchedule` 回傳 `OrchestrationError`（宣告草圖寫 `KernelError`），Kernel 的錯誤放在 `OrchestrationError::kernel`。
- **段 1 的突變：40 個殺 33**（`segment1/mutants-segment1.log`）。沒殺掉的 7 個還沒處理（停在這裡）：
  - 活下來 1：`or-oe3-module-on-plugin`。推測：`RegisteredUnits` 裡 Module 一定排在 Plugin 前，拿掉「Module 依賴 Plugin」的檢查後，同一處仍以「依賴在後」報 O-E3；測試只看有沒有 O-E3。要改測試看訊息。
  - 預期之外的失敗 6：`or-oe2-file-and-library`、`or-oe5-anchor-silent`、`or-oe5-gameinit-unchecked`、`or-oe7-silent`、`or-carry-not-given-back`、`or-owner-not-checked`。推測：失敗出現在沒有標記的 `REQUIRE_FALSE`，或空指標當掉（帶過去的值沒移回時 `Resource<KeyBindings>()` 是空的），在專屬標記印出之前就結束。這一輪的日誌沒有存每個突變的輸出，原因待下一次用 `--only` 重跑確認。
- 7 個重跑並存下輸出（`segment1/unkilled/`）：6 個是測試寫法——失敗出在沒有標記的 `REQUIRE_FALSE`（5 個），或帶過去的值沒移回時解參照空指標當掉（1 個）；活下來的 1 個是測試只看 O-E3、分不出是哪一條。照第 8.4 節的「不會用」改測試（加標記、先檢查非空、看規則專屬的訊息），機制不動。重跑 7/7 在預期的失敗下殺掉（`segment1/unkilled-7-after-fix.log`）。**段 1 的突變 40/40。**
- **段 2**：`ctest --preset test -L "cpu|shader"` 全過（`segment2/ctest-segment2.log`）；G-3 的 Kernel 掃描 19 個式子都 0 命中，每個式子先在 0f1f2d21 用已知例命中一次（`segment2/g3_scan.py`、`g3_scan.log`）。
- **G-8（Fable 5.2／high，唯讀，讀 d57356b2）**：找到突變候補 56 個（預期活下來 39）、測試弱點 13 處、疑似缺陷 5 處（報告 `segment1/g8-check-fable-high.md`）。處理：
  - 候補 56 個加進清單 52 個；4 個不加，理由寫在清單各自的 `what`（與既有突變等價，或只會崩潰、沒有可認的失敗）。
  - 弱點 W1～W13 全部改測試（機制不動）。
  - 缺陷 D1、D3、D4 照 §14.3 修（01ce5137）；D5 拿掉。
  - **D2 不改，第二段核准時請使用者確認讀法**：`RunFrame` 碰到空位或中斷的 World 就停下回傳，排在後面的 World 那一次不跑。宣告 3.4 寫「有錯就停下回傳」，7.3 寫「`RunFrame` 回 `WorldMissing` 且別的 World 照跑」；我讀成「別的 World 之後照樣能用 `RunSchedule` 跑、不被拖垮」，案 `O-8: RunSchedule on an interrupted or empty World; another World still runs…`（`CoverageTests.cpp`）守這個讀法。
- **完整突變 95 個：94 殺、1 活**（`segment1/mutants-after-g8.log`）。活的 `g8-anchor-last-occurrence`：重複錨點只測了 `InsertAfter`，補 `InsertBefore` 的一段後殺掉（`mutants-after-g8-anchor-fix.log`）。**95/95。**

## 下一步

1. 段 3（登記檔）：兩份 CSV、`build/cmake/GyoUnitRegistry.cmake`、`engine/` 的子目錄與 link、`gyo_product_units` 與它的測試 target、`cmake -P` 測試、G-3 的 link 比對。
2. 段 4（scenario 第一段）。
3. 文件（說明頁、v2 README 的索引）、完整突變、G-7、段 2～4 的 G-8；第二段核准的資料。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-orchestration-skeleton/`（git 不追蹤）：`inventory-startup-plugin.md`、`declaration-r0-adversarial-check.md`、`scenario-cross-engine.md`；`segment1/`：`baseline-build.log`、`baseline-ctest.log`、`ctest-segment1.log`、`red-stub.log`、`green.log`、`mutants-segment1.log`、`unkilled/`、`unkilled-7-after-fix.log`、`g8-check-fable-high.md`、`g8-defects-red.log`、`g8-defects-green.log`、`mutants-after-g8.log`、`mutants-after-g8-anchor-fix.log`；`segment2/`：`ctest-segment2.log`、`g3_scan.py`、`g3_scan.log`。

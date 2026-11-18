# 編排層的骨架（`v8-arch-gyo2-orchestration-skeleton`）

日期：2026-11-18～。分支 `claude/gyo2-orchestration-skeleton-67744c`（從 `gyo2` 0f1f2d21）。**狀態：進行中——段 1 完成（突變 40/40）；使用者 2026-11-18 說「繼續」，進行段 2。**

這一批由任務卡做（GYO2 主會話開出）。事前宣告：[orchestration-skeleton-declaration.zh-Hant.md](../architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md) 修訂 7；使用者 2026-11-18「核准」第一段（主會話轉達）。修訂 0 送 Fable／high 敵對式檢查（必改 10、建議改 16、小問題 8，報告 `declaration-r0-adversarial-check.md`）。實作者 Opus／high。

## 做了什麼（到目前為止）

| 段 | 提交 | 內容 |
|---|---|---|
| 段 1 編排層本體 | bb9a5811 | `engine/orchestration`：`Module`／`Plugin`、`Product`（World 群、每個 World 一份 Registry、build → BuildPlans → Finish → Check、`ProductStartupReport` 收齊 O-E1～O-E7）、遊戲初始化與每幀清單、只跑一次的 runner、準備時機、`ReplaceWorld`（重新登記、BuildNewFirst／DropOldFirst、`beforeSwap`、`CarryOver<T>`）、`InsertSystems`（拒絕或建 Plan 失敗記一次 `Engine::Base::Log` 警告）。行為測試 `tests/common/orchestration/`（判定 2 的讀檔案例在 `RestoreFlowTests.cpp`） |
| 段 1 的測試補強與突變清單 | 本提交 | 失敗訊息加專屬標記、加「遊戲初始化只在第一個 World」一案、插入丟例外後先在同一份 Registry 再插一次；突變清單 `docs/architecture/target/v2/scripts/orchestration_mutations.json`（40 個，只給行為測試；7.4 的掃描守衛延後） |

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
- G-8（Fable／high）：段 1 的行為測試與突變清單送檢查（背景）。

## 下一步

1. 用 `--only` 重跑 7 個沒殺掉的突變、看輸出，改測試（標記、斷言訊息、避免空指標），全部在預期的失敗下殺掉。
2. G-8（Fable／high）檢查段 1 的行為測試與突變清單。
3. 使用者說「繼續」之後：段 2（Kernel 減法）、段 3（登記檔）、段 4（scenario 第一段）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-orchestration-skeleton/`（git 不追蹤）：`inventory-startup-plugin.md`、`declaration-r0-adversarial-check.md`、`scenario-cross-engine.md`；`segment1/`：`baseline-build.log`、`baseline-ctest.log`、`ctest-segment1.log`、`red-stub.log`、`green.log`、`mutants-segment1.log`。

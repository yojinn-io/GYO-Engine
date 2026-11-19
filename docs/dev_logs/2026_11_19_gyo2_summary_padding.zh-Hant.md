# World 摘要的 padding：先補 Kernel 的配發器，正式解法在第 4 批（`v8-arch-gyo2-summary-padding-a-then-c`）

日期：2026-11-19。分支 `claude/gyo2-summary-padding`（從 `origin/gyo2` `eedc9a00`）。**狀態：未完全解決。** 這一批只做方案 A；正式解法是 Summary 改成逐型別雜湊（方案 C），排在第 4 批。

這一批由第 2 批的任務卡做（GYO2 主會話轉來）。實作者 Opus 5.5／high。

## 起因

- gyo2 合併後的 CI run 44464026083（`eedc9a00`）失敗在「Quick acceptance / toolchain / windows-x64」的「Build and test core with optional modules disabled」：`orchestration.restore` 的 case 5（同一份存檔讀兩次，World 摘要相同）不成立。
- 同一個 run 裡，全模組那次建置的同一個測試通過；前一次 run 44349806896（`f48af791`）同一列也通過。所以是不穩定的失敗。

## 原因（AGENTS §14.3）

- case 5 會被摘要雜湊的型別裡，只有 Kernel 附的 `RangeAllotter`（`engine/kernel/include/engine/kernel/detail/Storage.hpp`）有 padding：三個 `EntityId` 加一個 `bool`，sizeof 32，尾端 7 個 byte。測試自己的型別都沒有 padding。SaveData 已在第 2 批的 `60f64cc7` 補過。
- `World::Summary` 照原始位元組雜湊（`World.cpp` 的格式註解早就寫了「padding 照原樣雜湊」）。padding 沒有規定的內容，case 5 兩次讀檔都用 `RangeAllotter::Of` 做出新的值，MSVC 會把當時堆疊上的殘值帶進 World。這一段是推論，沒有在 Windows 上重現。
- 已有的記錄：`time-types-declaration` 第 9 點；authority 與 kernel-app-boundary 宣告的 digest 已定成「每個型別有雜湊函式，不直接雜湊原始位元組」。缺的是 Kernel 自己附的型別就違反了這個前提，而且沒有守衛。
- 本機重現（scratch，用真的 `libgyo_kernel.a`）：兩個 World 各放一個 `RangeAllotter`，四個成員相同、padding 一個是 00、一個是 ff；存進去的 byte 25..31 照原樣保留，兩個摘要不同。

## 決定

使用者 2026-11-19（主會話轉 side chat 的決定）原話：「先選A，但是把方案C記下來。這個case表為未完全解決，解決方案用C」。

| 選項 | 內容 | 結果 |
|---|---|---|
| A | Kernel 的 `RangeAllotter` 補一個明寫為 0 的欄位、加編譯期檢查；摘要契約補「進摘要的型別不能有 padding（作者的責任）」 | **這一批做** |
| B | A 再加登記時的檢查 | 不選（float 與不能平凡複製的型別也會被擋，太硬） |
| C | Summary 改逐型別雜湊，和 digest 的方向一致 | **正式解法，排在第 4 批** |
| D | 只改測試，避開這個型別 | 不選 |
| E | 把前提整個交給後面重定，case 5 標成已知不穩定 | 不選 |

## 做了什麼

- `RangeAllotter` 在 `exhausted` 後面加 `std::array<std::uint8_t, 7> reserved{}`；`Of` 明寫 `{}`；結構後面加 `static_assert(std::has_unique_object_representations_v<RangeAllotter>, …)`。成員的意思、`Allot`、`Owns` 都不變。
- 摘要契約：`World.hpp` 的 `Summary` 註解、`World.cpp` 的格式註解，各補一句。這些註解的新文字使用者看過後才提交，原話「可以，限制頁那條也加」。
- 限制頁加 LIM-8（`docs/architecture/target/limits.zh-Hant.md`）：Summary 雜湊原始位元組，作者的型別不能有 padding；第 4 批改成逐型別雜湊時解除。使用者同意加這一條；LIM 的實際文字提交後再給使用者看。
- 測試：
  - 新增 `WorldTests.cpp`「D-9: the kernel's RangeAllotter has no padding, so equal allotters give one World summary」：檢查型別沒有 padding（`[summary-padding-unique]`），並在先填 00 或 ff 的記憶體上用 `Of` 做出兩個配發器，各放進一個 World，比對摘要（`[summary-padding-equal]`）。
  - 會比對摘要的測試加註解「未完全解決，正式解法是逐型別雜湊，第 4 批」：restore case 5；J-6、J-44（`ExecutorTests.cpp`）；DI-KS 26（`RegistrationRoundTests.cpp`）；J-32（`WorldTests.cpp`）；J-55 與兩個回傳摘要的輔助函式（`SpawnRejectTests.cpp`）。
- 突變清單：
  - Kernel 清單加 2 個：`spa-reserved-short` 讓保留欄位少一個 byte，期待編譯期檢查讓建置失敗；`spa-reserved-short-unchecked` 少一個 byte、同時拿掉編譯期檢查，期待 `[summary-padding-unique]`。
  - 編排層清單的 `sc-lower-bound-not-moved`，find 與 replace 跟著 `Of` 的 `{}` 改。

## 驗證

- Red：修之前，新測試的兩個 CHECK 都失敗（macOS arm64，clang -O2）。兩個摘要是 `3959cdb8…` 與 `0eef79dc…`。
- Green：修之後，新測試 4/4 通過。
- ctest：`build/target/_build/test` 全量 113 項全過（macOS arm64）。
- 突變（D184：新增的 2 個，加上 find 在 `Storage.hpp` 的 5 個，Kernel 清單 4 個、編排層清單 1 個）：**7/7 殺掉**。Kernel 6/6：`we-j1-erase-swap-pop`、`we-j8-allotter-wraps`、`we-j8-allotter-reset`、`sr-range-absent-checked`、`spa-reserved-short`、`spa-reserved-short-unchecked`。編排層 1/1：`sc-lower-bound-not-moved`。第一次跑 Kernel 清單時沒給 `--catalog`，執行器找不到預設清單、沒跑任何突變就結束；加上 `--catalog` 重跑。
- Windows 上沒有實機重現；修正是否生效看 toolchain windows-x64 那一列之後的結果。

## 證據

主 checkout `build/evidence/gyo2-orchestration-skeleton/restore-case5-padding/`（這台 Mac）：
- `repro.cpp`、`SaveGame_types.hpp`、`repro-output.txt`：修之前的 scratch 重現。
- `ci-44464026083-excerpt.txt`：CI 失敗的節錄。
- `ctest-after-fix.txt`：修之後的全量 ctest。
- `mutations-kernel-after-fix.txt`：Kernel 清單 6 個。
- `mutations-after-fix.txt`：編排層清單 1 個，以及第一次少給 `--catalog` 的那段輸出。

## 下一步

- 第 4 批：Summary 改成逐型別雜湊（方案 C）。到時拿掉 `reserved` 是否合適、測試註解裡的「未完全解決」，在那一批一起看。
- 在第 4 批之前，作者自己登記的型別仍然沒有守衛（這是 A 刻意接受的範圍，記在 LIM-8）。

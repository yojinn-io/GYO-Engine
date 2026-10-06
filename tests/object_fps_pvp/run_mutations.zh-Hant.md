# 突變檢查工具說明書

Owner：`object_fps_pvp`。工具：[`run_mutations.py`](run_mutations.py)；清單：[`mutations.json`](mutations.json)；工具自己的測試：[`test_run_mutations.py`](test_run_mutations.py)（CTest `object_fps_pvp.mutation_tool`）。
本文件的寫法借用 Agent skill 的概念：先說何時用，再給固定步驟、規則與判讀。它只是說明書，不是 skill。

## 用途

證明新寫的測試真的會抓到它要守住的錯誤：把程式故意改壞一處（突變），確認對應的檢查會以**預期的訊息**失敗。

## 何時使用

- 一批新增或修改了驗證、拒絕路徑、權威規則、wire 轉換等「改錯不會報錯」的邏輯，並為它寫了測試時。
- 批次的驗收點要求突變檢查時（每批的 dev_log 記錄結果）。
- 重構既有測試後，確認舊突變仍被抓到。

## 何時不用

- 只改文件、註解、命名，或沒有新測試的變更。
- 想找新的錯誤：突變檢查只驗證「測試會失敗」，不尋找缺陷。
- 量測與效能比較：那是 L2 的事前宣告流程。

## 前提

- 已用 test preset 建置過（例如 `build/target/_build/test`）。
- 清單涉及的檔案與 HEAD 相同（先 commit）。工具會拒絕在未提交的檔案上執行，確保中途中斷時殘留的突變一定看得出來。
- 需要的指令：Python 3、cmake、ctest、git；Go 檢查需要 go。路徑不在 PATH 時用參數指定。

## 步驟

1. **寫測試，先確認它在正常程式碼上通過。**
2. **在 `mutations.json` 加入突變**（欄位見下表）。一個突變只改一處，說明「被改壞的是什麼」。
3. **先跑一個新突變：**
   ```bash
   python3 tests/object_fps_pvp/run_mutations.py --build-dir build/target/_build/test --only 09-hit-count
   ```
4. **跑整批：** `--batch 09`；全部：不加篩選。需要保存結果時加 `--report FILE`。
5. **判讀結果**（下表），把「N／N killed」與例外寫進該批的 dev_log。

各平台的寫法相同；指令不在 PATH 時：

```bash
python3 tests/object_fps_pvp/run_mutations.py --build-dir build/target/_build/test --cmake /path/to/cmake --ctest /path/to/ctest --go /path/to/go
```

Windows 用 `py` 或 `python` 執行同一個腳本即可，不需要其他 shell 指令。

## 清單欄位（`mutations.json`）

| 欄位 | 說明 |
|---|---|
| `id` | 唯一名稱，以批次開頭，例如 `09-arena-order` |
| `batch` | 批次編號（`--batch` 用） |
| `what` | 被改壞的是什麼（一句） |
| `file` | 相對 repo 根目錄的檔案 |
| `find` | 要替換的文字，必須在檔案中**剛好出現一次** |
| `replace` | 替換後的文字（刪除就寫空字串） |
| `check` | 使用 `checks` 中的哪個檢查 |
| `expect` | 預期失敗訊息的正規表示式（例如 doctest 的 TEST CASE 名稱、`Require` 的訊息、Go 的 `t.Fatalf` 訊息） |

`checks` 定義檢查本身：`build`（先建置的 target）加 `ctest`（測試名稱），或 `command`（`{go}`、`{python}` 會換成實際路徑）加 `cwd`。可選 `timeout`（秒）。

## 規則

- **只有「檢查失敗，而且輸出符合 `expect`」才算 killed。** 只看 exit code 不算數。
- 開始前，未突變的程式碼必須通過每個用到的檢查，否則整次中止（exit 2）。
- 每個突變跑完都還原原始碼；結束時重新建置用到的 target，二進位回到未突變狀態。
- `expect` 要寫到能分辨「這個突變被這個斷言抓到」。只寫「逾時」「失敗」這類泛用訊息不行：別的原因也會產生它。
- 只用跨平台的方式：Python、`subprocess` 的逾時、ctest。不用 `timeout`、`gtimeout`、GNU 專屬旗標等只存在某個 OS 的指令。

## 結果判讀

| 結果 | 意思 | 處理 |
|---|---|---|
| `killed` | 檢查以預期訊息失敗 | 通過 |
| `survived` | 突變後檢查仍通過 | 測試有缺口：補測試，或說明這是等價突變（寫進 dev_log） |
| `wrong_failure` | 檢查失敗了，但不是預期的訊息 | 查清原因；可能是別的斷言先失敗，或 `expect` 寫錯 |
| `stale` | `find` 找不到或不只一處 | 程式已改動：更新清單 |
| `build_failed` | 突變後無法建置 | 換一個能編譯的突變（編譯錯誤不證明測試有效） |
| `timeout` | 檢查超過時限 | 讓測試在該情況下以明確訊息失敗，而不是等到逾時 |
| `error` | 指令無法執行（例如找不到指令） | 修正環境或參數；**絕不算 killed** |

exit status：全部 killed 為 0；任何其他結果為 1；參數錯誤、檔案未提交或基準失敗為 2。

## 常見錯誤

- 用 shell 的 `timeout` 包住檢查：macOS 沒有這個指令，「找不到指令」會被誤當成突變被抓到（第 09 批發生過）。本工具把它分類為 `error`。
- 測試只會在逾時時失敗：突變被「抓到」的原因不明確。讓測試在錯誤被接受時立刻以具體訊息失敗。
- 突變改在測試本身或清單外的檔案：工具只還原清單中的檔案。
- 在未提交的檔案上加 `--allow-dirty` 執行後又中斷：先用 `git diff` 確認沒有殘留的突變。

## 清單維護

- CTest `object_fps_pvp.mutation_tool` 會核對每個 `find` 在 repo 中剛好出現一次，所以程式改動讓清單過期時，CI 會失敗。更新 `find`，或在功能已刪除時移除該突變。
- 本工具與清單屬於 `object_fps_pvp`：刪除本產品時一併刪除，共通層不引用它們。

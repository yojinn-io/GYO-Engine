# 架構漂移健檢

Owner：Engine（共通架構），由架構會話負責。本資料夾只放架構漂移的定期健檢；文件健檢不在此處。

每份報告以日期命名，只記錄當時 commit 的觀察，不回頭改寫；後續狀態寫進下一份報告。
報告的核心是兩節：**應收進 Engine 的功能**、**Engine 這一側的問題**。流程或專案內部的觀察放在附錄。

健檢只產生觀察與建議，不直接觸發重構。是否立案、何時處理由使用者決定（AGENTS.md §11、§13）。

## 何時做

從上一份報告的基準 commit 算起，符合下列任一條件就該做下一次健檢：

- 滿 14 天；
- master 合併了 20 個 PR（`git log --first-parent <base>..origin/master` 的筆數）；
- 有產品版本結案（例如 vN、vN.M 升格為穩定版本）；
- 新增 Engine 模組或最上層目錄。

架構會話每次被喚起時先檢查這些條件，達到就提醒使用者；是否開跑由使用者決定。

## 判斷標準

`apps/` 底下每個遊戲都是**獨立專案**，可以想成多個專案同時開發。
專案之間有相同的程式碼是正常的，本身不是問題。架構健檢主要問兩件事：

1. **Engine 是否被入侵**：產品知識（產品名稱、玩法常數、類型專用概念）有沒有進到 `engine/` 或共通層；
   依賴方向是否維持 `Application → Engine`、`Editor → Engine`。
2. **該由 Engine 負責的功能是否落在別處**：某個功能要不要收進 Engine（或 framework）。以下條件中，第 2 點是必要條件：
   1. 與遊戲類型無關；
   2. **Engine 有缺口**：專案必須繞過 Engine 的抽象（例如直接使用 SDL），或必須修改 Engine 才能取得；
   3. 多個獨立專案需要（重複只用來佐證需求）；
   4. 契約本來就歸 Engine 所有，但 Engine 只實作了一部分。

另外也記錄 Engine 對獨立消費端的影響，例如沒有遷移路徑的公開 API 破壞性變更。

Engine 內部也要看兩件事，結果寫進報告的「Engine 這一側的問題」：

- **OS 差異由誰處理**：同一種系統差異（檔案、等待與計時、執行緒、程序、socket 錯誤）是否由多個模組各自處理。
  若 Engine 已指定負責 OS 差異的模組，該模組以外的 OS 程式碼都是問題。
- **只有介面的抽象**：Engine 的抽象沒有實作，或除了測試沒有消費端。這是上面第 2 條第 4 點最常見的樣子；
  產品或工具繞過它自己做同樣的事時，兩者要一起記錄。

## 手動步驟

1. 以上一份報告的 commit 為基準，列出變更：`git log --first-parent <base>..HEAD`、`git diff --dirstat <base>..HEAD`。
2. 逐項確認上一份報告的未結項目：已解決、仍在，或已改變。
3. Engine 入侵：在 `engine/`（不含 `engine/config`）搜尋產品名稱與玩法用詞；確認 engine 的 CMake 只連結 `GYO::*` 與第三方 target；
   在 `build/cmake`、`build/*.py`、`build/acceptance/common`、`build/ci/common`、`tests/common`、`.github/workflows` 與根目錄 CMake 搜尋產品名稱分支。
4. 收進 Engine 的候選：在 `apps/`、`tools/` 搜尋直接的 SDL 呼叫與自行實作的基礎設施，再依上面的條件判斷。
5. OS 相依：在 `engine/`、`apps/`、`tools/` 搜尋 OS 巨集、OS 標頭與直接的 OS API，例如：
   ```bash
   git grep -n -E '#\s*if.*(_WIN32|__APPLE__|__linux__|__unix__|TARGET_OS_)|#\s*include\s*<(windows\.h|winsock2?\.h|unistd\.h|pthread\.h|sys/[a-z_]+\.h|mach/[a-z_]+\.h|signal\.h|dlfcn\.h|fcntl\.h)>' -- engine apps tools ':!*third_party*'
   ```
   巨集與標頭之外，也要看不靠 `#if` 的直接呼叫（例如 `std::signal`、平台專屬的 socket 錯誤碼）。
   產品與工具的命中依判斷標準第 2 條判斷；Engine 的命中依上面「OS 差異由誰處理」記錄。
6. 只有介面的抽象：在 `engine/*/include` 找出介面（純虛擬函式、`I` 開頭的類別）與只有宣告的契約，
   用 `git grep` 確認各有實作，並且在 Engine 自己的模組與測試以外有消費端；同時看產品或工具是否繞過它
   （例如不用 Engine 的檔案抽象，直接用 `std::fstream` 開檔）。
7. 執行期的證據：讀上一份報告以來各計畫 HANDOFF 的未結事項、事故與臨時測試紀錄，找出指向 Engine 缺口或平台差異的項目。
   這類問題靜態搜尋看不到，例如只在 Windows 的深資料夾才出現的路徑長度限制。
8. 在本資料夾新增 `YYYY_MM_DD_architecture.zh-Hant.md`，並更新下方的報告列表。

## 報告列表

| 日期 | 基準 commit | 報告 |
|---|---|---|
| 2026-10-04 | `c8ee213` | [架構漂移健檢](2026_10_04_architecture.zh-Hant.md) |
| 2026-10-27 | `37d533d` | [架構漂移健檢](2026_10_27_architecture.zh-Hant.md) |

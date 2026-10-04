# 例行健檢（Checkup）

Owner：Engine（共通架構）。本目錄記錄定期的「架構漂移」與「文件」健檢結果。
每份報告以日期命名，只記錄當時 commit 的觀察，不回頭改寫；後續狀態寫進下一份報告。

每份架構報告的核心是兩節：**應收進 Engine 的功能**、**Engine 這一側的問題**。流程或專案內部的觀察放在附錄。

健檢只產生觀察與建議，不直接觸發重構。是否立案、何時處理由使用者決定（AGENTS.md §11、§13）。

## 判斷標準

`apps/` 底下每個遊戲都是**獨立專案**，可以想成多個專案同時開發。
專案之間有相同的程式碼是正常的，本身不是問題。架構健檢只問兩件事：

1. **Engine 是否被入侵**：產品知識（產品名稱、玩法常數、類型專用概念）有沒有進到 `engine/` 或共通層；
   依賴方向是否維持 `Application → Engine`、`Editor → Engine`。
2. **該由 Engine 負責的功能是否落在別處**：某個功能要不要收進 Engine（或 framework）。以下條件中，第 2 點是必要條件：
   1. 與遊戲類型無關；
   2. **Engine 有缺口**：專案必須繞過 Engine 的抽象（例如直接使用 SDL），或必須修改 Engine 才能取得；
   3. 多個獨立專案需要（重複只用來佐證需求）；
   4. 契約本來就歸 Engine 所有，但 Engine 只實作了一部分。

另外也記錄 Engine 對獨立消費端的影響，例如沒有遷移路徑的公開 API 破壞性變更。

文件健檢檢查：斷鏈與錨點、與程式碼不符的過時內容、計畫／HANDOFF 的狀態與 git 紀錄是否一致、
各語言版本是否一致、繁中檔案的用字（不混簡體、避免大陸用語）。

## 手動步驟

1. 以上一份報告的 commit 為基準，列出變更：`git log --first-parent <base>..HEAD`、`git diff --dirstat <base>..HEAD`。
2. 逐項確認上一份報告的未結項目：已解決、仍在，或已改變。
3. Engine 入侵：在 `engine/`（不含 `engine/config`）搜尋產品名稱與玩法用詞；確認 engine 的 CMake 只連結 `GYO::*` 與第三方 target；
   在 `build/cmake`、`build/*.py`、`build/acceptance/common`、`build/ci/common`、`tests/common`、`.github/workflows` 與根目錄 CMake 搜尋產品名稱分支。
4. 收進 Engine 的候選：在 `apps/`、`tools/` 搜尋直接的 SDL 呼叫與自行實作的基礎設施，再依上面的條件判斷。
5. 文件：掃描 markdown 連結、錨點與反引號中的 repo 路徑；比對文件提到的 CMake 名稱、API、PR 編號與目前的程式碼和 `git log --merges`。
6. 在本目錄新增 `YYYY_MM_DD_<主題>.zh-Hant.md`，並更新下方的報告列表。

## 報告列表

| 日期 | 基準 commit | 報告 |
|---|---|---|
| 2026-10-04 | `c8ee213` | [架構漂移健檢](2026_10_04_architecture.zh-Hant.md) |

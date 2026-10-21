# 文件健檢

Owner：文件整理。本資料夾只放文件健檢；架構漂移健檢由另一個會話負責，不在此處。

文件健檢找出「文件說的和目前狀態不一樣」的地方，整理成報告，再依使用者核准的範圍修正。不判斷架構問題。

## 檢查什麼

1. **與程式碼不符**：CMake target、選項、preset、函式與 API 名稱、檔案路徑、登錄 CSV、workflow 與腳本名稱、協議版本與欄位。
2. **狀態不符**：計畫 README 的進度表、HANDOFF 開頭、批次文件開頭、穩定基線與 dev_log 的狀態行，和 `git log --merges` 的 PR 編號、合併 commit 是否一致；
   舊版本的入口頁是否指向新的穩定基線。
3. **斷鏈**：markdown 連結、圖片、錨點（GitHub slug 規則，全形標點會被去掉）、反引號中的 repo 路徑。
4. **各語言版本**：README 三語、`releasing`、`rendering_architecture`、`object_fps/acceptance` 的結構與反引號內容是否一致。
5. **用字**：繁中檔案不混簡體字與日文字形，避免大陸用語（生命週期、資料、介面、滑鼠、預設、執行緒、記憶體、快取、伺服器、網路等）。

不算問題的：dev_log 與帶日期的快照是歷史紀錄，本文不改寫，只在狀態行補「後續」；明確標示為歷史的舊路徑；
`build/target/` 與本機日誌等不納入版控的證據路徑；範例佔位名稱（`my_game` 等）。

## 手動步驟

1. 以上一份報告的 commit 為基準，列出變更：`git log --first-parent <base>..HEAD`、`git diff --stat <base>..HEAD -- '*.md'`。
   內容沒變、但描述的程式碼變了的文件，最容易過時。
2. 機械檢查：連結與錨點、反引號路徑、cp950 無法編碼的字與假名、大陸用語、各語言版本的結構比對。
3. 依主題人工核對：Engine 與共通文件（README、`docs/architecture*`、建置與發佈指南），以及各產品的計畫文件（進度表、HANDOFF、批次文件開頭）。
4. 在本資料夾新增 `YYYY_MM_DD_documentation.zh-Hant.md`，並更新下方的報告列表。修正以另一個變更進行，報告本身不回頭改寫。

## 報告列表

| 日期 | 基準 commit | 報告 |
|---|---|---|
| 2026-10-15 | `0f0bc91` | [文件健檢](2026_10_15_documentation.zh-Hant.md) |
| 2026-10-21 | `5ee499a` | [文件健檢](2026_10_21_documentation.zh-Hant.md) |

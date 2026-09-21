# GYO：從 GitHub 準備與發佈版本

[日本語](releasing.ja.md) · [整體架構](architecture.md) · [渲染驗收](rendering_architecture.zh-Hant.md#r09)

## 1. 日常開發與發佈入口

**在 GitHub 的 Actions 執行 `Prepare Release`，等完整驗收與附件準備完成後，再按 `Publish release`。** 不需要先建立 tag 或 Release。

| 操作 | 執行內容 | 遠端結果 |
|---|---|---|
| 分支 push／pull request | Quick：三平台引擎／Editor baseline，加上 CSV 選中 app 的獨立建置與驗收 | Actions artifacts |
| 手動執行一般 cross-platform workflow | 相同 quick 驗收 | Actions artifacts |
| 手動執行 **Prepare Release** | Baseline 與選中 app × 平台的完整驗收 | 成功後建立版本 tag、Draft Release 與計算出的附件集合 |
| 推送 tag | 不觸發上述建置 | 不自動建立 Release |
| **Publish release** | 公開已準備好的 Draft | 不重新建置 |

兩個入口共用 `build-and-validate.yml`；quick 與 release 的差別是驗收範圍。Tag 固定原始碼 commit，Release 提供版本說明與下載附件。

`config/engine/projects.csv` 是本機與 CI 的共同來源。每列只有 `enabled` 與目標平台欄位都啟用才進入 app 矩陣；`description`、`version` 是個人備註，不決定這裡輸入的 Release 版本。沒有 app 的平台仍測試引擎與 Editor、不產包；全部停用時一般 CI 仍可成功，**Prepare Release** 會在前置檢查拒絕空產物集合。

## 2. 第一次使用前

先將 `prepare-release.yml`、`build-and-validate.yml` 及相關程式合併到儲存庫的預設分支，GitHub 才會顯示 **Run workflow**。選擇的來源分支也必須包含這套 workflow；一般發佈可選預設分支，例如 `master`。

來源分支若還有新的 workflow 定義變更，先將這些變更合併至預設分支，再準備版本，避免 GitHub 對建立含未知 workflow commit 的 tag／Release 所施加的權限限制。

使用具有執行該儲存庫 Actions 與編輯 Release 權限的帳號。建置 job 只有讀取權限，最後的 Draft job 才有 `contents: write`，且必須等全部驗收成功後才能執行。

## 3. 在 GitHub 完成一次發佈

1. 將要發佈的程式推送到來源分支，確認日常 quick CI 結果。
2. 開啟儲存庫的 **Actions → Prepare Release → Run workflow**。
3. 在分支選單選擇來源分支。這次執行會固定當下的完整 commit SHA；後續分支的新 commit 不會混入本次套件。
4. 填寫 **version**，例如 `v1.0.1`；候選版可用 `v1.1.0-rc.1`。需要預發行標記時勾選 **prerelease**，再執行 **Run workflow**。
5. 等三平台 baseline 與全部選中 app × 平台的完整驗收通過。過程中先產生 Actions artifacts；所有必要檢查成功後，最後一個 job 才建立 tag、Draft Release 並上傳附件。
6. 開啟執行結果的 **Summary**，點選 Draft Release 連結。
7. 檢查版本、來源 commit、CSV 所計算出的全部附件及預發行狀態；編輯版本標題與更新說明。
8. 確認後按 **Publish release**。這一步公開既有附件，不會啟動另一輪完整建置。

## 4. 完整驗收與附件

Release 是外部專案／CI 管理操作。普通產品預設 `BUILD_TESTING=OFF`、`GYO_ENABLE_PACKAGING=OFF`；測試、CI、驗收檔案不存在時仍須能 build／run／install。CI 明確開啟所需軸，由產品外部載入 `tests/Tests.cmake` 與 `packaging/Package.cmake`。Package 契約或必要證據不足，只會讓該次明確要求的封裝／發行操作失敗，不使普通產品配置失效。

Release profile 固定測試三平台引擎與 UI editor，並為同一來源 commit 的 CSV 產生 app × 平台矩陣（Windows x64、Linux x64、macOS ARM64）。每個組合用隔離建置目錄，只選自己的 app、關閉 Editor，完成建置、測試、安裝、manifest 驗收與封裝。Editor 不放入 app 包，無 GPU／shader 需求的 app 不執行對應處理。

每個 app 必須註冊安裝後 startup 測試；共用 runner 從包外工作目錄執行生成的 `share/gyo/apps/<name>/manifest.json` 所宣告的命令，按 profile／平台／GPU 條件驗收。失敗、逾時或缺少必要證據都阻擋產包。Object_FPS 自己維護 gameplay、缺檔、Linux quick 一項與 release 八項 GPU 規則，詳見[app 驗收指南](../apps/object_fps/docs/acceptance.zh-Hant.md)。

每個選中組合有一個壓縮檔與一個 checksum，**附件數是選中組合數乘以二**：

```text
gyo-<name>-<platform>.tar.gz
gyo-<name>-<platform>.tar.gz.sha256
```

Archive 只有一個 `gyo-<name>` 根目錄，包含該 app 與必要依賴。例如 CSV 的 `object_fps` 名稱保留底線，不改成 `object-fps`。Release 使用相同來源 SHA 的 CSV 重建預期集合，拒絕缺包、多包或重複身份；上傳前核對 checksum、封存檔安全、必要內容、app、平台、來源 SHA、release profile 與完整驗收證據。Quick 證據不能冒充完整 release。必要 job 失敗、取消或跳過，均不能進入 Draft 階段。

Windows／macOS hosted CI 不執行實體 GPU 驗收；Linux Lavapipe 是軟體 Vulkan。各 app 的手動實機結果另記錄，不由編譯成功推定。CSV、能力解析、manifest 與 CI 資料流詳見[設計文件](architecture.md#build-project-management)。

新增 app 依[手動建立／複製流程](creating_apps.md)，再啟用對應 CSV 列與目標平台。`gyo_app_project()` 與 target／content helpers 會使用新身份，因此不用在 Release workflow 另增 app 名稱，也不用全域改名內部 AssetId 或 shader ID。日常 baseline 在三平台驗證 helper 契約；Windows release baseline 另跑真實 app 複本的同時建置與分別部署回歸。測試複本僅存在 scratch tree，其 archive 不上傳為附件；發行預期集合始終來自原來源 commit 的 CSV。

## 5. 失敗、重跑與已存在的版本

Draft 準備遇到 HTTP 500／502／503／504，或網路逾時、連線重設、回應截斷時，會自動恢復，最多嘗試四次（含首次），預設等待 2／4／8 秒。伺服器提供 `Retry-After` 時按其指示等待；要求超過 60 秒則停止並回報，不提前重試。權限、資料驗證與版本衝突會立即失敗。

每次恢復都重新讀取 tag、Draft 與附件，核對來源及 checksum 後決定還缺少什麼，不直接重送上一次 POST。若 GitHub 留下未完成的 `starter` 附件，流程先在上述次數限制內等待並重查；仍未完成時回報需處理的附件，不自動刪除或覆寫。

| 狀況 | 處理方式 |
|---|---|
| 建置或測試失敗 | 修正後準備新執行；該失敗執行不會新建 tag／Draft |
| 暫時的下載或上傳失敗，仍要使用同一份原始碼 | 在原執行頁面按 **Re-run failed jobs**；原事件 SHA 保持不變 |
| 舊版 publish 執行遇到 504 | **Re-run failed jobs** 仍使用該次舊 workflow，不會套用本次恢復修正 |
| 重跑時舊 artifacts 已過期 | 對原執行使用 **Re-run all jobs**，以相同事件 SHA 重新建立 |
| Draft 已建立，但附件只上傳一部分 | 重跑原執行；工具驗證已存在附件並只補缺檔，保留手寫的標題與說明 |
| 同名 tag 已存在 | 僅允許它指向本次完全相同的 commit；不移動既有 tag |
| Draft 存在，但它的 tag 被刪除 | 流程拒絕猜測來源；還原已確認的原 tag，或使用新版本號 |
| 同版本已經公開 | 此流程拒絕修改，請使用新版本號 |
| 分支已經前進，又按新的 **Run workflow** | 新執行選擇新的 SHA；不能拿相同版本號取代舊 commit |

要使用本次修正，先將它合併，再從新 commit 執行 **Prepare Release**。若原版本 tag 已指向不同 SHA，請填新的版本號；不要移動或刪除原 tag 來重用版本。

相同版本不會同時執行，正在執行的工作不會被新請求取消；GitHub 不保證等待中請求的順序，後來的請求可能取代尚未開始的請求。既有附件若內容或來源驗證不符會報錯，不覆寫。`prerelease` 輸入只套用於新建 Draft；既有 Draft 的預發行標記、標題與說明都會保留，可在 Draft 頁面確認後編輯。

## 6. 範圍與驗證狀態

新流程不會改寫已經存在的 Actions 執行，也不會改變舊 commit 的 workflow。舊 tag 或舊執行的 **Re-run** 仍使用當時的定義，可能保留以前的 tag／`release.published` 觸發方式。

本輪實作與本機測試不等於已在 GitHub 建立或公開 Release。請以實際 **Prepare Release** 的 Summary、三平台結果及 Draft 附件作為該版本的驗收證據。

以下為重構前的歷史證據，不代表 CSV 矩陣已完成 hosted 驗證：先前流程已通過三個 workflows 的 actionlint 1.7.12 與雙語指南封裝檢查。當時的自動恢復修正後，`tools/ci` 全部 80 項測試通過，包含 16 種實際 Bash 驗收 gate 狀態組合，以及請求已寫入但回應逾時的恢復案例。GitHub API 使用 mock，沒有建立遠端 Release；這些結果不能代表遠端 API 已恢復或 hosted Draft 準備已成功。同一次 Actions 執行的重跑可替換其 artifacts；Release 附件仍只補缺檔、不覆寫。

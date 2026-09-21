# GYO：準備與發佈版本

[日本語](releasing.ja.md) · [整體架構](architecture.md) · [渲染驗收](rendering_architecture.zh-Hant.md#r09)

在 GitHub Actions 執行 **Prepare Release**，等所有必要產品通過驗收並準備好 Draft，再檢查附件與說明、按 **Publish release**。本流程以 Engine 整合為單位，不以某個遊戲是否存在決定能否發佈。

## 1. 產品集合

每個支援平台固定產生 toolchain 產品，包含靜態連結引擎的 GUI UI editor 及必要 runtime libraries。`engine/config/projects.csv` 再選擇額外遊戲；每列只有 enabled 與目標平台欄位同時啟用才參與。`description`、`version` 是備註，不決定 Release 版本。

| 產品 | 壓縮檔 | 封存根目錄 |
|---|---|---|
| 固定設計工具鏈 | `gyo-toolchain-<platform>.tar.gz` | `gyo-toolchain` |
| CSV 選中的遊戲 | `gyo-<game>-<platform>.tar.gz` | `gyo-<game>` |

平台是 `windows-x64`、`linux-x64`、`macos-arm64`。每個壓縮檔另有同名 `.sha256`。因此沒有 app、CSV 只有標頭或全部停用時，仍有三平台 toolchain 與 checksum，可完成 Release。未登錄的目錄不會自動加入；已選中的遊戲缺檔、編譯或驗收失敗，則整次整合失敗。

產品是可執行程式與必要依賴，不提供 Engine SDK 或 source archive。Toolchain 不帶遊戲 source/assets；遊戲包不带 CI、tests、acceptance executable、editor 或來源美術。引擎靜態連結進各產品，第三方動態 runtime 按需要部署。

## 2. 入口與操作

| 操作 | 結果 |
|---|---|
| 分支 push／pull request／一般 workflow 手動執行 | Quick 整合與 Actions artifacts |
| **Prepare Release** | 固定 SHA，完整驗收全部必要產品，準備 tag、Draft 與附件 |
| 推送 tag | 不以此作為自動發佈入口 |
| **Publish release** | 公開已準備的 Draft，不重新編譯 |

第一次使用前，先把 workflow 與所需程式放入預設分支，GitHub 才能顯示 **Run workflow**。來源分支也必須包含相同支援。需要 Actions 執行與 Release 編輯權限；建置 jobs 保持讀取權限，最後 Draft job 才使用寫入權限。

1. 推送預定來源並確認 Quick 結果。
2. 在 **Actions → Prepare Release → Run workflow** 選擇來源分支。
3. 填入 version，例如 `v1.0.1`，必要時勾選 prerelease。
4. 等待三平台固定 toolchain 與全部 CSV 遊戲的必要驗收。
5. 從 Summary 開啟 Draft，核對 commit、版本、全部附件及說明。
6. 準備公開時按 **Publish release**。

工作流程先固定完整 commit SHA，後續分支變更不會混入同一次產品。必要 job 失敗、取消或意外跳過都阻止 Draft；不能用部分平台成功作為全體成功。

## 3. 組裝與驗收

本機與 CI 共用產品組裝：`build/assemble_runtime.py` 依 `assets/<game>/content.json` 驗證並組裝準備好的資產，CMake generic hook 自動呼叫。離線 shader compiler 位於 `engine/render/shaders/pipeline`。普通遊戲只需要產品 build，不需要 CI/tests 或 package acceptance。

組裝器與套件驗證共用 `build/content_contract.py`；C++ runtime 與 Editor 以同一組 fixtures 驗證原生解析器。Catalog 必須有整數 `version: 1`、合法 entry 與整個內容集合內唯一的 ID。不存在的資產目錄同步為空內容；目錄存在但描述檔缺失或無效則失敗，保留上次成功輸出。Object_FPS 僅部署實際使用的 builtin shaders，自訂 shader 驗證樣本屬於公共 GPU 測試。

生成的 product manifest 位於 `share/gyo/products/<product>/manifest.json`。Schema 2 包含 product、kind、executables、required_files、runtime_dependencies、checks。共通 runner 位於 `build/acceptance/common/run_package_checks.py`，native linkage 檢查位於同目錄的 `validate_package.py`；遊戲特殊規則在 `build/acceptance/<game>`，不放在遊戲程式碼中。

診斷 executable 從 `build/acceptance/<game>` 產生，位於 build tree 的 `acceptance/<game>/bin`。驗收 runner 可以在產品的臨時副本中放入 probe，讓它讀取同一份 executable-relative 資產；正式產品／archive 不含 probe 或 Python 驗收程式。遊戲 runtime 僅讀 `bin/assets/<game>`，內建與遊戲 shader 同樣位於此根下。

Quick 與 Release 由 checks 的 profile 控制產品驗收深度。Linux toolchain job 固定以 Xvfb／Lavapipe 執行共通引擎的 GPU 渲染測試，即使沒有遊戲亦然；遊戲 job 另執行其 contract 宣告的 GPU checks。缺少所需能力、逾時或錯誤皆為失敗。軟體 Vulkan 結果與 Windows/macOS hosted 建置不代表實體 GPU 已驗證。Object_FPS 的外部驗收見[專案指南](object_fps/acceptance.zh-Hant.md)。

封裝與 Draft 階段重新推導同一來源 SHA 的完整預期集合，驗證每項 product/platform、checksum、archive 路徑安全、manifest、必要內容及 Release profile 證據。缺包、多包、重复身份或 Quick-only 證據都不接受。

`acceptance.json` 的 `package_sha256` 綁定產品內所有檔案的內容與符號連結目標，僅排除封裝時生成的根目錄 `build_metadata.json`。CPU、GPU 驗收與封裝之間若內容變更便拒絕，archive 驗證會再獨立計算相同摘要。產品隔離檢查同時拒絕其他遊戲的資產、其他 product manifest，以及未登錄的 `gyo_*`／`.exe` 程式。

## 4. 失敗與重跑

暫時的 HTTP 500／502／503／504、逾時、連線重設或截斷回應最多恢復四次（含首次），正常退避為 2／4／8 秒。伺服器的 `Retry-After` 若超過 60 秒則停止並回報；權限、資料與版本衝突立即失敗。每次重試重新讀取 tag／Draft／附件，驗證來源與 checksum 後只補缺少的附件，不盲目重送寫入。

- 原執行暫時失敗：使用 **Re-run failed jobs**，維持該次 SHA。
- 舊 artifacts 過期：對原執行使用 **Re-run all jobs**。
- Draft 附件只上傳一部分：重跑會核對既有附件並補缺，保留手寫標題與說明。
- 同名 tag 只允許指向同一 commit；不移動 tag，不覆寫不符的附件。
- 已公開的版本不修改；來源 commit 改變時使用新版本。
- 未完成的 `starter` 附件先有限重查，仍未完成則回報，不自動刪除。

舊執行重跑仍使用舊 commit 的 workflow，不能取得新版本的修正。同版本不並行執行；新的請求不取消正在執行的發佈。既有 Draft 的 prerelease、標題與說明保持原值。

## 5. 驗證證據

本機測試與靜態 workflow 檢查不等於遠端三平台或 Draft 已成功。本次版本應以實際 Actions Summary、必要 jobs、驗收報告與附件為準。歷史 dev logs 僅記錄當時的路徑與結果，不作為此次重構已通過的證明。

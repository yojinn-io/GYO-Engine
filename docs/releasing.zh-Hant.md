# GYO：CI、發佈通道與發佈程序

[日本語](releasing.ja.md) · [整體架構](architecture.md) · [渲染驗收](rendering_architecture.zh-Hant.md#r09)

本文說明目前的 CI 與發佈流程。內容依據 `.github/workflows/*.yml`、`.github/actions/native-setup`、`build/ci/common/*.py`、`CMakePresets.json`、`build/acceptance/*/checks.json`、`engine/config/projects.csv` 與 `engine/config/tools.csv`。發現不一致時以程式為準，並修正本文。

- 第 1～9 節：機制與規則
- 第 10 節：實例（附可直接執行的指令）
- 第 11～13 節：產品驗收細節、失敗與重跑、後續工作

## 1. 整體概觀

驗證分四個層級。L1 擋住合併，master 完整執行擋住 snapshot。Release 的前提（完整執行已通過的 master commit）由營運規則 4 保證，workflow 本身不檢查。L4 的證據參照是 Release 的輸入，必須在建置之前備妥。

| 層級 | 何時執行 | 內容 | 擋住什麼 |
|---|---|---|---|
| **L1**（合併閘門） | Ready 的 pull request、推送 master、手動執行 Cross-platform CI | 每個平台一個 job，以 `ci-<os>` preset 建置登錄啟用的遊戲、預設工具、Engine 與測試，並執行 `cpu`／`shader` 標籤的測試。Linux 列另外執行 host shader 測試、以 Lavapipe 執行的通用 GPU 測試、core 基準、Go 的 vet／test／race，以及 Go 服務的交叉編譯。不封裝 | PR 合併（`CI gate`） |
| **master 完整執行**（L1 + Quick） | 推送 master、手動執行 Cross-platform CI | 在 L1 之外，以 `profile: quick` 對全部產品執行 `build-and-validate.yml`。四個 toolchain 列與遊戲各平台列進行封裝、安裝後 Quick 驗收與壓縮，並上傳 Actions artifacts | Snapshot 發佈（全部 train） |
| **Release** | **Prepare Release**（手動） | 針對所選 train，以 `profile: release` 建置並驗收四個 toolchain 基準列與該 train 的各列。所有列都執行測試、host shader 測試與 Release checks，Linux 列執行 Go checks | 建立 tag 與 Draft |
| **L4**（實機） | 由人在實機上執行 | 遊戲以 `release_evidence` 宣告的項目（實體 GPU、真實螢幕等）。CI 不執行 | 遊戲 train 的 Prepare Release（沒有證據參照就在建置前失敗），以及發佈者的確認 |

```text
feature branch ──PR──> L1 ──CI gate──> 合併 ──> master 完整執行 ──CI gate──> snapshot（每條 train）
                                                     │
                                     完整執行通過的 master commit
                                                     │
                                       Prepare Release（一條 train）
                                       Release 驗收 ─> tag + Draft ─> 由人按 Publish release
```

- master 完整執行中，任何產品的任何一列失敗都會讓 `CI gate` 失敗，該 commit 的 snapshot 在**所有 train** 都不會發佈（snapshot job 以 `CI gate` 與 Quick 都成功為前提）。
- Prepare Release 依 train 彼此獨立。遊戲 train 只建置 toolchain 基準與該遊戲的列，因此不會被其他遊戲的失敗擋住；toolchain 基準失敗則會擋住所有 train。
- 公開（**Publish release**）一律由人操作；推送 tag 不是發佈入口。

## 2. 產品與擁有者

產品由登錄資料推導，workflow 與共用程式不寫產品名稱。

| 產品 | 壓縮檔（各附 `.sha256`） | 平台 | 擁有者 |
|---|---|---|---|
| 設計工具鏈（toolchain） | `gyo-toolchain-<platform>.tar.gz`（根目錄 `gyo-toolchain`） | 固定四個：`windows-x64`、`linux-x64`、`macos-arm64`、`macos-x64` | `engine/config/tools.csv` 中 `release` 啟用的工具（目前為 UI Editor 的 GUI 預設模式）及必要 runtime libraries |
| 遊戲 | `gyo-<game>-<platform>.tar.gz`（根目錄 `gyo-<game>`）；`object_fps_pvp` 含 client（`main`）與 `match` | `projects.csv` 中該遊戲 `enabled` 與平台欄位都啟用者；`macos` 欄同時啟用 `macos-arm64` 與 `macos-x64` | `apps/<game>`、`assets/<game>`、`build/acceptance/<game>`、`projects.csv` 的那一列 |
| 遊戲的 Go 服務 | `gyo-<game>-<role>-<platform>.tar.gz`；`object_fps_pvp` 為 `gyo-object_fps_pvp-gateway-linux-x64.tar.gz` 與 `...-windows-x64.tar.gz` | 只有 `gyo_app_add_go_service(... PLATFORMS ...)` 宣告的平台（gateway 為 Linux 與 Windows） | 該遊戲（`apps/<game>/go.mod`） |

- 目前登錄中啟用的遊戲只有 `object_fps_pvp`（`object_fps`、`object_fps_v2` 為 `enabled=0`）。`tools.csv` 中 `release=true` 的工具只有 `ui_editor`；`object_fps_preview` 依賴遊戲，是本機工具，不進入發佈包。
- `services/gyo_gateway` 是 Engine 層的 Go module（擁有者 `engine`），提供可重複使用的 HTTP、session 與 framing 機制，不依賴任何遊戲。CMake 在每種配置都記錄它，所以 L1 的 Linux 列與 toolchain 列即使沒有遊戲也會執行 vet／test／race。它不單獨產生壓縮檔。
- Train 由登錄推導：`tools` train（四平台 toolchain），以及每個啟用遊戲各一條 train（例如 `object_fps_pvp`）。遊戲 train 包含該遊戲的原生套件，以及 Linux 封裝列在 `go-services.json` 記錄的服務壓縮檔。同一個 release 的 client、match 與 gateway 來自同一 commit 與同一版本。`tools`、`toolchain` 不能作為遊戲 id。
- 發佈物是可執行產品與必要依賴，不提供 Engine SDK 或 source archive。Toolchain 不帶遊戲 source／assets；遊戲包不帶 CI、tests、acceptance executable、editor 或來源美術。引擎靜態連結進各產品，第三方動態 runtime 按需要部署。
- 沒有遊戲時（CSV 只有標頭或全部停用）仍可發佈四平台 toolchain。已選中的遊戲缺檔、編譯失敗或必要驗收失敗，會讓該次執行整體失敗。

## 3. 觸發與執行內容

| 操作 | Workflow | 執行內容 | 顯示的 check |
|---|---|---|---|
| 推送 feature 分支 | 無 | 不執行 CI（Cross-platform CI 的 push 觸發只限 master） | — |
| 建立 Draft PR 或推送到 Draft PR | Cross-platform CI、Package trial | `Select CI scope`（CI 政策測試與範圍判定，理由 `draft`）。L1／Quick 略過，`CI gate` 以預期略過通過。Package trial 在沒有標籤時不建置 | `Select CI scope`、`CI gate`、`Select trial packaging`（略過的 job 也會顯示） |
| 只變更 `docs/` 底下的 PR | 同上 | 理由 `docs-only`。略過 L1，`CI gate` 通過 | 同上 |
| 已變更程式的 Ready PR，再推送只改 `docs/` 的 commit | 同上 | 理由 `docs-increment`。沿用較早一個 commit 已通過的 L1，略過 L1，`CI gate` 通過；條件見下方。條件不成立時理由為 `pull-request`，照常執行 L1 | 同上 |
| Ready 的 PR（建立、推送、改為 Ready、重新開啟） | 同上 | 理由 `pull-request`。執行四列 `L1 / <platform>`；不跑 Quick，也不封裝 | `Select CI scope`、`L1 / windows-x64`、`L1 / linux-x64`、`L1 / macos-arm64`、`L1 / macos-x64`、`Quick acceptance`（略過）、`CI gate`、`Select trial packaging`（建立／推送／重新開啟時） |
| 開啟中的 PR 加上 `package` 標籤 | Package trial | 以 Quick 封裝全部產品並上傳 Actions artifacts；標籤存在期間，每次推送都重新建置 | `Trial packages / ...`（不是必要檢查） |
| 手動執行 **Package trial** | Package trial | 在所選分支執行相同的 Quick 封裝 | — |
| 推送 master（合併） | Cross-platform CI | L1 + Quick + `CI gate`；通過後 `Snapshot / <train>` 為每條 train 發佈 snapshot；L1 儲存編譯快取 | `L1 / ...`、`Quick acceptance / ...`、`CI gate`、`Plan snapshot trains`、`Snapshot / tools`、`Snapshot / object_fps_pvp` |
| 手動執行 **Cross-platform CI** | Cross-platform CI | L1 + Quick + `CI gate`（理由 `integration`）；不發佈 snapshot；只有在 master 上執行時才儲存編譯快取 | 同上（沒有 snapshot） |
| **Prepare Release** | Prepare Release | 一條 train 的 Release 驗收、tag 與 Draft | `Validate train, version and capture source commit`、`Full release acceptance / ...`、`Prepare verified draft and downloads` |
| 推送 tag、**Publish release** | 無 | 不建置任何東西；Publish 只公開既有 Draft | — |

- 範圍判定（`build/ci/common/ci_scope.py`）在 job 內進行，不使用觸發條件的 path／draft 篩選，因此 `CI gate` 在每個 PR 事件都會回報。
- `docs-only` 只認以 `docs/` 開頭的路徑。變更根目錄 `README*.md`、`AGENTS.md` 等檔案的 PR 會執行 L1。重新命名會拆開判定，從 `docs/` 以外移入 `docs/` 的檔案也算非文件變更。
- `docs-increment`：從 PR head 的前一個 commit 沿第一親代往回找，最多 20 個，找到第一個同時滿足下列條件的 commit，就沿用它的 L1。
  - 四列 `L1 / <platform>` 的最新 check run 都是 GitHub Actions 的 `success`。略過、取消、失敗或尚未完成都不算。
  - 從它到 head 的變更只在 `docs/` 底下，判定方式與 `docs-only` 相同。
  - 目前 master 的尖端是它的祖先。master 只會前進，所以它當時測試的合併結果就是它本身；現在的合併結果就是 head，兩者的程式完全相同。
  - master 前進了、中間有程式變更、程式那次推送的 L1 被新推送取消、或讀取 check run 失敗，一律照常執行 L1。最壞情況是多跑一次，不會略過未測試的程式。
  - `Select CI scope` 的 Summary 會寫出沿用的 commit，或不沿用的原因。
  - 讀取 check run 需要 `checks: read`，往回找需要完整歷史（checkout 的 `fetch-depth: 0`）。前提是 master 不改寫歷史（不 force push）。
- Draft 判定使用執行開始時讀取的 PR 目前狀態；重新執行舊的 Draft 執行時，若 PR 已改為 Ready 就會跑 L1，仍是 Draft 則再次略過。
- 同一 PR 的新推送會取消舊執行；master 的執行彼此不取消。
- 只改 PR 目標分支（`edited`）不會重新執行。目前未支援 merge queue（`merge_group`）；啟用前須先加入該觸發與範圍判定。
- Package trial 不建置已關閉或已合併的 PR；加上 `package` 以外的標籤也不會取消執行中的 trial。

## 4. `CI gate`：唯一的必要檢查

建議以 branch protection（或 ruleset）把 master 的必要狀態檢查**只設為 `CI gate`**。這項設定在 GitHub 的 Settings → Branches（或 Rules → Rulesets）進行，不在儲存庫檔案中。

- `CI gate` 一定會執行；只有在 `Select CI scope` 成功、被選中的層級（L1、Quick）全部 `success`、未選中的層級為 `skipped` 時才通過。失敗、取消或非預期的略過都算失敗。
- 不要把 `L1 / <platform>` 或 Quick 各列個別設為必要檢查；它們在 Draft、docs-only 與 docs-increment 時會被略過，個別設定會讓 PR 無法合併。
- Package trial 的 check 不設為必要檢查。

## 5. 平台與驗證等級

| 平台 | Runner／host | Preset／編譯器 | CPU 測試執行方式 | GPU | 備註 |
|---|---|---|---|---|---|
| `linux-x64` | `ubuntu-24.04` | `ci-linux`、gcc-14 | 原生 | 以 Xvfb + Mesa Lavapipe（軟體 Vulkan）執行通用 GPU 測試與產品 GPU checks | L1 的 host shader 測試、core 基準、Go vet／test／race 與 Go 服務交叉編譯只在此列 |
| `windows-x64` | `windows-2025` | `ci-windows`、MSVC（`msvc-vs2026`） | 原生 | 無 | 封裝 jobs 的 toolchain 列也執行 core 基準；Release 另執行獨立複製驗證（`app_copy_integration.py`） |
| `macos-arm64` | `macos-15`（arm64） | `ci-macos`、Xcode 16.4 的 AppleClang，部署目標 13.3 | 原生 | 無 | 封裝 jobs 的 toolchain 列也執行 core 基準 |
| `macos-x64` | `macos-15`（在 arm64 host 交叉編譯） | `ci-macos-x64`（`CMAKE_OSX_ARCHITECTURES=x86_64`，部署目標 13.3） | 經 Rosetta 2 轉譯執行 | 無 | 沒有實體 Intel Mac 驗收；runner 無法使用 Rosetta 2 時在建置前失敗；core 基準由 host 列負責 |

- CI 沒有實體 GPU 驗收。軟體 Vulkan 與 Windows／macOS hosted runner 的結果不代表實體 GPU 已驗證；遊戲需要的實機確認屬於 L4（第 8 節）。
- 封裝 jobs 的 macOS 列在使用 shader bundle 的配置中，會確認可以離線編譯 Metal。
- Go 服務在 Linux 列以 `CGO_ENABLED=0` 交叉編譯到各平台；CI 不執行服務程式本身。
- Linkage 檢查要求 macOS 套件內的程式與原生函式庫只含目標架構。
- 每列 Summary 列出建置 host／目標與 CPU 執行方式，壓縮檔的 `build_metadata.json` 也以 `cpu_execution`（`native` 或 `rosetta2`）記錄。Snapshot 與 Draft 的說明會自動列出各壓縮檔的驗證等級（原生或交叉編譯、通過的 checks 數、是否經 Rosetta 2、有無 GPU checks、沒有實體 GPU）。

## 6. 快取與初次執行

- **編譯快取（sccache v0.18.0）**：`native-setup` 讓所有原生 job 的編譯經過 sccache。快取 key 依平台、toolchain、編譯器版本與 preset 區分，因此 `macos-x64` 的目標物件不會與 arm64 混用。**只有 master 上的 L1 會儲存**（推送 master，或在 master 手動執行）；PR 的 L1、Quick、trial 與 Prepare Release 只讀取最新的種子。
- **Host 工具**：Shader 編譯器等 host 工具依 runner 平台（host）以 `actions/cache` 儲存。`macos-arm64` 與 `macos-x64` 共用同一份 arm64 原生工具，目標架構不會帶進 host 工具建置。
- **Go**：`actions/setup-go` 以各 module 的 `go.mod`／`go.sum` 為 key 快取 module。

初次執行或更新 toolchain 後可預期的情況：

- master 的 L1 尚未完成過，或編譯器／Xcode／preset 改變之後，沒有可用的種子，PR 的 L1 與封裝 jobs 會全部重新編譯。macOS 列平行數為 2，最慢；每個原生 job 上限 120 分鐘。
- 新 toolchain 的種子要等下一次推送 master 的 L1 儲存後才生效；只反覆執行 PR 不會產生種子。
- `macos-x64` 列在缺少 Rosetta 2 的 runner 上會嘗試安裝，仍無法使用時在建置前以明確錯誤失敗；runner image 沒有固定的 Xcode 16.4 時同樣失敗。
- 只有 workflow 已在預設分支時，GitHub 才會顯示 **Run workflow**（手動執行）。新增 workflow 的 PR 要等合併後才能手動執行；所選來源分支也必須包含相同的 workflow 與支援程式。
- 儲存庫沒有 `package` 標籤時，先建立一次（第 10.3 節）。
- 第一個 snapshot 沒有需要刪除的舊 snapshot。

## 7. 發佈通道與版本

| 通道 | 入口 | 內容與保留 | Profile |
|---|---|---|---|
| **Trial** | 開啟中 PR 的 `package` 標籤，或手動執行 **Package trial** | 只有 Actions artifacts：`gyo-package-<product>-<platform>`、`gyo-service-<product>`、`diagnostics-<product>-<platform>`，保留 **14 天**；不建立 tag 或 release | Quick |
| **Snapshot** | `CI gate` 通過的 master 推送 | 每條 train 一個 prerelease（不標為 Latest）。Tag 為 `<train>-snapshot-<yyyymmdd>-<sha7>`（日期是 commit 的 UTC 日期，只用於命名）。每條 train 依發佈順序保留最近 **5 個**，更早的 snapshot release 與其 snapshot tag 會刪除（不碰其他 tag） | Quick |
| **正式發佈** | **Prepare Release**（手動）→ 由人按 **Publish release** | 每條 train 一個 Draft。Tag 為 `tools-vYYYY.M.N`（例如 `tools-v2026.10.1`）或 `<game>-vX.Y.Z`（例如 `object_fps_pvp-v5.0.0`） | Release |

- 推送 master 的 Quick artifacts 也以相同名稱與保留期限留在 Actions。
- Snapshot 說明會註明這是供測試的自動 snapshot，不是受支援的 release。重新執行舊的 run 時，若其 commit 是較新 snapshot commit 的祖先，便不發佈。
- **Tools 版本**採日曆版本 `vYYYY.M.N`：月份不補 0，N 從 1 起算（例如 `v2026.10.1`、`v2026.10.2`），也可加 `-rc.1` 等後綴。
- **遊戲版本**採帶 `v` 的 SemVer（例如 `v5.0.0`、`v5.1.0-rc.1`）。
- 產品版本與通訊協定版本彼此獨立，協定相容性不由 tag 表示。`projects.csv`／`tools.csv` 的 `version`、`description` 欄是備註，不決定發佈版本。
- 壓縮檔目前未做程式碼簽章（Draft 與 snapshot 說明會註明「Archives are not code-signed.」）。

## 8. 遊戲 train 的 L4 證據

- 項目由遊戲自己在 `build/acceptance/<game>/checks.json` 的選用欄位 `release_evidence` 宣告。每個項目只有三個欄位：`name`（識別字）、`description`（單行，最多 300 字元）、`platforms`（四平台的子集合），且只套用到該 train 啟用的平台。
- `object_fps_pvp` 在四個平台宣告 `gui_visible_latency`（在真實螢幕上短測 GUI 可見延遲）與 `physical_gpu_visual`（在實體 GPU 上檢查畫面）。
- 不設 self-hosted runner，CI 也不執行這些項目。由人在實機上執行，把結果記錄在 issue、PR、discussion 或 artifact，再把參照（單行、最多 500 字元、不以 `-` 開頭）填入 Prepare Release 的 `l4_evidence`。
- 遊戲宣告了項目而 `l4_evidence` 為空時，Prepare Release 在建置前的第一個 job 就失敗；反過來，沒有項目的 train（例如 `tools`）若填入 `l4_evidence` 也會失敗。Snapshot 不使用 L4 證據。
- Draft 說明會加入「Real-device evidence (L4)」一節，列出證據參照（網址顯示為自動連結，其他內容顯示為程式碼，不解讀為 Markdown）與各項目的核對清單。CI 只檢查有無參照，證據內容由發佈者確認。

## 9. 作業規則

1. **不移動 tag。** Prepare Release 在建立 Draft 時就建立 tag；同名 tag 只有指向同一 commit 時才沿用。來源 commit 改變時使用新版本；已公開的版本不修改。
2. **手動公開。** 檢查 Draft 後由人按 **Publish release**（或執行 `gh release edit <tag> --draft=false`）。公開不會重新編譯。
3. **master 當天修復或 revert。** master 完整執行失敗會停下所有 train 的 snapshot，也破壞下一次 Prepare Release 的前提。
4. **只從完整執行（L1 + Quick、`CI gate`）通過的 master commit 執行 Prepare Release。** Workflow 本身接受任何分支，不檢查這個條件；執行前請依第 10.5 節步驟 1 確認。
5. 變更 `.github/`、`build/ci/`、`build/acceptance/` 或封裝 CMake 的 PR，因為 L1 不執行封裝與安裝後驗收，合併前要在該分支手動執行 **Cross-platform CI**，或以 `package` 標籤執行 trial，確認 Quick 封裝通過。
6. 本機測試與 workflow 靜態檢查不等於遠端各平台或 Draft 已成功；以所發佈 revision 的 Actions Summary、必要 jobs、驗收報告與附件為準。`docs/dev_logs/` 的歷史紀錄只代表當時 commit 的結果。

## 10. 實例

指令假設已安裝 `gh` CLI、已執行 `gh auth login`，並在儲存庫的 clone 內執行。`<n>`、`<run-id>`、日期與 SHA 請換成實際值。

### 10.1 一般 PR：從 Draft 到合併

```sh
git switch -c feature/ui-text-wrap
# 修改並 commit
git push -u origin feature/ui-text-wrap          # 不執行 CI
gh pr create --draft --base master --fill        # Draft PR
```

1. 建立 Draft PR 與之後的推送，只會執行 `Select CI scope`（理由 `draft`）與 `CI gate`（通過）。`Select trial packaging` 也會出現，但沒有標籤所以不建置。
2. 準備好審查時改為 Ready：

   ```sh
   gh pr ready <n>
   gh pr checks <n> --watch
   ```

   會執行 `L1 / windows-x64`、`L1 / linux-x64`、`L1 / macos-arm64`、`L1 / macos-x64` 與 `CI gate`。若改為 Ready 時 head commit 沒有變，新的 `CI gate` 要等 L1 結束才出現，期間仍顯示 Draft 執行的通過結果。**新的 `CI gate` 回報前，不要合併或啟用 auto-merge。**
3. 之後每次推送都會取消同一 PR 的舊執行並重跑 L1。
4. 若變更了 `.github/`、`build/ci/`、`build/acceptance/` 或封裝 CMake，合併前也要確認封裝路徑（第 10.3 節，或下列指令）：

   ```sh
   gh workflow run cross-platform.yml --ref feature/ui-text-wrap
   ```

5. 合併：

   ```sh
   gh pr merge <n> --merge
   ```

6. 確認推送 master 的完整執行；snapshot 也在這次執行中發佈：

   ```sh
   gh run list --workflow cross-platform.yml --branch master --event push --limit 3
   gh run watch <run-id> --exit-status
   ```

### 10.2 只改文件的 PR

```sh
git switch -c docs/release-notes
# 只修改 docs/ 底下的檔案並 commit
git push -u origin docs/release-notes
gh pr create --base master --fill
gh pr checks <n>
```

`Select CI scope` 的 Summary 會顯示 `Reason: docs-only`，`CI gate` 隨即通過。若同時修改根目錄的 `README.md` 等檔案，就不再是 `docs-only`，會執行 L1。只改文件的 PR 加上 `package` 標籤仍會執行 trial 封裝，請不要加。合併後推送 master 照常執行完整流程與 snapshot。

### 10.3 取得 PR 的試用套件

使用標籤（只適用開啟中的 PR）：

```sh
gh label create package --description "Build trial packages for this pull request"   # 只需第一次
gh pr edit <n> --add-label package
gh run list --workflow package-trial.yml --branch feature/ui-text-wrap --limit 1
gh run watch <run-id> --exit-status
```

手動執行（沒有 input，以 `--ref` 選分支）：

```sh
gh workflow run package-trial.yml --ref feature/ui-text-wrap
```

從完成的 run 下載 artifact 並核對 checksum：

```sh
gh run download <run-id> -n gyo-package-toolchain-macos-arm64 -D trial
gh run download <run-id> -p 'gyo-package-object_fps_pvp-*' -D trial
gh run download <run-id> -n gyo-service-object_fps_pvp -D trial
cd trial && shasum -a 256 -c gyo-toolchain-macos-arm64.tar.gz.sha256      # Linux 使用 sha256sum -c
```

Artifacts 14 天後消失。標籤一直留著時，每次推送都會對全部產品執行 Quick 封裝；不再需要時以 `gh pr edit <n> --remove-label package` 移除。

### 10.4 尋找並下載 master snapshot

```sh
# tools train 的 snapshot（依公開時間由新到舊，排除 Draft）
gh release list --exclude-drafts --limit 50 --json tagName,publishedAt \
  --jq '[.[] | select(.tagName | startswith("tools-snapshot-"))] | sort_by(.publishedAt) | reverse | .[] | "\(.publishedAt) \(.tagName)"'

# 遊戲 train 的 snapshot
gh release list --exclude-drafts --limit 50 --json tagName,publishedAt \
  --jq '[.[] | select(.tagName | startswith("object_fps_pvp-snapshot-"))] | sort_by(.publishedAt) | reverse | .[] | .tagName'

# 只下載某個 snapshot 中需要的壓縮檔（tag 為範例）
gh release download tools-snapshot-20261002-1a2b3c4 -p 'gyo-toolchain-macos-arm64.tar.gz*' -D snapshot
gh release download object_fps_pvp-snapshot-20261002-1a2b3c4 -p 'gyo-object_fps_pvp-gateway-*' -D snapshot
cd snapshot && shasum -a 256 -c gyo-toolchain-macos-arm64.tar.gz.sha256
```

Tag 結尾的 7 個字元是 master commit 的 SHA。說明中的「CI verification」表列出各壓縮檔的驗證等級。每條 train 只保留最近 5 個，需要長期保存的版本請走正式發佈。

### 10.5 準備並公開 tools 版本

1. 確認目標 master commit 的完整執行已通過（規則 4）：

   ```sh
   git fetch origin && git rev-parse origin/master
   gh run list --workflow cross-platform.yml --commit "$(git rev-parse origin/master)" --event push \
     --json databaseId,conclusion,url
   gh run view <run-id> --json jobs --jq '.jobs[] | select(.name == "CI gate") | .conclusion'
   ```

   `conclusion` 是包含 Snapshot jobs 在內的整個 run 結果，比規則 4 的條件（`CI gate` 通過）更嚴格。若只有 snapshot 發佈失敗，請用第二行確認 `CI gate` job 為 `success`。Prepare Release 會固定執行當下 `--ref master` 的最新 commit；如果 master 之後又前進、而新 commit 的執行尚未完成，請等它完成再繼續。

2. 執行 Prepare Release。Input 為 `train`、`version`、`prerelease`（預設 `false`）與 `l4_evidence`（tools 保持空白）：

   ```sh
   gh workflow run prepare-release.yml --ref master -f train=tools -f version=v2026.10.1
   sleep 5
   gh run list --workflow prepare-release.yml --event workflow_dispatch --limit 5 \
     --json databaseId,headSha,displayTitle,createdAt
   gh run watch <run-id> --exit-status
   ```

   `gh workflow run` 不會回傳 run ID，run 登錄也需要幾秒，因此請從清單中找出 `displayTitle` 為 `Prepare Release / tools / v2026.10.1 / master` 的 run，把它的 `databaseId` 交給 `gh run watch`，並確認 `headSha` 與步驟 1 的 commit 相同。第一個 job 的 Summary 會列出 train、tag 與來源 commit；接著以 Release profile 建置並驗收四個 toolchain 列，通過後建立 tag `tools-v2026.10.1` 並準備 Draft。

3. 檢查 Draft：

   ```sh
   gh release view tools-v2026.10.1
   gh release download tools-v2026.10.1 -D review
   cd review && for f in *.sha256; do shasum -a 256 -c "$f"; done
   ```

   附件為 `gyo-toolchain-{windows-x64,linux-x64,macos-arm64,macos-x64}.tar.gz` 及各自的 `.sha256`，共 8 個檔案。說明包含自動產生的變更紀錄與「CI verification」表。核對 commit、版本、附件與說明。

4. 公開（由人操作）：在 GitHub 的 Draft 頁面按 **Publish release**，或執行：

   ```sh
   gh release edit tools-v2026.10.1 --draft=false
   ```

要發佈 RC 時，改用 `-f version=v2026.10.1-rc.1 -f prerelease=true`。

### 10.6 附 L4 證據準備遊戲版本

1. 依第 10.5 節步驟 1 確認 master commit。
2. 在實機上執行宣告的項目（`gui_visible_latency`、`physical_gpu_visual`），具體做法依各項目的 `description` 與遊戲文件。把結果整理在一個 issue：

   ```sh
   gh issue create --title "object_fps_pvp v5.0.0 L4 evidence" --body-file l4-evidence.md
   ```

3. 把 issue 網址填入 `l4_evidence` 並執行 Prepare Release：

   ```sh
   gh workflow run prepare-release.yml --ref master \
     -f train=object_fps_pvp -f version=v5.0.0 \
     -f l4_evidence=https://github.com/yojinn-io/GYO-Engine/issues/<n>
   ```

   省略 `l4_evidence` 時，第一個 job 會因一則以 `Train object_fps_pvp requires real-device (L4) release evidence for: gui_visible_latency, physical_gpu_visual` 開頭的錯誤而失敗（完整訊息前面加上 `Release validation failed: `，後面接著要求把證據記錄在 issue、PR、discussion 或 artifact，並把連結填入 `l4_evidence`），不會開始建置。

4. 四個 toolchain 基準列、四個 `object_fps_pvp` 列（Release profile）與 Linux 列的 gateway 交叉編譯都通過後，建立 tag `object_fps_pvp-v5.0.0` 與 Draft。附件為 4 個 `gyo-object_fps_pvp-<platform>.tar.gz`、`gyo-object_fps_pvp-gateway-linux-x64.tar.gz` 與 `...-windows-x64.tar.gz`，加上各自的 `.sha256`，共 12 個檔案。遊戲的 Draft 不含 toolchain 壓縮檔。
5. 在 Draft 的「Real-device evidence (L4)」一節逐項對照證據後，按 **Publish release**（或執行 `gh release edit object_fps_pvp-v5.0.0 --draft=false`）。

### 10.7 把新遊戲或新工具加入發佈

**遊戲**（例如 `my_game`），只變更該遊戲的內容與登錄資料：

1. 準備 `apps/my_game/`（`CMakeLists.txt`、`project.json`、原始碼）與 `assets/my_game/`（見[建立／複製遊戲](creating_apps.md)）。
2. 在 `engine/config/projects.csv` 新增一列：

   ```csv
   my_game,My game,,1,1,1,1
   ```

3. 需要時在 `build/acceptance/my_game/checks.json` 宣告安裝後 checks；需要實機確認時加上 `release_evidence`：

   ```json
   {
     "version": 1,
     "checks": [],
     "release_evidence": [
       {
         "name": "physical_gpu_visual",
         "description": "Client on a physical GPU: start and play one round; no missing geometry, shader or presentation errors",
         "platforms": ["windows-x64", "linux-x64", "macos-arm64", "macos-x64"]
       }
     ]
   }
   ```

4. 有 Go 服務時，放置 `apps/my_game/go.mod`，並在遊戲的 `CMakeLists.txt` 宣告。遊戲必須在 Linux 啟用，`PLATFORMS` 必須是該遊戲已啟用平台的子集合（違反時 configure 失敗）：

   ```cmake
   gyo_app_add_go_service(gateway PACKAGE ./gateway/cmd PLATFORMS linux-x64 windows-x64)
   ```

自動改變的部分：L1 在啟用的平台建置並測試這個遊戲（`ci-<os>` preset 為 `GYO_APPS=AUTO`），並在 Linux 列檢查其 Go module。master 的 Quick 會加入 `my_game` 各列，壓縮檔以 `gyo-package-my_game-<platform>` 上傳。`my_game` train 開始發佈 snapshot（`my_game-snapshot-...`），Prepare Release 接受 `train=my_game`。服務壓縮檔經 `go-services.json` 進入該 train。不需要修改 workflow 或 `build/ci`。移除遊戲時，停用 CSV 列，或把該遊戲的內容與登錄一起刪除。

**工具**（例如 `level_editor`）：

1. 準備 `tools/level_editor/`（`CMakeLists.txt`、`project.json`）。發佈工具的預設模式必須 `packageable: true`，且 `requires_apps` 為空（見[工具登錄與選擇](tool_projects.md)）。
2. 在 `engine/config/tools.csv` 新增一列：

   ```csv
   level_editor,Level editor,1,true,true,true,true,true,true
   ```

3. 在 `build/acceptance/level_editor/checks.json` 宣告 checks，使每個選中的發佈平台在 `quick` 與 `release` 都會執行一個已登錄的 executable 或 probe；缺少時 configure 失敗。

自動改變的部分：`default=true` 時 L1 會建置並測試；`release=true` 的平台會把它放進 `gyo-toolchain-<platform>`。壓縮檔驗證要求工具擁有者集合完全一致，而預期集合由登錄推導，所以下一個 snapshot 與 `tools` train 的 Draft 會自動包含它。

### 10.8 在開發機重現 L1

L1 的一列就是「以 `ci-<os>` preset configure → 建置 → 執行 `cpu|shader` 標籤的測試」。在 Apple silicon 的 Mac 上：

```sh
cmake --preset ci-macos
cmake --build --preset ci-macos
ctest --preset ci-macos              # 標籤 cpu|shader；沒有測試時視為錯誤
```

`macos-x64` 列（需要 Rosetta 2）：

```sh
softwareupdate --install-rosetta --agree-to-license   # 尚未安裝時
cmake --preset ci-macos-x64
cmake --build --preset ci-macos-x64
ctest --preset ci-macos-x64
```

Linux（gcc-14）用 `ci-linux`，Windows（Visual Studio 開發者 shell，`cl`）用 `ci-windows`，形式相同。沒有與 CI 相同的編譯器時，可改用標籤篩選相同的 `test` preset：

```sh
cmake --preset test && cmake --build --preset test && ctest --preset test
```

只有 Linux 列額外執行的部分：

```sh
# Host shader 測試（只在建置了 shader host 工具的配置）
ctest --test-dir build/target/_build/ci-linux/host-tools -C Release -L shader --output-on-failure --no-tests=error
# 通用 GPU 測試（需要 Vulkan 與顯示環境；CI 使用 Xvfb + Lavapipe）
xvfb-run -a ctest --test-dir build/target/_build/ci-linux -L gpu -R '^render\.' --no-tests=error --output-on-failure
# Core 基準
cmake --preset core -B build/target/_build/ci-linux-core -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
cmake --build build/target/_build/ci-linux-core
ctest --test-dir build/target/_build/ci-linux-core --output-on-failure --no-tests=error
```

Go（使用 configure 後 `gyo-build.json` 記錄的 module 與服務；PATH 上需要 Go）：

```sh
python3 build/ci/common/go_checks.py plan  --build-info build/target/_build/ci-macos/gyo-build.json
python3 build/ci/common/go_checks.py check --build-info build/target/_build/ci-macos/gyo-build.json          # vet 與 test
python3 build/ci/common/go_checks.py check --build-info build/target/_build/ci-macos/gyo-build.json --race   # CI 在 Linux 另跑 race
python3 build/ci/common/go_checks.py build --build-info build/target/_build/ci-macos/gyo-build.json --output-dir build/target/_build/ci-macos/go-services
ctest --test-dir build/target/_build/ci-macos -L go     # 透過 CTest 執行遊戲的 Go module（race 只在 Linux host）
```

`Select CI scope` 最先執行的 CI 政策測試：

```sh
python3 -m unittest discover -s tests/common/ci -v
```

本機通過不代表遠端四個平台的 L1 已通過（規則 6）。

## 11. 產品組裝與驗收（細節）

### 組裝

本機與 CI 共用產品組裝：`build/assemble_runtime.py` 依 `assets/<game>/content.json` 驗證並組裝準備好的資產，由 CMake generic hook 自動呼叫。離線 shader compiler 位於 `engine/render/shaders/pipeline`。普通遊戲只需要產品 build，不需要 CI／tests 或 package acceptance。

組裝器與套件驗證共用 `build/content_contract.py`；C++ runtime 與 Editor 以同一組 fixtures 驗證原生解析器。Catalog 必須有整數 `version: 1`、合法 entry 與整個內容集合內唯一的 ID。不存在的資產目錄同步為空內容；目錄存在但描述檔缺失或無效則失敗，保留上次成功輸出。Object_FPS 僅部署實際使用的 builtin shaders，自訂 shader 驗證樣本屬於公共 GPU 測試。

### Manifest 與安裝後驗收

- 生成的 product manifest 位於 `share/gyo/products/<product>/manifest.json`（schema 3），要求非空的建置 configuration。Executable 以 `owner.role` 登錄，各自記錄 owner、role、path、runtime_dependencies；另有 required_files、native_files 及 owner 所屬的 checks。Native 依賴聯集只決定複製哪些檔案，linkage 檢查逐一使用程式自己的需求，因此同包的 SDL GUI 與無 SDL CLI 可以共存。
- 共用 runner 位於 `build/acceptance/common/run_package_checks.py`，native linkage 檢查位於同目錄的 `validate_package.py`；專屬規則在 `build/acceptance/<owner>`。
- CMake 生成 `<build>/packages/<product>/<configuration>/acceptance-context.json`，以 `--context` 傳給 runner。Context 綁定 manifest 與建置配置，只解析 `@CHECK_ROOT@`、`@PROBE:<role>@` 所需的外部位置，不改變檢查命令或集合。`@EXECUTABLE:<role>@` 依 owner 查找產品，檢查與日誌以 `owner.name` 識別。Runner 可以在產品臨時副本中放入 probe，但不能覆寫正式程式；正式產品不包含 context、probe 或 Python 驗收程式。遊戲 runtime 僅讀 `bin/assets/<game>`。
- Quick 與 Release 的驗收深度及 GPU suite 參數由 owner 自己的 checks 決定。Linux toolchain 列即使沒有遊戲，也以 Xvfb／Lavapipe 執行通用引擎 GPU 渲染測試；所有產品（包含 toolchain）另外執行其 contract 宣告的 GPU checks。缺少所需能力、逾時或錯誤皆為失敗。另見 [Object_FPS 的外部驗收](object_fps/acceptance.zh-Hant.md)。

### 壓縮檔與 Draft 驗證

- 封裝、snapshot 與 Draft 準備都從固定 checkout 的登錄重新推導該 train 的預期集合，驗證每項 product／platform、工具擁有者集合、checksum、archive 路徑安全、manifest、必要內容與 profile 證據；服務壓縮檔須與服務紀錄（`go-services.json`）完全一致。缺包、多包、工具子集冒充完整工具鏈與重複身分都不接受。正式 Draft 只接受 Release 證據，snapshot 接受 Quick 證據。
- `acceptance.json` 的 `package_sha256` 綁定產品內所有檔案的內容與符號連結目標，僅排除封裝時生成的根目錄 `build_metadata.json`。驗收與封裝之間若內容變更便拒絕，archive 驗證會再獨立計算相同摘要。產品隔離依據登錄的程式、原生函式庫與內容清單，拒絕任何未登錄檔案，不依賴前綴或副檔名猜測。只有宣告的 native files 可使用限定在套件內的符號連結。

## 12. 失敗與重跑

- Prepare Release 一開始就固定完整的來源 SHA，後續分支變更不會混入。必要 job 失敗、取消或意外略過都會阻止 Draft；不能把部分平台成功當作整體成功。
- 暫時的 HTTP 500／502／503／504、逾時、連線重設或截斷回應，最多嘗試恢復四次（含首次），正常退避為 2／4／8 秒；伺服器的 `Retry-After` 超過 60 秒時停止並回報。權限、資料與版本衝突立即失敗。每次重試都重新讀取 tag／Draft／附件，驗證來源與 checksum 後只補缺少的部分。
- 暫時失敗：使用 **Re-run failed jobs**，維持該次執行的 SHA。
- Artifacts 已過期：對原執行使用 **Re-run all jobs**。
- Draft 附件只上傳一部分：重跑會核對既有附件並補齊，保留手寫的標題與說明。
- 剛建立的 tag 可能還讀不到（GitHub 的讀取比寫入慢一點）。這時依 1／2／4／8 秒重新讀取，仍讀不到才失敗；讀到的 tag 指向別的 commit 時立即失敗。
- 同名 tag 只允許指向同一 commit；不移動 tag，也不覆寫不符的附件。既有 Draft 的 tag 若已被刪除會失敗（請恢復 tag 或改用新版本）。
- 未完成的 `starter` 附件先有限次重查，仍未完成則回報，不自動刪除。
- 重跑舊執行時使用該 commit 的 workflow，不會取得新版本的修正。同一 train 與版本的 Prepare Release 不並行；新的請求不取消正在進行的準備。既有 Draft 的 prerelease、標題與說明保持原值。
- 已發佈的 snapshot 若缺少附件，不修改並直接失敗；要重新發佈同一 commit，先手動刪除該 snapshot。已升格為正式 release 的 snapshot 不修改。
- 只有 Draft job 與 snapshot job 具有 `contents: write`，建置 jobs 保持唯讀。從變更 workflow 檔案的分支建立 tag 或 Draft 時若遇到 403／404，需要另外設定具備 Contents 與 Workflows 寫入權限的 token。

## 13. 後續工作

- **Tools 與遊戲之間的 Data Contract 檢查**：tools train 與遊戲 train 各自訂版本，但目前沒有宣告並檢查「工具輸出的資料」與「遊戲讀取的資料」是否相容的機制。
- **程式碼簽章**：壓縮檔與執行檔都尚未簽章（TODO）。
- **Merge queue**：尚未支援 `merge_group` 觸發。
- **Prepare Release 的來源條件**：規則 4（只從完整執行通過的 master commit 執行）是作業約定，workflow 不檢查。

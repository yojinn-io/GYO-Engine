# 基礎後續整理：批次計畫

更新：2026-10-04。基準：master `05042fa`。**全部批次未開始。** 先讀 [進度](README.md)、[交接](HANDOFF.md)。

各項目的來源、證據與更正見 HANDOFF「正式項目清單」。本文件只寫每批做什麼、怎麼驗收、何時停止。

## 0. 共通規則（每批適用）

- 每次只做使用者指定的批次；一批一個 PR（commit 與 PR 用日語）。開始、里程碑、停止時更新 README、HANDOFF、dev_log 後停止，不自動開始下一批。
- 先凍結來源、產物與分析器再量測；開發與乾淨量測不同時進行；失敗跑次保留，先有限定位；長測另外授權。
- 每批同步更新 [未啟用產品的遷移清單](inactive_products.md) 的對應節。
- 檔位依變速箱；ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意（D8）。
- **權威不變的證明**：正式證明是同機兩樹（base／branch）比對（Math B6a 的先例）。Engine 側用 FF-1 語料紀錄；提出需求的消費端另有權威 digest 閘門，消費端存在時一併比對。CI 只做自洽檢查與不依賴 libm 的子集。不修改共通 workflow。
- **量測基線世代**（只適用於經由消費端做 L2 量測的批次）：一律在本批的 base commit 上，以凍結工具量 before／after 再比較；消費端較早的基線只作歷史參照，舊穩定基線的數字只用來對門檻。
- 不依 OS 名稱分支；共通測試只用合成資料，不假設任何產品存在。
- 平台表的六列固定如下；實機只有 macOS Intel／Metal（D11⑩）。

| 平台 | 預設 |
|---|---|
| macOS Intel／Metal 實機 | 本機建置與 L1；有 GUI 項目時加 L2 |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 既有 `render.*` GPU 測試照常執行 |
| Windows D3D12 實機 | 未執行（沒有實機；CI 也不執行 D3D12） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機；只有 CI 的 macos-arm64 L1） |

---

## FF-1 Collision 判定語料

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ff1.zh-Hant.md)。

### 目標與範圍

做：

- 在 `tests/common/collision` 新增合成語料，涵蓋**全部公開查詢**：
  - `RaycastAabb`（`engine/collision/src/Collision.cpp:164`）。
  - `RaycastCapsule` 兩組多載（`Collision.cpp:176` 的 `VerticalCapsule` 版、`CapsuleQueries.cpp:210` 的 `Math::Capsule` 版）。
  - `SweepSphereAgainstCapsule` 兩組多載（`Collision.cpp:186`、`CapsuleQueries.cpp:225`）。
  - `OverlapVerticalCapsuleAabb`、`OverlapVerticalCapsules`、`SweepVerticalCapsuleAgainstAabb`、`SweepVerticalCapsuleAgainstCapsule`（`CapsuleQueries.cpp:239-286`）。
- 情境類別：射線、掃掠、`VerticalCapsule`、`Aabb`；擦邊、平行、極短掃掠、長射線、容差邊界。
- 輸出每個查詢結果的可比對紀錄（命中與否、距離或 fraction 的位元、Contact 各欄位的位元），供 FF-9 同機兩樹比對。
- 膠囊另外比較兩個公開多載：`VerticalCapsule` 版（float 演算法）與 `ToCapsule` 轉成 `Math::Capsule` 後的版本（double 演算法）。統計命中翻轉數與距離 ULP 分布，寫進 HANDOFF。
- 退化膠囊只記錄現況（哪些輸入被哪一份驗證放行、之後在哪裡中止），不修。

不做：

- 不改 Collision 的任何程式碼。
- 不為了比較而公開內部函式（double 的 AABB 射線 `RayRoundedBox` 在匿名命名空間，`RaycastAabb` 的 float／double 比較留給 FF-9 的兩樹比對）。
- 不使用任何產品的 arena 或資料。

### 交付

- `tests/common/collision` 的語料與測試；紀錄輸出方式與兩樹比對步驟（腳本位置在開始時決定，候選為本夾 `scripts/`，比照 math-foundation）。
- HANDOFF 的 FF-1 節：語料規模、膠囊 float／double 統計、退化膠囊現況、紀錄的重現方法。
- 語料中的輸入同時滿足現行驗證與 FF-9 預定的 `IsValid`；不滿足後者的輸入另列，不放進比對集合。

### 驗收點

- L1：CI 四平台通過。同機重跑兩次，紀錄逐位元相同。靈敏度：在 scratch 改動任一路徑的容差，統計會跟著改變（scratch 修改不提交）。
- L2：無（沒有 GUI 或執行期項目）。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、統計與紀錄的產生 |
| CI 四平台 L1 | 預定執行 |
| Linux lavapipe GPU | 預定執行（既有 GPU 測試照常；本批不新增 GPU 項目） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

統計以本機為準。跨平台的紀錄可能因 libm 不同而不同；CI 只驗測試通過與自洽，不比對跨平台紀錄。

### 建議檔位

high。要讀懂兩條演算法路徑與兩套容差，才能設計擦邊與容差邊界的情境。只新增測試，有 CI 驗證。

### 依賴

無。FF-9 依賴本批。

### Architecture Delta

無。只新增共通測試；資料是合成的，不依賴任何產品，不改公開介面。

### 完成條件

語料涵蓋全部公開查詢；紀錄可重現；統計寫進 HANDOFF；CI 四平台通過。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 為了取得紀錄必須公開內部函式或修改 Collision：停下回報。
- 紀錄在同機重跑時不穩定：停下，先定位原因，不以放寬比對掩蓋。

---

## FF-2 共通測試的計時假設

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ff2.zh-Hant.md)。

### 目標與範圍

依 D6，把共通測試中依賴主機計時精度或檔案系統精度的判定改為結構條件。

做：

- `tests/common/render/sdl_gpu/MeshUpdateSmoke.cpp:237-249`：現在 16 幀都必須 `Presented`。改為「N 幀內至少 K 幀 `Presented`，且最後呈現的內容與更新一致；`Skipped` 不判失敗」。先確認新條件在現行的阻塞取得下通過。
- `tests/common/ci/test_package_checks.py:75-80`：現在子程序須在 0.2 秒的 timeout 內印出啟動訊息。改為等待就緒訊號後才開始計時。
- `tests/common/core/asset/AssetWatcherTests.cpp:39-66`：現在 20 ms 後改寫同大小的檔案，依賴 mtime 精度（FAT 為 2 秒）。改為內容或大小的變化，或強制把 mtime 往前推。
- `tests/object_fps/package_tools/test_gpu_smoke.py:55-60`（未啟用產品，0.5 秒）只寫進遷移清單。

不做：

- 不依 OS 名稱分支。
- 不改 render 的取得語意（屬 IP-2）。
- 不改未啟用產品的測試。

### 交付

- 三個共通測試的新判定與突變測試。
- 遷移清單的 FF-2 節。

### 驗收點

- L1：
  - CI 四平台通過；MeshUpdateSmoke 在 CI 的 Linux lavapipe GPU 作業通過。
  - 突變：一幀都沒有 `Presented`、或內容未更新時，MeshUpdateSmoke 仍判失敗；子程序從未送出就緒訊號時，package checks 仍判失敗；watcher 不回報修改時，AssetWatcher 測試仍判失敗。
  - 改過的測試以 `ctest --repeat until-fail:20` 跑，沒有 flake。
- L2：本機 macOS Intel／Metal 執行 MeshUpdateSmoke（`gpu` 標籤）一次。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：L1 與 MeshUpdateSmoke 本機 GPU 執行 |
| CI 四平台 L1 | 預定執行（`tests/common/ci` 與 AssetWatcher 在四平台執行） |
| Linux lavapipe GPU | 預定執行（MeshUpdateSmoke 的新判定） |
| Windows D3D12 實機 | 未執行（沒有實機；MeshUpdateSmoke 在 D3D12 上的 `Skipped` 頻率未知） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

- MeshUpdateSmoke：high。新判定要和 `PresentStatus` 以及 IP-2 預定的 `Skipped` 語意一致，需要讀懂 render。
- package checks、AssetWatcher：medium。範圍明確，有 CI 驗證。

### 依賴

無前置批次。IP-2 依賴本批：IP-2 會讓 `Skipped` 變常見，MeshUpdateSmoke 的新語意必須先合併（合併順序的例外，屬真正的依賴）。

### Architecture Delta

無。只改共通測試的判定條件，不改公開介面與依賴。

### 完成條件

三處判定改完；突變測試與重複執行通過；遷移清單已更新。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 新的 MeshUpdateSmoke 判定在現行阻塞取得下不通過：停下回報，不調低 K 來通過。
- 某個平台需要依 OS 名稱分支才能通過：停下回報。

---

## FF-3 FNV-1a 收為一份

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ff3.zh-Hant.md)。

### 目標與範圍

做：

- 三份 64-bit FNV-1a 收為 `GYO::Base` 的一個公開 header（檔名在開始時決定）：
  - `engine/asset/include/engine/asset/detail/Hash.hpp:11-27`（`Detail` 命名空間，被 `AssetId.hpp`、`AssetType.hpp` 使用）。
  - `engine/input/src/InputActionMap.cpp:6-19`（空字串回傳 0，和標準 FNV 不同）。
  - `tools/ui_editor/src/FileService.cpp:21-28`。
- `InputActionMap` 的「空字串→0」語意留在呼叫端。
- 舊的 `Detail` 版刪除，不留轉送 header。

不做：

- 不改雜湊演算法或常數。
- 不把雜湊放進 asset 或 input（會新增 input→asset 等不自然的邊）。

### 交付

- Base 的公開 header 與逐位元測試。
- 三處改用。
- 依賴邊比對結果（只多出核准的一條）。
- 遷移清單的 FF-3 節。

### 驗收點

- L1：
  - CTest 全標籤、CI 四平台通過。
  - 逐位元：已知向量（含空字串、單一位元組、非 ASCII）在新舊實作相同；`InputActionMap` 的空字串仍為 0。
  - `AssetId`、`AssetType` 的雜湊值與 base 相同（以既有測試或新增 characterization 鎖住）。
  - 依賴邊比對：與 base 相比只新增 `gyo_input→GYO::Base`。
- L2：無。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、依賴邊比對 |
| CI 四平台 L1 | 預定執行 |
| Linux lavapipe GPU | 預定執行（既有 GPU 測試照常；本批不新增 GPU 項目） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

high。跨 base、asset、input、ui_editor 的整理，要確認依賴邊與 Ownership。逐位元測試與 CI 做外部驗證。

### 依賴

- Engine 內：無。
- 建議合併順序：`InputActionMap.cpp` 由本批與 IP-1 先完成者先合併，後者 rebase；不是依賴。
- 消費端條件：提出需求的消費端在協議升版批計算 arena 內容 digest 時使用本批的 FNV-1a，所以本批要在那一批之前合併。

### Architecture Delta

1. 需求：Math 基礎統一範圍外事項「FNV-1a 重複」（PLAN 第2節；實為三份）。
2. 問題：同一演算法三份實作，其中一份在 asset 的 `Detail` 命名空間，其他模組無法正當使用；input 的版本另有空字串特例。
3. 邊界：`GYO::Base` 新增一個公開 header；asset 的 `Detail` 版刪除。
4. 影響：`engine/base`、`engine/asset`（`AssetId.hpp`、`AssetType.hpp`）、`engine/input`、`tools/ui_editor`；未啟用產品預計不受影響（見遷移清單）。
5. 依賴方向：新增 `gyo_input→GYO::Base`。Base 是最底層、header-only、只依賴標準庫，沒有傳遞依賴；asset 與 ui_editor 已經經由 `GYO::Engine` 連結 Base，不新增邊。
6. Ownership：雜湊歸 Base；空字串的語意歸 `InputActionMap`。
7. 更小的替代：只加註解無法消除重複；放在 asset 會讓 input 依賴 asset，方向更差。

### 完成條件

三份收為一份；逐位元與依賴邊比對通過；遷移清單已更新。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 任一雜湊值改變：停下回報。
- 需要 Base 以外的新依賴邊：停下回報。

---

## FF-4 有限性檢查收斂

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ff4.zh-Hant.md)。

### 目標與範圍

做：

- `Render::Color`（`engine/render/include/render/RenderTypes.hpp:15`）的有限性檢查有 4 份，收斂為 render 內一份：
  - `engine/render/src/RenderQueue.cpp:11,15`
  - `engine/render/src/Renderer.cpp:79,83`
  - `engine/render/backend/sdl_gpu/src/SdlGpuRenderDevice.cpp:17,21`
  - `engine/render/model/src/ModelRenderer.cpp:19-22`（`Finite`）
- `UiColor`（`engine/ui/include/ui/UiTypes.hpp:74`）的檢查（`engine/ui/src/UiValidation.cpp:24`）是第 5 份，留在 Ui，不改程式，只記錄理由（與下列不合併型別的理由相同：`gyo_ui` 不依賴 render，放進 Math 違反健檢）。
- 依 D11⑦ 保留兩個型別，在文件記錄不合併的理由：
  - `gyo_ui` 只連結 `GYO::Engine` 與 `GYO::Math`（`engine/ui/CMakeLists.txt:13-16`），由 `gyo_ui_renderer` 橋接 Render；合併到 Render 會新增 ui→render 的邊。
  - 合併到 Math 違反健檢「沒有消費者前不要擴充 Math」。

不做：

- SDL 後端的 8-bit Color（`SdlRenderer.hpp:37`）：語意不同，不屬同構。
- 不改 Color 型別本身或任何渲染結果。

### 交付

- render 內單一的 Color 有限性檢查與四處改用。
- 不合併兩個型別、以及 `UiValidation` 的檢查留在 Ui 的理由（寫進 HANDOFF 與相關 header 註解）。
- 遷移清單的 FF-4 節。

### 驗收點

- L1：CTest 全標籤、CI 四平台通過；Render 與 Ui 的 characterization 逐位元不變；非有限 Color 的錯誤碼與訊息不變；依賴邊比對無新增。
- L2：提出需求的消費端與 ui_editor 各跑一次 GUI 冒煙（report-only）。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、L2 冒煙 |
| CI 四平台 L1 | 預定執行 |
| Linux lavapipe GPU | 預定執行（既有 `render.*` GPU 測試作回歸） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

high。跨 render、render backend、model renderer 與 ui，要確認依賴方向與每處的錯誤語意。characterization 與 CI 做外部驗證。

### 依賴

- Engine 內：無前置批次。
- 建議合併順序：render 依 FF-2→IP-2→FF-4→FF-7；本批建議排在 IP-2 之後合併（`SdlGpuRenderDevice.cpp` 兩批都會修改），不是依賴，後合併的一方 rebase。FF-7 依賴本批。

### Architecture Delta

1. 需求：Math 基礎統一範圍外事項「`Render::Color` 與 `UiColor` 同構；有限性檢查重複」（PLAN 第2節、B7；實為 5 份）。
2. 問題：同一個檢查在 render 內有 4 份，分散在三個 target。
3. 邊界：render 公開介面新增一個 Color 有限性函式（位置在開始時決定）；兩個型別的邊界不變。
4. 影響：`engine/render`、`engine/render/backend/sdl_gpu`、`engine/render/model`；ui 只記錄理由，不改程式。
5. 依賴方向：不變；不得產生 ui→render 的新邊。
6. Ownership：Color 有限性檢查在 render 只留一處；`UiColor` 的檢查歸 Ui。
7. 更小的替代：只加註解無法消除重複；合併型別會改變依賴方向（使用者已決定不合併）。

### 完成條件

render 內只剩一份 Color 有限性檢查；characterization 逐位元不變；理由已記錄。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 任何渲染結果或錯誤語意改變：停下回報。
- 收斂需要新的依賴邊：停下回報。

---

## FF-5 Ui 與 ui_editor 的重複、`item_step` 驗證

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ff5.zh-Hant.md)。

### 目標與範圍

做：

- ui_editor 改用 Engine Ui 已有的規則，必要時由 Ui 公開既有函式：
  - letterbox：`engine/ui/src/UiRuntime.cpp:305`（`MakeFit`）對 `tools/ui_editor/src/PreviewAdapter.cpp:416-423`。
  - viewport 內判定：`PreviewAdapter.cpp:278-281`。
  - 文字對齊：`engine/ui/src/UiRenderer.cpp:412-421` 對 `PreviewAdapter.cpp:179-187`。
- `UiRuntime` 的 Evaluate（`:360` 的 `EvaluateElements`、`:390`）與 Compose（`:513` 的 `ComposeElements`、`:631`）重複的 layout 走訪合併為一次，輸出不得改變。
- `item_step` 驗證依 D11⑨：
  - 規則：`UiValidation.cpp:382-404`（`FixedStepList`）拒絕非有限值；codec 先以 double 檢查有限性再轉 float（`UiDocumentCodec.cpp:145-147,187-188,624`），所以超出 float 範圍的有限 double（例如 1e39）也要拒絕。
  - 不升 `kUiSchemaVersion`（定義在 `engine/ui/include/ui/UiDocument.hpp:16`，寫出在 `UiSerialization.cpp:281`）。
  - 在 `docs/ui_toolchain.md` 的 JSON v1 契約註明新規則，含「超出 float 範圍的有限 double 也會被拒絕」。
  - 提交的共通測試只用合成 fixture；repo 內產品的 UI 資料以一次性 scratch 腳本比對，結果寫進 HANDOFF。

不做：

- 點擊判定（ui_editor 已用 `HitTestUiLayout`）。
- 輸入框元件或 Ui 新功能。
- 對其他 float 欄位一致檢查範圍（若開始時認為需要，另提給使用者決定）。

### 交付

- ui_editor 改用 Ui 規則；`UiRuntime` 單次走訪。
- `item_step` 驗證、錯誤位置、合成 fixture 測試。
- `docs/ui_toolchain.md` 的契約更新。
- scratch 比對結果（HANDOFF）；遷移清單的 FF-5 節。

### 驗收點

- L1：
  - 合成 fixture 的 layout 結果在改動前後逐位元相同。
  - NaN、Inf、1e39 被拒，錯誤帶位置；既有合成 fixture 全部能載入。
  - ui_editor 測試、CTest 全標籤、CI 四平台通過。
  - scratch 腳本：repo 內所有 UI 資料在新規則下仍可載入，layout 結果逐位元相同（結果記錄，不提交為共通測試）。
- L2：ui_editor 開檔、編輯、存檔各一次。
- L3（可選）：使用者確認 ui_editor 的版面與點擊不變。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、scratch 比對、ui_editor 的 L2 |
| CI 四平台 L1 | 預定執行 |
| Linux lavapipe GPU | 預定執行（既有 GPU 測試照常；ui_editor 用 SDL_Renderer，不在 GPU 作業） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

high：要讀懂 Ui 與 editor 的分工與走訪順序。`item_step` 契約部分 medium：範圍小，有測試與 scratch 比對驗證。

### 依賴

無前置批次（FF-4 不改 `UiValidation.cpp`，兩批沒有同檔的依賴）。FF-7 依賴本批。

### Architecture Delta

屬 Editor／Runtime Data Contract 變更（AGENTS §8）。

1. 需求：Math 基礎統一範圍外事項（B5）：ui_editor 與 Ui 的規則重複；`item_step` 沒有有限性驗證。
2. 問題：ui_editor 重寫了 Engine Ui 已有的 layout 規則，兩份會持續漂移；`item_step` 的 Validate 與序列化不一致（`{NaN, 1}` 能通過，序列化成 `null` 後讀不回）。
3. 邊界：gyo.ui Data Contract 的 Validation Rule；Engine Ui 可能公開既有函式。
4. 影響：`engine/ui`、`tools/ui_editor`、所有 gyo.ui 資料（含提出需求的消費端與未啟用產品的 UI 資料）、`docs/ui_toolchain.md`。
5. 依賴方向：維持 Editor→Engine；Runtime 不依賴 Editor。
6. Ownership：layout 規則歸 Engine Ui；ui_editor 只呼叫。
7. 更小的替代：保留兩份實作會持續漂移；只改文件不改驗證，NaN 仍能寫出讀不回的檔案。新規則會拒絕現在可載入的手寫資料（例如 1e39），所以要寫進契約。

Fitness：刪除 ui_editor 後，既有有效資料仍可由 Runtime 讀取。

### 完成條件

重複規則改用 Ui；走訪合併後輸出逐位元不變；契約文件與驗證一致；scratch 比對已記錄。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 任何既有資料在新規則下無法載入，或 layout 結果改變：停下回報。
- 需要升 `kUiSchemaVersion`：停下，請使用者重新決定。

---

## FF-6 共通層衛生與產品登錄

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ff6.zh-Hant.md)。

### 目標與範圍

做：

- characterization helper（`SameBits`、`UlpDistance`、`Opaque`）收進 tests/common 的支援層（既有 `tests/common/support`）。現況三份：
  - `tests/common/math/MathCharacterizationTests.cpp:630,635,642`
  - 提出需求的消費端的測試（程式路徑作為資料保留：`tests/object_fps_pvp/CharacterizationSupport.hpp:17-52`）
  - `tests/ui_editor/PreviewSrgbCharacterizationTests.cpp:48,73`
  產品與工具的測試改依賴共通層；共通測試不依賴產品測試。
- `services/gyo_gateway/README.md:8-9,13` 去除產品名，改寫成不具名的說明。
- `tests/common/ci/test_workflow_gates.py:349` 是確認舊 CMake 變數（例如 `GYO_BUILD_OBJECT_FPS_PREVIEW`）已移除的負向檢查；改為不具名的寫法，或註明保留理由。
- 依 D11⑧，`object_fps_preview` 在 `engine/config/tools.csv` 停用（現為 `enabled=true`、`default=false`；它的 `requires_apps.game=object_fps`，而 object_fps 在 `projects.csv` 為停用，選取時 `build/cmake/GyoTools.cmake:41` 會 FATAL_ERROR）。輸入、Math、Result 的遷移需求寫進遷移清單。

不做：

- 不把登錄資料搬出 `engine/config`（候選）。
- 不遷移未啟用產品本身。

### 交付

- 共通層的 helper 與三處改用。
- 共通層文件與守衛的產品名整理。
- `tools.csv` 的 preview 停用；遷移清單的 FF-6 節。

### 驗收點

- L1：
  - CTest 全標籤、CI 四平台通過。
  - grep：`tests/common` 與 `services/gyo_gateway` 沒有產品名（負向守衛若保留，必須有註解）。
  - Removability：在 scratch worktree 刪除提出需求的消費端（程式、資產、測試、驗收、文件與登錄列）後，configure 與共通測試通過。
  - 只選 ui_editor 配置一次，通過。
- L2：無。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、scratch 移除檢查 |
| CI 四平台 L1 | 預定執行 |
| Linux lavapipe GPU | 預定執行（既有 GPU 測試照常；本批不新增 GPU 項目） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

medium。範圍明確的小修、文件與登錄資料；有 CI 與移除檢查做外部驗證。

### 依賴

無。

### Architecture Delta

1. 需求：Math 基礎統一範圍外事項（B7）：helper 三份、共通層出現產品名、preview 登錄無效。
2. 問題：共通測試不能依賴產品測試，helper 卻散在三處；preview 是無效登錄（已啟用，但無法 link）。
3. 邊界：測試支援的 Ownership 從產品／工具測試移到 tests/common；Product Registration 資料變更（只改 preview 的 selection）。
4. 影響：`tests/common`、消費端的測試、`tests/ui_editor`、`engine/config/tools.csv`、`services/gyo_gateway/README.md`、`tests/common/ci/test_workflow_gates.py`、未啟用的 preview（遷移清單）。
5. 依賴方向：產品與工具測試→共通支援，方向正確；不新增反向邊。
6. Ownership：helper 歸共通層；preview 的登錄維持由 preview owner 的登錄列表示，只改 enabled。
7. 更小的替代：只加註解無法讓共通測試脫離產品；保留 preview 為啟用會留下無效登錄。

Fitness：無效登錄消失；刪除消費端後共通層仍成立。

### 完成條件

helper 只剩共通一份；共通層無產品名（或有註解的負向守衛）；preview 已停用並記錄；移除檢查通過。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 移除檢查失敗（共通層仍引用消費端）：停下回報，不在共通層加特例。
- helper 的行為在三處不一致（例如 `Opaque` 的 `noexcept` 或型別不同）而無法單純合併：停下說明差異。

---

## FF-7 include 路徑風格徹底統一

狀態：未開始。

### 目標與範圍

依 D10 徹底統一公開 include 的根目錄命名。

現況（2026-10-04 抽查，開始時重新盤點）：

- `engine/<m>/`：`engine/{asset,base,collision,input,io,math,runtime}/include`。
- `<m>/`：`engine/model/include`（`model/`）、`engine/render/include`（`render/`）、`engine/text/include`（`text/`）、`engine/ui/include`（`ui/`）。
- backend 與其他：`engine/input/backend/sdl`（`input/`）、`engine/render/backend/{sdl,sdl_gpu}`（`render/`）、`engine/model/backend/ufbx`（`model/`）、`engine/text/backend/sdl_ttf`（`text/`），另有 `engine/render/model/include`（`model_renderer/`）與 `engine/platform/sdl/include`（`platform/`）。

做：

- 盤點全部公開 include 根目錄與所有 `#include` 消費端。
- 決定統一方向（`engine/<m>/` 或 `<m>/`），開始時提給使用者確認。
- 一次搬完、所有消費端（Engine、`tools/ui_editor`、提出需求的消費端的程式、驗收與測試、`tests/common`）一次改完；不留別名或轉送 header。
- 未啟用產品不改，遷移需求寫進遷移清單（2026-10-04 粗略 grep 約 94 行 `<m>/` 風格的 include，開始時重新盤點）。

不做：

- 不改 namespace、target 名稱或檔案內容。
- 不順便整理其他結構。

### 交付

- 盤點表（模組、舊根目錄、新根目錄、消費端數）。
- 搬移與消費端修改；建置與依賴邊比對。
- 遷移清單的 FF-7 節。

### 驗收點

- L1：
  - CTest 全標籤、CI 四平台通過。
  - grep：舊根目錄的 include 在 active 程式碼中為 0。
  - 依賴邊比對與 base 相同（只移動路徑，不新增邊）。
  - 權威不變：同機兩樹比對 FF-1 語料紀錄逐位元相同；消費端存在時，其權威 digest 兩樹比對相同。
- L2：無（不改行為）；必要時消費端與 ui_editor 各一次啟動冒煙（report-only）。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、兩樹比對 |
| CI 四平台 L1 | 預定執行（Windows 的路徑大小寫與分隔符由 CI 確認） |
| Linux lavapipe GPU | 預定執行（既有 GPU 測試作回歸） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

high。大範圍掃描（盤點與消費端改寫的檢查）建議 ultracode，開始時說明並徵求同意（D8）。

### 依賴

IP-1、IP-2、FF-4、FF-5（都會修改公開 header 或其消費端，先合併以避免衝突）。不卡消費端的協議批。
FF-8 若先合併，其新子系統一併納入盤點；若 FF-8 後合併，它必須採用本批決定的風格。

### Architecture Delta

屬大規模 File Move 與 Public Interface 變更（AGENTS §3）。

1. 需求：Math 基礎統一範圍外事項「include 路徑風格不一致」（PLAN 第2節）；使用者決定徹底統一（D10）。
2. 問題：公開 include 根目錄有兩種命名，新模組無從依循；同一 repo 的消費端要記兩套規則。
3. 邊界：所有 `<m>/`（或 `engine/<m>/`）根目錄的公開 header 路徑。
4. 影響：Engine 全部模組與 backend、`tools/ui_editor`、提出需求的消費端（程式、驗收、測試）、`tests/common`；未啟用產品（遷移清單）。
5. 依賴方向：不變；只改路徑。
6. Ownership：不變；header 仍屬原模組。
7. 更小的替代：只記錄新程式碼規則（不改舊路徑）會讓兩種風格長期並存；使用者已選徹底統一。留別名會形成第二套名稱，違反「無別名」。

### 完成條件

只剩一種 include 風格；建置、依賴邊與兩樹比對通過；遷移清單已更新。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 盤點後的影響範圍明顯大於上述清單（例如需要改 target 名稱或 namespace）：停下重新規劃。
- 任何兩樹比對出現差異：停下回報。

---

## FF-8 GYOP 標頭 C++ 編解碼收進 Engine

狀態：完成（2026-10-05）。見 [dev_log](../../../dev_logs/2026_10_05_engine_ff8.zh-Hant.md)。

### 目標與範圍

依 D4，把 GYOP 24-byte 標頭的 C++ 編解碼收進 Engine。Engine 已擁有 Go 版（`services/gyo_gateway/framing/framing.go`），C++ 版只存在於提出需求的消費端（`Wire.hpp:11-40`），而且版本號與訊息種類都寫死在裡面。

標頭格式（big-endian）：`"GYOP"` @0、Version u16 @4、Type u16 @6、Session u64 @8、Sequence u32 @16、payload length u16 @20、Channel u16 @22（v1 只用 channel 0，必須為 0；消費端 C++ 版目前當成 reserved）。

做：

- 新增最小 Engine 子系統（例如 `engine/net`，單一 target），只含標頭編解碼。
- 版本與訊息種類由呼叫端提供。解碼只檢查傳輸層條件（長度、magic、Channel @22 為 0、length 與實際大小一致、不超過 1200 bytes）；版本是否接受、Type 範圍由呼叫端檢查。
- 1200 bytes 是 Engine 的傳輸契約，與 Go 的 `framing.go` `MaxDatagram` 一致；產品可以設更小的上限，但不能放寬。
- @22 在 Engine C++ API 依 Go 命名為 `Channel`（v1 只用 channel 0），並在契約註解記錄。
- C++ 與 Go 共用 tests/common 的合成 golden 向量。
- 消費端的 `Wire.hpp` 改用它；訊息種類、Type 範圍與版本接受政策留在產品。位元組逐一相同，版本仍為消費端目前的值。

不做：

- 不改位元組格式或版本號。
- 不搬訊息種類、payload 或 TCP frame 政策。
- 不新增 Go 側功能。

### 交付

- Engine 子系統（header、實作、CMake target）與單元測試。
- tests/common 的合成 golden 向量；C++ 與 Go 各自讀取的測試。
- 消費端改用（同 PR）。
- 拒絕條件對照表：改動前後，消費端的組合解碼拒絕集合相同。
- 遷移清單的 FF-8 節。

### 驗收點

- L1：
  - C++ 與 Go 互相編解碼合成向量，逐位元相同。
  - 消費端 7 種訊息的編碼和 base 的 golden 逐位元相同。
  - 拒絕條件等價：長度、magic、@22、length 不一致、超過 1200 bytes 由 Engine 解碼拒絕；版本、Type 範圍由消費端（呼叫端）拒絕。以表格測試證明消費端組合後的整體行為不變。
  - CTest、Go unit／race、CI 四平台通過。
  - 權威不變：消費端的權威 digest 兩樹比對相同（消費端存在時）。
- L2：消費端的連線矩陣單案冒煙（report-only），在本批 base commit 上以凍結工具量 before／after。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、L2 冒煙 |
| CI 四平台 L1 | 預定執行（C++ 與 Go 測試） |
| Linux lavapipe GPU | 預定執行（既有 GPU 測試照常；本批不新增 GPU 項目） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

high：跨 Engine、消費端與 Go 的搬移，有 golden 驗證。解碼拒絕條件局部 xhigh：拒絕條件若悄悄改變，會成為協議漏洞。

### 依賴

- Engine 內：無。
- 消費端條件：排在消費端的協議升版批之前（該批由消費端經本批的 framing 傳入新版本號）；消費端的權威 digest 閘門要先存在，用來證明位元組與行為不變。
- include 風格：見 FF-7。

### Architecture Delta

新增 Subsystem（AGENTS §3）。

1. 需求：健檢候選「GYOP 傳輸框架的 C++ 版本」；時機條件「開始做新協議版本時」已成立；使用者決定納入（D4）。
2. 問題：同一份 Engine 傳輸契約，C++ 實作只存在於產品內，並寫死版本號；Go 與 C++ 的拒絕條件沒有共同的測試向量。
3. 邊界：新增 Engine 子系統（例如 `engine/net`，單一 CMake target）；不是新的 Top-level Directory。
4. 影響：Engine（新 target）、提出需求的消費端（`Wire.hpp`）、`services/gyo_gateway` 的測試（共用向量）、tests/common。
5. 依賴方向：新增「消費端→Engine net」，方向正確；Engine net 只依賴標準庫（與 Base）；Engine 不依賴任何產品。
6. Ownership：標頭編解碼與 1200 bytes 上限（傳輸契約）歸 Engine；訊息種類、Type 範圍、版本接受政策、payload 與更小的上限歸產品。
7. 更小的替代：目前只有一個 C++ 消費端，維持現狀也可行；使用者已決定收進 Engine，理由是傳輸契約本來就歸 Engine，且協議升版時要把版本號從標頭實作中拿掉。

Fitness：刪除消費端後，Engine net 與其測試（合成向量）仍成立。

### 完成條件

Engine 子系統與共用向量完成；消費端改用後位元組與拒絕集合不變；兩樹比對相同。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 任何位元組或拒絕條件改變：停下回報。
- 需要把 Type 範圍或版本政策放進 Engine 才能完成：停下回報。

---

## FF-9 Collision 統一與公開合法性檢查

狀態：未開始。

### 目標與範圍

依 D3 徹底統一 Collision。這是本計畫唯一會改變消費端權威判定的批次。

做（四個部分）：

1. **全部公開查詢共用一套 double 實作與單一容差**，含 `RaycastAabb`。
   - 現況：float 路徑在 `Collision.cpp`（`kEpsilon 1e-6f`，`:14`），double 路徑在 `CapsuleQueries.cpp`（`kTolerance 1e-7`，`:16`）。
   - 兩個容差擇一並記錄理由；刪除 float 演算法。
   - 公開簽名與 float 回傳型別不變。
2. **`VerticalCapsule` 多載只是薄包裝**：`RaycastCapsule`、`SweepSphereAgainstCapsule` 的 `VerticalCapsule` 版轉換成 `Math::Capsule`（`ToCapsule`）後呼叫 double 版。
3. **公開合法性檢查**：`IsValid(VerticalCapsule)`、`IsValid(Aabb)`、射線輸入檢查。
   - Engine 內部的 assert 改用它們。
   - 統一 AABB 的規則並記錄：Engine 兩份驗證（`Collision.cpp:169-172`、`CapsuleQueries.cpp:27-30`）只拒絕 `min>max`；消費端的驗證拒絕 `min>=max`。
   - 消費端的驗證改用公開函式（屬消費端的同 PR 部分）；出生點數量、牆數這類遊戲規則仍留在產品。
4. **退化膠囊一併拒絕**。
   - 現況：兩份驗證（`Collision.cpp:29-34` 以 float 算 `radius*2.0f`；`CapsuleQueries.cpp:17-21` 以 double 算 `2.0*radius`）都接受 `height == 2r` 與極小半徑。此時 `segmentTop < segmentBottom` 可能成立，`Collision.cpp:109` 的 `Math::Clamp` 前置條件在**所有建置**都會 assert 中止。
   - 新規則由 `IsValid(VerticalCapsule)` 定義，兩份驗證收為一份。

不做：

- 不改公開簽名、回傳型別或 Contact 結構。
- 不為比較而公開內部函式。
- 不改 Math。

### 事前宣告（開始時、實作前寫進 HANDOFF）

- 逐一列出會變動的公開函式；未列出的函式在兩樹比對中必須逐位元不變。
- FF-1 語料的命中翻轉上限與距離 ULP 上限（依 FF-1 的膠囊統計設定）。
- 容差與 AABB 規則的選擇及理由。選 `1e-7` 以外的容差時，移動用的 Overlap／Sweep 與 Client 預測的變化也要列入宣告。
- 消費端同時宣告：其權威 digest 情境中預期會變化的集合。

### 事後量測

- 同機兩樹（base／branch）比對 FF-1 語料：逐查詢的命中翻轉數與 ULP 分布。
- 消費端：權威 digest 第一個分歧的 Tick 與原因分類。
- 任一項超出事前宣告就停下回報，不事後放寬。

### 與消費端同一 PR 的分工

本批與提出需求的消費端的權威變更批在同一個 PR（D12：本文件不連結消費端的文件，分工以下表的文字為準）。理由：Engine 的數值改變會直接改變消費端的權威判定；分開合併會讓 master 上的消費端權威基準與 Engine 不一致。

| 部分 | 本批（Engine） | 消費端 |
|---|---|---|
| 程式 | Collision 的 ①–④；Engine 內部 assert 改用 `IsValid` | 自己的 arena 驗證與射擊查詢改用公開 `IsValid` |
| 事前宣告 | 變動的公開函式、FF-1 翻轉上限、容差與 AABB 規則 | 權威 digest 預期變化的情境集合 |
| 事後量測 | FF-1 語料兩樹比對 | 權威 digest 第一個分歧 Tick 與原因分類 |
| 基準更新 | `tests/common/collision` 期望值（附理由） | 權威 golden 只在此 PR 更新一次，舊基準保留在證據 |
| 紀錄 | 本夾 HANDOFF、`Collision.hpp` 的數值語意註解 | 消費端的協議契約文件 |

消費端的分析（依現況程式碼）：消費端已編譯的程式中，float 路徑只被射擊查詢使用（`RaycastAabb` 判牆、`VerticalCapsule` 版 `RaycastCapsule` 判玩家），而射擊查詢只在伺服器端的 Match 呼叫；移動與 Client 預測只走 double 的 Overlap／Sweep（`kTolerance 1e-7`）。所以影響取決於容差選擇：
- 選 `1e-7`（現行移動用的容差）：預期只改變 Match 的射擊判定（含牆面遮擋），移動與 Client 預測不變。
- 選其他容差（例如 `1e-6f`）：移動與 Client 預測也會變，變化必須在事前宣告中列出。

這是待驗證的預期，由事後量測確認。

### 消費端條件

消費端必須先完成協議升版（新版本號合併）。理由：消費端舊版本的穩定基線，其權威行為不得在舊版本號之下悄悄改變。若消費端在本批之前被移除，這個條件隨之消失，本批只以 Engine 側的驗收成立。

### 交付

- Collision 的 ①–④ 與 `IsValid` 公開 API；`Collision.hpp` 的數值語意註解。
- 事前宣告與事後量測紀錄（HANDOFF）。
- `tests/common/collision` 的更新與 `IsValid`、退化膠囊拒絕的測試。
- 遷移清單的 FF-9 節。

### 驗收點

- L1：
  - `tests/common/collision` 全部通過；更新的期望值逐一附理由。
  - FF-1 語料兩樹比對在事前宣告範圍內；未宣告的函式逐位元不變。
  - 靈敏度：在 scratch 改動容差，比對會偵測到（scratch 修改不提交）。
  - `IsValid` 的邊界測試（`min==max`、退化膠囊、零方向射線、非有限值）。
  - CI 四平台通過。
- L2：消費端的連線矩陣與動作短測，在本 PR 的 base commit 上以凍結工具量 before／after；命中率與拒絕分類不得出現未解釋的變化（消費端的分工）。
- L3：無。

### 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行：本機建置、L1、兩樹比對、消費端 L2 |
| CI 四平台 L1 | 預定執行（只做自洽與不依賴 libm 的子集；跨平台不比對紀錄） |
| Linux lavapipe GPU | 預定執行（既有 GPU 測試照常；本批不新增 GPU 項目） |
| Windows D3D12 實機 | 未執行（沒有實機） |
| Linux 實機 | 未執行（沒有實機） |
| macOS arm64 實機 | 未執行（沒有實機） |

### 建議檔位

主體 high。ultracode（證據整理＋1 次對抗式檢查）依 D8 在開始時說明並徵求同意。

局部 xhigh：容差選擇、`IsValid` 的規則與邊界、兩樹比對的差異歸因。這些是決定性與權威判定的改變，錯了不會報錯，需要用證據證明「只改了預期的部分」。機械性的改名、薄包裝改寫與常數改值維持 high。高於主對話的檔位在開始時說明並徵求同意（D8）。

### 依賴

- FF-1（語料與統計）。
- 消費端條件：見上。

### Architecture Delta

1. 需求：Math 基礎統一範圍外事項「Collision 內部 float 與 double 兩套演算法並存」（PLAN 第2節）、「退化膠囊」（純量統一後續）；健檢候選「公開 Collision 的合法性檢查」；使用者決定徹底統一（D3）。
2. 問題：同一形狀的查詢有兩套精度與容差不同的演算法；驗證只在內部 assert，消費端各自重寫且規則已不一致（`min>=max` 對 `min>max`）；退化膠囊會通過驗證、之後在所有建置中止；未啟用產品原本依賴 catch `std::invalid_argument` 做內容驗證，現在改為 assert 後沒有任何事前檢查的手段。
3. 邊界：Engine Collision 的數值結果契約（簽名不變）；公開 API 新增 `IsValid`；合法輸入的定義收緊（退化膠囊、AABB 規則）。
4. 影響：Collision 的所有消費端：提出需求的消費端的 Match 射擊判定與 arena 驗證（容差不是 1e-7 時，也影響移動與 Client 預測，須事前宣告）、未啟用產品（遷移清單）、`tests/common/collision`。
5. 依賴方向：不變。
6. Ownership：合法性規則歸 Collision 單一定義；遊戲規則（數量、配置）仍歸產品。
7. 更小的替代：只統一膠囊、保留 `RaycastAabb` 的 float 版，會留下同類的坑；只公開 `IsValid` 不統一演算法，結果仍不一致。使用者已選徹底方案。

### 完成條件

只剩一套演算法與一套容差；`IsValid` 公開且 Engine 內部改用；退化膠囊被拒；事後量測在宣告範圍內；消費端同 PR 完成其部分；遷移清單已更新。更新 README、HANDOFF、dev_log 後停止。

### 停止條件

- 任一事後量測超出事前宣告：停下回報，不事後放寬。
- 出現未宣告的變化，包括移動與 Client 預測（選 `1e-7` 時移動不在宣告內，任何移動變化都算未宣告）：停下回報。
- 消費端尚未完成協議升版：不開始。

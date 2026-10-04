# 第 08 批：刪除未編譯 29 檔與孤兒資產

狀態：未開始。依賴第 01 批完成。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md)。

## 目標與範圍

`apps/object_fps_pvp/sources.cmake` 的 `APP_DOMAIN_SOURCES`（:3-29）與 `APP_SUPPORT_SOURCES`（:41-52）
沒有被任何 target 使用（`apps/object_fps_pvp/CMakeLists.txt` 只用 `PVP_DOMAIN_SOURCES` 與明列的來源）。
依 D5，刪除其中未編譯的 29 檔，並清理只被它們使用的 header、資產與 catalog 條目。

兩個變數共 35 列。其中 6 檔仍被編譯，**不刪**：

- `match_domain`（`PVP_DOMAIN_SOURCES`）：`src/Collision/CharacterCollision.cpp`、`src/Gameplay/Player/PlanarMovement.cpp`。
- `app_support`（`CMakeLists.txt:41-43`）：`src/App/CharacterPresentationDefinition.cpp`、`src/App/WeaponViewModel.cpp`、
  `src/App/WeaponPresentationDefinition.cpp`、`src/App/AnimationSetDefinition.cpp`。

刪除的 29 檔（皆在 `apps/object_fps_pvp/`）：

- `src/App/`：`ObjectFpsUi.cpp`、`EnemyPresentation.cpp`、`EnemyPresentationDefinition.cpp`、`ObjectFpsApplication.cpp`、
  `CampaignContentLoader.cpp`、`ObjectFpsPresentation.cpp`、`ObjectFpsRuntimeClient.cpp`。
- `src/Collision/`：`GridWorldCollision.cpp`、`CombatCollision.cpp`、`GridCollision.cpp`。
- `src/Data/`：`Csv.cpp`、`GameData.cpp`。
- `src/Game/`：`CampaignRunState.cpp`、`CampaignContent.cpp`、`GameFlow.cpp`、`GameSession.cpp`。
- `src/Gameplay/`：`Combat/ProjectileSystem.cpp`、`Enemy/EnemyRig.cpp`、`Enemy/EnemySpawnDirector.cpp`、`Enemy/EnemySystem.cpp`、
  `Player/Player.cpp`、`Player/PlayerCombatState.cpp`、`Player/PlayerController.cpp`、`Player/PlayerSettings.cpp`、
  `Weapon/WeaponController.cpp`。
- `src/Rendering/MapGeometryGenerator.cpp`。
- `src/World/`：`GridMap.cpp`、`GridMapLoader.cpp`、`World.cpp`。

做：

- 刪除上列 29 檔與 `sources.cmake` 的兩個變數（`PVP_DOMAIN_SOURCES` 保留）。
- 刪除沒有其他引用的 header（`apps/object_fps_pvp/include/RetroFPS/` 下），開始時以 include 關係盤點；
  已編譯的 6 檔與 `src/Pvp/` 仍引用的 header 保留。
- 孤兒內容盤點，逐項刪除或附理由保留：
  - `assets/object_fps_pvp/asset_catalog.json:9-17`：enemy.melee／ranged（animations、character、enemy）與
    data.enemies／weapons／levels。
  - 同檔其他只被未編譯側引用的條目也一併盤點，例如 `ui.screens`（:18，引用者 `ObjectFpsRuntimeClient.hpp:29`）
    與 `map.room_01..03`（:20-22）。
  - 對應的資產檔（`characters/enemies/`、`data/*.csv`、`maps/`、`ui/screens.json` 等）。
- `assets/object_fps_pvp/content.json` 保留、不修改，不是刪除候選：它是資產根目錄的必要清單
  （`GyoContent.cmake:12-15` 缺少時 FATAL_ERROR；`package_contract.py:225`、`assemble_runtime.py:35` 要求它存在），
  內容只有 `asset_catalog.json` 與 builtin shader bundle。
- 刪除後不留空目錄。
- 第10項的兩列隨之結案：GroundPoint 有限性檢查三份與格子線段檢查兩份、
  `EnemyPresentationDefinition` 與 `EnemySystem` 攻擊時間容差不一致（1e-5／1e-6）。

不做：

- 不修改無法編譯的程式碼（只刪除）。
- 不動已編譯的 6 檔與 `src/Pvp/`。
- 不改 `apps/object_fps`、`apps/object_fps_v2` 的同名檔案（另一個 owner）。
- 不改寫歷史紀錄：math-foundation HANDOFF 在第 01 批只改了指向 Engine 計畫的連結；未編譯清單的歷史內容不改寫。
  健檢與 `docs/dev_logs/` 中對這些檔案的描述也保留原樣。
- 不改共通 Build、Packaging、Workflow。

## 交付

- 刪除與清理本身（產品內部變更）。
- 孤兒內容盤點表：每個條目的引用者、處置（刪除／保留）與保留理由。
- 產品自有的 L1 檢查：catalog 沒有失效或孤兒條目。
- 本批 dev_log（`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch08.zh-Hant.md`），README／HANDOFF 更新（第10項兩列標結案）。

## 驗收點

- L1 自動：
  - 新增 catalog 檢查：每個條目的 `path` 存在；每個 id 有已編譯來源或產品資料的引用，或在盤點表列為保留並附理由。
    檢查放在 `tests/object_fps_pvp`（產品自有），只在選擇 pvp 時組入，不放共通層。
  - CTest 全標籤通過；CI 四平台通過。
  - grep 確認沒有殘留參照（刪除的檔名、header、catalog id）；不計歷史文件。
  - 沒有 owner 不明的檔案；沒有空目錄。
  - 權威不變：本批不改任何已編譯來源。證明方式是 base／branch 的已編譯 target 來源清單與內容相同
    （diff 只含刪除的未編譯檔、header、`sources.cmake` 的兩個變數與資產）。若第 03 批已合併，
    附帶以其 runner 做同機兩樹 digest 比對。
- L2 實機：不適用（無行為變更）。
- L3 人工：不適用。

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（本機建置與 CTest；無實機操作項目） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 未執行：pvp 沒有宣告 GPU 檢查（checks.json 的 gpu 為 false）；CI toolchain 列的共通 render.* 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行（無實機項目；v6 實機只有 macOS Intel，D11⑩） |
| Linux 實機 | 未執行（同上） |
| macOS arm64 實機 | 未執行（同上） |

## 建議檔位

建議 medium。範圍明確的刪除與盤點；有建置、CTest 與 CI 四平台做外部驗證。
header 與資產的引用盤點要逐項確認，不降到 low。依 D8，開始時說明檔位。

## 依賴與合併順序

- 依賴第 01 批。與其他批次沒有程式碼依賴。
- 不在 `PvpApplication.cpp` 的建議合併順序上；不碰 `src/Pvp/`。合併順序只是建議，衝突時由後合併的一方 rebase。

## Architecture Delta

無。刪除的是未被任何 target 使用的產品內部檔案與只被它們使用的資產；
不改 Build Graph、Dependency Direction、Ownership 或 Data Contract，也不改共通層。
這是 AGENTS §7 的完全刪除整理，不是 Boundary 變更。

## 執行規則（沿用 v5）

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

- 只做使用者指定的本批；一批一個 PR，commit 與 PR 用日語。
- 開始、里程碑、停止時更新 README／HANDOFF／dev_log。
- 失敗跑次保留，不覆寫；長測另外授權，本批不跑長測。

## 完成與停止

完成條件：

- 29 檔、兩個變數、無引用的 header 已刪除；孤兒盤點表每列都有處置。
- catalog 檢查與 CI 四平台通過；grep 無殘留參照。
- 更新 README／HANDOFF／dev_log 後停止，不自動開始下一批。

停止條件：

- 已編譯來源、`src/Pvp/` 或 `tests/object_fps_pvp` 引用到預定刪除的 header 或資產：停下回報，不改已編譯程式碼。
- 刪除需要修改共通 Build、Packaging、Workflow 或其他 owner 的檔案：停下回報（屬 Removability 問題，另行規劃）。
- 某資產無法判定是否孤兒：保留並在盤點表寫明，不猜測刪除。

# 合併前核准：建置層認 `games/`（`v8-arch-gyo2-build-games-dir`）

- PR：[yojinn-io/GYO-Engine#294](https://github.com/yojinn-io/GYO-Engine/pull/294)（draft，到 `gyo2`）
- 事前宣告：[`build-games-dir-declaration.zh-Hant.md`](build-games-dir-declaration.zh-Hant.md)（修訂 2，動工前核准：使用者「照建議，選 B，守衛核准」）
- dev_log：[`2026_11_23_gyo2_build_games_dir.zh-Hant.md`](../../../dev_logs/2026_11_23_gyo2_build_games_dir.zh-Hant.md)
- 證據：主 checkout（`/Users/karasu/Code/Source/GYO-Engine`）的 `build/evidence/gyo2-build-games-dir/`
- 本頁的所有檢查都是 Opus／high，和作者同一個模型（Fable 用量不足，使用者經主會話指示「暫時用Opus」）；同一模型的盲點可能重疊。

## 1. 做完的與驗證

照宣告方案 B 做完：建置層讀 `games/<遊戲>/`；建置與 CI 的 app 改叫 game（`games.csv`、`GYO_GAMES`、`gyo_game_*`、KIND game、打包清單 4、工具描述 2、`game_registry.py`、`game_copy_integration.py`、身分標頭改成 `GYO_GAME_*` 巨集）；共用的 CSV 解析拆到 `GyoRegistryCsv.cmake`；五處守衛的掃描根；複製整合的 mirror 修正；LIM-9。

| 驗證 | 結果 |
|---|---|
| 本機 ctest（macOS Intel，test preset） | 改動前 196/196；改動後 196/196（最後一次在第 4 節的修正之後，見第 6 節） |
| L1 三平台 | run 45117108055（f52fbd2a）全綠；補測試後的 run 45121011173（11d34cf1）與本頁之後的 head 見第 6 節 |
| 複製整合 | Red（`Registry does not exist`）→ Green（兩個合成遊戲從 `games/` 建出、執行） |
| 臨時遊戲 `games/probe` 實測 | 正例、負例（`OS definition guard`）、對照（拿掉 `games` 根後負例通過） |
| 突變（D184） | 相關的既有突變 107/107（5 個 win32 限定 skip）；新加 9 個 9/9 killed |

## 2. 守衛判定的改動（§14.4；動工前已核准方向）

| 守衛 | 1 守的性質 | 2 緩的面 | 3 緊的面 | 4 緩的面誰守 | 5 證據 |
|---|---|---|---|---|---|
| naming `SCAN_ROOTS` | 產品的 C++ 不打開 `GYO::` 的根（N17） | `apps/` 底下的檔不再掃 | `games/` 底下開始掃 | 改後建置不讀 `apps/`，沒有對象 | 自測釘新值；`nm-scan-roots-examples` killed；驗證者在 base／HEAD 副本實測 |
| os `ROOTS`／`RUNTIME_ROOTS`／`GYO_OS_DEFINITION_ROOTS` | 平台分支與檔案 I/O 只在 OS 層（DI 1、DI 2） | 同上 | 同上 | 同上 | `os-g11`、`os-g15` killed；定義根靠 probe 實測（LIM-9） |
| `ProcessCodePageCheck`／`GyoCodePage` | 每個執行檔帶 UTF-8 manifest（DI 11） | 同上 | 同上 | 同上 | 新突變 `gdir-code-page-*` 2 個 killed |
| workflow gates | 共通 CI 定義不寫產品名；L1 不跑打包與複製整合 | `apps/` 底下的目錄名不再當產品名；**另有一個窄的緩面**：CI 文字寫 `apps/<已登記的遊戲或工具名>` 不再被抓（驗證者找到，宣告的第 4 欄說滿了） | `games/` 底下開始取名、比對；這條從 skip 變成實際執行 | `apps/` 已不是有效路徑，那種寫法是死參照，由 review 看 | 陽性對照突變 `gdir-gates-positive-*` 2 個 killed；`:701` 等價（驗證者實測） |
| `kernel_guard` `REGISTRIES` | 登記的產品名不進 Kernel（K1） | 不再讀 `projects.csv` | 無 | `projects.csv` 改名後不存在 | **更正**：宣告引的 `ks-k1-registry-ignored` 和路徑無關；守住路徑的是 fail-closed（指向不存在的檔時實樹與自測都失敗，驗證者實測） |

**要核准的**：workflow gates 那個窄的緩面（接受，理由如上），以及 `kernel_guard` 第 5 欄的更正。

## 3. 和宣告的偏差

- 加了宣告沒列的測試與突變（敵對式檢查找到的 6 類，dev_log「檢查」一節）：遊戲與工具同名的 project 模式案例、身分巨集的整行比對、遊戲套件的 owner（Python 與 CMake）、workflow gates 的陽性對照、mirror 每個分支、game 列的 `kind`。突變共 9 個（一批的上限 10）。
- `ProjectComposition`、`ToolRegistry`、`ProductPackaging` 三個測試加 `WORK_ROOT`（突變執行器從 repo 根跑時不在 repo 根留目錄；預設照舊）。
- 本批突變的前綴用 `gdir-`（`bg-` 已在 os 守衛清單指別的）。
- `docs/tool_projects.md`（工具描述的 Data Contract）、`docs/architecture.md:278-280`（斷掉的連結）跟著改。

## 4. 設計審查（Opus／high，`design-review.md`）

結論：可以合併，沒有推翻設計前提的問題。合併前要修的機械式項目已修：`release_channels.py` 的 `app_pairs`（改名表有、漏做）、`creating_games.md` 的 `--kind app` 與 `GYO_CURRENT_APP`（照抄會失敗）、同詞異義表 Registry 列與 `target/README.md` 的舊名、`GyoRegistryCsv.cmake` 的檔頭說法與區域變數、「比照 Cargo／go.work」取了什麼（`docs/architecture/README.md`）。

## 5. 交使用者決定的

1. **合併**（只認明確的「合併」）。
2. **第 2 節**：workflow gates 的窄緩面接受與否；`kernel_guard` 證據的更正。
3. **`game_registry.py` 的名字**（設計審查 B1）：檔裡除了遊戲的匯出，還有 CI 各平台的 runner／toolchain 表、工具的匯出，`ci_scope.py`、`release_pipeline.py`、`archive_package.py` 都從它匯入非遊戲的東西。CMake 那一側本批正是為了同一個理由拆出中性的 `GyoRegistryCsv.cmake`，Python 這一側沒照做，兩邊不一致。選項：
   - (a) 照舊（改名表核准的名字）。
   - (b) 改成中性的 `product_registry.py`（建議；現在約 7 個匯入點，最便宜，照「改名排前面」）。
   - (c) 拆成 CI 平台表一個檔、遊戲與工具的匯出一個檔。
   選 (b) 或 (c) 時本批改，改完重跑 `build.ci` 與相關突變、L1。
4. **後續要不要排**（本批不改；選要的話主會話記進批次表〈後續小事〉）：
   - 建置層內部「遊戲目錄規則 `games/<名>`」手寫 4 處（`GyoBuild.cmake` 兩個迴圈、`GyoGames.cmake:38`、`GyoGameRegistry.cmake:76`），可以收成一個出口（不影響守衛的刻意獨立）。
   - KIND 的集合 `{game, toolchain}` 列在三個讀者（`GyoRuntimeDeployment.cmake`、`package_contract.py`、`assemble_runtime.py`）；Python 兩處可以共用一個常數。
   - 只寫不讀的值：`GYO_GAME_ASSET_ROOT`、`GYO_GAME_MAIN_TARGET`、`GYO_GAME_CONTENT_STAGE_TARGET`、CI 輸出 `has_games`：先問該不該存在。
   - `MirrorTests` 的 carried 清單逐一釘住 mirror 的分支，接近釘實作；可以只留 Red 的回歸與「不帶產品登記、tests、CI」兩件。
   - `ProjectCompositionTests` 在預設產生器下 `#error` 那一案的增量建置不會失敗（測試對環境敏感，§14.1）。
   - 測試的合成資料還叫 `sample_app`、`other_app`。
   - README 三語版 `engine/` 列寫著 `config/` 的登記檔（master 留下的錯）、`docs/architecture.md` 其他寫 `apps/`、`GYO_APPS` 的段落：交圖書管理員。

## 6. 最後的狀態

（推送本頁後補：最後的 head、本機 ctest、L1 run。）

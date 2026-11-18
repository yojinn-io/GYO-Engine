# 事前宣告：編排層的骨架（`v8-arch-gyo2-orchestration-skeleton`）

更新：2026-11-18（修訂 6）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：等第一段核准（方向與設計）；核准之前不改程式。**

經過：修訂 0（f9f74684）送 Fable／high 敵對式檢查，必改 10、建議改 16、小問題 8（報告 `declaration-r0-adversarial-check.md`）。修訂 1 照它改，並收主會話轉來的使用者決定六則（scenario test、守衛延後、「機制穩定」的判準、scenario 失敗的分類、對照開源引擎的兩則）與主會話照 L17 的兩則判定（scenario 涵蓋第 1 批的機制、`TieBreaks` 那一步延到第 4 批）。處理對照在第 15 節。修訂 1 沒有再送檢查。修訂 2 收使用者對第 14 節第 3 件的決定（重跑登記），修訂 3 收第 2 件的決定（不重跑生命週期），修訂 4 收第 4 件的決定（替換的先後作者選），修訂 5 收第 4 件的補充（存檔版本 Module，不在本批），修訂 6 收第 1 件的決定（帶過去、做成機制），都只改它牽連的地方。

性質：實作批的事前宣告（AGENTS §3 的 Architecture Delta、§2 的基盤 Architecture、§14.4 的判定變更、Design Invariants 的變更）。實作段第 2 批。

根據：
- [批次表](rebuild-batch-plan.zh-Hant.md)第二段第 2 條（文件分支 `claude/gyo2-time-design` f65f1e41）。
- [統合宣告](kernel-app-boundary-declaration.zh-Hant.md)修訂 38（已核准）：關聯表 L5、L6、L15、L16、L17、L19、L20、L21；第 3 節；第 7.2 節 J-19、J-24、J-58 列；第 8 節本批那一列；第 9.1 節 DI-KS 15、22；第 9.2 節 O-1～O-5；第 15.2 節。
- [讀檔宣告](restore-entry-declaration.zh-Hant.md)修訂 11（已核准）：第 4 節、5.1、5.5、5.7、5.8、第 8 節、第 13 節、第 15 節確認清單。
- [第 1 批的宣告](kernel-subtract-structure-declaration.zh-Hant.md)（已合併，PR #242）：3.2 規則 1～10、3.3、第 11 節留給本批的兩件。
- [權威份](authority-module-declaration.zh-Hant.md) 4.4～4.7；[回滾份](rollback-module-declaration.zh-Hant.md)第 148 行的同一時機。
- HANDOFF 決定表（文件分支 f65f1e41）：`v8-arch-gyo2-orchestration-layer-name`、`-game-init-list`、`-two-layers-like-bevy`、`-init-values-like-bevy`、`-restore-entry-new-world-default`、`-restore-entry-ids-author-choice`、`-save-unaware-means-format-only`、`-module-plugin-registers`、`-module-plugin-owner-definition`、`-module-deps-in-registers`、`-products-pick-modules`、`-dynamic-insert-per-world`、`-thread-binding-by-runner`、`-kernel-subtract-structure-*`、`-immediate-spawn-returns-error`、`-failure-is-not-absence`、`-batch2-after-batch1-merged`；**2026-11-18 新加、指名本批照做的**：`-scenario-tests`、`-guards-after-mechanism-stable`、`-scenario-failure-by-contract`、`-scenario-cross-engine-check`。
- 文件分支的 AGENTS §3「ガードを立てる時期」（0f400dd3；還沒進 `gyo2`，本批照它做）。
- [error-handling.md](../../error-handling.md) 規則 1 第 3 步（22f3f392）；[共用規則頁](../../../cross-component-rules.md) CCR-3、CCR-4、CCR-5。
- 程式：`origin/gyo2` 0f1f2d21。

**標記**：寫「卡片照 L17 判」的，是任務卡照命名規範、別家原始碼與已定的原則自己判的，不是使用者的話（使用者 2026-11-17：「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」）。交使用者的只有第 14 節。

證據（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-orchestration-skeleton/`，git 不追蹤）：`inventory-startup-plugin.md`（開局與登記單位的關鍵字盤點，會多算）；`declaration-r0-adversarial-check.md`（修訂 0 的檢查）；`scenario-cross-engine.md`（第 8 節步驟表的別家對照與出處）。

---

## 1. 這一批做什麼（先用遊戲講）

**單機 2D 橫版。** 產品的 `main` 只寫「用預設的 Module、加上我自己的遊戲、跑起來」。編排層照 Module 登記檔把每個 Module 與產品自己的遊戲程式接進主 World：先登記（build），Kernel 建 Plan，再 finish，最後跑選了的檢查；全部過了才建 World。runner 先跑遊戲初始化清單一次（讀按鍵設定；狀態機把「進入主選單」插在它後面），再每幀跑每幀的清單。現在的 Kernel 自己認得「開局」（`ScheduleOptions::startup`），這一批把它拿掉、搬到編排層（使用者 11-17「用Bevy的方式走，」）。

**RPG 在遊戲中讀檔。** 玩家在第 3 關按「讀取存檔 2」。編排層照產品描述重新登記、建一個新的 World（使用者 2026-11-18 定：讀檔就是回到某一刻的狀態，之前動態插入的 System 一律重置），在第一次跑它之前給作者一個時機：把配發器範圍的下界移到存檔記的下一號之後、放好暫存的存檔資料與金幣的預設值 0；接著在新 World 上跑作者的讀檔 schedule（`ExclusiveWorld` 當場照存檔編號放回玩家與物品、金幣改成 120）；預設**全部成功才換上**，存檔讀不了（例：版本太舊不支援）就丟掉新的、舊的照跑；地圖大到兩個 World 放不下時，作者可以選「先丟舊的」（使用者 2026-11-18 定，第 14 節第 4 件）。遊戲初始化不再跑（它是行程一次）。按鍵設定是遊戲初始化放進主 World 的，玩家在選項選單裡還改過——主 World 換掉之後它照樣在：作者把它標記成「換 World 時帶過去」，編排層照標記搬過去（使用者 2026-11-18 定，3.12）。

**將來的指令集。** 輸入一行指令加一個 System。World 宣告准許時，編排層走 Kernel 的兩步（登記、建 Plan），下一幀那個 System 就跑；宣告不准時（PvP 的 Match），插入被拒、回傳錯誤、**記一次警告**、遊戲照跑（使用者 11-17：「插入就拒絕，返回信息（不退出，但是System無法插入，日誌報警告）」）。第 1 批只做了 Kernel 那一半，警告沒人記（第 1 批宣告第 11 節），這一批補上。

**PvP 的 Match。** 產品另選權威 Module，World 的描述帶「策略＝伺服器權威」。權威 Module 在所有 Module finish 之後跑一次 `Check`（權威份 4.5），不過就啟動失敗；Kernel 自己有啟動錯誤時 `Check` 不跑，報告寫「因 Kernel 錯誤而未檢查」（統合 O-5）。這一批沒有權威 Module，只做這個時機與彙整。

**單機不上權威。** Module 預設全帶，產品可以關（使用者 11-17「能，要不然單機遊戲也必須上權威了。」；主會話照 Bevy `DefaultPlugins`／Unity 判定預設全帶可關）。

**走路計分的小遊戲（scenario test 第一段，第 8 節）。** 只放在 `tests/`、沒有畫面與輸入、不是產品：遊戲初始化把分數設 0 → 放玩家在 (0,0) → 跑 5 幀 → 指令加入「加速」→ 再跑 5 幀 → 存檔 → 換 World 讀檔 → 再跑 5 幀，每一步檢查位置與分數。之後每一批往上加一段（第 3 批 tick 與固定步、第 4 批回滾）。

---

## 2. 範圍

### 2.1 這一批做的（O1～O13）

| # | 內容 | 根據 |
|---|---|---|
| O1 | 新的 Module `engine/orchestration`（`GYO::Engine::Orchestration`，target `gyo_orchestration`／`GYO::Orchestration`），只依賴 Kernel 與 Base | `orchestration-layer-name`；統合第 3 節、O-4；命名規範 N11、N14、N17 |
| O2 | 登記單位的兩種型別 `Module`、`Plugin`（從 Kernel 的 `Plugin` 搬來、拆成兩種）與生命週期：產品層一次的 `Configure`、每個 World 的 `Build`、`Finish`、`Check`；**產品自己的遊戲程式**由描述裡的回呼登記（不是第三種登記單位） | `module-plugin-registers`；統合 L5、O-2、3.2 第 1 列 |
| O3 | `Product`：World 群（每個 World 一份 Registry）、World 的描述（名字、准不准動態插入、用哪些登記單位、給各 Module 的選項包、產品自己的登記）；build → 建 Plan → finish → 檢查 | 統合 L5、O-2、15.2；權威份 4.4、4.5 |
| O4 | 啟動錯誤的彙整：Kernel 的 `StartupReport`（每個 World 一份）、編排層自己的碼、各登記單位回報的錯誤收成一份 | 統合 O-5 |
| O5 | Module／Plugin 登記檔：格式、CMake 讀取（`engine/` 的子目錄與各 Module 的 link 從它來、替產品產生登記單位的表）、編排層讀出依賴並檢查；Module 預設全帶、產品可以關，Plugin 由產品列出 | `module-plugin-registers`、`module-deps-in-registers`、`products-pick-modules`；統合 L20；批次表「主會話自己判的」第 3 條 |
| O6 | 遊戲初始化清單（產品一份、跑在第一個 World、行程一次、在第一幀之前）與每幀的清單（每個 World 一份）；清單可以在指定的 schedule 前後插入 | `game-init-list`、`two-layers-like-bevy`；統合 O-1 |
| O7 | runner 的介面（由登記單位在 `Configure` 裝、一個 `Product` 至多一個）與預設的「只跑一次」runner | `thread-binding-by-runner`；統合 O-3、第 8 節本批那一列 |
| O8 | 準備時機：建 World 之後、第一次 Run 之前，依序呼叫登記單位與產品給的準備函式 | 讀檔宣告修訂 10、11（5.1、5.8）；統合修訂 33 |
| O9 | 執行中替換 World：照產品描述重新登記、建新的、準備、在新 World 上跑「換上之前的 schedule」（讀檔）、全部成功才換上；遊戲初始化不重跑；中斷的 World 也能被換掉 | 讀檔宣告修訂 11（5.7、第 8 節） |
| O10 | 動態插入的編排半：插入入口；拒絕或建 Plan 失敗時回傳錯誤、經 `Engine::Base::Log` 記一次警告、World 照跑 | `dynamic-insert-per-world`；統合 L5；CCR-3 |
| O11 | **同一批拿掉 Kernel 的開局排程**：`ScheduleOptions::startup`、D-E21、D-E12 的開局那一支、`StartupNotNestable`、`StartupRunning`、DI-KS 22、World 與 `WorldExecutor` 的開局狀態；J-58 改寫後搬到編排層 | 批次表第 2 條；統合 3.2 第 3 列、7.2 J-58 列 |
| O12 | **同一批拿掉 Kernel 的登記單位**：`Plugin`、`FinishContext`（含 `InstallRunner`）、`Registry(std::vector<Plugin*>)`、`Registry::Finish()`、R-E1、R-E3、R-E6；Registry、Plan、C 指紋與 Plan 匯出格式改版、golden 重產 | 統合 3.2 第 1 列、7.2 J-19、J-24 列；第 1 批宣告 3.2 規則 9 |
| O13 | **scenario test 第一段**（走路計分的小遊戲；遊戲初始化、放玩家、跑幀、動態插入、存檔、換 World 讀檔），涵蓋第 1 批與本批的機制；之後各批往上加的接口 | `scenario-tests`、`scenario-failure-by-contract`、`scenario-cross-engine-check`；主會話照 L17（涵蓋第 1 批） |

另外一件（第 1 批的遺留，主會話交辦，已隨本宣告修訂 0 提交，f9f74684）：結構段 dev_log 的「驗證與經過」補 G-6 成立、開頭改成已合併。

### 2.2 不在這一批（留到哪一批）

| 留下的 | 隨哪一批 |
|---|---|
| 生成世界（每個 World 各自、可以多次、可以分批）；讀檔 System 掛在生成世界的方式 | 第 5 條「生成世界（重做）」。它接在本批 `ReplaceWorld` 的「換上之前的 schedule」與啟動時的同一個位置（3.9） |
| 多個 World 之間的交換點、extract 的時機（`SetExtractSource` 要在讀的那一份第一次建 Plan 之前設，`Product::Build` 第 3 步每個 World 新建 Registry 時留這個位置）、執行緒的安排；**替換 World 會重建它的 Registry**（3.9），以它為 extract 來源的 World 的處理（依賴它的一起重建，或改 Kernel 讓 extract 來源可以換）在第 5 條定 | 第 5 條「多個 World、交換點」 |
| 每幀迴圈的 runner 與 System 端的請求通道（結束、替換 World、從指令插入；照 Bevy 的 `AppExit` 是一種訊息） | 第一個需要它的批。訊息在第 3 條改名 `Message`，先做會白改一次（3.9） |
| 大型靜態地圖放在 World 外、多個 World 共用（Unity `BlobAssetReference` 那一型） | 資產或地圖那一批（`replace-order-author-choice` 的後續；BigMap 換圖時才會碰到） |
| 存檔的版本轉換 | 不做（`save-unaware-means-format-only`）；產品自己管版本，太舊的檔不支援、讓玩家開新檔（使用者 2026-11-18） |
| 存檔版本 Module（`v8-arch-gyo2-save-version-module`：預設不啟用、啟用後過時的存檔直接拒絕；使用者明說不放編排層） | 批次表第 5 條（讀檔的實作同一條線或緊接其後）。本批要做到的只有兩件：讀檔失敗的路接得住它的拒絕（3.9「存檔讀不了」）、登記檔擺得下「預設不帶的 Module」（3.2 的 `default` 欄） |
| 時鐘、`Domain`、tick | 第 3 條。之前遊戲初始化的 schedule 要屬於一個 domain（3.6） |
| 寫者數量 D-E2、`Shared` | 第 4 條。之前遊戲初始化的 System 照樣算寫者（第 7.1 節組 E） |
| 權威、回滾、時間、狀態機這些 Module 本身 | 各自的批 |
| 綁實作方式的守衛（掃描、符號、連結字面）與守它們的突變 | 涵蓋它的 scenario 段通過之後（第 7.4 節） |

---

## 3. 設計

本節的程式是草圖：成員的確切簽名、`const`、`[[nodiscard]]` 與 namespace 的寫法（例：在 `GYO::Engine::Orchestration` 裡要寫 `::Engine::Base::Result`，否則 `Engine::` 會先找到 `GYO::Engine`）在實作時定；定下的是型別的分工、呼叫的順序與錯誤的歸屬。

### 3.1 O1：Module 的位置

`engine/orchestration/`，底下 `include/engine/orchestration/` 與 `src/`（N11、N12）；namespace `GYO::Engine::Orchestration`（N17：新寫的 Module 照 N17）；target `gyo_orchestration`、別名 `GYO::Orchestration`（N14）。依賴：Kernel、Base（統合 O-4），寫在登記檔（3.2），不在 CMakeLists 另寫。

### 3.2 O5：Module／Plugin 登記檔

**事實**：gyo2 上還沒有這兩份檔；產品登記檔是 `engine/config/projects.csv`、`tools.csv`（CMake 用 `_gyo_registry_csv_records` 讀，`build/cmake/GyoAppRegistry.cmake`；CI 的 `build/ci/common/app_registry.py` 也讀）。11-14 定的：兩份、放建置層、只有一處；每列寫名字、標記（進 World／不進 World）、依賴；CMake 照它 link；執行時的 Registry 只收進 World 的登記單位、由清單產生、不另外手寫；清單是為了看得到，不是在執行時擋下沒登記的使用；Module 在 Plugin 前、Plugin 可以依賴 Module、反過來不行（`module-plugin-registers`）。11-17 定的：依賴只寫在登記檔、編排層讀出並在 build 時檢查、缺或排在後面是啟動錯誤、不重排、程式碼不寫 `Requires`（`module-deps-in-registers`）。格式在本批或建置層的批定（統合 L20）——**本批定**（批次表第 2 條寫了「登記檔的讀取」）。

**格式**（卡片照 L17 判：照 repo 既有的產品登記檔）：`engine/config/modules.csv`、`engine/config/plugins.csv`

| 欄 | 意思 |
|---|---|
| `name` | 名字＝`engine/<name>/` 或 `plugins/<name>/` 的目錄名（N11、N21）；CMake target 是 `gyo_<name>`（N14） |
| `target` | link 用的別名（例 `GYO::EntityStore`、`GYO::Io`）。N14 的別名大小寫不規則（`entity_store` → `EntityStore`），從名字推不出；既有的 `gyo_expect_links` 比的是別名字面，所以一律 link 這一欄 |
| `description` | 一句話 |
| `library_only` | `yes`＝不進 World（只當函式庫用，沒有登記單位）；`no`＝進 World |
| `default` | 進 World 的 Module：`yes`＝產品預設帶（可以關）、`no`＝預設不帶（產品要列出才帶；例：使用者明說「默認不啟用」的存檔版本 Module，`save-version-module`）；不進 World 的列與 `plugins.csv` 一律 `no`（Plugin 預設不帶） |
| `depends` | 依賴的 Module／Plugin 名字，`;` 分隔；CMake 以 PUBLIC（`INTERFACE` 函式庫以 INTERFACE）link |
| `private_depends` | 同上，CMake 以 PRIVATE link（現在 `io` → `math`、`os`，`threads` → `os` 是 PRIVATE；混成 PUBLIC 會把 OS 層的標頭漏給 io 的使用者） |
| `unit`、`header` | 進 World 的那一列：登記單位的完整類別名與標頭；不進 World 的列留空 |

本批寫進 `modules.csv` 的列：`base`、`math`、`entity_store`、`kernel`、`orchestration`、`os`、`time`、`threads`、`io`、`audio`，全部 `library_only=yes`（現在沒有任何 Module 有登記單位；時間的登記單位隨第 3 條）。順序＝現在 `engine/CMakeLists.txt` 的 `add_subdirectory` 順序（被依賴的在前）。`plugins.csv` 只有表頭。

**CMake 讀取**（新檔 `build/cmake/GyoUnitRegistry.cmake`，可以用 `cmake -P` 跑）：
1. **驗證檔案**（configure 時 `FATAL_ERROR`，訊息寫檔名、列、原因）：表頭不對；兩份檔裡名字重複；依賴指到不存在的名字或排在後面的列；Module 依賴 Plugin；`library_only` 和 `unit`／`header` 對不上。
2. **`engine/` 的子目錄與各 Module 的 link 從它來**：`engine/CMakeLists.txt` 照 `modules.csv` 的列序 `add_subdirectory`；每個 `engine/<name>/CMakeLists.txt` 的引擎內 `GYO::<Module>` link 換成 `gyo_link_registered_dependencies(<name>)`；第三方的 link（`Threads::Threads`、`shell32`）照舊留在 CMakeLists。卡片照 L17 判：AGENTS §9「同じ事実を手動で維持する場所は、一か所に限定する」，11-14 的原話「CMake 照它 link」。別家：O3DE 把 Gem 的依賴寫在 `gem.json`、CMake 的 target 依賴另寫（兩處）；Unreal 的 Module 依賴寫在 `.Build.cs`、Plugin 依賴在 `.uplugin`，UBT 照它們建（一處）。取一處。`tests/common/*` 的五處 `gyo_expect_links` 是既有的獨立核對（不是第二個來源，它們讀 link 的結果比對），照「第 1 批已立的守衛不拆」留著、不改；`gyo_orchestration` 不加新的一處（第 7.4 節延後）。
3. **替產品產生登記單位的表**：`gyo_product_units(<target> [DISABLE_MODULES <名字>…] [MODULES <名字>…] [PLUGINS <名字>…] [REGISTERS <modules.csv> <plugins.csv>])`：
   - 預設帶 `modules.csv` 裡**進 World 且 `default=yes` 的 Module**；`DISABLE_MODULES` 關掉列出的；`default=no` 的 Module 要在 `MODULES` 列出才帶；Plugin 預設不帶，`PLUGINS` 列出才帶。關掉的是帶上的單位依賴的、或列了不存在的名字，configure 時 `FATAL_ERROR`（檢查 S16；執行時 `Product::Build` 照舊再檢查，因為 `RegisteredUnits` 可以手寫）。
   - link 帶上的登記單位的 target；在 `<target>` 的 binary 目錄產生 `<target>_units.cpp`，定義 `RegisteredUnits MakeRegisteredUnits()`：兩份登記檔的全部列，與帶上的登記單位的實例（照登記檔的順序，每個實例和它那一列配成一對；Module 的實例放 `std::unique_ptr<Module>`、Plugin 的放 `std::unique_ptr<Plugin>`，放錯是編譯錯誤）。
   - `REGISTERS` 給測試用（共通測試用合成的登記檔，AGENTS §7）。
   - 產品選什麼寫在**產品自己的建置描述**（它自己的 `CMakeLists.txt` 裡這一個呼叫）：產品的新增、刪除只動它自己的內容與登記（AGENTS §7）。
4. 「Module 預設全帶、產品可以關」：照主會話的判定寫明（Bevy `DefaultPlugins` 加 `PluginGroupBuilder::disable`，`crates/bevy_app/src/plugin_group.rs:551`；Unity 內建模組預設帶、可關；O3DE 要列 `enabled_gems` 是少數）。**Plugin 預設不帶**（卡片照 L17 判：Plugin 是第三方、產品選用，同詞異義表 Plugin 列）。

**登記單位沒有 `Name()`**（卡片照 L17 判，檢查 S9）：名字、屬於哪一份、進不進 World，登記檔都有；產生的表把每一列和它的實例配成一對。舊 `Kernel::Plugin::Name()` 是沒有登記檔時的形狀（`target/README` 每批事前宣告的檢查項：「這是不是在為舊形狀找位置？」——是，所以不帶）。這樣「名字不在登記檔」「放錯那一份」在產生的表裡不會發生，也合 11-14「不是在執行時擋」。

**編排層讀出依賴並檢查**（執行時，`Product::Build`；使用者定的「編排層從登記檔讀出依賴並在 build 時檢查」）：對每個 World 實際用到的登記單位：
- 同一列配了兩個實例（O-E1，舊 R-E1）；
- 實例指到的列不存在或是 `library_only=yes`（O-E2；只有手寫的 `RegisteredUnits` 會出現）；
- 依賴：進 World 的依賴必須是這個 World 也用到、而且排在前面的單位；不進 World 的依賴視為已經在（CMake 已經 link）；Module 依賴 Plugin（O-E3，舊 R-E3）。
- 已知的限界（檢查 N7）：`depends` 一欄同時是 link 的依賴與 World 裡的登記依賴；多個 World 各自列 `units` 時，「這個 World 只用它的函式庫、不要它的登記單位」會被 O-E3 擋。L20 定的是一欄；碰到時照 RC-O6。

**退掉的選項**（AGENTS §4）：
- 格式：CMake 語法的檔——CI 的 Python 讀不了；JSON——repo 的登記檔都是 CSV，CMake 已有讀 CSV 的函式。系譜：`engine/config/projects.csv`。
- 登記單位怎麼產生：每個 Module 在自己的程式裡靜態自我登記——靜態初始化的順序與連結器丟掉沒被參照的物件檔，會讓「登記檔看得到的」和「實際接上的」不一致；產品 `main` 手寫清單——11-14「由清單產生，不另外手寫」。系譜：O3DE 與 Unreal 都由建置工具照清單產生接上的程式。
- 依賴的檢查只在 configure 時做——使用者定的是編排層在 build 時檢查；configure 時另外先擋一次。
- 登記單位帶 `Name()` 再比對登記檔——上一段。

### 3.3 O2：登記單位的兩種型別與產品自己的登記

```cpp
namespace GYO::Engine::Orchestration {
namespace Detail { class RegistrationUnit; }   // 兩種型別共用的基底；作者不直接繼承
class Module : public Detail::RegistrationUnit {};
class Plugin : public Detail::RegistrationUnit {};

class Detail::RegistrationUnit {
public:
    virtual ~RegistrationUnit();
    // 產品層、一次、在任何 World build 之前：插入遊戲初始化清單、裝 runner。
    virtual void Configure(ProductSetup& setup);
    // 每個用到它的 World 一次：setup.WorldName()、setup.Registrar()、setup.Options<T>()；
    // 插入這個 World 的每幀清單、準備函式。
    virtual void Build(WorldSetup& setup) = 0;
    // 這個 World 的 Plan 建好之後（版本 1），依同順序：讀唯讀登記表，可以回報錯誤；不能登記。
    virtual void Finish(const Kernel::FrozenRegistry& registry, WorldFinish& context);   // context.WorldName()
    // 這個 World 所有登記單位 finish 之後，依同順序：策略的檢查（權威份 4.5 的 Check）。
    virtual void Check(const Kernel::FrozenRegistry& registry, WorldCheck& context);     // context.WorldName()、Options、dynamicInsert
};
}
```

- **兩種型別**照 11-14「拆成兩種型別，優先也包含依賴方向」；名字照 N20。兩種的成員一樣，差在 `RegisteredUnits` 收它們的位置（Module 一份、Plugin 一份，Module 先）與依賴方向的檢查。共用基底放 `Detail`（N13）。
- **一個實例、多個 World**（檢查 S13）：一個 `Product` 每個登記單位一個實例，`Build`／`Finish`／`Check` 每個 World 各呼叫一次；三個 context 都交出 World 的名字，Module 以它當鍵記每個 World 自己的東西（權威份 4.4、4.7 要「這個 World 選了什麼策略、有沒有權威登記」）。
- **`Requires()` 拿掉**（`module-deps-in-registers`）；**`Name()` 拿掉**（3.2）。
- **生命週期的順序**（統合 L5、O-2）：見 3.4。和現在 Kernel 的 `Registry::Finish()` 不同：現在 `Plugin::Finish` 在建 Plan 之前跑、讀到版本 0（`Registry.cpp:1843-1852`）；改成在建 Plan 之後、讀到版本 1 與 Plan（第 1 批宣告 3.2 規則 9）。
- **`Finish` 與 `Check` 分開**（卡片照 L17 判）：任何 `Finish` 都讀得到最後的登記（`Finish` 不能登記）；分開是為了讓策略的檢查在**所有** `Finish` 之後（權威份 4.5），而且 Kernel 有啟動錯誤時兩者都不跑（O-5）。
- **為什麼多一個 `Configure`**（卡片照 L17 判）：遊戲初始化清單與 runner 屬於 `Product`，而 `Build` 每個 World 跑一次。Bevy 的 `Plugin::build(&mut App)` 一次涵蓋兩者，因為 Bevy 的 Plugin 只 build 一次、要碰子 App 時自己 `sub_app_mut`；GYO 每個 World 各有一份 Registry。
- **產品自己的遊戲程式**（檢查 M2；卡片照 L17 判）：遊戲本身的元件、System、schedule 不是 Module（不是 Engine 的）也不是 Plugin（不是第三方），不進兩份 Engine 的登記檔（AGENTS §7：產品的新增不改共通層）。產品在描述裡給回呼：`ProductDescription::configure`（同 `Configure`）、`WorldDescription::build`（同 `Build`，排在這個 World 所有登記單位之後；owner＝產品名）。不另立第三種登記單位：三家都讓遊戲程式不經外掛的概念直接登記——Bevy 在 `main` 呼叫 `App::add_systems`（`crates/bevy_app/src/app.rs` 的 `add_systems`）、flecs 在 `main` 呼叫 `ECS_SYSTEM`／`ecs_system_init`、Unity 的 System 由 bootstrap 找到。每個 World 一定經過產品的 `Register`（就算回呼是空的），所以沒有任何登記單位的 World 照樣凍結、建得起來（檢查 S2：`BuildPlans` 在沒有任何 `Register` 時不凍結，`Registry.cpp:1789`）。
- 系譜：Bevy `Plugin` 的 `build`／`finish`／`cleanup`（`crates/bevy_app/src/plugin.rs:59`、`:70`、`:77`）；Kernel 現在的 `Plugin`（`Registry.hpp:72-87`）。**退掉的**（檢查 S12）：「一種型別」（Bevy）——使用者 11-14 定兩種；「`Configure` 併進第一個 World 的 `Build`」——World 的順序是描述的細節，產品層的事不該綁在某個 World；「`Finish` 裡順便檢查、不另立 `Check`」——權威份要求在所有 `Finish` 之後。

### 3.4 O3：`Product`

```cpp
struct WorldDescription {
    std::string name;                         // Product 內唯一（O-E6）
    bool dynamicInsert = true;                // 轉成 Kernel::WorldOptions（L5）
    std::vector<std::string> units;           // 空＝Product 帶上的全部登記單位
    OptionBag options;                        // 各 Module 自己解讀的選項（例：權威策略）；編排層轉交、不解讀
    std::function<void(WorldSetup&)> build;   // 產品自己的登記（3.3），排在登記單位之後
    std::vector<std::string> frame;           // 這個 World 的每幀清單（schedule 名），登記單位可再插入
    std::vector<PrepareFn> prepare;           // 產品給的準備函式，排在登記單位的之後（3.8）
};
struct ProductDescription {
    std::string name;                         // 產品自己登記時的 owner 名
    std::vector<WorldDescription> worlds;     // 第一個是「第一個 World」：遊戲初始化在它上面跑
    std::vector<std::string> gameInit;        // 遊戲初始化清單，登記單位可再插入
    std::function<void(ProductSetup&)> configure;   // 產品自己的 Configure
};

class Product {
public:
    static Engine::Base::Result<std::unique_ptr<Product>, ProductStartupReport>
        Build(ProductDescription description, RegisteredUnits units);
    RunnerExit Run();                                                          // 呼叫 runner（3.7）
    // 給 runner 與 Product 的持有者（兩次 Run 之間、同一條執行緒）：
    Engine::Base::Result<void, Kernel::KernelError> RunGameInit();            // 只能一次（第二次斷言）
    Engine::Base::Result<void, Kernel::KernelError> RunFrame();               // 遊戲初始化還沒跑時先跑它
    Engine::Base::Result<void, Kernel::KernelError> RunSchedule(std::string_view world, std::string_view schedule);  // 同上，先確保遊戲初始化
    Kernel::WorldExecutor& World(std::string_view name);                      // 參照到這個名字下一次被替換為止
    Engine::Base::Result<void, OrchestrationError> ReplaceWorld(std::string_view name, ReplaceRequest request);  // 3.9
    Engine::Base::Result<void, OrchestrationError> InsertSystems(std::string_view world, std::string owner, InsertFn build);  // 3.10
};
```

**`Product::Build` 的順序**（卡片照 L17 判，順序本身照統合 O-2；錯誤收齊照現在 Kernel `Registry::Finish()` 的做法：出錯的單位不 build、其餘照做，檢查 M6）：
1. 檢查 World 描述（沒有 World、名字重複、`units` 指到沒帶上的單位：O-E6）與 `RegisteredUnits`（O-E1、O-E2、O-E3）。同一列配兩個實例的第二個、指到不存在或不進 World 的列的實例**不用**（不 `Configure`、不 `Build`），其餘照做——依賴的錯誤（O-E3）照樣 build，Kernel 的錯誤（R-E4 等）和它們進同一份報告（現在 Kernel 的 `Registry::Finish()` 也是重複的不 build、其餘照做，`RegistryTests` :251「R-E1 to R-E4 … all in one report」）。描述有錯的 World 不建 Registry。
2. 用到的登記單位依序 `Configure`（Module 先、各自照登記檔的順序），再呼叫產品的 `configure`：收集遊戲初始化清單的插入與 runner（第二個是 O-E4，舊 R-E6）。
3. 每個 World 依序：新建一份 `Kernel::Registry`（第 5 條的 `SetExtractSource` 在這裡、第一次 `BuildPlans` 之前）；用到的單位依序 `Register(owner, unit.Build)`（owner＝那一列的名字與它在 Product 裡的序號，統合 3.5），再 `Register({產品名, 序號}, 產品的 build)`；`BuildPlans`。**成功**：依序 `Finish`，再依序 `Check`。**失敗**：收下這個 World 的 Kernel `StartupReport`，這個 World 的 `Finish`、`Check` 與第 4 步的清單檢查不跑，報告寫「因 Kernel 錯誤而未檢查：Finish、Check、清單」（O-5）；別的 World 照做。
4. 清單：插入照 3.6 解析；遊戲初始化清單的每個名字在第一個 World 的 Registry 裡是 schedule；每幀清單的每個名字在它那個 World 的 Registry 裡是 schedule（O-E5）。
5. 有任何錯誤：回傳 `Err(ProductStartupReport)`，沒有任何 World 被建；第 1 批宣告第 11 節的「`BuildPlans` 換上之後策略檢查失敗、Kernel 撤不回」在啟動時不會發生（Registry 一起丟掉）。
6. 每個 World 依序建 `Kernel::WorldExecutor(registry, name, WorldOptions{dynamicInsert})`，跑準備（3.8）；準備失敗是 O-E7。O-E7 只會在其餘全部通過時出現（準備要在建好的 World 上跑），所以「一次收齊」是 O-E1～O-E6 與 Kernel、登記單位的錯誤一份，O-E7 另一份。

**每個 World 一份 Registry**（卡片照 L17 判）：統合 15.2「登記、Plan、Registry 都是每個 World 一份」；Kernel 允許一份 Registry 建多個 World，但那樣准與不准動態插入的 World 不能共用（DI-KS 26）。

**選項包**（`OptionBag`）：以型別為鍵、各 Module 讀自己的型別；編排層只存、只轉交（統合 O-2）。策略在建 World 時選、在各 Module build 之前（統合 15.2）。

**`WorldCheck` 交給 `Check` 的 World 宣告**：`Check` 跑的時候 World 還沒建（第 6 步才建），所以交出描述裡的 `dynamicInsert` 與選項包——和之後建 World 用的是同一份值（判定有一案比對兩者）。

**錯誤與警告的出口**（檢查 S5）：Kernel 的錯誤照舊只在 `WorldExecutor` 的錯誤處理器記一次（error-handling DI 2；編排層不改它的預設）；編排層自己的警告用既有的 `Engine::Base::Log(LogLevel::Warning, "orchestration", …)`（`engine/base/include/engine/base/Log.hpp:67`），不另立處理器（AGENTS §13：不和既有概念重複）。替換之後新的 `WorldExecutor` 的錯誤處理器沿用舊的那一個（`Product` 記住每個 World 設過的處理器）。

**中斷的 World**（檢查 S1）：Kernel 對中斷的 World 再 `Run` 是斷言（`Executor.cpp:195`）。`Product` 記住哪個 World 中斷：`RunFrame`／`RunSchedule` 碰到它回錯誤 `WorldInterrupted`、不呼叫 Kernel；別的 World 照樣能跑；`ReplaceWorld` 可以換掉它（讀檔失敗後的收拾）。「先丟舊的」替換失敗後，那個位置是**空的**（3.9）：`RunFrame`／`RunSchedule` 碰到它回 `WorldMissing`、不呼叫 Kernel，別的 World 照樣能跑，`ReplaceWorld` 可以再填上。遊戲初始化失敗之後第一個 World 是中斷的，`RunFrame` 回 `WorldInterrupted`。

**例外**（檢查 S4）：`Configure`／`Build`／`Finish`／`Check` 在 `Product::Build` 裡丟出例外：例外往外傳，`Product` 沒有建成、已建的 Registry 一起銷毀。`InsertSystems` 的 `build` 丟出例外：編排層先 `DiscardRegistrations`（Kernel 的規則 3 會把已記的留在暫存副本，留著的話下一次建 World 是斷言，`World.cpp:59`），再把例外往外傳。替換時準備或 schedule 丟出例外：照 3.9 第 4 步收拾，再往外傳。

**執行緒**：這一批的 `Product` 全部在呼叫的執行緒上做——登記、建 World、執行、替換、插入、解構（CCR-5；綁執行緒的 Resource 在 `~World` 斷言，`World.cpp:114-116`）。多執行緒在第 5 條。

**系譜**：Bevy `App` 持有主 World 與子 App（`crates/bevy_app/src/app.rs:1235` `insert_sub_app`）；Unity 的 `ICustomBootstrap` 在任何 World 建立之前讓作者自己建 World 群（`Unity.Entities/World.cs:90-94`、`DefaultWorldInitialization.cs:122`）。**退掉的**：Bevy 的「Plugin 自己決定碰哪些子 App」——GYO 每個 World 的描述寫它用哪些單位，看描述就知道 World 裡有什麼；Unity 用 `WorldSystemFilter` 屬性讓 System 選 World——GYO 沒有屬性的機制。

### 3.5 O4：啟動錯誤的彙整與執行時的錯誤

`ProductStartupReport`：每個 World 一段（Kernel 的 `StartupReport` 原封不動，Kernel 的 `StartupCode` 不擴充，O-5；或「通過」；或「未檢查」那一行）；編排層的 `OrchestrationCode`；各登記單位在 `Finish`／`Check` 回報的 `{那一列的名字、它自己的碼（字串）、訊息}`。

| 碼 | 意思 | 從哪來 |
|---|---|---|
| O-E1 | 同一列配了兩個實例 | 舊 R-E1（`Registry.cpp:753`） |
| O-E2 | 實例指到不存在或不進 World 的列（只在手寫的 `RegisteredUnits`） | 新 |
| O-E3 | 依賴缺、在後面，或 Module 依賴 Plugin | 舊 R-E3（`Registry.cpp:766`），來源從 `Requires()` 換成登記檔 |
| O-E4 | 兩個裝 runner | 舊 R-E6（`Registry.cpp:1536`） |
| O-E5 | 清單錯誤：錨點不在、名字不是那個 World 的 schedule | 新（Bevy 的 `insert_startup_before` 錨點不在時 panic，`main_schedule.rs:283`；GYO 改成啟動錯誤、一起收齊） |
| O-E6 | World 描述錯誤：沒有 World、名字重複、`units` 指到沒帶上的單位 | 新 |
| O-E7 | 啟動時準備函式回傳錯誤 | 新 |

執行時的錯誤 `OrchestrationError` 另一組碼（不和啟動碼共用，檢查 N5）：`ReplacePrepareFailed`、`ReplaceScheduleFailed`、`InsertRefused`（包住 Kernel 的 `DynamicInsertRefused`）、`InsertBuildFailed`（包住 Kernel 的建 Plan 報告）、`WorldInterrupted`、`WorldMissing`、`ReplaceRegistrationFailed`、`ReplaceCarryOverFailed`（3.12）、`OwnerTaken`（3.10）。準備函式回傳 `OrchestrationError`，作者填 `PrepareFailed`、自己的碼（字串）與訊息；`ReplaceWorld` 的錯誤原樣帶著它（3.9：存檔版本 Module 的拒絕靠這個分辨）。碼的寫法照 Kernel 的「R-E1」「D-E1」（卡片照 L17 判，前綴 O 取 orchestration）。

### 3.6 O6：遊戲初始化清單與每幀的清單

**遊戲初始化清單**（O-1）：一個 `Product` 一份，一串 schedule 名。初始內容來自 `ProductDescription::gameInit`；登記單位與產品在 `Configure` 裡用 `setup.GameInit().Append(name)`、`.InsertBefore(anchor, name)`、`.InsertAfter(anchor, name)` 插入。Engine 不給預設的名字（使用者：「內容有作者自己訂，Engine只提供機制」）。
- **插入的語意**（檢查 S14；卡片照 Bevy 判）：插入當下解析錨點（`Configure` 照單位的順序跑，所以先跑的單位插的東西後跑的看得到）；錨點不在記 O-E5、這一筆不插；兩個單位插在同一個錨點之前，後插的緊貼錨點（Bevy `insert_startup_before` 插在錨點的位置，`main_schedule.rs:274-284`）。同一個名字出現兩次**不是錯誤**（Bevy 不擋；一張 schedule 跑兩次是合理的用法，`engine-not-rigid`）；錨點的名字出現兩次時取第一個。
- 跑在**第一個 World**（使用者 `game-init-list`）。
- 行程一次：`RunGameInit()` 依序對第一個 World 各 `Run` 一次；第二次呼叫是斷言。`RunFrame()` 與 `RunSchedule()` 在遊戲初始化還沒跑時先跑它（主程式也可以先自己呼叫一次）。runner 直接呼叫 `WorldExecutor::Run` 是繞過編排層，不會先跑遊戲初始化（檢查 S10；說明頁寫明，runner 用 `RunSchedule`）。
- 任何一張回錯誤：停下、回傳那個錯誤，遊戲初始化記為已跑，第一個 World 中斷（3.4「中斷的 World」）。
- 每張 schedule 是一次獨立的最外層 `Run`：各自在開頭重讀 `KernelSettings`（DI-KS 24）。
- **第 3 條之前要有 domain**（檢查 M3）：拿掉 `startup` 之後，Kernel 的 D-E12 要每張 schedule 屬於一個 domain、每個用到的 domain 剛好一張 clock schedule（`Registry.cpp:989-1009`）。所以遊戲初始化的 schedule（與讀檔、放玩家這類只跑一次的 schedule）掛 `Domain::Frame`、`clock = false`，每幀的那一張當 Frame 的 clock。第 3 條拿掉 domain 時這些宣告跟著刪。
- 系譜：Bevy `MainScheduleOrder::startup_labels`、`insert_startup_before`／`_after`（`crates/bevy_app/src/main_schedule.rs:218`、`:260`、`:274`），`Main::run_main` 只在第一次跑它們（`:289-297`），只在主 World；flecs 的 `OnStart` 在第一次 `ecs_progress` 跑一次（`src/addons/pipeline/pipeline.c:715`、`:995`）。**退掉的**：Bevy 給三個預設名字——使用者定內容由作者訂；「每個 World 各跑一次」——使用者比過、沒選。`RunGameInit` 只准一次加 `RunFrame` 自動觸發是 K0 的形狀搬來（檢查 S12 的「舊形狀」一問）：Bevy 的 `run_main` 用 `Local<bool>` 做同一件事，不是只為了沿用。

**每幀的清單**：每個 World 一份（`WorldDescription::frame`，登記單位在 `Build` 裡用 `setup.Frame()` 插入，同樣的語意）。`RunFrame()` 依 World 的順序、每個 World 依清單順序 `Run`；有錯就停下回傳。動態插入登記的新 schedule 不自動進每幀清單（runner 用 `RunSchedule` 跑它；檢查 N5）。

**和現在的 Kernel 開局（K0）的差別**（J-58 的改寫，第 7.1 節組 A）：

| 現在（Kernel，DI-KS 22） | 之後（編排層，O-1） |
|---|---|
| 每個 World 至多一張 Startup（D-E21） | 一個 `Product` 一份清單、可以多張、只在第一個 World |
| 第一次跑**任何**別的 schedule 時，Kernel 先跑它（同一次 `Run` 裡） | `RunFrame`／`RunSchedule` 先跑它；直接 `WorldExecutor::Run` 不會 |
| System 不能跑它；它執行中不能跑別的 | Kernel 不認得它，這兩條沒有了 |
| 沒有 domain、沒有 clock，在推 tick 之前 | 第 3 條之前掛 Frame、不當 clock |
| 只掛在 Startup 的 System 不算寫者（DI-KS 6） | 照樣算（第 4 條拿掉 D-E2 之前） |

### 3.7 O7：runner

```cpp
using Runner = std::function<RunnerExit(Product&)>;
void ProductSetup::InstallRunner(std::string_view name, Runner runner);   // 一個 Product 至多一個；第二個是 O-E4
```

- 由登記單位（或產品的 `configure`）在 `Configure` 裡裝；沒人裝時用預設的**只跑一次**：`RunFrame()` 一次，成功回 `RunnerExit::Success`，失敗回 `Failure` 帶錯誤。`Product::Run()` 呼叫 runner 一次；第二次呼叫是斷言。
- runner 擁有迴圈、決定哪條執行緒跑哪個 World；只呼叫 `Product` 的執行入口（`RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`；之後加交換點），不排 System 的先後（統合 O-3）。
- 系譜：Bevy `App::set_runner`（`app.rs:224`）、預設的 `run_once`（`:1516`）只 `update` 一次；現在 Kernel 的 `FinishContext::InstallRunner` 只記「誰裝」（`Registry.hpp:59-62`）。**退掉的**（檢查 S12）：「在 finish 裡裝」（Kernel 現在）——runner 屬於 `Product`，不屬於某個 World 的 Plan；Bevy 也在 `build` 裡設。「沒裝就是錯誤」——Bevy 預設 `run_once`，測試與工具不必寫 runner。

### 3.8 O8：準備時機（建 World 之後、第一次 Run 之前）

```cpp
using PrepareFn = std::function<Engine::Base::Result<void, OrchestrationError>(Kernel::World&)>;
void WorldSetup::Prepare(PrepareFn fn);
```

- 每個 World 建好之後（啟動時，以及每次替換），第一次 `Run` 之前，依序呼叫：**登記單位在 `Build` 裡給的**（照單位的順序）→ **描述裡產品給的** → （替換時）這一次替換帶的 `extra`。帶過去（3.12）排在這三者**之前**：準備函式看到帶過來的值已經在，放預設值時不能蓋掉它（契約，3.12）。
- 這時 World 是 Ready，`World&` 的 API 都可以用：`SetResource`（Resource 的預設值、暫存的存檔資料；配發器也是 Resource，Ready 時設定是 DI-KS 15 第 (7) 項的機制，範圍的下界就在這裡移）、`Spawn`／`Insert` 等外部寫入。Kernel 不知道這是「準備」，不多一項職責（L17）。
- 準備不能跑 schedule（它拿到的是 `World&`）；要在換上之前跑的 schedule（讀檔）寫在替換的請求裡（3.9）。
- 讀檔宣告的三件要求（5.1、5.8）由這裡給：設配發器範圍的下界、放暫存的存檔資料、放 Resource 的預設值。
- 名字（卡片照 L17 判）：`Prepare`，不叫 setup（N8 的「組裝（setup）」已指產品啟動的那一段）。
- 系譜：Bevy 在 `App::run` 之前由 Plugin 或 `main` 用 `insert_resource` 放（`crates/bevy_ecs/src/world/mod.rs:2056`；Bevy 沒有替換 World，只有啟動時）；flecs 在 `ecs_progress` 之前設值；Unity 在建 World 的 bootstrap 裡放（`ICustomBootstrap` 是建 World 群的入口，不是 World 建好之後的回呼——檢查 N3 更正）。**退掉的**（檢查 S12）：「不設準備清單，啟動時由持有者用 `Product::World(name)` 直接寫、替換時只收一個回呼」（Bevy 的寫法）——登記單位要在每個新 World 放自己的預設值（例：時間 Module 的 Resource）時沒有地方，而且啟動與替換會變成兩條路。

### 3.9 O9：執行中替換 World

```cpp
struct ReplaceRequest {
    PrepareFn extra;                          // 這一次的準備（讀檔：放這一份存檔、移配發器下界）
    std::vector<std::string> beforeSwap;      // 換上之前在新 World 上跑的 schedule（讀檔：Load）；第 5 條的生成世界接在這裡
    ReplaceOrder order = ReplaceOrder::BuildNewFirst;   // 或 DropOldFirst（使用者 2026-11-18：作者選、預設先建新的）
};
```

`Product::ReplaceWorld(name, request)`，只能在兩次 `Run` 之間呼叫（在任何一個 World 的 `Run` 裡是斷言；CCR-5）。先後由作者選（使用者 2026-11-18 定，`v8-arch-gyo2-replace-order-author-choice`；第 14 節第 4 件）。**預設 `BuildNewFirst`**：
1. **照產品描述重新登記**（使用者 2026-11-18 定，第 14 節第 3 件）：新建一份 `Kernel::Registry`，對這個 World 走一次 3.4 第 3、4 步——用到的登記單位依序 `Build`、產品的 `build`、`BuildPlans`、依序 `Finish`、依序 `Check`、清單檢查；`Configure` 不重跑（它是產品層一次的，遊戲初始化清單與 runner 不變）。任何錯誤：回傳 `ReplaceRegistrationFailed`（帶這一輪的報告），舊的照跑。再用同一份 `WorldOptions` 建新的 `WorldExecutor`。之前在這個 World 動態插入的 System（不論來自指令或遊戲本身）**一律不在**新 World。
2. 帶過去（3.12）：把標記要帶的 Resource 從舊 World **移**到新 World。
3. 準備：登記單位的 → 描述的 → `extra`。
4. 在新 World 上依序 `Run` `beforeSwap` 的每一張（遊戲初始化沒跑過時照 3.6 先跑它——只會在啟動之後立刻替換時發生）。
5. 全部成功：銷毀舊的 `WorldExecutor`、再銷毀舊的 Registry，新的放進同一個位置（同一個名字、同一份描述的每幀清單、同一個錯誤處理器）；先前從 `Product::World(name)` 拿到的參照作廢（說明頁寫明）。**任何一步失敗或丟例外**：帶過去的移回舊 World、銷毀新的、舊的照跑、回傳 `ReplacePrepareFailed`／`ReplaceScheduleFailed`（或把例外往外傳）並記一次警告。
6. 遊戲初始化**不重跑**（行程一次；第一個 World 被換掉也一樣）。中斷的 World、空的位置也能被換掉。

**`DropOldFirst`**（地圖大到兩個 World 同時放不下時；使用者點名的 BigMap，5.2）：
1. 帶過去（3.12）：把標記要帶的 Resource 從舊 World 移到 `Product` 的暫存處。
2. 銷毀舊的 `WorldExecutor`、再銷毀舊的 Registry；這個位置變成空的。
3. 照上面的 1、3、4 重新登記、建新的、準備、跑 `beforeSwap`（帶過去從暫存處移入，在所有準備之前）。
4. 全部成功：新的放進這個位置。**任何一步失敗或丟例外**：銷毀新的，位置保持空的，回傳對的碼並記一次警告；帶過去的值留在暫存處，下一次替換這個位置時照樣帶。runner 的收拾（說明頁寫明）：再呼叫 `ReplaceWorld` 填上別的內容（例：回主選單的 World、另一份存檔），或結束；空的位置在 `RunFrame` 回 `WorldMissing`。

**存檔讀不了**（使用者 2026-11-18）：「壞檔問題是最嚴重的Bug，通常是來自於系統更新…我認為這個是系統層級是無解的，只能在運營端處理，或者提供存檔轉換功能。通常製作的時候直接上version 管理。太老的檔案不支持。讓玩家自己開新檔。」——Engine 不做存檔的版本轉換（`save-unaware-means-format-only`）；Engine 做的是讓讀檔失敗是安全的：預設先建新的、舊 World 照跑、錯誤回給作者（side chat 的讀法，不是使用者的話）。判定裡的壞檔用「版本不支援」當例子。
- **存檔版本 Module 的拒絕接得住，不改介面**（第 5 條做那個 Module；本批只確認路）：拒絕出現在準備（`extra` 或登記單位的準備回錯誤）或讀檔的 schedule（System 回錯誤）。前者 `ReplaceWorld` 回 `ReplacePrepareFailed`、後者回 `ReplaceScheduleFailed`；`BuildNewFirst` 時舊 World 照跑，`DropOldFirst` 時位置是空的、`RunFrame` 回 `WorldMissing`；啟動時讀檔被拒是 O-E7。準備函式回傳的錯誤帶回報者自己的碼（字串，例：那個 Module 的「版本不支援」）與訊息，`ReplaceWorld` 原樣放進它回傳的錯誤裡，作者分得出是版本被拒還是別的失敗（和登記單位在 `Finish`／`Check` 回報的形狀一樣，3.5）。

**重新登記**（`v8-arch-gyo2-restore-reruns-registration`）：使用者：「讀檔應該只有一種解釋就是返回到某一時刻的狀態。」「乾脆讀檔就全部重來，也不管指令了，標準的回到某個狀態。作者想要測試，就自己設計測試流程來規避插入系統的重置。」——問到能不能分清指令輸入與遊戲中的設計（分不清：同一個插入入口，來源只能呼叫的人自己填；Bevy、flecs、Unity 都不分）之後：「對，分不清就別冒險，我們已經有入口了，用不用是作者的決定。」比過的：沿用同一份 Registry（本卡修訂 1 的建議）／重跑（flecs、Unity）←選／依來源分（指令保留並給還原指令、遊戲追加的還原；因分不清來源而不採）。
- 「全部重來」只到這個 World 的登記（照產品描述），不是重開行程：遊戲初始化照舊只跑一次（第 14 節第 1 件照舊要定）。
- 作者要讓插入的 System 跨過讀檔，就把「插了什麼」存成自己的資料、讀檔後再插一次（入口已有）。
- 登記單位的 `Build`／`Finish`／`Check` 對同一個 World 名會再被呼叫：Module 以 World 名為鍵記的東西，在 `Build` 時重設（說明頁與 O-2 寫明）。
- 代價：每次讀檔重跑這個 World 的所有 build、finish、check（含策略的檢查）；第 5 條的 extract 若以這個 World 的 Registry 為來源，Registry 重建後來源失效（2.2）。
- 讀兩次相同（判定 2 案例 5）照舊成立：同一份描述重新登記，得到同樣的 id 與 Plan。

**「經開局」對新 World 的意思**：`new-world-default` 的原話（11-16）是「建一個新的 World，經開局與 Loading，由作者的讀檔 System 把存檔內容放回」。當時「開局」是 Kernel 每個 World 一次的 Startup；之後 `game-init-list` 把它切成遊戲初始化（行程一次、第一個 World）與生成世界（每個 World 各自），讀檔宣告第 15 節的確認清單（使用者 2026-11-17 核准）寫明新 World 走生成世界、遊戲初始化不再跑。本批的讀法：**「經開局」＝經這個 World 的準備與生成世界，不經遊戲初始化**。生成世界之前（第 5 條），由 `beforeSwap` 代替。

**System 端的請求通道不在本批**：玩家在選單按「讀取」是 System 處理輸入，它要讓 runner 知道「換 World」。本批之前，作者用自己的 Resource，自己的 runner 在兩幀之間讀它、呼叫 `ReplaceWorld`。Engine 給的通道照 Bevy `AppExit`（`bevy_app` 登記的一種訊息，`app.rs:130`、`:1406` `should_exit`）——在第一個每幀迴圈的 runner 那一批做（第 3 條之後，訊息已改名 `Message`）。

### 3.10 O10：動態插入的編排半

`Product::InsertSystems(world, owner, build)`，兩次 `Run` 之間：
0. `owner` 和這個 Product 的任何登記單位或產品名相同：回 `OwnerTaken`（Kernel 只比 owner 相等，`InitialValue` 的「只有擁有者能宣告」會被冒用；檢查 N5）。
1. `registry.Register({owner, 序號}, build)`：回 `DynamicInsertRefused` → 經 `Log` 記一次警告（World 名、owner、「這個 World 宣告不准動態插入」），回傳 `InsertRefused`；什麼都沒變（CCR-3）。`build` 丟例外 → 3.4「例外」。
2. `registry.BuildPlans()`：失敗（例：R-E7）→ 記一次警告（帶 Kernel 的報告）、回傳 `InsertBuildFailed`；Registry 與 World 留在上一個版本（DI-KS 26）。
3. 成功：World 下一次被用到時自己接上（DI-KS 26）；編排層持有的唯讀登記表參照照 CCR-4 重讀。
- **不重跑 `Finish` 與 `Check`，也不通知**（使用者 2026-11-18 定，`v8-arch-gyo2-dynamic-insert-no-lifecycle-rerun`；**改寫** HANDOFF `v8-arch-gyo2-dynamic-insert-per-world` 與統合 O-2 草案的「執行中的插入（指令等）照同一條路再走一次」那半句）。使用者：「不重跑，理由還是之前的那個，如果作者有需求就自己設計。系統有支持，最關鍵的是，Build 、Finish、Check都屬於Module自己的東西，如果一個system可以重置Module的生命週期，會讓系統層級分不開。」比過的：不重跑、不通知 ←選／重跑（HANDOFF 原文）／只通知、不能否決／能否決（改第 1 批的兩步）。後果（side chat 補的，不是使用者的話）：Module 在 `Finish` 建的衍生資料插入後可能過期，Module 用登記表的版本號自己發現（CCR-4）；需要檢查保證的策略（伺服器權威）本來就要求它的 World 不准動態插入（權威份 4.5）。
- 動態插入的 System 在這個 World 被替換（讀檔）時一律重置（3.9，`restore-reruns-registration`）。
- 指令集從 System 呼叫插入，和 3.9 的通道一起留到每幀迴圈的 runner 那一批。

### 3.11 O11、O12：Kernel 拿掉的

| 拿掉 | 現在 | 之後 |
|---|---|---|
| `ScheduleOptions::startup`、D-E21、D-E12 的開局那一支（「startup schedule … has a clock or a domain」） | `Declarations.hpp:121-125`、`:55` 的註解；`Registry.cpp:939`、`:979-991` | 遊戲初始化清單（3.6）；「沒有 domain」那一支照留（第 3 條拿掉） |
| 執行期的開局：`WorldExecutor::RunStartup`、`startup_`、`StartupScope`、Run 開頭的自動觸發、主程式跑 Startup 的斷言；`World::startupRan_`、`startupRunning_` | `Executor.cpp:36-42`、`:129-136`、`:199-213`、`:233-236`；`World.hpp:475-477`；`WorldExecutor.hpp:40-43`、`:113-114`、`:144-145` | `Product::RunGameInit`／`RunFrame`／`RunSchedule` |
| `StartupNotNestable`、`StartupRunning` 與巢狀入口的兩個檢查 | `KernelError.hpp:64-67`；`Registry.cpp:33-34`；`Executor.cpp:513-524`；`detail/Context.hpp:205` 的註解 | 沒有 |
| 寫者計數的開局豁免 | `Registry.cpp:1461-1469`（DI-KS 6） | 沒有（到第 4 條拿掉 D-E2 為止，遊戲初始化的 System 照樣算） |
| R-E7 的「開局的 schedule」「開局 schedule 上的掛載」兩支 | `Registry.cpp:1624-1640` | 沒有 |
| `Plugin`、`FinishContext`（含 `InstallRunner`）、`Registry(std::vector<Plugin*>)`、`Registry::Finish()`、`PluginRecord`、`data.plugins`、`runners`、`CheckPluginList`、`CheckRunners`；R-E1、R-E3、R-E6 | `Registry.hpp:51-87`、`:657-705`；`detail/RegistryData.hpp:40-46`、`:143`、`:170-171`；`Registry.cpp:124-129`、`:175-181`、`:392-398`、`:702-715`、`:742-775`、`:1532-1538`、`:1813-1815`、`:1823-1860` | 編排層（3.3、3.4）；碼換成 O-E1、O-E3、O-E4 |
| 錯誤訊息與註解裡的「plugin」（例 `Registry.cpp:548`、`Scheduler.cpp:338`） | — | 改成「owner」「登記者」 |
| 指紋與匯出裡的 `startup` | `Fingerprint.cpp:194`、`:424`、`:432` | Registry 指紋 3→4、Plan 指紋 5→6（Kernel 不再在第一次 Run 前跑開局，照 hook-flush 定的「規則變就升版」）、C 指紋 7→8（共用 schedule 的位元組）、Plan 匯出 `gyo.kernel.plans` 3→4（`startup=` 那一欄拿掉）；golden 重產 |

留在 Kernel：`RegistrationOwner`、`Registry()`、`Register`／`BuildPlans`／`DiscardRegistrations`、`WorldOptions`、R-E2、R-E4、R-E5、R-E7（其餘理由）。Kernel 的測試輔助 `Finish(builds)`（`KernelTestSupport.hpp:143`）改成每一對 `(名字, build)` 一次 `Register`、最後 `BuildPlans`；用到 `Plugin::Finish` 或清單檢查的案例搬到編排層（第 7 節）。

### 3.12 帶過去（`v8-arch-gyo2-carry-over-mechanism`）

使用者 2026-11-18 定（第 14 節第 1 件）：換 World 時，標記的東西帶過去；「帶過去，但是是要建立機制，而不是單純的寫死一個搬運Object」。

```cpp
template <class T> void WorldSetup::CarryOver();   // Build 裡（登記單位或產品的 build）標記：這個 World 被換掉時，T 帶到新的 World
```

- **機制，不是寫死**：任何 Resource 型別都能被標記；編排層只存「型別的身分（`TypeKey`）＋一個型別化的搬移函式」的清單，替換時照清單泛用地搬，不認得任何特定的型別或容器。各 Module（狀態機、時間）要不要帶自己的 Resource，用同一個標記自己決定。（side chat 對「機制」的讀法，不是使用者的話。）
- **Engine 不看值的內容**：整個值照搬（`ResourceMut<T>` 取出、`SetResource<T>` 放進新 World，都是 Kernel 公開的 API），和 Unity `EntityManager.MoveEntitiesFrom` 一樣不解讀內容。**值裡不能存指向舊 World 的參照**（`EntityId`、指標、句柄）——這是作者的責任，寫在契約與 O-10；照 `guards-after-mechanism-stable` 不立實作面的守衛，靠文件與審查。
- **時機**：在所有準備之前（3.8、3.9）；`DropOldFirst` 經 `Product` 的暫存處。準備函式放預設值時要先看值在不在（帶過來的已經在），不能蓋掉——契約寫明，說明頁給寫法。
- **清單從哪來**：照**舊** World 的標記取出（重新登記之後新 World 也會有同一份標記，因為描述相同）；舊 World 裡沒有那個值的，不帶（新 World 照準備給的值）；新 World 沒有登記那個型別的，這次替換失敗（`ReplaceCarryOverFailed`）、值移回舊的（`DropOldFirst` 時留在暫存處）。
- **失敗**：任何一步失敗或丟例外，帶過去的值移回舊 World（`DropOldFirst` 時留在暫存處，下次替換照樣帶）。
- 帶過去的值在新 World 裡取得新的 stamp（`SetResource` 寫入當下），`Changed<T>` 對新 World 的第一批讀者成立。綁執行緒的 Resource 在呼叫的執行緒上重新綁定（同一條執行緒，第 13 節）。
- 只帶 Resource，不帶實體（遊戲初始化生成的實體不帶；要帶的作者存成 Resource 或自己重建）。
- 系譜：Unity Entities `EntityManager.MoveEntitiesFrom`（`Unity.Entities/EntityManager.cs:3941`）、`CopyEntitiesFrom`（`:3785`）在 World 之間搬資料、不解讀；Bevy、flecs 沒有（不換 World）。**退掉的**（使用者比過）：長留的放在 World 外、每個新 World 由準備放進去；第一個 World 不換。

---

## 4. 要改的程式與文件

| 檔 | 改什麼 |
|---|---|
| `engine/orchestration/`（新）：`CMakeLists.txt`、`include/engine/orchestration/` 的標頭、`src/` | O1～O4、O6～O10（檔名照 N16） |
| `engine/CMakeLists.txt` | 照 `modules.csv` 的列序 `add_subdirectory` |
| `engine/config/modules.csv`、`plugins.csv`（新）；`build/cmake/GyoUnitRegistry.cmake`（新）；`build/cmake/GyoBuild.cmake` | O5 |
| `engine/{base,math,entity_store,kernel,os,time,threads,io,audio}/CMakeLists.txt` | 引擎內的 `GYO::<Module>` link 換成 `gyo_link_registered_dependencies(<name>)` |
| `engine/kernel/` 的標頭與 `src/{Registry,Executor,Fingerprint,World,Scheduler}.cpp` | O11、O12（3.11 的表） |
| `tests/common/orchestration/`（新）；`tests/common/scenario/`（新）；`tests/common/cmake/UnitRegistryTests.cmake`（新）；`tests/common/Requirements.cmake`、`tests/common/CMakeLists.txt` | 第 7.3 節、第 8 節 |
| `tests/common/kernel/*` 與 `KernelTestSupport.hpp`、`FinishRecord.cpp`、`WorldExecRecord.cpp`、`ThreadBoundProbe.cpp`；`test_kernel_symbols.py` | 第 7.1、7.2 節 |
| `docs/architecture/plans/kernel/scripts/mutations.json` | 第 9 節 |
| `docs/architecture/target/kernel.zh-Hant.md`；`docs/architecture/target/README.md` 的目標 Design Invariants 清單 | 第 10.1 節 |
| `docs/architecture/target/v2/orchestration.zh-Hant.md`（新） | 第 10.2 節 |
| `docs/architecture/target/v2/system-contract-page.zh-Hant.md` | 說明頁加「編排層」一節：生命週期、產品自己的登記、遊戲初始化、準備、替換、動態插入的警告、`World()` 參照的期限、runner 用 `RunSchedule` |
| `docs/cross-component-rules.md` | CCR-3 的欄寫明編排層是第一個實作者 |
| `docs/dev_logs/`、`docs/architecture/target/v2/README.md` 的索引 | 本批的 dev_log |

刻意不改、會過時的舊文件（照第 1 批第 4 節的寫法）：`docs/architecture/plans/kernel/spec.zh-Hant.md` 的 C.2、C.5、H-12、H-13、R-E1／R-E3／R-E6、J-58 的條文——v2 之前的文件先不動（`pre-v2-target-docs-later`）。

---

## 5. 基盤 Architecture 的根據（AGENTS §2）

### 5.1 為什麼算基盤

編排層的登記單位型別、生命週期、`Product` 的介面與登記檔格式，上面會積：權威、回滾、時間、狀態機、網路、audio 這些 Module（全部以登記單位接進來）；生成世界、多個 World 與交換點、讀檔的實作（全部在 `Product` 上）；每一個產品的建置描述與它自己的登記。改它們，上面全要跟著改，而且積得越多改得越多。

### 5.2 予見的根據 → 基盤的要求

| 根據（出處；種類） | 對編排層的要求 | 這一批 |
|---|---|---|
| 玩家存檔、遊戲中讀檔（使用者 11-16；名指的 Feature；Product：單機） | 建 World 後、第一次 Run 前的時機；執行中替換 World；讀檔失敗不掉進半空的世界；遊戲初始化不重跑 | 做（3.8、3.9） |
| 指令集（使用者 11-17；名指；Product：未指定） | 插入入口；拒絕時回錯誤、記一次警告、World 照跑 | 做 Kernel 之外的那一半；System 端的通道留介面形狀 |
| 伺服器權威（使用者 11-11、11-16；名指；PvP Match） | 策略在建 World 時選；build → Plan → finish → 檢查；檢查讀 World 的宣告；Kernel 錯誤時不檢查 | 做時機與彙整；權威 Module 在第 4 條 |
| 單機不上權威（使用者 11-17 L15；名指；單機） | 產品能只選部分 Module；預設全帶可關 | 做（3.2） |
| 多個 World（使用者 11-11、11-14；名指；PvP Client、Editor） | `Product` 持有 World 群；每個 World 自己的登記與單位；每個 context 交出 World 的身分 | 做 World 群；交換點、執行緒在第 5 條 |
| 狀態機的「進入初始狀態」要讀到設定（使用者 11-15、11-16；名指；所有遊戲） | 遊戲初始化清單可以在指定的 schedule 前後插入 | 做（3.6） |
| 產品自己的遊戲程式（每個產品；AGENTS §7） | 不改共通層就能登記遊戲本身 | 做（3.3） |
| 每幀迴圈由視窗的 Plugin 推（參考實作 Bevy `WinitPlugin` 設 runner） | runner 由登記單位裝、一個 Product 一個 | 做介面與只跑一次的 runner |
| BigMap：約 1 km²、高建築密度的大地圖（使用者點名，計畫在舊架構 master `docs/architecture/plans/big-map/`，`v8-arch-big-map-density`、`v8-arch-big-map-sim-model`；區塊化地圖 242 MB、碰撞盒約 103 萬個、Match 整張載入碰撞；Product：PvP） | 換到另一張大圖時兩個 World 不能同時在記憶體：替換要能「先丟舊的」 | 做（3.9 `DropOldFirst`）；大型靜態地圖放在 World 外共用留給資產或地圖那一批（2.2） |
| 讀取畫面、分批載入（使用者 11-11；名指；PvP Client） | 生成世界在編排層、每個 World 各自 | 不做（第 5 條）；留 `beforeSwap` 與啟動時的同一個位置 |
| Module 之間的依賴（使用者 11-17 L20） | 依賴只寫在登記檔；編排層讀出並檢查 | 做（3.2） |
| 參考實作（Bevy 6a911653f `bevy_app`；Unity Entities 1.5.0；flecs；O3DE、Unreal 的清單機制只讀過文件與統合的證據） | 生命週期分 build／finish；預設 run-once；開局只跑一次、只在主 World；清單產生接上的程式 | 「怎麼做」的根據，見各節系譜 |

規模與預算：本批不碰執行效率。`BuildNewFirst` 時替換的當下兩個 World 同時在記憶體裡（峰值約為一個 World 的兩倍）；BigMap 的規模（舊架構的計畫：約 1 km²、區塊化地圖 242 MB、碰撞盒約 103 萬個）兩份同時放不下，所以要有 `DropOldFirst`。GYO2 還沒有 World 大小的預算數字，本批不量。

### 5.3 Feature 與 Product

| Feature | Product | 誰點名 |
|---|---|---|
| 玩家存檔（含遊戲中讀檔） | 單機遊戲 | 使用者 11-16 |
| 指令集 | 原話沒有指定 | 使用者 11-17 |
| 伺服器權威 | PvP Match／Client | 使用者 11-11、11-16 |
| 不選權威時沒有限制、只選部分 Module | 單機 | 使用者 11-14、11-17 |
| 多個 World | PvP Client、Editor | 使用者 11-11、11-14 |
| 遊戲初始化可插入 | 所有遊戲（狀態機的例子） | 使用者 11-16 |

---

## 6. Architecture Delta（AGENTS §3 的七項）

1. **需要它的 Feature 或目的**：第 5.3 節；批次表第二段第 2 條。
2. **現在的問題**：Kernel 同時是 ECS 與組裝層——它認得開局（DI-KS 22）、登記單位（`Plugin`、`Requires`、`InstallRunner`），和「Kernel 的職責是 ECS」（L17）、「編排層認得階段、Kernel 不認得」（`two-layers-like-bevy`）相反；讀檔要的時機與替換沒有地方放；第 1 批留下的 `DynamicInsertRefused` 警告沒人記；依賴寫在登記檔又寫在 CMakeLists 會是兩處。
3. **變化的邊界**：新 Module `engine/orchestration`（新的 Subsystem）；Kernel 的公開介面減少（3.11）；建置層新增 Module／Plugin 登記檔與 `gyo_link_registered_dependencies`、`gyo_product_units`；`engine/` 的子目錄與各 Module 的 CMake 依賴改由登記檔產生（Build Graph 的構造變更）；Registry、Plan、C 指紋與 Plan 匯出的格式。
4. **影響的 Module／Product／Tool**：`engine/kernel`；新 `engine/orchestration`；`engine/` 下九個 Module 的 CMakeLists（只換寫法，依賴邊不變）；`build/cmake`；`tests/common/kernel`、新 `tests/common/orchestration`、`tests/common/scenario`、`tests/common/cmake`；突變清單；之後的每個 Module 與產品。gyo2 上沒有產品與工具。
5. **Dependency Direction**：新增 編排層 → Kernel、編排層 → Base；產品的 `<target>_units.cpp` → 編排層與它帶上的登記單位。Kernel 不依賴編排層；編排層不依賴任何 Module 的內部、任何第三方。引擎內既有的依賴邊一條都不變（判定 1 的 G-3 一次性比對）。
6. **Ownership**：登記單位與生命週期、runner、遊戲初始化、World 群、準備、替換、動態插入的警告、啟動錯誤的彙整：Kernel（或沒有人）→ 編排層。Module 之間依賴的事實：各 CMakeLists → 登記檔。owner 都是架構（架構師會話）。
7. **為什麼不能更小**：拿掉開局與 `Plugin` 而不建編排層，Kernel 的測試與遊戲初始化沒有新家（統合第 8 節「拿掉某條限制和接手它的新家放在同一條線」）；只建 `Product` 不做登記檔，依賴只能寫在程式碼（`Requires`），和 L20 相反；登記檔寫依賴而 CMakeLists 不讀，依賴兩處（AGENTS §9）；準備與替換不在本批，讀檔的實作（第 5 條）沒有前提。比過、沒採的較小做法：「`Kernel::Plugin` 原地留著、編排層包一層」——Kernel 照樣認得登記單位與 runner，L17 的職責清單不成立。

**和統合（與批次表、讀檔宣告）不同的**：
1. 動態插入不重跑 `Finish` 與 `Check`、也不通知（使用者 2026-11-18 定 `dynamic-insert-no-lifecycle-rerun`，改寫統合 O-2 草案與 HANDOFF `dynamic-insert-per-world` 的「照同一條路再走」；3.10）。
2. 登記單位多一個產品層一次的 `Configure`；產品自己的遊戲程式由描述的回呼登記（3.3）。
3. runner 在 `Configure` 裝，不在 finish（3.7）。
4. golden 重產多一次（統合第 8 節算三次，3.11）。
5. `engine/` 的子目錄與各 Module 的 CMake link 改由登記檔產生（統合 L20 只說「兩邊讀的格式」；3.2）。
6. 準備時機的形狀（統合修訂 33 交本批定；3.8）與 World 替換（讀檔卡修訂 11 的要求；3.9）。替換的先後做成作者選、預設先建新的（使用者 2026-11-18 `replace-order-author-choice`）；讀檔宣告第 4、15 節的字面「丟掉現在的 World、建新的」是 `DropOldFirst` 那一條。
7. O-1 的文字：統合 9.2 草案寫「跑在主 World」「不保證清單內的先後」；本宣告寫「第一個 World」（使用者 `game-init-list` 的原話）與「保證插入定下的順序」（3.6；草案的「不保證」指作者沒寫先後的部分，本宣告的清單是作者用插入排出來的，順序就是作者給的）。O-4 的職責清單多寫「World 的準備與替換」「編排層自己的警告」（第 10.2 節、第 14 節的「一起確認」）。
8. 時間估計 18～26 h（統合第 8 節 10～14 h；第 12 節）。
9. 替換 World 時重建它的 Registry（使用者 2026-11-18 `restore-reruns-registration`；3.9）：讀檔宣告與統合都沒有寫登記怎麼處理，第 5 條的 extract 要接這個後果。

---

## 7. 守衛與測試的變更（AGENTS §14.4；`v8-arch-gyo2-guards-after-mechanism-stable`）

**兩類**（使用者 2026-11-18，經主會話轉交；AGENTS §3「ガードを立てる時期」，文件分支 0f400dd3）：
- **行為的測試**照常寫、當判定，檢出力照 §14.2 用突變證明：scenario test（第 8 節）、讀檔案例、編排層的行為案例（7.3）、Kernel 既有案例的改寫。
- **綁實作方式的守衛**（掃描名字、符號、結構、連結字面的腳本，與守它們的突變）**不新增**，列進 7.4 的延後清單；條件是「涵蓋它的 scenario 段通過」。這段期間 Design Invariants 照寫進文件（第 10 節），靠審查守。
- **第 1 批已立的守衛不拆**，照舊跑；被本批拿掉的碼或名字逼著要同步的（例：DI-KS 15 的碼清單、`test_kernel_symbols.py` 裡的 `Plugin` 例），只做同步。

盤點：本卡以案身掃描（`\.startup\b|startup = true|Plugin|FinishContext|\.Finish\(\)`）加讀測試寫成（檢查 M9：修訂 0 只掃標題，漏了三案）；逐案的處置在實作開始時由實作者照組再核一次，和表不同的照 AGENTS §14 停下回報。**緩的面**＝以前判失敗、之後判通過的情形；**緊的面**＝以前判通過、之後判失敗的情形。

### 7.1 有緩的面的（要你逐組核准）

| 組 | 1. 守的性質 | 2. 緩的面 | 3. 緊的面 | 4. 性質之後由誰守 | 案例（處置） | 5. 根據 |
|---|---|---|---|---|---|---|
| A 開局（**DI-KS 22 的守衛**） | 一個 World 至多一張 Startup；Kernel 在第一次跑別的 schedule 前、同一次 Run 裡、推 tick 之前跑完它一次；System 不能跑它、它執行中不能跑別的；主程式明確呼叫只准一次 | 一個 World 可以有多張「只跑一次」的 schedule；System 可以巢狀跑遊戲初始化的 schedule、遊戲初始化中可以巢狀跑別的；直接呼叫 `WorldExecutor::Run` 不會先跑遊戲初始化；第二個以後的 World 不再各自自動跑一張 | 清單的順序（含插入）成為守的性質；錨點不在、名字不是第一個 World 的 schedule 是啟動錯誤（O-E5）；遊戲初始化的 schedule 要屬於一個 domain（第 3 條之前，3.6） | 「整份一次、在第一幀與 `RunSchedule` 之前、只在第一個 World、明確呼叫只准一次、錯誤時不跑每幀」：編排層 O-1 的行為案例（7.3）與 scenario 第 1 段。巢狀與 tick：不由機制守（Kernel 不認得開局） | `NestedRunTests` :1370、:1425、:1474、:1480、:1486（改寫搬到編排層）；:1393（拿掉：tick 隨第 3 條）；:1410（拿掉：任何 schedule 的性質，`HookFlushTests` :337 改寫後守）；:1433、:1450、:1464、:867（拿掉）；:1500（拆：「先於每幀」搬走；clock／巢狀拿掉）；:1792、:1832（拿掉：隱含觸發沒有了） | `game-init-list`、`two-layers-like-bevy`；統合 7.2 J-58 列 |
| B 登記單位的清單（J-19 的登記單位那半、J-24 的 R-E1／R-E3／R-E6；**含 DI-KS 25、26 的守衛**） | 清單裡名字重複是 R-E1、重複的不 build；`Requires` 缺或在後是 R-E3；兩個 runner 是 R-E6；build 每個一次、全部 build 完才 finish；`Plugin::Finish` 讀到版本 0；`Finish()` 只能是第一輪 | 不經編排層、直接用 Kernel 兩步登記的程式不受這些檢查（Kernel 不認得登記單位）；「`Plugin::Finish` 讀到版本 0」「`Finish()` 只能是第一輪」不再成立 | 依賴的來源從程式碼改成登記檔：Module 依賴 Plugin 是 O-E3（新）；`Finish` 讀得到 Plan 與版本 1；插入後不重跑 `Finish` | 編排層 O-2 的行為案例（O-E1、O-E3、O-E4、順序、「R-E1～R-E4 同一份報告」照舊一起收齊，7.3） | `RegistryTests` :142、:164、:177、:227 的 R-E6 段、:251 的 R-E1／R-E3 段（搬）；`CoverageTests` :86 的 `FinishContext` 段（搬：`WorldFinish` 不能複製、移動）、:130（搬：`Finish` 看得到解析後的集合與存取）；`RegistrationRoundTests` :735「Finish is only the first round」（拿掉）、:836（拿掉；編排層新案「插入之後不重跑 `Finish`」）、:901 的「version inside Plugin::Finish」段（拿掉；編排層新案「`Finish` 讀到版本 1」；同一案的「之後每次成功加一、失敗不加」留在 Kernel） | `module-deps-in-registers`、`module-plugin-registers`；統合 L5、L20、7.2 J-19、J-24 列 |
| C R-E7 的開局兩支（**DI-KS 26 的守衛**） | 之後一輪登記開局的 schedule、或在它上面掛 System，是 R-E7 | 沒有「開局的 schedule」，之後一輪登記任何 schedule 都照一般規則 | 無 | 不由機制守（概念沒有了） | `RegistrationRoundTests` 的三處（拿掉開局那兩支的斷言，其餘留） | 同 A |
| E 開局的寫者豁免（**DI-KS 6 的守衛**；檢查 M9 第 4 點更正：修訂 0 歸在「沒有緩的面」不成立） | 只掛在 Startup 的 System 不算寫者；Startup 列的 `Writes<…>` 不算 | **對實作**：把開局的 System 算成寫者的實作（突變 `rs-j15-startup-counted`）以前判失敗、之後判通過 | **對作者**：遊戲初始化與每幀都寫同一個 Exclusive 的欄，以前通過、之後是 D-E2（到第 4 條拿掉 D-E2 為止） | 不由機制守（Kernel 不認得開局；寫者數量本身在第 4 條拿掉） | `ZoneRuleTests` :188（拆：Startup 那半拿掉、Shared 那半留到第 4 條）；`NestedRunTests` :1918（拿掉） | `two-layers-like-bevy`；統合 3.2 第 4 列 |
| F 開局裡改 `KernelSettings`（**DI-KS 24 的守衛**；同上更正） | 開局在同一次 Run 裡先跑時，它改的 `KernelSettings` 從下一次 Run 才生效 | 把「開局改的值在同一次 Run 就生效」的實作（突變 `ks-s7-startup-change-same-run`、`ns-st-settings-after-startup`）之後判通過 | 無 | 「下一次最外層 Run 生效」由 J-50 的一般案守；遊戲初始化每張是獨立的 Run，編排層加一案「遊戲初始化改的設定從下一張生效」 | `NestedRunTests` :1324（拿掉） | `kernel-settings-read-write` |

### 7.2 沒有緩的面的（同一個性質換觀察方式、期待值隨定義變、或只緊不緩）

§14.4：一般測試由實作者改、事後報告；**Design Invariants 的守衛測試不論有沒有緩的面都要你核准**——下表的 D、I 與 G 列都在第二段核准裡。

| 組 | 案例 | 改什麼 | 為什麼沒有緩的面 |
|---|---|---|---|
| D DI-KS 15 的碼清單 | `RegistryTests` :274、:288 | 清單拿掉 D-E21、R-E1、R-E3、R-E6；「隨哪一批拿掉」那一列刪掉 | 准許的碼變少，只緊（已立的守衛同步，不是新守衛） |
| G hook 清空輪跑一次的 schedule | `HookFlushTests` :337、:754（檢查 M9 補） | 拿掉 `startup = true`，改成 `domain = Frame`、`clock = false`（3.6），斷言不變 | 同一個性質（任何 schedule 最後一個同步點的 hook 紀錄由它自己的清空輪套用；清空輪之後重取快照） |
| H 測試輔助改兩步 | `KernelTestSupport.hpp:143` 與用它的全部案例；`FinishRecord.cpp`、`WorldExecRecord.cpp`、`ThreadBoundProbe.cpp` | `Finish(builds)` 改成每對一次 `Register`、最後 `BuildPlans`；`TestPlugin` 刪掉 | 輔助沒有用到 `Plugin::Finish`；Registry 指紋雜湊的是 owner 的名字（`Fingerprint.cpp:285-286`），名字與順序照舊，id 與指紋的輸入不變 |
| I 指紋與匯出（DI-KS 3） | `RegistryTests` :43；`SchedulerTests` :152、:618、:694（Plan 匯出：`gyo.kernel.plans 3`、`fingerprint 5:`、`startup=0` 那一行；檢查 M9 補）；`HookFlushTests` :449、:865；`SpawnRejectTests` :789；`InitialValueTests` :684、:897；`CoverageTests` :501 | golden 與版本（Registry 4、Plan 6、C 8、匯出 4）；匯出的 schedule 那一行拿掉 `startup=` | 格式改版，判定（跨平台一致、決定性、不等式）不變 |
| J 符號掃描的自測試 | `test_kernel_symbols.py` 裡的 `Plugin` 例 | 換成留下的名字 | 已立的守衛同步，判定不變 |

（修訂 0 的組 J「`kernel_guard.py`、`naming_guard.py`、`kernel_symbols.py` 的掃描根加 `engine/orchestration`」**拿掉**：K1～K8 是 Kernel 的 DI 的守衛，K3（禁執行緒與時鐘）、K5（include 限 Kernel 的三個目錄）、K6（只准 `Executor*` 寫 log）沒有編排層 DI 的根據、和本宣告的設計相衝；也是延後的那一類。檢查 M5。）

### 7.3 新增的行為案例

Red 的做法（§14.2、§14.3）：新介面先加宣告與空的實作（建置得過、行為沒有），新案在它上面跑出失敗，再補實作看到通過；每個新案至少被一個專屬的突變在預期的訊息下殺掉（第 9 節、G-4）。測試用合成的登記單位、合成的登記檔與產品自己的回呼（AGENTS §7）。

| 項 | 案例（`tests/common/orchestration/`） |
|---|---|
| O-2 生命週期 | 兩個 Module、一個 Plugin、產品的回呼、兩個 World：`Configure` 各一次、在任何 `Build` 之前、產品的在最後；每個 World 裡 `Build` 照順序（Module 先、產品的最後）、全部 build 完才 `BuildPlans`、之後 `Finish` 照順序、全部 finish 完才 `Check`；`Finish` 讀到版本 1 與 Plan；三個 context 交出的 World 名對；World 的 `units` 只列一個時另一個不 build；沒有任何登記單位、產品回呼是空的 World 照樣建得起來（檢查 S2） |
| O-5 彙整 | 一個 World 的 Kernel 啟動錯誤加另一個 World 的 `Check` 錯誤加 O-E5 加 O-E3：一份報告都在；Kernel 失敗的那個 World 的 `Finish`、`Check`、清單檢查沒被呼叫、報告寫「未檢查」；沒有任何 World 被建、沒有任何準備被呼叫 |
| O-E1～O-E6 | 每個碼一個最小的 `Product`；O-E1～O-E6 全部出錯加一個 Kernel 錯誤的一份描述一次回報；O-E7 一個最小的 `Product`（另一份報告，3.4 第 6 步） |
| 依賴（O-E3） | 依賴缺、依賴在後（不重排）、Module 依賴 Plugin、不進 World 的依賴視為在、World 的 `units` 漏掉依賴（另一個 World 有也不算） |
| 選項包與 World 宣告 | Module 在 `Build` 讀到描述給它的選項；`Check` 拿到的 `dynamicInsert` 與選項和之後 `World::Options()` 相同 |
| O-1 遊戲初始化 | 清單三張、一張 `InsertBefore`、一張 `InsertAfter`、兩個單位插同一個錨點：跑的順序；錨點不在：O-E5、那一筆沒插；同名兩次：跑兩次；只在第一個 World 跑；`RunFrame` 兩次只跑一次、在第一幀的第一張之前；`RunSchedule` 也先跑它；先 `RunGameInit` 再 `RunFrame`：一次；`RunGameInit` 第二次、`RunFrame` 之後再 `RunGameInit`：斷言；一張回 Err：`RunFrame` 回它、每幀清單一張都沒跑、處理器只被呼叫一次、之後 `RunFrame` 回 `WorldInterrupted` 不呼叫 Kernel；清單空：`RunFrame` 直接跑每幀；遊戲初始化改的 `KernelSettings` 從下一張生效 |
| O-3 runner | 沒人裝：`Run()` 跑遊戲初始化與一幀、回 Success；裝了：runner 被呼叫一次、拿到同一個 `Product`；兩個裝：O-E4；`Run()` 第二次斷言 |
| O-8 準備 | 準備的順序（登記單位照順序、再描述的）；準備時 World 是 Ready、還沒有任何 schedule 跑過；第一幀的 System 讀到準備放的值；準備回 Err：O-E7 |
| O-9 替換 | 先動態插入一次（版本 2），再換：新 World 的 `Types()` 是新的物件、版本 1、插入的 System 不在；登記單位的 `Build`／`Finish`／`Check` 對這個 World 各被呼叫第二次、`Configure` 沒有；重新登記時 `Check` 回錯誤：`ReplaceRegistrationFailed`、舊的照跑；舊 World 被銷毀（解構會計數的 Resource）；舊的實體與被丟的紀錄都不在、Resource 回到準備給的值；遊戲初始化沒有重跑；每幀清單與錯誤處理器照舊；`extra` 在登記單位與描述的準備之後；`beforeSwap` 在準備之後、換上之前；`extra` 回 Err、`beforeSwap` 的 schedule 回 Err、`extra` 丟例外：舊的 World 照舊（實體、Resource、還能再跑一幀）、回傳對的碼、警告一次；`DropOldFirst`：舊的在新的 Registry 建之前就銷毀（解構計數的先後）、成功時結果和預設相同、失敗時位置是空的、`RunFrame` 回 `WorldMissing` 且別的 World 照跑、再 `ReplaceWorld` 填上之後正常；換掉一個中斷的 World；在 Run 裡呼叫：斷言 |
| O-10 動態插入 | 准的 World：成功、新 System 下一幀跑；不准的 World：回 `InsertRefused`、`Log` 的處理器收到一次 `Warning`（帶 World 名與 owner）、World 照跑、版本不變；替換之後仍被拒、仍是一次；建 Plan 失敗（R-E7）：回 `InsertBuildFailed`、警告一次、World 留在舊版本；`build` 丟例外之後 `ReplaceWorld` 正常（暫存副本已丟）；owner 和登記單位同名：`OwnerTaken`；`Finish`／`Check` 沒有被再呼叫 |
| 預設全帶（產生的表） | 合成的登記檔建一個測試 target：預設帶進 World 的 Module、不帶 Plugin；`DISABLE_MODULES` 關一個；`PLUGINS` 帶 Plugin；關掉被依賴的那個：configure 失敗（`cmake -P` 驗）；手寫的 `RegisteredUnits` 少了被依賴的：`Product::Build` 回 O-E3 |
| 登記檔格式（`cmake -P`） | 表頭不對、名字重複（跨兩份）、依賴不存在、依賴在後、Module 依賴 Plugin、`library_only` 和 `unit`／`header` 對不上：各自 `FATAL_ERROR` 且訊息含檔名、列、原因；正確的檔通過；`gyo_link_registered_dependencies` 產生的 link 和改之前的一樣（既有的 `gyo_expect_links` 五處照舊通過） |
| 讀檔（**讀檔宣告要求的案例**） | 第 11 節判定 2 |

### 7.4 延後的守衛（綁實作方式；條件＝涵蓋它的 scenario 段通過）

| 守衛（含守它的突變） | 守什麼 | 涵蓋它的 scenario 段 | 何時立 |
|---|---|---|---|
| `naming_guard.py` 的 `ROOTS` 加 `engine/orchestration`（目錄↔namespace） | N17 | 第 1 段（本批） | 第 3 批開頭 |
| `engine/orchestration` 的 include 掃描（只准 `engine/{kernel,base,orchestration}/` 與標準庫，含 `#include <第三方>`） | O-4（不帶第三方、不依賴 Module 的內部） | 第 1 段 | 第 3 批開頭 |
| `engine/orchestration` 不讀 Plan 的步驟、不呼叫 System 的掃描 | O-3 | 第 1 段 | 第 3 批開頭 |
| 引擎各 CMakeLists 沒有引擎內的 `GYO::<Module>` link（只看 `target_link_libraries(` 的引數，不看 `ALIAS` 與註解） | O-6（依賴只在登記檔） | 第 1 段 | 第 3 批開頭 |
| `gyo_expect_links(gyo_orchestration GYO::Base GYO::Kernel)` | O-4 的依賴 | 第 1 段 | 第 3 批開頭 |
| Kernel 裡不再出現 `Plugin`、`FinishContext`、開局識別字的常駐掃描（判定 1 的 G-3 是一次性的，不常駐） | DI-KS 15「不認得開局、登記單位、runner」 | 第 1 段 | 第 3 批開頭 |

**「第 3 批開頭」**（卡片照 L17 判；主會話要求標出）：條件在本批 scenario 第 1 段通過時就滿足，但本批還有第二段核准——使用者在那時退回某一組、設計就還會動；守衛立在第二段核准、合併之後的下一批開頭，避免「設計一改就碰到剛立的守衛」（使用者的原話：「在設計都不知道行不行就建立守衛，結果，要推翻設計，直接碰一堆的規則」）。第 3 批若改到上面某條守衛所守的設計（例：runner 的形狀），那一條再往後延到涵蓋新設計的段通過。

---

## 8. scenario test 第一段（`v8-arch-gyo2-scenario-tests`）

### 8.1 是什麼、放哪

照遊戲流程寫的整合測試，用一個合成的小遊戲把機制通過一次；**每批往上加一段**，前面的段一直要通過。限制（使用者決定，經主會話轉交）：只放在 `tests/`（`tests/common/scenario/`，ctest 名 `scenario.*`，標籤 `cpu`）；沒有畫面與輸入；不是產品（不走產品登記）；**不帶任何舊遊戲的程式**，只照新的契約寫。名字叫 scenario test（不叫 E2E、use case test）。

**小遊戲「走路計分」**（`tests/common/scenario/WalkerGame.hpp`；產品自己的登記回呼，3.3）：
- 元件 `Position`、`Velocity`、`Player`；Resource `Score`、`BestScore`；配發器：範圍型（`[1, 上限]` 起始）。
- System：`Walk`（讀 `Velocity`、寫 `Position`）；`CountScore`（寫 `Score`；第一次加分時經 `Commands` 放 `BestScore`）；`Boot`（`ExclusiveWorld`，`SetResource(Score{0})`）；`PlacePlayer`（`Spawn` 玩家在 (0,0)、速度 (1,0)）；`LoadSave`（`ExclusiveWorld`，見 S1-7）；之後動態插入的 `Accelerate`（寫 `Velocity` 為 (2,0)，掛在 `Walk` 之前）。
- schedule：`Boot`、`PlacePlayer`、`Load`（`Domain::Frame`、不當 clock）；`Play`（Frame 的 clock；`Walk` 然後 `CountScore`）。遊戲初始化清單＝`Boot`；每幀清單＝`Play`。
- **第 2 批的 Engine 上不用 `Shared` 繞路**（主會話照 L17 判）：每個欄只有一個簽名寫者——`Score` 由 `CountScore` 寫，`Boot`、`LoadSave` 用 `ExclusiveWorld`（不算寫者，DI-KS 6）；`BestScore` 經 untyped `Commands`（不算）；`Velocity` 只有 `Accelerate`；放回的實體由 `ExclusiveWorld` 生成。所以 D-E2 不會擋，也不碰已決定拿掉的 `Shared`。

### 8.2 步驟表（第一段）

「契約」欄是那一步依據的條文；**對照不到條文的步驟本身就算第三種**（8.4）。「別家」欄照 `scenario-cross-engine-check`（Bevy、flecs、Unity Entities，含官方或主流生態的套件；出處在證據 `scenario-cross-engine.md`）。分類：**全支持**＝細節照 L17 自己判；**分歧**＝交使用者、使用者定之前不進判定（使用者先前已定的寫明出處）；**無人支持**＝卡片重看情境、改寫或拿掉（第一段沒有）。

| # | 步驟（檢查） | 契約 | 涵蓋的第 1 批機制 | 別家 | 分類 |
|---|---|---|---|---|---|
| S1-1 | `Product::Build`：產品回呼登記上面的型別與 System，建 Plan；遊戲初始化清單有 `Boot` | O-2；DI-KS 26（兩步） | 1 登記與建 Plan 兩步 | Bevy `add_systems` 後 `Schedule::initialize`（`schedule.rs:637-650`）；flecs 有改動才重建 pipeline（`pipeline.c:297-311`）；Unity `CreateSystem` 後首次 update 排序 | 全支持 |
| S1-2 | `RunSchedule("Main","PlacePlayer")`：遊戲初始化先跑（`Score` 是 0），玩家在 (0,0)、id 1 | O-1；DI-KS 9（同步點後成員可見） | — | Bevy `Startup` 只在第一次（`main_schedule.rs:289-297`）；flecs `OnStart`；Unity bootstrap 或 `OnCreate` | 全支持 |
| S1-3 | `RunFrame` ×5：(5,0)、`Score` 5；第一幀之後 `BestScore` 存在 | O-1；DI-KS 15 (5)（`Commands` 增刪 Resource、同步點套用） | 4 一般 System 經 `Commands` 增刪 Resource | Bevy `Commands::insert_resource`（`commands/mod.rs:816`）；flecs 延後模式的 singleton 設值；Unity `EntityCommandBuffer`（singleton 是實體） | 全支持 |
| S1-4 | `InsertSystems("Main","console", Accelerate)`；`RunFrame` ×5：(15,0)、`Score` 10 | O-10；DI-KS 26（准的 World 在 Ready 時再登記、下次被用到時接上） | 2 准動態插入的 World 再登記一個 System | Bevy `Schedules` 是 World 裡的 Resource、可再 `add_systems`；flecs `ecs_system_init` 任何時候；Unity 隨時 `CreateSystem` | 全支持 |
| S1-5 | 存檔：讀出玩家的 id、位置、`Score`、`BestScore`、配發器的下一號，寫進作者自己的 `SaveData`（速度不存：存檔的核心是玩家狀態，`new-world-default`） | `save-unaware-means-format-only`（格式是作者的） | — | Bevy `DynamicWorldBuilder`（`dynamic_world_builder.rs:373`）；flecs `ecs_world_to_json`；Unity `SerializeWorld`（`SerializeUtility.cs:739`） | 全支持 |
| S1-6 | `ReplaceWorld("Main", {extra: 放 SaveData、配發器下界＝下一號、Score 預設 0; beforeSwap: Load})`（預設 `BuildNewFirst`）；S1-6c：同一步用 `DropOldFirst`，之後 S1-7～S1-9 的結果相同 | O-7、O-8；讀檔宣告 5.1、5.8 | 5（換 World 之後在兩次 Run 之間能再登記，見 S1-9） | 丟掉 World 建新的：flecs `ecs_fini`＋重新 import、Unity 新 World＋bootstrap（`DeserializeWorld` 只收空 World）、**Bevy 沒有**；先後：Unity 常見先建新的、Unreal 非 seamless travel 先拆舊的 | 分歧——**使用者已定**「一律在新的 World 讀」（`restore-entry-new-world-default`）與先後由作者選、預設先建新的（`replace-order-author-choice`） |
| S1-7 | `LoadSave`：`ExclusiveWorld` 當場 `SpawnWithId` 照存檔編號放回玩家（id 1、(15,0)、速度是預設的 (1,0)），同一個 System 裡馬上看得到；`Score` 改成 10、`BestScore` 放回；再當場 `Spawn` 一個「游標」實體，id ≥ 下一號 | DI-KS 18（`ExclusiveWorld` 當場改成員）；讀檔宣告 5.1 | 3 `ExclusiveWorld` 當場生成 | 當場生成：Bevy `&mut World` 的 `spawn`、flecs `immediate` System、Unity 主執行緒的 `EntityManager`——全支持。照存檔編號放回：Bevy `World::spawn_at`（`world/mod.rs:1076`）、flecs `ecs_make_alive`（`flecs.h:3343`）、Unity 沒有（只重新配號） | 當場生成：全支持。照編號放回：分歧——**使用者已定**兩種寫法由作者選（`restore-entry-ids-author-choice`） |
| S1-7b（分支） | 同上，但 `extra` 忘了移配發器下界、改在準備裡直接 `Insert` 放回玩家（外部寫入不檢查範圍）：`LoadSave` 當場 `Spawn` 拿到配發器給的 id 1（活著）→ 回 `AllottedIdAlive`，`LoadSave` 處理這個錯誤、World 不中斷、記一筆 `RejectedSpawn` | DI-KS 18 的 `AllottedIdAlive` 那一句；error-handling 規則 1 第 3 步 | 3 同一個 id 還活著時當場 `Spawn` 回 `AllottedIdAlive` | Bevy `spawn_at` 對已生成的 id 回 `SpawnError::AlreadySpawned`（`entity/mod.rs:1186`），配發器發到手動生成的號會 panic（`world/mod.rs:1063-1067` 的說明）；flecs `ecs_make_alive` 以 `ecs_check` 擋（`src/entity.c:2795`）；Unity 沒有可比的情形 | 分歧——**使用者已定**回錯誤碼、不回空（`immediate-spawn-returns-error`） |
| S1-8 | 換上之後 `RunFrame` ×5：(20,0)（「加速」不在，速度是 (1,0)）、`Score` 15；新 World 的唯讀登記表裡沒有 `Accelerate`；遊戲初始化的計數仍是 1；舊 World 已銷毀 | O-1、O-8；`restore-reruns-registration` | — | 重新登記：flecs、Unity 都重跑（`scenario-cross-engine.md`） | 分歧——**使用者已定**重跑（`restore-reruns-registration`） |
| S1-9 | 換上之後、兩次 Run 之間，再 `InsertSystems` 加回 `Accelerate`：成功；下一幀 (22,0) | DI-KS 26 規則 4（正在 Run 的 World 數為 0 時才能登記） | 5 正面：兩次 Run 之間能登記（在 Run 裡呼叫是斷言，情境走不進去，照舊由 `RegistrationRoundTests` 的單元測試守） | 同 S1-4 | 全支持 |
| S1-10 | 遊戲初始化在第一個 World 放 `KeyBindings`（產品的 build 標記帶過去），第 3 幀模擬選項選單把它改掉；換 World 之後 `KeyBindings` 是改過的值，`Score` 照存檔（不帶） | 3.12；O-10 | — | 帶過去：Unity Entities `EntityManager.MoveEntitiesFrom`（`EntityManager.cs:3941`）／`CopyEntitiesFrom`（`:3785`）；Bevy、flecs 沒有 | 分歧——**使用者已定**帶過去、做成機制（`carry-over-mechanism`） |
| （不放） | 走路與加速都寫同一個元件、沒寫先後：照登記順序跑、`TieBreaks` 列出這一對 | DI-KS 5 | 6 拿掉歧義報錯 | — | **延到第 4 批**的段（D-E2 與 `Shared` 拿掉之後）；之前由第 1 批既有的單元測試守（主會話照 L17 判：不把新的情境建在已決定拿掉的 `Shared` 上） |

不放進情境、照舊是單元測試的第 1 批機制：綁執行緒的 Resource 斷言、記號守衛、K8 這類實作面的檢查。照 `guards-after-mechanism-stable`：第一段通過之後，第 1 批表中 1～5 的機制就算「涵蓋它的 scenario 段已通過」。

### 8.3 之後各批往上加的接口

- 每段一個 `TEST_CASE("scenario segment N: …")`，共用 `WalkerGame` 的登記、存檔與讀檔函式；前面的段在每一批照跑。
- **第 3 批（時間 Module＋事件）**：`Walk` 改成由時間 Module 驅動的固定步；檢查第 k 個 tick 的位置；加分改成 `Message`（`ScoreGained`）；時間的 Resource 存進存檔、讀檔放回（讀檔宣告 5.5）；第 2 批為了 D-E12 加的 domain 宣告刪掉。
- **第 4 批（權威＋回滾）**：回滾到某個 tick 重跑，結果和第一次相同；加上「兩個寫者、照登記順序、`TieBreaks`」那一步。
- **第 5 條（生成世界、多個 World）**：`PlacePlayer` 與 `Load` 從 runner 與 `beforeSwap` 換成生成世界；多個 World 與交換點各加一段。
- 後面的批若要改前面的段的某一步，照 8.4 分類；是已核准的契約變了，就照 §14.4 寫明那一步的判定怎麼變、為什麼。

### 8.4 失敗時的分類（`v8-arch-gyo2-scenario-failure-by-contract`）

使用者（經主會話轉交）：「底層機制不會因為應用變就會變。要不然到頭來都是一通改。例如：不會用Unity和Unity不行是2個命題，如果混在一起，就完全沒有標準了。」scenario 失敗時，報告先標出是哪一種，並引用契約的條文（DI-KS／O-#／說明頁的哪一條）：

1. **不會用**：機制照契約做事，錯在情境的寫法（例：讀檔 System 用 `Commands` 放回玩家，照契約到同步點才生效，`Walk` 先跑了）→ 改情境，機制不動。
2. **機制不行**：契約承諾了、實際沒做到（例：契約說 `ExclusiveWorld` 當場生成的實體後面馬上看得到，實際看不到）→ 照 AGENTS §14.3 先固定成失敗的測試（Red）再修機制，契約不變。
3. **契約沒承諾過**：情境要的東西契約從沒說過（例：要一般 System 也能當場生成）→ 不算前兩種，是新需求；停下報告，照 Feature → Contract → Architecture 交使用者，**不在本批改**。

**對照開源引擎**（`v8-arch-gyo2-scenario-cross-engine-check`）：使用者：「情境測試，你也可以把同樣的情境放在哪些開源Engine上看。如果所有人都不支持，那麼這個情境要不然就是牛角尖，要不然就是偽命題。」「如果有支持和不支持的，就照L17的判法，讓人類來決定（分歧）」「全部的那種就不用我來定了…大概率要看場景定得對不對。」——之後每一段加步驟時照 8.2 的三種分類；無人支持的由寫那一段的卡片重看情境、在步驟表寫明「改成什麼／拿掉、理由」（例外：那一步直接來自使用者的決定或點名的 Feature，改它就是推翻使用者的決定，仍交使用者）。

---

## 9. 突變的處置

依 D184：每批只跑相關的突變（新的、`find` 在改動範圍的、殺它的測試被改的），PR 線合併前背景完整跑一次。實作開始時用「每個突變的 `find` 定位到行號、和 3.11 的範圍相交」的做法重跑一次，不只靠關鍵字（檢查 M10）。

| 處置 | 突變 | 理由 |
|---|---|---|
| 退役（性質拿掉） | `ns-startup-nestable`、`ns-startup-running-allows-nested`、`ns-st-clock-only`、`ns-st-tick-first-read`、`ns-st-tick-first-error`、`ns-st-tick-stamp`、`ns-st-running-not-cleared`、`ns-st-no-retake`、`ns-two-startups-allowed`、`ks-s2-re7-startup-schedule`、`ks-s2-re7-startup-mount`、`rs-j15-startup-counted`、`ks-s7-startup-change-same-run`、`ns-st-settings-after-startup`、`ks-s2-finish-not-first`、`hf-startup-skipped`（`HookFlushTests` 改寫後若 `find` 仍在，改寫而不退役） | 第 7.1 節組 A、B、C、E、F，第 7.2 節組 G |
| 搬到編排層、改寫 `find`／`expect` | `ns-st-not-auto`、`ns-st-not-marked-auto`、`ns-st-not-marked-explicit`、`ns-st-explicit-again`、`ns-st-after`、`ns-st-error-ignored`、`ns-st-error-not-reported`（→ O-1）；`rs-j19-duplicate-built`、`rs-j19-requires-unchecked`、`rs-j24-re6-removed`、`ks-s2-plugin-finish-again`（→ O-2、O-E1、O-E3、O-E4）；`ks-s3-version-before-finish`（→「`Finish` 讀到版本 1」）；`rs-fix-b4-finish-before-resolve`（殺它的 `CoverageTests` :130 搬到編排層） | 第 7.1 節組 A、B |
| 改寫（`find` 碰到改動，性質留在 Kernel） | `rs-j18-plugins-reversed`（id 照 `Register` 的順序）、`rs-j19-type-twice-allowed`、`rs-j19-unregistered-silent`、`ks-diks15-code-without-text`（拿掉的 `DE21` 換成留下的碼）、`rs-sym-inline-g`、`rs-sym-anonymous`（錨點 `Plugin::~Plugin() = default;` 換掉）、`ks-s7-first-run-only`、`ks-s7-first-run-only-limit`（`find` 含 `if (record->options.startup) {`）、`ks-s11-export-runner-back`（期待的匯出行跟著變）、`rs-j24-*` 裡 `find` 在 `Registry::Finish` 附近的 | `find` 的那一行被改或刪 |
| 新增（只給行為的測試） | 編排層、讀檔案例與 scenario 第一段的每個新案至少一個專屬突變；代表性的：`Configure` 在 `Build` 之後、`Finish` 在 `BuildPlans` 之前、`Check` 夾在 `Finish` 之間、Kernel 失敗時照樣 `Finish`、出錯的單位照樣 build、O-E1～O-E7 各拿掉一個、依賴不看順序、不進 World 的依賴也要求在清單、空的 World 不 `Register`、遊戲初始化每幀都跑、遊戲初始化跑在最後一個 World、`InsertBefore` 插成 after、`RunFrame`／`RunSchedule` 不先跑遊戲初始化、遊戲初始化 Err 時照跑每幀、中斷的 World 照樣呼叫 Kernel、準備在第一次 Run 之後才呼叫、準備的順序反過來、`beforeSwap` 在換上之後才跑、`extra` 失敗時換上新的、替換時不銷毀舊的、替換時沿用舊的 Registry（插入的 System 留著）、替換時重跑遊戲初始化、拒絕時不記警告、拒絕時記兩次、插入的 `build` 丟例外時不丟暫存副本、插入後重跑 `Finish`、預設不帶 Module、`DISABLE_MODULES` 無效、Plugin 預設帶、登記檔驗證的每一條拿掉一個 | 第 7.3、8 節 |
| **不新增** | 守 7.4 那些腳本守衛的突變 | 延後（第 7.4 節） |

每個退役或搬走的突變在清單裡寫理由；改寫與新增的突變的預期失敗寫成該突變專屬的訊息或案例名（§14.2）。

---

## 10. Design Invariants 的變更（逐條要你核准；最終文字在第二段核准）

### 10.1 Kernel（`target/kernel.zh-Hant.md`）

| DI／RC | 處置 | 改寫後的要旨 |
|---|---|---|
| DI-KS 3 | 刪「（或第一次的 `Finish`）」 | Plan 只在 `BuildPlans` 成功時建 |
| DI-KS 6 | 刪「非 Startup」「只掛在 Startup 的 System 不算寫者…（DI-KS 22）」 | 寫者以 System 計、整個 World 的所有 schedule；整條隨第 4 條拿掉 |
| DI-KS 15 | 過渡表刪第一列；正文的「不認得開局」照舊、加「不認得登記單位與 runner」；第 (7) 項照統合修訂 38 補「在 World 是 Ready 時設定配發器、含範圍的下界」（O-7 的 Kernel 那一半靠它；讀檔宣告 5.1 請主會話改的字面，本批一起同步；檢查 S15） | 守衛「DI-KS 15: the StartupCode values …」的清單同步 |
| DI-KS 16～22 的前言 | 「以下八條」「J-58 Startup」改成七條、拿掉開局 | — |
| DI-KS 22 | **拿掉**，保留編號：「移到編排層的 O-1（`target/v2/orchestration.zh-Hant.md`）」 | — |
| DI-KS 24 | 刪「開局在同一次 `Run` 裡先跑時不再讀」與守衛「開局改的值下一次 Run 生效」 | — |
| DI-KS 25 | 刪「`Plugin::Finish` 裡 0」「`Plugin::Finish` 讀的就是正在建的表」 | 「建好前 0、第一次成功 1」 |
| DI-KS 26 | 刪「schedule（開局的除外）」「開局的 schedule 與它上面的掛載」；R-E7 的清單同步 | — |
| RC 7「Host 又長回來」 | 移到編排層的 RC（統合 9.1：「保留原條件、改寫主詞」） | 見 10.2 RC-O1 |
| RC 13 | 「plugin」改「登記單位」 | — |

`docs/architecture/target/README.md` 的目標 Design Invariants 清單：DI-KS 22 那一列標拿掉、加編排層新頁一列。

### 10.2 編排層（新頁 `target/v2/orchestration.zh-Hant.md`，見出し `Design Invariants`）

「守衛」欄：行為的測試照常；腳本守衛延後的寫「延後（7.4）；這段期間靠文件與審查」。

- **O-1 遊戲初始化**：一個 `Product` 一份清單，行程一次、在所有 World 的第一幀與任何 `RunSchedule` 之前跑完、跑在第一個 World；明確呼叫只准一次；清單的順序照插入（插入當下解析錨點，錨點不在是啟動錯誤）。守衛：O-1 的行為案例；scenario S1-2。
- **O-2 登記單位與生命週期**：產品層一次 `Configure`（產品的最後）；每個 World：照順序 `Build`（Module 先、產品的最後）→ Kernel `BuildPlans` → 照順序 `Finish` → 照順序 `Check`；Kernel 有啟動錯誤的 World 不跑 `Finish`、`Check` 與清單檢查；出錯的單位不用、其餘照做、一起收齊；有任何錯誤就不建任何 World；動態插入只走 Kernel 的兩步，不重跑 `Finish`／`Check`、不通知——它們屬於登記單位自己的生命週期，System 不能重置它（`dynamic-insert-no-lifecycle-rerun`）；替換 World 時對那個 World 重走 `Build`～`Check`，登記單位以 World 名為鍵的資料在 `Build` 時重設；選項包只轉交。守衛：O-2 各案。
- **O-3 runner**：一個 `Product` 至多一個，在 `Configure` 裝；沒有時只跑一次；runner 只呼叫 `Product` 的執行入口，不排 System 的先後。守衛：O-3 的行為案例；「不讀 Plan 的步驟」的掃描延後（7.4），這段期間靠審查。
- **O-4 職責**：**編排層的職責是組裝與流程**（L17）。職責清單：登記單位與生命週期（含產品自己的登記）；依賴的讀取與檢查；`Product`（World 群、runner、World 的準備與替換）；遊戲初始化與每幀的清單；生成世界的流程（第 5 條）；交換點、extract 的時機、執行緒的安排（第 5 條）；啟動錯誤的彙整與編排層自己的警告。不帶任何第三方能力；不認得產品、策略的語意、時鐘、回滾。守衛：依賴只有 Kernel 與 Base（登記檔）；include 掃描與 `naming_guard` 延後（7.4），這段期間靠審查。（「World 的準備與替換」寫在「`Product`」那一項之內，是統合 O-4「`Product`（World 群與 runner）」的細分——**這個歸法請你一起確認**，第 14 節。）
- **O-5 啟動錯誤一次收齊**：Kernel 的 `StartupReport`、編排層的 `OrchestrationCode`、各登記單位的錯誤收成一份（準備的錯誤另一份）；Kernel 的 `StartupCode` 不擴充。守衛：O-5 各案。
- **O-6 依賴只在登記檔**：Module／Plugin 之間的依賴只寫在兩份 CSV；`engine/` 的子目錄、引擎內 link 與產品的登記單位表由它產生；編排層建立 `Product` 時檢查；Module 預設全帶、產品可以關，Plugin 由產品列出。守衛：登記檔的 `cmake -P` 行為案例；依賴各案；CMakeLists 的掃描延後（7.4）。
- **O-7 準備時機**：每個 World 建好之後（啟動與每次替換）、第一次 Run 之前，照「帶過去 → 登記單位的 → 描述的 → 替換帶的」順序呼叫準備；準備放預設值時不蓋掉已經在的值；失敗時這個 World 不用。守衛：O-8 各案；scenario S1-6。
- **O-8 替換**：替換照產品描述重新登記（新的 Registry；之前動態插入的全部重置；`Configure` 與遊戲初始化不重跑；`restore-reruns-registration`）、先後由作者選（預設先建新的：任何一步失敗就保留舊的；先丟舊的：失敗時位置是空的、`RunFrame` 回 `WorldMissing`、可以再填；`replace-order-author-choice`）、`beforeSwap` 在換上之前；遊戲初始化不重跑；只在兩次 Run 之間；中斷的 World 可以被換掉。守衛：O-9 各案、讀檔案例、scenario S1-6～S1-8。
- **O-10 帶過去**：任何 Resource 型別都能被標記帶過去；編排層照標記清單泛用地搬（型別身分＋型別化的搬移函式），不認得特定型別、不看值的內容；在所有準備之前；失敗移回（`DropOldFirst` 留在暫存處）；值裡不能存指向舊 World 的參照，是作者的責任。守衛：判定 2 案例 6、scenario S1-10、O-9 的帶過去各案；「值裡沒有舊 World 的參照」不立守衛，靠文件與審查（`guards-after-mechanism-stable`）。
- **O-9 動態插入的警告**：Kernel 拒絕或建 Plan 失敗時，編排層經 `Engine::Base::Log` 記**一次** `Warning`、把錯誤回給呼叫的人、World 照跑。守衛：O-10 各案。

**Replacement Criteria**（L17 的形式）：
- RC-O1（原 Kernel RC 7）：第一個真實能力接進機制時仍要改編排層的迴圈或 Kernel 本體；或編排層以外的協調邏輯在兩個以上產品中重複。
- RC-O2：每加一種產品或 World 的組法都要改編排層的程式。
- RC-O3：runner 需要排 System 的先後才做得出某個 Feature。
- RC-O4：編排層開始 include 某個第三方或某個 Module 的內部。
- RC-O5：編排層的職責清單（O-4）要多加一項，或判不出某樣東西屬於哪一層——不數次數，出現就停下來問。
- RC-O6：登記檔的欄位不夠用，需要第二份描述同一件事的檔，或 CMake 與編排層讀出的依賴不一致；或「只用函式庫、不要登記單位」的 World 被 O-E3 擋到（3.2 的已知限界）。
- RC-O7：替換 World 的某個用途需要「不重建 World 就換掉內容」（讀檔宣告 RC 第 1 條的同一件事）。
- RC-O9：讀檔（替換 World）重跑登記的時間成為讀取時間的主要部分，或第 5 條的 extract 為了它要改 Kernel——停下來比較做法（`restore-reruns-registration` 的代價）。
- RC-O8：scenario 的某一段為了通過，必須讓編排層知道某個 Module 或產品的語意（8.4 的第三種一直出現在編排層）。

---

## 11. 判定（先宣告，結果出來之後不放寬）

這不是計測的批（沒有閾值、試行次數）；判定都是一次決定的。分歧的步驟照使用者的決定寫（`scenario-cross-engine-check`）：第 14 節第 1～4 件都已定（帶過去、不重跑生命週期、重跑登記、替換的先後作者選），下面的判定照它們寫。

### 判定 1：通過與一次性的掃描

| # | 判定 | 怎麼量 |
|---|---|---|
| G-1 | 本機 `ctest --preset test -L "cpu|shader"` 全過（macOS，Debug）；新的 `orchestration.*`、`scenario.*`、`cmake.unit_registry` 在內 | ctest 的輸出 |
| G-2 | 第 7 節的每一案照表處置；新案照 7.3 的 Red 做法，並各有專屬突變 | 提交順序與輸出；Red 的輸出存證據資料夾 |
| G-3 | **一次性的驗收掃描**（不加進測試，第 7.4 節）：`git grep -n -P`，終了碼區分 0 命中與失敗，每個式子先在 0f1f2d21 用已知例命中一次（§14.2）。`engine/kernel` 對 `\bclass\s+Plugin\b`、`\bFinishContext\b`、`\bInstallRunner\b`、`\bRequires\s*\(`、`std::vector<Plugin\s*\*>`、`Registry::Finish\b`、`\.startup\b`、`\bstartup(_|Ran_|Running_|Schedule)\b`、`\bStartupScope\b`、`\bRunStartup\b`、`\bStartupNotNestable\b`、`\bStartupRunning\b`、`\bDE21\b`、`\bRE[136]\b`、`"H-1[23]"`、`\bPluginRecord\b`、`\bCheckPluginList\b`、`\bCheckRunners\b`、`\brunnerOwners\b`：0 命中。引擎各 CMakeLists 的 `target_link_libraries(` 引數裡的引擎內別名：0 命中（不看 `ALIAS` 行與註解；在 0f1f2d21 命中 link 的那幾行）；configure 之後每個引擎 target 的 `LINK_LIBRARIES`／`INTERFACE_LINK_LIBRARIES` 和改之前逐一相同 | 掃描腳本（Python 或 `git grep`，不丟 stderr）；比對結果存證據資料夾 |
| G-4 | 突變：第 9 節的退役以外的全部照 D184 跑，每一個都在預期的失敗下 killed | `tests/common/mutation/run_mutations.py` 的輸出 |
| G-5 | 守衛腳本的 Python 測試全過 | unittest 的輸出 |
| G-6 | CI 的 L1 三平台通過（PR 上；Windows、Linux 第一次跑編排層與登記檔的 CMake） | GitHub Actions |
| G-7 | 第 7.1 節有緩的面的五組與第 7.2 節的 DI 守衛各列，由檢證者用實際的測試與程式確認 1～5 的記述（§14.4） | 檢證報告 |
| G-8 | 編排層的行為案例、讀檔案例與 scenario 第一段（受け入れ判定）與它們的突變清單，由和作者不同的模型做一次敵對式檢查（§14.2）；報告寫檢查者的模型、找到的處數、加進去之後的結果（killed／總數） | 檢查報告 |

### 判定 2：讀檔的案例（讀檔宣告修訂 10、11 要求；`tests/common/orchestration/RestoreFlowTests.cpp`）

合成的「單機遊戲」（產品自己的回呼）：登記 `Player`、`Monster`、`ChaseTarget`、`Gold`（Resource）、範圍型配發器、`Load`（`ExclusiveWorld` 的 System；`Domain::Frame`、不當 clock）與每幀的 `Play`；存檔是測試裡的結構（玩家 12、怪物 57、58，57 的 `ChaseTarget` 是 12，金幣 120，下一號 900）。

1. **啟動時讀檔**：描述的準備把配發器範圍訂成 `[900, 上限]`、放暫存的存檔資料與金幣的預設值 0；`RunSchedule("Main","Load")`（遊戲初始化先跑；`SpawnWithId` 帶完整 bundle 放回、金幣改成 120），再 `RunFrame`（`Play` 生成一個選單實體）。之後：編號 12、57、58 在、值與存檔相同、`ChaseTarget` 仍是 12、選單實體的編號 ≥ 900、金幣 120、沒有被丟的生成。
2. **沒有移下界就整批被丟**（對照組）：同上但範圍 `[1, 上限]`：每一筆以 `IdInAllotterRange` 被丟並記紀錄——證明是準備時機讓案例 1 成立。
3. **遊戲中讀檔**：先跑三幀（生成實體、金幣改成 50、製造一筆被丟的生成），再動態插入一個 System；`ReplaceWorld("Main", {extra: 讀存檔; beforeSwap: Load})`。之後：只有讀檔放回的與之後生成的實體；金幣 120；動態插入的 System 不在；被丟的紀錄從空的開始；遊戲初始化的計數仍是 1；下一幀 `Play` 跑在新 World。
4. **讀檔失敗**：預設 `BuildNewFirst`——(a) `extra` 回 Err（存檔的版本不支援，碼 `"save-version-unsupported"`；回傳的錯誤帶著這個碼）；(b) `Load` 回 Err（存檔的實體部分讀不了）：兩者都回對的碼、警告一次；舊 World 的實體與金幣 50 照舊、再跑一幀沒有錯誤。`DropOldFirst`——(c) `extra` 回 Err（版本不支援）：回對的碼、警告一次；位置是空的，`RunFrame` 回 `WorldMissing`；(d) 接著用一份可以讀的存檔 `ReplaceWorld`：成功、之後和案例 3 的結果相同。
5. **讀兩次相同**：同一份存檔換兩次 World：兩次的 `Summary()` 相同。
6. **遊戲初始化放進第一個 World 的東西**（3.12）：遊戲初始化放 `KeyBindings`（宣告帶過去）與 `Score`（不帶），第二幀把 `KeyBindings` 改掉；`ReplaceWorld` 之後 `KeyBindings` 是改過的值、`Score` 回到準備的預設值、帶過去在所有準備之前；`extra` 失敗之後舊 World 的 `KeyBindings` 還在；兩種不同的型別（一個是登記單位標記的、一個是產品標記的）都帶過去；舊 World 裡沒有那個值：不帶、新 World 是準備給的；新 World 沒有登記那個型別：`ReplaceCarryOverFailed`、值移回舊的；`DropOldFirst` 失敗後再替換：暫存處的值照樣帶進去。

案例 5 守的是「新 World」這份契約的後果，靠 World 的建構本身，沒有對應的突變（寫明，不算進檢出力）。突變：準備移到第一次 Run 之後（案例 1）、準備不呼叫（案例 1、2）、替換時保留舊實體（案例 3）、沿用舊的被丟紀錄（案例 3）、重跑遊戲初始化（案例 3）、失敗時換上新的（案例 4a、4b）、`beforeSwap` 在換上之後（案例 4b）、`DropOldFirst` 仍先建新的（解構先後）、`DropOldFirst` 失敗時留下半成的新 World（案例 4c 的 `WorldMissing`）、空的位置不能再填（案例 4d）、帶過去漏掉、帶過去在 `extra` 之後、失敗時不移回（案例 6）。

### 判定 3：scenario test 第一段（`tests/common/scenario/ScenarioTests.cpp`）

第 8.2 節 S1-1～S1-10 的每一步的檢查都成立（含 S1-10）。失敗時照 8.4 分類報告。突變：至少各一個——遊戲初始化每幀都跑（S1-2、S1-3 的分數）、`Commands` 的 Resource 在同步點前就看得到或看不到（S1-3）、插入的 System 不接上（S1-4 的位置）、替換時沿用舊 Registry（S1-8 的位置與登記表）、配發器下界不移（S1-7 的 id 被丟）、當場生成的實體同一個 System 看不到（S1-7）、`AllottedIdAlive` 中斷 World（S1-7b）、替換時重跑遊戲初始化（S1-8 的計數）、換上後不能再插入（S1-9）、`KeyBindings` 沒帶過去或被準備蓋掉（S1-10）。

---

## 12. 誰做、用什麼、怎麼分段

**上段**（navigator：新 Module 的公開介面、Kernel 公開介面的減少、Design Invariants、建置圖的構造）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 事前宣告的作者 | Opus／high（本卡） | — |
| 事前宣告的敵對式檢查 | Fable／high | 和作者換模型（§4）；修訂 0 做過一次 |
| 實作者（四段） | Opus／high | 統合第 8 節本批那一列 |
| 審查者 | Fable／high | 統合第 8 節；和實作者換模型 |
| §14.4 的檢證者（G-7） | Fable／high | **要你同意**（統合第 8 節本批那一列只寫實作者與審查者；navigator 的預設是跟實作者，用 Opus／high）。理由：檢證要和實作者換模型才看得出盲點 |
| 受け入れ判定的突變清單敵對式檢查（G-8） | Fable／high | §14.2 要求換模型、不用小模型；同上，**要你同意** |
| `ReplaceWorld`（兩個 World 的生命期、失敗時的收拾）與 `Product::Build` 錯誤路徑的局部逐行複核 | Fable／high（審查者一併做） | 檢查 N4 的建議；第 1 批用 xhigh 的是時序與 stamp，這裡是物件的生命期，high 就夠 |

**分段**（先後卡片照 L17 判：新家先建、再拿掉舊的，測試才有地方搬；登記檔在後，因為編排層的依賴檢查只吃 `RegisteredUnits` 這份資料；scenario 最後，因為它要經過全部）。每段結束時 ctest 通過、提交、回報；每段結束時做那一段新行為案例的 G-8：
- **段 1 編排層本體**（O1～O4、O6～O10）：新 Module、登記單位型別、`Product`、彙整、清單、runner、準備、替換、動態插入；行為案例與讀檔案例。8～10 h。
- **段 2 Kernel 減法**（O11、O12）：拿掉開局與登記單位、測試輔助改兩步、J-58 與 J-19 的案搬完、指紋與匯出升版、DI 文字。4～6 h。
- **段 3 登記檔**（O5）：兩份 CSV、CMake 讀取與驗證、`engine/` 的子目錄與各 Module 的 link、`gyo_product_units` 與它的測試 target、`cmake -P` 測試、G-3 的 link 比對。3～5 h。
- **段 4 scenario 第一段**（O13）：`WalkerGame`、S1-1～S1-9、突變、別家對照的證據。2～3 h。
- 文件（說明頁、`orchestration.zh-Hant.md`、dev_log）與突變的完整跑：1～2 h。

合計 **18～26 h**（統合第 8 節估 10～14 h；多出的是登記檔的 CMake 與產生、準備與替換、讀檔案例與 scenario）。超過 1.5 倍（39 h）或單一問題累計超過 2 h 就停下回報。

**兩段核准**（照第 1 批）：這一次請你核准的是宣告（方向、設計、要動哪些判定、第 14 節）。實作完、合併前，帶 G-7 的檢證報告、G-8 的檢查與突變結果，再請你核准第 7.1 節各組、第 7.2 節的 DI 守衛各列與第 10 節的最終文字。第二段被你退回的組，合併前把那一組的改動撤回。

---

## 13. 範圍的限界

- 宣告寫在讀程式之後、實作之前；時間是估的，沒有量測。
- 第 7 節的案例清單由本卡以案身掃描與讀測試寫成，沒有像第 1 批那樣由偵察者逐案讀全部 15 個測試檔；實作開始時由實作者照組再核一次。
- 別家的系譜：Bevy（6a911653f）、Unity Entities（1.5.0）、flecs 讀本地原始碼的指定行；O3DE、Unreal 的清單機制只照文件與統合的證據，**沒有讀原始碼**。
- 空窗：(1) System 端的請求通道（結束、替換 World、從指令插入）在每幀迴圈的 runner 那一批之前沒有，作者用自己的 Resource 與自己的 runner 接；(2) 遊戲初始化的 System 在第 4 條之前照樣算寫者（以前開局有豁免）；(3) 遊戲初始化的 schedule 在第 3 條之前要掛一個 domain；(4) 生成世界之前，讀檔的 schedule 由 `RunSchedule` 或 `beforeSwap` 去跑；(5) 7.4 的腳本守衛在第 3 批開頭之前沒有，編排層的 O-3、O-4、O-6 靠文件與審查。
- 替換時兩個 World 同時存在（預設 `BuildNewFirst`）：獨占的外部資源（視窗、音訊裝置、固定埠、檔案鎖）若由登記單位的準備取得，新 World 的準備會在舊的還握著時跑；本批沒有這類 Module，碰到時那個 Module 的宣告要處理，或作者選 `DropOldFirst`。綁執行緒的 Resource 帶過去時在呼叫的執行緒上綁定，和舊的同一條，沒有跨執行緒的情形；第 5 條有多執行緒之後要重看。
- 產生的 `<target>_units.cpp` 在本批只有測試 target 用；第一個真實產品接上時，`gyo_product_units` 和產品登記檔、打包的互動（AGENTS §7 的刪除檢查）要在那一批核一次。
- 替換 World 重跑登記的時間沒有量（本批不碰執行效率）；第一個有讀取時間預算的產品出現時量（RC-O9）。
- scenario 第一段的「放玩家」與「讀檔」由 runner 式的呼叫與 `beforeSwap` 代替生成世界；第 5 條換掉時，那兩步的判定照 8.3 改寫。

---

## 14. 要你定的

### 第 1 件：World 被換掉時，遊戲初始化放進去的東西怎麼處理——**已定：帶過去，做成機制**（使用者 2026-11-18，`v8-arch-gyo2-carry-over-mechanism`）

使用者在 side chat 定（經主會話轉交）：問「但是帶過去，Engine就要理解這個值，是這個意思嗎？」（side chat 答不用：只知道型別的身分與作者的標記、整個值照搬，作者負責值裡不存指向舊 World 的參照）之後：「帶過去，但是是要建立機制，而不是單純的寫死一個搬運Object」。比過的選項：帶過去（本卡建議）←選／長留的放在 World 外、每個新 World 由準備放進去／第一個 World 不換。理由：使用者沒有另外說；本卡的理由（使用者看過）：選項選單改過的值在 World 裡，只有帶過去不用作者另外同步。本宣告已照它改（3.12 新節、3.8、3.9、3.5 的碼、7.3 O-9、8.2 S1-10、10.2 O-7／O-10、判定 2 案例 6、判定 3）。

### 第 2 件：動態插入要不要重跑 `Finish` 與 `Check`——**已定：不重跑、不通知**（使用者 2026-11-18，`v8-arch-gyo2-dynamic-insert-no-lifecycle-rerun`）

使用者在 side chat 定（經主會話轉交），原話、比過的選項與理由收在 3.10。這個決定改寫 HANDOFF `v8-arch-gyo2-dynamic-insert-per-world` 與統合 O-2 草案的「執行中的插入（指令等）照同一條路再走一次」那半句。本宣告已照它改（3.10、10.2 O-2、第 6 節差分第 1 項）。

### 第 3 件：換 World 時，登記沿用還是重跑——**已定：重跑**（使用者 2026-11-18，`v8-arch-gyo2-restore-reruns-registration`）

使用者在 side chat 定（經主會話轉交）：讀檔只有一種意思，回到某一刻的狀態；換 World 時照產品描述重新登記，所有動態插入的 System（含指令加的）一律重置；分不清插入的來源（指令或遊戲本身）就不冒險，作者要保留就自己存成資料、讀檔後再插。比過的選項：沿用同一份 Registry（本卡修訂 1 的建議）／重跑（flecs、Unity）←選／依來源分（因分不清來源而不採）。本宣告已照它改（3.9、3.10、7.3 O-9、8.2 S1-8～S1-9、10.2 O-2／O-8、判定 2 案例 3）。後果與留給第 5 條的 extract：3.9「重新登記」、2.2。

### 第 4 件：替換的先後——**已定：做成機制、作者選，預設先建新的**（使用者 2026-11-18，`v8-arch-gyo2-replace-order-author-choice`）

使用者在 side chat 定（經主會話轉交）：「建立機制，讓作者自己決定，預設先建新的。」點名的 Feature 是 BigMap（舊架構 master 的計畫，`v8-arch-big-map-density`、`v8-arch-big-map-sim-model`）：換到另一張大圖時兩張同時在記憶體撐不住。比過的選項：只做先建新的（本卡修訂 1 的建議）／只做先丟舊的（讀檔宣告的字面）／作者選、預設先建新的 ←選。壞檔：使用者認為系統層級無解，產品用版本管理、太老的檔不支援（原話在 3.9）。本宣告已照它改（3.9、3.4「中斷的 World」、3.5 的碼、5.2 的 BigMap、2.2、7.3 O-9、8.2 S1-6、10.2 O-8、判定 2 案例 4、第 13 節）。

### 第 5 件：檢證者與 G-8 檢查者用 Fable／high（同意與否）

第 12 節：統合第 8 節本批那一列只寫實作者與審查者；§14.4 的檢證者與 G-8 的敵對式檢查者，本宣告提議用 Fable／high（和實作者 Opus 換模型）。不同意的話照 navigator 的預設用 Opus／high（和作者同模型，報告寫明）。

### 一起確認（不是新的題目）

- 第 10.2 節 O-4 的歸法：「World 的準備與替換」（含帶過去，3.12）、「編排層自己的警告」寫在 O-4 原有的項目之內，不算職責清單多一項。你若認為是多一項，照 L17 那就是第 1 件的一部分。

**卡片照 L17 自己判、沒有交你的**（列出來讓你看得到，有意見請更正）：名字（`GYO::Engine::Orchestration`、`Module`／`Plugin`、`Product`、`Prepare`、`ReplaceRequest`、O-E1～O-E7）照命名規範；登記檔用 CSV、放 `engine/config/`、加 `target` 欄；`engine/` 的子目錄與各 Module 的 link 改由登記檔產生（AGENTS §9 一處）；登記單位沒有 `Name()`；Plugin 預設不帶；產品自己的遊戲程式用描述的回呼登記（不立第三種登記單位）；每個 World 一份 Registry；`Configure`／`Finish`／`Check` 分開；錯誤收齊照 Kernel 現在的做法；清單插入的語意照 Bevy、同名兩次不擋；警告用 `Engine::Base::Log`；System 端的請求通道留到每幀迴圈的 runner 那一批；延後的守衛排在第 3 批開頭；scenario 的小遊戲怎麼設計（不用 `Shared`）；分段的先後。

---

## 15. 修訂 0 檢查的處理（`declaration-r0-adversarial-check.md`）與修訂 1 收的轉交

| 發現 | 處理 |
|---|---|
| M1 沒接 scenario 與守衛延後的決定 | 根據改引 f65f1e41；新增第 8 節 scenario 第一段、第 7.4 節延後的守衛；第 7 節開頭兩類；G-3 改成一次性 |
| M2 產品自己的遊戲程式沒有登記的路 | 3.3：描述裡的回呼（`configure`、`build`），不立第三種登記單位（三家都讓遊戲程式直接登記）；判定 2、scenario 改用它 |
| M3 D-E12 的後果漏列 | 3.6「第 3 條之前要有 domain」；3.11 補 D-E12 的開局那一支；組 A 的緊的面；組 G 的改法照實寫；判定 2 與 scenario 的 schedule 掛 Frame |
| M4 G-3 的 CMake 式子、`gyo_expect_links` | 登記檔加 `target` 欄、link 別名字面；`gyo_expect_links` 五處不改、編排層那一處延後；G-3 改成只看 `target_link_libraries(` 的引數，加 configure 後的 link 比對 |
| M5 組 J 擴 Kernel 的守衛腳本 | 拿掉；`naming_guard` 那一項進 7.4 |
| M6 錯誤收齊前後不一致 | 3.4 第 1 步照 Kernel 現在的做法（出錯的不用、其餘照做）；O-E7 另一份；Kernel 失敗時清單檢查也不跑 |
| M7 讀檔失敗只涵蓋準備 | `ReplaceRequest::beforeSwap`：換上之前在新 World 上跑讀檔；判定 2 案例 4 分 (a)(b)；先後交第 14 節第 4 件 |
| M8 帶過去的三個缺口 | 帶過去排在所有準備之前、用移動、失敗移回；外部資源寫進第 13 節與第 14 節第 4 件的代價；案例 6 加「失敗之後舊的還在」 |
| M9 §14.4 漏列、組 E 歸類 | 補 `RegistrationRoundTests` :735、`HookFlushTests` :754、`SchedulerTests` :694 與匯出格式 3→4；組 E、F 搬到 7.1；盤點改掃案身 |
| M10 漏 6 個突變 | 第 9 節補上；實作開始時用 `find` 定位重跑 |
| S1 中斷的 World | 3.4「中斷的 World」；`WorldInterrupted`；可以被換掉 |
| S2 沒有登記單位的 World 斷言 | 每個 World 一定經過產品的 `Register`（3.3）；7.3 加案 |
| S3 判定的空白 | 7.3 O-9 改成先插入再換、比 `Types()` 的物件與插入的 System、舊 World 的解構計數；選項包、`WorldCheck` 的宣告、替換後的處理器、替換後仍被拒各加案 |
| S4 例外 | 3.4「例外」；插入的 `build` 丟例外先 `DiscardRegistrations`；替換時照 3.9 第 5 步；7.3 加兩案 |
| S5 警告另立處理器 | 改用 `Engine::Base::Log(LogLevel::Warning, "orchestration", …)` |
| S6 先建新的是分歧 | 第 14 節第 4 件；第 6 節差分第 6 項 |
| S7 第 1 件的選項不公平 | 第 14 節第 1 件重寫：別家的分布照實寫、第二個選項寫出合理的分工、第三個選項更正、建議的代價補齊 |
| S8 第 2 件少代價與中間的路 | 第 14 節第 2 件補「衍生資料過期」「只通知」「能否決」與 HANDOFF 的出處 |
| S9 `Name()` | 拿掉 `Name()`，名字由產生的表給；O-E2 縮成手寫的表才會出現；`engine/` 的子目錄也由登記檔產生 |
| S10 runner 直接 `Run` | `Product::RunSchedule` 先確保遊戲初始化；判定 2 案例 1 改用它；說明頁寫明 |
| S11 判定 2 在本批被 D-E2、D-E12 擋 | `Load` 用 `ExclusiveWorld`（不算寫者）；schedule 掛 Frame；不用 `Shared` |
| S12 §4 的三樣不齊 | 3.3、3.7、3.8 補「退掉的」；`Name()` 與 K0 形狀的「舊形狀」一問寫出 |
| S13 World 的身分 | 三個 context 交出 World 名；7.3 加案 |
| S14 清單插入的語意、`World()` 參照 | 3.6 照 Bevy 定；同名兩次不擋；`World()` 參照的期限寫明 |
| S15 第 9 節與差分的漏 | 10.1 加 DI-KS 3、DI-KS 16～22 前言、DI-KS 15 第 (7) 項、`target/README`；差分第 7 項 |
| S16 關掉被依賴的 Module 在 configure 擋 | 3.2 第 3 點 |
| N1 G-3 的漏 | 式子補上 |
| N2 3.11 漏列的位置 | 補上 |
| N3 Unity 的說法 | 3.8、3.4 更正 |
| N4 模型 | 檢證者與 G-8 檢查者改成「要你同意」（第 14 節第 5 件）；局部逐行複核由審查者做 |
| N5 零碎 | G-4 改寫；執行時的錯誤另一組碼；`PrepareFailed`；`OwnerTaken`；動態登記的 schedule 不進每幀清單；`Product` 的解構在同一條執行緒 |
| N6 第 4 節的漏 | 補 README 索引、會過時的舊文件、`tests/common/Requirements.cmake` |
| N7 `depends` 兩用 | 3.2 的已知限界、RC-O6 |
| N8 extract 的位置 | 3.4 第 3 步、2.2 |

**修訂 1 收的轉交**（主會話，2026-11-18）：`v8-arch-gyo2-scenario-tests`（第 8 節）；`v8-arch-gyo2-guards-after-mechanism-stable` 與它的補充「機制穩定＝涵蓋它的 scenario 段通過」（第 7 節開頭、7.4）；主會話照 L17 判的「scenario 第一段涵蓋第 1 批的機制」（8.2 的「涵蓋的第 1 批機制」欄）與「`TieBreaks` 那一步延到第 4 批」（8.2 最後一列）；`v8-arch-gyo2-scenario-failure-by-contract`（8.4）；`v8-arch-gyo2-scenario-cross-engine-check` 與它的兩則補充（8.2 的「別家」欄與分類、8.4、第 14 節第 3 件）。

**修訂 2 收的轉交**（主會話，2026-11-18）：`v8-arch-gyo2-restore-reruns-registration`——第 14 節第 3 件定為重跑（3.9 的步驟 1 與「重新登記」、2.2 的 extract、3.10、7.3 O-9、8.2 S1-5／S1-7～S1-9（存檔不存速度，讀檔後「加速」不在、重新加入）、10.2 O-2／O-8、RC-O9、判定 2 案例 3、判定 3 的突變、第 6 節差分第 9 項、第 13 節）。第 1、2、4、5 件使用者還在定，沒有改。

**修訂 3 收的轉交**（主會話，2026-11-18）：`v8-arch-gyo2-dynamic-insert-no-lifecycle-rerun`——第 14 節第 2 件定為不重跑、不通知（3.10 收原話、選項、理由與後果；10.2 O-2；第 6 節差分第 1 項；註明改寫 HANDOFF `dynamic-insert-per-world` 的半句）。第 1、4、5 件使用者還在定，沒有改。

**修訂 4 收的轉交**（主會話，2026-11-18）：`v8-arch-gyo2-replace-order-author-choice`——第 14 節第 4 件定為作者選、預設先建新的（`ReplaceRequest::order`；`DropOldFirst` 的步驟與失敗後的空位置、`WorldMissing`；壞檔的原話與「Engine 不做存檔版本轉換」；5.2 加 BigMap 為點名的 Feature；2.2 加「大型靜態地圖放 World 外共用」與「存檔版本轉換」；7.3 O-9、8.2 S1-6／S1-6c、10.2 O-8、判定 2 案例 4 (a)～(d)、判定 2 的突變、第 13 節）。第 1、5 件使用者還在定，沒有改。

**修訂 5 收的轉交**（主會話，2026-11-18）：`v8-arch-gyo2-save-version-module`（不在本批，批次表第 5 條）——2.2 加一列；3.9 確認讀檔失敗的路接得住它的拒絕、不改介面（準備的錯誤帶回報者自己的碼並原樣轉出，3.5 同步）；登記檔加 `default` 欄與 `gyo_product_units` 的 `MODULES` 引數，擺得下「預設不帶的 Module」；判定 2 案例 4(a) 檢查碼被轉出。第 1、5 件使用者還在定，沒有改。

**修訂 6 收的轉交**（主會話，2026-11-18）：`v8-arch-gyo2-carry-over-mechanism`——第 14 節第 1 件定為帶過去、做成機制（新節 3.12：泛用的 `CarryOver<T>()`、不看值、值裡不能存舊 World 的參照是作者的責任、在所有準備之前、準備不能蓋掉、清單照舊 World 的標記、失敗移回；3.5 加 `ReplaceCarryOverFailed`；10.2 加 O-10；7.3、8.2 S1-10、判定 2 案例 6、判定 3 從「待定」改成判定）。O-4 的歸法使用者沒有表態，留在「一起確認」。第 5 件使用者還在定，沒有改。

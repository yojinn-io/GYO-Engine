# 2026-11-21 core、ecs、orchestration 不再登記成 Module（v8-arch-gyo2-core-not-registered）

Owner：架構（GYO2 主會話）。分支 `claude/gyo2-core-unregistered`，從 `gyo2` f25837b0。

## 為什麼

- 使用者：「？？？？？？？為何放到Core的東西還要登記Module？？？」
- #257 分層時，主會話把 core（base、math、os、io、time、threads）、ecs（kernel、entity_store）、orchestration 留在 `config/modules.csv` 裡。建置把這份檔的每一列都當成種類 `Module`，於是「Module」同時指「登記上的所有 Engine 單位」和「分層上的 `modules/`」。
- 主會話先提了改檔名、從 `dir` 推層別、加依賴方向檢查的方案，使用者：「我求求你了，別再堆屎山了。行不行。」
- 根本的答案：core、ecs、orchestration 不是 Module，每個產品都有、不用選，就不該登記。O-6 的條文本來也只寫「Module／Plugin 之間的依賴只寫在登記檔」。使用者：「可以，照這樣改」。

## 做了什麼

- `config/modules.csv` 只留 `modules/` 的六列（json、audio、logging、trace、world_time、world_logging）；它們的 `depends`／`private_depends` 拿掉 core、ecs、orchestration 的名字，只剩 Module 之間的依賴（logging 私有依賴 json；trace 私有依賴 logging；world_time、world_logging 依賴 logging）。
- `engine/CMakeLists.txt` 直接 `add_subdirectory` core、ecs、orchestration，之後照登記檔加 Module。
- core、ecs、orchestration 的 CMakeLists 把 `gyo_link_registered_dependencies` 換成手寫的 `target_link_libraries`，連結集和之前登記檔產生的一樣（`gyo_expect_links` 的期待沒變）。Module 的 CMakeLists 手寫對 core、ecs、orchestration 的 link，Module 之間的 link 仍由登記檔產生。
- 守衛 `orchestration_guard.py` 的 O6：從「engine CMakeLists 的任何 `GYO::` link」改成「登記檔（modules.csv、plugins.csv）裡的 target 被手寫 link」；登記檔讀不到 target 時 O0（fail closed）。
- `UnitRegistryTests.cmake` 的 repo 自我檢查：只要求 `engine/modules/` 底下的目錄都有列；新加「core、ecs、orchestration 的名字不得出現在 modules.csv」；`kernel` 的 dir 期待改成 `world_time`。
- `tests/common/world_logging` 的編譯失敗樣本，include 目錄改從各 target 讀（之前讀登記檔的 `dir`）。
- 文件：`docs/architecture/README.md` 的分層說明、`orchestration.zh-Hant.md` 的 O-6、`world-time.zh-Hant.md` 的登記列與 WT-4 括號、`GyoBuild.cmake`／`GyoUnitRegistry.cmake` 的說明。

## 突變

- 改寫（find 對不上、守的性質不變）：`ks-links-extra`（kernel 的 link 改在 CMakeLists）、`or-links-more`（orchestration 的 link 改在 CMakeLists）、`or-guard-real-o6-hand-link`（陽性對照改成手寫登記的 `GYO::Json`；手寫 `GYO::Math` 現在是允許的）、`or-guard-o6-dropped`（expect 改成改名後的自我測試）。
- 新增 3 個：`or-guard-o6-every-alias`（O6 又擋所有 alias）、`or-guard-o0-registers-silent`（登記檔沒有 target 時不報）、`ur-core-row-allowed`（modules.csv 出現 core 的列不報）。
- os-subsystem 清單的 `os-g8-link`、`ti-x1`、`ti-x2`、`th-x1`、`os-g13`、`os-g14` 由 PR #260 處理；#260 把前四個改成打 modules.csv 的 os／time／threads 列，本批拿掉了那些列，兩個 PR 後合併的那一個要把它們對齊到手寫的 link。

## 守衛判定的變更（AGENTS §14.4）與核准

- 驗證者：Opus 5.5／high（和主會話同一個模型；別的 agent），只讀，用真實樹、自我測試與臨時複本的反例核對 1～5。
- O6：守的性質（Module／Plugin 之間的依賴只在登記檔）仍被守住——手寫 link 到登記的 Module 報、core 往上 link 到 Module 報（實測）。緩的面：手寫 link 到 core、ecs、orchestration 的 target 放行（設計如此）；沒登記的 `GYO::` 名字放行（CMake 在 generate 時以 CMP0028 擋，前提是有人用到）。緊的面：登記檔沒有任何 target 時 O0（只在兩份加起來都沒有時；只有 modules.csv 不見時不報，configure 會先失敗）。
- 驗證者找到草稿漏寫的緩面：(1) core、ecs、orchestration 之間的方向與無環不再由登記檔的「依賴在前」守，base、math、io、world_time 沒有連結集檢查；(2) PUBLIC／PRIVATE 可見性沒有檢查；(3) `engine/` 底下 `modules/` 以外的新目錄可以不登記也不被加入；(4) Module 列的 `dir` 沒有強制 `modules/` 開頭。以前就看不到的（變數展開、原始 target 名、`link_libraries` 等）不算這次的緩面。
- 使用者：「照 (a)，核准，開 PR」。(a)＝用現有的寫法補 (1)(2) 會出事的部分：base（不 link 任何東西）、math、io、world_time 的連結集檢查，io 的「Math、Os 不在 INTERFACE」；(3)(4) 接受，記在這裡。補的突變 5 個：`cr-base-links-more`、`cr-math-links-more`、`cr-io-links-more`、`cr-io-math-public`、`wt-links-more`，5/5 killed（`wt-links-more` 第一次 wrong_failure：CMake 把長訊息折行，expect 改用 `\s+`）。

## 驗證

- 本機 macOS（arm64）全新 configure（`BUILD_TESTING=ON`、`GYO_TEST_PROFILE=full`）＋全量建置＋ctest：156/156 通過；補 (a) 後重新 configure、建置、ctest 156/156。
- 守衛在真實的樹上：`orchestration_guard.py` clean；自我測試 12 個全過（新增 3 個：手寫 Module link 報、core 寫 Module link 報、手寫 core link 放行；登記檔缺時 O0）。
- 相關突變（find 在改動的檔、或指令跑改動的測試）：audio 1/1、entity-store 1/1、kernel 2/2、orchestration 72/72、r0b3 2/2、json 2/2、logging 1/1 killed；os-subsystem 2/7 killed、1 個只在 Windows 跑，其餘 5 個（`os-g13`、`os-g14`、`ti-x1`、`ti-x2`、`th-x1`）在基準上就 stale，由 PR #260 處理。
- 證據：主 checkout 的 `build/evidence/gyo2-core-unregistered/`（`mutations.log`）。

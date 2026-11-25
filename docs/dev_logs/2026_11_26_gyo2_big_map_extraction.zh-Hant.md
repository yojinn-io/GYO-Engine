# 2026-11-26 大地圖：250 m 圖進 repo、工具規則、voxel_map_editor 登記、Linux 的 GPU 量測（`v8-arch-gyo2-big-map`）

Owner：大地圖線的任務卡。分支 `claude/gyo2-big-map-s4-extraction`，從 #342 的 head `d9c2da35` 分出（#342 等 L1 時不打擾它），開向 `gyo2`。作者 Opus 5.5／high。

## 前提（使用者的決定，`v8-arch-gyo2-decisions-2026-11-26-night`，經主會話轉達）

1. 250 m 的圖：「只放烘好的 Extraction」。`.gox` 原始檔與 Incident 都不放。
2. tools.csv：「改規則：有 release 工具時才要求每個平台都有」。
3. 250 m 圖的 Client 幀率「在Linux上確認，我開機了」；之後使用者在 Linux 主機登入桌面，「登入了，可以量」。明天的聯機測試全部在 Mac 上跑（使用者更正）。

## 做了什麼

1. **Kiriyu_Extraction_250m 進 assets**：
   - 地圖目錄 16 region、6.8 MB。`verify-map` 確認和烘焙輸出逐位元組相同，`map.json`、`regions/*.json` 是 `-text`。
   - arena 檔 `kiriyu_extraction_250m.json`：出生點取「M1 | P1-P4 blue」4 塊平台的中心，地面 11.5 m，上方 1.8 m 內沒有盒子；範圍 250×250 m。
   - 加進 `arenas.json` 與 catalog（catalog 條目由工具的 `catalog` 命令產生）。
   - 重烘的命令：`gyo_voxel_map_editor import-gox Kiriyu_Extraction_250m.gox <grid> --exclude-layer 'Spawns*' --exclude-layer 'M? | *' --exclude-layer 'REF | *' --exclude-layer 'Story *'`，再 `build-map <grid> assets/object_fps_pvp/maps/kiriyu_extraction_250m`。原始格子不放進 repo（使用者：只放烘好的）。
2. **tools 的規則**：
   - `GyoToolRegistry.cmake` 的 `gyo_export_tool_matrix`：只有登記了 release 工具時，才要求每個平台都選到 release 工具。只有非 release 工具時，每個平台都不選工具，並印出「No release tool is registered」。
   - 測試照新規則改：`ToolRegistryTests.cmake` 的 `no_release` 案例；`test_product_registry.py` 的兩案，「只有非 release 工具」改成選不到工具並印通知，整個 repo 的匯出改成「有 release 工具時才每個平台都有工具」。
   - python 端（`product_registry.py`）照 CMake 的通知，分開印兩種訊息。
   - 「登記了 release 工具、但有平台沒有」照舊失敗（`missing_platform_release` 案例沒改）。
3. **`voxel_map_editor` 登記進 `config/tools.csv`**（release=false）：它和它的測試進 L1（`GYO_TOOLS=AUTO`）。release 管線不建它（不是 release 工具）。
4. **Linux 的 GPU 量測**：結果見宣告 5.2.1。
   - RADV 本來就裝好了，沒有下載任何東西。SSH 只看到 llvmpipe，是 `/dev/dri/renderD128` 的 ACL 只給 GDM 的緣故，使用者登入桌面後就解決了。
   - 量測程式不進 repo，放在主 checkout 的 `build/evidence/gyo2-big-map/gpu/`。

## 驗證

- 本機（Intel Mac，`test` preset）：
  - 第一次跑全部測試時有 2 個失敗，都是在守舊規則的測試：`build.ci` 裡的 `test_product_registry.py` 兩案，以及 `ArenaTests` 寫死 arena 是 3 張。照新規則與新地圖改了之後，相關的 14 個測試通過。
  - 全部 `cpu|shader` 的結果在下面「驗證（續）」。
- Linux GPU：第一輪沒有記錄「實際交給視窗的幀數」，所以不採用。補上 `FramesShown`／`FramesSkipped` 重跑，13,260 幀都交給了視窗。兩輪各方向的差：中位數最多 0.011 ms、p95 最多 0.108 ms、最大單幀最多 0.807 ms。
- 驗證（續）：修好上面兩處之後，全部 `cpu|shader` 254 個裡 252 個通過。剩下的 2 個是 `gyo_os_tests` 與 `os.wake_fallback` 逾時（60 s）。單獨重跑也逾時，當時本機的 load average 是 123（其他卡同時在建置），這兩個是對時間敏感的測試。這批沒有碰到 os（改的是 assets、tool registry 的 cmake 與 python、測試、文件），所以判斷和這批無關，交給 L1 確認。

## 設計審查與修正（同日）

- 審查者 Opus／high，只拿成果物與原始要求。報告在主 checkout 的 `build/evidence/gyo2-big-map/s4-extraction-review.md`：B 1、S 5、N 6。
- 審查確認沒問題的：
  - 規則只放寬了使用者說的那一面，「有 release 工具但某平台沒有」仍會失敗。
  - arena 的出生點與範圍和地圖一致：33,022 盒在 bounds 內，4 個出生點地面 11.5 m、身體高度內沒有盒子。
- B-1：`tests/common/ci/r0b3_mutations.json` 守舊規則的兩條突變，find 已經找不到（執行器只會印 stale，不會失敗）。
  - `ci-python-silent-empty` 改指 python 轉述 CMake 通知的那一行。
  - `ci-cmake-empty-never-refused` 改指新條件，由 `cmake.ToolRegistry` 的 `missing_platform_release` 殺。
  - 新增 `ci-cmake-release-count-ignores-enabled`、`ci-cmake-release-count-all-tools`，守放寬的那一面。
- S-1：python 不再在 CMake 的 stderr 裡比對字串、自己決定印哪句，改成原樣轉述 CMake 的通知。規則只在 `GyoToolRegistry.cmake` 一處決定。
- S-2：`build-and-validate.yml` 與 `release_pipeline.py` 的訊息改成「no (enabled) release tool」；檢查這兩段文字的 `test_release_pipeline.py`、`test_workflow_gates.py` 跟著改。
- S-3：計數只算啟用中的 release 工具（停用的不會被建，所以不算）。通知寫成「No enabled release tool is registered」，並新增「只有停用的 release 工具」這一案。
- S-4：repo 匯出測試改用 CMake 的判斷：`enabled` 與 `release` 都是 "true" 或 "1"，不分大小寫。
- S-5：5.2.1 照實寫兩輪的差，4.6 ms 最大單幀的原因標成未確認。
- N：中位數的中位數改成 0.18 ms，出幀間隔改寫為「由 CPU 與 GPU 中較慢的一方決定」，「沒量」補上載入時間與解析度，dev_log 的「進 CI」改成「進 L1」。arena 的 `cell_size`（沒人讀）與 ArenaTests 寫死份數，照其他 arena 的現況不改。
- 修正後重跑：`build.ci`、`cmake.ToolRegistry`、`object_fps_pvp.logic` 3/3 通過。突變 `ci-python-silent-empty`、`ci-cmake-empty-never-refused`、`ci-cmake-release-count-ignores-enabled`、`ci-cmake-release-count-all-tools` 4/4 killed，件數一致。

# PvP v7.1 windows-long-path：發行包解壓到深資料夾時也讀得到 shader（P1～P5）

日期：2026-10-25。分支 `claude/pvp-v71-release`（從 PR #95 的 `a0e6668` 切出，再 `--no-ff` 併進 OS 子系統的 `claude/trusting-joliot-f13b78`，D199）。

**狀態：完成（2026-10-27：實機驗收 P5-6 通過，Windows 專用突變見「Windows 機器的結果」）**。計畫與宣告見 [windows-long-path.md](../object_fps_pvp/plans/v7.1/windows-long-path.md) 的「P1～P5 的實作（D199）」一節；架構（A1～A3：`engine/os`、`engine/io` 的契約與唯讀原生後端）由 OS 子系統任務做，見 [os-subsystem PLAN](../architecture/plans/os-subsystem/PLAN.md) 第 8 節。使用者經該會話轉達：「讓v7.1會話開始P1～P5的任務」。

## 做了什麼

- **P1（`engine/os/src/windows/Files.cpp`）**：`OsCallPath` 先用 `GetFullPathNameW` 取完整路徑，長度達 248（`MAX_PATH - 12`）以上、而且在磁碟機代號上時，交給 API 的是 `\\?\` 加完整路徑。短路徑、UNC、裝置、呼叫端自己給的 `\\?\` 形式、取不到完整路徑的路徑都照舊。`MakeAbsolute` 改用同一個取完整路徑的函式，結果仍不帶前綴（os.md Invariant 5）。行為變更在開始時宣告（os.md Invariant 9 ★），契約寫進 `docs/architecture/os.md` 的 Contracts／Files。
- **P3（`engine/asset`）**：`NativeFileAssetSource::ReadAll`（類別名與 API 不變）改成 `engine/io` 的 `NativeFileSystem` 的薄轉接層：相對路徑先 `Os::Files::MakeAbsolute`，再 `FileUriFromNativePath`、`Helpers::ReadAllBytes`。`AssetCatalog` 讀 catalog、`ContentManifest::Load` 讀 `content.json` 都改用它（各自的錯誤碼與訊息不變）。守衛 allowlist 中 asset 的 3 條 R4 刪除。
- **P2**：讀取失敗的 detail 用 `Base::CauseDetail` 保留原因：原生路徑、OS 錯誤碼、`path_bytes`。例：`NotFound: C:/…/x.dxil; NotFound: win32=3; path_bytes=271`。呼叫端給相對路徑時，前面再加呼叫端的路徑。
- **P4**：resolver 根目錄與 catalog 路徑 7 處改用 `Os::Files::PathToUtf8`（不再是 `path::string()` 的 ANSI code page）：`PvpApplication.cpp`、`object_fps` 與 `object_fps_v2` 的 `ObjectFpsApplication.cpp`、ui_editor 的 `ReadOnlyAssetCatalog.cpp` 兩處、`ContentManifest.cpp` 兩處。產品與 `gyo_ui_editor_core` 明寫連結 `GYO::Os`。
- **P5 的工具**：產品的驗收目錄加 content probe（`build/acceptance/object_fps_pvp/content_main.cpp`，照 Client 開視窗前的順序讀 manifest、catalog、所有 shader bundle 與所有資產，不需要 GPU）與 `content_deep_path.py`（把發行包複製到 P＝200 的資料夾再跑 probe；Windows 上要求最長的 `.dxil` 路徑超過 260），登錄成產品檢查 `content_deep_path`（quick 與 release、四平台）。

## 驗證與經過

- **Red**：只加測試的 `80b7208`，run 40176646387。windows-x64 的 `gyo_os_tests`（L1 與 toolchain 兩列）只有「read an existing file under a path longer than 300 characters」失敗：`cannot open a 402-byte path: win32=3; path_bytes=402`，和事故 log 的錯誤碼相同。其他平台通過。
- **Green**：`89af354`，run 40177623030，四平台全部成功。windows-x64 的 `gyo_os_tests` 通過（含 402 位元組的讀取）；產品檢查 `content_deep_path` 在 windows-x64：`{"prefix_length": 200, "dxil_files": 4, "longest_dxil_path": 318, "probe_exit": 0}`，probe 讀了 1 個 bundle、2 個 program、24 個資產。P5-1～5 的宣告全部成立。
- 本機（macOS arm64）：`ctest` 93／93 通過；`content_deep_path.py` 在 P＝200 與 P＝146 通過（macOS 只有 metallib，所以沒有 `.dxil` 可數）；藏起一個 shader artifact 時 probe 以 exit 1 失敗，訊息帶 `errno=2; path_bytes=319`。
- **突變**（D184：新的、find 在改動範圍的）：
  - 新增 5 個到 `docs/architecture/plans/os-subsystem/scripts/mutations.json`：`p2-asset-detail-drops-cause`、`p3-relative-path-not-absolute` 本機 killed；`p1-no-long-path-form`、`p1-long-path-from-too-high`、`p4-manifest-ansi-path` 只能在 Windows 跑，未執行。Red 的 run 就是 `p1-no-long-path-form` 的內容（未修正的樹），已在 windows-x64 失敗。
  - 既有、find 在改動檔案上的：`os-g2-macro`、`os-g10-filesystem-op`、`16-observed-falls-back`、`sdl-g1-product-sdl-call` 都 killed。`os-f4-windows-acp`、`os-f5-windows-notfound` 只能在 Windows 跑。
- **判定的變更（AGENTS.md §14.4）**：
  - `FilesTests.cpp` 的「missing path longer than 260」由只記錄（NotFound 或 InvalidPath 都過）收緊為 NotFound，並要求 detail 帶 `path_bytes`、不含 `?`。只收緊。
  - `NativeFileAssetSourceTests.cpp` 的 detail 判定由「等於路徑」改為「以 `NotFound: ` 開頭、含路徑、含 OS 錯誤碼與 `path_bytes`」。原因：detail 的形式本身就是 P2 的契約變更。它守的性質（失敗時看得出是哪個路徑）由新判定中的「含路徑」繼續守住。這不是受入判定或 Design Invariants 的守衛測試。

## 證據

- Red：run 40176646387 的 `Quick acceptance / toolchain / windows-x64` 與 `L1 / windows-x64` 的 log（`FilesTests.cpp(390)`）。
- Green：run 40177623030 的 `L1 / windows-x64` 與 `Quick acceptance / object_fps_pvp / windows-x64` 的 log。
- 突變：本機執行輸出（git 忽略）。重跑：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/test --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json --only p2-asset-detail-drops-cause p3-relative-path-not-absolute os-g2-macro os-g10-filesystem-op`；`python3 tests/object_fps_pvp/run_mutations.py --build-dir build/target/_build/test --only 16-observed-falls-back sdl-g1-product-sdl-call`。

## 還沒做

- 實機（P5-6）：Windows 上解壓到 P＝156 與 P 約 210 各啟動 Client 一次，對照事故 log 第 2 行。需要使用者。
- Windows 專用的突變（P1 兩個、P4 一個、os-f4、os-f5）要在 Windows 機器上跑；建議排在 lan15-windows 用 Windows 機器時一起跑（OS 子系統會話的建議），發行時的完整突變（D185）若在 Windows 上跑也會涵蓋。
- OS 子系統會話審查了 engine/os 與 engine/io 的改動（`d56a8fc..432142c`），只指出 Engine 擁有的檔案裡出現消費端的版本名（os.md Invariant 7、AGENTS §7）；`1c16f8d` 改成 os-subsystem PLAN 的交接代號或行為描述（4 處，含審查沒列到的 `NativeFileAssetSourceTests.cpp`）。
- 下一個失敗點：產品自己開的 `arenas.json`（約 P≥219）、log 與 trace（約 P≥229），屬於發行後的 os-files-write、os-trace-writers。

## Windows 機器的結果（2026-10-27，D201、D211）

使用者在 Windows 11 機器上跑（發行線 `3b612cf` 的程式碼；MinGW 15.2 與 MSVC 19.51 兩套建置）。完整紀錄：[D201 的 Windows 清單](2026_10_27_pvp_v71_d201_windows.zh-Hant.md)。

- **P5-6 實機**：通過。本機 MinGW 建置的包複製到 `D:`（本機磁碟），P＝156（最長檔案路徑 272）與 P＝210（最長 326）都啟動成功，開 shader 的錯誤不再出現，`graphics ready driver=direct3d12`。用的是本機 MinGW 包，不是 CI 的 MSVC 發行包；讀檔的程式路徑相同。
- **突變**：`p1-no-long-path-form`、`p1-long-path-from-too-high` 在 MSVC 建置上 killed。在 MinGW 上，兩者的基準線都因為 libstdc++ 的 `remove_all` 在長路徑上卡住而逾時，所以沒跑；這是 MinGW 的問題，MSVC 上沒有出現。`p4-manifest-ansi-path` survived：這台的 ACP＝65001，ANSI 就是 UTF-8，突變後行為不變，所以是環境因素，不是測試失去檢出力。它之後的處理（程序以 manifest 固定 UTF-8 時會變成等價突變）是 OS 子系統的開放決策 19，等使用者決定。
- 附帶觀察（不擋發行）：MinGW 包裡沒有 MinGW runtime 的 DLL，在只有系統 PATH 的機器上 Client 起不來（`0xC0000135`）。發行用的是 CI 的 MSVC 包，不受影響。這件事記在 Engine 的建置基線提案。

## p4 退場（D212，2026-10-27）

OS 子系統的 os-utf8-codepage（Engine D64～D67）讓所有 Windows 執行檔以 manifest 把 ANSI code page 固定成 UTF-8。之後 `path::string()` 在我們的程序裡也是 UTF-8，`p4-manifest-ansi-path`（`ContentManifest.cpp` 的 `PathToUtf8` 換回 `path.string()`）在任何帶 manifest 的 Windows 上行為不變，是等價突變。照 D185「等價突變移出目錄並寫明理由」，與同類的 pu-x6、pu-x7（Engine D66）一樣退場，從 `docs/architecture/plans/os-subsystem/scripts/mutations.json` 刪除。

- 原本守的性質：catalog 與 resolver 根目錄的路徑以 UTF-8 交給 Engine，不經系統 code page。
- 之後由什麼守：manifest 的設定期檢查（每個 Windows 執行檔都帶 `activeCodePage=UTF-8`），以及 `gyo_os_tests` 檢查程序的 ACP 是 65001 的結構檢查（OS 子系統的 R6）。P5 宣告第 4 項的測試（ANSI code page 外的根目錄載入 `ContentManifest`）照常保留，在 manifest 下它驗的是「讀得到」，不再能區分轉換方式。
- 決定：使用者把 p4 交給本會話（「p4 的處理一併交給它」，經 OS 子系統的會話轉達），本會話選退場；另兩個選項（改寫成任何機器都判定得了的形式、只在刻意不帶 manifest 的程式上跑）都要新增只為這條突變存在的程式或測試，不採用。

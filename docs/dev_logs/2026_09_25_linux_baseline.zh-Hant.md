# Linux 新環境專案盤點

日期：2026-09-25。分析基準：`bbc1832`。

這是儲存庫結構、建置契約、主要執行路徑與可執行測試的基線盤點，
不是所有 C++ 程式的逐行審查，也不是 Linux 完整產品驗收。
開始時 Git working tree 乾淨。本次只新增本報告，未修改實作或登錄。

## 同日環境更新後複查

下方原始盤點保留第一次檢查的狀態；環境更新後的最新結果如下：

- CMake／CTest **4.4.3**、GCC／G++ **14.2.0** 可用；Ninja 維持 **1.11.1**。
  系統預設 `gcc/g++` 仍為 13.3，因此 configure 明確指定 `gcc-14/g++-14`。
- 原先缺少的 Linux GUI／audio／device 開發依賴現在都能由 `pkg-config` 找到；
  `vulkaninfo`、Xvfb、xauth 已安裝。
- `cmake --preset core -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14`
  configure 成功；`cmake --build --preset core --parallel 8` 成功。
- `ctest --preset core --parallel 4 --output-junit core-tests.xml`：**17／17 通過**，
  包含 `build.ci` 的 129 項與 `build.assembly` 的 14 項 Python 測試。
  首次檢查的九項 CMake 版本失敗已解除。
  測試報告位於 `build/target/_build/core/core-tests.xml`。
- Vulkan loader **1.3.275**；可辨識 NVIDIA GeForce RTX 3060
  （Vulkan 1.4.329，driver 595.84）、AMD RADV REMBRANDT
  （Vulkan 1.4.318，Mesa 25.2.8）以及 llvmpipe CPU fallback。
- Xvfb 顯示連線與 CI 使用的 **Xvfb + Lavapipe** Vulkan 診斷均成功。
  沙箱內的 GPU／display 存取失敗在沙箱外重跑後解除，並非主機驅動故障。
- Configure 有 doctest 舊 CMake compatibility 的非致命警告；編譯時
  `CapsuleQueries.cpp` 有 GCC 診斷，未阻擋建置。本次未修改程式處理警告。

**結論：基礎工具與 core 開發環境已可用。** 本次仍未建置完整 PvP／UI editor，
也未完成產品 shader、GPU 渲染或 LAN 驗收；這些不能由 core 通過推論。

隨後依使用者要求，已將使用者的預設 C／C++ 環境設為 GCC 14：
`~/.local/bin/{gcc,g++,cc,c++}` 指向 `/usr/bin/gcc-14` 或 `/usr/bin/g++-14`，
`~/.profile` 與 `~/.bashrc` 設定 PATH、CC、CXX；原檔各保留
`.gyo-before-gcc14` 備份。新的 login／interactive shell 皆通過 C17 與 C++20
編譯執行檢查；新的 `core-default-check` CMake 建置目錄未指定 compiler 參數，
自動選到 GCC／G++ 14.2。Ubuntu 系統 `/usr/bin/gcc` 仍保留原來的 13.3，
已開啟的程序需重新載入 shell 設定或重啟才能取得新的使用者環境。

## 同日完整建置與試運行

使用 `test` preset 建置目前登錄啟用的 `object_fps_pvp`、UI Editor、所有相應
Engine adapters、shader host tools、測試與驗收 probe。v1／v2 仍維持登錄停用，
本次未將它們納入建置。CMake 4.4.3／GCC 14.2 完整建置成功，未修改 C++ 實作。

- 一般 CTest：**27／27 通過**，報告 `build/target/_build/test/logs/tests.xml`。
- Shader host CTest：**3／3 通過**，報告 `build/target/_build/test/host-tools/shader-tests.xml`。
- 共通 GPU CTest：**1／1 通過**，獨立 Xvfb + Lavapipe，實際使用 Vulkan／SPIR-V；
  報告 `build/target/_build/test/logs/environment-recheck-gpu-tests.xml`。
- 公共 Gateway 與 PvP Go module：`go test -race -count=1 -timeout 90s ./...` 通過。
- 真實 Match／Gateway／兩個 Client 驗收成功：41 份共同 authority snapshots 一致，
  移動、cadence、timeout、leave／rejoin、Gateway 更換後清場、Match 失聯回 Lobby
  均通過。雙 GUI probe 在桌面圖形環境渲染出彼此，透過 SDL W 輸入產生約
  1.94 單位權威位移，ESC 回 Lobby 成功。
- 驗收結果、圖像與逐程序記錄：`build/target/_build/test/logs/pvp-network/`。
  `result.json` 的 `passed` 與 `gui_render_and_movement` 均為 true。
  這是 localhost 驗收，仍不代表實體雙機 LAN 通過。

Zed 已使用 `/usr/bin/clangd-19`；根目錄受忽略的 `compile_commands.json`
連向完整 test 資料庫。使用者 `~/.config/clangd/config.yaml` 僅對此儲存庫排除
三種 clangd 不接受的 GCC module-scanning 參數，未改建置參數。
`BinaryLoader.cpp` 的 clangd 檢查零錯誤；`RuntimeLoop.cpp` 無解析錯誤，但
clangd ExtractFunction 自我測試有三次失敗。`.gitignore` 增加 clangd index cache。

本機啟動：

```sh
python3 build/target/run-object-fps-pvp.py
```

此為 `build/target` 中的本機產物輔助腳本；確認 ports 未被占用後依序啟動
Match（127.0.0.1:27016）、Gateway（HTTP 8080／UDP 27015）與 Vulkan Client。
在 Lobby 選 Create + Join；WASD 移動、Mouse 轉視角、ESC 回 Lobby。
關閉 Client 會停止此腳本啟動的 Match／Gateway，不會清理其他程序。
記錄位於 `build/target/_run/object_fps_pvp/<timestamp>/`。

UI Editor 已建置於 `build/target/toolchain/bin/gyo_ui_editor`，其 core、CLI、
content validation 與 preview tests 通過；本次持續開啟的試運行視窗是 PvP Client。

## 目前專案

GYO 是 C++20 靜態連結的模組化遊戲引擎，另有 Python 建置／驗收支援與 Go 網路服務。
共有 855 個受版本管理的檔案（本報告加入前）。主要程式／建置檔粗計：
Engine 約 18,469 行、Apps 約 36,448 行、Tools 約 5,499 行、Go 公共服務 535 行、
Tests 約 19,052 行、Build 支援約 5,659 行；包括生成程式，不代表獨立手寫程式量。

| Owner | 目前責任 |
|---|---|
| `engine/base, io, runtime, asset` | 基礎型別、檔案／串流、生命週期、資產 catalog／載入／快取 |
| `engine/input, collision, model` | 輸入動作、幾何查詢、模型／動畫／CPU skinning |
| `engine/render, text, ui, platform` | 渲染契約與 SDL adapters、文字、JSON UI、視窗事件 |
| `apps/object_fps` | 第一代單機 FPS、campaign／戰鬥；登錄停用 |
| `apps/object_fps_v2` | 骨架敵人、部位傷害與診斷；登錄停用 |
| `apps/object_fps_pvp` | 目前唯一啟用遊戲；伺服器權威聯網 MVP |
| `tools/ui_editor` | 獨立 UI 設計工具，GUI 是預設／release variant；另有 CLI |
| `tools/object_fps_preview` | 明確依賴 v1 的本機專用 preview，非 release 工具 |
| `services/gyo_gateway` | 公共 HTTP、session、TCP／UDP framing，不擁有遊戲規則 |
| `build/cmake, ci, acceptance` | 產品組合、部署、封裝與外部驗收 |

`engine/config/projects.csv` 和 `tools.csv` 決定產品選取。
明確指定停用產品也會被 registry 拒絕；恢復 v1／v2 前需先調整其登錄。
`dev` 預設選取啟用遊戲且不含工具／測試；`test` 啟用測試與預設工具；
`core` 不選任何產品，驗證 backend-neutral 模組。

## 架構與資料流

依賴方向是 Application／Editor → Engine。抽查 Engine 實作與 CMake 未發現
直接引用具體 Object_FPS 程式的依賴；此結論不取代產品刪除／複製的實際建置驗證。

PvP 的主要路徑：

```text
SDL Client → HTTP / UDP → PvP Go Gateway → loopback TCP → C++ IPC Host
                                                           ↓
                                                  MatchRuntimeHost
                                                           ↓
                                                PvpMatch + Collision
                                                           ↑
                                               Engine FixedTickRuntime
```

Engine 處理固定步進計算；產品 Host 擁有 clock、thread、等待與停止；
Match 擁有世界規則；IPC 交換 owning commands／snapshots。
目前 authority 60 Hz、input 名義 30 Hz、snapshot 名義 20 Hz，最多兩人。
此切片提供加入、移動、碰撞與互相顯示；尚未提供射擊、傷害、跳躍、
prediction／reconciliation 或持久化。不能把 v2 的全部功能視為已接入 PvP。

Client 與 Runtime 使用兩份產品所有的 versioned Protobuf contract。
Go 產品 module 明確依賴公共服務 module，公共服務不 import 產品。
Gateway 是獨立建置目標，部署於 `_services`，不包含在 native product archive。

資產由 `content.json`、catalog 與 shader bundle 描述，Python 負責建置時組裝；
Runtime 從執行檔相對目錄載入，不要求 Editor 或 Python。
Linux 渲染使用 SDL_GPU／Vulkan／SPIR-V；shader host tools 另有固定版本的
SDL_shadercross、SPIRV-Cross 與 DXC 依賴。初次產品建置需要取得這些來源／工具。

## 新環境狀況

| 項目 | 實測 | 判斷 |
|---|---|---|
| OS | Ubuntu 24.04.5 LTS | 與 Linux CI 的 Ubuntu 24.04 系列相符 |
| CMake | 3.28.3 | **確定阻塞**：專案要求 ≥ 3.30 |
| Compiler | GCC／G++ 13.3 | CI 指定 gcc-14／g++-14，本機未找到；尚未證明 GCC 13 不相容 |
| Ninja、pkg-config | 可找到 | 工具入口存在 |
| Python | 3.12.3 | 可執行現有 Python 測試 |
| Go | 1.27.1 | 高於 module 的 1.23 要求；本次測試通過 |
| SDL 系統開發依賴 | pkg-config 找不到 x11、wayland-client、xtst、alsa、libpulse、libudev、xkbcommon、libdecor-0、egl、gl | GUI 建置環境尚未齊備 |
| GPU 驗證工具 | 未找到 vulkaninfo、Xvfb | 未驗證 Vulkan 裝置或 GUI；已有 ICD JSON 不等於裝置可用 |

`cmake --preset core` 在讀取 preset 時就因最低版本失敗，未進入 C++ 編譯。
不能把此結果判定為程式移植失敗，也不能宣稱 C++／GPU 已可用。
本次沒有安裝系統套件、降級專案要求或搬用 Windows build cache。

## 驗證結果

| 驗證 | 結果 |
|---|---|
| Python `tests/common/ci` | 129 項中 123 通過；3 failure、3 error 均因 CMake 版本 |
| Python `tests/common/assembly` | 14 項中 11 通過；3 failure 均因 CMake 版本 |
| 公共 Gateway `go test ./...` | framing、session 通過；httpserver 無測試 |
| PvP module `go test ./...` | gateway、adapter 通過；其餘 packages 無測試 |
| 三個遊戲的 content／catalog 契約與直接引用檔案 | 通過；v1 38、v2 46、PvP 48 個檔案（含 catalog） |
| Git tracked path 大小寫碰撞 | 未發現；不代表已驗證所有 C++ include 大小寫 |
| C++ build／CTest、shader、GPU、跨程序／實體 LAN | 未完成，不能引用歷史紀錄當作本機通過證據 |

Go 網路測試首次受沙箱 socket 限制；經允許在沙箱外重跑後通過。
PvP 使用指定的 `google.golang.org/protobuf v1.36.11`，測試 cache 放在 `/tmp`。
catalog 檢查不包含模型解碼、所有 JSON 間接引用或 shader 編譯。

## 需要追蹤的問題

1. **環境尚未具備原生產品建置條件。** 優先補 CMake、對齊 GCC 14，以及 workflow
   已列出的 Linux 開發依賴；不要為配合舊 CMake 任意降低專案最低版本。
2. **PvP 的 CI 驗收範圍小於完整聯網功能。** `build/acceptance/object_fps_pvp/checks.json`
   僅登錄 `network_probe --self-test`；該分支檢查 UDP 編解碼、sequence wrap 與錯誤長度，
   不啟動 Gateway／Match。`run_network.py` 與 GUI probe 已存在，但現有 workflow
   未呼叫它們，也未安排 Go unit tests。Native CI 成功不能代表完整聯網通過。
   後續接入時應維持產品 owner 選取，不向共通流程添加 PvP 專用分支。
3. **複製後的內容與實際執行範圍需要釐清。** v2／PvP 的 78 個共同檔案路徑中有
   60 個內容完全一致。PvP `sources.cmake` 仍有未被當前 CMake 使用的
   `APP_DOMAIN_SOURCES`／`APP_SUPPORT_SOURCES`；實際主要使用 `PVP_DOMAIN_SOURCES`
   與 `src/Pvp`。PvP 資產目錄約 47 MB，仍有 campaign／角色／武器內容。
   這是維護與封裝範圍的訊號，不足以直接判定全部可刪，也不構成把 gameplay
   搬入 Engine 的理由。
4. **操作文件偏向先前 Windows 路徑。** PvP 手動流程主要提供 PowerShell 範例，
   README 的 v1 範例需要先啟用登錄。建立 Linux 開發基線時應補上當前啟用產品
   的 Bash 啟動順序與驗證證據。

## 重新開始的順序與完成條件

1. **環境基線**：補齊 CMake ≥ 3.30、GCC 14、Linux 開發套件與驗證工具；
   精確套件清單以 `.github/workflows/build-and-validate.yml` 為依據。
2. **Engine 基線**：使用全新的 Linux `core` build tree，完成 configure、build、CTest；
   重跑 Python build／assembly suites，清除本次環境造成的九項失敗。
3. **產品基線**：先只選 `object_fps_pvp`，完成 Client、Match、Gateway 與 shader 建置。
   驗證部署後從非 source 工作目錄啟動，再跑 localhost 雙客戶端與斷線驗收。
4. **圖形與工具基線**：驗證 Vulkan、GUI client、UI editor；區分軟體 GPU、
   實體 GPU 與實體 LAN 證據。headless 通過不能取代 GUI 或 LAN 驗收。
5. **回到功能開發**：根據下一個目標選擇 PvP 或重新啟用 v2；遺留內容整理與
   CI 補強分別保持明確範圍，避免一開始就進行全庫重構。

注意：`build/` 含受版本管理的建置與驗收程式，不能整個刪除。
新的 cache／產物應放在 `build/target/`；更換 compiler 使用新的建置目錄。

本次沒有 Architecture Delta、Dependency／Ownership 或 Data Contract 變更。

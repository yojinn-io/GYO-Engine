# Engine os-path-utf8-sweep：剩下的窄字串路徑改 UTF-8

日期：2026-10-27。**完成**：Red 的 L1 如預期（[run 40431320545](https://github.com/yojinn-io/GYO-Engine/actions/runs/40431320545)，`f86f8e00`）；Green 的 L1 （[run 40433158383](https://github.com/yojinn-io/GYO-Engine/actions/runs/40433158383)，`bfea4f6b`）全部成功；windows-x64 上 Red 那次失敗的 6 項（engine_tests、object_fps_pvp.cpu、match_ingress_cli、gyo_ui_editor.core、content_cli、preview）都通過，ctest 95／95。
計畫見 [os-path-utf8-sweep](../architecture/plans/os-subsystem/PLAN.md)（第 7.11 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-path-utf8-sweep」；開工前的決定是 D63。

## 做了什麼

在 Windows 上，MSVC 的 `path::string()`、`generic_string()` 和以窄字串建 `path` 都用 ANSI code page：code page 表示不了的字元（日文、韓文）在 `string()` 會丟例外，UTF-8 文字建成 `path` 會被讀錯。這一批把使用中的程式碼剩下的這類轉換改成 UTF-8。

- engine/os：新增 `Os::Files::PathForMessage`，給訊息與 log 用的 UTF-8 文字。分隔字元照路徑原樣（Windows 有 `\` 就保留），所以 ASCII 路徑印出的位元組和以前的 `path::string()` 相同（D48）。永不失敗，不用 ANSI code page；沒有 UTF-8 形式的單位換成 U+FFFD（Windows 是落單的 surrogate；POSIX 是每個開頭不成 UTF-8 的位元組）。結果有損，不拿來再開檔（那是 `PathToUtf8`）。
- 產品（object_fps_pvp）：
  - trace 名稱的運算（`NumberedTracePath`、`MatchShotTracePath`、`MatchIngressTracePath`）改用 `u8string()`。
  - 錯誤訊息、Match 的啟動行（`arena_file=`、trace 檔名）、Client 的啟動行改用 `PathForMessage`。
  - Client 的 `--movement-trace`、`--log` 改用 `Os::Files::PathFromUtf8`；不是 UTF-8 的參數以結束碼 2 拒絕（與 D50 一致）。
  - arena 清單的項目名、content.json 的 shader bundle 名稱是 JSON 文字，以 UTF-8 轉成路徑。
- 編輯器（ui_editor）：
  - 路徑欄位以 UTF-8 填入，分隔字元照路徑原樣，Windows 上的顯示不變；沒有 UTF-8 形式的路徑留空。讀回時用 `PathFromUtf8`，欄位不是 UTF-8 就不掛載，在狀態列顯示錯誤。
  - 排隊中的掛載直接保存 `path`，不再經過 `string()` 往返。
  - catalog 解析出的路徑以 UTF-8 轉回 `path`。
  - 狀態列、標題、Read-only 標示、`--validate` 的 `valid:` 行改用 `PathForMessage`；存檔遇到外部修改時的覆寫確認，不再複製路徑。
  - `gyo_ui_editor_preview` 以 PRIVATE 連結 `GYO::Os`；工具本來就依賴它，不是新的依賴方向。
- 驗收程式（object_fps_pvp）：`gui_room.hpp` 的暫存檔名改成在 `path` 上接 `.tmp`，`gui_main.cpp` 的結尾訊息改用 `PathForMessage`；gui 目標明寫連結 `GYO::Os`。
- 拿掉公開的 `Engine::Platform::Sdl::PathFromUtf8`。`BaseDirectory()` 在內部照舊把 SDL 給的 UTF-8 位元組變成路徑，不加 platform/sdl → GYO::Os 的邊。
- engine/render 的 `ShaderLibrary`：bundle 內的相對路徑以 UTF-8 往返。這一處是在最後的 grep 找到的，開工時的盤點漏了 `generic_string()`。
- engine/asset 的 `ContentManifest`：content.json 的 catalog 名稱以 UTF-8 接到根目錄（審查找到的）。
- 主機 shader 工具（D63，使用者選的）：`.string()`、`.generic_string()` 共 23 處，以及從 JSON 與 include 文字建 `path` 的 6 處，改用工具內的 u8string 輔助函式。D63 數的是 12 處 `.string()`；同一個檔案裡同類的站點一起改了。它是另外設定的 host 專案，不連結 engine/os，建置的邊不變。
- 共通測試：新增 `tests/common/support/PathText.hpp`（`Engine::Test::Utf8Path`、`GenericUtf8Path`），asset 與 UI 的測試用它把路徑交給 Engine；`std::ofstream(p.string())` 改成 `std::ofstream(p)`；兩個標頭衛生測試的相對名稱也改成 UTF-8。
- 未啟用產品依 D63 不改，遷移需求記進 [foundation-followups/inactive_products.md](../architecture/plans/foundation-followups/inactive_products.md)。前四批漏記的節也一起補上。

## 驗證與經緯

- Red 測試（`5ae2000b`，實作仍是舊的）。在 macOS 上新舊實作都通過，Red 只會出現在 windows-x64：
  - 產品 DiagnosticFileTests：trace 名稱與拒絕訊息。
  - `match_ingress_cli_check.cmake`：Match 讀放在 `試合-한` 的 arena，啟動行要以 UTF-8 寫出它。
  - EditorCoreTests：掛載後解析出的路徑、匯出檢查、在這種資料夾下匯出。
  - PreviewAssetTests：GUI 的輸出路徑欄位與 `試合-한` 根目錄的掛載。
  - ArenaDigestTests：arena 的開檔訊息，以及清單裡的 `試合.json`。
  - ContentManifestTests：content.json 的 catalog 名稱 `記録-한.json`。
  - EditorCliTests.cmake：`--validate` 放在 `試合-한` 資料夾的文件，結束碼 0，`valid:` 行以 UTF-8 寫出資料夾名。
  - PreviewAssetTests 與 ArenaDigestTests 是實作時找到的同類站點，後兩組是審查找到的。都在推送前併進同一個 Red commit（本機的 commit 以 `git commit-tree` 重組，工作樹沒有動）。
  - 寫的時候，編輯器的案例在 macOS 失敗一次：我寫的 catalog 少了 `font.default`，預設文件先在驗證時失敗。補上後通過。
- xhigh 審查（1 位）。審查者只從 git 讀固定的 commit，因為同時有突變在改工作樹。
  - major 2 個，都已修。兩處都是 `->string()`，盤點的 grep 只搜 `.string()`，所以漏了；在 Windows 上會讓程式終止：
    - 編輯器存檔遇到外部修改時的覆寫確認。
    - `--validate` 的 `valid:` 行。
  - minor，已修：
    - content.json 的 catalog 與 shader bundle 名稱以窄字串接到根目錄。
    - 路徑欄位不是 UTF-8 時，會靜默改掛整個根目錄。
    - Windows 專用突變的 find 跨多行，CRLF 的 checkout 上會 stale。
    - os.md 沒有記 PathForMessage。
  - minor，記為缺口：ShaderLibrary 的測試沒有檢出力；編輯器的欄位轉換、Client 拒絕非 UTF-8，沒有直接的測試。
  - 確認的：
    - PathForMessage 符合契約。
    - 五組 Red 測試在 POSIX 的舊實作通過、在 windows-x64 的舊實作失敗、在新實作通過。
    - 新的依賴邊都是工具或產品指向 Engine；platform/sdl 沒有新的邊。
    - 沒有產品知識進入 Engine。
- Red 的 L1（[run 40431320545](https://github.com/yojinn-io/GYO-Engine/actions/runs/40431320545)，`f86f8e00`）：如預期，只有 windows-x64 失敗（L1 與 toolchain 的快速驗收；CI validation 與 CI gate 跟著失敗）；macos-arm64、macos-x64、linux-x64 全部通過。windows-x64 失敗的 6 項都是新的 Red 案例：
  - engine_tests：只有 ContentManifest 的新案例失敗，catalog 名稱被讀成 `è¨˜éŒ²-í•œ.json`，所以 NotFound。
  - object_fps_pvp.cpu：只有 3 個案例失敗。trace 名稱的案例丟出 MSVC 的「No mapping for the Unicode character…」；arena 開檔訊息的兩個新 CHECK；arena 清單的 `試合.json`。
  - match_ingress_cli：啟動行的 `arena_file=` 變成例外訊息。
  - gyo_ui_editor.core：只有「a resolved asset path keeps the characters of its root」。
  - gyo_ui_editor.content_cli：`--validate` 以 0xc0000409（未捕捉的例外）結束。
  - gyo_ui_editor.preview：0xc0000409。前兩個測試函式沒有印出 FAILED，所以停在最後的新函式〔推論：輸出看不出是哪一行〕。
- 本機（macOS Intel x86_64，修正後）：core 33／33；產品組態 95／95；`object_fps_pvp.worker` 與 `close_timing` 另外逐一串行 2／2；host-tools 的 shader 測試 3／3。
- 突變（新增）：
  - pu-x1、pu-x2 killed（POSIX）。
  - pu-x3～pu-x7 只在 Windows 跑，留給 Windows 機器（D62）。我在 macOS 上確認了 pu-x6、pu-x7 的取代能編譯。
- 突變（相關的既有）：
  - engine/os 目錄 10／10。
  - 輸入與呈現 12／12。第一次的基準失敗是因為沒有設 `SDL_VIDEODRIVER=dummy`，屬於環境問題；照那份計畫的紀錄補上後重跑。
  - 產品 13／13。
- 產品的突變有 4 條，因為 find 的字串改了，移到新的程式行：`v7-flow-trace-open-failure-ignored`、`v7-flow-trace-name-suffix-accepted`、`v7-flow-trace-name-keeps-commands-suffix`、`tno-number-after-commands`。
- 產品在 Windows 上的行為不另寫突變：產品的突變 runner 不支援平台限定。檢出力的證據是 Red 的 L1，也就是舊實作在 windows-x64 失敗。
- 拿掉的測試：EntryTests 的「PathFromUtf8 keeps non-ASCII text」跟著函式一起消失。依 AGENTS §14.4 說明如下。
  1. 它守的性質：UTF-8 文字變成路徑時位元組不變。
  2. 放寬的面：`BaseDirectory` 內部的同一個轉換，現在只在 base path 本身含非 ASCII 時才有測試看得到。
  3. 收緊的面：BaseDirectory 的測試改成比對 UTF-8 文字，比以前比對路徑更嚴。
  4. 產品用的轉換（`Os::Files::PathFromUtf8`）由 FilesTests 的往返案例（含日文、é）守住。
  5. 證據在 `tests/common/platform_sdl/EntryTests.cpp` 與 `tests/common/os/FilesTests.cpp`。

## 已知缺口

- shader 工具與驗收探針的 argv 在 Windows 上仍是 ANSI code page。要用 Windows API 才能改，不在本批範圍，記為未結。
- `ShaderLibrary` 的新測試在 ANSI code page 1252 上沒有檢出力：1252 對 UTF-8 位元組的往返不失真，只有雙位元組的 code page（932 等）才會讀錯。
- `IngressStatisticsTests` 的兩處 `filename().string()` 是 ASCII，刻意不改：改了會動到產品突變比對的期望文字。
- 事後宣告的差異，交使用者確認：
  - shader 工具：不是 UTF-8 的 include 名稱會以錯誤結束；非 ASCII 路徑在 Windows 主控台上顯示會變亂碼；depfile 改成 UTF-8。
  - ShaderLibrary：收到不是 UTF-8 的 manifest 路徑時，錯誤訊息依平台不同。
- 交使用者決定：os.md Invariant 5、7 的條文，要不要寫明 PathForMessage 的例外。

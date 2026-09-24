# 2026-09-24：Object_FPS_PVP 聯網 MVP

## 目的

以兩人 PvP FPS 驗證 Client → Go Gateway → ObjectFPS Adapter → C++ Headless
Match 的責任邊界；把可獨立於產品成立的固定 Tick 機制下沉 GYO。
第一輪只驗證加入、移動、地圖碰撞與互見，不擴充槍械、回合或通用遊戲協議。

## 實作與 Architecture Delta

- **GYO Runtime**：新增 `TickSettings`、`TickContext`、`FixedTickRuntime::Advance`。
  Engine 持有 accumulator、tick ID、固定步長與 bounded catch-up；不持有 clock、
  Run、sleep、thread 或產品生命週期。這修正了把固定 Tick 放到 IPC 的責任問題。
- **PvP Host／Domain**：`MatchRuntimeHost::Run` 管理程序時間與等待，將 typed 控制
  與 per-player latest Input 交給 `PvpMatch`。從現有 PvP 程式保留純 Movement／
  Collision，Match 不重用 Client `PlayerController`。抽離地圖到碰撞幾何的轉換。
- **services 頂層**：新增公共 Go HTTP、Session 與 transport/framing module。
  公共 `TCPConnection` 持有 byte framing、連線、讀寫期限與錯誤回傳；PvP
  runtime link 持有 Ready 握手、typed mailbox、codec 與錯誤的產品處置。
  Room、容量、身份映射、ObjectFPS Adapter 及 Gateway executable 都留在 PvP
  owner；公共 module 不依賴產品，不把兩人 Room 提升成所有遊戲的共同模型。
- **Build graph**：PvP native `main`／`match` roles、產品按需 Protobuf／Asio／
  cpp-httplib wrappers。Go 使用獨立 module 與 service staging；普通 C++ build
  不要求 Go，Gateway 不冒充 native release archive 中的 CMake executable。
- **Data contracts**：產品持有独立 Client v1／Runtime v1 `.proto` 和版本化
  Arena。Engine 沒有 Universal Runtime Protocol。Client 與 Authority Tick
  明確不同，Input sequence 負責排序，Snapshot cadence 只來自 Authority Tick。
- **Client**：入口改成 Lobby、60 Hz 本地輸入採樣、30 Hz latest input、權威位置
  與 100 ms 遠端插值。保留即時本地視角，不做位置預測。UI 地址編輯由 SDL
  adapter 處理，没有擴充公共 UI schema。

新增邊界是實際 Headless 與外部驅動需求所需；使用完整 Client Controller、讓
IPC 驅動時間或把 Room 塞進公共 Gateway 都會洩漏責任。v1／v2 的程式與既有
variable-frame gameplay 未遷移，已在主架構文件標明。

## 已確認的本地證據

以下只代表列出的檢查，不能代替完整建置或 LAN 驗收：

| 檢查 | 結果 |
|---|---|
| `PvpApplication.cpp`，MSVC 14.51 `/Zs /std:c++20 /permissive-` | 通過語法檢查 |
| `build/acceptance/object_fps_pvp/gui_main.cpp`，相同 compiler flags | 通過語法檢查 |
| 既有 `gyo_ui_editor --validate assets/object_fps_pvp/ui/pvp_lobby.json --asset-catalog assets/object_fps_pvp/asset_catalog.json --asset-root assets/object_fps_pvp` | 回報 `valid` |

GUI probe 使用產品公開 lifecycle 與真實 ClientConnection；兩個獨立程序各自
加入並讀取權威快照，實際 scene readback 生成 BMP。可選的 `--move` 推送 SDL
事件，經原有採樣路徑確認移動；產品沒有 probe hook 或假玩家資料。

獨立程式檢查另外發現取消 Join 與背景完成之間的 generation race：舊請求可能
在 Leave 之後重新發布 Connecting，留下已關閉 socket。已將 generation 檢查與
state publication 放入同一鎖，並限制 UDP publication 屬於目前連線 generation。
同時補上 Client 待送 Input 的持續 sequence 高水位，以及 Snapshot 的非零／
不重複 PlayerId 檢查。這些是程式檢查與修正紀錄，尚不代表競態回歸測試已通過。

## 整合驗證記錄

目前已取得的本機結果（MSVC 14.51、Release、Go 1.27.1）：

| 檢查 | 實際結果 |
|---|---|
| GYO `engine_tests` | 62 cases、984 assertions 通過，包含 FixedTickRuntime |
| PvP `object_fps_pvp.cpu` | 15 cases、766 assertions 通過，含 Leave 清除待消費 Input、重複 Join 保持 pose／sequence／freshness |
| `object_fps_pvp.wire` | UDP header、長度拒絕與 sequence wrap 通過 |
| Native Client／Match／兩種 probes | Protobuf v36.2 原始碼、C++ bindings、MSVC Release compile／link 通過 |
| Match headless | `dumpbin /DEPENDENTS` 無 SDL；隔離目錄僅放 Match executable，以外部 Arena 啟動成功 |
| Engine-only fixture | 實際無產品目錄／註冊的 source，60 cases、977 assertions 通過，無 SDL／Protobuf |
| PvP 移除 fixture | 原 PvP app／assets／tests／acceptance／註冊列均不存在；複製產品 domain／host 15 cases、766 assertions、公共 Go 8 tests、複製 Go 8 tests 通過 |
| 公共 Go module | 8 tests 通過，含 TCP 斷線／逾時／取消／Close 喚醒、malformed/repeated payload 限流且不刷新 liveness |
| PvP Go module | 8 tests 通過，含真實 HTTP／UDP／TCP fixture、雙 peer publication-only Snapshot、IPC failure 與分段 HTTP request |
| Go bindings | 使用 protoc 36.2 / protoc-gen-go 1.36.11 重生成，兩個檔案 SHA-256 完全一致 |
| Gateway executable | 獨立 build 及 `-h` 啟動通過 |
| 真實 Go／C++ 互通 | 兩個 C++ Client、41 份相同 Authority Snapshot、移動、cadence、輸入停止、Leave／新 PlayerId 重入通過 |
| 實際雙 GUI Client | 透過 Lobby 鍵盤操作 Create／Refresh／Join 後 localhost 互見；SDL W 輸入產生 1.10 單位權威位移，兩端最後記錄相同玩家位置；ESC 回 Lobby，before／world BMP 目視確認 |
| 失聯／IPC framing | 超過 64 KiB 的 IPC 長度不使 Match 崩潰；換 Gateway 後清場可重入；終止 Match 使兩個 Client 回 Lobby、清除 Snapshot，HTTP Room unavailable／players=0 |
| 未修改的 v2 | 42 cases、225,840 assertions 通過；v2 程式、資產與測試沒有 diff |
| 完整產品複製 | 僅複製產品內容／測試／acceptance 並增加暫存註冊列，Client、Match、兩種 probe 全部建置；clone 15 cases／766 assertions、wire 與 Go 8 tests 通過；clone 真實網路驗收取得 40 份共同 Snapshot |

命令與證據：

- `ctest --test-dir build/target/_build/pvp-mvp -R "^(engine_tests|object_fps_pvp.cpu|object_fps_pvp.wire)$" --output-on-failure`
- 兩個 Go module 分別執行 `GOWORK=off go test -v -timeout 30s ./...`。
- 日誌：`build/target/validation/pvp/go-shared-tests-final.log`、`go-product-tests-final.log`，
  以及 native build tree 的 `Testing/Temporary/LastTest.log`。
- Gateway：`build/target/validation/pvp/products/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway.exe`。
- 真實互通與 GUI：`build/target/validation/pvp/e2e/result.json`、`network.log`、
  `failure-probe.log`、`gui/*-report.txt`、`gui/*-before.bmp`、`gui/*-world.bmp`。
  移除暫時診斷並重編譯後的 network／lifecycle 重跑另在 `e2e-final-production/`。
- Product fitness：`build/target/validation/pvp/fitness/native-tests-detail.log`、
  `clone-e2e/result.json`、`core/test.log`、`removed/test.log`；各 fixture 的 README
  與 manifest 區分 source 物理移除、純 domain 測試與完整 clone executable 驗收。

完整 clone 驗收使用暫存 registry，沒有修改原 `projects.csv`；驗收後已還原
建置選擇並刪除本次建立的 clone app／assets／tests／acceptance、stage 與 build
目錄。原使用者 PvP 複製內容與修改保留，v1／v2 的程式及資產沒有變更。

實際互通另外揭露 Windows TCP 關閉時的問題：原 Create handler 未讀取 POST
body 就回覆，cpp-httplib 分開送 header／body 時會遭遇 WSAECONNRESET。
新增真實 TCP 分段 regression，先確認舊實作失敗，再讓 handler 完整讀取有界
JSON 後回覆。修正後端到端流程通過；未修改上游 httplib 原始碼或放寬 timeout。

尚未實測：兩台實體 Windows 機器的 LAN、Linux／macOS runtime、封裝 archive
完整發佈與 Lobby 畫面的目視檢查。localhost 互通不代替 LAN／防火牆驗證；
前述 MSVC 產物的無 SDL Match 仍需要主機具備相應 MSVC runtime。GUI 探針的 scene readback
不含 UI overlay，不能把 world 圖像當成 Lobby 畫面證據。

依賴固定為 Protobuf compiler/C++ runtime v36.2、Go protobuf/plugin v1.36.11、
Asio 1.38.2、cpp-httplib 0.56.0。生成與手動啟動方式見
[獨立網路架構文件](../object_fps_pvp/network-architecture.zh-Hant.md)。

## MinGW 建置補驗與修正

使用者依文件執行 `cmake --build build/target/_build/pvp` 時，該 cache 實際選用
MinGW GCC 15.2.0／MinGW-w64 13.0.0、RelWithDebInfo，而非先前驗證的 MSVC。
實際修正兩個相依問題：

- cpp-httplib 0.56.0 的非同步 DNS 使用 `GetAddrInfoExCancel`，這套 MinGW headers
  沒有宣告它；既有 `_WIN32_WINNT=0x0A00` 並不能解決。於 httplib wrapper
  增加 MinGW API compile/link 檢查，缺少時使用上游同步解析器。MSVC 設定保留。
  此 fallback 的 hostname DNS 查詢不受連線 timeout 約束；LAN literal IPv4
  不需 DNS 查詢。沒有修改 fetched 第三方原始碼或手工補假宣告。
- SDL_ttf 3.2.2 的 static target 只公開 SDL headers；缺少對 SDL library 的依賴，
  造成 MinGW 把 SDL import archive 放到 TTF 前面並出現 undefined SDL symbols。
  在既有 SDL_ttf wrapper 補上 private `SDL3_ttf-static → SDL3::SDL3` 依賴。
  這是既有 text adapter 的實際 implementation dependency，影響使用該 adapter
  的建置連結；沒有產品專用分支、public API 或遊戲 ownership 變更。

原 build directory 的完整預設 build 與使用者原命令重跑均通過；產出的 Client
和 Match 執行 `--help` 均 exit 0。證據為 `build/target/validation/pvp/` 下的
`mingw-build-final.log`、`mingw-build-confirm.log`。本次沒有將 MinGW build-only
結果提升為新的 GUI／雙機 LAN 驗收；先前端到端證據仍屬 MSVC 產物。
原 MSVC Client target 亦重建通過，記錄於 `msvc-wrapper-regression.log`。

## 已知範圍與後續壓力

目前最多一個固定 Room 1／Match 1、最多兩位玩家；建立前房間清單為空，
`POST /rooms` 建立房間，重試取得同一房間，不配置新的 Match process。
斷線清場並重新加入，不恢復舊世界。UDP 尚無可靠
事件、ACK 或多 channel；完整 Snapshot 與可重送握手支撐本輪持續狀態。

應由下一個真實需求決定一次性動作、Prediction、相容版本或其他 Driver 的介面。
這次沒有把未經第二案例驗證的 Player／Room／WorldSnapshot 升格成 GYO 公共協議。

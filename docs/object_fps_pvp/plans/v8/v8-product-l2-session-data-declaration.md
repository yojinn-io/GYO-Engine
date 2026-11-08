# 11/9 多人實機場次：資料收集事前宣告（v8-product-l2-session-data）

更新：2026-11-08。Owner：`object_fps_pvp`。來源：使用者 2026-11-08 的指示（架構師轉達）：場次的目的是「收集資料」，以 lag-l2 的標準做；每台一個離線 kit，直接啟動，結束後整包交回。
**狀態：草稿，kit 交付前給使用者看。** 架構師與產品一起宣告、一起讀（使用者 2026-11-08）：§2A 是每次場次都收的通用項（[field-session](../../../architecture/field-session.md) C1～C8，架構師對照檢查），§2 是產品項。有事前判定的只有 lag-l2（[宣告](v8-product-hitbox-lag-l2-declaration.md)修訂 9）與 K8（[hitbox-acceptance](v8-product-hitbox-acceptance-declaration.md) A4，使用者判斷）；其餘都是**探索**（AGENTS §14.1，不當受理判定）。

## 1. 構成與 kit

- Intel Mac：Match＋Gateway（不當玩家）。Windows 3 台：`win1`、`win2`、`win3`（2～3 人）。現場沒有網路。不加人工延遲（使用者 2026-11-08）。
- 程式：commit `24943706`（#167 pa-broadphase＋biome、#168 移動診斷）。Windows 是 master 整合建置 run `42351930949` 的 windows-x64 套件（tarball SHA-256 `32bd9c24…`，和 CI 的 `.sha256` 一致）；Mac 是同一個 commit 的本機建置（`pkg-diag/`）。
- kit：主 checkout 的 `build/evidence/v8-lag-l2-kit/kit-20261109/`，由 `build/acceptance/object_fps_pvp/lag_l2_session/make_kit.sh` 產生；每個 kit 有 `kit-manifest.txt`（commit 與每個檔案的 SHA-256）。
  - Windows：`lag-l2-kit-winN.zip`（套件、`windows.ps1`、`start.cmd`、`README.txt`）。雙擊 `start.cmd`、輸入 Mac 的 IP；結束後同一資料夾出現 `lag-l2-winN*.zip`，交回。
  - Mac：`mac/`（套件、Gateway、`mac.sh`、判定工具）。`mac.sh run` 一個終端機完成啟動與停止（`s`＝s1-end、`q`＝停止）；收到 Windows 的 zip 後 `mac.sh collect` 解壓、跑 lag-l2 判定、把整個場次打包成一個 zip。
- 退路：照 lag-l2 宣告修訂 8／7（`8b8eaa19` 的 biome kit、`54948874` 的 pvp_corners kit；兩者都要拿掉診斷開關）。收集標準不變，只是少了移動診斷。

## 2. 收什麼、在哪裡、怎麼分析

| # | 資料 | 檔案（場次資料夾內） | 格式 | 事後分析 | 判定 |
|---|---|---|---|---|---|
| 1 | lag-l2 的 view_lag、R | `mac/match-shots.jsonl`、`marks.txt`、`clients/*/client-*.log` | JSON Lines version 3；`s1_end` | `mac.sh judge`（`lag_l2_summary.py`：S1、S2、R1、R2） | **lag-l2 宣告** |
| 2 | K8（形狀、頭部受擊、斷線重連） | `k8-notes.txt` | 使用者的文字 | 使用者判斷 | **hitbox-acceptance A4** |
| 3 | 射擊判定（部位、命中、拒絕原因） | `mac/match-shots.jsonl`、Client log 的 `PvP summary` | 同上 | 部位分布、拒絕原因的計數 | 探索 |
| 4 | Match 的 tick 時間 | `mac/match.log` 的 `network statistics`（每個視窗的 tick_late、tick_overrun、advance 的分位數與最大值、tick_stalls） | 文字 log | 依時間畫出，找超過 16.67 ms 的視窗 | 探索 |
| 5 | snapshot 大小 | `mac/gateway.log` 的 `snapshot_bytes` | 文字 log | 分布與最大值（3 人、biome） | 探索 |
| 6 | 移動診斷（位置、desiredY、接觸法線、嵌入深度、supported、grounded） | `mac/match-movement-diag.jsonl`、`clients/*/client-*-movement-diag.jsonl` | `gyo.pvp.movement_diag` version 1 | pm-detect（lag-l2 之後）找卡住、空氣牆的步，固定成重現 | 探索（pm-detect 的輸入） |
| 7 | Client 幀時間 | Client log 的 `PvP summary`（每秒 fps、longest_frame_ms）、`PvP render` | 文字 log | 分布、長幀的時刻 | 探索 |
| 8 | 載入時間 | Client log 的 `graphics ready initialization_ms` 與 `connection phase=playing` 的時刻；`mac/match.log` 的 `start` 到 `Match ready` | 文字 log | 差值 | 探索 |
| 9 | 記憶體 | `mac/memory.csv`（Match、Gateway 的 RSS，每 10 秒）、`clients/*/client-memory.csv`（working set、private bytes，每 10 秒） | CSV | 隨時間的變化、最大值 | 探索 |
| 10 | 網路與時鐘 | Match／Client log 的 `clock sync`、`gw_transport`、`connection summary`、`cause records` | 文字 log | 延遲與中斷的時刻，對照 #1、#4 | 探索 |

- 探索的結果只記錄、不判定；要變成判定時，另寫事前宣告（看到資料之後寫的判定不當受理判定）。
- #4、#7、#8、#9 的預算：這次不設判定（AGENTS §2 的計劃預算由之後的 `pa-measure`、`scene v1` 宣告）。

## 2A. 通用項（field-session C1～C8）

| 項 | 這次 | 檔案 | 缺口 |
|---|---|---|---|
| C1 場次事實 | 有 | `marks.txt`（commit、Match／Gateway 的 SHA-256、arena、`mac_ip`、`s1_end`、起訖）、`mac/machine.txt`（macOS、CPU、GPU、RAM、arena、診斷旗標、人工延遲 none）、`clients/*/machine.txt`（名稱、Mac IP、commit、Client 的 SHA-256、Windows、CPU、GPU 與驅動、RAM、旗標、起訖、exit code）、各 kit 的 `kit-manifest.txt` | 參與者（誰用哪台）不記：由使用者寫進 `k8-notes.txt`（選擇性） |
| C2 時鐘 | 部分 | Match 與 Client 的每一行 log 都有牆鐘與 `mono_ns`（steady clock），統計行每 10 秒一次；Gateway 的 log 有牆鐘與時區 | Gateway 沒有 steady clock 的對照 |
| C3 伺服器 tick | 部分 | `mac/match.log` 的 `network statistics`（tick_late p50／p99／max、tick_overrun p99／max、advance p99／max、tick_stalls）、`statistics`（players） | 視窗是 10 秒不是 1 秒；分位數是 p50／p99，沒有 p95 |
| C4 wire per peer | 部分 | `mac/gateway.log` 的 `player send statistics`（每 10 秒：snapshot 與 results 的間隔、`snapshot_bytes` 四個值）、`gw_transport`、`gw_drop report`；Client 的 `worker statistics`（收、舊、壞的 datagram）、`clock sync`（delay min／max） | 每秒的 bytes in／out 沒有；RTT 只有 min／max、沒有 jitter；視窗 10 秒 |
| C5 Client 幀 | 部分 | Client log 的 `PvP summary`（每秒 fps、longest_frame_ms）、`PvP render` | 每秒的 p50／p95 沒有（只有平均 fps 與最長幀）；draw 數、三角形數沒有；預測修正的次數與最大量沒有逐秒值 |
| C6 載入與記憶體 | 部分 | Client 的 `graphics ready initialization_ms`、`connection phase=playing` 的時刻；`mac/match.log` 的 `start`→`Match ready`；`mac/memory.csv`、`clients/*/client-memory.csv`（每 10 秒）；kit-manifest 有每個檔案的大小 | 峰值 RSS 只靠 10 秒取樣，短尖峰看不到；Client 的「內容載入到 ready」沒有單獨的時間 |
| C7 故障 | 有 | Match 的 `end reason=… exit_code=…` 與 stderr（`mac/match.out`）；Client 的 exit code（`machine.txt`）與 stdout／stderr（`client-stdout.txt`、`client-stderr.txt`）；各 trace 的 `trace_end`／`diag_end`（dropped）；`leave reason`、`connection summary` | Gateway 異常結束的文字只在 `mac/gateway.out` |
| C8 kit | 部分 | §1：每台一包、同一 commit、`kit-manifest.txt`；`mac.sh collect` 把整場打成一個 zip | **Windows 的 `start.cmd`、`windows.ps1` 交出前沒有在 Windows 上跑過**（這台 Mac 沒有 PowerShell）；Mac 的命令都跑過 |

缺口是之後批次的候選（例如 Match 與 Client 每秒的統計、bytes in／out），這次不補（不改產品程式）。

## 3. 日誌量（先估）

依 D3（150 秒、3 個 bot）實測換算成 3 人 × 1 小時：

| 檔案 | 1 小時 |
|---|---|
| Match 的 movement trace（`match-commands.jsonl`） | 約 630 MB |
| Match 的移動診斷 | 約 340 MB（一行約 500 B，180 行／秒） |
| 每台 Client 的 movement trace | 約 500 MB |
| 每台 Client 的移動診斷 | 約 110 MB |
| 其他 log、shots、CSV | 數 MB |

- Mac 與每台 Windows 要留 2 GB 以上的空間。zip 會壓得很小（JSON Lines），交回的檔案量以壓縮後為準。
- 寫入對 tick 的影響：pm-detect-diag 的 D4（試行 2 通過，但量測波動大過閾值，證據偏弱）。場次中用 #4 的 tick_overrun 與 tick_stalls 觀察；出現時照記錄，不中止場次（lag-l2 宣告：場次只跑一次）。

## 3A. 這次執行檔裡的收集碼（field-session DI 3）

field-session 的 DI 3 規定，侵入式的收集碼要放在發行建置不編的地方。這次來不及改成建置選項（使用者接受），以下都**編進了發行建置**，只用執行期旗標開關（旗標不算分開）。之後要拆到測試變體時，照這張表找：

| 收集 | 旗標 | 位置 |
|---|---|---|
| 移動診斷（#168） | `--movement-diag` | `apps/object_fps_pvp/include/RetroFPS/Pvp/MovementDiag.hpp`；`StepMovement` 的 `MovementStepDiagnostics*` 參數（`Movement.hpp`、`Movement.cpp`）；`MoveCharacterBody` 的 `CharacterMoveDiagnostics*`（`CharacterCollision.hpp`、`.cpp`）；`PvpMatch.cpp` 與 `LocalPlayerPrediction.cpp` 的記錄點（`PredictStep`）；`match_main.cpp`、`main.cpp` 的旗標 |
| 影子紀錄 version 3（view_lag，#154） | `--movement-trace`（同資料夾的 `-shots.jsonl`） | `apps/object_fps_pvp/include/RetroFPS/Pvp/MatchShotTrace.hpp` 與它在 Match 的寫入點 |
| movement trace、ingress trace | `--movement-trace`、`--ingress-trace` | `MovementTrace.hpp`、`MovementTraceWriter.hpp`、`MatchIngressTrace.hpp`（v5～v7 以來的既有診斷） |
| 統計行、wall grid 的 log | 無（一直寫） | `match_main.cpp` 等的 `std::clog` 行；`wall grid` 一行（#167） |

kit 的腳本（`mac.sh`、`windows.ps1`、`start.cmd`、`make_kit.sh`）不在執行檔裡。

## 4. 範圍的限界

- `windows.ps1`（含新加的記憶體取樣）與 `start.cmd` **沒有在 Windows 上跑過**（這台 Mac 沒有 PowerShell）。建議今天（11/8，有網路）在一台 Windows 上先跑一次 `start.cmd`，連到 Mac 的 `mac.sh run`。
- Mac 端的 `mac.sh run`、`collect` 在 Intel Mac 上用 127.0.0.1 與 3 個無 GUI 的 bot 跑過（主 checkout 的 `build/evidence/v8-lag-l2-kit/dryrun-run-*`）。
- 記憶體每 10 秒一次，短的尖峰看不到。

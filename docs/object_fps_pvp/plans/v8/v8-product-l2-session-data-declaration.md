# 11/9 多人實機場次：資料收集事前宣告（v8-product-l2-session-data）

更新：2026-11-08。Owner：`object_fps_pvp`。來源：使用者 2026-11-08 的指示（架構師轉達）：場次的目的是「收集資料」，以 lag-l2 的標準做；每台一個離線 kit，直接啟動，結束後整包交回。
**狀態：草稿，kit 交付前給使用者看。** 有事前判定的只有 lag-l2（[宣告](v8-product-hitbox-lag-l2-declaration.md)修訂 9）與 K8（[hitbox-acceptance](v8-product-hitbox-acceptance-declaration.md) A4，使用者判斷）；其餘都是**探索**（AGENTS §14.1，不當受理判定）。

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

## 4. 範圍的限界

- `windows.ps1`（含新加的記憶體取樣）與 `start.cmd` **沒有在 Windows 上跑過**（這台 Mac 沒有 PowerShell）。建議今天（11/8，有網路）在一台 Windows 上先跑一次 `start.cmd`，連到 Mac 的 `mac.sh run`。
- Mac 端的 `mac.sh run`、`collect` 在 Intel Mac 上用 127.0.0.1 與 3 個無 GUI 的 bot 跑過（主 checkout 的 `build/evidence/v8-lag-l2-kit/dryrun-run-*`）。
- 記憶體每 10 秒一次，短的尖峰看不到。

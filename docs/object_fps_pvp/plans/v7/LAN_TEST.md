# PvP v7 LAN 聯機測試手冊

更新：2026-10-15（補上第 16 批 L2 的結果）。Owner：`object_fps_pvp`。來源：lan-kit-v7（[HANDOFF](HANDOFF.md) 的 D81）。比照 [v6 的手冊](../v6/LAN_TEST.md)。
構成：Match 與 Gateway 在 Mac（Intel）上；Client 是 Windows（朋友），Mac 也可以當第 3 位玩家。同一個區域網路。地圖用發行地圖 `pvp_corners_v1`（四角出生）。

## 測試前請先看：第 16 批 L2 的結果（2026-10-15 補充）

- 這個測試包的產品程式（`6d3f287`）就是第 16 批 L2 量測的版本。
- L2 依宣告**沒有通過**：GUI 戰鬥短測（v7 上第一次跑）判 HUD 的 HP／彈藥和「HUD 標示的 Tick」不一致。唯讀診斷的結論是 v6 分析器的假設在 v7 第 04 批之後不成立：HUD 顯示的是最新的權威值（正確），只是標示的 Tick 落後 1～2 個；玩家看不出差別，人工試玩不受影響（`build/target/_build/test/logs/pvp-v7-16-l2-20261015/hud-diagnosis.md`）。修法：只改分析器（v7 規則，D83），以補充量測 16-L2b 重跑後通過，**16-L2 整體通過**（2026-10-15 補充）。
- 其他項目都通過：25 案矩陣、背壓、網路、長測短模式、4 人 quad、1 GUI＋3 bot、GUI 動作與移動短測。
- 偶發的移動重設（D44）：260 次回歸 0 次（每個母體的 95% 上限 2.5%～18.1%）。實際網路下仍可能出現，遇到請記下時間。

## 0. 快速開始（複製貼上）

Mac 已準備好測試包 `~/gyo-lan-v7/`（不用下載）。先查 Mac 的 IP，下面的 `MAC_IP` 都換成它：

```bash
ipconfig getifaddr en0
```

Mac 終端機 1（Match；等到出現 `Match ready` 再開終端機 2）：

```bash
~/gyo-lan-v7/gyo-object_fps_pvp/bin/gyo_object_fps_pvp-match --arena ~/gyo-lan-v7/gyo-object_fps_pvp/bin/assets/object_fps_pvp/pvp_corners.json --listen 127.0.0.1:27016 --log ~/gyo-lan-v7/match.log --movement-trace ~/gyo-lan-v7/match-commands.jsonl
```

Mac 終端機 2（Gateway）：

```bash
~/gyo-lan-v7/gyo_object_fps_pvp-gateway -runtime 127.0.0.1:27016 -http 0.0.0.0:8080 -udp 0.0.0.0:27015 -advertise-ip MAC_IP -log ~/gyo-lan-v7/gateway.log
```

Mac 終端機 3（選用：Mac 也當玩家）：

```bash
cd ~/gyo-lan-v7/gyo-object_fps_pvp/bin && ./gyo_object_fps_pvp --gateway MAC_IP:8080 --movement-trace logs/client-commands.jsonl
```

Windows（每台）：從 snapshot `object_fps_pvp-snapshot-20261014-6d3f287` 下載 `gyo-object_fps_pvp-windows-x64.tar.gz`，`tar -xzf` 解壓後在命令提示字元執行：

```bat
cd gyo-object_fps_pvp\bin
gyo_object_fps_pvp.exe --gateway MAC_IP:8080 --movement-trace logs\client-commands.jsonl
```

注意：trace（`.jsonl`）每次啟動都從頭覆寫。中途重啟某個程式時，把它的 trace 檔名改成 `-2`、`-3` …（見第 3 節）。**v7.1 D191 起**不再覆寫：檔案已存在時自動改用 `client.2-commands.jsonl` 這類編號名，實際檔名寫在 log 裡，不必手動改名。
結束：Client 按 Esc 離開並關閉視窗 → Mac 的 Gateway 按 Ctrl+C → Match 按 Ctrl+C。
上傳：Mac 的 `~/gyo-lan-v7/` 中的 `match.log`、`match-commands.jsonl`、`match-ingress.jsonl`、`gateway.log`（Mac 當玩家時加上 `gyo-object_fps_pvp/bin/logs/`），以及每台 Windows 的 `bin\logs\` 整個資料夾。

## 1. 準備同一個版本

全部的程式都來自 master `6d3f287`（`6d3f287889289feac50a1c85d1ed4787afa0a4cc`，v7 的最終來源）。

- **Windows（朋友）**：從 GitHub 的 snapshot `object_fps_pvp-snapshot-20261014-6d3f287`（<https://github.com/yojinn-io/GYO-Engine/releases/tag/object_fps_pvp-snapshot-20261014-6d3f287>）下載 `gyo-object_fps_pvp-windows-x64.tar.gz`，在命令提示字元解壓：

  ```bat
  tar -xzf gyo-object_fps_pvp-windows-x64.tar.gz
  ```

  - 可以核對下載的檔案：`certutil -hashfile gyo-object_fps_pvp-windows-x64.tar.gz SHA256` 應該是 `f0e06e2862833ba2b8ed168d56fac4c9c5614f762e3442a5b5aa9abcfce965fe`（GitHub 記錄的值）。
  - 第一次執行時若出現 SmartScreen（未簽章的程式），按「其他資訊」→「仍要執行」。
- **Mac**：測試包已在 `~/gyo-lan-v7/`，從 `6d3f287` 在本機建置（打包方式和 CI 的 `macos-x64` 相同，不用下載）。內容與 SHA-256 見第 8 節。
  - `gyo-object_fps_pvp/`：Match、Mac 版 Client、資產、`lib/`（Client 用的 SDL）、`build_metadata.json`。
  - `gyo_object_fps_pvp-gateway`：Gateway（發行包沒有 macOS 版的 Gateway，和 v6 一樣在本機建置）。
- **核對版本**：
  - 每個發行包的 `gyo-object_fps_pvp/build_metadata.json` 中，`source_revision` 都要是 `6d3f287889289feac50a1c85d1ed4787afa0a4cc`（Windows 可以用記事本打開）。
  - 啟動後，每個角色的日誌第一行記錄自己執行檔的 SHA-256。分析時以這個值核對。
  - wire 仍是 pv6（v7 不改 wire），但 v6 和 v7 的程式不要混用：四台都要用上面的版本。

## 2. 網路

1. 查 Mac 的區域網路 IP（Wi-Fi 通常是 `en0`；有線可能是 `en1` 等，查不到時看「系統設定」→「網路」）：

   ```bash
   ipconfig getifaddr en0
   ```

2. 以下用 `MAC_IP` 代表這個位址。**Mac 的網段改變（重新連 Wi-Fi 等）時，IP 可能改變**：v6 測試時發生過，要用新的 `-advertise-ip` 重新啟動 Gateway（Match 不用重啟）。
3. Mac 的防火牆：第一次啟動 Gateway 時若詢問，允許接受連入連線。需要的連接埠是 TCP 8080（大廳）與 UDP 27015（遊戲）。Match 只聽 `127.0.0.1:27016`，不需要開放。
4. Windows：第一次啟動 Client 時若出現防火牆詢問，允許「私人網路」。
5. 確認連得到：在 Windows 的瀏覽器打開 `http://MAC_IP:8080/rooms`，看得到 `{"rooms":[...]}` 就可以。

## 3. 啟動

### Mac（兩個終端機，第 3 個選用）

終端機 1：Match。出現 `Object_FPS_PVP Match ready: 127.0.0.1:27016 arena=pvp_corners_v1 authority=60Hz` 之後，再啟動 Gateway：

```bash
~/gyo-lan-v7/gyo-object_fps_pvp/bin/gyo_object_fps_pvp-match --arena ~/gyo-lan-v7/gyo-object_fps_pvp/bin/assets/object_fps_pvp/pvp_corners.json --listen 127.0.0.1:27016 --log ~/gyo-lan-v7/match.log --movement-trace ~/gyo-lan-v7/match-commands.jsonl
```

- v7 新增：指定 `--movement-trace` 時，Match 會在同一個資料夾另外寫 `match-ingress.jsonl`（命令到達 Match 的細節：被替代的序號、晚到多久、被拒的原因）。檔名由 `-commands.jsonl` 換成 `-ingress.jsonl` 自動決定，不需要另外指定。
- `--log` 是附加寫入；`--movement-trace` 與 `match-ingress.jsonl` 每次啟動都**從頭覆寫**。中途重新啟動 Match 時，先把這兩個 `.jsonl` 改名（例如加 `-1`），否則前一段的 trace 會不見。**v7.1 D191 起**不再覆寫：已存在時自動改用 `match.2-commands.jsonl`、`match.2-ingress.jsonl`、`match.2-shots.jsonl`，實際檔名寫在 `match.log` 的 `trace files` 行。

終端機 2：Gateway。把 `MAC_IP` 換成實際的位址：

```bash
~/gyo-lan-v7/gyo_object_fps_pvp-gateway -runtime 127.0.0.1:27016 -http 0.0.0.0:8080 -udp 0.0.0.0:27015 -advertise-ip MAC_IP -log ~/gyo-lan-v7/gateway.log
```

- 啟動後會印出 `Object FPS Gateway HTTP=... UDP=... advertise=MAC_IP runtime=127.0.0.1:27016`。`advertise` 必須是 Mac 的 LAN IP，不能是 `127.0.0.1`，否則 Windows 連不上遊戲（大廳看得到，但進不了對局）。
- Gateway 沒有其他日誌選項：v7 的統計行（見第 6 節）都自動寫進 `-log` 的檔案。

終端機 3（選用）：Mac 也當玩家。和 Windows 一樣用 `MAC_IP`：

```bash
cd ~/gyo-lan-v7/gyo-object_fps_pvp/bin && ./gyo_object_fps_pvp --gateway MAC_IP:8080 --movement-trace logs/client-commands.jsonl
```

- 日誌自動寫到 `~/gyo-lan-v7/gyo-object_fps_pvp/bin/logs/client-<時間>.log`。重新啟動時和 Windows 一樣，把 trace 的檔名改成 `-2`、`-3` …（trace 每次從頭覆寫）。
- 注意：Mac 同時跑 Match、Gateway 與 Client（畫面），Match 的 Tick 可能受影響。Match 的 10 秒統計行會記錄晚醒，分析時可以看出有沒有影響。只有 2 位 Windows 玩家時才建議 Mac 加入。

### Windows（每台）

在解壓出來的 `gyo-object_fps_pvp\bin` 資料夾中執行，把 `MAC_IP` 換成實際的位址：

```bat
cd gyo-object_fps_pvp\bin
gyo_object_fps_pvp.exe --gateway MAC_IP:8080 --movement-trace logs\client-commands.jsonl
```

- **不要忘了 `--gateway`**：v6 測試時有一台沒加，連到預設的 `127.0.0.1` 而逾時。
- 日誌會自動寫到 `bin\logs\client-<時間>.log`（每次啟動一個新檔）。
- **`client-commands.jsonl` 每次啟動都從頭覆寫**。Client 中途重新啟動時，把命令中的檔名改成 `logs\client-commands-2.jsonl`、`-3` …，前一段的 trace 才會留下。
- 第一個人在大廳建立房間，其他人選房間加入；房間最多 4 人。
- Client 會依 Match 的地圖自動選 `pvp_corners_v1`。若出現 `arena_identity_mismatch` 或 `arena_content_mismatch`，表示 Client 和 Match 不是同一個版本。
- v7 沒有 FPS 上限的選項（移到 v8），FPS 跟著螢幕更新率。

## 4. 測試中

- 每一段建議 10～15 分鐘，段與段之間可以不關程式。發生異常（卡頓、瞬移或被拉回、斷線、射擊沒反應、`CONNECTION POOR`）時，記下大概的時間（看電腦右下角或右上角的時鐘，到秒更好）、是誰的畫面、當時在做什麼。
- v7 想確認的事（v7 把畫面、模擬、網路分成三個執行緒，命令改在固定步產生，網路改為事件驅動）：
  1. **連續射擊中切換視窗或桌面**：Windows 用 Alt+Tab、Win+D、Win+Ctrl+←／→（虛擬桌面），也可以拖動視窗、縮小再還原。v6 時這類操作可能讓主迴圈停住；v7 的模擬與網路不跟著畫面停，預期不斷線、回來後不被拉回。每次切換記下時間與做了什麼。
  2. **連點射擊**：盡量快地連點，看有沒有「點了沒射」或「射了被拒絕」的感覺。日誌記錄本機射擊閘擋下的點擊與伺服器的接受、拒絕次數。
  3. **FPS 低的玩家**：若有人的電腦只有 30 FPS 左右（或筆電省電模式），請記下是誰。v7 的主要改善之一是 30 FPS 的命令時序（v6 的 D21）；日誌的 `fps=` 可以確認。
  4. **Wi-Fi 與有線**：可以的話，記下每台是 Wi-Fi 還是有線；有人移動到訊號差的地方時也記下時間。
- 結束順序：
  1. 每個 Client 按 Esc 離開，再關閉視窗。
  2. Mac 上先在 Gateway 終端機按 Ctrl+C，再在 Match 終端機按 Ctrl+C。
  3. 不要強制結束程式（不要關掉終端機視窗、不要用工作管理員結束），日誌與 trace 才會完整寫完。Match 在 Ctrl+C 後會寫出最後一個統計視窗（`final=1`）和 `match-ingress.jsonl` 的結尾。

## 5. 要上傳的檔案

| 機器 | 檔案 |
|---|---|
| Mac | `~/gyo-lan-v7/` 中的 `match.log`、`match-commands.jsonl`、`match-ingress.jsonl`、`gateway.log`；Mac 當玩家時加上 `~/gyo-lan-v7/gyo-object_fps_pvp/bin/logs/` 整個資料夾；終端機的畫面輸出（若有錯誤訊息） |
| 每台 Windows | `bin\logs\` 整個資料夾（`client-*.log`、`client-commands.jsonl`） |
| 文字說明 | 每台機器的名稱、CPU、GPU、螢幕更新率、有線或 Wi-Fi；異常的時間與現象；切換視窗的時間；各機器的時區是否相同 |

## 6. 日誌的內容（分析用）

v6 的內容都保留（[v6 手冊](../v6/LAN_TEST.md)第 6 節）。下面標「v7」的是新增的部分。

- **時間**：每行有本機牆鐘時間（毫秒或微秒）。各機器的時鐘沒有對齊（跨機器對時在 v7.1 的 pv7），跨機器只能以牆鐘大致對照。
- **Client**（`client-*.log`）：
  - 啟動：執行檔 SHA-256、平台、GPU 驅動、Gateway 位址；連線階段、選到的地圖、錯誤與被移出的原因、`CONNECTION POOR`。
  - 每秒 `PvP summary`：FPS、最長幀、每秒收到的 Snapshot 與年齡、待確認命令、HP 與生命、送出的動作、接受、拒絕、命中、本機射擊閘擋下的點擊、未決的動作。
  - v7：`PvP render`（第一幀或超過 250 ms 的幀：幀間隔與準備、UI、繪製各花多久；切換視窗時的停頓會出現在這裡）、`PvP releasing pointer`（失去滑鼠鎖定）。
  - v7：每 10 秒 `[ObjectFPS/PvP] worker statistics`：網路 worker 的喚醒次數與 CPU 時間。
  - `client-commands.jsonl`：每個命令的產生與送出（movement trace）。
- **Gateway**（`gateway.log`）：
  - 啟動：SHA-256、Go 版本、時區與參數；事件：加入、Hello（UDP 位址）、啟用、離開、加入被拒、逐出與原因、過期。
  - 每 10 秒：`gateway statistics`、`gateway transport`、每位玩家的 `player statistics`（階段、UDP 位址、收到的封包數、最後收到距今）。
  - v7：`player send statistics`：送到 Client 的結果與 snapshot、送到 Match 的動作批次，各自的筆數與間隔 P50／P90／P99／最大值。
  - v7（08c）：`gateway ingress statistics` 與每位玩家的 `player ingress statistics`：收到的每個資料包的結局（轉送，或依原因計數的丟棄：限流、解碼失敗、舊序號、非 active 等）。
- **Match**（`match.log`）：
  - 啟動：SHA-256 與地圖；事件：加入、離開、死亡（攻擊者）、重生（位置）、逐出（延遲、替代比例、重設次數）；每 10 秒的 `statistics tick=`。
  - v7：每 10 秒 `[ObjectFPS/PvP Match] network statistics`：程序 CPU、IPC 迴圈次數、Tick 的晚醒分布。
  - v7（08c）：每 10 秒、每位玩家 `[ObjectFPS/PvP Match] ingress statistics`：每個命令的結局（新命令、重送的副本、晚到、依原因的拒絕、換代時丟掉的命令），結束時有 `final=1` 的一行。
  - `match-commands.jsonl`：每個命令在 Match 的收到、執行，以及重設與停頓（movement trace）。
  - v7（08c）：`match-ingress.jsonl`：被替代的序號（替代時刻、第一份到達、晚到多久、之後的副本數）與被拒命令的細節。
- 分析工具（之後由 Claude 執行）：`build/acceptance/object_fps_pvp/network_statistics.py`（三種日誌的 10 秒統計）等。

## 7. 已知限制

- **Windows 實機**：v7 只在 CI 的 Windows 上建置與測試，沒有在 Windows 實機上驗證過（Windows 的 LAN 場次與視窗切換原本是第 15 批，移到 v7.1）。這次的測試就是第一次。
- **偶發的移動重設（D44）**：偶爾整個移動佇列重設，畫面上看起來像短暫被拉回。v7 的整體回歸（第 16 批 L2）260 次 0 次，但實際網路下仍可能出現（補充 2026-10-15）；原文：v7 的整體回歸（第 16 批 L2）和本測試準備同時進行，結果還不知道；遇到時記下時間。
- **相位追蹤（[09b](09b-phase-tracking.md) 1-7 節）**：
  1. 網路延遲消失後，相位修正撤回時少撤 3.3 ms，約 4 秒後才修回（方向安全，不會造成重設）。
  2. 這約 4 秒內，壓著冷卻邊界的連點可能被伺服器以冷卻未到拒絕（約 8% 的那種點擊）。
  3. 延遲消失得晚時只部分撤回；150 ms 級的延遲消失時可能重設。
  4. 70 ms 以上的延遲消失時，修正可能連續發生兩次。
  5. 300 ms 級的延遲突然消失時照舊重設。
  6. 修正期間，送出的輸入封包每秒最多 82 個。
- **本機射擊閘的殘餘（[09](09-firegate-c2.md)）**：常數維持 v6 的值（2 ms、7 Tick）。網路延遲突然下降、主機或 Match 停頓 2 ms 以上、上面的修正期間，壓著冷卻邊界的點擊可能被伺服器拒絕；實際網路上的頻率未知，日誌的 `rejected=` 會記錄。修法在 v7.1。
- **日誌**：拒絕的原因在 Client 端沒有記錄（Match 的 ingress 統計有命令的拒絕原因，射擊的拒絕原因要等 v7.1 的日誌補強）；各機器的時鐘沒有對齊。
- **移到之後的版本**：跨機器對時與 pv7、日誌補強（v7.1）；FPS 上限、解析度選擇、音效（v8）；遠端角色外觀相同、受擊方向只指向最後一位攻擊者（不變）。
- Client 關閉時可能最多卡約 3 秒（已知，關閉後等它結束即可）。

## 8. 測試包的內容（Mac，`~/gyo-lan-v7/`）

2026-10-14 由 lan-kit-v7 準備。證據：`build/target/_build/test/logs/pvp-v7-lan-kit-20261014/`（git 忽略，只在本機；`summary.md`、`commands.txt`、`kit-SHA256SUMS.txt`）。全部檔案的 SHA-256 在 `~/gyo-lan-v7/SHA256SUMS.txt`。

| 檔案 | SHA-256 | 說明 |
|---|---|---|
| `gyo-object_fps_pvp/bin/gyo_object_fps_pvp-match` | `553b8e0d49383b954e8379147435a270e5b35d02391e07e870982f9d529a66a0` | Match |
| `gyo-object_fps_pvp/bin/gyo_object_fps_pvp` | `296c834b21b601f4dcb136449f9abc4693387c001322482ef07015c95645f3bf` | Mac 版 Client（x86_64） |
| `gyo-object_fps_pvp/lib/libSDL3.0.dylib` | `217a672a156ed87a9c1ecbae0b875cc5ceaf4f174dfd7dd7f51e8295e4e10a90` | Client 用的 SDL（隨包附帶，`@executable_path/../lib`） |
| `gyo_object_fps_pvp-gateway` | `a7aecefcb1a81a30a5bc18e35cedf3a0460ca336ae737277f9580a536154161d` | Gateway（Go 1.27.1，darwin/amd64） |
| `gyo-object_fps_pvp-macos-x64.tar.gz` | `71e2867defb299ba2ecbb9c964d34eea7ade16341bf57b1a40e3edab8345478f` | 上面 Match、Client、資產的壓縮包（已解壓在旁邊） |

- 建置：從 `../GYO-Engine-v7final`（`6d3f287`）的原始碼，用 CI 的 `ci-macos-x64` 設定（x86_64、macOS 13.3、`GYO_ENABLE_PACKAGING=ON`）在 repository 外的建置目錄建置，再照 CI 的步驟安裝、檢查連結（`validate_package.py`）、跑安裝後的檢查（quick）、打包（`archive_package.py`）。`build_metadata.json` 的 `source_revision` 是 `6d3f287889289feac50a1c85d1ed4787afa0a4cc`。
- 和 16-L1 的 v7final 是同一個程式：
  - Gateway 的 SHA-256 和 v7final 完全相同（同一個命令從同一個目錄建置）。
  - Match 與 Client 的 SHA-256 不同（建置目錄的路徑不同），以 16-L1 的正規化比對：符號 0 差異、反組譯 0 差異（Match 533,612 行、Client 1,025,177 行）；資產 31 檔完全相同。
- 這個 Mac 版 Client 和 GitHub snapshot 的 `macos-x64` 包不是逐位元組相同的檔案（各自建置），來源相同。
- 冒煙測試（2026-10-14 23:52～23:55，`MAC_IP`＝`127.0.0.1`，本節的命令）：`Match ready`、`/rooms` 回應、兩個無頭 Client 進房連射 8 秒（每人 210 個裁決，`passed`＝true）、Mac 版 Client 用 Metal 啟動並看到房間；依序 Ctrl+C（SIGINT）結束，Gateway 與 Match 都 exit 0，`match-commands.jsonl` 與 `match-ingress.jsonl` 都有 `trace_end`，兩邊都寫出 `final=1` 的 ingress 統計。冒煙測試的日誌已移到證據資料夾，`~/gyo-lan-v7/` 中沒有留下。

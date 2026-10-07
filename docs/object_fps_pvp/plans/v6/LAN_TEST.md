# PvP LAN 聯機測試手冊

更新：2026-10-08。Owner：`object_fps_pvp`。來源：[第 17 批](17-lan-test-prep.md)。
構成：Match 與 Gateway 在 Mac 上，Client 是 3 台 Windows，同一個區域網路。地圖用發行地圖 `pvp_corners_v1`（四角出生）。

## 1. 準備同一個版本

四台機器要用同一個 commit 的產物。第 17 批合併到 master 後，CI 會發佈 snapshot `object_fps_pvp-snapshot-<日期>-<sha7>`。

- **Windows（3 台）**：下載 `gyo-object_fps_pvp-windows-x64.tar.gz`，在命令提示字元解壓：

  ```bat
  tar -xzf gyo-object_fps_pvp-windows-x64.tar.gz
  ```

- **Mac 的 Match**：同一個 snapshot 的 `gyo-object_fps_pvp-macos-x64.tar.gz`（Intel Mac；Apple Silicon 用 `macos-arm64`）。
- **Mac 的 Gateway**：發行包不含 macOS 版的 Gateway，在 repository 中 checkout 同一個 commit 後建置：

  ```bash
  cd apps/object_fps_pvp && GOWORK=off go build -o ~/gyo-lan/gyo_object_fps_pvp-gateway ./gateway/cmd
  ```

- **核對版本**：每個發行包的 `gyo-object_fps_pvp/build_metadata.json` 中，`source_revision` 都要相同。啟動後，每個角色的日誌第一行也會記錄自己執行檔的 SHA-256。

## 2. 網路

1. 查 Mac 的區域網路 IP（Wi-Fi 通常是 `en0`）：

   ```bash
   ipconfig getifaddr en0
   ```

2. 以下用 `MAC_IP` 代表這個位址。
3. Mac 的防火牆：第一次啟動 Gateway 時若詢問，允許接受連入連線。需要的連接埠是 TCP 8080（大廳）與 UDP 27015（遊戲）。
4. Windows：第一次啟動 Client 時若出現防火牆詢問，允許「私人網路」。
5. 確認連得到：在 Windows 的瀏覽器打開 `http://MAC_IP:8080/rooms`，看得到 `{"rooms":[...]}` 就可以。

## 3. 啟動

### Mac（兩個終端機）

先建立放日誌的資料夾，並解壓 Match：

```bash
mkdir -p ~/gyo-lan && tar -xzf gyo-object_fps_pvp-macos-x64.tar.gz -C ~/gyo-lan
```

終端機 1：Match。出現 `Match ready` 之後，再啟動 Gateway：

```bash
~/gyo-lan/gyo-object_fps_pvp/bin/gyo_object_fps_pvp-match --arena ~/gyo-lan/gyo-object_fps_pvp/bin/assets/object_fps_pvp/pvp_corners.json --listen 127.0.0.1:27016 --log ~/gyo-lan/match.log --movement-trace ~/gyo-lan/match-commands.jsonl
```

終端機 2：Gateway。把 `MAC_IP` 換成實際的位址：

```bash
~/gyo-lan/gyo_object_fps_pvp-gateway -runtime 127.0.0.1:27016 -http 0.0.0.0:8080 -udp 0.0.0.0:27015 -advertise-ip MAC_IP -log ~/gyo-lan/gateway.log
```

### Windows（每台）

在解壓出來的 `gyo-object_fps_pvp\bin` 資料夾中執行，把 `MAC_IP` 換成實際的位址：

```bat
cd gyo-object_fps_pvp\bin
gyo_object_fps_pvp.exe --gateway MAC_IP:8080 --movement-trace logs\client-commands.jsonl
```

- 日誌會自動寫到 `bin\logs\client-<時間>.log`。
- 第一個人在大廳建立房間，其他人選房間加入；房間最多 4 人。
- Client 會依 Match 的地圖自動選 `pvp_corners_v1`。若出現 `arena_identity_mismatch` 或 `arena_content_mismatch`，表示 Client 和 Match 不是同一個版本。

## 4. 測試中

- 每一段建議 10～15 分鐘。發生異常（卡頓、瞬移、斷線、射擊沒反應）時，記下大概的時間（看電腦右下角的時鐘）和是誰的畫面。
- 想重現 v6 L3 的觀察時，可以在連續射擊中切換視窗或桌面，回來後看看有沒有斷線。
- 結束順序：
  1. 每個 Client 按 Esc 離開，再關閉視窗。
  2. Mac 上先在 Gateway 終端機按 Ctrl+C，再在 Match 終端機按 Ctrl+C。
  3. 不要強制結束程式，日誌與 trace 才會完整寫完。

## 5. 要上傳的檔案

| 機器 | 檔案 |
|---|---|
| Mac | `~/gyo-lan/match.log`、`~/gyo-lan/gateway.log`、`~/gyo-lan/match-commands.jsonl`，以及兩個終端機的畫面輸出（若有錯誤訊息） |
| 每台 Windows | `bin\logs\` 整個資料夾（`client-*.log`、`client-commands.jsonl`） |
| 文字說明 | 每台機器的名稱、GPU、螢幕更新率，有線或 Wi-Fi；異常的時間與現象；Mac 與 Windows 的時區是否相同 |

## 6. 日誌的內容（分析用）

- **時間**：每行有本機牆鐘時間（毫秒、時區）與單調時鐘 `mono_ns`。各機器的時鐘沒有對齊，跨機器只能以牆鐘大致對照。`mono_ns` 與同一台機器的 movement trace 用同一個時鐘。
- **Client**：
  - 啟動：執行檔 SHA-256、平台、GPU 驅動、Gateway 位址。
  - 連線：階段（lobby、requesting、connecting、playing）與選到的地圖。
  - 錯誤與被移出的原因、`CONNECTION POOR` 的失敗窗口數。
  - 對局中每秒一行摘要：FPS、最長幀、每秒收到的 Snapshot 與最新 Snapshot 的年齡、待確認的命令、HP 與生命、送出的動作、接受、拒絕、命中、本機射擊閘擋下的點擊。
- **Gateway**：
  - 啟動：SHA-256、Go 版本、時區。
  - 事件：加入（client 位址）、Hello（UDP 位址）、啟用、離開、加入被拒、逐出與原因、過期。
  - 每 10 秒：每位玩家的階段、UDP 位址、收到的封包數、最後收到距今的毫秒數。
- **Match**：
  - 啟動：SHA-256 與地圖。
  - 事件：加入、離開、死亡（攻擊者）、重生（位置）、逐出（延遲、替代比例、重設次數）。
  - 每 10 秒：Tick、玩家數、IPC 合併次數。
  - movement trace：每個命令的產生、送出、Match 收到、執行，以及重設與停頓。

## 7. 已知限制

- 遠端角色外觀相同；受擊方向只指向最後一位攻擊者。
- 視窗被遮住或切換桌面時可能斷線（v6 L3 的觀察，v7 處理）。
- 30 FPS 以下的相位餘裕不足（D21，v7 處理）。

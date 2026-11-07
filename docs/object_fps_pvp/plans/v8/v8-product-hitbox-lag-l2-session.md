# lag-l2＋K8 的 L2 場次手冊

更新：2026-11-07（宣告修訂 8：biome、2～3 人）。Owner：`object_fps_pvp`。判定以[事前宣告](v8-product-hitbox-lag-l2-declaration.md)為準；這份只寫怎麼做，兩者不一致時以宣告為準。K8 的內容見 [hitbox-acceptance 宣告](v8-product-hitbox-acceptance-declaration.md) A4。

構成（使用者 2026-11-05 決定，2026-11-07 改）：Intel Mac 只跑 Match＋Gateway，**不當玩家**；Windows 2～3 台（`win1`、`win2`、`win3`）。地圖是 biome（`ARENA_FILE=biome.json`）；pa-broadphase 在 11/8 晚上前沒合併時，照修訂 7 用 `pvp_corners`（不設 `ARENA_FILE`）與 `54948874` 的 kit。**現場沒有網路**，所以下面「事前」的東西都要先放好。

## 0. 順序

1. 事前準備（有網路時做）。
2. 啟動 Match、Gateway；兩台 Windows 加入同一個房間。
3. **lag-l2**：至少 10 分鐘連續對戰（建議 12 分鐘）。
4. 停止射擊與換彈至少 3 秒 → Mac 上 `s1-end`。
5. **K8 試玩**：形狀、頭部受擊動作、各一次斷線與重連。
6. 結束：每人停止射擊 3 秒 → Esc 離開 → 關視窗 → Gateway Ctrl+C → Match Ctrl+C。
7. 收 log、在 Mac 上跑判定。

## 1. 事前準備（有網路時）

- 程式全部來自同一個 commit：本批 PR（#155）合併到 master 後，master 的整合建置（`cross-platform` workflow）上傳的套件 artifacts（PR 的 L1 不產生套件，2026-11-05 查過）。run 編號與 commit 開場前記到 HANDOFF。Gateway 也從那個 commit 建置；`mac.sh`、`lag_l2_summary.py` 用那個 commit 的 checkout。
- 套件的 zip 裡是一個 `gyo-object_fps_pvp-<平台>.tar.gz`，解開後是 `gyo-object_fps_pvp/` 資料夾（裡面有 `build_metadata.json`、`bin/`、`lib/`）。**`PKG` 指的是這個 `gyo-object_fps_pvp` 資料夾**，不是解壓的上一層。
- **Windows（每台）**：從 run `42031502557` 下載 `gyo-package-object_fps_pvp-windows-x64`（artifact 保留到 2026-11-19），解開 zip 後在 PowerShell 用 `tar -xzf gyo-object_fps_pvp-windows-x64.tar.gz` 解開（檔案總管打不開 `.tar.gz`），放在本機磁碟的新資料夾。把 repo 的 `build/acceptance/object_fps_pvp/lag_l2_session/windows.ps1` 也複製過去。
- **場次的程式（修訂 8，2026-11-07）**：pa-broadphase 的 PR 合併後的 master commit 與它的整合建置 run（合併後記到 HANDOFF 與這裡）；Mac 的 `PKG`、`GATEWAY` 從同一個 commit 在本機建置，放在主 checkout 的 `build/evidence/v8-hitbox-lag-l2/pkg-biome/`。以下是修訂 7 的退路：
- **退路的程式（2026-11-05 定）**：commit `549488745d0221efc4cc651b0da0dd855bed3db9`（#155 的合併），master 整合建置 run `42031502557`。
- **Mac（已準備好）**：CI 沒有 macos-x64 套件，Match 與 Gateway 都是這台 Intel Mac 從同一個 commit 的本機建置（宣告修訂 7）：
  - `PKG`＝`/Users/karasu/Code/Source/GYO-Engine/build/evidence/v8-hitbox-lag-l2/pkg/gyo-object_fps_pvp`
  - `GATEWAY`＝`/Users/karasu/Code/Source/GYO-Engine/build/evidence/v8-hitbox-lag-l2/gateway`
  - SHA-256 記在同一資料夾的 `session_binaries_sha256.txt`；`prepare` 也會寫進 `marks.txt`。本機建置沒有隔離標記，不必 `xattr`。

  - 腳本用 repo 裡的 `build/acceptance/object_fps_pvp/lag_l2_session/mac.sh`（下面寫成 `mac.sh`）。
- **預演（有網路、在場次要用的同一台路由器上）**：Mac 跑一次 `prepare`（用一個試用的資料夾）、`match`、`gateway`；兩台 Windows 各跑一次 `windows.ps1`，確認印出 `source_revision`、連得到 Gateway、進得了 Lobby，然後關掉。
  - Mac 第一次啟動 Gateway 時，防火牆若問「允許傳入連線」，選允許。Mac 自己的 `check` 走 loopback，擋不擋防火牆都會通過，所以一定要從 Windows 確認。
  - Windows 上沒有網際網路的區域網路常被歸為「公用網路」：在「設定」→「網路」確認網路設定檔，防火牆詢問時允許對應的那一種（或兩種都允許）。

## 2. 啟動

Mac 終端機 1（準備；`SESSION` 用一個新的資料夾，例如 `.../v8-hitbox-lag-l2/session-20261106`）：

```bash
ARENA_FILE=biome.json mac.sh prepare SESSION PKG GATEWAY
```

它會寫出 `marks.txt` 的開頭（commit、SHA-256、arena 的位置），並列出這台 Mac 的位址。選和 Windows 同一個網段的那個，以下稱 `MAC_IP`。

Mac 終端機 1（Match；出現 ready 後再開終端機 2）：

```bash
ARENA_FILE=biome.json mac.sh match SESSION PKG
```

Mac 終端機 2（Gateway）：

```bash
mac.sh gateway SESSION GATEWAY MAC_IP
```

Mac 終端機 3（確認）：`mac.sh check MAC_IP`，看到 `{"rooms":[...]}` 就可以。

Windows（每台，在 PowerShell 裡）：

```powershell
powershell -ExecutionPolicy Bypass -File windows.ps1 -MacIp MAC_IP -Name win1 -Package PKG
```

- 它先印出 `source_revision`，再確認連得到 Gateway。連不到時什麼都不啟動，照它印的訊息檢查防火牆與網段。
- 第一次執行時，Windows 防火牆若詢問，允許目前網路設定檔的那一種（見預演）；SmartScreen 按「其他資訊」→「仍要執行」。
- 不要在 Client 結束前關掉這個 PowerShell 視窗：腳本要等 Client 結束才打包。若不小心關了，log 還在腳本旁邊的 `lag-l2-<名稱>-<時刻>` 資料夾，直接拷那個資料夾。
- 第一個人建立房間，另一個人加入。
- **規則**：最後一位玩家加入之前遇到環境問題可以重來，但要換新的 `SESSION`；之後不重啟任何程式。

## 3. lag-l2：10 分鐘以上的連續對戰

- 照常遊戲，**一邊移動一邊互射**：跑、跳、蹲、換彈、死亡與重生都可以。目標是兩人各至少 100 發，總共至少 500 發（每人每分鐘約 25 發以上就夠）。
- **不要**刻意斷線、按 Esc 或關視窗。
- 至少 10 分鐘（建議 12 分鐘）。結束時：兩人都**停止射擊與換彈至少 3 秒**，然後在 Mac 終端機 3 執行：

  ```bash
  mac.sh s1-end SESSION
  ```

- 中途有人掉線：照實記下，不要重來（宣告：場次只跑一次）。

## 4. K8 試玩（記錄，由你判斷）

在 `s1-end` 之後做，順序不拘。把感想寫在 `SESSION/k8-notes.txt`：

- 形狀的大小：頭、軀幹、腿打起來是否合理（打不中時記下來，不在這批改半徑）。
- 頭部受擊的動作看起來是否自然。
- 斷線、重連各一次後，部位判定是否照常：在一台 Windows 上拔網路線或關 Wi-Fi，**一直斷著，直到 Client 回到 Lobby、顯示 `Gateway connection timed out`**（約 5 秒以上），才接回，從 Lobby 重新加入同一個房間；再玩一兩分鐘確認。
  - 這樣舊的玩家 id 會被 Gateway 判為逾時，不進 S2 的對帳；重新加入的是新的 id，正常離開時照常對帳。不滿 5 秒就接回時會以同一個 id 續上，那段時間的動作會進對帳，這種情況沒有在實機驗證過。

## 5. 結束與收 log

1. 每人停止射擊與換彈至少 3 秒 → Esc 離開 → 關視窗。Windows 的腳本會把 log 打包成 `lag-l2-win1.zip`（在腳本旁邊）。
2. Mac：Gateway 終端機 Ctrl+C，接著 Match 終端機 Ctrl+C。**只用 Ctrl+C，不要直接關終端機視窗**：關視窗送的是 SIGHUP，Match 不會寫出 `trace_end`，整個場次的影子紀錄就不能用。
3. 用隨身碟把兩個 zip 拷到 Mac，用命令解壓（Finder 的雙擊會多一層資料夾，判定工具讀不到）：

   ```bash
   unzip lag-l2-win1.zip -d SESSION/clients/win1
   unzip lag-l2-win2.zip -d SESSION/clients/win2
   unzip lag-l2-win3.zip -d SESSION/clients/win3   # 3 人時
   ```

4. Mac 上：

   ```bash
   mac.sh judge SESSION
   ```

   輸出寫在 `SESSION/lag_l2_summary.json`。結果請交給我判讀與記錄；不論結果如何都不重跑（宣告）。

## 6. 乾跑紀錄與限界

- Mac 的每條命令（`prepare`、`match`、`gateway`、`check`、`s1-end`、`judge`）都在 Intel Mac 上用 127.0.0.1 實際跑過（2026-11-05，證據在主 checkout 的 `build/evidence/v8-hitbox-lag-l2/dryrun/`）。Match 寫出的 `match-shots.jsonl` 是 version 3；沒有玩家時 `judge` 回報 S1、S2、R1 為 undetermined。乾跑用的套件是照發行包的結構（`gyo-object_fps_pvp/` 下有 `build_metadata.json`、`bin/`）用本機建置組出來的，不是 CI 的產物；隔離標記與真的防火牆只有預演會碰到。
- `windows.ps1` 沒有在這台 Mac 跑過（沒有 PowerShell）。場次開始時先在一台 Windows 上執行一次，確認它印出 `source_revision` 並連得到 Gateway；不行就停下，不進入對戰。

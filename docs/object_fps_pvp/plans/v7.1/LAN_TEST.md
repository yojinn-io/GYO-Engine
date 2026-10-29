# PvP v7.1 LAN 場次手冊（lan15-windows）

更新：2026-10-29。Owner：`object_fps_pvp`。規則：[lan15-declaration.md](lan15-declaration.md)（第 2.1 版，D236、D237）。這份手冊只寫怎麼做；判定以宣告為準，兩者不一致時以宣告為準。比照 [v7 的手冊](../v7/LAN_TEST.md)。

構成：Mac（Intel）只跑 Match＋Gateway，**不當玩家**；Client 只在 Windows 上，N 台。人數 N 是 2 或 3（`n_w` 與 `n` 相同），當天看情況，**S1 開始前決定，之後不改**。地圖 `pvp_corners_v1`。

## 0. 這次要做的事（順序固定）

1. **S1**：全員加入後連續對戰至少 15 分鐘，不做刻意的干擾。
2. **S2**：只對 Windows 做斷線：每台 2 秒一次、7 秒一次。
3. **S3**：Q1 一台 Windows 直接關閉視窗後重新加入；Q2 Mac 用 quad probe 補人，試第 5 人加入。
4. **結束**：停止射擊 3 秒 → Esc → 關視窗 → Gateway Ctrl+C → Match Ctrl+C。
5. **全程**：看新的人物模型與第一人稱的 nailer 有沒有異常（S7）。

需要準備：一個手錶或手機（看秒），Mac 上的文字編輯器（寫 `marks.txt`）。

## 1. 準備程式（都來自同一個 commit）

- 產品的 commit：`646b808ffe9084889ed91ba4b9cbc12d3ff771d5`（發行線，含新模型的合併）。四平台 L1 是 [run 40793575034](https://github.com/yojinn-io/GYO-Engine/actions/runs/40793575034)。
- **Windows（每台）**：從那次 run 的 Artifacts 下載 `gyo-package-object_fps_pvp-windows-x64`（要登入 GitHub；朋友的機器可以由使用者下載後用隨身碟或雲端轉交）。解壓到**本機磁碟的新資料夾**（不要用網路共用）。
  - 用記事本打開 `gyo-object_fps_pvp\build_metadata.json`，`source_revision` 要是上面的 commit。
  - 第一次執行時若出現 SmartScreen，按「其他資訊」→「仍要執行」。
- **Mac**：
  - Match 與 Client：下載同一次 run 的 `gyo-package-object_fps_pvp-macos-x64`，解壓到 `~/gyo-lan15/pkg/`，同樣核對 `source_revision`。
  - Gateway：發行包沒有 macOS 版的 Gateway，在本機從同一個 commit 建置（Gateway 的原始碼在 `646b808f` 之後沒有改過）：

    ```bash
    cd /Users/karasu/Code/Source/GYO-Engine/.claude/worktrees/outage-resync/apps/object_fps_pvp && /usr/local/go/bin/go build -o ~/gyo-lan15/gyo_object_fps_pvp-gateway ./gateway/cmd
    ```

  - quad probe（Q2 用）：本機的建置 `build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_quad_probe`（同一個工作樹，產品原始碼與 `646b808f` 相同）。
  - 記下 Gateway 與 quad probe 的 SHA-256（`shasum -a 256 <檔案>`），寫進 `marks.txt` 的註解（`#` 開頭）。

## 2. 網路

1. 查 Mac 的區域網路 IP：

   ```bash
   ipconfig getifaddr en0
   ```

   以下用 `MAC_IP` 代表這個位址。**場次中不要讓 Mac 換網路**（Mac 是主機，Mac 斷線等於全員斷線；這次不做 Mac 的斷線，D235）。
2. Mac 的防火牆：第一次啟動 Gateway 時若詢問，允許接受連入連線（TCP 8080、UDP 27015）。
3. Windows：第一次啟動 Client 時若出現防火牆詢問，允許「私人網路」。
4. 確認連得到：在 Windows 的瀏覽器打開 `http://MAC_IP:8080/rooms`，看得到 `{"rooms":[...]}` 就可以。

## 3. 場次資料夾與 marks.txt

在 Mac 上開一個**新的空資料夾**，例如：

```bash
mkdir -p ~/gyo-lan15/session-20261029/mac ~/gyo-lan15/session-20261029/clients ~/gyo-lan15/session-20261029/quad
```

以下用 `SESSION` 代表這個資料夾。在 `SESSION/marks.txt` 一行寫一項（時刻用 `2026-10-29T20:15:00` 的格式，到秒）。範本：

```text
# gateway sha256=...  quad probe sha256=...
n=3
n_w=3
s1_end=2026-10-29T20:31:00
trial=win1,2s,2026-10-29T20:32:10
trial=win1,7s,2026-10-29T20:33:05
trial=win2,2s,2026-10-29T20:34:20
trial=win2,7s,2026-10-29T20:35:15
trial=win3,2s,2026-10-29T20:36:30
trial=win3,7s,2026-10-29T20:37:25
q1=win1
q2_start=2026-10-29T20:40:00
q2_end=2026-10-29T20:40:40
```

- `n`、`n_w` 在 S1 開始前寫，之後不改。
- `s1_end`、`q2_start`、`q2_end` 是 **Mac 的時刻**；`trial` 的時刻是**那台 Windows 的時刻**（看 ping 視窗最後一行的時間，見第 6 節）。
- `win1`、`win2` 是每台 Windows 的名稱，之後收 log 時用同一個名字當資料夾名。

## 4. 啟動

- **規則**：每個程式從頭到尾只啟動一次。**最後一位玩家加入之前**遇到環境問題（例如防火牆擋住）可以重來，但要換一個新的 `SESSION` 資料夾、全部重啟；之後不准重啟任何程式（宣告第 1 節）。

Mac 終端機 1（Match；出現 `ready` 之後再開終端機 2）：

```bash
~/gyo-lan15/pkg/gyo-object_fps_pvp/bin/gyo_object_fps_pvp-match --arena ~/gyo-lan15/pkg/gyo-object_fps_pvp/bin/assets/object_fps_pvp/pvp_corners.json --listen 127.0.0.1:27016 --log SESSION/mac/match.log --movement-trace SESSION/mac/match-commands.jsonl
```

Mac 終端機 2（Gateway；把 `MAC_IP` 換成實際的位址）：

```bash
~/gyo-lan15/gyo_object_fps_pvp-gateway -runtime 127.0.0.1:27016 -http 0.0.0.0:8080 -udp 0.0.0.0:27015 -advertise-ip MAC_IP -log SESSION/mac/gateway.log
```

- 啟動行的 `advertise` 必須是 `MAC_IP`，不能是 `127.0.0.1`。

Windows（每台，在解壓出來的 `gyo-object_fps_pvp\bin` 裡）：

```bat
gyo_object_fps_pvp.exe --gateway MAC_IP:8080 --movement-trace logs\client-commands.jsonl
```

- 不要忘了 `--gateway`。
- 第一個人建立房間，其他人加入。全員進入遊戲後，Mac 上記下：S1 從「最後一位玩家加入」開始算（工具從 Match 的 log 取，不用手記）。

## 5. S1：15 分鐘以上的對戰

- 照常遊戲：移動、射擊、換彈、死亡與重生、連點。**不要**刻意拔線、切網路、按 Esc 或關視窗。
- 至少 15 分鐘（建議 17 分鐘以上，因為工具從手記的結束時刻往前扣 30 秒）。
- 結束時在 Mac 上記下 `s1_end=`（Mac 的時刻），之後**再照常玩 60 秒、不做任何特別的事**，才開始 S2。
- 中途有人自己斷線或掉線：照實記下，不要重來（宣告：S1 只做一次）。

## 6. S2：Windows 的斷線（每台依序）

1. 在那台 Windows 開一個 PowerShell，執行下面這行（把 `MAC_IP` 換掉），一直開著到這台的 S2 做完：

   ```powershell
   while($true){ "$(Get-Date -Format o) $(Test-Connection MAC_IP -Count 1 -Quiet)"; Start-Sleep -Milliseconds 200 } | Tee-Object ping.txt
   ```

   每 0.2 秒一行，`True` 是通、`False` 是斷。
   - 開始前先確認一直是 `True`。一直是 `False` 的話，多半是 Mac 的防火牆開了「隱身模式」（不回應 ping）：在 Mac 的「系統設定」→「網路」→「防火牆」→「選項」關掉隱身模式，或改用有回應的位址；確認不了就不做這台的 S2，照實記下。
2. **2 秒**：看 ping 視窗最後一行的時間，寫進 `marks.txt`：`trial=win1,2s,<那個時間>`。拔網路線或關 Wi-Fi，數 2 秒，接回。
   - 預期：遊戲不回 Lobby，短暫卡住後繼續。
3. 等**至少 45 秒**（Match 的連線品質要恢復，否則可能被逐出）。
4. **7 秒**：同樣先記 `trial=win1,7s,<時間>`，斷線，數 7 秒，接回。
   - 預期：大約 5 秒後回到 Lobby，顯示 `Gateway connection timed out`。接回後**從 Lobby 重新加入**同一個房間。
5. 等**至少 45 秒**，再換下一台 Windows。
6. 做完後停掉 PowerShell（Ctrl+C），`ping.txt` 留在那台的使用者資料夾（`Tee-Object` 寫在 PowerShell 的目前資料夾）。
- 斷線的實際長度由 `ping.txt` 判斷：2 秒那次要在 1.5～4.0 秒、7 秒那次要 ≥6.0 秒，不在範圍內的那次記「確認不了」（不算失敗），**不要重做**。

## 7. S3

- **Q1（關閉視窗後重新加入）**：在 `marks.txt` 寫 `q1=win1`（選一台 Windows）。那台**直接按視窗的 X 關閉**（不要按 Esc），再用同一個指令啟動 Client，從 Lobby 加入同一個房間。其他人照常玩。
- 等**至少 45 秒**。
- **Q2（房間已滿）**：在 Mac 上記 `q2_start=`，在 Mac 終端機 4 執行（`<4−N>` 換成 4 減人數：3 人時是 1，2 人時是 2）：

  ```bash
  /Users/karasu/Code/Source/GYO-Engine/.claude/worktrees/outage-resync/build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_quad_probe --gateway MAC_IP:8080 --arena ~/gyo-lan15/pkg/gyo-object_fps_pvp/bin/assets/object_fps_pvp/pvp_corners.json --create false --clients <4−N> --expect-players 4 --fifth true --passive true --duration 20 --output SESSION/quad/out 2>&1 | tee SESSION/quad/probe.log
  ```

  - 機器人會加入房間補到 4 人，接著試第 5 人，預期被拒（`room_full`）。約 20 秒後機器人自己離開。
  - probe 結束後記 `q2_end=`。房間裡的人照常玩，不要離開。

## 8. 結束

1. 每位玩家**先停止射擊與換彈至少 3 秒**，再按 Esc 離開，然後關閉視窗。
2. Mac：Gateway 終端機按 Ctrl+C，再到 Match 終端機按 Ctrl+C。
3. 觀察紀錄（S7）：每個人說一下人物模型（動畫、法線、頭髮、缺面、閃爍）與第一人稱 nailer 看起來有沒有異常，寫在 `SESSION/notes.txt`。

## 9. 收集

把檔案整理成判定工具讀的形狀（宣告第 1 節）：

- `SESSION/mac/`：`match.log`、`gateway.log`、`match-commands.jsonl`、`match-ingress.jsonl`、`match-shots.jsonl`（Match 若自動改名成 `match.2-*.jsonl`，照實際檔名放；正常情況只有一組）。
- `SESSION/clients/<名稱>/`：那台 Windows 的 `bin\logs\` 裡所有 `client-*.log`、`client-commands*.jsonl`，加上 `ping.txt`。
- `SESSION/quad/`：已經在第 7 節寫好。
- `SESSION/marks.txt`、`SESSION/notes.txt`。

## 10. 判定（v7.1 會話做）

先核對判定工具沒有改過（sha256 要等於宣告第 7 節鎖定的值），再執行：

```bash
cd /Users/karasu/Code/Source/GYO-Engine/.claude/worktrees/outage-resync && shasum -a 256 build/acceptance/object_fps_pvp/lan15_judge.py build/acceptance/object_fps_pvp/ingress_evidence.py && python3 -I -B build/acceptance/object_fps_pvp/lan15_judge.py ~/gyo-lan15/session-20261029 --output ~/gyo-lan15/session-20261029/judge.json
```

工具判不了的（L3 的觀察、S7 的外觀）以使用者的紀錄判定。

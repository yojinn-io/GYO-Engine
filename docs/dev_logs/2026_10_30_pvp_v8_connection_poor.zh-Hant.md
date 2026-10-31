<!--
來源（2026-10-31 搬進 repo）：作者是 audio-line 會話（local_56d3dc93）開的 connection-poor 會話（local_0b06b609）。
原本的位置：Claude app 的會話筆記資料夾 `~/Library/Application Support/Claude/side-session-notes/local_56d3dc93-c1b7-4466-ab55-43b9abb25f6c/connection-poor-report.zh-Hant.md`，不在 git。
下面的報告本文照原文搬入，沒有改寫；文中的 `connection-poor-evidence/` 與 scratchpad 路徑指的是原本的位置。
證據資料夾 `connection-poor-evidence/`（約 54 MB、39 個檔）的原始 log 只在使用者的本機，不進 repo、不保證保存；要不要另外保存由使用者決定。
三支腳本與判讀用的數字附在文末「附錄」。
-->

# 「CONNECTION POOR」調查報告（object_fps_pvp，2026-10-30）

收支：一致（注入 4 次、Match 記到 4 次停頓；3 次 ≥100 ms 的停頓各產生 1 次 Backlog 重設、1 筆窗口失敗；60 ms 的那次 0 次重設）
範圍：只調查，沒有改程式、沒有 commit、沒有推送。用時約 1.5 小時（大半是 master 的完整建置）。

## 結論（一句話）

用戶端的音效不是成因。成因是 **Match 的 Tick 停頓超過約 100 ms**。停頓時 Engine 的追趕上限（5 步）會丟掉時間，Client 的命令序號因此多領先 2～4 個，Backlog 保險絲在 0.5 秒內換 epoch。品質窗口的規則是「有 1 次移動重設就判失敗」，所以 HUD 顯示 10 秒的警告。在**不含音效的 origin/master** 上，用無頭機器人＋SIGSTOP 停 Match 140 ms，**每次都重現**，失敗行和 ingress 計數與使用者那次逐項一致。這是文件裡已知的例外（network-architecture §7「P2 的第二個例外」），已排給 v8 B6（`v8-product-lead-withdraw`）。

還沒回答的是：**那次 136 ms 的整機停頓是不是因為音效變得比較常發生**。這要在 master 上用 GUI 實際玩才能判斷（見最後「待使用者決定」）。

## 1. 成因鏈

| 步驟 | 機制 | 證據 |
|---|---|---|
| ① 整機卡頓 | 20:40:04～09 用戶端、Gateway、Match 同時變慢：GPU 等 drawable 99.9 ms（04.423）、65.4 ms（06.222）；最長幀 66.5／49.6／39.4 ms；快照率掉到 51.8/s（08.843）；時鐘同步的 `delay_max_us=123500`（Client↔Gateway，13.646 那一窗）。04.845 有 `releasing pointer`（游標被釋放；可能切換了視窗，未確認） | 用戶端日誌 `pvp-quality-window-log-831143/build/target/object_fps_pvp/bin/logs/client-20261030-203938.log` |
| ② Match 的 Tick 晚醒 136 ms | `tick_late_max_us=136013 tick_stalls=1`（使用者終端機裡的輸出，沒有存檔） | `NetworkStatistics.cpp:71`（間隔 > 1.5 倍就算 1 次 stall） |
| ③ 追趕上限丟掉時間 | `FixedTickRuntime` 一次 Advance 最多補 5 步（83 ms），多的整步丟掉。Client 照自己的時鐘一直出命令，Match 的游標因此落後，連續待執行的命令從穩態的 2 變成 4～6 | `engine/runtime/include/engine/runtime/FixedTickRuntime.hpp:14,52,61`；`MatchRuntimeHost.hpp:248`（`ticker_{AuthorityTickRate}` 用預設的 5 步）。重現時的 `runtime_gap`：停 140 ms→丟 0.0667 s（4 Tick），停 100 ms→丟 0.0333 s（2 Tick） |
| ④ Backlog 保險絲換 epoch | 最近 30 Tick 的待執行數總和 ≥105（平均 3.5）就換 epoch。相位追蹤在 30 Tick 內收不掉多出的領先量 | `PvpMatch.cpp:525`（`backlog`）、`Movement.hpp:58-60`；重現時 trace 的 `"kind":"reset","reset_reason":"backlog"` 共 3 筆 |
| ⑤ 品質窗口判失敗 | `resets > 0` 就失敗；Backlog、Starvation、Resync 都計入 resets | `MatchRuntimeHost.cpp:722`、`PvpMatch.cpp:442` |
| ⑥ HUD 顯示 10 秒 | Snapshot 帶連續失敗數，≥1 就顯示「CONNECTION POOR - improve within 20s…」，下一個窗口合格就回到 0 | `PvpApplication.cpp:1156-1160` |

「移動時出現、射擊後消失」只是 10 秒窗口的時間點，和射擊無關。Backlog 不看有沒有在移動：命令每個 Tick 都會送。

這條鏈在文件裡已有記載：`docs/object_fps_pvp/network-architecture.zh-Hant.md:954`「P2 的第二個例外：Match 自己停頓…多出的領先量留到 Backlog 保險絲清掉…交給 v8 B6」。但那裡沒有寫到**它也會讓品質窗口判失敗**。

## 2. 「1 次移動重設就判失敗」是不是設計值

是。2026-10-02 核准（v5 fix/10，PR #11），之後一直沿用：

- `docs/object_fps_pvp/plans/v5/fix/10-connection-quality-eviction.md:55`：Starvation／Backlog 移動重設「≥1次」。依據寫的是「正常情境每局15分鐘0–數次；崩潰區約每秒一次」。第 57 行：「一次性卡頓、單次重設不會觸發」（指的是**移出**不會觸發，不是警告不會出現）。
- `docs/object_fps_pvp/protocol-v5.zh-Hant.md:39`（契約表「連線品質移出」）。
- `apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp:98-112`（註解與常數）。
- v7.1 把 Resync 也計入（`network-architecture.zh-Hant.md:623`，D172 的 D-10）。

所以這次的行為符合契約。單次停頓最多讓連續失敗數到 1，**不會被移出**（要連續 3 窗）。

## 3. 在不含音效的 master 上能不能重現

建置：`origin/master` 5cfa149c（#109）；與音效分支的分叉點 771ba6c2 之間，`apps/object_fps_pvp` 沒有差異。master 本來就沒有 `GYO::Audio`。Match 的 `executable_sha256=98faf15c…`。機器人用 `quad_probe`（走產品的 `ClientSimulationRole` 出命令，1 個 bot，60 FPS，左右平移並射擊）。

| 試行 | 內容 | 結果 |
|---|---|---|
| A（120 s，無注入） | 基準 | 窗口失敗 0、重設 0、`tick_stalls` 全部 0、最大晚醒 539 µs |
| B（160 s） | 30 s 停 60 ms | 晚醒 57.7 ms，**重設 0**，替代 2（`substituted_failed=0`） |
| | 60 s 停 140 ms | 晚醒 140.6 ms、`tick_stalls=1`、**Backlog 重設 1**、窗口失敗 |
| | 90 s 停 140 ms | 晚醒 138.0 ms、`tick_stalls=1`、**Backlog 重設 1**、窗口失敗 |
| | 120 s 停 100 ms | 晚醒 101.2 ms、**Backlog 重設 1**、窗口失敗 |

和使用者那次逐項比對（停 140 ms 的兩次都一樣）：

| 項目 | 使用者（音效分支、GUI） | master 重現 |
|---|---|---|
| `movement_resets` / `substituted_commands` | 1 / 3 | 1 / 3 |
| `tick_late_max_us` / `tick_stalls` | 136013 / 1 | 140596、137951 / 1 |
| ingress `late_first` / `late_copy` / `rotation_discarded` / `slack_late_samples` | 3 / 21 / 7 / 3 | 3 / 21 / 7 / 3 |

另一份旁證：音效分支 20:49 那次兩個 GUI 用戶端的 Match 日誌（`…/logs/match-20261030-204911.log`），停頓 47 ms 與 61 ms 都沒有重設、也沒有窗口失敗。和 B 的 60 ms 一致：**界線在約 85～100 ms**，也就是追趕上限開始丟時間的地方。

範圍的限制：
- SIGSTOP 會停住整個 Match 行程（IPC 執行緒也一起停）。使用者那次是整機卡頓。兩者走的路徑不同，但計數的指紋相同。
- master 的 GUI 用戶端沒有實際玩過，所以「音效是否讓整機卡頓變多」**判定不了**。到目前為止的樣本：使用者那次約 50 秒 1 次 136 ms；20:49 那次約 2 分鐘最大 61 ms；master 的無頭機器人 120 秒 0 次。樣本太少，不能比較頻率。

證據：`connection-poor-evidence/`（和本報告放在同一個資料夾）：`runA/`、`runB/`（match.log、gateway.log、movement.jsonl、ingress.jsonl、injections.log、probe.json），以及注入腳本 `stall_run.py`。

## 4. 選項（由使用者決定）

| 選項 | 做法 | 好處 | 代價／風險 | 證據 |
|---|---|---|---|---|
| a. 不改 | 保持現狀；把這個現象補進文件的已知例外 | 不動契約；單次停頓本來就不會移出 | 主機一卡（≥約 100 ms）玩家就看到 10 秒的「快被移出」警告，但問題不在玩家這邊。Windows／Linux 上多常發生還沒量過 | §1、§3、network-architecture:954 |
| b. 從根本收掉：Match 停頓後不留下多出的領先 | 照原計畫做 v8 B6（`v8-product-lead-withdraw`）；或者範圍更小：在產品端把 Match 的追趕上限 5 步加大（例如 10 步 ≈ 167 ms），停頓 ≤ 該值時不丟時間，也就不會 Backlog | 不改品質判定；B 的 60 ms（沒丟時間）顯示「不丟時間 → 不重設」 | 追趕上限：一次補更多 Tick，恢復時的 CPU 尖峰比較大，Snapshot 一次跳更多；超過新上限時照樣會發生。這個值是產品自己的常數，但會改權威 Tick 的時間行為，算 Architecture Delta，要先確認。B6 已排在 v8 且規模大（16～24 h，xhigh） | `FixedTickRuntime.hpp:14`、`MatchRuntimeHost.hpp:248`；v8 README 的 B6 列 |
| c. 放寬判定 | 例如：同一窗口裡，Match 自己丟過時間（`runtime_gap` 的 `dropped_seconds>0`）之後 1 秒內的 Backlog 不計入；或改成重設 ≥2 次才失敗 | 警告只針對玩家自己的問題 | 改 v5 的契約（protocol-v5 §、Movement.hpp），要走 AGENTS §14.4 的放寬流程（1～5 項、突變檢查、獨立審查、使用者核准）。「≥2 次」也會放過真的不穩定的連線；用成因排除比較準，但判定會多一條「是不是 Match 自己的錯」的分支 | `MatchRuntimeHost.cpp:722`、`PvpMatch.cpp:442` |
| d. 只改 HUD | 判定不變，用戶端在連續失敗 ≥2 時才顯示警告 | 不動 wire 與 Match | 真的不好的連線，警告延後 10 秒才出現（移出前只剩 10 秒）；只遮掉症狀 | `PvpApplication.cpp:1156` |
| e. 先量再決定 | 在 master 上用 GUI 玩 2～3 分鐘（Match 加 `--log`），和音效分支對照停頓的頻率 | 回答「跟音效有沒有關係」 | 要使用者玩；一次的樣本只能看出很明顯的差異 | 本報告 §3 的範圍限制 |

建議：先做 e，判斷音效有沒有讓整機卡頓變多。成因鏈和音效無關，而單次停頓不會造成移出，所以不急著改。之後如果要改，我傾向 b（B6 或加大追趕上限），因為它處理的是「Match 停頓後留下多餘的領先」這個已知例外本身；c 和 d 只是把結果藏起來。

## 5. 選項 e：master 的 GUI 對照（2026-10-30 21:20 準備好）

### 怎麼跑

一支腳本負責整場：啟動 master 的 Match（`--log`＋`--movement-trace`）與 Gateway（`-log`），再開 GUI 用戶端（`--log`）。關掉用戶端視窗後，腳本會停掉 Match 和 Gateway，並印出彙整。

- 腳本：`connection-poor-evidence/run_gui_master.sh`。scratchpad 裡有可以直接跑的同一份：`/private/tmp/claude-501/-Users-karasu-Code-Source-GYO-Engine--claude-worktrees-hopeful-wescoff-d692d8/1b113c72-4aa6-4a33-bbc2-435359f6d7fe/scratchpad/run_gui_master.sh`（它旁邊有 master 版的 `gateway` 執行檔）。
- 執行前的檢查：8080、27016 被佔用就停止；Match 執行檔不是 master 版（sha256 前 8 碼 `98faf15c`）也停止。
- log 放在：`hopeful-wescoff-d692d8/build/target/object_fps_pvp/bin/logs/master-gui-<時刻>/`
- 玩法：和 20:39 那次一樣。Create＋Join，走動 2～3 分鐘，可以射擊；之後關掉視窗。
- 彙整：`summarize_stalls.py`。只做計數，不做判定：每窗口最大晚醒、≥85 ms／≥50 ms 的窗口數、品質窗口失敗、重設原因、用戶端 ≥50 ms 的幀與 slow swapchain 的次數。

腳本的每一條命令都實際跑過一次：用環境變數 `TEST_CLIENT` 把 GUI 換成無頭機器人，跑 25 秒；啟動、收 log、收尾（沒有殘留行程、埠都釋放）、彙整都正常。證據在 `connection-poor-evidence/script-test-headless/`。GUI 用戶端本身只確認過 `--help`。

### 對照用的既有數字（同一個彙整腳本，音效分支）

| 場次 | Match 窗口 | 最大晚醒 | ≥85 ms 的窗口 | 窗口失敗 | 用戶端 ≥50 ms 的秒數／slow swapchain |
|---|---|---|---|---|---|
| 20:39（1 個 GUI，約 45 s） | （Match 沒存檔；使用者終端機裡是 136 ms） | 136 ms | 1 | 1 | 2／2（100.7 ms @20:40:04） |
| 20:49（2 個 GUI，約 2 分鐘） | 14 | 61 ms | 0 | 0 | 0／0（最大 48.2 ms） |

### 測腳本時順帶發現的事

在 master 上用無頭機器人跑 25 秒（沒有 GUI、沒有音效），Match 也自己晚醒了一次 86 ms（`frame_seconds=0.1027`，丟掉 1 Tick；沒有重設，因為只多領先 1 個）。也就是說，**這台 Intel Mac 本來就會偶爾卡 80～140 ms，跟音效無關**。至於音效會不會讓卡頓更常發生或更長，要等 GUI 對照的結果。一次對照只看得出很明顯的差異（例如 master 在 2～3 分鐘裡完全沒有 ≥50 ms 的卡頓，而音效分支每次都有）。

### 第 1 次 GUI 對照的結果（master，21:23:30）

- 實際玩了 **31 秒**（21:23:35 join → 21:24:06 leave）。計畫是 2～3 分鐘，所以樣本不夠。
- Match：3 個統計窗口，最大晚醒 54.4 ms，落在第一個窗口（包含啟動和 join，品質判定不看這個窗口）；之後的窗口是 0.2 ms 和 1.0 ms。沒有 ≥85 ms 的停頓，沒有重設，窗口失敗 0。
- 用戶端：最長的幀 18.7 ms，沒有 slow swapchain，`max_gap_ms=60`。
- 判讀：master 這 31 秒很乾淨。但音效分支 20:49 那次跑了 2 分鐘，最大也只有 61 ms，所以「音效讓卡頓變多」**仍判定不了**。證據在 `connection-poor-evidence/master-gui-20261030-212330/`。

## 決定（2026-10-30，使用者）

「到此為止」：選項 a（不改）。把這個現象當成已知例外，交給 v8 B6（`v8-product-lead-withdraw`）。音效和卡頓頻率有沒有關係，沒有再量。
待辦（給維護 `docs/object_fps_pvp/plans/v8/` 的會話，本會話沒有改那裡）：B6 那一列「Match 停頓後恢復時多出的領先量」要補一句：它也會讓品質窗口判失敗（停頓超過約 100 ms → Backlog → HUD 顯示 10 秒警告），證據指向本報告。

## 未驗證（交給使用者實機測）

| 項目 | 為什麼沒驗證 | 實機上要看什麼 |
|---|---|---|
| 音效（`GYO::Audio`／SDL 後端）會不會讓整機卡頓（≥85 ms）變多或變長 | 本機只有 3 份短樣本：音效分支 45 秒裡有 1 次 136 ms；音效分支 2 分鐘最大 61 ms；master GUI 31 秒最大 54 ms（啟動窗口），master 無頭 25 秒有 1 次 86 ms。樣本太少，比不出頻率 | Match 加 `--log`，看 `tick_late_max_us`（≥85000 就會 Backlog）與 `quality window failed`；用戶端看 `longest_frame_ms`、`slow swapchain acquire`、`sound summary` 的 `late`。要對照就各玩幾分鐘，比 ≥85 ms 窗口的次數 |
| 主戰場（Windows Client＋Linux Match）上會不會發生 | 只在 Intel Mac 上觀察過 | 同上。Linux Match 的 `tick_stalls` 和 `tick_late_max_us` 如果都在 85 ms 以下，就不會出現這個警告 |
| 20:39 那次 136 ms 的確切時刻與觸發原因（是否和 04.845 的 `releasing pointer`／切換視窗同時發生） | 那次 Match 的日誌沒有存檔 | Match 存檔後，對照用戶端的 `releasing pointer` 與 `slow swapchain` 時刻 |

已驗證的部分（不需要再測）：在不含音效的 master 上，Match 停頓 ≥約 100 ms 一定會產生 1 次 Backlog 重設和 1 次窗口失敗；60 ms 不會（§3，試行 B）。


## 附錄：腳本（2026-10-31 隨報告搬入，原文）

原始 log 只在本機、不保存；判讀用的數字見本文的「收支」與各節的表格。

### `stall_run.py`

```python
"""Master: Match + Gateway + one headless bot; SIGSTOP the Match on a fixed schedule.

Usage: python3 stall_run.py <out_dir> <duration_s> [<at_s>:<stop_ms> ...]
"""
import os, signal, subprocess, sys, time
from pathlib import Path

ROOT = Path('/Users/karasu/Code/Source/GYO-Engine/.claude/worktrees/hopeful-wescoff-d692d8')
BIN = ROOT / 'build/target/object_fps_pvp/bin'
PROBE = ROOT / 'build/target/_build/dev/acceptance/object_fps_pvp/gyo_object_fps_pvp_quad_probe'
GATEWAY = Path(__file__).resolve().parent / 'gateway'

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=False)
duration = float(sys.argv[2])
schedule = [(float(a), int(b)) for a, b in (x.split(':') for x in sys.argv[3:])]

match = subprocess.Popen([str(BIN / 'gyo_object_fps_pvp-match'), '--log', str(out / 'match.log'),
                          '--movement-trace', str(out / 'movement.jsonl')],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.0)
gateway = subprocess.Popen([str(GATEWAY), '-log', str(out / 'gateway.log'), '-http', '127.0.0.1:8080'],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)
probe = subprocess.Popen([str(PROBE), '--gateway', '127.0.0.1:8080', '--arena', str(BIN / 'assets/object_fps_pvp/pvp_corners.json'),
                          '--output', str(out / 'probe.json'), '--clients', '1', '--duration', str(duration)],
                         stdout=open(out / 'probe.out', 'w'), stderr=subprocess.STDOUT)
start = time.monotonic()
log = open(out / 'injections.log', 'w')
for at, ms in schedule:
    while time.monotonic() - start < at:
        time.sleep(0.005)
    t0 = time.monotonic_ns(); wall = time.strftime('%H:%M:%S')
    os.kill(match.pid, signal.SIGSTOP)
    time.sleep(ms / 1000)
    os.kill(match.pid, signal.SIGCONT)
    print(f'{wall} at={at} stop_ms={ms} actual_ms={(time.monotonic_ns()-t0)/1e6:.1f}', file=log, flush=True)
code = probe.wait()
time.sleep(1.0)
for p in (gateway, match):
    p.send_signal(signal.SIGINT)
    try: p.wait(5)
    except subprocess.TimeoutExpired: p.kill()
print('probe_exit', code, 'match_exit', match.returncode, 'gateway_exit', gateway.returncode)

```

### `run_gui_master.sh`

```bash
#!/bin/zsh
# master (no audio) GUI session: Match + Gateway + one client, every log saved.
# Close the client window (or ESC to lobby, then quit) to end; Match and Gateway stop after it.
set -u
HERE=${0:A:h}
W=/Users/karasu/Code/Source/GYO-Engine/.claude/worktrees/hopeful-wescoff-d692d8
B=$W/build/target/object_fps_pvp/bin
OUT=$B/logs/master-gui-$(date +%Y%m%d-%H%M%S)

if lsof -nP -iTCP:8080 -iTCP:27016 -sTCP:LISTEN >/dev/null 2>&1; then
  echo "port 8080 or 27016 is in use: stop the other Match/Gateway first"; exit 1
fi
SHA=$(shasum -a 256 $B/gyo_object_fps_pvp-match | cut -c1-8)
if [[ $SHA != 98faf15c ]]; then
  echo "Match binary is not the master 5cfa149c build (sha256 $SHA...): rebuild master first"; exit 1
fi
mkdir -p $OUT
$B/gyo_object_fps_pvp-match --log $OUT/match.log --movement-trace $OUT/movement.jsonl >/dev/null 2>&1 &
MATCH=$!
for i in {1..50}; do grep -q "Match ready" $OUT/match.log 2>/dev/null && break; python3 -c "import time;time.sleep(0.1)"; done
$HERE/gateway -log $OUT/gateway.log -http 127.0.0.1:8080 >/dev/null 2>&1 &
GATEWAY=$!
for i in {1..50}; do lsof -nP -iTCP:8080 -sTCP:LISTEN >/dev/null 2>&1 && break; python3 -c "import time;time.sleep(0.1)"; done
echo "logs: $OUT"
echo "play: Create + Join, walk around 2-3 minutes (shooting is fine), then close the window"
# TEST_CLIENT (testing only) replaces the GUI client with a headless bot run.
if [[ -n ${TEST_CLIENT:-} ]]; then eval "$TEST_CLIENT"; else $B/gyo_object_fps_pvp --gateway 127.0.0.1:8080 --log $OUT/client.log; fi
kill -INT $GATEWAY $MATCH 2>/dev/null
wait $GATEWAY $MATCH 2>/dev/null
echo "---- summary ----"
python3 -I $HERE/summarize_stalls.py --match $OUT/match.log --client $OUT/client.log --trace $OUT/movement.jsonl

```

### `summarize_stalls.py`

```python
"""Summarize host stalls and quality-window failures of one PvP session.

Usage: python3 summarize_stalls.py --match match.log [--client client.log] [--trace movement.jsonl]
Counts only; it judges nothing.
"""
import argparse, json, re

parser = argparse.ArgumentParser()
parser.add_argument('--match', required=True)
parser.add_argument('--client')
parser.add_argument('--trace')
args = parser.parse_args()

def field(line, key):
    m = re.search(r'\b' + key + r'=([0-9.]+)', line)
    return float(m.group(1)) if m else None

windows = stalls = 0
late = []
failures = []
first = last = None
for line in open(args.match, encoding='utf-8', errors='replace'):
    stamp = line[11:19]
    if 'network statistics' in line:
        first = first or stamp; last = stamp
        windows += 1
        s = field(line, 'tick_stalls') or 0
        stalls += int(s)
        late.append((field(line, 'tick_late_max_us') or 0, stamp))
    elif 'quality window failed' in line:
        failures.append(stamp + ' ' + ' '.join(k + '=' + str(int(field(line, k) or 0)) for k in
                        ('player', 'consecutive_failures', 'movement_resets', 'substituted_commands', 'reference_age_ms')))
    elif 'evicted' in line:
        failures.append(stamp + ' EVICTED ' + line.strip()[-160:])
late.sort(reverse=True)
print(f'match: {first}..{last} windows={windows} tick_stalls_total={stalls}')
print('match: worst tick_late_max_us per window:', ', '.join(f'{int(v)}@{t}' for v, t in late[:5]))
print('match: windows with tick_late_max_us >= 85000:', sum(v >= 85000 for v, _ in late),
      ' >= 50000:', sum(v >= 50000 for v, _ in late))
print(f'match: quality window failed lines={len(failures)}')
for f in failures:
    print('  ', f)

if args.trace:
    reasons = {}
    gaps = 0
    for line in open(args.trace, encoding='utf-8', errors='replace'):
        if '"kind":"reset"' in line:
            r = json.loads(line).get('reset_reason')
            reasons[r] = reasons.get(r, 0) + 1
        elif '"kind":"runtime_gap"' in line and json.loads(line).get('dropped_seconds', 0) > 0:
            gaps += 1
    print(f'trace: resets by reason={reasons} runtime_gaps_with_dropped_time={gaps}')

if args.client:
    frames = slow = 0
    longest = []
    for line in open(args.client, encoding='utf-8', errors='replace'):
        if 'PvP summary' in line:
            v = field(line, 'longest_frame_ms')
            if v is not None:
                longest.append((v, line[11:19]))
                frames += v >= 50
        elif 'slow swapchain acquire' in line:
            slow += 1
        elif 'sound summary' in line or 'sound started' in line or 'connection quality' in line:
            print('client:', line.strip()[30:200])
    longest.sort(reverse=True)
    print(f'client: summary_seconds={len(longest)} seconds_with_frame>=50ms={frames} slow_swapchain_acquire={slow}')
    print('client: worst longest_frame_ms:', ', '.join(f'{v}@{t}' for v, t in longest[:5]))

```

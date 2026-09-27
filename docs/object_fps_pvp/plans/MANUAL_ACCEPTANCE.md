# PvP v4 手動啟動與完整驗收

適用版本：2026-09-28驗收完成的 **v4穩定基線**。Owner：`object_fps_pvp`。
完整結果見 [驗收狀態與回報表](ACCEPTANCE_STATUS.md) 與
[基線指紋／認證範圍](STABLE_BASELINE.md)。既有v3 manifest與失敗紀錄保留。

> 2026-09-28 追加授權：使用者已將這次完整驗收交由 Agent 執行，並授權修復及通過後升格。
> 本文件不刪除；以下手動操作方式與門檻繼續保留，供未來測試案例參考。
> 本次三輪GUI、兩組長測與原生桌面操作已由Agent完成；以下是未來自行重跑的方式。

## 如何使用

1. 先依下一節自行開 Match、Gateway、兩個正式 Client，完成目視試玩並保存日誌。
2. 關閉試玩程序後，自行啟動量化 probe：GUI 三輪、headless 60／144 Hz 各 30 分鐘。
3. 保留完整輸出目錄，將摘要及填好的回報表交給 Agent。失敗保留原跑次，不覆寫，
   不無限重跑；先分析原因，再決定是否重跑受影響項目。

正式試玩程序由你啟動。量化 runner 則是你啟動一次命令後，由腳本在私有 localhost
連接埠建立／清理自己的 Match、Gateway、probe；它不操控你另外開啟的遊戲程序。
不要同時執行多個驗收、建置或大型工作，以免互相干擾。未來是否由Agent執行長測，以該次明確授權為準。

## 正式 Client 試玩：四個終端

以下命令已使用本機實際路徑；從同一份工作樹、同一套建置產物啟動。
本指南預設 Linux、同機、Vulkan。`--movement-trace` 是既有可選診斷，預設關閉；
由背景 writer 寫檔、記錄有界緩衝丟失數。Gateway 已有傳輸／限流／連線日誌，
直接保存 stdout／stderr，不增加每包同步寫檔或新的玩法控制。

先在第一個終端建立全新跑次目錄及四個終端共用的環境檔：

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
PVP_RUN=$(mktemp -d "$PWD/build/target/_build/test/logs/pvp-v4-manual-XXXXXXXX") || exit 1
export PVP_RUN
printf 'export PVP_RUN=%q\n' "$PVP_RUN" >| build/target/_build/test/logs/pvp-v4-manual-current.env
git rev-parse HEAD > "$PVP_RUN/head.txt"
git status --short > "$PVP_RUN/worktree-status.txt"
sha256sum \
  build/target/object_fps_pvp/bin/gyo_object_fps_pvp \
  build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  build/target/object_fps_pvp/bin/assets/object_fps_pvp/asset_catalog.json \
  > "$PVP_RUN/artifacts.sha256"
```

終端 1，Match（等待 `match.log` 出現 ready）：

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
source build/target/_build/test/logs/pvp-v4-manual-current.env
set -o noclobber
build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --listen 127.0.0.1:27016 --movement-trace "$PVP_RUN/match-commands.jsonl" \
  > "$PVP_RUN/match.log" 2>&1
printf '%s\n' "$?" > "$PVP_RUN/match.exit"
```

終端 2，Gateway：

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
source build/target/_build/test/logs/pvp-v4-manual-current.env
set -o noclobber
build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --runtime 127.0.0.1:27016 --http 127.0.0.1:8080 \
  --udp 127.0.0.1:27015 --advertise-ip 127.0.0.1 \
  > "$PVP_RUN/gateway.log" 2>&1
printf '%s\n' "$?" > "$PVP_RUN/gateway.exit"
```

終端 3，Client A，先在 Lobby 建房：

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
source build/target/_build/test/logs/pvp-v4-manual-current.env
set -o noclobber
build/target/object_fps_pvp/bin/gyo_object_fps_pvp \
  --gateway 127.0.0.1:8080 --gpu-driver vulkan \
  --movement-trace "$PVP_RUN/create-commands.jsonl" \
  > "$PVP_RUN/create.log" 2>&1
printf '%s\n' "$?" > "$PVP_RUN/create.exit"
```

終端 4，Client B，在 Lobby 加入同一房間：

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
source build/target/_build/test/logs/pvp-v4-manual-current.env
set -o noclobber
build/target/object_fps_pvp/bin/gyo_object_fps_pvp \
  --gateway 127.0.0.1:8080 --gpu-driver vulkan \
  --movement-trace "$PVP_RUN/join-commands.jsonl" \
  > "$PVP_RUN/join.log" 2>&1
printf '%s\n' "$?" > "$PVP_RUN/join.exit"
```

每次重跑都從準備步驟建立新目錄；四個終端啟用 noclobber，避免覆寫已存在日誌。
測完先正常關閉兩個 Client 視窗，再在 Gateway 終端 Ctrl+C，最後 Match Ctrl+C。
Client 不用強制 kill，才能完成 worker 清理及 `trace_end`。一般驗收退出碼應為 0；
強制中止、缺 `trace_end`、`dropped > 0` 或寫檔失敗均要回報，不能視為完整資料。
Gateway 結束時保存合併次數、寫出年齡、接受／限流包數。不要把封包缺口直接叫做丟包。

這些正式程序日誌可定位移動產生／傳送／執行、重設與連線問題，**沒有逐槍裁決帳本
或全部畫面位置**，因此不能單靠它們證明零重複扣血、射擊分位數或跨視窗延遲。
後兩節的 probe 另保存逐動作、每幀、權威來源及配對資料；不為人工試玩新增遊戲邏輯。

## 目視試玩清單

逐項在 [回報表](ACCEPTANCE_STATUS.md) 填通過／失敗／未測，並附異常發生的時間及步驟。
切換視窗必然造成其中一個 Client 失焦；此類生命週期操作與乾淨性能跑次分開。

| ID | 操作 | 預期 |
|---|---|---|
| V1 | 平移、斜走、停止、貼牆／牆角、只轉視角 | 本機連續順暢；鏡頭不穿牆；停止後不持續滑動 |
| V2 | 移動＋瞄準＋射擊 | 無新增卡頓；後座只影響槍模，不額外改瞄準 |
| V3 | 首次點擊捕捉；再次單擊；按住左鍵 | 捕捉那次不射擊；後續單擊即播一次動畫；按住不連發 |
| V4 | 近距離瞄準靜止玩家，每槍至少間隔 400 ms | 有效命中依序 100→75→50→25→0；每個動作只扣一次；接受且命中才亮標記 |
| V5 | 對牆、隔牆、刻意射偏 | 遮擋／未命中不扣對方 HP，也不亮玩家命中標記 |
| V6 | HP=0 後移動／射擊；Esc 離開再加入 | 0 血仍可操作；新加入滿血，無舊提示或幽靈玩家；PlayerId 是新加入身分，不要求沿用舊號 |
| V7 | Tab、切換視窗、拖標題列、縮放視窗 | 釋放輸入，不產生幽靈射擊／持續移動；已送出的操作仍完成裁決 |
| V8 | 雙方 Esc 離開、觀察 Lobby；關掉一個 Client | 人數與可加入狀態正確，舊角色不復活 |

v4 使用**當前權威姿態**，尚無歷史命中補償。移動目標畫面與權威位置不同的命中
爭議要記錄，不可靠修改傷害、期限或回溯政策來使本輪測試通過。
歷史 OS 拖窗整機停頓根因仍未證實；若重現，記錄 OS／GPU／視窗系統、時間及恢复方式。

## 量化門檻

| 指標 | 門檻與計算範圍 |
|---|---|
| 移動產生→首次成功送出 | 乾淨同機 P95 ≤22 ms |
| 移動產生→Actual | 乾淨同機 P50 ≤50 ms，P95 ≤66.7 ms；暖機後原始命令 Actual ≥99% |
| 跨視窗可見交越 | 每輪 P50 ≤50 ms、P95 ≤80 ms；200 個預排事件各取一組交越，配對率 ≥99% |
| 缺失事件 | 未配對／不明者按 +∞ 納入 nearest-rank 分位數；全部事件納入分母，保留結尾 2 秒配對 |
| 射擊即時呈現 | 下一個成功 `Presented` 幀開始；不等待 Match。Skipped 不可重用前一幀觀測 |
| 射擊產生→Match 裁決 | 乾淨同機 P95 ≤100 ms，包含接受與拒絕；同 ActionId 對齊 |
| 射擊產生→Client 消費裁決 | 乾淨同機 P95 ≤150 ms；合法節奏（間隔 ≥400 ms）應全部有裁決且接受 |
| 去重／權威效果 | 每 ActionId 最多一次接受／效果；零重複扣血；過期、冷卻、非法身分零傷害 |
| 容量 | 動作窗口 ≤32、動作批次 ≤8、完整 UDP 含 24-byte header ≤1,200 bytes；移動窗口 ≤12／Match 未來 ≤32 |
| 故障恢復 | 有效 Session 在最後干擾解除後 1.5 秒內恢復新 Actual、預測、目前遠端時間線及新裁決；窗口不滿、30 Tick 佇列和 <105，持續 ≥250 ms |
| 乾淨長測 | 60／144 Hz 分別實時量測 1,800 秒；零非預期 epoch 重設、零診斷遺失、零模擬掉時、零持續窗口凍結 |
| 干擾分類 | 注入故障、失焦／斷線／休眠、參與迴圈 ≥100 ms 停頓或任何模擬掉時，保留為受干擾跑次，不算乾淨通過 |

GUI 名義至少 60 FPS，記錄實際 FPS 及**所有幀間隔**，不剔除慢幀；命令時間用於定位，
畫面位移交越用於呈現。計時期間不存圖、不做 GPU readback；GPU 正確性另驗。
只比較同機單調時鐘，不稱作 input-to-photon。實體 LAN／Windows／跨主機時鐘漂移
需另報適用範圍，不套用同機時鐘門檻。冷卻壓力案例與合法射速報表分開。

## GUI 三輪，由使用者啟動

已有第 04 批 30／60／144 FPS 射擊短測及第 05 批 Agent 單輪整合結果。
未來完整重驗使用以下三輪，各輪獨立120秒／200事件，同時射擊與核對HP。
純移動模式只作診斷，不能算 v4 完整整合通過。任一輪失敗即停止，
後續輪標未執行，不把三輪合併平均。請勿加入 `--report-only`。

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
PVP_GUI="build/target/_build/test/logs/pvp-v4-user-gui-$(date +%Y%m%d-%H%M%S-%N)"
python3 build/acceptance/object_fps_pvp/run_timing.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output "$PVP_GUI" \
  --gui --combat --rounds 3 --duration 120 --events 200 --fps 60
printf '%s\n' "$?" > "$PVP_GUI/runner.exit"
```

這裡 `--arena` 必須使用部署目錄，runner 將同目錄當成 GUI asset root。
量測時讓兩個視窗持續呈現；不要操作它們、拖曳、切換／最小化或遮蔽造成節流。
人工輸入清單與自動 SDL 事件量測是不同跑次。

## Headless 兩組長測，由使用者啟動

兩個命令**依序**執行，勿同時執行。每組實時量測 1,800 秒，另有 2 秒暖機及
2 秒結尾裁決／確認排空；正常合法射擊間隔至少 400 ms。`--soak` 是顯式長測授權，
沒有它只能跑短模式；不會自動重跑。不使用舊 movement-only soak 代替。

60 Hz：

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
PVP_LONG60="build/target/_build/test/logs/pvp-v4-user-soak60-$(date +%Y%m%d-%H%M%S-%N)"
python3 build/acceptance/object_fps_pvp/run_action_legal.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_action_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output "$PVP_LONG60" --fps 60 --duration 1800 --soak
printf '%s\n' "$?" > "$PVP_LONG60/runner.exit"
```

144 Hz（第一組完成並保存後）：

```bash
cd /home/karasu/Workspace/Source/GYO/GYO-Engine
PVP_LONG144="build/target/_build/test/logs/pvp-v4-user-soak144-$(date +%Y%m%d-%H%M%S-%N)"
python3 build/acceptance/object_fps_pvp/run_action_legal.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_action_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output "$PVP_LONG144" --fps 144 --duration 1800 --soak
printf '%s\n' "$?" > "$PVP_LONG144/runner.exit"
```

這兩組驅動真正的 ClientConnection、移動預測、Match、Gateway，沒有 GPU 視窗，
**不是 GUI 長測**。兩端同時射擊；每份 HP 快照按該 Snapshot Tick 之前的唯一裁決
傷害累加核對，拒絕必須零傷害。前四次命中造成實際扣血到 0，後續有效命中傷害為
0，這是 v4 規則，不人工回血。0 血後單靠 HP 已無法辨認額外重複扣血，仍須核對
不可變動作識別、唯一裁決、接收／退休完整性及 domain 的去重測試；報告明列此限制。

Match 裁決時間以同 ActionId 的 `resolvedTick` 對齊產品緊接該 Tick 的快照產生
單調時間，作裁決延遲**保守上界**；找不到對應 Tick 就失敗，不臆造精確裁決時間。
Client 端則量到主迴圈取得裁決。資料串流寫入有界佇列，遺失／截斷不可算通過。

## 回傳哪些檔案

| 跑次 | 優先回傳 | 同時保留的原始資料 |
|---|---|---|
| 人工試玩 | 填好的回報表、四份 `.exit`、`artifacts.sha256`、異常 `.log` 片段 | 三份 `*commands.jsonl`、四份 `.log`、HEAD／worktree 狀態 |
| GUI 三輪 | 頂層 `results.json`、`summary.md`、`runner.exit` | 各 `round-*` 的 `round.json`、manifest、呈現／動作／HP trace、service logs、完整失敗原因 |
| Headless 60／144 | 各自 `result.json`、`report.txt`、`runner.exit`、`artifacts.json` | movement／action／frame 原始 trace、`command-evidence.json`、probe／服務 logs |

結果 JSON 的 `passed` 與完整長測資格都必須通過；退出 0 也不會自動改穩定基線。
GUI 的三輪結果不能和 Agent 的診斷跑次拼湊成三輪。若檔案太大，先回摘要、manifest、
失敗片段及原始目錄路徑；保留完整檔案以供後續針對性分析，無需先貼數百 MB 日誌。

## 回報與停止條件

- 正常完成：保存 JSON 摘要、可讀報告、退出碼及完整原始目錄；將摘要與回報表傳回。
- 失敗／受干擾：保存第一份失敗、最後錯誤、受影響時間及操作；不要刪檔或覆寫。
- 未完成／缺檔／非零退出碼：均不算通過。即使畫面手感正常，也不能省略未知裁決。
- 產品來源／二進位／Arena 變更後，原報告只能證明舊版本；記錄 hash，依影響重驗。
- 新版本／變更後的候選須先核對完整驗收，再更新基線；本次v4已完成升格，見[穩定基線](STABLE_BASELINE.md)。
  不自動把計畫「工具交付完成」等同「v4 穩定」。


## 原生 X11 操作回歸（本次追加工具）

需要已登入的 X11 桌面、GNOME 的預設 Super＋中鍵縮放手勢、libX11／libXtst、
`xwd` 與 Python Pillow。只操作 runner 自己建立且已驗證 PID／標題的兩個視窗。
若系統手勢不同，先核對桌面設定與測例，不把沒有命中裝飾區視為產品缺陷。
本模式是功能／生命週期測試，會實際轉移焦點、拖曳、縮放及關閉自己的視窗；
不要與乾淨延遲／長測同時執行。它不取代前述 GUI 分位數或 headless 30分鐘。

從 repository root 執行；`--output` 必須指定尚不存在的新目錄：

```bash
python3 build/acceptance/object_fps_pvp/run_native_window.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --gui-probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --arena-root build/target/object_fps_pvp/bin/assets/object_fps_pvp \
  --gpu-driver vulkan \
  --output build/target/_build/test/logs/pvp-v4-native-manual-1
```

輸出 `native-window-result.json`、兩端 `*-native-frames.jsonl`、console及包含HUD
的PNG。完整模式需V1–V8全部通過，失敗非零退出；`--window-only`只診斷視窗幾何，
不能取得完整資格。初次房間加入由probe準備，後續操作走原生XTest；圖像需另行目視。

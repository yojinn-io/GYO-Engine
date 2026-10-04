# 05 驗收 runner 同時啟動 Match 與 Gateway 的競態

狀態：已解決（驗收器修正）。發現與修正：2026-10-02。Owner：`object_fps_pvp`。
相關：PR #2（A1，已合併為`ff11ee3`，不含本修正）；本修正經PR #3（`claude/pvp-v5-start-phase-guard`）合併為`9cd7f26`。
範圍：只改產品自有驗收器`build/acceptance/object_fps_pvp/`；Match／Gateway／Client程式、wire契約、
啟動規則與門檻都不變。撰寫時本修正不代表第03批結案；第03批後於2026-10-02結案驗收通過（計次GUI短測3輪、25案矩陣25／25）。
索引與其他問題見[本目錄總覽](README.md)，時間順序見
[守門dev_log](../../../../dev_logs/2026_10_02_pvp_v5_start_phase_guard.zh-Hant.md)。
同批另一個驗收器修正（跨行程時鐘域）見[04](04-macos-clock-domain.md)，兩者的helper都在`run_network.py`。

## 問題成因

Gateway啟動時只撥號Match IPC一次，失敗即結束行程；Match要先載入Arena並完成listen才接得到。

| 位置 | 行為 |
|---|---|
| `apps/object_fps_pvp/gateway/runtime_link.go:37` `connectRuntime` | `framing.DialTCP(ctx, address, 3*time.Second)`單次撥號、無重試；連上後`conn.Read(3 * time.Second)`等Ready |
| `services/gyo_gateway/framing/connection.go:14` `DialTCP` | 一次`net.Dialer.DialContext`；埠尚未listen時立即回`connection refused` |
| `apps/object_fps_pvp/gateway/server.go:104` `New` | 撥號錯誤直接回傳，不建立Server |
| `apps/object_fps_pvp/gateway/cmd/main.go:23–25` | `gateway.New`失敗即`log.Fatal`，exit 1 |
| `apps/object_fps_pvp/match_main.cpp:78–86` | `Arena::Load`→建runtime與模擬執行緒→`ipc.Start(listen,error)`完成listen→才印`Object_FPS_PVP Match ready: <listen> arena=… authority=60Hz` |

這是既定產品行為，不是產品缺陷：[網路架構](../../../network-architecture.zh-Hant.md)寫明
「啟動順序錯誤或Runtime未Ready時Gateway啟動會失敗，不自動生成Match」，手動指南也是先開Match再開Gateway。

缺陷在驗收器：10個runner原本以`subprocess.Popen`連續啟動Match與Gateway，中間不等待，
只在Gateway啟動後輪詢HTTP `/rooms`。本機Intel Mac上Gateway常比Match先撥號：
`dial tcp 127.0.0.1:<ipc>: connect: connection refused`→Gateway退出→runner在`/rooms`迴圈
看到服務已結束而失敗（`run_timing.py`為`Service startup failed`）。`/rooms`輪詢補救不了：
Gateway已退出，不會再撥。

### 為什麼不能用裸TCP connect探測就緒

Match的IPC端把任何被accept的連線都當作Gateway session：

- `apps/object_fps_pvp/src/Pvp/IpcHost.cpp` `IpcHost::Start`：`listener.listen(1)`，backlog只有1。
- 同檔`IpcHost::Impl::Run`：一次只服務一條連線；accept後`host.RequestReset()`，
  `Connection()`先排入Ready封包並服務到斷線，斷線後再`RequestReset()`。

所以探測連線本身就是一次假的Gateway session：

- runtime多重設兩次，探測對被測系統有副作用。
- 探測連線關閉、Match察覺斷線並重設完成前，唯一的session被佔用；真Gateway只能在backlog等，
  而`connectRuntime`只等Ready 3秒，逾時同樣退出。
- 「連得上」只證明listen完成；Match自己的ready行在`ipc.Start`成功後才印，資訊相同且無副作用。

## 影響

- 受影響：所有同時啟動Match與Gateway的產品驗收runner，共10個：`action_probe.py`／`backpressure_probe.py`／
  `recovery_probe.py`／`run_action_legal.py`／`run_gameplay_gui.py`／`run_native_window.py`／
  `run_network.py`／`run_player_short.py`／`run_timing.py`／`run_weapon_short.py`。
  涵蓋GUI可見延遲短測、25案真網路矩陣（`run_gameplay.py`經`action_probe.run_case`）、合法射擊長測與各短測。
- 症狀：runner約0.4秒內失敗，`results.json`為`passed: false`；`gateway.log`只有一行`connection refused`；
  沒有任何量測資料。
- 實機實例：2026-10-02整合第2輪GUI短測（report-only、不計次）連續啟動三次，前兩次失敗
  （00:47:16與00:47:49，IPC埠51001／51010），第三次成功並PASS。第2次的`match.log`仍有ready行，
  表示Match正常啟動，只是晚於Gateway撥號。當時runner尚無任何等待。
- 計次風險：`run_timing.py`的`execute_rounds`把啟動失敗記為`failed`並把後續輪標`not_run`；
  補跑規則只依視窗證據（見[總覽](README.md)第06項），不涵蓋啟動失敗。若發生在計次輪，會留下非產品原因的失敗輪、
  依規保留且不能刪除，並使整組計次停止。本批未執行計次輪，未實際發生。
- 不受影響：產品Match／Gateway／Client與wire；依文件分終端手動啟動（人為間隔遠大於啟動耗時）；
  已成功啟動的跑次的量測值（競態只決定能否啟動，不改變啟動後的延遲資料）。
- 既有dev_log（含Linux v4驗收）未記錄此失敗；平台間啟動耗時差異未量測，不作結論。

## 如何復現

全部是實機行程，不需GUI／Metal；在repo根目錄執行。實測環境：Intel MacBook Pro 2019（x86_64）、
macOS 26.7.1。方法A在任何平台都必定失敗；方法B的失敗率取決於兩個行程的啟動耗時，其他機器可能不同。
方法A用埠47015／47016／47080，方法B用47101–47310，須未被佔用。

前置，建置兩個服務：

```bash
cmake --preset test
cmake --build --preset test --target gyo_object_fps_pvp-match gyo_object_fps_pvp-gateway
```

方法A（必定重現Gateway單次撥號）：不啟動Match，直接啟動Gateway。

```bash
build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --runtime 127.0.0.1:47016 --http 127.0.0.1:47080 --udp 127.0.0.1:47015 \
  --advertise-ip 127.0.0.1; echo "exit=$?"
# 預期：... dial tcp 127.0.0.1:47016: connect: connection refused，exit=1
```

方法B（重現競態，並以先等ready行作對照）：存成`race.sh`後以`bash race.sh`執行。

```bash
M=build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match
G=build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway
A=build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json
OUT="${OUT:-build/target/_build/test/logs/pvp-startup-race-$(date +%Y%m%d-%H%M%S)}"
ORDERED="${ORDERED:-0}"; N="${N:-10}"; mkdir -p "$OUT"; exited=0
for i in $(seq 1 "$N"); do
  ipc=$((47100 + i)); http=$((47200 + i)); udp=$((47300 + i))
  "$M" --arena "$A" --listen "127.0.0.1:$ipc" > "$OUT/match-$i.log" 2>&1 & mp=$!
  if [ "$ORDERED" = 1 ]; then  # 對照：等Match的ready行，最多15秒
    for _ in $(seq 1 750); do
      grep -q "^Object_FPS_PVP Match ready: 127.0.0.1:$ipc " "$OUT/match-$i.log" && break; sleep 0.02
    done
  fi
  "$G" --runtime "127.0.0.1:$ipc" --http "127.0.0.1:$http" --udp "127.0.0.1:$udp" \
    --advertise-ip 127.0.0.1 > "$OUT/gateway-$i.log" 2>&1 & gp=$!
  sleep 1
  kill -0 "$gp" 2>/dev/null || exited=$((exited + 1))
  kill "$gp" "$mp" 2>/dev/null; wait "$gp" "$mp" 2>/dev/null
done
echo "ordered=$ORDERED gateway exited at startup: $exited / $N"
```

- `bash race.sh`：同時啟動。預期部分或全部Gateway退出，`gateway-<i>.log`為`connection refused`，
  同號`match-<i>.log`仍有ready行。
- `ORDERED=1 bash race.sh`：先等ready行。預期`0 / N`。

runner層級：修正前的runner已不在工作樹；它在兩次`Popen`之間沒有等待，行為等同方法B的同時啟動。
不要為重現而改回工作樹上的runner；runner的啟動順序由下節`test_service_startup.py`鎖住，
刪掉任一runner的`wait_for_match_ready(...)`呼叫，該測試即失敗。

## 解決方案

Runner改為「啟動Match→讀Match自己的日誌等ready行→才啟動Gateway」。Gateway、Match與契約不改。

`build/acceptance/object_fps_pvp/run_network.py`新增共用helper：

| 項目 | 內容 |
|---|---|
| `MATCH_READY_PREFIX` | `"Object_FPS_PVP Match ready: "`，與`match_main.cpp`印出的字串相同 |
| `MATCH_READY_TIMEOUT_SECONDS` | `15.0` |
| `match_ready(log_text, listen)` | 某一行等於前綴＋`listen`，或以前綴＋`listen`＋空白開頭才算；`127.0.0.1:500`不會誤中`127.0.0.1:5000` |
| `wait_for_match_ready(process, log_path, listen, timeout=15.0, *, clock, sleep, poll_seconds=0.02)` | 每20ms讀一次Match日誌；見到ready行即返回。Match先退出→立即`RuntimeError("Match exited with code N before it listened on …; inspect match.log")`；逾時→`RuntimeError("Match did not report listening on … within 15 s; …")`。全程不開socket |

- 10個runner都在`start('match', …)`與第一個Gateway啟動之間呼叫一次，等的是剛啟動的Match、
  它的`match.log`和它自己的listen位址。`host-ipc`情境的`IpcPause` relay也在ready之後才建立。
- Gateway啟動後原有的`/rooms` HTTP輪詢保留，負責確認Gateway本身就緒。
- `run_network.py`的預設Arena檢查（`match-default-arena`，另一個埠）仍用`socket.create_connection`探測：
  那個Match沒有Gateway、隨即被終止，假session的副作用無關，維持原樣。
- 新增`build/acceptance/object_fps_pvp/test_service_startup.py`，並在`tests/object_fps_pvp/CMakeLists.txt`
  註冊CTest `object_fps_pvp.service_startup`（labels `cpu;pvp;acceptance`，TIMEOUT 10）。

不採用的做法：

| 做法 | 不採用原因 |
|---|---|
| Gateway撥號重試／退避 | 改變產品既定的fail-fast行為與文件化啟動順序，屬產品決策而非驗收器修正；也會掩蓋實際部署的設定錯誤 |
| 裸TCP connect探測 | 會成為假的Gateway session（見上節）：多兩次runtime重設，並可能佔住唯一session使真Gateway等Ready逾時 |
| 固定sleep | 數值任意：太短在慢機或負載下仍會競態，太長浪費時間；失敗時也沒有原因 |
| 只靠既有`/rooms`輪詢 | Gateway已退出，輪詢只能更晚報同一個失敗 |

## 驗證

自動測試（`test_service_startup.py`，7項；不開socket、不起服務）：

- `MatchReadyTests`（5項）：log由不存在→空→出現ready行，只輪詢、從不連線（`socket.socket`與
  `socket.create_connection`被patch成會失敗）；只認完全相同的listen位址；Match先退出時立即失敗、
  不等滿逾時；逾時有上限且常數為15秒；ready前綴確實出現在`match_main.cpp`且位於`ipc.Start(listen,error)`之後。
- `RunnerUsageTests`（2項）：以AST掃描本目錄所有非測試`*.py`，凡組出`--runtime`的檔案必須正好是上述
  10個runner；每個啟動Gateway的函式只有一次Match啟動與一次等待，且順序為Match→等待→第一個Gateway，
  等待對象、`match.log`與listen位址都對應同一個Match。另以mock跑`run_timing.execute_rounds`：
  等待失敗時只啟動並終止了Match、從未啟動Gateway，`runner-failure.txt`含`before it listened`。

執行：

```bash
python3 build/acceptance/object_fps_pvp/test_service_startup.py -v
ctest --preset test -R 'object_fps_pvp\.service_startup' --output-on-failure
ctest --preset test -L pvp --output-on-failure
```

修正後的實機啟動檢查（需顯示器與GPU；macOS需Metal toolchain；機器閒置、一次一輪；
report-only，不計次）：

```bash
cmake --build --preset test --target gyo_object_fps_pvp-match gyo_object_fps_pvp-gateway gyo_object_fps_pvp_gui_probe
python3 build/acceptance/object_fps_pvp/run_timing.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output "build/target/_build/test/logs/pvp-startup-check-$(date +%Y%m%d-%H%M%S)" \
  --gui --short --report-only --rounds 1 --duration 16 --events 20 --fps 60
# 預期：round-1/match.log先有ready行，gateway.log有Object FPS Gateway HTTP=…行；不出現connection refused
```

紀錄（本機證據，git-ignored）：

- 整合第3輪`build/target/_build/test/logs/pvp-v5-batch03-integration3-20261002-015053/`：
  `ctest -L pvp` 16／16通過（`service_startup` 0.61秒）；`python-tests.log`中本檔7項OK。
- 修正後實機啟動：同一輪GUI短測（report-only）與矩陣單案`upstream-250ms`（同時驗證[04](04-macos-clock-domain.md)）
  都正常啟動並PASS。兩者皆不計次、不作為驗收證據，也只是兩次啟動，不是統計證明；
  可靠性依據是ready行在listen完成後才印。
- 失敗原始紀錄：`build/target/_build/test/logs/pvp-v5-batch03-integration2-20261002-004224/`的
  `gui-smoke-attempt1-gateway.log`、`gui-smoke-attempt2-gateway.log`、`gui-smoke-attempt1-runner-failure.txt`、
  `gui-smoke-attempt2-match.log`；第三次成功的那輪在
  `build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/integration-smoke-2/`。
- 撰寫本文時（2026-10-02）以方法B在同一台Mac確認：同時啟動10／10 Gateway退出；先等ready行0／10。
  不計次、不作為驗收證據，輸出未保存。

## 殘留風險與後續

- 依賴ready行的字串。字串改變時`test_ready_line_is_the_one_the_match_prints`會失敗，不會靜默放行。
- 依賴runner把Match的stdout導向`match.log`；修改runner的`start()`時須保留，AST測試會檢查檔名。
- ready行只代表IPC已listen，不代表其他初始化完成；Gateway端仍由既有`/rooms`輪詢與Ready契約檢查把關。
- Gateway單次撥號仍是產品行為。日後若以服務管理器或打包腳本同時啟動兩個服務，會遇到同一問題；
  屆時要在啟動順序或Gateway重試之間作產品決定。目前未排程，不構成承諾。
- 本次只在macOS實機驗證；helper不依平台，但Linux／Windows未重跑這些runner。

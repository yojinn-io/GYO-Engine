# 02 stall reseed 使 A1 在整個 epoch 失效（開局卡頓）

狀態：**已解決（2026-10-02實作與CPU驗證完成，PR #11送審中；實機report-only GUI冒煙1輪PASS（不計次）；第03批結案驗收未執行）**。Owner：`object_fps_pvp`。
相關：A1本體PR #2（合併為`ff11ee3`）；守門v3與`CancelledByReseed`診斷經PR #3
（`claude/pvp-v5-start-phase-guard`）合併為`9cd7f26`。第03批未結案，計次輪未執行，第04批暫停。
已解決的只有「看得見」：產品回報取消原因，驗收器記錄取消與量測窗口內的對齊秒數。
取消後整個epoch不再對齊這件事本身未修；下列方案只做評估與原型，沒有進產品。
2026-10-02起與[08](./08-a1-clock-drift.md)（時鐘漂移）一起改以閉環的持續相位追蹤處理，見文末「方向變更」。
索引見[README](./README.md)；低幀率守門見[01](./01-a1-low-fps-regression.md)，
偶發掉幀重設見[03](./03-a1-missed-frame-starvation.md)，補跑規則見[06](./06-gui-window-interference-and-rerun-rule.md)，
啟動相位紀錄見[07](./07-start-phase-diagnostics.md)。

## 問題成因

**A1每個epoch只量一次。**`MatchRuntimeHost`記下epoch首窗口（含seq1）的收到時刻，到執行seq1的Tick
算出w，以`PlayerState.epochStartWaitMicros`隨Snapshot送出。Client在`LocalPlayerPrediction::Reconcile`
（`LocalPlayerPrediction.cpp`約192行）湊滿8個幀間隔後一次決定調整量：
w＋首窗口發布時首步已過時間−4ms（`MovementStartPhaseTargetSeconds`）。
這個w只描述「epoch開始播種的那個固定步相位」。

**卡頓丟時間 → Host替補 → stall reseed。**
- `Advance`在新播種後的第一幀、或`pending_`為空且幀長>3 Tick（`MovementMaximumRegularFrameSeconds`＝50ms）
  的covered gap，只推進至多1 Tick，其餘丟棄（trace `runtime_gap.dropped_seconds`）。
- 卡頓期間Client不產生命令，Host照60Hz前進並以Held替補。Snapshot的`lastResolvedCommand`超過
  Client的tip時，`Reconcile`走stall reseed（約154行，`authority.lastResolvedCommand > oldTip`→`SeedLead`）。
- `SeedLead`（約74行）`ticks_.Reset()`，從權威已執行序號重播兩個中立lead命令。
  固定步相位改由播種幀起算，與Host量到的相位無關，所以w失效。

**取消是整個epoch。**
- `SeedLead`在相位待決（`startPhasePending_`）或已決定（`armedPhaseShiftSeconds_`）時清除調整量；
  `startPhasePending_`只在`lastResolvedCommand == 0`的epoch開始播種時為真，之後不再量測、不再恢復。
  首窗口後到決定前若有丟時間的幀，`Advance`（約304行）同樣放棄待決量測。
- 只有新epoch才重量：Host移動重設（Starvation／Backlog）或重生（新lifeGeneration）。
  沒有重設時一個epoch可延續整條生命。其餘時間回到未對齊的啟動相位抽籤：reseed後的相位等於重抽一次。

**為何GUI冒煙容易觸發。**GUI probe（`build/acceptance/object_fps_pvp/gui_main.cpp`，`RunLatency`）在
`InitializeGraphics`後立刻`CreateAndJoin`（create＝移動方，約367行）；join方大廳一出現房間就加入。
兩者都在圖形就緒後不久進入世界：三輪≥95ms的卡頓都落在圖形就緒後0.2–0.7 s、第一個進入世界的行程
進入後0.09–0.36 s，正好是在世界中的那一方吃到。兩人同時在場的那一幀，移動方還會寫事件計畫並
`SDL_RaiseWindow`（約418行）。

**卡頓來源未分離。**行程啟動本身、首次世界／GPU渲染、首次渲染對手模型三者，三個樣本無法區分。
log的`render_ms`是CPU端時間，看不到GPU／driver／drawable阻塞。smoke-2、smoke-3的兩個行程在
同一牆鐘時刻一起卡頓；smoke-1的53ms與「首次看到對手＋`SDL_RaiseWindow`」同一幀，也分不開。

## 影響

**實機時間線**（本機Intel Mac，report-only冒煙，不計次、不作為驗收證據；時間以該角色epoch播種起算）：

| 冒煙（產品） | 角色 | 進入世界後的卡頓（丟棄時間） | stall reseed | 量測16 s內 |
|---|---|---|---|---|
| smoke-1（守門v1） | create（移動方） | 0.56 s：53.0ms（丟36.4ms） | 0.69 s，seq42起；決定後0.53 s | 對齊0 s（舊產品誤報`shift_armed`，重分析為取消） |
| | join | 大廳中卡120.7ms，進世界後無 | 無 | 對齊16 s（w 13.0ms、shift 18.7ms） |
| smoke-2（守門v2） | create | 首個世界幀卡約0.26 s（阻塞在播種前，預測未計入）；0.12 s：97.2ms（丟80.6ms） | 0.12 s，seq7起；決定前 | 對齊0 s |
| | join | 大廳中卡128.6ms | 無 | 對齊16 s（w 5.5ms、shift 2.1ms） |
| smoke-3（守門v3） | create | 0.26 s：66.0ms（丟49.3ms）；0.37 s：117.6ms（丟101.0ms）；另有約40ms未丟時間 | 兩次，seq15／seq22；產品自第12幀回報 | 對齊0 s |
| | join | 0.21 s：114.7ms（丟98.0ms） | seq13起；產品自第8幀回報（決定前） | 對齊0 s |

- 6個GUI角色中4個在世界中遇卡頓並被取消（三輪的移動方全中）；另2個join在大廳中吃掉卡頓，保持對齊。
  取消都發生在量測開始前1.8–2.2 s；量測期間幀間隔最長只有17.7–18.8ms，之後沒有新卡頓。
- 可見P50為47.4／38.3／46.5ms，三輪都在50ms以內，但兩輪餘裕只有2.6–3.5ms。
  由trace重算移動方reseed後generated→executed P50為38.6／29.6／38.4ms，
  可見−該值穩定在8.1–8.8ms。smoke-2的38.3ms來自reseed後幸運抽到的相位，不是A1的功勞。
  已對齊角色（smoke-1／2 join）為28.1／32.3ms，推估移動方對齊時可見約37–41ms。
- RTT相依（CPU模擬，實機幀序列重播；守門v2與v3行為逐位元相同）：RTT 0對齊比例0–44%；
  RTT 20／40約98–100%，但有reseed的重播都是靠Starvation移動重設開新epoch重量（smoke-2／3每次一個），
  代價是重設與Held；smoke-1的53ms在RTT 20／40下不觸發reseed。epoch中途卡頓同理：60 FPS於3 s／10 s
  各一次53ms，對齊14／100／100%（RTT 0／20／40），RTT 20的100%是18次跑共19次Starvation重設換來。
- 真實產品是從大廳點擊加入（沒有auto-join），視窗早已穩定，開局卡頓多半落在大廳，**很可能不具代表性**。
  但若來源是「首次渲染對手模型」，實戰中對手第一次出現時也會發生，屬於epoch中途卡頓。
- 不受影響：正確性（Actual／Held／HP／彈藥／生命週期）；取消本身只是回到未對齊相位，不增加Held或重設
  （A1對偶發掉幀的敏感另見[03](./03-a1-missed-frame-starvation.md)）。wire、門檻、lead、60Hz、插值不變；
  低於約54.5 FPS本來就不對齊（見[01](./01-a1-low-fps-regression.md)）；另一位玩家的對齊各自獨立；
  沒有丟時間卡頓時不會取消，60 FPS以上穩定遊玩與`d74830c`的A1逐位元相同（模擬）。

## 如何復現

### A. 實機GUI冒煙（report-only，不計次、不作為驗收證據）

- 平台：本機Intel MacBook Pro 2019（i9-9980HK，x86_64）、macOS 26.7.1、Xcode 26.6＋Metal toolchain
  （`xcodebuild -downloadComponent MetalToolchain`）、cmake 4.4.3、ninja 1.13.2、Go 1.27.1、Python 3.14。
  CI的macOS是arm64／Xcode 16.4，工具鏈不同；Linux／Windows未觀察。
- 機器須閒置：停止其他GUI輪、模擬、build；一次只跑一輪；接AC電源，不闔蓋、不鎖螢幕、
  不遮蔽或拖曳兩個probe視窗。卡頓是機率性的：本機3／3輪的移動方都被取消，不保證每輪重現。

```bash
cmake --preset test -DGYO_APPS=object_fps_pvp -DGYO_TOOLS=
cmake --build --preset test --parallel 6
cmake --build --preset test --target gyo_object_fps_pvp-gateway
PVP_SMOKE="build/target/_build/test/logs/pvp-v5-fix02-smoke-$(date +%Y%m%d-%H%M%S)"
python3 build/acceptance/object_fps_pvp/run_timing.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output "$PVP_SMOKE" \
  --gui --short --report-only --rounds 1 --duration 16 --events 20 --fps 60
```

讀法：
- `$PVP_SMOKE/summary.md`的`Round 1 start phase:`行，create是移動方。重現時為
  `create measured epoch 1/life 1 cancelled_by_reseed: … cancelled by a stall reseed (reported by the product
  from frame N), X s before measurement began; during measurement: cancelled_by_reseed …;
  shift applied 0.000 s, unaligned 16.000 s`。未重現時為`shift_armed`且`shift applied 16.000 s`。
- `Round 1 movement:`行列出量測期間的Host重設數與幀間隔（`>1.1 tick`、`>=2 ticks`）；
  `Round 1 window:`須無視窗干擾（見[06](./06-gui-window-interference-and-rerun-rule.md)）。
- 逐角色紀錄：`$PVP_SMOKE/round-1/{create,join}-start-phase.json`（狀態意義見[07](./07-start-phase-diagnostics.md)）。
- 找卡頓與reseed：`runtime_gap`的`frame_seconds`>50ms且`dropped_seconds`>0，之後出現
  `seeded_neutral`為真、序號>2的`generated`（epoch開始的中立lead是1、2）即為stall reseed。

```bash
python3 - "$PVP_SMOKE/round-1/create-commands.jsonl" <<'EOF'
import json, sys
t0 = None
for line in open(sys.argv[1]):
    e = json.loads(line); t = e.get("time_ns")
    if t is None: continue
    t0 = t0 or t
    if e["kind"] == "runtime_gap" or (e["kind"] == "generated" and e["seeded_neutral"] and e["sequence"] > 2):
        print(f'{(t - t0) / 1e9:7.3f}s {e["kind"]:11} seq={e["sequence"]} '
              f'frame={e["frame_seconds"] * 1e3:.1f}ms dropped={e["dropped_seconds"] * 1e3:.1f}ms')
EOF
```

`sequence=0`的`runtime_gap`是應用層≥100ms的長幀紀錄（`PvpApplication::Update`），可能在播種前、不影響預測。

### B. 實機幀序列重播（CPU模擬，不需GUI）

評估用暫存模擬器，未進產品／CTest；保存的二進位是x86_64 macOS，以守門v2產品原始碼建置
（v3只多診斷，行為相同）。重建腳本寫死評估用worktree與暫存路徑、不可攜，原始碼見`eval-option2.diff` Part B。

```bash
cd build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/judge/o2
UNALIGNED=1 VARIANT=U REPLAY_DIR=$PWD nice -n 19 ./bin/sim_V2 replay      # 未對齊
VARIANT=V2 REPLAY_DIR=$PWD nice -n 19 ./bin/sim_V2 replay                 # 現行A1＋守門
WORKER_ONCE=1 VARIANT=O2W REPLAY_DIR=$PWD nice -n 19 ./bin/sim_O2 replay  # 方案O2W原型
```

- 每次約8 s。輸出列`ROW|replay replay_<冒煙>_<角色>.txt off <偏移>|<變體>|<RTT>|…`，
  看`applied`（2–18 s窗口內套用調整量的幀比例）、`reseeds`、`rS`（Starvation重設）、`p50`／`p95`。
- `off 0`是實際開局；`replay_2_create`在`off 0`另注入260.4ms＋97.2ms。幀序列為
  `replay_{1,2,3}_{create,join}.txt`（微秒，呈現間隔）。結果應與`out/{U,V2,O2W}_replay.txt`的非`DIG`行一致
  （2026-10-02已重跑V2確認一致）。smoke-1以呈現間隔重建時比實際溫和（只有10／18次reseed）；
  以Advance重建（`judge/o3`）則18／18都reseed，較接近實機。
- 方案3的模擬結果保存在`judge/o3/out/`；其模擬器從寫死的暫存路徑讀幀序列，暫存清除後無法重跑。

### C. 診斷部分的單元測試（已解決部分）

```bash
build/target/_build/test/tests/object_fps_pvp/gyo_object_fps_pvp_tests -tc="PvP a stall reseed*"
build/target/_build/test/tests/object_fps_pvp/gyo_object_fps_pvp_start_phase_record_tests -tc="*reseed*"
cd build/acceptance/object_fps_pvp && python3 -m unittest -k reseed \
  test_presentation_evidence test_run_timing test_gameplay_evidence
```

## 解決方案

### 已完成：取消可觀測（守門v3，工作分支）

- v1／v2時reseed會靜默清掉調整量，觀測值卻保留舊shift：smoke-1報`shift_armed`，實際已取消，
  證據與事實相反。
- v3新增`StartPhaseSkip::CancelledByReseed`（`LocalPlayerPrediction.hpp`）。`SeedLead`在相位待決或已決定時
  清除`startPhaseShiftSeconds`並設此原因；已取得的w保留；HostLate的epoch維持HostLate；
  `Reset()`清除。只改觀測，行為與v2逐位元相同。
- 驗收器：`start_phase_record.hpp`將其映射為`cancelled_by_reseed`，對該epoch／life是最終狀態；
  `start_phase_evidence.py`把量測窗口拆成`shift_applied_seconds`／`unaligned_seconds`。
  舊產品沒有此原因時，由Client trace推導（`seeded_neutral`且序號>`INITIAL_COMMAND_LEAD`＝2），
  標`cancelled_by_reseed.source client_trace_fallback`並保留原記錄狀態。細節見[07](./07-start-phase-diagnostics.md)。

### 未完成：取消後恢復對齊（只評估，未實作）

| | 3c 驗收器預熱 | 3b 產品出生後暫停bootstrap | O2W reseed後重新量測 |
|---|---|---|---|
| 做法 | 兩角色先在大廳預熱再加入：圖形就緒後≥1 s且最近0.5 s無>2.5 Tick幀，上限3 s（逾時照加入並記錄擬議欄位`warmup_not_quiet`；未實作）；`SDL_RaiseWindow`移入預熱 | 冷加入後等顯示穩定才播種（原型：連續16幀≤2.5 Tick，上限1 s；建議改為0.3–0.5 s的時間條件） | reseed後Client把新相位的首個真實步宣告為anchor，傳輸worker只送一次；`MatchRuntimeHost`計時並隨Snapshot送出；Client走同一套守門決定 |
| 開局取消 | 預期可，但前提未驗證（模擬把卡頓放在行程時間，0次reseed是建模結果） | 可：實機重播reseed 72／72→0／72，對齊0→99.8%，P50／P95 31.5／44.4→29.9／37.8ms（RTT 0） | 可：重播對齊smoke-2 create 2→100、smoke-1 create 44→100、smoke-3 create 0→98.5、join 0→97.9%（RTT 0）；reseed後約50ms重新對齊 |
| 中途取消 | 不可 | 不可 | RTT約33ms以下可；RTT≥40ms時anchor多半晚於其Tick，仍靠Starvation重設 |
| 契約 | 無 | 無wire；protocol-v5 §1的bootstrap文字需改並核准 | +3個可選欄位：`PlayerInput.phase_anchor_sequence`、`PlayerState.phase_anchor_sequence`／`phase_anchor_wait_us`；新概念phase anchor；傳輸層「只宣告一次」規則 |
| 體驗 | 產品無影響；每輪多1–3 s大廳時間 | 冷加入後0.3–0.85 s（上限1 s）不能移動，但可見、可被射擊 | 無凍結；相位可提前到約−37ms（已核准A1為≥−4ms） |
| 工作量 | 0.5–1天＋1–2輪診斷GUI | 2.5–3天（27個測例fixture、2個重寫） | 5–7天＋2–3輪GUI |
| 主要風險 | 可能無效；改變驗收情境（不再開局即加入），須同時寫明產品限制並保留`cancelled_by_reseed`顯示 | 真實大廳流程不會觸發，卻付出凍結代價並增加大廳與預測的耦合 | 5個傳輸環節任一掉欄位就靜默退回守門；傳輸層開始理解預測計時語意；卡頓風暴時reseed與Held增加（45ms／400ms：66 vs 18次、Held 0.69 vs 0.25%） |

其他評估過的變體：
- **O2P規則**（O2W的wire＋決策規則）：不採用負的正規化等待，調整範圍與已核准A1相同；
  Host／IpcHost／Gateway遇無效宣告時剝除宣告，不拒絕整個窗口。本機重播P50 28.2–29.7ms
  （O2W 30.2–30.7）、P95約38.3相同，套用71–85%（未套用的本來就是較低延遲的相位）；
  RTT 20的Starvation重設次數與守門相同（7,410 vs 7,381），不改既有恢復路徑。
- **3a 延後首次發布**：否決。每次冷加入首窗口累積12個命令、約+183ms佇列，約0.5 s後Backlog重設，
  也違反bootstrap契約本意。
- **O2H Host自行偵測**（免Client→Host欄位，仍需snapshot欄位）：否決。誤觸發、紀錄覆寫競態
  （丟3個標記snapshot、RTT 20：Starvation 7 vs O2W 2）、遺失reseed後首窗口時調整量少1 Tick
  （延遲多約16.7ms；P50 47.4 vs 31.2ms）。
- 只讓Host回報已執行命令的等待：遺失時少1 Tick、漏snapshot就量不到，仍須改Gateway，未量測。
- Client沿用reseed前的相位：違反「不假設Client與權威時鐘關係」原則，且要改lead 2政策。
- 取消後讓驗收器重新加入：等於操弄量測，不採用。

評審建議（若恢復時參考，**未核准、未排程**）：
1. 第一階段3c，在閒置機器跑1–2輪不計次診斷GUI。移動方量測epoch為`shift_armed`且加入後沒有stall reseed，
   才進入第03批計次輪（規則見[06](./06-gui-window-interference-and-rerun-rule.md)）。
2. 加入後仍有reseed（卡頓跟著進世界或同場出現），或觀測到epoch中途卡頓時，才另案提O2W的wire＋O2P規則，
   並以契約變更與Architecture Delta（新概念phase anchor、傳輸層計時規則）送審。
3. 不建議3b；3a與O2H否決。負相位提前與「reseed後也維持4ms餘裕」是另一個行為變更，須單獨提案。

### 使用者決定（2026-10-02）

MVP／PvP技術驗證：不做3c、不跑計次輪，本問題列為未解決（暫緩），先處理其他問題。
第03批結案驗收（計次GUI可見延遲短測＋25案矩陣）因此未執行，第03批未結案；第04批暫停。

### 方向變更（2026-10-02）

使用者要求先處理本問題並選擇O2W＋O2P；草案送審時改為先討論根因。漂移模擬（[08](./08-a1-clock-drift.md)）
顯示「每epoch量一次」的開環設計在長局本身就會失準：無卡頓時Client快20ppm、RTT 20，延遲P50約第10分鐘
超過50ms且沒有任何重設修正；這種情境reseed為0次，O2W不會啟動。因此不再推薦O2W／3c，
改為評估閉環的持續相位追蹤，以同一個回饋處理epoch開始、reseed與漂移。上表的評估保留作為歷史紀錄。
使用者決定不做兩台實機的漂移實測（邏輯缺陷不因實測數值而消失），直接修正。

### 後續候選：CS式開局準備期（未排程、無承諾）

開局／回合開始時全員凍結且無敵並倒數；啟動相位量測與首幀卡頓都落在其中，因所有人同時凍結而公平
（不同於3b只凍結單一玩家且可被射擊）。需Match回合狀態、無敵規則、HUD倒數與契約變更，須另立計畫。
它只涵蓋開局／回合開始，不處理回合內重生的新epoch或epoch中途卡頓。

## 驗證

- 診斷：`gyo_object_fps_pvp_tests`的「PvP a stall reseed cancels an armed or withdrawn start phase without
  moving the phase」「PvP a stall reseed inside the frame window cancels the pending start phase and the next
  epoch starts afresh」；`gyo_object_fps_pvp_start_phase_record_tests`的reseed 2例；Python `-k reseed` 4項。
  2026-10-02上述篩選重跑全部通過。整體：C++ 133 cases／1,418,939 assertions、CTest `-L pvp` 16／16、Python 165項。
- 實機（report-only，不計次、不作為驗收證據）：smoke-3（守門v3）兩角色皆由產品回報`CancelledByReseed`；
  smoke-1／2（舊產品）以Client trace重分析，create皆為`cancelled_by_reseed`（seq42／seq7）。
- 方案只有原型與模擬：沒有任何恢復方案在實機GUI或真網路驗證過；卡頓來源與真實大廳流程是否觸發也未驗證。
- 持續相位追蹤（2026-10-02，PR #11）實機report-only GUI冒煙1輪（不計次、不作為驗收證據）：create方啟動時3次、
  join方1次stall reseed，兩方都在量測開始前1.8–2.1秒重新決定相位，量測期間全程tracking、0次修正、0次移動重設；
  可見P50／P95 36.754／38.811ms。只是一輪，不是統計證明。證據：`build/target/_build/test/logs/pvp-v5-ct-gui-smoke-20261002-192738/`。
- 證據（git忽略，只在本機；`build/`日後可能移出工作區，須保留）：
  `build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/`
  - `integration-smoke-{1,2,3}/`：summary與`round-1/{create,join}-{commands.jsonl,start-phase.json,presentation.csv}`
  - `reanalysis-integration-smoke-{1,2}/`：以新驗收器重分析的smoke-1／2
  - `opt2b/final_tables.txt`（Table 1／2）、`judge/o2`、`judge/o3`：模擬輸出與二進位
  - `eval-option2.diff`、`eval-option3.diff`：O2／3a／3b原型與模擬原始碼（從未commit）
  - 原始建置／冒煙紀錄：`pvp-v5-batch03-integration-20261001-234746`（smoke-1，守門v1）、
    `-integration2-20261002-004224`（smoke-2，v2）、`-integration3-20261002-015053`（smoke-3，v3）

## 殘留風險與後續

- 若第03批恢復結案，計次輪的移動方很可能再被取消，可見P50會落在未對齊抽籤上（三輪中兩輪只剩約3ms餘裕）。
  只能如實記錄，不改門檻、不以重跑挑分數；`cancelled_by_reseed`的輪不能當A1生效的證據。
- smoke-3的幀節奏約1.056 Tick，貼近1.06 Tick的恢復界線；不論採哪個方案，卡頓後約12%時間可能仍停在撤回狀態。
- 首窗口後、決定前若有丟時間但沒有reseed的covered gap，程式同樣放棄待決量測但不設skip原因，
  紀錄會顯示`not_armed_or_invalidated`；冒煙中未觀察到。
- 需要的觀測：區分卡頓來源（行程啟動／首次世界或GPU使用／首次渲染對手模型），以及從大廳點擊加入時是否仍發生。
- 經驗：開局即加入的GUI probe量到的是啟動假象而非穩定遊玩；診斷必須跟著每次狀態變更（撤回／恢復／取消），
  否則證據會與事實相反（smoke-1）。

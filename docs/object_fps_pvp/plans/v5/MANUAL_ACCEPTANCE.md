# PvP v5 手動啟動與完整驗收

更新：2026-10-03。Owner：`object_fps_pvp`。契約：[Protocol v5](../../protocol-v5.zh-Hant.md)。回報表：[驗收狀態](ACCEPTANCE_STATUS.md)。
v4的[手動指南](../v4/MANUAL_ACCEPTANCE.md)原樣保留；它的「零血仍可操作、無限彈藥」斷言不適用於v5。

本指南跨平台中立：所有量化命令都是Python runner加上明確路徑，同一份命令在macOS、Windows、Linux使用；
平台差異只列在[平台註記](#平台註記)。驗證分三層（第04批定案）：

| 層 | 內容 | 平台 |
|---|---|---|
| L1 功能與時序 | GUI probe以SDL注入輸入，runner與分析器判定 | 三平台同一命令 |
| L2 GPU圖像 | probe的capture走SDL GPU readback（Metal／Vulkan／D3D12） | 三平台同一命令 |
| L3 原生操作 | [原生操作清單](#l3-原生操作清單人工三平台共用)由人工操作並記錄 | 各平台各自執行 |

一個平台通過不代表其他平台。每份probe報告自報平台指紋（OS、SDL視訊驅動、GPU驅動、輸入方式）；
未在某平台執行的項目，在回報表標「未執行」。

## 共同前提

1. **同一份來源與產物。**從repository根目錄、同一套建置產物執行；量測開始後不再編譯或更新二進位。
   以`test` preset建置正式產品與probe（第一次建置會從原始碼編譯shader工具，耗時較長）：

   ```bash
   cmake --preset test
   cmake --build --preset test
   ```

2. **一次一項。**量測期間不同時建置、跑其他驗收或大型工作；GUI量測期間不碰滑鼠鍵盤、不移動或遮蔽兩個probe視窗。
3. **全新輸出目錄。**每個runner都拒絕已存在的`--output`；舊證據不覆寫。
4. **runner只管理自己啟動的程序**：在私有localhost連接埠建立／清理自己的Match、Gateway、probe，
   不操作你另外開啟的遊戲或服務。量測後確認沒有殘留程序。
5. **指紋。**每次跑次保存`git rev-parse HEAD`、`git status --short`與產物SHA-256（runner另在輸出寫入`artifacts.json`或`run-manifest.json`）。

以下命令使用這些路徑（相對repository根目錄；Windows另見平台註記）：

```bash
PVP_BIN=build/target/object_fps_pvp/bin
PVP_MATCH=$PVP_BIN/gyo_object_fps_pvp-match
PVP_CLIENT=$PVP_BIN/gyo_object_fps_pvp
PVP_GATEWAY=build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway
PVP_ARENA=$PVP_BIN/assets/object_fps_pvp/pvp_arena.json
PVP_ARENA_ROOT=$PVP_BIN/assets/object_fps_pvp
PVP_PROBES=build/target/_build/test/acceptance/object_fps_pvp
PVP_RUNNERS=build/acceptance/object_fps_pvp
PVP_LOGS=build/target/_build/test/logs
```

## 正式Client試玩：四個終端

試玩用正式產品，不是probe。先建立跑次目錄與指紋：

```bash
PVP_RUN=$PVP_LOGS/pvp-v5-manual-$(date +%Y%m%d-%H%M%S)
mkdir "$PVP_RUN"
git rev-parse HEAD > "$PVP_RUN/head.txt"
git status --short > "$PVP_RUN/worktree-status.txt"
python3 -c "import hashlib,sys;[print(hashlib.sha256(open(p,'rb').read()).hexdigest(),p) for p in sys.argv[1:]]" \
  "$PVP_CLIENT" "$PVP_MATCH" "$PVP_GATEWAY" "$PVP_ARENA" > "$PVP_RUN/artifacts.sha256"
```

每個終端先設定同一組路徑變數與`PVP_RUN`，再分別執行：

```bash
# 終端1：Match（match.log出現ready後再開Gateway）
"$PVP_MATCH" --arena "$PVP_ARENA" --listen 127.0.0.1:27016 \
  --movement-trace "$PVP_RUN/match-commands.jsonl" > "$PVP_RUN/match.log" 2>&1
# 終端2：Gateway
"$PVP_GATEWAY" -runtime 127.0.0.1:27016 -http 127.0.0.1:8080 -udp 127.0.0.1:27015 \
  -advertise-ip 127.0.0.1 > "$PVP_RUN/gateway.log" 2>&1
# 終端3：Client A，在Lobby建房
"$PVP_CLIENT" --gateway 127.0.0.1:8080 --gpu-driver auto \
  --movement-trace "$PVP_RUN/create-commands.jsonl" > "$PVP_RUN/create.log" 2>&1
# 終端4：Client B，在Lobby加入同一房間
"$PVP_CLIENT" --gateway 127.0.0.1:8080 --gpu-driver auto \
  --movement-trace "$PVP_RUN/join-commands.jsonl" > "$PVP_RUN/join.log" 2>&1
```

`--gpu-driver`可為`auto|metal|vulkan|d3d12`；`auto`選平台預設後端。結束順序：先正常關閉兩個Client視窗，
再在Gateway、Match終端按Ctrl+C。不要強制kill Client，才能寫完`trace_end`；強制中止、缺`trace_end`、
`dropped > 0`都要回報，不能視為完整資料。這些正式日誌沒有逐槍裁決帳本，不能單靠它們證明零重複扣血或延遲分位數；
量化證據由後面的runner產生。

## L3 原生操作清單（人工；三平台共用）

每項記錄：平台、操作者、時間、結果（通過／失敗／無法判定）與備註；失敗附截圖或錄影。
切換視窗必然造成其中一個Client失焦；這類操作與乾淨效能跑次分開。

| ID | 操作 | 預期（v5） |
|---|---|---|
| 1 | 進入大廳、加入對局；滑鼠捕捉、Esc釋放、重新捕捉 | 捕捉那次不射擊；釋放後不產生幽靈輸入 |
| 2 | WASD移動與滑鼠視角；Space單按、長按、半空中再按 | 長按不連跳；半空不二段跳 |
| 3 | 左鍵單發、快速連點、按住；打空彈匣後再點 | 10 Tick節奏；按住不連發；空彈匣只顯示提示，不送出、不播射擊動畫 |
| 4 | R換彈；換彈中射擊、換彈中再按R；移動中換彈、跳躍中射擊 | 換彈1.5秒；換彈中射擊與重複R被擋；換彈完成彈匣12 |
| 5 | 被擊殺 | 死亡期間操作被抑制、顯示倒數；3秒後重生，HP 100、彈匣12，可再操作 |
| 6 | Tab／切換視窗再回來；拖動標題列；縮放（含800×600）；關閉視窗後重新加入 | 釋放輸入、無幽靈射擊或持續移動；重新加入為新身分，無舊玩家殘留 |
| 7 | 對手畫面 | 遠端人物步態（Walk／Jog依速度混合）、持槍上身、射擊、換彈、起跳／空中／落地、死亡與新生命 |
| 8 | HUD | 彈匣、HP、換彈與死亡倒數；`CONNECTION POOR`警告（如可製造）與被移出後大廳的原因 |

已知限制（不是本清單的失敗）：遠端上半身沒有俯仰瞄準、沒有受擊反應；macOS拖動或縮放視窗時Engine呈現約1秒阻塞，
可能短暫出現`CONNECTION POOR`。這些延到[v6](../v6/HANDOFF.md)。

## L1／L2 短測

每個命令一次，輸出寫進新目錄；摘要印在stdout，詳細結果在輸出目錄的`result.json`／`round.json`等。

```bash
# 第一人稱與遠端動作（SDL注入，30／60／144 FPS）與GPU capture；--case可只跑其中一案
python3 $PVP_RUNNERS/run_action_short.py --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --gui-probe $PVP_PROBES/gyo_object_fps_pvp_gui_probe --arena "$PVP_ARENA" --arena-root "$PVP_ARENA_ROOT" \
  --gpu-driver auto --output $PVP_LOGS/pvp-v5-action-short-$(date +%Y%m%d-%H%M%S)

# 25案真網路玩法矩陣（headless；含RTT／丟包／阻塞／跨生命；約12分鐘）
python3 $PVP_RUNNERS/run_gameplay.py --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_action_probe --arena "$PVP_ARENA" \
  --output $PVP_LOGS/pvp-v5-matrix-$(date +%Y%m%d-%H%M%S)

# 雙GUI整合短測：移動延遲＋v5戰鬥排程（擊殺、重生、換彈），16秒
python3 $PVP_RUNNERS/run_timing.py --gui --combat --short --duration 16 --events 20 --fps 60 \
  --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" --probe $PVP_PROBES/gyo_object_fps_pvp_gui_probe \
  --arena "$PVP_ARENA" --output $PVP_LOGS/pvp-v5-gui-short-$(date +%Y%m%d-%H%M%S)

# 長測的短模式（2循環＝32秒）
python3 $PVP_RUNNERS/run_gameplay_soak.py --cycles 2 --fps 60 --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_action_probe --arena "$PVP_ARENA" \
  --output $PVP_LOGS/pvp-v5-soak-short-$(date +%Y%m%d-%H%M%S)
```

退出碼0為通過，1為失敗或不完整。GUI runner另判定視窗干擾：視窗受干擾的輪標`invalid_window_disturbed`，
即使數值門檻通過也不算通過。

## 量化門檻

| 指標 | 門檻與計算範圍 |
|---|---|
| 移動產生→首次送出 | 乾淨同機P95 ≤22 ms |
| 移動產生→Actual | P50 ≤50 ms、P95 ≤66.7 ms；原始命令Actual ≥99%（生命週期取消的舊生命命令另列，不算Actual也不算遺失） |
| 跨視窗可見交越 | 每輪P50 ≤50 ms、P95 ≤80 ms；全部預排事件納入分母，未配對按+∞ |
| 射擊即時回饋 | 下一個成功`Presented`幀；不等待裁決 |
| 射擊產生→Match裁決／→Client取得裁決 | 乾淨同機P95 ≤100 ms／≤150 ms |
| 動作正確性 | 每個預先宣告的動作有唯一且與計畫相同的判定；每ActionId至多一次效果 |
| 逐生命狀態 | 每幀快照與HUD的HP、彈匣、換彈、最後射擊等於該生命唯一裁決的重算結果 |
| 生命 | 死亡等待180 Tick；重生建立新epoch，Match的LifeRespawn重設與觀測到的死亡／新生命逐一相同 |
| 重設與干擾 | 零非預期重設；重生首幀的時間重設只有與LifeRespawn精確配對時不算干擾 |
| 60 Hz產量 | 每位玩家±2步，每次經核對的重生另加3步（新相位1步＋首次修正上限2 Tick） |
| 長測不漂移 | 移動門檻同樣套用到每10個循環（160秒）的窗口 |
| 容量 | 動作窗口≤32、批次≤8、UDP≤1200 bytes；Gateway零限流丟棄、每秒窗口≤120包 |

同機只比較單調時鐘，不稱input-to-photon。**同機三角色共用一個時鐘，測不到Client／Host時鐘頻率差**；
兩台實體機器的漂移實測是另一個須另行授權的項目。

## 完整驗收（需明確授權）

完整驗收約1.5小時，機器須閒置。執行前在輸出目錄寫下**事前宣告**：來源commit、產物指紋、總輪數與補跑規則。

```bash
PVP_ACCEPT=$PVP_LOGS/pvp-v5-acceptance-$(date +%Y%m%d)
mkdir "$PVP_ACCEPT"   # 先寫入宣告檔（declaration.md）與指紋，再開始第一項
```
兩組長測與GUI各輪**依序**執行，不並行、不自動重跑。

### GUI三輪（每輪120秒、200個移動事件、v5戰鬥排程）

每輪是一次獨立命令，`N`為1、2、3：

```bash
python3 $PVP_RUNNERS/run_timing.py --gui --combat --duration 120 --events 200 --fps 60 \
  --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" --probe $PVP_PROBES/gyo_object_fps_pvp_gui_probe \
  --arena "$PVP_ARENA" --output "$PVP_ACCEPT/gui-round-N"
```

戰鬥排程每0.8秒一格、17格一循環：4發擊殺→死亡等待中打屍體→4發擊殺重生的新生命→空彈匣點擊→R換彈→換彈中點擊。
120秒為150格、116個送出動作、18次死亡。

補跑規則（[fix/06](fix/06-gui-window-interference-and-rerun-rule.md)）：
1. 只有視窗證據判為干擾（`invalid_window_*`）的輪可補跑，輸出另命名（如`gui-round-3-rerun`），原輪保留。
2. 是否補跑只依視窗證據，不依延遲分數。
3. 每個受干擾輪最多補跑一次；補跑再受干擾即停止，交由使用者決定。
4. 數值門檻失敗的輪不補跑，保留並先分析。

### Headless兩組長測（60 Hz與144 Hz，各113循環＝1808秒）

```bash
python3 $PVP_RUNNERS/run_gameplay_soak.py --soak --fps 60 --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_action_probe --arena "$PVP_ARENA" --output "$PVP_ACCEPT/soak60"
python3 $PVP_RUNNERS/run_gameplay_soak.py --soak --fps 144 --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_action_probe --arena "$PVP_ARENA" --output "$PVP_ACCEPT/soak144"
```

`--soak`是顯式長測參數，固定113循環（涵蓋1800秒的最小整數循環），只允許60／144 Hz。
每循環重複第03批的16秒玩法計畫：B每循環死亡一次、A從不死亡；B的生命世代每循環+1。
這是headless網路／預測／玩法長測，**不是GUI長測**。144 Hz約產生1.2 GB證據，分析約1分鐘。

## 失敗與回報

- 保留首份失敗與全部原始資料，不覆寫、不無限重跑；先看摘要與異常片段，分析後再決定補驗範圍。
- 版本改變（來源或產物指紋不同）後，舊跑次不能與新跑次混算。
- 回傳：輸出目錄、stdout摘要、`report.txt`／`result.json`／`round.json`、事前宣告，以及[回報表](ACCEPTANCE_STATUS.md)填好的欄位。
  `未測`、`受干擾`、`缺資料`都不是通過。

## 平台註記

| 平台 | 差異 |
|---|---|
| macOS | GPU後端Metal（`auto`即選Metal）；Metal的GPU readback超過100 ms，capture模式不做計時判定。拖動或縮放視窗會讓呈現阻塞約1秒（v6已知問題） |
| Windows | 執行檔加`.exe`；用`python`取代`python3`；PowerShell設定變數用`$env:`或`$PVP_BIN = "..."`並以`&`執行程式；`date`改用`Get-Date -Format yyyyMMdd-HHmmss`。GPU後端D3D12或Vulkan |
| Linux | GPU後端Vulkan。Wayland下兩個probe視窗無法指定位置，可能重疊；runner會警告，被遮蔽的輪會判`invalid_window_disturbed`。X11可另用既有XTest runner（`run_native_window.py`）作為L3的可選自動化 |

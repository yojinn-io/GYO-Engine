# PvP v6 手動啟動與驗收

更新：2026-10-07。Owner：`object_fps_pvp`。契約：[v6 契約（pv6）](../../protocol-v6.zh-Hant.md)。回報表：[驗收狀態](ACCEPTANCE_STATUS.md)。
v5 的[手動指南](../v5/MANUAL_ACCEPTANCE.md)原樣保留；v6 的差異是房間上限 4 人、遠端俯仰瞄準、受擊反應、本機射擊冷卻閘與 4 人驗收工具。

本指南跨平台中立：所有量化命令都是 Python runner 加上明確路徑，同一份命令在 macOS、Windows、Linux 使用；平台差異只列在[平台註記](#平台註記)。驗證分三層：

| 層 | 內容 | 平台 |
|---|---|---|
| L1 功能與時序 | GUI probe 以 SDL 注入輸入，由 runner 與分析器判定 | 三平台同一命令 |
| L2 GPU 圖像 | probe 的 capture 走 SDL GPU readback（Metal／Vulkan／D3D12） | 三平台同一命令 |
| L3 原生操作 | [原生操作清單](#l3-原生操作清單人工三平台共用)由人工操作並記錄 | 各平台各自執行 |

一個平台通過不代表其他平台。每份 probe 報告自報平台指紋（OS、SDL 視訊驅動、GPU 驅動、輸入方式）；未在某平台執行的項目，在回報表標「未執行」。

## 共同前提

1. **同一份來源與產物。** 從 repository 根目錄、同一套建置產物執行；量測開始後不再編譯或更新二進位。以 `test` preset 建置正式產品與 probe，Gateway 另外建置：

   ```bash
   cmake --preset test
   cmake --build --preset test
   cmake --build build/target/_build/test --target gyo_object_fps_pvp-gateway
   ```

2. **一次一項。** 量測期間不同時建置、不跑其他驗收或大型工作；GUI 量測期間不碰滑鼠鍵盤、不移動或遮蔽 probe 視窗。
3. **全新輸出目錄。** 每個 runner 都拒絕已存在的 `--output`；舊證據不覆寫。
4. **runner 只管理自己啟動的程序。** 它們在私有 localhost 連接埠建立並清理自己的 Match、Gateway、probe 與 bot，不操作你另外開啟的遊戲或服務。量測後確認沒有殘留程序。
5. **指紋。** 每個跑次保存 `git rev-parse HEAD`、`git status --short` 與產物 SHA-256（runner 另在輸出寫入 `artifacts.json` 或 `run-manifest.json`）。

以下命令使用這些路徑（相對 repository 根目錄；Windows 另見平台註記）：

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

## 正式 Client 試玩

試玩用正式產品，不是 probe。先建立跑次目錄與指紋：

```bash
PVP_RUN=$PVP_LOGS/pvp-v6-manual-$(date +%Y%m%d-%H%M%S)
mkdir "$PVP_RUN"
git rev-parse HEAD > "$PVP_RUN/head.txt"
git status --short > "$PVP_RUN/worktree-status.txt"
python3 -c "import hashlib,sys;[print(hashlib.sha256(open(p,'rb').read()).hexdigest(),p) for p in sys.argv[1:]]" \
  "$PVP_CLIENT" "$PVP_MATCH" "$PVP_GATEWAY" "$PVP_ARENA" > "$PVP_RUN/artifacts.sha256"
```

每個終端先設定同一組路徑變數與 `PVP_RUN`，再分別執行：

```bash
# 終端 1：Match（match.log 出現 ready 後再開 Gateway）
"$PVP_MATCH" --arena "$PVP_ARENA" --listen 127.0.0.1:27016 \
  --movement-trace "$PVP_RUN/match-commands.jsonl" > "$PVP_RUN/match.log" 2>&1
# 終端 2：Gateway
"$PVP_GATEWAY" -runtime 127.0.0.1:27016 -http 127.0.0.1:8080 -udp 127.0.0.1:27015 \
  -advertise-ip 127.0.0.1 > "$PVP_RUN/gateway.log" 2>&1
# 終端 3：Client A，在 Lobby 建房
"$PVP_CLIENT" --gateway 127.0.0.1:8080 --gpu-driver auto \
  --movement-trace "$PVP_RUN/create-commands.jsonl" > "$PVP_RUN/create.log" 2>&1
# 終端 4：Client B，在 Lobby 加入同一房間（最多 4 人）
"$PVP_CLIENT" --gateway 127.0.0.1:8080 --gpu-driver auto \
  --movement-trace "$PVP_RUN/join-commands.jsonl" > "$PVP_RUN/join.log" 2>&1
```

人數不夠時，可以加 headless bot 進房（會移動並朝最近的存活玩家開槍）：

```bash
# 終端 5：3 個 bot 加入已存在的房間，300 秒後離開
$PVP_PROBES/gyo_object_fps_pvp_quad_probe --gateway 127.0.0.1:8080 --arena "$PVP_ARENA" \
  --output "$PVP_RUN/bots" --clients 3 --create false --duration 300
```

`--gpu-driver` 可為 `auto|metal|vulkan|d3d12`；`auto` 選平台預設後端。

地圖由 Match 的 `--arena` 決定（第 17 批）：`pvp_arena.json`（`pvp_training_v1`）是測試與驗收用的地圖，`pvp_corners.json`（`pvp_corners_v1`）是發行地圖，也是未指定時的預設。Client 安裝兩張，加入時依 Match 自動選擇。三個角色都可以加 `--log`（Gateway 為 `-log`）寫出帶時間戳的日誌；Client 未指定時寫到執行檔旁的 `logs/`。跨機器的聯機測試見 [LAN_TEST](LAN_TEST.md)。

結束順序：先正常關閉 Client 視窗，再在 Gateway、Match 終端按 Ctrl+C。不要強制 kill Client，才能寫完 `trace_end`。強制中止、缺 `trace_end`、`dropped > 0` 都要回報，不能視為完整資料。

## L3 原生操作清單（人工；三平台共用）

每項記錄平台、操作者、時間、結果（通過／失敗／無法判定）與備註；失敗附截圖或錄影。切換視窗必然造成其中一個 Client 失焦，這類操作與乾淨的效能跑次分開。

| ID | 操作 | 預期（v6） |
|---|---|---|
| 1 | 進入大廳、加入對局；滑鼠擷取、Esc 釋放、再擷取 | 擷取的那一下不射擊；釋放後沒有幽靈輸入 |
| 2 | WASD 移動與滑鼠視角；Space 單按、長按、半空中再按 | 長按不連跳；半空不二段跳 |
| 3 | 左鍵單發、快速連點、按住；打空彈匣後再點 | 10 Tick 節奏；快速連點時本機射擊閘擋下過早的點擊，不出現「本機有動畫、權威拒絕」；空彈匣只顯示提示 |
| 4 | R 換彈；換彈中射擊、再按 R；移動中換彈、跳躍中射擊 | 換彈 1.5 秒；換彈中射擊與重複 R 被擋；完成後彈匣 12 |
| 5 | 被擊殺 | 死亡期間操作被抑制、顯示倒數，第一人稱手臂不顯示；3 秒後重生，HP 100、彈匣 12，可再操作 |
| 6 | Tab／切換視窗再回來；拖動標題列；縮放（含 800×600）；關閉視窗後重新加入 | 釋放輸入，沒有幽靈射擊或持續移動；重新加入是新身分，沒有舊玩家殘留 |
| 7 | 對手畫面 | 步態、持槍上身、俯仰瞄準（抬頭低頭時上半身跟著轉）、射擊、換彈、受擊反應、起跳／空中／落地、死亡與新生命 |
| 8 | 被擊中時 | 畫面閃紅、HP 變紅、指向攻擊者的方向弧線（轉身時跟著轉）、輕微鏡頭晃動；送出的視角不受晃動影響 |
| 9 | HUD | 彈匣、HP、換彈與死亡倒數；大廳人數 n／4；`CONNECTION POOR` 警告（如可製造）與被移出後大廳顯示的原因 |
| 10 | 4 人房（加 bot 或 4 個 Client） | 看得到 3 個遠端角色；擊殺與重生正常；第 5 人加入時顯示 `room_full` |

已知限制（不算失敗）：

- 遠端角色外觀相同；新出生點在兩端出生點之間的直線上，沒有重生保護；受擊方向只指向最後一位攻擊者。
- macOS 按下縮放角到開始拖動之間仍會停住（v7 分執行緒處理，D19）；這段時間可能短暫出現 `CONNECTION POOR`。
- 30 FPS 的相位餘裕缺口（D21）由 v7 處理。

## L1／L2 短測

每個命令跑一次，輸出寫進新目錄；摘要印在 stdout，詳細結果在輸出目錄的 `result.json`、`round.json` 等。退出碼 0 為通過，1 為失敗或不完整。

```bash
# 第一人稱與遠端動作（SDL 注入，30／60／144 FPS）與 GPU capture；--case 可只跑其中一案
python3 $PVP_RUNNERS/run_action_short.py --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --gui-probe $PVP_PROBES/gyo_object_fps_pvp_gui_probe --arena "$PVP_ARENA" --arena-root "$PVP_ARENA_ROOT" \
  --gpu-driver auto --output $PVP_LOGS/pvp-v6-action-short-$(date +%Y%m%d-%H%M%S)

# 遠端人物呈現（30／60／144 FPS 與 capture）
python3 $PVP_RUNNERS/run_player_short.py --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --gui-probe $PVP_PROBES/gyo_object_fps_pvp_gui_probe --arena "$PVP_ARENA" --arena-root "$PVP_ARENA_ROOT" \
  --gpu-driver auto --output $PVP_LOGS/pvp-v6-player-short-$(date +%Y%m%d-%H%M%S)

# 25 案真網路玩法矩陣（headless）；--case 一次跑一案，一案失敗不影響其他案（D24）
python3 $PVP_RUNNERS/run_gameplay.py --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_action_probe --arena "$PVP_ARENA" --case clean-60 \
  --output $PVP_LOGS/pvp-v6-matrix-clean-60-$(date +%Y%m%d-%H%M%S)

# 雙 GUI 整合短測：移動延遲＋戰鬥排程（擊殺、重生、換彈），16 秒
python3 $PVP_RUNNERS/run_timing.py --gui --combat --short --duration 16 --events 20 --fps 60 \
  --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" --probe $PVP_PROBES/gyo_object_fps_pvp_gui_probe \
  --arena "$PVP_ARENA" --output $PVP_LOGS/pvp-v6-gui-short-$(date +%Y%m%d-%H%M%S)

# 長測的短模式（2 循環＝32 秒）
python3 $PVP_RUNNERS/run_gameplay_soak.py --cycles 2 --fps 60 --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_action_probe --arena "$PVP_ARENA" \
  --output $PVP_LOGS/pvp-v6-soak-short-$(date +%Y%m%d-%H%M%S)
```

GUI runner 另外判定視窗干擾：受干擾的輪標 `invalid_window_disturbed`，即使數值門檻通過也不算通過。

### 4 人

```bash
# 4 個 headless Client 同房 16 秒；第 5 人須被 room_full 拒絕。clean-60 判定，clean-30 只記錄（D21）
python3 $PVP_RUNNERS/run_quad.py --case clean-60 --rounds 5 --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_quad_probe --arena "$PVP_ARENA" \
  --output $PVP_LOGS/pvp-v6-quad-clean-60-$(date +%Y%m%d-%H%M%S)

# 1 GUI＋3 bot
python3 $PVP_RUNNERS/run_quad.py --case gui-bots --match "$PVP_MATCH" --gateway "$PVP_GATEWAY" \
  --probe $PVP_PROBES/gyo_object_fps_pvp_quad_probe --gui-probe $PVP_PROBES/gyo_object_fps_pvp_gui_quad_probe \
  --arena-root "$PVP_ARENA_ROOT" --gpu-driver auto --arena "$PVP_ARENA" \
  --output $PVP_LOGS/pvp-v6-quad-gui-bots-$(date +%Y%m%d-%H%M%S)

# GUI 短測在有 bot 的房間執行（2 GUI＋2 個被動 bot）：action short、player short、run_timing --gui 都接受
#   --bots 2 --quad-probe $PVP_PROBES/gyo_object_fps_pvp_quad_probe
```

版本混用（`run_quad.py --case mixed`）需要上一個容量的 Match、Gateway、action probe 與 arena（`--base-*`），只在容量改變的批次使用。

## 量化門檻

| 指標 | 門檻與計算範圍 |
|---|---|
| 移動產生→首次送出 | 乾淨同機 P95 ≤22 ms |
| 移動產生→Actual | P50 ≤50 ms、P95 ≤66.7 ms；原始命令 Actual ≥99%（生命週期取消的舊生命命令另列，不算 Actual 也不算遺失） |
| 跨視窗可見交越 | 每輪 P50 ≤50 ms、P95 ≤80 ms；全部預排事件納入分母，未配對以 +∞ 計 |
| 射擊即時回饋 | 下一個成功 `Presented` 幀；不等待裁決 |
| 射擊產生→Match 裁決／→Client 取得裁決 | 乾淨同機 P95 ≤100 ms／≤150 ms |
| 動作正確性 | 每個預先宣告的動作有唯一且與計畫相同的判定；每個 ActionId 至多一次效果 |
| 逐生命狀態 | 每幀快照與 HUD 的 HP、彈匣、換彈、最後射擊，等於該生命唯一裁決的重算結果 |
| 生命 | 死亡等待 180 Tick；重生建立新 epoch；Match 的 LifeRespawn 重設與觀測到的死亡／新生命逐一相同 |
| 重設與干擾 | 零非預期重設；重生首幀的時間重設只有與 LifeRespawn 精確配對時不算干擾 |
| 60 Hz 產量 | 每位玩家 ±2 步，每次經核對的重生另加 3 步 |
| 長測不漂移 | 移動門檻同樣套用到每 10 個循環（160 秒）的窗口 |
| 容量 | 房間 4 人；動作窗口 ≤32、批次 ≤8、UDP ≤1200 bytes（滿員 Snapshot 1055 bytes）；Gateway 零限流丟棄、每秒窗口 ≤120 包 |
| 4 人 | 第 5 人 `room_full`；每個動作恰有一個裁決；拒絕只有 Dead／StaleLife；重生在空的出生點；各 Client 同一 Tick 的 Snapshot 相同 |

clean-30 依 D21／D24 照跑並記錄，不判定。同機只比較單調時鐘，不稱 input-to-photon。**同機三角色共用一個時鐘，測不到 Client 與 Host 的時鐘頻率差**；兩台實體機器的漂移實測是另一個需要另外授權的項目。

## 完整驗收（14b，需另外授權）

規模沿用 v5：GUI 三輪（各 120 秒、200 個移動事件、戰鬥排程）、headless 長測 60 Hz 與 144 Hz 各 113 循環（1808 秒），以及 4 人的 1 GUI＋3 bot 一輪（D23⑤）。執行前在輸出目錄寫下事前宣告：來源 commit、產物指紋、總輪數與補跑規則。各項依序執行，不並行、不自動重跑。命令與補跑規則同 v5 指南的「完整驗收」節（[fix/06](../v5/fix/06-gui-window-interference-and-rerun-rule.md)），輸出目錄改為 `pvp-v6-acceptance-*`。

## 失敗與回報

- 保留首份失敗與全部原始資料，不覆寫、不無限重跑；先看摘要與異常片段，分析後再決定補驗範圍。
- 版本改變（來源或產物指紋不同）後，舊跑次不能與新跑次混算。
- 回傳：輸出目錄、stdout 摘要、`report.txt`／`result.json`／`round.json`、事前宣告，以及[回報表](ACCEPTANCE_STATUS.md)填好的欄位。`未執行`、`受干擾`、`缺資料` 都不是通過。

## 平台註記

| 平台 | 差異 |
|---|---|
| macOS | GPU 後端是 Metal（`auto` 即選 Metal）；Metal 的 GPU readback 超過 100 ms，capture 模式不做計時判定。顯示器上限 120 Hz 時，144 FPS 的跑次依 D11③ 無效（不計次）|
| Windows | 執行檔加 `.exe`；用 `python` 取代 `python3`；PowerShell 用 `$env:` 或 `$PVP_BIN = "..."` 設定變數，以 `&` 執行程式；`date` 改用 `Get-Date -Format yyyyMMdd-HHmmss`。GPU 後端 D3D12 或 Vulkan。拖動標題列會進入 Win32 modal 迴圈，行為可能和 macOS 不同 |
| Linux | GPU 後端 Vulkan。Wayland 下兩個 probe 視窗無法指定位置，可能重疊；runner 會警告，被遮蔽的輪判 `invalid_window_disturbed`。X11 可另用 XTest runner（`run_native_window.py`，只支援 2 人房）作為 L3 的可選自動化 |

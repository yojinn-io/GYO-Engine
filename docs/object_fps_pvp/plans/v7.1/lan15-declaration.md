# lan15-windows 的事前宣告（第 2.3 版，2026-10-29 核准）

更新：2026-10-29。Owner：`object_fps_pvp`。規劃：[lan15-windows.md](lan15-windows.md)（D232～D234）。
第 1 版（`e5443624`）經對抗式檢查（本文件最後一節）改成本版；使用者對三項待決選了建議（M7：Mac 的斷線列為限界；M9：實體斷線＋ping 量長度；M3：判計數一致、比例只記錄）。
核准（D236）：使用者：「核准 lan15 宣告第 2 版（人數與 Q2 的補人方法定了再補、只核准那幾處），開始 lan15-tools。」人數（N、N_w）與第 5 節 Q2 的補人方法補上時，只核准那幾處；其餘內容核准後不改，跑完不放寬（AGENTS.md §14.1）。人數與日期等使用者（lan15-windows.md 第 1 項）。第 2.2 版（D238 核准：使用者：「核准 lan15 宣告 2.2（ping.txt 的 UTF-16 修正與新的 sha256、Gateway 與 quad probe 的版本核對）。」）。第 2.1 版（D237 核准：使用者：「核准 lan15 宣告第 2.1 版補上的部分（人數、Q2 的 quad probe、marks.txt 與資料夾、結束前停 3 秒、判定工具的鎖定），接著做 lan15-kit。」）：人數是 2～3 人，看當天情況（使用者：「人數2～3人，得看情況。」）。N（2 或 3）與 N_w（N−1）在 S1 開始前寫進 `marks.txt`，之後不改。

## 0. 依據的事實

- repo 裡沒有專門判定 LAN 場次的入口。能直接讀 LAN log 的只有 `network_statistics.py`（只彙總）與 `ingress_evidence.py`（守恆等式，分析器版本 `pvp-v71-ingress-3`）。`command_evidence.py`、`late_count.py`、`quad_evidence.py`、`gameplay_soak_evidence.py` 要 probe 的產物或同一台機器的時鐘，LAN 不能用。
- 判定由 lan15-tools 寫的 `lan15_judge.py` 做：呼叫 `ingress_evidence.py`，並以正規式讀第 2～5 節列出的行。工具只把本宣告的條文寫成程式。
- 各機器的 log 時間是各自的牆鐘；同一份 log 內用 `mono_ns` 算時間差（`LogFile.cpp:28-35`）。跨機器只做計數比對，不比時刻，除非經第 1 節的偏移估計。
- 以前的真實 LAN 只有兩次：v6（2026-10-08，Mac 主持、2 台 Windows、約 14.5 分鐘）與 2026-10-16 的全 Windows 臨時測試（完整 pv7 之前，三人同場約 4 分 47 秒）。兩次都沒有 `connection failed` 與逐出；v6 沒有限流（`v6/ACCEPTANCE_STATUS.md:46`），10-16 的 Gateway `drop_*` 與限流是 0（v7.1 `HANDOFF.md:337`）。兩次都還沒有「中斷」（T_int）的概念；snapshot 空窗的最大值是 v6 的 126～171 ms、10-16 的年齡 124.2 ms，相對 T_int 0.5 s 約 3 倍餘裕，樣本合計約 19 分鐘。

## 1. 構成、前提與順序

- **構成**：~~Mac（Intel）跑 Match＋Gateway，也當 1 位玩家；Windows Client N_w 台。~~（2.3，D239 核准：使用者：「核准 lan15 宣告 2.3（Mac 只跑 Match＋Gateway、不當玩家，Client 只在 Windows，N 台）。」）Mac（Intel）只跑 Match＋Gateway，不當玩家；Client 只在 Windows 上，N 台（N＝2 或 3，`n_w`＝`n`）。使用者：「記得，給我打包好的gateway、match、client。這次match和gateway是mac上的，而client則是windows上的。」確認 Mac 是否當玩家時：「不當。」同一個區域網路。地圖 `pvp_corners_v1`。
- **程式**：最新發行線（含換模型的合併，D234）的四平台 L1 產物（D232）。Windows 用 windows-x64 套件，Mac 用 macos-x64 套件。各套件 `build_metadata.json` 的 `source_revision` 相同，而且每個啟動行的 `executable_sha256` 等於該套件的清單。
- **（2.2，D238 核准）Gateway 與 quad probe**：發行包沒有 macOS 版的 Gateway，quad probe 也不在發行包裡。兩者在 Mac 上從同一個工作樹建置（產品原始碼與 L1 的 commit `646b808f` 相同，`git diff 646b808f -- apps services engine` 為空），SHA-256 記在 `marks.txt` 的註解，以啟動行的 `executable_sha256` 核對。上一條「每個啟動行的 sha256 等於該套件的清單」只適用於套件內的 Match 與 Client。
- **場次資料夾**：Mac 上新建一個空資料夾放 Match 與 Gateway 的 log 與 trace；每台 Client 用新解壓的套件（`logs/` 是空的）。Mac 的 Client 用自己的 `--log` 與 `--movement-trace` 檔名。
- **（2.1）判定用的資料夾**：場次結束後整理成 `lan15_judge.py` 讀的形狀：`marks.txt`；`mac/`（match.log、gateway.log、`match*-ingress.jsonl`、`match*-shots.jsonl`、`match*-commands.jsonl`）；`clients/<名稱>/`（那台機器所有 Client 程序的 `.log`；Windows 另有 `ping.txt`；Mac 的 Client 也放一個資料夾）；`quad/`（Q2 的 quad probe 的輸出與 log）。
- **（2.1）`marks.txt` 的格式**（一行一個 `key=value`；時刻用 ISO 格式，到秒）：`n=`、`n_w=`、`s1_end=`（Mac 的時刻）；每次 S2 試行一行 `trial=<Windows 名稱>,<2s|7s>,<那台 Windows 的時刻>`；`q1=<Windows 名稱>`；`q2_start=`、`q2_end=`（Mac 的時刻）。
- **啟動的規則**：每份 log 恰好 1 行啟動（Match、Gateway、每個 Client 程序各自的 log）。最後一位玩家 `join player=N accepted` 之前遇到環境故障（例如防火牆擋 UDP）可以重啟，但要換新的資料夾重來；之後不准重啟任何程式。trace 依啟動行記錄的實際檔名讀（D191 的編號名）。
- **時鐘偏移**：每台 Client 用成對事件估計它對 Mac 的牆鐘偏移：Client 的 `PvP connection phase=playing player=N` 對 Gateway 的 `player active player=N`。只用在第 2 節的 S1 範圍；估計的誤差容許 ≤1 s，超過時那台 Client 的 J3 記確認不了。
- **順序**（一個場次，依序；使用者在每一步開始與結束時，記下 Mac 上的時刻到 `marks.txt`，精確到秒）：
  1. **S1**：全員加入後連續對戰至少 15 分鐘，不做刻意的干擾。S1 結束時記下時刻 `s1_end`，之後**空 60 s 不做任何事**（照常遊戲）。
  2. **S2**：只對 Windows 做（M7）。每台 Windows 依序：2 s → 等 ≥45 s → 7 s（回 Lobby 後重新加入）→ 等 ≥45 s → 下一台。
  3. **S3**：Q1（關閉視窗後重新加入）→ 等 ≥45 s → Q2（room_full）。
  4. **結束**：（2.1）每位玩家先停止射擊與換彈至少 3 秒（讓最後的裁決到達、最後一個 1 秒的摘要窗口寫出），再按 Esc 離開並關閉視窗 → Gateway Ctrl+C → Match Ctrl+C。
  - S7（新模型的外觀）在整個場次中觀察。S5、S6 從整個場次的 log 取。
- **≥45 s 的來源**：Match 的連線品質以 10 s 為一個窗口，連續 3 個窗口失敗就逐出（`MatchRuntimeHost.cpp:700-722`、`Movement.hpp:109-112`）。一次斷線最多讓 2 個窗口失敗，所以兩次斷線之間至少要有 1 個完整的乾淨窗口（最壞 20 s）；45 s 是 3 個窗口加餘裕。
- **不重跑**：S1 只做一次。中途有人的 Wi-Fi 掉線之類的事，也照實記錄成 S1 的結果，由使用者判斷要不要另排一次（§14.1）。

## 2. S1：完整 pv7 的對戰場次（L2）

- **S1 範圍**：起點＝Match 上最後一位玩家的 `join player=N accepted` 行（Mac 的牆鐘）；終點＝`marks.txt` 的 `s1_end` 減 30 s。不從任何中斷行推導起訖。
- **窗口**：S1 用 Gateway 的 10 秒統計窗口，只算完全落在 S1 範圍內的窗口。
- Client 的行換算到 Mac 的時間（第 1 節的偏移）後，落在 S1 範圍內的才算。

| 代號 | 判定 | 通過條件 | 門檻的來源 |
|---|---|---|---|
| J0 | 連續 15 分鐘、全員在場 | S1 範圍 ≥900 s；範圍內 Match 沒有任何 `join player=` 或 `leave player=` 行；範圍內每台 Client 的每行 `PvP summary` 的 `players=` 都等於 N | lan15-windows.md 的目的（至少 15 分鐘）；v7 D73 的兩機漂移 ≥15 分鐘 |
| J3 | 沒有非刻意的中斷、逐出與失敗 | S1 範圍內：(1) 每台 Client 0 行 `connection interrupted`、0 行 `connection failed`；(2) Gateway 0 行 `player evicted`、Match 0 行 `evicted player=`；(3) Match 0 行 `leave player= reason=`（範圍內任何 reason）；(4) Gateway 0 行 `player expired`；(5) Gateway 0 行 `join refused … code=protocol_version`、0 行 `runtime disconnected` | 產品契約：T_int＝0.5 s 以上沒收到才算中斷（`network-architecture.zh-Hant.md:560-568`）；第 0 節的兩次 LAN 觀測（約 3 倍餘裕、約 19 分鐘的樣本）；逐出規則 `Movement.hpp:109-112` |
| J4 | Gateway 沒有限流 | S1 範圍內每個窗口的 ingress 行 `drop_rate_limited_*` 都是 0 | v7 第 16 批「Gateway limiter」（`16-integration-acceptance.md:280`；`gameplay_soak_evidence.py:376`，D122：只判拒收）；限速器 GCRA 120/s、突發約 119 包（`services/gyo_gateway/session/session.go:42-52`） |
| J5 | 結果的 30 Hz 節奏 | S1 範圍內每個有值的窗口，Gateway 印出的 `results_min_interval_ms` ≥33.3；而且每位在 S1 有裁決的玩家至少有 1 個有值的窗口 | v7 第 08b 批判定 2（`16-integration-acceptance.md:171`）；寫出完成後才重設期限（`server.go:802-812`、`action_delivery.go:250-256`）；不得空判照 `edge-results-passthrough-l2.md:75` (c) |

S1 只記錄（不判定）：

- `link_action_min_interval_ms`（pv7 的 link 不再節拍，`edge-actions-passthrough-l2.md:95`）。
- 替代命令的比例（Match ingress 的 substituted、late），每位玩家；比較 v6 的 99.46%／99.74%（實際命令比例）與 10-16 的 0.55%／0.37%。
- connection quality 的 `failed_windows` 最大值（v6 最多 2／3，10-16 是 1／3）。
- Snapshot 年齡、FPS、最長幀（Client 的 `PvP summary`）。
- 非 respawn 的重設（D44 型）的次數與時刻（D155 的 300 ms 型照設計保留，不判 0）。
- Gateway 的 `drop report` 與 Match 的 `gw_drop report` 是否一對一相等（`compare_reports.py`，只記錄）。
- 動作的 Cooldown 與 Expired 比例（M3：沒有 LAN 的來源）。
- `network_statistics.py` 的彙總全文。

## 3. 整個場次的判定

| 代號 | 判定 | 通過條件 | 門檻的來源 |
|---|---|---|---|
| J1 | session 完整 | `match-ingress.jsonl` 的 `trace_end` 恰好一行且 `dropped == 0`；`match-shots.jsonl` 的 `trace_end` 恰好一行且 `dropped == 0`；每個在 Match 有 `join player=N accepted` 的玩家，恰好一行 `final=1` 的 ingress 行與一行 `final=1` 的 resync 行（讀 Match 的行）；Match 的 end 行 `reason=signal exit_code=0` | outage L2 的寫法（`resync-l2-l3-declaration.md` 第 3 節 J-R7、第 4 節前提）；結束步驟見 v7 `LAN_TEST.md:141` |
| J2 | ingress 守恆 | `ingress_evidence.py`（不帶 `--injection`）的整體狀態 `passed`；`undetermined` 記確認不了 | 等式出自 v7 `08c-drop-report.md` 與 protocol-v7 §2a、§9；整體狀態的用法見 `edge-rate-limit-l2.md:42-43`。涵蓋整個場次（工具沒有時間窗口） |
| K1 | 動作的計數一致（Client 自己） | 每個「以 Esc 正常離開」的 session（以 Client log 的 `player=` 分組）：最後一行 `PvP summary` 的 `actions_pending=0`，而且整個 session 的 `actions_submitted` 合計＝`accepted` 合計＋`rejected` 合計 | `PvpApplication.cpp:773-782`（每行是窗口內的增量）。只用單機的計數 |
| K2 | 動作的計數一致（Client 對 Match） | 同上的 session：Client 的 `accepted` 合計＝Match `match-shots.jsonl` 中該 player 的 `rejection` 為 `none` 的紀錄數；每個 reason 的 `rejected_<reason>` 合計＝`rejection` 為該 reason 的紀錄數 | `MatchShotTrace.hpp:110`、`:137-138`（每個裁決一筆紀錄，含 `rejection`）；要求 J1 的 `match-shots` `dropped == 0` |

- 以 R2（回 Lobby）或 Q1（關閉視窗）結束的 session，K1、K2 只記錄（裁決可能在 Client 離開之後才到）。

## 4. S2：斷線恢復（outage-resync 的 L3，D179；只對 Windows，M7）

- 每台 Windows 各做一次 2 s、一次 7 s。拔網路線或關 Wi-Fi 都可以，記下用哪種。
- **量中斷長度（M9）**：那台 Windows 在 S2 期間一直開著帶時間戳的 ping（PowerShell：`while($true){ "$(Get-Date -Format o) $(Test-Connection MAC_IP -Count 1 -Quiet)"; Start-Sleep -Milliseconds 200 } | Tee-Object ping.txt`）。中斷長度＝連續失敗的第一筆到恢復成功的第一筆。
- **有效試行**：2 s 那次的中斷長度在 1.5～4.0 s；7 s 那次 ≥6.0 s。不在範圍內的試行記確認不了（E），不記失敗，不重做。
- 兩次之間與換台之間至少 45 s（第 1 節）。

| 代號 | 判定 | 通過條件 | 來源 |
|---|---|---|---|
| R1（2 s） | 不回 Lobby，繼續遊戲 | 使用者觀察：沒有回 Lobby。該 Client 的 log：`connection interrupted` 之後有 `connection resumed`，中間沒有 `connection failed`；Gateway 沒有該玩家的 `player evicted` 或 `player expired` | D179；`after-re-evaluate.md:218`；network-architecture §4 |
| R2（7 s） | 約 5 秒後回 Lobby，Gateway 讓 session 到期 | 使用者觀察：回到 Lobby，顯示 `Gateway connection timed out`。該 Client 的 log：`connection interrupted` 之後有 `connection failed … reason=Gateway connection timed out`；T_max＝（兩行的 `mono_ns` 差）＋interrupted 行的 `gap_ms`，落在 [5000, 5100] ms。Gateway：該玩家有 `player expired player=N reason=session_timeout`，早於任何該玩家的 `player left`；Match：有該玩家的 `leave player=N reason=session_timeout` | T_max＝5 s（`network-architecture.zh-Hant.md:562`、`:568`；`ClientConnection.cpp:728-731`）；session 到期 `server.go:927-947`（K10 在 L2 量不到的經路，D233）。上限 5100：計時器與 log 的延遲；抓得到「T_max 從中斷才起算」（約 5500） |
| R3 | 回 Lobby 後能重新加入 | R2 之後從 Lobby 重新加入：Match 有新的 `join player=M accepted`（M ≠ N），該 Client 回到遊戲 | network-architecture §4「重新加入走新的 join」 |

只記錄：

- `connection summary` 的 `interruptions`、`max_gap_ms`、`transient_socket_errors`（整個 session 的總數，`ClientConnection.cpp:239`；Windows 上 WSAECONNRESET 的推論 D172 D-7）。
- 有沒有 `session rebound`（endpoint 改變）與 `silent_ms`。
- ping 量到的中斷長度。

## 5. S3：關閉視窗後重新加入；room_full（v7 L3 的補做）

| 代號 | 判定 | 通過條件 | 來源 |
|---|---|---|---|
| Q1 | 關閉視窗後重新加入 | 一位 Windows 玩家直接關閉視窗（不按 Esc），再啟動 Client 加入。Gateway 有舊 id 的 `player left` 或 `player expired`；Match 有舊 id 的 `leave`；之後 Match 有新的 `join player=M accepted`，M ≠ 舊 id；其他每台 Client 的 `PvP summary` 的 `players=` 先變成 N−1，再回到 N，期間沒有 `connection interrupted` 或 `connection failed` | v7 L3-4（`v7/STABLE_BASELINE.md:140`）；v6 `MANUAL_ACCEPTANCE.md:100`（重新加入是新身分，沒有舊玩家殘留） |
| Q2 | 房間已滿 | （2.1）補人：Mac 上用同一個 commit 在本機建置的 quad probe 補到 4 人，再試第 5 個：`gyo_object_fps_pvp_quad_probe --gateway MAC_IP:8080 --arena <pvp_corners.json> --create false --clients <4−N> --expect-players 4 --fifth true --passive true --duration 20 --output quad/out`，輸出與 log 存進 `quad/`。工具讀 `quad/` 的 JSON 的 `fifth`（`error` 是 `room_full`、`joined` 是 false）。房間有 4 位玩家（Gateway 的預約也算，`server.go:344`）時，第 5 個加入被拒：Gateway 有 `join refused … code=room_full`；那個 Client 的 log 有 `connection failed … reason=room_full`，留在 Lobby；房間內的 4 位玩家沒有 `connection interrupted` 或 `connection failed` | `server.go:344-345`（`MaxPlayers`＝4）；protocol-v7（409 `room_full`）；v6 `MANUAL_ACCEPTANCE.md:104` |

## 6. S5、S6、S7

- **S5（只記錄）**：每位玩家在 S1 範圍內的 clock sync 行，偏移與漂移（首尾差，µs／分鐘）。
- **S6（只記錄）**：握手期（`join` 到第一個 Welcome）的 `connection failed reason=` 的種類與次數（握手期的暫時性錯誤直接走 Failure、不計數，`ClientConnection.cpp:916-919`）。
- **S7（L3，使用者判定＋log 核對）**：新的人物模型（D234）在 Windows 上看起來正常（2.3：Mac 不當玩家，不看 Mac）（動畫、法線、頭髮、沒有缺面或閃爍），由使用者判定。log 核對：每台 Client 都有進入 Lobby 的 `PvP connection phase=` 行；沒有 `missing player animation`、`missing or ambiguous player bone` 這兩種錯誤（換模型分支 `PlayerPresentation.cpp:33`、`:94`、`:100`；合併後依實際的字串再核對一次，字串不同時以合併後的程式為準，在核准前更新本條）。

## 7. 收支與整體判定

- **收支**：
  - 檔案：Match（match.log、`match-commands.jsonl`、`match-ingress.jsonl`、`match-shots.jsonl`）、Gateway（gateway.log）、每台 Client 的 `logs/` 整個資料夾、`marks.txt`、每台 Windows 的 `ping.txt`。
  - 每份 C++ log（Match、每個 Client 程序）沒有 `[log] dropped_lines=` 行，而且有正常結束的最後幾行（Match 的 end 行；Client 的 `connection summary`）。
  - 每個在 Match 出現過的 player id，都對得上一份 Client log。
  - 每份 log 恰好 1 行啟動（第 1 節）。
- **證據不全一律記確認不了（E），不記失敗（F）**。E 不算通過發行條件，由使用者決定怎麼處理。
- **判定**：
  - S1：J0、J3、J4、J5 全部通過。
  - 整個場次：J1、J2、K1、K2 全部通過。
  - S2：每台每個 R 項都通過（E 的試行另計）。
  - S3：Q1、Q2 都通過。
  - S7：使用者判定通過，且 log 核對通過。
  - 有任何一項不通過，不重跑、不修改，照實記錄並停下，交使用者決定（§14 開頭）。
- **v7.1 的發行條件**：上面全部通過。S4 未量（D232），S5、S6 只記錄。
- **判定工具**（2.1）：`build/acceptance/object_fps_pvp/lan15_judge.py`（`judge_id` `pvp-v71-lan15-1`）。鎖定的 sha256：
  - ~~`lan15_judge.py`：`67c9e4d91dd949a2d29f7a4db16b8e8889f49b124b88376b3098b11655ff8751`~~（2.1，由 2.2 取代）
  - `ingress_evidence.py`（`pvp-v71-ingress-3`）：`567198d660985d862e01b49b481a57c0393bf4511267fec1b6787de831008302`
  - 鑑別力的測試 `tests/object_fps_pvp/test_lan15_judge.py`：~~`1a42ca509d98e3f4efa0b2c0c95f15bba2f12aa9d21a10b298d1b8df8eb45590`~~（2.1，由 2.2 取代）（CTest `object_fps_pvp.lan15_judge`，cpu 標籤）
  - 結果（2026-10-29，本機）：照宣告程序的合成場次判通過；25 份竄改（J0 ×2、J3 ×3、J4、J5 ×2、J1 ×2、J2 ×2、K1、K2、R1、R2 ×2、試行長度、Q1 ×3、Q2 ×2、S7、收支 ×2）全部判出失敗或確認不了（照宣告的分類）。判定工具的 7 個突變（J5 門檻、T_max 上限、J3 少了逐出、K1 少了 pending、收支少了 dropped_lines、Q1 少了人數的變化、Q2 少了房間內玩家的檢查）都被這組測試殺死。
  - J2 的等式本身由 `ingress_evidence.py` 自己的測試守；這裡只測 J2 怎麼讀它的結果。
  - **（2.2，D238 核准）工具的修正**：lan15-kit 寫手冊時發現，Windows PowerShell 5.1 的 `Tee-Object` 把 `ping.txt` 寫成 UTF-16，上面鎖定的工具以 UTF-8 讀，S2 的每次試行都會變成確認不了。修正：依 BOM 判斷 UTF-16，否則以 UTF-8 讀。判定的條文不變。新增 1 個測試（UTF-16 的 `ping.txt`，3 個 R 項都通過）；把讀法改回只讀 UTF-8，這個測試失敗。現在鎖定的 sha256（取代上面兩個舊值；`ingress_evidence.py` 不變）：
    - `lan15_judge.py`：`10f7ddc80d584f577e0df4ae5d11858d4f3047eb2205ac256f108b45366f1218`
    - `test_lan15_judge.py`：`ac548dbab48399bc64dce2f5f9e0b6d11704fd58118503246160f01f818dcc2e`（27 個測試）
- 用合成的紀錄做竄改檢查：每條判定至少 1 份應該失敗的紀錄（例如 S1 多一行 interrupted、少一行 resumed、多一行 evicted、`drop_rate_limited` 1、results 間隔 33.2、`dropped_lines` 1、T_max 5500、K2 少一筆 none），全部要判出失敗；沒有改動的合成紀錄要判通過。

## 8. 範圍的限界（照 HANDOFF「後續宣告要寫的範圍的限界」，直到解消）

- Windows 的 symlink 案例（MinGW 與沒有開發人員模式的 MSVC 都提早返回）。
- `os.console_close`（CI 與使用者的機器都 Skipped）與 Ctrl+Break（使用者的鍵盤沒有 Break 鍵）。
- quick-close 量的是產品的網路層，不是整個 Client 程序（D202）；`close_timing` 不在 CI 跑（D204）。
- ACP 非 UTF-8 時的行為在 manifest 之下不再測得到（D212）。
- OS 的 ★ 4 批已出貨、未經功能驗收（D219）。
- Windows 上 worker 驗收不算發行條件（D231）：沒有 SDL 的程序裡 Client 的重送約 43～53 Hz；mock 遇到 WSAECONNRESET 會讓驗收中止（約 5%）；「a token wait outlived four commands」以 mock 輪詢的約 1 ms 刻度量；D226 的 2 次計時器喚醒來源不明；ACK 檢查的相位問題沒有量；D227 只在 macOS 用突變確認過。
- S4（Windows Match 的 Tick 精度，F7）未量：Match 在 Mac（D232）。
- macOS Client 沒有測：Mac 不當玩家（2.3）；Mac 是主機，斷線等於全員斷線（M7）。
- ~~Mac 同時當主機與玩家，對 J3、J5 是 v6 之外的新條件。~~（2.3：Mac 不當玩家，這條不再適用；構成和 v6 的 LAN 相同：Mac 主持、Client 都是 Windows）
- 同機的延遲門檻（產生到 Actual 50／66.7 ms、首次送出 22 ms）在 LAN 不適用：跨機器的 trace 不是同一個時鐘。
- 擊殺、重生位置與命中回饋沒有機械判定：Match 的紀錄沒有這些欄位，只靠使用者觀察。
- K9 的「直接超過 T_max」分支（D200）這次只有在 S2 碰巧發生時才看得到，沒有刻意的試行。
- 真實互聯網（WAN）不在範圍。
- 樣本：一次場次，N 人。

## 對抗式檢查的結果（2026-10-29，xhigh-reviewer 1 人，讀 `e5443624`）

blocker 2、major 11、minor 14。主會話抽查了 B1（`action_delivery.go:240-242`、`edge-actions-passthrough-l2.md:95`）、M1（`IpcHost.cpp:713`、`server.go:763`、`:938`）、M5（`LogFile.cpp:151`）的證據，都成立。第 2 版照下面處理；標「待決」的三項，使用者選了建議（M7：列為限界；M9：實體斷線＋ping 量長度；M3：判計數一致、比例只記錄）。

| 代號 | 發現（摘要） | 第 2 版的處理 |
|---|---|---|
| B1 | J5 的 `link_action_min_interval_ms ≥33.3` 在 pv7 不成立（link 不再節拍，Wi-Fi 叢集會壓到 33.3 以下） | J5 只判 `results_min_interval_ms`；`link_action_min_interval_ms` 只記錄（來源 `edge-actions-passthrough-l2.md:95`）；刪掉「和網路無關」 |
| B2 | S1 的起訖在 Client 的 log 上取不出來；以第一行 interrupted 為終點會誤判通過 | S1 結束後空 60 s 才開始 S2；終點＝使用者記錄的時刻減 30 s，不從中斷行推導；各機器的時鐘偏移用成對事件估計（Client 的 `PvP connection phase=playing` 對 Gateway 的 `player active`），容許 ≤1 s，超過記確認不了；窗口指 Gateway 的窗口，只算完全落在範圍內的 |
| M1 | 「逐出」的取法錯（應讀 `player evicted`／`evicted player=`） | J3 拆成四條：evicted 0、非 `client_request` 的 leave 0、`player expired` 0、`code=protocol_version` 與 `runtime disconnected` 0 |
| M2 | 沒判「連續 15 分鐘、全員在場」 | 加 J0：S1 ≥900 s、範圍內 Match 沒有 join／leave、各 Client 的 `players=` 等於宣告人數 |
| M3 | 沒有射擊、裁決、擊殺、重生的判定 | **待決 3** |
| M4 | R2 沒查 Gateway 的 session 到期（K10 量不到的經路，lan15 是唯一實測處） | R2 加：`player expired … reason=session_timeout`，早於任何 `player left`；Match 對應的 `leave … reason=session_timeout` |
| M5 | 收支沒算 log 的計數丟棄；E／F 分類矛盾 | 收支加 `dropped_lines`、正常結束行、`trace_end` 的 dropped／suppressed、玩家 id 與 Client log 對應；證據不全一律記確認不了（E）；E 不算通過發行條件 |
| M6 | 判定工具沒有凍結、沒有鑑別力檢查 | 開跑前鎖定 `lan15_judge.py` 與 `ingress_evidence.py` 的 sha256；用合成紀錄做竄改檢查（每條判定至少 1 份） |
| M7 | 對 Mac 斷線＝主機斷線，影響全員 | **待決 1** |
| M8 | 「隔 15 秒」沒有來源，會在正確的產品上觸發逐出 | 間隔 ≥45 s（3 個 10 s 品質窗口＋餘裕，`MatchRuntimeHost.cpp:700-722`）；R1 加「沒有 `player evicted`」 |
| M9 | 實體斷線的長度不受控，2 s 可能變成 5 s 以上 | **待決 2** |
| M10 | J5 漏了「不得空判」 | 每位在 S1 有裁決的玩家，至少 1 個有值的窗口 |
| M11 | log 追加寫入＋不重跑，容易無法挽回 | 新建資料夾；每份 log 恰好 1 行啟動；trace 依啟動行的實際檔名讀；最後一位玩家加入前的環境故障可重啟（換新資料夾），之後不准 |
| m1～m14 | 來源描述、行的字面、`rate_limited` 是累計、T_max 其實量得到、`transient_socket_errors` 只有整個 session 的總數、S7 的錯誤字串、Q1 的新身分、Q2 未定、J2 是整個場次、多開 Client 要各自 `--log`、S5 範圍矛盾 | 全部照建議改：J1／J2 移出 S1、改為「整個場次」；R2 判 T_max＝（failed − interrupted 的 mono 差）＋gap_ms，落在 [5000, 5100]；S6 改看握手期的 `connection failed reason=`，「直接超過 T_max」分支列入限界；S7 改成列出確切的錯誤字串並要求啟動到 Lobby 的行存在；Q1 加新 id ≠ 舊 id、其他 Client 的 `players=` 先減 1 再回復；Q2 等人數定了再寫 |

## 第 1 次場次的結果（2026-10-29，離線的現場）

收支：確認不了。宣告的程序大部分沒有照做，或做不到：

- `marks.txt` 沒有寫。
- 同一個資料夾裡 Match 與 Gateway 各啟動了 2 次。第 1 次的 Gateway 公告的是 `192.168.107.189`：11:01:55～11:02:16 有 3 次 join reserved，都在 5 s 後以 `session_timeout` 到期，UDP 沒有到達。第 2 次公告的是 `192.168.50.64`。第 2 次啟動在最後一位玩家加入之前，宣告允許重來，但要換新的資料夾。
- S2 的 ping 沒有執行，也沒有做斷線。
- S3 的 Q1 沒有做。Q2 的 quad probe 當場失敗（`--fifth needs a full room this probe created`）：宣告寫的命令違反 probe 自己的限制，`--fifth` 要 `--create true` 而且補滿 4 人，`--passive` 也不能配 `--fifth`（`quad_main.cpp:62-63`）。這是主會話的錯：交出命令前沒有實際跑過。
- S7 的觀察沒有記錄。

依 §14.1，這次記錄為「確認不了」，不當作通過，也不被之後的場次覆蓋。

**判定**：照宣告，收支不齊，各項記確認不了。另外，S1 的長度只有約 6 分鐘（11:04:11 最後一位加入，11:10:11 第一位離開），就算收支齊了，J0（≥900 s）也不成立。

**構成**：Mac 跑 Match＋Gateway。Match 是本機建置：現場離線，CI 的 macos-x64 包沒有事先下載，改用同一份產品原始碼的本機建置，`executable_sha256` 開頭 `1d1a60dc`。這是對宣告第 1 節的偏差，照實記錄。Gateway 開頭 `b11aa6d4`。

Windows 有 2 台，都用 CI 套件 `gyo-package-object_fps_pvp-windows-x64`，`executable_sha256` 開頭 `89c23d40`，兩台相同：

- LUO：player 1，11:02:45 加入。
- KLYU：player 2，11:04:11 加入。

**探索性的觀察（不是判定，只用第 2 段的紀錄）**：
- `ingress_evidence.py` 對第 2 段（Match log 第 541 行起、Gateway log 第 331 行起、`match.2-ingress.jsonl`）的整體狀態是 `passed`；其中 I1、I1a 是 undetermined。
- 動作的計數，Client 與 Match 的 `match.2-shots.jsonl` 完全一致（`trace_end` 的 dropped 是 0）：
  - player 1：accepted 149、rejected 10，原因都是 `magazine_full`；
  - player 2：accepted 186、rejected 1，原因是 `dead`。
- 兩台都沒有 `connection interrupted`；`transient_socket_errors` 是 0；`max_gap_ms` 最大 141。
- KLYU 出現過一次 `failed_windows=1 of 3`（11:06:31），10 秒後恢復。
- Snapshot 年齡 P50 是 8.1／8.7 ms，最大 55.2／86.9 ms。
- FPS：LUO 最低 58，KLYU 最低 164。
- LUO 第一次連線時遇到 Gateway 的 HTTP 逾時與讀取失敗（11:01:07～11:01:47）。當時是第 1 次的 Gateway。

**證據**：Mac 的 `~/gyo-lan15/session-20261029/`；Windows 的 `~/Downloads/20261029_KLYU_logs.zip`、`~/Downloads/logs-LUO.zip`；探索性核對的輸出在會話的 scratchpad `lan15-20261029/`。

**使用者回報的「網路太慢被踢掉」的提示**：對得上 LUO 在 11:02:00～11:02:16 的 3 次 `connection failed … reason=Gateway connection timed out`（player 1～3）。這不是對戰中被逐出：
- 那 3 次都在加入後 5 s 失敗，`received_datagrams=0`，UDP 一個都沒收到。Gateway 同時記到 3 次 `join reserved` → `player expired … reason=session_timeout`。
- 原因：第 1 次的 Gateway 公告的是 `192.168.107.189`。這是 `start-gateway.sh` 以 `ipconfig getifaddr en0` 自動選的位址，但場次的 LAN 是 `192.168.50.x`（Mac 在 LAN 上是 `192.168.50.64`），Client 把 UDP 送到了錯的位址。
- 第 2 次 Gateway 公告 `192.168.50.64` 之後，兩台都正常進入，對戰中沒有任何中斷。
- 腳本的錯：沒有確認公告的位址和 Client 在同一個網段。

**教訓**：每台機器一支腳本，包辦啟動、ping、收 log，交出前在本機實際跑過一次；程式事先完整放好，不假設現場有網路；Gateway 公告的位址要列出所有網卡、由使用者確認和 Client 同網段，不自動選。

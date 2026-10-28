# lan15-windows 的事前宣告（第 1 版草案，待對抗式檢查與使用者核准）

更新：2026-10-29。Owner：`object_fps_pvp`。規劃：[lan15-windows.md](lan15-windows.md)（D232～D234）。
核准前不跑場次；核准後內容不改，跑完不放寬（AGENTS.md §14.1）。人數與日期等使用者（lan15-windows.md 第 1 項），本版以「Windows Client 1～2 台＋Mac Client 1 台」寫，人數定了再改第 1 節（改版要重新核准）。

## 0. 這份宣告依據的事實（盤點，2026-10-29）

- repo 裡沒有專門判定 LAN 場次的入口。能直接讀 LAN log 的只有 `network_statistics.py`（只彙總，沒有門檻）與 `ingress_evidence.py`（守恆等式）。`command_evidence.py`、`late_count.py`、`quad_evidence.py`、`gameplay_soak_evidence.py` 都要 probe 的產物或同一台機器的時鐘，LAN 不能用。
- 所以 lan15-tools 要寫一支 `lan15_judge.py`：呼叫 `ingress_evidence.py`，並用正規式讀下面第 4 節的證據行。這支工具只把本宣告的條文寫成程式，不增加判定。
- 以前的真實 LAN 只有兩次：v6（2026-10-08，Mac 主持、2 台 Windows、約 14.5 分鐘）與 2026-10-16 的全 Windows 臨時測試（完整 pv7 之前的樹，約 4 分 47 秒三人同場）。兩次的斷線、逐出、版本錯誤與 Gateway 的 `drop_*`、限流都是 0（`v6/ACCEPTANCE_STATUS.md:36-54`、v7.1 `HANDOFF.md:337`）。

## 1. 構成與順序

- **構成**：Mac（Intel）跑 Match＋Gateway，也當 1 位玩家；Windows Client 1～2 台（使用者的、朋友的）。同一個區域網路。地圖 `pvp_corners_v1`。
- **程式**：最新發行線（含換模型的合併，D234）的四平台 L1 產物（D232）。所有機器用同一個 commit；Windows 用 L1 的 windows-x64 套件，Mac 用同一 commit 在本機建置的套件。啟動行的 commit 要一致（S1 前提）。
- **順序**（一個場次內，依序）：
  1. S1：全員加入後連續對戰至少 15 分鐘，不做刻意的干擾。
  2. S2：斷線恢復（每台 Windows 與 Mac 各做 2 s 與 7 s）。
  3. S3：關閉視窗後重新加入；`room_full`。
  4. 結束：Client 按 Esc 離開並關閉視窗 → Gateway Ctrl+C → Match Ctrl+C（Match 要寫 `final=1` 與 end 行）。
  - S7（新模型的外觀）在整個場次中觀察。S5、S6 從整個場次的 log 取。
- **不重跑**：S1 只做一次。中途有人自己的 Wi-Fi 掉線之類的事，也照實記錄成 S1 的結果（不通過），由使用者判斷要不要另排一次（§14.1：受入判定的失敗不被後來的成功覆蓋）。

## 2. S1：完整 pv7 的對戰場次（L2）

判定的時間範圍：最後一位玩家 `join accepted` 起，到 S2 開始前（使用者記下的時刻；工具以第一次刻意斷線之前最後一個 10 秒統計窗口為界）。

| 代號 | 判定 | 通過條件 | 門檻的來源 |
|---|---|---|---|
| J1 | session 完整 | `match-ingress.jsonl` 的 `trace_end` 恰好一行且 `dropped == 0`；每位玩家恰好一行 `final=1` 的 ingress 行與 resync 行；Match 的 end 行 `reason=signal exit_code=0` | outage L2 的寫法（`resync-l2-l3-declaration.md` 第 3 節 J-R7、第 4 節前提）；結束步驟見 v7 `LAN_TEST.md:141` |
| J2 | ingress 守恆 | `ingress_evidence.py`（不帶 `--injection`）的所有等式 `passed`；`undetermined` 也算不通過 | 等式出自 v7 `08c-drop-report.md` 與 protocol-v7 §2a、§9；outage L2 的 J-R7 也用它 |
| J3 | 沒有非刻意的中斷與失敗 | S1 範圍內：每位玩家 0 行 `connection interrupted`、0 行 `connection failed`、0 次逐出（Match 的 `leave reason=` 不是玩家自己離開的）、0 次版本錯誤 | 產品契約：T_int＝0.5 s 以上沒收到才算中斷（`network-architecture.zh-Hant.md:560-568`），LAN 上不應發生；以前兩次真實 LAN 都是 0 |
| J4 | Gateway 沒有限流 | S1 範圍內 `rate_limited` 與每個 `drop_rate_limited_*` 都是 0 | v7 第 16 批「Gateway limiter」（`16-integration-acceptance.md:280`；`gameplay_soak_evidence.py:376`，D122：只判拒收） |
| J5 | 結果與動作的 30 Hz 節奏 | 每個有值的 10 秒窗口，Gateway 印出的 `results_min_interval_ms` 與 `link_action_min_interval_ms` ≥33.3 | v7 第 08b 批判定 2、5（`16-integration-acceptance.md:171`、`:267`）。這是 Gateway 的送出節奏，和網路無關 |

只記錄（S1 範圍內，不判定）：

- 替代命令的比例（Match ingress 的 substituted、late），每位玩家；比較 v6 的 99.46%／99.74%（實際命令比例）與 10-16 的 0.55%／0.37%。
- connection quality 的 `failed_windows` 最大值（v6 最多 2／3，10-16 是 1／3）。
- Snapshot 年齡 P50／P95、FPS、最長幀（Client 的 `PvP summary`）。
- 非 respawn 的重設（D44 型）的次數與時刻。LAN 有干擾，D155 的 300 ms 型照設計保留，所以不判 0。
- `transient_socket_errors`（S1 範圍內的值）。
- Gateway 的 `drop report` 與 Match 的 `gw_drop report` 是否一對一相等（`compare_reports.py`，工具標「只記錄」）。
- `network_statistics.py` 的彙總全文。

## 3. S2：斷線恢復（outage-resync 的 L3，D179）

每台 Windows 與 Mac 各做一次 2 s、一次 7 s。拔網路線或關 Wi-Fi 都可以，記下用哪種、開始與恢復的時刻（手錶或手機的秒數即可）。一次只對一台做；兩次之間至少隔 15 秒。

| 代號 | 判定 | 通過條件 | 來源 |
|---|---|---|---|
| R1（2 s） | 不回 Lobby，繼續遊戲 | 使用者觀察：畫面沒有回 Lobby；該 Client 的 log 有 `connection interrupted` 接著 `connection resumed`（中間沒有 `connection failed`） | D179；`after-re-evaluate.md:218`；network-architecture §4 |
| R2（7 s） | 約 5 秒後回 Lobby | 使用者觀察：回到 Lobby，顯示 `Gateway connection timed out`；log 有 `connection interrupted` 接著 `connection failed … reason=Gateway connection timed out` | 同上；T_max＝5 s（`network-architecture.zh-Hant.md:562`、`:568`） |
| R3 | 回 Lobby 後能重新加入 | R2 之後從 Lobby 重新加入成功（新的 `join accepted`，回到遊戲） | network-architecture §4「重新加入走新的 join」 |

只記錄：

- `connection summary` 的 `interruptions`、`max_gap_ms`、`transient_socket_errors`（Windows 上 WSAECONNRESET 的推論，D172 D-7，第一次實機確認）。
- 有沒有 `session rebound`（endpoint 改變；關 Wi-Fi 重連時可能換埠），以及 `silent_ms`。
- `gap_ms` 與 `connection failed` 相對 `connection interrupted` 的時間差。拔線的時刻沒有 log，所以不用 outage L2 的 [4980, 5060] 判定（那是用中繼 `relay.json` 量的）。

## 4. S3：關閉視窗後重新加入；room_full（v7 L3 的補做）

| 代號 | 判定 | 通過條件 | 來源 |
|---|---|---|---|
| Q1 | 關閉視窗後重新加入 | 一位 Windows 玩家直接關閉視窗（不按 Esc），再啟動 Client 加入：Gateway 有該玩家的 `player left` 或 `player expired`，之後新的 join／hello，回到遊戲。其他玩家不斷線 | v7 L3-4（`v7/STABLE_BASELINE.md:140`）；network-architecture §4 |
| Q2 | 房間已滿 | 房間 4 人時，第 5 個加入被拒：Gateway 有 `join refused … code=room_full`，那個 Client 顯示錯誤並留在 Lobby；房間內的 4 人不受影響 | `server.go:344-345`（`MaxPlayers`＝4）；protocol-v7（409 `room_full`） |

- Q2 要湊滿 5 個加入。人數不夠時，用 Mac 上多開的 Client 補（同一個套件，各用自己的 trace 檔名）。補人的方法等人數定了再寫進本節。

## 5. S5、S6、S7

- **S5（只記錄）**：Client 與 Gateway 的 clock sync 行（時間回聲），每位玩家在 S1 範圍內的偏移與漂移（首尾差，單位 µs／分鐘）。
- **S6（只記錄）**：握手期（`join` 到第一個 Welcome）的 `transient_socket_errors` 與錯誤種類（K9、W6）。
- **S7（L3，使用者判定）**：新的人物模型（D234）在 Windows 與 Mac 上看起來正常（動畫、法線、頭髮、沒有缺面或閃爍）。log 核對：各 Client 的 log 沒有模型或 asset 載入錯誤（`error` 或 `failed` 且提到 asset／model 的行）。

## 6. 收支與整體判定

- **收支**：每台機器的 log 與紀錄檔都要齊（Match：match.log、`match-commands.jsonl`、`match-ingress.jsonl`、`match-shots.jsonl`；Gateway：gateway.log；每台 Client：`logs/` 整個資料夾）。缺任何一份，判定該項為「確認不了」。
- **判定**：
  - S1：J1～J5 全部通過才算通過。
  - S2：每台每個 R 項都通過才算通過。
  - S3：Q1、Q2 都通過。
  - S7：使用者判定。
  - 有任何一項不通過，不重跑、不修改，照實記錄並停下，交使用者決定（§14 開頭）。
- **v7.1 的發行條件**：S1、S2、S3、S7 都通過。S4 未量（D232），S5、S6 只記錄。

## 7. 範圍的限界（照 HANDOFF「後續宣告要寫的範圍的限界」，直到解消）

- Windows 的 symlink 案例（MinGW 與沒有開發人員模式的 MSVC 都提早返回）。
- `os.console_close`（CI 與使用者的機器都 Skipped）與 Ctrl+Break（使用者的鍵盤沒有 Break 鍵）。
- quick-close 量的是產品的網路層，不是整個 Client 程序（D202）；`close_timing` 不在 CI 跑（D204）。
- ACP 非 UTF-8 時的行為在 manifest 之下不再測得到（D212）。
- OS 的 ★ 4 批已出貨、未經功能驗收（D219）。
- Windows 上 worker 驗收不算發行條件（D231）：沒有 SDL 的程序裡 Client 的重送約 43～53 Hz；mock 遇到 WSAECONNRESET 會讓驗收中止（約 5%）；「a token wait outlived four commands」以 mock 輪詢的約 1 ms 刻度量；D226 的 2 次計時器喚醒來源不明；ACK 檢查的相位問題沒有量；D227 只在 macOS 用突變確認過。
- S4（Windows Match 的 Tick 精度，F7）這次未量：Match 在 Mac（D232）。
- 同機的延遲門檻（產生到 Actual 50／66.7 ms、首次送出 22 ms）在 LAN 上不適用：跨機器的 trace 不是同一個時鐘。
- 真實互聯網（WAN）不在範圍。
- 樣本：一次場次，人數 2～3 人（待定）。

## 對抗式檢查的結果（2026-10-29，xhigh-reviewer 1 人，讀 `e5443624`）

blocker 2、major 11、minor 14。主會話抽查了 B1（`action_delivery.go:240-242`、`edge-actions-passthrough-l2.md:95`）、M1（`IpcHost.cpp:713`、`server.go:763`、`:938`）、M5（`LogFile.cpp:151`）的證據，都成立。第 2 版照下面處理；標「待決」的三項由使用者決定。

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

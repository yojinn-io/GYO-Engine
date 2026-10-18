# edge-rate-limit 的 L2 事前宣告（第 3 版）

- 狀態：**已核准（D123，2026-10-18）**，在使用者不用電腦的時段跑，使用者說要離開時才開始。第 1 版經 D45 的門檻來源對抗式檢查（1 位 xhigh，約 12 分）：必須修正 2、應修正 5、記錄 6，對應表在第 7 節。M1 已由 D122 決定（採 (a)，第 6 節第 1 項）。第 3 版＝第 2 版＋D122 之後的 A' commit、凍結清單、工具與乾跑的結果（第 8 節）；判定與門檻和第 2 版相同。§14.1：核准前不看 A' 的結果；核准後不放寬。
- 依據：D114、D117、D118、D119、D121、D122；protocol-v7 §2a、§4、§9；README `edge-actions-passthrough` 與 `edge-rate-limit` 列。
- 這份宣告取代 [edge-actions-passthrough 的 L2 事前宣告第 4 版](edge-actions-passthrough-l2.md)（D119：不沿用第 4 版，也不加臨時的 K1 規則）。第 3 版的 session 停在 `rapid-edge-30-r1-B`（rapid runner 收尾競態，`pvp-v71-edge-actions-l2-20261017/`），第 4 版的 session 停在 `gateway-1000ms-r4-B`（K1，`pvp-v71-edge-actions-l2-20261018/`），都保留。**以下沒有寫出的部分，照第 4 版**（含其第 11～13 節的 D45、乾跑與 D117 的修正）；改動只有第 1～5 節列出的地方。
- 數字沒有標「實測」的，是程式推導或推論。門檻裡的「B 的範圍」一律用本 session 的 B' 跑次計算（第 4 版的 N1）。

## 1. 目的

一次 session 驗兩件事：

1. **`edge-actions-passthrough`**（第 4 版第 1 節）：Gateway 不再保存動作視窗，每個動作 datagram 原樣成為一則 IPC 訊息。判定同第 4 版。
2. **K1 的修正**（D121）：通用層 session 的限速由固定 1 秒窗口改成 token bucket（每秒補 120、容量 120）。Gateway 自己停頓 1 秒後，不再把緩存的合法封包當成超速而丟掉。兩棵樹都帶這個修正，所以它不進 A'−B' 的比較，而是以第 3 節的 K1a、K1b 在兩棵樹各自判定。

## 2. 兩棵樹

| 樹 | commit | worktree |
|---|---|---|
| before（B'） | `1802f08`＝`f79bb10`＋D121 的限速修正（本機分支 `claude/pvp-v71-l2-b-ratelimit`，不推送） | `../GYO-Engine-rl-before`（新建） |
| after（A'） | `ed3481c`（之後的 commit 只改文件） | `../GYO-Engine-ap-after` |

- B'→A' 的程式差分：`edge-actions-passthrough`（含 xhigh 審查的修正）、D117 的 Match `IpcHost::Stop()` 記錄 session 結束、`queuedControl.at` 刪除（D120）、`runtime_v7.proto` 的 reserved 17、21、25、26、測試、突變與文件。runner 與分析器兩棵樹共用主 checkout 的凍結版本（3.2；D122 的修改在其中），不屬於樹的差分。Client 不變（工具階段已核對：C++ 只改 `src/Pvp/IpcHost.cpp`，它屬於 Match 的 `ipc` 程式庫，Client 不連結；`runtime_v7` 是 Gateway↔Match 的 IPC）。
- probe 與 arena：同第 4 版（`../GYO-Engine-ip-before`）。rapid 的 GUI probe 用 B' 樹的建置。

## 3. 判定的改動

### 3.1 新增（逐跑次，任一棵樹不成立就停）

| 代號 | 判定 | 適用 | 門檻、資料來源與來源 |
|---|---|---|---|
| K1a | 不限速丟棄 | 兩棵樹的**所有項目** | 每個 Gateway 日誌檔、每位玩家：`player ingress statistics` 行（逐窗口相加，含 `final=1`）的 `drop_rate_limited_{hello,input,actions,other}_packets` 全部是 0；四個鍵在每一行都要有，缺鍵＝工具無效（drop report 是累計值、不印 0 的鍵，不用它）。來源：真實重放（`gateway/testdata/k1-arrivals.txt`）修正後 0 拒收（修正前 53，與那次 Gateway 的計數相同）。修正前的實測：第 3、4 版兩次 session 的 gameplay gateway-1000ms 10 跑次，player 1 每跑次被丟 48～53 個（輸入 31～36、動作 15～16、Hello 1），player 2 每跑次 2～4 個輸入；backpressure 每位玩家 0～2、每跑次 0～4（8 跑次中 6 次不是 0，只在 Gateway 層）；其他情境都是 0。重放模型（relay 的轉送時間，停頓中的在恢復時讀到）和 Gateway 的實際計數在 20 跑次、兩位玩家都一致；在 token bucket 下最少還剩額度 player 1 27.5～30、player 2 58.5～60，也就是停頓要比實測（1002～1010 ms）再長約 0.3 秒，K1a 才會因停頓本身不成立 |
| K1b | 恢復後不再挨餓重設 | 兩棵樹的 gameplay gateway-1000ms | 資料：`match-commands.jsonl` 中 `kind`＝`reset`、`reset_reason`＝`starvation` 的紀錄，時刻 `time_ns` 和 `result.json` 的 `fault.start_ns`、`release_ns` 同一個時鐘（`judge_c2.d61_resets` 的讀法）；缺 fault 或缺 trace＝工具無效。判定：每位玩家在 `release_ns` 之後沒有 starvation 重設。正向對照（不成立＝工具無效，不算判定）：每位玩家在 (`start_ns`, `release_ns`] 之間**必須**有恰好 1 次 starvation（修正前 20 跑次全部有，都在停頓開始後 525～532 ms）。來源：修正前第 3、4 版兩次 session 的 10 個 gameplay gateway-1000ms 跑次，player 1 都在恢復後 0.933～1.005 秒有第二次 starvation（實測），由限速丟棄造成（重放）；對照的 host-ipc-1000ms 只有停頓中的 starvation，network 沒有非重生重設（實測） |

R1 的 D44 事件（E1：窗口外的非重生重設）照第 4 版，不放寬。

### 3.2 改用重新凍結的分析器（`edge-analyzers`）

R6 的 ingress 判定改用 `pvp-v71-ingress-3`。凍結清單是 `pvp-v71-edge-rate-limit-20261018/frozen-pv7-rl.sha256`（＝`edge-analyzers` 的 `frozen-pv7-ea.sha256` 加上 D122 改的 7 個檔：`gameplay_evidence.py`、`gameplay_soak_evidence.py`、`action_probe.py` 的限速檢查改成拒收數＝0，以及固定 `action_probe.py` 雜湊的兩個 runner 和兩個測試）。兩棵樹的期望：

| 檢查 | B'（`f79bb10` 的 Gateway：帶到 edge-results 的鍵組） | A'（帶到 edge-actions 的鍵組） |
|---|---|---|
| IG、gateway_global、T3、I0、I2、J5a、S1、S2、trace（rapid 允許缺檔） | passed | passed |
| `removed_paths` | passed | passed |
| `link_shots` | passed | 不適用 |
| `link_actions` | 不適用 | passed |
| I3 | 不適用 | 不適用 |
| I1 | 同第 4 版（passed；D110 的形狀與宣告的情境允許 undetermined） | 同左 |
| I1a | 不適用 | 同 I1 的規則 |

- 分析器的判定**逐個 Gateway 日誌檔**（network-impairments 的 `gateway-reconnected.log` 也判）。
- 第 4 版的 R6a（Gateway 的動作封包守恆）由分析器的 `link_actions` 取代。
- 第 4 版的 R6b（Match 收到＝Gateway 寫出）由 I1a 取代。I1a 的允許狀態逐條：passed；undetermined 只在下列情形允許（只記錄）：(i) 第 4 版 I1 允許 undetermined 的三類（backpressure／host-ipc-1000ms、host-ipc-1000ms、network-impairments）；(ii) D110 的形狀（只有 `eof_before_final` 不成立，其他前提成立）；(iii) 有寫出失敗時放棄的動作批（`no_abandoned_batch` 不成立，同第 4 版 R6b「前提不成立＝只記錄」）。其他 undetermined 或 failed 都停。R6b 原本的「清掉的批＝0」前提拿掉不算損失：關閉時清掉的批從未寫出，不影響「寫出＝收到」。
- R6c（relay→Match）保留在 judge，前提改成自己寫明：只判 A' 的 gameplay；Match 的 `ipc closed` 至多一行；整個跑次所有玩家的 `drop_cleared_on_close_actions_packets`、`drop_abandoned_on_close_actions_packets` 都是 0；Gateway 全域的 datagram 丟棄（undecodable、other version、room unavailable、unknown session）都是 0。
- 第 4 版的「佇列溢出時不判 I0」取消：`pvp-v71-ingress-3` 的 I0 已把 FIFO 溢出的命令算進出口。
- 第 4 版 R6 的鍵存在規則照舊（`removed_paths` 另外判它們是 0）。

### 3.3 參考值

B' 的限速和第 4 版的 B 不同，gateway-1000ms 的參考值（Expired 3～4、`to_tick` 的範圍等）不再適用；這個情境的門檻只用本 session 的 B' 跑次計算。其他 gameplay、quad、network、rapid 情境的限速不起作用（第 4 版兩次 session 的限速丟棄都是 0，實測），參考值照第 4 版。backpressure（timing probe 的 Gateway 與 host-ipc 停頓）在修正前有 0～2 個（實測），見 P8。

## 4. 預測（不是停止條件，新增）

| 代號 | 項目 | 預測 | 推翻條件 |
|---|---|---|---|
| P6 | gateway-1000ms 的 Expired | B'、A' 都不比修正前（第 3、4 版兩次 session 的 20 跑次都是 3～4，其中故障造成的 2～3）多；這只是預測用的參考，判定的門檻照 3.3 只用本 session 的 B' | 任一棵樹的中位數 > 4 |
| P7 | `max_session_window_packets`（Gateway 的診斷，固定 1 秒窗口的計數） | gateway-1000ms 約 170（player 1；重放預測 168～173），其他情境 ≤120 | 只記錄（D122：凍結分析器不再判它）。B' 開發跑次的實測（第 8 節）：gateway-1000ms 159、159，backpressure 的 Gateway 層 121，都和預測不同；預測不改，照實記錄 |

## 5. 情境、次數、順序、停止規則

同第 4 版第 5、8 節（168 項，約 2 h 46 分；受干擾累計 3 次就停；不重跑、不補跑）。乾跑前在 B' 跑開發跑次：gameplay gateway-1000ms 2 次（確認 K1a、K1b 在 B' 成立）、clean-60 1 次、backpressure 1 次（舊 corpus 的 backpressure 有修正前的限速丟棄，K1a 會判它不成立，fixture 改用 B' 的跑次）；rapid 的 fixture 沿用第 4 版的 B 開發跑次（Client 與動作路徑相同，限速在 rapid 不起作用，乾跑時核對）。B' 的開發跑次若 K1a 或 K1b 不成立，就停下、在核准前回報。

## 6. 等你決定

1. **M1：凍結分析器的 Gateway 窗口計數檢查**——**已決定（D122：採 (a)）**，以下保留當時的說明。`gameplay_evidence.py:210`、`gameplay_soak_evidence.py:377`、`action_probe.py:353` 要求 `max_session_window_packets ≤120`（固定 1 秒窗口內 Gateway 讀到的封包數）。限速改成 token bucket 後，停頓時緩存的封包在恢復時同一秒放行，gameplay gateway-1000ms 的這個值會到約 170（重放：修正前 20 跑次全部會超過），R1 在兩棵樹的第一個 gateway-1000ms 就停。來歷：同一處的「限速拒收＝0」在 gateway 模式刻意不判，`action_probe.py` 的說明寫「刻意停住的 Gateway 可能拒收排隊的到達突發」，也就是 K1 當年被當成預期行為接受。Client 自己的送出速率另有 `gameplay_evidence.py:205`（relay 的時間、每秒 ≤120）判定。選項：
   - (a)（建議）三處的窗口計數檢查換成限速器自己的拒收計數：`rate_limited_packets`＝0，**gateway 模式也判**（與 K1a 一致）；`max_session_window_packets` 改成只記錄。窗口計數原本的用意是「Gateway 的固定窗口沒有咬到」，在 token bucket 下直接的檢查就是拒收數；Client 的預算仍由 205 行判。改凍結的分析器，補測試、突變，重新凍結（§14.4：受入判定的變更要經你同意）。
   - (b) 三處的上限改成契約上限 240（120·1＋120）：仍是間接檢查，而且比現在寬。
   - (c) 分析器不動，judge 只在 gateway-1000ms 把「錯誤恰好只有這一條」記為預期：規則窄，但留下一條在新算法下沒有意義的檢查。
2. **核准本宣告**（第 3 版：M1 處理、工具與乾跑之後的版本；工具的雜湊在第 8 節）。
3. **什麼時候跑**：約 2 h 46 分加閒置門檻；rapid 需要顯示器亮著、不碰滑鼠。

## 7. D45 檢查的修正對應（第 1 版 → 第 2 版）

| 檢查項 | 修正 |
|---|---|
| M1 凍結分析器的窗口計數會讓 R1 在每個 gateway-1000ms 停下 | 第 6 節第 1 項，等你決定 |
| M2 R6c 的前提指向已刪的 R6b | 3.2 寫明 R6c 自己的前提 |
| S1 network-impairments 的第二個 Gateway 沒判 | 3.2：分析器逐檔判 |
| S2 「I1a 同 I1 的規則」不清楚 | 3.2 逐條寫出 I1a 允許的狀態 |
| S3 K1a、K1b 的資料來源與空判 | 3.1 寫明來源、缺鍵或缺檔＝工具無效；K1b 加正向對照（停頓中必須恰好 1 次） |
| S4 K1a 範圍太窄 | 3.1：K1a 擴到所有項目；原 P8（backpressure）併入 K1a |
| S5 引用的數字 | 3.1 更正（輸入 31～36、動作 15～16、恢復後 0.933～1.005 秒、backpressure 每跑次 0～4、player 2 的 2～4、重放模型的驗證與剩餘額度）；第 3 版、第 4 版的 session 分開寫 |
| N1 `release_ns` 的邊界 | 沒有問題（SIGCONT 之後立刻記下；K1 類的 starvation 最早在恢復後 520 ms） |
| N2 P6 與 3.3 矛盾 | P6 寫明只是預測用的參考 |
| N3 I0 的例外 | 3.2：取消 |
| N4 分析器的期望表 | 與實際輸出一致（第 4 版第 2 次 session 的全部目錄） |
| N5 R6b 的「清掉＝0」前提 | 拿掉不算損失（3.2） |
| N6 檢測力 | L2 只抓得到容量小於約 92 的退化；更小的由 L1 的 `v71rl-*` 覆蓋 |


## 8. 工具與乾跑（第 3 版新增）

工具在 `build/target/_build/test/logs/pvp-v71-edge-rate-limit-l2-prep-20261018/`（由第 4 版的工具複製後修改；確切指令在 `commands.txt`，檔案清單 `prep-files.sha256`）。

| 項目 | sha256 |
|---|---|
| `run_l2.py` | `a7e3b63db2d3a1c9945845256da15439e1087a5e002a8bcdd044c0cad51a5312` |
| `judge_l2.py` | `4244f6ac24890447877c86e51816d0d2c48aaed171f273c0912bcba52ebd72c2` |
| 凍結清單 `frozen-pv7-rl.sha256` | `09b5579b4f5ad34ace567d0553af2ef7087b631540bf28fba63e62eef66a8693` |
| `artifacts.sha256`（B'、A' 的 Match 與 Gateway；P 的 probe 與 arena；G＝B' 的 GUI probe） | `7bf29f7a827c84cd819b5e0d9a273a1a9eae6d97f19c9484b3300462b88e9f6b` |
| `ingress_evidence.py`（`pvp-v71-ingress-3`） | `567198d660985d862e01b49b481a57c0393bf4511267fec1b6787de831008302` |
| `run_action_short.py`（D117） | `89e54bb8b0850327d99f3fd721cd2d2d1511deea13312de6d91cc479b6aee74b` |

judge 的改動（對照第 4 版的工具）：兩棵樹改成 B'、A'；3.2 的分析器期望表逐個 `gateway*.log` 判（各配同目錄的 `match.log` 與 trace），I0 一律判；R6a、R6b 刪除（由 `link_actions`、I1a 取代）；R6c 用自己的前提；新增 K1a（所有項目）、K1b（gateway-1000ms）；P6、P7 只記錄。

**B' 的開發跑次**（`dev/`，只跑 B'）：

| 跑次 | 結果 | K1a | K1b（每位玩家：停頓中、恢復後） | Expired | P7 |
|---|---|---|---|---|---|
| gateway-1000ms r1 | P，無停止 | 0 拒收 | 1、0（停頓中的在恢復前 472 ms） | 3 | 159 |
| gateway-1000ms r2 | P，無停止 | 0 拒收 | 1、0（恢復前 479 ms） | 2 | 159 |
| clean-60 r1 | P，無停止 | 0 拒收 | 不適用 | 0 | 70 |
| backpressure r1 | P（2/2），無停止 | 0 拒收 | 不適用 | — | 121（Gateway 層） |

rapid 的 fixture 沿用第 4 版的 6 個 f79bb10 開發跑次（K1a 0 拒收）。

**用新 judge 重判之前兩次 session**（第 3 版 `pvp-v71-edge-actions-l2-20261017`、第 4 版 `-20261018`，共 131 項，修正前）：停止只有 gateway-1000ms 的 K1a、K1b，backpressure 的 K1a（都是修正前的限速丟棄），以及第 3 版 session 的 rapid 停止（D117 修正的收尾競態）。其他項目全部符合 3.2 的期望表，network-impairments 的兩個 Gateway 日誌都是。

**乾跑**（`dryrun/matrix.json`）：逐項 96 列、driver 檢查 10 項（含 preflight）、模擬 session 5 個，**全部符合預期**。新增的列：`link_actions` 不一致、I1a 不一致、I1a 有放棄的批（允許）、R6c 前提不成立（只記錄）、B' 的 `link_shots` 不成立、只有 `gateway-reconnected.log` 不成立、K1a 拒收（B'、A'）與缺鍵、K1b 恢復後的 starvation、正向對照 0 次與 2 次、缺 fault，以及修正前的真實 gateway-1000ms 跑次（K1a、K1b 都停）。真實 B corpus（`pvp-v71-edge-results-l2-20261017b` 的 A＝f79bb10 共 60 跑次，加 10 個開發跑次）：停止只在修正前的 10 個 gateway-1000ms、backpressure 跑次（K1a、K1b），開發跑次 0 停止。

## 9. 結果（2026-10-18，D123 核准後）

Session `build/target/_build/test/logs/pvp-v71-edge-rate-limit-l2-20261018/`（判定 `judgement.json`、`judgement.md`）：**168 項全部跑完，沒有停止，完整性問題 0，通過（L1～L4 都成立）**。受干擾的項目 2 個（`rapid-edge-144-r2-B`、`network40-r2-B`，未達累計 3 次）。

| 判定 | 結果 |
|---|---|
| K1a | 168 項全部 0 拒收（兩棵樹） |
| K1b | gateway-1000ms 10 跑次全部成立：每位玩家停頓中恰好 1 次 starvation（恢復前 474～484 ms），恢復後 0 次 |
| R1～R9、R6 的分析器期望表、R6c | 逐項全部成立 |
| L1（to_tick p95，A−B） | 全部成立；gateway-1000ms 0.480 ms（門檻 2.414，本 session 的 B' 範圍） |
| L2（Expired） | clean-60、loss-*、soak 兩棵樹全部 0；host-ipc-1000ms B、A 都是 1×5；gateway-1000ms B [2,3,3,3,3]、A [2,3,2,3,3] |
| L3 | B 32、A 35 項合格；A 的回報裡原因 33：未驗證（沒有發生） |
| L4 | 12 格全部 4 次 P、送出 ≥123，Cooldown 0 |

預測（只記錄）：P1～P4、P6 都沒有被推翻（P6：gateway-1000ms 的 Expired 中位數 B'、A' 都是 3）。P7：gateway-1000ms 156～160（預測約 170），backpressure 的 Gateway 層 120～122（預測 ≤120，有 9 次超過 1～2），和開發跑次一致；只記錄。

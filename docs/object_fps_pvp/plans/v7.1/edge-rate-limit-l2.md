# edge-rate-limit 的 L2 事前宣告（第 1 版草稿）

- 狀態：**草稿**。接下來做 D45 的門檻來源對抗式檢查、工具與乾跑，之後才請使用者核准（§14.1：核准前不看 A 的結果；核准後不放寬）。
- 依據：D114、D117、D118、D119、D121；protocol-v7 §2a、§4、§9；README `edge-actions-passthrough` 與 `edge-rate-limit` 列。
- 這份宣告取代 [edge-actions-passthrough 的 L2 事前宣告第 4 版](edge-actions-passthrough-l2.md)（D119：不沿用第 4 版，也不加臨時的 K1 規則）。第 4 版的第 2 次 session 停在 `gateway-1000ms-r4-B`（K1），保留在 `pvp-v71-edge-actions-l2-20261018/`。**以下沒有寫出的部分，照第 4 版**（含其第 11～13 節的 D45、乾跑與 D117 的修正）；改動只有第 1～5 節列出的地方。
- 數字沒有標「實測」的，是程式推導或推論。門檻裡的「B 的範圍」一律用本 session 的 B' 跑次計算（第 4 版的 N1）。

## 1. 目的

一次 session 驗兩件事：

1. **`edge-actions-passthrough`**（第 4 版第 1 節）：Gateway 不再保存動作視窗，每個動作 datagram 原樣成為一則 IPC 訊息。判定同第 4 版。
2. **K1 的修正**（D121）：通用層 session 的限速由固定 1 秒窗口改成 token bucket（每秒補 120、容量 120）。Gateway 自己停頓 1 秒後，不再把緩存的合法封包當成超速而丟掉。兩棵樹都帶這個修正，所以它不進 A'−B' 的比較，而是以第 3 節的 K1a、K1b 在兩棵樹各自判定。

## 2. 兩棵樹

| 樹 | commit | worktree |
|---|---|---|
| before（B'） | `1802f08`＝`f79bb10`＋D121 的限速修正（本機分支 `claude/pvp-v71-l2-b-ratelimit`，不推送） | `../GYO-Engine-rl-before`（新建） |
| after（A'） | `7bb1f84`（之後的 commit 只改文件） | `../GYO-Engine-ap-after` |

- B'→A' 的程式差分：`edge-actions-passthrough`（含 xhigh 審查的修正）、D117 的 Match `IpcHost::Stop()` 記錄 session 結束、`queuedControl.at` 刪除（D120）、`runtime_v7.proto` 的 reserved 17、21、25、26、測試、突變與文件。Client 不變（工具階段核對）。
- probe 與 arena：同第 4 版（`../GYO-Engine-ip-before`）。rapid 的 GUI probe 用 B' 樹的建置。

## 3. 判定的改動

### 3.1 新增（逐跑次，任一棵樹不成立就停）

| 代號 | 判定 | 適用 | 門檻與來源 |
|---|---|---|---|
| K1a | 停頓後不限速丟棄 | 兩棵樹的 gameplay gateway-1000ms | 每位玩家（整個跑次，含 `final=1` 行）的 `drop_rate_limited_{hello,input,actions,other}_packets` 全部是 0。來源：真實重放（`gateway/testdata/k1-arrivals.txt`）在修正後 0 拒收（修正前 53，與那次 Gateway 的計數相同）；第 4 版兩次 session 的 gateway-1000ms（10 跑次）每跑次 player 1 被限速丟掉 48～53 個（輸入 33～36、動作約 15、Hello 1；實測，修正前） |
| K1b | 停頓後不再挨餓重設 | 兩棵樹的 gameplay gateway-1000ms | 每位玩家在 Gateway 恢復之後（`release_ns` 之後）沒有 starvation 重設；停頓中的 starvation（原本每位玩家 1 次）照舊允許。來源：修正前每跑次 player 1 在恢復後約 0.93～0.98 秒都有第二次 starvation（第 4 版第 2 次 session 的 8 個 gateway-1000ms 跑次全部，實測），由限速丟棄造成（HANDOFF 2026-10-18 05:02 條目的重放） |

R1 的 D44 事件（E1：窗口外的非重生重設）照第 4 版，不放寬。

### 3.2 改用重新凍結的分析器（`edge-analyzers`）

R6 的 ingress 判定改用 `pvp-v71-ingress-3`（凍結清單 `pvp-v71-edge-analyzers-20261018/frozen-pv7-ea.sha256`）。兩棵樹的期望：

| 檢查 | B'（`f79bb10` 的 Gateway：帶到 edge-results 的鍵組） | A'（帶到 edge-actions 的鍵組） |
|---|---|---|
| IG、gateway_global、T3、I0、I2、J5a、S1、S2、trace（rapid 允許缺檔） | passed | passed |
| `removed_paths` | passed | passed |
| `link_shots` | passed | 不適用 |
| `link_actions` | 不適用 | passed |
| I3 | 不適用 | 不適用 |
| I1 | 同第 4 版（passed；D110 的形狀與宣告的情境允許 undetermined） | 同左 |
| I1a | 不適用 | 同 I1 的規則 |

第 4 版的 R6a（Gateway 的動作封包守恆）由分析器的 `link_actions` 取代，R6b（Match 收到＝Gateway 寫出）由 I1a 取代；R6c（relay→Match）照第 4 版保留在 judge。第 4 版 R6 的鍵存在規則照舊（`removed_paths` 另外判它們是 0）。

### 3.3 參考值

B' 的限速和第 4 版的 B 不同，gateway-1000ms 的參考值（Expired 3～4、`to_tick` 的範圍等）不再適用；這個情境的門檻只用本 session 的 B' 跑次計算。其他 gameplay、quad、network、rapid 情境的限速不起作用（第 4 版兩次 session 的限速丟棄都是 0，實測），參考值照第 4 版。backpressure（timing probe 的 Gateway 與 host-ipc 停頓）在修正前有 0～2 個（實測），見 P8。

## 4. 預測（不是停止條件，新增）

| 代號 | 項目 | 預測 | 推翻條件 |
|---|---|---|---|
| P6 | gateway-1000ms 的 Expired | B'、A' 都不比第 4 版的 B（3～4）多 | 任一棵樹的中位數 > 4 |
| P8 | backpressure 的限速丟棄 | B'、A' 都是 0（修正前 0～2） | 任一跑次 > 0 |
| P7 | `max_session_window_packets`（Gateway 的診斷，固定 1 秒窗口的計數） | gateway-1000ms 可以超過 120（限速改成 token bucket 後，緩存的封包會在同一秒被讀進並放行） | 只記錄 |

## 5. 情境、次數、順序、停止規則

同第 4 版第 5、8 節（168 項，約 2 h 46 分；受干擾累計 3 次就停；不重跑、不補跑）。乾跑前在 B' 跑開發跑次：gameplay gateway-1000ms 2 次（確認 K1a、K1b 在 B' 成立）、clean-60 1 次；rapid 的 fixture 沿用第 4 版的 B 開發跑次（Client 與動作路徑相同，限速在 rapid 不起作用，乾跑時核對）。B' 的開發跑次若 K1a 或 K1b 不成立，就停下、在核准前回報。

## 6. 等你決定

1. **核准本宣告**（D45 檢查與乾跑之後的版本）。
2. **什麼時候跑**：約 2 h 46 分加閒置門檻；rapid 需要顯示器亮著、不碰滑鼠。

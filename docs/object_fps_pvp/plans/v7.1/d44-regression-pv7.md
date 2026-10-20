# d44-regression-pv7：pv7 上線後的 D44 回歸（計畫，第 1 版）

- 狀態：**計畫第 1 版核准（D146）；L2 宣告第 2 版等你核准**（第 8 節；D45 對應在第 9 節）。
- 依據：README 的 `d44-regression-pv7` 列（「pv7 上線後的 D44 回歸，範圍涵蓋丟棄回報、三個 edge 行為批與 A1；加入持續延遲（x1、ramp）的樣本」；驗收「—／事前宣告＋D45 檢查／—」；估 10～14 h）、INVENTORY P8、A5、`plans/v7/HANDOFF.md` D44（:89）、D45（:90）、v7 第 16 批的整體回歸（`plans/v7/16-integration-acceptance.md`）。
- 檔位：計畫與工具 medium；宣告、分析 high；D45 檢查 1 位 xhigh（第 7 節 DR-9）。
- file:line 以 `8ecfe17` 為準；最後一個程式 commit 是 `572cc39`。

## 1. 問題

D44（2026-10-09）是第 08 批開發跑次的偶發 epoch 重設（新 worker 1／12），使用者提高嚴重度，要求整體回歸。v7 第 16 批在 v7 最終程式上跑了 260 個 case-run，0 事件，結論是「未再現、未結案」（`plans/v7/STABLE_BASELINE.md:111-129`）。

之後 pv7 改了三個行為（輸入、結果、動作的原樣轉送）、加了丟棄回報、A1 改了射擊判定，Gateway 的限速也換成 token bucket（D121）。v7.1 期間唯一一次 D44 型事件就出在這一段：edge-actions 的第 2 次 session（E1＝1），根因是 Gateway 的固定窗口限速在停頓後丟掉合法輸入（`HANDOFF.md:396`、`:404-409`），已修。

pv7-checkpoint 與 A1 的 L2 都只判了 clean 的非 respawn 重設（R3）與 D61 即時規則（R3f），明確把「D44 事件回歸」留給本批（`pv7-checkpoint-l2.md:14`、D129）。A1 之後又有 13 個 commit（tt2-trace、log-contract、dead-letter-position），所以**目前的建置還沒有被任何 D44 型的 L2 量過**。

本批要回答的問題和第 16 批相同：在目前的程式上，D44 事件「未再現／仍存在（低頻）／要停下回報」哪一種。外加 v7 從沒量過的持續延遲（x1、ramp）。

## 2. 現況（讀程式、文件與證據目錄得到的事實）

### 2.1 事件的定義（第 16 批，全部沿用凍結工具）

- **E1**：遊戲案例或長測中，Match 的非 respawn 重設在故障窗 (start, release＋1.5 s] 之外，或在沒有故障窗的案例（`judge_c2.d61_resets`，D61）。
- **E2**：凍結分析器的「Unexplained epoch reset … no preceding substantive interference」（`command_evidence.py:421`）。
- **E3**：`run_network.py` 的「an application stall reset the movement epoch」。只比較 6 秒停頓前後的 epoch，偵測範圍很窄（D45 檢查 5）。
- 出處：`16-integration-acceptance.md:181-188`。停止條件 3（clean 的非 respawn 重設）、4（D44 事件與 Cooldown 拒絕同時出現）、5（有事件的 case-run 累計 ≥3）在 `:151-160`。

### 2.2 pv7 上已經可用的工具

- A1 的 L2 工具（`logs/pvp-v71-a1-l2-prep-20261019/`）以雜湊載入 `judge_16b.py`，由它載入 `judge_16.py` 與 `judge_c2.py` 的偵測器（`judge_a1.py:15`、`:59-60`、`:88`），已在 pv7 跑次上判 R3f。所以 E1～E3 不用新寫，只要把 A1 的 runner 與 judge 改成本批的項目表。
- 持續延遲的 runner 已經有：`run_client_delay.py`（profile x1、ramp），在 cp 凍結清單裡（`frozen-pv7-cp.sha256:22`，雜湊和目前的檔案相同）。edge-input 的 L2 用過：x1 `--delay-ms 200 --window-ms 2000 --fault-at 7.5`、ramp `--peak-ms 250 --rise-ms 1500 --fall-ms 500 --fault-at 7.5`，各 5 次，E1～E3 全 0；E1 的故障窗規則套用到注入窗（`edge-input-passthrough-l2.md:67-68`、`:85`）。那次是 A1 之前的樹。
- Gateway→Match 的短停頓注入（`run_short_stall.py`）也在 cp 清單裡；要成為常設回歸的正式母體，需要先做 `N-injection-official-mode`（歸 `observability-tools`，排在本批之後）。

### 2.3 持續延遲的讀法限制

- x1、ramp 每跑次在故障窗內固定有 starvation 4＋backlog 1 次重設，由注入決定（`plans/v7/08c-drop-report.md:510`、`:725`）。E1 只抓窗外的重設；窗內的次數不等於 4＋1 時，照網路母體的先例立只記錄的旗標。
- 「單次 40～80 ms、持續 100～300 ms」的短延遲區域（A5 的 300 ms 型延遲消失重設）x1、ramp 碰不到（`plans/v7/09a-input-send-pinning.md:28`）；09R-map 的格點量過（`plans/v7/09b-phase-tracking.md:1018-1031`）。
- 「late 修正變多」要增量計數，屬 `observability-tools`。本批不判，只記錄凍結工具給得出的量。

### 2.4 會影響 L2 的已知問題

- pv6 的凍結工具讀不了 pv7 跑次（README:81）；要和 v7 比，只能用第 16 批的結果當普查基準，不重跑 v7final。
- d117 的舊版 `ingress_evidence` 讀不了 pv7 日誌（D144 的記錄）；本批用 cp 清單（0 差異）。
- `GYO-Engine-a1-before` 現在在 `0fabb52`，不是 A1 L2 宣告的 `7a5e0ea`；本批不用 B 樹時無關。
- `N-movement-recovery-flake` 只在 CI 的 macos-x64（Rosetta）出現。本批在這台 Intel Mac（i9-9980HK、x86_64，執行檔也是 x86_64）上跑，大概和第 16 批是同一台機器（D45 S2c 更正：第 1 版誤寫 arm64）。

## 3. 方案（建議）

**單樹＋普查基準**，照第 16 批 D 部分的母體，加上持續延遲：

| 母體 | 內容 | case-run | 0 事件時的單側 95% 上限 |
|---|---|---|---|
| 遊戲 | 25 案 × 3 輪＋閒置 clean-30／clean-60 各 10 | 95 | 3.1% |
| 背壓 | 6 案 × 20 | 120 | 2.5% |
| 網路一般 | `run_network.py` 30 次 | 30 | 9.5% |
| 網路 impairments | 15 次 | 15 | 18.1% |
| **持續延遲 x1**（新） | 15 次，參數同 edge-input | 15 | 18.1% |
| **持續延遲 ramp**（新） | 15 次，參數同 edge-input | 15 | 18.1% |
| 60 Hz 長測 | 30 分鐘 1 次，另列 | — | — |
| Gateway→Match 短停頓 | T＝2.0／11.5 各 3 次，**只記錄** | — | — |

- 量的是目前的程式（`572cc39` 之後只有文件的樹）。不跑 B 樹：問題是「還在不在」，不是 A／B 差異；v7 的基準是第 16 批的 260 個 case-run（0 事件），v7.1 各 L2 的跑次當參考列（edge-input 的 x1／ramp 各 5 次 0 事件、checkpoint 與 A1 的 R3／R3f）。
- 事件定義、停止條件、結果讀法都沿用第 16 批（2.1 節；結果三分法「未再現／仍存在（低頻）／停下回報」，`16-integration-acceptance.md:207-212`）。改動只有：
  - x1、ramp 計入停止條件 5（持續延遲正是 08c 說暴露上升的地方，`08c-drop-report.md:164`、`:762`）；
  - x1、ramp 窗內重設數不等於 4＋1 時立只記錄旗標；
  - 短停頓只記錄，不計入停止條件，不成為正式母體（避開 `N-injection-official-mode`）。
- 不含第 16 批的 I（整合判定）與 G（GUI）部分：pv7-checkpoint 與 A1 已做過整合判定。
- 有事件時照第 16 批：未達停止條件就記錄並做有限定位（≤30 分，凍結工具列重設前 30 Tick 的量），修法留給 `re-evaluate`；達到停止條件就停下，選項 (a) 列為已知問題繼續、(b) 提前調查（ultracode，D45）、(c) 依「先完成穩定版本」標缺陷版並停下。
- A5（300 ms 型的延遲消失重設、backlog 門檻 105）：本批只量頻率；修法在 `re-evaluate`。
- session 時間（估計）：遊戲＋背壓＋網路＋impairments 約 2.1 h（第 16 批 D 部分 136 分扣掉長測 31 分，加 I 部分第 1 輪約 20 分），x1／ramp 30 次約 0.6 h，長測 0.5 h，短停頓約 0.1 h，合計約 **3.3 h**。

## 4. 工具與步驟

1. **工具**（high，約 3 h）：從 A1 的 `run_a1.py`／`judge_a1.py` 改成 `run_d44.py`／`judge_d44.py`（新目錄 `logs/pvp-v71-d44-l2-prep-<日期>/`）：單樹、本批項目表、x1／ramp 的旗標、停止條件 3～5 與 8。凍結清單沿用 cp（開跑前確認 0 差異）；偵測器直接載入第 16 批的 `judge_16.py`（A1 經 `judge_16b.py` 載入同一個檔案；對本批的 runner 兩者等價），雜湊核對。
2. **開發跑次與乾跑**（約 2 h）：每個母體 1～2 次，確認偵測器在 x1／ramp 上分得出窗內與窗外；乾跑用合成事件確認停止條件 3、4、5 會觸發（突變）。
3. **宣告第 1 版**（high，約 1.5 h）＋**D45 檢查**（1 位 xhigh，約 1 h）→ 宣告第 2 版交你核准。
4. **session**（機器約 3.3 h；L2 期間不呼叫工具）。
5. **分析與紀錄**（high，約 2 h）：結果讀法、和第 16 批並列、STABLE_BASELINE 的 v7.1 補充（或交給 `incident-close`）、HANDOFF。

合計約 **13 h**（工具 3＋開發與乾跑 2＋宣告與 D45 2.5＋session 3.3＋分析 2），在 README 的 10～14 h 內。

## 5. 不做的事

- 加寬偵測器、增量 late 計數、換遺失種子、注入正式模式：都屬 `observability-tools`（INVENTORY `N-d44-network-detector-narrow`、`N-late-count-detector-increment`、E4、`N-injection-official-mode`）。本批用窄偵測器量，報告寫明偵測範圍。
- FireGate 重推、M0：已隨 A1、D90 取消（INVENTORY P8）。
- Windows／Linux 的 E1-5 實機值（log-contract 留下）：本批在這台 Mac 跑，交給 `lan15-windows`（第 7 節 DR-7）。

## 6. 風險

- 0 事件只代表「在這些母體與窄偵測器下未再現」；各母體上限見第 3 節。x1／ramp 15 次的上限 18%，只能排除高頻。
- 重複只抽樣主機時序，不抽樣丟包型態或故障情境（第 16 批 D45 檢查 2）。
- 持續延遲跑次的窗內重設是設計行為；若窗外出現重設，可能是 A5 的途徑，也可能是新退步，要靠有限定位分辨。

## 7. 等你決定

- **DR-1 對照組**：
  - (a) 單樹＋普查基準（建議）：第 16 批 260 個 case-run 當 v7 基準，v7.1 各 L2 當參考列。
  - (b) 加 B 樹（A1 之前 `7a5e0ea` 或 v7final `6d3f287`）：時間約加倍；v7final 只能用 pv6 工具，比較要跨工具。
- **DR-2 母體與次數**：
  - (a) 第 3 節的表（建議；session 約 3.3 h）。
  - (b) 減半（遊戲 25 案 × 1 輪＋閒置各 5、背壓 × 10、網路 15、impairments 8、x1／ramp 各 10；約 1.7 h）：各母體上限約加倍（x1／ramp 10 次 0 事件的上限 25.9%）。
  - (c) (a)＋09R-map 的 300 ms 型格點（A5 的區域；約加 1 h，格點的參數要另訂）。
- **DR-3 偵測器**：(a) 沿用第 16 批的 E1～E3（建議）；(b) 先做 `observability-tools` 的加寬偵測器再跑本批（改 D91 的順序）。
- **DR-4 Gateway→Match 短停頓**：(a) 只記錄（建議）；(b) 當正式母體（要先做 `N-injection-official-mode`）；(c) 不跑。
- **DR-5 停止條件 5 是否計入 x1、ramp**：(a) 計入（建議）；(b) 只記錄。
- **DR-6 有事件時**：(a) 照第 16 批：未達停止條件只記錄並做 ≤30 分的有限定位，達到就停下給 (a)／(b)／(c)（建議）；(b) 一出現就停下。
- **DR-7 Windows／Linux 的 E1-5 實機值**：(a) 交給 `lan15-windows`（建議）；(b) 本批另外安排。
- **DR-8 舊 worktree**：v7final（`6d3f287`）與 c2-after（`d685d1e`）原本保留到本批結束。(a) 保留到 `re-evaluate` 結束（建議：評估可能要回看 v7 的跑次與工具）；(b) 本批結束後歸檔。
- **DR-9 檔位**：宣告前的 D45 檢查 1 位 xhigh（照 v7.1 各批的先例）。

## 8. L2 事前宣告（第 2 版，D45 檢查之後）

本節是 session 的判定規則，看結果之前定下。第 1 版經 1 位 xhigh 的 D45 檢查（MUST-FIX 3、SHOULD-FIX 6、NOTE），對應見第 9 節。和第 16 批不同的地方標「宣告點」。

### 8.1 量測對象與工具

- 樹：主 checkout，量測 commit `572cc39`（最後一個程式 commit；它只改了 Go 測試，之後只有文件）。Gateway 在 `cf40497` 重建（`vcs.modified=false`，是 `572cc39` 只有文件的後代）。Match、probe 由 CMake 增量建置；沒有任何產品原始碼比 Match 執行檔新（D45 NOTE）。
- 工具目錄：`build/target/_build/test/logs/pvp-v71-d44-l2-prep-20261021/`
  - `run_d44.py`：driver。由 A1 的 `run_a1.py` 改成單樹，並加上 client-delay 的 runner（edge-input 的命令列）。
  - `judge_d44.py`：判定。
    - 事件偵測直接載入第 16 批的 `judge_16.py`，它再載入 `judge_c2.py`。
    - trace 完整性與 shot 紀錄的讀取器用 A1 的 `shadow_a1.py`。
    - 記錄用的日誌掃描用 edge-rate-limit 的 `judge_l2.py`。
    - 以上都以雜湊載入。
  - `dryrun.py`：乾跑。
  - 字面值（第 2 版定稿）：
    - `run_d44.py`：9ad263fde395d08cc547360081d32a8e237e60260005c3b4b988799bbbb302e6
    - `judge_d44.py`：d0610c61ac89a980e9652effd147c79974aca508df6f1c164fec9facffe38024
    - `artifacts.sha256`：cf992c74f6288c33c9e4ee7cf78b5cb690f2c6ec5be33d7901cf5b1bb869e25d（37 行：執行檔 5 個、arena 1 個、assets 31 個；寫入時 HEAD `d7dc7f3`，Gateway `cf40497`）
  - `--preflight` 通過（2026-10-21）。開發跑次 10 項以產物清單核對執行檔身分，0 個停止。
  - 開跑命令（輸出目錄 `logs/pvp-v71-d44-l2-<日期>/`，不可已存在）：

    ```bash
    caffeinate -dims python3 -I -B run_d44.py --output ../pvp-v71-d44-l2-20261021 --driver-sha 9ad263fde395d08cc547360081d32a8e237e60260005c3b4b988799bbbb302e6 --judge-sha d0610c61ac89a980e9652effd147c79974aca508df6f1c164fec9facffe38024 --artifacts-sha cf992c74f6288c33c9e4ee7cf78b5cb690f2c6ec5be33d7901cf5b1bb869e25d
    ```
- 凍結清單：
  - pv7-checkpoint 的 `frozen-pv7-cp.sha256`（f036b2af…），和目前的 `build/acceptance` 0 差異。`run_client_delay.py` 讀的是它加上 repo 路徑前綴的副本 `frozen-pv7-cp-rooted.sha256`，逐行核對。
  - `judge_c2` 匯入時會執行 c2-after 的 8 個模組，所以每個項目之前也核對第 16 批 `frozen.sha256`（8263d38c…）裡的 c2after 各行（D45 S3）。
  - 第 16 批 session 的 `judgement.json`（215aa308…）是 v7 基準的來源，以雜湊載入（D45 S2b）。
- 每個項目之前，driver 檢查：
  - STOP-REQUEST 檔；
  - 主 checkout 只有文件的變更、Gateway 的建置資訊；
  - 產物、凍結清單、NAMED 檔，以及 driver 與判定自己的雜湊。

  項目前後各跑一次 5 秒的睡眠探測。
- 開跑前的閒置閘門：連續 3 次睡眠探測的最大值都 <10 ms；不成立時每 30 秒重試，最多 30 分鐘。
- 逾時：
  - 遊戲 900 秒、背壓 1200 秒、網路 900 秒、x1／ramp／短停頓 300 秒。
  - 長測是 1808＋900 秒（D45 S4：edge-input 在 pv7 上的兩次長測各花 1980、1984 秒；第 1 版的 2228 秒餘量太薄）。

### 8.2 項目（197 項，估計約 3.0 h）

排法照第 16 批 D 部分的區塊交錯。沒有 I 部分，所以第 1 輪也排在區塊裡。
- 區塊 k＝1～30：
  - 網路 k；
  - 背壓（k≤20）；
  - 閒置的 clean-30／clean-60 一對（k≤10，順序交替）；
  - x1 與 ramp（k≤15，順序交替）；
  - 25 案（k≤3）。
- 區塊之後：impairments 15 次、短停頓 6 次（只記錄）、60 Hz 長測 1 次（30 分鐘）。

| 母體 | case-run | 0 事件時的單側 95% 上限 | v7 基準 |
|---|---|---|---|
| 遊戲（故障 66、clean 29） | 95 | 3.1%（故障 4.4%、clean 9.8%） | 第 16 批 session：95，0 事件 |
| 背壓 | 120 | 2.5% | 第 16 批：120，0 事件 |
| 網路一般 | 30 | 9.5% | 第 16 批：30，0 事件 |
| 網路 impairments | 15 | 18.1% | 第 16 批：15，0 事件 |
| x1 | 15 | 18.1% | 08c L2：同樣參數，3 棵 v7 樹共 30 次（x1 與 ramp 合計），每次窗內 4＋1、窗外 0（`08c-drop-report.md:308-309`、`:725`）；v7.1 edge-input 5 次，0 事件（A1 之前的樹） |
| ramp | 15 | 18.1% | 同上 |
| 60 Hz 長測 | 1（另列） | — | 第 16 批：1，0 事件 |
| 短停頓（只記錄） | 6，不計數 | — | 無 |

- 基準都是別的建置：第 16 批與 08c 是 v7（pv6），edge-input 是 pv7、A1 之前。所以表中只做並列，不做統計檢定。
- x1／ramp 只有 15 次。0 事件只能排除「每次 ≥18%」的高頻，報告會寫明這一點。
- 時間的來源是本目錄的開發跑次（含睡眠探測）：遊戲約 27 秒、背壓 82 秒、網路 30 秒、impairments 93 秒、x1／ramp／短停頓各 26 秒、長測 1968 秒。`run_d44.py --list-only` 用 A1 較保守的值，合計約 178 分（另加閒置閘門最多 30 分）。

### 8.3 事件（沿用第 16 批，不新訂量法）

- **E1**（遊戲案例、x1／ramp、短停頓、長測）：Match 的非 respawn 重設落在故障窗 (start, release＋1.5 s] 之外；沒有故障窗的跑次，任何非 respawn 重設都算（`judge_c2.d61_resets`）。
  - x1／ramp 的故障窗是注入窗，取自 `result.json` 的 `fault`，從 relay 的 arm 到排空結束：x1 約 2.19 s、ramp 約 2.00 s（`run_client_delay.py:309`、`:318`）。
  - 開發跑次的窗內重設最晚在 release 後 +0.68 s，1.5 s 的寬限還有約 0.8 s 餘量（D45 NOTE）。
- **E2**：凍結分析器錯誤中的「Unexplained epoch reset」。
  - 讀遊戲、x1／ramp、短停頓的 `result.json` 錯誤，以及背壓各案的 `command_evidence`。
  - 長測的分析器沒有這條，所以長測不可能出現 E2（D45 NOTE）。
- **E3**：`run_network.py` 的「an application stall reset the movement epoch」，兩種網路模式都算。
  - 偵測範圍窄：只比較 6 秒停頓前後的 epoch。
- **單位**：case-run，也就是一個遊戲案例、一個背壓案例、一次網路跑次、一次 x1／ramp、長測。一個 case-run 有事件就算 1，不論裡面有幾個事件。
- **短停頓**的事件照樣偵測、列出，但不計數（D146 DR-4）。
- **偵測器在 pv7 上有效**（D45 NOTE）：
  - classify、事件、Cooldown 讀的都是 pv7 runner 自己的輸出。
  - E2 的文字（`command_evidence.py:421`）從 pv6 起沒改過；E3 的文字在 `network_main.cpp:297`。
  - c2-after 的 pv6 模組只是被匯入，沒有拿來分析 pv7 資料。

### 8.4 停止條件（立即停下、保留全部證據、不重跑）

1. **工具失效**（不是判定），以下任一：
   - 任一項目的分類是 E 或 A。
   - 任一 trace 目錄不符 A1 的 T1（D45 M2）：三個 trace 各以 1 行 `trace_end` 結尾、`dropped` 為 0、shot 紀錄數等於 decision 行數。trace 滿了會丟事件，那樣 E1 與停止條件 3 都會變成空檢查。
   - Match trace 缺漏。
   - 日誌裡的執行檔雜湊和產物清單不同。
   - driver 的每項檢查不成立。
   - runner 的程序群組有殘留。
2. clean 跑次出現任何權威 Cooldown 拒絕。
   - clean 跑次：閒置對、25 案中的 clean-60／30／144、長測。
   - 計數取兩個來源中較大的那個：Client 的 `actions.jsonl`（第 16 批的來源）、Match 的 shot 紀錄。
   - A1-1 的「已解釋／無法解釋」只在旁邊記錄。
   - 第 1 版把這條放寬成「無法解釋的 Cooldown」；D45 M3 指出，A1 的這個放寬靠 C9（clean 的退回率＝0）配套，本批沒有。所以回到第 16 批的規則。A1 L2 實測 clean 的權威 Cooldown 是 0（`a1-shot-step.md:237`）。
3. clean 跑次出現非 respawn 重設。
4. 同一跑次同時有 D44 事件與任何權威 Cooldown 拒絕（含已解釋）。
   - D45 M1：A1-1 的兩種解釋，正好就是 epoch 重設造成的 Cooldown。所以第 1 版的「無法解釋」讓這條永遠不會觸發。
5. 有事件的 case-run 累計 ≥3。所有計數的母體都算，含 x1／ramp（D146 DR-5）。
6. 第 16 批的整合判定，本批沒有。
7. 沒有事件的失敗（分類 F）累計 ≥3。
   - impairments 的失敗只立旗標、不計數（第 16 批 D45 檢查 6）。
   - x1／ramp 與短停頓只有 R（完成）或 E 兩種分類，不會進這一條（D45 NOTE）。
8. `STOP-REQUEST` 檔、driver 自己的例外。

### 8.5 使用者旗標與只記錄

**使用者旗標**：列在讀法旁邊，報告時要你看，但不停下。
- clean 跑次出現逐出行或 Gateway session 的限速拒絕（D45 S6）。
  - v7.1 唯一一次 D44 型事件，根因就是限速（edge-actions，D121 修正）。
  - x1／ramp 有限速拒絕時，runner 自己就判注入無效，算停止條件 1；兩者的處理不同。
  - **宣告點**：A1 的 L2 把逐出與限速拒絕當停止條件。本批不停，因為它們不在 D44 事件的定義裡，而且有事件時正好是定位的線索。
- x1／ramp 的窗內重設多於 starvation 4＋backlog 1（D45 S1）。
  - 窗內的 backlog（release 後 +0.37～0.68 s）本身就是 A5 的機制。窗內多出來的重設可能藏著 D44，所以這個母體的讀法會加註「窗內超出、待定位」。

**只記錄**（報告列出，不影響讀法）：
- x1／ramp 的窗內重設比 4＋1 少，或組成不同。
- 網路的非 respawn 重設不等於「每個探針階段 1 次」。
- clean 以外的 Cooldown 拒絕：計數，以及 A1-1 的已解釋／無法解釋。
- clean 以外的逐出與限速拒絕；Match 的動作批拒絕。這些都寫進 `judgement.json` 的 `runs`。
- 受干擾的項目：任一次睡眠探測 ≥10 ms。
  - **宣告點**：A1 的 L2 有「同一情境受干擾 2 次或總共 6 次」就停的規則，第 16 批沒有；本批照第 16 批，只記錄。
  - 事件如果出在受干擾的項目，報告會並列標明。
- 第 16 批的只記錄量：late 修正、領先跳升、被釘住的秒數、Tick 間隔、30 Tick 排隊合計的最大值。
  - 本批在遊戲之外，也對 x1／ramp 計算這些量（D45 S1）。
  - 讀取器在 pv7 日誌上出錯時，只記下例外，不判定。

### 8.6 結果的讀法（第 16 批的三分法）

| 結果 | 意思 | 處理 |
|---|---|---|
| 全部 0 事件 | 在目前的程式上「未再現」，各母體的上限見 8.2 | 記錄。D44 仍不結案：A5 的結構性途徑仍在，交給 `re-evaluate` |
| 1～2 個有事件的 case-run，都不在 clean 跑次 | 「仍存在（低頻）」 | 記錄、保留 trace，做 ≤30 分的有限定位（用凍結工具列出重設前 30 Tick 的量）；修法在 `re-evaluate` |
| 停止條件 2～5、7 | 可能是退步或結構問題 | 停下回報。選項：(a) 列為已知問題繼續；(b) 提前調查（ultracode，D45）；(c) 依「先完成穩定版本」標成缺陷版並停下 |

- session 沒有完成時，讀法前面一律加「INCOMPLETE session（N／197 項）」。
- 有使用者旗標時，讀法之後列出旗標；x1／ramp 的窗內超出會在讀法本身加註。
- 各母體分開和基準並列。合併的上限只在「每個 case-run 互相獨立」的前提下成立，只當參考：重複跑次只抽樣主機時序（第 16 批 D45 檢查 2）。

### 8.7 乾跑（本目錄的 `dryrun/report.json`，報告裡記有判定的雜湊）

- **開發跑次**：10 項，每種 runner 至少 1 項，含 60 Hz 長測。全部沒有停止；x1 的窗內是 starvation 4＋backlog 1，窗外 0。
- **樣本**：由開發跑次複製後改一處。

| 改法 | 預期結果 |
|---|---|
| clean：重設、trace 缺漏、shot 紀錄缺漏、trace_end dropped | 停止條件 3，或停止條件 1（工具失效） |
| clean：無法解釋或已解釋的 Cooldown | 停止條件 2 |
| 故障案例：窗外重設 | 事件；第 3 個 → 停止條件 5 |
| 故障案例：事件＋無法解釋的 Cooldown | 停止條件 4 |
| 故障案例：事件＋「到期 Tick 不是步 Tick」的已解釋 Cooldown | 停止條件 4 |
| 故障案例：只有 Cooldown | 只記錄 |
| 故障案例：窗內重設 | 不是事件 |
| 故障案例：E2 | 事件 |
| 故障案例：trace 缺漏 | 停止條件 1 |
| x1：窗內少一次 | 只記錄 |
| x1：窗內多一次 | 使用者旗標 |
| x1：窗外重設、第 3 個 | 停止條件 5 |
| ramp：窗外重設 | 事件 |
| 短停頓：事件 | 不計數 |
| 網路：E3（第 3 個 → 停止條件 5）、階段重設不符 | 事件；只記錄 |
| 背壓：E2 | 事件 |
| 背壓、網路：失敗 | 停止條件 7 的計數 |
| impairments：失敗 | 不計數 |
| 逐出：clean／故障 | 使用者旗標／只記錄 |
| 逾時 | 停止條件 1 |
| 受干擾 | 只記錄 |
| 執行檔雜湊不符 | 停止條件 1 |
| 讀法：clean 事件／兩個故障事件 | 停下回報／仍存在（低頻） |

- **結果**：樣本與開發跑次 44／44、mock session 5／5、突變 18／18 killed（報告記錄的判定雜湊 d0610c61…、driver 9ad263fd…）。
- **突變**：判定各拿掉或改壞一條規則，共 18 個，全部 killed。
  - 第 1 次跑有 1 個存活：「缺 trace 不算工具無效」。它和 T1 的檢查重疊（T1 也會報 match-commands.jsonl 缺漏），所以拿掉，這裡記下。
  - D45 S5 指出 8 個現有樣本抓不到的突變，都已補上對應的樣本。
- **mock session**（排程的前 2 項）共 5 種：未完成、停止、順序錯、記錄的停止和檔案不一致、沒有閒置閘門。每一種都得到預期的完整性結果或讀法。
- **driver**：每種 runner 的命令列都檢查過；輸出目錄已存在時拒絕（例外，exit 1）。

## 9. D45 檢查的對應（第 1 版 → 第 2 版）

| 代號 | 指出的問題 | 對應 |
|---|---|---|
| M1 | 停止條件 4 只看「無法解釋」的 Cooldown；D44 自己造成的 Cooldown 都會被判成已解釋，所以永遠不觸發 | 改成任何 Cooldown；補樣本「事件＋到期 Tick 不是步 Tick」 |
| M2 | 沒有 trace 完整性檢查：trace 滿了會丟事件，E1 與停止條件 3 可能是空檢查；Cooldown 只讀一個來源 | 加 T1（停止條件 1）；Cooldown 取 Client 與 Match 兩個來源中較大者；補樣本 dropped、缺 shot 紀錄 |
| M3 | 停止條件 2 放寬成「無法解釋」，但少了 A1 的配套 C9 | 回到第 16 批「任何 Cooldown」；不再是宣告點 |
| S1 | x1／ramp 窗內的重設可能藏著 D44；只記錄量沒算 x1／ramp | 窗內超出列為使用者旗標並加註讀法；只記錄量加算 x1／ramp |
| S2 | x1／ramp 的 v7 基準寫「無」不對；judge 輸出的「普查基準」其實是第 16 批之前的普查；機器寫錯成 arm64 | 補 08c 基準；改讀第 16 批 session 的 judgement.json（雜湊）；機器更正為 Intel x86_64 |
| S3 | c2-after 有 8 個模組在匯入時執行，卻沒有核對雜湊 | 每個項目之前核對第 16 批凍結清單的 c2after 各行 |
| S4 | 長測的逾時餘量太薄，而且長測沒有開發跑次 | 逾時改成 1808＋900 秒；補長測的開發跑次 |
| S5 | 乾跑沒有綁在宣告的版本上；有 8 個突變現有樣本抓不到 | 報告記下判定的雜湊；補樣本、mock session 增加到 5 種、突變增加到 18 個 |
| S6 | clean 跑次的限速拒絕與逐出只記錄，D121 的退步會看不到 | 改成使用者旗標；文中寫明 x1／ramp 的處理不同 |
| NOTE | 偵測器在 pv7 上有效；E2 也會套用到 x1／ramp／短停頓；x1／ramp 不會進停止條件 7；工具失效的情況要列出；動作批拒絕要寫進 judgement | 都寫進 8.1～8.5；`runs` 加入 logs 與 cooldown 欄位 |

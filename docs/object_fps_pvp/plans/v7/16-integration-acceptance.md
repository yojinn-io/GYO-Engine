# 第 16 批：整合驗收與升格（縮小版，D76）

狀態：**16-plan、16-L1、16-removal、16-D44T、16-declare 完成；16-L2 正式 session 進行中；16-L3 等使用者；16-baseline 未開始**（2026-10-15）。PR 線 P7（分支 `claude/pvp-v7-p7`）。

- 清單提案：`build/target/_build/test/logs/pvp-v7-16-plan-20261014/proposal.md`（U1～U12 照建議，D80）。
- 16-L1、16-removal、16-D44T 的結果見 HANDOFF 的進度記錄器。
- D45 的 xhigh 檢查：`build/target/_build/test/logs/pvp-v7-16-prep-20261014/xhigh-d45-check.md`（MUST-FIX 4 項、SHOULD 5～11、測試缺口、事實更正全部套用）。

## 16-L2 的事前宣告（2026-10-15 核准，D82；依 D81 照建議）

下面是證據目錄 `build/target/_build/test/logs/pvp-v7-16-prep-20261014/declaration.md`（SHA-256 `1b2d2fbf26dbee80cce38ee33f4ff8b7dc66c5afd56554f778ca698471a380ce`）的全文，和同目錄 `declaration-draft.md` 逐位元組相同；標題的「未核准」等字樣是核准前的寫法，核准紀錄見 HANDOFF 的 D81、D82。

### 第 16 批 L2 的事前宣告（草案 v2，16-declare；D45 的 xhigh 檢查已套用，未核准）

核准時，本檔原樣複製為 prep 目錄的 `declaration.md`，它的 SHA-256 就是「執行步驟」的 `DECLARATION` 字面值。`run_16.py` 在開跑時核對這個字面值並寫進開始紀錄；`judge_16.py` 再核對一次。四個字面值（`--frozen-sha`、`--artifacts-sha`、`--declaration-sha`、`judge_16.DRIVER_SHA`）一律抄本檔，不從手邊的檔案重算。xhigh 的 D45 檢查見同目錄 `xhigh-d45-check.md`；v1 草稿保留在 scratchpad（`pre-xhigh/`）。

#### 白話摘要

- **要做什麼**：在 v7 最終程式（master `6d3f287`，worktree `../GYO-Engine-v7final`）上，一個 session 依序跑三部分（**I → D → G**，D45 檢查 8）：
  1. **I 整合判定（無視窗）**：25 案逐案、背壓 6 案、網路探針、長測短模式、quad（4 人）。quad 與長測短模式是**第一次在 v7 程式上判定**。
  2. **D D44 整體回歸**：把容易出事的案例多跑幾輪，數「解釋不了的 movement epoch 重設」。25 案再 2 輪、背壓再 19 次、網路一般模式再 29 次、閒置的 clean-30／clean-60 各 10 次；接著**網路 `--network-impairments` 15 次（D81 新增）**；最後 60 Hz 長測 1 次（30 分鐘）。
  3. **G 整合判定（會開遊戲視窗）**：1 GUI＋3 bot、GUI 短測（action、player、雙 GUI timing）、2 GUI＋2 bot 三項。放在最後，G 停下也不會讓 D44 回歸作廢。
- **怎麼判**：全部交給事先凍結的 `run_16.py`（執行）與 `judge_16.py`（判定；排程、分類、D61 規則直接匯入 C2 凍結的 `judge_c2.py`）。不重跑、不補做。
  - I 的整合判定不通過就立刻停下，D、G 不跑；**例外**：第 1 輪的失敗如果完全是它自己的 D44 事件造成的（D61 的故障窗外重設、E2、E3），只讓 `passed`＝false，session 繼續（D45 檢查 7）。
- **D44 的結果不是「通過／不通過」**：只分成「未再現」「仍存在（低頻）」「要停下回報」三種，附普查當基準（主要基準＝修好之後的資訊流）。`judgement.json` 的 `passed` 只看整合判定、完整性與給你的旗標。
- **時間**：約 **2.7 小時**（I 約 20 分、D 約 136 分，其中長測 31 分、impairments 約 24 分、G 約 7 分），另加閒置閘門（最少 20 秒、最多 30 分）與判定約 5 分。G 大約在開跑後 2.6 小時開始。
- **使用者不在（D81）**：G 部分約 7 分鐘會在桌面開遊戲視窗。**不要碰滑鼠、觸控板與鍵盤**；螢幕不能鎖定或進入螢幕保護；通知搶走焦點也算受干擾，所以開跑前打開勿擾（專注模式）。量測期間這台機器不做其他工作（含建置、打包、開 PR、推非文件）。
- **需要你決定的**：最後一節 U1～U12。D81 已授權「D45 檢查修正後照建議核准並執行」，所以每一項都寫成「D81 依建議採用」並列出替代方案。

#### 目的

- 依第 16 批清單提案（`pvp-v7-16-plan-20261014/proposal.md`，D80 核准 U1～U12）與 D81：
  - **L2-1～L2-6**：v7 最終程式的整合判定（提案 2-1）。
  - **L2-7＝D44 整體回歸**（提案第 3 節）：在最終程式上，偶發的 movement epoch 重設（D44）是否還在。涵蓋 08c 讓 Match 看得見的 Client→Gateway 遲到；**不涵蓋 D49 第二階段**（`GatewayDropReport`，D73 移到 v7.1）。不做原因調查（D45：真的調查時用 ultracode，v7.1 候選 A3）。
- D81 對網路部分的追加：一般模式 30 次（不變）＋`--network-impairments` 15 次。理由：到目前為止唯一的網路 E3（第 08 批開發跑次）發生在 impairments 模式的 RTT 0 階段，而這個模式從那之後一次都沒有跑過。
- 前面文件交給本批的事：
  - 事故紀錄 `Incident/2026-10-10-gateway-silent-drop.md:222`「第 16 批的整體回歸照 D44 不變，但基準換成修好之後的資訊流」：本宣告的主要基準就是修好之後的資訊流（見「普查基準」）。同一列的「D44 的頻率與歸因重做（R2）」**不在本批**，移到 v7.1 A3（原因調查一起做，D45）。
  - FireGate 重推條件 10（`09-firegate-c2.md:136`）點名第 16 批：本批只量得到其中的「權威 Cooldown 拒絕」（停止條件 2）。「窗口外的 R<earliest」與「R>earliest＋7」量不到：headless 的遊戲案例不經閘門、不記 earliest；GUI 取不到 earliest（`09-firegate-c2.md:172`，D75）。照 C2 宣告的寫法記為盲點。
- 本宣告取代提案第 3-6 節的「門檻來源預覽」；提案的其他部分照 D80。

#### 對象與產物

- **量測對象**：master `6d3f287889289feac50a1c85d1ed4787afa0a4cc`，detached worktree `/Users/karasu/Code/Source/GYO-Engine-v7final`（16-L1 從零建置，`build.sh`；`git status` 空）。
  - 產品程式等於 C2 after（`d685d1e`）：`git diff --stat d685d1e 6d3f287 -- . ':!docs' ':!*.md'` 為空。也等於 `b205615`（D72 矩陣）加註解。
  - 16-L1 和 `../GYO-Engine-c2-after` 正規化比對：C++ 四個執行檔與 8 個產品靜態庫的反組譯 0 差異；Gateway 只差 build info 的 `vcs.revision`／`vcs.time` 與 pclntab；資產 31 檔相同。所以 C2 的證據可以連到本建置。
  - 16-L1 的突變後重建改變了 C++ 執行檔的 SHA-256（連結時間戳與 LC_UUID）；對最終執行檔重做了正規化比對與權威 digest（35／35、0 差分）。**本 session 用最終狀態**。
- **產物清單**：`pvp-v7-16-l1-20261014/artifacts.sha256`（SHA-256 `f0a87dd78827a42dfab75546b69a73785d35442019fca03893775d4824f3b3cc`，46 行）。
  - Match `ce76bfa2…`、Gateway `a7aecefc…`、Client `8b62e42d…`、arena `0026013c…`、digest runner `35608bcc…`／`0589ac8b…`（-O0）、tests、SDL `4a79f345…`；
  - action probe `23d477ab…`、GUI probe `cc404b5e…`、GUI quad probe `18130f1d…`、network probe `b63ff5d0…`、quad probe `7cbd3bf9…`、timing probe `7daea6a8…`、worker probe `44d6c51a…`；部署資產 31 檔。
  - `run_16.py` 在每個項目之前核對全部；不一致就停下。
- **16-L1 的結果**（前提）：L1-1～L1-5、L1-7 全部通過（CTest 69／69、TSan 3／3、權威 35／35、Match 不連 SDL、突變 26／26、master CI 與 snapshot）；L1-6（16-removal）合格。

#### 分析器與腳本

凍結清單 `frozen.sha256`（SHA-256 `8263d38c5497f909477e4893dd5190fe45771ff047da2edc0bcd23e6ecba2272`，117 行），`run_16.py` 在每個項目之前核對清單本身（對宣告字面值）與清單內每個檔案。

- **v7final 的驗收工具 52 檔**：`build/acceptance/object_fps_pvp/*.py` 全部列入（傳遞匯入不必逐一追）。
- **c2-after 的驗收工具 52 檔**（D45 檢查 MUST-FIX 4）：`judge_c2.py` 把 `../GYO-Engine-c2-after/build/acceptance/object_fps_pvp` 放在 `sys.path` 的最前面（`judge_c2.py:77-83`），載入 `judge_16` 時有 10 個 c2-after 的模組被執行（`acceptance_util`、`action_evidence`、`action_probe`、`backpressure_probe`、`command_evidence`、`impaired_network`、`network_statistics`、`presentation_evidence`、`run_network`、`start_phase_evidence`），其中 8 個原本沒有凍結。現在 52 檔全部列入、每項之前核對。準備時 c2-after 與 v7final 的 52 檔逐檔相同（`prep.json`）。
- **匯入的凍結模組 7 行**（`judge_c2.py` 載入時自行再核對 6 個）：
  - C1 的 `summarize.py`（`bbf51ddc…`）、參考包的 `analyze.py`（`68ee027a…`）、`segments.py`（`6a3fa237…`）、`lead_jumps.py`（`14cd5b5c…`）；
  - c2-after 的 `network_statistics.py`（`b37baf63…`）與 `action_probe.py`（`59002b43…`）；
  - C2 的 `judge_c2.py`（`5d2ed0f3af7e5c7618c79cd850f663d6197e57c676fdcef8c9684ab3cac30355`）：D61 的即時與事後規則（基準＝`b205615` 的 D72 矩陣，`judge_c2.D61_BASELINE`）、25 案的分類與判定（`classify_gameplay`、`r_record`）、只記錄的項目、Cooldown 的數法、TimerBaseline 狀態。
- **本批腳本 6 檔**：
  - `run_16.py`（`ebc7e28211f283b749b4a34c4dd2757f971a64b247b1780986253274faf283ff`）；
  - `judge_16.py`（`29258be1ba4e63e761184b59ecbfbad5c646481198c953801861b55f3b6671af`；內含 `DRIVER_SHA`＝`ebc7e28211f283b749b4a34c4dd2757f971a64b247b1780986253274faf283ff`）；
  - `sleeper.py`（C1／C2 版 `98105dbf…`）、`census.py`（`f37ee398…`）、`census.json`（`cc333065…`）、`prep_hashes.py`（`6d63bc72…`）。
  - `judge_16.py` 同時提供 `run_16.py` 用的排程、每項的分類、D44 事件偵測與停止判斷，也提供 `census.py` 用的偵測器，所以執行、判定與普查用同一份程式。
- 事後重跑 `judge_16.py` 需要 `../GYO-Engine-c2-after` 還在（U12 保留）。
- **16-declare 對工具的改動**（D81 與 D45 檢查；改動前的版本保留在 scratchpad 的 `pre-declare/`、`pre-xhigh/`）：
  - 排程：順序改成 I → D → G；D 部分在第 30 塊之後、60 Hz 長測之前連續跑 15 次 `run_network.py --network-impairments`。
  - 分類：impairments 的跑次要通過，`result.json` 的 `network_impairments` 必須正好是 RTT 0、20、40 三步而且都通過；否則 A（旗標沒有生效）。一般模式出現 impairment 的步驟或 `network-rtt-*.log` 也是 A。
  - 第 1 輪的 D44 型失敗（`judge_16.integration_d44_only`）只立旗標、不停下；D 部分 impairments 的非事件失敗只立旗標、不計入停止條件 7。
  - 網路跑次「非 respawn 重設數 ≠ 每個探針階段 1 次」立只記錄的旗標（`record_flags`）。
  - D44 彙總：四個母體（遊戲、背壓、網路一般、網路 impairments）各自的上限，遊戲再分故障與 clean；session 不完整時讀法前面加「INCOMPLETE」。
  - `git ls-remote` 逾時記成 `remote_error`（開跑時停下，結束時由 judge 判問題）。
  - 普查加「修好之後的資訊流」主要基準與網路階段旗標；凍結清單加 c2-after 52 檔。

#### 主機

- **閒置**：
  - session 期間不做開發、不建置、不打包、不開 PR、不推非文件的 commit（`claude/pvp-v7-p7` 目前沒有 PR；如果有人開了，先暫停 auto-fix）。lan-kit-v7 已完成（`1f610b9`）。`caffeinate -dims`；前景沒有其他使用者程式。
  - 開跑前確認螢幕沒有鎖定（`ioreg -n Root -d1` 的 `IOConsoleLocked` 為 `No`；16-declare 時查詢為 `No`，另有一個 `caffeinate` 持有 `PreventUserIdleDisplaySleep`）。
  - **開跑前打開勿擾（專注模式）**：通知或其他程式搶走焦點，GUI probe 會記成受干擾（`presentation_evidence.py:27`），視窗閘門判 `invalid`。主對話無法用指令打開時，記錄「未確認勿擾」照樣開跑；G 在最後，停下也不影響 I 與 D。
- **非文件變更**（停止條件 1）：
  - 主 checkout：開跑時、每個項目之前、結束時，`git diff --name-only 6d3f287` 只能出現文件（`docs/` 下或 `*.md`）。比的是**量測的 commit `6d3f287`**，所以 commit 與未 commit 的變更、核准後開跑前的非文件 commit 都抓得到（C2 的 MUST-FIX 3 的做法）。
  - PR 分支在遠端：開跑時與結束時以 `git ls-remote origin refs/heads/claude/pvp-v7-p7` 取頭（60 秒逾時記成 `remote_error`）。這個頭必須已經在本機，而且和 `6d3f287` 的差異只有文件。開跑時不成立就停下；結束時的值寫進結束紀錄，由 `judge_16.py` 檢查。16-declare 時遠端頭是 `1f610b9`，只有文件（`dryrun/preflight-3.txt`）。
  - 量測的 worktree：每個項目之前 HEAD 必須是 `6d3f287`，追蹤檔沒有變更。
- **閒置閘門**（C1 第 2 次 session 的規則，使用者核准）：第 1 個項目之前連續 3 次 5 秒 sleeper，每次最大值都 <10 ms；不成立就每 30 秒重試，最多 30 分鐘。
- **每個項目前後**各跑一次 5 秒 sleeper，並記錄 CPU 最高的 5 個程序。
- **主機狀態只記錄、不分層**：
  - 遊戲案例記錄 probe 的 TimerBaseline（`timer_baseline.late_p99_ms`）≥6.0 ms 為 8 ms 狀態（`judge_c2.timer_state`），長測與 quad 也照列。
  - C2 用它分層（決定 8 核准，違反 probe 自己的標註「never a verdict, threshold or denominator」）。**本批沒有任何判定用它分層或當分母**，只列在結果旁邊；所以不違反標註。
  - 參考：C2 的同一產品程式在兩種狀態下，clean 案例的「產生→執行」P50 是 37.6～38.0 ms、P95 37.7～38.1 ms，沒有分開（`pvp-v7-09-c2-20261014/runs/clean-*-after`）。

#### 跑次

順序 I → D → G（D45 檢查 8；偏離 D80 的 I → G → D，見 U8）。I 停下時 D、G 不跑；D 停下時 G 不跑。不隨機化；不補跑、不重跑。使用者在 session 目錄放 `STOP-REQUEST` 檔，會在下一個項目之前停下。

- **I（30 項，無視窗）**：
  1. 25 案逐案 `run_gameplay.py --case`（C2 的順序；＝D44 矩陣第 1 輪；clean-30 計入判定，U2）；
  2. `backpressure_probe.py`（timing probe，6 案一次；＝D44 背壓第 1 輪）；
  3. `run_network.py` 一般模式（＝D44 網路第 1 輪）；
  4. `run_gameplay_soak.py --fps 60`（長測短模式，2 循環）；
  5. `run_quad.py --case clean-60 --rounds 5`（判定）；
  6. `run_quad.py --case clean-30 --rounds 5`（只記錄：凍結工具的 `judged: False`，U2 附註）。
- **D（134 項）**：
  - 第 k 塊，k＝2～30，依序：網路一般模式第 k 次；k≤20：背壓第 k 次；k≤11：閒置的 clean-30／clean-60 一對（第 k−1 對；奇數對先 clean-30，偶數對先 clean-60）；k≤3：25 案第 k 輪；
  - 接著：**`run_network.py --network-impairments` 15 次**（`network-impaired-r1`～`r15`；D81、D45 檢查 6）；
  - 最後：60 Hz 長測 `run_gameplay_soak.py --soak --fps 60`（113 循環＝1808 秒，U6）。
- **G（7 項，開視窗，`--gpu-driver metal`）**：
  1. `run_quad.py --case gui-bots`（1 GUI＋3 bot）；
  2. `run_action_short.py`（action30／60／144＋capture）；
  3. `run_player_short.py`（player30／60／144＋capture）；
  4. `run_timing.py --gui --combat --short --duration 16 --events 20 --fps 60`；
  5. 2 GUI＋2 bot：action60、player60、timing（`--bots 2 --quad-probe`）。
- **D44 的 case-run 數（長測另計）**：遊戲 95（25 案 3 輪 75＋閒置 20；其中故障 66、clean 29）、背壓 120（20 次×6 案）、網路一般模式 30、網路 impairments 15，合計 **260**。
- **目錄**：`runs/<項目名>/`（由各 runner 建立）；runner 的工作目錄是 `runs/`，子程序設 `PYTHONDONTWRITEBYTECODE`。另有 `progress.jsonl`（每個事件一行）；結束或停止時都寫 `files.sha256`。
- **時間**（`run_16.py --list-only`，`schedule.txt` SHA-256 `4f58a28eaff82c005f6d1ccbe0e1b35f09b0ea333617b4ecfdab77db95134062`；每項含兩次 sleeper 與核對）：

  | 部分 | 項數 | 估計 | 開始的時刻（約） |
  |---|---|---|---|
  | 閒置閘門 | — | ≥0.3 分（最多 30 分） | 0 |
  | I | 30 | 19.6 分 | 0:00 |
  | D | 134 | 135.7 分（其中 impairments 15 次約 23.5 分、長測 30.8 分） | 0:20 |
  | G | 7 | 6.5 分（桌面會開視窗） | 2:35 |
  | 判定 | — | 約 5 分 | 2:42 |
  | 合計 | 171 項 | **約 2.7 小時** | |

  - 單項時間的來源：C2 的 `progress.jsonl`（遊戲 25.1、背壓 81.8、網路 29.7 秒）；第一次在 v7 跑的項目用 v6 14b 的 `progress.txt`；impairments 用本批的開發跑次（runner 87.2 秒）。順序改變不改變總時間。

#### 分類

每一項由 `judge_16.classify` 分類；`judge_16.py` 從原始檔案重算，和 `progress.jsonl` 不一致就算完整性問題。

- **遊戲案例**（`judge_c2.classify_gameplay`，S1／C1／C2 的規則）：E＝runner 逾時、沒有判定行、`ready.json` 或 `result.json` 缺漏；A＝錯誤含 `summarize.STRUCTURAL` 的字樣，或沒有 `movement` 而錯誤不是 probe 失敗；F＝其他不通過；P＝通過。
- **背壓**：P＝`backpressure-results.json` 有 6 案而且全部通過；F＝某案的 `failure.json`；E＝兩者都沒有。
- **網路**：
  - P＝`result.json` 的 `passed` 而且結束碼 0；
  - impairments 模式另外要求 `network_impairments` 正好是 RTT 0、20、40 三步、都通過，否則 **A**（旗標沒有生效）；一般模式出現 impairment 的步驟或 `network-rtt-*.log` 也是 A；
  - E＝探針還沒跑就結束（沒有 `network.log` 也沒有 `match.log`）；其餘 F。impairments 的某一步失敗時 runner 拋出例外、不寫 `result.json`，所以是 F。
- **長測**：E＝沒有判定行或 `result.json`；A＝結構性的分析器拒絕；60 Hz 長測通過但 `full_30_minute_qualified` 不是 true 算 F；其餘照 `passed`。
- **quad**：E＝`summary.json` 缺漏或輪數不對（clean-60／30 是 5，gui-bots 是 1）；A＝結構性拒絕；其餘照 `passed`。
- **action／player 短測**：照各自的 matrix 檔；player144 的 `invalid` 只以 D11③（120 Hz 顯示器上限）接受，**不算通過也不算失敗**；其他 `invalid` 或不通過都是 F；`same_distance_phase_consistent` 必須是 true。
- **timing**：P＝`results.json` 的 `passed`、唯一一輪的狀態是 `passed`、判定行也是通過；視窗閘門判成 `invalid_window_*` 是 F。
- 逾時：遊戲 900 秒、背壓 1200 秒、網路 900 秒（impairments 也是；內部每一步探針 45 秒）、長測短模式 300 秒（60 Hz 長測再加 1928 秒）、quad／action／player 900 秒、timing 300 秒。超過就是 E。

#### 停止條件

立即停下，保留全部證據、不重跑，交使用者決定。由 `run_16.py` 在每個項目之後判斷（`judge_16.item_stop`），`judge_16.py` 事後重播；完整的 session 卻有應停未停，算完整性問題。

1. **分類 E 或 A**（單位：項目）。**雜湊與 worktree**（每個項目之前）：產物或凍結清單的雜湊不符（含清單本身對宣告字面值，以及清單內 117 個檔案逐一）、`declaration.md` 的雜湊不符（開跑時）、`run_16.py` 不等於 `DRIVER_SHA`（開跑時）、量測 worktree 的 HEAD 或追蹤檔改變、主 checkout 對 `6d3f287` 有非文件的差異、遠端 PR 分支不是只有文件或 `ls-remote` 失敗（開跑時；結束時由 judge 檢查）。
2. **clean 跑次的權威 Cooldown 拒絕**（單位：該項目 `actions.jsonl` 中 `rejection==3` 的裁決數，≥1 就停）。clean 跑次＝25 案中的 clean-30／60／144（第 1～3 輪）、D 的閒置 clean 對、長測短模式、60 Hz 長測。同時記為 FireGate 重推條件 10 與 D75 的 M0 觸發。
3. **clean 跑次的非 respawn 重設**（單位：該項目 Match movement trace 中 `reset_reason`≠`life_respawn` 的重設數，≥1 就停；trace 缺漏也停，因為數不出來）。來源：C1、C2 第 4 條（v7 的 clean 非 respawn 重設是 0）。
4. **同一項目同時有 D44 事件與 Cooldown 拒絕**（第 09 批「要把 D44 提前的條件」）。
5. **D44 事件的 case-run 累計 ≥3**（D80 U4）。單位＝**有事件的 case-run**：一個遊戲案例、一個背壓案例、一次網路跑次（任一模式）、60 Hz 長測各算 1；同一個 case-run 內有幾個事件都只算 1。四個母體合併計數（「還在」已經確定）。
6. **整合判定不通過**（I、G 的判定項目，單位：項目）；**閒置閘門 30 分鐘內不成立**。
   - **例外（D45 檢查 7）**：I 的項目不通過，而它的每一個問題都由它自己的 D44 事件解釋（遊戲：D61 即時規則的故障窗外重設，以及錯誤全是 E2 的凍結判定；背壓：每個失敗的案例都帶 E2；網路：E3）時，只立旗標（`passed`＝false），不停下；事件照樣計入停止條件 5。clean 案例仍依停止條件 3 立即停下。
7. **D 部分非事件的失敗累計 ≥3**〔本批新訂，U2〕：D 部分分類為 F 而且沒有 D44 事件的項目（例如背壓某案因 E2 以外的原因不通過、60 Hz 長測不通過），**impairments 除外**。每一次都立旗標（`passed`＝false）；第 3 次停下。
   - impairments 的非事件失敗（例如覆蓋條件不成立）**只立旗標、不計數、不停下**（D45 檢查 6），所以不會截斷其他母體；它們排在 D 的最後（長測之前），也不會在 D 的前段打斷計數。
8. `STOP-REQUEST` 檔；`run_16.py` 自己的其他例外。

- **不停下、記錄後繼續**（提案 3-5，D80 核准）：D 部分故障或網路案例中的 E1～E3，累計未達 3 次。做有限定位（≤30 分；用凍結工具列出重設前 30 Tick 的 late 修正、領先跳升、被釘住、Tick 間隔、排隊合計），修法留在 v7.1 A3。
- **25 案第 1 輪與第 2、3 輪的規則不同**：第 1 輪是整合判定（`judge_c2.r_record`）；只有 D61 即時規則不成立時，依上面 6 的例外只立旗標。第 2、3 輪的同一種重設記為 E1、累計到停止條件 5。D 的 25 案只看凍結判定與 D44 事件，不檢查 08b 的 relay／Gateway 規則。

#### 判定

##### 整合判定（I、G）

| 代號 | 項目 | 通過條件 | 備註 |
|---|---|---|---|
| L2-1 | 25 案（第 1 輪，25 項） | 每案 `judge_c2.r_record` 沒有問題：`run_gameplay.py` 的判定通過（凍結分析器，**不扣** `GAP_ERROR`，08b、C2 R 的做法）；relay 結果任一秒 ≤31，`relay.json` 缺漏或沒有玩家也算問題；Gateway 的 `results_min_interval_ms` 與 `link_action_min_interval_ms` 以印出值在每個有值的視窗 ≥33.3，每跑次至少 1 個有值的視窗；D61 即時規則不成立 | clean-30 計入（U2，D80）。D61 事後規則（基準 `b205615`）只立旗標 |
| L2-2 | 背壓與網路（第 1 輪） | 背壓 6／6 通過；`run_network.py` 一般模式通過 | impairments 不屬整合判定 |
| L2-3 | 長測短模式 | `run_gameplay_soak.py` 的判定（`gameplay_soak_evidence.py`）；全部條件見「門檻的來源」的長測各列 | **跨建置**。同時是 clean 跑次（停止條件 2、3） |
| L2-4 | quad | clean-60：5／5 輪通過，而且 `judged` 是 true（`quad_evidence.py`）。clean-30：只記錄 | **跨建置** |
| L2-5 | 1 GUI＋3 bot | `run_quad.py --case gui-bots` 通過；這個案例 `judged: True`（`run_quad.py:29`）：全程 4 人、死亡與重生、GUI 幀，以及 `quad_evidence` 的移動門檻 | **跨建置** |
| L2-6 | GUI 短測 | action 4 案全部通過；player 4 案全部通過（player144 的 `invalid` 只以 D11③ 接受）；`run_timing.py --gui --combat --short` 通過（移動門檻、跨視窗可見交越、combat 的裁決抵達、視窗閘門 `clean`）；`--bots 2` 三項通過 | **跨建置**。`gui_visible_latency`（`checks.json`）只記錄（U8：不發行） |

- 延遲門檻的讀法（C1 判定第 3 條的先例）：只因「產生→執行」、可見交越或裁決抵達的延遲門檻不成立時，報告標明 v7 的已知位移（產生改在步邊界，Actual 約 +8～10 ms，第 04、09b 批），由使用者判斷。不自動判失敗、不重跑。
- quad 或 G 出現「Unexplained epoch reset」時走整合判定的停止（停止條件 6），報告另外標成 D44 的實例。

##### D44 的分類（不進入 `passed`）

事件的定義（提案 3-2；全部沿用凍結工具，不新訂量法）：

- **E1**：遊戲案例或長測中，Match 的非 respawn 重設出現在故障窗 (start, release＋1.5 s] 之外，或出現在沒有故障窗的案例（`judge_c2.d61_resets`，D61）。只用在遊戲案例與長測；背壓與網路的非 respawn 重設只記錄（「和計畫不同的地方」5）。
- **E2**：凍結分析器錯誤中的「Unexplained epoch reset … no preceding substantive interference」（`command_evidence.py:421`）。出現在背壓的 `command_evidence` 與遊戲案例的錯誤；長測的分析器沒有這條（E2 不可能出現在長測）。
- **E3**：`run_network.py` 的「an application stall reset the movement epoch」（`network_main.cpp:297`），出現在 `network.log`、`network-rtt-*.log` 或 runner 的輸出。兩種網路模式都算；記下出現在哪個檔案。
- **網路母體的偵測範圍很窄**（D45 檢查 5）：E3 只比較 6 秒停頓前後的 `movementEpoch`（`network_main.cpp:282-297`）。所以「網路 0 事件」的意思是「**停頓期間 epoch 沒變**」，不是「整個網路跑次沒有解釋不了的重設」。每個探針階段在模擬角色停下約 0.54 秒後正好有 1 次 starvation 重設（一般模式 1 次、impairments 4 次）；數目不等於「每階段 1 次」時立只記錄的旗標（`record_flags`），在報告列出，不是事件、不影響 `passed`。

**統計的讀法**（D45 檢查 2）：

- 重複跑次不是獨立的情境樣本：矩陣的故障情境是決定性的（故障窗內的重設每輪正好 16 次，08b 開發、D72、C2 after 逐案相同）；impairments 的丟包用固定 seed（`impaired_network.py:156`；三個 RTT 階段的輸入丟包都是 77 個）；遊戲網路案例也是固定 seed（`08c-drop-report.md:734`）；同一 session 的主機狀態彼此相關。**重複只抽樣主機時序，不抽樣丟包型態或故障情境。**
- 所以以各母體的上限為主（0 事件時每 case-run 的單側 95% 上限）：

  | 母體 | case-run | 0 事件時的上限 | 和第 08 批比（新 worker） |
  |---|---|---|---|
  | 遊戲 | 95（故障 66、clean 29） | 3.1%（故障 4.4%、clean 9.8%） | 1／26：95 次全 0 的機率 2.4% |
  | 背壓 | 120 | 2.5% | 1／19：120 次全 0 的機率 0.15% |
  | 網路一般 | 30 | 9.5% | 0／11：不能比較 |
  | 網路 impairments | 15 | 18.1% | 1／1（同一個 seed）：不能比較 |

  合併 260 次的 1.15% 只在「每個 case-run 獨立」的前提下成立，只當參考。

結果的讀法（提案 3-5 的表，次數改為 260）：

| 結果 | 意思 | 處理 |
|---|---|---|
| 260 個 case-run 0 事件 | 在 v7 最終程式上「未再現」（各母體上限如上） | STABLE_BASELINE 寫「整體回歸未再現（各母體次數與上限）、未結案」。不結案：已知的結構性重設途徑仍在（09b 1-6 的 300 ms 延遲消失、1-7 第 5 點）；D49 第二階段不在範圍內；R2 → v7.1 A3 與 pv7 後再回歸 |
| 1～2 次，都在故障、網路或 impairments 案例 | 「仍存在（低頻）」 | 記錄、保留 trace、嚴重度維持「高」；不擋升格（除非你另外決定）；修法與調查在 v7.1 A3 |
| 只有故障窗內的重設，分布和 `b205615` 相同 | 已解釋的重設，不是 D44 | 只記錄（D61 事後規則 0 旗標；事後規則只套用在第 1 輪） |
| 停止條件 3、4、5，或疑似推翻設計前提 | 可能是退步或結構問題 | 停下回報。選項：(a) 列為已知問題照樣升格；(b) 提前調查（ultracode，D45）；(c) 依「先完成穩定版本」的規則標「缺陷版」並停下 |

- session 沒有完成（停止或缺項）時，讀法前面一律加「INCOMPLETE session（N／171 項）」，不會寫成「未再現」（D45 檢查 11）。
- 60 Hz 長測另列（`long_run`），不算進 260；它的 E1 算事件（停止條件 5），同時是 clean 跑次（停止條件 2、3）。
- 彙總分成四個母體，遊戲再分故障與 clean（`gameplay_split`），各自和普查基準並列。

##### 整體

- `judge_16.py` 的結束碼 0（`passed`＝true）只在以下全部成立時：
  - session 完整（有結束紀錄、171 項全部跑完、順序 I → D → G）、沒有停止紀錄、完整性沒有問題、沒有應停未停；
  - **沒有給使用者的旗標**：D61 事後規則、第 1 輪的 D44 型失敗、D 部分非事件失敗（含 impairments 與 60 Hz 長測）、M0 觸發；
  - L2-1～L2-6 全部通過。
- 所以 D44 的分類本身不影響 `passed`；但第 1 輪的 D44 型失敗、D 的任何非事件失敗都讓 `passed`＝false，交使用者判斷（U2、U7）。`record_flags`（網路階段重設數）不影響 `passed`。
- 完整性的檢查項目：
  - 開始紀錄要有 `DRIVER_SHA` 與三個字面值；兩個清單與 `declaration.md` 的雜湊等於字面值；`judge_16.py` 本身在凍結清單中；
  - 開始與結束紀錄的主 checkout 只有文件變更、遠端 PR 分支只有文件變更、量測 worktree 的 HEAD 是 `6d3f287`；
  - `files.sha256` 與檔案一致，`runs/` 沒有清單外的檔案；
  - 項目的順序等於宣告的排程；有通過的閒置閘門；
  - 每一項的分類、Cooldown、clean 重設、case-run 數、D44 事件與「D44 型失敗」可由原始檔案重算。

#### 只記錄

- **遊戲案例**（25 案 3 輪與閒置對；C2／S1 的定義，經 `judge_c2`）：
  - late 修正數（`start_phase_evidence.py` `e719dab6`；`start_phase.*.epochs[*].late_corrections`）；
  - 領先跳升（`lead_jumps.py` `14cd5b5c`）；
  - 被釘住秒數（每秒送出等待中位數 >2 ms）；
  - Tick 間隔（resolved 時刻 ≥ step＋1.5 ms）；
  - 30 Tick 排隊合計的最大值（對 105）；Cooldown；TimerBaseline 狀態。
- **網路**：Match movement trace 的非 respawn 重設（依原因計數）與探針階段數、`--network-impairments` 每一步的丟包與觀測延遲（`network-rtt-*.json`）、E3 出現的檔案。
- **背壓**：每案的 `unexpected_resets`、`reset_reasons`、30 Tick 排隊合計。
- **第一次在 v7 跑的項目**（`integration_details`）：長測短模式與 quad 每輪的 Actual P50／P95、首次送出 P95、Actual 比例、拒絕、主機狀態；timing 的可見交越 P50／P95。
- quad clean-30（工具 `judged: False`）、`gui_visible_latency`（U8 不發行）、player144 的 `invalid`（D11③）。
- 故障案例中窗內的重設（每輪矩陣正好 16 次，08b 開發、D72、C2 after 相同）。
- FireGate 重推條件 10 的「R<earliest」「R>earliest＋7」：量不到（盲點，見「目的」）。

#### 預測

可被推翻；不參與判定。

- P1 整合判定全部通過。依據：同一產品程式的 C2 回歸（25／25、背壓 6／6、網路通過）；16-D44T 的開發跑次（quad clean-60 1 輪、長測短模式）。
- P2 quad clean-60：Actual P50／P95 約 37.6／37.8 ms，首次送出 P95 約 0.24 ms，每位玩家正好 840 個命令（開發跑次）。
- P3 長測短模式：Actual 約 38.0／38.2 ms，合法射擊 Match／Client P95 約 14／18 ms，排隊合計最大 60（開發跑次）。
- P4 GUI 短測通過，可見交越 P50 約 33～37 ms。**依據最弱**：第 04 批（P1b 頭）兩次 GUI 短測（dev-gui 36.6／37.8、after-review 33.3／38.2），都沒有 `--combat`；最終程式上沒有跑過，`--combat` 的裁決抵達在 v7 從沒判定過。
- P5 D44：260 個 case-run 0 事件。依據：修好之後的資訊流上 0（遊戲 165、背壓 18、網路一般 1）；08b 之後的全部證據也是 0。impairments 大概也是 0，但只有本批的 1 次開發跑次（通過，starvation 4 次＝4 個階段）可依。
- P6 60 Hz 長測通過，`full_30_minute_qualified`＝true。**沒有 v7 的先例**：三角色從沒跑過 30 分鐘；上一次 1808 秒的 60 Hz 長測是 v5 的 soak60（原判定在 GAP 規則失敗，改用規則 B 才通過，最長幀 87 ms），v6 沒有跑過。
- P7 只記錄的項目：網路每個階段正好 1 次 starvation 重設（一般 1、impairments 4），`record_flags` 是空的；矩陣故障窗內的重設每輪 16 次，分布和 `b205615` 相同。
- P8 時間約 2.7 小時（不含閒置閘門的等待）。

#### 門檻的來源（D45）

每個門檻的來源建置、指標，以及為什麼適用。「跨建置」表示在別的建置上定出來的值，用在本次的建置；**CROSS-BUILD** 標的是 v5／v6 的門檻第一次用在 v7 的那一個工具或量法上。會影響 `passed` 的零容忍規則也列在這裡（D45 檢查 MUST-FIX 1）。

| 門檻 | 值 | 來源（建置／指標） | 跨建置？ | 為什麼適用／限制 |
|---|---|---|---|---|
| 25 案判定 | `run_gameplay.py` 的判定 | 凍結分析器（v6 最終清單＋`PROTOCOL_VERSION 6`）；C2 R 在同一產品程式上 25／25 | 否 | 同一套分析器在 08b、08c、D72、C2 R 都用在 v7 上 |
| relay 結果 | 任一秒 ≤31 | 第 08b 批判定 3；產品上限 30＝floor((1 s−1 ns)／I)＋1（08b `:270-279`），31 留 1 給 relay 端的時間壓縮 | 否 | `b205615`（同一產品程式）的乾跑最大值 30（C2 宣告）；C2 R 通過 |
| Gateway 最短間隔 | 印出值 ≥33.3 | 第 08b 批判定 2、5；I＝1/30 秒；Gateway 統計行 `%.1f` | 否 | `actionSendInterval`＝ceil(1e9/30)＝33,333,334 ns，寫出完成後才以 `time.Now()` 重設期限（`action_delivery.go:500-520`、`server.go:683-686`、`runtime_link.go:578`），所以間隔一定 ≥I。C2 R 的遊戲案例都是 33.4；C2 的 GUI 連點有 12 個跑次印出 33.3，是**設計下限**，不是浮點邊界風險。本批只在 L2-1 判這條 |
| D61 即時規則 | 故障窗 (start, release＋1.5 s] 之外，或沒有故障窗時出現 | D61（08c L2 核准）；C2 宣告的 U5（D79） | 否 | 25 案一律適用，沒有豁免；C2 R 0 次。第 1 輪只因它不成立時依停止條件 6 的例外只立旗標 |
| D61 事後規則 | 基準沒有的原因／位置，或次數比基準多 | D61；基準＝`b205615` 的 D72 矩陣 | 否 | 產品程式只差註解；每案 1 個跑次，只立旗標 |
| clean 非 respawn 重設 | 0 | C1、C2 第 4 條 | 否 | v7 的 clean 案例過去全部 0 |
| Cooldown（clean） | 0 | P2 停止條件、v6 第 12 批、C2 | 否 | 不是分布；headless 不經閘門，出現就表示時序前提被打破（D75） |
| 移動「產生→Actual」 | P50 ≤50、P95 ≤66.7 ms | v5／v6 契約（v6 `MANUAL_ACCEPTANCE.md`「量化門檻」）；v5 定的值 | 25 案：否（`command_evidence.py:402` 同一門檻在 v7 的矩陣一直在用）。**長測（`gameplay_soak_evidence.py:362`）與 quad（`quad_evidence.py:205`）：CROSS-BUILD** | v7 改在步邊界產生，Actual 約 +8～10 ms（第 04、09b 批：領先 2 步＋目標 4 ms）。同一產品程式在 C2 的 4 ms 與 8 ms 狀態下 P50 37.6～38.0、P95 37.7～38.1 ms；開發跑次長測 38.0／38.2、quad 37.6／37.8。餘裕 P50 約 12 ms、P95 約 28 ms |
| 長測的漂移窗口 | 每 10 循環（160 秒）的窗口同樣要 P50 ≤50、P95 ≤66.7 ms | v5／v6 契約「長測不漂移」（`gameplay_soak_evidence.py:368`，`DRIFT_BUCKET_CYCLES`＝10） | **CROSS-BUILD** | 30 分鐘在 v7 沒有證據（P6）；短模式只有 2 循環、1 個窗口 |
| 首次送出 | P95 ≤22 ms | v5／v6 契約「乾淨同機」 | 25 案：否。**長測（`:363`）與 quad（`:204`）：CROSS-BUILD** | v7 命令在步邊界產生後立即交給 worker；C2 同一產品程式 0.14～0.22 ms、開發跑次 0.21／0.24 ms。門檻幾乎不可能誤判；辨識力也低（只抓得到送出被卡住 >22 ms） |
| Actual 比例 | ≥99% | v5／v6 契約 | 長測、quad：CROSS-BUILD | 開發跑次 1.0 |
| 30 Tick 排隊合計 | <105 | backlog 重設門檻（Match 常數） | 否（同一個 Match 常數） | 開發跑次最大 60 |
| Client runtime gap（長測，`GAP_ERROR`） | Client 沒有未配對的 runtime gap（`frame_seconds` ≥0.1 或 `dropped_seconds` >0；正常的 LifeRespawn 與 session 開始的 seed 夾住除外） | **v5 的「規則 B」**（`command_evidence.py:39-51`；`gameplay_evidence.client_disturbance`）；C1 `05:55` | **CROSS-BUILD**（語意改變） | v7 的模擬角色在晚醒約 83 ms 或待送命令滿 12 時就發出 gap（`LocalPlayerPrediction.cpp:357`），不再每幀發出。上一次 1808 秒的 60 Hz 長測（v5 soak60）原判定在 GAP 規則失敗、改用規則 B 才通過，最長幀 87 ms（v5 `ACCEPTANCE_STATUS.md:36`）；v6 沒跑過 1808 秒（v6 `STABLE_BASELINE.md:68`）。**零容忍的主機停頓規則**：只因它不成立時 `passed`＝false，報告並列前後 sleeper 與 TimerBaseline，交使用者判斷 |
| 主執行緒幀（長測） | 任一幀 ≥100 ms 就不通過 | `gameplay_soak_evidence.py:324`（v5／v6） | **CROSS-BUILD** | v7 沒有放寬這條；它量的是主執行緒（事件＋畫面），v7 的模擬不受它影響，但規則照判。零容忍的主機停頓規則，讀法同上 |
| Match runtime gap（長測） | 0 | `gameplay_soak_evidence.py:366`；`MatchRuntimeHost.cpp` 在 Tick 間隔 ≥100 ms 或丟時間時發出 | **CROSS-BUILD** | 第 07 批之後的 Match。零容忍的主機停頓規則，讀法同上 |
| Gateway limiter（長測） | 每秒窗口 ≤120 包、`rate_limited` 0 | `gameplay_soak_evidence.py:376-377`；v6 `MANUAL_ACCEPTANCE.md`「容量」 | **CROSS-BUILD** | v7 的 Client 送出最多約 82 包／秒（09b 1-7）；仍在 120 之下；開發跑次通過 |
| Client 傳輸上限（長測） | pending ≤12、queued ≤32 | `gameplay_soak_evidence.py:287`；容量契約 | **CROSS-BUILD** | 容量契約的值，v7 沒有改；開發跑次通過（長測在 v7 只有這 1 次短模式） |
| 合法射擊 Match／Client | P95 ≤100／150 ms | v5／v6 契約 | 長測：CROSS-BUILD | v7 的 Gateway 結果改成 30 Hz；開發跑次 14.3／18.1 ms，餘裕大 |
| 60 Hz 長測的時間 | `full_30_minute_qualified`＝true | `run_gameplay_soak.py` | — | 不成立算 D 的非事件失敗（停止條件 7 的旗標，`passed`＝false） |
| 60 Hz 產量 | 每位玩家 ±2 步，每次經核對的重生另加 3 步 | v6 第 16 批（`quad_evidence.py:191-197`；v6 `MANUAL_ACCEPTANCE.md`「60 Hz 產量」） | **CROSS-BUILD** | v6 的命令跟著幀產生，±2 步是幀與窗口邊界的量化誤差。v7 改由模擬角色在固定步邊界產生，誤差只會更小。開發跑次 840／840。主機停頓（`gaps_100ms`）時這條不判 |
| quad 拒絕與重設 | 拒絕只有 Dead／StaleLife；沒有解釋不了的重設 | v6 第 16 批 | CROSS-BUILD | 和 25 案同一條「Unexplained epoch reset」規則（E2 的文字）；quad 出現時是整合判定停止，報告標成 D44 實例 |
| 跨視窗可見交越（GUI 短測） | 每輪 P50 ≤50、P95 ≤80 ms；全部預排事件為分母、未配對以 +∞ 計；配對 ≥99% | v5 契約（`presentation_evidence.py:601-607`；v6 `MANUAL_ACCEPTANCE.md`「量化門檻」）；`checks.json` 的 `gui_visible_latency` 同值 | **CROSS-BUILD（暫定）** | v6 combat 36.5～37.1 ms；v7 第 04 批 33.3～36.6 ms。Actual 上升約 8～10 ms，可見交越沒有跟著上升。**最終程式上沒有數據**。20 個事件時 P95 是第 19 名，1 個未配對就讓配對率 <99% |
| combat 的裁決抵達（GUI 短測） | P95 ≤150 ms | `combat_gui_evidence.py:234-236`；v5／v6 契約「射擊產生→Client 取得裁決」 | **CROSS-BUILD（暫定）** | 第 04 批的兩次 GUI 短測都是 `combat: False`；v7 從沒判定過。v7 的 Gateway 結果 30 Hz、合法射擊 Client P95 在 headless 長測約 18 ms |
| GUI 視窗閘門 | `window_disturbed` 必須是 false；缺漏、無效、未知都判 `invalid_*` | v5 fix 06（`run_timing.py:70-107`、`presentation_evidence.py`） | CROSS-BUILD（工具在 v6 與 v7 相同，GUI probe 是 v7 建置） | 判的是桌面狀態，不是產品。GUI probe 只在第 04 批的頭判定過 2 次，沒在最終程式上判定過。焦點事件算受干擾（`presentation_evidence.py:27`），所以要勿擾 |
| GUI 短測的移動門檻 | 22／50／66.7、≥99% | 同上（`run_timing.py:396-399` 經 `command_evidence` 或 `quad_evidence`） | 雙 GUI：否；`--bots 2`：CROSS-BUILD（`quad_evidence`） | 同上 |
| player short | 各案 `passed`、`same_distance_phase_consistent` | v5／v6 契約 | CROSS-BUILD | v7 從沒跑過 player short；player144 在 120 Hz 顯示器上依 D11③ 無效 |
| D44 累計停止 | 有事件的 case-run 3 次 | 提案 3-5〔提議〕，D80 U4 核准 | 新訂 | 「還在」已經確定，再多跑只加次數 |
| D 非事件失敗的停止 | 3 次（impairments 除外） | 本宣告新訂（U2、U6） | 新訂 | 照受干擾停止（C2 U7）的形狀；1～2 次可能是偶發，3 次表示 D 的資料不再可讀 |
| 網路階段的重設數 | 每個探針階段 1 次 | 本宣告（D45 檢查 5）；08b 之後 4／4、本批開發跑次 4 階段 4 次 | 新訂 | 只記錄的旗標 |
| 閒置閘門 | 3 次 5 秒 sleeper 最大 <10 ms，最多 30 分 | C1 第 2 次 session（使用者核准） | 否 | 與建置無關 |
| TimerBaseline 狀態 | `late_p99_ms` ≥6.0 | C1 `05:41`、C2 決定 8 | — | **只記錄**，不分層、不當分母 |
| 逾時 | 見「分類」 | S1（900 s）、C2；長測加 1808＋120 s | — | 超過就是 E |

- 「暫定」的意思：門檻照樣判定（U4）；不成立時停下並標明「v5／v6 的門檻第一次用在 v7」，交使用者決定是否當作 v7 的退步。
- 本表沒有任何門檻是看了乾跑結果才定或放寬的；乾跑的資料只用來驗證結構（C2 的 MUST-FIX 1 教訓）。D45 檢查也沒有找到這一類。

#### 普查基準

`census.py` 用和正式判定同一套偵測器（`judge_16.gameplay_events`、`bp_cases`、`network_events`）掃過既有證據，輸出 `census.json`、`census.txt`。單位是 case-run；`run_client_delay.py` 的注入案例（08c L2 的 `clientdelay-*`，30 次）只列出，不進基準。

- **收錄規則**：第 08 批開發與 ab、08 L2 兩次、08b 開發、08b L2、08c L2、D72、C2 的 session（A、R 與 before）。沒有收錄的：注入型的 session（D48 第②③步、09R-map、09ab-L2 的注入跑次）、C2 的 B（GUI 連點，不是 D44 的 runner），以及 08c-impl 22、S1 21、p2log 3、09ab-L2 非注入 64、D48③ 84、06／07 L2 120 次（D45 檢查另外看過，事件都是 0）。

**主要基準＝修好之後的資訊流**（D45 檢查 MUST-FIX 3；事故紀錄 `:222`）：08c L2 的 flow／flow09a、D72 的 flow／flow09a／after（`b205615`）、C2 after。

| 母體 | case-run | 有事件 |
|---|---|---|
| 遊戲 | 165 | 0 |
| 背壓 | 18 | 0 |
| 網路一般 | 1 | 0 |
| 網路 impairments | **0** | — |

- 網路一般在修好的資訊流上只有 1 次（C2 after），impairments 沒有。這兩個母體在主要基準上沒有可比的量。

**次要基準＝全部普查**：

| 母體 | case-run | 有事件 | 事件 |
|---|---|---|---|
| 遊戲（25 案與 clean） | 414 | 1 | 第 08 批開發 `gateway-1000ms`：backlog 重設在放開後 +1.93 s（故障窗外），D61 下為 E1 |
| 背壓 | 73 | 1 | 第 08 批開發 `host-ipc-250ms`：E2 |
| 網路一般 | 23 | 0 | — |
| 網路 impairments | **1** | 1 | 第 08 批開發 `dev-network`：一般階段通過，RTT 0 階段 E3（`network-rtt-0.log`） |

- 08b 之後的建置全部 0。注意：08 L2 的兩棵樹與 08b L2 的 before（36 次）是 08b 之前的建置；C2 before（15 次）是 v6final。
- 新舊（D44 的原始比較）：第 08 批的新 worker：遊戲 1／26、背壓 1／19、網路一般 0／11、impairments 1／1；舊 worker：背壓 0／18、網路一般 0／8。
- 次數太少，不做統計檢定；基準只用來說明「08 頭出現過、修好的資訊流上沒有出現」。
- 只記錄：故障窗內的重設每輪矩陣 16 次（08b 開發、D72、C2 after 相同）；08b 之後一般模式網路跑次每次 1 次 starvation 重設（4／4，`census.json` 的階段旗標清單是空的）。

#### 證據

`build/target/_build/test/logs/pvp-v7-16-prep-20261014/`（準備）：

- `declaration-draft.md`（本檔）、`xhigh-d45-check.md`、`run_16.py`、`judge_16.py`、`sleeper.py`、`census.py`、`census.json`、`census.txt`、`prep_hashes.py`、`prep.json`、`frozen.sha256`、`schedule.txt`、`summary.md`。
- `dryrun/`：`sources.py`、`fake_runner.py`、`driver_mock.py`、`judge_mock.py`、`tamper.py`、`run_tampers.sh`、`check_tampers.py`、`check_tampers_expect.py`、`mutate.py`、`mock-declaration.md` 與輸出（見下）。
- `dev/`：`argcheck.py` 與 `argcheck-3/`、16-D44T 的開發跑次（`quad-clean-60`、`soak-short`）、本批的 `network-impaired-1`（只記錄）。

正式 session：`build/target/_build/test/logs/pvp-v7-16-l2-<日期>/`（`progress.jsonl`、`runs/`、`files.sha256`、`judgement.json`）。

##### 乾跑（準備時做的，不是證據的一部分）

- **代替 session**（`dryrun/drv-4/ok`）：runner 換成連結既有證據的 `fake_runner.py`（第 1 輪＝C2 R；D 輪＝D72 與更早的 v7；長測、quad、GUI＝v6 14b 或 16-D44T 的開發跑次；impairments＝本批的開發跑次），sleeper 換成常數；雜湊、worktree、主 checkout、遠端 PR 分支（真的 `git ls-remote`）照真的做。171 項、順序 I → D → G、判定通過、D44「0／260」。**這是結構的驗證，不是量測**。
- **執行器的模擬**（`driver_mock.py`，27 情境，`dryrun/drv-4/`）：停止條件 1～7、`STOP-REQUEST`、閒置閘門、雜湊／worktree／主 checkout／遠端／宣告／`DRIVER_SHA`、impairments 的 E3（第 152 項後才由 `STOP-REQUEST` 停下）與旗標沒有生效，以及 D45 檢查之後新增的：第 1 輪背壓 E2 與網路 E3 只立 1 個旗標而繼續、impairments 非事件失敗 3 次只立 3 個旗標、跑完 171 項、**凍結清單內的檔案在第 5 項之後被改**（第 6 項之前停下）、60 Hz 長測的非 respawn 重設（停止條件 3）。全部在預期處停下或繼續，都寫出 `files.sha256`。
- **竄改測試**（`dryrun/neg-4/`）：原有 27 種＋新增 7 種（D 的單次非事件失敗、60 Hz 長測的重設、impairments 某一步不通過、impairments 失敗 3 次、第 1 輪的背壓 E2／網路 E3／D61 故障窗外重設），**34／34 被拒而且理由正確**；正向對照 3／3 通過（`neg-4/check.txt`）。
- **突變**（`mutate.py`，`neg-4/mutations-final.txt`）：D45 檢查的測試缺口各一個突變（刪單次非事件失敗的旗標、60 Hz 長測改成非 clean、刪 impairments 的「三步都通過」、第 1 輪一律停下、impairments 計入停止條件 7、刪 `run_16.frozen()` 的逐檔核對），**6／6 被殺**。其中「60 Hz 長測改成非 clean」在竄改測試看不到（judge 從紀錄讀 clean），由模擬 `soak-long-reset`（突變版的 driver 不停下）與排程核對抓到。
- **runner 參數檢查**（`dev/argcheck.py`，`dev/argcheck-3/`）：排程中 39 種不同的指令列，用真的 runner 的 argparse 與參數檢查跑過，全部通過；`run_network.py` 兩種模式用真的 argparse＋檔案檢查（在 `free_port` 之前停下），`network_impairments` 分別是 false 與 true。
- **impairments 的開發跑次**（`dev/network-impaired-1`，只記錄；當時沒有建置在跑）：v7final 的 network probe，`run_16.command()` 產生的指令列原樣執行；結束碼 0、87.2 秒、分類 P、RTT 0／20／40 三步都通過、E3 0、非 respawn 重設 starvation 4 次（4 個階段）。
- `run_16.py --preflight`（真的 `FROZEN`、`ARTIFACTS` 字面值；宣告還不存在）：只有「`declaration.md` 不存在」一個問題；遠端 PR 分支 `1f610b9`，只有文件（`dryrun/preflight-3.txt`）。

#### 和計畫不同的地方

1. **網路 impairments 15 次**（D81）：D44 的 case-run 從 245 變成 260；時間增加約 24 分（提案估計的上緣）。impairments 只在 D 部分、排在第 30 塊之後，不屬整合判定（L2-2 仍是一般模式 1 次）。
2. **順序 I → D → G**（D45 檢查 8；D80 是 I → G → D）。
3. **停止條件 7（D 部分非事件失敗累計 3 次，impairments 除外）是新訂**。提案只有 1～6。
4. **第 1 輪的 D44 型失敗只立旗標、不停下**（D45 檢查 7）；第 2、3 輪的 E1 累計到停止條件 5。依據：提案 3-5「記錄後繼續」（D80 核准）。
5. **E1 只用在遊戲案例與長測**：背壓與網路的非 respawn 重設只記錄。背壓有自己的 E2 規則；網路探針刻意做 6 秒停頓、停止與恢復、逾時，每個階段都有 1 次 starvation 重設，D61 不適用（改用只記錄的階段旗標）。提案 3-2 的 E1 定義只寫遊戲案例。
6. **長測短模式算 clean 跑次**（停止條件 2、3 套用）；提案的 clean 只列了 25 案與閒置對。
7. **`passed` 不含 D44 的分類**，但第 1 輪的 D44 型失敗、D 部分非事件的失敗、D61 事後規則、M0 觸發都會立旗標，讓 `passed`＝false。
8. **主要基準換成修好之後的資訊流**（事故紀錄 `:222`）；R2 移到 v7.1 A3。
9. **統計以各母體的上限為主**，合併的 1.15% 只當參考。
10. **非文件變更的檢查**：主 checkout 對量測的 commit `6d3f287`，再加遠端 PR 分支（C2 的 MUST-FIX 3 的做法）；`ls-remote` 逾時記成 `remote_error`。
11. **凍結清單加 c2-after 的 52 檔**（D45 檢查 MUST-FIX 4）。
12. **runner 的工作目錄是 `runs/`**（C2 的做法）。

#### 需要使用者決定的地方

D81 授權「D45 檢查修正後照建議核准並執行」。以下每一項都已「**D81 依建議採用**」，並列出替代方案；你回來後若要改，在 16-L2 開跑前告訴主對話。

- **U1 停止條件 5 的單位**——D81 依建議採用：「有事件的 case-run」累計 3 次停（背壓每案各算 1；同一個 case-run 內的多個事件只算 1；四個母體合併）。
  - 機制：D80 U4 已核准「累計 3 次停」，沒有寫單位。以 case-run 為單位和各母體上限的分母一致。
  - 替代：以「事件的個數」計（會更早停）；或各母體分開計數。
- **U2 停止條件 7**——D81 依建議採用：D 部分（impairments 除外）分類為 F 而沒有事件的項目，每次立旗標（`passed`＝false），累計 3 次停下。
  - 機制：第 2 輪以後和第 1 輪是同一個產品程式、同一個案例，失敗一樣是產品的結果，不能忽略；但 1 次不停，讓 D44 的計數可以繼續。
  - 替代：(a) 只記錄、不影響 `passed`；(b) 1 次就停。
  - 代價：D 有 1～2 次失敗時 `passed`＝false，主對話會停下等你判斷，L3 與 STABLE_BASELINE 會延後。
- **U3 25 案第 2、3 輪的故障窗外重設不立即停下**——D81 依建議採用（記為 E1、累計到停止條件 5；clean 案例仍立即停下）。
  - 替代：每一輪都用 D61 即時規則（第 1 次就停）。
- **U4 v5／v6 的門檻第一次用在 v7（CROSS-BUILD）**——D81 依建議採用：照樣判定；只因延遲門檻、`GAP_ERROR` 或其他零容忍的主機停頓規則不成立時，報告標明「v7 的已知位移／語意改變」並列 sleeper 與 TimerBaseline，交你判斷，不自動判失敗、不重跑（C1 判定第 3 條的先例）。可見交越與 combat 裁決抵達標「暫定」。
  - 替代：L2-3～L2-6 的延遲門檻只記錄（STABLE_BASELINE 就沒有可引用的判定）。
- **U5 網路母體的偵測範圍**（D45 檢查 5）——D81 依建議採用**兩者都做**：加只記錄的旗標「非 respawn 重設數 ≠ 每個探針階段 1 次」（報告列出，不是事件、不影響 `passed`），並寫明讀法「網路 0 事件＝停頓期間 epoch 沒變」。
  - 替代：(a) 只寫讀法；(b) 把階段數不符算成 D44 事件（等於新訂量法，提案 3-2 不允許）。
- **U6 impairments 的位置與計數**（D45 檢查 6）——D81 依建議採用：15 次全部排在第 30 塊之後、60 Hz 長測之前；它的非事件失敗只立旗標（`passed`＝false），**不計入停止條件 7、永不停下**，所以不會截斷其他母體；E3 仍算 D44 事件（停止條件 5）；旗標沒有生效算 A（停下）。這也解決了 v1 草稿「不屬整合判定」和「非事件失敗讓 `passed`＝false」的矛盾：impairments 不影響整合判定，但它的失敗仍交你判斷。
  - 替代：(a) 留在 D 的偶數塊穿插、停止條件 7 只立旗標；(b) 穿插而且合併計數（v1 草稿，可能在 D 開始約 15 分鐘就截斷三個母體）。
- **U7 第 1 輪的 D44 型失敗**（D45 檢查 7）——D81 依建議採用：遊戲的 D61 故障窗外重設（及錯誤全是 E2 的凍結判定）、背壓的 E2、網路的 E3，在第 1 輪只讓 `passed`＝false（L2-1 或 L2-2 不通過），不停下 D；其他原因的不通過照停止條件 6 停下。
  - 機制：提案 3-5 要求故障案例中的 E1～E3「記錄後繼續」；第 1 輪同時是 D44 第 1 輪，停下就沒有回歸可讀。
  - 替代：照 v1 草稿，第 1 輪任何不通過都停下（D 不跑）。
- **U8 順序 I → D → G**（D45 檢查 8）——D81 依建議採用，並在開跑前打開勿擾（專注模式）。
  - 機制：G 有 v7 沒跑過或暫定的項目（`--combat`、player short、可見交越、視窗閘門），使用者不在；排在最後，G 停下不會讓 D44 回歸作廢。焦點事件算受干擾。
  - 代價：G 在開跑後約 2.6 小時才開始，螢幕狀態要撐得比較久（`caffeinate -dims` 防止顯示器睡眠）。
  - 替代：照 D80 的 I → G → D。
- **U9 session 期間的 PR 與推送**（D45 檢查 9）——D81 依建議採用：不開 PR、不推非文件；如果有 PR，先暫停 auto-fix；`ls-remote` 逾時記成 `remote_error`（開跑時停下，結束時判問題）。
  - 替代：不寫明，只靠結束時的檢查（作廢的風險較晚才發現）。
- **U10 主要基準與 R2**（D45 檢查 MUST-FIX 3）——D81 依建議採用：主要基準＝修好之後的資訊流；全部普查降為次要基準；R2（頻率與歸因重做）移到 v7.1 A3。
  - 替代：維持全部普查為主要基準（和事故紀錄 `:222` 不一致）。
- **U11 FireGate 重推條件 10 的盲點**（D45 檢查 10）——D81 依建議採用：本批只判 Cooldown；R<earliest、R>earliest＋7 記為量不到。
  - 替代：改產品讓 GUI 取得 earliest（違反「失敗只保留、不改程式」，不建議）。
- **U12 c2-after 的傳遞匯入**（D45 檢查 MUST-FIX 4）——D81 依建議採用：c2-after 的 52 檔全部列入凍結清單，每項之前核對；新增「清單內的檔案中途被改」的模擬與突變。
  - 替代：維持只核對 2 個（v1 草稿；8 個在載入時執行的模組沒有凍結）。

#### 執行步驟（核准後）

```
##### 字面值一律抄宣告，不從檔案重算
P=build/target/_build/test/logs/pvp-v7-16-prep-20261014
cp $P/declaration-draft.md $P/declaration.md          # 核准的版本；它的 sha256 就是 DECLARATION
FROZEN=8263d38c5497f909477e4893dd5190fe45771ff047da2edc0bcd23e6ecba2272
ARTIFACTS=f0a87dd78827a42dfab75546b69a73785d35442019fca03893775d4824f3b3cc
DECLARATION=<核准時 declaration.md 的 sha256>
### judge_16.py = 29258be1ba4e63e761184b59ecbfbad5c646481198c953801861b55f3b6671af
### judge_16.DRIVER_SHA = ebc7e28211f283b749b4a34c4dd2757f971a64b247b1780986253274faf283ff（run_16.py）
ioreg -n Root -d1 | grep -o 'IOConsoleLocked[^,]*'     # 必須是 No
pgrep -l 'ninja|cmake|clang|go build'                  # 必須沒有建置在跑（clangd、gopls 這類語言伺服器不算）
### 打開勿擾（專注模式）；做不到時在 HANDOFF 記「未確認勿擾」
python3 -I -B $P/run_16.py --preflight --frozen-sha $FROZEN --artifacts-sha $ARTIFACTS --declaration-sha $DECLARATION
caffeinate -dims python3 -I -B $P/run_16.py --output build/target/_build/test/logs/pvp-v7-16-l2-<日期> --frozen-sha $FROZEN --artifacts-sha $ARTIFACTS --declaration-sha $DECLARATION
python3 -I -B $P/judge_16.py --session build/target/_build/test/logs/pvp-v7-16-l2-<日期> --frozen-sha $FROZEN --artifacts-sha $ARTIFACTS --declaration-sha $DECLARATION
```

- L2 結束後、使用者的 LAN 測試之前，把 L2 的結果補進 `plans/v7/LAN_TEST.md`（LAN 測試包和量測沒有交互）。


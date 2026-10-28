# PvP v7.1 交接

更新：2026-10-21。Owner：`object_fps_pvp`。**狀態：v7.1 成為穩定基線（master `5ee499a`、CI #541 通過；[STABLE_BASELINE](STABLE_BASELINE.md)，D157）。剩 `after-re-evaluate` 的 37 項等你排序（詳見「現況」）。**
本文件是 v7.1 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)，盤點在 [清單核對結果](INVENTORY.md)。

## 代號對照

本文件主要給 agent 接手用。批次代號像函數名，下表是它的註釋；樹狀總覽與各批明細見 README。

| 代號 | 意思 | 出處 |
|---|---|---|
| D86～D157 | 使用者在 v7.1 的決定（接續 v7 的 D1～D85） | 本文件「決策紀錄」 |
| P1～P8、A1、A4、A5、B6～B11、C12～C17、D58-*、D-LOG、E1～E5、F1～F7、G1、G2、M0、TT2-B10、IP34-B12、LAN15、HB、N-* | 盤點的項目代號（使用者的初始清單＋新發現） | [INVENTORY](INVENTORY.md) |
| plan-sync-docs | 同步 master（PR #75～77），寫本目錄 | README「樹狀總覽」 |
| pv7-line | pv7 的 PR 線：`pv7-design`～`pv7-merge`（含 `edge-*` 六批），三角色同一個 PR，最後一次合併 | README「pv7-line 各批」 |
| pv7-design | 所有 wire 欄位與語意一次定完，產出 `protocol-v7.zh-Hant.md` 草案 | 同上 |
| D-1～D-7 | `pv7-design` 的待決事項（決定後轉成 Dxx） | [pv7-design](pv7-design.md)「等你決定」 |
| D-8～D-15 | Gateway 方向（D-8）與選 (a) 時接著要決定的事 | [gateway-direction](gateway-direction.md) |
| edge-* | 選 (a) 時的 Gateway 批次：`edge-results-resend`、`edge-input-passthrough`、`edge-actions-passthrough`、`edge-results-passthrough`、`edge-analyzers`、`edge-contract`、`edge-rate-limit`（K1 的修正，D119） | [gateway-direction](gateway-direction.md) |
| proto-rename-v7 | proto、Go 套件、CMake target 改名為 v7，版本值仍是 6 | 同上 |
| version-errors | P1：版本 6→7、版本不符的明確錯誤、凍結 pv7 版分析器 | 同上 |
| heartbeat-sync | P2：runtime link 心跳對時（只作診斷） | 同上 |
| time-echo | P3：Client↔Gateway 時間回聲 | 同上 |
| drop-report | P4：Gateway 丟棄回報（D49 第二階段） | 同上 |
| gateway-duties | （已由 `edge-contract` 取代，D94）P5：Gateway 狀態的職責分類與正式條文 | 同上 |
| cause-records | E2、動作拒收回覆、Leave 原因 | 同上 |
| hitbox-fields | 骨骼 hitbox 的 wire 欄位，填等同現行的值 | 同上 |
| pv7-checkpoint | 行為改變前的整體回歸 | 同上 |
| a1-shot-step | A1：射擊照命令步判定，FireGate 退役（線上唯一的行為改變） | 同上 |
| pv7-merge | 四平台 L1 後合併 | 同上 |
| after-pv7 | `tt2-trace`、`log-contract`、`dead-letter-position`、`d44-regression-pv7`、`incident-close`、`observability-tools`、`re-evaluate` | README「after-pv7 各批」 |
| after-re-evaluate | 內容不變、評估後再排的項目（SDL 隔離、code smell、Windows LAN 等） | README「after-re-evaluate」 |
| ◆ | 事故後遺症（2026-10-10 Gateway 悄悄丟命令），v7.1 結束前全部結案 | README |
| pvN／vN | 網路協議版本／遊戲版本 | v6 交接 D20 |

## 現況（每次停下來時更新）

停下來等使用者、或回報里程碑之前，先更新本節再回報。「等你決定」只列需要使用者決定的事；決定之後移到決策紀錄，並從本節刪掉。

dev_log 的記錄規則（2026-10-23，使用者的決定，經規劃會話轉達）：

- 每批完成時，該批的 dev_log 與該批放在同一個變更裡提交，並列入該批的完成條件。
- v7.1 已完成批次的補寫由圖書管理員會話負責，規則也由它寫進 AGENTS.md。
- sdl-isolation（PR #85）的 dev_log 由本會話撰寫，隨 PR #85 提交：`docs/dev_logs/2026_10_23_pvp_v71_sdl_isolation.zh-Hant.md`（`9e36d0b`）。L2 完成時補上第 7 節的結果。

- **發行線（PR [#96](https://github.com/yojinn-io/GYO-Engine/pull/96)，draft）**：windows-long-path 完成（P5-6 實機 2026-10-27 通過；p1 兩條在 MSVC killed；p4 在 manifest 下成為等價突變而退場，D212）；quick-close 完成（Windows 10 次通過）。剩下：worker 的 Windows 失敗（D216、D217；worker-10x 由使用者在 Windows 上跑，D220）、lan15（D208 延期；D219 起不再等 OS）、四平台 L1（run 40644602172）、PR #96 合併。發行線不再併入 OS 批次（D218、D219）。
- **發行線的 quick-close**：D202 選 q1、門檻核准；D203 加 stalled ≤1 s 判定與 CTest `object_fps_pvp.close_timing`；macOS 驗收通過，Windows 與 D201 一起跑（指令在 [quick-close.md](quick-close.md)）。發行線一開始要在本機背景跑完整突變目錄（D185，4～6 h），這次沒有啟動：caffeinate 關著，Mac 可能中途睡眠。
- INVENTORY 的 G2（「時鐘到網路路徑」系列紀錄）照 D159 先不做，不在待決之列。
- **之後要做**：`lan15-windows`（含 outage-resync 的 L3，D179）延到本線全部、`client-silent-checks`、`rebind-on-new-port` 完成之後（D181；原定 2026-10-26 前後）。已知問題 K1～K10 的去處見 D181。v7.1 全部完成後發一次 release（使用者 2026-10-24；v7 的 `object_fps_pvp-v1.2.0` 已取消，D91 說下一次發行在 v7.1 之後）。 發行線一開始就在本機背景跑完整突變目錄（D185）。
- **完成**：「系統其餘＋觀測＋測試」線（PR [#95](https://github.com/yojinn-io/GYO-Engine/pull/95)，2026-10-25 合併，merge commit `37d533d`，L1 run 40179729383 四平台成功）。完成：`silent-branch-asserts`（D180）、`client-silent-checks`（D183）、`cxx-flag-removal`（L1 run 40110961077）、`rebind-on-new-port`（D188；L2 `pvp-v71-rebind-l2-20261025` 12／12 P，L1 run 40117207196）。`contract-readers`（含 K7）完成。`trace-no-overwrite`（含 K8）完成。`analyzer-converge`（D192）完成。`token-tests` 完成。`client-path-tests`（含 K9，D200 (b)）完成。`event-watch-overlap`（K11，D200 核准）完成。前一條線：`outage-resync` 完成，PR [#90](https://github.com/yojinn-io/GYO-Engine/pull/90) 2026-10-24 合併（merge commit `03453e2`，完整 L1 run 40089491812 四平台成功；D171～D179）。L3 移到 `lan15-windows`（D179）。
- **關卡改名（2026-10-23）**：`outage-resync` 的關卡 G1、G2 和 INVENTORY 的項目代號 G1、G2 撞名，改成 `gate-characterize`（舊 G1）、`gate-contract`（舊 G2）。決策紀錄與 commit 訊息裡指這兩個關卡的 G1、G2 照此讀。
- **任務線總覽**：
  - 完成：pv7-line、after-pv7（PR #82）、sdl-isolation-plan 與 sdl-isolation（PR #85）、black-map-observation（PR #89）、記錄器分支（PR #93）、outage-resync（PR #90）。
  - outage-resync（PR #90，已合併）取代了 after-re-evaluate 的 `substitution-single-store`，並把 `wake-reconnect` 的「短於 T_max 的睡眠＝中斷」（w1）併進來；其餘的 wake-reconnect 取消（D182）。
  - 未開始（after-re-evaluate，D159、D181、D182、D186）：觀測工具的 `contract-readers`（含 K7）、`trace-no-overwrite`（含 K8）、`analyzer-converge`；測試的 `token-tests`、`client-path-tests`（含 K9）、`event-watch-overlap`（K11）；發行的 `windows-long-path`、`quick-close`、`lan15-windows`（含 K10）。K12 的去處待定。系統與引擎的 4 批已在 PR #95 完成。D159 的三條 PR 線中，「系統其餘＋觀測＋測試」是 PR #95，「發行」還沒開。
  - 記錄器分支：2026-10-24 經 PR #93 合進 master（`489905d`），`worker-probe-repair` 的修正（`d2844a5`）也一起進了 master。之後本文件在 outage-resync 線上更新，隨 PR #90 合併。
  - 不做：INVENTORY 的 G2（D159）。
  - 交給 v8：B6、A4（連同 B10、N-correct-invariant-assert、E3、E5）、N-analyzer-literal-105、E1-2 的剩餘、預熱幀需求、outage-resync 的 P2 兩個已知例外、I-3（防竄改的 session 證明）。
- **臨時測試（只記錄）**：2026-10-16 的全 Windows LAN 臨時測試（`16216ff`）可以穩定遊玩；見規劃紀錄同日條目與 INVENTORY 的 F7、`N-summary-counter-wrap`、`N-hit-feedback-not-logged`、`N-windows-max-path`。
- **發行的完整突變（D185）**：完成（2026-10-25T10:01Z～15:17Z）。Engine 133／133（另 5 個 Windows 專用待 D201）；產品 643／654，非 killed 的 11 個：8 個是負載造成（單獨重跑 killed）、2 個期望訊息過時（`ed63c24` 更新）、1 個測試失去檢出力（`csc-r5-uncounted`，worker 的過期窗口檢查被 C17-3 削弱，已修，worker 為檢查的 47 個突變 47／47 killed）。擋發版的條件已解除。dev_log：`docs/dev_logs/2026_10_26_pvp_v71_release_full_mutation.zh-Hant.md`
- **今晚 Windows 機器的清單（D201、D203、D206、D207；全部在發行線 `claude/pvp-v71-release`，建置目錄記為 `<B>`）**：2026-10-27 第 1～5 項跑完（`3b612cf` 的程式碼，MinGW 與 MSVC 兩套建置），結果見 dev_log `docs/dev_logs/2026_10_27_pvp_v71_d201_windows.zh-Hant.md`。突變 killed 25、survived 6、未跑 3；第 3、4 項通過；第 5 項的 Ctrl+Break 與 console_close 不補（使用者）。下面的清單保留原文：
  1. Windows 專用突變：`python docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir <B> --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json --only p1-no-long-path-form p1-long-path-from-too-high p4-manifest-ansi-path os-f4-windows-acp os-f5-windows-notfound` 加上 OS 子系統列的 os-f9、os-f10、os-f11、os-w2、os-w3、os-w6、os-w8、os-w9、os-w10、os-x7～os-x11、os-g16、os-p5、io-w3、tt-x3、os-c4、os-c5、os-c6、os-c8、os-c9、os-g18（id 照目錄的全名）。p1 兩條與 p4 回本會話，其餘回 OS 子系統的會話。
  2. 確認案例真的跑了（`-s`）：`gyo_os_tests -s -tc="*resolve*"`（DefineDosDeviceW 的磁碟代號、`\\localhost\C$`）、`gyo_os_tests -s -tc="*symbolic*"`、`engine_tests -s -tc="*symbolic link*"`、`gyo_ui_editor_core_tests`（輸出有「symbolic links unavailable」就是提早返回）。回 OS 子系統的會話。
  3. 實機驗收（P5-6）：發行包解壓到本機磁碟（不要用網路共用），P＝156 與 P 約 210 各啟動 Client 一次，事故 log 第 2 行的錯誤不再出現。回本會話。
  4. quick-close：`python build/acceptance/object_fps_pvp/close_timing.py --probe <B>/acceptance/object_fps_pvp/gyo_object_fps_pvp_worker_probe.exe --runs 10 --output close-timing-windows.json`，exit 0 才通過，不重跑。回本會話。
  5. 主控台：Match 在主控台裡用 Ctrl+C、Ctrl+Break、關閉視窗結束，log 最後一行 `end version=1 reason=signal exit_code=0`；`ctest -R "^(os\.console_close|io\.unc_localhost|io\.unc_127\.0\.0\.1)$"` 預期 Passed（Windows Terminal 下 console_close 可能 Skipped，改用 conhost）。回 OS 子系統的會話。
  6. lan15（D208：延期，日期未定）。
  （D209、D215：lan15 等 OS 統一子系統全部完成後做，包括 os-net-errors、os-close；計時 L2 與基準改在 `claude/os-windows-run` 跑。）
- **後續宣告要寫的「範圍的限界」（AGENTS.md §14 冒頭，PR #98 起）**：lan15 與之後的宣告，要列出已記錄為量不到或判定不了的經路，直到記錄解消：Windows 的 symlink 案例（MinGW 與沒有開發人員模式的 MSVC 都提早返回）；`os.console_close`（CI 與使用者的機器都 Skipped）與 Ctrl+Break（使用者的鍵盤沒有 Break 鍵）；quick-close 量的是產品的網路層，不是整個 Client 程序（D202）；`close_timing` 不在 CI 跑（D204）；ACP 非 UTF-8 時的行為在 manifest 之下不再測得到（D212）。
- **最後更新**：2026-10-25，PR #95 合併、發行線 PR #96 開啟、完整突變開始。

## 閱讀入口

1. [README](README.md)：定位、樹狀總覽、各批範圍與驗收、工時、跨分支依賴、取消的項目。
2. 本文件：先看「現況」，再看決策與規劃紀錄。
3. [INVENTORY](INVENTORY.md)：盤點的每一項、證據與處置。
4. v7 的紀錄：[v7 交接](../v7/HANDOFF.md)（D1～D85）、[STABLE_BASELINE v7](../v7/STABLE_BASELINE.md)、[事故紀錄](../v7/Incident/2026-10-10-gateway-silent-drop.md)。
5. 規則：AGENTS.md §2「Derivation Order：Feature → Contract → Architecture」（PR #77）。

## 決策紀錄

| 編號 | 決定 | 理由與出處 |
|---|---|---|
| D86 | （2026-10-15）「功能 → 契約 → 架構」是有限度的參考順序。主對話提出三個限度：需要 pv7 資料才能校準的功能排在契約後；架構項目中和 wire 有關的決定提前到契約設計；A1 最先決定。使用者：「嗯，就這樣」 | 使用者：「把功能想任務往前推，架構調整放後面。否則功能項會一直被卡住」。之後由文件會話寫進 AGENTS.md §2（PR #77） |
| D87 | （2026-10-15）**v7.1 把架構和契約穩定下來，v8 只做功能和 editor。** v8 目前看得到的契約需求只有骨骼 hitbox：pv7 只做 hitbox 的契約支持，v8 才實作。其他 v8 候選不預先定契約。SDL 隔離不是移除 SDL，是讓產品不直接呼叫 SDL | 使用者：「否則 v8 有要做功能又要改契約，驗收難度大」「v8 契約目前可見的就是 hitbox，其他還都沒影子呢」 |
| D88 | （2026-10-15）契約是架構的一部分。「功能 → 契約 → 架構」是標準推導過程：架構支撐契約，契約改變時架構不一定要變。v7.1 同時處理契約與架構，是 2026-10-10 事故的後遺症；不在 v7.1 處理乾淨，v8 的功能製作很可能難產 | 使用者的說明；AGENTS.md §2 |
| D89 | （2026-10-15）v8 的三個 editor：UI Editor（已有）、GridMap Editor、Assets Editor。後兩者是通用工具，PvP 是實驗場，完成後其他遊戲可以參考；格式由 Engine 或工具層擁有，計畫放 Engine 計畫夾。GridMap 的 2D、3D 都支援，建地圖時選。對 v7.1 的影響不大，v7.1 不做內容格式的讀取端 | 使用者的回答 |
| D90 | （2026-10-15）**v7.1 是 hotfix：先檢測、再評估。** 使用者排定 pv7 與契約的順序：P1 版本升級（資訊分離）→ P2 心跳對時、P3 時間回聲（檢測功能）→ P4 丟棄回報（檢測完成後優先級最高）→ P5 Gateway 職責 → E2、動作拒收回覆與 Leave 原因 → 之後是日誌 Data Contract、P6 死信定位、hitbox 契約支持。**做法 (a)**：所有 wire 欄位一次設計，照這個順序在同一條 PR 線分批實作，最後一次合併。採用 A1，M0 隨之取消；輸入防偽造取消。其餘項目在 pv7 與契約完成後重新評估；缺工具時告訴使用者，由使用者判斷 | 使用者：「沒有正確的檢測數據，就沒法評估事故和故障。評估不了，事故再如何計算都解決不了」「這種情況下，升契約版本沒有意義」。背景見下面的「規劃紀錄」 |
| D91 | （2026-10-15）計畫的調整：A1 的手感盲測不做；proto 照慣例改名；v7 的 3 人 LAN 測試不跑（「先處理完問題」），所以 v7 不發行 v1.2.0。after-pv7 的順序照主對話的建議（`tt2-trace` → `log-contract` → `dead-letter-position` → `d44-regression-pv7` → `incident-close` → `observability-tools` → `re-evaluate`），使用者：「OK」。主對話對 pv7-line 的 4 個調整，使用者沒有提出異議：hitbox 欄位排在 A1 之前；凍結 pv7 版分析器併進 `version-errors`；A3／A5 的轉送與否在 `pv7-design` 決定；proto 改名獨立成一個 commit | 理由見 README 各批的備註 |
| D92 | （2026-10-15）核准開始 `pv7-design`，用 ultracode（3 個設計方向、1 位評審、1 次對抗式檢查），另做 1 次 xhigh 對抗式檢查 | 使用者：「開始 pv7-design，用 ultracode，另做 1 次 xhigh 對抗式檢查。開始前先說明檔位與時間估計。」；v7 D45 |
| D93 | （2026-10-15）pv7 定稿前先評估 Gateway 的方向（[設計事故 2026-10-15](Incident/2026-10-15-gateway-state-drift.md) 行動項目 1），用 ultracode（3 位評估者各負責 (a) 無狀態邊緣、(b) 遊戲 UDP 直接到 Match、(c) 維持現狀，1 位評審、1 次對抗式檢查）；之後重評 `pv7-design` 的 D-1～D-7 與 pv7-line | 使用者經分支會話：「轉給原規劃會話後，重新評估一次計劃。」；使用者：「用 ultracode 評估 Gateway 的三個方向（3 位評估者、1 位評審、1 次對抗式檢查），之後重評 D-1～D-7 與 pv7-line。」 |
| D94 | （2026-10-16）**Gateway 回到無狀態邊緣**（[gateway-direction](gateway-direction.md) D-8 選 (a)）：Gateway 只翻譯、逐則檢查、原樣轉送；內部訊息保留身分並冪等，去重、帳本與結果重送由 Match 負責；Gateway 只留連線與 session 的軟狀態。順序守住 D90（丟棄回報排在 Gateway 改動之前）。接受 pv7-line 約 170～179 h〔推論〕，約為 README 原估 91 h 的 1.9 倍。這是設計前提的改變（[設計事故 2026-10-15](Incident/2026-10-15-gateway-state-drift.md)），依 stable-version-first 在 v7.1 處理：v7 是穩定基準 | 使用者：「D-8 選 (a) 無狀態邊緣，順序守住 D90，接受 pv7-line 約 170～179 h。」 |
| D95 | （2026-10-16）`pv7-design` 與 Gateway 方向的其餘決定全部照建議：D-1 不再需要；D-2～D-6 照建議；D-7 確認排除清單（第 2 項改為「逐命令的完整丟棄身分」）；D-9 順序守住 D90，edge 批「結果先、動作後」，丟棄回報先照現行出口實作再縮減；D-10 IPC 依到達順序、controls 不丟、資料每位玩家有界 FIFO（丟最舊並回報）、結果 30 Hz 由 Gateway 只記上次送出時刻來守；D-11 解碼成功就提交活性；D-12 移除 snapshot 守衛 D1、D2；D-13 丟棄回報上 wire（約 29 個原因），逐出時佇列中的資料轉給 Match；D-14 Gateway 約束條文與白名單守衛；D-15 保留兩份 proto 加 adapter。檔位：`pv7-design` 的 IPC 送達語意做 1 次 xhigh 對抗式檢查、`edge-input-passthrough` 局部 xhigh，都同意。設計事故的行動 4（AGENTS.md）交給文件會話 | 使用者：「D-9～D-15、重評後的 D-1～D-7 全部照建議；pv7-design 做 1 次 xhigh 檢查、edge-input-passthrough 局部 xhigh 都同意；事故行動 4 交給文件會話。修訂草案與 README 後 commit 並推送。」 |
| D96 | （2026-10-16）**protocol-v7 定稿**（§2a 的 xhigh 檢查已套用：結果的 30 Hz 改為 Gateway 單槽持有、Match 最小 id 優先；中間 commit 轉送 ACK-only 批；結果重送不刷新期限；唯一 UDP 寫出者；給已移除玩家的結果不算損失；鍵對應與事前宣告的例外；FIFO 上限準則不少於 64 則窗口；守衛加強）。`PlayerInput`／`ActionBatch` 加 Client 的 GYOP 序號作為診斷欄位（runtime 7、5）。接受 pv7-line 約 177～191 h（加上序號欄位約 178～193 h）。定稿後改 wire 要先徵求同意 | 使用者：「草案定稿；PlayerInput／ActionBatch 加 Client 的 GYOP 序號作為診斷欄位；接受 pv7-line 約 177～191 h。更新文件後 commit 並推送。」 |
| D97 | （2026-10-16）開始 `proto-rename-v7`；規劃文件併進 pv7-line 的 PR，pv7-line 在 `claude/pvp-v71-plan` 接著做 | 使用者：「開始 proto-rename-v7。規劃文件併進 pv7-line 的 PR，在這個分支接著做。」；fewer-prs |
| D98 | （2026-10-16）開 pv7-line 的 draft PR #78（標題與內容用日語），開 Auto-fix；確認 `proto-rename-v7` 的四平台 L1 後開始 `version-errors` | 使用者：「開 pv7-line 的 draft PR（標題與內容用日語），讓四平台 CI 跑 L1。」「為 PR #78 開 Auto-fix。」「CI 可以了」「開始 version-errors。」 |
| D99 | （2026-10-16）protocol-v7 §1 的兩處條文照建議修改：「版本不符時的告知」加上實作範圍的附註（只有 Gateway 在讀 Ready 時送告知；Ready 之後的不符直接關閉、不送告知）；Gateway 拒絕 join 的 log 行加上 `remote=<addr>`，和既有的 `join refused remote=<addr> code=<code>` 同一系。wire 與實際行為不變 | 使用者：「契約兩處都照建議：§1 加上版本告知範圍的附註；拒絕 log 行加上 remote=<addr>。」；`version-errors` 的 ultracode 審查 architecture-F3、contract-F5 |
| D100 | （2026-10-16）開始 `heartbeat-sync`（檔位 high，時鐘域局部 xhigh，估 9 h） | 使用者：「開始 heartbeat-sync。」 |
| D101 | （2026-10-16）會話的 ultracode 開關不覆蓋計畫的檔位：每批照 README 樹狀總覽標的檔位做；標 ultracode 的批才用，標「ultracode（先問）」的到了先問一次；其餘不逐批確認，也不因為開關另開 workflow | 使用者（經分叉會話）：「一開始計劃裡有檔位規劃，以那個為準，計劃太多了，我沒法每一個都確認。」起因：`version-errors` 在開關打開後，事前沒問就跑了審查 workflow |
| D102 | （2026-10-16）開始 `time-echo`（檔位 high，估 4 h）；`heartbeat-sync` 的睡眠那段 L2 等之後的 L2 一起做 | 使用者：「開始 time-echo。睡眠那段 L2 等之後的 L2 一起做。」 |
| D103 | （2026-10-16）開始 `drop-report`（檔位 high，列舉與上限局部 xhigh，估 13～16 h） | 使用者：「開始 drop-report。」 |
| D104 | （2026-10-16）protocol-v7 §2「診斷訊框的計時與佇列」改成配合實作：GatewayDropReport 不經過單槽，計時器只標記一輪，writer 寫出前一刻才組回報（序號、樣本、dwell 終點在寫出時決定）。wire 與 Match 的檢查不變 | 使用者：「1a」；`drop-report` 的 xhigh should-3 |
| D105 | （2026-10-16）核准 `heartbeat-sync`、`time-echo`、`drop-report` 新增的 Gateway 欄位為診斷狀態（D-14 的 Architecture Delta）。條件：`edge-contract` 的白名單分三類——診斷狀態（判定路徑不得讀取）、單次呼叫的脈絡（`datagram`，不得跨資料包保留）、測試縫隙（正式路徑只用預設值）；分類先寫進 protocol-v7 §2a | 使用者：「2a（含條件）」；`drop-report` 的 xhigh should-4 |
| D106 | （2026-10-16）開始 `edge-results-resend`（檔位 high，通道計時局部 xhigh，估 7～8 h） | 使用者：「開始 edge-results-resend」 |
| D107 | （2026-10-16）protocol-v7 §9 的 `edge-results-resend` 驗收改成「Client 收到的內容不變；新裁決最多晚 1 個間隔」，並把這個延遲列進 `edge-results-passthrough` 事前宣告的 L2 對照。量測：無損模型 8 個種子，369 個裁決中 63 個晚到，最多晚 33 ms；未確認超過 8 筆時新 id 要等循環槽位（推論最晚約 7 個間隔），條文以括號註明 | 使用者：「§9 照 (a)」；`edge-results-resend` 的 xhigh should-1 |
| D108 | （2026-10-16）開始 `edge-input-passthrough`（檔位 high，局部 xhigh，估 20～26 h）。行為改變：L2 的事前宣告（門檻、試行次數、重試規則）在看結果之前經使用者核准（AGENTS.md §14.1），宣告前做門檻來源的對抗式檢查（D45） | 使用者：「開始」 |
| D109 | （2026-10-16）核准 `edge-input-passthrough` 的 L2 事前宣告第 3 版；第 10 節照建議：J4a 1.5 倍、跑 soak-long、heartbeat-sync 的睡眠那段在本 session 結束後接著做、次數照第 5 節（98 個項目）。確認 `ingress_cross_check.py` 只接受 protocol-v7 §9 宣告形狀的 I3 失敗（AGENTS.md §14.4） | 使用者：「核准 edge-input-passthrough 的 L2 事前宣告第 3 版，第 10 節照建議…ingress_cross_check 的 I3 例外也確認。」 |
| D110 | （2026-10-17）L2 第 1 次 session 在第 58 項停下（B 樹 clean-60 的 I1 undetermined：關閉時被 reset，已知的競態）。依 §14.1 回報後選 (a)：只改 I1 的規則（任何情境，只有 `eof_before_final` 不成立、其他前提成立的 undetermined 只記錄），整個 session 重跑；第 1 次保留為證據 | 使用者：「照 (a)」 |
| D111 | （2026-10-17）開始 `edge-results-passthrough`（檔位 high，估 12～16 h）。行為改變：L2 事前宣告在看結果之前經使用者核准（§14.1），宣告前做 D45 門檻來源檢查；休眠重連記為體驗優化，之後調整 | 使用者：「開始 edge-results-passthrough。」「休眠重連紀錄到未結束事項中，這個屬於體驗優化的情況，給後面調整。」 |
| D112 | （2026-10-17）protocol-v7 §9 的「每隔一則丟一則時送達」界限改成 ≤2⌈n/8⌉＋2（原寫 ≤2⌈n/8⌉）。契約行為不變；原因：Client 只能連續 ACK，有缺口時 Match 不知道 Client 已有哪些較大的 id | 使用者：「D112 選 (a) 把 §9 改成 ≤2⌈n/8⌉＋2」 |
| D113 | （2026-10-17）核准 `edge-results-passthrough` 的 L2 事前宣告第 3 版；R3（下行延遲上限）只在沒受干擾的跑次停下，受干擾的只記錄；在使用者不用電腦的時段跑 | 使用者：「核准 edge-results-passthrough 的 L2 事前宣告第 3 版，第 10 節照建議…在我不用電腦的時段跑。」 |
| D114 | （2026-10-17）開始 `edge-actions-passthrough`（檔位 high，估 14～17 h）。行為改變：L2 事前宣告在看結果之前經使用者核准（§14.1），宣告前做 D45 門檻來源檢查 | 使用者：「開始 edge-actions-passthrough。」 |
| D115 | （2026-10-17）核准 `edge-actions-passthrough` 的 L2 事前宣告第 3 版（[edge-actions-passthrough-l2.md](edge-actions-passthrough-l2.md)）；在使用者不用電腦的時段跑，等使用者說要離開時再開始 | 使用者：「核准 edge-actions-passthrough 的 L2 事前宣告第 3 版。在我不用電腦的時段跑，等我說要離開時再開始。」 |
| D116 | （2026-10-18）使用者去睡覺期間，edge 系列照順序做完：`edge-actions-passthrough` 的 L2 判定 → `edge-analyzers` → `edge-contract`。照常在關卡停下（L2 不成立、需要使用者同意的 Design Invariants 條文等） | 使用者：「我想去睡覺了，期間edge系列按順序執行完吧。」 |
| D117 | （2026-10-18）修 Match 的 `IpcHost::Stop()`（經 `Close` 結束 session）與 rapid runner 的停止順序；預先核准 L2 宣告第 4 版（＝第 3 版＋這兩個修正的 A commit 與雜湊，門檻不變）。乾跑全部符合預期就換新目錄重跑 session，通過後繼續 edge 系列；有任何不符就停下 | 使用者：「核准：修 Match 的 IpcHost::Stop() 與 rapid runner 的停止順序；預先核准宣告第 4 版（＝第 3 版＋這兩個修正的 A commit 與雜湊，門檻不變）。乾跑全部符合預期就換新目錄重跑 session，通過後繼續 edge 系列；有任何不符就停下。」 |
| D118 | （2026-10-18）Gateway 固定窗口限速在 Gateway 停頓後丟掉合法輸入的問題（K1）記為已知缺陷，先完成功能；v7.1 發版前重新測試，並在發版前重新評估方案 (a)（token bucket） | 使用者：「記住這個問題，標記為已知缺陷，先完成功能。在v7.1發版前，從重新測試（紀錄這個解決方案（a）在發版本前重新評估方案）。」 |
| D119 | （2026-10-18）K1 的處理放在 `edge-contract` 之後，作為 edge 系列收尾的新批 `edge-rate-limit`：重新測試 K1、對照 Gateway 狀態白名單評估方案 (a)、需要就實作，並跑一次涵蓋 `edge-actions-passthrough` 與 K1 的 L2（新宣告，經使用者核准後才跑；不沿用第 4 版，也不加臨時的 K1 規則）。D118 的時間點從「發版前」提前到這裡。`edge-actions-passthrough` 標為實作與 L1 完成、L2 待 `edge-rate-limit` | 使用者：「嗯，放在 edge-contract 之後吧，作為 edge 系列收尾。到時候重新跑一次測試，和解決方案（a）的評估。」「OK，這個問題處理找建議。開始 edge-analyzers」 |
| D120 | （2026-10-18）`edge-contract` 的條文照目前的內容核准：network-architecture §7「Gateway：無狀態邊緣」（四條原則、11 個允許類別、禁止項（D120 前改寫成「Gateway 不得…（負責方）」，protocol-v7 §2a 同步）、守衛與變更規則、Replacement Criteria），N3 照草稿列為獨立的允許類別；死欄位 `queuedControl.at` 刪除 | 使用者：「…死欄位 queuedControl.at 可以刪除。條文看完了。目前先這樣。」 |
| D121 | （2026-10-18）K1 採用方案 (a)：通用層 session 的限速改為 token bucket（每秒補 120、容量 120），改 network-architecture 的契約條文；修到兩個 Red 測試轉綠、突變與 CTest 通過後，起草 L2 宣告並做 D45、乾跑，停下等使用者核准宣告 | 使用者：「採用方案 (a)：token bucket，每秒補 120、容量 120；改通用層與 network-architecture:950 的契約條文。修到兩個 Red 測試轉綠、突變與 CTest 通過後，起草 L2 宣告並做 D45、乾跑，再停下等我核准宣告。」 |
| D122 | （2026-10-18）M1 照 (a)：凍結分析器三處的 Gateway 窗口計數檢查（`max_session_window_packets ≤120`）換成限速器自己的拒收計數 `rate_limited_packets`＝0，gateway 模式也判；窗口計數只記錄；補測試與突變、重新凍結，之後寫工具與乾跑，停下等使用者核准宣告 | 使用者：「照 (a)：三處的窗口計數檢查換成 rate_limited_packets＝0（gateway 模式也判），max_session_window_packets 只記錄；補測試與突變、重新凍結，然後寫工具與乾跑，再停下等我核准宣告。」 |
| D123 | （2026-10-18）核准 `edge-rate-limit` 的 L2 事前宣告第 3 版（[edge-rate-limit-l2.md](edge-rate-limit-l2.md)）；在使用者不用電腦的時段跑，等使用者說要離開時再開始 | 使用者：「核准 edge-rate-limit 的 L2 事前宣告第 3 版。在我不用電腦的時段跑，等我說要離開時再開始。」 |
| D124 | （2026-10-18）開始 `cause-records`（檔位 high，估 8～9 h）：protocol-v7 §5 的 dwell、Client 的 GYOP 序號（D96）、snapshot 的兩段停留（D-4）、動作拒收回覆（D-6）、Leave 原因；行為不變（§9），以測試、突變、差分測試證明 | 使用者：「開始」 |
| D125 | （2026-10-18）核准 `cause-records` 的 Gateway Architecture Delta：`linkWrite` 加 `received`（照抄佇列項目的收包時刻，寫出時算 dwell）；`snapshotFrame` 加讀取 IPC 框架的時刻與解碼後的 client snapshot（每位玩家寫出前填 `gateway_hold_us` 重新編碼）；network-architecture §7「diagnostics carried with a queue item」的「只在它被丟棄時拿來回報」改成「被丟棄時回報，或在寫出時算成 dwell 填進同一則 IPC 訊息；都不被判定讀取」 | 使用者：「同意：linkWrite 加 received、snapshotFrame 加讀取時刻與解碼後的 snapshot（每位玩家重新編碼），並改 network-architecture §7 那一句條文。繼續 cause-records。」 |
| D126 | （2026-10-18）開始 `hitbox-fields`（檔位 high，驗證窗口局部 xhigh，估 8 h）：protocol-v7 §7 的 HitPart、`hit_part`、`last_hit_part`、`view_tick_q16`、`maximum_shot_damage`，§6 的 E、S 與影子紀錄（D-2）；判定與 digest 不變 | 使用者：「開始」 |
| D127 | （2026-10-18）`hitbox-fields` 影子紀錄的「命令 S 的來源與是否之後才到」照 (a)：不由 Host 記錄，離線對上 `match-commands.jsonl` 與 `match-ingress.jsonl`；改寫 `protocol-v7.zh-Hant.md` §6 那一句。`pv7-checkpoint` 的宣告把兩件事寫成判定：(1) 對接鍵要加上連線或時間順序，處理重連；(2) 對接率：每個 on_time 路徑的裁決都要對得到 resolved 事件，對不到就是工具無效，避免軌跡掉資料時悄悄少算 | 使用者：「選(a)，並且在 `pv7-checkpoint` 的宣告裡把兩件事寫成判定：1）對接的鍵要加上連線或時間順序，處理重連；2）要求對接率，例如『每個 on_time 路徑的裁決都要對得到 resolved 事件，對不到就是工具無效』，避免軌跡掉資料時悄悄少算。」 |
| D128 | （2026-10-18）`match-shots.jsonl` 每筆加 `time_ns`（和 `match-commands.jsonl` 同一個時鐘 `MovementTraceNowNs`），補測試與突變；格式仍是版本 1（還沒有工具讀它） | 使用者：「現在補上：match-shots.jsonl 每筆加 time_ns（和 match-commands.jsonl 同一個時鐘），補測試與突變。」 |
| D129 | （2026-10-18）開始 `pv7-checkpoint`（檔位 medium、分析 high，估 8 h）：A1 前的整體回歸（凍結 pv7 工具與 v7 基準比較，先扣掉三個 edge 批的差異帶）、K 與 S 取法的影子資料（D127 的兩項判定）。（更正：最初寫的「D44 事件回歸」是錯的，D44 回歸屬於之後的 `d44-regression-pv7`。）行為改變：L2 事前宣告在看結果之前經使用者核准，宣告前做 D45 | 使用者：「開始」 |
| D130 | （2026-10-18）`pv7-checkpoint` 的比較照 (b)：同一 session 兩棵樹交錯，v7 基準樹 `../GYO-Engine-v7final`（`6d3f287`）用 16-L2 的凍結工具判，現在的樹用凍結 pv7 工具判；門檻用本 session 的 v7 範圍；edge 類指標不和 v7 比。K 的影子資料照 (i)：只用已會寫 trace 的 runner（clean、network20／40、upstream、Gateway→Match 延遲注入），不重新凍結 `run_action_short.py` | 使用者：「照推薦，繼續」 |
| D131 | （2026-10-18）`pv7-checkpoint` 的影子資料照 (A)＋(B)：重新凍結 `run_action_short.py` 傳 `--movement-trace`（連點也產生影子紀錄）；action／network／quad probe 照 Client 規則填 E、S；補測試後再起草宣告。取代 D130 的 (i) | 使用者：「照 (A)＋(B)：重新凍結 run_action_short.py 傳 --movement-trace，並讓 action／network／quad probe 照 Client 規則填 E、S；補測試後再起草宣告。」 |
| D132 | （2026-10-18）`pv7-checkpoint` 宣告第 2 版第 8 節全照建議：故障情境的 D61 即時規則立旗標並計數、同一棵樹累計 ≥3 才停；on_time 對不上時先找產品行為的解釋，有解釋的只記錄（改寫 `protocol-v7.zh-Hant.md` §6 的 D127 那一句）；合法射擊 P95（C4、C5）只記錄；核准前在 A 上把沒跑過的項目各跑 1 次開發跑次（只看分類、鍵與 trace 完整性）。之後寫工具與乾跑 | 使用者：「第 8 節的 4 項全照建議：待定 1 選 (a)、待定 2 同意並改 protocol-v7 §6 那一句、待定 3 選 (a)、核准前在 A 上跑開發跑次（只看分類、鍵與 trace 完整性）。繼續寫工具與乾跑。」 |
| D133 | （2026-10-19）核准 `pv7-checkpoint` 的 L2 事前宣告第 3 版（[pv7-checkpoint-l2.md](pv7-checkpoint-l2.md) 第 10 節的雜湊：driver `35c7e8a3…`、judge `b3472772…`、artifacts `ab6005fc…`），立即跑 session。跑前確認 Chrome 沒有在跑、暫停 PR #78 的 Auto-fix | 使用者：「核准 pv7-checkpoint 宣告第 3 版（第 10 節的雜湊），現在開始跑 session。」 |
| D134 | （2026-10-19）計畫裡標了 ultracode 的批（含「ultracode（先問）」）直接開，不必再問；產出碰到契約、架構或 L2 宣告的關卡照樣停。另外交代：今晚把 pv7-line 往前推，做到下一個要使用者決定的關卡就停 | 使用者：「我先去睡覺了，今晚把pv7-line都跑完吧。」「對了，如果計劃裡要開ultracode你直接開就行。」 |
| D135 | （2026-10-19）`pv7-checkpoint` 以標籤修正後的診斷（`judgement-relabelled.json`：C1、C2、C3 全部成立，逐項 0 停止）為判定，結案。凍結的 `judge_cp.py` 不改；它的標籤錯誤（`schedule()` 兩棵樹共用 `extra`）記為工具缺陷。C6 的 `socket-path-1000ms` 旗標只記錄、不追查 | 使用者：「採用 (a)：pv7-checkpoint 以修正標籤後的診斷結果為判定。C6 的旗標只記錄。K 與 S 我另外回覆。」 |
| D136 | （2026-10-19）`a1-shot-step` 的冷卻保留上限改成 **K＝min(K_max, cooldownTicks)、K_max＝2 Tick**（射速將來是資料驅動，固定的 K 可能超過某把武器的冷卻），Client 的 S 取 **N**。改寫 `protocol-v7.zh-Hant.md` §6 的 K 條文；演算法與 Design Invariants 記進 `network-architecture.zh-Hant.md` §7「射擊判定的冷卻保留與命令步」；`pv7-design.md` 的設定表與 v8 README 的多武器候選（權威在 Match、射速以 Tick 存）同步。盲區：沒有量到「退回＋壓著冷卻邊緣連射」，全自動武器上線時重評 K_max | 使用者：「嗯，就這麼定。核准，記得把這個算法紀錄到設計文檔中。」（前文：「但是，我的武器想做成資料驅動，射速是會放在本地參數中的。」） |
| D137 | （2026-10-19）核准 `a1-shot-step` 的計畫第 2 版（[a1-shot-step.md](a1-shot-step.md)）：權威 digest 的 `combat-magazine/corridor` 預期差異與該筆 golden 重產（舊檔保留）、其餘 34 個情境與 29 個 golden 0 差異為停止條件；`PvpMatch` 的建構子加 `CombatRules` 參數（預設 `PvpCombatRules`，既有介面擴充）。開始實作 | 使用者：「核准 a1-shot-step 計畫第 2 版：combat-magazine 的預期差異與 golden 重產、PvpMatch 加 CombatRules 參數都同意。開始實作。」 |
| D138 | （2026-10-19）核准 `a1-shot-step` 的 L2 事前宣告第 3 版（[a1-shot-step.md](a1-shot-step.md) 第 11 節的雜湊：driver `dbd7e999…`、judge `f967e651…`、artifacts `034a16ee…`），立即跑 session。跑前確認 Chrome 沒有在跑、暫停 PR #78 的 Auto-fix | 使用者：「核准 a1-shot-step 的 L2 宣告第 3 版（第 11 節的雜湊），現在跑 session。」 |
| D139 | （2026-10-20）核准 `tt2-trace` 計畫第 1 版（[tt2-trace.md](tt2-trace.md)），第 8 節照建議：T-1 選 (a)（本批改接移動紀錄與 `LogFile`，位元組不變；`match-shots.jsonl`、`match-ingress.jsonl` 留給 `log-contract`）；T-2 照 Engine 計畫（生產者預先序列化）；T-3 由斷言與 terminate 處理先 `FlushNow()`，被訊號殺掉時遺失最後一批；T-4 同意 1 位 xhigh 審查 agent 檢查公開介面與關閉順序。開始實作 | 使用者：「核准 tt2-trace 計畫第 1 版，第 8 節照建議：T-1 選 (a)、T-2 照 Engine 計畫、T-3 由斷言與 terminate 處理先 FlushNow、T-4 同意 1 位 xhigh 審查。開始實作。」 |
| D140 | （2026-10-20）核准 `tt2-trace` 的 L2 宣告第 2 版（[tt2-trace.md](tt2-trace.md) 第 11 節）：L2-1 選 (a)，README 的「射擊 P99 增量 ≤1 ms」換成第 11 節的判定（T2-1、T2-3／4 粗閘門、T2-5、T2-9；其餘只記錄），README 同步改寫；L2-2 選 (a)，接受目前的生產者成本。開始做 L2 工具、開發跑次與乾跑，完成後出第 3 版 | 使用者：「核准 tt2-trace 的 L2 宣告第 2 版：L2-1 選 (a)，把 README 的射擊 P99 換成第 11 節的判定；L2-2 選 (a)，接受目前的生產者成本。開始做 L2 工具、開發跑次與乾跑，完成後出第 3 版。」 |
| D141 | （2026-10-20）核准 `tt2-trace` 的 L2 事前宣告第 3 版（[tt2-trace.md](tt2-trace.md) 第 11、13 節：driver `00c8c13e…`、judge `40aee8b1…`、artifacts `d5377b35…`），立即跑 session | 使用者：「核准 tt2-trace 的 L2 宣告第 3 版（driver 00c8c13e…、judge 40aee8b1…、artifacts d5377b35…），現在跑 session。」 |
| D142 | （2026-10-20）核准 `log-contract` 計畫第 1 版（[log-contract.md](log-contract.md)），第 9 節照建議：LC-1 (a) 拆成 core／records 兩個子批；LC-2 (a) `match-shots.jsonl`、`match-ingress.jsonl` 維持主執行緒每 10 秒寫出；LC-3 (a) 由 `log-contract.zh-Hant.md` 取代 protocol-v7 §1 的日誌規則；LC-4 照現狀定案 D58(1)(8)、D37，並明確確認 D58(3)(4)(5)；LC-5 (a) Engine `RoleThread::WaitBackend()`；LC-6 同意契約的 Design Invariants；LC-7 (a) 不加 Gateway 欄位、用導出的守恆式。檔位照寫，records 的 `tick_overrun` 局部 xhigh 同意。開始 core | 使用者：「核准 log-contract 計畫第 1 版，第 9 節照建議：LC-1 (a) 拆兩個子批、LC-2 (a)、LC-3 (a)、LC-4 照現狀定案並確認 D58(3)(4)(5)、LC-5 (a)、LC-6 同意 Design Invariants、LC-7 (a)；檔位照寫，tick_overrun 局部 xhigh 同意。開始 core。」 |
| D143 | （2026-10-20）`log-contract` core 與 records 之間的關卡：LC-10 (a) 同意在 Gateway 套件新增 3 個哨兵錯誤（`errRuntimeLiveness`、`errRuntimeWriteTimeout`、`errRuntimeVersionMismatch`，immutable table）與錯誤型別 `protocolError`（欄位 per-call context），套用 patch（Gateway Design Invariants 的 Architecture Delta）；LC-8 四項（rotation 拆分、逐筆到達事件、樣本流失、週期性 IPC 寫出延遲）都移到 `observability-tools`；LC-9 (a) records 結束時跑只記錄的 L2。開始 records | 使用者：「照建議：LC-10 (a) 同意新增 3 個哨兵錯誤與 protocolError，套用 patch；LC-8 四項都移到 observability-tools；LC-9 (a) records 結束時跑只記錄的 L2。開始 records。」 |
| D144 | （2026-10-20）核准 `log-contract` 只記錄 L2 的宣告第 1 版（[log-contract.md](log-contract.md) 第 13 節，工具 `60af302b…`），立即跑 | 使用者：「核准 log-contract 只記錄 L2 的宣告第 1 版（工具 60af302b…），現在跑。」 |
| D145 | （2026-10-20）`dead-letter-position` 選 (a+)：維持 D57（兩份契約、一套詞彙、一套原則），`log-contract.zh-Hant.md` 加 `drop_` 鍵 ↔ `DropReason` 的對應表，再補反方向的 Go 守衛（沒有 `DropReason` 的 `drop_` 計數器＝宣告的只寫日誌集合）與突變。`log-contract` 的四平台 L1 通過後實作 | 使用者：「dead-letter-position 選 (a+)：維持 D57，文件加對應表，再補反方向的 Go 守衛與突變。L1 通過後實作。」 |
| D146 | （2026-10-21）核准 `d44-regression-pv7` 計畫第 1 版（[d44-regression-pv7.md](d44-regression-pv7.md)），DR-1～DR-9 全部照建議：單樹＋第 16 批普查基準；母體照第 16 批 D 部分加 x1、ramp 各 15 次；沿用 E1～E3；Gateway→Match 短停頓只記錄；x1、ramp 計入停止條件 5；有事件時照第 16 批（≤30 分有限定位，達停止條件停下給 (a)／(b)／(c)）；Windows／Linux 的 E1-5 交給 `lan15-windows`；v7final、c2-after 保留到 `re-evaluate` 結束；D45 檢查 1 位 xhigh | 使用者：「核准 d44-regression-pv7 計畫第 1 版，DR-1～DR-9 全部照建議。開始做工具。」 |
| D147 | （2026-10-21）核准 `d44-regression-pv7` 的 L2 宣告第 2 版（[d44-regression-pv7.md](d44-regression-pv7.md) 第 8 節；driver 9ad263fd…、judge d0610c61…、artifacts cf992c74…），兩個宣告點照寫（clean 跑次的逐出與限速拒絕列為使用者旗標、受干擾只記錄） | 使用者：「核准 d44-regression-pv7 的 L2 宣告第 2 版（driver 9ad263fd…、judge d0610c61…、artifacts cf992c74…），兩個宣告點照寫。現在跑 session。」 |
| D148 | （2026-10-21）使用者睡前的指示：after-pv7 盡量在早上前跑完，沒有問題就照建議走；範圍是 `d44-regression-pv7` 結案、`incident-close`、`observability-tools`，`re-evaluate` 由使用者隔天自己看。session 停在停止條件時照宣告停下等使用者 | 使用者：「我去睡覺了，早上之前，盡量把after-pv7跑完。如果沒有問題，就直接找建議走就好。」「那你做1-4就好。5明天我自己看看。」 |
| D149 | （2026-10-21）`d44-regression-pv7` 照 DS-1：x1／ramp 的注入無效改成不計數、累計 3 次才停；先做 §14.4 的 1～5 與審查，交使用者核准，再整個重跑 | 使用者：「d44-regression-pv7 照 DS-1：x1/ramp 的注入無效改成不計數、累計 3 次才停，做 §14.4 的 1～5 與審查後交我核准，再整個重跑。」 |
| D150 | （2026-10-21）核准 `d44-regression-pv7` 的 DS-1 宣告補遺（[d44-regression-pv7.md](d44-regression-pv7.md) 第 12 節；judge de62a759…、driver 9ad263fd…、artifacts cf992c74…），停止條件 1b 照第 12 節；整個重跑 session | 使用者：「核准 d44-regression-pv7 的 DS-1 宣告補遺（judge de62a759…、driver 9ad263fd…、artifacts cf992c74…），停止條件 1b 照第 12 節。現在整個重跑 session。」 |
| D151 | （2026-10-21）session 第 2 次的停止原因：使用者在 Zed 裡 review 程式碼時切換了主 checkout 的分支，不知道會影響 agent。使用者：可以繼續。照建議 DR3-1：不改工具，原樣整個重跑（`pvp-v71-d44-l2-20261021c/`），期間不切換主 checkout | 使用者：「抱歉我在zed中review代碼，不知道會連動影響Agent，你可以繼續。」 |
| D152 | （2026-10-21）`observability-tools` 的決定：OT-0a 選 (a)，相位決策直接寫進 `*-commands.jsonl` 的新 kind、升 schema，凍結讀者一起改，不另開檔案、不留相容層（「目前沒有發佈，不需要背這個技術債」）；其他照建議：OT-0b E1-6 納入（OT-6）、`N-trace-overwrite-on-restart` 移到 `re-evaluate 之後`；OT-0c 新凍結清單從下一個 L2 生效；OT-0d 注入正式模式維持現狀；OT-0e 三個 RTT 階段維持同一個種子（沒有建議，照「預設行為不變」） | 使用者：「OT-0a 直接改就行，目前沒有也沒有發佈，不需要被這個技術債。其他的照建議執行。」 |
| D153 | （2026-10-21）OT-0e（分支會話討論後由使用者決定，轉給主會話）：impairments 的三個 RTT 階段維持同一個種子，工具不改、不加「每個階段換種子」的選項（三個階段是只差 RTT 的受控對照）；下一個 L2 的宣告改成每次跑次用不同的基底種子（impairments 與遊戲的 network20／40，例如第 k 次 DEFAULT＋1000×k，事前宣告清單，宣告前逐一乾跑確認 `random_dropped`>0），讓重複跑次同時抽樣丟包型態。細節在 [observability-tools.md](observability-tools.md) 第 7 節 | 使用者（分支會話「btw: 這個你如何考慮？」，經該會話轉達）：「OK，轉給主會話」 |
| D154 | （2026-10-21）`re-evaluate`：同意第 2 版的判斷，但 B6、B7、A5 先補量再定（門檻 105 重推、快速正向回收隨 A5；N-phase-1-7 第 3 點隨 B6）；其他可以證明的直接處理（結案：N-respawn-reject-volume、N-conflict-window-permanent-loss、B9、N-quality-reference-age-sampling、N-09b-followup-measures；取消：B8；N-phase-1-7 第 6 點）；還需要的 A4、N-analyzer-literal-105 交給 v8；N-phase-estimator-wan-noise 等 WAN 測試。事故紀錄加後日談，只收和事故相關的項目 | 使用者：「同意 re-evaluate 第 2 版的判斷。12 項照寫結案或取消，重開條件照寫。還需要的 2 項交給 v8。」「等等，你把取消理由給我看一下呀。。。」「B6、B7、A5先補量再定，其他可以證明的，就直接處理掉，順便給事故紀錄加一個日後談，這個目錄就是設計失敗後不停加補丁的傷疤。」「當然，要選取相關事項，不是事故中的就不加了。」 |
| D155 | （2026-10-21）`re-evaluate` 補量之後：B6 交給 v8 重新規劃（不沿用 09b 的 S／M；09b 1-7 第 3 點隨它）；B7、A5 取消（A5 照設計保留，連同門檻 105 重推、快速正向回收）。**`re-evaluate` 結案** | 使用者：「OK，那是否需要重新規劃一下B6的計劃，如果直接沿用之前測方案，我怕達不到最好的效果。」「B6交給v8吧。B7、A5 取消就好。」 |
| D156 | （2026-10-21）PR #82（after-pv7 線：`tt2-trace`、`log-contract`、`dead-letter-position`、`d44-regression-pv7`、`incident-close`、`observability-tools`、`re-evaluate`）轉 ready 並合併。最後一次程式變更的四平台 L1 是 #532（`d5a4cec`），之後只有文件 | 使用者：「那沒問題了。#82合併。」 |
| D157 | （2026-10-21）v7.1 的穩定基線：master 的 CI 通過之後，v7.1 成為穩定基線（v5、v6 是事故前的缺陷版本），寫正式的 `STABLE_BASELINE.md`（參照 v7）。`N-v7-skipped-acceptance`：v7 跳過的驗收不補做，照樣列為「跳過（使用者決定）」。LC-8 四項（D143 移到 `observability-tools`，計畫裡漏列）收尾：rotation 拆分取消、逐筆到達事件跟著 B6 交給 v8、樣本流失取消、週期性 IPC 寫出延遲取消；INVENTORY 另外更正 records 已做完的 4 列 | 使用者：「v5、v6在事故前是缺陷版本，v7.1 CI 過了就變成穩定基線。」「寫成正式的v7.1基線文件，。參照v7的STABLE_BASELINE.md」；N-v7-skipped-acceptance 選「不補，照樣列跳過」；「這些收尾處理了吧，你看要不要補？」「OK，就照這樣。」 |
| D158 | （2026-10-21）`after-re-evaluate` 的判斷（[after-re-evaluate.md](after-re-evaluate.md) 第 2 版）：照第 1 版，另外 C12 併進 N-token-test-gaps，F4 取消並記成「沒有檢測手段」。還需要的 20 列依使用者的優先序（系統與引擎 → 觀測工具 → 測試 → 發行）排成 15 批的草案，等使用者核准；B10、N-correct-invariant-assert、E3、E5 跟 B6／A4 交給 v8。HANDOFF「未結事項」的 6 項補進 INVENTORY（N-worker-probe-stale、N-compare-reports-untracked、N-host-lock-keys-uncontracted、N-client-other-version-untested、N-sleep-reconnect、N-drop-ledger-growth）。G2 還沒決定 | 使用者：「先評估這份清單，然後優先處理系統和引擎的，其次是觀測工具，再來是測試、最後發行。用這個優先度來調整。」「C12 併進 N-token-test-gaps；F4 取消，記成沒有檢測手段。照這樣出第 2 版、同步 INVENTORY／README／HANDOFF，然後拆批給我看。」 |
| D159 | （2026-10-21）核准 `after-re-evaluate` 的批次草案（[after-re-evaluate.md](after-re-evaluate.md) 第 9 節）：決策點 A 選 (a)，substitution-single-store 在批內先寫 slack 樣本的特性測試；B：sdl-isolation-plan 用 ultracode；C：worker-probe-repair 提到最前面；PR 分三條（sdl-isolation 兩批、系統其餘＋觀測＋測試、發行）；G2 先不做。從 worker-probe-repair 開始 | 使用者：「核准批次草案：A 選 (a) 批內特性測試；B 用 ultracode；C 把 worker-probe-repair 提到最前面；PR 分三條。G2 先不做。從 worker-probe-repair 開始。」 |
| D160 | （2026-10-21）核准 `sdl-isolation` 的計畫：Engine 側 IP-3／IP-4（IP 的 D31，U1～U10 照建議，含接受約 56～73 h 的估時、各子批開始前重估、超過 1.5 倍就停）；產品側 `sdl-isolation.md` 的 S1 選 (b)（保留 `Platform()`，只移除 `SdlGpuRenderDevice&` 的公開）、S2 選「base 與頭各用自己的 probe」。設計寫成架構文件 `docs/architecture/platform-sdl.md`、`input.md`（使用者要求） | 使用者：「OK，照計劃執行，記得把這個設計落到架構文件中去。」 |
| D161 | （2026-10-21）核准 ip3a 的事前宣告（IP 的 D32）；`sdl-isolation.md` 的 S3 選 (a)：受擊回饋的 A 級日誌行改走 `Base::Log`，日誌檔的位元組不變，釘住測試的讀取位置跟著改。開始 ip3b | 使用者：「核准 ip3a 的事前宣告；S3 選 (a)。開始 ip3b。」 |
| D162 | （2026-10-21～22）`sdl-isolation` 的 ip3b、ip3c 期間的決定：巢狀 `PumpEvents` 保留（IP 的 D33）；`SampleClockPair` 加讀取注入的接縫、契約條文 (2) 修正（D34）；核准 ip3c 的判定變更（D35，§14.4，獨立核對 3 輪）。另：使用者要求開 PR，sdl-isolation 線開 draft PR #85（標題照 v7.1 的格式） | 使用者：「選 (a)：保留巢狀 PumpEvents…」「要不然你開一個PR吧。工作不少。」「版本還是用v7.1的」「選 (b)：SampleClockPair 加讀取注入的接縫…」「核准 ip3c 的判定變更（第 3 版）。」 |
| D163 | （2026-10-22）ip3d 完成後繼續 ip3e，檔位照建議（high，日誌橋局部 xhigh）。使用者睡前指示「可以的話把 IP-3、IP-4 跑完」，之後指示「跑完手頭上的任務就先暫停」（Mac 需要休息）。ip3e 完成後暫停 | 使用者：「檔位照建議」「我去睡覺了。可以的話吧IP-3、IP-4跑完。」「你跑完手頭上的任務就先暫停一下吧。我的mac幾天沒休息了。」 |
| D164 | （2026-10-22）ip3e 的 P1～P3（IP 的 D36）：P1、P2 合成一條，`docs/architecture/error-handling.md` 加「平台邊界的例外」條款並註明理由（照字面遵守會讓程式錯誤不可見，無法測試）；日誌橋的解構子回 noexcept、改 death test；`RunEntry` 不改；P3 改逐執行緒守恆。之後做 ip3f，L3 排最後 | 使用者（分支會話）：「其實P1和P2是一樣的問題呀。」「嗯，可以，但是需要註明為何會加這個條款。」；本會話：「處理完P1-P3後，處理ip3f，L3排最後。」 |
| D165 | （2026-10-22）ip3f 的 L3 通過（附兩項觀察：輸入「:」時卡住，推論是輸入法組字；手感的停頓是 IP-2 已知的 drawable 取得停頓）。v8 加一個候選：大廳的按鈕與位址輸入框元件化，並修改 ui_editor | 使用者：「OK了，第一項輸入的時候。在輸入：的時候忽然卡住，我不知道是不是輸入法的問題。其他功能是正常的，但是感覺操作手感怪怪的。v8排一個任務，組件化這些按鈕和輸入框，順便修改uieditor。」 |
| D166 | （2026-10-22）IP-4 介面審查（U10）後的修正（IP 的 D37）：Design Invariant 7 改寫（查詢類在 observer 與 live frame 內允許、操作類斷言、`EventWatch` 不參照 platform、分派深度的斷言）；回傳型別修正；刪除 ip4b 的「AddEventWatch 失敗路徑」驗收（§14.4）。之後宣告並做 ip4a | 使用者：「核准：DI 7 照建議改寫…；M2 照建議改回傳型別、刪除 AddEventWatch 失敗路徑的驗收。其餘意見照你的處理，宣告後做 ip4a。」 |
| D167 | （2026-10-22）ip4b 的 xhigh 審查之後（IP 的 D38）：時間戳判定採方案 B（區間＋兩條精確等式；§14.4，獨立核對後）；Design Invariant 7 加「`EventWatch` 只在主執行緒解構」（GYO_ASSERT＋death test）；Design Invariant 3 的守衛清單補上 watch 路徑。之後提交、推上 PR #85 | 使用者：「核准：F4 採方案 B（區間＋兩條精確等式）；F10 加 DI：EventWatch 只在主執行緒解構（GYO_ASSERT＋death test）；DI 3 守衛清單補上 watch 路徑。然後提交、推上 PR #85。」 |
| D168 | （2026-10-23）產品側 IP-4 的分批與事前宣告（`sdl-isolation-ip4-declaration.md` 第 2 版）核准，P1～P5 照建議：P1 節奏改用 Waiter、P2 產品加中立的 `RenderDeviceInfo()` 存取點、P3 pointer capture 改 golden 並由 Engine 補 s06（J1 的 §14.4 核准之後另交）、P4 gui-probe 放進 L2、P5 L2 第 2 版。判定原則：基準改變的量只記錄、不判定；和基準無關的量（產品行為、各樹各自的絕對門檻）照判 | 使用者：「基準變了的量，只記錄不判定；跟基準無關的量（產品行為、各自的絕對門檻）照樣判定。照建議執行。」 |
| D169 | （2026-10-23）sdl-isolation J1（pointer capture 的凍結 oracle 改 golden）的 §14.4 判定變更核准，採全欄位版 golden；v-s08～s10 選 (a)：Engine 在 `input.md` 的 Contracts 寫明本視窗的焦點喪失、按鍵、滑鼠鍵事件不去重，補 3 個測試與突變。之後提交、推上 PR #85、觸發 L1，接著準備 L2 第 3 版 | 使用者：「核准 J1 的判定變更（golden 全欄位版）；v-s08～s10 選 (a)：Engine 契約寫明不去重，補 3 個測試與突變。然後提交、推上 PR #85、觸發 L1，接著準備 L2 的第 3 版。」 |
| D170 | （2026-10-23）sdl-isolation 的 L2 第 3 版核准。語料重判選 (a)：改成 11 個 pv7 跑次，兩層（分類、重新分析）都要相同，6 個 pv6 跑次排除。capture BMP 選 (a)：前置改成頭的 `render.sdl_gpu_capture_file` 通過，拿掉 `--capture-bmp`。改好後列出開跑前的準備 | 使用者：「核准 L2 第 3 版：語料重判選 (a)，改成 11 個 pv7 跑次、兩層都要相同；capture BMP 選 (a)，改用頭的 render.sdl_gpu_capture_file 通過，拿掉 --capture-bmp。改好後告訴我開跑 L2 前要準備什麼。」 |
| D171 | （2026-10-23）`substitution-single-store`（C13）選 (b)：以 SlackTrack（行為紀錄）為唯一來源，帳本改讀它。保留範圍沿用帳本的 600 個序號，所以遲到 1～10 秒的命令也會產生 slack 樣本（行為改變）。斷線恢復時 Client 會收到很大的負 slack，這個問題用斷線重連（重新對時）處理；斷線偵測與重連只走 UDP（旁支會話轉達，記憶 udp-only-liveness）。新開 PR 線，規劃用 ultracode | 使用者：「選（b），合併後：遲到 1～10 秒的命令也會產生 slack 樣本，斷線恢復時，Client 會收到很大的負值的這個問題，用斷線重連的方式（重新對時？）來做。新開PR，用Ultracode來跑這個case的規劃」 |
| D172 | （2026-10-23）`outage-resync` 計畫第 2 版的 D-1～D-14 全部照建議：完整範圍（約 59～85 h）；D-2 選 I-2（session 在靜默 ≥T_int 後可改綁 endpoint，wire 不變，I-3 記到 v8 候選）；D-3 選 M-B′（晚到 >N_o 換 epoch、Resync 不受冷卻限制）；D-4 T_int 與 N_o 在 G1 依特性報告決定；D-5 T_max 從最後一份 snapshot 起算；D-6 C13 (b) 的兩面一起核准（§14.4 的材料在 retention-600 交）；D-7 照列表；D-8 (b) `reason=resume`；D-9 (a) 中斷期間照常送；D-10 品質照算；D-11 T_max 維持 5 s、wake 選 (w1)；D-12 照 K5；D-13 的判定、門檻、次數在 L2 前另行宣告核准；D-14 同意 Delta-1～3、新 Invariant 與 Replacement Criteria、`ConnectionLiveness`、wire 留 pv7、after-re-evaluate 決策點 A 選 (a) | 使用者：「OK，照建議」 |
| D173 | （2026-10-23）`outage-resync` 關卡 `gate-characterize`（舊稱 G1）：T_int＝0.5 s（clean 語料 snapshot 最大間隔 0.119 s）；N_o＝20 Tick（語料晚到最大 13.3 Tick，可抓到延遲 0.5 s 的部分送達）；D-15（只有上行中斷後的 12 命令領先量要 0.5～1 s 才由 Backlog 清掉）選 (a) 只記錄、交給 v8 B6，契約寫成 P2 的已知例外。特性報告：本線分支的 `outage-characterize-report.md` | 使用者：「G1 照建議：T_int 0.5 s、N_o 20 Tick、D-15 選 (a) 只記錄、交給 B6。L1 確認後開始 resync-contract。」 |
| D174 | （2026-10-23）`outage-resync` 關卡 `gate-contract` 核准：`resync-contract` 的條文照現狀（network-architecture §4「連線中斷與恢復」、§7 兩節 Design Invariants、protocol-v7 §2、log-contract §1.4、ingress README；含 xhigh 審查 3 輪的修訂：恢復時權威 < tip 只重設相位擷取、N_o 以時間判斷與 `late_beyond_no`、最後驗證時刻、P2 的第二個例外等）。開始 `drop-counters` | 使用者：「gate-contract 核准目前的條文（含三輪審查的修訂）。開始 drop-counters。」 |
| D175 | （2026-10-23）`outage-resync` 的 `single-store-refactor` 之後：network-architecture §7「被替代序號的單一紀錄」的權威名稱照實作由 SlackTrack 改為 `MatchSubstitutionRecords`（MatchRuntimeHost 的 `substitutions_`），帳本附欄的列舉照實作補齊；意圖不變。`match-resync` 在 L1 通過後以 xhigh 開始 | 使用者：「核准 §7 owner 改名為 MatchSubstitutionRecords。L1 通過後用 xhigh 開始 match-resync。」 |
| D176 | （2026-10-24）`outage-resync` 的 `retention-600`：核准判定變更 J-A（稀疏替代的守衛：600 序號以外的副本不再 Resync）、J-B（紀錄的單元測試換成新規則）、J-C（`v7-flow-ledger-drives-sample` 的改寫），以及 L2 不退步紀錄的宣告草案（x1、ramp、gateway-250ms 各 3 次，只跑頭；S1、S2、J5a、S3 與 runtimeFailed 0，0 容許，不重試）。先觸發 L1，再準備 L2 | 使用者：「核准 retention-600 的判定變更 J-A～J-C 與 L2 不退步紀錄的宣告草案。先觸發 L1，再準備 L2。」 |
| D177 | （2026-10-24）`outage-resync` 的 `retention-600`：確認 L2 宣告第 2 版（D45 檢查後的修訂）：gateway-250ms 的 runner 更正為 `backpressure_probe.py --layers gateway --durations-ms 250`；有效性前提不成立記為 E（停下回報、不自動補跑）；J5a 對 x1／ramp 帶 `--injection`；runtime 要求 end 行 reason=signal、exit_code=0；resyncs／late_beyond_no／resync 重設記為 FLAG 停下回報；S3 的逐玩家完整性與 player 0。開跑，檔位照建議（medium） | 使用者：「照第 2 版宣告開跑，檔位照建議」 |
| D178 | （2026-10-24）`outage-resync` 的 `resync-l2-l3`：核准 L2 宣告第 2 版（D45 小規模 ultracode 檢查後的改寫：11 項 × 3 輪、J-R1～J-R9、J-R3 只記錄、驅動與判定工具與凍結清單的 sha256）。結果：33 輪全部 P | 使用者：「照建議執行」 |
| D179 | （2026-10-24）`outage-resync` 的 L3（Windows 與 macOS 拔網路線或關 Wi-Fi，2 s 與 7 s 各一次；`transient_socket_errors` 與 endpoint 的紀錄）移到 after-re-evaluate 的 `lan15-windows`，一起做。outage-resync 不等 L3，L1 確認後合併。Windows 的 WSAECONNRESET（D172 D-7 的推論）在那之前未經實機確認，列為已知項目 | 使用者：「L3需要放到 lan15-windows一起吧。」；說明暫時性 socket 錯誤的風險後：「確實可以放lan15-windows做。時間大概在26日。…在v7.1完結後需要發一個release」 |
| D180 | （2026-10-24）`silent-branch-asserts` 的列點（[silent-branch-asserts.md](silent-branch-asserts.md)）：Go 端的 assert 用 (g2)「讓 runtime link 失敗」；A1～A12、C1、C2 照表；L1～L7 保留並修正 INVENTORY；C3 也做（Client 丟掉的封包計數，追加在 worker statistics 行尾）；U1～U3 與第 7 節「已計數、註解寫應為 0」的分支這一批不動。使用者補充：`lan15-windows` 之前要把已知問題全部處理掉，包括這些分支與 outage-resync 只記錄的問題（清單在本線整理後交使用者排序） | 使用者：「1、2 照建議。3、還是做吧，處理封包還是不留尾巴比較好。（那個問題列表，也是要v7.1的 lan15-windows前做的，只不過現在先不管）」 |
| D181 | （2026-10-24）`lan15-windows` 之前的已知問題（K1～K10，清單見 [silent-branch-asserts.md](silent-branch-asserts.md) 第 8 節）照建議排：K1～K4 新批 `client-silent-checks`，接在 `silent-branch-asserts` 之後；K5～K6 新批 `rebind-on-new-port`（先開關卡）；K7 併入 `contract-readers`，K8 併入 `trace-no-overwrite`，K9 併入 `client-path-tests`；K10 在 `lan15-windows` 的工具準備修；`lan15-windows` 延到這些做完之後（不再是 2026-10-26 前後） | 使用者：「照建議排：K1～K4 新批 client-silent-checks 接在本批後，K5～K6 新批 rebind-on-new-port（先開關卡），K7 併 contract-readers，K8 併 trace-no-overwrite，K9 併 client-path-tests，K10 放 lan15 工具準備；lan15 延到這些做完之後。」 |
| D182 | （2026-10-24）取消 `wake-reconnect`（N-sleep-reconnect）：睡眠超過 T_max 後不自動重新加入，照現行回 Lobby。理由：沒有需求，玩家讓電腦睡眠就是要離開；留在場上只會變成靶子。短於 T_max 的睡眠仍照 outage-resync 當中斷處理（D172 D-11 (w1)）。玩家離開後到 Gateway 判逾時（約 5 s）之間角色仍在場上，也不處理：不是正常退出時，本來就無法判定玩家的行為 | 使用者：「wake-reconnect 這個可以取消了，沒有需求，玩家睡眠就是要離開，但是離開了，人物在遊戲裡就是個靶子。」補充：「5s判超時不用處理，玩家本來就要離開了，如果不是退出。你無論如何都是沒有辦法判定玩家行為的。」 |
| D183 | （2026-10-24）`client-silent-checks` 的守衛追蹤（[client-silent-checks.md](client-silent-checks.md)）全部照建議：K1（時間線 `Push`）assert；K2 選 (b)，`Reconcile` 回傳 `bool`、產品呼叫端 assert；K3（`SendInput`）的 r2、r3、r4、r7 assert，r1、r5 在 worker statistics 行尾計數，r6 保留；K4 的 S1、S2（Match 端）assert，S3、S4（Gateway）用 `invariantFailed` | 使用者：「K1～K4 照建議：K1 assert；K2 選 (b) Reconcile 回傳 bool、產品呼叫端 assert；K3 的 r2、r3、r4、r7 assert，r1、r5 計數，r6 保留；K4 的 S1、S2 assert，S3、S4 用 invariantFailed。」 |
| D184 | （2026-10-24）突變的重跑範圍：每批只跑 (1) 新登記的突變、(2) find 落在本批改動範圍內的突變、(3) 負責殺死它的測試檔（以突變的 check 對應的測試來源判斷）被本批改動過、且所在檔案也被改動的突變；每條 PR 線合併前，在背景把改動檔案上的既有突變完整跑一次。取代 after-re-evaluate 決策點 C 引用的「目標檔案被改過的既有突變都要重跑」。原因：目錄已有 613 個，集中在幾個大檔，舊規則每批多 1～2 h；find 過時已由目錄測試檢查 | 使用者：「照建議改：每批只跑新突變、find 落在本批改動範圍的、以及殺死它的測試檔被改動的；每條 PR 線合併前在背景把改動檔案上的既有突變完整跑一次。」 |
| D185 | （2026-10-24）發版時在本機背景跑完整突變目錄，發行線一開始就啟動。非 killed 的逐個分類：測試失去檢出力（擋發版，發版前修好）、突變過時或等價（更新或移出目錄並寫明理由）；只有前者擋發版。結果寫進發行線的 dev_log。v7.1 的 release 是第一次。目的：(1) 補上 D184 只看改動檔案的缺口（某條線改了共用測試，削弱了未改動產品檔上的突變）；(2) 找出目錄測試抓不到、只有真的跑才看得到的「意義過時」突變並清理：期望訊息過時（wrong_failure：仍被抓到，但改由別的測試或路徑抓到、訊息變了 → 更新 `expect`）、等價突變（survived：周圍程式改了、突變不再改變行為 → 移出目錄並寫明理由）、檢查本身變了（殺死它的測試被改名、刪除或合併 → 改指新的檢查）。文字過時（find 對不上）已由 CTest 的目錄測試每次檢查，不靠這一次 | 使用者：「把剛才的建議，記成決定：發版時在本機背景跑完整突變目錄，發行線一開始就啟動；非 killed 逐個分類，只有「測試失去檢出力」擋發版；結果寫進發行線 dev_log。v7.1 的 release 是第一次。」目的 (2) 是使用者補充：「這個目的也加上」 |
| D186 | （2026-10-25）K11 列入 `lan15-windows` 之前的已知問題：Engine 的 `platform_sdl_concurrency`（`EventWatchTests.cpp:300`，200 輪中至少一次在回呼執行時解構）靠時序碰運氣，client-silent-checks 的 L1 在 macos-arm64 失敗一次。新批 `event-watch-overlap`：改成用同步保證重疊，排在 `client-path-tests` 之後 | 使用者：「列進 lan15 前的已知問題（K11）：把 EventWatchTests 的重疊檢查改成用同步保證重疊、不靠碰運氣，放在 client-path-tests 之後做。先繼續 cxx-flag-removal。」 |
| D187 | （2026-10-25）`rebind-on-new-port`（[rebind-on-new-port.md](rebind-on-new-port.md)）採 r1＋k3：Client 在 Welcome 之前、中斷期間、最近 250 ms 沒有送出 Input 或 Actions、或輸入已送出但確認滿 T_int 沒有前進時，Hello 每 250 ms，其他時候每 1 s。Gateway、wire、通用層 session、Design Invariants 不變；network-architecture §4 的 Hello 間隔與安全取捨條文跟著改；實作後做 1 次 xhigh 審查（使用者同意）；L2 宣告寫好後再交使用者核准 | 使用者：「照建議 r1＋k3：Client 在 Welcome 前、中斷期間、最近 250 ms 沒送 Input 或 Actions、或輸入確認滿 T_int 沒前進時，Hello 每 250 ms；契約條文跟著改；實作後做 1 次 xhigh 審查；L2 宣告之後再交我核准。」 |
| D188 | （2026-10-25）`rebind-on-new-port` 的 xhigh 審查推翻 D187 條件 (b) 的前提（Match 收不到命令時用替代命令讓確認照樣前進）後，改選 P3：拿掉 (a)、(b)，Client 的 Hello 一律每 250 ms。取代 D187 的規則；契約條文與測試跟著改；舊證據的 Hello／Welcome 計數比較註明換了規則 | 使用者：「選 P3：拿掉 (a)、(b)，Hello 一律每 250 ms；契約條文與測試跟著改，舊證據的計數比較註明換了規則。」 |
| D189 | （2026-10-25）核准 Design Invariants 守衛測試的判定變更：`test_diagnostic_isolation.py` 的 liveness 呼叫次數由 12 改成 11（D188 拿掉 Hello 間隔裡的 `Interrupted()` 讀取）。§14.4 的 1～5、突變檢查與獨立核對見 dev_log 的 rebind-on-new-port 一節。核對時發現的既有缺口（解析器看不到的呼叫）列為 K12 | 使用者：「核准把 test_diagnostic_isolation.py 的 liveness 呼叫次數從 12 改成 11。解析器看不到的呼叫那個既有缺口，列進 lan15 前的已知問題。」 |
| D190 | （2026-10-25）核准 `rebind-on-new-port` 的 L2 宣告第 2 版（[rebind-on-new-port-l2-declaration.md](rebind-on-new-port-l2-declaration.md)；D45 檢查後改寫；含受入判定的變更 N9，§14.4 的 1～5 在宣告第 3 節），立即跑 | 使用者：「核准 rebind-on-new-port 的 L2 宣告第 2 版（含 N9 的判定變更），立即跑。」 |
| D191 | （2026-10-25）`trace-no-overwrite` 採 T2：trace 檔已存在時，自動換下一個沒用過的名字（`match-commands.jsonl` → `match.2-commands.jsonl`），跟著它決定檔名的 `-ingress.jsonl`、`-shots.jsonl` 一起換；啟動時把實際檔名寫進 log；Client 同樣適用。讀者不改（一檔一段不變）。K8（tip 恢復時補寫 `phase_path=resume_reset`）照做 | 使用者：「用 T2：檔案已存在時自動換下一個沒用過的名字（match.2-commands.jsonl），其他兩個檔跟著換，啟動時把實際檔名寫進 log；Client 同樣適用。K8 照做。」 |
| D192 | （2026-10-25）`analyzer-converge`（C15）採 a：`command_evidence.analyze_commands` 改成支援 N 人，刪掉 `quad_evidence.command_metrics`，`analyze_quad` 直接呼叫它；2 人時輸出逐位元不變；用 pv6／pv7 語料新舊各分析一次，結果相同。代價：凍結的 `command_evidence.py` 改變，現有凍結清單失效，lan15-windows 的 L2 要重新凍結。**補充的驗收條件**（使用者在分叉的會話裡說，經規劃會話轉達）：收斂後的 `analyze_commands` 不寫死人數（`players=None` 依跑次宣告的 `player_ids` 決定），只有 2 人分析器的呼叫端以 `players=2` 要求剛好 2 人；不得退化回寫死 2 人。「支援 N 人」只指分析器，協議上限仍是 `MaxPlayers`＝4（`Movement.hpp:21`，Client 丟棄超過 4 人的 snapshot）。由測試釘住（4 人合成跑次經 `players=None` 通過）。規劃會話之後更正：測試用 4 人即可 | 使用者：「用 a：analyze_commands 改成支援 N 人，刪掉 command_metrics，analyze_quad 直接呼叫它；2 人時輸出逐位元不變；用 pv6／pv7 語料新舊各分析一次，結果相同。」 |
| D193 | （2026-10-25）`windows-long-path` 採 w2（[windows-long-path.md](windows-long-path.md) 的「決定」一節）：路徑問題只在 `engine/asset` 的載入模組內處理，不動 Engine 其他模組、ID 存取與產品（不另做 `IAssetSource`）。只改 `NativeFileAssetSource` 向 OS 開檔的那一處；只在 Windows 且路徑過長時轉成正規化的絕對路徑、加 `\\?\` 前綴、以寬字元開檔；resolver、錯誤訊息、manifest 的路徑維持一般形式；開工前確認所有載入器都經過 `IAssetSource`。實作等使用者說開始 | 使用者在分叉的會話裡確認，經規劃會話轉達 |
| D194 | （2026-10-25）使用者希望不只修檔案路徑，而是把所有系統差異（檔案、底層互動等）收攏到 Engine 的統一子系統。原本交給架構會話；使用者之後決定另開任務：架構會話開了任務卡「規劃 Engine 的 OS 統一子系統」，使用者啟動後是新的會話。那個任務先做 os-longpath-decision（v7.1 的 windows-long-path 只修一處，還是做 Files 第一片），結論經使用者決定後通知本會話；完整 PLAN、分期與 Architecture Delta 另外做。D193 的實作暫停 | 使用者：「這樣，你這個任務交給架構掃描會話，讓她開一個任務開來做。」 |
| D195 | （2026-10-25）`windows-long-path` 採 L4，取代 D193 的範圍：為 `engine/io` 的 `IFileSystem` 寫只讀的原生實作（Windows 長路徑在它內部處理），asset 的讀檔（`NativeFileAssetSource`、`ContentManifest`、`AssetCatalog`）改走它；v7.1 不開 `engine/os`。子決策（放置、路徑編碼、`file://` Uri 契約）在 OS 子系統的會話確認中；估時 6～9 h。細節與證據見 [windows-long-path.md](windows-long-path.md) 的「改為 L4」一節 | 使用者在「規劃 Engine 的 OS 統一子系統」會話：「v7.1 的 windows-long-path 採 L4：為 engine/io 的 IFileSystem 寫只讀的原生實作，asset 的讀檔改走它。」（經該會話轉達） |
| D196 | （2026-10-25）L4 的子決策定案：先開 `engine/os`，`IFileSystem` 的原生實作放在那裡（`engine/io` 只留中立抽象），路徑字串改成 UTF-8，現在就修 `ParseUri` 的 `file://` 規則。這一片由 OS 子系統任務在 PLAN 經使用者核准後實作，就是 windows-long-path 的修正；v7.1 不再自己實作，`lan15-windows` 等它完成。產品 `PvpApplication.cpp` 會和 PR #95 碰到同一個檔案，合併順序之後協調。細節見 [windows-long-path.md](windows-long-path.md) 的 D196 一節 | 使用者在 OS 子系統的會話：「你要先開engine/os，然後把io下嗎相關系統的實作移動過去。」「現在改成 UTF-8」「現在就修 ParseUri」「你的目標不是修路徑，你的目標是整理OS的子系統。」（經該會話轉達） |
| D197 | （2026-10-25）分工更正：OS 子系統任務只做架構（`engine/os` 與從 `engine/io` 移過去的系統實作），windows-long-path 的修正與驗收由 v7.1（本會話）在架構完成後實作。產品任務和架構分開。界線和那個會話確認後記在 [windows-long-path.md](windows-long-path.md) 的 D197 一節 | 使用者：「你來做！！！，那邊只修架構，你等那邊修好了，就做。產品任務和架構分開。」 |
| D198 | （2026-10-25）OS 子系統的 PLAN 核准（Engine D46）。對 v7.1：A1～A3（`engine/os` 骨架、`engine/io` 契約、唯讀檔案）在發行前、由 OS 子系統任務在它的分支完成，併進發行線 PR，加上本會話的 P1～P5（兩個任務、一個 PR）；UNC 根目錄這一輪不支援；P4 多兩處（`ContentManifest.cpp:110`、`ReadOnlyAssetCatalog.cpp:55`）；P5 的驗收條件見 [windows-long-path.md](windows-long-path.md) 的 D198 一節；其他 OS 部件排在發行後 | 使用者在 OS 子系統的會話：「核准 os-subsystem 的 PLAN。開放決策 1、14、15 照建議……同意 A1～A3 的 Delta（新 target GYO::Os、os.md 與 io.md 的 Design Invariants 節）。」（經該會話轉達） |
| D199 | （2026-10-25）windows-long-path 的 P1～P5 開工。發行線 `claude/pvp-v71-release` 不等 PR #95 合併，從 PR #95 目前的最新提交 `a0e6668` 切出，再 `--no-ff` 併進 A1～A3（`f3a6770`，無衝突）；PR #95 合併後，發行線相對 master 的差異同樣只剩 A1～A3 與 P1～P5。P1 的行為變更（Windows 上完整路徑達 248 字元以上時交給 API 的是 `\\?\` 加完整路徑）與 P5 的驗收在執行前宣告，見 [windows-long-path.md](windows-long-path.md) 的 D199 一節 | 使用者經 OS 子系統的會話轉達：「讓v7.1會話開始P1～P5的任務」；分支的先後由本會話判斷（該會話：「先後怎麼排……由你判斷，必要時再問使用者」） |
| D200 | （2026-10-25）K9 照建議 (b)：握手期暫時性錯誤與直接超過 T_max 的兩個判斷抽成純函式（`SocketErrorActionFor`、`PollRecordsInterruption`）做單元測試，接線由 `test_diagnostic_isolation.py` 的靜態檢查守，產品不加測試入口；執行期由 lan15-windows 的 L3 補。K11 的判定變更核准，patch 套進 `EventWatchTests.cpp`。細節見 dev_log 的 client-path-tests 與 event-watch-overlap 兩節 | 使用者：「K9、K11照建議開始」 |
| D201 | （2026-10-25）windows-long-path 的實機驗收（P5-6）併進 lan15-windows。2026-10-26 在 Windows 機器上依序做：Windows 專用的 5 個突變（`p1-no-long-path-form`、`p1-long-path-from-too-high`、`p4-manifest-ansi-path`、`os-f4-windows-acp`、`os-f5-windows-notfound`）→ 實機在 P＝156 與 P 約 210 各啟動 Client 一次 → lan15 | 使用者：「這個能合到 lan15去吧，明天我先跑 Windows 專用的5個突變，在跑驗收，最後 lan15.」 |
| D202 | （2026-10-25）quick-close 採 q1：Client 關閉時的 `/leave` 改用 300 ms 的逾時（總上限 600 ms），其他請求維持 2 秒。驗收門檻在執行前核准：從關閉要求到程序結束，Gateway 可連 ≤500 ms、Gateway 關掉 ≤1 s，macOS 與 Windows 各 10 次全部符合、不重試、跑完不放寬 | 使用者：「q1，門檻核准」 |
| D203 | （2026-10-25）quick-close 的驗收判定加「Gateway 收下 `/leave` 卻不回（stalled）≤1 s」，macOS 與 Windows 各 10 次；另加 CTest 回歸測試 `object_fps_pvp.close_timing`（每種情境 1 次、同一組門檻），讓突變抓得到。這條判定是在看過 Red（修正前 2004 ms）與第一次 macOS 驗收（約 304 ms）之後加的，只收緊 | 使用者：「加：stalled ≤1 s 列入驗收判定（macOS、Windows 各 10 次），並加一個 ctest 回歸測試讓突變抓得到。」 |
| D204 | （2026-10-25）`object_fps_pvp.close_timing` 維持 `network` 標籤（和 `object_fps_pvp.worker` 一樣用本機 socket），CI 的 L1 不跑它；quick-close 在 Windows 上的證據靠 D201 的手動 10 次 | 使用者：「維持 network 標籤，Windows 證據靠晚上手動的 10 次。」 |
| D205 | （2026-10-25）OS 統一子系統其餘的批次改成現在就開始，不等 v7.1 發行（取代 D46 的「發行後」）。協調：那個會話的分支從發行線 `claude/pvp-v71-release` 切出（已含 A1～A3、P1～P5、quick-close），PR #96 合併前要改發行線也改到的檔案時先通知本會話；lan15 若需要修正，由本會話在發行線做，再由那邊併入 | 使用者：「我覺得，現在開始  OS 統一子系統的工作比較好。」 |
| D206 | （2026-10-26）lan15 的 Windows 機器上加做 OS 子系統的主控台實機驗證（Engine D51，PLAN 第 7.7 節的 L3，只記錄）：Match 在主控台裡用 Ctrl+C、Ctrl+Break、關閉主控台視窗三種方式結束，log 最後一行都是 `end version=1 reason=signal exit_code=0`；結束要求後再按一次 Ctrl+C 會直接結束、沒有 end 紀錄（補充）；可以的話跑 `ctest -R "^os\.console_close$"`（Windows Terminal 下可能 Skipped，改用 conhost）與 os-subsystem 目錄的 Windows 專用突變（os-c4、os-c5、os-c6、os-c8、os-c9、os-g18、os-f4、os-f5、os-f9～os-f11、os-g16、os-p5、os-x7～os-x11（os-tool-files，Engine D56／D57）、pu-x3～pu-x7（os-path-utf8-sweep，D63）；OS 子系統的完整清單（34 條）以它交給使用者的為準。checkout 必須是 LF（`git config core.autocrlf false` 後再 checkout），否則跨行的 find 會 stale 與 v7.1 的 p1 兩個、p4）；io-native-paths（Engine D54／D55）的探針 `io.unc_localhost`、`io.unc_127.0.0.1` 預期 Passed（連不到 `\\localhost\C$` 時 Skipped）。D207 起都在發行線上；結果回給 OS 子系統的會話 | OS 子系統的會話轉達使用者的決定 D51 |
| D207 | （2026-10-27）OS 子系統的 7 批（os-guard-wide、os-process-info、os-console-shutdown、os-files-write、io-native-paths、os-tool-files、os-trace-writers；Engine D48～D60）不另開 PR，併進發行線 PR #96：`claude/trusting-joliot-f13b78` 以 `--no-ff` 併入（`32ce9b6d` 到 `ca434952`、`f87b77d0` 到 `d32def55`、`2aebc143` 到 `e5fec6ef`），本機 `ctest` 97／97；每批的四平台 L1：run 40194493434、40205158822、40213916104、40295007179、40324361964、40339587478。lan15 用合併後的發行線跑；D206 的 OS 驗證也改在發行線上做 | OS 子系統的會話轉達：「OS 子系統的 commit 不另開 PR，統一放進 #96」；使用者在本會話：「推送，今晚 lan15 用合併後的發行線跑。」 |
| D208 | （2026-10-27）lan15 延期（時間對不上），新日期未定；PR #96 的合併仍等 lan15 | 使用者：「lan15 延期，時間對不上。」（分兩則送出） |
| D209 | （2026-10-27）Windows 機器的清單（第 1～5 項）與 lan15，等 OS 統一子系統做到 os-net-errors 之前的批次全部完成後一起做。使用者在 OS 子系統的會話確認（Engine D62）：「之前」含計時核心，剩 4 批：os-path-utf8-sweep、os-wake-record、os-threads-backend、os-wait-backend（time-threads-invariants 已完成）。那次 Windows 機器上另加計時核心的 L2 與 os-wake-record 的 Windows 基準。排在 PR #96 合併後的只有 os-net-errors 與 os-close | 使用者：「我想乾脆吧「OS 統一子系統」的任務跑完一起弄。」之後：「那就到 os-net-errors 這個任務之前。」範圍經 OS 子系統的會話向使用者確認（Engine D62） |
| D210 | （2026-10-27）OS 子系統的 os-path-utf8-sweep（Engine D63）與 time-threads-invariants 併進發行線：`16377127`（`--no-ff` 到 `69e39a26`），本機 `ctest` 97／97；Red run 40431320545（只有 windows-x64 的新案例失敗）、Green run 40433158383（`bfea4f6b`，四平台成功）。產品端：Client 的 `--movement-trace`、`--log` 改走 `Os::Files::PathFromUtf8`（不是 UTF-8 的參數以結束碼 2 拒絕）；產品突變 4 條重新定位 | 使用者在 OS 子系統的會話：「推送 69e39a26，並告訴消費端可以把本分支併到 69e39a26（os-path-utf8-sweep 與 time-threads-invariants）。」（經該會話轉達） |
| D211 | （2026-10-27）先在 Windows 機器上跑 Windows 專用的測試（發行線 `3b612cf2`，LF checkout），完成之後才讓 OS 子系統開始剩下的 os-wake-record、os-threads-backend、os-wait-backend；期間發行線不再併入新的批次 | 使用者：「先等等，我讓windows跑完專用的測試，完成之後直接讓 OS子系統開始」 |
| D212 | （2026-10-27）OS 子系統的 os-utf8-codepage（Engine D64～D67：所有 Windows 執行檔以 manifest 固定 ANSI code page 為 UTF-8）併進發行線：`7e92069d`（`--no-ff` 到 `3e9e9cf3`）；Red run 40450140816（只有 windows-x64 的新案例失敗）、Green run 40456874710（四平台成功）。`p4-manifest-ansi-path` 在 manifest 下成為等價突變，照 D185 退場（從 os-subsystem 的目錄刪除；理由寫在 windows-long-path 的 dev_log）。啟動行記錄 `acp=` 屬於日誌契約，這次不加 | 使用者在 OS 子系統的會話：「推送 3e9e9cf3，並告訴消費端可以把本分支併到 3e9e9cf3（os-utf8-codepage）。p4 的處理一併交給它。」（經該會話轉達） |
| D213 | （2026-10-27）Engine 的 `platform_sdl_concurrency` 在 linux-x64 第一次逾時（run 40510515336，OS 子系統的分支，本機重現不了；OS 子系統的會話交給本會話，因為本會話是 K11 的 owner）。使用者選「重新檢視前提」：把候選 A（`PumpEvents` 在佇列被另一個執行緒持續灌滿時，一輪 poll 的 sentinel 放不進去而不會結束）當成 Engine 可能的缺陷，先排一批去重現。之後使用者把它轉給曾修過同組測試的會話「Fix flaky SdlLogBridge concurrency test on Windows」（PR #91），本會話不再處理。拆成三個 CTest 的 patch 沒有提交，留在本會話的 scratchpad 供參考 | 使用者：「重新檢視前提：把候選 A（PumpEvents 在佇列被持續灌滿時不會結束）當成 Engine 可能的缺陷，先排一批去重現它。」之後：「我發現這個問題這個 platform_sdl_concurrency 相關問題在windows下也有過問題。還開過任務卡，你把這個問題轉過去。」 |
| D214 | （2026-10-28）master（`5d688aaf`：PR #98 的規則文件、PR #99 的 `platform_sdl_concurrency` 看門狗）併進發行線：`c9c4b62f`，無衝突，本機 `ctest` 98／98。PR #99 只改 `tests/common/platform_sdl`（不改判定、時限與 Engine 程式），逾時時 stderr 留下卡住的案例、最後標記與輪次；候選 A 在 Linux 容器沒有重現（原因未查明，不是修正）。範圍的限界：容器排程與 CI 不同、容器沒有 evdev／udev 裝置、沒有照 CI 順序跑前面的測試 | 「Fix flaky SdlLogBridge concurrency test on Windows」會話轉達：何時把 master 同步進 #96 由本會話決定（使用者指示） |
| D215 | （2026-10-28）OS 子系統的 D73：Windows 的計時 L2 與 os-wake-record 的基準改在分支 `claude/os-windows-run` 由 Windows 機器上的 Claude 跑（不需要人操作的項目），不再加在 lan15；os-net-errors、os-close 改排在 lan15 之前（取代 D209 中「排在 PR #96 合併後」與「那次 Windows 機器上另加計時 L2 與基準」）。os-net-errors 會動 `ClientConnection.cpp` 的 `TransientSocketError`（K9 的 `SocketErrorActionFor` 呼叫它），開工前那個會話會再通知。lan15 與它的 L3 仍屬本會話 | 使用者在 OS 子系統的會話：「我相識讓windows跑完，不用依賴人的case，這樣就可以把 OS子系統 完結掉。os-net-error 也有可以參考的對象。也可以從windows量測。」（經該會話轉達） |
| D216 | （2026-10-28）`object_fps_pvp.worker` 在 Windows MSVC 上失敗（os-windows-run 第 1 次，「new window waited behind a fully acknowledged window's deadline」），交給本會話。加了觀測（失敗訊息印出 `restart_ms` 與 `completed_window_gap_ms`），Windows 上 10 次的量測宣告是草案，等使用者核准：[worker-windows-timing.md](worker-windows-timing.md)。OS 子系統的 W3 不通過（D74），os-wait-backend 暫不併入，lan15 跟著延後 | 「對接 OS 子系統的 Windows 實機結果」會話轉達：使用者已決定把 worker 交給本會話 |
| D217 | （2026-10-28）核准 worker-windows-timing.md 第 3 步的事前宣告（MSVC，發行線含 `d7e0af31`，串行 10 次，只記錄並分類），交給 OS 子系統 Windows 續跑的 worker-10x（windows-run.md「第 1 次續跑」，W5 之後、W7 之前） | 使用者：「核准 worker-windows-timing.md 的事前宣告，寫進文件，交給 Windows 續跑的 worker-10x。」 |
| D218 | （2026-10-28）OS 子系統依原意「OS 相容層」重新定界（核心是支撐遊戲跑起來，不是 QA 或多系統相容）：「對接 OS 子系統的 Windows 實機結果」已封存、「規劃 Engine 的 OS 統一子系統」停止；新任務卡「盤點 OS 子系統並收口到支撐遊戲運行」盤點後提出收口方案，核准前不動發行線。在那之前發行線不再併入新的 OS 批次，也不為 OS 安排實機量測。D209、D215 中「lan15 等 OS 子系統全部完成」的前提跟著失效，lan15 的排程待定。worker-10x（D217）原本排在 OS 的 Windows 續跑裡，現在沒有執行者 | 使用者的決定，架構健檢會話轉達 |
| D219 | （2026-10-28）OS 子系統的收口提案核准（Engine D78）。對 v7.1：發行線不再併入 OS 批次；os-wake-record、os-threads-backend 在 v7.1 發行後由 OS 分支直接以 PR 併 master；os-wait-backend 凍結並撤出 OS 分支；os-net-errors 不開始、os-close 取消。已在發行線的 OS 10 批全部留下，其中 4 批是沒有功能支撐的行為變更（★：os-console-shutdown、io-native-paths、os-tool-files、os-utf8-codepage），標為「已出貨、未經功能驗收」。D209、D215 的「lan15 等 OS 全部完成」不再成立；D206 的主控台檢查已在 D201 做過。發行線自 `4a4a0ef7` 起沒有四平台 L1，派發 run 40644602172（`d7ecf047`）。產品線索（只供判讀）：W4（Windows 沒有視窗的程序，60 Hz 晚醒 P50：condition_variable 5.28 ms、sleep_until 15.63 ms、Waiter 0.617 ms）給 F5／F7；W6（Windows 上未連線的 UDP socket 被拒後收到 WSAECONNRESET，socket 仍可用）給 K9：握手期這個錯誤會讓加入失敗（`SocketErrorActionFor` 在遊玩前判 Fail），列進 lan15 的 L3 觀察 | 「盤點 OS 子系統並收口到支撐遊戲運行」會話轉達使用者的核准（Engine D78，決定 1～4 都選 (a)） |
| D220 | （2026-10-28）worker-10x（D217 的宣告）由使用者在 Windows 機器上開 Claude Code，照 worker-windows-timing.md 跑；結果寫進該文件的「第 3 步的結果」一節並推送到發行線。lan15 的日期之後再定 | 使用者：「worker-10x 由我在 Windows 機器上開 Claude Code 照 worker-windows-timing.md 跑；lan15 的日期之後再定。」 |

## 規劃紀錄（記錄器）

- 2026-10-15：盤點 workflow（5 位讀者、2 位對抗式核對、1 位完整性評審，約 34 分）。結果整理成 [INVENTORY](INVENTORY.md)，對抗式核對的更正已套用。
- 2026-10-15：第一次分批規劃。中途 Claude Code 程序結束，接續時又碰到用量上限；恢復後，兩份方案草稿把 v7.1 估到 359～398 h（43～65 批）。使用者：「400 小時是一個很恐怖的工作量」，規劃停止，草稿不採用。
  - 原因（主對話的檢討）：把「盤點清單全部結案」當成目標；把「不留坑」解讀成每一項都必做；把 v8 的契約支持做成內容讀取端；又拿 v7 的規模當錨點。
  - 原因（使用者）：沒有檢測數據的死循環。在量不到的問題上直接排修正，每一項都得自帶量測與宣告。
  - 之後改由主對話列出「功能項＋理由＋簡介」，使用者自己排順序（D90）。
- 2026-10-15：照使用者的順序整理計畫（D91）。工時沿用草稿各批的估計：pv7-line 約 91 h、after-pv7 約 52～60 h、after-re-evaluate 約 95～105 h。
- 2026-10-15：`plan-sync-docs`。合併 origin/master（`8f08f2c`，PR #75～77）；只有 v7 HANDOFF 的「現況」衝突，保留 master 的寫法並補上 v7 結束的紀錄。寫本目錄的 README、HANDOFF、INVENTORY；更正 v8 README 中 hitbox 與 FireGate 的過時備註。合併與文件分成兩個 commit 推送。

- 2026-10-15：`pv7-design`（D92）。ultracode：3 個設計方向（最小改動 7 分、語意分離 7.5 分、以可診斷為主 6 分），評審合成；對抗式檢查（必須修正 3、應修正 5）與 xhigh 檢查（必須修正 2、應修正 7）的結果都已套用或轉成 D-1～D-7。產出 [protocol-v7 草案](../../protocol-v7.zh-Hant.md) 與 [pv7-design](pv7-design.md)。約 84 分的 workflow 加上草案撰寫。尚未 commit。

- 2026-10-15～16：Gateway 方向的評估（D93）。分支會話轉來設計事故（Gateway 由無狀態漂移成持有 Match 決策狀態）與你的要求；事故紀錄補上「跨 agent 漂移」與「AGENTS.md 的重要性」。ultracode：3 位評估者（(a) 153 h、(b) 171 h、(c) 114 h）、評審（建議 (a)，7.5 分；(b) 5 分；(c) 5.5 分）、對抗式檢查（必須修正 2：結果 30 Hz 的執行點、順序要守 D90）。主對話預查 v7 L2 語料：有狀態的出口不是 0（拒絕線溢出丟棄 1488、A3 丟棄 42、舊序號 Actions 436；Gateway 擋掉的重複命令約 201 萬）。產出 [gateway-direction](gateway-direction.md)。尚未 commit。

- 2026-10-16：D94（Gateway 回到無狀態邊緣）、D95（其餘決定照建議）。protocol-v7 草案新增 §2a「IPC 送達語意」，丟棄原因縮為 29 個（D94 之前的過渡原因只存在於中間 commit），dwell 改為一對一；README 的 pv7-line 改為 17 批、約 170～179 h；INVENTORY、pv7-design、gateway-direction、設計事故紀錄同步更新；事故行動 4 轉給文件會話。與本紀錄同一個 commit 推送。

- 2026-10-16：§2a「IPC 送達語意」的 xhigh 對抗式檢查（D95，1 位 xhigh 審查者，約 22 分）：沒有推翻 D94；必須修正 2（結果 30 Hz 改為 Gateway 單槽持有加最小 id 優先；中間 commit 的 ACK-only 轉送）、應修正 6、附註 5，都已套用到草案與 pv7-design 的測試清單；工時加 7～12 h。尚未 commit。

- 2026-10-16：`pv7-design` 完成（D96）。protocol-v7 定稿，加 Client 的 GYOP 序號欄位；README 的 pv7-design 標為完成，pv7-line 約 178～193 h。與本紀錄同一個 commit 推送。

- 2026-10-16：`proto-rename-v7` 完成（D97）。`client_v6`／`runtime_v6` 改名為 `client_v7`／`runtime_v7`（proto 檔名、package `object_fps_pvp.{client,runtime}.v7`、Go 套件 `clientv7`／`runtimev7`、CMake target `gyo_<app>_{client,runtime}_v7`、C++ 的 include 與 namespace、`protocol/README.md`），版本值仍是 6。Go 綁定用專案建置中的 protoc 36.2.0 與從本機模組快取離線建置的 protoc-gen-go v1.36.11 重產；先確認重產 v6 與提交版逐位元一致，新舊的差只有 v6→v7 的名稱。驗收（L1，本機）：`go vet`、`go test ./...` 通過；`test` preset 全量 CTest 70／70 通過；權威 digest 同機兩樹比較（base＝v7final `6d3f287`）scale 1、3 各 35 情境 0 差異（`build/target/_build/test/logs/pvp-v71-proto-rename-20261016/`）。四平台 CI 等 PR。Architecture Delta：Build Graph 的 target 名稱（只改名，依賴關係不變）。實際約 1 h（估 3 h）。

- 2026-10-16：開 draft PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78)（pv7-line，含計畫文件，D97）。CI 對 draft PR 跳過 L1，改以手動觸發 Cross-platform CI 跑四平台 L1（`d5104a0`：run 38771828429）。 四平台 L1、Quick acceptance、CI gate 全部通過，`proto-rename-v7` 驗收完成。

- 2026-10-16 02:01：`version-errors` 開始（D98，檔位 high，估 12 h）。

- 2026-10-16：`version-errors` 實作與驗收。
  - 版本值 6→7：Client、Gateway、Match、驗收工具四處一起改，`test_protocol_version.py` 核對四者一致。
  - HTTP：Gateway 的拒絕 body 帶自己的版本，並寫一行 log（期望值、收到值）。Client 依 body 的 `error` 判斷，產生 `protocol_version_mismatch: Client 7, Gateway <n|unknown>`；join 成功回覆的版本不符也用同一個錯誤。
  - IPC：兩端把版本不符、解析失敗、訊息為空、未知欄位（寫出欄位號）分開。Gateway 讀到其他版本的 Ready 時，關閉前送出只含版本的 envelope；Match 對「Connected 之後沒收到任何框架就 eof」寫一行提示。告知只在 Ready 時送：Match 先送 Ready，實際會偵測到不符的只有這裡；之後再送會讓 link 出現第二個寫入者。這比契約原文窄，D99 核准後在契約加上附註。
  - Client：其他版本的 GYOP 照舊靜默丟棄，另外計數，寫進 worker 統計行的 `other_version_datagrams`。
  - 測試：Go 新增 3 個；IPC 新增 2 個（各種協議違反的關閉原因、eof 提示）；Wire 的位元組黃金值改為 `0x0007`，並測 `OtherVersion`；worker 的模擬 Gateway 測 4 種拒絕（上一版、下一版、沒寫版本、`room_full`）。突變新增 7 個（`v71-version-errors`），`09-version-python` 改 find 字串。
  - 建置中途的修正：IpcHostTests 的 `pb` 別名在 doctest 的測試函式（全域範圍）與 protobuf 的 `::pb` 衝突，改用匿名命名空間裡的 `Envelope`。
  - 驗收（L1，本機）：`test` preset 全量 CTest 70／70（含權威 golden 30／30）；突變 7／7 與 `09-version-python` 都 killed；權威 digest 同機兩樹比較（base＝v7final `6d3f287`）scale 1、3 各 35 情境 0 差異。
  - 凍結 pv7 版分析器：`frozen-pv7.sha256`（54 檔，清單的 sha256 `b4a8963001883160d5bb7642b7da89afd343c686d29752874461762e17a291c2`，審查修正後重產）。和 v7final 相比只改了 `acceptance_util.py`（版本常數）與 `run_quad.py`（mixed）。
  - 凍結前的 pv6 語料核對（16-L2 的 43 項工作：遊戲 25、backpressure 6、soak 1、quad 10、GUI 1；每項分析複本，不寫入語料）：
    - 舊分析器 43／43 重現 session 存下的結果（確認輸入重建正確）。
    - 新舊的差只在事前列出的版本比對項：`gameplay_evidence.py:140-141`、`gameplay_soak_evidence.py:178-181`、`combat_gui_evidence.py:155`，以及輸出的 `protocol` 欄位。GUI 在版本檢查就提前返回，所以另做一輪：把 pv6 報告的版本欄位改成 7 再比，只剩 `protocol` 欄位本身不同，GUI 的其餘證據也逐值一致。
  - `run_quad` 的 mixed 改寫成 pv7 版（protocol-v7 §1「驗收工具」）：older-client 只接受版本錯誤；older-match 要求 pv7 Gateway 寫出期望值與收到值；older-gateway 要求 pv7 Match 寫出 eof 提示。`older-arena` 退役：它驗的是 v6 的容量變更，pv7 不改 arena（v7final 的 arena 逐位元相同），容量拒絕由 `ArenaDigestTests.cpp:133-134` 與突變覆蓋。對 v7final 的 pv6 執行檔跑一次，3／3 通過；改寫前那一次的 older-arena 失敗保留在 `short/mixed/`。
  - 短跑次（只記錄；pv7 工具與執行檔，沒有閒置閘門）：clean-60、network、backpressure 6 情境、soak-short、quad clean-60 一輪，全部通過；network 的 `other_version_datagrams` 都是 0。有 11 個數值落在 16-L2 的範圍外，最大的是 quad 一輪有 8 個替代命令、actual P95 42 ms。之後 v7final 與 pv7 交錯各跑 4 輪 quad clean-60：兩邊都偶爾出現替代命令（v7final 一輪 4 個、pv7 一輪 11 個），其餘輪次都是 0、P95 37.5～38.0 ms。判斷為主機擾動，不是回歸。
  - Gateway 執行檔 `gyo_object_fps_pvp-gateway` 是不在 ALL 裡的自訂 target，跑驗收前要單獨建（`cmake --build --preset test --target gyo_object_fps_pvp-gateway`）；這次建之前，它還是 10-11 的舊版。
  - 證據：`build/target/_build/test/logs/pvp-v71-version-errors-20261016/`（`digest-scale{1,3}`、`frozen-pv7.sha256`、`pv6-corpus/`、`short/`、`short/quad-ab/`）。
  - ultracode 審查（3 位審查者：契約一致性、測試與驗收工具、架構與跨平台；1 位對抗式檢查；沿用主對話檔位；約 25 分）：19 項發現，對抗式檢查確認 17 項（有重複）、推翻 2 項。在契約範圍內的都已修正：
    - HTTP join 先讀版本，再做嚴格解碼與 request_id 檢查：之後版本的 Client 多帶一個鍵，仍然得到 409 與 Gateway 的版本（原本會是 400 `invalid_join`）。
    - Gateway 讀 IPC envelope 時先單獨讀欄位 1：另一版的內容在本版 schema 下解析失敗時，仍報版本不符，Ready 時也照樣送告知。Match 端不改：Ready 之後它會收到的另一版框架只有只含欄位 1 的告知，一定能解析。
    - Client：join 成功回覆缺少 `protocol_version` 時也是 `protocol_version_mismatch`（原本是 JSON 例外）。
    - Match 的 eof 提示：閒置的同版 Gateway 停下時也會觸發，措辭改為「it was idle or speaks another protocol version」（前綴不變）。`heartbeat-sync` 的心跳之後會讓這種情況變少。
    - 補測試：join 的版本優先與 log 行；400 的版本拒絕；不是無號整數的版本（null、-1、"8"、7.5）；join 回覆缺版本；只含版本的告知（版本 1～6 與 8）；另一版加上本版會拒絕或無法解析的內容；eof 之後沒有提示的反面案例；arena 出生點不足時 `IpcHost` 拒絕建構。IPC 違規測試改成只看該次連線之後的 log；eof 提示的測試改成正常關閉（shutdown 後讀到 Match 關閉），避免連線被重設造成的偶發失敗。
    - 突變再加 7 個（這批共 14 個）：join 先判內容再判版本、envelope 先解析再讀版本、Client 只認 409、Client 接受任何數字、Match 先判空訊息、提示不看框架數、Match 接受出生點不足的 arena。`v71ve-envelope-version-unnamed` 改 find 字串。
    - `run_quad` 的 mixed：older-match 加上端到端確認（pv6 Match 以自己的原因 `frame parse or protocol version` 關閉，表示收到了告知）；Match 記錄連線重設（`read:`）時標為無法判定，最多跑 3 次並保留每次的紀錄；docstring 改指 IpcHostTests 的 arena 測試（審查指出 `ArenaDigestTests` 只測判斷函式，沒測到 `IpcHost` 的呼叫）。
    - 推翻的 2 項：§1 附註的措辭（屬措辭偏好）；v6 驗收文件裡 mixed 的說明（v6 的歷史紀錄，不改）。
    - 動到已定稿契約文字的 2 項先保留契約原文、轉成使用者決定；D99 照建議修改（另一個 commit）。
  - 修正後重驗：`test` preset 全量 CTest 70／70；`go vet`、`go test ./...` 通過（新的 Go 測試另以 `-race -count=3` 跑過）；突變 14／14 與 `09-version-python` 都 killed（`v71ve-ipc-version-as-parse` 第一次是 wrong_failure：測試改用 `mismatch(version)` 組字串後，doctest 只印出運算式，改 expect 後重跑 killed）；權威 digest 兩樹比較 scale 1、3 各 35 情境 0 差異；mixed 對 v7final 的 pv6 執行檔 3／3 通過，都在第一次判定。分析器只多改了 `run_quad.py`（runner，不在 pv6 語料核對的範圍內），核對結果仍然有效。證據 `after-review/`。
  - Architecture Delta：無。wire 不變；契約文字兩處依 D99 修改（Data Contract 的條文，非 wire）。新的依賴只有產品內部的：`IpcHost.cpp` 用 protobuf 的 `UnknownFieldSet` 列出未知欄位，Gateway 用 `protowire` 寫版本告知；Engine 與共通層沒有改。
  - 實際約 1.3 h（02:01～03:20，含審查 25 分；估 12 h）。
  - 四平台 L1（手動觸發，`046a0fd`）：run 38784720725，L1 四平台、Quick acceptance、CI gate 全部通過。

- 2026-10-16：D99，protocol-v7 §1 的兩處條文照建議修改（`16216ff`）。

- 2026-10-16 10:22：`heartbeat-sync` 開始（D100，檔位 high，時鐘域局部 xhigh，估 9 h）。一開始依會話的 ultracode 開關啟動了設計 workflow，D101 之後停止（沒有 agent 完成），改照計畫的檔位：high 實作，時鐘域局部 xhigh。

- 2026-10-16：`heartbeat-sync` 實作與驗收。
  - wire：`runtime_v7.proto` 加 `ClockSample`（欄位 1～3）與 `RuntimeEnvelope.clock`（oneof 20）；Go 綁定照上一批的方式重產。`client_v7` 的 ClockSample 留到 `time-echo` 再加（這批用不到）。
  - 估計器：Match 的 `ClockSync`（`ClockSync.hpp/.cpp`，放在 ipc 函式庫、不依賴 protobuf，`time-echo` 時 Client 也能用）與 Gateway 的 `clock_sync.go` 是同一套演算法，時間都由呼叫者傳入：
    - transmit＝max(now, 上一則＋1)；比對自己最近 8 個 transmit；四時間戳公式；負 delay 丟棄並計數。
    - 語意違規計數後忽略（沒有否決權）；時間 ≥2^53 µs 視為不合法，int64 運算因此精確。
    - 視窗不用固定格線：滿 10 秒（或連線結束）時結算，offset 取 delay 最小的樣本。
    - 跳變：兩個視窗的 offset 差，超過 max(1 ms, 100 ppm × 兩樣本的間隔)，再加上兩邊 delay 的一半（各自的不確定度）。
    - 漂移：跳變之後最多 30 個視窗的最小平方回歸（ppm）。
    - log 行族兩端同格式：`clock sync window_ms=… samples=… missing=… invalid=… negative_delay=… replaced=… offset_us=… delay_min_us=… delay_max_us=… drift_ppm=… jump=… final=…`（日誌格式在 `log-contract` 之前暫定）。
  - Match（IpcHost）：
    - 心跳回覆放在 session 的單槽（新蓋舊，計入 replaced），寫出排在 controls、action lane、snapshot 之後，transmit 在選中框架時才蓋；ScheduleWrite 也看這個槽。
    - 每次 Pump 檢查視窗；連線關閉時寫最後一個視窗（`final=1`）。
    - 心跳本身也是框架，所以送過心跳的同版 Gateway 停下時，不再觸發 eof 提示。
  - Gateway：
    - Server 擁有的 1 Hz 診斷 goroutine（`diagnostics.go`）組心跳，放進 link 的單槽診斷佇列（新蓋舊並計數）。
    - writer 只多一條獨立喚醒的分支：寫出這個槽，不呼叫 `take()`、不動 `l.wake` 與動作計時器。
    - reader 的活性改成以剩餘時間呼叫 Read，只由 Snapshot、JoinResult、Evicted、Error 刷新（ActionResults 與時鐘回覆不刷新）；剩餘 ≤0 時不呼叫 Read，直接以 `runtime liveness: i/o timeout` 失效。活性從 `run()` 起算。
    - `Server.Close` 時同步寫最後一個視窗（和診斷 goroutine 共用一次）。
  - D32：
    - 原始碼守衛：`test_diagnostic_isolation.py`（CTest `object_fps_pvp.diagnostic_isolation`）確認契約列出的決策檔案不引用時鐘符號、估計器只在 I/O 層；Go 的 `TestClockStaysOutOfDecisions` 確認 session 套件與 `server.go` 不引用。
    - 差分測試：IpcHost＋host（假時鐘，由測試推進 tick；每步以 Leave(99) 的 log 行當柵欄）同一情境跑兩次，一次沒有心跳，一次有極端值的心跳（相隔 10 s、重複、比對不到的 origin、receive 晚於 transmit、跳變）。決策（含由參考年齡判出的 EXPIRED）、join 結果與最後一個 snapshot 逐位元相同；有心跳那次 Match 確實量到樣本。
  - 測試：
    - Go：估計器 7 個；活性表格（4 種刷新、2 種不刷新，假時鐘）；實時預算；單槽；寫出分支 3 個（不取資料 pass、不延後計時器、不吃掉資料喚醒）；往返量測；golden bytes；守衛。
    - C++：估計器 7 個；IPC 4 個（回覆與量測、burst 的「回覆數＋replaced＝送出數」、差分、golden bytes）。
    - 突變 15 個（`v71-heartbeat-sync`）。
  - 時鐘域局部 xhigh 檢查（1 位，約 30 分）：沒有 must 級缺陷；活性、寫出分支、鎖順序、時鐘域、D32 隔離都核對為正確。should 4、note 10 都是測試面的缺口：
    - 已補：寫出分支的計時器與資料喚醒、回覆計數、差分測試量到樣本並正常關閉、golden bytes、活性表格、跳變的兩項容許、驗證順序、Gateway 結束時的最後一個視窗、實時測試的預算放寬到 1 s。
    - 審查中發現、已修：reader 的活性原本從 goroutine 啟動時起算，改成從 `run()` 起算（測試推進假時鐘時暴露）。
    - 未做，記在未結事項：見下方。
  - 驗收（L1，本機）：`test` preset 全量 CTest 71／71；`go vet`、`go test -race -count=3` 通過；權威 digest 同機兩樹比較（base＝v7final `6d3f287`）scale 1、3 各 35 情境 0 差異；突變選出 149 個（這批 15、`v71-version-errors` 14，以及目標檔案在這批被改過的既有突變 120），全部 killed。過程中有兩個先是逾時、判讀失敗，修測試後重跑 killed：
    - `v7-07-ipc-no-publish-wake`：Match 完全不回應時，IPC 測試逐一等待逾時，超過 CTest 上限。修法：違規測試開頭先用 REQUIRE 做一次檢查；ipc 測試的 TIMEOUT 由 60 秒放寬到 120 秒。
    - `v71hb-heartbeat-wakes-data`：Go 測試等心跳寫出時沒有截止時間，加上了。
    - 改過 IPC 測試之後，21 個 ipc 突變重跑一次，全部 killed。
  - 同機長跑次（只記錄，soak-long 113 個循環約 30 分，pv7 工具與執行檔）：兩端各約 180 個時鐘視窗、約 1810 個樣本。缺樣本、跳變、不合法、負 delay、被替換都是 0；offset 在 30 分鐘內只飄 32～87 µs；delay 最小值的中位數約 0.1 ms；漂移約 0 ppm。soak 本身的判定兩次都沒有通過，都是主機干擾：
    - 第 1 次（`long/`，10:56～11:29）：Client 的長幀與 gap 發生在第 1246、1399、1599 秒，同一段時間 xhigh 審查者正在反覆跑整套 `go test -race`。
    - 第 2 次（`long2/`，12:54～13:27）：唯一的異常（Match 的 runtime gap、兩位玩家的 Client gap、111 ms 長幀）全都在第 1206.6 秒，也就是 13:14:18。系統電源紀錄顯示 13:12～13:15 用電池（使用者移動機器）。其餘 Client gap 都是正常的復活（序號 3）。
    - 判斷：心跳不碰 Client 與模擬，時鐘數據全程正常，所以不算這批的回歸；soak 的正式判定留到之後的 L2。
    - 睡眠那段（中途讓系統睡眠，量跳變）要你在場，等之後的 L2 一起做（D102）。
  - 證據：`build/target/_build/test/logs/pvp-v71-heartbeat-sync-20261016/`。
  - Architecture Delta：無。新的依賴只在產品內：ipc 函式庫多一個原始檔 `ClockSync.cpp`；Gateway 多 `clock_sync.go`、`diagnostics.go`。Engine 與共通層（`services/gyo_gateway`）沒有改。wire 新增 `ClockSample`（契約 §3 已定）。
  - 實際約 3.1 h（10:22～13:30，含 xhigh 檢查約 30 分、突變約 1 h、soak 兩次各 30 分；估 9 h）。

- 2026-10-16：全 Windows LAN 臨時測試（使用者要求，不是批次；只記錄）。產物：Package trial run 38833224220（`16216ff`，即 `version-errors` 完成時，`heartbeat-sync` 之前）。構成：Main Client（i7-8750H、GTX 1060）同時跑 Match、Gateway 與 Client，Client A（Core 7 240H、RTX 5050）、Client B（i7-12700H、RTX 4060）連線；三台都是 Wi-Fi、JST，連線都用 192.168.50.x。三人同時在場約 4 分 47 秒（10:55:53～11:00:40），Gateway 從 10:49:11 運作到 11:01:13。方案 A 健康報告（檔位 high，約 1.5 h）：
  - 結論：可以穩定遊玩。斷線、逐出、版本錯誤都是 0，Gateway 的 `drop_*` 與限流也都是 0。
  - 替代命令：p1（A）233／42129 晚到（0.55%），p3（B）64／17154（0.37%），p2（Main）0。九成集中在 4 波（A：10:52:17、10:52:47、10:55:17～28；B：10:57:38～48），和 Client 端的 Snapshot 年齡尖峰（A 最大 124.2 ms）、`connection quality failed_windows=1 of 3` 同時出現。同一窗口 Gateway 收到的封包數不變（每秒約 61），重複命令與寫出時已解決的命令暴增，代表是延遲後一次擠過來，不是遺失。〔推論〕來源是各 Client 那一段 Wi-Fi：10:57 那一波兩人同時在場，只有 B 受影響；Main 用自己的 LAN IP 連線，封包不經過無線電。
  - Windows Match 的 Tick（F7 的第一份實機數據）：主機上沒開 Client 時（10:49:11～10:51:50）晚醒 ≥2 ms 佔 1.67%（9632 Tick 中 161 個，最大 14.7 ms），開著時（10:51:50～11:01:13）0（33707 Tick，最大 1.6 ms）。〔推論〕本機的 GPU 程式改變了計時器解析度或省電狀態；樣本只有 16 個窗口。
  - Client A 在 11:00:51～11:01:03 只有 13～43 FPS（最長幀約 90 ms），Snapshot 年齡正常，是本機畫面的問題，原因不明。
  - 新發現記到 INVENTORY：`N-summary-counter-wrap`、`N-hit-feedback-not-logged`（使用者回報 Main 有一次受擊沒閃紅，時間不記得，日誌無法定位）、`N-windows-max-path`；F7 加上這次的數據。
  - 端到端延遲和凍結分析器的 trace 分析（方案 B）沒有做。
  - 證據：`build/target/_build/test/logs/pvp-v71-windows-lan-20261016/`（`logs/`、`Info.txt`、分析腳本 `scripts/`；10/08 的舊日誌已排除）。

- 2026-10-16 13:50：`time-echo` 開始（D102，檔位 high，估 4 h）。`heartbeat-sync`（`4a66b58`）的四平台 L1 是 run 38850836007。

- 2026-10-16：`time-echo` 實作與驗收。
  - wire：`client_v7.proto` 加 `ClockSample`（和 runtime 同號同型）、`Hello.clock`（2）、`Welcome.clock`（11）；Go 綁定照前兩批的方式重產。不新增封包與 GYOP 種類。
  - 估計器：C++ 的 `ClockSync` 改成只有標頭檔（`ClockSync.cpp` 刪除），讓 ipc 與 client_network 兩個函式庫共用，不新增 build target。三個突變的檔案路徑跟著改，find 字串不變。
  - Gateway（`echo.go`，時鐘程式不進 `server.go`）：
    - 每個 session 一份估計器；Hello 通過認證後才讀它的 clock，不合法就計數、忽略，Hello 的 admission、活性、Join、Welcome 照常。
    - Welcome 回聲最新的已認證 Hello，transmit 在編碼時蓋。
    - 每次 Hello 檢查 10 秒視窗，寫 `client clock sync player=… window_ms=…`；session 移除時寫最後一個視窗。runtime 失效時直接清空玩家，不寫最後一個視窗。
  - Client（`ClientConnection`）：每次建立 UDP 連線時重設估計器；Welcome 一解析出來就量測，排在 `CheckArena` 與規則檢查之前，不影響 welcomed；Hello 帶回聲並蓋 transmit；每次網路處理檢查 10 秒視窗，寫 `clock sync` 行；關閉連線時寫最後一個視窗。
  - `ReadyFitsWelcome` 把最大的 ClockSample（三個欄位都是最大值）算進最壞情況的 Welcome；既有的邊界測試改成含 clock 時剛好 1200 bytes。
  - D32 守衛：估計器的使用者改為 `ClientConnection.cpp`、`ClockSync.hpp`、`IpcHost.cpp`。
  - 測試：
    - Go：Welcome 回聲最新的已認證 Hello 並量到樣本；回聲不合法或沒有回聲時一切照常；未認證的 Hello 不被量測；離開時寫最後一個視窗；最壞情況 Welcome 含 clock。
    - C++（worker 的模擬 Gateway）：第一個 Welcome 的回聲不合法時 Client 照樣進入 Playing；每個 Hello 回聲最新的 Welcome、transmit 遞增；第一個 Hello 沒有回聲。
    - 突變 5 個（`v71-time-echo`）。
  - 驗收（L1，本機）：`test` preset 全量 CTest 71／71；`go vet`、`go test -race` 通過；權威 digest 同機兩樹比較 scale 1、3 各 35 情境 0 差異；突變選出 80 個（這批 5 個，以及目標檔案在這批被改過的既有突變 75 個），全部 killed。
  - 短跑次（只記錄，pv7 工具與執行檔）：clean-60、network、soak-short、quad clean-60 一輪全部通過，凍結的分析器能接受新的 log 行。回聲的數據互相吻合：
    - Gateway 量到的 offset（C−G）和 Client 量到的（G−C）正負相反、大小相同，差在幾十 µs 以內。
    - 同一台機器上 Match 和 Client 都用 steady_clock，所以 Gateway 對兩者量到的 offset 幾乎相同。
    - delay 最小值 0.1～0.4 ms；沒有不合法、負 delay 或缺樣本。
  - 證據：`build/target/_build/test/logs/pvp-v71-time-echo-20261016/`。
  - Architecture Delta：無。產品內的變動：`ClockSync` 改成只有標頭檔，ipc 的原始檔少一個；Gateway 多 `echo.go`。
  - 實際約 0.7 h（13:48～14:30；估 4 h）。

- 2026-10-16 14:41：`drop-report` 開始（D103，檔位 high，列舉與上限局部 xhigh，估 13～16 h）。`time-echo`（`55dc734`）的四平台 L1 是 run 38855969436。

- 2026-10-16：`drop-report` 實作與驗收。
  - wire：`runtime_v7.proto` 加 `GatewayDropReport`、`DropCount`、`DropSample`、`DropReason` 與 oneof 21。過渡原因的號碼由契約的保留號與 `ingress_statistics.go` 的鍵順序還原：16、17＝A3／A5 的封包，20、21＝其射擊數，22＝截短，23＝拒絕線溢出，24～26＝Leave 時清除（命令、射擊、ACK），43、44＝D1、D2；這個中間 commit 先用，edge 批刪掉出口時一起刪、改成 reserved。32～34 只定名稱，出口在 edge 批。
  - 名稱表：`drop_report.go` 的 `dropReasons` 是 DropReason↔ingress 鍵的唯一綁定；`tests/object_fps_pvp/fixtures/drop_reasons.json` 是 Go、C++、Python 共用的 golden（40 個值）。
  - 新出口（新鍵加在行尾）：玩家的 `drop_write_failed_{snapshot,results,control}_packets`（D5）、`drop_control_queue_full_control_packets`（D6）、`drop_replaced_snapshot_packets`（送出迴圈中途被取代，每位還沒拿到的 active 玩家各 1）；Gateway 的 `drop_coalesced_snapshots`（讀取端新蓋舊）。它們不在 IG 內（是事件，不是收到的資料包）。ingress golden fixture 維持凍結分析器釘住的 v1 鍵，Go 測試另外確認行尾的新鍵。
  - 帳本（`dropLedger`，葉鎖）：Server 的 `countIngress`／`countIngressAt`／`countGatewayLocked` 與 link 的 `countLocked` 在更新 10 秒行的同一處記入累計值。樣本帶收包時刻與 GYOP 序號（Server 在處理資料包時記下）或事件時刻與身分（下行的 Gateway 序號、snapshot tick）；link 內的過渡原因只帶項目數與事件時刻。
  - 回報：1 Hz 計時器隨心跳標記一輪；writer 在寫出前一刻才組每條串流的回報（report_sequence、樣本、dwell 都在那時決定），每輪最多 MaxPlayers×2 條；玩家移除後送一則 final（從沒計數過的不送），延後佇列滿了就併進 Gateway 串流的 `merged_streams`；每寫出一則記一行 `drop report player=… report_sequence=… …`。
  - 優雅關閉：`Server.Close` 改成先寫所有非空串流的 final（含 Gateway 串流與延後中的）再關 link；writer 正在寫時照原本的方式中斷，不寫 final（記一行 log）。link 已失效時無法回報（契約的「只寫 log」）。
  - Match（`IpcHost` 的 `DropReports`）：每則回報記 `gw_drop report <同 Gateway 的內容>`；序號、累計倒退或消失、final 之後、範圍、樣本原因、samples_omitted／merged_streams 倒退都記 `gw_drop violation …` 並計數，不關 link（D-5）。
  - D32：守衛加入丟棄回報的符號；差分測試的有心跳那一輪也送極端值的回報。
  - 驗收（L1，本機）：`test` preset 全量 CTest 72／72；`go vet`、`go test -race -count=3`；權威 digest 同機兩樹比較 scale 1、3 各 35 情境 0 差異；突變：本批 17 個（`v71-drop-report`）加上受影響的既有突變共 194 個全部殺死。第一輪有 6 個沒殺死，都是突變或測試寫法的問題，修正後重跑全部殺死：兩個 Go 突變改成仍能編譯的替換；兩個 C++ 違規檢查改成逐行字串常值（doctest 原本印出的是變數名，不是內容）；`v7-flow-snapshot-regressed-uncounted` 的期望容許行尾新增的鍵；`v7-flow-final-before-writer-exit` 見下面 must-1。
  - 短跑次（只記錄）：clean-60、loss-shot、backpressure 6 情境、network、soak-short、quad clean-60 一輪全部通過。有回報的跑次（backpressure 的 gateway 與 host-ipc 故障）兩端依 (player, report_sequence) 逐則相等、沒有違規；乾淨跑次沒有丟棄、也沒有回報。host-ipc 的串流沒有 final：Match 那端先停，Gateway 讀到 EOF、link 失效，無法回報，符合契約。比對腳本 `compare_reports.py`。
  - 局部 xhigh 檢查（列舉與上限，1 位，約 38 分）：列舉、範圍、名稱、golden 四方一致，累計、上限、合併與單一寫出者都核對為正確；must 1、should 6、note 7。處理：
    - must-1（已修）：優雅關閉時 writer 正在寫，所有 final 都不送。改成一律有上限地等 writer 退出（3.5 秒，長於一次寫出的期限）再寫 final；writer 在每則寫出前看到 link 關閉，就把剩下的記為 abandoned 並返回，不再觸發 runtime 失效。等待上限改成套件變數；既有的 `blocked_writer` 收尾測試把它縮成 20 ms，讓 writer 撐過等待、關閉放棄 final 並中斷寫出，期望值維持 abandoned 3（這樣「final 行要等 writer 退出」的突變仍會被抓到）。
    - should-1（已修）：「每條串流每秒最多 1 則」原本只做到「每次觸發最多 1 輪」。現在距離上一輪不到 1 秒時延後，並在滿 1 秒時喚醒 writer。
    - should-2（已修）：Match 對 dwell 超過 10 秒的樣本記 `dwell_range` 違規。
    - should-5、should-6（已補）：
      - Go：執行中的 Server 有變化才送、離開後恰好一則 final；關閉時已回報過的串流與延後中的 final 也送；不先取消 ctx 的 Close；生產路徑的下行出口（Welcome、snapshot）帶 Gateway 序號與 tick；43、44 帶 tick；上行樣本帶 GYOP 序號、dwell 從收包起算；dwell 夾在 10 秒；每秒最多一輪；上限對照契約值；排序用 7 個原因；log 行格式釘死（C++ 測試用同一字串）。
      - C++：補齊各類違規（merged_streams、samples_omitted 倒退、超過 4 個樣本、counts 亂序／重複／為 0、counts 消失、dwell 範圍）。
      - 測試改用 `setForTest`（`t.Cleanup` 還原全域變數），避免 race detector 抓到的「還原早於背景 goroutine 結束」。
    - should-3、should-4：動到定稿契約與架構，由使用者決定（D104、D105）。
    - note：拿掉沒人讀的 `Violations()`；更正 IG 的註解（新增的下行 `_packets` 不在 IG 內）。其餘記在未結事項。
  - 證據：`build/target/_build/test/logs/pvp-v71-drop-report-20261016/`。
  - Architecture Delta：Gateway 新增的診斷狀態欄位，已核准（D105）。其餘在產品內：Gateway 多 `drop_report.go`；`Server.Close` 的收尾改成先寫 final；ipc 測試多連結 nlohmann_json（讀 golden）。
  - 實際約 2.7 h（14:41～17:21，含 xhigh 檢查約 38 分、突變約 1 h；估 13～16 h）。

- 2026-10-16 17:25：`edge-results-resend` 開始（D106，檔位 high，通道計時局部 xhigh，估 7～8 h）。`drop-report`（`598b609`）的四平台 L1 是 run 38873139138（手動觸發）。D104 的 §2 措辭與 D105 的白名單分類已寫進 protocol-v7。
- 2026-10-16：`edge-results-resend` 實作與驗收。
  - Match（`IpcHost` 的 `ActionLane`）：由「每個裁決只送一次」改成 §2a 的重送。Client ACK 之前每位玩家每 1/30 秒最多一框；每框是退役值加至多 8 筆裁決：最小的 4 筆未確認 id 每框都有，其餘 4 個槽位用游標循環，框內依 id 遞增。退役前進時送一次；ACK-only 而且 0 < ack ≤ 退役值，或批被拒收（Host 拒收、超過 8 筆、格式錯誤）時，再送一次（`asked`，仍守每間隔一框）。
  - Gateway：行為不變，仍以帳本冪等合併副本；只更正過時的註解。runtime proto 的 `ActionBatch`／`ActionResults` 加上送達註解（Go 綁定重產，差異只有註解）。
  - 測試：
    - IpcHost：改寫原本「每個裁決只送一次」的測試（現在是重送次數的上下限與退役後停止），新增最小 4 筆與循環（16 筆、14 筆兩種，後者在框內繞回）、ACK-only 補送退役、三種拒收各回一框、連續 60 次拒收仍守每間隔一框。
    - Go：副本在 Gateway 不改變任何東西（「只送一次」與「加上副本」兩種 IPC 輸入，假時鐘下 Client 的資料包逐則相同，含遺失與 ACK-only）；另用忠實的通道模型和「立刻送」比較 Client 第一次收到每個裁決的時刻（見「等你決定」）。
  - 突變：本批 12 個（`v71-edge-results-resend`）加上受影響的 v7-08b 與 `v7-flow-ledger-ipc-*` 共 31 個，全部殺死。`v7-08b-lane-no-sent-tracking` 刪除（它描述的「重送到 ACK 為止」就是現在要的行為）；`v7-08b-lane-repeat`、兩個 `v7-flow-ledger-ipc-*-silent` 改成新寫法。第一輪有 2 個是錯誤訊息不符：`v7-08b-lane-unpaced` 讓差分測試的讀取迴圈停不下來（重送不設節奏時 Match 一直在寫），整個 ipc 測試逾時；讀取迴圈加上 2 秒期限後，用預期的訊息失敗。`v71rr-cursor-stuck` 讓 id 14／16 永遠送不出去，在等那一個裁決時就失敗，期望值加上這個訊息。
  - 驗收（L1，本機）：`test` preset 全量 CTest 72／72；`go vet`、`go test -race -count=3`；權威 digest 同機兩樹比較 scale 1、3 各 35 情境 0 差異（IpcHost 不在 digest 的路徑上，只作確認）。
  - 局部 xhigh 檢查（通道計時，1 位，約 13 分）：must 0、should 3、note 8。
    - should-1：新裁決可能晚到，§9「Client 看到的不變」的說法太強。無損模型 8 個種子：369 個裁決中 63 個（17%）比「立刻送」晚到，最多晚 33 ms（上限「≤1 個間隔＋1 ms」在跑之前寫定）。§9 依 D107 修改。
    - should-2（已補）：連續觸發時的速率上限測試，與「被觸發的通道立即到期」突變。
    - should-3（已補）：循環槽位在框內繞回的情形、框內嚴格遞增（Client 會整框拒收重複 id），與「不排序」「不繞回」兩個突變。
    - note（已處理）：只有 ACK-only 批才讀 Host 的結果；補「超過 8 筆的拒收不回覆」突變；更正兩處註解。
    - note（記錄）：見未結事項。
  - 證據：`build/target/_build/test/logs/pvp-v71-edge-results-resend-20261016/`。
  - Architecture Delta：無（Match 的通道狀態多 `cursor`、`asked`，少了 `sent`；屬 IpcHost 內部）。
  - 實際約 1 h（17:25～18:19，含 xhigh 檢查約 13 分；估 7～8 h）。
- 2026-10-16：同步 master（`497c138`，PR #79、#80）。只有 AGENTS.md 與 network-architecture 的標題改變，沒有衝突。之後照新的 AGENTS.md 做：§3 的 `Design Invariants`（含 `Replacement Criteria`）與「状態の権威」、§14 Test and Defect Handling（改既有測試的判定要寫理由；驗收判定與 Design Invariants 守衛的判定要經你核准；事前宣告的門檻要在看結果之前核准）。本批改動既有測試判定的理由已寫在上面的紀錄與 commit 訊息。

- 2026-10-16 20:09：`edge-input-passthrough` 開始（D108，檔位 high，局部 xhigh，估 20～26 h）。`edge-results-resend`（`5c0d8f5`）的四平台 L1 是 run 38879679708（手動觸發）。

- 2026-10-16：`edge-input-passthrough` 實作與 L1（進行中，記錄器）。
  - Gateway：`runtime_link.go` 的輸入改成依到達順序的單一佇列（controls 與輸入窗口同一個 FIFO；controls 不丟、超過 64 則 link 失效；每位玩家至多 64 則輸入窗口，滿了丟最舊，計入 `drop_ipc_queue_overflow_input_packets`（回報 32，樣本帶被丟資料包的身分）與行尾的新屬性鍵 `drop_ipc_queue_overflow_input_commands`）。刪除 `lastResolved` 副本、命令表、epoch／life、拒絕線、已寫出去重、合併、截短、D1、D2；Leave 與逐出不再丟佇列中的輸入。解碼成功就提交序號與活性（D-11）。
  - 丟棄原因 22～24、43、44 在 proto 改成 reserved；golden 重產（35 個原因）。被刪路徑的 ingress v1 鍵照樣輸出 0。
  - Match：host mutex 的鎖等待直方圖（SubmitInput 與 step 各一，`network statistics` 行尾的 `host_lock_*` 鍵；只記錄，D32 守衛加一項）。protocol-v7 §9 寫「IpcHost 層」，實作在 `MatchRuntimeHost`（鎖在那裡），用語差異記在未結事項。
  - 測試：Go 的原樣轉送、到達順序、Leave 不超車、FIFO 丟最舊與回報、寫出卡住時的佇列深度、控制訊息上限、輸入立刻喚醒 writer、解碼後才提交、無 snapshot 守衛、I0；C++ 的副本差分（每個窗口送兩次並夾帶已解析命令與另一個 life 的窗口，判定與最終 snapshot 逐位元相同，並斷言 host 確實計到副本）、鎖等待的記錄；worker probe 的「Client 拒收舊、重複、倒退的 snapshot」。
  - 改了既有檢查的判定（AGENTS.md §14.4，理由）：`ingress_cross_check.py` 只接受 I3 以 protocol-v7 §9 宣告的形狀失敗（Gateway 副本 0、Match 副本 > 0）；其他形狀照樣失敗。舊的去重、路由、拒絕線測試隨程式刪除（`dedup_test.go` 整檔）。
  - 突變：舊的 50 個（去重、路由、拒絕線、截短、D1、D2）隨程式刪除；7 個改成新寫法；本批新增。第一輪 214 個中 205 個殺死，其餘是新程式下的寫法或期望值問題，修正後殺死（其中 `v71ip-match-takes-another-life` 讓副本差分的未來 life 窗口改帶不同內容才抓得到）。
  - 局部 xhigh 檢查（程式，1 位，約 27 分）：must 0、should 5、note 11。should 全部補測試與突變（控制訊息上限與歸零、另一位玩家的佇列、Server 組的樣本身分、輸入喚醒、溢出命令數）；note 處理了 link 拒收不再讓房間失效、Client 的 epoch 倒退、副本差分的計數斷言、測試持鎖 100 ms、過時註解；其餘記在未結事項。
  - D45 門檻來源檢查（L2 草稿第 1 版，1 位 xhigh，約 17 分）：送核准前必須修正 7、應修正 8；改寫成 [第 2 版](edge-input-passthrough-l2.md)，對應表在其第 11 節。也發現 clean 時 B 本來就是一包一則 IPC，D-8 擔心的負載主要在故障情境。
  - 兩個 commit：B `42f6e88`（只加鎖等待量測；B 樹全量 CTest 72／72）、A `6d9ecf5`；四平台 L1 是 run 38914084925。兩棵樹：`../GYO-Engine-ip-before`、`../GYO-Engine-ip-after`（detached、乾淨）。L2 工具在 `build/target/_build/test/logs/pvp-v71-edge-input-l2-prep-20261016/`，乾跑 31 種情況全部符合預期，A 沒有跑過任何項目。
  - 時間：22:46 時，實作、L1、兩次 xhigh 檢查完成；接著是兩個 commit、L2 工具與乾跑，然後停下來等你核准宣告。

- 2026-10-17：`edge-input-passthrough` 的 L2（D109、D110）**通過**。
  - 第 1 次 session 停在第 58 項（B 樹 clean-60 的 I1：關閉時被 reset，宣告缺漏），依 D110 只改 I1 的規則後整個重跑；第 1 次保留在 `build/target/_build/test/logs/pvp-v71-edge-input-l2-20261017/`。
  - 第 2 次 session（`pvp-v71-edge-input-l2-20261017b/`，00:58～03:08）：98／98 完成，逐跑次判定沒有停下，受干擾 1 個（network-impairments-r2-A，上限 3）；`judgement.md`。
  - J2 替代率：各情境 A−B 都在門檻內（clean、quad 0／0；network-impairments 21.9→22.7‰，門檻 +1.25；backpressure、short-stall 差 ≤0.03‰；x1 −0.4‰）。ramp、soak 只記錄（61.2→60.7、0.05→0.03‰）。
  - J3（soak）：兩棵樹逐出 0、`connection_quality_failures_max` 0。J4a（soak 180 個視窗）：A／B＝0.97（門檻 1.5）。J4b：所有情境的晚醒 p99 都在第 0 格。J4c：鎖等待 ≥250 µs 的比例兩棵樹都是 0。J7：成立。
  - 預測都沒有被推翻：P0 clean 的 `received_inputs` A 1921／B 1920，host-ipc-1000ms +101（預測約 +100）；P1 clean 的 Match 命令 A／B＝2.99；P2 clean 的 Match 副本 3838（B 的 written duplicate 3836）；P4 CPU clean 1.06、quad 1.02。故障情境的佇列溢出也都是 0。
  - 差異帶（給 `pv7-checkpoint`，宣告第 9 節）：以上的 A−B。
  - 實際約 7 h（2026-10-16 20:09～10-17 03:15；含 xhigh 兩次、L2 工具、兩次 session 約 3.5 h；估 20～26 h）。

- 2026-10-17：`heartbeat-sync` 的睡眠那段（D102、D109，只記錄）。A 樹（`6d9ecf5`）的 Match、Gateway 與兩個 Client 的 timing probe，跑次中由使用者從選單讓 Mac 睡眠。證據 `build/target/_build/test/logs/pvp-v71-heartbeat-sleep-20261017/`（`sleep_run.py`、`run/`、`clocks.jsonl`）。
  - 睡眠：`pmset` 紀錄 11:42:40 進入、11:43:14 喚醒（34 秒）；腳本每秒的讀數顯示牆鐘過了 31 秒，`time.monotonic`（mach_absolute_time）與 `CLOCK_UPTIME_RAW` 只過了 4.2 秒，`CLOCK_MONOTONIC`／`_RAW` 過了 31 秒：實際暫停約 26.8 秒。
  - 時鐘域：Go 的單調時鐘（G）睡眠時停止，C++ 的 `steady_clock`（M、C）繼續走。Match 的時鐘視窗在醒來後出現一次 offset 跳變約 26.73 秒（`jump=1`，跨睡眠的那個樣本 delay 約 26.7 秒）；Gateway 對兩個 Client 的回聲在跨睡眠的視窗各有 1 個 `negative_delay`。和事前的推論一致（未結事項的 heartbeat-sync 項）；protocol-v7 §2 的〔推論〕改成實測。
  - runtime link 撐過睡眠：Gateway 的活性用 G，看不到這段時間，沒有逾時；Match 到結束都沒有斷線（最後是正常的 EOF）。
  - Client 在醒來後約 1 秒斷線（`Gateway connection timed out`）：Client 用 C，看到約 27 秒沒有 Gateway 的封包，超過自己的逾時。兩位玩家因此 Leave，probe 以失敗結束。這是既有行為（筆電睡眠＝斷線），不是本批造成的；要不要在醒來後改成重連或延長逾時，是之後的功能題，記在未結事項。

- 2026-10-17 11:57：`edge-results-passthrough` 開始（D111，檔位 high，估 12～16 h）。`edge-input-passthrough`（`42f6e88`、`6d9ecf5`）的四平台 L1 是 run 38914084925（通過）。

- 2026-10-17：`edge-results-passthrough` 實作與 L1（記錄器）。
  - Gateway：刪除 session 的結果帳本（requests、decisions、退役、`retiredSent`、`retirementAsked`、結果節拍、補送退役）與 A3（依帳本驗證動作批）。Match 寫來的結果框架逐則檢查後放進該玩家的單槽 `resultSlot`（latest-wins），在「上次寫出完成＋1/30 秒」時放出；被取代的計入新鍵 `drop_results_over_rate_packets`（回報 34，樣本帶被取代框架的 action id 範圍）。給非 active 玩家的結果只計數（`gateway transport` 行尾的 `results_for_inactive_frames`，§4 (d)）。
  - 唯一寫出者：controls（Welcome、Error）改成在 UDP 寫出者（`sendUDP`）寫出前才指定 GYOP 序號。舊程式在入列時就編號，晚編號的 snapshot 或結果若先寫出，Client 會把序號較小的 control 當成舊封包丟掉；`backpressure_test` 原本把這個順序寫成期望值，已更正。
  - link：ACK-only 批即使 ACK 不超過退役值也會寫出（window 的 `asked`），讓 Match 收得到補送退役的要求；ACK 的帳目（D56）仍由 window 記。一開始用「ACK-only 走到達順序佇列」的做法，會讓 Leave 之後仍寫出的 ACK 被算成丟棄（`TestLinkConservesActionRequestsAcrossEveryExit` 抓到），改成這個做法。
  - 丟棄原因 16、20（A3）改成 reserved；golden 重產（33 個原因）。
  - 測試：單槽放出、到達間隔 ±0.5 ms 時 0 取代、Client 每隔一則丟一則時的送達（見下）、退役遺失後 2 個間隔內補到、非 active 玩家的結果只計數；改寫依 Gateway 帳本重送的舊測試（G5、G6 刪除，喚醒與 1 kHz 到達的節拍改寫）；上一批的 `results_resend_test.go`（Gateway 帳本合併副本）改寫成這些端到端測試。
  - **D112（已決定，2026-10-17）**：protocol-v7 §9 寫「每隔一則丟一則時，未確認 16 或 32 筆在 ≤2⌈n/8⌉ 則內送達」。照 §2a 的設計實測只能 ≤2⌈n/8⌉＋2（n＝16：6；24：8；32：10）。原因：Client 只能連續 ACK，有缺口時 Match 不知道 Client 已有哪些較大的 id，循環槽位會在一個收到的框架裡重送它已有的 id。測試暫時以實測的界限釘住。
  - 突變：舊的 6 個（Gateway 帳本的重送、補送、A3、上一批的 Gateway 合併）隨程式刪除；12 個改成新寫法；本批新增 5 個（含計畫要求的「到達即丟、門檻＝I」）。168 個中 166 個第一輪殺死，2 個是判讀問題（測試的空指標、期望值），修正後殺死。
  - 驗收（L1，本機）：CTest 72／72 與 preset 外的 4 個；`go test -race -count=2`。
  - Architecture Delta（D-14）：Gateway 新增狀態欄位 reservation 的 `results resultSlot`（§2a 允許：至多 1 則待送的結果框架與上次送出時刻）、Server 的 `resultsForInactive`（診斷計數）；link window 的 `asked`（隨 window 在 `edge-actions-passthrough` 刪除）；刪除 reservation 的 `actions`（session 帳本）。

- 2026-10-17 15:07～15:14：`edge-results-passthrough` 的 L2 第 1 次 session（`build/target/_build/test/logs/pvp-v71-edge-results-l2-20261017/`）在第 12 項停下：受干擾的項目累計 3 個（loss-shot-r1-B 之後 38.5 ms、gateway-1000ms-r1-A 之前 24.4 ms、clean-60-r2-B 之前 26.3 ms；門檻 10 ms），照宣告第 8 節停止。CPU 前幾名是 Google Chrome 的 Helper（42～54%）、WindowServer（螢幕因 `caffeinate -d` 保持亮著）與 Claude app。完成的 11 項沒有任何判定不成立。不重跑，保留為證據，等你決定（§14.1）。

- 2026-10-17：`edge-results-passthrough` 的 L2（D113）**通過**。
  - 第 2 次 session（`build/target/_build/test/logs/pvp-v71-edge-results-l2-20261017b/`，15:19～17:39；Chrome 結束後重跑）：120／120 完成，逐跑次判定沒有停下，受干擾 2 個（clean-60-r6-A、soak-long-A，上限 3；兩者都沒有 R3 超標）；`judgement.md`。
  - L1 下行延遲（每跑次 d 的 p95 的中位數，A−B）：clean-60 −0.08 ms、loss-shot −0.15、loss-result −0.04（被丟框架的恢復約 34.5 ms，兩棵樹相同）、loss-ack −0.05、host-ipc-1000ms ＋0.23、gateway-1000ms −0.31；門檻都是 1 ms。network 與 soak 只記錄。
  - L2 射擊時序（`to_tick_ms` p95 的中位數，A−B）：最大 ＋2.26 ms（host-ipc-1000ms），門檻 16.7 ms。
  - L3：合格項目 B 33、A 32；A 的回報有 `drop_results_over_rate_packets`（已驗到）。
  - 預測都沒有被推翻：P1 clean 的 d p95 A 1.29／B 1.37 ms；P2 回報 34 在 clean、loss、network、quad、soak 都是 0，只出現在停頓的釋放（host-ipc-1000ms 每跑次 29～30、gateway-1000ms 5 次中 2 次 29）；P3 relay 上的結果框架 A＝B（clean 每跑次帶裁決 43、只帶退役 43、重複 0）。
  - 差異帶（給 `pv7-checkpoint`）：以上的 A−B。
  - 第 1 次 session（15:07～15:14）因 Chrome 的干擾在第 12 項停下，保留在 `pvp-v71-edge-results-l2-20261017/`。
  - 實際約 6 h（2026-10-17 11:57～17:45；含 D45 檢查、L2 工具、兩次 session 約 2.5 h；估 12～16 h）。

- 2026-10-17 17:41：`edge-actions-passthrough` 開始（D114，檔位 high，估 14～17 h）。`edge-results-passthrough`（`f79bb10`）的四平台 L1 是 run 39002862931（工具鏈下載逾時重跑後通過）。

- 2026-10-17：`edge-actions-passthrough` 實作（記錄器）。
  - Gateway：刪除 link 的動作視窗（每位玩家的 requests、ACK 游標、退役、`asked`、30 Hz 持有與重送）與 A5（視窗驗證、玩家數上限）。每個解碼成功的動作 datagram 在 commit 序號之後（D-11），原樣成為一則 IPC 訊息，排進每位玩家的 FIFO（與輸入同一個佇列；審查後改成分開計數，各 64 則）；重複、衝突、ACK-only、未知 ACK、遠在視窗外的 id 都照樣送到 Match，由 Match 判定（`PvpMatch.cpp` 的 Conflict／OutsideWindow／Full 與 `CombatHostTests.cpp`）。
  - FIFO 滿時丟同種類最舊的那則，動作計入既有的鍵 `drop_ipc_queue_overflow_actions_packets`（回報 33，樣本帶該 datagram 的傳輸序號、action id 的最小／最大與筆數；ACK-only 沒有 id，填 0、筆數 1，見下面的審查）。Leave 與踢出不再丟動作：已排隊的照到達順序送到 Match。
  - 新鍵（附加在行尾，D-14 的計數）：`fwd_link_written_actions_packets`、`drop_cleared_on_close_actions_packets`、`drop_abandoned_on_close_actions_packets`；守恆是 `fwd_handed_actions_packets`＝寫出＋溢出＋關閉時清掉＋寫出失敗放棄。D56／D59 的 shot、ACK 鍵保留、恆為 0（`ingress_evidence.py` 凍結，`link_shots` 從此是 0＝0）。
  - 丟棄原因 17、21、25、26 改成 reserved；golden 重產（29 個原因；33 改成 exit）。
  - writer 迴圈：沒有動作期限之後，計時器只是閒置等待；`writerIdleWait` 在啟動時讀一次（`-race` 抓到測試還原它時 writer 還在讀）。
  - 測試：每個 datagram 一則 IPC 且內容不變、四個出口的守恆（300 個種子，四個出口都有被走到）、溢出樣本、Leave 後仍寫出、真實 writer 迴圈不節拍不重送；身分只認 session；結果單槽的取代與期限部分保留。改寫依視窗的舊測試（生命週期、驗證、玩家數上限、link 節拍）。
  - 突變：舊的 15 個（v7-08 的 link 期限 2、A5 的 2、D56 的 7、D59 的 4）隨程式刪除；8 個改成新寫法；本批新增 12 個（含 D45 檢查指出的「動作入列不喚醒 writer」）。選出 126 個（本批 12，以及目標檔案在這批被改過的既有突變），第一輪 125 個中 122 個殺死；其餘 3 個：`16-evict-keeps-link` 存活（踢出時不清 link 統計，補測試）、`v71ap-commit-before-decode` 存活（解碼失敗的動作 datagram 會 commit 序號，補 ingress 結果表的一列）、`v71ip-drop-newest` 是新加的溢出測試在突變下 panic（取空 shots 的第 0 個）、讓整個測試程式中止，改成不 panic 的寫法。補完後這 3 個與新突變 4／4 殺死。
  - 測試補了「動作入列立刻喚醒 writer」（`TestRuntimeLinkWritesAnActionAtOnce`）。
  - L2 事前宣告第 1 版（`edge-actions-passthrough-l2.md`）經 D45 的門檻來源對抗式檢查（1 位 xhigh，約 12 分）：必須修正 3、應修正 6、記錄 8，全部改進第 2 版（第 11 節）。工具目錄 `build/target/_build/test/logs/pvp-v71-edge-actions-l2-prep-20261017/`（judge／run／fixture／乾跑改寫；B 的 8 種跑次判通過、沒有誤停）。
  - 記錄給 A1：`FireGate.hpp:31-32` 的註解「Gateway writes inputs before actions」已過時（現在照到達順序；Client 同一輪先送輸入，影響在微秒級）。
  - 驗收（L1，本機）：`45bda2b` 時 CTest 72／72、preset 外 4／4、`go test -race -count=2`；四平台 L1 是 run 39022776414。
  - xhigh 程式碼審查（約 7 分）：守恆、順序、喚醒沒有找到缺陷；State Authority 沒有新增 Match 權威狀態的副本。修正：
    - must（已修）：輸入與動作共用每位玩家 64 則的上限，違反 §2a「每位玩家不少於 64 則輸入窗口」（writer 卡住時輸入約 0.71 秒就開始溢出，只有輸入時約 1.07 秒；也會被大量輸入擠掉動作）。改成同一個 FIFO 內兩種分開計數（`maxQueuedInputs`＝64、`maxQueuedActions`＝64），只丟同種類最舊的那則，與 §4 的 32、33 兩列「最舊的輸入窗口／動作批」一致，不改契約。
    - should（已修）：玩家離開後，佇列裡的批寫出時會重建 `actionStats`、一直留著。改成入列時建立、寫出時只在存在時記錄。
    - should（已修）：33 的 ACK-only 樣本把 ACK 值填進 action id 欄位，契約沒有定義。改成 id 為 0（ledger 照共通規則把那一批記成 1 筆）。
    - 測試缺口（已補）：輸入與動作混合溢出、同一玩家跨種類的到達順序、離開後寫出不重建統計、`checkIngressConservation` 加上動作批的守恆；新突變 3 個。審查提到的「被丟的那則依新來那則的種類歸類」在修正後是等價突變（被丟的一定與新來的同種類），不加。
    - 修正後重跑：兩個被改檔案上的突變與本批全部，69／69 殺死（加入時的等價突變 1 個已刪除）；`go test -race -count=2` 通過。
  - 修正提交 `5b4d626`（CTest 72／72、preset 外 4／4），四平台 L1 是 run 39025534085：第一次 macos-arm64 的 72 個測試通過後，上傳 artifact 時 DNS 解析失敗（`ENOTFOUND`，環境），CI gate 跟著失敗；重跑失敗的 job。`45bda2b` 的 run 39022776414 通過。A 樹 `../GYO-Engine-ap-after` 建在 `5b4d626`。
  - L2 宣告第 3 版：B 的 rapid 開發跑次 6 次（使用者在場、不碰滑鼠，全部 class P、Cooldown 0）；乾跑 82 列、驅動檢查、5 個模擬 session 全部符合預期，B 語料 66 跑次 0 誤停；preflight 通過。等使用者核准。
  - 2026-10-18 01:00：L2 session 開始（`pvp-v71-edge-actions-l2-20261017/`）。`5b4d626` 的 L1 重跑後通過。
  - 2026-10-18 01:01～01:18：L2 第 1 次 session（`build/target/_build/test/logs/pvp-v71-edge-actions-l2-20261017/`）在第 29 項 `rapid-edge-30-r1-B`（B 樹）停下：R6 的 I1 是 undetermined（宣告要求 passed）。完成的 28 項（B 14、A 14）沒有其他停止，受干擾 1 項。照 §14.1 不重跑、保留為證據。
    - 原因：這一跑次的 Match 日誌在 `final=1` 的 ingress 行之後沒有「ipc closed reason=eof」（其他 rapid 跑次與 B 的 6 次開發跑次都有），I1 的前提 `eof_before_final` 與 `single_connection` 都判不出來（`no_ipc_pause` 成立）。`run_action_short.py:258-268` 收尾時對所有行程幾乎同時送 SIGTERM，Match 若在讀到 Gateway 關閉之前處理 SIGTERM、寫出 final 行就結束，就沒有那一行。是 runner 收尾的競態，與樹無關（停在 B），和 D110（關閉競態）同類；目前 13 個 rapid 跑次出現 1 次。
    - 方案：
      - (a)（建議）照 D110 的先例修宣告：rapid 項目的 I1，在 Match 日誌沒有「ipc closed」行、而 `no_ipc_pause` 成立時，允許 undetermined（只記錄）；有那一行時照舊判 passed。judge 改一處、重算雜湊、乾跑加一列，整個 session 換新目錄重跑（第 1 次保留）。
      - (b) 改 runner 的收尾順序（先停 Gateway、等 Match 讀到 EOF，再停 Match）：要改凍結的 `run_action_short.py`（v7 第 09 批 C2-B 也用它），新雜湊、B 的開發跑次重跑。
      - (c) rapid 不判 I1（rapid 的用途是 Cooldown；I1 由 gameplay 項目判）。
    - 使用者（2026-10-18）：「我不想要改動小，我想要能稍微精確定位問題和處理掉的方案。」→ 不放寬，改成定位並修掉。
    - 定位（重現實驗，`scratchpad/rp/stoporder`）：用 B 的執行檔起 Match＋Gateway，先停 Match 的 3 次都沒有「ipc closed」與 clock sync final 行（final ingress 行有）；先停 Gateway 並等它結束的 3 次都有 `reason=eof`。和停下的跑次的日誌特徵相同。兩個缺陷：
      - Match：`IpcHost::Stop()`（`IpcHost.cpp:721-730`）直接關 socket，繞過自稱「結束 session 的唯一路徑」的 `Close()`（`:395-400`），還開著的 session 不寫最後的 clock 視窗與「ipc closed」。
      - rapid runner：`run_action_short.py:260-268` 一次對所有行程送 SIGTERM（競態）；gameplay runner（`action_probe.py:590-591`）是先停 Gateway、等它結束再停 Match。
    - 提出的處理：Match 的 `Stop()` 經 `Close("stopped")` 結束 session（先寫失敗的 IPC 測試）；rapid runner 改成 GUI → Gateway → Match 的順序；宣告第 4 版只換 A 的 commit 與雜湊（門檻不變）、B 的 rapid 開發跑次與乾跑後換新目錄重跑。
  - D117 的修正（2026-10-18）：
    - Match：`IpcHost` 把 session 結束的記錄（最後的 clock 視窗與「ipc closed reason=…」）抽成 `LogEnd`，`Close()` 與 `Stop()` 共用；`Stop()` 在 IPC 執行緒上對還連著的 session 記 `reason=stopped`，不重設 Match（最多等 2 秒，IPC 執行緒已失效時記一行說明）。測試 `PvP IPC logs the close of a connected session when the Match stops` 先確認失敗（3 個檢查不成立）再修，修後 IPC 測試連跑 3 次通過。修正後的 Match 被先停時，I1 只有 `eof_before_final` 不成立（D110 的形狀）；修正前是兩個前提都判不出來。
    - rapid runner：`stop_in_order()` 先停客戶端，再停 Gateway 並等它結束，等 Match 日誌出現 session 結束的行（上限 2 秒）才停 Match。測試用延遲 0.1 秒才出現 EOF 行的假 Match，驗證順序與等待。
    - 突變新增 3 個（`v71st-*`：Stop 不記錄、runner 先停 Match、runner 不等那一行）；新增 check `action_runner`。連同兩個檔案上的既有突變 36／36 殺死；CTest 72／72、preset 外 4／4。
    - 提交 `67d7da7`（L1 run 39064524202）；A 樹重建在 `67d7da7`。宣告第 4 版（工具目錄 `pvp-v71-edge-actions-l2-prep-20261018/`）：凍結清單換成只更新兩個 D117 檔案的 `frozen-pv7-d117.sha256`（第一次乾跑的 preflight 因原清單而不成立，預期之內）；B 的 rapid 開發跑次 6 次全部 P、都有 `reason=eof`；乾跑全部符合預期、B 語料 66 跑次 0 誤停、preflight 通過。
  - 2026-10-18 05:02～06:00：L2 第 2 次 session（宣告第 4 版，`pvp-v71-edge-actions-l2-20261018/`）在第 102 項 `gateway-1000ms-r4-B`（B 樹）停下：R1 的 D44 事件 E1＝1。完成的 101 項沒有其他停止（受干擾 1 項）；A 的 51 項全部通過。照 §14.1 不重跑、保留為證據。
    - 現象：player 1 的重設序列（相對於 Gateway 停頓開始）：+525 ms starvation、+1942 ms starvation（釋放後 934 ms，在窗口內）、+2959 ms **backlog**（釋放後 1950 ms，超出「釋放後 1.5 秒」的窗口 450 ms）。backlog 的條件是 30 個 tick 的排隊命令總和 ≥105（`PvpMatch.cpp:454-455`）：第二次重設後 player 1 的排隊數從 6 降到 4 就停住（正常是 2），約 0.7 秒後觸發。
    - 定位（到目前）：問題在第二次重設後（epoch 3）Client 的第一次相位決策。誤差是 tick 的整數倍（決策時 Match 已多排的步數），修正有上限，殘留的步數讓排隊數停在 3～4。
      - 本次 session：B 的 4 次是 +49.7、+49.8、+16.3、+66.9 ms（排隊數停在 3、3、2、4）；A 的 4 次是 −0.2、+17.0、+16.5、+16.9 ms（都回到 2）。
      - 上一場 L2（它的 A＝本次 B，同一批執行檔）10 次最多 +33.6 ms，都回到 2。
      - 只出現在 player 1（唯一會開槍的玩家；player 2 沒有動作）。本次 session 的主機都在 8 ms 計時器狀態，但上一場也大多是 8 ms，不足以解釋。
    - 根因（2026-10-18 定位，使用者：「繼續定位根因……定位完提出處理方案再停下」；工具在 `scratchpad/rp/rootcause/`）：
      - 逐命令時間線（Client 送出 → Match 收進 → 執行）：新 epoch 的第一個輸入 datagram，r4-B 在 Client 送出後 67.5 ms 才被 Match 收進（r4-A 17.1 ms，之後的都約 0.6 ms）；這段時間 relay 照常轉送，Match 沒有拒收，player 2 的輸入也沒到（Match 為它代用命令）。Gateway 的寫出延遲整場最大約 0.2 ms：封包在 Gateway 交給 link 之前就沒了。
      - Gateway 的計數：player 1 有 36 個輸入被 `drop_rate_limited_input_packets` 丟掉（A 33；player 2 是 4／3）。用 relay 的轉送時間重放 `session.go:118-123` 的限速（固定 1 秒窗口、超過 120 就拒；Gateway 停頓期間的封包在恢復時才讀到），**3 個跑次、兩位玩家、每一種封包的預測拒收數都和 Gateway 的計數完全相同**。
      - 因果鏈：Gateway 停頓 1 秒，核心替它緩衝了約 1 秒份的合法封包，恢復時一次讀進同一個窗口；player 1 平時每秒約 91 個（輸入 60＋動作約 30），約 0.4 秒後超過 120，窗口剩下約 0.6 秒的**每一個輸入**都被丟 → Match 讓它挨餓，在恢復後約 0.93～0.98 秒做 starvation 重設 → 新 epoch 的頭幾個輸入也落在同一個窗口被丟，Match 等窗口結束才開始執行 → Client 已多送了 1～4 步，相位修正補不回來，排隊數停在 3～4 → 約 1 秒後 backlog 重設，落在「恢復後 1.5 秒」的寬限之外。B 與 A 的差別只是重設離窗口結束多遠（67 對 17 ms），和樹無關；上一場同一個執行檔沒出事也是時機。
      - 每一次 gateway-1000ms 的 player 1 都有第二次 starvation 重設（`starvation/in: 3` 的其中一次），也是這個限速造成的；一直存在，被 D61 的 1.5 秒寬限蓋住。
      - 契約：固定窗口 120 包／秒寫在 `network-architecture.zh-Hant.md:950`（protocol-v5 也寫「既有 120 包／秒限制不變」），在通用層 `services/gyo_gateway/session/session.go`。
    - 提出的處理（等使用者決定）：
      - (a)（建議）改成 token bucket（每秒補 120、容量 120）：長期上限不變（任何 1 秒滑動窗口仍 ≤240，與固定窗口跨邊界時相同），但 Gateway 自己停頓 1 秒後讀進的緩衝封包（約 91 個）用掉累積的 token，之後 91／秒 低於補充率，不再丟。改契約條文；先寫失敗的測試（用這次的到達序列重放，期望 0 拒收；洪水仍限在 120／秒）、突變。L2 兩棵樹都帶這個修正（B'＝`f79bb10`＋修正，A'＝`67d7da7`＋修正），比較仍只看這批的改動；宣告第 5 版加判定：gateway-1000ms 的限速丟棄 0、player 1 的 starvation 重設只剩停頓中那一次。
      - (b) 只修 A，B 照舊：B 每次 gateway-1000ms 仍有這個時機風險（本場 4 次中 1 次），session 可能再停在 B。
      - (c) 這批先不修，記為已知缺陷、照宣告重跑：根因不處理，結果靠時機。
    - 使用者選擇（D118）：記為已知缺陷 K1，先完成功能，發版前重新測試與重新評估 (a)。
    - L2 怎麼完成（提案，宣告第 5 版，等核准）：只在 gateway-1000ms，一個 E1（D61 窗口外的非重生重設）在下列全部成立時記為 K1、不停；任一不成立照舊停：
      1. 它是 player p 的 backlog 重設；
      2. 用 relay 的轉送時間重放 `session.go:118-123` 的限速，預測的拒收數與 Gateway 每位玩家、每種封包的 `drop_rate_limited_*` 計數完全相同；
      3. player p 在恢復後有被限速丟掉的輸入，而且這個 backlog 之前的那次 starvation 重設落在被丟的時段內（第一個被丟的輸入到窗口結束）；
      4. 那次 starvation 與這個 backlog 之間，p 沒有其他重設。
      兩棵樹同一條規則；K1 的次數逐樹記錄（只記錄）。其他判定與門檻不變。乾跑用本次的 `gateway-1000ms-r4-B` 當真實 fixture（應記為 K1、不停），另做計數不符、starvation 不在丟棄時段內兩個變體（應停）。
    - 記錄：連線關閉後送來的動作 datagram 由 `server.go:471-479` 改記到 `room_unavailable`，不會遺失；handed 與出口分在 Server 與 link 兩張表，都在 `s.mu` 下一起取，不會出現出口早於 handed 的視窗（將來若有路徑在 `s.mu` 之外呼叫 `link.actions`，就要重新確認）；`tests/object_fps_pvp/fixtures/ingress_v1/README.zh-Hant.md:68,74` 的 D56／D59 說明留給 `edge-analyzers`。

- 2026-10-18：`edge-analyzers`（D119 之後開始；檔位 medium、守恆式 high；估 5～6 h，實際約 1.5 h）。
  - 分析器 `ingress_evidence.py` 改為 `pvp-v71-ingress-3`：認得 drop-report、edge-input、edge-results、edge-actions 四組 Gateway 附加鍵與全域的 `drop_coalesced_snapshots`（`unknown_keys` 清空）；每一行帶的組數（整個 log 取最小）決定哪些判定適用，舊日誌照舊判。
    - I0 的出口加上 FIFO 溢出的命令（N10），有溢出時也精確成立。
    - 新增 `removed_paths`（精確）：被刪路徑的鍵必須是 0（protocol-v7 §9「被刪路徑的 v1 鍵照原順序輸出 0」的直接檢查；edge-input 19 個＋全域 2 個，edge-results 加 A3 的 2 個，edge-actions 加 A5 的 2 個與 D56／D59 的 8 個）。
    - 新增 `link_actions`（精確，取代直通後的 `link_shots`）與 I1a（條件判定：Gateway 寫出的動作批＝Match 收到的動作批，前提同 I1，另加沒有寫出失敗放棄的批）。
    - 直通的 Gateway 上 I3 與 `link_shots` 為「不適用」（副本由 Match 判定；Gateway 不去重由 `removed_paths` 判）。
  - `ingress_cross_check.py`：拿掉 I3 的宣告例外；必要檢查加上 `removed_paths`、`link_actions`、I1a，I3 與 `link_shots` 必須是不適用；I1a 和 I1 一樣在關閉競態時重試。
  - 測試：分析器自測 52 → 60（直通：全部鍵組一致時通過、I0 含溢出、各個被刪路徑鍵、鍵組決定適用範圍、link_actions 各項、I1a 的不等與前提、全域附加鍵）。`link_actions` 的測試把出口鍵寫成字面值（突變縮短分析器的常數時，原本測試也跟著少測一項，見下）。
  - 驗收（新舊各分析一次，`build/target/_build/test/logs/pvp-v71-edge-analyzers-20261018/`：`compare.py`、`compare.json`，舊版取自 git 的 `pvp-v7-ingress-2`）：v7.1 四場 L2 與 v7 第 16 批 L2 共 669 個案例目錄。差異全部是預期的：直通 Gateway 的 I3 由「失敗（宣告的形狀）」變「不適用」（328 個），直通動作的 A 由 `link_shots` 0＝0 變「不適用」；新判定在直通的跑次都通過（I1a 有 12 個 undetermined，和 I1 同樣卡在前提），直通前的 Gateway（含 pv6 的 282 個）一律不適用；IG、I0、I1、I2 等既有判定在 669 個目錄都和舊版相同。新版沒有任何目錄不通過。
  - 重新凍結：`pvp-v71-edge-analyzers-20261018/frozen-pv7-ea.sha256` `77cb956b…`（＝`frozen-pv7-d117.sha256` 只更新 `ingress_evidence.py` `567198d6…` 與 `test_ingress_evidence.py` 兩行），給之後的 L2（`edge-rate-limit`）用。
  - 突變：分析器與跨程序核對上的 35 個（既有 26 個中 2 個更新 find 字串，新增 9 個 `v71ea-*`）全部殺死；`v71ea-link-actions-term-missing` 第一次是錯誤訊息不符（被 I1a 的測試抓到，但期望的測試因為拿分析器的常數當迴圈清單而跟著少測），測試改成字面值後殺死。
  - 驗收（L1，本機）：CTest 72／72、preset 外 4／4（`ingress_cross` 在新的必要檢查下通過、不再需要宣告例外）。

- 2026-10-18：`edge-contract`（檔位 medium、守衛 high；估 4 h）。條文做到草稿與守衛，停下等使用者核准（使用者：「開始 edge-contract。Design Invariants 的條文做到草稿與守衛後停下等我核准。」）。
  - 條文草稿：`network-architecture.zh-Hant.md` §7 `Design Invariants` 之下新增「Gateway：無狀態邊緣」（標「草稿，待使用者核准」）：四條原則（只持有軟狀態、不持有 Match 決策狀態的副本、每則訊息獨立處理、丟棄有界並回報）、允許的 11 個類別（名稱和守衛一致）、禁止項、守衛與變更規則、Replacement Criteria、目前的已知項目（K1、`queuedControl.at`）。同一節的「Gateway 的設計意圖與漂移」狀態改成「edge 系列已改回無狀態邊緣」。protocol-v7 §2a 加上守衛與條文的位置。
  - 守衛 `apps/object_fps_pvp/gateway/state_whitelist_test.go`（三個測試）：
    - reflect 走訪 Gateway 套件每個 struct 型別（26 個；用 `go/ast` 確認清單不漏）的欄位，含指標、map 的鍵與元素、slice、array、channel 與匿名 struct；介面欄位要明列它持有什麼（目前只有 `Server.listener`，`net.Listener`）。
    - `go/ast` 掃描 `gateway` 與 `gateway/adapter` 的套件變數（27 個）。
    - `go/ast` 掃描以 `go` 啟動的函式與閉包裡的區域 map、slice（目前 2 個：`receiveUDP` 的讀取緩衝、`sendUDP` 每個 snapshot 的玩家清單，都是單次處理用）。
    - 分類表沒有「Match 決策狀態」類別；未分類、分類表多出不存在的項目、或類別不在允許清單，測試都失敗。
  - 盤點時發現：`queuedControl.at` 入列時寫入、沒有任何地方讀取（死欄位），沒有順手刪。
  - 突變：新增 5 個（`v71ec-*`：runtime link 加 `lastResolved` 副本、保留加匿名 struct 的 map、套件變數、writer goroutine 的區域 map、`any` 欄位），全部殺死；會加欄位或區域 map 的既有 6 個突變重跑，仍以原本的訊息殺死。
  - 驗收（L1，本機）：`go test -race`、CTest 72／72、preset 外 4／4。
  - 等使用者決定（條文核准時一起）：
    1. 條文本身（四條原則、11 個類別、Replacement Criteria）。
    2. N3：佇列項目帶著的診斷身分（`queuedMessage.event`、`resultSlot.event`）和 D105「單次呼叫的脈絡不得跨資料包保留」字面衝突；草稿列為獨立的允許類別「diagnostics carried with a queue item」。
    3. `queuedControl.at` 要不要刪。
  - D120（使用者核准）：條文照目前的內容（禁止項先改寫成「Gateway 不得…（負責方）」，protocol-v7 §2a 同步），N3 照草稿；`queuedControl.at` 刪除（白名單同步移除）。

- 2026-10-18：`edge-rate-limit` 開始（D119；檔位 high；估 8～12 h，含 L2）。第一個關卡「要不要採用方案 (a)」會改通用層與契約，先做重新測試與評估，停下等使用者決定。
  - 重新測試 K1（兩個失敗的測試，Red，尚未提交）：
    - 產品層 `apps/object_fps_pvp/gateway/rate_limit_replay_test.go`：把 r4-B 那次 player 1 的真實到達序列（`testdata/k1-arrivals.txt`，1089 個，由 relay 的轉送時間與停頓區間產生）重放給 `session.Admit`，期望 0 拒收；目前拒收 **53**，和那次 Gateway 的計數相同。
    - 通用層 `services/gyo_gateway/session/session_test.go`：合成資料（每 11 ms 一個、讀取端停頓 1 秒），期望 0 拒收；目前拒收 62。另加洪水上限測試（任何長度 T 的區間放行 ≤120T＋120），現行與方案 (a) 都成立。
  - 評估（模擬，未改程式）：
    | 情形 | 固定窗口（現行）拒收 | token bucket（每秒補 120、容量 120）拒收 |
    |---|---|---|
    | 重放 r4-B player 1（停頓 1 秒） | 53 | 0 |
    | 合成：91／秒、停頓 1 秒 | 62 | 0 |
    | 停頓 1.5 秒 | 108 | 18 |
    | 停頓 2 秒 | 153 | 63 |
    | 洪水 300／秒 × 3 秒，放行數 | 360 | 479（上限 480 之內） |
    - 對照白名單（D120）：token bucket 的狀態（剩餘額度、上次補充時刻）留在通用層 `session.Session`，屬於 connection and session 類（速率），可以重建、不參與遊戲判定、不是 Match 狀態；產品層 Gateway 不新增欄位。符合條文。
    - 上限：長期速率不變（120／秒）；任何 1 秒滑動區間最多 240，和固定窗口跨邊界時相同。差別是最初的 120 額度可以一次用掉（洪水 3 秒多放行 119）。
    - 限制：容量 120 約等於開槍玩家 1.3 秒份的封包；Gateway 停頓超過約 1.3 秒時仍會拒收（2 秒停頓 63 個）。驗收裡最長的 Gateway 停頓是 1 秒。
    - 影響範圍：通用層 `services/gyo_gateway/session` 的使用者只有 `object_fps_pvp`；契約條文 `network-architecture.zh-Hant.md:950`（fixed-window cap 120 packets/s）要改；產品層的診斷 `max_session_window_packets`（`reservation.packetWindow／packetCount`，固定 1 秒窗口的計數）意義不再等於限速器的窗口，只記錄。
  - 替代做法：(a′) 容量加大到 240（停頓約 2.6 秒內不拒收；但任何 1 秒滑動區間最多 360，比現行寬）；(b) 固定窗口提高上限（同樣放寬洪水上限，停頓更長時仍會拒收）；(c) 用 Client 送出時刻計速（要上 wire，protocol-v7 §10 第 3 項，不在 pv7）；(d) 偵測 Gateway 自己的停頓並豁免（Gateway 端的特例，違反 Replacement Criteria 的精神）。

- 2026-10-18：`edge-rate-limit` 的限速修正（D121）。
  - 通用層 `services/gyo_gateway/session/session.go`：固定 1 秒窗口改成 token bucket（每秒補 120、容量 120），以 GCRA（虛擬排程時刻）整數時間實作；被拒的封包不扣額度，閒置的額度不超過容量。`Session` 的 `window`、`packets` 兩個欄位換成一個 `schedule`。
  - Red → Green：產品層真實重放 53 → 0、通用層合成 62 → 0；既有的限速測試（同一瞬間第 121 個被拒、下一秒恢復、被拒的 payload 仍扣額度）不變；新增「閒置 10 秒後瞬間洪水只放行 120」。
  - 契約：`network-architecture.zh-Hant.md` 的 Gateway 圖（998 行，原 950 行）與第 08c 批的說明（1014 行）改成 token bucket，K1 的已知項目改成已處理。
  - 突變：新增 4 個（`v71rl-*`：沒有 burst、閒置額度無上限、被拒仍扣額度、容量 61），全部殺死；新增 check `gateway_session`（通用層的 `go test ./session/...`）。突變目錄 402 個，沒有過時的 find。
  - 驗收（L1，本機）：通用層與產品層 `go test -race`、CTest 72／72、preset 外 4／4。通用層的測試在 CI 由 `go_checks.py` 對登記的 Go 模組執行。
  - Architecture Delta：engine owner 的通用層 `services/gyo_gateway/session` 的限速算法改變（行為：Gateway 自己停頓後不再把緩衝的合法封包當成超速）；上限的數字與介面不變，使用者只有 `object_fps_pvp`。依據是產品的 L2 證據（K1），但算法本身與產品無關。
  - L2 的兩棵樹：B'＝`1802f08`（`f79bb10`＋只套用這次的限速修正；本機分支 `claude/pvp-v71-l2-b-ratelimit`，不推送；`../GYO-Engine-rl-before`），A'＝`7bb1f84`（`../GYO-Engine-ap-after`）。宣告 [edge-rate-limit-l2.md](edge-rate-limit-l2.md) 第 1 版草稿（取代第 4 版；新增 K1a「gateway-1000ms 沒有限速丟棄」、K1b「Gateway 恢復後沒有 starvation 重設」，R6 改用 `pvp-v71-ingress-3`）。核對實測值時更正兩處：修正前每跑次 player 1 的限速丟棄合計 48～53（不只輸入），backpressure 也有 0～2。
  - 宣告第 1 版經 D45 檢查（1 位 xhigh，約 12 分）：必須修正 2、應修正 5、記錄 6，改進第 2 版（第 7 節）。M1 要使用者決定：凍結分析器 `gameplay_evidence.py:210`、`gameplay_soak_evidence.py:377`、`action_probe.py:353` 要求 Gateway 的固定 1 秒窗口計數 `max_session_window_packets ≤120`；token bucket 下停頓後同一秒會放行約 170，兩棵樹都會在第一個 gateway-1000ms 停下。來歷：同一處的「限速拒收＝0」在 gateway 模式刻意不判（K1 當年被當成預期行為接受）。建議 (a) 改用限速器自己的拒收計數＝0（gateway 模式也判）、窗口計數只記錄。A' 已建置；B' 建置中。
  - D122 的修改：`gameplay_evidence.py` 新增 `gateway_limiter_problems()`（拒收＝0，所有模式；窗口計數只記錄），`gameplay_soak_evidence.py` 共用它；`action_probe.py` 的檢查改成所有模式都要拒收＝0、`rate_limit_acceptance_policy` 的說明同步。測試：`test_gameplay_evidence.py` 加 1 個、`test_action_evidence.py` 改 1 個並加 1 個（窗口 170 但沒有拒收時，限速檢查不出錯）。突變新增 4 個（拒收被忽略、窗口檢查回來 ×2、gateway 模式又被豁免），全部殺死；新增 check `gameplay_evidence`、`action_evidence`。重新凍結：`build/target/_build/test/logs/pvp-v71-edge-rate-limit-20261018/frozen-pv7-rl.sha256` `09b5579b…`（＝`frozen-pv7-ea.sha256` 只更新 7 個檔案：上面 5 個，加上把 `action_probe.py` 的雜湊寫死在程式裡的 `run_client_delay.py`、`run_short_stall.py`）。**經過**：第一次提交（`c82ecc5`）時 CTest 有 1 項失敗（`client_delay_runner`：這兩個 runner 繼承 `action_probe` 的 relay，要求它的雜湊等於寫死的值），指令串沒有在失敗時中止，提交與推送照樣做了。修正：兩個 runner 的常數更新成新雜湊（`action_probe.py` 只改了限速判定，relay 沒動），凍結清單補上它們；之後 CTest 72／72、preset 外 4／4。soak 的呼叫點沒有突變覆蓋（`analyze_soak` 沒有可餵 Gateway 計數的測試 fixture），由共用函式的測試與 L2 的 soak 項目守。
  - L2 的工具與乾跑（`build/target/_build/test/logs/pvp-v71-edge-rate-limit-l2-prep-20261018/`）：A' 移到 `ed3481c` 重建（`vcs.modified=false`），宣告的 A' commit 由 `7bb1f84` 改成 `ed3481c`。judge 由第 4 版的工具改：分析器期望表逐個 `gateway*.log` 判、I0 一律判、R6a／R6b 刪除、R6c 用自己的前提、新增 K1a、K1b、P6、P7。B' 開發跑次 4 個（gateway-1000ms ×2、clean-60、backpressure）全部 P、無停止：K1a 0 拒收；K1b 每位玩家停頓中恰好 1 次 starvation、恢復後 0 次；Expired 3、2；P7（固定窗口計數）gateway-1000ms 159、159，backpressure 的 Gateway 層 121（預測是約 170 與 ≤120，只記錄，預測不改）。用新 judge 重判之前兩次 session 的 131 項：停止只有修正前的 K1 與第 3 版的 rapid 收尾競態。乾跑 96 列、driver 10 項、模擬 session 5 個全部符合預期。Client 不變已核對（C++ 只改 Match 的 `IpcHost.cpp`）。宣告第 3 版：[edge-rate-limit-l2.md](edge-rate-limit-l2.md) 第 8 節。
  - L1（`ed3481c`，run 39102188643）：windows-x64、linux-x64、macos-arm64 通過；**macos-x64 不通過**，`object_fps_pvp.cpu` 的「PvP a delayed first rendered frame does not establish a persistent bootstrap backlog」（`MovementRecoveryTests.cpp:456`，product 路徑 fps 30、RTT 0、bootstrap gap 108、worker 4000、authority 6000：`unexpectedRejections` 1）。`ed3481c` 對 `c82ecc5` 只改 Python；同樣的 C++ 在 `7bb1f84`（run 39100385472）的 macos-x64 通過，兩次的 runner 映像相同（macos-15-arm64 上以 Rosetta 2 跑 x64）。測試用注入的時鐘與固定種子，原則上是決定性的；本機（Intel x64）單案例 200 次、整個執行檔 20 次都 0 失敗。原因未切分（實作的非決定性、測試，或 Rosetta 環境），已只重跑失敗的 job；原本的失敗保留記錄（§14.1）。重跑（attempt 2）macos-x64 與 CI gate 通過：同一份程式碼結果會變，原因仍未切分，列入未結事項 `N-movement-recovery-flake`。

- 2026-10-18：`edge-rate-limit` 的 L2 session（D123；`build/target/_build/test/logs/pvp-v71-edge-rate-limit-l2-20261018/`）。跑前確認 Chrome 沒有在跑、暫停 PR #78 的 Auto-fix；跑後重新打開。168 項全部跑完、沒有停止、完整性問題 0，判定通過（L1～L4 成立）。K1a 168 項全部 0 拒收；K1b gateway-1000ms 10 跑次全部成立（停頓中每位玩家恰好 1 次 starvation，恢復後 0 次）。L2 的 gateway-1000ms Expired B [2,3,3,3,3]、A [2,3,2,3,3]；P7 gateway-1000ms 156～160、backpressure 的 Gateway 層 120～122（只記錄）。受干擾 2 項。結果寫在宣告第 9 節。`edge-actions-passthrough` 與 `edge-rate-limit` 完成，edge 系列完成；README 同步。

- 2026-10-18：`cause-records` 開始（D124；檔位 high；估 8～9 h）。範圍與測試清單照 protocol-v7 §5、§2、§9 與 pv7-design 的測試清單。

- 2026-10-18：`cause-records` 的實作（D124、D125；19:00 開始，19:52 本機驗收完成，約 1 h，估 8～9 h 之內）。
  - proto：runtime `PlayerInput` 6、7、`ActionBatch` 4、5（dwell、Client 的 GYOP 序號），`WorldSnapshot` 4（`match_hold_us`），`ActionResults` 4、5（`refused_batches`、`last_refusal`），`PlayerLeave` 2（`LeaveReason`）；client `WorldSnapshot` 4、5，`ActionResults` 3、4；`ActionRefusal` 兩份同號。Go 產生碼以 build 的 protoc 36.2 與 module 固定的 protoc-gen-go v1.36.11 重新產生（schema 不變時重新產生無差異，已先確認）。
  - Gateway：`stampDwell`（writer 開始 Marshal 時）、`dwellMicros`（與 DropSample 共用的唯一定義）、交給 link 時填 Client 序號；`snapshotFrame` 改存解碼後的 snapshot 與讀取時刻，送給每位玩家前填 `gateway_hold_us` 重新編碼，讀取端的大小檢查以 hold 上限計算；adapter 照抄 `match_hold_us` 與拒收兩欄；三條 Leave 路徑帶原因。白名單測試：`linkWrite.received`、`snapshotFrame.read`（diagnostics carried with a queue item）、`snapshotFrame.snapshot`（queue），`snapshotFrame.payload` 刪除。network-architecture §7 的類別說明照 D125 改寫（另加上 snapshot 的讀取時刻與 hold，同屬 D125 核准的範圍）。
  - Match：Host 的 `SubmitActionBatch` 以輸出參數交出拒收原因、`TakeSnapshot` 帶回發布時刻、`Now()` 提供 Host 的時鐘（判定不變；呼叫點不需改）；IpcHost 在選中 snapshot 時才編碼並填 `match_hold_us`，動作 lane 累計拒收與最後原因，`gw_transport` 行族（每 10 秒與結束時），Leave 行尾加 `reason=` 與違規行。Client：`ClientCauseRecords`（`NetworkStatistics`），每個統計窗口一行 cause records。日誌形狀寫進 protocol-v7 §5「實作與紀錄」。
  - 最大的 4 人 snapshot datagram 1055 → 1065 B（兩段停留在 10 s 上限時各 5 B；pv7-design 估的是 18 B）。Go 的 capacity 測試與 worker probe（`worker_main.cpp`，不在凍結清單）同步。
  - 測試：Go 新增 `cause_records_test.go`（dwell 一對一與序號、10 s 上限、同一輪裡前一則寫出阻塞 500 ms 時後一則的 dwell、每位玩家各自的 hold 與中途取代、大小上限、D32 的符號守衛涵蓋 session 套件與 admission／逾時／路由函式）、adapter 的照抄與兩份列舉一致；既有 3 個 Leave 測試加原因檢查；2 個 snapshot 測試的 3 處位元組比對改成忽略各自的 hold（§14.4：因為每位玩家的位元組本來就不同）。C++：差分測試（極端 dwell、序號、所有 Leave 原因：決定、join 結果與最後的 snapshot 逐位元相同）、Match hold（每份都有、在 Host 時鐘上 <1 s、至少一份 ≥1 tick）、拒收累計與原因、Leave 原因與違規、序號缺口與 dwell 統計、拒收名稱與 Host 一致、Host 交出 Resetting 與發布時刻、Client 行的格式。D32 原始碼守衛加入這批的符號。
  - 突變：新增 18 個（`v71cr-*`，Gateway 8、Match 9、Client 1）全部殺死；7 個既有突變的 find 跟著程式碼更新（意圖不變）。`v71cr-match-hold-at-take` 與對應測試的「至少一份 ≥1 tick」依賴 IPC 執行緒在測試推進假時鐘之後才選框（實務上幾乎必然，理論上非決定性）。
  - 沒有測到的：讀取端「snapshot 超過 datagram 上限」在合法的 4 人 snapshot 下到不了（最大 1065 B），hold 上限計入大小檢查的程式沒有突變。
  - 驗收（L1，本機）：CTest 72／72、Go `-race`、突變 18／18。

- 2026-10-18：`hitbox-fields` 開始（D126；20:0x）。範圍照 `protocol-v7.zh-Hant.md` §6「欄位」「影子量測」、§7、§8 與 `pv7-design.md` 的測試清單。影子紀錄照 `match-ingress.jsonl` 的先例，另開一個由 `--movement-trace` 推出路徑、有版本號的明細檔（不在 `commands.jsonl` 加事件種類）。

- 2026-10-18：`hitbox-fields` 的實作（D126；19:58 開始，20:35 本機驗收完成，估 8 h 之內）。
  - proto（兩份同號）：`ShotRequest` 7、8、9（E、S、`view_tick_q16`），`ShotDecision.hit_part` 11、`CombatState.last_hit_part` 14、`CombatRules.maximum_shot_damage` 9，`HitPart` 列舉。Go 產生碼重新產生。
  - Domain：`ShotRequest` 加 E、S、view tick（`operator==` 是 default，自動包含）；`ShotDecision.hitPart`、`CombatState.lastHitPart`、`CombatRules.maximumShotDamage`（25）；`ValidDamageRecord` 擴成四欄；共用的 `ValidDecisionHitPart`、`ValidShotDamageBounds`。PvpMatch 命中玩家時填 Body（裁決與受擊紀錄），重生時整個 `CombatState` 重建所以一起歸零。權威 digest 是逐欄列出的，新欄位不進 digest。
  - Gateway（adapter）：E、S、view tick 照抄不驗證並進 `EqualShot`；`hit_part` 的規則、damage 上限改成 `maximum_shot_damage`、`ValidRules` 的三條規則、`last_hit_part` 進受擊紀錄檢查；`EqualDecision` 含 `hit_part`。Gateway 沒有新增狀態欄位。
  - Client：`ReadRules`、`AcceptResults`、snapshot 解碼帶新欄位與規則；送出時帶 E、S、view tick（`ActionContext`，取自同一份 `ClientPresented`）；view tick 用 `SnapshotTimeline::GlobalCursor()`，每幀抽樣遠端後換算，Presented 時記下（`PvpApplication`）。
  - Match：IpcHost 編解碼新欄位（HitPart 的值由 `static_assert` 守住）；Host 在交接時留請求副本、裁決的 Tick 寫影子紀錄（`MatchShotTrace.hpp` 的 `ShotShadowTrack`），`match_main` 寫 `match-shots.jsonl`（格式寫在 `protocol-v7.zh-Hant.md` §6「影子量測」）。紀錄性的 view tick 檢查與 view_lag 在影子紀錄裡，不加在 Match 的 ingress 統計行（凍結的 ingress 分析器對那一行的鍵順序嚴格）。
  - 大小：4 人 snapshot 的最大 datagram 1065 → 1073 B（`last_hit_part` 為 HEAD 時每人 2 B，等於 `protocol-v7.zh-Hant.md` §8 的契約最大值）；ActionBatch、ActionResults 的最大值測試帶入新欄位的最大值。
  - 權威不變：`compare_authority_trees.py`（基準 `../GYO-Engine-ap-after` 的 `ed3481c`，含 cause-records 之前），scale 1、3 各 35 個情境 0 差異（`build/target/_build/test/logs/pvp-v71-hitbox-fields-20261018/digest/`）；golden 的 CTest `object_fps_pvp.authority_digest` 通過。
  - 測試：Go 的 HitPart 正反例、三條規則與 damage 上限、E／S／view tick 照抄與 `EqualShot`；受擊紀錄四欄（Go 與 C++）；Match 只產生 Body、受擊紀錄的 Body 與重生歸零；`operator==`；路徑分類、view tick 檢查、紀錄行與路徑；Host 的差分（E、S、view tick 不同時裁決與世界逐欄相同、invalid 計數）、view_lag、冷卻保留量；IPC 的 Ready 與 E、S、view tick 到 Host；Client 的 `ActionContext` 與全域游標（遠端換段的案例）。D32 原始碼守衛加一條：判定程式不讀 S 與 view tick（E 與玩家狀態同名，靠差分測試與 digest）。既有測試的資料補上新欄位：Go 的 `testRules`、最大值測試、受擊紀錄、`match_ingress_cli_check.cmake`（Match 在 trace 旁多寫 `-shots.jsonl`）。
  - 突變：新增 22 個（`v71hf-*`，Gateway 7、Match 與 Domain 13、Client 2）全部殺死。
  - **等你決定**：契約寫「Host 對每個裁決記錄……命令 S 的來源（Actual／Held／Neutral）與命令 S 是否之後才到」。這兩項只有在 T(S) 解析之後才知道；Host 的 `TrackSlack` 只知道「有沒有收據」，分不出 Held 與 Neutral（那是 PvpMatch 決定的）。目前的做法：影子紀錄記 A 側的欄位，來源與晚到由 (player, E, life, S) 離線對上 `match-commands.jsonl` 的 resolved 事件（帶 `source`）與 `match-ingress.jsonl` 的替代紀錄（第一份晚到的時刻）。(a) 維持這個做法，改寫契約的那一句（PvpMatch 不變；`pv7-checkpoint` 的分析器要做對接）；(b) 讓 PvpMatch 對外提供每個 Tick 的解析來源，Host 延後到 T(S) 才完成紀錄（動到判定檔的介面，約 1.5～2 h）。

- 2026-10-18：D128，`match-shots.jsonl` 每筆加 `time_ns`。Host 每個 Tick 讀一次 `MovementTraceNowNs()` 交給 `ShotShadowTrack::Decided`，同一 Tick 的紀錄共用；格式仍是版本 1（還沒有工具讀它）。測試：紀錄行的格式、Host 的時刻落在該次 Step 前後讀到的時鐘之間且依序不倒退。突變新增 2 個（`v71hf-shadow-time-unset`、`v71hf-shadow-line-time-missing`）都殺死。本機 CTest 72／72。

- 2026-10-18：`pv7-checkpoint` 開始（D129）。先收集材料：v7 基準（16-L2、D44）、三個 edge 批 session 的實測差異帶、凍結 pv7 工具的涵蓋範圍、影子資料需要的情境（`pv7-design.md:119`）。
  - 收集結果（唯讀子代理）：16-L2（`pvp-v7-16-l2-20261015`、`-l2b-`）171 項，判定 L2-1～L2-6；`judgement.json` 的遊戲案例沒有逐案指標，可由 `runs/*/result.json` 重算（clean-60 Actual P95 37.81 ms 等）。三個 edge 批的差異帶在各 session 的 `judgement.json`（edge-input 的 J2 替代率、J4；edge-results 與 edge-rate-limit 的 `bands_section_9`：d p95、to_tick p95、Expired、match_received）。
  - 照 README 的方法做不到的地方：edge 指標在 16-L2 沒有基準值（16-L2 的原始檔是 pv6，凍結 pv7 分析器不讀）；三個帶在不同 session、不同主機狀態下量，直接相加會膨脹；K1 修正在 B′、A′ 兩棵都有，不在任何帶裡；v7 到 edge 前的 8 個「行為不變」批沒有帶；很多項目（clean-30／144、upstream、downstream、GUI 等）沒有帶。凍結 pv7 工具涵蓋 16-L2 的全部項目（`frozen-pv7-rl.sha256` 54 個檔對現在的樹全部相符）。
  - 影子資料：`run_gameplay.py`、`backpressure_probe.py`、`run_network.py`、`run_quad.py`、`run_gameplay_soak.py`、`run_timing.py`、`run_short_stall.py` 會傳 `--movement-trace`（因此會寫 `match-shots.jsonl`）；連點的 `run_action_short.py`、`run_player_short.py`、`run_gameplay_gui.py` 不傳。upstream 有 `upstream-250ms`／`-1000ms` 案例，Gateway→Match 延遲注入有 `run_short_stall.py --delay-ms`。
  - 更正 D130 的 (i)：probe 送射擊時不帶 `ShotContext`（`action_main.cpp:186`、`network_main.cpp:182`、`quad_main.cpp:202`），E＝S＝0，影子紀錄全部走 epoch 路徑，不能拿來決定 K 與 S。會填 E、S 的只有遊戲本體（`PvpApplication`：GUI timing、連點、player 短測），其中只有 `run_timing.py` 傳 `--movement-trace`。等使用者重新決定影子資料的來源。
  - D131 的實作：probe 照 Client 規則填 E、S、view tick，直接用 Client 的 `ActionContext`／`ViewTickQ16`（`ClientConnection::SubmitShot` 加預設為空的 `ShotContext` 參數）。改了 `gameplay_action.hpp`（`run_gameplay.py` 的預排動作；每位玩家這一幀的 `presented` 與遠端取樣的 view tick）、`action_main.cpp`、`quad_main.cpp`（bot 不呈現遠端，view tick 0）。network probe 只在模擬角色建立之前的清場分支開槍，照規則 E＝S＝0，不改。`run_action_short.py` 的 Match 命令列加 `--movement-trace`（`match_command()`，listen 位址留在呼叫處，凍結的 `test_service_startup.py` 用 AST 檢查它）。重新凍結：`pvp-v71-pv7-checkpoint-20261018/frozen-pv7-cp.sha256` `f036b2af…`（＝`frozen-pv7-rl.sha256` 只更新 `run_action_short.py` 與 `test_run_action_short.py`）。突變新增 1 個（`v71cp-rapid-without-trace`）殺死；probe 的 C++ 沒有單元測試，改以兩次工具驗證的開發跑次確認（只讀影子紀錄，不看回歸指標）：`run_gameplay.py` clean-60 43 筆裡 on_time 41、life 2（跨生命），S−L 都是 1，view 延遲約 1.7～1.8 Tick；`run_quad.py` clean-60 1 輪 54 筆全部 on_time。改之前同一個 clean-60 是 43 筆全部 epoch（E＝S＝0）。本機 CTest 72／72。
  - 宣告第 1 版草稿：[pv7-checkpoint-l2.md](pv7-checkpoint-l2.md)（兩棵樹各 106 項，加只在 A 的延遲注入 6 項，共 218 項，約 2.2 h；停止條件 R1～R4、K1a、H1～H3（D127 的兩項：對接鍵加時間順序與窗口、on_time 全部對得到）；結束時判定 C1～C5；影子資料只記錄）。D45（1 位 xhigh）進行中。
  - D45（1 位 xhigh，約 20 分）：MUST-FIX 4（C3 的 99% 套到故障情境一定不成立；C4、C5 的合法射擊 P95 依 v7 D46⑦ 不能跨建置比較；H2 要求每筆都對到之後的事件，late／life 會讓第一個 clean-60 就停；`match.log` 的 `ipc closed` 沒有時間戳，不能當 session 邊界）、SHOULD-FIX 9、NOTE 9。第 2 版逐項修正（對應表在宣告第 9 節），4 項待使用者決定。
  - 工具（2026-10-18 22:50，D132 之後）：`build/target/_build/test/logs/pvp-v71-pv7-checkpoint-l2-prep-20261018/` 的 `judge_cp.py`（排程 232 項、第 4 節逐項判定、第 5 節結束判定）、`run_cp.py`（driver）、`shadow_cp.py`（T1、H1～H3、第 6 節）、`timing_v7_a.py`（16-L2b 的 v7 HUD 規則綁 A 的模組，另開行程）、`corpus.py`（B 的語料檢查）。A 的主 checkout 在 HEAD `12ebe2b` 重建：Gateway 的建置資訊原本是 `12e6faf`、modified＝true（D131 提交前建的），重建後 `12ebe2b`、false；`artifacts-A.sha256` `ab6005fc…`（41 行，arena 31 檔和 v7final 的 16-L1 逐檔相同）。預檢通過。
  - B 的語料檢查：16-L2（168 項）、16-L2b（4 項）、C2 的 c2-after 連點，共 183 個 B 跑次當成 B 項目重判：182 個分類和當時的紀錄相同、0 個停止；唯一不同是 16-L2 的 `timing-gui-short`（凍結 F，v7 HUD 規則 P，符合 D83）。
  - A 的開發跑次（D132，只看分類、鍵與 T1、H1～H3）發現一個工具錯誤：`upstream-1000ms` 有 1 筆 on_time 對不到事件，原因是 `reset` 事件記的是**新的** epoch（這筆在記錄後 33 ms 遇到 backlog 重設，epoch 2→3，在 T(S) 之前），工具卻用舊的 E 去找。已改成「同一 session、同一玩家、紀錄之後到窗口結束之間有 epoch 大於 E 的重設」，夾具改成真實的形狀。這正是 D45 預告的「on_time 之後接 backlog 重設」。另外 `duplicate-reorder-conflict` 的 Match 動作批拒收 6（conflict，情境本身的設計；R8 對第一次跑的情境只記錄）。
  - 誤看：查 `result.json` 的鍵時，不小心印出了 A 開發跑次 clean-60 的合法射擊 P95（Match 13.8、Client 32.7 ms）。C4、C5 依 D132 只記錄、不判，不影響任何門檻；之後的工具只輸出分類、停止與旗標，不讀第 5 節的指標。
  - 開發跑次完成（2026-10-18 23:30）：A 45 項（25 案、背壓、網路兩種、長測短模式、quad 5 輪、延遲注入兩個 T、G 7 項、連點 6 格）、B 6 項，全部分類 P（延遲注入為 R）、0 停止。影子對接：on_time 全部對上或有解釋（有解釋的 2 筆：`upstream-1000ms` 的 backlog 重設、`rapid-interval12-144` 最後一發時 trace 先結束）。中途改 `shadow_cp.py` 卻沒同步 `judge_cp.py` 釘的雜湊，19 項被拒絕而沒跑，同步後補跑（教訓：開發跑次進行中不改被釘住的檔）。
  - 乾跑（`dryrun.py`）：逐項 89 列、突變 11 個全部殺死、driver 8 項、模擬 session 7 種、第 5 節合成資料 11 列，全部如預期。第一次乾跑的失敗都是夾具或預期寫錯（B 夾具的目錄結構不合 `r_record`、clean 的夾具插入重設本該停、合成數字的四捨五入、模擬 session 選錯項目），另外補了 H2 主機停頓的夾具（原本「H2 窗口沒有 1 秒寬限」的突變沒殺到）與 C3 恰好等於下限時的浮點容差 1e-9（宣告寫的是 ≤）。
  - 宣告第 3 版：把工具的機械規則寫回第 2、4、5 節（B 的已知限制＝D44 型、R8 判的 11 個情境、T1 的目錄、H3 的重設解釋、C1／C3 的情境清單、C3 的 N、C6 的「A 的每跑次分布等於 B 某一跑次」），第 10 節是雜湊、開發跑次與乾跑。時間以開發跑次更正為 148.8 分加閒置門檻。
- 2026-10-19 00:02～02:24：`pv7-checkpoint` 的 L2 session（D133；`build/target/_build/test/logs/pvp-v71-pv7-checkpoint-l2-20261019/`，141.9 分，估 148.8 分）。232／232 完成、0 停止、完整性問題 0；受干擾 2 項（`clean-60-r1-A` 的前 sleeper 48.9 ms，當時佔 CPU 的是 Claude App 的 Renderer＝我在 session 中呼叫工具造成，之後停掉監看不再呼叫；`network-r2-B`）。R8 的旗標：`duplicate-reorder-conflict` 的 A 三次都有 conflict 拒收 6（情境設計，只記錄）。B 的已知限制 0。
  - 正式判定（`judgement.json`）：`passed`＝false，C1、C2 為 None、C3 成立。原因是判定工具的標籤錯誤（見現況第 1 項），乾跑沒有抓到：模擬 session 只檢查完整性與停止，第 5 節的合成測試直接給 records，沒有經過 `schedule()` 的兩棵樹。診斷（`judgement-relabelled.json`，`relabel_diagnostic.py` 包住 `schedule()`、凍結檔不改）：C1、C2、C3 全部成立；19 案與長測短模式的 P95「A 變慢」最大 0.282 ms（drain-stall）、P50 最大 0.287 ms（host-ipc-250ms），quad clean-60 0.203／0.065 ms，下限都是 1 ms。C3：gateway-1000ms 的 Actual 比例 B 0.966、A 0.987（K1 修正，P2），其他兩棵相同。
  - 預測：P1（C1、C2 成立）、P2（B 的 gateway-1000ms 三次都在恢復後 starvation，A 三次都沒有）、P3（clean 退回率 0）、P4（141.9 分，差 5%）都沒有被推翻。
  - 只記錄：C4／C5 合法射擊 P95（例：clean-60 Match B 11.6～15.8、A 14.5～15.3 ms；Client B 17.5～19.4、A 19.3～33.8 ms；依 D46⑦ 不能跨建置比較）；TimerBaseline 兩棵都是 4 ms 狀態。
  - 影子資料（第 6 節，給 `a1-shot-step`）：見現況第 3 項；網路兩種的 probe 照規則 E＝S＝0，全部走 epoch 路徑；有解釋的未對上 3 筆（都在 `upstream-1000ms`，backlog 重設）。
- 2026-10-19：`a1-shot-step` 開始（D136：K＝min(2, cooldownTicks)、S＝N）。使用者問到資料驅動與連射武器（P90、MP5 約 4 Tick 冷卻）：固定的 K 會在冷卻比 K 短的武器上違反 0≤K≤cooldownTicks，所以改成跟著武器的冷卻走。另從 session 補量：late 的晚到量 d＝L(A)−S 為 0:23、1:26、2:2、3:10、4:3、5:11、6:1、9:1、11:3、14:8、15:1、19:3、20:3、28:3（98 筆）；連續兩發的間隔，故障情境的 probe 多為 15 Tick（比冷卻多 5），壓著冷卻邊緣的只有 clean 下的連點，所以「退回後下一發落在冷卻內」0 次只反映測試設計。
  - 計畫與事前宣告第 1 版草稿：[a1-shot-step.md](a1-shot-step.md)。讀程式時確認：權威 digest 的射擊沒有填 E、S（`AuthorityDigest.cpp:314`），E＝0 走 pv6 的到達判定；凍結分析器盤點（Expired 例外的方向安全、合法性重播一致、GUI 裁決抵達 P95 實測約 33 ms 對 150 ms 門檻）；action short 的冷卻注入（E＝0）靠「同玩家依 ActionId」仍在第一發之後判定。
  - D45（1 位 xhigh，約 14 分）：MUST-FIX 2（M1：冷卻保留不分路徑，`combat-magazine/corridor` 每 4 Tick 開一槍、冷卻 10，A1 一定有差，「現有 digest 0 差異」不成立；M2：L2 的 Part I 拿掉了動作路徑的故障情境，R8 的 11 個情境與重送去重、life／epoch 退回都沒人跑）、SHOULD-FIX 7（A1-1 不是契約不變式、C9 範圍、A1-2 把 D 與 J 混在一起、新情境缺 6 項、盤點漏 `cooldownRemainingSeconds`／兩個延遲門檻／`quad_evidence.py:33`、「K 固定 2」的突變殺不到、B 的二進位會被 A' 的建置覆蓋）、NOTE 7。檢查者用 A1 規則重放 checkpoint 的影子資料（`sim_a1.py`，近似）：故障情境 A1 造成的 Cooldown 0，保留只有 upstream-1000ms 3 次（gap 2）；連點在 FireGate 時代的送出時刻下 rapid-edge-60 會保留 28 次、edge-144 19 次。我核對了 M1（`AuthorityDigest.cpp:418`）。第 2 版逐項修正（宣告第 9 節）。
- 2026-10-19：`a1-shot-step` 實作（D137；細節在 [a1-shot-step.md](a1-shot-step.md) 第 10 節）。`PvpMatch` 改照 A1 判定（到期、保留一次、同玩家依 ActionId、參考年齡在 A 量）、`CombatRules` 參數；影子紀錄版本 2；`ShotGate`（序號閘門）取代 FireGate。本機：`cpu|shader` 範圍的 CTest 只有 digest 依宣告不符；`combat-magazine/corridor` 的逐筆判定和手算表完全相同後重產該筆 golden（舊檔保留）；A1 的突變 19 個全部殺死，FireGate 的 10 個退役。發現：「K＝min(2, 冷卻)」與「K＝2」在行為上等價（gap 不會超過冷卻），D45 S6 的突變是等價突變；`CombatRules` 參數改用來測冷卻 2、4 與一發致死的情境。同機兩樹比較的 base（`7a5e0ea`）在新 worktree `../GYO-Engine-a1-before` 建置中，L2 的 B 樹也用它。
  - 補上計畫第 3 節的 digest 情境 `combat-step/corridor`（我原本只用單元測試涵蓋新情境表，和核准的計畫不符，補做）：11 個判定和手算表逐筆相同，進 golden。同機兩樹比較（base `7a5e0ea`）：scale 1、3 各 36 個情境，只有 `combat-magazine/corridor` 不同，新情境只在 branch；其餘 34 個 0 差分。本機 `cpu|shader` 72／72。突變完整重跑時 4 個因 base 同時建置、cpu 測試超過 60 秒而 `wrong_failure`，空閒時重跑全部殺死（環境，不是程式）。
  - 四平台 L1（`43a8617`，run 39234431732）全部通過：新 golden `combat-step/corridor` 在四個平台逐位元一致。
  - L2 工具（`build/target/_build/test/logs/pvp-v71-a1-l2-prep-20261019/`）：`pv7-checkpoint` 的工具改成兩棵 pv7 樹（B＝`../GYO-Engine-a1-before` 的 `7a5e0ea`、A'＝主 checkout 的 `43a8617`），修掉 D135 的標籤錯誤；版本 2 紀錄的 A1-2 改用判定 Tick 對接；A' 的 clean Cooldown 改判 A1-1（有解釋的只記錄）。開發跑次 A' 45 種、B 5 種全部 P、0 停止。乾跑：逐項 93、突變 12（全殺）、driver 8、模擬 session 7、第 5 節合成 14，全部如預期（第一次乾跑的 4 個失敗都是工具或夾具：合成紀錄放在檔尾被讀成新 session、A1-1 讀到版本 1 紀錄時崩潰、預檢訊息的舊檔名；已修）。宣告第 3 版：[a1-shot-step.md](a1-shot-step.md) 第 6、11 節。
  - L2 session（D138；`build/target/_build/test/logs/pvp-v71-a1-l2-20261019/`，145.4 分）：238／238、0 停止、0 受干擾，`passed`＝true。C1／C2 的 A'−B 最大 +0.213／+0.149 ms；C3、C9 成立；A' 的 clean 項目權威 Cooldown 0、保留 0；保留只在 upstream-1000ms 3 次（和 D45 的模擬預測相同）。合法射擊 P95（只記錄）：Match 端約 +17 ms（1 Tick），Client 端 +14～33 ms。C6 旗標 2 個（Expired 少 1、接受多 1）。細節在 [a1-shot-step.md](a1-shot-step.md) 第 12 節。
  - L3（[a1-shot-step.md](a1-shot-step.md) 第 13 節；`build/target/_build/test/logs/pvp-v71-a1-l3-20261019-233447/`）：兩個產品 Client＋2 個 bot 的 4 人房；兩位使用者 113 發全部準時、J−A＝1、保留 0、權威 Cooldown 0，Client 本機擋下 0。使用者：「比之前好了好多，沒有那種黏滯感了。而且連點都比較平均」。`a1-shot-step` 結案。
- 2026-10-19：`pv7-merge` 開始（使用者：「開始 pv7-merge：PR #78 轉 ready，四平台 L1 通過後用 gh pr merge --merge 合併，同步文件。」）。合併前同步文件：`protocol-v7.zh-Hant.md` 標為生效，v4～v6 契約的「現行 wire」註記改成 pv7；README 的 `pv7-merge` 標 ✓。PR 轉 ready 後由四平台 L1 決定合併。
- 2026-10-20：PR #78 轉 ready 後四平台 L1 通過（run 39271741788，`af29796`；同時被取消的 synchronize run 39271740271 的 CI gate 失敗是取消造成），以 `gh pr merge --merge` 合併（`0fabb52`）。**pv7-line 完成。**
- 2026-10-20：after-pv7 開始（使用者：「開始 after-pv7：從 master 開新分支，先做 tt2-trace 的計畫，交我核准再實作。」）。分支 `claude/pvp-v71-after-pv7`。`tt2-trace` 計畫第 1 版：[tt2-trace.md](tt2-trace.md)。讀程式時確認：產品有四種寫出器（移動紀錄每 20 ms 輪詢、`LogFile` 在呼叫端每行 flush、射擊與 ingress 紀錄在主執行緒每秒寫）；`MovementTraceWriter::Finish` 有一個不計數的遺失窗口（計畫第 6 節）；估 14 h（README 原估 10 h 沒含產品改接與 L2 工具）。
- 2026-10-20：`tt2-trace` 實作（D139；細節在 [tt2-trace.md](tt2-trace.md) 第 10 節）。Engine 的 `GYO::Trace`（`TraceSink`）與 `Log.hpp`；產品的移動紀錄與 `LogFile` 改接，位元組不變（對照 `0fabb52` 的寫檔器產生的 golden）。本機：CTest `cpu|shader` 73／73（權威 digest 0 差異）、Engine 突變 5／5、產品突變 3／3、TSan 乾淨。實作時更正計畫第 6 節：Client 原本就把 SDL 日誌轉進日誌檔，所以 Client 裝一個 facade handler 轉給 SDL，慢 acquire 行的位元組不變。xhigh 審查（T-4）與 L2 宣告第 1 版草稿的 D45 檢查進行中；L2 的 B 樹＝`../GYO-Engine-a1-before` 切到 `0fabb52` 重建。
- 2026-10-20：xhigh 審查（T-4）：沒有 MUST-FIX；SHOULD 6、NIT 6，處理記在 [tt2-trace.md](tt2-trace.md) 第 10 節與 Engine 的 TT HANDOFF。四平台 L1 第一輪（`cd41c70`，run 39281009934）Windows 的 Trace 測試失敗：sink 以文字模式寫檔（和被取代的寫出器相同，Windows 是 `\r\n`），測試寫死了 `\n`；改測試後重跑（`d30da3f`，run 39285479124）。我在還原一個突變時誤用 `git checkout`，把 `TraceSink.cpp` 未提交的審查修正一起還原，已重新套用並重跑全部測試與突變。
- 2026-10-20：L2 宣告第 1 版的 D45 檢查（1 位 xhigh）：MUST-FIX 5（鎖等待 P99 卡在第一格、Client 端沒人量、`LogFile` 不在 L2、兩棵樹要從零建置、換驗收判定要你核准）、SHOULD-FIX 4；第 2 版的對應在 [tt2-trace.md](tt2-trace.md) 第 12 節。開發量測：A 每筆移動事件在生產者上的成本約為 B 的 10 倍（P99 約 25 µs 對 1.5 µs），8 筆一個 Tick 仍在 1 ms 內（第 11.6 節）。
- 2026-10-20：多生產者的開發量測（使用者在分支會話中同意，不算判定；[tt2-trace.md](tt2-trace.md) 第 11.6 節）：三條生產者照 Match／Client 的實際節奏同時記錄，B、A 交錯各 2 次。結果和單一生產者相反：A 每筆 P50 0.1～1.3 µs、P99 4～32 µs，B 3～20 µs、31～52 µs；Tick 的 8 筆合計 P99 A 約 40 µs、B 約 90～100 µs。推論是 CPU 省電狀態（B 的寫檔執行緒 20 ms 才醒一次），未驗證。兩種量法下都在 1 ms 的換算預算內。仍停在第 11.7 節的 L2-1、L2-2。
- 2026-10-20：D140 之後做 L2 準備（[tt2-trace.md](tt2-trace.md) 第 13 節）。兩棵樹乾淨重建（`ninja clean`＋刪 stage 目錄；B 的 Gateway 先前是 `7a5e0ea` 的舊建置，這次以 Gateway 目標重建成 `0fabb52`；A 先提交文件 `1218a74` 讓 Gateway 不標 modified）。工具 `run_tt2.py`、`judge_tt2.py`、`producer_t29`；開發跑次 16 項全 P、T2-9 每筆 P99 29.5 µs；乾跑第一次有 1 列是乾跑自己的合成數據錯（已改、保留輸出），第二次全部如預期（突變 15／15）。宣告第 3 版補明重建方式（第 2 版的「刪掉 `build/target`」會連 logs 一起刪）、沿用的停止條件（R8、K1a、身分、A1-2 是漏列的既有條件；R1s 不適用）、T2-7 只有 P95。
- 2026-10-20：`tt2-trace` 的 L2 session（D141；`build/target/_build/test/logs/pvp-v71-tt2-l2-20261020/`）：48／48、0 停止、0 受干擾、完整性問題 0，`passed`＝true。T2-3／4（鎖等待 P99）兩棵樹都在 52 µs 以下、≥250 µs 0 筆；T2-5 A−B −0.14～+0.22 ms；T2-9 每筆 P99 27.6 µs（×8＝0.22 ms）。只記錄：送出 P95 +0.04 ms（四個情境方向一致，在預測內）；Match 的 CPU 時間 +25～35%（寫檔執行緒每批被叫醒一次，B 是 20 ms 輪詢）。細節在 [tt2-trace.md](tt2-trace.md) 第 14 節。**`tt2-trace` 結案**（README 沒有 L3）。
- 2026-10-20：`log-contract` 開始（使用者：「開始 log-contract：照計畫用 ultracode 規劃，先出計畫交我核准。」）。範圍：README 的列、INVENTORY 指向它的項目（D-LOG、D58-1／3／4／5／8、E1、E1-2／5／7、TT2-B10、N-* 7 項；原寫 9 項是數錯，D142 之後更正），加上本文件記下的兩種 i/o timeout 字串、兩個 `coalesced_snapshots`、暫定的 clock sync 行族，以及 `tt2-trace` 留下的 `match-shots.jsonl`、`match-ingress.jsonl` 寫出（D139 T-1）。
- 2026-10-20：`log-contract` 的 ultracode 規劃完成（盤點 2、方案 3、評審合成 1 用 high；對抗式檢查 1 用 xhigh；7 個 agent 約 42 分鐘）。評審以「讀者優先」方案為底稿，接上「觀測缺口優先」的紀錄與「最小契約」的停損門檻，合成約 19 h 的草案；對抗式檢查 MUST-FIX 7（間隔用晚醒的 bins 量不出停頓、沒有版本鍵的凍結讀者升版時會靜默讀錯、i/o timeout 的處置和「統一」相反、新增 Gateway 欄位要你同意、改斷線行會打斷既有測試與突變、「被解析的行一定有 golden」當天不成立、和 protocol-v7 §1 的日誌規則衝突）、SHOULD-FIX 10、NOTE 5，全部吸收進計畫第 1 版（[log-contract.md](log-contract.md) 第 10 節），估時改為兩個子批共約 23 h。順帶更正 `tt2-trace.md` 第 1 節的「1 秒」（實際是 10 秒的統計視窗）。
- 2026-10-20：`log-contract` core 實作（D142；[log-contract.md](log-contract.md) 第 11 節）：摘要下溢（先 Red 後 Green）、clock sync 共用 golden 與 Client 行的 player、coalesced 守恆式、A 級釘住（18 族）與凍結讀者守衛、共用禁用片語清單、契約文件 `log-contract.zh-Hant.md`（取代 protocol-v7 §1 的日誌規則）、INVENTORY 9 項結案。CTest 74／74、突變 9 個 killed。發現：Gateway 斷線分類要新增套件變數與錯誤型別，依 Gateway 的 Design Invariants 是要你同意的 Architecture Delta，計畫沒列，所以存成 patch、不提交（LC-10）。偏差：直接寫出的行改用原始碼層的鍵序列釘住；ingress 沒有新增逐字 golden。
- 2026-10-20：`log-contract` records 實作（D143；[log-contract.md](log-contract.md) 第 12 節）。全部切片完成，突變全部 killed，CTest 74／74。開發跑次抓到：`action_late` 的定義錯（閒置 lane 的第一筆被算成晚了數秒，改成從開始有待送內容起算）、守衛的測試恆真（generator 的真假判定）、core 的樣本 `*.log` 被 gitignore（core 的四平台 L1 run 39422895712 因此失敗，已改 `.txt`）。`end` 行的 `authority_tick` 常是小值（房間清空時 Match 重設），只記錄。
- 2026-10-20：`log-contract` 的只記錄 L2（D144；[log-contract.md](log-contract.md) 第 14 節；`build/target/_build/test/logs/pvp-v71-log-contract-l2-20261020/`）：8 個記錄項。受控停頓 53 ms 後 `tick_overrun` 39.7 ms、`tick_stalls` 2，和 trace 重算的間隔差 34 µs；同一 probe 程序的對時行 player 1～4 分得出；repo 版 `ingress_evidence` passed、未知鍵 0；d117 的舊版讀不了 pv7-line 的日誌（本批之前就是如此，只記錄）。
- 2026-10-20：`dead-letter-position` 開始（使用者：「L1 通過後接著開始 dead-letter-position（P6：確認 D57），放在同一個 PR #82，先出計畫。」）。L1 仍在跑，先寫計畫（只讀）：[dead-letter-position.md](dead-letter-position.md)。發現：`drop_report_golden_test.go` 只檢查「每個 DropReason 對到一個 ingress 鍵」，沒有反方向（沒有 DropReason 的 `drop_` 計數器是否就是宣告的只寫日誌原因）。
- 2026-10-20：`log-contract` 的四平台 L1 通過（run 39428630663，`71c9c76`）。**`log-contract` 結案**（只記錄的 L2 是 D144，README 沒有 L3）。
- 2026-10-20：`dead-letter-position` 實作（D145；[dead-letter-position.md](dead-letter-position.md) 第 5 節）。反方向的 Go 守衛第一次執行就失敗：12 個沒有 `DropReason` 的 `drop_` 計數器不是只寫日誌，而是 pv7 刪除的路徑（11 個，恆為 0，依日誌 R1 保留鍵）與已回報丟棄的另一個單位（1 個）。計畫第 2 節把「沒有 `DropReason`」都當成「只寫日誌」，少看了這兩類；守衛改成三類宣告，對應表也列五類。突變 `dlp-unclassified-drop-exit` killed。
- 2026-10-21：`dead-letter-position` 的四平台 L1 通過（`572cc39`，run 39441359248）。**`dead-letter-position` 結案**。依 AGENTS.md §14.4（新版）補記判定變更的 1～5（[dead-letter-position.md](dead-letter-position.md) 第 5 節）：只收緊，舊判定的失敗例手動確認新判定仍失敗。
- 2026-10-21：`d44-regression-pv7` 開始（使用者：「開始」）。先寫計畫（只讀盤點）。
- 2026-10-21：`d44-regression-pv7` 計畫第 1 版（[d44-regression-pv7.md](d44-regression-pv7.md)）。盤點：A1 的 L2 工具已用 `judge_16b` 把第 16 批的 E1～E3 接到 pv7 跑次上，`run_client_delay.py` 在 cp 凍結清單內（雜湊相同），所以不用新寫偵測器。A1 之後還有 13 個 commit，目前的建置沒有被 D44 型 L2 量過。建議：單樹＋第 16 批普查基準，母體照第 16 批 D 部分加 x1、ramp 各 15 次（session 約 3.3 h），短停頓只記錄；合計約 13 h。
- 2026-10-21：`d44-regression-pv7` 的 L2 工具（D146；`logs/pvp-v71-d44-l2-prep-20261021/`）：`run_d44.py`、`judge_d44.py`（事件偵測沿用第 16 批的 `judge_16.py`／`judge_c2.py`，Cooldown 的解釋沿用 A1 的 A1-1）、`dryrun.py`。Gateway 在 `cf40497` 重建（原本是 `70ad10d` 的建置，不是 `572cc39` 的後代）。開發跑次 9 項沒有停止；x1 的窗內重設是 starvation 4＋backlog 1、窗外 0。乾跑 32／32 符合、突變 10／10 killed（第一次跑有 3 個乾跑工具自己的錯：開發跑次的 stdout 路徑、窗內樣本的時刻、impairments 樣本沒有失敗；另換掉 1 個沒有意義的突變）。宣告第 1 版寫在第 8 節，宣告點 4 個（Cooldown 改成「無法解釋」、逐出與限速拒絕只記錄、受干擾只記錄、沒有整合判定）。
- 2026-10-21：`d44-regression-pv7` 宣告第 1 版的 D45 檢查（1 位 xhigh）：MUST-FIX 3（停止條件 4 對 D44 自己造成的 Cooldown 永遠不觸發；沒有 trace 完整性檢查，E1 與停止條件 3 可能是空檢查；停止條件 2 放寬卻少了 A1 的配套 C9）、SHOULD-FIX 6、NOTE。第 2 版：停止條件 2、4 回到「任何 Cooldown」、加 T1、窗內超出與 clean 的逐出／限速拒絕列為使用者旗標、核對 c2-after 模組、v7 基準改讀第 16 批 session 與 08c、長測逾時 1808＋900 秒、機器更正為 Intel x86_64。長測開發跑次 P（1968 秒）。乾跑 44／44、mock session 5／5、突變 18／18。產物清單寫入，preflight 通過。對應表在 [d44-regression-pv7.md](d44-regression-pv7.md) 第 9 節。
- 2026-10-21：`d44-regression-pv7` 的 session 第 1 次（D147；01:34～02:41 JST）停在第 117／197 項 `ramp-r7`：停止條件 1，`run_client_delay.py` 的注入純度檢查不成立（relay 送出延遲 P99 22.3 ms，上限 10 ms；其他 47 次延遲跑次是 2.7～3.6 ms；前後睡眠探測正常）。切分為環境。前 116 項 0 事件、使用者旗標 0。照宣告不重跑；選項在 [d44-regression-pv7.md](d44-regression-pv7.md) 第 11 節。
- 2026-10-21：`observability-tools` 開始（D148）。盤點：late 計數在 v7.1 的判定上已經是增量（d44 證據 121＝121）；相位決策寫進 trace 會改 Data Contract（新 kind 要升版、凍結讀者會停），所以留給使用者決定格式；工具改動會碰 d44 session 凍結的檔案，所以放在另一個 worktree 分支。計畫：[observability-tools.md](observability-tools.md)。
- 2026-10-21：`observability-tools` 的 OT-2～OT-5（[observability-tools.md](observability-tools.md) 第 6 節）：`late_count.py`、`network_resets.py`、遺失種子參數化（預設值重現舊常數），延遲 runner 的 D50 雜湊釘住改到新檔案；單元測試 3 本；新舊分析在 d44 證據上一致（late 121＝121、網路候選 0）；實跑 network20 與 impairments（種子 12345）都通過。注入正式模式（OT-4 的另一半）是改 D50 機制的設計變更，沒做。過程中的錯：第一次讓 `ActionRelay` 用四個參數呼叫基底類別，延遲 runner 的測試全部失敗，改回三個參數；第一次讀 action-frames 時沒處理結尾的 `evidence_end` 行，frames 的計數讀成缺漏。
- 2026-10-21：`d44-regression-pv7` 的 DS-1 補遺（D149；[d44-regression-pv7.md](d44-regression-pv7.md) 第 12 節）。停止條件 1b：只有「量到的 relay 送出延遲超過上限」的注入無效才算 I（不計數，第 3 次停），其他注入無效仍是停止條件 1（限速拒絕是 D121 的訊號，不放寬）。§14.4 的 1～5 由 1 位審查者以資料核對：P1（p99 沒有量到也會被判成 I）已改程式；P2（範圍 2.6～3.6 ms）已更正。乾跑 51／51、突變 23／23，preflight 通過。第 2 版判定另存為 `judge_d44-v2.py`，用來重播第 1 次 session。
- 2026-10-21：`observability-tools` 分支的四平台 L1 #503（run 39470556006，`cbdc2b9`）通過。
- 2026-10-21：`d44-regression-pv7` 的 session 第 2 次（D150；11:13～12:03 JST）在第 92 項之後停下：12:03 主 checkout 被切到 `master`（reflog；不是本會話，本會話期間沒有呼叫工具），凍結的驗收工具變成舊版，driver 依停止條件 1 停下。前 92 項 0 事件、0 個 I。使用者同意後切回 `claude/pvp-v71-after-pv7`，preflight 再次通過。選項在 [d44-regression-pv7.md](d44-regression-pv7.md) 第 14 節。
- 2026-10-21：`d44-regression-pv7` 的 session 第 3 次（D151；12:11～15:03 JST）完成：197／197 項，290 個 case-run 與 60 Hz 長測 0 個 D44 事件，沒有停止、沒有 I 項、使用者旗標 0；受干擾 9 項（都是 0 事件）。使用者搬動電腦與電源線鬆脫的兩段電池供電（13:03:18～13:06:06、13:54:15～13:55:00）和 6 個項目重疊，都是 P、0 事件、沒有受干擾，標記在 `power-intervals.json`。讀法：「未再現」；D44 不結案（A5 → `re-evaluate`）。**`d44-regression-pv7` 結案**（[d44-regression-pv7.md](d44-regression-pv7.md) 第 15 節）。
- 2026-10-21：`incident-close`（D148：照建議走；只改文件）。兩份事故紀錄改為已結案：2026-10-10 Gateway 悄悄丟命令（修正：08c、`drop-report`、edge 系列；重新量測：`d44-regression-pv7` 未再現；09b 重做的剩餘與 D44 歸因交給 `re-evaluate`；M0／FireGate 隨 A1 取消）；2026-10-15 Gateway 狀態漂移（行動 1～4 完成；log-contract 的 D143 是約束在運作的證據）。順帶處理 INVENTORY `N-netarch-ambiguity`：network-architecture 的舊段落仍描述 08c（pv6）的 Gateway 合併、游標副本、拒絕線與去重，加了補充指向 protocol-v7 §2a 與 Design Invariants，原文保留。**`incident-close` 結案**。
- 2026-10-21：`observability-tools` 分支併回 after-pv7 線（merge `f34645c`，無衝突；併回後五本相關的單元測試在主 checkout 通過）。之後的 L2 要用新的凍結清單（`action_probe.py`、`impaired_network.py`、`run_gameplay.py`、`run_network.py`、`run_short_stall.py`、`run_client_delay.py` 都改了；OT-0c）。`observability-tools` 仍有 OT-0a～e 等使用者決定，所以 README 維持 ▶。
- 2026-10-21：使用者的讀法：D44（解釋不了的 epoch 重設）是 Gateway 設計問題最早、最直接的徵兆。寫進 [d44-regression-pv7.md](d44-regression-pv7.md) 第 15 節與兩份事故紀錄的偵測一節、時間線。主對話提議的規則（「被定義成解釋不了的現象，先找看不到的地方，而不是加偵測」）使用者不採用：這條規則在邏輯上是互鎖的。看不到，就需要先建立視野（偵測）；但規則要求先停下來找出看不到的地方，而停下來的時候還沒有視野，用原本的方法很難找到。
- 2026-10-21：after-pv7 線（含 `observability-tools` 的併回，`499f625`）的四平台 L1 #511 通過。
- 2026-10-21：`observability-tools` 的 OT-1 與 OT-6（D152；[observability-tools.md](observability-tools.md) 第 8 節）。OT-1：`phase_decision` 寫進移動紀錄（schema 3），讀者、golden、fixture、日誌契約一起改。第一版每個 window 決定都寫，實跑 16 秒 1177 筆，改成只寫首次、late 與啟動修正的 window 決定。突變第一次有 1 個存活：測試到不了 window 決定，是空檢查，已修。實跑 loss-shot：trace 的修正次數等於 start_phase 的修正次數。OT-6：跑次後再量一次計時器基準。CTest 77／77，相關突變 3／3 killed。
- 2026-10-21：`observability-tools` 的四平台 L1 #517（`3346379`）：第 1 次只有 macos-x64 不通過（`MovementRecoveryTests.cpp:429`，`N-movement-recovery-flake` 的第 2 個實例），使用者同意後重跑失敗的 job，通過。**`observability-tools` 結案**。
- 2026-10-21：`re-evaluate` 開始。範圍是 INVENTORY 處置為 re-evaluate 的 12 項（A4、A5、B6～B9、N-phase-estimator-wan-noise、N-conflict-window-permanent-loss、N-09b-followup-measures、N-phase-1-7-items-3-6、N-quality-reference-age-sampling、N-respawn-reject-volume），加上今天交過來的 D44 歸因與 09b 重做的剩餘（R4、U6）。產出是建議清單，由使用者排序。
- 2026-10-21：`re-evaluate` 建議第 1 版（[re-evaluate.md](re-evaluate.md)）。三組只讀評估：現在結案 6 項（respawn 的 epoch_old 已解釋、衝突窗口在 pv7 不再永久、B9、B8 的 N、N-phase-1-7 第 6 點、N-09b-followup）；小修 1 項（分析器寫死的 105）；已知限制 3 項（A5、B6、參考年齡）；延後 6 項（A4 → v8、B7 → pv8、門檻 105、B8 幅度、快速正向回收、WAN 雜訊）。順帶更正 `d44-regression-pv7.md` 第 15 節的兩處：排隊合計最大 294 出在 upstream-1000ms、backlog 重設在放出之後；late 修正 266 只算遊戲與 x1／ramp。
- 2026-10-21：`re-evaluate` 第 2 版。使用者指出方向錯了：「re-evaluate 就是評估，這些case還是否需要繼續的，而不是評估要如何做的」。第 1 版寫了修法、工時與順序；第 2 版只判斷每項還需不需要，並附根據與重開條件。B7 由使用者先問（「我想的是B7，現在還有必要嗎？」），判斷為取消。
- 2026-10-21：**OT-4 引入的缺陷**（`observability-tools` 結案之後，`re-evaluate` 補量時發現）：`run_case` 建立 relay 時多傳了 `loss_seed`，而 `run_client_delay.py` 替換進去的 `DelayRelay` 只收 3 個參數，所以每一次真實的延遲注入都是 runner error（補量第 1 次 18/18 失敗，證據 `logs/pvp-v71-reeval-map-20261021/runs-attempt1-runner-error/`）。單元測試把 `run_case` 整個替換掉，L1 也不跑真實的延遲注入，所以沒有抓到。照 §14.3：重現（真實跑次）→ 加測試 `test_the_delaying_relay_accepts_the_arguments_run_case_passes`（以 `run_case` 原始碼裡的呼叫參數數去綁定替換類別的建構子），Red 確認 → 修 `DelayRelay` 接受並轉傳種子 → Green；突變 `ot4-delay-relay-three-arguments` killed（新增 `client_delay_runner` 檢查：原本掛在 `cpu` 上時存活，因為 `cpu` 只跑 C++ doctest）。補量第 2 次 18/18 完成。後日談依使用者補上 B8、WAN 雜訊、參考年齡，B9 列為部分相關。
- 2026-10-21：L1 #525（`3ca4e6a`，涵蓋 `98d3c45`）與 #532（`d5a4cec`，OT-4 缺陷修正）都是四平台通過。`re-evaluate` 補量的結果（[re-evaluate.md](re-evaluate.md) 第 4.3 節）：分布和 v7 的 09ab L2 相同；A5 的 300 ms 型 12/12 backlog 重設；B6 在 100 ms 格是 +3 ms、約 4 秒，但 150 ms 格的部分撤回留下 +22～27 ms、約 4 秒（4/8 個單位），比第 1 版的判斷大。
- 2026-10-21：使用者問 B6 的機制與為何仍成立。說明：B6 是相位追蹤自己的時序問題（late 修正後的重新擷取在 slew 結束時決定，用到延遲期間的樣本），09b 的量測本來就是在修好資訊流之後做的，所以不是事故的副作用；第 1 版把它說成「+3 ms」，是沒有區分 1-7 第 1 點與第 3 點。使用者擔心沿用舊方案效果不好，決定交給 v8 重新規劃（D155）；B7、A5 取消。**`re-evaluate` 結案**；after-pv7 的 7 批全部結案。
- 2026-10-21：PR #82 合併（D156，`5ee499a`；轉 ready 後 head 經分支會話與文件會話各推一個只改文件的 commit：`111e3ad` 改「後日談」、`71f550e` 四層總表）。master 的 CI #541 全部通過，v7.1 成為穩定基線（D157）。寫 [STABLE_BASELINE.md](STABLE_BASELINE.md)（參照 v7；pv7-line 的事實由代理彙整、逐項附出處）。寫之前發現兩件事：`N-v7-skipped-acceptance` 標「STABLE_BASELINE v7.1 前決定」，使用者選不補；log-contract 的 LC-8 四項（D143 移到 `observability-tools`）在它的計畫裡漏列、結案時也沒核對，使用者同意照「還需不需要」收尾（D157）。INVENTORY 另外更正 records 已做完卻沒更新的 4 列。
- 2026-10-21：使用者要求在 2026-10-15 設計事故紀錄第 11 節（後日談）開頭補「補丁有多少」的四層總表（Gateway 自己長出的狀態 8 種機制、留下的 33 個計數鍵與 11 個過渡原因、建在截斷資料上的 3 組推導、待評估清單的 9 項），原表保留為第 4 層細目；2026-10-10 事故紀錄第 7 節補充處加連結。由文件會話撰寫，併進 PR #82。

- 2026-10-21：`after-re-evaluate` 開始（PR #83 合併為 `fa3f02d`、文件會話的 #84 合併為 `6fbb6a9` 之後）。使用者：「先評估這份清單，然後優先處理系統和引擎的，其次是觀測工具，再來是測試、最後發行。」範圍是 INVENTORY 處置為「re-evaluate 之後」的項目，加上本文件「未結事項」裡沒進 INVENTORY 的項目。做法同 `re-evaluate` 第 2 版：只判斷還需不需要（附根據與重開條件），再依優先序分組排列；不寫修法。證據收集由 4 個唯讀子 agent（檔位 medium）分組進行，判斷由主對話做。估約 2 h。

- 2026-10-21：`after-re-evaluate` 判斷第 1 版（[after-re-evaluate.md](after-re-evaluate.md)）。共 47 項：還需要 16 項（系統與引擎 4、觀測工具 4、測試 4、發行 4）、交給 v8 隨 B6／A4 4 項、有條件 3 項、不需要繼續 21 列、不在四類 1 項（G2）、清單更正 1 項（N-v7-skipped-acceptance）。HANDOFF「未結事項」裡沒進 INVENTORY 的 6 項給了 `N-` 代號。順帶發現：產品直接呼叫 SDL 的次數在 tt2-trace 後變多（`ForwardEngineLog`）；`compare_reports.py` 只存在 git 忽略的目錄，卻是 log-contract 指定的讀者；worker probe 壞掉讓 22 個 `worker` 突變判不了。停下等使用者決定。

- 2026-10-21：`after-re-evaluate` 第 2 版（D158）。拆批時發現兩個依賴：(1) `track.substituted` 產生負 slack 樣本，是 late 修正的輸入，可是相位追蹤沒有 golden（B10 在 v8），C13 那批改壞了不會有測試失敗；(2) 22 個 `worker` 突變中 20 個指向 `ClientConnection.cpp`，系統類的 cxx-flag-removal 會改到那個檔案，worker probe 壞著就判不了。兩者都列成決策點。INVENTORY 統計對過：121 列，表格逐列解析的結果與統計表一致。

- 2026-10-21：`after-re-evaluate` 批次核准（D159）。`worker-probe-repair` 開始（檔位 high，估 2～4 h，超過 6 h 停下回報）。已知原因一：mock Welcome 沒填 `maximum_shot_damage`；原因二（補上後的 mock 等待逾時）未知，照 §14.3 先重現，查不出就加觀測再停。

- 2026-10-21：`worker-probe-repair` 完成（probe 修正 `d2844a5`，新突變 `5044702`）。
  - 原因兩個，都是 hitbox-fields（`91b0264`）加了契約欄位、mock 沒跟上，不是產品缺陷（§14：先判斷哪一邊錯；Client 照 pv7 §7 拒收是對的）：
    - mock Welcome 沒填 `maximum_shot_damage`，Client 判成「Invalid Match combat rules」。
    - mock snapshot 的命中紀錄只填 3 欄、少了 `last_hit_part`；pv7 §7 要求 4 欄全 0 或全填，Client 把 snapshot **安靜丟掉**，所以只看到「等待逾時」。
  - 找第二個原因的方法：讓 `Until` 的逾時訊息帶出等待的行號（`std::source_location`），定位到「等 Playing」那一步，再對照 Client 收 snapshot 的驗證。這個觀測留在 probe 裡。
  - 判定只收緊：probe 另外確認 Client 收到的 `maximumShotDamage==111`、`lastHitPart==Body`。照 §14.4 事後報告：守的性質是「Client 照 Welcome／snapshot 的權威值保存，不自己補預設」；沒有放寬的面；收緊的面是 Client 把 `maximum_shot_damage` 抄成 `shot_damage`、或讀錯命中部位時會失敗；檢出力由新突變 `wpr-client-maximum-damage-from-shot-damage`、`wpr-client-body-hit-read-as-head` 證明（2／2 killed）。
  - 驗收：本機 worker CTest 通過（6.3～6.8 秒，連跑 5 次 0 失敗）；22 個 `worker` 突變重判 22／22 killed；`object_fps_pvp.mutation_tool` 通過。四平台 L1 未跑（worker 不在 CI 範圍；這次只改 probe 與突變目錄）。
  - 實際約 1 h（估 2～4 h）。
  - Architecture Delta：無。

- 2026-10-21：`sdl-isolation-plan`（D159，ultracode）。分支 `claude/pvp-v71-sdl-isolation`（從 master 開，`sdl-isolation` 的 PR 線）。8 個 agent：Engine 端與產品端各 1 位盤點（medium）、3 個獨立方案（high）、1 位評審合成（high）、1 位 xhigh 公開介面審查（v7 D39）、1 次對抗式檢查（high）；約 38 分鐘。
  - xhigh 審查：有條件接受，三個設計前提成立，必修 6 項已併入草案。對抗式檢查的高嚴重度 3 項：時間戳的整合判定被 clamp 吸收（突變殺不掉）；PR 結構與 v7 P4「一起合併」衝突（照 P4 與 D159 改成一個 PR，Engine 側記為 IP 的 D29）；產品的符號守衛沒有範圍與突變（改成兩層並宣告突變）。產品的工作清單原本寫在 Engine 的 PLAN，屬 Ownership 漂移並違反 Engine 計畫的匿名規則，移到 `sdl-isolation.md`。
  - 產出（`b2024c3`）：Engine 的 IP PLAN 文末的 IP-3／IP-4 草案（API、時間戳契約、日誌橋、子批 ip3a～f、ip4a～d、Design Invariants 與 Replacement Criteria、遷移清單、U1～U10）；IP 的 HANDOFF 的規劃紀錄與決策 D29、D30；產品側 `sdl-isolation.md`（工作清單、符號守衛、L2、估時、S1、S2）。
  - 估時：Engine 37～46.5 h＋產品 19～26 h，約 56～73 h；after-re-evaluate 第 9 節原估 26～34 h，約 2 倍。照時間盒規則停下，交你決定（Engine 的 U1）。
  - 實際約 1.5 h（估 4～6 h，含 ultracode 約 38 分鐘）。

- 2026-10-21：`sdl-isolation` 計畫核准（D160）。ip3a 完成（約 1 h，估 3～4 h）：設計寫成架構文件 `platform-sdl.md`、`input.md`（標明已核准、尚未實作，每條寫出由哪個里程碑成立、由哪個守衛守住）；事前宣告；消費端日誌契約的核對發現 A 級的受擊回饋行經 SDL 寫出，列為 S3。停在關卡。

- 2026-10-21：ip3b 完成（`5e8b353`，分支 `claude/pvp-v71-sdl-isolation` 已推送）。標頭去 SDL、Native 層、G1／G2（先 Red 後 Green）、機械修改 52 處；突變 4／4 killed；`test` preset 78／78；兩樹權威 digest scale 1、3 各 36 情境 0 差異。產品、probe 與 pointer capture 測試暫時改連 Native 目標（IP-4 移除）。發現：IP-2 的測試明確支援巢狀 pump，與架構文件 DI 7 衝突，ip3c 前交使用者決定。四平台 L1 #548 執行中。

- 2026-10-21～22：ip3b 的 L1 #548 成功；draft PR #85。ip3c（事件時間換算到 Engine 時間基準）完成：xhigh 審查發現事前宣告的整合判定在 Linux 與 Apple Silicon 上會誤判正確實作，經 §14.4 與 3 輪獨立核對後改判定（D35），並補讀取注入的接縫（D34）；突變 22／22 killed。實際約 6 h（估 4～5 h）。L1 #550 執行中。

- 2026-10-22：ip3d（文字輸入與剪貼簿，`6ed2952`；原記「L1 #553 成功」不實，見 2026-10-22 的更正；產品的位址輸入改用 Engine，差異：數字鍵盤的 Enter 也會確認）。ip3e（進入點、執行檔目錄、平台名、SDL 日誌橋，`97e2a22`；產品的日誌經 Base 寫檔、位元組不變）。xhigh 審查重現並修正日誌橋的競態；突變 36／36 killed。待決 P1～P3。依使用者指示暫停（D163）。

- 2026-10-22：ip3e 的 P1～P3 依 D164 處理；ip3f 完成（突變 36／36，dev_log）。IP-3 的實作完成，只剩 L3。

- 2026-10-22：ip3f 的 L3 通過（D165）；**IP-3 完成**。v8 候選加「大廳 UI 的元件化（按鈕、位址輸入框）並修改 ui_editor」。

- 2026-10-22：IP-4 介面審查（U10，接受但需修改，D166）；ip4a（查詢、操作、注入，突變 11／11）完成。產品本體（`main.cpp`、`PvpApplication.cpp`）的 SDL 符號歸零，不再連 Native。

- 2026-10-22：ip4b（觀測 hook、capture）完成。
  - watch 的執行緒與生命週期做了局部 xhigh 審查（F1～F14）並修正。
  - 時間戳判定變更與主執行緒規則（D167）。
  - 突變 74／74 殺掉。
  - 提交 `b3f6386`。

- 2026-10-22（更正）：PR #85 是 draft，推送觸發的 run 只做範圍判斷，L1 job 跳過。#553（ip3d）、#554（ip3e）、#558（ip4a）、#560（ip4b）都沒有真的跑 L1，之前記為「成功」不實；真正跑過四平台的只有手動觸發的 #548（ip3b）、#550（ip3c）。已手動觸發 #561（`b3f6386`），補 ip3d～ip4b。

## 未結事項

- **（已處理，2026-10-23）v7.1 README 的「開發紀錄」清單**：PR #88（`c31887f`）合併後，記錄器分支同步 master，並在 README 加上「開發紀錄」一節。sdl-isolation 那一項標為「進行中，隨 PR #85」，該分支合併後改成連結。

- **（已結論，2026-10-23）`N-gui-black-map-intermittent`**：GUI 測試偶發的地圖黑屏。
  - 原因：timing probe 的暖機幀。觀測見 [black-map-observation](black-map-observation.md)，PR #89（`cdb9b07`）。
    - 產品在 `InitializeGraphics` 送出暖機幀：背景純黑、沒有地圖與 HUD、只有人物與武器。
    - timing probe 的 `RunLatency` 先擺放視窗（`gui_main.cpp:389`），讓視窗顯示，接著量計時器基準約 3 秒不 Render（`:390`），所以螢幕停在暖機幀上約 2.5 秒。
    - 只出現在 timing 類跑次（O3、O4 合計 6 次中 5 次），其他 8 種案例和產品 Client（連續啟動 5 次）都沒有。
  - 使用者的決定（2026-10-23）：
    - 只記錄，不改 probe，因為對測試的正確性影響不大；
    - 「預熱幀不得讓玩家看到」是需求，放進 v8。大地圖之後，這個問題可能會被放大。
    - 已加到 [v8 README](../v8/README.md) 的候選表，以及「預計壓力」第 8 項。
  - 還沒解釋、只記錄的兩點：
    - 「視野內沒有玩家的那一端整個黑」：兩端的暖機幀應該相同；
    - timing 那 1 次視窗晚約 4 秒才顯示。
    - 若之後還看得到，可以直接用 `black_map_watch.py` 追。
- **（已處理：`worker-probe-repair`，`d2844a5`）** `N-worker-probe-stale`（2026-10-19，`a1-shot-step` 途中發現，範圍外只記錄）：CTest `object_fps_pvp.worker`（標籤 `network`，不在 `test` preset 與 CI 的 `cpu|shader` 範圍）在 HEAD 也失敗。原因一：`worker_main.cpp:82-84` 的 mock Welcome 沒有填 `maximum_shot_damage`，`hitbox-fields` 之後 Client 的 `ReadRules` 要求 shot_damage≤maximum_shot_damage，所以判成「Invalid Match combat rules」；補上之後又在後面的一個等待逾時（`mock worker wait timed out`），起點未查。修 mock 與查等待逾時要另排；之前「本機 CTest 72／72」的說法只涵蓋 `cpu|shader` 範圍。
- `N-shadow-join-key`（2026-10-18，D127）：`pv7-checkpoint` 的宣告必須把影子紀錄的對接寫成判定：(1) 對接鍵在 (player, E, life, S) 之外加上連線或時間順序（重連後 Match 會 reset，A 與序號從頭算，鍵可能重複）；(2) 對接率：每個 on_time 路徑的裁決都要對得到 `match-commands.jsonl` 的 resolved 事件，對不到就是工具無效。影子紀錄這一側的順序鍵：D128 已在 `match-shots.jsonl` 每筆加 `time_ns`（和 `match-commands.jsonl` 同一個時鐘）。

- `N-movement-recovery-flake`（2026-10-18）：`MovementRecoveryTests.cpp:456`「PvP a delayed first rendered frame does not establish a persistent bootstrap backlog」在 CI macos-x64（Rosetta 2）的 run 39102188643 第 1 次失敗（product 路徑 fps 30、RTT 0、gap 108、worker 4000、authority 6000：`unexpectedRejections` 1），重跑通過；本機 Intel x64 220 次 0 失敗。測試用注入時鐘與固定種子，應是決定性的。要找出非決定性的來源（實作、測試或環境）；在那之前，這個案例的失敗不當成程式碼的回歸。調查（2026-10-18，另一個 session）：本機 x64 在所有試行裡事件序列完全一致，重現不了，分類未切分；依 §14.3 只加了失敗時才輸出的觀測（分支 `claude/bold-hermann-282919` 的 `923bd0e`，只改測試、判定不變，未開 PR、未合併）。使用者：只記錄，其他不處理。
  - 第 2 個實例（2026-10-21，run 39557685906，L1 #517，`3346379`）：另一個案例 `MovementRecoveryTests.cpp:429`「PvP exhausted lead recovers 108 ms 250 ms and six second render stalls across LAN phases」（path v6、fps 144、RTT 20、stall 108、impaired、worker 8000、authority 6000：`unexpectedRejections` 1），同樣只在 macos-x64；windows-x64、linux-x64、macos-arm64 通過，本機 CTest 77／77。這次的推送改了 `LocalPlayerPrediction`（只多寫 trace；沒有安裝 sink 時只是取鎖後返回），所以不能完全排除和它有關；但失敗的種類（同檔、同斷言、只在 Rosetta）和 10-18 改動之前的那一次相同。照平台優先序只記錄。
  - 觀測併入（2026-10-21，使用者：「能加一個保險麼」，選「下次失敗留下證據」）：把 `923bd0e`（只在失敗時印出 Host 的拒絕記錄與每 0.5 秒虛擬時間的事件序列摘要值，判定不變）挑進 after-pv7 線，並在第 2 個實例的案例（:429 的 exhausted lead）也印出。下次 CI 再失敗時，用失敗那一組參數在本機重跑、比對第一個不同的摘要值，就能縮小分歧的區間。故意讓斷言失敗確認過輸出；本機 `object_fps_pvp.cpu` 通過（21 秒）。該 commit 當時已在本機 Intel x64 用 ASan＋UBSan、MallocPreScribble、`-ftrivial-auto-var-init=pattern` 跑過，事件序列全部一致（未初始化記憶體的假設在原生 x86 上是陰性）。

- **（已處理：D121、`edge-rate-limit` 的 L2 通過）K1：Gateway 固定窗口限速在 Gateway 停頓後丟掉合法輸入（已知缺陷，D118）**（2026-10-18，`edge-actions-passthrough` 的 L2 第 2 次 session 停下的根因）。`services/gyo_gateway/session/session.go:118-123` 的限速是「固定 1 秒窗口、超過 120 就拒」；Gateway 停頓時核心緩衝的合法封包在恢復時一次讀進同一窗口，開槍的玩家（約 91 包／秒）約 0.4 秒後超過 120，窗口剩下約 0.6 秒的輸入全部被丟，引發 starvation 重設、延後的新 epoch 與之後的 backlog 重設。每一次 gateway-1000ms 都有它造成的 starvation 重設（被 D61 的 1.5 秒寬限蓋住）；兩棵樹都有，與這批無關。證據與重放工具見規劃紀錄 2026-10-18 05:02 條目。
  - **edge 系列收尾的 `edge-rate-limit`（D119；原為發版前，D118）**：重新測試，並重新評估方案 (a)（token bucket：每秒補 120、容量 120；改 `network-architecture.zh-Hant.md:950` 的契約條文，先寫失敗的重放測試）；(b)、(c) 見同一條目。評估時一併看 `edge-contract` 的 Gateway 狀態白名單（限速是 Gateway 的狀態與丟棄）。
  - 背景（使用者，2026-10-18）：網路在 v7.1 多次修改，很多是改網路機制；這可能是舊 Gateway 留下的痕跡。分析：限速本身是通用層的防洪機制（v5 以前就有），不是產品層的狀態副本；但它的兩個假設（Gateway 即時讀封包；流量遠低於 120）被 v7／v7.1 的修改侵蝕（30 Hz 動作批、時間回聲、09a 的 82 包輸入上限，開槍玩家平時已約 91 包／秒）。

- **（已補進 INVENTORY 為 N-sleep-reconnect，D158）** **筆電睡眠後 Client 會斷線（休眠重連，體驗優化）**（2026-10-17 睡眠測試）：Client 的逾時用 `steady_clock`，睡眠時繼續走，醒來就判定 Gateway 逾時。既有行為，不在 v7.1 範圍。使用者（2026-10-17）：「休眠重連紀錄到未結束事項中，這個屬於體驗優化的情況，給後面調整。」之後排進體驗優化（醒來後重連、或用不含睡眠的時鐘判逾時）。

- **`edge-input-passthrough` 的 xhigh note**（記錄；D158：N3、N10 結案，N5 補進 INVENTORY 為 N-host-lock-keys-uncontracted）：
  - N3：佇列項目攜帶收包時刻與 GYOP 序號（回報 32 的 dwell 起點），字面上和 D105「單次呼叫的脈絡不得跨資料包保留」衝突。建議 `edge-contract` 把它歸類為「隨佇列項目攜帶的診斷狀態」，由你決定。
  - N5：protocol-v7 §9 寫鎖等待直方圖在「IpcHost 層」，實作在 `MatchRuntimeHost`；`network_statistics.py` 不彙總 `host_lock_*`，L2 的 judge 直接讀。
  - N10：`fwd_main_input_packets` 現在是「交給 link」，溢出的資料包同時算它和回報 32；`edge-analyzers` 照這個語意定義 I0。

- **`edge-results-resend` 的 xhigh note**（記錄；D158：N1、N2 被取代，N3 前提消失，N4 只在 Match 與 Gateway 跨機器時重開）：
  - N1：這批 Match 的「ACK-only 補送退役」只在競態下觸發（link 只在 ACK 超前退役時才送 ACK-only），Client 漏收退役仍由 Gateway session 補送；`edge-results-passthrough` 讓 link 原樣轉送 ACK-only 後才真的用到。
  - N2：link 對每個副本都 `notify()` 一次，每位玩家約 30 Hz 多一次 writer 檢查，不影響時序；可改成合併有變化時才喚醒。
  - N3：link 對無法裁決的被拒請求照舊 30 Hz 重送，現在 Match 每次都回一框；L2 的流量數字會看到雙向 30 Hz。
  - N4：重送的副本排在 snapshot 前面；只有 TCP 低於約 120 框／秒才有影響（推論，loopback 可忽略）。

- **（已補進 INVENTORY 為 N-drop-ledger-growth，D158：取消）** **丟棄帳本的 `ended`**（D105）：只增不減，上限是離開過的玩家數；長時間執行的 Gateway 會累積。在 `edge-contract` 時一起看。

- **保留的 worktree 與本機分支**：
  - `GYO-Engine-v7final`（`6d3f287`）、`GYO-Engine-c2-after`（`d685d1e`）：v7 的基準與凍結工具副本，保留到 `d44-regression-pv7` 完成（D146 DR-8 延到 `re-evaluate` 結束）。**（已處理，2026-10-21）**：`re-evaluate` 已結束；worktree 的清理由使用者交給架構會話。
  - `GYO-Engine-m0`（`5ef8a89`，分支 `m0-probe-5ef8a89`，未推送）：M0 已取消（D90），經使用者同意後歸檔。
  - `tt1-l2-wake-record`（`a8f9535`）、`v6c2`（`1be1edd`）等其他未推送的 commit 與 worktree：列清單請使用者決定。
  - 任何刪除都要先經使用者同意。
- **（已處理：D142）D58 (1)(8) 的暫定決定**：在 `log-contract` 一起確認。
- **檔位**：`pv7-design`、`log-contract` 開始前先問 ultracode（v7 D45）；`pv7-design` 另做 1 次 xhigh 對抗式檢查；每次 L2 宣告前做門檻來源的對抗式檢查（D45）。
- **after-re-evaluate 的項目**：判斷已確定（D158），批次草案等核准（[after-re-evaluate.md](after-re-evaluate.md) 第 9 節）。
- **Client 的 `other_version_datagrams`**：只確認了接線（短跑次值為 0），沒有正向測試：統計視窗 10 秒，補測試會讓 worker 測試變長很多。在 `drop-report` 或 `observability-tools` 補。**（仍未補，2026-10-21）**：兩批都沒有處理。**（已補進 INVENTORY 為 N-client-other-version-untested，D158）**
- **`heartbeat-sync` 留下的項目**（xhigh 檢查的 note；D158：「只為測試存在的狀態」已由 edge-contract 分類，結案）：
  - Match 回覆「排在 controls、action lane、snapshot 之後」沒有可決定的測試（asio 下同一個 socket 可寫優先於可讀），只靠審查與 L2。
  - 差分測試不到 10 秒，所以視窗只在關閉時結算一次，跳變與漂移沒有在差分中算到（估計器另有單元測試，且結構上隔離）；也不跑滿品質視窗（600 tick × 3），逐出與 160 ms 門檻沒有在差分中判定。要延長會讓 CI 多數十秒。
  - Match 被 `Stop()`（不經過 `Close`）時沒有最後一個視窗的 log；runner 先停 Gateway，所以平常會走 `Close`。
  - 活性逾時有兩種字串：Read 逾時是 `read tcp …: i/o timeout`，剩餘 ≤0 是 `runtime liveness: i/o timeout`，兩者都含 `i/o timeout`；在 `log-contract` 統一。**（已處理：D143 的斷線哨兵錯誤）**
  - 只為測試存在的狀態：`runtimeLink.now`、`takes`，套件變數 `runtimeLiveness`、`writerIdleWait`、`diagnosticInterval`。`edge-contract` 的白名單（D-14）到時要分類。
  - 差分測試的結尾等 300 ms 沒有訊息，已改成整體截止 3 秒；`edge-results-resend` 之後 Match 會重送未確認的結果，到時再確認。
  - L2 的預期（xhigh 推導，〔推論〕）：Windows 上 Go 的單調時鐘解析度可能是 15.6 ms，Gateway 在 Windows 時會出現假的 `jump=1` 與 `negative_delay`（`lan15-windows` 要事前宣告）；macOS 上 Go 用 `mach_absolute_time`（睡眠時停），libc++ 的 steady_clock 可能睡眠時繼續走，所以睡眠那段兩端都預期出現一次約等於睡眠長度的跳變，不算失敗。
- **`drop-report` 留下的項目**（xhigh 的 note）：
  - 記憶體只增不減：Gateway 帳本的 `ended` 與 Match 的串流表都依玩家 id 累積；惡意的 join／leave 推不高訊息數，但推得高記憶體。
  - 過渡原因的樣本身分不完整：16、17、20、21、28 沒有 action id 範圍；22～26 的 dwell 起點是計數時刻。edge 批刪除過渡原因時一併消失；28（結果寫出失敗）的 action id 範圍在 `edge-results-*` 補。
  - Match 照單接收違規回報的值、繼續往下比對（不標不可信）；L2 分析要把出現過違規的串流視為不可信。
  - 兩個 `coalesced_snapshots`：`gateway transport coalesced_snapshots` 含送出迴圈中途的取代，新鍵 45 只計讀取端；在 `log-contract` 統一。**（已處理：D142，鍵名不改，差額由 `backpressure_test.go` 守住）**
  - L2 的比對腳本 `compare_reports.py` 還在證據目錄；L2 事前宣告前要收進 `build/acceptance/object_fps_pvp` 並加測試（會動到凍結清單：新增檔案）。**（v7.1 contract-readers 已收進 `build/acceptance/object_fps_pvp/`；以下是當時的紀錄。仍未收進，2026-10-21；已補進 INVENTORY 為 N-compare-reports-untracked，D158）**
- **`edge-contract` 的約束**（文件會話，設計事故行動 4）：契約節的標題改成英文 `Design Invariants`，不隨文件語言翻譯（使用者的決定，文件會話 PR #80，2026-10-16 通知時尚未合併；這更正了 PR #79 時「標題逐字保持『不可破壞的契約』」的要求）。
  - `edge-contract` 把 Gateway 的正式條文寫在 `network-architecture.zh-Hant.md` 的 `### Design Invariants` 節下。PR #80 已合併到 master（`8f2f3fe`，四平台 L1 通過）；本分支在 `edge-results-resend` commit 之後同步 master，不自行改名。master 的 AGENTS.md 另新增 §14 Test and Defect Handling（原 §14、§15 往後移），同步後照新規則做。
  - AGENTS.md §3 改成一般規則「状態の権威（State Authority）」，不再列 Gateway 的細節；Gateway 的允許／禁止清單（protocol-v7 §2a）只放在 Design Invariants 節。
  - 同節新增 `Replacement Criteria`：新建元件或加大既有元件的責務之前，先在 Design Invariants 寫下「什麼情況就換掉它」。`edge-contract` 時提案 Gateway 的 Replacement Criteria，由你決定（**已處理**：`network-architecture.zh-Hant.md` 的 Design Invariants「Gateway」一節已有 Replacement Criteria）。

- 2026-10-22：L1 #561 的 windows-x64 失敗。原因是測試自身：並行解構的測試缺握手。延後推送執行緒啟動可以在本機重現，加握手後通過；修正後重跑 #562，四平台成功。
- 2026-10-22：ip4c 完成（`98f5025`）：後端的 SDL3 改成 PRIVATE，加上 G3，突變 78／78 殺掉。L1 #564 四平台成功。

- 2026-10-22：ip4d 完成（`e455d98`、`18b6c03`）：突變全跑一次、沒有重跑，IP 78／78、TT 14／14；寫 IP-4 的 dev_log；L3（live resize）通過。**IP-4 的 Engine 側完成。** 主 checkout 的分支依文件會話轉達的使用者交代，合併 master（#87，`df0fefb`）。

- 2026-10-22：產品側 IP-4 開始規劃（使用者：「開始產品側 IP-4 遷移：先整理工作分批、事前宣告（含 L2 門檻、U5／U6 的判定變更草案）給我核准，核准前不動 probe。」）。3 個唯讀子 agent 盤點：probe 的 SDL 使用 276 行（A 約 186、B 約 21、C 約 67）、L2 門檻來源、pointer capture 的 oracle。草案第 1 版的 L2 經 D45 對抗式檢查改寫為第 2 版（G8、G1、G9、停止規則重寫）。估時 19～25 h（含 L2）。

- 2026-10-23：D168 核准後開始實作。`probe_queries_entry`（檔位 high）完成，提交 `5ef5e69`：
  - probe 的進入點改成 `GyoMain`；查詢、操作、capture 改用 Engine API；產品加 `RenderDeviceInfo()`。
  - `test` preset 80／80；GUI 短測 1 次 P。
  - L1 #573 執行中。
  - `object_fps_pvp.worker` 失敗：這是已知的 `N-worker-probe-stale`，修正 `d2844a5` 只在本分支，還沒進 sdl-isolation 分支。
  - L1 #573（`5ef5e69`）四平台成功。
- 2026-10-23：`probe_injector`（檔位 high）完成：probe 的輸入改用 `SdlInputInjector`。本機 GUI 驗收（`pvp_arena`）：
  - action60 P：事件 80、檢查 21／21、注入射擊 16，與 pv7-checkpoint 的通過跑次相同。
  - capture P、player60 P、gui-bots P。
  - timing-combat：凍結的分析器只因 HUD 判 F，依 D83／D84 改用 v7 HUD 規則判 P；移動 Actual 37.51／37.60 ms。
  - 第一次跑時，場地誤給 `pvp_corners.json`，頭與 base 以相同的方式失敗。這是操作錯誤（E 類），不是缺陷，證據已保留。之前的通過跑次都用 `pvp_arena.json`；batch 1 的 timing 短測也用錯了場地（判的是移動，仍通過），記為「場地不合慣例」。
  - 提交 `d5ecfa3`；手動觸發 L1。
  - L1 #575（`d5ecfa3`）四平台成功。
- 2026-10-23：`probe_window_move`（檔位 high，事件順序局部 xhigh）完成（`089c0b5`）：合成 MOVED 改成真的移動視窗（J2）。
  - xhigh 審查：macOS 上 MOVED 在 `SetWindowPosition` 內就已排進佇列（實測 20／20），沒有誤判的路徑。修正 M1、m2～m4：寫入移動紀錄、改正註解、錯誤訊息帶 detail、drag 前要求擷取中。
  - 本機 action60 4 次、gui-probe 2 次都通過，G8 三行都是 true，`Sync()` <1 ms。
  - L1 #577 四平台成功。
- 2026-10-23：`probe_watch_pacing`（檔位 high，watch 執行緒局部 xhigh）完成（`bbc716a`）：
  - 兩個 watch 改用 `EventWatch`；13 處睡眠改用 `Waiter`；標籤改名；probe 拿掉 Native，沒有 SDL 的 include 路徑也能建置。
  - xhigh 審查：沒有 critical 或 major。minor 5 項已修，測試缺口 T1～T4 記在宣告第 9 節。
  - 本機：
    - timing-short、timing-combat 都 P：synthetic 20／0，沒有受干擾，實測 FPS 59.8，符合 G5 的預測。
    - player60、action60、gui-bots 都 P。
    - phase-stalls 6／6 恢復，下限比 v6 低 1～2 ms。
  - L1 #579（`bbc716a`）四平台成功（先前誤記為 #578）。
  - 下一批 `capture_golden_guard`：J1 的 §14.4 要使用者核准。實作與核對做完就停下，等核准。
- 2026-10-23：`capture_golden_guard`（檔位 high）進行到 J1 的核准關卡，停下。
  - Engine 側不需要核准的已提交：`8d7182e`（insideWindow 依處理時的大小）、`0e977cf`（取得焦點會設 windowFocused）。
  - 工作樹裡已完成、尚未提交：golden（15014 幀，除時間戳以外的全欄位，0 不一致）、新測試、符號守衛 (a)(b)、產品突變工具的 `expect_build_failure`。突變：產品 34／34，Engine 2／2。
  - 獨立核對：第 1、3 點成立，第 2、4、5 點部分成立。指出的 L5（欄位投影）與 v-s07 已處理；v-s08～s10 交使用者決定。
- 2026-10-23：D169。`capture_golden_guard` 完成：`e0de346`（Engine 不去重的契約、測試、突變 3／3）與 `7f7b704`（golden、新測試、守衛 (a)(b)、突變工具）。L1 #582 執行中。下一步 `l2_interleaved` 的第 3 版。
- 2026-10-23：L1 #582（`7f7b704`）四平台成功。L2 第 3 版準備完成（`3106b5b`）：
  - 兩樹 B `2307a56`、A `7f7b704` 都在專用 worktree 完整建置；權威 digest 0 差分。
  - 工具 `run_si.py`／`judge_si.py` 等已寫好，乾跑 117／118（唯一不符的一項是 dryrun 受現況影響）。
  - 開發跑次 12 次全部 P。
  - 照字面做不到的兩項交使用者決定。
- 2026-10-23：D170 已實作：語料重判 11／11（排除 6），頭的 capture 測試通過，乾跑 118／118，預檢通過。L2 等使用者在場時開跑。
- 2026-10-23：sdl-isolation 產品側的 dev_log 已加上（`9e36d0b`），涵蓋已完成的 5 批；L2 完成後補寫。
- 2026-10-23：`l2_interleaved` 完成。
  - 第一次 session 依使用者的「先等等」，在第一項開始前停下（沒有項目跑過）。
  - 第二次 session `pvp-v71-sdl-isolation-l2-20261023b`：**通過**。20／20 P，G1～G3、G6～G8、G10、G11 成立；G3 最大差 0.32 ms；G5 的預測成立；沒有受干擾。
  - **產品側 IP-4 完成**：提交 `d51cbc1`，PR #85 的清單已更新。
  - 記錄上的錯誤：`7ae0588` 的腳本把宣告清空後提交（先以寫入模式打開再讀取），D170 的修改一起遺失。已在 `d51cbc1` 從 `3106b5b` 還原並重新套用。
- 2026-10-23：PR #85（sdl-isolation：Engine IP-3／IP-4 與產品側 IP-4）合併，merge commit `054b08f`。
  - 合併前：同步 master（含 PR #88）、轉成 ready；L1 #592（手動觸發）與 #591（PR）都是四平台成功，PR 狀態 CLEAN。
  - 本分支同步 master 時，`tests/object_fps_pvp/mutations.json` 發生衝突：本分支的 worker-probe-repair 突變與 master 的 sdl-isolation 突變都加在目錄末尾。兩邊都保留（共 521 個，id 不重複），目錄測試 7／7 通過。
  - README「開發紀錄」裡 sdl-isolation 的連結，目標檔案現在已在本分支上。
- 2026-10-23：`substitution-single-store` 開始（檔位 high）。
  - 讀程式碼後停在關卡：兩份「被替代的序號」由同一個迴圈、同一次時鐘讀取寫入（`MatchRuntimeHost.cpp:647-657`），只差保留範圍（64／600）與用途（判定／診斷）。帳本不能驅動判定（`network-architecture.zh-Hant.md:771`）。
  - 提了三個選項：(a) 只記錄並加守衛；(b) 以 SlackTrack 為唯一來源合併；(c) 以帳本為唯一來源合併（不可行）。
  - 使用者選 (b)，並擴大範圍：見 D171。
  - 使用者補充設計原則（2026-10-23）：重連後要重新建立的是「時間上的對齊」。
    - 時鐘：時間回聲重新取樣，不沿用斷線期間的時鐘視窗；
    - 相位：重新擷取送命令的時機，不沿用斷線前的領先量（B6 那一區的機制）；
    - 命令序列：用新的移動 epoch 重新起步，斷線期間的舊命令直接丟棄，不補算；
    - 身分：同一位玩家、同一個 session 要能接回來，這是 Gateway 無狀態原則要特別設計的部分。
    - 整體是 UDP 加應用層的存活判斷，重連時重新對時。要寫成明確的狀態與規則：偵測門檻、中斷期間的行為、恢復流程、真正斷線的上限。
- 2026-10-23：`outage-resync` 分線。從 master `cdb9b07` 開出 `claude/pvp-v71-outage-resync`，draft PR #90。
  - ultracode（workflow `wf_3c8b9349-036`）：2 位偵察、3 份計畫（最小契約 52、契約先行 53、風險優先 58，滿分 70）、評審綜合、對抗式審查。
  - 審查判定「必須修訂」：草案漏讀上面的設計原則（身分接回被縮成同一 endpoint）；部分送達、只有上行、冷卻期內的大負值沒有處理；`slack_clamped` 追加到 ingress 行會觸發 log-contract 的 Replacement Criteria。主會話抽查程式，三點都成立。
  - 第 2 版：狀態與規則寫成表；Match 端加「晚到過久就換 epoch」（N_o）；身分接回列為 D-2（建議 session 的 endpoint 重新綁定，wire 不變）；L2 讀的行改成新的 A 級行族。估時約 59～85 h（不做身分接回時約 53～76 h）。
  - 本線的現況與 dev_log 在本線分支（計畫的「現況」一節、`docs/dev_logs/2026_10_23_pvp_v71_outage_resync.zh-Hant.md`）；本文件只在分線、決定、收尾時更新。

- 2026-10-23：`outage-resync` 的關卡改名：G1 → `gate-characterize`、G2 → `gate-contract`。原因：和 INVENTORY 的項目代號 G1、G2 撞名，使用者從分支會話看到「等你決定」寫著 G2，誤以為和 D159「G2 先不做」有關。同時整理「現況」：刪掉過時的「下一步」（sdl-isolation 已合併）與「最後更新」（2026-10-20），加上任務線總覽。本線分支同步（`42b8e72`）。
- 2026-10-24：`outage-resync` 完成 `single-store-refactor`、`match-resync`（L1 都四平台成功），`retention-600` 的程式完成，等使用者核准判定變更與 L2 宣告。使用者睡前指示「弄完或要確認的內容就等明天」，停在這裡。
- 2026-10-24：`outage-resync` 的 `retention-600` 完成：L2 9／9 P（session `pvp-v71-retention-600-l2-20261024`）。L1 的 Windows Quick acceptance 中 Engine 測試的競態，由另一個會話以 PR #91 修正並合併（master `253808a`）。
- 2026-10-24：`outage-resync` 與 master 同步（帶進 PR #91）。`client-interrupted` 實作完成（`d0efbed` 等）：本機測試全過、突變 72 個全部 killed；本線也帶進本分支 `d2844a5` 的 worker probe 修正（`3547c05`），worker CTest 在本線恢復通過。
- 2026-10-24：`client-interrupted` 的對抗式審查（xhigh 1 位）Major 2、Minor 6，使用者核准補強；補強完成，M2（差分 oracle）的 §14.4 1～5 使用者核准。Gateway 時鐘視窗的重開移到 `identity-rebind`（使用者核准）。
- 2026-10-24：`client-interrupted` 補強後 L1（run 40074065448）四平台成功。`identity-rebind` 完成：通用層 session 的最後驗證時刻與有條件的改綁、Gateway 的 `session rebound` 行與時鐘視窗重開；突變 77 個全部 killed。Hello 間隔維持 1 s（只記錄）。
- 2026-10-24：`resync-l2-l3` 準備完成：注入工具 `run_outage.py`、L2 宣告第 2 版（D45 小規模 ultracode 後改寫：Critical 1、Major 7、Minor 6 全部處置）、乾跑 11 項全部 P、判定工具的竄改檢查 10／10。等使用者核准。
- 2026-10-24：使用者核准 `resync-l2-l3` 的 L2 宣告第 2 版（D178）；L2 33 輪全部 P（session `pvp-v71-resync-l2-20261024`）。記錄器分支開 PR #93（draft，L1 run 40086897377），並合進 outage-resync 線。
- 2026-10-24：`outage-resync` 的 `line-close`（不依賴 L3 的部分）：本線同步 master（含 PR #93）；README、after-re-evaluate 的 `substitution-single-store` 與 `wake-reconnect` 處置；v8 README 的 B6 交接；Fitness Check（AGENTS.md §12）與 Architecture Report（§15）寫在本線 dev_log 的 line-close 一節。剩 L3。
- 2026-10-24：D179：outage-resync 的 L3 移到 `lan15-windows`。PR #90 改成 ready，跑全量 L1。
- 2026-10-24：PR #90（outage-resync）合併，merge commit `03453e2`。之後開 PR #94：CI 方針的單元測試 `test_go_checks.py` 故意觸發的 `::error::` 不再在「Select CI scope」留下紅色注記（使用者要求，在 #90 合併後另開）。

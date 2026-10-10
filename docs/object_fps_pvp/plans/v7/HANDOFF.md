# PvP v7 交接

更新：2026-10-09。Owner：`object_fps_pvp`。**狀態：P1a、P1b 完成**（PR [#70](https://github.com/yojinn-io/GYO-Engine/pull/70) `f3d176d`、PR [#71](https://github.com/yojinn-io/GYO-Engine/pull/71) `660310e` 已合併；第 01～05 批與 TT-1 完成）；**P2 進行中**（PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73)，分支 `claude/pvp-v7-p2`：P2-log 完成；P2-log 與第 06 批完成；P2-log、第 06、07 批完成；第 08 批實作中）。
本文件是 v7 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。
數字未標「實機」的，是 CPU 模擬或靜態分析的結果。

## 現況（每次停下來時更新）

停下來等使用者、或回報里程碑之前，先更新本節再回報。「等你決定」只列需要使用者決定的事；決定之後移到決策紀錄或進度記錄器，並從本節刪掉。長期的已知問題放「未結事項」，不在這裡重複。

- **等你決定**（使用者休息中，下面都先照建議暫定、可撤回，F1b-2 以後照暫定做）：(1) runtime link 在處理某個資料包時失效（只在關閉的空檔，每個跑次最多幾個），這個包怎麼計：F1b-1 暫定把它從玩家的 `received_datagrams` 退回、改計全域 `drop_room_unavailable_datagrams`；建議改成玩家行尾新增 `drop_runtime_failed_packets`（保留玩家歸屬，不用遞減計數；代價是 IG 的加總改用明列的鍵）；(2) J5a 的「never_arrived＝0」解讀成 `unarrived_removed`＋`unarrived_overflow`＋`unarrived_end`＝0（建議照這個，F3 的 `ingress_evidence.py` 依此實作）；(3) F2b-1 改寫了 F2a 的一個測試斷言（`PvpMatchTests.cpp`「counts every input it refuses by reason」：原文 `resolvedUntracked == 2`，改成 `== 0` 並加 `ResolvedCopy == 1`、`LateCopy == 1`、`lateOnlyInputs == 2`；理由：F2a 的分類是 README 明訂的暫定，帳本落地後分類更細，判定沒有放寬；建議接受）；(4) D55 ① 的「動作 ack」計數需要新單位 `acks`（比照 D55 ② 的 `snapshots`），已照 D55 ① 補上 `reset_discarded_actions_acks`；不同意可撤回。（5）F2b-2 新增單位 `samples`（movement slack 樣本的流失計數，比照 `acks`、`snapshots`）；只給 `--ingress-trace`、沒有 `--movement-trace` 時當用法錯誤（exit 2）；不同意可撤回。（6）**L2 宣告前要處理**：J5b（host 端「連續 ≥`MovementPhaseLateSamples` 次發布帶負樣本、沒有被覆寫或取代」）用加總的計數判定不了，要逐次發布的細節；選項：(a) 由既有的 movement trace 推（若它有逐次發布的 slack）、(b) 在 `match-ingress.jsonl` 加一個 kind、(c) J5b 降為只記錄。F3 先查 (a) 是否可行，結果放進 L2 宣告的草稿。
- **進行中**：使用者休息前要求「08c 剩下的部分做完」（2026-10-11 02:5x）：依序做 F1b-2、F2b-2、F3、L1；L2 要宣告核准，不做。F1b-1、F2b-1、F1b-2、F2b-2 完成；F3 開始。
- **下一步**：F3（跨端測試、`ingress_evidence.py`、契約文件；high）→ L1（medium）→ 停下，L2 宣告草稿送核准（D45 的對抗式檢查要你同意檔位）。
- **最後更新**：2026-10-11 04:2x，依據本次推送。

## 閱讀入口

1. [進度與執行規則](README.md)：任務、子系統的處理、批次、PR 線、依賴、平台表。
2. 本文件：先看「現況」；再看決策、各批紀錄、P2 以後各批的範圍、未結事項。
3. [盤點](INVENTORY.md)：時間、執行緒、sleep、socket 輪詢的唯讀盤點，以及更正與研究摘要。
4. 指定批次的文件（02～06、P2-log）。
5. Engine 部分的正式來源：
   - [時間、執行緒與 Trace](../../../architecture/plans/time-threads-trace/README.md)：TT-1、TT-2。
   - [輸入與呈現](../../../architecture/plans/input-and-present/README.md)：IP-3～IP-5。
6. v6 的紀錄：[v6 交接](../v6/HANDOFF.md)（D0～D26）、[v6 穩定基線](../v6/STABLE_BASELINE.md)。

## 決策紀錄

決策從 D27 接續編號（v6 交接與各 Engine 計畫夾共用同一套 D 編號）。引用 v6 的決策時寫「v6 Dxx」。

| # | 決策 |
|---|---|
| D27 | （2026-10-08）Engine 子系統的分配。使用者的修正（原話）：「照這樣修正」，針對「Engine 做最小的角色執行緒，不做執行緒池」的提案。<br>①建進 Engine：Time、最小的 Threads（角色執行緒：名稱、協作停止、優先級提示）、精簡的 Trace、SDL 隔離、Display、Settings 的 io 機制、Audio。<br>②Channels 不先統一，各自實作，重複出現再抽出。<br>③Net transport 留在產品內（asio）。<br>④Job／執行緒池 v7 不建；方向寫進 README：建在 Threads 之上，worker 數＝核心數扣掉角色執行緒。<br>⑤不改 `FixedTickRuntime` 與 `RuntimeLoop` 的語意，不做 render 執行緒，不做錄製重播。<br>⑥修訂輸入與呈現計畫的 D19（原文：「主執行緒（事件＋畫面）、模擬、網路三個角色的分離屬新的 Engine 計畫」）：三角色的執行緒由 Engine 的 `GYO::Threads` 與 `GYO::Time` 提供；角色本身（迴圈、交接、政策）由消費端在產品內做。<br>⑦framework 層的現況與啟動條件寫進 README（使用者同意） |
| D28 | （2026-10-08，照建議）Engine 計畫夾：新建 `time-threads-trace`（TT-1、TT-2）；SDL 隔離與顯示接續在輸入與呈現計畫，編為 IP-3～IP-5（owner 模組相同，另加 `engine/io`）；`audio` 等 P6 開始時再建 |
| D29 | （2026-10-08）`GYO::Time` 是獨立的 Base 層 target（`engine/time`，不含 SDL），並且是 **Engine 的時間基準**。使用者原話：「D29 選 A，並把 GYO::Time 定為 Engine 的時間基準；RuntimeLoop 也在 TT-1 改用它，只換時鐘來源，不改 loop 的語意。」<br>Waiter 採 latch，不收 `std::stop_token`；後端：macOS kqueue、Windows 高解析度 waitable timer、其他平台 cv，都用相對期限加醒後依 steady 重新檢查。例外：`AssetWatcher` 的檔案時間屬牆鐘語意，v7 不改 |
| D30 | （2026-10-08，照建議；2026-10-09 修訂）主執行緒停住、沒有發布輸入時，模擬角色沿用最後的意圖，直到年齡超過 max(150 ms, 3×近期發布間隔)；之後軸回中立、瞄準維持、丟棄跳躍上升沿。宣告最低支援 20 FPS。修訂：最小門檻由 100 ms 改為 3×(1／20 FPS)＝150 ms，讓單一 120 ms 的長幀也不中途停步（第 04 批的驗收 (d) 照原文） |
| D31 | （2026-10-08，照建議）30 FPS 對比拆成 C1（第 05 批：`fee92ff` 對 P1b 頭，只有任務 1、2，Gateway 缺陷兩樹都保留）與 C2（第 09 批：任務 3 之後、pv7 之前）。各指標的權威來源、主機狀態的分層與獨立性檢查、輪數，見 README 的對比做法 |
| D32 | （2026-10-08，照建議）pv7 只在第 11 批升一次，嚴格版本相等，三個角色同一個 PR，舊程序重新啟動（沿用 v6 D11①） |
| D33 | （2026-10-08，照建議）任務 7 的完成條件以 SDL 符號判定，範圍含產品測試；Engine 後端公開標頭去掉 SDL 型別（`*Native.hpp`），SDL3 改為 PRIVATE 連結。ui_editor 與未啟用產品只寫遷移清單 |
| D34 | （2026-10-08，照建議）跨平台驗收依 README 的平台表：Windows 只靠 LAN 場次；Linux 與 macOS arm64 標「未驗證」；不做 CI 只記錄的計時測試；LAN 排不出時，是否升格由使用者在第 16 批開始時決定（比照 v6 D26） |
| — | （2026-10-08）P1 拆成 P1a（02、03、03a，只改產品）與 P1b（TT-1、04、05，跨層）；加做 03a 小量測（使用者決定） |
| D40 | （2026-10-08）第 03 批移到 P1b，和第 04 批一起合併（使用者原話：「選 (1)，第 03 批移到 P1b」）。理由：產品路徑的回復測試在 fps 30 的 1 個組合出現 Held（v6 路徑沒有），原因是在命令跟著畫面幀產生的架構下相位更早收斂；不放寬測試，也不讓 master 出現 30 FPS 變差的中間狀態。P1a＝02、03a；P1b＝03、TT-1、04、05。03a 改在含 03 的 P1b 分支頭上量測，那個頭不合併。D35～D39 是預留給各功能線開始前確認的編號，所以本決定編為 D40 |
| D41 | （2026-10-09，同日更正與補充）FPS 和解析度一樣是玩家的選擇（如同遊戲內的畫面設定），不只是限幀。v7 沒有 UI，第 13 批以命令列選項讓玩家選擇解析度、視窗模式與 FPS 上限（`--fps <上限>` 等），30 FPS 手感的 L3 也在第 13 批做；在 v7 收尾時和解析度功能一起做，不另開批次與 PR。遊戲內的設定 UI（設定選單、套用後未確認就還原）與保存選擇的 `settings.json` 移到 v8。連帶：IP-5 的使用者目錄與 `engine/io` 的原子寫入只服務設定的保存，也一起移到 v8。使用者原話：「30 FPS 手感：PR #71 合併後另開小批，可以開在v7驗收之前的批次裡，產品加 --fps 限幀選項，再做 30 FPS 的 L3。在調整畫面的功能裡，要有畫面分辨率和fps選擇。」；更正：「D41 理解有誤：30 FPS 的 --fps 限幀和 30 FPS 手感的 L3 併進第 13 批（解析度與畫面設定），在 v7 收尾時和解析度功能一起做，不另開 05b／P1c。」；補充：「D41 我來說說我的想法，首先這個不單單是一個fps限制，也應該是玩家的選擇，像是在遊戲中那樣調整fps和解析度那樣。不過現在可以用 --fps 的方式來做，因為現在沒有UI。UI的部分我想是放到v8裡來實現。」「1 照建議用命令列，2 放到 v8」。最初誤記為另開第 05b 批（PR 線 P1c），已刪除 |
| D42 | （2026-10-09）第 06 批碰到停止條件（`action_probe.py` 的「結果每秒 ≤31」在 relay 量到 32）的處理選 (a)：接受本批的行為，不改凍結的 `action_probe.py`；Gateway 的統計行加上最短的寫出間隔，「不爆量」改在 Gateway 端判定。使用者原話：「a 照建議，繼續」 |
| D43 | （2026-10-09）第 08 批的評估之後：D36 維持（評估顯示只縮短約 1～2 ms，但事件驅動後本來就要這樣做）；Gateway→Match 的 action batch 套用 I/2 容許，以及 Client 記錄 worker 收到裁決的時刻（分析器 v7 新增不量化的指標），併入第 08 批；結果改為事件驅動轉送另開第 08b 批，排在第 09 批之前。使用者原話：「D36 維持，三個選項都照建議做」 |
| D44 | （2026-10-09）第 08 批開發跑次的偶發 epoch 重設（新 worker 1／12 次、1／19 案；舊 worker 0／8 次、0／18 案，分不出新舊）：記進「未結事項」並**提高嚴重度**，照常跑第 08 批 L2；各批的開發跑次累計新舊的次數；**v7 完成後（第 16 批）做一次整體回歸**，確認問題是否還在。使用者認為有隱患：網路問題已修了幾輪，可能是多執行緒造成的堆積，而且 Gateway 的頻率也改了。使用者原話：「L2 宣告核准，2 照建議，開始執行。但是吧這個問題的嚴重性稍微調高一下，等v7完成之後，做一次整體回歸看看這個問題是否還在，我覺得裡面有問題隱患。網絡問題我們已經修了幾輪了。可能是多執行緒造成的堆積。而且改了gateway的頻率。」 |
| D45 | （2026-10-09）P2 剩下的部分與 P3 的換檔：一部分改用 ultracode。理由：P2 到目前有三處「推論看起來合理，但沒有對證據核對」：①link 的 I/2 容許在事件觸發下會變成約 60 Hz（使用者在補充中指出）；②第 08 批評估「68～70 ms＝兩個 ticker」沒有根據（後來加了更正）；③L2 判定 2 的 33.3 ms 取自帶 I/2 的建置，又把 ACK 的批次算進去，L2 第 1 次在第 1 輪就停下。這類錯誤應在寫進文件或宣告之前，由對抗式檢查攔下。<br>規模照換檔規則（約 3 個方案、1 位評審、1 次對抗式檢查）：第 08b 批的規劃（Match 與 Gateway 的事件驅動轉送、時序、「結果每秒 ≤31」）用 ultracode；第 09 批 FireGate 常數的推導用 ultracode（要用證據證明，推導完就凍結）；P2、P3 每批 L2 的事前宣告，送使用者核准前先做 1 次對抗式檢查，專門核對每個判定門檻的來源（取自哪個建置、哪個指標、是否混入其他流量）；P3 的 pv7 契約（第 11 批）用 ultracode；TT-2 與第 10 批用 ultracode 規劃（日誌格式會成為 Data Contract），實作回到 high；D44 偶發 epoch 重設的原因調查，真的開始調查時用 ultracode，排程照 D44 不變。<br>執行規則：ultracode 的開關由使用者決定；到了上述項目先停下來問「要開 ultracode 嗎」，同意後才開；開了之後少量偵察就啟動 workflow，並在同一則訊息說明各子 agent 的檔位，高於主對話檔位的部分先徵得同意。一般實作仍用 high，只在有風險的局部升檔。使用者原話（標題）：「換檔決定：P2 剩餘部分與 P3 改用 ultracode」 |
| D46 | （2026-10-09）第 08b 批草案的決定 1～9 照建議：①「事件」＝有新內容，拿掉兩段只帶 retired 的重複包；Match lane 每個裁決與每次退休只送一次，不重送；Gateway 對 Client 未 ACK 的裁決每 I 重送、退休送一次、對 ACK≤retired 的 ACK-only 批次補送退休（poke）。②relay 的「結果任一秒 ≤31」列為 L2 判定（由 judge 套用）。③結果與 link 的最短間隔都用由程式推導的 33.3（字面值比較）。④c 變短列為判定，只比方向。⑤L2＝第 08 批的 4 案加 downstream-250ms，共 72 次。⑥統計行不加欄位。⑦b 的更正已由核對加上；probe 幀格點的相位問題記進未結事項，起點隨機化留到第 09 批規劃。⑧`run_network.py` 加 `--movement-trace`（只多寫 trace，不改判定）。⑨計時器鎖定只記錄，列為第 09 批的輸入。另外，已完成批次核對的 2 項待量測不補量。使用者原話：「決定 1～9 照建議，待量測不補，開始實作，檔位照建議」 |
| D47 | （2026-10-10）第 09 批草案的決定 1～11 照建議：①輸入送出被釘住另開第 09a 批，修正 Client 的 token bucket（產品內的 `ClientConnection`）；②guard 維持 2 ms；③spread 維持 7；④凍結前做 M0 影子量測；⑤相位追蹤的既有缺陷本批不處理，寫進重推條件（同日更正：在 P2 處理，排在 09a 之後、M0 與凍結之前；使用者原話：「抱歉，改一下，決定 5 也在 P2 處理，排在 09a 之後、M0 和凍結之前。」）；⑥只有 M0 的每段起點加 seed 偏移，矩陣、回歸、30 FPS 記錄不隨機化；⑦C2 連點＝12 Tick（判定）＋冷卻邊緣連點（after 判定），其餘只記錄，earliest 只在 M0 量；⑧C2 的主機狀態宣告為偏離 C1，以 TimerBaseline 分層、前後 sleeper 判定轉換；⑨矩陣沿用 v6final 並核對雜湊，連點另開 worktree 加 probe-only 修補；⑩D44 維持第 16 批；⑪檔位：主力 high，09a 與凍結的局部、各宣告的對抗式檢查用 xhigh。使用者原話：「決定 1～11 照建議，開始第 09a 批，檔位照建議」 |
| D48 | （2026-10-10）第 09b 批決定 1（D44 的提前條件）先不選 (a)／(b)，先用測試確認。理由（使用者）：如果 D44 是結構性問題，(a) 只是在補證據；但如果 (b) 的推論是真的，而 09a 照 (a) 做也能通過驗收（閒置主機的 clean 跑次幾乎不會出現 late 修正，碰不到脆弱窗），問題就會被埋得更深。分三步，每步結束先回報：①只讀既有證據擴大樣本（先凍結 late 修正偵測器，以 action-frames.jsonl 的 phase_late_corrections 為主，輔以「大於 20 ms 的負向步之前 30 Tick 內有遲到、且不是 reseed 後的首次決定」；掃過所有 v7 跑次，依「當時是否被釘住」與「之後 30 Tick 合計的最大值」分類，報告 n、2×2 表與每筆證據位置）；②新增 30～150 ms 短停頓的驗收案例（要宣告；先驗證注入乾淨、能穩定重現成對遲到→late 修正）；③三棵 tree 的原型對照（現在的頭／09a 原型／09a＋B2 原型，獨立 worktree、不合併不推送；要宣告並經核准）。測試必須設計成「推論為真時會失敗」，碰不到脆弱窗就判為「無法判定」。09a 的產品實作在這之前不開始；S1 不受影響。 |
| D49 | （2026-10-10）Gateway 職責原則：**Gateway 可以執行丟棄，但丟棄的決定權和知情權留在 Match**；每一次丟棄都要讓 Match 知道丟了什麼、為什麼丟（機制在邊緣，政策與真相在權威）。**名詞**（同日追加）：Gateway 是 PEP（Policy Enforcement Point，執行點），Match 是遊戲語意的 PDP（Policy Decision Point，決策點）。PEP 可以執行丟棄，但要把「做了什麼、什麼時候收到的」交回給 PDP。**和 web 的差別**（同日追加）：兩邊的 gateway 都會保護後面的系統，差別在權威的判斷依賴什麼。web 後端只看請求內容，請求彼此獨立，gateway 安靜地擋掉、去重、快取，不會改變後端的判斷；遊戲的權威（Match）還依賴「相對 Tick 什麼時候到」與串流的歷史，edge 丟掉一個命令就等於改了權威的輸入。所以遊戲的 PEP 除了照規則執行，還要回報時間資訊。**判斷規則**（同日追加）：之後新增任何邏輯，先問「這是執行，還是決策」。決策放 Match；執行可以放 Gateway，但必須回報。①Gateway 的職責：翻譯；守門（擋下無效的：格式錯誤、未授權、超過速率、epoch 不符、內容衝突）；執行權威交代的政策。②遊戲語意與時間的判斷（例如「這個命令過時了」「它晚了多少」）屬於 Match。③告知 Match（帶進 runtime link，Match 可以拿來做決定）與寫 log（給人觀測）分開處理，可以分階段：先 log（第 10 批），協議升版時再進 Match（第 11 批，pv7）。④回報要有界：每個視窗回報計數，細節只回報前 N 筆或取樣，避免被惡意流量放大。⑤將來的丟棄政策（例如反作弊）由 Match 或後端訂成 Data Contract，交給 Gateway 執行再回報；Gateway 不自己發明規則。背景（使用者）：Gateway 不一定要無狀態（session、授權、速率限制本來就是狀態），但職責必須明確；丟棄不一定是錯的，問題在 Match 不知道。產品層的 Gateway 持有遊戲狀態的延遲副本（epoch、life、lastResolved、命令表、動作視窗、結果帳本），會拿它做判斷並丟棄，遊戲語意的決定權因此在看不見的情況下從 Match 轉移到 Gateway。目前的丟棄清單（主對話逐項核對）：(1) 已解析（過時）的命令：`server.go:420-423` 逐個命令略過，同一封包裡較新的命令照樣轉送（`:435-438`）；第二道在 `runtime_link.go` 的 `acknowledge()`（`:167-198`），snapshot 經過時剪掉 link 待送視窗中序號 ≤lastResolved 的命令（`:192-197`），epoch／life 升高時整個待送視窗刪除（`:189-191`）。Match 端早就能收已解析的命令：`PvpMatch.cpp:67` 略過（註解「An irrevocably resolved step」）、`MatchRuntimeHost.cpp:88` 合併時略過、`:104-112` 把「已被替代的命令晚到」記成負的 slack 樣本。判斷是保守的：Gateway 的 lastResolved 只在 Match 的 snapshot 經過時更新（`server.go:520-534`），永遠落後 Match，所以它丟的命令 Match 一定也會忽略；執行結果與權威 digest 不受影響，失去的只有時間訊號。(2) epoch／life 不符的整個封包：`server.go:413-415`。(3) 超過未來 32 個、同序號內容衝突的整個封包：`server.go:424-429`。(4) 速率限制：通用層 `services/gyo_gateway/session/session.go:122-123`（每秒 120 包）。(5) 非 active 玩家的結果（下行）：`action_delivery.go:357-359`。lastResolved 不只用來過濾，還用在未來 32 個的上限（`:424-426`）、命令表清理（`:529-533`）、通知 link 剪除（`:534`）、epoch／life 檢查（`:413-415`），所以拔掉一行換不到「無狀態」。正式條文（`network-architecture.zh-Hant.md` 的「Gateway 職責」一節）在第 11 批的契約規劃時寫，現在不改（待寫）。使用者原話（標題）：「記錄：Gateway 職責原則 → 第 10、11 批的輸入（只記錄，不實作）」<br>（2026-10-10 D51）**已解析命令的回報提前處理**：Gateway 改為照常轉送已解析的命令（連同 link 的剪除與合併上限），在 P2 規劃。（同日更正：D51 ⑩ 把範圍擴大到 Gateway 的**每一種**丟棄都要讓 Match 知道，D49 的第二階段整個提前；原本這裡寫的「其他丟棄這次只計數、寫 log，正式回報仍留給第 10、11 批」已被取代。能轉送的交給 Match 判斷；拿不到命令內容的幾類怎麼讓 Match 知道，等使用者在丟棄回報規劃的第 1 批之後決定。） |
| D50 | （2026-10-10）D48 第②步：注入改為 Gateway→Match 的 IPC 延遲（原本的 client→Gateway 短停頓記為「無法判定」），實作採 (b1)：runner 在自己的行程裡替換 `action_probe.IpcPause`，凍結檔不變。條件：①宣告寫明注入延遲的是 Gateway→Match 方向的全部流量（輸入、動作、ACK、控制一起延遲），不只是輸入；②宣告寫明 Python relay（IpcPause）整場都在 Gateway 與 Match 之間（`action_probe.py:531-533`），不是正式的部署拓撲；第③步三棵 tree 都經過同一個 relay，所以彼此可比；③runner 開頭核對 `action_probe.py` 與 `backpressure_probe.py` 的 sha256，不符就拒絕執行；每次跑完確認 evidence 裡有 delay 欄位，證明替換真的生效；④run_case 的判定只記錄，判定來自凍結後的量測腳本。之後若這個注入要變成常設回歸（第 16 批的 D44 回歸或第 10 批），再走 (c) 升級成凍結工具的正式模式，並宣告更新凍結清單。使用者原話（標題）：「附加：D48 第②步採用 b1 的條件」 |
| D51 | （2026-10-10）**問題的重新定義：先修 PEP／PDP 的資訊流，再談 late 修正與 backlog。**<br>①事實：產品層的 Gateway（PEP）拿權威狀態的延遲副本 `lastResolved` 做判斷，悄悄丟掉已解析的命令（`gateway/server.go:421-423`），link 在寫出前再剪一次（`runtime_link.go:192-197`）。Match（PDP）不知道。<br>②對 Match 的後果：到達紀錄被截斷，晚於 Gateway 水位線的到達全部看不見。遲到量測（`MatchRuntimeHost.cpp:104-112` 的負 slack 樣本）只涵蓋一部分遲到，Client 的相位追蹤在偏差的樣本上做控制；建在這些量測上的 late 修正規則、backlog 餘裕、FireGate guard 的推導，都是在「Client 端網路遲到看不見」的世界裡校準的。<br>③對調查的後果：每次都得繞過盲點。D48 第②步的注入被迫改到 Gateway→Match（D50），第③步的範圍被限縮，結論一再標成推論。一句話：PEP 悄悄做了決定，PDP 拿到的是截斷的輸入。<br>④backlog 餘裕的問題（105 從 v3 沒改，v7 的穩態排隊是 v6 的兩倍）仍然是真的，但屬第二層：要等 Match 看得見全部的遲到，才能正確量出需要多少餘裕。<br>⑤順序：定義問題 → 修資訊流 → 在完整的資料上重新量測（真實流量：network20／40、upstream 等既有案例，不再靠特製的注入）→ 再決定 late 修正（含 B2）、backlog 餘裕、09a／09b 的範圍 → 之後才是 M0、凍結 FireGate、C2。<br>⑥方向（這次只規劃，不實作）：Gateway 停止丟棄已解析的命令，改為照常轉送；必須一起處理 (a) link 的 `acknowledge()` 在寫出前的剪除（`runtime_link.go:192-197`）、(b) link 合併視窗的 32 個上限把已解析的命令也算進去（`:146-148`；超過時整批在 `server.go:439-441` 被悄悄丟掉）。其他丟棄（epoch／life 不符、超過上限、內容衝突、速率限制、非 active 玩家的結果）這次只計數、寫 log。行為會改變（Match 看得見 Client 端的網路遲到之後 late 修正會變多），要明寫，而且必須在 M0 與常數凍結之前發生。<br>⑦D48 第③步的準備成果（三棵 worktree、原型、宣告、證據目錄）原樣保留，不刪、不改。註：第③步的 session 已在使用者核准後執行完畢（18:33～19:15，`step3`＝stopped，見第 09b 批「D48 第③步的結果」），結果同樣保留；它是在截斷的資料上量到的。<br>⑧工作方式（同日，記進 memory）：開始前先估時間，實際超過估計的 1.5 倍、或單一問題累計超過 2 小時就停下回報（已花多少時間、得到什麼、還差什麼、值不值得繼續）；每個關卡先問「這一步的結果會改變哪個決定？」，答不出來就不做。<br>⑨定性（同日追加，使用者：「嚴格來說目前的資訊流就是不正常的」）：**現在的資訊流是整合缺陷，不是取捨。**Gateway 的過濾是 v3 加的（`391ca00`，2026-09-26），理由寫在註解「Resolved steps are obsolete」（`server.go:416-417`），當時成立；`a4ccaa5`（2026-10-02，以持續的相位追蹤取代 A1）讓 Match 開始量「被替代之後才到的命令晚了多少」（`MatchRuntimeHost.cpp:104-112` 的 `substituted`），需要的正是被 Gateway 丟掉的已解析命令；同一個 commit 改過 Gateway 的同一個函式（`server.go:418-419`），過濾卻沒有重新檢視。之後兩個元件的假設互相矛盾，Match 的遲到量測對大多數遲到走不到，late 修正的規則從第一天起就在截斷的樣本上校準。後果：①修它不需要量測證明「值不值得」，量測只用來回答下游的決定（late 修正、B2、backlog 餘裕）；②脆弱度在現實中本來就存在，只是以 Held／starvation 重設的形式看不見，修好資訊流不製造新的風險，P2 照平常的規則合併；③既有工作依基準分兩類：09a 的機制與設計、單次事件的力學（第②步的模型、第③步 J1）、FireGate 的推導公式、第 08b 批保留；09b 的影響大小與 B2 的設計、所有頻率類的結論（第①步普查、D44、Cooldown、backlog 餘裕）、延遲位移、注入的位置要在新基準上重做。（同日更正，依[事故紀錄](Incident/2026-10-10-gateway-silent-drop.md) 3.4 的盤點，使用者核准：② 的「脆弱度以 Held／starvation 重設的形式看不見」是推論（U1），待重新量測；③ 細化為：延遲位移在 clean 下保留、只有損傷網路下的要重做；第 08b 批的結果路徑保留，但 L2 與草案中的次數是頻率類，要重做；Cooldown 拒絕的存在與力學保留，發生率重做；FireGate 的 guard 實測值也保留，但「領先＝2」的前提多常被打破要重做；「注入被迫改位置」只對一半，第②步試跑的 0／5 也有 relay「擋住再放出」只保留最新一包的原因。）<br>⑩目標的擴大（同日，使用者原話）：「現在先處理 gateway 丟指令的問題。先設計一個新機制讓 Match 知道 Gateway 丟了哪些命令、為什麼丟。否則 Match 都是在有限的資訊下做決策。我們不管如何做量測，和對抗測試，都是在解一個不可解的問題。」所以 ⑥ 的範圍改為：Gateway 的**每一種**丟棄（不只已解析的命令）都要讓 Match 知道丟了什麼、為什麼丟；⑥ 的「其他丟棄只計數、寫 log」由這個機制取代。D49 的第二階段因此提前；仍然只規劃，不實作。<br>使用者原話（標題）：「改定問題的定義：先修 PEP／PDP 的資訊流，再談 late 修正與 backlog」 |
| D52 | （2026-10-10，照建議）丟棄回報機制第 1 批之後的三個決定：①照常轉送已解析的命令後 slack 的分布改變（負樣本變多），算**行為改變**，留在 P2（依據：`runtime_v6.proto:63-69` 的欄位定義已涵蓋被替代後才到的命令記負值；schema 與欄位定義不變）；②拿不到命令內容的四類丟棄（速率限制、UDP 舊序號、解碼失敗、未授權）：P2 先由 Gateway 依原因、依玩家計數並寫 log，正式回報在第 11 批 pv7 做（D32 不變；中間期間 Match 不知道這四類，寫成已知缺口）；③Match 自己的無聲拒絕（`IpcHost.cpp:354`、`:358`，`MatchRuntimeHost.cpp:72`、`:90`、`:93`）一併納入。使用者原話：「決定照建議，開始第 2 批，檔位照建議。」 |
| D53 | （2026-10-10，照建議的組合）丟棄回報機制的計畫（`pvp-v7-infoflow-plan-20261010/batch2/plan-draft.md` 第 14 節，以及 `dedup-evaluation.md`）：①＋②＋③ 一起做（互補，不是三選一）。第 1 項 ★：舊序號的 Input 照樣解碼並轉送、不呼叫 CommitSequence（D52 ② 因此縮為速率限制、解碼失敗、未授權、舊序號的 Actions／其他種類）；第 2 項：衝突走拒絕線，D52 ① 的「行為改變」延伸到舊／未來 epoch、衝突、超過上限的窗口；第 3 項：ingress 計數行與 `match-ingress.jsonl` 先定暫定的 v1，第 10 批決定沿用或改版，細節檔路徑由 movement trace 推出（選項 (a)）；第 4 項：清單外的 Match 無聲丟棄（交接被拒、換代丟棄、Leave 丟棄、動作那一側）一併納入；第 5 項：時鐘錨點不做；第 6 項：**改採 Gateway 的「已寫出」去重**（標記點在 link 的 `batch()`；只標記主線；保留期 632，以寫出過的最大序號量；換代與 Leave 時清空；整包被去重時 observed 併入待送窗口），取代「不去重」；第 7 項：重新量測三棵樹交錯（修之前、資訊流、資訊流＋09a 原型）；第 8 項：動作的語意丟棄 P2 只計數；第 9 項：實作 high，拒絕線的順序與切段、★ 的提交、`AdmitInput` 的等價性三處局部 xhigh。另外：`runtime_v6.proto:31` 的註解改寫比照 D22／D23，維持 pv6、同一個 commit 切換；J7 改成去重之後的驗證項。使用者原話：「照建議的組合，開始第 3 批，檔位照建議」。 |
| D54 | （2026-10-10，照建議）核准第 08c 批（[Gateway 丟棄回報](08c-drop-report.md)）實作的分批 F0、F1a、F1b-1、F1b-2、F2a、F2b-1、F2b-2、F3、L1、L2 與檔位（high；拒絕線的順序與切段、★ 的提交與 liveness、`AdmitInput` 的等價性局部 xhigh）；批次名稱 08c。每批結束停下回報。使用者原話：「可以，照建議。」 |
| D55 | （2026-10-11，照建議）第 08c 批 F1b-1、F2b-1 開始前的四個小決定：①F2b-1 為 `ClearState` 清掉的暫存（命令、動作、動作的 ack）與 Leave／踢出時丟掉的暫存動作加原因計數（每個跑次結束都會經過 `ClearState`）；②Gateway 拒收 Match 的 snapshot（D1、D2）新增單位 `snapshots`，鍵接在全域行尾；③`fwd_stale_sequence_input_packets` 是互斥的結果：舊序號的 Input 不論走主線、拒絕線或被整包去重，都只計在這裡，IG 才精確；④不認得的 admission 錯誤由「當成接受」改成丟棄並計 `drop_admission_unknown_packets`（fail-closed；可重用層目前只回三種錯誤，這條路走不到）。另外 Match 動作拒絕的 `full` 依程式推導走不到，比照 `staged_over_window` 註明「應恆為 0」。使用者原話：「照建議，繼續」 |

在各功能線開始前確認（先附建議）：

| # | 問題 | 建議 | 時點 |
|---|---|---|---|
| D35 | Client↔Gateway 時間回聲要不要放進 pv7 | 放進：LAN 實際的偏移（約 0.45 s）在 Client 與 Mac 之間，runtime link 同機；這是量兩機漂移的唯一方法 | P3 開始前 |
| D36 | 任務 3 之後的動作送出語意 | 保持 30 Hz 的最小間隔，符合資格時在 SubmitAction 當下就送出（平均約縮短 16 ms，不改 wire） | P2 |
| D37 | 設定與日誌的位置；ui_editor 的原子寫入要不要併入 `engine/io` | 設定放在 `SDL_GetPrefPath`；日誌留在執行檔旁（LAN 手冊依賴）；併入（工具 ownership 的變更） | P5 |
| D38 | 音效資產的來源 | 由產品腳本程序生成 WAV（48 kHz），可重現、授權單純 | P6 |
| D39 | 審查方式 | pv7 契約用 ultracode 審查（比照 v6 第 09 批）；TT-1 與 IP-3 的公開介面各 1 位 xhigh 審查；C1 宣告草案由 1 位評審加 1 次對抗式檢查。各批開始時徵求同意 | 各批開始時 |

## 第 01 批進度（記錄器）：計畫

- 2026-10-08：v7 開始（使用者指示）。先確認 v1.1.0 已發佈，紀錄寫進 v6 交接的第 17 批。
- 2026-10-08：唯讀盤點（ultracode）：9 個區域 agent（Engine 核心、Engine 其他模組與 services、Client、Match、Gateway、驗收 C++、驗收 Python／測試／CI、其他產品與工具、本機證據日誌）＋1 次 xhigh 對抗式完整性檢查。303 個地點；結果見 [盤點](INVENTORY.md)。
- 2026-10-08：分批規劃（ultracode）：研究 2 個、方案 3 個（證據優先、產品垂直切片、Engine 由下而上）、評審 1 個（xhigh，選垂直切片為主幹，嫁接 15 個做法）、對抗式檢查 1 次（xhigh，0 blocker、10 major，全部修進計畫）。
- 2026-10-08：使用者決定 D27～D34，以及 P1 拆成 P1a／P1b、加做 03a。檔位：第 01 批 medium。
- 本機證據（git 忽略）：
  - `build/target/_build/test/logs/pvp-v7-inventory-20261008/`：inventory.json、digest、規劃的 JSON、確認後的計畫草稿、分析輔助腳本；`files.sha256` 的 SHA-256 `ddc23da0…`。
  - `build/target/_build/test/logs/engine-time-platform-inventory-20261008/`：Engine 範圍的研究 JSON 與計時探針原始碼；`files.sha256` 的 SHA-256 `3f2822ba…`。
- 2026-10-08：PR [#69](https://github.com/yojinn-io/GYO-Engine/pull/69) 開出。CI 的「Select CI scope」失敗：共通測試 `test_app_registry.py` 的 Prepare 測試以寫死的 `v2026.10.1` 在真正的 repo 上執行，`tools-v2026.10.1` 實際發佈之後，`9a6fa8e` 以外的 commit 都會失敗。改用不會發佈的 `v2099.12.99`（`1f7dfc6`），本機 `tests/common/ci` 241 個測試通過，CI 全綠。
- 2026-10-08：#69 依使用者指示合併（`a7cba38`）。**第 01 批完成。**

## 第 02 批進度（記錄器）：ClientSimulation 接縫

- 2026-10-08：開始（使用者指示）。主對話檔位 high。分支 `claude/pvp-v7-p1a` 自 master `a7cba38`。
- 2026-10-08：實作與 L1 完成。細節與結果見[第 02 批](02-client-simulation-seam.md)。
  - 新的產品庫 `client_simulation`（`ClientSimulation`）：持有 `LocalPlayerPrediction`、`PredictionElapsedTime` 與 snapshot 閘；`Observe` 與 `Frame` 對應 v6 的 `PvpApplication.cpp:419-424` 與 `:969-976`。`SelectArena` 照 v6 保留 snapshot 閘。
  - 邊界：Drain、`SnapshotTimeline.Push` 與 `SendInput` 留在呼叫端（`Frame` 回傳要發布的視窗）；本批的庫不依賴網路庫。這是相對於計畫文字（「包住 Drain 與 SendInput」）的調整，理由是 Drain 的其他結果屬於呈現，第 04 批的模擬角色改用另一個自身樣本佇列。
  - 產品與 5 個無頭 probe（矩陣 `gameplay_action.hpp`、`action_main`、`timing_main`、`quad_main`、`network_main`）改走 `ClientSimulation`；GUI probe 經由 `PvpApplication`。
  - 等價測試：v6 產品路徑的凍結參考模型與 `ClientSimulation` 在 60／30／144 FPS、停頓、長幀、重設、arena 重選、失去控制下逐位元組相同。
  - 原始碼守衛 `object_fps_pvp.probe_command_path`；突變 `v7-02-*` 5／5 killed。
  - CTest 全標籤 64／64；權威兩樹比對 35／35（base `a7cba38`，worktree `../GYO-Engine-v7base`）。
  - 開發跑次（不計次）：矩陣 clean-60、clean-30 各 1 輪通過；`run_network.py` 1 次通過。
  - probe 端的行為差異（第一幀 elapsed、`action_main`／`timing_main` 開始套用移動規則、`network_main` 停頓後的 elapsed）照實記在批次文件。
  - 證據：`build/target/_build/test/logs/pvp-v7-batch02-20261008/`。
- 下一步：第 03 批（每份 snapshot 進相位追蹤）。

## 第 03 批進度（記錄器）：每份 snapshot 進相位追蹤

- 2026-10-08：開始（使用者指示）。主對話 high；實作後由 1 位 xhigh 審查 agent 對抗式審查（使用者同意）。
- 2026-10-08：實作與 L1。細節見[第 03 批](03-per-snapshot-phase.md)。
  - `LocalPlayerPrediction::ObservePhaseSample`、`Reseeds`；`ClientSimulation::ObserveSample`；觀測值新增 `phaseSamples`、`phaseSampleSequence`。
  - 一幀一份時與 v6 參考模型逐位元組相同；多份時每個樣本只用一次、依序；突變 v7-03 4／4 killed；權威 35／35；CTest 64／64（審查前）。
  - 開發跑次：clean-60 通過；clean-30 在 8 ms 狀態失敗（Actual 97.6%、Held 21／20），保留不重跑；`run_network.py` 通過。
- 2026-10-08：xhigh 審查：沒有 blocker。major：既有的收斂與 Held 測試只走 v6 路徑。minor：舊樣本觸發的修正被重新播種丟掉但計數照算；測試涵蓋與守衛的順序檢查；settling 期間的舊樣本留在下一個視窗（既有，屬定義，列為後續）。審查也解釋了 clean-30：樣本加倍讓相位更早收斂，在幀量化之下餘裕偏緊。
- 2026-10-08：依審查修正（使用者同意 A）。`MovementRecoveryTests` 改為 v6／產品兩條路徑各跑一次之後，產品路徑出現 1 個失敗組合（fps 30、RTT 20、108 ms 停頓、受損網路：恢復期限後 Held 4 次）。拿掉「重新播種時丟掉舊樣本」仍失敗，原因是本批的核心。依變速箱規則停下回報。
- 2026-10-08：使用者決定 D40：第 03 批移到 P1b。工作 commit 在 `claude/pvp-v7-p1b`（`03b2ea7`，含已知失敗的測試）；`claude/pvp-v7-p1a` 維持只有第 02 批。
- 證據：`build/target/_build/test/logs/pvp-v7-batch03-20261008/`。
- 下一步：第 03a 批（量測含 03 的 P1b 分支頭），建議 medium。

## 第 03a 批進度（記錄器）：相位追蹤的小量測

- 2026-10-08：開始（使用者指示，主對話 medium）。worktree `../GYO-Engine-v6final`（`fee92ff`）與 `../GYO-Engine-p1b03`（`03b2ea7`）從零建置。事前宣告（使用者核准，`56bba67e…`）。
- 2026-10-08：第一次嘗試因 runner 錯誤，12 輪都在啟動前結束（沒有數據，保留）；修正後經使用者同意照原宣告重跑，12 輪完成。結果與觀察見[第 03a 批](03a-phase-only-measurement.md)：11 輪在 4 ms 狀態；clean-30 看不出方向；clean-60 的 after 有 1 輪集中的 Held 與 2 輪延遲中位數約晚 1 幀（和審查指出的相位偏晚同方向）。**第 03a 批完成。**
- 證據：`build/target/_build/test/logs/pvp-v7-batch03a-20261008/`（`08922f71…`）。
- 下一步：開 P1a 的 PR（第 02 批的程式＋文件）。

## P1b 進度（記錄器）

- 2026-10-08：#70 合併（`f3d176d`），P1a 完成；worktree `../GYO-Engine-v7base`、`../GYO-Engine-p1b03` 移除；`claude/pvp-v7-p1b` rebase 到新的 master（第 03 批為 `a393f7e`）。
- 2026-10-08：使用者決定：TT-1 的公開介面以 1 位 xhigh 審查 agent 檢查（D39）；相位追蹤的既有問題（settling 的樣本留在下一個視窗）先不改，第 04 批照計畫做，看 C1 的結果再決定。
- 2026-10-08：TT-1 實作與 L1 完成、xhigh 審查完成並修正（Engine 計畫的交接）；L2（只記錄）等使用者核准事前宣告。下一步：第 04 批。
- 2026-10-08：第 04 批開始（WIP commit）：模擬角色 `ClientSimulationRole`、PvpApplication 與無頭 probe 改接、GUI probe 斷言改寫、L1 (a)～(g)；產品路徑的回復測試在模擬角色下通過（第 03 批的已知失敗解除）。未完：分析器 v7、突變正式跑次、TSan、紀錄。待使用者決定的兩點寫在[第 04 批](04-client-roles.md)。
- 2026-10-09：第 04 批的實作與 L1 大致完成（`aa9a342` 起）：全量 CTest 67／67、突變 9／9、權威 35／35、TSan 0 報告、開發跑次 clean-30／clean-60／250 ms 停頓都通過。剩下：GUI probe 的開發跑次與 GUI 突變（需要畫面）、使用者的 2 項決定（D30 與 (d)、輸入延遲），以及是否對局部做 xhigh 審查。
- 2026-10-09：使用者決定 D30 修訂（150 ms）、其他照建議；GUI 開發跑次（使用者不在、caffeinate）與 GUI 突變完成；xhigh 審查（1 major、4 minor）修正完成（`347d375`）：突變 14／14、GUI 突變 killed、開發跑次與 TSan 都通過。待使用者：D30 門檻逐級放大的小決定（見第 04 批）、TT-1 的 L2 宣告、第 05 批的宣告。下一步：P1b 的 PR（CI 綠燈後才能做 C1）。
- 2026-10-09：使用者決定：D30 門檻逐級放大的問題先不改（C1 之後再看）；第 05 批宣告做 D39 審查（1 位評審加 1 次對抗式檢查）。宣告草案已具體化（`05-30fps-comparison.md`），PR #71 已開（auto-fix）。
- 2026-10-09：第 05 批宣告的 D39 審查完成（評審：必修 6、建議 13；對抗式檢查 12 項），草案依審查改寫，待使用者核准。PR #71 的 CI 在 `805bd11` 全綠。
- 2026-10-09：使用者核准 C1 宣告與 TT-1 的 L2（「C1 宣告核准，TT-1 的 L2 也核准，可以啟動了」）。TT-1 L2 的量測程式不放進 PR #71（會讓 C1 作廢），留在本機分支 `claude/tt1-l2-wake-record`（`a8f9535`），之後的 PR 再合入（使用者選 (b)）。after tree 從零建置（`../GYO-Engine-p1b-c1`，`805bd11`）、預檢通過；暫停 PR #71 的 auto-fix；C1 於 10:27 開始。
- 2026-10-09：C1 第 1 次 session 在基本 30 輪後依停止條件停下（before 無法重現參考範圍；主機不在閒置狀態：`host_late_p99` 10～47 ms、sleeper 間歇 20～75 ms、背景的瀏覽器約 50% CPU）。依規則沒有重跑。TT-1 的 L2（只記錄）已執行：方向規則兩種情境都成立。待使用者決定：在閒置的機器上以同一宣告開新的 session。PR #71 的 auto-fix 維持暫停，直到 C1 結束。
- 2026-10-09：C1 第 2 次 session（使用者核准閒置檢查；關掉 Chrome、ChatGPT／Codex）**通過**：4 ms 狀態有對照，v7 clean-30 的 Client 替代 0.010%（12／12 通過）對 v6 0.570%（9／11）；v7 clean-60 0%（12／12）；停頓重設 0。8 ms 狀態未驗證（after 沒有樣本）。TT-1 的 L2 在閒置主機再記錄一次（Waiter P99 0.10／0.13 ms）。剩下：使用者操作的 L2（拖動與縮放）與 L3，之後合併 PR #71；C1 之後要決定的：延遲位移與相位追蹤、D30 門檻的放大。
- 2026-10-09：使用者操作的 L2、L3 通過：拖動與縮放造成的 11 次 modal loop（1.1～4.0 秒）中，兩位玩家的 Match 替代 0 筆、停頓重設 0、LifeRespawn 以外的 epoch 重設 0；Spaces、縮小、遮住都沒有斷線，模擬角色沒有 ≥100 ms 的晚醒。第 05 批完成，P1b 的驗收全部完成。
- 2026-10-09：#71 依使用者指示合併（`660310e`），**P1b 完成**；worktree `../GYO-Engine-p1b-c1` 移除。
- 2026-10-09：使用者更正 D41：`--fps` 限幀與 30 FPS 手感的 L3 併進第 13 批，不另開第 05b 批與 P1c。下一步改回 P2。
- 2026-10-09：使用者補充 D41：FPS 與解析度是玩家的選擇；v7 的第 13 批用命令列選項，設定 UI 與 `settings.json` 移到 v8（連帶 IP-5 的使用者目錄與原子寫入）。第 13 批、IP-5 與 v8 的 README 依此更新。

## P2 進度（記錄器）

- 2026-10-09：使用者指示開始 P2-log（「IP-5 照你的判斷，開始 P2-log」）。批次文件 [P2-log](p2-log-network-statistics.md)：Gateway、Match、Client worker 各自每 10 秒一行統計，只加記錄、不改行為。分支 `claude/pvp-v7-p2`（從 `660310e`）。
- 2026-10-09：master 的 CI #244（#71 合併）只有 `Snapshot / object_fps_pvp` 失敗：剛建立的 snapshot tag 立刻讀回時還讀不到（GitHub 的讀取延遲），與 #71 的內容無關。依使用者指示重跑失敗的 job 後發布；發行流程的修正單獨開 PR [#72](https://github.com/yojinn-io/GYO-Engine/pull/72)（`fix(ci)`），依使用者指示合併（`247aa2b`）。
- 2026-10-09：P2-log 的實作與 L1 完成：全量 CTest 68／68（權威 digest 不變）、Go 的 vet／test／race 通過；clean-60 開發跑次確認三個程序都有統計行（結果通道約 15 Hz，是第 06 批要修的缺陷）。見[批次文件](p2-log-network-statistics.md)的「結果」。P2 的 PR 待開。
  - 更正（2026-10-09，核對）：「約 15 Hz」由 P50 推得；relay 量到的平均約 15～17 包／秒，約 12％ 的間隔是單一 ticker（見 P2-log 文件的更正）。缺陷本身成立。
- 2026-10-09：#72 合併後，`claude/pvp-v7-p2` rebase 到 `247aa2b`，P2-log 的 commit 為 `983e091`（P2 的 before）。PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73) 已開（auto-fix）。
- 2026-10-09：使用者指示繼續，第 06 批開始（[批次文件](06-gateway-results-30hz.md)）。實作、L1、開發跑次完成；`action_probe.py` 的「結果每秒 ≤31」在 2 案超出，依停止條件停下，等使用者決定。程式留在本機，未推送。
- 2026-10-09：PR #73 的 `L1 / macos-arm64` 失敗：P2-log 的 Match host 測試以固定 200 ms 等 Tick，忙碌的 runner 只量到 2 次。改為輪詢累計（`364884f`），重跑後 CI 全綠（`61ca497`）。
- 2026-10-09：使用者選 (a)（D42）。Gateway 的統計行加上最短間隔；故障案例的確認跑次 6 次全部通過，Gateway 端的最短間隔 ≥18.0 ms。全量 CTest 68／68。第 06 批的程式為 `7da6b0d`，推上 PR #73。L2 的事前宣告寫進批次文件，待使用者核准。
- 2026-10-09：使用者核准第 06 批的 L2 宣告（「L2 宣告核准，Chrome 關了，開始跑」）。60 次全部完成，**通過**：after 全部通過，結果間隔 P50 66.7→33.3 ms，Gateway 端最小間隔 ≥20.6 ms，relay 的任一秒結果數全部 ≤31。Client 端「產生到收到結果」的 P95 中位數下降（clean-30 101.4→68.7 ms）。**第 06 批完成。**
- 2026-10-09：使用者指示開始第 07 批（「開始第 07 批」）。批次文件 [07](07-match-tick-ipc.md)：模擬執行緒改由 `MatchRuntimeHost` 擁有（`GYO::Threads`）並以 Waiter 等絕對期限；IpcHost 改為一條 asio io 執行緒，移除 1／5／10 ms 的輪詢。檔位 high，沒有另開 xhigh 審查 agent。
- 2026-10-09：第 07 批的實作與 L1 完成：A（`ffe9a99`）Match 的模擬角色以 Waiter 等絕對期限、發布通知、覆蓋計數；B（`10003b9`）IpcHost 改為一條 asio io 執行緒、移除輪詢、連線結束的路徑明確化。全量 CTest 69／69、突變 3／3、TSan 無報告、backpressure 6 案與 25 案矩陣通過。clean-60 的 Tick 晚醒全部 <250 µs（P2-log 以 2～4 ms 為主），Match 程序 CPU 約為 P2-log 的 1/3。L2 的事前宣告寫進批次文件，待使用者核准。
  - 更正（2026-10-09，核對）：「約為 P2-log 的 1/3」是不同 session 的開發跑次之比（0.042→0.015）。同一 Match C++ 在第 06、07 批 L2 是 0.030／0.0315，P2-log 的開發跑次偏高；交錯執行的第 07 批 L2 是 0.0315→0.0185（約 −41％）。
- 2026-10-09：使用者核准第 07 批的 L2 宣告（Chrome 開著但不操作）。60 次全部完成，**通過**：after 的 Tick 晚醒全部 <250 µs（P99 上界 244 µs，兩種主機狀態都一樣），before 的 P99 上界 8 ms；Match 程序 CPU 約 -41％。**第 07 批完成。** 接著做橫向對比（見[第 07 批](07-match-tick-ipc.md)的「橫向對比」）：後段變慢是 8 ms 主機狀態下舊 Match 的晚醒與補步，第 07 批之後消失；clean-30 仍受主機狀態影響，來源可能在 Client 端，第 08 批之後再對比。
  - 更正（2026-10-09，核對）：「後段變慢是 8 ms 主機狀態下舊 Match 的晚醒」以 Match 自己的晚醒分布核對大致成立，但機制是 Tick 晚到（a＋b 超過 1 Tick 而 k＝0），不是補步；例外是第 06 批 L2 clean-60 before 第 5、6 輪沒有變慢（見第 07 批「橫向對比」的補充更正）。「clean-30 仍受主機狀態影響，來源可能在 Client 端」不成立：新 Match 的 clean-30 快慢兩群在 8 ms 狀態都有，來自 probe 幀相位與結果的時槽；舊 worker 的送出側在 8 ms 狀態也沒有變慢。
- 2026-10-09：使用者指示開始第 08 批（「開始第 08 批，檔位照建議來」：high，局部 xhigh，對喚醒與期限的交接開 1 次 xhigh 審查）。批次文件 [08](08-client-worker-asio.md)。ACK 維持在主執行緒的 `Drain`（改了會讓尚未取走的裁決在退休時被刪除）。D36 在 P2 開始時漏了確認，現在請使用者確認。
- 2026-10-09：使用者確認 D36（「D36 照建議，開始實作」）。worker 改寫中，`object_fps_pvp.worker` 失敗（「unexpected stalled action batch」）。
- 2026-10-09：使用者要求在 D36 確認前評估 clean-30 的快慢雙峰（只用既有證據）。結論：probe 的時間戳記在幀開始，Client 延遲只能是幀長的整數倍；雙峰來自路徑上幾個 30 Hz 節拍的相位；Gateway→Match 的 action batch 仍有「每隔一次 ticker」的缺陷；D36 只能縮短約 1～2 ms。第 07 批「橫向對比」的結論加了更正。worker 的改寫先存成 stash，實作暫停。
  - 更正（2026-10-09，核對）：「Client 延遲只能是幀長整數倍」對 clean-30 成立，clean-60 的幀長不固定（偏離 P95 0.25～0.33 幀）。「雙峰來自 30 Hz 節拍相位」只對新 Match 成立，而且節拍並不獨立（Gateway 結果鎖在 snapshot 轉送後，c 在同一跑次內也不是單一值）；舊 Match 的 `legal_match_p95_ms` 快慢主要是主機的計時狀態。「Gateway→Match 的 action batch 仍有『每隔一次 ticker』的缺陷」不成立：link 迴圈每個輸入、動作、結果都會叫醒（`runtime_link.go:163`、`:246`），新動作到達時 nextSend 幾乎都已過去。「D36 只能縮短約 1～2 ms」對 a 成立（第 08 批 L2：2.1→0.35 ms），但裁決 Tick 沒有提前（k 前後都約為 0，第 08b 批）。
- 2026-10-09：使用者決定 D43：D36 維持；I/2 容許與 worker 收到裁決的時刻併入第 08 批；結果的事件驅動轉送另開第 08b 批。第 08 批恢復實作。
- 2026-10-09：`worker_main` 的偶發失敗是 D36 讓第一批在分配途中送出、測試邊界的時間競態；使用者核准「worker_main 照建議改」（記下 `firstActions` 前先等 50 ms，斷言不變）。xhigh 審查：無 blocker／major，minor 2 件已修。
- 2026-10-09：使用者的補充：(1) link 的 I/2 在事件觸發下會約 60 Hz → 選 (a) 嚴格間隔加到期計時器；(2) 第 08 批 L2 加 b 段依相位的比較與 probe 幀的相位；(3) 第 08b 批的規格改為「≥I 立即送、不到 I 設計時器」，不帶 I/2。第 08 批的實作與 L1 完成（全量 CTest 70／70、突變 5／5、TSan 無報告、worker 驗收 30 次通過）。
- 2026-10-09：第 08 批的開發跑次：25 案矩陣通過；偶發 epoch 重設 2 次（`run_network.py`、`backpressure_probe.py` 各 1 次）。用本批之前的 probe 交錯跑基準：舊 worker 0／8 次、0／18 案，新 worker 合計 1／12 次、1／19 案，分不出新舊。失敗的跑次保留，沒有重跑取代。
  - 更正（2026-10-09，核對）：基準的「舊」是舊 probe＋第 07 批 Gateway，「新」大多是新 probe＋第 08 批 Gateway，比較的是整組變更；只有前 3 對交錯。分不出新舊的結論不變。
- 2026-10-09：使用者核准第 08 批 L2 的事前宣告並決定 D44（偶發 epoch 重設：記入未結事項並提高嚴重度，v7 完成後整體回歸）。L2 開始執行。
- 2026-10-09：第 08 批 L2 第 1 次在第 1 輪停下（判定 2 不成立：after 的 link 批次間隔 P50 68.4 ms）。查明是宣告的錯：判定 2 的 33.3 ms 來自帶 I/2 的建置，而且這個指標也計入 ACK 批次；嚴格間隔本身成立（最短 33.4 ms）。第 08 批評估「68～70 ms＝兩個 ticker」的推論同樣沒有根據，已加更正（保留原文）。
- 2026-10-09：PR #73 的 CI（macOS arm64）`object_fps_pvp.cpu` 失敗：Match host 的 Tick 統計測試以固定 12 為上限，最後的視窗含 reset 的等待，在負載高的機器上超過。上限改為「視窗長度 × 60 Hz ＋ 3」（`e89d959`）。
- 2026-10-09：使用者決定「判定 2 照建議改，從頭重跑 L2」：判定 2 改為 after 每個視窗的 link 最短間隔 ≥ I−1 ms。L2 第 2 次開始。
- 2026-10-09：使用者決定 D45（P2 剩下的部分與 P3 的換檔：08b 規劃、09 常數推導、11 契約、TT-2／10 規劃用 ultracode；每批 L2 的事前宣告先做 1 次對抗式檢查）。L2 第 2 次照常執行，沒有打斷。
- 2026-10-09：第 08 批 L2 第 2 次通過（60 次；判定 1、2 成立）。觀察：b 段 after 長約 3.5 ms、a 短約 1.8 ms；worker 喚醒與 CPU 約減半。Client 端的主機狀態橫向對比沒有 8 ms 狀態的 clean-30 樣本，無法對比。第 08 批完成。
- 2026-10-09：使用者決定「b 段併進第 08b 批的 ultracode 規劃一起查」：第 08 批 L2 的 b 段變長（after 長約 3.5 ms）在第 08b 批的規劃一起調查。
- 2026-10-09：使用者「開始第 08b 批，開 ultracode，檔位照建議」。第 08b 批開始，先做 ultracode 規劃（方案 high ×3、評審 high、對抗式檢查 xhigh）。
- 2026-10-09：第 08b 批的規劃完成（草案 `08b-event-driven-results.md`）。b 段變長：Tick 解析度下沒有退步，來自量測上的耦合（D36 讓 a 縮短的時間移到 b、probe 幀格點相對 Tick 提早）。D44 一節（分析 high＋對抗式檢查 xhigh）：08b 不直接改變輸入抵達與 backlog；D44 的可能機制是「短停頓 → 成對遲到 → 相位前移 2 Tick → backlog」（推論），排程不變，列為第 09 批的輸入。
- 2026-10-09：非 active 期間的結果（使用者提問；分析＋對抗式檢查，都用 high）：會被丟掉的只有「加入完成之前」（不可能有結果）與「移除之後」（終態，id 不重用，Match 同時刪除狀態）兩種，08b 只送一次不會多遺失；不變式與 G10 測試、3 個突變寫進草案。草案的「心跳」改稱「只帶 retired 的重複包」（使用者要求，避免和第 11 批的心跳對時混淆）。
- 2026-10-09：已完成批次結論的對抗式核對（ultracode：逐條核對 high＋對抗式檢查 xhigh；只讀，不量測）：P2-log、第 06、07、08 批與 HANDOFF 共 82 條，成立 46、更正 34、待量測 2。更正已加在各處，原文保留。主要更正：第 06 批 before 的結果約 12～25％ 是單一 ticker；第 07 批 L2 有 1 次主機狀態分錯（TimerBaseline 只量連線前 3 秒），「4 ms 也有 21.0 ms」的反例因此不成立，舊 Match 變慢的機制是 Tick 晚到而不是補步；第 07 批的 Match 之後 Gateway 的結果鎖在 snapshot 轉送後；第 08 批評估中 link 的「每隔一次 ticker」、節拍彼此獨立且相位固定、c 只有兩個值，都不成立。證據：`build/target/_build/test/logs/pvp-v7-audit-06-08-20261009/`（`itemize/`、`adversarial/`，各有 `commands.txt`、`sha256.txt`）。
- 2026-10-09：使用者決定 D46（第 08b 批決定 1～9 照建議，待量測不補）並開始實作。
- 2026-10-09：第 08b 批的實作與 L1 完成（`a954aa3`）：全量 CTest 70／70、Go race、worker 30 次、IPC 30 次＋TSan、突變 17／17。xhigh review 的 major 2 件已修。
- 2026-10-09：第 08b 批的開發跑次：矩陣 25／25、backpressure 12／12 案、`run_network.py` 3／3；c 的中位數約 0.3 ms（第 08 批約 18／34 ms）；D44 的遲到修正 0 次。L2 宣告定稿，D45 的門檻來源檢查（xhigh）：門檻都成立，修正了覆蓋範圍（每個跑次只有 1 個統計視窗、空洞通過）、after 產物的建置條件（開發跑次的 Gateway 是提交前的工作樹建置）、D44 停止條件與 `command_evidence.py` 的規則對齊、判定 4 的彙總方式與「只改一段」的限制、`%.1f` 看不到的範圍（0.083 ms）。
- 2026-10-10：使用者核准第 08b 批 L2 的宣告，D44 停止條件照建議（只對 after 停下）。L2 開始。
- 2026-10-10：第 08b 批 L2 通過（72 次）：c 的合併中位數 clean-30 17.8→0.37 ms、clean-60 34.2→0.36 ms；relay 結果任一秒最大 15；Gateway 結果與 link 的最短間隔 33.4；D44 的重設與遲到修正 0 次。Client 看到的延遲 clean-30 P95 71.8→37.9 ms、clean-60 中位數 49.7→16.7 ms。第 08b 批完成。
- 2026-10-10：使用者「開始第 09 批，開 ultracode，檔位照建議」。第 09 批開始，先做 ultracode 規劃。
- 2026-10-10：第 09 批的規劃完成（草案 `09-firegate-c2.md`）。常數推導為 guard 2 ms、spread 7（都不變），但只在「命令沒有被釘住、沒有主機停頓、領先＝2」的範圍內成立。三份方案獨立發現「輸入送出被釘住」：token 補充率等於命令產生率，一次停頓後送出等待一直維持（11～16 ms），射擊因此可能早 1 Tick 判定（08b gateway-250ms r6 有 3 發；v6 也有相同的規則）。用 guard 涵蓋需要約 1 step，碰到停止條件。D44 不需要提前。對抗式檢查（xhigh）更正了 guard 的需求量（漏了角色晚醒與送出等待：餘裕約 1.0 ms、設計最壞值約 3.1 ms）、尾巴的取樣、C2 主機狀態分類的出處與 C1 規則的沿用。
- 2026-10-10：使用者決定 D47（第 09 批決定 1～11 照建議）並開始第 09a 批。依計畫先做 S1（P2 頭 30 FPS 記錄，必須在任何產品變更之前），09a 先做設計分析。
- 2026-10-10：使用者更正 D47⑤：相位追蹤的既有缺陷（settling 期間的舊樣本與 late 樣本、延遲位移）也在 P2 處理，排在 09a 之後、M0 與凍結之前。
- 2026-10-10：使用者「09b 的規劃開 ultracode，檔位照建議」。第 09b 批的規劃開始（方案 high ×3：缺陷、延遲位移、交互與風險；評審 high；對抗式檢查 xhigh）。
- 2026-10-10（夜間，使用者休息時）：
  - 第 09b 批的規劃完成（草案 `09b-phase-tracking.md`）：缺陷 A 真實但在 v7 clean ≤0.3 ms，建議 A1（settle 時只保留結尾的 late 樣本）；缺陷 B 在持續延遲下其實有用，建議不改；延遲位移不是缺陷，是「步邊界產生」的代價（I2E 多約 1 幀、backlog 餘裕 75→45）。對抗式檢查（xhigh）的最重要發現：送出被釘住正在吸收 late 修正的額外領先，09a 修好後 D44 可能變多，符合 D44 的提前條件。
  - 第 09a 批的設計完成（草案 `09a-input-send-pinning.md`）：機制是補充率等於產生率造成的中性穩定；建議等待過的送出依等待時間打折、R＝80，恢復最多 4 個命令（實作前推導）。對抗式檢查推翻了 bool 版「不受三包間隔限制」與「可改 R＝75」等說法。依 D44 的提前條件，實作前停下。
  - S1 的準備完成：P1b 與 P2 頭在 worktree 從零建置，宣告與腳本經 xhigh 對抗式檢查，修正 A～J 已套用並由另一位核對。P1b 沒有重現 C1 的雜湊（內嵌路徑與目的檔時刻），要使用者核准。
- 2026-10-10：使用者核准 S1 的宣告。S1 完成（12 次全部 P，全部 4 ms 狀態，只記錄）：Client 看到的動作延遲 P95 從 P1b 的 3～4 幀（100～133 ms）變成 P2 的約 1 幀（35.5～37.8 ms）；c 在 P2 全部落在 1 ms 內；移動「產生→執行」兩邊都約 37.8 ms；替代、領先跳升、重設、Cooldown 拒絕都是 0。
- 2026-10-10：D48 第①步完成（只讀；分析 high＋核對 high）。498 個有 trace 的 v7 跑次中，tracking 型 late 修正只有已知的 5 筆（4 個獨立事件），閒置主機 0 筆。依凍結定義，5 筆在修正前都沒有被釘住；09b 的 4 對 1 用的是修正後的等待，而那是 slew 本身凍結出來的值。核對推翻了「M5 是反例」（重設由故障解除觸發），並找出真正的變數：到達 Match 的領先 ≈ 修正前＋33.3−W_post，超過約 66.7 ms 就重設（5 筆單調一致）。
- 2026-10-10：使用者「做第②步，宣告照新的預測寫，檔位照建議」。D48 第②步開始（工具與試跑 high；宣告的對抗式檢查 xhigh）。
- 2026-10-10：D48 第②步在試跑時停下：client→Gateway 的短停頓 0／5 跑次產生 late 修正。原因（已核對程式）：Gateway 會略過 ≤lastResolved 的命令，晚到的命令大多被吞掉；而且擋住再放出的封包只落在一次發布裡，late 修正需要連續 2 次。改在 Gateway→Match 的 IPC 輸入路徑加延遲，可穩定重現（8／8），模型預覽相差 ≤0.3 ms，但脆弱窗 0／4 跑次被碰到。另發現第二條重設途徑：兩次 late 修正串接（缺陷 B）。
- 2026-10-10：使用者決定 D50（注入改為 Gateway→Match 的 IPC 延遲、實作 b1、四個條件）與 D48 第②步的決定 3＝(iii)：「決定 3 選 (iii)，照建議寫第②步的宣告」。第②步只驗證注入的乾淨、late 修正的重現與模型的精度（不越線那一側；偶然越線的也照預測檢驗），越線那一側交給第③步的 09a 原型。
- 2026-10-10：D48 第②步的宣告完成：runner 依 D50 (b1) 定稿、量測腳本凍結候選；開發跑次 z1 自然碰到越線（W_post 1.6／2.1 ms，領先 69.5／68.9 ms，預測差 0.1 ms，兩位都重設），也暴露量測草稿把越線事件設限掉的結構缺陷，已改為「結果決定之前」才設限。D45 的對抗式檢查（xhigh）：可以核准但要先修正（`measure.py` 4 個漏洞、runner 5 個測試缺口、來源表 5 處、建置程序 2 道核對），已套用。
- 2026-10-10：使用者核准 D48 第②步的宣告（「宣告核准，CTest 不登記，開始跑」）：`test_run_short_stall.py` 不登記進 CTest；runner 與測試隨本 commit（session 頭 H）提交。
- 2026-10-10：D48 第②步的 session 完成，`step2`＝passed（V1 8／8、V2 8／8、V3 16 事件最大差 0.645 ms、V4 不越線側一致、越線側無法判定）。2／8 個跑次自然走進脆弱窗（W_post 3.1～3.9 ms），領先 66.8～67.4 ms，都重設；模型有 +0.04～+0.65 ms 的正偏差，邊界附近要用實測的領先。
- 2026-10-10：使用者「開始第③步，檔位照建議」。D48 第③步的準備開始。
- 2026-10-10：D48 第③步的準備完成（workflow 5 位：原型 high、審查 xhigh、修正 high、宣告 high、D45 對抗式檢查 xhigh）。原型：09a 的 worker 等待 16.5／12.3／8.1／3.9／≤0.4 ms，和推導 N＝4 相符；審查推翻「09a 的折讓與晚醒相消」（已修，並加 bucket 剩餘空間的上限）、指出第②步的量測在結構上判不了 B2（改寫 `measure3.py`）、B2 與 09a 的測試缺口（已補，突變改為被殺）。對抗式檢查推翻產生空檔的固定帶與 J2d 的比較型門檻，修正已套用並由主對話重跑乾跑核對。宣告送使用者核准。
- 2026-10-10：使用者先要求補三處文件（第 09 批的重推條件 11、第③步宣告的範圍限制、D44 未結事項的 D49 第二階段），再核准 D48 第③步的宣告（「宣告核准，四件事照建議，檔位照建議，開始跑」）：三棵 tree 共用 base 的 Match、Gateway 與 arena；84 個項目；J2d 為絕對門檻；開發跑次每棵 2 次已確認。session 由主對話直接執行。
- 2026-10-10：D48 第③步的 session 完成，`step3`＝stopped：停止條件 7（權威 Cooldown 拒絕）在 chain-r2-base、chain-r5-09a-b2 各 1 次。被拒的那一發本身沒有被延遲；前一發在注入視窗內被延遲 7 Tick，權威的間隔被壓成 9 Tick（cooldown 10）。只記錄的計算值：V4X 一致；J1 base 2／10 對 09a 10／10（p＝0.00036，09a 的 W_post ≤0.156 ms）；J2 20 個事件全部撤回約 +30 ms、合計 71～74、不重設；J3 不成立（09a-b2 的 chain 4／10 單位重設：延遲比重新擷取長時串接或撤回不足）。不補跑，交使用者決定。
- 2026-10-10：使用者決定 D51（問題的重新定義：先修 PEP／PDP 的資訊流，再談 late 修正與 backlog），並在 D49 註明已解析命令的回報提前處理。D48 第③步的成果原樣保留。「修資訊流」計畫的規劃待使用者決定是否開 ultracode。
- 2026-10-10：使用者同意把「現在的資訊流是整合缺陷，不是取捨」補進 D51（⑨，附 `391ca00`、`a4ccaa5` 的證據），並決定「修資訊流」的規劃開 ultracode。
- 2026-10-10：使用者擴大目標（D51 ⑩）：先設計讓 Match 知道 Gateway 丟了哪些命令、為什麼丟的機制。規劃開始（ultracode）。
- 2026-10-10：使用者認定這一天「撞出了一個事故」，要求在 `v7/Incident/` 新開資料夾記錄經過；事故紀錄的 workflow 開始。
- 2026-10-10：使用者要求把丟棄回報機制的規劃分批（「可以想辦法分批處理嗎？不開 ultra，我怕又繞進去了」）：停掉一次跑完的 workflow，改為三批，每批結束回報。
- 2026-10-10：丟棄回報機制第 1 批（事實核對 3 位，19 分鐘）完成。要點：Gateway 有 30 多處「收到卻不轉送」，只有速率限制有計數；lastResolved 只落後 Match 典型 <0.5 ms，所以 Match 幾乎量不到任何遲到；D48 第②步的 0／5 有兩層原因（relay 的擋住再放出只保留最新一包，Gateway 再過濾掉送得到的那幾個）；只刪 `server.go:421-423` 會讓 `:424` 無號數下溢、整包被丟，而且沒有測試釘住過濾；照常轉送不改 wire，新增回報欄位或訊息依 `protocol-v6.zh-Hant.md:50` 算改 wire；Match 自己的拒絕也是無聲的。證據：`pvp-v7-infoflow-plan-20261010/facts-*/`。
- 2026-10-10：事故紀錄完成初版：[Incident/2026-10-10-gateway-silent-drop.md](Incident/2026-10-10-gateway-silent-drop.md)（新資料夾 `Incident/`，索引在 `Incident/README.md`）。workflow：時間線與盤點 2 位、撰寫 1 位、對抗式核對 1 位（high，26 分鐘）；核對的 18 項修正與第 1 批的事實已併入。同時更正 D49 的 D51 加註（和 ⑩ 矛盾）。
- 2026-10-10：使用者要求補完事故紀錄。補上第 1 批的五點（盲區的大小與兩種看得見的情況、注入工具也在丟資料、丟棄點的規模、「只拔一行」會下溢、嚴重度的界線）；確認穩定基線：`391ca00` 與 `a4ccaa5` 都在 v5（`f97beb5`）、v6（`c7d6dd3`、`fee92ff`、`object_fps_pvp-v1.1.0`）之內，v4（`bfeb047`）只有過濾、沒有遲到量測（推論：不受影響）。
- 2026-10-10：使用者決定 D52（第 1 批之後的三個決定照建議），第 2 批開始。
- 2026-10-10：丟棄回報機制第 2 批完成。評審把三個方案整合成計畫草案：log 統一成 ingress statistics 行族（Gateway 的 `fwd_*`／`drop_*` 與 Match 的帳本行）；①的拒絕線必須和②的拒絕帳本同一批落地；②提的對帳式不成立（link 合併去重、游標落差），改成 I0～I2 三條精確守恆；方案 ③ 發現 UDP 舊序號的 Input 內容完整、只是不解碼，network* 約 4～5% 的首份副本被丟（推論），列為改變 D52 ② 的決定。實作估 13～16 小時，要分批。
- 2026-10-10：使用者要求先評估第 14 節第 6 項的替代案（Gateway 依「已寫給 Match 過」去重，延伸 link 的合併去重；保留期對齊帳本的 10 秒），1 位 high 評估開始。
- 2026-10-10：第 6 項替代案的評估完成：Match 對每個命令只用第一份（`MatchRuntimeHost.cpp:103`、`:107-111`、`:117`），丟掉相同的後續副本不損失決定用的資訊；標記點在 `batch()` 消除了「bitmap 說謊」（截短與溢出都在取走之前）；性質是 link 合併去重的延伸（傳輸事實），和 08b 的 `sent` 同一模式；流量從約 180 降到約 60 命令／s／玩家，停頓時 0 則。發現的接縫：`runtime_v6.proto:31` 的註解、換代清空是正確性條件、HEAD 的 `server_test.go:306-310`、`:675-679` 斷言重送要照轉（v3）。淨增 1～1.5 小時。
- 2026-10-10：使用者決定 D53（照建議的組合），第 3 批（對抗式檢查，xhigh）開始。
- 2026-10-10：第 3 批完成。判定：去重的完整性前提、★、拒絕線與衝突（限正常 Client）、契約與凍結分析器、digest 都成立；需修正：①缺資料包層的守恆 IG（否則 `receivePacket` 裡新的無聲 return 抓不到，正是這次事故的型態）；②I1 的 6 個前提（寫出數在 Write 成功後計、final 行用 Once、EOF 與單一連線、分桶、帳本跨過 ClearState、每行是增量）；③J4 被推翻（拒絕線溢出不只在寫出端阻塞時發生），J5 改成 J5a／J5b，J7 的單位；④09a 那棵樹要建 Client，注入工具不得經過三條既有的丟包路徑；⑤★ 的測試要用從沒寫出過的命令，`MatchRuntimeHost.cpp:93` 是死碼；⑥protocol-v6 加修訂節。總時間估 15～19 小時，F1b、F2b 要再拆。
- 2026-10-10：使用者決定事故紀錄第 8 節的行動項目：D51 ⑨③ 的回寫與 U1 的推論標記、截斷資訊流上的三處加註（核准，已改）；v5、v6 的 `STABLE_BASELINE.md` 加「已知問題」並連到事故紀錄，重新發佈修好之後再決定（已改）；注入工具併入丟棄回報計畫的 L2（斜坡注入）；M0 等的順序標成已決定（D51 ⑤）；U4 併入重新量測。並決定繼續把計畫寫成批次文件。
- 2026-10-10：第 08c 批（Gateway 丟棄回報）的批次文件完成：[08c-drop-report.md](08c-drop-report.md)。第 3 批的修正全部併入（IG、I1 的前提、J4／J5／J7、三棵樹各建 Client、注入工具不經過既有的丟包路徑、★ 的測試、死碼、protocol-v6 修訂節、拆批）。整體估 15～19 小時，分 8 批加 L1、L2。
- 2026-10-10：使用者核准第 08c 批的分批與檔位（D54）。F0 開始。
- 2026-10-10：第 08c 批 F0 完成：鍵名的單一來源（Go `gateway/ingress_statistics.go`、C++ `IngressStatistics.hpp`，都還沒接到執行路徑上，行為不變）、golden 樣本 v1（`tests/object_fps_pvp/fixtures/ingress_v1/`）、Go 與 C++ 的格式與凍結分析器約束測試。和計畫不同的命名：全域四類加 `drop_` 前綴；全域行多 `received_datagrams`、`drop_rate_limited_packets`；「交給 link」與「已寫出」依線分開；close 清掉與寫出後放棄分成兩個鍵；Match 端多 `accepted_inputs`、動作的 `resetting`。留給 F2b 接在行尾：替代計數與關閉原因、樣本流失計數。
- 2026-10-11：第 08c 批 F1a（Go）與 F2a（C++）完成並 commit。F1a：主線照常轉送並修 `:424` 的下溢、拒絕線、已寫出去重、★、link 的 I0 計數；xhigh 審查沒有 blocker／major，補了 5 個測試缺口（速率限制、提交時機、換代清 `newestWritten`、寫出時標記已解析的命令）、主線在已解析游標先切一刀、link 退回原因細分。F2a：`AdmitInput`、Match 依原因的拒絕計數、IpcHost 不再無聲、統計行與 final 行；xhigh 審查以差分實驗（300 種子 × 3000 步等）確認和 HEAD 行為相同，修了踢出時 `leave_discarded` 多算。驗證：go vet／test／race、全量 CTest 66／66、權威 digest 兩棵樹 0 差分；`mutations.json` 新增 75 個（Go 52、C++ 23），用 `run_mutations.py` 全部 killed。
- 2026-10-11：使用者決定 D55（F1b-1、F2b-1 開始前的四個小決定，照建議）。F1b-1 與 F2b-1 並行開始（各 1 位 high）。
- 2026-10-11：第 08c 批 F1b-1（Go）與 F2b-1（C++）完成（各 1 位 high，27 分鐘、47 分鐘）。F1b-1：Gateway 的 ③ 計數（速率限制、解碼失敗、未授權、舊序號的 Actions／其他、未知種類、非 active、A3、A5、D1、D2）、IG 與 T3、D55 ②～④；玩家的計數在 reservation 移除後仍保留到被取走；45 個子案例的「每個資料包恰好一個結果」測試。F2b-1：host 層的替代紀錄帳本 `MatchIngressLedger.hpp`（保留 600、上限 1024、關閉原因 7 種）、六類分類依帳本產生、`late_only_inputs`、J5a 的鍵、D55 ① 的重設／Leave 丟棄計數；權威 digest 和 `3909484` 比較 0 差分（scale 1、3），差分 harness 80 個種子的 slack 與權威行為相同。使用者休息後主對話補上 D55 ① 的動作 ack 計數（新單位 `acks`）。驗證：go vet／test／race、全量 CTest 66／66；`mutations.json` 新增 67 個、更新 6 個（F1b-1 讓 4 個既有 Go 突變的 find 失效），用 `run_mutations.py` 跑 73 個全部 killed。待使用者確認的四點見「現況」。證據：`pvp-v7-08c-impl-20261011/f1b1/`、`f2b1/`、`verify-f1b1-f2b1/`。
- 2026-10-11：第 08c 批 F1b-2（Go，27 分鐘）與 F2b-2（C++，70 分鐘）完成（各 1 位 high）。F1b-2：Gateway 每 10 s 寫 player／gateway ingress 行（增量），`flushFinal`（`sync.Once`）在 Close、ctx 取消、runtime 失效任一路徑只寫一次，等 link 關閉、writer goroutine 退出後才取計數；6 種結束路徑的測試（含寫出失敗、writer 阻塞、失效與 Close 競爭）。F2b-2：`match-ingress.jsonl` 寫出端（`MatchIngressTrace.hpp`；rejection 每桶每原因每視窗 16 筆、總量 16384、suppressed／dropped、`trace_end` 最後、I/O 失敗 exit 1、檔名推導與 `--ingress-trace`）、`DrainIngressRecords()`、slack 樣本流失的 10 個計數（S1：executed＋late＝merged＋discarded＋published；S2：published＝overwritten＋taken＋unclaimed）、新 CTest `object_fps_pvp.match_ingress_cli`。權威 digest 和 `d49ee1e` 0 差分，差分 harness 80 個種子相同。驗證：全量 CTest 67／67；`mutations.json` 新增 49 個、更新 2 個、新增 check `match_ingress_cli`，用 `run_mutations.py` 跑 51 個全部 killed。證據：`f1b2/`、`f2b2/`、`verify-f1b2-f2b2/`。
- 2026-10-09：使用者的補充（不打斷規劃 workflow，完成後處理）：(1) 第 08b 批草案加一節「對 movement 佇列與 backlog 的影響」，回答 08b 的改動會不會改變 movement 輸入抵達 Match 的時機或集中度、會不會讓 backlog（30 Tick 內排隊命令合計 ≥105）更容易觸發；只用既有證據與程式分析，回答不了的列出需要的量測。(2) 第 09 批的 ultracode 規劃把 D44 列為輸入：D44 是否要在常數推導前查清楚，或在什麼條件下要重推常數。D44 的排程不變，規劃認為必須提前時停下來問。(3) 第 08b 批草案送出後、實作前，用 ultracode 對第 06、07、08 批（含 P2-log）的推論與結論做對抗式核對：證據、數字的來源建置與指標、是否混入其他流量；只讀不量測，有問題的照慣例加「更正」並保留原文，腳本放新的證據目錄並記雜湊。
- 2026-10-09：使用者同意檔位（「xhigh 可以，照建議配置」）：第 08b 批草案的 D44 一節由 1 位分析（high）＋1 位對抗式檢查（xhigh）撰寫；已完成批次的核對由 1 位逐條核對（high）＋1 位對抗式檢查（xhigh）。

## P2 以後各批的範圍

批次文件在該線開始時撰寫；行號以那時的程式為準重新核對。

### P2：網路路徑（任務 3）

- **P2-log**（單獨的子批次，先 commit，作為 P2 的 before；06～08 的修正都依賴它）：
  - Gateway 每 10 秒的統計：每個 session 的 results 與 snapshot 送出間隔分布、runtime link action batch 的間隔。
  - Match：程序 CPU 秒數、IPC 迴圈每秒迭代、Tick 的預定與實際喚醒。
  - Client worker：每 10 秒的喚醒次數與 CPU 秒數。
- **06** Gateway 結果通道：資格判定改為只在 `now + I/2 < nextSend` 時才跳過（`action_delivery.go:205`），保留寫出後的重新錨定與「不爆量」。修正後若碰到分析器「每秒 ≤31」的窗口規則，停下由使用者決定。runtime link 的 action batch 不改，只量測。
- **07** Match：Tick 改為 Waiter 的絕對期限（Advance 前的取樣時刻加 `secondsUntilNextTick`），晚醒寫進 10 秒統計；MatchRuntimeHost 在 snapshot、results、evictions、重設完成時通知 IpcHost，並計數 `snapshot_` 槽被覆蓋的次數；IpcHost 改為一條 asio io 執行緒（async accept／read／write，pump 的優先序 controls > actions > snapshot 不變），1／5／10 ms 的輪詢全部移除；連線結束的路徑明確化。執行緒改用 `GYO::Threads`。完成後做 P2-log、第 06、07 批的 clean-30／clean-60 橫向對比（2026-10-09 使用者要求，見「未結事項」）。
- **08** Client：worker 改為一條 asio io 執行緒（async receive、各期限一個 steady_timer），SendInput／SubmitAction 以 post 喚醒；速率語意不變；httplib 留在 worker（只在大廳切換與關閉時阻塞 UDP）。ACK 判定在 L1 顯示語意不變時改為網路角色收到裁決時前進，否則維持並記錄理由。動作送出語意依 D36。
- **08b**（D43；規劃依 D45 用 ultracode，並一起調查第 08 批 L2 的 b 段變長）結果改為事件驅動轉送：Match 的 action lane 與 Gateway 的結果，距上次送出 ≥I 就立即送；不到 I 就設計時器在「上次送出＋I」送出，不帶 I/2（2026-10-09 使用者更正：I/2 在事件觸發下會讓結果每秒約 60 次，碰到「結果每秒 ≤31」）。不改 wire；relay 的「結果每秒 ≤31」要重新確認。排在第 09 批之前，因為第 09 批要用這些通道推導 FireGate 常數。
- **09** FireGate 與 C2（規劃依 D45 用 ultracode；D44 列為輸入：是否要在常數推導前查清楚，或在什麼條件下要重推常數）：先推導常數並凍結，再跑 C2 與 25 案回歸（README「本機射擊閘的兩個常數」）。P2 頭另跑一次 30 FPS，只記錄，單獨顯示任務 3 的影響。
- 停止條件：權威 digest 改變；需要改 wire；macOS 的 Tick 晚醒沒有改善；worker_main 的斷言需要放寬；常數必須比 v6 大；乾淨跑次出現權威 Cooldown 拒絕。

### P3：診斷與 pv7（任務 8、4）

- **TT-2**（Engine）：見 Engine 計畫。寫檔執行緒用 `GYO::Threads`；只在佇列由空轉為非空、或達到批次門檻時才 Notify。
- **10** 任務 8 的紀錄：Client 的拒絕原因、未指定 `--gateway` 的提示、以 worker 收包時間戳計算的 snapshot 年齡、模擬步晚醒的 10 秒摘要；Match 的動作裁決與原因（`match-actions.jsonl`）、結束紀錄、runtime link 關閉紀錄（含原本吞掉的例外）、每位玩家每 10 秒的 Held／Neutral；Gateway 的收包間隔分布、拒絕原因、control lane 丟棄計數、週期性 IPC 寫出延遲、loopback advertise-ip 的警告。日誌格式是新的產品 Data Contract：帶版本與驗證規則，C++ 與 Go 兩端的測試解析同一份樣本。新紀錄寫到另外的檔案。
  - **輸入：觀測缺口清單**（2026-10-10 使用者要求；規劃依 D45 用 ultracode，開始前先問）。這次 v7 有不少「未驗證／無法判定」，不是事情沒發生，而是沒有 log 可以觀測。第 10 批的規劃以這份清單排優先順序，目標是之後像 D44 這種問題能從 log 直接回答。每一項附上目前的繞法，以及因此只能標成推論的結論：
    1. **Gateway 的所有丟棄都沒有計數或 log**（D49 的丟棄清單 (1)～(5)：已解析的命令 `server.go:420-423` 與 link 的剪除 `runtime_link.go:189-197`；epoch／life 不符 `server.go:413-415`；超過未來 32 個或內容衝突 `:424-429`；速率限制 `services/gyo_gateway/session/session.go:122-123`；非 active 玩家的結果 `action_delivery.go:357-359`）。這是 D49 原則的第一階段：只計數與寫 log，不改行為，零風險。繞法：只能由 Client 的 `sent` 與 Match 的 `host_accepted` 對不上來推。推論：D48 第②步「client→Gateway 的遲到大多被 Gateway 吞掉」是由程式與 0／5 的試跑推出的，沒有直接的計數。
    2. **Match 的 `host_accepted` 只記新收下的序號**（`MatchRuntimeHost.cpp:86-92`、`:128-131`），遲到、已被替代的命令不寫 trace。它們會在 `:104-112` 變成負的 slack 樣本、隨 snapshot 送給 Client，但只有最小的一個，而且不落地。繞法：由 `resolved`（source＝held／neutral）與 Client 的 `sent` 重建。推論：第①步「遲到命令」的認定、late 修正的觸發條件。
    3. **相位決策（首次決定、視窗修正、late 修正與幅度）不寫進 match／clients trace**（`MovementTrace.hpp:95-105` 沒有對應的事件）。只能從 action probe 每幀的計數器推回來（`gameplay_action.hpp:153-154`：`phase_state`、`phase_corrections`、`phase_late_corrections`），時間解析度 16～33 ms；timing／network probe 沒有這些計數器，只能用產生間隔的相位步重建（D48 第①步的輔助偵測器）。推論：late 修正的幅度、是否串接（缺陷 B）、settle 時刻；第 09b 批的開環重播只吻合 72／131 的視窗決策。
    4. **Gateway 丟掉非 active 玩家的結果，不寫 log**（`gateway/action_delivery.go:350-359`）。繞法：第 08b 批只能靠程式論證（加入完成之前不可能有結果、移除是終態）與 1087 份 gateway.log 的 id 掃描。推論：「移除之後到達的結果有多少」數不出來。
    5. **match.log 的 `tick_late` 會漏掉 Tick 間隔**：第 08 批 L2 第 2 次 clean-60-r3-before 的 `tick_late_max_us`＝146，但 resolved 時刻的 Tick 間隔有 28.0 ms（第 09 批規劃的對抗式檢查；以 `snapshot_produced` 重算，最大間隔 30.9 ms）。繞法：用 resolved／`snapshot_produced` 時刻的 Tick 間隔偵測停頓。推論：第 09 批「新 Match 的 Tick 停頓 9／162」等計數依賴這個繞法。
    6. **TimerBaseline 只在 probe 連線前量一次 3 秒**（`timer_baseline.hpp:3-5`、`:20`），跑次中途的主機狀態轉換抓不到；第 07 批 L2 有 1 次因此分錯（第 07 批文件 L2 宣告「主機」的更正）。繞法：每輪前後的獨立 sleeper；舊 Match 用自己的晚醒交叉確認（新 Match 的晚醒不反映主機狀態）。推論：各批 L2 依主機狀態分層的結果。
    7. **Match lane（IpcHost）的送出間隔沒有統計欄位**：第 08b 批 D46⑥ 決定不加、留給第 10 批。繞法：L1 的 M3 測試（3 秒內的 frame 數）與 relay 下行的 c 段。推論：L2 中 Match lane 實際的送出節奏。
- **11** pv7：契約文件 `docs/object_fps_pvp/protocol-v7.zh-Hant.md`；ProtocolVersion 6→7（ClientVersion 與 RuntimeVersion 都由它導出）；runtime link 每秒 1 次心跳（偏移、RTT 最小值濾波、漂移視窗回歸，每 10 秒寫進兩端日誌）；Client↔Gateway 時間回聲（D35）；版本不一致時給明確的錯誤；驗收工具升 pv7。紀錄若需要 wire 上的資料，併入本批，不做第二次 wire 變更。
  - **輸入：runtime link 的丟棄回報**（D49 的第二階段；規劃依 D45 用 ultracode，開始前先問）。第一列的建議做法是「轉送身分，不轉送內容」：把被剔掉的序號隨下一筆 IPC 訊息帶給 Match，Match 用自己的時鐘就能算出晚了多少，沿用 `MatchRuntimeHost.cpp:104-112` 的邏輯，不需要跨機器對時；若要回報 Gateway 的收包時刻，才要依賴本批的心跳對時。回報要有界（每個視窗計數，細節只取前 N 筆或取樣）。
    - **提醒（推論）**：讓 Match 看見網路遲到之後，late 修正會變多；在現在的常數下 D44 的風險會上升。所以丟棄回報要和 late 修正（第 09b 批的 B2）、backlog 門檻（105 從 v3 沒改；v7 的穩態排隊是 v6 的兩倍）一起決定，不能單獨改。
    - `network-architecture.zh-Hant.md` 的正式條文（「Gateway 職責」一節）在本批的契約規劃時寫（待寫，現在不改），以 PEP／PDP 的名詞寫（D49）。
    - **依 D49 的判斷規則重新檢視 v7 加進 Gateway 的狀態**：第 06、08、08b 批加的狀態與規則逐項分成「UDP 可靠性的接縫，屬於執行」或「語意判斷，應交回 Match」。例如：第 06 批的結果送出資格與 I/2 容許（08b 已移除）、第 08 批 link 的嚴格間隔與期限計時器（`nextSend`、`nextActionDeadline`）、第 08b 批的退休只送一次、poke、`retiredSent`／`retirementAsked`（D46①）、結果帳本與 `lastResolved` 的副本。
- 產品的 `-fexperimental-library` 在最後一個 `std::jthread`／`std::stop_token` 使用者遷移完時移除（`apps/object_fps_pvp/CMakeLists.txt:30-32`、`tests/object_fps_pvp/CMakeLists.txt:187`），並加守衛（產品、probe、產品測試中 0 件）。預計在 P3。

### P4：SDL 隔離（任務 7）

- IP-3、IP-4（Engine）：見輸入與呈現計畫。
- **12**：main、PvpApplication（移除 `SdlPlatform&`／`SdlGpuRenderDevice&` 的公開暴露）、7 個 probe 檔（gui_main、gui_quad_main、gui_input、action_short、player_short、native_window、platform_fingerprint）、產品測試（PointerCaptureCharacterizationTests、PlayerPresentationTests）改用 Engine API；SDL_Delay 改用 Waiter；SDL_Log 經 facade 寫進 LogFile。完成條件的 CTest（只在選擇本產品時啟用）以符號判定（D33），例外清單從空開始。L2 在同一場次交錯跑 P4 的 base 與頭的 GUI 短測；L3 確認大廳的文字輸入與剪貼簿。

### P5：解析度與 FPS 的選擇（任務 6）

- IP-5（Engine）：見輸入與呈現計畫。使用者目錄與 `engine/io` 的原子寫入依 D41 移到 v8；其餘項目在 P5 開始時依「只有命令列選項」重新核對（例如執行中改變大小是否仍需要）。
- **13**：玩家以產品 Client 的命令列選項選擇解析度、視窗模式與 FPS 上限（D41；例如 `--resolution 1920x1080`、`--window-mode`、`--fps <上限>`）。選項名稱、允許值與預設值在批次開始時提案確認；顯示器、像素密度、vsync 是否也做成選項，同時提案。指定的值不能用時退回預設並記一行日誌；選擇的值寫進啟動日誌。限幀的機制放在哪一層（產品，或 Engine 的 `RuntimeLoop`）在開工時依當時的程式決定，v8 的設定 UI 共用同一個機制。HUD 依解析度縮放；移除寫死的 1280×720。設定 UI 與 `settings.json` 在 v8（D41）。L1 包含選項的解析與限幀的節拍（注入時鐘）；L3 包含 30 FPS 的手感（D41；使用者實際遊玩，保留日誌與 movement trace）。

### P6：音效（任務 5）

- AU-1（Engine）：P6 開始時建立 audio 計畫夾。中立混音器（PCM16、48 kHz、自己的樣本計數、即時安全的有界 SPSC、依時間戳換算樣本位置）＋SDL 後端（`SDL_OpenAudioDeviceStream` 的 callback）。
- **14**：在事件真正發生的地方以 Engine 時間戳觸發：本機射擊（輸入事件的時間戳）、命中確認、受擊、換彈、遠端射擊。L2 以 `SDL_AUDIO_DRIVER=disk` 在 30 FPS 連射，分析起音間隔要對應射擊 Tick，而不是 33 ms 的幀格點。

### P7：整合與升格

- **15** LAN 場次（事前宣告，只記錄）：Windows Client 的模擬步晚醒（含縮小與遮住）、Windows 切換視窗時的 Held、Mac Gateway↔Windows Client 的偏移與漂移（至少 15 分鐘）、Gateway 各通道在真實網路下的分布、任務 8 的清單（只看日誌能否回答 v6 LAN 的問題）；朋友能主持時加測 Windows Match。
- **16** 整合驗收與升格：CTest 全部、權威 35／35、Match 的連結閉包不含 SDL 與 audio、產品移除檢查、25 案矩陣（clean-30 是否計入判定在開始時依 C1 決定）、短測、quad、1 GUI＋3 bot、L3 清單、STABLE_BASELINE v7；是否發行由使用者決定。

## 未結事項

- **Gateway 與 Match 重複過濾已解析的命令**（2026-10-10 使用者要求記錄）：Gateway 在 `gateway/server.go:420-423` 略過 ≤`lastResolved` 的命令，`lastResolved` 在收到 snapshot 時更新（`:520-534`）；Match 的 host 在 `MatchRuntimeHost.cpp:88`、`PvpMatch` 在 `PvpMatch.cpp:78-81` 也只收下 >`lastResolvedCommand` 的命令。這是協議契約定下的（`docs/object_fps_pvp/network-architecture.zh-Hant.md:230-233`：「已完成／舊 epoch 不重執行」），平常無害；副作用是 Client→Gateway 路上遲到的命令被 Gateway 吞掉，到不了 Match，也不產生相位樣本（D48 第②步在 client→Gateway 注入短停頓，0／5 跑次觸發 late 修正）。要不要改屬於協議契約的決定，另立，不在 09a／09b 順手改。另有第二道過濾：`runtime_link.go` 的 `acknowledge()` 在 snapshot 經過時剪掉 link 待送視窗中序號 ≤lastResolved 的命令（`:192-197`），所以只拔 `server.go` 那一行，遲到的命令仍可能在這裡被剪掉。原則見 D49（丟棄的決定權與知情權留在 Match）。
  - 加註（2026-10-10，D51 ⑨，使用者核准）：「平常無害」是在截斷的資訊流上的判斷。這個過濾讓 Match 的遲到量測幾乎看不到 Client 端的網路遲到，是 2026-10-10 事故的根因（[事故紀錄](Incident/2026-10-10-gateway-silent-drop.md)）；處理見 D53 的丟棄回報計畫。
- 「時鐘到網路路徑」系列紀錄（2026-10-10 使用者要求）：v7 完成後（第 16 批之後）撰寫，分 5 集：①時鐘、②命令脫離畫面幀、③網路路徑的節拍、④FireGate 與 C2、⑤跨機器的時間。附各批的 commit id、L2 數字與調試過程（含更正）。讀者與存放位置屆時決定。
- **（嚴重度高，D44）偶發的 movement epoch 重設**：（第 08b 批草案已回答：08b 不直接改變輸入抵達與 backlog；可能機制是約 30～150 ms 的短停頓造成成對遲到，Client 相位前移 2 Tick，30 Tick 合計 ≥105，屬推論，見[第 08b 批](08b-event-driven-results.md)「對 movement 佇列與 backlog 的影響」。第 09 批規劃要回答是否影響常數的凍結。）第 08 批的開發跑次中 `run_network.py`「an application stall reset the movement epoch」1 次、`backpressure_probe.py` host-ipc-250ms「Unexplained epoch reset … (reason backlog)」1 次（重設前 sim 角色的 generation 間隔縮成 13.3 ms）。和本批之前的 probe 交錯跑分不出新舊（見[第 08 批](08-client-worker-asio.md)「實作與 L1」）。使用者懷疑的方向：多執行緒造成的堆積（backlog 判定是 30 Tick 內排隊命令合計 ≥105）、Gateway 頻率的修改（第 06、08 批）。處理：各批的開發跑次記下新舊的失敗次數；比例變高或舊版也出現時停下調查；v7 完成後（第 16 批）做整體回歸。失敗的跑次保留在 `pvp-v7-batch08-20261009/`。
  - D49 第二階段（2026-10-10 使用者要求記錄）：D49 第二階段會讓 Client 端網路遲到開始觸發 late 修正，D44 的暴露會上升；第 16 批的整體回歸要在 D49 第二階段之後，或明寫它涵蓋的範圍。（依據：D48 第②步的試跑，Client→Gateway 的短停頓 0／5 跑次產生 late 修正，原因是 Gateway 略過 ≤`lastResolved` 的命令；第 09 批的重推條件 11。）
- probe 幀格點的相位（D46⑦）：action probe 的起點綁在 join Wait 之後收到 snapshot 的時刻（`gameplay_action.hpp:109`；Wait 每 2 ms 輪詢，`action_main.cpp:79-82`），超時時重新錨定（`:170`）。所以建置不同時，probe 幀相對 Match Tick 的相位會系統性地移動，b、a＋b、`legal_match_p95_ms`、d 不能跨建置比較（第 08b 批「b 段變長的調查」）。起點隨機化留到第 09 批規劃時決定。
- 系統時鐘（主機計時狀態）與後段變慢（2026-10-09 使用者提出）：第 07 批之後做了橫向對比（[第 07 批](07-match-tick-ipc.md)的「橫向對比」）。Match 端的原因（8 ms 狀態下舊 Match 的晚醒與補步）已由第 07 批消除。剩下：clean-30 的 `legal_match_p95_ms`／`legal_client_p95_ms` 在 8 ms 狀態仍偏高，來源可能是 Client 的 worker 輪詢或 probe 的 30 FPS 幀節拍；第 08 批之後用同樣的分組再對比一次。第 08 批 L2（2026-10-09）的 clean-30 全部是 4 ms 狀態，無法對比；之後的量測出現 8 ms 狀態時再補。第 08 批的評估更正了 clean-30 的部分：它的快慢兩群來自 30 Hz 節拍的相位與 probe 的幀量化，不是主機狀態（見[第 07 批](07-match-tick-ipc.md)「橫向對比」的更正）。
  - 更正（2026-10-09，核對）：「Match 端的原因已由第 07 批消除」成立：新 Match 在兩種狀態都沒有晚醒、Tick 間隔最大 16.8 ms，clean-60 的 `legal_match_p95_ms` 8.8～11.9 ms；機制是舊 Match 的 Tick 晚到，而不是補步。本條中段「clean-30 在 8 ms 狀態仍偏高，來源可能是 worker 輪詢或幀節拍」已由末尾的更正推翻（舊 worker 的送出側在 8 ms 狀態也沒有變慢，a 的 P95 3.0～3.6 ms）。仍未結的只有：新 worker 在 8 ms 狀態下沒有樣本。另外，主機狀態以 probe 連線前 3 秒的 TimerBaseline 判定，第 07 批 L2 有 1 次在跑次中轉換而被分錯（見第 07 批「橫向對比」的補充更正）；舊 Match 的跑次可以改用 Match 自己的晚醒分布判定。
- C1 之後要決定的事（C1 已在 2026-10-09 完成，現在待使用者決定）：
  - 相位追蹤的既有問題（第 03 批的 xhigh 審查；第 03a 批 clean-60 的延遲中位數約晚 1 幀，方向相同）：settling 期間收進來的舊樣本留在下一個視窗，修正後第一個視窗的 P90 實際約 P93；settling 期間累積的 late 樣本，會在 settle 完成時立刻觸發第二次 late 修正。
  - 延遲的位移（第 04 批記錄，C1 證實）：常數不變時，v7 的「產生→執行」約 38 ms（30／60 FPS 相同），v6 是 21～25 ms；「輸入取樣→執行」約多 1 幀。
  - 這兩項都要改相位常數或相位追蹤的定義，屬於停止條件，要另立批次；候選是第 09 批（FireGate 重估）。
  - 2026-10-10 使用者決定（D47⑤ 的更正）：這兩項在 P2 處理，排在第 09a 批之後、M0 與常數凍結之前（暫稱第 09b 批）。
  - D30 門檻可能被逐漸變長的發布間隔逐級放大（低機率）：使用者決定 C1 之後再看（見[第 04 批](04-client-roles.md)）。
- C1 的 8 ms 狀態未驗證（第 2 次 session 中 after 沒有落到 8 ms 狀態）：之後的量測出現 8 ms 狀態時再補。
- TT-1 L2 的量測程式在本機分支 `claude/tt1-l2-wake-record`（`a8f9535`，未推送）：為了不讓 C1 作廢，沒有放進 PR #71；之後的 PR（例如 TT-2）再合入。
- 第 04 批 xhigh 審查留下、只記錄的項目（時間倒退的意圖、網路 probe 不再涵蓋伺服器端的輸入逾時、世代檢查與送出路徑沒有 L1、負向 slew 時 FireGate 偏保守）：見[第 04 批](04-client-roles.md)的「xhigh 審查」。
- worktree：`../GYO-Engine-v6final`（`fee92ff`）保留給第 09 批 C2 的 before。
- Spaces、縮小時的斷線可能來自 App Nap（任務 1 解決不了）：第 05 批的 L3（2026-10-09）沒有重現，模擬角色也沒有 ≥100 ms 的晚醒。之後若重現，提出程序活動宣告作為新的 Architecture Delta。
- Windows Match 的 Tick 與 IPC 精度從未量過；Match 不連結 SDL，所以 SDL 調高計時器解析度的效果不適用。朋友能主持時在第 15 批量，否則標「未驗證」。
- `ClientConnection` 關閉時最多約 3 秒的阻塞（httplib），維持已知限制。
- 自旋（期限前忙等）：2026-10-09 使用者決定目前不加，現行的分離執行緒已經夠用。等第 07 批（Match Tick 改用 Waiter）與第 15 批（Windows 退回一般 waitable timer 時的精度）的數據再評估；要加就是 Architecture Delta，先量測再決定。
- 驗收分析器 `quad_evidence` 與 `command_evidence` 的收斂：維持候選。
- v6 文件中其他舊的行號（D20 的 `runtime_v5.proto`、D21 的 `IpcHost.cpp:267`、v6 交接延後項目 8 的 `backpressure_test.go:102-158` 等）：只列在[盤點](INVENTORY.md)，不修改（AGENTS §11）。

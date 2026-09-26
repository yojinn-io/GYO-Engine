# PvP v3 命令時序、低延遲與積欠恢復

狀態：實作與自動驗收完成。release版本的CPU、Go race、worker、GPU、網路、
故障恢復、30／144FPS GUI、三輪延遲及兩組完整30分鐘長測皆通過。
最終跨視窗可見P50為45.02–48.13ms，P95為45.75–48.75ms；人工試玩另確認手感。使用者已批准提前量耗盡重設及首次完整窗口啟動。以下保留
各階段成功與失敗，不以舊候選結果替代正式版本。所有時間比較皆為同機單調時鐘，

呈現時間是 Renderer 成功提交後的觀測，並非 input-to-photon。

## 原因與證據

v2 的三個中立命令會形成持續序號差。Client 與 Match 同為 60 Hz，初始差額不會
自行被消耗；它是持續延遲成本，不能只歸因於 30 Hz 傳送、20 Hz Snapshot 與
50 ms 遠端插值。未確認窗口還包含 ACK 返程與取樣等待，不能直接拿它當 Match
實際積欠。新的積欠觀測使用 Match 游標後「連續已收到、尚未執行」的命令數。

保存 v2 原始碼、靜態庫、執行檔、生成協定與既有驗收後，僅補唯讀 trace 及
Presented 觀測，使用同一組 120 秒／200 個預先安排移動事件量測基線：

| 量測 | v2 基線 |
|---|---:|
| 命令產生 → 首次送出 P95 | 18.65 ms |
| 命令產生 → Actual P50／P95 | 55.94／63.44 ms |
| 原始命令 Actual 比例 | 100%（14,400 個非播種命令） |
| 跨視窗位移交越 P50／P95 | 108.24／115.49 ms |
| 呈現事件配對 | 200／200 |
| 兩視窗實際平均 FPS | 約 59.70／59.70 |

基線位置交越缺少 v3 的命令區間欄位，使用預先安排的幾何事件區間配對並明確
標為 fallback；沒有把未配對事件排除。本次基線全部配對成功。v2 的 Sent 診斷
加在原有 send 呼叫後；原實作會吞掉 would-block，因此此指標限於這次乾淨本機
跑次，不代表 v2 已具備精確傳送失敗觀測。v3 只在實際成功 send 後記錄。

## 已實作的修正

- 固定兩步 neutral lead；Authority、Client 固定步進、worker 傳送與 Snapshot
  均為 60 Hz。窗口 12、Match 未來數量與距離 32、完整 datagram 1,200 bytes。
- `ACK == tip` 正常完成確認，保留累積器；只有超前或新 epoch 才播種。新播種
  首次 Advance、空 pending 且 frame gap 大於 50 ms 的情況限制為一步，避免
  重播權威已涵蓋的停頓。一般 30 FPS 每幀仍可產生兩步。
- v3 兩份 Protocol、C++ 型別、Go bindings、join／Ready／Welcome 與 adapters
  同步升級。epoch 由 Match 推進、每世代 sequence 從 1 開始，不回繞。衝突未完成
  命令整批拒絕；重送不多走步數、不刷新替代輸入期限。
- Match 每 Running Tick 完成一序號，Actual／Held／Neutral 都推進；Held 只取
  最近實際執行的真實命令，最多 15 Tick。最近 30 Tick 的連續未來命令總和達
  105、冷卻 60 Tick 時，下一個邊界換 epoch、保留 pose 並等待新序號 1。
- Worker 原子 Drain 連線狀態與最多 64 筆接收時間戳快照。遠端固定斜率時間線
  落後一 Tick，線性位置／最短角度 yaw 插值，無外推；新玩家 epoch 只切自己的
  區段。停止主執行緒後選目前區間，不逐幀播放舊快照。
- 尚未開始的完整 Snapshot 可合併，已部分寫出的 TCP frame 完成或失敗關閉。
  沒有宣稱能撤回 OS buffer 中的資料。世界模擬不等待 socket。
- 診斷預設關閉、有界，獨立執行緒寫檔。溢位／截斷／不合法記錄不能當成功。
  Presented-only 觀測包含插值區間、位置、校正、保持姿態與略過幀；Skipped
  不形成新的呈現或 hold 狀態轉換。

兩步約 33.3 ms 是序列提前量的成本，但實際「產生到執行」仍取決於首批、幀、
Authority 與傳送的相位，不能直接加常數當成呈現延遲。先前單步候選在抖動下
Actual 比例不穩定，因此沒有為達到量測門檻擅自切換單步。

105／30等於平均3.5個已收到的連續未來命令；它是本輪固定的異常積欠門檻，
不是Client pending或RTT的換算。正常phase與30FPS批次必須在此門檻下成立，
而非測到重設就放寬數值。冷卻60Tick限制連續重設，Awaiting seq1不採樣；
提前量耗盡則使用同窗sum0＋曾替代＋整個future map空的獨立條件。兩者分別
處理過多與耗盡，沒有按封包數推進世界，也沒有自動變更lead。

## 驗收抓到的額外問題

### 完全確認後仍被舊傳送期限擋住

初版 v3 在 30 FPS 真 socket 短測只有 94.972% Actual。已全確認的舊窗口最後一次
重送仍留下 deadline，新的命令可能在 Authority 執行前 0.7 ms 才發送、晚於
執行後才進 Host；直走時 Held 會掩蓋這個問題。

修正為：ACK 清空窗口後回到「無可用窗口」狀態，下一個非空窗口立即開始新的
60 Hz 傳送排程。部分 ACK 不重置 deadline，也不對每個新命令加發封包。
真 ClientConnection／loopback 回歸量到約 2 ms 的重新啟動、部分 ACK 保留約
16.7 ms 間隔，60 FPS 即時確認仍為每秒 60 包。修正後 30 秒／30 FPS 為
3,600／3,600 Actual，首次傳送 P95 1.97 ms，Actual P50／P95 11.69／28.23 ms。

人工排程測試亦修正初始化因果：不能憑空使用尚未發布的 Tick 0 Snapshot，
又讓同一邊界消耗 seq1。由實際 Snapshot → worker → render 啟動後，162 個
30／60／144 FPS、相位、同時事件先後與首幀 elapsed 0／不足一步組合皆通過。
這是測試模型錯誤的修正，沒有增加第三個 lead。

### RTT 40 ms 的長停頓恢復：已批准的契約補充

實際受控網路 RTT 40 ms／抖動／5% 掉包在六秒主執行緒停頓後出現無法移動。
產品類別的獨立事件排程重現進一步排除丟包與抖動：RTT 40 ms、對稱 20 ms、
零丟包時，30／60／144 FPS 停頓後最後兩秒均為 0／120 Actual；相同設定無
停頓皆為 120／120，RTT 0／20 ms 的恢復亦通過。

原因是 Running 游標不停前進，收到的 ACK 已舊了 20 ms，再補兩步（33.3 ms）
並送回還需 20 ms。可能只重播種一次就維持 `tip − 即時游標 = 1`，每個新命令
卻都晚於自己的執行槽，永久 Neutral。這不只是重送頻率或反覆播種問題。

原計畫同時固定兩步、要求 RTT 40 ms 停頓恢復、又只允許積欠觸發 epoch 重設，
在這個情況無法同時成立。第一個隔離候選「連續 30 Tick 無 Actual 且窗口空」
能修復完全停止，但 250 ms 停頓後可能持續只有一半命令 Actual，零星 Actual
會一直清掉連續 missing 計數，因此這個候選不完整。

更新的最小候選使用既有 30 Tick 窗口：**連續待執行數總和為 0、同窗曾發生
fallback、且整個未來命令 map 為空**，在 60 Tick 冷卻允許的下一邊界換 epoch。
沒有調整兩步 lead，也不是按 Actual 百分比自適應。它識別已耗盡的序列提前量；
當每步仍為 Actual 時，即使 queue 為 0 也不重設。

隔離副本 486 組涵蓋 30／60／144 FPS、RTT 0／20／40 ms、108／250／6,000 ms
停頓、首兩包丟失／抖動／5% 掉包、各三個 worker／Authority 相位；最後干擾
解除後，250 ms 連續 Actual 區間最慢約 509 ms 開始，1.5 秒後無持續 fallback。
最多兩次重設包含首包造成的積欠；等待 seq1 時不循環重設。

使用者已明確批准這個額外重設條件，正式 source 已套用。正式
`MovementRecoveryTests.cpp` 的 486 組矩陣另外加入每次重設首兩份 Snapshot
遺失及延後重複命令，全部通過。舊的連續 missing 提案已被取代；隔離候選
與正式測試、真 socket 結果仍分開保存。

### GUI 幀內停頓被重複計時

`pvp-v3-gui-latency-worker-fixed/round-2` 出現一次 backlog reset，該輪判定失敗。
Player2 的兩次呈現實際間隔 64.268 ms，但第一幀在事件處理前已取到
16.734 ms 的 FrameContext delta。幀內等待結束後，Reconcile 已收到
ACK659 > tip658，播種660／661並產生662；約1 ms後的下一次 Update 卻又收到
48.389 ms 的舊 frame delta，額外產生663／664。這把權威已涵蓋的停頓再次
算入本機積分，造成持續增加的提前量，最後碰到105／30積欠門檻。

修正放在產品 Application：在 RefreshState／資產工作後、實際 Advance 前以
steady_clock 取樣兩次呼叫的間隔，生命週期清除取樣點。domain predictor 仍只
消費傳入 elapsed，不取得時鐘；Engine 的 FrameContext 不變。測試同時覆蓋
ProcessEvents → Update 與 Update → Render 之間的 probe-only 停頓。
此根因是命令多生，不是把 GPU／OS 整機無響應宣稱已查明。

### 完整ACK將正常插值誤當校正

正式程式的額外review以純產品probe重現：在alpha=0.5、authority位置完全等於
predicted seq3時，`ACK==tip`把previous／current都設為authority，原本正常的
相鄰插值差0.025世界單位被當成校正。重複完全正確的ACK後，到seq10竟累積
−0.115115的顯示校正，display2.29448而同命令無ACK的正常插值為2.4，平白
落後約35ms並趨近50ms。沒有真實權威誤差也會觸發，違反相同序號／預測終點
比較誤差的契約。

修正保留舊previous所代表的命令序號；若ACK已涵蓋它，按重播終點的真正位置
差平移這個相鄰狀態，使新舊base對應相同兩個命令。Authority tip、命令產生、
Match步數不因插值修正改變。補測完整／部分ACK、frozen、真誤差、牆壁與硬校正。

另外將遠端「缺少未來Snapshot」和「正在移動卻只能保持姿態」分開觀測：靜止
pose不再累加動作停頓次數；新epoch僅一點時不推斷運動。此修正不改呈現游標
或位置，只修正診斷含義。

## 各階段量測與失敗紀錄

初版 v3 三輪 GUI（每輪 120 秒、200 事件、2 秒暖機／尾端配對）：

| 輪次 | 命令 Actual P50／P95 | 跨視窗 P50／P95 | Actual／呈現配對 |
|---|---:|---:|---:|
| 1 | 34.56／42.16 ms | 43.38／43.89 ms | 100%／200:200 |
| 2 | 36.53／49.55 ms | 38.89／39.44 ms | 100%／200:200 |
| 3 | 34.97／42.85 ms | 44.78／45.35 ms | 100%／200:200 |

以上為中間版本，最終結果見下方正式驗收。慢幀沒有排除，計時沒有 GPU readback 或存圖。位移交越
使用 epoch／命令插值區間辨識事件，未配對或不明者以正無限延遲進分位數。
命令與呈現兩種資料分開判定；只有同時通過才算整輪通過。

中間版本六種實際阻塞（Host IPC／Gateway／下游，各 250 ms／1 秒）均通過：阻塞期間
仍分別執行 15／60 Authority Tick；解除後約 53–59 ms 開始恢復、約 304–325 ms
完成至少 250 ms 穩定觀察。穩定區間要求兩玩家當前 epoch 新產生的命令持續 Actual、
Match 30 Tick 真積欠總和低於 105，並檢查預測窗口／遠端接收時間線。

IPC proxy 測到 3-byte 部分 header 及後續 7-byte 分片的 framing，但一秒正常
小型 Snapshot 流量可能裝得進 kernel buffer，故**沒有證明 Host kernel send
buffer 已滿或真的發生部分 write_some**。Go 阻塞 consumer 測試另確認應用佇列
有界、控制順序及 latest-wins。目前 frame age 指當前尚未完成版本的年齡；
零bytes寫出而持續被替換的版本不累積同一episode年齡，外部故障解除到恢復的
時間由probe另量。此證據邊界明確保留。

最初兩組 30 分鐘長測保留版本指紋，不能當成修正後證據：

- 60 Hz：216,000 個非播種命令，213,183 Actual、2,817 Held，Actual 比例
  98.6958%，**未通過 99% 門檻**；零 epoch 重設。前24分鐘131個Held，最後
  6分鐘2,686個，反映完全ACK後未解除舊worker deadline的時相問題。
- 144 Hz：216,000／216,000 Actual，零 epoch 重設／診斷遺失，通過該舊版本
  長测。但它仍早於上述修正，不替代最終版本驗收。

修正 worker 排程與 starvation 後的 60／144 Hz 各 30 分鐘重測均通過，結果與
版本指紋另存；之後又完整重跑呈現修正版。這些仍早於首次完整窗口啟動補充，
因此最終正式版本再各運行 30 分鐘。

期間以60秒唯讀 /proc 採樣，診斷開啟時：Match約單核心1.9%、Gateway約3.2%、
雙Client headless probe約2.5–2.7%；RSS分別約5.6／16.3–16.6／8.5–12.4 MiB。
這不是GUI GPU使用率，也不是關閉診斷的成本基準。

bootstrap 補充前，固定RTT0／20／40ms × 108／250／6,000ms停頓的九組真socket測試，
指定恢復窗口全部通過：33–550ms開始恢復，連續250ms各玩家15–16個新Actual，
queue30sum最高43；六秒停頓期間Authority持續360步。RTT在解除停頓後仍保留。
原固定RTT代理卻讓Hello／Welcome立即通過：共享UDP序號使較晚的保活超越
在途移動封包，導致後者被當過期丟棄。它其實額外注入週期性亂序，造成部分
案例故障前後的starvation重設。原證據保留；修正代理讓所有UDP同等延遲，
另加Hello／Welcome不能超車的回歸，再以`pvp-v3-final-recovery-fixed-rtt`
完整重跑。沒有為此修改產品或公共Session邏輯。

中間版本本機GUI的三輪（完整ACK插值修正後、bootstrap補充前）可見P50／P95為23.675／24.288、
35.244／35.830、48.704／49.288ms，各200／200配對，但每輪仍有一次starvation
重設，**不能列為乾淨通過**。第一輪player2 seq4241僅在執行槽前0.970ms產生，
0.201ms前送出，Host在槽後0.504ms才收到；worker的0.769ms反應沒有拖慢。
前後Advance隔29.708ms，是主執行緒的單幀延後。重設後88us fresh-seed掉時是
結果，不得反過來作為干擾原因。後續隔離測試確認elapsed=0時先送neutral窗口
會耗掉原定提前量，促成下節經使用者批准的首次完整窗口啟動。固定兩步與其他
門檻不變。

診斷分析器另補false-pass回歸：每五步只有一步Actual不能叫連續恢復；越界queue
／pending、缺少HostAccepted／Sent、倒置時序不能通過。長Presented空檔不內插
虛構交越；未知事件仍以無限延遲計入分母。後來的分析器以原始完整trace重算，
不需要把失敗資料刪掉或重新生成。

UDP流量參考：校正代理RTT0／108ms完整session共4,623個datagram、253,671 bytes
（Input92,399、Snapshot155,632、Control5,640），最大90bytes。名義19秒約
13.04KiB/s，是含啟動／故障／關閉的雙Client雙向應用層估算，排除UDP/IP/link
headers；未保存精確relay起止時間，所以不稱精確clean throughput。

RuntimeGap／完整ACK顯示修正後的`pvp-v3-final-presentation-soak-*`兩組30分鐘
皆完成：100% Actual、零重設／干擾。它們仍早於bootstrap補充，保留為階段證據。

## 已批准的首次完整窗口啟動

在首幀elapsed=0或不足一步時，原來立即發布neutral1／2，Match可能先消耗它們，
等第一個合法active3產生時，真正時間餘裕只剩約一Tick。密集相位測試也找到
30FPS持續交替晚到，而不只是偶發GUI慢幀：queue在0／1間交替，既不是積欠，
也不滿足整窗zeroqueue的starvation條件，因此不能靠重設門檻掩蓋。

使用者已明確批准補充首次發布規則：在新epoch且ACK=0時，保留兩個中立命令，
等第一個合法固定步形成後才發布完整未確認窗口。30FPS該幀若合法產生兩步，
一併包含它們；沒有多造Tick或增加neutral數。Running中ACK>tip但cursor>0的
播種仍按原來規則發布。這是明示的bootstrap發布條件變更，並非調低驗收標準。

隔離候選用正式product library測972組恢復矩陣（首幀0／2ms），全部通過，最慢
600ms開始穩定恢復。更密集的phase sweep下，舊30FPS有48／864個相位持續
fallback；舊144FPS加29.7ms gap有93／864個fallback、62個reset。候選這些案例
皆為零，但確實提高持續generation→execution的時間餘裕與成本；不能稱免費修正。

第一輪隔離GUI120秒／200事件：200／200配對，14,401／14,401 Actual，零Held、
零reset、零干擾。可見P50／P95為46.577／47.010ms；命令執行34.955／44.550ms。
原始source diff、link commands、正式檔案未修改證明及候選SHA256見
`pvp-v3-bootstrap-candidate`；正式採用後已重新建置，下節使用正式執行檔。

兩組此前的30分鐘命令路徑長測已通過：60Hz為215,999／215,999 Actual，
144Hz為216,000／216,000，零重設／診斷遺失／干擾；執行P50／P95分別為
29.700／29.744、26.398／29.225ms。它們早於bootstrap補充，不作為最後版本的
長測替代。所有長測的manifest與失敗GUI均保留。

## 採用兩項補充後的驗收（最後邊界修正前）

正式 `pvp-v3-approved-*` 使用重建後的產品執行檔及嚴格分析器；manifest 保存
SHA256。驗收期間沒有改產品程式、門檻或 lead。GUI 觀測先寫入有界記憶體，
完成計時及兩秒配對尾段後才輸出檔案；溢位為失敗，期間不存圖／GPU readback。

三輪各 120 秒、200 個預先安排事件，分別通過：

| 輪次 | 首送 P95 | 命令 Actual P50／P95 | 跨視窗 P50／P95 | Actual／配對 |
|---|---:|---:|---:|---:|
| 1 | 15.99 ms | 30.81／38.43 ms | 40.33／40.73 ms | 14,400:14,400／200:200 |
| 2 | 15.91 ms | 38.08／45.61 ms | 46.89／47.64 ms | 14,400:14,400／200:200 |
| 3 | 15.94 ms | 40.46／48.03 ms | 49.45／50.03 ms | 14,400:14,400／200:200 |

三輪均零 epoch 重設、Held／Neutral、診斷遺失及干擾。名義 60 FPS，兩視窗實際
平均 59.75–59.78 FPS；各輪最大 Presented 間隔分別為 26.03／22.13／33.15 ms，
全部保留，無 Skipped。積欠 30 Tick 總和最高 60，未達 105。不能把三輪合併或
挑最快一輪來聲稱固定 40 ms，亦不把成功提交 Renderer 當成螢幕掃描輸出。

| 正式測試 | 結果／證據 |
|---|---|
| CPU／建置 CTest | 31／31；PvP 63 cases、1,383,577 assertions |
| 稠密正常相位 | 1,296 組，30／60／144 FPS、72 相位、兩種同時排程、三種首幀 elapsed |
| domain 故障恢復 | 972 組；首幀 0／2 ms、重設首兩份 Snapshot 遺失等 |
| Go race | 產品與公共 Gateway 通過 |
| Shader／GPU／真 worker | 3／3、1／1、1／1；GPU 正確性獨立於計時測試 |
| 固定 RTT 與停頓 | 9／9；33.44–566.78 ms 開始恢復，300.11–816.78 ms 完成連續 250 ms |
| Host／Gateway／下游阻塞 | 6／6；53.19–75.23 ms 開始恢復，303.20–341.89 ms 完成穩定區間 |
| 抖動與丟包、生命週期 | RTT 0／20／40 ms、單向抖動 10 ms、5% 隨機／首包／連兩包遺失全通過 |

恢復判定要求解除干擾後**新產生**、當前 epoch、連續 Authority Tick 的 Actual，
兩玩家均持續至少 250 ms，同時預測窗口未滿、積欠總和低於 105、遠端時間線追上。
沒有把單次 Actual 或 Held 中偶發 Actual 算恢復。九組固定 RTT 期間所有 UDP 類型
同樣延遲；故障前及恢復後 1.75 秒以後均無重設。最差 Authority 間隔 17.15 ms；
六秒停頓和一秒阻塞仍持續分別模擬 360／60 步。窗口／佇列邊界全程驗證。

測試清單另包括停止／恢復、六秒主執行緒停頓而 worker 保活、連續離開重入的
玩家身分與 Lobby 計數、取消 join、失敗 leave 清理屏障、destructor、IPC 失敗
雙 Client 回 Lobby 及替換 Gateway 後重新加入。真實網卡 LAN 尚未實測。

最後邊界修正後會使用獨立的 `pvp-v3-release-*` 重新驗收，不覆寫本節證據。

## 最後邊界稽核

### 尚未發布的啟動時間積欠

`pvp-v3-approved-phase-30` 六個故障階段均能恢復，但額外的暖機稽核發現一次
故障前 backlog reset，不能列為乾淨通過。Player2 第一次 Advance 為 0，保留
seed1／2 且未發布，卻已清除 freshSeed。首次世界繪製約 48.7 ms；兩次實際
Advance 相隔 51.008 ms，因此第二次產生3／4／5。首個送出窗口為1..5，Match
Tick50起 queued4／3 交替，30次剛好累積105，Tick80重設。沒有100ms長幀或
模擬掉時，不能事後把這次重設歸類為受干擾跑次。

只檢查單幀是否超過50ms也不完整：先15ms不足一步、再49ms，或0／7／7／49ms，
同樣會在首次發布前累積三步。修正針對**尚未發布、ACK0、僅有兩個中立命令**的
bootstrap：以固定步進器的值副本預估這次步數，若會一次產生三步以上，沿用既有
一次最多計入1/60秒的保護並保留餘數；副本不產生命令、不推動真實游標。正常
30FPS首批的兩個合法步驟照常保留。被省略時間完整記錄，不提高積欠門檻，也不
增加neutral。這是首次完整窗口的啟動邊界修正，沒有修改Running中的正常步進。

### 觀測與交接精度

- 遠端的 raw missingFutureSnapshot 代表沒有未來資料；當每幀仍收到新位置時，
  不能把連續選擇最新快照誤記成同一姿態持續停住。早期正式GUI最大hold讀值約
  435ms，該段位置實際持續前進。最終計數改以真正Presented姿態是否重複判定，
  仍排除已靜止角色，Skipped不更新比較基準。
- Failure 發布 Lobby／新generation 與清除接收歷史／累積overflow統計必須在同一
  mutex 範圍內完成；否則Drain可能看到舊generation的累積值被清零。
- Gateway 對下一個尚未送出的peer datagram，會在前一個Write返回後重新選最新
  snapshot；不能只在取出雙peer批次時合併，再把兩份都固定為舊快照。
- 實際144FPS的GUI驗證使用既有SdlGpuOptions.vsync=false；產品composition只
  傳遞正常圖形設定，預設仍true。此前名義144的測試受VSync限於約60FPS，證據
  明確保留，不能用名義值宣稱144FPS呈現通過。

### 傳送時間的觀測區間

成功UDP send返回後若worker未立即取得排程，Host可先接受甚至執行命令，晚記錄
的Sent不能當作真正發送時刻。診斷schema2保存successful send-call的started_ns及
結束time_ns，僅成功時輸出；Generated→首次成功傳送仍用結束時間作保守上界，
不改22ms門檻。因果檢查為Generated≤Send start≤HostAccepted≤Resolved，允許
Host在send呼叫區間內接受；開始晚於結束或命令產生晚於開始仍失敗。舊schema1
明確按舊單點時間讀取，不能與schema2混寫。這不改Client／Runtime v3 wire。

稍後完成的舊schema1 `pvp-v3-approved-soak-60` 實際捕捉到這個反例：player1 的
seq22276與69717，HostAccepted分別比Sent紀錄早65,583ns／1,153,993ns。該輪
216,000個命令全Actual、零重設／干擾，但因兩筆因果鏈無法由舊資料精確證明，
仍保留為失敗，沒有事後推造Send start時間。舊144Hz跑次通過；兩者都不取代
schema2 release版30分鐘驗收。

## 最終 release 驗收

最終程式與分析器於2026-09-25 14:40 UTC凍結。`pvp-v3-release-manifest.json`
保存來源指紋，並核對各執行檔仍符合長測開始前的manifest。先前 `approved-*`
資料全部保留；以下僅使用 `release-*`。

- 完整CTest **31／31**；PvP **67 cases／1,389,737 assertions**。其中包含
  1,296組正常相位、972組故障恢復、648組延後bootstrap以及新增send區間回歸。
  分析器另有31個命令證據、13個呈現證據、2個relay及2個背壓證據測試。
- 真worker **1／1**，包含16次接收歷史overflow→Error→密集Drain→重新加入，
  舊generation不再觀察到被清零的累積值。Go完整race與GPU smoke各通過。
- 固定RTT九組 **9／9**：33.44–550.12ms開始恢復，283.44–800.12ms完成
  連續250ms驗證。六組背壓 **6／6**：54.21–70.02ms開始恢復，
  305.59–336.69ms完成穩定觀察。命令因果鏈也由相同嚴格分析器驗證。
- 完整網路／抖動／丟包／leave-rejoin／IPC故障流程通過。實際最大UDP大小與
  各種注入統計保存在network/result.json；不把快照缺口直接稱為網路丟包率。
- 30FPS及解除VSync的144FPS GUI，各6種64／83／250ms幀內停頓 **全通過**，
  兩輪均零epoch重設，命令窗口／佇列有界。30FPS的Presented中位間隔33.397ms；
  144模式為7.003ms，觀察端實際平均142.79FPS。操作端包含人工注入停頓後的
  全程平均137.63FPS，不能把停頓幀刪掉而宣稱全程144FPS。
- GUI焦點／滑鼠釋放／停止／Tab／Esc及擷取驗收通過，零epoch重設。它使用SDL
  事件注入驗證產品路徑，並不代表已人工重現視窗管理員拖曳或整機停頓。

三輪最終GUI延遲各120秒、200事件，全數通過，使用schema2的保守首送上界：

| 輪次 | 首送P95 | Actual P50／P95 | 跨視窗P50／P95 | Actual／配對 |
|---|---:|---:|---:|---:|
| 1 | 15.79ms | 36.77／46.18ms | 48.13／48.75ms | 14,399:14,399／200:200 |
| 2 | 15.94ms | 36.11／43.80ms | 45.87／46.54ms | 14,399:14,399／200:200 |
| 3 | 16.12ms | 34.72／42.91ms | 45.02／45.75ms | 14,400:14,400／200:200 |

每輪獨立達標；零重設／干擾／診斷遺失／Skipped，實際平均59.74–59.75FPS。
完整保留所有幀，最大間隔17.94ms；沒有移動中的重複Presented姿態hold、時間原點
重定位或接收歷史遺失。本機顯示校正最大9.54×10⁻⁷世界單位。命令數的單步差異
來自半開量測窗口的幀相位，符合每玩家60Hz邊界容差，不忽略分母或慢幀。
資料與逐幀統計見`pvp-v3-release-gui-latency/{results,summary}.json`。

相較v2基線108.24／115.49ms，最終三輪的可見P50降至45.02–48.13ms、P95降至
45.75–48.75ms。這是同機Submitted畫面位移交越，不是實體螢幕input-to-photon。

60／144Hz最終各運行完整1,800秒，兩輪皆**乾淨通過**：

| 驅動頻率 | 非seed產生／Actual | 首送P95 | Actual P50／P95 | 最大幀間隔 |
|---|---:|---:|---:|---:|
| 60Hz | 215,999／215,999 | 15.97ms | 45.86／45.95ms | 18.00ms |
| 144Hz | 216,000／216,000 | 15.94ms | 38.77／41.57ms | 9.55ms |

零Held／Neutral替代、零epoch重設、零診斷遺失、零模擬掉時、零窗口凍結、零
接收歷史溢位、零100ms排程間隔。積欠30Tick總和最高均為60，沒有持續增長；
記錄保留全部108,001／259,201個量測frame。幀間隔P50／P95分別為
16.667／16.707ms及6.944／6.988ms。共431,999個非seed命令全部Actual，
3,467,182筆完整schema2診斷記錄均通過因果／容量／完整性驗證。

此為headless產品網路／預測長測；GUI實際幀率及呈現延遲使用前述獨立測試。
結果見`pvp-v3-release-soak-summary.json`及各原始round.json／frames.csv／jsonl。
最後再次建置C++與Gateway，並核對二進位與驗收時SHA256一致。

## 驗收工具與證據

全部專屬 `object_fps_pvp`，不在正式遊戲加入故障控制：

- `run_timing.py`／`timing_main.cpp`：真時鐘、真 worker／socket／預測；保存所有
  frame gap、命令 trace、暖機與量測邊界、故障釋放時間。
- `command_evidence.py`：相同命令識別配對、Actual 分母、每玩家 60 Hz 產消覆蓋、
  trace 完整性、當前 epoch 恢復、掉時與接收停頓分類。
- `gui_main.cpp`／`presentation_evidence.py`：Presented-only 交越及固定 200 事件
  分母；三輪分別評估，不將多輪合併來掩蓋失敗。
- `worker_main.cpp`：真 ClientConnection 的窗口清空／部分 ACK／重送排程。
- `backpressure_probe.py`／Go backpressure tests：分層阻塞及有界恢復。
- `impaired_network.py`：保留 datagram 原文的 LAN 抖動／丟包 relay。

原始證據位於 `build/target/_build/test/logs/` 的 `pvp-v3-baseline`、
`pvp-v3-short-60b`、`pvp-v3-fixed-worker-30`、`pvp-v3-gui-latency`、
`pvp-v3-network`、`pvp-v3-backpressure`、`pvp-v3-soak-{60,144}`、
`pvp-v3-resource-cost`，以及階段版的 `pvp-v3-final-*`、正式版的 `pvp-v3-approved-*`。失敗跑次保留，不能
改名成成功或提高門檻。run-manifest記錄實際測試執行檔SHA256及參數。

### 重現入口

從repository root使用既有產品驗收腳本；每次指定新的output目錄，保留失敗資料。
`run_timing.py --gui --duration 120 --events 200 --rounds 3 --fps 60` 是完整GUI
延遲驗收；`--soak --duration 1800 --fps 60` 與 `--fps 144` 各運行實時30分鐘。
這些指令均需共同的 `--match`、`--gateway`、`--probe`、`--arena`、`--output`：

| 參數 | 建置後路徑 |
|---|---|
| Match | build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match |
| Gateway | build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway |
| GUI probe | build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe |
| headless probe | build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_timing_probe |
| Arena | build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json |

`recovery_probe.py` 使用同一組headless參數，預設九組固定RTT／停頓矩陣；
`backpressure_probe.py` 預設六組分層阻塞。`run_network.py --network-impairments`
改使用同目錄的network_probe，驗證端到端生命週期與抖動／丟包。短GUI runner
與逐案例Actual分析腳本完整複製到各release-phase/lifecycle證據目錄，可讀取
原始手動組裝流程，而非依賴不透明測試平台。

`cmake --build --preset test --parallel 6` 建置C++與部署內容；Gateway另外在
apps/object_fps_pvp執行`GOWORK=off go build -o ../../build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway ./gateway/cmd`。
CPU入口是`ctest --preset test`；worker／GPU需要桌面及loopback socket權限，
分別使用`ctest --test-dir build/target/_build/test -R '^object_fps_pvp.worker$'`
與`-R '^render.sdl_gpu_mesh_smoke$'`。Go race用產品與公共Gateway各自的module。

### 最後建置與雙 Client 試玩

2026-09-26 00:12 JST再次建置後，Match／Gateway／timing／GUI probe及Arena的
SHA256均與正式驗收一致。已啟動正式Match、Gateway及兩個Vulkan Client；兩者
皆完成graphics初始化。啟動器為`build/target/run-object-fps-pvp.py`，只管理自己
啟動的程序；關閉兩個Client後會清理對應服務。

本次試玩日誌：`build/target/_run/object_fps_pvp/20260926-001238-887787/`。
Client A使用Create + Join，Client B使用Refresh再Join Room。WASD平移，滑鼠
轉向；Tab釋放／恢復捕捉，Esc回Lobby。人工確認平移、轉向、同時操作及拖曳
標題列仍待使用者實際試玩，沒有把自動SDL事件注入宣稱為已完成的人工驗收。

## Architecture Delta

需求來自可重現的本機鏡頭及跨 Client 延遲、不可恢復的命令積欠。變更邊界限於
`object_fps_pvp` 的命令世代、確認處理、worker 排程、呈現時間線及唯讀診斷。
Client／Runtime Protobuf v3 是產品資料契約變更，三個部署角色必須一起升級。
新增驗收 target 仍由同一 owner 選取，依賴既有產品 network／domain 與 Python。

沒有新 Top-level Directory、Engine prediction framework、公共 Gateway 遊戲邏輯、
新產品間依賴或倒轉依賴。Client → Engine、Match → Engine 的方向保持不變。
單改插值或頻率無法解決不可變命令身份／ACK／停頓游標問題，因此需要這個產品內
垂直切片，而不是把責任移入 Engine。

產品可移除性實際以source副本刪除192項owner內容與registration驗證，沒有
改公共實作：7個中立Engine library與engine_tests重建，5個common CTest、
2個content移除生命週期測試及公共Gateway測試通過。兩個既有獨立copy_game
fixture皆建置／安裝／執行；現有object_fps／object_fps_v2在無PvP內容下完成
完整configure。為避免長測期間重複建置大型SDL依賴，未重新執行這兩個完整
GUI產品。證據：`build/target/fitness/pvp-removal-20260925/logs/`。

整機卡頓／拖曳標題列的歷史回報仍無足夠證據指向單一 OS／GPU 原因。先前的
焦點／滑鼠釋放／worker 保活修正與本輪命令時序證據分開記錄，不把未證實根因
寫成已查明，也不把故障恢復成功稱為整機停頓原因已修復。

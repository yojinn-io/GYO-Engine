# PvP v7.1 log-contract：日誌 Data Contract v1，凍結分析器不會靜默讀錯

日期：2026-10-20。**完成**（計畫 `caf6a81`；core `108ca0d`～`4cc2ec6`、契約文件 `0394d64`；records `a98bceb`～`71c9c76`；只記錄的 L2 `fdb94a4`；四平台 L1 通過後結案）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#82](https://github.com/yojinn-io/GYO-Engine/pull/82)（after-pv7 線）合併（`5ee499a`，2026-10-21）。
計畫與紀錄見 [log-contract](../object_fps_pvp/plans/v7.1/log-contract.md)；契約本體見 [日誌契約](../object_fps_pvp/log-contract.zh-Hant.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D142～D144。
前提 `tt2-trace`（Engine 的 `GYO::Trace` 與日誌 facade）見 [Engine TT-2 的 dev log](2026_10_20_engine_tt2.zh-Hant.md)。

## 做了什麼

- 範圍：README 的 `log-contract` 列，包含原 v7 第 10 批的紀錄（拒絕原因、snapshot 年齡、Match 的裁決與結束紀錄等）、v7 D58 的暫定項、INVENTORY 指向本批的項目，以及 `tt2-trace` 留下的 `match-shots.jsonl`、`match-ingress.jsonl` 的寫出。
- 規劃用 ultracode（盤點 2、方案 3、評審合成 1，以上 high；對抗式檢查 1 位 xhigh：MUST-FIX 7、SHOULD-FIX 10、NOTE 5）。原估 16 h，計畫重估約 23 h（約 1.4 倍），拆成 core 與 records 兩個子批（D142）。
- 契約：新增 `log-contract.zh-Hant.md`，**取代 `protocol-v7` §1 的日誌規則**。被工具解析的行族列為 A 級（18 族釘住鍵序列），其餘是 B 級、不承諾相容；破壞性變更改用新的事件名或前綴，讓凍結分析器大聲失敗、不靜默讀錯；契約有自己的 Design Invariants 與 Replacement Criteria（D142 LC-6）。
- D58 的暫定項照現狀定案（D58(1)(8)、D37），並請使用者明確確認 D58(3)(4)(5)。
- core：Client 摘要的武器計數下溢（先 Red 後 Green）；對時行由 C++ 與 Go 共用一份 golden，Client 的對時行帶 player，同一程序多個 Client 也分得出；`coalesced_snapshots` 不加 Gateway 欄位，改用測試守住守恆式；C++、Go、Python 共用一份禁用片語清單。
- 關卡（D143）：Gateway 斷線分類要新增 3 個哨兵錯誤與 1 個錯誤型別，依 Gateway 的 Design Invariants 是要使用者同意的 Architecture Delta，計畫沒有列，所以先存成 patch、不提交；使用者同意後套用（`a98bceb`），斷線行改成 `runtime disconnected reason=… error=…`，統一兩種 `i/o timeout` 字串。
- records：Match 的 Tick 超過名目週期的量與停頓次數（E1-5）、新行族 `ipc statistics version=1`（E1-7）、Client 的受擊與依原因的拒絕數、四條 role 的 `role started`（Engine 加 `RoleThread::WaitBackend()`）、Match 的 `end` 行。
- 移出的 4 項（LC-8）交給 `observability-tools`；之後在那邊漏列，2026-10-21 依「還需不需要」重新判斷（D157）。

## 驗證與經過

- core：CTest 74／74、Go 全部通過、`lc-*` 突變 9 個 killed。和計畫的偏差照實記：直接以 `log.Printf` 寫出的行沒有可呼叫的組行函式，改用原始碼層的鍵序列釘住；ingress 沒有新增逐字 golden，改由真實跑次的樣本讓凍結讀者讀。
- core 的四平台 L1 第一次全部失敗：樣本是 `*.log`，被 `.gitignore` 排除、沒有進 repo。改成 `.txt`（`70ad10d`）。
- records 的開發跑次抓到兩個錯：`action_late` 把閒置很久的 lane 的第一筆算成晚了 5.2 秒（改成從開始有待送內容起算後最大 0.17 ms）；一個守衛測試對 generator 做真假判定，恆為真。都修正。
- 只記錄的 L2（D144，沒有門檻）：quad clean-60 一輪，對 Match 做 50 ms 的受控停頓（實際 53.0 ms）。repo 版的凍結分析器讀新日誌，未知鍵 0；E1-5 的停頓量和 trace 重算的間隔差 34.4 µs（1 Tick 內）；player 1～4 的對時行都分得出；`trace_end` 的 `dropped`＝0。沒有本批造成的意外。
- 只記錄的觀察：舊版（d117 清單）的 `ingress_evidence` 本來就讀不了 pv7-line 的日誌，之後要當基準得先換成新版本。
- 四平台 L1 通過（run 39428630663，`71c9c76`），**`log-contract` 結案**（README 沒有 L3）。

## 證據

`build/target/_build/test/logs/pvp-v71-log-contract-core-20261020/`（含 Gateway 斷線分類的 patch）、`pvp-v71-log-contract-l2-prep-20261020/`（工具與開發跑次）、`pvp-v71-log-contract-l2-20261020/`（`record.json`）（git 忽略）。

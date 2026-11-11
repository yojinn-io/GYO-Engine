# 限制與修改條件

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。使用者 2026-11-11 要求：新架構文件要有一頁把**限制**（設計上刻意接受的上限或慣例）與**解除／修改它的條件**集中記錄。性質上是 AGENTS §3 的 Replacement Criteria 加「範囲の限界」（AGENTS §14）的集中頁；各部件文件的 Replacement Criteria 仍留在部件文件，本頁只收**跨部件**或**影響事前宣告**的項目。位置由圖書管理員定（架構師暫放 `target/`，平鋪）。

規則：每一條寫「限制、來源、成本落在哪、修改／解除條件、守衛」。後續批次的事前宣告引用本頁的編號，標為範囲の限界，直到該條記錄「已解除（批次、日期）」。

| # | 限制 | 來源 | 成本落在 | 修改／解除條件 | 守衛 |
|---|---|---|---|---|---|
| LIM-1 | **讀 (c) 資料的 (b) System 必須與 (c) 寫者有顯式順序**；慣例：呈現放在模擬之後的 stage | spec D.6、K.3 第 5 項（使用者 2026-11-11） | 呈現 System 的數量：每個讀 (c) 的呈現 System 都要落在正確 stage；產品線遷移時確認 | spec K.2 第 3 條：同一版本內為消除 D-E8 加的順序宣告一半以上與資料流無關 → 區劃錯了，重審區劃 | spec J-22、J-44 |
| LIM-2 | **v1 只有 Serial**：任何 `Parallel` 宣告是啟動錯誤 D-E17；含 (c) 的 schedule 要開 Parallel 必須有「與 Serial 逐位元相同」的守衛 | spec E.2、X-2、K.3 第 14 項（待定）；mechanism 主張 B | 一個 World 內 System 不並行。**只限一個 World 內**：Host 層的執行緒（IO、模擬、主執行緒、音訊）是不同 World 或外部服務，不受限，與今天相同 | 量到某個 schedule 的 tick 超出計畫預算（AGENTS §2 計畫條件 2），且有 spec J-27 的守衛。超預算的三條路：(1) Parallel 加守衛；(2) 拆 World（兩時鐘兩執行緒）；(3) 葉子函式內的 job 並行（不經 Scheduler） | spec J-27 |
| LIM-3 | **debug 存取核對攔不到的形狀**（spike 範囲の限界 L1～L8）：寫了又寫回、經存下參考只讀、有 Write 宣告時繞過句柄寫、堆積成員、全域狀態、host 碰過同欄後的繞路寫、其他執行緒、懸空參考 | spike-report〈範囲の限界〉；使用者 2026-11-11 R-01 | 不經 World 的存取只能靠審查（mechanism §3.5 三層）；-audit 第二問改為「跨 tick 指標或參考」「全域可變狀態」 | `-spike-pageguard`（debug 頁保護，只對寫設保護、只保護獨占欄、只在 (c) 區 System 執行時）可解除 L1、L6；L2（只讀）、L5（全域）不解除；L7 等 Scheduler 批再評估 | spike `Limit:` 測試（機制進步時會失敗） |
| LIM-4 | **進 digest 的欄不得 Shared**（S-12）；預設獨占，Shared 要登記時標明 | spec B.3、K.3 第 3 項（使用者 2026-11-11） | 多來源影響同一 digest 欄時必須走漏斗（事件或中間欄）由唯一寫者合併 | 不解除（是 Design Invariant）；同一 World 的 (c) Exclusive 欄為繞過檢查改 Shared 兩次以上 → 重新劃分元件（spec K.2 第 10 條） | spec J-15、J-16 |

## 說明

- **Serial 的範圍**：LIM-2 只限制一個 World 內 System 的並行。Match 的 IO 執行緒、模擬執行緒、Client 的主執行緒與音訊回呼，各是不同 World 的 runner 或外部服務，彼此並行，與今天相同。
- **為什麼 C++ 要守衛而 Bevy 不用**：Bevy 能預設並行，是因為 Rust 的借用檢查保證存取宣告可信；C++ 做不到（LIM-3），所以含 (c) 的並行要用逐位元等價的守衛證明。Bevy 預設忽略歧義是目標不同（它不以跨平台位元一致為目標），不是語言差異。
- 本頁與各部件文件的 Replacement Criteria 的分工：部件內的置換條件留部件文件；跨部件、或會出現在批次事前宣告「範囲の限界」裡的，列本頁。

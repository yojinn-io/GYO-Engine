# PvP v7.1 edge-actions-passthrough、edge-analyzers、edge-contract：動作原樣轉送、守恆分析器重新凍結、無狀態邊緣的約束與守衛

日期：2026-10-17～2026-10-18。**完成**（edge-actions-passthrough `45bda2b`、審查修正 `5b4d626`、D117 修正 `67d7da7`；edge-analyzers `4f771d5`；edge-contract `7e2dc09`、`3e0924e`。edge-actions-passthrough 的 L2 兩次 session 停下，最後併入 edge-rate-limit 的 L2 通過）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78) 合併（`0fabb52`，2026-10-20）。
計畫見 [v7.1 README](../object_fps_pvp/plans/v7.1/README.md) 的三列；L2 宣告見 [edge-actions-passthrough](../object_fps_pvp/plans/v7.1/edge-actions-passthrough-l2.md)（第 4 版，後由 [edge-rate-limit 的宣告](../object_fps_pvp/plans/v7.1/edge-rate-limit-l2.md)取代）；條文見 [network-architecture](../object_fps_pvp/network-architecture.zh-Hant.md) §7 `Design Invariants` 的「Gateway：無狀態邊緣」；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D114～D120。

## 做了什麼

- edge-actions-passthrough（D114）：刪除 link 的動作視窗（requests、ACK 游標、退役、`asked`、30 Hz 持有與重送）與 A5。每個解碼成功的動作 datagram 原樣成為一則 IPC 訊息，排進與輸入同一個 FIFO；重複、衝突、ACK-only、未知 ACK 都交給 Match 判定。Leave 與踢出不再丟已排隊的動作。新增守恆鍵：handed＝寫出＋溢出＋關閉時清掉＋寫出失敗放棄。
- edge-analyzers（D119）：ingress 分析器改為 `pvp-v71-ingress-3`，認得四組 Gateway 附加鍵；新增 `removed_paths`（被刪路徑的鍵必須是 0）、`link_actions` 與 I1a（Gateway 寫出的動作批＝Match 收到的動作批）；直通後的 I3 與 `link_shots` 判為不適用，跨程序核對拿掉 I3 的宣告例外。重新凍結為 `frozen-pv7-ea.sha256`。
- edge-contract（P5）：在 network-architecture §7 寫下「Gateway：無狀態邊緣」：四條原則（只持有軟狀態、不持有 Match 決策狀態的副本、每則訊息獨立處理、丟棄有界並回報）、11 個允許類別、禁止項、守衛與變更規則、Replacement Criteria；protocol-v7 §2a 同步。守衛 `state_whitelist_test.go` 以 reflect 走訪 Gateway 的 26 個 struct、以 `go/ast` 掃描 27 個套件變數與 goroutine 裡的區域 map、slice；未分類或類別不在允許清單就失敗。取代 `gateway-duties`。

## 驗證與經過

- edge-actions-passthrough：突變 126 個第一輪 122 個殺死；存活的 `16-evict-keeps-link`、`v71ap-commit-before-decode` 補測試，panic 的溢出測試改寫法後殺死。xhigh 程式碼審查找到 1 個 must：輸入與動作共用 64 則上限，違反 §2a「每位玩家不少於 64 則輸入窗口」，改成同一 FIFO 內分開計數（各 64 則、只丟同種類最舊）；另修 2 個 should、補 3 個突變。四平台 L1 一次因 macos-arm64 上傳 artifact 的 DNS 失敗（環境）重跑後通過。
- L2 宣告第 1 版經 D45 檢查（必須修正 3、應修正 6），第 3 版由使用者核准（D115）。使用者去睡覺期間照順序做完 edge 系列（D116）。
- 第 1 次 session 停在第 29 項 `rapid-edge-30-r1-B`：Match 日誌沒有「ipc closed」行，I1 判不出來。建議照 D110 放寬，使用者回答：「我不想要改動小，我想要能稍微精確定位問題和處理掉的方案。」重現後找到兩個缺陷：Match 的 `IpcHost::Stop()` 繞過 `Close()`，以及 rapid runner 同時對所有行程送 SIGTERM。修正（D117）：`Stop()` 經共用的 `LogEnd` 記 `reason=stopped`（先寫失敗的測試），runner 改成 GUI → Gateway → Match 的順序；宣告第 4 版預先核准。
- 第 2 次 session 停在第 102 項 `gateway-1000ms-r4-B`：player 1 的 backlog 重設落在「恢復後 1.5 秒」寬限之外。根因定位到通用層 `session.go` 的固定 1 秒窗口限速：Gateway 停頓後一次讀進緩衝的合法封包，約 0.4 秒後超過 120，窗口剩下的輸入全部被丟；用 relay 的轉送時間重放，3 個跑次的預測拒收數與 Gateway 的計數完全相同。兩棵樹都有，與這批無關。使用者記為已知缺陷 K1（D118），之後改到 edge-rate-limit 收尾處理（D119），本批 L2 也移到那裡。兩次失敗 session 照 §14.1 保留、不重跑。
- edge-analyzers：新舊分析器各分析 669 個案例目錄，差異全部是預期的（直通的 I3 由「宣告的失敗」變「不適用」等），既有判定都相同、新版沒有不通過。分析器自測 52 → 60，突變 35 個全部殺死（1 個因測試拿分析器常數當迴圈清單而少測，改成字面值）。實際約 1.5 h（估 5～6 h）。
- edge-contract：做到草稿與守衛後停下。突變 5 個（`v71ec-*`）全部殺死，會加欄位的既有 6 個仍以原本的訊息殺死。盤點時發現死欄位 `queuedControl.at`；N3（佇列項目帶著的診斷身分）和 D105 的字面衝突，草稿列為獨立類別。使用者核准條文、N3 照草稿、刪除死欄位（D120）。

## 證據

`build/target/_build/test/logs/pvp-v71-edge-actions-l2-prep-20261017/`、`pvp-v71-edge-actions-l2-20261017/`（第 1 次，停止）、`pvp-v71-edge-actions-l2-prep-20261018/`（第 4 版）、`pvp-v71-edge-actions-l2-20261018/`（第 2 次，停止）、`pvp-v71-edge-analyzers-20261018/`（`compare.py`、`compare.json`、`frozen-pv7-ea.sha256`）（git 忽略）。

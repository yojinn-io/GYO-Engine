# silent-branch-asserts：無聲分支的列點（HEAD `0681463`；2026-10-24 使用者決定，D180）

日期：2026-10-24。after-re-evaluate 的第一批（INVENTORY `N-silent-branches-assert`）。PR #95，分支 `claude/pvp-v71-system-observe-test`。

**無聲分支**：程式碼丟掉了輸入、狀態或訊息（提早 return、continue、丟棄的回傳值、default），卻沒有計數、記錄、回報錯誤或 assert；而且程式碼或註解認為它走不到。

分類照 [error-handling.md](../../../architecture/error-handling.md)：

- **Assert**：Programmer Error，內部不變量壞了，C++ 用 `GYO_ASSERT`；
- **計數／Result**：Runtime Error，環境（網路對端、檔案、時序）能觸發；
- **保留**：走得到，而且現在的處理是對的。

**來源與方法**：

- 原清單在 git 忽略的證據 `build/target/_build/test/logs/pvp-v7-dlq-eval-20261011/b-report-final.md:134-142`（`91b97ee`）。後來 edge 批改寫了 Gateway，所以對 HEAD 重新列點。
- 範圍：產品程式，包括 C++ 與 Gateway 的 Go，以及通用層 session。不含測試、驗收工具與 Engine。
- 子 Agent 列點，主會話抽查 A1、A2、A7、A8、C1、C3 的行號與內容，都相符。

## 1. 先要決定的機制

- **C++**：`GYO_ASSERT`。error-handling.md 規定產品的新程式照這套規則（`LogFile.cpp` 已經在用）。產品其他地方過去用 `throw std::logic_error` 表示不變量壞了；這一批只把被改到的地方改成 `GYO_ASSERT`，不順手把其他的 `logic_error` 一起換掉。
- **Go（需要你決定）**：Gateway 目前沒有 `panic`，也沒有 assert 機制。選項：
  - (g1) **panic**：整個 Gateway 程序結束，房間裡所有人都斷線。最接近 C++ 的 abort。
  - (g2) **讓 runtime link 失敗**：記錄一行原因，房間變成 unavailable。這是現在處理 IPC 協議錯誤的同一條路，所以可見、可恢復（重啟服務），不需要新機制。
  - (g3) **計數加記錄，不中斷**：不會讓人斷線，但不變量壞了之後仍繼續跑。
  - 建議 (g2)：沿用既有的失敗出口，又不會無聲。
- **D180**：採用 (g2)。A1～A12、C1、C2 照表；L1～L7 保留並修正 INVENTORY；C3 也做。

## 2. Assert 候選（守衛已確認、外部輸入到不了）

| 代號 | 位置 | 現在無聲丟掉什麼 | 為什麼走不到 |
|---|---|---|---|
| A1 | `MatchRuntimeHost.cpp:183-188`（`SubmitInput`） | 身分不同時，清掉暫存的命令，不計數 | 身分只在 `match_.Tick` 內改變，同一個 lambda 會在同一把鎖內清掉舊身分並計數（`:456-466`）；`AdmitInput` 也要求輸入的身分等於現在的（`PvpMatch.cpp:72-76`）。條件本身走得到，但只會遇到空的預設項目。建議 assert `merged.commands.empty()` |
| A2 | `MatchRuntimeHost.cpp:391-392` | `AcknowledgeActions` 的回傳值被丟掉 | 每個值收到時都驗證過（`:305`、`:341`）；有效性對 `through` 單調，兩個有效值的最大值也有效 |
| A3 | `MatchRuntimeHost.cpp:429` | `SubmitInput` 回傳 false 時命令被丟 | 剛剛在同一把鎖、同一個狀態下 `CanSubmitInput` 回傳 true，`SubmitInput` 重做同一個檢查（`PvpMatch.cpp:91-92`） |
| A4 | `action_delivery.go:82-85`（舊 D8） | 結果框架編碼失敗就丟掉（序號已經加 1） | 一框最多 8 筆、列舉有界，約 620 bytes，上限 1176（`framing.go:27`） |
| A5 | `server.go:631-634`（`sendControl`） | Marshal 失敗 | 字串在收到 runtime 訊息時已驗證過 UTF-8，其他欄位是數字或常數 |
| A6 | `server.go:856-859`、`:861-864`（`snapshotForPeer`） | 每位玩家的 snapshot 的 Marshal／編碼失敗 | `runtimeMessage` 已用最寬的 hold 編過同一份 snapshot 並檢查大小；每位玩家的 hold 被夾在那個最大值以內 |
| A7 | `server.go:722-725` | 內層的 `default` 丟掉 snapshot 框架 | 剛清空、容量 1 的 channel，只有單一 reader goroutine 會寫入（沒有註解寫明這點） |
| A8 | `server.go:1123`、`action_delivery.go:124` | `CommitSequence` 的錯誤被丟掉 | 同一把鎖下 `Admit` 剛確認序號較新，`CommitSequence` 只在序號不新時失敗（註解已寫「cannot fail」） |
| A9 | `ClientConnection.cpp:566` | `combatRules` 不在時丟掉有效 snapshot | `welcomed` 與 `combatRules` 一起設定；清掉規則的地方一定同時清 `welcomed` 或換代 |
| A10 | `MatchRuntimeHost.cpp:596`（`TakeSnapshot`） | 找不到參考時 `publishedAt` 沒設（只影響診斷） | `snapshot_` 只在剛放入自己的參考之後設定；兩者一起清除 |
| A11 | `MatchRuntimeHost.cpp:696`、`:742`（優先度低） | 品質資料不在就跳過判定；逐出時丟掉 `Leave` 的回傳值 | `quality_` 的鍵是 Match 玩家的子集，跟著 Join、Leave、清場一起增刪 |
| A12 | `IpcHost.cpp:692-693`（`Pump`） | 找不到對應 join 的結果被跳過 | 兩種情況合在一起：Leave 的結果是設計上忽略（走得到）；Join 的結果找不到則走不到。建議：找不到時 assert 它是 Leave |

另有一處相關但不是無聲分支：`MatchRuntimeHost.cpp:177-179` 對 `find_if` 的結果直接取值，沒檢查 `end()`（依賴准入已接受該玩家）。這是未定義行為，適合和 A1 一起加 assert。

## 3. 計數／Result 候選（環境能觸發）

| 代號 | 位置 | 說明 | 建議 |
|---|---|---|---|
| C1 | `server.go:646-650`（`controlPacket`） | 控制封包編碼失敗就丟掉，序號已加 1。Welcome 與逐出通知有界，但 `join_rejected` 的原因字串是 Match 給的、沒有長度上限（`runtime_v7.proto:61`），原樣轉發 | 收到時驗證長度，超過就當協議錯誤讓 link 失敗（比照 `SnapshotForClient`），之後這裡改成 assert |
| C2 | `server.go:666` 的 `p != nil && p.phase != joining` 那一半 | 只有 Match 送出重複或沒要求的 JoinResult 才會走到 | 當成 IPC 契約違反，讓 link 失敗（比照 `runtime_link.go:578-580`）；`p == nil` 那一半是 D9，走得到 |
| C3 | `ClientConnection.cpp` 的 `Network`：`:527`、`:556`、`:557-558`、`:611`、`:626`、`:670`、`:678`、`:680` | 從 Gateway 收到、被丟掉的封包沒有計數。旁邊的其他結果都有計數，所以 `received_datagrams` 不等於各結果的總和。損壞、偽造、其他版本的封包，或遺失的 Welcome，都會走到 | 在 worker statistics 行尾追加計數鍵（日誌契約 R1 允許只在行尾追加），或只記錄、不修 |

## 4. 保留（走得到，現在的處理正確）

| 代號 | 位置 | 為什麼保留 |
|---|---|---|
| L1 | `server.go:734-737`（D9，逐出時 `p == nil`） | 原清單把它列成 Assert 是錯的：Gateway 已經移除玩家（離開或逾時），Match 的逐出還在路上時就會走到。要從 INVENTORY 的 Assert 列刪掉 |
| L2 | `MatchRuntimeHost.cpp:385` | 同一個競態；沒有東西要丟 |
| L3 | `server.go:687-690`、`:805-811`、`:850-852`（D4） | 房間已不可用、玩家已移除或不在 active 時的 snapshot |
| L4 | `IpcHost.cpp:747`、`:803` | `Step` 在 `Pump` 與 lane 迴圈之間逐出玩家時會走到，結果跟著玩家一起消失 |
| L5 | `IpcHost.cpp:548`、`:553` | Leave 之後到達的批次（pv7 §2a 的設計），拒收已由 host 計數 |
| L6 | `drop_report.go:148-150` | 最後一份回報之後不再記入 ledger；10 s 的行已先計數（查證屬實） |
| L7 | `MatchRuntimeHost.cpp:244` | 已排定 Resync 時回傳 false，已由 `lateBeyondNoCommands` 計數 |

## 5. 守衛還沒追完的候選（Client 對已驗證資料的再檢查）

| 代號 | 位置 | 說明 |
|---|---|---|
| U1 | `PvpApplication.cpp:446-447` | `timeline.Push` 的回傳值被丟掉；檢查和 `ClientConnection.cpp` 的驗證重複 |
| U2 | `LocalPlayerPrediction.cpp:232-242` | `Reconcile` 遇到無效或倒退的權威狀態時無聲 return |
| U3 | `ClientConnection.cpp:931`、`:949`（`SendInput`） | 無效窗口、錯的玩家、改了未確認命令的呼叫被丟掉；屬於 API 誤用 |

建議：這一批先不動，只記錄；要改的話，每點再追約 30 分鐘。

**D180**：U1～U3 與第 7 節的分支這一批不動。使用者補充：`lan15-windows` 之前要把已知問題全部處理掉，包括這些分支與 outage-resync 只記錄的問題。

## 6. 原清單各項的去向

- `MatchRuntimeHost.cpp:173-175` → 移到 `:183-188`，仍無聲（A1）。
- `MatchRuntimeHost.cpp:372` → 移到 `:391-392`，仍無聲（A2）。
- `server.go:523-531` → edge 批拆開了：Marshal 失敗在 `:631-634`（A5），編碼失敗在 `:646-650`（C1），控制佇列滿（D6）現在在 `:638` 計數。
- `server.go:638-641`（D9）→ 移到 `:734-737`，走得到，改為保留（L1）。
- D8（`action_delivery.go:268-280`）→ 移到 `:82-85`（A4）；它的 Marshal 現在會讓 runtime 失敗，不再無聲。
- `v7/HANDOFF.md:491` → 現在在 `:492`，INVENTORY 的引用差一行。

## 7. 已經計數、但註解寫「應為 0」的分支（不在本清單，供參考）

`MatchRuntimeHost.cpp:198-200`（`StagedOverWindow`）、`:411-418`（`HandoffRejected`）、`server.go:1130-1134`（`ErrInput`）、`action_delivery.go:137-141`（`ErrActions`）。它們不是無聲的；要不要也改成 assert，可以之後再決定。

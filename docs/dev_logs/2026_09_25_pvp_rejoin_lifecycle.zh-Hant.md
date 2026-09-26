# 2026-09-25 — PvP Esc 重入的身分標示與人數更新

## 回報與範圍

使用者回報：按 Esc 回到 Lobby，再進入同一房間時，Player 編號與玩家人數不對，
看起來每次都換了一個角色。此次針對退出、重入、名額顯示與舊 Session 清理；
沒有加入帳號、持久角色、斷線續玩或空房自動關閉政策。

## 原因

### 1. 暫時身分編號被呈現成玩家編號

Gateway 每次建立新 reservation 都增加 `nextPlayer`，不是重用 Player1／Player2。
`player_id` 代表這次加入的角色實例，不能拿來當人數或固定席位。原本 HUD 卻顯示
`PLAYER 5 / 2 players`，將內部身分與人數放在一起；對手顏色還依 ID 奇偶改變，
讓重入後的角色看起來不同。

原試玩日誌可見同一 Client 依次進入 ID 2、3、5，另一個為 1、4。這證明身分遞增，
不能僅憑該編號宣稱場中存在五個角色。既有測試明確要求重入不可沿用舊身分。
離開時原角色被移除；再次加入會重新出生，目前沒有保存角色位置或帳號身分。

修正 HUD 僅顯示 `PLAYERS ONLINE: N`，數字來自權威快照的實際角色集合。
兩人 PvP 的對手使用固定顏色；內部 ID 保留於協定、日誌與驗收資料。

### 2. Lobby 顯示舊的名額快取

原本 `ClientConnectionState::rooms` 只在手動 Refresh 時更新。Create／Join／Leave
既不清空它，也不在離開後重新查詢。建立房間的 Client 可能仍顯示沒有房間，
另一個 Client 則沿用加入前的人數；即使其他人已離開也不會自動更新。

另外，`GET /rooms` 的 `players` 是占用名額數，包含 reserved／joining／active；
世界快照只包含已進入 Match 的角色。這兩個數字在握手期間本來就可能不同。
Lobby 改標示 `N of 2 slots occupied`，保留原本容量契約，不把尚未完成握手的名額
誤當成可再次分配的空位。

### 3. 離開失敗被當成已完成清理

原本 `DisconnectRemote()` 忽略 HTTP Leave 的回覆，無論成功、逾時或被拒絕都會
清除本機 Session 憑證。此時若立即重新加入，舊名額／角色仍可能保留到五秒超時，
造成短暫的額外角色或 room_full。這是可由程式碼確認的錯誤路徑；不能在沒有失敗
日誌的情況下宣稱使用者那次退出一定發生過 HTTP 失敗。

修正將已確認的 Leave 作為下一次加入前的必要條件。只有 HTTP 成功且 `left:true`
才丟棄舊憑證；失敗時停止舊 UDP／保活與本機世界，但保留待清理憑證並顯示錯誤。
下一個明確操作先重試冪等 Leave，成功後才能 Refresh／Create／Join。
自動 Lobby 查詢不能繞過這項清理。

## 修正後的生命週期

1. Esc 立即清除本機世界、預測及歷史，顯示聯絡 Gateway 的狀態。
2. Worker 完成有效 Leave，再讀取最新房間列表，才回到可操作的 Lobby。
3. Lobby 每秒約查詢一次房間名額；新的操作使舊查詢結果失效，不覆蓋新連線狀態。
4. 新 Join 建立新角色實例，舊角色不能復活；世界人數來自最新權威快照。
5. 所有人離開後 Room 1 仍保留為零占用名額。本次不改空房保留政策。

## 驗證

- 完整 C++ 建置成功；CTest 27／27 通過，含產品、組合、包裝與 owner 選取檢查。
- 實際 Gateway／Match／Client socket 驗收通過，基線有 81 份共同權威快照一致。
- 每組連續三次 Leave → Join：留下的 Client 快照必須恰好只包含自己；重入後兩端
  都必須恰好包含新的自身 ID 與另一位原 ID，共兩位。退休 ID 不得再使用。
- 獨立、沒有手動 Refresh 的 Lobby observer 檢查名額 2 → 1 → 2，最後兩人離開後
  所有 Lobby 回到 0。也檢查立刻取消 Join 與 Client 析構後的清理。
- 專屬 HTTP relay 將第一次 Leave 回覆改為 503 且不轉送，確認 Client 顯示錯誤、
  清空世界與舊清單；下一次 Join 必須先用相同舊憑證成功 Leave，才可重新 Join。
  新角色的權威快照只有自身一位，不能帶回舊角色；析構後名額回到 0。
- 同一整套流程再以 RTT 0／20／40 ms、最多 10 ms 單向抖動、5% 丟包、首批與
  連續兩包遺失執行，三組皆通過。HTTP 故障注入位於驗收 probe，正式遊戲無測試開關。
- 雙 GUI 驗收通過：經 SDL Esc 立即清除預測，等待成功清理後返回 Lobby 並取得
  房間清單。GUI 的內部 ID 已到 29／30，但兩端權威玩家集合仍恰好只有這兩位。
- 本機 32／32 個穩定移動幀持續位移，其中 21 幀沒有新 Snapshot；跨視窗位移量測
  中位數 100.375 ms、最大 101.725 ms，低於既有 150 ms 門檻。這是同機 post-Render
  量測，不是實體 LAN／螢幕輸入延遲保證。

原始證據：`build/target/_build/test/logs/pvp-rejoin-lifecycle/result.json`、
`network.log`、`network-rtt-*.json`、`gui/*-report.txt`；CTest 報告為
`build/target/_build/test/logs/pvp-rejoin-tests.xml`。這些是 git 忽略的可再生本機輸出。

本次沒有證據顯示「正常成功 Leave」會讓 Gateway 持續累加角色。已證實的是編號／
人數標示混淆、Lobby 快取未更新，以及注入 HTTP 失敗時會遺失清理狀態的錯誤路徑。
實際使用者那次是否發生 HTTP 失敗無法由舊日誌判定。

重跑命令（repository 根目錄，使用已建置的 Gateway）：

```bash
cmake --build --preset test --parallel 6
ctest --preset test --parallel 4 --output-junit logs/pvp-rejoin-tests.xml
python3 build/acceptance/object_fps_pvp/run_network.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_network_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --gui-probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena-root build/target/object_fps_pvp/bin/assets/object_fps_pvp \
  --network-impairments --output build/target/_build/test/logs/pvp-rejoin-lifecycle
```

人工試玩：兩個 Client 加入同一房間，A 按 Esc，B 應剩 1 人；A 返回 Lobby 後
選 Join Room，兩端應回到 2 人。重複數次，再讓兩端都離開，Lobby 約一秒更新後
應為 0／2 占用名額。再次加入仍會重新出生，並非恢復上一個角色的位置。

## 變更邊界

修正位於 object_fps_pvp 的 Client worker、產品 HUD 與專屬驗收。沒有改動 Engine、
公共 Gateway、Authority Tick、移動命令契約或 wire schema；Room／Session ownership
仍屬產品 Gateway。UI 不再把診斷 ID 當成玩家席位，不導入新的永久身分模型。

Architecture Delta 僅有驗收 target 新增對既有 `httplib::httplib` 的私有依賴：
需要在產品 probe 中重現 HTTP Leave 503，原本 Client network 的私有依賴不會暴露
HTTP header 給 probe。由專屬 probe 建立小型 HTTP relay，正式 Client 不增加測試
控制。此依賴只隨 object_fps_pvp 驗收選取而存在，沒有新的公共 target、owner
或反向依賴；比在正式網路類別加入故障注入介面更局部。

# 2026-09-25 — PvP 本機移動預測與伺服器校正

## 完成內容

本機鏡頭原本直接使用 20 Hz 權威快照，WASD 造成每 50 ms 一次的位置跳動。
現在產品內的 `StepMovement` 共用方向正規化、速度、視角限制及靜態牆壁碰撞；
Client 以 60 Hz 預測自己的位置，收到權威狀態後只重播未確認命令。鏡頭插值相鄰
預測狀態，小校正於 100 ms 內消除，誤差達 1 世界單位直接定位，顯示偏移受碰撞限制。
滑鼠仍每幀消費一次。遠端玩家原有 100 ms 插值維持不變。

Client／Runtime Protocol 同時升為 v2（含 Go bindings），拒絕舊版。單步命令
固定 1/60 秒，不接受 Client 指定時間或位置；每位玩家的 `lastResolvedCommand`
與全域 Authority Tick 分離，包含缺包替代步驟。Authority 維持 60 Hz、Snapshot 20 Hz。

Client 初次／重新同步時建立六個中立命令，再加入當前操作。每秒約 30 批重送
完整未確認窗口，最多 12 命令，滿載暫停位置預測但保留滑鼠視角及重送。
Gateway／Host 合併完整命令，最多 32 個未來步驟。相同序號不可修改，晚到不重播，
重複不刷新移動期限；缺命令最多沿用最近實際採用輸入 15 個 Tick，之後中立。

加入、離開、斷線、身分變更清空預測、歷史及顯示校正。審查另外修正凍結窗口收到
同 ACK 新快照時的插值反跳，以及合法大型出生 yaw 的正規化問題。
`PvpApplication::LocalMovement()` 只有唯讀觀測，不提供遊戲測試控制。

## 自動驗證結果

| 驗證 | 結果 |
|---|---|
| 完整 `test` profile C++ 建置 | 通過，含 Client／Match／UI Editor／probes |
| `ctest --preset test --parallel 4` | 27／27 通過 |
| 最後出生角度修正後的 PvP CPU／wire | 2／2 通過；PvP 27 個 case、44,332 個斷言 |
| 公共與產品 Go `test -race ./...` | 兩個 module 皆通過 |
| Shader host tests | 3／3 通過 |
| Vulkan GPU smoke（Xvfb + Lavapipe） | 1／1 通過 |
| 真實 Match／Gateway／雙 Client socket | 通過，81 份相同 Authority Tick 的完整狀態一致 |
| 真實桌面雙 GUI + GPU scene capture | 通過，33／33 個穩定移動幀有鏡頭位移，其中 22 幀沒有新 Snapshot |
| 真實 UDP RTT 0／20／40 ms + 抖動／丟包 | 三組皆通過，每組 71 份共同權威快照一致 |

產品測試涵蓋直走、斜走、轉向、停止、貼牆／牆角、已確認命令不重播、100 ms 校正、
1 單位重新定位、顯示碰撞、窗口滿載／解凍、失焦中立命令、身分重設及 Server catch-up。
30／60／144 FPS 的獨立呈現測試中，穩定移動期間每個呈現幀都有位移，最大單幀距離
不超過 `3 / FPS + 0.004`。虛擬 wire 另外覆蓋延遲、亂序、重複、首包、停止連續丟包
及 250 ms Client 停頓；檢查重播結果、窗口上限與恢復。

真實 socket 干擾由 owner 專屬 HTTP／UDP 代理施加，正式程式沒有測試分支。
每位玩家固定丟首批及第 61／62 批（停止附近），另以固定 seed 按 5% 機率丟 Input／
Snapshot。單向排程延遲為 RTT/2 ±10 ms，最小為零；Hello／Welcome／HTTP 保持可靠。
每組觀察到 241 個輸入封包、16 個輸入丟包（首批 2、連續批次 4、隨機 10），
196 個 Snapshot、12 個隨機丟包。RTT 0／20／40 的最大觀測單向延遲約為
10.055／20.083／30.072 ms，保留 OS 排程開銷而非把設定值當量測值。
實際最大 UDP 分別為 144／134／144 bytes；另以最大序號、全部非零欄位的 12 命令
codec 測試確認不超過 1,200 bytes。

真實網路驗收也確認中立停止、再次移動、輸入超時、Leave／新 ID 重入、無效 IPC、
替換 Gateway 清場、Match 斷線時兩個 Client 回 Lobby 並清空狀態。
GUI probe 經 SDL 鍵盤操作真實 Lobby、W 移動與 ESC，沒有直接注入假世界；
攝影機位置觀測與實際 RenderQueue 的輸入相同。

## 本次產物與重現

詳細原始產物在忽略的 build tree：

- `build/target/_build/test/logs/pvp-v2-tests.xml`
- `build/target/_build/test/logs/pvp-v2-final-pvp.xml`
- `build/target/_build/test/logs/pvp-v2-gpu.xml`
- `build/target/_build/test/host-tools/shader-tests.xml`
- `build/target/_build/test/logs/pvp-v2-network/result.json`
- `build/target/_build/test/logs/pvp-v2-network/gui/create-report.txt`
- `build/target/_build/test/logs/pvp-v2-network/gui/create-movement.csv`
- `build/target/_build/test/logs/pvp-v2-network/gui/*-before.bmp`、`*-world.bmp`
- `build/target/_build/test/logs/pvp-v2-impaired/network-rtt-{0,20,40}.json`

從 repository root 執行：

```bash
cmake --preset test
CMAKE_BUILD_PARALLEL_LEVEL=6 cmake --build --preset test --parallel 6
ctest --preset test --parallel 4
ctest --test-dir build/target/_build/test/host-tools -C Release -L shader --output-on-failure
(cd services/gyo_gateway && GOWORK=off go test -race ./...)
(cd apps/object_fps_pvp && GOWORK=off go test -race ./...)
cmake --build --preset test --target gyo_object_fps_pvp-gateway
python3 build/acceptance/object_fps_pvp/run_network.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_network_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --gui-probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena-root build/target/object_fps_pvp/bin/assets/object_fps_pvp \
  --network-impairments --output build/target/_build/test/logs/pvp-v2-combined
```

實際測試使用既有 GCC 14／CMake test cache、固定版本依賴與獨立 `/tmp` Go cache。
這是 Linux localhost 的實際傳輸與桌面 GPU 證據；未宣稱實體雙機 LAN、Windows
本次重建或高延遲 Internet 驗證。

## Architecture Delta

為消除自身操控等待快照的延遲與階梯位移，Client 新增本機移動預測／重播責任；
輸入由持續狀態改為可確認的單步命令，因此兩份產品 wire contract 升級。
修改影響 PvP domain、Client、Runtime Host／IPC、產品 Go Adapter／Gateway、
既有產品 CMake 來源／schema target 與 owner 專屬驗收。只平滑快照會保留操作等待，
不能達到本次即時預測目標；不需要公共預測框架。

新增檔案、命令窗口、預測及碰撞政策全部歸 object_fps_pvp。Client／Match 使用同一
產品 domain，Engine 與公共 services/gyo_gateway 沒有新增遊戲邏輯、產品參照或
反向依賴。無新頂層目錄／Subsystem／owner；Arena JSON 與 Editor Runtime
Data Contract 不變。公共 CMake／registry／packaging 沒有加入產品分支，owner
專屬測試仍僅在選擇本產品時組入，既有 composition／registry／packaging checks 通過。

## 人工試玩

最後重新建置後，已啟動正式 Match／Gateway 與兩個正式 Client 視窗。
第一個選 Create + Join；第二個 Refresh 後 Join Room。
請分別確認 WASD 平移、只轉滑鼠視角與兩者同時操作。使用者已確認本機平移與鼠標視角明顯順暢，但回報拖曳標題列時系統失去回應、
Player1 掉線，以及另一視窗人物有可見滯後。後續調整與證據記錄於
[LAN 延遲與拖窗處理](2026_09_25_pvp_lan_latency.zh-Hant.md)；本篇六步／100 ms 為初版驗收配置。

本次試玩日誌：`build/target/_run/object_fps_pvp/20260925-190207-336496/`。
本機忽略的啟動 helper 為 `build/target/run-object-fps-pvp.py`，關閉兩個 Client 後
會停止由該次 helper 啟動的服務。

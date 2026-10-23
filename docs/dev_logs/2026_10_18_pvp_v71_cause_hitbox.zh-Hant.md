# PvP v7.1 cause-records、hitbox-fields：原因紀錄（E2、動作拒收回覆、Leave 原因）與 hitbox 欄位、E 和 S 的影子紀錄

日期：2026-10-18。**完成**（cause-records `d63f722`；hitbox-fields `91b0264`、`time_ns` 補充 `6f3c1a0`；兩批都是行為不變；README 的 L2 欄是「只記錄」，沒有單獨的 L2 session）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78) 合併（`0fabb52`，2026-10-20）。
計畫見 [v7.1 README](../object_fps_pvp/plans/v7.1/README.md) 的兩列與 [pv7 設計](../object_fps_pvp/plans/v7.1/pv7-design.md) 的測試清單；契約見 [protocol-v7](../object_fps_pvp/protocol-v7.zh-Hant.md) §5～§8；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D124～D128。

## 做了什麼

- cause-records（D124）：補上 E2（需要 wire 資料的紀錄）。輸入與動作批帶 Gateway 的 dwell（一對一）與 Client 的 GYOP 序號（只作診斷，D96）；snapshot 帶兩段停留（Match 的 `match_hold_us`、Gateway 每位玩家各自的 `gateway_hold_us`，D-4）；`ActionResults` 帶拒收批數與最後的拒收原因（D-6）；`PlayerLeave` 帶 `LeaveReason`。Match 加 `gw_transport` 行族與 Leave 行尾的 `reason=`，Client 每個統計窗口一行 cause records；日誌形狀寫進 protocol-v7 §5。最大的 4 人 snapshot datagram 1055 → 1065 B。
- Gateway 的 Architecture Delta（D125，使用者核准）：`linkWrite` 加 `received`（寫出時算 dwell）、`snapshotFrame` 改存讀取時刻與解碼後的 snapshot（每位玩家重新編碼）；network-architecture §7「diagnostics carried with a queue item」的說明改成「被丟棄時回報，或在寫出時算成 dwell；都不被判定讀取」。白名單守衛同步。
- hitbox-fields（D126）：`HitPart` 列舉，命中部位一律填 Body；`hit_part`、`last_hit_part`、`maximum_shot_damage`（25）、`view_tick_q16`；射擊帶 A1 用的 E、S，由 Client 照實填（取自同一份 `ClientPresented`）。Gateway 照抄不驗證、不加狀態欄位；Match 驗證並記錄、不改判定，影子紀錄另開有版本號的 `match-shots.jsonl`（由 `--movement-trace` 推出路徑）。4 人 snapshot 最大 1065 → 1073 B，等於 §8 的契約最大值。
- D128：`match-shots.jsonl` 每筆加 `time_ns`（和 `match-commands.jsonl` 同一個時鐘），給之後的對接當順序鍵。

## 驗證與經過

- cause-records：Go 新增 `cause_records_test.go`（dwell 一對一、10 s 上限、前一則阻塞時後一則的 dwell、每位玩家各自的 hold、大小上限、D32 符號守衛）；C++ 差分測試（極端 dwell、序號、所有 Leave 原因下，判定、join 結果與最後的 snapshot 逐位元相同）。2 個 snapshot 測試的位元組比對改成忽略各自的 hold，理由照 §14.4 寫明（每位玩家的位元組本來就不同）。突變新增 18 個（`v71cr-*`）全部殺死，7 個既有突變的 find 跟著更新。CTest 72／72、Go `-race`。實際約 1 h（估 8～9 h）。
- cause-records 的 Go 產生碼以 build 的 protoc 36.2 與 module 固定的 protoc-gen-go v1.36.11 重新產生（先確認 schema 不變時重新產生沒有差異）；hitbox-fields 也重新產生。
- cause-records 照實記下的限制：`v71cr-match-hold-at-take` 與「至少一份 ≥1 tick」依賴 IPC 執行緒的時機（實務上幾乎必然，理論上非決定性）；讀取端「snapshot 超過上限」在合法的 4 人 snapshot 下到不了，那段程式沒有突變。
- hitbox-fields：權威 digest 與基準 `ed3481c` 比較，scale 1、3 各 35 情境 0 差異；Host 差分測試確認 E、S、view tick 不同時裁決與世界逐欄相同；D32 守衛加一條「判定程式不讀 S 與 view tick」。突變新增 22 個（`v71hf-*`）全部殺死，D128 再加 2 個也殺死。20:35 本機驗收完成（估 8 h）。
- 實作時發現契約寫「Host 記錄命令 S 的來源（Actual／Held／Neutral）與是否之後才到」，但這兩項到 T(S) 解析之後才知道，Host 分不出 Held 與 Neutral。停下讓使用者選：(a) 離線對上 `match-commands.jsonl` 與 `match-ingress.jsonl`、改寫契約那一句，或 (b) 動判定檔的介面。使用者選 (a)（D127），並要求 `pv7-checkpoint` 的宣告把兩件事寫成判定：對接鍵加上連線或時間順序以處理重連，以及每個 on_time 路徑的裁決都要對得到 resolved 事件，對不到就是工具無效。D128 的 `time_ns` 就是這個順序鍵。

## 證據

`build/target/_build/test/logs/pvp-v71-hitbox-fields-20261018/`（`digest/`）（git 忽略）。cause-records 沒有另存證據目錄，驗收結果記在交接的規劃紀錄。

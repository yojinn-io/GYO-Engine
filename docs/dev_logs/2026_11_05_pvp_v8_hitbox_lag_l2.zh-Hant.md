# PvP v8：v8-product-hitbox-lag-l2（view_lag 的分布與回溯範圍 R）

日期：2026-11-05。分支 `claude/pvp-v8-lag-l2`。主會話 Opus 5.5／medium；審查者子 agent Opus 5.5／high（general-purpose，唯讀；和實作者同一個模型）。估時 3～5 h＋一次場次。

**狀態：工具與場次腳本完成（T1～T3 通過）；L2 場次未跑，R 還沒有。** [事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-declaration.md)已核准（`-session` (a) 和 K8 同一場次、`-r-quantile` (a) p99＋2 Tick），之後有修訂 3、4（都在看到場次結果之前）。機器（使用者 2026-11-05）：Intel Mac 跑 Match＋Gateway，Windows 2 台，現場沒有網路。

## 做了什麼

- `build/acceptance/object_fps_pvp/lag_l2_summary.py`：
  - 只讀 version 3 的 `match-shots.jsonl`；
  - S1：區間內有效 Shot ≥500 且至少 2 人各 ≥100；
  - S2：三方對帳（Client 送出＝接受＋拒絕、依原因的計數＝Match 裁決、`trace_end` 的計數）；
  - R1：R＝min(15, ceil(最近秩 p99)＋2)；
  - R2：J 與 A 的差＝冷卻保留；
  - 不正常的紀錄一律以終止碼 2 拒絕，不產生摘要。
  - 重用 `lan15_judge.py` 的 `Session`、K1、K2。
- `tests/object_fps_pvp/test_lag_l2_summary.py`（CTest `object_fps_pvp.lag_l2_summary`，`cpu;pvp`）：21 個 case，用手算期望值的合成場次。
- 突變 5 個：分位數改內插、`null` 當 0、不檢查版本、S1 從第一個加入算起、不檢查送出數。
- 場次 kit：`build/acceptance/object_fps_pvp/lag_l2_session/mac.sh`、`windows.ps1`；手冊 [v8-product-hitbox-lag-l2-session](../object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-session.md)。

## 判定

| 判定 | 結果 |
|---|---|
| T1 工具測試 | 通過：21／21 |
| T2 本機 | PvP `cpu\|shader` 51／51（多了 `lag_l2_summary`） |
| T3 突變 | 5／5 killed |
| S1、S2、R1、R2 | 場次未跑 |

## 經過

- 審查（第一次，工具）找到 1 個 blocker：S2 只用了 lan15 的 K2，沒有對「送出數」與 `trace_end` 的計數，和宣告的三方對帳不符。照宣告補上（修訂 4），並加了對應的測試與突變。其他採納的指正：S1 起點沒有測試、兩個數量門檻沒有分開測試、R2 不成立時 R1 仍給值、被排除的紀錄沒有計數、非預期例外的終止碼和失敗相同、`10 分鐘` 被當成 S1 門檻（宣告裡它是進行方式）。
- 改完後突變 `lag-l2-null-as-zero` 的 `find` 變成 stale，照新程式改寫後 killed。
- 寫場次腳本時發現 L2 的機器在 `v8-product-hitbox-l2-machines` 一直沒有答案，停下問使用者。
- 地圖編輯器線（PR #152，未合併）會把 arena 搬到 `maps/`；`mac.sh` 用搜尋找 `pvp_corners.json`，不寫死路徑。

## 證據

主 checkout 的 `build/evidence/v8-hitbox-lag-l2/`（Intel Mac）：`ctest_*.txt`、`mutations*.txt`／`.json`、`dryrun/`、`dryrun2/`（Mac 腳本的乾跑：prepare 的輸出、Match 與 Gateway 的 log、judge 的結果）。

## 範圍的限界

- `windows.ps1` 沒有在這台 Mac 跑過（沒有 PowerShell）；場次開始時先在一台 Windows 上確認。
- 只有 LAN；漏收與合併的落差量不到（宣告「不在這批」）。

## 審查

- 第一次（工具）：見「經過」。
- 第二次（場次 kit，Opus 5.5／high）：1 個 blocker——手冊把解壓的上一層當成 `PKG`，照做時 `prepare` 與 `windows.ps1` 都會在讀 `build_metadata.json` 時失敗（乾跑用的是手組的套件，沒抓到）。採納的指正：
  - `prepare` 先讀完再建資料夾；
  - Match 與 Gateway 用 `caffeinate -i` 執行，避免無人操作的 Mac 睡著；
  - 手冊加上：只用 Ctrl+C 結束（關視窗不會寫 `trace_end`）、用 `unzip -d` 解壓、Mac 的隔離標記、預演 LAN 路徑與 Windows 的網路設定檔、K8 的斷線要持續到回到 Lobby；
  - `windows.ps1` 的 log 與 trace 檔名加時刻，重跑時不互相覆蓋；
  - 宣告修訂 5：更正修訂 4 對 K8 斷線與對帳的說法。
- 改完後照發行包的結構重新乾跑（`dryrun2/`）：錯的 `PKG` 不留下資料夾；Match 與 Gateway 在 `caffeinate` 下執行，Ctrl+C 後 `trace_end` 照常寫出。

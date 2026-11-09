# PvP v8：v8-product-hitbox-lag-l2（view_lag 的分布與回溯範圍 R）

日期：2026-11-05。分支 `claude/pvp-v8-lag-l2`。主會話 Opus 5.5／medium；審查者子 agent Opus 5.5／high（general-purpose，唯讀；和實作者同一個模型）。估時 3～5 h＋一次場次。

**狀態：工具與場次腳本完成（T1～T3 通過）；11/9 場次判定不成立（S1 undetermined、R2 failed），使用者決定當探索，R 還沒有。** [事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-lag-l2-declaration.md)已核准（`-session` (a) 和 K8 同一場次、`-r-quantile` (a) p99＋2 Tick），之後有修訂 3、4（都在看到場次結果之前）。機器（使用者 2026-11-05）：Intel Mac 跑 Match＋Gateway，Windows 2 台，現場沒有網路。

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

## 場次（2026-11-09）：判定不成立，停下（AGENTS §14）

```text
件數的突き合わせ：一致（S2：Client 送出＝接受＋拒絕且無未決（K1）、依原因計數＝Match 裁決（K2）、trace_end records 1,230＝裁決 1,230、dropped 0、view_tick_invalid 0）
```

- 構成：Intel Mac（Match＋Gateway，`mac.sh run`，Mac IP `192.168.50.64`）＋Windows 3 台，biome，commit `24943706`，移動診斷開著。10:11～10:49。玩家 id 1～7（加入與離開見 Match log），死亡 73 次。
- 收到的 log：win1＝`lag-l2-win1-20261109-101244.zip`（i7-8750H、GTX 1060）；win2＝`lag-l2-win2-20261109-101113.7z`（Core 7 240H、RTX 5050）；win3＝`lag-l2-win2-20261109-101109.zip`（i7-12700H、RTX 4060）。第 3 台的測試員用了 win2 的 kit，所以兩個檔的 `machine.txt` 都寫 `name=win2`；使用者 2026-11-09 說明 zip 是 win3、7z 是 win2，照此放到 `clients/win2`、`clients/win3`（`clients/MAPPING.txt`）。
- 判定（`lag_l2_summary.py`，照宣告）：
  - **S1 undetermined**：`marks.txt` 沒有 `s1_end`（連續對戰結束時沒有按 `s`）。
  - S2 passed。
  - **R2 failed**：1,056／1,067 個有 view_lag 的 shot，J－A ≠ `cooldown_hold_ticks`。這些 shot 的 hold 都是 0，而 J－A 是 1（828 個）或 2（228 個）；J－A 與 `due_tick`－A 完全相同。也就是 J＝due tick，宣告的「J 與 A 的差＝冷卻保留」漏了動作從接受到 due 的 1～2 tick。這個說明改變判定的前提，依 §14 停下。
  - R1 undetermined（S1、R2 不成立時不給 R）。
- 探索（不是受理判定）：以 J 計的 view_lag n＝1,067，最近秩 p50 3.75、p90 4.29、p95 4.39、p99 5.00、max 14.00 tick；以 A 計 p50 2.49、p99 4.00、max 14.00。整場都算（沒有 S1 區間）。
- 使用者的觀察（readme）：win1 曾嚴重卡頓（其他 Client 看不到）；win1 被命中時沒有受擊畫面與聲音；移動中卡住、跳就能過；重生後有看不見的牆、跳就消失。追加希望：NPC、地圖 1.5 倍且能進建築與上屋頂、霰彈槍與狙擊槍。
- 證據：主 checkout 的 `build/evidence/v8-lag-l2-session-20261109/`（`received/` 原樣、`session/` 判定用的複本與 `lag_l2_summary.json`、`exploration_view_lag.txt`）。
- **使用者決定（2026-11-09）：這次場次當探索**（「今回は探索扱い」）。R 不由這次決定；下一次決定 R 時，另寫宣告並改寫 R2 的前提（J－A＝due－A＋冷卻保留），也要讓 `s1_end` 不靠記得按鍵。
- **探索：以最晚結束的 Client 當 `s1_end`（使用者 2026-11-09 指示）**。最晚的是 win1 的 `machine.txt` `end=10:40:18`（win3 10:40:14、win2 10:37:40）。在複本 `session-explore-s1end/` 的 `marks.txt` 加上 `s1_end=2026-11-09T10:40:18` 後，照宣告的工具重算。區間是場次之後才定的，**不當受理判定**（§14.1）。
  - S1 passed：區間依工具規則從 `s1_end` 前最後一次加入（player 7，10:23:01）到 10:40:18，1,036.9 秒、有效 shot 627，三人各 ≥100（286、194、147）；區間前排除 455 個。
  - S2 passed；宣告的 R2 仍 failed（1,056）。改寫後的前提「J－A＝(due－A)＋冷卻保留」：1,067 個 shot 不一致 0。
  - 區間內以 J 計：p50 3.85、p90 4.30、p95 4.41、p99 4.57、max 5.04 tick；R 8～15 時被夾取的比例都是 0。
  - 照宣告的規則 min(15, ceil(p99)＋2) 算出 **R＝7 tick（約 117 ms）**——探索值。
  - 證據：主 checkout 的 `build/evidence/v8-lag-l2-session-20261109/session-explore-s1end/`（`lag_l2_summary.json`、`exploration_r.txt`）。

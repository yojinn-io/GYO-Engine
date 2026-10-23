# PvP v7.1 edge-rate-limit：限速改成 token bucket（K1），並跑 edge-actions-passthrough 的 L2

日期：2026-10-18。**完成**（限速修正 `7bb1f84`；分析器重新凍結 `c82ecc5`、雜湊漏改修正 `ed3481c`；L2 通過，紀錄 `548bff1`）。edge 系列在這批完成。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78) 合併（`0fabb52`，2026-10-20）。
計畫見 [v7.1 README](../object_fps_pvp/plans/v7.1/README.md) 的 `edge-rate-limit` 列；L2 宣告見 [edge-rate-limit 的 L2 事前宣告](../object_fps_pvp/plans/v7.1/edge-rate-limit-l2.md)（第 3 版，結果在第 9 節）；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D118、D119、D121～D123 與 K1 條目。

## 做了什麼

- 重新測試 K1（edge-actions-passthrough 的 L2 第 2 次 session 停下的根因）：先寫兩個失敗的測試。產品層把 `gateway-1000ms-r4-B` 那次 player 1 的真實到達序列（1089 個）重放給 `session.Admit`，現行拒收 53，和當時 Gateway 的計數相同；通用層用合成資料（每 11 ms 一個、讀取端停頓 1 秒），現行拒收 62。另加洪水上限測試。
- 評估方案 (a)（token bucket，每秒補 120、容量 120）：重放與合成都是 0 拒收；長期速率不變，任何 1 秒滑動區間最多 240，和固定窗口跨邊界時相同；Gateway 停頓超過約 1.3 秒時仍會拒收（驗收裡最長的停頓是 1 秒）。對照 edge-contract 的白名單：額度與上次補充時刻留在通用層 `session.Session`，屬於「connection and session」類，產品層不加欄位。也列了 (a′)、(b)、(c)、(d) 等替代做法，停下等使用者決定。
- 使用者採用 (a)（D121）：通用層 `services/gyo_gateway/session/session.go` 改成以 GCRA 整數時間實作的 token bucket，被拒的封包不扣額度、閒置額度不超過容量；network-architecture 的 Gateway 圖與第 08c 批說明同步改寫，K1 改成已處理。
- 凍結分析器的 Gateway 窗口計數檢查（`max_session_window_packets ≤120`）在 token bucket 下會誤停（停頓後同一秒放行約 170）。使用者選 (a)（D122）：三處改成限速器自己的拒收計數 `rate_limited_packets`＝0，gateway 模式也判；窗口計數只記錄。重新凍結為 `frozen-pv7-rl.sha256`。

## 驗證與經過

- Red → Green：真實重放 53 → 0、合成 62 → 0；既有的限速測試不變，新增「閒置 10 秒後瞬間洪水只放行 120」。突變新增 4 個（`v71rl-*`）全部殺死；CTest 72／72。
- Architecture Delta：engine owner 的通用層限速算法改變，上限數字與介面不變，使用者只有 `object_fps_pvp`；依據是產品的 L2 證據，算法本身與產品無關。
- D122 的經過：第一次提交（`c82ecc5`）時 CTest 有 1 項失敗（兩個 runner 把 `action_probe.py` 的雜湊寫死），指令串沒有在失敗時中止，提交與推送照樣做了。之後更新兩個常數、補進凍結清單（`ed3481c`），宣告的 A' commit 由 `7bb1f84` 改成 `ed3481c`。
- L2 宣告第 1 版經 D45 檢查（必須修正 2、應修正 5），M1 由 D122 決定，第 3 版由使用者核准（D123）。兩棵樹都帶限速修正：B'＝`1802f08`（`f79bb10`＋只套用限速修正，本機分支、不推送）、A'＝`ed3481c`。B' 開發跑次 4 個全部通過；用新 judge 重判之前兩次 session 的 131 項，停止只剩修正前的 K1 與 rapid 收尾競態。
- `ed3481c` 的四平台 L1 在 macos-x64 有 1 個 C++ 測試失敗（`MovementRecoveryTests.cpp:456`），同樣的 C++ 在 `7bb1f84` 通過、本機重複 200 次 0 失敗；只重跑失敗的 job 後通過，原本的失敗保留，原因未切分，列為未結事項 `N-movement-recovery-flake`。
- L2 session（使用者不用電腦的時段，跑前關掉 Chrome、暫停 PR #78 的 Auto-fix）：168 項全部跑完、沒有停止、完整性問題 0，**通過**。K1a 168 項全部 0 拒收；K1b gateway-1000ms 10 跑次全部成立（停頓中每位玩家恰好 1 次 starvation，恢復後 0 次）；受干擾 2 項。edge-actions-passthrough 的判定也在這次 session 成立。

## 證據

`build/target/_build/test/logs/pvp-v71-edge-rate-limit-20261018/`（`frozen-pv7-rl.sha256`）、`pvp-v71-edge-rate-limit-l2-prep-20261018/`、`pvp-v71-edge-rate-limit-l2-20261018/`；K1 停下的跑次在 `pvp-v71-edge-actions-l2-20261018/`，定位經過見交接 2026-10-18 05:02 條目（git 忽略）。

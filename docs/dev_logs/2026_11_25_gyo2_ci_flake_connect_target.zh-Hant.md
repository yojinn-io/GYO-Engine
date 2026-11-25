# 2026-11-25 `net.connect-target` 斷線等待改成有上限的等待（`v8-arch-gyo2-ci-flake-connect-target`）

Owner：Engine（net 的測試）。GYO2 主會話轉來的 hotfix（使用者「fix CI」），作者 Opus 5.5。

## 現象

gyo2 全量 CI run 45591557592（head 9add7bec）：`Quick acceptance / toolchain / macos-arm64` 的「Build and test core without the SDL Plugins」，`tests/common/net/ConnectTargetTests.cpp:187`「the server took the player off (the Disconnect went before the close)」失敗；同一 job 前一步（含 SDL）的同一個測試是過的。CI 上偶發，本機未重現（修前修後各跑 10 次 `connect-target:*` 全過）。

## 原因與修正

- 「J4 J5 over UDP」一案換成沒有目標的 World 後，固定跑 5 幀就判定伺服器已移出玩家。走的是真的 UDP loopback，Disconnect 在忙碌的 runner 上 5 幀內不一定到。同一案前面等連線用的是「最多 200 幀、條件成立就停」。
- 改成同樣的有上限等待：最多 200 幀，Client 的 socket 關了且伺服器的 peers 空了就停。兩個判定（`[nt-target-socket-closed]`、伺服器移出玩家）原樣不變。這是修正測試「固定幀數」的錯誤前提，不是放寬判定（AGENTS §2.5）。
- 同檔其他固定幀數：`Pair`（MemoryLink，確定的）、沒有 socket 的單機、類型不符與開不了 socket 的 link 失敗（本地、確定的），都不等網路封包，不改。

## 驗證

- 本機（Intel Mac）：`ctest -R "^net\."` 18/18；`connect-target:*` 連跑 10 次全過。
- 突變：`nt-target-socket-kept`（cases 是這一案）killed（等滿 200 幀後判定失敗）。
- 沒驗：CI 上的偶發是否消失（L1 dispatch，之後的全量 CI 看）。

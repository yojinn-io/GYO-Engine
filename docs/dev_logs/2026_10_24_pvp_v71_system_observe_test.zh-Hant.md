# PvP v7.1 after-re-evaluate：系統其餘＋觀測＋測試

日期：2026-10-24 起。分支 `claude/pvp-v71-system-observe-test`（從 master `23fab75`）。

**狀態：進行中**。D159 的三條 PR 線之中的第二條（第一條 sdl-isolation 是 PR #85，第三條「發行」之後另開）。批次的內容、驗收與決策點見 [after-re-evaluate 第 9 節](../object_fps_pvp/plans/v7.1/after-re-evaluate.md)。使用者 2026-10-24：「after-re-evaluate 按順序來，開個PR。」

## 批次

| 批次 | 檔位 | 估時（h） | 狀態 |
|---|---|---|---|
| `silent-branch-asserts` | high | 4～6（C3 另加 1～2） | 實作完成，L1 待確認（D180） |
| `client-silent-checks` | high | 4～6 | 實作中（D181 新增；D183 決定） |
| `cxx-flag-removal` | high | 3～4 | |
| `rebind-on-new-port` | 先開關卡，局部 xhigh | 5～8 | D181 新增（K5、K6） |
| `contract-readers` | medium | 3～4（K7 另加 2～3） | D181 併入 K7 |
| `trace-no-overwrite` | high | 4～6（K8 另加 1～2） | D181 併入 K8 |
| `analyzer-converge` | high | 5 | |
| `token-tests` | high | 4～6 | |
| `client-path-tests` | high | 3～4（K9 另加 2～3） | D181 併入 K9 |

時間框：任一批超過估時 1.5 倍，或單一問題累計超過 2 小時，就停下回報。

## silent-branch-asserts：無聲分支改成 assert 或計數

列點與決定見 [silent-branch-asserts.md](../object_fps_pvp/plans/v7.1/silent-branch-asserts.md)（D180）。

### 做了什麼

- **C++ 的 assert**（`GYO_ASSERT`，每種建置都檢查）：
  - `MatchRuntimeHost.cpp`：A1（身分不同時只會遇到空的暫存）、`find_if` 沒檢查 `end()` 的那一處、A2、A3、A10、A11 兩處；
  - `IpcHost.cpp`：A12（找不到 join 的結果必須是 Leave 的）；
  - `ClientConnection.cpp`：A9。
  - 有副作用的呼叫先存進變數再 assert（`Assert.hpp` 的規定）。
- **Go 端的 assert（g2）**：新增 `invariantFailed`，在自己的 goroutine 裡呼叫 `runtimeFailed`，因為呼叫者可能拿著 `s.mu`。日誌行是 `runtime disconnected reason=other error="gateway invariant failed: …"`，房間變成 unavailable。用在 A4～A8。沒有新增套件變數（`state_whitelist_test.go` 不變）。
- **C1**：Match 給的 `join_rejected` 原因字串，放進 Error 控制封包後超過一個資料包，就當協議錯誤讓 link 失敗。收到時就檢查，所以 `controlPacket` 的編碼失敗改成 assert。
- **C2**：給不在 joining 的玩家的 JoinResult，當成 IPC 契約違反，讓 link 失敗。給已離開玩家的結果仍然忽略（D9，走得到）。
- **C3**：Client 從 Gateway 收到卻丟掉的封包，現在計數。worker statistics 行尾追加 5 個鍵：`unparsed_datagrams`、`early_datagrams`、`invalid_snapshot_datagrams`、`ignored_snapshot_datagrams`、`unexpected_kind_datagrams`（日誌契約 R1 允許只在行尾追加；`log-contract.zh-Hant.md` 已加註）。
- **L1～L7** 保留。INVENTORY 的 `N-silent-branches-assert` 改成已解決，原清單的 `server.go:638-641` 改列保留，v7 HANDOFF 的引用 `:491` 改成 `:492`。
- **U1～U3** 與第 7 節的分支這一批不動；照 D180 的補充，排在 `lan15-windows` 之前處理。

### 驗證

- 本機：`ctest -R object_fps_pvp -L "cpu|network|go"` 48／48；Go `./...` 與 `-race` 通過。
- **worker 驗收**：`worker_main.cpp` 的 mock Gateway 在 Welcome 之前先送 Snapshot 與空的 ActionResults，之後注入解析不了的 Welcome／Snapshot／Error、tick 0 的 Snapshot、長度不一的 Snapshot、舊 tick 的 Snapshot，以及 Gateway 不會送的 Hello／Input／Actions。第一行 worker statistics 是 `unparsed_datagrams=3 early_datagrams=2 invalid_snapshot_datagrams=2 ignored_snapshot_datagrams=1 unexpected_kind_datagrams=3`，驗收要求的下限是 3／2／2／1／3。
- **Go 新測試**（`invariant_test.go`）：
  - 太長的拒絕原因會讓 link 失敗；放得下的原樣轉發；
  - 給非 joining 玩家的結果會讓 link 失敗；
  - 給已離開玩家的結果被忽略。
- **突變**：本批新增 14 個（`batch` 是 `v71-silent-branch-asserts`），14／14 killed。
  - assert：故意弄壞上游的前提（不推參考、Leave 不刪品質資料、不記錄 Join 請求），A10、A11、A12 各自以 `GYO_ASSERT failed: …` 終止；
  - C3：每條丟棄路徑不計數各 1 個，加上統計行少一個鍵；
  - Go：C1、C2 的兩半、`invariantFailed` 變成無聲。
  - 既有突變 `v7-flow-fwd-handed-actions-after-commit` 的 find 改成新的程式碼，期望字串不變。
  - **改動檔案上的既有突變重新判定**（當時的規則；之後由 D184 改成只跑相關的）：230 個，228 killed，2 個 wrong_failure（`v71ip-commit-before-decode`、`v71ap-commit-before-decode`）。
    - 原因：這兩個突變在解碼前就提交序號，之後那次正常提交就會失敗。D180 之後那是 Gateway 不變量壞了，會讓 runtime 失敗；`actionFixture` 的 link 沒有連線和 `done` channel，關閉時 panic，整個測試程式在本來會報錯的測試之前就倒了。突變仍然被抓到，只是訊息不同。
    - 修正：`actionFixture` 改用 `dialedLink`（`actions_test.go`），讓 fixture 的 link 能像真的一樣關閉。判定與期望字串都不變。重跑 2／2 killed。

### §14.4：提交失敗的測試搬家

- **變更**：`ingress_outcomes_test.go` 的表格案例 `handed/actions_commit_failed` 刪除，改成專用測試 `TestAnAdmittedSequenceThatDoesNotCommitFailsTheRuntime`。原因：D180 之後那條路是 Gateway 不變量壞了、會讓 runtime 失敗；表格的 fixture 沒有 link，失敗的 goroutine 會 panic。
1. **守護的性質**：一個資料包只計一個結果（IG），而且 IG、Gateway 全域守恆、T3、I0 都成立。
2. **放寬的面**：無。新測試在失敗寫出的最終行上做同樣的判定：玩家視窗只有 `received_datagrams=1` 與 `fwd_handed_actions_packets=1`，Gateway 只有 `received_datagrams=1`，再用 `checkIngressLines` 檢查守恆。
3. **收緊的面**：之前不變量壞掉時房間繼續可用也算通過；現在必須留下 `gateway invariant failed` 一行，而且房間變成 unavailable。
4. （沒有放寬的面，不適用。）
5. **證據**：在提交失敗的位置做 4 個臨時突變（多計一個鍵、換成別的鍵、不計、多計一次 received），新測試 4／4 失敗。這幾種都是舊表格判定會判失敗的情況。
- **經過**：第一版搬家（`eaeb40b`）只檢查了兩個鍵，少了「只計這兩個」與守恆，算放寬。完成前自己發現，`c3243f3` 補回後才符合上面的描述。

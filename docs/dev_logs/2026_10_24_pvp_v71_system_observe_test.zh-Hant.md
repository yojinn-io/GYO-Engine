# PvP v7.1 after-re-evaluate：系統其餘＋觀測＋測試

日期：2026-10-24 起。分支 `claude/pvp-v71-system-observe-test`（從 master `23fab75`）。

**狀態：進行中**。D159 的三條 PR 線之中的第二條（第一條 sdl-isolation 是 PR #85，第三條「發行」之後另開）。批次的內容、驗收與決策點見 [after-re-evaluate 第 9 節](../object_fps_pvp/plans/v7.1/after-re-evaluate.md)。使用者 2026-10-24：「after-re-evaluate 按順序來，開個PR。」

## 批次

| 批次 | 檔位 | 估時（h） | 狀態 |
|---|---|---|---|
| `silent-branch-asserts` | high | 4～6（C3 另加 1～2） | 實作完成，L1 待確認（D180） |
| `client-silent-checks` | high | 4～6 | 完成（D183） |
| `cxx-flag-removal` | high | 3～4 | 完成（L1 run 40110961077 成功） |
| `rebind-on-new-port` | 先開關卡，局部 xhigh | 5～8 | 完成（D181 新增；D187 r1＋k3，審查後 D188 改為 Hello 一律 250 ms；L2 12／12 P） |
| `contract-readers` | medium | 3～4（K7 另加 2～3） | 實作完成，L1 待確認（D181 併入 K7） |
| `trace-no-overwrite` | high | 4～6（K8 另加 1～2） | D181 併入 K8 |
| `analyzer-converge` | high | 5 | |
| `token-tests` | high | 4～6 | |
| `client-path-tests` | high | 3～4（K9 另加 2～3） | D181 併入 K9 |
| `event-watch-overlap` | high | 1～2 | D186 新增（K11） |

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

## client-silent-checks：Client 的再檢查與「應為 0」的分支

守衛的追蹤與決定見 [client-silent-checks.md](../object_fps_pvp/plans/v7.1/client-silent-checks.md)（D181 排入，D183 決定）。

### 做了什麼

- **K1**：`PvpApplication.cpp` 對 `SnapshotTimeline::Push` 的回傳值 assert。
- **K2**：`LocalPlayerPrediction::Reconcile` 改成回傳 `bool`（是否採用），`ClientSimulation::Observe` 對它 assert。
  - 和 client-silent-checks.md 的 (b) 不同的一點：沒有加 `[[nodiscard]]`。測試直接呼叫 `Reconcile` 的地方有 117 處，加了每處都會出警告；產品只有一個呼叫端，那裡有 assert。
- **K3**：`ClientConnection::SendInput`
  - r2（不在 Playing／Connecting）、r3（別的玩家）、r4（無效輸入）、r7（改了或少了未確認的命令，守 network-architecture §7「一個命令身分只有一種內容」）改成 assert；
  - r1（別的世代）、r5（epoch 或 life 已被最新 snapshot 取代）計數，worker statistics 行尾追加 `stale_input_windows`（日誌契約 R1；`log-contract.zh-Hant.md` 已加註）。計數在 `SendInput` 的鎖內累加（它在模擬執行緒上跑），統計行在同一把鎖裡取走；
  - r6（全部已確認）保留。
- **K4**：
  - Match 端 S1（`StagedOverWindow`）、S2（`HandoffRejected`）改成 assert；S2 原本寫 trace 的分支變成死碼，一起刪掉。日誌鍵 `staged_over_window`、`handoff_rejected` 照日誌契約 R1 保留，一直為 0；
  - Gateway 的 S3（`ErrInput`）、S4（`ErrActions`）改成 `invariantFailed`，同時保留原本的計數，維持每個資料包只計一個結果（IG）。

### 驗證

- 本機：`ctest -R object_fps_pvp -L "cpu|network|go"` 48／48，沒有任何 assert 觸發；目錄、日誌契約、ingress_evidence 的測試通過；Go `./...` 與 `-race` 通過。
- **worker 驗收**：在 `CheckDroppedDatagrams` 裡送一個別的世代、一個別的 epoch 的窗口，要求兩個都沒送出，且第一行 worker statistics 的 `stale_input_windows` ≥2。實測 2。
- **單元測試**：`PredictionTests.cpp` 新增「無效或較舊的權威狀態回報為未採用」。
- **突變**（D184 的挑選：新突變、find 落在本批改動範圍、斷言附近被改過；挑選腳本在會話的工作目錄）：16 個，15 killed，1 survived；存活的那個是等價突變，換成不等價的版本後 killed（見下）。
  - 新增 8 個（`batch` 是 `v71-client-silent-checks`）：K2 的上游前提被弄壞時 assert 觸發、`Reconcile` 把無效或較舊的狀態報成已採用（2 個）、r1／r5 不計數或照送（3 個）、統計行少一個鍵、S2 的上游前提被弄壞。
  - 既有 8 個：find 落在改動範圍 1 個；worker 驗收裡統計檢查那一段被改過，所以重跑 C3 的 7 個。
  - **存活 → 等價突變**：`csc-s2-rotation-keeps-old-life`（Tick 換生命時不清掉舊生命的暫存）存活。使用者要求補主機層級的擊殺與重生測試（時限 2 小時）。查證後發現補測試也殺不死它：Match 唯一讓 life 加 1 的地方是重生（`PvpMatch.cpp:272`），同一個 Tick 內一定呼叫 `ResetMovementEpoch` 讓 epoch 加 1（`:286`），所以輪替條件裡「life 不同」永遠被「epoch 不同」蓋住。這是等價突變，移出目錄（照 outage-resync 的先例）；產品的 life 條件保留，它寫明意圖、成本為 0。
  - **替換**：`csc-s2-rotation-keeps-old-epoch`（拿掉「epoch 不同」那一半），killed，由 S2 或 A1 的 assert 觸發，證明 assert 在上游前提壞掉時有作用。用時約 15 分鐘。
- **L1**：
  - run 40107059342（`3db1070`）失敗：Quick acceptance / toolchain / macos-arm64 的 Engine CTest `platform_sdl_concurrency`，`EventWatchTests.cpp:300`「Destroying an event watch waits for a callback on another thread」的 `CHECK(overlappedRounds > 0)`，實際 0。歸類：測試本身（靠時序碰運氣的斷言：200 輪中至少一次在回呼執行時解構），環境的時序是觸發條件；不是本線的實作，兩次 run 之間只差突變目錄與文件，Engine 沒有改動。同一個 CTest 先前在 L1 #561（windows-x64，`19e32be` 補了推送執行緒的握手）與 outage-resync 的 L1（windows-x64，`LogBridgeTests.cpp:172`）各失敗過一次。屬於 Engine（input-and-present）的範圍，本線不修，交使用者決定 → D186：列為 K11，新批 `event-watch-overlap`。
  - run 40108527438（`9f31da6`）四平台成功。
- **K1、S3、S4 沒有突變**：K1 的 assert 在 App 端，cpu 測試跑不到；S3、S4 走不到，沒有能讓它們失敗的上游突變（和 D180 的 A4～A7 相同）。

## cxx-flag-removal：不再需要 experimental 程式庫

INVENTORY 的 C16 與 N-osyncstream-experimental。`-fexperimental-library` 只在 AppleClang 加，原本為了 `std::jthread`；`jthread` 在 tt2-trace 之後已從產品消失，只剩 `std::osyncstream`（22 處）需要它。旗標以 PUBLIC 掛在產品 domain 上，所有依賴的目標（測試、驗收 probe）都帶著它。

### 做了什麼

- **`ClogLine`**（新增 `RetroFPS/Pvp/ClogLine.hpp`，產品內、只有標頭）取代 `std::osyncstream`：先把文字組在自己的緩衝區，解構時在一把產品共用的鎖下，以一次 `sputn` 交給建構時 `std::clog` 的 streambuf。和 `osyncstream` 送出時相同：一次傳送、不要求 flush、預設格式（不沿用 `std::clog` 的旗標），支援 `std::fixed` 這類操作子。不經 `ClogLine` 的寫入不和它排序（`osyncstream` 也一樣）。
  - 沒有改用 Engine 的日誌 facade：它的輸出格式是 `[category] level: text`，和產品寫進 `std::clog` 的位元組不同。
- 22 處替換：`match_main.cpp`、`IpcHost.cpp`、`MatchRuntimeHost.cpp`、`ClientSimulationRole.cpp`、`ClientConnection.cpp`；移除 `<syncstream>`。
- 刪除產品 domain 的 PUBLIC 旗標（`apps/object_fps_pvp/CMakeLists.txt`，連同過時的 jthread 註解），以及測試 `digest_o0` 另外加的旗標（`tests/object_fps_pvp/CMakeLists.txt`）。
- **驗收 probe**：`worker_main.cpp`、`network_main.cpp` 還在用 `std::jthread`，以前靠 domain 傳過來的旗標才能在 AppleClang 編譯。改成 `std::thread` 加明確的 join（MockGateway 的解構、8 個 producer、LeaveFailureRelay 原本就有 join）。
- 突變 `ci-resume-drops-window` 的 find 改成 `ClogLine()`。

### 驗證

- 本機（AppleClang，Intel Mac）：`build.ninja` 與 `compile_commands.json` 裡已沒有 `-fexperimental-library`；全部建置成功；`ctest -L "cpu|shader"` 84／84；`ctest -R object_fps_pvp -L "cpu|network|go"` 48／48；目錄、日誌契約、ingress_evidence、start_phase、platform_sdl 7／7。
- **日誌位元組**：既有 golden 測的是組行函式，不受影響；新增 `ClogLine` 的單元測試（`NetworkStatisticsTests.cpp`）：4 個執行緒各寫 500 行，每一行完整、沒有混入別的行；`std::fixed`／`std::setprecision(3)` 生效。
- **突變**（D184 的挑選）：4 個，4 killed。新增 2 個（`ClogLine` 不寫任何東西、忽略操作子）；find 落在改動範圍 1 個（`ci-resume-drops-window`）；斷言附近被改過 1 個（`worker_main.cpp` 的 producer 那段，`wpr-client-body-hit-read-as-head`）。
- 四平台 L1：旗標只在 AppleClang，其他平台應不受影響，由 L1 確認。

### Architecture

- 沒有新的依賴邊；`ClogLine` 是產品內的標頭，取代標準程式庫的 experimental 部分。建置設定只少了一個編譯選項，模組結構不變。

## rebind-on-new-port：換埠後的改綁時機與 Hello 間隔

INVENTORY 之外的已知問題 K5、K6（D181）。關卡與經過見 [rebind-on-new-port.md](../object_fps_pvp/plans/v7.1/rebind-on-new-port.md)。

### 經過

- **關卡（D187）**：約 2 s 的延遲只發生在「上行換埠、下行仍到舊埠」的不對稱情況，Client 不判中斷，只能等每秒一次的 Hello。選 r1＋k3：Client 在「最近 250 ms 沒送 Input／Actions」與「已送出的命令確認滿 T_int 沒前進」時也每 250 ms 送 Hello。實作 `aed91dd`，6 個突變 killed。
- **xhigh 審查推翻了 (b) 的前提**：Match 收不到命令時用替代命令（Held，之後 Neutral）解決下一個序號，確認照樣每個 Tick 前進；純不對稱換埠時 (b) 要等 Starvation 換 epoch 後再過 T_int 才成立（約 t0＋1.1 s）。L2 的 C10 是「先雙向斷 1 s 再換埠」，(b) 剛好成立，worker 測試與計畫中的 L2 都看不出差別；關卡文件第 2 節把 C10 當成純不對稱換埠是誤認。審查也找到條文巢狀錯誤（`037e27b` 修正）與數個測試缺口。
- **重新決定（D188）**：選 P3，Hello 一律每 250 ms，取代 D187。拿掉 (a)、(b) 與其狀態（`ClientConnection.cpp` 相對於 D187 之前只差 7 行）。

### 做了什麼（D188）

- `ClientConnection.cpp`：`HelloInterval` 改成固定 250 ms 的常數。
- 契約：network-architecture §4 的「Hello 間隔」改寫（所有階段一律 250 ms；註明 D188 之前的證據裡 Hello／Welcome 計數是舊規則）、保活與安全取捨的條文、Design Invariants 的守衛清單。
- 預算註解：`Movement.hpp`（Hello 每秒 4 個，116 仍成立）、`worker_main.cpp` 的 static_assert（Hello 由 +2 改 +4，117 ≤120）。
- **worker 驗收** `CheckHelloCadence`：第一份 snapshot 前、輸入流動且確認落後 2 個時、確認停住而 snapshot 仍到時，Hello 都每 250 ms（間隔 ≤350 ms），而且不更頻繁（間隔 ≥200 ms）。MockGateway 加 `ResetAck()`，讓之後的情境從確認 0 開始。

### §14.4：Design Invariants 守衛測試的判定變更（D189，使用者核准）

`test_diagnostic_isolation.py` 的 `test_liveness_reads_only_arrival_times`，liveness 呼叫次數由 12 改成 11。

1. **守護的性質**：網路 worker 交給 `ConnectionLiveness` 的只有資料包到達時讀的 steady_clock 或「現在」，不是時鐘回聲推出的時刻（network-architecture「連線中斷與重新對時」的 Design Invariant「中斷判定只讀到達時刻與常數」）。計數讓每個新的呼叫處都被看到。
2. **放寬的面**：剛好 11 個呼叫的原始碼，以前判失敗、現在判通過。具體是現在的程式：和以前相比只少了一個不帶參數的 `Interrupted()` 讀取（舊的 Hello 間隔用的）。
3. **收緊的面**：12 個呼叫的原始碼（例如 D188 之前的程式）現在判失敗。
4. **性質由哪裡繼續守住**：同一個測試的逐一呼叫的方法與參數表、`polledAt=Clock::now()`／`Poll(polledAt)` 的精確比對、`arrivedAt` 定義成 `Clock::now()` 的檢查。把推導時刻餵給 liveness 的 6 種突變，新舊判定都失敗。
5. **證據**：`206d967` 的 diff；測試輸出 `11 != 12`；突變表（現在的程式：舊失敗／新通過；D188 之前的程式：舊通過／新失敗；Poll 餵回聲時刻、Snapshot 餵 arrivedAt＋1 ms、多一個餵回聲時刻的呼叫、少一個呼叫：新舊都失敗）。獨立審查者在副本上重跑，結果相同，並列出現在的 11 個呼叫，確認唯一消失的是那個 `Interrupted()`。
- 核對時發現的既有缺口：解析器看不到的呼叫（別名、方法名前多空格、區域內重新宣告 `arrivedAt`）新舊判定都擋不住，計數本來也擋不住。列為 K12。

### 驗證

- 本機：`ctest -R object_fps_pvp -L "cpu|network|go"` 48／48（`-j4`）；worker 驗收單獨連跑 5 次通過。
- **一次失敗與切分**：在 `-j8` 並行時 worker 驗收失敗一次（`worker emitted a resend catch-up burst`）。這些時序檢查量的是 mock 收到封包的時刻，CPU 負載高時 mock 可能一次讀進兩個封包。切分：加 16 個忙碌程序的負載，D188 跑 10 次失敗 2 次（`a token wait outlived four commands`），舊規則（暫時改回、跑完還原）同樣負載下也失敗 2 次（`the waiting sends did not drain the token debt`）。歸類：測試本身對負載敏感（環境是觸發條件），不是 D188 的缺陷。這不是事前宣告的受入試行。
- **突變**（D184 的挑選）：D187 的 6 個已移除（對應的程式不存在了）；D188 新增 3 個（Hello 回到每 1 s、每次 worker 經過都送、喚醒晚到 1 s），加上斷言附近被改過的 1 個，4／4 killed。
- **L2**（[宣告](../object_fps_pvp/plans/v7.1/rebind-on-new-port-l2-declaration.md)第 2 版，D190）：
  - 宣告前：探索性試跑 4 項各 1 輪（不判定）發現凍結的「J-R9 epoch」在 C10 會因 D188 判 F。原因是 outage-resync 已知的 P2 例外：C10 恢復後到改綁前的不對稱期間累積的領先量，改綁後約 0.5 s 由 Backlog 清掉。舊行為改綁晚，那次 Backlog 落在計數窗外；D188 讓它落進來。宣告以 N9 取代（§14.4 的 1～5 在宣告第 3 節）。
  - D45 對抗式檢查：6 點，都已修正（最重要的是 N9 第 1 版放得太寬，加上「改綁後最多一次 Backlog」）。檢查中也發現凍結判定會寫回 `ingress-evidence.json`：本會話曾直接重判舊 session 的兩輪，以 `files.sha256` 核對 638 個檔案全部相符。之後的重判都用副本。
  - 結果：session `pvp-v71-rebind-l2-20261025`，**12／12 P**。C10 的改綁從窗口結束後約 1004 ms 縮到約 229～246 ms；C0 的 Hello 每秒 4.00、間隔 249.8～250.4 ms、Welcome＝Hello。
- **L1**：run 40117207196（`56aed98`）四平台成功；之後只改文件。

## contract-readers：契約讀者收進 repo、host_lock 鍵進契約、K7

INVENTORY 的 N-compare-reports-untracked、N-host-lock-keys-uncontracted，加上 D181 併入的 K7。

### 做了什麼

- **compare_reports**：log-contract 指定的 Match `gw_drop` 回報讀者，原本只存在 git 忽略的 drop-report 證據目錄（在主 checkout，`pvp-v71-drop-report-20261016/`）。原封不動搬進 `build/acceptance/object_fps_pvp/compare_reports.py`，位元組相同，以前用雜湊載入它的 L2 工具仍然對得上。
  - 單元測試 `test_compare_reports.py`（5 個：相等通過並列出沒有 final 的流、Match 少讀一則、內容不同、有違規、只看有 match.log 的目錄），CTest `object_fps_pvp.compare_reports`（標籤 cpu、pvp、acceptance）。
  - 凍結清單只檢查列出的檔案（`run_client_delay.frozen_problems`），在目錄裡新增檔案不會讓既有的凍結工具失效。
  - 用 drop-report 的舊證據重跑（工具只讀不寫）：證據目錄那份和 repo 這份輸出逐位元相同，11 個跑次、20 則回報全部相等、0 個違規。
- **host_lock 鍵**：log-contract 的 Match network statistics 列記下 `host_lock_input_*`、`host_lock_step_*`（由 `MatchRuntimeHost` 量，`edge-input-passthrough` 起，L2 的判定工具直接讀）；protocol-v7 的批次表把「IpcHost 層」改正為 host mutex 的擁有者 `MatchRuntimeHost`。分析器的彙總不在這一項的範圍內。
- **K7**：outage-resync 新增的 Client A 級行只有組行函式的 golden。worker 驗收新增 `RequireOutageLines`：每次 `RunOutage`（一般與極端時鐘回聲各一次）期間寫出的連線行，必須依序是中斷（`gap_ms` 500～520、`epoch=1`）、時鐘視窗的 `reason=resume`（`final=0`）、恢復（`gap_ms` ≥1000、epoch 1→1）、第二次中斷、摘要（`interruptions=2`、`max_gap_ms` 等於恢復的 `gap_ms`）、逾時失敗。

### 驗證

- 本機：`ctest -R object_fps_pvp -L "cpu|network|go"` 49／49（多了 compare_reports）；worker 驗收連跑 3 次通過。
- **突變**（D184 的挑選）：新增 6 個（compare_reports 把每個跑次都判相等、忽略違規；K7 的中斷行、恢復行、時鐘 resume 行不寫、摘要不計中斷），6／6 killed；沒有 find 落在改動範圍或斷言附近被改過的既有突變。

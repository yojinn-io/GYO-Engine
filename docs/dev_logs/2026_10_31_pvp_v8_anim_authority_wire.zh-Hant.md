# PvP v8：v8-product-anim-authority-wire（A3：動畫狀態上 wire）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 5～7 h，實際約 2.5 h。

收支：一致（E5 的兩張表每一列都有接受或拒絕的實例；E6 的三段各有一個往返測試；E7 的大小都以 `proto.Size` 實測，沒有推論值）

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-anim-authority-declaration.md)的 A3。Client 還沒有用到這些值，A4 才改呈現。

## 做了什麼

- **proto**：兩份 proto 加上 `JumpPhase` enum（0 無效）、`PlayerAnimation` 訊息（protocol-v9 §4.3 的 8 個欄位），以及 `WorldSnapshot.animation`（client 8／runtime 7，原本就保留給這個號碼）。Go 綁定用 protoc 36.2 與 protoc-gen-go v1.36.11 重產。
- **IpcHost**：把 `WorldSnapshot.animation` 編進 runtime proto。`player_id`、`life_generation`、`jump_phase_tick` 用 `wire::Counter` 轉換；`JumpPhase` 的五個值用 `static_assert` 逐一對照。
- **Gateway**（`adapter.go`）：
  - `SnapshotForClient` 檢查數量一致、`player_id` 在玩家之中且不重複、`validAnimation`（命、權重 ≤ 1000、沒有 sprint 權重、階段 1～4、零點 ≤ tick、活著的玩家要和 `grounded` 一致）。
  - 檢查通過後，逐欄照抄到 client proto。
- **Client**（`ClientConnection.cpp`）：
  - 解碼前先檢查 `jump_phase` 的範圍。如果先轉成 uint8，258 會被讀成 Start。
  - 之後呼叫和 Match 共用的 `ValidPlayerAnimations`。不合法時照既有的方式丟掉這則 snapshot，並計入 `invalid_snapshot_datagrams`。
- **驗收工具**：
  - worker 的 mock Gateway，在 `Snapshot()` 與 `Crowded()` 帶上每位玩家的動畫值；最大欄位的 snapshot 也帶上動畫。
  - `network_main.cpp` 的合法最大 snapshot 帶上動畫。
- **Go 測試的輔助**：pv9 之前寫的測試 snapshot 沒有動畫值。照 `fakeRuntime.send` 自動補上 combat 的先例，在送出點補上合法的動畫值：
  - gateway 套件：`fillAnimation`；
  - adapter 套件：`snapshotForClient`，只在 `Animation` 是 nil 時補。
  - 專門測動畫檢查的測試，直接呼叫 `SnapshotForClient`。`capacity_test.go` 把玩家數截短時，動畫也一起清掉。
- **文件**：protocol-v9 §6 的實測值；`protocol/README.md` 的 `WorldSnapshot.animation` 段落。

## 判定

- **E5：通過。**
  - Go（`TestSnapshotAnimationIsCheckedAndCopied`）：17 種情況都被拒絕。
    - 數量不足、沒有動畫、不認識的玩家、同一位玩家兩次、別的命；
    - `move` 1001、`jog` 1001、`sprint` 1、階段 0／5／258、零點 > tick；
    - Start 卻 grounded、Airborne 卻 grounded、在空中卻是 Land、在空中卻是 Grounded、nil。
    - 另外兩種照舊接受：順序互換、死亡玩家凍結的空中值。
  - C++：同一張表由 A2 的 E4 守住（`ValidPlayerAnimations`，14 種）。Client 解碼另外加上：
    - 範圍檢查：worker 送出 `jump_phase`＝258 的 snapshot，被計為不合法；
    - 共用的檢查：worker 送出 sprint 權重 1 的 snapshot，被計為不合法。
    - 這兩項讓 `invalid_snapshot_datagrams` 的下限由 2 提高到 4（收緊）。
- **E6：通過。**
  - C++ → runtime proto：「PvP IPC carries the animation authority of every player」。實際的 Match 經 IPC 送出 animation，數量等於玩家數，值和 Match 一致（走 3 步：相位 > 0、`move` 500、GROUNDED、零點 ≤ tick、sprint 0）。
  - runtime → client proto：上面那個 Go 測試的照抄檢查（`proto.Equal`）。
  - client proto → Client：worker probe 送出 `{相位 0xFFFF1234, move 500, jog 250, LAND, 零點 1}`，Client 讀到完全相同的 `PlayerAnimation`。
- **E7：通過，實測**（含 24 B 標頭）：
  - 契約最大每人 +34 B：40+177N → 40+211N。4 人＝**884 B**，`capacity_test.go` 與 `worker_main.cpp` 各自釘住。
  - 型別最大每人 +50 B：42+195N → 42+245N。4 人＝**1022 B**，和 protocol-v9 §6 的推論相同。
  - 合法值的 snapshot（`network_main.cpp`）：706 → **842 B**。
  - 都 ≤ 1200 B。契約最大的推論原本是約 896 B，那是把 sprint 權重算成 ② 之後的 1000；現行的規則是 0，不會編碼。

## 改了判定的既有測試（AGENTS §14.4）

| # | 測試 | 改法 | 寬鬆面 | 收緊面 |
|---|---|---|---|---|
| W2 | 大小的釘（`capacity_test.go` 兩個公式、`worker_main.cpp` 748 → 884、`network_main.cpp` 706 → 842） | 換成實測值，仍然是完全相等 | 無 | 無（多了欄位，每一個都計入） |
| X1 | `test_wire_counter_sites.py`：`IpcHost.cpp` 的窄化 setter 數 30 → 33，窄化欄位清單加入 `jump_phase_tick` | 新的編碼點要經過 `wire::Counter` | 無 | 有：多守 3 個 setter |
| X2 | worker 的 `invalid_snapshot_datagrams` ≥ 2 → ≥ 4 | 加上 2 個不合法的動畫 snapshot | 無 | 有 |

X1、X2 只有收緊面，照 §14.4 由我改，事後揭露。舊判定會失敗的輸入，新判定照樣失敗：X1 的 setter 少一個時仍然不相等；X2 的下限只升不降。檢出力見「突變」：`a3-client-range-unchecked` 與 `a3-client-unchecked` 都是由 X2 抓到的。

## 經過（AGENTS §14.1）

- 第一次跑 Go 測試時，因為測試裡的 snapshot 沒有動畫值，Gateway 回錯誤、讓 runtime 失效，測試一直等到 4 分鐘逾時。這是預期中的結果，原因見上面的「Go 測試的輔助」。
- **worker 偶發失敗一次，原因分不出來。**
  - 經過：全部 CTest 以 `-j4` 跑時，`object_fps_pvp.worker` 失敗一次。那次沒有加 `--output-on-failure`，失敗訊息沒有留下來。
  - 之後的重跑都通過：序列 2 次、5 個測試一起 `-j5` 3 次、全部 `-j4` 2 次，共 7 次。
  - 能確認的事實：worker 量時間（重送間隔等），平行負載會影響它；這批在 `CheckDroppedDatagrams` 多送了 2 個 snapshot。
  - 結論：分不出是實作、測試本身，還是環境。這次執行不是受入判定的試行，我也不宣稱已經修好。四平台 L1 時要特別看它。

## 突變

（待補）

## 其他測試

- `test` preset 的 CTest 111／111、`go test ./...` 都通過。

## 範圍的限界

- 只在這台 Intel Mac 上跑過；四平台由 L1 檢查（`pv9-single-merge`）。
- Client 只收下、檢查動畫值，還不拿它播放（A4）。

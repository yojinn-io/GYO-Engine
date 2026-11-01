# PvP v8：v8-product-hitbox-shot-wire（S3：Match 接上姿勢模型）

日期：2026-11-01。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 3～4 h，實際約 2 h（含開工時的停下約 50 分鐘）。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-shot-declaration.md)的 S3，範圍照 `v8-product-hitbox-shot-j7-split` 調整過（見「開工時的停下」）。`v8-product-hitbox-shot` 三個子批全部完成。

## 開工時的停下（AGENTS §14）

J7 要求「用真 Match 開槍的驗收在新判定下照常通過」。開工後先在改動之前跑了一次，兩支都已經失敗：

1. `run_network.py`：把 Match 複製到沒有內容的暫存資料夾，證明「不需要資產也能啟動」。pv8 起 Match 要讀 `movement_rules.json`（`4c9cbbea`，2026-10-30），第一個無效埠檢查就失敗（`Cannot open movement rules`）。life-cleanup 的 `d.damage==25` 也還是 pv9 改成千分之一點之前的值。
2. `action_probe.py --case baseline`：分析器要求 hp＝上限減掉所有傷害，6 秒內玩家 2 死亡（hp 0），「HP differs」與「Snapshot combat effect differs」不成立。原因推測是分析器沒有考慮死亡，未查證。

兩支都不在 CTest／CI，宣告裡「CI 的真 Match 驗收」寫錯了。另外，宣告的 J6（行程內 IPC 測試）抓不到「Match 執行檔仍傳合成模型」。停下回報後，使用者決定 `v8-product-hitbox-shot-j7-split`：J7（與 W1、W2）移到另一批 `v8-product-real-match-acceptance-repair`；J6 改成跨行程檢查。

證據：主 checkout 的 `build/evidence/v8-hitbox-shot-wire-stop/`（Intel Mac；`network/`、`network.out`、`action-baseline/`、`action-baseline.out`）。

## 做了什麼

- `match_main.cpp`：用啟動時已載入的姿勢定義（D3）與形狀（D1）做 `MakePosedPlayerHitModel`，交給 `MatchRuntimeHost`；log 多一行 `hit model=posed shapes=7`。
- `network_main.cpp` 新增 `--hit-parts-only --content DIR`：兩個 Client 進房，射手對站在出生點的目標先射頭、再射腿。瞄準點用同一個姿勢模型，從射手的 snapshot 擺出目標，取該部位第一個形狀的中點；眼睛高度用 `StanceEyeHeight`。要求 Client 收到的 decision：accepted、命中玩家、目標正確、部位與傷害符合，最後目標的 hp＝上限 − 50000 − 18750（從射手的 snapshot 讀）。probe 因此多連結 `player_content`。
- `tests/object_fps_pvp/hit_part_cross_check.py`（CTest `object_fps_pvp.hit_part_cross`，標籤 network;go;pvp）：建 Gateway、啟動 Match 執行檔、跑 probe，並確認 Match log 有 `hit model=posed shapes=7`。放在產品測試資料夾，理由和 `ingress_cross_check.py` 相同。
- 文件：protocol-v9 §8 寫入 `v8-product-hitbox-tick-pose` 的決定；hitbox-shapes 的讀者寫上實際的判定與 probe。`ShotQuery.hpp` 的註解在 S2 已改成形狀。

## 判定

- **J6：通過。** 真 Gateway＋真 Match 執行檔：頭 → `part=2`（HEAD）、`damage=50000`；腿 → `part=3`（LEG）、`damage=18750`；目標 hp 31250。這是 LEG 第一次由真的判定產生，經 IPC、Gateway 送到 Client。
- **J7：移出本批**（`v8-product-hitbox-shot-j7-split`）。

## 經緯

- 第一版的 hp 檢查讀 `b.State().snapshot->combat`：在 range-for 裡直接用暫時物件的成員，迴圈開始前物件已經銷毀，讀到空的。不是 Client 的缺陷；改成先複製狀態，並改讀射手的 snapshot。
- 這次沒有改既有測試的判定（§14.4 不適用）。宣告的 W1、W2 隨 J7 移出；W3、W4 在 S2 已處理。

## 突變

**1／1 killed**（只在預期的訊息失敗時才算 killed），在未提交的工作樹上用 `--allow-dirty` 跑。

- 本批新增：`s3-match-synthetic-model`（Match 執行檔不傳姿勢模型）。新的 `hit_part_cross` 檢查失敗在 `Hit-part shot at the head decided accepted=1 kind=2 … part=1 damage=25000`，也就是合成模型把頭判成 Body。
- D184 相關的既有突變：0 個。`match_main.cpp` 既有的兩個突變，find 不在改動的行；network probe 沒有被突變用到的檢查（`--self-test` 沒有突變）；既有測試都沒有改。

## 其他測試

- `test` preset 的全部 CTest（`-j4`）：112 個，111 通過，**`object_fps_pvp.worker` 失敗**（第 4 次偶發）。這次的完整輸出有存檔：失敗在 `an interrupted line has the wrong gap or epoch`，gap_ms＝590（要求 500～520）。
- worker 單獨連跑 3 次：2 次通過，第 3 次失敗在另一個時間檢查 `unacknowledged window was not resent at approximately 60 Hz`（最大間隔 68.41 ms）。當時本機負載約 5～6。
- 切分（§14.1）：S3 沒有改 worker 走的路徑（`ClientConnection` 與 worker 都沒動）；失敗的都是時間窗的檢查，而且每次不同。推測是環境（負載）加上測試本身的時間窗很窄，但分不出實作是否也有份。A3 起記過，pv9 的 L1 要看 Linux／Windows 上會不會出現。
- `hit_part_cross` 與 `ingress_cross` 一起跑：都通過。
- 證據：主 checkout 的 `build/evidence/v8-hitbox-shot-wire/`（Intel Mac；`ctest-full.log`、`worker-serial-1..3.log`、`build.log`）。

## 範圍的限界

- 跨行程檢查只覆蓋站著不動的目標。其他姿勢由 S2 的 J2、「every kind of input」守住。
- 新 CTest 需要 Go，和 `ingress_cross` 一樣不在 cpu preset；CI 有沒有跑到它，要在 pv9 的 L1 確認。
- 真 Match 的 `run_network.py`、`action_probe.py` 仍然壞著，留給 `v8-product-real-match-acceptance-repair`。
- 合成模型改判 Leg 時，`CombatHostTests` 附近有一個既有測試會卡住（S2 記錄）。真的 Match 現在會產生 Leg，那個測試用的是合成模型，不受影響；原因未查。

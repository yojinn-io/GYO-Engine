# PvP v8：v8-product-hitbox-b5（A2：Client 畫的骨骼與 Match 判定的骨骼差多少）

日期：2026-11-03。分支 `claude/pvp-v8-after-pv8`。上段｜Opus 5.5／high。估時 2～3 h，實際約 1 h。

**狀態：完成（只量不判，`v8-product-hitbox-b5-threshold` (a)）。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-acceptance-declaration.md)的 A2。

## 做了什麼

在 `PlayerHitModelTests.cpp` 加一個量測用的測試「PvP B5: shapes of the Client's interpolated body against the Match's judged body」：

- **權威序列**：真的 `PvpMatch`，用產品的動畫規則，每個情境跑 240 Tick。受擊直接寫進錄下的戰鬥狀態（每 25 Tick 一次，頭與軀幹交替），兩端讀的是同一份權威值。
- **Client 側**：照 `SnapshotTimeline` 的取樣方式。
  - 位置、yaw、pitch 在兩個 snapshot 之間內插，戰鬥狀態取前一個 snapshot，動畫用 `InterpolatePlayerAnimation`；
  - locomotion 狀態照時間順序延續，和真的 Client 一樣；
  - 每個 Tick 間隔取 α＝0.25、0.5、0.75 三個點。
- **Match 側**：Tick J 判定用的形狀（姿勢模型）。
- **比較**：7 個形狀端點的最大距離。另外拆成兩部分，再量 Match 自己從 J 到 J+1 的移動：
  - 只看內插：Client 改用每次重來的 locomotion 狀態；
  - 只看 Client 的歷史：延續的狀態對每次重來的狀態。

## 結果（Intel Mac，每個情境 537 個樣本，單位 m）

| 情境 | Client 畫的對 Match 判定的（P50／P95／最大） | Match 自己每 Tick 的移動（P50／P95／最大） |
|---|---|---|
| idle | 0.0002／0.0005／0.0005 | 0.0005／0.0007／0.0007 |
| 跑步並轉向 | 0.041／0.074／0.087 | 0.083／0.108／0.116 |
| 往前跳 | 0.038／**0.326**／**0.546** | 0.072／0.465／0.579 |
| 蹲下橫移 | 0.020／0.052／0.076 | 0.038／0.093／0.101 |
| 跑步中受擊 | 0.041／0.074／0.089 | 0.083／0.112／0.119 |

- **Client 的歷史貢獻 0**（所有情境的 P50、P95、最大都是 0）。差距全部來自同一個 Tick 之內的內插：Client 畫的是 J 與 J+1 之間的姿勢，Match 判定的是 J。這和 A1 的設計一致：姿勢只取決於當下的權威值。
- 差距都小於 Match 自己一個 Tick 的移動，所以 B5 量到的就是「判定每 Tick 取樣一次，兩次之間畫面還在動」。
- **跳躍是例外地大**：起跳與落地的動作把整個 clip 壓進 0.1 s（`jump_start_seconds`、`jump_land_seconds`，6 個 Tick）。Match 自己一個 Tick 就移動最多 0.58 m，畫面和判定最多差 0.55 m。原因是推測（clip 被壓縮後動得很快），沒有逐一量 clip 長度。
- 已知的另一個差距：判定的 Tick 動作重新開始時最多 0.14 m（S2、P1 的量測），兩者是不同的現象。

## 突變

照宣告：選 (a) 只量不判，不登記突變。

## 範圍的限界

- 只用合成的權威序列，沒有網路的抖動與遺失。
- 沒有包含延遲：真的射擊是在 J 之後很多 Tick 才判定，那一段由 rewind-line 處理；B5 只量一個 Tick 之內的部分。
- 跳躍的 0.55 m 要不要處理（例如放寬起跳與落地的跨度，或讓判定看 Tick 內的時間），不在本批；在 L2 的觀感裡對照，之後交使用者決定。
- 證據：主 checkout 的 `build/evidence/v8-hitbox-b5/b5.txt`（Intel Mac）。

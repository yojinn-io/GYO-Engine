# 文件健檢（2026-10-21）

基準：master `5ee499a`（PR #82 合併後）。上一份報告的基準是 `0f0bc91`；之後合併了 PR #75～#82，共 99 份 md 有變更。
範圍：全部 244 份追蹤中的 md，不含 `third_party/`。判斷標準與步驟見 [README](README.md)。

## 結論

- **pv7 已是現行協議，但兩份入口文件還停在 pv6。** 程式端已確認是 pv7：`Wire.hpp` 與 Go `adapter.go` 的 `ProtocolVersion` 都是 7，proto 檔是 `client_v7.proto`／`runtime_v7.proto`。但下面兩份入口文件還在講 pv6 的內容，有些甚至是 pv7 已經禁止的 Gateway 行為：
  - `docs/object_fps_pvp/network-architecture.zh-Hant.md` 的開頭、§1 的圖、§3、§4、§5 的生成指令，以及 §7 的修改入口表。
  - `apps/object_fps_pvp/protocol/README.md`。
- **Trace 模組（TT-2）已隨 PR #82 合併，Engine 與共通文件還沒跟上。** 計畫文件還寫「未合併」，`architecture.md` 與 README 三語都沒有 `engine/trace`。
- **機械檢查大致乾淨。** 斷鏈 1、壞錨點 0、多語版本 0 差異；沒有文件再拿舊的「不可破壞的契約」當標題。
- v7.1 還沒結束：after-re-evaluate 那組未完成，穩定基線也還沒建立。v8 仍是「未開始」，和實際一致。

## 一、PvP 文件

### 高

| 位置 | 過時的內容 | 實際狀態 |
|---|---|---|
| `network-architecture.zh-Hant.md:7-8` | 「目前 wire 為 pv6」，pv7 在 v7.1 | PR #78 起 pv7 生效；`protocol-v6:5`、`protocol-v7:4` 都寫現行是 pv7 |
| `network-architecture.zh-Hant.md:116-117`、`:128`、`:131`、`:148`、`:154`、`:159`（§1 的圖） | Gateway 有「strict 30 Hz action timer; written-dedup; reject lane」，runtime link 負責合併、已寫出去重、拒絕線；協議標 pv6 | pv7 的 Gateway 是無狀態邊緣：原樣轉送、每位玩家一個有界佇列、丟棄回報、心跳、時間回聲。同檔 §7 的 Design Invariants 也這樣寫，兩處直接矛盾 |
| `network-architecture.zh-Hant.md:442-487`（§3） | 來源寫 `client_v6.proto`／`runtime_v6.proto`、版本 6，標題「UDP（pv6）」「IPC（pv6）」；RuntimeEnvelope 少了 ClockSample、GatewayDropReport；Ready 寫「容量 2」 | pv7；這兩個訊息已加入；房間上限 4 人（v6 第 16 批起） |
| `network-architecture.zh-Hant.md:624`（§5） | Protobuf 生成指令指向不存在的 `*_v6.proto`，照著跑會失敗 | 應改用 v7 檔名，與協議 README 一致 |
| `apps/object_fps_pvp/protocol/README.md:3-7`、`:24-33`、`:56` | 正式版使用 pv6；Gateway 合併輸入窗口、去重已寫出的命令、持有動作帳本與退役游標 | pv7；這些行為已在 edge 系列刪除。同檔後段已改用 `client_v7.proto`，前後自相矛盾 |

### 中

- **network-architecture 的其他位置：**
  - `:851`、`:854`：修改入口表仍寫 runtime link 負責合併、已寫出去重、拒絕線、動作節拍；線上資料契約仍指向 v6 的 proto。
  - `:820`：內文還寫「上表的『不可破壞的契約』」，但標題已改名為 `Design Invariants`。
  - `:506`、`:509`：Join 範例寫 `"protocol_version":5` 和「最多兩人」。
  - `:1250` 起：版本變更列表只到 v7，沒有 v7.1／pv7。
- **v7.1 的狀態行：**
  - `v7.1/README.md:3-4`：停在 2026-10-16 的狀態，但同檔的樹狀總覽已全部打勾。
  - `v7.1/HANDOFF.md:3`、`:41`、`:43`：還寫「PR #82 合併中」，實際已合併為 `5ee499a`。
  - `a1-shot-step.md:3`、`:112`：寫「等核准」，第 12、13 節已記錄結案。
  - `pv7-checkpoint-l2.md:3`：寫「等核准」，已完成。
  - `tt2-trace.md:3`：寫「實作中」，第 14 節已寫「L2 通過」。
- **舊版入口沒有交代最新狀態：**
  - `v4/README.md:5-6`、`v4/HANDOFF.md:4`：停在 v5 時期的狀態。
  - `v5/HANDOFF.md`、`v5/STABLE_BASELINE.md`：沒有指向 v6、v7。
  - `v6/HANDOFF.md:3`：寫「現行 wire 是 pv6」。
  - `v7/README.md`、`v7/STABLE_BASELINE.md`、`v7/HANDOFF.md`：沒有寫 pv7 已在 PR #78 合併。
- **斷鏈：** `plans/v8/README.md:40` 的 `../network-architecture.zh-Hant.md` 少了一層，應為 `../../`。

### 低

- **v7.1 HANDOFF：**
  - 代號對照寫「D86～D122」，實際到 D156。
  - edge 系列少了 `edge-rate-limit`。
  - 「未結事項」有幾項已處理，但沒有標結果。
- **更新日期比內容舊：** `v7.1/INVENTORY.md:3`、`v8/README.md:3`、`v7/HANDOFF.md:3`。
- **待補的後續註記：**
  - `v7/HANDOFF.md:48`：D58 的兩項已在 v7.1 `log-contract`（D142）定案，可以補一句後續。
  - `tests/object_fps_pvp/fixtures/ingress_v1/README.zh-Hant.md:3`：仍寫「第 10 批決定」，應改指向 `log-contract.zh-Hant.md`。
- **不在 repo 的工具：** `log-contract.zh-Hant.md:23-29` 列出的 `judge_l2.py` 不在 repo 裡（在證據目錄），應比照 `compare_reports.py` 註明位置。
- **v7 dev log 少了合併紀錄：**
  - 第 03、03a、04、05 批沒有寫隨 PR #71（`660310e`）合併。
  - 第 06、07 批與 P2-log 沒有寫隨 PR #73（`6d3f287`）合併。
- **沒有標成歷史的 FireGate 路徑：** `v7/README.md:290,299`、`v7/09-firegate-c2.md:8` 提到的 `FireGate.hpp` 等，已在 v7.1 退役（D90），文中沒有註記。

## 二、Engine 與共通文件

### 中

- **time-threads-trace 計畫：** 以下位置還寫「TT-2 未合併」或「實作中」，實際已隨 PR #82（`5ee499a`）合併：
  - `README.md:4`、`:34`
  - `PLAN.md:3`
  - `HANDOFF.md:3`（「TT-2 紀錄」也沒有合併的條目）
  - `foundation-followups/inactive_products.md:131`
- **`docs/architecture.md` 沒有跟上 TT-2：**
  - 目錄樹少了 `trace/`。
  - 責任表沒有 `GYO::Trace`，`GYO::Base` 那一列也沒提 Log facade（`Log.hpp`）。
  - 沒有 TT-2 的 Architecture Delta 段落；TT-1 有。
  - 沒有連到 `docs/architecture/trace.md`。
- **README 三語：** 模組清單少了 Trace，設計資料清單也沒有 `trace.md`。

### 低

- `docs/dev_logs/2026_10_20_engine_tt2.zh-Hant.md:3`：仍寫「尚未合併」，應補一行後續。
- `services/gyo_gateway/README.md`：沒有提到 session 的 token bucket 限速。這不是錯誤，只是不完整；產品文件已經寫了。
- **AGENTS.md：**
  - 同一個意思有三種寫法：「受け入れ検証」（§7）、「受け入れ判定」（§14）、「受入検証」（README.md）。
  - §3 要求 Design Invariants 寫在「`docs/<owner>/` 配下」，但 Engine 的文件實際放在 `docs/architecture/`（例如 `trace.md`），照字面會找不到。
- `math-foundation/HANDOFF.md:627` 的「AGENTS §14」：指的是當時的 Before Completing，現在是 §15。這是歷史紀錄，維持原樣。

## 三、機械檢查

| 項目 | 結果 |
|---|---|
| 連結與圖片 | 不存在的相對目標 1 處（v8 README，見第一節）；絕對路徑 0；壞錨點 0（共檢查 33 個） |
| 反引號中的 repo 路徑 | 找不到的 35 處：24 處已標成歷史、4 處是計畫裡的時點內容；**沒有標示的 7 處**列在下方。另外，network-architecture 裡的 v6 檔名已併入第一節 |
| 繁中用字 | 「緩存」7 處，應為「緩衝」：`network-architecture.zh-Hant.md:784,1052`、`v7.1/HANDOFF.md:416,420,485,603`、`v7.1/edge-rate-limit-l2.md:13,74`<br>「默認」1 處：`v7.1/Incident/2026-10-15-gateway-state-drift.md:131`<br>「接口」6 處：要區分是否為使用者原話。引用保留；`network-architecture` 的敘述句不是引用<br>「受入」1 處：`v7.1/HANDOFF.md:312`，日文詞，不是引用 |
| 多語版本 | README 三語、`releasing`、`rendering_architecture`、`object_fps/acceptance`：結構、連結、反引號內容完全一致 |
| 舊契約標題 | 沒有文件再用舊名稱當標題，只剩 `network-architecture.zh-Hant.md:820` 的內文殘留一處（見第一節） |

沒有標示為歷史、但已不存在的路徑：

- `foundation-followups/HANDOFF.md:57` 與 `PLAN.md:180`：`detail/Hash.hpp`，已刪除。
- `foundation-followups/HANDOFF.md:60`：`ui/UiDocument.hpp`，是 FF-7 之前的路徑。
- `time-threads-trace/HANDOFF.md:99`：`tests/common/time/WakeRecord.cpp`，不在 master 上。
- 第一節最後一項提到的 FireGate 相關路徑。

## 已確認無問題

- protocol-v4、v5、v6 的開頭都寫明現行是 pv7，並指向 protocol-v7。
- protocol-v7：
  - 標頭是「生效（PR #78 起）」。
  - 丟棄原因表和 `runtime_v7.proto` 一致。
  - 引用的 Gateway 檔案都存在。
  - 四處版本定義有一致性測試守著（`object_fps_pvp.protocol_version`）。
- `network-architecture.zh-Hant.md` §7 的 `Design Invariants`：已包含「Gateway：無狀態邊緣」的允許類別、禁止事項、守衛測試，以及 Replacement Criteria（`:775`）。限速相關的 Replacement Criteria 也在（`:812`）。
- 現有的 `Design Invariants` 節：
  - `docs/architecture/trace.md:40`
  - `docs/object_fps_pvp/log-contract.zh-Hant.md:113`
  - `docs/object_fps_pvp/network-architecture.zh-Hant.md:719`
  - 前兩份都附有 Replacement Criteria。
- 兩份事故紀錄與兩個索引頁的狀態都是「已結案（2026-10-21）」，彼此一致。
- AGENTS.md：
  - 節號 §1～§16 連續，節內交叉引用都存在。
  - 其他文件引用 §14 之後的節號，都是 #80 之後寫的，指向正確。
- Gateway 的限速：產品文件和 `session.go` 都已改成 token bucket，彼此一致。
- docs/checkup：
  - 上層 README 和兩個子資料夾一致。
  - 兩份報告列表都和實際檔案相符。

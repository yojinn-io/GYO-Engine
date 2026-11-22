# 2026-11-23 World 摘要與訊息（`v8-arch-gyo2-kernel-summary-messages`）

Owner：Kernel（GYO2 主會話開的任務卡 `local_b3d28b59` 的第二件）。分支 `claude/gyo2-kernel-summary-messages-decl`（開向 `gyo2`）。作者 Opus 5.5；宣告的敵對式檢查與窄複查 Fable 5.2／high（唯讀）。

## 為什麼

asset 宣告第 15 節第 8 項回報：Kernel 的 `World::Summary`（版本 3）雜湊每種訊息的寫入序號、每筆紀錄的序號與游標表。這些是歷史的計數，只增不減：多寫過一筆，就算早已讀完、修剪掉，之後每一次摘要都不同。依完成時序或外部事件發訊息的 Module（網路的 `NetLinkFailed` 等）會讓同樣的狀態得到不同摘要；回滾重跑會再寫一次步裡的訊息，R-2 成立時摘要卻和直跑不同（實測，宣告第 1.2 節）。

## 經過與結論

宣告：[kernel-summary-messages-declaration.zh-Hant.md](../architecture/target/v2/kernel-summary-messages-declaration.zh-Hant.md)。

| 修訂 | 內容 | 檢查 |
|---|---|---|
| 0 | 建議 (e) 訊息與游標都不進摘要 | Fable／high：blocker 2、should 5、nit 4（主 checkout `build/evidence/gyo2-kernel-summary-msg/declaration-check-r0-fable.md`） |
| 1 | 例一改用 `NetLinkFailed`；系譜補上「內容進摘要」是規格 D-9 與版本 1；建議改成 (b) | 主會話要求窄複查（AGENTS §4） |
| 2 | 補 (b) 看不見的（讀者跑過沒有）與例二成立的條件（沒有讀者拉著重跑寫的紀錄）；§14.4 表、RC、突變的路 | Fable／high：blocker 0、should 4、nit 6（`declaration-check-r1-narrow-fable.md`） |

使用者 2026-11-23：「選 (b)，§14.4 五點核准」（§14.4 那句是對第一件的），之後說「開始」。決定照四樣轉給主會話（2026-11-23），HANDOFF 由主會話記（`v8-arch-gyo2-kernel-summary-messages`）。

## 做了什麼

- `engine/ecs/kernel/src/World.cpp`：`Summary` 版本 3 → 4。每種訊息只雜湊留著的筆數與每筆內容（依寫入順序）；拿掉 `nextSeq`、每筆的序號、游標表。`HashMissing` 照舊檢查訊息型別。
- 註解：`World.hpp`（`Summary`）、`detail/Storage.hpp`（`MessageCursor`）、`world_asset/AssetView.hpp`（不雜湊的理由）。
- `tests/common/kernel/MessageCursorTests.cpp`：J-36 的 `[msg-summary-cursors]`、`[msg-summary-seq]`、`[msg-summary-cursor-position]` 改成斷言**相同**，案名跟著改；新案「the same held record with another history gives one World summary」`[msg-summary-record-seq]`（讀者排在寫者前，一份跑兩次、一份跑一次，都留一筆 `Ping{1}`；測試台每次 Run 寫的 `TickCount` 比之前設成同一個值）。
- `tests/common/kernel/WorldTests.cpp`：J-32 的寫死值換成版本 4 的值（`4:5f5fc699…`）。
- `tests/common/rollback/StrategyTests.cpp`：新案「R-2: a message written and read within the step leaves the summary of running straight」`[rb-r2-message-in-step]`（宣告的例二：固定步寫 `Hit`、同一步讀；回滾重跑第 5～9 步，讀到 12＋5 筆；緩衝都空、血量相同、摘要相同）。
- 文件：
  - `target/kernel.zh-Hant.md`：DI-KS 10、DI-KS 27 的摘要那幾句與守衛列（草稿），RC 25 新增（草稿），表頭。
  - `target/README.md`：RC 1～25。
  - 規格 `plans/kernel/spec.zh-Hant.md` D-9：補「不含訊息的寫入序號、紀錄的序號與游標」。
  - 事件游標宣告頂端的更正註記。
  - `rollback.zh-Hant.md` R-2 的守衛列。
  - asset 宣告兩處註記。
  - `v2/README.md`：宣告與本 dev_log 列入。
- 突變清單 `docs/architecture/plans/kernel/scripts/mutations.json`（660 個，數目不變）：見〈突變〉。

## 判定的變更（AGENTS §14.4）

五點寫在宣告第 9 節（修訂 2），使用者 2026-11-23 選 (b) 時一併看過。實作和表相同：三個 J-36 判定翻成「相同」（緩的面：只差在游標或歷史的 World 判相同；緩掉的性質沒有判定再守，決定拿掉，RC 25 記下重評的條件）；`[ks-summary-message-payload]`、`[ks-hash-missing-message]` 不變；J-32 只換寫死值。

Design Invariants 的守衛不論有沒有緩的面都要使用者核准，所以三個翻轉的判定在合併前核准時，連同 DI-KS 10、27 的最終文字與 RC 25 一起交使用者。

## 檢驗

本機 macOS（Intel，RelWithDebInfo，preset `test`），已跟上 `origin/gyo2`（c9796d07）。

- `ctest -L "cpu|shader"`：182/182（`impl/ctest.log`）。
- 突變（`mutation-rerun-scope`：新加的；find 落在改動範圍；殺它們的測試被改過的）：Kernel 的 `msg-summary-next-seq-back`、`msg-summary-record-seq-back`、`msg-summary-cursors-back`、`msg-summary-version-not-bumped`、`ks-summary-message-unhashed-unreported`、`ks-summary-message-payload-dropped` 6/6 killed（`impl/mutants-kernel.log`）；回滾的 `[rb-r2-equal]` 那四個（`rb-tick-not-rewound`、`rb-inputs-not-written-back`、`rb-record-new-only`、`rb-input-record-drops-later`；摘要的內容變了，確認它們仍被抓到）4/4 killed（`impl/mutants-rollback.log`）。每個都照它自己的 expect 算。
- 改動範圍內的突變的 find：22 份清單裡指到改過的 7 個檔的 205 筆，find 各命中一次。改之前 `ks-summary-message-payload-dropped` 是 0 次，表示這個掃描有作用；它的 find 照新寫法改了，性質不變。
- 證據：主 checkout `build/evidence/gyo2-kernel-summary-msg/impl/`（git 不追蹤，這台 macOS Intel 機器）。

## 突變（§14.5）

| id | 只有它抓得到的 | 守的 |
|---|---|---|
| `msg-summary-next-seq-back` | 摘要又收了寫入序號（`[msg-summary-seq]`） | kernel DI-KS 10；宣告第 9 節 |
| `msg-summary-record-seq-back` | 摘要又收了紀錄的序號；`[msg-summary-seq]` 比的 World 都沒有留著的紀錄，抓不到（`[msg-summary-record-seq]`） | 同上 |
| `msg-summary-cursors-back` | 摘要又收了游標表；只收回「讀到的位置」的改法也會被 `[msg-summary-cursors]` 抓到，所以 `[msg-summary-cursor-position]` 用「合わせ方」併進這一個 | 同上 |

- 退役三個：`msg-summary-no-cursors`、`msg-summary-no-next-seq`、`msg-summary-no-cursor-position`（守的性質拿掉了；`tests/common/mutation/README.md`〈加える基準〉第 7 項）。
- 改寫兩個（性質不變）：`msg-summary-version-not-bumped` 改指版本 4；`ks-summary-message-payload-dropped` 的 find 照新寫法。
- 回滾案和 Kernel 的案抓同樣的路，回滾那邊不另加（同第 1 項）。
- 加三個、退三個，未超過「變更的條文數 × 5」。

## 合併前的設計審查

〔待做：Fable／high，唯讀〕

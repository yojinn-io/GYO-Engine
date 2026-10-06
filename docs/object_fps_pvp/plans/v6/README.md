# PvP v6 分批計畫與進度

更新：2026-10-05。Owner：`object_fps_pvp`。
**第 01～03 批完成並合併**（PR #39、#40、#41；master `6381e9d`）。**第 04 批完成**（PR [#42](https://github.com/yojinn-io/GYO-Engine/pull/42)）；其餘批次都未開始；現行 wire 與玩法仍是 v5（[v5 穩定基線](../v5/STABLE_BASELINE.md)）。

v6 處理 v5 期間延後的項目（[交接](HANDOFF.md)），並在同一次協議升級中收下 Math 基礎統一的範圍外事項。
範圍分四群，使用者 2026-10-04 決定全部納入：

- A 玩法與呈現：遠端俯仰瞄準、受擊反應、本機冷卻閘、死亡後第一人稱手臂（缺陷）。
- B Engine 平台：視窗互動時的渲染阻塞、輸入層的完整按鍵與視窗事件。
- C 驗收工具：v4 語意模式清理、計時器基線、跨平台相容性。
- D Math 範圍外事項：Collision、Color、FNV-1a、Ui、include 風格、測試支援、產品登錄等。

先讀 [交接](HANDOFF.md)、[基線](BASELINE.md) 和指定批次。協議與資料語意只在 [v6 契約](../../protocol-v6.zh-Hant.md) 維護（目前是骨架，第 09 批定稿）。

Engine 與共通層的工作不屬於本產品，計畫與紀錄放在兩個 Engine 計畫夾（使用者決定 D1）。刪除本產品時，那些紀錄仍然完整：

- [輸入與呈現](../../../architecture/plans/input-and-present/README.md)：IP-1 輸入層、IP-2 呈現不阻塞。
- [基礎後續整理](../../../architecture/plans/foundation-followups/README.md)：FF-1～FF-9（Collision 語料與統一、共通測試計時、FNV-1a、有限性檢查、Ui、共通層衛生、include 風格、GYOP 標頭）。

## 進度

### 本產品的批次

| 批次 | 文件 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|---|
| 01 | [計畫、分批與基線](01-plan-and-baseline.md) | ultracode → medium → low | 完成，PR [#39](https://github.com/yojinn-io/GYO-Engine/pull/39) 已合併（`a687663`） | 分批計畫、HANDOFF 更正與決策紀錄、基線 CTest、v5 證據保存、契約骨架、兩個 Engine 計畫夾 |
| 02 | [驗收工具可信化](02-acceptance-tools.md) | high（02c 計時判定局部 xhigh） | 完成，PR [#40](https://github.com/yojinn-io/GYO-Engine/pull/40) 已合併（`a36b319`；②③' 兩處待產品配合） | 02a 刪除 v4 語意模式與版本常數、02b 計時器基線、02c 計時假設改為結構條件並重新分析 v5 資料 |
| 03 | [權威 digest 閘門](03-authority-digest.md) | high（golden 策略局部 xhigh） | 完成，PR [#41](https://github.com/yojinn-io/GYO-Engine/pull/41) 已合併（`6381e9d`） | 逐 Tick 權威狀態 digest、同機兩樹比對、CI 自洽檢查 |
| 04 | [量測基線 B0 與缺陷重現](04-measurement-baseline.md) | medium | 完成，PR [#42](https://github.com/yojinn-io/GYO-Engine/pull/42) 已合併（`30f87d5`；第 3 項重現，第 6 項未重現） | 新工具量 v5 產品；重現第 3、6 項（需要使用者在場）；無程式變更 |
| 05 | [死亡後第一人稱手臂](05-first-person-arms-after-death.md) | high | 不執行（第 04 批未重現；依停止條件） | 第 6 項的缺陷修正；無法重現就停下 |
| 06 | [改經 Engine 取得輸入](06-input-migration.md) | high | 完成，PR [#54](https://github.com/yojinn-io/GYO-Engine/pull/54) 已合併（`0e29d06`） | 產品與 GUI probe 改用 IP-1；文字輸入例外 |
| 07 | [量測基線 B1](07-measurement-baseline-b1.md) | medium | 完成，PR [#56](https://github.com/yojinn-io/GYO-Engine/pull/56) 已合併（`c1ee2b2`；B1 見 BASELINE；交錯 A/B 調查與 D21 見 HANDOFF） | IP-2 之後重取基線 |
| 07a | 驗收工具修正：連線時初始 seed 的夾住（見 [HANDOFF](HANDOFF.md) 的第 07a 批） | xhigh（主對話） | 完成，隨第 07 批的 PR | 豁免每位玩家一次的初始 seed 夾住；停頓重設仍判干擾 |
| 08 | [刪除未編譯 29 檔](08-remove-uncompiled.md) | medium | 完成，PR [#55](https://github.com/yojinn-io/GYO-Engine/pull/55) 已合併（`5b0553a`） | 29 檔與孤兒資產 |
| 09 | [協議 v6](09-protocol-v6.md) | high（ultracode 審查契約；版本閘與解碼局部 xhigh） | 進行中（分支 `claude/pvp-v6-batch09`；契約定稿、commit 1～4 完成，L1 通過；L2／L3 待執行） | **唯一的 wire 變更**：升 v6、受擊欄位（含攻擊者 id）、arena 內容 digest |
| 10 | [Collision 權威變更（產品端）](10-collision-authority.md) | high（ultracode 審查證據；容差、`IsValid` 套用後的 arena 規則與差異歸因局部 xhigh） | 未開始 | **唯一的權威變更**；與 FF-9 同一 PR |
| 11 | [遠端俯仰瞄準](11-remote-pitch-aim.md) | high | 未開始 | 第 1 項 |
| 12 | [本機射擊冷卻閘](12-local-fire-gate.md) | high（Tick 估計局部 xhigh） | 未開始 | 第 4 項 |
| 13 | [受擊反應與方向指示](13-hit-reaction.md) | high | 未開始 | 第 2 項的呈現 |
| 14 | [整合驗收與升格](14-integration-and-acceptance.md) | medium | 未開始 | 14a 整合短測與產品移除檢查；14b 完整驗收與升格（另外授權） |

### Engine 計畫的批次（狀態以各計畫夾為準）

| 批次 | 內容 | 建議檔位 | 本產品的關係 |
|---|---|---|---|
| IP-1 | 輸入層：完整 scancode、視窗互動事件、windowID 過濾 | high（介面以 ultracode 審查） | 第 06 批依賴 |
| IP-2 | 呈現不阻塞主迴圈（先拆分量測 fence 與 `nextDrawable`） | high（局部 xhigh） | 需要第 04 批的重現；第 07 批依賴 |
| FF-1 | Collision 判定語料（全部公開查詢） | high | 第 10 批依賴 |
| FF-2 | 共通測試的計時假設 | high／medium | IP-2 依賴 |
| FF-3 | FNV-1a 收為一份 | high | 第 09 批的 arena digest 使用 |
| FF-4 | 有限性檢查收斂（Color 保留兩型別） | high | — |
| FF-5 | Ui 與 ui_editor 的重複、`item_step` 驗證 | high | — |
| FF-6 | 共通層衛生與產品登錄 | medium | — |
| FF-7 | include 路徑風格統一 | high（建議 ultracode） | — |
| FF-8 | GYOP 標頭的 C++ 編解碼收進 Engine | high（解碼局部 xhigh） | 排在第 09 批之前 |
| FF-9 | Collision 統一與公開合法性檢查 | high（ultracode 審查；容差、`IsValid`、差異歸因局部 xhigh） | 與第 10 批同一 PR；須在第 09 批之後 |

```text
01 ─┬─ 02(a→b→c) ─┬─ 04 ─┬─ 05
    │             │      ├─ 06 ←── IP-1
    │             │      ├─ 11 ─┐
    │             │      └─ 12  │
    ├─ 03 ────────┤             │
    │             └──────────── 09 ←── FF-3、FF-8 ─┬─ 10（＝FF-9 同 PR）←── FF-1
    │                                              └─ 13 ←── 11
    ├─ 08
    └─ 04 ──→ IP-2（另需 FF-2）──→ 07
全部 ──→ 14a ──→〔另外授權〕14b
```

關鍵路徑：01→02→04→IP-2→07，以及 01→03→09→10。
平行軌道（03、08、IP-1、FF-1～FF-6、FF-8）由使用者分別啟動。

## 執行規則

- 每次只執行使用者指定的批次；一批一個 PR，commit 與 PR 用日語。依賴未完成時，不得以部分成果頂替。
- 每批開始、里程碑、停止時更新本表、[交接](HANDOFF.md) 與 dev_log，然後停止，不自動開始下一批。
- 主對話的檔位由使用者決定；表中是建議值。ultracode 和高於主對話的檔位，在批次開始時說明理由並徵求同意（使用者決定 D8）。xhigh 只用在批次內的局部。
- wire 只在第 09 批升一次。從第 09 批到升格之前，原則上不再改 wire；若必須改，需要使用者同意，並沿用三角色同一 PR、舊程序重啟的規則（D11①）。
- 權威結果只在第 10 批改變一次。其他批次都要以第 03 批的閘門證明權威不變：正式證明是同機兩樹 digest 比對。
- 量測基線分世代：B0＝第 04 批；IP-2 合併後由第 07 批重取 B1。之後各批在自己的 base commit 上，以凍結的工具量 before／after。v5 穩定基線的數字只用來對門檻。
- 先凍結來源、產物與分析器再量測；開發和乾淨量測不同時進行。失敗的跑次保留，先有限定位，再重驗受影響的項目，不用大量重跑代替分析。
- 長測與完整 GUI 三輪需要另外明確授權（第 14b 批）。
- 對玩家的門檻（50／66.7／80／100／150 ms、≥99%、恢復 1.5 秒、窗口與包率上限）跨平台相同，不因平台放寬。
- 實機驗收只在本機 macOS Intel／Metal；Windows、Linux 實機與 macOS arm64 實機標「未執行」（D11⑩）。每份批次文件都有平台表。
- 合併順序只是建議，用來減少衝突、方便歸因，不是依賴：後合併的一方 rebase。例如 `PvpApplication.cpp` 會被多批修改，建議順序是 05→06→12→11→13。真正的依賴只寫在進度表和依賴圖。
- 範圍、門檻或 ownership 需要改變時，先報告具體證據，不以猜測改政策或放寬驗收。

## 接續處理文字

> v6 第 01～04、06、08 批完成並合併，第 05 批不執行；Engine 的 FF-1～FF-8、IP-1、IP-2 已合併（IP-2 縮小交付）。分批與決定見本文件、HANDOFF 與 BASELINE。
> 第 07、07a 批完成並合併（#56），B1 見 BASELINE；30 FPS 列為設計範圍的邊界（D21）。第 09 批進行中；之後可以開始的是第 11、12 批。Engine 的現況：include 根目錄統一為 `engine/<m>/`；縮放拖動中以 live frame 持續更新，主執行緒／模擬／網路的分離延到 v7（D19）。FF-9 與第 10 批同一個 PR。版本號：vN＝遊戲版本，pvN＝協議版本（目前 pv5，第 09 批升 pv6，D20）。
> Windows／Linux 實機驗收與兩台機器的時鐘漂移實測需要另外授權。

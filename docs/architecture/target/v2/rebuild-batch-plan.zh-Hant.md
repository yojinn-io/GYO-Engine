# GYO2 批次表（2026-11-17 重排）

Owner：架構（GYO2 主會話）。使用者 2026-11-17：「你把過時的任務先重做，然後排給我。」

這一頁**取代**[時間設計](rebuild-time-design.zh-Hant.md)第 11 節的批次表（那一張建立在「Kernel 管組裝、限制在 Kernel」的前提上，已過時）。新的前提是 2026-11-16～17 在邊界卡定的：Kernel 只做 ECS、另立編排層、時鐘與 tick 在時間的 Module、權威與回滾是作者可選的策略式 Module、每層一份職責清單（HANDOFF `v8-arch-gyo2-kernel-is-ecs-no-restrictions`、`-orchestration-layer-name`、`-clock-out-of-kernel`、`-rollback-module-strategy`、`-layer-duty-lists` 等）。舊卡回收了什麼、哪些不帶進來：[recovered-from-stale-cards.zh-Hant.md](recovered-from-stale-cards.zh-Hant.md)。

每一批開工前都有事前宣告、經使用者核准；實作批的判定照 AGENTS §14。

---

## 第一段：設計（現在）

| 批 | 做什麼 | 狀態 | 依賴 |
|---|---|---|---|
| 邊界的統合 `v8-arch-gyo2-kernel-app-boundary` | 分層、職責清單、跨份的關聯表 L1～L21 | 邊界卡範圍的待判全部定完；等下面三份寫完，一起送換模型檢查，再請使用者核准 | — |
| 權威 Module 的宣告 `v8-arch-gyo2-authority-module` | 伺服器權威策略檢查什麼、策略 interface、代為登記、digest 比對與伺服器覆蓋、佔位策略 | 進行中 | 統合 |
| 回滾 Module 的宣告 `v8-arch-gyo2-rollback-module` | `RollbackStrategy` interface、第一版的狀態觸發、要 Kernel 給的原語清單 | 修訂 0 寫完（分支 `claude/gyo2-rollback-module-8bac02` b55af62e），已交統合；使用者要判的 7 件在第 10 節，等收口後問 | 統合 |
| 事件游標的宣告 `v8-arch-gyo2-event-cursor` | 讀者游標、上限與單位（不用 tick）、修剪、丟棄的記錄 | 修訂 0 寫完（分支 `claude/gyo2-event-cursor` facc0846），已交統合；使用者要判的 8 件在第 4 節，等收口後問 | 統合 |
| 讀檔的宣告 `v8-arch-gyo2-restore-entry` | 讀檔一律在新的 World、編號兩種寫法、hook | 進行中，等使用者回 5.3；對回滾、權威的要求等那兩份收口後對齊 | 回滾、權威 |

三份子宣告並行寫；寫完由統合那張卡一起送換模型檢查（`v8-arch-gyo2-boundary-split-integrator-first`）。

## 第二段：統合核准之後

| 批 | 做什麼（新前提下的範圍） | 取代 | 依賴 |
|---|---|---|---|
| Kernel 減法 | 照統合把疊加的限制移出 Kernel（寫者數量、區規則與 Z5、執行期攔截、時鐘與 tick 與 `Domain`、開局排程、歧義的偵測與嚴格模式 #228、D-E18）；`Product` 改名 `WorldExecutor`；登記與建 Plan 兩步、可重複；唯讀帶版本號的登記表；`ExclusiveWorld` 全讀寫、`Writes<…>` 選填；`KernelSettings` 每次最外層 `Run` 重讀。守那些限制的守衛與突變一起處理（K0 剛加的部分也在內） | 新的一批 | 統合 |
| 編排層的骨架 | `engine/orchestration`：`Product`、runner、build → 建 Plan → finish → 策略檢查、遊戲初始化清單、Module／Plugin 登記檔的讀取與依賴檢查、每個 World 准不准動態插入、多個 World | 舊 T2、T5 的一部分 | Kernel 減法（兩步的 API） |
| 時間 Module | 舊 T1 的型別與累加器（四件待使用者，見下）加上時鐘與 tick 移入（存放與回滾怎麼還原待定）、時間的 System、驅動固定步的獨占 System | 舊 T1、T3 | 純型別的部分可以先做；System 依賴編排層的 runner |
| 事件游標的實作 | 照它的宣告。**排在 Kernel 減法之前，或和它同一批**：減法會拿掉照時鐘的修剪，游標要先接上（統合第 8 節；`v8-arch-gyo2-event-trim-by-cursor` 的後果） | 舊 T4 | 事件游標的宣告 |
| 生成世界（重做） | 每個 World 各自、可以多次、可以分批；放編排層；Kernel 不認得階段（`-two-layers-like-bevy`）。新卡從回收清單第 2 節起 | 已歸檔的「重新評估 Loading」 | 統合、編排層的骨架 |
| 狀態機（重做成修訂 8） | 只改建立在舊前提上的地方：`RequestSilent` 還要不要、(c) 區改走權威策略、插進遊戲初始化清單、套用 System 的插法、以 D-E2／R-E2 為守衛的 DI；遊戲看得到的行為照舊（回收清單第 1 節） | 已歸檔的狀態機卡 | 統合、權威 Module 的宣告；實作依賴編排層、時間 Module |
| 權威 Module、回滾 Module 的實作 | 照各自的宣告 | — | 各自的宣告、Kernel 減法、編排層、時間 Module |
| 讀檔的實作 | 照它的宣告 | — | 讀檔的宣告、編排層 |

建議的先後：事件游標的實作（或和 Kernel 減法同一批）→ Kernel 減法 → 編排層的骨架 → 時間 Module → 其餘可以並行。（2026-11-17 照事件游標卡的回報改：原本寫游標依賴減法，和統合第 8 節對不上。）

## 不擋路、等使用者的小事

- **#236、#238**（測試舊帳）合併：先 #236、再 #238。
- **T1 的四件**：頻率用整數還是小數、主會話對設計的四處調整、核准前要不要再送檢查、核准整份宣告。
- **K0 入口檢查的先後**：Kernel 減法時可以一起定。
- **Module 的預設**：預設全帶可關，還是預設不帶要列（`-products-pick-modules` 裡沒有定的那一層）。
- **SDL 的 adapter、ufbx 歸哪一類**（收斂定義留下的未確認）。
- **五個試做的工作樹**要不要刪。
- **「誰實際寫的」除錯工具**：和 `Writes<…>` 選填、權威策略數寫者可能重疊，等權威 Module 的宣告再看。
- **v2 之前的 target 文件**統一處理（runtime-composition §4、架構 README 的兩行）；**LIM-7** 隨區規則移出要重看。

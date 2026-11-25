# GYO2 批次表（2026-11-17 重排）

Owner：架構（GYO2 主會話）。使用者 2026-11-17：「你把過時的任務先重做，然後排給我。」

這一頁**取代**[時間設計](rebuild-time-design.zh-Hant.md)第 11 節的批次表（那一張建立在「Kernel 管組裝、限制在 Kernel」的前提上，已過時）。新的前提是 2026-11-16～17 在邊界卡定的：Kernel 只做 ECS、另立編排層、時鐘與 tick 在時間的 Module、權威與回滾是作者可選的策略式 Module、每層一份職責清單（HANDOFF `v8-arch-gyo2-kernel-is-ecs-no-restrictions`、`-orchestration-layer-name`、`-clock-out-of-kernel`、`-rollback-module-strategy`、`-layer-duty-lists` 等）。舊卡回收了什麼、哪些不帶進來：[recovered-from-stale-cards.zh-Hant.md](recovered-from-stale-cards.zh-Hant.md)。

每一批開工前都有事前宣告、經使用者核准；實作批的判定照 AGENTS §14。

---

## 每批的設計審查（2026-11-22 起，每批固定的一步）

使用者 2026-11-22（`v8-arch-gyo2-design-review-stage`）：「而是要有一個code review的階段。目的不是找bug，而是看這個設計是否合理。」底層刻意不替誤用、亂用加守衛和突變（守衛過多會把設計鎖回舊形狀，master 就是這樣），少掉的那層由這一步補上。

- **時機**：實作與測試都通過之後、交使用者合併前核准之前。
- **誰做**：和實作者不同模型的審查者（Fable／high），唯讀。
- **看什麼**（設計合不合理，不是找 bug）：
  1. **機制有沒有分散在各個實現裡**：同一件事只該有一個公開出口，各實現注入自己的那一份；呼叫的一方只用公開介面，不用知道背後是哪個實現。例：os 只有一個對外介面，Linux、Windows、macOS 各自實作；外面完全不必管 OS，只呼叫介面就能用 OS 的能力。
  2. 責任與依賴方向；有沒有用契約以外的 API、繞過連結去 include 別的單位（守衛不擋的部分在這裡看）。
  3. 有沒有更簡單的做法、有沒有堆屎山；能拿掉的是否拿掉。
  4. 會變的規則有沒有被釘死在 Engine 裡；守衛是在守設計的本質，還是在擋誤用。
  5. 和別家成熟設計的取捨是否合理。**主參考**（使用者 2026-11-23「照建議」，`v8-arch-gyo2-hierarchy-bevy-and-primary-reference`；「主要我不想當縫合怪…衝突點太多。會導致膠水代碼爆炸。」）：ECS 相關的機制以 Bevy（含其生態：lightyear、bevy_replicon、avian、rapier）為主參考；偏離 Bevy 或改用別家做法時，宣告與審查要寫明碰到哪些衝突點、要寫多少膠水；GYO 自己的設計（編排層、權威策略、異常分級、Module／Plugin）寫清楚和 Bevy 的接合點；Unity、Godot、flecs 當對照與查證，不當混搭的來源。
- **產出**：審查報告放主 checkout 的 `build/evidence/<批次>/`；結論寫進合併前核准頁的一節；要使用者定的設計問題在核准時一起交。
- **先例**：第 4 批第 8 步的「整條線的審查」（找出 A3、A5）。

## 設計段（2026-11-17 完成）

| 批 | 做什麼 | 狀態 | 依賴 |
|---|---|---|---|
| 邊界的統合 `v8-arch-gyo2-kernel-app-boundary` | 分層、職責清單、跨份的關聯表 L1～L21 | **2026-11-17 使用者核准最終版**（c1be70eb，`v8-arch-gyo2-five-declarations-approved`） | — |
| 權威 Module 的宣告 `v8-arch-gyo2-authority-module` | 伺服器權威策略檢查什麼、策略 interface、代為登記、digest 比對與伺服器覆蓋、佔位策略 | **2026-11-17 使用者核准最終版**（7f9bbb85，`v8-arch-gyo2-five-declarations-approved`） | 統合 |
| 回滾 Module 的宣告 `v8-arch-gyo2-rollback-module` | `RollbackStrategy` interface、第一版的狀態觸發、要 Kernel 給的原語清單 | **2026-11-17 使用者核准最終版**（9418fcf1，`v8-arch-gyo2-five-declarations-approved`） | 統合 |
| 事件游標的宣告 `v8-arch-gyo2-event-cursor` | 讀者游標、上限與單位（不用 tick）、修剪、丟棄的記錄。2026-11-17 起範圍擴大成「事件」：保留式改名 `Message`，加推送式 `Event`＋`Observer` 一節（照統合第 15.1 節的判定；主會話排，同一張卡寫，因為兩種共用名字、觸發點與回滾的交界） | **2026-11-17 使用者核准最終版**（758f651f，`v8-arch-gyo2-five-declarations-approved`） | 統合 |
| 讀檔的宣告 `v8-arch-gyo2-restore-entry` | 讀檔一律在新的 World、編號兩種寫法、hook | **2026-11-17 使用者核准最終版**（ec31e863，`v8-arch-gyo2-five-declarations-approved`） | 回滾、權威 |

三份子宣告並行寫；寫完由統合那張卡一起送換模型檢查（`v8-arch-gyo2-boundary-split-integrator-first`）。

## 實作段（統合核准之後；現在）

用詞（2026-11-18）：整條線分「設計段／實作段」；每批的核准分「動工前核准／合併前核准」（舊紀錄寫的「第一段核准／第二段核准」即此）；一批之內的實作步驟叫「第 N 步」（宣告裡寫的「段 1～段 4」即此）。

2026-11-17 照統合修訂 16 第 8 節的提案重排（統合卡 13b640f4）：Kernel 減法拆成兩段。理由：一次拿掉全部的話，時鐘拿掉時事件還沒有游標（修剪靠時鐘），權威規則拿掉時權威 Module 還沒有（守衛空窗）。所以「拿掉某條限制」和「接手它的新家」放在同一條線，不需要新家的先做。這個先後要使用者核准統合時一起確認。

| 批 | 做什麼（新前提下的範圍） | 取代 | 依賴 |
|---|---|---|---|
| 1. Kernel 減法・結構段 | **2026-11-18 完成**（PR #242 併入 `gyo2`，0f1f2d21）。不需要新家的部分：`Product` 改名 `WorldExecutor`；登記與建 Plan 兩步、可重複；唯讀帶版本號的登記表；`ExclusiveWorld` 全讀寫、`Writes<…>` 選填；`KernelSettings` 每次最外層 `Run` 重讀、拿掉 D-E18；拿掉執行期攔截（L3）；拿掉歧義的偵測與 `RequireExplicitOrder`（#228）；拿掉執行緒的啟動檢查（改成 Resource 斷言）。守這些限制的守衛與突變一起處理（K0 剛加的部分也在內。2026-11-17 更正：`we-j3-spawn-immediate` **不退役**——它改的是一般 System 的 `Spawn`，一般 System 仍只經同步點；結構段卡偵察後判定，主會話先前寫「隨 `ExclusiveWorld` 可當場改成員退役」是讀錯） | 新的一批 | 統合 |
| 2. 編排層的骨架 | **2026-11-19 完成**（PR #244 併入 `gyo2`，f48af791；dev_log 收尾 PR #245，440d2f01）。合併前核准：使用者「核准，開 PR」，全部核准。D2（`-broken-world-ends-product`）在本批實作（宣告修訂 8、新 DI O-11）。以下是動工前的範圍（`v8-arch-gyo2-orchestration-skeleton-declaration-approved`；卡片分支 `claude/gyo2-orchestration-skeleton-67744c`）。帶上第 1 批的遺留（G-6 已補，f9f74684）：結構段 dev_log（`docs/dev_logs/2026_11_18_gyo2_kernel_subtract_structure.zh-Hant.md`）「驗證與經過」補 G-6 成立（PR #242 的 L1 三平台 linux-x64 5m12s、macos-arm64 4m33s、windows-x64 5m52s 通過，CI gate 通過，run 44243311890；Windows／Linux 第一次跑這批），開頭改成已合併（PR #242，0f1f2d21，2026-11-18）——使用者在結構段卡定「交給主會話，讓他加到下一批裡」。`engine/orchestration`：`Product`、runner、build → 建 Plan → finish → 策略檢查、遊戲初始化清單、Module／Plugin 登記檔的讀取與依賴檢查、每個 World 准不准動態插入。**同一批拿掉 Kernel 的開局排程**，J-58 改寫後搬到編排層。讀檔卡的要求（修訂 10）：建 World 後、第一次 Run 前給作者一個時機（讀檔要：設配發器範圍的下界、放暫存的存檔資料與 Resource 的預設值；實體由生成世界裡的 System 放回；統合修訂 33 已收，形狀在本批宣告定；Kernel 那一半屬 DI-KS 15 第 (7) 項「生成」）；判定要有讀檔的案例。另兩條（讀檔卡修訂 11）：執行中丟掉現在的 World、建新的（遊戲中讀檔靠它）；遊戲初始化跑在第一個 World，丟掉第一個 World 後那些內容怎麼處理，以及 `new-world-default` 原話的「經開局」對新 World 是什麼意思 | 舊 T2、T5 的一部分 | 1（兩步的 API） |
| 3. 時間 Module＋事件（同一條線） | **2026-11-20 完成**（PR #253 併入 `gyo2`，59fd4da9；合併前核准 `v8-arch-gyo2-time-events-merge-approved`）。以下是動工前的範圍：**2026-11-19 事前宣告動工前核准**（`v8-arch-gyo2-time-events-declaration-approved`；宣告 `time-events-declaration.zh-Hant.md`，卡片分支 `claude/gyo2-time-events-8fe1bf`；遊戲時間全部在 `engine/world_time`，`-game-time-in-world-time`）。舊 T1 的型別與累加器、時鐘與 tick 移入、時間的 System、驅動固定步的獨占 System（讀檔卡的要求：時間的 Resource 能被讀檔放回；「重跑中」的 Resource 由時間 Module 收，不收時退回回滾 Module）；事件照 `v8-arch-gyo2-events-two-kinds-like-bevy` 分兩種：保留式改名 `Message` 並照游標的宣告修剪，推送式 `Event`＋`Observer`（含生命週期觸發）照統合第 15.1 節；不另做 EventBus，Engine 機制（時間 tick、狀態機切換、World 壞掉、存讀檔完成等）經 Kernel 的 Event／Observer 發，宣告逐個對照 Bevy、flecs 定送法（`v8-arch-gyo2-no-separate-eventbus`）；**同一條線拿掉 Kernel 的時鐘、`Domain` 與 `EventTrim`**；**改第 2 批已合併的編排層：預設的 schedule 骨架**（每幀 `Main`、固定步 `FixedUpdate`，預設帶、作者可換或關；時間 Module 只往上掛，`v8-arch-gyo2-orchestration-gives-schedule-skeleton`） | 舊 T1、T3、T4 | 2；事件游標的宣告。純型別的部分可以先做 |
| 4. 權威 Module＋回滾 Module（同一條線） | **2026-11-22 完成**（PR #259 併入 `gyo2`，bb5d010d；`v8-arch-gyo2-authority-rollback-merged`）。以下是進行中的紀錄：**2026-11-20 開始；2026-11-21 使用者說開始實作（`v8-arch-gyo2-authority-rollback-start`），更新到 `gyo2` f25837b0（卡片誤用 rebase＋強推，事故 `v8-arch-gyo2-authority-rollback-force-push-incident`）；第 1 步完成、draft PR #259（L1 綠）；A11 照建議（`-a11`），進行第 2 步**（批次代號 `v8-arch-gyo2-authority-rollback`；宣告 `authority-rollback-declaration.zh-Hant.md`，卡片分支 `claude/gyo2-authority-rollback-*`）。照各自的宣告；**另收 `World::Summary` 改成逐型別的雜湊函式**（和 digest 方向一致；解決原始位元組雜湊的 padding、指標／heap 漏算、+0／-0 三種毛病；在那之前 restore case 5 與其他比摘要的測試標「未完全解決」，`v8-arch-gyo2-summary-padding-a-then-c`）；**同一條線拿掉 Kernel 的權威規則**（寫者數量、digest、區規則與 Z5、回滾的宣告與 Rewind／Replay）。守它們的突變一起處理：Z5、寫者數量、區規則相關的 `we-*`、`td-*` 裡的 W-1 期待、`rs-j17-zone-b-listed`（測試舊帳卡交接） | — | 各自的宣告、2、3 |
| 5. 生成世界（重做） | **2026-11-22 完成**（PR #272 併入 `gyo2`，879350d4；`v8-arch-gyo2-world-gen-merged`）。**2026-11-22 宣告動工前核准、同日實作開始**（`v8-arch-gyo2-world-gen-declaration-approved`、`-world-gen-start`）；**2026-11-22 開宣告卡**（`v8-arch-gyo2-batch5-start`，`local_df401d25`）。每個 World 各自、可以多次、可以分批；放編排層；Kernel 不認得階段（`-two-layers-like-bevy`）。另收**預設的實體配發器**（`v8-arch-gyo2-default-allotter-module`，主會話照 L17 判）：由一個預設帶、產品可關或替換的 Module 在每個 World 登記範圍配發器並放好預設範圍，作者不宣告也能 `Spawn`；配發策略照舊是產品可換的 Resource，Kernel 不內建預設策略、職責清單不變；不和 Base 的 `IdIssuer` 共用。新卡從回收清單第 2 節起。讀檔卡的要求：讀檔 System 掛在生成世界的方式，判定要有讀檔的案例 | 已歸檔的「重新評估 Loading」 | 2 |
| 5. 多個 World、交換點 | **2026-11-22 完成**（PR #270 併入 `gyo2`，eacab4d7；`v8-arch-gyo2-exchange-merged`）。**2026-11-22 宣告動工前核准、同日實作開始**（`v8-arch-gyo2-exchange-declaration-approved`、`-exchange-impl-start`；PR #270）；**2026-11-22 開宣告卡**（`v8-arch-gyo2-batch5-start`，`local_7876c9c8`；連同 Product 層 Resource、第 5 批開頭的兩條守衛）。照統合 | — | 2 |
| 5. 狀態機（重做成修訂 8） | **2026-11-23 完成**（PR #281 併入 `gyo2`，1df88fe7）；**2026-11-23 合併前核准**（PR #281，`v8-arch-gyo2-state-machine-merge-approved`；合併等「合併」）；**2026-11-22 修訂 8 動工前核准、實作開始**（`v8-arch-gyo2-state-machine-r8-approved`）；**2026-11-22 開卡**（`v8-arch-gyo2-next-batches-start`，`local_6b397224`；先重做宣告）。只改建立在舊前提上的地方：`RequestSilent` 還要不要、(c) 區改走權威策略、插進遊戲初始化清單、套用 System 的插法、以 D-E2／R-E2 為守衛的 DI；遊戲看得到的行為照舊（回收清單第 1 節）。(b) 區的部分照統合 | 已歸檔的狀態機卡 | 權威 Module 的宣告；實作依賴 2、3（(c) 區依賴 4） |
| 5. 讀檔的實作 | **2026-11-23 完成**（PR #276 併入 `gyo2`，eb08ffce；含存檔版本純程式庫 `save_version`）；**2026-11-22 合併前核准**（PR #276，`v8-arch-gyo2-restore-impl-merge-approved`；合併等「合併」）；**2026-11-22 修訂 12（含存檔版本）動工前核准**（`v8-arch-gyo2-restore-impl-r12-approved`）；**2026-11-22 開卡**（`v8-arch-gyo2-next-batches-start`，`local_24fe0b56`；先對照宣告與現況）。照它的宣告 | — | 讀檔的宣告、2 |
| 5. 存檔版本 Module | **2026-11-22 改成純程式庫 `save_version`**（`v8-arch-gyo2-save-version-library`；併入讀檔的實作，PR #276）；**2026-11-22 併入「讀檔的實作」**（`v8-arch-gyo2-save-version-into-restore`；同一份宣告一節、同一個 PR）。2026-11-18 使用者：「需要提供version module，默認不啟用，啟用之後過時檔會直接拒絕。這個補充不放在編排裡，但是得放計劃中。Engine需要支持。」（`v8-arch-gyo2-save-version-module`）。Engine 提供的 Module，**預設不啟用**（使用者明說的例外，不同於「Module 預設全帶」）；啟用後比對存檔帶的版本標記與作者宣告的最低支援版本，過時就拒絕。主會話的讀法（開宣告時確認）：只比對版本標記、不讀存檔內容（和「Engine 不認得存檔格式」不衝突）；拒絕走讀檔失敗的路（`Result` 回錯誤碼，預設先建新的時舊 World 照跑）。排在讀檔的實作同一條線或緊接其後（主會話照 L17 排） | — | 讀檔的實作、2 |
| 6. JSON Module（最先） | **2026-11-20 完成**（PR #252 併入 `gyo2`，8ec005f2；合併前核准 `v8-arch-gyo2-json-module-merge-approved`）。動工前核准（`v8-arch-gyo2-json-module-declaration-approved`；卡片分支 `claude/gyo2-json-module-9cb753`）。封裝的實作，早於日誌與 trace（`v8-arch-gyo2-json-early`）；走 yyjson＋自己的介面，只依賴 Base，可以和第 3 批並行 | master 用 nlohmann（不帶） | Base |
| 6. 網路 Module | **2026-11-23 PR-2 已合併**（PR #290 併入 `gyo2`，04dec9a8；第 4、5 步：K3、相關性、頻寬判定）；**2026-11-23 PR-1 已合併**（PR #275 併入 `gyo2`，494f1d95；使用者「你把需要合併的，按順序合併掉。」）；**2026-11-23 PR-1 合併前核准**（PR #275，`v8-arch-gyo2-net-pr1-merge-approved`；使用者選先等 os 逾時調查卡 #289 的結論再合，`-net-pr1-merge-wait-os-timeout`）；**2026-11-22 宣告動工前核准**（`v8-arch-gyo2-net-module-declaration-approved`，9e3f1cb4）；**同日實作開始**（`v8-arch-gyo2-net-module-start`，Q4 維持）；**2026-11-22 開卡**（`v8-arch-gyo2-next-batches-start`，`local_a19de0ec`；先寫事前宣告）。網路是機制、在搬產品之前做（`v8-arch-gyo2-net-is-mechanism-first`）。照點名的功能逐一推 wire 需求、對照 lightyear、bevy_replicon、Unity Netcode；Gateway 原意（外部多種 Client、內部一套抽象協議、無狀態）；斷線偵測與重連只在 UDP 上 | master 的 `engine/net`、`services/gyo_gateway`（參考） | 3、4；多個 World 與交換點 |
| 6. Kernel：Resource 的變更偵測（`Res::Changed()`） | **2026-11-23 完成**（PR #284 併入 `gyo2`，7cf0861e）；**2026-11-23 合併前核准**（PR #284，`v8-arch-gyo2-kernel-res-changed-merge-approved`；合併等「合併」）；**2026-11-23 宣告核准、實作開始**（`v8-arch-gyo2-kernel-res-changed-approved`；3.1 (a)、CCR-8）；**2026-11-22 開卡**（`v8-arch-gyo2-kernel-res-changed-start`，`local_12fc6672`）。`Res<T>`／`ResMut<T>::IsChanged()`，判定照元件 `Changed<T>`；run 條件不做；先寫小宣告 | — | 網路的 Resource 複製（Q8 (b)）等它 |
| 6. 日誌機制 | **2026-11-21 完成**（日誌 PR #256 併入 `gyo2`，d3371355；`v8-arch-gyo2-logging-merged`）。trace 另成 Module、另寫宣告與 PR（`v8-arch-gyo2-logging-trace-two-modules`、`-trace-direction`），使用者「合併完先等等。」後，2026-11-21「通知日誌卡，可以繼續。」，從 bb2497b0 寫 trace 宣告；同日動工前核准（`v8-arch-gyo2-trace-declaration-approved`）；同日合併 PR #258（f25837b0，`v8-arch-gyo2-trace-merged`），**第 6 組完成**。 | master 的 `engine/trace`（參考） | 2、3 |
| 6. 異常分級 | **2026-11-21 完成**（PR #254 併入 `gyo2`，e0806f1f；`v8-arch-gyo2-error-levels-merged`）。範圍收窄到 D1、D2、P12、P15、退出流程與條文草稿（`v8-arch-gyo2-error-levels-shrink`、`-exit-flow`）。 | `docs/architecture/error-handling.md`（R0–R6 的契約，起點） | 2、3 |
| 6. Engine 分層 | **2026-11-21 完成**（PR #257 併入 `gyo2`，bb2497b0；`v8-arch-gyo2-engine-layers-merged`）。後續：core、ecs、orchestration 不登記成 Module（PR #261，2acf9cee；`v8-arch-gyo2-core-not-registered`）。`engine/` 分成 `core/`、`ecs/`、`orchestration/`、`modules/`，登記檔加 `dir` 欄並搬到頂層 `config/`；主會話親自做 | — | 已合併的各批 |
| 6. SDL（第一個 Plugin 批） | **2026-11-23 宣告整份動工前核准**（修訂 21，26e37273，`v8-arch-gyo2-sdl-declaration-approved`；14 個子批 R1a、R1b、KR、X1、X2、I、P、W、A、T、R2a、K、R2b、U，各子批另寫事前宣告；KR 已合併 #295 d02e60f4、X1 已合併 #298 077ae2af、T 已合併 #297 ecbb0bda、R1a 已合併 #299、P 已合併 #300；I 已合併 #301 743d7fd2、X2 已合併 #303 eff6408b、W 已合併 #308 6df0937b、A 已合併 #306 426add81、R2a 已合併 #315 906b1c5e、R2b 已合併 #319 83a5ebb0（2D）＋#320 3dc2d5df（3D）、U 已合併 #322 2a28d9b6＋#326 bec63ca5；下一步 K）；**2026-11-23 開宣告卡**（`v8-arch-gyo2-sdl-declaration-start`，`local_2cc7e494`）。`engine/platform` 不回來，功能拆進各 Module，怎麼拆在這批分析（`v8-arch-gyo2-platform-obsolete-split-into-modules`）；plugins 目錄的建置；input、render、text、ui 依序回來；碰外部組件的程式一律放各 Module 的 `src/adapters/<組件>/`（SDL 放 `src/adapters/sdl/`）（`engine/core/os` 不搬：core 用 `src/<平台>/`，`src/adapters/<組件>/` 是 Module 的結構，`v8-arch-gyo2-os-in-core-platform-dirs`；SDL 帶的視窗、輸入、聲音裝置不進 core/os）；**render 回來時一起做 11-11 的 K.3-6**：模擬的 World 與渲染的 World 分開，渲染的有自己的 Registry，只經 Extract 唯讀拿資料（Kernel 減法時 Extract 已拿掉，gyo2 上沒有；2026-11-21 補列，`v8-arch-gyo2-plan-gaps-2026-11-21`） | master 的 platform、input、render、text、ui（參考） | 2、3；多個 World、交換點 |
| 物理（2D、3D 兩個 Module） | **3D 已合併**（2026-11-24：#309 起步、#311 角色控制器、#312 接縫修正，`v8-arch-gyo2-physics-3d-merged`）；2D 跟 Object_Connect（盤點沒列物理缺口）。2026-11-23 使用者定 2D、3D 分成兩個 Module（`v8-arch-gyo2-2d-and-3d`）；master 的 collision 只有 3D | — | 各自跟點名的產品回來：3D 跟 PvP、2D 跟使用者要移植的 2D 遊戲 |
| 6. core/os 等待原語＋core/time 改呼叫 os | **2026-11-21 開卡；同日合併前核准（`v8-arch-gyo2-os-wait-merge-approved`，PR #262）**（`v8-arch-gyo2-os-wait-unfreeze`；卡片 `local_16721d66`）。os 是底層兼容層、只對外提供介面；`core/time/src/Waiter.cpp` 不再直接碰 OS API。先查 10-28 W3 不通過的原因並寫事前宣告（Windows 實機計時門檻），核准後做 core/os（`src/<平台>/`）、再讓 time 呼叫 os、刪准許清單 | 10-28 撤回的 os-wait-backend（`4041cf42`、`756073aa`） | — |
| 6. namespace 根的改名（機械式） | **2026-11-22 完成**（PR #266 併入 `gyo2`，45f13757；`v8-arch-gyo2-namespace-root-merged`）；**開卡**（`v8-arch-gyo2-namespace-root-start`，卡片 `local_36acb56b`；改名＋立守衛）。11-14 命名規範第 6 節已定（`v8-arch-gyo2-naming-1-to-4`）：帶過來的 base、math、os、time、threads、io、audio、entity_store 的根從 `Engine::<X>` 統一成 N17 的 `GYO::Engine::<Module>`（含 `Engine::IO` 的大小寫），只改名字、不改行為，以全部測試通過為準；層名不進 namespace（N17 的例子 `engine/ecs/kernel`↔`GYO::Kernel`）。原定「System 契約確認之後、移植任何產品之前」，之前沒排進批次表（2026-11-21 補列，`v8-arch-gyo2-namespace-rename-placement`）。位置：第 4 批合併之後、threads／io／audio 重審之前，單獨一個 PR（先改名，重審時不必邊改名邊改介面） | — | 4 |
| 6. threads、io 重審 | **2026-11-23 完成**（PR #277 併入 `gyo2`，0f3be2cc；`v8-arch-gyo2-threads-io-review-merged`）；**2026-11-22 修訂 1 動工前核准、實作開始**（`v8-arch-gyo2-threads-io-review-approved`；Q1 `WorkerPool`、Q2 拿掉 Vfs 一族）；**2026-11-22 開卡**（`v8-arch-gyo2-threads-io-audio-review-start`，`local_a184932c`）。R0（`v8-arch-rebuild-r0-approved`）和 audio 一起帶過來、標「帶過去並重審」，之後沒排進批次表（2026-11-21 補列，`v8-arch-gyo2-plan-gaps-2026-11-21`）。重審點照 R0 清單附錄：threads 的 `RoleThread` 系譜是 PvP 的 sim 角色（TT-1），②③是 runner 的執行緒、要和編排層的 runner 與交換點對齊；io 的系譜是舊引擎＋os 線的 io-contracts，工具存讀檔、遊戲資料、存檔都用，要和讀檔、存檔版本 Module、asset 的需求對齊。和 audio 重審排在一起 | master 的 `engine/threads`、`engine/io`（參考） | 2；多個 World、交換點；讀檔 |
| 6. core/os 使用者資料目錄 | **2026-11-23 完成**（PR #280 併入 `gyo2`，d83d0057）；**2026-11-22 宣告動工前核准、實作開始**（`v8-arch-gyo2-os-user-data-approved`；做成 `OS::Files::UserDataDirectory(organization, application)`，和 `FontDirectories` 同處；`OS::Paths` 原是舉例）。2026-11-22 使用者定補在 os 層、另開小批（`v8-arch-gyo2-threads-io-review-approved` Q3）。給「這個使用者的資料目錄」：macOS Application Support、Windows `%APPDATA%`、Linux `$XDG_DATA_HOME`；一個公開介面、三個平台在 `src/sys/<平台>` 注入。Feature：玩家存檔（讀檔卡 19.6 指路過來）。對照 Godot `user://`、Unity `persistentDataPath`、Unreal `ProjectSavedDir`、SDL `SDL_GetPrefPath` | — | 和網路的 Q5（`core/os` 收 UDP socket）都改 os.md，後合併的一方 merge 跟上 |
| 6. audio 重審 | **2026-11-23 完成**（PR #279 併入 `gyo2`，3497d708）；**2026-11-23 合併前核准**（PR #279，`v8-arch-gyo2-audio-review-merge-approved`；合併等「合併」）；**2026-11-22 宣告修訂 2 動工前核准**（`v8-arch-gyo2-audio-review-approved`；實作等「開始」）；**2026-11-22 開卡**（`v8-arch-gyo2-threads-io-audio-review-start`，`local_2e536b3b`）。R0（`v8-arch-rebuild-r0-approved`）帶過來時標「帶過去並重審」，之後一直沒排進批次表（2026-11-21 主會話回報整體進度時漏列，使用者問「還有audio呢？」才補上）。已知的重審點：(1) `PlayRequest::at` 用 Engine 的真實時間（`Mixer.hpp`），World 的暫停、慢動作會碰到——rebuild-concept 第 2 節判「不相合，要處理」；(2) 提交介面歸第一個呼叫它的執行緒所有（Submitter 的執行緒契約），要和交換點模型比過（R0 清單附錄）；(3) Engine 只提供機制、會被替換的規則外移的方向（11-13）是否適用；(4) SDL 的輸出後端 `GYO::AudioBackendSDL` 沒有帶過來，歸 SDL 那批。現況：`engine/modules/audio`（Mixer、ClipBank、VoiceMixer）與測試照 master 帶過來，只隨 #257 搬過目錄。 | master 的 `engine/audio`（參考） | 3（`world_time`）；多個 World、交換點 |
| 6. audio 那一側的小批（clip 型別與音效 loader） | **2026-11-23 完成**（PR #288 併入 `gyo2`，da01276b；使用者「你把需要合併的，按順序合併掉。」）；實作完成、PR #288（合併前核准四件已過，`v8-arch-gyo2-audio-clip-loader-merge-approved`）；**2026-11-23 開卡**（`v8-arch-gyo2-audio-clip-and-kernel-summary-start`，`local_94ab993d`）。**2026-11-22 取代原 world_audio 一批**（`v8-arch-gyo2-world-audio-withdrawn`）。範圍：(3) clip 的來源——`ClipBank` 該不該存在、共用或複製（audio 重審宣告第 5 節 A～D；(D) 借 asset 記憶體記成 RC）；(7) 音效 loader 與 clip 的 Data Contract（型別名 `"sound"`、格式與 Version、驗證沿用 `ClipValidation`；工具 audio_editor 與遊戲共用；`GYO::Audio` 連 asset 會改 DI 2，或另開小函式庫，在那批定）。不設 World 層的 Module：World 接混音器的寫法是作者的，寫在 `audio.zh-Hant.md`（#279）。另順手：audio 頁 §5.2 引用 O-2 新補的半句（`v8-arch-gyo2-audio-o2-cite-deferred`）。預設起播時刻是作者的（說明頁）；輸入事件的到達時刻歸 SDL 那批 | — | asset 實作；排在 asset 實作合併後，或併進 SDL 那批（那時才有真的 `Render` 可驗） |
| 6. asset | **2026-11-23 完成**（PR #283 併入 `gyo2`，1d4a8d7a）；**2026-11-23 合併前核准**（PR #283，`v8-arch-gyo2-asset-merge-approved`；合併等「合併」；adapter 放 world_asset，`v8-arch-gyo2-asset-adapter-world-asset`）；**2026-11-22 實作開始**（使用者「asset宣告任務卡開做」，`v8-arch-gyo2-next-batches-start`；宣告卡 `local_d37017a1` 接著做）。**2026-11-21 開宣告卡；同日使用者定：資產管理放在 World 外、World 只持 id（推翻 11-11 方向 2，`v8-arch-gyo2-asset-outside-world`）、作者只用 id（`-author-uses-id`，catalog owner 未決項解消）、大地圖照 Bevy 日後用 Module 加；第 4 題到了就可見＋就緒查詢、第 2 題原始位元組雜湊當能力、依賴由 Engine 推出（修訂 3，fd109e1a）**（`v8-arch-gyo2-asset-three-decisions`；卡片 `local_d37017a1`，分支 `claude/gyo2-asset-*`；只寫宣告）。歸 Engine 的 Module（`engine/modules/asset`）；核心不認得格式，loader 歸擁有型別的 Module（第三方放該 Module 的 `src/adapters/`）；圖片解碼用 stb_image、不走 SDL；DI 照新前提重寫（舊 `target/assets.zh-Hant.md` 的 DI 2、3 用 (c) 區與 Ingress stage）；起步執行期讀原始檔、loader 介面接得上日後工具事先轉檔的路（AGENTS §8）。資源目錄、載入、生命週期 | master 的 `engine/asset`（參考）；`target/assets.zh-Hant.md`（11-11 方向、11-13 DI） | 宣告：現在；實作：2、JSON、**多個 World、交換點**（舊 DI 2 的 IO 執行緒＋交換點；2026-11-21 更正，原寫「2；JSON」） |
| 6. 之後的小事（JSON Module 卡交來） | Kernel 指紋報告手寫 JSON 改用 JSON Module（要動 Kernel 的依賴，另議）；master 七份資料契約的驗證規則收緊，在各契約回到 `gyo2` 的批寫明；打包時附第三方授權告示（yyjson、fast_float 起） | JSON Module 宣告第 15 節 | JSON |

**產品與資產的頂層目錄**（2026-11-23 使用者定，`v8-arch-gyo2-top-dirs-games-assets-tools`）：`games/<遊戲>/`、`assets/<產品>/`、`tools/<工具>/`；Object_Connect 的 `games/object_connect/`、`assets/object_connect/` 與頂層 `tools/` 已用 `.gitkeep` 先建（`v8-arch-gyo2-object-connect-dirs-now`）。

先後：1 → 2 → 3 → 4 → 5（5 的各批可以並行）→ 6 的基礎模組（JSON 最先、可以現在就和 3 並行；網路 Module 在 4 之後即可和 5 並行；日誌與 trace 在 JSON 之後）→ 搬產品（`v8-arch-gyo2-infrastructure-before-products`）。2026-11-18 使用者定：第 2 批的宣告卡等第 1 批收尾、併進 `gyo2` 才開，不並行（`v8-arch-gyo2-batch2-after-batch1-merged`：「這個底層設計，還是穩一點好。而且有些決策還不穩，之前的很多批都是這樣廢掉的。」）。（舊版寫「事件游標依賴減法」，後來改成「游標排在減法之前」，現在照統合併進第 3 條線。）

## 後續小事（卡片交來、還沒排）

2026-11-21 補列（`v8-arch-gyo2-plan-gaps-2026-11-21`）。原本只在 HANDOFF 或主會話手上。

| 項目 | 來源 | 什麼時候做 |
|---|---|---|
| 9 個本來就壞的突變退役或重寫（`ks-k6-calls-off`、`ks-k6-every-file-exempt`、`ks-k8-skips-executor-files` 找的 `executor_file` 已在 #254 拿掉；`os-g8-link`、`os-g13`、`os-g14`、`ti-x1`、`ti-x2`、`th-x1` 的 find 在基準就已過時） | Engine 分層（#257）的 dev_log | **已完成**：Kernel 3 個由 #265 處理（2 退役、`ks-k6-calls-off` 改寫）、os 6 個由 #260 改寫；機械清理卡（`v8-arch-gyo2-mech-cleanup`）實跑確認 |
| 日誌測試把 `logging-test-files/` 寫進工作目錄，改寫到建置目錄 | Engine 分層時誤 commit 98 MB | **已完成**：trace 批 db83c6ef（`GYO_LOGGING_TEST_BINARY_DIR`）；機械清理卡（`v8-arch-gyo2-mech-cleanup`）從 repo 根跑測試後確認根目錄沒有殘留 |
| Windows 上非 Install 的執行緒走到 `std::terminate` 時沒有致命紀錄（日誌 S1） | 日誌批合併前核准頁 | 另開 |
| Parallel 下 trace 段的父子、段在別的執行緒結束（RC-T1） | trace 批合併前核准頁 | Kernel 解除 LIM-2 時重看 |
| `bs-` 開頭的突變搬到 Base 的清單 | 突變清點卡（#247） | 下一次動 Base 的批 |
| 突變數量的增長判準交風紀委員 | 突變清點卡（#247） | 風紀委員待辦 |
| 每步除了相關突變，再跑一次只數 find 次數的靜態掃描（第 4 批漏 8 個 stale） | 第 4 批後續卡（#265） | 風紀委員待辦 |
| 第 5 批開頭的守衛：Kernel「拿掉的名字不回來」、權威 Module 的 `gyo_expect_links`（不必再加 DEFER 突變） | 第 4 批宣告 9.5、#265 | 第 5 批開頭 |
| entity_store、trace、logging 三個 include 守衛的 `..` 路徑：照 O4 先例補 | #265 設計審查 | 2026-11-22 開卡（`v8-arch-gyo2-stale-cleanup-start`） |
| kernel_guard K6 的標頭分支（守的是寫法）值不值得留 | #265 設計審查 | 下一次設計審查一起看 |
| world_logging 給一個接 `WorldSetup` 的登記函式（登記呼叫 `TakeKernelDiagnostics` 的交換函式「出」），加走真的 `TakeKernelDiagnostics` 的測試；**同時把 O-3 與 O-12 對診斷紀錄的例子改成一致**（Design Invariant 條文，改前給使用者看） | 交換點實作 PR #270 的設計審查；2026-11-23 代碼審計後使用者選 (a) 留交換函式、接上（`v8-arch-gyo2-forwarding-audit-exchange-keep`） | **2026-11-23 完成**（任務卡，`v8-arch-gyo2-kernel-diagnostics-exchange`：`WorldLogging::ForwardKernelDiagnostics`、兩個測試、O-3／O-11／O-12／LG-1；[宣告](kernel-diagnostics-exchange-declaration.zh-Hant.md)、dev_log `2026_11_23_gyo2_kernel_diagnostics_exchange`） |
| `LogWorldLabel<D>`：System 拿不到 World 名，每個描述器各抄一份 Resource（`LogWorldLabel.hpp:10-21`、`Forward.hpp:147-155`）；Bevy 的 `WorldId` 是 SystemParam（`bevy_ecs/src/world/identifier.rs:65`）。只有 Kernel 能補 | 2026-11-23 代碼審計（Fable 檢查 N2） | 另開小批，不併進 KR（KR 卡建議、主會話判，2026-11-23） |
| 「原始碼在哪些頂層目錄」手寫在 6 處（`GyoBuild.cmake` 的 `add_subdirectory`、`GyoCodePage.cmake:27`、`os_boundary_guard.py:31-32`、`tests/common/os/CMakeLists.txt:158`、`naming_guard.py:63`、`test_workflow_gates.py:1162`），AGENTS §9「同一事實一處」；收成一處時要保留「各守衛把清單釘死」 | apps→games 宣告 §6 第 1 項 | apps→games 合併後另開一批 |
| `plugins/` 不在 code page 的根（`GyoCodePage.cmake:27`），也不在 os 守衛的根 | apps→games 宣告 §6 第 2 項 | 已轉 SDL 宣告卡：第一個 Plugin 進來前補 |
| `examples/` 有執行檔，卻不在 DI 11（`ProcessCodePageCheck`）的 manifest 根裡（現存缺口） | apps→games 宣告修訂 1 | 和「頂層目錄手寫 6 處收成一處」那批一起 |
| asset 在建置側的鏡像（`content_contract.py`）和 C++ 契約不一致（例：`shader_bundles` 一邊必填、一邊可省） | apps→games 宣告修訂 1 | owner asset；搬產品、打包回來前對齊 |
| os.md :318-319 寫「每個存在的根至少要有一個原始碼」，`os_boundary_guard` 對空的產品根是跳過，兩者不一致 | apps→games 宣告修訂 2 | 和「頂層目錄手寫 6 處收成一處」那批一起 |
| `examples/orchestration/product_main.cpp` 不示範取壞掉 World 的診斷紀錄；world_logging 做成登記單位 | 診斷紀錄接線卡 #292 | 有產品需要時 |
| O-13 的字面沒有涵蓋同一條執行緒上的第二個 World（例：同執行緒的兩個 World 都要輸入） | SDL 宣告修訂 19 第 16 節第 3 件 | SDL 子批 W 的宣告一起提條文 |
| `CaseTiming.cpp` 的 listener 與沒有呼叫者的 `Note()`：照 #289 自己寫的條件拿掉 | os 退避卡 PR #293 | 下一次動 os 測試的批 |
| 「loses no signal」的掃描：遺失只落在 `round % 64` 的頭 0～3 格，縮小掃描範圍檢出力可能提高好幾倍（宣告 Q3） | os 退避卡 PR #293 | 要研究時另開一批（使用者未定） |
| os Invariant 8 寫「cleared under its mutex」，但把後備實作的 `pending = false` 移到鎖外任何案例都抓不到（只剩 data race） | os 退避卡的驗證者 | 交設計審查，不加突變（`no-guards-against-misuse`） |
| os.md DI 2 字面仍寫「the host shader tool」豁免，gyo2 的程式已拿掉（`test_os_boundary_guard.py` 的 `test_render_shader_tool_is_runtime_code_for_r4`） | SDL 宣告修訂 20 第 14 節 | **R2a #315 一起改**（主會話 2026-11-24 判：shader 工具在 `plugins/sdl_gpu` 照 R4，例外已無對象） |
| 建置層內「遊戲目錄」的規則手寫 4 處；KIND 集合有 3 個讀者；4 個只寫不讀的值 | apps→games #294 設計審查 | 和「頂層目錄手寫 6 處收成一處」那批一起 |
| `MirrorTests` 的瘦身；`ProjectComposition` 測試對產生器敏感；測試資料仍叫 `sample_app` | apps→games #294 | 下一次動建置層測試的批 |
| README 三語版、`docs/architecture.md`、`docs/creating_games.md`（#294 由 `creating_apps.md` 改名）大多是 master 的舊內容 | apps→games #294 | 圖書管理員 |
| `RejectedSpawn` 的當場回傳值把 hook 輪也算進去（既有；KR 已把 `ChildOf` 的當場回傳值改成只看這一筆） | KR #295 dev_log「審查之後」 | 下一次動 Kernel 的批 |
| Kernel 的 `Detail::Relationships` 和 `Derive` 的旗標重疊 | KR #295 dev_log「審查之後」 | 下一次動 Kernel 的批 |
| HarfBuzz 單檔彙整 `harfbuzz.cc` 本機編譯 172.6 s（T 的停止門檻 180 s，只差 4%；FreeType 35.4 s，合計 208 s／門檻 300 s）：單一編譯單位不能並行、可能成為建置的關鍵路徑；起因是 SDL 總宣告第 9.1 節「third_party 無條件加入、每個組態（含 Match）都建」。改法候選：改編上游個別 `hb-*.cc` 讓它並行／HarfBuzz 精簡組態／third_party 跟著有人用才建（建置圖的 Architecture Delta） | T #297（主 checkout `build/evidence/gyo2-sdl-text/build-time/measurements.md`、design-review 第 3 條） | 下一個加 third_party 的批之前看；或 L1 時間明顯變長時 |
| hitbox 回溯（lag compensation）：master 沒有（判定用當 tick 姿勢），GYO2 要做是全新設計；命中形狀的姿勢要等 model＋骨骼動畫（Match 端也要） | PvP 移植準備（共享筆記 `gyo2-pvp-prep-physics-needs.md`） | PvP 移植時，和網路、權威一起設計 |
| **待使用者定（未決）：model Module 的時機**——前提：model 放 Engine，做成 `engine/modules/model` 函式庫（`library_only=yes`、`default=no`、無 `ModelModule`；節點、材質、蒙皮網格、動畫片段、取樣 Pose＋ufbx adapter）。(A) 照 11-19 跟著產品（PvP、model_viewer）回來／(B) 現在當基礎做，排在 SDL 之後或併進畫面子批 | 主會話 2026-11-23 提（原文只在對話，11-23 晚補進本表） | PvP 移植準備的盤點出來後交使用者 |
| **待使用者定（未決）：骨骼動畫的三層**——1 資料與取樣（骨架、片段、時刻 t 的姿勢純函式）→ model 函式庫；2 播放（每個實體播哪個片段、播到哪裡，照 World 時間前進取樣 Pose）→ 登記單位 `animation`／`AnimationModule`，PvP 進模擬所以要能進回滾、取樣照 tick，碰時間／回滾／權威，是基盤；3 蒙皮顯示 → render，經 Extract 拿 Pose。hitbox、切片段、混合規則是遊戲規則 | 主會話 2026-11-23 提（同上） | 同上 |
| **已定：留著**（`v8-arch-gyo2-decisions-2026-11-24-morning`）：`tests/common/model/contract/skinned_bar.fbx`（#313 合進 gyo2）帶著本機絕對路徑（含使用者名稱，Blender 寫進貼圖的路徑）；#317 已改成從 `/tmp/gyo_model_contract/` 匯出、重產的檔沒有，但舊內容仍在 gyo2 的 git 歷史（b6196726 之前）。不 rebase、不強推的前提下只能留著 | model #317 | 不處理 |
| 動畫事件（PvP 盤點 4.6 放在第 1 層，model #313 範圍沒做）：model 的純函式「區間內穿過了哪些時刻（含跨 loop）」＋它的資料；PvP 的事件在 `viewmodel.anim_events.json`（例：`Armature|Reload` 1.887 s 的 `magazine_in`），JSON 的讀法與事件→聲音是遊戲的 | model #313（`model.zh-Hant.md` 第 5 節、dev_log 後續；使用者「沒做的，可以轉給主會話。」） | PvP 移植接 viewmodel 動畫時 |
| K（KeepTool＋ImGui）：ImGui 預設 16 位元索引，`RenderCommands` 用 32 位元，K 要定 `ImDrawIdx` 怎麼接；R2a 沒排 extract schedule，第一個讀來源 World 的子批（K 或 R2b）排 | R2a #315 | 子批 K（開卡時寫進 brief；R2b 已排 extract schedule，#319） |
| R2b 讀 `ImageData` 照 `image.zh-Hant.md` 第 2 節：RGBA8、straight alpha、第 0 列在最上面、不翻轉、rowPitch＝寬×4、色彩空間是標籤；上下方向在 R2b 的 UV 轉換單一邊界處理（master 坑 17）。另：X1 定的 2D 排序是 z 大在前 | I #301、X1 #298 | 子批 R2b（開卡時寫進 brief） |
| 角色控制器（#311，照 avian MoveAndSlide）的已知缺陷 S1：`BoxSet` 相鄰同高方塊的接縫，`CapsuleY(0.5,0.5)` 走過最大抬高 8.8 mm、水平速度慢約 10%（相鄰頂邊給 11° 的接觸平面）；PvP 體素地圖接縫多。主會話判（卡片建議）：修在 Engine，照 parry 複合形狀 `FIX_INTERNAL_EDGES`（`src/shape/compound.rs:30-40`），`BoxSet` 加作者可選的旗標。比過：(a) 工具烘焙時合併共面方塊／(b) Engine 旗標←選／(c) 作者在 onHit 忽略近地面平面 | 3D 物理卡 #311（owner 頁第 3 節、dev_log `2026_11_23_gyo2_physics_3d_move_and_slide`） | 3D 物理卡第三個 PR（疊在 #311 上） |
| `BoxSet` 的 `FixInternalEdges` 預設改 ON（主會話判，2026-11-23）：照 parry／avian 的 Voxels 形狀（無開關、永遠用鄰居分類避開內部邊，parry `src/shape/voxels/voxels.rs:14-28`），`BoxSet` 用途就是體素地圖，Compound 預設 OFF 的理由大多不成立；§1-10「既定値は便利に」；OFF 時 PvP 忘了開 S1 會靜靜回來。比過：OFF（照 Compound）／ON←選。代價：建立時一次 O(n log n)＋每方塊每面一格記憶體。§12 的「照 parry 定」是卡片自己寫的，改預設不是放寬判定 | 3D 物理 #312 | — |
| 互相重疊（非剛好相接）的方塊 `BoxSet` 不當延續（#312 限制）；要不要由體素工具／voxel 契約保證 colliders 不重疊 | 3D 物理 #312 | PvP 的地圖／體素那一批回來時在契約裡定 |
| W（#308）CI 沒驗到、留實機的：執行中改「能不能改大小」、`ResizeWindow`、游標（warp、相對移動、進出）在真視窗上的行為 | W #308 | Object_Connect 第一次在實機跑時 |
| 角色控制器不放站地、坡度、台階、貼地、重力（rapier 綁在控制器裡；§1-1 作者的規則） | #311 卡片判斷 | 只記錄 |
| Bevy `TransformHelper`（從 `Transform`＋`ChildOf` 當場算全域姿態）：目前沒有判定讀姿態，沒加 | X2 #303 | 有 System 在傳播之前要讀全域姿態時 |
| 3D 物理卡（#309）照新 AGENTS 自判的四件（使用者可在合併前推翻）：(1) `BoxSet` 的加速結構不改結果（等於依索引線性掃描，最近者＋平手規則答案唯一；BVH 而非均勻格）；(2) 內部 double、結果 float（大地圖精度，Jolt DOUBLE_PRECISION、parry f64）；(3) 不另立物理姿態元件，`Transform` 就是判定值（CCR-12），有剛體時重看（RC-P3-3）；(4) World 查詢遇不支援的組合整個回 `Unsupported`（P3-4、§1-9） | 3D 物理 #309（宣告 4.2、4.4、第 5 節；owner 頁 `physics-3d.zh-Hant.md`） | 只記錄；角色控制器在同一張卡另開 PR |
| I 卡照 L17 自判的六件 I-1～I-6（只有 RGBA8；settings 只收 colorSpace；image 是 library_only、產品自己 `Register<PngLoader>`；型別名 `ImageData`；長寬上限各 16384；stb 只抓單一檔＋SHA-256）在 `image.zh-Hant.md` 第 5 節。照新 AGENTS §2.6 不必逐條核准；使用者想推翻時再開 | I #301 | 只記錄 |
| **Object_Connect 移植實作**（**2026-11-24 四段全部合併**：#314 1d591cd1、#316 a083d2a4、#318 a3757a5f、#321 2376bac7，`v8-arch-gyo2-object-connect-port-merging`；2026-11-24 開卡 `local_f0c55417`；使用者「我只看到你宣告做完了，實作移植呢？」）：移植準備（#302、#304）只搬了遊戲邏輯與 World 側，沒有主程式、畫面、輸入、聲音、文字。分段：主程式＋W 的輸入＋T 的文字（現在）→ A 合併後接聲音 → R2a 合併後用 2D pass 畫出來、實機可玩 → R2b 合併後補動態頂點與固定解析度縮放 | 主會話 | 進行中 |
| Object_Connect 盤點的 Engine 缺口（`games/object_connect/docs/porting-inventory.zh-Hant.md`，#302）：G3 游標 warp、G4 滑鼠相對移動量、G5 游標進出視窗、G6 視窗不可改大小 | Object_Connect 移植準備 #302 | 子批 W（開卡時寫進 brief） |
| Object_Connect 盤點的 Engine 缺口：G1 播放中改音量、G2 循環播放 | Object_Connect 移植準備 #302 | audio 小批（Object_Connect 要聲音前） |
| examples 只有一個 `examples/CMakeLists.txt`：第一個帶 `sdl*` 的 example 要能被 `plugin_removal.py` 拿掉（例：每個 example 一個目錄、照目錄加入），否則 CI 的 core 步驟報「still names the removed Plugin」；Plugin 自己的測試可用 `gyo_product_units(... PLUGINS <自己>)` | P #300 | 子批 W |
| master 留下的 SDL「component」機制是死碼、沒接上 `plugins/sdl`：`GyoComponents.cmake` 的 `SDL_*`、`GYO_ENABLE_SDL3`（無人讀）、`GyoRuntimeDeployment.cmake` 的 `TARGET SDL3-shared`（在 Plugin 加入前查，永遠 false）、core preset 的 `GYO_BUILD_SDL_*`；寫在 `sdl.zh-Hant.md` 範囲の限界 | P #300 | 重審產品登記機制那批（Object_Connect 移植時） |
| SDL 升到 3.4.20（使用者已同意另開；補丁要在新版重現缺陷再移植，步驟在 `sdl.zh-Hant.md`「升版」） | P #300 | 小批，W／R2a 之後 |
| SDL-1 的原始碼掃描與「SDL 只被 `sdl*` PRIVATE 連結」的設定時檢查（總宣告 9.1） | P #300 | K 之後 |
| SDL 目前靜態建置（master 是動態）；改回動態要和 K 的打包一起，並處理上一列的順序問題 | P #300（卡片判斷，可推翻） | K 的打包 |
| CI 在「另一棵原始碼樹」上建置時，Windows（sccache＋MSVC）疑似把 checkout 的 object 給副本用（`std::source_location` 是原路徑，`authority.authority_sources` 失敗；本機無 MSVC 未證實）：之後這類建置不該和 checkout 共用編譯快取；工作流程政策規定 launcher 只由 native-setup 設 | P #300 第三輪 CI | 下一次動 CI 的批 |
| `AssetLoaderRegistry`、`AssetStoreDiagnostics`（共用處的兩個視圖）在正式程式裡沒有讀者，只有測試讀；「asset 不依賴 threads」（`asset.zh-Hant.md:129`）讓 `WorkerPoolExecutor` 只轉手，判保留（測試的完成順序接點） | 2026-11-23 代碼審計 | 產品第一個用到 asset 時再看 |
| 守衛的讀法再收斂：entity_store_guard、os_boundary_guard 各自還有一份 `strip_comments_and_strings`；「切段找 `..`」的同一段寫法有 5 份 | #269 | 下一次碰守衛的批 |
| naming_guard 的 `SCAN_ROOTS` 沒涵蓋 `build/acceptance` | #269 | 同上 |
| 三份突變清單的路徑還指向 master（big-map engine_format 67 個、audio 12 個、time-threads-trace 6 個）；authority_scan 的路徑寫法待確認。2026-11-22 交換點卡的靜態掃描：所有清單裡 find 指向 gyo2 已刪檔案的共 103 條（master 時代舊計畫的清單） | #269、#270 | **已完成**（`v8-arch-gyo2-mech-cleanup`）：engine_format 整份刪除（68 退役）、time-threads-trace 的 `tt2-*` 6 退役（`TraceSink` 不在 gyo2；傳輸決定搬到 logging LG-6，逐條比對後 `tt2-flush-no-wait` 改寫成 logging 清單的 `lg-file-flush-no-wait`，另 2 條列下一列）；audio 已被 audio 重審、#288 跟上；authority_scan 的 `file` 以清單所在目錄為基準，18/18 killed。掃描後全部清單的 `find` 都恰好命中一次 |
| logging LG-6 的兩個決定沒有突變守：「推進空佇列才喚醒寫者」（`JsonLinesFileSink.cpp` 的 `if (wake) writer->Notify();`；現有案例都 Hold 住 output，突變只會 timeout）、「Close 寫完停止後取到的那一批、再寫結尾行」（沒有決定性的案例）。都要先動測試再加突變 | 機械清理卡（`v8-arch-gyo2-mech-cleanup`）退役 `tt2-no-wake`、`tt2-final-batch-unwritten` 時的比對 | 下一次動 logging 的批 |
| 實樹陽性對照重疊：`wt-guard-real` 與 `nm-real-acronym` | #269 | 下一次碰 naming 的批 |
| 作者寫 `WorldGeneration` 就能結束或重新進入生成，`Product::Regenerate` 不必做 | 生成世界 PR #272 的設計審查 | 有產品需要重新生成時再看 |
| 預設都帶 `entity_ids` 與回滾時，配發器沒有登記成可回滾 | 同上 | 回滾與 entity_ids 一起動的批 |
| 網路複製的外來編號也要先動配發器的範圍 | 同上 | 網路複製那批 |
| 「條件擋住的讀者拉住保留」是事件游標契約的問題 | 同上 | 狀態機那一批提 |
| 私有常數 `kSkeletonOwner` 的名字已過時 | 同上 | 下一次動 orchestration 的批 |
| 權威突變 `au-z6-skipped-counted` 存活（find 在 `ServerAuthorityStrategy.cpp` 的 `RunsWhileResimulating` 那行，replace 換成 `MountedIn`）：突變過時（fadaeb55 之後兩者可能等價），或測試在 #269 之後失去檢出力；不是第 5 批造成 | 交換點卡的合併前完整突變（0bca189d；證據 主 checkout `build/evidence/gyo2-exchange/full-mutation-0bca189d/authority_mutations.log`） | 2026-11-22 開卡查，PR #273（原因：61d6f72c 起測試失去檢出力；修正：expect 改指既有 CHECK `[au-z6-mount-added-after-skip]`，不放寬判定）|
| 入口寫 **Component** 的決策輸入時沒有輸入歷史，預測的 World 重跑時讀到最新值；61d6f72c 只替 Resource 解掉（代為登記 `rollback.Input<T>()`），Component 那邊還在。機制層問題（`v8-arch-gyo2-component-input-history-gap`） | au-z6 卡（PR #273）| 已含在網路宣告第 3 步（已核准），網路卡做 |
| N13 條文補兩條判斷、兩個例外：2026-11-22 使用者「照建議補」，PR #274 已合併（8e894776，`v8-arch-gyo2-n13-rule-text`）。剩 tests/、examples/ 的 493 處全名（89 檔）與 `GYO::Test` 裡的寫法；過時註解 `BufferedStream.cpp:10`、`MountPoint.hpp:7` | N13 短名卡（#271） | 493 處：下一次碰測試命名的批 |
| 生成世界的文件遺留：dev_log 開頭改已合併（PR #272，879350d4）、CI 補 run 45019227814；核准頁第 5 節補 16a3b0d9 的 L1；README 索引兩行改已合併；宣告「三家讀檔都重新配號」改正（flecs 是混合型，`deserialize.c:129-168`；讀檔卡 2026-11-22 指出） | 生成世界卡（#272 合併後） | 下一批一起改（第 1 批先例） |
| 已合併批次的 dev_log 開頭補「已合併」：threads／io（PR #277，0f3be2cc）、狀態機（PR #281，1df88fe7；L1 run 45042604998） | threads／io 卡、狀態機卡 | **已完成**（`v8-arch-gyo2-mech-cleanup`） |
| loader 失敗訊息裡的 id 由每個 loader 各拼一次、store 又加一次（設計審查 S-2） | audio 小批 #288 | owner asset；第三個 loader 出現前定 |
| `Mixer.cpp` 裡兩份手寫的 SPSC 環（設計審查 S-3） | audio 小批 #288 | SDL 後端接上 `Render` 之前整理 |
| 突變 `au-ctl-stress-reassign-every-block` 在 gyo2 上兩次存活（時序偶發，未改的程式上也存活） | audio 重審、audio 小批 | 下一次動 audio 時重看這條突變 |
| 建置層仍寫死 master 的 `apps/`（`build/cmake/GyoAppRegistry.cmake:261`、`GyoApps.cmake:38`、`GyoBuild.cmake:68-91`、`tests/common/ci` 的 app 測試）；頂層已定為 `games/`（`v8-arch-gyo2-top-dirs-games-assets-tools`） | 主會話 2026-11-23 | **2026-11-23 完成**（PR #294 併入 `gyo2`，92d398b5；使用者「ci 綠了就合併」）（`v8-arch-gyo2-build-games-dir`，任務卡；方案 B：目錄與用詞 app→game、四個守衛的掃描根、複製整合的 mirror；dev_log `2026_11_23_gyo2_build_games_dir`；LIM-9） |
| gyo2 上狀態機的測試有一個編譯警告（網路卡的實作者跟上 gyo2 時看到；Windows 若把警告當錯誤會擋住） | 網路卡 2026-11-23 | 看 #275 的 L1（run 45056638255）；擋住就交網路卡在同一 PR 修，否則下一次動狀態機的批 |
| **Kernel 的 `Message` 與 `Summary`**：`Summary` 雜湊每個訊息緩衝的序號與游標，依完成時序或外部事件發訊息的 Module（網路、輸入）會讓摘要依時序變；asset 用 Resource 繞開。要不要給訊息型別「不進摘要」的選項 | asset 宣告第 15 節第 8 項 | 2026-11-23 開卡（`local_b3d28b59`）；宣告修訂 1（分支 `claude/gyo2-kernel-summary-messages-decl`）建議 (b) 回到摘要版本 1（只收每種訊息的筆數與內容，拿掉序號與游標表）；權威 digest 本來不收訊息、受影響的是用 `World::Summary` 判相同的測試；使用者選 (b)（`v8-arch-gyo2-kernel-summary-messages`）；實作完成 PR #287，合併前核准（`v8-arch-gyo2-kernel-summary-messages-merge-approved`）；2026-11-23 使用者「合併 #287」，已合併（72b664fb） |
| **Kernel 負向編譯樣本可能空過**：`tests/common/kernel/CMakeLists.txt:231` 只在輸出裡找標記，同一行有無關硬錯誤時編譯器印出原始碼行、標記就在裡面，樣本永遠通過；asset 改用「靜態斷言失敗＋標記同一行」 | asset 宣告第 15 節第 12 項 | 2026-11-23 開卡（`local_b3d28b59`）；PR #286：重現空過、判法收成共用函式 `tests/common/support/ExpectStaticAssert.cmake`（Kernel、world_logging、world_asset 三處）、27/27；§14.4 已核准，**PR #286 已合併**（94ca1517，`v8-arch-gyo2-compile-fail-judgement-merged`） |
| 負向編譯樣本在判法以外的骨架（對照組、迴圈、MSVC 旗標）三處各有一份（Kernel、world_logging、world_asset） | #286 設計審查 should 4 | 下一次碰負向編譯樣本的批收成一份 |
| 游標的鍵含 System 的索引、跟登記順序走，和 D-9／J-44「對登記順序不敏感」相衝（讀程式推得、未實測）；摘要選 (b) 時離開摘要 | Kernel 卡 | 摘要定案後看還在不在 |
| asset 丟棄件數（E3）沒有出口：登記單位原本沒有 Product 結束回呼 | asset 宣告第 15 節第 9 項 | 交換點批已加 O-15 `OnDestroy`，待確認是否已解 |
| 批次表 asset 列的依賴（生成世界、JSON）與單位兩個；SDL 那批範圍記「image Module（stb adapter）」 | asset 宣告第 15 節第 1 項 | 已轉 SDL 宣告卡 |
| v2 之前的 target 文件（`runtime-composition` §3a 與 DI 3、`target/README.md` K.3 表 A3、A4）建立在已拿掉的前提上；舊頁 `assets.zh-Hant.md` 與 `rebuild-1111-decisions-status` 對 11-11 方向 2 已推翻的狀態要更新 | asset 宣告第 15 節第 2、7 項 | 主會話的文件整理（併「v2 之前的 target 文件」那件） |
| 打包時附第三方授權告示加 stb_image | asset 宣告第 15 節第 3 項 | 併「打包時附第三方授權告示」 |
| catalog 工具（audio_editor 寫，ui_editor、model_viewer 讀）搬回時照新的契約（格式歸 asset） | asset 宣告第 15 節第 5 項 | 搬產品時 |
| io 裡還有沒人用、但不在重審宣告清單裡的介面：`IOFwd.hpp`、`IFileSystem::Name()`、`FileInfo` 的 ctime／atime／permissions、`NormalizeOptions` 的旗標；`OS::Files::DefaultCaseSensitivePaths` 留不留未定（threads／io dev_log「後續」） | threads／io 卡（#277） | 下一次動 io 的批照同一判準（該不該存在）處理 |
| `gyo_os_tests`／`os.wake_fallback` 在本機負載下偶爾逾時：**原因已切分**（os 逾時卡 PR #289，`v8-arch-gyo2-os-test-timeout-cause`）——屬「測試」：`WakeSignalTests.cpp`「loses no signal」的 signaller 自旋 4096 次後 yield，負載下每次讓出 CPU 最多約 10 ms，案例時間≈進 Block 的回合數×反應延遲（16363×1.695 ms≈27.7 s，實測 27.9 s）；觸發條件屬「環境」（機器的 CPU 負載）；「實作」沒有證據（WakeSignal 喚醒延遲負載下約 10 µs）。60 秒本身沒重現；負載下這個案例的檢出力變弱（進 Block 的回合 2.5～19% → 26～88%）。**範圍の限界**：網路卡那一次 `os.wake_fallback` 失敗的輸出沒存到，分不出失敗或逾時 | os 逾時卡 | #289 只加觀測（使用者選 (d)），2026-11-23 已合併（3dd3c272；使用者「你把需要合併的，按順序合併掉。」）；改退避的 (e) 另開卡（`local_3ef794a9`，PR #293）：**2026-11-23 使用者撤回退避的修改，只拿掉 Note**，PR #293 已合併（558d41bf；使用者「L1 過了就合併」）（`v8-arch-gyo2-os-wake-test-backoff-withdrawn`；CI 與 Linux 上沒有這個問題，只在 Intel Mac 本機滿載時出現） |
| `target/v2/README.md` 的 dev_log 清單：每合一個 PR，其他開著的 PR 就在這裡撞一次 | 2026-11-23 合併順序 | 待使用者決定要不要改寫法 |
| `orchestration_mutations.json` 約第 3521～3532 行留著 `or-exch-round-after-failed-in` 的舊副本（find 是 C19 之前的寫法，少了收尾 `},` 而和 `wg-held-not-cleared` 黏成一個物件，鍵重複被後面那組蓋掉）。同 id 的現行版（find 對 `RunRound`）在後面另一個物件，已執行並 killed（`build/evidence/gyo2-exchange/c19-mutants.log`）。刪掉舊副本後解析結果完全相同（282 條、無重複鍵、find 各命中一次），不必重跑。threads／io 卡發現（它判成「從沒執行過」，實際只是殘留文字） | 7b4cf2e9（交換點補回突變時）；threads／io 卡 2026-11-22 | **已完成**：讀檔實作 d7c0dd74 時已正規化（`2026_11_22_gyo2_restore_impl` dev_log）；機械清理卡確認 288 條、無重複鍵與 id |
| 游標宣告第 3.2 節引的 lightyear 行號漂移（現在 `receive.rs:54、:105-110、:153-160、:447-456`） | 狀態機卡交叉比對 | 圖書管理員 |
| 「送進 World 外服務的時刻都是時間基準」要不要成跨 Component 規則（對照 `docs/cross-component-rules.md`） | audio 重審宣告 DI 3 | 圖書管理員 |
| asset 宣告第 140、994 行仍寫 SDL adapter／ufbx「未定」，已由 `v8-arch-gyo2-sdl-q1-b-prime`、`-sdl-q2-ufbx-stb-module` 定 | SDL 宣告卡 | 圖書管理員 |
| 回滾頁 R-2 前提 1 要不要明寫「`Changed<T>`／`Res::Changed()` 讀的是 stamp，不在前提內」 | Kernel IsChanged 卡 | 下一次動回滾 Module 的批 |
| time-events 宣告 5.1 第 3 項「VirtualTime 用 `Changed<…>` 抓暫停」不成立（每幀被 AdvanceTime 寫）；宣告不改，Kernel 說明頁列出 Engine 每幀寫的 Resource | Kernel IsChanged 卡 | 由 Kernel 卡的說明頁收掉；宣告是當時紀錄不改 |
| 事件游標重評（停跑的讀者拉住保留、恢復時補讀；Kernel 的事件游標契約） | 狀態機修訂 8 交叉比對，使用者選 (A＋)（`v8-arch-gyo2-state-machine-r8-crosscheck`） | 2026-11-22 開卡；同日使用者定 (A)＋說明頁、不改 Kernel（`v8-arch-gyo2-event-cursor-reeval-author-side`）；**PR #278 合併**（6ee8d8b1） |
| Engine 公開標頭與共通測試裡的產品名（`v8-arch-gyo2-oc-separation-review` 1、2）：window／input 標頭的 Object_Connect G 編號只留 Bevy 對應；sdl_gpu、audio、image、text 測試的產品名與「Object_Connect-sized」預算改成 Engine 自己的目標（判定不動） | 架構師 2026-11-24 | **已處理**：#319 的獨立 commit 29d27487 |
| **已定 (a)（暫定，`v8-arch-gyo2-oc-fast-float-a`）**：Object_Connect 直接連 `gyo_fast_float`（`v8-arch-gyo2-oc-separation-review` 3）——產品自己宣告依賴；使用者「後面我想想如何改」 | 架構師 2026-11-24 | **註解已合併**（#328 034ab13f）；之後使用者可能改 |
| `sdl_gpu.frames` 允許全部幀被跳過（swapchain 拿不到畫面時 0 幀也算通過）；Object_Connect #318 在 Linux Xvfb 的隱藏視窗 63 幀全跳過，是遊戲測試的「至少顯示一幀」抓到、sdl_gpu 自己的測試沒抓到 | Object_Connect 卡 2026-11-24 | **已處理**：#320 c862aed1（2D／3D 幀測試改用顯示的視窗、要求至少一幀送到視窗） |
| `engine/modules/render/include/engine/render/RenderCommands.hpp:5-9` 開頭註解過時：還寫「R2a 只有 2D pass、3D 跟 R2b 來」與「view 拉滿整個目標」；R2b（#319、#320）之後是 `Pass2D::viewport` 與 `Pass3D` | Object_Connect #321 設計審查 2026-11-24 | 下一個動 render 的批（K）順手改 |
| Object_Connect 拖線時不切 `CursorGrab::Locked`：視窗貼螢幕邊時推不過那個邊；要把 `PointerWrap` 改成相對量追蹤（產品內的改動） | Object_Connect 卡 2026-11-24（#318、#321 的已知限制） | **使用者實機試過**（2026-11-24「這個我已經試過了。沒多大問題。」）；**已合併**：#329 f332b234（Engine 選項）＋#328 034ab13f（鎖游標＋fast_float 註解）；Windows 實機速度未量 |
| JSON Module 加排版輸出（pretty print）：`gyo.ui` 經 JSON Module 寫出變成一行，Editor 回來前要有排版輸出，否則檔案 diff 沒法看（`v8-arch-gyo2-sdl-u-three-judgments` 2） | U #322 | Editor（工具線）回來前 |
| `TextComposition.cursor` 改成位元組、由 `sdl_window` 的 adapter 換算（同 winit；`v8-arch-gyo2-sdl-u-three-judgments` 3） | U #322 設計審查 S2 | **已處理**：#326 bec63ca5 |
| **已定：移植做完再開**（`v8-arch-gyo2-big-map-after-ports`）**大地圖線**（`v8-arch-gyo2-pvp-playtest-2026-11-24` 3）：master big-map（1 km²、cell 0.5 m、`gyo.voxel_map`＋`gyo.voxel_region`）做到格式、分塊工具、broadphase，scene v1 與 pa-measure 沒做；gyo2 只有單檔 v1、無剔除、無地圖編輯器；約 200 m 見方內現狀可行（未量）。開線要照 gyo2 重對層的歸屬（工具與遊戲只用資料形式交接） | PvP 卡 2026-11-24 | 兩個移植做完後 |
| **已定 (B) 准許換來源**（`v8-arch-gyo2-sdl-r1b-approved-b`，實作中；宣告 `sdl-extract-replace-declaration.zh-Hant.md` 修訂 1，分支 `claude/gyo2-sdl-r1b-extract-replace-8886f8` 6ff83c7b）。更正總宣告 6.1-4：只有 (B) 要改 Kernel 條文（DI-KS 13、26、37、D-E14；新增 DI-KS 38、RC 29），(A) 只改編排層 O-8、O-12 | R1b 卡 2026-11-25 | 實作中；條文最終文字合併前核准 |
| PvP：連線目標、userId、token 在 `NetProductOptions`（建 Product 時定）、前台 World 一準備好就握手；要「每次換前台時才給目標、大廳不連線」要由遊戲或 net 定（R1b 的用法：前台 World＋渲染 World，runner `ReplaceWorld("front", {extra})`） | R1b 卡 2026-11-25 | **主會話判 (1)**（`v8-arch-gyo2-net-connect-target-per-world`）：PvP 卡另開 net PR；PvP 改成換前台 World 要等 R1b 與它都合 |

## 不擋路的小事

2026-11-17 照使用者的話重篩（「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」，HANDOFF 經過同日）：能照命名規範、別家原始碼、職責清單自己判的，主會話自己判並標出；只留會推翻使用者之前的決定、別家分歧且真有取捨、職責清單要多一項、或破壞性的操作給使用者。批次先後、各宣告由誰寫，是主會話排的（統合卡判）。

**交使用者的**
- **五個試做的工作樹**要不要刪（刪除是破壞性操作）。2026-11-18 使用者：「如果沒有影響你的記憶，工作樹在最後統一處理。」——GYO2 線收尾時一起處理，到時再問刪除。
- **SDL 的 adapter、ufbx 歸哪一類**：牽涉 AGENTS §6 的判準和 11-16 新定義的對齊（收斂定義留下的未確認），會動規則文字，不自己判。

**主會話自己判的（照 L17）**
- **時間 Module 的宣告由主會話寫**：統合核准後，把舊 T1（`time-types-declaration.zh-Hant.md` 修訂 2）的型別與累加器、時鐘與 tick 移入、時間的 System 合成一份；T1 待使用者的四件在那時重篩——頻率的型別照 Bevy `Time<Fixed>`／Unity 的做法判，主會話的四處調整照新前提重寫；只有會推翻使用者決定的才進確認清單。
- **K0 入口檢查的先後**：Kernel 減法・結構段的事前宣告裡定，照現有程式與 Bevy 的檢查順序。
- **Module 的預設**：預設全帶、產品可以關（Bevy `DefaultPlugins`、Unity 內建模組都是預設帶、可關；O3DE 要列 `enabled_gems`，是少數）。照 `engine-not-rigid` 的「預設方便、不禁止別的做法」。在編排層骨架的宣告寫明。
- **「誰實際寫的」除錯工具**：權威策略不數寫者之後，沒有和它重疊的檢查；不另開一批，等有產品需要時再看（AGENTS §1）。
- **v2 之前的 target 文件**（runtime-composition §4、架構 README 的兩行）：統合核准後由主會話一次改；**LIM-7** 隨區規則移出，在權威＋回滾那條線的宣告裡重看。

# GYO2 批次表（2026-11-17 重排）

Owner：架構（GYO2 主會話）。使用者 2026-11-17：「你把過時的任務先重做，然後排給我。」

這一頁**取代**[時間設計](rebuild-time-design.zh-Hant.md)第 11 節的批次表（那一張建立在「Kernel 管組裝、限制在 Kernel」的前提上，已過時）。新的前提是 2026-11-16～17 在邊界卡定的：Kernel 只做 ECS、另立編排層、時鐘與 tick 在時間的 Module、權威與回滾是作者可選的策略式 Module、每層一份職責清單（HANDOFF `v8-arch-gyo2-kernel-is-ecs-no-restrictions`、`-orchestration-layer-name`、`-clock-out-of-kernel`、`-rollback-module-strategy`、`-layer-duty-lists` 等）。舊卡回收了什麼、哪些不帶進來：[recovered-from-stale-cards.zh-Hant.md](recovered-from-stale-cards.zh-Hant.md)。

每一批開工前都有事前宣告、經使用者核准；實作批的判定照 AGENTS §14。

---

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
| 4. 權威 Module＋回滾 Module（同一條線） | **2026-11-20 開始、寫事前宣告中**（批次代號 `v8-arch-gyo2-authority-rollback`；宣告 `authority-rollback-declaration.zh-Hant.md`，卡片分支 `claude/gyo2-authority-rollback-*`）。照各自的宣告；**另收 `World::Summary` 改成逐型別的雜湊函式**（和 digest 方向一致；解決原始位元組雜湊的 padding、指標／heap 漏算、+0／-0 三種毛病；在那之前 restore case 5 與其他比摘要的測試標「未完全解決」，`v8-arch-gyo2-summary-padding-a-then-c`）；**同一條線拿掉 Kernel 的權威規則**（寫者數量、digest、區規則與 Z5、回滾的宣告與 Rewind／Replay）。守它們的突變一起處理：Z5、寫者數量、區規則相關的 `we-*`、`td-*` 裡的 W-1 期待、`rs-j17-zone-b-listed`（測試舊帳卡交接） | — | 各自的宣告、2、3 |
| 5. 生成世界（重做） | 每個 World 各自、可以多次、可以分批；放編排層；Kernel 不認得階段（`-two-layers-like-bevy`）。另收**預設的實體配發器**（`v8-arch-gyo2-default-allotter-module`，主會話照 L17 判）：由一個預設帶、產品可關或替換的 Module 在每個 World 登記範圍配發器並放好預設範圍，作者不宣告也能 `Spawn`；配發策略照舊是產品可換的 Resource，Kernel 不內建預設策略、職責清單不變；不和 Base 的 `IdIssuer` 共用。新卡從回收清單第 2 節起。讀檔卡的要求：讀檔 System 掛在生成世界的方式，判定要有讀檔的案例 | 已歸檔的「重新評估 Loading」 | 2 |
| 5. 多個 World、交換點 | 照統合 | — | 2 |
| 5. 狀態機（重做成修訂 8） | 只改建立在舊前提上的地方：`RequestSilent` 還要不要、(c) 區改走權威策略、插進遊戲初始化清單、套用 System 的插法、以 D-E2／R-E2 為守衛的 DI；遊戲看得到的行為照舊（回收清單第 1 節）。(b) 區的部分照統合 | 已歸檔的狀態機卡 | 權威 Module 的宣告；實作依賴 2、3（(c) 區依賴 4） |
| 5. 讀檔的實作 | 照它的宣告 | — | 讀檔的宣告、2 |
| 5. 存檔版本 Module | 2026-11-18 使用者：「需要提供version module，默認不啟用，啟用之後過時檔會直接拒絕。這個補充不放在編排裡，但是得放計劃中。Engine需要支持。」（`v8-arch-gyo2-save-version-module`）。Engine 提供的 Module，**預設不啟用**（使用者明說的例外，不同於「Module 預設全帶」）；啟用後比對存檔帶的版本標記與作者宣告的最低支援版本，過時就拒絕。主會話的讀法（開宣告時確認）：只比對版本標記、不讀存檔內容（和「Engine 不認得存檔格式」不衝突）；拒絕走讀檔失敗的路（`Result` 回錯誤碼，預設先建新的時舊 World 照跑）。排在讀檔的實作同一條線或緊接其後（主會話照 L17 排） | — | 讀檔的實作、2 |
| 6. JSON Module（最先） | **2026-11-20 完成**（PR #252 併入 `gyo2`，8ec005f2；合併前核准 `v8-arch-gyo2-json-module-merge-approved`）。動工前核准（`v8-arch-gyo2-json-module-declaration-approved`；卡片分支 `claude/gyo2-json-module-9cb753`）。封裝的實作，早於日誌與 trace（`v8-arch-gyo2-json-early`）；走 yyjson＋自己的介面，只依賴 Base，可以和第 3 批並行 | master 用 nlohmann（不帶） | Base |
| 6. 網路 Module | 網路是機制、在搬產品之前做（`v8-arch-gyo2-net-is-mechanism-first`）。照點名的功能逐一推 wire 需求、對照 lightyear、bevy_replicon、Unity Netcode；Gateway 原意（外部多種 Client、內部一套抽象協議、無狀態）；斷線偵測與重連只在 UDP 上 | master 的 `engine/net`、`services/gyo_gateway`（參考） | 3、4；多個 World 與交換點 |
| 6. 日誌機制 | **2026-11-21 完成（不含 span）**（PR #256 併入 `gyo2`，d3371355；`v8-arch-gyo2-logging-merged`）。trace 另成 Module、另寫宣告與 PR（`v8-arch-gyo2-logging-trace-two-modules`、`-trace-direction`），使用者「合併完先等等。」，宣告尚未開始。 | master 的 `engine/trace`（參考） | 2、3 |
| 6. 異常分級 | **2026-11-21 完成**（PR #254 併入 `gyo2`，e0806f1f；`v8-arch-gyo2-error-levels-merged`）。範圍收窄到 D1、D2、P12、P15、退出流程與條文草稿（`v8-arch-gyo2-error-levels-shrink`、`-exit-flow`）。 | `docs/architecture/error-handling.md`（R0–R6 的契約，起點） | 2、3 |
| 6. SDL（第一個 Plugin 批） | `engine/platform` 不回來，功能拆進各 Module，怎麼拆在這批分析（`v8-arch-gyo2-platform-obsolete-split-into-modules`）；plugins 目錄的建置；input、render、text、ui 依序回來；碰外部組件的程式一律放各 Module 的 `src/adapters/<組件>/`（SDL 放 `src/adapters/sdl/`），同一批把 `engine/os` 的 `src/posix`、`src/windows` 搬到 `src/adapters/`（`v8-arch-gyo2-json-adapters-dir` 的推廣；搬檔時查守衛 glob 與突變 find 路徑） | master 的 platform、input、render、text、ui（參考） | 2、3 |
| 6. asset | 資源目錄、載入、生命週期 | master 的 `engine/asset`（參考） | 2；JSON |
| 6. 之後的小事（JSON Module 卡交來） | Kernel 指紋報告手寫 JSON 改用 JSON Module（要動 Kernel 的依賴，另議）；master 七份資料契約的驗證規則收緊，在各契約回到 `gyo2` 的批寫明；打包時附第三方授權告示（yyjson、fast_float 起） | JSON Module 宣告第 15 節 | JSON |

先後：1 → 2 → 3 → 4 → 5（5 的各批可以並行）→ 6 的基礎模組（JSON 最先、可以現在就和 3 並行；網路 Module 在 4 之後即可和 5 並行；日誌與 trace 在 JSON 之後）→ 搬產品（`v8-arch-gyo2-infrastructure-before-products`）。2026-11-18 使用者定：第 2 批的宣告卡等第 1 批收尾、併進 `gyo2` 才開，不並行（`v8-arch-gyo2-batch2-after-batch1-merged`：「這個底層設計，還是穩一點好。而且有些決策還不穩，之前的很多批都是這樣廢掉的。」）。（舊版寫「事件游標依賴減法」，後來改成「游標排在減法之前」，現在照統合併進第 3 條線。）

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

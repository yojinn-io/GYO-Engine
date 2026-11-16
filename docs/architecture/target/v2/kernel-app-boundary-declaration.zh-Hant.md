# Kernel 與編排層的邊界：事前宣告（修訂 1）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。核准本宣告不等於 AGENTS §14.4 的判定變更核准**：每一個守衛判定的變更，在實作那一批的事前宣告逐案列出、逐條核准。
> 批次代號：`v8-arch-gyo2-kernel-app-boundary`。推導過程與四家（Bevy、flecs、Unity Entities、Unreal Mass）的對照在[設計稿](kernel-app-boundary-draft.zh-Hant.md)；本頁是要核准的內容。

**修訂紀錄**：修訂 0（2026-11-16）送 Fable／high 敵對式檢查，16 條，12 條判為核准前必改（證據 `declaration-r0-adversarial-check.md`）。修訂 1 依使用者「重跑一次宣告…以最新的為準，如果拿不準的去檢查一下定案的理由和時間」從頭重寫；各條發現的處理見第 11 節。

## 0. 依據與範圍

**讀的版本**：程式 gyo2 cb6bccca（`origin/gyo2` 到 2026-11-16 深夜沒有新 commit）；HANDOFF `claude/gyo2-time-design` 25515a80；Module／Plugin 定義 PR #240（`claude/gyo2-module-plugin-definition` b0ce94ed，**未合併**）；Bevy 6a911653f。證據：主 checkout `build/evidence/gyo2-kernel-app-boundary/`。

**新舊衝突的規則**（使用者的指示）：兩個已定的事衝突時，以時間較新的為準；第 2.2 節逐條寫出較新的那一條有沒有回應較舊的理由。

**文件的位置**：使用者 2026-11-16 定「v2 之前的 target 文件先不動，後面統一處理」（`v8-arch-gyo2-pre-v2-target-docs-later`）。本宣告與新部件（編排層、權威 Module）的條文放 `target/v2/`。Kernel 的 Design Invariants 仍在 `target/kernel.zh-Hant.md`（`target/README.md`：「改任何部件之前，先讀 kernel.zh-Hant.md」；K0 於 2026-11-16 更新過），它的改寫在實作批做。`runtime-composition`、`world`、`scheduler`、`system-contract`、`limits` 等 v2 之前的 target 文件，本宣告只列「會被影響」，不改，等統一處理。

**範圍（卡片的提議，使用者沒有判，第 9 節第 1 件）**：本宣告定**層與歸屬**——Kernel 留什麼、每樣移出的東西去哪一層、Kernel 要對外多交出什麼、編排層的條文、守衛的處置原則、批次先後。下面兩題各自另寫事前宣告：
- **權威 Module 的內容**：伺服器權威策略到底檢查什麼、執行期的區邏輯怎麼處置、預設區、C 指紋與 G-5 匯出——使用者說「先移除出來。然後得重新評估設計」，這一份做重新評估，不照搬。
- **事件游標**（原時間設計 T4）：游標放哪、進不進回滾快照、上限的單位與誰設、不執行的讀者、跨時鐘事件的網路要求。

---

## 1. 先用遊戲講

**單機 2D 橫版。** 產品的 `main` 只寫「用這些 Module、跑起來」。編排層照 Module 登記檔把各 Module build、finish，建出 `Product`（一個 World），在主 World 跑**遊戲初始化**清單一次（讀按鍵設定；狀態機可以把「進入主選單」插在它後面），然後 runner 每幀跑每幀的清單。「受傷」與「撿補血包」兩個 System 都直接寫主角的血量：Kernel 不攔，照宣告的先後，沒宣告就照登記順序。沒有選權威 Module，就沒有寫者、區、digest 的規則。

**PvP 的 Match。** 產品另外選權威 Module、策略選「伺服器權威」。作者寫 `authority.Component<Health>({...})`，權威 Module 記下它自己的宣告，再替作者登記進 Kernel。編排層 finish 時，策略讀 Kernel 交出的登記資訊加上自己的登記做檢查，不過就啟動失敗。（檢查哪些、執行中怎麼攔，在權威 Module 的宣告定。）

**PvP 的 Client。** 模擬、每幀、渲染三個 World 都在編排層的 `Product` 裡；runner 決定哪條執行緒跑哪個 World、交換點與 extract 什麼時候做。Kernel 只提供 `WorldExecutor`（一個 World 與跑它的執行狀態）、在它上面跑一張 schedule、讀另一個 World 的唯讀參數。

**格鬥遊戲（P2P 回滾）。** 作者選「P2P 回滾」策略——第一版只佔位，啟動時報「這個策略還沒實作」。作者也可以自己寫策略，和 Engine 附的走同一個介面。

---

## 2. 已定的事

### 2.1 這一批依據的決定（使用者，2026-11-16）

| 代號 | 內容 | 原話（摘；全文在 HANDOFF 或設計稿） |
|---|---|---|
| `v8-arch-gyo2-kernel-is-ecs-no-restrictions` | Kernel 是 ECS，不疊加限制；先移出，再重新評估設計 | 「Kernel其實就是個ECS，本身就不應該迭代限制在裡面的。」「先移除出來。然後得重新評估設計。」 |
| `v8-arch-gyo2-orchestration-layer-name` | 那一層叫編排層，`engine/orchestration` | 「算了用這個吧，編排也是挺應景的。」 |
| `v8-arch-gyo2-access-verification-stays` | 存取驗證留 Kernel（開發版開）；DI-KS 2 後半隨權威移出 | 「1）照建議」 |
| `v8-arch-gyo2-event-trim-by-cursor` | Kernel 照游標修剪事件，不看時鐘 | 「它讓『不漏讀』成為 Kernel 自己的保證，不必依賴編排層排得對。」 |
| `v8-arch-gyo2-kernel-world-exec-type` | Kernel 保留一個型別包 World＋執行狀態，叫 `WorldExecutor`；`Product` 搬到編排層 | 「第二個，保留一個型別、另取名。」「照建議」 |
| `v8-arch-gyo2-module-plugin-owner-definition` | Module＝自己維護、介面是自己的；Plugin＝第三方官方維護；權威檢查放 Module | 「權威檢查我想放Module，這個是底層能力。」 |
| `v8-arch-gyo2-module-plugin-definition`（PR #240，未合併） | 定義寫進 AGENTS §6 與 v2 文件；兩份登記檔與兩種型別不變、照新定義重分；Module 排在 Plugin 前面仍寫在同詞異義表 | 三件未確認：產品能否只選部分 Module、SDL 的 adapter 歸哪、ufbx 歸哪 |
| `v8-arch-gyo2-authority-own-registration`（取代 `-authority-tags-in-kernel`） | 權威 Module 自己登記並代為登記進 Kernel；Kernel 不存標籤 | 「4.1 權威 Module 自己登記。」 |
| `v8-arch-gyo2-authority-is-game-design` | 權威是遊戲設計的一部分；策略式 Module，作者選策略 | 「這個東西本身就是遊戲邏輯的一部分…例如我做格鬥遊戲，就直接p2p了。」「讀法一」 |
| `v8-arch-gyo2-authority-strategy-mechanism` | 策略介面＋檢查；作者可自寫；Engine 附的只實作伺服器權威，其他佔位、選了啟動時報錯 | 「這個核心是要建立程機制而不是寫死。」「伺服器權威（PvP 射擊）就行，其他的可以先佔個位子…」「啟動時報」 |
| `v8-arch-gyo2-game-init-list` | 開局切成遊戲初始化（行程一次、主 World、清單可插入）與生成世界（Loading 卡） | 「第一個World，內容有作者自己訂，Engine只提供機制」「對的。」 |
| `v8-arch-gyo2-thread-binding-by-runner` | runner 決定執行緒；需要綁執行緒的 Resource 自己斷言；Kernel 的執行緒啟動檢查拿掉 | 「第二個，別人掉過的坑，我沒必要在跳一次。」 |
| `v8-arch-gyo2-no-ambiguity-detection` | Kernel 照登記順序平手、不報錯；Kernel 與權威策略都不偵測歧義；伺服器權威連線時比 C 指紋 | 「偵測歧義其實是很麻煩的。我個人覺得沒必要。」「可以。」 |
| `v8-arch-gyo2-pre-v2-target-docs-later` | v2 之前的 target 文件先不動，後面統一處理 | 「之前就先不管了。後面統一處理」 |

### 2.2 被這一批改寫的較早決定（逐條比對時間與理由）

| 較早的決定（日期） | 當時的內容與理由 | 較新的決定 | 較新的有沒有回應較舊的理由 | 本批的處置 |
|---|---|---|---|---|
| `v8-arch-kernel-exclusive-default`（11-11）、`v8-arch-gyo2-check-history-first` 第 3 點（11-14：預設獨占留著） | 預設獨占、Shared 才可多寫；理由「權威判定，要可回滾、可比對」；11-14 使用者「多寫的問題有詳細討論過，你去查查歷史」 | `kernel-is-ecs-no-restrictions`、`authority-is-game-design`（11-16） | 有：卡片在 11-16 查了歷史（`single-writer-origin-and-pvp.md`：理念出自 PvP 的 Gateway、案例全是 PvP），使用者看過後說「權威判定落的太深了…本身就是遊戲邏輯的一部分」 | 寫者規則移出 Kernel，歸權威 Module 的策略 |
| `check-history-first` 第 2 點（11-14：`Domain` 四個值留著） | 理由「Loading 不是 PVP 特有的，而是遊戲都有」 | `build-world-phase-is-mechanism`（11-16：「有沒有是作者決定的，而不是Engine」）、`game-init-list`、`event-trim-by-cursor`（11-16） | 有：11-16 使用者自己更正了 11-14 的理由 | `Domain` 移出 Kernel，時鐘歸時間的 Module |
| `check-history-first` 第 5、6 點（11-14：`DigestSet` 與禁 Shared 不急；`Zone`／`Role`／`Rollback`／`CFingerprint` 照不照原核准的切，未回） | 「digest 用不用先不急定」 | `kernel-is-ecs-no-restrictions`（11-16） | 部分：使用者定「先移出」，但沒有回答要不要留 digest；去留在權威 Module 的宣告重新評估 | 移出 Kernel；去留待權威宣告 |
| `v8-arch-gyo2-strict-order`（11-14，PR #228 已合併） | `RequireExplicitOrder` 開關，(b) 區歧義可設為錯誤；使用者「合併」，五項沒有逐條核准 | `no-ambiguity-detection`（11-16） | 有：使用者「偵測歧義其實是很麻煩的…普通程序無法判斷」 | **拿掉**（不是搬到權威）；`StrictOrderTests.cpp` 8 案與 11 條突變退役 |
| DI-KS 5「(c) 歧義是啟動錯誤」「(c) 區對登記順序不敏感」（11-13 核准） | 決定性區不得依登記順序決定結果 | `no-ambiguity-detection`（11-16） | 部分：使用者接受「登記順序相同時平手是決定性的，只有兩邊版本不同才會出錯，比指紋就能抓到」；「對登記順序不敏感」這個性質放棄 | 平手是 Kernel 的決定性行為；「不敏感」不再保證（第 6 節列為放寬的面） |
| K0：`startup-run-by-kernel`、`startup-before-any-schedule`（11-15，#239 已合併）；DI-KS 22 | Kernel 排第一、只跑一次、執行中不准跑別的；理由「系統必須的，就不讓作者碰」 | `game-init-list`（11-16） | 有：使用者「Engine只提供機制」；卡片補的「切開後遊戲初始化不碰寫者豁免」使用者看過 | 遊戲初始化清單在編排層（可插入）；生成世界交 Loading 卡 |
| `v8-arch-gyo2-exclusive-declared-writes`（11-15）、`shared-means-no-owner`（11-15）；DI-KS 23 | `ExclusiveWorld<Writes<R…>>` 列出單一寫者 Resource、列的算寫者；理由「要保證誰寫，別開shared」 | `kernel-is-ecs-no-restrictions`（11-16） | 部分：使用者沒有單獨回這一條；「算寫者」屬寫者規則，隨權威移出；`Writes<…>` 的存取宣告本身怎麼留，見第 9 節第 3 件 | 待第 9 節第 3 件 |
| `v8-arch-gyo2-zone-c-exclusive-apply`（11-15～16） | Kernel 加「(c) 區的獨占套用」，改 S-13、DI-KS 2、Z2、C 指紋 | `kernel-is-ecs-no-restrictions`（11-16） | 沒有直接回應 | Kernel 沒有區之後，這一批的形狀要重看；由權威宣告或那一批的宣告處理 |
| `v8-arch-gyo2-cross-clock-events-open`（11-14） | 跨時鐘事件放開；要求「不光要考慮單機，也要兼容網絡部分」 | `event-trim-by-cursor`（11-16） | 部分：游標保證不漏讀，回應了「放開」；「兼容網絡」沒有回應 | 交事件游標的宣告 |
| `v8-arch-gyo2-module-plugin-registers`（11-14） | 型別 `GYO::Kernel::Module`／`Plugin`、兩份登記檔在建置層、Module 在 Plugin 前、Plugin 可依賴 Module | `module-plugin-owner-definition`（11-16）、PR #240 | 有：判準換了，兩份登記檔與型別保留，照新定義重分 | 型別搬到編排層（命名空間改）；其餘照舊 |
| runtime-composition DI 2（11-13：一個 Runtime 一個 runner）、DI 3／3a（11-11、11-13：世界內容只經 Loading、stage 序）、DI 4（一 World 一執行緒） | — | `game-init-list`、`build-world-phase-is-mechanism`、`thread-binding-by-runner`（11-16） | DI 2 照舊（改成一個 `Product` 一個 runner）；DI 3／3a 交 Loading 卡；DI 4 改成執行時斷言 | 文件不改（`pre-v2-target-docs-later`），統一處理時照本表 |

---

## 3. 分層與歸屬

```
產品（Game／Tool）          main：用哪些 Module／Plugin（產品登記檔）、跑起來
   │
   ├─ 權威 Module（名字待定）、時間、狀態機、audio、網路……（Module）   SDL、ImGui（Plugin）
   │        │ 以登記單位（編排層的 Module／Plugin 型別）接進來
   ▼        ▼
編排層 engine/orchestration   登記單位與生命週期、Product（World 群＋runner）、遊戲初始化清單、
   │                          每幀清單、交換點、extract 的時機、執行緒的安排、啟動錯誤的彙整
   ▼
Kernel engine/kernel          World、ECS 元件／Resource／事件、System 參數與存取推導、schedule、Plan、
                              WorldExecutor、同步點、hook、巢狀跑、存取驗證、登記資訊（FrozenRegistry）
```

依賴方向：箭頭朝下。權威、時間等 Module 依賴編排層的登記單位型別與 Kernel（和 Bevy 的 `bevy_time`、`bevy_state` 依賴 `bevy_app` 同形）；編排層只依賴 Kernel、不依賴任何第三方與任何 Module 的內部；Kernel 不依賴上面任何一層。歸屬照 Module／Plugin 定義（PR #240；本頁不另寫定義）。

**系譜**（AGENTS §4）：分層＝`bevy_ecs`／`bevy_app`（`bevy_app/Cargo.toml` 只依賴 `bevy_ecs` 等底層 crate，wgpu、serde_json 在用到它的 crate）；權威放 Module＝使用者 11-16（Unity Netcode 是套件、lightyear／replicon 是第三方 crate）；`WorldExecutor` 保留為型別＝現在的 `Product` 改名（退掉的：併進 `World`，照 Bevy，要改 B-12、DI-KS 12 與「World＝資料」的定義）。

### 3.1 Kernel 留下的

| 留下 | 現在的位置 | 改什麼 |
|---|---|---|
| World、ECS 元件、Resource、事件緩衝 | `World.hpp`、`detail/Storage.hpp` | 事件改游標（另一份宣告） |
| System 參數與「存取從簽名推出」 | `Params.hpp`、`SystemTraits.hpp`、`Access.hpp` | `CurrentTick`、`ExclusiveWorld` 的 Contribution 見第 9 節第 3、5 件 |
| 登記動作 | `Registrar` | 拿掉 zone、寫入政策、區與回滾的旗標、`DigestSet`、`RequireExplicitZones`、`RequireExplicitOrder`；登記單位的身分改由編排層給（不透明的 owner id，`InitialValue` 的「只有擁有者能宣告」要用，`Registry.cpp:850`） |
| schedule、Plan、Serial 執行、同步點、hook 清空輪 | `Plan.hpp`、`Scheduler.cpp`、`Executor.cpp` | Plan 拿掉 `domain` 與 `EventTrim` 步（Plan 指紋改版，見 3.4） |
| `WorldExecutor`（現在的 `Product` 改名） | `Product.hpp` | 拿掉開局的排程（K0） |
| 巢狀跑 schedule | `ExclusiveWorld::RunSchedule`（DI-KS 16～21） | 刪去和開局、區、clock 相關的分支 |
| 存取驗證（開發版） | W-5、A.6、`World.cpp` | `ExclusiveWorld` 的已宣告集合現在以「zone B 且 Shared」定義（`World.cpp:154-157`、`:983-987`），要改寫（第 9 節第 3 件） |
| `FrozenRegistry`、Registry 指紋 | `Registry.hpp:504-555`、`Fingerprint.cpp` | 多交出讀取面（3.3）；指紋拿掉移出的欄位、改版（3.4） |
| `KernelSettings` | `Registry.hpp:389-393` | 「System 不准讀它」（D-E18）去留見第 9 節第 4 件 |
| `InitialValue`、`SpawnWithId`／配發器、`RejectedSpawn`、`DiagnosticsWriter` | — | 區規則的部分移出；`RejectedSpawn` 與診斷紀錄帶的 tick 來源見第 9 節第 5 件 |
| Extract 參數（唯讀）、`ScheduleOptions::extract`、`ExtractNotNestable`、finish 時用來源 Registry 解 Extract 的 id（`Registry.cpp:343-355`） | — | 來源 World 與執行時機移編排層；`SetExtractSource` 留作 finish 的輸入 |
| 執行緒：需要綁執行緒的 Resource 記來源執行緒、碰錯斷言（**新**，取代 H-2、X-3 的啟動檢查） | `ResourceOptions::affinity` 改義 | 系譜 Bevy `bevy_ecs/src/storage/non_send.rs:74-84` |

**Kernel 保留的啟動錯誤**（ECS 自己的一致性）：R-E2、R-E4（不含 digest set 的那一處 `Registry.cpp:487`）、R-E5、D-E1（不含 Z3 的 ingress stage `:909` 與 eventTrim `:925`）、D-E3（沒有配發器，`:1047`、`:1118`）、D-E4、D-E5、D-E6、D-E7 中的機制規則（H-6 Extract 只在 extract schedule `:1128`、`:1376`；S-8 Diagnostics 只能來自 `DiagnosticsWriter` `:1079`；S-18 hook 不得用 `ExclusiveWorld<Writes<…>>` `:1133`）、D-E17、D-E19、D-E20；D-E18 待定。

### 3.2 移出 Kernel 的（去處）

| 移出 | 現在 | 去處 |
|---|---|---|
| 登記單位 `Plugin`／`Requires`／`Finish`／`InstallRunner`；R-E1、R-E3、R-E6 | `Registry.hpp:41-75`、`Registry.cpp:646-724`、`:1520` | 編排層 |
| `Product` 這個名字 | `Product.hpp` | 編排層（一個產品的 World 群＋runner） |
| 開局的排程（K0）、`ScheduleOptions::startup`（`Declarations.hpp:138`）、D-E21、`StartupNotNestable`、`StartupRunning` | `Executor.cpp:151-170`、`:457-468`；DI-KS 22 | 編排層的遊戲初始化清單；生成世界交 Loading 卡 |
| 開局的寫者豁免（`Registry.cpp:1448-1457`） | DI-KS 6 | 權威 Module（「生成世界的階段」由它登記；保證見第 9 節第 6 件） |
| `Domain`、`clock`、Kernel 推 tick、`AccessKind::DomainTick`、`CurrentTick`、`eventTrim`、D-E12、F-E1 | `Declarations.hpp:56-62`、`Executor.cpp:192-196` | 時間的 Module；事件改游標 |
| `ScheduleOptions::runner`、`Affinity` 的啟動比對、`kMainRole`；H-2／D-E13、X-3／D-E10、X-E2、X-E3 | DI-KS 14 | runner 安排執行緒；Resource 自己斷言（3.1） |
| `Zone`、`WritePolicy`、`Role::Input`、`Rollback`、`external`、`ingress`、`ingressStage`、`rollbackEnabled`、`notInRollback`、`rollbackSafe`、`DigestSet`、`RequireExplicitZones`、`parallelGuardTest`；D-E2、D-E7 的區規則部分（含 S-17 `:1335`）、D-E9（`:1510`）、D-E11 與 X-E1（`:1423`）、D-E15、D-E16、Z3、digest set 的 R-E4；`CFingerprint`、`ExportZoneCSources`；記錄的 `emitterZone`（`detail/Context.hpp:90`）與 Z5 複核 | `Declarations.hpp`、`Registry.*`、`Fingerprint.cpp`、`World.cpp`、`Executor.cpp` | 權威 Module（內容與執行期的處置在權威宣告重新評估；第 6 節列各條的處置選項） |
| `RequireExplicitOrder`、D-E8 | `Registry.hpp:255`、`Scheduler.cpp` | **拿掉**（`no-ambiguity-detection`）；`TieBreaks` 留 |
| `effects`／`Registrar::Effect` | `SystemOptions::effects` | 留 Kernel（同一個 effect 的兩個 System 是衝突邊，是排程資訊）；「(c) 區不得宣告 Io effect」（S-7）移權威 |
| `ResourceOptions::affinity`、`UnreadableResource` | — | 改義為「綁執行緒的 Resource」（3.1）；`ExclusiveWorld` 讀它的檢查改成存取時斷言 |

### 3.3 Kernel 要對外多交出的讀取面（新；第 9 節第 2 件）

權威 Module 與編排層在 Kernel 之外做檢查，現在的 `FrozenRegistry` 不夠（修訂 0 寫「已經交出」是錯的）：缺 hooks（觸發、元件、存取）、`InitialValue`（Target、Request、宣告者）、schedule 的選項與 stage、Plan 的順序圖（D-E16 的 Derived 檢查用到，`Scheduler.cpp:503-521`）、每個 System 的 owner 與登記位置（G-5 匯出要）。要交出的只是**唯讀的登記資訊**，Kernel 不解讀它們的意義。

### 3.4 指紋

Registry 指紋現在雜湊 zone、writePolicy、role、rollback、external、affinity、domain、startup、clock、runner、eventTrim、ingress、effects（`Fingerprint.cpp:12-18`、`:149-198`）；Plan 指紋含 domain 與 EventTrim 步（`:203`）。移出之後**格式改版、所有 golden 重產**（J-18、J-20 是「改」，第 6 節）。C 指紋移權威 Module，靠 3.3 的讀取面計算。

---

## 4. Architecture Delta（AGENTS §3 的七項）

1. **需要它的 Feature 或目的**：使用者的 Review 目的（「Kernel是機制，那組裝放在哪裡？」「Kernel其實就是個ECS」）；第 5.4 節的 Feature 表。
2. **現在的問題**：Kernel 混了 `bevy_ecs` 與 `bevy_app` 兩半（`inventory.md` 第 1、2 節：純 App 的有 `Plugin`、`InstallRunner`、清單檢查；混在一起的有 `Registry::Finish`、`Product`、`startup`、`Domain`、`runner`）；並疊加了四家都沒有放在核心的規則，單機也要承擔（兩個 System 寫血量就啟動失敗）。
3. **變化的邊界**：Kernel／編排層（新）／權威 Module（新）／時間；Kernel 的公開介面（`Plugin`、`Product`、`ScheduleOptions`、`ComponentOptions`、`ResourceOptions`、`EventOptions`、`SystemOptions`、`Mount`、`Registrar` 的部分函式、`FrozenRegistry`、`KernelErrorCode`、`StartupCode`、`AccessKind`、`Plan`）；Registry 與 Plan 指紋的格式。
4. **影響的 Module／Product／Tool**：`engine/kernel`；新 `engine/orchestration`；新權威 Module；`engine/time`（T1／T3）；`tests/common/kernel` 的 15 個測試檔 329 個案例與守衛腳本（`kernel_guard.py`、`naming_guard.py`、`kernel_symbols.py`、`store_stamp_guard.py`、`zone_c_export_check.py`、`zone_c_fixture/`、`compile_fail/`）；突變清單 `docs/architecture/plans/kernel/scripts/mutations.json`（558 個）；已核准、還沒開工的狀態機 (b) 區（不受影響）與 (c) 區的獨占套用（要重看）。gyo2 上沒有產品與工具。
5. **Dependency Direction**：新增 編排層→Kernel、各 Module→編排層（登記單位型別）、權威→Kernel（讀取面）；產品→編排層（取代產品直接建 `Registry`／`Product`）。Kernel 不依賴任何新層。
6. **Ownership**：登記單位的生命週期、runner、遊戲初始化、多個 World、執行緒安排、啟動錯誤的彙整：Kernel → 編排層。寫者、區、digest、回滾的規則：Kernel → 權威 Module。時鐘：Kernel → 時間。事件的保留：時鐘 → 游標（仍在 Kernel）。owner 都是架構（架構師會話）。
7. **為什麼不能更小**：比過「留在 Kernel 但預設關」（設計稿第 4 節），使用者選移出；「只改名與文件」不解決單機承擔規則的問題。搬動已合併的程式（K0 的開局排程約 30 行與 J-58；`Plugin`／`FinishContext`／清單檢查約 80 行；`StrictOrderTests` 8 案）是代價，列在第 7 節的估計。

---

## 5. 基盤 Architecture 的根據（AGENTS §2）

### 5.1 為什麼算基盤

上面已經積了、或排著要積的：狀態機 (b) 區（已核准，用 `ExclusiveWorld` 與 Module 登記）、時間 T1～T5、Loading 卡（生成世界）、還原入口卡（讀檔）、(c) 區的獨占套用、權威 Module。改 Kernel 的公開介面與 Plan 指紋之後，它們都要跟著改，越晚改越多。

### 5.2 予見的根據 → 基盤的要求

| 根據（出處） | 要求 |
|---|---|
| 單機不增加複雜度（使用者 11-14 `v8-arch-single-player-no-complexity`） | 不選權威時沒有寫者、區、digest 的規則 |
| PvP 的伺服器權威（Match／Client，11-11 起；11-16 只實作這一個策略） | 權威 Module 能讀到判定所需的全部登記資訊（3.3）；連線時比 C 指紋 |
| Client 三個 World（11-11）、`Product` 多個 World（11-14） | 多 World 在編排層；Kernel 只提供 `WorldExecutor` |
| 讀取畫面與分批載入（11-11） | 生成世界由編排層推動（Loading 卡），Kernel 不認得 |
| 作者自寫策略、格鬥遊戲的 P2P（11-16，只佔位） | 策略介面在權威 Module；Kernel 不為某個策略增加知識 |
| 狀態機的「進入初始狀態」要讀到設定（11-15 Bevy 的例子） | 遊戲初始化清單可插入 |
| Bevy 0.10 拿掉整個 World 綁執行緒（soundness #3310） | 執行緒的保證放在需要它的 Resource |

### 5.3 預算

本批不碰執行效率。事件游標的記憶體上限是預算項目，在游標的宣告寫。

### 5.4 Feature 與 Product

| Feature | Product | 誰點名 |
|---|---|---|
| 不選權威時沒有限制 | 單機 | 使用者 11-14、11-16 |
| 伺服器權威 | PvP Match／Client | 使用者 11-11、11-16 |
| 多個 World | PvP Client、Editor（文件 World＋渲染 World，11-11） | 使用者 11-11、11-14 |
| 遊戲初始化可插入 | 所有遊戲（狀態機的例子） | 使用者 11-16 |
| 讀取畫面與分批載入 | PvP Client（11-11）；原話沒有指定其他 Product | 使用者 11-11 |
| 作者自寫策略、P2P 回滾 | 格鬥遊戲（使用者舉例，沒有點名為要做的 Product） | 使用者 11-16 |

---

## 6. 守衛與測試的處置

### 6.1 原則

- **留**：判定不變。**搬**：判定照搬到新的 owner（照搬的部分不放寬；若因新家的能力不同而必須改，標「改」）。**改**：判定改變。**拿掉**：這個性質不再由機制守。
- 每一列寫：守的性質、處置、放寬的面、收緊的面、之後由誰守、根據與證據。
- 逐案清單：`build/evidence/gyo2-kernel-app-boundary/test-case-inventory.tsv`（329 個 TEST_CASE，每案標出碰到哪些移出的概念：zone 165、writer 54、startup 20、clock 48、thread 3、ambiguity 18、rollback 41、lifecycle 10、extract 5、fingerprint 57；一個都沒碰的 83）。「碰到」是關鍵字掃描，很多只是在準備資料時寫了 `Zone::B`；**判定變不變**在各實作批的事前宣告逐案判斷、逐條請使用者核准。
- 突變：558 個，碰到移出概念的約 176～201 個（依關鍵字式不同；`Registry.cpp` 77、`World.cpp` 27、`Fingerprint.cpp` 25、`Executor.cpp` 24 起）。跟著程式搬的突變一起搬；被拿掉的性質的突變退役並寫理由；各批照 D184 只跑相關突變、PR 線合併前完整跑一次。

### 6.2 依守衛

| 守衛（gyo2 現有） | 守的性質 | 處置 | 放寬的面 | 收緊的面 | 之後由誰守 |
|---|---|---|---|---|---|
| J-1～J-5、J-7～J-11、J-13、J-25、J-26、J-29、J-31、J-33、J-35、J-53 | 儲存順序、句柄、簽名、編譯失敗、條件、同步點、例外、驗證 | 留（逐案確認準備資料裡的 `Zone::` 等只是移除參數） | 無 | 無 | Kernel |
| J-12 | 簽名推出的存取集合 | 改：`CurrentTick`、`ExclusiveWorld` 的期望值跟著第 9 節第 3、5 件改 | 待定 | 待定 | Kernel |
| J-6 | Running 期間外部讀寫斷言；Ready 時外部寫 (c) 欄斷言（W-6b） | 前半留；後半移出 | Kernel 不再擋「第一次跑之後從外面寫 (c) 欄」 | 無 | 權威（執行期，第 6.3 節） |
| J-14、J-16、J-17、J-38～J-41、J-27、J-47 的 Z1 案 | 區規則、digest 禁 Shared、(c) 來源掃描、(c) Parallel 守衛 | 移出 | 不選權威時全部通過 | 無 | 權威（重新評估） |
| J-15 | 第二個寫者失敗；Startup 不計；Shared 兩寫者通過 | 移出 | 不選權威時兩寫者通過 | 無 | 權威 |
| J-18、J-20 | id 依登記順序；Plan 決定性；指紋跨平台 golden | 改：golden 重產（格式改版） | 舊 golden 不再比對 | 無 | Kernel |
| J-19 | 登記單位的重複、requires、凍結後登記、build／finish 順序、型別兩次、R-E4 | 拆：登記單位那半搬編排層；R-E2／R-E4 留 | 無 | 無 | 編排層／Kernel |
| J-21、J-23 | 衝突邊有先後；(b) 平手依 registrationSeq | 改：平手範圍擴大到所有衝突對 | 無 | (c) 也照登記順序 | Kernel |
| J-22 | (c) 歧義在 finish 失敗 | 拿掉 | 沒有先後的衝突對在 (c) 也通過 | 無 | 不由機制守；連線比 C 指紋只抓「兩邊程式不同」 |
| J-44 | (c) 登記順序 20 種排列：全部 D-E8，或結果都不變 | **拿掉**（修訂 0 寫「去掉一支」不成立：有歧義的排列在剩下那支必然失敗） | 「(c) 對登記順序不敏感」不再保證 | 無 | 不由機制守 |
| `StrictOrderTests.cpp`（8 案） | `RequireExplicitOrder` | 拿掉 | (b) 嚴格模式消失 | 無 | 不由機制守 |
| J-24 | 每個啟動錯誤碼一個最小 Registry | 拆：保留的碼留 Kernel；R-E1／R-E3／R-E6 搬編排層；D-E2、D-E7 區規則部分、D-E9、D-E11、X-E1、D-E15、D-E16、Z3、digest 的 R-E4 移權威；**刪**：D-E8、D-E10、D-E12、D-E13、D-E21、F-E1、X-E2、X-E3 | 刪的碼不再報 | 無 | 各新 owner |
| J-28 | `Role(Main)` 的 System 放錯執行緒，finish 失敗（gyo2 只有 finish 那半，`SchedulerTests.cpp:353`） | 拿掉；新增「綁執行緒的 Resource 被別的執行緒碰到就斷言」 | 啟動時不再擋 | 存取時斷言（新） | Kernel（Resource） |
| J-30 | stamp 序列與 tickStamps | 改：tickStamps 隨時鐘移出 | 待時間的宣告 | 無 | Kernel／時間 |
| J-32、J-32b | 結構變更的決定性；untyped `Commands` 動 (c)／Exclusive、(b) despawn 帶 (c) 欄回 Err（`ExecutorTests.cpp:296`） | J-32 留；J-32b 移出 | J-32b：Kernel 不再回 Err | 無 | 權威（執行期，6.3） |
| J-34 | hook 依登記順序、下一窗生效；(c) hook 衝突（D-E9） | 前半留；D-E9 案移出 | Kernel 不再擋 (c) hook 衝突 | 無 | Kernel／權威 |
| J-36 | 事件讀到的時機、隔兩 tick 讀不到、順序鍵 | 改（另一份宣告） | 待游標宣告 | 待游標宣告 | Kernel |
| J-37 | 事件跨 domain 失敗 | 拿掉 | 跨時鐘讀寫通過 | 無 | 游標保證不漏讀（另一份宣告） |
| J-45、J-57 的 Resource 與區的案 | `ExclusiveWorld` 的區與寫入限制 | 第 9 節第 1、3 件 | 待定 | 待定 | 待定 |
| J-50 | `KernelSettings` | 留（D-E18 案待第 9 節第 4 件） | 待定 | 無 | Kernel |
| J-52 | Z5 複核 | 移出 | Kernel 不再複核 | 無 | 權威（執行期，6.3） |
| J-54、J-55 | hook 清空輪、丟掉不能生成的那一筆 | 留；其中「上限在 C 指紋」「event trim」「startup」「record 的 tick 是 domain tick」「指紋版本」的案改 | 逐案在實作批列 | 逐案 | Kernel |
| J-56 | 巢狀執行 | 留；Z5 七案移權威；clock tick、內層 clock 修剪、extract、單一寫者 Resource 的驗證案改 | 逐案 | 逐案 | Kernel |
| J-58 | Startup 由 Kernel 跑 | 搬，改寫成遊戲初始化清單（O-1）；Kernel 的版本在減法批拿掉 | 「Startup 執行中跑別的」「一個 World 兩張」不再是錯誤 | 「整份一次、在所有 World 每幀之前」（O-1） | 編排層 |
| 沒有 J 編號的：`InitialValueTests`（45）、`SkeletonTests`（9，含「Exclusive 拒第二個寫者」）、Plan export（2） | 初始值的填入；漏斗的寫者；骨架 | 初始值的填入留；Z1／Z2／Z3／Z6 與漏斗寫者、C 指紋、zone C 匯出的案移權威；Skeleton 的寫者案移權威 | 不選權威時通過 | 無 | Kernel／權威 |
| `naming_guard.py`、`kernel_guard.py`、`kernel_symbols.py` | 命名、Kernel 目錄的掃描 | 改：掃描根加 `engine/orchestration` 與權威 Module | 無 | 掃描範圍變大 | 同 |
| `store_stamp_guard.py`、`zone_c_export_check.py`、`zone_c_fixture/` | stamp 寫入、zone C 匯出 | 前者留；後兩者移權威 | — | — | Kernel／權威 |

### 6.3 執行期的區邏輯：五條規則的去處（第 9 節第 1 件的材料）

| 規則（現在在 Kernel 的執行期） | 位置 | 「Kernel 提供中性的寫入檢查掛鉤，權威掛上去」能不能蓋到 | 「選了伺服器權威就禁止那幾種參數」能不能蓋到 | 「執行期的攔截拿掉」 |
|---|---|---|---|---|
| untyped `Commands` 只寫 zone B 的 Shared 欄（DI-KS 2、J-32b） | `World.cpp`、`Params.hpp` | 能（記錄當下） | 能（禁 untyped `Commands`） | 作者自律 |
| `ExclusiveWorld` 不寫 zone C 與 Exclusive（J-45） | `World.cpp:154-157`、`:983-987` | 能 | 能，但和狀態機（`ExclusiveWorld`）、(c) 區獨占套用衝突 | 作者自律 |
| `ExclusiveWorld` 寫單一寫者 Resource 要列出（DI-KS 23） | `Params.hpp` | 能 | 同上 | 作者自律 |
| Z5 複核：同窗 (b) despawn 碰 (c) insert（J-52；每筆記錄帶 `emitterZone`） | `World.cpp:586-629`、`detail/Context.hpp:90` | **不能**（發生在同步點套用時，不是寫入當下）；要另一個「套用前」掛鉤 | **不能**（`Despawn` 是一般參數，`World.cpp:356-359`） | 作者自律 |
| 第一次跑之後外部寫 (c) 欄（W-6b、J-6 後半） | `World.cpp:196-202` | 能（外部寫入也經同一路徑） | 不能（不是 System 參數） | 作者自律 |

掛鉤的選項和 `authority-own-registration` 的理由「Kernel 完全不動」、以及第 8.5 節草案的 Kernel RC「Kernel 又長出限制」相抵——掛鉤本身中性，但它是為一種策略加的執行期分支。

---

## 7. 批次、先後與模型

navigator 的上段（公開介面、Design Invariants），全部批次的實作者不用小模型。

| 批 | 內容 | 依賴 | 實作者／審查者 | 估計 |
|---|---|---|---|---|
| `v8-arch-gyo2-orchestration-skeleton` | 新 Module `engine/orchestration`；`Module`／`Plugin` 型別與生命週期、R-E1／R-E3／R-E6 搬出 Kernel；`Product` 搬上來、Kernel 的改名 `WorldExecutor`；遊戲初始化清單（O-1，**暫時**呼叫 Kernel 現有的 Startup 機制，J-58 留在 Kernel 到減法批；兩套並存的期間寫在 dev_log）；runner 介面與只跑一次的 runner；啟動錯誤的彙整（Kernel 的 `StartupReport` 加編排層自己的錯誤；Kernel 失敗時不跑後續檢查並寫明） | 本宣告核准；PR #240 合併 | Opus／high；Fable／high | 10～14 h |
| 事件游標（原 T4 改寫） | 另一份宣告 | 本宣告核准 | 另定 | 另定 |
| 時間（T1、T3 改寫：時鐘與 tick 在時間的 Module） | 另一份宣告；`Domain` 拿掉之前必須有它 | 本宣告核准；事件游標 | 另定 | 另定 |
| `v8-arch-gyo2-kernel-subtraction` ＋ 權威 Module（同一條 PR 線） | Kernel 拿掉開局排程、`Domain`、執行緒啟動檢查、歧義錯誤與 `RequireExplicitOrder`、區／寫者／digest／回滾的宣告與檢查；多交出讀取面（3.3）；指紋改版、golden 重產；綁執行緒 Resource 的斷言；守衛照第 6 節；權威 Module 照它自己的宣告 | 前三批；權威 Module 的宣告核准 | Opus／high；Fable／high；§14.4 的檢證者 Fable／high | Kernel 減法 14～20 h（含約 200 個突變的搬遷與退役、golden 重產）；權威另估 |
| 之後 | 多個 World（T5）與交換點（原 T2）在編排層；生成世界（Loading 卡） | 上面 | 各自的宣告 | — |

先後的理由：骨架先立，Module 才有地方接；事件游標要在 `Domain` 拿掉之前（現在的修剪靠時鐘）；時間要在 `Domain` 拿掉之前（`CurrentTick` 在 10 個測試檔用了 75 處）；Kernel 減法與權威同一條線，守衛不會有「從 Kernel 拿掉、權威還沒接上」的空窗。

時間設計 T2「Kernel：可呼叫的 runner、交換函式的登記」改成編排層；T4 改寫成事件游標；T5 在編排層。

---

## 8. 判定（各批的事前宣告再細化）

- L1 三平台（linux-x64、windows-x64、macos-arm64）綠。
- 第 6 節「留」的守衛判定不變；「搬」的在新家先紅後綠；「改」「拿掉」的已在該批的事前宣告逐條經使用者核准（§14.4 的五點，含收緊的面與證據位置）。
- 新增守衛的突變清單經別的模型敵對式檢查（§14.2）。
- 掃描（減法批之後）：`git grep -n -P "\b(Zone|WritePolicy|Domain|Rollback)::|\bRequireExplicit(Zones|Order)\b|\bemitterZone\b|\brollbackEnabled\b|\bnotInRollback\b|\bingressStage\b|\bDigestSet\b|\bCFingerprint\b|\bstartupRan_\b" -- engine/kernel` 0 命中。先在 cb6bccca 命中一次證明掃描有效（2026-11-17 實跑：135 行、終了碼 0；`-E` 在 macOS 的 git grep 不認 `\b`，同一式子 0 命中，所以用 `-P`）；用 `git grep` 的終了碼區分「0 命中」（1）與「失敗」（其他）。註解裡的「startup error」等一般用語不在式子裡。
- 單機的合成產品（不選權威）：兩個 System 寫同一個元件、跨時鐘讀事件（游標批之後）、沒寫先後的衝突對——都能啟動並照登記順序跑。

---

## 8.5 Design Invariants 與 Replacement Criteria（草案；核准後在實作批寫進各 owner 的頁）

### Kernel（`target/kernel.zh-Hant.md` 的改寫）

| DI-KS | 處置 | 改寫後的要旨 |
|---|---|---|
| 1 | 留；`ExclusiveWorld` 的 Contribution 待第 9 節第 3 件 | — |
| 2 | 前半留；後半移出 | 驗證開著時，經 World 的未宣告存取與 System 邊界的淨變化記為違規、以 Err 回傳；驗證是開發工具，不是規則 |
| 3、4 | 留 | — |
| 5 | 改寫 | 衝突與順序分開；沒有順序的衝突對照登記順序平手並列在 `TieBreaks`，不是錯誤；Kernel 不保證結果對登記順序不敏感 |
| 6、7、8 | 移出 → 權威 | — |
| 9 | 留；刪「(c) 區」措辭 | — |
| 10 | 改寫（事件游標的宣告） | 方向：讀者游標內不漏讀，Kernel 自己保證 |
| 11 | 留；「寫入計入獨占」「(c) 的 hook 衝突是啟動錯誤」移出 | hook 只有 OnAdd／OnRemove、同步執行、依登記順序 |
| 12 | 第 9 節第 7 件（回滾的機制放哪） | — |
| 13 | 留「Extract 參數唯讀、無寫回、只在 extract schedule」；來源 World 與時機移編排層 | — |
| 14 | 改寫 | 保留：Running 期間外部不碰 World（W-6）、World 的生命期是一個世代、世代切換＝重建 World。移出：一 World 一執行緒的啟動檢查（改成綁執行緒的 Resource 自己斷言）、每 domain 一 clock schedule |
| 15 | 改寫（取代 15 與 22 的例外） | Kernel 只做 ECS：不內建 schedule 或 stage 名；不認得開局、時鐘、區、權威、產品、執行緒角色；不擁有執行緒、不讀時鐘或亂數。啟動錯誤只有第 3.1 節列的那幾種（可檢查：`StartupCode` 的列舉值就是那份清單） |
| 16～18、20、21 | 留；刪去開局、區相關的措辭 | — |
| 19 | 留；刪 Z5 那一句（Z5 移出） | — |
| 22 | 移出 → 編排層 O-1 | — |
| 23 | 第 9 節第 3 件 | — |

Kernel 的 RC：RC 4、6、13、14、15 留；RC 3（觸發條件是「為消除 D-E8 加的 before／after」）隨 D-E8 拿掉；RC 5、10、11 隨規則移到權威的頁；RC 1 的 (c) 條件、RC 9、RC 12 改寫掉 (c) 與 J-17 的字樣（權威的對應條件寫在權威的頁）；RC 7 改寫為「**Kernel 又長出限制**：Kernel 為某一種產品或策略加了欄位、啟動檢查或執行期分支（例如重新認得開局、時鐘、區），同一版本兩次以上」；RC 8 留。「兩次以上」沒有根據，是提案者給的（第 9 節第 8 件）。

### 編排層（新頁 `target/v2/orchestration.zh-Hant.md`）

- **O-1 遊戲初始化**：一個 `Product` 一份遊戲初始化清單，行程啟動時跑一次、在所有 World 開始每幀之前跑完、跑在主 World；清單內的先後由作者與 Module 插入；編排層保證「整份一次、在前」，不保證清單內的先後。
- **O-2 登記單位**：依登記檔順序 build、再依同順序 finish；Module 在 Plugin 之前（同詞異義表，PR #240）；requires 只檢查、不重排；名字重複是啟動錯誤；一個 `Product` 至多一個 runner。
- **O-3 runner**：runner 擁有迴圈、決定哪條執行緒跑哪個 World；只呼叫 `WorldExecutor::RunSchedule` 與交換點，不排 System 的先後。
- **O-4 中性**：編排層不帶任何第三方能力，不認得產品、權威策略、時鐘的語意；schedule 名由作者與 Module 給。
- **O-5 啟動錯誤一次收齊**：Kernel 的啟動錯誤與編排層、各 Module 的 finish 錯誤彙整成一份回報；Kernel 失敗時，依賴 `FrozenRegistry` 的檢查不跑，回報寫明「因 Kernel 錯誤而未檢查」。
- RC：每加一種產品或 World 的組法都要改編排層的程式；runner 需要排 System 的先後才做得出某個 Feature；編排層開始 include 某個第三方或某個 Module 的內部。

### 權威 Module

條文在權威 Module 的宣告。本宣告只定它的位置（Module、作者選策略）、登記方式（自己登記並代為登記進 Kernel）、讀 Kernel 的方式（3.3 的讀取面）、檢查的時機（編排層 finish）、佔位策略在啟動時報錯——都是使用者 11-16 定的。

---

## 9. 還要使用者判的

1. **宣告的範圍**：這一份只定層與歸屬，權威 Module 的內容與事件游標各寫一份（卡片的提議，第 0 節）／全部寫在這一份。拆的代價：Kernel 減法要等權威宣告核准；好處：Loading 卡、T2、T3、T5 可以先接著做。
2. **Kernel 多交出讀取面**（3.3）：交出 hooks、`InitialValue`、schedule 選項、順序圖、owner 與登記位置（唯讀、不解讀）／不交，權威的檢查改成只依賴它自己登記的資訊（能檢查的變少）。需要它的 Feature：PvP 的伺服器權威。系譜：Bevy 的 `Schedules`、`ScheduleGraph` 都是公開可讀的；Unity 的 `World.Systems` 可列舉。
3. **`ExclusiveWorld` 的存取**：現在 Contribution 是「讀全部、寫 zone B 且 Shared 的全部」、驗證的已宣告集合也這樣定義。選項：「整個 World 的讀寫」（Bevy 的 `&mut World`；後果：選了伺服器權威時，獨占的 System 是每一欄的寫者——狀態機用 `ExclusiveWorld`，伺服器權威下會撞寫者規則）／保留 `Writes<…>` 的列出當存取宣告、Kernel 不按 zone 擋（權威策略拿它數寫者；系譜：K0 的 `exclusive-declared-writes`）。需要它的：狀態機 (b) 區、時間的固定步驅動、PvP。
4. **D-E18「System 不准宣告 `Res`／`ResMut<KernelSettings>`」**：留（理由：`KernelSettings` 是 Kernel 自己的、第一次 Run 時快取，執行中改它沒有效果）／拿掉（理由：它是疊加的規則；`kernel-is-ecs-no-restrictions` 列在第二類，範圍未選）。
5. **時鐘拿掉之後 Kernel 用的計數**：`RejectedSpawn`、診斷紀錄、事件的順序鍵現在帶 domain tick。選項：改用 Kernel 自己的 stamp 或 sync epoch（中性）／由時間的 Module 寫一個 Resource、Kernel 讀它（Kernel 依賴上層的概念）。`CurrentTick` 參數：拿掉，改成讀時間 Module 的 Resource（系譜 Bevy `Res<Time<Fixed>>`）。
6. **伺服器權威下「生成世界的階段不計寫者」的保證**：現在的豁免理由是 Kernel 保證開局只跑一次、最先、獨自跑。移出後，誰保證生成世界的那幾張 schedule 不和每幀的寫者交錯？選項：編排層保證「生成世界期間不跑每幀清單」（Loading 卡定形狀）／權威策略只在建 World 時讀一份「生成世界清單」並自己檢查／這個豁免在伺服器權威下取消（生成世界也算寫者，作者用漏斗或 `InitialValue`）。依賴 Loading 卡。
7. **回滾的機制放哪**（快照、Rewind／Replay、Restore；gyo2 還沒實作，DI-KS 12）：Kernel（World 的快照與還原是 ECS 的機制，策略只決定哪些資料進快照）／權威 Module／PvP。系譜：lightyear 在網路套件；flecs v3 的 snapshot 在 addon、v4 拿掉（理由沒找到）；Unity Netcode 在套件。需要它的：PvP Client 的預測與回滾、P2P 回滾（佔位）。
8. **新 RC 的「兩次以上」**：沒有根據，是提案者給的。照用／改成「一次就停下報告」／其他。
9. **權威 Module 的名字**：`engine/authority`（`GYO::Engine::Authority`）／其他。照 N17。
10. **兩層的讀法確認**：Loading 卡留下的「Engine 認得建世界的階段」與「ECS 不該有階段」——本宣告的讀法是兩層：Engine（編排層與權威 Module）認得生成世界的階段，Kernel 不認得。

交給另外兩份宣告、這裡不判的：伺服器權威到底開哪些檢查、6.3 的五條各選哪個（若第 9 節第 1 件選「拆」，6.3 的決定屬權威宣告）、未經權威登記的資料的預設區、跨時鐘事件的網路要求、游標的位置與上限。

---

## 10. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；時間估計是讀程式量估的。
- 第 6 節的處置是按守衛的說明與逐案清單的關鍵字掃描歸類的；每個測試案例的判定變不變，在各實作批逐案判斷。關鍵字掃描會多算（準備資料裡的 `Zone::B`），也可能少算（沒用到關鍵字卻依賴移出概念的案例）。
- 四家的做法來自子 agent 讀原始碼與文件，主會話抽查了一部分；Unity 讀非官方鏡像，Unreal 沒讀到原始碼。
- 已定的事的原話，有一部分是側邊會話與主會話轉達的；本頁照 HANDOFF（25515a80）與轉達的文字，沒有重讀原始對話。
- PR #240 未合併；O-2 的「Module 在 Plugin 之前」與定義引用它。三件未確認（產品能否只選部分 Module 等）由定義卡列給使用者；「產品選用權威 Module」以第一件為前提。
- 換模型檢查：修訂 0 做過（Fable／high）；修訂 1 尚未送檢查。

---

## 11. 修訂 0 檢查發現的處理

| 發現 | 處理 |
|---|---|
| 1 「留」的列含會變判定的守衛 | 6.2 拆開列；逐案清單 TSV；判定在實作批逐案 |
| 2 指紋與 FrozenRegistry | 3.3、3.4；第 9 節第 2 件 |
| 3 沒有 J 編號的測試與突變 | 6.1、6.2 末幾列 |
| 4 歧義自相矛盾 | `RequireExplicitOrder`、D-E8 改為拿掉；J-44 拿掉；DI-KS 5 改寫並寫明放寬；RC 3 隨 D-E8 拿掉 |
| 5 執行期的區邏輯 | 6.3 的矩陣；第 9 節第 1 件把它交權威宣告或在此判 |
| 6 代為登記蓋不到別的 Module 的資料 | 預設區交權威宣告（第 9 節末）；批次加時間的依賴 |
| 7 批次空窗 | 骨架批暫時並存並寫明；綁執行緒的斷言歸 Kernel、在減法批；豁免的保證列第 9 節第 6 件 |
| 8 引用與錯誤碼歸屬 | 3.1、3.2 照 `Registry.cpp` 的實際位置重列 |
| 9 和已定的決定不符 | 第 2.2 節逐條比對；D-E18 列第 9 節第 4 件；A-5 不再照搬（交權威宣告）；跨時鐘的網路要求交游標宣告 |
| 10 事件游標的缺口 | 交事件游標的宣告（第 0 節） |
| 11 掃描無效 | 第 8 節改用識別字的式子 |
| 12 §14.4 的形式 | 6.1 的欄位；開頭寫明核准本宣告不等於 §14.4 的核准；O-5 啟動錯誤彙整 |
| 13 DI 草案 | DI-KS 14、15、19 與 RC 1、9、12 改寫 |
| 14 §2、§4 | 第 5 節；第 3 節的系譜；第 9 節各件的 Feature 與系譜；圖的依賴方向改正 |
| 15 漏的待判 | 第 9 節第 4～8 件 |
| 16 估計偏低 | 第 7 節重估 |

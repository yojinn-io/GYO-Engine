# Kernel 與編排層的邊界：事前宣告（統合；修訂 13）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。核准本宣告不等於 AGENTS §14.4 的判定變更核准**：每一個守衛判定的變更，在實作那一批的事前宣告逐案列出、逐條核准。
> 批次代號：`v8-arch-gyo2-kernel-app-boundary`。推導與四家（Bevy、flecs、Unity Entities、Unreal Mass）的對照在[設計稿](kernel-app-boundary-draft.zh-Hant.md)。

**這份是三份宣告的統合**（使用者 2026-11-17，`v8-arch-gyo2-boundary-split-integrator-first`）：

> 「其實我想拆成3份，但是我怕有關聯的內容你會誤判。有這個可能性嗎」「就是併發3份然後有一個統合對麼。」

- 統合（本頁）先立：層與歸屬、**跨份的關聯表（第 1 節）**、會改 Kernel 公開介面的項目。
- 權威 Module、事件游標、回滾 Module（2026-11-17 加入）的宣告由主會話另開卡，**以本頁為準、不改本頁定過的事**；發現新的關聯回報本卡。
- 收口：三份一起送換模型檢查（本卡負責），專找「每份都說是別份負責」的縫，缺口補回本頁，再請使用者逐件定。

**修訂紀錄**：修訂 0、1 各送 Fable／high 敵對式檢查（16 條／12 必改；15 條／9 必改；證據 `declaration-r0-adversarial-check.md`、`declaration-r1-adversarial-check.md`）。修訂 1 在 git 歷史（d169e1f7）。修訂 2 改寫成統合，修訂 1 檢查的處理見第 12 節。修訂 3（2026-11-17）：L2 時鐘定案、新增 L20 Module 之間的依賴。修訂 4（同日）：L20 定案。修訂 5（同日）：L3 定案（拿掉執行期攔截）。修訂 6（同日）：L4 定案；7.3 補有型別／無型別的範圍；記通用考量 `engine-not-rigid`。修訂 7（同日）：L5 定案（生命週期、動態插入、每個 World 的宣告）。修訂 8（同日）：L6 定案（唯讀登記表、版本號）。修訂 9（同日）：L7 定案（`Writes<…>` 選填）。修訂 10（同日）：L9 定案（`KernelSettings` 准讀准寫、下一次 Run 生效）。修訂 11（同日）：L11 定案（回滾獨立 Module、策略）；新增 L21（策略的構造）；子卡加回滾份。修訂 12（同日）：L16 定案。修訂 13（同日）：Loading 卡已歸檔，「交給 Loading 卡」改成「生成世界之後另開一份宣告，放編排層」。

## 0. 依據

**讀的版本**：程式 gyo2 cb6bccca（`origin/gyo2` 到 2026-11-17 沒有新 commit）；HANDOFF `claude/gyo2-time-design` 25515a80 以後；Module／Plugin 定義 PR #240（`claude/gyo2-module-plugin-definition` b0ce94ed，**未合併**）；Bevy 6a911653f。證據：主 checkout `build/evidence/gyo2-kernel-app-boundary/`。

**新舊衝突的規則**（使用者：「以最新的為準，如果拿不準的去檢查一下定案的理由和時間」）：較新的為準；第 2.2 節逐條寫出較新的有沒有回應較舊的理由。

**文件的位置**：「v2 之前的 target 文件先不動，後面統一處理」（`v8-arch-gyo2-pre-v2-target-docs-later`）。新條文（編排層）放 `target/v2/`；Kernel 的 Design Invariants 仍在 `target/kernel.zh-Hant.md`（K0 於 11-16 更新過），改寫在實作批做；`runtime-composition`、`world`、`scheduler`、`system-contract`、`limits` 只列「會被影響」，不改。

---

## 1. 關聯表（三份的交界；兩張子卡開頭引用這張表）

欄位：**誰定**＝哪一份的宣告做決定；**誰等它**＝哪一份要以它為前提；**動到誰的介面**；**狀態**。

| # | 事項 | 誰定 | 誰等它 | 動到誰的介面 | 狀態 |
|---|---|---|---|---|---|
| L1 | 分層與歸屬（第 3 節） | 統合 | 權威、游標、回滾、生成世界、時間、狀態機 | Kernel、編排層 | 已定（第 2.1 節的決定） |
| L2 | **時鐘與 tick 移出 Kernel，到時間的 Module**（`Domain`、`ScheduleOptions::clock`、Kernel 推 tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、D-E12、F-E1） | 統合 | 游標（順序鍵、上限的單位不能用 tick）、權威（「tick 只有一個來源」由伺服器權威策略檢查）、時間 T1／T3、L8、L20 | Kernel 公開介面、時間 Module | **已定**（2026-11-17，`v8-arch-gyo2-clock-out-of-kernel`；第 10 節第 1 件） |
| L3 | **執行期的攔截**（第 7.3 節的六條） | 統合 | 權威（改靠 digest 比對與伺服器覆蓋抓錯，內容在 L12） | Kernel 公開介面（拿掉） | **已定**（2026-11-17，`v8-arch-gyo2-no-runtime-interception`）：拿掉執行期的攔截，Kernel 不加觀察點；伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯 |
| L4 | **生成世界時寫進去的初始值算不算寫者** | 統合 | 權威（寫者規則只數「修改」）、生成世界（不受綁） | 權威 | **已定**（2026-11-17，`v8-arch-gyo2-init-values-like-bevy`）：參照 Bevy——初始值在建立實體的當下給（`Spawn` 帶的值、`InitialValue`），生成世界就是一般流程裡的 schedule，不另設「寫入不算」的階段；伺服器權威只數修改既有值的寫者。「對已存在的實體插入元件」算建立還是修改，由權威份定 |
| L5 | **登記的生命週期與動態插入** | 統合 | 編排層骨架、權威（檢查時機；PvP 宣告不允許）、指令集（未來） | Kernel 公開介面（`Registry`、`WorldExecutor`） | **已定**（2026-11-17，`v8-arch-gyo2-dynamic-insert-per-world`）：Kernel 提供「開 Registrar」「建 Plan」兩步、可重複呼叫；編排層 build → 建 Plan → finish → 權威檢查；World 在 Ready 時可再登記或拿掉 System、下一次執行前重建受影響的 Plan（「凍結」只代表這一次執行的 Plan 已排好）；**每個 World 建立時宣告准不准動態插入**，預設允許、不保證指紋一致；不允許時插入被拒絕：回傳執行時錯誤、記警告、不退出、World 照跑；伺服器權威策略在啟動時要求它的 World 宣告不允許。這一批只定契約與介面，Plan 的重建先薄 |
| L6 | Kernel 交出的唯讀登記資訊（第 3.3 節） | 統合 | 權威、除錯工具、編輯器、指令集 | `FrozenRegistry` | **已定**（2026-11-17，`v8-arch-gyo2-read-only-registry-view`）：交出唯讀的表（補上 hooks、`InitialValue`、schedule 選項與 stage、順序圖），帶版本號、每次重建 Plan 加一；交出去的參照只在當前版本有效（或交複本）；修改一律走 `Registrar`（L5 的關卡） |
| L7 | `ExclusiveWorld` 的存取與 `Writes<…>` | 統合 | 權威（數寫者）、狀態機、時間（驅動固定步的 System）、指令集 | `Params.hpp`、驗證（`World.cpp`） | **已定**（2026-11-17，`v8-arch-gyo2-exclusive-writes-optional`）：隨 L3，`ExclusiveWorld` 能用閉包讀寫任何元件與 Resource 的值（不再分區、不再回 Err）；`Writes<…>` 留作**選填**的標註，Kernel 不解讀，伺服器權威策略可以拿它數寫者。留下的 Kernel 機制：閉包與回傳值不指回 World（DI-KS 21）、結構變更只經同步點（DI-KS 18）、唯一參數（S-10）。指令的插入照 L5 的宣告 |
| L8 | 不靠時鐘的計數（`RejectedSpawn`、診斷紀錄帶的 tick） | 統合 | 游標（事件順序鍵也不能用 tick） | Kernel | 隨 L2：改用 Kernel 自己的計數（stamp 或同步點序號；「Kernel 讀時間 Module 的 Resource」違反第 3 節的依賴方向）。用哪一個由實作批定，**卡片的推論，使用者沒判** |
| L9 | D-E18（System 不准宣告 `Res`／`ResMut<KernelSettings>`） | 統合 | — | Kernel | **已定**（2026-11-17，`v8-arch-gyo2-kernel-settings-read-write`）：拿掉 D-E18；System 准讀准寫 `KernelSettings`，**下一次最外層的 Run 才生效**（Kernel 每次最外層 Run 開始時重讀，取代「第一次 Run 時快取」）；`KernelSettings` 只放開發與調校的設定（驗證、出錯時停下、hook 清空輪上限）；L5 的開關不在這裡 |
| L10 | Registry 與 Plan 指紋改版、golden 重產（第 3.4 節） | 統合（改版這件事）；游標（Plan 的 `EventTrim`）、權威（C 指紋） | 三份 | 指紋格式 | 已定為後果；重產次數見第 8 節 |
| L11 | **回滾的機制放哪** | 統合（Kernel 的原語）；**回滾份**（回滾 Module 的設計） | 權威（伺服器權威策略選用某個回滾策略）、游標（事件進不進快照）、狀態機 | Kernel（原語）、回滾 Module、權威 Module | **已定**（2026-11-17，`v8-arch-gyo2-rollback-module-strategy`）：Kernel 只給原語（讀寫整欄、設 stamp、整批複製／替換之類，具體清單由回滾份回報統合定）；回滾是**獨立的 Module**，有自己的策略：`RollbackStrategy` interface＋官方實作，作者可以自己寫，也可以繼承官方實作改裝；第一版官方只實作 PvP Client 要的「狀態觸發、逐元件歷史、平滑修正」，P2P 的「輸入觸發、存整個狀態」佔位、選了啟動時報錯；伺服器權威策略依賴回滾 Module（登記檔，L20）。回滾的設計另開第四份宣告 |
| L12 | 伺服器權威檢查哪些、預設區、C 指紋與 G-5 匯出的內容、digest 比對與伺服器覆蓋、(c) 區獨占套用的新形狀、權威 Module 的名字；`AuthorityStrategy` interface 的形狀 | **權威** | 統合（缺口回報）、狀態機 (c) 區、回滾份 | 權威 Module | 待權威份；回滾的內容移到回滾份（L11） |
| L13 | 事件游標：游標放哪、上限與單位、丟棄記錄、不執行的讀者、順序鍵、跨時鐘事件的網路要求（11-14「也要兼容網絡部分」） | **游標** | 統合（L2、L11）、時間 | Kernel 的事件 API（`EventReader`、`EventWriter`、`World::EventPayloads`、Plan 的 `EventTrim`）——**只限這些；動到別的 Kernel 介面回報統合** | 待游標份 |
| L14 | 生成世界的形狀（每個 World 一次或分批、誰推動、什麼時候算好、讀取畫面） | **生成世界的宣告**（之後另開一份，放編排層；生成世界的宣告已在使用者要求下歸檔，L16 的答案記在 `recovered-from-stale-cards.zh-Hant.md` 第 2 節，分支 `claude/gyo2-time-design`） | 統合（L4、L16） | 編排層 | 等統合；L4 定了之後不再綁 Kernel 減法 |
| L15 | 產品能不能只選部分 Module（「產品選用權威」的前提） | 定義卡（PR #240） | 權威 | 建置層 | 待使用者 |
| L16 | 兩層的讀法：Engine（編排層）認得遊戲初始化與生成世界的階段、Kernel 不認得 | 統合 | 生成世界 | — | **已定**（2026-11-17，`v8-arch-gyo2-two-layers-like-bevy`）：照 Bevy——`bevy_app` 認得開局（`Main` 的開局清單，`bevy_app/src/main_schedule.rs:213-305`），`bevy_ecs` 不認得（搜不到 startup）。編排層認得、排流程；Kernel 只跑被叫的 schedule |
| L17 | 新 RC 的次數（「兩次以上」沒有根據） | 統合 | 權威、游標（它們的 RC 同理） | — | 待使用者（第 10 節第 10 件） |
| L18 | 狀態機 (b) 區（已核准）的影響：初始進入改由編排層的遊戲初始化保證；`ExclusiveWorld` 隨 L7；`RegisterStateMachine<S, Zone::C>` 的 `Zone` 隨 L12 | 統合列影響；狀態機卡處理 | 狀態機卡 | 狀態機 Module | 列出，不判 |
| L19 | 時間設計 T2（runner、交換函式的登記）、T5（多個 World）改在編排層；T4 改成游標份；T1／T3 的時間 Module 持有時鐘與 tick（L2） | 統合 | 時間設計 | 編排層、時間 Module | 已定（位置） |
| L20 | **Module 之間的依賴**（例：伺服器權威策略要檢查「tick 只有一個來源」，權威 Module 依賴時間 Module） | 統合（編排層讀登記檔、檢查）＋定義卡或建置層的批（登記檔的格式） | 權威、游標、時間、狀態機 | 編排層、建置層；Kernel 不參與 | **已定**（2026-11-17，`v8-arch-gyo2-module-deps-in-registers`）：依賴**只寫在 Module／Plugin 登記檔**的「依賴」欄（11-14 已定每列寫依賴、CMake 照它 link）；編排層從登記檔讀出依賴並在 build 時檢查（O-2）；**程式碼裡不另外寫 `Requires`**；不做策略層級的依賴，權威 Module 整個依賴時間 Module。登記檔給 CMake 與編排層兩邊讀的格式，在編排層骨架批或建置層的批定 |
| L21 | **策略的構造**（權威與回滾共用）：interface 是契約、保證不變；Engine 附官方實作，和作者寫的走同一個插口、沒有特權；作者可以從頭實作 interface（廢掉官方的），也可以繼承官方實作改裝——後者是作者的選擇，官方實作的內部不保證不變 | 統合 | 權威份、回滾份 | 權威 Module、回滾 Module | **已定**（2026-11-17，`v8-arch-gyo2-strategy-interface-impl`）；C++ 的寫法：虛函式的抽象基底類別（GYO 的 `Plugin` 已是這個寫法，`Registry.hpp:60-75`） |

**三張子卡的範圍**（一句話；回滾份 2026-11-17 加入）：
- **權威 Module**：照 L12 重新評估伺服器權威策略到底檢查什麼，並設計策略介面、權威 Module 的登記、佔位策略；以 L1～L7、L10、L11 的統合結論為前提。開頭引用 L1、L3、L4、L5、L6、L7、L10、L11、L12、L15、L17、L18、L20、L21。
- **回滾 Module**：照 L11、L21 設計 `RollbackStrategy` interface 與第一版官方實作（狀態觸發、逐元件歷史、平滑修正）、P2P 的佔位，以及需要 Kernel 提供的原語（回報統合定）；以 L1、L2、L5、L6、L10、L11、L13、L20、L21 為前提。開頭引用 L1、L2、L5、L6、L10、L11、L13、L17、L20、L21。
- **事件游標**：照 L13 設計讀者游標與修剪，「讀者在上限內不漏讀」由 Kernel 保證；以 L2、L8、L10、L11 為前提。開頭引用 L1、L2、L8、L10、L11、L13、L17。

---

## 2. 已定的事

### 2.1 這一批依據的決定（使用者）

| 代號 | 內容 | 原話（摘） |
|---|---|---|
| `v8-arch-gyo2-kernel-is-ecs-no-restrictions`（11-16） | Kernel 是 ECS，不疊加限制；先移出，再重新評估設計（範圍：第二、三類「未選」，先研究） | 「Kernel其實就是個ECS，本身就不應該迭代限制在裡面的。」「先移除出來。然後得重新評估設計。」 |
| `v8-arch-gyo2-orchestration-layer-name`（11-16） | 編排層，`engine/orchestration` | 「算了用這個吧，編排也是挺應景的。」 |
| `v8-arch-gyo2-access-verification-stays`（11-16） | 存取驗證留 Kernel（開發版）；DI-KS 2 後半隨權威移出 | 「1）照建議」 |
| `v8-arch-gyo2-event-trim-by-cursor`（11-16） | Kernel 照游標修剪事件，不看時鐘 | 「它讓『不漏讀』成為 Kernel 自己的保證，不必依賴編排層排得對。」 |
| `v8-arch-gyo2-kernel-world-exec-type`（11-16） | Kernel 的型別 `WorldExecutor`；`Product` 搬到編排層 | 「第二個，保留一個型別、另取名。」「照建議」 |
| `v8-arch-gyo2-module-plugin-owner-definition`（11-16） | Module＝自己維護、介面是自己的；Plugin＝第三方官方維護；權威檢查放 Module | 「權威檢查我想放Module，這個是底層能力。」 |
| `v8-arch-gyo2-module-plugin-definition`（11-16，PR #240 未合併） | 定義寫進 AGENTS §6（只放思想）與 v2；兩份登記檔與兩種型別不變、照新定義重分；Module 在 Plugin 前 | 三件未確認（L15 等） |
| `v8-arch-gyo2-authority-own-registration`（11-16） | 權威 Module 自己登記並代為登記進 Kernel；Kernel 不存標籤 | 「4.1 權威 Module 自己登記。」 |
| `v8-arch-gyo2-authority-is-game-design`（11-16） | 權威是遊戲設計的一部分；策略式 Module，作者選策略 | 「這個東西本身就是遊戲邏輯的一部分…」「讀法一」 |
| `v8-arch-gyo2-authority-strategy-mechanism`（11-16） | 策略介面＋檢查；作者可自寫；Engine 附的只實作伺服器權威，其他佔位、選了啟動時報錯 | 「這個核心是要建立程機制而不是寫死。」「啟動時報」 |
| `v8-arch-gyo2-game-init-list`（11-16） | 遊戲初始化（行程一次、主 World、清單可插入）與生成世界（生成世界的宣告）分開 | 「第一個World，內容有作者自己訂，Engine只提供機制」「對的。」 |
| `v8-arch-gyo2-thread-binding-by-runner`（11-16） | runner 決定執行緒；綁執行緒的 Resource 自己斷言；Kernel 的執行緒啟動檢查拿掉 | 「第二個，別人掉過的坑，我沒必要在跳一次。」 |
| `v8-arch-gyo2-no-ambiguity-detection`（11-16） | Kernel 照登記順序平手、不報錯；Kernel 與權威策略都不偵測歧義；伺服器權威連線時比 C 指紋 | 「偵測歧義其實是很麻煩的。我個人覺得沒必要。」「可以。」 |
| `v8-arch-gyo2-pre-v2-target-docs-later`（11-16） | v2 之前的 target 文件先不動 | 「之前就先不管了。後面統一處理」 |
| `v8-arch-gyo2-boundary-split-integrator-first`（11-17） | 拆三份、統合先立 | 見本頁開頭 |
| `v8-arch-gyo2-clock-out-of-kernel`（11-17） | 時鐘與 tick 移出 Kernel，到時間的 Module；作者可選；「tick 只有一個來源」隨權威 | 「照建議，並修改相關文檔。我記得tick的控制之前也是pvp的需求，這個明顯是遊戲需求入侵了Engine設計。剝離出去給其他Module實現最好，和權威一樣，作者可以選，不過能多Module是有依賴關係的，這個依賴需要處理。」 |
| `v8-arch-gyo2-module-deps-in-registers`（11-17） | Module 之間的依賴只寫在登記檔，編排層讀出並檢查；程式碼不寫 `Requires`；權威 Module 整個依賴時間 Module | 「其他家的這個依賴關係如何處理？」→（卡片查四家，建議只寫在登記檔）「這個照你建議。」 |
| `v8-arch-gyo2-no-runtime-interception`（11-17） | 拿掉執行期的攔截（第 7.3 節六條）；伺服器權威改靠 digest 比對與伺服器覆蓋抓錯 | 「別家如何決定的？」→（卡片查四家：觀察點只在結構變更或整格替換、沒有一家讓外部掛鉤拒絕寫入、網路權威靠伺服器覆蓋；看法改成拿掉）「拿掉執行期的攔截，伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。」 |
| `v8-arch-gyo2-init-values-like-bevy`（11-17） | 生成世界的初始值在建立當下給、不算寫者；不設豁免的階段 | 「這個可以參照Bevy。」 |
| `v8-arch-gyo2-engine-not-rigid`（11-17，通用考量） | 做 Engine 不把規則釘死：預設方便、不禁止別的做法，限制只放在作者自選的地方；先懂別家為何這樣做；很多開源都這樣做就不頭鐵；拿不準交使用者 | 「我們做的是給別人用的東西，如何什麼東西都釘死…我們的限制會把別人的idea卡死。」「如果很多開源項目都這麼幹，就不要頭鐵了…你拿不準的可以交給我，這個是度的問題。」 |
| `v8-arch-gyo2-dynamic-insert-per-world`（11-17） | 執行中可以插入 System；每個 World 宣告准不准（預設准、不保證指紋一致）；不准時拒絕、回傳錯誤、記警告、不退出 | 「那我先問一個問題，動態插入系統有什麼好處？用例子回答。」→「我明白了，這個就類似於指令模式，現在啟動的world沒有系統，但是動態插入可以讓我使用指令，動態加入這個機能，這個應該才是最核心的。未來我自己要做指令集。也是要考慮的。」→「我再問一個問題，GYO能走2條路嗎？我不要求作者一定不能動態插入，而是在World裡聲明，如果World的宣告可以，那就可以動態插入系統，但是，寫明不保證指紋一致性。如果宣告不行，那麼就不能動態插入，插入就拒絕，返回信息（不退出，但是System無法插入，日誌報警告）。」→（卡片複述並列四點建議：預設允許、宣告在建 World 時、拒絕回 Result＋警告、允許時指紋不保證一致）「對的。」 |
| `v8-arch-gyo2-read-only-registry-view`（11-17） | Kernel 交出唯讀的登記表、帶版本號；參照只在當前版本有效；修改只走登記 | 「交出是什麼意思，公開出去可以修改還是說，外部能讀到？」→「和剛才的system插入有關聯嗎？」→（卡片列三處關聯與整體形狀）「可以」 |
| `v8-arch-gyo2-exclusive-writes-optional`（11-17） | `ExclusiveWorld` 能寫任何值（隨 L3）；`Writes<…>` 留作選填標註 | 「現在的GYO呢？」→「這題，你和看下上面的決策，例如System可以動態插入那樣的（如果開啟的話）。」→「我看不懂你在說啥？不是在說World是否能讀寫的問題嗎？我讓你看上面的決策想問是否有關聯。」→（卡片列關聯：這一題大半已由 L3 定，只剩 `Writes<…>` 留不留）「嗯，可以留選填。」 |
| `v8-arch-gyo2-kernel-settings-read-write`（11-17） | System 准讀准寫 `KernelSettings`，下一次 Run 生效；拿掉 D-E18 | 「你的調查，只有半分，3家也是只讀不寫嗎？」→（卡片重查：三家都准寫，差在生效時機）「這個問題是不是涉及上面決策的開關？」→（卡片答 L5 的開關不能放進可寫的設定）「但是L5的開關說的是System的插入。但是這題說的是系統的寫入值。是一樣的嗎？」→（卡片：不一樣，L5 的開關本來就在建 World 時的選項）「這個照建議。」 |
| `v8-arch-gyo2-rollback-module-strategy`（11-17） | Kernel 只給原語；回滾是獨立的 Module、有策略（interface＋官方實作），第一版只實作狀態觸發，P2P 佔位；另開第四份宣告 | 「Kernel 只給原語，回滾放權威 Module 的策略，我在想回滾要不要和權威放一起。你的看法呢」→「我在繼續問問題，這樣可以找到本質性的問題。問題：回滾有策略嗎？」→「不是，我是說本質上如果回滾有策略，就不能單單有官方默認實現，而是相當於一個interface、有官方實現，作者也可以自己複寫。否則作者無法達到自己的目的」→「這2件事，照建議。」 |
| `v8-arch-gyo2-strategy-interface-impl`（11-17） | 權威與回滾的策略都是 interface＋官方實作；作者可自寫或繼承改裝（選擇，不是做法） | 「權威 Module 也是一樣的構造（作者可以複寫，或者繼承改裝），如果是java就是一個interface + imple，但是C++我不知道有沒有類似的方式。」→「繼承官方實作改裝是選擇，而不是做法，你可以繼承改裝，也可以廢棄掉官方做法，而用自己的實現接上。」 |
| `v8-arch-gyo2-two-layers-like-bevy`（11-17） | 編排層認得遊戲初始化與生成世界的階段、Kernel 不認得（照 Bevy 的 `bevy_app`／`bevy_ecs`） | 「用Bevy的方式走，」 |

### 2.2 被這一批改寫的較早決定

「回應」欄只寫使用者的話；卡片或側邊會話的說明另外標出。

| 較早的決定（日期） | 當時的內容與理由 | 較新的決定 | 較新的有沒有回應較舊的理由 | 本批的處置 |
|---|---|---|---|---|
| `v8-arch-kernel-exclusive-default`（11-11）、`v8-arch-gyo2-check-history-first` 第 3 點（11-14）、`v8-arch-gyo2-exclusive-default-already-decided`（11-15：「預設是單一寫者」，訂正紀錄） | 預設獨占、Shared 才可多寫；理由欄「權威判定，要可回滾、可比對」 | `kernel-is-ecs-no-restrictions`、`authority-is-game-design`（11-16） | 有：卡片 11-16 查了歷史（理念出自 PvP 的 Gateway、案例全是 PvP），使用者看過後說「權威判定落的太深了…本身就是遊戲邏輯的一部分」 | 寫者規則移出 Kernel，內容由權威份重新評估（L12） |
| `v8-arch-gyo2-zone-default-b`（11-14：`Zone`、`Role`、`Rollback`、`DigestSet`、`CFingerprint`、zone C 匯出全部留；`Zone` 不填視為 B；`RequireExplicitZones`；「B，先把骨架跑通」） | 讓單機與工具作者不用多寫 | `kernel-is-ecs-no-restrictions`（11-16） | 部分：11-14 的理由（單機不多寫）在移出後仍成立；「全部留在 Kernel」被「先移出」取代 | 移出 Kernel，內容由權威份重新評估 |
| `check-history-first` 第 2 點（11-14：`Domain` 四個值留著） | 「Loading 不是 PVP 特有的，而是遊戲都有。」 | `build-world-phase-is-mechanism`（11-16：「有沒有是作者決定的，而不是Engine」） | **只有 Loading**：使用者更正了 Loading 的理由；整個時鐘在 11-17 另定（`clock-out-of-kernel`） | `Domain` 整個移出 Kernel（L2） |
| `check-history-first` 第 5、6 點（11-14：`DigestSet` 不急；`Zone`／`Role`／`Rollback`／`CFingerprint` 照不照原核准的切） | 「digest 用不用先不急定。」 | `kernel-is-ecs-no-restrictions`（11-16） | 部分：使用者定「先移出」，沒有回答 digest 的去留 | 移出 Kernel；去留由權威份 |
| K.3 第 11 項（11-11：(c) 區的時間只來自 domain tick 與被記錄的 boundary 輸入，例 PvP 的 `HostClockSample`、輸入的到達時刻；`spec.zh-Hant.md:1219`、`:789`）、DI-KS 8、12「domain tick 只有一個來源、任何 System 不得寫、在快照內」 | tick 是 (c) 區（PvP 的權威與回滾）唯一的時間來源 | `clock-out-of-kernel`（11-17） | 有：「我記得tick的控制之前也是pvp的需求，這個明顯是遊戲需求入侵了Engine設計」 | tick 移到時間 Module；「(c) 區的時間只來自 tick」「tick 只有一個來源」歸伺服器權威策略（L12）；DI-KS 8、12 的這部分移出 |
| `v8-arch-gyo2-fixed-step-in-world`、`v8-arch-gyo2-time-plan-two`（11-14：固定步由 World 裡的獨占 System 驅動；`Product` 多個 World） | 「固定步這個我不想限制作者的遊戲邏輯」 | `clock-out-of-kernel`（11-17） | 不衝突：驅動固定步的 System 本來就在時間 Module；改的只是 tick 存在哪裡（Kernel 的 domain tick → 時間 Module 的 Resource） | 照舊；時間設計 T1／T3 照 L2 改寫 tick 的存放 |
| `v8-arch-gyo2-strict-order`（11-14，PR #228 已合併） | `RequireExplicitOrder`；使用者「合併」，五項沒有逐條核准 | `no-ambiguity-detection`（11-16） | 有：「偵測歧義其實是很麻煩的…普通程序無法判斷」 | 拿掉；`StrictOrderTests.cpp` 8 案與 11 條突變退役 |
| DI-KS 5「(c) 歧義是啟動錯誤」「(c) 區對登記順序不敏感」（11-13 核准） | 決定性區不得依登記順序決定結果 | `no-ambiguity-detection`（11-16） | 部分：使用者的原話是「偵測歧義其實是很麻煩的…」與對側邊會話建議的「可以。」；「登記順序相同時平手是決定性的、比指紋抓版本不同」是側邊會話的說明，不是使用者的話 | 平手是 Kernel 的決定性行為；「對登記順序不敏感」不再保證（第 7 節列為放寬的面） |
| K0：`startup-run-by-kernel`、`startup-before-any-schedule`（11-15，#239）；DI-KS 22 | Kernel 排第一、只跑一次、執行中不准跑別的；「系統必須的，就不讓作者碰」 | `game-init-list`（11-16） | 有：「Engine只提供機制」 | 遊戲初始化清單在編排層（可插入）；生成世界交 生成世界的宣告；寫者豁免改成「建立時的值不算寫者」（L4） |
| `v8-arch-gyo2-exclusive-declared-writes`、`shared-means-no-owner`（11-15）；DI-KS 23 | `ExclusiveWorld<Writes<R…>>` 列出單一寫者 Resource、列的算寫者；「要保證誰寫，別開shared」 | `kernel-is-ecs-no-restrictions`（11-16） | 沒有單獨回應 | `Writes<…>` 留作選填標註，「列的算寫者」由權威策略決定、Kernel 不再擋沒列的寫入（L7） |
| `v8-arch-gyo2-spawn-reject-zone-c-blocked-by-zone-b`（11-15） | (c) 區的生成被 (b) 擋住時照樣丟、明細帶記錄者的區（公開欄位 `RejectedSpawn::emitterZone`，`World.hpp:86`）；「之前的權威Match的設計就是為了避免這種情況的」 | `kernel-is-ecs-no-restrictions`（11-16） | 沒有單獨回應 | 丟掉那一筆的行為留 Kernel（DI-KS 9）；「明細帶區」隨區移出，權威份定替代 |
| `v8-arch-gyo2-zone-c-exclusive-apply`（11-15～16） | Kernel 加「(c) 區的獨占套用」 | `kernel-is-ecs-no-restrictions`（11-16） | 沒有直接回應 | 形狀由權威份重看（L12） |
| 狀態機宣告（11-16 核准，`state-machine-declaration-approved`） | Kernel 不改；初始進入在開局之後；(c) 區介面 `RegisterStateMachine<S, Zone::C>` | `game-init-list`（11-16） | 部分：「初始進入在開局之後」可維持或改成插進清單，由狀態機卡另定 | L18 |
| `v8-arch-gyo2-product-type-name`（11-14：Kernel 的型別叫 `Product`） | 「這個不叫 App 了，太模糊 直接用 Product」 | `kernel-world-exec-type`（11-16） | 有：使用者選「保留一個型別、另取名」，`Product` 的名字搬上去 | Kernel 的型別改名 `WorldExecutor` |
| `v8-arch-gyo2-cross-clock-events-open`（11-14） | 跨時鐘事件放開；「不光要考慮單機，也要兼容網絡部分」 | `event-trim-by-cursor`（11-16） | 部分：游標保證不漏讀回應了「放開」；「兼容網絡」沒有回應 | 游標份（L13） |
| DI-KS 3、規格 D-1、D-6（Plan 在 Finish 建一次、永不重建）、R-2（凍結後不能登記）（11-11～13 核准） | 決定性；網路的 wire id 依登記順序，「finish 後登錄不可再變（wire id 的前提）」（`docs/architecture/plans/kernel/networking.zh-Hant.md` 第 5 列） | `dynamic-insert-per-world`（11-17） | 有：使用者點名未來的指令集（「動態插入可以讓我使用指令，動態加入這個機能」），並用「每個 World 宣告」保住 PvP 的需要 | 改：Plan 在兩次執行之間可重建；凍結不再是永久；不允許的 World 照舊不變 |
| `v8-arch-gyo2-module-plugin-registers`（11-14） | 型別 `GYO::Kernel::Module`／`Plugin`、兩份登記檔、Module 在 Plugin 前 | `module-plugin-owner-definition`（11-16）、PR #240 | 有：判準換了，兩份登記檔與型別保留 | 型別搬到編排層（命名空間改） |
| runtime-composition DI 2（11-13：一個 Runtime 一個 runner）、DI 3／3a（11-11、11-13：世界內容只經 Loading；使用者 11-11 的理由「如果現在物件稀少可以算成是Startup，那麼未來東西越來越多呢」）、DI 4（一 World 一執行緒） | — | `game-init-list`、`build-world-phase-is-mechanism`、`thread-binding-by-runner`（11-16） | DI 2：照舊，改成一個 `Product` 一個 runner；DI 3／3a：11-11 的理由由 生成世界的宣告的「可分批」回應；DI 4：「別人掉過的坑」 | 文件不改（統一處理）；DI 3 交 生成世界的宣告；DI 4 改成存取時斷言 |

---

## 3. 分層與歸屬

```
產品（Game／Tool）          main：用哪些 Module／Plugin（產品登記檔）、跑起來
   │
   ├─ 權威 Module、時間、狀態機、audio、網路……（Module）   SDL、ImGui（Plugin）
   │        │ 以登記單位（編排層的 Module／Plugin 型別）接進來
   ▼        ▼
編排層 engine/orchestration   登記單位與生命週期、Product（World 群＋runner）、遊戲初始化清單、
   │                          每幀清單、交換點、extract 的時機、執行緒的安排、啟動錯誤的彙整
   ▼
Kernel engine/kernel          World、ECS 元件／Resource／事件、System 參數與存取推導、schedule、Plan、
                              WorldExecutor、同步點、hook、巢狀跑、存取驗證、登記資訊（FrozenRegistry）
```

依賴方向：箭頭朝下。各 Module 依賴編排層的登記單位型別與 Kernel（和 `bevy_time`、`bevy_state` 依賴 `bevy_app` 同形）；編排層只依賴 Kernel、不依賴任何第三方與任何 Module 的內部；Kernel 不依賴上面任何一層。歸屬照 Module／Plugin 定義（PR #240）。

**系譜**（AGENTS §4）：分層＝`bevy_ecs`／`bevy_app`（`bevy_app/Cargo.toml` 只依賴 `bevy_ecs` 等底層 crate）；權威放 Module＝使用者 11-16（Unity Netcode 是套件、lightyear／replicon 是第三方 crate）；`WorldExecutor` 保留為型別（退掉：併進 `World`，照 Bevy，要改 B-12、DI-KS 12 與「World＝資料」）。

### 3.1 Kernel 留下的

| 留下 | 現在的位置 | 改什麼 |
|---|---|---|
| World、ECS 元件、Resource、事件緩衝 | `World.hpp`、`detail/Storage.hpp` | 事件改游標（L13） |
| System 參數與「存取從簽名推出」 | `Params.hpp`、`SystemTraits.hpp`、`Access.hpp` | `CurrentTick`、`AccessKind::DomainTick` 拿掉（L2）；`ExclusiveWorld` 改成整個 World 的值的讀寫、`Writes<…>` 選填（L7） |
| 登記動作 | `Registrar` | 拿掉 zone、寫入政策、區與回滾的旗標、`DigestSet`、`RequireExplicitZones`、`RequireExplicitOrder`；登記單位的身分由編排層給（3.5） |
| schedule、Plan、Serial 執行、同步點、hook 清空輪 | `Plan.hpp`、`Scheduler.cpp`、`Executor.cpp` | Plan 的 `domain` 拿掉（L2）；`EventTrim` 步隨游標拿掉 |
| `WorldExecutor`（現在的 `Product` 改名） | `Product.hpp` | 拿掉開局的排程（K0） |
| 巢狀跑 schedule | `ExclusiveWorld::RunSchedule`（DI-KS 16～21） | 刪去開局、區、clock 相關的分支 |
| 存取驗證（開發版） | W-5、A.6 | `ExclusiveWorld` 的已宣告集合以「zone B 且 Shared」定義（`World.cpp:154-157`、`:983-987`），隨 L7 改寫 |
| `FrozenRegistry`、Registry 指紋 | `Registry.hpp:504-555`、`Fingerprint.cpp` | 讀取面（3.3）；指紋改版（3.4） |
| `KernelSettings` | `Registry.hpp:389-393` | 拿掉 D-E18；准讀准寫，下一次最外層 Run 生效（L9） |
| `InitialValue`、`SpawnWithId`／配發器、丟掉不能生成的那一筆 | — | 區規則的部分移出；計數待 L8 |
| Extract 參數（唯讀）、`ScheduleOptions::extract`、`ExtractNotNestable`、finish 時用來源 Registry 解 Extract 的 id（`Registry.cpp:343-355`）、D-E14（來源未設或未凍結，`:1060`） | — | 來源 World 與執行時機移編排層；`SetExtractSource` 留作 finish 的輸入 |
| 綁執行緒的 Resource 記來源執行緒、碰錯斷言（**新**，取代 H-2、X-3 的啟動檢查） | `ResourceOptions::affinity` 改義 | 系譜 Bevy `bevy_ecs/src/storage/non_send.rs:74-84` |

**Kernel 保留的啟動錯誤**：R-E2；R-E4（不含 digest set `Registry.cpp:487`）；R-E5；D-E1（不含 Z3 的 ingress stage `:909`；eventTrim `:925` 隨游標）；D-E3（沒有配發器，`:1047`、`:1118`）；D-E4、D-E5、D-E6；D-E7 的機制規則——H-6 Extract 只在 extract schedule（`:1127`、`:1376`）、S-8 的「Diagnostics 只能來自 `DiagnosticsWriter`」（`:1079`）、S-18 hook 不得用 `ExclusiveWorld<Writes<…>>`（`:1133`）；D-E14（`:1060`）；D-E17；D-E19；D-E20。D-E18 拿掉（L9）。D-E12、F-E1 拿掉（L2）。
**Kernel 保留的執行期錯誤**（`KernelErrorCode`）：驗證、巢狀、hook、配發器、結構、Extract 相關的保留；`UntypedWrite`、`ZoneCDespawn`、`ExclusiveWorldWrite` 拿掉（L3）；`UndeclaredResourceWrite` 拿掉（L7）；`StartupNotNestable`、`StartupRunning` 拿掉。

### 3.2 移出 Kernel 的（去處）

| 移出 | 現在 | 去處 |
|---|---|---|
| 登記單位 `Plugin`／`Requires`／`Finish`／`InstallRunner`；R-E1、R-E3、R-E6 | `Registry.hpp:41-75`、`Registry.cpp:646-724`、`:1520` | 編排層（L5）；`Requires()` 拿掉，依賴改從登記檔讀（L20） |
| `Product` 這個名字 | `Product.hpp` | 編排層 |
| 開局的排程（K0）、`ScheduleOptions::startup`（`Declarations.hpp:138`）、D-E21、`StartupNotNestable`、`StartupRunning` | `Executor.cpp:151-170`、`:457-468`；DI-KS 22 | 遊戲初始化清單（編排層）；生成世界交 生成世界的宣告 |
| 開局的寫者豁免（`Registry.cpp:1448-1457`） | DI-KS 6 | L4 |
| `Domain`（含 `Loading`）、`ScheduleOptions::clock`、Kernel 推 tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、D-E12、F-E1；`EventOptions::domain` | `Declarations.hpp:56-62`、`Executor.cpp:192-196`、`World.hpp:453-461`、`Params.hpp:645` | 時間 Module（L2）；事件的歸屬改游標（L13） |
| `ScheduleOptions::runner`、`Affinity` 的啟動比對、`kMainRole`；H-2／D-E13、X-3／D-E10、X-E2、X-E3 | DI-KS 14 | runner 安排；Resource 自己斷言（3.1） |
| `Zone`、`WritePolicy`、`Role::Input`、`Rollback`、`external`、`ingress`、`ingressStage`、`rollbackEnabled`、`notInRollback`、`rollbackSafe`、`DigestSet`、`RequireExplicitZones`、`parallelGuardTest`；D-E2；D-E7 的區規則部分（含 S-6、S-7 `Local` `:1319`、S-8 的「(c) 不得宣告 Io effect」`:1313`、S-9、S-17 `:1335`）；D-E9（`:1510`）；D-E11 與 X-E1（`:1423`）；D-E15；D-E16；Z3；digest 的 R-E4；`CFingerprint`、`ExportZoneCSources`；記錄的 `emitterZone`（`detail/Context.hpp:90`）、`RejectedSpawn::emitterZone`（`World.hpp:86`） | `Declarations.hpp`、`Registry.*`、`Fingerprint.cpp`、`World.*`、`Executor.cpp` | 權威 Module（內容 L12；豁免 L4）；執行期的攔截拿掉（L3）：`emitterZone`、Z5 的呼叫當下與同步點複核、W-6b 都不再存在 |
| `RequireExplicitOrder`、D-E8 | `Registry.hpp:255`、`Scheduler.cpp` | **拿掉**；`TieBreaks` 留 |
| `effects`／`Registrar::Effect` | `SystemOptions::effects` | 留 Kernel（同 effect 的兩個 System 是衝突邊） |

### 3.3 Kernel 交出的唯讀登記資訊（L6；已定）

**已定**：唯讀（外部讀得到、不能從這裡改；修改只走 `Registrar`，L5 的「每個 World 宣告准不准插入」是唯一的關卡）；帶版本號，每次重建 Plan 加一，在下一次執行前更新；交出去的參照只在當前版本有效、版本一變就作廢（或交複本）。系譜：Bevy `Schedule::graph()` 回傳 `&ScheduleGraph`（`schedule.rs:677`）、`World::components()`（`world/mod.rs:259`）、`ComponentInfo::hooks()`（`component/info.rs:135`）；Unity `World.Systems` 是 `NoAllocReadOnlyCollection`（`World.cs:142`）；flecs 的 System 是實體、讀改同一份（不採：改動會繞過 L5 的關卡）；版本號的系譜 flecs `rebuild_count`（`pipeline.c:277`）、Bevy `graph.changed`（`schedule.rs:642`）。證據 `read-surface-other-engines.md`。


現在 `FrozenRegistry` 已有：元件、Resource、事件、System（含掛點、存取、`source`、`ownerPlugin`，`Registry.hpp:473-483`）、Plan（含 `DeclaredEdge`）、`TieBreaks`、具名集合、Registry 指紋。缺：hooks（觸發、元件、存取、登記者）、`InitialValue`（Target、Request、宣告者）、schedule 的選項與 stage、Plan 之外的順序圖（D-E16 的 Derived 檢查用到，`Scheduler.cpp:503-521`）。要交出的只是唯讀資訊，Kernel 不解讀它們的意義。C 指紋（`Fingerprint.cpp:331-406`）讀的正是這些。

### 3.4 指紋（L10）

Registry 指紋現在雜湊登記單位名單（`Fingerprint.cpp:11`）、zone、writePolicy、role、rollback、external、affinity、domain、startup、clock、runner、eventTrim、ingress、effects（`:12-18`、`:149-198`）；Plan 指紋含 domain 與 EventTrim 步（`:203`）。移出之後**格式改版、所有 golden 重產**。C 指紋移權威 Module，用 3.3 的讀取面計算。登記單位名單改由編排層給（3.5）。

### 3.5 登記單位的身分

Kernel 不再認得 `Plugin`，但 `InitialValue` 的「只有擁有者能宣告」（`Registry.cpp:850`）、錯誤訊息的 `OwnerName()`、`ColumnInfo::ownerPlugin` 等需要「是誰登記的」。做法：編排層開 `Registrar` 時給一個 owner（名字與序號）；Kernel 只存、只比相等、只拿來印，不解讀。欄位名 `ownerPlugin` 改成中性的 `owner`（N5）。

---

## 4. Architecture Delta（AGENTS §3 的七項）

1. **需要它的 Feature 或目的**：使用者的 Review 目的（「Kernel是機制，那組裝放在哪裡？」「Kernel其實就是個ECS」）；第 5.4 節的 Feature 表。
2. **現在的問題**：Kernel 混了 `bevy_ecs` 與 `bevy_app` 兩半（`inventory.md` 第 1、2 節）；並疊加了四家都沒有放在核心的規則，單機也要承擔。
3. **變化的邊界**：Kernel／編排層（新）／權威 Module（新）／時間 Module；Kernel 的公開介面（`Plugin`、`Product`、`Registry`（L5）、`ScheduleOptions`、`ComponentOptions`、`ResourceOptions`、`EventOptions`、`SystemOptions`、`Mount`、`Registrar`、`FrozenRegistry`（L6）、`KernelErrorCode`、`StartupCode`、`AccessKind`、`Plan`、`RejectedSpawn`、`ColumnInfo` 等的 `ownerPlugin`）；Registry 與 Plan 指紋的格式。
4. **影響的 Module／Product／Tool**：`engine/kernel`；新 `engine/orchestration`；新權威 Module；`engine/time` 與時間 Module（L2）；狀態機 Module（已核准未實作，L18）；`tests/common/kernel` 的 15 個測試檔 329 個案例與守衛腳本（`kernel_guard.py`、`naming_guard.py`、`kernel_symbols.py`、`store_stamp_guard.py`、`zone_c_export_check.py`、`zone_c_fixture/`、`compile_fail/`）；突變清單（558 個，碰到移出概念的約 176～201 個）。gyo2 上沒有產品與工具。
5. **Dependency Direction**：新增 編排層→Kernel、各 Module→編排層（登記單位型別）、權威→Kernel（讀取面）；產品→編排層。Kernel 不依賴任何新層。
6. **Ownership**：登記單位的生命週期、runner、遊戲初始化、多個 World、執行緒安排、啟動錯誤的彙整：Kernel → 編排層。寫者、區、digest、回滾的規則：Kernel → 權威。時鐘與 tick：Kernel → 時間 Module。事件的保留：時鐘 → 游標（仍在 Kernel）。owner 都是架構（架構師會話）。
7. **為什麼不能更小**：比過「留在 Kernel 但預設關」（設計稿第 4 節），使用者選移出；「只改名與文件」不解決單機承擔規則的問題。搬動已合併程式（K0 的開局排程與 J-58；`Plugin`／清單檢查；`StrictOrderTests`）是代價，列在第 8 節。

---

## 5. 基盤 Architecture 的根據（AGENTS §2）

### 5.1 為什麼算基盤

上面已積或排著要積的：狀態機 (b) 區（已核准）、時間 T1～T5、生成世界的宣告、還原入口卡（讀檔）、(c) 區的獨占套用、權威 Module。Kernel 的公開介面與指紋一改，它們都要跟著改。

### 5.2 予見的根據 → 基盤的要求

| 根據（出處） | 要求 |
|---|---|
| 單機不增加複雜度（11-14 `v8-arch-single-player-no-complexity`） | 不選權威時沒有寫者、區、digest 的規則 |
| PvP 的伺服器權威（11-11 起；11-16 只實作這一個策略） | 權威讀得到判定所需的登記資訊（L6）；檢查的時機在 Plan 建好之後（L5）；連線時比 C 指紋 |
| Client 三個 World（11-11）、`Product` 多個 World（11-14） | 多 World 在編排層；Kernel 只提供 `WorldExecutor` |
| 讀取畫面與分批載入（11-11） | 生成世界由編排層推動（生成世界的宣告）；寫者豁免的保證（L4） |
| 作者自寫策略、格鬥遊戲的 P2P（11-16，只佔位） | Kernel 不為某個策略增加知識 |
| 狀態機的「進入初始狀態」要讀到設定（11-15 的 Bevy 例子） | 遊戲初始化清單可插入 |
| Bevy 0.10 拿掉整個 World 綁執行緒（soundness #3310） | 執行緒的保證放在需要它的 Resource |
| 未來的指令集（使用者 11-17 點名：「未來我自己要做指令集」） | World 在 Ready 時可再登記 System、重建 Plan；每個 World 宣告准不准 |

### 5.3 預算

本批不碰執行效率。事件游標的記憶體上限是預算項目，在游標份寫。

### 5.4 Feature 與 Product

| Feature | Product | 誰點名 |
|---|---|---|
| 不選權威時沒有限制 | 單機 | 使用者 11-14、11-16 |
| 伺服器權威 | PvP Match／Client | 使用者 11-11、11-16 |
| 多個 World | PvP Client、Editor（文件 World＋渲染 World，11-11） | 使用者 11-11、11-14 |
| 遊戲初始化可插入 | 所有遊戲（狀態機的例子） | 使用者 11-16 |
| 讀取畫面與分批載入 | PvP Client；原話沒有指定其他 Product | 使用者 11-11 |
| 作者自寫策略、P2P 回滾 | 格鬥遊戲（使用者舉例，沒有點名為要做的 Product） | 使用者 11-16 |
| 指令集（執行中用指令加上或拿掉機能） | 原話沒有指定 Product | 使用者 11-17 |

---

## 6. 先用遊戲講

**單機 2D 橫版。** 產品的 `main` 只寫「用這些 Module、跑起來」。編排層照 Module 登記檔把各 Module build，Kernel 凍結、建 Plan，各 Module finish；建出 `Product`（一個 World），在主 World 跑遊戲初始化清單一次（讀按鍵設定；狀態機可以把「進入主選單」插在它後面），然後 runner 每幀跑每幀的清單。「受傷」與「撿補血包」兩個 System 都直接寫主角的血量：Kernel 不攔，照宣告的先後，沒宣告就照登記順序。

**PvP 的 Match。** 產品另選權威 Module、策略選「伺服器權威」。作者寫 `authority.Component<Health>({...})`，權威 Module 記下自己的宣告，再替作者登記進 Kernel。Kernel 凍結、建好 Plan 之後，策略讀 Kernel 交出的登記資訊加上自己的登記做檢查，不過就啟動失敗。檢查哪些由權威份定；執行中怎麼攔由本頁 L3 定。

**PvP 的 Client。** 三個 World 在編排層的 `Product` 裡；runner 決定執行緒、交換點與 extract 的時機。Kernel 只提供 `WorldExecutor`、在它上面跑一張 schedule、讀另一個 World 的唯讀參數。

**格鬥遊戲（P2P 回滾）。** 作者選「P2P 回滾」——第一版只佔位，啟動時報「這個策略還沒實作」。

---

## 7. 守衛與測試的處置

### 7.1 原則

- **留**：判定不變。**搬**：判定一字不改地搬到新的 owner（沒有放寬面；有放寬面的一律標「改」）。**改**：判定改變。**拿掉**：這個性質不再由機制守。**移出**：性質交給權威份重新評估，判定不保證照搬（等同「改」，在權威份逐條核准）。
- 每一列寫：守的性質、處置、放寬的面、收緊的面、之後由誰守。根據與證據：守衛的說明在 `docs/architecture/plans/kernel/spec.zh-Hant.md` 第 1070～1135 行；逐案清單 `build/evidence/gyo2-kernel-app-boundary/test-case-inventory.tsv`（329 案；標籤 zone 165、writer 54、startup 20、clock 48、thread 3、ambiguity 18、rollback 41、lifecycle 10、extract 5、fingerprint 57；都沒碰的 83）。關鍵字掃描會多算（準備資料裡的 `Zone::B`），也可能少算。**每一案的判定變不變，在各實作批逐案判斷、逐條請使用者核准。**
- 突變：558 個，碰到移出概念的約 176～201 個（依關鍵字式不同）。跟著程式搬的突變一起搬；被拿掉的性質的突變退役並寫理由；各批照 D184 只跑相關突變、PR 線合併前完整跑一次。

### 7.2 依守衛

| 守衛（gyo2 現有） | 守的性質 | 處置 | 放寬的面 | 收緊的面 | 之後由誰守 |
|---|---|---|---|---|---|
| J-1～J-5、J-7～J-11、J-13、J-26、J-29、J-31、J-33、J-35、J-53 | 儲存順序、句柄、簽名、編譯失敗、條件、同步點、例外、驗證 | 留（逐案確認準備資料裡的 `Zone::` 等只是移除參數） | 無 | 無 | Kernel |
| J-12 | 簽名推出的存取集合 | 改：`CurrentTick` 拿掉（L2）、`ExclusiveWorld` 改成整個 World 的值的讀寫（L7） | 待定 | 待定 | Kernel |
| J-6 | Running 期間外部讀寫斷言；Ready 時外部寫 (c) 欄斷言（W-6b） | 前半留；後半拿掉（L3） | 第一次跑之後從外面寫 (c) 欄不再斷言 | 無 | Kernel；後半不由機制守（權威靠 digest 與伺服器覆蓋） |
| J-14、J-16、J-17、J-38～J-41、J-47 的 Z1 案 | 區規則、digest 禁 Shared、(c) 來源掃描 | 移出 | 不選權威時全部通過 | 無 | 權威 |
| J-15 | 第二個寫者失敗；Startup 不計；Shared 兩寫者通過 | 移出；「Startup 不計」改成「建立時的值不計」（L4） | 不選權威時兩寫者通過 | 無 | 權威 |
| J-18、J-20 | id 依登記順序；Plan 決定性；指紋跨平台 golden | 改：golden 重產（L10） | 舊 golden 不再比對 | 無 | Kernel |
| J-19 | 登記單位的重複、requires、凍結後登記、build／finish 順序、型別兩次、R-E4 | 拆：登記單位那半搬編排層（順序照 L5：build → 建 Plan → finish）；「凍結後登記」改成：World 宣告允許時可以、不允許時回傳錯誤並記警告；R-E2／R-E4 留 | 允許的 World 執行中可以登記 | 不允許的 World 從斷言改成回傳錯誤（不退出） | 編排層／Kernel |
| J-21、J-23 | 衝突邊有先後；(b) 平手依 registrationSeq | 改：平手範圍擴大到所有衝突對 | 無 | (c) 也照登記順序 | Kernel |
| J-22 | (c) 歧義在 finish 失敗；`SchedulerTests.cpp:250`、`:265` 守「哪些對算衝突」與 before／after 的效果 | 改：歧義失敗的案拿掉；「哪些對算衝突」「加 before 後的先後」的案留 | 沒有先後的衝突對在 (c) 也通過 | 無 | Kernel（衝突判定）；歧義不由機制守 |
| J-25 | With／Without 不減少衝突（斷言 `ErrorCount(DE8) == 1`，`SchedulerTests.cpp:313-320`） | 改：改用 `TieBreaks` 或衝突邊斷言 | 無（同一個性質換觀察方式） | 無 | Kernel |
| J-27 | (c) Parallel 無守衛 id 失敗；Parallel 未實作 D-E17（`SchedulerTests.cpp:324-350`） | 拆：D-E17 留；(c) 守衛 id 移出 | 不選權威時 (c) Parallel 的守衛 id 不查 | 無 | Kernel／權威 |
| J-44 | (c) 登記順序 20 種排列：全部 D-E8，或結果都不變 | 拿掉（有歧義的排列在剩下那支必然失敗） | 「(c) 對登記順序不敏感」不再保證 | 無 | 不由機制守 |
| `StrictOrderTests.cpp`（8 案） | `RequireExplicitOrder` | 拿掉 | (b) 嚴格模式消失 | 無 | 不由機制守 |
| J-24 | 每個啟動錯誤碼一個最小 Registry | 拆：保留的碼留；R-E1／R-E3／R-E6 搬編排層；區、寫者、digest、回滾的碼移權威；**刪**：D-E8、D-E10、D-E12、D-E13、D-E18、D-E21、X-E2、X-E3、F-E1 | 刪的碼不再報 | 無 | 各新 owner |
| J-28 | `Role(Main)` 的 System 放錯執行緒 finish 失敗（gyo2 只有這半，`SchedulerTests.cpp:353`） | 拿掉；新增「綁執行緒的 Resource 被別的執行緒碰到就斷言」 | 啟動時不再擋 | 存取時斷言（新） | Kernel（Resource） |
| J-30 | stamp 序列與 tickStamps | 改：stamp 序列留；tickStamps（tick 視角）移時間 Module 或拿掉，由時間的宣告定 | tick 視角的換算不再由 Kernel 保證 | 無 | Kernel／時間 |
| J-32 | 結構變更的決定性 | 留 | 無 | 無 | Kernel |
| J-32b | untyped `Commands` 動 (c)／Exclusive、(b) despawn 帶 (c) 欄回 Err（`ExecutorTests.cpp:296`） | 拿掉（L3） | 這些寫入不再回 Err | 無 | 不由機制守（權威靠 digest 與伺服器覆蓋） |
| J-34 | hook 依登記順序、下一窗生效；(c) hook 衝突 D-E9 | 前半留；D-E9 案移出 | Kernel 不再擋 (c) hook 衝突 | 無 | Kernel／權威 |
| J-36 | 事件讀到的時機、隔兩 tick 讀不到、順序鍵 | 改（游標份） | 待游標份 | 待游標份 | Kernel |
| J-37 | 事件跨 domain 失敗 | 拿掉（在游標批，和 F-E1 一起） | 跨時鐘讀寫通過 | 無 | 游標保證不漏讀 |
| J-45、J-57 的 Resource 與區的案 | `ExclusiveWorld` 的區與寫入限制 | 拿掉（L3、L7）；J-57 的閉包案留 | 獨占 System 寫 (c)／Exclusive、寫沒列出的單一寫者 Resource 都不再回 Err | 無 | 閉包：Kernel；寫入：不由機制守（權威可讀 `Writes<…>` 標註） |
| J-50 | `KernelSettings`：System 宣告 `Res` 是 D-E18；第一次 Run 後外部再寫斷言；第一次 Run 時才讀到 | 改（L9）：D-E18 案拿掉；「第一次 Run 後再寫斷言」拿掉；改成「每次最外層 Run 開始時讀到最新值、Run 中途改的值下一次 Run 才生效」 | System 可以讀寫；第一次 Run 後也能改 | 「下一次 Run 生效」成為守的性質 | Kernel |
| J-52 | Z5 複核 | 拿掉（L3） | 同窗 (b) despawn 碰 (c) insert 不再中斷 | 無 | 不由機制守 |
| J-54、J-55 | hook 清空輪、丟掉不能生成的那一筆 | 留；「上限在 C 指紋」移出；「event trim」隨游標；「startup」隨編排層；「record 的 tick」待 L8；「指紋版本」隨 L10 | 逐案在實作批列 | 逐案 | Kernel |
| J-56 | 巢狀執行 | 留；Z5 七案拿掉（L3）；clock tick 的案移時間 Module、內層 clock 修剪隨游標；extract 案留；單一寫者 Resource 的驗證案拿掉（L7） | 逐案 | 逐案 | Kernel |
| J-58 | Startup 由 Kernel 跑 | **改**：寫成遊戲初始化清單（O-1）搬到編排層；Kernel 的版本在減法批拿掉 | 「Startup 執行中跑別的」「一個 World 兩張」不再是錯誤 | 「整份一次、在所有 World 每幀之前」 | 編排層 |
| 沒有 J 編號：`InitialValueTests`（45）、`SkeletonTests`（9，含「Exclusive 拒第二個寫者」）、Plan export（2） | 初始值、漏斗寫者、骨架 | 初始值的填入留；Z1／Z2／Z3／Z6 與漏斗寫者、C 指紋、zone C 匯出的案移出；Skeleton 的寫者案移出 | 不選權威時通過 | 無 | Kernel／權威 |
| `naming_guard.py`、`kernel_guard.py`、`kernel_symbols.py` | 命名、Kernel 目錄的掃描 | 改：掃描根加 `engine/orchestration` 與權威 Module | 無 | 範圍變大 | 同 |
| `store_stamp_guard.py`；`zone_c_export_check.py`、`zone_c_fixture/` | stamp 寫入；zone C 匯出 | 前者留；後兩者移出 | — | — | Kernel／權威 |

### 7.3 執行期的六條規則（L3 的材料；**已定拿掉**，2026-11-17）

**範圍**（使用者 2026-11-17 問「假如作者寫的是正常業務呢，比如玩家使用技能、或者道具恢復HP，這應該如何算？」之後補）：拿掉的只是**沒有型別的路**。正常業務用有型別的參數，啟動時就看得出寫哪一欄，照舊由伺服器權威策略在啟動時檢查。

| 寫法 | 啟動時看得出寫哪一欄嗎 | 由什麼守 |
|---|---|---|
| `Write<Health>`、`EventWriter<Healed>`、`CommandsFor<Inserts<…>>`、`Spawn<Bundle<…>>` 等有型別的參數（技能補血、道具回 HP、受傷都是這一種；多來源時走漏斗：各自發事件、唯一的寫者 `ApplyHealth` 套用合併規則，規格 B.3.1） | 看得出 | 伺服器權威策略在啟動時檢查（L12） |
| untyped `Commands`、`ExclusiveWorld` 的閉包、從外面寫（除錯工具、編輯器這類逃生口） | 看不出 | 下表的六條；已定拿掉，改靠 digest 比對與伺服器覆蓋 |

| 規則 | 位置 | 「Kernel 提供中性的檢查掛鉤，權威掛上去」 | 「選了伺服器權威就禁止那幾種參數」 | 「執行期的攔截拿掉」 |
|---|---|---|---|---|
| untyped `Commands` 只寫 zone B 的 Shared 欄（DI-KS 2、J-32b） | `World.cpp`、`Params.hpp` | 蓋得到（記錄當下） | 蓋得到（禁 untyped `Commands`） | 作者自律 |
| `ExclusiveWorld` 不寫 zone C 與 Exclusive（J-45） | `World.cpp:154-157`、`:983-987` | 蓋得到 | 蓋得到，但和狀態機、(c) 區獨占套用衝突 | 作者自律 |
| `ExclusiveWorld` 寫單一寫者 Resource 要列出（DI-KS 23） | `Params.hpp` | 蓋得到 | 同上 | 作者自律 |
| 呼叫當下的 Z5：(b) 的 `Despawn` 碰帶 (c) 欄的實體（`CheckDespawn`） | `World.cpp:355-365` | 蓋得到（呼叫當下） | 蓋不到（`Despawn` 是一般參數） | 作者自律 |
| 同步點的 Z5 複核：同窗 (b) despawn 碰 (c) insert（J-52；每筆記錄帶 `emitterZone`） | `World.cpp:586-629`、`detail/Context.hpp:90` | 蓋不到（發生在套用時）；要另一個「套用前」掛鉤 | 蓋不到 | 作者自律 |
| 第一次跑之後外部寫 (c) 欄（W-6b、J-6 後半） | `World.cpp:196-202` | 蓋得到（外部寫入走同一路徑） | 蓋不到（不是 System 參數） | 作者自律 |

掛鉤的選項和 `authority-own-registration` 的理由「Kernel 完全不動」、第 9.1 節的新 RC「Kernel 又長出限制」相抵——掛鉤本身中性，但它是為一種策略加的執行期分支。

---

## 8. 批次、先後與模型

navigator 的上段（公開介面、Design Invariants），實作者不用小模型。

| 批 | 內容 | 依賴 | 實作者／審查者 | 估計 |
|---|---|---|---|---|
| `v8-arch-gyo2-orchestration-skeleton` | 新 Module `engine/orchestration`；登記單位型別與生命週期、R-E1／R-E3／R-E6 搬出；`Registry::Finish` 拆段（L5）；`Product` 搬上來、`WorldExecutor` 改名；owner 身分（3.5）；遊戲初始化清單（O-1，**暫時**呼叫 Kernel 現有的 Startup 機制，J-58 與寫者豁免留在 Kernel 到減法批；兩套並存的期間寫在 dev_log）；runner 介面與只跑一次的 runner；啟動錯誤的彙整（O-5） | 本宣告核准；PR #240 合併 | Opus／high；Fable／high | 12～16 h |
| 事件游標 | 游標份的宣告；含 F-E1、J-37 的拿掉與 Plan 的 `EventTrim` | 本宣告核准；骨架 | 游標份定 | 游標份定 |
| 時間（T1、T3 改寫） | 時間設計：時間 Module 持有時鐘與 tick（L2）；`Domain` 拿掉之前必須有它 | 骨架（runner）；游標 | — | — |
| `v8-arch-gyo2-kernel-subtraction` ＋ 權威 Module ＋ 生成世界（同一條 PR 線） | Kernel 拿掉開局排程、執行緒啟動檢查、歧義錯誤與 `RequireExplicitOrder`、區／寫者／digest／回滾的宣告與檢查、Loading 列舉值；L2、L3、L7、L8、L9 照使用者的決定；讀取面（L6）；指紋改版、golden 重產；綁執行緒 Resource 的斷言；權威 Module 照它的宣告；J-15 的「Startup 不計」改成權威的「建立時的值不計」（L4），不再依賴生成世界的形狀 | 前面各批；權威份與 生成世界的宣告的宣告核准 | Opus／high；Fable／high；§14.4 檢證者 Fable／high | Kernel 減法 16～24 h（含約 200 個突變的搬遷與退役、golden 重產、讀取面、新守衛的突變檢查）；權威與生成世界另估 |
| 之後 | 多個 World（T5）與交換點（原 T2）在編排層 | 骨架 | 各自的宣告 | — |

先後的理由：骨架先立，Module 才有地方接、權威檢查才有凍結後的時機；游標在拿掉 Plan 的 `EventTrim` 之前；時間在拿掉時鐘之前（L2；`CurrentTick` 在 10 個測試檔用了 74 處）；Kernel 減法、權威、生成世界同一條線，守衛不會有「從 Kernel 拿掉、新家還沒接上」的空窗。golden 會重產兩次（游標批改 Plan、減法批改 Registry 與 Plan），已計入估計。

---

## 9. Design Invariants 與 Replacement Criteria（草案；核准後在實作批寫進各 owner 的頁）

### 9.1 Kernel（`target/kernel.zh-Hant.md` 的改寫）

| DI-KS | 處置 | 改寫後的要旨 |
|---|---|---|
| 1 | 改寫 | 描述從簽名推出；`ExclusiveWorld` 的 Contribution 是整個 World 的值的讀寫（L7），`Writes<…>` 是選填的標註、不改變它的存取 |
| 2 | 前半留；後半拿掉（L3） | 驗證開著時，經 World 的未宣告存取與 System 邊界的淨變化記為違規、以 Err 回傳；驗證是開發工具 |
| 3 | 改寫 | 順序只來自 id 與 Plan；Plan 在兩次執行之間可以重建（World 宣告允許動態插入時），同樣的登記得到同樣的 Plan |
| 4 | 留 | — |
| 5 | 改寫 | 衝突與順序分開；沒有順序的衝突對照登記順序平手並列在 `TieBreaks`，不是錯誤；Kernel 不保證結果對登記順序不敏感 |
| 6、7、8 | 移出 → 權威（8 的「domain tick 只有一個來源、System 不得寫」隨 L2 移伺服器權威策略） | — |
| 9 | 留；刪「(c) 區」措辭 | — |
| 10 | 改寫（游標份） | 讀者游標內不漏讀，Kernel 自己保證 |
| 11 | 留；「寫入計入獨占」「(c) 的 hook 衝突」移出 | hook 只有 OnAdd／OnRemove、同步執行、依登記順序 |
| 12 | 改寫（L11） | Kernel 提供回滾用的原語（清單由回滾份定），不提供 Rewind／Replay／Restore 與「哪些資料要回滾」的判斷；回滾在回滾 Module |
| 13 | 留「Extract 參數唯讀、無寫回、只在 extract schedule」；來源 World 與時機移編排層 | — |
| 14 | 改寫 | 保留：Running 期間外部不碰 World（W-6）；同一個 World 同一時間只被一條執行緒跑（`WorldExecutor::RunSchedule` 進入時斷言；系譜 Bevy 的 `&mut World`）；World 的生命期是一個世代，世代切換＝重建 World。移出：一 World 一執行緒角色的啟動檢查（改成綁執行緒的 Resource 自己斷言）；每 domain 一 clock schedule（L2，移時間 Module） |
| 15 | 改寫（取代 15 與 22 的例外） | Kernel 只做 ECS：不內建 schedule 或 stage 名；不認得開局、時鐘、區、權威、產品、執行緒角色；不擁有執行緒、不讀時鐘或亂數。啟動錯誤只有第 3.1 節列的那幾種：`StartupCode` 的列舉值等於那份清單（守衛：列舉值與清單比對的測試） |
| 16～18、20、21 | 留；刪去開局、區的措辭 | — |
| 19 | 留；Z5 那一句拿掉（L3） | — |
| 22 | 移出 → 編排層 O-1 | — |
| 23 | 移出 → 權威（`Writes<…>` 留作選填標註，L7） | — |
| 新 | 新增 | **Kernel 的設定下一次 Run 生效**：`KernelSettings` 只放開發與調校的設定，System 與外部都可以讀寫，每次最外層 Run 開始時重讀；會改變 World 規則的開關（例如 L5 的動態插入）放在建 World 時的選項、執行中不能改 |
| 新 | 新增 | **登記表唯讀、帶版本**：Kernel 交出的登記資訊是唯讀的、帶版本號；交出去的參照只在當前版本有效；修改只走 `Registrar` |
| 新 | 新增 | **動態插入照 World 的宣告**：World 建立時宣告准不准在 Ready 時再登記或拿掉 System；不准時，插入回傳執行時錯誤、記警告、不改 World；准時，下一次執行前重建受影響的 Plan，指紋跟著重算、不保證兩端一致 |

Kernel 的 RC：RC 4、5（D-8 誤報是排程的事，留 Kernel）、6、13、14、15 留；RC 3（觸發條件是 D-E8）拿掉；RC 10、11 隨規則移權威；RC 12（X-2 的守衛移權威）移權威；RC 1 的 (c) 條件、RC 9 改寫掉 (c) 與 J-17；RC 7「Host 又長回來」**保留原條件**、改寫主詞（舊的 Host 職責現在在編排層：「第一個真實能力接進機制時仍要改編排層的迴圈或 Kernel 本體；或 H.7 以外的協調邏輯在兩個以上產品中重複」），另**新增** RC「Kernel 又長出限制：Kernel 為某一種產品或策略加了欄位、啟動檢查或執行期分支（例如重新認得開局、區），次數見 L17」；RC 8 留。

### 9.2 編排層（新頁 `target/v2/orchestration.zh-Hant.md`）

- **O-1 遊戲初始化**：一個 `Product` 一份遊戲初始化清單，行程啟動時跑一次、在所有 World 開始每幀之前跑完、跑在主 World；清單內的先後由作者與 Module 插入；編排層保證「整份一次、在前」，不保證清單內的先後。守衛：J-58 改寫。
- **O-2 登記單位**：依登記檔順序 build → Kernel 建 Plan → 依同順序 finish → 選了的檢查（權威）（L5）；執行中的插入（指令等）照同一條路再走一次；Module 在 Plugin 之前（PR #240）；**依賴只從登記檔讀**（L20），缺依賴或依賴排在後面是啟動錯誤、不重排；名字重複是啟動錯誤；一個 `Product` 至多一個 runner。守衛：J-19 的登記單位那半（requires 的案改成讀登記檔）。
- **O-3 runner**：runner 擁有迴圈、決定哪條執行緒跑哪個 World；只呼叫 `WorldExecutor::RunSchedule` 與交換點，不排 System 的先後。守衛：掃描（`engine/orchestration` 不呼叫 System、不讀 Plan 的步驟）。
- **O-4 中性**：編排層不帶任何第三方能力，不認得產品、權威策略、時鐘的語意。守衛：依賴掃描（`engine/orchestration` 只 link Kernel 與 base）、`naming_guard`。
- **O-5 啟動錯誤一次收齊**：Kernel 的啟動錯誤與編排層、各 Module 的錯誤彙整成一份回報；Kernel 失敗時，依賴 `FrozenRegistry` 的檢查不跑，回報寫明「因 Kernel 錯誤而未檢查」。錯誤型別：編排層自己的報告型別包住 Kernel 的 `StartupReport` 與各 Module 的錯誤（Kernel 的 `StartupCode` 不擴充）。
- RC：每加一種產品或 World 的組法都要改編排層的程式；runner 需要排 System 的先後才做得出某個 Feature；編排層開始 include 某個第三方或某個 Module 的內部。次數見 L17。

### 9.3 權威 Module、事件游標

條文在各自的宣告。

---

## 10. 要使用者判的（會改 Kernel 公開介面，在統合定）

1. **時鐘與 tick**（L2）：**已定（2026-11-17）——移到時間的 Module**，作者可選；「tick 只有一個來源」歸伺服器權威策略。原話見第 2.1 節。比過的：留在 Kernel、只拿掉流程名字（現在的做法）／移到時間的 Module（建議；Bevy `bevy_time`、Unity 的 World 時間）。理由（使用者的）：「tick的控制之前也是pvp的需求，這個明顯是遊戲需求入侵了Engine設計」。查證：K.3 第 11 項（`spec.zh-Hant.md:1219`）的內容是 (c) 區（PvP 的權威與回滾）的時間來源。
2. **執行期的攔截**（L3）：**已定（2026-11-17）——拿掉**；伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。比過的：Kernel 提供中性觀察點、策略在觀察點拒絕（卡片原建議；四家的觀察點只到觀察，沒有拒絕的前例，Bevy 明寫每次寫入呼叫 hook 成本太高，`bevy_ecs/src/component/mod.rs:705-712`）／選了伺服器權威就禁那幾種參數（蓋不到三條）／拿掉（四家共同的做法；卡片查完後改建議這個）。理由：卡片的（四家的做法；不讓 Kernel 為策略長出分支）；使用者照說，沒有另外說。證據 `runtime-interception-other-engines.md`。後果：untyped `Commands`、`ExclusiveWorld`、`Despawn`、外部寫入對權威資料的錯誤不會當場被抓，要等 digest 比對或被伺服器覆蓋。
3. **生成世界時寫進去的初始值**（L4）：**已定（2026-11-17）——參照 Bevy**：初始值在建立實體的當下給，不設「寫入不算」的階段；伺服器權威只數修改既有值的寫者。比過的：建立時的值不算寫者（建議；Bevy required components `bevy_ecs/src/world/mod.rs:400-411`、場景在每幀的 `SpawnScene` 生成 `bevy_scene/src/lib.rs:98-104`；flecs prefab `docs/PrefabsManual.md:18-39`；Unity `EntityManager.Instantiate` `EntityManager.cs:3513`）／編排層保證生成期間不跑每幀、權威信任（K0 的延伸，無先例）／權威自己核對編排層的清單（無先例）／另一個 World 建好再搬（Unity 串流載入；要 Kernel 加跨 World 搬移）。理由：使用者「這個可以參照Bevy」。證據 `init-values-other-engines.md`。
4. **登記的生命週期與動態插入**（L5）：**已定（2026-11-17）**——見關聯表 L5 與第 2.1 節。比過的：生命週期——Kernel 兩步、編排層依序呼叫（建議）／照 Bevy 第一次跑才建 Plan；凍結——照舊永遠凍結／預設可改、PvP 鎖住（Bevy `schedule.rs:637-650` 圖有改動才重建；flecs `pipeline.c:269-277`；Unity 執行中可建 System）；開關——使用者提出「每個 World 宣告」（四家都沒有這種開關；Bevy 最接近的是 Finished 之後加 Plugin 就 panic，`bevy_app/src/app.rs:642-652`）。理由（使用者的）：未來的指令集；「我不要求作者一定不能動態插入，而是在World裡聲明」。卡片補的理由（使用者看過）：L3 拿掉執行期攔截後，權威策略擋不到執行中的插入，World 自己的宣告補上這個缺口。
5. **Kernel 交出的唯讀登記資訊**（L6）：**已定（2026-11-17）**——見第 3.3 節。比過的：交出唯讀的表（建議；三家都這樣）／不交（作者自寫的策略與工具看不到；和已定的「連線比 C 指紋」相抵）／讀改同一份（flecs）。理由：卡片的（三家的做法；不交與 C 指紋相抵；修改只走一條路才守得住 L5 的關卡）；使用者「可以」，沒有另外說。
6. **`ExclusiveWorld` 的存取**（L7）：**已定（2026-11-17）**——見關聯表 L7。和前面的關聯：大半由 L3 定（「只能寫一般區 Shared、其他回 Err」是 L3 拿掉的六條之一）；插入 System 照 L5；改登記只走 `Registrar`（L6）。只剩的一問：`Writes<…>` 留作選填（建議）／拿掉（完全照 Bevy）→ 留作選填。系譜：Bevy 獨占系統 `&mut World` 不列寫入（`multi_threaded.rs:173`、`:506`）、flecs `immediate`（`docs/Systems.md:1500-1512`）、Unity `SystemBase` 直接用 `EntityManager`；K0 的 `exclusive-declared-writes`（使用者 11-15「要保證誰寫，別開shared」）。理由：卡片的（不解讀的標註、不擋任何人，PvP 想查誰寫時可用）；使用者「嗯，可以留選填」。證據 `exclusive-access-other-engines.md`。
7. **D-E18**（L9）：**已定（2026-11-17）**——見關聯表 L9。比過的：准讀准寫、下一次 Run 生效（建議；Bevy `FallbackErrorHandler` 是 Resource，「schedule 正在跑時改，要等跑完才生效」`bevy_ecs/src/error/handler.rs:110-117`）／准讀、寫只在兩次執行之間（flecs `ecs_set_threads`「never while running a system or pipeline」`include/flecs/addons/pipeline.h:216-221`）／准讀不准寫（卡片前一版，依據不完整）／照舊不准（現在）。其他：Bevy `ScheduleBuildSettings` 隨時可設（`schedule.rs:556-576`）、`App::set_error_handler` 只能設一次（`bevy_app/src/app.rs:1471-1499`）；Unity `World.SetTime`（`World.cs:365-372`）。理由：卡片的（三家都准寫；開發中用指令打開驗證接上指令集）；使用者「這個照建議」。卡片第一次的調查只查了「讀」，使用者指出；也一度把 L5 的開關和這一題扯在一起，使用者指出不是同一件事。
8. **回滾的機制放哪**（L11）：**已定（2026-11-17）**——見關聯表 L11、L21。比過的：Kernel 只給原語、回滾放權威（卡片第一版）→ 回滾獨立成 Module（卡片第二版；lightyear 把 `lightyear_prediction` 與 `lightyear_replication` 分成兩個 crate，`crates/replication/prediction/Cargo.toml:2`、`:29`，決定性同步也依賴它，`crates/deterministic/deterministic_replication/Cargo.toml:16`、`:25`）／Kernel 提供完整的快照與還原（現在的 DI-KS 12；flecs v3 snapshot 在 addon、v4 拿掉）／放 PvP 產品。回滾有策略：lightyear `RollbackPolicy`（觸發來源 state／input，模式 Always／Check／Disabled，深度上限，`manager.rs:15-63`）與 `CorrectionPolicy`（`correction.rs:1034`）；GGPO、GekkoNet 由遊戲提供存讀回呼。理由（使用者的）：「如果回滾有策略，就不能單單有官方默認實現…否則作者無法達到自己的目的」。證據 `rollback-placement-other-engines.md`。
9. **兩層的讀法**（L16）：**已定（2026-11-17）——照 Bevy**：編排層認得、Kernel 不認得。使用者 11-16 的兩句（「世界生成階段（初始化）Engine不認識，那Engine如何排列流程？」「ECS，是不是不應該有（階段）這個概念」）分別指兩層。系譜：Bevy（`bevy_app` 的 `Main` 開局清單；`bevy_ecs` 零命中）、flecs（pipeline addon 的 `OnStart`；core 只預留 id）、Unity（Entities 沒有 World 級開局）。原話：「用Bevy的方式走，」
10. **新 RC 的次數**（L17）：「兩次以上」沒有根據，是提案者給的。照用／「一次就停下報告」／其他。
11. **Module 之間的依賴**（L20）：**已定（2026-11-17）**——依賴只寫在登記檔，編排層讀出並檢查；程式碼不寫 `Requires`；不做策略層級的依賴，權威 Module 整個依賴時間 Module。比過的：只寫登記檔（建議；系譜 O3DE `gem.json`＋`enabled_gems`、Unreal `.uplugin`）／Module 自己引入依賴（flecs `ecs_import`，可重入；代價：登記檔看不出實際用了什麼）／照舊程式碼寫 `Requires`、編排層檢查（登記檔與程式碼各一份，違反 AGENTS §9）。理由：卡片的（合 11-14「登記檔是為了看得到用了什麼」、同一事實一處）；使用者「這個照你建議」，沒有另外說。證據 `module-dependencies-other-engines.md`。

---

## 11. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；時間估計是讀程式量估的。
- 第 7 節的處置按守衛說明與逐案清單的關鍵字掃描歸類；每案的判定在各實作批逐案判斷。
- 四家的做法來自子 agent 讀原始碼與文件，主會話抽查了一部分；Unity 讀非官方鏡像，Unreal 沒讀到原始碼。
- 已定的事的原話，有一部分是側邊會話與主會話轉達的；本頁照 HANDOFF 與轉達的文字，沒有重讀原始對話。
- PR #240 未合併；O-2 的「Module 在 Plugin 之前」與定義引用它。
- 修訂 2 尚未送檢查（收口時與另外兩份一起送）。

### 附錄：減法批之後的掃描（判定之一）

`git grep -n -P "<式子>" -- engine/kernel` 0 命中；每個式子先在 cb6bccca 用已知例命中一次證明有效；用終了碼區分 0 命中（1）與失敗（其他）。macOS 的 git grep 加 `-E` 不認 `\b`（同一式子 0 命中），一律用 `-P`。式子：

- 一定拿掉的：`\b(Zone|WritePolicy|Rollback)::`、`\bRole::Input\b`、`\bRequireExplicit(Zones|Order)\b`、`\bemitterZone\b`、`\brollbackEnabled\b`、`\bnotInRollback\b`、`\brollbackSafe\b`、`\bingress(Stage)?\b`、`\bDigestSet\b`、`\bCFingerprint\b`、`\bExportZoneCSources\b`、`\bparallelGuardTest\b`、`\bstartupRan_\b`、`\bStartup(NotNestable|Running)\b`、`\bInstallRunner\b`、`\bFinishContext\b`、`\bkMainRole\b`、`\bDomain::Loading\b`、`\bownerPlugin\b`、`\bDE(8|10|13|18|21)\b`、`\bXE[23]\b`
- 時鐘（L2 已定）：`\bDomain::`、`\bclock\b`、`\bCurrentTick\b`、`\bDomainTick\b`、`\btickStamps_\b`、`\beventTrim\b|\bEventTrim\b`（後者在游標批）、`\bDE12\b`、`\bFE1\b`
- 執行期攔截（L3 已定）：`\bUntypedWrite\b`、`\bZoneCDespawn\b`、`\bExclusiveWorldWrite\b`、`\bCheckDespawn\b`（`World.cpp:355` 的 Z5 呼叫當下；`Despawn` 本身留，檢查函式拿掉）、`"Z5"`（規則字串，`World.cpp:360`、`:637`）
- `ExclusiveWorld` 的區限制（L7 已定）：`\bUndeclaredResourceWrite\b`
- 另一個判定：`StartupCode` 的列舉值等於第 3.1 節的清單（測試比對）。

---

## 12. 修訂 1 檢查發現的處理

修訂 0 的 16 條見修訂 1 的第 11 節（git 歷史 d169e1f7）；這裡列修訂 1 檢查的 15 條。

| 發現 | 處理 |
|---|---|
| 1 拆三份當成已定在寫 | 使用者 11-17 定了拆三份＋統合先立；本頁改寫成統合，第 1 節關聯表；會改 Kernel 介面的項目拉回本頁（第 10 節） |
| 2 時鐘移出沒有使用者的決定 | 改成 L2、第 10 節第 1 件；2.2 的 `Domain` 列改「只有 Loading」；補 K.3 第 11 項、DI-KS 8／12、`fixed-step-in-world`、`time-plan-two` |
| 3 2.2 漏列 | 補 `zone-default-b`、`product-type-name`、`spawn-reject-zone-c-blocked-by-zone-b`、狀態機宣告、`exclusive-default-already-decided`；runtime-composition 補理由 |
| 4 狀態機「不受影響」 | 改成 L18；第 4 節第 4 項列入 |
| 5 D-E14 與執行期錯誤沒歸屬 | 3.1 補 D-E14 與 `KernelErrorCode` 的去處；DI-KS 15 的守衛改成「列舉值等於清單」 |
| 6 生命週期做不到 | L5、第 10 節第 4 件；O-2 加凍結後的步驟 |
| 7 7.2 歸錯 | J-25、J-27、J-22 拆開；J-58 改標「改」；新增「移出」的定義 |
| 8 批次空窗 | 生成世界與減法、權威同一條線；時間依賴骨架；F-E1、J-37 在游標批；golden 重產兩次計入 |
| 9 掃描只蓋一部分 | 附錄補齊識別字，依 L2、L3、L7 分組；加列舉值比對 |
| 10 3.3、3.4 | 3.3 改正（`source`、`ownerPlugin` 已有）；「不交」與 C 指紋相抵寫進第 10 節第 5 件；3.4 補登記單位名單；3.5 owner 身分 |
| 11 6.3 少一條 | 7.3 補「呼叫當下的 Z5」 |
| 12 DI 與 RC | RC 5 留 Kernel；RC 7 保留原條件、改寫主詞，另立新 RC；RC 12 移權威；DI-KS 14 補「同一時間只被一條執行緒跑」；O-3、O-4 補守衛 |
| 13 第 10 節第 3、5 件的選項 | L7 的第二個選項寫明「哪些要列要另定」；S-18 標依賴 L7；修訂 1 的「計數」那件併入 L2 的代價與 L8 |
| 14 替使用者補理由 | 2.2 的 DI-KS 5、K0 兩列改成只寫使用者的話，側邊會話的說明另標 |
| 15 小錯 | S-7／S-8 更正；H-6 `:1127`；`CurrentTick` 74 處；估計重算 |

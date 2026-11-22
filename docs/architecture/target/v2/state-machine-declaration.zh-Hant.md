# 事前宣告：遊戲的狀態機（`v8-arch-gyo2-state-machine`，修訂 8）

更新：2026-11-22（修訂 8）。Owner：架構（GYO2 主會話開出的任務卡 gyo2-state-machine-r8）。**狀態：修訂 8，使用者 2026-11-22 動工前核准（「照建議，三件都核准，開始」）：第 2.1 節選 (d)、第 2.2 節選 (i)、第 2.3 節選 (A)、不另開 Kernel 的一批；第 2.4 節照判。下文「選 (a) 或 (d) 時」「選 (i) 時」的條件都照這個結果成立。之後使用者要求多家交叉比對（第 12 節），同日再定兩件（「照建議，1 保留，2 選 A＋，開始」）：「兩端同規則」回滾時重跑進入／離開照舊保留，進入／離開裡的副作用由作者用 `Resimulation` 擋；第 2.3 節改成 (A＋)——這一批照 (A)，另開一張「事件游標重評」的卡。**

**起點**：修訂 7（2026-11-16 使用者核准；分支 `claude/gyo2-state-machine-520d19`，commit 25c3949b，同名檔案）。核准之後，前提換了（Kernel 只做 ECS、另立編排層、權威與回滾是 Module，第 0 節）。批次表第 5 條要求重做成修訂 8：**只改建立在舊前提上的地方，遊戲看得到的行為照舊**（[回收清單](recovered-from-stale-cards.zh-Hant.md)第 1 節）。修訂 7 第 3 節的選項表、偵察與檢查的經過，以修訂 7 為準，這裡不重抄；本頁沿用修訂 7 的節號（第 5～10 節），改過的地方寫明。

**經過**：
- 修訂 0～7（2026-11-15～16）：見修訂 7 的〈經過〉。修訂 0、1、5、6 各送過一次 Fable／high 的檢查。
- 修訂 8 初稿（2026-11-22，作者 Opus 5.5）：照前提表逐項重看。期間 #270（交換點）合併、讀檔卡把「讀檔時狀態怎麼放回」交給這一批（經主會話轉達），都寫進來。
- 初稿送敵對式檢查（Fable 5.2／high，唯讀）：blocker 4、should 11、nit 13（`declaration-check-r8-fable.md`）。四個 blocker：事件題目建議的選項解決不了它舉的例子；子狀態的讀檔入口和 Bevy 不同、又碰到你定的規則，卻自己判了；「權威端換、Client 採用」沒有設計出來，`Change` 參數的出處掛錯；`RequestSilent` 的「拿掉」建立在一個不成立的前提上（Engine 的放回都有型別）。
- **修訂 8（這一份）**：照檢查報告全部改；改法對照在第 11 節。改完沒有再送檢查。

**寫法**：每件事標出處——【你定的】（附 HANDOFF 代號）、【已核准的文件】（附節號）、【程式】（附位置）、【我判的】（附依據）、【要你定】。

**證據**：這台 Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-state-machine-r8/`（git 不追蹤）：`declaration-check-r8-fable.md`。讀程式與 Bevy 原始碼的偵察沒有另存（未保存）。修訂 7 的偵察與檢查報告在 `build/evidence/gyo2-state-machine/`。別家的原始碼：`~/Code/GitHome/bevy`（commit 6a911653f）。

---

## 0. 前提表

修訂 7 寫在左邊那一欄的前提底下；「現在」那一欄是 2026-11-22 的 `gyo2`（eacab4d7，含 #270 交換點、#272 生成世界）。

| # | 前提 | 誰定的／出處 | 修訂 7 時 → 現在 | 掛在它上面的 |
|---|---|---|---|---|
| P1 | Kernel 只做 ECS、不疊加限制：沒有「每欄一個寫者」（D-E2）、沒有區（Z1～Z3）、沒有「獨占 System 只能是 (b) 區」（S-13） | 你（`v8-arch-gyo2-kernel-is-ecs-no-restrictions`）；第 1、4 批已拿掉（【程式】`KernelError.hpp:142-149` 的 `StartupCode` 只剩 DE1、DE3～DE7、DE14、DE17、DE19、DE20、RE2、RE4、RE5、RE7） | 成立 → **不成立** | 修訂 7 的 `RequestSilent` 的理由；DI 1、DI 7 的守衛；(c) 區要 Kernel 加「(c) 區的獨占套用」 |
| P2 | `ExclusiveWorld` 能讀寫任何元件與 Resource、能 `SetResource`；`Writes<…>` 是選填的標註，Kernel 不解讀 | 你（`v8-arch-gyo2-exclusive-writes-optional`）；【程式】`Params.hpp:939-1160` | 只能寫 (b) 區、要列寫入 → **全讀寫** | 套用的 System 的寫入；修訂 7 前提 9「放不進第一個值」 |
| P3 | 同一個型別登記兩次是 R-E2 | 【程式】`Registry.cpp:208` | 不變 | DI 7「登記兩次」那一半 |
| P4 | schedule 不帶時鐘、不帶 runner；「事件只能在帶時鐘的 schedule 讀寫」（F-9）拿掉 | 你（`v8-arch-gyo2-clock-out-of-kernel`）；【程式】`Declarations.hpp` 的 `ScheduleOptions` | 要選時鐘與 runner、不能讀寫事件 → **都不用** | 修訂 7 第 5.2 節第 5 列的參數；第 4 節第 1 點；第 3.11 節兩列的理由；第 2.3 節 |
| P5 | 編排層排流程：遊戲初始化清單（一個 `Product` 一次、在所有 World 的第一幀之前、跑在第一個 World）、每個 World 的每幀清單、預設骨架 `Main`（`First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）與 `FixedUpdate`；Kernel 不認得階段 | 你（`v8-arch-gyo2-game-init-list`、`-two-layers-like-bevy`、`-orchestration-gives-schedule-skeleton`）；[編排層](orchestration.zh-Hant.md) O-1、O-4；【程式】`ScheduleSkeleton.hpp` | 「開局」是 Kernel 的、T3 骨架沒定 → **編排層的清單與骨架** | 初始進入的接法（第 1 節第 3 項）；套用的 System 掛在哪（第 4 項） |
| P6 | 替換 World：照描述重新登記；準備函式的順序「帶過去 → 登記單位（產品自己的 `build` 加的在最後）→ 描述的 `prepare` → `ReplaceRequest.extra`」；準備放預設值不蓋掉已經在的值；遊戲初始化不重跑；`beforeSwap` 在準備之後 | 你（`-restore-reruns-registration`、`-carry-over-mechanism`）；O-7、O-8、O-10；【程式】`Setup.hpp` 的 `WorldSetup::Prepare`、`Product.hpp:77-83`、`Product.cpp:308-322`、`:381-403`、`:812`、`:841` | 沒有 → **有** | 第一個值誰放（第 5 項）；讀檔（第 6 節） |
| P7 | 生成世界：每個 World 建好就是生成中，每幀清單照跑；作者用 `WhileGenerating()`／`AfterGenerating()` 分開；一輪開始時是生成中、這一輪沒有人 `Hold()`，幀尾才結束生成 | 你（world-gen 2026-11-22 動工前與合併前核准）；O-14；【程式】`WorldGeneration.hpp`、`Product.cpp:619`、`:629-632`。「`Regenerate` 不加、作者寫 `WorldGeneration` 就能重新進入生成」是 world-gen 設計審查的看法（合併前核准第 4 節「記下、交之後的批的」），我照它判 | 沒有 → **有** | 初始進入和生成的先後、讀檔（第 6 節） |
| P8 | 權威是作者選的策略式 Module；狀態機與權威互不依賴；(c) 區的狀態機由作者標記：值用 `Mark`／`MarkResource`、套用的 System 用 `Runs`；`RegisterStateMachine<S, Zone::C>` 的 `Zone::C` 拿掉；「權威端換、Client 採用」不需要獨占套用，它是伺服器覆蓋 | 你（確認清單第 6 條，`v8-arch-gyo2-confirm-derived-removals`）；統合 L26；[權威份](authority-module-declaration.zh-Hant.md)第 5.8 節；[權威頁](authority.zh-Hant.md) DI-A 5、DI-A 7、DI-A 9 | Kernel 加「(c) 區的獨占套用」另開一批 → **作者標記** | (c) 區（第 1 節第 2 項、第 5.3 節） |
| P9 | 回滾是 Module；可回滾的步上寫的判定資料要登記成可回滾（AU-E6）；巢狀 schedule 由作者登記（`NestedSchedule(名字, 步的 schedule)`）；回滾的型別要可複製、有 `==`；回滾不認得狀態機 | 你（`-rollback-module-strategy`）；[回滾份](rollback-module-declaration.zh-Hant.md)第 5.4 節；R-3；【程式】`rollback/Registrations.hpp:279-286`、`:299` | `gyo2` 沒有回滾 → **有** | (c) 區「兩端同規則」的重跑 |
| P10 | Engine 的放回都有型別：`World::SetResource<T>`、`ExclusiveWorld::SetResource<T>`、回滾的 `RestoreSet::Resource<T>`、`ResourceHistory<T>`；網路 Module（伺服器覆蓋的搬運）還沒有 | 【程式】`Params.hpp:862-866`、`:905-921`、`World.hpp:262-270`、`rollback/History.hpp:174-216`；權威份第 5.7 節 | — | 第 2.1 節 |
| P11 | Module＝自己維護、介面是我們的；依賴只寫在登記檔 `config/modules.csv` | 你（`-module-plugin-owner-definition`、`-module-deps-in-registers`）；O-6 | Module 的判準兩套沒對齊 → **一句** | 位置（第 5.2 節） |
| P12 | 命名：目錄、namespace、target 一一對應，`naming_guard.py` 從目錄自動推 | 你（`v8-arch-gyo2-namespace-root`、`-naming-cleanup`）；[命名](rebuild-naming.zh-Hant.md) N13、N17 | 守衛的 `ROOTS` 只有 Kernel 一列 → **自動涵蓋** | 修訂 7 第 3.13 節「命名守衛加一列」 |
| P13 | 行為的測試照常先寫；綁實作方式的守衛（掃描、名字、結構）與守它們的突變等機制穩定再立；不為防誤用加守衛，守衛只守寫明的契約，誤用由每批的設計審查看 | 你（`v8-arch-gyo2-guards-after-mechanism-stable`）；「不為防誤用加守衛」見 world-gen 宣告第 11 節引的 `design-review-not-guards`（主會話 2026-11-21～22 轉達） | 修訂 7 用掃描守「不碰時間」 → **改由連結集與設計審查** | DI 1、DI 7、DI 8 的守衛；第 7 節的突變 |
| P14 | 事件游標：讀者被 run 條件跳過或所在 schedule 沒跑，游標停著、下次補讀（上限內）；跑過一次的讀者才拉住保留；每次最外層 Run 結束修剪到拉住的游標的最小值，沒有人拉住就全部修剪；碰到上限丟最舊、記一筆 `DroppedMessages`（列出拉住的讀者） | 你（`event-trim-by-cursor`）；「被跳過的讀者下次補讀」是游標宣告第 4.3 節由統合照 L17 推出的（[游標宣告](event-cursor-declaration.zh-Hant.md)第 2、3.2、4 節，核准）；【程式】`World.cpp:1042-1049`、`World.hpp:133-149` | 事件在進入／離開那張不能讀寫 → **能讀寫，而且讀者會拉住** | 第 2.3 節 |
| P15 | 異常只告訴外面發生了什麼；System 回 Err，World 中斷，`Product` 結束 | 你（`v8-arch-gyo2-error-levels`、`-broken-world-ends-product`）；O-11 | 不變 | 套用的 System 的錯誤 |
| P16 | 交換點與多個 World 的執行緒（O-12、O-13）；Product 一份的共用處（O-15） | 你（`v8-arch-gyo2-exchange`，#270 eacab4d7） | 沒有 → **有** | 狀態機不用：一個 World 一份；兩個 World 之間要傳「暫停」這類要求，是交換的資料（第 10 節） |
| P17 | 讀檔一律在新的 World；讀檔是生成世界裡作者的 System 或 `beforeSwap`；讀檔時狀態怎麼放回由狀態機這一批定，讀檔卡的說明頁指路到這裡 | 你（`-restore-entry` 的決定）；讀檔卡修訂 12 草案第 18 節 C-8（分支 `claude/gyo2-restore-impl-7e6bc6` 7970a9d6，經主會話 2026-11-22 轉達） | 沒有 → **有** | 第 2.2 節、第 6 節 |
| P18 | 遊戲看得到的行為（點名的 Feature、套用的順序、子狀態、第一次套用、名字……） | 你，回收清單第 1.1 節列的代號 | 照舊 | 第 3 節 |

修訂 7 前提 2、3（T3、Loading 由誰跑）由 P5、P7 取代；前提 9、10、11 由 P1、P2、P8、P9 取代；前提 13（`TimeControl`）不變，狀態機照樣不碰它。

---

## 1. 修訂 8 改了什麼（總表）

「別家」一欄引的是 Bevy（`bevy_state`），其他引擎在修訂 7 的第 3 節。

| # | 項目 | 修訂 7 | 前提變了什麼 | 別家 | 修訂 8 | 誰判 |
|---|---|---|---|---|---|---|
| 1 | `RequestSilent` | 要求分兩種；「直接蓋值做不出來」 | P1：理由三條裡兩條沒了；P8、P10：Client 採用改走伺服器覆蓋，但覆蓋要從值建出 `StateMachine<S>` | 換到別的值時沒有「不跑轉換」的要求（`resources.rs:198-210`；`set_if_different` 只在同值時不跑） | **建議和網路 Module 那一批一起定**，這一批不做 | 【要你定】第 2.1 節 |
| 2 | (c) 區 | `Zone::C` 用約束擋；等 Kernel 的「(c) 區獨占套用」；`Change` 參數下一批再問 | P8 | Bevy 的 States 不分等級 | 登記函式沒有等級參數（【已核准的文件】權威份 5.8）。**這一批只實作並驗「兩端同規則」**：作者 `Mark` 兩個 Resource（依型別，進 digest）、`Runs`、登記回滾（第 5.3 節）。「權威端換、Client 採用」——Client 端登記什麼、一致性指紋怎麼對、第一次套用與覆蓋的先後，以及你定過的 `Change` 參數（`-remaining-eight` 四，修訂 7 留到後一批再問）——和第 2.1 節一起留到網路 Module 那一批 | 【我判的】範圍照修訂 7 的切法；「依型別標記」：產品同時連兩個 Module，可以用型別（權威頁第 4 節：依名字的不進 digest） |
| 3 | 插進遊戲初始化清單 | 第一次套用在開局之後 | P5：遊戲初始化一個 `Product` 只跑一次、只在第一個 World | 把 `StateTransition` 插在開局清單的 `PreStartup` **之前**（`bevy_app/src/main_schedule.rs:16-24`、`bevy_state/src/app.rs:335-336`） | **不插**。初始進入照舊在套用的 System 第一次跑時（每個 World、每次替換）；O-1 保證它在遊戲初始化之後 | 【我判的】插進清單的話，替換後的 World 與第二個 World 永遠不會進初始值（`Product.cpp:572-582`）。「初始進入在開局之後」出自你定的 `-startup-before-any-schedule` 與 Feature 1（按鈕用開局放的設定生成），統合 L18 寫「初始進入改由編排層的遊戲初始化保證」；和 Bevy 不同的這一點不再問 |
| 4 | 套用的 System 的插法 | 「哪張 schedule、錨點、前或後、進入／離開那兩張的時鐘與 runner」，`InsertStage` 自己插一個 stage | P4、P5 | `insert_after(PreUpdate, StateTransition)`（`app.rs:335`）；`StateTransition` 不能帶條件 | 照舊自己插一個 stage `<狀態名>.Apply`；**預設錨點＝骨架 `Main` 的 `PreUpdate` 之後**；時鐘與 runner 參數拿掉；多一個選項：套用的 System 的 run 條件與 set（Bevy 沒有；理由是 GYO 有「生成中」，第 2.4 節） | 【我判的】 |
| 5 | 第一個值誰放 | 組裝的程式 `InstallStateMachine<S>(world[, 初始值])`；忘了、呼叫兩次是斷言 | P6 | `init_state`／`insert_state` 在登記當下放（`app.rs:96-152`） | **拿掉 `InstallStateMachine`**：登記函式加一個準備函式放第一個值（已經在就不蓋）。執行期的初始值（你定的「`init_state`／`insert_state` 那兩種」）改成 `StateMachine<S>::SetInitial(值)`：第一次套用之前，準備函式或 System 都能呼叫（第 5.2 節） | 【我判的】編排層的機制（O-7）；能拿掉的拿掉 |
| 6 | DI 1「現在的值只有這個 Module 改得了」 | 守衛：型別的 `requires` 測試、D-E2 | P1、P9、P10：回滾與 `CarryOver` 要公開的複製、搬移、`==` | `State::new` 是公開的（`resources.rs:64`），作者可以 `insert_resource` 蓋掉——修訂 7「照用並修補」補的洞 | 條文改成實際保證的：**沒有從 `S` 的值建構、進入之後沒有改成指定值的成員**；複製、搬移、比較公開。守衛留「型別不給改」（`requires` 測試）：它守的是你定的契約，不是誤用 | 【我判的】P13 |
| 7 | DI 7「同一種狀態只在一處套用」 | 守衛：R-E2、D-E2 | P1 | — | 條文留；「登記兩次」照舊由 R-E2 擋（組合的案例）；「作者另外登記套用的函式」是誤用，不守 | 【我判的】P13 |
| 8 | DI 8「狀態機不碰時間」 | include 掃描＋自我測試 | P13 | — | 行為測試的 target 只連 `gyo_state_machine`（與它連的 Kernel、編排層），編得過就表示標頭沒有用到時間；連結集釘住 `gyo_state_machine` 的連結；組合的案例另一個 target | 【我判的】專案現行的寫法（DI-A 9、R-3、WT-4）；樣板函式庫的標頭在使用者的 target 裡編，所以要靠只連它的那個 target |
| 9 | 命名守衛加一列 | 實作前列五點請你核准 | P12 | — | **不需要**：`engine/modules/state_machine` ↔ `GYO::Engine::StateMachine` 自動涵蓋 | 【我判的】 |
| 10 | 進入／離開那兩張裡的事件 | 不能讀寫（F-9） | P4、P14 | 「離開／進入時清掉某種訊息」（`state_scoped_events.rs:67-107`） | 能讀寫；讀者的保留有三個遊戲看得到的後果 | 【要你定】第 2.3 節 |
| 11 | 第 3.11 節「離開時清掉某種訊息」「轉換的事件」 | 不做；理由之一是事件不能在進入／離開裡讀寫 | P4 | 同上 | 照舊不做；理由只剩「沒有 Feature 需要」；前者跟著第 2.3 節的結果再看 | 【我判的】 |
| 12 | 子的特性對照的約束、`Zone::C` 的約束 | `requires` 約束，測試驗接不接受 | P8、P13 | — | `Zone::C` 那條拿掉。父子特性對照的約束留（寫錯的特性編不過），不立「接不接受」的測試與突變 | 【我判的】它擋的是寫錯的特性（誤用） |
| 13 | 讀檔 | 沒有 | P6、P7、P17 | 父：`insert_state` 直接用給的值當初始值（`app.rs:122-152`）；子：同一幀設 `NextState`，子直接進那個值（`sub_states.rs:166`、`state_set.rs:171`） | 父：`SetInitial`，第一次套用直接進存檔的值。子：【要你定】第 2.2 節 | 父【我判的】（照 Bevy；第 6 節）；子【要你定】 |

**沒有改的**：遊戲看得到的行為（第 3 節）；名字（`-state-machine-named-state-machine`）；狀態機是 Engine 的 Module（新定義下仍是：我們自己維護）；暫停要不要停時間由作者決定、狀態機不碰 `TimeControl`；兩棵互不相干的狀態樹之間的先後不當規則承諾。

---

## 2. 要你定的（三件）與我判的（第 2.4 節）

### 2.1 `RequestSilent` 還要不要

**用遊戲講**：聯機的天氣 `Weather`。伺服器在第 3600 個 tick 換成起霧；Client 收到快照，畫面要跟著起霧。另一個需求：多一個按鈕可以改天氣。

**修訂 7 為什麼有它**：Client 採用伺服器的值時，遊戲可以選「不跑離開與進入」（`-remaining-eight` 七：「這個也是看遊戲設計的」）。當時「直接蓋值」做不出來：型別封閉（你定的「照用並修補」）、每欄一個寫者（D-E2）、區規則（Z2）三條同時成立。你比過「Client 把快照的值當要求」「Module 開『依快照設值』的入口」「權威端換的狀態機 Client 不登記」「要求分兩種」，選了最後一個（`-silent-request-and-child-reset` 一，原話「1）可以。」；理由：「狀態機不能手動換，相當於遊戲邏輯必須完全依託於狀態機了。假如：我多了個遊戲需求，可以有個按鈕改變天氣呢？我難道還要去改狀態機不成？」），之後又定了「父 silent 換成同一個值時子不動」（`-state-machine-silent-parent-keeps-child`，「照建議」）。

**前提變了什麼、沒變什麼**：
- D-E2、Z2 沒了；型別封閉還在。作者照樣不能直接蓋值。
- 「權威端換、Client 採用」改叫**伺服器覆蓋**（權威份第 5.8 節；DI-A 7：規定在權威、搬運在網路 Module、重算在回滾 Module）。但 Engine 的放回都有型別（P10）：網路 Module 要把線上來的值放進 World，就得建出一個 `StateMachine<Weather>`——型別封閉之下只有狀態機 Module 建得出來。所以**不論選什麼，網路那一批都會回來要一條路**：「依快照設值」的入口，或 `RequestSilent`。
- 網路 Module 還沒有，`gyo2` 上現在沒有任何東西會用 silent。
- 按鈕改天氣：`Request(Rain)` 就換，不用改狀態機——下面每個選項都滿足。

| | (a) 拿掉 | (b) 留 | (c) 開放型別，讓作者直接蓋值 | (d) 這一批不做，和網路 Module 一起定（我的建議） |
|---|---|---|---|---|
| 這一批的程式 | 沒有 silent | 照修訂 7 做 silent | 公開「從值建構」 | 沒有 silent（同 (a)） |
| 網路那一批要什麼 | 「依快照設值」的入口——你當時比過、沒選的那一個；或把 silent 加回來 | 可能直接用 silent | 不用 | 那時照它的形狀定：silent、依快照設值、或別的 |
| 快照蓋掉值時「跑／不跑」離開與進入（`-remaining-eight` 七） | 跑：Client 不讓覆蓋碰狀態機的值、自己收值後 `Request`。不跑：等網路那一批的入口 | 跑：`Request`；不跑：`RequestSilent` | 作者自己蓋（不跑） | 同 (a)，等那一批 |
| 離開與進入不成對 | 覆蓋與回滾還原換掉值時不成對 | 同左，另加 silent 造成的 | 同左 | 同 (a) |
| 系譜 | Bevy（只有會跑轉換的要求） | GYO 自己加的 | Bevy 的 `State::new` 公開（`resources.rs:64`）——修訂 7 補掉的洞 | — |
| 推翻／受影響的你的決定 | 推翻 `-silent-request-and-child-reset` 一、`-silent-parent-keeps-child`；`-remaining-eight` 七的「不跑」暫時沒有路 | 不推翻；理由改成「型別封閉之下，作者換值不跑轉換的路」 | 推翻 `-bevy-comparison-answers` 的「照用並修補」 | 不推翻，留到那一批重問；`-remaining-eight` 七的「不跑」暫時沒有路（同 (a)） |
| 代價 | 網路那一批多半回到你沒選的那條路 | 做出現在沒有人用的機制：兩種要求、silent 對子的三條規則、判定多 5 個案例與 6 個突變；網路那一批的形狀若不同，要改 | 子的存在跟不上父（父被蓋成主選單，子還在） | 這一批期間「快照蓋值不跑轉換」沒有路——但這一批也沒有網路，用不到 |

**建議 (d)**：真正用它的是網路 Module 那一批，形狀要看那一批怎麼搬伺服器的值；現在做 (b) 是先建一個沒人用、可能要改的機制，現在選 (a) 等於替那一批先排除了你選過的那條路。理由是「該不該現在存在」，不是「之後可以加回來」。

### 2.2 讀檔時子狀態能不能直接回到存檔的值

讀檔卡交給這一批的（P17）。父的部分我判了（第 6 節：`SetInitial`，第一次套用直接進存檔的值，照 Bevy `insert_state`）；子這一半和 Bevy 不同、又碰到你定的規則，所以交你。

**用遊戲講**：存檔時在「關卡中＋暫停」。讀檔在新的 World。父用 `SetInitial(關卡中)` 之後，第一次套用進「關卡中」；`Pause` 跟著出現，照你定的「子每次出現從初始值開始」（`-pause-as-substate`、`-remaining-eight` 一）是「進行中」。

| | (i) 子也能 `SetInitial`（我的建議） | (ii) 不加：讀檔只給父，子用要求換 | (iii) 讀檔時的第一次套用取走子的要求 |
|---|---|---|---|
| 讀檔後跑什麼 | 「進入關卡中」「進入暫停」「進入關閉（背包）」 | 第一次套用：「進入關卡中」「進入進行中」「進入關閉」；下一次套用：「離開進行中」「進入暫停」 | 同 (i) |
| 系譜 | 結果和 Bevy 一樣（Bevy：父 `insert_state`、子同一幀 `NextState`，子直接進那個值；`sub_states.rs:166`、`state_set.rs:171`）；入口是新導的 | 你定的規則照字面 | Bevy 的做法本身 |
| 和你的決定 | 「子每次出現從初始值開始」多一個例外：第一次套用時，作者指定的值優先（只用一次） | 不碰 | 「第一次套用不取走任何要求」（`-first-apply-enters-initial`）多一個例外 |
| 代價 | 子的 `StateMachine` 多存一個「待用的初始值」，第一次套用用掉就清 | 多跑一次「進入進行中、離開進行中」，晚一次套用才暫停——和讀檔卡舉的「先進標題畫面再轉戰鬥」同一個問題 | 要分「讀檔的第一次」與「一般的第一次」，規則變兩套 |

**建議 (i)**：只在「初始值」上開例外，要求的規則不動；結果和 Bevy 相同。你當時比過、沒選的「先存著、等子出現再套用」，理由是「玩家一進關卡就是暫停，比較怪」——那是一般的進關卡；讀檔時「一進關卡就是暫停」正是要的，而且只有作者明寫 `SetInitial` 才會發生。

### 2.3 事件游標：被條件擋住、或不常跑的讀者

生成世界的設計審查留下的（[合併前核准](world-gen-merge-approval.zh-Hant.md)第 4 節：「條件擋住的讀者拉住保留」是事件游標契約的問題，狀態機那一批提）。**判斷：和狀態機直接有關**——狀態機正是讓讀者長期不跑的主要來源，而且進入／離開那兩張現在能讀事件了（P4）。

**三個遊戲看得到的後果**（P14 的規則推出來的）：
1. **拉住保留、一直記丟棄**：主選單的「處理按鈕」帶 `InState<Flow, MainMenu>`，讀 UI 寫的 `ButtonPressed`。進了關卡它就不跑，但它跑過，所以拉住。關卡裡的暫停選單照樣寫 `ButtonPressed`；寫滿上限（預設 1024）之後，每次有人寫，那次 Run 就丟最舊的、記一筆 `DroppedMessages`（「讀者：處理主選單按鈕」）。記錄失去警報的作用。
2. **恢復時補讀舊的**：同一個讀者，玩家回到主選單的那一幀，把關卡期間暫停選單寫的 `ButtonPressed`（上限內全部）一次讀到並處理。**這是玩家看得到的錯**。
3. **進入那張第一次讀不到、之後每次讀到上次以來的全部**：`Flow.Enter.InLevel` 裡的 System 讀 `LevelSelected`。第一次進關卡時它從沒跑過、不拉住，選關的那一幀結束 `LevelSelected` 就被修剪，它讀到空的；之後每次進入，讀到上次進入以來的全部。生成世界也有同樣的情形（world-gen 宣告第 324、467 行：送給還沒跑過的讀者的訊息在那次 Run 結束被修剪；帶 `WhileGenerating()` 的讀者生成完永遠不跑）。

**需要它的 Feature**：四個點名的 Feature 都不需要「在帶狀態條件的 System、或進入／離開那兩張裡讀訊息」；要帶進「進入」的資料（例：選了哪一關）可以放 Resource。游標宣告第 3.2 節已經把「停跑的讀者拉住保留，靠上限擋」記成接受的代價。

| | (A) 不改契約，說明頁寫明（我的建議） | (B) 離開時清掉某種訊息 | (C) 改契約：被 run 條件擋住的那一次放開保留 | (E) 讀者的選項「沒跑就放開，下次從當下讀起」 | (F) 訊息種類的選項「固定保留 N 次 Run，不看讀者」 | (G) 進入時把指定讀者的游標推到最新 |
|---|---|---|---|---|---|---|
| 系譜 | GYO 的游標宣告（Bevy 暫停時也全部留著：有固定步時只在跑過固定步的那幀輪替，`bevy_time/src/lib.rs:102-106`、`message/update.rs:43-57`） | Bevy `clear_messages_on_exit`（`state_scoped_events.rs:67-86`） | 接近 Bevy 的效果 | 新導 | Bevy 的保留模型（兩次 update，不看讀者；有固定步時只在跑過固定步的那幀輪替，暫停時全部留著） | Bevy `clear_messages_on_enter`（`:88-107`）的對應 |
| 後果 1 | 不解；記錄照記 | 不解：清掉之後那個讀者照樣拉住之後的（Bevy 能用是因為它不看讀者） | 解 | 解 | 不解（那種訊息不再被讀者拉住，但也沒有丟棄記錄） | 不解 |
| 後果 2 | 不解；說明頁寫 | 部分（只清離開時已有的） | 解 | 解 | 解（N 次之後就沒了） | 解 |
| 後果 3 | 不解；說明頁寫：要帶進「進入」的放 Resource | 不解 | 不解（那是 schedule 沒跑，不是條件） | 不解（進入時從當下讀起，讀不到上一幀的） | 解（N≥2） | 不解 |
| 連續跑的讀者跨幀讀不讀得到 | 讀得到 | 讀得到 | 讀得到 | 讀得到 | 只在 N 次內 | 讀得到 |
| 「暫停後補讀」（游標宣告第 2 節的槍口火光） | 照舊 | 照舊 | **推翻**（沒有別的讀者拉住的話，暫停期間的就讀不到） | 照舊（不選就照舊） | 那種訊息推翻 | 照舊 |
| 推翻的 | 無 | 無 | 游標宣告第 4.3 節（統合推出的，不是你的原話）與第 2 節的例子 | 無 | 無（作者對那種訊息選） | 無 |
| Kernel 要加什麼 | 無 | 清掉一種訊息的入口（`ExclusiveWorld` 不碰訊息，DI-KS 1） | 改條件與修剪的規則 | 讀者的一個選項 | 訊息種類的一個選項 | 推游標的入口 |

**建議 (A)**：沒有點名的 Feature 需要它；(B)～(G) 都要改 Kernel 的公開介面（基盤），照 AGENTS §2 的 Derivation Order，示不出 Feature 的延期。這一批做的：說明頁寫明三個後果與寫法（進入／離開那兩張與帶狀態條件的 System 要的資料放 Resource）；RC 7 在作者開始寫補償時觸發，那時再開 Kernel 的一批，比較 (C)～(G)。**要不要現在就另開 Kernel 的一批，由你定**；若要，我傾向 (E)＋(F) 讓作者依讀者、依訊息種類選，但那是那一批的宣告要比的。

### 2.4 我判的、讓你知道的（可以更正）

| 件 | 我的判斷 | 理由 |
|---|---|---|
| 舊卡第 (3) 題：從主選單進關卡用新 World 還是 World 內（world-gen 第 5.11 節交這一批問） | 兩種都支援，由作者選；說明頁寫兩種的寫法（第 6 節） | Engine 的機制兩種都有；選哪一種是遊戲設計 |
| 套用的 System 預設不帶條件 | 第一幀就進初始值，即使 World 還在生成中；要等生成完（例：讀檔、載入畫面）就給 `options.conditions = {AfterGenerating()}` | 預設帶 `AfterGenerating()` 的話，每個產品的主選單都晚一幀出現（第一輪結束才結束生成）；Bevy 在開局就進。代價：讀檔與要等生成的遊戲要記得給條件，說明頁與讀檔的案例寫明 |
| 套用的 System 能帶條件與 set | 加（Bevy 的 `StateTransition` 不能帶） | GYO 有「生成中」；讀檔要等生成完才進存檔的值 |
| `Change` 參數 | 這一批沒有；和第 2.1 節一起在網路 Module 那一批再問 | 修訂 7 已經寫「到後一批再請你判」；「權威端換、Client 採用」那一型的形狀在那一批才定得下來 |

---

## 3. 遊戲看得到的行為（照舊）

【你定的】，代號見回收清單第 1.1 節。例子沿用修訂 7：`Flow`（主選單／關卡中），它的子 `Pause`（進行中／暫停）、`Inventory`（關閉／開啟），兩個子都只在關卡中存在，登記順序 `Pause`、`Inventory`。

1. 一棵狀態樹一個套用的 System；一次套用：先跑離開（讀到舊值）、再改值、再跑進入（`-exit-reads-old-value`）。
2. 子狀態只在父的指定值底下存在；離開由子到父、進入由父到子；同一個父底下的子照登記的順序；子不存在時對它的要求丟掉；子每次出現從初始值開始（第 2.2 節選 (i) 時，第一次套用時作者用 `SetInitial` 指定的值優先，只用一次）；同一幀父與子都有要求時，先換父、子從初始值開始，再在同一次套用裡換子（`-pause-as-substate`、`-remaining-eight` 一、三）。
3. 父換成同一個值時默認重設子（子離開、回到初始值、進入）；作者在子的特性裡打開 `KeepOnParentReenter` 改成保留，只對同一個值（`-silent-request-and-child-reset` 二）。
4. 第一次套用一律先進初始值（父的初始值與它底下的子）；之前提出的要求留到下一次（`-first-apply-enters-initial`）。初始值能在執行期給（`-bevy-comparison-answers` 的「`init_state`／`insert_state` 那兩種」）。
5. 一次套用對每種狀態最多處理一個要求；同一幀後寫蓋前寫，被蓋掉的留累計計數；換成同一個值照樣跑離開與進入（`-bevy-comparison-answers`、`-overwritten-request-counted`）。
6. 每個值一張進入、一張離開的 schedule；條件 `InState`／`NotInState`；「還沒進入」與子「不存在」時兩種條件都回否。
7. 狀態機不碰時間；兩棵互不相干的狀態樹之間的先後不當規則承諾（`-remaining-eight` 二）。
8. 只是機制：不帶世代與換的 tick；快照蓋掉值時跑不跑離開與進入由遊戲決定（`-remaining-eight` 七；「不跑」的路視第 2.1 節而定）；AI 這類每個實體一份的狀態機不用這一層（`-remaining-eight` 五、七、八）。

---

## 4. 照既有規則推出來的（照寫，不再問）

修訂 7 第 4 節的七點，照新前提改寫：

1. 進入／離開那兩張可以讀寫事件（P4）。在那裡**讀**訊息：第一次轉換之前寫的讀不到，之後每次進入讀到上次以來的全部（第 2.3 節後果 3）；要帶進「進入」的資料放 Resource——說明頁講明。
2. 遊戲初始化在所有 World 的第一幀之前跑完（O-1），所以初始的「進入主選單」讀得到遊戲初始化放的設定。套用的 System 不要掛在遊戲初始化清單的 schedule 上（那裡跑的是一個 `Product` 一次的事）——說明頁講明，不擋。
3. 套用的 System 是獨占的，和同一個 stage 的每個 System 都衝突，所以自己佔一個 stage（第 1 節第 4 項）。
4. 套用的 System 經閉包讀寫兩種 Resource、經 `RunSchedule` 跑進入與離開；寫入用 `ExclusiveWorld<Writes<…>>` 標註父與每個子的 `StateMachine<…>` 與 `StateMachineRequest<…>`（取走要求也是寫）——Kernel 不解讀，伺服器權威策略靠它看見寫了什麼（權威份第 5.8 節、AU-E6）。多個寫要求的 System 在同一個 stage 照登記的先後（Kernel 不偵測歧義，`v8-arch-gyo2-no-ambiguity-detection`）。
5. 「進入主選單」裡生成的按鈕，在那張的同步點就套用（`-nested-apply-per-level`）；排在套用的 System 後面的 System 同一幀看得到。
6. 每個 World 各自一份；不同 World 的狀態機互不相干（O-13 的單位是一整個 World）。
7. 進入之後，作者的程式改不了現在的值，只能經要求（型別封閉，DI 1）。回滾還原、`CarryOver` 用的是公開的複製與搬移，放回的是之前存下的值。
8. 換 World 時不要 `CarryOver` 狀態機的兩種 Resource：帶過去的值是「已經進入過」，新 World 不會跑進入。要在新 World 接著某個值，用 `SetInitial`（第 6 節）。說明頁講明，不擋。

（修訂 8 初稿的「動態插入的狀態機沒有第一個值」拿掉：動態插入拿到的是 `Kernel::Registrar&`，叫不到收 `WorldSetup&` 的登記函式，不會發生。）

---

## 5. 要改的程式

### 5.1 一次套用的順序（照修訂 7 第 5.1 節的「我補的細節」，拿掉 silent）

- **寫入**：一棵樹一個套用的 System，一併處理父與它的子；子的離開要排在父的離開前面，只有父的套用 System 做得到。
- **子的清單寫在父的特性裡**：`StateMachineTraits<Flow>` 寫 `using SubStates = TypeList<Pause, Inventory>;`；子的特性寫父與父的值（`using Parent = Flow; static constexpr Flow kParentValue = Flow::InLevel;`）。對照用約束（`requires` 子句），寫錯編不過；子不能再有 `SubStates`（只做一層），子不能單獨登記成一棵樹。
- **取走要求**：第一次以外的每次套用，一開始就把父與每個子的待處理要求一起取走、一起驗（任一個值不在對照表就回 Err，什麼都不跑；子存不存在都驗）；這一次套用裡新提出的要求留到下一次。第一次套用不取走任何要求。
- **子什麼時候記成「不存在」**：每個子的離開一跑完就記。
- **一次套用的順序**：
  1. 取走父與子的要求。
  2. 父有要求：每個存在的子照登記的順序跑離開、每個一離開完就記成不存在 → 父的離開 → 改父的值 → 父的進入 → 在新的父值底下該存在的子，照登記的順序先設成初始值、再跑進入。父換成同一個值、子打開了 `KeepOnParentReenter`：那個子不離開、不重設、不進入。
  3. 每個子照登記的順序，有要求就處理：子存在 → 子的離開、改值、子的進入；子不存在 → 丟掉。
- **第一次套用**：父進初始值（`SetInitial` 給過就用它，否則特性的預設）、跑父的進入；初始的父值底下該存在的子，進子的初始值（第 2.2 節選 (i) 時：`SetInitial` 給過就用它，用完清掉；否則特性的預設）、跑子的進入。父的初始值底下不存在的子，`SetInitial` 給的值丟掉。
- **錯誤**：巢狀的 Err 往上傳，World 中斷（P15）；已經改的值不還原（「離開出錯」「進入出錯」案例）。

和 Bevy 的差別照修訂 7 第 5.1 節的表（同一幀父與子都有要求時子先進初始值；父換成同一個值時默認重設）。

### 5.2 程式

| # | 位置 | 內容 |
|---|---|---|
| 1 | 新 Module `engine/modules/state_machine/`；`config/modules.csv` 加一列 `state_machine`（`library_only=yes`、`default=no`、`depends` 空：只依賴 Kernel 與編排層，那兩者照 O-6 手寫在 CMakeLists；先例 `world_logging`）；target `GYO::StateMachine`；namespace `GYO::Engine::StateMachine` | 樣板函式庫。沒有登記單位：狀態的列舉是產品的，Module 不知道有哪些型別；產品在自己的 `build` 呼叫登記函式。修訂 7 寫的 `engine/state_machine` 跟著 P11 搬到 `engine/modules/` 底下 |
| 2 | `StateMachineTraits<S>` | 作者在產品端特化：狀態名、值與名字的對照（`constexpr` 陣列）、預設的初始值；父另列 `SubStates`；子另寫父、父的值、`KeepOnParentReenter`（默認關） |
| 3 | `StateMachine<S>` | 現在的值、「還沒進入」、或（子）「不存在」；`.Current()` 讀；`SetInitial(值)`：第一次套用之前換這一次的初始值（之後呼叫是斷言；子的只在第 2.2 節選 (i) 時有）。沒有從 `S` 的值建構、沒有其他改值的成員；複製、搬移、`==` 公開（回滾、`CarryOver`、`World::SetResource` 要）。`HashValue` 收全部欄位（給 World 摘要與權威的 digest；先例：`WorldGeneration`） |
| 4 | `StateMachineRequest<S>` | 待處理的要求（沒有、或一個值）；`Request(值)`；被蓋掉的累計次數。複製、搬移、`==` 公開。`HashValue` 不收計數（它是診斷，兩端不必相同） |
| 5 | `RegisterStateMachine<S>(WorldSetup& setup, StateMachineOptions options = {})` | 在產品的 `build` 呼叫。`StateMachineOptions`：`schedule`（預設 `DefaultSchedules::frame`）、`anchor`（預設 `DefaultSchedules::preUpdate`）、`place`（預設 `StagePlace::After`）、套用的 System 的 `conditions` 與 `sets`。做：登記父與每個子的兩種 Resource（名字 `StateMachine.<狀態名>`、`StateMachineRequest.<狀態名>`）；每個值兩張 schedule（`<狀態名>.Enter.<值名>`、`<狀態名>.Exit.<值名>`，各一個 stage `Update`）；`InsertStage` 一個 `<狀態名>.Apply`；`AddSystem` 套用的 System `ApplyStateMachine.<狀態名>`；`setup.Prepare` 放父與每個子的第一個值（已經在就不蓋，O-7）。骨架關掉而沒給 `schedule`／`anchor` 時是 Kernel 的 D-E1 |
| 6 | `ApplyStateMachine<S>` | 獨占的 System，照第 5.1 節；`ExclusiveWorld<Writes<父與每個子的 StateMachine 與 StateMachineRequest>>`。要求值不在對照表：取走時回 Err（這個 Module 自己的規則字串），不跑任何離開、不改任何值 |
| 7 | `InState<S, V>()`、`NotInState<S, V>()` | 回傳 `ConditionRef`，名字 `InState.<狀態名>.<值名>`、`NotInState.<狀態名>.<值名>`（條件名進權威的一致性指紋，名字固定；先例 `WhileGenerating`）；讀 `Res<StateMachine<S>>`；「還沒進入」「不存在」都回否 |
| 8 | 測試 `tests/common/state_machine/`（ctest 標籤 `cpu`） | 行為的案例一個 target，只連 `gyo_state_machine`；組合的案例（權威、回滾、生成、替換）另一個 target；連結集 `gyo_expect_links(gyo_state_machine …)`（DI 8） |
| 9 | 文件 | 新頁 `docs/architecture/target/v2/state-machine.zh-Hant.md`（Design Invariants、Replacement Criteria、寫法；同 `world-time.zh-Hant.md` 的形）；[System 契約說明頁](system-contract-page.zh-Hant.md)加一節（掛在哪、進入／離開、條件、子狀態與 `KeepOnParentReenter`、跨樹的先後看插入位置、暫停要不要停時間的寫法、生成中與 `AfterGenerating`、`SetInitial` 與讀檔、新 World 與 World 內兩種進關卡、(c) 區的標記與回滾的登記、第 4 節講明的幾點、第 2.3 節的三個後果與寫法、遊戲初始化裡 `InState`／`NotInState` 一律回否、套用的 System 帶的條件擋的是每一次套用（例：帶 `AfterGenerating()` 時，World 內重新生成期間的要求延到生成結束）、重跑時用 `Resimulation` 擋只該發生一次的副作用）；[用詞表](rebuild-vocabulary.zh-Hant.md)加一列「State Machine（狀態機）：一個 World 一份的全局狀態機；AI 這類每個實體一份的狀態機不用這一層」；`docs/cross-component-rules.md` 查有沒有同樣的規則；dev_log |

**不碰**：`engine/ecs/kernel`、`engine/orchestration`、其他 Module 的程式；既有測試與守衛的判定；`.github/`。

### 5.3 (c) 區「兩端同規則」的接法（作者的寫法；這一批用組合的案例驗）

例：聯機的天氣，Server 與 Client 用同一條規則在同一個 tick 換（lightyear 決定性模式那一型）。

```cpp
// Product build, both ends (sketch; exact names follow the naming rules at implementation).
using namespace GYO::Engine;
StateMachine::RegisterStateMachine<Weather>(setup, {.schedule = "FixedUpdate", .anchor = "First"});
Authority::AuthorityEntry authority(setup);
authority.Mark<StateMachine::StateMachine<Weather>>();          // by type: in the digest
authority.Mark<StateMachine::StateMachineRequest<Weather>>();
authority.Runs("ApplyStateMachine.Weather", {"Weather.Enter.Clear", "Weather.Exit.Clear" /* ... every enter/exit */});
// A Client with a rollback strategy: the two Resources roll back, and the enter/exit schedules run nested on the step.
rollback.Resource<StateMachine::StateMachine<Weather>>();
rollback.Resource<StateMachine::StateMachineRequest<Weather>>();
rollback.NestedSchedule("Weather.Enter.Fog", "FixedUpdate"); // ... one per enter/exit schedule
```

**重跑時的副作用**（使用者 2026-11-22「1 保留」）：回滾重跑那一步時，「進入起霧」會再跑一次。改世界狀態的（能見度）照常重跑；只該發生一次的副作用（音效、粒子）由作者在進入／離開裡讀 `Resimulation` 擋——別家的對應是 lightyear 的 `is_in_rollback`、Unity Netcode 的 `IsFirstTimeFullyPredictingTick`。別家都不重跑轉換（lightyear 回滾只重跑固定步、Bevy 的轉換不在固定步裡），GYO 照使用者 11-15～16 的決定重跑（第 12 節）。組合的案例示範一次。

沒有回滾登記時，套用的 System 寫的判定資料在可回滾的步上會報 AU-E6（P9），回滾重跑也退不回轉換。`StateMachineRequest` 被回滾還原時，被蓋掉的計數跟著還原（不再是整個 World 期間的累計）——寫進第 10 節。

「權威端換、Client 採用」：留到網路 Module 那一批（第 1 節第 2 項、第 2.1 節）。

---

## 6. 和生成世界、讀檔的交界

**生成世界**（world-gen 第 5.9、5.11 節留給這一批的四件）：

| 件 | 修訂 8 | 依據 |
|---|---|---|
| 初始狀態和生成的先後 | 狀態機不認得生成。套用的 System 預設沒有條件：第一幀就進初始值，即使 World 還在生成中——這時只帶 `InState` 的遊戲邏輯在生成中就會跑。要等生成完才進，作者給 `options.conditions = {AfterGenerating()}`（或放進帶它的 set）；那樣生成中 `InState` 都回否 | 【我判的】第 2.4 節 |
| 舊卡第 (3) 題：從主選單進關卡用新 World 還是 World 內 | 兩種都支援，由作者選。**World 內**：「進入關卡中」裡把 `WorldGeneration` 寫回生成中；這一輪開始時不是生成中，所以這一幀尾不判定；下一幀開頭 `held` 清成 false，要靠帶 `WhileGenerating()` 的 System 每幀 `Hold()` 撐到載完（P7）。**新 World**：`ReplaceWorld`，新 World 的狀態機從初始值開始；要直接在「關卡中」就 `SetInitial` | 【我判的】第 2.4 節 |
| World 內換關卡要不要 `Regenerate` | 不要 | world-gen 設計審查的看法，我照它判（P7） |
| 和「生成中」的分工 | 生成是編排層的、每個 World 建好時那一段；遊戲的段落（主選單、關卡中、暫停）是狀態機的 | world-gen 第 5.9 節 |

**讀檔**（P17；讀檔卡的說明頁指路到這裡）：

例子（讀檔卡舉的）：RPG 存檔時在「戰鬥中」，初始狀態是「標題畫面」。

| | 第一次套用直接進存檔的值（這一批的做法） | 照常先進初始值，再轉到存檔的值 |
|---|---|---|
| 讀檔後跑什麼 | 「進入戰鬥中」一次；「標題畫面」的進入與離開都不跑 | 「進入標題畫面」（播標題音樂、建標題 UI）→ 下一次套用「離開標題畫面」→「進入戰鬥中」 |
| 系譜 | Bevy `insert_state`：給的值直接當初始值（`bevy_state/src/app.rs:122-152`） | — |
| 代價 | 讀檔的程式要在第一次套用之前給值 | 多跑一次初始狀態的進入與離開，而且晚一次套用才到 |

**做法**：
- 讀檔的 System 在生成中讀到存檔的狀態，寫 `ResMut<StateMachine<Flow>>` 的 `SetInitial(戰鬥中)`；套用的 System 帶 `AfterGenerating()`，所以第一次套用在生成結束之後，直接進「戰鬥中」。發替換之前就知道的，也可以在 `ReplaceRequest.extra` 裡 `world.ResourceMut<StateMachine<Flow>>()->SetInitial(…)`。
- 子（存檔時暫停）：照第 2.2 節的結果。
- **為什麼不推翻「第一次套用一律先進初始值」**（`-first-apply-enters-initial`）：那條說的是第一次套用進初始值、之前提出的要求留到下一次；修訂 7 已核准的同一組規則就有「初始值能在執行期給」（`-bevy-comparison-answers` 的「`init_state`／`insert_state` 那兩種」；修訂 7 第 3.8 節：「建 World 前就知道的，用『指定初始值』」），讀檔用的是這一條。你當時的理由是「沒有主選單，你就沒有讀檔和重新設定的能力」——讀檔是在舊 World 的主選單發起、在新 World 接續（P17），主選單已經出現過，和這條不衝突。
- **不要直接放回 `StateMachine<S>` 的值**（`CarryOver`，或讀檔的 System 用存下的副本蓋掉）：新 World 會以為已經進入過，不跑進入（第 4 節第 8 點）。讀檔卡若要「放回值、不跑進入」（例：介面由讀檔的 System 自己建），狀態機要另給一個入口，那時再談（RC 8）。
- 讀檔卡的案例要固定這個行為的話，用第 7 節的「讀檔直接進存檔的值」。

---

## 7. 判定（先宣告，結果出來之後不放寬）

測試用的產品照第 3 節的例子，另有互不相干的 `Weather`（晴／雨）。套用的 System 用預設的插法（骨架 `Main` 的 `PreUpdate` 之後）。下面寫「各一次」「沒有跑」時，指的是案例點名的那幾張。

**判定 1：行為。** 修訂 7 第 7 節判定 1 的案例，照下表增減；其餘照抄（初始的進入、遊戲初始化裡的要求留到下一次、初始之前條件為否、換狀態、跑對那一張、離開時的值、換完不再換、一次只處理一個要求、一個子的進入裡對另一個子提要求、第一次套用時子的要求、子狀態跟著出現、子進入時讀到的值、子在父的進入裡還不存在、暫停時遊戲邏輯不跑、子換時父不動、子狀態跟著消失、多個子一個一個消失、子狀態重新出現時回到初始值、子狀態不存在時的要求、父與子同一次都有要求、兩個子同一次都有要求、父換成同一個值（默認）、父換成同一個值（保留）、兩個要求、不在對照表的值、子的值不在對照表、離開出錯、進入出錯、子的離開出錯、子的進入出錯、名字、型別不給改）。修訂 7 案例裡的「開局」一律改成遊戲初始化（「初始的進入」「開局裡的要求留到下一次」「第一次套用時子的要求」）。

| 案例 | 修訂 8 | 驗什麼 |
|---|---|---|
| 不跑離開與進入的要求、silent 讓子消失、silent 換成同一個值、子的 silent | 拿掉（第 2.1 節選 (a) 或 (d) 時） | — |
| 「兩個要求」裡 `Request` 被 `RequestSilent` 蓋掉的那一句；「不在對照表的值」末句「用 `RequestSilent` 提出同樣回 Err」 | 拿掉（同上） | — |
| 沒放第一個值、Install 呼叫兩次 | 拿掉（`InstallStateMachine` 沒了） | — |
| 型別不給改 | 改 | 用 `requires` 運算式確認：`StateMachine<Flow>` 不能從 `Flow` 的值建構、沒有 `SetInitial` 以外改值的成員；複製、搬移、`==` 可用 |
| 子的第一個值 | 改 | 登記之後、第一次 Run 之前，父與每個子都有值（父「還沒進入」、子「不存在」），作者不必另外呼叫 |
| 指定初始值 | 改 | 描述的準備函式裡 `SetInitial(InLevel)`：第一次套用跑「進入關卡中」「進入進行中」「進入關閉」；「進入主選單」沒有跑 |
| 讀檔直接進存檔的值 | 新 | 套用的 System 帶 `AfterGenerating()`；一個帶 `WhileGenerating()` 的 System 在生成中 `SetInitial(InLevel)`（第 2.2 節選 (i) 時另 `SetInitial` `Pause` 為暫停）：生成中沒有任何進入；生成結束後的第一次套用依序「進入關卡中」「進入暫停」（選 (i) 時；否則「進入進行中」）「進入關閉」，「進入主選單」沒有跑。選 (i) 時再一步：之後回主選單再進關卡，`Pause` 是進行中（待用值只用一次）；另一組只給 `Pause` 的 `SetInitial`（父照預設主選單）：`Pause` 不存在，之後進關卡中時 `Pause` 是進行中 |
| 特性對不上、寫者、(c) 區還不支援、runner 與時鐘、不碰時間 | 拿掉（第 1 節第 7、8、12 項；P4） | — |
| 預設的插法 | 新 | 沒給 `schedule`／`anchor`：套用的 stage 在 `Main` 的 `PreUpdate` 之後、`FixedMainLoop` 之前；排在 `PreUpdate` 的 System 同一幀讀到換之前的值，排在 `Update` 的讀到換之後的 |
| 套用的 System 的條件 | 新 | `options.conditions = {AfterGenerating()}`：生成中的那幾幀沒有進初始值；生成結束之後的第一幀進 |
| 進入裡讀寫事件 | 新 | 「進入關卡中」裡寫一筆訊息，排在套用的 System 後面的讀者同一幀讀到 |

**組合的案例**（驗的是 Kernel、編排層、別的 Module 和這個 Module 接起來的行為；不算進判定 2 的檢出力）：
- 進入裡生成的實體，排在套用的 System 後面的 System 同一幀看得到（修訂 7 的組合案例，`-nested-apply-per-level`）。
- 同一種狀態登記兩次報 R-E2。
- 替換 World：新 World 從初始值開始、跑初始的進入；`ReplaceRequest.extra` 裡 `SetInitial` 時進那個值。
- 兩棵樹的實際先後（`Weather` 與 `Flow` 插在同一個錨點之後、`Flow` 先登記 → `Weather` 的 stage 緊貼錨點、先套用）——固定實際行為，不是承諾。
- 生成中、套用的 System 沒有條件：第一幀就進初始值（固定實際行為）。
- 進入那張的讀者：第一次進入讀不到選關那一幀寫的訊息；第二次進入讀到上次以來的全部（固定第 2.3 節後果 3 的實際行為；第 2.3 節的結果改了契約時跟著改）。
- (c) 區「兩端同規則」（第 5.3 節）：選伺服器權威的 World，`Mark`、`Runs` 不報 AU-E4；回滾登記了兩種 Resource 與巢狀的進入／離開時不報 AU-E6、AU-E9；值換了 digest 跟著變；回滾退回 k 步重跑時，那一步的轉換重跑一次（「進入起霧」在重跑裡跑了），而進入裡讀 `Resimulation` 擋住的副作用在重跑裡沒有發生。

**判定 2：檢出力。** 突變清單由實作者（Opus）定案，交別的模型（Fable）做敵對式檢查（AGENTS §14.2），找出「把實作改壞而案例全過」的地方補上。預期的那個案例失敗才算抓到；建置失敗不算。每個突變照 §14.5 寫它獨自抓的錯與守的對象；只跑這一批相關的突變。至少包含修訂 7 第 7 節判定 2 表裡和上面案例有關的那幾列（行為的突變），並照下表增減：

| 突變 | 修訂 8 |
|---|---|
| silent 的六列；`Request` 被 `RequestSilent` 蓋掉不算；不合法的值用 `RequestSilent` 提出時不回 Err | 拿掉（第 2.1 節選 (a) 或 (d) 時） |
| `InstallStateMachine` 呼叫第二次不斷言；沒放值時回 Err 而不是斷言 | 拿掉 |
| 拿掉特性對照的四個約束；現在的值改成 `Shared`；忽略 runner 或時鐘；拿掉 `Zone::C` 的擋；掃描的三列 | 拿掉（守的是誤用或已經沒有的規則，P13） |
| `StateMachine` 開放帶值的建構；開放預設建構；開放改值的成員 | 留，改成：開放從值建構；開放 `SetInitial` 以外的改值成員（型別不給改） |
| `InstallStateMachine` 不放子的第一個值；忽略指定的初始值 | 改成：登記的準備函式不放子的第一個值（子的第一個值）；`SetInitial` 沒有作用（指定初始值） |
| 子的 `SetInitial` 沒有作用；子的待用值用完沒清；子的 `SetInitial` 在父的初始值底下不存在時照樣讓子出現 | 新（讀檔直接進存檔的值；第 2.2 節選 (i) 時） |
| 預設錨點放在 `PreUpdate` 之前 | 新（預設的插法） |
| 忽略 `options.conditions` | 新（套用的 System 的條件） |

「登記的準備函式蓋掉已經在的值」：值事先在的情形只有 `CarryOver`（第 4 節第 8 點說不要用），沒有案例能獨自抓到，照 §14.5 不加。

**判定 3：沒有回歸。** 本機完整 ctest 全過；三平台 L1（linux-x64、windows-x64、macos-arm64）全過。既有測試的判定一個都不改。

---

## 8. Architecture Delta、Design Invariants、Replacement Criteria

### 8.1 Architecture Delta（AGENTS §3 的七項）

1. **需要它的 Feature**：啟動後進主選單、從主選單進關卡、暫停時不跑遊戲邏輯（單機 Game）；遊戲中的全局狀態（天氣、時段、地圖的全局事件；聯機 Game 的 Server 與 Client，單機也用）。
2. **現在的問題**：沒有狀態切換的形狀；每個產品會各寫一套「在哪一段」「進入時做一次」。
3. **變動的邊界**：Engine 多一個 Module `engine/modules/state_machine`（新目錄、CMake target、登記檔一列、測試目錄、它的條文頁）。Kernel 與編排層的邊界不變。
4. **影響**：`engine/modules/state_machine`、它的測試、`config/modules.csv`、CMake 的建置圖；說明頁、用詞表。`gyo2` 上沒有產品。
5. **依賴方向**：`state_machine` → 編排層 → Kernel。Kernel、編排層、時間、權威、回滾都不依賴它；它也不依賴時間、權威、回滾（組合的案例的 target 連它們，那是測試）。
6. **Ownership**：「現在在哪一段、什麼時候換」歸這個 Module 的 Resource 與套用的 System；狀態的列舉、名字表、子的清單、什麼時候要求換歸產品；插在哪、套用的 System 的條件歸產品的登記；初始值的預設歸特性、執行期的初始值歸作者（準備函式或生成中的 System）；「暫停時時間停不停」歸作者；(c) 區「誰是判定資料、誰會巢狀跑、誰可回滾」歸作者對權威與回滾的登記；兩棵狀態樹之間的先後歸插入位置。
7. **為什麼不能更小**：只給說明頁的範本（Godot、Unity 走這一條；每個產品各寫一套）；每種狀態只有一對 schedule（修訂 7 第 3.9 節）。

基盤的理由（AGENTS §2）：每一個有選單、有暫停的產品都會照它寫（進入的 schedule 名、條件、要求）；之後改「轉換在一幀的哪裡」「進入時看得到什麼」，上面積多少改多少。

### 8.2 Design Invariants（新；要你同意。修訂 7 第 8.2 節的改法寫在每條後面）

1. 一種狀態進入之後，作者的程式只能經要求改它的值：型別沒有從 `S` 的值建構、沒有 `SetInitial` 以外的改值成員，`SetInitial` 只在第一次套用之前有用；複製、搬移、比較公開（回滾與 `CarryOver` 放回的是之前存下的值）。守衛：「型別不給改」（`requires` 測試，守你定的「照用並修補」）。修訂 7 的「寫者」測試拿掉（D-E2 沒了）。
2. 一次套用的順序：取走要求 →（子照登記的順序離開、每個一離開就記成不存在）→ 父的離開（現在的值仍是舊的）→ 改值 → 父的進入 →（子照登記的順序設初始值、進入）→ 子各自處理自己的要求。守衛：照修訂 7。
3. 一次套用對每種狀態最多處理一個要求，要求在套用一開始就取走；離開或進入裡提出的要求留到下一次。守衛：照修訂 7。
4. 第一次套用一律先進初始值（套用的 System 第一次執行時；在遊戲初始化之後，O-1）；初始值是特性的預設、或第一次套用之前用 `SetInitial` 給的；之前提出的要求留到下一次；在那之前兩種條件都回否。每個 World 各自、每次建 World 各自。守衛：照修訂 7，加「指定初始值」「讀檔直接進存檔的值」。
5. 子只在父的指定值底下存在；不存在時兩種條件都回否、對它的要求丟掉；每次重新出現從初始值開始（第 2.2 節選 (i) 時：第一次套用時 `SetInitial` 給的值優先，只用一次）；父換成同一個值時默認重設，`KeepOnParentReenter` 時保留。守衛：照修訂 7，選 (i) 時加「讀檔直接進存檔的值」。
6. ~~`RequestSilent`~~：第 2.1 節選 (a) 或 (d) 時沒有這一條；選 (b) 時照修訂 7。
7. 同一種狀態只在一處套用。守衛：Kernel 的 R-E2（登記函式呼叫兩次），組合的案例固定。修訂 7 的 D-E2 那一半拿掉（作者另外登記套用的函式是誤用，P13）。
8. 狀態機不碰時間：不讀寫 `TimeControl` 與任何時間的 Resource，不依賴時間 Module。守衛：行為測試的 target 只連 `gyo_state_machine` 而編得過，加 `gyo_state_machine` 的連結集（修訂 7 的 include 掃描與自我測試拿掉）。
9. Kernel 與編排層不知道狀態機：這個 Module 只用它們的公開介面。守衛：Kernel 由 `kernel_guard.py` K5 的 include 准許清單、編排層由 `orchestration_guard.py` O4 的准許清單守（既有的守衛，不改）。
10. 名字固定由特性組出，同一組特性在兩端組出相同的名字：兩種 Resource（`StateMachine.<狀態名>`、`StateMachineRequest.<狀態名>`）、套用的 System（`ApplyStateMachine.<狀態名>`）、stage（`<狀態名>.Apply`）、進入／離開（`<狀態名>.Enter.<值名>`、`<狀態名>.Exit.<值名>`）、條件（`InState.<狀態名>.<值名>`、`NotInState.<狀態名>.<值名>`）。守衛：「名字」案例。（新加：(c) 區要兩端比一致性指紋，指紋用 Resource 的登記名與 System 名，`Consistency.cpp:173-181`。）

### 8.3 Replacement Criteria（新）

符合任一條時，停下來交你「修還是重建」。修訂 7 的第 3 條（Kernel 的 (c) 區獨占套用是另一批）拿掉；第 7 條改寫；第 8 條新加。

1. (c) 區的狀態因為這個 Module 的形狀，出現「兩端不一致而 digest 看不到」的情形。
2. 一個點名的 Feature 需要同一種狀態在每幀與固定步兩處換。
3. 為了狀態機要在 Kernel 或編排層加型別、分支或介面（DI 9 被打破）。
4. Engine 的另一個 Module 需要讀狀態（例：Loading 或生成改成綁在狀態機上），而這個 Module 的形狀接不上。
5. 作者為了繞過「一次只處理一個要求」「進入在套用處跑」「第一次一律進初始值」，在兩處以上寫了同樣的補償（例：自己再跑一次套用、自己存「上一個狀態」）。
6. 子狀態要第二層，或一個子要掛在兩個父值底下，而這個形狀接不上。
7. 作者為了訊息的保留（第 2.3 節的三個後果），在兩處以上寫「一直跑的 System 讀、抄進自己的 Resource」，或為了後果 2 自己記「上次讀到哪」。
8. 網路 Module 那一批的伺服器覆蓋、或讀檔，要一條「放回值、不跑進入」的路，而這個 Module 的型別封閉接不上。

---

## 9. 誰做、用什麼檔位

上段：新的 Module、新的 Design Invariants（navigator 的上段條件）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作 | Opus／high | 順序與狀態容易錯，但範圍明確；xhigh 只在要調查時用（使用者 2026-11-19） |
| 突變清單的敵對式檢查 | Fable／high | 和定清單的人（Opus）不同模型（AGENTS §14.2） |
| 合併前的設計審查（唯讀） | Fable／high | 批次表〈每批的設計審查〉；和實作者不同模型 |
| 本宣告的敵對式檢查 | Fable 5.2／high（唯讀，已做） | AGENTS §4、navigator；作者是 Opus 5.5，不同模型 |

**步驟**（一批、一個 PR，base `gyo2`）：
1. Module 的骨架：登記檔一列、target、特性、兩種 Resource、條件、登記函式（含準備函式）；行為測試的前半。
2. 套用的 System（第 5.1 節）與行為測試的後半。
3. 組合的案例（R-E2、替換、生成、讀檔、(c) 區與權威、回滾）。
4. 突變與敵對式檢查；條文頁、說明頁、用詞表；dev_log；設計審查；開 PR。

**時間的估計**（agent 的時間）：實作與測試 7 到 10 小時；審查與敵對式檢查 2 到 3 小時。超過估計的 1.5 倍，或同一個問題換了兩次做法還沒結論，就停下來回報（AGENTS §14）。

---

## 10. 範圍的限界

- 這份宣告照讀程式與 Bevy 的原始碼寫，沒有試做。修訂 7 的限界裡和舊前提有關的（K0、T3 還沒做、`gyo2` 沒有回滾）已經不成立；其餘照舊。修訂 8 照檢查報告改完沒有再送檢查。
- 「權威端換、Client 採用」、`Change` 參數、快照蓋值時「不跑轉換」的路：留到網路 Module 那一批（第 2.1 節）。
- `StateMachineRequest` 被回滾還原時，被蓋掉的計數跟著還原；「World 存在期間不歸零」在回滾的 World 裡不成立。
- 第 2.3 節選 (A) 時，三個後果照舊，靠說明頁與 RC 7。
- **三個 (b) 區的 Feature 只在一個 World 的組合下驗。** 模擬與畫面分成兩個 World 時（交換點 O-12、O-13），暫停的要求在畫面的 World、固定步在模擬的 World，怎麼傳是交換的資料，不是狀態機的事；第一個點名的使用者那一批（PvP Client 的移植）再看。
- 套件插的 stage 別人可以掛進來；作者自己的獨占 System 可以直接跑「進入」——不擋，說明頁講明。
- 回滾時重跑轉換：這一批只驗一個組合的案例；「事件進不進回滾的快照」照回滾份，不在這一批。
- Bevy 的行為引自原始碼（只讀，沒有編譯或執行）；其他引擎的引文照修訂 7 的限界。
- 效能沒有量。

---

## 11. 檢查報告的處理（`declaration-check-r8-fable.md`）

| 條 | 處理 |
|---|---|
| B1 事件 | 第 2.3 節重寫：三個後果、補 (E)(F)(G)、(D) 撤掉（不拉住的讀者在現在的修剪下只讀得到同一次 Run 裡在它前面寫的）、加「需要它的 Feature」；建議改成 (A) |
| B2 子的讀檔 | 移到第 2.2 節交你；Bevy 的引文更正；第 3 節、5.1、DI 4、DI 5 跟著寫 |
| B3 (c) 區 | 這一批只做「兩端同規則」；「權威端換、Client 採用」與 `Change` 留到網路那一批，出處改正（第 1 節第 2 項、第 2.4 節） |
| B4 silent | 第 2.1 節重寫：寫明放回都有型別、網路那一批要的路、補 `-remaining-eight` 七、「不成對」兩邊都算、拿掉「之後可以加回來」的理由；加 (d)，建議改成 (d) |
| S1 | 5.3 補回滾登記；5.2 寫明複製、搬移、`==`、`Writes` 加要求、計數不進雜湊；組合案例加 AU-E6；第 10 節記計數 |
| S2 | DI 1 字面改寫；「型別不給改」留（守你定的契約） |
| S3 | 第 6 節的 `Hold()` 改正 |
| S4 | `SetInitial` 改成型別的成員，生成中的 System 也能呼叫；讀檔的做法配 `AfterGenerating()`；預設的後果寫明（第 2.4、6 節） |
| S5 | 第 3 節第 8 點補回 |
| S6 | 第 6 節的論證改引修訂 7 的「指定初始值」，引全原話 |
| S7 | 組合案例補回 |
| S8 | P7 與第 6 節的出處改正；第 (3) 題列進第 2.4 節 |
| S9 | DI 8 的守衛改成「行為測試的 target 只連它」 |
| S10 | 第 4 節第 1 點補讀的兩個陷阱；組合案例加一個 |
| S11 | 案例加「待用值只用一次」；「準備函式不蓋」照 §14.5 不加並寫明；DI 4 的守衛欄補 |
| nit 1～13 | 行號、代號、草稿程式、`set_if_different`、前提的編號、證據的「未保存」、名字列進 DI 10、開局的字、動態插入那一點拿掉、`SetInitial` 的時機、和 Bevy 不同的條件入口、作者與檢查者的模型，都已改 |

---

## 12. 多家交叉比對（2026-11-22，動工前核准之後）

使用者問「你有交叉比對其他家的實作嗎？」——修訂 8 只對照了 Bevy。使用者：「先做比對，有分歧再交給我」。三個唯讀偵察（Opus／high），讀本機 `~/Code/GitHome` 的原始碼；報告在主 checkout `build/evidence/gyo2-state-machine-r8/`：`crosscheck-network-adoption.md`（bevy_replicon、lightyear、Unity Netcode）、`crosscheck-initial-load.md`（flecs、Unity Entities、MassSample）、`crosscheck-message-retention.md`（flecs、Unity Entities、Unity Netcode、lightyear、bevy_replicon）。

| 題目 | 結果 |
|---|---|
| 第 2.1 節 (d) | 站得住：三家的網路層都直接寫值、不跑轉換；要跑轉換由作者另走訊息再發要求。網路宣告核准的 (b)（`-net-…` Q8，Request 與 RequestSilent 兩個套用函式）蓋得到兩種，相關兩點已轉主會話 |
| 初始值、讀檔、子狀態、套用的位置與條件 | 沒有分歧：Unity、Mass 也是「初始化之後、每個 World 各自」；Unity 的 `DeserializeWorld` 要讀進空 World、讀完才加 System，和「新 World、等載完、直接進存檔的值」相符；flecs 讀檔途中就觸發進入（反例） |
| 「兩端同規則」回滾重跑進入／離開 | **分歧**：沒有一家重跑轉換。交使用者 → 「1 保留」；副作用用 `Resimulation` 擋（第 5.3 節） |
| 停跑的讀者拉住保留＋恢復時補讀 | **分歧**：五家都沒有這個組合；Bevy 在兩次 update 的曝露下就遇到跨狀態讀到舊訊息，在 Plugin 層加了清除（commit adc2cf7df，#15085）；replicon 自己寫補償。交使用者 → 「2 選 A＋」：這一批照 (A)，另開「事件游標重評」的卡（照游標修剪是使用者定的 `event-trim-by-cursor`，結論交使用者） |
| 事實更正 | Bevy 暫停時訊息全部留著（第 2.3 節表已改）；游標宣告第 3.2 節引的 lightyear 行號已漂移（現在 `receive.rs:54`、`:105-110`、`:153-160`、`:447-456`），轉主會話交圖書管理員 |

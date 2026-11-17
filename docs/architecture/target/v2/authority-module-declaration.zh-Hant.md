# 權威 Module：事前宣告（子份；修訂 2）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。核准本宣告不等於 AGENTS §14.4 的判定變更核准**：搬到權威 Module 的每一個守衛判定，在實作那一批的事前宣告逐案列出、逐條核准。
> 批次代號：`v8-arch-gyo2-authority-module`。
> **這份是「Kernel 與編排層的邊界」拆出來的三份子份之一**（`v8-arch-gyo2-boundary-split-integrator-first`）。統合是 [`kernel-app-boundary-declaration.zh-Hant.md`](kernel-app-boundary-declaration.zh-Hant.md)（分支 `claude/gyo2-kernel-app-boundary-4af68c`，修訂 15，6ba6124d）。**以統合為準**：統合定過的事這裡不改；需要改 Kernel 公開介面的，或關聯表沒有的新關聯，回報統合（第 8 節），不在這裡判。
> **修訂紀錄**：修訂 0（0b802f5e）回報統合 N1～N8。修訂 1（2026-11-17）：統合修訂 18（ffdc8014）照 L17 判了 N1～N8（L22～L28、L6 與 L10 的補句）；四份交叉檢查（Fable／high，證據 `build/evidence/gyo2-kernel-app-boundary/four-docs-adversarial-check.md`）之後，統合修訂 19（1f0e5dac）通知本份九件，照改：接 Z6、Z8、D-E15（5.14）；加豁免寫者的登記（4.8）；P6 與 5.1 標待 L28；6.2、6.13 改指 L29；4.3 補事件的登記與「只標記」的入口；確認清單（6.14）；4.5 拿掉違反 O-4 的核對法；第 10 節改成統合第 8 節的五段；引用統合 3.3 補的 effects 與 `Writes<…>`。修訂 2（同日）：統合修訂 20（20f62ae1）照第二輪交叉檢查（`four-docs-adversarial-check-r2.md`）通知本份十一件，照改：豁免只由伺服器權威策略讀回滾的名單代為登記（4.8）；代為登記的時機寫進 interface（4.4 的「登記當下」）；tick 驅動者與對時怎麼過規則 1（5.6、6.11）；C 指紋的「進／不進／待判」表（5.8），入口的 stage 回到選項（6.15）；代登記進回滾的範圍（4.3）；規則 1 對生成怎麼算（5.3）；`Digest`、`OnMismatch` 不在這次核准的契約內（4.4）；狀態機用名字標記、不依賴（4.2）；巢狀跑的 schedule 與事件在交叉檢查裡怎麼算（5.14）；L3 列更正、補 L30、改讀回滾 d8125486；建議改用選項的描述名、寫清楚「N」的三種意思。**要問使用者的題目由統合合併排序（統合第 14 節，修訂 20 起 20 題），由統合或主會話一次問，本份不自己問。**
> **「N」的三種意思**：N1～N8＝本份修訂 0 回報統合的編號（第 8 節）；N11～N17、N5＝命名規則（`rebuild-naming.zh-Hant.md`）的條號；N2（寫作「舊網路設計稿的 N2」）＝`docs/architecture/plans/kernel/networking.zh-Hant.md` 的第 2 條。
> 範圍：統合關聯表 L12。**回滾的內容不在這份**（回滾 Module 那一份，L11）；事件游標在游標份（L13）；tick 存在哪裡在時間 Module（L2）。

---

## 0. 依據

| 項目 | 版本 |
|---|---|
| 程式 | `gyo2` f00433b0（#240 合併後；Kernel 的程式和統合讀的 cb6bccca 相同） |
| 決定表 | HANDOFF，分支 `claude/gyo2-time-design` 2416d4c0 |
| 統合 | 修訂 20，20f62ae1（修訂 0 讀修訂 15，修訂 1 讀修訂 19） |
| 回滾份 | d8125486（分支 `claude/gyo2-rollback-module-8bac02`）：公開登記 `rollback.Component<T>(options)`、`rollback.Resource<R>(options)`（沒登記的就是 No）、可回滾的 schedule `rollback.Schedule(name)`、重跑時不跑 `rollback.SkipDuringResimulation(...)`（登記 System 時掛 Kernel 的執行條件）、重跑者與還原者的公開名單（第 5.4 節第 0 點、第 5.6 節）；回滾不依賴權威（5.2、R-3） |
| 回收清單 | `recovered-from-stale-cards.zh-Hant.md`（同 HANDOFF 的分支）——狀態機卡的「`Level::C`／`Zone::C` 的 static_assert」「(c) 區要等 Kernel 的獨占套用」不帶進來，這份重新看 |
| 舊規格（只取設計思想，不照搬） | `docs/architecture/plans/kernel/spec.zh-Hant.md`：B.3、B.3.1、G 節、K.3 |
| 別家 | 見第 0.1 節 |
| 證據 | 主 checkout `build/evidence/gyo2-authority-module/`（別家偵察的報告 `other-engines-authority.md`）；統合的證據 `build/evidence/gyo2-kernel-app-boundary/`（`single-writer-origin-and-pvp.md`、`runtime-interception-other-engines.md`、`rollback-placement-other-engines.md`、`authority-open-source.md`） |

**新舊衝突的規則**照統合第 0 節：較新的為準，寫出較新的有沒有回應較舊的理由。使用者 11-15 對舊架構的話也照用：「不用帶規格，但是但是這個點的設計思想可以帶過來。（新老架構不同，不能照搬。）」

### 0.1 別家（讀原始碼；出處逐條寫在各節）

子 agent（general-purpose，Opus／high，唯讀）讀本機原始碼；報告與卡片的抽查在 `build/evidence/gyo2-authority-module/other-engines-authority.md`。版本：bevy_replicon d80c849、lightyear 736e1695（**這版 lightyear 的實體複製建在 bevy_replicon 0.45.2 上**，`crates/replication/replication/src/lib.rs:1-6`）、Unity Netcode for Entities 7.2.0（非官方鏡像 c4cabf4）、Unity Entities 1.5.0（4630ff5）。GGPO、GekkoNet 本機沒有原始碼，沒有讀。

卡片抽查 5 處：replicon `src/server.rs:279-292`、`src/lib.rs:226-236`，lightyear `checksum.rs:505-511`、`crates/core/utils/src/registry.rs:86-92` 對得上；Unity `GhostPredictionSystemGroup.cs` 的路徑更正為 `Runtime/PredictionTicking/`。

三家共同的事實（下面各節引用）：一、都在**握手或登記階段**擋兩端不一致（斷線或 panic）；二、都**不在執行期擋 Client 寫入**，靠伺服器的值蓋回；三、執行中的 checksum 只有 lightyear 有，不一致**只記 log**；四、tick 在三家都是一般可寫的狀態，**沒有「只有一個來源」的檢查**；五、都沒有 World 內寫者數量的規則。

---

## 1. 統合關聯表的引用

照統合第 1 節。「本份怎麼用」是這份以它為前提做的事，不改它。

| # | 統合的結論（摘；原文以統合為準） | 本份怎麼用 |
|---|---|---|
| L1 | 分層：產品 → 各 Module（權威、時間、狀態機……）→ 編排層 → Kernel；箭頭朝下 | 權威 Module 依賴編排層的登記單位型別與 Kernel；Kernel、編排層不認得權威 |
| L2 | 時鐘與 tick 移到時間的 Module；「tick 只有一個來源、System 不得寫」與「(c) 區的時間只來自 tick」歸伺服器權威策略 | 第 5.6 節設計這兩條在策略裡的檢查形狀 |
| L3 | Kernel 拿掉執行期的攔截；伺服器權威改靠 digest 比對與伺服器覆蓋抓錯（使用者的原話只到這裡；「Kernel 不加觀察點」是卡片的說法，見 L30） | 權威 Module 只有啟動時的檢查與執行中的比對，**不攔寫入**；第 5.9、5.10 節設計比對與覆蓋 |
| L4 | 初始值在建立實體的當下給、不算寫者；伺服器權威只數修改既有值的寫者；「對已存在的實體插入元件算建立還是修改」由權威份定 | 第 5.7 節 |
| L5 | Kernel 提供「開 Registrar」「建 Plan」兩步；編排層 build → 建 Plan → finish → 權威檢查；每個 World 建立時宣告准不准動態插入；伺服器權威策略在啟動時要求它的 World 宣告不允許 | 檢查的時機（第 4.5 節）；執行中插入與 wire id（第 5.8 節） |
| L6 | Kernel 交出唯讀、帶版本號的登記表；參照只在當前版本有效；修改只走 `Registrar`；另交出每種事件的上限與讀者名單、**每個 World 建立時的宣告**（權威份 N4）；統合 3.3 補了 `SystemInfo` 的 effects 與 `ExclusiveWorld` 的 `Writes<…>` 標註 | 策略讀它做檢查與 C 指紋；權威 Module 不改它；5.1 讀 `Writes<…>`、5.3 讀 effects、4.5 讀 World 的宣告 |
| L7 | `ExclusiveWorld` 能寫任何值；`Writes<…>` 留作選填標註、Kernel 不解讀，伺服器權威策略可以拿它數寫者 | 第 5.1、5.11 節 |
| L10 | 指紋改版；C 指紋移權威 Module，用唯讀登記表計算；**跨端比較的指紋不用 Kernel 的編號**，用本份 6.8 選的編號（使用者判）；Registry 與 Plan 指紋只在同一端比 | 第 5.8 節 |
| L11 | 回滾是獨立的 Module、有自己的策略；伺服器權威策略依賴回滾 Module | 回滾的資料登記（Yes／No、Derived、D-E16）在回滾份；「判定資料 × 回滾」的交叉規則在本份（L24，5.14） |
| L12 | 伺服器權威檢查哪些、預設區、C 指紋與 G-5 匯出、digest 比對與伺服器覆蓋、(c) 區獨占套用的新形狀、名字、`AuthorityStrategy` interface 的形狀 | **本份的範圍** |
| L15 | 產品可以只選部分 Module（「要不然單機遊戲也必須上權威了」） | 單機不選權威 Module，就沒有任何寫者、區、digest 的規則 |
| L17 | 兩條核心「Engine 只提供機制」「Kernel 的職責是 ECS」；每層一份職責清單；替換條件＝清單要多加一項或判不出屬於哪一層 | 第 4.1 節的職責清單；第 7 節的 RC |
| L18 | 狀態機 (b) 區已核准；`RegisterStateMachine<S, Zone::C>` 的 `Zone` 隨 L12 | 第 5.11 節給 (c) 區的新形狀；狀態機卡照它改 |
| L20 | Module 之間的依賴只寫在登記檔的「依賴」欄，編排層讀出並檢查；不做策略層級的依賴，權威 Module 整個依賴時間 Module | 第 4.2 節 |
| L22 | 權威的登記與回滾的登記是兩件事實，各自登記；伺服器權威策略可以替作者把權威元件登記進回滾 Module；權威只用回滾 Module 的公開登記 | 4.3、5.14 |
| L23 | 搬運伺服器的值、交換 C 指紋、送 digest、wire id 的呼叫者是 Engine 的網路 Module；本份只定規定，把網路 Module 要的介面列成需求；網路 Module 的寫入 System 用豁免寫者的登記 | 4.8、5.9、5.10、第 8.1 節 |
| L24 | Yes／No 與 Derived（D-E16）歸回滾 Module；Z6、Z8、D-E15 是伺服器權威策略的交叉檢查，讀回滾 Module 的公開登記；作者各自選兩個策略，伺服器權威只檢查相容、不指定；回滾的還原與重跑時退 tick 不算寫者；(3) 權威 Module 提供公開的豁免寫者的登記；(4) 伺服器權威策略替作者把 ingress 登記進回滾 Module 的「重跑時不跑」；(5) 回滾 Module 登記並公開哪些 schedule 可回滾 | 4.8、5.1、5.5、5.14 |
| L25 | 代為登記的 Module 用作者的 `Registrar`，owner 照舊是作者 | 4.3 |
| L26 | 狀態機公開它的登記，伺服器權威策略可以替作者把套用 System 登記成判定的；狀態機不依賴權威 | 4.3（只標記）、5.11 |
| L27 | 時間 Module 公開 tick 的型別與驅動者，驅動者標 `Writes<Tick>`（列為時間宣告的要求） | 5.6 |
| L28 | L4 翻掉 11-14 的 `spawn-writer-funnel`？**待使用者**（統合第 14 節第 1 題） | P6、5.1、6.3、6.7 依賴它 |
| L29 | 回滾策略選在哪一層、哪些 World 可以沒有回滾、選了 Module 沒選策略：**待使用者**（統合第 14 節第 3 題，合併本份 6.2、6.13） | 4.7、5.14 |
| L30 | 排錯用的「誰實際寫的」（11-15 使用者已定保留、另外排一批）要在 Kernel 的寫入關口留記號（只記錄、不擋人）；算不算 Kernel 職責是確認題（統合第 14 節第 17 題） | 和本份不衝突：記號不擋人，權威 Module 不讀它；若做成，digest 不一致時可以拿它查是誰寫的（5.9） |
| L21 | 策略的構造：interface 是契約；官方實作和作者寫的走同一個插口、沒有特權；作者可以從頭實作，也可以繼承官方實作改裝，官方實作的內部不保證不變；C++ 用虛函式的抽象基底類別 | 第 4.4 節 |

---

## 2. 前提表

這份每一個建議都掛在下面的前提上。前提換掉，掛在上面的建議要重看（`premises-before-decisions`）。

| # | 前提 | 誰定（代號） | 掛在上面的 |
|---|---|---|---|
| P1 | 權威是遊戲設計的一部分；作者選策略，策略決定開哪些檢查 | 使用者 11-16（`-authority-is-game-design`） | 全部 |
| P2 | Engine 附的第一版只實作伺服器權威（PvP 射擊）；其他佔位、選了啟動時報錯 | 使用者 11-16（`-authority-strategy-mechanism`） | 4.6、5.x 只為伺服器權威設計 |
| P3 | 權威 Module 自己登記、代為登記進 Kernel；Kernel 不存標籤 | 使用者 11-16（`-authority-own-registration`） | 4.3、5.2、5.3 |
| P4 | Kernel 不攔執行期的寫入；權威靠 digest 與伺服器覆蓋抓錯 | 使用者 11-17（L3） | 5.9、5.10；寫者規則只能是啟動時的靜態檢查 |
| P5 | Kernel 照登記順序平手、不偵測歧義；伺服器權威連線時比 C 指紋 | 使用者 11-16（`-no-ambiguity-detection`） | 5.1（多寫者的先後是決定性的）、5.8 |
| P6 | 初始值在建立當下給、不算寫者——**待 L28**（L4 是否取代 11-14 的 `spawn-writer-funnel`，統合第 14 節第 1 題）；**不當前提** | 使用者 11-17（L4）；11-14 的較早決定相反 | 5.1 的 `Spawn` 列、5.7、6.3、6.7 |
| P7 | 元件與 Resource 預設單一寫者（11-11）——**重新評估的對象**，不是前提 | 使用者 11-11（`v8-arch-kernel-exclusive-default`）；11-15 確認過一次 | 5.1 |
| P8 | 「要保證誰寫，別開 shared」 | 使用者 11-15（`-shared-means-no-owner`） | 5.1、5.4 |
| P9 | 每個 World 宣告准不准動態插入；伺服器權威要求不准 | 使用者 11-17（L5） | 5.8 |
| P10 | 回滾是獨立 Module；伺服器權威策略依賴它；兩個策略作者各自選，權威只檢查相容 | 使用者 11-17（L11）；統合照 L17 判（L24） | 5.10、5.14 |
| P11 | 狀態機 (c) 區兩種形狀都支援（權威端換、Client 採用；兩端同一個 tick 自己換） | 使用者 11-15～16（`-state-machine-zone-c-both-shapes`） | 5.11 |
| P12 | 做 Engine 不把規則釘死：預設方便、限制只放在作者自選的地方 | 使用者 11-17（`-engine-not-rigid`） | 全部；選了伺服器權威就是「作者自選的地方」 |

---

## 3. 先用遊戲講

### 3.1 PvP 射擊的 Match（伺服器，權威端）

作者的產品選了權威 Module，Match 的模擬 World 選「伺服器權威」策略。作者寫：

```cpp
authority.Component<Health>({.name = "Health"});            // 判定用的資料：血量
authority.Component<Position>({.name = "Position"});
authority.Event<Damaged>({.name = "Damaged"});              // 判定用的事件（受傷）
authority.Mark<Tick>();                                     // 時間 Module 已登記的 tick：只標記成判定資料，不重複登記
registrar.Component<HitFlash>({.name = "HitFlash"});        // 一般的資料（畫面用），不經權威
```

權威 Module 記下「`Health`、`Position` 是判定用的資料」，再替作者呼叫 Kernel 的 `Registrar::Component`。Kernel 照常凍結、建 Plan；編排層 finish 之後叫策略檢查：`ApplyDamage` 與 `ApplyHeal` 都寫 `Health` 嗎？讀了畫面用的 `HitFlash` 再寫 `Health` 嗎？不過就啟動失敗、訊息列出是誰。執行中策略不攔任何寫入（L3）。

### 3.2 PvP 射擊的 Client（預測）

Client 有三個 World（模擬、每幀、渲染）。只有**模擬 World** 選伺服器權威——它要和 Match 的模擬 World 跑同一套判定。Client 的模擬 World 另外有只在本機的東西（預測的子彈拖尾、除錯面板），Kernel 的登記表和 Match 不一樣；所以連線時比的是「判定用的那一部分」的指紋（C 指紋），而且不能用 Kernel 的編號（第 5.8 節）。

玩家按下開槍，Client 先預測（自己算傷害、畫特效）。伺服器的結果回來：判定用的資料以伺服器為準——預測錯了就被蓋掉、重算（伺服器覆蓋：把伺服器的值送來並寫進 World 的是網路 Module，L23；重算與平滑在回滾 Module）。網路 Module 寫值的 System 與回滾 Module 的重跑者看起來都像「多一個寫者」，它們經權威 Module 的豁免寫者的登記（4.8）說明自己不算。如果 Client 有一段除錯程式用 untyped `Commands` 偷改了血量，Kernel 不擋（L3）；下一次伺服器的值會蓋掉它；不在伺服器資料裡的判定資料，靠 digest 比對發現兩端不一樣（第 5.9 節）。

### 3.3 格鬥遊戲（P2P 回滾；只佔位）

作者選「P2P 回滾」策略。第一版沒有實作：編排層 finish 時報啟動錯誤「這個策略還沒實作」（P2）。名字與 interface 留著，作者也可以自己寫一個接上（L21）。

### 3.4 單機 2D 橫版

不選權威 Module（L15）。沒有寫者、區、digest 的任何規則；「受傷」與「撿補血包」都直接寫主角的血量，照宣告的先後，沒宣告就照登記順序。

---

## 4. 權威 Module 的形狀

這一節是骨架：位置、依賴、登記的走法、interface 的輪廓、時機。要使用者選的放在第 5、6 節。

### 4.1 職責清單（L17）

**權威 Module 的職責是「讓作者選的一致性策略在啟動時與執行中有機制可用」**。清單：

1. 作者的權威登記（判定用的元件、Resource、事件與它們的屬性），並代為登記進 Kernel（P3）；別的 Module 已登記的型別只標記（4.3）。
2. 策略的插口（`AuthorityStrategy`，L21）與選擇；佔位策略的啟動錯誤。
3. 啟動時的檢查：讀 Kernel 的唯讀登記表（L6）＋自己的登記＋依賴 Module 公開的資訊，回報啟動錯誤（給編排層彙整，O-5）。
4. 一致性的計算：C 指紋（連線時比）、digest（執行中比），以及判定用的來源檔清單（G-5 的後繼，第 5.12 節）。
5. 策略對 World 的要求（例：不准動態插入），在啟動時讀 World 的宣告核對（4.5）。
6. 豁免寫者的登記（4.8，L24 (3)）：公開給回滾 Module、網路 Module、作者自寫的回滾策略。
7. 「判定資料 × 回滾」的交叉檢查（5.14，L24）：讀回滾 Module 的公開登記。

**不在清單上**（照 L17 的預設放別層）：回滾的快照、還原、重算、平滑（回滾 Module，L11）；tick 的存放與推進（時間 Module，L2）；把資料送過網路、握手的傳輸、wire id（網路 Module，L23；需求列在第 8.1 節）；可回滾的資料與 schedule 的登記、Derived 與 D-E16（回滾 Module，L24）；執行期攔截（L3 拿掉）；排程與先後（Kernel）。

系譜（同類型多家，L17）：lightyear 把「複製」「預測（回滾）」「決定性同步」分成三個 crate（`crates/replication/replication`、`crates/replication/prediction`、`crates/deterministic/deterministic_replication`）；Unity Netcode for Entities 是一個套件，但預測（`GhostPredictionHistorySystem`、`GhostUpdateSystem`）與複製（ghost 序列化）分在不同的 System 群組；bevy_replicon 只做伺服器權威的複製（`src/lib.rs:2`）、沒有內建預測（`:629-631`）。權威的標示都在網路套件那一層：replicon 在 `app.replicate*` 與 `Replicated` 元件；lightyear 這版沒有 `HasAuthority`，改用實體上的 `ControlledBy`（`crates/replication/replication/src/control.rs:17-60`）；Unity 是型別上的 `[GhostComponent]` 屬性（`Runtime/Authoring/GhostComponentAttribute.cs:14-39`）與實體上的 `GhostOwner`（`Runtime/Snapshot/GhostOwner.cs:20-51`）。三家都沒有「World 內寫者規則」，那一部分是 GYO 自己的（第 5.1 節）。

### 4.2 位置與依賴（L1、L20）

- 位置：Engine 的 Module（我們自己維護、介面是我們的，`-module-plugin-owner-definition`）。目錄、命名空間、target 照名字（第 6.1 節）定；照 N11～N17 的格式是 `engine/<名字>`、`GYO::Engine::<名字>`、`gyo_<名字>`。
- 依賴（只寫在 Module 登記檔的「依賴」欄，L20）：Kernel（唯讀登記表、`Registrar`）、編排層（登記單位型別、啟動錯誤的回報型別）、**時間 Module**（tick 的 Resource 型別與它的推進者，第 5.6 節）、**回滾 Module**（L11）。
- **不依賴狀態機 Module**：狀態機的值與套用 System 由作者（產品同時認得兩邊）用 `authority.MarkResource("名字")`、`authority.MarkSystem("名字", …)` 以名字標記，對到 Kernel 唯讀登記表裡的名字（4.3）。比過的：權威整個依賴狀態機 Module、讓伺服器權威策略讀狀態機的公開登記代為標記（L26 的「可以」）——代價是照 L20 選了權威的產品都要連狀態機 Module（不用狀態機的 PvP 也是）。選名字標記：不多一條依賴，代價是作者多寫一兩行、名字打錯在啟動時報錯。L26 的「伺服器權威策略可以替作者」因此改成「作者用名字標記」，已在報告裡告訴統合。
- 被誰依賴：產品；之後的網路 Module（握手時要 C 指紋、執行中要 digest、它的寫入 System 要登記豁免；L23、第 8.1 節）。Kernel、編排層、時間、回滾、狀態機都不依賴權威 Module（L22、L26）。
- 單機不選它（L15）；選了 P2P 佔位策略的產品也會連時間與回滾 Module（L20「不做策略層級的依賴」的後果，列出不判）。Match 不回滾卻也連回滾 Module、哪些 World 可以沒有回滾，是 L29（待使用者）。

### 4.3 登記：代為登記進 Kernel（P3）

作者經權威 Module 的登記 API 登記判定用的東西；權威 Module 記下自己的宣告，再呼叫 Kernel 的 `Registrar` 做一般的登記。Kernel 收到的是一般的元件、Resource 與事件，看不出它是判定用的。

```
作者 ──authority.Component<Health>(opts)──▶ 權威 Module
                                              ├─ 記下：Health 是判定用的資料、opts 的權威屬性
                                              └─ registrar.Component<Health>({.name = …}) ──▶ Kernel
```

**登記入口**（暫名；名字在實作批定）：

| 入口 | 用在 | 做什麼 |
|---|---|---|
| `authority.Component<T>`、`authority.Resource<R>`、`authority.Event<E>` | 作者自己的判定資料 | 記下權威屬性，再代為登記進 Kernel |
| `authority.Mark<T>()`、`authority.MarkResource("名字")` | **別的 Module 已登記的型別**：時間 Module 的 tick（L27；權威依賴時間，可以用型別）、狀態機的值（L26；不依賴狀態機，用名字） | 只在權威 Module 記下「它是判定資料」與屬性，**不再呼叫 Kernel**（重複登記是 Kernel 的 R-E2）；權威 Module 在檢查時確認那個型別確實在 Kernel 的唯讀登記表裡，沒有就是啟動錯誤 |
| `authority.System<&F>`、`authority.Ingress<&F>`、`authority.Exclusive<&F>`（隨 6.4 的選擇） | 判定的 System、外部輸入的入口、判定的獨占 System | 記下身分，再代為 `AddSystem` |
| `authority.MarkSystem("<名字>", …)` | 別的 Module 已登記的 System：狀態機的套用 System（L26） | 只記下身分與宣告（例：獨占 System 會跑的 schedule），不再 `AddSystem`；用名字對到唯讀登記表裡的 System，對不到是啟動錯誤 |

- **事件**：現行 C 指紋含 (c) 區的事件（`Fingerprint.cpp:349-356`），Z1 也查事件讀寫；判定的 System 發與讀的事件要是判定事件（5.3）。digest 含不含事件與游標是統合第 14 節第 12 題（6.9）。
- `Registrar` 由編排層在 build 時給（L5）；**權威 Module 用作者的同一個 `Registrar`**，owner 照舊是作者的登記單位（L25；否則 `InitialValue` 的「只有擁有者能宣告」會誤報）。
- **代為登記進回滾 Module**（L22）：伺服器權威策略替作者呼叫回滾 Module 的公開登記，PvP 的作者不寫兩次；權威不讀回滾的內部。範圍與規則（卡片的建議；L22 只定「可以」）：
  - **預設**：經權威登記的判定元件與 Resource，由伺服器權威策略代為呼叫 `rollback.Component<T>`／`rollback.Resource<R>`（可回滾）；**外部輸入的暫存（5.5）一律不代登記**（Z8：輸入不回滾）。
  - **作者怎麼改**：權威登記帶一個 `rollback` 選項——「可回滾（預設）」「不回滾」「Derived」「不代登記（我自己登記）」；這個選項只是轉交給回滾 Module 的登記，事實存在回滾 Module（L24），權威 Module 不另存一份。
  - **和作者自己的登記衝突**：作者已經自己登記進回滾 Module 的型別，策略不代登記（作者的為準）；作者在代登記之後才自己登記同一個型別，由回滾 Module 照它的「重複登記」規則處理（回滾份定），權威 Module 不另外判。
  - 這一層只在選了伺服器權威、而且那個 World 有回滾策略時發生（哪些 World 有回滾，待統合第 3 題）。
  - 所以 5.14 的「判定寫的要能回滾」「判定資料不能在可回滾 schedule 上不回滾」對官方實作也多半是自我核對，只有作者選了「不回滾」或「不代登記」時才會抓到東西。
- 執行中的插入（L5）：伺服器權威要求 World 不准插入，所以伺服器權威下不會發生；作者自寫的策略若允許，插入照同一條路（權威 Module 的登記 → `Registrar`），之後的檢查由那個策略決定。
- 權威登記的屬性有哪些，隨第 5 節的選擇定。第一版最少要有：判定用的資料（是／否；或區）、`Shared` 標記（若第 5.1 節留寫者規則）、進不進 digest（若第 5.4 節留 digest 集合）、`role = Input`（若第 5.5 節留 ingress）。**回滾的屬性（Yes／No、Derived）在回滾 Module 的登記，不在權威登記**（L24）。
- 前例：lightyear 的 `register_component` 同時登記進 ECS（`crates/replication/replication/src/registry/mod.rs:196-200`）；replicon 的 `app.replicate::<C>()`（`src/shared/replication/rules.rs:23`）；Unity 走另一條路——宣告寫在型別上的屬性（`GhostComponentAttribute.cs:14-39`），11-16 比過、使用者選了自己登記。

### 4.4 策略的 interface（L21）

**照 L21**：`AuthorityStrategy` 是虛函式的抽象基底類別（GYO 的 `Plugin` 已是這個寫法，`engine/kernel/include/engine/kernel/Registry.hpp:60-75`）；interface 是契約、保證不變；官方的 `ServerAuthorityStrategy` 和作者寫的走同一個插口、沒有特權；作者可以從頭實作 `AuthorityStrategy`，也可以繼承 `ServerAuthorityStrategy` 改裝，官方實作的內部不保證不變。

輪廓（函式的確切簽名在實作批定；這裡定「interface 承諾哪些時機」）：

| 時機 | interface 的函式（暫名） | 輸入 | 輸出 |
|---|---|---|---|
| **登記當下**（作者呼叫權威的登記入口時；一定在建 Plan 之前） | `OnRegister(entry, proxy)`（暫名） | 這一筆權威登記（型別、屬性、System）；能代為呼叫 Kernel `Registrar` 與回滾、豁免登記的代理 | 策略要做的代為登記：進回滾 Module（4.3）、重跑時不跑（5.5）、豁免（4.8）、標記（4.3） |
| 啟動時（編排層 finish 之後，L5） | `Check(view, errors)` | Kernel 的唯讀登記表（L6，含每個 World 建立時的宣告）、權威 Module 的登記（含豁免寫者）、依賴 Module 的公開登記（回滾、時間） | 啟動錯誤（給 O-5 彙整） |
| 啟動時、連線前 | `ConsistencyFingerprint(view)` | 同上 | C 指紋（第 5.8 節） |
| 執行中 | `Digest(world, …)` | World 的唯讀存取 | digest（第 5.9 節）——**形狀待網路 Module 的宣告，不在這次核准的契約內** |
| 執行中 | `OnMismatch(…)` | 比對的結果 | 要做的事——**形狀待網路 Module 的宣告，不在這次核准的契約內** |
| 任何時候 | `Name()`、`Version()` | — | 進 C 指紋，兩端策略不同就不連 |

- **代為登記為什麼要在「登記當下」**：「重跑時不跑」是掛在 `Mount` 上的執行條件（`Registry.hpp:125-138`），只能在 `AddSystem` 當下給；伺服器權威的 World 不准動態插入（L5），Plan 建好之後補不上；回滾的公開登記在它的啟動檢查之後也不變（回滾份 R-6）。比過的兩種：
  - **interface 加一個 build 階段的時機**（所有登記單位 build 完、建 Plan 之前，策略一次做完代為登記）：要編排層多一個「全部 build 完之後、建 Plan 之前」的通用階段（O-2 的生命週期多一步；編排層不認得權威，所以只能是給所有 Module 的通用階段）——動到編排層，要回報統合。
  - **由權威的登記入口在登記當下交給已選的策略**（本份採這個；卡片照 L17 在權威的職責清單內判，因為不動編排層。若統合第 3 題的答案讓策略無法在登記之前選定，回到上一種並回報統合）：權威的登記入口每收到一筆，就呼叫策略的 `OnRegister`。代價：**策略要在作者第一次呼叫權威登記之前選定**——掛統合第 3 題（策略選在哪一層）。沒選就呼叫登記入口是啟動錯誤（隨第 3 題的「沒選策略怎麼辦」）。
- 「啟動時的檢查」與「執行中的比對」是同一個 interface 的兩組函式。比對的**傳輸**（誰把 C 指紋、digest 送給對方）在網路 Module（L23）。
- 佔位策略（P2P 回滾）實作同一個 interface，`Check` 一律回報「還沒實作」（第 4.6 節）。
- 作者自寫的策略可以什麼都不檢查（等於不用權威 Module 的規則，但留著登記與指紋）。

### 4.5 檢查的時機（L5）

照 L5 的順序：編排層依登記檔 build → Kernel 建 Plan → 依同順序 finish → **權威檢查**。策略的 `Check` 在這一步跑一次。Kernel 有啟動錯誤時不跑（O-5：「因 Kernel 錯誤而未檢查」）。

策略對 World 的要求（伺服器權威：不准動態插入）由**策略在 `Check` 時讀 World 的宣告**核對（L6 交出每個 World 建立時的宣告）。修訂 0 列的另一種「編排層建 World 時比」拿掉：編排層不認得權威策略（統合 O-4）。

### 4.6 佔位策略與它的啟動錯誤（P2）

- Engine 附 `PeerRollbackStrategy`（暫名；P2P 回滾）的佔位：名字與 interface 留著，`Check` 一律回報啟動錯誤，訊息寫明「這個策略還沒實作，可以自己實作 `AuthorityStrategy` 接上」。
- 錯誤的形狀：權威 Module 自己的錯誤碼（不擴充 Kernel 的 `StartupCode`，統合 O-5）。時機：編排層 finish 之後的權威檢查那一步（使用者 11-16「啟動時報」）。
- 佔位清單只有 P2P 回滾一個：使用者點名的例子只有格鬥遊戲（11-16）。

### 4.7 選了權威 Module 但沒選策略

併進 L29（統合第 14 節第 3 題：回滾策略選在哪一層、哪些 World 可以沒有回滾、選了 Module 沒選策略）。本份 6.2、6.13 的選項是那一題的材料。

### 4.8 豁免寫者的登記（L24 (3)、L23）

**例子**：Client 的模擬 World 裡，回滾 Module 的重跑者把 `Position` 還原到第 1200 個 tick 再重跑；網路 Module 的 `ApplyServerState` 把伺服器送來的 `Health` 寫進去；時間 Module 的驅動者每步把 tick 加一，Client 的對時也會調整 tick。啟動時策略看到的是「`Position` 多一個寫者」「一般的 System 寫了判定的 `Health`」——不說明的話，5.1 與 5.3 會擋住。

**形狀**：權威 Module 公開一個登記，`authority.ExemptWriter("<System 的名字>", <它寫的判定資料>, <理由>)`（暫名）：被登記的 System 寫**列出的**判定資料時，策略不把它算寫者，也不套「判定資料只由判定的 System 寫」；寫沒列出的判定資料照常檢查（例：驅動者只豁免 tick）。

| 誰登記 | 登記誰 | 根據 |
|---|---|---|
| **伺服器權威策略代為**（讀回滾 Module 公開的重跑者與還原者名單，回滾份 5.4 第 0 點） | 回滾的還原者與重跑者（含重跑時退 tick 的那一個） | L24：回滾的還原與重跑時退 tick 不算寫者（舊規格 B.3；lightyear 回滾時也推 tick，`crates/core/core/src/timeline.rs:300-313`）。**回滾 Module 不依賴權威（回滾份 5.2、R-3），不能自己登記**；它只公開名單 |
| **伺服器權威策略代為**（讀時間 Module 公開的驅動者與對時 System，L27） | 時間 Module 的驅動者、Client 的對時（若對時會改 tick） | 5.6、6.11 的「驅動者登記豁免」；時間不依賴權威 |
| 作者在產品裡登記（或網路 Module 若依賴權威，自己登記） | 網路 Module 把伺服器的值寫進 World 的 System | L23；網路依不依賴權威由網路 Module 的宣告定（第 8.1 節） |
| **作者在產品裡登記** | 作者自寫的回滾策略的還原者與重跑者 | L24 (3)：同一個登記，沒有特權（L21） |

- 豁免是公開、具名、有理由的：啟動錯誤與 G-5 的報告列出所有豁免，看得到誰免了什麼。
- **豁免不進 C 指紋**：Client 有回滾的重跑者與網路的寫入者，Match 沒有；放進去兩端就永遠對不上。C 指紋只看兩端應該相同的判定部分（5.8）。
- 只豁免「寫者的數法」與「誰能寫判定資料」；被豁免的 System 讀什麼不受限（它本來就是機制，不是判定）。
- 代為登記豁免也在「登記當下」做（4.4）：策略收到選定時，讀回滾與時間公開的名單一次登記。名單在回滾、時間 Module 的 build 時就要公開（它們排在權威之前，L20 的依賴順序）。
- 回滾 Module 的名單怎麼叫、重跑者是哪幾個 System，由回滾份定。

---

## 5. 伺服器權威策略到底檢查什麼（重新評估）

### 5.0 總表

舊的規則都是「Kernel 對每個產品都檢查」寫的；現在只在作者選了伺服器權威時才檢查（P1、P12）。逐條看「當初的理由現在還在不在」。**「建議」欄是卡片的看法，沒有一條經使用者判過**；要使用者判的在第 6 節列選項。

| 舊規則（出處） | 守的性質 | 當初的理由 | 理由現在還在嗎 | 建議 | 判 |
|---|---|---|---|---|---|
| 每欄一個寫者 D-E2／S-11（B.3） | 一個判定資料只有一處寫 | AGENTS §3 狀態的權威；「權威判定，要可回滾、可比對」「多寫者會兩人同時拿到」（K.3 第 3 項，案例全是 PvP） | **部分**：P5 之後多寫者的先後是決定性的（照登記順序、C 指紋比得到），「可回滾、可比對」不再靠它；「合併規則集中在一處」「擊殺歸屬只有一個判定」仍是遊戲設計的理由 | 留，只數判定用的資料 | 5.1、6.3 |
| digest 欄禁 Shared（S-12） | 進 digest 的資料只有一個寫者 | 同上 | 同上 | 隨 5.1 | 5.4、6.6 |
| 每個 System 都宣告區、沒有預設（S-6、G-1） | 不會忘了標 | 「預設 B 讓 (c) 靜默變寬鬆，預設 C 讓呈現層被迫寫順序」 | 部分：11-14 已改成預設 B；現在區可以由別的東西推出 | 改：不叫作者對 System 宣告區 | 5.2、6.4 |
| Z1（(c) 的 System 全部存取都在 (c) 資料上） | 判定只從判定資料算出 | 兩端要算出一樣的結果 | **在** | 留 | 5.3 |
| Z2（(c) 資料只由 (c) 的 System 或 hook 寫） | 同上 | 同上 | **在** | 留（和 Z1 合成一條，看 5.2 的選擇） | 5.3 |
| Z3（ingress 可寫 `role = Input` 的 (c) 資料，在 ingress stage）、S-17 | 外部輸入只從一個入口進 | PvP 的輸入以 tick 為鍵 | 在，但「ingress stage」的 stage 名是 Kernel 已拿掉的 `ingressStage` | 留「入口」的性質，形狀改 | 5.5 |
| Z4：S-7 不用 `Local`、S-8 不宣告 Io effect、S-9 不用 `Changed`／`Added`、S-13 不用 `ExclusiveWorld` | 判定不依賴 World 外的狀態與 stamp | S-7、S-8：回滾重跑時殘留或重複；S-9：兩端 stamp 毫無關係（G-3）；S-13：寫入看不見 | S-7、S-8、S-9：**在**；S-13：L7 之後由 `Writes<…>` 補，狀態機需要 (c) 的獨占 System | S-7、S-8、S-9 留；S-13 改 | 5.3、5.11 |
| Z5（(b) 不得 despawn 帶 (c) 欄的實體） | 判定的實體不被一般的程式刪掉 | 執行期檢查 | L3 拿掉 | 拿掉；改由 digest 抓 | — |
| Z6、Z8、D-E15（判定資料 × 回滾） | 回滾重跑時判定正確 | 回滾 | **在** | 留，成為伺服器權威策略的交叉檢查，讀回滾 Module 的公開登記（L24） | 5.14 |
| D-E16（Derived 的順序）、可回滾的 Yes／No | 回滾的資料本身 | 回滾 | 在 | 回滾 Module 的登記與檢查（L24），不在本份 | — |
| 漏斗 B1～B3（11-14 `funnel-block-b1-b3-fingerprint-a`） | 漏斗不能繞過區與回滾 | 「B1–B3 要擋」 | 在 | B1、B2 由 5.3＋5.5 涵蓋；B3 併進 Z6 的交叉檢查；「宣告進指紋」隨 C 指紋（5.8） | 5.14、6.14 |
| Z7（(c) 不寫 external） | 外部資料不進判定 | 同 Z1 | `external` 移出 Kernel；Z1 已涵蓋 | 併進 Z1 | — |
| D-E9（(c) 的 hook 衝突） | hook 沒有 before／after，兩端可能先後不同 | 歧義 | P5：照登記順序平手、C 指紋比得到 | 拿掉（確認清單 6.14） | 5.3 |
| K.3 第 5 項（讀 (c) 的 (b) System 要和 (c) 寫者有顯式順序） | 呈現讀到的是同一 tick 的值 | 歧義 | P5 | 拿掉（不是錯誤；確認清單 6.14） | — |
| X-2、D-E11、X-E1（含 (c) 的 schedule 宣告 Parallel 要守衛測試） | 平行執行不改結果 | 決定性 | Kernel 的 D-E17 讓 v1 任何 Parallel 都是錯誤 | 留在策略、目前不會觸發 | — |
| 「tick 只有一個來源、System 不得寫」（DI-KS 8）、「(c) 的時間只來自 tick」（K.3 第 11 項） | 時間不讓兩端分岔 | PvP | L2 已定歸伺服器權威策略 | 改寫成 5.1＋5.3 的特例 | 5.6、6.11 |
| C 指紋（`Fingerprint.cpp:330-406`）、G-5 兩份匯出 | 連線時兩端的判定程式相同；判定程式碼的來源可掃描 | `-no-ambiguity-detection` | 在 | 留；編號改 | 5.8、5.12 |
| `RejectedSpawn::emitterZone`（`World.hpp:86`） | 被丟的生成的明細帶區 | 11-15 | 統合 3.2：隨區移出、權威份定替代 | 權威 Module 用明細裡的 `emitter` 名字自己查（確認清單 6.14） | 5.13 |

### 5.1 寫者規則（D-E2 的後繼）

**例子**：PvP 射擊。`ApplyDamage` 寫血量；同時有人加了 `ApplyHeal` 也直接寫血量。兩個寫者時，同一個 tick 先扣後補或先補後扣，照登記順序決定（P5），兩端一樣；但「受傷與補血誰先」是登記順序決定的，作者沒寫在任何地方。單一寫者的檢查逼作者把兩者寫成事件、由唯一的 `ApplyHealth` 套用合併規則（舊規格 B.3.1 的漏斗）。另一個例子：Boss 擊殺歸屬——兩個 System 都能寫「誰擊殺」，兩人可能都拿到。

**別家**：四家 ECS 都沒有寫者數量的規則（`single-writer-origin-and-pvp.md`；Bevy、Unity 反向：准多寫、Unity 有 WriteGroups）；三家網路套件都沒有 World 內的寫者規則，權威是「機器之間」——伺服器寫、Client 收（replicon 的 Client 只送 ack 與 message，`src/shared/backend/channels.rs:158-169`；Unity 的 Client 只送輸入與 RPC，`Runtime/Command/CommandSendSystem.cs:27-45`）。GYO 的單一寫者是 11-10 架構師的提案、11-11 使用者核准，案例全是 PvP（`single-writer-origin-and-pvp.md`）。

**這條在新架構裡守的是什麼**：P5 之後，多寫者的結果在兩端仍然一樣（登記順序相同、C 指紋比得到）。所以它**不再是決定性或可比對的必要條件**；它守的是遊戲設計的一條紀律：「判定只有一處寫，合併規則看得到」（P8：「要保證誰寫，別開shared」）。

**數到哪些寫者**（不論選哪個選項，數法都要定；卡片的建議，沒經使用者判）：

| 寫法 | 數不數 | 根據 |
|---|---|---|
| `Write<T>`、`ResMut<T>` | 數 | 啟動時看得出（統合 7.3） |
| 型別化 Commands 的 insert（對已存在的實體） | 隨第 5.7 節（依賴 L28） | — |
| 型別化 Commands 的 remove | 數（成員關係結束之外，它讓值消失） | 舊 B.3 |
| `Spawn<Bundle<…>>`、`SpawnWithId<…>`、`InitialValue` 的值 | **待 L28**：照 L4 是「不數」；照 11-14 的 `spawn-writer-funnel` 是「數」 | 統合第 14 節第 1 題；P6 不當前提 |
| `Despawn` | 不數 | K.3 第 4 項（11-11）；舊 Z5 拿掉後，誤刪判定的實體靠 digest 抓 |
| hook 宣告寫 `T` | 數 | 舊 B.3 |
| `ExclusiveWorld<Writes<T…>>` 列出的 | 數 | L7；從唯讀登記表讀標註（統合 3.3 已補） |
| `ExclusiveWorld` 沒列的寫入、untyped `Commands`、從外面寫 | 看不到，不數 | L3、L7；靠 digest 與伺服器覆蓋 |
| 回滾 Module 的還原者與重跑者（含重跑時退 tick）、網路 Module 寫伺服器值的 System、作者自寫回滾策略的還原者 | 不數——**經豁免寫者的登記**（4.8）策略才認得出來 | L24、L23；舊 B.3「Runtime 的回滾還原不算」 |

**範圍**：只數**經權威登記的資料**（判定用的資料）。一般的資料（`HitFlash`、UI）不數——單機與畫面層不受綁（P12）。

選項在第 6.3 節。

### 5.2 System 的區怎麼來（S-6 的後繼）

舊的做法：每個 System 自己宣告 `zone = C／B`（S-6）；11-14 改成不填視為 B（`-zone-default-b`）。L3 之後 System 的登記照常走 Kernel 的 `Registrar`（`AddSystem`），P3 只說資料經權威登記。

**例子**：`ApplyDamage` 讀 `Position`、寫 `Health`，是判定；`DrawHealthBar` 讀 `Health`、寫 UI，是畫面。要不要讓作者對每個 System 說「我是判定」？

**別家**：沒有一家對 System 標「決定性區」。最接近的是「群組或 schedule 的成員」——Unity 的 `PredictedSimulationSystemGroup`（被預測的 System 放進這個群組）、lightyear 的 `FixedMain`（回滾只重跑它，`crates/replication/prediction/src/rollback.rs:78-81`、`:1268-1275`）。Unity 的群組是「重跑哪些」，不是「誰能寫」：沒有「預測的元件只能由某群組寫」的啟動檢查，只有文件（`Runtime/PredictionTicking/GhostPredictionSystemGroup.cs:162-203`；`CommandSendSystem.cs:31-34`）。

選項在第 6.4 節。三種都滿足 Z1、Z2；差在作者要寫什麼、錯的時候訊息在哪。

### 5.3 留下的區規則（Z1、Z2、Z4 的一部分）

不論第 6.4 節選哪一種，伺服器權威策略的區規則收成兩條（卡片的建議；Z1、Z2 的理由在，形狀照 L3、L7 改）：

1. **判定只從判定資料算出**（Z1＋Z2＋Z7）：判定的 System 或 hook，啟動時看得到的全部存取（讀、寫、With／Without、事件、延後寫、run 條件、具名集合展開）都在判定資料上；判定資料只由判定的 System、hook、或第 5.5 節的入口寫。
2. **判定不依賴 World 之外與 stamp**（S-7、S-8、S-9）：判定的 System 沒有 `Local<T>`、不宣告 Io effect（`Diagnostics` 例外照舊；effects 從唯讀登記表的 `SystemInfo` 讀，統合 3.3 已補）、不用 `Changed`／`Added`。理由照舊在：`Local` 不在 World、回滾不還原；Io 在重跑時重複；兩端的 stamp 毫無關係（G-3）。

拿掉的：D-E9（hook 衝突）——P5 之後 hook 照登記順序，C 指紋含 hook 的登記順序就比得到；K.3 第 5 項（(b) 讀 (c) 的順序）——同理，不是錯誤。兩條都是從既定決定推出來的，列進確認清單（6.14），不當成已定。

漏斗（`InitialValue`）的「寫請求的人在區的檢查裡當作也寫了目標」（`-funnel-one-principle`，11-14）照搬到策略：它是 Z1／Z2 的推導，不是新規則。

**規則 1 對「生成」怎麼算**：現行 Z2 對所有寫入都算，包括 `Spawn` 帶的值（`Registry.cpp:1283-1290`）；開局的豁免只在數寫者那裡（`:1448-1457`）。所以生成世界或讀檔的一般 System 讀地圖檔、生成帶 `Health` 的敵人時，照現行會被規則 1 擋（一般的 System 寫了判定資料）。L4、L28 只講寫者的**數量**；「**誰能寫**」是另一半，統合第 1 題分開問。兩種讀法：生成帶的值也受規則 1 管（生成判定實體的 System 必須是判定的，讀的地圖資料也要是判定資料或經入口進來）；或生成帶的值不受規則 1 管（初始狀態從哪來都可以，兩端初始狀態不同由 digest 抓）。本份不判。

### 5.4 digest 禁 Shared（S-12）

**例子**：血量進 digest、又標 `Shared` 讓 `ApplyDamage` 與 `ApplyHeal` 都寫。

當初的理由與 5.1 相同。P5 之後兩寫者的結果兩端仍一樣，digest 比得到；所以 S-12 守的也是「判定只有一處寫」的紀律，不是比對的必要條件。它和 5.1 綁在一起：5.1 不數寫者，S-12 就沒有意義。選項在第 6.6 節。

### 5.5 外部輸入的入口（Z3、Z8 的一半、S-17）

**例子**：Match 收到玩家的按鍵封包，寫進「第 1200 個 tick 的輸入」；判定的 `ApplyInput` 從那裡讀。封包到的時刻（牆鐘）只決定寫進哪一格或因過期丟掉，不改判定。

舊的形狀：`role = Input` 的 (c) 資料、`ingress = true` 的 (b) System、schedule 的 `ingressStage`（Z3）、ingress 不能是獨占 System（S-17）、Input 不回滾（Z8）。統合把 `ingress`、`ingressStage`、`Role::Input` 都移出 Kernel（3.2）。

新形狀（卡片的建議）：入口是權威登記的屬性——`authority.Resource<InputRing>({.input = true})` 標記「外部輸入的暫存」，`authority.Ingress<&StageInput>(…)` 登記入口 System（權威 Module 代為呼叫 `AddSystem`）。策略檢查：只有入口 System 寫輸入的暫存；入口 System 不能是獨占的（S-17）。**不再用 stage 名檢查**：stage 是 Kernel 與編排層的，權威 Module 不認得 stage 名。要不要改用順序圖檢查「入口在讀者之前」，見 6.15。「Input 不回滾」（Z8）與「入口在重跑時不跑」（D-E15）是 5.14 的交叉檢查；伺服器權威策略替作者把 `authority.Ingress` 登記的入口登記進回滾 Module 的「重跑時不跑」（L24 (4)）。「入口排在哪裡要不要檢查」：入口不進 C 指紋（5.8），修訂 2 起是選項（6.15）。

**別家**：Unity Netcode 的輸入只准在 `GhostInputSystemGroup` 寫（`CommandSendSystem.cs:27-45`；只是文件的規定，`:31-34`，沒有啟動檢查）。



### 5.6 tick 只有一個來源、(c) 的時間只來自 tick（L2）

**例子**：Match 每 15.625 ms 推一個 tick。判定的 `Cooldown` 用「第幾個 tick」算冷卻；如果有人讀了牆鐘或每幀的 `deltaTime`，兩端的冷卻就會差一點。

L2 之後 tick 是時間 Module 的 Resource（存放在時間 Module 那一批定），由時間 Module 的固定步驅動者（一個獨占 System，`-fixed-step-in-world`）推。

**驅動者過不了規則 1**（第二輪交叉檢查第 4 條）：tick 若是判定資料，規則 1 要求它只由判定的 System 寫；但驅動者不經權威登記（時間不依賴權威），又必然讀真實時間與累加器（回滾份 7.2）——當成一般 System，它「寫了判定資料」；當成判定的 System，它「讀了一般資料」；當成判定的獨占 System，5.11 第 3 點又要求它跑的固定步 schedule 裡全是判定的 System，可是入口（5.5）也在那張上。Client 的對時若會改 tick，同一個問題。

三種過法（選項在 6.11）：

- **驅動者登記豁免**：伺服器權威策略讀時間 Module 公開的驅動者與對時 System（L27），代為登記豁免，只豁免 tick（4.8）。tick 仍是判定資料；「只有一個來源」＝除了豁免的驅動者與對時，沒有別的寫者。驅動者不是判定的 System，5.11 第 3 點不適用它。
- **`MarkSystem` 加例外**：把驅動者標成判定的獨占 System，另寫兩個例外：它可以讀累加器與真實時間、它跑的 schedule 可以有非判定的 System。
- **專用規則**：tick 不當一般的判定資料；另立「tick 只由時間 Module 公開的驅動者與對時寫」，並在規則 1 開一個例外「判定的 System 可以讀 tick」。

卡片的看法：(c) 的時間只來自 tick 的另一半照舊是 5.3 的特例——

- **tick 只有一個來源**：照 6.11 選的過法。驅動者用 `ExclusiveWorld<Writes<Tick>>` 標註（L7、L27），策略讀得到它寫 tick。
- **(c) 的時間只來自 tick**＝別的時間 Resource（每幀的時間、牆鐘的取樣）不是判定資料；Z1 讓判定的 System 讀不到它們。被記錄的外部輸入（封包到達時刻）走 5.5 的入口。
- 函式體裡直接呼叫牆鐘（`std::chrono`、`Time::Now`）啟動時看不到，照舊靠判定程式碼的來源掃描（5.12）與 digest。

**別家**：三家的 tick 都是一般可寫的狀態，**沒有「只有一個來源」的檢查**——replicon 的 `ServerTick` 是 Resource、公開 `increment`，也可以關掉自動遞增改手動（`src/server/server_tick.rs:22-36`、`src/server.rs:67-73`）；lightyear 的 `LocalTimeline` 是 Resource、公開 `apply_delta`，回滾時也會推（`crates/core/core/src/timeline.rs:212-241`、`:300-313`）；Unity 的 `NetworkTime.ServerTick` 是 singleton 的公開欄位，伺服器與 Client 由不同的東西寫（`Runtime/PredictionTicking/NetworkTime.cs:46-73`）。所以這條是 GYO 自己的；它放在作者選的伺服器權威策略裡，合 P12。注意 lightyear 回滾時推 tick 的例子：若 GYO 的回滾 Module 也要在重算時推 tick，「回滾的還原與重算不算寫者」（5.1）要涵蓋它——統合已判不算（L24），經豁免寫者的登記（4.8）。

怎麼讓策略知道「哪個 Resource 是 tick」：時間 Module 公開 tick 的型別與驅動者，驅動者標 `Writes<Tick>`（L27，列為時間宣告的要求）；作者（或伺服器權威策略代為）用 `authority.Mark<Tick>()` 把它標成判定資料（4.3），不重複登記。時間 Module 不依賴權威。選項在第 6.11 節。

「tick 在快照內」（DI-KS 8、12 的另一半）屬回滾 Module（L24）。

### 5.7 對已存在的實體插入元件：建立還是修改（L4 交下來的）

**例子**：PvP 射擊。玩家撿到護盾，`PickupShield` 對玩家實體插入 `Shield{100}`；另一個 `ShieldSkill` 技能也對玩家插入 `Shield{50}`。玩家已經有護盾時，插入會**蓋掉**舊值（Bevy 的 `EntityCommands::insert` 文件：「This will overwrite any previous value(s) of the same component type」，另有 `insert_if_new` 保留舊值，`crates/bevy_ecs/src/system/commands/mod.rs:1381-1384`，6a911653f）。

- 算**修改**：兩個 System 插入 `Shield` 就是兩個寫者，單一寫者時要走漏斗（各自發事件、`ApplyShield` 決定合併）。
- 算**建立**：插入不算寫者，任意多個 System 可以插入；「蓋掉舊值」那一刻沒有被數到。

啟動時的檢查看不到「插入時目標有沒有這個元件」（那是執行期的事，L3 不攔），所以不能「沒有時算建立、有時算修改」——只能二選一。

**別家**：replicon 的協議分「插入、移除、despawn」（可靠、有序）與「修改」（不可靠）兩條 channel（`src/shared/backend/channels.rs:130-141`），但**伺服器對已有這個元件的實體插入，Client 當成修改**——可變的元件在 Client 已存在就改值、不存在才插入（`src/lib.rs:226-236`，原文「the client will treat it as a mutation」）；要分開，作者把元件標成不可變。lightyear 同 replicon。Unity 的快照不分插入與修改，會複製的元件在 prefab 製作時固定，執行中新加的不複製（`Documentation~/ghost-snapshots.md:48-65`）。Bevy 的 `on_insert` 在插入與取代時都觸發、`on_add` 只在新加時觸發（統合證據 `runtime-interception-other-engines.md` 引 `lifecycle.rs`）。

選項在第 6.7 節。

### 5.8 執行中插入、wire id 與 C 指紋用的編號（L5、L10）

**L5 已定**：伺服器權威要求它的 World 宣告不准動態插入（P9）。所以伺服器權威下，啟動後判定的 System 與資料不會變；C 指紋在啟動時算一次。這和別家一致：replicon、lightyear 在登記完成後再登記會 panic（下面的出處），Unity 的 RPC 也是（`RpcCollection.cs:141-142`）；Unity 唯一允許執行中加的是 ghost 型別，靠「伺服器送清單與雜湊、Client 不合就斷線」維持一致。作者自寫的策略若允許插入，就要自己做類似 Unity 的重新比對（不在第一版）。

**修訂 0 的新發現（N3；統合已補進 L10）**：現在的 C 指紋寫的是 Kernel 的編號（`Fingerprint.cpp:340-352` 的 `out.U32(id)`；`ComponentId` 照登記順序配）。Client 的模擬 World 有 Match 沒有的一般資料（預測的特效、除錯面板），**只要一般資料的登記插在判定資料之前，同一份判定資料在兩端就會拿到不同的 Kernel 編號**，C 指紋就不同、連不上——即使判定完全一樣。所以 C 指紋（和之後網路用的 wire id）不能用 Kernel 的編號。

**例子**：Match 先登記 `Health`（編號 0）；Client 先登記 `HitFlash`（編號 0）再登記 `Health`（編號 1）。

**別家**：
- replicon：wire id＝登記順序（`src/shared/replication/registry.rs:111-124`）；協議雜湊算登記的種類＋型別名＋**順序**（`src/shared/protocol.rs:13-20`、`:133-147`），連線時伺服器比、不同就斷線（`src/server.rs:279-292`）；build 完成後再登記會 panic（雜湊器在 finish 被移除，`src/shared.rs:123-130`）。
- lightyear：message／channel 依登記順序配 id、型別名依順序雜湊、finish 後再登記 panic 並寫明「Tried to register type … after the protocol was finished」（`crates/core/utils/src/registry.rs:36-46`、`:86-92`）。**反面教材**：這版元件的協議雜湊從來沒有被餵任何型別（`crates/replication/replication/src/registry/mod.rs:166`、`:211`），等於常數——指紋要有測試證明「改了登記就會變」（replicon 在 `protocol.rs:277` 釘死預期值）。
- Unity：元件依型別雜湊排序（`Runtime/Snapshot/GhostCollectionSystem.cs:51-65`），編號和登記順序無關；元件集合的雜湊含欄位的序列化屬性（`:134-150`），握手時比、不同就斷線（`Runtime/Rpc/RpcSystem.cs:266-273`）。ghost 的型別可以在執行中加，伺服器把清單與雜湊送過去，Client 發現同一個編號的雜湊變了就斷線（`GhostReceiveSystem.cs:470-486`）。

舊 GYO 的 N2：「元件在 wire 上的 id 是登錄順序索引……GYO 用 protobuf 欄位號＝契約」（`docs/architecture/plans/kernel/networking.zh-Hant.md:30`）。

選項在第 6.8 節。

**wire id 本身**：歸網路 Module（L23）。權威 Module 只定「C 指紋用的穩定編號」（6.8，使用者判）；網路 Module 可以沿用它（第 8.1 節的需求）。判定資料與要複製的資料不一定同一組：有些判定資料不複製、有些複製的資料不是判定。

**C 指紋裡有什麼**（第二輪交叉檢查第 5 條；卡片的建議，標「待判」的跟著使用者的題目）。判準：**兩端應該相同、而且會改變判定結果的才進**；只在一端有的不進，否則永遠連不上。

| 項目 | 進／不進／待判 | 理由 | 現行（`Fingerprint.cpp`） |
|---|---|---|---|
| 策略的名字與版本 | 進 | 兩端策略不同就不該連 | 沒有 |
| 判定資料的型別清單（穩定編號、名字、權威屬性） | 進；編號待判（6.8） | 判定資料不同就不能比 | 含 zone C 的元件、Resource、事件，用 Kernel 編號（`:338-356`） |
| 判定 System 與 hook 的存取 | 進 | 存取不同，結果可能不同 | 含（`:357-368`） |
| 判定 System 在各 Plan 的相對順序與其間的同步點（精簡成只剩判定步驟與同步點；以 schedule 名對應） | 進 | P5：多寫者、hook、事件的先後靠登記順序，兩端靠這一項保證相同 | 含精簡後的步驟（`:382-405`，精簡在 `:398`） |
| 判定 hook 的順序 | 進 | 同上（D-E9 拿掉的前提） | 含 hook 的登記順序（`:361-368`） |
| `InitialValue`（目標是判定資料的） | 進 | 同步點上 Kernel 替遊戲寫判定資料 | 含（`:370-378`） |
| schedule 的 stage 名單 | **不進** | Client 可能為畫面多一個 stage；stage 只經同步點影響結果，同步點已在上面 | 含（`:391`） |
| hook 清空輪的上限 | 進 | 改變判定 hook 跑幾輪 | 含（`:393-394`） |
| 判定的獨占 System 宣告會跑的 schedule 清單（5.11） | 進 | 巢狀的組成兩端要一樣（K0 宣告第 9 節的限界） | 沒有 |
| 入口（ingress） | **不進** | Match 的入口讀網路、Client 的入口讀本機輸入，常常不是同一個 System | zone B，不在裡面（`:357-360`） |
| 外部輸入暫存的型別 | 進（它是判定資料） | 判定 System 讀它 | 含（zone C、role Input） |
| 判定事件的上限 | 進 | 滿了丟紀錄會改變判定結果 | 沒有（現行沒有上限，游標份新增） |
| 回滾的登記（可回滾的資料與 schedule） | **不進** | Match 不回滾、Client 回滾，進了永遠連不上；回滾只在 Client 那一端影響預測 | 含 `rollbackEnabled`（`:390`），照搬就連不上 |
| 豁免寫者 | **不進** | 只在 Client 有（4.8） | 沒有 |
| 時間 Module 的步長等設定 | 待判 | 會改變判定結果，但它是 Resource 的值、不是登記；放指紋或放網路握手，由網路 Module 的宣告定 | 沒有 |

這張表改寫了 6.14 第 1、2 條的理由（「C 指紋比得到」指的是上表的「相對順序」與「hook 的順序」兩列），也讓 6.14 第 3 條（入口的 stage）站不住——入口不進指紋，兩端各自排錯位置比不到，所以那一條回到選項（6.15）。

### 5.9 digest 比對：何時比、比什麼、不一致怎麼辦（L3）

L3 已定：伺服器權威靠 digest 比對與伺服器覆蓋抓錯。這一節定 digest 的部分。

**例子**：Client 的除錯面板用 untyped `Commands` 把「冷卻剩餘」改成 0（它是判定資料，但伺服器不送，Client 自己預測）。伺服器覆蓋碰不到它，Client 的冷卻和伺服器分岔；下一發子彈 Client 以為能開、伺服器不准。digest 比對抓這種「覆蓋碰不到的分岔」。

**在伺服器權威裡 digest 的位置**：伺服器送來的資料會蓋掉 Client 的（5.10）；所以 digest 主要抓兩種：(1) 不在伺服器資料裡的判定資料分岔；(2) 回歸測試——同一串輸入，判定的結果和上次一樣（舊 PvP 的 `AuthorityDigest` 就是這種：情境×tick 的 golden，`tests/object_fps_pvp/AuthorityDigest.cpp`，master）。

**別家**：
- lightyear（`crates/deterministic/deterministic_replication/src/checksum.rs`）：只算掛了 `Deterministic` 標記的實體、只算登記了雜湊函式的元件，各自雜湊後 XOR 合併（`:6-7`、`:312-354`）；Client 算「最後確認輸入」那個 tick 的值、經不可靠的 channel 送（`:206-278`），伺服器每個 tick 算、留 30 tick（`:450-493`）；P2P 時互比（`:94-139`）。**不一致只記 error**，不回滾、不斷線、不重新同步（`:508-511`、`:400-408`）。
- Unity Netcode：沒有狀態的 checksum；只有除錯版的逐欄預測誤差統計（`Runtime/Snapshot/GhostPredictionDebugSystem.cs:3-10`）。
- replicon：沒有（子 agent 的掃描沒先用正例驗證，只當「沒找到」）。
- GGPO、GekkoNet：本機沒有原始碼，沒有讀；統合的證據（`authority-open-source.md`）只記到 GekkoNet 有 `GekkoDesyncDetected`。

**權威 Module 提供的**（卡片的建議）：`Digest(world, …)` 的計算（策略決定範圍）；比對的結果交給策略的 `OnMismatch`。**誰傳、何時傳**是網路 Module 的事（L23，第 8.1 節）；第一版沒有網路，digest 只用在回歸測試（同一個 World 跑兩次、或 golden）。

**比什麼**是統合第 14 節第 12 題（含不含事件與游標；游標份 L13 (4) 把「digest 含不含游標」交本份；回滾的歷史若存在 World 裡要不要排除，掛第 11 題）。**何時比、不一致怎麼辦**延到網路 Module 的宣告（統合第 14 節「不問的」）；6.9 的那兩張表留作那份宣告的材料。

### 5.10 伺服器覆蓋（L3）

**例子**：Client 預測自己往前走了 3 格；伺服器說只走了 2 格（撞到牆）。Client 的位置被改成伺服器的值，再從那個 tick 用之後的輸入重算到現在，畫面平滑地拉回去。

這件事分三段，各屬不同的 Module：

| 段 | 做什麼 | 屬於 |
|---|---|---|
| 規定 | 哪些資料以伺服器為準、方向（伺服器 → Client） | **權威 Module**（伺服器權威策略：判定資料一律以伺服器為準） |
| 搬運 | 把伺服器的值送過來、寫進 Client 的 World | 網路 Module（L23）；它的寫入 System 經豁免寫者的登記（4.8） |
| 重算 | 預測的資料：比對歷史、還原、重跑、平滑 | 回滾 Module（L11：狀態觸發、逐元件歷史、平滑修正） |

這份只定第一段：**伺服器權威策略的判定資料以伺服器為準**；覆蓋與回滾的寫入不算寫者（5.1，經 4.8 認得出來）。搬運歸網路 Module（L23），重算歸回滾 Module（L11、L24）。

**別家**：三家都**沒有**擋 Client 寫的地方。差在蓋回的時機：Unity 每收到一份新快照，預測的 ghost 一律從快照還原再重跑、還原前不比對（`Runtime/Snapshot/GhostUpdateSystem.cs:25-50`），所以 Client 偷改的值撐不過下一份快照；replicon 靠伺服器的變更偵測決定送什麼（`src/lib.rs:228-243`），伺服器那邊沒變就不送，Client 偷改的只接收元件可能長時間不被蓋（子 agent 的推論）；lightyear 預測的元件比對確認值與歷史、不同才回滾（`crates/replication/prediction/src/registry.rs:590-613`）。所以**「伺服器覆蓋」抓得到多少錯，取決於搬運那一段是「每次無條件蓋」還是「伺服器有變才蓋」**——那一段在網路 Module（L23），但會決定 5.9 的 digest 要補多少。

### 5.11 (c) 區的獨占套用：新形狀（L12、L18）

**例子**：天氣在第 3600 個 tick 起霧（P11 的「兩端同一個 tick 自己換」）。狀態機的套用是一個獨占 System：跑離開的 schedule、改天氣的值、跑進入的 schedule。

**舊的**（`-zone-c-exclusive-apply`，11-15～16）：Kernel 加「(c) 區的獨占套用」——允許 (c) 的獨占 System；登記時列出它要寫的 (c) Resource 與它會跑的幾張 schedule；啟動時檢查它只跑列出的那幾張；C 指紋收進。回收清單說這個建立在 S-13 上，「這一批可能不需要或形狀不同」。

**現在**：L7 讓 `ExclusiveWorld` 能寫任何值，Kernel 不需要為 (c) 加任何東西。剩下的全在權威 Module（卡片的建議）：

1. 判定的獨占 System 經權威登記（`authority.Exclusive<&ApplyWeather>(…, {.runs = {"OnEnter_Fog", …}})`，權威 Module 代為 `AddSystem`），宣告它會跑的 schedule。
2. 它寫的判定資料用 `ExclusiveWorld<Writes<…>>` 列出（L7），算寫者（5.1）。
3. 策略把它當判定的 System 套 5.3：它列出的 schedule 裡的 System 也都要是判定的（跑判定的 schedule 的人自己是判定）。時間 Module 的驅動者不是這一種（5.6）。
4. 「只跑列出的那幾張」**執行期不檢查**（L3）；列出的清單進 C 指紋，兩端清單不同就連不上；列了沒照做由 digest 抓。這也回答 K0 宣告第 9 節的限界「巢狀的組成不在 C 指紋裡」——列出的部分進 C 指紋。

**和使用者 11-15～16 同意的不一樣**：`zone-c-exclusive-apply` 同意的是「Kernel 加 (c) 區的獨占套用、啟動時檢查它只跑列出的那幾張」。新形狀是 Kernel 不加、清單進 C 指紋、執行期不核對——是由 L3（拿掉攔截）與 L7（`ExclusiveWorld` 能寫任何值）推出來的，列進確認清單（6.14），不當成已定。

「權威端換、Client 採用」那一種不需要獨占的 (c) 套用：它是伺服器覆蓋（5.10）。

狀態機卡的 `RegisterStateMachine<S, Zone::C>`：照 L26，狀態機公開它的登記、不依賴權威；權威也不依賴狀態機（4.2），所以由**作者**在選了伺服器權威的產品裡，用 `authority.MarkResource("名字")` 把狀態機的值標成判定資料、用 `authority.MarkSystem("名字", {.runs = …})` 把狀態機已登記的套用 System 標成判定的獨占 System，宣告它會跑的 schedule（4.3）。`Zone::C` 這個參數拿掉（回收清單 1.2 已列「不帶進來」）。細節在狀態機 (c) 區的批。

### 5.12 G-5 的兩份匯出

舊 G-5：啟動時輸出「(c) 區資料集」與「(c) 區 System 的來源檔清單」；後者給守衛掃描（J-17：判定的程式碼不 include `<chrono>`、`<random>`，不用全域可變狀態……）。現在的實作：`FrozenRegistry::ExportZoneCSources`（`engine/kernel/src/Fingerprint.cpp:458`）。

統合 3.2 把它移到權威 Module。卡片的看法：**留**——L3 拿掉執行期攔截之後，「函式體裡讀牆鐘」只剩這個掃描與 digest 能抓；它便宜（啟動時一份清單）。權威 Module 用唯讀登記表裡的 `source`（`Registry.hpp:473-483` 已有）加自己的登記算出。資料集那一份就是 C 指紋的可讀版本。

### 5.13 `RejectedSpawn::emitterZone` 的替代

統合 3.2：「明細帶區」隨區移出，權威份定替代。明細已經有記錄者的名字（`RejectedSpawn::emitter`，`World.hpp:84`）。卡片的看法：Kernel 拿掉 `emitterZone`，**不加替代欄位**；權威 Module 需要時用 `emitter` 名字查自己的登記（是不是判定的 System）。LIM-7 的文字跟著改：「(c) 區的生成」改成「判定的 System 的生成」，Kernel 不擋這一點不變（LIM-7 的 owner 是架構師會話，改在減法批）。 「不給替代」是卡片從 L3 推的，列進確認清單（6.14）。

### 5.14 判定資料 × 回滾的交叉檢查（Z6、Z8、D-E15、B3；L24）

**例子**：PvP 的 Client。`ApplyDamage` 是判定的 System，掛在可回滾的固定步 schedule 上，寫 `Health`；作者忘了把 `Health` 登記成可回滾。伺服器說第 1200 個 tick 的位置不對，回滾 Module 把可回滾的資料還原到第 1200 個 tick、重跑——`Health` 沒被還原，卻被重跑的 `ApplyDamage` 再扣一次。另一個：入口 System 把封包寫進輸入暫存，重跑時又跑一次，同一個輸入被寫兩遍。

**L24 已定**：可回滾的資料（Yes／No）、Derived、D-E16、可回滾的 schedule 由回滾 Module 登記並公開；這幾條「判定資料 × 回滾」的規則由伺服器權威策略檢查，讀回滾 Module 的公開登記；作者各自選兩個策略，伺服器權威只檢查相容、不指定回滾策略。

伺服器權威策略的交叉檢查（卡片的建議；舊規則的理由都在，只換落點）：

| 規則 | 讀什麼 | 檢查 | 舊出處 |
|---|---|---|---|
| 判定寫的要能回滾 | 回滾 Module 公開的「可回滾的 schedule」（L24 (5)）與「可回滾的資料、Derived」 | 掛在可回滾 schedule 上的判定 System，寫的每個判定資料都是可回滾或 Derived；判定 hook 寫的要是可回滾（hook 不能是 Derived） | Z6（修訂 S12） |
| 判定資料不能在可回滾 schedule 上不回滾 | 同上 | 可回滾 schedule 上被判定 System 或漏斗寫到的判定元件，不能是「不回滾」——就是上一列套在漏斗上（`-funnel-one-principle`：寫請求的人當作也寫了目標） | 11-14 漏斗 B3 |
| 輸入不回滾 | 權威登記的輸入暫存（5.5）＋回滾 Module 的可回滾資料 | 標成外部輸入的暫存不能登記成可回滾（輸入是以 tick 為鍵的歷史，還原會抹掉之後收到的輸入） | Z8（修訂 B7） |
| 入口在重跑時不跑 | 權威登記的入口（5.5）＋回滾 Module 公開的「重跑時不跑」 | 掛在可回滾 schedule 上的入口 System 都在「重跑時不跑」裡。官方的伺服器權威策略替作者登記進去（L24 (4)），所以這條對官方實作是自我核對；作者自寫的策略或手動登記時才會抓到漏的 | D-E15（修訂 B7／B-16） |

- **相容**的意思就是上面四條成立；沒有回滾策略的 World（例：Match，待 L29）這四條都不適用。
- 回滾 Module 的公開登記怎麼叫由回滾份定；這份只要求它們是公開、可在啟動時讀的（L22：權威不讀回滾的內部）。
- Derived 的順序（D-E16）在回滾 Module 自己的檢查，不在這裡。
- **從可回滾 schedule 巢狀跑的 schedule**（例：狀態機「兩端同一個 tick 自己換」在固定步裡跑進入／離開的 schedule，P11）：算不算可回滾，由回滾份 5.4 寫規則（統合已通知回滾份）；本份照它——算的話，那些 schedule 上的判定 System 也套第一、二列。回滾份寫好之前，本份先把「判定的獨占 System 宣告會跑的 schedule」（5.11）當成和跑它的 schedule 同一類。
- **事件**：判定事件（4.3）也是判定資料。舊 Z6 跳過事件（`Registry.cpp:1382`）。事件進不進回滾的快照是統合第 10 題；那一題定之前，第一列對事件照舊跳過。
- 舊的 D-E15 是 Kernel 的啟動錯誤；現在是權威 Module 的錯誤碼（O-5 彙整）。

---

## 6. 要使用者判的：給統合第 14 節的材料（本份不自己問）

**題目由統合合併去重、排好順序**（統合第 14 節），由統合或主會話一次問。下面每件是那一題的材料：先講例子，再列公平的選項；「建議」另外標明，是卡片的看法。

| 本份 | 統合第 14 節 |
|---|---|
| 6.3、6.7 的前提（L28：生成時帶的值算不算寫者）；5.3 規則 1 對生成怎麼算 | 第 1 題（分「數量」與「誰能寫」） |
| 6.2、6.13；4.4 代為登記要求策略先選定 | 第 3 題（L29，和回滾份第 5 件合一） |
| 6.3 | 第 4 題 |
| 6.7 | 第 5 題 |
| 6.5 | 第 6 題 |
| 6.4 → 6.11 → 6.8 → 6.12 | 第 7 題 |
| 5.3、5.14 是不是回滾等價的前提之一 | 第 12 題（R-2 的前提由誰守） |
| 6.9 的「比什麼」（含不含事件與游標）→ 6.6 | 第 14 題 |
| 6.14 確認清單 | 第 18 題 |
| 6.1 | 第 19 題 |
| 6.15（入口的位置） | **未排**（修訂 2 新增，請統合排） |
| 6.9 的「何時比」「不一致怎麼辦」 | 不問，延到網路 Module 的宣告（L23） |
| 6.10 | 不是題目 |

### 6.1 權威 Module 的名字

| 選項 | 系譜 | 代價 |
|---|---|---|
| `Authority`（權威；`engine/authority`） | 使用者一直叫「權威 Module」；replicon 自稱 server-authoritative（`src/lib.rs:2`）；lightyear 舊版有 `HasAuthority`，這版拿掉、改叫 `ControlledBy`（`crates/platform/web/CHANGELOG.md:214-216`） | P2P 沒有單一的權威，佔位策略的名字讀起來不搭 |
| `Consistency`（一致性） | 描述「兩端一致」的機制本身；三種策略都適用 | 新詞，要進同詞異義表；中文「一致性」容易和資料庫的一致性混 |
| `Netcode` | Unity Netcode for Entities | 範圍比這個 Module 大（含傳輸、複製） |
| `Determinism`（決定性） | 舊 GYO 的 determinism.md | 伺服器權威不只是決定性；和 `docs/architecture/target/determinism.md` 同名 |

建議（卡片）：`Authority`——使用者與所有決定都用這個詞，改名的成本高於「P2P 讀起來不搭」。

### 6.2 策略選在哪一層（併入 L29）

統合把這件與 6.13、回滾份第 5 件（哪些 World 可以沒有回滾）合成一題：Match 不回滾，但權威 Module 整個依賴回滾 Module；兩個策略各自選在哪一層要一起定。下面是權威這一半的材料。

**例子**：Client 有三個 World，只有模擬 World 要伺服器權威；渲染 World 不要。

| 選項 | 代價 |
|---|---|
| 每個 World 選一個策略（權威登記本來就經那個 World 的 `Registrar`） | 作者要對每個 World 選；同一個 `Product` 裡可以一個 World 伺服器權威、另一個不選 |
| 每個 `Product` 選一個策略，套到所有經權威登記的 World | 寫一次；但策略要知道哪些 World 有判定資料 |

建議（卡片）：每個 World——登記、Plan、Registry 都是每個 World 一份（K.3 第 6 項：渲染 World 有自己的 Registry），策略跟著登記走最少意外。

### 6.3 寫者規則：數不數、預設哪邊

例子與數法在 5.1。**依賴 L28**：生成時帶的值算不算寫者（統合第 14 節第 1 題）定了，5.1 的數法表才完整。

| 選項 | 系譜 | 代價 | 需要它的 Feature |
|---|---|---|---|
| 數；經權威登記的資料預設單一寫者，作者標 `Shared` 才准多寫 | 11-11 的 `v8-arch-kernel-exclusive-default`（使用者核准，案例 PvP）；P8；縮到只對判定資料 | 判定資料多來源時要寫漏斗或標 `Shared`；四家與三家網路套件都沒有 | 伺服器權威（PvP）；Boss 擊殺歸屬這類「只能一個人拿到」 |
| 數；預設多寫者，作者標 `Exclusive` 才只准一個 | 反過來的預設；Bevy、Unity 的預設 | 忘了標就沒有保護；P7 的 11-11 決定要改 | 同上，但只有作者在意的資料 |
| 不數 | 四家 ECS、三家網路套件 | 「合併規則在哪」不再被逼著寫清楚；擊殺歸屬這類 bug 只能測出來 | — |

建議（卡片）：「數；經權威登記的資料預設單一寫者」。理由：作者選了伺服器權威，就是「作者自選的地方」（P12）；只數判定資料，單機與畫面層不受綁；P7 是使用者兩次確認過的方向，這個選項只縮範圍、不改方向。**但要說清楚**：P5 之後它不再是決定性的必要條件，守的是設計紀律（5.1）。

### 6.4 System 的區怎麼來

例子在 5.2。

| 選項 | 系譜 | 作者要寫什麼 | 錯的時候 |
|---|---|---|---|
| 宣告：判定的 System 經權威登記（`authority.System<&ApplyDamage>(…)`，權威 Module 代為 `AddSystem`） | 舊 S-6（11-11）＋P3 | 判定的 System 多寫一個字；一般的照舊 | 「`ApplyDamage` 是判定的，讀了一般的 `HitFlash`」 |
| 由寫的資料推出：寫了判定資料的 System 就是判定的 | 新；Z2 的反推 | 什麼都不用寫 | 「`DrawHealthBar` 寫了判定的 `Health`，所以是判定的，但它讀了一般的 `UiTheme`」——訊息指向推出來的身分，作者要想一下為什麼 |
| 由 schedule 推出：作者把幾張 schedule 經權威登記為判定的 schedule，掛在上面的 System 都是判定的 | Unity `PredictedSimulationSystemGroup`、lightyear `FixedMain`（兩家的群組是「重跑哪些」，見 5.2） | 登記幾張 schedule | 「`FixedUpdate` 是判定的 schedule，`DrawHealthBar` 掛在上面」；一般的 System 不能掛在判定的 schedule |

建議（卡片）：由寫的資料推出——作者完全不用多寫，Z1、Z2 的性質一樣守得住；別家的「群組」是為了決定**重跑哪些**（回滾），這裡只問**誰是判定**，兩件事分開，前者交回滾 Module。代價是錯誤訊息比較繞（列在上表）。

### 6.5 資料的區：經權威登記的就是判定資料嗎

| 選項 | 代價 |
|---|---|
| 經權威登記＝判定資料（一個屬性都不用填） | 有些資料作者想複製給 Client、但不是判定（例：名字、皮膚）就不能經權威登記——那是網路 Module 的登記，不是權威的 |
| 經權威登記、另填 `decisional = true／false`（舊的 `zone`） | 多一個欄位；但權威 Module 可以同時是「要複製的資料」的登記處 |

建議（卡片）：「經權威登記＝判定資料」——權威 Module 只管一致性，「要不要複製」交給之後的網路 Module 自己的登記（lightyear、replicon 都是網路套件自己的登記）。

### 6.6 digest 禁 Shared

| 選項 | 代價 |
|---|---|
| 留：進 digest 的判定資料不能標 `Shared` | 判定資料只要進 digest 就一定單一寫者 |
| 拿掉：`Shared` 的判定資料也能進 digest | 多寫者的判定資料也比得到（P5：兩端先後相同） |

建議（卡片）：隨第 6.3 節——選「數、預設單一寫者」就留（同一條紀律）；選「不數」就沒有意義、拿掉。

### 6.7 對已存在的實體插入元件：建立還是修改

例子在 5.7。**依賴 L28**：若 L28 定「生成時帶的值也算寫者」（照 11-14），插入算修改就是自然的延伸；若照 L4「不算」，這一題才是真的二選一。

| 選項 | 系譜 | 代價 |
|---|---|---|
| 算修改（插入的 System 是寫者） | 舊 B.3「型別化 Commands 中宣告 insert／remove `T` 的 System 算寫者」（11-11）；replicon 把「對已有元件的實體插入」在 Client 當成修改（`src/lib.rs:226-236`）；Bevy 的 insert 會蓋舊值（`commands/mod.rs:1381-1384`） | 多個 System 插入同一種元件（護盾、各種減速效果）要走漏斗或標 `Shared` |
| 算建立（插入不算寫者） | L4 的延伸（`Spawn` 帶的值不算）；Bevy 的 required components | 「蓋掉舊值」那一刻沒被數到；同一個 tick 兩個插入誰贏照登記順序，作者沒寫在任何地方 |

建議（卡片）：算修改——啟動時分不出目標有沒有這個元件，而插入在目標已有時就是蓋值；L4 的「建立」指的是實體誕生的那一刻，對已存在的實體插入不是誕生。

### 6.8 C 指紋（與之後的 wire id）用的編號

例子在 5.8。

| 選項 | 系譜 | 代價 |
|---|---|---|
| 權威 Module 的登記順序（只數經權威登記的東西） | replicon 的登記順序（`registry.rs:111-124`），順序算進協議雜湊；舊 GYO N2 | 兩端的**權威登記順序**要一樣（一般資料的順序不影響）；順序寫錯 C 指紋就不同、連不上（錯得早、錯得明白） |
| 型別的名字（登記時給的 `name`）排序後雜湊 | Unity 依型別雜湊排序（`GhostCollectionSystem.cs:51-65`）；C++ 要自己保證名字跨編譯器穩定（`typeid` 的名字不行） | 兩端順序不用一樣；改名就不相容；名字重複要擋 |
| 契約檔的編號（舊 GYO 的 .proto 欄位號） | 舊 N2「GYO 用 protobuf 欄位號＝契約」 | 多一份檔；判定資料改一次要改兩處（AGENTS §9 同一事實一處） |
| Kernel 的編號（現在） | 現在的 `Fingerprint.cpp` | 一般資料的登記順序也要兩端一樣（5.8 的問題） |

建議（卡片）：權威 Module 的登記順序——只要求判定的部分兩端一致，和「C 指紋只看判定」同一個範圍；wire id 之後可以沿用。

### 6.9 digest：比什麼（何時比、不一致怎麼辦延到網路 Module）

例子在 5.9。「比什麼」是統合第 14 節第 12 題；「何時比」「不一致怎麼辦」延到網路 Module 的宣告（L23），兩張表留作那份宣告的材料。

**比什麼**

| 選項 | 系譜 | 代價 |
|---|---|---|
| 全部的判定資料 | 舊 G-5 的「(c) 區資料集」 | 資料多時算得慢（可以抽樣頻率補） |
| 作者選的集合（舊 `DigestSet`） | 舊 GYO（11-14 留著但「不急定」）；lightyear 只算標了 `Deterministic` 的實體與登記了雜湊函式的元件（`checksum.rs`、`registry/deterministic.rs:47-75`） | 多一個登記；漏選的分岔抓不到 |
| 全部的判定資料，作者可以排除幾項 | 折衷 | 多一個屬性 |

**事件與游標**（交叉檢查第 12 條、游標份 L13 (4)）：判定事件的紀錄（4.3）要不要算進 digest；游標若定在 World 裡（游標份 4.1），讀者游標要不要算。

| 選項 | 代價 |
|---|---|
| 都不算：只比元件與 Resource | 事件晚一個 tick 才反映到資料上，分岔晚一點才被看到；游標不同（某個讀者漏讀）看不到 |
| 算判定事件的紀錄，不算游標 | 兩端同一個 tick 的事件紀錄要一樣（判定事件只由判定 System 發，本來就該一樣）；游標是讀的進度，不是遊戲狀態 |
| 都算 | 游標的位置和讀者的登記有關，兩端的一般讀者不同就會不一致（同 5.8 的編號問題） |

建議（卡片）：算判定事件的紀錄、不算游標——游標屬於讀者，兩端的一般讀者本來就不同。這題依賴游標份 4.1（游標放哪）與回滾份第 1 件（歷史存哪），統合已排在它們之後。

**何時比**（延到網路 Module 的宣告）

| 選項 | 代價 |
|---|---|
| 每個 tick | 網路量（每 tick 幾個位元組）與計算；lightyear 伺服器每 tick 算、Client 每 tick 送一次 |
| 每 N 個 tick（N 由策略設定） | 晚 N 個 tick 才發現 |
| 只在測試與除錯時（第一版沒有網路，只有回歸測試） | 上線後的分岔要靠伺服器覆蓋自己修好、看不到 |

**不一致怎麼辦**（伺服器權威；延到網路 Module 的宣告）

| 選項 | 系譜 | 代價 |
|---|---|---|
| 記錄與計數，等伺服器覆蓋修好 | lightyear（`checksum.rs:508-511`，只記 error） | 覆蓋碰不到的資料會一直分岔 |
| 記錄，並要求伺服器送一次完整的判定資料（重新同步） | 三家都沒有這樣做 | 要網路 Module 支援「完整重送」 |
| 斷線 | 三家都沒有；GGPO、GekkoNet 沒讀 | 玩家被踢 |

建議（卡片）：比全部的判定資料（作者可排除）、每 N 個 tick、不一致時記錄與計數。理由：唯一有執行中 checksum 的 lightyear 只記 log；伺服器權威下伺服器的值是對的，斷線沒有必要；「要求完整重送」沒有先例、又要網路 Module 有那個能力（L23），列成選項但不建議。**注意**：覆蓋碰不到的資料會一直分岔——碰得到多少，取決於搬運是「每次無條件蓋」還是「伺服器有變才蓋」（5.10），那一段定了之後這一件可能要重看。第一版沒有網路，只實作計算與回歸測試。

### 6.10 伺服器覆蓋的歸屬

5.10 的三段。權威 Module 只定「規定」那一段；統合已判搬運歸網路 Module（L23）、重算歸回滾 Module（L11、L24）。不是題目。

### 6.11 「tick 只有一個來源」的檢查形狀

例子在 5.6。

| 選項 | 代價 |
|---|---|
例子在 5.6。**隨 6.4**（System 的區怎麼來）：統合把這一題排在 6.4 之後（第 7 題）。

| 選項 | 驅動者怎麼過規則 1 | 代價 |
|---|---|---|
| 驅動者登記豁免：`authority.Mark<Tick>()` 把 tick 標成判定資料（L27），伺服器權威策略讀時間 Module 公開的驅動者與對時，代為登記只豁免 tick 的寫者（4.8） | 豁免 | 「只有一個來源」靠時間 Module 公開的名單正確；若 6.3 選「不數寫者」，判定的 System 寫 tick 不會被擋，要另加一句「tick 只有豁免的寫者」 |
| `MarkSystem` 加例外：驅動者標成判定的獨占 System | 規則 1 的讀與 5.11 第 3 點各開一個只給驅動者的例外 | 規則裡多兩個特例；6.4 選「由寫的資料推出」時，驅動者會被推成判定的，例外必要 |
| 專用規則：tick 不當一般判定資料；「tick 只由時間 Module 公開的驅動者與對時寫」＋規則 1 開「判定的 System 可以讀 tick」 | 不經規則 1 | 多一條專用規則和一個例外；不依賴 5.1、6.3 |

建議（卡片）：驅動者登記豁免——用的是已經要有的豁免機制（回滾、網路也用），不另立規則；範圍只到 tick。若 6.3 選「不數寫者」，補上表裡那一句。

### 6.12 G-5 匯出留不留

5.12。選項：留兩份／只留來源檔清單／都拿掉（只靠 digest）。建議（卡片）：留兩份（理由在 5.12）。

### 6.13 選了權威 Module 但沒選策略（併入 L29）

| 選項 | 代價 |
|---|---|
| 啟動錯誤（「選了權威 Module，World X 沒有選策略」） | 明確 |
| 當作沒有規則（登記照走、不檢查） | 作者以為有保護其實沒有 |

建議（卡片）：啟動錯誤——和佔位策略的時機一致。L29 若定「某些 World 可以沒有權威策略」（例：Client 的渲染 World），這個錯誤只對「經權威登記過東西的 World」報。

### 6.14 確認清單：由既定決定推出、本份拿掉或改形狀的規則（統合第 14 節第 18 題）

這些不是新的選擇題：每一條都是卡片從使用者已定的決定推出來的，但推翻或改了使用者較早核准過的東西，所以請使用者確認推得對不對。「推不出來」的話，那一條回到選項。

| # | 較早的（誰核准、何時） | 本份改成 | 從哪個決定推出 | 推的理由 |
|---|---|---|---|---|
| 1 | D-E9：(c) 的 hook 衝突是啟動錯誤（11-11 規格，使用者核准規格） | 拿掉 | `no-ambiguity-detection`（11-16：Kernel 照登記順序平手、伺服器權威連線比 C 指紋） | hook 照登記順序跑；C 指紋含判定 hook 的順序（5.8 的表「判定 hook 的順序」列），兩端順序不同就連不上 |
| 2 | K.3 第 5 項：讀 (c) 的 (b) System 要和 (c) 寫者有顯式順序，否則 D-E8（使用者 11-11） | 拿掉，不是錯誤 | 同上 | 讀判定資料的一般 System（畫面）不影響判定結果，兩端排法不同也只影響畫面；D-E8 本身已隨 `no-ambiguity-detection` 拿掉 |
| 3 | Z3：入口要在 schedule 指定的 ingress stage（11-11 規格） | **修訂 2 移到選項（6.15）**：修訂 1 的理由「兩端排法相同、C 指紋比得到」不成立——入口不進 C 指紋（5.8 的表） | — | — |
| 4 | `zone-c-exclusive-apply`：**Kernel 加** (c) 區的獨占套用，**啟動時檢查**它只跑列出的那幾張 schedule（使用者 11-15～16「這個照建議。」） | Kernel 不加；權威 Module 登記獨占 System 會跑的 schedule，清單進 C 指紋，執行期不核對 | L3（拿掉執行期攔截）、L7（`ExclusiveWorld` 能寫任何值） | 「只跑列出的那幾張」只能在執行期知道；L3 之後沒有執行期的核對，改成兩端比清單＋digest |
| 5 | `spawn-reject-zone-c-blocked-by-zone-b`：被丟的生成的明細帶記錄者的區（使用者 11-15） | 拿掉 `emitterZone`，不給替代欄位；權威 Module 用 `emitter` 名字查自己的登記 | 統合 3.2（區移出 Kernel）、L3 | Kernel 不再認得區；明細已有名字，權威 Module 查得到 |
| 6 | `funnel-block-b1-b3-fingerprint-a`：「B1–B3 要擋，指紋用方案 A」（使用者 11-14） | B1（入口經輸入的請求造出判定的狀態）、B2（判定的 System 造出輸入）：由 5.3 的「判定只從判定資料算出」＋5.5 的「只有入口寫輸入暫存」與漏斗原則涵蓋；B3（可回滾 schedule 裡出現不回滾的判定元件）：5.14 的交叉檢查；方案 A（宣告一律進指紋）：隨 C 指紋到權威 Module（5.8），Registry 指紋照統合 3.4 改版 | `kernel-is-ecs-no-restrictions`（區移出 Kernel）、L24（交叉檢查歸伺服器權威策略） | 三條擋的性質都還在，只是換到權威 Module；擋不擋沒有改 |

### 6.15 外部輸入的入口要不要排在讀它的判定 System 之前（從 6.14 第 3 條移來）

**例子**：Match 的入口 `StageInput` 把封包寫進「第 1200 個 tick 的輸入」；判定的 `ApplyInput` 在同一張固定步讀它。作者把 `StageInput` 排到 `ApplyInput` 之後：第 1200 個 tick 的判定讀不到這一 tick 剛到的輸入，要到伺服器覆蓋或 digest 才看得出 Client 預測的結果和伺服器不同。入口不進 C 指紋（5.8），兩端各自排錯也比不到。

| 選項 | 系譜 | 代價 |
|---|---|---|
| 不檢查入口的位置 | 統合拿掉 `ingressStage`；Unity 的輸入群組只是文件（`CommandSendSystem.cs:31-34`） | 排錯是遊戲的 bug，只有覆蓋或 digest 會間接看出來 |
| 檢查「同一張 schedule 上，入口排在讀同一份輸入暫存的判定 System 之前」：讀唯讀登記表的順序圖（L6），不用 stage 名 | 舊 Z3 的「入口在 schedule 的第一個 stage」的意圖，換成順序 | 多一條檢查；要讀順序圖 |
| 入口進 C 指紋 | — | Match 與 Client 的入口常常不是同一個 System，進了就連不上；不建議 |

建議（卡片）：檢查入口排在讀者之前——舊 Z3 的意圖還在，換成不用 stage 名的寫法，便宜。**這一題統合第 14 節還沒排**，請統合排進去。

---

## 7. Design Invariants 與 Replacement Criteria（草案；核准後在實作批寫進權威 Module 的頁）

DI 的編號在實作批定（暫用 DI-A）。依賴第 6 節的選擇的，標「隨 6.x」。

| # | Design Invariant |
|---|---|
| DI-A 1 | **權威 Module 的職責是讓作者選的一致性策略有機制可用**（4.1 的清單）。不攔執行期的寫入、不做回滾、不存 tick、不傳輸 |
| DI-A 2 | Kernel 不認得權威：權威 Module 的宣告只存在權威 Module；Kernel 收到的是一般的登記。守衛：Kernel 目錄的掃描（統合附錄的式子） |
| DI-A 3 | 策略是 interface＋實作（L21）：官方實作與作者的走同一個插口；interface 的函式與時機（4.4）是契約，官方實作的內部不是 |
| DI-A 4 | 檢查只在啟動時（L5 的權威檢查那一步，含讀 World 建立時的宣告）；執行中只計算與比對，不拒絕寫入（L3） |
| DI-A 5 | C 指紋只看判定的部分，用的編號與一般資料的登記無關（隨 6.8）。守衛要證明兩個方向：判定的登記改了指紋就變；一般資料的登記改了指紋不變（lightyear 的元件雜湊沒餵資料、等於常數，是反面例子） |
| DI-A 6 | 佔位策略選了就是啟動錯誤，訊息寫明還沒實作 |
| DI-A 7 | 伺服器權威下判定資料以伺服器為準；覆蓋與回滾的寫入不算寫者，**而且只經公開的豁免寫者的登記才不算**（4.8）：豁免公開、具名、有理由，不進 C 指紋 |
| DI-A 9 | 權威不讀回滾、時間的內部，只讀它們的公開登記；不依賴狀態機（以名字標記）；回滾、時間、狀態機都不依賴權威（L22、L26、L27） |
| DI-A 11 | 代為登記（進 Kernel、進回滾、重跑時不跑、豁免、標記）都在作者呼叫權威登記入口的當下做，一定在建 Plan 之前（4.4）；策略要先選定 |
| DI-A 10 | 別的 Module 已登記的型別與 System 只標記、不重複登記（4.3）；標記對不到唯讀登記表是啟動錯誤 |
| DI-A 8 | （隨 6.3）經權威登記的判定資料預設單一寫者；寫者的數法照 5.1 的表 |

**Replacement Criteria**（L17：不數次數，出現就停下來問）：

1. 權威 Module 的職責清單（4.1）要多加一項，或判不出某樣東西屬權威、回滾、時間、網路哪一個。
2. 要讓伺服器權威成立，必須在 Kernel 加任何權威專用的欄位、分支或觀察點（L3、P3 的反面）。
3. 伺服器權威策略的檢查，在作者照正常業務寫（統合 7.3 的有型別寫法）時誤報，而且改寫法要繞過機制（不是用漏斗或 `Shared`）。
4. 第二個官方策略（P2P 回滾）實作時，interface（4.4）要改——表示 interface 是照伺服器權威的形狀寫的。

---

## 8. 修訂 0 回報統合的 N1～N8 與網路 Module 的需求

修訂 0 回報統合的八件，統合修訂 18、19 照 L17 判了（N5 交使用者）：

| # | 修訂 0 回報的 | 統合的處理 | 本份怎麼改 |
|---|---|---|---|
| N1 | 網路／複製這一層不在關聯表 | L23：歸 Engine 的網路 Module；本份只定規定、把需求列出 | 5.9、5.10；第 8.1 節 |
| N2 | 回滾的宣告與檢查歸誰 | L24：Yes／No、Derived、D-E16 歸回滾；Z6、Z8、D-E15 是伺服器權威策略的交叉檢查；兩個策略各自選；退 tick 不算寫者；豁免寫者的登記；ingress 代登記「重跑時不跑」；可回滾 schedule 由回滾登記 | 4.8、5.14 |
| N3 | C 指紋不能用 Kernel 的編號 | L10 補句 | 5.8 |
| N4 | World 的宣告讀不到 | L6 補：唯讀登記表交出每個 World 建立時的宣告 | 4.5 |
| N5 | L4 翻掉 11-14 的 `spawn-writer-funnel` | L28，待使用者（統合第 14 節第 1 題） | P6、5.1、6.3、6.7 標「待 L28」 |
| N6 | 代為登記時的 owner | L25：用作者的 `Registrar`，owner 照舊是作者 | 4.3 |
| N7 | 狀態機 (c) 區的接法 | L26：狀態機公開登記，權威替作者標成判定的 | 4.3（`MarkSystem`）、5.11 |
| N8 | 時間 Module 公開 tick | L27：列為時間宣告的要求 | 4.3（`Mark`）、5.6 |

修訂 1 沒有新的關聯要回報。

### 8.1 網路 Module 要權威 Module 提供的（L23 要求的需求清單）

網路 Module 的設計不在這一輪；這是它和權威 Module 之間的介面需求，給網路 Module 的宣告當輸入。

| 需求 | 權威 Module 提供 | 本份的節 |
|---|---|---|
| 連線時交換並比對一致性 | `ConsistencyFingerprint(view)`：位元組與版本；策略的名字與版本也在裡面 | 4.4、5.8 |
| 執行中比對 | `Digest(world, …)`：計算某個 tick 的 digest；範圍隨統合第 14 節第 12 題 | 5.9、6.9 |
| 不一致時怎麼辦 | `OnMismatch(…)`：策略決定（何時比、不一致怎麼辦延到網路 Module 的宣告定，6.9 的兩張表是材料） | 4.4、6.9 |
| 哪些資料以伺服器為準 | 判定資料的清單，用 6.8 選的穩定編號（wire id 可以沿用） | 5.8、5.10 |
| 收包入口的登記 | 網路 Module 收包、寫進輸入暫存的 System 要經 `authority.Ingress`（或以名字標記成入口）登記，才能寫輸入暫存（5.5）、才會被代為登記「重跑時不跑」（5.14）。兩種寫法：網路 Module 依賴權威、自己登記；或網路不依賴權威、作者在產品裡用名字標記。由網路 Module 的宣告定 | 5.5、5.14 |
| 寫伺服器值的 System 登記豁免 | 同上的兩種寫法（4.8） | 4.8 |
| 執行中插入的處理 | 伺服器權威要求 World 不准插入（L5）；作者自寫的策略若允許，網路 Module 要能重新交換指紋（Unity 的 ghost 型別清單＋雜湊是前例，`GhostReceiveSystem.cs:470-486`） | 5.8 |

另外兩件是網路 Module 自己要定、但會影響權威的：蓋回是「每次無條件蓋」還是「伺服器有變才蓋」（5.10，決定 digest 要補多少）；完整重送要不要有（6.9）。

---

## 9. Architecture Delta（AGENTS §3；只寫和統合第 4 節的差分）

1. **Feature**：統合 5.4「伺服器權威」（PvP Match／Client；使用者 11-11、11-16）、「作者自寫策略、P2P 回滾」（格鬥遊戲，只佔位）；L5 的指令集（伺服器權威要求不准插入）。
2. **現在的問題**：寫者、區、digest、C 指紋、G-5 都在 Kernel（`Registry.cpp:1440-1490`、`:1240-1300`；`Fingerprint.cpp:330-406`、`:458`）；Kernel 對單機也執行；C 指紋用 Kernel 的編號（新發現，5.8）。
3. **變化的邊界**：新 Module（權威）；它的公開介面（權威登記 API、`AuthorityStrategy`、`ServerAuthorityStrategy`、佔位策略、指紋與 digest 的計算、G-5 的匯出）。
4. **影響的 Module／Product／Tool**：權威 Module（新）；Kernel（減法批拿掉的部分，統合已列）；時間 Module（公開 tick 型別、驅動者標 `Writes<…>`，L27）；回滾 Module（公開可回滾的資料與 schedule、重跑時不跑、重跑者與還原者，L24）；狀態機 Module（公開登記，L26）；網路 Module（之後；用豁免寫者的登記與第 8.1 節的介面，L23）；編排層（佔位策略的錯誤彙整）；測試：統合 7.2 標「移出 → 權威」的 J-14、J-15、J-16、J-17、J-27 的守衛 id、J-34 的 D-E9、J-38～J-41、J-47 的 Z1、`InitialValueTests` 的 Z1／Z2／Z3／Z6、`SkeletonTests` 的寫者案、`zone_c_export_check.py`、`zone_c_fixture/`——**逐案在實作批判斷**；其中 Z6、D-E15 的案搬到權威的交叉檢查（5.14），D-E16 的案搬回滾 Module（L24）。
5. **Dependency Direction**：新增 權威→Kernel、權威→編排層、權威→時間、權威→回滾（不依賴狀態機：以名字標記，4.2）；網路→權威（之後）；產品→權威。沒有反向。
6. **Ownership**：寫者、區、digest、C 指紋、G-5：Kernel → 權威 Module（統合已列）；tick 的單一來源：Kernel → 權威策略（L2）；Z6、Z8、D-E15：Kernel → 權威策略的交叉檢查（L24）。owner 是架構（架構師會話）。
7. **為什麼不能更小**：權威的規則留在 Kernel 已被使用者否決（`-kernel-is-ecs-no-restrictions`）；不設權威 Module、規則全交產品，伺服器權威的檢查會在每個 PvP 產品重寫（使用者 11-16「權威檢查我想放Module，這個是底層能力」）。

### 9.1 基盤的根據（AGENTS §2）

**為什麼算基盤**：伺服器權威的產品、之後的網路 Module、回滾 Module、狀態機的 (c) 區都會積在它的 interface 與 C 指紋上；C 指紋是兩端握手的內容，一改就不相容。

| 根據（出處） | 要求 |
|---|---|
| PvP 伺服器權威（使用者 11-11、11-16） | 啟動時的檢查（5.1～5.6）；C 指紋（5.8）；伺服器覆蓋的規定（5.10） |
| Client 三個 World、只有模擬 World 判定（11-11） | 策略選在哪一層（L29，待使用者）；C 指紋不受一般資料影響（5.8）；豁免不進 C 指紋（4.8） |
| 格鬥遊戲 P2P（11-16，佔位） | interface 不照伺服器權威的形狀寫死（RC 4） |
| 作者自寫策略（11-16）、策略的構造（L21） | interface＋官方實作 |
| 指令集（11-17） | 不准插入的 World 由策略要求（L5） |
| 參考：lightyear、replicon、Unity Netcode | 權威的宣告在網路套件自己的登記（4.1）；連線時比協議雜湊、不同就斷線（5.8）；執行中不擋寫入、靠蓋回（5.10） |

**實作薄的部分**：第一版只實作登記、啟動時檢查、C 指紋與 digest 的計算、G-5 匯出、佔位策略；比對的傳輸、伺服器覆蓋的搬運等網路 Module（L23，第 8.1 節）。

### 9.2 預算

這份不碰執行效率。digest 的計算量在選了「每 N 個 tick」之後由實作批量。

---

## 10. 批次（草案）

照統合第 8 節的五段（結構段 → 編排層骨架 → 時間＋游標＋拿掉時鐘 → **權威＋回滾＋拿掉權威規則** → 之後）。權威 Module 在第四條線，和回滾 Module、Kernel 拿掉權威與回滾規則同一條，守衛不會有「從 Kernel 拿掉、新家還沒接上」的空窗。依賴：編排層骨架、時間 Module（權威依賴它，L20；L27 的 tick 公開）、本宣告與回滾份核准。

第四條線內的先後（統合第 8 節修訂 19）：

| 步 | 內容 | 誰的宣告 | 模型／ギア（navigator 上段：公開介面、Design Invariants） |
|---|---|---|---|
| 1 | Kernel 給回滾原語 | 統合 L11、回滾份第 6 節 | 回滾份定 |
| 2 | 回滾 Module 的公開登記：可回滾的資料與 schedule、重跑者與還原者、重跑時不跑 | 回滾份 | 回滾份定 |
| 3 | **權威 Module 的骨架與豁免寫者的登記**：登記入口（4.3 三種）、代為登記進 Kernel 與回滾 Module、`AuthorityStrategy`、佔位策略、啟動錯誤、豁免寫者的登記（4.8）；伺服器權威的區與寫者檢查（5.1～5.6、5.11）；C 指紋（新編號）、digest 計算、G-5 匯出 | 本份 | 實作 Opus／high；審查 Fable／high |
| 4 | **權威的交叉檢查**（5.14：Z6、Z8、D-E15、B3） | 本份 | 實作 Opus／high；審查 Fable／high |
| 5 | 拿掉 Kernel 的權威與回滾規則；從 Kernel 搬來的守衛逐案判定（§14.4）；golden 重產（第三次） | 統合第 8 節 | Opus／high；Fable／high；§14.4 檢證者 Fable／high |

估計（本份的步 3、4）：骨架、登記與豁免 8～12 h；區與寫者檢查、交叉檢查與守衛的逐案判定 10～16 h（約 100 個測試案例與它們的突變，數字從統合 7.1 的標籤估，實作批重算）；指紋與 digest 4～6 h。

---

## 11. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；時間估計是讀程式量估的。
- 回滾的資料登記、事件游標、tick 的存放不在這份；交界由統合定（L22～L27）。
- 網路 Module 的設計不在這一輪（L23）；第 5.9、5.10 節只定權威 Module 那一段，第 8.1 節是給它的需求。
- 回滾 Module 的公開登記（L24 (3)～(5)）的名字由回滾份定；5.14、4.8 只寫要讀什麼。
- 依賴未定的題：L28（P6、5.3 的生成）、L29（4.4 的策略先選定、4.7、6.2、6.13）、統合第 10 題（5.14 的事件）；回滾份 5.4 要寫的巢狀 schedule 規則（5.14）。
- 修訂 2 起本份不依賴狀態機 Module，L26 的「伺服器權威策略可以替作者」改成「作者用名字標記」（4.2），已告訴統合。
- 第一版沒有網路：L3 拿掉的執行期攔截，在網路 Module 之前沒有東西在執行中接手（digest 只用在回歸測試）；gyo2 上目前沒有產品（統合第 8 節記這段空窗）。
- 別家的做法來自子 agent 讀本機原始碼（第 0.1 節），卡片抽查了一部分；Unity 讀的是非官方鏡像。
- 舊規格只取設計思想；舊的具體做法（`InputRing`、`HostClockSample`、`Restore`）不帶進來。
- 修訂 0 已隨四份一起送換模型檢查（Fable／high，統合卡負責）；修訂 1、2 照統合修訂 19、20 的通知改（修訂 1 隨第二輪送過檢查），修訂 2 沒有再送檢查。

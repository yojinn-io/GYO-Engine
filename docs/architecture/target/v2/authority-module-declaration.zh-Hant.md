# 權威 Module：事前宣告（子份；修訂 0）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。核准本宣告不等於 AGENTS §14.4 的判定變更核准**：搬到權威 Module 的每一個守衛判定，在實作那一批的事前宣告逐案列出、逐條核准。
> 批次代號：`v8-arch-gyo2-authority-module`。
> **這份是「Kernel 與編排層的邊界」拆出來的三份子份之一**（`v8-arch-gyo2-boundary-split-integrator-first`）。統合是 [`kernel-app-boundary-declaration.zh-Hant.md`](kernel-app-boundary-declaration.zh-Hant.md)（分支 `claude/gyo2-kernel-app-boundary-4af68c`，修訂 15，6ba6124d）。**以統合為準**：統合定過的事這裡不改；需要改 Kernel 公開介面的，或關聯表沒有的新關聯，回報統合（第 8 節），不在這裡判。
> 範圍：統合關聯表 L12。**回滾的內容不在這份**（回滾 Module 那一份，L11）；事件游標在游標份（L13）；tick 存在哪裡在時間 Module（L2）。

---

## 0. 依據

| 項目 | 版本 |
|---|---|
| 程式 | `gyo2` f00433b0（#240 合併後；Kernel 的程式和統合讀的 cb6bccca 相同） |
| 決定表 | HANDOFF，分支 `claude/gyo2-time-design` 2416d4c0 |
| 統合 | 修訂 15，6ba6124d |
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
| L3 | Kernel 拿掉執行期的攔截、不加觀察點；伺服器權威改靠 digest 比對與伺服器覆蓋抓錯 | 權威 Module 只有啟動時的檢查與執行中的比對，**不攔寫入**；第 5.9、5.10 節設計比對與覆蓋 |
| L4 | 初始值在建立實體的當下給、不算寫者；伺服器權威只數修改既有值的寫者；「對已存在的實體插入元件算建立還是修改」由權威份定 | 第 5.7 節 |
| L5 | Kernel 提供「開 Registrar」「建 Plan」兩步；編排層 build → 建 Plan → finish → 權威檢查；每個 World 建立時宣告准不准動態插入；伺服器權威策略在啟動時要求它的 World 宣告不允許 | 檢查的時機（第 4.5 節）；執行中插入與 wire id（第 5.8 節） |
| L6 | Kernel 交出唯讀、帶版本號的登記表；參照只在當前版本有效；修改只走 `Registrar` | 策略讀它做檢查與 C 指紋；權威 Module 不改它 |
| L7 | `ExclusiveWorld` 能寫任何值；`Writes<…>` 留作選填標註、Kernel 不解讀，伺服器權威策略可以拿它數寫者 | 第 5.1、5.11 節 |
| L10 | 指紋改版；C 指紋移權威 Module，用唯讀登記表計算 | 第 5.8 節（C 指紋用的編號） |
| L11 | 回滾是獨立的 Module、有自己的策略；伺服器權威策略依賴回滾 Module | 回滾的宣告與檢查不在這份；交界列在第 8 節 |
| L12 | 伺服器權威檢查哪些、預設區、C 指紋與 G-5 匯出、digest 比對與伺服器覆蓋、(c) 區獨占套用的新形狀、名字、`AuthorityStrategy` interface 的形狀 | **本份的範圍** |
| L15 | 產品可以只選部分 Module（「要不然單機遊戲也必須上權威了」） | 單機不選權威 Module，就沒有任何寫者、區、digest 的規則 |
| L17 | 兩條核心「Engine 只提供機制」「Kernel 的職責是 ECS」；每層一份職責清單；替換條件＝清單要多加一項或判不出屬於哪一層 | 第 4.1 節的職責清單；第 7 節的 RC |
| L18 | 狀態機 (b) 區已核准；`RegisterStateMachine<S, Zone::C>` 的 `Zone` 隨 L12 | 第 5.11 節給 (c) 區的新形狀；狀態機卡照它改 |
| L20 | Module 之間的依賴只寫在登記檔的「依賴」欄，編排層讀出並檢查；不做策略層級的依賴，權威 Module 整個依賴時間 Module | 第 4.2 節 |
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
| P6 | 初始值在建立當下給、不算寫者 | 使用者 11-17（L4） | 5.1、5.7 |
| P7 | 元件與 Resource 預設單一寫者（11-11）——**重新評估的對象**，不是前提 | 使用者 11-11（`v8-arch-kernel-exclusive-default`）；11-15 確認過一次 | 5.1 |
| P8 | 「要保證誰寫，別開 shared」 | 使用者 11-15（`-shared-means-no-owner`） | 5.1、5.4 |
| P9 | 每個 World 宣告准不准動態插入；伺服器權威要求不准 | 使用者 11-17（L5） | 5.8 |
| P10 | 回滾是獨立 Module；伺服器權威策略依賴它 | 使用者 11-17（L11） | 5.10、第 8 節 |
| P11 | 狀態機 (c) 區兩種形狀都支援（權威端換、Client 採用；兩端同一個 tick 自己換） | 使用者 11-15～16（`-state-machine-zone-c-both-shapes`） | 5.11 |
| P12 | 做 Engine 不把規則釘死：預設方便、限制只放在作者自選的地方 | 使用者 11-17（`-engine-not-rigid`） | 全部；選了伺服器權威就是「作者自選的地方」 |

---

## 3. 先用遊戲講

### 3.1 PvP 射擊的 Match（伺服器，權威端）

作者的產品選了權威 Module，Match 的模擬 World 選「伺服器權威」策略。作者寫：

```cpp
authority.Component<Health>({.name = "Health"});            // 判定用的資料：血量
authority.Component<Position>({.name = "Position"});
authority.Resource<MatchClock>(...);                        // 例：時間 Module 的 tick
registrar.Component<HitFlash>({.name = "HitFlash"});        // 一般的資料（畫面用），不經權威
```

權威 Module 記下「`Health`、`Position` 是判定用的資料」，再替作者呼叫 Kernel 的 `Registrar::Component`。Kernel 照常凍結、建 Plan；編排層 finish 之後叫策略檢查：`ApplyDamage` 與 `ApplyHeal` 都寫 `Health` 嗎？讀了畫面用的 `HitFlash` 再寫 `Health` 嗎？不過就啟動失敗、訊息列出是誰。執行中策略不攔任何寫入（L3）。

### 3.2 PvP 射擊的 Client（預測）

Client 有三個 World（模擬、每幀、渲染）。只有**模擬 World** 選伺服器權威——它要和 Match 的模擬 World 跑同一套判定。Client 的模擬 World 另外有只在本機的東西（預測的子彈拖尾、除錯面板），Kernel 的登記表和 Match 不一樣；所以連線時比的是「判定用的那一部分」的指紋（C 指紋），而且不能用 Kernel 的編號（第 5.8 節）。

玩家按下開槍，Client 先預測（自己算傷害、畫特效）。伺服器的結果回來：判定用的資料以伺服器為準——預測錯了就被蓋掉、重算（伺服器覆蓋；重算與平滑在回滾 Module）。如果 Client 有一段除錯程式用 untyped `Commands` 偷改了血量，Kernel 不擋（L3）；下一次伺服器的值會蓋掉它；不在伺服器資料裡的判定資料，靠 digest 比對發現兩端不一樣（第 5.9 節）。

### 3.3 格鬥遊戲（P2P 回滾；只佔位）

作者選「P2P 回滾」策略。第一版沒有實作：編排層 finish 時報啟動錯誤「這個策略還沒實作」（P2）。名字與 interface 留著，作者也可以自己寫一個接上（L21）。

### 3.4 單機 2D 橫版

不選權威 Module（L15）。沒有寫者、區、digest 的任何規則；「受傷」與「撿補血包」都直接寫主角的血量，照宣告的先後，沒宣告就照登記順序。

---

## 4. 權威 Module 的形狀

這一節是骨架：位置、依賴、登記的走法、interface 的輪廓、時機。要使用者選的放在第 5、6 節。

### 4.1 職責清單（L17）

**權威 Module 的職責是「讓作者選的一致性策略在啟動時與執行中有機制可用」**。清單：

1. 作者的權威登記（判定用的資料、它們的屬性），並代為登記進 Kernel（P3）。
2. 策略的插口（`AuthorityStrategy`，L21）與選擇；佔位策略的啟動錯誤。
3. 啟動時的檢查：讀 Kernel 的唯讀登記表（L6）＋自己的登記＋依賴 Module 公開的資訊，回報啟動錯誤（給編排層彙整，O-5）。
4. 一致性的計算：C 指紋（連線時比）、digest（執行中比），以及判定用的來源檔清單（G-5 的後繼，第 5.12 節）。
5. 策略對 World 的要求（例：不准動態插入），在啟動時核對。

**不在清單上**（照 L17 的預設放別層）：回滾的快照、還原、重算、平滑（回滾 Module，L11）；tick 的存放與推進（時間 Module，L2）；把資料送過網路、握手的傳輸（網路／複製，**統合的關聯表沒有這一層**，第 8 節 N1）；執行期攔截（L3 拿掉）；排程與先後（Kernel）。

系譜（同類型多家，L17）：lightyear 把「複製」「預測（回滾）」「決定性同步」分成三個 crate（`crates/replication/replication`、`crates/replication/prediction`、`crates/deterministic/deterministic_replication`）；Unity Netcode for Entities 是一個套件，但預測（`GhostPredictionHistorySystem`、`GhostUpdateSystem`）與複製（ghost 序列化）分在不同的 System 群組；bevy_replicon 只做伺服器權威的複製（`src/lib.rs:2`）、沒有內建預測（`:629-631`）。權威的標示都在網路套件那一層：replicon 在 `app.replicate*` 與 `Replicated` 元件；lightyear 這版沒有 `HasAuthority`，改用實體上的 `ControlledBy`（`crates/replication/replication/src/control.rs:17-60`）；Unity 是型別上的 `[GhostComponent]` 屬性（`Runtime/Authoring/GhostComponentAttribute.cs:14-39`）與實體上的 `GhostOwner`（`Runtime/Snapshot/GhostOwner.cs:20-51`）。三家都沒有「World 內寫者規則」，那一部分是 GYO 自己的（第 5.1 節）。

### 4.2 位置與依賴（L1、L20）

- 位置：Engine 的 Module（我們自己維護、介面是我們的，`-module-plugin-owner-definition`）。目錄、命名空間、target 照名字（第 6.1 節）定；照 N11～N17 的格式是 `engine/<名字>`、`GYO::Engine::<名字>`、`gyo_<名字>`。
- 依賴（只寫在 Module 登記檔的「依賴」欄，L20）：Kernel（唯讀登記表、`Registrar`）、編排層（登記單位型別、啟動錯誤的回報型別）、**時間 Module**（tick 的 Resource 型別與它的推進者，第 5.6 節）、**回滾 Module**（L11）。
- 被誰依賴：產品；之後的網路／複製 Module（握手時要 C 指紋、執行中要 digest；第 8 節 N1）。Kernel、編排層、時間、回滾都不依賴權威 Module。
- 單機不選它（L15）；選了 P2P 佔位策略的產品也會連時間與回滾 Module（L20「不做策略層級的依賴」的後果，列出不判）。

### 4.3 登記：代為登記進 Kernel（P3）

作者經權威 Module 的登記 API 登記判定用的東西；權威 Module 記下自己的宣告，再呼叫 Kernel 的 `Registrar` 做一般的登記。Kernel 收到的是一般的元件與 Resource，看不出它是判定用的。

```
作者 ──authority.Component<Health>(opts)──▶ 權威 Module
                                              ├─ 記下：Health 是判定用的資料、opts 的權威屬性
                                              └─ registrar.Component<Health>({.name = …}) ──▶ Kernel
```

- `Registrar` 由編排層在 build 時給（L5）；權威 Module 用同一個 `Registrar`，不另開。登記者的身分（owner）照統合 3.5，由編排層給；權威 Module 代登記時用的是**作者那個登記單位**的 owner，不是權威 Module 自己的（否則 `InitialValue` 的「只有擁有者能宣告」會錯；第 8 節 N6）。
- 執行中的插入（L5）：伺服器權威要求 World 不准插入，所以伺服器權威下不會發生；作者自寫的策略若允許，插入照同一條路（權威 Module 的登記 → `Registrar`），之後的檢查由那個策略決定。
- 權威登記的屬性有哪些，隨第 5 節的選擇定。第一版最少要有：判定用的資料（是／否；或區）、`Shared` 標記（若第 5.1 節留寫者規則）、進不進 digest（若第 5.4 節留 digest 集合）、`role = Input`（若第 5.5 節留 ingress）。**回滾的屬性（`rollback = Yes／No／Derived`）不在這份**（第 8 節 N2）。
- 前例：lightyear 的 `register_component` 同時登記進 ECS（`crates/replication/replication/src/registry/mod.rs:196-200`）；replicon 的 `app.replicate::<C>()`（`src/shared/replication/rules.rs:23`）；Unity 走另一條路——宣告寫在型別上的屬性（`GhostComponentAttribute.cs:14-39`），11-16 比過、使用者選了自己登記。

### 4.4 策略的 interface（L21）

**照 L21**：`AuthorityStrategy` 是虛函式的抽象基底類別（GYO 的 `Plugin` 已是這個寫法，`engine/kernel/include/engine/kernel/Registry.hpp:60-75`）；interface 是契約、保證不變；官方的 `ServerAuthorityStrategy` 和作者寫的走同一個插口、沒有特權；作者可以從頭實作 `AuthorityStrategy`，也可以繼承 `ServerAuthorityStrategy` 改裝，官方實作的內部不保證不變。

輪廓（函式的確切簽名在實作批定；這裡定「interface 承諾哪些時機」）：

| 時機 | interface 的函式（暫名） | 輸入 | 輸出 |
|---|---|---|---|
| World 建立前 | `WorldRequirements()` | — | 這個策略對 World 的要求（例：不准動態插入） |
| 啟動時（編排層 finish 之後，L5） | `Check(view, errors)` | Kernel 的唯讀登記表（L6）、權威 Module 的登記、依賴 Module 公開的資訊 | 啟動錯誤（給 O-5 彙整） |
| 啟動時、連線前 | `ConsistencyFingerprint(view)` | 同上 | C 指紋（第 5.8 節） |
| 執行中 | `Digest(world, …)` | World 的唯讀存取 | digest（第 5.9 節） |
| 執行中 | `OnMismatch(…)` | 比對的結果 | 要做的事（第 5.9 節的選項） |
| 任何時候 | `Name()`、`Version()` | — | 進 C 指紋，兩端策略不同就不連 |

- 「啟動時的檢查」與「執行中的比對」是同一個 interface 的兩組函式。比對的**傳輸**（誰把 C 指紋、digest 送給對方）不在權威 Module（第 8 節 N1）。
- 佔位策略（P2P 回滾）實作同一個 interface，`Check` 一律回報「還沒實作」（第 4.6 節）。
- 作者自寫的策略可以什麼都不檢查（等於不用權威 Module 的規則，但留著登記與指紋）。

### 4.5 檢查的時機（L5）

照 L5 的順序：編排層依登記檔 build → Kernel 建 Plan → 依同順序 finish → **權威檢查**。策略的 `Check` 在這一步跑一次。Kernel 有啟動錯誤時不跑（O-5：「因 Kernel 錯誤而未檢查」）。

`WorldRequirements()` 要在 **World 建立時**核對（L5 的宣告在建 World 時給）。誰核對：編排層建 World 時把策略的要求和 World 的宣告比，不合是啟動錯誤；或策略在 `Check` 時讀 World 的宣告。兩種都需要讀得到 World 的宣告——**統合 L6 的唯讀登記表沒有列 World 的這個宣告**（第 8 節 N4）。

### 4.6 佔位策略與它的啟動錯誤（P2）

- Engine 附 `PeerRollbackStrategy`（暫名；P2P 回滾）的佔位：名字與 interface 留著，`Check` 一律回報啟動錯誤，訊息寫明「這個策略還沒實作，可以自己實作 `AuthorityStrategy` 接上」。
- 錯誤的形狀：權威 Module 自己的錯誤碼（不擴充 Kernel 的 `StartupCode`，統合 O-5）。時機：編排層 finish 之後的權威檢查那一步（使用者 11-16「啟動時報」）。
- 佔位清單只有 P2P 回滾一個：使用者點名的例子只有格鬥遊戲（11-16）。

### 4.7 選了權威 Module 但沒選策略

使用者沒有判過。列成選項（第 6.13 節）。

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
| Z6、Z8、D-E15、D-E16（回滾的資料與順序） | 回滾重跑正確 | 回滾 | 在，但屬回滾 | **不在這份**（第 8 節 N2） | — |
| Z7（(c) 不寫 external） | 外部資料不進判定 | 同 Z1 | `external` 移出 Kernel；Z1 已涵蓋 | 併進 Z1 | — |
| D-E9（(c) 的 hook 衝突） | hook 沒有 before／after，兩端可能先後不同 | 歧義 | P5：照登記順序平手、C 指紋比得到 | 拿掉 | 5.3 |
| K.3 第 5 項（讀 (c) 的 (b) System 要和 (c) 寫者有顯式順序） | 呈現讀到的是同一 tick 的值 | 歧義 | P5 | 拿掉（不是錯誤） | — |
| X-2、D-E11、X-E1（含 (c) 的 schedule 宣告 Parallel 要守衛測試） | 平行執行不改結果 | 決定性 | Kernel 的 D-E17 讓 v1 任何 Parallel 都是錯誤 | 留在策略、目前不會觸發 | — |
| 「tick 只有一個來源、System 不得寫」（DI-KS 8）、「(c) 的時間只來自 tick」（K.3 第 11 項） | 時間不讓兩端分岔 | PvP | L2 已定歸伺服器權威策略 | 改寫成 5.1＋5.3 的特例 | 5.6、6.11 |
| C 指紋（`Fingerprint.cpp:330-406`）、G-5 兩份匯出 | 連線時兩端的判定程式相同；判定程式碼的來源可掃描 | `-no-ambiguity-detection` | 在 | 留；編號改 | 5.8、5.12 |
| `RejectedSpawn::emitterZone`（`World.hpp:86`） | 被丟的生成的明細帶區 | 11-15 | 統合 3.2：隨區移出、權威份定替代 | 權威 Module 用明細裡的 `emitter` 名字自己查 | 5.13 |

### 5.1 寫者規則（D-E2 的後繼）

**例子**：PvP 射擊。`ApplyDamage` 寫血量；同時有人加了 `ApplyHeal` 也直接寫血量。兩個寫者時，同一個 tick 先扣後補或先補後扣，照登記順序決定（P5），兩端一樣；但「受傷與補血誰先」是登記順序決定的，作者沒寫在任何地方。單一寫者的檢查逼作者把兩者寫成事件、由唯一的 `ApplyHealth` 套用合併規則（舊規格 B.3.1 的漏斗）。另一個例子：Boss 擊殺歸屬——兩個 System 都能寫「誰擊殺」，兩人可能都拿到。

**別家**：四家 ECS 都沒有寫者數量的規則（`single-writer-origin-and-pvp.md`；Bevy、Unity 反向：准多寫、Unity 有 WriteGroups）；三家網路套件都沒有 World 內的寫者規則，權威是「機器之間」——伺服器寫、Client 收（replicon 的 Client 只送 ack 與 message，`src/shared/backend/channels.rs:158-169`；Unity 的 Client 只送輸入與 RPC，`Runtime/Command/CommandSendSystem.cs:27-45`）。GYO 的單一寫者是 11-10 架構師的提案、11-11 使用者核准，案例全是 PvP（`single-writer-origin-and-pvp.md`）。

**這條在新架構裡守的是什麼**：P5 之後，多寫者的結果在兩端仍然一樣（登記順序相同、C 指紋比得到）。所以它**不再是決定性或可比對的必要條件**；它守的是遊戲設計的一條紀律：「判定只有一處寫，合併規則看得到」（P8：「要保證誰寫，別開shared」）。

**數到哪些寫者**（不論選哪個選項，數法都要定；卡片的建議，沒經使用者判）：

| 寫法 | 數不數 | 根據 |
|---|---|---|
| `Write<T>`、`ResMut<T>` | 數 | 啟動時看得出（統合 7.3） |
| 型別化 Commands 的 insert（對已存在的實體） | 隨第 5.7 節 | — |
| 型別化 Commands 的 remove | 數（成員關係結束之外，它讓值消失） | 舊 B.3 |
| `Spawn<Bundle<…>>`、`SpawnWithId<…>`、`InitialValue` 的值 | **不數** | L4（P6）；**改寫了 11-14 的 `-spawn-writer-funnel`**（第 8 節 N5） |
| `Despawn` | 不數 | K.3 第 4 項（11-11）；舊 Z5 拿掉後，誤刪判定的實體靠 digest 抓 |
| hook 宣告寫 `T` | 數 | 舊 B.3 |
| `ExclusiveWorld<Writes<T…>>` 列出的 | 數 | L7 |
| `ExclusiveWorld` 沒列的寫入、untyped `Commands`、從外面寫 | 看不到，不數 | L3、L7；靠 digest 與伺服器覆蓋 |
| 回滾 Module 的還原、伺服器覆蓋的寫入 | 不數 | 舊 B.3「Runtime 的回滾還原不算」；它們是機制替遊戲做的 |

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
2. **判定不依賴 World 之外與 stamp**（S-7、S-8、S-9）：判定的 System 沒有 `Local<T>`、不宣告 Io effect（`Diagnostics` 例外照舊）、不用 `Changed`／`Added`。理由照舊在：`Local` 不在 World、回滾不還原；Io 在重跑時重複；兩端的 stamp 毫無關係（G-3）。

拿掉的：D-E9（hook 衝突）——P5 之後 hook 照登記順序，C 指紋含 hook 的登記順序就比得到；K.3 第 5 項（(b) 讀 (c) 的順序）——同理，不是錯誤。

漏斗（`InitialValue`）的「寫請求的人在區的檢查裡當作也寫了目標」（`-funnel-one-principle`，11-14）照搬到策略：它是 Z1／Z2 的推導，不是新規則。

### 5.4 digest 禁 Shared（S-12）

**例子**：血量進 digest、又標 `Shared` 讓 `ApplyDamage` 與 `ApplyHeal` 都寫。

當初的理由與 5.1 相同。P5 之後兩寫者的結果兩端仍一樣，digest 比得到；所以 S-12 守的也是「判定只有一處寫」的紀律，不是比對的必要條件。它和 5.1 綁在一起：5.1 不數寫者，S-12 就沒有意義。選項在第 6.6 節。

### 5.5 外部輸入的入口（Z3、Z8 的一半、S-17）

**例子**：Match 收到玩家的按鍵封包，寫進「第 1200 個 tick 的輸入」；判定的 `ApplyInput` 從那裡讀。封包到的時刻（牆鐘）只決定寫進哪一格或因過期丟掉，不改判定。

舊的形狀：`role = Input` 的 (c) 資料、`ingress = true` 的 (b) System、schedule 的 `ingressStage`（Z3）、ingress 不能是獨占 System（S-17）、Input 不回滾（Z8）。統合把 `ingress`、`ingressStage`、`Role::Input` 都移出 Kernel（3.2）。

新形狀（卡片的建議）：入口是權威登記的屬性——`authority.Resource<InputRing>({.input = true})` 標記「外部輸入的暫存」，`authority.Ingress<&StageInput>(…)` 登記入口 System（權威 Module 代為呼叫 `AddSystem`）。策略檢查：只有入口 System 寫輸入的暫存；入口 System 不能是獨占的（S-17）。**「入口排在哪個 stage」不再檢查**：stage 是 Kernel 與編排層的，權威 Module 不認得 stage 名；排錯了（判定在入口之前讀）結果仍是決定性的（兩端排法相同、C 指紋比得到），只是晚一個 tick——那是遊戲的 bug，不是一致性的 bug。「Input 不回滾」（Z8）是回滾的規則（第 8 節 N2）。

**別家**：Unity Netcode 的輸入只准在 `GhostInputSystemGroup` 寫（`CommandSendSystem.cs:27-45`；只是文件的規定，`:31-34`，沒有啟動檢查）。

這一件沒有另列選項；使用者若要保留 stage 的檢查，在收口時提。

### 5.6 tick 只有一個來源、(c) 的時間只來自 tick（L2）

**例子**：Match 每 15.625 ms 推一個 tick。判定的 `Cooldown` 用「第幾個 tick」算冷卻；如果有人讀了牆鐘或每幀的 `deltaTime`，兩端的冷卻就會差一點。

L2 之後 tick 是時間 Module 的 Resource（存放在時間 Module 那一批定），由時間 Module 的固定步驅動者（一個獨占 System，`-fixed-step-in-world`）推。

卡片的看法：這兩條**不需要新的機制**，是 5.1 與 5.3 套在 tick 上的特例——

- **tick 只有一個來源**＝tick 的 Resource 是判定資料、單一寫者，寫者是時間 Module 的驅動者（它用 `ExclusiveWorld<Writes<Tick>>` 標註，L7；時間 Module 是我們自己的，能保證標註）。前提：5.1 留寫者規則。
- **(c) 的時間只來自 tick**＝別的時間 Resource（每幀的時間、牆鐘的取樣）不是判定資料；Z1 讓判定的 System 讀不到它們。被記錄的外部輸入（封包到達時刻）走 5.5 的入口。
- 函式體裡直接呼叫牆鐘（`std::chrono`、`Time::Now`）啟動時看不到，照舊靠判定程式碼的來源掃描（5.12）與 digest。

**別家**：三家的 tick 都是一般可寫的狀態，**沒有「只有一個來源」的檢查**——replicon 的 `ServerTick` 是 Resource、公開 `increment`，也可以關掉自動遞增改手動（`src/server/server_tick.rs:22-36`、`src/server.rs:67-73`）；lightyear 的 `LocalTimeline` 是 Resource、公開 `apply_delta`，回滾時也會推（`crates/core/core/src/timeline.rs:212-241`、`:300-313`）；Unity 的 `NetworkTime.ServerTick` 是 singleton 的公開欄位，伺服器與 Client 由不同的東西寫（`Runtime/PredictionTicking/NetworkTime.cs:46-73`）。所以這條是 GYO 自己的；它放在作者選的伺服器權威策略裡，合 P12。注意 lightyear 回滾時推 tick 的例子：若 GYO 的回滾 Module 也要在重算時推 tick，「回滾的還原與重算不算寫者」（5.1）要涵蓋它（N2）。

怎麼讓策略知道「哪個 Resource 是 tick」：權威 Module 依賴時間 Module（L20），可以直接認得時間 Module 公開的 tick 型別，替它做權威登記（時間 Module 不依賴權威）。選項在第 6.11 節。

「tick 在快照內」（DI-KS 8、12 的另一半）屬回滾（第 8 節 N2）。

### 5.7 對已存在的實體插入元件：建立還是修改（L4 交下來的）

**例子**：PvP 射擊。玩家撿到護盾，`PickupShield` 對玩家實體插入 `Shield{100}`；另一個 `ShieldSkill` 技能也對玩家插入 `Shield{50}`。玩家已經有護盾時，插入會**蓋掉**舊值（Bevy 的 `EntityCommands::insert` 文件：「This will overwrite any previous value(s) of the same component type」，另有 `insert_if_new` 保留舊值，`crates/bevy_ecs/src/system/commands/mod.rs:1381-1384`，6a911653f）。

- 算**修改**：兩個 System 插入 `Shield` 就是兩個寫者，單一寫者時要走漏斗（各自發事件、`ApplyShield` 決定合併）。
- 算**建立**：插入不算寫者，任意多個 System 可以插入；「蓋掉舊值」那一刻沒有被數到。

啟動時的檢查看不到「插入時目標有沒有這個元件」（那是執行期的事，L3 不攔），所以不能「沒有時算建立、有時算修改」——只能二選一。

**別家**：replicon 的協議分「插入、移除、despawn」（可靠、有序）與「修改」（不可靠）兩條 channel（`src/shared/backend/channels.rs:130-141`），但**伺服器對已有這個元件的實體插入，Client 當成修改**——可變的元件在 Client 已存在就改值、不存在才插入（`src/lib.rs:226-236`，原文「the client will treat it as a mutation」）；要分開，作者把元件標成不可變。lightyear 同 replicon。Unity 的快照不分插入與修改，會複製的元件在 prefab 製作時固定，執行中新加的不複製（`Documentation~/ghost-snapshots.md:48-65`）。Bevy 的 `on_insert` 在插入與取代時都觸發、`on_add` 只在新加時觸發（統合證據 `runtime-interception-other-engines.md` 引 `lifecycle.rs`）。

選項在第 6.7 節。

### 5.8 執行中插入、wire id 與 C 指紋用的編號（L5、L10）

**L5 已定**：伺服器權威要求它的 World 宣告不准動態插入（P9）。所以伺服器權威下，啟動後判定的 System 與資料不會變；C 指紋在啟動時算一次。這和別家一致：replicon、lightyear 在登記完成後再登記會 panic（下面的出處），Unity 的 RPC 也是（`RpcCollection.cs:141-142`）；Unity 唯一允許執行中加的是 ghost 型別，靠「伺服器送清單與雜湊、Client 不合就斷線」維持一致。作者自寫的策略若允許插入，就要自己做類似 Unity 的重新比對（不在第一版）。

**新發現（第 8 節 N3）**：現在的 C 指紋寫的是 Kernel 的編號（`Fingerprint.cpp:340-352` 的 `out.U32(id)`；`ComponentId` 照登記順序配）。Client 的模擬 World 有 Match 沒有的一般資料（預測的特效、除錯面板），**只要一般資料的登記插在判定資料之前，同一份判定資料在兩端就會拿到不同的 Kernel 編號**，C 指紋就不同、連不上——即使判定完全一樣。所以 C 指紋（和之後網路用的 wire id）不能用 Kernel 的編號。

**例子**：Match 先登記 `Health`（編號 0）；Client 先登記 `HitFlash`（編號 0）再登記 `Health`（編號 1）。

**別家**：
- replicon：wire id＝登記順序（`src/shared/replication/registry.rs:111-124`）；協議雜湊算登記的種類＋型別名＋**順序**（`src/shared/protocol.rs:13-20`、`:133-147`），連線時伺服器比、不同就斷線（`src/server.rs:279-292`）；build 完成後再登記會 panic（雜湊器在 finish 被移除，`src/shared.rs:123-130`）。
- lightyear：message／channel 依登記順序配 id、型別名依順序雜湊、finish 後再登記 panic 並寫明「Tried to register type … after the protocol was finished」（`crates/core/utils/src/registry.rs:36-46`、`:86-92`）。**反面教材**：這版元件的協議雜湊從來沒有被餵任何型別（`crates/replication/replication/src/registry/mod.rs:166`、`:211`），等於常數——指紋要有測試證明「改了登記就會變」（replicon 在 `protocol.rs:277` 釘死預期值）。
- Unity：元件依型別雜湊排序（`Runtime/Snapshot/GhostCollectionSystem.cs:51-65`），編號和登記順序無關；元件集合的雜湊含欄位的序列化屬性（`:134-150`），握手時比、不同就斷線（`Runtime/Rpc/RpcSystem.cs:266-273`）。ghost 的型別可以在執行中加，伺服器把清單與雜湊送過去，Client 發現同一個編號的雜湊變了就斷線（`GhostReceiveSystem.cs:470-486`）。

舊 GYO 的 N2：「元件在 wire 上的 id 是登錄順序索引……GYO 用 protobuf 欄位號＝契約」（`docs/architecture/plans/kernel/networking.zh-Hant.md:30`）。

選項在第 6.8 節。

**wire id 本身**歸屬誰：wire id 是網路／複製用的編號，權威 Module 只需要「C 指紋用的穩定編號」。兩者可以是同一個（判定資料剛好就是要複製的資料），也可以不是（有些判定資料不複製、有些複製的資料不是判定）。卡片的看法：**這份只定 C 指紋用的編號**；wire id 交給之後的網路／複製 Module，它可以沿用權威 Module 的編號（第 8 節 N1）。

### 5.9 digest 比對：何時比、比什麼、不一致怎麼辦（L3）

L3 已定：伺服器權威靠 digest 比對與伺服器覆蓋抓錯。這一節定 digest 的部分。

**例子**：Client 的除錯面板用 untyped `Commands` 把「冷卻剩餘」改成 0（它是判定資料，但伺服器不送，Client 自己預測）。伺服器覆蓋碰不到它，Client 的冷卻和伺服器分岔；下一發子彈 Client 以為能開、伺服器不准。digest 比對抓這種「覆蓋碰不到的分岔」。

**在伺服器權威裡 digest 的位置**：伺服器送來的資料會蓋掉 Client 的（5.10）；所以 digest 主要抓兩種：(1) 不在伺服器資料裡的判定資料分岔；(2) 回歸測試——同一串輸入，判定的結果和上次一樣（舊 PvP 的 `AuthorityDigest` 就是這種：情境×tick 的 golden，`tests/object_fps_pvp/AuthorityDigest.cpp`，master）。

**別家**：
- lightyear（`crates/deterministic/deterministic_replication/src/checksum.rs`）：只算掛了 `Deterministic` 標記的實體、只算登記了雜湊函式的元件，各自雜湊後 XOR 合併（`:6-7`、`:312-354`）；Client 算「最後確認輸入」那個 tick 的值、經不可靠的 channel 送（`:206-278`），伺服器每個 tick 算、留 30 tick（`:450-493`）；P2P 時互比（`:94-139`）。**不一致只記 error**，不回滾、不斷線、不重新同步（`:508-511`、`:400-408`）。
- Unity Netcode：沒有狀態的 checksum；只有除錯版的逐欄預測誤差統計（`Runtime/Snapshot/GhostPredictionDebugSystem.cs:3-10`）。
- replicon：沒有（子 agent 的掃描沒先用正例驗證，只當「沒找到」）。
- GGPO、GekkoNet：本機沒有原始碼，沒有讀；統合的證據（`authority-open-source.md`）只記到 GekkoNet 有 `GekkoDesyncDetected`。

**權威 Module 提供的**（卡片的建議）：`Digest(world, …)` 的計算（策略決定範圍）；比對的結果交給策略的 `OnMismatch`。**誰傳、何時傳**是網路／複製的事（第 8 節 N1）；第一版沒有網路，digest 只用在回歸測試（同一個 World 跑兩次、或 golden）。

三件要使用者判（第 6.9 節）：比什麼、何時比、不一致怎麼辦。

### 5.10 伺服器覆蓋（L3）

**例子**：Client 預測自己往前走了 3 格；伺服器說只走了 2 格（撞到牆）。Client 的位置被改成伺服器的值，再從那個 tick 用之後的輸入重算到現在，畫面平滑地拉回去。

這件事分三段，各屬不同的 Module：

| 段 | 做什麼 | 屬於 |
|---|---|---|
| 規定 | 哪些資料以伺服器為準、方向（伺服器 → Client） | **權威 Module**（伺服器權威策略：判定資料一律以伺服器為準） |
| 搬運 | 把伺服器的值送過來、寫進 Client 的 World | 網路／複製（**關聯表沒有**，第 8 節 N1） |
| 重算 | 預測的資料：比對歷史、還原、重跑、平滑 | 回滾 Module（L11：狀態觸發、逐元件歷史、平滑修正） |

這份只定第一段：**伺服器權威策略的判定資料以伺服器為準**；覆蓋的寫入不算寫者（5.1）。後兩段的交界回報統合（N1、N2）。

**別家**：三家都**沒有**擋 Client 寫的地方。差在蓋回的時機：Unity 每收到一份新快照，預測的 ghost 一律從快照還原再重跑、還原前不比對（`Runtime/Snapshot/GhostUpdateSystem.cs:25-50`），所以 Client 偷改的值撐不過下一份快照；replicon 靠伺服器的變更偵測決定送什麼（`src/lib.rs:228-243`），伺服器那邊沒變就不送，Client 偷改的只接收元件可能長時間不被蓋（子 agent 的推論）；lightyear 預測的元件比對確認值與歷史、不同才回滾（`crates/replication/prediction/src/registry.rs:590-613`）。所以**「伺服器覆蓋」抓得到多少錯，取決於搬運那一段是「每次無條件蓋」還是「伺服器有變才蓋」**——那一段不在這份（N1），但會決定 5.9 的 digest 要補多少。

### 5.11 (c) 區的獨占套用：新形狀（L12、L18）

**例子**：天氣在第 3600 個 tick 起霧（P11 的「兩端同一個 tick 自己換」）。狀態機的套用是一個獨占 System：跑離開的 schedule、改天氣的值、跑進入的 schedule。

**舊的**（`-zone-c-exclusive-apply`，11-15～16）：Kernel 加「(c) 區的獨占套用」——允許 (c) 的獨占 System；登記時列出它要寫的 (c) Resource 與它會跑的幾張 schedule；啟動時檢查它只跑列出的那幾張；C 指紋收進。回收清單說這個建立在 S-13 上，「這一批可能不需要或形狀不同」。

**現在**：L7 讓 `ExclusiveWorld` 能寫任何值，Kernel 不需要為 (c) 加任何東西。剩下的全在權威 Module（卡片的建議）：

1. 判定的獨占 System 經權威登記（`authority.Exclusive<&ApplyWeather>(…, {.runs = {"OnEnter_Fog", …}})`，權威 Module 代為 `AddSystem`），宣告它會跑的 schedule。
2. 它寫的判定資料用 `ExclusiveWorld<Writes<…>>` 列出（L7），算寫者（5.1）。
3. 策略把它當判定的 System 套 5.3：它列出的 schedule 裡的 System 也都要是判定的（跑判定的 schedule 的人自己是判定）。
4. 「只跑列出的那幾張」**執行期不檢查**（L3）；列出的清單進 C 指紋，兩端清單不同就連不上；列了沒照做由 digest 抓。這也回答 K0 宣告第 9 節的限界「巢狀的組成不在 C 指紋裡」——列出的部分進 C 指紋。

「權威端換、Client 採用」那一種不需要獨占的 (c) 套用：它是伺服器覆蓋（5.10）。

狀態機卡的 `RegisterStateMachine<S, Zone::C>`：`Zone::C` 改成「經權威 Module 登記」——狀態機 Module 不依賴權威 Module（單機也用狀態機），所以是**作者**在選了權威的產品裡，用權威 Module 登記狀態機的值與套用 System（或狀態機 Module 提供一個不依賴權威的掛點、權威 Module 去認它）。哪一種由狀態機卡重開時定；這份只要求「套用 System 能被權威 Module 登記為判定的獨占 System」（第 8 節 N7）。

### 5.12 G-5 的兩份匯出

舊 G-5：啟動時輸出「(c) 區資料集」與「(c) 區 System 的來源檔清單」；後者給守衛掃描（J-17：判定的程式碼不 include `<chrono>`、`<random>`，不用全域可變狀態……）。現在的實作：`FrozenRegistry::ExportZoneCSources`（`engine/kernel/src/Fingerprint.cpp:458`）。

統合 3.2 把它移到權威 Module。卡片的看法：**留**——L3 拿掉執行期攔截之後，「函式體裡讀牆鐘」只剩這個掃描與 digest 能抓；它便宜（啟動時一份清單）。權威 Module 用唯讀登記表裡的 `source`（`Registry.hpp:473-483` 已有）加自己的登記算出。資料集那一份就是 C 指紋的可讀版本。

### 5.13 `RejectedSpawn::emitterZone` 的替代

統合 3.2：「明細帶區」隨區移出，權威份定替代。明細已經有記錄者的名字（`RejectedSpawn::emitter`，`World.hpp:84`）。卡片的看法：Kernel 拿掉 `emitterZone`，**不加替代欄位**；權威 Module 需要時用 `emitter` 名字查自己的登記（是不是判定的 System）。LIM-7 的文字跟著改：「(c) 區的生成」改成「判定的 System 的生成」，Kernel 不擋這一點不變（LIM-7 的 owner 是架構師會話，改在減法批）。

---

## 6. 要使用者判的（統合收口之後逐件問）

每件先講例子，再列公平的選項；「建議」另外標明，是卡片的看法。

### 6.1 權威 Module 的名字

| 選項 | 系譜 | 代價 |
|---|---|---|
| `Authority`（權威；`engine/authority`） | 使用者一直叫「權威 Module」；replicon 自稱 server-authoritative（`src/lib.rs:2`）；lightyear 舊版有 `HasAuthority`，這版拿掉、改叫 `ControlledBy`（`crates/platform/web/CHANGELOG.md:214-216`） | P2P 沒有單一的權威，佔位策略的名字讀起來不搭 |
| `Consistency`（一致性） | 描述「兩端一致」的機制本身；三種策略都適用 | 新詞，要進同詞異義表；中文「一致性」容易和資料庫的一致性混 |
| `Netcode` | Unity Netcode for Entities | 範圍比這個 Module 大（含傳輸、複製） |
| `Determinism`（決定性） | 舊 GYO 的 determinism.md | 伺服器權威不只是決定性；和 `docs/architecture/target/determinism.md` 同名 |

建議（卡片）：`Authority`——使用者與所有決定都用這個詞，改名的成本高於「P2P 讀起來不搭」。

### 6.2 策略選在哪一層

**例子**：Client 有三個 World，只有模擬 World 要伺服器權威；渲染 World 不要。

| 選項 | 代價 |
|---|---|
| 每個 World 選一個策略（權威登記本來就經那個 World 的 `Registrar`） | 作者要對每個 World 選；同一個 `Product` 裡可以一個 World 伺服器權威、另一個不選 |
| 每個 `Product` 選一個策略，套到所有經權威登記的 World | 寫一次；但策略要知道哪些 World 有判定資料 |

建議（卡片）：每個 World——登記、Plan、Registry 都是每個 World 一份（K.3 第 6 項：渲染 World 有自己的 Registry），策略跟著登記走最少意外。

### 6.3 寫者規則：數不數、預設哪邊

例子與數法在 5.1。

| 選項 | 系譜 | 代價 | 需要它的 Feature |
|---|---|---|---|
| 數；經權威登記的資料預設單一寫者，作者標 `Shared` 才准多寫 | 11-11 的 `v8-arch-kernel-exclusive-default`（使用者核准，案例 PvP）；P8；縮到只對判定資料 | 判定資料多來源時要寫漏斗或標 `Shared`；四家與三家網路套件都沒有 | 伺服器權威（PvP）；Boss 擊殺歸屬這類「只能一個人拿到」 |
| 數；預設多寫者，作者標 `Exclusive` 才只准一個 | 反過來的預設；Bevy、Unity 的預設 | 忘了標就沒有保護；P7 的 11-11 決定要改 | 同上，但只有作者在意的資料 |
| 不數 | 四家 ECS、三家網路套件 | 「合併規則在哪」不再被逼著寫清楚；擊殺歸屬這類 bug 只能測出來 | — |

建議（卡片）：第一個。理由：作者選了伺服器權威，就是「作者自選的地方」（P12）；只數判定資料，單機與畫面層不受綁；P7 是使用者兩次確認過的方向，這個選項只縮範圍、不改方向。**但要說清楚**：P5 之後它不再是決定性的必要條件，守的是設計紀律（5.1）。

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
| 經權威登記＝判定資料（一個屬性都不用填） | 有些資料作者想複製給 Client、但不是判定（例：名字、皮膚）就不能經權威登記——那是網路／複製 Module 的登記，不是權威的 |
| 經權威登記、另填 `decisional = true／false`（舊的 `zone`） | 多一個欄位；但權威 Module 可以同時是「要複製的資料」的登記處 |

建議（卡片）：第一個——權威 Module 只管一致性，「要不要複製」交給之後的網路／複製 Module 自己的登記（lightyear、replicon 都是網路套件自己的登記）。

### 6.6 digest 禁 Shared

| 選項 | 代價 |
|---|---|
| 留：進 digest 的判定資料不能標 `Shared` | 判定資料只要進 digest 就一定單一寫者 |
| 拿掉：`Shared` 的判定資料也能進 digest | 多寫者的判定資料也比得到（P5：兩端先後相同） |

建議（卡片）：隨第 6.3 節——選「數、預設單一寫者」就留（同一條紀律）；選「不數」就沒有意義、拿掉。

### 6.7 對已存在的實體插入元件：建立還是修改

例子在 5.7。

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

### 6.9 digest：比什麼、何時比、不一致怎麼辦

例子在 5.9。三件分開判。

**比什麼**

| 選項 | 系譜 | 代價 |
|---|---|---|
| 全部的判定資料 | 舊 G-5 的「(c) 區資料集」 | 資料多時算得慢（可以抽樣頻率補） |
| 作者選的集合（舊 `DigestSet`） | 舊 GYO（11-14 留著但「不急定」）；lightyear 只算標了 `Deterministic` 的實體與登記了雜湊函式的元件（`checksum.rs`、`registry/deterministic.rs:47-75`） | 多一個登記；漏選的分岔抓不到 |
| 全部的判定資料，作者可以排除幾項 | 折衷 | 多一個屬性 |

**何時比**

| 選項 | 代價 |
|---|---|
| 每個 tick | 網路量（每 tick 幾個位元組）與計算；lightyear 伺服器每 tick 算、Client 每 tick 送一次 |
| 每 N 個 tick（N 由策略設定） | 晚 N 個 tick 才發現 |
| 只在測試與除錯時（第一版沒有網路，只有回歸測試） | 上線後的分岔要靠伺服器覆蓋自己修好、看不到 |

**不一致怎麼辦**（伺服器權威）

| 選項 | 系譜 | 代價 |
|---|---|---|
| 記錄與計數，等伺服器覆蓋修好 | lightyear（`checksum.rs:508-511`，只記 error） | 覆蓋碰不到的資料會一直分岔 |
| 記錄，並要求伺服器送一次完整的判定資料（重新同步） | 三家都沒有這樣做 | 要網路／複製 Module 支援「完整重送」 |
| 斷線 | 三家都沒有；GGPO、GekkoNet 沒讀 | 玩家被踢 |

建議（卡片）：比全部的判定資料（作者可排除）、每 N 個 tick、不一致時記錄與計數。理由：唯一有執行中 checksum 的 lightyear 只記 log；伺服器權威下伺服器的值是對的，斷線沒有必要；「要求完整重送」沒有先例、又要網路／複製 Module 有那個能力（N1），列成選項但不建議。**注意**：覆蓋碰不到的資料會一直分岔——碰得到多少，取決於搬運是「每次無條件蓋」還是「伺服器有變才蓋」（5.10），那一段定了之後這一件可能要重看。第一版沒有網路，只實作計算與回歸測試。

### 6.10 伺服器覆蓋的歸屬

5.10 的三段。權威 Module 只定「規定」那一段；「搬運」「重算」的歸屬回報統合（第 8 節 N1、N2），由統合與回滾份定。這一件在這份沒有選項。

### 6.11 「tick 只有一個來源」的檢查形狀

例子在 5.6。

| 選項 | 代價 |
|---|---|
| 不另立規則：權威 Module 替時間 Module 的 tick 做權威登記，套 5.1（單一寫者）與 5.3（判定只讀判定資料） | 依賴 5.1 留寫者規則；選「不數寫者」時這條要另寫 |
| 另立一條「tick 的 Resource 只有時間 Module 的驅動者寫」 | 多一條專用規則；不依賴 5.1 |

建議（卡片）：不另立——它就是單一寫者的一個例子。若 6.3 選「不數」，改成第二個。

### 6.12 G-5 匯出留不留

5.12。選項：留兩份／只留來源檔清單／都拿掉（只靠 digest）。建議（卡片）：留兩份（理由在 5.12）。

### 6.13 選了權威 Module 但沒選策略

| 選項 | 代價 |
|---|---|
| 啟動錯誤（「選了權威 Module，World X 沒有選策略」） | 明確 |
| 當作沒有規則（登記照走、不檢查） | 作者以為有保護其實沒有 |

建議（卡片）：啟動錯誤——和佔位策略的時機一致。

---

## 7. Design Invariants 與 Replacement Criteria（草案；核准後在實作批寫進權威 Module 的頁）

DI 的編號在實作批定（暫用 DI-A）。依賴第 6 節的選擇的，標「隨 6.x」。

| # | Design Invariant |
|---|---|
| DI-A 1 | **權威 Module 的職責是讓作者選的一致性策略有機制可用**（4.1 的清單）。不攔執行期的寫入、不做回滾、不存 tick、不傳輸 |
| DI-A 2 | Kernel 不認得權威：權威 Module 的宣告只存在權威 Module；Kernel 收到的是一般的登記。守衛：Kernel 目錄的掃描（統合附錄的式子） |
| DI-A 3 | 策略是 interface＋實作（L21）：官方實作與作者的走同一個插口；interface 的函式與時機（4.4）是契約，官方實作的內部不是 |
| DI-A 4 | 檢查只在啟動時（L5 的權威檢查那一步）與 World 建立時；執行中只計算與比對，不拒絕寫入（L3） |
| DI-A 5 | C 指紋只看判定的部分，用的編號與一般資料的登記無關（隨 6.8）。守衛要證明兩個方向：判定的登記改了指紋就變；一般資料的登記改了指紋不變（lightyear 的元件雜湊沒餵資料、等於常數，是反面例子） |
| DI-A 6 | 佔位策略選了就是啟動錯誤，訊息寫明還沒實作 |
| DI-A 7 | 伺服器權威下判定資料以伺服器為準；覆蓋與回滾還原的寫入不算寫者 |
| DI-A 8 | （隨 6.3）經權威登記的判定資料預設單一寫者；寫者的數法照 5.1 的表 |

**Replacement Criteria**（L17：不數次數，出現就停下來問）：

1. 權威 Module 的職責清單（4.1）要多加一項，或判不出某樣東西屬權威、回滾、時間、網路哪一個。
2. 要讓伺服器權威成立，必須在 Kernel 加任何權威專用的欄位、分支或觀察點（L3、P3 的反面）。
3. 伺服器權威策略的檢查，在作者照正常業務寫（統合 7.3 的有型別寫法）時誤報，而且改寫法要繞過機制（不是用漏斗或 `Shared`）。
4. 第二個官方策略（P2P 回滾）實作時，interface（4.4）要改——表示 interface 是照伺服器權威的形狀寫的。

---

## 8. 回報統合的新關聯與缺口

這些是關聯表沒有、或會碰到別份的。**這份不判**，已用訊息回報統合那張卡。

| # | 事項 | 碰到誰 | 為什麼要統合定 |
|---|---|---|---|
| N1 | **網路／複製這一層不在關聯表**。伺服器覆蓋的「搬運」、連線時交換 C 指紋、執行中傳 digest、wire id，都要一個呼叫者 | 權威、回滾（狀態觸發要收到伺服器的狀態）、之後的網路 Module | L11 的回滾「狀態觸發」與 L3 的「伺服器覆蓋」都假設有它；它屬哪一層、第一版要不要有，沒有人定 |
| N2 | **回滾的宣告與檢查歸誰**：`rollback = Yes／No／Derived` 的屬性、Z6（(c) 寫的要能回滾）、Z8（輸入不回滾）、D-E15（入口在回滾時不跑）、D-E16（Derived 的順序）。作者是在權威登記裡填、還是在回滾 Module 另登記一次；伺服器權威策略「選用回滾策略」的接法（L11）——由權威策略指定回滾策略、還是作者各自選、權威檢查兩者相容 | 權威、回滾 | 兩份都可能說是對方的；lightyear 是在同一個元件的登記上串預測（現在寫 `app.component::<C>().predict()`，舊名 `add_prediction` 已標 deprecated，`crates/replication/prediction/src/registry.rs:741-748`）——宣告在同一個登記上，但預測是另一個 crate 的擴充 |
| N3 | **C 指紋不能用 Kernel 的編號**（5.8）：Client 與 Match 的 Kernel 登記不同。L10「C 指紋用唯讀登記表計算」要補一句「編號不用 Kernel 的 id」 | 權威（L10）、游標（Plan 的指紋若也要跨端比，同一個問題） | 會影響 L10 與 J-18／J-20 的 golden 怎麼重產 |
| N4 | **World 的「准不准動態插入」宣告要讀得到**（4.5）：策略在啟動時要求它。L6 的唯讀登記表沒有列 World 建立時的選項 | Kernel 公開介面（L5、L6）、編排層 | 動到 Kernel 的讀取面 |
| N5 | **L4 改寫了 `v8-arch-gyo2-spawn-writer-funnel`（11-14）**：那一條定「生成的 bundle 照舊算寫者」並否決了「生成不算寫者」；L4 的「`Spawn` 帶的值不算」翻過來。統合 2.2 沒有列這一列。漏斗工具（PR #226、#229）的前提跟著變：只剩「多來源修改」要它 | 統合 2.2、漏斗工具 | L4 的原話「這個可以參照Bevy」沒有直接回應 11-14 否決的理由（「Despawn 是機制，不能當成普通的實作參考」）；要不要請使用者再確認，統合判 |
| N6 | **代為登記時的 owner**（4.3）：權威 Module 代作者呼叫 `Registrar` 時，owner 應是作者的登記單位，不是權威 Module。統合 3.5「編排層開 `Registrar` 時給 owner」——同一個 `Registrar` 被權威 Module 拿去用，owner 自然是作者的；若編排層另給權威 Module 一個 `Registrar`，就會錯 | 編排層骨架批（3.5） | 一句話的約定，但寫錯會讓 `InitialValue` 的「只有擁有者能宣告」誤報 |
| N7 | **狀態機 (c) 區的接法**（5.11）：狀態機 Module 不依賴權威 Module（單機也用），(c) 的套用 System 怎麼被權威登記 | 狀態機卡（L18） | L18 只說 `Zone` 隨 L12 |
| N8 | **時間 Module 要公開 tick 的型別與驅動者，並在驅動者上標 `Writes<Tick>`**（5.6） | 時間 Module（L2、T1／T3） | 時間那一批定 tick 的存放時要知道 |

---

## 9. Architecture Delta（AGENTS §3；只寫和統合第 4 節的差分）

1. **Feature**：統合 5.4「伺服器權威」（PvP Match／Client；使用者 11-11、11-16）、「作者自寫策略、P2P 回滾」（格鬥遊戲，只佔位）；L5 的指令集（伺服器權威要求不准插入）。
2. **現在的問題**：寫者、區、digest、C 指紋、G-5 都在 Kernel（`Registry.cpp:1440-1490`、`:1240-1300`；`Fingerprint.cpp:330-406`、`:458`）；Kernel 對單機也執行；C 指紋用 Kernel 的編號（新發現，5.8）。
3. **變化的邊界**：新 Module（權威）；它的公開介面（權威登記 API、`AuthorityStrategy`、`ServerAuthorityStrategy`、佔位策略、指紋與 digest 的計算、G-5 的匯出）。
4. **影響的 Module／Product／Tool**：權威 Module（新）；Kernel（減法批拿掉的部分，統合已列）；時間 Module（公開 tick 型別、驅動者標 `Writes<…>`，N8）；回滾 Module（N2）；狀態機 Module（N7）；編排層（佔位策略的錯誤彙整、World 要求的核對）；測試：統合 7.2 標「移出 → 權威」的 J-14、J-15、J-16、J-17、J-27 的守衛 id、J-34 的 D-E9、J-38～J-41、J-47 的 Z1、`InitialValueTests` 的 Z1／Z2／Z3／Z6、`SkeletonTests` 的寫者案、`zone_c_export_check.py`、`zone_c_fixture/`——**逐案在實作批判斷**；其中 Z6、D-E15、D-E16 的案等 N2。
5. **Dependency Direction**：新增 權威→Kernel、權威→編排層、權威→時間、權威→回滾；產品→權威。沒有反向。
6. **Ownership**：寫者、區、digest、C 指紋、G-5：Kernel → 權威 Module（統合已列）；tick 的單一來源：Kernel → 權威策略（L2）。owner 是架構（架構師會話）。
7. **為什麼不能更小**：權威的規則留在 Kernel 已被使用者否決（`-kernel-is-ecs-no-restrictions`）；不設權威 Module、規則全交產品，伺服器權威的檢查會在每個 PvP 產品重寫（使用者 11-16「權威檢查我想放Module，這個是底層能力」）。

### 9.1 基盤的根據（AGENTS §2）

**為什麼算基盤**：伺服器權威的產品、之後的網路／複製 Module、回滾 Module、狀態機的 (c) 區都會積在它的 interface 與 C 指紋上；C 指紋是兩端握手的內容，一改就不相容。

| 根據（出處） | 要求 |
|---|---|
| PvP 伺服器權威（使用者 11-11、11-16） | 啟動時的檢查（5.1～5.6）；C 指紋（5.8）；伺服器覆蓋的規定（5.10） |
| Client 三個 World、只有模擬 World 判定（11-11） | 策略選在 World（6.2）；C 指紋不受一般資料影響（5.8） |
| 格鬥遊戲 P2P（11-16，佔位） | interface 不照伺服器權威的形狀寫死（RC 4） |
| 作者自寫策略（11-16）、策略的構造（L21） | interface＋官方實作 |
| 指令集（11-17） | 不准插入的 World 由策略要求（L5） |
| 參考：lightyear、replicon、Unity Netcode | 權威的宣告在網路套件自己的登記（4.1）；連線時比協議雜湊、不同就斷線（5.8）；執行中不擋寫入、靠蓋回（5.10） |

**實作薄的部分**：第一版只實作登記、啟動時檢查、C 指紋與 digest 的計算、G-5 匯出、佔位策略；比對的傳輸、伺服器覆蓋的搬運等網路／複製（N1）。

### 9.2 預算

這份不碰執行效率。digest 的計算量在選了「每 N 個 tick」之後由實作批量。

---

## 10. 批次（草案）

統合第 8 節把權威 Module 排在 `v8-arch-gyo2-kernel-subtraction`＋生成世界的同一條 PR 線（守衛不會有「從 Kernel 拿掉、新家還沒接上」的空窗）。這份照它，不另排。

| 內容 | 依賴 | 模型／ギア（navigator 上段：公開介面、Design Invariants） |
|---|---|---|
| 權威 Module 的骨架：登記 API、代為登記、`AuthorityStrategy`、佔位策略、啟動錯誤 | 編排層骨架；本宣告與統合核准；N2、N4、N6 定案 | 實作 Opus／high；審查 Fable／high |
| 伺服器權威策略的檢查（5.1～5.6、5.11）＋從 Kernel 搬來的守衛逐案判定（§14.4） | 上一步；時間 Module（N8） | 實作 Opus／high；審查 Fable／high；§14.4 檢證者 Fable／high |
| C 指紋（新編號）、digest 計算、G-5 匯出、回歸測試用的 digest | 上一步 | 同上 |

估計：骨架 6～10 h；檢查與守衛搬遷 10～16 h（約 100 個測試案例與它們的突變要逐案判，數字從統合 7.1 的標籤估，實作批重算）；指紋與 digest 4～6 h。

---

## 11. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；時間估計是讀程式量估的。
- 回滾、事件游標、tick 的存放不在這份；第 8 節的交界由統合定。
- 網路／複製這一層沒有人定（N1），第 5.9、5.10 節只定權威 Module 那一段。
- 別家的做法來自子 agent 讀本機原始碼（第 0.1 節），卡片抽查了一部分；Unity 讀的是非官方鏡像。
- 舊規格只取設計思想；舊的具體做法（`InputRing`、`HostClockSample`、`Restore`）不帶進來。
- 修訂 0 尚未送換模型檢查（收口時三份一起送，統合卡負責）。

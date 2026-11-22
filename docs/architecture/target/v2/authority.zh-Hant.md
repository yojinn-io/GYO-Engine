# 權威 Module（`engine/modules/authority`）

更新：2026-11-22（`v8-arch-gyo2-batch4-followups`：DI-A 2 的守衛改成常設的寫法）；2026-11-21（新頁，`v8-arch-gyo2-authority-rollback` 第 5 步）。Owner：架構。程式：`engine/modules/authority/`（`GYO::Engine::Authority`，target `GYO::Authority`，登記單位 `AuthorityModule`）。行為測試：`tests/common/authority/`（ctest `authority.authority_socket`、`authority.authority_registrations`、`authority.authority_checks`、`authority.authority_consistency`、`authority.authority_sources`、`authority.sources_scan`，標籤 `cpu`）；突變清單 [`scripts/authority_mutations.json`](scripts/authority_mutations.json)（`au-`）。產品那一側的來源掃描：`tests/common/authority_scan/`（原 `zone_scan`，第 5 步照命名規範改名；規則 ZS1～ZS7 不改名）。

由來：[權威份](authority-module-declaration.zh-Hant.md)（修訂 5，已核准）第 4、5、7 節；[第 4 批宣告](authority-rollback-declaration.zh-Hant.md)第 4、6 節與第 11.2 節（方向使用者 2026-11-20 核准：「DI 文字方向照建議」）。Design Invariants 的最終文字使用者 2026-11-22 核准（[合併前核准](authority-rollback-merge-approval.zh-Hant.md)）；和權威份字面不同的地方在條末標「實作」。

## 一個例子

PvP 射擊的 Match 與 Client 都選 `ServerAuthorityStrategy`。作者在產品自己的 `build` 寫 `AuthorityEntry(s).Component<Health>("Health")`：策略在**這一刻**用作者的 `Registrar` 把 `Health` 登記進 Kernel，再替作者呼叫 `rollback.Component<Health>()`（Client 有回滾策略，會生效；Match 沒有，不生效）。輸入的入口 `authority.Ingress<&ReadInput>(…)` 由策略先掛上「重跑時不跑」再 `AddSystem`。啟動時策略檢查：World 不准動態插入、`Tick` 只有時間 Module 的驅動者寫、可回滾 schedule 上寫的判定資料都能回滾（Z6）。連線前兩端比**一致性指紋**（只看兩端該相同的判定部分，Client 多了畫面用的資料、除錯面板、回滾的登記都不影響），執行中可以比 **digest**（判定資料的雜湊和）。Kernel 不認得「權威」：它收到的是一般的登記。

## 1. 位置與依賴

```text
產品 ──→ 權威 Module ──→ 回滾 Module ──→ 時間 Module
              │              （公開登記）     （Tick、TickWriters）
              ├──→ 編排層（登記單位、ModuleBuffer）──→ Kernel
              └──→ Kernel（唯讀登記表、World 的讀取）
```

- 登記檔 `config/modules.csv` 的 `authority` 列（`rollback` 之後），`depends=world_time;rollback`（只寫 Module 之間的依賴，照 include；對 core、ecs、編排層的連結手寫在 CMakeLists），`default=yes`：沒有權威登記的 World 不用選策略。
- 標頭：`AuthorityStrategy.hpp`（插口、`CheckContext`、`FingerprintContext`、`AuthorityOptions`、P2P 佔位）；`Registrations.hpp`（`AuthorityEntry`、`RegisterContext`、紀錄與 World 的 Resource `AuthorityRegistry`）；`ServerAuthorityStrategy.hpp`（官方策略）；`Consistency.hpp`（一致性指紋、digest、判定資料集、G-5 匯出、`AuthorityFailure`）；`AuthorityModule.hpp`。

## 2. 登記入口（`AuthorityEntry`）

| 入口 | 意思 |
|---|---|
| `Component<T>(name, options)`、`Resource<T>(name, options)` | 作者的判定資料；`T` 要有雜湊函式（編譯期檢查）。`options`：`input`（判定的輸入，Z8）、`digestExclude`、`rollback`（`Rollback` 預設／`NoRollback`／`Derived`／`Self`：作者自己登記） |
| `Message<M>(name, capacity)` | 判定訊息（不進 digest；上限進一致性指紋） |
| `Mark<T>()`、`MarkResource(name)` | 別的 Module 已登記的型別：只記下、不再登記；依型別的進 digest，依名字的本批不進（實作） |
| `Ingress<&F>(options)` | 外部輸入的入口 System：代為 `AddSystem` |
| `Runs(system, schedules)` | 會巢狀跑別的 schedule 的獨占 System |
| `OneEndOnly(name, reason)` | 寫判定資料、只在一端有的 System（hook、observer 也可以，名字三張表都查；實作）：不進一致性指紋與 G-5，判定資料集列出 |

沒有 `==` 的型別不能由策略代為登記成可回滾：斷言，訊息請作者選 `RollbackChoice::Self` 並自己 `rollback.Component<T>({.equal = …})`（實作）。

## Design Invariants

<a id="di-a-1"></a>
**DI-A 1 職責**：權威 Module 的職責是讓作者選的一致性策略有機制可用——作者的權威登記（代為登記進 Kernel）、策略的插口與佔位策略、代為登記（Kernel、回滾、重跑時不跑）、啟動時的檢查、一致性的計算（一致性指紋、digest、G-5 的兩份匯出）。不攔執行期的寫入、不做回滾、不存 tick、不傳輸、不檢查判定資料的讀寫。

<a id="di-a-2"></a>
**DI-A 2 Kernel 不認得權威**：權威的宣告只存在權威 Module；Kernel 收到的是一般的登記。守衛：Kernel 由 `kernel_guard.py` K5 的 include 准許清單與連結集 `gyo_expect_links(gyo_kernel …)` 守；編排層由 `orchestration_guard.py` O4 的准許清單與連結集 `gyo_expect_links(gyo_orchestration …)` 守；`world_time` 由連結集（WT-4）守。不經連結的 include（相對路徑 `..`、另加標頭目錄）與用到權威的名字，交每批的設計審查（`v8-arch-gyo2-include-bypass-to-review`）。Kernel 自己長回區、寫者等舊概念不屬這條，由[第 4 批宣告](authority-rollback-declaration.zh-Hant.md) 9.5 的「拿掉的名字不回來」管。

<a id="di-a-3"></a>
**DI-A 3 策略是 interface＋實作**：官方實作與作者的走同一個插口、沒有特權。`Name`、`Version`、`OnRegister`、`Check`、`ConsistencyFingerprint` 與它們的時機是契約，每個掛點只收一個自己的情境物件（實作）；`Digest`、`OnMismatch` 的形狀不在這次的契約內（官方 digest 是 World 的函式，`Consistency.hpp`）。守衛：`authority socket: an author's strategy…`、`authority checks: AU-E8…`（作者繼承官方實作改掉一個掛點）。

<a id="di-a-4"></a>
**DI-A 4 只在啟動時檢查**：檢查只在編排層 `Finish` 之後的 `Check`；執行中只計算，不拒絕寫入。

<a id="di-a-5"></a>
**DI-A 5 一致性指紋**：只看兩端應該相同的判定部分；編號用權威登記的順序、型別與存取都用名字（不用 Kernel 的編號），和一般資料的登記無關。內容照權威份 5.5 的表：策略的名字與版本、判定型別（權威登記的順序、種類、名字、訊息上限、`input`、`digestExclude`；標記的型別與標記方式）、啟動時看得到寫判定資料的 System（扣掉 `OneEndOnly`）的存取、掛載（schedule、stage）、執行條件與所屬 set 的執行條件（set 的條件也決定它跑不跑；實作）、它們在各 Plan 的相對順序與其間有沒有同步點、hook 的順序、元件與存取、observer 看的對象與存取（依名字，不收先後）、判定元件的 `InitialValue`、hook 清空輪的上限（schedule 的選項）、`Runs`。回滾的「重跑時不跑」條件與它帶進來的 `Resimulation` 讀取不進（回滾的登記只在一端；實作）。守衛證明兩個方向：`authority consistency: the fingerprint changes…`、`…each item of the fingerprint's content is in it`（上面每一項各一條 CHECK）、`…does not change…`（一般資料與它的寫者、`OneEndOnly` 的面板與 hook、只有 Client 有的回滾策略、步的 schedule 與「重跑時不跑」）。

<a id="di-a-6"></a>
**DI-A 6 佔位策略**：選了 `PeerAuthorityStrategy` 是啟動錯誤 AU-E1，訊息寫明還沒實作；它仍把型別登記進 Kernel，讓作者看到的是 AU-E1 而不是 Kernel 的 R-E4（實作）。守衛：`authority socket: the peer-to-peer placeholder…`、`authority registrations: the peer-to-peer placeholder still puts the types into the kernel…`。

<a id="di-a-7"></a>
**DI-A 7**：伺服器權威下判定資料以伺服器為準（規定在權威，搬運在網路 Module，重算在回滾 Module）。

<a id="di-a-8"></a>
**DI-A 8 不數寫者、不檢查讀寫**：伺服器權威策略不數寫者、不檢查判定資料的讀寫（P13、P14）；同一張 schedule 內收進一致性指紋的 System 之間的先後由指紋蓋到，跨 schedule、只在一端、untyped 的寫入只靠 digest 與覆蓋。

<a id="di-a-9"></a>
**DI-A 9 只讀公開的**：權威不讀回滾、時間的內部，只讀公開登記（回滾的 `Resolve` 與 `RollbackRegistry`、時間的 `Tick` 與 `TickWriters()`）；不依賴狀態機；回滾、時間、狀態機都不依賴權威。

<a id="di-a-10"></a>
**DI-A 10 標記不重複登記**：別的 Module 已登記的型別與 System 只標記、不重複登記；標記、`Ingress`、`Runs`、`OneEndOnly` 對不到唯讀登記表是啟動錯誤 AU-E4。守衛：`authority registrations: a mark only records…`、`authority checks: AU-E4…`。

<a id="di-a-11"></a>
**DI-A 11 代為登記在登記當下**：代為登記（進 Kernel、進回滾、重跑時不跑）都在作者呼叫權威登記入口的當下做（`OnRegister`），一定在建 Plan 之前；策略在建 World 時選定（`AuthorityOptions`）。沒選策略時入口仍把型別放進 Kernel（讓作者看到 AU-E2 而不是 R-E4）、也把入口 System 加進去，`Check` 報 AU-E2（實作，宣告 4.2；任何一種權威登記都算「有權威登記」）。守衛：`authority registrations:` 各案（加入口 System 這一半從外面看不到：World 一定停在 AU-E2，不立守衛）。

<a id="di-a-12"></a>
**DI-A 12 digest**（第 4 批宣告 11.2 加）：digest 的每個值用 Kernel 定的同一個雜湊客製點（`HashValue`），以型別的權威登記序號一起雜湊，合併和順序無關（相加 mod 2^64：兩個相同的值不會像 XOR 那樣互相抵銷；實作）。不含實體 id（宣告 4.4：本批不定）。守衛：`authority consistency: the digest is order free…`（同一組值換到別的實體上 digest 不變；相同的值不抵銷；排除；tick）、`…hashes each value with its type's number`。

### Replacement Criteria

1. 權威 Module 的職責清單（DI-A 1）要多加一項，或判不出某樣東西屬權威、回滾、時間、網路哪一個。
2. 要讓伺服器權威成立，必須在 Kernel 加任何權威專用的欄位、分支或觀察點。
3. 伺服器權威策略的檢查，在作者照正常業務寫（有型別的參數）時誤報，而且改寫法要繞過機制。
4. 第二個官方策略（P2P 回滾）實作時，interface（DI-A 3 的契約部分）要改。
5. `OneEndOnly` 或 `Runs` 這類標記，開始需要每加一個新機制（Module）就多一種，或作者要為了過檢查而標記一般的遊戲邏輯。

## 3. 伺服器權威策略的檢查（`ServerAuthorityStrategy::Check`）

| 碼 | 檢查 | 由來 | 守衛 |
|---|---|---|---|
| AU-E1 | P2P 佔位策略 | 4.6 | `authority socket:` |
| AU-E2 | 有權威登記卻沒選策略（Module 的 `Check`） | 4.7 | `authority registrations: without a strategy…` |
| AU-E3 | World 准動態插入 | 5.5 | `authority checks: AU-E3…` |
| AU-E4 | 標記、`Ingress`、`Runs`、`OneEndOnly` 對不到（被 `RemoveSystem` 拿掉的 System 算對不到） | DI-A 10 | `…AU-E4…` 兩案、`…a mark only records…` |
| AU-E5 | `Tick` 的寫者不在時間 Module 的 `TickWriters()`（System、hook、observer；`Writes<Tick>` 標註也算） | 5.4 | `…AU-E5…` 兩案 |
| AU-E6 | Z6：可回滾 schedule（步的與巢狀的）上、重跑時會跑的 System 寫的判定資料，要可回滾或 Derived；World 有步的 schedule 時，hook 與 observer 寫的判定資料要可回滾（不能是 Derived）。`Tick` 例外、有 `rollback.Input` 歷史的輸入例外（重跑前會寫回；實作），訊息不看；延後寫、`Writes<…>` 標註、經 `InitialValue` 的漏斗（沿鏈；原地寫與移除請求不算）都算寫 | 5.11 | `…AU-E6…` 五案 |
| AU-E7 | Z8：判定的輸入登記成回滾狀態或 Derived（`rollback.Input<T>` 不算；元件與 Resource 都查） | 5.11 | `…AU-E7…` |
| AU-E8 | D-E15：可回滾 schedule 上的入口不在「重跑時不跑」 | 5.11 | `…AU-E8…` |
| AU-E9 | 可回滾 schedule 上的 `Runs` System 會跑的 schedule 沒登記成巢狀（步的 schedule 本身不算巢狀登記） | 5.11 | `…AU-E9…` 兩案 |

「啟動時看得到的寫」：有型別的參數（立即與延後）、`ExclusiveWorld<Writes<…>>` 標註、hook 與 observer 的存取、同步點經 `Registrar::InitialValue` 替插入請求的人填的元件（沿鏈）。看不到 untyped 的寫入與沒標註的獨占寫入（L3、L7）。

## 4. 本批的限界

- Z6 不看「重跑時不跑」的 System（入口寫判定的輸入，重跑時不會再寫；實作）。權威份 5.11 寫「掛在可回滾 schedule 上的 System」，這是它的細化。
- 判定的輸入（Resource）由伺服器權威代為登記 `rollback.Input<T>()`（策略的深度；使用者 2026-11-22 選 (c)，對照 lightyear 的 InputBuffer、Unity Netcode 的 `IInputComponentData`：宣告一次輸入，紀錄自動就有）。宣告輸入不等於選了預測：紀錄只在選了回滾策略的 World 建（R-4），Match 與不預測的 Client 什麼都不建。作者選 `Self` 或 `NoRollback` 時不代為登記。元件的輸入沒有紀錄可代為登記（回滾 Module 只記 Resource 的輸入），照舊受 Z6 檢查。守衛：`authority registrations: a decision input brings its recorded history…`、scenario 第 5 段（作者不再寫這行）；突變 `au-input-history-not-recorded`、`au-input-self-recorded`。
- 有 `rollback.Input` 歷史的輸入不受 Z6 檢查（審查 1-5：輸入不能登記成可回滾（Z8），步上的 System 消耗它時沒有合法出口；回滾在重跑每一步前把它寫回）。沒有歷史的輸入在步上被寫照樣 AU-E6。
- 一致性指紋不收 `KernelSettings::hookRoundLimit`（World 的設定值，執行時才讀），只收 schedule 的選項。
- System 自己讀 `Resimulation`、又帶回滾的「重跑時不跑」條件時，指紋看不出前者（條件的讀取和它分不開）。
- G-5 的根目錄比對：Windows 的磁碟路徑不分大小寫（照 Kernel 原本的匯出）；本機的測試走不到磁碟路徑，靠 windows-x64 的 L1。
- 「啟動報告列出所有 `OneEndOnly`」（權威份 4.3）：由 `DatasetReport` 給，產品在啟動時呼叫；編排層沒有自動的啟動報告。
- 依名字標記的型別（`MarkResource`）不進 digest：Kernel 沒有逐 id 的雜湊入口，權威手上只有名字（實作）。狀態機的值要進 digest 時，狀態機的那一批用型別標記或另議。
- 時間 Module 目前只公開驅動者（`RunFixedSteps`），沒有對時 System；一致性指紋「扣掉對時 System」等時間 Module 分開公開對時名單時再接。
- 時間 Module 的步長等設定不進一致性指紋（延到網路 Module 的宣告：放指紋或放握手）。
- digest 何時比、不一致怎麼辦，延到網路 Module；本批只用在回歸測試與 scenario 第 5 段（`duel`）。
- `authority_scan` 只收 `gyo.authority-sources`；Kernel 舊名 `gyo.zone-c-sources` 的清單第 6 步起拒收（契約頁改名為 [authority-sources](../../authority-sources.zh-Hant.md)）。
- 新增的錯誤型別 `AuthorityFailure`（E2：`NoAuthority`、`SourceOutsideRoot`），在合併前核准時給使用者看（宣告 10.3：新型別照後綴取名）。

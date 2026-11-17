# 權威 Module：事前宣告（子份；修訂 4，最後一版）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。核准本宣告不等於 AGENTS §14.4 的判定變更核准**：從 Kernel 搬來或退役的每一個守衛判定，在實作那一批的事前宣告逐案列出、逐條核准。
> 批次代號：`v8-arch-gyo2-authority-module`。
> **這份是「Kernel 與編排層的邊界」拆出來的子份之一**（`v8-arch-gyo2-boundary-split-integrator-first`）。統合是 [`kernel-app-boundary-declaration.zh-Hant.md`](kernel-app-boundary-declaration.zh-Hant.md)（分支 `claude/gyo2-kernel-app-boundary-4af68c`，修訂 28，16c84630）。**以統合為準**；統合第 15 節的判定（15.3 權威、15.6 確認清單）是本版的依據。
> 範圍：統合關聯表 L12。回滾的資料登記與官方回滾策略在回滾份（L11、L24）；事件游標在游標份（L13）；tick 存在哪裡在時間 Module（L2）。
>
> **修訂紀錄**：修訂 0（0b802f5e）回報統合 N1～N8。修訂 1（03320e5e）接統合修訂 18、19：Z6、Z8、D-E15 的交叉檢查、豁免寫者的登記、事件的登記與只標記的入口、確認清單。修訂 2（4a4f177b）接第二輪交叉檢查：代為登記在登記當下、tick 驅動者、C 指紋的內容表、狀態機用名字標記。修訂 3（9efe5e1a）接使用者「伺服器權威不數寫者」：連帶影響表、規則 1 拆兩半成選項。**修訂 4（2026-11-17）**：統合第 15 節把題目全部定完（使用者核准確認清單；其餘由統合照 L17 判，見第 6 節）、第三輪交叉檢查（`build/evidence/gyo2-kernel-app-boundary/four-docs-adversarial-check-r3.md`）仍適用的部分照改。主要變化：判定資料的讀寫規則兩半都拿掉，連帶拿掉規則 2、System 的區、入口的先後、豁免寫者的登記；「tick 只有一個來源」用專用檢查；C 指紋與 wire id 用權威登記順序；事件改名 `Message`。這一版是重寫，修訂 3 以前的段落在 git 歷史，不逐句標刪改。
> **「N」的三種意思**：N1～N8＝本份修訂 0 回報統合的編號（第 8 節）；N11～N17＝命名規則（`rebuild-naming.zh-Hant.md`）的條號；「舊網路設計稿的 N2」＝`docs/architecture/plans/kernel/networking.zh-Hant.md` 的第 2 條。

---

## 0. 依據

| 項目 | 版本 |
|---|---|
| 程式 | `gyo2` f00433b0（Kernel 的程式和統合讀的 cb6bccca 相同） |
| 決定表 | HANDOFF，分支 `claude/gyo2-time-design` |
| 統合 | 修訂 28，16c84630（第 15 節；15.6 確認清單使用者核准，`v8-arch-gyo2-confirm-derived-removals`） |
| 回滾份 | 修訂 4，1f83041f（分支 `claude/gyo2-rollback-module-8bac02`）：公開登記 `rollback.Component<T>`、`rollback.Resource<R>`（沒登記＝不回滾）、可回滾的 schedule `rollback.Schedule(name)`、巢狀跑的 schedule 的登記、重跑時不跑 `rollback.SkipDuringResimulation(...)`；重複登記作者的為準、不論先後，登記帶「作者／代為」的來源（5.4）；「重跑中」Resource 放在時間 Module（時間宣告的要求；不收時退回由回滾 Module 登記，恆在）；回滾不依賴權威 |
| 回收清單 | `recovered-from-stale-cards.zh-Hant.md`（同 HANDOFF 的分支） |
| 舊規格（只取設計思想） | `docs/architecture/plans/kernel/spec.zh-Hant.md`：B.3、G 節、K.3 |
| 證據 | 主 checkout `build/evidence/gyo2-authority-module/other-engines-authority.md`（本份的偵察）；`build/evidence/gyo2-kernel-app-boundary/`（`section15-cross-engine.md`、`four-docs-adversarial-check*.md`、`single-writer-origin-and-pvp.md` 等） |

使用者 11-15 對舊架構的話照用：「不用帶規格，但是但是這個點的設計思想可以帶過來。（新老架構不同，不能照搬。）」

### 0.1 別家（讀本機原始碼；出處逐條寫在各節）

版本：bevy_replicon d80c849、lightyear 736e1695（實體複製建在 bevy_replicon 0.45.2 上，`crates/replication/replication/src/lib.rs:1-6`）、Unity Netcode for Entities 7.2.0（非官方鏡像 c4cabf4）、Unity Entities 1.5.0（4630ff5）、Bevy 6a911653f。GGPO、GekkoNet 本機沒有，沒讀。卡片抽查 5 處，4 處對得上、1 處更正路徑（`GhostPredictionSystemGroup.cs` 在 `Runtime/PredictionTicking/`）。

三家網路套件共同的事實：
1. 都在**握手或登記階段**擋兩端不一致：replicon 協議雜湊不同就斷線（`src/server.rs:279-292`）；Unity 元件集合雜湊不同就斷線（`Runtime/Rpc/RpcSystem.cs:266-273`）；lightyear 登記完成後再登記會 panic（`crates/core/utils/src/registry.rs:86-92`）。
2. 都**不在執行期擋 Client 寫入**，錯的值靠伺服器的值蓋回：Unity 每份新快照一律還原（`Runtime/Snapshot/GhostUpdateSystem.cs:25-50`）；replicon 只在伺服器那邊有變時才送（`src/lib.rs:228-243`）。
3. 執行中的 checksum 只有 lightyear 有，不一致**只記 log**（`crates/deterministic/deterministic_replication/src/checksum.rs:508-511`）。
4. tick 都是一般可寫的狀態，沒有「只有一個來源」的檢查（replicon `src/server/server_tick.rs:22-36`、lightyear `crates/core/core/src/timeline.rs:212-241`、Unity `Runtime/PredictionTicking/NetworkTime.cs:46-73`）。
5. 都沒有 World 內寫者數量、也沒有「判定只讀判定資料」這類規則；Unity 的「輸入只能在 `GhostInputSystemGroup` 寫」只是文件（`Runtime/Command/CommandSendSystem.cs:31-34`）。

---

## 1. 統合關聯表的引用

照統合第 1 節與第 15 節。統合部分列的舊句（L4、L23、L26、L29）以第 15 節為準。

| # | 統合的結論（摘） | 本份怎麼用 |
|---|---|---|
| L1 | 分層：產品 → 各 Module → 編排層 → Kernel；箭頭朝下 | 權威 Module 依賴編排層的登記單位型別與 Kernel；Kernel、編排層不認得權威 |
| L2 | 時鐘與 tick 移到時間 Module；「tick 只有一個來源、System 不得寫」與「(c) 區的時間只來自 tick」歸伺服器權威策略 | 5.4：前者用專用檢查；後者隨讀寫規則拿掉而**不再由啟動檢查守**（第 8.2 節回報） |
| L3 | Kernel 拿掉執行期的攔截；伺服器權威改靠 digest 比對與伺服器覆蓋抓錯（使用者的原話只到這裡） | 權威只有啟動時的檢查與執行中的計算，不攔寫入 |
| L4 | 初始值在建立當下給、不算寫者（P13 之後「寫者」不再數） | 不影響本份；生成不受任何規則管（5.2） |
| L5 | Kernel 兩步、可重複；編排層 build → 建 Plan → finish → 權威檢查；每個 World 宣告准不准動態插入；伺服器權威要求不准 | 4.5、5.5 |
| L6 | 唯讀、帶版本號的登記表；另交出每個 World 建立時的宣告、`SystemInfo` 的 effects、`ExclusiveWorld` 的 `Writes<…>` 標註 | 策略讀它做檢查與 C 指紋 |
| L7 | `ExclusiveWorld` 能寫任何值；`Writes<…>` 留作選填標註、Kernel 不解讀 | `Writes<…>` 只剩讓策略看得到獨占 System 寫了什麼（確認清單第 4 條）：5.4、5.5、5.11 |
| L10 | 指紋改版；C 指紋移權威 Module；跨端比較的指紋不用 Kernel 的編號 | 5.5：用權威登記順序 |
| L11 | 回滾是獨立 Module、有自己的策略；伺服器權威依賴回滾 Module；訊息不進回滾快照（15.1） | 5.11 |
| L12 | 伺服器權威檢查哪些、C 指紋與 G-5、digest 與覆蓋、(c) 獨占套用的新形狀、名字、interface | **本份的範圍** |
| L15 | 產品可以只選部分 Module | 單機不選權威 Module |
| L17 | 兩條核心＋每層的職責清單；替換條件＝清單要多一項或判不出屬於哪一層 | 4.1、第 7 節 |
| L18 | 狀態機 (b) 區已核准；`Zone` 隨 L12 | 5.8 |
| L20 | 依賴只寫在登記檔；不做策略層級的依賴，權威 Module 整個依賴時間 Module | 4.2（使用者核准，照舊） |
| L21 | 策略：interface 是契約；官方實作和作者寫的同一個插口；作者可從頭實作或繼承改裝；C++ 虛函式的抽象基底類別 | 4.4 |
| L22 | 權威與回滾各自登記；伺服器權威策略可以替作者登記進回滾 Module；權威只用回滾的公開登記 | 4.3 |
| L23 | 搬運伺服器的值、交換 C 指紋、送 digest、wire id 的呼叫者是 Engine 的網路 Module；本份只定規定並列需求 | 5.7、第 8.1 節 |
| L24 | Yes／No、Derived、D-E16 歸回滾；Z6、Z8、D-E15 是伺服器權威策略讀回滾公開登記的交叉檢查；兩個策略作者各自選，只檢查相容；可回滾的 schedule 由回滾登記。修訂 19 起的「豁免寫者的登記」隨 15.3 拿掉 | 5.11 |
| L25 | 代為登記用作者的 `Registrar`，owner 照舊是作者 | 4.3 |
| L26 | 狀態機與權威互不依賴；作者用名字標記（修訂 21） | 4.3、5.8 |
| L27 | 時間 Module 公開 tick 的型別、驅動者與對時，驅動者標 `Writes<Tick>`（時間宣告的要求） | 5.4 |
| L28 | 不用定（P13） | — |
| L29 | 策略選在 World（建 World 時，各 Module build 之前）；沒有回滾策略的 World 不回滾、不報錯；World 有權威登記卻沒選權威策略是啟動錯誤（統合 15.2） | 4.4、4.7 |
| L30 | 「誰實際寫的」記號另外排一批（15.1） | 和本份不衝突：記號不擋人；若做成，digest 不一致時可以拿它查是誰寫的 |

---

## 2. 前提表

| # | 前提 | 誰定 | 掛在上面的 |
|---|---|---|---|
| P1 | 權威是遊戲設計的一部分；作者選策略 | 使用者 11-16（`-authority-is-game-design`） | 全部 |
| P2 | Engine 附的第一版只實作伺服器權威；其他佔位、選了啟動時報錯 | 使用者 11-16（`-authority-strategy-mechanism`） | 4.6 |
| P3 | 權威 Module 自己登記、代為登記進 Kernel；Kernel 不存標籤 | 使用者 11-16（`-authority-own-registration`） | 4.3 |
| P4 | Kernel 不攔執行期的寫入；權威靠 digest 與伺服器覆蓋抓錯 | 使用者 11-17（L3） | 5.6、5.7；所有檢查都只看啟動時看得到的（有型別的參數、`Writes<…>` 標註） |
| P5 | Kernel 照登記順序平手、不偵測歧義；伺服器權威連線時比 C 指紋 | 使用者 11-16（`-no-ambiguity-detection`） | 5.5 |
| P9 | 每個 World 宣告准不准動態插入；伺服器權威要求不准 | 使用者 11-17（L5） | 5.5 |
| P10 | 回滾是獨立 Module；兩個策略作者各自選，權威只檢查相容 | 使用者 11-17（L11）；統合照 L17（L24） | 5.11 |
| P11 | 狀態機 (c) 區兩種形狀都支援 | 使用者 11-15～16 | 5.8 |
| P12 | 做 Engine 不把規則釘死：預設方便、限制只放在作者自選的地方 | 使用者 11-17（`-engine-not-rigid`） | 全部 |
| P13 | **伺服器權威策略不數寫者** | 使用者 11-17（`v8-arch-gyo2-authority-no-writer-count`） | 5.1 |
| P14 | **判定資料的讀寫規則兩半都拿掉** | 統合照使用者的原則判（15.3） | 5.2 |

（修訂 3 的 P6、P7、P8 已隨 P13 撤掉，不再列。）

---

## 3. 先用遊戲講

### 3.1 PvP 射擊的 Match（伺服器）

建 World 時選「伺服器權威」策略。作者寫：

```cpp
authority.Component<Health>({.name = "Health"});            // 判定用的資料：血量
authority.Component<Position>({.name = "Position"});
authority.Message<Damaged>({.name = "Damaged"});            // 判定用的訊息（保留式事件）
authority.Mark<Tick>();                                     // 時間 Module 已登記的 tick：只標記成判定資料
registrar.Component<HitFlash>({.name = "HitFlash"});        // 一般的資料（畫面用），不經權威
```

權威 Module 記下判定資料，替作者呼叫 Kernel 的 `Registrar`。Kernel 凍結、建 Plan；編排層 finish 之後策略檢查：World 是不是宣告了不准動態插入？除了時間 Module 公開的驅動者，還有誰寫 tick？`ApplyDamage` 與 `ApplyHeal` 都寫 `Health` 不檢查（P13），先後照登記順序；判定的 System 讀了 `HitFlash` 也不檢查（P14）。執行中策略不攔任何寫入（L3）。

### 3.2 PvP 射擊的 Client（預測）

Client 的模擬 World 也選伺服器權威，再選回滾策略（L29）。它比 Match 多登記一般資料（預測特效、除錯面板），所以連線時比的 C 指紋只看判定的部分、不用 Kernel 的編號（5.5）。伺服器的結果回來：判定資料以伺服器為準，網路 Module 把值寫進來，回滾 Module 還原、重跑、平滑。除錯面板用 untyped `Commands` 偷改血量，Kernel 不擋；伺服器的下一份值會蓋掉它；覆蓋碰不到的判定資料靠 digest 發現（5.6）。

### 3.3 格鬥遊戲（P2P 回滾；只佔位）

選「P2P 回滾」策略：第一版沒有實作，編排層 finish 時報啟動錯誤（P2）。作者也可以自己實作 `AuthorityStrategy` 接上（L21）。

### 3.4 單機 2D 橫版

不選權威 Module（L15），沒有任何權威的規則。

---

## 4. 權威 Module 的形狀

### 4.1 職責清單（L17）

**權威 Module 的職責是「讓作者選的一致性策略在啟動時與執行中有機制可用」**：

1. 作者的權威登記（判定用的元件、Resource、訊息與它們的屬性），代為登記進 Kernel（P3）；別的 Module 已登記的型別只標記。
2. 策略的插口（`AuthorityStrategy`，L21）；佔位策略的啟動錯誤。
3. 代為登記：在作者登記的當下，交給已選的策略代為呼叫 Kernel 與回滾 Module 的公開登記（4.4）。
4. 啟動時的檢查：讀 Kernel 的唯讀登記表（L6）、自己的登記、依賴 Module 的公開登記，回報啟動錯誤（給編排層彙整，O-5）。
5. 一致性的計算：C 指紋（連線時比）、digest（執行中比）、G-5 的兩份匯出。

**不在清單上**：回滾的快照、還原、重跑、平滑與回滾資料的登記（回滾 Module）；tick 的存放與推進（時間 Module）；傳輸、握手、wire id 的分配（網路 Module，L23）；執行期攔截（L3 拿掉）；排程（Kernel）；判定資料的讀寫規則（P14 拿掉）。

系譜（同類型多家，L17）：lightyear 把複製、預測、決定性同步分成三個 crate（`crates/replication/replication`、`crates/replication/prediction`、`crates/deterministic/deterministic_replication`）；Unity Netcode 的預測與複製分在不同的 System 群組；replicon 只做伺服器權威的複製（`src/lib.rs:2`）。權威的宣告都在網路套件自己的登記：replicon `app.replicate*`、lightyear 實體上的 `ControlledBy`（`crates/replication/replication/src/control.rs:17-60`）、Unity 型別上的 `[GhostComponent]`（`Runtime/Authoring/GhostComponentAttribute.cs:14-39`）。GYO 把「判定資料」和「要不要複製」分成權威與網路兩層，理由是權威可以不靠網路（單機測試、回歸 digest；統合 15.3）。

### 4.2 位置與依賴（L1、L20）

- 名字：`Authority`。目錄 `engine/authority`、命名空間 `GYO::Engine::Authority`、target `gyo_authority`（照 N11～N17；統合 15.3）。
- 位置：Engine 的 Module（我們自己維護、介面是我們的）。
- 依賴（只寫在 Module 登記檔，L20）：Kernel、編排層、**時間 Module**（tick 的型別、公開的驅動者與對時名單，5.4）、**回滾 Module**（公開登記，4.3、5.11）。L20「權威整個依賴時間 Module」是使用者核准的，照舊；它當初舉的理由「檢查 tick 只有一個來源」在本版仍成立（5.4 是專用檢查）。
- **不依賴狀態機 Module**：狀態機的值與套用 System 由作者用名字標記（L26 修訂 21）。
- 被誰依賴：產品；之後的網路 Module（第 8.1 節）。Kernel、編排層、時間、回滾、狀態機都不依賴權威。
- 選了 P2P 佔位策略的產品也會連時間與回滾 Module（L20「不做策略層級的依賴」的後果）。

### 4.3 登記

作者經權威 Module 的登記入口登記判定用的東西；權威 Module 記下，再用**作者的同一個 `Registrar`** 呼叫 Kernel（owner 照舊是作者，L25）。Kernel 收到的是一般的登記，看不出它是判定用的（P3）。

**經權威登記＝判定資料**，不另填屬性；「要不要複製」是網路 Module 自己的登記（統合 15.3）。

**登記入口**（暫名；名字在實作批照命名規範定）：

| 入口 | 用在 | 做什麼 |
|---|---|---|
| `authority.Component<T>`、`authority.Resource<R>`、`authority.Message<M>` | 作者自己的判定資料 | 記下屬性，代為登記進 Kernel；型別要提供雜湊函式（digest 用，5.6；登記時編譯期檢查） |
| `authority.Mark<T>()`、`authority.MarkResource("名字")` | 別的 Module 已登記的型別：時間 Module 的 tick（權威依賴時間，用型別）、狀態機的值（不依賴狀態機，用名字） | 只記下「它是判定資料」，不再呼叫 Kernel（重複登記是 Kernel 的 R-E2）；檢查時對不到唯讀登記表是啟動錯誤 |
| `authority.Ingress<&F>` | 外部輸入的入口 System | 代為 `AddSystem`；代為登記回滾的「重跑時不跑」（4.4、5.11） |
| `authority.Runs("System 名", {schedule…})` | 會巢狀跑別的 schedule 的獨占 System（例：狀態機的套用） | 記下它會跑哪些 schedule：進 C 指紋（5.5）、回滾交叉檢查用（5.11）；對不到是啟動錯誤 |
| `authority.OneEndOnly("System 名", 理由)` | 寫判定資料、但只在一端存在的 System（例：Client 的除錯面板若用有型別的參數寫判定資料、網路 Module 寫伺服器值的 System） | 不收進 C 指紋的 System 先後（5.5）；啟動報告列出所有這種標記 |

**屬性**：
- 外部輸入的暫存：`authority.Resource<InputRing>({.input = true})`——5.11 的 Z8 用。
- digest 排除：`{.digestExclude = true}`——5.6。
- 回滾的選項（只是轉交給回滾 Module 的登記，事實存在回滾 Module，L24）：`rollback = 可回滾（預設）／不回滾／Derived／不代登記`。

**代為登記進回滾 Module**（L22；統合 15.2 統一了假設）：
- 「重跑中」Resource 恆在（放在時間 Module；時間宣告不收時退回由回滾 Module 登記，回滾份修訂 4），回滾 Module 的公開登記建起來就在，所以代為登記**一律做**，不必先知道那個 World 有沒有回滾策略；沒有回滾策略時登記不生效（Match）。`OnRegister` 不讀「重跑中」的值，只透過 `rollback.SkipDuringResimulation` 掛上讀它的執行條件。
- 預設：經權威登記的判定元件與 Resource，由策略代為呼叫 `rollback.Component<T>`／`rollback.Resource<R>`（可回滾）；**外部輸入的暫存一律不代登記**（Z8）。作者用上面的回滾選項改成不回滾、Derived，或「不代登記、我自己登記」。
- 重複登記照回滾份 5.4 的規則：**作者的為準、不論先後**——作者先登記，策略就不代登記；策略先代為登記、作者後登記，也是作者的蓋過代為的。登記帶「作者／代為」的來源，代為不是特權（和 L21 同理）。權威 Module 不另外判。

**前例**：lightyear 的 `register_component` 同時登記進 ECS（`crates/replication/replication/src/registry/mod.rs:196-200`），預測串在同一個登記上（`app.component::<C>().predict()`，`crates/replication/prediction/src/registry.rs:741-748`）；replicon `app.replicate::<C>()`（`src/shared/replication/rules.rs:23`）；Unity 寫在型別的屬性上（11-16 比過，使用者選自己登記）。

### 4.4 策略的 interface（L21）

**照 L21**：`AuthorityStrategy` 是虛函式的抽象基底類別（GYO 的 `Plugin` 已是這個寫法，`engine/kernel/include/engine/kernel/Registry.hpp:60-75`）；官方的 `ServerAuthorityStrategy` 和作者寫的走同一個插口、沒有特權；作者可以從頭實作，也可以繼承官方實作改裝，官方實作的內部不保證不變。

| 時機 | 函式（暫名） | 輸入 | 輸出 | 在這次核准的契約內 |
|---|---|---|---|---|
| 登記當下（作者呼叫權威的登記入口時；一定在建 Plan 之前） | `OnRegister(entry, proxy)` | 這一筆權威登記；能代為呼叫 Kernel `Registrar` 與回滾公開登記的代理 | 代為登記：進 Kernel、進回滾（4.3）、重跑時不跑（`Ingress`） | 是 |
| 啟動時（編排層 finish 之後，L5） | `Check(view, errors)` | Kernel 的唯讀登記表（含每個 World 建立時的宣告）、權威的登記、時間與回滾的公開登記 | 啟動錯誤 | 是 |
| 啟動時、連線前 | `ConsistencyFingerprint(view)` | 同上 | C 指紋 | 是 |
| 執行中 | `Digest(world, …)` | World 的唯讀存取 | digest | **否：形狀待網路 Module 的宣告** |
| 執行中 | `OnMismatch(…)` | 比對結果 | 要做的事 | **否：形狀待網路 Module 的宣告** |
| 任何時候 | `Name()`、`Version()` | — | 進 C 指紋 | 是 |

- **策略在建 World 時選定**，在各 Module build 之前（統合 15.2）；所以作者在任何 Module 的 build 裡呼叫權威登記入口時，策略已經在了。
- **為什麼代為登記在「登記當下」**：「重跑時不跑」是 `Mount` 上的執行條件（`Registry.hpp:125-138`），只能在 `AddSystem` 當下給；伺服器權威的 World 不准動態插入，Plan 建好後補不上。比過「interface 加一個全部 build 完、建 Plan 前的階段」：要編排層多一個通用階段（動 O-2），沒採。
- 別的 Module 已經 `AddSystem` 的 System（例：網路 Module 的收包 System），權威掛不上「重跑時不跑」；那條路要那個 Module 自己登記（第 8.1 節）。
- 佔位策略實作同一個 interface，`Check` 一律回報「還沒實作」。
- 作者自寫的策略可以什麼都不檢查。

### 4.5 檢查的時機（L5）

編排層依登記檔 build → Kernel 建 Plan → 依同順序 finish → **權威檢查**（`Check` 跑一次）。Kernel 有啟動錯誤時不跑（O-5）。策略對 World 的要求（不准動態插入）由 `Check` 讀 World 建立時的宣告核對（L6）。

### 4.6 佔位策略

- Engine 附 `PeerRollbackStrategy`（暫名；P2P 回滾）的佔位：`Check` 一律回報啟動錯誤，訊息寫明「這個策略還沒實作，可以自己實作 `AuthorityStrategy` 接上」。
- 錯誤碼是權威 Module 自己的（不擴充 Kernel 的 `StartupCode`，O-5）；時機：編排層 finish 之後（使用者 11-16「啟動時報」）。

### 4.7 沒選策略

World 有權威登記（任何一個權威登記入口被呼叫過）卻沒選權威策略：啟動錯誤（統合 15.2）。沒有權威登記的 World（渲染 World）不用選。

（修訂 1～3 的 4.8「豁免寫者的登記」：P14 拿掉判定資料的讀寫規則之後，它沒有要豁免的東西，整節拿掉；回滾的重跑者名單、網路寫入者的豁免也跟著不需要。）

---

## 5. 伺服器權威策略到底檢查什麼

### 5.0 總表

| 舊規則（出處） | 結果 | 依據 |
|---|---|---|
| 每欄一個寫者 D-E2／S-11（B.3）與數寫者的細則（寫者單位、Despawn 不算寫者、開局不計） | 拿掉 | P13；確認清單第 5 條 |
| digest 欄禁 Shared（S-12）；`Shared` 標記 | 拿掉 | P13；確認清單第 3 條 |
| `ExclusiveWorld<Writes<…>>` 列出的算寫者（DI-KS 23） | 標註照 L7 留，不算寫者 | 確認清單第 4 條 |
| 每個 System 宣告區（S-6）；Z1（判定只讀判定資料）、Z2（判定資料只由判定的 System 寫）、Z7 | 拿掉 | P14 |
| 規則 2：S-7（`Local`）、S-8（Io effect）、S-9（`Changed`／`Added`） | 拿掉 | P14 的連帶（統合 15.3） |
| Z3（入口在 ingress stage）、S-17（入口不能是獨占 System）、入口的先後（修訂 2、3 的 6.15） | 拿掉 | P14；確認清單第 10 條 |
| Z5（(b) 不得 despawn 帶 (c) 欄的實體） | 拿掉 | L3 |
| D-E9（(c) 的 hook 衝突）、K.3 第 5 項 | 拿掉 | `no-ambiguity-detection`；確認清單第 8、9 條 |
| 漏斗 B1、B2（11-14） | 不擋 | 確認清單第 1 條 |
| 漏斗是單一寫者下的必經之路（11-14 `spawn-writer-funnel`） | 不是；`InitialValue` 留在 Kernel 當作者自選的寫法 | 確認清單第 2 條 |
| (c) 區的獨占套用（11-15～16） | 不加進 Kernel；狀態機以名字標記，獨占 System 用 `Runs` 宣告 | 確認清單第 6 條；5.8 |
| `RejectedSpawn::emitterZone` | 拿掉、不給替代 | 確認清單第 7 條 |
| Z6、漏斗 B3、Z8、D-E15（判定資料 × 回滾） | **留**：伺服器權威讀回滾公開登記的交叉檢查 | L24；確認清單第 1 條；5.11 |
| D-E16（Derived 的順序）、可回滾的 Yes／No | 回滾 Module | L24 |
| X-2、D-E11、X-E1（含 (c) 的 schedule 宣告 Parallel 要守衛測試） | 拿掉：「含 (c) 的 schedule」已無定義；Parallel 在 v1 是 Kernel 的 D-E17 | P14 的連帶（卡片照 L17 判） |
| tick 只有一個來源（DI-KS 8） | **留**：專用檢查 | L2；統合 15.3；5.4 |
| (c) 區的時間只來自 tick（K.3 第 11 項） | 不再由啟動檢查守；只剩來源掃描與 digest | P14 的連帶；5.4；第 8.2 節 |
| C 指紋、G-5 兩份匯出 | 留；編號改用權威登記順序 | 統合 15.3；5.5、5.9 |
| World 不准動態插入 | 留 | L5；5.5 |

### 5.1 寫者規則：拿掉（P13）

使用者 11-17 定伺服器權威策略不數寫者。比過的四個：數、預設單一寫者、`Shared` 才准多寫（11-11 的原方向）／數、預設多寫者／不數（三家網路套件與四家 ECS 的做法）← 選／帶檢查但預設關。

留下來要說清楚的（第三輪交叉檢查必改 4）：
- **蓋得到的**：同一張 schedule 裡、收進 C 指紋的 System 之間，誰先寫誰後寫是 Kernel 照登記順序平手的結果，已在精簡後的 Plan 步驟裡；兩端不同就連不上（5.5）。
- **蓋不到的**：兩個寫者在**不同的 schedule**（先後由 runner 或驅動固定步的 System 決定，不是登記順序）；標了 `OneEndOnly` 的寫者；untyped 的寫入與獨占 System 沒標註的寫入。這幾種造成的分岔，**靠 digest 與伺服器覆蓋**，C 指紋不保證。
- 「Boss 擊殺歸屬兩人都拿到」這類設計錯誤不由機制擋；作者要「只有一處寫」可以自己用漏斗或訊息。

### 5.2 判定資料的讀寫規則：兩半都拿掉（P14）

統合照使用者的原則判（15.3）：不檢查「判定的 System 只讀判定資料」，也不檢查「判定資料只由判定的 System 寫」。套用的原則：Engine 只提供機制；很多開源都這樣做就不頭鐵（七家都沒有）；不建立用不到、有漏洞的機制（兩半對 untyped 與閉包都看不到）；不逼作者標記；限制只放作者自選處（作者可自寫策略）。

連帶（卡片照 L17 判，統合 15.3 列的照做）：
- 「System 是不是判定的」不用問，作者不標判定的 System。
- 規則 2（不用 `Local`、不宣告 Io、不用 `Changed`）拿掉——它的對象是「判定的 System」。
- 生成帶的值不受任何規則管（生成世界、讀檔的一般 System 可以生成判定實體）；兩端初始狀態不同，連線後由 digest 看出來。
- `funnel-one-principle`（11-14：寫請求的人在區的檢查裡當作也寫了目標）沒有對象了，拿掉。
- 回滾等價的前提（可回滾的資料只被可回滾 schedule 上的 System 寫等）是作者的責任；伺服器權威只守 5.11 那幾條。
- 判定的 System 讀了只在一端有的資料（UI、設定）造成的分岔，只能靠 digest 與覆蓋。

### 5.3 外部輸入

外部輸入的暫存（`{.input = true}`）與入口（`authority.Ingress`）的身分留著，只給 5.11 的 Z8、D-E15 用。「只有入口寫輸入暫存」「入口排在讀者之前」「入口在 ingress stage」「入口不能是獨占的」都拿掉（P14）。入口的「重跑時不跑」由 `authority.Ingress` 代為登記進回滾（4.4）；不經 `authority.Ingress` 的入口（例：網路 Module 自己 `AddSystem` 的收包 System）由那個 Module 自己登記（第 8.1 節）。

### 5.4 tick 只有一個來源：專用檢查

**判定**（統合 15.3）：只准時間 Module 公開的驅動者與對時寫 tick。別家都沒有這個檢查（0.1 第 4 點）；保留是因為使用者 11-17 `clock-out-of-kernel` 已把這條歸伺服器權威策略。

**形狀**（卡片照 L17 判）：
- tick 用 `authority.Mark<Tick>()` 標成判定資料（進 digest 與 C 指紋）。
- 時間 Module 公開「驅動者與對時 System 的名單」與 tick 的型別，驅動者標 `ExclusiveWorld<Writes<Tick>>`（L27，時間宣告的要求）。
- `Check` 時：唯讀登記表裡，凡是啟動時看得到寫 tick 的 System（有型別的參數、`Writes<Tick>` 標註、hook 的存取），不在時間 Module 的名單裡就是啟動錯誤，訊息列出是誰。名單在 `Check` 時讀（時間 Module 在權威之前 build，L20 的順序），不受登記當下的時機限制。
- 看不到的：untyped 的寫入、獨占 System 沒標註的寫入（L3、L7）。回滾的重跑把 tick 退回（回滾份 7.2）若經 Kernel 原語或沒標註的獨占 System，看不到、也不擋；若回滾的重跑者用有型別的參數寫 tick，要列進時間 Module 的名單或改用原語（回滾份與時間宣告定）。
- 對時只在 Client 有：它不在 C 指紋裡（名單是時間 Module 的事，不進指紋）。

**(c) 區的時間只來自 tick**（K.3 第 11 項，L2 歸伺服器權威策略）：原本靠「判定的 System 只讀判定資料」（每幀時間、牆鐘的取樣不是判定資料）。P14 拿掉讀的一半之後，**這條不再由啟動檢查守**；剩下 G-5 的來源掃描（判定程式碼不 include `<chrono>` 等，5.9）與 digest。這是 P14 推出來的後果，回報統合（第 8.2 節）。

時間設計 DI 2「`RealTime`、`VirtualTime`、`FixedTime` 各只有一個寫者…既有的獨占寫入檢查就是守衛」（`rebuild-time-design.zh-Hant.md:544`）的守衛隨 P13 沒有了；列進時間 Module 宣告的要求（第 8.2 節）。

### 5.5 C 指紋與動態插入

**動態插入**：伺服器權威在 `Check` 時要求 World 宣告不准動態插入（L5）。所以啟動後判定的部分不變，C 指紋在啟動時算一次。別家一致：replicon、lightyear 登記完成後再登記會 panic；Unity 只有 ghost 型別能執行中加，靠「送清單與雜湊、不合就斷線」。

**編號**（統合 15.3）：用**權威登記的順序**（只數經權威登記的東西），wire id 由網路 Module 沿用。系譜：replicon 登記順序＋協議雜湊（`src/shared/replication/registry.rs:111-124`、`src/shared/protocol.rs:13-20`）、lightyear 登記順序（`crates/core/utils/src/registry.rs:36-46`）；Unity 依型別雜湊排序（`Runtime/Snapshot/GhostCollectionSystem.cs:51-65`），因為它跨組件、順序控制不了。**前提**：兩端的 Module build 順序由產品登記檔固定，登記順序進 C 指紋（不同就連不上）。現在的 C 指紋用 Kernel 的編號（`engine/kernel/src/Fingerprint.cpp:338-356`），Client 多登記一般資料就對不上，所以要改（L10）。

**收哪些 System**（卡片照 L17 判；統合 15.3 交本份）：P14 之後沒有「判定的 System」；C 指紋收**啟動時看得到寫了判定資料的 System**（有型別的寫、`Writes<…>` 標註、hook 的寫），**扣掉**：
- 標了 `authority.OneEndOnly` 的（4.3）；
- 回滾 Module 公開的還原者與重跑者（`Check` 時讀回滾的公開名單；回滾不依賴權威，所以是策略去讀，不是回滾來登記）；
- 時間 Module 公開的對時 System（只在 Client）。

漏標時的後果是兩端指紋不同、**連不上**——錯得早、錯得明白；G-5 的資料集報告列出收了哪些 System，對照兩端的報告就知道差在哪。比過「C 指紋不收任何 System 的先後」：不用標記，但使用者 11-16 定「伺服器權威連線時比 C 指紋」的目的（兩端的平手順序一樣）就沒了，所以沒採。

**C 指紋的內容**：

| 項目 | 進／不進 | 理由 | 現行（`Fingerprint.cpp`） |
|---|---|---|---|
| 策略的名字與版本 | 進 | 兩端策略不同就不該連 | 沒有 |
| 判定資料的型別清單（權威登記順序、名字） | 進 | 判定資料不同就不能比 | 含 zone C 的元件、Resource、事件（`:338-356`），用 Kernel 編號 |
| 收進來的 System（上面的定義）的存取 | 進 | 存取不同，結果可能不同 | 含 zone C 的 System 與 hook（`:357-368`） |
| 收進來的 System 在各 Plan 的相對順序 | 進 | P5：同一張 schedule 內的平手順序 | 含精簡後的步驟（`:382-405`） |
| 收進來的 System 之間的同步點 | 進，**只記「兩個收進來的步之間有沒有同步點」**，不記同步點的名字 | 每個 stage 結尾都有一個以 stage 命名的同步點（`Scheduler.cpp:216`、`Declarations.hpp:122-123`）；Client 為畫面多一個 stage，名字與數量會變，但收進來的步之間有沒有同步點才影響結果 | 現行保留全部非 System 步並寫名字（`:398-403`）——改 |
| 收進來的 System 的 run 條件 | 進（條件的名字） | 改變它跑不跑 | 含在 `MountBytes` |
| 收進來的 System 的 before／after | 不進 | 已反映在相對順序；可能指到只在一端有的 System | 含在 `MountBytes`——改 |
| 判定 hook 的順序（hook 寫判定資料的） | 進 | 照登記順序跑（D-E9 拿掉的前提） | 含（`:361-368`） |
| `InitialValue`（目標是判定資料的） | 進 | 同步點上 Kernel 替遊戲寫判定資料 | 含（`:370-378`） |
| hook 清空輪的上限 | 進 | 改變 hook 跑幾輪 | 含（`:393-394`） |
| `Runs` 宣告的獨占 System 會跑的 schedule | 進 | 巢狀的組成兩端要一樣（K0 宣告第 9 節的限界） | 沒有 |
| 判定訊息的上限 | 進 | 滿了丟最舊會改變結果（游標份：預設 1024、登記可改） | 沒有 |
| stage 名單、`writePolicy`、zone、rollback 的屬性、`rollbackEnabled` | 不進 | stage 見上；後幾項已拿掉或屬回滾；`rollbackEnabled`（`:390`）照搬的話 Match 不回滾、Client 回滾就永遠連不上 | 含——改 |
| 回滾的登記、`OneEndOnly`、時間的對時名單 | 不進 | 只在一端有 | 沒有 |
| 時間 Module 的步長等設定 | 延到網路 Module 的宣告（放指紋或放握手） | 它是 Resource 的值、不是登記 | 沒有 |

**守衛**：要證明兩個方向——判定的登記改了指紋就變；一般資料的登記改了指紋不變（lightyear 這版元件的協議雜湊沒餵資料、等於常數，`crates/replication/replication/src/registry/mod.rs:166`、`:211`，是反面例子；replicon 在 `protocol.rs:277` 釘死預期值）。

### 5.6 digest

**判定**（統合 15.3）：全部的判定元件與 Resource，作者可以排除（`digestExclude`）；**不含訊息與游標**；**合併用和順序無關的方式**（兩端實體的迭代順序不保證一致）；**每種型別要有雜湊函式**，不直接雜湊原始位元組（padding）。系譜：只有 lightyear 可比——逐元件登記雜湊函式（`crates/replication/replication/src/registry/deterministic.rs:47-75`）、XOR 合併（`checksum.rs:6-7`、`:312-354`）；GYO 把 Resource 也算進去是自己的擴充。

**何時比、不一致怎麼辦**：延到網路 Module 的宣告（L23）。第一版沒有網路，digest 只用在回歸測試（同一串輸入跑兩次或對 golden；舊 PvP 的 `AuthorityDigest` 的設計思想）。參考材料（給網路 Module 的宣告）：lightyear 伺服器每 tick 算、Client 每 tick 送一次、不一致只記 error（`checksum.rs:206-278`、`:450-493`、`:508-511`）；Unity、replicon 沒有狀態 checksum。

在網路 Module 之前，L3 拿掉的執行期攔截沒有東西在執行中接手（統合第 8 節記這段空窗；gyo2 上目前沒有產品）。

### 5.7 伺服器覆蓋

三段：**規定**（判定資料以伺服器為準）——權威 Module；**搬運**（把伺服器的值送來、寫進 World）——網路 Module（L23）；**重算**（還原、重跑、平滑）——回滾 Module（L11）。P13、P14 之後，網路寫入者與回滾的還原者不需要任何權威的豁免；只有寫判定資料、只在一端的 System 要排除在 C 指紋之外（5.5）。

覆蓋抓得到多少，取決於搬運是「每份快照無條件還原」（Unity，`GhostUpdateSystem.cs:25-50`）還是「伺服器有變才送」（replicon，`src/lib.rs:228-243`）——由網路 Module 的宣告定。

### 5.8 (c) 區的獨占套用與狀態機

**確認清單第 6 條（使用者核准）**：不加進 Kernel；狀態機的套用 System 由作者用名字標記給權威。形狀：
- 作者在選了伺服器權威的產品裡：`authority.MarkResource("狀態機的值的名字")` 把值標成判定資料；`authority.Runs("套用 System 的名字", {"OnEnter_Fog", …})` 宣告它會跑的 schedule（4.3）。
- 套用 System 寫的判定資料用 `ExclusiveWorld<Writes<…>>` 列出（L7），策略才看得到它寫了什麼（C 指紋的存取、5.11 的 Z6）。
- 「只跑列出的那幾張」執行期不核對（L3）；清單進 C 指紋，列了沒照做由 digest 抓。這也回答 K0 宣告第 9 節的限界（巢狀的組成不在 C 指紋裡）。
- 「權威端換、Client 採用」那一種不需要獨占套用，它是伺服器覆蓋。
- 狀態機卡的 `RegisterStateMachine<S, Zone::C>` 的 `Zone::C` 參數拿掉。

### 5.9 G-5 的兩份匯出

**判定**（統合 15.3）：留。權威 Module 用唯讀登記表的 `source`（`Registry.hpp:473-483`）與自己的登記算出：
- **判定資料集**：C 指紋的可讀版本（型別、收進來的 System、`Runs`、`OneEndOnly`），兩端對照找連不上的原因。
- **來源檔清單**：寫判定資料的 System（5.5 的定義）、`InitialValue` 的函式的來源檔，給產品的守衛掃描（舊 J-17：不 include `<chrono>`、`<random>`，不用全域可變狀態……）。P14 之後「判定的 System」沒有了，清單的範圍改成「寫判定資料的 System」；這是「函式體裡讀牆鐘」唯一的啟動前防線（5.4）。

現在的實作 `FrozenRegistry::ExportZoneCSources`（`engine/kernel/src/Fingerprint.cpp:458`）移到權威 Module。

### 5.10 `RejectedSpawn::emitterZone`

**確認清單第 7 條（使用者核准）**：拿掉、不給替代。「丟掉那一筆」照留（Kernel，DI-KS 9）。LIM-7 的「(c) 區的生成」措辭在減法批改（owner 是架構師會話）。

### 5.11 判定資料 × 回滾的交叉檢查（Z6、B3、Z8、D-E15；L24）

**例子**：Client 的固定步是可回滾的 schedule。`ApplyDamage` 寫 `Health`，作者把 `Health` 的回滾選項設成「不回滾」。回滾還原到第 1200 個 tick 再重跑——`Health` 沒還原，被重跑的 `ApplyDamage` 再扣一次。

讀的都是回滾 Module 的公開登記（可回滾的資料、Derived、可回滾的 schedule、巢狀跑的 schedule、重跑時不跑）與權威的登記：

| 規則 | 檢查 | 舊出處 |
|---|---|---|
| 可回滾 schedule 上寫的判定資料要能回滾 | 掛在可回滾 schedule 上、啟動時看得到寫了判定資料的 System 或 hook（扣掉回滾的還原者與重跑者），寫的每個判定資料都是可回滾或 Derived；hook 寫的要是可回滾。P14 之後不再分「判定的 System」，改用「寫了判定資料的」 | Z6；11-14 漏斗 B3（確認清單第 1 條：照擋） |
| 輸入不回滾 | 標成外部輸入的暫存不能登記成可回滾 | Z8 |
| 入口在重跑時不跑 | 掛在可回滾 schedule 上、經 `authority.Ingress` 登記的入口都在「重跑時不跑」裡 | D-E15 |
| 巢狀跑的 schedule 也要登記 | 掛在可回滾 schedule 上、經 `authority.Runs` 宣告的獨占 System，宣告會跑的 schedule 都在回滾的巢狀 schedule 登記裡 | 第三輪交叉檢查必改 6；回滾份 5.4 第 0 點 |

- 官方策略的代為登記（4.3、4.4）會讓前兩列與第三列對官方實作多半是自我核對；作者改了回滾選項、不經 `authority.Ingress` 登記入口、或自寫策略時才會抓到東西。
- 沒有回滾策略的 World（Match）：回滾的公開登記仍在但不生效，這幾條照查也不會錯（可回滾的 schedule 是空的）。
- 訊息：不進回滾快照（統合 15.1），第一列對訊息跳過（舊 Z6 也跳過事件，`Registry.cpp:1382`）。
- Derived 的順序（D-E16）是回滾 Module 自己的檢查。
- 錯誤碼是權威 Module 的（O-5 彙整）。

---

## 6. 決定的紀錄（修訂 4：題目都定了，本份不再問）

| 本份的題 | 結果 | 誰定 | 出處 |
|---|---|---|---|
| 6.1 名字 | `Authority`（`engine/authority`） | 統合照 L17 判 | 統合 15.3 |
| 6.2、6.13 策略選在哪一層、沒選策略 | 選在 World（建 World 時、各 Module build 之前）；有權威登記卻沒選是啟動錯誤 | 統合照 L17 判 | 統合 15.2 |
| 6.3 寫者規則 | 不數 | **使用者** | `v8-arch-gyo2-authority-no-writer-count` |
| 6.4 System 的區怎麼來 | 不用問（P14） | 統合照 L17 判 | 統合 15.3 |
| 6.5 經權威登記的就是判定資料嗎 | 是，不另填屬性 | 統合照 L17 判 | 統合 15.3 |
| 6.6 digest 禁 Shared、6.7 插入算建立還是修改 | 不問（P13） | 統合 | 統合修訂 22 |
| 6.8 C 指紋與 wire id 的編號 | 權威登記順序；前提：build 順序由產品登記檔固定、登記順序進 C 指紋 | 統合照 L17 判 | 統合 15.3 |
| 6.9 digest 比什麼 | 全部判定元件與 Resource、作者可排除、不含訊息與游標、和順序無關的合併、每種型別要有雜湊函式 | 統合照 L17 判 | 統合 15.3 |
| 6.9 何時比、不一致怎麼辦 | 延到網路 Module 的宣告 | 統合 | L23 |
| 6.10 伺服器覆蓋的歸屬 | 不是題目（L23、L11） | 統合 | — |
| 6.11 tick 只有一個來源 | 專用檢查（只准時間 Module 公開的驅動者與對時寫） | 統合照 L17 判 | 統合 15.3 |
| 6.12 G-5 匯出 | 留 | 統合照 L17 判 | 統合 15.3 |
| 6.14 確認清單 | **已核准**（統合 15.6 的 10 條） | **使用者**（「核准」） | `v8-arch-gyo2-confirm-derived-removals` |
| 6.15 入口的先後 | 拿掉（P14） | 統合照 L17 判 | 統合 15.3 |
| 6.16 判定資料的讀寫規則 | 兩半都拿掉 | 統合照使用者的原則判 | 統合 15.3 |

本版內由卡片照 L17 判、統合沒有逐條列的（都在權威的職責清單內）：`OneEndOnly` 標記與「C 指紋收寫了判定資料的 System」（5.5）；同步點只記有無、不記名字，before／after 不進（5.5）；X-2 拿掉（5.0）；`Runs` 宣告與巢狀 schedule 的交叉檢查（5.8、5.11）；tick 檢查的形狀（5.4）；代為登記一律做、沒有回滾策略時不生效（4.3，和回滾份統一）。

---

## 7. Design Invariants 與 Replacement Criteria（草案；核准後在實作批寫進權威 Module 的頁）

| # | Design Invariant |
|---|---|
| DI-A 1 | **權威 Module 的職責是讓作者選的一致性策略有機制可用**（4.1 的清單）。不攔執行期的寫入、不做回滾、不存 tick、不傳輸、不檢查判定資料的讀寫 |
| DI-A 2 | Kernel 不認得權威：權威的宣告只存在權威 Module；Kernel 收到的是一般的登記。守衛：Kernel 目錄的掃描（統合附錄的式子） |
| DI-A 3 | 策略是 interface＋實作（L21）：官方實作與作者的走同一個插口；4.4 表裡標「是」的函式與時機是契約，`Digest`、`OnMismatch` 的形狀不在這次的契約內 |
| DI-A 4 | 檢查只在啟動時（L5 的權威檢查那一步）；執行中只計算，不拒絕寫入（L3） |
| DI-A 5 | C 指紋只看兩端應該相同的判定部分，編號用權威登記順序、和一般資料的登記無關；守衛證明兩個方向（5.5） |
| DI-A 6 | 佔位策略選了就是啟動錯誤，訊息寫明還沒實作 |
| DI-A 7 | 伺服器權威下判定資料以伺服器為準 |
| DI-A 8 | 伺服器權威策略不數寫者、不檢查判定資料的讀寫（P13、P14）；同一張 schedule 內收進 C 指紋的 System 之間的先後由 C 指紋蓋到，跨 schedule、只在一端、untyped 的寫入只靠 digest 與覆蓋（5.1） |
| DI-A 9 | 權威不讀回滾、時間的內部，只讀公開登記；不依賴狀態機；回滾、時間、狀態機都不依賴權威 |
| DI-A 10 | 別的 Module 已登記的型別與 System 只標記、不重複登記；標記對不到唯讀登記表是啟動錯誤 |
| DI-A 11 | 代為登記（進 Kernel、進回滾、重跑時不跑）都在作者呼叫權威登記入口的當下做（`OnRegister`），一定在建 Plan 之前；策略在建 World 時選定 |

**Replacement Criteria**（L17：不數次數，出現就停下來問）：

1. 權威 Module 的職責清單（4.1）要多加一項，或判不出某樣東西屬權威、回滾、時間、網路哪一個。
2. 要讓伺服器權威成立，必須在 Kernel 加任何權威專用的欄位、分支或觀察點。
3. 伺服器權威策略的檢查，在作者照正常業務寫（有型別的參數）時誤報，而且改寫法要繞過機制。
4. 第二個官方策略（P2P 回滾）實作時，interface（4.4 的契約部分）要改。
5. `OneEndOnly` 或 `Runs` 這類標記，開始需要每加一個新機制（Module）就多一種，或作者要為了過檢查而標記一般的遊戲邏輯——表示又長回「要作者標記」的方向（P14 的反面）。

---

## 8. 回報統合的事

### 8.0 修訂 0 的 N1～N8（統合已判）

| # | 修訂 0 回報的 | 統合的處理 | 本版 |
|---|---|---|---|
| N1 | 網路／複製這一層不在關聯表 | L23：網路 Module | 5.6、5.7、8.1 |
| N2 | 回滾的宣告與檢查歸誰 | L24 | 4.3、5.11 |
| N3 | C 指紋不能用 Kernel 的編號 | L10 補句；編號＝權威登記順序（15.3） | 5.5 |
| N4 | World 的宣告讀不到 | L6 補 | 4.5 |
| N5 | L4 翻掉 11-14 的 `spawn-writer-funnel` | L28：不用定（P13）；漏斗不是必經之路（確認清單第 2 條） | 5.0 |
| N6 | 代為登記時的 owner | L25 | 4.3 |
| N7 | 狀態機 (c) 區的接法 | L26：作者用名字標記 | 5.8 |
| N8 | 時間 Module 公開 tick | L27 | 5.4 |

### 8.1 網路 Module 要權威 Module 提供的（L23 的需求清單）

| 需求 | 權威 Module 提供 | 本份 |
|---|---|---|
| 連線時交換並比對 | `ConsistencyFingerprint(view)`：位元組與版本，含策略的名字與版本 | 4.4、5.5 |
| 執行中比對 | `Digest(world, …)`（形狀由網路 Module 的宣告定） | 5.6 |
| 不一致時 | `OnMismatch(…)`（同上） | 5.6 |
| 哪些資料以伺服器為準、wire id | 判定資料的清單，用權威登記順序的編號 | 5.5、5.7 |
| 寫伺服器值的 System | 若用有型別的參數寫判定資料，要標 `authority.OneEndOnly`（只在 Client），否則兩端 C 指紋不同 | 4.3、5.5 |
| 收包、寫進輸入暫存的 System | 要「重跑時不跑」：網路 Module 自己 `AddSystem` 時，自己呼叫回滾的 `rollback.SkipDuringResimulation`（權威掛不上已登記的 System，4.4）；或作者改用 `authority.Ingress` 登記 | 5.3、5.11 |
| 執行中插入 | 伺服器權威要求不准；作者自寫的策略若允許，要能重新交換指紋（Unity 的 ghost 型別清單＋雜湊是前例，`GhostReceiveSystem.cs:470-486`） | 5.5 |
| 時間 Module 的步長 | 放進指紋或握手，由網路 Module 的宣告定 | 5.5 |

網路 Module 自己要定、會影響權威的：蓋回是「每次無條件」還是「有變才送」（5.7）。

### 8.2 修訂 4 新回報的

1. **「(c) 區的時間只來自 tick」不再由啟動檢查守**（5.4）：L2 把它歸伺服器權威策略（`clock-out-of-kernel`），它原本靠「判定的 System 只讀判定資料」；15.3 拿掉讀的一半之後，只剩 G-5 的來源掃描與 digest。這是 P14 推出來的，改寫了使用者 11-11 K.3 第 11 項與 11-17 L2 的一句；**要不要列進確認清單請統合判**。
2. **時間設計 DI 2 的守衛沒了**：「`RealTime`、`VirtualTime`、`FixedTime` 各只有一個寫者…既有的獨占寫入檢查就是守衛」（`rebuild-time-design.zh-Hant.md:544`）隨 P13 失去守衛，列進時間 Module 宣告的要求。
3. **`OneEndOnly` 標記**（5.5、4.3）：為了 C 指紋能收「寫判定資料的 System」又不收只在一端的。網路 Module 的需求多一條（8.1）。
4. **讀檔卡**：統合 L28 寫過「讀檔放回的值不算寫者，已通知讀檔卡」；現在要改通知成「不數寫者、也不檢查誰寫判定資料」（P13、P14）。

---

## 9. Architecture Delta（AGENTS §3；只寫和統合第 4 節的差分）

1. **Feature**：統合 5.4「伺服器權威」（PvP Match／Client）、「作者自寫策略、P2P 回滾」（只佔位）；L5 的指令集（伺服器權威要求不准插入）。
2. **現在的問題**：寫者、區、digest、C 指紋、G-5 都在 Kernel（`Registry.cpp:1440-1490`、`:1240-1300`；`Fingerprint.cpp:330-406`、`:458`）；Kernel 對單機也執行；C 指紋用 Kernel 的編號。
3. **變化的邊界**：新 Module `Authority`；公開介面：登記入口（4.3）、`AuthorityStrategy`、`ServerAuthorityStrategy`、佔位策略、C 指紋與 digest 的計算、G-5 的匯出。
4. **影響的 Module／Product／Tool**：權威 Module（新）；Kernel（減法批拿掉的部分，統合已列）；時間 Module（公開 tick 型別、驅動者與對時名單、驅動者標 `Writes<Tick>`，L27）；回滾 Module（公開登記，L24）；狀態機（`Zone::C` 拿掉，作者以名字標記）；網路 Module（之後，8.1）；編排層（佔位策略與「沒選策略」的錯誤彙整；建 World 時帶策略，15.2）。測試：
   - **退役**（P13、P14；各案在實作批逐案列，§14.4）：J-15（第二個寫者；含 `NestedRunTests.cpp:1373` 的 DI-KS 23 案與 `:2066` 的開局豁免案）、J-16（digest 禁 Shared；`ZoneRuleTests.cpp:180`、`:219`）、`InitialValueTests.cpp:177`、`CoverageTests.cpp:300` 的 D-E2 案、`SkeletonTests` 的寫者案；J-14、J-38～J-41、J-47 的 Z1、`InitialValueTests` 的 Z1／Z2／Z3 與漏斗 B1、B2 的案、J-27 的守衛 id（X-2）、J-34 的 D-E9；它們的突變一起退役並寫理由。
   - **搬到權威**：Z6、B3、Z8、D-E15 的案 → 5.11 的交叉檢查；C 指紋的案（新編號、新內容，golden 重產）；`zone_c_export_check.py`、`zone_c_fixture/` → G-5 的匯出與掃描（範圍改成寫判定資料的 System）；新增 tick 專用檢查、`Runs`、`OneEndOnly`、C 指紋兩方向的守衛。
   - **搬到回滾**：D-E16 的案（L24）。
5. **Dependency Direction**：新增 權威→Kernel、權威→編排層、權威→時間、權威→回滾；網路→權威（之後）；產品→權威。沒有反向。
6. **Ownership**：區、digest、C 指紋、G-5：Kernel → 權威 Module；寫者數量、判定資料的讀寫：Kernel → 不由機制守（P13、P14）；tick 的單一來源：Kernel → 伺服器權威策略（L2）；Z6、Z8、D-E15：Kernel → 伺服器權威策略的交叉檢查（L24）。owner 是架構（架構師會話）。
7. **為什麼不能更小**：權威的規則留在 Kernel 已被使用者否決（`-kernel-is-ecs-no-restrictions`）；規則全交產品，伺服器權威的檢查會在每個 PvP 產品重寫（使用者 11-16「權威檢查我想放Module，這個是底層能力」）。

### 9.1 基盤的根據（AGENTS §2）

**為什麼算基盤**：伺服器權威的產品、網路 Module、回滾 Module、狀態機的 (c) 區都會積在它的 interface 與 C 指紋上；C 指紋是兩端握手的內容，一改就不相容。

| 根據 | 要求 |
|---|---|
| PvP 伺服器權威（使用者 11-11、11-16） | 啟動時的檢查（5.4、5.5、5.11）；C 指紋；伺服器覆蓋的規定 |
| Client 三個 World、只有模擬 World 判定（11-11） | 策略選在 World；C 指紋不受一般資料影響 |
| 格鬥遊戲 P2P（11-16，佔位） | interface 不照伺服器權威的形狀寫死（RC 4） |
| 作者自寫策略（11-16）、策略的構造（L21） | interface＋官方實作 |
| 指令集（11-17） | 不准插入的 World 由策略要求 |
| 參考：lightyear、replicon、Unity Netcode | 權威的宣告在網路套件自己的登記；連線時比協議雜湊；執行中不擋寫入、靠蓋回 |

**實作薄的部分**：第一版只實作登記、代為登記、啟動時檢查、C 指紋與 digest 的計算、G-5 匯出、佔位策略；比對的傳輸、覆蓋的搬運在網路 Module。

### 9.2 預算

不碰執行效率。digest 的計算量在網路 Module 定了比的頻率之後量。

---

## 10. 批次（草案）

照統合第 8 節的五段，權威 Module 在第四條線（權威＋回滾＋拿掉 Kernel 的權威與回滾規則同一條，守衛沒有空窗）。依賴：編排層骨架、時間 Module（L20；L27 的公開名單）、本宣告與回滾份核准。批次的先後由主會話排（統合 15.2）。

| 步 | 內容 | 誰的宣告 | 模型／ギア（navigator 上段：公開介面、Design Invariants） |
|---|---|---|---|
| 1 | Kernel 給回滾原語 | 統合 L11、回滾份 | 回滾份定 |
| 2 | 回滾 Module 的公開登記 | 回滾份 | 回滾份定 |
| 3 | **權威 Module**：登記入口（4.3）、代為登記（`OnRegister`）、`AuthorityStrategy`、佔位策略、啟動錯誤；tick 專用檢查（5.4）；C 指紋（新編號、新內容）、digest 計算、G-5 匯出 | 本份 | 實作 Opus／high；審查 Fable／high |
| 4 | **交叉檢查**（5.11） | 本份 | 實作 Opus／high；審查 Fable／high |
| 5 | 拿掉 Kernel 的權威與回滾規則；守衛逐案判定（§14.4：退役、搬權威、搬回滾）；golden 重產（第三次） | 統合第 8 節 | Opus／high；Fable／high；§14.4 檢證者 Fable／high |

估計（本份的步 3、4）：6～10 h（登記、代為登記、策略、tick 檢查、交叉檢查）；4～6 h（C 指紋與 digest、G-5、兩方向的守衛）。數字比修訂 3 少：讀寫規則、寫者規則與豁免都拿掉了；退役的案在步 5 統一處理。

---

## 11. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；時間估計是讀程式量估的。
- 回滾的資料登記、事件游標、tick 的存放不在這份。
- 網路 Module 的設計不在這一輪；5.6、5.7 只定權威那一段，8.1 是給它的需求。
- 回滾 Module 的公開登記（可回滾的 schedule、巢狀 schedule、重跑時不跑、重跑者與還原者名單）的名字由回滾份定；5.5、5.11 只寫要讀什麼。
- 別家的做法來自子 agent 讀本機原始碼（0.1），卡片抽查了一部分；Unity 讀的是非官方鏡像。
- 舊規格只取設計思想；舊的具體做法（`InputRing`、`HostClockSample`、`Restore`）不帶進來。
- 8.2 第 1 條（「(c) 區的時間只來自 tick」不再由啟動檢查守）等統合判要不要列進確認清單。
- 修訂 0～2 送過三輪四份交叉檢查；修訂 3、4 沒有單獨送，收口時四份再一起送一次（統合卡負責）。

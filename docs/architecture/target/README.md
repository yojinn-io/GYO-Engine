# docs/architecture/target：目標架構的文件

**改任何部件之前，先讀 [kernel.zh-Hant.md](kernel.zh-Hant.md)。** Kernel 的 Design Invariants 全文只在那一頁；其他部件頁保留條款標題、內容連到那一頁的單一條款，自己只寫該部件特有的規則（使用者 2026-11-13）。完整清單見本頁〈目標 Design Invariants 清單〉。

資料夾與規則由圖書管理員定（2026-11-11），依使用者的要求「新版架構文件與舊版分開、至少分資料夾、不混放」。

## 分界

- `docs/architecture/*.md`（頂層檔案）＝**現況**：已實作、正在跑的架構與它們的 Design Invariants。
- `docs/architecture/target/`＝**目標架構**：把 Bevy 的抽象能力（World／System Contract／Scheduler／Executor）帶入 GYO 之後各部件的文件、Design Invariants 與 Replacement Criteria、使用者已定的設計決定。
- `docs/architecture/plans/`＝**過程紀錄**：計畫、HANDOFF、dev_log 的索引、spike 報告、規格草案。

放在 `docs/architecture/` 之下而不是同層另一個資料夾，是因為 AGENTS.md §3 規定 Engine 的 Design Invariants 放在 `docs/architecture/` 配下；目標架構的 DI 也要落在 agent「改 Component 前先讀該 owner 的 DI」會看到的範圍內。

## 命名

- 平鋪，不分子資料夾：檔名＝搬回後在 `docs/architecture/` 的檔名。例：`target/world.zh-Hant.md` → `docs/architecture/world.zh-Hant.md`。工作線（kernel）不是文件的最終位置，所以不出現在路徑裡；搬回只是 `git mv`。
- 搬回時若取代現況的同名文件，取代後刪除舊檔（歷史留在 git），不並存。
- 搬回的時機：逐部件，在該部件的遷移批完成時；遷移前不整體改寫現況文件，只在有 target 對應的現況文件檔頭加一行「現況文件；目標見 target/…；遷移批完成時取代」（架構師 2026-11-12；遷移批待 `v8-arch-migration-plan` 排定）。framework 三份已標 deprecated 的文件保留到 `framework/` 程式被 plugins 遷移批刪除時，同一個 PR 一起刪。
- **設計權威與 legacy（`v8-arch-docs-legacy`，使用者 2026-11-12，取代 `v8-arch-docs-next-folder` 的並存做法）**：所有架構設計決定只寫在 target；下列舊檔降級為 legacy（現況紀錄），檔頭一行以 `> **Legacy**` 開頭，凍結設計內容，只更新兩類事實：仍存在的程式的現況描述、仍有測試守衛的 Design Invariants（例如 entity-store DI 8／9、OS DI、determinism DI 1～3）。另外，被遷移批取代的條文要標「已被取代（批次代號）」（見下方「目標 DI 與現況 DI 衝突時」）。原地標示、不搬資料夾（圖書管理員 2026-11-12，使用者同意：約 300 個引用含程式註解、守衛測試與不改寫的 dev_log；部件遷移完成時本來就會刪檔）。
  - legacy 清單：`docs/architecture.md`、`architecture/entity-store.md`、`architecture/determinism.md`、`architecture/error-handling.md`、`docs/framework/` 三份（architecture、event-sounds、preview）、`object_fps_pvp/entity-model.zh-Hant.md`、`object_fps_pvp/network-architecture.zh-Hant.md`。操作類文件（建置、發佈、creating_apps、tool_projects、ui_toolchain）沒有 target 對應，不標 legacy。
  - 遷移期間，現有程式受其 legacy 文件的 Design Invariants 約束；新程式（`engine/kernel` 起）受 target 約束。判斷依據是程式是舊是新，不是文件日期。一個部件遷移完成時，legacy 檔刪、target 檔搬回 `docs/architecture/`，檔名照本節的對照。framework 三份隨 `framework/` 程式被 plugins 遷移批刪除時一起刪。

## 狀態標記

標題下第一行，固定英文、只有兩種（和 `Design Invariants` 見出し同理，方便 grep、不因翻譯走樣）：

```text
> Status: target, not implemented
> Status: implemented in <batch-id>, pending move to docs/architecture/
```

搬回 `docs/architecture/` 時刪掉這一行。

## 目標 DI 與現況 DI 衝突時（架構師決定，2026-11-11）

- 遷移批內的新程式受 target 的 Design Invariants 約束；尚未遷移的舊程式受其 legacy 文件的 Design Invariants 約束。一批同時碰到兩者（例：新 System 要讀舊元件）時，以 target 為準，並在那批的事前宣告列出被取代的 legacy 條文、在 legacy 檔把它標為「已被取代（批次代號）」（措辭統一：架構師 2026-11-12，配合 `v8-arch-docs-legacy`）。這是 AGENTS §3「改 Component 前先讀該 owner 的 Design Invariants」在兩套文件並存期間的解釋：一個 Component 在任一時刻只受一套 DI 約束，由它是否已遷移決定。
- 遷移批次的事前宣告要寫明它遷移哪些 Component、從此它們受哪份 target 文件約束；遷移完成後文件搬回，衝突自然消失。
- 目標 DI 本身的變更仍是 Architecture Delta（§3），要使用者同意。

## 每批事前宣告的檢查項（使用者 2026-11-11 的原則，自 -audit 後適用）

**不要用舊思路處理新架構。** 每批事前宣告列選項時，每個選項先問一句：**「這是不是在為舊形狀找位置？」** 是的就不採，即使它最省工。來源：audit 待決第 2 項（Editor 預覽的三個選項都在問「readback 放哪」，等於沿用 ImGuiShell 的形狀，全部不採）、第 1 項（framework 改 DI 還是退役）、第 4 項（AssetManager 選哪個 World 還是拆開）。使用者原話：新架構若沿用舊架構的思路與原則，架構變更就失去意義——思想不變，修改就是在破船上打補丁。

## 限制與修改條件（limits 頁）的規則（圖書管理員 2026-11-11 定；兩條解釋由架構師決定）

- 位置：現在 `target/limits.zh-Hant.md`，最終 `docs/architecture/limits.zh-Hant.md`，檔名不變；**逐條搬**，每條 LIM 自帶狀態行，所涉部件遷移完成時整段搬到頂層，未實作的留 target/。編號全域唯一、永不重用、搬動不改號；解除的標 `Lifted in <batch-id>` 不刪。引用一律用編號。
- **(a) 與 AGENTS §3「Replacement Criteria 寫在部件 DI 節內」的關係（架構師決定）**：詳細內容只在 limits 頁一處（§9 同一事實只在一處維護）；部件文件 DI 節的 Replacement Criteria 對跨部件的條目只列一行「LIM-n：一句摘要」連結到 limits 頁；只屬該部件的置換條件仍寫在部件文件。
- **(b) 與 AGENTS §14「範囲の限界要寫進受影響批次的事前宣告」的關係（架構師決定）**：事前宣告寫「LIM-n＋一句摘要」，不能只貼編號；只寫在決定紀錄或 limits 頁裡不算寫進宣告。
- checkup（圖書管理員）：LIM 編號唯一不重用；條目狀態與所在資料夾一致；部件 RC 引用的 LIM 都存在。

## spec K.3 決定對照表（2026-11-11 全部 17 項已定）

| # | 決定（一句） | 寫在哪個部件文件 |
|---|---|---|
| A1 | （audit §3.1）framework 退役；組合與第三方對接由 `plugins/`（新 top-level）承擔；Editor host 骨架＝runner plugin；三層改為 Engine／plugins／產品與工具 | runtime-composition §4、DI 2；spec C.2、R-E6、L.2 |
| A2 | （audit §3.2）Editor 預覽資料只往前流：文件 World 出場景與 UI draw list，渲染 World 合成；H-6 原樣；ImGui plugin 拆兩半 | runtime-composition §3b |
| A3 | （audit §3.6 第 4）`AssetManager` 不選 World，照 Bevy 資產模型拆開；載入是外部服務、依請求順序套用；內容身分 (c)；命名規範待定 | assets §1～2、DI 1～3 |
| A4 | （audit §3.6 第 5）初始成員不走 Startup；新增 Loading schedule（地圖→場景→可互動物件→敵人→玩家，可分批）；對局 tick 於 ready 後起算；Client ready 前 loading | runtime-composition §3a、DI 3；spec H.2b |
| A5 | （同上附帶）Peer 與 Player 分開；Link 是 Resource 不是實體 | runtime-composition §3a；networking §3b |
| A6 | （audit §4.5）決定性區表三漏一錯採 (a)，排第一個文件批；條目照歸屬命名 | naming §適用；determinism.md 第一個文件批 |
| A7 | 命名規範五條：歸屬／詞性／World 前綴／區＝位置／禁止 Manager、Renderer、ViewModel、Host、Shell、Runtime、Controller | naming、DI 1～3 |
| 1 | Entity 鍵格式 u64（低 32 index、高 32 generation）是 Kernel 的；配發策略是產品 Resource，PvP 不重用、generation 0 | world §1、DI 1～2 |
| 2 | change stamp 依 Plan 位置取值，tick 視角以 tickStamps 換算；(c) 禁變更過濾 | world §2、DI 3 |
| 3 | 預設獨占、Shared 可選；進 digest 禁 Shared；資料路徑表，事件漏斗可選 | world §3、DI 4；limits LIM-4 |
| 4 | Despawn 非寫者；同步點套用順序寫死 | world §4；kernel DI-KS 9 |
| 5 | (b) 讀 (c) 要顯式順序；慣例「呈現放模擬之後」記為限制 | scheduler §2；limits LIM-1 |
| 6 | Client 的 Frame 與 Render World 分開 | runtime-composition §2、DI 1 |
| 7 | Architecture Delta 三處同意（`docs/architecture.md`〈Runtime boundaries〉節末的禁令段、entity-store DI 4／DeferredChanges、determinism.md §4.7） | 文件修改排第一個文件批（plans/kernel/HANDOFF「下一步」） |
| 8 | 移除 object_fps、object_fps_v2、object_fps_preview | 不是部件文件；批 `v8-arch-remove-object-fps`（plans/kernel/README）。**已移除**（2026-11-12） |
| 9 | 獨占寫者以 System 計、Startup 不計；Body 拆欄 | world §3 |
| 10 | Join／Leave 進 tick 內 | runtime-composition §3；limits LIM-5 |
| 11 | (c) 的時間只來自 domain tick 與被記錄的 boundary 輸入 | determinism §11、DI 2（`v8-arch-a6-revision` 起節號：原 §2） |
| 12 | F.2 套用順序（與第 4 項一起） | world §4 |
| 13 | RC 第 1 條改寫為三分項（核准） | system-contract RC |
| 14 | v1 任何 Parallel 宣告是啟動錯誤 | limits LIM-2；scheduler RC |
| 15 | 事件只在該 domain 的 clock schedule 內讀寫 | world §5、DI 6 |
| 16 | spike 判定 2 照原文成立；上限記為範囲の限界 | limits LIM-3 |
| 17 | System 回 Result<void, KernelError>；Executor 只處理一次；不靜默 | system-contract §2、DI 2；error-handling |

## 目標 Design Invariants 清單（兩層；`v8-arch-a6-revision`，供使用者一次核准——遷移計畫 §7 U13）

全文條數 **34**＝kernel 頁 15＋各部件頁特有 19（其中 3 條在產品的 target 檔 `docs/object_fps_pvp/target/entity-model.zh-Hant.md`；只算 `docs/architecture/target/` 是 31）。部件頁裡「受 kernel DI-KS n 約束」的條保留標題、內容連到 kernel 頁的那一條：整條都是連結的 15 條不計入；determinism 的 DI 2、DI 3 各拆成兩半（Kernel 的半條是連結，產品與 Host 的半條是本頁特有），算在第二層。「對 legacy」欄：**新**＝legacy 沒有這一條；**相容**＝與仍生效的 legacy 條文同向；**相容（措辭更新）**＝同一性質、措辭或機制換了；**取代**＝該部件遷移完成後 legacy 的那一條不再成立。逐條重標的依據在 `plans/kernel/a6-revision-report.zh-Hant.md`。

### 第一層：跨部件，先讀——[kernel.zh-Hant.md](kernel.zh-Hant.md)

**改任何部件之前先讀這一層。**

| 條 | 一句 | 對 legacy |
|---|---|---|
| [DI-KS 1](kernel.zh-Hant.md#di-ks-1) | System 的存取集合從簽名推出、只有一份；System 無狀態 | 新 |
| [DI-KS 2](kernel.zh-Hant.md#di-ks-2) | System 回 `Result<void, KernelError>`；繞路由存取驗證攔；Err 由 Executor 記一次、不靜默 | 新；legacy error-handling「no error chains」一句對 Kernel 收窄（`error-handling.md`） |
| [DI-KS 3](kernel.zh-Hant.md#di-ks-3) | 順序只來自 id 與 Plan | 相容：entity-store DI 2、8；entity-model E-5、E-6 |
| [DI-KS 4](kernel.zh-Hant.md#di-ks-4) | id 由登記順序或配發器決定；World 不配發；鍵格式含 generation、策略歸產品 | 相容：entity-model E-1、E-2。相容（措辭更新）：entity-store DI 1、RC 2；entity-model E-3。取代 entity-store RC 1 |
| [DI-KS 5](kernel.zh-Hant.md#di-ks-5) | 衝突與順序分開；(c) 的歧義是啟動錯誤 | 新 |
| [DI-KS 6](kernel.zh-Hant.md#di-ks-6) | 獨占寫入是啟動檢查；進 digest 的欄必須獨占 | 新（AGENTS §3 狀態權威在 Kernel 的落點） |
| [DI-KS 7](kernel.zh-Hant.md#di-ks-7) | (c) 預設 Serial；Parallel 要逐位元守衛 | 新 |
| [DI-KS 8](kernel.zh-Hant.md#di-ks-8) | 區規則 Z1～Z8 是啟動檢查 | 相容：legacy determinism DI 1、DI 4 |
| [DI-KS 9](kernel.zh-Hant.md#di-ks-9) | 結構變更延後，在同步點依固定順序套用 | 相容（措辭更新）：entity-store DI 4；entity-model E-12 |
| [DI-KS 10](kernel.zh-Hant.md#di-ks-10) | 事件無游標、不跨 clock domain | 新 |
| [DI-KS 11](kernel.zh-Hant.md#di-ks-11) | hook 只有 OnAdd 與 OnRemove | 新 |
| [DI-KS 12](kernel.zh-Hant.md#di-ks-12) | 回滾所需的狀態全在 World，固定步可重入 | 新 |
| [DI-KS 13](kernel.zh-Hant.md#di-ks-13) | 渲染只經 Extract 唯讀來源 World，無反饋 | 新（legacy preview DI 4 的目標條文在 runtime-composition §3b，不是這一條） |
| [DI-KS 14](kernel.zh-Hant.md#di-ks-14) | 一個 World 一條執行緒；外部系統只在交換點交換 | 新；與 threads DI 4 相容 |
| [DI-KS 15](kernel.zh-Hant.md#di-ks-15) | Kernel 中性；Host 保留多時鐘、啟停、交換點，不排 System 順序 | 取代 legacy `docs/architecture.md` 的「Engine 無 system／scheduler／world」禁令（已核准，K.3 第 7 項）與 `RuntimeLoop` 三段 |

### 第二層：各部件特有的條文

| 部件頁 | 特有的條 | 一句 | 對 legacy |
|---|---|---|---|
| world | DI 3 | change stamp 依 Plan 順序推進、單調、不進 (c) | 新；取代 entity-store RC 6 |
| error-handling | DI 2 | `Base::Result` 是純值；只有 Executor 記錄 Kernel 的錯誤 | 新；與 legacy 規則 1～7 相容 |
| determinism | DI 2（產品與 Host 的半條） | (c) 的原始檔不讀時鐘；時鐘取樣與到達時刻是被記錄的輸入 | 相容：legacy determinism DI 4 |
| determinism | DI 3（產品的半條） | (c) 的亂數是每實體、可回滾、進 digest 的狀態 | 相容：entity-model E-10 |
| runtime-composition | DI 2 | 一個 Runtime 一個 runner；Engine 不含入 plugin；plugin 的選擇是資料 | 新；framework 的 DI 隨 framework 刪除，其中 DI 6 的性質由這一條承接，DI 1、DI 3 的性質由 plugins 層的 DI（`docs/plugins/`，plugins-foundation 建）承接 |
| runtime-composition | DI 3 | 世界內容只經 Loading 進來；Player 與 Peer 是不同實體 | 新；entity-model §5.2 已加修訂註 |
| naming | DI 1～3 | namespace 與目錄對應；詞性；禁止舊角色後綴 | 新 |
| naming | DI 4 | 名字是概念的 tag；Engine 的名字只指 Engine 的概念（分層由 runtime-composition §4 引用） | 相容並加嚴：承接 `docs/architecture.md`〈Layers〉check 1；「名字乾淨不等於概念乾淨」一句是新增（使用者 2026-11-13） |
| assets | DI 1～3 | 沒有資產總管物件；載入是外部服務、依請求序套用；內容身分是 (c) | 取代 `docs/architecture.md` 的 `AssetManager` 一句 |
| zone-c-sources | DI 1～3（草案） | 清單只說哪些檔；版本進格式；不含產品名分支 | 新 |
| 產品 `docs/object_fps_pvp/target/entity-model.zh-Hant.md` | E-3、E-7、E-12 | 配發策略；沒有孤兒元件；Tick 內不改成員 | 相容（措辭更新）：entity-model 同號條 |

## 來源與引用

- 概念基準：`docs/architecture/plans/kernel/mechanism.zh-Hant.md`；規格草案：`docs/architecture/plans/kernel/spec.zh-Hant.md`。規格是草案，決定以使用者在 K.3 的裁決為準；裁決後寫進對應的部件文件。
- checkup：圖書管理員在 `docs/checkup/documentation/README.md` 加項目核對 Status 行、implemented 久留未搬、同名並存、現況檔混入目標內容。

## 目錄

- [kernel.zh-Hant.md](kernel.zh-Hant.md)：**先讀**。Kernel（`engine/kernel`）的 Design Invariants（DI-KS 1～15）與 Replacement Criteria（RC 1～13）全文；每條有錨點，部件頁連到單一條款。
- [system-contract.zh-Hant.md](system-contract.zh-Hant.md)：System 契約（描述從簽名推出、回 `Result<void, KernelError>`、無狀態）。
- [error-handling.md](error-handling.md)：錯誤處理的擴充列（以執行期資料偵測的 Programmer Error：Result 帶上下文、Executor 記錄一次並 Interrupted、不靜默）。英文，因為最終併入的 `docs/architecture/error-handling.md` 是英文。
- [determinism.md](determinism.md)：決定性（區是宣告的契約、(c) 的時間只來自 domain tick 與被記錄的 boundary 輸入、亂數是每實體元件、四處契約）。英文，因為最終併入的 `docs/architecture/determinism.md` 是英文。
- [assets.zh-Hant.md](assets.zh-Hant.md)：資產（不選 World、照 Bevy 拆成請求狀態／`Assets<T>`／GPU 資源；載入是外部服務；內容身分 (c)；命名待辦）。
- [naming.zh-Hant.md](naming.zh-Hant.md)：命名規範五條（歸屬＝namespace 與目錄、分類＝詞性、World＝前綴、區＝位置、禁止舊角色後綴）。
- [limits.zh-Hant.md](limits.zh-Hant.md)：**限制與修改條件**（使用者 2026-11-11 要求的集中頁；LIM-1～LIM-6；最終位置 `docs/architecture/limits.zh-Hant.md`，逐條搬）。
- [runtime-composition.zh-Hant.md](runtime-composition.zh-Hant.md)：Runtime 的組合（Client 的 Frame 與 Render World 分開、Extract 唯讀無反饋、一 World 一執行緒、Match 的 Join／Leave 在 tick 內）。
- [scheduler.zh-Hant.md](scheduler.zh-Hant.md)：Scheduler（衝突邊與順序邊、歧義處置、(b) 讀 (c) 的顯式順序）。
- [world.zh-Hant.md](world.zh-Hant.md)：World（鍵格式與配發策略、change stamp、寫入政策預設獨占與資料路徑、結構變更的套用順序與 Despawn、事件只在 clock schedule 內；其餘待規格核准）。

規則（使用者 2026-11-11）：spec K.3 的**每一項**決定都要寫進對應的部件文件（Design Invariants 或資料路徑），不只改 spec 的 K.3 與對應節。

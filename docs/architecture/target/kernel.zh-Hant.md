# Kernel

> Status: target, not implemented

更新：2026-11-13（`v8-arch-a6-revision`）。Owner：架構（架構師會話）。部件：`engine/kernel`（World、System 契約、Registry、Scheduler、Executor）。

**改任何用到 Kernel 的部件之前，先讀這一頁。** Kernel 的 `Design Invariants` 與 `Replacement Criteria` 全文只在這裡（使用者 2026-11-13 決定：條款的出處要有一個明確的位置，文件太散反而沒人讀）。其他部件頁（world、system-contract、scheduler、determinism、runtime-composition、error-handling）裡與本頁同一性質的條，保留條款標題，內容改成指向本頁那一條的連結（每條有自己的錨點 `#di-ks-n`、`#rc-n`）；部件頁自己特有的規則留在部件頁，不進本頁。

與規格的關係：`docs/architecture/plans/kernel/spec.zh-Hant.md` 是設計說明（為什麼這樣定、怎麼推出來、每個代號 W-n／S-n／D-n／Z-n／J-n 的定義）；本頁是約束。條文取自 spec K.1（DI-KS 1～15）與 K.2（第 1～13 條），**條號沿用、不重用**；方括號〔併自 …〕標的句子是原本寫在部件頁同一性質的條（或該條所依的部件頁本文）裡、spec K.1 沒有逐字寫到的部分，併進來後部件頁的條不再重複。自本批起 spec K.1、K.2 是凍結的來源紀錄，兩邊不一致時以本頁為準。

各條末尾的括號是它在規格裡的依據；「守衛」的 J-n 定義在 spec J.2，突變對象在 J.3。

## Design Invariants

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。本節的變更是 Architecture Delta（AGENTS §3），要使用者同意。

<a id="di-ks-1"></a>
**DI-KS 1.** **描述從簽名推出，一份。** System 的資料存取集合等於其參數型別的 Contribution 聯集（含 bundle 的延後寫、`SetRead` 在 finish 展開的讀；`ExclusiveWorld` 的 Contribution 是「讀全部、寫 zone=B 且 Shared 的全部」）；登記 API 不接受手寫的第二份存取宣告；System 是無狀態的可呼叫物，參數只能是 `ParamTraits` 特化過的型別，句柄不可複製、不可移動。守衛：J-10～J-12。（DI-K2 的推導半；S-1～S-4、S-13）
〔併自 system-contract DI 1、DI 3〕可呼叫物是非成員函式或無捕獲 lambda：無捕獲、無 `this`；有捕獲的 lambda、成員函式、泛型 lambda、多載的 `operator()` 編譯失敗。

<a id="di-ks-2"></a>
**DI-KS 2.** **System 回傳 `Result<void, KernelError>`（回傳契約，不是錯誤通道；Ok 是正常路徑）；經 World 的繞路由存取驗證（可關）攔，違規與 untyped 路徑都以 Err 回傳、Executor 記錄一次並 Interrupted，任何組態不靜默。** System 自己的執行緒上經 World API 存取未宣告的 `(id, mode)`，驗證在存取當下記為違規；未宣告寫入的欄在 System 邊界有淨變化，驗證記為違規；違規由跳板回 Err；驗證關閉（release 預設）時不查、System 回 Ok，寫明且列 LIM-3。untyped `Commands`、`Despawn`、`ExclusiveWorld` 對 zone=C 或 Exclusive 資料的寫入，在所有組態回 `Err(KernelError)`（帶上下文），System 包一層回傳，Executor 記錄一次並標 Interrupted；記錯當下不斷言，只有會返回的 `breakOnKernelError` 掛鉤（一個事件只有一種性質）。守衛：J-5、J-6、J-32b、J-45。（W-5、W-6、F-10、Z5、S-13；修訂：B4／S6／S8）
〔併自 system-contract DI 2、error-handling DI 1〕`Result<void, KernelError>` 是回傳契約，Ok 是正常路徑；Err 帶完整上下文，逐層包一層（上下文鏈）；存取驗證記下的違規與 System 自己的 Err 是並列清單、不互包；release 目標的測試也要看到 Err 與 Interrupted。補守衛：J-10（回傳型別）、J-35。

<a id="di-ks-3"></a>
**DI-KS 3.** **順序只來自 id 與 Plan。** 每個欄依 EntityId（u64 的數值，Kernel 不解讀 index／generation 兩段）嚴格升冪，查詢依升冪或交集的升冪產出；World、Scheduler、Executor 不用雜湊順序、指標比較、執行緒完成順序決定任何順序。守衛：J-1、J-2、J-9。（DI-K3 的一部分；W-1、W-2、W-10）
〔併自 world DI 1、scheduler DI 3〕World 對 `EntityId` 只做 64 位元整數比較；任何疊代、交集、同步點套用的順序都以它為鍵。每個 schedule 的 Plan 唯一、只在 finish 建一次，只取決於登記序與宣告，不依雜湊、位址、執行緒、平台；Running 期間不重建。補守衛：J-20。

<a id="di-ks-4"></a>
**DI-KS 4.** **id 由登記順序或配發器決定，凍結後不變。** ComponentId、ResourceId、EventTypeId、registrationSeq＝登記順序；TypeKey 只用於行程內查表；執行期 id 不上 wire、不進 digest；World 不配發實體 id；配發器是產品的 Resource，鍵格式固定為低 32 index、高 32 generation，配發策略（PvP：單調不重用、generation 恆 0）由產品宣告；stale 句柄的偵測能力取決於策略，Kernel 只保證鍵還活著。守衛：J-8、J-18、J-19；掃描。（DI-K3；R-1～R-3、W-9；使用者 2026-11-11 K.3 第 1 項）
〔併自 world DI 2〕World 內沒有配發、回收、歸零的操作；世代切換＝重建 World。J-8 含突變「用盡時回繞」「加 Reset 歸零」。

<a id="di-ks-5"></a>
**DI-KS 5.** **衝突與順序分開；(c) 的歧義是啟動錯誤。** 衝突邊只表示「不能同時」，順序邊表示先後。有衝突且無順序的對：任一方 zone=C→啟動錯誤；兩方 zone=B→登記序平手。Kernel 不提供歧義豁免。(c) 區對登記順序不敏感。守衛：J-20～J-24、J-44。（DI-K1 的修正版；D-1～D-10；主張 A）
〔併自 scheduler DI 1、DI 2〕衝突邊與順序邊分開存，Plan 的建立只讀這兩種邊，衝突邊不隱含先後。J-44＝(c) System 登記順序的排列測試：要嘛全部 D-E8，要嘛結果不變。

<a id="di-ks-6"></a>
**DI-KS 6.** **獨占寫入是啟動檢查。** 元件與 Resource 預設 Exclusive；Exclusive 的欄在一個 World 的所有非 Startup schedule 中寫者（以 System 計）至多一個，寫者含 `Write`、型別化 Commands、`Spawn`／`SpawnWithId` 的 bundle、hook；沒有函式體內決定型別的寫入路徑能繞過計數（untyped 路徑寫不到 Exclusive 欄，記錄當下以 Err 回傳）；進 digest 的欄必須 Exclusive。守衛：J-15、J-16、J-32b。（DI-K2 的單一 writer，範圍從「同階段」改為整個 World；AGENTS §3；修訂：B3／S1）
〔併自 world DI 4〕第二個寫者是啟動錯誤；執行期不加鎖、不仲裁，Exclusive 與 Shared 的執行期行為相同。

<a id="di-ks-7"></a>
**DI-KS 7.** **(c) 預設 Serial；Parallel 要逐位元守衛。** 含 zone=C System 的 schedule 宣告 Parallel 時，必須指名一個存在、帶 `cpu` 標籤、比對 Serial 與 Parallel 的 (c) digest 逐位元相同的測試，否則啟動錯誤。守衛：J-27。（DI-K1「不做自動平行」的修正版；主張 B）

<a id="di-ks-8"></a>
**DI-KS 8.** **區規則是啟動檢查。** 每個 System、元件、Resource、事件宣告 zone，沒有預設；Z1～Z8。zone=C System 的全部存取（讀、寫、With／Without、事件、延後寫、run 條件）都落在 zone=C 資料；(c) 只從 domain tick 與被記錄的 boundary 輸入取時間、從 World 取亂數、從 `InputRing`／`MembershipRequests`／`AssetStaging`（role=Input 暫存）取輸入；不用變更過濾；不讀寫 external Resource；role=Input 的資料不回滾。守衛：J-14、J-38～J-41；J-17。（DI-K5、DI-K6；G 節；修訂：B2／B6／B7）
〔併自 determinism DI 1、DI 2 的 Kernel 半條〕區沒有預設值；違反是啟動錯誤，不是執行期檢查。boundary Resource 只由 Host（World 在 Ready 時）或 ingress System（在 ingress stage）寫，其他 System 宣告寫它是啟動錯誤；domain tick 只由該 domain 的 clock schedule 的 `Run` 第一步推進，任何 System 宣告寫它是啟動錯誤、Host 也不直接寫它（J-39；DI-KS 12）。Kernel 驗不到函式體有沒有自己讀時鐘：那一半是產品與 Host 的規則，寫在 `determinism.md` DI 2、DI 3。

<a id="di-ks-9"></a>
**DI-KS 9.** **結構變更延後、在宣告的同步點依「hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource」套用，每階段依 id，同鍵依 (planPos, recSeq)，鍵唯一。** Running 期間成員不變；hook 的變更屬下一窗；結果不依 System 之間的記錄順序。守衛：J-3、J-31～J-34。（K4；W-3、F-1～F-5；延伸 entity-store DI 4；修訂：B1／S4）
〔併自 world DI 5〕despawn 冪等（同一 id 記兩次只處理一次），對窗開始時不存在的 id 無作用；Despawn 不計入獨占寫者、不產生衝突邊，(c) 資料的保護由 Z5。補守衛：J-32b（突變：spawn 先於 despawn、不去重、依記錄順序套用）。

<a id="di-ks-10"></a>
**DI-KS 10.** **事件無游標、本 tick 寫者之後就可讀、保留到下 tick 末、不跨 clock domain。** 守衛：J-36、J-37。（F-7～F-9）
〔併自 world DI 6〕每個事件型別屬於一個 clock domain，寫者與讀者都在該 domain 的 clock schedule 內；讀取依 Plan 位置。

<a id="di-ks-11"></a>
**DI-KS 11.** **hook 只有 OnAdd 與 OnRemove，同步執行、依登記順序，寫入計入獨占，(c) 的 hook 衝突是啟動錯誤。** 守衛：J-34。（K6；F-4、F-11）

<a id="di-ks-12"></a>
**DI-KS 12.** **回滾所需的狀態全在 World，固定步可重入。** (c) System 不得有 Local；domain tick 只有一個來源且在快照內；快照含 `rollback = Yes` 的欄與 Resource、配發器、Fixed domain 事件緩衝，不含 role=Input 的歷史；Rewind 或 Restore 加 Replay 與一次跑完逐位元相同（stamps 除外）；可回滾 schedule 的 ingress 在 Replay 中不執行；Rewind／Restore／Replay 只由 Host 在 Ready 呼叫；Restore 不觸發 hook。守衛：J-42、J-42b、J-42c、J-46。（K8、DI-K5；H-5、H-8～H-11；修訂：B5／B7／S2／S12／S15）
〔併自 system-contract DI 3〕補守衛：J-14（(c) System 帶 `Local` 在 finish 失敗）。

<a id="di-ks-13"></a>
**DI-KS 13.** **渲染只經 Extract 以 const 方式讀來源 World，無反饋。** 守衛：J-43。（DI-K7；H-6）
〔併自 runtime-composition DI 1〕渲染 World 有自己的 Registry；來源 World 的 Registry 必須先凍結；`Extract<Write<…>>` 不存在（編譯失敗測試）。補守衛：J-16。

<a id="di-ks-14"></a>
**DI-KS 14.** **一個 World 一條執行緒、每個 domain 一個 clock schedule、每個 domain 的事件只在它的 clock schedule 內。** 同一 World 的所有 schedule 綁同一執行緒（Parallel 下一個 System 的存取只來自派它的 worker、只在那次呼叫內）；不同執行緒上的時鐘是不同的 World，經 Host 的交換點交換；外部系統不持有、不修改 World，只經 Link 類 Resource 在交換點、World Ready 時交換，Running 期間不碰 System 側緩衝；World 的生命期是一個世代，世代切換＝重建 World。守衛：J-15、J-6、J-37；finish 檢查 H-1、H-2；審查。（W-6、W-9、H-1、H-2、I-1～I-4、F-9；修訂：S4／S11／S14／N3）
〔併自 runtime-composition DI 4〕finish 檢查 H-2：不同執行緒上的時鐘必須是不同的 World。

<a id="di-ks-15"></a>
**DI-KS 15.** **Kernel 中性，Host 保留多時鐘、啟停、IO 交換點、runner 的指定；Host 不排 System 順序。** Kernel 的程式碼與 Registry 不含產品、工具、能力的名字，不內建 schedule 或 stage 名，不擁有執行緒，不讀時鐘或亂數；已遷移的 host 只呼叫 `Run`、`Rewind`／`Replay` 與交換點。守衛：J-9；H-7 的掃描；審查。（DI-K4；R-7、X-8、H-7）

### Replacement Criteria

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。符合任一條時，停止在 Kernel 上修補，向使用者提出「修還是重建」。跨部件的限制與解除條件在 `limits.zh-Hant.md`（LIM-1～LIM-6），本節不重複。

<a id="rc-1"></a>
**RC 1.** **契約不可靠**（修訂：S8。原稿的條件「判定 (2) 在任一編譯器不成立」已被 spike 的「判定 2 部分不成立」觸發，但觸發的部分是 mechanism §3.5 事前寫明的上限（不經 World 的讀取），不是推導或核對本身失效；而且原稿的處置「退回顯式宣告」對那種形狀同樣無效。條件改寫成只指向本機制真正依賴的性質）：(a) spike 判定 (1)（簽名推存取集合）在任一 L1 編譯器不成立；或 (b) 「System 自己的執行緒上經 World API 的存取」在驗證開著時攔不到（A.6 第一層失效；2026-11-12 改名，原文「在 debug 攔不到」）；或 (c) 上線後「宣告與實際存取不符、A.6 兩層核對與 J-17 掃描都攔不到、且不屬於 spike〈判定 2 的上限〉已列的形狀」的案例在一個版本內出現兩次以上。→ (a)：退回「顯式宣告＋存取驗證」（bevy_core:29、:64）；(b)：核對層重寫；(c)：評估頁保護（spike 報告末段）或縮小「包」的範圍。中間層照 mechanism §1 全部重寫。（blueprint RC 3）**這條的改寫已由使用者 2026-11-11 核准**（K.3 第 13 項）。
〔併自 system-contract RC〕目前狀態（2026-11-11）：(a) 三平台成立（L1 run 43018979148）；(b) 成立（spike (c1)–(c5)）；(c) 尚無案例。

<a id="rc-2"></a>
**RC 2.** ~~**id 要重用**~~ **取消（使用者 2026-11-11）**：鍵格式已含 generation，重用是配發策略的選擇，不是 Kernel 的置換條件。保留編號以免後文錯位。

<a id="rc-3"></a>
**RC 3.** **歧義錯誤成為負擔**：同一版本內，為消除 D-E8 而加的 before／after 有一半以上與資料流無關（只是為了讓 Plan 建立）。→ (c) 的粒度或區劃錯了。

<a id="rc-4"></a>
**RC 4.** **第二種排序機制**：為表達需求，stage 內必須引入依賴求解以外的排序（優先權數字、依資料值決定順序、執行期改 Plan）。（blueprint RC 2）

<a id="rc-5"></a>
**RC 5.** **過濾誤報**：D-8 造成的誤報使某個 (c) schedule 中的 `after` 宣告超過 System 數的一半，或需要並行的 schedule 因誤報無法並行。→ 提案加入過濾式的不相交判斷。

<a id="rc-6"></a>
**RC 6.** **欄結構不夠**：entity-model RC-1 或 RC-3 觸發（4,000 實體、1 ms p95、動態組合過半）。→ 換儲存，DI-KS 3 的順序不變，不在上面加 archetype。

<a id="rc-7"></a>
**RC 7.** **Host 又長回來**：第一個真實能力（場景線的拉桿開門、VFX）接進機制時仍要改 Host 迴圈或 Kernel 本體；或 H.7 以外的協調邏輯在兩個以上 host 中重複。（blueprint RC 1）

<a id="rc-8"></a>
**RC 8.** **遷移成本**：遷移一個 host 的實測成本超過重寫它，或遷移到一半必須讓兩種動詞長期並存（違反 H-7）。（blueprint RC 4）

<a id="rc-9"></a>
**RC 9.** **同步點模型錯了**：(c) 的某條規則必須在同一窗內讀到自己剛生成的實體，而宣告同步點無法滿足。

<a id="rc-10"></a>
**RC 10.** **獨占模型與需求不符**：同一 World 中 (c) 的 Exclusive 欄為繞過 DI-KS 6 被改成 Shared 兩次以上。→ 重新劃分元件。

<a id="rc-11"></a>
**RC 11.** **回滾等價破裂**：J-42 在 L1 平台因 Kernel 以外的原因（Math、編譯器）失敗，且 E0 的結論是 determinism.md RC 3。→ (c) 縮為 (b)，DI-KS 7、12 的「逐位元」改寫。

<a id="rc-12"></a>
**RC 12.** **並行不是策略問題**：為讓 Parallel 通過 X-2 的守衛必須改 (c) System 的語義或放寬 golden。→ 停下。

<a id="rc-13"></a>
**RC 13.** **每加一個 System 或 plugin 都要重新驗證 Kernel 本體的守衛**（突變名單以外）。→ Kernel 不是穩定的基盤。

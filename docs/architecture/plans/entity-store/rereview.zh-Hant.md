# entity-store 重審：審查報告（`v8-arch-entity-store-rereview`）

- **狀態**：給使用者決定的審查意見（2026-11-14，審核卡 07，架構師開卡）。本報告不改 engine、spec、target、DI 條文與 PR #218；結論分三類（§7），凡要決定的事單列在文末（D1～D10），標「建議」的只是審查意見，不代替決定。
- **給使用者的三行摘要**：(1) Feature 支持的是「(c) 查詢順序只取決於兩端共有的契約鍵」，不是「欄的容器順序」；後者與「以 EntityId 為鍵」都是從 PvP 的 `std::map` 一路傳下來的形狀（§5.3）。(2) Delta-3（entity_store 當 World 的欄）主要理由是「它已經存在」，同意出處也標錯（§5）；#176 要求的 EnTT 重判沒做。(3) 有兩個在 AI 批之前要定的缺口：Client 預測生成拿不到 id（C4）、Restore 快照沒有鍵驗證（C5）。
- **敵對檢查**：本稿的前一版（審查者 Opus／high）經 Fable／high 敵對檢查一次（只讀），發現必改 4、應改 8、建議 6，全部處置，處置紀錄在文末〈敵對檢查處置〉。
- **範圍**：`docs/architecture/entity-store.md` 的 DI 1～10；kernel 對它的沿用（spec §A、§F、§H、K.3 第 1、7 項；遷移計畫 §4 Delta-3；target/world、target/kernel DI-KS 3、4、9、12）；PR #176 §1.1；PR #218（`origin/claude/arch-kernel-world-exec`，head `ea62c6e1`，OPEN）的實際用法；兩條 Bevy 研究線。
- **證據位置**：本機（Intel Mac）主 checkout `build/evidence/entity-store-rereview/`：`recon-a-history.md`、`recon-b-bevy.md`、`recon-c-research-lines.md`（三份偵察）、`review-draft-v1-before-adversarial.md`（敵對檢查前的審查稿）、`adversarial-fable.md`（敵對檢查全文）、`transcripts/`（五個子 agent 的原始紀錄）。PR #218 的 diff 只在審查會話的 scratchpad 暫存，未保存（出處＝該分支，可重取）。repo 行號以 master `c8c7dfb8` 為準；Bevy＝`~/Code/GitHome/bevy` `6a911653f`（路徑相對 `crates/bevy_ecs/src/`）；lightyear `736e1695`；bevy_replicon `d80c8497`。
- **方法**：讀三份偵察 → 把當結論依據的關鍵事實回到原始文件或原始碼親自讀一次。標記：**已回讀**＝本審查者親自讀過該行；**偵察**＝只依偵察、未回讀；**未查證**＝無法確認；**推論**＝由程式結構或已知數字推得、未量測。用來支持「0 件」的掃描都做了正例（見各處；§4 那一支的正例在敵對檢查後以 Python 重跑更正）。編制：偵察 ×3 Opus／high、審查者 Opus／high、敵對檢查者 Fable／high（與審查者不同模型）、卡片主會話（Opus）處置與定稿。
- **立場的自我檢查**：既有設計是同一群會話寫的，審查者容易覺得「還能救」。每條「仍成立」都附最強反方論點與回應（§7.1）。

---

## 1. entity-store DI 1～10：不守的話誰會壞、理由是獨立的還是沿用舊形

前提（已回讀）：Kernel 之後，store 的新 consumer 是 PR #218 的 `Column<T>`（`Storage.hpp` 的 `std::vector<Cell<T>>`）；PvP 現行程式仍用 `GYO::EntityStore`，遷移後它的欄變成 World 的欄。所以「誰會壞」分**今天**（PvP＋PR #218）與**遷移後**（只剩 Kernel）。

出處類型照偵察 A：(i) 由 Feature／Contract 獨立推出；(ii) 從第一個 consumer（PvP）抽出；(iii) 依賴撤回的 `DeferredChanges` 或舊 framework／舊禁令前提。

| DI（`entity-store.md` 行，已回讀） | 不守的話誰會壞 | 原理由的出處類型 | 有沒有獨立理由（寫出來） | 判定 |
|---|---|---|---|---|
| 1 鍵由外部給、無 generation（:130-136） | 今天：PvP 的 Gateway 玩家段（E-2）無法直接當鍵。遷移後：store 在 World 之下，World 已不配發（DI-KS 4），store 配不配發已無人觀察得到 | (ii)：唯一根據 E-2、E-3（survey 自標「沒有參照先例、根據只有 E-2」）；「無 generation」半條已被 K.3 第 1 項推翻（格式含 generation，:136 的註） | **部分**：「配發器必須是排程與快照看得到的 Resource」有獨立理由（重放 host、Rewind 要重現同一組 id；spec A.1 :95-97、H.5 :902）。但這推不出「容器／World 不配發、由產品給」——Kernel 內建一個 Resource 型配發器同樣滿足（PR #218 自己就附 `RangeAllotter`，`detail/Storage.hpp:305-314`）。「由產品給」的根據是「多個配發者（Gateway 段＋Match 段）＋Client 不配」，即 E-2／E-3，出處類型仍是 (ii)；成熟參照是 Flecs 的 entity range／`make_alive`（entity-model :40-41，Flecs 未查證）。「無 generation」半條沒有獨立理由，且已不成立 | 理由要換（敵對檢查第 8 項後修正：「與 PvP 無關」過度） |
| 2 順序是不變式不是操作（:137-142） | 今天：PvP 的 `RequireEntityOrder`、digest 編碼假設欄已升冪。遷移後：只有 W-1／J-1（欄升冪）與 W-2／J-2（查詢順序）依賴它；**若查詢順序由 Kernel 另行保證（§2 方案 C），沒有任何 Feature 會壞** | (ii)：最上游是 PvP E-5／E-9 與起點理由「增刪歷史會讓 Client 與 Match desync」（entity-model :57，已回讀；該處自己已把理由修正為「成本與預設路徑」） | **部分有**：Delta-1 的 Feature（Match／Client 共用模擬核心）＋H.5 的部分快照 Restore，確實需要「查詢順序只取決於成員集合」（W-2，§2.2）。但它要求的是**查詢順序**，不是**容器順序**；「只有容器順序做得到」沒有根據 | 理由要換；「容器順序」是實作選擇，要不要升為不變式見 D1 |
| 3 查找＝欄上二分、無側表（:143-145） | 無人功能上會壞；是效能與一致性的選擇 | (ii)＋(i)：「側表是成員的第二份副本」借 AGENTS §3；「sparse 長在死 id 上」以 PvP 的單調不回收、340 萬累計為前提 | **弱**：AGENTS §3 管的是**別的 Component** 持有的權威狀態副本，同一容器內可重建的索引不在其列；「死 id」只在不回收策略下成立。可換成：「在已排序的欄上，400 實體二分約 9 次比較，不值得再維護索引」（規模理由，推論未量） | 理由要換；結論隨 D1 |
| 4 結構變更延後、依 id 套用（:146-150） | store 半條（只提供有序 insert／erase）＝DI 2 的重述；World 半條的權威已移到 DI-KS 9 | (iii)：原條全文就是撤回的 `DeferredChanges`；consumer 證據 E-12 是 PvP | **有（World 層，§6.4 第 8 列）**：同步點前持有的參考不失效（C++ vector 的失效規則）；套用依 id 而非記錄序，結果不依 System 之間的記錄順序 | 理由要換；store 側這條可併入 DI 2（改 DI 要使用者同意） |
| 5 store 不執行任何東西（:151-156） | 若 store 呼叫使用者程式，World 外就多一條 hook／callback 派送路徑 | (iii)：原理由是舊 `docs/architecture.md` 禁令（survey 六次引用）；2026-11-11 已改 | **有**：使用者程式只由 Executor／World 派送（DI-KS 11 的 hook、E 節的 System）；多一條派送路徑就多一個順序契約（spec M 表不做 Observer 的同一理由，:1307，已回讀） | 理由要換、結論不變 |
| 6 狀態權威（:157-161） | 若 store 快取 change tick、count 等，World 的 stamp（target/world §6）就有第二份 | (i)＋(ii)：條文是 AGENTS §3 的機制版；change tick 部分引 PvP | **有**：AGENTS §3。PR #218 的 `store_stamp_guard.py` 守住 | 仍成立 |
| 7 誤用是 Programmer Error（:162-165） | 若改回 Result，所有呼叫端要處理不會發生的錯 | (i)：error-handling 規則 3、6 | **有**；但前提「所有輸入都來自呼叫端程式」在 Kernel 下多一條路徑：Client 的 `Restore` 用**由 wire 解碼的**部分快照整欄替換（spec H.5 :903、H.4 ClientSim 列 :877，已回讀）。解碼後若有重複或亂序的 id：若 Restore 逐筆插入，會走到 `InsertEntity` 的 GYO_ASSERT；若整欄替換而驗證關著，則**靜默破壞 W-1**（推論：Restore 屬 rollback-extract 批，尚未實作）。spec 對 `SpawnWithId` 的外部重複 id 寫了 Runtime Error（:98），對 Restore 的解碼結果**未找到**對應條文（掃描 0 件；正例：同類正規式命中 :98 的 SpawnWithId 條 2 件） | 仍成立（store 層）；缺口另列 C5 |
| 8 由構造保證決定性（:166-169） | 若 store 用 hash／指標順序，World 的 W-10 掃描會漏 | (ii)＋(iii)殘句：「consumer 跨程序跨平台逐位元比對」＝PvP digest；括號內是撤回機制 | **有**：Kernel 自己的 W-10／DI-KS 3（Kernel 目錄掃描照 entity-store 守衛列 5、6、8，spec :169）與重放 host、等價 host（H.4）都要求同一輸入同一結果 | 理由要換、結論不變；刪括號殘句 |
| 9 不認識產品的葉（:170-173） | 下層模組依賴產品＝AGENTS §6 違反 | (i) | **有**：AGENTS §6、§9 | 仍成立 |
| 10 鍵只有一種讀法（:174-178） | 若 store 指名欄位，consumer 的命名變 Engine 知識 | (ii) 觸發（PvP 的 `playerId` 過渡欄）＋(i) 理由 | **有**：AGENTS §6；PR #218 的 `Cell<T>` 用主模板讀 `id`，仍受益 | 仍成立 |

**小結（§1）**：理由獨立成立的 4 條（6、7、9、10）；理由沿用 PvP 或撤回機制、但能換成獨立理由的 4 條（1、4、5、8）；**DI 2（與隨它的 DI 3）換掉理由之後，結論的範圍要縮小**：Feature 支持的是查詢順序，不是容器順序——所以列在 §7.3（C6），不在「結論不變」。DI 2 的形狀從哪裡來，見 §5.3 的系譜。

---

## 2. 「依 id 排序的欄＋多欄交集」對 Bevy 的「archetype table＋swap_remove、順序不保證」

### 2.1 Bevy 與 lightyear 實際怎麼做（已回讀）

| 事實 | 出處 |
|---|---|
| 刪除＝`swap_remove`：最後一列搬進洞，回傳被搬動的 entity | `storage/table/mod.rs:227-260` |
| 每個 iter 文件寫「Iteration order is not guaranteed.」 | `system/query.rs:655` |
| 要決定性順序：「sort the results of the query using the desired component as a key」，要取回整個結果集、配一個 `Vec` | `world/mod.rs:1832-1835` |
| `sort` 不跨 System 執行快取 | `query/iter.rs:505` |
| Bevy 文件沒有聲稱查詢順序可重現或跨機器同序 | 偵察 B §6（`git grep -i determinis`；本審未重跑） |
| lightyear：「we don't have a good way to guarantee that we are iterating through entities in a stable order on both client and server」→ checksum 只雜湊元件資料再 XOR（與順序無關、**不含 entity id**） | lightyear `checksum.rs:6-7`、`:317`（TODO）、`:351-352` |
| lightyear 鎖步範例：Avian 物理受插入順序影響，所以先依遊戲自訂的 `PlayerId` 排序再插入 | lightyear `examples/deterministic_replication/src/client.rs:94-98` |

### 2.2 GYO 的決定性需求逐條：要的是「容器順序」「查詢順序」還是「結果決定性」

先區分三個層次（本節的核心）：

- **容器順序**（W-1：每欄 keys 嚴格升冪）
- **查詢順序**（W-2：查詢依 id 升冪、只取決於成員集合）
- **結果決定性**（同一輸入同一結果；跨 World 比較時同一成員同一結果）

| 需求（出處，已回讀） | 原文要求 | 需要容器順序？ | 需要查詢順序？ |
|---|---|---|---|
| Delta-1 Feature：Match 權威與 Client 預測共用同一份模擬核心（migration-plan :738）；mechanism §4 第 3 點「同一份 System」（:111）；H.3「同一組 (c) System」、cFingerprint 相同（spec :846-848） | 共用 System；未提順序 | 否 | **是**（下方推導） |
| spec A.3 第 4 點（:147）、H.3 末句（:848）、A.8 取捨 1 理由 (1)（:212） | 「不需要每個 System 自己排序」 | 否（A.8 自己承認替代是「依契約鍵排序」） | 是（理由是**負擔分散**，不是「做不到」） |
| PvP E-5（entity-model :275） | 「每個元件欄依 EntityId 嚴格升冪」 | **是**——但這是產品條文，不是 Feature | — |
| PvP E-9（:279） | digest「每欄依 EntityId 升冪編碼」 | 否（編碼器可排序） | 否（是編碼順序） |
| PvP E-6（:276） | 跨實體平手只用 EntityId 升冪；「判定結果不依容器而定」 | 否：現有平手是顯式排序或取最小 id（`ResolveActions` 的 (A, PlayerId, ActionId) 排序、`QueryShot` 等距取最小 id，entity-model :112） | 否 |
| 問卷 4-6（entity-requirements :74、:155） | AI「只要 Match 自己一致」 | 否 | 否（Match 內歷史相同） |
| determinism 邊界規則 2（target/determinism.md :35） | Client 預測、與權威比較的路徑在邊界內 | 否 | 是（同 Delta-1） |
| determinism 表「Total order from ids, never from containers」（:60） | 規則名本身說「順序來自 id，不來自容器」 | 否——字面上反而支持「順序由 id 算出，不靠容器」 | 是 |
| H.5／DI-KS 12：Rewind 或 Restore＋Replay 與一次跑完逐位元相同（:899-907；DI-KS 12） | 結果逐位元 | 否（§2.4） | Restore 那半條：是 |

**推導（推論，未量測）**：

1. 在**同一個 World、同一段歷史**裡，`swap_remove` 類儲存的實體順序是操作序列的確定函數（`table/mod.rs:227-260` 是純函數；archetype 依建立順序編號，偵察 B §3，未回讀）。所以 Match 自己的決定性、重放 host、三平台 golden，**只要歷史相同，非排序儲存也成立**。問卷 4-6 與現有 37 行 golden 不能區分方案。
2. 會讓歷史不同的是**跨 World** 的比較：Client 在 H.5 以權威部分快照 `Restore`——「這些欄的成員集合整欄替換成快照的集合」（:903）——之後的插入／刪除歷史與 Match 不同。非排序儲存下，Client 這些欄的實體順序＝快照解碼順序＋自己之後的歷史；Match 的順序＝Match 自己的歷史。
3. 因此，結果依跨實體順序而定的 (c) System（事件的發出順序、記錄時配 id 的 `Spawn`、浮點累加、推擠——entity-model §5.1 點名 Boss「不受推擠」與 `TraceMovement` 的輸出順序破壞獨立性，:233）在 Client 與 Match 上會得到不同結果，除非該 System 自己依 id 排序。
4. 今天 Client 只預測本地玩家移動（單一實體，H.5 :907），所以**今天**沒有觀測到的壓力；但 Delta-1 是已核准的 Feature，H.4 ClientSim 列寫明 `Simulate` 是「與 Match 同一份 System」（:877）。這屬 AGENTS §2 的**基盤預見**（根據 1：名指的 Feature），可以不等觀測。

結論（敵對檢查第 4 項後收窄）：

- **Feature 層能獨立推出的只有**：結果依跨實體順序的 (c) System，兩端要同結果，所以 **(c) 查詢的順序必須是兩端共有的契約鍵的函數**。
- 「以 World 的鍵 EntityId 為序」還要加一個前提：**契約鍵＝World 鍵**（A.8 取捨 1、E-1／E-2，產品條文＋使用者 2026-11-11 的決定）。若契約鍵是一個元件（提案 2／3、replicon 的作法），Kernel 依那個元件排序同樣滿足。
- 推導 2 的「Restore 讓歷史不同」也不完全獨立：H.5 的「(keys, 值) 整欄替換」本來就是在「id＝契約鍵、欄依 id 排序」之下設計的（修訂 S2），屬於被審的同一份設計。
- 部分快照下兩端的成員集合本來就不同，W-2 只保證**共有實體的相對順序**一致——必要，不充分（結果仍要該 System 只讀到兩端共有的實體）。
- W-1（容器順序）沒有 Feature 根據。

### 2.2b (B) 出局的真正條件（敵對檢查第 4 (d) 項）

若權威快照是**全量**、**依 Match 的儲存順序編碼**、Client **依解碼順序重建**，swap_remove 類儲存在兩端是同序的（推論）。破壞它的是兩個條件：

1. **部分快照**（H.5 :903）——Client 缺的實體在 Match 佔了槽位。這是 Feature 層的事實（Client 只收到有興趣的部分，entity-model §4.2 interest）。
2. **wire 依 id 排序**（E-9、entity-model §5.1 :232）——這是**產品條文**，不是 Feature。

所以「(B) 出局」實際上靠的是條件 1（Feature）＋ (B) 的每個 System 要記得排序的負擔；條件 2 只在不想動產品條文時才算數。

### 2.3 五個方案的比較

規模基準：同時約 400 實體、6 種固定元件組合（entity-model §1.4 :56，已回讀；問卷 1-2）；RC-1：4,000 實體、欄維護 p95 1 ms（entity-model RC-1，已回讀）。
唯一的實測：PR #218 的 BU-2 記錄（本機 Intel Mac、RelWithDebInfo，只當參考，宣告判定 8）——一次 Run（400 實體×8 欄、20 個空函式體的 (c) System，各取一個兩欄 Query 但**不疊代**）p95 17.7 µs；生滅 4＋4、8 欄：400 實體 p95 14.0 µs、4,000 實體 p95 142 µs（PR #218 dev_log、`tests/common/kernel/WorldExecRecord.cpp` 檔頭，已回讀）。注意（敵對檢查第 3 項後補）：該基準的元件值只有 8 位元組（`Col{int32,int32}`，`Cell`＝32 B，`WorldExecRecord.cpp:38-41`）；生成是**最大 id**（插入落在尾端，最便宜），刪除是**最舊**（落在頭端，最貴）；量的是「只有 Churn System 的整次 Run」；檔頭自述「recorded, not judged」（`:1-20`），而 RC-1 的判定機器是 Linux Match。保序插刪的成本與 `sizeof(Cell<T>)`×欄數成正比（142 µs ≈ 32 次搬 128 KB），元件變大就線性放大。**查詢疊代完全沒量**：PR 的實作每實體約 4 次二分（`Matches` 2 次＋`ValueOf` 2 次），`Entities()` 每次配置一個 vector（`Params.hpp:231-251`、`:283-287`、`:321-329`）；兩位審查者各自粗估 400 實體×20 System 為數十到數百 µs、4,000 實體可到數 ms——**都是推論，未量**。所以 BU-2 **不能**當「排序欄在預算內」的證據。

| | (A) 現況：排序欄、W-2 為容器不變式 | (B) 非排序（swap_remove／archetype）＋需要的 System 自己排序（Bevy `sort*`） | (C) 非排序＋Kernel 只提供「依 id 有序」的 (c) 查詢 | (D) 順序無關 digest（lightyear XOR）＋只在平手處排序 | (E) 兩個 id 空間＋對映（replicon），Client 自配 id |
|---|---|---|---|---|---|
| 生滅成本 | 每筆插入／刪除 O(n) 搬移；PR #218 逐筆插入 O(n·k)（spec A.6 的一次合併 O(n＋k) 未實作）。**實測** 400：14 µs、4,000：142 µs（4＋4） | O(1)＋位置表更新；archetype 加減元件要整列搬表（`move_row`，偵察 B §3） | 同 (B) 的 O(1)；另要維護「依 id 有序」的鍵視圖（每個變動的欄在同步點 O(n＋k) 合併一次，或每次查詢排序）——推論 | 同所搭儲存 | 另加對映表的插刪 |
| 單欄疊代 | 線性 | 線性（最快） | 經有序鍵視圖間接取值（多一次間接、cache 較差）——推論 | 同所搭儲存 | 同所搭儲存 |
| 多欄查詢 | spec 寫 leapfrog O(總長)；**PR #218 實作是「最小欄驅動＋其餘欄二分」O(m·t·log n)**，`ForEach` 經 `Entities()` 跑一次 `Matches`，之後每項 `ValueOf` 再 `Find`（重跑 `Matches` 的是 `Get`／`GetMut`，`Params.hpp:283-287`）。400 實體、2 項：每查詢數千次二分、數十 µs 量級——推論，未量；4,000 實體、20 個 System 是否超過 1 ms：**未量** | archetype 預先配對、無查找（最快）；要排序時每次 collect＋sort＋逐實體隨機取（`iter.rs:1060-1070`，偵察 B） | 驅動欄的有序鍵＋其餘欄 O(1) 查找（需要索引，與 entity-store DI 3／DI 8 衝突，所以不會是 entity_store）——推論 | — | 排序鍵是一個元件（server id），不是 Entity |
| 每個 System 的負擔 | 無 | **每個結果依順序的 (c) System 都要記得排序**；Kernel 無法判斷哪些 System 依順序 | 無（(c) 區沒有「無序查詢」這種形式） | 只解 digest；System 結果的順序依賴照樣存在 | 依順序的 System 要依 server id 元件排序（lightyear 範例即如此） |
| 守衛 | J-1（欄升冪）、J-2（不同歷史同一序列）＋`es-row2-*` 突變 | 只能逐點審查；或加「打亂儲存順序再比 digest」的差分測試（GYO 沒有；只涵蓋測試情境走到的路徑）——推論 | J-2 照用（只測查詢）；J-1 改測有序鍵視圖 | XOR 讓兩筆相同紀錄互相抵銷；不含 id 時抓不到 id 錯誤；無法定位差在哪一列 | 對映表的一致性（第二份成員關係）要新守衛 |
| 回滾（Rewind，自己的快照） | 快照＝(keys, 值)；還原後順序與當時相同 | 快照若是欄的整份拷貝，儲存順序本來就在裡面，還原後順序相同（敵對檢查第 11 (e) 項：前一版把這寫成缺點，偏嚴） | 查詢順序由成員算出；但 H-5、DI-KS 12、J-42 比的是**回滾集合的位元組**、A.6 第二層比 `StorageBytes()`、`World::Summary` 依欄序（`World.cpp:829-836`）。dense 順序依歷史，所以快照要連 dense 順序存回，或把這些比較改成經有序查詢——前一版寫「快照不必存順序」偏寬 | 同所搭儲存 | 同所搭儲存 |
| 部分快照 Restore（Client，權威送來） | 整欄替換後仍是 id 序，與 Match 同序 | **Client 的實體順序與 Match 不同**（§2.2 推導 2）；只有自己排序的 System 安全。archetype 下「只換部分欄的成員」還會引起大量搬表 | 不受影響 | 同所搭儲存 | Client 的本地 id 依接收順序配，與 server 順序無關；依 server id 元件排序才同序 |
| digest（E-9） | 直接依欄序編碼 | 編碼器先排序（400 個，微不足道——推論） | 走有序查詢編碼 | 改格式＝37 行 golden 重生（§14.4 核准） | digest 寫 server id 元件 |
| 平手規則（E-6） | 已是顯式排序，不靠欄序 | 同左 | 同左 | 同左 | 平手鍵改成 server id 元件 |

**(C) 有兩種，前一版混在一起**（敵對檢查第 11 (c) 項）：

- **(C1) 有序鍵視圖**：keys 仍嚴格升冪（W-1 的字面照樣成立），只是值不隨鍵搬，經間接索引取值。不必動 W-1，只要 RC（K.2 第 6 條／target/kernel :95 已寫「換儲存，DI-KS 3 的順序不變」）寫明不變的是什麼。
- **(C2) 每次查詢排序**：容器無序，W-1 不再成立。

**前一版漏掉的兩個方案**（敵對檢查第 6、7 項）：

| | (F) 每個元件組合一張依 id 排序的表＋查詢時 k 路合併 | (C)×(E) World 配鍵＋Kernel 依「契約鍵元件」的有序查詢 |
|---|---|---|
| 是什麼 | archetype 式的組合表，但表內保持 id 升冪（不 swap_remove）；多組合的查詢做 k 路合併 | World 自己配 Entity（可回收），產品的契約 id 是一個元件；(c) 查詢依該元件排序（Bevy `world/mod.rs:1832-1835` 建議的排序鍵就是元件；lightyear 範例依 `PlayerId` 排序，`client.rs:94-98`） |
| 好處 | 名指規模是 6 種**固定**組合——正是 archetype 不搬表、不碎片的情況；消掉 PR #218 查詢的每實體二分 | 「每個 System 自己排序」的負擔一樣由 Kernel 承擔；World 不必信任外部給的鍵 |
| 代價 | Kernel 通用時 Insert／Remove 元件要搬表（保序搬移）；查詢要合併多表 | 有序查詢多一層排序或索引；digest／wire 要寫契約鍵元件；違反 E-1／E-9（產品條文） |
| 判定 | **未比較**——spec M 表與 A.7 第 4 列以「archetype 的好處用不到」否決，但那只對 swap_remove 的表成立 | 讓 A.8 取捨 1 的理由 (1)「每個 System 的負擔」也不能區分兩案（§6.4 第 4 列、C9） |

### 2.4 Restore 對 (B)、(C) 的影響

- **(B)**：Client 每次 Restore 後，被替換的欄依快照解碼順序重建，沒被替換的欄保留 Client 自己的歷史；之後的 Replay 再疊上 Client 的生滅歷史。Match 是 Match 的歷史。兩者的實體順序一般不同。後果：沒有自己排序的、依順序的 (c) System，在 Client 上的結果與 Match 不同 → 比較不一致 → 再 Restore＋Replay（最壞情況是每個權威快照都觸發回滾）——推論。另外 Rewind 用的 PredictionHistory 必須連實體順序一起存，否則 DI-KS 12 的「與一次跑完逐位元相同」對依順序的 System 不成立。
- **(C)**：查詢順序是成員集合的函數，Restore 只改成員與值；**查詢層**沒有 (B) 的問題。但位元組層的比較（H-5、DI-KS 12、J-42、A.6 第二層、`World::Summary`）看的是儲存，(C2) 的 dense 順序依歷史，要嘛快照連順序存回、要嘛把這些比較改成經有序查詢（§2.3 Rewind 列）。代價還有：儲存要有索引（或每次查詢排序），且不能再用 `engine/entity_store`（DI 3、DI 8 禁側表與雜湊）。
- **(A)**：與 (C) 一樣不受影響；代價是每次插入／刪除搬移值。

### 2.5 哪個 Feature 讓哪個方案出局

| 方案 | 出局嗎 | 讓它出局的 Feature 或理由（出處） |
|---|---|---|
| (A) | 否 | — |
| (B) | **出局（條件式）** | Delta-1（共用模擬核心，migration-plan :738）＋部分快照（§2.2b 條件 1，Feature）；「wire 依 id 排序」（條件 2）是產品條文，不算 Feature。依順序的 (c) System 都要自己排序，漏一處就靜默分歧。這是「負擔與可檢查性」的理由（AGENTS §3：判斷できる形で書かれた約束），不是「做不到」。若使用者接受「每個依順序的 System 自己排序＋差分測試」，(B) 技術上可行 |
| (C) | 否 | **在 Feature 層，沒有 Feature 能區分 (A) 與 (C)**（位元組層的比較要另付代價，§2.4）。差別在效能形狀與實作複雜度：值大、生滅多時 (C) 較省；400 實體、6 組合、值小時 (A) 較簡單——**但 (A) 的查詢疊代沒量，「在預算內」沒有證據**（§2.3 注意） |
| (F) | 否（未比較） | 見上表 |
| (C)×(E) | 否 | 只有產品條文 E-1／E-9 與使用者 2026-11-11 的決定讓它不採；沒有 Feature 讓它出局 |
| (D) | 單獨用：出局 | 只處理 digest，不處理 Delta-1 的 System 結果；改 E-9 要重生 golden。可以當 (A)／(C) 之外的附加檢查，不是替代 |
| (E) | 否（目前沒有 Feature 需要） | 支持它的 Feature 是「Client 要生出有權威意義的實體」（entity-model RC-5）；問卷 1-5 選了「生成事件＋純軌跡；不當每 Tick 同步的實體，但要有 id 讓命中結果能指它」（`docs/object_fps_pvp/plans/v8/entity-requirements.md:37`；問卷沒寫「id 由權威給」，前一版轉述有誤），所以目前不需要。反對它的是產品條文 E-1／E-2／E-9（一個 id 空間、id 直接進 wire 與 digest），不是 Feature。**但見 §3.5：即使不採 (E)，Client 預測生成的缺口仍在** |

---

## 3. id：外部給、不重用、generation 恆 0

### 3.1 是 Feature 要求還是舊形

| 主張 | 最早出處（偵察 A，部分已回讀） | Feature／規模根據 | 判定 |
|---|---|---|---|
| 玩家段由 Gateway 配、EntityId＝player_id | Gateway `nextPlayer` 自 MVP（2026-09-25）就存在；2026-11-01 才寫成 E-2 | 有：Gateway 依 player_id 路由；若改由 Match 配，Gateway 就要持 player_id↔entity_id 對照表＝Match 配發狀態的副本（entity-model §2.3 第 2 點，已回讀；AGENTS §3） | 產品決定；先有實作後補理由，但理由本身獨立成立 |
| Match 段單調不重用 | E-3（PvP） | 有：規模——一個程序 7 天約 340 萬（問卷 1-4）；Match 段 [2^24, 2^28−1] 約 2.5 億（entity-model :82-83，已回讀），沒有回收壓力；wire 的 id 契約最大 4 位元組 varint（同處註解） | 產品策略，理由獨立（規模＋wire 相容） |
| generation 恆 0 | K.3 第 1 項的 PvP 策略 | 有：同上；值與今天逐位元相同，wire、digest 不改（spec :87，已回讀） | 產品策略，成立 |
| Kernel 的格式含 generation | 使用者 2026-11-11（K.3 第 1 項） | **未寫**：A.8 只寫「Engine 保通用」（:212）。AGENTS §2 的基盤預見要根據 1（名指 Feature）或 3（規模），兩者都沒寫；能想到的候選是編輯器文件 World（長時間編輯的生滅），沒有文件點名 | 對 PvP 零成本；**預見的根據沒寫**（併入 D3） |

### 3.2 與 Bevy 配發器比較（已回讀）

| Bevy | 出處 | GYO | 差別的意義 |
|---|---|---|---|
| 先取 free list，空了才 `fetch_add` 新 index | `entity/remote_allocator.rs:853-856` | `RangeAllotter`：單調、用盡回報、不回繞、無 Reset（PR #218 dev_log）；產品可給自己的 `EntityAllotter`（PR #218 `static_assert(EntityAllotter<T>…)`） | GYO 預設不回收 |
| free list **LIFO**（取 `len-1`） | `remote_allocator.rs:643-652` | — | Bevy 的重用順序取決於釋放歷史 |
| despawn 時 generation＋1，回繞只 `warn!` | `entity/mod.rs:1058-1071` | PvP generation 恆 0 | — |
| `spawn_at`：「可以 spawn 尚未配發的 entity，但配發器之後可能發出同一 index → panic」 | `world/mod.rs:1062-1066` | `SpawnWithId`：id 落在本 World 配發器範圍內＝GYO_ASSERT；分段讓兩個配發者不相交（spec :98） | GYO 用「分段＋Client 不配發」避開 Bevy 警告的碰撞 |
| `Entity::from_index`：「一般不應試圖讓兩個 instance 的 `Entity` 對齊，應另插一個識別元件」 | `entity/mod.rs:554-566`（引句 :563-565；前一版寫 :533-544 是抄偵察的錯行號） | GYO 正好相反：產品的契約 id **就是** World 的鍵（spec :91） | GYO 的取捨：不要對照表（快照逐實體比、id 直接進 digest／wire，entity-model §1.2 第 5 點）；代價是 Client 不能自己生出實體（§3.5） |
| `Ord` 直接比 `to_bits()`：先比 generation，再比低 32 位 | `entity/mod.rs:492-500`；測試註解「ordering is by generation then by index」`:1391` | 數值升冪＝先比高 32（generation）再比低 32（index） | 欄位位置相同 |
| 低 32 位存的是 **`!index`**（`NonMaxU32` 以 transmute 取位元；測試 `from_bits(0x…00cc).index_u32() == !0xcc`） | `entity/mod.rs:180-183`、`:1331-1333` | 低 32 位存 index 本身 | 同一代內 Bevy 是 **index 大者在前**（`:1449-1456`：`(1,gen1) > (2,gen1)`），GYO 是 index 小者在前 |
| `to_bits`「No particular structure is guaranteed for the returned bits」；位元與 `Ord` 可跨版本改變 | `entity/mod.rs:582-588`；偵察 B §1（:374-383，未回讀） | GYO 把位元結構寫進契約（DI-KS 4） | Bevy 不把 Entity 位元當跨 instance 契約 |

**「照 Bevy 格式」是否準確**：只有「index 在低 32、generation 在高 32」這一點準確。低位的編碼（Bevy 存 `!index`）、同代內的排序方向（相反）、Bevy 明說位元無保證結構——三點都不同。對 PvP 沒有功能後果（generation 恆 0、GYO 自己定義數值升冪），但「照 Bevy」容易讓讀者以為 GYO 的順序或位元與 Bevy 相容 → R13（理由要換，結論不變）。

### 3.3 target/world DI 2 與 W-9「不重用」對 world §1「別的產品可選槽回收」的矛盾

已回讀：
- spec W-9（:168）：「只有 `EntityAllotter` Resource 配發，**單調、不重用**、用盡時回報錯誤不回繞」，守衛 J-8（:1053）。
- target/world DI 2（:72）：「World 不配發、**不重用鍵**」。
- target/world §1（:14）、spec A.1（:87）：「別的產品可選『槽回收加版本號』」。
- DI-KS 4：「配發策略（PvP：單調不重用、generation 恆 0）由產品宣告」。
- PR #218：配發器是一個 concept（產品可給任何實作），Kernel 附的 `RangeAllotter` 是單調的；J-8 的突變打的是 `RangeAllotter`（`we-j8-allotter-wraps`、`we-j8-allotter-reset`）。

評估：程式（concept＋預設實作）與 DI-KS 4、world §1 一致；**W-9 與 world DI 2 的標題把 PvP 的策略寫成 Kernel 條文**。有一個兩全的讀法：「不重用」指 **u64 值**不重用，而槽回收＋generation＋1 產生的值本來就不同（只要 generation 不回繞）；但 W-9 的「單調」排除了這個讀法（回收槽帶 generation＋1，值落在所有 generation 0 的值之上，之後新配的 generation 0 值又比它小，序列不單調）→ C2，選項在 D3。

### 3.4 為什麼 Client 與 Match 的同序不需要「不重用」

只要 **Client 不配發**（E-3），Client 拿到的每個 id 都是 Match 配的，兩端的值相同，不論 Match 是否回收。「不重用」真正保護的是**持有 id 的引用**（`targetId`、`lastAttackerId`、晚到的事件）不會指到新實體；有 generation 時由 generation 承擔，但 generation 要上 wire（超過 4 位元組 varint）。所以「不重用」的獨立理由是**引用安全＋wire 相容**，不是疊代順序。這修正 A.8 取捨 1 理由 (3)「E-3 的不重用讓 generation 沒有用途」的因果方向：是「選了不重用，所以不需要 generation」。同理，spec :87 與 :212 末句「槽回收→兩端配發歷史不同時，數值順序不再是契約鍵的順序」把「回收」與「兩端各自配發」混在一起：只要 Client 不配發，Match 回收槽也不會讓兩端不同序（R15）。

### 3.5 Client 預測生成拿不到 id 的缺口

已回讀的事實鏈：
1. Client 的模擬 World 沒有配發器（E-3）；在這種 World 登記帶 `Spawn` 的 System＝「缺 Resource」啟動錯誤 D-E3（spec :98、:474、J-8 :1053）。
2. ClientSim 的 `Simulate`＝「與 Match 同一份 System」、cFingerprint 相同（:877、H-4 :939）。
3. 已點名的功能會在 tick 內生成實體：AI 會死、屍體另生實體、Boss 發射投射物、撿物品刪實體（entity-model §5.2 :239，已回讀）；遷移後這些是 (c) System 的 `Spawn`。
4. kernel／target／networking 中**沒有**預測生成或 id 對映的條文（偵察 C 的掃描；本審重做：`ServerEntityMap|PreSpawn|預測生成|cFingerprint` 等，於 spec、target、networking 只命中 networking :16 把 PreSpawned 當 hook 的用途、:42 的「預生成」一詞與 cFingerprint 本身的定義，已逐行看過）。

推論：AI 批把生成實體的 System 放進 FixedUpdate 的那一刻，1 與 2 就互相矛盾——要嘛 Client 不能登記同一組 System（cFingerprint 不同，Delta-1 的「同一份」不成立），要嘛 Client 要有配發器（E-3 禁止）。replicon 用 `Signature`、lightyear 用 `PreSpawned`（生成 tick＋元件種類的雜湊；回滾時刪掉重生、拿新的 `Entity`）處理這件事（replicon `lib.rs:634-644`，已回讀；lightyear 偵察 B §8，未回讀）。這是基盤預見（AGENTS §2 根據 1：AI 批是名指的 Feature），應在 AI 批之前決定 → C4、D4。

---

## 4. #176「自製（維持）」：比較是否公平、漏了什麼、重判做了沒

已回讀 `evaluation.zh-Hant.md` §1.1（:37-76）。

| 檢查項 | 事實 | 評估 |
|---|---|---|
| 判準 | 「順序不變式」與「id 由外部給」（:67） | **循環**：兩條判準就是被比較者自己的 DI 2 與 DI 1。公平的判準應是 Feature 層（跨 World 結果一致、digest 可重現、規模預算），不是「候選有沒有我們的不變式」 |
| 「需要它的 Feature」欄 | 列 AI 小怪、場景物件「3b `DeferredChanges` 與第一個非玩家批同時」、pv10「RC 6 的 change tick 候選」（:44） | 三項中兩項的依據已撤回或被取代（`DeferredChanges` 2026-11-11 撤回；RC 6 被 World 的 stamp 取代） |
| 限制 2「兩者都由容器配發」（:61） | Flecs 只寫「可回收」 | **漏**：entity-model §1.1-§1.2（:31-43，已回讀）自己記錄 Flecs 的 `make_alive`（文件說用途之一是網路 peer 同步）與 entity range（每個 range 自己配發、用盡 assert），正好對應 E-2／E-3；#176 沒有引用（Flecs 原始碼本機沒有：未查證） |
| 「開源 ECS＋排序層」 | 一句否決：「每 Tick 要再排序……或自己維護一份順序——後者就是 EntityStore 本身」（:60） | 沒有成本估計或量測；也沒有考慮 §2 的 (C)（排序在查詢層、不在容器）。當時 store 沒有查詢層（entity-store「Out of scope: Queries」），所以 (C) 在當時的比較框架裡不存在 |
| EnTT `sort_as`／group | 評估正文無；entity-model §1.3、survey 有 | 小漏（結論引用 survey） |
| Bevy 本身 | 不是候選（Rust，不能當 C++ 庫）；只當設計參照 | 合理 |
| 其他 C++ ECS | 範圍外（README 的 14 列） | 範圍內合理，但結論不能讀成「開源 ECS 都不行」 |
| 量測 | 「沒有試建、沒有量測」（:362，偵察 A，未回讀） | 判定沒有量化依據；這不違規（評估卡範圍如此），但「量級：週」的採用代價也是估計 |
| 採用代價的算法 | 把已花的調查與 PR #156 的實作作廢算進「採的代價」（:70） | 沉沒成本，不該當選型理由（敵對檢查第 17 項） |
| EnTT 的 in_place 刪除策略 | entity-model §1.1（:33）自己記了 in_place（tombstone）；#176 沒檢視「in_place＋單調 id」的組合 | 小漏；EnTT 本機無原始碼，未查證 |
| 2026-11-11 註記「待規格與 spike 之後重判」（:76） | 重判只剩「World 儲存層借 EnTT 還是自寫」 | **沒做**。spec M 表「本規格核准後，評估卡以 K.1 判斷候選能否當 World 的實作」（:1311，已回讀）；Delta-3 第 7 項「在 world-exec 宣告以 W-1～W-11 核對後定」（migration-plan :766，已回讀）。這項核對是**使用者 2026-11-11 的決定**（kernel HANDOFF:47 `v8-arch-kernel-audit-third-party`：「World 儲存層借 EnTT 或自寫不在 audit 做，等 World 實作批用 W-1～W-11 核對」；migration-plan :77「只在 world-exec 的事前宣告以 W-1～W-11 核對 EnTT 能否當儲存」，卡片主會話已回讀）。PR #218 整個 diff 不提 EnTT／Flecs：Python 重跑，正規式 `\bentt\b|flecs` 不分大小寫，master migration-plan 命中 3 行（正例）、PR diff 0 行（敵對檢查第 12 項：前一版寫的「正例 19 行」是無字界的另一支掃描，命中 `MovementTrace` 之類；分大小寫那支命中不了「EnTT」，是失效的掃描。結論不變）。world-exec 宣告提到的 W-1～W-11 核對是「EntityStore 撐不撐得住」（宣告的停止條件 :88），不是 EnTT |

**理由今天是否仍成立**：
- 原比較（「EntityStore 容器 vs EnTT／Flecs 容器」）已被 2026-11-11 註記自己宣告失效。
- 剩下的題目「World 儲存層借還是自寫」有**獨立的自寫理由**（推論，未量測）：Kernel 已自寫 Registry、Scheduler、延後佇列、stamp；存取驗證第二層要對每欄的連續位元組做快照比對（spec A.6 第二層、PR #218 `StorageBytes()`）；Restore 要整欄替換；PredictionHistory 要複製欄。外部儲存要經一個 adapter（AGENTS §6）把這些全露出來，而 PR #218 的欄本身只有約 100 行（`Storage.hpp` 的 `Column<T>`）。EnTT 的 sparse 頁表隨最大 id 增長、單調不回收時只會變大（對抗檢查 A23；EnTT 原始碼本機沒有：未查證）。
- 但這些理由**從未寫進任何決定**；Delta-3 寫的否決理由是「swap-and-pop 違 W-2」，而 §2 說明 W-2 可以不靠容器順序成立（(C)）。

→ **未決**（C7，敵對檢查第 2 項後由「理由要換、結論不變」改列）：重判沒做，新理由一半未查證、一半是未量的推論，不能說「結論不變」。是否補做、做到哪裡見 D5——跳過 EnTT 項等於偏離使用者 2026-11-11 的決定，要使用者明示。

---

## 5. Delta-3 是否也是「為舊形狀找位置」

〈Batch declaration checks〉（`docs/architecture/README.md:20-23` → `target/README.md:47-49`，已回讀）只有一個問題：「**這是不是在為舊形狀找位置？是的就不採，即使它最省工。**」

### 5.1 逐項對照 Delta-3 的七項（migration-plan :758-766，已回讀）

| Delta-3 項 | 原文要點 | 是否在為舊形狀找位置 |
|---|---|---|
| 1 Feature | 「World 的欄依 id 升冪的疊代順序（決定性 (c) 的 World 契約，PvP digest；DI-KS 3），沿用已驗證的演算法」 | **是**。列的不是 Feature，是契約（DI-KS 3）與產品 digest；「沿用已驗證的演算法」是保留舊模組的動機。真正的 Feature（Delta-1）只需要 W-2（§2.2） |
| 2 問題 | 「World 要 keys／值／stamps 平行陣列同步搬動，`EntityColumns` 把鍵放在元素內、沒有回傳位置的查找」 | **是**。這個「問題」只在決定沿用 `EntityColumns` 之後才存在 |
| 3 邊界 | entity_store 成為 World 欄的實作；DI 4 延伸；DI 5 仍成立；`DeferredChanges` 撤回 | 混合：撤回 `DeferredChanges` 是在丟舊形；「成為 World 欄的實作」不在 K.3 第 7 項第 (2) 點的文字內（spec :1183，已回讀；原文只有「DI 4 延伸到 insert／remove、DI 5 對 store 本身仍成立、`DeferredChanges` 撤回」），出自 A.6 與 L.1 的**初判**表 |

**同意的出處**（敵對檢查第 10 項，卡片主會話查證）：Delta-3 的標題寫「使用者已同意，K.3 第 7 項第 (2) 點」，但 (2) 不含「entity_store 成為 World 欄」與新邊 Kernel→EntityStore。實際把這兩件事帶進核准的是**遷移計畫本體的核准**（2026-11-13，kernel HANDOFF:68 `v8-arch-migration-plan-approved`「核准：計畫本體、A9 文字修正」；Delta-3 寫在該計畫 §4 :758-766）。也就是：這條邊是和一份大計畫一起被核准的，沒有單獨被問過；標題的出處要更正（R16）。
| 4 影響 | 「回傳位置版本或 `Cell<T>`，二選一」 | 兩個選項都以「store 必須用上」為前提 |
| 5 Dependency | 新邊 Kernel → EntityStore | 為保留舊模組新增的邊 |
| 6 Ownership | 「排序演算法仍歸 entity_store」 | 同上 |
| 7 更小的方案 | 「World 自寫一套排序欄——重複同一演算法與守衛（Valid evidence 第 1 條），不採；借 EnTT——swap-and-pop 違 W-2」 | **兩個否決理由都有問題**：(a)「重複」：遷移後 PvP 的欄進 World，entity_store 的 consumer 只剩 Kernel，不存在兩處重複；且 §5.2 顯示 PR #218 自己仍手寫了同一演算法。(b)「違 W-2」：W-2 是查詢契約，(C) 可以不靠容器順序滿足。替代方案只列兩個，(C) 沒進比較 |

### 5.2 PR #218 的實際形狀說明了什麼（已回讀 `Storage.hpp`、`Params.hpp`、`World.cpp`，分支 `ea62c6e1`）

| 事實 | 位置 | 說明了什麼 |
|---|---|---|
| 欄＝`std::vector<Cell<T>>`，`Cell<T>{id, added, changed, value}`；選它的理由是「store 不必加回傳位置的介面、它的突變一覽不必重列」 | `Storage.hpp:48-54`；target/world §6 定案段（diff） | 佈局以「不動舊模組」為理由；spec A.6 寫的 SoA（keys、值、stamps 分開）被 AoS 取代 |
| 新增要求：元件型別要 nothrow move 指派（vector 中段插刪用 move 指派） | target/world §6 定案段 | 為了用舊容器，對所有元件型別多加一條約束（交後續批改 W-8） |
| `World::entities_`（全部活實體）用 `std::lower_bound`／`insert`／`erase`／`std::binary_search` **手寫維護** | `World.cpp:100-131`、`:548-563` | 同一演算法在 Kernel 裡又寫一次——`EntityId` 沒有 `id` 成員，但 DI 10 的 `EntityKey<T>` 可以特化（`EntityKey.hpp:27-33`），是**沒有用**，不是用不上。Delta-3 第 7 項「避免重複」的理由在實作中沒有達成 |
| `Changed`／`Added` 的 `Fresh` 是**手寫二分**（經虛擬 `KeyAt`） | `Params.hpp:311-319` | 第二處繞過 |
| 欄解構用 `std::reverse`＋`pop_back` 達成 W-7 | `Storage.hpp` 的 `~Column()` | 第三處（與排序演算法無關，但同樣在 store 之外處理順序） |
| 多欄查詢是「最小欄驅動，其餘每項是查找」，不是 spec A.3／A.6 的 leapfrog | `Params.hpp:229-251`、`:287-309`；dev_log「結果相同、成本不同」 | 只有**驅動欄**需要有序；其餘欄只需要「有沒有這個 id」。這個查詢形狀本身已接近 (C)：順序由驅動欄給，其餘欄的儲存順序無關 |
| 同步點各階段先 `std::sort` 佇列，再逐筆 `InsertEntity`（O(n·k)） | `World.cpp:548-645` | Kernel 本來就在每個同步點排序；A.6 的一次合併未做 |
| 為了這條邊：K5 守衛白名單加 `engine/entity_store/`（§14.4 判定變更，使用者 2026-11-13 核准）、新增 `store_stamp_guard.py` | `kernel_guard.py:100-103`（偵察 A）；`tests/common/kernel/` | 保留舊模組要付守衛的代價 |
| World 摘要依欄序逐格寫 key＋值 | `World.cpp:829-836` | 摘要依賴欄序；(C) 下改走有序查詢即可 |

**判斷**：Delta-3 符合〈Batch declaration checks〉描述的情形——Feature 欄寫的是舊模組的不變式，「問題」由沿用舊模組而生，替代方案只比了另一個排序容器與一個被同一不變式否決的第三方庫，實作又在三處繞過它。這不表示 (A) 的排序欄錯了（§2.5：(A) 與 (C) 沒有 Feature 能區分），而是表示「**`engine/entity_store` 這個模組**作為 World 的欄」的理由主要來自「它已經存在」→ C3、D2。

### 5.3 id 序形狀的系譜（敵對檢查第 5 項；事實依偵察 A §3，commit 層未回讀）

使用者問的「舊形狀影響新設計」，最直接的證據是這條鏈：

```text
2026-09-25  PvP MVP：std::map<PlayerId, Participant> players_      ← 順序是容器附帶的，不是被要求的
            快照依 map 順序推成平行陣列                            （bbc18321 PvpMatch.hpp:61、:35）
     │
     ├─→ digest golden（37 行）依這個順序編碼
     ▼
2026-11-01  entity-model：E-5「每個元件欄依 EntityId 嚴格升冪」、E-9 digest 依 id 編碼
            §1.5 自述：「按 id 升冪的平行陣列在快照層已存在，只是由 std::map 隱含保證」
     ▼
2026-11-03  PvP EntityColumns.hpp（78778d00，PR #140）：vector＋lower_bound＋保序 erase
     ▼
2026-11-05  搬進 Engine：GYO::EntityStore（delta :31「欄的函數是現有形狀的搬移」）
     ▼
2026-11-11～13  Kernel：W-1／W-2、DI-KS 3、Delta-3（entity_store 當 World 的欄）
```

每一步都有當時的理由，但沒有一步回頭問「最上游的順序是被要求的，還是 `std::map` 附帶的」。§2.2 的回答是：被要求的只有「(c) 查詢順序是兩端共有契約鍵的函數」；「容器依 id 排序」與「契約鍵＝World 鍵」都是這條鏈帶下來的。

---

## 6. 兩條 Bevy 研究線的交叉分析

兩線：entity-store survey（2026-11-05，Bevy `v0.20.1`、EnTT `v3.16.0`）與 kernel-study（2026-11-10，Bevy main `6a911653f`、replicon、lightyear）。偵察 C 的主張驗證中，本審抽查了當結論依據的幾條（下列「已回讀」），其餘依偵察 C。

### 6.1 兩線互不引用

已回讀：`git grep -E "entity-store/survey|survey\.zh-Hant"` 於 `docs/architecture/plans/kernel` 與 `docs/architecture/target`：命中 4 檔，全部是 `json-library-survey` 與 `feel-survey`（逐行看過），**沒有一處指向 entity-store survey**；正例：同正規式於 `plans/third-party-evaluation` 命中 3 檔。後果：kernel 線把 survey 已有行號的事標成「未查證」（§6.3），並在相反方向下了結論而沒有討論（§6.2）。

### 6.2 結論相反之處

| 主題 | entity-store 線 | kernel 線 | 審查 |
|---|---|---|---|
| **何時配 id** | 「容器不在入隊時配 id」，新 id 由產品配發器依裁決順序配（survey §3b；DI 4 原文「never allots at record time」，`entity-store.md:146`，已回讀） | 「id 在**記錄時**就配好」（spec :96，已回讀）；F.7 對 Bevy「Commands 在 spawn 當下配 Entity」寫「相同」（:723，已回讀） | kernel 採 Bevy 的入隊配發，以「配發器是 Resource、由衝突分析序列化」補決定性。這是**有理由的改變**（id 立即可用於 Insert、不需 provisional handle，F.1），但沒有回頭說明 entity-store 線為何當初反對；DI 4 被改寫成只約束 store，矛盾被措辭吸收而不是被討論。附帶後果：記錄時配 id → 若記錄者在疊代中生成，id 取決於疊代順序 → 又一次依賴 W-2（Match 內同歷史，所以成立） |
| hook 引起的變更何時套用 | `DeferredChanges` 的不可重入規則：「a change caused by a spawn or destroy belongs to the next sync point」（`entity-store.md:81-82`，已回讀；撤回、未實作） | F.8 取捨 2 以這句為理由（:735，已回讀） | 見 §6.5 |
| spawn／despawn 先後 | spawn 先 | despawn 先（F.2） | 已明文處理（entity-store.md 的 Deferred changes 段開頭），不是隱性落差 |

### 6.3 「未查證」其實已查過

| spec 位置（已回讀） | 標記 | 實際 |
|---|---|---|
| A.7 第 2 列「Entity 帶 generation、index 重用」（:201） | 「bevy_core.md **未查證**（報告無此行）」 | survey :35 附 v0.20.1 行號已記；本審於 HEAD 回讀成立（§3.2：LIFO free list、generation＋1、Ord） |
| A.7 第 8 列「查詢疊代順序」（:208） | 「bevy_core.md **未查證**」 | survey §3a（:68-79，已回讀）已記；本審回讀成立（`query.rs:655`、`world/mod.rs:1832-1835`） |
| A.7 第 4 列「Table／SparseSet、archetype」（:203） | 證據 bevy_core:55 | bevy_core:55 只說有兩種儲存，**沒有「搬移改變順序」的證據**；那個證據在 survey §3a（偵察 C T6；本審回讀 `table/mod.rs:227-260` 成立） |

### 6.4 「理由欄引用舊形」候選表：逐列判定

判定問題：理由能不能換成**獨立理由**（Feature、Bevy 原始碼、AGENTS 原則、規模預算）？換得了＝理由要換；換不了＝舊形，結論可能要改。

| # | 位置 | 理由欄引用的 | 獨立理由（若有） | 判定 |
|---|---|---|---|---|
| 1 | spec A.7 並行配發器（:200） | E-9（PvP digest） | 有：重放 host 與 Rewind 要重現同一組 id，配發順序必須是 Plan 的函數（H.4、H.5） | 理由要換 |
| 2 | A.7 generation／index 重用（:201） | A.1；entity-store DI 1、E-3；GYO 欄「u64、不重用、無 generation」 | 無——該列結論已被 K.3 第 1 項推翻，GYO 欄過時 | **結論要改（文字）**：GYO 欄改為「格式含 generation、策略歸產品」，證據改引 survey 與 Bevy 原始碼（R14） |
| 3 | A.7 Table／SparseSet、archetype（:203） | M 節 → RC-3 | 前一版補的「規模讓 archetype 的好處用不到；搬表破壞順序」不成立：6 種**固定**組合正是 archetype 不搬表、不碎片的情況，「搬表破壞順序」只對 swap_remove 的表成立（敵對檢查第 7 項） | **未比較**（§2.3 方案 F） |
| 4 | A.8 取捨 1（:212） | 理由 (2)「要改四處 Architecture Delta」；(3)「E-3 的不重用讓 generation 沒有用途」 | (2) **改舊文件的成本被當成選型理由**，不是架構理由；(3) 因果方向要反過來（§3.4）；刪 (2)、改 (3) 之後只剩 (1)「每個 System 的負擔」，而 (1) 被方案 (C)×(E)（Kernel 依契約鍵元件的有序查詢）化解，不能區分兩案。剩下的支撐只有產品條文 E-1／E-9（免對照表）與使用者 2026-11-11 的決定 | **結論可能要改／交使用者確認**（C9，敵對檢查第 6 項後由「理由要換」改列） |
| 5 | A.3 第 4 點（:147） | E-2（id 是契約鍵） | 有，但要寫成條件句：「**若**產品讓兩端持有同一組 id，查詢順序只取決於成員集合，兩端同序」 | 理由要換 |
| 6 | A.6（:174、:176） | entity-store DI 2；RC-1（PvP 值） | 門檻：以名指產品的規模為預算是 AGENTS §2 根據 3，合法，但要標明「PvP 的值」；DI 2 的引用隨 D1 | 理由要換 |
| 7 | E.8 取捨 1（:629） | 撤回的 `DeferredChanges` 對 throw 的規則（當對照） | 該列結論（採 Interrupted）有自己的理由「半個 stage 的狀態對 (c) 不是完整狀態」；對照句可刪 | 理由要換（刪對照） |
| 8 | F.7「套用依拓撲序、FIFO」列（:724） | entity-store DI 4、E-12 | 有：結果不依記錄順序（換 plugin 順序不會靜默改結果，該列自己的後半句）；同步點前參考不失效 | 理由要換 |
| 9 | F.8 取捨 1（:734） | 「延伸 entity-store DI 4」；「若使用者選 generation 模型，改回提案 2 的順序」 | 同 8；後半句在格式已含 generation 後沒說明是否觸發——PvP 策略下不觸發（id 在記錄時配好與格式無關） | 理由要換（刪 DI 4 引用、處理過時的後半句） |
| 10 | F.8 取捨 2（:735） | 撤回的 `DeferredChanges` 句 | 候選「避免 N 輪設定與連鎖」仍可主張，但有反證（§6.5）；Bevy 實際相反 | **結論可能要改**（已在架構師的 hook 重審中） |
| 11 | K.2 第 6 條（:1166） | entity-model RC-1、RC-3 | 同 6：標明為名指產品的規模預算 | 理由要換 |
| 12 | M 表 Archetype（:1309） | RC-3 | 同 3 | **未比較**（§2.3 方案 F） |
| 13 | networking.zh-Hant.md:14 | 「EntityStore 3b（RecordSpawn／Destroy，依 id 升冪）已設計」當現況 | 有：spec F 已規格化延後佇列 | 理由要換（改引 spec F；現況欄寫的是撤回的設計，事實錯誤） |
| 14 | target/world DI 2（:72）；W-9（:168） | 隱含 E-3（不重用） | 無（Kernel 層沒有要求不重用的 Feature） | **結論可能要改**（§3.3，C2） |
| 15（本審加） | #176 §1.1「需要它的 Feature」（:44） | 3b `DeferredChanges`；RC 6 change tick | 無（兩者都已撤回或被取代） | 未決（C7，併入 D5） |

連鎖（偵察 C；本審抽查 DI 4、F.7、DI-KS 9 三環成立）：E-12（PvP）→ survey「GYO 約束」→ entity-store DI 4 → spec F.7／F.8 → DI-KS 9。Bevy 原始碼在這條鏈上只當「不這樣做」的對照。

### 6.5 hook／observer flush：Bevy 的實際做法與 F.8 取捨 2

已回讀：
- hook 拿 `DeferredWorld`，`commands()` 推進 World 的命令佇列（`world/deferred_world.rs:76-79`）。
- 每條命令 `apply` 後立刻 `world.flush()`，註解說明是為了套用該命令新排入的命令（`world/command_queue.rs:123-131`）；`CommandQueue::apply` 開頭先 flush World 內部佇列（`:181-183`）。
- 直接的結構操作結束時也 flush（`world/entity_access/world_mut.rs:1086-1097`）；`World::flush`＝`flush_components`＋`flush_commands`（`world/mod.rs:3215-3218`）。

所以 Bevy 的語意是「**在觸發它的那次操作內、深度優先連鎖套用到清空**」，沒有輪數上限，也沒有「下一個 sync point」。F.8 取捨 2 寫的「與 entity-store 一致」引的是 `DeferredChanges` 的**不可重入斷言**（Apply 期間再記錄就 GYO_ASSERT，`entity-store.md:81-82`）——那是「不准這樣用」的規則，不是「延到下一窗」的設計，更不是 Bevy 的做法。

與 W-2／entity-store 的關係：兩種模型在 GYO 都能決定（hook 依 F.2 的階段與 id 執行）。架構師的 hook 重審（`build/evidence/kernel-hook-window-review/sources.md:13`，已回讀）指出的反證：快照不含 hook 佇列（spec :902），「下一窗」模型下 tick T 最後一個同步點的 (c) hook 記錄跨到 T＋1，Rewind／Restore 後不重現——碰到 DI-KS 12。Observer：M 表以「順序依 EntityHashMap」不做（:1307）；偵察 C C11 指出 `EntityHash` 是固定、無亂數種子的雜湊（決定但未規定），本審未回讀。本審不重複 hook 重審，只確認：**F.8 取捨 2 的現有理由不成立**，結論是否要改由那條重審決定（PR #218 commit `36612cb5` 待決）。

---

## 7. 結論（三類）

### 7.1 仍成立（附獨立理由、最強反方與回應）

| # | 結論 | 獨立理由 | 最強反方論點 | 回應 | 證據 |
|---|---|---|---|---|---|
| S1 | (a) 配發器是排程與快照看得到的 Resource（不藏在容器裡）；(b) PvP 下由產品給、World 不配 | (a) 配發順序必須是 Plan 的函數（重放、Rewind 要重現 id），所以配發狀態要進快照、進衝突分析；(b) 多個配發者（Gateway 段＋Match 段）＋Client 不配（E-2／E-3；成熟參照 Flecs entity range／`make_alive`，未查證） | Bevy 在容器內配發，單執行緒時也是決定的；(a) 用 Kernel 內建的配發器也滿足 | (a) 成立：Bevy 的重用依 LIFO＋128 格本地緩衝（`remote_allocator.rs:643-652`、:955-964），決定但依釋放歷史，要在回滾集裡就得是 Resource。(b) 的根據是產品條文，**不是**與 PvP 無關的獨立理由（敵對檢查第 8 項） | spec :95-98、:902；DI-KS 4；PR `detail/Storage.hpp:305-314` |
| S2 | **(c) 查詢順序是兩端共有契約鍵的函數**，(c) System 不必自己排序；以 EntityId 為序是 A.8 取捨 1（契約鍵＝World 鍵）之下的實現 | Delta-1（共用模擬核心）＋部分快照（§2.2b 條件 1）：依順序的 (c) System 要兩端同結果，順序必須由兩端共有的東西決定 | 今天 Client 只預測一個實體；(B)＋差分測試也能抓漏排序；H.5 的整欄替換本身是被審設計的一部分 | Delta-1 已核准、ClientSim 的 Simulate 明寫同一份 System（:877），屬基盤預見；差分測試只涵蓋走到的情境。前一版把「依 EntityId、只取決於成員集合」整句當成獨立結論，敵對檢查第 4 項指出第二條腿不獨立，已收窄；部分快照下 W-2 只保證共有實體的相對順序（必要不充分） | migration-plan :738；spec :877、:903、:212；§2.2、§2.2b |
| S3 | store 不執行任何東西（DI 5 的結論） | 使用者程式只由 Executor／World 派送；多一條路徑＝多一個順序契約 | 容器回呼（如 EnTT signal）是常見做法 | GYO 的 hook 已在 World 層（DI-KS 11），store 再有一條就是第二個派送點 | spec M :1307；DI-KS 11 |
| S4 | store 的狀態權威（DI 6） | AGENTS §3 | (C) 需要索引，算不算「store 的狀態」？ | (C) 的索引會在 World 的儲存裡，不是 entity_store；DI 6 對 store 本身照舊 | `store_stamp_guard.py`（PR #218） |
| S5 | 誤用是 Programmer Error（DI 7，store 層） | error-handling 規則 3、6 | Restore 讓 wire 資料走到 InsertEntity | 對 store 仍對（它不該分辨來源）；缺口在 Restore 的解碼驗證（C5） | entity-store.md:162-165；spec :98、:903 |
| S6 | 不認識產品的葉（DI 9）、鍵只有一種讀法（DI 10） | AGENTS §6、§9 | DI 10 的機制是為 PvP 的 `playerId` 過渡欄設計的 | 觸發來自 PvP，但「consumer 命名不得成為 Engine 知識」的理由與 PvP 無關；`Cell<T>` 仍經主模板讀 `id` | entity-store.md:170-178 |
| S7 | 結構變更延後（同步點前持有的參考不失效）；同步點依「階段＋id」套用（DI-KS 9 的主體，不含 hook 那句） | 「延後」：C++ vector 的參考失效規則（FIFO 同樣滿足）。「依 id」：Despawn 不產生衝突邊（K.3 第 4 項，spec :1180），所以 Plan 不強制記錄 Despawn 的 System 之間的順序；依 Plan 位置的 FIFO 會讓 despawn 與 OnRemove hook 的順序隨 plugin 順序改變 | Bevy 依 system index 的 FIFO 在同一 Plan 下也是決定的；D-E8 已強制有衝突的 System 寫順序 | D-E8 只管有衝突邊的 System，Despawn 沒有衝突邊，所以 FIFO 下的順序不受任何檢查約束；依 id 讓結果與 plugin 順序無關。PR #218 已實作並有突變守衛（敵對檢查第 18 項後寫出機制） | spec F.2 :651-661、F.7 :724、:1180 |
| S8 | PvP：玩家段由 Gateway 配、Match 段單調不重用、generation 恆 0（產品策略） | 規模（340 萬 ≪ Match 段 2.5 億）＋wire 4 位元組 varint＋Gateway 不持對照表（AGENTS §3） | 槽回收＋generation 是業界標準（Bevy、EnTT、Flecs） | 沒有 Feature 需要回收；回收要讓 generation 上 wire 並改 digest。若日後需要（RC-2），格式已容得下 | entity-model :81-83、§2.3；問卷 1-4 |
（前一版的 S9「排序欄 (A) 作為目前的實作，生滅已實測在預算內」移到 §7.3 C8：BU-2 量到的比前一版說的窄，查詢疊代完全沒量，敵對檢查第 3 項。）

### 7.2 理由要換、結論不變（寫出新理由）

（R1 DI 2／DI-KS 3、R2 DI 3、R9 #176 在敵對檢查第 1、2 項後移到 §7.3 的 C6、C7：它們的結論範圍會變或還沒判，不是「結論不變」。編號保留空缺，以便對照前一版。）

| # | 對象 | 舊理由 | 新理由 | 影響的文字 |
|---|---|---|---|---|
| R3 | DI 4（store 半條） | `DeferredChanges`、E-12 | DI 2 的重述；World 半條見 S7 | entity-store.md:146-150 |
| R4 | DI 5 | 舊 architecture.md 禁令 | S3 | entity-store.md:151-156 |
| R5 | DI 8 | PvP digest；`DeferredChanges` 括號 | Kernel W-10／DI-KS 3、重放與等價 host | entity-store.md:166-169 |
| R6 | DI 1（store 半條） | E-2、E-3 | S1；「無 generation bits」改為「不解讀 generation bits」 | entity-store.md:130-136 |
| R7 | spec A.3 第 4 點、H.3 末句 | 「因為 id 是契約鍵（E-2）」 | 條件句：「若產品讓兩端持有同一組 id」 | spec :147、:848 |
| R8 | spec A.8 取捨 1 | (2)「四處 Delta 的成本」；(3)「不重用讓 generation 沒用途」 | 刪 (2)；(3) 改為「選了不重用，所以不需要 generation 偵測 stale 引用」（§3.4） | spec :212 |
| R10 | spec A.7 第 4 列、M 表 Archetype、K.2 第 6 條、A.6 門檻 | RC-3、RC-1（PvP 的值）當 Kernel 理由 | AGENTS §2 根據 3：「名指產品的已知規模」，明寫是 PvP 的值 | spec :203、:1309、:1166、:176 |
| R11 | spec F.7、F.8 取捨 1、E.8 取捨 1 | entity-store DI 4、E-12、撤回的 throw 規則 | S7 的理由；刪對照句；處理 F.8 取捨 1 過時的後半句 | spec :724、:734、:629 |
| R12 | networking.zh-Hant.md:14「GYO 現況」 | EntityStore 3b 已設計 | 改引 spec F | networking :14 |
| R13 | 「照 Bevy 的格式」 | 照 Bevy | 「index 在低 32、generation 在高 32（欄位位置同 Bevy；Bevy 低位存 `!index`、同代內排序方向相反、且不保證位元結構）；GYO 的順序是 u64 數值升冪」 | spec :86；target/world §1 :10；DI-KS 4；PR #218 `Storage.hpp` 註解 |
| R14 | spec A.7 第 2、8 列的「未查證」 | — | 改引 survey §3a 與 Bevy 原始碼（§3.2、§6.3） | spec :201、:208 |
| R15 | spec A.1 :87、A.8 取捨 1 末句 | 「槽回收→兩端配發歷史不同時，數值順序不再是契約鍵順序」 | 兩端同序靠「Client 不配發」，與 Match 是否回收無關（§3.4）；要寫成「兩端都配發時」 | spec :87、:212；target/world §1 |
| R16 | migration-plan Delta-3 標題 | 「使用者已同意，K.3 第 7 項第 (2) 點」 | 改為「隨遷移計畫本體核准（2026-11-13，`v8-arch-migration-plan-approved`）；K.3 第 7 項第 (2) 點只涵蓋 DI 4 延伸、DI 5、`DeferredChanges` 撤回」（§5.1） | migration-plan :758 |

### 7.3 結論可能要改（附代價與影響範圍）

| # | 結論 | 為什麼可能要改 | 改的代價與影響範圍 |
|---|---|---|---|
| C1 | F.8 取捨 2「hook 的變更進下一窗」 | 理由引撤回的句子；Bevy 相反；快照不含 hook 佇列 → 回滾不重現（hook 重審 sources.md:13） | spec F.2 第 0 步、F.8、A.2 表「hook（Applying，記到下一窗）」、DI-KS 9 末句「hook 的變更屬下一窗」；PR #218 commit `36612cb5`；J-32／J-34 系守衛。**已由架構師的 hook 重審處理，本審只確認理由不成立** |
| C2 | W-9 與 target/world DI 2 的「不重用」 | 把 PvP 策略寫成 Kernel 條文，與 world §1、DI-KS 4 矛盾（§3.3） | spec W-9、J-8（只打 `RangeAllotter`，可不動）、A.7 :201、M 表 :1310；target/world DI 2 標題；PR #218 程式不受影響（已是 concept＋預設實作）。改 DI 要使用者同意 |
| C3 | Delta-3：`engine/entity_store` 成為 World 欄的實作 | 主理由「避免重複」在 PR #218 未達成（`entities_`、`Fresh` 手寫）；Feature 欄寫的是舊模組的不變式；佈局以「不動舊模組」選定；(C)、(F) 未進比較；同意的出處標錯，實際隨遷移計畫本體一起核准、沒有單獨被問過（§5） | migration-plan §4 Delta-3；Delta-1 第 5 項的「＋`GYO::EntityStore`」；`kernel_guard.py` K5 白名單（§14.4 核准過）；`store_stamp_guard.py`；`tests/common/kernel/CMakeLists.txt:222` 的 `gyo_expect_links`；target/world §6；entity-store.md（legacy）的去留。PvP 遷移前 PvP 仍用 store，任何變動都排在 PvP 欄遷入 World 之後才有意義 |
| C4 | Client 預測生成：Client World 無配發器 × 共用 FixedUpdate | AI 批的生成 System 進 FixedUpdate 時，D-E3 與「同一份 System」（cFingerprint）互相矛盾；kernel 線沒有預測生成或對映的條文（§3.5） | spec A.1 :95-98、H.3、H.4 ClientSim 列、H-4；entity-model E-3、RC-5；networking（N1～N12 無對映、無預生成）；可能新增 Kernel 概念（預測配發器或 Signature 式配對）。屬基盤預見，應在 AI 批前定 |
| C5 | Restore 的外部快照沒有鍵的驗證條文 | 解碼後重複或亂序的 id：逐筆插入時撞上 store 的 GYO_ASSERT，整欄替換且驗證關著時靜默破壞 W-1（推論，Restore 尚未實作）；與 spec 對 `SpawnWithId` 外部重複 id 的處理（Runtime Error）不一致。敵對檢查重掃，spec 同樣沒有 Restore 的鍵驗證條文 | spec H.5（加一條：Host 解碼後驗證嚴格升冪、無重複，違反＝Runtime Error 並丟棄該快照）；rollback-extract 批的守衛 |
| C6 | entity-store DI 2／DI-KS 3／W-1「每個欄依 EntityId 嚴格升冪」是不變式（含 DI 3「無側表」） | Feature 只支持「(c) 查詢順序是兩端共有契約鍵的函數」（§2.2）；容器順序是從 `std::map` 傳下來的形狀（§5.3）。換掉理由之後結論的範圍要縮小 | DI-KS 3（2026-11-13 才核准）、spec W-1、A.3、A.6、J-1（Design Invariants 的守衛，改判定走 AGENTS §14.4：1～5、使用者核准、敵對檢查）、entity-store DI 2／3 的引用。程式（PR #218）在 D1 的任何選項下都不必改。見 D1 |
| C7 | #176「自製（維持）」→ World 儲存自寫 | 判準是被比較者自己的 DI（循環）；「需要它的 Feature」欄兩項已撤回；使用者 2026-11-11 決定的 EnTT 重判沒做；新理由（adapter 要露出位元組驗證、Restore、stamp；EnTT sparse 頁表隨最大 id 增長）一半未查證、一半是未量的推論 | evaluation.zh-Hant.md §1.1；kernel HANDOFF:47；migration-plan :77、:766；spec M :1311。見 D5 |
| C8 | 排序欄 (A) 作為目前的實作「在預算內」 | BU-2 只量 8 位元組值、4＋4 生滅、整次 Run、本機 Intel Mac、「recorded, not judged」；保序插刪隨值大小線性放大；**查詢疊代沒量**，粗估在 4,000 實體可能到數 ms（推論） | RC-1（4,000 實體、1 ms p95）的判定；PR #218 的查詢實作（`Params.hpp`）；方案 (C)、(F) 的比較。見 D5 |
| C9 | A.8 取捨 1／K.3 第 1 項「契約鍵＝World 鍵（World 不配鍵、產品的 id 就是 Entity）」 | 刪掉改文件成本 (2)、反轉 (3) 後只剩 (1)「每個 System 的負擔」，而 (1) 被 (C)×(E)（Kernel 依契約鍵元件排序）化解；剩下的支撐只有 E-1／E-9 與使用者的決定 | spec A.1、A.8、DI-KS 4；target/world §1；entity-model E-1／E-2／E-3／E-9、RC-5；digest 與 wire 的 id 語意；與 C4（預測生成）連動。**這是使用者 2026-11-11 的決定，本審不建議推翻，只指出它的支撐是產品條文**。見 D4、D10 |

---

## 未查證清單

| 項目 | 狀態 |
|---|---|
| EnTT 的全部事實（sparse 頁表增長、`sort_as`、group、`create(hint)`、`continuous_loader`） | 未查證（本機無原始碼；只經 survey、recon-01、對抗檢查 A23） |
| Flecs 的 `order_by`、`make_alive`、entity range | 未查證（本機無原始碼；只經 entity-model §1） |
| PR #218 查詢疊代在 400／4,000 實體的成本（BU-2 的 System 只取 Query、不疊代） | 未量測；§2.3 的「數十 µs」是推論 |
| (B)、(C) 的任何成本 | 推論，未量測 |
| 「非排序儲存在同一歷史下決定」 | 推論（由 `swap_remove` 的程式結構）；archetype 依建立順序編號只依偵察 B §3，未回讀 |
| lightyear 回滾刪除並重生 PreSpawned；replicon client 端對未見 entity 佔位生成（`client.rs:679-701`） | 偵察 B／C，未回讀（`ServerEntityMap` 結構與 lib.rs 的說明已回讀） |
| Bevy `git grep determinis` 的全部命中、`par_iter` 無排序版、`EntityHash` 固定雜湊 | 偵察 B §5-§6、偵察 C C11，未重跑 |
| Bevy 位元穩定性警告的 doc（`entity/mod.rs:374-383`） | 偵察 B，未回讀（`to_bits` 的「No particular structure」已回讀） |
| `ResolveLifeBoundaries` 的重生順序（`PvpMatch.cpp:271-273`）是否依賴容器序 | 未查證（只讀到 entity-model :112 的描述） |
| #176 對抗檢查報告（`adversarial-check-20261110.md`）與 recon-01 | 偵察 A，未回讀 |
| `evaluation.zh-Hant.md:362`「沒有試建、沒有量測」 | 偵察 A，未回讀 |
| `kernel_guard.py:100-103` 的 K5 白名單行號 | 偵察 A，未回讀（白名單存在由 PR #218 dev_log 確認） |
| 本機 BU-2 數字在 Linux 容器（U8 的閘門機器）上是否相近 | 未量測（宣告自己寫明只當參考） |

---

## 要使用者決定的事

**D1. DI-KS 3／W-1：容器順序是不變式，還是實作？**（C6、S2、C8）
- (a) 維持：每欄依 id 升冪（W-1）與查詢順序（W-2）都是 DI-KS 3 的不變式；只換理由文字。取捨：最省事；將來若要 (C2) 或 (F) 就是改 DI（Architecture Delta）。
- (a') 維持 DI 不動，只在既有的置換條件（K.2 第 6 條、target/kernel :95「換儲存，DI-KS 3 的順序不變」）寫明「不變的是**查詢順序**；(C1) 有序鍵視圖下 keys 仍升冪、W-1 字面照樣成立」。取捨：改動最小，不碰 DI 守衛；但 (C2)、(F) 仍要改 DI。
- (b) 拆開：W-2（(c) 查詢順序是契約鍵的函數、(c) 區只有有序查詢）是不變式；W-1 降為目前的實作，寫 Replacement Criteria。取捨：要改 2026-11-13 才核准的 DI-KS 3（Architecture Delta）與它的守衛 J-1——AGENTS §14.4 規定 DI 守衛的判定變更不論鬆緊都要 1～5、使用者核准，並先做突變與別的檢證者檢證；再加 spec A.3、A.6、W-1、entity-store DI 2 的引用。好處是 Feature 根據與條文一致。
- **建議**：先 (a')，等 D5 的量測結果再決定要不要 (b)。理由：§2 顯示 Feature 只支持 W-2，但目前沒有任何量測顯示 (A) 不夠，(b) 的手續成本是確定的、好處要等換儲存時才兌現。PR #218 的程式在三案下都不用改。

**D2. Delta-3 的去留**（C3）
- (a) 保留，只換理由（C6、C7 的新理由）並記錄三處繞過。取捨：零程式改動；Kernel 繼續付 K5 白名單、`store_stamp_guard`、多一條依賴邊。
- (b) PvP 欄遷入 World 後，把約 60 行的演算法併入 `engine/kernel`（由 Kernel 一份實作同時服務 `entities_` 與欄），`engine/entity_store` 與其 legacy DI 退役。取捨：一次小改動加守衛與突變的搬移；去掉為舊模組付的守衛代價與一條邊。
- (b') **現在**就由 Kernel 自持欄的演算法（PR #218 合併前或下一批），`engine/entity_store` 只留給尚未遷移的 PvP，PvP 遷移後退役。取捨：在 loading、rollback-extract 等批疊上去之前拿掉新邊 Kernel→EntityStore、K5 白名單放寬、`store_stamp_guard`、為了舊容器多加的「nothrow move 指派」約束（W-8 要改）；代價是 PR #218 要再改一輪、Engine 暫時有兩份同一演算法（遷移期內）。
- (c) 等 D1 與 RC-1 的查詢量測（D5）之後再定。
- 時機的提醒（敵對檢查第 10 項）：AGENTS §2 要求基盤的反覆在往上疊之前完成；PR #218 還沒合併，以上四件都還沒落地。等到「PvP 遷移之後」，中間的批都會疊在這條邊上。停或續由使用者決定（D9）。
- **建議**：(b') 或 (c)，看 D9。前一版建議 (b)「排在 PvP 遷移之後」，敵對檢查指出那偏向現狀，已撤回該建議。若 D1 選 (a)，D2 (a) 也可接受。

**D3. W-9／target/world DI 2 的「不重用」，以及 generation 的預見根據**（C2、R13、§3.1）
- (a) Kernel 層改為「World 不配發；配發器策略由產品宣告；Kernel 只附一個單調的 `RangeAllotter`」，「單調不重用」移到 PvP 產品條文；同時補寫 generation 的預見根據（名指 Feature 或規模，AGENTS §2）。
- (b) Kernel 禁止重用：刪 world §1 與 spec :87 的「別的產品可選槽回收」；generation 位元變成沒有用途（要說明為何保留）。
- (c) 「不重用」定義為 u64 **值**不重用（允許槽回收＋generation＋1），刪「單調」。
- 取捨：(a) 與已核准的 DI-KS 4 一致、改動最小；(b) 收窄通用性；(c) 保留通用性但要多一條「generation 不回繞」的條件。

**D4. Client 預測生成與共用 FixedUpdate 的矛盾**（C4）
- (a) 生成實體的 System 只在 Match（放在 Client 不登記的 stage 或 schedule），Client 的成員變化一律經 Restore；cFingerprint 的範圍要排除它們。取捨：最簡單；Client 看到生成會晚一個權威快照，投射物等靠「生成事件＋純軌跡」的呈現（問卷 1-5 的方向）。
- (b) Client 有一個「預測配發器」（獨立的暫時 id 段），預測實體以內容雜湊與權威實體配對（lightyear `PreSpawned`／replicon `Signature` 的做法）。取捨：要新的 Kernel 概念與配對規則；E-3 要改。
- (c) 兩個 id 空間＋對映（方案 E）。取捨：改 E-1、E-9、digest 與 wire 的 id 語意，範圍最大；觸發既有的置換條件 entity-store RC 11、entity-model RC-5、target/world 的 RC「需要第二個鍵空間的對照表」（:83）。
- (d) Client 也有配發器，配發器狀態隨權威快照 Restore（H.5 的快照本來就含配發器，spec :902）。取捨：E-3 要改；部分資訊下預測的配號可能與 Match 不同，要定不同時的處置。
- (e) 可推導的 id：由（生成者 id、tick 或 ActionId、序號）算出，兩端都用 `SpawnWithId`，不需要配發器也不需要對照表。E-3 已有「（自己的 id, ActionId）當暫時鍵」的雛形（entity-model :273）。取捨：id 寬度可能超過 wire 的 4 位元組 varint；碰撞規則要定。
- 時機：AI 批（第一個在 tick 內生成實體的批）之前。與 D10 連動。

**D5. #176 的重判與 RC-1 的查詢量測怎麼收尾**（C7、C8、§6.4 第 15 列）
- (a) 正式結案：寫入「World 儲存自寫」與 §4 的新理由，並註記「未做 2026-11-11 決定的 EnTT 核對，理由如下」。取捨：不花時間；C8 的「在預算內」仍無證據。
- (b) 做一個小 spike（在 Linux 容器量，照 `v8-arch-u8-gate-in-linux-container`）：同一個 BU-2 形狀但**含查詢疊代**、值大小至少兩種（8 B 與實際元件大小），比較 `Cell<T>`（現況）、leapfrog、(C1) 有序鍵視圖、(F) 每組合一張有序表、EnTT storage＋Kernel 有序查詢。取捨：數小時到一天；得到 RC-1 的實測，也回答 D1、D2，並完成 2026-11-11 的決定。
- (b-) 同 (b) 但不含 EnTT。**注意：這等於偏離使用者 2026-11-11 的決定**（kernel HANDOFF:47），要使用者明示同意偏離。
- (c) 不處理。
- **建議**：(b)。前一版建議 (b-)，敵對檢查指出那等於再延一次使用者已決定要做的核對而沒有說明，已改。量測屬重的本機執行，開工前照慣例先說明影響。

**D6. 文字修正批**（R3～R8、R10～R16，不涉及上述決定的部分）
- 由誰在哪一批做（架構師的 spec 修訂，或圖書管理員的文件整理）；DI 的理由文字變更是否視為 DI 變更（AGENTS §3：節的變更要同意）——審查者的理解：只換理由、不改條文要求，仍屬 DI 節的變更，宜一次性同意。

**D7. Restore 外部快照的鍵驗證**（C5）
- 是否在 spec H.5 加「Host 解碼後驗證嚴格升冪、無重複；違反＝Runtime Error、丟棄該快照」，並排進 rollback-extract 批的守衛。

**D8. PR #218（`v8-arch-kernel-world-exec`，未合併）是否照現狀合併**（敵對檢查第 10 項新增）
- (a) 照現狀合併，C3／C6／C8 的處置排到之後的批。取捨：不打斷 world-exec；但新邊、K5 白名單、`store_stamp_guard`、nothrow move 指派的約束會落地並被後續批依賴。
- (b) 合併前先處置 D2（b'）或至少 D5 的量測。取捨：world-exec 延後；基盤的反覆在疊上去之前完成（AGENTS §2）。
- (c) 合併，但在 HANDOFF 記一行「Delta-3 待重審，後續批不得新增對 entity_store 的依賴」。取捨：折衷；要有人守這一行。
- 注意：PR #218 另有 hook 窗口（C1，commit `36612cb5`）待架構師的 hook 重審決定，與本項同時看。

**D9. 哪些後續批被 D1／D2／D4／D7 卡住，要不要先停**（敵對檢查第 10 項新增）
- 受影響的候選（依本審的影響範圍，未逐批查宣告）：rollback-extract（D7 的 Restore 鍵驗證、C6 的位元組比較）、loading（成員生成路徑）、AI 批與 pv10（D4 預測生成）、PvP 遷移批 match-sim／match-host（D2 的時機、C6 的 E-5 守衛改寫）。
- 選項：(a) 照計畫進行，決定項各自在相關批的事前宣告前定；(b) 先停，等 D1／D2／D4 定完；(c) 只停 AI 批（D4 是它的前提），其餘照計畫。
- 停或續由使用者決定（AGENTS §2〈反覆を計畫する條件〉末段）。本審不建議。

**D10. 契約鍵＝World 鍵（A.8 取捨 1／K.3 第 1 項）是否維持**（C9，敵對檢查第 6 項新增）
- (a) 維持：理由寫成「產品條文 E-1／E-9（免對照表、id 直接進 wire 與 digest）＋使用者 2026-11-11 的決定」，刪掉「改四處 Delta」的成本理由。
- (b) 重開：World 自己配 Entity，契約 id 是元件，Kernel 依契約鍵元件提供有序查詢（§2.3 (C)×(E)）。取捨：E-1／E-9、digest、wire、DI-KS 4 都要改；好處是預測生成（D4）與槽回收不再碰到「兩端同一個 id」的約束。
- **建議**：(a)。目前沒有 Feature 需要 (b)；但若 D4 選 (c)，(b) 會一起被觸發，兩項要一起看。

（C1／F.8 取捨 2 已在架構師的 hook 重審中，不在此重複列為新的決定。）

---

## 敵對檢查處置（Fable／high，一次，只讀；全文 `build/evidence/entity-store-rereview/adversarial-fable.md`）

檢查者的整體判斷：「不能照現狀交使用者，修正後可以」。Bevy／lightyear／replicon 的原始碼語意全部核對正確，問題集中在分類偏向「理由要換、結論不變」、S2 的獨立性、S9 的實測範圍、選項與決定項有缺。處置者：卡片主會話（Opus）。第 2、10、12 項的關鍵事實由處置者親自回讀確認（kernel HANDOFF:47、:68；migration-plan :77、:758；spec :1180-1186；Bevy `entity/mod.rs:490-500、:554-566、:1389-1391`；entity-requirements :37）。

| # | 嚴重度 | 發現（要點） | 處置 |
|---|---|---|---|
| 1 | 必改 | R1、R2 放在「結論不變」，與 §1 小結自相矛盾 | 採。移到 §7.3 C6；§1 小結改寫 |
| 2 | 必改 | R9 把未做的重判列為「結論不變」；D5 的建議跳過 EnTT＝偏離 2026-11-11 的決定 | 採。改列 C7；§4 補引 HANDOFF:47、mp:77；D5 加 (b-) 並明寫偏離，建議改為 (b) |
| 3 | 必改 | S9 的實測說過頭：值 8 B、形狀窄、本機、查詢疊代沒量 | 採。S9 移到 C8；§2.3 補 BU-2 的範圍；刪「已實測在預算內」 |
| 4 | 必改 | S2 的第二條腿（H.5 整欄替換）不獨立；W-2 只是必要不充分；(B) 出局的條件沒寫 | 採。S2 收窄為「(c) 查詢順序是契約鍵的函數」；新增 §2.2b |
| 5 | 應改 | 缺 `std::map` 起點的系譜 | 採。新增 §5.3 |
| 6 | 應改 | A.8 取捨 1 判「理由要換」過輕；漏 (C)×(E) | 採。§6.4 第 4 列改判、新增 C9、D10；§2.3 補方案 |
| 7 | 應改 | 漏「每組合一張有序表＋k 路合併」；archetype 的否決理由不成立 | 採。§2.3 補方案 (F)；§6.4 第 3、12 列改「未比較」 |
| 8 | 應改 | S1「與 PvP 無關」過度 | 採。S1 拆成 (a)(b)；§1 DI 1 列改寫 |
| 9 | 應改 | D4 漏 (d)(e)、漏引 RC；問卷 1-5 行號與轉述錯 | 採。D4 補選項與 RC；更正為 :37 與原文 |
| 10 | 應改 | D2 建議偏向現狀；漏 (b')；漏「PR #218 是否照現狀合併」「哪些批要停」；Delta-3 同意出處 | 採。D2 補 (b')、撤回原建議；新增 D8、D9；處置者查證同意出處（§5.1、R16） |
| 11 | 應改 | D1 (b) 成本低估（§14.4）；(C) 有兩種；(C) 對位元組比較的代價漏寫；(B) 的 Rewind 偏嚴 | 採。D1 重寫並加 (a')、建議改為先 (a')；§2.3 補 (C1)(C2) 與 Rewind 列；§2.4 補位元組層 |
| 12 | 應改 | §4 掃描的正例不是同一支；Bevy 行號三處偏移 | 採。處置者以 Python 重跑（同表）並更正；行號更正 |
| 13 | 建議 | C5「abort」是對未實作 Restore 的猜測 | 採。標推論，寫出兩種情況 |
| 14 | 建議 | 「用不上 store」不準 | 採。改「沒有用」 |
| 15 | 建議 | `ForEach` 的描述錯 | 採 |
| 16 | 建議 | spec :87、:212 末句混淆回收與雙方配發 | 採。§3.4 補、新增 R15 |
| 17 | 建議 | #176 把沉沒成本算進代價；漏 in_place | 採。§4 表補兩列 |
| 18 | 建議 | S7 的第一個理由不支持「依 id」 | 採。S7 寫出 Despawn 無衝突邊的機制 |

沒有不採的項目。檢查者自述沒檢查到的範圍：EnTT、Flecs（本機無原始碼）、recon-a 的 commit 層事實、entity-store survey 本文、`bevy_core.md`、PR #218 的測試與 `mutations.json` 內容；第 3 項的查詢成本估算同樣是推論。

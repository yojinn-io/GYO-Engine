# 事前宣告：`v8-arch-spec-v05`（Kernel 規格 v0.5 修訂與遷移計畫的後續待決）

更新：2026-11-13。Owner：架構（架構師會話開出的任務卡 05）。**狀態：宣告寫完，等使用者核准與 A 各題的決定；未動 spec 與計畫。**

## 目的

把三處留下的待決與機械性錯誤收掉：

- registry-schedule（`v8-arch-kernel-registry-schedule`，PR #213 已合併）回報給架構師的 spec 問題：配發器的獨占（審查 C5）、資料流邊的定義（宣告修訂 6）、錯誤代號重疊、D7 兩個空白。出處：[registry-schedule dev_log](../../../dev_logs/2026_11_13_arch_kernel_registry_schedule.zh-Hant.md)〈給架構師〉、[事前宣告](registry-schedule-declaration.zh-Hant.md)〈核定後的修訂〉第 6、12 點。
- 遷移規劃稿（`v8-arch-adapter-plugins-plan`，PR #212 已合併）最終檢查交主會話的三條：S-9、S-12、S-14。出處：[adapter-plugins-plan.zh-Hant.md](adapter-plugins-plan.zh-Hant.md) 文末〈最終檢查處置記錄〉。
- spec 裡與 #212 決定不一致、已過時的句子。

A 是要使用者定的；B 照已定事實改，不需使用者決定（但 B-1 的方向以實作為準，若使用者不同意請指出）。本宣告不替使用者選，每題附建議。

---

## A. 要使用者定的

### A-1　兩個會生成實體的 System，算「要排序的衝突」還是「第二寫者錯誤」（審查 C5）

**現況的矛盾**

- spec A.1（`Spawn<Bundle<…>>` 那一條）與 W-9、B 節的 Bevy 對照表、附錄一 X15 都說：`Spawn` 推出 `write(EntityAllotter)`，所以兩個生成者「一定有資料衝突」，(c) 沒寫順序＝歧義錯誤 D-E8，(b) 依登記序平手；「配發序列化是存取契約推出來的結果」。
- 但配發器是普通 Resource，預設 Exclusive（`v8-arch-kernel-exclusive-default`）。實作把 `Spawn` 推出的配發器寫**計入**獨占寫者數，所以第二個 `Spawn` System＝D-E2（第二寫者），根本走不到「排序」。
- spec B.3 與 `target/kernel.zh-Hant.md` DI-KS 6 的寫者清單**字面上沒有列「配發器寫」**：列的是 `Write`／`ResMut`、型別化 Commands、`Spawn`／`SpawnWithId` 的 **bundle**、hook。所以實作比 spec 字面嚴。DI-KS 4 只說「配發器是產品的 Resource」，不涉及這題。
- registry-schedule 為了隔開這題，J-15 的 Spawn 案例把配發器設 Shared（測試加註）；world-exec 沿用現狀。PvP 的 `ApplyMembership` 用 `SpawnWithId`（不經配發器），卡 01 的 plan-dryrun 不受影響。

**選項**

| | A（照實作，嚴） | B（照 spec 字面，寬）**建議** |
|---|---|---|
| 規則 | `Spawn` 推出的配發器寫計入獨占寫者。預設下一個 World 只有一個 `Spawn` System；多個生成者要嘛走漏斗（發事件給唯一的生成者），要嘛產品把配發器登記為 Shared，之後由衝突序列化 | `Spawn` 推出的配發器寫**不計入**獨占寫者數，但照樣產生衝突邊：(c) 要顯式順序（否則 D-E8）、(b) 依登記序平手。`ResMut<EntityAllotter>`（直接改配發器）仍計入 |
| 決定性 | 一樣：配發順序都是 Plan 的函數 | 一樣：(c) 的兩個生成者必須寫順序 |
| 要改的文件 | spec A.1、W-9、B 節對照表、F 節對照表、K.1 第 1 條摘要、附錄一 X15 改寫；**DI-KS 6 寫者清單加「`Spawn` 推出的配發器寫」**（Design Invariants 文字變更） | spec B.3 寫者清單加一句「`Spawn` 推出的配發器寫不算寫者（理由：配發是 Kernel 定了合併規則的服務——依 Plan 順序遞增）」；DI-KS 6 條文不變，可加同一句釐清（屬 DI 文字，算在本題的同意裡） |
| 要改的程式（不在本批） | 無 | Registry 的獨占計數排除 `Spawn` 推出的配發器寫；J-15 是 DI-KS 6 的守衛測試，判定**變寬**（兩個有順序的 `Spawn` 從 D-E2 變成通過）→ 實作批照 AGENTS §14.4 寫 1～5、做突變與驗證者檢查、取得使用者核准 |
| 代價 | 每個新的生成來源（投射物、掉落物、特效）都要走漏斗，或用 Shared 出口；Shared 出口的前提是配發器不在 digestSet（S-12），**PvP 的配發器在不在 digestSet 未查證** | 程式與守衛各改一次；Exclusive 的語義多一個明列的例外 |

**建議 B 的理由**：Exclusive 要守的是「同一欄的值只有一個權威、合併規則寫在一處」（B.3.1）。配發器的合併規則是 Kernel 固定的（依 Plan 順序遞增、不回繞），沒有產品可以選的合併規則，所以多一個生成者不產生第二個權威。B 也是 spec 原本推導的結論（A.1 那一條的最後一句）。使用者 #212 複核時說過「規矩太多、做功能前一堆規矩要守」，A 會讓每個新生成來源先繞一次漏斗或 Shared。A 的優點是不動程式與守衛。

**證據位置**：spec A.1、B.3、W-9、X15；`target/kernel.zh-Hant.md` DI-KS 4、6；`engine/kernel/src/Registry.cpp` 的 allotter 寫入推導（`data.allotter` 兩處）；registry-schedule 宣告修訂 12。

---

### A-2　資料流邊的定義要不要擴充（registry-schedule 宣告修訂 6；遷移計畫 §2.1 G1 (ii)）

**現況**：G1 (ii)「為消 D-E8 加的順序邊中，非資料流邊 ≤50%」。資料流邊 u→v 以計畫字面定義：`writes(u) ∩ (reads(v) ∪ writes(v)) ≠ ∅`，只算**立即**寫與**立即**讀。實作另印一個擴充定義的比例 `non_dataflow_edges_extended`（把 u 的延後寫、v 的 With／Without 也算進去），只印不判。

**機制上要知道的三件事**

1. **擴充只會讓比例變低**（擴充定義的兩個集合都包含字面定義的集合）。所以卡 01 照字面判**成立**時，換定義不改變結論；只有字面**不成立**時才有差。
2. **同一窗內，延後寫對讀者不是資料流**：延後寫在同步點才落地，同窗的讀者（含 With／Without）不論排在前後都看不到（spec D.1 第 4 條）。所以「u 的延後寫 → 同窗 v 的 With<T>」加的順序邊沒有帶資料，擴充定義會把它錯算成資料流。隔著同步點時（u 在前一個 stage），v 看得到 u 落地的結果，那才是資料流。
3. **同窗的延後寫對延後寫（同一 ComponentId）是真的資料依賴**：兩筆 insert 的勝負依 `(planPos, recSeq)`（F.2），順序邊決定誰贏。字面定義把它算成非資料流。

**選項**

| | 內容 | 影響 |
|---|---|---|
| (a) 維持字面（現行） | 不改 | 較嚴；第 3 點的邊被算成非資料流 |
| (b) 改用實作現有的擴充定義 | 判定改讀 `non_dataflow_edges_extended`（實作：`Written(u) ∩ (reads(v) ∪ With／Without(v) ∪ Written(v))`，`Written`＝立即寫＋延後寫） | 較寬；第 3 點算對了，但第 2 點的邊、以及「u 的立即寫對 v 的延後寫」（spec D.1 第 4 條：順序無關）也被算成資料流 |
| (c) 照機制精確定義 **建議** | 資料流邊＝字面定義，**加上**：u 的延後寫 ∩ v 的延後寫（同一 ComponentId）；u 的延後寫 ∩ v 的立即讀或 With／Without（**只在 u、v 之間隔著同步點時**） | 介於 (a)、(b) 之間；要改 Scheduler 的比例計算（程式，不在本批）與遷移計畫 §2.1 的條文 |

**與卡 01 的關係（卡片要求寫明，不追溯）**：卡 01 的 plan-dryrun 照現行字面判 G1 (ii)，不改。若選 (b) 或 (c)，新定義從「實作它的 Kernel 批合併之後」起，用於 G1 的重判（若有）與之後的 Plan 匯出；卡 01 已有的結果不重算。**注意 AGENTS §14.1**：受理判定在看結果後不得放寬。若卡 01 已經看到字面不成立的結果，再改定義（放寬）就不能用來改寫卡 01 的判定，只能作為之後的定義。這題最好在卡 01 出結果前定。

**證據位置**：遷移計畫 §2.1 G1；`engine/kernel/include/engine/kernel/Plan.hpp`（`DeclaredEdge` 的註解）、`engine/kernel/src/Scheduler.cpp`（`dataflow`／`extended` 的計算）；spec D.1 第 4 條、F.2。

---

### A-3　遷移計畫的後續三條（規劃稿最終檢查交主會話）

S-9 與 S-12 互相牽動，一起看。

**S-9：json-adapter 若在 G2b 判定前合併，會混進 G2b 的 BU-4／BU-6 比較**

- 事實：G2b 的 BU-4（穩態 RSS）、BU-6（真實行程到 ready）分母是 pvp-baseline 在舊程式（nlohmann）上量的。json-adapter 的位置是 G2a 後、adapters-relocate 前；它若先合併、Match 線 rebase，G2b 量到的就是「Kernel＋yyjson」對「舊路徑＋nlohmann」。調查報告 §2.6：repo 最大的 `biome.voxel_mesh.json` 解析 nlohmann 41.2 ms、yyjson 3.8 ms；BU-6 的容許是基線＋100 ms 或 ×1.10。JSON 變快可能蓋掉同量級的 Kernel 載入成本。
- 選項：
  - (a) json-adapter 移到 G2b 判定之後合併（動排程）。若保留 S-12 的串行，adapters-relocate 與其後的 2' 階段鏈跟著延後。
  - (b) **建議**：分母重量——json-adapter 合併後、match-host 刪舊路徑之前，用 pvp-baseline 同一腳本在「舊程式＋新 JSON」上重量 BU-4／BU-6 的分母，G2b 以重量值為分母（動已核准的閘門定義；必須在看 G2b 結果前定與量）。代價：一次基線重量（推定 2～4 h），且有時間窗限制（舊路徑還在時）。
  - (c) 分母不動，量測時把 JSON 解析時間另列，比較扣除它之後的值（也動閘門定義；JSON 解析時間要能從載入時間中可靠地分離，未查證）。
- 建議 (b) 的理由：閘門要比的是 Kernel 的成本，(b) 讓兩邊的 JSON 路徑相同，比較的意思不變；(a) 為量測順序延後一條建造鏈。

**S-12：json-adapter → adapters-relocate 的串行要不要拿掉**

- 事實：串行不是技術依賴（決定行原文「shader 工具搬進 SDL plugin 後也改用它」）。拿掉後 adapters-relocate 可先做，但 shader 工具搬進 SDL plugin 時仍連 nlohmann（plugin 裡暫時多一份 JSON 函式庫的直連），N1 在 adapters-relocate 的判定要另寫過渡（json-adapter 未完成時 `engine/` 仍有 nlohmann 7 處直連）。
- 選項：(a) **建議** 保留串行；(b) 拿掉，adapters-relocate 宣告寫 N1 的過渡判定與 shader 工具的暫時直連。
- 取捨：依 §2.3 的 list scheduling，2' 階段鏈在 Engine／plugins 線上；拿掉串行可讓 adapters-relocate 提前最多約 22～44 h（json-adapter 的估時，推定），但總完成點會不會縮短**未重算**。
- 牽動：**S-9 選 (a) 時，S-12 實際上必須選 (b)**，否則 2' 階段整條鏈等到 G2b。S-9 選 (b) 時，S-12 兩個選項都可行。建議組合：S-9 (b)＋S-12 (a)。

**S-14：Gateway 的 Go module 的 owner 怎麼登記**

- 事實：使用者已定 `services/gyo_gateway` 的 owner＝網路 plugin 的 owner（`v8-arch-adapter-plugins-review` 第 (7) 點）。今天共通 build 登記它：`build/cmake/GyoBuild.cmake` 的 `gyo_register_go_module(OWNER engine …)`（註解：「每個組態都記錄，沒有產品時 Go 檢查也跑」）；`build/cmake/GyoGo.cmake` 的契約說 OWNER 是「已登記的 product 或保留的 engine 層」；`build/ci/common/go_checks.py` 以 `ENGINE_OWNER = "engine"` 決定在 toolchain 列跑。另一處 `tests/common/cmake/GoServiceTests.cmake` 的 `OWNER engine` 是共通測試的 module，與 Gateway 無關。
- 限制：OWNER 不能在共通 `GyoBuild.cmake` 改寫成網路 plugin 的名字——共通層寫特定 plugin 的名字違反 AGENTS §9。所以登記的呼叫要隨 owner 移進網路 plugin 自己的 CMake，契約也要多認一種 owner。
- 選項：
  - (a) **建議**：登記移進 `plugins/net_link` 的 CMake；`GyoGo.cmake` 的 OWNER 契約加「已登記的 plugin」一類；`go_checks.py` 認得 plugin owner，在選了網路 plugin 的列跑 Gateway 的 Go 檢查。module 目錄留在 `services/gyo_gateway`（不搬：搬了會動 Go 測試讀向量檔的相對路徑；AGENTS §7 允許 owner 與目錄不一致，owner 由 plugin 登記與 `docs/plugins/net_link` 識別）。
  - (a′) 同 (a)，但目錄搬進 `plugins/net_link/` 下（目錄與 owner 一致；要改 Go 測試的相對路徑、CI 的路徑）。
  - 不採：保留 `OWNER engine`、只在文件寫 owner 是網路 plugin（登記資料與事實不一致）；改由 object_fps_pvp 登記（owner 錯）。
- 後果（兩個選項都有）：Gateway 的 Go 檢查從「每個組態都跑」變成「選了網路 plugin 才跑」。沒有產品的 toolchain 列不再跑它；PvP 的列選了網路 plugin，照樣跑。刪網路 plugin 時 Gateway 的登記隨之消失（§7 removability 的正確方向）。
- 做的批：net-link（遷移計畫 §3.12；它依賴 plugins-foundation 的 plugin 登記）。本批只把決定寫進計畫 §3.12 與 Delta-7 第 6 項，不改 CMake。

**證據位置**：遷移計畫 §2.1 G2b、§3.12、§3.16、§3.17c〈並行〉、§4 Delta-7、§6 BU-4／BU-6；規劃稿〈最終檢查處置記錄〉S-9、S-12、S-14；`build/cmake/GyoBuild.cmake`、`GyoGo.cmake`、`build/ci/common/go_checks.py`；JSON 調查報告 `docs/architecture/plans/third-party-evaluation/json-library-survey.zh-Hant.md` §2.6。

---

### A-4　`Changed<T>`／`Added<T>` 要不要算「讀 T」（D7 空白之一；有語義，所以列進 A）

**現況**：spec 沒寫 `Changed<T>`／`Added<T>` 的 Contribution；實作不給它讀集合，所以它與 `Write<T>` 之間沒有衝突邊。

**機制**：With／Without 不產生衝突是因為成員在 Running 期間不變（W-3）。`Changed<T>` 不一樣：它看 T 的變更 stamp，而 `Write<T>` 在 Running 期間就會更新這個 stamp。所以同 stage 內兩者的先後會影響「這一 tick 看到還是下一 tick 才看到」。不會漏看（stamp 單調、跟上次執行比），只差一個 tick 的延遲；但目前這個先後是靠登記序平手隱性決定，也不在歧義診斷裡出現。(c) 禁用變更過濾（S-9），所以只影響 (b)。

**選項**：(a) 維持、spec 寫明「`Changed`／`Added` 不產生衝突；與 `Write<T>` 的先後只影響延遲一個 tick」；(b) **建議**：`Changed<T>`／`Added<T>` 的 Contribution 是「讀 T 的變更 stamp」，計入讀集合，與 `Write<T>` 有衝突邊（Bevy 也把它當讀 T）。

**取捨**：(b) 讓 (b) 區多出一些平手邊與診斷，(c) 區不受影響（不准用）；要改實作（Contribution）與 J-12 的期望值（不在本批；J-12 是存取推導的測試，不是 DI 守衛的判定放寬：多推一個讀＝締める面）。(a) 不動程式，但延遲差異是隱性的。

---

### A-5　(c) System 持 untyped `Commands` 要不要在 finish 報錯（D7 空白之二；有語義，列進 A）

**現況**：finish 不查。執行期：untyped `Commands` 寫 zone=C 或 Exclusive 欄會回 Err（DI-KS 2）；寫 zone=B、Shared 的欄則沒有任何檢查擋。

**機制**：DI-KS 8 要 zone=C System 的全部存取（含延後寫）都落在 zone=C 資料；untyped `Commands` 唯一合法的目標（zone=B、Shared）正好違反它。所以 (c) System 持 untyped `Commands` 沒有任何合法用途；而寫 zone=B 的那條路是一條沒人查的 (c)→(b) 寫入（`Diagnostics` 是 v0.3 唯一核准的 (c)→(b) 例外）。

**選項**：(a) 維持、spec 寫明這個限界；(b) **建議**：Z4 加「無 untyped `Commands`」，finish 報 D-E7（與 S-7、S-8、S-9、S-13 同類）。

**取捨**：(b) 是收緊，DI-KS 8 的條文不用改（它已經蘊含這條），只改 spec G.1 Z4 與 J-38 的案例；實作要加一項檢查（不在本批）。

### A-6　F.2 第 7 步（Resource 操作）沒有發出者：刪掉，還是補一個句柄（架構師 2026-11-14 轉達卡 06 world-exec 的發現）

**現況**：spec F.2 第 7 步寫「Resource 操作：依 (ResourceId 升冪, planPos 升冪, recSeq 升冪)」，但 v1 的參數型別裡沒有任何句柄能記錄 Resource 的延後插入／移除（全文掃 `InsertResource`／`RemoveResource`／「Resource 操作」只命中第 664 行這一處）。卡 06 照 spec 實作了這一步，結果是到不了的死碼（卡 06 dev_log〈未加入〉，PR #218，未合併）。

**機制**：Resource 目前只經 `ResMut<T>` 即時寫；登記在 finish 前完成，執行期不增減。第 7 步的排序鍵是預留給「延後插入／移除 Resource」的，但沒有 Feature 指名要它。

**選項**：
- (a) **建議**：刪掉第 7 步的內容，**保留編號**寫「（v0.5 刪除：v1 無發出者）」，第 8 步不改號（world-exec 的程式註解與 dev_log 都以「0～8 步」引用）。實作的死碼在後續批刪。日後若有 Feature 需要，再在 spec 修訂裡定它的位置與排序鍵。
- (b) 保留第 7 步，補一個句柄（例如 `Commands` 加 Resource 的延後插入／移除）。要有指名的 Feature（AGENTS §2 Derivation Order），目前找不到。
- (c) 保留第 7 步不動，註明「v1 無發出者，預留」。

**取捨**：(a) 讓規格與可達的行為一致，代價是日後加回時要重決它在 0～8 步中的位置（同步點的順序影響決定性，屆時是基盤變更）。(c) 省掉那次重決，但留下一條沒有 Feature 支撐、沒有測試能到達的規則（AGENTS §1「將來可能需要」不是根據）。(b) 目前沒有根據。

---

## B. 機械性修正（照已定事實改）

| # | 出處 | 改法 |
|---|---|---|
| B-1 | spec D.2 第 7、8、8b 步與第 3 步；C.4；E.6 X-2、X-3；F-9 | 錯誤代號重疊統一，**以實作為準**（registry-schedule 已過審查與 L1；兩欄做法經架構師核定，宣告修訂 5）：(1) 親和不符：主代號 X-E2／X-E3，D-E10 是**步驟代號**；(2) (c) 的 Parallel 無守衛：主代號 X-E1，D-E11 是步驟代號；(3) 事件跨 domain 或在非 clock schedule 讀寫：F-E1，從第 7 步（D-E7）的清單移出；(4) 參數引用未登記的型別（含 Resource）：R-E4；D-E3 只留「World 沒有配發器卻登記 `Spawn`」與「Startup 結束時仍未初始化」。D.2 末段寫明錯誤帶兩欄（代號＋步驟代號，步驟代號只在與代號不同時才有）。D-5、J-24 的代號清單不變（D-E10、D-E11 仍是 J-24 可比對的步驟代號）。 |
| B-2 | spec D.1 第 5 條、G.1 Z4（D7 兩個空白） | 依 A-4、A-5 的決定寫。選 (a) 就寫明現行行為與限界；選 (b) 就寫新規則並註明「實作在後續批」。 |
| B-3 | `docs/architecture/plans/foundation-followups/PLAN.md:583`「Engine 已擁有 Go 版」 | 改成「Go 版在 `services/gyo_gateway`（owner＝網路 plugin 的 owner，`v8-arch-adapter-plugins-review` (7)）」。該段是當時的計畫敘述，只改這個事實，加註更正日期。 |
| B-4 | spec L.3 表 Gateway 列（`services/gyo_gateway`）「外部，不變」 | 改成「外部行程；owner＝網路 plugin 的 owner（同一份 GYOP 的 C++／Go 兩個實作）；維持無狀態」，引決定行。I.1 表 Gateway 列同步加 owner。`guards-inventory.zh-Hant.md:197`、`legacy-impact.zh-Hant.md:170` 引用的是當時的 spec 句子，判斷（無狀態、相容）不受 owner 變更影響，**不改**。 |
| B-5 | spec H.4.2「SDL 視窗 runner plugin」 | 改成「視窗 runner plugin（不含 SDL；SDL 呼叫只在 SDL plugin）」（`v8-arch-adapter-plugins-review` RC (ii)；遷移計畫 §3 window-runner 的 Feature）。 |
| B-6 | spec I.1 前註「asio、httplib、protobuf 隨 IO 服務」 | 補「IO 服務與這三個函式庫的加入點在網路 plugin」（review 決定行；遷移計畫 net-link 差分）。 |
| B-7 | spec I.1 表 SDL 平台列、音訊 Mixer 列 | 補「SDL 呼叫在 SDL plugin；Engine 留中性 platform 契約與 audio core」（review (3)、RC (ii)）。 |
| B-8 | spec L.1 表 `base`（StrictJson）、`io`（JSON 輔助）兩列 | 補「JSON 是 Engine 的能力：一個 Engine 的 JSON 模組經唯一 adapter 連 yyjson，數字走原文路、轉換用 fast_float；其他模組改用它」（`v8-arch-json-yyjson-raw-number`、`v8-arch-json-fast-float`）。初判欄不改。 |
| B-9 | spec L.1 表 `net` 列 | 補「搬進網路 plugin（`plugins/net_link`），owner＝網路 plugin」（review；遷移計畫 net-link）。 |
| B-10 | spec L.1 表 `platform`（sdl）、`input`、`text`、`asset`、`render`、`audio`、`model` 七列 | 補 SDL 家族與 ufbx 的去處：後端程式搬進 `plugins/sdl`、`plugins/sdl_image`、`plugins/sdl_ttf`、`plugins/ufbx`（只搬不改形狀；adapters-relocate）；render 列加「shader 編譯器搬進 SDL plugin，編譯步驟留共通內容管線」（`v8-arch-shader-compiler-in-sdl-plugin`）；audio 列加「audio core 留 Engine」（review (3)）；model 列加「取樣規則留 Engine」（review (3)）。初判欄不改。 |
| B-11 | spec L.2 表 `ImGuiShell` 列 | 補「ImGui 官方 SDL3／SDL_GPU 後端放 SDL plugin 內的獨立 target（例 `sdl_imgui`），只有選了 imgui 的產品連它」（review RC (ii)）。 |
| B-12 | spec §0 Runtime 列（型別名 App） | 已由 registry-schedule 改完；本批只確認全文沒有把 Runtime 當型別寫的識別字（`Runtime::`、`Runtime&` 等），有就改 `App`。 |
| B-13 | spec 檔頭、文末 | 狀態改 v0.5；修訂記錄加「v0.5（`v8-arch-spec-v05`）」一節，逐條列 A 的決定與 B 的修正。 |
| B-14 | 遷移計畫 §2.1 G1／G2b、§3.12、§3.16、§3.17b／c、§4 Delta-7、規劃稿處置記錄 | 依 A-2、A-3 的決定改條文；規劃稿〈最終檢查處置記錄〉的 S-9、S-12、S-14 加「已定（決定行代號）」一欄，不改當時的處置文字。 |

| B-15 | spec A.5 W-8、A.6〈與 EntityStore 的關係〉（架構師 2026-11-13 轉達卡 06 world-exec 的定案） | world-exec 採 `Cell<T>` 包法。W-8 加「元件型別須 nothrow move assignable」，檢查欄指向 world-exec 的 `static_assert`（照它實際的位置寫）；A.6 的「兩者語義相同；spike 選其一並報告」改成「已定 `Cell<T>` 包法（world-exec；`target/world.zh-Hant.md` §6）」。 |
| B-16 | spec 中提到 store 不持 stamp 的地方（A.6、L.1 `entity_store` 列的 DI 6） | target/world §6 承諾的文字掃描由卡 06 實作為 `tests/common/kernel/store_stamp_guard.py`（ctest `kernel.store_stamp_guard`）；spec 若有引用處，守衛欄對上這個名字。 |
| B-17 | spec A.6 第二層第 2 點、E.2 System 的第 ⓪ 步（架構師 2026-11-14 轉達卡 06 的發現） | 在 Serial Executor 下第 ⓪ 步（呼叫前比對）到不了：一次 `Run` 開頭的 Ready 窗口比對、同步點套用後重取的快照已涵蓋它能抓到的變化。補一句事實：「Serial 下由 Run 開頭比對與同步點後的重取快照涵蓋，⓪ 不會命中；保留給 Parallel Executor」。不改步驟、不改語義。 |

B-3～B-11 只改事實、不改設計；每處註明引用的決定行。**B-15～B-17 的前提**：卡 06（PR #218）已合併到 master（2026-11-14 確認時 `target/world.zh-Hant.md` §6 的定案句與 `store_stamp_guard.py` 還不在 origin/master）。動手時若還沒合併，就停下回報架構師，不引用未合併的檔名。

---

## 範圍

只動 `docs/`：

- `docs/architecture/plans/kernel/spec.zh-Hant.md`（v0.4 → v0.5）
- `docs/architecture/target/kernel.zh-Hant.md`（只在 A-1 選 A，或選 B 並同意加釐清句時動 DI-KS 6；其他 DI 不動）
- `docs/architecture/plans/kernel/migration-plan.zh-Hant.md`、`adapter-plugins-plan.zh-Hant.md`（B-14）
- `docs/architecture/plans/foundation-followups/PLAN.md`（B-3，一句）
- `docs/architecture/plans/kernel/HANDOFF.md`（A 每題一行決定、現況一行）、`docs/architecture/plans/kernel/README.md`（dev_log 一覽）
- `docs/dev_logs/2026_11_13_arch_spec_v05.zh-Hant.md`（新）
- 本宣告

不碰：`engine/`、`tests/`、`build/`、`services/`（A 的決定若要改程式，寫進對應批的範圍，不在本批做）；卡 01 的 plan-dryrun 與它的判定。

## 判定（看結果前宣告）

掃描一律用 Python（或 `git grep -P`），不丟 stderr，結束碼區分「零命中」與「失敗」，先以已知正例命中。腳本與輸出存主 checkout `build/evidence/spec-v05/`（git 管理外，本機 Intel Mac）。

1. **範圍**：`git diff --name-only origin/master...HEAD` 全部在 `docs/` 下（Python；正例：對一個造出的 `engine/x` 路徑命中）。
2. **錯誤代號重疊 0**：四條檢查——(1) D.2 第 8 步的主代號是 X-E2／X-E3，D-E10 只以「步驟代號」出現；(2) 第 8b 步主代號 X-E1，D-E11 只以步驟代號出現；(3) 第 7 步的檢查欄不含 F-9；(4) 第 3 步的條件不含「未登記」。基準：對 origin/master 的 spec 4 條全命中（重疊存在）；結果 0。
3. **對 legacy 的行號引用仍為 0**：照 a6-revision 判定 4 的正規式（`architecture\.md:\d+`、`entity-model[^ ]*第 ?\d+ ?行`、`determinism\.md:\d+`、`network-architecture[^ ]*第 ?\d+`、`entity-store\.md:\d+`、`error-handling\.md:\d+`），掃 spec、target/、mechanism；正例：a6-revision 之前的 spec（`2f2a9575^`）命中；結果 0。
4. **過時句 0**：`Engine 已擁有 Go 版`（PLAN.md）與 spec 的 Gateway 列「外部，不變」；基準 origin/master 命中 2；結果 0。
5. **B-5～B-11**：每處改動在報告的對照表列「位置 → 引用的決定行」，條數與本宣告 B 表一致（件数の突き合わせ）。
6. **A 的決定**：每題一行決定進 HANDOFF；spec／計畫的改動與決定逐條相符（審查，見下）。
7. **審查**：Fable／high 只讀一次（撰寫者 Opus，模型不同；navigator〈敵対的検査〉），看 diff 是否有與 target/、遷移計畫互相矛盾的句子、是否有改到設計（B 只准改事實）。找到 blocker 就回修。若使用者認為 docs-only 不需要，可取消，報告寫明。

## 範囲の限界

- A 的決定若要改程式（A-1 B、A-2 (b)(c)、A-4 (b)、A-5 (b)、A-6 (a)(b)），本批只改規格與計畫；程式、測試與守衛的改動在後續批，那之前**規格與實作不一致**，HANDOFF〈未結事項〉記回頭驗的時點。
- A-1 的 Shared 出口：PvP 的配發器是否在 digestSet 未查證（只影響選項 A 的代價描述）。
- A-3 S-9 (b)：重量的時間窗（舊路徑還在時）與 2～4 h 是推定。
- 卡 06 另有兩題 F.2 讀法（hook 佇列是否依 schedule 分開、despawn 是否先跑全部 OnRemove）待使用者定；定了由架構師轉來，再決定進 A 還是 B。
- 繼承的限界：LIM-3（規格文字改了，掃描與檢查的檢出力要在實作批用突變證明）。

## 停止條件

- 超過估時 1.5 倍，或單一問題累計超過 2 h。
- 改 B 項時發現要改設計（不只是事實）→ 停，移到 A 問使用者。
- 發現 #212 決定行之間互相矛盾 → 停，回報架構師。

## 模型／檔位與估時

- 撰寫：本會話 Opus 5.5／high（架構師卡片建議）。審查：Fable／high 只讀一次（判定 7）。
- 估時：宣告約 2 h（已花）；核准後的修改 4～6 h（spec 約 2～3 h、計畫與其他文件約 1 h、掃描與審查約 1～1.5 h、dev_log／HANDOFF／PR 約 0.5 h），全部推定。A-1 選 A 時加 DI-KS 6 的修改約 0.5 h。

## 件数の突き合わせ（報告開頭的口徑）

「A 決定 6／6 進 HANDOFF；B 修正 n／17 列落字；錯誤代號重疊 4→0；legacy 行號引用 0→0；過時句 2→0；diff 檔數（docs 以外 0）」。

# 全局計畫審計：舊架構污染、PvP 污染與合理（v8-arch-global-audit）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：審計完成；使用者 2026-11-14 已依第 5 節定了範圍與停點（kernel `HANDOFF.md` 決定行 `v8-arch-engine-scope-general`、`v8-arch-gyo2-in-repo`、`v8-arch-single-player-no-complexity`、`v8-arch-rederive-medium`），之後改走新分支重建（`v8-arch-rebuild-branch`）。下一批：[rebuild 事前宣告](rebuild-declaration.zh-Hant.md)。**

使用者的要求：檢查哪些地方受舊架構污染導致決策偏差、哪些受 PvP 的設計污染、哪些合理，並附一次架構基本前提的對抗。使用者的判斷：「現在的決策都是局部決策，最嚴重的是架構上的偏差，越到實作越明顯，整個架構都開始偏回之前系統的形狀。」

過程與證據（主 checkout `build/evidence/global-audit-2026-11-14/`，git 不追蹤）：
- 審計 A 計畫與決定的系譜（Fable／high）：`audit-A-plan-lineage.md`
- 審計 B 目標條文的系譜（Fable／high）：`audit-B-target-lineage.md`
- 審計 C 實作的回歸（Fable／high）：`audit-C-implementation.md`
- 基本前提的對抗（Opus／xhigh）：`premise-adversary-opus.md`
- 統合 v1（Opus，主會話）：`integrated-v1.md`；敵對檢查（Fable／high，一次）：`adversary-fable.md`（blocker 5／should 10／nit 6，全部採用）；本文＝統合 v2。
- 先前的兩份：卡 07 entity-store 重審（PR #220，`docs/architecture/plans/entity-store/rereview.zh-Hant.md`）、PvP 滲入檢查（`build/evidence/pvp-leak-check/integrated-v2.md`）。
- 編制照使用者 2026-11-14 的指示：審計用 Fable、統合用 Opus、敵對檢查用 Fable、前提對抗用 Opus／xhigh。

## 0. 結論
1. **方向成立**：Kernel 的資料模型、Host 契約與 Kernel API 的一部分，是照舊實作與 PvP 塑形、而且對所有 World 生效的。四份檢查方向一致，細項有互相矛盾處（第 1.4 節）。
2. **「系譜追得到舊程式」不等於「錯」**。把舊規則變成機制，本來就是這次改造寫明的方法（mechanism §4、v0.2「Mechanism over Convention」）。真正的問題分兩種：
   - **範圍**：只有 PvP 需要的規則被機制化成「對所有 World 生效」。
   - **理由**：結論可能對，但唯一的理由是「與舊的一致／延伸舊的／不必動已驗證的」，沒有和被否決的替代案正面比較。
3. **偏差的主要上游是舊實作的附帶性質**（B 追到 commit）：`std::map` 的順序 → DI-KS 3 的容器序；舊程式持參照 → E-12 → F.2；`PvpMatch::Reset()` → 世代；`FixedTickRuntime` → Host 的累加器；舊區表 → determinism target。
4. **越到實作越明顯的可檢驗機制**（C 的發現最具體）：Engine／Kernel 的設計選項以「不擾動已驗證的東西」決勝（#218 宣告：「選 `Cell<T>` 時 es-row 突變不動」）；守衛把被否決的替代案寫成必殺突變（J-32、J-34）；閘門全是 PvP；呈給使用者的選項由 spec 取捨段框定（K.3 多項是「採規格」）。
5. **核心機制站得住**，但 R 也不全乾淨（1.1 的註記）。

## 1. 分類（每列：拿掉舊或 PvP 的理由後，結論還贏不贏替代案）

類別：R＝有獨立理由；O-結＝結論沿用舊形、拿掉舊理由後輸給替代案；O-理＝結論可能對、但理由只剩舊的（要換理由或補比較）；P-範＝PvP 需要、但對所有 World 生效（範圍要收）；S＝規格本身的缺陷；?＝未判（要量或要使用者名指 Feature）。

### 1.1 R（保留）與註記
| 項 | 獨立理由 | 註記 |
|---|---|---|
| DI-KS 1 簽名推存取 | 不手寫第二份 | 條文以 zone 定義 `ExclusiveWorld` 的 Contribution，zone 移出時要改寫 |
| DI-KS 5 衝突與順序分開、平手依登記序 | 換 plugin 順序不靜默改結果 | 歧義規則的參數是 zone；zone 若移出，要定新參數 |
| DI-KS 2 Result／Err | 使用者主動改的 | 「Interrupted 不能回 Ready」理由是 (c)：P-範 |
| 延後結構變更、OnAdd／OnRemove | 疊代中不失效 | — |
| 兩段登記、一個型別一個 owner | AGENTS §7 | 「id＝登記序」與「兩個 Registry 共用 zone C」衝突（卡 08 的第二套身分）：S |
| 一 World 一執行緒、交換點的概念、runner 由 plugin 提供 | Bevy `set_runner`；任何 IO 邊界 | 交換點的數量與位置＝兩把 mutex：見 1.2 |
| Extract 單向、渲染 World 分開 | Bevy | 「無反饋」的出處是決定性（BP:35-45）：? |
| 單一寫者可宣告 | AGENTS §3 | — |
| 同平台順序決定性 | 成本低 | J-9 全面禁 `unordered_` 把 entity-store DI 3 也固定進 Kernel：O-理 |
| stamp 依 Plan、assets 無總管、naming DI、error-handling DI 2 | Bevy／反 O | — |
| 第一個 host＝Match | AGENTS §2「先在一個 host 量費用」；先拿需求最嚴的 | 缺的是並行的非 PvP 檢驗，不是順序本身 |
| plugins 的 runner／egui 切法 | 對得上 Bevy 的 winit runner、ScheduleRunner、bevy_egui | 理由要從「framework 部件」換成 Bevy：O-理 |
| C.6／M「不做 plugin ready、串流」 | 沒有 Feature 就延後（AGENTS §2） | 理由句引現況表：O-理 |

### 1.2 需要處理的項
| 項 | 類別 | 說明 |
|---|---|---|
| DI-KS 3 的 W-1（容器依 id 排序） | O-結（候選） | 沒有 Feature 根據；W-2（(c) 查詢序是契約鍵的函數）有 Delta-1 的 Feature，是 P-範 |
| RC 6 的處置句「順序不變、不加 archetype」 | O-結 | 比 legacy 還嚴；把置換的出口鎖住 |
| Delta-3 World 重用 entity_store | O-結 | #218：store 呼叫 6 處（含 const 版重複）、鍵集合上手寫二分 11 處、兩層虛擬包裝、守衛 3 件；理由「突變一覽不必重列」 |
| DI-KS 9 的「依 id 套用」 | P-範 | 對 (c) 有獨立理由（結果與 System 間記錄順序無關，同 DI-KS 5 的原則）；對 (b) 生效是範圍問題；與 Bevy 的實質差是階段序（insert 一律勝 remove） |
| DI-KS 9 的「hook 下一窗」 | S | 理由「避免 N 與連鎖」獨立；但有三個缺陷（回滾不重現、跨 schedule、半成品），三個替代選項都有代價 |
| DI-KS 4 的「世代＝重建 World」 | O-理 | 出自 `PvpMatch::Reset()`；代價被 B 誇大（單調配發下 Editor 不需要新世代） |
| W-9「單調、不重用」、`RangeAllotter`、`Owns` | P-範 | 與「策略由產品定」矛盾；「世代內不重複配發」是 Kernel 自己的前提，要留 |
| DI-KS 15＋H.7 | 拆列 | 多數列有理由（Bevy 也在 World 外）；O-結 只有兩列：固定步累加留 Host（Bevy 在 World 內）、H-7 白名單寫死 `FixedTickRuntime::Advance` |
| DI-KS 12 回滾由 Host 呼叫 | ? | 未比較；lightyear 也是在另一個 schedule 呼叫 FixedMain，搬進 World 要 Kernel 加重入，比較結果可能是維持 |
| DI-KS 14 的落地（Client 三 World） | P-範 | 有量測根據（v6 D19、D21、`nextDrawable`）；Kernel 允許共用 World（spec:826）。問題只在寫進 Engine target 當通則（runtime-composition §2、LIM-6「三個 Runtime」） |
| 預設 Exclusive | ? | 理由是 AGENTS §3，使用者 2026-11-11 定；「工具被迫收成大 System」沒量過，要工具 dry-run 才能判 |
| zone／role／rollback／ingress／digestSet／cFingerprint／ExportZoneCSources／DeclaredEdge.dataflow（已合併） | P-範 | 宣告面與啟動檢查已在 master、被 7 個指紋 golden 固定；執行期半在 master 沒有（master 還沒有 World）；#218 合併後 World 有 15 處 `Zone::` 分支——「移出最便宜的窗口以 #218 為界」 |
| `Domain` 封閉列舉 | O-結 | spec 說名字由 Host 給，程式是封閉列舉；改成本低 |
| Loading 的形狀、Player／Peer（runtime-composition DI 3） | P-範 | Feature 是 Client 的 loading 頁面；DI 已核准 |
| 事件無游標、不跨 domain | P-範 | 理由是回滾 |
| cFingerprint 與 G1 (iv) | S | 規格自己的組合下 Match 與 ClientSim 必不同，G1 照現狀過不了；與本審計無關也要決定 |
| spec:214「Kernel 不知道 digest」對 :376 `digestSet` | S | 自相矛盾 |
| `KernelSettings` 6 處特例、第二層驗證成本（4,000 實體 p95 29.8 ms／Run，未對預算判） | S | spike 是新程式，不是舊架構 |
| `World::Summary` 成為 API 與契約 | S | 測試需求升格 |
| 死碼與死欄位 7 處（F.2 第 7 步、E.2 ⓪、trivialRelocate、rollbackSafe、rollbackActive…） | S | 宣告先行 |
| determinism target 照搬 legacy 區表；legacy 那一節自標 Candidate | O-理 | 是否核准過，未查證 |
| 計畫：批以舊模組切；audit 的判定詞成為批與估時；前提改了不重跑（HO:136，架構師的決定） | O-理 | A：37 批中約 16 批的結論或切法會變，約 20 批不變 |
| legacy 轉入：相容 177／198、守衛留 31／63、突變留 1131／1841 | O-理 | 「相容」沒有獨立理由就轉正；重審要限定在 target DI 引用與各批碰到的條文 |
| 四個閘門全是 PvP；工具排最後、無閘門 | P-範 | 閘門本身檢的是新性質（G1 謂詞、G2a 成本、G3 回滾等價），不是只檢舊輸出 |
| golden 37 與 wire 不改 | 產品層 R；Kernel 層 O-理 | 對產品遷移批，golden 不變就是 Feature 的定義；問題是它把 T=35／R=215、欄序這類舊偶然變成 Kernel 的約束 |
| 刪除 object_fps（唯一的非 PvP 遊戲產品）在 Kernel 設計前 | 後果 P | — |

### 1.3 「範圍」取決於使用者的答案
- 若 Engine 只服務這個 FPS 家族（證據：v6 HANDOFF:238-242 單機＝本機跑同一套權威模擬；v8 README:127、:131；HO:40 刪 object_fps）：1.2 的 P-範 多數變成家族內合理，只剩工具付宣告成本（zone 無預設、預設 Exclusive）。
- 若 PvP 與單機（含 Editor／工具）都是一等公民（證據：v0.2 副標「走向契約驅動的通用 Runtime」）：P-範 全部要收範圍，前提對抗的 A1、A2 致命成立。

### 1.4 輸入之間的矛盾（未裁定）
- DI-KS 9 依 id 套用：卡 07 S7、前提對抗 D2 說成立；B 說是循環論證。Fable 檢查判 B 的循環說不成立。
- threading：A 判結論不變；B 判落地是 O。
- 預設 Exclusive：PvP v2 判 P；前提對抗判 D6 可守（預設值除外）；B 判 P。
- 「使用者裁決都是新形狀贏」：K.3 第 1、3、4、7、11、12 項也是使用者經側邊會話「採規格」定的。準確的說法：使用者主動改規格的，都是新形狀贏；「採規格」通過的，含 O 與 P。

## 2. 基本前提的對抗（全文見證據 `premise-adversary-opus.md`，含各替代前提的代價）
- 致命（取決於 1.3 的答案）：A1 Derivation Order 沒有執行（最小協定從 netcode 倒推；AGENTS §2：參考實作單獨不成根據）；A2 Kernel 把通用 ECS 與決定性平台綁在一起。
- 嚴重：A4 儲存與順序、A5 domain／Loading／事件、A6(b) 三 World 寫成通則、A7 工具只付成本、A8 RC 量不到偏差、A9 結構原因。
- 可守：D1～D7。
- 替代前提的代價（v1 漏掉的）：擴充點本身是新的基盤 API；產品忘了選決定性 plugin 時 (c) 檢查整個消失；有序與無序兩種疊代並存；Client 的 deadline 有回到 v7 時序問題的風險；反射是大 API。
- 未查證的反例：Photon Quantum、Netcode for Entities 這類多人引擎把決定性放在核心。「GYO 是多人模擬引擎還是通用引擎」是定位問題。

## 3. 越到實作越明顯：機制與可檢驗的預測
機制：
1. Engine／Kernel 的設計選項以「不擾動已驗證的東西」決勝（#218 宣告的 `Cell<T>` 理由；測試型別為保 golden 定形）。規則層的來源：AGENTS §1 必要最小限、§4 Prefer「Existing Interface Extension」、§14.4 的守衛變更成本——四份輸入都沒審 AGENTS 本身。
2. 守衛把被否決的替代案寫成必殺突變（J-32、J-34），新守衛釘住新邊（`expect_links`、`store_stamp_guard`）。
3. 閘門全是 PvP；golden 與 wire 不變把舊偶然變成 Kernel 約束。
4. 呈給使用者的選項由 spec 取捨段框定，系譜與被否決的替代案沒有一起呈上。
5. 已存在卻沒擋住的規則：target README〈Batch declaration checks〉「是不是在為舊形狀找位置」、守衛四選一（使用者 2026-11-12 說守衛會把架構拉回原形）。#218 仍選了不動突變的那一案——再加禁令不夠，要有機制（誰查、何時、看什麼產物）。
趨勢（三層的類別定義不同，只能當趨勢）：DI 層 15 條中 4 條「相容 legacy」；計畫層 37 批中帶 O 約 21 批、結論會變約 16 批；實作層 C 表 1.1 的 6 列中 5 列 O。
可檢驗的預測：卡 01 的 dryrun System 序列對 `PvpMatch::Tick` 的呼叫序；各批宣告中選項理由引用守衛或突變成本的比例。

## 4. 現況
- master：kernel-scaffold、registry-schedule（含 PvP 概念的宣告面）、entity_store、a6 重標、34 條目標 DI 與 RC。master 還沒有 World 與 Executor。
- 未合併：#218 world-exec（其中 Executor、存取驗證兩層、W-4 的 `{callSerial, syncEpoch}`、AccessCheckTests 與儲存無關、是 R）、#219 B1、#220 卡 07；卡 08 patch；卡 01～05 的宣告分支。

## 5. 要使用者決定的事
### 5.1 先定（其他都以它為參數）
1. **Engine 的目標範圍**：只服務這個 FPS 家族，還是 PvP 與單機（含 Editor／工具）都是一等公民。兩邊的證據見 1.3。
2. 附題：單機要不要名指成 Feature（沒有名指，照 AGENTS §2 不能當預見的根據）。
3. 附題：要不要重開 2026-11-11 的「帶入現有 GYO，不是 GYO2」（`v8-arch-kernel-bring-into-gyo`；mechanism「名詞留、換動詞」）。A 把它標為 O 的總入口。

### 5.2 停點（三選一；估時都是推定）
| 選項 | 內容 | 估時 | 停下的範圍 |
|---|---|---|---|
| 小 | 只凍結 World 資料模型：#218 不合併；DI-KS 3 的 W-1、DI-KS 4 的配發與世代、DI-KS 9、RC 6 處置句、Delta-3 暫不往上積。先加兩條流程規則與「形狀回歸」的 RC（Architecture Delta）。不依賴 World 的批照走（json-adapter、pvp-baseline、pageguard、B1 規劃等） | 8～16 h（文件與規則） | world-exec 以後依賴 World 的批 |
| 中（我建議） | 小的全部，加檢測先行、設時間盒：概念出處表（每條 DI 與 Kernel 公開欄位列出需要它的產品類別，以及它對其他類的強制成本）；一個工具（voxel_map_editor 或 ui_editor）在已合併的 Registry／Scheduler 上做宣告級 dry-run，量 zone 宣告占比、Shared 標記數、被迫合併的 System 數——門檻在看結果前由你定。再依數字定重推的範圍 | 小＋24～46 h | 同小，外加依結果決定的範圍 |
| 大 | 全停、從三類產品的 Feature 全面重新推導，遷移計畫重寫 | 120～250 h 後才恢復實作 | 全部 |
理由：功能線已停，再全停 Contract 與 Architecture 就是 AGENTS §2 要防的「三者同停」；中的選項先量，再決定重推多大，可能只需要資料模型加 zone 的抽離。

### 5.3 流程修正（要機制，不只禁令；改 AGENTS 要經風紀委員與你）
1. 呈給你的選項必須附：系譜（這個形狀從哪裡來）、被否決的替代案與決勝理由、需要它的產品類別。
2. Engine 與 Kernel 的設計選項，不得以守衛、突變、golden 的維護成本決勝（產品遷移批不適用：golden 不變是它們的 Feature）。由每批事前宣告的檢查項執行，審查者核對。
3. 新 RC（Architecture Delta）：概念出處表（只有一類需要、而且對其他類有強制成本就觸發）、與舊 host 的同構比（限 Engine／Kernel／Host 契約）。
4. legacy「相容」不自動轉正，限 target DI 引用的條文與各批碰到的條文。

### 5.4 個別項（可在 5.1、5.2 之後）
- hook 下一窗的三個選項；卡 06 的兩個待決 commit。
- cFingerprint 與 G1 (iv)（卡 01 Q1）——照現狀 G1 過不了。
- #218：關閉或保留為證據；與儲存無關的部分（Executor、存取驗證）是否拆出。
- 卡 08 patch：擱置（cFingerprint 的去留取決於 5.1）。
- 卡 07 D1～D10、#219、#220 的合併。
- 已合併的 PvP 概念宣告面的去留：條件句——若 5.1 答「一等公民」，在 #218 之前移出最便宜（7 個指紋 golden 與 DI 守衛的判定變更照 §14.4）。

## 6. 範囲の限界
- 只讀，沒有建置與測試；非本機的參考實作未查證。
- 四份都沒看的區域：AGENTS.md 本身、CI 與守衛基礎設施、`apps/object_fps_pvp` 與 `services/` 的現況程式、四個工具的程式、Kernel 改造範圍外的 Engine 模組（threads、time、os、net）、networking 計畫稿的全文。master 上沒有 `plugins/`，那一層只有計畫可審。
- 單機只有候選，沒有名指的 Feature。使用者「很多單機遊戲」的原話在 docs 與 AGENTS 找不到出處（是本次對話的話）。
- 統合者 Opus；敵對檢查 Fable 一次；v2 修訂後沒有再檢查。

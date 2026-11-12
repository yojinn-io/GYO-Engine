# 遷移批規劃（統合稿）：`v8-arch-migration-plan`

日期：2026-11-12。性質：ultracode 的統合稿（提案 Opus×3／high：風險優先、價值優先、成本優先；統合 Fable／high；對抗 Opus／xhigh 在本稿之後）。只讀 repo、只寫本檔；不寫程式、不改既有檔。
基準：origin/master `3061e879`（PR #196 `v8-arch-legacy-impact-dispositions` 已併入）；另加本分支 `claude/arch-migration-plan` 的 `4164635a`（`v8-arch-plugin-selection-data`，使用者 2026-11-12 的 plugin 選擇決定，三份提案寫成時尚未存在）。三份提案在主 checkout `build/evidence/kernel-migration-plan/proposal-{risk,value,cost}.zh-Hant.md`（git 管理外）。
引用寫法：檔名＋章節（例：spec B.1、audit §1.3、legacy-impact §3 衝突 4、HANDOFF 決定行 `v8-arch-…`）。

統合規則（任務指定）：三案相同的直接採；不同的在該節末「取捨」寫採哪一案、為什麼、放棄的代價；任一案提出而其他案漏的風險收進去；每批的順序、依賴、判定都可檢查；未決不替使用者決定。

---

## 0. 件数の突き合わせ與受理判定（看結果前宣告）

**件数の突き合わせ（本稿吸收的輸入 → §5 對照表）**：輸入 **38** 條＝ci-readiness §4 的 5 條＋spike-pageguard-report 末尾 11 點＋-detect 7 條（D1 Life 時序 1、D3 failures 寫點 1、G.4 補列候選 4、`TraceMovement` 讀時鐘 1）＋HANDOFF〈觀察〉windows 3 條＋legacy-impact 處置 12 條。§5 恰好 38 列，每列有「進哪一批、怎麼驗」。三案的計數：風險 38（寫法 5＋11＋7＋3＋12）、價值 38、成本 37（把 `TraceMovement` 併入補列第 4 條）；本稿照價值案拆成 38，因為 `TraceMovement` 的去處（diagnostics-service）與補列第 4 條（match-host）不同批。
**批次**：**32** 批（含條件批 1：`v8-arch-pageguard-os`）；§2 的樹、§2 的表、§3 的節各 32（工具四批在 §3 共用一節加差異表，仍各算一批）。閘門 4 個：G1、G2a、G2b、G3。
**已定前提的差異**：plugin 選擇已由使用者定為**資料**（HANDOFF `v8-arch-plugin-selection-data`，2026-11-12）；三案都把它列為未決（風險 U2、價值 U2、成本 U-2），本稿改為已定前提，§4 Delta-5 寫成「已定」、§7 不再列（只記這件事）。

**受理判定**（統合者與對抗檢查者照此判；任一不成立＝回提案，不改判定文字）：

| # | 判定 | 怎麼查 |
|---|---|---|
| A1 | 對抗檢查（Opus／xhigh）0 blocker；blocker＝照計畫做會破 Design Invariants、Product Boundary、(c) 區四處契約（mechanism §3.3），或違反 §1.1 的已定前提；should 全部處置或寫明不採的理由 | 對抗檢查報告 |
| A2 | 每批都有 Feature → Contract → Architecture 三段；Feature 寫「這批結束時誰看到什麼」；跨產品能力寫出使用它的 Product；Architecture 欄只在 Contract 撐不住時才有變化，撐得住就寫「不變」 | §3 逐批 |
| A3 | §3 Architecture Delta 七項只在 §4 寫一次（五個 Delta 各七項齊）；§3 各批只寫「引 §4 Delta-n」與差分 | §3、§4 |
| A4 | 每批有上段／下段與理由、實作者／審查者／驗證者的「模型／檔位」；上段不用小模型；審查者與實作者不同模型；§14.4 驗證者不是提議者本人 | §2 表、§3 |
| A5 | 性能相關批（`-pvp-baseline`、`-first-host-cost`、`-match-host`、`-client-sim`、`-client-frame`、條件批 `-pageguard-os`、`-registry-schedule` 的 B-4、`-world-exec` 的 B-2）引 §6 的 B-n 數字與量測方法，數字只在 §6 一處 | §6 |
| A6 | 每批的範囲の限界以「LIM-n＋一句摘要」寫，不只貼編號（target/README limits 規則 (b)）；每批寫「若這批失敗，後面哪些批白做」與停止條件 | §3 |
| A7 | 38 條輸入各有一列；不重開已定前提、不排暫停項（lag win1、網路帳本與兩通道、DET-0～3） | §5、§1 |

---

## 1. 原則與邊界

### 1.1 已定、不可重開的前提（出處：kernel `HANDOFF.md` 決定表）

- Kernel（World／System Contract／Scheduler／Executor）進 Engine（`v8-arch-kernel-bring-into-gyo`；K.3 第 7 項 `v8-arch-kernel-delta-approved`）。
- framework 退役 → `plugins/` 層；三層＝Engine／plugins／產品與工具（`v8-arch-framework-retire`；audit §3.1、§10）。
- 改造完成前功能線停，停法 (a)（`v8-arch-kernel-pause-mode`）：本計畫每批的 Feature 是「既有 Feature 搬到 kernel 上不變」加「它替哪個已點名的下一版 Feature 鋪路」，不新增玩法。
- Bevy 的實作選擇不照抄（HANDOFF 2026-11-10 現況條）。
- 基盤先設計、實作薄、先一個 host 量成本（AGENTS §2〈基盤 Architecture〉設計の進め方）。
- Data Contract 版本進格式（AGENTS §8；`gyo.zone-c-sources` v1 已核定）。
- 驗證＝啟動參數的執行期開關，四條件、LIM-6（`v8-arch-kernel-verification-not-assert`）。
- B-8 一個事件只有一種性質（`v8-arch-kernel-break-hook`）。
- 舊文件 legacy、設計權威在 target（`v8-arch-docs-legacy`）；現有程式受 legacy DI、新程式受 target DI，部件遷移完成時 legacy 刪、target 搬回。
- object_fps／v2／object_fps_preview、IRuntimePort、AnimationInstance 已刪（PR #185）。
- J-17 掃描器已有（PR #192；確認清單基線 26、候選 9）；-detect 已固定現況（T=35／R=215、`kernel_prep_sites` 白名單 47 列）；-zones 卡進行中（搬區不改行為）——本計畫把它當外部依賴，不重排。
- **plugin 的選擇是資料**（`v8-arch-plugin-selection-data`）：`project.json` 加 `plugins` 欄，共通 CMake 讀它連結 plugin target 並由建置產生登記的編譯單元；`main` 保持薄；System 的登記仍是程式。
- 暫停不討論：lag win1、網路帳本與兩通道、DET-0～3（`v8-arch-kernel-pending-paused`）。

### 1.2 本稿的排序原則（三案的交集，加統合者的一條）

1. **先證偽、後建造**（風險）：「若錯了後面全部白做」的性質（Plan 唯一且三平台相同；PvP 能在獨占＋區規則下表達）排在建造它之上的東西之前，各成一個單獨可證偽的批（G1）。
2. **第一個 host＝Match**（三案一致）：37 個 golden 與 `AuthorityDigest` 逐位元判定（spec J.4）是全 repo 最強的自動判官；wire 不變，所以 Match 遷完能用現有 Client 對戰。
3. **Kernel 只做第一個 host 用得到的部分**（價值、成本）：回滾與 Extract（H.5、H.6）契約已定，實作與 Match 遷移並行、不在關鍵路徑上。
4. **一個 host 要嘛整個在機制上、要嘛不在**（風險；spec H-7、K.2 第 8 條）：同一 host 的 sim 與 host 兩批可分開審，但同一條 PR 線一起合併，master 不出現半個 host（U3 的建議，§7）。
5. **量測先於預算判定**（風險、成本）：先量今天的 Match 當分母；預算寫成「基線的函數＋絕對上限」，在看遷移結果前由使用者核准。
6. **共用的東西先做一次**（成本）：net-link 的 IO 服務殼、sdl-window-runner、Engine 的「包」各一批，不在每個 host 各做一次。
7. **條件 Delta 不擋關鍵路徑**（風險）：頁保護（第三層）採不採在 world-exec 開工前定（U1），實作本身並行。
8. **統合者加的一條：閘門分兩段**（§7 U4）：G2a（重放 host 量 (c) 的 Kernel 成本）只放行**Kernel／plugins 側**與 Client 共用的工作；G2b（真實 Match 行程）才放行第二個 host 的遷移。這是對 AGENTS §2「一つの host で費用を計測し…他の host へ広げる前に」的嚴格讀法，同時不把 Client 線的前置工作卡死。

### 1.3 不在範圍

- 暫停項（§1.1 末條）：不排程、不設計、不以它們為驗收。
- **wire 不改**：遷移期間 pv 版本不變。Loading 的 ready 在 v1 以「Welcome 隱含 ready」實現（Match 只在 `LoadingState = Ready` 後接受 join）；需要新 wire 事件＝停下回報，進 pv10-requirements，不在批內改（§8 RC-9）。
- 新 Feature（功能線停）；Parallel executor（LIM-2，v1 任何 Parallel 宣告＝D-E17）；Observer、archetype、動態查詢（spec M）；ExclusiveWorld（J-45 守形狀，v1 無使用者）。
- 第三方 ECS 選庫：只在 world-exec 的事前宣告以 W-1～W-11 核對 EnTT 能否當儲存（`v8-arch-kernel-audit-third-party`），不另開評估。
- 工具的行為修正（model_viewer `ViewerWindow.cpp:112` 記帳缺陷等；audit §3.6）：不順手改；統一 editor-dialogs 若使行為變，事前宣告列為行為差異交使用者。
- AGENTS §2／§3 條文修訂（風紀委員線）。
- -zones 卡本身（產品線前置第二批，進行中）。

---

## 2. 批次總覽

### 2.1 樹（代號｜段｜實作者 模型／檔位｜一句白話）

```text
v8-arch-migration（第一個 host＝Match；★＝關鍵路徑；◇＝閘門）
├── 階段 0  前提（全部並行；-zones 卡進行中，是 match-sim 的外部依賴）
│   ├── ★v8-arch-a6-revision          上｜Fable/high 主會話自寫＋對抗 Opus/high｜spec A.6 收頁保護 11 點、legacy 處置 12 條、kernel DI 落 target、三個決定（U1、U5、U13）
│   ├── v8-product-pvp-baseline        下｜Opus/medium｜量今天 Match 的 tick、記憶體、到 ready 的時間（§6 的分母）
│   ├── v8-arch-kernel-scaffold        上｜Opus/medium｜engine/kernel 空殼進 L1 三平台、J-9 自寫禁字表、編譯失敗測試形狀在 Windows L1 定案
│   └── v8-arch-h7-scanner             下｜Fable/high（規則 xhigh）｜H-7 呼叫點掃描器＋基線（host 不得手排 Tick／Run）
├── 階段 1  Kernel 薄（先只做第一個 host 要的）
│   ├── ★v8-arch-kernel-registry-schedule 上｜Opus/xhigh｜Registry、簽名推描述、Scheduler、Plan 指紋三平台、錯誤代號表、G-5 匯出
│   ├── ◇G1 v8-product-pvp-plan-dryrun  上｜Opus/high｜只登記 PvP 的 System 描述跑 finish：拆欄後無第二寫者、(c) 歧義可解、Life 形狀定案
│   ├── ★v8-arch-kernel-world-exec      上｜Opus/xhigh｜World 欄、延後佇列、事件、hook、Serial Executor、Result／Interrupted、驗證兩層與開關
│   ├── ★v8-arch-kernel-loading         上｜Opus/xhigh｜Loading schedule、domain tick、ready、資產暫存連續前綴
│   ├── v8-arch-kernel-threading        上｜Opus/xhigh｜runner 介面、親和、一 World 一執行緒、交換點原語（Mailbox／Link swap）、TSan
│   └── v8-arch-pageguard-os（條件）    上｜Opus/xhigh＋敵對 Fable/xhigh｜U1 採第三層時：頁保護平台層進 engine/os、kernel 只持策略
├── 階段 2  第一個 host：Match
│   ├── v8-arch-plugins-foundation     上｜Opus/high｜plugins/ top-level、project.json 的 plugins 欄→建置產生登記、headless-runner、刪除驗證、Windows 啟停壓力
│   ├── v8-arch-plugin-net-link        上｜Opus/high（交換點 xhigh）｜Link Resource 家族＋IO 服務殼（Match 先用、Client 重用）
│   ├── v8-arch-diagnostics-service    上｜Opus/high（檔案生命期 xhigh）｜trace-diagnostics plugin、DiagnosticsWriter 接收端；windows 觀察 (a)(b) 驗收
│   ├── ★v8-product-match-sim          上｜Opus/high（Life／拆欄 xhigh）｜PvpMatch 拆成 (c) System；重放 host 上 golden 37 逐位元
│   ├── ◇G2a ★v8-arch-first-host-cost  上｜量測 Opus/medium、門檻對抗 Fable/high｜重放 host 量成本對 §6；放行 Kernel／plugins 側的 Client 前置
│   └── ◇G2b ★v8-product-match-host    上｜Opus/high（Ingress／Judge xhigh）｜真實 Match 行程跑在 kernel 上；B-1′ 實量；與 match-sim 同一 PR 線合併
├── 階段 2' Kernel 後半與 Engine 的「包」（G2a 之後；與階段 2 並行）
│   ├── v8-arch-kernel-rollback-extract 上｜Opus/xhigh｜H.5 回滾（Rewind／Restore／Replay）與 H.6 Extract、渲染 World 自己的 Registry
│   ├── v8-arch-engine-wrap-render     上｜Opus/high｜render、input、platform、text、audio 收成 Resource／Host 零件
│   ├── v8-arch-engine-wrap-assets     上｜Opus/high｜AssetManager 拆成三類 Resource、UiRenderer 局部重寫、io
│   └── v8-arch-plugins-frame          上｜Opus/high｜sdl-window-runner、asset-catalog、event-sounds 三個 plugin
├── 階段 3  Client（G2b 之後；三批同一 PR 線）
│   ├── ★v8-product-client-sim         上｜Opus/xhigh｜ClientSim Runtime：預測走回滾機制、與 Match 同一份 (c) System；windows 觀察 (c) 收口
│   ├── v8-product-client-link         上｜Opus/high（交換 xhigh）｜ClientConnection → net-link 的 Client 端
│   └── ◇G3 ★v8-product-client-frame   上｜Opus/high｜PvpApplication → ClientFrame／ClientRender；Client main＝plugin 清單＋Run
├── 階段 4  Editor 與工具（G2b 與 wrap-assets 之後；與階段 3 並行）
│   ├── v8-arch-plugins-editor         上｜Opus/high｜imgui 兩半、editor-dialogs、offscreen-preview（無 readback）；PreviewTests 重寫
│   ├── v8-product-tool-voxel          上｜Opus/high｜voxel_map_editor：第一個 Editor 樣本（文件 World＋渲染 World）
│   ├── v8-product-tool-model-viewer   下｜Opus/high｜model_viewer 遷移（`pendingAction_` 改資料）
│   ├── v8-product-tool-audio          下｜Opus/high｜audio_editor 遷移
│   ├── v8-product-tool-ui             下｜Opus/high｜ui_editor 遷移（`EditorApp` 介面重寫）
│   └── v8-arch-framework-delete       上｜Opus/medium｜framework/、三份文件、守衛、FRAMEWORK component 完整刪除（§7 驗證）
└── 階段 5  收口
    ├── v8-arch-ci-spike-retire        上｜Opus/low｜spikes/kernel 與 CI 的 spike 步驟移除（兩條件成立即可做，可早於階段 5）
    ├── ★v8-arch-runtime-loop-retire   上｜Opus/high｜刪 RuntimeLoop／IRuntimeClient（最後一個使用者遷完後）
    └── ★v8-arch-migration-closeout    下｜Opus/medium（審查圖書管理員）｜legacy 刪、target 搬回、LIM 逐條搬、Windows CI 同法重掃 60 run
```

審查者一律與實作者不同模型（實作 Opus → 審查 Fable；h7-scanner 相反）。閘門定義：**G1**＝`-registry-schedule` 的 Plan 指紋三平台相同且 `-plan-dryrun` 成立（D-E2＝0、非資料流順序邊 ≤50%、Life 形狀保住 T=35／R=215）；**G2a**＝`-first-host-cost` 在重放 host 上 B-1、B-2、B-3、B-5 都 ≤ §6 預算（任一在 1.0～1.5 倍＝報告由使用者決定；≥1.5 倍＝停、§8 RC-1）；**G2b**＝`-match-host` 的 B-1′、B-5（RSS）、B-7 在真實 Match 行程（Linux）上同樣成立且 golden 37 不變；**G3**＝`-client-sim` 的回滾等價（產品版 J-42）在 L1 三平台成立。

### 2.2 表

估時欄寫「本稿區間（來源與調整）」；單位工時，區間不是承諾（audit §1.4）。線：架構＝架構師主會話；產品 M／產品 C＝兩條產品遷移線（§7 U11）；包＝Engine／plugins 線；掃描＝J-17 掃描器線；CI；文件＝圖書管理員。

| 代號 | 依賴 | 產出 | 受理判定要點（細節 §3） | 估時（h） | 線／可並行 |
|---|---|---|---|---|---|
| `v8-arch-a6-revision` | — | spec v0.4（A.6、G.4 補列、行號改條號、鏈／清單措辭）、target 修訂、kernel DI 頁、legacy 36 條重標、三個決定（U1、U5、U13） | 對抗 0 blocker；§5 歸本批的 16 列各有落字位置；36 條重標件数一致；行號引用 13 → 0（正例先命中） | 6～12（-spec-v03 同形） | 架構；與 baseline、scaffold、h7 並行 |
| `v8-product-pvp-baseline` | — | 量測執行檔（不進 ctest）＋基準報告 CSV（`format=gyo.kernel-baseline, version=1`） | 件数（情境數、tick 數）；同機 5 次 p95 散佈 ≤10%，否則不出基準 | 3～8 | 產品 M |
| `v8-arch-kernel-scaffold` | — | `engine/kernel` 空殼 4 新增＋2 修改、`gyo_kernel_tests` 進 L1、J-9 禁字表＋自測、`mutations.json` 骨架、編譯失敗測試兩種樣本 | 清單外零檔；L1 三平台綠（含 Linux `core`）；兩種樣本在 windows-x64 過，選定寫進 registry-schedule 宣告 | 3～6（ci-readiness 實走 2 h＋J-9＋樣本） | CI |
| `v8-arch-h7-scanner` | — | `plans/kernel/scripts/h7_scan.py`＋自測＋ctest＋基線報告 | 正例 `MatchRuntimeHost.cpp:441`、`ClientSimulationRole.cpp:196` 必報；白名單依型別；突變全殺；基線＝match-host／client-frame 的歸零目標 | 4～8（J-17 同形、規則少） | 掃描 |
| `v8-arch-kernel-registry-schedule` | a6-revision（DI 頁、鏈措辭）、scaffold（編譯失敗形狀） | Registry／描述／Scheduler／Plan／錯誤代號表／G-5 匯出；不跑 System | J-10～J-25、J-27（D-E17 半）、J-28（finish 半）、J-37～J-41、J-44、J-47 三平台；Plan 指紋 golden 三平台相同；J-44 20 種排列；突變 2、3、4、5、6、11、14 killed；B-4 | 20～40（守衛約 25 條×1～2 h） | 架構 |
| `v8-product-pvp-plan-dryrun` | registry-schedule；-zones 建議先完成（非必要） | `tests/object_fps_pvp` 的描述登記測試（空函式體） | finish 成立；D-E2＝0 不靠改 Shared；非資料流順序邊 ≤50%；Match／ClientSim cFingerprint 相同；Life 形狀保 T=35／R=215（U6） | 6～12 | 產品 M |
| `v8-arch-kernel-world-exec` | registry-schedule；a6-revision 的 U1 決定（欄配置介面） | World、F.2、事件、hook、Serial Executor、Result／Interrupted、驗證兩層＋epoch、`KernelSettings`、`DiagnosticsWriter<T>` World 端緩衝 | J-1～J-9、J-26、J-29～J-37、J-32b、J-45、J-50、J-52、J-53 三平台；同一執行檔開／關兩個 World；突變 1、7、8、9、12、13、16 killed；B-2 | 24～48 | 架構；G1 前可開工，G1 不成立即停 |
| `v8-arch-kernel-loading` | world-exec | Loading schedule、domain tick、`CurrentTick`、`AssetStaging` 連續前綴、`LoadingStageDone<Stage>`、ready | J-48、J-49、J-51（合成資產）三平台；runtime-composition DI 3 等價測試；B-7 的每批上限（合成） | 10～20 | 架構；與 threading 並行 |
| `v8-arch-kernel-threading` | world-exec | runner 介面、親和檢查、`ownerThread`、交換點原語（`Mailbox<T>`、Link swap）、TSan 案例 | J-15（H-2）、J-17b、J-28（執行期半）三平台；TSan 零報告；D-E13 | 12～24 | 架構；與 loading 並行 |
| `v8-arch-pageguard-os`（條件） | U1 採第三層；world-exec | `engine/os` 平台層、kernel 第三層策略、`gyo_kernel_pageguard_tests`（`SKIP_RETURN_CODE 77`）、os.md DI 加條 | spike 同組測試重新宣告重跑，L1 **RelWithDebInfo** 三平台綠；os 邊界守衛對 kernel 零命中；B-9 | 10～20 | 架構或 CI；不在關鍵路徑 |
| `v8-arch-plugins-foundation` | registry-schedule、threading | `plugins/` top-level、`project.json` 的 `plugins` 欄與建置產生的登記（Delta-5 實作）、headless-runner plugin、依賴守衛、合成 plugin 的加入／刪除測試 | engine 不 include plugins（掃描）；刪合成 plugin 共通層零修改；缺 plugin／依賴衝突是明確 configure 錯誤；R-E6；headless-runner 的 Windows 啟停壓力 N＝200 零逾時 | 14～28（HeadlessShell 0＋S1 半 5～10＋build 層 5～10＋產生登記 4～8） | 包 |
| `v8-arch-plugin-net-link` | threading、plugins-foundation | `plugins/net_link/`：`Link`、`LinkMembershipInbound`、`LinkInputInbound`、`Link.outbound`、IO 服務殼（asio、httplib、protobuf 內部） | 合成 Link 的 swap 測試（兩執行緒、Running 期間零外部存取）；J-41 在 plugin 登記下成立；protobuf pool 若由 Egress 直接編碼列 I-5 例外 | 10～20（從 P3、P4 抽出） | 包；Match 側第一個消費者 |
| `v8-arch-diagnostics-service` | world-exec（`DiagnosticsWriter` 參數）、plugins-foundation | `plugins/trace_diagnostics/`：`TraceSink`／`LogFile`／`MovementDiag` 服務；P8／P12 服務化 | windows 觀察 (a)：Close 後無開啟句柄，同形測試在測試內重複 100 次零失敗；(b)：Close 尾行順序契約測試 100 次相同；J-14 的 Diagnostics 例外不擴大；trace 既有測試全過 | 12～26（trace 2～6＋P8 8～16＋P12 4～10 服務部分） | 包；與 match-sim 並行（介面誰先定寫在宣告） |
| `v8-product-match-sim` | plan-dryrun（G1）、world-exec、loading、-zones 卡完成（J-17 確認清單 0）、pvp-baseline | 產品 (c) plugin（`ObjectFpsPvp::Sim::`）、Body 四欄、Life 形狀、`ApplyMembership`、`AuthorityDigest` System、重放 host 跑 Match Runtime | golden 37 逐位元（LIM-5 例外只限事前列名）；-detect D1 兩案例不改一字仍過、`kp-death-next-tick`／`kp-respawn-next-tick` 仍 killed；J-17 對 Sim 零命中（清單由 G-5 匯出）；Loading 等價（分批數、完成序打亂）；cFingerprint 存證據 | 40～85（P1 40～80＋P7／P9／P13 4～18，扣 -zones 已做約 20%） | 產品 M |
| `v8-arch-first-host-cost`（G2a） | match-sim、pvp-baseline | 成本報告＋go／no-go | 件数（tick 數＝記錄數、5 次）；B-1、B-2、B-3、B-5（World 位元組）全在預算＝go；1.0～1.5 倍＝使用者決定；≥1.5 倍＝停（§8 RC-1）；驗證開／關各量一次 | 4～8 | 架構 |
| `v8-product-match-host`（G2b） | match-sim、first-host-cost go、plugins-foundation、net-link、diagnostics-service、h7-scanner | headless-runner 上的 Match Runtime；Ingress（`StageMembershipRequests`、`StageInputs`）／Judge／Egress；`IpcHost` → net-link 服務；`match_main`＝plugins 資料＋Run；刪舊 `PvpMatch`、`QueueActionAcknowledgement`／`SubmitActions` | `kernel_prep_sites` 白名單 A 22 列→0、W 類只剩 runner／Host 檔、G 類→0（逐列對帳）；H-7 掃描 Match 0；golden 37；Gateway go 測試、pvp 57 測試、acceptance 30 全過；D3 host 層特徵原樣過；**B-1′、B-5 RSS、B-7 在真實行程（Linux）實量** | 60～120（P2 40～70＋P3 20～40＋S1 半 5～10，扣 net-link 5～10，加補列 0～5） | 產品 M；與 match-sim 同一 PR 線 |
| `v8-arch-kernel-rollback-extract` | world-exec、loading；G2a go | H.5 `Rewind`／`Restore`／`Replay`、PredictionHistory；H.6 `Extract<Read<T>>`／`ExtractRes<R>`、渲染 World 自己的 Registry | J-42、J-42b、J-42c、J-43（含 `Extract<Write>` 編譯失敗）、J-46、J-49 三平台；突變 10、15 killed；B-6 合成記錄 | 24～44 | 架構；與 match-host 並行 |
| `v8-arch-engine-wrap-render` | world-exec；G2a go | render（渲染 World Resource、親和 `GraphicsContextOwner`）、input 後端 Host＋core 留、platform Host、text 包、audio `Submitter` | 各模組既有測試全過；platform-sdl G1～G3、input IP-1、`gyo_expect_*_links` 不變；跨元件跨幀裸指標 4 處→0；naming 第 5 條後綴掃描新程式零命中 | 19～53（audit §1.1 不調） | 包 |
| `v8-arch-engine-wrap-assets` | wrap-render、loading | 請求狀態 Resource、`Assets<T>`、`RenderAssets<T>`、`AssetInbox` IO 服務、loader 由 plugin 登記；`UiRenderer` 局部重寫；io 檔案服務包 | J-51 的真資料版；`AnimationContentDigest`／`ArenaContentDigest` 不變；`AssetManager*`／`&` 成員 0；ui 既有測試全過 | 19～48（asset 8～20＋ui 8～18＋io 2～6＋model 1～4） | 包 |
| `v8-arch-plugins-frame` | wrap-render、plugins-foundation | sdl-window-runner、asset-catalog、event-sounds 三個 plugin | 刪任一 plugin 只壞 audit §10.4 表列的消費者；event-sounds／catalog 資料契約 DI 照搬（owner 改）；R-E6 | 15～30（WindowShell 16～32、Catalog 5～10 的一部分） | 包 |
| `v8-product-client-sim` | G2b、rollback-extract、net-link | ClientSim Runtime（Loading＋FixedUpdate rollbackEnabled）、runner 的時鐘策略、`LocalPlayerPrediction` → Restore／Replay 的使用者 | 產品版 J-42（直接跑 k tick 對 Rewind＋Replay 逐位元）三平台；cFingerprint＝Match（H-4）；預測與對帳既有測試不改全過；**windows 觀察 (c)**：`object_fps_pvp.cpu` windows-x64 workflow_dispatch 重跑 10 次 0 timeout；D2 棘輪 Client 側歸 0；B-8 的模擬 tick 項 | 50～100（P5 20～40＋P6 30～60） | 產品 C |
| `v8-product-client-link` | G2b、net-link | `ClientConnection` → net-link 的 Client 端服務＋Link | network 標籤測試本機全過；`client_delay_runner` 等輸出欄位不變；IO 執行緒不碰 World（掃描＋審查） | 22～40（P4 30～50 扣 net-link） | 產品 C；與 client-sim 並行（第二張卡） |
| `v8-product-client-frame`（G3 後合併） | client-sim（G3）、client-link、wrap-assets、plugins-frame | `ClientFrame`／`ClientRender`、P10 包（時鐘改 `FrameTime`）、`src/App` 拆包、`PvpApplication` → plugins 資料＋Run | H-6／J-43 產品版；呈現 System D-E8＝0 且 LIM-1 成本記錄；D2 棘輪 Client 側 0；H-7 掃描 Client 0；acceptance 全過；Windows 實機一場（離線場次腳本）；B-8 | 61～122（P11 40～80＋P10 10～20＋App 6～12＋S1 半 5～10） | 產品 C；與 client-sim、client-link 同一 PR 線 |
| `v8-arch-plugins-editor` | wrap-assets、rollback-extract、plugins-frame；G2b | imgui 兩半、editor-dialogs（三種記帳統一）、offscreen-preview（無 readback）；`PreviewTests` PV2～PV5 與 pv-* 突變重寫 | H-6 原樣（無例外、無晚一幀）；每個 plugin 刪除驗證；editor-dialogs 被 ≥2 個工具使用；重寫的測試以突變證明檢出力 | 20～40（framework 33～66 扣 plugins-frame 已做） | 包 |
| `v8-product-tool-voxel` | plugins-editor | voxel_map_editor：main＝plugins 資料＋Run、`EditSession` 等包、W6／W7 守衛觀測手段重寫 | 工具既有測試全過；`gyo.voxel_map` 不變；H-7 掃描 0；預覽無 readback | 24～48 | 包；第一個 Editor 樣本 |
| `v8-product-tool-model-viewer` | tool-voxel | model_viewer 遷移；`pendingAction_` 改資料 | 同上 | 16～32 | 包；三工具三卡並行 |
| `v8-product-tool-audio` | tool-voxel | audio_editor 遷移 | 同上 | 8～16 | 包 |
| `v8-product-tool-ui` | tool-voxel | ui_editor 遷移；`EditorApp` 介面重寫 | 同上；若要改 `GYO::Ui` 介面→升上段、停 | 32～56 | 包 |
| `v8-arch-framework-delete` | 四工具批、client-frame | 刪 `framework/`、`docs/framework/` 三份、`tests/framework/`、`FRAMEWORK` component、`GYO_ENABLE_FRAMEWORK`、`ToolRegistryTests` 保留名、`framework_dependency_guard.py` | -remove-object-fps 五條判定同法；共通層改動逐條列（預期非零：Delta-2 影響檔）；空目錄 0 | 4～8 | 包 |
| `v8-arch-ci-spike-retire` | world-exec（J-12、J-5 三平台綠）＋（pageguard-os 完成 或 U1 定不採） | 刪 `spikes/kernel/`、CI spike 步驟、上傳路徑、摘要列（`.github/` 由使用者改） | ci-readiness §2.6 兩條件逐條勾；`spikes/kernel` 引用 0（歷史 dev_log 除外） | 2～4 | CI |
| `v8-arch-runtime-loop-retire` | client-frame、framework-delete | 刪 `RuntimeLoop`／`IRuntimeClient` 與測試；`FixedTickRuntime` 留 runner plugin | include 0；H-7 (a) 改為「檔案不存在」也過；`engine_tests` 案例逐條對照只減被刪的 | 6～15 | 架構 |
| `v8-arch-migration-closeout` | 全部 | legacy 九檔刪、target 搬回、LIM 逐條搬或標 Lifted、Status 行清、Windows CI 同法重掃 60 run 比較 | 同名並存 0；反向依賴 91 處 0 斷鏈；`tally.md` 同法比較表；圖書管理員 checkup 項全勾 | 10～20 | 文件＋CI |

**合計**：約 **560～1,140 h**（不含條件批 10～20 h）。三案分別 545～1,070／530～1,050／560～1,130，同一量級；Kernel 本體（registry-schedule＋world-exec＋loading＋threading＋rollback-extract）約 90～176 h 沒有 audit 數字可引，是以守衛條數×1～2 h 估的（成本案的估法），第一個 host 完成後依實測重估（§8 RC-3）。

### 2.3 關鍵路徑與里程碑

```text
a6-revision 6～12 → registry-schedule 20～40 → world-exec 24～48 → loading 10～20
 → match-sim 40～85 → first-host-cost 4～8 (G2a) → match-host 60～120 (G2b)
 → client-sim 50～100 (G3) → client-frame 61～122 → runtime-loop-retire 6～15 → closeout 10～20
＝ 約 290～590 h（plan-dryrun 與 world-exec 並行，不在路徑上；threading、plugins、net-link、diagnostics 在 match-host 前並行完成）
```

| 里程碑 | 在哪批結束 | 誰看到什麼 |
|---|---|---|
| M1 | registry-schedule＋plan-dryrun（G1） | 開發者：`gyo_kernel_tests` 三平台綠；合成與 PvP 的 (c) System 有歧義、多寫者時 finish 一次列出；Plan 可印出、三平台指紋相同 |
| M2 | match-sim | 使用者：37 個 golden 由 kernel 上的 PvP 模擬產生、逐位元相同 |
| M3 | first-host-cost（G2a） | 使用者：同機新舊兩路的 tick p50／p95／max、World 記憶體、載入時間，與 go／no-go |
| M4 | match-host（G2b） | 使用者：Linux 上真實 Match 行程跑在 kernel 上，用現有 Client 連 Gateway 對戰；統計行印 tick p50／p95／max |
| M5 | client-frame（G3 後） | 玩家：Client 全在 kernel 上；畫面、預測與今天一致；loading 畫面在 ready 前出現 |
| M6 | tool-ui（最後一個工具） | 工具使用者：四個工具在 plugins 上，預覽無 readback |
| M7 | closeout | 所有人：`docs/architecture` 只有一套；framework、RuntimeLoop 不存在 |

到 M3 約 108～213 h（關鍵路徑）；到 M4 約 168～333 h。只維持現有四條線（不開 U11 的兩條產品線與包線）時，產品批全部串行，關鍵路徑多約 80～155 h（成本案 §2.3 的計算）。

### 取捨（§2）

- **Kernel 的第一批是 Registry＋Scheduler（風險案），不是 World（價值、成本案）**。理由：沒有 spike 證據的兩件事——Plan 唯一且三平台相同、PvP 能在獨占＋區規則下表達——都只需要 Registry 與 Scheduler 的 finish 就能證偽（dryrun 的函式體是空的）；World 的存取推導與兩層驗證 spike 已證過一半。成本案說「Scheduler 讀 World 的存取描述與 writePolicy」，但描述與 `ColumnInfo` 是 Registry 的資料，不是 World 的儲存，所以依賴方向不成問題。代價：world-exec 的實作者要等 registry-schedule 的型別定稿才能接（約 1～2 週串行）；補救＝world-exec 在 G1 前就可開工，G1 不成立即停。
- **baseline、scaffold、h7-scanner 各成一批（成本案；風險案有 baseline）**，價值案把它們併進別批。理由：三者都便宜（各 3～8 h）、都在關鍵路徑外、都給後面的批一個「看結果前就有的」門檻（分母、Windows 的編譯失敗形狀、H-7 歸零目標）；併進大批會讓那些門檻在結果出來後才定（違 §14.1）。
- **loading 與 threading 各成一批（風險案）**，價值、成本案併進 schedule／plugins。理由：Loading 有自己的契約七點與 J-48～J-51，threading 有 H-2／H-3 與 TSan，各自可單獨證偽並行；代價：批數多、交接多（本稿 32 批對價值案 16 批）。
- **rollback 與 extract 合一批（價值、成本案）**，不照風險案拆兩批：兩者都只在 Client／Editor 線用、守衛數少（6 條）、同一實作者連做省交接。
- **net-link 獨立一批（成本案）**：Match 與 Client 是兩個已點名的消費者（spec I.2 的 Link 家族是契約），不算單一 use case 的一般化（AGENTS §1）。風險：兩端形狀分不出共同部分 → 停、拆成兩個 plugin（§3.12）。
- **閘門兩段（統合者）**，替代風險案「閘門在 match-host 後」與價值／成本案「閘門在重放 host 後」：見 §1.2 第 8 條與 §7 U4；放棄的代價寫在 U4。
- **match-sim 與 match-host 同一 PR 線（風險案）**，不採價值案的有界並存：見 §7 U3。
- **工具四批的段**：voxel 上段（價值案：第一個 Editor 樣本決定後三個的形狀），其餘三個下段（風險、成本案）。

---

## 3. 每批

**各批共通（不重複寫）**：
- 事前宣告檔：架構批 `docs/architecture/plans/kernel/<批>-declaration.zh-Hant.md`，產品批 `docs/object_fps_pvp/plans/v8/<批>-declaration.md`，工具批放各工具的 plans；看結果前宣告門檻、件数の突き合わせ、範囲の限界、改名清單（target/naming）、被取代的 legacy 條文；使用者核准再開工（AGENTS §14.1）。量測類門檻先做對抗式檢查（Opus／high 只讀，memory v7-ultracode-gearing）。
- 完成時 dev_log 與該批同一變更（AGENTS §15）；HANDOFF 每批開始／停止時更新。
- 停止條件共通：AGENTS §14 冒頭三條；單一問題累計 >2 h 或總時數 >估時上限×1.5（memory time-box）；golden 或 digest 出現事前未列的差＝停。
- 範囲の限界共通（所有寫 kernel 程式或 System 的批）：**LIM-2**（v1 只有 Serial，Parallel 宣告是 D-E17）、**LIM-3**（存取驗證攔不到 spike L1～L8 的形狀；驗證關閉時這一類全部不偵測；不經 World 的存取靠審查）、**LIM-6**（驗證開關是行程級啟動參數，「無違規」必附開關值與被排除的大欄清單）。動到 Match 成員的批另列 **LIM-5**（Join／Leave 在 tick 內，晚最多一 tick）；呈現批另列 **LIM-1**（(b) 讀 (c) 要顯式順序，成本在呈現 System 數）。各批只再列自己多出的。
- 突變：每批只跑相關突變，PR 線合併前背景全跑（memory mutation-rerun-scope）；計測を伴う批的報告第一行是件数の突き合わせ（AGENTS §14）。
- Delta 一律引 §4 編號、只寫差分。
- 執行者：控制權在架構師主會話（`v8-arch-kernel-control`），各批由主會話開子代理；產品批的內容 owner 是 `object_fps_pvp`，工具批的 owner 是各工具，執行者是架構師開的子代理（同 kernel-prep 卡）。
- §14.4 驗證者：只有改 golden、守衛判定或 DI 守衛測試時出場；模型照該批實作者（navigator），不是提議者本人，不用小模型。

### 3.1 `v8-arch-a6-revision`（A.6 修訂＋legacy 處置＋kernel DI 落 target＋三個決定）

- **Feature**：(i) 場次中開著驗證抓繞路，「無違規」報告帶「驗證開著」（PvP 的 Match／Client；開發者可見）；(ii) 後面每批的事前宣告能引用穩定條號——結束時開發者看到 kernel 的 `Design Invariants` 頁與 `Replacement Criteria` 在 target/，legacy 36 條標好「已被取代（批次代號）」或「相容（措辭更新）」。
- **Contract**：spec v0.4——A.6 第三層寫成條件化條文並限定「Exclusive 欄、(c) System、開關開」（pageguard 1）；Resource 是否保護（pageguard 2；建議 v1 不保護）；(b) System 成本規則（pageguard 3）；handler 串接與 `reinstalls` 進證據（4）；多 World 身分（5）；`tlCurrent` 為空的性質（7；建議 GYO_ASSERT：System 外寫入是 W-6 的 Programmer Error，一個事件一種性質）；記憶體估算分平台（9）；「鏈」與「清單」措辭統一（衝突 1）；G.4 併入 -detect 補列（第 5 條擴充、補第 6 條）；target/system-contract §2b 加 field-session 澄清句（衝突 3）；target/world §6 寫明 stamp 在 kernel 不在 store 並搬入 entity-model 第 139 行欄位表；target/determinism 收 legacy §1 三級、§2 區表、§4 區表（含 4.7a／4.7b）、§7、E0～E5；target/runtime-composition §3b 加 preview DI 4 已被取代的新條文；target/error-handling 註明與 L124 的關係；kernel DI 頁（spec K.1 → target，候選 `target/kernel.zh-Hant.md`，位置與圖書管理員對齊）。
- **Architecture**：文件層；不改結構。採第三層時的 OS Delta 只在這裡**寫**（§4 Delta-4），實作在 pageguard-os。
- **範圍**：只動 `docs/`：spec A.6／G.4／K／修訂記錄；target 各部件；legacy 檔只加標註不改設計內容；spec／target 的行號引用 13 處改條號或章節名；#176 判定總表四處收窄加五列（audit §4.4）。
- **受理判定輪廓**：(1) 對抗 0 blocker；(2) §5 中歸本批的 16 列各有落字位置（件数相等）；(3) legacy-impact §2 的 36 條依新定義重標，重標後 被取代 n＋相容（措辭更新）m＝36；(4) `git grep -nP` 對 spec／target／mechanism 的 legacy 行號引用（`:\d+`、「第 \d+ 行」）為 0，先以已知正例命中確認掃描有效、不丟 stderr（-remove-object-fps 教訓）；(5) kernel DI 頁有 `Design Invariants` 見出し與 `Replacement Criteria`，每條附守衛 J 號；(6) 只有 `docs/` 有 diff；(7) 三個決定記進 HANDOFF 決定表：U1（第三層）、U5（編譯失敗形狀的預設）、U13（目標 DI 核准時點）。
- **範囲の限界**：只寫文字，不驗條文可實作（由 registry-schedule、world-exec 驗）；「被取代」的標記在遷移批完成時才真正生效。
- **段**：上段（Design Invariants、契約措辭、OS Delta 文字）。**模型／檔位**：主會話 Fable／high 自寫；對抗 Opus／high 只讀（-spec-v03 同形）；驗證者不需要。
- **估時**：6～12 h。
- **被取代的 legacy 條文**：本批是重標的來源批；本身不取代條文。
- **風險與停止**：重標遇到「半條」（E-3 類）照處置決定行歸相容（措辭更新）；一條兩邊都說不通→停，列給使用者不自行拆條。A.6 修訂發現 K.3 已決事項互相矛盾→停（§14「原因改變前提」）。U1 拖延→world-exec 以「欄用一般配置、保留換獨立頁的介面」開工（§7 U1 預設）。
- **若失敗**：後面每批的事前宣告沒有穩定條號可引；不擋程式批開工，但擋它們的宣告核准。

### 3.2 `v8-product-pvp-baseline`（今天的 Match 量多少）

- **Feature**：無新 Feature；它是 §6 預算的分母（AGENTS §2「性能や資源に関わる Feature の計画は、判定に使う予算を書く」）。Product：object_fps_pvp Match。
- **Contract**：量測口徑——「一個 tick 的工作」＝今天 `MatchRuntimeHost::Step` 全段（取樣時鐘後到開始等待前；`MatchRuntimeHost.cpp:371` 起），遷移後同口徑＝交換點 A→B（含 Ingress／Judge／Egress）；「到 ready」＝行程啟動到送 `runtime.Ready` 的時點；輸出格式照 field-session C3（tick p50／p95／max、overrun 數、人數）、C6（到 ready 時間、峰值 RSS），CSV 帶 `format=gyo.kernel-baseline, version=1`。
- **Architecture**：不變。量測執行檔放 `tests/object_fps_pvp/`、不進 ctest（比照 `gyo_time_wake_record`「Deliberately not CTests」）。
- **範圍**：`tests/object_fps_pvp/` 新增一支量測執行檔（重放 host：`AuthorityDigest` 的腳本時鐘＋37 個情境，另加一個 8 人長情境 ≥3,600 tick）；不碰 `apps/`、`engine/`、`build/`、`.github/`。
- **受理判定輪廓**：每情境記錄的 tick 數＝跑的 tick 數；同機 5 次 p95 散佈 ≤10%（超過＝量測不穩，不出基準，停）；RSS 以 `/usr/bin/time -l`（mac）／`-v`（Linux）量穩態；工具鏈與組態（RelWithDebInfo）記錄。
- **範囲の限界**：本機 Intel Mac 與（可用時）Linux 實機（U8）；CI 共用 runner 的計時是雜訊不用；Windows 的 Match 成本不量（主戰場是 Linux，memory platform-priority）；量測要改產品程式才量得到→停（不是本批範圍）。
- **段**：下段（不碰 DI／契約／介面）。但它是上段計畫的輸入，**不用小模型**（navigator「上段の計画に入力する偵察」）。**模型／檔位**：實作 Opus／medium；審查 Fable／medium（重點：量到的是不是 tick 本體、不含等待）；驗證者不需要。
- **估時**：3～8 h。
- **被取代的 legacy 條文**：無。
- **風險與停止**：重度本機量測先講影響並取得同意（memory warn-before-heavy-local-runs；本批 headless、記憶體小，預期不需）。**若失敗**（量不穩）→§6 全部無分母，G2a 判不了；停下改口徑，不先開 match-host。

### 3.3 `v8-arch-kernel-scaffold`（kernel 進 L1 的腳手架）

- **Feature**：kernel 的每一條守衛從第一個 commit 就在 L1 三平台跑（開發者可見）；支撐的 Product：object_fps_pvp、四工具（之後都經 Kernel）。
- **Contract**：`gyo_expect_links(gyo_kernel GYO::Base)`（有 `.cpp` 時 STATIC；ci-readiness §1.1）；`mutations.json` 沿用 time-threads-trace 的 `run_mutations.py`（ci-readiness §2.4）；J-9 的禁字表自寫（不沿用 `neutral_names_guard.py` 的 WORDS，legacy-impact §4 第 3 列），自測含 `far`／`near`（memory windows-far-near-macro）；**編譯失敗測試的形狀**（ci-readiness §4 第 2 項）：(a) `try_compile` 預期失敗與 (b) trait＋doctest 各做一個最小樣本，在 windows-x64 L1 跑過，依 U5 的預設（核心 3 條用 (a)、其餘 (b)）寫進 registry-schedule 的宣告。
- **Architecture**：§4 Delta-1 的第一步（目錄與 build 2 行）。
- **範圍**：`engine/kernel/CMakeLists.txt`、`engine/kernel/include/engine/kernel/`（空殼標頭）、`tests/common/kernel/`（CMake＋smoke＋J-9 腳本與自測）、`engine/CMakeLists.txt`、`tests/common/CMakeLists.txt` 各 1 行、`docs/architecture/plans/kernel/scripts/mutations.json` 骨架。`build/`、workflow 不動。
- **受理判定輪廓**：清單外零檔（ci-readiness 判定 1 同法）；L1 三平台綠（含 Linux `core` preset，ci-readiness §1.3）；兩種編譯失敗樣本在 windows-x64 過並記錄何者較穩；J-9 自測正例（加一個產品名）失敗、反例（`Despawn`／`ExtractRes`／`zone`）不誤判；`gyo_expect_links` 反例（多連一個 target）讓 configure 失敗。
- **範囲の限界**：STATIC＋PRIVATE 連 `GYO::Os` 的形狀不在本批（pageguard-os 才需要）；MSVC `/W4` 零警告在本機 macOS 無法先驗。
- **段**：上段（Build Graph 變更是 AGENTS §3 Delta 項目）。**模型／檔位**：實作 Opus／medium（清單已實走過、L1 會驗，gearbox 原則 4）；審查 Fable／high；驗證者 Opus／medium。
- **估時**：3～6 h。
- **被取代的 legacy 條文**：無。
- **風險與停止**：清單外還要改檔才綠→停（ci-readiness 判定 1 的反例；記 Delta）。**若失敗**：registry-schedule 沒有 L1 落點；停。

### 3.4 `v8-arch-h7-scanner`（H-7 呼叫點掃描器）

- **Feature**：「每遷一個 host 少一套規矩」可檢查（HANDOFF〈觀測到的壓力〉）；使用者：遷移 Match／Client／工具 host 的開發者；Product：object_fps_pvp、四工具。
- **Contract**：spec H-7 守衛兩條：(a) 不 include `IRuntimeClient.hpp`／`RuntimeLoop.hpp`；(b) 呼叫點規則——host 原始碼不得呼叫產品或能力模組型別的成員 `Tick`／`Advance`／`Update`／`Step`／`Run`；白名單依型別（Kernel 的 `Runtime::Run`／`Rewind`／`Replay`、`FixedTickRuntime::Advance`）；報告格式照 J-17「檔:行 規則 文字」（target/zone-c-sources §5 同形）。
- **Architecture**：無；掃描器在 `plans/kernel/scripts/h7_scan.py`（spec H.8 原文位置）＋ctest（`tests/common/zone_scan/` 旁或 `tests/common/kernel/`）。
- **範圍**：腳本、自測、ctest 一個、基線報告；不改 apps。
- **受理判定輪廓**：正例 `MatchRuntimeHost.cpp:441`（`match_.Tick`）、`ClientSimulationRole.cpp:196`（`loop_.Run`）必報；反例 `FixedTickRuntime::Advance`、Kernel 的 `Runtime::Run` 不報；突變（刪規則、改正規式）全殺；掃描用 Python 或 `git grep -P`、不丟 stderr；基線報告（今天各 host 的命中數）＝match-host／client-frame／工具批的歸零目標。
- **範囲の限界**：依型別判若需要 C++ 解析而 Python 做不到→改成依「宣告型別的檔」近似並寫限界。
- **段**：下段（守衛工具，不改契約）。**模型／檔位**：實作 Fable／high（規則正規式 xhigh）；審查 Opus／high；驗證者照實作者。
- **估時**：4～8 h。
- **被取代的 legacy 條文**：無。
- **風險與停止**：見限界。**若失敗**：match-host 的 H-7 判定只剩審查；不擋開工，擋 match-host 的宣告。

### 3.5 `v8-arch-kernel-registry-schedule`（Registry、簽名推描述、Scheduler、Plan）

- **Feature**：跨產品能力「Match 與 Client 預測共用同一份模擬核心」（object_fps_pvp 的 Match 與 Client；玩家可見：預測不因兩端順序不同而回彈）；開發者可見：產品加 System 時，順序歧義、第二寫者、區違反在 finish 一次列出（mechanism §5）。結束時＝M1 的前半：Plan 可印出、三平台指紋相同。
- **Contract**：Registry 生命週期（spec C.2 build／finish／凍結、R-E1～R-E6、runner 在 finish 安裝）；SystemDescriptor 從簽名推出（S-1～S-4，J-12），含 `mounts` 多掛點與 D-E19；Plan 唯一（D-1）、(c) 歧義＝D-E8、(b) 依 registrationSeq 平手（D-3、D-4、D-10）；錯誤代號表 D-E1～D-E19、R-E1～R-E6、X-E1～X-E3、F-E1（D.2）一次回報（D-5）；Plan 指紋（C.2 第 5 點）與 Registry 指紋（J-18）帶版本欄；區規則 Z1～Z8 的 finish 檢查；**G-5 匯出的 (c) 來源檔清單照 `gyo.zone-c-sources` v1**（`generated_by = "gyo-kernel-report"`，格式不變、只換生產端）；API 名稱本批定並寫進 target/scheduler、system-contract。
- **Architecture**：§4 Delta-1 的主體（新 Engine 子系統 `engine/kernel`）。只做到「描述→Plan」，不跑 System（Executor 在 world-exec）。Contract 撐不住的只有一處：編譯失敗測試需要 build 層的形狀（scaffold 已定）。
- **範圍**：`engine/kernel/`、`tests/common/kernel/`；spike 的 `AccessDerivationTests.cpp` 搬改為 J-12、`RegistryWorldTests.cpp` 第 14／40 行搬改為 J-18／J-19、第 50 行重寫（ci-readiness §2.2）。
- **受理判定輪廓**：J-10～J-25、J-27（D-E17 半）、J-28（finish 半）、J-37～J-41、J-44、J-47 在 L1 三平台綠；**Plan 指紋 golden**：三個合成 Registry（含 (c)／(b) 混合、延後寫、事件寫）在 linux／windows／macos 指紋逐位元相同；J-44 的 20 種排列全過；J-24 每種錯誤代號一個最小 Registry；J-17 kernel 端自測：合成 (c) System 檔由 G-5 匯出，`zone_scan.py` 讀得進且違規數與預期相同；突變優先清單第 2、3、4、5、6、11、14 條 killed；B-4（finish ≤50 ms @500 System，本機 Release）。
- **範囲の限界**：共通三條；只驗描述，不驗函式體實際存取（LIM-3）；指紋相同不代表 (c) 結果相同（Math／編譯是 G.2 第 4 處，Kernel 檢查不了）；J-27 只有 D-E17 那半（LIM-2）。
- **段**：上段（公開介面、Design Invariants、決定性）。**模型／檔位**：實作 Opus／xhigh（決定性與平手規則是「気づかないうちに誤りやすい」）；審查 Fable／xhigh；驗證者（若有 §14.4 變更）Opus／high 另一代理。
- **估時**：20～40 h。
- **被取代的 legacy 條文**：entity-store RC 5（交集手寫 >3 → 查詢層：world §1 已定）；architecture.md L136 已同步（PR #190 後條號改引「Runtime boundaries 段的禁令」）。
- **風險與停止**：**若這批失敗，後面全部白做**（World、Executor、四個 host 都假設同一 Registry 得同一 Plan、三平台相同）。指紋在某平台不同、修兩次仍不同→停，觸發 spec K.2 第 1 條 (a)／第 4 條與 §8 RC-2。K.2 第 3 條（為消 D-E8 加的順序一半以上與資料流無關）在合成測試就出現→停。G-5 的 `source_location` 只知登記處 `.cpp`，產品 System 跨編譯單位時照 zone-c-sources 的 RC 改版本，不加欄硬撐。

### 3.6 `v8-product-pvp-plan-dryrun`（PvP 能不能表達成這套規則；閘門 G1）

- **Feature**：同 3.5 的跨產品能力；另一個名指 Feature：Body 拆欄後「獨占寫者」成立（使用者 K.3 第 9 項：不拆的話架構改造就沒有意義）。Product：object_fps_pvp 的 Match 與 Client。
- **Contract**：PvP 的欄與 System 宣告（zone、writePolicy、role、rollback；Body／Life／InputCursor／Connection 診斷四欄＋Player／Peer 分開）；Match 的 Loading＋FixedUpdate 與 ClientSim 的 FixedUpdate 兩套 ScheduleDef；**Life 的寫者形狀**（§7 U6 的候選，本批以 finish 結果定）。只寫描述，函式體是空的（回 Ok）。
- **Architecture**：不變（產品測試目錄裡的一份登記）；Contract 撐不住時才回報（例：某 digest 欄必須 Shared 才能登記＝撐不住，LIM-4）。
- **範圍**：`tests/object_fps_pvp/` 新增一支測試；System 名照 audit §1.3 P1／P2／P9、spec H.4.1／H.4.2 與 target/naming。
- **受理判定輪廓**：finish 成立（零啟動錯誤）；D-E2＝0（Exclusive 欄寫者都 ≤1，**不靠改 Shared**）；為消除 D-E8 加的 before／after 中「與資料流無關」者 ≤50%（K.2 第 3 條的量化；每條邊附資料流理由，審查者逐條判）；Match 與 ClientSim 的 cFingerprint 相同（H-4）；**Life 的寫者形狀能同時保住 -detect 的 T=35（死亡當 tick 進 snapshot）與 R=215（復活在 tick 開頭生效）**，且不觸發 D-E19（同一 schedule 掛兩次）——這是 U6 的判準。
- **範囲の限界**：共通三條；描述不是程式——函式體經全域或捕獲讀到的東西不在描述裡（LIM-3 的 L5）；dryrun 成立只表示「規則表達得出」，不表示遷移後行為相同（那是 match-sim 的 golden）。
- **段**：上段（區判定、digest 欄的 writePolicy＝資料形式）。**模型／檔位**：實作 Opus／high；審查 Fable／high（重點：非資料流的順序邊、Life 形狀）。
- **估時**：6～12 h。
- **被取代的 legacy 條文**：無（只登記，不改產品）。
- **風險與停止**：**若這批失敗，match-sim（40～85 h）、match-host（60～120 h）、client-sim 白做**。Life 無法在單寫者下保住 T／R→停，交使用者（U6）；非資料流邊 >50% 或某 digest 欄必須 Shared→停，重劃區或重拆欄（K.2 第 3、10 條，§8 RC-4），不硬過。

### 3.7 `v8-arch-kernel-world-exec`（World 與 Serial Executor；驗證兩層）

- **Feature**：同 3.5；另「進 digest 的狀態只有一個寫者」成為啟動檢查（AGENTS §3 State Authority 的機制版）；下一版的 loading 分批載入需要延後佇列與同步點。結束時開發者看到 `gyo_kernel_tests` 跑 World 的守衛，驗證開／關兩個 World 在同一執行檔。
- **Contract**：World（A.1～A.5：鍵 u64、欄依 id 升冪、同步點後參考失效、SoA keys／值／stamps）；延後佇列 F.2 套用順序（hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource；每階段依 id，同鍵依 (planPos, recSeq)）；事件 F-7～F-9；hook F.4；Serial Executor E.2（含第 ⓪ 步呼叫前比對與第 ⑤ 步呼叫後比對）、`Result<void, KernelError>`、Interrupted（E.5）；驗證兩層＋句柄 epoch、`KernelSettings` 兩參數（驗證開關、`breakOnKernelError`）四條件（LIM-6）；第一層與第二層都以 Err 回傳（pageguard 8）；違規帶 World 身分（pageguard 5 的第二層部分）；欄配置走一個小介面（一般配置／獨立頁，pageguard 6）；`DiagnosticsWriter<T>` 參數與 World 端緩衝、Interrupted 不丟（S-8 例外、B-13）。
- **Architecture**：§4 Delta-1 續；§4 Delta-3（World 的欄儲存重用 `engine/entity_store` 的演算法，二選一：回傳位置的查找版本或 `Cell<T>`；事前宣告以 W-1～W-11 核對 EnTT 後定，預期自寫——swap-and-pop 違 W-2）。
- **範圍**：`engine/kernel/`（續）、`engine/entity_store`（若選回傳位置版本，加小介面；Delta-3 差分寫在宣告）、`tests/common/kernel/`；spike 的 `AccessCheckTests.cpp` 搬改（斷言→Err、兩執行檔→一執行檔兩 World；ci-readiness §2.2 表）、`RegistryWorldTests.cpp` 第 74 行搬改 J-7、第 94／114 行**重寫**（spike 語意與 F-2／F-7 相反）。
- **受理判定輪廓**：J-1～J-9、J-26、J-29～J-37、J-32b、J-45、J-50、J-52、J-53 在 L1 三平台綠（L1 主圖 RelWithDebInfo）；J-5 原文三種繞路開著攔、關著回 Ok 並帶「驗證開著」欄位，旁邊印 `IsDebuggerPresent()`（ci-readiness §3.2 的推斷變證據）；J-32 三平台 golden；突變優先清單第 1、7、8、9、12、13、16 條 killed；`gyo_expect_links(gyo_kernel GYO::Base GYO::EntityStore)` 在 configure 生效；B-2（Kernel 自身開銷，合成 Registry 記錄執行檔）。
- **範囲の限界**：共通三條；第一層在 spike 是斷言、本批直接照規格 Err；L7（其他執行緒的存取當 host）到 threading 才評估；回滾、Extract 不實作（契約在 H.5／H.6）；頁保護不在本批（L1、L6 仍不解）。
- **段**：上段（Design Invariants、資料佈局、錯誤契約）。**模型／檔位**：實作 Opus／xhigh（F.2 套用順序、stamp 取值、驗證比對是決定性與時序類）；審查 Fable／xhigh；驗證者 Opus／high 另一代理。
- **估時**：24～48 h。
- **被取代的 legacy 條文**（重標後以 a6-revision 清單為準）：entity-store DI 1（對 store 仍成立的半條歸相容）、DI 4、RC 1、RC 2、RC 6；error-handling L124（衝突 1：宣告引 target 並註明關係）；field-session DI 3（衝突 3，相容，引 §2b 澄清句）；error-handling.md:81「`GYO_DEBUG_ASSERT` 將來加」（誤導句，legacy-impact §5 第 5 條）。
- **風險與停止**：MSVC 的 `std::launder`／`static inline char` token 在 RelWithDebInfo 無 L1 證據→第一次 L1 紅先切分實作／測試／環境（§14.1）；J-2／J-32 需要改 World 資料模型才能過→spec K.2 第 1 條→停，交使用者「修還是重建」；W-1～W-11 核對發現 EntityStore 撐不住 SoA 而要 archetype→停（spec M、K.2 第 6 條）。**若失敗**：loading、rollback、所有 host 白做。

### 3.8 `v8-arch-kernel-loading`（Loading schedule）

- **Feature**：loading 頁面與分批載入（使用者名指，spec H.2b；object_fps_pvp 的 Match 與 Client；玩家可見：Client 在 ready 前停在 loading）；初始成員走同一條路。
- **Contract**：Loading 是 clock schedule、`tick[Loading]` 是批次計數不是遊戲 tick（S-4：(c) 可讀但等價測試不比它）；stage 固定序 Ingress → 資產 → 地圖 → 場景 → 可互動物件 → 敵人 → 玩家；`StageMembershipRequests`／`StageAssets` 的 Ingress 掛點；`AssetStaging` 依請求序號連續前綴套用（`ApplyStagedAssets`、`AssetCursor` 唯一寫者）；`LoadingStageDone<Stage>` 每 stage 一寫者 `Finish<Stage>`；`LoadingState = Ready` 由 `Finish<玩家>` 寫；對局 tick 在 ready 時為 0、第一個模擬 tick 是 1（H.2b 第 5 點）；domain tick 是唯一 tick 來源（H-1、`CurrentTick` 參數、D-E12）；`MembershipRequests` 以序號為鍵＋`MembershipCursor`（H.2b 第 4 點）。
- **Architecture**：不變（spec 已定形狀落地，§4 Delta-1 內）。
- **範圍**：`engine/kernel/`（domain tick、`CurrentTick`、clock schedule 計數）、`tests/common/kernel/`（合成 `AssetInbox`）。
- **受理判定輪廓**：J-48、J-49、J-51（合成資產）三平台；runtime-composition DI 3 的等價測試（分批數不同、資產完成順序打亂，ready 時 (c) 欄與 Resource 逐位元相同，排除 role=Input 暫存與 `tick[Loading]`）；B-7 的「Loading 每批 ≤16.67 ms」在合成資料上量一次（本機）。
- **範囲の限界**：共通三條；真資產載入服務在 engine-wrap-assets，本批用合成 `AssetInbox`；Client 的 Loading（H.2b 第 8 點：manifest 同源、成員經 Restore）在 client-sim 才接。
- **段**：上段（契約、Design Invariants）。**模型／檔位**：實作 Opus／xhigh（游標與連續前綴）；審查 Fable／high。
- **估時**：10～20 h。
- **被取代的 legacy 條文**：entity-model E-12（W-3、F.2 取代；§5.2 已加修訂註）。
- **風險與停止**：**若失敗**→match-sim 的初始成員路徑沒有落點（Startup 不放內容是已定），match-host 白做；停。

### 3.9 `v8-arch-kernel-threading`（執行緒模型）

- **Feature**：Match 每 tick 準時跑、Client 預測執行緒與主執行緒互不阻塞（object_fps_pvp 的 Match 與 Client；玩家可見：tick 不漏、畫面不卡）；開發者可見：親和錯誤在 finish 報出（H-3：headless 沒綁 `Main`）。
- **Contract**：一個 World 一條執行緒（H-2、D-E13、`ownerThread`）；`runner = Main | Role(name)`、System 與 Resource 的親和（X-3、D-E10、X-E2、X-E3）；Executor 不擁有執行緒（E.3：RoleThread 由 Host 建、Host 決定停止順序；threads DI 4 相容）；交換點原語（H.1）：`Mailbox<T>` 與 Link 家族的 swap、Running 期間零外部存取（I.2 的可檢查句）；runner 介面（R-E6「一個 Runtime 一個 runner」）在 Kernel，實作在 plugins。
- **Architecture**：不新增子系統；Contract 撐不住的地方：今天 `ClientSimulationRole` 與 `MatchRuntimeHost` 各自的 deadline 邏輯（audit P5「何時跑下一步判 Host」）——本批只提供 runner 骨架與等待工具的接法，不搬產品的相位策略。
- **範圍**：`engine/kernel/`（runner 介面、親和檢查、交換點原語）；`engine/threads`、`engine/time` 不改；`tests/common/kernel/` 加執行緒案例與 TSan 案例（本機 `build/target/_build/tsan`）。
- **受理判定輪廓**：J-15（H-2）、J-17b、J-28（執行期半：執行緒不符 GYO_ASSERT）、D-E13 三平台；交換點資料競爭測試在本機 TSan 零報告。（Windows 的 RoleThread 啟停壓力放在 plugins-foundation 的 headless-runner 上，見 3.11；Client 側的最終收口在 client-sim。）
- **範囲の限界**：共通三條；L7 仍在（`tlCurrent` 是 thread_local，其他執行緒的存取當 host；Parallel 不實作）；windows 觀察 (c) 的舊原因未特定，本線驗的是「新 runner 上不再出現」，不是「找到並修了舊的」（AGENTS §14.3）。
- **段**：上段（執行緒模型、公開介面）。**模型／檔位**：實作 Opus／xhigh（時序）；審查 Fable／xhigh。
- **估時**：12～24 h。
- **被取代的 legacy 條文**：architecture.md L140-155（`FixedTickRuntime` 擁有累加器與 tick ID → G-6：累加器是 runner 的時鐘工具、`tickId_` 不是 domain tick 來源；條號在 a6-revision 定）；network-architecture 第 180-181 行（誤導句 §5 第 17 條，文件線加註）。
- **風險與停止**：**若失敗，match-host、client-sim、client-frame 白做**（都跑在 runner＋交換點上）。TSan 報告修兩次不收斂→停。

### 3.10 `v8-arch-pageguard-os`（條件批：頁保護平台層的 OS Delta）

- **開批條件**：U1 選「採」（a6-revision 時或 G2a 後，見 §7 U1）。不成立就不開，LIM-3 的 L1／L6 維持，本批自 §2 刪除、§4 Delta-4 撤銷。
- **Feature**：存取驗證第三層——場次與測試中攔「(c) System 寫了又寫回 Exclusive 欄」（LIM-3 的 L1；object_fps_pvp 的測試版與場次，Match 與 Client 都受益）。這是 os.md RC 9 末句「and no Feature」的出口（legacy-impact 衝突 4 處置）。結束時 LIM-3 的 L1 標 `Lifted in v8-arch-pageguard-os`（L6 不能解）。
- **Contract**：A.6 第三層範圍（只寫、只 Exclusive 欄、只 (c) System、隨同一開關；pageguard 1）；Resource 保護依 a6-revision 的決定（2）；`engine/os` 新介面：`PageSize`／`AllocatePages`／`FreePages`／`SetWritable`／`InstallFaultHandler`（回呼 `bool(*)(uintptr_t, bool isWrite)`）／handler 統計（ci-readiness §2.3）；與其他 SIGSEGV／SIGBUS／VEH 使用者共存（交前任、保持安裝、`EnterRun` 重裝並計數 `reinstalls` 進「驗證開著」證據；pageguard 4）；區間表不可變陣列原子發佈（5）；欄成長後新頁依狀態設唯讀（6）；(c) 呼叫後比對跳過受保護欄的最佳化只在量了之後做（11）。
- **Architecture**：§4 Delta-4。kernel 連 `GYO::Os` PRIVATE → kernel 由 INTERFACE 改 STATIC；區間表、違規記錄、策略留 kernel（不讓 os 反向依賴 kernel，AGENTS §6）。
- **範圍**：`engine/os`（新檔）、`engine/kernel`（策略）、`tests/common/kernel/` 新執行檔 `gyo_kernel_pageguard_tests`（只它定義 `DOCTEST_CONFIG_NO_POSIX_SIGNALS`；sanitizer 下 `return 77`＋`SKIP_RETURN_CODE 77`，ci-readiness §2.5）；`tests/common/os` allowlist 不動；os.md DI 加一條（使用者同意）；spike `PageGuardTests.cpp` 19 案例照 ci-readiness §2.2 搬改；Cost 案例改成不進 ctest 的記錄執行檔。
- **受理判定輪廓**：判定 1 照 `v8-arch-kernel-pageguard-reading`「重新宣告、重跑同一組測試」——L1、(b1)、(b2)、宣告形狀的 L6 攔到，(b3) 依 Resource 決定（保護則攔到，不保護則是 Limit 測試；宣告不得再自相矛盾）；**MSVC RelWithDebInfo 的 L1 證據**（spike 只有 Debug；`volatile` 寫法在 `/O2` 無既有證據）；VEH handle 保存、可卸除；os 邊界守衛 R1～R4 對 `engine/kernel` 零命中；`gyo_expect_links` 檢查 kernel 不外露 `GYO::Os`（trace 先例）；B-9。
- **範囲の限界**：L6（Ready 期間 host 經存下指標寫）不能解；L2、L5 不解；sanitizer 建置下 skipped（看得見的缺口）；debugger 附加時每次受保護寫入停在 debugger（寫進開發文件）；Windows 每欄 64 KiB 位址空間。
- **段**：上段（OS 層 Delta、Design Invariants）。**模型／檔位**：實作 Opus／xhigh；敵對 Fable／xhigh 只讀（同 -spike-pageguard）。
- **估時**：10～20 h。
- **被取代的 legacy 條文**：os.md RC 9（**不是被取代**：使用者在 U1 決定例外或改寫，本批照決定標）。
- **風險與停止**：MSVC 最佳化組態攔不到、或與 doctest／CRT 衝突、或 VEH 風險十條（ci-readiness §3.3）任一實際發生→**不重排計畫**：第三層不採，LIM-3 的 L1 維持，獨立頁配置介面保留但走一般配置；其他批不受影響——這就是它不在關鍵路徑上的理由。

### 3.11 `v8-arch-plugins-foundation`（plugins 層進 build graph；plugin 選擇是資料；headless-runner）

- **Feature**：Product 的追加・複製・削除不改共通層（AGENTS §7）延伸到 plugins：刪一個 plugin，Engine 與其他 plugin 成立；Match 能以 headless-runner plugin 啟動（object_fps_pvp Match）；開發者看到 `plugins/` 進 build、headless-runner 能跑一個合成 Runtime 並在 SIGINT 依序停下。
- **Contract**：plugin 介面（spec C.5：`Name`／`Requires`／`Build`／`Finish`）；runner 在 finish 安裝、一 Runtime 一 runner（R-E6）；**plugin 選擇是資料**（`v8-arch-plugin-selection-data`）：`project.json` 加 `plugins` 欄（有序清單或依賴宣告，二選一在宣告寫明——使用者決定行說「初始化順序由資料或 plugin 宣告的依賴決定」），共通 CMake 讀它連結 plugin target 並**由建置產生登記的編譯單元**（與 `components` → `GyoComponents.cmake` 同一條路）；`project.json` 格式版本升一（AGENTS §8）；缺 plugin／依賴衝突是 configure 的明確錯誤（§9）；System 的登記仍是程式（plugin 的 build 函式）；守衛掃 `main` 與產品程式不直接引用 plugin 的 build 函式（runtime-composition DI 2，名稱本批定）。
- **Architecture**：§4 Delta-2（`plugins/` 新 top-level）與 Delta-5（Product Registration 加 `plugins` 欄；已定）。Contract 撐不住的地方：今天的登記（兩份 CSV、`project.json` components）只描述 Engine 元件、沒有 plugin 的位置；`GyoApps.cmake`／`GyoComponents.cmake` 要加讀 `plugins` 欄與產生登記單元。
- **範圍**：新增 `plugins/`、`plugins/headless_runner/`（收 `HeadlessShell` 與 `match_main` 的迴圈形狀、停止順序；audit §10.1／§10.2）；`build/cmake/GyoBuild.cmake`（加 `plugins/`）、`GyoApps.cmake`／`GyoComponents.cmake`（讀 `plugins` 欄、產生登記）；依賴守衛（`engine/` 不 include `plugins/`）；`FRAMEWORK` component 與 `GYO_ENABLE_FRAMEWORK` 此時**不動**（framework-delete 才動）。
- **受理判定輪廓**：(1) 依賴守衛自測（正例先命中）；(2) 刪除驗證——合成一個測試用 plugin，加入與刪除只改它自己的目錄與 `project.json`（共通層零修改，照 -remove-object-fps 判定 2：`git diff --stat` 只有登記）；(3) 缺 plugin／循環依賴各一例讓 configure 失敗並指名；(4) 產生的登記單元與 `project.json` 一致（反例：手改產生物→重 configure 覆寫）；(5) R-E6 測試；(6) **windows 觀察 (c) 的第一段**：headless-runner 上跑一個合成 Runtime 的「Start→Run×k→Stop」在測試內重複 N＝200 次（windows-x64 L1，每次 ≤1 s、總逾時 60 s），零逾時、零 hang，輸出每次啟動的 wait backend 名；(7) ctest 名單前後只多本批的測試；(8) L1 三平台綠。`.github/` 若要改（新目錄的路徑過濾）交使用者。
- **範囲の限界**：共通三條；只有 headless-runner 一個真 plugin，其餘切法（audit §10.4 十個候選）在各自的批決定；N＝200 看不到偶發率低於 1/200 的問題（closeout 再以 60 run 統計比較）。
- **段**：上段（新 top-level、Build Graph、Product Registration）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high。
- **估時**：14～28 h（HeadlessShell 0＋S1 Match 半 5～10＋build 層 5～10＋產生登記 4～8）。
- **被取代的 legacy 條文**：architecture.md 三層表（L52-85）與 framework 定位（L33／L50）→ runtime-composition §4；AGENTS §9 候選 5 已由決定結案（本批實作「資料」，宣告寫與 §9 的關係給風紀委員）。
- **風險與停止**：plugin 選擇做不到單一事實、或刪一個 plugin 要改共通層、或共通層出現產品名分支（§9 MUST NOT）→停，回使用者。**若失敗**：match-host、所有工具批的組合方式要重想。

### 3.12 `v8-arch-plugin-net-link`（Link Resource 家族＋IO 服務殼）

- **Feature**：Match 與 Client 的連線都經 Link Resource 在交換點換入換出，IO 執行緒碰不到 World（跨產品能力：object_fps_pvp 的 Match（Gateway loopback TCP）與 Client（asio））。
- **Contract**：Link 家族（spec I.2、H.2b 第 2 點、K-S5）：`Link`（external、zone=B）、收件依種類分 `LinkMembershipInbound`／`LinkInputInbound`（每筆 `{arrivalNs, bytes}`）、`Link.outbound`；持有者標記各一；交換點一起 swap；wire 不變。
- **Architecture**：§4 Delta-2 的一個 plugin（audit §10.4 net-link 候選）；asio、httplib、protobuf 是服務內部實作（audit §0）。
- **範圍**：`plugins/net_link/`；**不搬** `IpcHost`／`ClientConnection` 的產品邏輯（各在 match-host、client-link）。
- **受理判定輪廓**：合成 Link 的 swap 測試（兩執行緒；Running 期間零外部存取，以交換點計數證明）；J-41（(c) 讀 Link 失敗）在 plugin 登記下仍成立；protobuf generated pool 若由 Egress 直接編碼，比照 I-5 明列例外（audit §3.6）；刪除驗證（刪它只壞 Match／Client）。
- **範囲の限界**：共通三條；network 標籤測試只在本機（memory ci-runs-cpu-shader-only）。
- **段**：上段（網路資料流的 Architecture）。**模型／檔位**：實作 Opus／high（交換點的執行緒順序 xhigh）；審查 Fable／xhigh；驗證者 Opus／high。
- **估時**：10～20 h（從 audit P3 20～40、P4 30～50 抽出共用殼；P3／P4 各扣）。
- **被取代的 legacy 條文**：無（network-architecture 的入口表在 match-host／client-link 加註）。
- **風險與停止**：Match 與 Client 的 Link 形狀分不出共同部分（例：Match 的同步 refusal 必須保留）→停，改為兩個 plugin 並記重工（§8 RC-6）。

### 3.13 `v8-arch-diagnostics-service`（診斷與 trace 成服務；windows 觀察 (a)(b)）

- **Feature**：移動診斷與 trace 在新架構下照常輸出（object_fps_pvp 的 Match／Client 開發者；`TraceMovement` 等資料契約不變；場次證據 C7）；Windows 上不再因檔案句柄與執行緒順序失敗。
- **Contract**：`DiagnosticsWriter<T>`（(c) 唯一 effect 例外，S-8、J-14）寫 World 端緩衝、在交換點交 sink、Interrupted 不丟（B-13）；trace 檔的關閉契約：flush 與檔案句柄在 `Close` 返回前全部釋放、尾行順序＝寫入順序；trace／log 的檔案格式不變。
- **Architecture**：trace-diagnostics plugin（audit §10.4：`TraceSink`／`LogFile`／`MovementDiag` 服務）。Contract 撐不住的地方：今天 `MovementDiag.hpp:109` 自起 `std::thread`、全域 atomic 指標（G.4 第 3 條）——改成服務與句柄。
- **範圍**：`plugins/trace_diagnostics/`；`engine/trace` 不改（trace DI 1～8 相容）；產品 P8／P12 的服務部分（audit §1.3）；`MovementTrace.hpp` 的 `TraceMovement` 改經 `DiagnosticsWriter<T>`、時間用 tick（-detect 另記）。
- **受理判定輪廓**：**windows 觀察 (a)**：`DiagnosticFileTests.cpp:216` 形狀（檔案開著時 `remove_all`）以「Close 後無開啟句柄」的斷言固定，在測試內重複 100 次、windows-x64 L1 零「process cannot access the file」；**(b)**：`TraceSinkTests.cpp:155` 形狀的 Close 尾行順序以契約測試固定、100 次相同；J-14 的 Diagnostics 例外不擴大到其他 effect；trace 既有測試全過；`kernel_prep_sites` 白名單 G 類 13 列歸零或改登 Diagnostics（逐列）。
- **範囲の限界**：共通三條；兩條觀察的舊原因未特定，本批驗的是「新服務上不出現」（不報「修了舊的」，AGENTS §14.3）；在測試內重複 100 次看不到低於 1/100 的偶發（closeout 60 run 比較）。
- **段**：上段（公開介面 `DiagnosticsWriter`、執行緒、資料契約 owner 移動）。**模型／檔位**：實作 Opus／high（檔案生命期與執行緒順序段 xhigh）；審查 Fable／high；驗證者 Opus／high。
- **估時**：12～26 h。
- **被取代的 legacy 條文**：無（trace.md 相容）。
- **風險與停止**：windows 重跑仍有失敗且分不出實作／測試／環境→照 §14.1 切分報告，不判「已修」。**若失敗**：match-sim 的移動 trace 沒有接收端；可暫以「診斷關閉」跑 golden（digest 不含診斷），所以只擋 match-host 的合併、不擋 match-sim 的 golden 驗證。

### 3.14 `v8-product-match-sim`（PvpMatch 拆成 (c) System；重放 host 上 golden 37）

- **Feature**：玩家今天的對局判定（移動、射擊、擊殺、復活、動畫推進）在 kernel 上產生**相同結果**（Product：object_fps_pvp Match；Client 在 client-sim 重用同一組 System）。支撐的下一版 Feature：loading 畫面（Match 側機制，本批做地圖與玩家兩個 stage）、場景線第一個非玩家實體（「可互動物件」stage 的掛點）。結束時＝M2：37 個 golden 由 kernel 上的模擬產生、逐位元相同；Plan 印出 Simulate stage 的 System 順序。
- **Contract**：Data 不變（arena、golden fixture、`AuthorityDigest` 編碼）；wire 不變；Body 依 entity-model 第 139 行（a6-revision 已搬進 target/world）拆成 Body／Life／InputCursor／Connection 診斷四欄（`v8-arch-kernel-writer-unit-system`）；Player／Peer 分開；Life 的寫者形狀照 dryrun 定、使用者核准（U6）；(c) 時間用整數 ns（J-17 規則 1；-zones 已改型別）；產品 Resource 契約：`EntityAllotter`（單調不重用、generation 0）、`HostClockSample`、`PublishedReferences`、`MembershipRequests`、`MembershipCursor`、`InputRing[tick]`、`LoadingState`；digest 欄不得 Shared（LIM-4）。
- **Architecture**：產品 (c) plugin（`ObjectFpsPvp::Sim::`，naming 第 4 條）：`PvpMatch::Tick` 的寫死序 → `ApplyMembership`、生死邊界、`StepMovement` 呼叫點、`ResolveActions`、`AdvanceAnimation` 等 System；`AuthorityDigest` 成 System（依 id 升冪讀欄）；重放 host（spec H.4.4）取代今天 `AuthorityDigest` 執行器的驅動部分；`Movement.cpp` 不改（audit §2、G.4 第 3 條）。
- **範圍**：audit §1.3 P1 `PvpMatch`（重寫為 (c) plugin）、P7 `StepMovement`（留）、P9（留；`ShotReferenceAge` 改法在 match-host）、P13（留）、`src/Collision`／`src/Gameplay`（留）、`AuthorityDigest`（包成 System）；`tests/object_fps_pvp` 的 replay host 改跑 Match Runtime；搬到 -zones 卡整理出的 `Sim` 目錄。**不碰** P2～P6、P11。
- **受理判定輪廓**：(1) 件数：golden 37／比對 37／差 0，或差的只限事前宣告列名的 LIM-5 集合（§14.4 的 1～5 寫好、驗證者複核、逐條核准，U9）；(2) -detect 的 D1 兩案例**不改一字**仍過（T=35、R=215、R−T=180）；`kp-death-next-tick`、`kp-respawn-next-tick` 仍 killed；(3) J-17 確認清單（含新 Sim 檔，清單由 G-5 匯出）違規 0，`zone_scan` 結果與手寫版相同；候選清單 9 是參考值不是門檻；(4) Loading 等價（分一批與多批、資產完成順序打亂，ready 時 (c) 欄逐位元相同；J-51 的產品版）；(5) D-E2＝0 且順序邊理由沿用 dryrun；(6) `cFingerprint` 印出存證據；(7) B-1、B-2、B-3、B-5 的數字在重放 host 上量出（給 G2a）。
- **範囲の限界**：共通三條；**LIM-5**（初始成員經 Loading 玩家 stage；以對局開始為基準的 golden 預期不變——實測，不先假設）；**LIM-4**；只在重放 host（腳本時鐘）：Ingress／Judge／Egress 與 IPC 不在本批，G.4 的牆鐘讀點仍在舊 host（白名單 W／A 列不減是預期）；`PlayerAnimation` 推進的區（audit §4.5 標錯候選）等決定性線核對前照現況標。
- **段**：上段（digest、資料形式、區）。**模型／檔位**：實作 Opus／high（Life 漏斗、Body 拆欄與 golden 比對段 xhigh）；審查 Fable／xhigh（重點：搬錯區、拆欄改了寫入時序、digest 編碼）；§14.4 驗證者 Opus／high 另一代理（只在 LIM-5 集合要重設基準時）。
- **估時**：40～85 h（audit P1 40～80＋P7／P9／P13 4～18＝44～98，扣 -zones 已做搬區與整數 ns 約 20%）。
- **被取代的 legacy 條文**（重標後）：entity-model E-7、E-12、RC-2；E-3 依新定義歸「相容（措辭更新）」；entity-model 第 153 行「拆欄不在 split 做」（誤導句 §5 第 16 條）。
- **風險與停止**：golden 不逐位元、原因分析兩輪不收斂→停（§14 冒頭；§8 RC-4）；差異只在 LIM-5 預期的情境→不自己重設基準，照 §14.4 交使用者；Body 拆欄需要 Shared→停（LIM-4、K.2 第 10 條）；Life 形狀若需同窗讀自己生成的實體（K.2 第 9 條）→停。-zones 未完成或確認清單不為 0→本批不開。本批與 match-host 同一 PR 線（U3）：批內舊 `PvpMatch` 與新 (c) plugin 並存只在分支上、不進 master；並存期間任何一路的行為修改都禁止。**若失敗，match-host、client-sim 白做**。

### 3.15 `v8-arch-first-host-cost`（閘門 G2a：重放 host 上的成本）

- **Feature**：無新 Feature；AGENTS §2「他の host・Product へ広げる前に、一つの host で費用を計測し、計画の予算と比べる」的第一段。結束時＝M3：同機、同情境、新舊兩路的 tick p50／p95／max、World 記憶體、載入時間，與 go／no-go。
- **Contract**：與 pvp-baseline 同口徑、同機器、同工具鏈、同組態；報告格式照 §6；量測執行檔不進 ctest。
- **Architecture**：不變。
- **範圍**：量測執行檔改跑遷移後的 Match Runtime（重放 host；驗證關／開兩種）；`tests/common/kernel/` 的合成負載記錄執行檔（B-2）。
- **受理判定輪廓**：(1) 件数：宣告 tick 數＝記錄數（扣暖機 600 tick）、每情境 5 次都有；(2) B-1、B-2、B-3、B-5（World 位元組與快照緩衝）對 §6：全在預算＝go（放行 2' 階段：rollback-extract、engine-wrap、plugins-frame 開工）；任一 1.0～1.5 倍＝報告，使用者決定；任一 ≥1.5 倍＝停、重排（§8 RC-1）；(3) 驗證開與關各量一次，報告附開關值與排除欄清單（LIM-6）；(4) 量測前告知使用者機器負載。
- **範囲の限界**：共通三條；重放 host 沒有 IO 執行緒與交換點的真實負載——真實行程的 B-1′、RSS、B-7 在 match-host（G2b）再量；量測機若不是 Linux，結果只作代理值（U8）；L2 本機量測不是 CI 判定。
- **段**：上段（決定是否繼續疊 host，屬 Architecture 判斷）。**模型／檔位**：量測與記錄 Opus／medium；門檻來源的對抗檢查 Fable／high（看結果前）；判定由主會話 Fable／high 下；驗證者不需要。
- **估時**：4～8 h。
- **被取代的 legacy 條文**：無。
- **風險與停止**：舊路徑基線重複間 p95 差 >20%→先停下判環境，不改門檻（§14.1）。

### 3.16 `v8-product-match-host`（真實 Match 行程跑在 kernel 上；閘門 G2b）

- **Feature**：玩家今天的對戰，Match 端在 kernel 上；**用現有 Client 連 Gateway 就能玩**（wire 不變）；玩家可見的唯一差異：refusal 回覆可能晚一 tick（U7）。支撐的下一版 Feature：觀戰者（Peer 實體）、pv10 的複製層（networking §1 八條由 kernel 提供）。結束時＝M4。
- **Contract**：wire 不變（pv9）；Link 家族（net-link）；`HostClockSample`、`PublishedReferences` boundary Resource（G.4 第 1 條：射擊 reference age 保 pv7 D-3「在 tick A 量一次」）；連線品質 reference age 用 ring 的到達時刻（G.4 第 2 條）；`RequestMovementResync` 改由 Judge／Simulate 的 (c) System 從到達時刻算（G.4 第 4 條）；IPC 執行緒不碰 World（G.4 第 5 條＋-detect 補列）；`connectionQualityFailures` 成為 Player 的 Connection 診斷欄、Judge（(c)）唯一寫者（-detect D3）；refusal 的回覆時機——**事前宣告第一步查證 pv9 契約與 Gateway 測試有無規定**（spec 附錄二「未查證」），有規定→停。
- **Architecture**：headless-runner 上跑 Match Runtime；`MatchRuntimeHost` 的時鐘、執行緒、交換點留在 runner（H.7）；`:386-440` 的交接成 Ingress（`StageMembershipRequests`、`StageInputs`）、`:450-451` 成 Judge（(c)）、`:471` 起成 Egress（spec L.3）；`IpcHost` → net-link 的 IO 服務（只處理位元組與到達時刻）；`match_main` → `project.json` 的 plugins 資料＋Run（audit §10.2 S1）；H-7 守衛由產品持有（spec J.4）。
- **範圍**：audit §1.3 P2、P3、S1（Match 半）；P8／P12 接 diagnostics-service；-detect 補列：`IngressBucketId`／`RecordRejection` 的 `ContainsPlayer`／`Snapshot()` 搬進 Ingress、`match_main` 的 `TakeIngressStatistics` 改經交換點複製的 Host 統計、`TickCount()` 改讀 World 的 domain tick（Ready 狀態 Host 讀，A.2 允許）、`QueueActionAcknowledgement`／`SubmitActions` **刪除**（無正式呼叫者，§7；刪 TEST_CASE 前逐 CHECK 看）、`IpcHost` 的 `GetActionResults` ×4 與 `NoteWireRejection` ×2 改讀 `Link.outbound`；刪舊 `PvpMatch`（結束並存）。
- **受理判定輪廓**：(1) `kernel_prep_sites` 白名單：A 類 22 列→0、G 類→0（或 diagnostics-service 已改登）、W 類只剩 runner／Host 檔，件数照 47 列逐列對帳、每刪一列記 dev_log；(2) H-7 掃描 Match 0 違規（正例先命中；白名單只有 `Run`／交換點）；(3) golden 37（同 match-sim 的集合規則）；(4) Gateway go 測試（`state_whitelist_test.go` 等）、pvp 57 測試、acceptance 30 項全過（network 只在本機）；(5) D3 host 層特徵（W=600、1200／1800／2400）原樣過；Match 層「`Snapshot()` 的 failures 恆 0」是舊結構特徵，改寫照 §14.4 寫 1～5（無緩める面則事後報告）；(6) G.4 第 1、2 條「golden 預期不變」以 digest 實測；(7) **B-1′、B-5 RSS、B-7 在真實 Match 行程（Linux）實量**＝G2b 的數字，與舊 binary 同情境比較；(8) 統計行印 tick p50／p95／max（field-session C3 形狀）。
- **範囲の限界**：共通三條；**LIM-5**；refusal 回覆時機改變（pv9 未規定時記為行為差異，不是 Defect）；Windows 不是 Match 的主平台（觀察 (c) 的 Client 側在 client-sim）。
- **段**：上段（協定時機、執行緒、State Authority、事故類 Ingress）。**模型／檔位**：實作 Opus／high（Ingress 的序號配發、Judge 的時序與交換點段 xhigh）；審查 Fable／xhigh；§14.4 驗證者 Opus／high 另一代理。
- **估時**：60～120 h。
- **被取代的 legacy 條文**：architecture.md L138（`RuntimeLoop` 三段）的 Match 側、L140-155（product host 擁有迴圈）；entity-model E-12（舊 `PvpMatch` 刪後對任何程式都不再成立→標「已被取代（v8-product-match-host）」）；network-architecture 第 225-227、1034-1057 行（誤導句 §5 第 18、19 條，加註）。
- **風險與停止**：pv9 規定 refusal 同步回覆→停（wire 不在本計畫內改）；`ShotReferenceAge` 改 `HostClockSample` 後 digest 變→停（audit §3.4 第 1 條）；spec K.2 第 7 條（H.7 以外的協調邏輯回到 runner）→停；B-1′ ≥1.5 倍→停（§8 RC-1）；實測工時 >1.5×估計且 >重寫估計→K.2 第 8 條，停（§8 RC-5）。與 match-sim 同一 PR 線合併（U3）。**若失敗，G2b 判不了，階段 3、4 全部不開**。

### 3.17 `v8-arch-kernel-rollback-extract`（回滾與 Extract）

- **Feature**：Client 預測與回滾由機制提供，不由產品手寫重放迴圈（object_fps_pvp Client；玩家可見：本地移動即時、權威修正平順）；渲染只讀模擬（使用者 K.3 第 6 項）；Editor 預覽資料只往前流（三個有預覽的工具）。暫停中的 hitbox-rewind 重開後用同一回滾設施。結束時開發者看到合成 FixedUpdate 跑 10 tick 與「跑到 5、Rewind、Replay 回 10」逐位元相同；`Extract<Write>` 編譯失敗。
- **Contract**：H.5：`Rewind`／`Restore`（部分快照、整欄替換、不觸發 hook）／`Replay`、PredictionHistory 由 Runtime 自動存（深度由登記定）；快照含 domain tick、`rollback = Yes` 的欄與 Resource、配發器、Fixed 事件緩衝；不含 role=Input（Z8）、Derived（D-E16）；D-E15 逐掛點。H.6：渲染 World 自己的 Registry、`Extract<Read<T>>`／`ExtractRes<R>` 只讀、`Extract<Write>` 編譯失敗、來源 Registry 先凍結（D-E14）、同一執行緒（v1）。
- **Architecture**：§4 Delta-1 內；無新邊。
- **範圍**：`engine/kernel/`、`tests/common/kernel/`（合成型別）。
- **受理判定輪廓**：J-42、J-42b、J-42c、J-43（含編譯失敗，形狀照 scaffold）、J-46、J-49 三平台；突變優先清單第 10、15 條 killed；B-6 在合成資料上記錄。
- **範囲の限界**：共通三條；v1 回滾集合由產品登記（可只含本地玩家，H.5「與今天的差距」）；K.2 第 11 條（J-42 因 Kernel 以外原因失敗＋E0 結論）→停、不追（DET 暫停）。
- **段**：上段（決定性、公開介面、DI-KS 12／13）。**模型／檔位**：實作 Opus／xhigh；審查 Fable／xhigh；驗證者 Opus／xhigh。
- **估時**：24～44 h。
- **並行**：G2a 後開工，與 match-host 並行（不同目錄；match-sim／host 不用回滾）。
- **被取代的 legacy 條文**：無直接條文（`LocalPlayerPrediction::Reconcile` 的形狀在 client-sim 取代；determinism.md §3 在 client-sim 標）。
- **風險與停止**：J-42 在某平台不等價且原因在 Kernel 外→停、報告（不放寬「逐位元」）。**若失敗**：client-sim、client-frame、plugins-editor 白做。

### 3.18 `v8-arch-engine-wrap-render`（Engine 的「包」：render、input、platform、text、audio）

- **Feature**：Client 與工具的渲染、輸入、平台、文字、音訊以 Resource 或 Host 零件接進機制（跨產品能力：object_fps_pvp Client、四工具）。
- **Contract**：render 的 Resource 親和 `GraphicsContextOwner`（spec E.3）；`PhysicalInputFrame` 是 boundary 資料（audit §1.1 input）；`PlatformEvents` boundary；audio `Submitter` 帶親和；公開介面變更列在事前宣告（AGENTS §3「Public Interface の大規模変更」）。
- **Architecture**：audit §1.1 判定照做：render 包（先收攏 4 處跨元件指標）、input 後端 Host＋core 留、platform Host、text 包、audio 服務；§4 Delta-2 的差分（這些 Engine 零件的消費者是 plugins）。
- **範圍**：`engine/render`、`engine/input`、`engine/platform`、`engine/text`、`engine/audio`。
- **受理判定輪廓**：各模組既有測試全過；platform-sdl G1～G3、input IP-1、`gyo_expect_*_links` 不變（legacy-impact §4 第 9、16、17 列）；跨元件跨幀裸指標 4 處（`Renderer.cpp:134-135`、`SdlGpuRenderDevice.cpp:117`、`UiRenderer.cpp:147-149`、`ModelRenderer.cpp:22`）→0；naming 第 5 條後綴掃描新程式零命中；render.* 在 Linux Lavapipe 綠。
- **範囲の限界**：共通三條；GPU 相關只在 Linux Lavapipe 與本機。
- **段**：上段（公開介面）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high。
- **估時**：19～53 h（audit 不調）。
- **被取代的 legacy 條文**：platform-sdl RC 2 的例外名單改名在 plugins-editor 處理。
- **風險與停止**：包需要改 platform-sdl DI 7（observer 與 live frame 內不得做 operation）→停（Delta）。**若失敗**：plugins-frame、client-frame 的畫面側沒有零件。

### 3.19 `v8-arch-engine-wrap-assets`（資產三類 Resource；UiRenderer 局部重寫；io）

- **Feature**：資產照 Bevy 模型載入並顯示，Loading 頁面能顯示進度（object_fps_pvp Client、四工具；已點名 Feature：loading 分批載入的資產階段）。
- **Contract**：target/assets §1、DI 1～3：請求狀態 Resource、每型別 `Assets<T>`（Handle 引用計數）、`RenderAssets<T>`、loader 由 plugin 登記、載入是外部服務（IO 執行緒＋交換點）→`AssetInbox`、完成依請求序號連續前綴套用（J-51）、內容身分 (c)（determinism §4.9 digest 不變）；`WriteFileAtomically` 只從 `Io:file` 的 (b) System 呼叫。
- **Architecture**：`AssetManager` 物件消失（audit §3.6）；`UiRenderer` 局部重寫為 System＋`Gpu*` Resource（naming 第 5 條）；`WeaponViewModel` 的跨 World 指標隨之消失。
- **範圍**：`engine/asset`、`engine/ui`、`engine/io`、`engine/model`（loader 登記）。
- **受理判定輪廓**：J-51 用真資料情境；`AnimationContentDigest`／`ArenaContentDigest` 不變；掃描「無 `AssetManager*`／`&` 成員」零命中（assets DI 1）；資產等價測試（打亂完成順序，`Assets<T>` 與 Handle 序列相同，DI 2）；ui 既有測試全過；改名清單（target/assets §2）在宣告列出。
- **範囲の限界**：共通三條。
- **段**：上段（公開介面消失、資料契約 owner）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high。
- **估時**：19～48 h。
- **被取代的 legacy 條文**：architecture.md L108-132 模組表 Framework 列與 L301 `AssetManager`；ui_toolchain 第 6-16 行圖（誤導句 §5 第 22 條，文件線）。
- **風險與停止**：**若失敗**：client-frame 與工具批的資產路徑沒有落點；停。

### 3.20 `v8-arch-plugins-frame`（視窗側的 plugins：sdl-window-runner、asset-catalog、event-sounds）

- **Feature**：Client 與四工具共用一個 SDL 視窗 runner，不再各寫 pump 與 live frame（object_fps_pvp Client、四工具）。
- **Contract**：runner plugin 在 finish 安裝（R-E6）；`PlatformEvents` boundary；catalog plan 與 event sounds 是資料契約，owner＝提供該 plugin 的人（audit §10.1；DI 照搬，owner 改）。
- **Architecture**：§4 Delta-2 的三個 plugin（audit §10.4 候選）。
- **範圍**：`plugins/sdl_window_runner/`（`PvpApplication::ProcessEvents` 與 `SdlPlatform` 的 pump、live frame、`SdlInput` 後端、對話框回覆）、`plugins/asset_catalog/`、`plugins/event_sounds/`；framework 的對應程式**先不刪**（工具還在用）。
- **受理判定輪廓**：刪任一 plugin 只壞 audit §10.4 表列的消費者；event-sounds／catalog 資料契約 DI 的守衛搬到 plugin 後仍綠；一個合成 Client 形狀（Frame＋Render World）用這些 plugin 開窗畫一幀（本機）；live frame 以 run 條件表達（spec H.4.2）。
- **範囲の限界**：共通三條；framework 與 plugin 兩份 runner 並存期間＝「兩種動詞並存」（K.2 第 8 條），限定在工具遷完前、framework-delete 結束。
- **段**：上段（plugin 邊界、runner）。**模型／檔位**：實作 Opus／high；審查 Fable／high。
- **估時**：15～30 h。
- **被取代的 legacy 條文**：framework/event-sounds 的資料契約 DI 不取代（owner 隨 plugin 搬，HANDOFF 2026-11-12 S-8 解讀）。
- **風險與停止**：live frame（pump 重入）在 runner 下無法表達成 run 條件→停。**若失敗**：client-frame、工具批沒有視窗 runner。

### 3.21 `v8-product-client-sim`（Client 的模擬與預測跑在 kernel 上；閘門 G3）

- **Feature**：玩家的本地預測與對帳在 kernel 上，手感不變；跨產品能力完成——Match 與 Client 登記同一組 (c) System、`cFingerprint` 相同（H-4）。結束時開發者看到預測與對帳測試在新路徑原樣過、Windows CI 的 pvp.cpu 重跑 0 次 RoleThread／wait 卡住。
- **Contract**：ClientSim Runtime（H.4.2）：Loading（同 Match 的 stage 序、manifest 序號同源，H.2b 第 8 點）→ ready 後以第一個權威快照 `Restore`（含成員與 `tick[Fixed]`）→ FixedUpdate（rollbackEnabled、Serial、runner＝`gyo-client-sim`）；Ingress 全部 `NotInRollback`；權威快照不走 System（Host 在交換點解碼成部分快照）；wire 不變；ready 以「Welcome 隱含」實現（§1.3）。
- **Architecture**：`ClientSimulationRole`／`ClientSimulation` 重寫成 ClientSim Runtime＋runner 的時鐘策略（audit P5：「何時跑下一步」是 Host）；`LocalPlayerPrediction` 重寫為回滾機制的使用者（P6）。
- **範圍**：audit §1.3 P5、P6；`tests/object_fps_pvp/`。
- **受理判定輪廓**：(1) 產品版 J-42（PvP 的 FixedUpdate 直接跑 k tick 對 Rewind＋Replay，回滾集合逐位元相同）三平台＝**G3**；(2) `cFingerprint` Match＝Client（H-4 產品守衛）；(3) 預測與對帳既有測試不改全過；預測修正次數與最大修正量在固定劇本下與基線相同（C5 欄位）；(4) **windows 觀察 (c) 收口**：`object_fps_pvp.cpu` 在 windows-x64 以 workflow_dispatch 重跑 10 次 0 Timeout，任何一次停在 `role started … wait_backend=` 之後＝不成立；輸出含 doctest 案例名（緩衝問題一併處理）；(5) H-7 掃描 ClientSim 0；(6) D2 棘輪 Client 側（`LocalPlayerPrediction.cpp:21`、`:31` 的全域）歸 0；(7) B-8 的模擬 tick 項。
- **範囲の限界**：共通三條；回滾集合 v1 只登記本地玩家的欄；DET 暫停、win1 的 ULP 差異不在本批；既有 wire 不夠表達 manifest→停（§8 RC-9）；windows 10 次看不到低於 1/10 的偶發（closeout 60 run）。
- **段**：上段（決定性、回滾、執行緒、回歸風險）。**模型／檔位**：實作 Opus／xhigh；審查 Fable／xhigh；驗證者 Opus／high（若預測測試判定要改）。
- **估時**：50～100 h。
- **被取代的 legacy 條文**：determinism.md §3（`Reconcile` 只重放本地玩家）→ target 回滾機制；network-architecture 第 180-181 行（Client 角色決定執行緒存活 → runner）。
- **風險與停止**：W-3 的 timeout 在新 runner 仍出現→§14.1 切分，切不出就停，不靠加 TIMEOUT 通過（§14.4）；Client 與 Match 的指紋不同而要在 System 內分支產品角色→停（違 DI-KS 15）。**若失敗（G3）**：client-frame 不合併（它依賴 ClientSim 的呈現 mailbox）。

### 3.22 `v8-product-client-link`（ClientConnection → net-link）

- **Feature**：PvP Client 照舊連線（玩家可見：無變化）；模擬執行緒不再直接呼叫 `SendInput`。
- **Contract**：Link 家族（net-link）；asio 的 IO 執行緒只處理位元組與到達時刻；Running 期間零外部存取；wire 不變。
- **Architecture**：`ClientConnection` 重寫為 net-link 的 Client 端服務＋Link；狀態→Resource（audit P4）。
- **範圍**：audit §1.3 P4。
- **受理判定輪廓**：Link 家族「Running 期間零外部存取」測試（交換點計數）；network 標籤測試本機全過；`client_delay_runner` 等既有 runner 的輸出欄位不變；IO 執行緒不碰 World（掃描＋審查）。
- **範囲の限界**：共通三條；network 測試只在本機。
- **段**：上段（wire 邊界的執行緒、外部服務）。**模型／檔位**：實作 Opus／high（執行緒交換 xhigh）；審查 Fable／high；驗證者 Opus／high。
- **估時**：22～40 h。
- **並行**：與 client-sim 並行需要第二張產品卡（U11）；不開時接在 client-sim 後，關鍵路徑加 22～40 h。
- **被取代的 legacy 條文**：network-architecture §7 入口表（誤導句 §5 第 19 條，表頭加註）。
- **風險與停止**：**若失敗**：client-frame 的 Link swap 沒有實作；停。

### 3.23 `v8-product-client-frame`（Client 的畫面與輸入；G3 後與 client-sim、client-link 同一 PR 線合併）

- **Feature**：玩家看到的畫面、輸入、UI、命中回饋、音效在 Frame／Render World 上，與今天一致；loading 畫面在 ready 前顯示（H.2b 的 Feature 開始成立）；Editor 與 Client 同形（runtime-composition §3b 第 4 點）。結束時＝M5：`PvpApplication` 只剩 plugins 資料＋Run。
- **Contract**：ClientFrame（FrameUpdate，runner＝Main；`PlatformEvents` boundary、`FrameTime` external）、ClientRender（Extract→RenderSubmit，親和 `GraphicsContextOwner`）；Mailbox 意圖 F→S、呈現 S→F 只在 schedule 之間存取（H.4.2）；live frame 時 `Input`／`Intent` 被 run 條件跳過。
- **Architecture**：`PvpApplication` 重寫成 plugins 資料＋Run＋產品 (b) System（P11）；P10 呈現包（時鐘由 `FrameTime` 傳入）；`src/App` 的 `WeaponViewModel` 先拆再包；呈現 System 放模擬之後的 stage（LIM-1 慣例）。
- **範圍**：audit §1.3 P10、P11、`src/App`、S1（Client 半）。
- **受理判定輪廓**：(1) H-6／J-43 產品版（呈現不寫 S、R 不寫 F）；(2) 呈現 System 的 D-E8＝0 且 LIM-1 成本記錄（呈現 System 數、各在哪個 stage）；(3) D2 棘輪 Client 側 0；H-7 掃描 Client host 0；(4) client 驗收（acceptance）全過；(5) **Windows 實機一場**（CI 做不到的視覺部分；memory ci-is-primary-standard、offline-session-scripts）；(6) B-8（Client 幀 p95 ≤基線×1.10，基線在本批開工前另量）；(7) `native_window_evidence`、`action_runner` 在 windows-x64 L1 的逾時次數只記錄、不作門檻（觀察 (2) 的疑似項）。
- **範囲の限界**：共通三條；**LIM-1**；macos-x64 偶發只記錄（memory platform-priority）。
- **段**：上段（Client host 骨架＝runner plugin、多 World 組合、回歸）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high（若畫面相關判定要改）。
- **估時**：61～122 h。
- **被取代的 legacy 條文**：architecture.md L138 的 Client 側；platform-sdl RC 2（例外名單改成 imgui plugin target——在 plugins-editor 做）。
- **風險與停止**：B-8 >1.5 倍→§8 RC-1；live frame 期間需要跑 Intent 才能保持今天行為→停（spec H.4.2）。

### 3.24 `v8-arch-plugins-editor`（Editor 側的 plugins）

- **Feature**：四個工具的視窗、ImGui、預覽、選擇窗照舊（工具使用者可見：無變化；預覽不再晚一幀、無 readback，`v8-arch-editor-forward-only`）；四工具的選擇窗記帳與關窗提示只有一份（架構師保留：必須真的進共用 plugin）。
- **Contract**：imgui plugin 兩半（文件 World 的輸入與 widget System；渲染 World 的 draw list 繪製；ImGui context 全域明列例外 I-5）；offscreen-preview 在渲染 World 合成、預覽視窗＝draw list 的貼圖 id（runtime-composition §3b）；editor-dialogs（三種記帳統一、關窗提示；行為差異寫進宣告，§1.3）。
- **Architecture**：§4 Delta-2 的延續；preview 資料契約 DI 1～3、5 照搬、DI 4 已被取代。
- **範圍**：`plugins/imgui/`、`plugins/editor_dialogs/`、`plugins/offscreen_preview/`；framework `ImGuiShell`、`OffscreenPreview`、`PreviewCamera`／`PreviewTexture`／`UiTextInput`／json binding（成函式庫，放哪個 target 本批定）；`tests/framework/PreviewTests.cpp` PV2～PV5 與 pv-* 突變**重寫不搬**（衝突 2 處置）。
- **受理判定輪廓**：H-6 原樣（無例外、無晚一幀）；合成 Editor 組合的 Extract 測試；每個 plugin 的刪除驗證（刪它只壞依賴它的工具）；editor-dialogs 被 ≥2 個工具使用、三種記帳統一後四工具的對話框測試全過；重寫的 PreviewTests 以突變證明檢出力（§14.4 驗證者）；三平台綠。
- **範囲の限界**：共通三條；model_viewer `ViewerWindow.cpp:112` 缺陷不修（統一記帳若順手修好＝行為變更，宣告寫明交使用者）；預覽視覺只在本機與 Lavapipe。
- **段**：上段（plugin 邊界、資料契約 owner 移動）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high（PreviewTests 判定改寫屬 §14.4，有緩める面就要使用者核准）。
- **估時**：20～40 h（audit framework 33～66 扣 plugins-frame 已做的 runner 與 catalog）。
- **被取代的 legacy 條文**：preview.md DI 4（衝突 2）、RC 4；model_viewer RC 1；platform-sdl RC 2；framework/architecture DI 1～6、RC 全部（隨 framework-delete 刪檔）。
- **風險與停止**：選擇窗記帳三種實作語義不同、無法統一→停、報告；不在 plugin 內放三份。**若失敗**：四個工具批沒有 Editor 骨架。

### 3.25～3.28 工具四批（`v8-product-tool-voxel`、`-tool-model-viewer`、`-tool-audio`、`-tool-ui`）

共通（每批一節的欄位，差異列在表）：
- **Feature**：該工具的編輯功能照舊（工具使用者可見：無變化）；voxel 是 Editor 形狀（文件 World＋渲染 World）的第一個樣本（README「-audit 的起點」），結束時工具使用者用新版編一張圖、存檔照 `gyo.voxel_map`、Match 讀得進。
- **Contract**：工具的 `project.json` 加 plugins 欄、main＝Run；工具專有的編輯 System 進該工具自己的 plugin；文件 World（兼 Frame）＋渲染 World；存檔載入是 `Io:file` 的 (b) System、經各自資料契約（`gyo.voxel_map` 等，AGENTS §8，不變）；工具守衛正向清單改掛 plugin target（legacy-impact §4 第 11 列）。
- **Architecture**：應用 Architecture（工具內部；AGENTS §2「応用」）——Resource 與 System 拆法依觀測，不預先一般化；形狀由 voxel 定、後三個照做。
- **受理判定輪廓**：工具既有測試與依賴守衛全過；main 只有 Run（掃描）；H-7 掃描對工具 host 0；預覽無 readback。
- **範囲の限界**：共通三條；工具全 zone=B。
- **段**：voxel **上段**（第一個 Editor 樣本，影響後三個）；其餘三個**下段**。**模型／檔位**：實作 Opus／high；審查 Fable／high（voxel）／Fable／medium（其餘，只看 plugin 依賴清單與守衛）；下段批的檔案搬移與 CMake 清單更新等 low 定型作業可用 Sonnet／low（navigator：下段的 low 作業，計畫寫明）。ui_editor 若要改 `GYO::Ui` 介面→升上段、停下重排。
- **風險與停止**：同一工具出現第二套組合規矩（framework 老問題換名）→停，回 plugins-editor。三工具三卡並行。

| 代號 | 範圍（audit §1.2） | 差異 | 估時 | 被取代的 legacy 條文 |
|---|---|---|---|---|
| `v8-product-tool-voxel` | client Host 重寫；`EditSession`、`MapPreview`、`PreviewScene` 包 | 第一個樣本；W6／W7 守衛的觀測手段在 plugin 版重寫（legacy-impact §9 第 8 點） | 24～48 | voxel_map_editor DI 1；design.zh-Hant 四處措辭 |
| `v8-product-tool-model-viewer` | client Host；預覽與音訊包 | `pendingAction_` 跨幀捕獲 `this` 改成資料 | 16～32 | model_viewer DI 1、2、4；RC 1 |
| `v8-product-tool-audio` | client Host；`PreviewPlayer` 包 | — | 8～16 | audio_editor DI 1 |
| `v8-product-tool-ui` | client Host；`EditorApp` 重寫（介面） | `EditorApp` 存兄弟物件參考 | 32～56 | ui_toolchain ui_editor DI 1～3 |

### 3.29 `v8-arch-framework-delete`

- **Feature**：framework 概念退役完成（`v8-arch-framework-retire`）；AGENTS §7 完整刪除；Product removability 的第二次實測。使用者：開發者。
- **Contract**：無新契約（資料契約已在 plugins-frame／editor 搬走）；刪 `FRAMEWORK` component（build 層資料）。
- **Architecture**：§4 Delta-2 的收尾。
- **範圍**：`framework/`、`docs/framework/` 三份（同一 PR，`v8-arch-docs-legacy`）、`tests/framework/`、`GyoComponents.cmake` 的 `FRAMEWORK`、`GyoBuild.cmake` 的 `GYO_ENABLE_FRAMEWORK`、`ToolRegistryTests.cmake` 的保留名、`framework_dependency_guard.py`（legacy-impact §4 第 10～13 列）。
- **受理判定輪廓**：-remove-object-fps 五條判定同法（刪檔逐檔相同、共通層只動已列的檔、殘留 grep 用 `git grep -P`／Python 不丟 stderr、空目錄 0、ctest 名單差異只有刪掉的）；共通層改動逐條列（預期非零：build 層的 FRAMEWORK 是 Delta-2 影響檔）。
- **範囲の限界**：無新增。
- **段**：上段（Build Graph、Product Registration 保留名）。**模型／檔位**：實作 Opus／medium（-remove-object-fps 實作 27 分）；審查 Fable／high。
- **估時**：4～8 h。
- **被取代的 legacy 條文**：framework 三份全文（刪檔）；AGENTS §2 L96「framework の共通構造」（交風紀委員改「plugins 層の共通構造」）；cross-component-rules 定義句（誤導句第 33 條）。
- **風險與停止**：共通層要改的檔超出宣告清單→停、記 Delta。

### 3.30 `v8-arch-ci-spike-retire`

- **Feature**：CI 只有一套 kernel 證據（ci-readiness §2.6）。使用者：開發者。
- **Contract**：無。**Architecture**：刪 top-level `spikes/`（AGENTS §7 不留空目錄）。
- **條件**（兩者都要）：(a) world-exec 的 J-12 與 J-5 兩層在 L1 三平台綠；(b) pageguard-os 完成（`PageGuardTests.cpp` 已搬）或 U1 定不採。只滿足 (a) 時 spike 步驟仍是頁保護三平台證據的唯一來源，不能刪。
- **範圍**：`spikes/kernel/` 整個目錄；`.github/workflows/cross-platform.yml` 的步驟本體、上傳路徑兩行、摘要列與 env（**由使用者改**）。報告與 dev_log 的歷史路徑不改寫，README 加一句「已於 PR # 移除」。
- **受理判定輪廓**：兩條件逐條勾；`spikes/kernel` 引用 0（歷史 dev_log 除外）；L1 綠。
- **段**：上段（CI 結構）。**模型／檔位**：實作 Opus／low（逐字刪除）；審查 Fable／medium。
- **估時**：2～4 h。
- **被取代的 legacy 條文**：無。
- **風險與停止**：無。

### 3.31 `v8-arch-runtime-loop-retire`

- **Feature**：舊的三段呼叫序在 repo 中消失，任何 host 都不能「一半在機制上」（H-7）。使用者：開發者。
- **Contract**：無。**Architecture**：audit §1.1 runtime 列的重寫收尾；`FixedTickRuntime` 留給 runner plugin（Host 零件）。
- **範圍**：`engine/runtime` 的 `RuntimeLoop`、`IRuntimeClient` 與測試。
- **受理判定輪廓**：include 0；H-7 守衛 (a) 改為「檔案不存在」也過；`engine_tests` 案例數只減被刪的部分（逐案例對照；刪前看每個 CHECK，-remove-object-fps S1 教訓）。
- **段**：上段（公開介面刪除）。**模型／檔位**：實作 Opus／high；審查 Fable／high。
- **估時**：6～15 h（audit runtime 8～20，只刪不寫）。
- **被取代的 legacy 條文**：architecture.md L138（`RuntimeLoop` 三段）——此時對任何程式都不再成立。
- **風險與停止**：仍有使用者（工具或 Client 未遷完）→不開。

### 3.32 `v8-arch-migration-closeout`

- **Feature**：所有人看到一套架構文件；舊概念（framework、RuntimeLoop、IRuntimeClient）不存在；windows 觀察結案或重開有依據。結束時＝M7。
- **Contract**：`gyo.zone-c-sources` 由 Kernel 匯出接手後，v1 是否停用（zone-c-sources §4）。
- **Architecture**：不變。
- **範圍**：legacy 九檔逐部件刪、target 搬回 `docs/architecture/`（target/README 命名規則；每部件的搬回**隨該部件最後一批**做：world／system-contract／scheduler／determinism／error-handling 隨 runtime-loop-retire；assets 隨 wrap-assets；runtime-composition 隨 framework-delete；本批是最終掃描）；LIM 逐條搬或標 Lifted；Status 行刪；Windows CI 以 `build/evidence/ci-windows-cpu-flaky/tally.md` 的同一方法掃最近 60 個 run 比較（HANDOFF〈觀察〉處置）；macOS Quick acceptance 被 cancelled（觀察 (4)）一併列、不追。
- **受理判定輪廓**：同名並存 0；`RuntimeLoop` 引用 0；反向依賴 91 處（legacy-impact §6）0 斷鏈；LIM 狀態與資料夾一致；重掃比較表；圖書管理員 checkup 項全勾。
- **範囲の限界**：無新增。
- **段**：下段（文件、執行）。**模型／檔位**：實作 Opus／medium；審查圖書管理員（主會話模型）；重掃計數可用 Sonnet／medium（low 定型）。
- **估時**：10～20 h。
- **被取代的 legacy 條文**：legacy 九檔刪除。
- **風險與停止**：重掃發現新的 Windows 專屬失敗型→不在收尾修，記觀察、交使用者。

### 取捨（§3）

- **Life 的寫者形狀**：價值案建議「`ApplyLife` 一個 System 兩個掛點（復活在 Simulate 開頭、死亡在 `ResolveActions` 之後）」，成本案寫「ApplyLife 同時處理復活與死亡兩個 Plan 位置」——兩者都是**同一 schedule 掛兩次，spec B.1 明文是啟動錯誤 D-E19**（`ApplyMembership` 的兩個掛點之所以合法，是因為在 Loading 與 FixedUpdate 兩個不同 schedule）。本稿採風險案：形狀交 dryrun 以 finish 結果定，候選只剩「Life 唯一寫者在 tick 開頭＋死亡寫獨立中間欄」或「拆 Life 為 `LifeState`＋`DeathRecord`、digest 組裝讀兩欄」（§7 U6）；「事件＋下一 tick 套用」會改 golden，不建議。放棄的代價：拆欄多一欄、digest 組裝多一步讀。
- **Windows 觀察的驗收數字**：風險案 N＝100／200 在測試內重複、價值與成本案 workflow_dispatch 重跑 10 次。本稿兩者都用：測試內重複（便宜、一次 CI 就有）當批內門檻；CI 重跑 10 次只用在 client-sim 的觀察 (c)（它的卡點在整個測試執行檔的啟動路徑，測試內重複看不到）；closeout 以 60 run 統計比較。
- **net-link 的時點**：成本案在 match-host 前獨立做；風險案在 client-link 併 Match 側；價值案在 match-host。採成本案（§2 取捨）。
- **first-host-cost 的段**：風險案下段、價值與成本案上段。採上段：它決定是否繼續疊 host，是 Architecture 判斷；量測執行本身 medium。

---

## 4. §3 Architecture Delta 七項（計畫層寫一次；各批只引用）

### Delta-1：Kernel 進 Engine（使用者已同意，K.3 第 7 項 `v8-arch-kernel-delta-approved`、`v8-arch-kernel-bring-into-gyo`）

1. **Feature**：Match 權威與 Client 預測共用同一份模擬核心（跨產品能力，object_fps_pvp 的兩個 Product）；loading 畫面與分批載入（H.2b）；Editor 與 Client 同形的組合（四工具）；規則變機制（開發者可見的啟動錯誤）。
2. **現在的問題**：每個 host 一套手排順序、四個工具 client 各一套、framework 在中間補（audit §1.4、§3.1；使用者「沒有辦法使用統一的邏輯來執行工作」）；`PvpMatch::Tick` 寫死四段、`MatchRuntimeHost::Step` 一把鎖包住全部；IO 執行緒與牆鐘直接碰權威（G.4、-detect 白名單 A 22 列／W 12 列）。
3. **邊界**：Engine 多一個子系統 `engine/kernel`（World／System Contract／Registry／Scheduler／Executor）；architecture.md 的禁令改為「Kernel 進 Engine；plugin ABI 與動態載入仍禁」（已改）。
4. **影響**：`engine/kernel`（新）、`engine/entity_store`（Delta-3）、`tests/common/kernel`（新）、`engine/CMakeLists.txt`、`tests/common/CMakeLists.txt`（各 1 行）、`engine/runtime`（RuntimeLoop 刪）；之後每個遷移的 host（object_fps_pvp Match／Client、四工具）與 plugins。
5. **Dependency**：`GYO::Kernel → GYO::Base`（＋`GYO::EntityStore`；Delta-4 成立時 `GYO::Os` PRIVATE）；產品與 plugins → Kernel；Kernel 不依賴任何能力模組、plugin 或產品（J-9）。
6. **Ownership**：呼叫順序的權威從各 host（`MatchRuntimeHost`、`ClientSimulationRole`、`PvpApplication`、`RuntimeLoop`）與 framework 移到 Plan；判定用狀態的權威從產品物件（`PvpMatch` 的 vector、`MatchRuntimeHost::connections_`）移到 World 的欄（每欄一個寫者 System，AGENTS §3 State Authority 的機制版）；host 只剩 H.7 的職責。
7. **更小的方案**：在各 host 內各自整理順序、加守衛或特例——正是 v7.1 事故後的路，使用者明示不採（「破船上打補丁」）；framework 補 Kernel 的職責已被否決（Delta-2）。
- 各批差分：kernel-scaffold（目錄與 build 2 行）、registry-schedule（公開型別）、world-exec、loading、threading、rollback-extract（各自加的公開型別列在事前宣告）。

### Delta-2：framework 退役 → `plugins/`（使用者已同意，`v8-arch-framework-retire`；AGENTS §10 新 top-level）

1. **Feature**：Match／Client／工具的組合是「plugins 資料＋Run」；四工具的視窗、ImGui、預覽、對話框；PvP 的 event-sounds；Match 的 headless 啟動；預覽資料只往前流；共用的對話框記帳只有一份。
2. **問題**：framework 承擔「組合 Engine 能力＋對接第三方」，但它本身是一套 host 骨架（WindowShell 擁有 RuntimeLoop），與 H-7 不相容；`ImGuiShell` 寫死幀序、readback 違 H-6；選擇窗記帳三種實作（audit §3.6、framework-tools 觀察 3）；framework 在代替不存在的 Kernel（HANDOFF 2026-11-10）。
3. **邊界**：新 top-level `plugins/`；三層改為 Engine／plugins／產品與工具；`framework/` 刪除；framework DI 整節廢止（資料契約 DI 不廢止，owner 隨 plugin 搬）。
4. **影響**：`framework/`（刪）、`plugins/`（新）、四工具、object_fps_pvp 的 Match／Client main、`build/cmake`（`GyoBuild.cmake` 加 plugins、`FRAMEWORK` component、`GYO_ENABLE_FRAMEWORK`）、`tests/framework/`、`framework_dependency_guard.py`、四支工具守衛、`ToolRegistryTests` 保留名、`docs/framework/` 三份（legacy-impact §4 第 10～13 列）。
5. **Dependency**：產品／工具 → plugins → Engine；plugin 之間只有宣告的正向清單（audit §10.4 表）；Engine 永不 include plugins。
6. **Ownership**：組合格式（catalog plan、event sounds）的資料契約 owner＝提供該 plugin 的人；runner（H.7 職責）由 runner plugin 持有；葉子函式庫與層無關。
7. **更小的方案**：改 framework DI 2／6、重寫兩個 shell（audit 原提案）——使用者不採（「思想不變，修改就是在破船上打補丁」）。
- 各批差分：plugins-foundation（top-level＋headless-runner）、net-link、diagnostics-service、plugins-frame、plugins-editor、framework-delete。

### Delta-3：entity_store 在 World 之下（使用者已同意，K.3 第 7 項第 (2) 點）

1. **Feature**：World 的欄依 id 升冪的疊代順序（決定性 (c) 的 World 契約，PvP digest；DI-KS 3），沿用已驗證的演算法。
2. **問題**：World 要 keys／值／stamps 平行陣列同步搬動，`EntityColumns` 把鍵放在元素內、沒有回傳位置的查找（spec A.6）。
3. **邊界**：`engine/entity_store` 成為 World 欄的實作；DI 4 延伸到 insert／remove；DI 5 對 store 本身仍成立；`DeferredChanges`（3b）撤回；stamp 陣列放 kernel 不放 store（DI 6 對 store 仍成立；處置決定行）。
4. **影響**：`engine/entity_store`（回傳位置版本或 `Cell<T>`，二選一）、`tests/common/entity_store`（守衛 W1 字表含 `Player`／`Match`，所以 World 不放進 entity_store 目錄）、`engine/kernel`。
5. **Dependency**：新邊 `Kernel → EntityStore`（上層用下層）；store 不知道 kernel。
6. **Ownership**：排序演算法仍歸 entity_store；成員關係、延後套用、stamp、同步點的擁有者歸 World（entity-store.md 圖的誤導句第 6 條同步）。
7. **更小的方案**：World 自寫一套排序欄——重複同一演算法與守衛（AGENTS §2 Valid evidence 第 1 條），不採；借 EnTT——swap-and-pop 違 W-2（spec M），在 world-exec 宣告以 W-1～W-11 核對後定。
- 差分：world-exec 事前宣告選定兩種做法之一。

### Delta-4：OS 層頁保護平台層（**條件**：U1 採第三層；os.md RC 9 的出口是 Feature）

1. **Feature**：存取驗證第三層，攔 LIM-3 的 L1「寫了又寫回」（object_fps_pvp 的測試版與場次）；這是 RC 9 末句「and no Feature」的出口（legacy-impact 衝突 4 處置）。
2. **問題**：頁保護需要 `mprotect`／`VirtualProtect`／信號與 VEH；OS 邊界守衛 R1 禁 kernel 持 OS 碼（探測 27 筆，ci-readiness §1.4）；os.md RC 9（D78）只准觀測壓力成長；allowlist 放寬觸 RC 6。兩條合起來把頁保護鎖在門外。
3. **邊界**：`engine/os` 新增「記憶體保護與故障處理」能力；os.md Design Invariants 加一條（行程級故障處理器，只在回呼說「是我的」時處理、其他交前任）。
4. **影響**：`engine/os`、`engine/kernel`（INTERFACE→STATIC；PRIVATE 連 `GYO::Os`）、`tests/common/os`（allowlist 不動、`gyo_expect_os_links` 不動）、`tests/common/kernel`（第三層獨立執行檔 `gyo_kernel_pageguard_tests`）、`cross-platform.yml` 的 spike 步驟移除時點（ci-spike-retire）。
5. **Dependency**：新邊 `Kernel → Os`（PRIVATE，trace 的先例；不外露）；os 不依賴 kernel（只收一個 `bool(*)(uintptr_t, bool)` 回呼）。
6. **Ownership**：平台 handler 與保護 API 歸 engine/os；區間表、違規記錄與策略歸 kernel。
7. **更小的方案**：沒有——R1 不許 kernel 自帶、allowlist 放寬觸 RC 6；唯一的「更小」是不採第三層（U1 的 (c)）。改 RC 9 或開例外由使用者定。
- 差分：pageguard-os。**U1 選不採 → 本 Delta 撤銷、pageguard-os 自 §2 刪除、LIM-3 的 L1／L6 維持。**

### Delta-5：Product Registration 加 plugin 選擇（**已定**：資料；`v8-arch-plugin-selection-data`，2026-11-12）

1. **Feature**：Product 的追加・複製・削除只改它自己的內容與登記資料（AGENTS §7、§12 Add／Clone）；PvP Match／Client 與四工具各自選 plugin。
2. **問題**：今天的登記（`projects.csv`、`tools.csv`、`project.json` components）只描述 Engine 元件；plugin 清單若放 main（程式），「這個產品用哪些能力」就有兩處（legacy-impact 候選 5；AGENTS §9「同じ事実を手動で維持する場所は一か所」）。
3. **邊界**：Product Registration 的格式：`project.json` 加 `plugins` 欄（格式版本升一，AGENTS §8）；建置產生登記的編譯單元（與 `components` → `GyoComponents.cmake` 同一條路）；`main` 保持薄。
4. **影響**：`build/cmake`（`GyoApps.cmake`／`GyoComponents.cmake` 讀 `plugins` 欄、產生登記）、各產品與工具的 `project.json`、`ToolRegistryTests`、產品 main（變薄）。
5. **Dependency**：不變（共通 build 只解讀登記資料，不知產品名）；build 層讀登記資料產生連結與登記。
6. **Ownership**：plugin 選擇的權威在登記資料；初始化順序由資料或 plugin 宣告的依賴決定（宣告時二選一寫明）；缺 plugin／依賴衝突是 Contract 的明確錯誤（§9）；System 的登記仍是程式（plugin 的 build 函式）。
7. **更小的方案**：main 列清單（Bevy 的 `add_plugins`）——兩處同一事實，違 §9；兩處並用一致性檢查——同上。使用者已決定資料。
- 差分：plugins-foundation 實作；與 §9 的關係寫在宣告給風紀委員（候選 5 結案）。

**計畫層另記（不另立 Delta，已在上列範圍內或已核定）**：`gyo.zone-c-sources` v1 的生產端由手寫換成 Kernel 的 G-5 匯出（格式不變）；error-handling 契約的 `KernelError` 鏈（已同意，衝突 1 的收窄在 a6-revision）；event-sounds／preview 的資料契約 owner 搬到 plugin（Delta-2 第 6 項）；field-session DI 3 的澄清句（衝突 3，相容）。

---

## 5. 吸收的輸入逐條對照（38 列）

| # | 來源 | 輸入 | 進哪一批 | 怎麼驗 |
|---|---|---|---|---|
| 1 | ci-readiness §4-1 | OS 層 Delta：頁保護平台層進 `engine/os` | a6-revision（Delta-4 文字、U1）→ pageguard-os（條件） | os 邊界守衛對 `engine/kernel` 零命中；`gyo_expect_links` 檢查 kernel 不外露 `GYO::Os` |
| 2 | ci-readiness §4-2 | 編譯失敗測試的形狀（try_compile vs trait＋doctest） | kernel-scaffold（兩種樣本過 windows L1、U5 預設）→ registry-schedule 第一次實作 | J-10／J-11／J-13 在 windows-x64 L1 過；對應突變以 `expect_build_failure` killed |
| 3 | ci-readiness §4-3 | 第三層獨立執行檔＋`SKIP_RETURN_CODE 77` | pageguard-os | sanitizer 建置下 ctest 顯示 skipped（本機 tsan 樹一次）；一般建置執行 |
| 4 | ci-readiness §4-4 | spike 步驟移除的時點與範圍 | ci-spike-retire（條件 (a) world-exec 後、(b) pageguard-os 或 U1 不採）；`.github/` 交使用者 | §2.6 兩條件逐條勾；`spikes/kernel` 引用 0 |
| 5 | ci-readiness §4-5 | 驗證開關是執行期選項；L1 只有 RelWithDebInfo | world-exec | 同一執行檔開／關兩個 World；L1 xml 兩組案例都有 |
| 6 | pageguard 1 | 第三層範圍限定（Exclusive 欄、(c) System、開關開） | a6-revision（條文）→ pageguard-os | A.6 原文含三個限定詞；對抗核對；新宣告的判定 1 不含範圍外形狀 |
| 7 | pageguard 2 | Resource 要不要保護 | a6-revision（決定；建議 v1 不保護）→ pageguard-os | 決定寫進 A.6；(b3) 測試形狀隨決定（不保護則記 Limit） |
| 8 | pageguard 3 | (b) System 成本 2×\|Exclusive 欄\| | a6-revision（規則：(b) 只解它宣告寫的欄＋Shared）→ pageguard-os（量，B-9）；first-host-cost 先數 PvP 的 Exclusive 欄數 | B-9 實量 |
| 9 | pageguard 4 | 與其他 SIGSEGV／SIGBUS 使用者共存；`reinstalls` 進證據 | a6-revision（條文）→ pageguard-os | 串接測試（外來故障交前任）；`reinstalls` 出現在「驗證開著」報告欄位 |
| 10 | pageguard 5 | 多 World：違規帶 World 身分；區間表不可變陣列原子發佈 | world-exec（Err 帶 World 身分，第二層）→ pageguard-os（區間表） | J-5 跨 World 案例；TSan 下區間表讀寫零報告 |
| 11 | pageguard 6 | 欄重新配置與保護狀態 | world-exec（欄配置介面）→ pageguard-os | F.2 在 Run 內讓欄成長後新頁仍唯讀的測試 |
| 12 | pageguard 7 | `tlCurrent` 為空的故障：GYO_ASSERT 或 Err | a6-revision（決定；建議 GYO_ASSERT） | A.6 條文；pageguard-os 對應測試用 `GYO_CHECK_ASSERTS` |
| 13 | pageguard 8 | 第一層改 Err | world-exec | J-5 (c) 類以 Err 回傳；spike `SPIKE_CHECK_ASSERTS` 案例改檢查 RunResult |
| 14 | pageguard 9 | 頁大小與粒度分平台估算 | a6-revision → §6 B-5／B-9 | 預算寫 4／16／64 KiB 三種 |
| 15 | pageguard 10 | 平台層放 OS 子系統、Kernel 只持介面 | Delta-4；pageguard-os | 同第 1 列 |
| 16 | pageguard 11 | 第二層不能減；保護欄可跳過呼叫後比對 | world-exec（兩層都實作）→ pageguard-os（跳過的最佳化，量了才做） | 第二層關掉時 Shared／Resource／事件案例失敗的突變；跳過前後 J-5 全過 |
| 17 | -detect D1 | Life 時序 T=35／R=215（R−T=180），兩層相同 | plan-dryrun（形狀，U6）→ match-sim（實作） | `KernelPrepDetectTests.cpp` D1 兩案例**不改**而通過；`kp-death-next-tick`、`kp-respawn-next-tick` 仍 killed |
| 18 | -detect D3 | `connectionQualityFailures` 在 Judge 寫、kW 當 tick 進發布快照；Match `Snapshot()` 恆 0 | match-host（Judge 是 (c) System，Connection 診斷欄唯一寫者） | host 層特徵（W=600、1200／1800／2400）原樣過；Match 層那條照 §14.4 改寫；`kp-failures-*` 仍 killed |
| 19 | -detect 補列 1 | IPC 執行緒 `IngressBucketId`／`RecordRejection` 的 `ContainsPlayer`／`Snapshot()` | a6-revision（併入 G.4 第 5 條）→ match-host | `kernel_prep_sites` A 類對應列刪除；Ingress 拒收計數測試 |
| 20 | -detect 補列 2 | 主執行緒 `TakeIngressStatistics` 的 `Snapshot().players`；Stop 後 `TickCount()` | a6-revision（G.4 補第 6 條）→ match-host | 統計改經交換點複製（Host 診斷）；A 類列刪除；`TickCount()` 改讀 domain tick（Ready 狀態） |
| 21 | -detect 補列 3 | 無正式呼叫者 `QueueActionAcknowledgement`、`SubmitActions` | match-host（§7 刪除） | `git grep -nP` 引用 0；改寫的測試照 §14.4 說明、刪 TEST_CASE 前逐 CHECK 看 |
| 22 | -detect 補列 4 | `IpcHost` 的 `GetActionResults` ×4、`NoteWireRejection` ×2 | a6-revision（G.4 第 5 條）→ match-host | 改讀 `Link.outbound`；A 類列刪除；Gateway go 測試全過 |
| 23 | -detect 另記 | `TraceMovement` 本身讀 `MovementTraceNowNs()` | a6-revision（G.4 第 3 條措辭）→ diagnostics-service＋match-sim | (c) System 持 `DiagnosticsWriter<T>`、時間用 tick；J-17 ZS4 在確認清單為 0；白名單 W／G 對應列刪除 |
| 24 | windows (a) | `DiagnosticFileTests.cpp:216` 檔案仍開著時 `remove_all` 失敗 | diagnostics-service | 「Close 後無開啟句柄」斷言，測試內 100 次、windows-x64 L1 零失敗；closeout 60 run 比較 |
| 25 | windows (b) | `TraceSinkTests.cpp:155` Close 尾行順序不符 | diagnostics-service | 尾行順序契約測試 100 次相同；closeout 比較 |
| 26 | windows (c) | pvp.cpu Timeout 停在 RoleThread／wait backend（PR #190 run 43200345263） | plugins-foundation（headless-runner 啟停 N＝200）→ client-sim（`gyo-client-sim` 角色，dispatch 10 次 0 timeout）；match-host 只觀察；closeout 重掃 | 零逾時、零 hang、每次記 wait backend 名；60 run 統計比較 |
| 27 | legacy 衝突 1 | error-handling L124「no error chains」 | a6-revision（收窄、legacy 加「已被取代」註、鏈／清單措辭統一）；world-exec 宣告引用 | spec 內「鏈」「清單」用詞一致；對抗核對 |
| 28 | legacy 衝突 2 | preview.md DI 4（readback） | a6-revision（L3 carry-over 限縮 DI 1～3、5，DI 4 標已被取代，§3b 新條文）→ plugins-editor（PV2～PV5、pv-* 重寫） | 重寫的測試無 readback 斷言且以突變證明檢出力；readback 路徑引用 0 |
| 29 | legacy 衝突 3 | field-session DI 3 vs 執行期驗證開關 | a6-revision（target/system-contract §2b 加一句；field-session 不動） | 句子存在；對抗核對 |
| 30 | legacy 衝突 4 | os RC 9（D78）vs 頁保護 | a6-revision（U1 決定）→ pageguard-os（條件） | 使用者決定行進 HANDOFF；os.md 依決定標 |
| 31 | legacy 候選 5 | AGENTS §9 vs plugin 清單在 main | **已由 `v8-arch-plugin-selection-data` 結案**（資料）→ plugins-foundation 實作 | 共通層無產品名分支（掃描）；`project.json` 是唯一事實（產生物與資料一致的反例測試）；宣告寫與 §9 的關係給風紀委員 |
| 32 | legacy「被取代」定義 | 36 條重標 | a6-revision | 重標後 被取代＋相容（措辭更新）＝36；E-3 類「半條」歸相容 |
| 33 | legacy determinism | DI 1～6 現行；§1／§2（及 §4 區表、§7、E0～E5）搬 target | a6-revision（搬內容）；狀態行由圖書管理員改 | target/determinism 不再寫「this page does not change §1, §2」；反向依賴對 §1／§2 的引用 0 |
| 34 | legacy 行號引用 | 13 處改條號或章節名 | a6-revision | `git grep -nP` 行號引用 0（先以正例命中） |
| 35 | legacy entity-model 第 139 行 | 欄位分類表搬 target/world | a6-revision（match-sim 的拆欄引用它） | target/world 有表；spec 引用改條號 |
| 36 | legacy entity-store DI 6 | stamp 在 kernel 不在 store | a6-revision（target/world §6）→ world-exec（實作） | `engine/entity_store` 無 stamp（審查＋grep）；J-30 |
| 37 | legacy J-9 | 自寫產品名禁字表 | kernel-scaffold | 自測：加一個產品名失敗；`Despawn`／`ExtractRes`／`zone`、`far`／`near` 不誤判 |
| 38 | legacy 守衛遷移影響檔 | framework guard、4 工具守衛、`FRAMEWORK`、`ToolRegistryTests` 保留名 | plugins-foundation（加 plugins 對應、不刪）→ 工具四批（正向清單）→ framework-delete（刪） | 各守衛在對應批後仍綠或被正確刪除；刪 framework 後 grep 殘 0 |

件数：38 列；進批 31、條件批專屬 6（第 3、6～9、11、15 列中只在 pageguard-os 落實的部分）、交未決 1（第 30 列，U1）、已結案 1（第 31 列）、不採 0。

---

## 6. 預算（第一個 host＝Match 上量；AGENTS §2）

**數字的根據**（AGENTS §2 予見の根拠 3「既知の規模と予算」）：Match／Client 固定步 60 Hz（`adapter.AuthorityTickRate = 60`；`Movement.hpp`、`LocalPlayerPrediction.hpp` 註解）＝每 tick **16.67 ms**；entity-model RC-1：同時實體 4,000＝需求的 10 倍（需求 **400**）、Linux Match 欄維護每 tick p95 **1 ms**；entity-store RC-1 同門檻；field-session C3／C6 的統計形狀。**今天的 tick 成本未量**：pvp-baseline 先量分母。下表是候選，**在 match-sim 開工前由使用者核准**（§14.1；U12），看結果後不放寬。絕對上限（B-1 的 2.0 ms、B-3 的 8.33 ms）根據最弱，是三案都點名的弱點。

| # | 項 | 預算（驗證關＝release 預設） | 驗證開（測試版預設） | 量測方法 | 在哪批量 |
|---|---|---|---|---|---|
| B-1 | Match FixedUpdate 一個 tick 的工作（重放 host；37 golden 情境＋8 人長情境 ≥3,600 tick） | p95 ≤ max(基線 p95 ×1.25, 基線 p95 ＋0.25 ms)，且絕對 p95 ≤ **2.0 ms**（tick 的 12%）；p99 ≤ 4.17 ms（tick 的 25%） | p95 ≤ 驗證關 ×3 且 ≤ **8.33 ms**（tick 50%，測試版仍能即時跑） | 口徑同 pvp-baseline（交換點 A→B）；RelWithDebInfo；暖機 600 tick 不計；同機 5 次取每次 p95 的中位數；新舊同情境 | first-host-cost（G2a） |
| B-1′ | 同上，真實 Match 行程（含交換點與 IO 執行緒） | 同 B-1；10 分鐘 bot 對戰或 acceptance 腳本中 overrun（>16.67 ms）0 次 | 同 B-1 | field-session C3 形狀的統計行；Linux | match-host（G2b） |
| B-2 | Kernel 自身開銷（合成：400 實體×8 欄×20 個空函式體 (c) System，Plan 形狀仿 PvP Simulate、含延後 spawn／despawn） | 一次 `Run` p95 ≤ **2.0 ms**；每次 System 呼叫派發 ≤2 µs、每個空佇列同步點 ≤5 µs；以 PvP 的 System 數（dryrun 得到）換算不得超過 B-1 允許增量的一半；欄維護（insert／erase／合併）p95 ≤ **1 ms**，4,000 實體時仍 ≤1 ms（超過＝spec K.2 第 6 條） | 第二層比對成本 ≤ 欄總位元組的線性成本，記錄斜率 | `tests/common/kernel` 記錄執行檔（不進 ctest），100,000 次 Run；本機 Release＋Linux | world-exec（初量）、first-host-cost |
| B-3 | finish（Registry 凍結＋Scheduler 分析） | ≤ **50 ms** @ 500 System（spec D.4 的 O(N²) 未量） | — | registry-schedule 的合成 Registry；本機 Release | registry-schedule |
| B-4 | World 記憶體 | 400 實體：World 位元組（欄＋keys 8 B＋stamps 16 B／格）≤ **4 MiB**；Match 行程穩態 RSS ≤ 基線 ＋16 MiB 且 ≤ 基線 ×1.10 | 快照緩衝 ≤ 欄總位元組 ×2；**關時快照緩衝 0 位元組**（LIM-6 條件 4） | World 自報每欄位元組（kernel 診斷 Resource，可加總核對）；RSS 由 `/usr/bin/time -l`／`-v` 或 `/proc/self/status` VmHWM 穩態取 | first-host-cost（World）、match-host（RSS） |
| B-5 | PredictionHistory（Client） | ≤ 深度 × 回滾集合位元組 ×1.2 | — | rollback-extract 合成記錄；client-sim 實測 | rollback-extract、client-sim |
| B-6 | 載入到 ready（Loading → `LoadingState = Ready`；現有 arena） | ≤ max(基線「啟動到可接受 join」×1.10, 基線 ＋100 ms)，絕對 ≤ **1 s**；每個 Loading 批 ≤ **16.67 ms**（loading 畫面不卡幀，H.2b 的 Feature） | 同左 ×1.5 | Host 側時間戳；10 次冷啟動中位數；Loading 批計時由 runner 記錄 | loading（合成，每批上限）、first-host-cost（重放）、match-host（真實行程）、client-sim／client-frame（Client 批） |
| B-7 | Client 幀（階段 3） | ClientFrame＋ClientRender p95 ≤ 基線 ×1.10（多一個 World 與每幀一次 Extract 的代價）；Client 模擬 tick 套 B-1 規則對舊 Client | — | 基線在 client-frame 開工前另量（同口徑：一幀從 pump 後到 Present）；Windows 實機（CI 量不到）＋runner 記錄 | client-sim、client-frame |
| B-8（條件） | 頁保護（pageguard-os） | 不適用（關時零） | (c) System 每次切換 ≤10 µs；(b) System 成本 2×\|Exclusive 欄\|，64 欄 ≤250 µs、實際欄數 ≤1 ms／(b) System；記憶體 commit ≤ 欄數×頁大小（4 KiB Linux／16 KiB Apple Silicon／Windows 64 KiB 保留位址，總保留 ≤64 MiB） | spike Cost 案例改成記錄執行檔；Release 組態（spike 參照：full ≈200 µs@64 欄、switch ≈7 µs） | pageguard-os |

量測紀律：量測執行檔不進 ctest（共用 runner 的 µs 計時是雜訊；ci-readiness §2.2）；證據放主 checkout `build/evidence/kernel-migration/<批>/`（git 管理外），文件只寫結論與判定用的數字（AGENTS §15）；舊路徑基線在同一批、同一台機器、看新路徑結果前量，重複間 p95 差 >20% 先停下判環境；機器照 U8（Linux 判定、Intel Mac 代理並寫限界）；超預算處置：<1.5 倍＝報告、使用者決定；≥1.5 倍＝停、重排（§8 RC-1）。LIM-2 的三條路（Parallel 加守衛、拆 World、葉子函式內 job）是重排時的選項，不在超標時自動採用。本機重 GPU／大記憶體量測先講影響並取得同意（memory warn-before-heavy-local-runs；B-1～B-6 都是 headless、記憶體小）。

### 取捨（§6）

三案的 B-1 相對係數不同（風險 ×1.15／＋0.25 ms；價值 ×1.25／＋0.25；成本 ×1.25／＋0.5 且 ≤2.0 ms）。採 ×1.25／＋0.25／絕對 2.0 ms：×1.15 在沒有分母時過嚴（第一個 host 的 Kernel 額外成本就是要量的東西），＋0.5 ms 在 16.67 ms 的 tick 下與 ×1.25 重疊無意義；絕對上限保留成本案的 2.0 ms 當「不論基線多小都不能超過」的護欄。放棄的代價：若今天的 tick 遠小於 1 ms，×1.25 允許的絕對增量很小，可能因量測雜訊誤判——所以同時要求 5 次中位數與 ≤10% 散佈（pvp-baseline）。

---

## 7. 未決（要使用者定；附選項與建議）

| # | 事項 | 選項 | 三案的建議 | 統合建議與理由 |
|---|---|---|---|---|
| U1 | **os RC 9（D78）與頁保護第三層** | (a) 現在採：第三層列為名指 Feature（RC 9 末句出口），開例外、RC 9 原文不動，排 pageguard-os；(b) 改寫 RC 9（基盤可依預見成長）；(c) 延後到 G2a 之後再定（數得出 PvP 的 Exclusive 欄數、量得到 (b) System 成本），決定前 world-exec 用「一般配置＋可換獨立頁的介面」；(d) v1 不採，LIM-3 的 L1／L6 維持 | 風險 (a)；價值 (c) 傾向 (d)；成本 (c) | **(c)**：第三層不在任何 host 的關鍵路徑；L1 形狀至今無觀測案例；成本（Apple Silicon 每欄 16 KiB、(b) System 2×\|欄\|）要等 PvP 的欄數才有根據；(a) 的好處只有場次覆蓋多一種形狀。代價：spike 步驟與 `spikes/` 多留一段（ci-spike-retire 等 G2a）；若在 G2a 後選採，Delta-4 要在那時走 §3 同意。選 (a) 不影響關鍵路徑 |
| U2 | ~~plugin 選擇是資料還是程式~~ | — | 三案都列未決（風險 (a) 資料；價值、成本 (b) 程式） | **已由使用者決定：資料**（`v8-arch-plugin-selection-data`，2026-11-12）。不再是未決；plugins-foundation 實作，Delta-5 寫成已定。這裡只記錄差異來源 |
| U3 | **先穩定版還是邊做邊遷** | (a) 每個 host 整個遷完才合併：match-sim＋match-host 同一 PR 線、client 三批同一 PR 線，master 一直可發佈，穩定點 S1＝Match 在 kernel 上（Client 仍舊，wire 相容）、S2＝Client、S3＝工具與 framework 刪；(b) 逐批合併、有界並存：match-sim 先進 master，舊 `PvpMatch` 與新 (c) plugin 並存到 match-host 結束；(c) 長命分支全部 host 遷完才合併 | 風險 (a)；價值 (b)；成本 (b)（match-host 不在關鍵路徑） | **(a)**。(b) 讓 master 上同時有兩種動詞（spec K.2 第 8 條、H-7「一個 host 不會一半在機制上」）且兩路都要產同一 golden、任何修正都要雙寫；(c) 讓成本閘門與 L1 反饋都延後。代價：Match 的 PR 線長（約 100～205 h）、L1 反饋集中在線末——補救＝分支上每批仍跑 draft PR 的 L1（memory draft-pr-l1-skip：手動 dispatch） |
| U4 | **host 順序與閘門嚴格度** | 順序三案一致：Match → Client → 工具。閘門：(a) 一段，在 match-host 後（嚴格讀 AGENTS §2「一つの host」）；(b) 一段，在重放 host（match-sim）後；(c) **兩段**：G2a（重放 host）放行 Kernel／plugins 側與 Client 共用的工作（rollback-extract、engine-wrap、plugins-frame），G2b（真實 Match 行程）才放行 client-sim／client-link／client-frame 與 Editor 線 | 風險 (a)；價值 (b)；成本 (b)（嚴格讀法多 60～115 h） | **(c)**。第二個 host 的遷移不在真實 host 量過前開始（守 §2 字面），但 Kernel 後半與 Engine 的「包」本來就要做、與 host 無關，先做不違反「廣げる前に」。代價：若 G2b 不過而 G2a 過，2' 階段約 70～175 h 的工作可能要改；這些批的產出都在 kernel／engine 內、不依賴 Match 的數字，改動局部 |
| U5 | 編譯失敗測試的形狀（ci-readiness §4-2） | (a) CMake `try_compile` 預期失敗（J-10 原文）；(b) trait＋doctest（spike 做法）；(c) 核心 3 條（J-10 的 lambda 捕獲、`World&` 參數、回傳型別）用 (a)，其餘 (b) | 風險 (c)；價值 (b)＋一個 try_compile 金絲雀；成本：scaffold 兩種樣本後選 | **(c)**，在 kernel-scaffold 以兩種樣本的 windows-x64 結果定案：(a) 無先例、Windows 未驗；(b) 已在 spike 三平台過。若 (a) 在 Windows 不穩，全用 (b) 並記 J-10 原文的差異 |
| U6 | **Life 的寫者形狀**（dryrun 的結論要核准） | (a) Life 唯一寫者在 tick 開頭（處理到期復活），死亡由 `ResolveActions` 寫獨立中間欄 `DeathRecord`、同 tick 之後的 System 與 digest 讀它、下一 tick 開頭併入；(b) 事件＋下一 tick 套用（死亡晚一 tick 進 snapshot，golden 變）；(c) 拆 Life 為 `LifeState`（tick 開頭寫者）與 `DeathRecord`（裁決寫者），digest 組裝讀兩欄得同位元組；(d) 同一 System 兩個掛點——**不可行**（spec B.1：同一 schedule 掛兩次是 D-E19） | 風險 (a)／(c) 由 dryrun 定；價值 (d)；成本 (d) 或其他保 T=35 的形狀 | **(a) 或 (c) 由 dryrun 的 finish 結果選**；判準＝-detect D1 測試不改一字而過、golden 37 不變、D-E2＝0、不觸發 D-E19 與 K.2 第 9 條。(b) 改 golden 不建議；(d) 是啟動錯誤。這是 digest 相關的產品決定，所以列未決 |
| U7 | refusal 回覆時機（spec H.4.1 (b)） | (a) 接受晚最多一 tick（Gateway 與 Client 測試照舊過即可）；(b) 保持同步回覆（IO 執行緒要讀權威＝違 I-1，不可行）；(c) Gateway 先回「已收」、結果晚到 | 三案 (a) | **(a)**，前提是 match-host 事前宣告第一步查證 pv9 契約與 Gateway 測試對時機沒有規定；有規定→停、回來再定 |
| U8 | 成本量測機器 | (a) 使用者的 Linux 機器（Match 主戰場）；(b) Intel Mac 代理；(c) 兩者，判定用 (a)、(b) 當開發參考；不用 CI runner | 三案同向 | **(c)**；Linux 不可用時判定用 (b) 並在報告寫範囲の限界 |
| U9 | LIM-5 造成的 golden 重設基準 | (a) 每個情境逐條照 §14.4 1～5 交使用者核准（事前宣告列可能受影響的 golden 名、驗證者複核）；(b) 事先核准「只限 Join／Leave 差一 tick 的情境」的重設規則 | 風險 (a)；價值、成本：事前列名＋§14.4 | **(a)**。事先空白授權等於結果後放寬；事前宣告仍要列出可能受影響的 golden 名（結果後不加名） |
| U10 | `spikes/` top-level 與 CI spike 步驟 | (a) 條件滿足時刪（ci-readiness §2.6；`.github/` 由使用者改）；(b) 保留 `spikes/` 當未來 spike 位置 | 三案 (a) | **(a)**（AGENTS §7 不留空目錄）；時點隨 U1：選 (c)／(d) 時 world-exec 後即可，選 (a) 時等 pageguard-os |
| U11 | **並行線的數量** | (a) 開「產品-Match」「產品-Client」兩條產品遷移線（產品前置線升格）與「Engine／plugins 線」，控制權仍在架構師主會話；(b) 維持現有四條（掃描器、產品前置、CI、文件），產品批串行、Engine 包進架構師線 | 成本 (a)（關鍵路徑短 80～155 h）；風險、價值未列 | **(a)**。代價：使用者同時要看的關卡變多（每批事前宣告與合併）；§8 RC-7 管並行衝突 |
| U12 | §6 預算數字的核准 | 照 §6 候選，或改數字 | 三案都要求事前核准 | 在 match-sim 宣告前核准（§14.1）；B-1 的 2.0 ms 與 B-1 驗證開的 8.33 ms 是推出來的、根據最弱，請特別看；pvp-baseline 的分母出來後若尺度明顯不同，由使用者在看遷移結果前改一次 |
| U13 | 目標 DI 的核准時點 | (a) a6-revision 一次核准 world／system-contract／scheduler／runtime-composition／assets／determinism／error-handling／naming 的候選 DI 與 kernel DI 頁；(b) 每部件在其第一批開工前各核准一次 | 成本 (a)；風險、價值把 DI 頁放 a6-revision | **(a)**：一次讀完、關卡少；AGENTS §3「機能を積み上げる前」對 registry-schedule 已足。代價：核准後再改是 Delta |
| U14 | Loading 的 ready 在 v1 的實現 | (a) 「Welcome 隱含 ready」（Match 只在 Ready 後接受 join；Client 等 Welcome），wire 不變；(b) 新 wire 事件（pv10） | 價值 (a)；成本 (a)（ready 事件屬 pv10，不在遷移內）；風險 (a) | **(a)**；Client 的 Loading manifest 若既有 wire 不夠表達→client-sim 停下報告，進 pv10-requirements（§8 RC-9） |

---

## 8. Replacement Criteria（這份計畫什麼時候該重排）

符合任一條：停在當前批的下一個關卡，不在計畫上補丁，回到規劃（統合者重寫受影響的階段）並交使用者；「計画の前提を見直す（範囲を変える）」永遠是選項之一（AGENTS §14）。

| # | 條件 | 重排什麼 |
|---|---|---|
| RC-1 | **成本**：G2a（B-1、B-2、B-4、B-6）或 G2b（B-1′、B-4 RSS、B-6）或 client-frame 的 B-7 任一項 ≥ §6 預算 ×1.5 | 該閘門之後的階段；選項（交使用者）：Kernel 熱路徑最佳化批、World 儲存換結構（spec K.2 第 6 條）、縮小 (c) 範圍、LIM-2 的三條路（Parallel＋J-27 守衛／拆 World／葉子 job） |
| RC-2 | **G1 失敗**：Plan 指紋三平台不同或 J-44 不過，修兩次不收斂；或 K.2 第 3 條在合成測試就出現 | 全計畫（spec K.2 第 1、4 條；可能退回顯式宣告） |
| RC-3 | **估時失準**：第一個 host（match-sim＋match-host）完成後，實際／估計比的中位數落在 0.5～1.5 以外；或 Kernel 本體四批實際合計 >估時上限（132 h）×1.5 | 以實測重估剩餘各批並重算關鍵路徑（不只改數字，看依賴是否變） |
| RC-4 | **判官失效**：dryrun 有 digest 欄必須 Shared、非資料流順序邊 >50%、Life 形狀保不住 T／R；或 match-sim／match-host 的 golden 37 在原因分析兩輪後仍不逐位元且差異不在事前列名的 LIM-5 集合 | 階段 2（重劃區／重拆欄；K.2 第 3、10 條）；回到「是不是在為舊形狀找位置」（target/README 檢查項） |
| RC-5 | **遷移成本**：任一 host 的實測工時 >1.5×估計且大於重寫估計；或遷移到一半必須讓兩種動詞長期並存（K.2 第 8 條；Match 兩批合計 >上限 205 h ×1.5＝308 h 即觸發） | 該 host 之後的批；提「修還是重建」（memory repair-vs-rebuild） |
| RC-6 | **共用件撐不住／Host 又長回來**：net-link、sdl-window-runner、editor-dialogs 任一在第二個消費者時要加消費者專屬分支；第二個 host 遷移時要改 Kernel 本體或 runner 骨架；H.7 以外的協調邏輯在兩個 host 重複（K.2 第 7 條） | 拆 plugin 並記重工；同一形狀發生兩次→重排 plugin 邊界（audit §10.4）／Kernel 介面批 |
| RC-7 | **並行衝突**：兩條並行線在同一檔案的衝突解決累計 >2 h，或一批的受理因另一線的合併而重跑兩次以上 | 該兩線改串行（U11 回 (b)） |
| RC-8 | **Delta 增生**：新發現的 Architecture Delta 不在 §4 五項之內累計達 2 個 | §4 與受影響的批 |
| RC-9 | **前提改變**：需要改 wire（pv 版本；含 ready 事件、refusal 時機被 pv9 規定、manifest 不夠表達）、使用者恢復任一暫停項（lag win1、帳本、兩通道、DET）、或 U1／U3／U4／U11 的選擇與本稿建議不同 | 受影響的階段；依新選擇重算 §2.3（各 U 已寫差額） |
| RC-10 | **基盤不穩**：每加一個 System 或 plugin 都要重新驗證 Kernel 本體的守衛（突變名單以外；K.2 第 13 條）；或 threading 的 TSan／Windows 壓力出現逾時且觀測顯示卡點在 Kernel／runner 設計（不是 runner 機器） | Kernel 批重開，host 批暫停 |
| RC-11 | **總量**：任何時點「已完成實際＋剩餘估計上限」>1,140 h ×1.5 | 停下報告，重看範圍 |

**不觸發重排的（寫明以免誤用）**：pageguard-os 失敗（第三層不採、LIM-3 L1 維持，§3.10）；單一批超時 1.5 倍（照 memory 停下回報該批，不必重排全計畫，除非同時觸發 RC-5）；工具批內部的拆法變動（應用 Architecture，依觀測調整）；前置 -zones 卡延誤（只延後 match-sim 的開工，不改依賴）。

---

## 附：三案的主要差異與本稿的取法（一覽）

| 題 | 風險優先 | 價值優先 | 成本優先 | 本稿 |
|---|---|---|---|---|
| Kernel 第一批 | Scheduler 先、dryrun 證偽 | World 先 | World 先 | Scheduler＋Registry 先、dryrun＝G1（§2 取捨） |
| Kernel 批數 | 5（scheduler／world-exec／threading／loading／rollback／extract 拆） | 3（world／schedule／rollback-extract） | 3＋scaffold | 5＋scaffold（registry-schedule／world-exec／loading／threading／rollback-extract） |
| 基線量測 | 獨立批 | 併入 first-host-cost | 獨立批 | 獨立批 `v8-product-pvp-baseline` |
| 成本閘門位置 | match-host 後 | match-sim 後 | match-sim 後（嚴格讀法另加 60～115 h） | 兩段 G2a／G2b（U4） |
| match-sim 與 match-host 的合併 | 同一 PR 線 | 有界並存、分開合併 | 分開（match-host 不在關鍵路徑） | 同一 PR 線（U3） |
| net-link | 在 client-link | 在 match-host | 獨立批、Match 先用 | 獨立批（§2 取捨） |
| Life 形狀 | dryrun 定；(a)／(c) | 同 tick 兩掛點 | 同 tick 兩掛點 | dryrun 定；兩掛點是 D-E19（§3 取捨、U6） |
| 頁保護 U1 | 現在採 (a) | 延後、傾向不採 | 延後 | 延後到 G2a 後定（U1） |
| plugin 選擇 | 資料 | 程式 | 程式 | 已定資料（`v8-arch-plugin-selection-data`） |
| Windows 觀察驗收 | 測試內 N＝100／200 | dispatch 10 次 | dispatch 10 次 | 測試內重複為批內門檻；dispatch 10 次只用在 client-sim；closeout 60 run |
| 並行線 | 未列 | 未列 | 開兩條產品線＋包線 | 採（U11） |
| 估時合計 | 545～1,070 h | 530～1,050 h | 560～1,130 h | 560～1,140 h（不含條件批） |

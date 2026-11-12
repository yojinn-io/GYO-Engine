# 遷移批規劃（統合稿）：`v8-arch-migration-plan`

狀態：**已核准（使用者 2026-11-13；HANDOFF `v8-arch-migration-plan-approved`）**；未決 13 項已定（HANDOFF `v8-arch-migration-plan-undecided`，§7）。
日期：2026-11-12。性質：ultracode 的統合稿（提案 Opus×3／high：風險優先、價值優先、成本優先；統合 Fable／high；第一輪對抗 Opus／xhigh 的 blocker 4／should 19／nit 16 與第二輪對抗 Opus／xhigh 的 blocker 1／should 11／nit 14 都已照架構師處置修訂，處置記錄在文末「對抗檢查處置記錄」（第一輪、第二輪各一節）；第三輪限定範圍的對抗 blocker 0／should 8／nit 12 也已照架構師處置修訂（文末第三輪一節））。只讀 repo、只寫本檔；不寫程式、不改既有檔。
基準：origin/master `3061e879`（PR #196 `v8-arch-legacy-impact-dispositions` 已併入）；另加本分支 `claude/arch-migration-plan` 的 `4164635a`（`v8-arch-plugin-selection-data`，使用者 2026-11-12 的 plugin 選擇決定，三份提案寫成時尚未存在）。**本計畫與 `4164635a` 在同一分支、同一 PR 合併**，所以基準內含該決定不成問題。第二輪修訂另以本分支的三個決定行為基準（同一分支、同一 PR）：`bea68b05`（`v8-product-kernel-prep-zones-gate`，-zones 事前宣告的核定）、`26b1976e`（`v8-arch-guards-migrate-with-code`，守衛與突變隨程式遷移）、`78d06961`（`v8-arch-migration-plan-round2`，第二輪處置）。三份提案與三輪對抗報告在主 checkout `build/evidence/kernel-migration-plan/`（`proposal-{risk,value,cost}.zh-Hant.md`、`adversary-report.md`、`adversary-report-r2.md`、`adversary-report-r3.md`；git 管理外）。
引用寫法：檔名＋章節（例：spec B.1、audit §1.3、legacy-impact §3 衝突 4、HANDOFF 決定行 `v8-arch-…`）。

統合規則（任務指定）：三案相同的直接採；不同的在該節末「取捨」寫採哪一案、為什麼、放棄的代價；任一案提出而其他案漏的風險收進去；每批的順序、依賴、判定都可檢查；未決不替使用者決定。

---

## 0. 件数の突き合わせ與受理判定（看結果前宣告）

**件数の突き合わせ（本稿吸收的輸入 → §5 對照表）**：輸入 **42** 條＝ci-readiness §4 的 5 條＋spike-pageguard-report 末尾 11 點＋-detect 7 條（D1 Life 時序 1、D3 failures 寫點 1、G.4 補列候選 4、`TraceMovement` 讀時鐘 1）＋HANDOFF〈觀察〉windows 3 條＋legacy-impact 處置 12 條＋-zones dev_log「回報架構師」4 條（第三輪對抗 S3-8 吸收）。§5 有 42 列，每列有「進哪一批、怎麼驗」：**一致**（42＝42；§5 末行的分類計數 39＋2＋0＋1＝42 也一致）。三案的計數：風險 38（寫法 5＋11＋7＋3＋12）、價值 38、成本 37（把 `TraceMovement` 併入補列第 4 條）；本稿照價值案拆成 38，因為 `TraceMovement` 的去處（pvp-diagnostics）與補列第 4 條（match-host）不同批。
**另兩組輸入（第二輪吸收，不併入 §5 的 42 列）**：-zones 核定宣告交遷移批的 **24** 處（B 5、F 2、D 3、E 14；`v8-product-kernel-prep-zones-gate`）在 §5.1 逐處分派批與判定（24＝24）；守衛去處的起點清單＝legacy-impact §4 的 **21** 個守衛（`v8-arch-guards-migrate-with-code`），§5.2 逐列寫預計在哪一批判去處（21＝21）。
**批次**：**34** 批（含條件批 1：`v8-arch-pageguard-os`；第一輪對抗後新增 `v8-arch-assets-core`（第一輪對抗 B-4 拆批）與 `v8-product-pvp-diagnostics`（第一輪對抗 B-3 拆批））；§2 的樹、§2 的表、§3 的節各 34（工具四批在 §3 共用一節加差異表，仍各算一批）。閘門 4 個：G1、G2a、G2b、G3。
**未決 13 項已由使用者定（2026-11-13，HANDOFF `v8-arch-migration-plan-undecided`；§7 改為已定事項），A 判定不變。**
**已定前提的差異**：plugin 選擇已由使用者定為**資料**（HANDOFF `v8-arch-plugin-selection-data`，2026-11-12）；三案都把它列為未決（風險 U2、價值 U2、成本 U-2），本稿改為已定前提，§4 Delta-5 寫成「已定」並引決定行、§7 不再列（差異來源只記在文末「附：三案差異」的 plugin 選擇列）。

**受理判定**（統合者與對抗檢查者照此判；任一不成立＝回提案，不改判定文字）：

| # | 判定 | 怎麼查 |
|---|---|---|
| A1 | 第一輪對抗檢查（Opus／xhigh）的 blocker 4 全部處置、should 19 全部處置或寫明不採的理由、nit 16 全部處置（處置記錄在文末）；blocker＝照計畫做會破 Design Invariants、Product Boundary、(c) 區四處契約（mechanism §3.3），或違反 §1.1 的已定前提 | 對抗檢查報告、文末「對抗檢查處置記錄」 |
| A2 | 每批都有 Feature → Contract → Architecture 三段；Feature 寫「這批結束時誰看到什麼」；跨產品能力寫出使用它的 Product；Architecture 欄只在 Contract 撐不住時才有變化，撐得住就寫「不變」 | §3 逐批 |
| A3 | §3 Architecture Delta 七項只在 §4 寫一次（六個 Delta 各七項齊）；§3 各批只寫「引 §4 Delta-n」與差分 | §3、§4 |
| A4 | 每批有上段／下段與理由、實作者／審查者／驗證者的「模型／檔位」；上段不用小模型；審查者與實作者不同模型、檔位至少 high；§14.4 驗證者不是提議者本人；量測門檻的對抗檢查者與宣告作者不同模型；突變一覽的敵對檢查者（AGENTS §14.2）與一覽作者不同模型、不用小模型（§3 共通「守衛與突變的去處」第 3 點）；超過主會話的指派列在 §2.2 末「需使用者同意的指派」 | §2 表、§3 |
| A5 | 性能相關批（`-pvp-baseline`、`-first-host-cost`、`-match-host`、`-client-sim`、`-client-frame`、條件批 `-pageguard-os`、`-registry-schedule` 的 BU-3、`-world-exec` 的 BU-2、`-loading` 的 BU-6 每批上限）引 §6 的 BU-n 數字與量測方法，數字只在 §6 一處；每個數字有「出所」或標「推定」 | §6 |
| A6 | 每批的範囲の限界以「LIM-n＋一句摘要」寫，不只貼編號（target/README limits 規則 (b)）；每批寫「若這批失敗，後面哪些批白做」與停止條件 | §3 |
| A7 | 42 條輸入各有一列；不重開已定前提、不排暫停項（lag win1、網路帳本與兩通道、DET-0～3） | §5、§1 |
| A8 | 全稿的 BU-n 引用（§0、§2、§3、§5、§8）全部能在 §6 找到，且引用處寫的項目名稱與 §6 一致；G2a、G2b、RC-1 的集合各只寫一次完整集合並附項目名稱 | 掃 `BU-\d`：用 Python（`python3 -I`；`git grep -P` 在 UTF-8 下 `′?` 只套最後一個位元組，第二輪對抗已示範），先以 §6 表的 `BU-1′` 正例命中確認掃描有效、不丟 stderr，逐處對照 §6 表；spec 的 B-n 寫成「spec B-n」、對抗報告的寫成「第 n 輪對抗 B-n」，不是預算編號、不在掃描對象 |
| A9 | 對抗檢查第二輪（Opus／xhigh，只讀）的 blocker 1／should 11／nit 14 全部處置（文末第二輪一節）；第三輪限定範圍的對抗（範圍＝第二輪修訂的差分與 A10；`v8-arch-migration-plan-round2`）0 blocker。**宣告文字的修正（架構師 2026-11-12 核）：原文「第二輪 0 blocker」改為「最終輪 0 blocker」**——看到第二輪結果後改的判定文字，**使用者 2026-11-13 核准（HANDOFF `v8-arch-migration-plan-a9`）**；第三輪 blocker 0 | 第二輪、第三輪對抗檢查報告 |
| A10 | 守衛與突變隨程式遷移（`v8-arch-guards-migrate-with-code`）：每批事前宣告有「守衛與突變的去處」表（§3 共通，四選一）；遷移完成的部件**不再有守衛的性質只來自 legacy 檔**（含「留」型：其 DI 在 legacy 檔刪除前要先寫進 target 或搬回後的現況檔，§3 共通規則第 2 點）；起點清單＝legacy-impact §4 的 21 個守衛（§5.2 逐列寫判去處的批與 DI 落點），closeout 時「性質只來自 legacy 檔的守衛」＝0、每個「留」的守衛其 DI 在 target 或現況檔有條號 | §3 共通、§5.2、各批事前宣告、§3.32 判定 |

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
- spec B-8：一個事件只有一種性質（`v8-arch-kernel-break-hook`）。
- 舊文件 legacy、設計權威在 target（`v8-arch-docs-legacy`）；現有程式受 legacy DI、新程式受 target DI，部件遷移完成時 legacy 刪、target 搬回。
- object_fps／v2／object_fps_preview、IRuntimePort、AnimationInstance 已刪（PR #185）。
- J-17 掃描器已有（PR #192；確認清單基線 26、候選 9）；-detect 已固定現況（T=35／R=215、`kernel_prep_sites` 白名單 47 列）；-zones 卡的事前宣告已核定（`v8-product-kernel-prep-zones-gate`）：門 26→**7**——確認清單的掃描結果正好等於 `test_zone_c_lists.py` 的 RESIDUAL 釘住的 7 處殘留（5 處 `<filesystem>` 載入器簽名＋2 處 ZS7），**不是 0**；Sim 用 `inline namespace sim`（v1；closeout 改非 inline——架構師 2026-11-12 定，§3.14、§3.32）；產品 namespace 是 `fps::pvp`（`RetroFPS` 只是 include 目錄名）。實作進行中（搬區不改行為）——本計畫把它當外部依賴，不重排；它交遷移批的 24 處逐處分派在 §5.1。
- **plugin 的選擇是資料**（`v8-arch-plugin-selection-data`）：`project.json` 加 `plugins` 欄，共通 CMake 讀它連結 plugin target 並由建置產生登記的編譯單元；`main` 保持薄；System 的登記仍是程式。
- **§7 的未決已定**（`v8-arch-migration-plan-undecided`，使用者 2026-11-13）：U1 延後到 G2a 後、U3 master 不得新舊並存、U4 兩段閘門、U5 核心三條 try_compile、U6 `LifeState`＋`DeathEvent`、U7 refusal 晚一 tick（bevy_replicon 式分層）、U8 三層量測（判定在 Docker Linux 容器）、U9 golden 重設逐條核准、U10 刪 spike 留永久連結、U11 三條常設線分階段開、U12 基線後再核准預算、U13 目標 DI 一次核准、U14 Welcome 隱含 ready（未結事項）。
- **守衛與突變隨程式一起遷移**（`v8-arch-guards-migrate-with-code`，使用者決定）：否則會把架構拉回舊形狀；規則在 §3 共通「守衛與突變的去處」，判定 §0 A10。
- 暫停不討論：lag win1、網路帳本與兩通道、DET-0～3（`v8-arch-kernel-pending-paused`）。

### 1.2 本稿的排序原則（三案的交集，加統合者的一條）

1. **先證偽、後建造**（風險）：「若錯了後面全部白做」的性質（Plan 唯一且三平台相同；PvP 能在獨占＋區規則下表達）排在建造它之上的東西之前，各成一個單獨可證偽的批（G1）。
2. **第一個 host＝Match**（三案一致）：37 個 golden 與 `AuthorityDigest` 逐位元判定（spec J.4）是全 repo 最強的自動判官；wire 不變，所以 Match 遷完能用現有 Client 對戰。
3. **Kernel 只做第一個 host 用得到的部分**（價值、成本）：回滾與 Extract（H.5、H.6）契約已定，實作與 Match 遷移並行、不在關鍵路徑上。
4. **一個 host 要嘛整個在機制上、要嘛不在**（風險；spec H-7、K.2 第 8 條）：同一 host 的 sim 與 host 兩批可分開審，但同一條 PR 線一起合併；**原則：master 不得新舊並存**（U3 已定 (a)：退役舊文件就是為了防過渡期，並存會造成版本分裂）——match-sim＋match-host 一條 PR 線、client-sim／client-link／client-frame 一條 PR 線（已寫在 §2.2、§3.14、§3.21～3.23，確認）。
5. **量測先於預算判定**（風險、成本）：先量今天的 Match 當分母；預算寫成「基線的函數＋絕對上限」，在看遷移結果前由使用者核准。
6. **共用的東西先做一次**（成本）：net-link 的 IO 服務殼、sdl-window-runner、Engine 的「包」各一批，不在每個 host 各做一次。
7. **條件 Delta 不擋關鍵路徑**（風險）：頁保護（第三層）採不採**延到 G2a 後定**（U1 已定 (c)），world-exec 只留「一般配置／可換獨立頁」的欄配置介面，實作本身並行。
8. **統合者加的一條：閘門分兩段**（§7 U4，已定 (c)，確認）：G2a（重放 host 量 (c) 的 Kernel 成本）只放行**Kernel／plugins 側**與 Client 共用的工作；G2b（真實 Match 行程）才放行第二個 host 的遷移。這是對 AGENTS §2「一つの host で費用を計測し…他の host へ広げる前に」的嚴格讀法，同時不把 Client 線的前置工作卡死。

### 1.3 不在範圍

- 暫停項（§1.1 末條）：不排程、不設計、不以它們為驗收。
- **wire 不改**：遷移期間 pv 版本不變。Loading 的 ready 在 v1 **已定 (a)「Welcome 隱含 ready」**（U14，使用者 2026-11-13）；它與 spec H.2b 第 4 點的衝突記為**未結事項（可能的設計缺陷）**：v1 的 Match 玩家 stage 不生成初始成員、H.2b 第 4 點只在合成測試成立，pv10 補 ready 事件時回頭驗、closeout 前不得刪（HANDOFF「未結事項」節）；需要新 wire 事件＝停下回報，進 pv10-requirements，不在批內改（§8 RC-9）。
- 新 Feature（功能線停）；Parallel executor（LIM-2，v1 任何 Parallel 宣告＝D-E17）；Observer、archetype、動態查詢（spec M）；ExclusiveWorld（J-45 守形狀，v1 無使用者）。
- 第三方 ECS 選庫：只在 world-exec 的事前宣告以 W-1～W-11 核對 EnTT 能否當儲存（`v8-arch-kernel-audit-third-party`），不另開評估。
- 工具的行為修正（model_viewer `ViewerWindow.cpp:112` 記帳缺陷等；audit §3.6）：不順手改；統一 editor-dialogs 若使行為變，事前宣告列為行為差異交使用者。
- AGENTS §2／§3 條文修訂（風紀委員線）。
- -zones 卡本身（產品線前置第二批，宣告已核定、實作中）；它的門（7 處殘留）與交遷移批的 24 處見 §1.1、§5.1。

---

## 2. 批次總覽

### 2.1 樹（代號｜段｜實作者 模型／檔位｜一句白話）

```text
v8-arch-migration（第一個 host＝Match；★＝關鍵路徑；◇＝閘門）
├── 階段 0  前提（全部並行；-zones 卡（宣告已核定、實作中）是 match-sim 的外部依賴）
│   ├── ★v8-arch-a6-revision          上｜主會話自寫（模型由使用者定）/high＋對抗（與主會話不同的模型）/high｜spec A.6 收頁保護 11 點、legacy 處置 12 條、kernel DI 落 target、目標 DI 清單兩層（U13）
│   ├── v8-product-pvp-baseline        下｜Opus/medium｜量今天 Match 的 tick、記憶體、到 ready 的時間（§6 的分母；重放 host 與真實行程兩種）
│   ├── v8-arch-kernel-scaffold        上｜Opus/high｜engine/kernel 空殼進 L1 三平台、J-9 自寫禁字表、編譯失敗測試形狀在 Windows L1 定案
│   └── v8-arch-h7-scanner             下｜Fable/high（規則 xhigh）｜H-7 呼叫點掃描器（共通層只有掃描器＋合成自測）；各 owner 在自己的 tests/ 註冊掃描與基線
├── 階段 1  Kernel 薄（先只做第一個 host 要的）
│   ├── ★v8-arch-kernel-registry-schedule 上｜Opus/xhigh｜Registry、簽名推描述（全部參數型別）、Scheduler、Plan 指紋三平台、錯誤代號表、G-5 匯出
│   ├── ◇G1 v8-product-pvp-plan-dryrun  上｜Opus/high｜只登記 PvP 的 System 描述跑 finish：拆欄後無第二寫者、(c) 歧義可解、Life 形狀的 Plan 謂詞成立
│   ├── ★v8-arch-kernel-world-exec      上｜Opus/xhigh｜World 欄、延後佇列、事件、hook、Serial Executor、Result／Interrupted、驗證兩層與開關
│   ├── v8-arch-kernel-loading         上｜Opus/xhigh｜Loading schedule、domain tick、ready、型別無關的暫存與游標（連續前綴；合成型別）
│   ├── v8-arch-kernel-threading       上｜Opus/xhigh｜runner 介面、親和、一 World 一執行緒、交換點原語（Mailbox／Link swap）、合成 IO 執行緒案例（測試內建立執行緒）、TSan
│   └── v8-arch-pageguard-os（條件）    上｜Opus/xhigh＋敵對 Fable/xhigh｜G2a 後 U1 決定採第三層時：頁保護平台層進 engine/os、kernel 只持策略
├── 階段 2  第一個 host：Match
│   ├── v8-arch-plugins-foundation     上｜Opus/high（交換點與啟停 xhigh）｜plugins/ top-level；最小形（plugin build 函式 API、project.json 的 plugins 欄→建置產生登記；match-sim 用）＋headless-runner 與 asset-io 服務（AssetInbox 的 IO 執行緒＋交換點；match-host 用）；刪除驗證、Windows 啟停壓力
│   ├── ★v8-arch-assets-core           上｜Opus/high｜engine/asset：Assets<T>、請求狀態 Resource、AssetInbox、StageAssets／ApplyStagedAssets、loader 登記 API（由 plugin 呼叫；loader 收位元組不收路徑）；AssetManager 轉接（期限 framework-delete）；只依賴 world-exec／loading
│   ├── v8-arch-plugin-net-link        上｜Opus/high（交換點 xhigh）｜Link Resource 家族＋IO 服務殼：只處理位元組、到達時刻與交換（Match 先用、Client 重用）
│   ├── v8-arch-diagnostics-service    上｜Opus/high（檔案生命期 xhigh）｜trace-diagnostics plugin：TraceSink／LogFile 服務化、DiagnosticsWriter 的 sink 接收端、檔案生命期契約；windows 觀察 (a)(b) 驗收
│   ├── v8-product-pvp-diagnostics     上｜Opus/high｜MovementDiag／MovementTrace 的 PvP 部分留產品、只經通用 sink 輸出；TraceMovement 時間用 HostClockSample；Match 側呼叫點改句柄（Client 側歸 client-sim）
│   ├── ★v8-product-match-sim          上｜Opus/high（Life／拆欄 xhigh）｜PvpMatch 拆成 fps::pvp::sim 的 (c) System；重放 host 上 golden 37 逐位元；換有序容器；測試、守衛與突變的去處
│   ├── ◇G2a ★v8-arch-first-host-cost  上｜量測 Opus/medium、門檻對抗（與宣告作者不同的模型）/high｜重放 host 量成本對 §6；放行 Kernel／plugins 側的 Client 前置
│   └── ◇G2b ★v8-product-match-host    上｜Opus/high（Ingress／Judge xhigh）｜真實 Match 行程跑在 kernel 上；BU-1′ 實量；移行の手間 ≤1.5；與 match-sim 同一 PR 線合併
├── 階段 2' Kernel 後半與 Engine 的「包」（G2a 之後；與階段 2 並行）
│   ├── v8-arch-kernel-rollback-extract 上｜Opus/xhigh｜H.5 回滾（Rewind／Restore／Replay）與 H.6 Extract、渲染 World 自己的 Registry
│   ├── v8-arch-engine-wrap-render     上｜Opus/high｜render、input、platform、text、audio 收成 Resource／Host 零件
│   ├── v8-arch-engine-wrap-assets     上｜Opus/high｜RenderAssets<T>、UiRenderer 局部重寫（assets-core 之後）
│   └── v8-arch-plugins-frame          上｜Opus/high｜sdl-window-runner（含 SdlInput 後端搬入）、asset-catalog、event-sounds、model-assets 四個 plugin（model-assets 取代 pvp 的最小 ufbx plugin）
├── 階段 3  Client（G2b 之後；三批同一 PR 線）
│   ├── ★v8-product-client-sim         上｜Opus/xhigh｜ClientSim Runtime：預測走回滾機制、與 Match 同一份 (c) System；測試、守衛與突變的去處
│   ├── v8-product-client-link         上｜Opus/high（交換 xhigh）｜ClientConnection → net-link 的 Client 端；windows 觀察 (c) 收口（與 client-sim 同一宣告）
│   └── ◇G3 ★v8-product-client-frame   上｜Opus/high｜PvpApplication → ClientFrame／ClientRender；Client main＝plugins 資料＋Run
├── 階段 4  Editor 與工具（G2b 與 wrap-assets 之後；與階段 3 並行）
│   ├── v8-arch-plugins-editor         上｜Opus/high｜imgui 兩半、editor-dialogs、offscreen-preview（無 readback）；PreviewTests 重寫
│   ├── v8-product-tool-voxel          上｜Opus/high｜voxel_map_editor：第一個 Editor 樣本（文件 World＋渲染 World）
│   ├── v8-product-tool-model-viewer   上｜Opus/high｜model_viewer 遷移（`pendingAction_` 改資料；取代其 DI）
│   ├── v8-product-tool-audio          上｜Opus/high｜audio_editor 遷移（取代其 DI）
│   ├── v8-product-tool-ui             上｜Opus/high｜ui_editor 遷移（`EditorApp` 介面重寫；取代其 DI）
│   └── ★v8-arch-framework-delete      上｜Opus/high｜framework/、三份文件、守衛、FRAMEWORK component、AssetManager 轉接 完整刪除（§7 驗證）
└── 階段 5  收口
    ├── v8-arch-ci-spike-retire        上｜Opus/low｜spikes/kernel 與 CI 的 spike 步驟移除（兩條件成立即可做，可早於階段 5）
    ├── ★v8-arch-runtime-loop-retire   上｜Opus/high｜刪 RuntimeLoop／IRuntimeClient（最後一個使用者遷完後）
    └── ★v8-arch-migration-closeout    上｜Opus/medium（改非 inline 一步 high；審查 Fable/high）｜legacy 刪（含 legacy DI 節）、最終掃描、LIM 逐條搬、`fps::pvp::sim` 改非 inline、Windows CI 同法重掃 60 run
```

審查者一律與實作者不同模型、檔位至少 high（實作 Opus → 審查 Fable；h7-scanner 相反；gearbox「high：…code review」）。閘門定義（集合只在這裡寫一次完整、附項目名稱；§3 與 §8 引用這裡）：

- **G1**＝`-registry-schedule` 的 Plan 指紋三平台相同，且 `-plan-dryrun` 的下列謂詞在 Plan 上全部成立（都由 Plan 匯出機械檢查，不靠審查者判斷）：(i) D-E2＝0（不靠改 Shared）；(ii) 為消 D-E8 加的順序邊中，非資料流邊 ≤50%——「資料流邊 u→v」定義為 `writes(u) ∩ (reads(v) ∪ writes(v)) ≠ ∅`，由描述算出、工具輸出比例；(iii) Life 形狀的 Plan 謂詞（U6 已定）：`LifeState` 單一寫者在 tick 開頭（`StepMovement` 之前）、`DeathEvent` 由 `ResolveActions` 發、snapshot／digest 組裝在其後讀（同一 tick）、D-E19＝0；(iv) Match 與 ClientSim 的 cFingerprint 相同。T=35／R=215 的數值在 dryrun（函式體為空）量不到，留到 match-sim 在新重放 host 上實測。
- **G2a**＝`-first-host-cost` 在重放 host 上，**BU-1（Match tick 的工作）、BU-2（Kernel 自身開銷）、BU-4（World 位元組與快照緩衝）、BU-6（載入到 ready，重放 host；分母＝pvp-baseline 以同一內容、同一重放形式量的今天數字）** 都 ≤ §6 預算（任一在 1.0～1.5 倍＝報告由使用者決定；≥1.5 倍＝停、§8 RC-1）。BU-3（finish）在 registry-schedule 量、不在 G2a。
- **G2b**＝`-match-host` 在真實 Match 行程（Linux）上 **BU-1′（真實行程 tick）、BU-4（穩態 RSS）、BU-6（真實行程到 ready）** 同樣成立、golden 37 不變，且**移行の手間**：match-sim＋match-host 的實際工時合計／兩批事前宣告核准時寫明的估時（上限值）合計 ≤1.5——分母在看結果前固定，實際工時＝各會話在 HANDOFF 記下的開始／停止時刻之差的加總（每次開始、停止、開始等 CI 或等使用者核准時各記一行；等待時間不計；第三輪對抗 N3-10）（AGENTS §2「性能、資源、移行の手間」；不成立照 §8 RC-3／RC-5 停，不開階段 3）。
- **G3**＝`-client-sim` 的回滾等價（產品版 J-42）在 L1 三平台成立。

### 2.2 表

估時欄寫「本稿區間（出所與調整）」；單位工時，區間不是承諾（audit §1.4）。**出所規則**：括號內寫出所（audit 的 P／S 列、先例批的實走時數、守衛條數）；括號寫「推定」的＝無根據的估計，U12 特別看；產品批另列「測試、守衛與突變的去處」（第一輪對抗 S-1）。線：架構＝架構師主會話；產品 M／產品 C＝兩條產品遷移線（§7 U11）；包＝Engine／plugins 線；掃描＝J-17 掃描器線；CI；文件＝圖書管理員。

| 代號 | 依賴 | 產出 | 受理判定要點（細節 §3） | 估時（h） | 線／可並行 |
|---|---|---|---|---|---|
| `v8-arch-a6-revision` | — | spec v0.4（A.6、G.4 補列、行號改條號、鏈／清單措辭）、target 修訂、kernel DI 頁、產品 target 文件（entity-model 後繼）、legacy 36 條重標、目標 DI 清單兩層（跨部件先讀／各部件）＋legacy 對照（U13） | 對抗 0 blocker；§5 歸本批的 24 列各有落字位置；36 條重標件数一致；legacy 行號引用 13 → 0（正規式限 legacy 檔名、正例先命中） | 6～12（-spec-v03 估 3～4 h、實走約一個工作日加五輪對抗；本批範圍更大，所以取其 2～3 倍：推定） | 架構；與 baseline、scaffold、h7 並行 |
| `v8-product-pvp-baseline` | — | 量測執行檔（不進 ctest）＋基準報告 CSV（`format=gyo.kernel-baseline, version=1`）；重放 host 基準（BU-1 的分母；另量今天重放 host 以同一內容（arena、動畫內容）、同一重放形式載入到可開始模擬的時間（從讀檔開始計）＝G2a 的 BU-6 分母）與**真實 Match 行程基準**（BU-1′、BU-4 RSS、BU-6 真實行程的分母，Linux） | 件数（情境數、tick 數）；同機 5 次 p95 散佈 ≤10%，否則不出基準；真實行程基準在看新路徑結果前量 | 3～8（推定；比照 `gyo_time_wake_record` 類記錄執行檔） | 產品 M |
| `v8-arch-kernel-scaffold` | — | `engine/kernel` 空殼 4 新增＋2 修改、`gyo_kernel_tests` 進 L1、J-9 禁字表＋自測、naming 第 5 條後綴掃描、`mutations.json` 骨架、編譯失敗測試兩種樣本 | 清單外零檔；L1 三平台綠（含 Linux `core`）；兩種樣本在 windows-x64 過，選定寫進 registry-schedule 宣告 | 3～6（ci-readiness 實走 2 h＋J-9 與後綴掃描＋樣本：推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**4～8** | CI |
| `v8-arch-h7-scanner` | — | `plans/kernel/scripts/h7_scan.py`＋規則資料＋合成檔自測＋突變（共通層）；object_fps_pvp 與四工具各在 `tests/<owner>/` 註冊掃描與基線 | 合成正例必報、合成反例不報；突變全殺；共通 ctest 在沒有 owner 清單時報「nothing to scan」並通過；刪一個 owner 的登記與目錄後共通層零修改、自測仍綠；各 owner 基線＝match-host／client-frame／工具批的歸零目標 | 4～8（J-17 線實走：規則與自測 4＋突變 1＋基線與 ctest 1.5＋文件 1；規則更少、owner 註冊更多）＋守衛去處判斷 1～2（推定，§3 共通）＝**5～10** | 掃描 |
| `v8-arch-kernel-registry-schedule` | a6-revision（DI 頁、鏈措辭）、scaffold（編譯失敗形狀） | Registry／描述（**全部**參數型別的描述與 Contribution）／Scheduler／Plan（含資料流邊比例匯出）／錯誤代號表／G-5 匯出；不跑 System；API 名稱（含 `Runtime` 與 naming 第 5 條的處理） | J-10～J-25、J-27（D-E17 半）、J-28（finish 半）、J-37～J-41、J-44、J-47 三平台；Plan 指紋 golden 三平台相同；J-44 20 種排列；突變 2、3、4、5、6、11、14 killed；BU-3（finish） | 20～40（守衛約 25 條×1～2 h＝25～50 的下修：推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**21～42** | 架構 |
| `v8-product-pvp-plan-dryrun` | registry-schedule；-zones 建議先完成（非必要） | `tests/object_fps_pvp` 的描述登記測試（空函式體） | G1 的四個謂詞（§2.1）由 Plan 匯出機械檢查；manifest 查證（N-11）記進宣告 | 6～12（推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**7～14** | 產品 M |
| `v8-arch-kernel-world-exec` | registry-schedule（U1 已定：欄配置留介面，第三層延到 G2a 後定） | World、F.2、事件、hook、Serial Executor、Result／Interrupted、驗證兩層＋epoch、`KernelSettings`、`DiagnosticsWriter<T>` World 端緩衝 | J-1～J-8（J-6 的 ownerThread 半在 threading）、J-26、J-29～J-37、J-32b、J-45、J-50、J-52、J-53 三平台；同一執行檔開／關兩個 World；突變 1、7、8、9、12、13、16 killed；BU-2（Kernel 自身開銷，初量） | 24～48（推定；spike 搬改 ci-readiness §2.2 表＋守衛約 20 條）＋守衛去處判斷 1～2（推定，§3 共通）＝**25～50** | 架構；G1 前可開工，G1 不成立即停 |
| `v8-arch-kernel-loading` | world-exec | Loading schedule、domain tick、`CurrentTick`、型別無關的暫存與游標（以請求序號為鍵、連續前綴；套用到 `Assets<T>` 的 System 在 assets-core）、`LoadingStageDone<Stage>`、ready；runtime-composition DI 3 的 Startup／Loading 守衛 | J-48、J-51（合成型別，Kernel 端）三平台；runtime-composition DI 3 等價測試與兩條守衛（Startup 不寫世界內容欄、Loading 非 Ingress stage 不讀 external）；BU-6 的每批上限（合成） | 10～20（推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**11～22** | 架構；與 threading 並行；不在關鍵路徑 |
| `v8-arch-kernel-threading` | world-exec | runner 介面、親和檢查、`ownerThread`、交換點原語（`Mailbox<T>`、Link swap）、TSan 案例 | J-6 的 ownerThread 半（W-6）、J-17b、J-28（執行期半）三平台；TSan 零報告；D-E13；合成的 IO 執行緒案例（測試內建立執行緒、在交換點餵入合成 external Resource、X-3 親和）——正式的 `AssetInbox` IO 服務不在本批（Kernel 不擁有執行緒，DI-KS 15／J-9），在 plugins-foundation | 12～24（推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**13～26** | 架構；與 loading、assets-core 並行；不在關鍵路徑 |
| `v8-arch-pageguard-os`（條件） | G2a 後使用者決定採第三層（U1）；world-exec | `engine/os` 平台層、kernel 第三層策略、`gyo_kernel_pageguard_tests`（`SKIP_RETURN_CODE 77`）、os.md DI 加條 | spike 同組測試重新宣告重跑，L1 **RelWithDebInfo** 三平台綠；os 邊界守衛對 kernel 零命中；BU-8（頁保護） | 10～20（-spike-pageguard 實走加 OS 層：推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**11～22** | 架構或 CI；不在關鍵路徑 |
| `v8-arch-plugins-foundation` | 最小形：registry-schedule；headless-runner 與 asset-io 服務：threading（交換點原語與親和的契約） | `plugins/` top-level；**最小形**（plugin 介面與 build 函式 API、`project.json` 的 `plugins` 欄與建置產生的登記；Delta-5 實作）先交給 match-sim；headless-runner plugin；**asset-io 服務**（IO 執行緒讀檔、在交換點把完成項寫進 `AssetInbox`；Client 由 sdl-window-runner 啟動同一服務）；依賴守衛、合成 plugin 的加入／刪除測試（合成 plugin 與合成 `project.json` 放測試夾具，不常駐 `plugins/`） | engine 不 include plugins（掃描）；刪合成 plugin 共通層零修改；缺 plugin／依賴衝突是明確 configure 錯誤；R-E6；`-ffp-contract=off` 對 plugins target 生效的 configure 檢查；headless-runner 的 Windows 啟停壓力 N＝200 零逾時；asset-io 服務的交換點測試（兩執行緒、Running 期間零外部存取）TSan 零報告、`engine/` 無 `<thread>`（J-9）；守衛與突變的去處（§5.2 第 11、12 列初判） | 18～36（HeadlessShell 0＋S1 半 5～10＋build 層 5～10＋產生登記 4～8＋asset-io 服務 4～8；後三項推定；最小形＝build 層＋產生登記 9～18）＋守衛去處判斷 1～2（推定，§3 共通）＝**19～38** | 包；最小形在 match-sim 前、全批在 match-host 前並行完成，不在關鍵路徑（registry-schedule → 最小形 9～18 h，短於 world-exec → assets-core 37～82 h） |
| `v8-arch-assets-core` | 開工：world-exec（Resource 型別）；結束：loading（J-51 真資料版由本批執行、記本批 dev_log）——**不依賴** threading 與 plugins-foundation（`v8-arch-migration-plan-round2`：本批只提供 loader 登記 API，由 plugin 在 build 函式呼叫——登記者是 plugin，不是 host 或產品程式直接登記；`AssetInbox` 在重放 host 由測試同步餵入，正式的 IO 執行緒服務在 plugins-foundation） | `engine/asset`（依賴 `engine/kernel`；kernel 不知道 asset）：請求狀態 Resource、`Assets<T>`、`AssetInbox` Resource、`StageAssets`／`ApplyStagedAssets` System（套用 loading 的型別無關暫存與游標；由 `engine/asset` 提供的登記函式登記進 Loading schedule，呼叫者是 runner 側 plugin（headless-runner／sdl-window-runner 的 asset-io 部分），重放 host 由測試呼叫同一函式；`engine/asset` 的 (c) System 不在 `gyo.zone-c-sources` 的 owner 清單，照 zone-c-sources §3 由 assets DI 的守衛負責——第三輪對抗 N3-12）、loader 登記 API（loader 收位元組或 `AssetInbox` 的完成項、不收路徑——-zones 的 B 5 處由此消去，match-sim 消費）；`AssetManager` → `Assets<T>` 的轉接（期限：framework-delete 刪）；io 檔案服務包；`engine/model` 只提供 loader 函式（不登記、不連 Kernel） | J-51 的真資料版（本批的 `ApplyStagedAssets` 接 loading 的暫存與游標）；`gyo_expect_model_links` 不變；`AnimationContentDigest`／`ArenaContentDigest` 不變；資產等價測試（打亂完成順序，`Assets<T>` 與 Handle 序列相同，assets DI 2）；轉接的使用處逐列登記（framework-delete 的歸零清單） | 11～30（audit asset 8～20＋io 2～6＋model 1～4；由原 wrap-assets 19～48 拆出，拆法推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**12～32** | 包；與 loading、threading 並行；在關鍵路徑（world-exec → assets-core → match-sim） |
| `v8-arch-plugin-net-link` | threading、plugins-foundation | `plugins/net_link/`：`Link`、`LinkMembershipInbound`、`LinkInputInbound`、`Link.outbound` 的 Resource 型別、swap、IO 服務殼（asio、httplib、protobuf 內部）；只處理位元組、到達時刻與交換 | 合成 Link 的 swap 測試（兩執行緒、Running 期間零外部存取）；J-41 在 plugin 登記下成立；plugin 不 include 產品 proto 與 `fps::pvp`（掃描先用正例驗證）；刪 object_fps_pvp 後 plugin 自測仍綠；protobuf pool 若由 Egress 直接編碼列 I-5 例外 | 10～20（從 audit P3 20～40、P4 30～50 抽出共用殼；抽法推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**11～22** | 包；Match 側第一個消費者 |
| `v8-arch-diagnostics-service` | world-exec（`DiagnosticsWriter` 參數）、plugins-foundation 全批（交換點交 sink 要 threading 的交換點原語，經它間接依賴） | `plugins/trace_diagnostics/`：`TraceSink`／`LogFile` 服務化、`DiagnosticsWriter<T>` 的 sink 接收端、檔案生命期契約；**不含任何 PvP 型別** | windows 觀察 (a)：Close 後無開啟句柄，同形測試在測試內重複 100 次零失敗；(b)：Close 尾行順序契約測試 100 次相同；J-14 的 Diagnostics 例外不擴大；trace 既有測試全過；plugin 不引用 `fps::pvp`／`RetroFPS`／`object_fps_pvp`（掃描先用正例驗證）；刪 object_fps_pvp 後 plugin 仍 build 並通過自測 | 6～14（trace 2～6＋sink 接收端與檔案生命期 4～8；後者推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**7～16** | 包；與 match-sim 並行（介面誰先定寫在宣告） |
| `v8-product-pvp-diagnostics` | diagnostics-service、world-exec（`DiagnosticsWriter<T>`） | `MovementDiag`／`MovementTrace` 的 PvP 部分（P8／P12）留在 `apps/object_fps_pvp`，改成只經通用 sink 輸出；`TraceMovement` 經 `DiagnosticsWriter<T>`、時間用被記錄的 boundary 輸入 `HostClockSample`（牆鐘語義保留） | trace／log 檔案格式與欄位語義不變（逐欄對照；任一欄語義變＝升契約版本並列消費者，不得默改）；Match 側呼叫點不經 `MovementDiag.hpp:109` 的自起 `std::thread` 與全域 atomic 指標（全域與寫檔執行緒為舊 Client 保留到 client-sim）；`kernel_prep_sites` G 類 13 列中 **Match 側 6 列**（`PvpMatch.cpp` 4、`MatchRuntimeHost.cpp` 2）逐列歸零、**Client 側 7 列**（`LocalPlayerPrediction.cpp`）歸 client-sim；-zones 的 E 14 處：本批加句柄形式並改 Match 側，Client 側呼叫點遷完時歸零（§5.1）；位置由產品線提、架構師核（kernel-prep 卡）；守衛與突變的去處 | 6～12（audit P8 8～16＋P12 4～10 扣 diagnostics-service 已做的服務部分：推定）＋測試、守衛與突變的去處 2～4（推定）＝**8～16**＋守衛去處判斷 1～2（推定，§3 共通）＝**9～18** | 產品 M；與 match-sim 並行需要第二張產品卡（接點：Sim 的 (c) System 持 `DiagnosticsWriter<T>`，由 match-sim 定）；不開時接在 match-sim 後、match-host 前，關鍵路徑加 9～18 h |
| `v8-product-match-sim` | plan-dryrun（G1）、world-exec、loading、assets-core、plugins-foundation 的最小形（產品 (c) plugin 的 build 函式在那裡登記 System 與 P13 loader）、-zones 卡完成（`object_fps_pvp.zone_c_lists` 綠：J-17 確認清單＝核定的 7 處殘留、`test_zone_c_lists.py` 的 RESIDUAL 釘住，**不是 0**）、pvp-baseline | 產品 (c) plugin（`fps::pvp::sim`：-zones 定的 inline namespace，v1；closeout 改非 inline）、Body 四欄、Life 形狀、`ApplyMembership`、`AuthorityDigest` System、重放 host 跑 Match Runtime；P13 載入器改收位元組（-zones 的 B 5 處）；-zones 的 F 2 處（`App/AnimationSetDefinition` 的 `unordered_map`）換有序容器；**測試、守衛與突變的去處**（`tests/object_fps_pvp` 中引用 `PvpMatch` 的 Sim 層測試逐檔搬改／重寫／刪；碰到的守衛與突變四選一） | golden 37 逐位元（LIM-5 例外只限事前列名）；D1 的 T=35／R=215 在新重放 host 上以 snapshot(T)／snapshot(R) 實測、突變改針對新 System 仍 killed；J-17 確認清單開工時＝RESIDUAL 的 7 處、本批消去 B 5 與 F 2 時 RESIDUAL 逐列刪（締める面）、新 Sim 檔零新增違規（清單由 G-5 匯出）；換有序容器後 digest／golden 零差；Loading 等價（分批數、完成序打亂）；cFingerprint 存證據；DI 守衛測試（`EntityModelTests` 等）照 §14.4 交使用者 | 35～78（audit P1 40～80＋P7／P9／P13 4～18＝44～98，扣 -zones 已做約 20%：扣法推定）＋測試、守衛與突變的去處 10～20（推定）＝**45～98**＋F 換容器 1～3＋守衛去處判斷 1～2（推定，§3 共通）＝**47～103** | 產品 M |
| `v8-arch-first-host-cost`（G2a） | match-sim、pvp-baseline | 成本報告＋go／no-go | 件数（tick 數＝記錄數、5 次）；G2a 集合（§2.1：BU-1、BU-2、BU-4 World 位元組、BU-6 重放）全在預算＝go；1.0～1.5 倍＝使用者決定；≥1.5 倍＝停（§8 RC-1）；驗證開／關各量一次 | 4～8（推定） | 架構 |
| `v8-product-match-host`（G2b） | match-sim、first-host-cost go、plugins-foundation 全批（headless-runner＋asset-io 服務：`AssetInbox` 在交換點餵入、X-3 親和；依賴 threading 的契約）、net-link（Link 在交換點餵入）、diagnostics-service、pvp-diagnostics、h7-scanner | headless-runner 上的 Match Runtime；Ingress（`StageMembershipRequests`、`StageInputs`）／Judge／Egress；`IpcHost` → net-link 服務（種類判別與 proto 解碼在產品側 adapter）；`match_main`＝plugins 資料＋Run；刪舊 `PvpMatch`、`QueueActionAcknowledgement`／`SubmitActions`；**測試、守衛與突變的去處**（host 層測試：`IpcHostTests`、`KernelPrepDetectTests` 的 D3 等） | `kernel_prep_sites` 白名單 A 22 列→0、W 類只剩 runner／Host 檔、**Match 側** G 類 6 列→0（Client 側 7 列在 client-sim；逐列對帳）；Match 側 `AssetManager` 轉接使用處 0；`zone-c-rewrite.json` 的 P1 列移除；H-7 掃描 Match 0；golden 37；Gateway go 測試、pvp 測試、acceptance 30 全過；D3 host 層特徵在新 host 上重現；**G2b 集合（§2.1：BU-1′、BU-4 RSS、BU-6 真實行程）在真實行程（Linux）實量**；移行の手間 ≤1.5 | 55～120（audit P2 40～70＋P3 20～40＋S1 半 5～10＝65～120，扣 net-link 5～10，加補列 0～5）＋測試、守衛與突變的去處 10～20（推定）＝**65～140**＋守衛去處判斷 1～2（推定，§3 共通）＝**66～142** | 產品 M；與 match-sim 同一 PR 線 |
| `v8-arch-kernel-rollback-extract` | world-exec、loading；G2a go | H.5 `Rewind`／`Restore`／`Replay`、PredictionHistory；H.6 `Extract<Read<T>>`／`ExtractRes<R>`、渲染 World 自己的 Registry | J-42、J-42b、J-42c、J-43（含 `Extract<Write>` 編譯失敗）、J-46、J-49 三平台；突變 10、15 killed；BU-5（PredictionHistory）合成記錄 | 24～44（推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**25～46** | 架構；與 match-host 並行 |
| `v8-arch-engine-wrap-render` | world-exec、threading（親和 `GraphicsContextOwner`、audio `Submitter`）；G2a go | render（渲染 World Resource、親和 `GraphicsContextOwner`）、input 後端介面（Host 零件）＋core 留、platform Host、text 包、audio `Submitter` | 各模組既有測試全過；platform-sdl G1～G3、input IP-1 照守衛去處表；`gyo_expect_*_links` 只照宣告列出的變更、其餘不變；跨元件跨幀裸指標 4 處→0；naming 第 5 條後綴掃描新程式零命中；input 的 `<chrono>` 只在 (b) World（legacy-impact §9 第 8 點） | 19～53（audit §1.1 不調）＋守衛去處判斷 1～2（推定，§3 共通）＝**20～55** | 包 |
| `v8-arch-engine-wrap-assets` | assets-core、wrap-render | `RenderAssets<T>`；`UiRenderer` 局部重寫為 System＋`Gpu*` Resource；`WeaponViewModel` 的跨 World 指標消失 | ui 既有測試全過；`engine/ui` 無 `AssetManager*`／`&` 成員（掃描範圍只有 engine/，全 repo 歸零在 framework-delete）；改名清單（target/assets §2）在宣告列出 | 8～18（audit ui 8～18 只估 `UiRenderer`；`RenderAssets<T>` 沒有獨立出所——audit asset 8～20 拆批時全數歸 assets-core、沒有餘量——本稿計在這 8～18 內，推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**9～20** | 包 |
| `v8-arch-plugins-frame` | wrap-render、plugins-foundation、rollback-extract（合成 Client 形狀的 Extract） | sdl-window-runner（含 `SdlInput` 後端程式碼搬入）、asset-catalog、event-sounds、model-assets 四個 plugin（model-assets 取代並刪 pvp 的最小 ufbx plugin） | 刪任一 plugin 只壞 audit §10.4 表列的消費者；event-sounds／catalog 資料契約 DI 照搬（owner 改）；R-E6 | 15～30（WindowShell 16～32、Catalog 5～10 的一部分）＋守衛去處判斷 1～2（推定，§3 共通）＝**16～32** | 包 |
| `v8-product-client-sim` | G2b、rollback-extract、net-link | ClientSim Runtime（Loading＋FixedUpdate rollbackEnabled）、runner 的時鐘策略、`LocalPlayerPrediction` → Restore／Replay 的使用者；**測試、守衛與突變的去處**（`PredictionTests` 等） | 產品版 J-42（直接跑 k tick 對 Rewind＋Replay 逐位元）三平台；cFingerprint＝Match（H-4）；預測與對帳的性質在新路徑以同一劇本重現；D2 棘輪 Client 側歸 0；`kernel_prep_sites` G 類 Client 側 7 列→0；-zones 的 D 3 處→0、E 14 處中 `LocalPlayerPrediction` 內的呼叫點改句柄（14→0 在 client-frame）、`zone-c-rewrite.json` 的 P5／P6 列移除（§5.1）；BU-7 的 Client 模擬 tick 項；windows 觀察 (c) 的收口在 client-link 同一宣告（PR 線末端） | 50～100（audit P5 20～40＋P6 30～60）＋測試、守衛與突變的去處 8～16（推定）＝**58～116**＋守衛去處判斷 1～2（推定，§3 共通）＝**59～118** | 產品 C |
| `v8-product-client-link` | G2b、net-link | `ClientConnection` → net-link 的 Client 端服務＋Link（種類判別與 proto 解碼在產品側 adapter）；**測試、守衛與突變的去處**（network 標籤測試） | network 標籤測試本機全過；`client_delay_runner` 等輸出欄位不變；IO 執行緒不碰 World（掃描＋審查）；`ClientConnection.cpp:714、803` 的 `TraceMovement` 改句柄（E 14 處的 Client 呼叫點之一）；**windows 觀察 (c) 收口**：以事前宣告的 N 次 workflow_dispatch 重跑（N 由基準發生率定，使「無改善卻 0 次逾時」機率 ≤5%）0 timeout | 22～40（audit P4 30～50 扣 net-link）＋測試、守衛與突變的去處 4～8（推定）＝**26～48**＋守衛去處判斷 1～2（推定，§3 共通）＝**27～50** | 產品 C；與 client-sim 並行（第二張卡） |
| `v8-product-client-frame`（G3 後合併） | client-sim（G3）、client-link、wrap-assets、plugins-frame | `ClientFrame`／`ClientRender`、P10 包（時鐘改 `FrameTime`）、`src/App` 拆包、`PvpApplication` → plugins 資料＋Run；**測試、守衛與突變的去處**（App／呈現層測試） | H-6／J-43 產品版；呈現 System D-E8＝0 且 LIM-1 成本記錄；D2 棘輪 Client 側 0；H-7 掃描 Client 0；acceptance 全過；`PvpApplication.cpp:1369`、`main.cpp:157` 改句柄後刪診斷全域與寫檔執行緒、**-zones 的 E 14 處→0**；Sim 外讀檔入口的 Client 使用處 0、入口刪；Windows 實機一場（離線場次腳本）；BU-7（Client 幀） | 61～122（audit P11 40～80＋P10 10～20＋App 6～12＋S1 半 5～10）＋測試、守衛與突變的去處 4～8（推定）＝**65～130**＋守衛去處判斷 1～2（推定，§3 共通）＝**66～132** | 產品 C；與 client-sim、client-link 同一 PR 線 |
| `v8-arch-plugins-editor` | wrap-assets、rollback-extract、plugins-frame；G2b | imgui 兩半、editor-dialogs（三種記帳統一）、offscreen-preview（無 readback）；`PreviewTests` PV2～PV5 與 pv-* 突變重寫 | H-6 原樣（無例外、無晚一幀）；每個 plugin 刪除驗證；editor-dialogs 被 ≥2 個工具使用；重寫的測試以突變證明檢出力 | 20～40（audit framework 33～66 扣 plugins-frame 已做：扣法推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**21～42** | 包 |
| `v8-product-tool-voxel` | plugins-editor | voxel_map_editor：main＝plugins 資料＋Run、`EditSession` 等包、W6／W7 守衛觀測手段重寫；測試、守衛與突變的去處 | 工具既有測試全過；`gyo.voxel_map` 不變；H-7 掃描 0；預覽無 readback；取代的 DI 照 §14.4 交使用者 | 24～48（audit §1.2）＋測試、守衛與突變的去處 2～4（推定）＝**26～52**＋守衛去處判斷 1～2（推定，§3 共通）＝**27～54** | 包；第一個 Editor 樣本 |
| `v8-product-tool-model-viewer` | tool-voxel | model_viewer 遷移；`pendingAction_` 改資料；測試、守衛與突變的去處 | 同上 | 16～32（audit §1.2）＋2～4（推定）＝**18～36**＋守衛去處判斷 1～2（推定，§3 共通）＝**19～38** | 包；三工具三卡並行 |
| `v8-product-tool-audio` | tool-voxel | audio_editor 遷移；測試、守衛與突變的去處 | 同上 | 8～16（audit §1.2）＋2～4（推定）＝**10～20**＋守衛去處判斷 1～2（推定，§3 共通）＝**11～22** | 包 |
| `v8-product-tool-ui` | tool-voxel | ui_editor 遷移；`EditorApp` 介面重寫；測試、守衛與突變的去處 | 同上；若要改 `GYO::Ui` 介面→停、重排 | 32～56（audit §1.2）＋2～4（推定）＝**34～60**＋守衛去處判斷 1～2（推定，§3 共通）＝**35～62** | 包 |
| `v8-arch-framework-delete` | 四工具批、client-frame | 刪 `framework/`、`docs/framework/` 三份、`tests/framework/`、`FRAMEWORK` component、`GYO_ENABLE_FRAMEWORK`、`ToolRegistryTests` 保留名、`framework_dependency_guard.py`、**`AssetManager` 與其轉接**（最後消費者 tool-ui／client-frame 之後） | -remove-object-fps 五條判定同法；共通層改動逐條列（預期非零：Delta-2 影響檔）；空目錄 0；`AssetManager` 全 repo（engine／plugins／apps／tools）引用 0 | 4～8（-remove-object-fps 實作 27 分＋掃描一次停過；本批範圍大數倍：推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**5～10** | 包；在關鍵路徑（client-frame → framework-delete → runtime-loop-retire） |
| `v8-arch-ci-spike-retire` | registry-schedule 的 J-12 與 world-exec 的 J-5 三平台綠＋（pageguard-os 完成 或 G2a 後 U1 定不採） | 刪 `spikes/kernel/`、CI spike 步驟、上傳路徑、摘要列（`.github/` 由使用者改） | ci-readiness §2.6 兩條件逐條勾；`spikes/kernel` 引用 0（歷史 dev_log 除外） | 2～4（推定） | CI |
| `v8-arch-runtime-loop-retire` | client-frame、framework-delete | 刪 `RuntimeLoop`／`IRuntimeClient` 與測試；`FixedTickRuntime` 留 runner plugin | include 0；H-7 (a) 改為「檔案不存在」也過；`engine_tests` 案例逐條對照只減被刪的 | 6～15（audit runtime 8～20，只刪不寫）＋守衛去處判斷 1～2（推定，§3 共通）＝**7～17** | 架構 |
| `v8-arch-migration-closeout` | 全部 | 刪各部件最後一批沒刪的 legacy 檔（含 legacy DI 節；legacy determinism §7／E0～E5 的去處在此定）；最終掃描；target/naming、target/zone-c-sources、target/limits（LIM 逐條搬或標 Lifted）與 target/README 搬回或收掉（其餘部件已在 runtime-loop-retire／framework-delete 搬回，§3.32）；`fps::pvp::sim` 改非 inline；Status 行清；Windows CI 同法重掃 60 run 比較 | 同名並存 0；反向依賴 91 處 0 斷鏈；**改非 inline：`inline namespace sim` 0 命中、全建置與 pvp ctest 過、golden／digest 零差**；A10 歸零；`tally.md` 同法比較表；圖書管理員 checkup 項全勾 | 10～20（推定）＋守衛去處判斷 1～2（推定，§3 共通）＝**11～22**＋改非 inline 2～4（推定）＝**13～26** | 文件＋CI |

**合計**：約 **632～1,299 h**（不含條件批 11～22 h；其中測試、守衛與突變的去處 46～92 h——第一輪對抗 S-1 後新列 44～88、第二輪 N2-8 加 pvp-diagnostics 2～4——、守衛去處判斷 29～58 h（§3 共通規則第 6 點）、match-sim 的 F 換容器 1～3 h、closeout 的改非 inline 2～4 h，全為推定）。第一輪對抗前為 560～1,140 h、第一輪後 594～1,222 h；第二輪 +30～61 h（plugins-foundation 的 asset-io 服務 4～8、pvp-diagnostics 的測試去處 2～4、F 換容器 1～3、守衛去處判斷 23～46）；第三輪 +8～16 h（只有「新增」守衛的 6 批的守衛去處判斷 6～12——S3-4、closeout 的改非 inline 2～4——S3-7）；三案分別 545～1,070／530～1,050／560～1,130，同一量級。Kernel 本體五批（registry-schedule＋world-exec＋loading＋threading＋rollback-extract）約 **95～186 h** 沒有 audit 數字可引，各批都標「推定」：成本案的「守衛條數×1～2 h」只是起點，套到各批對不上（registry-schedule 25 條→公式 25～50 而本稿 20～40（不含守衛去處判斷）；loading 3 條→10～20），所以不宣稱公式。**第一次重估的時點**：G1 成立時（registry-schedule 的實際／估計比，只重估 Kernel 四批）；第二次：match-sim＋match-host 完成後（§8 RC-3）。

**需使用者同意的指派**（navigator〈モデルを決める人〉：主會話的模型與檔位由使用者定；下列若超過主會話，開批前取得同意）：(1) 檔位 **xhigh** 的實作者與審查者——registry-schedule、world-exec、loading、threading、pageguard-os、rollback-extract、client-sim 全批，以及 plugins-foundation／net-link／diagnostics-service／match-sim／match-host／client-link 標 xhigh 的段；審查 Fable／xhigh 的批（registry-schedule、world-exec、threading、net-link、match-sim、match-host、rollback-extract、client-sim）；(2) 模型 **Fable** 的指派——全部審查者（實作 Opus 的批）、h7-scanner 的實作者、pageguard-os 的敵對檢查者；(3) 門檻對抗檢查者「與宣告作者不同的模型」若落在 Fable 亦同；(4) rollback-extract 的驗證者 Opus／xhigh；a6-revision 的對抗者（主會話是 Opus 時就是 Fable）；突變一覽的敵對檢查者若落在 Fable（實作 Opus 的批）亦同。a6-revision 的「主會話自寫」與 first-host-cost 的「判定由主會話下」不指定模型。

### 2.3 關鍵路徑與里程碑

```text
a6-revision 6～12 → registry-schedule 21～42 → world-exec 25～50 → assets-core 12～32
 → match-sim 47～103 → first-host-cost 4～8 (G2a) → match-host 66～142 (G2b)
 → client-sim 59～118 (G3) → client-frame 66～132 → framework-delete 5～10 → runtime-loop-retire 7～17 → closeout 13～26
＝ 約 331～692 h（plan-dryrun、loading 10～20、threading 與 world-exec／assets-core 並行，不在路徑上；plugins-foundation 的最小形在 match-sim 前、全批與 net-link、diagnostics-service、pvp-diagnostics 在 match-host 前並行完成；工具線 match-host → plugins-editor → tool-voxel → tool-ui 83～158 h 短於 client-sim → client-frame 125～250 h）
```

第一輪對抗前的路徑是 world-exec → loading → match-sim（290～590 h）。第一輪對抗 B-4 把資產核心（assets-core，11～30 h）排到 match-sim 之前、與 loading（10～20 h）並行，路徑改經 assets-core，多 1～10 h；assets-core 不依賴 threading 與 plugins-foundation。第二輪（`v8-arch-migration-plan-round2`）：loader 照 target/assets §1 第 3 點由 plugin 登記，所以 **match-sim 依賴 plugins-foundation 的最小形**（registry-schedule 之後 9～18 h 可交，短於 world-exec → assets-core 的 37～82 h，不進路徑）；正式的 `AssetInbox` IO 服務在 plugins-foundation 全批（依賴 threading），只擋 match-host，也不進路徑；路徑補上漏掉的 framework-delete（第二輪對抗 N2-2）；路徑上各批另加守衛去處判斷與 match-sim 的 F 換容器（§3 共通規則第 6 點；架構師 2026-11-12 判定後加，路徑由 318～665 變 328～686 h；第三輪再加 registry-schedule 的守衛去處判斷（S3-4）與 closeout 的改非 inline（S3-7），變 331～692 h）。其餘增量來自 第一輪對抗 S-1 的測試、守衛與突變的去處（關鍵路徑上 32～64 h：match-sim 10～20＋match-host 10～20＋client-sim 8～16＋client-frame 4～8；第二輪對抗 N2-1 修正了先前多算 client-link 的 36～72）與 N-2 的算術修正。

| 里程碑 | 在哪批結束 | 誰看到什麼 |
|---|---|---|
| M1 | registry-schedule＋plan-dryrun（G1） | 開發者：`gyo_kernel_tests` 三平台綠；合成與 PvP 的 (c) System 有歧義、多寫者時 finish 一次列出；Plan 可印出、三平台指紋相同 |
| M2 | match-sim | 使用者：37 個 golden 由 kernel 上的 PvP 模擬產生、逐位元相同 |
| M3 | first-host-cost（G2a） | 使用者：同機新舊兩路的 tick p50／p95／max、World 記憶體、載入時間，與 go／no-go |
| M4 | match-host（G2b） | 使用者：Linux 上真實 Match 行程跑在 kernel 上，用現有 Client 連 Gateway 對戰；統計行印 tick p50／p95／max |
| M5 | client-frame（G3 後） | 玩家：Client 全在 kernel 上；畫面、預測與今天一致；loading 畫面在 ready 前出現 |
| M6 | tool-ui（最後一個工具） | 工具使用者：四個工具在 plugins 上，預覽無 readback |
| M7 | closeout | 所有人：`docs/architecture` 只有一套；framework、RuntimeLoop 不存在 |

到 M3 約 115～247 h（關鍵路徑：6＋21＋25＋12＋47＋4／12＋42＋50＋32＋103＋8）；到 M4 約 181～389 h。**U11 已定 (a)**：分階段開三條常設線——**G1 過開「產品-Match」、G2a 過開「Engine／plugins」、G2b 過開「產品-Client」**；原則：能並行就並行、不讓產品線卡著，**某線前置齊了不等閘門湊整**。G2a 前的 Engine／plugins 批（plugins-foundation、assets-core、net-link、diagnostics-service）由架構師線做。照這個開線法（架構線、產品-Match、Engine／plugins、產品-Client 各自串行；工具三卡並行）做 list scheduling，完成點約 **383～792 h**——331～692 h 是依賴的下界（假設卡數不限），差額來自 G2a 前架構線同時扛 Kernel 與四個包批、產品-Match 線上 pvp-diagnostics 與 match-sim 串行（估算非最佳排程，只作量級；計算未保存）。對照：只維持現有四條線（不開 U11 的兩條產品線與包線）時，產品批全部串行、Engine 包進架構師線，完成時點約 475～968 h，比 331～692 h 長約 **144～276 h**（第二輪重算，N2-13：依 §2.2 的依賴與估時，以「架構線（Kernel＋Engine／plugins 批）」與「產品線（全部產品批）」兩條各自串行做 list scheduling 的估算，非最佳排程、只作量級；計算未保存，可依 §2.2 重現）。

### 取捨（§2）

- **Kernel 的第一批是 Registry＋Scheduler（風險案），不是 World（價值、成本案）**。理由：沒有 spike 證據的兩件事——Plan 唯一且三平台相同、PvP 能在獨占＋區規則下表達——都只需要 Registry 與 Scheduler 的 finish 就能證偽（dryrun 的函式體是空的）；World 的存取推導與兩層驗證 spike 已證過一半。成本案說「Scheduler 讀 World 的存取描述與 writePolicy」，但描述與 `ColumnInfo` 是 Registry 的資料，不是 World 的儲存，所以依賴方向不成問題。代價：world-exec 的實作者要等 registry-schedule 的型別定稿才能接（約 1～2 週串行）；補救＝world-exec 在 G1 前就可開工，G1 不成立即停。
- **baseline、scaffold、h7-scanner 各成一批（成本案；風險案有 baseline）**，價值案把它們併進別批。理由：三者都便宜（各 3～8 h）、都在關鍵路徑外、都給後面的批一個「看結果前就有的」門檻（分母、Windows 的編譯失敗形狀、H-7 歸零目標）；併進大批會讓那些門檻在結果出來後才定（違 §14.1）。
- **loading 與 threading 各成一批（風險案）**，價值、成本案併進 schedule／plugins。理由：Loading 有自己的契約七點與 J-48、J-51，threading 有 H-2／H-3 與 TSan，各自可單獨證偽並行；代價：批數多、交接多（本稿 34 批對價值案 16 批）。
- **資產核心獨立成批、排在 match-sim 之前（第一輪對抗 B-4）**：match-sim 的資產階段等價與 G2a 的「到 ready」都要真資料的 `Assets<T>`／`AssetInbox`，原本排在 G2a 之後的 wrap-assets 等於讓關鍵路徑依賴還不存在的 Contract；兩種繞法（Host 在 Startup 前用舊 `AssetManager` 寫內容＝違 runtime-composition DI 3／assets DI 3；產品自做臨時 `Assets<T>`＝第二份同一事實）都不採。代價：關鍵路徑改經 assets-core（11～30 h）而非 loading（10～20 h），多 1～10 h（§2.3）。loader 照 target/assets §1 第 3 點**由 plugin 登記**（engine/asset 只提供登記 API；`v8-arch-migration-plan-round2` 撤回第一輪修訂時的「host／產品程式直接登記」），所以 match-sim 依賴 plugins-foundation 的最小形——它在 registry-schedule 之後 9～18 h 可交，短於 world-exec → assets-core，不進關鍵路徑。
- **rollback 與 extract 合一批（價值、成本案）**，不照風險案拆兩批：兩者都只在 Client／Editor 線用、守衛數少（6 條）、同一實作者連做省交接。
- **net-link 獨立一批（成本案）**：Match 與 Client 是兩個已點名的消費者（spec I.2 的 Link 家族是契約），不算單一 use case 的一般化（AGENTS §1）。風險：兩端形狀分不出共同部分 → 停、拆成兩個 plugin（§3.12）。
- **閘門兩段（統合者）**，替代風險案「閘門在 match-host 後」與價值／成本案「閘門在重放 host 後」：見 §1.2 第 8 條與 §7 U4；放棄的代價寫在 U4。
- **match-sim 與 match-host 同一 PR 線（風險案）**，不採價值案的有界並存：見 §7 U3。
- **工具四批的段**：三案都把 voxel 以外的三個放下段；第一輪對抗 S-12 指出它們各自取代該工具的 Design Invariants（model_viewer DI 1、2、4；audio_editor DI 1；ui_editor DI 1～3）並改 Product Registration 的資料格式（`project.json` 的 plugins 欄），照 navigator〈上段と下段〉是上段。本稿四批全部上段（不拆小批：拆出的「碰 DI 的部分」就是整批的主體，拆了只剩檔案搬移）；下段的 low 定型作業（檔案搬移、CMake 清單）仍可用小模型，計畫寫明即可（navigator：上段中的 low 定型作業）。closeout 同理改上段（刪 legacy 的 DI 節、把 target 的 DI 搬回是 AGENTS §3 的 Delta）。

---

## 3. 每批

**各批共通（不重複寫）**：
- 事前宣告檔：架構批 `docs/architecture/plans/kernel/<批>-declaration.zh-Hant.md`，產品批 `docs/object_fps_pvp/plans/v8/<批>-declaration.md`，工具批放各工具的 plans；看結果前宣告門檻、件数の突き合わせ、範囲の限界、改名清單（target/naming）、被取代的 legacy 條文；使用者核准再開工（AGENTS §14.1；**宣告未核准＝該批不開工**，沒有「先開工、宣告後補」）。量測類門檻先做對抗式檢查（**與宣告作者不同的模型**／high 只讀：作者是 Fable 就用 Opus，反之亦然；navigator「敵対的検査者は検査する相手と別のモデル」；memory v7-ultracode-gearing）。
- 完成時 dev_log 與該批同一變更（AGENTS §15）；HANDOFF 每批開始／停止時更新。
- 停止條件共通：AGENTS §14 冒頭三條；單一問題累計 >2 h 或總時數 >估時上限×1.5（memory time-box）；golden 或 digest 出現事前未列的差＝停。
- **測試、守衛與突變的去處（產品批、工具批與 Engine「包」批共通；第一輪對抗 S-1 的「測試移植」改名，第二輪併入下一條的守衛規則）**：遷移把 `PvpMatch`／`MatchRuntimeHost`／`ClientSimulationRole`／`PvpApplication` 等型別刪掉或重寫，引用它們的既有測試（`tests/object_fps_pvp/` 約 20 檔、約 360 個 TEST_CASE）不可能「不改而過」：在舊路徑並存期間它們跑的是舊程式，不是新路徑的證據（AGENTS §14.2）；舊型別刪掉後連編譯都過不了。所以每個產品批的事前宣告列「測試、守衛與突變的去處」表：一般測試逐檔寫搬改（換成新 host／System 的入口，判定不變）、重寫（性質在新路徑以同一劇本重現）或刪（刪 TEST_CASE 前逐 CHECK 看，-remove-object-fps S1 教訓）；守衛測試與突變照下一條四選一。其中 Design Invariants 的守衛（`EntityModelTests` G1、G2、G7、G13 等，legacy-impact §4 第 18 列）與受理判定用的測試照 AGENTS §14.4 寫 1～5、由驗證者複核、交使用者核准（DI 守衛不論有沒有緩める面都要核准）。針對舊程式的突變（`kp-*`）改寫成針對新 System 的突變，預期失敗照 §14.2 寫可識別。工時另列（§2.2 各批「＋測試、守衛與突變的去處」）。
- **守衛與突變隨程式一起遷移（橫切規則；使用者決定 `v8-arch-guards-migrate-with-code`；判定 §0 A10）**：舊守衛與舊突變判的是舊形狀，不跟著程式遷移就會把架構拉回原來的形狀。
  1. 每批事前宣告列「守衛與突變的去處」：本批碰到的每個守衛測試與每條突變**四選一**——**留**（性質在 target 或仍生效的 legacy DI 裡、形狀不變）／**改寫**（性質保留、形狀換新）／**退役**（性質只在 legacy、已被 target 取代；照 §14.4 寫 1～5，DI 守衛交使用者核准）／**新增**（target DI 尚無守衛）。未列的守衛不動。
  2. 判斷標準是**性質**，不是測試名；只在 legacy 的性質在該部件遷移的**同一批**退役，不拖到 closeout。**「留」的守衛，其 DI 若寫在 legacy 檔：該部件遷移完成、legacy 檔刪除前，性質要先寫進 target（或搬回後的現況檔）**，寫入的批與落點列在 §5.2「DI 落點」欄（第三輪對抗 S3-6）。
  3. 突變隨守衛重列；**新增與改寫**的守衛／受理判定測試（含 Kernel 的 J-n、target DI 的新守衛、Plan 指紋 golden、掃描器自測），其突變一覽照 AGENTS §14.2 做一次敵對檢查（時機：新做時、§14.4 改判定時；第三輪對抗 S3-4），檢查者**與該守衛／突變一覽的作者不同、且不是小模型**（navigator〈敵対的な検査〉）；宣告沒另指定時用該批審查者的模型；報告寫檢查者模型、找到的處數、加入後的檢出數／總數；每批事前宣告列檢查者模型；針對已刪程式碼的突變退役並記錄。
  4. 起點清單＝legacy-impact §4 的 21 個守衛（§5.2 逐列寫預計在哪一批判去處）；遷移完成的部件不再有任何守衛的性質只來自 legacy（§0 A10），closeout 歸零。
  5. 先例：-detect 的 D2 棘輪白名單（`test_kernel_prep_sites.py`）＝**改寫型**（搬檔只改檔名欄、不放寬）。第一批要判去處的：entity_store 禁字表（world-exec）、OS 連結期望與 boundary guard（kernel-scaffold）、framework ratchet 與四個工具正向清單（plugins-foundation 初判）、`EntityModelTests` DI 守衛 G1／G2／G7／G13（match-sim）、determinism 區表測試（碰到它的批；legacy-impact §4 結論是今天沒有以 determinism.md 為判定依據的守衛——宣告先查證它指哪個測試）、`neutral_names_guard`（kernel-scaffold）。
  6. 工時：守衛去處的判斷（四選一、§14.4 的 1～5、突變一覽的敵對檢查）每批另加 **1～2 h（推定）**，加在每個有守衛去處項的批（kernel-scaffold、h7-scanner、registry-schedule、plan-dryrun、world-exec、loading、threading、pageguard-os（條件）、plugins-foundation、assets-core、net-link、diagnostics-service、pvp-diagnostics、match-sim、match-host、rollback-extract、wrap-render、wrap-assets、plugins-frame、client-sim、client-link、client-frame、plugins-editor、工具四批、framework-delete、runtime-loop-retire、closeout；含只有「新增」項的批；非條件 29 批＝29～58 h），已併進 §2.2 各批工時與合計；寫「無」的批不加。
- 範囲の限界共通（所有寫 kernel 程式或 System 的批）：**LIM-2**（v1 只有 Serial，Parallel 宣告是 D-E17）、**LIM-3**（存取驗證攔不到 spike L1～L8 的形狀；驗證關閉時這一類全部不偵測；不經 World 的存取靠審查）、**LIM-6**（驗證開關是行程級啟動參數，「無違規」必附開關值與被排除的大欄清單）。動到 Match 成員的批另列 **LIM-5**（Join／Leave 在 tick 內，晚最多一 tick）；呈現批另列 **LIM-1**（(b) 讀 (c) 要顯式順序，成本在呈現 System 數）。各批只再列自己多出的。
- 突變：每批只跑相關突變，PR 線合併前背景全跑（memory mutation-rerun-scope）；計測を伴う批的報告第一行是件数の突き合わせ（AGENTS §14）。
- Delta 一律引 §4 編號、只寫差分。
- 執行者：控制權在架構師主會話（`v8-arch-kernel-control`），各批由主會話開子代理；產品批的內容 owner 是 `object_fps_pvp`，工具批的 owner 是各工具，執行者是架構師開的子代理（同 kernel-prep 卡）。
- **golden 重設（U9 已定 (a)）**：LIM-5 等原因造成的 golden 重設，逐條照 §14.4 1～5 交使用者核准，事前宣告列可能受影響的 golden 名（結果後不加名），驗證者用與提議者不同的模型。
- §14.4 驗證者：只有改 golden、守衛判定或 DI 守衛測試時出場；模型照該批實作者（navigator），不是提議者本人，不用小模型。

### 3.1 `v8-arch-a6-revision`（A.6 修訂＋legacy 處置＋kernel DI 落 target＋三個決定）

- **Feature**：(i) 場次中開著驗證抓繞路，「無違規」報告帶「驗證開著」（PvP 的 Match／Client；開發者可見）；(ii) 後面每批的事前宣告能引用穩定條號——結束時開發者看到 kernel 的 `Design Invariants` 頁與 `Replacement Criteria` 在 target/，legacy 36 條標好「已被取代（批次代號）」或「相容（措辭更新）」。
- **Contract**：spec v0.4——A.6 第三層寫成條件化條文並限定「Exclusive 欄、(c) System、開關開」（pageguard 1）；Resource 是否保護（pageguard 2；建議 v1 不保護）；(b) System 成本規則（pageguard 3）；handler 串接與 `reinstalls` 進證據（4）；多 World 身分（5）；`tlCurrent` 為空的性質（7；建議 GYO_ASSERT：System 外寫入是 W-6 的 Programmer Error，一個事件一種性質）；記憶體估算分平台（9）；「鏈」與「清單」措辭統一（衝突 1）；G.4 併入 -detect 補列（第 5 條擴充、補第 6 條）；target/system-contract §2b 加 field-session 澄清句（衝突 3）；target/world §6 **只寫通用規則**（stamp 在 kernel 不在 store；不收任何產品欄位）；entity-model 第 139 行的 PvP 欄位分類表搬進**產品自己的 target 文件**（entity-model 的後繼，owner object_fps_pvp；位置本批定，候選 `docs/object_fps_pvp/target/entity-model.zh-Hant.md`，與圖書管理員對齊；第一輪對抗 S-18 修正了處置決定行的「搬 target/world」——產品知識不進 Engine 層的設計權威，AGENTS §6／§7）；target/determinism **只收** legacy §1 三級、§2 區表、§4 區表（含 4.7a／4.7b；`v8-arch-determinism-table-fix` 已裁決）（區表照搬：表內對 E-n 的引用照原文保留，不改寫已裁決的區表——第二輪對抗 S2-2）——legacy §7 缺口清單的條目（E0～E5 作為計畫項）**不進 target**（它們與 DET-0～3 是同一組暫停項，`v8-arch-kernel-pending-paused`；target/determinism 檔頭「records only what the user has decided」），留在 legacy、在 HANDOFF〈暫停〉節記一行，closeout 刪 legacy 前再定去處；target/runtime-composition §3b 加 preview DI 4 已被取代的新條文；target/runtime-composition §4 加「**plugins 受 OS 邊界守衛掃描**」的原則（掃描根含 `plugins/`；守衛的 ROOTS 變更由 plugins-foundation 提，Q1）；target/naming 第 4 條的舊例子 `ObjectFpsPvp::Sim::` 待改為 `fps::pvp::sim`（a6-revision），DI 1（namespace 與目錄一一對應）依 `v8-product-kernel-prep-zones-gate` 改：產品 namespace 是 `fps::pvp`、`RetroFPS` 是 include 目錄名，對應寫成 `RetroFPS/Sim` ↔ `fps::pvp::sim`（第三輪對抗 N3-7；DI 1 的守衛＝新增，派 kernel-scaffold（J-9 掃描器擴充；Q9））；**區表修正**：§5 第 39～42 列的 -zones 回報四項判區（S3-8），並列出受判區影響的 -zones kz-* 突變（4＋1 條）與預期去處，供 match-sim、client-frame 的宣告照四選一判（本批只寫文件）；§5.2「DI 落點」欄中落在 target 或產品 target 文件、由本批寫入的條文；target/error-handling 註明與 L124 的關係；kernel DI 頁（spec K.1 → target，候選 `target/kernel.zh-Hant.md`，位置與圖書管理員對齊）。
- **Architecture**：文件層；不改結構。採第三層時的 OS Delta 只在這裡**寫**（§4 Delta-4），實作在 pageguard-os。
- **範圍**：只動 `docs/`：spec A.6／G.4／K／修訂記錄；target 各部件；產品 target 文件（新）；legacy 檔只加標註不改設計內容；spec／target 對 legacy 文件的行號引用 13 處改條號或章節名；#176 判定總表四處收窄加五列（audit §4.4）；HANDOFF〈暫停〉加 determinism §7／E0～E5 一行。守衛與突變的去處：本批只動 `docs/`、不改守衛程式；宣告提「`neutral_names_guard` 改為只掃程式與測試、不掃 `docs/architecture/voxel-mesh.md`／`map-markers.md`」——緩める面，照 §14.4 寫 1～5 交使用者（Q10）；核准前兩份文件的註記避開 `zone`／`Extract`／`spawn` 子字串；核准後腳本的改動由 kernel-scaffold 做（它是第一個判 `neutral_names_guard` 去處的批）。
- **受理判定輪廓**：(1) 對抗 0 blocker；(2) §5 中歸本批的 **24 列**（第 1、6、7、8、9、12、14、19、20、22、23、27、28、29、30、32、33、34、35、36、39、40、41、42 列）各有落字位置（件数 24＝24）；(3) legacy-impact §2 的 36 條依新定義重標，重標後 被取代 n＋相容（措辭更新）m＝36；(4) legacy 文件的行號引用為 0：正規式**限定 legacy 檔名**（例：`architecture\.md:\d+`、`entity-model[^ ]*第 ?\d+ ?行`、`determinism\.md:\d+`、`network-architecture[^ ]*第 ?\d+`），不掃程式碼行號（spec 內 `MatchRuntimeHost.cpp:441` 這類 119 行是程式引用，不在範圍）；事前宣告寫**基準件数＝13**（legacy-impact 處置行），先以已知正例命中確認掃描有效、不丟 stderr（-remove-object-fps 教訓）；(5) kernel DI 頁有 `Design Invariants` 見出し與 `Replacement Criteria`，每條附守衛 J 號；(6) 只有 `docs/` 有 diff；(7) 目標 DI 清單分兩層——「跨部件（先讀）」與「各部件」——並附 legacy 條文對照，一次交使用者核准（U13 已定 (a)）；U1、U5 已由使用者定（`v8-arch-migration-plan-undecided`），本批不再決定；(8) target/world 無產品欄位名（`bodies_`、`PlayerState`、`combats_` 等掃描 0）；(9) target/determinism 不含 legacy §7 缺口清單的條目（E0～E5 作為計畫項）：以章節比對——target/determinism 沒有對應 legacy §7 的節、沒有逐條搬入 §7 的條目；不用 `E[0-5]\b` 掃描（照搬的 §2／§4 區表與既有的「after E1」本來就含 E-n，掃描必然命中）。
- **範囲の限界**：只寫文字，不驗條文可實作（由 registry-schedule、world-exec 驗）；「被取代」的標記在遷移批完成時才真正生效。
- **段**：上段（Design Invariants、契約措辭、OS Delta 文字）。**模型／檔位**：主會話自寫（模型由使用者定）／high；對抗 與主會話不同的模型／high 只讀（-spec-v03 同形）；驗證者不需要。
- **估時**：6～12 h（出所：§2.2）。
- **被取代的 legacy 條文**：本批是重標的來源批；本身不取代條文。
- **風險與停止**：重標遇到「半條」（E-3 類）照處置決定行歸相容（措辭更新）；一條兩邊都說不通→停，列給使用者不自行拆條。A.6 修訂發現 K.3 已決事項互相矛盾→停（§14「原因改變前提」）。U1 拖延→world-exec 以「欄用一般配置、保留換獨立頁的介面」開工（§7 U1 預設）。
- **若失敗**：後面每批的事前宣告沒有穩定條號可引、宣告不能核准；照 §3 共通「宣告未核准＝不開工」，程式批（registry-schedule 起）**也不開工**。

### 3.2 `v8-product-pvp-baseline`（今天的 Match 量多少）

- **Feature**：無新 Feature；它是 §6 預算的分母（AGENTS §2「性能や資源に関わる Feature の計画は、判定に使う予算を書く」）。Product：object_fps_pvp Match。
- **Contract**：量測口徑——「一個 tick 的工作」＝今天 `MatchRuntimeHost::Step` 全段（取樣時鐘後到開始等待前；`MatchRuntimeHost.cpp:371` 起），遷移後同口徑＝交換點 A→B（含 Ingress／Judge／Egress）；「到 ready」＝行程啟動到送 `runtime.Ready` 的時點；輸出格式照 field-session C3（tick p50／p95／max、overrun 數、人數）、C6（到 ready 時間、峰值 RSS），CSV 帶 `format=gyo.kernel-baseline, version=1`。
- **Architecture**：不變。量測執行檔放 `tests/object_fps_pvp/`、不進 ctest（比照 `gyo_time_wake_record`「Deliberately not CTests」）。
- **範圍**：**建 Dockerfile**（照 CI toolchain 版本；U8 已定）並在容器內量基線，Dockerfile 的位置與映像版本寫進基準報告；`tests/object_fps_pvp/` 新增一支量測執行檔（重放 host：`AuthorityDigest` 的腳本時鐘＋37 個情境，另加一個 8 人長情境 ≥3,600 tick）＝BU-1、BU-2 對照、BU-4 World 位元組的分母；**另量今天的重放 host 以同一內容（同一 arena、同一動畫內容）、同一重放形式載入到可開始模擬的時間（10 次中位數）＝G2a 的 BU-6 分母**——兩邊都**從讀檔開始計**：舊路徑的讀檔在 loader 內（`Arena::Load(path)` 等），新路徑把測試餵入 `AssetInbox` 前的讀檔計入（第三輪對抗 N3-11）（第二輪對抗 S2-10：重放 host 沒有行程啟動與 IO 執行緒，拿它比真實行程的「到 ready」口徑不同、判定沒有檢出力）；**另量今天真實 Match 行程（舊 binary，Linux）的基準**＝BU-1′（統計行 tick p50／p95／max、overrun）、BU-4 穩態 RSS、BU-6「啟動到可接受 join」的分母——10 分鐘 bot 對戰或 acceptance 腳本，10 次冷啟動；兩種基準都在**看新路徑結果前**量（§6 量測紀律），不留到 match-host 才量。不碰 `apps/`、`engine/`、`build/`、`.github/`。守衛與突變的去處：無（量測執行檔不是守衛、不進 ctest）。
- **受理判定輪廓**：每情境記錄的 tick 數＝跑的 tick 數；同機 5 次 p95 散佈 ≤10%（超過＝量測不穩，不出基準，停；§6 與 first-host-cost 用同一個 10%）；RSS 以 `/usr/bin/time -l`（mac）／`-v`（Linux）量穩態；工具鏈與組態（RelWithDebInfo）記錄；真實行程基準的情境腳本與人數寫進 CSV 標頭，match-host 用同一腳本。
- **範囲の限界**：判定用 Docker Linux 容器（U8 已定三層；絕對數字標「容器內」）、Intel Mac 原生只作開發參考、Linux 實機在出問題且允許時確認；CI 共用 runner 的計時是雜訊不用；Windows 的 Match 成本不量（主戰場是 Linux，memory platform-priority）；量測要改產品程式才量得到→停（不是本批範圍）。
- **段**：下段（不碰 DI／契約／介面）。但它是上段計畫的輸入，**不用小模型**（navigator「上段の計画に入力する偵察」）。**模型／檔位**：實作 Opus／medium；審查 Fable／high（重點：量到的是不是 tick 本體、不含等待；gearbox「code review 是 high」）；驗證者不需要。
- **估時**：3～8 h（推定）。
- **被取代的 legacy 條文**：無。
- **風險與停止**：重度本機量測先講影響並取得同意（memory warn-before-heavy-local-runs；本批 headless、記憶體小，預期不需）。**若失敗**（量不穩）→§6 全部無分母，G2a 判不了；停下改口徑，不先開 match-host。

### 3.3 `v8-arch-kernel-scaffold`（kernel 進 L1 的腳手架）

- **Feature**：kernel 的每一條守衛從第一個 commit 就在 L1 三平台跑（開發者可見）；支撐的 Product：object_fps_pvp、四工具（之後都經 Kernel）。
- **Contract**：`gyo_expect_links(gyo_kernel GYO::Base)`（有 `.cpp` 時 STATIC；ci-readiness §1.1）；`mutations.json` 沿用 time-threads-trace 的 `run_mutations.py`（ci-readiness §2.4）；J-9 的禁字表自寫（不沿用 `neutral_names_guard.py` 的 WORDS，legacy-impact §4 第 3 列），自測含 `far`／`near`（memory windows-far-near-macro）；**編譯失敗測試的形狀**（ci-readiness §4 第 2 項）：(a) `try_compile` 預期失敗與 (b) trait＋doctest 各做一個最小樣本，在 windows-x64 L1 跑過，依 U5 已定（核心 3 條用 (a) `try_compile`、其餘 (b) trait＋doctest）寫進 registry-schedule 的宣告。
- **Architecture**：§4 Delta-1 的第一步（目錄與 build 2 行）。
- **範圍**：`engine/kernel/CMakeLists.txt`、`engine/kernel/include/engine/kernel/`（空殼標頭）、`tests/common/kernel/`（CMake＋smoke＋J-9 腳本與自測）、`engine/CMakeLists.txt`、`tests/common/CMakeLists.txt` 各 1 行、`docs/architecture/plans/kernel/scripts/mutations.json` 骨架。`build/`、workflow 不動。
- **受理判定輪廓**：清單外零檔（ci-readiness 判定 1 同法）；L1 三平台綠（含 Linux `core` preset，ci-readiness §1.3）；兩種編譯失敗樣本先過 windows-x64 L1，過了之後才照 U5 選定分工（核心三條 (a)、其餘 (b)；(a) 在 Windows 不穩則報告使用者），並記錄何者較穩；J-9 自測正例（加一個產品名）失敗、反例（`Despawn`／`ExtractRes`／`zone`）不誤判；`gyo_expect_links` 反例（多連一個 target）讓 configure 失敗；**naming DI 3 的後綴掃描**（禁止後綴）掛在 `engine/kernel`（Kernel 新程式是第一個受它約束的地方，不等 wrap-render），自測正例失敗、反例不誤判；**守衛與突變的去處**（§5.2 第 3～7 列）：`neutral_names_guard` 留（不套到 kernel；J-9 自寫禁字表＝新增）、`os_boundary_guard`／OS 編譯定義檢查／`ProcessCodePageCheck`／`gyo_expect_os_links` 留（kernel 無 OS 碼；新測試執行檔在掃描根下）；新增（J-9 禁字表——含 error-handling DI 2「Executor 以外無 log 呼叫」的禁字（Q9）——、naming DI 1 的 namespace↔目錄掃描（Q9；`RetroFPS/Sim` ↔ `fps::pvp::sim`）、後綴掃描、編譯失敗樣本）與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點）。
- **範囲の限界**：STATIC＋PRIVATE 連 `GYO::Os` 的形狀不在本批（pageguard-os 才需要）；MSVC `/W4` 零警告在本機 macOS 無法先驗。
- **段**：上段（Build Graph 變更是 AGENTS §3 Delta 項目）。**模型／檔位**：實作 Opus／high（編譯失敗測試的形狀在 Windows 無先例，ci-readiness §1.2；gearbox 原則 4 只允許對「實際會跑的驗證」降檔，而兩種樣本本身就是本批要造的驗證——其中 CMake 兩行、目錄與空殼標頭是 low 定義的逐字作業）；審查 Fable／high；驗證者 Opus／medium。
- **估時**：4～8 h（出所：§2.2；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：無。
- **風險與停止**：清單外還要改檔才綠→停（ci-readiness 判定 1 的反例；記 Delta）。**若失敗**：registry-schedule 沒有 L1 落點；停。

### 3.4 `v8-arch-h7-scanner`（H-7 呼叫點掃描器）

- **Feature**：「每遷一個 host 少一套規矩」可檢查（HANDOFF〈觀測到的壓力〉）；使用者：遷移 Match／Client／工具 host 的開發者；Product：object_fps_pvp、四工具。
- **Contract**：spec H-7 守衛兩條：(a) 不 include `IRuntimeClient.hpp`／`RuntimeLoop.hpp`；(b) 呼叫點規則——host 原始碼不得呼叫產品或能力模組型別的成員 `Tick`／`Advance`／`Update`／`Step`／`Run`；白名單依型別（Kernel 的 `Runtime::Run`／`Rewind`／`Replay`、`FixedTickRuntime::Advance`）；報告格式照 J-17「檔:行 規則 文字」（target/zone-c-sources §5 同形）。
- **Architecture**：無新結構，但形狀照 J-17 修正後的分工（j17-scanner 卡「給 -zones 批」節；AGENTS §7、spec J.1／J.4——H-7 掃描是**產品守衛，由 owner 持有**，共通 Test 不得以特定 Product 的存在為前提）：
  - **共通層**（`plans/kernel/scripts/h7_scan.py`、規則資料、`tests/common/h7_scan/`）只放掃描器、規則資料、**合成檔**的自測與突變；共通 ctest 在沒有任何 owner 清單時報「nothing to scan」並通過。
  - **每個 owner**（object_fps_pvp、voxel_map_editor、model_viewer、audio_editor、ui_editor）在自己的 `tests/<owner>/` 由該 owner 的 CMake 註冊 H-7 掃描與基線清單（呼叫共通腳本、傳自己的檔案清單與白名單）。
  - 這與第一稿「ctest 放 `tests/common/`、正例是產品 host」不同：那是 J-17 已犯過、-zones 卡正在修的同一個耦合（第一輪對抗 B-2）。
- **範圍**：共通腳本、規則資料、合成自測與突變；五個 owner 的 `tests/<owner>/` 各一個註冊（CMake）與基線清單（內容 owner＝該產品或工具，執行者是本線；產品線之後可自行調整，不經共通層）；不改 `apps/`、`tools/` 的程式。
- **受理判定輪廓**：(1) **合成**正例（合成 host 檔含 `foo_.Tick()`、`loop_.Run()`）必報、合成反例（`FixedTickRuntime::Advance`、Kernel 的 `Runtime::Run`、白名單型別）不報；(2) 突變（刪規則、改正規式、改白名單判別）全殺，預期失敗可識別；(3) 掃描用 Python 或 `git grep -P`、不丟 stderr、終止碼區分「零命中」與「失敗」；(4) 共通 ctest 在 owner 清單為空時報「nothing to scan」並通過；(5) **Removability**：刪一個 owner（例：合成一個假 owner 後刪它，或以 `git stash` 暫移 object_fps_pvp 的登記與目錄）後，共通層零修改、共通自測仍綠、其他 owner 的掃描照常；(6) 各 owner 的基線報告（今天各 host 的命中數；object_fps_pvp 的正例 `MatchRuntimeHost.cpp:441` `match_.Tick`、`ClientSimulationRole.cpp:196` `loop_.Run` 在**產品自己的**基線測試中必報）＝match-host／client-frame／工具批的歸零目標；(7) **守衛與突變的去處**：碰到的既有守衛無；新增＝掃描器、規則資料與合成自測，新增與改寫守衛的突變一覽敵對檢查者 Opus（實作 Fable）（與作者不同、不是小模型；§3 共通規則第 3 點）。
- **範囲の限界**：依型別判若需要 C++ 解析而 Python 做不到→改成依「宣告型別的檔」近似並寫限界。
- **段**：下段（守衛工具，不改契約；J-17 線同判）。**模型／檔位**：實作 Fable／high（規則正規式 xhigh）；審查 Opus／high；驗證者照實作者。
- **估時**：5～10 h（出所：§2.2；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：無。
- **風險與停止**：見限界。**若失敗**：match-host 的 H-7 判定只剩審查；不擋其他批開工，但 match-host 的宣告缺 H-7 基線就不能核准（§3 共通）。

### 3.5 `v8-arch-kernel-registry-schedule`（Registry、簽名推描述、Scheduler、Plan）

- **Feature**：跨產品能力「Match 與 Client 預測共用同一份模擬核心」（object_fps_pvp 的 Match 與 Client；玩家可見：預測不因兩端順序不同而回彈）；開發者可見：產品加 System 時，順序歧義、第二寫者、區違反在 finish 一次列出（mechanism §5）。結束時＝M1 的前半：Plan 可印出、三平台指紋相同。
- **Contract**：Registry 生命週期（spec C.2 build／finish／凍結、R-E1～R-E6、runner 在 finish 安裝）；SystemDescriptor 從簽名推出（spec S-1～S-4，J-12），含 `mounts` 多掛點與 D-E19；Plan 唯一（D-1）、(c) 歧義＝D-E8、(b) 依 registrationSeq 平手（D-3、D-4、D-10）；錯誤代號表 D-E1～D-E19、R-E1～R-E6、X-E1～X-E3、F-E1（D.2）一次回報（D-5）；Plan 指紋（C.2 第 5 點）與 Registry 指紋（J-18）帶版本欄；區規則 Z1～Z8 的 finish 檢查；**G-5 匯出的 (c) 來源檔清單照 `gyo.zone-c-sources` v1**（`generated_by = "gyo-kernel-report"`，格式不變、只換生產端）；**全部參數型別的描述與 Contribution 在本批定義**（J-12 的清單加 `CurrentTick`、`DiagnosticsWriter<T>`、`Extract<Read<T>>`／`ExtractRes<R>`、`Local`、`ExclusiveWorld`），執行期行為由後續批實作（`DiagnosticsWriter` 緩衝在 world-exec、`CurrentTick` 取值在 loading、Extract 在 rollback-extract）——否則 plan-dryrun 登記不了 Ingress 與持 `DiagnosticsWriter<T>` 的 (c) System（第一輪對抗 S-4）；**Plan 匯出帶資料流邊比例**：對每條 before／after 邊輸出是否為資料流邊（`writes(u) ∩ (reads(v) ∪ writes(v)) ≠ ∅`，由描述算）與非資料流邊的比例（G1 謂詞 (ii) 的工具）；API 名稱本批定並寫進 target/scheduler、system-contract——**spec 的 `Runtime` 型別名以 `Runtime` 結尾，與 target/naming 第 5 條衝突，本批定名時處理**（改名或在 naming 列明例外，寫進宣告交使用者）。
- **Architecture**：§4 Delta-1 的主體（新 Engine 子系統 `engine/kernel`）。只做到「描述→Plan」，不跑 System（Executor 在 world-exec）。Contract 撐不住的只有一處：編譯失敗測試需要 build 層的形狀（scaffold 已定）。
- **範圍**：`engine/kernel/`、`tests/common/kernel/`；spike 的 `AccessDerivationTests.cpp` 搬改為 J-12、`RegistryWorldTests.cpp` 第 14／40 行搬改為 J-18／J-19、第 50 行重寫（ci-readiness §2.2）。
- **受理判定輪廓**：J-10～J-25、J-27（D-E17 半）、J-28（finish 半）、J-37～J-41、J-44、J-47 在 L1 三平台綠；J-12 覆蓋上列全部參數型別（漏一個＝不成立）；**Plan 指紋 golden**：三個合成 Registry（含 (c)／(b) 混合、延後寫、事件寫）在 linux／windows／macos 指紋逐位元相同；J-44 的 20 種排列全過；J-24 每種錯誤代號一個最小 Registry；資料流邊比例的匯出有合成正例（已知 2 條非資料流邊／5 條→40%）；J-17 kernel 端自測：合成 (c) System 檔由 G-5 匯出，`zone_scan.py` 讀得進且違規數與預期相同；naming 第 5 條後綴掃描（scaffold 掛的）對本批新程式零命中；突變優先清單第 2、3、4、5、6、11、14 條 killed；**BU-3**（finish；數字與量法在 §6）；**守衛與突變的去處**：碰到的既有守衛無（spike 測試搬改為 J-12／J-18／J-19＝改寫）；新增＝J-10～J-47 的本批部分、Plan 指紋 golden、資料流邊比例的合成正例，新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點）。
- **範囲の限界**：共通三條；只驗描述，不驗函式體實際存取（LIM-3）；指紋相同不代表 (c) 結果相同（Math／編譯是 G.2 第 4 處，Kernel 檢查不了）；J-27 只有 D-E17 那半（LIM-2）。
- **段**：上段（公開介面、Design Invariants、決定性）。**模型／檔位**：實作 Opus／xhigh（決定性與平手規則是「気づかないうちに誤りやすい」）；審查 Fable／xhigh；驗證者（若有 §14.4 變更）Opus／high 另一代理。
- **估時**：21～42 h（推定；§2.2；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：entity-store RC 5（交集手寫 >3 → 查詢層：world §1 已定）；architecture.md L136 已同步（PR #190 後條號改引「Runtime boundaries 段的禁令」）。
- **風險與停止**：**若這批失敗，後面全部白做**（World、Executor、四個 host 都假設同一 Registry 得同一 Plan、三平台相同）。指紋在某平台不同、修兩次仍不同→停，觸發 spec K.2 第 1 條 (a)／第 4 條與 §8 RC-2。K.2 第 3 條（為消 D-E8 加的順序一半以上與資料流無關）在合成測試就出現→停。G-5 的 `source_location` 只知登記處 `.cpp`，產品 System 跨編譯單位時照 zone-c-sources 的 RC 改版本，不加欄硬撐。

### 3.6 `v8-product-pvp-plan-dryrun`（PvP 能不能表達成這套規則；閘門 G1）

- **Feature**：同 3.5 的跨產品能力；另一個名指 Feature：Body 拆欄後「獨占寫者」成立（使用者 K.3 第 9 項：不拆的話架構改造就沒有意義）。Product：object_fps_pvp 的 Match 與 Client。
- **Contract**：PvP 的欄與 System 宣告（zone、writePolicy、role、rollback；Body／Life／InputCursor／Connection 診斷四欄＋Player／Peer 分開）；Match 的 Loading＋FixedUpdate 與 ClientSim 的 FixedUpdate 兩套 ScheduleDef；**Life 的寫者形狀**（U6 已定：`LifeState`＋`DeathEvent`，不要中間狀態欄；本批以 finish 結果驗證謂詞）。只寫描述，函式體是空的（回 Ok）。
- **Architecture**：不變（產品測試目錄裡的一份登記）；Contract 撐不住時才回報（例：某 digest 欄必須 Shared 才能登記＝撐不住，LIM-4）。
- **範圍**：`tests/object_fps_pvp/` 新增一支測試；System 名照 audit §1.3 P1／P2／P9、spec H.4.1／H.4.2 與 target/naming。
- **受理判定輪廓**（＝G1 的四個謂詞，§2.1；全部由 Plan 匯出機械檢查，審查者只核對匯出與描述一致，不做主觀判斷——第一輪對抗 S-3）：(i) finish 成立（零啟動錯誤）且 D-E2＝0（Exclusive 欄寫者都 ≤1，**不靠改 Shared**）；(ii) 為消除 D-E8 加的 before／after 邊中，非資料流邊 ≤50%（K.2 第 3 條的量化；「資料流邊」照 §2.1 的定義由 registry-schedule 的匯出算，比例由工具印出）；(iii) **Life 形狀的 Plan 謂詞**（U6 已定）：`LifeState` 單一寫者在 tick 開頭（`StepMovement` 之前）、`DeathEvent` 由 `ResolveActions` 發、snapshot／digest 組裝在其後讀（同一 tick）；D-E19＝0（同一 schedule 掛兩次）；(iv) Match 與 ClientSim 的 cFingerprint 相同（H-4）。**T=35／R=215 的數值不在本批判**（函式體為空量不到），留 match-sim 在新重放 host 上實測。另加一條查證（N-11）：Client 能否從 Welcome 已有的 `arena_id`／`arena_version`／`arena_digest`／`animation_digest`（`client_v9.proto:19-36`）推出 Loading 的請求清單——結論寫進宣告與 U14，讓 client-sim 不必到 400 h 後才知道要不要停。**守衛與突變的去處**：碰到的既有守衛無；新增＝G1 四個謂詞的描述登記測試，新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點）。
- **範囲の限界**：共通三條；描述不是程式——函式體經全域或捕獲讀到的東西不在描述裡（LIM-3 的 L5）；dryrun 成立只表示「規則表達得出」，不表示遷移後行為相同（那是 match-sim 的 golden）；Life 的 T／R 數值留 match-sim。
- **段**：上段（區判定、digest 欄的 writePolicy＝資料形式）。**模型／檔位**：實作 Opus／high；審查 Fable／high（重點：匯出與描述一致、Life 謂詞對應 U6 候選）。
- **估時**：7～14 h（推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：無（只登記，不改產品）。
- **風險與停止**：**若這批失敗，match-sim（47～103 h）、match-host（66～142 h）、client-sim 白做**。Life 的謂詞（U6 已定的形狀）不成立→停，交使用者（U6 重開）；非資料流邊 >50% 或某 digest 欄必須 Shared→停，重劃區或重拆欄（K.2 第 3、10 條，§8 RC-4），不硬過。

### 3.7 `v8-arch-kernel-world-exec`（World 與 Serial Executor；驗證兩層）

- **Feature**：同 3.5；另「進 digest 的狀態只有一個寫者」成為啟動檢查（AGENTS §3 State Authority 的機制版）；下一版的 loading 分批載入需要延後佇列與同步點。結束時開發者看到 `gyo_kernel_tests` 跑 World 的守衛，驗證開／關兩個 World 在同一執行檔。
- **Contract**：World（A.1～A.5：鍵 u64、欄依 id 升冪、同步點後參考失效、SoA keys／值／stamps）；延後佇列 F.2 套用順序（hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource；每階段依 id，同鍵依 (planPos, recSeq)）；事件 F-7～F-9；hook F.4；Serial Executor E.2（含第 ⓪ 步呼叫前比對與第 ⑤ 步呼叫後比對）、`Result<void, KernelError>`、Interrupted（E.5）；驗證兩層＋句柄 epoch、`KernelSettings` 兩參數（驗證開關、`breakOnKernelError`）四條件（LIM-6）；第一層與第二層都以 Err 回傳（pageguard 8）；違規帶 World 身分（pageguard 5 的第二層部分）；欄配置走一個小介面（一般配置／獨立頁，pageguard 6）；`DiagnosticsWriter<T>` 參數與 World 端緩衝、Interrupted 不丟（spec S-8 例外、spec B-13）。
- **Architecture**：§4 Delta-1 續；§4 Delta-3（World 的欄儲存重用 `engine/entity_store` 的演算法，二選一：回傳位置的查找版本或 `Cell<T>`；事前宣告以 W-1～W-11 核對 EnTT 後定，預期自寫——swap-and-pop 違 W-2）。
- **範圍**：`engine/kernel/`（續；含**欄配置介面：一般配置／可換獨立頁**——U1 已定延到 G2a 後決定第三層，本批只留介面、走一般配置）、`engine/entity_store`（若選回傳位置版本，加小介面；Delta-3 差分寫在宣告）、`tests/common/kernel/`；spike 的 `AccessCheckTests.cpp` 搬改（斷言→Err、兩執行檔→一執行檔兩 World；ci-readiness §2.2 表）、`RegistryWorldTests.cpp` 第 74 行搬改 J-7、第 94／114 行**重寫**（spike 語意與 F-2／F-7 相反）。
- **受理判定輪廓**：J-1～J-8（J-9 在 scaffold；J-6 的 ownerThread 半在 threading）、J-26、J-29～J-37、J-32b、J-45、J-50、J-52、J-53 在 L1 三平台綠（L1 主圖 RelWithDebInfo）；J-5 原文三種繞路開著攔、關著回 Ok 並帶「驗證開著」欄位，旁邊印 `IsDebuggerPresent()`（ci-readiness §3.2 的推斷變證據）；J-32 三平台 golden；突變優先清單第 1、7、8、9、12、13、16 條 killed；`gyo_expect_links(gyo_kernel GYO::Base GYO::EntityStore)` 在 configure 生效；BU-2（Kernel 自身開銷，合成 Registry 記錄執行檔）；**守衛與突變的去處**（§5.2 第 1、2 列）：entity_store 禁字表 W1～W5 與 `gyo_expect_links(gyo_entity_store GYO::Base)` 的性質對 store 仍成立＝留（World 不放進 entity_store 目錄）；Delta-3 加的小介面照舊受 W 字表掃描；新增（J-1～J-8 等本批守衛、J-32 golden）與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點）。
- **範囲の限界**：共通三條；第一層在 spike 是斷言、本批直接照規格 Err；L7（其他執行緒的存取當 host）到 threading 才評估；回滾、Extract 不實作（契約在 H.5／H.6）；頁保護不在本批（L1、L6 仍不解）。
- **段**：上段（Design Invariants、資料佈局、錯誤契約）。**模型／檔位**：實作 Opus／xhigh（F.2 套用順序、stamp 取值、驗證比對是決定性與時序類）；審查 Fable／xhigh；驗證者 Opus／high 另一代理。
- **估時**：25～50 h（推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**（重標後以 a6-revision 清單為準）：entity-store DI 1（對 store 仍成立的半條歸相容）、DI 4、RC 1、RC 2、RC 6；error-handling L124（衝突 1：宣告引 target 並註明關係）；field-session DI 3（衝突 3，相容，引 §2b 澄清句）；error-handling.md:81「`GYO_DEBUG_ASSERT` 將來加」（誤導句，legacy-impact §5 第 5 條）。
- **風險與停止**：MSVC 的 `std::launder`／`static inline char` token 在 RelWithDebInfo 無 L1 證據→第一次 L1 紅先切分實作／測試／環境（§14.1）；J-2／J-32 需要改 World 資料模型才能過→spec K.2 第 1 條→停，交使用者「修還是重建」；W-1～W-11 核對發現 EntityStore 撐不住 SoA 而要 archetype→停（spec M、K.2 第 6 條）。**若失敗**：loading、rollback、所有 host 白做。

### 3.8 `v8-arch-kernel-loading`（Loading schedule）

- **Feature**：loading 頁面與分批載入（使用者名指，spec H.2b；object_fps_pvp 的 Match 與 Client；玩家可見：Client 在 ready 前停在 loading）；初始成員走同一條路（機制上成立；在 v1 的真實 Match 上是否被跑到取決於 U14，見限界）。
- **Contract**：Loading 是 clock schedule、`tick[Loading]` 是批次計數不是遊戲 tick（spec S-4：(c) 可讀但等價測試不比它）；stage 固定序 Ingress → 資產 → 地圖 → 場景 → 可互動物件 → 敵人 → 玩家；`StageMembershipRequests`／`StageAssets` 的 Ingress 掛點；**型別無關**的暫存與游標機制（以請求序號為鍵的暫存、連續前綴、游標唯一寫者）——把資產套進 `Assets<T>` 的 `StageAssets`／`ApplyStagedAssets` System 與 `AssetStaging`／`AssetCursor` 的資產型別在 `engine/asset`（assets-core；Kernel 不知道 asset，第二輪對抗 S2-6）；`LoadingStageDone<Stage>` 每 stage 一寫者 `Finish<Stage>`；`LoadingState = Ready` 由 `Finish<玩家>` 寫；對局 tick 在 ready 時為 0、第一個模擬 tick 是 1（H.2b 第 5 點）；domain tick 是唯一 tick 來源（H-1、`CurrentTick` 參數、D-E12）；`MembershipRequests` 以序號為鍵＋`MembershipCursor`（H.2b 第 4 點）。
- **Architecture**：不變（spec 已定形狀落地，§4 Delta-1 內）。
- **範圍**：`engine/kernel/`（domain tick、`CurrentTick`、clock schedule 計數、型別無關的暫存與游標）、`tests/common/kernel/`（合成型別的暫存與 J-51）。守衛與突變的去處：碰到的既有守衛無（新增 J-48、J-51 與 DI 3 的兩條守衛；新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點））。
- **受理判定輪廓**：J-48、J-51（合成型別，Kernel 端；spec J-51 的 `Assets<T>` 以合成型別代表）三平台（**J-49 不在本批**：它要 `rollbackEnabled` 的 Rewind／Replay，是 H.5，只留在 rollback-extract；第一輪對抗 S-4）；runtime-composition DI 3 的等價測試（分批數不同、資產完成順序打亂，ready 時 (c) 欄與 Resource 逐位元相同，排除 role=Input 暫存與 `tick[Loading]`）；runtime-composition DI 3 的兩條守衛（N-7）：Startup 不寫世界內容欄（合成 Startup System 寫 (c) 欄＝啟動錯誤）、Loading 非 Ingress stage 的 System 不讀 external（描述層 finish 失敗）；**BU-6** 的每批上限在合成資料上量一次（本機；數字在 §6）。
- **範囲の限界**：共通三條；套用到真資產（`Assets<T>`）在 **assets-core**、正式的 IO 服務在 plugins-foundation，本批只用合成型別；Client 的 Loading（H.2b 第 8 點：manifest 同源、成員經 Restore）在 client-sim 才接；**U14 已定 (a)**（Match 只在 Ready 後接受 join；記為未結事項——可能的設計缺陷，HANDOFF「未結事項」節），v1 的 Match「玩家」stage 不生成成員，初始成員全走 FixedUpdate 的 `ApplyMembership` 掛點——H.2b 第 4 點的 Loading 路徑只在本批的合成測試成立，真實 host 上不被跑到（第一輪對抗 S-8）。
- **段**：上段（契約、Design Invariants）。**模型／檔位**：實作 Opus／xhigh（游標與連續前綴）；審查 Fable／high。
- **估時**：11～22 h（推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：entity-model E-12（W-3、F.2 取代；§5.2 已加修訂註）。
- **風險與停止**：**若失敗**→match-sim 的初始成員路徑沒有落點（Startup 不放內容是已定），match-host 白做；停。

### 3.9 `v8-arch-kernel-threading`（執行緒模型）

- **Feature**：Match 每 tick 準時跑、Client 預測執行緒與主執行緒互不阻塞（object_fps_pvp 的 Match 與 Client；玩家可見：tick 不漏、畫面不卡）；開發者可見：親和錯誤在 finish 報出（H-3：headless 沒綁 `Main`）。
- **Contract**：一個 World 一條執行緒（H-2、D-E13、`ownerThread`）；`runner = Main | Role(name)`、System 與 Resource 的親和（X-3、D-E10、X-E2、X-E3）；Executor 不擁有執行緒（E.3：RoleThread 由 Host 建、Host 決定停止順序；threads DI 4 相容）；交換點原語（H.1）：`Mailbox<T>` 與 Link 家族的 swap、Running 期間零外部存取（I.2 的可檢查句）；runner 介面（R-E6「一個 Runtime 一個 runner」）在 Kernel，實作在 plugins。
- **Architecture**：不新增子系統；Contract 撐不住的地方：今天 `ClientSimulationRole` 與 `MatchRuntimeHost` 各自的 deadline 邏輯（audit P5「何時跑下一步判 Host」）——本批只提供 runner 骨架與等待工具的接法，不搬產品的相位策略。
- **範圍**：`engine/kernel/`（runner 介面、親和檢查、交換點原語）；**交換點原語＋合成的 IO 執行緒案例**（測試內建立執行緒，在交換點把完成項餵進合成 external Resource——`AssetInbox`、Link 同形，H.1；X-3 親和）；正式的 `AssetInbox` IO 服務（執行緒＋交換點）**不在本批**：Kernel 不擁有執行緒（DI-KS 15、E.3；J-9「Parallel 模組外無 `<thread>`」），由 plugins-foundation 的 headless-runner／asset-io 服務擁有、依賴本批的契約（`v8-arch-migration-plan-round2`）；`engine/threads`、`engine/time` 不改；`tests/common/kernel/` 加執行緒案例、合成 IO 執行緒案例與 TSan 案例（本機 `build/target/_build/tsan`）。
- **受理判定輪廓**：J-6 的 ownerThread 半（W-6：其他執行緒存取 World 是 GYO_ASSERT；J-15 是獨占第二寫者、屬 registry-schedule，第一稿誤標）、J-17b、J-28（執行期半：執行緒不符 GYO_ASSERT）、D-E13 三平台；交換點資料競爭測試在本機 TSan 零報告；守衛與突變的去處（§5.2 第 14 列）：threads／time 的守衛與連結期望留（只測各自模組；kernel 不連 Threads）；新增（J-6 的 ownerThread 半、J-17b、J-28 執行期半、TSan 案例）與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點）。（Windows 的 RoleThread 啟停壓力放在 plugins-foundation 的 headless-runner 上，見 3.11；Client 側的最終收口在 client-sim。）
- **範囲の限界**：共通三條；L7 仍在（`tlCurrent` 是 thread_local，其他執行緒的存取當 host；Parallel 不實作）；windows 觀察 (c) 的舊原因未特定，本線驗的是「新 runner 上不再出現」，不是「找到並修了舊的」（AGENTS §14.3）。
- **段**：上段（執行緒模型、公開介面）。**模型／檔位**：實作 Opus／xhigh（時序）；審查 Fable／xhigh。
- **估時**：13～26 h（推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：architecture.md L140-155（`FixedTickRuntime` 擁有累加器與 tick ID → G-6：累加器是 runner 的時鐘工具、`tickId_` 不是 domain tick 來源；條號在 a6-revision 定）；network-architecture 第 180-181 行（誤導句 §5 第 17 條，文件線加註）。
- **風險與停止**：**若失敗，plugins-foundation 的 headless-runner 與 asset-io 服務、match-host、client-sim、client-frame 白做**（都跑在 runner＋交換點上）。TSan 報告修兩次不收斂→停。

### 3.10 `v8-arch-pageguard-os`（條件批：頁保護平台層的 OS Delta）

- **開批條件**：**G2a 後**使用者決定採第三層（U1 已定 (c)：延到 G2a 後，那時才有 PvP 的 Exclusive 欄數與 (b) System 成本）。不成立就不開，LIM-3 的 L1／L6 維持，本批自 §2 刪除、§4 Delta-4 撤銷。
- **Feature**：存取驗證第三層——場次與測試中攔「(c) System 寫了又寫回 Exclusive 欄」（LIM-3 的 L1；object_fps_pvp 的測試版與場次，Match 與 Client 都受益）。這是 os.md RC 9 末句「and no Feature」的出口（legacy-impact 衝突 4 處置）。結束時 LIM-3 的 L1 標 `Lifted in v8-arch-pageguard-os`（L6 不能解）。
- **Contract**：A.6 第三層範圍（只寫、只 Exclusive 欄、只 (c) System、隨同一開關；pageguard 1）；Resource 保護依 a6-revision 的決定（2）；`engine/os` 新介面：`PageSize`／`AllocatePages`／`FreePages`／`SetWritable`／`InstallFaultHandler`（回呼 `bool(*)(uintptr_t, bool isWrite)`）／handler 統計（ci-readiness §2.3）；與其他 SIGSEGV／SIGBUS／VEH 使用者共存（交前任、保持安裝、`EnterRun` 重裝並計數 `reinstalls` 進「驗證開著」證據；pageguard 4）；區間表不可變陣列原子發佈（5）；欄成長後新頁依狀態設唯讀（6）；(c) 呼叫後比對跳過受保護欄的最佳化只在量了之後做（11）。
- **Architecture**：§4 Delta-4。kernel 連 `GYO::Os` PRIVATE → kernel 由 INTERFACE 改 STATIC；區間表、違規記錄、策略留 kernel（不讓 os 反向依賴 kernel，AGENTS §6）。
- **範圍**：`engine/os`（新檔）、`engine/kernel`（策略）、`tests/common/kernel/` 新執行檔 `gyo_kernel_pageguard_tests`（只它定義 `DOCTEST_CONFIG_NO_POSIX_SIGNALS`；sanitizer 下 `return 77`＋`SKIP_RETURN_CODE 77`，ci-readiness §2.5）；`tests/common/os` allowlist 不動；os.md DI 加一條（使用者同意）；spike `PageGuardTests.cpp` 19 案例照 ci-readiness §2.2 搬改；Cost 案例改成不進 ctest 的記錄執行檔。
- **受理判定輪廓**：判定 1 照 `v8-arch-kernel-pageguard-reading`「重新宣告、重跑同一組測試」——L1、(b1)、(b2)、宣告形狀的 L6 攔到，(b3) 依 Resource 決定（保護則攔到，不保護則是 Limit 測試；宣告不得再自相矛盾）；**MSVC RelWithDebInfo 的 L1 證據**（spike 只有 Debug；`volatile` 寫法在 `/O2` 無既有證據）；VEH handle 保存、可卸除；os 邊界守衛 R1～R4 對 `engine/kernel` 零命中；`gyo_expect_links` 檢查 kernel 不外露 `GYO::Os`（trace 先例）；**BU-8**（頁保護；§6）。
- **範囲の限界**：L6（Ready 期間 host 經存下指標寫）不能解；L2、L5 不解；sanitizer 建置下 skipped（看得見的缺口）；debugger 附加時每次受保護寫入停在 debugger（寫進開發文件）；Windows 每欄 64 KiB 位址空間。
- **段**：上段（OS 層 Delta、Design Invariants）。**模型／檔位**：實作 Opus／xhigh；敵對 Fable／xhigh 只讀（同 -spike-pageguard）。
- **估時**：11～22 h（推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：os.md RC 9（**不是被取代**：使用者在 U1 決定例外或改寫，本批照決定標）。
- **風險與停止**：MSVC 最佳化組態攔不到、或與 doctest／CRT 衝突、或 VEH 風險十條（ci-readiness §3.3）任一實際發生→**不重排計畫**：第三層不採，LIM-3 的 L1 維持，獨立頁配置介面保留但走一般配置；其他批不受影響——這就是它不在關鍵路徑上的理由。**若失敗**：沒有後續批白做（§8 不觸發重排；ci-spike-retire 改依 U1 不採的條件）。守衛與突變的去處（§5.2 第 4、7、8 列重判）：os 邊界守衛對 os 新檔、`gyo_expect_os_links`（不加系統庫）、`OsHeaderHygieneTests`（新公開標頭）留；新增（第三層測試）的突變一覽敵對檢查者 Fable（與作者 Opus 不同、不是小模型；可由本批的敵對 Fable／xhigh 兼任）。

### 3.11 `v8-arch-plugins-foundation`（plugins 層進 build graph；plugin 選擇是資料；headless-runner；asset-io 服務）

- **Feature**：Product 的追加・複製・削除不改共通層（AGENTS §7）延伸到 plugins：刪一個 plugin，Engine 與其他 plugin 成立；Match 能以 headless-runner plugin 啟動（object_fps_pvp Match）；Match 的世界內容由 IO 執行緒載入、在交換點進 World，不同步讀檔（target/assets §1 第 4 點；Client 與四工具重用同一服務）；開發者看到 `plugins/` 進 build、headless-runner 能跑一個合成 Runtime 並在 SIGINT 依序停下。
- **Contract**：plugin 介面（spec C.5：`Name`／`Requires`／`Build`／`Finish`）；runner 在 finish 安裝、一 Runtime 一 runner（R-E6）；**plugin 選擇是資料**（`v8-arch-plugin-selection-data`）：`project.json` 加 `plugins` 欄（有序清單或依賴宣告，二選一在宣告寫明——使用者決定行說「初始化順序由資料或 plugin 宣告的依賴決定」），共通 CMake 讀它連結 plugin target 並**由建置產生登記的編譯單元**（與 `components` → `GyoComponents.cmake` 同一條路）；`project.json` 格式版本升一（AGENTS §8）；缺 plugin／依賴衝突是 configure 的明確錯誤（§9）；System 的登記仍是程式（plugin 的 build 函式）；守衛掃 `main` 與產品程式不直接引用 plugin 的 build 函式（runtime-composition DI 2，名稱本批定）；**loader 由 plugin 在 build 函式裡呼叫 `engine/asset` 的登記 API 登記**（target/assets §1 第 3 點）；**asset-io 服務**（`v8-arch-migration-plan-round2`）：IO 執行緒讀檔、完成項由 Host 在交換點寫進 external 的 `AssetInbox`（target/assets §1 第 4 點、spec I.1／I.2，與 Link 同形），X-3 親和；Kernel 不擁有執行緒（DI-KS 15），執行緒在本 plugin。
- **分段交付**：(i) **最小形**＝plugin 介面與 build 函式 API、`project.json` 的 `plugins` 欄與建置產生的登記——只依賴 registry-schedule，先交給 match-sim（產品 (c) plugin 在 build 函式登記 System 與 P13 loader）；(ii) headless-runner 與 asset-io 服務——依賴 threading 的交換點原語與親和契約，在 match-host 前完成。兩段同一批、同一宣告，(i) 完成時在 HANDOFF 記一行（match-sim 的開工條件）。
- **段 (i) 的判定與審查**（第三輪對抗 N3-5）：段 (i) 判 (1)(2)(3)(4)(6)(7)(9)(10)（只含段 (i) 的檔），由本批審查者（Fable／high）審過、寫 dev_log，再在 HANDOFF 記一行放行 match-sim；段 (ii) 判 (5)(8)(11)(12) 與全批的 (9)(10)。段 (i) 也讓產品自己的最小 plugin（例：match-sim 用的 pvp 最小 ufbx plugin，登記 Match 側的 `ModelAsset` loader；S3-3）經同一個 build 函式 API 登記。
- **Architecture**：§4 Delta-2（`plugins/` 新 top-level）與 Delta-5（Product Registration 加 `plugins` 欄；已定）。Contract 撐不住的地方：今天的登記（兩份 CSV、`project.json` components）只描述 Engine 元件、沒有 plugin 的位置；`GyoApps.cmake`／`GyoComponents.cmake` 要加讀 `plugins` 欄與產生登記單元。
- **範圍**：新增 **`docs/plugins/`**（plugins 層自己的 owner 文件：README＝層的 DI——無產品名、中性命名、每 plugin 可刪——每 plugin 一頁含 DI 與 Replacement Criteria；framework DI 1「不是產品」／DI 3「中性」的性質由層 DI 承接；target/runtime-composition 的 DI 只留跨 plugin 規則；Q8）；新增 `plugins/`、`plugins/headless_runner/`（收 `HeadlessShell` 與 `match_main` 的迴圈形狀、停止順序；audit §10.1／§10.2）；asset-io 服務（放 headless-runner 內或獨立的 `plugins/asset_io/`，宣告寫明；sdl-window-runner 在 plugins-frame 啟動同一服務）；`build/cmake/GyoBuild.cmake`（加 `plugins/`）、`GyoApps.cmake`／`GyoComponents.cmake`（讀 `plugins` 欄、產生登記）；依賴守衛（`engine/` 不 include `plugins/`）；`FRAMEWORK` component 與 `GYO_ENABLE_FRAMEWORK` 此時**不動**（framework-delete 才動）。
- **受理判定輪廓**：(1) 依賴守衛自測（正例先命中）；(2) 刪除驗證——合成一個測試用 plugin **與一個合成 `project.json`**（兩者都放測試夾具 `tests/common/plugins/fixtures/`，不常駐 `plugins/`、不改真產品的 `project.json`：常駐＝Dummy Target、耦合真產品＝共通 Test 以特定 Product 為前提，AGENTS §7），加入與刪除只改它自己的目錄與那份合成 `project.json`（共通層零修改，照 -remove-object-fps 判定 2：`git diff --stat` 只有登記）；(3) 缺 plugin／循環依賴各一例讓 configure 失敗並指名；(4) 產生的登記單元與 `project.json` 一致（反例：手改產生物→重 configure 覆寫）；(5) R-E6 測試；(6) **`project.json` 格式版本升一**：既有**六個** `project.json`（object_fps_pvp、四工具，以及 `tests/common/fixtures/copy_game/app/project.json`——`tests/common/ci/app_copy_integration.py` 的 Product 複製夾具；不升它複製測試就壞）**同批升版**、讀取端只認新版（不做兩版相容——相容分支是 AGENTS §7 MUST NOT 的 Compatibility Branch；升版只加一個欄，產品那邊是登記資料的一行）；(7) **Math／編譯契約**（mechanism §3.3 第 4 處）：`plugins/` 在 `GyoBuild.cmake:41` 全域 `-ffp-contract=off` 之後加入，configure 檢查新的 plugin target 吃到該旗標（反例：把 `plugins/` 移到該行之前→檢查失敗）；(8) **windows 觀察 (c) 的第一段**：headless-runner 上跑一個合成 Runtime 的「Start→Run×k→Stop」在測試內重複 N＝200 次（windows-x64 L1，每次 ≤1 s、總逾時 60 s），零逾時、零 hang，輸出每次啟動的 wait backend 名；(9) ctest 名單前後只多本批的測試；(10) L1 三平台綠；(11) **asset-io 服務**：合成資產的交換點測試（兩執行緒、Running 期間零外部存取、交換點計數；完成序打亂時套用仍依請求序號——套用端在 assets-core），本機 TSan 零報告；`engine/` 無 `<thread>`（J-9）；(12) **守衛與突變的去處**（§3 共通；§5.2 第 4～6、10、11、12 列）：**拉回風險**——四個工具正向清單（`ALLOWED_TARGETS` 沒有 `GYO::Kernel` 與任何 plugin target）不改，工具就只能留在 framework 上；處置＝本批初判並加 plugins 對應（改寫），各工具批刪 `ToolFramework*` 項、改掛 plugin target，每條新邊照 §14.4 交使用者。**空轉**：`os_boundary_guard.py`、OS 編譯定義檢查、`ProcessCodePageCheck` 的掃描根不含 `plugins/`——本批 ROOTS 加 `plugins`（締める面，但 DI 守衛→使用者核准；Q1），判定：加入後以一個含 OS 標頭的合成 plugin 檔正例命中。`FRAMEWORK` component 與 `GYO_ENABLE_FRAMEWORK`＝留到 framework-delete 退役；fw-* 針對本批搬走的 `framework/src`（`HeadlessShell` 等）的 29 條重列（檢查者 Fable）；新增：依賴守衛、刪除驗證、`docs/plugins/` 層 DI 的兩條守衛（共用 plugin 無產品名、中性命名；Q8）。`.github/` 若要改（新目錄的路徑過濾）交使用者。
- **範囲の限界**：共通三條；只有 headless-runner 一個真 plugin，其餘切法（audit §10.4 十個候選）在各自的批決定；N＝200 看不到偶發率低於 1/200 的問題（closeout 再以 60 run 統計比較）。
- **段**：上段（新 top-level、Build Graph、Product Registration）。**模型／檔位**：實作 Opus／high（asset-io 服務的交換點與 headless-runner 的啟停順序段 xhigh，比照 net-link、diagnostics-service；第三輪對抗 N3-6）；審查 Fable／high（段 (i) 與段 (ii) 各審一次）；驗證者 Opus／high；新增守衛（依賴守衛、刪除驗證、asset-io 交換點測試）的突變一覽敵對檢查者 Fable（與作者不同、不是小模型）。
- **估時**：19～38 h（HeadlessShell 0＋S1 Match 半 5～10（audit）＋build 層 5～10＋產生登記 4～8＋asset-io 服務 4～8（後三項推定）；最小形＝build 層＋產生登記 9～18 h；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：architecture.md 三層表（L52-85）與 framework 定位（L33／L50）→ runtime-composition §4；AGENTS §9 候選 5 已由決定結案（本批實作「資料」，宣告寫與 §9 的關係給風紀委員）。
- **風險與停止**：plugin 選擇做不到單一事實、或刪一個 plugin 要改共通層、或共通層出現產品名分支（§9 MUST NOT）→停，回使用者。**若失敗**：最小形失敗→match-sim 不開（關鍵路徑停）；全批失敗→match-host、所有工具批的組合方式要重想。

### 3.12 `v8-arch-plugin-net-link`（Link Resource 家族＋IO 服務殼）

- **Feature**：Match 與 Client 的連線都經 Link Resource 在交換點換入換出，IO 執行緒碰不到 World（跨產品能力：object_fps_pvp 的 Match（Gateway loopback TCP）與 Client（asio））。
- **Contract**：Link 家族（spec I.2、H.2b 第 2 點、K-S5）：`Link`（external、zone=B）、收件依種類分 `LinkMembershipInbound`／`LinkInputInbound`（每筆 `{arrivalNs, bytes}`）、`Link.outbound`；持有者標記各一；交換點一起 swap；wire 不變。**分層（第一輪對抗 S-11）**：plugin 只處理**位元組、到達時刻與交換**——提供這些 Resource 的型別、swap、IO 執行緒的服務殼（socket、HTTP、執行緒）；「這筆是 membership 還是 input」的**種類判別與 proto 解碼在產品側**（產品的 IO adapter 在外部執行緒依種類填入另一半——I.1 的「外部執行緒依種類填入」——或產品的 Ingress System 解碼）。plugin 不知道 `client_v9.proto`／`runtime_v9.proto`，不 include `fps::pvp`。
- **Architecture**：§4 Delta-2 的一個 plugin（audit §10.4 net-link 候選）；asio、httplib、protobuf 的**執行期**（socket、pool）是服務內部實作（audit §0），產品 proto 的 generated code 在產品。
- **範圍**：`plugins/net_link/`；**不搬** `IpcHost`／`ClientConnection` 的產品邏輯（各在 match-host、client-link；它們的 adapter 把 wire 訊息分到兩個 inbound Resource）。
- **受理判定輪廓**：合成 Link 的 swap 測試（兩執行緒；Running 期間零外部存取，以交換點計數證明；合成的「種類」只是兩個 byte 標籤，不用產品 proto）；J-41（(c) 讀 Link 失敗）在 plugin 登記下仍成立；protobuf generated pool 若由 Egress 直接編碼，比照 I-5 明列例外（audit §3.6）；**反向刪除驗證**：`plugins/net_link` 不 include 產品 proto、`fps::pvp`、`RetroFPS`、`object_fps_pvp`（掃描先以正例——暫時加一行 include——命中），刪 object_fps_pvp 的登記與目錄後 plugin 仍 build 並自測綠；正向刪除驗證（刪它只壞 Match／Client）。
- **範囲の限界**：共通三條；network 標籤測試只在本機（memory ci-runs-cpu-shader-only）。
- **段**：上段（網路資料流的 Architecture）。**模型／檔位**：實作 Opus／high（交換點的執行緒順序 xhigh）；審查 Fable／xhigh；驗證者 Opus／high。
- **估時**：11～22 h（從 audit P3 20～40、P4 30～50 抽出共用殼；抽法推定；P3／P4 各扣；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：無（network-architecture 的入口表在 match-host／client-link 加註）。
- **風險與停止**：Match 與 Client 的 Link 形狀分不出共同部分（例：Match 的同步 refusal 必須保留）→停，改為兩個 plugin 並記重工（§8 RC-6）。**若失敗**：match-host 與 client-link 沒有 Link 實作——G2b 判不了、階段 3 不開。守衛與突變的去處：碰到的既有守衛無（plugin 的反向刪除驗證與 swap 測試＝新增；新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點））。

### 3.13 `v8-arch-diagnostics-service`（通用診斷 sink 成服務；windows 觀察 (a)(b)）

- **Feature**：任何產品的 (c) System 能經 `DiagnosticsWriter<T>` 輸出診斷、由 plugin 提供的 sink 落到檔案（object_fps_pvp 的 Match／Client 開發者是第一個使用者，見 3.13b；四工具的 log 是第二個）；Windows 上不再因檔案句柄與執行緒順序失敗。
- **Contract**：`DiagnosticsWriter<T>`（(c) 唯一 effect 例外，spec S-8、J-14）寫 World 端緩衝、在交換點交 sink、Interrupted 不丟（spec B-13）；sink 接收端的介面（型別無關：`T` 由產品定、plugin 只搬位元組與記錄邊界）；trace 檔的關閉契約：flush 與檔案句柄在 `Close` 返回前全部釋放、尾行順序＝寫入順序；`engine/trace` 的檔案格式不變。
- **Architecture**：trace-diagnostics plugin **只收通用服務**：`TraceSink`／`LogFile` 的服務化、`DiagnosticsWriter<T>` 的 sink 接收端、檔案生命期契約（第一輪對抗 B-3：第一稿把 PvP 專屬的 `MovementDiag`（`namespace fps::pvp`，include `RetroFPS/Pvp/Movement.hpp`）與 P8／P12 放進這個 plugin，是 plugins 層反向依賴產品、違 AGENTS §6／§9 與 Delta-2 第 5 項；audit §10.4 的表明寫切法候選「不是決定」）。Contract 撐不住的地方：`engine/trace` 今天是程式庫不是服務（無交換點、無 Runtime 生命期）——加一層服務殼，`engine/trace` 本身不改。
- **範圍**：`plugins/trace_diagnostics/`；`engine/trace` 不改（trace DI 1～8 相容；trace RC 1「重放記錄的路徑」的措辭在本批宣告註明與 sink 的關係，legacy-impact §9 第 8 點）；**不碰** `apps/object_fps_pvp`（P8／P12 與 `MovementTrace` 在 3.13b）。
- **受理判定輪廓**：**windows 觀察 (a)**：`DiagnosticFileTests.cpp:216` 形狀（檔案開著時 `remove_all`）以「Close 後無開啟句柄」的斷言固定，在測試內重複 100 次、windows-x64 L1 零「process cannot access the file」；**(b)**：`TraceSinkTests.cpp:155` 形狀的 Close 尾行順序以契約測試固定、100 次相同；J-14 的 Diagnostics 例外不擴大到其他 effect；trace 既有測試全過；**Product Boundary**：`plugins/trace_diagnostics` 不引用 `fps::pvp`、`RetroFPS`、`object_fps_pvp`（`git grep -nP`，先以正例——暫加一行 include——命中確認掃描有效、不丟 stderr）；刪 object_fps_pvp 的登記與目錄後該 plugin 仍能 build 並通過自測（刪除驗證，與其他 plugin 批同形）；合成 `T` 的 end-to-end 測試（合成 (c) System 寫 → 交換點 → 檔案，Interrupted 時記錄不丟）；**守衛與突變的去處**（§5.2 第 15 列）：trace 的連結期望與 `TraceSinkTests` 留（`:155` 形狀另以 (b) 的契約測試新增；新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點））。
- **範囲の限界**：共通三條；兩條觀察的舊原因未特定，本批驗的是「新服務上不出現」（不報「修了舊的」，AGENTS §14.3）；在測試內重複 100 次看不到低於 1/100 的偶發（closeout 60 run 比較）。
- **段**：上段（公開介面 `DiagnosticsWriter`、執行緒、檔案生命期契約的 owner）。**模型／檔位**：實作 Opus／high（檔案生命期與執行緒順序段 xhigh）；審查 Fable／high；驗證者 Opus／high。
- **估時**：7～16 h（trace 2～6（audit）＋sink 接收端與檔案生命期 4～8（推定）；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：無（trace.md 相容；RC 1 加註）。
- **風險與停止**：windows 重跑仍有失敗且分不出實作／測試／環境→照 §14.1 切分報告，不判「已修」。**若失敗**：pvp-diagnostics 沒有接收端；match-sim 可暫以「診斷關閉」跑 golden（digest 不含診斷），所以只擋 match-host 的合併、不擋 match-sim 的 golden 驗證。

### 3.13b `v8-product-pvp-diagnostics`（PvP 的移動診斷與 trace 改經通用 sink；owner object_fps_pvp）

- **Feature**：移動診斷與 `TraceMovement` 在新架構下照常輸出，檔案格式與欄位語義不變（object_fps_pvp 的 Match／Client 開發者；場次證據 C7 與分析工具照舊讀得懂）。這是第一輪對抗 B-3 從 diagnostics-service 拆出的產品批。
- **Contract**：`MovementDiag`／`MovementTrace` 的記錄型別（`playerId`、`lifeGeneration`、`MovementDiagRole{Match, Client}`、`timeNs` 等）**owner 仍是 object_fps_pvp**，只經 diagnostics-service 的通用 sink 輸出；**`TraceMovement` 的時間來源**（第一輪對抗 S-17，採 (a)）：用被記錄的 boundary 輸入 `HostClockSample`（G.4 第 1 條的 Resource；重放 host 能重現），`timeNs` 保留牆鐘語義、合 target/determinism §2 的 boundary 規則；**若任一欄的語義仍不得不變**（例：某欄只能由 tick 推出），升 trace 契約版本（AGENTS §8：Version 是 Data Contract 的一部分）並在宣告列出受影響的消費者（C7 分析腳本、對時工具），不得默改。
- **Architecture**：產品內部（應用 Architecture）：`MovementDiag.hpp:109` 的自起 `std::thread` 與全域 atomic 指標（G.4 第 3 條；-zones 的 E 14 處所在）改成句柄：Match 側由持 `DiagnosticsWriter<T>` 的 (c) System 寫緩衝、sink 在交換點收；**舊 Client 仍用全域與寫檔執行緒**（`apps/object_fps_pvp/main.cpp:157` 的 `MovementDiagFile`、`LocalPlayerPrediction.cpp:21-31` 的 `RecordMovementDiag`、`ClientConnection.cpp:714、803` 與 `PvpApplication.cpp:1369` 的 `TraceMovement`），U3 (a) 下 Match 線合併後 master 上的 Client 仍是舊的，所以**全域與寫檔執行緒保留到 Client 側呼叫點遷完**（client-sim 的 PR 線），本批不刪（第二輪對抗 S2-7）；P8／P12 的服務部分的**位置由產品線提、架構師核**（kernel-prep 卡第二批的規則）。
- **範圍**：`apps/object_fps_pvp` 的 P8／P12（audit §1.3）、`MovementTrace.hpp`／`MovementDiag.hpp`／`MovementTraceWriter.hpp`（句柄形式）；Match 側呼叫點 6 列（`PvpMatch.cpp` 4、`MatchRuntimeHost.cpp` 2）——這 6 列在舊 `PvpMatch`／`MatchRuntimeHost` 內，改的是取得方式（全域→句柄）、不改輸出（判定 (1)(2) 逐欄相同），它與 §3.14「並存期間任何一路的行為修改都禁止」的關係在宣告逐列說明；`tests/object_fps_pvp` 的對應測試、守衛與突變的去處。不碰 `plugins/`、`engine/`、Client 側呼叫點。
- **受理判定輪廓**：(1) trace／log 的檔案格式與每一欄語義逐欄對照不變（宣告列欄表；任一欄變＝版本升一並列消費者）；(2) 以同一記錄（-detect 的重放記錄）在舊路徑與新路徑各產一份 trace，除 `timeNs` 的牆鐘值外逐欄相同、`timeNs` 單調且與 `HostClockSample` 一致；(3) Match 側路徑不經自起執行緒與全域 atomic 指標（掃描：Match 側檔案的 `std::thread`、`std::atomic<…*>`、全域診斷呼叫 0 命中，正例先命中）；全域與寫檔執行緒只剩 Client 側使用者，逐處列名（client-frame（PR 線末端）歸 0；client-sim 只消 `LocalPlayerPrediction` 內的呼叫點）；(4) `kernel_prep_sites` 白名單 G 類 13 列拆兩半：**Match 側 6 列**（`PvpMatch.cpp` 4、`MatchRuntimeHost.cpp` 2）本批逐列歸零，**Client 側 7 列**（`LocalPlayerPrediction.cpp`）不動、歸 client-sim（改登成別類＝改守衛判定，寫進事前宣告並照 §14.4 寫有無緩める面）；(5) J-17 ZS4 在確認清單為 0；(6) 測試、守衛與突變的去處表（§3 共通；`kp-*` 中針對診斷的突變改針對句柄）。
- **範囲の限界**：共通三條；診斷關閉時 golden 不受影響（digest 不含診斷），所以本批不擋 match-sim；只擋 match-host（P8／P12 接進真實 host）。
- **段**：上段（資料契約的語義與版本、執行緒）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high（若 trace 契約版本要升）。
- **估時**：9～18 h（audit P8 8～16＋P12 4～10 扣 diagnostics-service 已做的服務部分：扣法推定，6～12；＋測試、守衛與突變的去處 2～4（推定）；＋守衛去處判斷 1～2（推定，§3 共通））。
- **並行**：與 match-sim 並行（接點：Sim 的哪個 (c) System 持 `DiagnosticsWriter<T>` 由 match-sim 的宣告定），需要第二張產品卡；不開時接在 match-sim 後、match-host 前，關鍵路徑加 9～18 h（第二輪對抗 N2-8）；在 match-host 之前完成。
- **被取代的 legacy 條文**：無（network-architecture／entity-model 對 `MovementDiag` 的描述在 match-host 加註）。
- **風險與停止**：`HostClockSample` 的取樣粒度（每 tick 一次）不足以保留今天 `timeNs` 的用法（例：分析工具依賴 tick 內多個時間點）→停，交使用者選 (b) 升版；**若失敗**：match-host 的 P8／P12 接點沒有落點，擋 match-host 合併。

### 3.14 `v8-product-match-sim`（PvpMatch 拆成 (c) System；重放 host 上 golden 37）

- **Feature**：玩家今天的對局判定（移動、射擊、擊殺、復活、動畫推進）在 kernel 上產生**相同結果**（Product：object_fps_pvp Match；Client 在 client-sim 重用同一組 System）。支撐的下一版 Feature：loading 畫面（Match 側機制，本批做地圖與玩家兩個 stage）、場景線第一個非玩家實體（「可互動物件」stage 的掛點）。結束時＝M2：37 個 golden 由 kernel 上的模擬產生、逐位元相同；Plan 印出 Simulate stage 的 System 順序。
- **Contract**：Data 不變（arena、golden fixture、`AuthorityDigest` 編碼）；wire 不變；Body 依 entity-model 第 139 行的欄位分類表（a6-revision 已搬進**產品的 target 文件**，不在 target/world）拆成 Body／Life／InputCursor／Connection 診斷四欄（`v8-arch-kernel-writer-unit-system`）；Player／Peer 分開、Peer 元件不掛在 Player 實體上（runtime-composition DI 3，N-7）；Life 的寫者形狀照 U6 已定：`LifeState`（單一寫者在 tick 開頭，處理到期復活）＋`DeathEvent`（`ResolveActions` 發；走 spec F 的事件生命週期，同步點清掉；回滾重放時重發、不另存）、**不設中間狀態欄**，snapshot／digest 組裝在其後讀；(c) 時間用整數 ns（J-17 規則 1；-zones 已改型別）；資產階段用 **assets-core 的 `Assets<T>`／`AssetInbox`**（真資料），(c) 程式只經 `Res<Assets<T>>` 取資產、不用舊 `AssetManager` 也不經轉接（Startup 不放內容，runtime-composition DI 3；存取要在簽名上，DI-KS 1）；P13 loader（arena、移動規則、玩家動畫內容、pose）由產品 (c) plugin 在 build 函式呼叫 `engine/asset` 的登記 API 登記（target/assets §1 第 3 點；plugins-foundation 的最小形），loader 收位元組或 `AssetInbox` 的完成項、不收 `std::filesystem::path`（assets-core 的契約；-zones 的 B 5 處由此消去）；產品 Resource 契約：`EntityAllotter`（單調不重用、generation 0）、`HostClockSample`、`PublishedReferences`、`MembershipRequests`、`MembershipCursor`、`InputRing[tick]`、`LoadingState`；digest 欄不得 Shared（LIM-4）。
- **Architecture**：產品 (c) plugin，namespace `fps::pvp::sim`（-zones 定的 `inline namespace sim`，v1；`v8-product-kernel-prep-zones-gate`；naming 第 4 條）——**改非 inline 的時點定為 closeout**（舊 Client 已刪、工具已遷之後；架構師 2026-11-12 定），本批沿用 inline，新程式用 `fps::pvp::sim::` 拼法以便 grep。拆法：`PvpMatch::Tick` 的寫死序 → `ApplyMembership`、生死邊界、`StepMovement` 呼叫點、`ResolveActions`、`AdvanceAnimation` 等 System；`AuthorityDigest` 成 System（依 id 升冪讀欄）；重放 host（spec H.4.4）取代今天 `AuthorityDigest` 執行器的驅動部分；`Movement.cpp` 不改（audit §2、G.4 第 3 條）。
- **範圍**：audit §1.3 P1 `PvpMatch`（重寫為 (c) plugin）、P7 `StepMovement`（留）、P9（留；`ShotReferenceAge` 改法在 match-host）、P13（解析留；簽名改收位元組——-zones 的 B 5 處：`Sim/Arena.hpp`、`Sim/MovementRulesFile.hpp`、`Sim/PlayerAnimationContent.hpp`、`Pvp/NativeFiles.hpp:4`、`Sim/PlayerPose.hpp` 的 `<filesystem>`）——**舊呼叫端**（S3-2）：讀檔入口留在 Sim 外（`Pvp/` 保留原名原簽名，內部讀檔後轉呼 Sim 的解析），是期限明確的轉接，使用處登記成清單：舊 `match_main.cpp` 的 `:100`、`:106`、`:125`、`:137` 在 match-host 刪舊路徑時消失，舊 Client 的 `PvpApplication.cpp:963` `LoadInstalledArenas` 等在 client-frame 歸零，`tests/object_fps_pvp` 的使用者隨測試去處表處理；並存期間舊路徑保留 `<filesystem>`（不在 Sim，不違反門）；P11 不動；候選清單的同型項（`Sim/PlayerHitbox.hpp:9` `LoadPlayerHitbox(path)`＝B 型、`Sim/PlayerPose.cpp:17` `unordered_set`＝F 型）同法處理（候選清單不是門）；-zones 的 F 2 處（`App/AnimationSetDefinition.hpp:8、23` 的 `unordered_map`）換有序容器（判定：digest／golden 零差；依賴 §5 第 40 列的區判定；舊 Client 的共用使用者同批跟著換）；**Match 側的 ufbx（`ModelAsset`）loader 由 pvp 自己的最小 plugin 經 plugins-foundation 最小形的 build 函式登記**（今天 `PlayerAnimationContent.cpp:88-96` 在 `LoaderRegistry` 登記 `UfbxModelLoader`；產品 loader 讀 `Assets<ModelAsset>`、不重複解析；plugins-frame 的 model-assets plugin 成形後取代它，S3-3）；-zones「回報架構師」四項（§5 第 39～42 列）依 a6-revision 的區判定搬動或改含入（含 `LoadPlayerPresentationDefinition`／`SamplePlayerPresentationPose`：只有 Client 用，判呈現則搬出 Sim）；`src/Collision`／`src/Gameplay`（留）、`AuthorityDigest`（包成 System）；`PlayerHitbox.cpp`／`PlayerPose.cpp` 在新 (c) 路徑只經 `Res<Assets<T>>`；**過渡轉接**（assets-core 的 `AssetManager` → `Assets<T>` 轉接）的方向＝舊程式（`AssetManager` 的消費者）經轉接讀到與 `Assets<T>` 同一份內容，新 (c) 程式不經轉接；Match 側的轉接使用者＝並存期間的舊 `PvpMatch` 路徑（清單在宣告），**出口＝match-host 刪舊 `PvpMatch` 時 Match 側使用處歸零**（Client／ui_editor 的使用處在 client-frame／tool-ui、轉接本體在 framework-delete）；被新 (c) 與舊 Client 共用的葉子函式不得在同一函式內並存兩種後端（spec K.2 第 8 條），做法寫進宣告、做不到→停；`tests/object_fps_pvp` 的 replay host 改跑 Match Runtime；**測試、守衛與突變的去處**（§3 共通）：引用 `PvpMatch` 的 Sim 層測試（`CombatTests` 54、`PvpMatchTests` 63、`EntityModelTests` 等，逐檔列表在宣告）；搬到 -zones 卡整理出的 `Sim` 目錄。**不碰** P2～P6、P11。
- **受理判定輪廓**：(1) 件数：golden 37／比對 37／差 0，或差的只限事前宣告列名的 LIM-5 集合（§14.4 的 1～5 寫好、驗證者複核、逐條核准，U9）；(2) **D1 的 Life 時序在新路徑實測**（`KernelPrepDetectTests` 改寫：經新 host 觀測 T=35／R=215；若使用者在 U6 選改變時序的方案，退役並記錄、golden 照 U9 逐條核——Q6）（第一輪對抗 S-1：第一稿「不改一字仍過」在結構上達不到——D1 直接建構 `PvpMatch`，跑的是舊程式）：以新的重放 host 跑 D1 的劇本、觀測 snapshot(T)／snapshot(R)，T=35、R=215、R−T=180 與 -detect 固定的現況相同；針對新 System 的突變 `kp-death-next-tick`／`kp-respawn-next-tick`（改成讓 `DeathEvent` 晚一 tick 被 digest 組裝讀到／`LifeState` 晚一 tick 復活）各以指定的 CHECK 失敗；(3) **J-17 確認清單＝核定的 7 處殘留**（`test_zone_c_lists.py` 的 RESIDUAL 釘住，**不是 0**；`v8-product-kernel-prep-zones-gate`）：開工時掃描結果正好等於 RESIDUAL；本批消去 B 5 處（簽名改收位元組）與 F 2 處（換有序容器）時，RESIDUAL 只因該處原因消除而刪該列（締める面，照 §14.4 事後報告；多一處、換一處都不成立；架構師 2026-11-12 核）；新 Sim 檔（清單由 G-5 匯出）零新增違規，`zone_scan` 結果與手寫版相同；候選清單 9 是參考值不是門檻；(4) Loading 等價（分一批與多批、資產完成順序打亂，ready 時 (c) 欄逐位元相同；J-51 的產品版，真資料經 assets-core）；(5) D-E2＝0 且順序邊理由沿用 dryrun；Peer 元件不在 Player 實體上（產品測試）；(6) `cFingerprint` 印出存證據；(7) **G2a 集合的數字**（§2.1：BU-1、BU-2、BU-4 World 位元組、BU-6 重放）在重放 host 上量出（給 first-host-cost）；(8) 測試、守衛與突變的去處表逐檔勾完——**拉回風險**：`EntityModelTests` 是 `PvpMatch` 的 friend、直接讀七個私有欄（`bodies_` 等），要它編譯就得保留 `PvpMatch` 的私有 vector；處置＝同批退役或改寫（G1-i／G2-i 退役由 J-1／J-2／J-32 承擔，G1-ii 改寫成產品掃描，G7／G13 改寫成對 World 與 Plan 謂詞的測試），**不留 `PvpMatch` 門面包 World**、不拖到 match-host；`RequireEntityOrder` 退役；G3 歸 match-host（盤點 §2.18）。DI 守衛（`EntityModelTests` G1、G2、G7、G13；§5.2 第 18 列）判四選一——G1／G2 守的性質由 J-1／J-32 承擔時是退役或改寫，照 §14.4 1～5 交使用者；golden（G11）留；D2 棘輪白名單（`test_kernel_prep_sites.py`）＝改寫型（搬檔只改檔名欄、不放寬）；改寫的守衛其突變一覽由與作者不同的模型敵對檢查一次；(9) F 2 處換有序容器後 `AuthorityDigest`、`AnimationContentDigest` 與 golden 37 零差，Client 呈現層既有測試全過、舊 Client 的 arena／animation digest 不變；(10) Sim 外讀檔入口的使用處清單與 `git grep` 件数一致（正例先命中）；(11) §5 第 39～42 列照 a6-revision 的區判定處理，確認／候選清單與 `test_zone_c_lists.py` 隨之更新（改守衛判定照 §14.4 寫有無緩める面）；(12) **守衛與突變的去處**另含：naming DI 1 的守衛（kernel-scaffold 新增）對新 `fps::pvp::sim` 檔照判、-zones 的 kz-* 突變（4＋1 條）判四選一；新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型）。
- **範囲の限界**：共通三條；**LIM-5**（Join／Leave 晚最多一 tick；以對局開始為基準的 golden 預期不變——實測，不先假設。**U14 已定 (a)**——未結事項（可能的設計缺陷，HANDOFF「未結事項」節）：v1 的 Match 玩家 stage 不生成成員、初始成員全走 FixedUpdate 掛點，H.2b 第 4 點的 Loading 路徑只在合成測試成立——第一輪對抗 S-8）；**LIM-4**（digest 欄不得 Shared：某 digest 欄必須 Shared 才能登記＝契約撐不住，停）；只在重放 host（腳本時鐘）：Ingress／Judge／Egress 與 IPC 不在本批，G.4 的牆鐘讀點仍在舊 host（白名單 W／A 列不減是預期）；`PlayerAnimation` 推進的區（audit §4.5 標錯候選）等決定性線核對前照現況標。
- **段**：上段（digest、資料形式、區）。**模型／檔位**：實作 Opus／high（Life 漏斗、Body 拆欄與 golden 比對段 xhigh）；審查 Fable／xhigh（重點：搬錯區、拆欄改了寫入時序、digest 編碼）；§14.4 驗證者 Opus／high 另一代理（LIM-5 集合要重設基準時，以及 DI 守衛測試改寫時）。
- **估時**：47～103 h（audit P1 40～80＋P7／P9／P13 4～18＝44～98，扣 -zones 已做搬區與整數 ns 約 20%＝35～78（扣法推定）；＋測試、守衛與突變的去處 10～20（推定）；＋F 換容器 1～3（推定）＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**（重標後）：entity-model E-7、E-12、RC-2；E-3 依新定義歸「相容（措辭更新）」；entity-model 第 153 行「拆欄不在 split 做」（誤導句 §5 第 16 條）。
- **風險與停止**：golden 不逐位元、原因分析兩輪不收斂→停（§14 冒頭；§8 RC-4）；差異只在 LIM-5 預期的情境→不自己重設基準，照 §14.4 交使用者；Body 拆欄需要 Shared→停（LIM-4、K.2 第 10 條）；Life 形狀若需同窗讀自己生成的實體（K.2 第 9 條）→停。-zones 未完成（`object_fps_pvp.zone_c_lists` 不綠，或確認清單不等於 RESIDUAL 的 7 處）→本批不開；assets-core 或 plugins-foundation 的最小形未完成→資產階段與 loader 登記沒有落點，本批不開；換有序容器後 digest 或 golden 有差→停（不改 digest 編碼，交決定性線核對）。本批與 match-host 同一 PR 線（U3）：批內舊 `PvpMatch` 與新 (c) plugin 並存只在分支上、不進 master；並存期間任何一路的行為修改都禁止。**若失敗，match-host、client-sim 白做**。

### 3.15 `v8-arch-first-host-cost`（閘門 G2a：重放 host 上的成本）

- **Feature**：無新 Feature；AGENTS §2「他の host・Product へ広げる前に、一つの host で費用を計測し、計画の予算と比べる」的第一段。結束時＝M3：同機、同情境、新舊兩路的 tick p50／p95／max、World 記憶體、載入時間，與 go／no-go。
- **Contract**：與 pvp-baseline 同口徑、同機器、同工具鏈、同組態；BU-6 的分母＝pvp-baseline 以同一內容、同一重放形式量的今天載入時間（不是真實行程的「到 ready」，第二輪對抗 S2-10）；報告格式照 §6；量測執行檔不進 ctest。
- **Architecture**：不變。
- **範圍**：量測執行檔改跑遷移後的 Match Runtime（重放 host；驗證關／開兩種）；`tests/common/kernel/` 的合成負載記錄執行檔（BU-2）。
- **受理判定輪廓**：(1) 件数：宣告 tick 數＝記錄數（扣暖機 600 tick）、每情境 5 次都有；(2) **G2a 集合**（§2.1 只寫一次：BU-1 Match tick 的工作、BU-2 Kernel 自身開銷、BU-4 World 位元組與快照緩衝、BU-6 載入到 ready 的重放 host 值）對 §6：全在預算＝go（放行 2' 階段：rollback-extract、engine-wrap、plugins-frame 開工）；任一 1.0～1.5 倍＝報告，使用者決定；任一 ≥1.5 倍＝停、重排（§8 RC-1）；(3) 驗證開與關各量一次，報告附開關值與排除欄清單（LIM-6）；(4) 量測前告知使用者機器負載。
- **範囲の限界**：共通三條；重放 host 沒有 IO 執行緒與交換點的真實負載——真實行程的 BU-1′、BU-4 RSS、BU-6 真實行程值在 match-host（G2b）再量（它們的分母由 pvp-baseline 先量好）；判定在 Docker Linux 容器、與 pvp-baseline 同一映像（U8；Intel Mac 原生只作參考）；L2 本機量測不是 CI 判定。
- **段**：上段（決定是否繼續疊 host，屬 Architecture 判斷）。**模型／檔位**：量測與記錄 Opus／medium；門檻來源的對抗檢查 **與宣告作者不同的模型**／high（看結果前；§6 與宣告由主會話寫，所以檢查者是主會話以外的模型）；判定由主會話下（模型由使用者定）；驗證者不需要。
- **估時**：4～8 h（推定）。
- **被取代的 legacy 條文**：無。
- **風險與停止**：舊路徑基線同機 5 次 p95 散佈 >10%（與 pvp-baseline 同一門檻）→先停下判環境，不改門檻（§14.1）。**若失敗**（任一項 ≥1.5 倍，或使用者在 1.0～1.5 倍時決定不放行）：2' 階段與 match-host 不開，rollback-extract、wrap-*、plugins-frame 尚未開工所以不白做；已完成的 match-sim 留在分支（U3 (a)：不進 master），照 §8 RC-1 重排。守衛與突變的去處：無（量測批）。

### 3.16 `v8-product-match-host`（真實 Match 行程跑在 kernel 上；閘門 G2b）

- **Feature**：玩家今天的對戰，Match 端在 kernel 上；**用現有 Client 連 Gateway 就能玩**（wire 不變）；玩家可見的唯一差異：refusal 回覆可能晚一 tick（U7 已定 (a)）。支撐的下一版 Feature：觀戰者（Peer 實體；Player／Peer 分開是本批的欄形狀）。pv10 複製層不列為支撐對象：它依賴已暫停的帳本與兩通道（networking N3、N4；`v8-arch-kernel-pending-paused`）。結束時＝M4。
- **Contract**：wire 不變（pv9）；Link 家族（net-link）；`HostClockSample`、`PublishedReferences` boundary Resource（G.4 第 1 條：射擊 reference age 保 pv7 D-3「在 tick A 量一次」）；連線品質 reference age 用 ring 的到達時刻（G.4 第 2 條）；`RequestMovementResync` 改由 Judge／Simulate 的 (c) System 從到達時刻算（G.4 第 4 條）；IPC 執行緒不碰 World（G.4 第 5 條＋-detect 補列）；`connectionQualityFailures` 成為 Player 的 Connection 診斷欄、Judge（(c)）唯一寫者（-detect D3）；refusal 的回覆時機（U7 已定 (a)，最多晚一 tick）——**事前宣告第一步查證 pv9 契約與 Gateway 測試對同步回覆有無規定**（spec 附錄二「未查證」），有規定→停；實作照 bevy_replicon 式分層：IO 服務只搬位元組（排程開頭 insert、末尾 drain）、Judge 在 tick 內的 (c) System 判、refusal 寫進 per-connection 的 Egress Resource `{tick, 動作序號, 原因}`、Egress 在同 tick 末尾送出；**不做** ServerTick 門與 tick 前綴對齊（屬 pv10）。
- **Architecture**：headless-runner 上跑 Match Runtime；`MatchRuntimeHost` 的時鐘、執行緒、交換點留在 runner（H.7）；`:386-440` 的交接成 Ingress（`StageMembershipRequests`、`StageInputs`）、`:450-451` 成 Judge（(c)）、`:471` 起成 Egress（spec L.3）；`IpcHost` → net-link 的 IO 服務（plugin 只處理位元組與到達時刻；**訊息種類判別與 proto 解碼在產品側 adapter**，第一輪對抗 S-11）；`match_main` → `project.json` 的 plugins 資料＋Run（audit §10.2 S1）；H-7 守衛由產品持有（spec J.4；基線由 h7-scanner 批在 `tests/object_fps_pvp/` 註冊，本批歸零）。
- **範圍**：audit §1.3 P2、P3、S1（Match 半）；P8／P12 接 pvp-diagnostics 的產出；`AssetInbox` 接 plugins-foundation 的 asset-io 服務（本批只接線、不實作 IO 服務）；-detect 補列：`IngressBucketId`／`RecordRejection` 的 `ContainsPlayer`／`Snapshot()` 搬進 Ingress、`match_main` 的 `TakeIngressStatistics` 改經交換點複製的 Host 統計、**`TickCount()`**（`match_main.cpp:220`，Stop 之後的主執行緒）比照 `TakeIngressStatistics`：runner 在**最後一個交換點**把 domain tick 複製進 Host 統計，主執行緒讀那份複本——不讓主執行緒讀 World（第一稿「Ready 狀態 Host 讀，A.2 允許」是錯的：A.2 的允許條件是「Ready **且在 ownerThread 上**」，W-6 是常開的 GYO_ASSERT；第一輪對抗 S-9）、`QueueActionAcknowledgement`／`SubmitActions` **刪除**（無正式呼叫者，§7；刪 TEST_CASE 前逐 CHECK 看）、`IpcHost` 的 `GetActionResults` ×4 與 `NoteWireRejection` ×2 改讀 `Link.outbound`；`match_main` 的 `PlayerAnimationContent` 載入（`PlayerAnimationContent.cpp:95`）改走 Loading 的資產階段（assets-core）；刪舊 `PvpMatch`（結束並存；Match 側 `AssetManager` 轉接使用處與 Sim 外讀檔入口的 Match 使用處隨之歸零——S2-8、S3-2 的出口；`zone-c-rewrite.json` 的 P1 列移除——`test_zone_c_lists.py` 釘住 rewrite 清單的三個成員，刪列是改守衛判定，照 §14.4 寫 1～5、事前宣告列出）；**測試、守衛與突變的去處**（§3 共通）：host 層測試（`IpcHostTests` 38、`KernelPrepDetectTests` 的 D3 等；逐檔列表在宣告）；§5.2 第 18～20 列（`EntityModelTests` G3 改寫（host 逐出改經 Ingress／runner）；`PvpMatchTests.cpp:82` 的 S3 單一紀錄改寫成獨占欄的啟動檢查＋等式測試；`:1774` 註解隨測試改指產品 target 文件；`state_whitelist_test.go` 留）；**新增**：runtime-composition DI 3「Peer 元件不掛在 Player 實體」的掃描（產品實體登記這批才存在；Q9）。
- **受理判定輪廓**：(1) `kernel_prep_sites` 白名單：A 類 22 列→0、**Match 側 G 類 6 列→0**（pvp-diagnostics 已歸零的列在此對帳；Client 側 7 列在 client-sim；若有列改登成別類，事前宣告寫明並照 §14.4 寫有無緩める面，不在事後改登——N-15）、W 類只剩 runner／Host 檔，件数照 47 列逐列對帳、每刪一列記 dev_log；(2) H-7 掃描 Match 0 違規（產品自己的基線測試；正例先命中；白名單只有 `Run`／交換點）；(3) golden 37（同 match-sim 的集合規則）；(4) Gateway go 測試（`state_whitelist_test.go` 等）、pvp 測試（移植後的全部）、acceptance 30 項全過（network 只在本機）；(5) **D3 的 host 層特徵在新 host 上重現**（W=600、1200／1800／2400 由新的 Judge 寫 Connection 診斷欄、kW 當 tick 進發布快照；D3 案例直接建構 `MatchRuntimeHost`，所以是重寫不是「原樣過」，第一輪對抗 S-1）；Match 層「`Snapshot()` 的 failures 恆 0」是舊結構特徵，改寫照 §14.4 寫 1～5（無緩める面則事後報告）；`kp-failures-*` 突變改針對新 Judge；(6) G.4 第 1、2 條「golden 預期不變」以 digest 實測；(7) **G2b 集合**（§2.1 只寫一次：BU-1′ 真實行程 tick、BU-4 穩態 RSS、BU-6 真實行程到 ready）在真實 Match 行程（Linux）實量，與 pvp-baseline 量好的舊 binary 同情境基準比較；(8) 統計行印 tick p50／p95／max（field-session C3 形狀）；(9) **移行の手間**：match-sim＋match-host 的實際工時合計／兩批事前宣告核准時寫明的估時（上限值）合計 ≤1.5（第一輪對抗 S-19、第二輪對抗 S2-9：分母在看結果前固定；實際工時的定義同 §2.1 G2b；AGENTS §2「移行の手間」），不成立照 §8 RC-3／RC-5 停，不開階段 3；(10) 測試、守衛與突變的去處表逐檔勾完。
- **範囲の限界**：共通三條；**LIM-5**（Join／Leave 在 tick 內、晚最多一 tick；U14 已定 (a)：初始成員全走 FixedUpdate 的 `ApplyMembership`，Loading 的玩家 stage 在真實 host 上不被跑到——未結事項（可能的設計缺陷），引 HANDOFF「未結事項」節）；refusal 回覆時機改變（pv9 未規定時記為行為差異，不是 Defect）；Windows 不是 Match 的主平台（觀察 (c) 的 Client 側在 client-sim／client-link）。
- **段**：上段（協定時機、執行緒、State Authority、事故類 Ingress）。**模型／檔位**：實作 Opus／high（Ingress 的序號配發、Judge 的時序與交換點段 xhigh）；審查 Fable／xhigh；§14.4 驗證者 Opus／high 另一代理。
- **估時**：66～142 h（audit P2 40～70＋P3 20～40＋S1 半 5～10＝65～120，扣 net-link 5～10、加補列 0～5＝55～120；＋測試、守衛與突變的去處 10～20（推定）；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：architecture.md L138（`RuntimeLoop` 三段）的 Match 側、L140-155（product host 擁有迴圈）；entity-model E-12（舊 `PvpMatch` 刪後對任何程式都不再成立→標「已被取代（v8-product-match-host）」）；network-architecture 第 225-227、1034-1057 行（誤導句 §5 第 18、19 條，加註）。
- **風險與停止**：pv9 規定 refusal 同步回覆→停（wire 不在本計畫內改）；`ShotReferenceAge` 改 `HostClockSample` 後 digest 變→停（audit §3.4 第 1 條）；spec K.2 第 7 條（H.7 以外的協調邏輯回到 runner）→停；BU-1′ ≥1.5 倍→停（§8 RC-1）；兩批實際工時／估時 >1.5→停（G2b 第 (9) 項；§8 RC-3），且 >重寫估計→K.2 第 8 條，停（§8 RC-5）。與 match-sim 同一 PR 線合併（U3）。**若失敗，G2b 判不了，階段 3、4 全部不開**。

### 3.17 `v8-arch-kernel-rollback-extract`（回滾與 Extract）

- **Feature**：Client 預測與回滾由機制提供，不由產品手寫重放迴圈（object_fps_pvp Client；玩家可見：本地移動即時、權威修正平順）；渲染只讀模擬（使用者 K.3 第 6 項）；Editor 預覽資料只往前流（三個有預覽的工具）。暫停中的 hitbox-rewind 重開後用同一回滾設施。結束時開發者看到合成 FixedUpdate 跑 10 tick 與「跑到 5、Rewind、Replay 回 10」逐位元相同；`Extract<Write>` 編譯失敗。
- **Contract**：H.5：`Rewind`／`Restore`（部分快照、整欄替換、不觸發 hook）／`Replay`、PredictionHistory 由 Runtime 自動存（深度由登記定）；快照含 domain tick、`rollback = Yes` 的欄與 Resource、配發器、Fixed 事件緩衝；不含 role=Input（Z8）、Derived（D-E16）；D-E15 逐掛點。H.6：渲染 World 自己的 Registry、`Extract<Read<T>>`／`ExtractRes<R>` 只讀、`Extract<Write>` 編譯失敗、來源 Registry 先凍結（D-E14）、同一執行緒（v1）。
- **Architecture**：§4 Delta-1 內；無新邊。
- **範圍**：`engine/kernel/`、`tests/common/kernel/`（合成型別）。
- **受理判定輪廓**：J-42、J-42b、J-42c、J-43（含編譯失敗，形狀照 scaffold）、J-46、**J-49**（`MembershipRequests` 消費與回滾——只在本批，loading 不列；第一輪對抗 S-4）三平台；突變優先清單第 10、15 條 killed；**BU-5**（PredictionHistory；§6）在合成資料上記錄；**守衛與突變的去處**：碰到的既有守衛無；新增＝J-42 系、J-43、J-46、J-49，新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型；§3 共通規則第 3 點）。
- **範囲の限界**：共通三條；v1 回滾集合由產品登記（可只含本地玩家，H.5「與今天的差距」）；K.2 第 11 條（J-42 因 Kernel 以外原因失敗＋E0 結論）→停、不追（DET 暫停）。
- **段**：上段（決定性、公開介面、DI-KS 12／13）。**模型／檔位**：實作 Opus／xhigh；審查 Fable／xhigh；驗證者 Opus／xhigh。
- **估時**：25～46 h（推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **並行**：G2a 後開工，與 match-host 並行（不同目錄；match-sim／host 不用回滾）。
- **被取代的 legacy 條文**：無直接條文（`LocalPlayerPrediction::Reconcile` 的形狀在 client-sim 取代；determinism.md §3 在 client-sim 標）。
- **風險與停止**：J-42 在某平台不等價且原因在 Kernel 外→停、報告（不放寬「逐位元」）。**若失敗**：client-sim、client-frame、plugins-editor 白做。

### 3.18 `v8-arch-engine-wrap-render`（Engine 的「包」：render、input、platform、text、audio）

- **Feature**：Client 與工具的渲染、輸入、平台、文字、音訊以 Resource 或 Host 零件接進機制（跨產品能力：object_fps_pvp Client、四工具）。
- **Contract**：render 的 Resource 親和 `GraphicsContextOwner`（spec E.3）；`PhysicalInputFrame` 是 boundary 資料（audit §1.1 input）；`PlatformEvents` boundary；audio `Submitter` 帶親和；公開介面變更列在事前宣告（AGENTS §3「Public Interface の大規模変更」）。
- **Architecture**：audit §1.1 判定照做：render 包（先收攏 4 處跨元件指標）、input **後端介面**為 Host 零件＋core 留（`SdlInput` 後端的程式碼**不在本批搬**，由 plugins-frame 搬進 sdl-window-runner；本批只定 `engine/input` 的後端介面與 core——N-12）、platform Host、text 包、audio 服務；**§4 Delta-6** 的差分（Engine 能力模組改成 Resource／Host 零件／服務；第一輪對抗 S-2）。依賴 threading：`GraphicsContextOwner` 與 audio `Submitter` 的親和（X-3、D-E10）是 threading 的原語。
- **範圍**：`engine/render`、`engine/input`、`engine/platform`、`engine/text`、`engine/audio`。
- **受理判定輪廓**：各模組既有測試全過；**守衛與突變的去處**（§5.2 第 9、16、17 列）：platform-sdl G1～G3、input IP-1 與 m7～m9 判四選一（性質仍在＝留、形狀換成 Resource／Host 零件＝改寫）；**空轉**：platform-sdl G1（CMake :38-53）、G3（:88-92）與 `tests/common/input/CMakeLists.txt:14` 以 `if(TARGET)` 掛入，target 搬走或改名時不建也不報——本批改成「該 target 應存在則必須存在」的硬性檢查（締める面，事後報告；Q11），判定：刪一個列名 target 的合成反例讓 configure 失敗；G1／G2 對**搬進 plugin 的後端**退役，理由：plugin 是第三方的對接層、其公開標頭可露 SDL 的規則改由 plugins-frame 的 plugin 標頭衛生守衛管（Q2），G2 對 `engine/**/include` 留；`gyo_expect_*_links` 只照事前宣告列出的變更（例：新增 `GYO::Kernel`），其餘不變——`gyo_audio` 的連結（`GYO::Base GYO::Time`）若因 `Submitter` 的親和要連 Kernel，改的是 audio 的連結守衛＝DI 變更，宣告列出交使用者（第二輪對抗 N2-14）；跨元件跨幀裸指標 4 處（`Renderer.cpp:134-135`、`SdlGpuRenderDevice.cpp:117`、`UiRenderer.cpp:147-149`、`ModelRenderer.cpp:22`）→0；naming 第 5 條後綴掃描新程式零命中；input 的 `<chrono>` 只在 (b) World 的程式（legacy-impact §9 第 8 點；掃描 `engine/input` 的 core 無 `<chrono>`）；render.* 在 Linux Lavapipe 綠。
- **範囲の限界**：共通三條；GPU 相關只在 Linux Lavapipe 與本機。
- **段**：上段（公開介面）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high。
- **估時**：20～55 h（audit §1.1 不調；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：platform-sdl RC 2 的例外名單改名在 plugins-editor 處理。
- **風險與停止**：包需要改 platform-sdl DI 7（observer 與 live frame 內不得做 operation）→停（Delta）。**若失敗**：plugins-frame、client-frame 的畫面側沒有零件。

### 3.19 `v8-arch-assets-core`（資產核心：`Assets<T>`、請求狀態、`AssetInbox`、套用 System、loader 登記 API；match-sim 之前）

- **Feature**：Match 的世界內容（arena、玩家動畫內容）經 Loading 的資產階段進 World——match-sim 的資產等價與 G2a 的「到 ready」都要它；Loading 頁面能顯示進度（object_fps_pvp Match 先用，Client 與四工具重用；已點名 Feature：loading 分批載入的資產階段）。這是第一輪對抗 B-4 從 wrap-assets 拆出的前半：第一稿把它排在 G2a 之後，等於關鍵路徑上的 match-sim 依賴一個還不存在的 Contract。
- **Contract**：target/assets §1、DI 1～3：請求狀態 Resource、每型別 `Assets<T>`（Handle 引用計數）、**loader 由 plugin 登記**（§1 第 3 點：本批提供登記 API，登記者是 plugin 的 build 函式，不是 host 或產品程式直接登記；loader 收位元組或 `AssetInbox` 的完成項、不收路徑）、載入是外部服務（IO 執行緒＋交換點）→`AssetInbox`（服務本身在 plugins-foundation）、完成依請求序號連續前綴套用（J-51；套用端 `StageAssets`／`ApplyStagedAssets` 在本批的 `engine/asset`，用 loading 的型別無關暫存與游標）、內容身分 (c)（determinism §4.9 digest 不變）；`WriteFileAtomically` 只從 `Io:file` 的 (b) System 呼叫；**`AssetManager` 的轉接**：`AssetManager` 物件本身**不在本批刪**（第一輪對抗 S-2：它的消費者 `apps/object_fps_pvp` 17 檔、`tools/ui_editor` 1 檔在 match-sim／match-host／client-frame／tool-ui 才遷完），本批提供 `AssetManager` → `Assets<T>` 的轉接，**期限明確：framework-delete 時刪**，轉接的每個使用處登記成清單（framework-delete 的歸零對帳表）。
- **Architecture**：§4 Delta-6 的差分（`engine/asset` 的職責移到三類 Resource、`AssetInbox` 與登記 API 的 loader）。**模組邊界**（第二輪對抗 S2-6）：`Assets<T>`、請求狀態 Resource、`AssetInbox`、`AssetStaging`／`AssetCursor` 的資產型別與 `StageAssets`／`ApplyStagedAssets` System 在 `engine/asset`，依賴 `engine/kernel`；**kernel 不知道 asset**（Delta-1 第 5 項、J-9），只提供型別無關的暫存與游標（loading）；`engine/asset` 今天在單體 `GYO::Engine` 內，所以這條邊實際是 `GYO::Engine → GYO::Kernel`（**v1 不另開 CMake target**，留在 `GYO::Engine`，守衛的連結期望與 framework／工具正向清單不動；wrap-assets 時再評估——Q3）。**只依賴 world-exec 與 loading**（`v8-arch-migration-plan-round2`，撤回第一輪修訂時的「host／產品程式直接登記 loader」）：本批只提供登記 API；登記者是 plugin——Match 側是產品 (c) plugin（match-sim，靠 plugins-foundation 的最小形）、Client 與工具側的 model loader（ufbx）由 model-assets plugin（plugins-frame 成形；match-sim 時 Match 側的 ufbx loader 先由 pvp 自己的最小 plugin 登記，S3-3）；`engine/model` 只提供 loader 函式（不登記、不連 Kernel，`gyo_expect_model_links`／animation-events DI 1 不變）；`AssetInbox` 在本批的測試與重放 host 由測試**同步餵入**（Host 在 Ready 窗口寫 external Resource，A.2）；正式的 IO 執行緒服務（交換點、X-3 親和）在 plugins-foundation 的 headless-runner／asset-io 服務（Kernel 不擁有執行緒，DI-KS 15）。
- **範圍**：`engine/asset`、`engine/io`（檔案服務包）、`engine/model`（只提供 loader 函式；不登記）；`tests/common/asset`。不碰 `engine/ui`（wrap-assets）、`apps/`（P13 載入器簽名的改寫在 match-sim）。
- **受理判定輪廓**：J-51 用真資料情境（本批的 `ApplyStagedAssets` 接 loading 的暫存與游標；本批可在 loading 完成前開工，但**結束依賴 loading**：真資料版由本批執行、記本批的 dev_log——第二輪對抗 N2-12）；loader 登記 API 的合成 plugin 測試（合成 plugin 在 build 函式登記 loader；loader 簽名不收 `std::filesystem::path`）；`AnimationContentDigest`／`ArenaContentDigest` 不變；資產等價測試（打亂完成順序，`Assets<T>` 與 Handle 序列相同，assets DI 2）；轉接的使用處清單與 `git grep -nP 'AssetManager'` 的結果件数一致（正例先命中）；既有 asset／io／model 測試全過；改名清單（target/assets §2）在宣告列出；**守衛與突變的去處**（§5.2 第 16 列）：`gyo_expect_model_links` 留（model 只提供函式）；asset／io 的既有測試留或改寫（`AssetManager` 測試在轉接期間留、framework-delete 退役）。
- **範囲の限界**：共通三條；`RenderAssets<T>`、`UiRenderer` 在 wrap-assets；`AssetManager` 在本批仍存在（經轉接），assets DI 1「無 `AssetManager*`／`&` 成員」只對本批新程式判，全 repo 歸零在 framework-delete。
- **段**：上段（公開介面、資料契約 owner）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high。
- **估時**：12～32 h（audit asset 8～20＋io 2～6＋model 1～4；自原 wrap-assets 19～48 拆出，拆法推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **並行**：world-exec 後開工，與 loading、threading 並行；結束（J-51 真資料版）等 loading；在關鍵路徑上（world-exec → assets-core → match-sim；assets-core 12～32 h ≥ loading 10～20 h，等 loading 不拉長路徑）。
- **被取代的 legacy 條文**：architecture.md L301 `AssetManager`（標記在 framework-delete 刪掉它時才生效）。
- **風險與停止**：轉接做不到「同一事實一處」（例：`AssetManager` 與 `Assets<T>` 各自持有一份內容）→停（AGENTS §9）；**若失敗**：match-sim 的資產階段沒有落點，關鍵路徑停。

### 3.19b `v8-arch-engine-wrap-assets`（`RenderAssets<T>`；UiRenderer 局部重寫；G2a 之後）

- **Feature**：資產照 Bevy 模型顯示（object_fps_pvp Client、四工具）：渲染端的 `RenderAssets<T>` 與 UI 的 GPU 資源成 Resource。
- **Contract**：target/assets 的 `RenderAssets<T>`；`UiRenderer` 的 `Gpu*` Resource（naming 第 5 條）。
- **Architecture**：§4 Delta-6 的差分（`engine/ui` 與 `engine/asset` 的渲染側）；`UiRenderer` 局部重寫為 System＋`Gpu*` Resource；`WeaponViewModel` 的跨 World 指標隨之消失。
- **範圍**：`engine/ui`、`engine/asset` 的 `RenderAssets<T>`。target/assets 不在本批搬回（隨 framework-delete，S3-5）。
- **受理判定輪廓**：守衛與突變的去處（§5.2 第 17 列）：`ui_dependency_guard.py` 判四選一；`engine/ui` 無 `AssetManager*`／`&` 成員（掃描範圍只有 `engine/`；`apps/`、`tools/` 的消費者在各自的批遷、全 repo 歸零在 framework-delete——第一輪對抗 S-2）；ui 既有測試全過；render.* 在 Linux Lavapipe 綠。
- **範囲の限界**：共通三條；GPU 相關只在 Linux Lavapipe 與本機。
- **段**：上段（公開介面）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high。
- **估時**：9～20 h（audit ui 8～18 只估 `UiRenderer`；`RenderAssets<T>` 沒有獨立出所——audit asset 8～20 拆批時全數歸 assets-core、沒有餘量——本稿計在這 8～18 內，推定；第二輪對抗 N2-4；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：architecture.md L108-132 模組表 Framework 列；ui_toolchain 第 6-16 行圖（誤導句 §5 第 22 條，文件線）。
- **風險與停止**：**若失敗**：client-frame 與工具批的畫面側資產沒有落點；停。

### 3.20 `v8-arch-plugins-frame`（視窗側的 plugins：sdl-window-runner、asset-catalog、event-sounds）

- **Feature**：Client 與四工具共用一個 SDL 視窗 runner，不再各寫 pump 與 live frame（object_fps_pvp Client、四工具）。
- **Contract**：runner plugin 在 finish 安裝（R-E6）；sdl-window-runner 啟動 plugins-foundation 的同一個 asset-io 服務（Client 不另寫）；Client 與工具的 model loader（ufbx）由 **model-assets plugin** 在 build 函式登記（target/assets §1 第 3 點；本批成形；第三輪對抗 S3-3）——它成形時取代 match-sim 用的 pvp 最小 ufbx plugin（Match 的 `project.json` plugins 欄改列 model-assets、pvp 最小 plugin 刪），**同一資產型別兩個 loader 登記是 finish 的明確錯誤**（§9）；產品 loader 讀 `Assets<ModelAsset>`，不重複解析模型；工時計在本批 15～30 的推定內（ufbx 包裝只是登記與 loader 函式的呼叫）；`PlatformEvents` boundary；catalog plan 與 event sounds 是資料契約，owner＝提供該 plugin 的人（audit §10.1；DI 照搬，owner 改）。
- **Architecture**：§4 Delta-2 的四個 plugin（audit §10.4 候選三個＋model-assets）。**`SdlInput` 後端的程式碼從 `engine/input` 搬進 sdl-window-runner**（Ownership 變更：後端歸 runner plugin，`engine/input` 只留 core 與後端介面——N-12；wrap-render 不搬）。依賴 rollback-extract：合成 Client 形狀的「開窗畫一幀」要 Extract（H.6；第一輪對抗 S-5）。
- **範圍**：`plugins/sdl_window_runner/`（`PvpApplication::ProcessEvents` 與 `SdlPlatform` 的 pump、live frame、`SdlInput` 後端程式碼、對話框回覆）、`plugins/asset_catalog/`、`plugins/event_sounds/`、`plugins/model_assets/`（登記 ufbx loader；取代並刪 pvp 的最小 ufbx plugin，改 Match 的 `project.json` plugins 欄——產品登記資料的一行）；`engine/input` 的 `SdlInput` 檔移出；framework 的對應程式**先不刪**（工具還在用）。
- **受理判定輪廓**：刪任一 plugin 只壞 audit §10.4 表列的消費者（model-assets：只壞讀模型的 Match／Client／工具）；同一資產型別登記兩個 loader 時 finish 回明確錯誤（合成反例）；pvp 的最小 ufbx plugin 引用 0、Match 的 golden 37 與 `AnimationContentDigest` 不變；event-sounds／catalog 資料契約 DI 的守衛搬到 plugin 後仍綠；一個合成 Client 形狀（Frame＋Render World）用這些 plugin 開窗畫一幀（本機）；live frame 以 run 條件表達（spec H.4.2）；`engine/input` 無 SDL include（掃描，正例先命中）；守衛與突變的去處：**新增「plugin 公開標頭衛生」守衛**——只有該第三方的 adapter plugin 可在公開標頭露其型別（sdl-window-runner 露 SDL 句柄；imgui plugin 露 ImGui，在 plugins-editor），其他 plugin 不可、Engine 標頭一律不可，例外寫在該 plugin 的 `docs/plugins/` 頁（Q2）；搬走的 SDL 後端其 platform-sdl G1／G2 退役（§5.2 第 9 列）、`m19b` 重列；fw-* 針對本批搬走的 `framework/src` 的 31 條重列（檢查者 Fable）；event-sounds／catalog 資料契約 DI 的守衛＝留（owner 隨 plugin 搬）、`SdlInput` 相關的 input 守衛隨程式搬＝改寫。
- **範囲の限界**：共通三條；framework 與 plugin 兩份 runner 並存期間＝「兩種動詞並存」（K.2 第 8 條），限定在工具遷完前、framework-delete 結束。
- **段**：上段（plugin 邊界、runner、Ownership）。**模型／檔位**：實作 Opus／high；審查 Fable／high。
- **估時**：16～32 h（audit WindowShell 16～32、Catalog 5～10 的一部分；取法推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：framework/event-sounds 的資料契約 DI 不取代（owner 隨 plugin 搬，出處：`-spec-v03` 第三輪對抗檢查的 S-8，架構師解讀見 kernel HANDOFF 2026-11-12 現況條「對抗檢查 S-8 的解讀」：廢止的是 framework 自己的結構 DI，event-sounds／preview 的資料契約 DI 隨 plugin 搬）。
- **風險與停止**：live frame（pump 重入）在 runner 下無法表達成 run 條件→停。**若失敗**：client-frame、工具批沒有視窗 runner。

### 3.21 `v8-product-client-sim`（Client 的模擬與預測跑在 kernel 上；閘門 G3）

- **Feature**：玩家的本地預測與對帳在 kernel 上，手感不變；跨產品能力完成——Match 與 Client 登記同一組 (c) System、`cFingerprint` 相同（H-4）。結束時開發者看到預測與對帳的性質在新路徑以同一劇本重現；Windows CI 的 pvp.cpu 卡住的收口在 client-link（同一 PR 線末端）。
- **Contract**：ClientSim Runtime（H.4.2）：Loading（同 Match 的 stage 序、manifest 序號同源，H.2b 第 8 點）→ ready 後以第一個權威快照 `Restore`（含成員與 `tick[Fixed]`）→ FixedUpdate（rollbackEnabled、Serial、runner＝`gyo-client-sim`）；Ingress 全部 `NotInRollback`；權威快照不走 System（Host 在交換點解碼成部分快照）；wire 不變；ready 的實現照 U14 的決定（§1.3）。
- **Architecture**：`ClientSimulationRole`／`ClientSimulation` 重寫成 ClientSim Runtime＋runner 的時鐘策略（audit P5：「何時跑下一步」是 Host）；`LocalPlayerPrediction` 重寫為回滾機制的使用者（P6）。
- **範圍**：audit §1.3 P5、P6；`tests/object_fps_pvp/`；**測試、守衛與突變的去處**（§3 共通）：`PredictionTests` 54 等引用 `ClientSimulationRole`／`LocalPlayerPrediction` 的測試逐檔列表在宣告。
- **受理判定輪廓**：(1) 產品版 J-42（PvP 的 FixedUpdate 直接跑 k tick 對 Rewind＋Replay，回滾集合逐位元相同）三平台＝**G3**；(2) `cFingerprint` Match＝Client（H-4 產品守衛）；(3) **預測與對帳的性質在新路徑以同一劇本重現**（P6 是重寫，「既有測試不改全過」在結構上達不到——第一輪對抗 S-1）：預測修正次數與最大修正量在固定劇本下與基線相同（C5 欄位）、對帳後位置與基線逐位元相同；(4) windows 觀察 (c)：本批只記錄（卡點輸出 `role=gyo-client-net／gyo-test-sim`，`gyo-client-net` 是 `ClientConnection` 的角色、由 client-link 重寫，所以在 client-link 前重跑看到的仍是舊網路角色——第一輪對抗 S-10），**收口判定在 client-link**（3.22 (5)），兩批同一宣告、同一 N；輸出含 doctest 案例名（緩衝問題一併處理）；(5) H-7 掃描 ClientSim 0；(6) D2 棘輪 Client 側（`LocalPlayerPrediction.cpp:21`、`:31` 的全域）歸 0；`kernel_prep_sites` G 類 **Client 側 7 列**（`LocalPlayerPrediction.cpp`）→0（第二輪對抗 S2-7）；(7) **BU-7** 的 Client 模擬 tick 項（§6：套 BU-1 規則對舊 Client）；(8) 測試、守衛與突變的去處表逐檔勾完——**拉回風險**：`PvpMatchTests` 的 OutageRig 四個 TEST_CASE 建構 `LocalPlayerPrediction` 並把 `MovementTrace` 當建構參數，留著就逼本批保留 `LocalPlayerPrediction` 門面；處置＝以新 Runtime（ClientSim＋回滾）同一劇本重寫，`mr-*`／`ci-*`／`csc-*` 中針對預測側的突變重列（檢查者 Fable）（§5.2 第 19 列、Q5）；(9) -zones 交遷移批的 **D 3 處**（`ClientSimulation.hpp:7` `<chrono>`、`PredictionElapsedTime.hpp:3` `<chrono>`、`:12` `steady_clock`）→0（Host 時鐘移到 runner 的時鐘策略）；**E 14 處**：本批只消 `LocalPlayerPrediction` 內的呼叫點（改句柄）；`ClientConnection.cpp:714、803` 在 client-link、`PvpApplication.cpp:1369` 與 `main.cpp:157` 在 client-frame 改句柄，PR 線末端（**client-frame**）刪全域與寫檔執行緒、14→0（第三輪對抗 S3-1）；`zone-c-rewrite.json` 的 P5／P6 列移除（P1 已在 match-host 移除；清單空則刪檔，AGENTS §7 不留空；`test_zone_c_lists.py` 釘住的成員同步改，§14.4）；新增與改寫守衛的突變一覽敵對檢查者 Fable（與作者不同、不是小模型）。
- **範囲の限界**：共通三條；回滾集合 v1 只登記本地玩家的欄；DET 暫停、win1 的 ULP 差異不在本批；既有 wire 不夠表達 manifest→停（§8 RC-9；plan-dryrun 的查證 N-11 先給答案）；windows 觀察 (c) 的收口不在本批。
- **段**：上段（決定性、回滾、執行緒、回歸風險）。**模型／檔位**：實作 Opus／xhigh；審查 Fable／xhigh；驗證者 Opus／high（預測測試判定改寫時）。
- **估時**：59～118 h（audit P5 20～40＋P6 30～60；＋測試、守衛與突變的去處 8～16（推定）；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：determinism.md §3（`Reconcile` 只重放本地玩家）→ target 回滾機制；network-architecture 第 180-181 行（Client 角色決定執行緒存活 → runner）。
- **風險與停止**：W-3 的 timeout 在新 runner 仍出現→§14.1 切分，切不出就停，不靠加 TIMEOUT 通過（§14.4）；Client 與 Match 的指紋不同而要在 System 內分支產品角色→停（違 DI-KS 15）。**若失敗（G3）**：client-frame 不合併（它依賴 ClientSim 的呈現 mailbox）。

### 3.22 `v8-product-client-link`（ClientConnection → net-link）

- **Feature**：PvP Client 照舊連線（玩家可見：無變化）；模擬執行緒不再直接呼叫 `SendInput`。
- **Contract**：Link 家族（net-link）；asio 的 IO 執行緒只處理位元組與到達時刻；Running 期間零外部存取；wire 不變。
- **Architecture**：`ClientConnection` 重寫為 net-link 的 Client 端服務＋Link；狀態→Resource（audit P4）；訊息種類判別與 proto 解碼在產品側 adapter（第一輪對抗 S-11）。
- **範圍**：audit §1.3 P4；`ClientConnection.cpp:714、803` 的 `TraceMovement` 改經句柄（E 14 處的 Client 呼叫點之一，S3-1）；**測試、守衛與突變的去處**（network 標籤測試與 `ClientConnection` 的單元測試，逐檔列表在宣告）。
- **受理判定輪廓**：(1) Link 家族「Running 期間零外部存取」測試（交換點計數）；(2) network 標籤測試本機全過；(3) `client_delay_runner` 等既有 runner 的輸出欄位不變；(4) IO 執行緒不碰 World（掃描＋審查）；(5) **windows 觀察 (c) 收口**（第一輪對抗 S-10；與 client-sim 同一宣告，在兩批都合進 PR 線後做）：**先算基準發生率**——修改前**必做** N₀ 次 workflow_dispatch 當基線（N₀ 在宣告中定、看結果前固定），配合 `build/evidence/ci-windows-cpu-flaky/tally.md` 的同一方法算出修改前 `object_fps_pvp.cpu` windows-x64 每 run 的逾時率 p，再定 N 使「沒有改善卻 0 次逾時」的機率 (1−p)^N ≤ 5%；**N 上限 60**（與 closeout 重掃同量級）：算出的 N >60、或 N₀ 次 0 逾時而 p 無法估時，不做 dispatch 收口，(c) 改以 closeout 的 60 run 統計比較判定，並把「未能以 dispatch 證明」寫進範囲の限界（第二輪對抗 N2-9：一次乾淨的 run 給不出 p 的下界）；修改後以同一 N 次 workflow_dispatch 重跑 0 Timeout，任何一次停在 `role started … wait_backend=` 之後＝不成立；前後用同一個宣告比較（AGENTS §14.3）；(6) 測試、守衛與突變的去處表逐檔勾完（新增的 Link 測試其突變一覽由與作者不同、不是小模型的檢查者敵對檢查，預設 Fable）；(7) `ClientConnection.cpp:714、803` 不再呼叫診斷全域（掃描，正例先命中）。
- **範囲の限界**：共通三條；network 測試只在本機；N 次看不到低於 1/N 的偶發（closeout 60 run 比較）。
- **段**：上段（wire 邊界的執行緒、外部服務）。**模型／檔位**：實作 Opus／high（執行緒交換 xhigh）；審查 Fable／high；驗證者 Opus／high。
- **估時**：27～50 h（audit P4 30～50 扣 net-link；＋測試、守衛與突變的去處 4～8（推定）；＋守衛去處判斷 1～2（推定，§3 共通））。
- **並行**：與 client-sim 並行需要第二張產品卡（U11）；不開時接在 client-sim 後，關鍵路徑加 27～50 h。
- **被取代的 legacy 條文**：network-architecture §7 入口表（誤導句 §5 第 19 條，表頭加註）。
- **風險與停止**：**若失敗**：client-frame 的 Link swap 沒有實作；停。

### 3.23 `v8-product-client-frame`（Client 的畫面與輸入；G3 後與 client-sim、client-link 同一 PR 線合併）

- **Feature**：玩家看到的畫面、輸入、UI、命中回饋、音效在 Frame／Render World 上，與今天一致；loading 畫面在 ready 前顯示（H.2b 的 Feature 開始成立）；Editor 與 Client 同形（runtime-composition §3b 第 4 點）。結束時＝M5：`PvpApplication` 只剩 plugins 資料＋Run。
- **Contract**：ClientFrame（FrameUpdate，runner＝Main；`PlatformEvents` boundary、`FrameTime` external）、ClientRender（Extract→RenderSubmit，親和 `GraphicsContextOwner`）；Mailbox 意圖 F→S、呈現 S→F 只在 schedule 之間存取（H.4.2）；live frame 時 `Input`／`Intent` 被 run 條件跳過。
- **Architecture**：`PvpApplication` 重寫成 plugins 資料＋Run＋產品 (b) System（P11）；P10 呈現包（時鐘由 `FrameTime` 傳入）；`src/App` 的 `WeaponViewModel` 先拆再包；呈現 System 放模擬之後的 stage（LIM-1 慣例）。
- **範圍**：audit §1.3 P10、P11、`src/App`、S1（Client 半）、Client `main.cpp`；Client 側 `AssetManager` 使用改走 `Assets<T>`／`RenderAssets<T>`；Sim 外讀檔入口的 Client 使用處（`PvpApplication.cpp:963` 的 `LoadInstalledArenas` 等）改經 `Assets<T>`、入口刪（S3-2）；診斷全域與寫檔執行緒（`MovementDiag.hpp`／`MovementTrace.hpp`／`MovementTraceWriter.hpp`）刪除（PR 線末端，S3-1）；**測試、守衛與突變的去處**（App 與呈現層測試，逐檔列表在宣告）。
- **受理判定輪廓**：(1) H-6／J-43 產品版（呈現不寫 S、R 不寫 F）；(2) 呈現 System 的 D-E8＝0 且 LIM-1 成本記錄（呈現 System 數、各在哪個 stage）；(3) D2 棘輪 Client 側 0；H-7 掃描 Client host 0；(4) client 驗收（acceptance）全過；(5) **Windows 實機一場**（CI 做不到的視覺部分；memory ci-is-primary-standard、offline-session-scripts）；(6) **BU-7**（Client 幀；數字與基線量法在 §6）；(7) `native_window_evidence`、`action_runner` 在 windows-x64 L1 的逾時次數只記錄、不作門檻（觀察 (2) 的疑似項）；(8) `apps/object_fps_pvp` 無 `AssetManager` 引用（轉接也不留；framework-delete 的對帳表 Client 列歸零）；(9) 測試、守衛與突變的去處表逐檔勾完；(10) Client 側最後的呼叫點（`PvpApplication.cpp:1369` 的 `TraceMovement`、`main.cpp:157` 的 `MovementDiagFile`）改句柄後刪全域與寫檔執行緒：**-zones 的 E 14 處→0**（逐處對帳；掃描正例先命中）；(11) Sim 外讀檔入口的使用處 0、入口檔刪（S3-2）；(12) 守衛與突變的去處另含 -zones kz-* 突變中針對 Client 側檔案（Present／App、`ClientConnection.cpp` 的換算等）的部分判四選一（a6-revision 區表修正列出的清單）。
- **範囲の限界**：共通三條；**LIM-1**（(b) 讀 (c) 要顯式順序，成本在呈現 System 數；本批的呈現 System 都放模擬之後的 stage）；macos-x64 偶發只記錄（memory platform-priority）。
- **段**：上段（Client host 骨架＝runner plugin、多 World 組合、回歸）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high（若畫面相關判定要改）。
- **估時**：66～132 h（audit P11 40～80＋P10 10～20＋App 6～12＋S1 半 5～10；＋測試、守衛與突變的去處 4～8（推定）；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：architecture.md L138 的 Client 側；platform-sdl RC 2（例外名單改成 imgui plugin target——在 plugins-editor 做）。
- **風險與停止**：BU-7 ≥1.5 倍→§8 RC-1；live frame 期間需要跑 Intent 才能保持今天行為→停（spec H.4.2）。**若失敗**：Client 的 PR 線（client-sim、client-link、client-frame）不能合併、留在分支；framework-delete、runtime-loop-retire、closeout 不開。

### 3.24 `v8-arch-plugins-editor`（Editor 側的 plugins）

- **Feature**：四個工具的視窗、ImGui、預覽、選擇窗照舊（工具使用者可見：無變化；預覽不再晚一幀、無 readback，`v8-arch-editor-forward-only`）；四工具的選擇窗記帳與關窗提示只有一份（架構師保留：必須真的進共用 plugin）。
- **Contract**：imgui plugin 兩半（文件 World 的輸入與 widget System；渲染 World 的 draw list 繪製；ImGui context 全域明列例外 I-5）；offscreen-preview 在渲染 World 合成、預覽視窗＝draw list 的貼圖 id（runtime-composition §3b）；editor-dialogs（三種記帳統一、關窗提示；行為差異寫進宣告，§1.3）。
- **Architecture**：§4 Delta-2 的延續；preview 資料契約 DI 1～3、5 照搬、DI 4 已被取代。
- **範圍**：`plugins/imgui/`、`plugins/editor_dialogs/`、`plugins/offscreen_preview/`；framework `ImGuiShell`、`OffscreenPreview`、`PreviewCamera`／`PreviewTexture`／`UiTextInput`／json binding（成函式庫，放哪個 target 本批定）；`tests/framework/PreviewTests.cpp` PV2～PV5 與 pv-* 突變**重寫不搬**（衝突 2 處置）。
- **受理判定輪廓**：H-6 原樣（無例外、無晚一幀）；合成 Editor 組合的 Extract 測試；每個 plugin 的刪除驗證（刪它只壞依賴它的工具）；editor-dialogs 被 ≥2 個工具使用、三種記帳統一後四工具的對話框測試全過；重寫的 PreviewTests 以突變證明檢出力（§14.4 驗證者）；**守衛與突變的去處**（§5.2 第 10 列）：**拉回風險**——PV2（`EndFrame` 交出本幀 readback）與 PV5（`UploadFrame` 貼圖）的判定**就是 readback 形狀**，搬進 plugin 不改判定就把「每幀 readback 再上傳」帶進渲染 World；處置＝這兩條退役（接手：H-6／J-43 的 Extract 測試＋本批「預覽無 readback」掃描），PV1／PV3／PV4／PV6 改寫進 offscreen-preview plugin 的測試；fw-* 針對本批搬走的 `framework/src` 的 10 條重列；imgui plugin 的公開標頭可露 ImGui（Q2，寫在其 `docs/plugins/` 頁）。PV2～PV5 中守 readback 的判定與 pv-* 突變＝退役（preview DI 4 已被取代，§14.4 寫 1～5、DI 守衛交使用者），其餘性質＝改寫（無 readback 的新形狀），改寫的突變一覽由與作者不同的模型敵對檢查；三平台綠。
- **範囲の限界**：共通三條；model_viewer `ViewerWindow.cpp:112` 缺陷不修（統一記帳若順手修好＝行為變更，宣告寫明交使用者）；預覽視覺只在本機與 Lavapipe。
- **段**：上段（plugin 邊界、資料契約 owner 移動）。**模型／檔位**：實作 Opus／high；審查 Fable／high；驗證者 Opus／high（PreviewTests 判定改寫屬 §14.4，有緩める面就要使用者核准）。
- **估時**：21～42 h（audit framework 33～66 扣 plugins-frame 已做的 runner 與 catalog；扣法推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：preview.md DI 4（衝突 2）、RC 4；model_viewer RC 1；platform-sdl RC 2；framework/architecture DI 1～6、RC 全部（隨 framework-delete 刪檔）。
- **風險與停止**：選擇窗記帳三種實作語義不同、無法統一→停、報告；不在 plugin 內放三份。**若失敗**：四個工具批沒有 Editor 骨架。

### 3.25～3.28 工具四批（`v8-product-tool-voxel`、`-tool-model-viewer`、`-tool-audio`、`-tool-ui`）

共通（每批一節的欄位，差異列在表）：
- **Feature**：該工具的編輯功能照舊（工具使用者可見：無變化）；voxel 是 Editor 形狀（文件 World＋渲染 World）的第一個樣本（README「-audit 的起點」），結束時工具使用者用新版編一張圖、存檔照 `gyo.voxel_map`、Match 讀得進。
- **Contract**：工具的 `project.json` 加 plugins 欄、main＝Run；工具專有的編輯 System 進該工具自己的 plugin；文件 World（兼 Frame）＋渲染 World；存檔載入是 `Io:file` 的 (b) System、經各自資料契約（`gyo.voxel_map` 等，AGENTS §8，不變）；工具守衛正向清單改掛 plugin target（legacy-impact §4 第 11 列）。
- **Architecture**：應用 Architecture（工具內部；AGENTS §2「応用」）——Resource 與 System 拆法依觀測，不預先一般化；形狀由 voxel 定、後三個照做。
- **受理判定輪廓**：工具既有測試（移植後）與依賴守衛全過；main 只有 Run（掃描）；H-7 掃描對工具 host 0（各工具自己的基線測試，h7-scanner 批註冊）；預覽無 readback；**取代的 Design Invariants**（表列）照 AGENTS §3／§14.4：守衛測試的改寫寫 1～5、驗證者複核、交使用者核准；**測試、守衛與突變的去處**表逐檔勾完——**拉回風險**：工具正向清單不刪 `ToolFramework*` 項，工具就能同時連 framework 與 plugin（半遷移），不加 `GYO::Kernel`／plugin target 就只能留在 framework；處置＝該工具批刪 framework 項、加 plugin 項、性質改為「組合只經 plugin」，每條新邊照 §14.4 交使用者（§3 共通；§5.2 第 11 列：該工具的正向清單改掛 plugin target＝改寫，突變一覽由與作者不同的模型敵對檢查）；tool-ui 另：`tools/ui_editor` 無 `AssetManager` 引用（framework-delete 對帳表 ui_editor 列歸零）。
- **範囲の限界**：共通三條；工具全 zone=B。
- **段**：**四批全部上段**（第一輪對抗 S-12：每批取代該工具的 Design Invariants、改 Product Registration 的資料格式，照 navigator〈上段と下段〉是上段；三案把後三個放下段是誤判——見 §2 取捨）。**模型／檔位**：實作 Opus／high；審查 Fable／high（全部四批；gearbox「code review 是 high」）；上段中的 low 定型作業（檔案搬移、CMake 清單更新、`project.json` 加欄）可用 Sonnet／low（navigator：上段のバッチの中の low の定型作業、計畫寫明）；驗證者 Opus／high（DI 守衛改寫時）。ui_editor 若要改 `GYO::Ui` 介面→停下重排（已是上段，不是「升段」）。
- **風險與停止**：同一工具出現第二套組合規矩（framework 老問題換名）→停，回 plugins-editor。三工具三卡並行。**若失敗**：任一工具失敗→framework-delete 不開（framework 的最後消費者仍在），runtime-loop-retire、closeout 隨之不開；tool-voxel 失敗時後三個工具照它的形狀，也白做。

| 代號 | 範圍（audit §1.2） | 差異 | 估時（audit §1.2＋測試、守衛與突變的去處 2～4 推定＋守衛去處判斷 1～2 推定） | 被取代的 legacy 條文 |
|---|---|---|---|---|
| `v8-product-tool-voxel` | client Host 重寫；`EditSession`、`MapPreview`、`PreviewScene` 包 | 第一個樣本；W6／W7 守衛的觀測手段在 plugin 版重寫（legacy-impact §9 第 8 點）；voxel 守衛的 core／window 結構規則**從 target 的文件 World／渲染 World 重導（改寫）**，不保留舊的兩個 library 切法（Q12） | 24～48＋2～4＋1～2＝27～54 | voxel_map_editor DI 1；design.zh-Hant 四處措辭 |
| `v8-product-tool-model-viewer` | client Host；預覽與音訊包 | `pendingAction_` 跨幀捕獲 `this` 改成資料 | 16～32＋2～4＋1～2＝19～38 | model_viewer DI 1、2、4；RC 1 |
| `v8-product-tool-audio` | client Host；`PreviewPlayer` 包 | — | 8～16＋2～4＋1～2＝11～22 | audio_editor DI 1 |
| `v8-product-tool-ui` | client Host；`EditorApp` 重寫（介面）；`AssetManager` 使用改走 `Assets<T>` | `EditorApp` 存兄弟物件參考 | 32～56＋2～4＋1～2＝35～62 | ui_toolchain ui_editor DI 1～3 |

### 3.29 `v8-arch-framework-delete`

- **Feature**：framework 概念退役完成（`v8-arch-framework-retire`）；AGENTS §7 完整刪除；Product removability 的第二次實測。使用者：開發者。
- **Contract**：無新契約（資料契約已在 plugins-frame／editor 搬走）；刪 `FRAMEWORK` component（build 層資料）。
- **Architecture**：§4 Delta-2 的收尾。
- **範圍**：`framework/`、`docs/framework/` 三份（同一 PR，`v8-arch-docs-legacy`）、`tests/framework/`、`GyoComponents.cmake` 的 `FRAMEWORK`、`GyoBuild.cmake` 的 `GYO_ENABLE_FRAMEWORK`、`ToolRegistryTests.cmake` 的保留名、`framework_dependency_guard.py`（legacy-impact §4 第 10～13 列）；target/assets 與 target/runtime-composition 搬回 `docs/architecture/`（assets DI 1 此時對全 repo 成立；S3-5）；**`AssetManager` 與 assets-core 的轉接**（最後消費者 tool-ui／client-frame 都在本批之前；第一輪對抗 S-2）。
- **受理判定輪廓**：**搬回**（S3-5）：target/assets 與 target/runtime-composition 搬回後同名並存 0、對應 legacy 內容（framework 三份在本批刪；architecture.md 中被取代的段落加「已被取代」註、整檔在 closeout 刪）入鏈 0 斷（反向依賴同法重掃）、assets DI 1 對全 repo 成立；-remove-object-fps 五條判定同法（刪檔逐檔相同、共通層只動已列的檔、殘留 grep 用 `git grep -P`／Python 不丟 stderr、空目錄 0、ctest 名單差異只有刪掉的）；共通層改動逐條列（預期非零：build 層的 FRAMEWORK 是 Delta-2 影響檔）；**`AssetManager` 全 repo 引用 0**（掃描範圍 `engine/`、`plugins/`、`apps/`、`tools/`；assets-core 的轉接使用處對帳表全部歸零；正例先命中；assets DI 1 至此對全 repo 成立）。
- **範囲の限界**：無新增。
- **段**：上段（Build Graph、Product Registration 保留名）。**模型／檔位**：實作 Opus／high（第一輪對抗 S-13：先例 -remove-object-fps 是 Opus／high 且因掃描錯誤停過一次；本批範圍大數倍、含 Delta-2 影響檔的共通層改動）——其中逐字作業（刪目錄、CMake 清單的既定行）是 low 定義，可在宣告列出用 low；審查 Fable／high。
- **估時**：5～10 h（-remove-object-fps 實作 27 分為下界參照；本批範圍更大：推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：framework 三份全文（刪檔）；AGENTS §2 L96「framework の共通構造」（交風紀委員改「plugins 層の共通構造」）；cross-component-rules 定義句（誤導句第 33 條）。
- **風險與停止**：共通層要改的檔超出宣告清單→停、記 Delta。**若失敗**：runtime-loop-retire 與 closeout 不開；`AssetManager` 與轉接、framework 留著（M7 不成立）。**守衛與突變的去處**（§5.2 第 10、12、13 列）：**空轉**——`framework/` 刪掉後 `framework_dependency_guard.py` 的 `rglob` 回空、守衛空轉通過，而 `tests/framework` 的 CMake link check 對不存在的目錄 configure 失敗，所以 **`tests/framework` 與 `framework/` 同批刪**（判定：刪後 ctest 名單無 `gyo_framework.*`、configure 綠）；`framework_dependency_guard.py`、link check、fw-* 突變、`FRAMEWORK` component、`GYO_ENABLE_FRAMEWORK`、`ToolRegistryTests` 保留名＝退役（性質只在 framework legacy DI；§14.4 寫 1～5，DI 守衛交使用者核准）；`AssetManager` 的測試與突變退役並記錄。

### 3.30 `v8-arch-ci-spike-retire`

- **Feature**：CI 只有一套 kernel 證據（ci-readiness §2.6）。使用者：開發者。
- **Contract**：無。**Architecture**：刪 top-level `spikes/`（AGENTS §7 不留空目錄）。
- **條件**（兩者都要）：(a) registry-schedule 的 J-12 與 world-exec 的 J-5 兩層在 L1 三平台綠（N-1：J-12 在 registry-schedule）；(b) pageguard-os 完成（`PageGuardTests.cpp` 已搬）或 G2a 後 U1 定不採（U10 的時點隨 U1）。只滿足 (a) 時 spike 步驟仍是頁保護三平台證據的唯一來源，不能刪。
- **範圍**：`spikes/kernel/` 整個目錄；`.github/workflows/cross-platform.yml` 的步驟本體、上傳路徑兩行、摘要列與 env（**由使用者改**）。報告與 dev_log 的歷史路徑不改寫，README 加一句「已於 PR # 移除」；**U10 已定**：刪除時在 target 的 A.6／驗證節與兩份 spike 報告開頭留 git 永久連結 `https://github.com/yojinn-io/GYO-Engine/tree/<刪除前 sha>/spikes/kernel`（sha 用刪除前最後一個含 `spikes/kernel` 的 commit），判定：連結可開、指向的樹含 `spikes/kernel`。
- **受理判定輪廓**：兩條件逐條勾；`spikes/kernel` 引用 0（歷史 dev_log 除外）；L1 綠。
- **段**：上段（CI 結構）。**模型／檔位**：實作 Opus／low（逐字刪除，low 定義）；審查 Fable／high（第一輪對抗 S-13）。
- **估時**：2～4 h（推定）。
- **被取代的 legacy 條文**：無。
- **範囲の限界**：`.github/` 由使用者改，本批只驗 `spikes/kernel` 引用 0 與 L1 綠；歷史 dev_log 的路徑不改寫（懸空是預期）；頁保護的三平台證據在 U1 不採時隨 spike 消失（LIM-3 的 L1 維持）。
- **風險與停止**：無。**若失敗**：不擋任何批（spike 步驟與 `spikes/` 多留一段，U10）。守衛與突變的去處：spike 的測試已在 registry-schedule／world-exec／pageguard-os 搬改，本批只刪殘留。

### 3.31 `v8-arch-runtime-loop-retire`

- **Feature**：舊的三段呼叫序在 repo 中消失，任何 host 都不能「一半在機制上」（H-7）。使用者：開發者。
- **Contract**：無。**Architecture**：audit §1.1 runtime 列的重寫收尾；`FixedTickRuntime` 留給 runner plugin（Host 零件）。
- **範圍**：`engine/runtime` 的 `RuntimeLoop`、`IRuntimeClient` 與測試；target 的 world／system-contract／scheduler／determinism／error-handling 與 **kernel DI 頁**搬回 `docs/architecture/`（這些部件的最後一批，S3-5；kernel DI 頁選本批而不是 closeout 的理由：Kernel 相關的最後一次程式變更是本批——`RuntimeLoop` 刪掉後 H-7 的雙軌結束，之後沒有批再改 Kernel 的條文；target/limits 留 closeout，因 LIM 的狀態要等全部批完成才定）；對應 legacy 檔的刪除見判定。
- **受理判定輪廓**：**搬回**（S3-5）：target 的 world／system-contract／scheduler／determinism／error-handling 與 kernel DI 頁搬回後，對應的 legacy 檔（entity-store.md、error-handling.md；determinism.md 只在 §7／E0～E5 的去處已有使用者決定時刪，否則留到 closeout）刪除、入鏈 0 斷（legacy-impact §6 的反向依賴同法重掃）、同名並存 0；include 0；H-7 守衛 (a) 改為「檔案不存在」也過；`engine_tests` 案例數只減被刪的部分（逐案例對照；刪前看每個 CHECK，-remove-object-fps S1 教訓）。
- **段**：上段（公開介面刪除）。**模型／檔位**：實作 Opus／high；審查 Fable／high。
- **估時**：7～17 h（audit runtime 8～20，只刪不寫：下修推定；＋守衛去處判斷 1～2（推定，§3 共通））。
- **被取代的 legacy 條文**：architecture.md L138（`RuntimeLoop` 三段）——此時對任何程式都不再成立。
- **範囲の限界**：不寫 System（共通三條不適用）；只刪不寫——`FixedTickRuntime` 的行為不在本批驗（runner plugin 的測試守）。
- **風險與停止**：仍有使用者（工具或 Client 未遷完）→不開。**若失敗**：closeout 的「`RuntimeLoop` 引用 0」不成立，closeout 不開。守衛與突變的去處：H-7 守衛 (a) 改寫（檔案不存在也過）；針對 `RuntimeLoop` 的測試與突變退役並記錄。

### 3.32 `v8-arch-migration-closeout`

- **Feature**：所有人看到一套架構文件；舊概念（framework、RuntimeLoop、IRuntimeClient）不存在；windows 觀察結案或重開有依據。結束時＝M7。
- **Contract**：`gyo.zone-c-sources` 由 Kernel 匯出接手後，v1 是否停用（zone-c-sources §4）。
- **Architecture**：不變。
- **範圍**：legacy 九檔中各部件最後一批未刪者在本批刪（含各檔的 `Design Invariants` 節——節的刪除是 AGENTS §3 要使用者同意的 Delta，逐檔列在宣告）、target 搬回 `docs/architecture/`（target/README 命名規則；每部件的搬回**隨該部件最後一批**做，動作與判定寫在那一批：world／system-contract／scheduler／determinism／error-handling 與 kernel DI 頁隨 runtime-loop-retire（§3.31）；**assets 與 runtime-composition 隨 framework-delete**（§3.29；`AssetManager` 刪除那批，assets DI 1 在搬回時對全 repo 成立——第三輪對抗 S3-5，取代第二輪的「隨 wrap-assets＋附註」）；本批只做最終掃描、剩下的 legacy 刪除，以及 naming／zone-c-sources／limits／README 的搬回或收掉——這幾個是跨部件的規則或清單，最後一批就是本批）；**legacy determinism §7 缺口清單與 E0～E5 的去處在刪 legacy 前定**（第一輪對抗 S-6：a6-revision 不搬它們進 target；選項：留在 HANDOFF〈暫停〉節、或隨 DET-0～3 重開時進 target；交使用者）；LIM 逐條搬或標 Lifted；Status 行刪；Windows CI 以 `build/evidence/ci-windows-cpu-flaky/tally.md` 的同一方法掃最近 60 個 run 比較（HANDOFF〈觀察〉處置）；macOS Quick acceptance 被 cancelled（觀察 (4)）一併列、不追；**`fps::pvp::sim` 改成非 inline**（-zones 的 v1 inline namespace；此時舊 Client 已刪、工具已遷，沒有以 `fps::pvp::X` 拼法引用 (c) 型別的舊程式；架構師 2026-11-12 定），改名清單在宣告列出。
- **受理判定輪廓**：同名並存 0；`RuntimeLoop` 引用 0；**改非 inline**（S3-7）：`inline namespace sim` 在 `apps/object_fps_pvp`、`tests/object_fps_pvp` 0 命中（正例先命中）、全建置與 object_fps_pvp 的 ctest 全過（L1 三平台）、golden 37 與 `AuthorityDigest`／`AnimationContentDigest` 零差；target/determinism 照搬區表內的 E-n 引用在刪 legacy §7 後不懸空（改註「待 DET-0，暫停」或指向 HANDOFF〈暫停〉，第三輪對抗 N3-8）；反向依賴 91 處（legacy-impact §6）0 斷鏈；LIM 狀態與資料夾一致；重掃比較表；determinism §7／E0～E5 的去處有使用者決定行；**spike 刪除處的永久連結存在且有效**（U10：target A.6／驗證節與兩份 spike 報告開頭的引用逐處開啟確認）；HANDOFF「未結事項」節（含 U14）沒有被刪；圖書管理員 checkup 項全勾；**A10 歸零**：§5.2 的 21 列每列有去處與落點批，「性質只來自 legacy 檔的守衛」＝0，每個「留」的守衛其 DI 在 target 或現況檔有條號（§5.2「DI 落點」欄逐列核對，第三輪對抗 S3-6；第 21 列的註解引用隨 architecture.md 刪除改指 target）。
- **範囲の限界**：無新增。
- **段**：**上段**（第一輪對抗 S-12：刪 legacy 的 DI 節、把 target 的 DI 搬回是 AGENTS §3「節そのものの変更・削除」的 Delta）。**模型／檔位**：實作 Opus／medium（文件搬移與掃描，medium 是外部驗證（圖書管理員 checkup）存在時的下限）；**改非 inline 一步 Opus／high**（全產品多檔修改＝gearbox high；產品程式的 owner 是 object_fps_pvp，執行者是架構師開的子代理，同 kernel-prep 卡）；審查 Fable／high（與實作者 Opus 不同）；圖書管理員 checkup 是外部驗證、不是審查者；重掃計數由實作者（Opus／medium）做——它是 medium 定義的作業，上段批只有 low 定型作業可用小模型（第二輪對抗 N2-10）。
- **估時**：13～26 h（推定；＋守衛去處判斷 1～2（推定，§3 共通）；＋改非 inline 2～4（推定，S3-7；守衛去處判斷已含在前））。
- **被取代的 legacy 條文**：legacy 九檔刪除。
- **風險與停止**：重掃發現新的 Windows 專屬失敗型→不在收尾修，記觀察、交使用者。**若失敗**：沒有後續批；legacy 與 target 並存延續，M7 不成立，交使用者。

### 取捨（§3）

- **Life 的寫者形狀**：價值案建議「`ApplyLife` 一個 System 兩個掛點（復活在 Simulate 開頭、死亡在 `ResolveActions` 之後）」，成本案寫「ApplyLife 同時處理復活與死亡兩個 Plan 位置」——兩者都是**同一 schedule 掛兩次，spec B.1 明文是啟動錯誤 D-E19**（`ApplyMembership` 的兩個掛點之所以合法，是因為在 Loading 與 FixedUpdate 兩個不同 schedule）。本稿採風險案：形狀交 dryrun 以 finish 結果定（判準是 Plan 上的謂詞，§2.1 G1 (iii)；T／R 數值在 match-sim 實測），**使用者 2026-11-13 已定（U6）**：`LifeState` 單一寫者在 tick 開頭＋`DeathEvent` 事件由 `ResolveActions` 發（同 tick 內讀、spec F 生命週期、同步點清掉、回滾重放時重發不另存）；不要中間狀態欄（同步負擔）；「事件＋下一 tick 套用」排除（改 golden）。原候選（中間欄、`DeathRecord` 欄）不再採用。
- **Windows 觀察的驗收數字**：風險案 N＝100／200 在測試內重複、價值與成本案 workflow_dispatch 重跑 10 次。本稿兩者都用：測試內重複（便宜、一次 CI 就有）當批內門檻；CI 重跑只用在觀察 (c)（它的卡點在整個測試執行檔的啟動路徑，測試內重複看不到），**次數 N 不寫死 10**——第一輪對抗 S-10：基準發生率不明時 10 次幾乎沒有檢出力，所以先用 `tally.md` 的方法算基準發生率再定 N（「無改善卻 0 次逾時」≤5%），收口點放在 client-sim＋client-link 都完成的 PR 線末端（卡住的角色 `gyo-client-net` 由 client-link 重寫）；closeout 以 60 run 統計比較。
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
6. **Ownership**：組合格式（catalog plan、event sounds）的資料契約 owner＝提供該 plugin 的人；runner（H.7 職責）由 runner plugin 持有；葉子函式庫與層無關。**Product Ownership（第一輪對抗 B-3 補）**：診斷 sink 的服務化與檔案生命期契約的 owner＝trace-diagnostics plugin（通用，型別無關）；`MovementDiag`／`MovementTrace` 的記錄型別、欄位語義與 PvP 的診斷邏輯 owner **仍是 object_fps_pvp**（v8-product-pvp-diagnostics），只經通用 sink 輸出——plugins 層不收任何產品型別。net-link 同理：Link 的 Resource 型別、swap 與 IO 殼 owner＝net-link plugin，訊息種類判別與 proto 解碼 owner＝產品（第一輪對抗 S-11）。資產的 IO 執行緒服務（`AssetInbox` 的交換點餵入）owner＝plugins-foundation 的 headless-runner／asset-io 服務（Kernel 不擁有執行緒，DI-KS 15；第二輪 `v8-arch-migration-plan-round2`）。`SdlInput` 後端：owner 從 `engine/input` 移到 sdl-window-runner plugin（N-12）。
7. **更小的方案**：改 framework DI 2／6、重寫兩個 shell（audit 原提案）——使用者不採（「思想不變，修改就是在破船上打補丁」）。
- 各批差分：plugins-foundation（top-level＋headless-runner＋asset-io 服務）、net-link、diagnostics-service、plugins-frame（含 `SdlInput` 搬入、model-assets plugin）、plugins-editor、framework-delete；pvp-diagnostics 是產品側的對應批（不是 Delta-2 的差分，列在這裡只為對照 Ownership）。

### Delta-3：entity_store 在 World 之下（使用者已同意，K.3 第 7 項第 (2) 點）

1. **Feature**：World 的欄依 id 升冪的疊代順序（決定性 (c) 的 World 契約，PvP digest；DI-KS 3），沿用已驗證的演算法。
2. **問題**：World 要 keys／值／stamps 平行陣列同步搬動，`EntityColumns` 把鍵放在元素內、沒有回傳位置的查找（spec A.6）。
3. **邊界**：`engine/entity_store` 成為 World 欄的實作；DI 4 延伸到 insert／remove；DI 5 對 store 本身仍成立；`DeferredChanges`（3b）撤回；stamp 陣列放 kernel 不放 store（DI 6 對 store 仍成立；處置決定行）。
4. **影響**：`engine/entity_store`（回傳位置版本或 `Cell<T>`，二選一）、`tests/common/entity_store`（守衛 W1 字表含 `Player`／`Match`，所以 World 不放進 entity_store 目錄）、`engine/kernel`。
5. **Dependency**：新邊 `Kernel → EntityStore`（上層用下層）；store 不知道 kernel。
6. **Ownership**：排序演算法仍歸 entity_store；成員關係、延後套用、stamp、同步點的擁有者歸 World（entity-store.md 圖的誤導句第 6 條同步）。
7. **更小的方案**：World 自寫一套排序欄——重複同一演算法與守衛（AGENTS §2 Valid evidence 第 1 條），不採；借 EnTT——swap-and-pop 違 W-2（spec M），在 world-exec 宣告以 W-1～W-11 核對後定。
- 差分：world-exec 事前宣告選定兩種做法之一。

### Delta-4：OS 層頁保護平台層（**條件**：G2a 後 U1 決定採第三層；os.md RC 9 的出口是 Feature）

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
4. **影響**：`build/cmake`（`GyoApps.cmake`／`GyoComponents.cmake` 讀 `plugins` 欄、產生登記）、六份 `project.json`（object_fps_pvp、四工具、`tests/common/fixtures/copy_game/app/project.json` 的 Product 複製夾具）、`ToolRegistryTests`、產品 main（變薄）。
5. **Dependency**：不變（共通 build 只解讀登記資料，不知產品名）；build 層讀登記資料產生連結與登記。
6. **Ownership**：plugin 選擇的權威在登記資料；初始化順序由資料或 plugin 宣告的依賴決定（宣告時二選一寫明）；缺 plugin／依賴衝突是 Contract 的明確錯誤（§9）；System 的登記仍是程式（plugin 的 build 函式）。
7. **更小的方案**：main 列清單（Bevy 的 `add_plugins`）——兩處同一事實，違 §9；兩處並用一致性檢查——同上。使用者已決定資料。
- 差分：plugins-foundation 實作（`project.json` 的 `plugins` 欄、CMake 讀取連結、建置產生登記編譯單元、`main` 不手寫清單、守衛掃 main 與產品程式不直接引用 plugin 的 build 函式——runtime-composition §4 與 DI 2 已改）；與 §9 的關係寫在宣告給風紀委員（候選 5 結案）。

### Delta-6：Engine 能力模組改成 Resource／Host 零件／服務（第一輪對抗 S-2 補；AGENTS §3「Public Interface の大規模変更」「複数 Module にまたがる Responsibility 移動」）

1. **Feature**：Client 與四工具的渲染、輸入、平台、文字、音訊、資產以 Resource 或 Host 零件接進機制（object_fps_pvp Client、四工具）；Match 的世界內容經 Loading 的資產階段進 World（object_fps_pvp Match）；loading 畫面顯示進度（H.2b）。
2. **現在的問題**：這些模組是物件導向的服務物件（`AssetManager`、`Renderer` 持跨元件跨幀裸指標 4 處、`SdlInput` 後端與 core 同目錄、audio 直接提交），host 直接持有並呼叫；在機制下，World 的 (c) 內容只能經 Loading 進來（runtime-composition DI 3），渲染只讀模擬（H-6），親和要能在 finish 檢查（X-3）——現有物件形狀表達不了這些約束（audit §1.1 判定）。
3. **邊界**：`engine/asset` 的 `AssetManager` → 請求狀態 Resource＋`Assets<T>`＋`RenderAssets<T>`＋`AssetInbox`（Resource 在 `engine/asset`；IO 執行緒服務在 plugins-foundation 的 headless-runner／asset-io 服務）＋plugin 登記的 loader（`engine/asset` 提供登記 API）；`engine/render` → 渲染 World 的 Resource（親和 `GraphicsContextOwner`）；`engine/input` → core＋後端介面（Host 零件；`SdlInput` 實作搬 plugin）；`engine/platform` → Host 零件（`PlatformEvents` boundary）；`engine/text` → 包；`engine/audio` → `Submitter` 服務（帶親和）；`engine/ui` 的 `UiRenderer` → System＋`Gpu*` Resource；`engine/io` → 檔案服務包（`WriteFileAtomically` 只從 `Io:file` (b) System 呼叫）。Engine 公開介面改變，但 Engine 仍不知道任何產品。
4. **影響**（全部消費者）：`engine/asset`、`engine/render`、`engine/input`、`engine/platform`、`engine/text`、`engine/audio`、`engine/ui`、`engine/io`、`engine/model`（只提供 loader 函式；不登記、不連 Kernel）、`plugins/`（asset-io 服務）；消費者：`apps/object_fps_pvp`（`AssetManager` 使用 17 檔，含 Match 側 `PlayerHitbox.cpp`、`PlayerPose.cpp`、`match_main` 的 `PlayerAnimationContent.cpp:95`，Client 側 `src/App`、`WeaponViewModel`）、`tools/ui_editor`（`AssetManager` 1 檔）、四工具的渲染與輸入、`plugins/`（sdl-window-runner、asset-catalog、imgui、offscreen-preview）、`tests/common/{asset,render,input,platform,audio,ui}`、`gyo_expect_*_links`。
5. **Dependency**：方向不變（plugins／產品 → Engine 能力模組 → Base）；新邊：`engine/asset` → `engine/kernel`（`Assets<T>`、請求狀態、`AssetInbox` 是 Resource；`StageAssets`／`ApplyStagedAssets` System 在 `engine/asset`；Kernel 只提供型別無關的暫存與游標、**不知道 asset**——第二輪對抗 S2-6；粒度：`engine/asset` 今天在單體 `GYO::Engine` 內，這條邊實際是 `GYO::Engine → GYO::Kernel`，所有 `GYO::Engine` 消費者多一條傳遞依賴，v1 不另開 target（Q3），wrap-assets 時再評估）、`engine/render` → Kernel（渲染 World 的 Resource 親和）——Engine 能力模組依賴 Kernel 是 Delta-1 的既定方向（Kernel 是 Engine 的子系統，能力模組在它之上）；`engine/input` 不再依賴 SDL（搬去 plugin）。
6. **Ownership**：資產內容身分與請求序號的權威在 `Assets<T>`／`AssetCursor`（(c) Resource），不在 `AssetManager`；渲染資源的權威在渲染 World 的 Resource；`SdlInput` 後端歸 sdl-window-runner plugin；`AssetManager` 在 framework-delete 刪、之前只以轉接存在（assets-core；期限明確，不是永續設計；新 (c) 程式不經轉接）；loader 登記的權威在 plugin（target/assets §1 第 3 點）；資產 IO 執行緒的 owner＝plugins-foundation 的 asset-io 服務。
7. **更小的方案**：保留 `AssetManager` 等物件、由 Host 在 Startup 前填內容——違 runtime-composition DI 3／assets DI 3（世界內容只經 Loading）；產品各自做臨時 `Assets<T>`——第二份同一事實（AGENTS §9）；只包不改（adapter 包住舊物件）——親和與只讀在 finish 檢查不到，是 workaround 固定為永續設計（AGENTS §4）。
- 各批差分：assets-core（`engine/asset`、`io`；`model` 只提供 loader 函式；轉接）、plugins-foundation（asset-io 服務）、wrap-render（`render`、`input` 介面、`platform`、`text`、`audio`）、wrap-assets（`RenderAssets<T>`、`ui`）、plugins-frame（`SdlInput` 搬入）、framework-delete（`AssetManager` 刪）。各批事前宣告列自己改的公開介面與改名清單（target/naming、target/assets §2）。

**計畫層另記（不另立 Delta，已在上列範圍內或已核定）**：`gyo.zone-c-sources` v1 的生產端由手寫換成 Kernel 的 G-5 匯出（格式不變）；error-handling 契約的 `KernelError` 鏈（已同意，衝突 1 的收窄在 a6-revision）；event-sounds／preview 的資料契約 owner 搬到 plugin（Delta-2 第 6 項）；field-session DI 3 的澄清句（衝突 3，相容）。

---

## 5. 吸收的輸入逐條對照（42 列）

| # | 來源 | 輸入 | 進哪一批 | 怎麼驗 |
|---|---|---|---|---|
| 1 | ci-readiness §4-1 | OS 層 Delta：頁保護平台層進 `engine/os` | a6-revision（Delta-4 文字、U1）→ pageguard-os（條件） | os 邊界守衛對 `engine/kernel` 零命中；`gyo_expect_links` 檢查 kernel 不外露 `GYO::Os` |
| 2 | ci-readiness §4-2 | 編譯失敗測試的形狀（try_compile vs trait＋doctest） | kernel-scaffold（兩種樣本過 windows L1、U5 預設）→ registry-schedule 第一次實作 | J-10／J-11／J-13 在 windows-x64 L1 過；對應突變以 `expect_build_failure` killed |
| 3 | ci-readiness §4-3 | 第三層獨立執行檔＋`SKIP_RETURN_CODE 77` | pageguard-os | sanitizer 建置下 ctest 顯示 skipped（本機 tsan 樹一次）；一般建置執行 |
| 4 | ci-readiness §4-4 | spike 步驟移除的時點與範圍 | ci-spike-retire（條件 (a) world-exec 後、(b) pageguard-os 或 U1 不採）；`.github/` 交使用者 | §2.6 兩條件逐條勾；`spikes/kernel` 引用 0 |
| 5 | ci-readiness §4-5 | 驗證開關是執行期選項；L1 只有 RelWithDebInfo | world-exec | 同一執行檔開／關兩個 World；L1 xml 兩組案例都有 |
| 6 | pageguard 1 | 第三層範圍限定（Exclusive 欄、(c) System、開關開） | a6-revision（條文）→ pageguard-os | A.6 原文含三個限定詞；對抗核對；新宣告的判定 1 不含範圍外形狀 |
| 7 | pageguard 2 | Resource 要不要保護 | a6-revision（決定；建議 v1 不保護）→ pageguard-os | 決定寫進 A.6；(b3) 測試形狀隨決定（不保護則記 Limit） |
| 8 | pageguard 3 | (b) System 成本 2×\|Exclusive 欄\| | a6-revision（規則：(b) 只解它宣告寫的欄＋Shared）→ pageguard-os（量，BU-8）；first-host-cost 先數 PvP 的 Exclusive 欄數 | BU-8（頁保護）實量 |
| 9 | pageguard 4 | 與其他 SIGSEGV／SIGBUS 使用者共存；`reinstalls` 進證據 | a6-revision（條文）→ pageguard-os | 串接測試（外來故障交前任）；`reinstalls` 出現在「驗證開著」報告欄位 |
| 10 | pageguard 5 | 多 World：違規帶 World 身分；區間表不可變陣列原子發佈 | world-exec（Err 帶 World 身分，第二層）→ pageguard-os（區間表） | J-5 跨 World 案例；TSan 下區間表讀寫零報告 |
| 11 | pageguard 6 | 欄重新配置與保護狀態 | world-exec（欄配置介面）→ pageguard-os | F.2 在 Run 內讓欄成長後新頁仍唯讀的測試 |
| 12 | pageguard 7 | `tlCurrent` 為空的故障：GYO_ASSERT 或 Err | a6-revision（決定；建議 GYO_ASSERT） | A.6 條文；pageguard-os 對應測試用 `GYO_CHECK_ASSERTS` |
| 13 | pageguard 8 | 第一層改 Err | world-exec | J-5 (c) 類以 Err 回傳；spike `SPIKE_CHECK_ASSERTS` 案例改檢查 RunResult |
| 14 | pageguard 9 | 頁大小與粒度分平台估算 | a6-revision → §6 BU-4（World 記憶體）／BU-8（頁保護） | 預算寫 4／16／64 KiB 三種 |
| 15 | pageguard 10 | 平台層放 OS 子系統、Kernel 只持介面 | Delta-4；pageguard-os | 同第 1 列 |
| 16 | pageguard 11 | 第二層不能減；保護欄可跳過呼叫後比對 | world-exec（兩層都實作）→ pageguard-os（跳過的最佳化，量了才做） | 第二層關掉時 Shared／Resource／事件案例失敗的突變；跳過前後 J-5 全過 |
| 17 | -detect D1 | Life 時序 T=35／R=215（R−T=180），兩層相同 | plan-dryrun（Plan 謂詞，U6 已定 `LifeState`＋`DeathEvent`）→ match-sim（新路徑實測） | match-sim 以新重放 host 跑 D1 劇本、觀測 snapshot(T)／snapshot(R)：T=35、R=215、R−T=180 與 -detect 現況相同；針對新 System 的 `kp-death-next-tick`、`kp-respawn-next-tick` 各以指定 CHECK 失敗（D1 原案例直接建構 `PvpMatch`，列入測試、守衛與突變的去處——第一輪對抗 S-1） |
| 18 | -detect D3 | `connectionQualityFailures` 在 Judge 寫、kW 當 tick 進發布快照；Match `Snapshot()` 恆 0 | match-host（Judge 是 (c) System，Connection 診斷欄唯一寫者） | host 層特徵（W=600、1200／1800／2400）在新 host 上重現（D3 原案例直接建構 `MatchRuntimeHost`，重寫不是「原樣過」）；Match 層那條照 §14.4 改寫；`kp-failures-*` 改針對新 Judge 仍 killed |
| 19 | -detect 補列 1 | IPC 執行緒 `IngressBucketId`／`RecordRejection` 的 `ContainsPlayer`／`Snapshot()` | a6-revision（併入 G.4 第 5 條）→ match-host | `kernel_prep_sites` A 類對應列刪除；Ingress 拒收計數測試 |
| 20 | -detect 補列 2 | 主執行緒 `TakeIngressStatistics` 的 `Snapshot().players`；Stop 後 `TickCount()` | a6-revision（G.4 補第 6 條）→ match-host | 統計改經交換點複製（Host 診斷）；A 類列刪除；`TickCount()` 同法：runner 在最後一個交換點把 domain tick 複製進 Host 統計，主執行緒讀複本（不讀 World；W-6——第一輪對抗 S-9） |
| 21 | -detect 補列 3 | 無正式呼叫者 `QueueActionAcknowledgement`、`SubmitActions` | match-host（§7 刪除） | `git grep -nP` 引用 0；改寫的測試照 §14.4 說明、刪 TEST_CASE 前逐 CHECK 看 |
| 22 | -detect 補列 4 | `IpcHost` 的 `GetActionResults` ×4、`NoteWireRejection` ×2 | a6-revision（G.4 第 5 條）→ match-host | 改讀 `Link.outbound`；A 類列刪除；Gateway go 測試全過 |
| 23 | -detect 另記 | `TraceMovement` 本身讀 `MovementTraceNowNs()` | a6-revision（G.4 第 3 條措辭）→ pvp-diagnostics（owner object_fps_pvp；接收端在 diagnostics-service；Match 側）＋match-sim（持 `DiagnosticsWriter<T>` 的 (c) System）＋client-sim／client-link／client-frame（Client 側呼叫點；全域在 PR 線末端 client-frame 刪；S2-7、S3-1） | (c) System 持 `DiagnosticsWriter<T>`、時間用被記錄的 boundary 輸入 `HostClockSample`（牆鐘語義保留；任一欄語義變＝升契約版本並列消費者——第一輪對抗 S-17）；J-17 ZS4 在確認清單為 0；白名單 W／G 對應列刪除 |
| 24 | windows (a) | `DiagnosticFileTests.cpp:216` 檔案仍開著時 `remove_all` 失敗 | diagnostics-service | 「Close 後無開啟句柄」斷言，測試內 100 次、windows-x64 L1 零失敗；closeout 60 run 比較 |
| 25 | windows (b) | `TraceSinkTests.cpp:155` Close 尾行順序不符 | diagnostics-service | 尾行順序契約測試 100 次相同；closeout 比較 |
| 26 | windows (c) | pvp.cpu Timeout 停在 RoleThread／wait backend（PR #190 run 43200345263；卡住的角色 `gyo-client-net`＝`ClientConnection`） | plugins-foundation（headless-runner 啟停 N＝200）→ client-sim（只記錄）→ **client-link**（收口：與 client-sim 同一宣告，PR 線末端）；match-host 只觀察；closeout 重掃 | 測試內零逾時、零 hang、每次記 wait backend 名；收口：先以 `tally.md` 方法算基準發生率 p、定 N 使 (1−p)^N ≤5%，修改後 N 次 dispatch 0 timeout（第一輪對抗 S-10）；60 run 統計比較 |
| 27 | legacy 衝突 1 | error-handling L124「no error chains」 | a6-revision（收窄、legacy 加「已被取代」註、鏈／清單措辭統一）；world-exec 宣告引用 | spec 內「鏈」「清單」用詞一致；對抗核對 |
| 28 | legacy 衝突 2 | preview.md DI 4（readback） | a6-revision（L3 carry-over 限縮 DI 1～3、5，DI 4 標已被取代，§3b 新條文）→ plugins-editor（PV2～PV5、pv-* 重寫） | 重寫的測試無 readback 斷言且以突變證明檢出力；readback 路徑引用 0 |
| 29 | legacy 衝突 3 | field-session DI 3 vs 執行期驗證開關 | a6-revision（target/system-contract §2b 加一句；field-session 不動） | 句子存在；對抗核對 |
| 30 | legacy 衝突 4 | os RC 9（D78）vs 頁保護 | a6-revision（Delta-4 與 os.md 標註的文字；U1 已定延到 G2a 後決定）→ pageguard-os（條件） | 使用者決定行進 HANDOFF；os.md 依決定標 |
| 31 | legacy 候選 5 | AGENTS §9 vs plugin 清單在 main | **已由 `v8-arch-plugin-selection-data` 結案**（資料）→ plugins-foundation 實作 | 共通層無產品名分支（掃描）；`project.json` 是唯一事實（產生物與資料一致的反例測試）；宣告寫與 §9 的關係給風紀委員 |
| 32 | legacy「被取代」定義 | 36 條重標 | a6-revision | 重標後 被取代＋相容（措辭更新）＝36；E-3 類「半條」歸相容 |
| 33 | legacy determinism | DI 1～6 現行；§1／§2 與 §4 區表（已裁決）搬 target；**§7 與 E0～E5 不搬**（與 DET-0～3 同組暫停項） | a6-revision（搬 §1、§2、§4 區表；HANDOFF〈暫停〉加一行）→ closeout（§7／E0～E5 去處在刪 legacy 前定）；狀態行由圖書管理員改 | target/determinism 不再寫「this page does not change §1, §2」、不含 legacy §7 缺口清單的條目（章節比對；照搬的區表內的 E-n 引用照原文保留，不用 `E[0-5]` 掃描——S2-2）；反向依賴對 §1／§2 的引用 0；closeout 有去處的決定行 |
| 34 | legacy 行號引用 | 13 處改條號或章節名 | a6-revision | 正規式限定 legacy 檔名的 `git grep -nP` 為 0；事前宣告基準件数 13、先以正例命中、不丟 stderr（第一輪對抗 S-7） |
| 35 | legacy entity-model 第 139 行 | 欄位分類表搬進**產品的 target 文件**（entity-model 後繼；第一輪對抗 S-18 修正處置行的「搬 target/world」） | a6-revision（match-sim 的拆欄引用它；target/world 只寫通用規則） | 產品 target 文件有表；target/world 無產品欄位名（掃描）；spec 引用改條號 |
| 36 | legacy entity-store DI 6 | stamp 在 kernel 不在 store | a6-revision（target/world §6）→ world-exec（實作） | `engine/entity_store` 無 stamp（審查＋grep）；J-30 |
| 37 | legacy J-9 | 自寫產品名禁字表 | kernel-scaffold | 自測：加一個產品名失敗；`Despawn`／`ExtractRes`／`zone`、`far`／`near` 不誤判 |
| 38 | legacy 守衛遷移影響檔 | framework guard、4 工具守衛、`FRAMEWORK`、`ToolRegistryTests` 保留名 | plugins-foundation（加 plugins 對應、不刪；初判）→ 工具四批（正向清單改寫）→ framework-delete（退役） | 各批「守衛與突變的去處」表照四選一（§3 共通；§5.2 第 10～13 列）；各守衛在對應批後仍綠或被正確退役；刪 framework 後 grep 殘 0 |
| 39 | -zones 回報 1（出處 `docs/dev_logs/2026_11_12_pvp_kernel_prep_zones.zh-Hant.md`「回報架構師」節） | `ArenaMesh` 疑為 audit P13 誤判（內容屬呈現） | a6-revision（區表修正：判區）→ match-sim（判呈現則搬出 Sim 或改含入） | 區表有判定與理由；確認／候選清單與 `test_zone_c_lists.py` 隨判定更新（改守衛判定照 §14.4） |
| 40 | -zones 回報 2（出處 `docs/dev_logs/2026_11_12_pvp_kernel_prep_zones.zh-Hant.md`「回報架構師」節） | `App/AnimationSetDefinition`、`CharacterPresentationDefinition`、`AssetDefinitionHelpers.hpp` 的區待判（Sim 含入 App 三處） | a6-revision（判區）→ match-sim（搬動或改含入；§5.1 的 F 2 依賴本列） | 區表有判定；Sim 不再含入未判區的 App 檔（掃描，正例先命中） |
| 41 | -zones 回報 3（出處 `docs/dev_logs/2026_11_12_pvp_kernel_prep_zones.zh-Hant.md`「回報架構師」節） | `Combat.hpp` 的 audit P1 分組與 J-17 放 Sim 不同 | a6-revision（判區，對齊 audit P1 分組）→ match-sim（依判定搬動） | 區表與 audit 分組一致；`zone_scan` 結果與判定一致 |
| 42 | -zones 回報 4（出處 `docs/dev_logs/2026_11_12_pvp_kernel_prep_zones.zh-Hant.md`「回報架構師」節） | `PVP_DOMAIN_SOURCES` 含 `Present/HitFeedback.cpp`、`Present/SoundCues.cpp`；兩個 Present 標頭含入 `PvpMatch.hpp`；具體函式 `LoadPlayerPresentationDefinition`／`SamplePlayerPresentationPose` 只有 Client 用卻進了 Sim | a6-revision（判區）→ match-sim（改 build 清單或改含入；`PvpMatch.hpp` 的含入最遲在 match-host 刪舊 `PvpMatch` 時消） | `PVP_DOMAIN_SOURCES` 不含 Present 檔；Present 標頭不含 `PvpMatch.hpp`（掃描） |

件数：42 列＝輸入 42 條：**一致**。分類（每列只算一類，以第一個落點為準）：進非條件批 **39**（第 30 列的第一個落點是 a6-revision——該批寫 Delta-4 與標註的文字；U1 本身已定延到 G2a 後——照本規則算進批，第二輪對抗 N2-7；第 39～42 列是第三輪 S3-8 吸收的 -zones 回報四項，第一個落點 a6-revision）；只在條件批落實 **2**（第 3、15 列——第 6～9、11、14、16 列先在 a6-revision 或 world-exec 落字，算進批）；交未決 **0**；已結案 **1**（第 31 列）；不採 0。39＋2＋0＋1＝42。第一稿寫「31＋6＋1＋1」加總是 39、括號列了 7 列，不一致（第一輪對抗 S-16）；第一輪修訂寫 34＋2＋1＋1，第二輪照「第一個落點」規則改正。歸 a6-revision 的 24 列見 §3.1 判定 (2)。

### 5.1 -zones 交遷移批的 24 處（`v8-product-kernel-prep-zones-gate`；第二輪對抗 B2-1）

出處：-zones 事前宣告（分支 `claude/arch-pvp-kernel-prep-zones`）的殘留分類表與 Q3(a)；門＝確認清單 7 處（B 5＋F 2）由 `test_zone_c_lists.py` 的 RESIDUAL 釘住，D 3、E 14 不在確認清單上（住在不搬的 P5／P8 檔）、只交遷移批。件数：5＋2＋3＋14＝**24**。

| 類 | 處數 | 位置 | 進哪一批 | 怎麼驗 |
|---|---|---|---|---|
| B（P13 載入器簽名） | 5 | `Sim/Arena.hpp`、`Sim/MovementRulesFile.hpp`、`Sim/PlayerAnimationContent.hpp`、`Pvp/NativeFiles.hpp:4`、`Sim/PlayerPose.hpp` 的 `<filesystem>` | assets-core（loader 契約：收位元組／`AssetInbox` 的完成項、不收路徑；登記 API）→ match-sim 消費（產品 loader 改簽名、由產品 (c) plugin 登記）；舊呼叫端：Sim 外的讀檔入口（`Pvp/` 保留原名原簽名、內部轉呼 Sim 的解析）是期限明確的轉接——舊 `match_main` 的使用處（`:100`、`:106`、`:125`、`:137`）在 match-host 刪舊路徑時消失，舊 Client 的 `LoadInstalledArenas`（`PvpApplication.cpp:963`）等在 client-frame 歸零；並存期間舊路徑保留 `<filesystem>`（不在 Sim，不違反門） | match-sim 判定 (3)：RESIDUAL 只因該處原因消除而刪列（締める面）；不做成 Startup System（runtime-composition DI 3）；入口使用處清單在 match-host／client-frame 歸零 |
| F（容器） | 2 | `App/AnimationSetDefinition.hpp:8、23` 的 `unordered_map` | match-sim（換有序容器；依賴 §5 第 40 列的區判定——檔在 `App/`，要不要搬進 Sim 由 a6-revision 的區表修正先定）；舊 Client 的共用使用者（`App/CharacterPresentationDefinition.cpp`、`Present/WeaponPresentationDefinition.cpp`）同批跟著換、行為相同 | match-sim 判定 (3)(9)：RESIDUAL 刪兩列；`AuthorityDigest`、`AnimationContentDigest`、golden 37 零差，Client 呈現層既有測試全過、舊 Client 的 arena／animation digest 不變；有差即停、交決定性線 |
| D（P5 Host 時鐘） | 3 | `ClientSimulation.hpp:7` `<chrono>`、`PredictionElapsedTime.hpp:3` `<chrono>`、`:12` `steady_clock` | client-sim（Host 時鐘移到 runner 的時鐘策略） | client-sim 判定 (9)：3 處→0 |
| E（P8 診斷全域） | 14 | `MovementDiag.hpp:18、19、21、23、26、188`、`MovementTrace.hpp:4、5、9、59、101、102`、`MovementTraceWriter.hpp:9、10` | pvp-diagnostics（G.4 第 3 條改句柄；Match 側呼叫點）→ client-sim（`LocalPlayerPrediction` 內的呼叫點）→ client-link（`ClientConnection.cpp:714、803`）→ **client-frame**（`PvpApplication.cpp:1369`、`main.cpp:157`；最後一個 Client 呼叫點遷完後刪全域與寫檔執行緒；S3-1） | pvp-diagnostics 判定 (3)(4)（Match 側）；client-sim 判定 (9)、client-link 判定 (7)（各自的呼叫點）；**client-frame 判定 (10)：14 處→0** |

`zone-c-rewrite.json`（P1 `PvpMatch.cpp`、P5 `ClientSimulation.cpp`、P6 `LocalPlayerPrediction.cpp`；只報告）：P1 列在 match-host 刪舊 `PvpMatch` 時移除，P5／P6 列在 client-sim 移除；清單空則刪檔（AGENTS §7 不留空）。namespace：(c) 用 `fps::pvp::sim`（inline，v1），closeout 改非 inline（§3.14 Architecture、§3.32 範圍）。

### 5.2 守衛去處的起點清單（legacy-impact §4 的 21 個守衛；`v8-arch-guards-migrate-with-code`）

每列寫「預計在哪一批判去處」、預期的四選一（最後以該批事前宣告為準）與 DI 落點（「留」型的 DI 在 legacy 檔刪除前要先寫進 target 或現況檔，§3 共通規則第 2 點；第三輪對抗 S3-6）；closeout 時「性質只來自 legacy 的守衛」＝0（§0 A10）。件数：**21**。逐子列的性質、來源條文、突變與「不動會判誰違規」見守衛盤點 `docs/architecture/plans/kernel/guards-inventory.zh-Hant.md`（PR #198；以下稱盤點）；本表照盤點 §6 的 9 處不一致與 HANDOFF 決定行 `v8-arch-guards-inventory-dispositions` 改過。**件数來源（盤點 §0）**：21 列拆成子列 **63**（留 31／改寫 21／退役 11，另新增 3：J-9 禁字表、plugins 依賴守衛、trace `:155` 契約測試）；突變清單 **26 檔 1,841 條**（留 1,131／重列 627／退役 83）；執行器 3 支。

| # | 守衛（legacy-impact §4） | 判去處的批 | 預期去處 | DI 落點（legacy 檔刪除前寫入；寫入的批） |
|---|---|---|---|---|
| 1 | `entity_store_guard.py`（W1～W5） | world-exec | **留（守衛）／突變條件重列**：Delta-3 的兩個選項都改 store API，`es-row*` 15 條的 `find` 文字會過期，world-exec 宣告列「重列」；**整體：store 併入 kernel 時條件退役**（`SCANNED` 目錄不存在即失敗）（Q7） | entity-store.md DI 8、9（legacy）→ target/world 的 store 節（world-exec 寫入） |
| 2 | `gyo_expect_links(gyo_entity_store GYO::Base)` | world-exec | 留 | 同第 1 列（world-exec 寫入） |
| 3 | `neutral_names_guard.py` | kernel-scaffold | 留（不套到 kernel；J-9 自寫＝新增） | voxel-mesh／map-markers DI 1（現況檔，不需搬）；architecture.md Layers check 1 的部分（legacy）→ target/naming 或 kernel DI 頁（a6-revision 寫入） |
| 4 | `os_boundary_guard.py`（R1～R6） | kernel-scaffold；pageguard-os（條件）重判；**plugins-foundation 改寫（ROOTS 加 `plugins`）** | 留；plugins-foundation 改掃描根（締める面，但 DI 守衛→使用者核准；Q1；不改＝對 plugins 空轉） | os.md DI 1～3（現況檔，不需搬） |
| 5 | OS 編譯定義檢查 | kernel-scaffold；**plugins-foundation 改寫（掃描根加 `plugins`）** | 留＋改掃描根（同第 4 列） | os.md DI 1（現況檔） |
| 6 | `ProcessCodePageCheck.cmake` | kernel-scaffold；**plugins-foundation 改寫（掃描根加 `plugins`）** | 留＋改掃描根（同第 4 列） | os.md DI 11（現況檔） |
| 7 | `gyo_expect_os_links` | kernel-scaffold；pageguard-os（條件）重判 | 留 | os.md DI 3（現況檔） |
| 8 | `OsHeaderHygieneTests.cpp` | pageguard-os（條件；不採時不碰） | 留 | os.md DI 4（現況檔） |
| 9 | platform-sdl G1／G2／G3 | wrap-render；**plugins-frame**（`SdlInput` 搬入） | G2 留；G1／G3 改寫——G1／G3 以 `if(TARGET)` 掛入，後端搬走或改名時**靜默退出**（空轉），wrap-render 改成硬性存在檢查（締める面，Q11）；搬走的後端其 G1／G2 退役，plugins-frame 新增 plugin 版標頭衛生守衛（Q2） | platform-sdl.md DI 1、2（現況檔） |
| 10 | `framework_dependency_guard.py`、link check、PV／fw-* 突變 | plugins-foundation、plugins-frame、plugins-editor（搬檔的批）；framework-delete | framework 側退役（framework-delete；`framework/` 刪後 `rglob` 回空會空轉通過，所以 `tests/framework` 同批刪）；**plugin 側是新增**（另寫一支，不是改寫）；PV2／PV5 的 readback 判定退役、PV1／PV3／PV4／PV6 改寫；**fw-* 針對 `framework/src` 的 29＋31＋10 條在 plugins-foundation／plugins-frame／plugins-editor 搬檔時重列**（不等 framework-delete） | framework／preview（legacy）：退役者不需落點；改寫的 PV 性質 → target/runtime-composition §3b（a6-revision 已寫） |
| 11 | 工具守衛 ×4（正向清單） | plugins-foundation 初判 → 工具四批 | 改寫（改掛 plugin target） | framework/architecture DI 2（legacy）→ 各工具自己的 Architecture 文件 DI（工具四批寫入） |
| 12 | `GyoComponents.cmake` `FRAMEWORK`、`GYO_ENABLE_FRAMEWORK` | plugins-foundation 初判 → framework-delete | 留到 framework-delete，退役 | 退役，不需落點 |
| 13 | `ToolRegistryTests.cmake` 保留名 `framework` | framework-delete | 退役（§7 不留 Dummy） | 退役，不需落點 |
| 14 | threads／time 守衛與連結期望 | threading | 留 | threads.md、time.md（現況檔） |
| 15 | trace 連結期望、`TraceSinkTests` | diagnostics-service | 留（`:155` 形狀另新增契約測試） | trace.md（現況檔） |
| 16 | `gyo_expect_collision/model/map_markers/voxel_mesh/audio_links` 等 | assets-core（model）；wrap-render（audio） | 留；audio 若要連 Kernel＝DI 變更交使用者 | collision／animation-events／map-markers／voxel-mesh／audio（現況檔） |
| 17 | `tests/common/input/`（IP-1、m7～m9）、`ui_dependency_guard.py` | wrap-render（input）；plugins-frame（`SdlInput` 搬）；wrap-assets（ui） | 留或改寫 | input.md、ui_toolchain（現況檔） |
| 18 | `EntityModelTests`（G1、G2、G3、G7、G13）、`RequireEntityOrder`、`authority_digest`（G11） | match-sim；**G3→match-host**（綁 `MatchRuntimeHost` API；Q4） | G1-i／G2-i 由 J-1／J-2／J-32 承擔＝退役；G1-ii（每個實體在種類表的每個欄都有一列）改寫成產品掃描；G2-ii、G7、G13 改寫（G13 優先成 Plan 謂詞）；G3 改寫（host 側）；**`RequireEntityOrder`→退役**（函式隨 `PvpMatch` 刪；J-1 由 World 守）；golden 留；DI 守衛的改寫與退役照 §14.4 交使用者 | entity-model（legacy）→ 產品 target 文件（entity-model 後繼；a6-revision 建檔、match-sim 寫入 G7／G13／G11 的條文；G1／G2 由 kernel DI 頁的 J-1／J-32 承擔） |
| 19 | `PvpMatchTests.cpp:1774`（network §7 DI 註解；OutageRig 四個 TEST_CASE）、`:82`（S3 單一紀錄）、`fixtures/ingress_v1/README` | **client-sim**（OutageRig 的預測側）＋**match-host**（S3 帳本側）（Q5）；closeout（註解引用） | **client-sim 改寫**（OutageRig 以新 Runtime 重寫）＋**match-host 改寫**（S3 成獨占欄的啟動檢查＋等式測試）；註解引用隨 legacy 刪改指 target | network-architecture §7（legacy）→ 產品 target 文件（network 後繼；match-host 寫入 S3 單一紀錄、client-sim 寫入連線中斷的條文） |
| 20 | `state_whitelist_test.go` | match-host | 留 | 同第 19 列（match-host 寫入） |
| 21 | 註解引用 architecture.md（`neutral_names_guard.py:3`、collision `CMakeLists.txt:10`） | closeout | **改引用**（只差註解；註解改指 target） | architecture.md（legacy）→ 註解改指 target（closeout） |

**補遺：盤點 §2.22 的六個守衛**（不在 legacy-impact 的 21 列，不計入 21；給各批事前宣告對帳）：

| # | 守衛 | 判去處的批 | 預期去處 |
|---|---|---|---|
| 補1 | `test_kernel_prep_sites.py`（D2 棘輪白名單） | pvp-diagnostics、match-host、client-sim | **改寫型**（搬檔只改檔名欄、不放寬）；Match 側 G 6 列→pvp-diagnostics、A 22 列→match-host、Client 側 G 7 列→client-sim 歸零 |
| 補2 | `KernelPrepDetectTests.cpp`（D1 Life 時序、D3 failures 寫點） | match-sim（D1）、match-host（D3） | 改寫（經新 host 觀測 T=35／R=215；D3 由新 Judge 重現）；U6 選改變時序的方案時退役並記錄、golden 照 U9 逐條核（Q6） |
| 補3 | `test_zone_c_lists.py`＋`zone_c_scan_*` | match-sim（之後 match-host、client-sim 刪 rewrite 列） | 留（target 側守衛）；新 (c) System 檔必須同時列進 `zone-c-sources.json` |
| 補4 | `tests/common/zone_scan/`（`zone_scan.selftest`；ZS1～ZS7） | — | 留（J-17 的規則本體） |
| 補5 | `sdl_symbol_guard.py`＋產品 SDL link guard | client-frame | 留（產品中性不變）；`sdl-*` 34 條突變隨 client-frame 重列 |
| 補6 | `test_diagnostic_isolation.py` | pvp-diagnostics | 留（性質）＋改引用（新輸出路徑） |

**盤點 §4 缺口 9 條的歸批**（HANDOFF `v8-arch-guards-inventory-dispositions`）：缺口 1 naming DI 1 的 namespace↔目錄掃描→kernel-scaffold（J-9 掃描器擴充；Q9）；缺口 2 runtime-composition DI 3「Peer 不掛 Player」掃描→match-host（Q9）；缺口 3 error-handling DI 2「Executor 以外無 log」→kernel-scaffold 的 J-9 禁字表（Q9）；缺口 4、5 framework DI 1「不是產品」／DI 3「中性」→`docs/plugins/` 的層 DI 與 plugins-foundation 新增的守衛（Q8）；缺口 6 G1-ii「每個實體在種類表的每個欄都有一列」→match-sim 的產品掃描（第 18 列）；缺口 7 OS 邊界三件的掃描根→plugins-foundation（第 4～6 列；Q1）；缺口 8 SDL 後端搬進 plugin 後的標頭衛生→plugins-frame（第 9 列；Q2）；缺口 9 `if(TARGET)` 靜默退出→wrap-render（第 9、17 列；Q11）。

---

## 6. 預算（第一個 host＝Match 上量；AGENTS §2）

**數字的根據**（AGENTS §2 予見の根拠 3「既知の規模と予算」）：Match／Client 固定步 60 Hz（`adapter.AuthorityTickRate = 60`；`Movement.hpp`、`LocalPlayerPrediction.hpp` 註解）＝每 tick **16.67 ms**；entity-model RC-1：同時實體 4,000＝需求的 10 倍（需求 **400**）、Linux Match 欄維護每 tick p95 **1 ms**；entity-store RC-1 同門檻；field-session C3／C6 的統計形狀。**今天的 tick 成本未量**：pvp-baseline 先量分母（重放 host 的 BU-1 分母，與真實 Match 行程的 BU-1′／BU-4 RSS／BU-6 分母——N-16：真實行程的舊 binary 基準由 pvp-baseline 在看新路徑結果前量，不留到 match-host）。下表是候選；**U12 已定**：等 pvp-baseline 的數字出來，把各項改寫成「今天的值 × 倍數」，在 **a6-revision 完成後、match-sim 宣告前**再交使用者核准一次（§14.1），看結果後不放寬；Kernel 工時在 G1 成立時重估（§8 RC-3）。**量測機器（U8 已定三層）**：Intel Mac 原生＝開發參考；**Docker Linux 容器＝閘門判定**（Match headless 不需 GPU；新舊基線用同一映像；絕對數字標「容器內」）；Linux 實機＝出問題且允許時確認；CI runner 不用。每個數字附「出所」欄；寫「推定」的沒有根據，是三案都點名的弱點（第一稿只承認 BU-1 的 2.0 ms 與驗證開的 8.33 ms 弱，第一輪對抗 S-15 指出其餘也沒有根據）。**編號只有這一套**（BU-1、BU-1′、BU-2～BU-8；第二輪由 B-n 改 BU-n，以區別 spec 的 B-n 與對抗報告的 B-n——第二輪對抗 S2-1），全稿引用以此為準（A8）。

| # | 項 | 預算（驗證關＝release 預設） | 驗證開（測試版預設） | 量測方法 | 在哪批量 | 出所 |
|---|---|---|---|---|---|---|
| BU-1 | Match FixedUpdate 一個 tick 的工作（重放 host；37 golden 情境＋8 人長情境 ≥3,600 tick） | p95 ≤ max(基線 p95 ×1.25, 基線 p95 ＋0.25 ms)，且絕對 p95 ≤ **2.0 ms**（tick 的 12%）；p99 ≤ 4.17 ms（tick 的 25%） | p95 ≤ 驗證關 ×3 且 ≤ **8.33 ms**（tick 50%，測試版仍能即時跑） | 口徑同 pvp-baseline（交換點 A→B）；RelWithDebInfo；判定在 Docker Linux 容器、與基線同一映像（U8；絕對數字標「容器內」）；暖機 600 tick 不計；同機 5 次取每次 p95 的中位數；新舊同情境 | first-host-cost（G2a） | 16.67 ms：60 Hz（`adapter.AuthorityTickRate = 60`）；×1.25／＋0.25：三案折衷（§6 取捨，推定）；2.0 ms（12%）、4.17 ms（25%）、8.33 ms（50%）、×3：**推定** |
| BU-1′ | 同上，真實 Match 行程（含交換點與 IO 執行緒） | 同 BU-1；10 分鐘 bot 對戰或 acceptance 腳本中 overrun（>16.67 ms）0 次 | 同 BU-1 | field-session C3 形狀的統計行；Docker Linux 容器（U8）；分母＝pvp-baseline 在同一映像量的真實行程基準 | match-host（G2b） | overrun 欄：field-session C3；「0 次」：**推定**（今天的 overrun 數由 pvp-baseline 給，若非 0 則改為「≤基線」並經 U12） |
| BU-2 | Kernel 自身開銷（合成：400 實體×8 欄×20 個空函式體 (c) System，Plan 形狀仿 PvP Simulate、含延後 spawn／despawn） | 一次 `Run` p95 ≤ **2.0 ms**；每次 System 呼叫派發 ≤2 µs、每個空佇列同步點 ≤5 µs；以 PvP 的 System 數（dryrun 得到）換算不得超過 BU-1 允許增量的一半；欄維護（insert／erase／合併）p95 ≤ **1 ms**，4,000 實體時仍 ≤1 ms（超過＝spec K.2 第 6 條） | 第二層比對成本 ≤ 欄總位元組的線性成本，記錄斜率 | `tests/common/kernel` 記錄執行檔（不進 ctest），100,000 次 Run；本機 Release＋Linux | world-exec（初量）、first-host-cost | 400 實體、4,000、1 ms：entity-model RC-1／entity-store RC-1；8 欄×20 System：PvP 形狀估（推定）；2.0 ms、2 µs、5 µs、「一半」：**推定** |
| BU-3 | finish（Registry 凍結＋Scheduler 分析） | ≤ **50 ms** @ 500 System（spec D.4 的 O(N²) 未量） | — | registry-schedule 的合成 Registry；本機 Release | registry-schedule | 500：PvP 約數十個 System 的 10 倍（比照 RC-1 的 10 倍法，推定）；50 ms：**推定** |
| BU-4 | World 記憶體 | 400 實體：World 位元組（欄＋keys 8 B＋stamps 16 B／格）≤ **4 MiB**；Match 行程穩態 RSS ≤ 基線 ＋16 MiB 且 ≤ 基線 ×1.10 | 快照緩衝 ≤ 欄總位元組 ×2；**關時快照緩衝 0 位元組**（LIM-6 條件 4） | World 自報每欄位元組（kernel 診斷 Resource，可加總核對）；RSS 由 `/usr/bin/time -l`／`-v` 或 `/proc/self/status` VmHWM 穩態取；分母＝pvp-baseline 的真實行程 RSS | first-host-cost（World）、match-host（RSS） | 8 B／16 B：spec A.1 的 keys／stamps 佈局；0 位元組：LIM-6 條件 4；4 MiB、＋16 MiB、×1.10、×2：**推定**；頁大小三種：pageguard 9 |
| BU-5 | PredictionHistory（Client） | ≤ 深度 × 回滾集合位元組 ×1.2 | — | rollback-extract 合成記錄；client-sim 實測 | rollback-extract、client-sim | 深度×集合：H.5 的資料模型（下界）；×1.2：**推定** |
| BU-6 | 載入到 ready（Loading → `LoadingState = Ready`；現有 arena） | ≤ max(同口徑基線 ×1.10, 同口徑基線 ＋100 ms)，絕對 ≤ **1 s**；每個 Loading 批 ≤ **16.67 ms**（loading 畫面不卡幀，H.2b 的 Feature） | 同左 ×1.5 | Host 側時間戳；10 次中位數；Loading 批計時由 runner 記錄；分母：重放 host 值（G2a）＝pvp-baseline 以同一內容、同一重放形式量的今天載入時間，真實行程值（G2b）＝pvp-baseline 的真實行程「啟動到可接受 join」（10 次冷啟動）——第二輪對抗 S2-10 | loading（合成，每批上限）、first-host-cost（重放）、match-host（真實行程）、client-sim／client-frame（Client 批） | 16.67 ms：60 Hz 一幀（H.2b Feature 的直接推論）；×1.10、＋100 ms、1 s、×1.5：**推定** |
| BU-7 | Client 幀（階段 3） | ClientFrame＋ClientRender p95 ≤ 基線 ×1.10（多一個 World 與每幀一次 Extract 的代價）；Client 模擬 tick 套 BU-1 規則對舊 Client | — | 基線在 client-frame 開工前另量（同口徑：一幀從 pump 後到 Present）；Windows 實機（CI 量不到）＋runner 記錄 | client-sim、client-frame | ×1.10：**推定** |
| BU-8（條件） | 頁保護（pageguard-os） | 不適用（關時零） | (c) System 每次切換 ≤10 µs；(b) System 成本 2×\|Exclusive 欄\|，64 欄 ≤250 µs、實際欄數 ≤1 ms／(b) System；記憶體 commit ≤ 欄數×頁大小（4 KiB Linux／16 KiB Apple Silicon／Windows 64 KiB 保留位址，總保留 ≤64 MiB） | spike Cost 案例改成記錄執行檔；Release 組態（spike 參照：full ≈200 µs@64 欄、switch ≈7 µs） | pageguard-os | 2×\|欄\|：pageguard 3；≈200 µs、≈7 µs：spike 實測（Debug）；10 µs、250 µs：spike 值的 1.25～1.4 倍（推定）；1 ms、64 MiB：**推定**；頁大小：pageguard 9 |

量測紀律：量測執行檔不進 ctest（共用 runner 的 µs 計時是雜訊；ci-readiness §2.2）；證據放主 checkout `build/evidence/kernel-migration/<批>/`（git 管理外），文件只寫結論與判定用的數字（AGENTS §15）；舊路徑基線在同一批（重放 host 與真實行程都由 pvp-baseline 量）、同一台機器、看新路徑結果前量，同機 5 次 p95 散佈 >10% 先停下判環境（與 pvp-baseline 同一門檻，N-3）；機器照 U8 已定三層（Docker Linux 容器判定、Intel Mac 原生只作開發參考、Linux 實機在出問題且允許時確認；CI runner 不用）；超預算處置：<1.5 倍＝報告、使用者決定；≥1.5 倍＝停、重排（§8 RC-1）。LIM-2 的三條路（Parallel 加守衛、拆 World、葉子函式內 job）是重排時的選項，不在超標時自動採用。本機重 GPU／大記憶體量測先講影響並取得同意（memory warn-before-heavy-local-runs；BU-1～BU-6 都是 headless、記憶體小）。

### 取捨（§6）

三案的 BU-1 相對係數不同（風險 ×1.15／＋0.25 ms；價值 ×1.25／＋0.25；成本 ×1.25／＋0.5 且 ≤2.0 ms）。採 ×1.25／＋0.25／絕對 2.0 ms：×1.15 在沒有分母時過嚴（第一個 host 的 Kernel 額外成本就是要量的東西），＋0.5 ms 在 16.67 ms 的 tick 下與 ×1.25 重疊無意義；絕對上限保留成本案的 2.0 ms 當「不論基線多小都不能超過」的護欄。放棄的代價：若今天的 tick 遠小於 1 ms，×1.25 允許的絕對增量很小，可能因量測雜訊誤判——所以同時要求 5 次中位數與 ≤10% 散佈（pvp-baseline）。

---

## 7. 已定事項（原未決；使用者 2026-11-13 逐項定，HANDOFF `v8-arch-migration-plan-undecided`）

| # | 事項 | 選項（原文，供對照） | 三案的建議 | 已定（使用者 2026-11-13）：選項＋理由；其後是原統合建議（供對照） |
|---|---|---|---|---|
| U1 | **os RC 9（D78）與頁保護第三層** | (a) 現在採：第三層列為名指 Feature（RC 9 末句出口），開例外、RC 9 原文不動，排 pageguard-os；(b) 改寫 RC 9（基盤可依預見成長）；(c) 延後到 G2a 之後再定（數得出 PvP 的 Exclusive 欄數、量得到 (b) System 成本），決定前 world-exec 用「一般配置＋可換獨立頁的介面」；(d) v1 不採，LIM-3 的 L1／L6 維持 | 風險 (a)；價值 (c) 傾向 (d)；成本 (c) | **已定（使用者 2026-11-13）**：**(c)** 延後到 G2a 後再定；world-exec 的欄配置留「一般配置／可換獨立頁」介面；os RC 9 不動——理由：決定它的兩個數字（PvP 的 Exclusive 欄數、(b) System 成本）G2a 才有。 原統合建議：**(c)**：第三層不在任何 host 的關鍵路徑；L1 形狀至今無觀測案例；成本（Apple Silicon 每欄 16 KiB、(b) System 2×\|欄\|）要等 PvP 的欄數才有根據；(a) 的好處只有場次覆蓋多一種形狀。代價：spike 步驟與 `spikes/` 多留一段（ci-spike-retire 等 G2a）；若在 G2a 後選採，Delta-4 要在那時走 §3 同意。選 (a) 不影響關鍵路徑 |
| U3 | **先穩定版還是邊做邊遷** | (a) 每個 host 整個遷完才合併：match-sim＋match-host 同一 PR 線、client 三批同一 PR 線，master 一直可發佈，穩定點 S1＝Match 在 kernel 上（Client 仍舊，wire 相容）、S2＝Client、S3＝工具與 framework 刪；(b) 逐批合併、有界並存：match-sim 先進 master，舊 `PvpMatch` 與新 (c) plugin 並存到 match-host 結束；(c) 長命分支全部 host 遷完才合併 | 風險 (a)；價值 (b)；成本 (b)（match-host 不在關鍵路徑） | **已定（使用者 2026-11-13）**：**(a)** 每個 host 整個遷完才合併；**master 不得新舊並存**——理由：退役舊文件就是為了防過渡期，並存會造成版本分裂。 原統合建議：**(a)**。(b) 讓 master 上同時有兩種動詞（spec K.2 第 8 條、H-7「一個 host 不會一半在機制上」）且兩路都要產同一 golden、任何修正都要雙寫；(c) 讓成本閘門與 L1 反饋都延後。代價：Match 的 PR 線長（約 113～245 h，含測試、守衛與突變的去處與守衛去處判斷）、L1 反饋集中在線末——補救＝分支上每批仍跑 draft PR 的 L1（memory draft-pr-l1-skip：手動 dispatch） |
| U4 | **host 順序與閘門嚴格度** | 順序三案一致：Match → Client → 工具。閘門：(a) 一段，在 match-host 後（嚴格讀 AGENTS §2「一つの host」）；(b) 一段，在重放 host（match-sim）後；(c) **兩段**：G2a（重放 host）放行 Kernel／plugins 側與 Client 共用的工作（rollback-extract、engine-wrap、plugins-frame），G2b（真實 Match 行程＋移行の手間）才放行 client-sim／client-link／client-frame 與 Editor 線 | 風險 (a)；價值 (b)；成本 (b)（嚴格讀法多 60～115 h） | **已定（使用者 2026-11-13）**：**(c)** 兩段閘門：G2a 放行 Kernel／plugins 側共用工作、G2b 放行 Client；順序 Match → Client → 工具。 原統合建議：**(c)**。第二個 host 的遷移不在真實 host 量過前開始（守 §2 字面），但 Kernel 後半與 Engine 的「包」本來就要做、與 host 無關，先做不違反「廣げる前に」。代價：若 G2b 不過而 G2a 過，2' 階段約 **70～153 h**（rollback-extract 25～46＋wrap-render 20～55＋wrap-assets 9～20＋plugins-frame 16～32；assets-core 已移到 G2a 之前不計；第二輪對抗 N2-1 修正 67～149 的加總錯為 66～145，架構師判定後加守衛去處判斷為 69～151，第三輪 rollback-extract 再加「新增」守衛的判斷 1～2）的工作可能要改；這些批的產出都在 kernel／engine 內、不依賴 Match 的數字，改動局部 |
| U5 | 編譯失敗測試的形狀（ci-readiness §4-2） | (a) CMake `try_compile` 預期失敗（J-10 原文）；(b) trait＋doctest（spike 做法）；(c) 核心 3 條（J-10 的 lambda 捕獲、`World&` 參數、回傳型別）用 (a)，其餘 (b) | 風險 (c)；價值 (b)＋一個 try_compile 金絲雀；成本：scaffold 兩種樣本後選 | **已定（使用者 2026-11-13）**：**(c)** 核心三條用 `try_compile`、其餘 trait＋doctest；kernel-scaffold 先做兩種樣本過 Windows L1 後選。 原統合建議：**(c)**，在 kernel-scaffold 以兩種樣本的 windows-x64 結果定案：(a) 無先例、Windows 未驗；(b) 已在 spike 三平台過。若 (a) 在 Windows 不穩，全用 (b) 並記 J-10 原文的差異 |
| U6 | **Life 的寫者形狀**（dryrun 的結論要核准） | (a) Life 唯一寫者在 tick 開頭（處理到期復活），死亡由 `ResolveActions` 寫獨立中間欄 `DeathRecord`、同 tick 之後的 System 與 digest 讀它、下一 tick 開頭併入；(b) 事件＋下一 tick 套用（死亡晚一 tick 進 snapshot，golden 變）；(c) 拆 Life 為 `LifeState`（tick 開頭寫者）與 `DeathRecord`（裁決寫者），digest 組裝讀兩欄得同位元組；(d) 同一 System 兩個掛點——**不可行**（spec B.1：同一 schedule 掛兩次是 D-E19） | 風險 (a)／(c) 由 dryrun 定；價值 (d)；成本 (d) 或其他保 T=35 的形狀 | **已定（使用者 2026-11-13）**：**(c) 的事件形**：`LifeState` 單一寫者在 tick 開頭＋`DeathEvent` 事件由 `ResolveActions` 發；排除 (b)；**不要中間狀態欄**（理由：同步負擔）；事件走 spec F 的生命週期、同步點清掉；回滾重放時事件重發、不另存；G1 謂詞改「`LifeState` 單一寫者在 tick 開頭、`DeathEvent` 由 `ResolveActions` 發、snapshot／digest 在其後讀」；match-sim 實測 T=35／R=215、golden 不變。 原統合建議：**(a) 或 (c) 由 dryrun 的 finish 結果選**；判準分兩段：dryrun 判 G1 的 Plan 謂詞（§2.1 (iii)：`LifeState` 唯一寫者在 `StepMovement` 前、`DeathRecord` 寫者是 `ResolveActions`、讀者在其後同一 tick、D-E19＝0、D-E2＝0）；match-sim 在新重放 host 上實測 T=35／R=215 與 golden 37 不變（第一輪對抗 S-1／S-3：D1 原測試不可能「不改一字而過」）；K.2 第 9 條不觸發。(b) 改 golden 不建議；(d) 是啟動錯誤。這是 digest 相關的產品決定，所以列未決 |
| U7 | refusal 回覆時機（spec H.4.1 (b)） | (a) 接受晚最多一 tick（Gateway 與 Client 測試照舊過即可）；(b) 保持同步回覆（IO 執行緒要讀權威＝違 I-1，不可行）；(c) Gateway 先回「已收」、結果晚到 | 三案 (a) | **已定（使用者 2026-11-13）**：**(a)** 接受最多晚一 tick，實作依據 bevy_replicon：IO 服務只搬位元組（排程開頭 insert、末尾 drain）、Judge 在 tick 內的 (c) System 判、refusal 寫 per-connection 的 Egress Resource {tick, 動作序號, 原因}、Egress 同 tick 末尾送出；不做 ServerTick 門與 tick 前綴對齊（pv10）；前提：match-host 宣告第一步查 pv9 與 Gateway 測試對同步回覆有無規定。 原統合建議：**(a)**，前提是 match-host 事前宣告第一步查證 pv9 契約與 Gateway 測試對時機沒有規定；有規定→停、回來再定 |
| U8 | 成本量測機器 | (a) 使用者的 Linux 機器（Match 主戰場）；(b) Intel Mac 代理；(c) 兩者，判定用 (a)、(b) 當開發參考；不用 CI runner | 三案同向 | **已定（使用者 2026-11-13）**：**三層**：Intel Mac 原生（開發參考）→ Docker Linux 容器（閘門判定；Match headless 不需 GPU；基線同一映像；絕對數字標「容器內」）→ Linux 實機（出問題且允許時確認）；CI runner 不用；Dockerfile 照 CI toolchain 版本，由 pvp-baseline 順手建並寫進報告。 原統合建議：**(c)**；Linux 不可用時判定用 (b) 並在報告寫範囲の限界 |
| U9 | LIM-5 造成的 golden 重設基準 | (a) 每個情境逐條照 §14.4 1～5 交使用者核准（事前宣告列可能受影響的 golden 名、驗證者複核）；(b) 事先核准「只限 Join／Leave 差一 tick 的情境」的重設規則 | 風險 (a)；價值、成本：事前列名＋§14.4 | **已定（使用者 2026-11-13）**：**(a)** golden 重設逐條照 §14.4 1～5 交使用者核准，驗證者用不同模型。 原統合建議：**(a)**。事先空白授權等於結果後放寬；事前宣告仍要列出可能受影響的 golden 名（結果後不加名） |
| U10 | `spikes/` top-level 與 CI spike 步驟 | (a) 條件滿足時刪（ci-readiness §2.6；`.github/` 由使用者改）；(b) 保留 `spikes/` 當未來 spike 位置 | 三案 (a) | **已定（使用者 2026-11-13）**：**(a)** 條件滿足時刪 `spikes/kernel` 與 CI 步驟（`.github/` 交使用者）；時點隨 U1；刪時在 target A.6／驗證節與兩份 spike 報告開頭留 git 永久連結 `https://github.com/yojinn-io/GYO-Engine/tree/<刪除前 sha>/spikes/kernel`，closeout 判定含「引用存在且連結有效」。 原統合建議：**(a)**（AGENTS §7 不留空目錄）；時點隨 U1：選 (c)／(d) 時 world-exec 後即可，選 (a) 時等 pageguard-os |
| U11 | **並行線的數量** | (a) 開「產品-Match」「產品-Client」兩條產品遷移線（產品前置線升格）與「Engine／plugins 線」，控制權仍在架構師主會話；(b) 維持現有四條（掃描器、產品前置、CI、文件），產品批串行、Engine 包進架構師線 | 成本 (a)（成本案原估關鍵路徑短 80～155 h，對抗前數字）；風險、價值未列 | **已定（使用者 2026-11-13）**：**(a)** 分階段開三條常設線：G1 過開「產品-Match」、G2a 過開「Engine／plugins」、G2b 過開「產品-Client」；原則：能並行就並行、不讓產品線卡著；**某線前置齊了不等閘門湊整**。 原統合建議：**(a)**。第二輪重算（N2-13）：(a) 的關鍵路徑 331～692 h（§2.3）；(b) 以兩條串行線（架構線含 Engine／plugins 批、產品批一條）做 list scheduling 估約 475～968 h，(a) 短約 **144～276 h**（估算非最佳排程，只作量級；§2.3）。代價：使用者同時要看的關卡變多（每批事前宣告與合併）；§8 RC-7 管並行衝突 |
| U12 | §6 預算數字與標「推定」的工時的核准 | 照 §6 候選，或改數字 | 三案都要求事前核准 | **已定（使用者 2026-11-13）**：等 pvp-baseline 的數字，§6 改成「今天的值 × 倍數」再一次核准；時點＝a6-revision 完成後、match-sim 宣告前；Kernel 工時在 G1 成立時重估（RC-3）。 原統合建議：在 match-sim 宣告前核准（§14.1）；§6「出所」欄標 **推定** 的每個數字都沒有根據（第一輪對抗 S-15），請逐個看，不只 BU-1 的 2.0 ms 與 8.33 ms；§2.2 標「推定」的工時（Kernel 五批全部、測試、守衛與突變的去處）同樣是無根據的估計，RC-3 的第一次重估在 G1 成立時；pvp-baseline 的分母出來後若尺度明顯不同，由使用者在看遷移結果前改一次 |
| U13 | 目標 DI 的核准時點 | (a) a6-revision 一次核准 world／system-contract／scheduler／runtime-composition／assets／determinism／error-handling／naming 的候選 DI 與 kernel DI 頁；(b) 每部件在其第一批開工前各核准一次 | 成本 (a)；風險、價值把 DI 頁放 a6-revision | **已定（使用者 2026-11-13）**：**(a)** a6-revision 一次核准全部目標 DI；清單分「跨部件（先讀）」「各部件」兩層並附 legacy 條文對照。 原統合建議：**(a)**：一次讀完、關卡少；AGENTS §3「機能を積み上げる前」對 registry-schedule 已足。代價：核准後再改是 Delta |
| U14 | Loading 的 ready 在 v1 的實現 | (a) 「Welcome 隱含 ready」（Match 只在 `LoadingState = Ready` 後接受 join；Client 等 Welcome），wire 不變；(b) 新 wire 事件（pv10） | 價值 (a)；成本 (a)（ready 事件屬 pv10，不在遷移內）；風險 (a) | **已定（使用者 2026-11-13）**：**(a)** Welcome 隱含 ready；**記為未結事項並標「可能的設計缺陷」**（v1 的 Match 玩家 stage 不生成初始成員、H.2b 第 4 點只在合成測試成立）；HANDOFF 的「未結事項」節記它，pv10 補 ready 事件時回頭驗，closeout 前不得刪。 原統合建議：**建議 (a)，但有一個第一稿沒標出的衝突（第一輪對抗 S-8），請使用者看過再定**：spec H.2b 第 4 點與 target/runtime-composition §3a／DI 3 的形狀是初始成員由 Loading 的「玩家」stage 經 `ApplyMembership` 生成；照 (a)，Match 在 ready 前收不到 join，**v1 的 Match 玩家 stage 永遠是空的**，初始成員全走 FixedUpdate 的 `ApplyMembership` 掛點，loading 批宣稱的「初始成員走同一條路」在真實 host 上不被跑到（只在合成測試成立）。對 golden 的影響：LIM-5「初始成員經 Loading 玩家 stage」的註要改寫；H.2b 第 5 點說以對局開始為基準的 golden 預期不變，但那是以玩家 stage 生成成員為前提——實測，不先假設（match-sim 判定 (1)）。選 (a) 時 loading 與 match-sim 的範囲の限界已加這一句。(b) 要改 wire（§8 RC-9，不在本計畫內）。Client 的 Loading manifest 若既有 wire 不夠表達→client-sim 停下報告，進 pv10-requirements（§8 RC-9；plan-dryrun 的 N-11 查證先給答案） |

U1、U3～U14 已由使用者在 2026-11-13 逐項定（HANDOFF `v8-arch-migration-plan-undecided`；各列理由在決定行）；U14 另記在 HANDOFF「未結事項」節。U2（plugin 選擇是資料還是程式）已由使用者決定為資料（`v8-arch-plugin-selection-data`，2026-11-12；§1.1、§4 Delta-5），自本表刪除；編號保留不重用。

---

## 8. Replacement Criteria（這份計畫什麼時候該重排）

符合任一條：停在當前批的下一個關卡，不在計畫上補丁，回到規劃（統合者重寫受影響的階段）並交使用者；「計画の前提を見直す（範囲を変える）」永遠是選項之一（AGENTS §14）。

| # | 條件 | 重排什麼 |
|---|---|---|
| RC-1 | **成本**：G2a 集合（§2.1：BU-1 Match tick、BU-2 Kernel 開銷、BU-4 World 位元組、BU-6 重放到 ready）或 G2b 集合（BU-1′ 真實行程 tick、BU-4 穩態 RSS、BU-6 真實行程到 ready）或 client-frame 的 BU-7（Client 幀）任一項 ≥ §6 預算 ×1.5 | 該閘門之後的階段；選項（交使用者）：Kernel 熱路徑最佳化批、World 儲存換結構（spec K.2 第 6 條）、縮小 (c) 範圍、LIM-2 的三條路（Parallel＋J-27 守衛／拆 World／葉子 job） |
| RC-2 | **G1 失敗**：Plan 指紋三平台不同或 J-44 不過，修兩次不收斂；或 K.2 第 3 條在合成測試就出現 | 全計畫（spec K.2 第 1、4 條；可能退回顯式宣告） |
| RC-3 | **估時失準**：**第一次重估在 G1 成立時**（registry-schedule 的實際／估計比落在 0.5～1.5 以外→重估 Kernel 其餘四批）；第二次在第一個 host（match-sim＋match-host）完成後：各批實際／估計比的中位數落在 0.5～1.5 以外（分母與 G2b 第 (9) 項同為事前宣告核准時寫明的估時上限；G2b (9) 用兩批合計之比，本條用各批比值的中位數，兩者並列、不互代）；或 Kernel 本體**五批**（registry-schedule、world-exec、loading、threading、rollback-extract）實際合計 >估時上限（**186 h**，§2.2）×1.5 | 以實測重估剩餘各批並重算關鍵路徑（不只改數字，看依賴是否變） |
| RC-4 | **判官失效**：dryrun 有 digest 欄必須 Shared、非資料流順序邊 >50%、Life 形狀的 Plan 謂詞（G1 (iii)，U6 已定的形狀）不成立；或 match-sim 在新重放 host 上量到的 T／R 與 -detect 現況不同；或 match-sim／match-host 的 golden 37 在原因分析兩輪後仍不逐位元且差異不在事前列名的 LIM-5 集合 | 階段 2（重劃區／重拆欄；K.2 第 3、10 條）；回到「是不是在為舊形狀找位置」（target/README 檢查項） |
| RC-5 | **遷移成本**：任一 host 的實測工時 >1.5×估計且大於重寫估計；或遷移到一半必須讓兩種動詞長期並存（K.2 第 8 條；Match 兩批合計 >兩批事前宣告核准時寫明的估時上限合計 ×1.5 即觸發——宣告前以 §2.2 的上限 **245 h**（47～103＋66～142 的上限）×1.5＝**367.5 h** 估） | 該 host 之後的批；提「修還是重建」（memory repair-vs-rebuild） |
| RC-6 | **共用件撐不住／Host 又長回來**：net-link、sdl-window-runner、editor-dialogs 任一在第二個消費者時要加消費者專屬分支；第二個 host 遷移時要改 Kernel 本體或 runner 骨架；H.7 以外的協調邏輯在兩個 host 重複（K.2 第 7 條） | 拆 plugin 並記重工；同一形狀發生兩次→重排 plugin 邊界（audit §10.4）／Kernel 介面批 |
| RC-7 | **並行衝突**：兩條並行線在同一檔案的衝突解決累計 >2 h，或一批的受理因另一線的合併而重跑兩次以上 | 該兩線改串行（U11 回 (b)） |
| RC-8 | **Delta 增生**：新發現的 Architecture Delta 不在 §4 六項之內累計達 2 個（第一輪對抗 S-2 的 Delta-6 已補進 §4，不計） | §4 與受影響的批 |
| RC-9 | **前提改變**：需要改 wire（pv 版本；含 ready 事件、refusal 時機被 pv9 規定、manifest 不夠表達）、使用者恢復任一暫停項（lag win1、帳本、兩通道、DET）、或使用者改變 2026-11-13 已定的 U1／U3／U4／U11（`v8-arch-migration-plan-undecided`） | 受影響的階段；依新選擇重算 §2.3（各 U 已寫差額） |
| RC-10 | **基盤不穩**：每加一個 System 或 plugin 都要重新驗證 Kernel 本體的守衛（突變名單以外；K.2 第 13 條）；或 threading 的 TSan／Windows 壓力出現逾時且觀測顯示卡點在 Kernel／runner 設計（不是 runner 機器） | Kernel 批重開，host 批暫停 |
| RC-11 | **總量**：任何時點「已完成實際＋剩餘估計上限」>1,299 h ×1.5 | 停下報告，重看範圍 |

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
| Windows 觀察驗收 | 測試內 N＝100／200 | dispatch 10 次 | dispatch 10 次 | 測試內重複為批內門檻；dispatch N 次（N 由基準發生率定）只用在觀察 (c)、收口在 client-link；closeout 60 run |
| 並行線 | 未列 | 未列 | 開兩條產品線＋包線 | 採（U11） |
| 估時合計 | 545～1,070 h | 530～1,050 h | 560～1,130 h | 632～1,299 h（不含條件批；第一輪對抗前 560～1,140；第一輪後 594～1,222（測試移植 44～88＋第一輪對抗 B-4／N-2 重算）；第二輪 +30～61（asset-io 服務 4～8、pvp-diagnostics 的測試去處 2～4、F 換容器 1～3、守衛去處判斷 23～46）；第三輪 +8～16（「新增」守衛 6 批的判斷 6～12、改非 inline 2～4）） |

---

## 對抗檢查處置記錄（第一輪，Opus／xhigh；給使用者的審計線）

第一輪報告：主 checkout `build/evidence/kernel-migration-plan/adversary-report.md`（blocker 4／should 19／nit 16）。處置由架構師定（HANDOFF 決定行 `v8-arch-migration-plan-round1`），修訂由子代理（Fable／high）套用。每條寫「改在哪節」；「採建議」＝照報告該條的建議改。

| 編號 | 要點 | 處置 | 改在哪節 |
|---|---|---|---|
| B-1 | §6 預算編號與引用錯位一格 | 全稿統一用 §6 的 B-n；G2a／G2b／RC-1 各寫一次完整集合並附名稱；§0 加判定 A8 | §0 A5、A8；§2.1 閘門定義；§2.2 各列；§3.5、3.8、3.10、3.14、3.15、3.16、3.17、3.21、3.23；§5 第 8、14 列；§6 前言與表；§8 RC-1 |
| B-2 | H-7 掃描的 ctest 放 `tests/common/`，正例與基線是產品 host | 採建議：共通層只放掃描器、規則資料、合成檔自測與突變；owner 各自在 `tests/<owner>/` 註冊；nothing to scan 通過；加 Removability 判定 | §2.1 樹；§2.2 h7-scanner 列；§3.4 全節；§3.16 Architecture |
| B-3 | PvP 專屬 `MovementDiag`／P8／P12 進 `plugins/trace_diagnostics` | 採建議：plugin 只收通用服務；PvP 部分進新批 `v8-product-pvp-diagnostics`（owner object_fps_pvp，位置產品線提、架構師核）；加 Product Boundary 判定；§4 補 Product Ownership | §0 批次數；§2.1 樹；§2.2 diagnostics-service 與 pvp-diagnostics 列；§3.13 重寫；§3.13b 新增；§4 Delta-2 第 6 項；§5 第 23 列 |
| B-4 | match-sim 需要的真資產管線排在 G2a 之後 | 採建議第一項：拆成 `v8-arch-assets-core`（match-sim 前）與 `v8-arch-engine-wrap-assets`（G2a 後）；重算 §2.2 依賴與 §2.3 關鍵路徑 | §0；§2.1 樹（★ 重標）；§2.2 兩列與 match-sim 依賴；§2.3；§2 取捨；§3.8 限界；§3.14；§3.19 新增；§3.19b 重寫；§4 Delta-6。架構師第二輪：assets-core 只依賴 world-exec／loading（不依賴 threading、plugins-foundation），IO 執行緒服務形式寫進 §3.9 範圍與 match-host 依賴 |
| S-1 | 「既有測試不改而過」與 API 重寫矛盾；約 360 個 TEST_CASE 移植未排 | 採建議四點：每個產品批加「測試移植」；D1／D3 改新 host 實測；client-sim (3) 改「同一劇本重現」；移植工時進 §2.2 與 RC-3 分母 | §3 共通；§2.2 產品批與工具批列；§3.14 (2)(8)；§3.16 (5)(10)；§3.21 (3)(8)；§3.22；§3.23；§3.25～28；§5 第 17、18 列；§7 U6 |
| S-2 | Engine「包」沒進 §4 七項；`AssetManager` 消失時點早於消費者 | 採建議：加 Delta-6 七項；`AssetManager` 在 framework-delete 刪、之前以期限明確的轉接存在；掃描範圍寫明 | §4 Delta-6 新增；§3.18、3.19、3.19b Architecture；§3.29；§3.23 (8)；工具 ui 列；§0 A3；§8 RC-8 |
| S-3 | G1 的 T／R 在空函式體 dryrun 量不到；非資料流邊靠主觀 | 採建議：G1 改 Plan 上可機械檢查的謂詞；T／R 留 match-sim；資料流邊定義由描述算、工具輸出比例 | §2.1 G1；§3.5 Contract／判定（匯出比例）；§3.6 判定；§3 取捨 Life；§7 U6；§8 RC-4 |
| S-4 | 參數型別分配使幾批判定當批達不到；J-49 在 loading | 採建議：registry-schedule 定義全部參數型別的描述與 Contribution；J-49 只留 rollback-extract；loading 用 J-48、J-51、DI 3 等價測試 | §3.5 Contract；§3.8 判定；§3.17 判定；§2.2 三列 |
| S-5 | 依賴欄漏邊 | 補兩條（wrap-render→threading；plugins-frame→rollback-extract）；wrap-assets 經 wrap-render 間接依賴 threading；assets-core 的那條**不採**（架構師第二輪：只依賴 world-exec／loading，見 B-4 列）；重算 §2.3 與 U4 2' 工時 | §2.2 依賴欄；§2.3；§3.18、3.19、3.20 Architecture；§7 U4 |
| S-6 | a6-revision 把 determinism §7 與 E0～E5 搬 target，碰暫停項 | 只搬 §1 三級、§2 區表、§4 區表；§7／E0～E5 留 legacy、HANDOFF〈暫停〉記一行、closeout 定去處 | §3.1 Contract／範圍／判定 (9)；§3.32；§5 第 33 列；§2.2 兩列 |
| S-7 | 行號引用的正規式會掃到 119 行程式碼行號 | 正規式限定 legacy 檔名；基準件数 13；正例驗證、不丟 stderr | §3.1 判定 (4)；§5 第 34 列；§2.2 a6 列 |
| S-8 | 「Ready 後才接受 join」與 H.2b 第 4 點衝突未標出 | 採建議：U14 明寫衝突與對 golden 的影響；§1.3 改「待 U14」；選 (a) 時 loading 與 match-sim 的限界加一句 | §1.3；§3.8 Feature／限界；§3.14 限界；§3.16 限界；§3.21 Contract；§7 U14 |
| S-9 | 主執行緒 Stop 後讀 `TickCount()`，「A.2 允許」是錯的 | 採建議：runner 在最後一個交換點把 domain tick 複製進 Host 統計，主執行緒讀複本；刪「A.2 允許」 | §3.16 範圍；§5 第 20 列 |
| S-10 | windows (c) 收口 10 次無檢出力、漏 client-link | 採建議：先算基準發生率再定 N（無改善卻 0 次 ≤5%）；收口移到 client-sim＋client-link 之後、client-link 加同一判定；前後同一宣告 | §2.1 樹；§2.2 兩列；§3.21 (4)；§3.22 (5)；§3 取捨 Windows；§5 第 26 列；附表 |
| S-11 | net-link 可能把 PvP wire 知識帶進 plugins | 採建議：plugin 只處理位元組、到達時刻與交換；種類判別與 proto 解碼在產品側；加反向刪除驗證 | §3.12 Contract／Architecture／判定；§3.16 Architecture；§3.22 Architecture；§4 Delta-2 第 6 項 |
| S-12 | 工具三批與 closeout 標下段違 navigator | 四個工具批與 closeout 改上段（不拆小批，理由在 §2 取捨） | §2.1 樹；§2 取捨；§3.25～28 段；§3.32 段 |
| S-13 | 幾處檔位低於 gearbox 定義 | 審查者一律至少 high；scaffold 與 framework-delete 實作改 high 並寫明 low 的逐字子步驟 | §2.1 樹；§3.2、3.3、3.25～28、3.29、3.30 段 |
| S-14 | 門檻對抗檢查者可能與作者同模型 | 統一寫「與宣告作者不同的模型」 | §3 共通；§3.15 段；§2.1 樹；§0 A4 |
| S-15 | 工時與預算缺出所 | 每個數字標「出所」或「推定」；Kernel 工時不宣稱公式、全標推定；RC-3 寫第一次重估時點 | §2.2 出所規則、各列、合計段；§3 各批估時；§6 加「出所」欄；§7 U12；§8 RC-3 |
| S-16 | 件数の突き合わせ本身不一致 | 重算：34＋2＋1＋1＝38 一致；a6-revision 判定 (2) 改 20 列並逐列列出 | §0；§3.1 判定 (2)；§5 末行 |
| S-17 | `TraceMovement` 改 tick 是語義變更卻寫「格式不變」 | 採 (a)：用被記錄的 boundary 輸入 `HostClockSample`，牆鐘語義保留；任一欄語義變則升版並列消費者 | §3.13b Contract／判定；§5 第 23 列 |
| S-18 | PvP 欄位表搬 target/world 是產品知識進 Engine | 採建議（修正架構師處置）：表放產品的 target 文件；target/world 只寫通用規則 | §3.1 Contract／判定 (8)；§3.14 Contract；§5 第 35 列 |
| S-19 | G2b 沒有「移行の手間」 | G2b 加「match-sim＋match-host 實際／估時 ≤1.5」，不成立照 RC-3／RC-5 停 | §2.1 G2b；§3.16 (9) 與風險；§7 U4；§8 RC-3 |
| N-1 | 守衛號重複與誤標 | J-9 只在 scaffold（world-exec 改 J-1～J-8）；threading 改 J-6 的 ownerThread 半；ci-spike-retire 條件 (a) 改 registry-schedule 的 J-12 | §2.2；§3.7；§3.9；§3.30 |
| N-2 | 算術 | M3／M4、2' 階段、match-host、RC-3 全部重算 | §2.2 合計；§2.3；§7 U4；§8 RC-3、RC-5 |
| N-3 | 穩定性門檻兩處不一 | 統一 ≤10% 散佈 | §3.2；§3.15 風險；§6 量測紀律 |
| N-4 | plugins-foundation 的合成 plugin 位置、`project.json` 升版策略 | 合成 plugin 與合成 `project.json` 放測試夾具；五個 `project.json` 同批升版、讀取端只認新版 | §3.11 判定 (2)(6)；§2.2 列 |
| N-5 | `-ffp-contract=off` 對 plugins target | plugins-foundation 加 configure 檢查 | §3.11 判定 (7)；§2.2 列 |
| N-6 | naming DI 3 後綴掃描只派 wrap-render；`Runtime` 型別名衝突 | 後綴掃描掛 scaffold、registry-schedule 判定引用；`Runtime` 命名在 registry-schedule 處理 | §3.3 判定；§3.5 Contract／判定 |
| N-7 | runtime-composition DI 3 的三條守衛沒派批 | Startup／Loading 兩條→loading；Peer 不掛 Player→match-sim | §3.8 判定；§3.14 Contract／判定 (5) |
| N-8 | legacy-impact §9 第 8 點兩項沒對應 | input `<chrono>`→wrap-render 判定；trace RC 1→diagnostics-service 範圍 | §3.18；§3.13 |
| N-9 | match-host 引用暫停的 pv10 複製層當 Feature | 只引用觀戰者（Peer）；pv10 複製層明寫不列 | §3.16 Feature |
| N-10 | 計畫替主會話指定模型與檔位 | 「主會話 Fable／high」改「主會話（模型由使用者定）」；加「需使用者同意的指派」清單 | §2.1 樹；§2.2 末；§3.1 段；§3.15 段；§0 A4 |
| N-11 | manifest 風險可提前查證 | plan-dryrun 加查證項（Welcome 已有的 arena／animation digest 欄） | §3.6 判定；§3.21 限界；§7 U14 |
| N-12 | `SdlInput` 後端歸屬不明 | 程式碼搬 sdl-window-runner（plugins-frame）；wrap-render 只定介面 | §3.18；§3.20；§4 Delta-2 第 6 項、Delta-6 |
| N-13 | 基準含未合併的 `4164635a` | 寫明同一分支、同一 PR | 檔頭 |
| N-14 | a6-revision「不擋開工但擋宣告」矛盾 | 宣告未核准＝不開工 | §3 共通；§3.1 若失敗；§3.4 若失敗 |
| N-15 | 白名單改登等於改判定 | 改登寫進事前宣告並照 §14.4 | §3.16 (1)；§3.13b (4) |
| N-16 | 真實行程基準由誰、何時量 | pvp-baseline 在看新路徑結果前量真實 Match 行程基準 | §3.2 範圍／判定；§6 前言、B-1′／B-4／B-6 量測方法、量測紀律 |
| （另） | U2 plugin 選擇已定 | 自 §7 刪除；Delta-5 寫已定並引決定行；plugins-foundation 範圍寫明 | §0；§1.1；§4 Delta-5 差分；§7 末 |
| （追加） | B-1′「overrun 0 次」的保險句：若 pvp-baseline 量到今天的 overrun 非 0，改為「≤基線」並經 U12 | **修訂者追加、架構師核**（不在第一輪處置清單內） | §6 B-1′ 出所欄 |
| （追加） | §5 分類法改為「每列只算一類、以第一個落點為準」 | 修訂者為使件数一致而改，架構師第二輪接受 | §5 末行 |

## 對抗檢查處置記錄（第二輪，Opus／xhigh；給使用者的審計線）

第二輪報告：主 checkout `build/evidence/kernel-migration-plan/adversary-report-r2.md`（blocker 1／should 11／nit 14；第一輪 39 條中 34 條已處置、5 條只改一半：B-1、S-2、S-6、N-2、N-4）。處置由架構師定（HANDOFF 決定行 `v8-arch-migration-plan-round2`；守衛規則是使用者決定 `v8-arch-guards-migrate-with-code`），修訂由子代理（Opus／high）套用。「照建議」＝照第二輪報告該條的建議改。之後第三輪限定範圍的對抗（§0 A9）。

| 編號 | 要點 | 處置 | 改在哪節 |
|---|---|---|---|
| B2-1 | 沒吸收 `v8-product-kernel-prep-zones-gate`：match-sim 開工條件「確認清單 0」照計畫永遠不成立 | 開工條件與判定 (3) 改「J-17 確認清單＝核定的 7 處殘留（RESIDUAL 釘住），不是 0」；24 處逐處分派（B 5→assets-core 定 loader 契約、match-sim 消費；F 2→match-sim 換有序容器、digest／golden 零差；D 3→client-sim；E 14→pvp-diagnostics 改句柄、Client 側呼叫點歸 client-sim）；namespace 改 `fps::pvp::sim`（inline，v1；重寫批改非 inline）；`zone-c-rewrite.json` 的去處（照建議） | 檔頭基準；§0 另兩組輸入；§1.1；§1.3；§2.1 樹；§2.2 assets-core、pvp-diagnostics、match-sim、match-host、client-sim 列；§3.13b；§3.14 Contract／Architecture／範圍／判定 (3)(8)(9)／風險；§3.16 範圍／判定 (1)；§3.19；§3.21 判定 (9)；§5.1 新增 |
| S2-1 | （B-1 只改一半）B-n 三套編號同字形，A8 字面不成立 | 預算編號改 `BU-1～BU-8`（含 BU-1′），全稿同步；spec 的寫「spec B-n」、第一輪對抗的寫「第一輪對抗 B-n」；A8 改掃 `BU-\d`（Python、正例先命中） | §0 A5、A8；§1.1；§2.1；§2.2；§2.3；§3 各批；§5；§6 前言與表；§8 RC-1 |
| S2-2 | （S-6 只改一半）a6 判定 (9) 的 `E[0-5]\b` 與要搬的區表衝突 | 判定 (9) 改「target/determinism 不含 legacy §7 缺口清單的條目（E0～E5 作為計畫項）」、以章節比對；§2／§4 區表照搬（E-n 引用照原文） | §3.1 Contract／判定 (9)；§5 第 33 列 |
| S2-3 | （S-2 只改一半）closeout 的 assets 搬回與 `AssetManager` 活到 framework-delete 矛盾 | assets 文件搬回時點照 wrap-assets，附註 `AssetManager` 活到 framework-delete、DI 1 對全 repo 成立在該批 | §3.32 範圍 |
| S2-4 | 正式的 `AssetInbox` IO 服務沒有批負責 | 架構師第二輪 (b)：由 plugins-foundation 的 headless-runner／asset-io 服務擁有（Kernel 不擁有執行緒），依賴 threading 的契約，match-host 消費；threading 只留交換點原語＋合成 IO 執行緒案例；plugins-foundation 加 asset-io 服務 4～8 h（推定） | §2.1 樹；§2.2 threading、plugins-foundation、match-host 列與合計；§3.9 範圍／判定／風險；§3.11 Feature／Contract／分段交付／範圍／判定 (11)／估時；§3.16 範圍；§3.20 Contract；§4 Delta-2 第 6 項與差分、Delta-6 第 3、4、6 項與差分 |
| S2-5 | loader 由誰登記與 target/assets §1 第 3 點衝突 | 架構師第二輪 (a)：照 target 由 plugin 登記，撤回「host／產品直接登記」；match-sim 依賴 plugins-foundation 的最小形；assets-core 仍只依賴 world-exec／loading；`engine/model` 只提供 loader 函式（`gyo_expect_model_links` 不變）；Client／工具側 model loader 的 plugin 暫放 plugins-frame（照建議「寫明登記者與批」） | §2 取捨；§2.1 樹；§2.2 assets-core、match-sim 列；§2.3；§3.14 Contract；§3.19 Contract／Architecture／範圍／判定；§3.20 Contract；§4 Delta-6 |
| S2-6 | Kernel 與 `engine/asset` 的模組邊界沒寫 | `Assets<T>`／請求狀態 Resource／套用 System 在 `engine/asset`、依賴 `engine/kernel`；kernel 只提供型別無關的暫存與游標、不知道 asset；J-51 在 Kernel 端用合成型別；Delta-6 第 5 項寫實際粒度（`GYO::Engine → GYO::Kernel`）（照建議） | §2.1 樹 loading；§2.2 loading、assets-core 列；§3.8 Contract／範圍／判定／限界；§3.19 Architecture；§4 Delta-6 第 5 項 |
| S2-7 | pvp-diagnostics 的 (3)(4) 與 match-host (1) 超出範圍 | G 類 13 列拆 Match 側 6 列（本批）／Client 側 7 列（client-sim）；match-host 改「Match 側 G→0」；寫檔執行緒與全域保留到 Client 遷移 | §2.1 樹；§2.2 pvp-diagnostics、match-host、client-sim 列；§3.13b Architecture／範圍／判定 (3)(4)；§3.16 判定 (1)；§3.21 判定 (6)(9)；§3.23 判定 (10)；§5 第 23 列；§5.1 E 列 |
| S2-8 | match-sim 留了「或轉接」的出口 | (c) 程式只經 `Res<Assets<T>>`；轉接方向與使用者寫明；出口＝match-host 刪舊 `PvpMatch`；刪「或轉接」 | §3.14 Contract／範圍；§3.16 範圍；§2.2 match-host 列；§4 Delta-6 第 6 項 |
| S2-9 | G2b「移行の手間 ≤1.5」的分母沒定 | 分母＝match-sim 與 match-host 事前宣告核准時寫明的估時（上限值）；實際工時＝HANDOFF／dev_log 的會話時數加總；RC-3／RC-5 同一分母 | §2.1 G2b；§3.16 判定 (9)；§8 RC-3、RC-5 |
| S2-10 | G2a 的 BU-6 沒有同口徑分母 | 分母＝pvp-baseline 以同一內容、同一重放形式量的今天數字 | §2.1 G2a；§2.2 baseline 列；§3.2 範圍；§3.15 Contract；§6 BU-6 列 |
| S2-11 | §14.2 的突變一覽敵對檢查沒派給任何批 | 寫進橫切規則第 3 條；每批宣告列檢查者模型（未另指定＝審查者的模型） | §3 共通；§0 A4；§2.2 末「需使用者同意的指派」(4) |
| N2-1 | （N-2 只改一半）U4 2' 與路徑上測試去處的算術 | 2' 改 66～145；路徑上 32～64 | §7 U4；§2.3 |
| N2-2 | 關鍵路徑漏 framework-delete | 加入、★；路徑 318～665（M3 110～236、M4 175～376 不變） | §2.1 樹；§2.2 framework-delete 列；§2.3 |
| N2-3 | （N-4 只改一半）`project.json` 是 6 份 | 含 `tests/common/fixtures/copy_game/app/project.json` 同批升版 | §3.11 判定 (6)；§4 Delta-5 第 4 項 |
| N2-4 | wrap-assets 的「`RenderAssets<T>` 含在餘量」不成立 | 改寫：沒有獨立出所、assets-core 沒有餘量，計在 wrap-assets 8～18 內、推定（數字不變，U4 的 66～145 不受影響） | §2.2 wrap-assets 列；§3.19b 估時 |
| N2-5 | （A5 字面）三處數字不在 §6 | §3.5、§3.8、§3.23 改引 §6 | §3.5；§3.8；§3.23 |
| N2-6 | （A6 字面）缺「若失敗」與限界 | 補 net-link、first-host-cost、client-frame、工具四批、framework-delete、ci-spike-retire、runtime-loop-retire、closeout、pageguard-os 的「若失敗」；ci-spike-retire、runtime-loop-retire 的範囲の限界；match-sim 的 LIM-4 摘要 | §3.10、3.12、3.14 限界、3.15、3.23、3.25～28、3.29、3.30、3.31、3.32 |
| N2-7 | §5 第 30 列分類 | 35＋2＋0＋1＝38 | §0；§5 末行 |
| N2-8 | pvp-diagnostics 沒列測試工時、沒寫不開第二張卡的代價 | ＋2～4（推定）＝8～16；不開時路徑加 8～16 h | §2.2 pvp-diagnostics 列與合計；§3.13b 估時／並行 |
| N2-9 | windows (c) 的 N 沒有下界與上限 | N₀ 基線必做（宣告定）；N 上限 60；超過或 p 無法估時改由 closeout 60 run 判、記限界 | §3.22 判定 (5) |
| N2-10 | closeout 審查者驗不了 A4；Sonnet／medium 矛盾 | 審查 Fable／high；圖書管理員 checkup 是外部驗證；重掃計數由實作者 Opus／medium | §3.32 段 |
| N2-11 | 同意清單漏項 | 加 rollback-extract 驗證者 Opus／xhigh、a6-revision 的對抗者 | §2.2 末 |
| N2-12 | J-51 真資料版由誰執行 | assets-core 開工依賴 world-exec、結束依賴 loading；真資料版由 assets-core 執行、記其 dev_log | §2.2 assets-core 列；§3.19 判定／並行 |
| N2-13 | U11 的「短 80～155 h」是對抗前數字 | 重算：(b) 約 455～927 h，(a) 短約 137～262 h（list scheduling 估算） | §2.3 末段；§7 U11 |
| N2-14 | wrap-render「`gyo_expect_*_links` 不變」與 Delta-6 不一致 | 改「只照宣告列出的變更」；audio 若連 Kernel＝DI 變更交使用者 | §2.2 wrap-render 列；§3.18 判定 |
| 橫切 | `v8-arch-guards-migrate-with-code`（使用者決定） | §3 共通加五點規則；「測試移植」全稿改「測試、守衛與突變的去處」並照四選一寫；Engine「包」批與 Kernel 批補去處；§0 加 A10；§5.2 加 21 個守衛的起點清單 | §0 A10；§1.1；§3 共通；§3.1、3.3、3.7、3.8、3.9、3.10、3.11、3.12、3.13、3.13b、3.14、3.15、3.16、3.18、3.19、3.19b、3.20、3.21、3.22、3.23、3.24、3.25～28、3.29、3.30、3.31、3.32；§5 第 38 列；§5.2 新增 |
| （追加） | A9 的文字：第二輪已有 blocker，原文「第二輪 0 blocker」不可能成立 | **修訂者改寫、待架構師核**：改「第二輪全部處置＋第三輪限定範圍對抗 0 blocker」（依 HANDOFF 決定行「之後第三輪限定範圍對抗」） | §0 A9 |
| （追加） | match-sim 判定 (3) 的終態：本批消去 B 5、F 2 時 RESIDUAL 逐列刪（締める面、§14.4 事後報告） | **修訂者追加、待架構師核**（處置只寫「＝7，不是 0」，沒寫本批內消去後的形狀） | §3.14 判定 (3)；§5.1 |

**架構師對修訂者十點的判定（2026-11-12）**：上表各列的數字是判定前的；判定後的數字以本稿正文為準（合計 624～1,283 h、關鍵路徑 328～686 h、M3 114～245 h、M4 180～387 h、2' 69～151 h、U11 差 143～274 h、RC-5 367.5 h、RC-11 1,283 h、Kernel 五批 92～180 h，都以 Python 核對）。

| # | 事項 | 判定 | 改在哪節 |
|---|---|---|---|
| 1 | A9 改寫 | 接受；該列旁註「宣告文字的修正（架構師 2026-11-12 核）：原文『第二輪 0 blocker』改為『最終輪 0 blocker』」 | §0 A9 |
| 2 | match-sim (3) 的終態 | 接受：RESIDUAL 只因該處原因消除而刪列，締める面、§14.4 事後報告 | §3.14 判定 (3)；§5.1 |
| 3 | pvp-diagnostics 的 Match 側 6 列 | 接受：只改取得方式、輸出逐欄相同、宣告逐列說明 | §3.13b 範圍 |
| 4 | E 14 處的 Client 側 | 接受：判定掛 client-sim、PR 線末端歸零 | §3.21 判定 (9)；§5.1 |
| 5 | 改非 inline 的時點 | **定為 closeout**（舊 Client 已刪、工具已遷之後） | §2.2 match-sim 列；§3.14 Architecture；§3.32 範圍；§5.1 |
| 6 | model-assets plugin 與 asset-io 的位置 | 接受：model-assets plugin 暫放 plugins-frame；asset-io 位置留宣告 | §3.11 範圍；§3.20 Contract |
| 7 | 工時 | **補上**（推定）：match-sim 加 F 換容器 1～3 h；每個有守衛去處項的批加 1～2 h（非條件 23 批＝23～46 h、條件批 pageguard-os 另 1～2），計畫層寫在 §3 共通規則第 6 點並進各批工時；合計、關鍵路徑、M3／M4、2'、U3、U11、RC-3、RC-5、RC-11 重算 | §2.2 各列與合計；§2.3；§3 共通第 6 點；§3 各批估時；§3.25～28 表；§3.6、3.13b、3.19、3.22；§7 U3、U4、U11；§8 RC-3、RC-5、RC-11；附表 |
| 8 | determinism 區表測試找不到 | 接受：碰到的批先查證 | §3 共通第 5 點 |
| 9 | §5.2 獨立 21 列 | 接受 | §5.2 |
| 10 | 突變一覽檢查者的模型 | 改寫成「與該守衛／突變一覽的作者不同、且不是小模型（navigator）；宣告沒另指定時用該批審查者的模型」 | §3 共通第 3 點 |

## 對抗檢查處置記錄（第三輪，限定範圍；給使用者的審計線）

第三輪報告：主 checkout `build/evidence/kernel-migration-plan/adversary-report-r3.md`（blocker 0／should 8／nit 12；第二輪只改一半 5 條：B2-1、S2-3、S2-5、S2-11、N2-10；第一輪 S-2 經 S2-3 仍半）。處置由架構師定，修訂由子代理（Opus／high）套用。修訂後數字（Python 核對）：合計 **632～1,299 h**（條件批 11～22）、關鍵路徑 **331～692 h**、M3 **115～247 h**、M4 **181～389 h**、2' **70～153 h**、Kernel 五批 95～186 h、U11 (b) 475～968 h（差 144～276）、RC-11 1,299 h；Match 線 113～245 h 與 RC-5 367.5 h 不變。

| 編號 | 要點 | 處置 | 改在哪節 |
|---|---|---|---|
| S3-1 | （B2-1 半）E 14 的歸零沒有執行批 | 歸零執行批＝**client-frame**（PR 線末端）；client-sim 只消 `LocalPlayerPrediction` 內的呼叫點；`ClientConnection.cpp:714、803` 進 client-link；`main.cpp:157` 進 client-frame | §2.2 client-sim／client-link／client-frame 列；§3.21 (9)；§3.22 範圍、(7)；§3.23 範圍、(10)；§5 第 23 列；§5.1 E 列 |
| S3-2 | （B2-1 半）B 5／F 2 的舊呼叫端沒有落點 | 舊 `match_main` 的使用處→match-host（刪舊路徑時消失）；舊 Client 的 `LoadInstalledArenas` 等→client-frame；Sim 外讀檔入口是期限明確的轉接、使用處登記；並存期間舊路徑保留 `<filesystem>`（不在 Sim，不違反門）；F 加 Client 側判定 | §3.14 範圍、(9)(10)；§3.16 範圍；§3.23 範圍、(11)；§2.2 client-frame 列；§5.1 B、F 列 |
| S3-3 | （S2-5 半）match-sim 時 Match 側的 ufbx loader 登記者 | pvp 自己的最小 plugin 經 plugins-foundation 最小形的 build 函式登記；model-assets plugin 在 plugins-frame 成形並取代它；同一型別兩個 loader＝finish 錯誤 | §2.1 樹；§2.2 plugins-frame 列；§3.11 段 (i)；§3.14 範圍；§3.19 Architecture；§3.20 Contract／Architecture／範圍／判定；§4 Delta-2 差分 |
| S3-4 | （S2-11 半）§14.2 敵對檢查沒覆蓋「新增」 | 規則第 3 點改「新增與改寫」；Kernel 批與新增守衛的批在「守衛去處」項寫檢查者模型；規則第 6 點補只有「新增」項的 6 批（+6～12 h） | §3 共通第 3、6 點；§3.2、3.3、3.4、3.5、3.6、3.7、3.8、3.9、3.10、3.11、3.12、3.13、3.14、3.17、3.21、3.22；§2.2 六列與合計 |
| S3-5 | （S2-3、S-2 半）assets 搬回時點 | 統一為 **framework-delete**；各部件的搬回動作寫進所寫的批（runtime-loop-retire、framework-delete），closeout 只做最終掃描 | §3.19b 範圍；§3.29 範圍；§3.31 範圍；§3.32 範圍 |
| S3-6 | 「留」的守衛 DI 在 legacy 檔 | 規則第 2 點後半：legacy 檔刪除前先寫進 target 或現況檔；A10 改「不再有守衛的性質只來自 legacy 檔（含留型）」；§5.2 加「DI 落點」欄；closeout 判定逐列核對 | §0 A10；§3 共通第 2 點；§3.1 Contract；§3.32 判定；§5.2 |
| S3-7 | closeout 的改非 inline 沒判定、工時、檔位 | 加判定（`inline namespace sim` 0 命中、全建置與 pvp ctest 過、golden／digest 零差）、工時 2～4 h 推定、該步 Opus／high | §2.1 樹；§2.2 closeout 列；§3.32 判定、段、估時 |
| S3-8 | -zones「回報架構師」四項未吸收 | §5 加第 39～42 列（a6-revision 判區＋match-sim 搬動或改含入；出處 dev_log）；輸入 38→42、分類 39＋2＋0＋1；F 2 依賴第 40 列；候選清單同型項、`zone_c_lists` 釘住的 rewrite 成員、kz-* 突變寫進 match-sim／match-host／client-sim 的守衛去處 | §0；§0 A7；§2.2 a6 列；§3.1 Contract、判定 (2)；§3.14 範圍、(11)(12)；§3.16 範圍；§3.21 (9)；§5 標題、第 39～42 列、末行；§5.1 F 列 |
| N3-1 | （N2-10 半）樹的 closeout 審查者 | 改「審查 Fable/high」 | §2.1 樹 |
| N3-2 | （A6）LIM 只貼編號 | §3.23 的 LIM-1、§3.16 的 LIM-5 補摘要 | §3.16；§3.23 |
| N3-3 | S-n 兩套同字形 | spec 的寫「spec S-n」、第一輪對抗的寫「第一輪對抗 S-n」；§3.20 的「S-8 解讀」由宣告查證 | §3.5、3.8、3.7、3.13、3.14、3.17、3.20；§7 U14 |
| N3-4 | diagnostics-service 的依賴 | 改「plugins-foundation 全批」（經它依賴 threading） | §2.2 |
| N3-5 | 段 (i) 的判定、審查、dev_log | 寫明段 (i)／(ii) 各判哪幾項、段 (i) 審查並寫 dev_log | §3.11 |
| N3-6 | asset-io 與 headless-runner 的檔位 | 交換點與啟停段 xhigh；同意清單 (1) 同步 | §2.1 樹；§2.2 末；§3.11 段 |
| N3-7 | target/naming 與 zones-gate 不一致 | a6-revision 改 naming 第 4 條例子與 DI 1；DI 1 守衛（新增）派 match-sim | §3.1 Contract；§3.14 (12) |
| N3-8 | 照搬區表的 E-n 會懸空 | closeout 判定加一句 | §3.32 判定 |
| N3-9 | A9 看結果後改文字 | §0 A9 旁加「待使用者核准（HANDOFF 決定行 `v8-arch-migration-plan-a9`）」 | §0 A9 |
| N3-10 | G2b 實際工時的定義 | 會話開始／停止時刻之差的加總，等 CI 與等使用者不計，每次在 HANDOFF 記一行 | §2.1 G2b；§3.16 (9) |
| N3-11 | G2a BU-6 是否含讀檔 | 兩邊都從讀檔開始計 | §2.2 baseline 列；§3.2 範圍 |
| N3-12 | 套用 System 的登記者與區守衛 | `engine/asset` 的登記函式由 runner 側 plugin 呼叫，重放 host 由測試呼叫；(c) System 由 assets DI 的守衛負責 | §2.2 assets-core 列 |
| U4 2' | 架構師指示「改 69～151」 | 該數是判定後、第三輪前的值；第三輪 rollback-extract 依規則第 6 點加 1～2，現為 **70～153**（修訂者照規則算，待架構師核） | §7 U4 |

**驗收點檢後補齊（2026-11-13；點檢 `build/evidence/kernel-migration-plan/verify-r3.md`：A1～A10 成立、數字一致、8 條只改一半）**：

| 編號 | 補了什麼 | 改在哪節 |
|---|---|---|
| S3-1（＋B2-1） | §3.13b 判定 (3) 改「client-frame（PR 線末端）歸 0；client-sim 只消 `LocalPlayerPrediction` 內的呼叫點」 | §3.13b 判定 (3) |
| S3-5（＋S2-3、S-2） | §2.2 closeout 列改成與 §3.32 一致；指定搬回批：world／system-contract／scheduler／determinism／error-handling 與 kernel DI 頁隨 runtime-loop-retire（理由：Kernel 最後一次程式變更），assets／runtime-composition 隨 framework-delete，naming／zone-c-sources／limits／README 隨 closeout（跨部件的規則與清單）；§3.29、§3.31 判定加搬回判定（搬回後對應 legacy 刪、入鏈 0 斷、同名並存 0；determinism.md 等 §7 去處決定後才刪） | §2.2 closeout 列；§3.29、§3.31 範圍與判定；§3.32 範圍 |
| S3-7 | §1.1 改「closeout 改非 inline」；§2.2 closeout 列判定要點加改非 inline 的三項判定 | §1.1；§2.2 closeout 列 |
| S3-8（＋B2-1） | `LoadPlayerPresentationDefinition`／`SamplePlayerPresentationPose` 寫進 §5 第 42 列與 match-sim 範圍；kz-* 寫進 match-sim（判定 (12)）、a6-revision（區表修正列出清單）、client-frame（判定 (12)）三批（上表 S3-8 列寫的 match-host／client-sim 以此為準更正） | §5 第 42 列；§3.14 範圍；§3.1 Contract；§3.23 判定 (12) |
| N3-3 | 正文裸 `S-n` 30 處全部標「第一輪對抗 S-n」；§3.20 引 HANDOFF 的「S-8」已查明＝-spec-v03 第三輪對抗 S-8，改成帶出處的引用 | 全稿 §2.2～§7 |
| `ObjectFpsPvp` 字面 | 改「target/naming 第 4 條的舊例子 `ObjectFpsPvp::Sim::` 待改為 `fps::pvp::sim`（a6-revision）」 | §3.1 Contract |

## 對抗檢查處置記錄（守衛盤點回寫，2026-11-13）

來源：守衛盤點 `docs/architecture/plans/kernel/guards-inventory.zh-Hant.md`（PR #198；dev_log `docs/dev_logs/2026_11_12_arch_guards_inventory.zh-Hant.md`）的 §5 十二題與 §6 九處不一致；處置＝HANDOFF 決定行 `v8-arch-guards-inventory-dispositions`。工時不另加（守衛去處已有每批 1～2 h），數字沒有連動（合計 632～1,299 h、關鍵路徑 331～692 h 不變）。

| 項 | 回寫內容 | 改在哪節 |
|---|---|---|
| §6 九處不一致 | 第 1 列「留（守衛）／突變條件重列」、整體 store 併入 kernel 時條件退役；第 4～6 列加 plugins-foundation 改掃描根；第 9 列加 plugins-frame、G1／G3 的 `if(TARGET)` 靜默退出；第 10 列 plugin 側是新增、fw-* 29＋31＋10 條在 plugins-foundation／frame／editor 重列；第 18 列 G3→match-host、`RequireEntityOrder`→退役；第 19 列 client-sim 改寫＋match-host 改寫；第 21 列改引用；件数來源（子列 63＝留 31／改寫 21／退役 11＋新增 3；突變 26 檔 1,841 條＝留 1,131／重列 627／退役 83） | §5.2 前言與第 1、4、5、6、9、10、18、19、21 列 |
| Q1 | a6-revision 在 target/runtime-composition §4 寫「plugins 受 OS 邊界守衛掃描」；plugins-foundation 提 ROOTS 變更（DI 守衛→使用者核准） | §3.1 Contract；§3.11 判定 (12)；§5.2 第 4～6 列 |
| Q2 | 只有該第三方的 adapter plugin 可在公開標頭露其型別；plugins-frame 新增 plugin 標頭衛生守衛；wrap-render 寫明搬走後端的 G1／G2 退役理由 | §3.20；§3.24；§3.18；§5.2 第 9 列 |
| Q3 | assets-core v1 不另開 CMake target | §3.19 Architecture；§4 Delta-6 第 5 項 |
| Q4／Q5 | G3→match-host；OutageRig→client-sim 改寫、S3→match-host 改寫 | §5.2 第 18、19 列；§3.16；§3.21 |
| Q6 | `KernelPrepDetectTests` 改寫（經新 host 觀測 T=35／R=215）；U6 選改時序方案時退役＋U9 | §3.14 判定 (2)；§5.2 補遺 補2 |
| Q7 | entity_store 守衛留、突變條件重列；store 併入 kernel 時條件退役 | §5.2 第 1 列 |
| Q8 | plugins-foundation 建 `docs/plugins/`（層 DI＋每 plugin 一頁含 DI／RC）與兩條新增守衛（無產品名、中性命名）；runtime-composition DI 只留跨 plugin 規則 | §3.11 範圍、判定 (12)；§5.2 缺口歸批 |
| Q9 | naming DI 1 掃描與 error-handling DI 2 禁字→kernel-scaffold；Peer 不掛 Player 掃描→match-host（取代第三輪 N3-7 的「派 match-sim」） | §3.3；§3.1 Contract；§3.14 (12)；§3.16；§5.2 缺口歸批 |
| Q10 | a6-revision 提 `neutral_names_guard` 只掃程式（§14.4 1～5 交使用者），核准後由 kernel-scaffold 改腳本（修訂者選的執行批，待架構師核） | §3.1 範圍 |
| Q11 | wrap-render 把 `if(TARGET)` 改硬性檢查（締める面、事後報告） | §3.18；§5.2 第 9 列 |
| Q12 | tool-voxel 從 target 的文件 World／渲染 World 重導 core／window 規則（改寫） | §3.25～28 表 voxel 列 |
| 拉回風險四項 | `EntityModelTests` 的 friend 讀私有欄（match-sim）、OutageRig（client-sim）、PV2／PV5 readback（plugins-editor）、四工具正向清單（plugins-foundation 初判＋工具四批）各在守衛去處項首標「拉回風險」並寫處置 | §3.14 (8)；§3.21 (8)；§3.24；§3.11 (12)；§3.25～28 |
| 空轉三項 | os_boundary_guard ROOTS（plugins-foundation）、platform-sdl G1／G3 `if(TARGET)`（wrap-render）、framework_dependency_guard 空 rglob→`tests/framework` 同批刪（framework-delete）寫進判定 | §3.11 (12)；§3.18；§3.29 |
| 補遺六守衛 | 盤點 §2.22 的六個守衛加進 §5.2 補遺表 | §5.2 補遺 |

## 對抗檢查處置記錄（未決回寫與核准，2026-11-13）

使用者 2026-11-13 逐項定了 §7 的未決（HANDOFF `v8-arch-migration-plan-undecided`），並核准計畫本體與 A9 的文字修正（HANDOFF `v8-arch-migration-plan-approved`、`v8-arch-migration-plan-a9`）。工時沒有變（合計 632～1,299 h、關鍵路徑 331～692 h）；新增的估算只有 U11 (a) 的開線完成點（約 383～792 h，list scheduling 估算，只作量級）。

| 項 | 回寫內容 | 改在哪節 |
|---|---|---|
| 狀態 | 檔頭加「已核准（使用者 2026-11-13）；未決 13 項已定」；§0 加「未決 13 項已定、A 判定不變」；A9 改「使用者 2026-11-13 核准」 | 檔頭；§0；§0 A9 |
| §7 | 表頭改「已定事項（原未決）」；每列加「已定（使用者 2026-11-13）：選項＋理由」，原選項與原統合建議保留供對照 | §7 |
| U1 | world-exec 範圍加欄配置介面（一般配置／可換獨立頁）；pageguard-os 開批條件改「G2a 後決定」；ci-spike-retire 條件與 U10 時點同步；Delta-4 條件同步 | §1.2 第 7 點；§2.1 樹；§2.2 world-exec、pageguard-os、ci-spike-retire 列；§3.7 範圍；§3.10；§3.30；§4 Delta-4；§5 第 30 列與末行 |
| U3 | §1.2 寫「master 不得新舊並存」為原則，確認兩條 PR 線 | §1.2 第 4 點 |
| U4 | 兩段閘門確認 | §1.2 第 8 點 |
| U5 | kernel-scaffold：兩種樣本先過 Windows L1 再照 U5 分工 | §3.3 Contract、判定 |
| U6 | G1 謂詞改「`LifeState` 單一寫者在 tick 開頭、`DeathEvent` 由 `ResolveActions` 發、snapshot／digest 在其後讀」；match-sim 寫 `LifeState`＋`DeathEvent`（spec F 生命週期、同步點清掉、回滾重放時重發不另存）、突變改針對 `DeathEvent`；刪中間欄寫法 | §2.1 G1；§3.6；§3.14 Contract、判定 (2)；§3 取捨 Life；§5 第 17 列；§8 RC-4 |
| U7 | match-host Contract 寫 bevy_replicon 式分層與 per-connection Egress Resource；宣告第一步查 pv9 與 Gateway 測試 | §3.16 Feature、Contract |
| U8 | §6 量測改三層（Docker Linux 容器判定、絕對數字標「容器內」）；pvp-baseline 建 Dockerfile 並在容器內量 | §6 前言、BU-1、BU-1′、量測紀律；§3.2 範圍、限界；§3.15 限界 |
| U9 | §3 共通加 golden 重設逐條核准；RC-9 改「使用者改變已定事項」 | §3 共通；§8 RC-9 |
| U10 | ci-spike-retire 寫永久連結要求；closeout 判定加「連結存在且有效」 | §3.30 範圍；§3.32 判定 |
| U11 | §2.3 寫三條常設線的開線時點與「前置齊了不等閘門湊整」，重算 (a) 的完成點說明 | §2.3 |
| U12 | §6 前言：a6-revision 後、match-sim 宣告前以「今天的值 × 倍數」再核准 | §6 前言 |
| U13 | a6-revision 產出：目標 DI 清單兩層（跨部件先讀／各部件）＋legacy 對照；判定 (7) 改寫 | §2.1 樹；§2.2 a6 列；§3.1 判定 (7) |
| U14 | §1.3 改「已定 (a)」並記未結事項（可能的設計缺陷）；loading、match-sim、match-host 的限界列它並引 HANDOFF「未結事項」節；closeout 判定加「未結事項節沒有被刪」 | §1.3；§3.8、§3.14、§3.16 限界；§3.32 判定 |

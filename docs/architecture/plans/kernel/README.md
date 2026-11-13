# Kernel 研究線：GYO 的組合協定（設計圖）

更新：2026-11-12。Owner：架構（架構師會話）。**狀態：改造中。規格 v0.3 定稿（K.3 全 17 項已定）；-spike、-audit、-spec-v03、-spike-pageguard、-remove-object-fps、-ci-readiness 完成；並行線（J-17 掃描器、圖書管理員、產品前置 -detect、風紀委員）已開；架構師線下一批：遷移批規劃（`v8-arch-migration-plan`）。**
起點是使用者的願景稿「GYO-Engine 架構思想研究」（2026-11-10）與「GYO 架構設計思想」Concept Draft v0.2（2026-11-11；Bevy ECS／Scheduler、Service Mesh、Control／Data Plane）。

使用者 2026-11-10：**基盤的協定不能等壓力出現才改**；功能層的擴充才等壓力。
使用者 2026-11-11：**要的是 Bevy 那種抽象能力（World／System Contract／Scheduler／Executor），帶入現有 GYO，不是 GYO2；架構改造完成前，功能線停。** 2026-11-11 定：停法 (a)（已開工的卡做到下一個關卡停）；規格與 spike 同時開，audit 等 spec；控制權在主會話，子代理由主會話開與收（`v8-arch-kernel-control`）。

## 為什麼（使用者的觀測，2026-11-11）

> 雖然工作量爆炸，但是我卻沒有之前 GYO 那種改不動的感覺。第一次改不動是 10/10 的事故，第二次還沒碰到，但是我感覺到架構組織其實有壞味道，我沒有辦法使用統一的邏輯來執行工作。

-audit 把這個壞味道量化成「重寫集中在組織層、每個 host 一套手排順序、四個工具 client 各一套、framework 在中間補」。改造的方向是每遷一個 host 少一套規矩。

## 方法

不列功能，找不變量：讀 Bevy 與兩個網路 plugin 的原始碼，問「這些功能都靠哪幾條不變量才成立」，再逐條問 GYO 有沒有、GYO 的約束（決定性、狀態的權威、Engine 中性、C++ 無反射與借用檢查）會不會讓它變形、變形後還守不守得住效果。

證據（只讀審閱的報告，每條有檔名行號）：主 checkout `build/evidence/kernel-study/`（`bevy_core.md`、`replicon.md`、`lightyear.md`，`render.md`、`assets.md`、`platform.md`、`capabilities.md`、`os_io.md`）。原始碼在 `~/Code/GitHome/{bevy, bevy_replicon, lightyear}`（使用者 2026-11-10 放入）。

## 文件

- [mechanism.zh-Hant.md](mechanism.zh-Hant.md)：**本線的基準**。對 v0.2 的理解、機制骨架、契約與控制面（含使用者提供的審查修正與架構師兩條主張）、網路作為 System、規則變機制、不解決什麼、改造的性質與風險、判準。三張圖在 `diagrams/`。
- [spec.zh-Hant.md](spec.zh-Hant.md)：**規格 v0.3（定稿；-spec-v03 五輪對抗／複核後，PR #182）**。A～M 十三節、守衛 J-1～J-53、Design Invariants 與 Replacement Criteria；K.3 的 17 項全部由使用者決定（對照表在 target/README）；各輪對抗檢查的處置在文末修訂記錄。
- [spike-report.zh-Hant.md](spike-report.zh-Hant.md)（分支 `claude/arch-kernel-spike`）：`-spike` 批的報告；範圍の限界 L1～L8。
- [networking.zh-Hant.md](networking.zh-Hant.md)：網路側的設計稿——網路要成為機制上的 System，機制必須提供的八條；網路層自己的設計圖 N1–N12；對 GYO 現況的對照。
- [blueprint.zh-Hant.md](blueprint.zh-Hant.md)：**輸入**（2026-11-11 降級）——Kernel 八條不變量、各層補充的不變量（R／A／P／C／O／N）、GYO 刻意的偏離（候選 DI-K1～K7）、反例清單、現況對照、Replacement Criteria。它沒有契約分類與 Executor 的分離，遷移順序一節假設了改造路線；以 mechanism 為準。
- [audit.zh-Hant.md](audit.zh-Hant.md)：**`-audit` 的合併報告（完成）**：逐模組留／包／重寫、跨部件發現與待決、區表回對、規格修正七項、對抗複核。部分報告：`audit-part-{engine,framework-tools,product-thirdparty}.zh-Hant.md`。
- [spec-v03-declaration.zh-Hant.md](spec-v03-declaration.zh-Hant.md)：`-spec-v03` 的事前宣告（完成，PR #182，2026-11-12）。
- [spike-pageguard-declaration.zh-Hant.md](spike-pageguard-declaration.zh-Hant.md)：`-spike-pageguard` 的事前宣告（完成，PR #184，2026-11-12）。
- [remove-object-fps-declaration.zh-Hant.md](remove-object-fps-declaration.zh-Hant.md)：`-remove-object-fps` 的事前宣告（核准並執行，PR #185，2026-11-12）。
- [remove-object-fps-report.zh-Hant.md](remove-object-fps-report.zh-Hant.md)：`-remove-object-fps` 的報告（判定 1～5 全部成立；§7 實測：共通層零修改）。
- [spike-pageguard-report.zh-Hant.md](spike-pageguard-report.zh-Hant.md)：`-spike-pageguard` 的報告（判定 1 照原文不成立，使用者 2026-11-12 定讀法 (i)；判定 2、3、4、5 成立）。
- [audit-declaration.zh-Hant.md](audit-declaration.zh-Hant.md)：`-audit` 的事前宣告（已完成）。
- [j17-scanner.zh-Hant.md](j17-scanner.zh-Hant.md)：**J-17 掃描器線的交接卡**（並行線；使用者 2026-11-12 同意分出；卡的現況由該線更新）。
- [ci-readiness.zh-Hant.md](ci-readiness.zh-Hant.md)：`v8-arch-kernel-ci-readiness`（任務卡，2026-11-12）——新 Engine 子系統進 L1 的檔清單（假子系統實走）、spike 測試升格方案（逐檔搬／重寫／刪）、Windows VEH 在 CI 的證據與風險；共通層零修改。
- [legacy-impact.zh-Hant.md](legacy-impact.zh-Hant.md)：`v8-arch-legacy-impact-scan`（任務卡，2026-11-12）——舊架構文件對新架構的影響掃描：37 檔的 DI／RC 三選一判定（衝突 4、被取代 36）、守衛會不會誤判新程式（1 個、有條件）、誤導句 33、target 對 legacy 的反向依賴 91 處；對抗複核 Opus／high 一致 8／不一致 2；只寫不改。
- [guards-inventory.zh-Hant.md](guards-inventory.zh-Hant.md)：`v8-arch-guards-inventory`（任務卡，2026-11-12）——21 個 legacy 守衛（63 子列）與 26 檔 1841 條突變的逐列盤點：守的性質、來源條文、去處四選一、涉及的突變、預計的批、不動會把哪一批的新程式判違規；缺口 9、給架構師的問題 12、與遷移計畫 §5.2 的不一致 9。
- [tool-hosts-inventory.zh-Hant.md](tool-hosts-inventory.zh-Hant.md)：`v8-arch-tool-hosts-inventory`（任務卡，2026-11-13）——四個工具 host 骨架 10 項 × 4 的盤點：各怎麼做、共用程度（framework 一份／各自一份）、依賴邊逐工具（CMake 13／12／13／10 vs. include 15／14／18／12，差異全由 PUBLIC 傳遞解釋）、性質來源與會撞到的守衛（引 guards-inventory）、工具專屬、重複但內容不同 15 列、plugin 切法候選 9 個（候選，不是決定）。
- 產品線前置的交接卡：[docs/object_fps_pvp/plans/v8/kernel-prep.zh-Hant.md](../../../object_fps_pvp/plans/v8/kernel-prep.zh-Hant.md)（`v8-product-kernel-prep`，2026-11-12）。
- [migration-plan.zh-Hant.md](migration-plan.zh-Hant.md)：**遷移批規劃（`v8-arch-migration-plan`；稿完成、待使用者核准）**——34 批＋條件批、閘門 G1／G2a／G2b／G3、§3 七項（Delta-1～6）、各批 Feature→Contract→Architecture 與模型／檔位、預算 BU-1～BU-8（多為推定）、未決 U1～U14、守衛與突變隨程式遷移的橫切規則；三輪對抗＋驗收點檢，處置記錄在文末。
- [HANDOFF.md](HANDOFF.md)：記錄器。
- `docs/architecture/target/`（圖書管理員 2026-11-11 定）：**目標架構的部件文件**，只寫使用者已裁決的部分；首篇 [kernel/world.zh-Hant.md](../../target/world.zh-Hant.md)（鍵格式與配發策略、change stamp）。

## 批次（2026-11-11 核准）

```text
v8-arch-kernel
├── -spec    上段｜ultracode：提案 Opus×3／high、統合 Fable／high、對抗 Opus／xhigh｜約 1 天｜mechanism → 正式規格，每部件一節
├── -spike   上段｜實作 Opus／xhigh（主會話的子代理）、審查 Fable／xhigh｜2～3 天｜獨立子目錄最小 C++ 原型，證明簽名推存取與受限句柄
├── -spec-v03  上段｜主會話 Fable／high 自寫、對抗 Opus／high 只讀｜規格修訂九項＋第一個文件批（Delta 文件修改）＋五輪對抗／複核（spec 修訂記錄）｜完成（PR #182，2026-11-12）
├── -spike-pageguard 上段｜實作 Opus／xhigh、敵對 Fable／xhigh｜頁保護：只對寫設保護、只保護獨占寫入的欄、只在 (c) 區 System 執行時、歸「存取驗證（可關）」的第三層（不是 GYO_ASSERT；頁錯誤處理器記違規、System 以 Err 回傳；B-9 結案後分類）；完成：判定 1 照原文不成立（讀法 (i)，使用者 2026-11-12）、2／4／5 成立、3 成立（L1 run 43169204148）；PR #184（2026-11-12）
├── -j17-scanner 下段（並行線）｜規則與遮罩 Fable／xhigh、其餘 Fable／high、審查 Opus／high｜J-17 掃描器：ZS1～ZS7、自測 65、突變 18/18、pvp 基線兩份（確認 12 檔／候選 5 檔）、清單契約 v1｜實作完成，待 L1 與合併（卡 `j17-scanner.zh-Hant.md`）
├── -h7-scanner 下段（遷移計畫 §3.4）｜實作 Opus／high（型別近似 xhigh）、審查與敵對檢查 Fable／high｜H-7 呼叫點掃描器：共通 `tests/common/h7_scan/`（自測 124、突變 104/104，Fable 審查與敵對檢查已處置）＋五個 owner 的 `h7-hosts.json`（18 檔，基線 H7-a 2／H7-b 11，只報告）｜完成，已合併（PR #207，2026-11-13；L1 run 43362195890 三平台綠；[宣告](h7-scanner-declaration.zh-Hant.md)）
├── remove-object-fps 上段｜實作 Opus／high、審查 Fable／high｜改造前獨立批｜移除 apps/object_fps、object_fps_v2、tools/object_fps_preview，**加 engine/runtime 的 IRuntimePort 與 engine/model 的 AnimationInstance（含各自測試；使用者 2026-11-11）**（§7 完整刪除；兼驗 removability：共通層零修改是判定，不成立就記 Delta 不修）｜完成：判定 1～5 成立（三平台 L1 run 43188319281），§7 實測共通層零修改；PR #185（2026-11-12）
├── -kernel-scaffold 上段｜實作 Opus／high、審查＋敵對檢查 Fable／high（一次）｜kernel 進 L1 的腳手架：`engine/kernel`（INTERFACE、只連 Base）、J-9 與 naming DI 1／3 掃描、J-10 兩種樣本、突變 37｜完成，已合併（PR #204，2026-11-13；突變 79/79；[宣告](kernel-scaffold-declaration.zh-Hant.md)）
└── -audit   **完成**｜Opus／high ×3、對抗 Fable／high（4/5）｜現有模組逐一「留／包／重寫」，給中間層成本
```

| 批 | 內容 | 產出 | 判定（事前宣告） |
|---|---|---|---|
| `-spec` | **完成**（v0.1→v0.2 隨 K.3 決定，PR #179；v0.3 由 -spec-v03 定稿）。把 mechanism 落成規格：World、System 契約（兩半）、Registry、Scheduler（三種分析結果、歧義處置）、Executor（策略與執行緒親和）、延後佇列與事件（同步點）、多 World 與 schedule 組合、Host 保留的職責；每節：資料模型／操作／不變量／C++ 實現手段／Bevy 對照與不採的理由；Design Invariants 與 Replacement Criteria 草案；不含遊戲型別的驗證方式 | `spec.zh-Hant.md`；對抗檢查附末 | 對抗檢查者找不到「規格允許、但會破壞 (c) 區四處契約或獨占寫入」的反例；找到則回提案。不改程式 |
| `-spike` | **完成（PR #180 合併）**：判定 1 三平台成立（L1 run 43018979148）；判定 2 照原文成立、攔不到的形狀記為範囲の限界 L1～L8；判定 3 成立（34/34）。獨立子目錄（候選 `spikes/kernel/`，不進任何 target 的依賴），只做兩件：(1) 從函式參數型別在編譯期推出 `{reads, writes}`，含 Component、Resource、事件、延後佇列句柄；(2) 受限句柄＋debug 建置的 World 存取核對，未宣告的 id 即斷言。測試用的型別只存在測試裡 | 原型程式＋測試＋`spike-report.zh-Hant.md`（兩個編譯器的差異、做不到的事） | (1) 10 個以上不同簽名的 System，推出的集合與人手寫的期望逐一相同（static_assert＋執行期），L1 三平台（MSVC、gcc／clang、AppleClang）全綠；(2) 3 種繞路（存參考、拿裸指標、經未宣告 id 讀 World）在 debug 建置各被斷言攔下，release 不攔並寫明；(3) 突變：移除任一宣告 → 對應測試失敗。任一不成立＝spike 結論「做不到」，照 §14 停下回報，不補救 |
| `-audit` | 拿 `-spec` 去讀現有每個模組：有沒有自己的生命週期或更新迴圈；**有沒有跨 tick 的指標或參考；有沒有全域可變狀態**（原第二問「實際存取能不能被契約框住」依 `v8-arch-kernel-limits-accept` 改為這兩個可檢查的問題）；是不是純函式。事前宣告必須列 spike-report 的範囲の限界 L1～L8 | `audit.zh-Hant.md`：逐模組留／包／重寫＋理由＋行數 | 每個結論附檔名與行號；對抗檢查抽 5 個模組複核 |

## -audit 的起點（各線 2026-11-11 回報）

- 工具線 voxel 編輯器：迴圈在 `tools/voxel_map_editor/src/window/EditorWindow.cpp`（ImGui 每幀：Shortcuts → DrawPreview［SyncPreview 重烘、scene Apply／Render、Fly、Pick、Edit］→ Overlay）；葉子在核心（Build／MapBuild／Visibility／FlyCamera／Tools／Editing）。這是 Editor Runtime 對照的第一個樣本。
- 工具線 model_viewer：`ViewerWindow.cpp:112` 已知缺陷（選擇窗不比對 tag、Failed 結束等待），改造期間不修，記在工具線 HANDOFF 與跨元件規則登記頁；是「契約後果在消費端各寫一份」的實例，-audit 讀 framework 時要看。
- 產品線：PR #167、#168、#173 已合併，無進行中實作；改造前置的 -detect 由架構師的任務卡做（`docs/object_fps_pvp/plans/v8/kernel-prep.zh-Hant.md`）。

## 與其他線的關係

- 決定性等級（PR #177，`docs/architecture/determinism.md`）：(c)／(b) 區的定義；mechanism §3.3 的四處契約引用它。
- 第三方評估卡（PR #176）：每個候選方案要能當機制上的 System；選庫在責任邊界確認之後。
- 場景線（`docs/architecture/plans/scene/README.md`）：第一個非玩家實體與邏輯連線，是機制第一個真實的壓力來源。
- pv10 需求（`docs/object_fps_pvp/plans/v8/pv10-requirements.md`）：網路設計稿的輸入。
- ~~AGENTS §2 的修訂候選~~ 已於 PR #178 合併（§2 基盤／応用之分）；本批兩條教訓（刪 TEST_CASE 前看每個 CHECK、宣告的「不碰」要用有效掃描證明）2026-11-12 已送風紀委員。

## dev_log

- [2026_11_13_arch_adapter_plugins_plan.zh-Hant.md](../../../dev_logs/2026_11_13_arch_adapter_plugins_plan.zh-Hant.md)：-adapter-plugins-plan（規劃批；[宣告](adapter-plugins-plan-declaration.zh-Hant.md)、[規劃稿](adapter-plugins-plan.zh-Hant.md)）。
- [2026_11_13_arch_h7_scanner.zh-Hant.md](../../../dev_logs/2026_11_13_arch_h7_scanner.zh-Hant.md)：-h7-scanner（任務卡；[宣告](h7-scanner-declaration.zh-Hant.md)）。
- [2026_11_13_arch_kernel_scaffold.zh-Hant.md](../../../dev_logs/2026_11_13_arch_kernel_scaffold.zh-Hant.md)：-kernel-scaffold（[宣告](kernel-scaffold-declaration.zh-Hant.md)）。
- [2026_11_13_arch_kernel_a6_revision.zh-Hant.md](../../../dev_logs/2026_11_13_arch_kernel_a6_revision.zh-Hant.md)：-a6-revision（spec v0.4、Kernel 的 DI 落 target；[宣告](a6-revision-declaration.zh-Hant.md)、[報告](a6-revision-report.zh-Hant.md)）。
- [2026_11_12_arch_guards_inventory.zh-Hant.md](../../../dev_logs/2026_11_12_arch_guards_inventory.zh-Hant.md)：-guards-inventory（任務卡）。
- [2026_11_13_arch_tool_hosts_inventory.zh-Hant.md](../../../dev_logs/2026_11_13_arch_tool_hosts_inventory.zh-Hant.md)：-tool-hosts-inventory（任務卡）。
- [2026_11_12_arch_legacy_impact_scan.zh-Hant.md](../../../dev_logs/2026_11_12_arch_legacy_impact_scan.zh-Hant.md)：-legacy-impact-scan（任務卡）。
- [2026_11_13_arch_kernel_migration_plan.zh-Hant.md](../../../dev_logs/2026_11_13_arch_kernel_migration_plan.zh-Hant.md)：-migration-plan（規劃稿，待核准）。
- [2026_11_12_arch_j17_scanner.zh-Hant.md](../../../dev_logs/2026_11_12_arch_j17_scanner.zh-Hant.md)：-j17-scanner（J-17 掃描器線）。
- [2026_11_12_arch_kernel_ci_readiness.zh-Hant.md](../../../dev_logs/2026_11_12_arch_kernel_ci_readiness.zh-Hant.md)：-ci-readiness（任務卡）。
- [2026_11_11_arch_kernel_spec.zh-Hant.md](../../../dev_logs/2026_11_11_arch_kernel_spec.zh-Hant.md)：-spec（v0.1→v0.2；補寫於 2026-11-12）。
- [2026_11_12_arch_remove_object_fps.zh-Hant.md](../../../dev_logs/2026_11_12_arch_remove_object_fps.zh-Hant.md)：-remove-object-fps。
- [2026_11_12_arch_kernel_spike_pageguard.zh-Hant.md](../../../dev_logs/2026_11_12_arch_kernel_spike_pageguard.zh-Hant.md)：-spike-pageguard。
- [2026_11_12_arch_kernel_spec_v03.zh-Hant.md](../../../dev_logs/2026_11_12_arch_kernel_spec_v03.zh-Hant.md)：-spec-v03。
- [2026_11_11_arch_kernel_audit.zh-Hant.md](../../../dev_logs/2026_11_11_arch_kernel_audit.zh-Hant.md)：-audit。
- [2026_11_11_arch_kernel_spike.zh-Hant.md](../../../dev_logs/2026_11_11_arch_kernel_spike.zh-Hant.md)：-spike。

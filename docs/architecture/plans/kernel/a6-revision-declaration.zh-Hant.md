# 事前宣告：`v8-arch-a6-revision`（spec A.6 修訂＋legacy 處置＋kernel DI 落 target）

更新：2026-11-13。Owner：架構（架構師會話自寫）。**狀態：草案，待使用者核准；核准前不改任何文件。**
出處：遷移計畫 `migration-plan.zh-Hant.md` §3.1（已核准，`v8-arch-migration-plan-approved`）、§5 歸本批的 24 列、§7 已定的 U1（第三層延到 G2a 後定）、U13（目標 DI 一次核准、清單兩層）、U14（未結事項）；HANDOFF 決定行 `v8-arch-legacy-impact-dispositions`（含 S-18 修正）、`v8-arch-guards-inventory-dispositions`（Q1、Q8、Q10）、`v8-arch-plugin-selection-data`、`v8-arch-guards-migrate-with-code`。

## 目的（Feature → Contract → Architecture，照計畫 §3.1）

- **Feature**：(i) 場次中開著驗證抓繞路，「無違規」報告帶「驗證開著」（PvP 的 Match／Client）；(ii) 後面每批的事前宣告能引用穩定條號——結束時 kernel 的 `Design Invariants` 與 `Replacement Criteria` 在 target/ 有自己的一頁，legacy 36 條標好「已被取代（批次代號）」或「相容（措辭更新）」。
- **Contract**：spec 升 **v0.4**（文件層的契約修訂，不改任何已定前提）。
- **Architecture**：不改結構。採第三層時的 OS Delta 只在這裡**寫**（計畫 §4 Delta-4 的文字；實作在條件批 pageguard-os，時點隨 U1）。

## 範圍（只動 `docs/`；逐檔）

| 檔 | 改什麼 | 來源列（計畫 §5） |
|---|---|---|
| `plans/kernel/spec.zh-Hant.md` | A.6：第三層寫成**條件化**條文（「若採用」），限定 Exclusive 欄、(c) System、開關開（6）；Resource 不保護（v1）並寫為 Limit（7）；(b) System 只解它宣告寫的欄＋Shared（8）；handler 串接與 `reinstalls` 進「驗證開著」報告欄位（9）；多 World 身分（pageguard 5）；`tlCurrent` 為空的故障＝GYO_ASSERT（12；一個事件一種性質）；頁大小分平台估算指向 §6 BU-4／BU-8（14）。G.4：第 5 條擴充（19、22）、補第 6 條主執行緒讀點（20）、第 3 條措辭含 `TraceMovement` 本身（23）。「鏈」與「清單」措辭統一（27）。L.1／區表：對 -zones 回報 39～42 判區並寫理由。對 legacy 的行號引用改條號（34）。修訂記錄「v0.4（a6-revision）」。 | 6、7、8、9、12、14、19、20、22、23、27、34、39～42 |
| `target/system-contract.zh-Hant.md` | §2b 加 field-session 澄清句（衝突 3）；驗證開著的報告欄位（含 `reinstalls`） | 29、9 |
| `target/error-handling.md` | 收窄 L124 的關係：`Base::Error` 不含鏈，Kernel 的 `KernelError` 帶上下文鏈 | 27 |
| `target/world.zh-Hant.md` | §6 只寫通用規則：store 不持 stamp，World 以 SoA 持 keys／值／stamps（36）；**不放**產品欄位名（S-18） | 36、35 |
| `target/runtime-composition.zh-Hant.md` | §3b 加 preview 新條文（每幀各自：render target 在渲染 World 合成，draw list 以貼圖 id 引用）（28）；§4 加「plugins 受 OS 邊界守衛掃描」原則（Q1）；§4 已有的 plugin 選擇＝資料不動 | 28、Q1 |
| `target/determinism.md` | 從 legacy `determinism.md` 搬 §1 三級、§2 區表、§4 區表（含 4.7a／4.7b；已裁決）；**不搬 §7 與 E0～E5** | 33 |
| `target/limits.zh-Hant.md` | LIM-3 條文改為「第三層為條件批；解除 L1 的條件…」；LIM-6 不動 | 6 |
| `target/kernel.zh-Hant.md`（**新**） | kernel 的 Design Invariants（spec DI-KS 15 條）與 Replacement Criteria（RC 13 條）落成 target 一頁，條號穩定；寫明它與 spec 的關係（spec 是設計說明、此頁是約束） | §3.1 Feature (ii) |
| `target/README.md` | 部件一覽加 kernel 頁；**目標 DI 清單兩層**（跨部件先讀／各部件）附 legacy 條文對照（U13 的核准清單） | U13 |
| `docs/object_fps_pvp/target/entity-model.zh-Hant.md`（**新**，產品的 target 文件） | entity-model 第 139 行的欄位分類表搬入；E-3 類「半條」照「相容（措辭更新）」改寫；標 target、not implemented | 35、32 |
| legacy 檔：`architecture.md`、`architecture/entity-store.md`、`determinism.md`、`error-handling.md`、`framework/preview.md`、`framework/model-viewer.md`、`object_fps_pvp/entity-model.zh-Hant.md`、`network-architecture.zh-Hant.md` | 只加標註，不改設計內容：36 條逐條標「已被取代（`v8-arch-a6-revision`）」或「相容（措辭更新）」；preview.md L3 的 carry-over 限縮為 DI 1～3、5，DI 4 標已被取代；model_viewer RC 1、preview RC 4 同步；error-handling L124 加註 | 32、28、27 |
| `plans/kernel/audit.zh-Hant.md`、`third-party-evaluation/evaluation.zh-Hant.md`（#176 判定總表） | §4.4 四處收窄加五列（audit 既有結論，只落字） | 計畫 §3.1 範圍 |
| `plans/kernel/HANDOFF.md` | 〈暫停〉加「determinism §7／E0～E5 留 legacy、closeout 前定去處」一行；現況 | 33 |
| `plans/kernel/README.md`、dev_log | 批次樹該列、dev_log 一覽；dev_log `docs/dev_logs/<日期>_arch_kernel_a6_revision.zh-Hant.md` | AGENTS §15 |

不碰：任何程式、測試、守衛腳本、CMake、`.github/`；legacy 檔的設計內容；`mechanism.zh-Hant.md`（只改引用的行號若有）；暫停項（DET-0～3、E0～E5、網路帳本）。

## 守衛與突變的去處（計畫 §3 共通）

本批只動 `docs/`，不改任何守衛或突變。唯一提案：**Q10**——`tests/common/neutral_names_guard.py`（或其所在）掃 `docs/architecture/voxel-mesh.md`／`map-markers.md` 的 WORDS 子字串（`zone`／`Extract`／`spawn`），讓 target 對照註記無法寫；提案改為只掃程式與測試。這是**緩める面**，本批在報告附 §14.4 的 1～5（守的性質、緩的面、締的面、性質由誰接、證據）交使用者；核准前，那兩份文件的註記避開這三個子字串；腳本的修改由 kernel-scaffold 做（Q10 處置）。

## 判定（看結果前宣告）

1. **對抗檢查 0 blocker**：Opus／high 只讀（與作者 Fable 不同模型），範圍＝本批 diff；blocker＝改了已定前提、改了 legacy 的設計內容、target 條文互相矛盾、A.6 第三層沒有寫成條件化。should 全處置或寫不採理由。
2. **24 列落字**：計畫 §5 歸本批的 24 列（第 1、6、7、8、9、12、14、19、20、22、23、27、28、29、30、32、33、34、35、36、39、40、41、42）各有「檔＋節」；件数 24＝24，缺一列不成立。
3. **36 條重標**：legacy-impact §2 的 36 條依 `v8-arch-legacy-impact-dispositions` 的定義逐條重標；重標後 被取代 n＋相容（措辭更新）m＝36；「一條兩邊都說不通」→停，列給使用者不自行拆條。
4. **legacy 行號引用歸零**：對 spec／target／mechanism 以**限定 legacy 檔名**的正規式掃（`architecture\.md:\d+`、`entity-model[^ ]*第 ?\d+ ?行`、`determinism\.md:\d+`、`network-architecture[^ ]*第 ?\d+`、`entity-store\.md:\d+`、`error-handling\.md:\d+`），事前基準 **13**（legacy-impact §6），先以一個已知正例命中確認掃描有效、不丟 stderr、結束碼區分零命中與失敗；結果 0。程式碼行號（`MatchRuntimeHost.cpp:441` 類）不在範圍。
5. **目標 DI 清單兩層**：target/README 的清單分「跨部件（先讀）」「各部件」，每條附 legacy 對照（相容／被取代／新）；清單中的 DI 總數＝各 target 檔 `Design Invariants` 節條數之和（件数一致）。
6. **區表判定**：-zones 回報的四項（39～42）各有「判區＋理由＋match-sim 的動作」；判定與 `zone_scan` 三份清單一致或寫明差異。
7. **零程式變更**：`git diff --name-only origin/master` 只含 `docs/`；`docs/architecture/plans/kernel/scripts/` 不動。
8. **Q10 的 §14.4 1～5** 寫在報告並標「待使用者」。

件数の突き合わせ（報告開頭）：「24 列落字 24／24；36 條重標 n＋m＝36；行號引用 13→0；DI 清單 N＝各檔之和；diff 檔數（docs 以外 0）」。

## 範囲の限界

- 只寫文字，不驗條文可實作（由 registry-schedule、world-exec 驗）。
- 「被取代」標記在該部件遷移完成時才真正生效（legacy 規則）。
- U1 未定：A.6 第三層寫成條件化；pageguard-os 時點隨 U1。
- determinism §7 與 E0～E5 不搬（暫停項）；§4.2a 若涉 DET 項目只搬已裁決部分。
- U14：loading／match-sim／match-host 的限界要引 HANDOFF「未結事項」；本批在 spec H.2b 第 4 點旁加「v1 的實現見 HANDOFF 未結事項」註，不改條文。

## 模型／檔位

上段（Design Invariants、契約措辭、OS Delta 文字）。作者：主會話（模型由使用者定）／high；對抗檢查者：**Opus／high 只讀**（與作者不同模型；上段不用小模型）；不需要驗證者（無判定變更，Q10 另送）。估時 6～12 h（出所：計畫 §2.2）；單一文件磨超過 2 小時停下報告。證據：主 checkout `build/evidence/kernel-a6-revision/`（對抗報告、掃描輸出、件数）。

## 停止條件

- A.6 修訂發現 K.3 已決事項互相矛盾 → 停（§14「原因改變前提」）。
- 重標遇到一條兩邊都說不通 → 停，列給使用者。
- 目標 DI 清單在整理時發現 target 檔之間 DI 重複或衝突 → 停，列給使用者（這正是 U13 一次核准前要抓的）。

## 若失敗

後面每批的事前宣告沒有穩定條號可引、宣告不能核准；照計畫 §3 共通「宣告未核准＝不開工」，程式批（registry-schedule 起）也不開工。

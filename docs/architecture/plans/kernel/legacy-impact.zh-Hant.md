# 舊架構文件對新架構的影響掃描（`v8-arch-legacy-impact-scan`）

更新：2026-11-12。Owner：架構（架構師會話開出的任務卡）。**狀態：掃描完成；對抗複核見 §8。只讀：未改任何舊文件、target、spec、測試。**
決定依據：`v8-arch-docs-legacy`（kernel HANDOFF，PR #191 已合併；舊架構設計檔降級 legacy，設計權威即刻在 `docs/architecture/target/`；規則「現有程式受 legacy 的 DI 約束、新程式（`engine/kernel` 起）受 target 約束」）。問題：舊文件裡有什麼會擋住或誤導新架構的實作與遷移？發現只寫、不改；處置交架構師（設計、Delta）與圖書管理員（文件措辭、入鏈）。

**件数の突き合わせ：一致**——掃了 37 檔（卡片點名的對象另有 1 個不存在，列進覆蓋表；`docs/cross-component-rules.md` 在掃描期間由 PR #190 建立、已補掃）；Design Invariants 198 條（相容 177／衝突 2／被取代 19），Replacement Criteria 134 條（相容 116／衝突 1／被取代 17），無 DI 節的檔另判 15 條契約條文（相容 8／衝突 1／被取代 6）；守衛測試與腳本 21 個（會誤判新程式 1 個，條件見 §4）；誤導句 33 條（其中 4 條已被 PR #190／#191 處理、1 條部分處理）；target／spec／mechanism 對 legacy 的反向依賴 91 處（其中行號引用 13 處，PR #191 之後已有實際失效）；對抗複核抽 10 條（含全部衝突 4 條與規則層候選 1 條），一致 8／不一致 2（§8）。

## 0. 範圍、方法與判定

- **掃描對象**（卡片點名；逐檔見 §1）：`docs/architecture.md`、`docs/architecture/*.md`（排除 `plans/`、`target/`）17 檔、`docs/framework/*.md` 3 檔、`docs/object_fps_pvp/entity-model.zh-Hant.md`、`network-architecture.zh-Hant.md`、`docs/*.md` 頂層 7 檔（含 ja 譯本 2 檔）、工具設計文件 5 檔、`AGENTS.md`；`docs/tools/*.md` 頂層與 `docs/cross-component-rules.md` 不存在。
- **legacy 九檔**（`v8-arch-docs-legacy` 點名）：`architecture.md`、`entity-store.md`、`determinism.md`、`error-handling.md`、framework 三份、pvp 的 `entity-model`／`network-architecture`。其餘 `docs/architecture/*.md` 仍是**現況文件**（target/README「分界」），不降級；本報告對它們同樣做四類掃描，因為它們也約束 kernel 要連的模組。
- **對照基準**：`docs/architecture/target/*.md`（10 檔）、`plans/kernel/spec.zh-Hant.md` v0.3（§A～§M、K.1～K.3、附錄）、`mechanism.zh-Hant.md`、`audit.zh-Hant.md`、`ci-readiness.zh-Hant.md` §4、`spike-pageguard-report.zh-Hant.md` 末 11 點、kernel HANDOFF 決定表、AGENTS.md §3、§6～§9。
- **三選一判定的定義**：**相容**＝新舊都成立（kernel 與遷移照它做不違 spec／target）；**衝突**＝照它做會違反 spec 或 target（附 AGENTS §3 七項中會動的項目）；**被取代**＝target 已有新條文，舊條只剩守衛現有程式的作用（遷移批照 target）。
- **判定（看結果前宣告，2026-11-12；parent 會話在本卡的會話列表中找不到，門檻已在開始前給使用者看過、使用者說「開始」）**：
  1. 覆蓋：對象以 origin/master `git ls-files` 列出並逐檔列進 §1，缺一檔即不成立 → **成立**（37 檔＋1 個不存在的對象；cross-component-rules.md 在 PR #190 後補掃）。
  2. Design Invariants：每個 DI 節的每一條與每條 RC 都有三選一判定，附 target／spec 對應位置或「無對應」；衝突類附 Delta 項目 → **成立**（§2、§3）。
  3. 守衛：引用舊文件的測試／腳本都有「遷移時會不會誤判新程式」判定；掃描用 `git grep -nP`，先以正例（`tests/common/entity_store/entity_store_guard.py` 引用 entity-store.md）命中、退出碼 0 → **成立**（§4；證據 `guard-scan*.txt`）。
  4. 對抗複核：另一模型（Opus／high）抽 ≥8 條且含全部衝突類；不一致逐條並列、不改原判定 → 見 §8。
  5. 零修改：`git diff --stat origin/master` 恰為報告、dev_log、kernel README、HANDOFF 四檔 → 見 PR 的 diff。
- **證據**：主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/legacy-impact/`（git 管理外）：`guard-scan.txt`（寬掃，476 命中）、`guard-scan-narrow.txt`（tests／build／.github 對 docs 路徑與 DI 字樣，約 200 命中）、`reverse-scan.txt`（target／spec／mechanism 對舊檔的引用，110 命中）、`guard-scope.txt`、對抗複核原文 `adversary-report.md`。
- **行號基準與偏移**：本報告引用的行號以 origin/master `c3ce8e9f`（開卡時）為準。掃描期間 PR #190、#191 合併（2026-11-12 10:25／10:54 UTC；master `53ead000`），legacy 九檔檔頭加了 `> **Legacy**` 行；本分支已 rebase 到 `53ead000`，但不逐一改寫引用，讀 master 時依下表加偏移：

  | 檔 | 偏移 |
  |---|---|
  | determinism.md、entity-store.md、error-handling.md、framework/architecture.md、event-sounds.md、preview.md | 全檔 +2 |
  | architecture.md | L1-50 → +2；L52 起 → +4（L51 後加 cross-component rules 段）。spec／target 引的 `architecture.md:136` 在 master 已是 L140 |
  | entity-model.zh-Hant.md | L1-15 → +2；L16-57 → +3（起點 2 的 2026-11-12 Kernel 註）；L58 起 → +5（§1.5 補註）。spec 引的「第 139 行」「§5 第 83 行」「第 237 行」在 master 已指到別的內容（**已失效**，§6） |
  | network-architecture.zh-Hant.md | L1-5 → +2；L6 起 → +4（檔頭 Legacy 行＋pv9 補註） |
  | ui_toolchain.md | L203 起 → +1 |
  | 其餘 | 只有同行替換，不偏移 |

- **時間**：約 4 小時（估 4；含 rebase 與併入複核）；沒有單一文件超過 1.5 小時。

## 1. 覆蓋表

欄位：legacy＝是否在 `v8-arch-docs-legacy` 點名的九檔；四類＝①DI／RC 判定、②守衛引用、③誤導句、④反向依賴（有／無）。

| # | 檔 | legacy | ① | ② | ③ | ④ | 備註 |
|---|---|---|---|---|---|---|---|
| 1 | `docs/architecture.md` | 是 | 無 DI 節；條文 6 條判定 | 有（註解引用 2 處） | 有（4） | 有（:136 行號引用 ×4） | Layers 三層表無加註 |
| 2 | `docs/architecture/animation-events.md` | 否 | DI 5／RC 3 全相容 | 有（model links、ae-*） | 無 | 無 | — |
| 3 | `docs/architecture/audio.md` | 否 | DI 11／RC 8 全相容 | 有 | 無 | 有（spec I.1，非 legacy 不斷） | — |
| 4 | `docs/architecture/collision.md` | 否 | DI 3／RC 5 全相容 | 有 | 無 | 有（spec E.2「比照 collision DI 1」） | — |
| 5 | `docs/architecture/determinism.md` | 是 | DI 6／RC 5 全相容 | 無（守衛在產品與各模組） | 有（4） | 有（22 處；target 刻意不重述 §1／§2） | 狀態「proposal」vs target「current contract」 |
| 6 | `docs/architecture/entity-store.md` | 是 | DI 10（相容 8／被取代 2）、RC 15（相容 10／被取代 5） | 有（entity_store_guard、expect_links） | 有（1） | 有（21 處） | — |
| 7 | `docs/architecture/error-handling.md` | 是 | 無 DI 節；條文 8 條（相容 7／**衝突 1**） | 無 | 有（1） | 有（4 處） | L124「no error chains」 |
| 8 | `docs/architecture/field-session.md` | 否 | DI 3（相容 2／**衝突 1**）、RC 2 相容 | 無 | 無 | 無（引 architecture.md） | DI 3 vs 執行期驗證開關 |
| 9 | `docs/architecture/input.md` | 否 | DI 4／RC 3 全相容 | 有（tests/common/input） | 無 | 無 | 注意 <chrono> 型別 |
| 10 | `docs/architecture/io.md` | 否 | DI 5／RC 4 全相容 | 有（OS boundary R1-R4 範圍） | 無 | 無 | — |
| 11 | `docs/architecture/map-markers.md` | 否 | DI 5／RC 3 全相容 | 有（neutral_names_guard） | 無 | 無（引 architecture.md Layers） | — |
| 12 | `docs/architecture/math.md` | 否 | 無 DI 節；Numeric policy 1 條相容 | 有（gyo_math_tests） | 有（1） | 無 | — |
| 13 | `docs/architecture/os.md` | 否 | DI 11 相容、RC 9（相容 8／**衝突 1**） | 有（os_boundary_guard 等 5 個） | 無 | 有（ci-readiness §4.1） | RC 9（D78）vs 頁保護進 engine/os |
| 14 | `docs/architecture/platform-sdl.md` | 否 | DI 10 相容、RC 9（相容 8／被取代 1） | 有（G1-G3、pd-*） | 有（1） | 有（spec L.1 DI 7，非 legacy） | — |
| 15 | `docs/architecture/threads.md` | 否 | DI 9／RC 3 全相容 | 有（RoleThreadTests） | 無 | 有（spec E.3、L.1 DI 4，非 legacy） | 卡片的衝突候選不成立 |
| 16 | `docs/architecture/time.md` | 否 | DI 11／RC 4 全相容 | 有（WaiterTests、links） | 無 | 無 | — |
| 17 | `docs/architecture/trace.md` | 否 | DI 8／RC 3 全相容 | 有（TraceSinkTests、links） | 無 | 無 | RC 1 注意（重放記錄） |
| 18 | `docs/architecture/voxel-mesh.md` | 否 | DI 7／RC 7 全相容 | 有（neutral_names_guard **禁字表會攔 kernel 詞**） | 無 | 無 | — |
| 19 | `docs/framework/architecture.md` | 是（已標 deprecated） | DI 6／RC 8 全被取代 | 有（framework guard、CMake link check、build 層 FRAMEWORK、4 支工具守衛註解） | 有（已標） | 有（2 處「廢止它」） | — |
| 20 | `docs/framework/event-sounds.md` | 是（資料契約 DI 搬 plugin） | DI 6／RC 4 全相容 | 有（EventSoundsTests、es-*） | 有（1） | 無 | — |
| 21 | `docs/framework/preview.md` | 是（資料契約 DI 搬 plugin） | DI 5（相容 4／**衝突 1**）、RC 4（相容 3／被取代 1） | 有（PreviewTests PV2-5、pv-*） | 有（1） | 無 | DI 4 readback vs 資料只往前流 |
| 22 | `docs/object_fps_pvp/entity-model.zh-Hant.md` | 是 | DI 12（相容 9／被取代 3）、RC 6（相容 5／被取代 1） | 有（EntityModelTests、RequireEntityOrder、authority_digest） | 有（3） | 有（21 處；行號引用 6 處） | — |
| 23 | `docs/object_fps_pvp/network-architecture.zh-Hant.md` | 是 | DI 28／RC 13 全相容 | 有（PvpMatchTests:1774、state_whitelist_test.go） | 有（4） | 有（1 處） | — |
| 24 | `docs/creating_apps.md` | 操作類 | 無 DI | 無 | 無 | 無 | §7 列過時項 |
| 25 | `docs/releasing.zh-Hant.md` | 操作類 | 無 DI | 無 | 無 | 無 | 不過時 |
| 26 | `docs/releasing.ja.md` | 操作類（譯本） | 同 25（章節數相同） | 無 | 無 | 無 | 與 zh 同步修 |
| 27 | `docs/rendering_architecture.zh-Hant.md` | 操作／設計 | 無 DI | 無 | 有（1） | 無 | §7 |
| 28 | `docs/rendering_architecture.ja.md` | 譯本 | 同 27 | 無 | 同 27 | 無 | — |
| 29 | `docs/tool_projects.md` | 操作類 | 無 DI | 無 | 有（1） | 無 | §7 |
| 30 | `docs/ui_toolchain.md` | 工具設計 | DI 12（相容 9／被取代 3）、RC 2 相容 | 有（ui_dependency_guard、ui_editor guard） | 有（4） | 無 | — |
| 31 | `docs/audio_editor/architecture.md` | 工具設計 | DI 8（相容 7／被取代 1）、RC 4 相容 | 有（audio_editor guard） | 有（1） | 無 | — |
| 32 | `docs/model_viewer/architecture.md` | 工具設計 | DI 6（相容 3／被取代 3）、RC 4（相容 3／被取代 1） | 有（model_viewer guard） | 有（1） | 無 | — |
| 33 | `docs/voxel_map_editor/architecture.md` | 工具設計 | DI 7（相容 6／被取代 1）、RC 6 相容 | 有（voxel_map_editor guard、W6／W7） | 有（1） | 無 | — |
| 34 | `docs/voxel_map_editor/design.zh-Hant.md` | 工具設計稿 | 無自己的 DI（§5.2 引 architecture.md） | 無 | 有（1；4 處） | 無 | — |
| 35 | `docs/voxel_map_editor/feel-survey.zh-Hant.md` | 工具記錄 | 無 DI | 無 | 有（1） | 無 | — |
| 36 | `AGENTS.md`（§2、§6、§8、§9、§10 引舊結構的條文） | 規則 | 條文 5 處判定（相容 3／被取代 1／**規則層衝突候選 1**） | — | 無 | — | §9 vs plugin 清單在 main |
| 37 | `docs/tools/*.md`（頂層） | — | 不存在（`docs/tools/` 只有 `plans/`） | — | — | — | 覆蓋表保留列 |
| 38 | `docs/cross-component-rules.md`（PR #190 建，24 行，CCR-1） | 否（跨元件規則登記頁） | 無 DI；CCR-1 相容（path dialog 記帳規則不涉 kernel） | 無 | 有（1；§5 第 33 條） | 無 | 「Target owner」欄指向 framework 的 `PathDialogRequests` 並註明 framework 退役進 plugins；plugins 批時改欄 |

## 2. Design Invariants 與 Replacement Criteria 逐條

寫法：相容的條目合併成一列（列出編號與對應的 target／spec 位置）；衝突與被取代逐條展開。「無對應」＝target 沒有新條文、舊條獨立成立。行號以 origin/master `c3ce8e9f` 為準。

### 2.1 legacy 九檔

**`docs/architecture.md`（無 `Design Invariants` 節；以條文判）**

| 條文 | 判定 | 對照 |
|---|---|---|
| L52-85「Layers: admission and format ownership」三層表（Engine／Framework／Product）、Checks 2、4、L84 Guards「framework dependency guard」 | 被取代 | runtime-composition §4：三層改 Engine／plugins／產品與工具；L136 的加註「Until the migration batches land, the three-layer table above describes the running code」只在 L136，表本體無註 |
| L33、L50 `framework/` 的定位 | 被取代 | 同上 |
| L136（2026-11-11 修訂：Kernel 進 Engine；plugin ABI 仍禁；framework 退役） | 被取代／已同步 | spec K.3 第 7 項、X14；target/README K.3 第 7 列 |
| L138「`RuntimeLoop` calls ProcessEvents／Update／Render in order」 | 被取代 | audit §1.1 runtime 列「重寫（RuntimeLoop／IRuntimeClient）」；spec L.1「三段是硬編碼的調度，由 Host 加 ScheduleDef 取代」 |
| L140-155「Fixed simulation time and product hosts」：`FixedTickRuntime` owns accumulator and tick ID；product host owns loops | 被取代 | spec G-6（累加器在 Host／runner plugin、`tickId_` 不是 domain tick 來源）、determinism.md §4.7b；runtime-composition §1、§4（Host＝runner plugin＋產品 main） |
| L108-132 模組表：Framework 列（L132）、`GYO::Engine`「runtime lifecycle」（L118）；L301「`AssetManager` owns IDs/handles…」 | 被取代 | runtime-composition §4；target/assets §1（`AssetManager` 物件消失、拆三類 Resource） |

**`docs/architecture/entity-store.md`（DI L124-172、RC L174-224）**

| 條 | 判定 | 對照與理由 |
|---|---|---|
| DI 2 order invariant、DI 3 binary search no side table、DI 5 store runs nothing、DI 7 misuse is PE、DI 8 deterministic、DI 9 leaf links Base、DI 10 key read one way | 相容 | spec W-1／W-2（J-1、J-2）；A.6「回傳位置的查找版本或 `Cell<T>` 包法」是 API 增補不是側表；J-9（W-10）；J.1 第 3 點引守衛列 9 |
| DI 6 state authority／no change tick | 相容（限 store） | World 每格的 `added`／`changed` stamp（target/world §2、DI 3）屬 World 不屬 store；若 World 以 entity_store 為儲存實作，stamp 陣列要放 kernel 不放 `engine/entity_store`，否則違 DI 6——建議 target/world §6 或遷移批事前宣告寫明 |
| **DI 1** key given never made、no generation bits | 被取代 | target/world §1、DI 1～2：鍵格式 u64 含 generation 位元（Kernel 不解讀）、配發策略是產品 Resource `EntityAllotter`。對 store 本身仍成立（spec A.1 L89、L.1） |
| **DI 4** deferred structural changes（2026-11-11 已改寫指向 spec F.2） | 被取代 | target/world DI 5 是新權威 |
| RC 3、4、7、9、10、11、12、13、14、15 | 相容 | K.2 第 6 條沿用 RC 3／4 門檻；RC 7＝LIM-2＋Jobs 另一能力（spec E.3）；RC 9＝K.2 第 9 條；RC 14 的「owning column type」＝A.6 的有型別欄；RC 15＝K.2 第 13 條同形 |
| **RC 1**「recycle ids → propose a generation in the key; Invariant 1 changes」 | 被取代 | world §1：格式已含 generation、重用是產品配發策略；world RC「World 解讀 generation → 重新設計」。原文會引導遷移者改 DI 1 |
| **RC 2** shared allotment policy → Engine allotter beside the store | 被取代 | world §1：配發器是產品 Resource，不是 Engine 的 |
| **RC 5** intersections hand-written >3 → query layer | 被取代 | world §1：多欄 merge-join 查詢已是契約，不等觀測 |
| **RC 6** change ticks only after measurement | 被取代 | world §2 已決定 stamp |
| **RC 8**「second product needs world/resource/app-plugin assembly → framework-layer question (`docs/architecture.md:129`); plugin ABI ban revisited」 | 被取代 | runtime-composition §4（plugins 層、framework 退役）、architecture.md:136（Kernel 進 Engine）。`:129` 今天是 `GYO::UiRenderer` 列，行號已漂移 |

**`docs/architecture/determinism.md`（DI L194-203 候選、RC L205-213）**

| 條 | 判定 | 對照與理由 |
|---|---|---|
| DI 1 boundary written down | 相容 | target/determinism §1（zone 是宣告並啟動檢查）；G-5「(c) 區資料集與 `DigestFields` 對照（determinism.md DI 1）」 |
| DI 2 one level per zone | 相容 | spec zone∈{C,B} 二分；等級 (a)(b)(c) 三分，(a) 無對應 zone 但不矛盾 |
| DI 3 portable arithmetic | 相容 | target/determinism §4 Math／build 列與 spec G.2 直接引為「current contract DI 3」；Kernel 檢查不了，只輸出 (c) 來源檔清單（J-17） |
| DI 4 order and inputs explicit；「time enters as an input (the tick, or a clock injected at construction, as the host's `now_`)」 | 相容（附誤導句） | target/determinism §2 明寫「DI 4 stands」；但「clock injected at construction」這個形狀在 (c) System 內被禁（spec G.4 第 1 條：`now_()` 回呼改 `HostClockSample`；J-39）→ §5 誤導句 3 |
| DI 5 third-party outside unless proven、DI 6 goldens compare bits | 相容 | audit §1.3 第三方列；spec J.4（遷移後 golden 一位元不變） |
| RC 1、2、4、5 | 相容 | 無對應 |
| RC 3（E0 的 ULP 來自編譯器 → 預測核心降 (b)） | 相容 | spec K.2 第 11 條直接引用 |

**`docs/architecture/error-handling.md`（無 DI 節；規則 1～7 與錯誤型別節）**

| 條文 | 判定 | 對照與理由 |
|---|---|---|
| 規則 1 分類順序、規則 2 wide contracts 回 Result、規則 3 API misuse is PE、規則 4 keep code and detail、規則 5 translate at boundaries、規則 6 no exceptions、規則 7 `Describe` | 相容 | 第三列（L19-30，-spec-v03 已寫入）補在規則之上；Executor 不捕捉例外（spec E.5）；`KernelError` 鏈是 Kernel 自己的型別 |
| **L124**「GYO has no global error enum, no error chains and no type-erased error categories」 | **衝突** | target/error-handling DI 1「wrapped layer by layer（Go `%w` chain）… siblings in one error list」、system-contract §2「包一層自己的上下文回傳」。照 L124 做，`KernelError` 不能是鏈。見 §3 衝突 1 |

**`docs/framework/architecture.md`（已標 deprecated；DI L56-158、RC L160-180）**

| 條 | 判定 | 對照 |
|---|---|---|
| DI 1 not a product／`FRAMEWORK` component、DI 2 dependency one way／core 不碰 SDL・imgui・nlohmann、DI 3 neutral／composition format、DI 4 writes no file、DI 5 core is the product shell／「兩個以上產品以同一方式組合者先進 framework」、DI 6 shell lifecycle（RunWindowShell owns the one RuntimeLoop） | 被取代（全部） | runtime-composition §4、DI 2；audit §3.1、§10.1；HANDOFF `v8-arch-framework-retire`（「DI 不修訂、隨元件退役」）。DI 5 的規則改寫為「先成為共用 plugin」；DI 6 由 runner plugin（R-E6）承接 |
| RC 全部 8 條（L165-180） | 被取代 | 框架退役後不再適用 |

**`docs/framework/event-sounds.md`（DI L148-176、RC L178-187；資料契約 DI 搬 plugin）**

| 條 | 判定 | 對照 |
|---|---|---|
| DI 1 neutral、DI 2 one validator、DI 3 strict、DI 4 bound to one anim_events、DI 6 one schedule | 相容 | owner 改 event-sounds plugin（audit §10.1），內容不變 |
| DI 5 writes nothing（引「the framework's Design Invariant 4」） | 相容 | 出處要改寫（framework DI 4 退役） |
| RC 1～4 | 相容 | RC 4「the framework would copy it」措辭改 plugin |

**`docs/framework/preview.md`（DI L64-88、RC L90-99；資料契約 DI 搬 plugin）**

| 條 | 判定 | 對照與理由 |
|---|---|---|
| DI 1 client's preview、DI 2 neutral、DI 3 Engine draws、DI 5 built where Engine targets are | 相容 | offscreen-preview plugin（audit §10.1）；DI 1 的「windowed shell and ImGui binding neither create nor choose it」改寫為 runner／imgui plugin |
| **DI 4**「Each frame is its own… `EndFrame` hands out that frame's readback… `UploadFrame` leaves the texture holding that frame's bytes」 | **衝突** | runtime-composition §3b／audit §3.2「資料只往前流；readback 路徑消失；預覽視窗＝draw list 裡引用 render target 的貼圖 id」。L3 說資料契約 DI「carry over」會把 DI 4 的 readback 形狀帶進 plugin。見 §3 衝突 2 |
| RC 1～3 | 相容 | — |
| **RC 4**「synchronous readback per frame misses frame-time → async readback／smaller preview／drawing into the window's GPU device」 | 被取代 | target 已選「渲染 World 合成」，三選項都是「readback 放哪」（使用者不採） |

**`docs/object_fps_pvp/entity-model.zh-Hant.md`（DI L258-277 E-1～E-12、RC L279-288）**

| 條 | 判定 | 對照與理由 |
|---|---|---|
| E-1 單一 EntityId 空間、E-2 兩段配發、E-4 以元件挑實體、E-5 欄升冪、E-6 平手鍵、E-8 元件是權威、E-9 id 進 digest 欄序不變、E-10 亂數只來自 `Brain.rngState`、E-11 手寫投影無反射 | 相容 | world §1（PvP 值逐位元相同；配發策略是產品 Resource）；With／Without 即「以元件挑」；W-1／W-2；J.4 golden 不變；target/determinism §3 直接引 E-10；M 表無動態查詢 |
| **E-3** Match 段單調、不回收、Reset 歸回 First、不需 generation | 被取代 | world §1（格式含 generation 位元、PvP 恆 0）；spec A.1 L88／X24：World 內無 Reset，世代切換＝Host 重建 World（同一效果、不同機制） |
| **E-7** 刪實體只走 `PvpMatch::EraseEntity` | 被取代 | World 的 `Despawn` 延後佇列、F.2 despawn 階段依 id、所有欄同窗刪（world §4） |
| **E-12** Tick 內不改成員、待辦固定階段套用、`GYO_ASSERT(!inTick_)` | 被取代 | W-3、F.2；spec L.3「E-12 → DI-KS 9」；§5.2 已加 2026-11-11 修訂註（Join／Leave 進 tick、LIM-5） |
| RC-1、RC-3、RC-4、RC-4b、RC-5 | 相容 | spec K.2 第 6 條沿用 RC-1／RC-3 門檻 |
| **RC-2** 回收 id → E-3 不成立、引入 generation 位元 | 被取代 | 格式已含 generation；重用是配發策略（world §1、RC） |

**`docs/object_fps_pvp/network-architecture.zh-Hant.md`（DI §7 L843-1016 四組表＋一節）**

| 條 | 判定 | 對照 |
|---|---|---|
| §7 主表 9 條（封包／命令／Tick／frame 分開；共用純規則各端持有狀態；ACK 確認已完成模擬；觀測與模擬契約分開；判定姿勢只來自 Match…） | 相容 | mechanism §4 第 3 點（Match 與 Client 共用 FixedUpdate System）；S-8 Diagnostics 只寫不讀回；G.2 Ingress |
| Gateway 無狀態邊緣 4 條＋允許類別（L861-903） | 相容 | spec I.1「Gateway（Go）外部，不變；維持無狀態」；mechanism §4 的「帳本放哪」已暫停（HANDOFF `v8-arch-kernel-pending-paused`） |
| 連線中斷與重新對時 7 條（L911-925） | 相容 | 「中斷判定只讀到達時刻與常數」「epoch 只由 PvpMatch 推進」與 G.4 第 2、4 條同向（到達時刻進 ring；Resync 由 (c) System 從 ring 算） |
| 被替代序號的單一紀錄 3 條（L967-983） | 相容 | 權威／方向規則＝Resource 與唯一寫者的關係 |
| 射擊判定冷卻保留 5 條（L1003-1009） | 相容 | 純規則 |
| RC 四組 13 條 | 相容 | 無對應 |

### 2.2 現況文件（非 legacy；kernel 要連或相鄰的模組）

| 檔 | 條 | 判定 | 對照與理由 |
|---|---|---|---|
| animation-events.md | DI 1～5；RC 1～3 | 相容 | 葉子、格式契約（audit：model 留；loader 是 (c) 內容身分） |
| audio.md | DI 1～11；RC 1～8 | 相容 | audio 判「服務」，`Submitter` 是帶親和的 Resource（spec I.1）；DI 3 的 `MonotonicClock` 時間點只能由 (b) System 給（Z1 擋 (c)）；DI 4 與 AGENTS §3 State Authority 同向；RC 5 的 SPSC 前提在「使用 Submitter 的 System 綁同一角色」下仍成立 |
| collision.md | DI 1～3；RC 1～5 | 相容 | 留；DI 1 被 spec E.2／X-2 與 mechanism 主張 B 引為先例 |
| field-session.md | DI 1、DI 2；RC 1、2 | 相容 | 流程文件 |
| field-session.md | **DI 3**「Test code apart from release code… A runtime flag inside the release binary is not separation」 | **衝突** | system-contract §2b／limits LIM-6：驗證「程式碼永遠在；啟動參數可在 release 開（場次）；同一二進位可開著跑場次測試；開關值進場次記錄 header」。見 §3 衝突 3 |
| input.md | DI 1～4；RC 1～3 | 相容 | input core 留、後端 Host；frame 成 boundary Resource。注意 L32 `std::chrono::steady_clock::time_point`：含 `PhysicalInputFrame` 的檔 include `<chrono>`，只能在 (b) World（J-17 規則 (1)）——遷移批事前宣告寫明 |
| io.md | DI 1～5；RC 1～4 | 相容 | OS boundary R1-R4 只涵蓋 engine/io；`WriteFileAtomically` 讀時鐘與亂數（spec L.1、S17）只能從宣告 `Io:file` 的 (b) System 呼叫、(c) 檔不得 include——io.md 未寫，遷移批寫 |
| map-markers.md | DI 1～5；RC 1～3 | 相容 | 留；DI 1 守衛 `neutral_names_guard.py` 見 §4 第 3 列 |
| math.md | Numeric policy（無 DI 節） | 相容 | determinism DI 3 引用；L78 libm 現況，E1 屬暫停的 DET 線 |
| os.md | DI 1～11 | 相容 | 前提：頁保護平台層進 engine/os（ci-readiness §2.3）。DI 1／R1 會攔 kernel 內的 OS 標頭（「27 筆探測」）——正確攔截不是誤判。DI 3：POSIX 的 mprotect／sigaction 在 libc、Windows 的 VirtualProtect／VEH 在 kernel32（MSVC 預設連結）→ `gyo_expect_links(gyo_os …)` 不必改（ci-readiness §4.1 同判）。DI 6：mprotect 失敗回 `Result<_, OsError>`。DI 8：handler 串接狀態（前任 handler、`reinstalls`）是 engine/os 自己的 OS 資源 |
| os.md | RC 1～8 | 相容 | RC 1：頁大小以中性查詢 `PageSize()` 提供（Apple Silicon 16 KiB／Windows 64 KiB 保留粒度由 kernel 依查詢值處理，不分支 OS）；RC 6：kernel 不可用 allowlist 暫放 OS 碼（會觸發「allowlist 連兩批成長」） |
| os.md | **RC 9**（D78）「engine/os grows only with observed pressure（stricter than AGENTS §2）… A component or batch that no observed pressure and no Feature needs is not started」 | **衝突** | ci-readiness §4.1「頁保護平台層進 engine/os」、spike 建議 10、limits LIM-3「`-spike-pageguard` 可解除 L1、L6」。頁保護是新能力、壓力是預見的。見 §3 衝突 4 |
| platform-sdl.md | DI 1～10 | 相容 | platform 判 Host／runner plugin 零件；DI 7（observer／live frame 內不得做 operation）spec L.1 明寫「是 Host 的 pump 與 live frame 處理器要守的，不進 System」→ runner plugin 設計承接；DI 10 與 audit「登記表與 thread_local 無法成 Resource → Host」一致 |
| platform-sdl.md | RC 1、3～9 | 相容 | — |
| platform-sdl.md | **RC 2**「Apart from the framework's ImGui binding (GYO::ToolFrameworkImGui)… an active product needs a *Native target → the neutral API is cut wrongly」 | 被取代 | ImGui 成 plugin（runtime-composition §4、audit §10.1）：例外名單要改名為 imgui plugin 的 target，否則新 plugin 連 `*Native` 會被讀成「切錯」 |
| threads.md | DI 1～9；RC 1～3 | 相容 | **DI 4「The owner orders roles」vs spec E.3 執行緒親和：不衝突**——E.3 明寫「Executor 不擁有執行緒：專用執行緒由 Host 用 `RoleThread::Start` 建立並決定停止順序（`RoleThread.hpp:62`）」、X-6 v1 不跨執行緒轉送；`Role(name)` 只是啟動檢查。DI 8（body 不讓例外逸出、RoleThread 不 catch）與 E.5（Executor 不捕捉）同向，runner body 負責（H.7）。DI 9：kernel 不連 `GYO::Threads`（J-9 Parallel 模組外無 `<thread>`）。RC 1「thread pool／scheduler → component of its own」正是 Kernel 與 Jobs |
| time.md | DI 1～11；RC 1～4 | 相容 | DI 1 管「量時間用哪個鐘」，target 管「誰可以量」（(c) 檔不得 include `engine/time/MonotonicClock.hpp`／`<chrono>`，J-17）；RC 4：domain tick 是計數非時基、`HostClockSample` 是此時基的取樣 |
| trace.md | DI 1～8；RC 2、3 | 相容 | Diagnostics 接收端（B-13 World 端緩衝在交換點交出）；DI 8 與 `breakOnKernelError` 掛鉤（在 `KernelError` 建構處，不在 Log）不衝突 |
| trace.md | RC 1「consumer needs lines never to be lost (a record used for replay)」 | 相容（注意） | spec G-4 要求 Host 寫入的 (c) 輸入可記錄、重放 host 從記錄重現；重放記錄若走 TraceSink 的丟棄佇列就觸發 RC 1——遷移批設計記錄路徑時避開 |
| voxel-mesh.md | DI 1～7；RC 1～7 | 相容 | 留；契約資料 (c)。DI 1 的共用守衛禁字表見 §4 第 3 列 |

### 2.3 工具設計文件與 AGENTS.md

| 檔 | 條 | 判定 | 對照與理由 |
|---|---|---|---|
| ui_toolchain.md | GYO::Ui DI 1～7；RC 2 條 | 相容 | audit：UiRuntime 包、版面與編解碼留、狀態變 Resource；守衛 `ui_dependency_guard.py` 只看 engine/ui |
| ui_toolchain.md | ui_editor DI 4、DI 5 | 相容 | — |
| ui_toolchain.md | **ui_editor DI 1**「comes through the framework first… the framework's ImGui binding (framework DI 2)」、**DI 2**「The framework's ImGui binding keeps ImGui's side files out (framework DI 4)」、**DI 3**「The mount is the framework's `ReadOnlyAssetCatalog` (framework DI 4)」 | 被取代 | plugins（imgui、sdl-window-runner、asset-catalog plugin；audit §10.1）；規則內容留、owner 與出處改 |
| audio_editor/architecture.md | DI 2～8；RC 1～4 | 相容 | — |
| audio_editor/architecture.md | **DI 1**「comes through the framework first… the framework calls Engine」 | 被取代 | plugins |
| model_viewer/architecture.md | DI 3 lossless、DI 5 preview is Engine's、DI 6 neutral；RC 2～4 | 相容 | — |
| model_viewer/architecture.md | **DI 1**「comes through the framework (ParseEventSounds…)」、**DI 2**「the framework (ParseEventSounds and BindEventSounds) alone」、**DI 4**「through the framework's RegisterAsset」 | 被取代 | event-sounds／asset-catalog plugin；規則留、owner 改 |
| model_viewer/architecture.md | **RC 1**「Engine's offscreen drawing with a synchronous readback and upload misses the frame-time judgment… Propose (an asynchronous readback, a smaller preview, moving the ImGui binding to SDL_GPU)」 | 被取代 | runtime-composition §3b：render target 在渲染 World 合成、readback 消失；三選項都是舊思路（與 preview.md RC 4 同源） |
| voxel_map_editor/architecture.md | DI 2～7；RC 1～6 | 相容 | DI 7 內容（所見即 Engine 的展開）相容；「framework's offscreen preview」措辭與 L204-208 的 readback→ImGui 貼圖是目標不採的形狀，W6／W7 守衛的觀測手段遷移時要重寫（注意） |
| voxel_map_editor/architecture.md | **DI 1**（core 用「the framework's headless shell」；window 用 `GYO::ToolFrameworkImGui`、`GYO::ToolFrameworkPreview`） | 被取代 | headless-runner／imgui／offscreen-preview plugin |
| AGENTS.md §6 L346-355（Application ──→ Engine／Editor ──→ Engine；Engine 不依賴 Application／Editor）、§8 L446-490、§10 L570-580、§3 L241-262 | 相容 | plugins 層在中間（Application／Editor → plugins → Engine）；Engine 不 include plugins（runtime-composition DI 2）；plugins/ 新 top-level 的 Delta 已同意；target/ 在 `docs/architecture/` 下 |
| AGENTS.md §2 L96 基盤例「framework の共通構造」 | 被取代 | framework 退役 → 「plugins 層の共通構造」（風紀委員） |
| AGENTS.md §9「Product-specific Component Selection は Data（Registration／Description Data）に記述する」「同じ事実を手動で維持する場所は一か所」 | **規則層衝突候選** | runtime-composition §4：「產品與工具只在 `main` 列 plugin」「每個 plugin 一個 target、自己的依賴正向清單」。plugin 清單在程式（main＋CMake），Engine components 仍在 `project.json`（資料）→「這個產品用哪些能力」兩處維護。AGENTS §3 的「Product Registration 構造の変更」Delta。見 §3 候選 5 |

## 3. 衝突：Architecture Delta 七項對照（AGENTS §3）

每條衝突寫「照它做會違反什麼」與七項中會動的項目；處置交架構師。

### 衝突 1：`error-handling.md` L124「GYO has no … error chains」 vs `KernelError` 的上下文鏈

- 違反：target/error-handling DI 1「wrapped layer by layer … siblings in one error list」；system-contract §2「System 收到錯誤後包一層自己的上下文回傳（Go 的 `%w` 鏈）」。照 L124 做，`KernelError` 只能是單層碼，Executor 記錄不到鏈路。
- 七項：① Feature＝Kernel 的錯誤回傳契約（K.3 第 17 項）；② 現況＝`Base::Error<Code>` 單層、文件全域禁鏈；③ 邊界＝Base 的錯誤模型 vs Kernel 的錯誤型別；④ 影響＝`engine/base`（不改）、`engine/kernel`（`KernelError` 帶鏈）、error-handling.md 條文；⑤ 依賴方向不變；⑥ Ownership＝鏈的表示法 owner 是 Kernel（`Base::Result` 仍純值，target/error-handling DI 2）；⑦ 更小方案＝把 L124 收窄為「`Base::Error` 不含鏈；Kernel 的 `KernelError` 是唯一帶鏈的錯誤型別」即可，不必改 Base。
- 建議：target/error-handling 合併回現檔時改寫 L124；在那之前 kernel 第一批的事前宣告引用 target 條文並註明與 L124 的關係。

### 衝突 2：`framework/preview.md` DI 4（readback → 視窗貼圖）vs 資料只往前流

- 違反：runtime-composition §3b／audit §3.2「沒有影像回到文件 World；H-6 原樣、不開例外、沒有晚一幀；readback 路徑消失；預覽視窗＝draw list 裡引用 render target 的貼圖 id」。preview.md L3 說資料契約 DI「carry over」到 plugin，DI 4 正是 readback 形狀；照它搬，offscreen-preview plugin 會把 readback 帶進新架構，違 H-6 或要求「晚一幀」。
- 七項：① Feature＝Editor 預覽（三個有預覽的工具）；② 現況＝`OffscreenPreview::EndFrame` 讀回、`UploadFrame` 上傳到視窗 renderer；③ 邊界＝Editor 的資料流（文件 World → Extract → 渲染 World 合成）；④ 影響＝offscreen-preview、imgui plugin、model_viewer／voxel_map_editor／ui_editor 的預覽碼、`tests/framework/PreviewTests.cpp` PV2～PV5 與 pv-* 突變（守 readback 行為，要重寫不是搬）；⑤ 依賴＝plugin 間（offscreen-preview → imgui 的貼圖 id 解析，audit §10.4）；⑥ Ownership＝render target 的合成在渲染 World（offscreen-preview plugin），不在工具；⑦ 更小方案＝不能保留 DI 4 又滿足 H-6。
- 建議：第一個文件批把 preview.md L3 的「carry over」限縮為 DI 1～3、5；DI 4 改寫為「每幀各自：render target 在渲染 World 合成，draw list 以貼圖 id 引用」；model_viewer RC 1、preview RC 4 同步。

### 衝突 3：`field-session.md` DI 3「A runtime flag inside the release binary is not separation」 vs 執行期驗證開關

- 違反：system-contract §2b 條件 3「預設跟建置變體，參數只能覆寫：啟動參數可在 release 開（場次）」、「同一二進位可開著跑場次測試」；limits LIM-6「開關值進啟動日誌、場次記錄 header」。照 DI 3 字面做（把驗證碼編譯出 release）會讓「release 開驗證跑場次」不可能。
- 範圖：只在「Kernel 存取驗證與 `breakOnKernelError`」這一類；場次的高頻記錄（每 tick／每幀）仍照 DI 3 走編譯期變體。兩份文件對「場次用的測試版」定義不同（編譯期分離 vs 執行期開關）。
- 七項：① Feature＝場次中開著驗證抓繞路（spec A.6；「無違規」報告帶「驗證開著」）；② 現況＝field-session DI 3 要求 collection code 不進 release；③ 邊界＝release／test variant 的定義；④ 影響＝field-session.md、kernel 的 `KernelSettings`、package check（「拒絕 metadata 標 test variant 的套件」要能接受「release 二進位＋啟動參數」）、C1 的「configuration in force」；⑤ 依賴不變；⑥ Ownership＝開關狀態的證據歸 C1 的 session facts；⑦ 更小方案＝field-session DI 3 加一句例外（「Kernel 的執行期驗證參數不是本條的 collection code；其值進 C1」），不改 §2b 的四條件（使用者定）。

### 衝突 4：`os.md` RC 9（D78「只以觀測壓力成長」）vs 頁保護平台層進 `engine/os`

- 違反：ci-readiness §4.1「頁保護平台層進 `engine/os`」；spike 建議 10「handler／mmap／VirtualProtect 進 Engine 時應放 OS 子系統」；limits LIM-3「`-spike-pageguard` 可解除 L1、L6」。頁保護是**新能力**（不是搬遷），壓力是**預見的**（LIM-3 的解除；`v8-arch-kernel-pageguard-reading`：頁保護暫不進 A.6）。照 RC 9 做＝不開這個 engine/os 元件；照 os.md DI 1／R1 做＝不能放 kernel（OS boundary guard 掃 engine/kernel；「27 筆探測」）。兩條合起來把頁保護鎖在門外。
- 條件：使用者決定採用存取驗證第三層（頁保護）時成立；今天 A.6 未收進，衝突是潛在的，但 ci-readiness §4.1 已點名要 Delta。
- 七項：① Feature＝存取驗證第三層（解 LIM-3 的 L1）；② 現況＝RC 9 只准觀測壓力、R1 禁 kernel 持 OS 碼、allowlist 走法觸 RC 6；③ 邊界＝engine/os 新增「記憶體保護與故障處理」能力；④ 影響＝`engine/os`、`engine/kernel`（連 `GYO::Os` PRIVATE → kernel 改 STATIC）、`tests/common/os`（allowlist 不動、`gyo_expect_links(gyo_os …)` 不動）、第三層測試執行檔（`SKIP_RETURN_CODE 77`）；⑤ 依賴＝新邊 kernel → GYO::Os；⑥ Ownership＝平台 handler 與保護 API 歸 engine/os，區間表與違規記錄歸 kernel（spike 建議 5、10）；⑦ 更小方案＝R1 不允許 kernel 自帶、allowlist 觸 RC 6，所以沒有。
- 注意：RC 9 是使用者的 owner 決定（D78，`os-closeout-d78`），比 AGENTS §2「基盤可預見」更嚴；改它或開例外都要使用者。

### 候選 5（規則層，非 DI）：AGENTS §9「Product-specific Component Selection は Data に記述」 vs 「產品只在 `main` 列 plugin」

- 張力：runtime-composition §4 讓 plugin 清單在程式（`main`）與 CMake 連結；Engine components（SDL_RENDERER 等）今天在 `project.json`（資料）。同一事實（「這個產品用哪些能力」）會有兩處；AGENTS §3 把「Product Registration 構造の変更」列為 Delta。
- 處置：plugins 層進 build graph 的計畫批要決定 plugin 的選擇是 `project.json` 的 components（資料）、還是 CMake 連結（程式），並說明與 §9 的關係；風紀委員不先改條文。交架構師。

### 卡片列的衝突候選中不成立的

- **threads.md DI 4「The owner orders roles」vs Executor 執行緒親和**：相容。spec E.3 明寫 Executor 不擁有執行緒、專用執行緒由 Host 用 `RoleThread::Start` 建立並決定停止順序（引 `RoleThread.hpp:62`）；`Role(name)` 親和只是啟動檢查「schedule 的 runner 角色＝此名」；X-6 v1 不跨執行緒轉送。
- **determinism.md DI 1～3 vs spec G／區表**：相容。target/determinism 刻意不重述 §1／§2 並引 DI 3 為 Math／build 契約；DI 4 的「clock injected」只是誤導句（§5 第 3 條），target 已說明「DI 4 stands」。
- **entity-store DI vs World 建在其上**：DI 1、DI 4 被取代，其餘相容（store 本身仍成立，spec L.1）。注意 DI 6 的 stamp 位置（§2.1）。

## 4. 守衛測試與腳本：遷移時會不會把新程式判成違規

掃描：`git grep -nP`（正例 `entity_store_guard.py:1` 命中 `entity-store`，退出碼 0；證據 `guard-scan-narrow.txt`）。「會不會誤判」指對 `engine/kernel`、`tests/common/kernel` 與遷移後的 plugins／產品程式。

| # | 守衛／腳本 | 引用的舊文件 | 遷移時會不會誤判新程式 | 說明 |
|---|---|---|---|---|
| 1 | `tests/common/entity_store/entity_store_guard.py`（W1～W5） | entity-store.md DI 8、9 | 不會 | `SCANNED = "engine/entity_store"`（:39）；不掃 kernel。注意 W1 字表 `object_fps|RetroFPS|fps::pvp|\b[Pp]vp|[Pp]layer|Gateway|\bMatch\b`（:42）：若把 World 的欄實作放進 `engine/entity_store`，`Player`／`Match` 這類詞會被攔——kernel 放 `engine/kernel` 即可 |
| 2 | `gyo_expect_links(gyo_entity_store GYO::Base)`（`tests/common/entity_store/CMakeLists.txt:35`） | entity-store.md DI 9 | 不會 | 只看 `gyo_entity_store` 目標的連結；kernel 連 `GYO::EntityStore` 不改它 |
| 3 | `tests/common/scripts/neutral_names_guard.py` | voxel-mesh.md DI 1、map-markers.md DI 1；architecture.md Layers check 1 | **會（條件：被套到 kernel）** | `WORDS = ("spawn", "boss", "loot", "extract", "patrol", "team", "zone")`（:20），大小寫不分。Kernel 規格詞彙含 `Spawn`／`SpawnWithId`／despawn、`zone ∈ {C, B}`、`Extract<Read<T>>`／`ExtractRes`。樣式是子字串、不分大小寫、無 `\b`（:24），所以 `Despawn`、`ExtractRes`、`timezone` 都命中（複核者在 scratchpad 實測，退出碼 1）。今天掛在 voxel_mesh 與 map_markers 的 CTest（`tests/common/voxel_mesh/CMakeLists.txt:10`、`tests/common/map_markers/CMakeLists.txt:18`），掃的是那兩個 engine 目錄、它們的測試**與 voxel-mesh.md、map-markers.md 兩份契約文件**→ 現況不誤判，但遷移批或文件批只要在那兩份文件寫進 `zone`／`Extract`（例如加 target 對照註）就會失敗；J-9「無產品名」掃描**不可沿用** WORDS，要自寫產品名禁字表（照 entity_store_guard W1 的形狀） |
| 4 | `tests/common/os/os_boundary_guard.py`（R1～R6；ROOTS engine、apps、tools、build/acceptance、tests） | os.md DI 1～3 | 不會誤判（正確攔截） | kernel 含 `<sys/mman.h>`、`<signal.h>`、`windows.h`、VEH 就違 R1（ci-readiness「27 筆探測」）→ 要求頁保護平台層進 engine/os（§3 衝突 4）。kernel 本體（無 OS 碼）不受影響 |
| 5 | OS 編譯定義檢查（`tests/common/os/CMakeLists.txt:63-101`） | os.md DI 1 | 不會 | kernel 目標不得帶 OS 字樣的定義；spike 的 NDEBUG 雙目標不是 OS 定義 |
| 6 | `ProcessCodePageCheck.cmake`（DI 11） | os.md DI 11 | 不會 | 新測試執行檔在 `tests/common` 下自動帶 `GYO_PROCESS_CODE_PAGE`；第三層獨立執行檔也要在掃描根下 |
| 7 | `gyo_expect_os_links`（`tests/common/os/CMakeLists.txt:54-61`） | os.md DI 3 | 不會 | 頁保護不加系統庫（libc／kernel32 預設） |
| 8 | `OsHeaderHygieneTests.cpp`（DI 4） | os.md DI 4 | 不會 | 只檢 engine/os 公開標頭 |
| 9 | platform-sdl G1／G2／G3（`tests/common/platform_sdl/`；G2 ratchet 掃 `engine/**/include` 外 `native/`） | platform-sdl.md DI 1、2 | 不會 | kernel 標頭無 `SDL_` 符號 |
| 10 | `tests/framework/framework_dependency_guard.py`、`tests/framework/CMakeLists.txt:347` link check、PV／fw-* 突變 | framework/architecture.md DI 1～3、6；preview.md | 不涉 kernel | 隨 framework 程式刪；PreviewTests PV2～PV5 守 readback 行為，plugin 版要重寫不是搬（§3 衝突 2） |
| 11 | 工具守衛 ×4：`tests/model_viewer/model_viewer_dependency_guard.py`（:33、:43-46）、`tests/ui_editor/ui_editor_dependency_guard.py`（:28、:33）、`tests/audio_editor/audio_editor_dependency_guard.py`（:25、:32）、`tests/voxel_map_editor/voxel_map_editor_dependency_guard.py`（:49-54） | framework/architecture.md DI 2 | 不涉 kernel；**遷移時必改** | 正向清單含 `GYO::ToolFramework*` 目標與 `gyo/framework/` 前綴；plugins 遷移時清單改掛 plugin 目標，註解的出處改 |
| 12 | `build/cmake/GyoComponents.cmake:3-5`（`FRAMEWORK` 是 `GYO_KNOWN_COMPONENTS`）、`build/cmake/GyoBuild.cmake:55-56、91`（`GYO_ENABLE_FRAMEWORK`） | framework/architecture.md DI 1 | 不涉 kernel；plugins 進 build graph 時改 | build 層以 framework DI 1 為據；是 §3 的 Delta 項 4 影響檔（plugins/ top-level） |
| 13 | `tests/common/cmake/ToolRegistryTests.cmake:100-102`（保留名 `framework` 不可當 tool） | framework/architecture.md DI 1 | 不涉 kernel | 退役後保留名留或刪由 plugins 批定（§7 不留 Dummy） |
| 14 | `tests/common/threads/RoleThreadTests.cpp:94`（DI 6）、`tests/common/time/WaiterTests.cpp:31`（DI 7）、`tests/common/time/CMakeLists.txt` links（threads DI 9、time DI 10） | threads.md、time.md | 不會 | 只測各自模組；kernel 不連 Threads |
| 15 | `tests/common/trace/CMakeLists.txt:9` links（DI 7）、TraceSinkTests | trace.md | 不會 | — |
| 16 | `gyo_expect_collision/model/map_markers/voxel_mesh/audio_links`、`gyo_audio_board_readers`、`gyo_audio_alloc_tests` | collision、animation-events、map-markers、voxel-mesh、audio | 不會 | 只看各自目標 |
| 17 | `tests/common/input/`（IP-1、m7～m9）、`tests/common/ui/ui_dependency_guard.py` | input.md、ui_toolchain GYO::Ui DI 2、7 | 不會 | 範圍 engine/input、engine/ui |
| 18 | `tests/object_fps_pvp/EntityModelTests.cpp`（G1、G2、G7、G13）、`RequireEntityOrder`、`authority_digest`（G11，37 golden） | entity-model.zh-Hant.md | 不會（產品守衛） | 遷移後 G1／G2 守的性質由 J-1／J-32 承擔；golden 一位元不變是驗收（spec J.4）；LIM-5 的 golden 差一 tick 由遷移批重設基準（§14.4） |
| 19 | `tests/object_fps_pvp/PvpMatchTests.cpp:1774`（network §7 連線中斷 DI）、`fixtures/ingress_v1/README.zh-Hant.md:59` | network-architecture.zh-Hant.md §7 | 不會（產品） | 註解引用；刪檔時懸空 |
| 20 | `apps/object_fps_pvp/gateway/state_whitelist_test.go` | network-architecture §7 Gateway | 不會（Go） | — |
| 21 | 註解引用 architecture.md：`tests/common/scripts/neutral_names_guard.py:3`（Layers check 1）、`tests/common/collision/CMakeLists.txt:10`（module table） | architecture.md | 不會 | 刪檔時註解懸空 |

結論：對 `engine/kernel` 會誤判的只有第 3 列，且只在把共用禁字表套到 kernel 時；第 4 列是正確攔截（它決定頁保護的位置）；第 10～13 列是 framework 退役→plugins 的影響檔（Delta 項 4）。無守衛以 determinism.md、error-handling.md 為判定依據。

## 5. 誤導性說法（仍以現在式寫、已被 target 取代或與 spec 相反；只列不改）

| # | 檔：行 | 原文摘錄 | 為什麼誤導 | 建議處置（圖書管理員） |
|---|---|---|---|---|
| 1 | determinism.md:4 | "Status: proposal. The Design Invariants section below is a candidate; it binds only after the user approves it." | target/determinism L5、L21、L36 與 spec G.2 把 DI 3、DI 4 當 "current contract" 引用 | 架構師定 DI 1～6 是否已核准；狀態行同步 |
| 2 | determinism.md:13-20 | "## 0. Decisions for the user … DET-0 … DET-3" | HANDOFF `v8-arch-kernel-pending-paused`（2026-11-12）已把 DET-0～3 列「暫停、改造完成前不討論」 | §0 加「暫停」註 |
| 3 | determinism.md:201 | "time enters as an input (the tick, or a clock injected at construction, as the host's `now_`)" | (c) System 內注入時鐘回呼被禁（spec G.4 第 1 條：`now_()` → `HostClockSample`；J-39）；target 說 DI 4 stands 但形狀變了 | 合併時改「the tick, or a recorded boundary input」 |
| 4 | determinism.md:186-190 | "§8 Wording for the Kernel blueprint (DI-K5) … 變更 tick 用遊戲 tick 標記 … 時間只來自 tick … the wording below now serves as the definition behind mechanism §3.1／§3.3" | 兩句已被 K.3 第 2、11 項取代（spec X2、X22） | 加「已修訂」註 |
| 5 | error-handling.md:81 | "There is no debug-only level yet. `GYO_DEBUG_ASSERT`, for checks too expensive to run in release, will be added together with the first such check." | 第一個「太貴不能在 release 跑」的檢查（A.6 存取驗證）已決定走執行期開關的「驗證」，不是 `GYO_DEBUG_ASSERT`（system-contract §2b、`v8-arch-kernel-verification-not-assert`） | 改寫或加註 |
| 6 | entity-store.md:24-30 | 圖 "consumer (a product) … owns: … frozen phases and the sync point" | 目標下 owner 是 World（DI 4 已改，圖未改） | 改圖 |
| 7 | math.md:5 | "inactive products still use the removed names and must migrate before re-enabling" | object_fps、object_fps_v2 已於 `v8-arch-remove-object-fps` 刪除，不會 re-enable；三份 inactive_products.md 成歷史 | 改寫 |
| 8 | architecture.md:52-85 | Layers 三層表（Engine／Framework／Product）與 Checks 2、4 | framework 退役；加註只在 L136，表本體無註 | 表上方加一行指向 L136 與 runtime-composition §4 |
| 9 | architecture.md:138 | "`RuntimeLoop` calls `ProcessEvents(frame)`, `Update(frame)` and `Render(frame)` in order." | audit 判重寫（Host＋ScheduleDef 取代） | 加「現況；目標下由 schedule 取代」註 |
| 10 | architecture.md:142-155 | "`FixedTickRuntime` owns the conversion … The runtime owns its accumulator and tick ID … The product host owns process lifetime, its loops and deadlines" | spec G-6／determinism §4.7b：累加器在 runner plugin、`tickId_` 不是 domain tick 來源；Host＝runner plugin＋產品 main | 加註 |
| 11 | architecture.md:301 | "`AssetManager` owns IDs/handles, catalog lookup, records, cache/lifetime and loader dispatch." | target/assets §1：`AssetManager` 物件消失、拆三類 Resource | 加註 |
| 12 | framework/event-sounds.md:119 | "The model viewer is planned and not built: today one product reads the mapping" | model_viewer 已建、是第二讀者（`v8-tool-model-viewer-event-sounds`） | 改寫 |
| 13 | framework/preview.md:16-18、L3 | "reads each frame back and shows it as a texture of the window's own SDL renderer"；L3 "The data contract and its Design Invariants below carry over" | readback 形狀是使用者不採的；L3 等於肯定它會搬 | §3 衝突 2 |
| 14 | entity-model.zh-Hant.md:13 | 「採『ECS 的資料佈局，不採通用 ECS 執行期』…系統＝對這些陣列切片的自由函數」 | 與目標（World／System／Scheduler／Executor）相反、無註 | **部分處理**（PR #191 在起點 2 加的 Kernel 註涵蓋概念，起點 1 本文仍無註）；補一句 |
| 15 | entity-model.zh-Hant.md:14-15、380 | 起點 2「Engine 不加 ECS」與 §11 第 5 項的 2026-11-05 註只提 entity-store，沒提 Kernel 進 Engine | 讀者會以為 Engine 只到 entity-store | **已處理**（PR #191，2026-11-12 註） |
| 16 | entity-model.zh-Hant.md:153 | 「PlayerState 的拆欄不在 split 做…拆欄的壓力在第一個 AI 批出現」 | K.3 第 9 項已決定依第 139 行分類拆欄（audit §4.1） | 加註 |
| 17 | network-architecture.zh-Hant.md:180-181 | 「Engine 決定時間如何切成 Tick 與如何等到期限；Host 與兩個 Client 角色決定執行緒如何存活」 | 目標：tick 由 clock schedule 推進、累加器在 runner plugin（G-6）；執行緒由 runner plugin | 加註 |
| 18 | network-architecture.zh-Hant.md:225-227 | 「`gyo-match-sim`（`MatchRuntimeHost::Body`）使用 `steady_clock` 讀 elapsed，呼叫 `Advance`，再以 Waiter 等到下一個 Tick 格點」 | audit：MatchRuntimeHost 重寫為 Host＋Ingress／Judge／Egress；H.4.1 | 加註 |
| 19 | network-architecture.zh-Hant.md:1034-1057 | 「修改入口與 Ownership」表（PvpMatch.cpp、MatchRuntimeHost.cpp、IpcHost.cpp…） | 遷移後入口失效（審查判重寫 6 子列） | 表頭加「現況入口；遷移批改寫」 |
| 20 | network-architecture.zh-Hant.md:1059-1063 | 「尚不足以把這套策略直接下沉為 Engine API…未來若第二個產品出現相同責任，應先比較實際共同契約」 | 與 `v8-arch-kernel-foundation-first`（基盤不等壓力）相反的舊觀點；雖標「v3 的記錄」，仍像原則 | 加「已被 foundation-first 取代」註 |
| 21 | platform-sdl.md:34 | "the caller decides what happens next (the framework's windowed shell asks its client, `v8-framework-close-request`)" | framework 退役；關窗請求由 SDL 視窗 runner plugin 承接（audit §10.1） | 改措辭 |
| 22 | ui_toolchain.md:6-16 | 圖 "GYO::UiRenderer ------------> AssetManager, text rasterizer, RenderQueue Overlay" | `AssetManager` 消失；`UiRenderer` 局部重寫並照 naming 第 5 條改名（assets §2） | 加註 |
| 23 | ui_toolchain.md:203-205 | "Object_FPS stores Exposure and Gamma in its app runtime client." | 產品已刪，段落懸空 | **已處理**（PR #190 改寫） |
| 24 | ui_toolchain.md:223 | "does not link ObjectFPS" | object_fps 已刪 | **已處理**（PR #190） |
| 25 | ui_toolchain.md:279-281 | "must not depend on the UI editor or turn it into a generic plugin host" | 「plugin host」與目標的 plugins 層撞名（此處指 ABI 式外掛宿主，仍禁；plugins 層的 plugin 是 CMake target） | 加註區分 |
| 26 | audio_editor/architecture.md:86-90 | "Window: the framework's ImGui shell (`RunImGuiShell`); its context lends Engine's platform" | framework 退役，無 deprecated 註 | 加註 |
| 27 | model_viewer/architecture.md:80-82、88-95 | "runs on the framework's ImGui shell … `OffscreenPreview`: the offscreen GPU device, Engine's `Renderer` and the readback … The framework's `UploadFrame`" | readback 形狀是目標不採的；framework 退役 | 加註；RC 1 見 §2.3 |
| 28 | voxel_map_editor/architecture.md:201、204-208 | "No orthographic view yet (it needs the framework or Engine)"；預覽 readback→ImGui 貼圖路徑 | framework 退役（→ plugin 或 Engine）；readback 形狀 | 加註 |
| 29 | voxel_map_editor/design.zh-Hant.md:279、281、369、370 | 「不必改 framework」「另開 framework 的卡（owner 是架構會話）」「會碰 framework 或 Engine 的相機」「碰 framework：預期沒有」 | framework 退役 → plugin | 改措辭（4 處） |
| 30 | voxel_map_editor/feel-survey.zh-Hant.md:58 | 「`OrbitCamera`（framework）」 | `OrbitCamera` 成函式庫（audit §10.1） | 改出處 |
| 31 | rendering_architecture.zh-Hant.md:66、129（ja 同位置） | 「Object_FPS：決定物件、相機、材質 shader ID、武器動作」「Object_FPS 目前僅包含 shaders/builtin」 | object_fps 已刪；現役是 object_fps_pvp | **已處理**（PR #190） |
| 32 | tool_projects.md:15 | "every supported package platform (windows-x64, linux-x64, macos-arm64, macos-x64 …)" | L1／release 平台自 2026-10-31 只有三個（determinism.md:32、releasing §5） | 改寫 |
| 33 | cross-component-rules.md:3-4（master） | "a direct consequence of an Engine or framework contract"；CCR-1「Target owner」＝framework `PathDialogRequests` | framework 退役；該欄已註明退役進 plugins，但規則的定義句仍以 framework 為層 | plugins 批時改「Engine or plugin contract」（複核者指出） |

## 6. target／spec 對 legacy 的反向依賴（舊檔刪掉時會斷的引用）

來源：`reverse-scan.txt`（110 命中，逐筆歸類；非 legacy 檔的引用另列）。「行號引用」＝引用舊檔的**行號**，最脆弱。

| 被引用的 legacy 檔 | 引用處（target／spec／mechanism） | 處 | 刪檔前要做 |
|---|---|---|---|
| `determinism.md` | target/determinism.md L5（「the levels and zones themselves are defined by the current contract … §1, §2, which this page does not change」）、L21（DI 4）、L36（current contract DI 3、E0 to E5）；target/README L55（A6）、L84（目錄列，提到「最終併入的 docs/architecture/determinism.md」）；target/assets L15、L28（§4.9）；target/naming L24（§4 區表）；spec L15、L51（§1、§2）、L732、L734、L756（DI 4）、L757（§2 time is an input）、L758（DI 3）、L768（DI 1）、L782（DI 1）、L835、L884（§2 scripted clock）、L895（§3）、L1159（RC 3）、L1178（DI 4 不動）、L1197（§4.8）、L1199（§4.6）、L1201（§2）、L1214（§4.9）、L1248（§4.9）、L1339（B6：DI 4）；mechanism L87（DI 4）、L89 | 27 | **target/determinism.md 刻意不重述三級（§1）與區表（§2）**：刪 legacy 前先把 §1、§2、§4 區表（含 4.7a／4.7b）、§7 gaps、E0～E5 搬進 target 或合併回現檔；DI 1、3、4 的狀態（候選／核准）先定 |
| `entity-store.md` | spec L89（DI 1）、L150（Invalidation）、L167（守衛列 5、6）、L172（DI 2）、L174（RC-1 門檻）、L190（DI 1）、L574（Out of scope：Jobs）、L618（throw 契約）、L650、L713（DI 4）、L723（DI 4）、L724、L993（守衛列 5、6、8）、L1026（守衛列 9）、L1094（mutations.json 做法）、L1138（K.1 第 9 條延伸 DI 4）、L1200（DI 1、3、5、6）、L1298（X4）、L1308（X14）、L1401（N-1）、L201（A.8 取捨 1 引 DI 1）；target/README L63 | 22 | 守衛列 5、6、8、9 的「照 entity-store 的做法」要在 kernel 計畫資料夾自帶描述；DI 1～10 的去處（搬回或併進 world 文件）先定 |
| `entity-model.zh-Hant.md` | spec L88（E-3、**§5 第 83 行**）、L89（E-1～E-3、E-9）、L253（**第 139 行**）、L656（§5.3）、L713（E-12）、L1154（RC-1／RC-3）、L1176（**第 139 行**）、L1177（§5.2）、L1249、L1274（RC-3）、L1309（X15）、L1314（X20 E-10）、L1317（X23 §5.2）、L1318（X24 E-3、**§5 第 83 行**）、L1363（**第 3 行**）、L1379（**第 3、83、91、139、237、242、244、264-265 行**）、L201、L853（**§5.2 第 237 行**）、L1236（**第 3、139 行**）、L1341（**第 139 行**）、L1407；target/determinism L25（E-10）；target/world L13（E-1～E-3）、L30（第 139 行的欄位分類＝拆欄依據在 target 的唯一出處）；target/runtime-composition L26（§5.2）；limits L15（§5.2）；mechanism L125（E-12） | 27（行號引用 9） | 「第 139 行的欄位分類」是拆欄的依據（K.3 第 9 項）：把那張表（Body／Life／InputCursor／Connection 診斷）搬進 target/world 或 product 的目標文件，再引條號不引行號 |
| `architecture.md` | spec L1171（K.3 第 7 項 `:136`）、L1279（M 表 `:136`）、L1308（X14 `:136`）；target/README L63（`:136`） | 4（全是行號） | 行號引用改成章節名（「Runtime boundaries 段的禁令」）；PR #190 後 `:136` 已是 L140（**已失效**）；entity-store RC 8 的 `:129` 已漂移 |
| `error-handling.md` | target/error-handling L5（「Extends the current contract」、「amended in -spec-v03」）、L9（兩欄表）；target/README L83（目錄列，提到「最終併入的」路徑）；target/system-contract L20；spec §讀法 L21、L1184（K.3 第 17 項）、L1405（B-8） | 7 | target 合併回現檔即可（target 自帶全文） |
| `framework/architecture.md` | target/runtime-composition L48（「DI 整節廢止」）、target/README L50（A1） | 2 | 「廢止它」的引用；刪檔時改成「已於 <批> 刪除」 |
| `network-architecture.zh-Hant.md` | spec L1376（附錄二：grep 無 refusal 條目） | 1 | 無 |
| 小計 | | **91**（target 部件文件 20、spec 68、mechanism 3；行號引用 13）。複核者以 `git grep -nP` 重掃得 82 命中行（spec 64、target 15、mechanism 3），口徑是「行」；本表是「處」，一行可含多處 | |

非 legacy 現況檔的引用（刪檔不在議程，列作對照）：spec L560（collision DI 1）、L1206（threads DI 4）、L1211／L1377（platform-sdl DI 7）、I.1 表（audio.md）。

另（其他舊檔→legacy，刪檔時斷但不屬 target）：field-session.md C3→architecture.md「fixed simulation time」；map-markers.md L68／RC 3、event-sounds.md L117、preview.md L53→architecture.md「Layers」；math.md L3→error-handling.md；rendering_architecture L3→architecture.md；`neutral_names_guard.py:3`、collision CMake :10→architecture.md。

## 7. 操作類文件：會因 framework 退役→plugins、object_fps 已刪、kernel 新 target 而過時的流程（只列）

| 檔 | 段 | 過時原因 | 時點 |
|---|---|---|---|
| creating_apps.md | L31-41 最小專案（`main.cpp` 直接 `LIBRARIES GYO::Engine`；`project.json` components）；L41 公開 target 清單 | 目標下產品＝plugin 清單＋Run、連 `plugins/` 目標（runtime-composition §4、DI 2）；清單缺 Kernel／plugins | plugins 進 build graph、kernel 進 L1 時 |
| creating_apps.md | L142 Manual copy | 已更新（object_fps 刪除） | — |
| tool_projects.md | L35-58 Add a tool by hand 步驟 3（`target_link_libraries(example_tool PRIVATE GYO::Engine)`） | 工具＝plugin 清單＋Run | plugins 進 build graph 時 |
| tool_projects.md | L15 四平台 | L1 三平台（2026-10-31） | 現在（與 kernel 無關） |
| ui_toolchain.md | L207-225 Editor workflow（`gyo_ui_editor` 建置、standalone entry） | 不過時（選擇與建置路徑不變）；L223「does not link ObjectFPS」措辭 | — |
| rendering_architecture.zh-Hant.md／ja | §7 建置選擇、§8 部署、§9 整合與驗收 | 不過時；§3 表與 §5 的 Object_FPS 措辭（§5 第 31 條）；§6 離屏裝置 `ReadFrame` 是 Engine 能力、留；工具的 readback 用法消失 | — |
| releasing.zh-Hant.md／ja | 全檔 | 不過時：無 framework／kernel 字樣；object_fps 已更新；plugin 不是產品、不打包，§2 不必改 | — |
| network-architecture.zh-Hant.md | §5 手動建置與啟動 L661-754 | 不過時（PvP 不用 framework；match_main 遷移為 headless runner plugin 後執行檔名不變） | — |
| architecture.md | L254-272 Build and project management、L276-311 Asset preparation | 不過時（登錄、content.json 契約不變）；L301 `AssetManager` 段見 §5 第 11 條 | — |
| math.md | L5 inactive products 三份清單 | object_fps 刪除後成歷史 | 現在 |

## 8. 對抗複核（Opus／high，只讀；原文 `build/evidence/legacy-impact/adversary-report.md`）

抽樣規則：全部衝突類（4 條）＋規則層候選（1 條）＋5 條其他（卡片點名的候選與守衛判定）。不一致的逐條並列，不改原判定。

複核者：**Opus 5.5／high**（本卡主體 Fable 5.1；複核者在報告開頭誤記作者為 Opus 5.5，實際是不同模型），用時約 8 分鐘，基準 `c3ce8e9f`、另查 `53ead000`。結果：**一致 8／不一致 2**（#3、#10）；一致的 8 條中 7 條附補充。原判定不改，並列如下；「作者備註」是本卡對補充的回應，處置交架構師。

| # | 條目 | 報告判定 | 複核判定 | 一致 | 複核理由（摘） | 作者備註 |
|---|---|---|---|---|---|---|
| 1 | error-handling.md L124 無錯誤鏈 | 衝突 | 衝突，且同檔自相矛盾 | 一致 | 同檔 L19-30 第三列已寫「Go `%w` chain」；spec L275、L1403（B-9）寫「帶錯誤清單、不包鏈」vs §讀法 L21「包一層」——L124 的收窄句要同時對齊兩種說法 | 接受：衝突 1 的處置加「先統一 spec 內『鏈』與『清單』的措辭」；同檔矛盾比 legacy-vs-target 更急 |
| 2 | preview.md DI 4 readback | 衝突 | 衝突 | 一致 | DI 3（L77-79「the readback are Engine's」）、DI 5（L85-88 綁 SDL renderer，只因 UploadFrame 才需要）、RC 2 也以 readback 為詞；報告建議「限縮為 DI 1～3、5」仍會把 readback 帶進 plugin；preview.md 其實沒有資料契約，L3 的 carry-over 句像照抄 event-sounds | 接受：衝突 2 的處置改為「DI 3、4、5 與 RC 2、4 都要改寫；L3 的 carry-over 句刪或改」（master 的新 Legacy 檔頭與 L3 並存，矛盾更明顯） |
| 3 | field-session.md DI 3 | 衝突 | 相容（字面範圍不涵蓋），有誤導風險 | **不一致** | DI 3 的主詞是 collection code（「anything beyond the product's normal logging」）；Kernel 驗證是 Engine 正確性檢查、程式碼永遠在；真正的張力在非 DI 的「Test builds」節 L61-66 對 §2b 條件 3，但那只是允許不是要求，可並存；加一句澄清即可，不是 Delta | 並列不改。本卡的理由：遷移者讀 L104「A runtime flag inside the release binary is not separation」時分不出主詞範圍，§2b 的「同一二進位開著跑場次」正是場次情境；複核者的讀法成立時衝突降為誤導句，處置相同（加一句） |
| 4 | os.md RC 9 vs 頁保護 | 衝突（條件成立時） | 衝突（條件成立時），但較弱 | 一致（⑦ 不成立） | RC 9 末句「no observed pressure **and no Feature** needs is not started」給了出口：使用者採用第三層的決定命名為 Feature 即可放行；ci-readiness §4.1 只點名 os.md DI 未點名 RC 9 | 接受：衝突 4 的 ⑦ 改為「有更小方案：把第三層列為 Feature 並寫進 RC 9 的出口；是否動 RC 9 仍由使用者（D78）」 |
| 5 | AGENTS §9 vs plugin 清單在 main | 規則層衝突候選 | 同左 | 一致 | 雙處維護今天已存在（`tools/model_viewer/project.json` 列 IMGUI、SDL_RENDERER；`tools/model_viewer/CMakeLists.txt:33-64` 連 `GYO::ToolFramework*`）；target 的 main 清單是第三處，擴大既有張力 | 接受：§3 候選 5 補「今天已兩處；plugins 批不要變三處」 |
| 6 | threads.md DI 4 | 相容 | 相容 | 一致 | spec L574、L570（X-6）；找不到否定點 | — |
| 7 | determinism.md DI 4 括號句 | 相容，附誤導句 | 主規則相容；括號句單看是衝突 | 一致 | 括號句正是 spec G.4 第 1 條（L773）列的違反形狀；spec L756「與 DI 4『the tick, or a clock injected』一致」、L1178「DI 4 不動」等於 spec 自己背書這個片語——改寫要連 spec L756 一起 | 接受：§5 第 3 條的處置加 spec L756、L1178 |
| 8 | entity-store.md DI 6 與 stamp | 相容；stamp 放 kernel | 相容 | 一致 | spec A.6 L173 有兩條路：(a) SoA 另存 stamp 陣列；(b) `vector<Cell<T>>{key, added, changed, value}` 重用 store——stamp 在元素裡但元素屬呼叫者，`Cell` 與 stamp 的讀寫放 kernel 即仍成立；路 (b) 需 kernel 的 `EntityKey<Cell<T>>` 特化（DI 10 允許） | 接受：§2.1 的註只對路 (a) 精確；兩條路都要寫進 target/world §6 |
| 9 | neutral_names_guard.py | 現況不誤判；套到 kernel 會誤判 | 現況不誤判；遷移改到既有掛載路徑時就會誤判 | 一致（條件寫窄了） | 子字串、無 `\b`：`Despawn`、`ExtractRes`、`timezone`、`steam` 都命中（實測退出碼 1）；掛載點掃 voxel-mesh.md、map-markers.md 與兩個 engine 目錄——文件批寫進 `zone`／`Extract` 就失敗 | 接受：§4 第 3 列已照此補寫 |
| 10 | entity-model E-3 | 被取代 | 相容（措辭要更新） | **不一致** | E-3 是 PvP 的配發策略：target/world §1 L14-16 寫 PvP 選「單調不重用、generation 恆 0」且值逐位元相同；spec L89「E-1～E-3 就是 World 的鍵」；E-3 遷移後仍約束 PvP 的 `EntityAllotter` 與守衛 G4、G6，變的只是 Reset 的機制（X24）；報告對 E-1、E-2 判相容而 E-3 判被取代，前後不一致 | 並列不改。本卡的理由：E-3 的「Reset 歸回 First」「不需要 generation」兩句在目標下由 World 重建與格式含 generation 取代，所以歸「被取代」；複核者指出配發策略那半句仍約束新程式——若採「條文拆半」則 E-3 前半相容、後半被取代。同樣的分歧也在 entity-store DI 1（複核者「報告漏掉的」第 3 點）：報告定義「被取代＝target 已有新條文、舊條只剩守衛現有程式」，而 DI 1 對 store 仍成立——架構師定義「對舊程式仍成立、對新程式由 target 條文取代」要歸哪一類 |

**複核者另外指出、已併入本報告的**：基準過時（已 rebase 並加偏移表，§0）；cross-component-rules.md 已建（§1 第 38 列、§5 第 33 條）；§5 第 15 條已在 PR #191 處理；spec 引 entity-model 的行號在 master 已實際失效（§6）；§6 漏列 15 處已補（76→91）；target/README L83、L84 的歸類（目錄列提到「最終併入的」legacy 路徑）保留但註明。

**複核者的限制**：複核者寫「沒有換模型、盲點可能重疊」是誤記（作者 Fable 5.1、複核 Opus 5.5，有換模型）；複核的 `git grep` 輸出未保存。

## 9. 給架構師

1. **四條衝突的處置**（§3）：衝突 1（error-handling L124）與衝突 3（field-session DI 3）可以在第一個文件批以「收窄一句」解；衝突 2（preview DI 4）要改寫 preview.md L3 的 carry-over 範圍與 DI 4 本文，並同步 model_viewer RC 1；衝突 4（os RC 9 vs 頁保護）只在採用第三層時成立，但 ci-readiness §4.1 已點名——建議在 A.6 修訂批一起寫 Delta 七項，並向使用者提出 RC 9（D78）的例外或改寫。
2. **規則層候選 5**（AGENTS §9 vs plugin 清單在 main）：plugins 進 build graph 的計畫要先定 plugin 選擇是資料還是程式、與 `project.json` components 的關係；AGENTS §3 的「Product Registration 構造の変更」Delta。
3. **determinism.md 的狀態**：legacy 寫「proposal」，target 當「current contract」引用（§5 第 1 條）；且 target/determinism 刻意不重述 §1／§2。刪 legacy 前要先定 DI 1～6 的狀態、把三級與區表搬進 target（§6 第 1 列）。
4. **行號引用 10 處**（§6）：`architecture.md:136` ×4、entity-model 第 139 行 ×2、第 83 行 ×2、第 3 行 ×1、spec L1379 的多行一處——legacy 檔任何編輯都會讓它們漂移（entity-store RC 8 的 `:129` 已漂移）。建議 spec／target 改引條號或章節名；第 139 行的欄位分類表搬進 target/world。
5. **entity-store DI 6 與 World 的 stamp**：stamp 陣列要放 kernel 不放 `engine/entity_store`（§2.1）；建議 target/world §6 寫明「store 不持 stamp；World 以 SoA 持 keys／值／stamps（spec A.6）」。
6. **J-9 的「無產品名」掃描不可沿用 `neutral_names_guard.py` 的 WORDS**（§4 第 3 列）：自寫產品名禁字表。
7. **守衛的遷移影響檔**（§4 第 10～13 列）：framework guard、4 支工具守衛的正向清單、`GyoComponents.cmake` 的 `FRAMEWORK`、`ToolRegistryTests` 的保留名——plugins 計畫批的 Delta 項 4。
8. **注意事項**（不是衝突）：input.md 的 `<chrono>` 型別只能在 (b) World；io.md 的 `WriteFileAtomically` 只能從 `Io:file` 的 (b) System 呼叫；trace RC 1 與重放記錄的路徵；voxel_map_editor W6／W7 守衛的觀測手段在 plugin 版要重寫。
9. **卡片列的三個衝突候選中兩個不成立**（threads DI 4；determinism DI 1～3），一個部分成立（entity-store DI 1、DI 4 被取代）。
10. **「被取代」的定義要定一邊**（複核 #10 與 DI 1）：條文對舊程式仍成立、對新程式由 target 取代時，歸相容還是被取代？本報告歸被取代（E-3、entity-store DI 1、RC 1 等 36 條）；target/README 的新規則（PR #191：「一批同時碰到兩者時以 target 為準，事前宣告列出被取代的 legacy 條文、在 legacy 檔標『已被取代（批次代號）』」）剛好需要這份清單——§2 的被取代 36 條可直接當標記來源，但 E-3 這類「半條」要先定拆不拆。
11. **複核者補的三個處置修正**（§8 #1、#2、#4）：spec 內「鏈」與「清單」措辭先統一；preview.md DI 3、4、5 與 RC 2、4 一起改寫、L3 carry-over 句刪；RC 9 的出口是把第三層列為 Feature。

## 10. 給圖書管理員

1. §5 的 32 條誤導句：加註或改措辭（條目已附建議）；優先 architecture.md Layers 表（第 8 條）、entity-model 起點 1／2（第 14、15 條）、determinism §0／§8（第 2、4 條）。
2. §6 的反向依賴：legacy 檔搬到 `legacy/` 或刪除前，先把 target/README、spec、mechanism 的連結改到新位置；行號引用改條號。
3. §7 的操作類：tool_projects.md L15 四平台、math.md L5 inactive products 現在就能改；其餘等 plugins／kernel 進 build graph。
4. `docs/cross-component-rules.md` 尚未建（卡片點名的對象之一）；framework 三份的 deprecated 狀態行已齊，但 audio_editor／model_viewer／voxel_map_editor／ui_toolchain 四份工具文件引用 framework 處無任何註（§5 第 22～30 條）。
5. ja 譯本（releasing、rendering_architecture）章節數與 zh 一致；zh 改動時同步。

## 範囲の限界

- 判定是讀文件對照 target／spec 的推論，沒有實作任何 kernel 程式驗證「照它做會不會違反」；衝突 4 的「27 筆探測」引自 ci-readiness，本批未重跑 OS boundary guard。
- 守衛「會不會誤判」只看掃描根與字表，未在 kernel 假目錄上實跑守衛（neutral_names 的誤判是從 WORDS 與 spec 詞彙推出）。
- network-architecture.zh-Hant.md 1,500 行：§1、§2 前段、§5、§7 全文讀過；§3（兩份 protocol）、§4（Room／Session）、§8～§9（成本回顧、v4～v7 升級入口）只掃標題——那些是 wire 與歷史，不含 DI。
- AGENTS.md 只掃引用舊結構（framework、Application→Engine 圖、§9 登錄）的條文，不是全文審查。
- parent 會話找不到（`list_sessions linked` 為空），數字門檻未經 parent 核定；已在開始前給使用者看過。

## 證據與出處

- 主 checkout `build/evidence/legacy-impact/`：`guard-scan.txt`、`guard-scan.stderr`、`guard-scan-narrow.txt`、`reverse-scan.txt`、`reverse-scan.stderr`、`guard-scope.txt`、`adversary-report.md`（對抗複核原文）。
- 行號以 origin/master `c3ce8e9f`（2026-11-12）為準；kernel HANDOFF 的 `v8-arch-docs-legacy` 決定讀自 `origin/claude/arch-kernel-docs-sync`（PR #191）。

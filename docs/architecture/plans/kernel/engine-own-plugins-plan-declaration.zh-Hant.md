# 事前宣告：`v8-arch-engine-own-plugins-plan`（規劃批 B1：Engine 模組改為 Engine 自有 plugin、由 App 組裝）

更新：2026-11-13。Owner：架構（架構師會話開出的任務卡 04）。**狀態：已核准（使用者 2026-11-14「核准，選 A」）。**

出處（已定，本批不重開）：kernel `HANDOFF.md` 決定行 `v8-arch-adapter-plugins-review`（方向往 Bevy 靠、B1 另開、owner 與組成形式分開、R3）、`v8-arch-plugin-selection-data`、`v8-arch-engine-or-plugin-criteria`、`v8-arch-external-components-as-adapters`、`v8-arch-target-di-approved`；HANDOFF 現況「使用者對 #212 的方向性疑慮」；遷移計畫 `migration-plan.zh-Hant.md` §3.11（plugins-foundation）、§3.31b（本批的占位）、§3.32（closeout）、§4、§8 RC-8／RC-11；規劃稿 `adapter-plugins-plan.zh-Hant.md` 最終檢查處置記錄 S-1、S-3、S-5、S-8 與文末〈仍往產品加步驟的條目〉；`target/runtime-composition.zh-Hant.md` §1、§4、Design Invariants；kernel `spec.zh-Hant.md` §C.2～C.6。

## 1. 目的（Feature → Contract → Architecture）

### Feature（使用者看得到的部分）

使用者的原意（`v8-arch-adapter-plugins-review` 與 HANDOFF 的方向性疑慮）：「讓產品能自由使用 Engine 的能力」；「Bevy 的擴展能力，是 ECS 為核心的 System 管理、擴展的能力」；規則只放在 Engine 與 adapter 的邊界，不往產品的開發流程加步驟。

| # | Feature | 用到的 Product |
|---|---|---|
| F1 | 產品挑它要的 Engine 能力來組裝 App（例：headless 的 Match 不組裝視窗與渲染；Client 的三個 Runtime 各組各的）；不挑的能力不進它的 Registry | object_fps_pvp 的 Match 與 Client、四個工具 |
| F2 | 產品的 System 直接登記到 App，不必先包成 plugin（Bevy 的 `App::add_systems`；R3「包成 plugin 是產品內部的選擇」） | 同上 |
| F3 | Engine 自有 plugin 與 `plugins/` 的擴展用同一種組成形式；產品組裝時不必分辨 owner，換 owner（例：某能力日後改由外部維護）不改產品的寫法 | 同上 |

約束（不是 Feature，是判定的一部分）：本批的提案不往產品與工具的日常開發流程加步驟；若不得不加，照實列出並交使用者（§5 判定 6）。

### Contract（本批要提出、交使用者定的約定）

- **App 的組裝入口**：plugin 清單（資料，`project.json` 的同一個選擇欄）之外，產品能否、怎麼直接登記 System（對應 F2；今天 spec C.2 只有「Host 列出 plugin → 各 plugin 的 `Build`」一條路，見 §3 題 S-3 (2)）。
- **plugin 登記資料的 owner 欄**：「Engine 自有」與「`plugins/` 擴展」的區分（plugins-foundation 段 (i) 只預留形狀、不實作；本批定預留的形狀要裝什麼）。
- **plugin 的需求宣告**：plugin 能否宣告「需要某後端」（不只 Engine 的能力）、target 層級的可選依賴怎麼表達（§3 題 S-1）。
- **露出第三方型別的界線**：依賴者拿到 adapter 露出的第三方型別能做什麼（§3 題 S-5）。
- **Engine 自有 plugin 的預設組**：是否提供像 `DefaultPlugins`／`MinimalPlugins` 的預設組合，它是資料還是程式（與 `v8-arch-plugin-selection-data`「plugin 的選擇是資料」對齊）。

### Architecture

Engine 的能力模組各自提供一個 Engine 自有 plugin，登記自己的 Resource、System、loader；產品與工具由 App 組裝它們。owner 不變（仍是 Engine、隨 Engine 改版），能力的歸屬不變（判斷順序已定的結果不重開）；變的是**組成形式**：今天由產品或 Host 手寫的初始化，改為由 plugin 的 build／finish 承擔。Kernel 本體不加能力名（DI-KS 15、spec R-7）。這是新的 Architecture Delta（§4 已有 Delta-1～7 都不含「Engine 模組改為 plugin」）；本批只**規劃**，七項寫在規劃稿，執行由本批提出的執行批做。

## 2. 予見的根據（AGENTS §2 基盤規則）

判為**基盤 Architecture**：App 的組裝入口、plugin 登記資料的形狀、Engine 能力的初始化方式，改了之後，上面所有產品、工具、`plugins/` 的擴展、登記契約的測試都要跟著改，量隨積上去的東西增加。

| 根據 | 出處 | 導出的基盤要求 |
|---|---|---|
| 1 名指的 Feature | F1～F3（上表）；`v8-arch-adapter-plugins-review` 的 B1 條；plugins-foundation 段 (i) 要在宣告前得到 S-1、S-3、S-5 的答案（規劃稿處置記錄） | 組裝入口（F1、F2）；owner 欄（F3）；需求宣告與可選依賴（S-1）；露出界線（S-5） |
| 2 參考實作（只答「怎麼做」） | Bevy 0.20.0-dev，本機 `~/Code/GitHome/bevy` `6a911653f`（2026-11-10）：`App::add_systems`（`crates/bevy_app/src/app.rs:321`，main 不經 plugin 也能登記 System）；`Plugin` 的 `build`／`ready`／`finish`／`cleanup`／`is_unique`（`plugin.rs:59-89`）；`DefaultPlugins`／`MinimalPlugins`（`crates/bevy_internal/src/default_plugins.rs:3`、`:161`：每個 Engine crate 一個 plugin、以 feature 開關、組內有序）；`PluginGroupBuilder::set`／`disable`／`add_before`／`add_after`（`plugin_group.rs`）；`set_runner`（`app.rs:224`） | 相似點與要改的部分在規劃稿寫；已定的不同點不重開：spec C.6（不做 ready／cleanup、中央 Registry 凍結、`requires` 檢查不拓撲排序）、外部組件的 adapter 原則（刻意與 Bevy 不同） |
| 3 規模 | `engine/` 下 23 個頂層模組（`ls engine` 今天 23 個目錄＋`CMakeLists.txt`；開工時重數）；六個 `project.json`（plugins-foundation 判定 (6) 的清單）；Client 三個 Runtime、Match 一個、Editor 文件 World＋渲染 World（target §2、§3、§3b） | 每個模組的去處逐一判（§5 判定 1）；組裝入口要能表達「同一行程多個 Runtime 各自的清單」 |

1 與 3 都有；2 只用在「怎麼做」。導不出的要求寫成未決交使用者，不寫成假設。

## 3. 本批要回答的題（產出裡附選項與建議，不自己定）

| 題 | 內容 | 出處 |
|---|---|---|
| S-3 (2) | 照目前機制產品的 System 要經 plugin 的 build 函式登記，與 R3「包成 plugin 是產品內部的選擇」不一致；連帶 runtime-composition DI 2 的「守衛掃 `main` 與產品程式不直接引用 plugin 的 build 函式」、DI 2 增補第三點（S-3 (1)）怎麼讀 | 規劃稿〈仍往產品加步驟的條目〉第 3、4 條 |
| S-1 | `sdl_imgui` 的依賴方向與登記：plugin 能否宣告「需要後端」、或 DI 6 只管 Engine 的能力；第 1 類 plugin 的一個 target 依賴第 2 類 plugin 怎麼表達；刪 imgui plugin 時的預期；(iii) 對 `sdl_imgui` 的讀法 | 規劃稿 §3.3 末、處置記錄 S-1 |
| S-5 | 依賴者拿到 adapter 露出的第三方型別能做什麼（只持有與傳遞，或也能呼叫）；測試夾具算不算在「整個 repo 的 SDL 呼叫只在 sdl plugin」之內 | 規劃稿 §2〈`*Native` target 的去處〉、處置記錄 S-5 |
| S-8 | RC-8 對 B1：(a) B1 是計畫內的規劃批、它的 Delta 不算新發現；(b) 照算、B1 完成時觸發、回到規劃就是 B1 的修訂 | 計畫 §8 RC-8 |
| 必答 1 | B1 產出的執行批與功能線重開的先後；排在 closeout 之後要說明為何不違反 AGENTS §2「基盤の上に機能を積み上げ始める前に、基盤の反復を済ませる」 | 計畫 §3.31b |
| 必答 2 | 執行批的估時加入 RC-11 的重判 | 同上 |
| 必答 3 | Engine 自有 plugin 不借用過渡登記表；closeout 的 0 列不因本批放寬 | 同上、§3.32 |

## 4. 本批產出（只動 `docs/`）

| # | 產出 | 位置 |
|---|---|---|
| 1 | **規劃稿**：§1 `engine/` 23 個模組逐一的去處（Engine 自有 plugin／Kernel 之下的基礎、不成 plugin／已定進 `plugins/`），每列寫今天誰初始化它（檔＋符號）、成為 plugin 後 build／finish 登記什麼、依賴哪些 plugin；§2 Contract 提案（組裝入口、owner 欄、預設組、需求宣告）；§3 上表七題的選項、建議與影響；§4 執行批的切分、依賴、估時與排程；§5 新 Delta 的七項（AGENTS §3）；§6 對 target DI 的提案（不改原文）；§7 往產品加步驟的條目的增減（對照規劃稿的 7 條） | 新檔 `plans/kernel/engine-own-plugins-plan.zh-Hant.md` |
| 2 | **遷移計畫修訂**：§3.31b 補全；新增的執行批；§3.11 段 (i)「預留形狀」的具體內容；受影響的批（plugins-foundation、wrap-*、plugins-frame、plugins-editor、client-frame、工具四批、closeout）的差分；§2 樹與表、§2.2 合計；§4 新 Delta；§8 RC-8、RC-11 的重判 | `migration-plan.zh-Hant.md`（修訂記錄加一節） |
| 3 | **要交使用者的未決**：§3 七題＋盤點中新出現的題，各附選項、建議、代價 | 規劃稿〈未決〉 |
| 4 | HANDOFF 一行、kernel README 的清單、dev_log、PR | AGENTS §15 |

**不改**：`target/runtime-composition.zh-Hant.md` 的 `Design Invariants` 節（卡 03 正在落 DI）；需要改的列成提案（規劃稿 §6）。target 本文本批也不改，提案隨 DI 的提案一起交。kernel `spec.zh-Hant.md` 不改；若 F2 的答案要動 spec C.2（「Host 列出 plugin」是唯一登記路徑），列為未決與 Delta 的一部分。

## 5. 判定（看結果前宣告）

1. **件數**：規劃稿 §1 的列數＝開工時 `engine/` 的頂層模組目錄數（今天 23）；以 `git ls-tree -d --name-only HEAD engine/` 計，先以已知的 `engine/kernel` 命中，寫進件数の突き合わせ。
2. **不改已定的歸屬與前提**：判斷順序已定的結果（collision、math、model 取樣、audio core、ui、JSON 留 Engine；網路含 `engine/net` 是 `plugins/` 的擴展）0 列改變；spec K.3 的 17 項、計畫 §7 的 U1～U14、今天的決定行不動。B1 只改組成形式；任何一列需要改歸屬或 owner＝停止條件。
3. **每項都有支撐的 Feature**：每個執行批、每個 Contract 項目寫明支撐 F1～F3 的哪一個（含它用到的 Product）；寫不出的延後、不列入。
4. **七題都有答案的形**：§3 每題有 ≥2 個選項、建議、每個選項的代價與影響的批；都標「交使用者」。
5. **計畫內部一致**：§2 樹、§2.2 表、§3 各批、§4 Delta、§8 對同一批的範圍、依賴、估時說法一致；批數與合計重算一致；新估時有出處或標「推定」。
6. **不往產品加步驟**：規劃稿 §7 列出本批提案前後〈仍往產品加步驟的條目〉的列數；增加的每一列寫明是哪個 Feature 本身需要，否則交使用者。
7. **敵對檢查**：Fable／high 一次、0 blocker；should 全部處置或寫不採的理由。
8. **範圍**：只有 `docs/` 有 diff；`target/runtime-composition.zh-Hant.md` 的 diff 為 0（`git diff --stat origin/master -- docs/architecture/target/runtime-composition.zh-Hant.md` 空）。

## 6. 做法與編制（使用者核准時選一）

| 選項 | 編制（模型／檔位） | 估時 | 取捨 |
|---|---|---|---|
| **A（建議）ultracode 小編制** | 偵察 Opus／high ×2（(a) 23 模組今天的初始化點與產品端呼叫；(b) Bevy 對照與計畫受影響各批的摘錄）；提案 Opus／high ×3（①風險與可逆：搬動順序、Kernel 不變、守衛；②產品的自由度：F1～F3 與往產品加步驟的條目；③成本：執行批的切分、排程與 RC-11）；統合 Opus／high；**敵對檢查 Fable／high 一次**、只讀；修訂與定稿主會話 Opus／high | 8～16 h | 基盤、跨多批、四題相互牽動；三個角度能把「規則往產品流程加步驟」單獨看一遍。計畫 §3.31b 原寫統合 Fable／敵對 Opus xhigh，這裡照使用者目前的臨時做法：Fable 只用一次、做敵對檢查 |
| B 主會話單稿 | 主會話 Opus／high 偵察與寫稿；敵對檢查 Fable／high 一次 | 6～12 h | 便宜；只有一個角度，四題的取捨可能偏向主會話的既有讀法 |

所有子代理明寫模型與檔位，不繼承。偵察與提案的原始輸出存主 checkout `build/evidence/engine-own-plugins-plan/`（git 不追蹤）。

## 7. 範囲の限界

- 只規劃，不驗證可實作；組裝入口與登記形狀由執行批與 plugins-foundation 的事前宣告定案。
- Bevy 是 Rust（trait、crate、feature 開關）；它的組成方式對 C++／CMake 的可移植性只以對照說明，不算證據。
- plugins-foundation 段 (i) 尚未開工；它的宣告可能改變本批的前提（例：能力使用的推導可行與否）。本批的提案寫明依賴哪個前提。
- 卡 03 同時在落 runtime-composition 的 DI；本批的 DI 提案以落地後的條文為準，有衝突交架構師。
- 執行批的成本在使用者核准前不加進計畫 §2.2 合計（RC-11 重判照必答 2 的答案）。

## 8. 時點、估時、停止條件

- **時點**：plugins-foundation 段 (i) 的事前宣告之前（S-1、S-3、S-5 是它的輸入）；Kernel 線內、功能線重開之前。
- **估時**：選項 A 8～16 h、選項 B 6～12 h（推定；比照 `v8-arch-adapter-plugins-plan` 的小編制）。
- **停止**：AGENTS §14 冒頭三條；任一模組需要改歸屬或 owner（判定 2）；某題在 Kernel 中性（DI-KS 15、spec R-7）與 F2 之間沒有可行選項（列給使用者、不自己定）；總時數超過上限的 1.5 倍（A 24 h、B 18 h）或單一問題累計超過 2 h。

## 9. 若失敗

plugins-foundation 段 (i) 的登記格式只預留 owner 區分的形狀、S-1／S-3／S-5 由它的宣告自己交使用者；Engine 模組維持今天的初始化方式，B1 的執行批不排。G2a 之前的 Kernel 批不受影響。

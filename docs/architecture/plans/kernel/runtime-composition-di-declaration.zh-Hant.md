# 事前宣告：`v8-arch-runtime-composition-di`（runtime-composition 的 DI 增改落字）

更新：2026-11-13。Owner：架構（架構師會話開出的任務卡 03）。**狀態：待使用者核准；另有一個張力（§4）要使用者先定。**
模型／檔位：Opus／high（上段：Design Invariants；條文已由使用者核准，本批是落字與對照）。

## 目的（Feature → Contract → Architecture）

- **Feature**：plugins-foundation 段 (i) 與 adapters-relocate 的事前宣告能引用穩定條號——N2（選擇欄的契約測試）與 N1（Engine 側含入與連結掃描）以這些條為依據（規劃稿 §4 未決 11「落字時點：plugins-foundation 的事前宣告之前」）。
- **Contract**：不改任何契約；把使用者已核准的條文寫進 target 頁的 `Design Invariants` 節。
- **Architecture**：Architecture Delta（AGENTS §3「Component 的 Design Invariants」：節的變更要使用者同意）。同意已在下列出處取得；本宣告只確認落字的位置與字面。

## 核准出處

| 條 | 出處 |
|---|---|
| DI 2 增補、DI 6、兩條 RC | kernel `HANDOFF.md` 現況「2026-11-13 #212 的使用者決定（續）」的 **未決 11**：「DI 2 增補、DI 6、兩條 RC 照草稿」 |
| 新 DI 5 | 同上（依未決 10 改窄）＋決定行 `v8-arch-adapter-plugins-review` (2)：「Engine 不連外部組件」改為「Engine 只在判斷順序判定屬 Engine 的能力裡、經它唯一的 adapter 連外部組件」 |
| 條文字面 | `adapter-plugins-plan.zh-Hant.md` §4〈未決 11〉（第三次修訂後；#212 合併時的版本） |

## 範圍

只動 `docs/`：

| 檔 | 改什麼 |
|---|---|
| `target/runtime-composition.zh-Hant.md` | `Design Invariants` 節：DI 2 加增補；新 DI 5、DI 6；`Replacement Criteria` 加兩條；節首「正式」行加本批代號。**節外（§1～§4 本文）不動** |
| `target/README.md` | 〈目標 Design Invariants 清單〉的條數（34→36、部件特有 13→15）、第二層表加 DI 5、DI 6 兩列、DI 2 那列的一句補「選擇以可執行單位為粒度」 |
| `README.md`（入口） | 〈Layers and dependencies〉：目前只連到 runtime-composition §4；若改後要連 DI 5 才改連結，不加規則（檔案歸圖書管理員，只動那一條）。**預計不改** |
| `plans/kernel/HANDOFF.md` | 一行 |
| `docs/dev_logs/2026_11_13_arch_runtime_composition_di.zh-Hant.md` | 新建 |
| `plans/kernel/README.md`（若有批次清單） | 加本批與 dev_log |

不碰：程式、CMake、守衛、spec、規劃稿（規劃稿 §4 未決 11 的「目前只有 DI 1～4」一句由本批 dev_log 記錄已落，不改規劃稿本文）。

## 1. 要落的條文（原文照規劃稿 §4 未決 11）

### DI 2 增補（加在「產品與工具的 plugin 選擇只寫在 `project.json`（資料），登記由建置產生，`main` 不手寫 plugin 清單」之後）

> 選擇以可執行單位（role、variant）為粒度；第 1 類後端也經同一個欄選擇；產品與工具的 CMake 不寫 plugin 或後端的 target 名。

守衛行：既有守衛不動；增補的部分寫「N2（選擇欄的契約測試，由 `v8-arch-plugins-foundation` 段 (i) 新增）；第三點不設白名單（review：N4 刪）」。**第三點的字面依 §4 使用者的選擇而定。**

### 新 DI 5

> 外部組件的型別與呼叫只在它的 adapter，每個組件一個 adapter：歸 plugin 的能力，adapter 在那個 plugin；**Engine 只在判斷順序判定屬 Engine 的能力裡、經它唯一的 adapter 連外部組件**。Engine 的標頭，以及 adapter 實作 Engine 介面的部分，只用 Engine 的詞；adapter 可以對依賴它的產品與 plugin 露出第三方型別。

守衛行：Engine 側的含入與連結掃描（N1，只掃 `engine/`；由 `v8-arch-adapters-relocate` 新增）＋plugin 版標頭衛生守衛（由 `v8-arch-adapters-relocate` 新增）＋依賴方向掃描（既有，DI 2）。附一句事實：Engine 目前唯一的外部組件 adapter 是 JSON 模組（yyjson 與 fast_float，兩個組件都只在這個 adapter；未決 18）——**標「現況，非條文」**，JSON 模組在 json-adapter 批才實作。

### 新 DI 6

> 每個可執行單位用到的、需要後端的能力恰好有一個已選後端；缺或多是 configure 錯誤並指名能力。

守衛行：N2（§3.4 的三種 configure 錯誤各一個合成反例；由 `v8-arch-plugins-foundation` 段 (i) 新增）。

### Replacement Criteria 兩條

> (i) 升級一個第 1 類 adapter 時，它輸出的內容身分 (c) 改變（例：ufbx 的載入結果變了，`AnimationContentDigest` 跟著變）→ 不是「只改 adapter」，走內容身分的版本流程。
> (ii) 同一組件的對接出現在第二個 plugin 時，不放寬 DI 5，而是把對接搬回它的 adapter；介面不足就回 Engine 補介面。**已用一次**：ImGui 的 SDL 後端放進 SDL plugin 內的 `sdl_imgui`，不在 imgui plugin（`v8-arch-adapter-plugins-review`）。

條號：既有 RC 是無號的三點（LIM-5、渲染寫回、Host 長回來），新兩條接在後面、照規劃稿標 (i)(ii)。

## 2. 與既有 DI 1～4 的對照

| 既有 | 與新條的關係 |
|---|---|
| DI 1（受 DI-KS 13 約束） | 無關 |
| DI 2 | 增補是加細，不改既有三句與守衛；「Engine 不 include 任何 plugin」與 DI 5 的「Engine 只經自己的 adapter 連外部組件」是兩件事（前者是對 plugin 的依賴方向，後者是對第三方），不重疊 |
| DI 3 | 無關 |
| DI 4（受 DI-KS 14 約束） | 無關 |
| kernel DI-KS 15 | DI 6 的「能力」名不進 Kernel（§4 本文已寫「能力名不進 Kernel（kernel DI-KS 15）」）；DI 6 是 build／configure 層的條，不碰 Kernel |

## 3. 與 §4 本文的對照（逐點，本文不改）

| 新條 | §4 本文對應句 | 一致？ |
|---|---|---|
| DI 2 增補 1、2 | 「同一個選擇欄」：兩類 plugin 用同一個選擇欄 | 一致。「可執行單位」一詞 §4 本文沒有，但規劃稿 §0 第 2 點已定（app 的 role、tool 的 variant） |
| DI 2 增補 3 | §4 本文**沒有**這句 | 只在 DI 增補裡出現；見 §4 張力 |
| DI 5 前半 | 「外部組件是可升級、可替換的後端」三原則 (1)(2) | 一致 |
| DI 5「Engine 只在…」 | 「能力放哪與怎麼碰外部組件是兩件事」 | 一致（同一決定行 (2)） |
| DI 5 末句（Engine 的詞、可露出第三方型別） | 原則 (3)、Q2 句（#212 未決 10 (iii)） | 一致 |
| DI 6 | 「同一個選擇欄」：「產品用到某能力卻沒選它的後端、或選了兩個，是明確錯誤，在 configure 階段報出是哪個能力缺後端」 | 一致；DI 6 用「每個可執行單位」，本文用「產品」——粒度依 DI 2 增補 1，條文較準 |
| RC (i) | 「範囲の限界：升級靠型別隔離今天就成立」 | 補充，不衝突：型別隔離不保證輸出的內容身分不變 |
| RC (ii) | 「SDL 家族」：整個 repo 的 SDL 呼叫只在 `sdl` plugin | 一致 |

## 4. 要使用者先定的張力：DI 2 增補第三點

**事實**（規劃稿最終檢查處置 S-3 (1)、§3.5 末、文末〈仍往產品加步驟的條目〉第 3 列）：

- 第三點「產品與工具的 CMake 不寫 plugin 或後端的 target 名」**沒有守衛**（review 刪了 N4 白名單），closeout 也不掃。
- 與 **R3** 衝突：產品獨有的能力屬於產品，包成產品目錄下的 plugin 是產品內部的選擇——那個 plugin 的 CMake 必然寫它自己的 target 名。
- 與 **未決 8 A** 衝突：產品的 protobuf generated code 經網路 plugin 用它的 protoc 規則與執行期函式庫——產品 CMake 要寫網路 plugin 提供的 target（今天是 `apps/object_fps_pvp/CMakeLists.txt:5-24` 直接寫 `protobuf::libprotobuf`、`protobuf::protoc`）。
- 今天的現況（只是過渡事實）：產品 CMake 用 `PRIVATE_COMPONENTS SDL_IMAGE UFBX …`（同檔 59-62 行）；四個工具直接連 `imgui::core`、`GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDL` 等（例 `tools/voxel_map_editor/CMakeLists.txt:68`）。這些在各遷移批改成「選擇欄＋Run」時消失，不論選哪個選項。
- 第三點原本想守的性質：**「這個可執行單位用哪些 plugin」只有一個手寫來源（選擇欄）**（AGENTS §9「同一事實手動維護的地方只有一處」）。

**選項**（不替使用者選；每個都改使用者已核准的字面或保留它）：

| 選項 | 第三點寫成 | 機制 | 取捨 | 守衛（本批不建，只寫由誰建） |
|---|---|---|---|---|
| **A 保留原文** | 照原文 | 無機械檢查；靠審查 | 條文與 R3、未決 8 A 的字面衝突留著；沒有守衛的 DI 會讓後來的 agent 以為有人在守，或以為可以照字面拒絕產品自己的 plugin | 無 |
| **B 刪第三點** | （刪） | 選擇的唯一來源只靠第一點「選擇只寫在 `project.json`」＋N2 | 與 R3、未決 8 A 不再衝突；失去「CMake 不得偷偷多連一個 plugin」的字面約束（第一點的「選擇只寫在 project.json」仍在，但 CMake 直接連 target 是否算「選擇」沒寫明） | 無 |
| **C 改成「CMake 不引入選擇欄以外的 plugin 或後端」** | 產品與工具的 CMake 可以引用自己的 target 與**已在該單位選擇欄裡**的 plugin 提供的 target，不得引入選擇欄以外的 plugin 或後端 | configure 時比對：每個可執行單位連到的 plugin target 都在它的選擇欄裡；否則 configure 錯誤並指名 | 守住原本的性質（選擇只有一個來源），又容得下 R3 的產品 plugin（它也寫在選擇欄）與未決 8 A（網路 plugin 已被選）；代價：configure 要能從 target 反查它屬於哪個 plugin（plugin 登記資料要列出它提供的 target），可能讓 §3.4 的 configure 錯誤從 3 種變 4 種 | plugins-foundation 段 (i)（與 N2 同處；是否可行由段 (i) 的先驗判，和 S03／S08 的 CMake 推導同一類問題） |
| **D 改成「只禁產品 CMake 直接連第三方的 target」** | 產品與工具的 CMake 不直接連外部組件的 target（例 `imgui::core`、`protobuf::*`、`SDL3::*`），經對接它的 plugin | 掃描 `apps/`、`tools/` 的 CMake 不出現第三方 target 名（名單來自 plugin 登記資料或 third_party 清單） | 等於把 R3 的硬規則（產品連外部組件時經對接它的 plugin）寫成 CMake 可檢查的形；不再管「選擇是否只有一個來源」——那個性質回到第一點；未決 8 A 要網路 plugin 提供 protoc 規則的包裝（不讓產品寫 `protobuf::protoc`） | adapters-relocate 或各工具遷移批（今天四工具直接連 `imgui::core`，守衛要等它們遷完才能是零命中；過渡期要棘輪，而 review 刪過產品側棘輪） |

證據位置：規劃稿 `adapter-plugins-plan.zh-Hant.md` §3.5 末、§4 未決 8／10／11、文末〈仍往產品加步驟的條目〉、處置記錄 S-3；決定行 `v8-arch-adapter-plugins-review`（R3＝(6)）；CMake 現況見上列行號（本宣告寫作時讀 origin/master `e1518759`）。

**另一個相關、但不在本批範圍的張力**（S-3 (2)）：產品的 System 要經 plugin 的 build 函式登記，照目前機制等於產品程式要包成 plugin，與 R3「包成 plugin 是產品內部的選擇」不一致。它不在 runtime-composition 的 DI 節裡，本批不處理；若 §4 選 C，條文用語「產品自己的 plugin」會碰到它，屆時照實寫、不擴大。

## 5. 判定（完成條件）

1. `Design Invariants` 節的 diff 只含：DI 2 那一條加增補（與守衛行）、新 DI 5、新 DI 6、RC 加 (i)(ii)、節首正式行加本批代號；DI 1、DI 3、DI 4 與既有三條 RC 位元相同。以 `git diff` 的 hunk 逐一對照本宣告 §1。
2. §1～§4 本文 md5 不變（只動 DI 節之後；用 Python 切出 `## Design Invariants` 之前的部分算 md5，改前改後相同）。
3. `target/README.md` 條數：全文 36＝kernel 15＋部件特有 15＋資料契約 3＋產品 3；第二層表多 2 列。以 Python 數第二層表的部件特有條數核對。
4. 入口 `README.md` 預計不改；若改，diff 只在〈Layers and dependencies〉的連結。
5. 只動 `docs/`（`git diff --name-only origin/master` 全在 `docs/` 下）。

## 範圍の限界

- 本批不新增守衛；新條在守衛建好之前只靠審查。各條寫明由哪批建守衛。
- DI 5 附的「JSON 模組是唯一 adapter」是現況描述，json-adapter 批實作前不成立；標成現況，不當條文。
- N2 能不能守 DI 2 增補第一、二點（粒度與第 1 類後端同欄），取決於段 (i) 的反例怎麼設計；本批照規劃稿寫「N2」，不替段 (i) 擔保。

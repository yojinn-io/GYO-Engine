# 事前宣告：`v8-arch-adapter-plugins-plan`（外部組件改成 adapter plugin 的規劃批）

更新：2026-11-13。Owner：架構（架構師會話）。**狀態：已核准（使用者 2026-11-13「核准，開始」）。**

出處（全部已定，本批不重開）：kernel `HANDOFF.md` 決定行 `v8-arch-external-components-as-adapters`（立場、三條原則、方向 C、plugin 兩類、細節六題）、`v8-arch-engine-or-plugin-criteria`（留 Engine 還是 plugin 的判斷順序）、`v8-arch-plugin-selection-data`、`v8-arch-framework-retire`、`v8-arch-docs-scope-of-target`、`v8-arch-architecture-entry`；遷移計畫 `migration-plan.zh-Hant.md` §3.11（plugins-foundation）、§3.12（net-link）、§3.18（wrap-render）、§3.20（plugins-frame）、§3.24（plugins-editor）、§3.25～3.28（工具四批）、§4 Delta-2／5／6、§5.2；`target/runtime-composition.zh-Hant.md` §4；`guards-inventory.zh-Hant.md` Q1、Q2；`tool-hosts-inventory` 報告 §8。

## 1. 目的（Feature → Contract → Architecture）

- **Feature**（使用者原話：「我做這個隔離的基礎是外部組件也會有升級。我一個是可以替換，其次我可以更換新版本的外部組件。」）：
  1. 升級一個外部組件（SDL、ufbx、asio…）只改對接它的那一塊；用到它的產品（object_fps_pvp 的 Match 與 Client、四個工具）不跟著改。
  2. 產品在一個登記欄裡選它要的能力與後端；沒選某個能力就不連它的後端（例：headless 的 Match 不連視窗與 GPU）。
  3. 缺後端或選了兩個，在 configure 就報出是哪個能力。
- **Contract**：plugin 分兩類（Engine 必備的後端／第三方能力提昇）共用一份層規則；`project.json` 用同一個選擇欄；Engine 的能力宣告「我需要一個後端」；SDL 家族各自一個 plugin、宣告依賴 SDL 那個 plugin。
- **Architecture**：外部組件的 adapter 從 Engine 搬到 plugins 層，Engine 只留中性介面。這是新的 Architecture Delta（計畫裡的 Delta-6 只把這些模組改成 Resource／Host 零件，沒有把後端搬出 Engine）。本批只**規劃**，不改程式。

## 2. 本批產出（只動 `docs/`）

| # | 產出 | 位置 |
|---|---|---|
| 1 | **模組歸類表**：`engine/` 下每個模組、`third_party/` 下每個外部組件，逐一套 `v8-arch-engine-or-plugin-criteria` 的順序，寫出三個問題的答案、歸屬（留 Engine／第 1 類 plugin／第 2 類 plugin／待使用者）與證據（檔＋符號） | 新檔 `plans/kernel/adapter-plugins-plan.zh-Hant.md` §1 |
| 2 | **中性介面盤點**：每個後端（platform、input、render 的 SDL renderer 與 SDL_GPU、text、audio、asset 的 SDL_image 與 ufbx、net）今天的公開標頭是否已不含外部組件的型別；不乾淨的列出要改的符號與估計 | 同上 §2 |
| 3 | **選擇欄的形狀提案**：今天的 components 與 plugins 併成一個欄的名字、版本號、能力與後端的宣告方式、configure 錯誤的訊息形狀；給 plugins-foundation 當事前宣告的輸入（欄名由 plugins-foundation 定，本批只提方案與理由） | 同上 §3 |
| 4 | **遷移計畫修訂**：§1.1 前提加今天的決定；§2 樹與表；受影響各批（plugins-foundation、net-link、wrap-render、plugins-frame、plugins-editor、client-link、client-frame、工具四批、framework-delete）的範圍、判定、估時；必要時新增或拆分批；§4 新增 Delta（七項）；§5.2 受影響守衛的去處；§8 Replacement Criteria 是否觸發 | `migration-plan.zh-Hant.md`（修訂記錄加一節） |
| 5 | **target 條文**：`target/runtime-composition.zh-Hant.md` §4 寫入判斷順序、三條原則、兩類 plugin、同一選擇欄、SDL 家族的依賴宣告；這一節的 Design Invariants 若要增改，列為 Architecture Delta 交使用者（目標 DI 已核准，變更要同意） | `target/runtime-composition.zh-Hant.md` |
| 6 | **入口 README**：`Layers and dependencies` 條目裡「待寫入 §4」那一句刪掉（§4 已寫入） | `docs/architecture/README.md`（檔案歸圖書管理員，連結目標歸架構師） |
| 7 | HANDOFF 決定行與現況、kernel README、dev_log | 照 AGENTS §15 |

`docs/architecture/platform-sdl.md`、`audio.md`、`input.md` 等頂層文件：本批**不改內容、不加 Legacy 行**。照 `v8-arch-docs-scope-of-target`，在 target 開對應檔的批才加 Legacy 行；本批只在計畫裡寫明每份由哪一批開 target 檔。

## 3. 已知要交使用者的問題（本批產出裡列成未決，附建議，不自己定）

查證時已看到的三題，其餘在盤點中出現時同樣列出：

1. **JSON 解析庫在 Engine 的契約資料解析裡**：`nlohmann` 今天被 `engine/asset`（`ContentManifest.cpp`、`catalog/CatalogParser.cpp`）、`engine/render`（`ShaderLibrary.cpp`）、`engine/ui`（`UiDocumentCodec.cpp`、`UiSerialization.cpp`）、`engine/map_markers`、`engine/voxel_mesh`、`engine/model` 直接含入；`engine/base` 的 `StrictJson.hpp` 以模板參數接它。照三條原則它要有自己的 adapter；照「Engine 永不 include plugins」，Engine 的契約資料解析又不能依賴 plugin。另見記憶中使用者先前的方向「JSON 函式庫是可插拔模組」（framework-controller-principle）。本批要提出選項（例：Engine 定一個中性的文件樹介面、JSON 的 adapter 以第 1 類 plugin 提供並在連結時選定；或判定 JSON 屬「固化的基礎」留 Engine）與各自的代價。
2. **建置期工具**：`engine/render/shaders/pipeline`（shader 編譯工具，含 `SDL3_shadercross`、DXC）在建置時跑、不進執行檔。三條原則是否涵蓋建置期工具、它算不算 Engine 的一部分。
3. **候選模組的歸屬**：model 的取樣規則與 audio（使用者說「可以考慮」放 plugin）——本批查清它們的對外介面、規則會不會被替換、決定性守衛要怎麼跟著搬，給出建議。

## 4. 做法

ultracode 的小編制（變速箱：要求大且跨多批、要證據；上段）。**編制照使用者 2026-11-13 同意的臨時做法**：Opus 統合，Fable 只做一次敵對檢查（平常的上段編制是 Fable 統合）。

| 角色 | 模型／檔位 | 做什麼 |
|---|---|---|
| 偵察 | Opus／high ×2（上段的偵察不用小模型） | (a) `engine/` 與 `third_party/` 的連結與含入事實、公開標頭掃描（`git grep -P`，先以已知正例命中、不丟 stderr）；(b) 遷移計畫受影響各批的現況摘錄 |
| 提案者 | Opus／high ×3 | 同一組事實，三個角度：①風險與可逆（搬動順序、守衛、決定性）②價值（Feature 與產品可見的效果、最小可行的搬法）③成本（工時、批的切分、關鍵路徑） |
| 統合者 | Opus／high | 比較三案，寫成產出 1～5 的單一稿 |
| 敵對檢查者 | **Fable／high**，一次，只讀 | 否定統合稿：與已定前提矛盾、歸類違反判斷順序、中性介面的事實錯、計畫各批不一致、估時沒有出處 |
| 修訂與定稿 | 主會話（Opus）／high | 處置敵對檢查的發現、寫 HANDOFF、dev_log、開 PR |

所有 agent 明寫模型與檔位（不繼承）。偵察與提案的原始輸出存主 checkout `build/evidence/adapter-plugins-plan/`。

## 5. 判定（看結果前宣告）

1. **歸類表的件數**：`engine/` 下的頂層模組目錄（今天 23 個，含 `kernel`、`config`）與 `third_party/` 下的外部組件（今天 10 個目錄：asio、doctest、httplib、imgui、nlohmann_json、protobuf、sdl3、sdl3_image、sdl3_ttf、ufbx；另有建置期使用、不在 `third_party/` 的 SDL_shadercross 與 DXC——開工時以 `ls` 與 CMake 的 FetchContent／find_package 重數、寫進件数の突き合わせ）每一個都在表裡，件數相等。
2. **歸類照順序、不與已定歸屬矛盾**：每一列寫出三個問題的答案與證據；collision、math 是留 Engine（已定）；網路（含 `engine/net`）是 plugin（已定）；SDL、SDL_image、SDL_ttf、ufbx 是第 1 類（已定）；ImGui 是第 2 類（已定）。有任何一列套準則後得到與已定歸屬相反的結論＝停止條件。
3. **中性介面盤點有證據**：每個後端的公開、非 native 標頭以掃描判定是否含外部組件的型別或標頭；掃描先以已知正例（例：`engine/platform/sdl/native/` 的 SDL 含入）命中；結果與掃描指令一起寫進產出 §2。
4. **計畫內部一致**：§2 樹、§2.2 表、§3 各批、§4 Delta、§5.2 守衛四處對同一批的範圍與依賴說法一致；批數與估時的加總重算一致；每個新估時有出處（audit、既有批的實績或標「推定」）。
5. **不改已定前提**：spec K.3 的 17 項、計畫 §7 的 U1～U14、今天的決定行都不動；需要動的列給使用者。
6. **敵對檢查 0 blocker**（Fable／high）；should 全部處置或寫不採理由。
7. **只有 `docs/` 有 diff**。

## 6. 範囲の限界

- 只規劃，不驗條文可實作；中性介面的形狀以盤點與提案為準，由各實作批的事前宣告定案。
- 「替換成另一個組件」要到第二個實作出現才算驗證過，本批的規劃在那之前只算預留（`v8-arch-external-components-as-adapters` 的限界）。
- 不以 Bevy 為根據（使用者的立場刻意與它不同）；參考實作只在說明取捨時引用。
- 四環境量測（`v8-arch-measure-env-baseline`）與本批無依賴；它會改的是 U8 與預算，不是層的歸屬。
- U1（頁保護是否採用）仍延到 G2a 後；它的 OS Delta 不在本批範圍。

## 7. 時點、估時、停止條件

- **時點**：要在 `v8-arch-plugins-foundation` 的事前宣告之前完成（它要定選擇欄）；plugins-foundation 排在 registry-schedule 之後，所以本批與 registry-schedule 的宣告可以並行。
- **估時**：8～16 h（推定；比照遷移計畫的規劃批，範圍較小但要逐模組查證）。
- **停止**：AGENTS §14 冒頭三條；某模組套準則與已定歸屬矛盾；某題在三條原則與「Engine 永不 include plugins」之間沒有可行選項（列給使用者，不自己定）；總時數超過 24 h（16 h×1.5）或單一問題累計超過 2 h。

## 8. 若失敗

plugins-foundation 的選擇欄沒有依據，它的事前宣告不能核准；G2a 之前的其他批（registry-schedule、plan-dryrun、world-exec、loading、threading）不受影響。

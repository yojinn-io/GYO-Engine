# 2026-11-14 `v8-arch-engine-own-plugins-plan`：Engine 模組改為 Engine 自有 plugin 的規劃（B1）

Owner：架構（架構師會話開出的任務卡 04）。只動 `docs/`。宣告：`docs/architecture/plans/kernel/engine-own-plugins-plan-declaration.zh-Hant.md`（使用者 2026-11-14 核准，選 A）；規劃稿：`docs/architecture/plans/kernel/engine-own-plugins-plan.zh-Hant.md`。證據：主 checkout（`~/Code/Source/GYO-Engine`，本機 Mac）的 `build/evidence/engine-own-plugins-plan/`（偵察 2 份、提案 3 份、統合者的計畫修訂清單、Fable 敵對檢查報告；git 不追蹤）。

## 做了什麼

- **規劃稿**：`engine/` 23 個模組逐一的去處——7 個成為 Engine 自有 plugin（asset、audio、input、model、render、text、ui；另衍生 `model_render`、`ui_render`、中性 platform 契約），其餘是 Kernel 之下的基礎、函式庫、機制或已定進 `plugins/`；歸屬 0 列改變。Contract 提案：App 記錄（產品的 System 直接登記到 App，F2）、`plugin.json` 的 `owner` 欄、以 Runtime 為粒度的選擇欄、需求宣告、設定值。七題（S-3 (2)、S-3 (1)、S-1、S-5、S-8、必答 1～3）各附選項與建議。新批 `v8-arch-app-assembly` 與 12 個既有批的差分。Delta-8 七項。target DI 與 spec 的提案（不改原文）。〈仍往產品加步驟的條目〉前後對照 7→7。未決 B1-U1～U22 全部交使用者。
- **遷移計畫修訂**：§3.31b 補全、新增 §3.20b app-assembly、受影響各批加「B1 的差分」、§2 樹與表、§4 Delta-8、§8 RC-8／RC-11。B1 帶來的變更全部標「待使用者核准（B1-U…）」；核准前合計仍是現行的 701～1,450 h。
- **target、spec、HANDOFF 決定行**：沒有改（宣告 §4；卡 03 在落 runtime-composition 的 DI）。

## 經緯

1. 事前宣告寫完停下；使用者核准、選編制 A。
2. ultracode 小編制：偵察 Opus／high×2 → 提案 Opus／high×3（風險、產品自由度、成本）→ 統合 Opus／high（寫規劃稿與計畫修訂清單）→ 敵對檢查 Fable／high 一次（照使用者目前的臨時做法：Fable 只用一次）。workflow 共 7 個 agent。
3. Fable 的結果：blocker 1／should 15／nit 12。blocker B-1：`CFingerprintOf`（`engine/kernel/src/Fingerprint.cpp:310-326`）寫入 zone-C 項目在全體中的絕對 id，和 spec「只含 zone=C 的部分」不一致。主會話讀原始碼確認後，照 AGENTS §14.3 報給架構師；架構師開了**缺陷卡 08**（先 Red，改成 zone-C 內序位＋名字，升版本到 3）。規劃稿改為「另由卡 08 處理」，B1-U15 改為知悉。
4. 修訂：宣告 §6 寫「修訂與定稿由主會話做」，實際交給一個 Opus／high 的修訂子代理（處置量大：28 條加上計畫落字），處置方針由主會話逐條定。28 條全部處置：S-7 的建議從 (ii) 改為 (i) 刪（避免重新引入 review 已刪的產品側守衛）；S-8 刪掉已處置過的 imgui `requires: ["input"]`；S-13 照判定 3 延後了三項沒有 Feature 支撐的守衛／DI；N-8 把 app-assembly 改到架構線、Opus／xhigh。
5. 修訂後**沒有再跑敵對檢查**。唯一的 blocker 已移出 B1（卡 08），本稿不再含那個主張；要不要再檢查一次，由使用者決定。

## 判定（宣告 §5）

1. 件數：成立。§1 共 23 列；`git ls-tree -d --name-only HEAD engine/`＝23（正例 `engine/kernel` 命中）。
2. 不改已定歸屬與前提：成立。Fable 核對 0 列改歸屬。動到的已定文字（spec C.2、DI 2 增補、DI 6、HANDOFF 決定行 B1 條、計畫 §3.11 預留形狀等）都列成未決，本批沒有改。
3. 每項有支撐的 Feature：成立。標不出的三項已延後（S-13）。
4. 七題有選項與建議：成立。
5. 計畫一致性：成立。修訂者用 Python 自查：樹 38＝表 38；724.5、1,507、338～707、208、441.5 在兩檔各處一致。主會話抽查了這五個數。
6. 不往產品加步驟：列數 7→7，內容有變。刪第 3、4、5 條；新增 3 條：寫初值（條件，F1）、main 以 Runtime 名取清單（F1）、產品 `plugin.json` 寫 `owner: product`（交使用者）。
7. 敵對檢查：Fable／high 一次，blocker 1（移交卡 08）、should 15、nit 12，全部處置。修訂後的稿沒有重驗 0 blocker。
8. 範圍：成立。只有 `docs/` 有 diff；`target/`、`spec.zh-Hant.md`、`HANDOFF.md` 決定行的 diff 是 0（HANDOFF 只加現況一行）。

## 數字（若 B1 的未決照建議核准；全為推定）

| 項目 | 數字 |
|---|---|
| 執行批淨增 | +23.5～57 h（差分 15～40＋app-assembly 8.5～17） |
| 計畫合計 | 701～1,450 → 724.5～1,507 h；非條件批 36→37 |
| 關鍵路徑 | 333～696 → 338～707 h |
| 超出 RC-11 錨點 1,299 h 的量 | 151 → 208 h |
| RC-11 餘裕 | 441.5 h（不觸發） |

## 時間

約 10 h（推定，含 workflow 約 39 分鐘的 wall-clock、修訂約 14 分鐘），在宣告估時 8～16 h 之內。

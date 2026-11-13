# 2026-11-13 `v8-arch-adapter-plugins-plan`：外部組件改成 adapter plugin 的規劃

Owner：架構（架構師會話）。只動 `docs/`。宣告：`docs/architecture/plans/kernel/adapter-plugins-plan-declaration.zh-Hant.md`（使用者核准）；規劃稿：`docs/architecture/plans/kernel/adapter-plugins-plan.zh-Hant.md`。

## 做了什麼

- **規劃稿**：`engine/` 23 個模組、`third_party/` 10 個與建置期 3 個外部組件，照使用者的判斷順序逐列寫三個問題的答案與歸屬；9 個後端的中性介面盤點；`project.json` 單一選擇欄的形狀提案（欄名由 plugins-foundation 定）；未決 17 題，每題附選項、代價、建議；三案取捨。
- **遷移計畫修訂**：批數 34→36（新增 `v8-arch-adapters-relocate`：G2a 後、wrap-render 前「只搬不改形狀」；條件批 `v8-arch-json-adapter`）；受影響的 17 批改範圍、判定與估時；新增 Delta-7；§5.2 的守衛去處；§8 的 RC。非條件批合計 632～1,299 → 681～1,400 h（全推定）；關鍵路徑 331～692 → 332～694 h。
- **target**：`runtime-composition.zh-Hant.md` §4 寫入判斷順序、三條原則、兩類 plugin、同一選擇欄、SDL 家族的依賴宣告；Design Invariants 節逐位元未動。
- **入口** `docs/architecture/README.md`：`Layers and dependencies` 條目刪「待寫入 §4」一句。

## 經緯

1. ultracode 小編制：偵察 Opus／high×2 → 提案 Opus／high×3（風險、價值、成本）→ 統合 Opus／high。編制照使用者同意的臨時做法（平常上段由 Fable 統合）。
2. Fable／high 敵對檢查一次：blocker 6、should 13、nit 8。主要是兩處未決寫成已定（target 頁的初始化順序、一個守衛的去處）、歸類違反判斷順序（model core、trace core）、中性介面的事實錯、網路的消費者漏了 Gateway 的 Go 測試與兩個 PvP 測試目錄、計畫內部不一致。
3. Opus／high 修訂者處置全部 27 條，沒有不採；其中 5 條轉成未決。照順序是 plugin、但建議留 Engine 的 model core 與 audio core，歸屬寫成「待使用者」，不改寫答案配合結論。
4. Opus／high 點檢者（只讀）：blocker 6／6 已解決；should 12 已解決、1 部分（S-13 的三處網路分層用語，主會話補完）；新不一致 5 條（舊數字、修訂記錄與本文相反、計數重現不出），主會話修正並重跑加總，結果不變。
5. 掃描先以已知正例命中；第一次查證時架構師用了 `git grep -E` 配 `\s`（ERE 不支援），零命中被正例對照抓到，改用 `-P`。

## 判定（宣告 §5）

| # | 結果 |
|---|---|
| 1 歸類表件數 | 成立（23＋10＋3＝36 主列；建置期外部組件比宣告多 SPIRV-Cross 一個，宣告漏數） |
| 2 照順序、不與已定歸屬矛盾 | 成立（照順序是 plugin 的兩列改列未決 3） |
| 3 中性介面盤點有證據 | 成立（修訂後；原稿四處事實錯已改） |
| 4 計畫內部一致 | 成立（修訂與點檢後；加總以腳本重算） |
| 5 不改已定前提 | 成立 |
| 6 Fable 敵對檢查 0 blocker | **不成立**：唯一一次 Fable 檢查 6 條 blocker；修訂後只經同模型點檢。是否以一次 Fable 做限定範圍的複查，待使用者 |
| 7 只動 docs/ | 成立 |

## 證據

主 checkout（Intel Mac）`build/evidence/adapter-plugins-plan/`（git 管理外）：偵察兩份、提案三份、統合筆記與加總腳本、敵對檢查報告、修訂者的掃描與加總腳本、點檢報告。

用時：workflow 約 71 分鐘＋修訂 24 分鐘＋點檢 7 分鐘＋主會話處置（估 8～16 h 內）。

## 複核之後（同日追加）

1. 使用者對第一稿的疑慮：「這個規範其實是給 Engine 套上了鐐銬。」架構師以 Opus／high 盤點 74 條規範性條目、寫複核意見；使用者在討論卡逐項談，定出根因（三原則被綁到能力歸屬）與一組改動已定項（決定行 `v8-arch-adapter-plugins-review`）。未決 1 結案為 yyjson＋原文路（`v8-arch-json-yyjson-raw-number`）。
2. 第二次修訂（Opus／high）照決定改寫；判定 6 (c) 的 Fable／high 檢查一次：blocker 4／should 18／nit 7。Fable 本機實測浮點 `std::from_chars` 在 macOS 13.3 不可用；shader 工具搬家會產生共通 build → plugin 的邊。兩題交使用者，定為 fast_float 與只搬編譯器（`v8-arch-json-fast-float`、`v8-arch-shader-compiler-in-sdl-plugin`）。
3. 第三次修訂（Opus／high）處置全部 29 條；七條交主會話的待定項列在規劃稿處置記錄，要在各自指定的批之前定。
4. 結果：37 批、非條件 701～1,450 h、關鍵路徑 333～696 h；configure 錯誤 9→3；補遺二守衛 7→3。判定 6 以「Fable 檢查一次、處置後」記為成立（使用者的 (c)）；第三次修訂沒有再經不同模型檢查。

證據：主 checkout `build/evidence/adapter-plugins-plan/` 的 `review-inventory.md`、`discussion-notes.md`、`adversary-final.md`、`reviser2-sum.*`、`reviser3-sum.*`。

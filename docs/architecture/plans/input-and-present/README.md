# 輸入與呈現：分批計畫與進度

更新：2026-10-04。Owner：Engine（`engine/input`、`engine/input/backend/sdl`、`engine/platform/sdl`、`engine/render/backend/sdl_gpu`；`engine/runtime` 只讀）。
**狀態：未開始。** 本文件與 [PLAN](PLAN.md)、[HANDOFF](HANDOFF.md) 於 2026-10-04 建立，沒有任何批次開始。

Engine 的輸入層與呈現路徑各有一個缺口：

- 輸入：`Key` 只有 14 個鍵，視窗互動事件（移動、縮放、縮小、還原）沒有 Engine 表示。遊戲與工具兩種獨立消費端都繞過 Engine，直接讀 `SDL_Event`。
- 呈現：`SdlGpuRenderDevice` 以阻塞方式取得 swapchain。單執行緒的 `RuntimeLoop` 在 macOS 拖動／縮放視窗時跟著停住，最長約 1.2 秒。

本計畫補上這兩個缺口。何時擷取或釋放指標、停頓時如何處理遊戲邏輯，仍由各消費端決定。

先讀 [批次計畫](PLAN.md)（範圍、驗收、平台表、Architecture Delta）和 [交接](HANDOFF.md)（正式項目清單、決策、證據）。

## 來源

- [架構漂移健檢（2026-10-04）](../../../checkup/2026_10_04_architecture.zh-Hant.md)「應收進 Engine 的功能」中，優先度最高的是輸入層。
- 規劃提出需求的消費端時（2026-10-04），使用者決定把 Engine 平台的兩個項目（渲染阻塞主迴圈、輸入層缺口）放到本夾，作為正式來源。
- **提出需求的消費端**是一個線上遊戲：它在 L3 人工驗收中觀測到停頓，也直接處理 SDL 視窗事件。需求與證據以文字摘要寫在 [HANDOFF](HANDOFF.md)；刪除該消費端後，本計畫仍可獨立理解。
- 本夾的文件不連結、也不點名消費端（D12）；程式檔案路徑與證據路徑作為資料保留。

## 已定案方針（2026-10-04，使用者）

1. **D7 先量測再選修法**：IP-2 先分開量 fence 等待與 `nextDrawable`。
   - 停頓在 fence：改為不阻塞取得，取不到時走既有的 `Skipped`，並加節流。
   - 停頓在 `nextDrawable`（SDL 內部）：停下，帶證據請使用者在兩個方案之間選擇：「視窗移動／縮放期間暫停 acquire（用 IP-1 的視窗事件）」或「分執行緒」。
2. **D8 檔位**：ultracode 與高於主對話的檔位，逐批開始時說明並徵求同意。
3. 輸入層以加法擴充：保留既有 `Key` 名稱與 `NativeEventObserver`；擷取政策不進 Engine；文字輸入與剪貼簿維持候選，不在本計畫。
4. **D12 匿名**：本夾的文件一律寫「提出需求的消費端」，不寫產品名，也不連結消費端的文件。
5. **D13 IP-2 的 L2 在合併前完成**：若消費端的呈現分析器必須修改才能處理「`Skipped` 變常見」，IP-2 停下；先由消費端在 IP-2 的 base commit 上做驗收工具修正批（仍是阻塞取得），再回到 IP-2 跑 L2。
6. 驗收平台比照消費端既有的驗收：實機只有 macOS Intel／Metal；CI 四平台跑 L1；GPU 測試只在 Linux lavapipe；Windows／Linux 實機與 macOS arm64 實機標「未執行」。

## 進度

| 批次 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|
| IP-1 輸入層：完整 scancode 與視窗互動事件 | high；公開介面以 ultracode 審查（開始時徵求同意） | 未開始 | `Key` 以加法擴充為完整鍵盤 scancode；同一幀內的上升沿次數（或依序的事件清單）與點擊座標；視窗互動事件（含縮放的新寬高）與本視窗過濾；`tests/common/input` 測試（含同一幀按下再放開）；遷移清單加 preview 的數字鍵。不改任何消費端 |
| IP-2 呈現不阻塞主迴圈 | high；取得、節流與 `Skipped` 語意局部 xhigh | 未開始 | 先拆分量測；停頓在 fence 時改為不阻塞取得＋節流，Engine 層假 device 單元測試；停頓在 `nextDrawable` 時停下請使用者選擇。before 在本批 base commit 重量；L2 在合併前完成（D13）。不宣稱 Windows 已解決 |

```text
IP-1 ─────────────────────────────→ 消費端改用 Engine 輸入（消費端自己的批次）
  └─（只在 D7 選「暫停 acquire」時）─┐
FF-2（共通測試的計時假設）─────────┼→ IP-2 ─→ 消費端重取量測基線 B1
消費端的拖動重現（量測基線 B0）────┘
IP-1、IP-2 ─→ FF-7（include 路徑統一，大範圍搬移，排在後面以免衝突）
```

FF 批次見 [基礎後續整理](../foundation-followups/README.md)。

合併順序只是建議，用來減少衝突與方便歸因，不是依賴；兩批同時完成時由後合併的一方 rebase。真正的依賴只寫在上圖與 [PLAN](PLAN.md) 的依賴欄。例外：FF-2 必須先於 IP-2（`MeshUpdateSmoke` 的語意）；FF-7 依賴 IP-1、IP-2、FF-4、FF-5。

- render：建議 FF-2 → IP-2 → FF-4 → FF-7（其中「FF-2 先於 IP-2」與「FF-7 在 IP-2、FF-4 之後」是依賴）。
- `InputActionMap.cpp`：IP-1 與 FF-3 不同時進行；先後不限，後合併的一方 rebase。

## 執行規則

- 每次只執行使用者指定的批次；一批一個 PR，commit 與 PR 用日語。
- 每批開始、里程碑、停止時更新本表、[HANDOFF](HANDOFF.md) 與 dev_log（`docs/dev_logs/YYYY_MM_DD_engine_ip1.zh-Hant.md`、`..._engine_ip2.zh-Hant.md`），然後停止，不自動開始下一批。
- 主對話檔位由使用者決定；表中檔位是建議值。ultracode 與高於主對話的檔位，在該批開始時說明理由並徵求同意（D8），不沿用前一批的同意。
- 先凍結來源、產物與分析器，再量測；開發與乾淨量測不同時進行；失敗跑次保留，先做有限定位。長測另外授權。
- 每批同步更新未啟用產品的遷移清單：寫在 [foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md) 的「IP」節，本夾不另建（格式沿用 [Math](../math-foundation/inactive_products.md)／[Result](../result-unification/inactive_products.md)）。
- 不改變權威結果。正式證明是同機兩樹（base／branch）digest 比對；CI 只做自洽檢查。不修改共通 workflow。
- 出現非預期回歸、範圍擴大、停頓位置與預期不同，或需要新增依賴邊時，停下回報並重新規劃。

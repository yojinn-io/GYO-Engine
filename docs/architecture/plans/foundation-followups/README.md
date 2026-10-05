# 基礎後續整理：分批計畫與進度

更新：2026-10-05。Owner：Engine（collision、render、ui、base、asset、input、共通測試、建置登錄、`tools/ui_editor` 的對應部分）。
**FF-1～FF-6、FF-8 完成並合併**（2026-10-05，PR #43～#49）；FF-7、FF-9 未開始。

本計畫承接三類 Engine 層的後續事項：

- [Math 基礎統一](../math-foundation/README.md)（PR #17～#28，含後續 #28）各批「範圍外，只回報」的項目中，屬於 Engine、工具、共通測試與建置登錄的部分。
- 測試與驗收器跨平台稽核中，屬於共通測試的部分（依賴主機計時精度或檔案系統精度的判定）。
- [架構漂移健檢（2026-10-04）](../../../checkup/2026_10_04_architecture.zh-Hant.md)的兩個候選：GYOP 傳輸標頭的 C++ 編解碼（Engine 只有 Go 版）、Collision 合法性檢查公開（Engine 只在內部 assert，消費端各自重寫且規則已不一致）。

本夾是上述項目的**正式來源**。提出需求的消費端只保留自己的產品項目，並以連結指向本夾。本夾文件依 D12 完全匿名：一律寫「提出需求的消費端」，不寫產品名，也不連結消費端的文件；程式檔案路徑與證據路徑作為資料保留。
同時進行的姊妹計畫是 [輸入與呈現](../input-and-present/README.md)（IP-1、IP-2），兩者的依賴與建議合併順序見下。

先讀 [批次計畫](PLAN.md) 和 [交接](HANDOFF.md)。未啟用產品的遷移需求見 [遷移清單](inactive_products.md)。

## 已定案方針（2026-10-04，使用者）

1. **D3 Collision 徹底統一**：「這個地方不想再埋坑」。
   - ① 所有公開查詢（含 `RaycastAabb`）共用一套 double 實作與單一容差，刪除 float 演算法；公開簽名與 float 回傳型別不變。
   - ② `VerticalCapsule` 多載只是轉換到 `Math::Capsule` 的薄包裝。
   - ③ 公開 `IsValid`（`VerticalCapsule`、`Aabb`、射線輸入）；Engine 內部 assert 與消費端的驗證都改用它，統一 `min>=max` 與 `min>max` 兩種規則。
   - ④ 退化膠囊一併拒絕。
   - 語料以同機兩樹比對，不為比較而公開內部函式。
2. **D4 GYOP 標頭收進 Engine**：24-byte 標頭的 C++ 編解碼歸 Engine；位元組不變；排在消費端的協議升版批之前；訊息種類留在產品。
3. **D6 共通測試改為結構條件**：MeshUpdateSmoke、package checks、AssetWatcher 全部改；未啟用產品的 `test_gpu_smoke` 只寫進遷移清單。
4. **D8 檔位**：ultracode 與高於主對話的檔位，逐批開始時說明並徵求同意。
5. **D10 include 路徑風格**：另立一批徹底統一（盤點、七點 Delta、無別名、更新遷移清單）；不卡消費端的協議批。
6. **D11⑦** `Render::Color` 與 `UiColor` 保留兩個型別，只收斂有限性檢查，並記錄不合併的理由。
7. **D11⑧** `object_fps_preview` 在 `engine/config/tools.csv` 停用；輸入、Math、Result 的遷移需求寫進遷移清單。
8. **D11⑨** `item_step` 加有限性驗證，不升 gyo.ui 版本（`kUiSchemaVersion`），但在 `docs/ui_toolchain.md` 契約註明「超出 float 範圍的有限 double（例如 1e39）也會被拒絕」；提交的共通測試只用合成 fixture，產品資料的比對以一次性 scratch 腳本進行並記錄結果。
9. **D12 Engine 計畫文件匿名**：本夾與輸入與呈現夾一律寫「提出需求的消費端」，不寫產品名，不連結消費端的文件；程式檔案路徑與證據路徑作為資料可以保留。反向（消費端文件連到本夾）不受限。

依既有決定的定案（2026-10-04，主對話）：

- **FF-4**：render 內四份 Color 有限性檢查（`RenderQueue`、`Renderer`、`SdlGpuRenderDevice`、`ModelRenderer`）收斂為 render 內一份；Ui 的 `UiValidation` 那一份留在 Ui，並記錄理由（`gyo_ui` 不依賴 render；放進 Math 違反健檢「沒有消費者前不要擴充 Math」；依 D11⑦ 保留兩個型別）。
- **FF-8**：1200 bytes 是 Engine 的傳輸契約（與 Go 的 `framing.go` `MaxDatagram` 一致），產品可以設更小的上限；@22 依 Go 命名為 `Channel`（v1 只用 channel 0）；版本與 Type 由呼叫端檢查，「拒絕條件等價」以消費端組合後的整體行為證明。
- **FF-9**：容差選 `1e-7`（現行移動用的 `kTolerance`）時，預期移動與 Client 預測不變；選其他容差時，移動的變化必須事前宣告。停止條件是「出現未宣告的變化」。
- **檔位**：xhigh 只用在局部。FF-9 主體為 high（ultracode 依 D8 開始時徵求同意），只有容差選擇、`IsValid`、差異歸因局部 xhigh。
- **合併順序**只是建議，用來減少衝突與方便歸因，不是依賴：後合併的一方 rebase。依賴欄只寫真正的依賴；例外是 FF-2 必須先於 IP-2（MeshUpdateSmoke 語意），以及 FF-7 依賴 IP-1、IP-2、FF-4、FF-5（大範圍搬移，避免衝突）。

範圍不含未啟用產品（`object_fps`、`object_fps_v2`、`tools/object_fps_preview`）的程式碼；它們重新啟用前的遷移需求記在 [遷移清單](inactive_products.md)。
維持候選、不在本計畫：文字輸入與剪貼簿、AssetManager `LoadShared`、產品登錄資料搬出 `engine/config`（D11②），以及 Logging（健檢的候選，不屬 D11②）；見 HANDOFF。

## 進度

| 批次 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|
| FF-1 Collision 判定語料 | high | 完成，PR [#46](https://github.com/yojinn-io/GYO-Engine/pull/46) 已合併（`0dc3697`）） | `tests/common/collision` 合成語料，涵蓋全部公開查詢；可比對紀錄；膠囊 float／double 公開多載的命中翻轉與 ULP 統計。不改 Collision |
| FF-2 共通測試的計時假設 | high（MeshUpdateSmoke）／medium（其餘） | 完成，PR [#43](https://github.com/yojinn-io/GYO-Engine/pull/43) 已合併（`3520dd2`）） | MeshUpdateSmoke、package checks、AssetWatcher 改為結構條件；`test_gpu_smoke` 寫進遷移清單 |
| FF-3 FNV-1a 收為一份 | high | 完成，PR [#44](https://github.com/yojinn-io/GYO-Engine/pull/44) 已合併（`006b3d2`）） | `GYO::Base` 公開 header；三份實作改用它；新增依賴邊 `gyo_input→GYO::Base` |
| FF-4 有限性檢查收斂 | high | 完成，PR [#47](https://github.com/yojinn-io/GYO-Engine/pull/47) 已合併（`70aa2c9`；原建議在 IP-2 之後合併，使用者指示依序先合併） | render 內 4 份 Color 有限性檢查收斂為一份；`UiValidation` 那份留在 Ui；兩個 Color 型別保留，兩者都記錄理由 |
| FF-5 Ui 與 ui_editor 的重複、`item_step` 驗證 | high（契約部分 medium） | 完成，PR [#49](https://github.com/yojinn-io/GYO-Engine/pull/49) 已合併（`d66a442`）） | ui_editor 改用 Engine Ui 的 letterbox、viewport 判定、文字對齊；`UiRuntime` 走訪合併；`item_step` 驗證與契約文件 |
| FF-6 共通層衛生與產品登錄 | medium | 完成，PR [#48](https://github.com/yojinn-io/GYO-Engine/pull/48) 已合併（`c6eed3d`）） | characterization helper 收進 tests/common；共通層去除產品名；preview 停用並寫進遷移清單 |
| FF-7 include 路徑風格徹底統一 | high（大範圍掃描建議 ultracode） | 未開始 | 全部公開 include 根目錄統一、所有消費端一次改完、無別名 |
| FF-8 GYOP 標頭 C++ 編解碼 | high（解碼拒絕條件局部 xhigh） | 完成，PR [#45](https://github.com/yojinn-io/GYO-Engine/pull/45) 已合併（`745590b`）（未升 xhigh，見 HANDOFF） | 新增最小 Engine 子系統（單一 target）；1200 bytes 為 Engine 傳輸契約；@22 命名 `Channel`；與 Go framing 共用合成 golden 向量；位元組不變 |
| FF-9 Collision 統一與公開合法性檢查 | high（ultracode 開始時徵求同意；容差選擇、`IsValid`、差異歸因局部 xhigh） | 未開始 | D3 ①–④；事前宣告與事後兩樹比對；與消費端權威批同一 PR |

## 依賴圖

```text
FF-1 Collision 語料 ──────────────────────────────┐
                                                  ├─→ FF-9 Collision 統一（與消費端權威批同 PR）
〔消費端已升新協議版本〕─────────────────────────────┘

FF-2 共通測試計時 ─→ IP-2（MeshUpdateSmoke 語意）
FF-3 FNV-1a ─→〔消費端協議批的 arena digest 使用它〕
FF-8 GYOP 標頭 ─→〔消費端協議批之前〕
FF-4 有限性、FF-5 Ui／item_step、FF-6 共通衛生與登錄（Engine 內無前置批次）
IP-1、IP-2、FF-4、FF-5 ─→ FF-7 include 統一
```

上圖只畫真正的依賴。

建議合併順序（只用來減少同檔衝突與方便歸因，不是依賴；後合併的一方 rebase）：

- Collision：FF-1→FF-9（FF-9 本來就依賴 FF-1）。
- render：FF-2→IP-2→FF-4→FF-7。其中 FF-2→IP-2 與 →FF-7 是依賴；FF-4 排在 IP-2 之後只是建議（兩批都改 `SdlGpuRenderDevice.cpp`）。
- `InputActionMap.cpp`：IP-1 與 FF-3 先完成者先合併，後者 rebase；不是依賴。

FF-1、FF-2、FF-3、FF-4、FF-5、FF-6、FF-8 沒有 Engine 內的前置批次，由使用者分別啟動。

## 執行規則

- 每次只執行使用者指定的批次；一批一個 PR，commit 與 PR 用日語。
- 每批開始、里程碑、停止時更新本表、[HANDOFF](HANDOFF.md) 與 dev_log（`docs/dev_logs/YYYY_MM_DD_engine_ffN.zh-Hant.md`），然後停止，不自動開始下一批。
- 每批同步更新 [未啟用產品的遷移清單](inactive_products.md)。
- 主對話檔位由使用者決定；表中檔位是建議值。ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意（D8）。
- 先凍結來源、產物與分析器再量測；開發與乾淨量測不同時進行。失敗跑次保留，先有限定位。長測另外授權。
- 「權威不變」的正式證明是同機兩樹（base／branch）比對；CI 只做自洽檢查與不依賴 libm 的子集。不修改共通 workflow 來容納產品的比對。
- 每批文件有平台表。實機只有 macOS Intel／Metal；CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）跑 L1；GPU 測試只在 CI 的 Linux lavapipe；Windows D3D12 實機、Linux 實機、macOS arm64 實機標「未執行」。
- 出現非預期回歸、範圍擴大，或超出事前宣告的變化時，停下回報並重新規劃，不事後放寬。

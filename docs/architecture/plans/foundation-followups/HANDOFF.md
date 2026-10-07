# 基礎後續整理：交接

更新：2026-10-05。**FF-1～FF-6、FF-8 完成並合併**（PR #43～#49，2026-10-05 由使用者指示依序合併）；FF-7 完成並合併（PR #52，`ef12037`）；**FF-9 完成並合併**（2026-10-07，與提出需求的消費端的權威變更批同一個 PR #58，`7226ca0`）。 本文件建立於計畫落盤時（基準 master `05042fa`），之後每批開始、里程碑、停止時與工作同一變更更新。

## 閱讀入口

1. [進度與執行規則](README.md)。
2. [批次計畫](PLAN.md)：FF-1…FF-9 的範圍、驗收、平台表、Architecture Delta、停止條件。
3. 本文件：決策、正式項目清單、各批紀錄、未結事項。
4. [未啟用產品的遷移清單](inactive_products.md)：object_fps、object_fps_v2、object_fps_preview 重新啟用前要做的遷移（本計畫與 IP 計畫共用）。
5. 相關：[Math 基礎統一](../math-foundation/HANDOFF.md)（項目的原始來源）、[輸入與呈現](../input-and-present/HANDOFF.md)（姊妹計畫）、[架構漂移健檢](../../../checkup/2026_10_04_architecture.zh-Hant.md)。

## 決策紀錄

| 日期 | 決策 | 來源 |
|---|---|---|
| 2026-10-04 | Math 基礎統一的範圍外事項，與消費端協議升版一起處理 | 使用者 |
| 2026-10-04 | D1：Engine 工作放兩個 Engine 計畫夾（本夾 FF、輸入與呈現 IP），正式清單移到 Engine 夾；消費端的交接只保留產品列並以連結指向這裡 | 使用者 |
| 2026-10-04 | D3：Collision 徹底統一。① 全部公開查詢（含 `RaycastAabb`）共用一套 double 實作與單一容差，刪除 float 演算法；② `VerticalCapsule` 多載只是轉換到 `Math::Capsule` 的薄包裝；③ 公開 `IsValid`（`VerticalCapsule`、`Aabb`、射線輸入），Engine 內部 assert 與消費端驗證改用，消除 `min>=max` 與 `min>max` 的不一致；④ 退化膠囊一併拒絕。語料以同機兩樹比對，不為比較而公開內部函式（使用者原話：「這個地方不想再埋坑，用徹底的方案」） | 使用者 |
| 2026-10-04 | D4：GYOP 24-byte 標頭的 C++ 編解碼收進 Engine；位元組不變；排在消費端協議升版批之前；訊息種類留產品 | 使用者 |
| 2026-10-04 | D6：共通測試的計時判定全部改為結構條件（MeshUpdateSmoke、package checks、AssetWatcher）；未啟用產品的 `test_gpu_smoke` 只寫進遷移清單 | 使用者 |
| 2026-10-04 | D8：ultracode 與高於主對話的檔位，逐批開始時說明並徵求同意 | 使用者 |
| 2026-10-04 | D10：include 路徑風格另立一批徹底統一（盤點、七點 Delta、無別名、更新遷移清單；不卡消費端協議批） | 使用者 |
| 2026-10-04 | D11⑦：`Render::Color` 與 `UiColor` 保留兩個型別，只收斂有限性檢查，記錄不合併的理由 | 使用者 |
| 2026-10-04 | D11⑧：`object_fps_preview` 在 `engine/config/tools.csv` 停用；輸入、Math、Result 的遷移需求寫進遷移清單 | 使用者 |
| 2026-10-04 | D11⑨：`item_step` 加有限性驗證，不升 `kUiSchemaVersion`；`docs/ui_toolchain.md` 註明超出 float 範圍的有限 double（例如 1e39）也會被拒絕；提交的共通測試只用合成 fixture，產品資料以一次性 scratch 腳本比對並記錄 | 使用者 |
| 2026-10-04 | D11⑩：驗收平台：實機只有 macOS Intel／Metal；CI 四平台跑 L1；GPU 測試只在 Linux lavapipe；Windows／Linux 實機、macOS arm64 實機標「未執行」 | 使用者 |
| 2026-10-04 | D11②：文字輸入與剪貼簿、AssetManager `LoadShared`、產品登錄資料搬家維持候選（Logging 是健檢的候選，不屬 D11②） | 使用者 |
| 2026-10-04 | D12：Engine 計畫文件（本夾與輸入與呈現夾）完全匿名、不連結：一律寫「提出需求的消費端」，不寫產品名，不連結消費端的文件；程式檔案路徑與證據路徑作為資料保留。反向（消費端文件連到 Engine 文件）不受限 | 使用者 |
| 2026-10-04 | 合併順序只是建議（減少衝突、方便歸因），不是依賴，後合併的一方 rebase；依賴欄只寫真正的依賴。例外：FF-2 先於 IP-2（MeshUpdateSmoke 語意）；FF-7 依賴 IP-1、IP-2、FF-4、FF-5。`InputActionMap.cpp` 由 IP-1 與 FF-3 先完成者先合併 | 主對話依既有決定定案 |
| 2026-10-04 | FF-4：render 內四份 Color 有限性檢查（`RenderQueue`、`Renderer`、`SdlGpuRenderDevice`、`ModelRenderer`）收斂為 render 內一份；`UiValidation` 那一份留在 Ui 並記錄理由（`gyo_ui` 不依賴 render；放進 Math 違反健檢「沒有消費者前不要擴充 Math」；D11⑦ 保留兩型別）。因此刪除「`UiValidation`：FF-4→FF-5」的合併順序與 FF-5 對 FF-4 的依賴 | 主對話依既有決定定案 |
| 2026-10-04 | FF-8：1200 bytes 是 Engine 的傳輸契約（與 Go `framing.go` 的 `MaxDatagram` 一致），產品可以設更小的上限；@22 依 Go 命名為 `Channel`（v1 只用 channel 0）；版本與 Type 由呼叫端檢查，「拒絕條件等價」以消費端組合後的整體行為證明 | 主對話依既有決定定案 |
| 2026-10-04 | FF-9：容差選 `1e-7`（現行移動用的 `kTolerance`）時預期移動與 Client 預測不變；選其他容差時，移動的變化必須事前宣告；停止條件是「出現未宣告的變化」 | 主對話依既有決定定案 |
| 2026-10-04 | 檔位：xhigh 只用在局部。FF-9 主體 high（ultracode 依 D8 開始時徵求同意），只有容差選擇、`IsValid`、差異歸因局部 xhigh | 主對話依既有決定定案 |
| 2026-10-05 | FF-7 的統一方向：公開 include 根目錄統一為 `engine/<m>/`（多數模組已是此形式；有前綴，不與消費端或第三方 header 撞名） | 使用者 |
| 2026-10-05 | FF-9 維持與提出需求的消費端的權威變更批同一個 PR，不單獨進行 | 使用者 |
| 2026-10-05 | FF-7 先於 IP-2：IP-2 的程式尚未開始，搬移在沒有其他程式變更進行時最乾淨；IP-2 建在搬移後的 master 上 | 使用者 |
| 2026-10-05 | PR 以功能線為單位，盡量少開（每次合併觸發全量 CI）；合併後的狀態同步併進同一條線的 PR | 使用者 |

## 正式項目清單

本節是下列項目的**正式來源**（2026-10-04 從消費端的交接移入，並套用規劃時查證的更正）：

- Math 基礎統一範圍外事項（消費端交接第10項）中，屬於 Engine、工具、共通測試、建置登錄的各列。
- 測試與驗收器跨平台稽核（消費端交接第8項）中，屬於共通測試的各列。

消費端交接只保留產品列（arena 內容比對、未編譯檔中的 `GroundPoint` 與 Enemy 容差、第8項的產品驗收器各列），並以連結指向本節。

### A. Math 基礎統一的範圍外事項

來源：[Math 基礎統一](../math-foundation/HANDOFF.md)（2026-10-03～04，PR #17～#28，含後續 #28）各批「範圍外，只回報」的項目；「來源批次」指該計畫的 PLAN 章節或批次。

| # | 項目（含更正） | 範圍 | 來源批次 | 批次 |
|---|---|---|---|---|
| A1 | Collision 內部 float 與 double 兩套演算法並存，容差不同。float 路徑在 `engine/collision/src/Collision.cpp`（`kEpsilon 1e-6f` `:14`；`RaycastAabb` `:164`、`VerticalCapsule` 版 `RaycastCapsule` `:176`、`SweepSphereAgainstCapsule` `:186`）；double 路徑在 `CapsuleQueries.cpp`（`kTolerance 1e-7` `:16`；`Math::Capsule` 多載 `:210`、`:225`；Overlap／Sweep `:239-286`）。只有膠囊同時有兩個公開多載；`RaycastAabb` 沒有公開的 double 版（`RayRoundedBox` 為內部函式）。統一會改變消費端的權威判定；依現況，消費端只有伺服器端射擊查詢走 float 路徑，移動與預測只走 double（`kTolerance 1e-7`）。影響取決於容差選擇：選 `1e-7` 時預期只改變射擊判定，移動與 Client 預測不變；選其他容差（例如 `1e-6f`）時移動也會變，變化必須在 FF-9 事前宣告 | Engine | PLAN 第2節 | FF-1（語料）、FF-9（統一） |
| A2 | `Render::Color`（`RenderTypes.hpp:15`）與 `UiColor`（`UiTypes.hpp:74`）同構。Color 的有限性檢查是 **5 份**（原記 4 份）：`RenderQueue.cpp`、`Renderer.cpp`、`SdlGpuRenderDevice.cpp`、`engine/render/model/src/ModelRenderer.cpp:19-22`、`UiValidation.cpp`。FF-4 收斂 render 內的 4 份；`UiValidation.cpp` 那份留在 Ui 並記錄理由。`gyo_ui` 只連結 `GYO::Engine` 與 `GYO::Math`，由 `gyo_ui_renderer` 橋接 Render，合併型別牽涉依賴方向。SDL 後端的 8-bit Color（`SdlRenderer.hpp:37`）語意不同，不屬同構 | Engine | PLAN 第2節、B7 | FF-4 |
| A3 | FNV-1a 是 **三份**（原記兩份）：`engine/asset/include/engine/asset/detail/Hash.hpp:11-27`（`Detail` 命名空間，非公開 API）、`engine/input/src/InputActionMap.cpp:6-19`（空字串回傳 0，和標準 FNV 不同）、`tools/ui_editor/src/FileService.cpp:21-28`。三份常數相同。`gyo_input` 目前不連結任何模組，收進 Base 會新增 `gyo_input→GYO::Base` | Engine、工具 | PLAN 第2節 | FF-3 |
| A4 | include 路徑風格不一致：`engine/{asset,base,collision,input,io,math,runtime}` 為 `engine/<m>/`；model、render、text、ui 與各 backend、`render/model`（`model_renderer/`）、`platform/sdl`（`platform/`）為 `<m>/`。統一屬大規模 File Move 與 Public Interface 變更 | Engine、工具 | PLAN 第2節 | FF-7 |
| A5 | `object_fps_preview` 的登錄無效：`engine/config/tools.csv` 為 `enabled=true`、`default=false`；它的 `requires_apps.game=object_fps`，而 object_fps 在 `projects.csv` 為停用；選取 preview 時 `build/cmake/GyoTools.cmake:41` FATAL_ERROR。它的 2 個 TU 可以編譯，但 link 依賴 object_fps 的 `app_support`，所以它的輸入遷移目前無法建置驗證 | 建置登錄 | PLAN 第2節 | FF-6 |
| A6 | gyo.ui 的 `item_step` 沒有有限性驗證：`{NaN, 1}` 能通過 Validate，序列化後變成 `null` 而無法讀回。另外 codec 先以 double 檢查有限性再轉 float，所以手寫的 1e39 現在可載入（變成 float 的 +inf），新規則會拒絕它。契約文件是 `docs/ui_toolchain.md`（JSON v1），版本常數 `kUiSchemaVersion`（`engine/ui/include/ui/UiDocument.hpp:16`）。屬資料契約變更 | Engine（Ui）、資料契約 | B5 | FF-5 |
| A7 | ui_editor 與 Ui 的 letterbox、viewport 內判定、文字對齊各有一份；`UiRuntime` 的 Evaluate 與 Compose 重複 layout 走訪。**更正**：點擊判定已改用 Engine 的 `HitTestUiLayout`，不再重複 | Engine（Ui）、工具 | B5 | FF-5 |
| A8 | characterization 的共用 helper（`SameBits`、`UlpDistance`、`Opaque`）在 tests/common、消費端測試、tests/ui_editor 各一份；共通測試不能依賴產品測試 | 測試 | B7 | FF-6 |
| A9 | 共通層出現產品名：`services/gyo_gateway/README.md:8-9,13` 實際寫出產品名與路徑；`tests/common/ci/test_workflow_gates.py:349` 是確認舊 CMake 變數已移除的負向檢查（性質不同，可改為不具名或註明保留理由） | 共通層 | B7 | FF-6 |
| A10 | Collision 的膠囊驗證有 **兩份**：`Collision.cpp:29-34`（float，`radius*2.0f`）與 `CapsuleQueries.cpp:17-21`（double，`2.0*radius`），都接受半徑極小（約 `2^-23` 倍）、`height == 2r` 的退化膠囊。此時 `segmentTop < segmentBottom` 可能成立，`Collision.cpp:109` 的 `Math::Clamp` 前置條件被違反。**更正**：`GYO_ASSERT` 在所有建置都會中止，不只 debug | Engine（Collision） | 純量統一後續 | FF-9 |

已解決、不轉入（記錄用）：

| 項目 | 處置 |
|---|---|
| 根目錄三份 README 的範例 `cmake --preset dev -DGYO_APPS=object_fps` 指向停用中的產品 | 已解決：2026-10-04 文件整理改為目前啟用的產品 |
| float 純量的 clamp／min／max 寫法 | 不轉入：使用者決定在 Math 計畫的後續中統一，已由 #28 完成 |

留在消費端（不在本夾）：arena 只比對 id 與 version（消費端協議批）；未編譯檔中的 `GroundPoint` 有限性檢查與格子線段檢查、Enemy 攻擊時間容差（隨消費端刪除未編譯檔結案）。

### B. 共通測試的計時假設（跨平台稽核的共通列）

來源：2026-10-03 使用者要求的測試與驗收器跨平台稽核。原則（使用者定案）：對玩家的門檻跨平台相同、不因平台放寬；測量工具對主機計時精度或視窗／GPU 行為的假設，優先改成結構條件；非得用時間常數時以實測基線解讀；不依 OS 名稱分支。當時這些項目在 macOS 都通過，沒有失敗證據；依 D6 全部改。

| # | 項目（含更正） | 批次 |
|---|---|---|
| B1 | `tests/common/render/sdl_gpu/MeshUpdateSmoke.cpp:237-249`：16 幀都必須 `Presented`；視窗被遮蔽或 headless GPU 會回 `Skipped`。標籤為 `gpu`，只在 CI 的 Linux lavapipe 與本機執行。IP-2 會讓 `Skipped` 變常見，是直接的壓力 | FF-2 |
| B2 | `tests/common/ci/test_package_checks.py:75-80`：子程序須在 0.2 秒的 timeout 內印出啟動訊息。`tests/common/ci` 經 CTest 在 CI 四平台執行 | FF-2 |
| B3 | `tests/common/core/asset/AssetWatcherTests.cpp:39-66`：20 ms 後改寫同大小檔案，依賴檔案系統 mtime 精度（FAT 為 2 秒） | FF-2 |
| B4 | `tests/object_fps/package_tools/test_gpu_smoke.py:55-60`（0.5 秒）：屬未啟用的 object_fps，不會執行也無法驗證 | FF-2（只寫進遷移清單） |

### C. 健檢候選

| 項目 | 處置 |
|---|---|
| 公開 Collision 的合法性檢查（`ValidateCapsule` 只在內部 assert；消費端各自重寫且規則不一致） | 納入 FF-9（D3） |
| GYOP 傳輸框架的 C++ 版本（Engine 只有 Go 版；C++ 版寫死版本號與訊息種類） | 納入 FF-8（D4） |
| Collision 的語意改變，消費端沒有任何訊號（無效輸入改為 assert 後，未啟用產品原本 catch `invalid_argument` 的內容驗證會直接中止） | 由 FF-9 的公開 `IsValid` 提供事前檢查手段；遷移需求寫進遷移清單 |
| 文字輸入與剪貼簿、AssetManager `LoadShared`、產品登錄資料搬出 `engine/config` | 維持候選（D11②） |
| Logging | 維持候選（健檢的候選，不屬 D11②） |

## 各批紀錄

| 批次 | 狀態 | 紀錄 |
|---|---|---|
| FF-1 | 完成，PR [#46](https://github.com/yojinn-io/GYO-Engine/pull/46) 已合併（`0dc3697`） | 見下方「FF-1」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff1.zh-Hant.md) |
| FF-2 | 完成，PR [#43](https://github.com/yojinn-io/GYO-Engine/pull/43) 已合併（`3520dd2`） | 見下方「FF-2」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff2.zh-Hant.md) |
| FF-3 | 完成，PR [#44](https://github.com/yojinn-io/GYO-Engine/pull/44) 已合併（`006b3d2`） | 見下方「FF-3」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff3.zh-Hant.md) |
| FF-4 | 完成，PR [#47](https://github.com/yojinn-io/GYO-Engine/pull/47) 已合併（`70aa2c9`） | 見下方「FF-4」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff4.zh-Hant.md) |
| FF-5 | 完成，PR [#49](https://github.com/yojinn-io/GYO-Engine/pull/49) 已合併（`d66a442`） | 見下方「FF-5」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff5.zh-Hant.md) |
| FF-6 | 完成，PR [#48](https://github.com/yojinn-io/GYO-Engine/pull/48) 已合併（`c6eed3d`） | 見下方「FF-6」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff6.zh-Hant.md) |
| FF-7 | 完成，PR [#52](https://github.com/yojinn-io/GYO-Engine/pull/52) 已合併（`ef12037`） | 見下方「FF-7」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff7.zh-Hant.md) |
| FF-8 | 完成，PR [#45](https://github.com/yojinn-io/GYO-Engine/pull/45) 已合併（`745590b`） | 見下方「FF-8」節與 [dev_log](../../../dev_logs/2026_10_05_engine_ff8.zh-Hant.md) |
| FF-9 | 完成，PR #58 已合併（`7226ca0`，與消費端同一個 PR） | 見下方「FF-9」節與 [dev_log](../../../dev_logs/2026_10_07_engine_ff9.zh-Hant.md) |

每批開始時在此新增一節：分支、base commit、檔位（含使用者同意的紀錄）、事前宣告（若有）、里程碑、證據位置、結果與停止理由。

### FF-2（記錄器）

- 2026-10-05：使用者指示「開始 Engine 側的工作」；依建議順序先做 FF-2（IP-2 的前提）。分支 `claude/engine-ff2`，base master `6381e9d`。
  檔位照計畫：MeshUpdateSmoke 用 high，package checks、AssetWatcher 用 medium；不用 ultracode，沒有高於主對話的檔位，不需另外同意（D8）。
- 2026-10-05：B1 MeshUpdateSmoke、B2 package checks 照宣告完成，突變都被抓到；B3 AssetWatcher 的測試改完並通過。
- 2026-10-05：**停止**：B3 的突變 20 次只有 18 次失敗。原因是 `AssetWatcher.cpp` 的 `FileTimeToSystemNs` 每次以當下時鐘換算，同一檔案時間約 2.2% 的換算結果不同，watcher 會偶發誤報 `Modified`。修正屬範圍擴大，等使用者決定。
- 2026-10-05：使用者決定在 FF-2 一起修。`FileTimeToSystemNs` 改為 process 內只取樣一次時鐘差；新增「沒變的檔案 Poll 1,000 次不得回報」測試（舊實作 10／10 失敗、新實作 20／20 通過）；原宣告的突變改為 20／20 失敗。
- 2026-10-05：全部 CTest 55／55；完成，開 PR。

### FF-3（記錄器）

- 2026-10-05：使用者離開前指示「把 Engine 側的功能完成」。依建議順序接著做 FF-3。分支 `claude/engine-ff3`，疊在 FF-2（`claude/engine-ff2`）上。
  檔位 high（計畫建議值，等於主對話檔位），不用 ultracode。
- 2026-10-05：先加 characterization 測試鎖住舊雜湊值（`AssetId`、`AssetType` 標準型別、`InputActionId`／`InputAxisId` 含空字串為 0、ui_editor 的檔案內容雜湊），在舊實作上通過。
- 2026-10-05：新增 `engine/base/include/engine/base/Fnv1a.hpp`（`Engine::Base::Fnv1a64(std::string_view)`，constexpr）與參考向量測試；三處改用；刪除 `engine/asset/include/engine/asset/detail/Hash.hpp`（含沒有使用者的 `HashCombine`）。
- 2026-10-05：CTest 55／55；依賴邊比對（`cmake --graphviz`，base 與 branch）只新增 `gyo_input→gyo_base`，沒有刪除的邊。完成，開 PR。

### FF-8（記錄器）

- 2026-10-05：接續使用者「把 Engine 側的功能完成」的指示。分支 `claude/engine-ff8`，疊在 FF-3 上。
  檔位 high。計畫建議「解碼拒絕條件局部 xhigh」，但 xhigh 高於主對話檔位，使用者不在、無法徵求同意（D8），所以不升檔；改以先鎖舊行為的拒絕表測試補足，並在 PR 標明。
- 2026-10-05：先加消費端的 characterization（7 種訊息的位元組、完整拒絕表、超長編碼丟例外），在舊的 `Wire.hpp` 上通過。
- 2026-10-05：新增 Engine 子系統 `engine/net`（target `gyo_net`／`GYO::Net`，只依賴 `GYO::Base`）：`Engine::Net::EncodeGyopDatagram`／`DecodeGyopDatagram`，錯誤以 `Result<…, NetError>` 回傳。include 暫用 `engine/net/` 風格（與 base、input 相同），由 FF-7 統一。
- 2026-10-05：合成向量 `tests/common/net/fixtures/gyop_datagram_vectors.json`（以 Python `struct` 獨立產生），C++（`gyo_net_tests`）與 Go（`services/gyo_gateway/framing/vectors_test.go`）讀同一份。
  突變：拿掉 channel／length／1200 檢查、encode 接受 channel 1，C++ 測試都失敗；過程中發現自己寫的測試以暫存物件做 range-for，迴圈其實沒跑，已修正後才得到這個結果。
- 2026-10-05：消費端 `Wire.hpp` 改用（版本收成 `ProtocolVersion` 一個常數，Type 範圍與 TCP frame 留在產品）。characterization 通過；CTest 56／56；權威 digest 同機兩樹（base＝FF-3 tip）35 個情境 0 不同；L2 clean-60 冒煙 base／branch 都通過（report-only）。完成，開 PR。

### FF-1（記錄器）

- 2026-10-05：接續使用者「把 Engine 側的功能完成」。分支 `claude/engine-ff1`，疊在 FF-8 上。檔位 high（計畫建議值），不用 ultracode。
- 2026-10-05：語料完成：`tests/common/collision/CollisionCorpus.{hpp,cpp}`（12 類紀錄、16,940 筆）、`gyo_collision_corpus` runner（`--out`、`--stats`、`--degenerate`）、`CollisionCorpusTests.cpp`（涵蓋全部公開查詢、process 內決定性、統計自洽）。Collision 的程式沒有改，也沒有公開內部函式。
- 2026-10-05：L1：同機重跑兩次紀錄逐位元相同（SHA-256 `666f17ca…`）；靈敏度（scratch，不提交）：float `kEpsilon` 1e-6→1e-5 改變 9 筆；double `kTolerance` 1e-7→1e-5 改變 32 筆、→1e-9 改變 14 筆。第一次檢查時 double 容差改動沒有任何影響，原因是語料缺「初始淺重疊後分離」的情境，補上後才有靈敏度。CTest 56／56。完成，開 PR。

#### FF-1 結果：語料規模

| 紀錄 | 筆數 |
|---|---|
| `RaycastAabb` | 850（5 個 box） |
| `RaycastCapsule/vertical`、`/general`（同一輸入的兩個公開多載） | 各 2,490（5 個膠囊×3 種 sweep 半徑） |
| `RaycastCapsule/arbitrary`（傾斜、水平的 `Math::Capsule`） | 1,494 |
| `SweepSphereAgainstCapsule/vertical`、`/general` | 各 2,385 |
| `SweepSphereAgainstCapsule/arbitrary` | 1,431 |
| `OverlapVerticalCapsuleAabb`、`SweepVerticalCapsuleAgainstAabb` | 各 865（2026-10-07 FF-9 更正：原記 857 為筆誤；5 個 box ×（15＋8＋150），總數 16,940 才一致） |
| `OverlapVerticalCapsules`、`SweepVerticalCapsuleAgainstCapsule` | 各 840 |
| `ToCapsule` | 5 |

情境：正面、擦邊（切線、平面上平行、邊緣）、差 2^-20 的內外、近平行（方向分量在 1e-6 與水平分量 1e-3 的 float 容差邊界上下）、max distance 等於／略短於命中距離、零距離、長射線（1e4）、未正規化方向、極短掃掠（5e-7、2e-6）、零位移、初始接觸往內／往外／切線、淺重疊（5e-8～5e-5）往外與切線、高速（1000）、角與邊，以及以整數格點產生的隨機情境。
所有輸入都通過現行驗證，box 每軸嚴格 min<max，膠囊 height 嚴格大於 2r（不放 height＝2r，以免 FF-9 的 `IsValid` 拒絕它們）。

#### FF-1 結果：膠囊兩個公開多載（VerticalCapsule＝float 演算法；ToCapsule 後＝double 演算法）

本機 macOS Intel、Apple clang 21、RelWithDebInfo：

| 查詢 | 情境 | 兩者皆未中 | 兩者皆命中 | 只有 float 命中 | 只有 double 命中 | 距離 ULP 0／1／2-4／5-16／17-256／>256 | 最大 ULP |
|---|---|---|---|---|---|---|---|
| `RaycastCapsule` | 2,490 | 2,085 | 402 | 3 | 0 | 120／50／73／87／56／16 | 2,896 |
| `SweepSphereAgainstCapsule` | 2,385 | 2,200 | 181 | 0 | 4 | 88／26／24／23／5／15 | 3,355 |

命中翻轉合計 7 筆；兩者皆命中時有 31 筆差超過 256 ULP。FF-9 統一為 double 時，這些就是 `VerticalCapsule` 多載的預期變化範圍，需在 FF-9 事前宣告中歸因。

#### FF-1 結果：退化膠囊現況（只記錄，不修）

`gyo_collision_corpus --degenerate`：25 個 height＝2r 的膠囊（feet.y 1～65536，r 1e-3～1e-7）。
- 兩份驗證（float `ValidateCapsule`、double `Validate`）都放行全部 25 個。
- float 算出的 segment 上端低於下端：2 個（feet.y＝1、r＝1e-3；feet.y＝1000、r＝1e-4）。這兩個 float 查詢會在 noexcept 的 `RayCapsuleUnchecked` 內違反 `Math::Clamp` 前置條件；第一次執行時以可拋出的 handler 測試也直接 terminate（`AssertionFailure` 從 noexcept 函式拋出），所以報告不執行它們。
- 其餘 23 個 float 查詢與全部 25 個 double 查詢（經 `ToCapsule`，長度 0 的球）都正常回傳。

#### FF-1：重現方法（供 FF-9）

1. 在 base 與 branch 兩個 worktree 各自建置 `gyo_collision_corpus`（test preset）。
2. `python3 docs/architecture/plans/foundation-followups/scripts/compare_collision_corpus.py --base <base runner> --branch <branch runner> --output <dir>`。
3. 回報每個查詢不同的筆數與第一筆差異；`comparison.json` 保存全部結果。統計以 `--stats`、退化現況以 `--degenerate` 取得。

### FF-9（記錄器）

- 2026-10-07：開始。與提出需求的消費端的權威變更批同一個 PR（見 PLAN 的分工表）。消費端的協議升版已合併，消費端條件成立。
  - 檔位：使用者同意事前宣告以 ultracode 規劃（3 案、1 位評審、1 次對抗檢查）；容差、`IsValid` 規則、差異歸因局部 xhigh，其餘 high。
  - 事前宣告（變動的公開函式、FF-1 翻轉與 ULP 上限、容差與 AABB 規則）在實作前寫入本記錄器。
- 2026-10-07：事前宣告以 ultracode 規劃（3 份草案、1 位評審、1 次對抗檢查；對抗檢查 major 1、minor 10，全部套入），使用者核准。

#### FF-9 事前宣告（2026-10-07，實作前；之後不得放寬）

基準：master `5da939f`。

- base binary 重產的 FF-1 語料共 16,940 筆，SHA-256 `666f17ca…`（與 FF-1 相同）；`--stats`、`--degenerate` 與 FF-1 節相同。
- 平台：macOS Intel、test preset。全專案 `-ffp-contract=off`（`build/cmake/GyoBuild.cmake:35-36`）。
- 本宣告只對同機兩樹成立；CI 只做程序內自洽，不比對跨平台紀錄。
- 附 file:line 的是事實；標「推論」的是推論。

**1. 容差**

- **唯一容差**：`kTolerance = 1e-7`（`CapsuleQueries.cpp:16`），只用於 `SeparatingContact`（`:199-201`）。值、型別與用途都不變。
- **刪除**：
  - float 的 `kEpsilon = 1e-6f`（`Collision.cpp:14`），連同它的四個用途：方向長度（`:26`）、slab 平行帶（`:75`）、水平分量帶（`:122`）、短掃掠改為重疊（`:195`）。
  - `CapsuleQueries.cpp:218` 的方向長度字面值 `1.0e-6`，改由 `IsValid(Ray)` 取代。
- **不算容差，維持不動**，寫進 `Collision.hpp`：
  - `Roots` 的捨入防護 `16ε(|b²|+|ac|)`（`:45-47`）。
  - `RayRoundedBox` 的區間窗 `1e-12`（`:131`）。
  - `Roots` 的捨入防護用於所有 double 查詢（`RayCapsule`：射線、球掃掠、膠囊間 Sweep；`RayRoundedBox`：盒 Sweep）；`1e-12` 區間窗只在 `RayRoundedBox`（盒 Sweep）。改動任一者都會改變移動。
  - 半徑 >0 時，`Roots` 的防護相當於 p≤r＋16εL²／r 的 double 級帶（L＝100 m、r＝0.25 時約 1.4e-10 m），不構成 float 級的第二容差；只有半徑 0 時才退化為 tanθ≤√(32ε)。
- **理由**：語料中有 16 筆初始淺重疊目前回傳阻擋：
  - `SweepVerticalCapsuleAgainstAabb` t0、t1、t2、t4 的 #17、#18。
  - `SweepVerticalCapsuleAgainstCapsule` t0–t3 的 #12、#13。
  - 深度在 4.77e-7～5.07e-7 之間。容差改成 1e-6 時這些會改為放行，也就是改變移動。
  - 選 1e-7 時，Overlap／Sweep 的程式與常數都不變。

**2. 演算法**

- **`RaycastAabb`**：保留 slab 結構，改用 double。
  - 先轉成 `Vec3d`，在 double 中正規化（同 `CapsuleQueries.cpp:216-219`）。
  - 只有方向分量 `== 0`（含 −0）才算平行；不設平行帶。
  - entry、exit 與 `maximumDistance` 的比較都在 double 中做，最後只轉一次 float。
  - 距離為 0 時回傳 +0。
  - 不用 `RayRoundedBox` 半徑 0（推論）：
    - 半徑 0 時，`Roots` 的捨入防護會變成 tanθ≤√(32ε)≈8.4e-8 的擦邊接受帶。這只有 float 等級的精度，等於隱藏的第二個容差。
    - 語料中最近的未命中（t1#8、t2#8 above-top-plane-parallel，間隙／距離＝1.59e-7）只有 1.9 倍餘裕。
- **`RaycastCapsule`、`SweepSphereAgainstCapsule` 的 `VerticalCapsule` 版**：改成薄包裝，先 `ToCapsule`，再呼叫 `Math::Capsule` 版。
  - 刪除 `RaySphere`、`RayAabb`、`RayCapsuleUnchecked`、`ValidateQuery`、`ValidateCapsule`。
- **其餘 double 函式**（`Roots`、`RayCapsule`、`RayRoundedBox`、`Shape`、`BoxContactPoint`、`BoxContact`、`CapsuleContact`、`SeparatingContact`）：運算式不改寫。只允許搬移，以及把驗證換成 `GYO_ASSERT(IsValid(...))`。

**3. 公開合法性檢查**（`Collision.hpp`，`[[nodiscard]] bool … noexcept`）

| 函式 | 規則 |
|---|---|
| `IsValid(const Math::Aabb&)` | min、max 有限，每軸 `min <= max`（允許零厚度） |
| `IsValid(const VerticalCapsule&)` | feet、height、radius 有限；`radius > 0`；`height >= 2r`（沿用 `CapsuleQueries.cpp:17-21` 的 double 判定；`Collision.hpp:19` 的契約 "at least two radii" 不變）。height == 2r（球）合法 |
| `IsValid(const Math::Capsule&)` | 兩個端點與 radius 有限；`radius > 0`（等於現行 `CapsuleQueries.cpp:22-26`）。長度 0 的膠囊（球）合法 |
| `IsValid(const Math::Ray&)` | origin、direction 有限；direction 不是零向量 |

- **Aabb**：
  - 規則等於現行兩份 Engine 驗證（`Collision.cpp:170-172`、`CapsuleQueries.cpp:27-30`），也等於 `Math::Aabb` 的有效定義（`Aabb.hpp:10`）再加上有限性。
  - double 演算法對零厚度有定義。「非空」若有需要，由消費端以內容規則另外檢查。
- **膠囊**（2026-10-07 方向修正，見文末修訂）：
  - 退化膠囊造成的中止一併消除：height == 2r 維持合法，`IsValid` 沿用現行 double 驗證。
  - 理由：FF-1 量過的 25 個 height == 2r 案例，double 路徑全部正常；只有 float 路徑會讓 segment 上下端顛倒，並在 `Collision.cpp:109` 中止。float 路徑刪除後這個危險不存在；double 的 `Shape` 以 double 計算長度，結果 ≥ 0（`CapsuleQueries.cpp:145-148`）。
  - float `radius*2.0f`（`Collision.cpp:33`）與 double `2.0*radius`（`CapsuleQueries.cpp:20`）對有限輸入的判定相同：2r 在 float 中精確，溢位時兩者都拒絕。
  - 不設最小半徑，也不檢查 `ToCapsule` 導出端點的有限性（沿用現行）：極端輸入（例：feet.y＝3e38、height＝1e38）在 `IsValid` 為真時，仍在轉換後的內部 assert 中止，與現在相同，記為已知限制。
  - `VerticalCapsule` 與 `Math::Capsule` 都接受球。
- **射線**（推論）：任何非零的有限 float 方向，在 double 中正規化都不會溢位，也不會下溢。
- **純量**：`maximumDistance`、`sweepRadius`（有限且 ≥0）與 Segment 端點的檢查維持內部 assert，寫進 header 的前置條件，不公開。`Math::Capsule` 的驗證改為公開的 `IsValid(Math::Capsule)`。
- **Engine 內部**：所有檢查改為 `GYO_ASSERT(IsValid(...))`。
  - commit 1、2 中，float 路徑在 `IsValid` 之外保留 `Collision.cpp:24-26` 與 `:193-194` 的內部長度檢查；下表兩個「放寬」列只在 commit 3（刪除 float 路徑）生效，對應的不中止測試也在 commit 3。

合法輸入集合的變化（只有下列；對新舊規則都合法的輸入，結果見 §5）：

| 方向 | 函式 | 輸入 |
|---|---|---|
| 放寬 | `RaycastAabb`、`RaycastCapsule`×2 | 方向長度在 (0, 1e-6] |
| 放寬 | `RaycastAabb`、V 版 `RaycastCapsule`、V 版 `SweepSphereAgainstCapsule` | float `Length` 溢位的有限方向或路徑（`Collision.cpp:25-26`、`:193-194`） |
| 不變 | 接收 `Aabb` 的 3 個 | — |

**4. 結果會變動的公開函式**

- 只有三個：
  - `RaycastAabb`
  - `RaycastCapsule(…, const VerticalCapsule&, …)`
  - `SweepSphereAgainstCapsule(…, const VerticalCapsule&)`
- 其餘公開函式（`ToCapsule`、兩個 `Math::Capsule` 版、`OverlapVerticalCapsuleAabb`、`OverlapVerticalCapsules`、`SweepVerticalCapsuleAgainstAabb`、`SweepVerticalCapsuleAgainstCapsule`）對合法輸入逐位元不變。

**5. FF-1 語料上限**（同機兩樹，branch 對 base；以查詢＋target＋index 為 key）

| 紀錄 | 筆數 | 宣告 | 歸因 |
|---|---|---|---|
| `RaycastAabb` | 850 | 每筆等於精確參考。結果恰 53 筆不同：10 筆 miss→hit（t0–t4 的 #10 near-parallel-1e-6、#12 near-parallel-below-epsilon，值恰為 2.0f `40000000`），43 筆各差 1 ULP（全是 random），其餘 797 筆逐位元相同 | 翻轉：float 平行帶（`Collision.cpp:75`）把正規化後 \|d_y\|＝1e-6f、9.999999e-7f 當成平行。ULP：float 正規化與 slab 除法的捨入（`:82-83`、`:173`） |
| `RaycastCapsule/vertical` | 2,490 | 每筆等於 base 同 key 的 `/general`。所以恰 285 筆不同：3 筆 hit→miss（t0#2、t1#2 side-tangent-outside；t1#6 top-cap-graze）；282 筆的 ULP 1／2-4／5-16／17-256／>256＝50／73／87／56／16，最大 2,896 | float 判別式在切線附近的捨入（`:42-50`、`:121-127`），以及長射線（1e4）的消去。>256 的 16 筆：long 12、side-tangent-inside 3、top-cap-graze 1 |
| `SweepSphereAgainstCapsule/vertical` | 2,385 | 每筆等於 base 同 key 的 `/general`。所以恰 97 筆不同：4 筆 miss→hit（t0–t3 的 #3 short-5e-7-toward）；93 筆的 ULP＝26／24／23／5／15，最大 3,355 | 翻轉：float 把長度 ≤1e-6 的路徑改成起點的重疊查詢（`:195-197`），double 以參數 t 求解（`CapsuleQueries.cpp:231-236`）。>256 的 15 筆全是 long |
| 其餘 9 類：`RaycastCapsule/general`、`/arbitrary`；`SweepSphereAgainstCapsule/general`、`/arbitrary`；Overlap×2（865、840）；Sweep×2（865、840）；`ToCapsule` | 11,215 | 0 筆不同（Contact 全欄位） | 程式與常數不變。語料輸入在新舊規則下都合法：box 每軸嚴格，隨機膠囊 height＝2r＋k/128，k≥1（`CollisionCorpus.cpp:244`、`:273`） |
| 合計 | 16,940 | 恰 435 筆不同，翻轉 17 | — |

- **「恰」的意思**：少於宣告也要停下，因為這代表殘留了平行帶，或包裝的結果不等於 `Math::Capsule` 版。
- **`RaycastAabb` 的精確參考**：
  - 以有理數做 slab，分量恰為 0 才算平行。
  - 命中條件：entry≤exit、exit≥0，且 max(entry,0)·|d|≤`maximumDistance`（平方後精確比較）。
  - 距離＝max(entry,0)·|d|，以中點平方比較正確捨入到 float。
  - base 對參考的實測：翻轉 10 筆；ULP 為 0 的 175 筆、為 1 的 43 筆。
- **double slab 等於參考的理由**（推論）：
  - 實作誤差約為數個 double ulp（相對 ≲1e-15）。
  - 非整值命中距離中，離 float 捨入中點最近的是 0.0026 ULP（相對約 3e-10，t2#93）。
  - 決策邊界分兩類：(a) 軸向單位方向的精確運算（max-equals-hit、max-short-of-hit〔絕對差 1.9e-6〕、zero-max-on-face、zero-max-outside〔含 t3#16〕）；(b) 非軸向方向，相對餘裕 ≥1.0e-5（t2#18、t4#18 long-diagonal）。
- **膠囊「等於 `/general`」的理由**：語料的 `/general` 就是先 `ToCapsule(k)` 再呼叫 `Math::Capsule` 版（`CollisionCorpus.cpp:326-343`），與薄包裝是同一個呼叫序列。
- **commit 1、2**（公開 `IsValid` 與 Engine assert、消費端改用）：16,940 筆全部與 base 相同。
- **branch 的 `--stats`**：
  - RaycastCapsule：`both_miss=2088 both_hit=402`，兩種翻轉都是 0，ULP `{402,0,0,0,0,0}`，`max_ulp=0`。
  - SweepSphereAgainstCapsule：`both_miss=2200 both_hit=185`，翻轉 0，ULP `{185,0,0,0,0,0}`，`max_ulp=0`。
- **branch 的 `--degenerate`**：25 個全部 `IsValid`＝true；commit 3 之後 25 個查詢全部執行並正常回傳（原本因 float 路徑中止而不執行的 2 個也執行）。
- **靈敏度**（scratch，不提交）：`kTolerance` 改成 1e-6 時，分析器必須報告 §1 的 16 筆超出宣告。

**6. 測試與工具**

- **既有輸入都不改**：`CollisionTests.cpp:164-171` 的 height＝2r 在新規則下仍合法（2026-10-07 方向修正）。
- **`:98-111`**：統一後恆等。期望值不變；改名與逐位元比較在 commit 3。
- **其餘既有期望值不變**：數值檢查是 Approx 或精確值；assert 案例（`:42-47`、`:59`、`:243-251`）在新規則下仍不合法。
- **commit 1 新增 `IsValid` 邊界測試**：
  - Aabb：各軸 `min==max` 合法；`min>max`、NaN、inf 不合法。
  - `VerticalCapsule`：`height==2r` 合法；比 2r 小一個可表示值時不合法；r 為 0、負或非有限時不合法。
  - `Math::Capsule`：長度 0（球）合法；端點非有限、r 為 0 或負時不合法。
  - 射線：零向量、−0 向量、非有限都不合法。
  - 球（height == 2r 的 `VerticalCapsule`、長度 0 的 `Math::Capsule`）的查詢結果正確，例如射線穿過球心時距離恰為 d − r（double 多載在 commit 1；V 版多載在 commit 3 刪除 float 路徑後）。
- **commit 3 新增**（float 路徑刪除後才成立）：
  - 次正規方向，以及長度在 (0,1e-6] 的方向，查詢都不中止。
  - 半徑極小、height 等於或略大於 2r 的膠囊，各查詢都不中止（含 FF-1 `--degenerate` 的 25 個）。
  - `CollisionCorpusTests` 加上「兩個多載翻轉為 0、`maximumUlp` 為 0」，作為四平台的程序內自洽檢查。
- **`CollisionCorpus.{hpp,cpp}`**：輸入與紀錄格式不變，只改說明 float 演算法的註解（`hpp:20-21`）。
- **`CollisionCorpusMain.cpp`**：
  - `--degenerate` 在 commit 1 改為報告 `IsValid`；float 端點與 not-run 分支在 commit 3 刪除。
  - `--stats` 的格式不變。
- **分析器**：現行 `scripts/compare_collision_corpus.py` 只數不同筆數與第一筆差異（`:40-52`），不足以檢查本宣告。
  - 在任何實作之前（commit 0）擴充並凍結，內容包括：
    - 逐查詢的翻轉方向與 ULP 分桶。
    - branch `/vertical` 對 base `/general` 的逐筆交叉檢查。
    - `RaycastAabb` 的精確參考檢查。
    - 其餘紀錄逐位元相同。
    - 任何一項不符就以非零結束。
  - **自我檢查（commit 0，凍結前）**：以 base binary 同時當 base 與 branch 執行。分析器必須以非零結束，並恰好報告：`RaycastAabb` 對精確參考 53 筆（翻轉 10＝t0–t4 的 #10、#12；1 ULP 43，key 清單存入證據）；`/vertical` 對 base `/general`，`RaycastCapsule` 285（hit→miss 3）、`SweepSphereAgainstCapsule` 97（miss→hit 4），ULP 分桶同 FF-1；其餘 9 類 0 筆。輸出不同就代表分析器有錯。
  - **全等模式**（commit 1、2 用）：16,940 筆與 base 逐位元相同才以 0 結束。

**7. 消費端**（文字摘要）

- 移動與 Client 預測只走 Overlap／Sweep，不變。
- 射擊查詢使用 `RaycastAabb` 與 V 版 `RaycastCapsule`。
- 消費端自行宣告權威 digest 的預期變化集合為空，golden 不更新。

**8. 停止條件（追加）**

- §5 任何數字超出或少於宣告。
- commit 1、2 的語料出現差異。
- §6 以外的既有期望值改變。
- 分析器在量測之後被修改。

**9. 遷移清單 FF-9 節**

- `min==max` 的 AABB 不會被拒。
- 球形 `VerticalCapsule`（height＝2r）與長度 0 的 `Math::Capsule`（start == end）仍然合法；未啟用產品的兩處球形膠囊與球形頭部 hurt region 不受影響。
- 近平行（\|d_i\|≤1e-6）的射線不再被當成平行，視線判定可能由未中變為命中。
- 內容驗證改為在載入時呼叫 `IsValid`。

使用者決定（2026-10-07）：`RaycastAabb` 用 double slab；接受 commit 0（分析器先凍結）；消費端 golden 不更新。

**修訂（2026-10-07，使用者方向變更，實作前）**：「退化膠囊一併拒絕」改為「退化膠囊造成的中止一併消除」。
- `IsValid(VerticalCapsule)` 沿用現行 double 驗證（height ≥ 2r，球合法）；新增公開 `IsValid(Math::Capsule)`（端點與 radius 有限、radius > 0，等於現行驗證；長度 0 合法）。不加 `ToCapsule` 端點有限的檢查。
- 因此 commit 1 不改變任何輸入的合法性：FF-1 語料在 commit 1、2 必須全等。原「退化膠囊拒絕移到 commit 1」刪除。
- `RaycastCapsule(Math::Capsule)`、`SweepSphereAgainstCapsule(Math::Capsule)` 明列為不變的函式（本來就是 double 路徑）；語料上限（§5）不變。

- 2026-10-07：commit 0（分析器擴充並凍結，不含 Collision 實作）。`scripts/compare_collision_corpus.py` 新增 `--check equal`（全部逐位元相同）與 `--check ff9`（`RaycastAabb` 對精確參考、`/vertical` 對 base `/general`、其餘逐位元相同；翻轉方向與 ULP 分桶）。
  - 自我檢查（base binary 同時當兩棵樹）：以 1 結束，恰好報告 `RaycastAabb` 53（miss→hit 10＝t0–t4 的 #10、#12；1 ULP 43）、`RaycastCapsule/vertical` 285（hit→miss 3＝t0#2、t1#2、t1#6；ULP 1／2-4／5-16／17-256／>256＝50／73／87／56／16，最大 2,896）、`SweepSphereAgainstCapsule/vertical` 97（miss→hit 4＝t0–t3 的 #3；26／24／23／5／15，最大 3,355），其餘 0。與宣告完全一致；1 ULP 的 43 個 key 與規劃時對抗檢查獨立算出的清單相同。`--check equal` 以 0 結束。
  - 凍結清單：分析器、權威兩樹比對腳本、語料產生器（`CollisionCorpus.{cpp,hpp}`）、權威 digest runner（清單本身 SHA-256 `00841fe1d5ba82fc…`）。唯一宣告過的例外：§6 的 `CollisionCorpus.hpp:20-21` 說明註解在 commit 3 更新，屆時以 diff 證明只改註解。證據在 `build/target/_build/test/logs/engine-ff9-20261007/`。
- 2026-10-07：commit 1（公開 `IsValid`：Aabb、VerticalCapsule、Math::Capsule、Ray；Engine 內部 assert 改用；合法輸入集合不變）。語料 `--check equal` 16,940 筆與 base 逐位元相同；消費端的權威 digest 不變。
- 2026-10-07：commit 2（消費端改用 `IsValid`）。語料 `--check equal` 逐位元相同。
- 2026-10-07：commit 3（公開查詢統一為 double 的一套實作與一個容差；刪除 float 演算法、`kEpsilon 1e-6f`、方向長度 1e-6 檢查）。**事後量測全部在宣告內**：
  - 凍結的分析器 `--check ff9`（凍結清單核對：只有宣告過的 `CollisionCorpus.hpp` 註解不同，diff 證明只改註解）：相對 base 恰好 435 筆不同（`RaycastAabb` 53：miss→hit 10、1 ULP 43；`RaycastCapsule/vertical` 285：hit→miss 3，ULP 50／73／87／56／16，最大 2,896；`SweepSphereAgainstCapsule/vertical` 97：miss→hit 4，ULP 26／24／23／5／15，最大 3,355）；三項判定（精確參考、`/vertical` 對 base `/general`、其餘逐位元相同）全部為 0。
  - `--stats`：兩個多載翻轉 0、`max_ulp` 0（RaycastCapsule `both_miss=2088 both_hit=402`；SweepSphereAgainstCapsule `2200／185`）。`--degenerate`：25 個 `IsValid`＝true，全部執行，assert 0。
  - 靈敏度（scratch，不提交）：容差改為 1e-6 時，分析器恰好報告宣告中的 16 筆（Sweep 對 AABB 8、Sweep 對膠囊 8）；還原後重新建置，`--check ff9` 回到 0。
  - 消費端：權威 digest 同機兩樹 35 個情境 0 不同，golden 30／30（由消費端記錄）。
  - CTest 61／61。新增測試：兩個多載逐位元一致、極小與次正規的方向、球（height==2r、長度 0）與極小半徑的各查詢、語料的多載翻轉 0。
  - 一個測試寫錯又修正：近平行射線的第一版幾何讓射線根本到不了盒子，實作是對的；改用夠寬的盒子後，測到「原本被當成平行而未中、現在命中」。
- 2026-10-07：PR #58（與消費端同一個），四平台 CI 通過。消費端的 L2（實機的連線矩陣與動作短測，before／after，事前宣告後執行）：命中類別與拒絕分類沒有可歸因於 Collision 的變化（由消費端記錄）。
- 2026-10-07：PR #58 合併（使用者指示，`7226ca0`）。FF-9 完成。

### FF-4（記錄器）

- 2026-10-05：接續使用者「把 Engine 側的功能完成」。分支 `claude/engine-ff4`，疊在 FF-1 上。檔位 high，不用 ultracode。
  計畫建議 FF-4 在 IP-2 之後合併（兩批都改 `SdlGpuRenderDevice.cpp`）；這只是建議，本批先完成，後合併的一方 rebase。
- 2026-10-05：先加 characterization（RenderQueue 的 mesh／sprite tint、Renderer 的 clear color、ModelRenderer 的 material／instance tint、SDL_GPU 裝置的 clear value；每個 channel、NaN 與 ±inf，錯誤碼與訊息），在舊實作上通過。
- 2026-10-05：`Engine::Render::IsFinite(const Color&)` 放在 `render/RenderTypes.hpp`（緊接 `Color` 之後，所有 render target 都已能看到這個 header）；四處改用。Renderer 的本地 `IsFinite(float)` 只被 Color 版使用，一起刪除。
  `UiColor` 與 `UiValidation` 不動，理由寫在 `UiTypes.hpp` 與 `RenderTypes.hpp` 的註解。
- 2026-10-05：CTest 56／56；沒有改 CMake，依賴邊不變。L2（report-only）：消費端 `action60` 通過；ui_editor 開啟 `minimal.ui.json` 4 秒，正常繪製並在 SIGTERM 後結束。完成，開 PR。

**不合併兩個 Color 型別的理由（D11⑦）**：`gyo_ui` 只連結 `GYO::Engine` 與 `GYO::Math`，由 `gyo_ui_renderer` 橋接 Render；合併到 Render 會新增 ui→render 的邊。合併到 Math 違反健檢「沒有消費者前不要擴充 Math」。`UiValidation.cpp` 的 `UiColor` 有限性檢查因此也留在 Ui。

### FF-6（記錄器）

- 2026-10-05：接續使用者「把 Engine 側的功能完成」。分支 `claude/engine-ff6`，疊在 FF-4 上。檔位 medium（計畫建議值，低於主對話），不用 ultracode。
- 2026-10-05：三份 helper 的行為相同，差別只有 `noexcept` 有無與消費端多一個 double 版 `UlpDistance`，屬可單純合併（停止條件不成立）。共通版 `tests/common/support/CharacterizationBits.hpp`（`Engine::Test`，全部 `noexcept`），共通 Math 測試、ui_editor 測試、消費端測試改用；消費端只留自己的 `Random`。
- 2026-10-05：`services/gyo_gateway/README.md` 改為不具名；`test_workflow_gates.py` 的舊開關守衛改為由 `engine/config/tools.csv` 的登錄名組出（守衛照舊，不寫產品名）。第一版曾用「任何 `GYO_BUILD_*`」的 regex，但 preset 有合法的 `GYO_BUILD_SDL_*` 開關，範圍過寬，改為登錄名。
- 2026-10-05：`object_fps_preview` 在 `tools.csv` 停用（D11⑧），遷移需求已在遷移清單。
- 2026-10-05：CTest 56／56；只選 ui_editor 的設定（`GYO_APPS=` 空、`GYO_TOOLS=ui_editor`）configure 通過，`gyo_ui_editor_preview_srgb_tests` 建置並通過。Removability 檢查進行中（scratch worktree）。
- 2026-10-05：Removability 通過：scratch worktree（`d397b32`）刪除消費端的程式、資產、測試、驗收、文件與 `projects.csv` 登錄列後，test preset configure（選取的遊戲為空、工具 ui_editor）、建置成功，CTest 34／34 通過。完成，開 PR。
  另記：`docs/architecture/plans/math-foundation/scripts/syntax_check.py`、`docs/architecture/plans/result-unification/scripts/` 兩個歷史計畫的腳本仍寫著消費端的路徑；它們不在建置與測試內，屬已結束計畫的紀錄，本批不改。

### FF-5（記錄器）

- 2026-10-05：接續使用者「把 Engine 側的功能完成」。分支 `claude/engine-ff5`，疊在 FF-6 上。檔位 high（`item_step` 契約部分 medium），不用 ultracode。
- 2026-10-05：Ui 公開既有規則 `FitDesignCanvas`、`ContainsUiPoint`、`AlignUiText`（`ui/UiRuntime.hpp`）；UiRuntime、UiRenderer、ui_editor `PreviewAdapter.cpp` 的 letterbox、viewport 內判定、文字對齊改用它們。
  UiRuntime 的 Evaluate 與 Compose 改為同一個 `WalkElements` 走訪。
- 2026-10-05：`item_step` 在 `UiValidation` 檢查有限性（float 之後；1e39 載入成 inf 也被拒絕），錯誤位置是元素的 `/item_step`；不升 `kUiSchemaVersion`；`docs/ui_toolchain.md` 的 JSON v1 契約註明。
- 2026-10-05：驗證：`gyo_ui_tests` 既有的逐位元 layout／draw-list golden 照舊通過；CTest 56／56；scratch 兩樹比對（下節）逐位元相同。完成，開 PR。

#### FF-5 結果：repo 內 UI 資料的 scratch 比對（不提交為共通測試）

- 方法：一次性 scratch dumper（連結 `GYO::Ui`，不提交），對每個檔的每個 canvas、5 種 viewport（1280×720、1920×1080、800×600、1000×1000、333×777）印出 `EvaluatePreviewLayout` 與 `ComposePreview` 的全部欄位位元。
  base＝FF-6 tip `57f3ef6`（`GYO-Engine-v6b04` worktree），branch＝本分支。
- 對象：`assets/object_fps/ui/screens.json`、`assets/object_fps_v2/ui/screens.json`、`assets/object_fps_pvp/ui/screens.json`、`assets/object_fps_pvp/ui/pvp_lobby.json`、`tests/ui_editor/fixtures/minimal.ui.json`、`build/acceptance/ui_editor/fixtures/minimal.ui.json`（75 組 canvas×viewport，1,540 筆 layout 與 draw 紀錄）。
- 結果：全部在新規則下載入成功；兩樹輸出逐位元相同（SHA-256 `768a15a8…`）。三個 `screens.json` 都有 `fixed_step_list`，其 `item_step` 都是有限值。

### FF-7（記錄器）

- 2026-10-05：使用者指示「先把 Engine 的任務清了」，並決定方向 `engine/<m>/`、FF-7 先於 IP-2。分支 `claude/engine-ff7`，自 master `0dfc4cc`。
  檔位 high。計畫建議大範圍掃描用 ultracode，但**沒有使用**：盤點與改寫以腳本對「搬移前實際存在的 header 路徑」逐字比對（不靠前綴猜測），完整性由建置、CTest 與兩樹比對外部驗證。
- 2026-10-05：盤點（搬移前）：11 個非 `engine/` 前綴的根目錄、30 個 header；全 repo 254 行 include 指向它們。其中 160 行在 Engine、`tests/common`、`tools/ui_editor`、`tests/ui_editor`、提出需求的消費端（程式、驗收、測試，含尚未刪除的未編譯檔）；94 行在未啟用產品（不改，寫進遷移清單）。
- 2026-10-05：規則：header 路徑＝`engine/`＋模組在 `engine/` 下的目錄。target 名稱、namespace、CMake 的 include 目錄（仍是各模組的 `include`）都不變；只在 `include/` 之下多一層 `engine/`。

  | 舊根目錄（所在） | 新根目錄 | header 數 |
  |---|---|---|
  | `render/`（`engine/render/include`） | `engine/render/` | 12 |
  | `render/backend/sdl/`、`render/backend/sdl_gpu/`（各 backend 的 `include`） | `engine/render/backend/sdl/`、`engine/render/backend/sdl_gpu/` | 2 |
  | `model_renderer/`（`engine/render/model/include`） | `engine/render/model/` | 1 |
  | `model/`（`engine/model/include`） | `engine/model/` | 3 |
  | `model/backend/ufbx/`（`engine/model/backend/ufbx/include`） | `engine/model/backend/ufbx/` | 1 |
  | `text/`（`engine/text/include`） | `engine/text/` | 3 |
  | `text/backend/sdl_ttf/`（`engine/text/backend/sdl_ttf/include`） | `engine/text/backend/sdl_ttf/` | 1 |
  | `ui/`（`engine/ui/include`） | `engine/ui/` | 5 |
  | `platform/sdl/`（`engine/platform/sdl/include`） | `engine/platform/sdl/` | 1 |
  | `input/backend/sdl/`（`engine/input/backend/sdl/include`） | `engine/input/backend/sdl/` | 1 |

  `model_renderer/` 改為 `engine/render/model/`，依上述規則對應它的目錄（target 仍是 `gyo_model_renderer`）。
- 2026-10-05：驗證：
  - grep：舊根目錄的 include 在使用中的程式為 0，剩下 94 行全在未啟用產品。
  - CMake 沒有變更，依賴邊不變。
  - 建置成功；CTest 56／56（test preset 的 52 個，加上 preset 標籤以外的 go vet、go test、worker、sdl_gpu mesh smoke 4 個）。
  - 兩樹比對（base＝master `0dfc4cc`，在 scratch worktree 建置）：權威 digest 35 個情境 0 不同；collision 語料 16,940 筆 0 不同。
  - 證據在 `build/target/_build/test/logs/engine-ff7-20261005/`。完成，開 PR。
- 2026-10-05：PR #52 的 CI 四平台通過，經使用者同意合併到 master（`ef12037`）。

## 未結事項

- ui_editor 既有缺陷（FF-5 冒煙時發現，不在範圍）：以命令列 `--open` 開檔時沒有選取文件的第一個 canvas（`EditorApp.cpp` 啟動路徑，選單開檔的 `OpenDocument` 有做），第一個 canvas 不叫 `main` 的文件會顯示 `unknown canvas 'main'`。已另開獨立任務建議。

- FF-2 發現：AssetWatcher 在檔案沒變時偶發回報 `Modified`（換算抖動，約 2.2%／次）；使用者決定在 FF-2 修正，已完成。

- FF-7：統一方向（`engine/<m>/` 或 `<m>/`）未定，開始時提給使用者。FF-8 的新子系統暫用 `engine/net/`（與 base、input 相同），由 FF-7 統一。
- ~~FF-9：容差（`1e-6f` 或 `1e-7`）與 AABB 規則（拒絕 `min>=max` 或只拒絕 `min>max`）未定，開始時事前宣告。選 `1e-7` 時預期移動與 Client 預測不變；選其他容差時，移動的變化要列入事前宣告。若選只拒絕 `min>max`，消費端的 arena 規則是否保留「牆必須有厚度」作為遊戲規則，由消費端決定。~~ 2026-10-07 事前宣告結案：容差 `1e-7`；AABB 只拒絕 `min>max`，消費端保留「非空」作為內容規則。
- FF-8：已定案（見決策紀錄）：@22 在 Engine C++ API 依 Go 命名為 `Channel`；1200 bytes 是 Engine 傳輸契約；Engine 解碼只做傳輸層檢查，版本與 Type 由呼叫端檢查。開始時只需確認實作與 Go 契約一致。
- 合併順序：已定案為建議，不是依賴（見決策紀錄）。`InputActionMap.cpp` 由 IP-1 與 FF-3 先完成者先合併，後者 rebase；FF-4 建議排在 IP-2 之後合併，但不是依賴。
- FF-1：兩樹比對腳本已定為本夾 [`scripts/compare_collision_corpus.py`](scripts/compare_collision_corpus.py)。
- FF-9 須知（FF-1 發現）：height＝2r 的膠囊不必半徑極小也會讓 float 路徑的 segment 上端低於下端（例：feet.y＝1、r＝1e-3）；float 查詢會在 noexcept 函式內違反 `Math::Clamp` 前置條件而終止程式。
- 既有文件的連結：Math 基礎統一 HANDOFF 的「範圍外，只回報」節與健檢報告，原本把這些項目指向消費端的交接；改指本夾由計畫落盤批處理，本文件不宣稱已完成。

# 基礎後續整理：未啟用產品的遷移清單

建立：2026-10-04（計畫落盤時）。基準：master `05042fa`。**本計畫各批都未開始；本文件目前只列「預計造成的遷移需求」，沒有任何量測。**

本計畫依範圍不修改 object_fps、object_fps_v2 與 object_fps_preview。各批改動 Engine 公開介面、共通測試或登錄資料後，這些產品重新啟用前需要先遷移；本文件是遷移清單。
重新啟用時，除了本文件，還要處理 [Math 的遷移清單](../math-foundation/inactive_products.md) 與 [Result 統一的遷移清單](../result-unification/inactive_products.md)。計畫與紀錄見 [README](README.md)、[HANDOFF](HANDOFF.md)。

[輸入與呈現計畫](../input-and-present/README.md)（IP）的遷移需求也記在本文件的「IP」節，不另建清單，避免同一件事寫在兩處。

## 1. 結論

| 項目 | 狀態 |
|---|---|
| 已量測的批次 | 無（全部未開始） |
| 新增的編譯失敗 TU | 未量測 |
| 新增的執行期語意變化 | 未量測 |

每批完成時，比照 Math 清單的方法（`git archive` 匯出 scratch、指向 scratch registry 啟用產品、`ninja -k 0`、逐 TU `-fsyntax-only`）量測，並把本節與對應節從「預計」改為實測結果。repo 只讀不寫。

## 2. 各批預計造成的遷移需求

### FF-1 Collision 判定語料

狀態：FF-1 完成（2026-10-05）。

- 無：只新增共通測試與比對腳本，Collision 的程式與公開介面沒有改。

### FF-2 共通測試的計時假設

狀態：FF-2 完成（2026-10-05）。

- `tests/object_fps/package_tools/test_gpu_smoke.py:55-60`：子程序須在 0.5 秒的 timeout 內輸出。重新啟用時比照 FF-2 對 `tests/common/ci/test_package_checks.py` 的修法（D6）：
  子程序輸出後寫就緒檔，測試以 `run_after_ready` 型的包裝讓 timeout 從就緒後才開始計時；runner 不改。
- 未啟用產品若使用 `AssetWatcher`：FF-2 修正了換算抖動造成的誤報 `Modified`，不需要遷移；`WatchedInfo::lastWriteTimeNs` 的值會差一個固定的時鐘差，不影響比較。

### FF-3 FNV-1a 收為一份

狀態：FF-3 完成（2026-10-05）。

- 無：2026-10-05 以 grep 確認 `apps`、`tools`、`tests`、`build/acceptance` 都沒有使用 `Detail::Fnv1a64`、`Asset::Detail` 或 `detail/Hash.hpp`；`AssetId`、`AssetType` 的公開用法與雜湊值不變（characterization 測試鎖住）。
- 未啟用產品若需要 FNV-1a，直接用 `engine/base/Fnv1a.hpp` 的 `Engine::Base::Fnv1a64`。

### FF-4 有限性檢查收斂

狀態：FF-4 完成（2026-10-05）。

- 無破損：只新增 render 的公開函式 `Engine::Render::IsFinite(const Color&)`（`render/RenderTypes.hpp`），沒有移除公開名稱。
- 可選：未啟用產品自己的 Color 有限性檢查可改用它（不影響編譯）。

### FF-5 Ui 與 ui_editor 的重複、`item_step` 驗證

狀態：FF-5 完成（2026-10-05）。

- 未啟用產品的 UI 資料（`assets/object_fps/ui/screens.json`、`assets/object_fps_v2/ui/screens.json`）：2026-10-05 的 scratch 比對中，兩者在新的 `item_step` 規則下都能載入，`gyo_ui_editor --validate` 也是 exit 0；layout 與 draw list 在 5 種 viewport 下與改動前逐位元相同。不需要遷移。
- Ui 新公開的 `FitDesignCanvas`、`ContainsUiPoint`、`AlignUiText`（`ui/UiRuntime.hpp`）屬加法，不需遷移。

### FF-6 共通層衛生與產品登錄

狀態：FF-6 完成（2026-10-05）。`object_fps_preview` 已在 `tools.csv` 停用。

- `object_fps_preview` 在 `engine/config/tools.csv` 改為停用（D11⑧）。重新啟用順序：先依各清單遷移並重新啟用 object_fps（preview link 依賴它的 `app_support`），再把 preview 的登錄改回啟用。
- preview 的輸入遷移見本文件「IP」節；Math、Result 的遷移見各自的清單。
- 未啟用產品的測試若自帶 characterization helper，不受影響；若重新啟用後要使用，改依賴 tests/common 的共通版本 `tests/common/support/CharacterizationBits.hpp`（`Engine::Test::Opaque`、`SameBits`、`UlpDistance`，連結 `gyo_test_support`）。

### FF-7 include 路徑風格徹底統一

狀態：未開始，批次完成時更新。

- 所有舊根目錄的 `#include` 要改成新的根目錄；不留別名，所以未遷移的 include 會直接編譯失敗。
- 2026-10-04 粗略 grep：未啟用產品（程式、測試、驗收）約有 94 行使用 `<m>/` 風格（`render/`、`model/`、`text/`、`ui/`、`input/`、`platform/`、`model_renderer/`）。實際影響取決於 FF-7 選定的方向，批次完成時重新量測。

### FF-8 GYOP 標頭 C++ 編解碼

狀態：FF-8 完成（2026-10-05）。

- 無：2026-10-05 grep `apps/object_fps`、`apps/object_fps_v2`、`tools/object_fps_preview` 沒有 GYOP、`wire::` 或 `Wire.hpp`。
- 未啟用產品若之後使用 GYOP，直接連結 `GYO::Net`（`engine/net/GyopDatagram.hpp`），版本與訊息種類由產品自己決定。

### FF-9 Collision 統一與公開合法性檢查

狀態：未開始，批次完成時更新。

- 數值：`RaycastAabb`、`VerticalCapsule` 版的 `RaycastCapsule` 與 `SweepSphereAgainstCapsule` 改為 double 實作；擦邊與容差邊界的命中可能翻轉，距離可能差數個 ULP。依賴這些結果的產品測試期望值可能要更新。
- 合法性：退化膠囊與（依 FF-9 的決定）`min==max` 的 AABB 可能被拒；違反時 assert 中止。
- 內容驗證：未啟用產品原本以 catch `std::invalid_argument` 驗證內容；Collision 改為 assert 後這招已失效（見 Result 清單）。重新啟用時改為在載入時呼叫公開 `IsValid`。

## 3. IP（輸入與呈現計畫）

IP 計畫的批次完成時，由該批更新本節（IP 夾不另建清單）。

### IP-1 輸入層：完整 scancode 與視窗互動事件

狀態：IP-1 完成（2026-10-05）。

- `tools/object_fps_preview/ViewmodelPreview.cpp:74-82` 直接讀 `SDL_SCANCODE_1..6` 與 `SDL_SCANCODE_ESCAPE`。重新啟用時改用 Engine 的 `Key::Digit1`～`Key::Digit6` 與 `Key::Escape`（`PhysicalInputFrame::Get(Key)` 或 `InputActionMap`）。preview 目前無法 link（見 FF-6），只能在 object_fps 重新啟用後驗證。
- 破損：無。`Key`、`MouseButton` 只在原有值之後追加；`PhysicalInputFrame` 新增 `events`（`std::vector`），既有的 `pressed`／`released`／`held` 語意不變；`NativeEventObserver` 保留。2026-10-05 grep：未啟用產品沒有依賴 `Key::Count`、`MouseButton::Count` 或 `PhysicalInputFrame` 的大小。

### IP-2 呈現不阻塞主迴圈

狀態：未開始，批次完成時更新。

- 未啟用產品使用 sdl_gpu；若 IP-2 改變取得語意，`Skipped` 會變常見。依賴「每幀都 `Presented`」的產品測試或呈現邏輯需要檢查。

# 架構漂移健檢（2026-10-04）

基準：master `c8ee213`（Result／Assert 統一 PR #30–#37 合併後）。前兩次掃描的基準是 `0bd5363` 和 `eeebc1e`，結果沒有另外成文，有延續的項目直接寫進本報告。
判斷標準與手動步驟見 [README](README.md)。本報告的核心是第一節「應收進 Engine 的功能」與第二節「Engine 這一側的問題」；附錄只是順帶記錄。
除了已註明列入待辦的項目以外，其餘都尚未立案。

## 結論

- **Engine 沒有被產品入侵。** `engine/`（含 `engine/math`、`engine/base`）沒有產品名稱或玩法常數；engine 的 CMake 沒有連結任何 app 或 tool；共通層沒有依產品名稱分支。
- **依賴方向正確，沒有循環。** `GYO::Base`（只依賴標準庫）← `GYO::Math` ← `GYO::Engine`，其餘模組依序連結。
- **Math 與 Result／Assert 兩次統一都在 Engine 層完成。** 原本由各專案各自補的缺口已收回 Engine：各 app 的向量型別、自寫的 `Explain(error)`，現在改由 `Engine::Math` 與 `Base::Describe` 提供。目前啟用的消費端（`object_fps_pvp`、`ui_editor`）在同一批 PR 內已完成遷移。
- **主要的待處理項目是 Engine 的缺口**，其中最明確的是輸入層：已經有遊戲和工具兩種獨立消費端在繞過 Engine。

## 一、應收進 Engine 的功能

| 優先 | 功能 | 證據 | 建議的最小做法 |
|---|---|---|---|
| 高 | **輸入層：完整按鍵集與視窗互動事件** | `engine/input/include/engine/input/PhysicalInputFrame.hpp` 的 `Key` 是一份只有 14 個鍵的封閉清單（沒有 Tab，也沒有數字鍵）。<br>`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` 的 `HandleNativeEvent` 直接處理 `SDL_Event`：用 Tab 切換滑鼠鎖定、失焦／移動／縮小時釋放滑鼠、過濾 windowID、點擊擷取。<br>`tools/object_fps_preview/ViewmodelPreview.cpp` 直接讀 `SDL_SCANCODE_1..6`。<br>過去為了產品需求，F3、H、R 都是直接改 Engine 的 enum 加進去的。 | Engine 提供完整的鍵盤 scancode 和視窗互動事件。何時鎖定或釋放滑鼠，仍由各專案自己決定。這屬於 Architecture Delta（`engine/input` 公開介面會擴充），應先寫計畫再動手。<br>**已立案**：[輸入與呈現](../architecture/plans/input-and-present/README.md) IP-1（2026-10-04，由 PvP v6 規劃提出）。 |
| 中 | **公開 Collision 的合法性檢查** | `engine/collision/src/Collision.cpp` 的 `ValidateCapsule` 只在內部使用，失敗時 assert。<br>pvp 的 `Pvp/Arena.cpp`（`Arena::Validate`）和 `Pvp/ShotQuery.cpp` 各自重寫了膠囊、AABB、射線的檢查，而且規則已經和 Engine 不一致：Arena 拒絕 `min >= max`，Engine 只拒絕 `min > max`。 | 由 Collision 公開 `IsValid(VerticalCapsule)`、`IsValidBounds(Aabb)` 和射線輸入的檢查，Engine 內部的 assert 也改用這些函式。出生點數量、牆數這類遊戲規則仍留在產品。<br>**已立案**：[基礎後續整理](../architecture/plans/foundation-followups/README.md) FF-9（2026-10-04，與 Collision 統一一起處理）。 |
| 中 | 文字輸入與剪貼簿 | 同一個 `PvpApplication.cpp` 直接呼叫 `SDL_StartTextInput`、處理 `SDL_EVENT_TEXT_INPUT`，並自己實作 Backspace、Ctrl+A、Ctrl+V（`SDL_GetClipboardText`）。<br>`engine/input` 和 `engine/ui` 都沒有對應的功能。 | 先提供文字輸入事件與剪貼簿這兩個基本能力；輸入框元件等第二個專案需要時再做。 |
| 中 | `AssetManager` 缺少「載入並取得型別化 payload」的入口 | 三個 app 的 `src/App/AssetDefinitionHelpers.hpp` 各有一份 `LoadShared<T>`（Load → `GetSharedConst` → Release，再加上錯誤訊息）。<br>Result 統一修改了 `Load`、`GetError` 的簽名，但沒有補上這個入口。 | 在 `AssetManager` 加一個同功能的成員函式，錯誤訊息改用 `Base::Describe`。 |
| 中低 | `Base::Describe` 不輸出 `UiError` 的位置欄位 | `docs/architecture/error-handling.md` 規定只用一個格式化函式，但 pvp 的 `ExplainUi` 和 ui_editor 的 `UiDocumentBridge.cpp` 都還是自己拼接 `jsonPointer`。 | 為 `UiError` 提供 overload，或建立錯誤時就把 source 和 pointer 併進 `detail`。 |
| 中低 | Logging | Engine 沒有 logging。直接呼叫 `SDL_Log*` 的有 pvp 13 處、ui_editor 6 處、preview 5 處。<br>Assert 文件寫著「Product handlers may log」，但 Engine 沒有提供可以寫入的出口。 | 先觀察。若出現沒有 SDL 的伺服器或工具專案需要 log，再收進 Engine。 |
| 等時機 | GYOP 傳輸框架的 C++ 版本 | Engine 擁有 Go 版（`services/gyo_gateway/framing/framing.go`），C++ 版只存在於 pvp 的 `Pvp/Wire.hpp`，而且版本號 5 和訊息種類都寫死在裡面。 | 第二個連線專案出現，或開始做 v6 協定時，只把 24-byte 標頭的編碼與解碼收進 Engine；訊息種類留在產品。<br>**已立案**：[基礎後續整理](../architecture/plans/foundation-followups/README.md) FF-8（2026-10-04）。 |
| 等時機 | 網路驗收的流程控制 | `build/acceptance/object_fps_pvp/run_network.py`（`free_port`、多行程啟停）只有一個連線專案在用；`build/acceptance/common` 目前只有封裝檢查。 | 第二個專案需要網路驗收時，再收進 `acceptance/common`。 |

### 判斷過、不建議收進 Engine 的

- `Csv.cpp`：目前啟用的專案沒有人用。pvp 裡唯一呼叫它的 `GameData.cpp` 本身沒有被編譯。
- 武器、子彈、敵人、Grid 地圖與碰撞、關卡：屬於 FPS 類型的玩法。
- 角色移動碰撞（move-and-slide）：註解已寫明這是產品自己的策略，Engine 只負責形狀查詢。
- `SnapshotTimeline`、預測、延遲補償：和 pvp 的協定綁在一起，而且只有一個專案使用。

## 二、Engine 這一側的問題

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 中 | **對獨立消費端的破壞性變更** | Math 和 Result 兩次統一都直接刪除或改變公開 API，沒有過渡期。<br>未啟用的 `object_fps`、`object_fps_v2` 有 40 個 TU 無法編譯（共 507 個錯誤）；`tools.csv` 仍把依賴 `object_fps` 的 `object_fps_preview` 標為啟用。<br>遷移清單：[Math](../architecture/plans/math-foundation/inactive_products.md)、[Result](../architecture/plans/result-unification/inactive_products.md)。<br>**待決定**：Engine 公開 API 要不要定棄用規則；未啟用產品要遷移還是刪除；preview 是否先停用。 |
| 中 | **Collision 的語意改變，消費端卻沒有任何訊號** | 無效輸入原本 `throw invalid_argument`，現在改為 `GYO_ASSERT`，在任何建置下都會直接中止（`Collision.hpp`）。<br>未啟用產品是靠 catch 這個例外來做內容驗證的，重新啟用後會直接當掉，而且編譯時不會報錯。<br>只要完成上面「公開 Collision 的合法性檢查」，消費端就能在載入時先驗證。 |
| 中 | 產品登錄資料放在 `engine/config/*.csv` | 每新增或移除一個專案，都要修改 Engine 目錄底下的檔案。要搬到哪裡，屬於 Product Registration 結構的變更（AGENTS.md §3），需要另外立案。 |
| 低 | 同一個建置事實維護在兩處 | `engine/base/include/engine/base/Assert.hpp` 要求 MSVC 加上 `/Zc:preprocessor`，但這個旗標是由 `build/cmake/GyoBuild.cmake` 全域設定，shader 工具又自己設了一次。建議改為 `GYO::Base` 的 INTERFACE 編譯選項。 |
| 低 | Engine 內部的 `IsFinite` 重複 | `IsFinite(float/Color)` 在 render 有 3 份（`RenderQueue.cpp`、`Renderer.cpp`、`SdlGpuRenderDevice.cpp`），`ModelRenderer.cpp` 和 `UiValidation.cpp` 各 1 份。 |
| 低 | Math 的公開介面有一部分沒有消費者 | `Sphere`、`Triangle`、`Matrix3`、`Vec4`、`Vec2i`／`Vec3i`、`Inverse` 等目前只有測試在用。這是使用者核准過的範圍，但沒有消費者之前不要再擴充。 |
| 記錄 | 行為變更 | Math 的 `Clamp` 改為在所有建置都 assert，也不再是 `noexcept`；`math.md` 已記錄。<br>全域 `-ffp-contract=off` 是為了跨平台決定性（`GyoBuild.cmake`）。 |
| 記錄 | 輕微的命名與歸屬問題 | `MeshLayer::ViewModel` 的名稱帶有 FPS 色彩，但機制本身是通用的。<br>`services/gyo_gateway` 由 engine 擁有，卻放在根目錄的 `services/`；README 的目錄表已補上說明。 |

## 附錄 A：流程與專案內部（不屬於健檢核心）

- **計畫用的工具放在 `docs/` 底下，已重複兩次。** Math 和 Result 兩個計畫都把可執行的 `scripts/*.py`（語法檢查、依賴邊、稽核）和 `baseline/*.tsv` 放在 `docs/architecture/plans/` 裡，而且重新產生了同一批資料（`gyo_dependency_edges.txt`、`pvp_uncompiled_syntax.tsv`、`core_tests.txt`）。依 AGENTS.md §5（手動 → Script → Tool），這已經足以考慮把通用的稽核與語法檢查移到 `build/` 底下的開發支援工具。
- **pvp 有 29 個未編譯的 `.cpp`（約 7,500 行）。** `apps/object_fps_pvp/sources.cmake` 裡的 `APP_DOMAIN_SOURCES`、`APP_SUPPORT_SOURCES` 沒有任何地方使用。這是專案內部的事，但 Math 和 Result 兩個計畫都額外替它們做了遷移，實際拖慢了 Engine 的工作。要保留還是刪除，由專案負責人決定。
- pvp 的測試 include 驗收目錄 `build/acceptance/object_fps_pvp`，驗收腳本的啟停流程也有重複。兩者屬於同一個 owner，可以接受。

## 附錄 B：已確認無問題

- `engine/` 不含產品名稱。唯一的命中是 asset 文件中的範例 `"player_tex"`，以及動畫的 playback player，兩者都是通用用法。
- 共通層沒有產品分支。唯一的命中是 `tests/common/ci/test_workflow_gates.py` 中的負向檢查，它確認舊的 CMake 變數已移除。
- app 和 tool 裡沒有自己實作的 Result、expected 或 assert 巨集；pvp 的各種 `*Result` 都是遊戲規則的資料型別。
- 沒有新的最上層目錄。沒有 Editor 依賴 Runtime 的情況，也沒有新的產品間依賴。
- `object_fps_preview` 對 `object_fps` 的依賴已經寫進登錄資料（`tools/object_fps_preview/project.json`）。

# framework 整改：v8-framework-catalog-structure（catalog 登錄的結構與 JSON 文字分開）

日期：2026-11-01。分支 `claude/framework-rework`。檔位 high（主會話，Opus 5.5），沒有子 Agent。估 8～12 h，實際約 2 h。

**狀態：** 完成（Mac 上的測試、突變、全量 CTest、L2 兩次 configure）。Windows、Linux 由 CI 的 L1 驗證。

相關文件：[PLAN 第 2.5 節](../architecture/plans/framework-rework/PLAN.md)、[README](../architecture/plans/framework-rework/README.md) 的 D-1、[framework 架構](../framework/architecture.md)。

## 做了什麼

- **Engine（AD4，D-1 的 b-lite，使用者已核准）**：`Engine::Asset::IsRelativeContentPath` 從 `ContentManifest.cpp` 的匿名命名空間移到 `ContentManifest.hpp` 公開，參數改成 `std::string_view`，行為不變。C++ 的內容路徑規則只剩這一份。
- **核心** `gyo/framework/CatalogPlan.hpp`（不帶 nlohmann）：條目型別直接用 Engine 的 `RawCatalogEntry`；`RegistrationPolicy`（id 換 type、路徑已屬另一個 id，預設都拒絕；兩者都不是內容契約的規則）；`CheckCatalogEntries`（單一 catalog 內 id 唯一、路徑合 Engine 規則且不是 content.json）；`PlanRegistration`（新增或更新、previousPath）。`CatalogUpdate` 從舊的 `CatalogRegistration.hpp` 移過來，舊標頭改 include 它，舊 API 行為不變。
- **JSON 綁定** `GYO::ToolFrameworkJson`（`framework/bindings/json/`，隨核心建，自己 PRIVATE 連 nlohmann）：`ReadCatalog`（是否為 catalog v1 交給 Engine 的 `CatalogParser::Parse`，再做核心的檢查）、`RegisterAsset(text, entry, policy)`（保留位元組的改寫從舊碼複製；寫完重新解析並比對全文，再讀一次確認仍合法）。
- `tests/framework` 在 configure 時確認 JSON 綁定有建。
- **文件**：framework 的組成清單與 Using it；`docs/architecture.md` 的 framework 列與內容路徑規則的一句。Design Invariants 不變（DI 4 的改寫照 PLAN 留到 v8-framework-invariants）。
- 舊的 `CatalogRegistration`、它的測試與突變不動。已知缺陷（更新條目時丟掉額外欄位）照現行行為搬，下一批修（D-6）。

## 驗證與經緯

件數の突き合わせ：一致（宣告 9・執行 9・killed 9、stale 0；另依 D184 重跑 fw-shell-binding-missing 1・1・1）。

- **cases.json 對齊**：以 `ContentManifest::Parse` 分類，適用 34 條（合法 4、不合法 30）、排除 20 條（多個 catalog、shader bundle、content 本身不合法），和實作前的宣告相同，測試斷言這三個件數。
- **Red→Green**：核心先用舊 framework 的路徑規則，C1 只有 `asset-path-control`（路徑含換行）不一致，C6 的含換行路徑沒被擋；換成 Engine 的規則後 C1～C7 全過。這是 D-1 要消掉的分歧。
- **突變**：拿掉控制字元檢查（Engine）、content.json 檢查、重複 id 檢查、不經 CatalogParser、改寫時動到其他條目、政策兩項被忽略、核心 include nlohmann（守衛）、JSON 綁定沒建（configure 失敗）。宣告前做了預檢（find 恰好一次；預期訊息對守衛的輸出格式、CMake 折行與不折行都成立）。
- D184：所有突變目錄預檢都沒有 stale；錨在這批改過的兩個 CMakeLists 的既有突變，錨點都不在改動的行。存在檢查前面多了 JSON 的檢查，重跑 fw-shell-binding-missing：killed。
- 全量 CTest 119/119，包含 `gyo_audio_editor.cli`（`convert_taken_path` 仍回 4）與 Engine 的 `AssetContentContractTests`。新 API 以預設政策拒絕 `convert_taken_path` 的情境（C5）。
- L2：`GYO_TOOLS` 為空（沒有 framework、沒有 SDL3 target）；只選 audio_editor（有 JSON 綁定，沒有 SDL3 target）。
- 範圍的限界：寫完後的全文比對在正確的改寫下殺不到，不設突變（舊碼相同）；跨 catalog 重複與 shader 重疊不在這批。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-framework-catalog-structure/`（git 不追蹤，見該資料夾的 README）。

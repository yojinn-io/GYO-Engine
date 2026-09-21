[日本語](README.md) · [English](README.en.md)

# GYO-Engine

GYO 是 C++20 遊戲引擎，明確區分 Runtime、Asset、Input、Collision、Model、Text、Render 與 UI。引擎程式庫靜態連結進遊戲與設計工具；遊戲擁有自己的規則與內容，引擎不依賴特定遊戲。

## 目錄與責任

| 目錄 | 責任 |
|---|---|
| `apps/` | 遊戲程式碼與最小專案宣告 |
| `assets/<game>/` | 準備完成的 runtime 內容、catalog 與 `content.json` |
| `engine/` | 所有引擎模組、adapter 與 `config/projects.csv` |
| `tools/` | 設計支援，目前是 UI editor |
| `tests/common`、`tests/<project>` | 共通引擎測試與專案專屬驗證 |
| `build/cmake`、`build/ci` | 編譯、整合、封裝及驗收支援 |
| `build/target/` | Git 忽略的建置樹、可執行產品與報告 |
| `docs/`、`third_party/` | 設計文件與固定版本的第三方套件 wrapper |

離線 shader compiler 歸 `engine/render/shaders/pipeline` 所有，根目錄 `tools/` 專供設計工具。請勿刪除整個 `build/`，其中包含需要版本管理的開發支援程式。

## 建置遊戲

使用 CMake 3.30 以上、C++20 compiler 與 Ninja。Windows 先初始化 x64 MSVC 開發環境。資產組裝使用 Python 3，shader 使用引擎管理的原生 host compiler。第三方來源下載至建置樹，也可明確重用既有 source cache。

在 `engine/config/projects.csv` 啟用遊戲與目標 OS，再從儲存庫根目錄執行：

```sh
cmake --preset dev -DGYO_APPS=object_fps -DGYO_BUILD_UI_EDITOR=OFF
cmake --build --preset dev --target gyo_object_fps
```

建置狀態位於 `build/target/_build/dev`，可執行遊戲組裝於 `build/target/object_fps/bin`。遊戲只讀取執行檔相對的 `assets/object_fps`；內建與遊戲 shader 也位於其中。組裝完成的產品不需要 source checkout 才能執行。

普通產品建置關閉測試與 CI/package acceptance，不依賴 tests、CI 程式或 editor。驗證 backend-neutral engine：

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

使用 `test` preset 驗證選中的遊戲。GPU 測試需要對應圖形環境，驗證結果與 CPU/CLI 分開記錄。歷史日誌不能作為新 revision 已通過的證據。

## 設計工具與資產

```sh
cmake --preset dev -DGYO_APPS= -DGYO_BUILD_UI_EDITOR=ON
cmake --build --preset dev --target gyo_ui_editor
```

GUI editor 組裝於 `build/target/toolchain/bin`，透過唯讀 catalog 編輯 `gyo.ui` JSON。作者手動準備、複製並登錄所選來源資產，之後共通 build hook 依 `content.json` 自動組裝資產與編譯 shader。本機與 CI 使用同一路徑，遊戲 CMake 不維護資產規則。

變異專案只需手動複製 `apps/<game>` 與 `assets/<game>`，再新增 CSV 列。遊戲不攜帶 CI、測試、editor 或原始美術封存。詳見[建立／複製遊戲](docs/creating_apps.md)。

## 整合與發佈

Registry 決定遊戲集合，不從目錄數量推測。任何選中遊戲失敗都會讓 Engine 整合失敗。每個支援平台固定產生含 GUI UI editor 與其靜態連結引擎的 toolchain archive，再為 CSV 啟用遊戲產生獨立 archive。零遊戲仍可成功發佈。配布物是可執行產品與必要依賴，沒有 Engine SDK 或原始碼套件。

`Prepare Release` 固定 commit，建置並驗證完整產品集合，再準備 tag 與 Draft Release。公開 Draft 是使用者的獨立操作。遊戲包只包含遊戲、資產與 runtime 依賴；驗收工具從產品外部執行。

## 設計資料

- [整體架構與責任邊界](docs/architecture.md)
- [UI 標準與設計流程](docs/ui_toolchain.md)
- [渲染與 shader 契約](docs/rendering_architecture.zh-Hant.md)
- [3D 資產與動畫限制](docs/architecture/3d-assets.md)
- [發佈程序](docs/releasing.zh-Hant.md)

`Object_FPS` 是可移除的使用端，驗證 campaign、UI、grid combat、CPU skinning 第一人稱武器及 SDL_GPU；它不定義通用遊戲 framework。目前不包含一般 scene 管理、完整物理、GPU skinning、PBR、scripting 或動態 plugin ABI。

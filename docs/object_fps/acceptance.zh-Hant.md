# Object_FPS：外部產品與 GPU 驗收

[日本語](acceptance.ja.md) · [渲染設計](../rendering_architecture.zh-Hant.md) · [發佈程序](../releasing.zh-Hant.md)

## 1. 遊戲與驗收分離

`apps/object_fps` 只保存遊戲程式、metadata 與 source declarations。遊戲單獨建置時不需要測試、CI、packaging 或設計工具。`tests/object_fps` 擁有 unit tests，`build/acceptance/object_fps` 擁有獨立 `gyo_object_fps_acceptance`；產品 executable 不注入 diagnostics source 或 test macro。互動 viewmodel 設計預覽由 `tools/object_fps_preview` 的 `gyo_object_fps_preview` 提供，不是遊戲啟動模式。

共通驗收 runner 在 `build/acceptance/common`，Object_FPS 的 GPU／內容檢查在 `build/acceptance/object_fps`。正式 game archive 不含這些程式、Python scripts 或 probe。

先在 `engine/config/projects.csv` 啟用遊戲與目標平台，再明確建立驗證配置：

```sh
cmake --preset test -DGYO_APPS=object_fps -DGYO_TOOLS= -DGYO_ENABLE_PACKAGING=ON
cmake --build --preset test
ctest --preset test
cmake --install build/target/_build/test --prefix build/target/acceptance-stage/object_fps --component object_fps
```

產品組裝於 `build/target/object_fps`，probe 在 `build/target/_build/test/acceptance/object_fps/bin`。遊戲資產全部位於 `bin/assets/object_fps`，其中包含 catalog、資料、模型、字型、`shaders/builtin`。Source manifest 的 build-only shader source 欄位不進 runtime descriptor，缺檔不能回退至 source tree。

產品 manifest 是 `share/gyo/products/object_fps/manifest.json`。外部 runner 讀取 identity、executables 與 declared checks，不猜測原始遊戲名稱。複製成其他遊戲時，deployment identity 改變，內部 AssetId 或 shader ID 可以保持不變；參見[建立／複製指南](../creating_apps.md#manual-copy)。

## 2. 執行驗收

以下範例在 Windows 使用上一節 install 產生的獨立驗收產品，包含 generated product manifest。將 `FULL_COMMIT_SHA` 換成本次完整 commit；其他平台換用對應 platform 名稱。

```sh
python build/acceptance/common/run_package_checks.py --stage build/target/acceptance-stage/object_fps --product object_fps --platform windows-x64 --revision FULL_COMMIT_SHA --profile release --logs build/target/acceptance/object_fps --context build/target/_build/test/packages/object_fps/RelWithDebInfo/acceptance-context.json
```

Runner 在外部工作目錄驗證產品，必要時建立隔離副本並把 probe 暫時放在副本的 bin，使它使用與遊戲相同的資產路徑。失敗、逾時或缺少必要證據都使驗收失敗。Probe 不會寫進正式 archive。

| 檢查 | 範圍 |
|---|---|
| Startup／內容載入 | Catalog、campaign、模型與 compiled shader，不需要 GPU |
| Headless gameplay | 選單、開始、移動／跳躍、暫停、射擊及換彈狀態 |
| Package negative cases | 故意移走必要資產／shader，要求明確失敗，不能讀回 checkout |
| GPU quick | 宣告的平台執行 viewmodel 渲染／讀回 |
| GPU release／manual | 世界、選單、武器、換彈和多比例槍口診斷 |

舊的 `--startup-smoke-test`、`--headless-smoke-test`、`--validate-package` 與 GPU probe switches 現在屬於 acceptance executable，不能傳给遊戲產品。正常遊戲只保留一般操作、`--help` 與 `--gpu-driver`。

## 3. GPU 與實機

目前宣告的 GPU release check 對象是 Linux；先執行同一 profile 的非 GPU 檢查，再以相同 logs 加 `--gpu --driver vulkan`。Common runner 僅執行 contract 宣告的平台與 profile，不能將空的 GPU check 集合視為實機通過。

Windows／macOS 的手動 GPU 驗收使用 `build/acceptance/object_fps/manual_gpu_smoke.py`。先複製已 install 的產品至獨立驗收目錄，將對應 probe 及其必要 runtime libraries 放入副本的 bin，再傳 `--package` 與 `--probe`；Windows 分別選 `--driver d3d12`／`vulkan`，macOS 選 `metal`。`--suite quick` 執行 viewmodel，`--suite ci` 執行 viewmodel、world、menu；`--suite full` 執行七項診斷，`--output` 指定報告目錄。不要把 probe 複製回正式產品或封存檔。

Linux CI 的 Xvfb／Lavapipe 驗證軟體 Vulkan。它不代表實體 GPU；Windows/macOS hosted 的編譯／CPU 成功也不能作為實機畫面正確的證據。任何要求的 GPU 能力缺失、shader 格式或 driver 不符、逾時、未產生必要圖像，都必須報告失敗。

手動驗證滑鼠／鍵盤、Space 跳躍、R 換彈、H 收槍／拔槍、手與槍身遮擋、貼牆射擊、曝光與 HUD。記錄 OS、GPU／driver、product revision、要求及實際 renderer，並保存驗收 summary／圖像。

Engine 的獨立 GPU 測試位於 `tests/common`，驗證 frame lifecycle、mesh 更新、UV、深度／剔除、色彩／alpha、矩陣、後處理及自訂 channel-swap shader 的紅／藍像素讀回。Shader 樣本由共通測試擁有，不放入遊戲 archive。歷史驗證表仍保留在[渲染文件](../rendering_architecture.zh-Hant.md#r10)，其結果不代表目前 revision 已通過。

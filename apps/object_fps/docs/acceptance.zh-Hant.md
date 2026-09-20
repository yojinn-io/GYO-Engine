# Object_FPS：套件與 GPU 驗收

[日本語](acceptance.ja.md) · [共用渲染設計](../../../docs/rendering_architecture.zh-Hant.md) · [版本發佈](../../../docs/releasing.zh-Hant.md)

## 1. App 自有契約

Object_FPS 的 `CMakeLists.txt` 宣告 SDL_GPU、input、image、ttf 與 ufbx 需求，並註冊自己的資產、shader、安裝與驗收命令。先在 Engine 的 `config/engine/projects.csv` 啟用此 app 及目標平台；共用 CI 只讀生成的 package manifest，不知道遊戲名稱或玩法。

```sh
cmake --preset dev -DGYO_APPS=object_fps -DGYO_BUILD_UI_EDITOR=OFF
cmake --build --preset dev
ctest --preset dev
cmake --install build/dev --prefix /absolute/path/to/stage
```

套件包含 `bin/gyo_object_fps`、`bin/assets/common`、`bin/assets/object_fps`、`bin/shaders/builtin`、`bin/shaders/object_fps`、必要動態庫、生成的 `share/gyo/apps/object_fps/manifest.json`，以及本 app 的驗收工具。UI editor 不在套件中。Archive 使用 `gyo-object_fps-<platform>.tar.gz`，根目錄為 `gyo-object_fps`。

`--validate-package` 不建立視窗或 GPU，驗證安裝資產、shader bundle 與讀入流程。Release 的 app 自有驗收從獨立位置執行完整套件，依序移走 common 資產、內建 shader manifest、遊戲 shader manifest，要求每項缺檔正確失敗；不能回到 source tree 補檔。遊戲需要 GPU 的建置宣告，因此 `GYO_RENDER_DEVICE=NONE` 會在配置時拒絕；headless 檢查是執行模式，並非缺少編譯依賴的替代方式。

## 2. Quick、Release 與實機邊界

| 檢查 | 平台 | 實際邊界 |
|---|---|---|
| `--startup-smoke-test` | Windows／Linux／macOS，快速與 Release | 從其他工作目錄啟動部署版，實際載入 catalog／campaign／FBX 模型組裝；不建立視窗或 GPU |
| `manual_gpu_smoke.py --suite quick` | Linux，快速 CI | Xvfb 提供顯示環境，強制 Mesa Lavapipe Vulkan；執行一次自訂 shader 紅／藍渲染與讀回 |
| `--headless-smoke-test` | Windows／Linux／macOS，僅 Release | 另檢查選單、開始、拔槍、跳躍、暫停／恢復、落地、單次射擊與換彈完成；不建立視窗或 GPU |
| `manual_gpu_smoke.py --suite full` | Linux，僅 Release | shader、世界、選單、武器、換彈與三種比例的槍口診斷，共八項 |
| 實體 GPU／互動驗收 | 各平台實機 | 完整八項 GPU 診斷及手動操作；Windows／macOS hosted runner 不宣稱已通過此項 |

Linux 缺少 Lavapipe、無法建立 device、渲染失敗或逾時都讓 job 失敗；沒有「找不到 GPU 就跳過並回報成功」的路徑。Helper 也要求日誌中的實際 driver／shader 格式符合請求，將結果寫入 `summary.json`。`vulkan-info.log` 記錄 ICD 與 Vulkan 裝置資訊。這證明軟體 Vulkan 渲染路徑可以運作，實體顯示卡仍需驗收。

共用驗證流程匯總三平台 matrix，任何必要 job 失敗、取消或跳過都不能進入建立 Draft 的階段。快速路徑依設計不執行 Release 專屬重型步驟，Summary 明列模式與未執行項目，不將它們視為完整驗收。若設定 branch protection，請使用 quick 流程實際顯示的彙總檢查名稱。

## 3. 下載後的實機驗收

從 Actions artifacts 或 Release 下載對應套件，解開 tar.gz，在有圖形桌面的目標機器執行：

```sh
python manual_gpu_smoke.py --package /absolute/path/to/gyo-object_fps \
  --driver vulkan --suite full --output /absolute/path/to/diagnostics
```

macOS 使用 `--driver metal`，Windows 分別測 `--driver d3d12` 和 `--driver vulkan`。Python helper 逐項執行自訂 shader 紅／藍色讀回、世界、選單、武器、完整換彈，以及 16:9／4:3／21:9 槍口投影 smoke，保留各項 exit code、日誌和診斷影像；每項最多 120 秒。不具有 Python 的電腦也能直接執行 `bin/gyo_object_fps --gpu-driver metal --muzzle-smoke-test --capture-dir /absolute/path/to/captures` 等對應指令。

`--suite full` 為預設的完整八項；`--suite quick` 只渲染一項 shader；`--suite ci` 保留為手動選擇 shader、world、menu 三項的工具。`--timeout` 調整各項秒數上限。任一項失敗使 helper 退出碼非零，其餘案例仍執行並保留結果。

若受限環境無法使用 Python 臨時目錄，可替 GPU helper 加上 `--work-directory /absolute/path/to/new-work`。該目錄必須尚不存在，驗收後會保留供檢查；工作目錄須放在 `--package` 之外。

另外手動檢查：滑鼠／鍵盤、Space 跳躍、R 換彈、H 收槍／拔槍、手指與槍身遮擋、貼牆射擊、曝光與 HUD、視窗縮放／最小化。UI editor 另在工具建置中驗收。回報 OS、CPU 架構、GPU／驅動、套件 commit、指定／實際後端與 `summary.json`，才能區分建置、資料和 GPU 問題。

若在目標機器從原始碼建置，可用 `ctest --test-dir build/dev -L gpu --output-on-failure` 跑完整 engine＋game GPU 集合。獨立的 `render.sdl_gpu_mesh_smoke` 另以讀回數值檢查 UV／子矩形、深度與剔除、sRGB／線性色彩、alpha、非對稱矩陣，以及 13×7 後處理；此測試 executable 不包含在 Object_FPS 下載套件中。

原始碼中的 helper 位於 `apps/object_fps/ci/manual_gpu_smoke.py`；安裝包根目錄保留 `manual_gpu_smoke.py`，可將上述命令中的 script 改成其絕對路徑。發行前的實機結果與 CI 軟體 Vulkan 證據分開記錄。過去結果保留在[歷史驗證](../../../docs/rendering_architecture.zh-Hant.md#r10)，不代表新 registry／matrix 已在 hosted CI 通過。


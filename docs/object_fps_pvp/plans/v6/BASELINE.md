# v6 第 01 批：基線

日期：2026-10-04。Owner：`object_fps_pvp`。這是 v6 開發的起點，**不是 v6 驗收**。
現行 wire 與玩法仍是 v5（[v5 契約](../../protocol-v5.zh-Hant.md)、[v5 穩定基線](../v5/STABLE_BASELINE.md)）。
v6 的政策以 [v6 契約](../../protocol-v6.zh-Hant.md) 為單一入口（目前是骨架）。

## 指紋與證據範圍

- 基準 commit：master `05042fa`（PR #38 合併後），工作樹乾淨。
- 環境：macOS 26.7.1（25G241），x86_64（Intel），Apple clang 21.0.0（clang-2100.1.1.101），test preset（RelWithDebInfo）。
- 本批的證據放在 git 忽略的 `build/target/_build/test/logs/pvp-v6-batch01-20261004/`：
  - `head.txt`：基準 commit。
  - `build.log`：`cmake --preset test` 與建置的完整輸出。
  - `ctest.log`、`ctest.xml`：全部 CTest。
  - `v5-evidence.sha256`：複製過來的 v5 原始證據的逐檔雜湊（見下節）。

提交的 Markdown 不保存 build 內的證據；需要時請另行保留。

| 產物 | SHA-256 |
|---|---|
| Client `build/target/object_fps_pvp/bin/gyo_object_fps_pvp` | `973147f6da4c26bf3b1c7fe9bc9322c7bfb895f0614ae7cd738704b55450c39b` |
| Match `build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match` | `eeb2e9ddcab6a5ddcf13174e375b506a7af5e6d7a993478b25d95e7fff3bd247` |
| Gateway `build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway` | `32fcbd3cfadd4d67835927959f88178acc4545ad5309c757196b7299c39e3252` |
| action probe | `a58faecb4199753265dca8e0575bd243fd38f701ec2b1878314eb8c9b7a72d44` |
| GUI probe | `d157f6522a59bd0b58cae7d83acd552dfed134c5af4b9322f8c98796c6ad0f87` |
| worker probe | `a7e61436626fa3d5fecc2aeefd1b34d809940bc3c7167b156b0920fb48b0647e` |
| timing probe | `263b9365db9badda7039435d1a40eea6654cbba85a451a089b164cdea18b9ec5` |
| network probe | `f9e676f124c318855e4219f1d50f663f8814850ddac0207426653a809f7f8808` |
| 部署 `asset_catalog.json` | `4d2667f1e9d56ecd8e23aaddeabd292c9d732140917491c5b1eba08b71b42672` |
| 部署 `pvp_arena.json` | `6f716a4efa68a173d26d916ccfd072fa735ee2c69453c63a68575440581b16ce`（與 v5 第01批相同） |

probe 位於 `build/target/_build/test/acceptance/object_fps_pvp/`。
Gateway 的建置時間早於本次建置（19:27），本次的 test preset 建置沒有重建它。

## 短回歸

在 repository root 執行。cmake、ninja 以絕對路徑呼叫，因為 Claude 的 shell 不會載入 `~/.zshrc`：

```bash
cmake --preset test
cmake --build build/target/_build/test --parallel 4
ctest --test-dir build/target/_build/test --output-on-failure \
  --output-junit logs/pvp-v6-batch01-20261004/ctest.xml
```

結果：

- 建置成功：59 個 Ninja 步驟。
- CTest 54／54 通過，實際耗時 97.64 秒。

| 標籤 | 測試數 | 合計時間（秒） |
|---|---|---|
| cpu | 50 | 84.29 |
| build | 11 | 41.39 |
| pvp | 19 | 41.28 |
| acceptance | 15 | 22.65 |
| gameplay | 1 | 8.39 |
| go | 2 | 8.00 |
| presentation | 2 | 5.77 |
| network | 1 | 4.16 |
| editor | 6 | 1.56 |
| gpu | 1 | 1.16 |

這只是現有 domain、分析器、建置與共通測試的回歸。不是 GPU、GUI、真網路矩陣或長測的認證；沒有啟動這些項目。

## v5 原始證據（使用者決定 D9：複製需要的目錄並記錄雜湊）

v5 的原始證據原本只存在另一個 worktree，`/Users/karasu/Code/Source/GYO-Engine-v5`（分支 `claude/pvp-v5-stable-baseline`）的 `build/target/_build/test/logs/`。
第 02c 批的重新分析與第 04 批的重現都需要它。本批用 APFS clone（`cp -c`）把需要的目錄複製到本 worktree 的同一相對路徑，所以 v5 文件中的證據路徑在這裡同樣有效。

- 雜湊清單：`build/target/_build/test/logs/pvp-v6-batch01-20261004/v5-evidence.sha256`，共 1,325 個檔案。
- 清單本身的 SHA-256：`8329023b969c64309caaede8831fea6b7cdd8da303512a7c40b081346c5364d3`。
- 以 `shasum -a 256 -c` 對 v5 worktree 的原檔逐一核對：1,325／1,325 相同。

| 目錄 | 檔案數 | 大小 | 用途 |
|---|---|---|---|
| `pvp-v5-acceptance-20261003` | 154 | 4.5 GB | v5 完整驗收：GUI 三輪、60／144 Hz 長測（第 02c 批重新分析） |
| `pvp-v5-batch03-closure-20261002` | 616 | 345 MB | 第 03 批結案驗收（GUI 三輪、25 案矩陣） |
| `pvp-v5-batch04-20261002` | 12 | 756 KB | 第 04 批 L1／L2 |
| `pvp-v5-batch04-20261002-manual` | 5 | 20 KB | 第 04 批 L3：拖動視窗時的 1134 ms render（第 3 項） |
| `pvp-v5-batch04-20261002-manual-2` | 5 | 20 KB | 第 04 批 L3：render 約 1199 ms 的停頓（第 3 項） |
| `pvp-v5-batch04-20261002-run2` | 61 | 22 MB | 第 04 批重跑 |
| `pvp-v5-batch04-5-player-1` | 37 | 12 MB | 人物短測（04-5）：30／60 FPS 通過；144 FPS 案只到約 120 FPS 而失敗（第 8 項） |
| `pvp-v5-batch04-5-player-2` | 24 | 32 MB | 人物 GPU capture 的第一輪（因實體滑鼠移動而失敗，保留） |
| `pvp-v5-batch04-5-gait-manual` | 5 | 20 KB | 步態人工確認 |
| `pvp-v5-batch05-3-20261003` | 406 | 245 MB | 第 05 批整合短測與 25 案矩陣 |

其餘 v5 開發跑次沒有複製，仍只在 v5 worktree。

## wire 版本號寫死處（第 09 批的檢查表）

v5 是單一 commit 同時升級三角色（`de87bb9`）。v6 也是這樣做（[第 09 批](09-protocol-v6.md)）。以下是基準 commit 上寫死 v5 的位置：

- GYOP 標頭：`apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp:26`（Encode 寫入 5）、`:36`（Decode 要求 5）。
  `HeaderSize = 24`（`:11`）。
- 產品 C++ 的 IPC 與 join：
  - `apps/object_fps_pvp/src/Pvp/IpcHost.cpp:60,86,132,156,196,206`。
  - `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:270`（join request）、`:275`（join reply 檢查）。
- Go：`apps/object_fps_pvp/gateway/adapter/adapter.go:15-16`（`ClientVersion`、`RuntimeVersion`）已經是單一常數，Gateway 其餘地方都用它；測試在 `adapter/v5_test.go`。
- 驗收端 C++：
  - `build/acceptance/object_fps_pvp/worker_main.cpp:40,120,377`（`:374` 刻意用 4，測試拒絕舊版本）。
  - `action_short.hpp:97`、`combat_latency.hpp:194`、`gameplay_action.hpp:81,106,161`（`:161` 同時寫入 `gameplay_v5`）。
- 驗收端 Python：
  - `combat_gui_evidence.py:120,154`。
  - `gameplay_evidence.py:139-140`。
  - `gameplay_soak_evidence.py:177,180`。
  - `test_combat_gui_evidence.py:102`。
- 檔名與套件名稱：
  - `apps/object_fps_pvp/protocol/client_v5.proto`、`runtime_v5.proto`。
  - Go 套件 `protocol/clientv5/`、`protocol/runtimev5/`。
  - 產生的 `client_v5.pb.h`（`worker_main.cpp:5`、`network_main.cpp:4`）。
  - CMake 的 `foreach(contract client_v5 runtime_v5)`（`apps/object_fps_pvp/CMakeLists.txt:16`）與連結（`:37,40`、`build/acceptance/object_fps_pvp/CMakeLists.txt:6,22`）。

第 02a 批會先把驗收端收成單一常數（值仍為 5），第 09 批再改值，並加入跨語言一致性測試。

## CI 的實際涵蓋

- 四平台：windows-x64、linux-x64、macos-arm64（原生）、macos-x64（Rosetta 2），見 `build/ci/common/app_registry.py:28-36`。
- GPU 測試只在 Linux 以 Mesa lavapipe（Vulkan）執行 `gpu` 標籤的 `render.*`（`.github/workflows/build-and-validate.yml:219-220`）。
- macOS 只做 Metal 的離線編譯，沒有執行 GPU；Windows 的 D3D12 完全沒有執行。

所以 v6 的實機驗收（L2／L3）只有 macOS Intel／Metal；其餘平台在各批的平台表中標「未執行」（使用者決定 D11⑩）。

## 權威決定性（第 03 批補上）

- 閘門：`tests/object_fps_pvp/AuthorityDigest.cpp`（digest 版本 1，35 個情境，其中 30 個屬 golden 子集），CTest `object_fps_pvp.authority_digest`。
- 正式證明：`tests/object_fps_pvp/compare_authority_trees.py`（同機兩樹比對）。
- `05042fa` 的 digest：第 01～03 批都沒有改權威相關來源（`git diff 05042fa -- apps/object_fps_pvp/src apps/object_fps_pvp/include engine/` 為空），
  所以 2026-10-05 的 runner 輸出就是 `05042fa` 的 digest。macOS Intel、Apple clang 21.0.0、RelWithDebInfo：

| 規模 | 紀錄數 | 輸出檔 SHA-256 |
|---|---|---|
| 1 | 17,226 | `34d0ca7f84cb778725aae61ed199513973b30f554525ed090a18e0d0c85d52c6` |
| 10 | 171,927 | `f9e0ef413c5ede6d24efc962ece63280a5dfd6de11edc5a2025bb7d6292393db` |

- 輸出檔（含每個情境的 digest）在 `build/target/_build/test/logs/pvp-v6-batch03-digest-20261005/`。
- golden 子集的 digest 已提交在 `tests/object_fps_pvp/fixtures/authority_golden.txt`；-O0 與最佳化建置在規模 1、10 都相同。

## 第 02 批凍結的驗收工具（供第 04 批使用）

- 2026-10-04 凍結：`build/acceptance/object_fps_pvp` 的 52 個檔案。
- 雜湊清單：`build/target/_build/test/logs/pvp-v6-batch02-reanalysis-20261004/frozen-tools.sha256`。
- 清單本身的 SHA-256：`b5f4bf79d1a312e860a69b8c8038aa535ad5f5ac16a91dbea0dad2267ff59b77`。
- 第 04 批量測前要先核對工具與這份清單相同；不同就先查明差異，不得直接量測。

## 量測基線 B0（第 04 批，2026-10-05）

v5 產品×第 02 批的凍結工具。IP-2 合併後由第 07 批重取 B1；之後各批在自己的 base commit 上量 before／after，B0 只作歷史參照。經過見 [第 04 批 dev_log](../../../dev_logs/2026_10_05_pvp_v6_batch04.zh-Hant.md)。

### 來源與指紋

- 來源：master `6381e9d`（第 01～03 批合併後），獨立 worktree（detached）以 test preset 從零建置。沒有合併任何 Engine 批次（IP-*、FF-*）；產品執行期來源與 `05042fa` 相同。
- 環境：macOS 26.7.1（25G241），x86_64（Intel），Apple clang 21.0.0，Metal。
- 凍結工具：52／52 與第 02 批清單（`b5f4bf79…`）相同。
- 權威 digest：規模 1、10 與 `05042fa` 逐位元相同（`34d0ca7f…`、`f9e0ef41…`）。
- CTest（只確認建置）：55／55，101.92 秒。
- 證據：`build/target/_build/test/logs/pvp-v6-batch04-20261005/`（git 忽略）。

| 產物 | SHA-256 |
|---|---|
| Client | `d738a01902bdb1d04421005b7ce6491a49aa5a51536c5a2df76dc5a98197fd06` |
| Match | `2b78275dd774f599b214af6237da1eeaa2e1526b828520e62ec1c73da9e338fc` |
| Gateway | `1f4ca6bfaa74f5a92814e204e812abfd742a98ae85df0668aa3526d410560cad` |
| GUI probe | `ebdaf903319e91bfc6ae6de4f14cabcc142a43a23dae74bacb727a5ad8bb0744` |
| action probe | `f8d3590a7025df067ded50c0c7c518eda8da95b18b03e7bb20069b755b87c87c` |
| `asset_catalog.json`、`pvp_arena.json` | 與第 01 批相同 |

執行檔雜湊與第 01 批不同，因為建置路徑不同（新 worktree）。

### 平台指紋：計時器分布（只供解讀）

| probe | 睡法 | 晚醒 P50／P99 | 間隔 >18 ms 比例 |
|---|---|---|---|
| GUI probe（動作、人物短測） | `SDL_DelayNS` 餘量，frame_relative | 約 0.6／1.1 ms | 0 |
| headless action probe（矩陣） | `std::this_thread::sleep_until`，absolute | 2.3～3.0／3.8～4.2 ms | 9～22% |

### 短測

| 跑次 | 結果 | 主要數字 |
|---|---|---|
| 動作短測 | 4／4 通過 | — |
| 人物短測 | player30、player60、capture 通過；player144 `invalid_capacity`（未驗證） | 達成 FPS：29.4／29.5、57.5／57.6；144 案 create 137.1、join 120.01 |
| 雙 GUI 整合短測 | 通過（視窗閘門 clean） | 移動 Actual 1.0，P50 30.1 ms、P95 37.3 ms；首次送出 P95 2.4 ms；交越 20／20，P50 37.7 ms、P95 39.8 ms；2 死亡、2 重生 |
| 25 案矩陣（第 1 次） | **失敗**，停在 clean-30（2／25 執行） | probe 一幀 57.7 ms 造成丟時；保留，推測主機干擾 |
| 25 案矩陣（重跑） | 25／25 通過 | clean-60／30／144：Match 裁決 P95 11.9／11.1／20.1 ms；Client 取得裁決 P95 100.5／102.6／110.8 ms |

### 缺陷重現

- 第 3 項：**重現**。拖動標題列時 `render_ms` 約 1197 ms（3 次中 2 次）。縮放時停頓在事件處理（0.4／3.0／1.0 秒），不在 render。Engine 側摘要寫在 [輸入與呈現交接](../../../architecture/plans/input-and-present/HANDOFF.md)。
- 第 6 項：**未重現**。probe 路徑兩次死亡共 18 張截圖都沒有第一人稱手臂；使用者看過自動測試畫面後決定不再以正式 Client 重現。第 05 批依停止條件不執行。

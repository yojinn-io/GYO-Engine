# 第 02 批：驗收工具可信化

狀態：未開始。先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與 [第01批](01-plan-and-baseline.md)。
依賴：第01批。對應 HANDOFF 第5、7項與第8項的 pvp 列；第8項的共通列在 Engine 計畫
[foundation-followups](../../../architecture/plans/foundation-followups/README.md) 的 FF-2。

本批只改 pvp 自有的驗收工具與測試，讓後續每一批的 L1／L2 與基線比較可信。
**不改產品行為、不改 wire、不改權威結果、不改對玩家的門檻。**

## 子批與提交方式

分三個子批，依序進行，各自 commit 並各自重新分析；是否分成三個 PR 由使用者在開始時決定。

| 子批 | 內容 | 對應項目 |
|---|---|---|
| 02a | 舊驗收模式移除＋驗收端版本常數 | 第5項 |
| 02b | 平台指紋加計時器晚醒分布 | 第7項 |
| 02c | pvp 計時判定改為結構條件 | 第8項 pvp 列 |

順序理由：02c 的時間常數解讀要用 02b 的分布；02a 先刪掉舊模式，02c 才不必改寫將被刪除的檔案。
正式重新分析一次（三個子批完成、工具凍結後）；各子批另外以自己的 commit 對同一批 v5 原始資料跑歸因用的分析，
分類改變時能歸因到單一子批。歸因用的分析不取代正式重新分析。

## 02a：舊驗收模式移除與版本常數

### 做

1. **先把第6項需要的欄位移進存續的 probe**，再刪任何東西。
   - `weapon_short.hpp:42` 是目前唯一逐幀輸出本機第一人稱 `submitted_meshes` 的地方。
   - 把 `weapon.submittedMeshes` 與逐幀 Presented／Skipped 加進 `action_short.hpp` 的 `LocalActionSample`（:39-46）
     或 `player_short.hpp`；選哪一個在開始時寫明理由。
   - 加突變測試：本機 Dead 且某個 Presented 幀 `submitted_meshes>0` 時，分析器必須判失敗。
   - 限制：產品在 `!Alive()` 時直接把此欄位設為 0（`PvpApplication.cpp:516`），所以它只能抓到
     「`Alive()` 仍為真時提交了手臂」的情形，抓不到不經 `SubmitWeapon` 的殘留畫面。第04批的重現另需截圖。
2. **涵蓋對照表**：逐條列出被刪除模式的每個斷言，由 `run_action_short.py` 或 `run_gameplay_soak.py` 的哪一條取代；
   依 v5 契約 §7 不得漏驗新行為。沒有對應者不刪，先停下回報。
3. **刪除**（附對照表後）：
   - `weapon_short.hpp`、`run_weapon_short.py`（v4「HP=0 仍可操作」語意）。
   - `run_action_legal.py`、probe 的 `--legal-shots`（`action_main.cpp:89-195`）、
     `action_evidence.analyze_legal` 及其測試（全部接受、無限彈藥的舊語意）。
   - `run_native_window.py` 的 HP 歸零段（:428-437，斷言「HP zero blocked movement」）。該 runner 只支援 Linux X11。
4. **刪除前先處理的依賴**（盤點自目前的 master，開始時重新確認）：
   - `weapon_short.hpp` 另外提供共用 helper：`WeaponJson`、`PushMouse`、`PushWindowEvent`、`PushMotion`。
     `action_short.hpp`、`player_short.hpp`、`combat_latency.hpp` 都在用；`native_window.hpp` 也用 `WeaponJson`
     （:30,33,59,68,75,115，由 `gui_main.cpp:238` include）。`gui_main.cpp:235` include 它，
     `:1059` 分派 `RunWeaponShort`。這些 helper 移到存續的 pvp 驗收檔，不跟著刪。
   - `run_weapon_short.py` 的 `digest` 被 `run_player_short.py:13` 與 `run_action_short.py:14` import，
     `rank` 被 `player_presentation_evidence.py:7` import，`gate_latency_case` 由 `test_run_timing.py:13,474-488` 測試。
     存續的函式先搬走，測試跟著搬。
   - `action_probe.py` 也用到舊模式：`:27` import `analyze_legal`，`:407`、`:483`、`:496-497`、`:555` 依 `legal_shots` 分支。
   - `test_service_startup.py:22-24` 的 `RUNNERS` 列有被刪的 runner。
5. **驗收端版本號收成單一常數**（值仍為 5，不升版）：
   - C++：`worker_main.cpp:40,120,377`、`action_short.hpp:97`、`combat_latency.hpp:194`、`gameplay_action.hpp:81,106,161`。
     `worker_main.cpp:374` 故意用 4 測舊版拒絕，改為「常數減一」或保留字面並註明，不得變成跟著常數走的同值。
   - Python：`combat_gui_evidence.py:120,154`、`gameplay_evidence.py:139-140`、`gameplay_soak_evidence.py:177,180`、
     `test_combat_gui_evidence.py:102`。
   - 規格未列、但目前也寫死 5 的位置（開始時確認後一併收斂）：`run_gameplay.py:40,49`、`gameplay_evidence.py:336`、
     `gameplay_soak_evidence.py:380`、`run_gameplay_gui.py:19,21`，以及 `backpressure_probe.py:84` 比對的 IPC 位元組 `b'\x08\x05'`。
   - C++ 與 Python 各有一處定義；兩者一致的跨語言測試在第09批建立，本批先讓各語言內只剩一處。

### 不做

- 不改產品的 `WeaponFeedbackObservation` 或 `PvpApplication`；只讀既有欄位。
- 不把 `run_native_window.py` 擴充到 macOS／Windows。
- 不升協議版本（第09批）。

## 02b：計時器晚醒分布

### 做

- 平台指紋加一段數秒的空 60 Hz 迴圈，記錄晚醒分布：P50、P99、超過 18 ms 的比例。
- headless probe 與 GUI probe 都記錄，寫進報告。
- 只供解讀，不參與 PASS／FAIL。
- 不依 OS 名稱分支。

### 不做

- 不以這組分布調整任何門檻或分母。
- 不改 `platform_fingerprint.hpp` 既有欄位的意義。

## 02c：pvp 計時判定改為結構條件

### 做

把下列判定改為以送出端序號、Tick、幀計數或事件順序判斷。非用時間常數不可時，以 02b 的分布解讀，只寫在報告。

| 位置 | 現況 |
|---|---|
| `player_presentation_evidence.py:101-103` | 達成 FPS 未達名義×0.85 判失敗 |
| `player_presentation_evidence.py:122-139` | 腳本時窗切出步態區段 |
| `worker_main.cpp:336-352` | 重送次數 55–65、相鄰重送 ≥14 ms |
| `worker_main.cpp:219,340,366` | 接收端 Pump 間隔 |
| `worker_main.cpp:230-234`、`action_probe.py:402-412`、`gameplay_evidence.py:202-206` | 以 relay 接收時間做一秒滑動窗 |
| `action_short.hpp:150-158` | 射擊間隔 0.25 秒、換彈 200 ms 餘裕 |
| `gui_main.cpp:657-662`、`:932`、`:1004`、`:571-686` | 1 ms 容差與幀時間假設 |
| `presentation_evidence.py:16,130` | `MINIMUM_DELAY_SECONDS = -0.02` |
| `run_timing.py:69-105` | 視窗證據的時間判定 |
| `apps/object_fps_pvp/gateway/backpressure_test.go:102-158` | ticker 節奏 |
| 分析器說明與報告欄位 | 「≥100 ms 停頓」約 83 ms 即觸發，寫明 |

- 每一處附兩種測試：
  - **舊條件誤判情境**：例如注入 3–8 ms 晚醒、relay 停頓擠包；舊版失敗、新版通過。
  - **真缺陷突變**：真的超送、真的掉命令、真的 83 ms 停頓；新版仍失敗。
- 以突變測試證明 02b 的分布欄位不影響判定。
- **FPS 不足（D11③）**：人物短測達成 FPS 未達名義×0.85 判 invalid，不判 fail：
  - 不計入宣告的跑次。
  - 報告列出達成 FPS 與計時器分布。
  - 同一跑次的延遲門檻照判。
  - 有效輪不足時該項標「未驗證」，不算通過。
- **對玩家的門檻一字不改**：50／66.7／80／100／150 ms、≥99%、1.5 秒、窗口與包率上限。

### 不做

- 共通列（`MeshUpdateSmoke`、package checks、`AssetWatcher`）屬 FF-2。
- `tests/object_fps/package_tools/test_gpu_smoke.py`（停用產品）只寫進 FF 的遷移清單。
- 產品行為、量測基線（第04批）。

## 工具凍結後的重新分析

三個子批完成後凍結工具（記錄分析器檔案雜湊），用新分析器重新分析第01批已複製的 v5 原始資料（位置與雜湊見 [基線](BASELINE.md)）：

- `pvp-v5-acceptance-20261003` 的 GUI 三輪（`gui-round-1`～`3`）與 `soak60`／`soak144`。
- `pvp-v5-batch04-5-player-1/player144`：名義 144 FPS、達成約 120 FPS 的失敗輪。

先寫事前宣告，再跑分析：

- 原本通過的跑次仍通過。
- 原本失敗的跑次，新分類逐一事前宣告。依 D11③，`player144` 預期由 fail 改為 invalid（不計次），同跑次的延遲門檻照判。
- 原始資料唯讀，不覆寫；輸出放新的證據目錄。

產出判定對照表：跑次、舊判定、新判定、事前宣告、是否一致。

## 驗收點

- **L1（自動）**：
  - `ctest -L pvp`、Python 分析器單元測試、Go `go test` 與 `-race` 通過。
  - 02a：Dead 且 Presented 幀 `submitted_meshes>0` 的突變判失敗；被刪模式涵蓋的項目，在新短測注入缺陷後仍失敗；
    `git grep` 不再有 `weapon_short`、`run_action_legal`、`legal_shots`、`analyze_legal`，各語言的版本字面 5 只剩一處定義
    （`:374` 的刻意舊版除外，註明）。
  - 02b：突變證明分布欄位不影響判定。
  - 02c：每一處都有「舊條件誤判」與「真缺陷突變」兩種測試且結果如上。
  - 重新分析的判定對照表與事前宣告一致。
- **L2（實機）**：macOS Intel／Metal 跑一次 report-only 動作短測與 GUI probe，確認報告有晚醒分布欄位與 `submitted_meshes` 逐幀欄位。不計次。
- **L3（人工）**：無。

## 平台

| 平台 | 本批 | 說明 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | L2 report-only 一次；v5 原始資料的重新分析也在本機 |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 預定執行 | L1 全部；macos-x64 經 Rosetta 2 |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 未執行 | pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行 | 依 D11⑩，實機只有 macOS Intel |
| Linux 實機 | 未執行 | 同上；`run_native_window.py` 不在本批執行 |
| macOS arm64 實機 | 未執行 | 同上 |

## 建議檔位

- 主體 high：跨 C++ probe、Python 分析器與 Go 測試的改寫，有 CTest、突變與重新分析做外部驗證。
- 02c 的計時判定局部升 xhigh：改錯不會報錯，只會靜默放寬或誤判。處理完降回 high。
- 檔位高於主對話時，開始時說明並徵求同意（D8）。

## Architecture Delta

無。只改 `object_fps_pvp` 自有的驗收工具（`build/acceptance/object_fps_pvp`）、產品測試與產品 Go gateway 測試；
不新增依賴邊，刪除 pvp 時這些檔案一起刪除。

## 執行規則

- 每次只做使用者指定的批次或子批；commit 與 PR 用日語。
- 開始、里程碑、停止時更新 [進度](README.md)、[交接](HANDOFF.md) 與 dev_log
  （`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch02.zh-Hant.md`），然後停止，不自動開始第03或04批。
- 先凍結分析器再重新分析；開發與重新分析不同時進行。
- 失敗跑次保留，先有限定位。不跑長測。
- 執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 完成條件

- 三個子批的 L1 全部通過；涵蓋對照表、判定對照表、事前宣告都已寫入本批 dev_log。
- L2 確認新欄位出現。
- 工具凍結的雜湊記錄在 [基線](BASELINE.md)，供第04批使用。

## 停止條件

- 涵蓋對照表有舊斷言找不到替代：停下回報，不刪。
- 重新分析的分類與事前宣告不一致：停下，保留輸出，有限定位後回報；不事後改宣告。
- 某處計時判定找不到結構條件、只能放寬時間常數：停下回報，不自行放寬。
- 需要改產品程式（例如觀測欄位不足）：停下回報，另行規劃。

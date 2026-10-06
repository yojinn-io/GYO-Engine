# 第 09 批：協議 v6（唯一的 wire 變更）

狀態：完成（2026-10-06；PR #57 待合併）。先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與
[v6 契約](../../protocol-v6.zh-Hant.md)（第 01 批的草稿，本批定稿）。

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 目標與範圍

Client／Gateway／Match 三角色在同一個 PR 升到 6，拒絕 v1–v5。
本批是 v6 唯一的 wire 變更（決定 D11①），**權威結果不變**。

做：

- 改名：proto `client_v6`／`runtime_v6`、Go 套件 `clientv6`／`runtimev6`，重新產生 bindings 與 CMake target。
- 版本號：產品 C++ 收成一個 constexpr；GYOP 標頭版本經 FF-8 的 Engine framing 由產品傳入；Go 沿用
  `adapter.go:15-16`；驗收端沿用第 02a 批的單一常數；三者改為 6。
- 受擊欄位（決定 D2）：`CombatState` 加 `last_damage_tick`、同生命內遞增的受擊計數、最後攻擊者 id。
- arena 內容 digest：以解析後的正規化內容計算，三角色比對（見下）。
- 契約 [protocol-v6](../../protocol-v6.zh-Hant.md) §1–7 定稿。
- 產品文件 `apps/object_fps_pvp/protocol/README.md` 改寫為 v6（目前寫「use protocol v5 and reject v1–v4」）。

不做：

- 受擊呈現與方向指示（→[第 13 批](13-hit-reaction.md)）。
- Collision 統一與權威變更（→[第 10 批](10-collision-authority.md)、FF-9）。
- 門檻調整、回合準備期、文字輸入與剪貼簿（候選）。
- 修改 v5 契約、v5 計畫文件與 v5 證據（全部保留）。
- 修改 `services/gyo_gateway`（共通 framing 的 Version 已是呼叫端參數）與共通 workflow。

## 依賴

- [第 02 批](02-acceptance-tools.md)（02a 的驗收端單一常數）。
- [第 03 批](03-authority-digest.md)（權威 digest 閘門與 `05042fa` 的基準 digest）。
- FF-3（Engine FNV-1a，arena digest 使用）、FF-8（GYOP 標頭編解碼收進 Engine，排在本批之前）。
  見 [foundation-followups 計畫](../../../architecture/plans/foundation-followups/PLAN.md)。
- 不依賴 IP-1／IP-2；本批 L2 的比較基準見「量測」。
  FF-3 與 IP-1 在 `InputActionMap.cpp` 的合併順序只是建議（後合併者 rebase），不是依賴；本批不等 IP-1。

## 版本號寫死處（檢查表）

來源：[BASELINE](BASELINE.md)「wire 版本號寫死處」（基準 `05042fa`），本批補齊名稱與呼叫端。
FF-8、02a 合併後行號會變；**本批開始時以 grep 重新產生此表，逐列打勾，差異寫進 dev_log**。

### A. 版本值

| # | 位置（`05042fa`） | 現況 | 本批處理 |
|---|---|---|---|
| A1 | `apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp:26` | Encode 寫入 5 | FF-8 後由產品常數傳入 Engine framing |
| A2 | 同檔 `:36` | Decode 要求 5 | 同上；拒絕 1–5 |
| A3 | `apps/object_fps_pvp/src/Pvp/IpcHost.cpp:60,86,132,196,206` | `set_protocol_version(5)` | 改用產品常數 |
| A4 | 同檔 `:156` | 收到 `!=5` 即丟棄 | 改用產品常數 |
| A5 | `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:270` | join request 寫 5 | 改用產品常數 |
| A6 | 同檔 `:275` | join reply 要求 5 | 改用產品常數 |
| A7 | `apps/object_fps_pvp/gateway/adapter/adapter.go:15-16` | `ClientVersion`／`RuntimeVersion` = 5 | 只改值 |
| A8 | `apps/object_fps_pvp/gateway/server_test.go:535,775` | 拒絕清單 `{1,2,3,4}` | 改為 `{1,2,3,4,5}` |
| A9 | `build/acceptance/object_fps_pvp/worker_main.cpp:40,120,377` | 假 Gateway 要求／回覆 5 | 02a 已改讀常數；本批確認 |
| A10 | 同檔 `:374` | 刻意用 4 測拒絕 | 改為測「拒絕 5」 |
| A11 | `action_short.hpp:97`、`combat_latency.hpp:194`、`gameplay_action.hpp:81,106,161` | 證據寫 `"protocol",5` | 02a 已改讀常數；本批確認 |
| A12 | `combat_gui_evidence.py:120,154`、`gameplay_evidence.py:139-140`、`gameplay_soak_evidence.py:177,180`、`test_combat_gui_evidence.py:102` | 分析器要求 5 | 02a 已改讀常數；本批確認 |
| A13 | `run_gameplay.py:40,49`、`gameplay_evidence.py:336`、`gameplay_soak_evidence.py:380`、`run_gameplay_gui.py:19,21`、`backpressure_probe.py:84` | 寫死 5；`backpressure_probe.py:84` 為位元組 `b'\x08\x05'`（protobuf 欄位 1＝5） | 02a 已改讀常數者本批確認；位元組樣式升版後為 `b'\x08\x06'`，以產品常數產生或逐列確認 |

### B. 名稱

| # | 位置 | 本批處理 |
|---|---|---|
| B1 | `apps/object_fps_pvp/protocol/client_v5.proto`、`runtime_v5.proto`（檔名、`package object_fps_pvp.{client,runtime}.v5`、`go_package`） | 改為 v6 |
| B2 | `protocol/clientv5/`、`protocol/runtimev5/`（checked-in 的 `.pb.go`） | 依 `protocol/README.md` 的 protoc 命令重新產生；不手改 |
| B3 | Go import：`adapter.go:11-12`、`server.go:24-25`、`runtime_link.go:14`、`action_delivery.go:13`，以及 `actions_test.go`、`adapter/actions_test.go`、`adapter_test.go`、`v5_test.go`、`backpressure_test.go`、`lifecycle_test.go`、`server_test.go` | 改為 `clientv6`／`runtimev6` |
| B4 | `apps/object_fps_pvp/CMakeLists.txt:16`（`foreach(contract client_v5 runtime_v5)`）、`:37,40` | 改為 v6 |
| B5 | `build/acceptance/object_fps_pvp/CMakeLists.txt:6,22` | 改為 v6 target |
| B6 | `#include "client_v5.pb.h"`：`ClientConnection.cpp:5`、`worker_main.cpp:5`、`network_main.cpp:4`；`#include "runtime_v5.pb.h"`：`IpcHost.cpp:5` | 改為 v6 |
| B7 | `namespace pb=...::v5`：`ClientConnection.cpp:24`、`IpcHost.cpp:17`、`worker_main.cpp:26`、`network_main.cpp:123` | 改為 v6 |
| B8 | 證據欄位名 `gameplay_v5`：`gameplay_action.hpp:161`、`gameplay_evidence.py:139`、`gameplay_soak_evidence.py:177` | 開始時決定改名或保留；改名則分析器同 commit 改 |

### C. 允許殘留（事前宣告）

grep 殘留只允許下列類別，每項附理由寫進 dev_log：

- 描述 v5 引入之語意的測試名稱（例：`tests/object_fps_pvp/LifePredictionTests.cpp` 的 `TEST_CASE("v5 ...")`、
  `adapter/v5_test.go` 的 `TestV5...`）。語意在 v6 沿用；是否改名開始時決定，不影響行為。
- 歷史文件與證據：`docs/object_fps_pvp/protocol-v5.zh-Hant.md`、`plans/v5/`、`docs/dev_logs/*`、
  `docs/architecture/plans/*/baseline/gyo_dependency_edges.txt`。

grep 至少涵蓋：`protocol_version\(5\)`、`!=\s*5`、`"protocol",\s*5`、`\\x08\\x05`（protobuf 欄位 1＝5 的位元組樣式）、`_v5`、`v5\b`、`clientv5`、`runtimev5`、`::v5`，
範圍為 `apps/object_fps_pvp`、`build/acceptance/object_fps_pvp`、`tests/object_fps_pvp`。

## 受擊欄位（決定 D2）

- 位置：兩份 proto 的 `CombatState`（`runtime_v5.proto:140`、`client_v5.proto:102`），C++ `Combat.hpp:80` 一帶，
  比照 `last_shot_tick`（`:150`／`:112`；寫入在 `PvpMatch.cpp:311`）。
- 寫入：Match 裁決命中時寫入受害者的三個欄位。同 Tick 多次命中：計數逐次遞增，攻擊者取最後一次裁決者
  （規則在契約定稿）。
- 重生歸零：重生時 `player.combat = CombatState{id}`（`PvpMatch.cpp:241`），新欄位預設值即歸零；以測試鎖住。
- 名稱、哨兵值（「沒有攻擊者」）與溢位語意在契約 §2 定稿。
- 本批只產生與傳送，不做呈現。

## arena 內容 digest

- 計算：Match 對解析後的正規化 arena 內容計算 digest，不用檔案位元組（避免 autocrlf）。演算法用 FF-3 的 Engine FNV-1a。
  正規化規則（欄位順序、浮點表示）寫進契約。
- 發布：Match 在 Ready 發布（`runtime` Ready 目前只有 `arena_id = 1`、`arena_version = 2`）。
- Gateway 轉入：房間狀態 JSON（`server.go:200`）、HTTP join 回覆（`:276`）、Welcome（`:440`）。Gateway 只轉送，不計算。
- Client 比對：`ClientConnection.cpp:180-184` 的 `CheckArena`，HTTP join（`:276`）與 Welcome（`:388`）兩條路徑都比對。
  不符時以契約定義的明確錯誤碼拒絕（目前只丟 `"Arena content version mismatch"`）。
- 呼叫端：`SetArenaIdentity`（`ClientConnection.cpp:582`）需帶 digest。呼叫者：`PvpApplication.cpp:679`、
  `action_main.cpp:110`、`timing_main.cpp:55`、`gameplay_action.hpp:88`、`worker_main.cpp:296`
  （假 Gateway 另在 `:44`、`:68` 回覆 arena 身分）。02a 刪除 `--legal-shots` 後，以實際存在者為準。

## commit 拆分

一批一個 PR（commit 與 PR 用日語）。每個 commit 都要能建置並通過 L1；權威 digest（不含新欄位）在每個 commit 上都與第 03 批基準相同。

| 順序 | 內容 | 檔位 | 版本值 |
|---|---|---|---|
| 0 | 契約 protocol-v6 §1–7 定稿（ultracode 的結論與對抗檢查結果） | ultracode | — |
| 1 | 改名（檢查表 B）：proto、Go 套件與 import、bindings 重新產生、CMake target、include 與 namespace。行為不變 | high | 仍為 5 |
| 2 | 版本（檢查表 A）：產品 C++ 收成一個 constexpr、Go 改值、驗收常數改值、拒絕 v1–v5、跨語言一致性測試、grep 檢查 | high；版本閘與解碼拒絕路徑局部 xhigh | 改為 6 |
| 3 | 受擊欄位：proto、C++ 型別、Match 寫入、IpcHost／Go adapter／Client 轉換、測試 | high | 6 |
| 4 | arena digest：計算、Ready 欄位、Gateway 轉送、Client 兩條路徑比對、錯誤碼、呼叫端 | high；解碼與拒絕路徑局部 xhigh | 6 |
| 5 | 文件：產品 `protocol/README.md`、本批 README／HANDOFF／dev_log | medium | — |

- commit 1 的 diff 只能有名稱變更與重新產生的 bindings；行為差異屬 commit 2 以後。
- commit 2 是唯一改版本值的 commit。

## 交付

- 契約 [protocol-v6](../../protocol-v6.zh-Hant.md) §1–7 定稿；§7 記錄本批的 Architecture Delta。
- 上述 commit 1–5。
- 檢查表 A／B 逐列結果與允許殘留清單（dev_log）。
- 權威 digest 兩樹比對報告（base＝本批 base commit，branch＝本批 HEAD）。
- L2 before／after 報告。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch09.zh-Hant.md`。

## 驗收點

L1（自動）：

- v6 wire golden 往返正確；標頭版本為 6。
- v1–v5 被拒：HTTP join、UDP 標頭、IPC envelope 三處；`worker_main` 測試「拒絕 5」。
- 跨語言一致性測試：C++、Go、Python 的版本常數相同。
- grep：除事前宣告的允許殘留外，沒有版本字面 5 與 v5 名稱。
- 受擊欄位：同 Tick 互射、致命命中後死者在同 Tick 的射擊被拒且不寫入、死亡 Tick、重生歸零、舊生命不殘留、未命中不寫入（2026-10-06 使用者決定：v6 規則下同一受害者每 Tick 至多被命中 1 次，改測可達情境；契約保留通用規則）。
- arena：digest 正規化穩定（同內容不同換行／空白得到同值）；內容不同 id／version 相同時，兩條路徑都以明確錯誤碼拒絕。
- 完整 UDP（含 24-byte 標頭）≤1200 bytes：以最大玩家數與新欄位的最大值測 Snapshot。
- CTest 全標籤、Go unit／race、CI 四平台通過。

權威不變的證明：

- 正式證明：同機兩樹（base／branch）以第 03 批的 runner 比對 digest（Math B6a 先例）。
  digest 的欄位清單由第 03 批固定，本批不修改 digest 程式；新欄位另行記錄，不進入此比對。
- CI 只做自洽檢查與不依賴 libm 的 golden 子集（yaw＝pitch＝0、軸向牆）。不修改共通 workflow。

L2（實機，macOS Intel／Metal）：

- 25 案矩陣一次、雙 GUI 整合短測一輪，before（base commit）／after（branch）比較。

L3（人工）：

- 使用者以兩個正式 Client 進一局，確認可加入、移動、射擊、死亡、重生；故意用不同 arena 內容啟動一方，確認被拒並看到原因。

## 量測

- 先凍結來源／產物／分析器，再量測；開發與乾淨量測不同時進行。
- 量測基線世代：before／after 都在本批的 base commit 與 branch 上，以凍結工具量。B0（[第 04 批](04-measurement-baseline.md)）
  與 B1（[第 07 批](07-measurement-baseline-b1.md)）只作歷史參照；v5 [STABLE_BASELINE](../v5/STABLE_BASELINE.md) 的數字只用來對門檻。
- 版本常數本身會從 5 變 6：分析器除版本常數外必須逐位元組相同，以 diff 證明並寫進報告。
- 失敗跑次保留；先有限定位再決定補驗。不跑長測。

## 平台表

| 平台 | 本批 | 理由 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | L2 與 L3、兩樹 digest 比對 |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 預定執行 | L1 全部；Go unit／race |
| Linux lavapipe GPU | 未執行 | pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行 | 沒有實機（決定 D11⑩） |
| Linux 實機 | 未執行 | 沒有實機（決定 D11⑩） |
| macOS arm64 實機 | 未執行 | 沒有實機（決定 D11⑩） |

## 建議檔位

主體 high；契約設計用 ultracode；版本閘與解碼拒絕路徑局部 xhigh。

- 協議與資料格式屬高風險，契約先用 ultracode（約 3 案、1 位評審、1 次對抗式檢查）。
- 改名、重新產生 bindings 與常數改值是機械工作，用 high。
- 漏改一處版本閘，執行時會靜默拒絕連線或混版，所以只有版本閘、arena 拒絕與解碼拒絕路徑局部升 xhigh，處理完降回。
- ultracode 與高於主對話的檔位，在本批開始時說明並徵求使用者同意（決定 D8）。

## 合併順序

合併順序只是建議，用來減少衝突與方便歸因，不是依賴；後合併的一方 rebase。

- `PvpApplication.cpp` 的建議合併順序為 05→06→12→11→13，不含本批；`PvpApplication.cpp:679` 的 `SetArenaIdentity` 呼叫改動屬本批，
  是本批在該檔唯一的改動。合併前 rebase 到當時的 master，衝突只能在這一行。
- `ClientConnection.cpp`、`IpcHost.cpp`、`Wire.hpp` 須在 FF-8 合併之後開始（FF-8 是依賴，見「依賴」）。

## 候選期規則

本批合併後到第 14b 批升格之前，版本號保持 6，原則上不再改 wire（決定 D11①）。
若必須改：需使用者同意，三角色同一 PR、舊程序重啟、不混用不同 commit 的 v6 候選。

## Architecture Delta

1. 需求：HANDOFF 第2項（受擊時點）、第10項（arena 只比 id／version），以及 v6 升版。
2. 問題：Snapshot 只有 hp，呈現無法以「同區間權威資料的純函數」決定受擊（違反 v5 §6 原則）；
   id／version 相同但內容不同的 arena，會讓預測與權威悄悄分歧。
3. 邊界：產品的 Client／Runtime wire 契約升到 v6（Runtime 與 Client 的 Data Contract 變更）。
4. 影響：pvp 的 Client、Match、IpcHost、產品 Go adapter 與 Gateway、驗收 probe 與分析器。
   Engine 與 `services/gyo_gateway` 不改（GYOP 標頭的 Engine 化屬 FF-8）。
5. 依賴方向不變：產品→Engine（FNV-1a 在 `GYO::Base`、framing 在 FF-8 的 Engine target）是既有方向。
6. Ownership：受擊欄位由 Match 產生；arena 內容由 Match 宣告、Client 驗證、Gateway 只轉送。版本常數由產品擁有，Engine framing 只接收。
7. 更小的變更不可行：不升版就無法拒絕舊端；以 HP 下降推導受擊違反純函數原則，且無法得到攻擊者。

## 完成條件與停止

完成條件：

- 檢查表 A／B 全部打勾，殘留只在事前宣告清單內。
- L1 全部通過；兩樹 digest 相同；L2 報告完成；L3 由使用者確認。
- 契約 §1–7 定稿；README／HANDOFF／dev_log 更新後停止，不自動開始下一批。

停止條件：

- 第 02、03 批或 FF-3、FF-8 未合併：不開始。
- 兩樹 digest 出現任何分歧：停下回報第一個分歧的 Tick 與欄位，不修改 digest 程式來消除分歧。
- Snapshot 加入新欄位後超過 1200 bytes：停下，帶數字請使用者決定。
- 發現 Engine 或 `services/gyo_gateway` 必須修改才能升版：停下，屬範圍外的 Architecture Delta。
- 開發中發現需要第二次 wire 變更：停下，依候選期規則徵求同意。
- 失敗跑次保留，先有限定位再回報。

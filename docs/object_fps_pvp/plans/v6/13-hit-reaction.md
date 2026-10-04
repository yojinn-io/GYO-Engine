# 第 13 批：受擊反應與方向指示

狀態：未開始。對應 v6 [交接](HANDOFF.md)第2項（呈現部分；wire 欄位在[第09批](09-protocol-v6.md)）。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與 [v6 契約](../../protocol-v6.zh-Hant.md)。

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 目標與範圍

v5 第04批 L3：被擊中沒有反應。v5 的 Snapshot 只有 `CombatState.hp`，沒有受擊時點。
[第09批](09-protocol-v6.md)依 D2 在 `CombatState` 加入 `last_damage_tick`、同生命內遞增的受擊計數與最後攻擊者 id；
本批只使用這些已記錄在契約的欄位。

做：

- 以受擊欄位決定反應區間：只限同一生命；不重播、不補播（例如受擊計數一次跳多格時只起一次反應，
  晚到的 Snapshot 不從頭再播）。
- 遠端人物：`Armature|Hit_Chest`（0.333 秒），加進 `players/animations.animset.json` 與 `PlayerPresentation` 的 clip 清單。
  不用 `Hit_Head`：權威命中只有 `Miss／World／Player`（`ShotQuery.hpp:9`），沒有部位依據。
- 第一人稱：畫面閃紅、HUD 受擊提示。
- 受擊方向指示：用最後攻擊者 id 找出攻擊者，以同一取樣中兩者的位置計算方向；攻擊者不在 Snapshot 時不顯示。
- 鏡頭晃動只作呈現：不改送出的 `yaw`／`pitch`（`PvpApplication.cpp:446` 的 `SubmitAction`）、`PlayerInput` 或射線。
- 優先順序：死亡＞受擊＞射擊／換彈＞俯仰基底（[第11批](11-remote-pitch-aim.md)）。

不做：

- 不改 wire、不改 Match（欄位由第09批寫入，本批只讀）。
- 不做爆頭或部位反應。
- 不做遠端屍體上的世界手槍處理（不在範圍）。

## 開始時的提案（D11⑥）

呈現細節在本批開始時提案，使用者確認後才實作。預設提案：

- 遠端只用 `Hit_Chest`；疊加方式（上半身遮罩或全身）與時間長度在提案中寫明。
- 第一人稱閃紅＋HUD；晃動只作呈現，幅度與衰減放產品資料。
- 方向指示的樣式與顯示時間。

使用者未確認前不寫實作。

## 交付

- 遠端受擊反應、第一人稱閃紅、HUD 提示、方向指示、呈現專用晃動。
- 參數放產品資料（`players/presentation.json` 或同 owner 的 UI／呈現資料），不寫進 C++。
- L1 測試、L2 證據、L3 回報。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch13.zh-Hant.md`。

## 驗收點

L1（自動）：

- 純函數測試：受擊與死亡同 Tick（死亡優先）、同 Tick 多次受擊、Snapshot 掉包、生命切換隔離（新生命不播舊受擊）、
  重生後計數歸零、重複／晚到 Snapshot 不重播。
- 方向指示：攻擊者存在、不在 Snapshot、與自己同位置三種情況。
- 輸入隔離：晃動開啟時，`PlayerInput` 與 Action 的 `yaw`／`pitch` 與關閉時逐位元相同（測試鎖住）。
- 突變檢查：讓晃動寫回視角、或忽略生命世代時，測試必須失敗。
- 權威 digest（[第03批](03-authority-digest.md)）：權威狀態（不含受擊欄位）不變，作附帶檢查。

L2（實機）：

- 雙 GUI 戰鬥短測，在本批 base commit 上以凍結工具量 before／after。
- Metal capture：遠端受擊、第一人稱閃紅與方向指示各一張。

L3（人工）：使用者確認被擊中時有反應，方向指示指向攻擊者；死亡時只看到死亡呈現。

## 量測與權威規則

- 量測基線世代：在本批的 base commit 上，以凍結工具量 before／after 比較；
  [第04批](04-measurement-baseline.md)（B0）只作歷史參照；v5 STABLE_BASELINE 的數字只用來對門檻。
- 先凍結來源／產物／分析器再量測；開發與乾淨量測不同時進行。
- 權威不變的正式證明：同機兩樹（base／branch）digest 比對；CI 只做自洽與不依賴 libm 的 golden 子集。不修改共通 workflow。
- 候選期規則（D11①）：本批在第09批之後，原則上不改 wire。若必須改，先取得使用者同意，並沿用三角色同 PR 規則。

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（L2 雙 GUI 短測、Metal capture、L3） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU | 未執行：pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行：沒有實機（D11⑩） |
| Linux 實機 | 未執行：沒有實機（D11⑩） |
| macOS arm64 實機 | 未執行：沒有實機（D11⑩） |

## 建議檔位

high。產品內的呈現實作；最大風險是呈現改變輸入（隱藏玩法變更），以 L1 輸入隔離測試鎖住。

## 依賴與合併順序

依賴：

- [第09批](09-protocol-v6.md)（受擊欄位）、[第11批](11-remote-pitch-aim.md)（俯仰基底）。

合併順序（建議）：

- 合併順序只是建議，用來減少衝突與方便歸因，不是依賴；後合併的一方 rebase。
- `PvpApplication.cpp` 的建議合併順序：05→06→12→11→**13**（最後）；順序不同時由後合併者 rebase，不因此等待。

## Architecture Delta

無。只使用第09批已記錄在契約的欄位；反應、HUD 與晃動都屬產品 Client 呈現與產品資產；
不新增 Dependency Edge，不改 wire、Match 或 Engine。

## 完成條件與停止條件

完成：

- D11⑥ 提案經使用者確認並照做；L1 全過（含輸入隔離與突變）；L2 通過；L3 使用者確認。
- 更新 [進度](README.md)、[交接](HANDOFF.md)、dev_log 後停止，不自動開始下一批。
- 一批一個 PR；commit 與 PR 用日語。

停止：

- 提案未經使用者確認：只停在提案，不實作。
- 第09批的欄位不足以表達需求（例如需要命中部位或更多攻擊資訊）：停下回報；改 wire 需使用者同意（D11①）。
- 晃動或回饋無法與送出的視角分離：停下回報，不以放寬測試處理。
- L2 失敗：保留跑次，有限定位後停下回報。

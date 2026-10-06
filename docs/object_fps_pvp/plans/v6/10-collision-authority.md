# 第 10 批：Collision 權威變更的 pvp 端（與 FF-9 同一 PR）

狀態：進行中（2026-10-07 開始；事前宣告規劃中，尚未寫程式）。先讀 [進度](README.md)、[交接](HANDOFF.md)、[v6 契約](../../protocol-v6.zh-Hant.md) §7，
以及 Engine 端的 [foundation-followups 計畫](../../../architecture/plans/foundation-followups/PLAN.md) FF-9。

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 目標與範圍

FF-9 統一 Engine Collision 的公開查詢並公開合法性檢查（決定 D3）。這會改變 pvp 的權威判定。
本批是 v6 唯一的權威變更，處理其中屬於 pvp 的部分。**不改 wire**，版本號維持 6。

做：

- `Arena::Validate`（`apps/object_fps_pvp/src/Pvp/Arena.cpp:20`）與 `ShotQuery`（`src/Pvp/ShotQuery.cpp:14`）改用 FF-9 的公開 `IsValid`。
- 事前宣告第 03 批情境中預期會變化的集合，事後產出差異報告。
- 權威 golden 只在本批更新一次；舊基準保留在證據。
- 記錄在 [protocol-v6](../../protocol-v6.zh-Hant.md) §7。

不做：

- Engine Collision 的實作、容差選擇、FF-1 語料統計（屬 FF-9）。
- 修改 wire 或版本號。
- 修改移動、預測或射擊規則本身（只換底層查詢與合法性檢查）。
- 調整門檻。

## 與 FF-9 的分工

同一個 PR，兩邊各自記錄，不互相重複：

| 項目 | 負責 | 紀錄位置 |
|---|---|---|
| 公開查詢統一為一套 double 實作與單一容差、刪除 float 演算法 | FF-9 | FF PLAN／HANDOFF |
| 容差擇一（`Collision.cpp:14` 的 `kEpsilon 1e-6f` 或 `CapsuleQueries.cpp:16` 的 `kTolerance 1e-7`）與理由 | FF-9 | FF PLAN、`Collision.hpp` 註解 |
| 公開 `IsValid`、`min>=max`／`min>max` 規則統一、退化膠囊拒絕 | FF-9 | FF PLAN |
| 會變動的公開函式清單、FF-1 語料命中翻轉上限（事前）；ULP 分布與翻轉數（事後） | FF-9 | FF HANDOFF |
| 未啟用產品的遷移清單 | FF-9 | [FF inactive_products](../../../architecture/plans/foundation-followups/inactive_products.md) |
| `Arena::Validate`、`ShotQuery` 改用 `IsValid` | 本批 | 本文件、dev_log |
| 第 03 批情境中預期變化的集合（事前） | 本批 | 本文件（開始時填入）、dev_log |
| pvp digest 第一個分歧 Tick 與原因分類（事後） | 本批 | dev_log、protocol-v6 §7 |
| pvp 權威 golden 更新（唯一一次） | 本批 | dev_log；舊基準保留在證據 |

Engine 文件只以文字摘要提到「提出需求的消費端」，不依賴本文件；本文件引用 FF-9 的宣告時以連結指向 FF。

## 依賴

- [第 09 批](09-protocol-v6.md)：v5 穩定基線的權威行為不得在版本號 5 之下悄悄改變，所以先升 v6。
- [第 03 批](03-authority-digest.md)：權威 digest runner 與兩樹比對腳本。
- FF-1（Collision 判定語料）已合併：FF-9 的事前宣告以 FF-1 語料為準（本批與 FF-9 同 PR）。

## 事前宣告（開始寫程式之前）

由 FF-9 提供（引用，不在此重複數字）：

- 會變動的公開函式清單。未列出的函式，第 03 批 digest 必須不變。
- FF-1 語料的命中翻轉上限。

由本批填入：

- pvp 使用的查詢與路徑：
  - 射擊（只有 Match）：`ShotQuery.cpp:41` `RaycastAabb`（牆遮擋）、`:48` `RaycastCapsule`（VerticalCapsule 多載，判玩家）。
    `QueryShot` 只在 `PvpMatch.cpp:307` 呼叫。
  - 移動（Match 與 Client 預測共用）：`src/Collision/CharacterCollision.cpp` 的 `OverlapVerticalCapsuleAabb`、
    `OverlapVerticalCapsules`、`SweepVerticalCapsuleAgainstAabb`、`SweepVerticalCapsuleAgainstCapsule`，
    以及 `src/Pvp/Movement.cpp`（`:37` 直接呼叫 Collision，也經 `CharacterCollision`）。
    `LocalPlayerPrediction` 經 `StepMovement`（`LocalPlayerPrediction.cpp:77`）進入 `Movement.cpp`，與 Match 使用同一組查詢。
- 移動與 Client 預測是否會變（與 FF-9 的條件式一致）：移動已走 double 實作（`kTolerance 1e-7`）。
  - FF-9 選 `1e-7`（現行移動用的 `kTolerance`）時，預期移動與 Client 預測不變。
  - 選其他容差時，移動的變化必須在此事前宣告：明列受影響的移動情境，並在 protocol-v6 §7 註明 Client 與 Match 必須同 commit（候選期規則）。
  - 停止條件是「出現未宣告的變化」，不是「移動有變化」。
- 第 03 批情境中預期會變化的集合（逐一列名）；其餘情境 digest 必須相同。
- arena 合法性：pvp 目前拒絕空的牆（`Arena.cpp:35-36` 用 `>=`），Engine 內部 assert 只拒絕 `min>max`
  （`CapsuleQueries.cpp` 的 `Validate(Aabb)`）。改用 `IsValid` 後，pvp 的 arena 規則不得默默放寬：
  若 FF-9 的 `IsValid(Aabb)` 允許空 AABB，pvp 保留「非空」的額外檢查並寫明理由。
  `assets/object_fps_pvp` 現有 arena 的合法性判定必須不變。

## commit 拆分

一個 PR（commit 與 PR 用日語），Engine 與 pvp 的 commit 分開：

| 順序 | 內容 | 負責 | 權威 digest |
|---|---|---|---|
| 1 | 公開 `IsValid`；Engine 內部 assert 改用 | FF-9 | 必須不變 |
| 2 | `Arena::Validate`、`ShotQuery` 改用 `IsValid` | 本批 | 必須不變 |
| 3 | 公開查詢統一、刪除 float 演算法、退化膠囊拒絕；同 commit 更新 pvp 權威 golden（唯一一次） | FF-9＋本批 | 依事前宣告變化 |
| 4 | 文件：FF PLAN／HANDOFF、本文件、protocol-v6 §7、dev_log | 各自 | — |

- commit 2 證明「改用 `IsValid`」本身不改權威；權威變化只出現在 commit 3。
- golden 與演算法同一個 commit，讓每個 commit 都能通過 L1。

## 交付

- `Arena.cpp`、`ShotQuery.cpp` 的修改與測試。
- 更新後的 pvp 權威 golden；舊基準複製到證據目錄並記錄雜湊。
- 差異報告：兩樹 digest 比對、第一個分歧的 Tick、欄位、原因分類（射擊遮擋／射擊命中／移動／arena 合法性），
  每一項對應到事前宣告。
- protocol-v6 §7 的權威變更紀錄。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch10.zh-Hant.md`（Engine 端另有 FF-9 的 dev_log）。

## 驗收點

L1（自動）：

- `tests/object_fps_pvp` 全部通過；arena 測試涵蓋空牆、`min>max`、非有限值、退化膠囊參數。
- 現有 arena 內容的合法性判定不變。
- 第 03 批 digest：未宣告的情境不變；宣告的情境每一項都有歸因。
- CI 自洽與不依賴 libm 的 golden 子集（yaw＝pitch＝0、軸向牆）在新 golden 下通過。不修改共通 workflow。
- CTest 全標籤、Go unit、CI 四平台通過。

權威變更的證明：

- 正式證明：同機兩樹（base／branch）以第 03 批的 runner 比對 digest（Math B6a 先例）。
- 分歧只能出現在事前宣告的情境，且原因分類與 FF-9 宣告的變動函式一致。

L2（實機，macOS Intel／Metal）：

- 25 案矩陣與動作短測，before（base commit）／after（branch）比較；命中率與拒絕分類不得出現未解釋的變化。

L3（人工）：無。本批不改呈現與操作。

## 量測

- 先凍結來源／產物／分析器，再量測；開發與乾淨量測不同時進行。
- 量測基線世代：before／after 在本批 base commit 與 branch 上以凍結工具量。B0（[第 04 批](04-measurement-baseline.md)）、
  B1（[第 07 批](07-measurement-baseline-b1.md)）只作歷史參照；v5 [STABLE_BASELINE](../v5/STABLE_BASELINE.md) 只用來對門檻。
- 失敗跑次保留；先有限定位再決定補驗。不跑長測。

## 平台表

| 平台 | 本批 | 理由 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | L2、兩樹 digest 比對 |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 預定執行 | L1 全部 |
| Linux lavapipe GPU | 未執行 | pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行 | 沒有實機（決定 D11⑩） |
| Linux 實機 | 未執行 | 沒有實機（決定 D11⑩） |
| macOS arm64 實機 | 未執行 | 沒有實機（決定 D11⑩） |

各平台 libm 不同，跨平台的權威一致只由 CI 的 libm 無關子集檢查，不宣稱逐平台相同。

## 建議檔位

主體 high；容差選擇、`IsValid` 套用後的 arena 規則與差異歸因局部 xhigh；事前宣告與差異歸因的證據用 ultracode。

- `Arena::Validate`、`ShotQuery` 改用 `IsValid` 與 golden 更新是機械性修改，用 high。
- 決定性與權威判定改變，錯了不會報錯；容差選擇（與 FF-9 對齊）、`IsValid` 套用後的 arena 規則與差異歸因局部升 xhigh，處理完降回。
- ultracode 規模：約 3 案、1 位評審、1 次對抗式檢查，用在事前宣告與差異歸因，需要用證據證明「只改了宣告的部分」。
- ultracode 與高於主對話的檔位，在本批開始時說明並徵求使用者同意（決定 D8）。

## Architecture Delta

pvp 端（Engine 端的七點由 FF-9 在 FF PLAN 記錄）：

1. 需求：HANDOFF 第10項（Collision float／double 兩套演算法並存）與決定 D3。
2. 問題：同形狀的查詢有兩套精度與容差；pvp 的 arena 合法性規則（`>=`）與 Engine 的前置條件（`>`）不一致，各自維護。
3. 邊界：pvp 權威判定的數值結果（protocol-v6 §7）；pvp arena 合法性改由 Engine 公開檢查提供，產品只保留產品自有的額外規則。
4. 影響：pvp Match（射擊；容差不是 `1e-7` 時含移動）、Client 預測（只在移動變化經事前宣告時）、`tests/object_fps_pvp`、權威 golden。
5. 依賴方向不變：產品→Engine Collision 是既有方向；不新增邊。
6. Ownership：幾何合法性歸 Engine Collision；arena 內容規則（數量、出生點、非空牆等）仍歸 pvp。
7. 更小的變更不可行：pvp 自行保留一份合法性判定，就會延續兩份規則的不一致；權威 golden 不同步更新，閘門會失敗。

## 完成條件與停止

完成條件：

- 事前宣告寫入本文件與 dev_log 後才開始實作。
- L1 全部通過；兩樹差異全部落在宣告範圍內並有歸因；L2 報告完成。
- golden 只更新一次，舊基準保留；protocol-v6 §7 與 FF 紀錄完成。
- README／HANDOFF／dev_log 更新後停止，不自動開始下一批。

停止條件：

- 第 09、03 批或 FF-1 未合併：不開始。
- 任何差異超出事前宣告（情境集合、FF-1 翻轉上限、變動函式清單）：停下回報，不事後放寬宣告。
- 現有 arena 內容的合法性判定改變：停下回報。
- 統一需要改 wire：停下，依候選期規則徵求同意（決定 D11①）。
- 失敗跑次保留，先有限定位再回報。

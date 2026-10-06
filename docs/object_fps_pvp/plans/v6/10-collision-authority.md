# 第 10 批：Collision 權威變更的 pvp 端（與 FF-9 同一 PR）

狀態：完成（2026-10-07；與 FF-9 同一個 PR，待開；L2 待執行）。先讀 [進度](README.md)、[交接](HANDOFF.md)、[v6 契約](../../protocol-v6.zh-Hant.md) §7，
以及 Engine 端的 [foundation-followups 計畫](../../../architecture/plans/foundation-followups/PLAN.md) FF-9。

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 目標與範圍

FF-9 統一 Engine Collision 的公開查詢並公開合法性檢查（決定 D3）。這會改變 pvp 的權威判定。
本批是 v6 唯一的權威變更，處理其中屬於 pvp 的部分。**不改 wire**，版本號維持 6。

做：

- `Arena::Validate`（`apps/object_fps_pvp/src/Pvp/Arena.cpp:20`）與 `ShotQuery`（`src/Pvp/ShotQuery.cpp:14`）改用 FF-9 的公開 `IsValid`。
- 事前宣告第 03 批情境中預期會變化的集合，事後產出差異報告。
- 權威 golden：事前宣告預期 35 個情境全部不變，**不重新產生**；base 的 digest 與 golden 雜湊記錄在證據（2026-10-07 使用者決定）。
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
| pvp 權威 digest 不變的證明（golden 不重新產生） | 本批 | dev_log；base 的 digest 與 golden 雜湊記錄在證據 |

Engine 文件只以文字摘要提到「提出需求的消費端」，不依賴本文件；本文件引用 FF-9 的宣告時以連結指向 FF。

## 依賴

- [第 09 批](09-protocol-v6.md)：v5 穩定基線的權威行為不得在版本號 5 之下悄悄改變，所以先升 v6。
- [第 03 批](03-authority-digest.md)：權威 digest runner 與兩樹比對腳本。
- FF-1（Collision 判定語料）已合併：FF-9 的事前宣告以 FF-1 語料為準（本批與 FF-9 同 PR）。

## 事前宣告（2026-10-07，寫程式之前；之後不得放寬）

Engine 側（容差、變動的公開函式、FF-1 語料上限、`IsValid` 規則）見 [FF-9 記錄器](../../../architecture/plans/foundation-followups/HANDOFF.md) 的「FF-9（記錄器）」節，這裡不重複數字。本批引用的結論：

- 容差是 `1e-7`。
- 結果會變的只有三個：`RaycastAabb`，以及 `VerticalCapsule` 版的 `RaycastCapsule`、`SweepSphereAgainstCapsule`。
- `IsValid(Aabb)` 允許零厚度。
- `IsValid(VerticalCapsule)` 沿用現行規則：`height >= 2r` 合法（球合法）；另有公開的 `IsValid(Math::Capsule)`（2026-10-07 方向修正）。

### pvp 使用的查詢（事實）

- **射擊**（只在 Match）：
  - `ShotQuery.cpp:41` 的 `RaycastAabb` 與 `:48` 的 V 版 `RaycastCapsule`。
  - `QueryShot` 只在 `PvpMatch.cpp:307` 呼叫。
- **移動、Client 預測、出生**：只走不變的 Overlap／Sweep。
  - 路徑：`CharacterCollision.cpp`、`Movement.cpp:37`。
  - Client 預測經 `StepMovement`（`LocalPlayerPrediction.cpp:77`、`:187`、`:292`）與 `MoveCharacterBody`（`:342`，renderPosition）。
  - 出生與 arena 檢查經 `CanPlaceCharacterBody`（`PvpMatch.cpp:195`、`Arena.cpp:49`）。
- 結論：移動與 Client 預測不變，不需要「Client 與 Match 同 commit」的註記。

### 權威 digest：預期變化集合＝∅

- **宣告**：
  - 第 03 批的 35 個情境全部逐位元不變（base `--run` 的 SHA-256 為 `34d0ca7f…`）。
  - 30 個 golden 不變（`authority_golden.txt` SHA-256 `f01f3a32…`；base `--check-golden` 30／30）。
  - 本批不重新產生 golden。
  - commit 1～3 的每一個都必須 35／35 相同。
  - 兩樹比對與 `--run` 一律用 scale 1（`compare_authority_trees.py` 與 CTest 的預設）；147 發與其餘裕只對 scale 1 成立，其他 scale 不在本宣告內。
- **依據 1：digest v1 沒有射擊距離。**
  - 欄位見 `AuthorityDigest.cpp:42-56`，`EncodeDecision` 見 `:169-181`。
  - `ShotDecision` 本身沒有距離欄位（`Combat.hpp:56-68`）。
  - 所以距離的 ULP 變化在 digest 中看不到；只有命中類別或目標翻轉才會出現。
- **依據 2：25 個不射擊情境**（move-* 18 個、prediction-* 7 個）不經過變動的查詢。射擊只在 `AuthorityDigest.cpp:402-464` 與 `:591-594` 提交。
- **依據 3：10 個射擊情境共 147 發被接受的射擊。**
  - 以 base `--dump` 的位置，用 double 參考幾何重算，hitKind 147／147 一致。
  - 移動在裁決之前（protocol-v6 §5；`PvpMatch.cpp:415`、`:449`），所以同一 Tick 記錄的位置就是裁決時的位置。
  - 最小餘裕：
    - 膠囊輪廓：5.0e-3 m（combat-graze，yaw −0.0245）；其餘情境 ≥0.085 m。
    - World 與 Player 的距離差：≥3.27 m。
    - 近平行分量只出現在 aimed/contract（yaw＝float(π/2)，\|d_z\|≈4.4e-8）。原點到 z 面 ≥2.0 m，遠大於 4.4e-8×100 m。
  - golden 射擊情境的方向恰為 (0,−0,1)。
- **推論**：舊 float 路徑在這個尺度的命中誤差 ≲1e-4 m，比最小餘裕小 50 倍以上；距離誤差 ≲0.03 m，比 3.27 m 小 100 倍以上。
- **若出現分歧**：一律停下，並依原因分類回報。
  - 第一個分歧的 Tick 是某發射擊的裁決 Tick，且第一個分歧欄位是 decision（hitKind、targetId、damage、targetLife）或目標的 hp、lifeState：歸為射擊命中或遮擋。
  - 分歧發生在非射擊 Tick，或第一個分歧欄位是 position、verticalVelocity、grounded、prediction：歸為移動。Engine 側已宣告移動不變，所以這屬於未宣告的變化。

### arena 合法性

- **牆**（`Arena.cpp:36-38`）：
  - 改為 `!Engine::Collision::IsValid(wall) || 任一軸 min >= max`；錯誤訊息不變。
  - Engine 只拒絕非有限與 `min>max`。
  - 「非空」保留為 pvp 的內容規則：零厚度的牆看不見，也沒有阻擋意義，屬於編輯錯誤。規則不放寬。
- **身體**（`Arena.cpp:30`）：
  - `bodyHeight < radius * 2` 改為 `!Engine::Collision::IsValid(Engine::Collision::VerticalCapsule{{}, bodyHeight, radius})`。
  - 其餘遊戲規則不動：`positive`、`eyeHeight <= bodyHeight`、出生點兩個、牆數 ≤1024。
- **判定變化：無。** `IsValid(VerticalCapsule)` 沿用現行規則（height ≥ 2r），與 `bodyHeight < radius * 2` 的判定完全相同；`body_height == 2·radius` 維持合法，arena 格式版本與內容契約都不變（2026-10-07 方向修正）。
- **現有內容的判定不變**（逐一確認）。以下 arena 的牆每軸 `min<max`，且 height＞2r：
  - `assets/object_fps_pvp/pvp_arena.json`（5 面牆，1.8／0.25）。
  - `tests/object_fps_pvp/fixtures/arena.json`（4 面牆，1.8／0.25）。
  - `ArenaDigestTests.cpp:17-20` 的合成 arena（1.85／0.27，含 ×1.01 的變體）。
  - digest 內建的 arena（`AuthorityDigest.cpp:200-232`）。
  - 各測試的 `TestArena`。

### protocol-v6 §7 的紀錄

本批完成時在 §7 記錄兩項：

1. 射擊判定的語意：`RaycastAabb` 只有方向分量恰為 0 才算平行，近平行射線不再視為平行；V 版 `RaycastCapsule` 改用 double，擦邊命中可能翻轉、距離有 ULP 變化。
2. 第 03 批 35 個情境的 digest 不變（記錄 base 的 `--run` 與 golden 雜湊）。

### 射擊查詢

- `QueryShot`（`ShotQuery.cpp:14`）：`:22` 的 origin 檢查改為算出 direction 之後檢查 `Engine::Collision::IsValid(Engine::Math::Ray{origin, direction})`。
  - 失敗時照舊 throw `invalid_argument("Invalid shot origin")`。
  - direction 由已驗證的角度算出，恆為單位向量，所以判定與現在相同。
- 牆與身體屬 domain-owned（`ShotQuery.hpp:19`），不在這裡重驗。

### 測試

- **既有期望值全部不變**：
  - `ShotQueryTests`：
    - 4.75 是精確值：`Roots` 的 a＝1、b＝−5、c＝24.9375，判別式 0.0625，24.9375／5.25＝4.75。
    - 牆距離 3、4.75、0 是軸向的精確值。
    - 方向的 −0 分量依「恰為 0 才算平行」處理。
    - 等距時 World 優先。
  - 另外 `PvpMatchTests`、`CombatTests`、`CombatHostTests`、`PredictionTests`、`ArenaDigestTests` 也不變。
- **commit 2 新增 arena 測試**：
  - 零厚度的牆被拒（產品規則）。
  - `min>max` 被拒。
  - NaN 被拒（既有，`PvpMatchTests.cpp:733`）。
  - `body_height == 2·radius` 合法（現行規則，2026-10-07 方向修正）；比 2·radius 小一個可表示值時被拒。
- golden 不變。

### L2

- 在 before（base `5da939f`）與 after 跑 25 案矩陣與動作短測。
- 預期命中率與拒絕分類沒有可歸因於 Collision 的變化，理由同 digest。
- 超出既有門檻，或出現新的拒絕類別，就停下。

### 停止條件（追加）

- 任一 commit 中，任何 digest 情境出現分歧。
- 現有 arena 內容的判定改變。
- 本節以外的期望值改變。

使用者決定（2026-10-07）：`RaycastAabb` 用 double slab；接受 commit 0（分析器先凍結）、golden 不更新。

**方向修正（2026-10-07，實作前，使用者）**：「退化膠囊一併拒絕」改為「退化膠囊造成的中止一併消除」（見 HANDOFF D3 的修訂）。`IsValid(VerticalCapsule)` 沿用現行規則（球合法），新增公開 `IsValid(Math::Capsule)`；arena 的合法性與契約都不變；「退化膠囊拒絕移到 commit 1」刪除，commit 1、2 不改變任何輸入的合法性；權威 digest 仍預期全部不變。PvP 的骨骼 hitbox 不在本批，列為 v7 候選。規劃以 ultracode 進行（workflow `wf_28494fb3-383`，對抗檢查 major 1、minor 10，全部套入）。

## commit 拆分

一個 PR（commit 與 PR 用日語），Engine 與 pvp 的 commit 分開：

| 順序 | 內容 | 負責 | 權威 digest |
|---|---|---|---|
| 0 | FF-9 分析器擴充並凍結（逐查詢翻轉與 ULP、`/vertical` 對 base `/general`、`RaycastAabb` 精確參考），以 base 對 base 自我檢查 | FF-9 | 不變 |
| 1 | 公開 `IsValid`（Aabb、VerticalCapsule、Math::Capsule、Ray；合法輸入集合不變）；Engine 內部 assert 改用 | FF-9 | 必須不變 |
| 2 | `Arena::Validate`、`ShotQuery` 改用 `IsValid`；arena 測試 | 本批 | 必須不變 |
| 3 | 公開查詢統一、刪除 float 演算法（`RaycastAabb` 改 double slab，`VerticalCapsule` 版改為薄包裝） | FF-9 | 必須不變（預期變化集合為空） |
| 4 | 文件：FF PLAN／HANDOFF、本文件、protocol-v6 §7、dev_log | 各自 | — |

- 2026-10-07 使用者決定：新增 commit 0，golden 不更新；方向修正後 commit 1 不改變任何輸入的合法性。
- commit 2 證明「改用 `IsValid`」本身不改權威；commit 3 的語意變化（近平行、擦邊）在 35 個情境中都觀測不到，所以每個 commit 的 digest 都必須不變，每個 commit 都能通過 L1。

## 交付

- `Arena.cpp`、`ShotQuery.cpp` 的修改與測試。
- 權威 digest 不變的證明：base 的 `--run` 與 golden 雜湊、每個 commit 的兩樹比對（golden 不重新產生）。
- 差異報告：兩樹 digest 比對、第一個分歧的 Tick、欄位、原因分類（射擊遮擋／射擊命中／移動／arena 合法性），
  每一項對應到事前宣告。
- protocol-v6 §7 的權威變更紀錄。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch10.zh-Hant.md`（Engine 端另有 FF-9 的 dev_log）。

## 驗收點

L1（自動）：

- `tests/object_fps_pvp` 全部通過；arena 測試涵蓋空牆、`min>max`、非有限值、退化膠囊參數。
- 現有 arena 內容的合法性判定不變。
- 第 03 批 digest：未宣告的情境不變；宣告的情境每一項都有歸因。
- CI 自洽與不依賴 libm 的 golden 子集（yaw＝pitch＝0、軸向牆）在既有 golden 下通過。不修改共通 workflow。
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

- `Arena::Validate`、`ShotQuery` 改用 `IsValid` 是機械性修改，用 high。
- 決定性與權威判定改變，錯了不會報錯；容差選擇（與 FF-9 對齊）、`IsValid` 套用後的 arena 規則與差異歸因局部升 xhigh，處理完降回。
- ultracode 規模：約 3 案、1 位評審、1 次對抗式檢查，用在事前宣告與差異歸因，需要用證據證明「只改了宣告的部分」。
- ultracode 與高於主對話的檔位，在本批開始時說明並徵求使用者同意（決定 D8）。

## Architecture Delta

pvp 端（Engine 端的七點由 FF-9 在 FF PLAN 記錄）：

1. 需求：HANDOFF 第10項（Collision float／double 兩套演算法並存）與決定 D3。
2. 問題：同形狀的查詢有兩套精度與容差；pvp 的 arena 合法性規則（`>=`）與 Engine 的前置條件（`>`）不一致，各自維護。
3. 邊界：pvp 權威判定的數值結果（protocol-v6 §7）；pvp arena 合法性改由 Engine 公開檢查提供，產品只保留產品自有的額外規則。
4. 影響：pvp Match（射擊；容差不是 `1e-7` 時含移動）、Client 預測（只在移動變化經事前宣告時）、`tests/object_fps_pvp`、權威 digest（預期不變）。
5. 依賴方向不變：產品→Engine Collision 是既有方向；不新增邊。
6. Ownership：幾何合法性歸 Engine Collision；arena 內容規則（數量、出生點、非空牆等）仍歸 pvp。
7. 更小的變更不可行：pvp 自行保留一份合法性判定，就會延續兩份規則的不一致。

## 完成條件與停止

完成條件：

- 事前宣告寫入本文件與 dev_log 後才開始實作。
- L1 全部通過；兩樹差異全部落在宣告範圍內並有歸因；L2 報告完成。
- golden 不重新產生，base 的 digest 與 golden 雜湊記錄在證據；protocol-v6 §7 與 FF 紀錄完成。
- README／HANDOFF／dev_log 更新後停止，不自動開始下一批。

停止條件：

- 第 09、03 批或 FF-1 未合併：不開始。
- 任何差異超出事前宣告（情境集合、FF-1 翻轉上限、變動函式清單）：停下回報，不事後放寬宣告。
- 現有 arena 內容的合法性判定改變：停下回報。
- 統一需要改 wire：停下，依候選期規則徵求同意（決定 D11①）。
- 失敗跑次保留，先有限定位再回報。

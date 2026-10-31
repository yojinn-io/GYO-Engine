# v8-product-anim-bottlenecks 的事前宣告

更新：2026-10-31。Owner：`object_fps_pvp`。**狀態：草稿，等你核准。** 計畫：[hitbox-plan](hitbox-plan.md) 第 5 節的 B1～B3（`v8-product-anim-bottlenecks-first`）。檔位 high｜Opus 5.5（繼承主會話），估時 4～6 h（超過 9 h 停下回報）。

## 這批要回答什麼

以 Valorant 為基底之後，Match 要能算出玩家的姿勢。動手搬動畫之前，先量三件事。每件事都對應一個後面的決定：

| # | 問題 | 會改變的決定 |
|---|---|---|
| B1 | Match 算姿勢會不會太貴 | `v8-product-anim-authority` 要不要多做省成本的工作 |
| B2 | Match 載入動畫檔要多久、佔多少記憶體 | `v8-product-hitbox-data` 的載入方式：Match 直接讀 FBX，或改讀預先轉好的動畫資料 |
| B3 | 同一個姿勢在四個平台上是否逐位元相同 | `v8-product-hitbox-golden-policy`：含動畫命中的情境能不能進跨平台 golden |

這批只量，不改 Match、不改 wire、不改既有測試的判定。

## 量測工具

- 新的報告用程式 `gyo_object_fps_pvp_anim_bottlenecks`（`tests/object_fps_pvp/AnimBottlenecks.cpp`），和 `hitbox_survey` 一樣放在 presentation 的 stage，CTest 名 `object_fps_pvp.anim_bottlenecks`。Owner 是 `object_fps_pvp` 的測試；不碰 Engine。
- 載入路徑和 Client 相同：asset catalog → `UfbxModelLoader` → `LoadPlayerPresentationDefinition`（含 `TransferCompatibleAnimation` 把 15 個 clip 轉到 Player_V20）。
- 姿勢用現有的 `SamplePlayerPresentationPose`。它現在屬於 Client 的呈現層；`v8-product-anim-authority` 搬到 domain 之後，取樣的呼叫（`SamplePose`、`BlendPoses`、階層合成）仍是同一組，所以拿它當代理。它順便算了武器的姿勢，判定用不到，因此量到的是**上限**。
- 測試只在載入失敗或自我檢查失敗時不通過；量到的數字不決定 CTest 結果。
- 輸出：一行摘要印到 stdout（CTest 對通過的測試只保留約 1 KB 的輸出，摘要壓在這之內），完整報告寫成 JSON。CI 的 diagnostics artifact 會收 `tests.xml`，從那裡讀摘要。

### 姿勢的狀態集（固定、事先列出）

用 hitbox survey 已用過的狀態，每種固定幾個時間點，共 **24 個姿勢**：

- 站立移動（walk／jog 混合，`moveWeight` 0、0.5、1）× 上半身 Hold、Shoot：6 個
- 蹲（`moveWeight` 0、0.5、1）：3 個
- 起跳、空中、落地（各 2 個時間點）：6 個
- 死亡（2 個時間點）：2 個
- 瞄準上、中、下（`aimWeight` −1、0、1）：3 個
- 上半身 Reload、Hit（各 2 個時間點）：4 個

清單寫死在程式裡，四個平台跑完全相同的輸入。

## 受入判定（看結果之前宣告）

### 收支

每個平台的報告必須有：24 個姿勢 × B1 的次數全部完成、B2 的 3 次載入、B3 的兩個雜湊各算 2 次。任一平台缺報告或缺項 → 收支記「確認できない」，那個平台的判定不下，等你決定。

### B1：取樣的耗時

**最壞情況的算法**：Match 只有在判定射擊時才需要姿勢（狀態機每 Tick 推進的是幾個數字，很便宜；貴的是取樣骨骼）。這是照 Valorant「不需要時不算」的做法，CS 也是在處理射擊命令時才為回溯重建姿勢。一個 Tick 最多 4 人同時開槍、每槍檢查另外 3 人，回溯之後每槍看的 Tick 可能不同，所以**最壞 12 次取樣／Tick**。

- 量法：每個姿勢先熱身 100 次，再量 2000 次，取每次的耗時；以 24 個姿勢中 p99 最大的那個當「單次成本」。
- 判定平台：CI 的 linux-x64（Match 的主戰場是 Linux；CI 的共用 VM 比實機慢，偏保守）。Intel Mac 本機、其他三個 CI 平台只記錄。
- 判定：**12 × 單次成本 ≤ 1.67 ms（60 Hz 一個 Tick 16.67 ms 的 10%）** → `v8-product-anim-authority` 不加省成本的工作；**超過** → 停下回報，選項是照抄 Valorant 的另一招（每 N Tick 算一次＋內插）或只取樣命中判定用到的骨頭鏈。
- 10% 的理由：Match 每 Tick 還要做移動、碰撞、射擊、序列化；留九成給既有工作。數值是我提的，可以改（見「等你決定」）。

### B2：載入

- 量法：同一個 process 內從零載入 3 次（catalog → FBX → 轉換 15 個 clip），取最慢的一次；記錄 process 的峰值記憶體增量（Linux／macOS 用 `getrusage`；Windows 記 n/a，不判定）。另外記錄：兩個 FBX 的檔案大小、轉換後 15 個 clip 的 key 佔用的位元組數（估「只帶需要的 clip」時的大小）。
- 判定平台：CI 的 linux-x64。
- 判定：**載入 ≤ 2 s 且峰值記憶體增量 ≤ 200 MB** → `v8-product-hitbox-data` 讓 Match 直接用 `UfbxModelLoader` 讀 FBX（改動最小、可逆，Match 多依賴 `GYO::AssetUfbx`，已在 hitbox-plan 第 7 節）；**任一項超過** → 改讀預先轉好的動畫資料（新的產品內 Data Contract 加一個轉換工具，`v8-product-hitbox-data` 估時要重估），先回報再定。
- 2 s／200 MB 的理由：Match 啟動時多出的等待在 2 s 內玩家感覺不到太大差別；200 MB 是一台 Linux 主機同時開幾個 Match 還有餘裕的量。兩個數字都是我提的，可以改。

### B3：跨平台決定性

- 量法：兩個雜湊（FNV-1a，逐個 float 的位元）：
  - **H-load**：轉換後 15 個 clip 的全部 key；
  - **H-pose**：24 個姿勢的全部骨骼 global transform。
  同一個 job 內各算 2 次。分兩個是為了知道不一致出在載入轉換，還是取樣。
- 自我檢查（不通過就讓 CTest 失敗）：把任一個 float 翻一個位元，雜湊要變；雜湊涵蓋的 float 個數要等於預期（骨骼數 × 16 × 24），避免「什麼都沒算所以處處相同」。
- 判定：
  - 同一平台兩次的雜湊不同 → **停下回報**（連「Match 可重現」都不成立）。
  - 四平台 H-pose 全部相同 → 含動畫命中的情境可以進跨平台 golden。
  - 有平台不同 → `v8-product-hitbox-golden-policy` 照計畫的建議：含動畫命中的情境只做同平台兩樹比對，跨平台 golden 只留不含動畫命中的情境。
  - macos-x64 只有它不同時，照平台優先序只記錄（它在 CI 上是 Rosetta），不讓它單獨決定 golden 的政策。

## 不改的東西

- 既有測試的判定一個都不改（AGENTS §14.4 不適用）。開工後若發現要改，停下回報。
- 這個工具只報告，不加突變；B3 的自我檢查就是它的檢出力證明。

## 等你決定

### `v8-product-anim-bottlenecks-thresholds`：B1、B2 的門檻

- (a) 照上面：B1 12 次取樣 ≤ 1.67 ms（Tick 的 10%）；B2 載入 ≤ 2 s、記憶體 ≤ 200 MB（建議）；
- (b) 你給別的數字。

### `v8-product-anim-bottlenecks-worst-case`：B1 的最壞情況

- (a) 12 次取樣／Tick（4 人同時開槍 × 3 個目標，各自回溯到不同 Tick）（建議：rewind-line 之後就是這個上限）；
- (b) 4 次／Tick（每人每 Tick 取樣一次後共用，不考慮回溯）。現在還沒有回溯，但 rewind-line 加上去之後要重量。

### `v8-product-anim-bottlenecks-ci-run`：怎麼拿到四平台的數字

工具要在四平台 L1 上跑才有 B1 的 linux-x64 與 B3。

- (a) 推送這條分支，手動 dispatch `cross-platform.yml`，從 diagnostics artifact 讀摘要；不開 PR（建議：hitbox-line 的 PR 之後才開，照「PR 少開」）；
- (b) 開 draft PR 再手動 dispatch。

## 範圍的限界

- B1 的數字來自 CI 的共用 VM，不是 Linux 實機；耗時有雜訊。實機的數字等之後有 L2 時補，不在這批。
- 量的是 Client 現在的取樣函數（含武器，偏高），不是搬到 domain 後的版本。`v8-product-hitbox-acceptance` 會在 Match 上再量一次。
- B4（wire 大小）、B5（Client 與 Match 的姿勢一致度）不在這批，排在 `v8-product-hitbox-acceptance`。

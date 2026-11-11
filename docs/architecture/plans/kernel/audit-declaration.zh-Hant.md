# 事前宣告：`v8-arch-kernel-audit`（現有模組對規格的審查）

更新：2026-11-11。Owner：架構（架構師會話）。**狀態：事前宣告，待使用者確認；未啟動。**
依 AGENTS §14.1，判定在看結果前宣告；看結果後不放寬。依 §14 冒頭與 `docs/architecture/target/README.md`，範囲の限界寫編號加一句摘要。

## 目的

拿規格 v0.2（`spec.zh-Hant.md`）去讀現有每個模組，逐模組回答三個可檢查的問題，判「留／包／重寫／服務／Host」，給出中間層的成本形狀。這是 mechanism §1 第 3 項輸入，也是改造批次計畫的根據。只讀，不改程式。

## 範圍

- 審：`engine/*`（每個子目錄一列）、`framework/*`、`apps/object_fps_pvp/src/*`（依現有子目錄）、`tools/voxel_map_editor`、`tools/audio_editor`、`tools/model_viewer`、`tools/ui_editor`。
- 不審（寫明理由）：`apps/object_fps`、`apps/object_fps_v2`、`tools/object_fps_preview`（使用者 K.3 第 8 項：移除）；`services/gyo_gateway`（外部服務，Go）；`third_party/`（第三方）；`tests/`（守衛本身，另在 J 節）。
- 規格 L 節的初判是起點，不是結論；每列要重新用證據判。

## 每個模組回答的問題（spec I.3）

1. 有沒有自己的**生命週期或更新迴圈**：`Create`／`Start`／`Stop`／`Init`、`BeginFrame`／`Update`／`Tick`／`Advance`／`Pump`／`Poll`；是否擁有執行緒（`RoleThread`、`std::thread`）、`std::mutex`、`std::atomic`。
2. 有沒有**跨 tick 的指標或參考**指向 World 會持有的資料（成員指標、快取的參考、回呼捕獲）。
3. 有沒有**全域可變狀態**：命名空間或函式內 `static` 可變變數、`thread_local`、單例、第三方全域 context、`<chrono>`／`steady_clock`、`<random>`／`random_device`。
4. 是否**自己起執行緒碰**將成為 World 的資料。

判法：1 為是 → 組織層程式碼（重寫或變 Host／服務）；2～4 皆否 → 可**包**（狀態變 Resource、更新變 System、生命週期交 Host）；2～4 任一為是 → 契約只能靠審查，偏向**重寫**那一層介面或先拆掉那些狀態；純函式、三否定皆滿足 → **留**。

## 產出

- `docs/architecture/plans/kernel/audit.zh-Hant.md`：逐模組一列——模組、行數（腳本統計）、四問的答案各附檔名行號、判定、理由、工時粗估（區間；不是承諾）；另三節：(a) Body 拆欄的範圍與受影響的 golden 清單（K.3 第 9、10 項）；(b) Host 保留職責的現況位置清單（spec H.7、H-7 的掃描清單）；(c) spec G.4「現況中的違反」的逐條確認。
- `docs/architecture/plans/kernel/scripts/audit_scan.py`：可重跑的掃描（更新入口函式名、執行緒原語、全域狀態、時鐘與亂數），輸出 CSV 到主 checkout `build/evidence/kernel-audit/`；報告的每個掃描結論引用 CSV 的列。
- dev_log `docs/dev_logs/2026_MM_DD_arch_kernel_audit.zh-Hant.md`；HANDOFF 更新。

## 判定（看結果前宣告）

1. **覆蓋**：範圍內每個目錄都有一列；「未審」只允許附理由（移除、第三方、外部）。缺列＝不通過。
2. **證據**：每列的四問各至少一個檔名行號（或「掃描無命中」加 CSV 列號）；沒有證據的判定不算判定。
3. **對抗複核**：另一模型的檢查者抽至少 5 個模組（必含一個「留」、一個「包」、一個「重寫」），不看審查者的理由、只看程式，獨立判一次；5 個中至少 4 個與審查者一致，否則審查回到提案（不一致的模組全部重判並寫明原因）。
4. **掃描可重跑**：腳本在主 checkout 重跑一次，CSV 與報告引用的列一致；件数の突き合わせ：掃描的目錄數＝報告的列數＋「未審」列數。
5. **不改程式、不改契約**：`git diff` 除 `docs/` 與 `docs/.../scripts/` 外為空。

## 範囲の限界（引用 `docs/architecture/target/limits.zh-Hant.md`）

- **LIM-3**：debug 存取核對攔不到的形狀（寫了又寫回、經存下參考只讀、堆積成員、全域狀態、host 碰過同欄、其他執行緒等）。本批的四問以掃描加閱讀判，**判不到執行期才出現的繞路**；「包」的結論表示「靜態看不到違反」，不表示執行期一定合規。
- **LIM-1**（(b) 讀 (c) 要顯式順序）、**LIM-2**（v1 只有 Serial）、**LIM-5**（Join／Leave 在 tick 內）：本批不量它們的成本，只在對應模組列註記「受 LIM-n 影響」。
- 工時估是粗估區間，不是承諾；精確估計在各改造批的事前宣告。

## 模型／檔位（照領航員規則；上段）

- 審查者：Opus／high（讀碼、跑掃描、寫報告）。
- 對抗複核者：Fable／high（與審查者不同模型；只讀；抽 5 個模組獨立判）。
- 主會話（Fable）持控制權：開子代理、收結果、逐列讀過再採用（`v8-arch-kernel-control`）。

## 估時與停止

- 估：審查者 6～10 小時的代理工作（掃描腳本 1、逐模組 4～7、三節 1～2）；對抗 1～2 小時。上限 1.5 倍；超過停下回報。
- 停止條件照 AGENTS §14 冒頭：同一模組換兩種判法仍無結論；發現規格前提錯誤（例如 I.3 三問判不出某類模組）→ 停下報告，不自行改規格。

## 不做

- 不開始遷移、不改任何程式；不決定批次順序（那是改造計畫的事）；不重估 spike 的結論。

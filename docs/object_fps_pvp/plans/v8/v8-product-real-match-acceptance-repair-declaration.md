# v8-product-real-match-acceptance-repair 的事前宣告

更新：2026-11-01。Owner：`object_fps_pvp`。**狀態：草稿，等你決定 4 項**。來源：`v8-product-hitbox-shot-j7-split`（J7 移到這一批）。檔位 high｜Opus 5.5。估時 6～9 h（超過 13.5 h 停下回報）。

## 這批做什麼

讓真 Gateway＋真 Match 開槍的本機驗收重新能用、能判定。它們都不在 CI（L1 只跑 `cpu|shader` 標籤），所以壞了沒有人發現。

不在這批：
- 依部位推算傷害、顯示受擊：`v8-product-hit-presentation`（GUI 驗收工具 `run_action_short.py`、`combat_gui_evidence.py` 等也歸那一批）。
- 把這些驗收放進 CI。

## 開工前查到的事實（2026-11-01）

證據都在主 checkout 的 `build/evidence/`（Intel Mac）：`v8-hitbox-shot-wire-stop/`、`v8-real-match-repair-survey/`。

1. **`run_gameplay.py`（v5 起實際在用的開槍驗收）被 #128 弄壞了。** 這是回歸（AGENTS §14），S3 的宣告沒有列到它。
   - `clean-60` 在 master（姿勢模型）上兩次都失敗：`Invalid v5 damage attribution`、`Predeclared lethal hit did not damage life1 exactly once`。
   - 同一棵樹暫時改回合成模型：第一次失敗在時間檢查（`Clean Match runtime gap/dropped time`），第二次通過。
   - 原因：`gameplay_evidence.py:186-187` 只接受傷害 0 或 25000（Body）。probe（`gameplay_action.hpp:167`）對目標射擊用 pitch 0，從眼睛高度水平射出，姿勢模型下打中頭，傷害是 50000。
   - `gameplay_soak_evidence.py:228-232` 有相同的假設，還沒有跑過。
2. **`run_network.py` 自 pv8 起就跑不起來。**
   - 它把 Match 複製到沒有內容的暫存資料夾，註解寫「證明 Match 不需要產品的 SDL DLL 或 asset catalog 就能啟動」。
   - pv8 起 Match 要讀 `movement_rules.json`（`4c9cbbea`，2026-10-30），第一個無效埠檢查就失敗了。pv9 起又經 `ContentManifest`＋asset catalog 讀動畫與形狀（`PlayerAnimationContent.cpp:73-86`，protocol-v9 §4.4 已核准）。所以這個前提的「不需要 asset catalog」已經不成立。「不需要 SDL」仍然成立（Match 執行檔沒有連結 SDL）。
   - life-cleanup（`network_main.cpp:194`）要求 `d.damage==25`。pv9 把傷害改成千分之一點以後，實際值是 25000，這行沒有跟著改。它也用 pitch 0。
   - 修好啟動以後，後面的階段（主迴圈、life-cleanup、斷線）還會不會有問題，現在看不到。
3. **`action_probe.py` 自己的主模式，早就知道是失敗的。**
   - v7 第 06 批的 dev_log（2026-10-09）記過：主模式是 v4 語意、已凍結，v5 起的驗收不用它，before／after 都以相同的錯誤失敗。
   - 這次查到的錯誤（6 秒內雙方都被打死 4 次、重生後 hp 回滿，分析器卻把每條命的傷害加總）和那時同一類。S3 宣告把 `action_main.cpp` 列進 J7，是我沒查到這筆紀錄。
   - 凍結的 `action_probe.py` 被其他 runner 原樣匯入，雜湊寫死（`run_client_delay.py` 等）。

## 判定（結果出來前宣告，§14.1）

- **K1 `run_gameplay.py` 全部案例通過**（不帶 `--case`，Intel Mac，一輪）。
  - 每個案例跑一次。只在 `Clean Match runtime gap/dropped time` 這類時間檢查失敗時，可以重跑一次，兩次都記錄；重跑也失敗就停下回報。
  - 和傷害、生命、決定有關的錯誤不重跑，直接停下回報。
- **K2 `run_gameplay_soak.py` 短模式通過**（2～4 輪，一次）。
- **K3 `run_network.py` 預設模式通過**（不帶 `--network-impairments`、`--gui-probe`，一次），含 life-cleanup。
- **K4 瞄準點是軀幹**：K1～K3 裡所有命中的決定，部位都是 BODY。這不是新的判定：分析器只接受 25000，已經隱含這一點。
- 時間檢查以外的失敗，原因落在本宣告以外的程式時，停下回報。

## 會改判定的既有測試

| # | 測試 | 改法 | 寬鬆面 |
|---|---|---|---|
| W1 | `network_main.cpp:194` life-cleanup 的 `d.damage==25` | 改成 Welcome 給的 `shotDamage`（25000）。瞄準點依 `v8-product-real-match-aim` | 無（單位換算漏改；舊值在 pv9 下任何輸入都失敗） |
| W2 | `run_network.py` 隔離啟動的前提 | 依 `v8-product-real-match-isolation` | **有**，見下面的 1～5 |
| W3 | `gameplay_action.hpp:167` 對目標的 pitch 0 | 依 `v8-product-real-match-aim` 改瞄準；`gameplay_evidence.py` 與 soak 的分析器不動 | 無（只動 probe，判定不變） |

### W2 的 1～5（AGENTS §14.4，選 (a) 時）

1. **守的性質**：部署好的 Match 不依賴產品的 SDL DLL 和 asset catalog，從執行檔所在的位置找到自己的預設 arena。
2. **寬鬆面**：Match 需要 asset catalog 時，以前會判失敗，以後判通過。
3. **收緊面**：暫存資料夾只放 Match 自己的內容資料夾，缺任何一個 Match 要讀的檔，仍然失敗；SDL DLL 仍然不放。
4. **寬鬆面的性質由誰守**：「Match 需要內容」是 protocol-v9 §4.4 核准的設計，不再是要守的性質。內容錯誤時讀者會拒絕（D1、D3 的讀者測試），`match_main.cpp` 載入失敗就以 1 結束。但「部署的 Match 少了內容檔就停止」有沒有執行檔層級的測試，我還沒查到；開工時確認，沒有就在 W2 的突變檢查裡補上。
5. **根據**：本宣告「事實」第 2 點；證據在 `v8-hitbox-shot-wire-stop/network.out`。
- 照 §14.4，承認前要做突變檢查（暫存資料夾少一個內容檔要失敗、放進 SDL DLL 的檢查仍在），並由另一位審查者用實際的資料核對 1～5。

## 突變

- `v8-product-real-match-aim`：probe 的瞄準改回 pitch 0，K1 要失敗在 `Invalid v5 damage attribution`。
- W2：暫存資料夾少一個內容檔，K3 要失敗在啟動。
- 這些 runner 不在 CTest，`run_mutations.py` 跑不到。它們的突變照 S3 的作法，用跑次腳本手動執行，記錄在 dev_log。

## 等你決定

### `v8-product-real-match-aim`：驗收怎麼避開部位傷害

- (a) probe 用姿勢模型算出目標軀幹的瞄準點（建議）。
  - action probe 和 network probe 一樣連結 `player_content`。
  - 理由：分析器不動，「每發 25000、第 4 發致死」的預先宣告照舊；瞄準點跟著內容走，換模型也對。
- (b) 分析器接受各部位的傷害，依實際的命中重算致死的那一發。
  - probe 不動，但凍結的分析器要改判定，而且 v5 的「預先宣告的致死那一發」會變成依結果決定，屬於寬鬆面。
- (c) probe 寫死一個往下的 pitch。
  - 最快，但瞄準點依賴目前的身高與距離，換內容就會失效。

### `v8-product-real-match-isolation`：`run_network.py` 的隔離啟動

- (a) 暫存資料夾放 Match＋它自己的內容資料夾（建議）。
  - 理由：留下仍然成立的「不需要 SDL」，拿掉已經不成立的「不需要 asset catalog」；揭露見 W2 的 1～5。
- (b) 不再隔離，直接在建置的位置啟動。
  - 最簡單，但連「不需要 SDL」也不再驗證。

### `v8-product-real-match-action-probe-main`：`action_probe.py` 的主模式

- (a) 不修，照 v7 第 06 批的紀錄當成已退役的凍結模式；在 HANDOFF 與這份宣告寫清楚「主模式已知失敗、不當驗收」（建議）。
  - 理由：v5 起不用它，其他 runner 匯入它，雜湊寫死；修它會碰到凍結的檔案。
- (b) 修分析器，讓它考慮死亡與重生。
  - 要解凍並重算雜湊，換來一個沒有人用的模式。

### `v8-product-real-match-pr`：放在哪個 PR

- (a) 和下一條產品線 `v8-product-hit-presentation` 同一個 PR（建議）。
  - 理由：PR 以功能線為單位、盡量少開；兩者都在調整驗收工具對部位傷害的假設。這些 runner 不在 CI，等的這段期間 CI 不會因此變紅。
- (b) 自己開一個小 PR，做完就合併。
  - master 上的本機驗收早點恢復，但多一次全量 CI。

## 範圍的限界

- K1～K3 只在 Intel Mac 跑。Linux（Match 的主戰場）與 Windows 要另外安排；CI 不跑它們。
- 修好以後，它們仍然不在 CI，下次壞掉還是不會自動發現。要不要讓一部分進 CI，不在這批（CI 的標籤、時間預算是另一個決定）。
- K4 只確認瞄準點是軀幹，不驗證頭和腿；部位由 S2 的單元測試與 S3 的 `hit_part_cross` 守住。
- `run_network.py` 修好啟動以後，後面的階段可能還有別的問題，本宣告沒辦法事先列出；碰到時照 §14 停下。

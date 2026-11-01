# PvP v8：v8-product-real-match-acceptance-repair（修真 Match 開槍的本機驗收）

日期：2026-11-01。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5；W2 的 §14.4 審查：Sonnet 5.5（額外意見）＋ Fable 5.1（審查者）。估時 6～9 h，實際約 3 h。

**狀態：完成。** W2 已由使用者承認（`v8-product-real-match-w2-approve`）。 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-real-match-acceptance-repair-declaration.md)，4 項決定都照 (a)；試行 1 有兩項不成立，照「新宣告重跑」的判定重跑後通過。

## 做了什麼

- `hit_aim.hpp`（新）：probe 共用的瞄準。用產品的姿勢模型，從射手的 snapshot 擺出目標，瞄該部位第一個形狀的中點，眼睛高度照射手的姿態。內容根目錄取 arena 檔所在的資料夾；`--content` 只有 `--hit-parts-only` 用。
- `gameplay_action.hpp`：對目標的射擊改瞄軀幹（`v8-product-real-match-aim` (a)），換彈和不對目標的射擊照舊。action probe 多連結 `player_content`。凍結的 `action_probe.py` 和分析器都沒有改。
- `network_main.cpp`：
  - life-cleanup 也瞄軀幹；傷害改成 Welcome 給的 `shotDamage`（W1）。
  - `--hit-parts-only` 改用 `hit_aim.hpp`，行為不變。
  - life-cleanup、failure probe 都需要 `--arena`。
- `run_network.py`（W2，`v8-product-real-match-isolation` (a)）：
  - 隔離副本只放執行檔和 `assets/object_fps_pvp`，放在 `match/` 子資料夾，工作資料夾裡沒有資產。
  - 新增檢查：少了 `movement_rules.json` 的副本必須以 1 結束。
  - life-cleanup 與 failure probe 傳 `--arena`。
- `action_probe.py` 的主模式不修，記為已退役（`v8-product-real-match-action-probe-main` (a)）。

## 判定

試行 1（宣告的判定，證據在主 checkout 的 `build/evidence/v8-real-match-repair/`，Intel Mac）：

- K1 `run_gameplay.py` 全部案例：**通過**，25／25 一次通過、沒有重跑（`K1.out`）。
- K2 soak 短模式：**不成立**。只失敗在時間檢查（`Client runtime gap/dropped time …`、`Clean Match runtime gap/dropped time`）；動作 87／87、死亡與重生 2／2 都正確（`K2/`）。
- K3 `run_network.py`：**不成立**。主迴圈與 life-cleanup 通過；failure probe 失敗，因為我沒有把 `--arena` 傳給它（`K3/failure-probe.log`）。

停下回報，使用者選「新宣告重跑」，核准重跑的判定（`v8-product-real-match-rerun-approve`）：

- K2′ 連跑 3 次都要整體通過：**通過**，3／3（`K2p-1..3`）。
- K3′ 套上修正後跑 1 次：**通過**（`K3p.out`）。
- 負載：K2′ 期間 load average 7～14，比試行 1 失敗時（約 3）高，仍然通過。所以試行 1 的時間失敗，不像只是負載造成。原因分不出來（§14.1）。這條 Match 時間檢查在本批改動之前（合成模型的 clean-60）也失敗過一次。
- K4（命中都是軀幹）：K1～K3′ 的分析器只接受 25000，全部通過。

## W2 的審查與突變（§14.4）

- **Sonnet 5.5**（PR #129 的規則下只算額外意見）找到兩點，都已修：
  - 複製整個 `assets`，多放寬了；
  - 工作資料夾裡有資產，「不從工作資料夾讀」的驗證會失效。
- **Fable 5.1**（審查者）逐項核對，結論是 1～5 和程式相符，有兩處要修，都已照改進宣告：
  - 1 的措辭：要守的性質不能再寫「不需要 asset catalog」；
  - 3 的範圍：新檢查只拿掉 movement rules。
- 審查者另外確認：
  - `MATCH_CONTENT` 和 `AppConfig::Assets` 相同；
  - Match 執行檔沒有連結 SDL（`otool -L`）；
  - 預設 arena 的檢查證明的是「不用 argv[0]、不用工作資料夾」，和以前相同。
- 突變（手動，這些 runner 不在 CTest）：
  - 瞄準改回 pitch 0：`clean-60` 失敗在 `Invalid v5 damage attribution`，killed（`mut-aim-pitch0.out`）。
  - 隔離副本少了 `characters/player/hitbox.json`：`run_network.py` 在啟動時失敗，killed（`mut-no-hitbox.out`）。

## 經緯

- 開工前查到：`run_gameplay.py` 是 #128 造成的回歸。S3 的宣告漏了它，因為我只查了 `action_main.cpp`，沒有查 v5 起實際在用的 runner。
- `run_gameplay.py` 經凍結的 `action_probe.run_case` 啟動 probe，雜湊寫死，不能加參數。所以 probe 從 arena 檔的資料夾找內容；repo 的 `assets/object_fps_pvp` 有自己的 `content.json` 與 catalog。

## 其他測試

- `test` preset 的全部 CTest（`-j4`）：119／119 通過，含 worker、`hit_part_cross`、`ingress_cross`（`ctest-full.log`）。

## 範圍的限界

- K1～K3′ 只在 Intel Mac 跑。Linux 與 Windows 另外安排。它們仍然不在 CI（L1 只跑 `cpu|shader`）。
- K2 試行 1 的時間檢查失敗，原因沒有分清。之後如果再出現，要先建檢測。
- Match 如果改成少了動畫或形狀時用預設值繼續跑，執行檔層級只有 movement rules 的檢查，發現不了（Fable 的指出）。
- GUI 的驗收工具（`run_action_short.py`、`combat_gui_evidence.py` 等）仍然假設每發傷害固定，歸 `v8-product-hit-presentation`。

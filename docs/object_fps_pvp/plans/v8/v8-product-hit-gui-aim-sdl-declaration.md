# v8-product-hit-gui-aim-sdl 的事前宣告

更新：2026-11-04。Owner：`object_fps_pvp`。**狀態：草稿，等使用者核准。** 估時 1～2 h（超過 3 h 停下回報）。檔位 medium（執行驗收與記錄）｜主會話 Opus 5.5；不開子 agent。

## 這批做什麼

P2（`v8-product-hit-gui-aim`）讓 SDL 的 17 格排程（`build/acceptance/object_fps_pvp/combat_latency.hpp`）每幀轉向對手的軀幹，但這條路徑沒有正式驗收：唯一用到它的 `run_timing.py --gui --combat` 一直報 HUD 不符。`v8-product-hud-tick-offset` 查明那是 v7 起已知的凍結分析器誤判（D83／D84）：HUD 改由 `combat_gui_evidence_v7.py` 判定。這批照這個規則正式跑驗收，讓 hitbox-line 的這一項結案。

不在這批：K8 試玩（等實機）；G5、G6（已在 Docker 的 L2 通過）。

## 開工前的事實

1. 探索跑次（不是驗收，2026-11-03，Intel Mac）：瞄軀幹與合成模型 Match 各 1 輪，凍結分析器只有 HUD 的 11 個畫面不符；v7 分析器重判兩輪都 passed、other_errors 0、flags 0。證據在主 checkout 的 `build/evidence/v8-hit-gui-aim/explore-timing-combat*/`。
2. `run_timing.py` 的整體結果（`overall_passed`）用凍結分析器，所以只要有 HUD 畫面就會判 false、程式以失敗結束，而且失敗後不會跑下一輪。這是已知行為（D84），本批不改 runner。
3. 部位錯了會被抓到：排程的分析器用每條生命的重播計算期望的 hp，每次命中扣 25000（Body）。打到頭（50000）或腿（18750）時，snapshot 的戰鬥狀態和重播不同，凍結分析器會報「snapshot combat state differs from the per-life replay」，這不是 HUD 錯誤，v7 也照判。
4. 這支只在本機跑，不在 CI（不屬於 `cpu|shader` 標籤）。本機負載高時 GUI 的時間檢查可能失敗（K3 的教訓），開跑前記錄負載。

## 判定（結果出來前宣告）

命令（每輪各自一個輸出資料夾，`--rounds 1`）：

```
run_timing.py --match <match> --gateway <gateway> --probe <gui_probe> --arena <pvp_arena.json> \
  --output <dir> --gui --combat --short --duration 20
python3 combat_gui_evidence_v7.py <dir>/round-1
```

- **S1（每輪）**：`round.json` 的 `passed`（移動）為 true，`presentation.passed` 為 true；v7 分析器 `passed` 為 true、`other_errors` 空、`flags` 空、`life_respawns_verified` 為 true。凍結分析器的錯誤只能是 HUD 不符的訊息（v7 的 `frozen_hud_errors` 之外沒有其他）。
- **S2**：連跑 3 輪，3 輪都要 S1 成立。不重試；任何一輪不成立就停下回報（§14.1），失敗的輪保留紀錄。
- **S3（負面對照，見選項 `-sdl-negative`）**：把 `combat_latency.hpp` 第 99 行的 `aim->pitch` 換成 `0.0f`（P2 之前的瞄法，會打到頭），跑 1 輪；預期凍結分析器報「snapshot combat state differs from the per-life replay」，v7 分析器 `passed` 為 false。若沒有失敗，表示這支驗收抓不到部位錯誤，停下回報。跑完還原。
- 每輪開跑前後記錄 `uptime`；開跑前 1 分鐘負載平均要低於 4。

## 證據

主 checkout 的 `build/evidence/v8-hit-gui-aim-sdl/`（Intel Mac）：每輪的資料夾、runner 輸出、v7 分析器的 JSON、負載紀錄。

## 選項（附建議）

| 代號 | 問題 | 選項 | 建議 |
|---|---|---|---|
| `-sdl-rounds` | 幾輪 | (a) 3 輪 ／ (b) 1 輪 | (a)：GUI 的時間檢查會受負載影響，3 輪才看得出是否穩定；多約 2 分鐘 |
| `-sdl-negative` | 要不要跑負面對照 S3 | (a) 跑 ／ (b) 不跑 | (a)：證明這支驗收對「部位錯了」有檢出力（§14.2）；約 10 分鐘（含重建） |

## 完成條件

S1～S3 成立；dev_log（`docs/dev_logs/`）與 HANDOFF、README 同一變更提交；P2 的限界「SDL 排程瞄軀幹沒有驗證」在 HANDOFF 標為已結案。

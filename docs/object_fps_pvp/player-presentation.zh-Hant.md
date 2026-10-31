# 遠端玩家的呈現（`PlayerPresentation`）

Owner：`object_fps_pvp`。程式：`apps/object_fps_pvp/include/RetroFPS/Pvp/PlayerPresentation.hpp`、`src/Pvp/PlayerPresentation.cpp`；Match 的權威動畫狀態在 `PlayerAnimation.hpp`（`match_domain`）。

## 職責

把 snapshot 時間軸取樣到的遠端玩家權威狀態，轉成畫面上的骨架姿勢，並交給 Renderer。步伐、權重、跳躍由 Match 決定（[protocol-v9](protocol-v9.zh-Hant.md) §4），這一層只負責播放。

- **輸入**：`PlayerPresentationFrame`，內容是時間軸取樣的位置、yaw、pitch、生命、戰鬥紀錄，以及 Match 的動畫內插值 `PlayerAnimationSample`。
- **狀態**：`PlayerLocomotionState`。只放呈現用的值：
  - 目前顯示的相位與權重；
  - holding 的凍結；
  - idle 時鐘；
  - 位移的診斷值與重設的觀測（驗收工具讀這些值）。
- **輸出**：骨架姿勢（`SamplePlayerPresentationPose`），以及觀測 `PlayerPresentationObservation`。

## Design Invariants

（2026-10-31 核准，`v8-product-anim-presentation-invariants`；落地於 `v8-product-anim-authority` A4。）

1. 遠端玩家的步伐相位、移動權重、jog 權重、跳躍階段與它的零點，只來自 snapshot 的 `animation`，也就是 Match 的權威。Client 不由位移、速度或呈現的時間累積它們。
2. Client 對它們只做 protocol-v9 §4.4 的內插（`InterpolatePlayerAnimation`，和 Match 的回溯共用），以及 holding 時的凍結。
3. 其他影響姿勢的輸入（瞄準、蹲姿、死亡、射擊、換彈、受擊、idle），都是 snapshot 權威欄位與呈現時間的純函式。同一組 snapshot 在同一個呈現 Tick，不論 frame rate，都得到同一組姿勢輸入。
4. 呈現的狀態不回流到判定，也不回流到任何權威狀態。
5. **Replacement Criteria**：出現下面任何一種情況時，停下來提出「改契約（加欄位或改內插規則）還是重建呈現層」，交給使用者決定。
   - 為了讓畫面看起來對，Client 必須自己修正或推測權威的動畫狀態（例如漏收時另外外插相位）；
   - 或者每加一種動作，都要同時改 Match 的狀態機與 Client 的呈現規則才能一致。

### 守衛

- 第 1、2 條：
  - `PlayerPresentationTests.cpp`：「PvP presentation follows the Match animation identically at 30 60 and 144 FPS」「PvP presentation reanchors its observations but keeps showing the authority phase」「PvP stop and turning show no phantom footsteps and a timeline hold freezes the pose」；
  - 驗收分析器 `player_presentation_evidence.py`：顯示的相位必須等於 `authority_phase_cycles`。這兩個值都由同一個取樣值算出，所以只抓得到 Client 自己的後處理錯誤。取樣本身由 `TimelineTests.cpp` 守住。`PvpApplication.cpp` 把取樣放進 frame 的那一行，目前沒有單元測試，只有 GUI 驗收的區段檢查會發現（A4 dev_log 的範圍限界）。
- holding 只凍結步伐（相位、`move_weight`、`jog_weight`），跳躍階段照樣跟著權威：原地起跳不會改變水平位置，會被時間軸判成 holding（A4 的 xhigh 審查 F1）。守衛：「PvP a timeline hold does not freeze the jump phase」。
- 第 3 條：上面第 1 項的 30／60／144 FPS 測試（姿勢輸入完全相同）；`ResolvePlayerActions` 的既有純函式測試。
- 內插本身：`PlayerAnimationTests.cpp` 的「PvP animation interpolation takes the signed phase, linear weights and the earlier jump phase」，以及 `TimelineTests.cpp` 的「PvP remote history samples the Match animation between its bracketing states」。

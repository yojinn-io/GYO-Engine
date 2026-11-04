# 架構評估請求：動畫事件 → 聲音的對應由誰持有（`v8-tool-anim-event-sound-request`）

日期：2026-11-04。提出：v8 工具線（audio_editor 的會話）。交給：架構師（例行架構掃描的會話）。
狀態：**等架構評估**。使用者說「這個問題是架構變動，整理一下，交給架構師評估。」A／B（下方）使用者還沒選。

## 功能（使用者的需求）

- 使用者常把換彈的聲音照動畫剪輯、合併。
- 想在 audio_editor 剪好，直接在 Model Viewer（`v8-tool-model-viewer`，anim-events-line 的下一批）裡和動畫一起預覽，不必進遊戲測試。

## 現況（事實與位置）

- PvP 的換彈音是一個剪好的片段 `object_fps_pvp.audio.gun_reload`。
- 動畫事件：`assets/object_fps_pvp/weapons/nailer_v2/viewmodel/viewmodel.anim_events.json`，在 `Armature|Reload` 的 2.35 s 有 `magazine_in`。
- 事件對應的聲音：`assets/object_fps_pvp/audio/sfx.json`（產品自己的格式，version 2），`"animation_events": { "magazine_in": { "cue": "reload", "sync_seconds": 0.33 } }`。意思是片段的第 0.33 s 對齊這個事件。
- 動畫事件的契約（`docs/architecture/animation-events.md`，owner 是 Engine 的動畫）：
  - 事件代表什麼、觸發什麼，由產品決定；格式與 `GYO::Model` 只負責攜帶。
  - PvP 會把換彈動畫拉長或縮短，配合權威端決定的換彈時間（同文件「Using it」）。
- audio_editor（`docs/audio_editor/architecture.md`）：
  - DI 3：不動產品的資料（例如音效 cue 表）；
  - DI 8：中立，不知道 cue 名稱；
  - Replacement Criteria：需要多軌或混音時間軸時，停下來決定修還是重建。
- Model Viewer 是中立工具（AGENTS §8、§9），計畫中 12～20 h，還沒開工。

## 觀察到的壓力

- Model Viewer 要預覽聲音，必須知道「哪個事件播哪個聲音、從片段的哪裡開始」。
- 這件事實現在只寫在 PvP 自己的 `sfx.json`：
  - 中立的 Model Viewer 不能讀產品的格式（AGENTS §9）；
  - 在 Model Viewer 自己的檔案再寫一份，會變成同一件事實手動維護兩處（AGENTS §9「同じ事実を手動で維持する場所は、一か所に限定する」）。
- 所以「事件 → 聲音」這件事實沒有一個通用的持有者。工具要用它時就卡住了。

## 聲音那一側的兩條路（使用者還沒選）

- **A. 拼成一個片段**（現在的做法）：
  - audio_editor 要做拼接時間軸，12～20 h，會碰到它的 Replacement Criteria；
  - PvP 拉伸換彈動畫時，片段只有一個對齊點，其他動作的聲音會偏掉。
- **B. 每個動作一個事件、一段聲音**（`magazine_out`、`magazine_in`、`bolt_pull` 等）：
  - audio_editor 只需要做到一個原檔切出多段、批次匯出，加淡入淡出（8～12 h），不需要時間軸；
  - 參考的成熟做法：Unreal 的 AnimNotify、Unity 的動畫事件，搭配 FMOD／Wwise 由事件觸發聲音；
  - GYO::Audio 本來就依事件時間戳排程播放。
- 不論 A 或 B，Model Viewer 的預覽都需要上面那件「事件 → 聲音」的事實。

## 想請架構評估的問題

1. 「事件 → 聲音（與片段內的對齊點）」是否該成為產品與工具共用的 Data Contract？owner 是誰？候選：
   - (a) 新的通用格式，owner 是 Engine 的音訊或資產；
   - (b) 擴充 `gyo.anim_events`（version 2）。但這會改動「事件代表什麼由產品決定」的原則；
   - (c) 維持產品自己持有，Model Viewer 不預覽聲音（照舊進遊戲測）；
   - (d) 其他。
2. 若成為契約：PvP 的 `sfx.json`（`animation_events` 段）要怎麼改，舊資料怎麼處理？由誰執行（產品會話）？
3. 這個決定對 A／B 的影響：B 是否在契約上比較自然？A 是否等於觸發 audio_editor 的 Replacement Criteria？
4. 影響的 owner：Engine（動畫或音訊）、PvP（`sfx.json` 與讀它的程式）、audio_editor（下一批的範圍）、Model Viewer（聲音預覽，anim-events-line）。

## 等這個決定的工作

- audio_editor 的下一批：多區段（B）或拼接（A）。#141 合併之後才開。
- Model Viewer 的聲音預覽：anim-events-line，在 `v8-tool-model-viewer` 本體之外約 4～6 h。
- PvP 的 `sfx.json` 與讀它的程式（如果契約改了）。

## 本線不做的事

- 本線不改契約、不改 PvP 資料、不改 Model Viewer。
- 架構評估出來、使用者決定之後，本線只寫 audio_editor 那一批的事前宣告。

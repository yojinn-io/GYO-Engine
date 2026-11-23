# 2026-11-24 Object_Connect 移植實作第 2 段：聲音（`v8-product-object-connect-port`）

Owner：產品（Object_Connect）。GYO2 主會話開的任務卡（使用者「開 Object_Connect 移植實作 卡」；主會話 2026-11-24 轉告 A #306 已合併、第 2 段可以開始）。分支 `claude/gyo2-object-connect-audio-d5884a`（開向 `gyo2`，疊在第 1 段 #314 上）。作者 Opus 5.5／high。
取捨寫在盤點第 9 節。

## 做了什麼

1. `runtime/GameSound`：`ConfigureProduct`（登記 loader，含 `WavLoader`；從 `sdl_audio` 的 `Mixer` 建 `Submitter` 放進共用處的 `SoundPort`）、`PlaySounds` System（`Update`，`PresentMenus` 之後）：BGM 序奏＋迴圈、拿線時壓低（`SetGain`＋ramp）、拉線音淡出後停、放開音、回主選單時重播。
2. `GameContent` 加 4 個音檔（可有可無，失敗只警告）。
3. 主程式帶 `sdl_audio`；裝置開不了時記 Warning 照跑，`--verbose` 印驅動名。
4. 測試：`object_connect.runtime` 加 7 個情境（審查後）（測試自己建 `Mixer` 放在 `sdl_audio` 放的位置、`Render` 一個 block 後讀 `Stats`）：開始與壓低與拉線音／放開音、回主選單重播、沒有混音器時靜音照玩。原 `GameAudioTests` 依新寫法改寫成這三個。

## 驗證

- 本機（Intel Mac）：`object_connect.*` 4/4（連跑 5 次）；全量 `ctest -L cpu` 229/229（審查前）。
- 實機：`gyo_object_connect --frames 240 --verbose`：`sound on the coreaudio driver`、進主選單、exit 0。**聲音本身沒聽**（這個會話聽不到）；壓低、淡出的聽感交使用者。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

blocker 0、should 6、nit 7。處理：

- should：壓低中途反轉時，遊戲記的值與混音器實際的增益差了延遲（60 ms），盤點寫的「曲線相同」不對 → ramp 改照混音器的時間線算（`PlanDuck`、`GainRamp`），用 Engine 時間基準；盤點寫明剩下的差別（比延遲短的拿線不會壓低）。
- should：放手後 0.25 s 內回主選單，重播的 BGM 停在半壓低 → 重播時補送 `SetGain` 接完 ramp。
- should：測試沒驗增益與時間、取消時不播放開音、缺音檔 → 補 `PlanDuck` 的四種情形、取消、回復中重播、缺序奏的內容根（測試複製 assets 刪一個 wav）；停兩個聲部改成精確等於。手動突變 4 個（取消也播放開音、重播不補 `SetGain`、攻放速率對調、拿掉空 clip 防護）全被抓到。
- nit 處理：淡出中的舊拉線音在重按時切掉；壓低改用 Engine 時間（不受遊戲時間暫停、縮放影響）。
- nit 不處理：見盤點第 9 節末。

## 盤點的更正

- G2 那列寫「拉線音迴圈」不對：原版拉線音以 `loop=false` 播（`GameAudio.cpp:158`）。只有 BGM 的迴圈段迴圈。

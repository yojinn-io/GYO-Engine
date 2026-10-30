# v8-product-sfx：PvP 的音效（進行中）

日期：2026-10-30。**進行中**：音效已接上，L1 完成；停在 L2 宣告，等使用者核准。Engine 部分見 [audio 計畫](../architecture/plans/audio/README.md)。v8 計畫（`docs/object_fps_pvp/plans/v8/`）由產品會話同步，本批不改。

## 為什麼做

- v8 的 audio-line 的產品批（high，估 8～12 h）。使用者原話：「繼續」「遠端槍聲＋命中 hit_flesh」。

## 開工前的決定（使用者，2026-10-30）

| 項目 | 決定 |
|---|---|
| 資產來源（v7 D38） | 使用者提供 4 個音效（射擊、受擊、換彈、腳步）；我轉成 48 kHz PCM16（「這個你先轉好，需求加到 assets editor 中。」） |
| 本機射擊的時刻 | 按下事件的 `InputEvent::timestamp` |
| 遠端射擊、命中、受擊的時刻 | 以 `SnapshotTimeline` 把事件 Tick 換成該 Tick 被顯示的時間 |
| 腳步 | 依距離觸發＋落地聲 |
| 用途對應 | 遠端射擊用槍聲（依方向與距離），命中確認用 hit_flesh（較小聲） |

## 資產的來源與轉換

| 檔案 | 用途 | 原檔 |
|---|---|---|
| `assets/object_fps_pvp/audio/gun_shoot.wav` | 本機與遠端射擊 | 44.1 kHz、32 位元整數、立體聲，0.320 s |
| `assets/object_fps_pvp/audio/hit_flesh.wav` | 受擊、命中確認 | 同上，0.365 s |
| `assets/object_fps_pvp/audio/gun_reload.wav` | 換彈 | 同上，0.958 s |
| `assets/object_fps_pvp/audio/player_footstep.wav` | 腳步、落地 | 同上，0.179 s |

- 來源：2026-10-30 使用者提供（主 checkout 的 `assets/object_fps_pvp/audio/`，未追蹤）。授權：未記錄，待使用者補上。
- 以 `docs/architecture/plans/audio/scripts/convert_wav.py <原檔> <輸出>` 轉成 48 kHz PCM16（窗化 sinc、固定種子的三角抖動，可重現），沒有飽和。
- 原檔不放進 repo：產品的內容契約要求資產根目錄下每個檔案都在 catalog（`tests/object_fps_pvp/test_asset_catalog.py` 的 `test_every_asset_file_is_in_the_catalog`）。第一次我把原檔放在 `audio/source/`、說明放在 `audio/README.md`，這個測試失敗（commit `30b36bdb` 造成的回歸），已移出。原檔該放哪裡交給 assets editor 的規劃。
- 注意：主 checkout 裡未追蹤的原檔和轉好的檔同名同路徑；合併本分支後在主 checkout 更新前，要先把原檔移走，否則 git 會拒絕覆寫。

## 做了什麼

- **內容**：`assets/object_fps_pvp/audio/sfx.json`（各提示的片段與音量、空間化參數、步距；改內容不必改 C++），catalog 加 5 筆（設定 1、音效 4，type `sound`）。
- **component**：`project.json` 宣告 `SDL_AUDIO`；`app_support` 連結 `GYO::Audio` 與 `SDL_AUDIO`。Match 的連結閉包沒有 `gyo_audio` 與 SDL（以 ninja 的連結命令確認）。
- **`SoundCues`（domain，純邏輯，不依賴音效）**：
  - `ParseSoundSettings`：解析 sfx.json。
  - `CentredGains`、`SpatialGains`：等功率聲像（方位相對視角 yaw，與 HitFeedback 同一慣例）＋距離衰減（參考距離內全音量，之後 參考／距離，不低於最小增益）。
  - `RemoteShotTracker`：每個遠端玩家的 `lastShotActionId` 變了就是新的一槍；每條命的第一個狀態不重播。
  - `FootstepTracker`：在地面上每走一個步距響一次，著地響一次落地聲；瞬移、新的一條命、失去身體時靜默重來。
- **`PvpSound`（app_support）**：載入片段到 `ClipBank`、建立混音器與一個提交端（所有提示都在主執行緒偵測）、開 SDL 輸出。開不了裝置時記一行日誌、維持無聲；片段不合格式是內容錯誤。結束時記一行計數（`PvP sound summary`）。
- **時刻**：
  - `PointerCapture` 記下造成射擊的那次按下的時間戳（`pendingShotPressedAt`）。
  - `SnapshotTimeline::DisplayTime(tick)`：取樣游標的反函數。
- **觸發點**（`PvpApplication.cpp`）：
  - 本機射擊：動作送出成功時，時刻＝按下時間。
  - 換彈：換彈動畫開始時，時刻往前推已經過的權威時間。
  - 受擊：`HitFeedback` 判定 Played 時，時刻＝`DisplayTime(lastDamageTick)`，從攻擊者方向。
  - 命中確認：決定被接受且打中玩家時，時刻＝`DisplayTime(resolvedTick)`，從目標位置。
  - 遠端射擊：每個抵達的 snapshot 依序偵測，時刻＝`DisplayTime(lastShotTick)`，從射手位置。
  - 腳步：本機在每幀取得呈現位置之後；遠端在 `PrepareWorld` 用內插位置。

## 驗證

- L1：`SoundCuesTests.cpp` 5 案（設定、聲像與衰減、遠端射擊、腳步、`DisplayTime` 是取樣游標的反函數且間隔恰為整數 Tick），`PointerCaptureCharacterizationTests.cpp` 加 1 案（射擊保留第一次按下的時間戳）。
- 建置：test 組態沒有錯誤與警告。
- 完整 `ctest --preset test`：第一次 96／97，`object_fps_pvp.asset_catalog` 失敗（原檔放在資產根目錄下，見「資產的來源與轉換」）；移出後內容測試 1／1 通過，其餘 96 個那一輪都通過。
- 打包：`build/target/object_fps_pvp/bin/assets/object_fps_pvp/audio/` 有 4 個 wav 與 sfx.json。
- 實際啟動用戶端 5 秒（大廳，沒有對局）：`PvP sound started driver=coreaudio device_frames=1024 schedule_delay_ms=60`，結束時 `PvP sound summary submitted=0 …`，正常退出。對局中的聲音還沒有實機確認（L2／L3）。

## L2 宣告（草案，等使用者核准；看結果前不改）

見 HANDOFF 或回報中的宣告；核准後寫進本節並執行。

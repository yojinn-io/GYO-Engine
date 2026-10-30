# v8-product-sfx：PvP 的音效

日期：2026-10-30。**完成到 PR**：音效已接上，L1、突變、使用者試聽與戰鬥短測完成；產品 L2 沒有執行（宣告草案未核准）。Engine 部分見 [audio 計畫](../architecture/plans/audio/README.md)。v8 計畫（`docs/object_fps_pvp/plans/v8/`）由產品會話同步，本批不改。

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

- 來源：2026-10-30 使用者提供（主 checkout 的 `assets/object_fps_pvp/audio/`，未追蹤）。授權：使用者自製（使用者原話：「那個是我自己混剪的。」）。
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

- 突變（`tests/object_fps_pvp/mutations.json`，batch `v8-product-sfx`）：8／8 被殺（聲像左右顛倒、無最小增益、遠端第一個狀態重播、空中也算步數、瞬移算步數、顯示時間少了延遲、舊 Tick 以無號數換算、射擊保留最後一次按下）。
  - 經緯：`sfx-display-time-unsigned` 第一次存活。無號數回繞後的巨大秒數轉成時間點時溢位，結果恰好仍「比較早」，只比較大小抓不到。測試改為檢查確切的值後被殺。

## 使用者試玩（L3，2026-10-30）

- 單人一場約 50 秒（用戶端日誌 `client-20261030-203938.log`）：`PvP sound summary submitted=69 on_time=69 late=0 queue_full=0 no_voice=0 … local_shot=29 reload=5 footstep=35 landing=0`。收支一致，沒有遲到。遠端射擊、受擊、命中要兩個玩家，未測。
- 使用者：「reload 的最後一聲太早了和動畫對不上，聲音先出來，彈倉動畫才插入，大概快了500ms左右。」
  - 原因：權威端換彈是 90 Tick（1.5 s），片段 0.958 s；聲音從換彈開始播，最後一聲裝填約在 0.9 s，動畫在接近 1.5 s 插入彈倉，差約 0.54 s。
  - 修正：換彈聲改為結尾對齊換彈結束，開始時刻＝權威開始＋max(0, 換彈長度−片段長度)（`ReloadSoundOffsetSeconds`，加 1 案與 1 個突變）。待使用者再聽一次確認。
- 使用者：「我發現出現了掉線提示（只有移動，射擊後提示消失）」
  - 那是「CONNECTION POOR」提示，不是斷線（`interruptions=0`、最長空窗 157 ms）。用戶端 `20:40:13` 收到 `failed_windows=1 of 3`、`20:40:23` 回到 0，提示隨之消失，和射擊無關。
  - Match 同一窗口：`quality window failed … movement_resets=1 … movement_resets_failed=1`，Tick 晚醒最多 136 ms（`tick_stalls=1`）。
  - 判斷：Match 端的一次停頓造成一次移動重設，品質窗口以「1 次重設即失敗」判定。Match 的連結沒有音效。沒有在不含音效的版本重現，所以不能斷定完全無關；屬網路／連線品質的範圍，不在本批修，交給使用者決定。

## 第二次試玩（2026-10-30，兩個用戶端）

- player 1 約 1.5 分鐘（`client-20261030-204937.log`）：`submitted=159 on_time=159 late=0 … local_shot=94 reload=25 footstep=40`，連線無中斷（`max_gap_ms=21`），沒有出現 CONNECTION POOR。遠端射擊、受擊、命中仍為 0（沒有互相射擊）。
- 使用者：「有一些違和感，這個聲音和動畫的管理是資料化的吧，是的話，就給Assets Editor來處理。」
  - 現況：片段、音量、空間化、步距是資料（sfx.json）；聲音對到動畫的時刻不是資料。換彈的對齊是 C++ 的過渡規則（`ReloadSoundOffsetSeconds`），動畫片段沒有事件時刻的標記。
  - 處理：需求「在動畫上標記事件時刻、音效掛在標記上、Assets Editor 編輯、格式是 Data Contract」傳給工具線（`v8-tool-assets-editor`）。過渡規則保留到那時。

## 戰鬥短測（2026-10-30，代替單機無法測的遠端聲音）

- 使用者：「主要是那兩個聲音，一台機器無法測試。」→ 以兩個 GUI 探針自動互相射擊：`run_timing.py --gui --combat --short --duration 16 --events 20`（Gateway 以 `go build` 建在 scratchpad）。證據：`build/target/_build/test/logs/sfx-combat-short-20261030/`（本工作樹）。

| | 射擊方（create） | 被射方（join） |
|---|---|---|
| 送出／準時／遲到／丟棄 | 38／38／0／0 | 35／35／0／0 |
| 本機射擊 | 15 | 0 |
| 遠端射擊 | 0 | 15（和對方的 15 發一致） |
| 命中確認 | 11 | 0 |
| 受擊 | 0 | 9 |
| 換彈 | 1 | 0 |
| 腳步 | 11 | 11 |

- 受擊 9 對命中 11：被射方觀察到 11 次受擊，9 次 `reason=played`、2 次致命一擊 `reason=dead`。受擊聲沿用 `HitFeedback` 的「死亡時抑制」，所以被打死的那一下沒有受擊聲。是否改成致命一擊也響，交給使用者決定。
- 驗收結果 `passed: false`，兩個原因都與音效無關：
  - HUD 不一致 18 筆：`run_timing.py` 用的是凍結的 v6 分析器，這條規則從 v7 第 04 批起意義改變。以 `combat_gui_evidence_v7.py` 重判同一份證據：`passed: true`、`errors: []`。
  - `invalid_window_disturbed`：視窗受到干擾，是這台機器的環境因素。
- 本次是探索用的試跑，不是 L2 判定。
- 使用者：「致命一擊也響」。受擊聲改為新的受擊與致命一擊都響、新的一條命的第一個狀態不響（`HitTakenSounds`）；閃紅維持死亡時抑制。加 1 案、2 個突變（2／2 被殺）。
  - 經緯：修改後 `object_fps_pvp.cpu` 在 CTest 的 60 秒上限逾時。直接執行 340 案全過、耗時 61.7 s（user 46 s），當時負載 21（另一個會話的建置佔滿 CPU，前幾名都是 clang）。切分為環境因素；之前完整 CTest 那一輪它在上限內通過。突變的基準在同樣負載下通過。

## 合併 master 與 PR

- 使用者：「開PR推送」「我出去吃些東西，把這個audio-line 給完成了吧。」
- 併入 origin/master（含蹲下 `v8-product-crouch`）：`tests/object_fps_pvp/CMakeLists.txt`（兩邊各加一個測試檔）與 `mutations.json`（兩邊各在末尾加項目）衝突，兩邊都保留（master 706 項＋本批 11 項）。
- 合併後本機：`ctest --preset test` 102／102、`ctest --preset core` 37／37；本批與 Engine 的突變 find 字串都還在。

## L2 宣告（草案，等使用者核准；看結果前不改）

- 目的：本機射擊的起音跟著按下的時刻，不落在 30 FPS 的幀格點上。
- 場次：本機 Gateway＋Match＋GUI 用戶端（既有的 GUI 探針，`--fps 30`）與 bot；`SDL_AUDIO_DRIVER=disk`、每塊 480、timescale 1。
- 刺激：從另一個執行緒，在不對齊幀的時刻注入左鍵點擊（間隔以 350／410／470 ms 循環，皆大於射擊冷卻），每場 60 次。
- 需要新增：用戶端的診斷開關（環境變數）讓輸出記錄錨點並寫檔，以及每個本機射擊提示的時刻寫一行日誌。
- 判定（每場；3 場，不重試，任一場不成立即不成立）：
  1. 收支：submitted＝準時＋遲到＋丟棄，丟棄為 0；本機射擊提示數＝用戶端記錄送出成功的射擊數。
  2. 每個本機射擊的起音都落在由錨點重建的 frame（100%）。
  3. 本機射擊起音在 Engine 時間下的間隔，與按下時間戳的間隔相差小於 1 個 frame（20.8 µs）。
  4. 本機射擊的遲到為 0。
  5. 有效性：至少一半的按下時間戳離最近的幀開始超過 2 ms（確認注入不在幀格點上）；不成立則該場無效，停下回報。
- 估時：3～4 h。

# Engine v8-engine-audio-plan：建立音效計畫夾

日期：2026-10-30。**完成**：建立 Engine 的音效計畫夾 [docs/architecture/plans/audio/](../architecture/plans/audio/README.md)（README、PLAN、HANDOFF），使用者同日核准 6 項待決事項全部照建議。沒有動程式。

## 為什麼做

- 消費端 v8 把音效排第一優先；它的 audio-line 第一批是本批（high，估 3～5 h）。
- 消費端 v7 D28（2026-10-14 依 D76 改到 v8）：audio 計畫夾在音效批開始時建立。

## 做了什麼

- 讀消費端 v8 計畫的 audio-line 三列與「已決定的項目」的音效一項、v7 交接的「P6：音效」與 D28、D38、D76。
- 讀 `engine/asset` 的 `SoundLoader`、[platform-sdl.md](../architecture/platform-sdl.md)、[time.md](../architecture/time.md)，以及 SDL 3.4.0 的音效原始碼（預設區塊大小、`disk` 與 `dummy` 驅動）。
- 寫計畫：
  - 範圍：中立混音器（48 kHz、PCM16、立體聲、自己的樣本計數）、輸出前登錄的不可變片段庫、一次性播放請求、以每區塊的錨點把 Engine 時間戳換成樣本位置、有界 SPSC 提交端、收支計數、SDL 後端。不做串流、迴圈、效果器、重新取樣。
  - Owner 與依賴：`GYO::Audio`（Base、Time）與 `GYO::AudioBackendSDL`（Audio；SDL3 PRIVATE），不依賴 `GYO::Engine` 與 `GYO::PlatformSDL`；Architecture Delta 7 項。
  - 公開介面草案、Design Invariants 9 條、Replacement Criteria 6 條。
  - 分批：`v8-engine-audio-core` → `-schedule`（局部 xhigh）→ `-sdl` → `-record`，合計 12～18 h，引擎全部繼承。
  - 驗收：L1 以離線 `Render` 與 `dummy`／`disk` 驅動；L2 構想以 `disk` 驅動量起音。
- 核准（使用者原話：「audio 計畫的 6 項待決事項都照建議核准，記進 HANDOFF 並寫 dev_log，然後停下。」）：producers＝每個生產者執行緒一個 SPSC；clip-format＝`ClipBank` 複製、非 48 kHz 回錯、`SoundLoader` 測試在 core 補；delay＝可設定、預設值在 sdl 批實測後提案；late＝區塊開頭立刻播放並計數；l2＝Engine 自有 L2（只記錄、每塊 480 樣本、以錨點估速率）；gears＝照計畫、不用 ultracode。記在 HANDOFF 的決策紀錄。

## 驗證與經緯

- 只改文件，沒有建置或測試。
- 發現：SDL 的 `disk` 驅動每塊之後以整數毫秒相對等待（`SDL_diskaudio.c:123`、`:35`），48 kHz 預設 1024 樣本時截成 21 ms，檔案時間軸比真實時間快約 1.6%，晚醒又讓它變慢。這會讓起音間隔偏同樣比例，因此 L2 固定每塊 480 樣本並估計速率（`v8-engine-audio-l2`）。
- 時間：開始時重估 3 h，實際約 1.7 h（含核准紀錄）。

## 證據

- 計畫：[README](../architecture/plans/audio/README.md)、[PLAN](../architecture/plans/audio/PLAN.md)、[HANDOFF](../architecture/plans/audio/HANDOFF.md)（研究事實 1～7 附檔案與行號）。
- 分支 `claude/v8-engine-audio-95a496`，未推送。

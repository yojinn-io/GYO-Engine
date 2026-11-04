# v8-product-event-sounds-migrate 的事前宣告

更新：2026-11-05。Owner：`object_fps_pvp`。**狀態：草稿，等使用者核准。** 估時 4～6 h（超過 9 h 停下回報）。

上段（理由：PvP 的資料格式 `sfx.json` 升版；這批是 framework DI 6「consumer 的遷移批次要審查」點名的那種批次）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | 主會話 Opus 5.5／high | 既有設計的遷移、多檔修改 |
| 審查者 | 子 agent Opus 5.5／high（general-purpose，唯讀） | 上段不用小模型；和實作者同模型，因為是 code review 不是敵對檢查 |
| 檢驗者（§14.4） | 不設 | 這批不改受け入れ判定或 Design Invariants 的守衛測試（見「測試判定的變更」） |

## 這批做什麼

架構卡 `v8-arch-event-sounds-contract`（PR #145，2026-11-04 合併）讓 framework 持有「動畫事件 → 聲音」的組合格式 `gyo.event_sounds` v1。這批是卡片「給下游的交接」裡的 PvP 遷移：PvP 不再自己寫這份對應，改讀 framework 的格式。**玩家聽到的不變**。

支援的 Feature：Model Viewer 配的聲音能在遊戲裡聽到。Model Viewer 的試用記錄（PR #146 分支的 `docs/tools/plans/v8/model-viewer-trial.md`）把 L3 第 5 項記成「做不到，PvP 不讀 `gyo.event_sounds`」，這批解開它。

不在這批：
- B 的內容（`magazine_out`、`bolt_pull` 等更多事件與多段聲音）。需要 audio_editor 匯出的片段，另開。
- Model Viewer 與 audio_editor 的程式。
- framework 與 Engine（不改）。

## 開工前的事實（master `4a88f67f`）

1. PvP 的對應寫在 `assets/object_fps_pvp/audio/sfx.json`（version 2）的 `animation_events` 段：`magazine_in` → cue `reload`（clip `object_fps_pvp.audio.gun_reload`，volume 0.8），`sync_seconds` 0.33。
2. 讀它的是 `SoundCues.cpp`（`ParseSoundSettings`、`FindEventSound`）；排程在 `PvpApplication.cpp` 換彈那段，用 PvP 自己的 `EventSoundOffsetSeconds`；「對齊點不超過片段長度」在 `PvpSound::Initialize`。
3. `WeaponPresentationDefinition` 與 `PvpSound` 都在 `app_support` 目標裡，它已宣告元件 `FRAMEWORK`（IU-3 起，連 `GYO::ToolFramework`）。`ParseEventSounds` 在 JSON 綁定 `GYO::ToolFrameworkJson`，PvP 還沒連它。framework DI 1 說綁定和核心一起建，DI 2 允許「game → framework」。
4. `reload` cue 只被 `animation_events` 用到（換彈聲沒有其他觸發點）。
5. `PvP sound summary` 行有 `reload=` 計數。這一行不在 [log 契約](../../log-contract.zh-Hant.md)的受保護表裡，repo 裡沒有讀者（grep 只有寫出端 `PvpSound.cpp`）。
6. `sound_shots`（L2）的 `sound-cues.csv` 用 cue 名稱記每次播放；`sound_l2.py` 只讀 `local_shot` 那幾行，和 `sfx.json` 的 `cues.local_shot.volume`。
7. 事件的時刻在 `viewmodel.anim_events.json`（master 2.35 s；PR #146 改成 1.887 s，使用者決定保留）。這批不動這個檔，測試也不寫死時刻。
8. Model Viewer 試用文件建議的新檔 id 與路徑：`object_fps_pvp.event_sounds.nailer_v2.viewmodel`、`weapons/nailer_v2/viewmodel/viewmodel.event_sounds.json`。這批沿用。

## 改什麼

| 檔 | 改動 |
|---|---|
| `weapons/nailer_v2/viewmodel/viewmodel.event_sounds.json`（新） | `gyo.event_sounds` v1：`animation_events_asset_id` 是武器的 anim_events id；`magazine_in` → `object_fps_pvp.audio.gun_reload`、`sync_seconds` 0.33、`gain` 0.8（原 cue 的 volume）。手寫 |
| `asset_catalog.json` | 登錄上一列的檔，type `text` |
| `nailer_v2_viewmodel.json` | 加 `event_sounds_asset_id` |
| `audio/sfx.json` | version 3：刪 `animation_events` 段與 `reload` cue |
| `WeaponPresentationDefinition` | 有 `event_sounds_asset_id` 時載入文字、`ParseEventSounds`。核對它點名的 anim_events id 等於武器的 `animation_events_asset_id`（framework 不知道 id，呼叫者核對）。保留解析後的事件集合給綁定 |
| `PvpSound` | 載入每個事件聲音的片段、記下長度，用 `BindEventSounds` 綁定（片段長度由它回答）。新增「播事件聲音」：用綁定結果的 gain，置中 |
| `PvpApplication` 換彈段 | `Find(事件名)` → framework 的 `EventSoundOffsetSeconds` → 播事件聲音 |
| `SoundCues` | 刪 `EventSound`、`FindEventSound`、`EventSoundOffsetSeconds`、`Reload` cue；版本檢查改 3 |
| `app_support` 的 CMake | `PRIVATE_LIBRARIES` 加 `GYO::ToolFrameworkJson` |
| 診斷與 log | `sound-cues.csv` 的事件聲音以 `event.<事件名>` 記一行（觀測不消失）；summary 依選項 `-log` |

## 判定（結果出來前宣告）

- **E1（產品資料能綁定，`cpu` 測試，讀真實檔）**：新檔經 `ParseEventSounds` 成功。它點名的 anim_events id 等於武器設定的那個。`BindEventSounds` 用 Engine 解析的真實 anim_events 和 `gun_reload` 的真實長度綁定成功。`Find("magazine_in")` 給 `gun_reload`、0.33、0.8。取代舊測試「PvP animation event sounds name events of the weapon's gyo.anim_events」。
- **E2（壞資料在啟動時被拒，`cpu` 測試）**：下列各自讓載入回報錯誤、不靜默：
  - event_sounds 點名別的 anim_events id；
  - 事件名不在 anim_events 裡；
  - `sync_seconds` 超過聲音長度；
  - `sfx.json` 是 version 2；
  - `sfx.json` 還帶 `animation_events` 段（version 3 不認這個欄位）。
- **E3（玩家聽到的不變）**：
  - 同一份真實資料下，新路徑的開始時刻等於舊公式 `event × (action / clip) − sync` 的值（`cpu` 測試）；
  - 音量等於舊 cue 的 0.8，置中；
  - `apps/object_fps_pvp` 裡沒有 `EventSoundOffsetSeconds` 的定義（grep 0 件，DI 6）。
- **E4**：全量 `cpu|shader` 通過；L1 三平台通過。
- **E5（突變，見下）**：宣告的突變全部 killed，stale 0。D184 的重跑範圍也全部 killed。
- **E6（選項 `-listen`）**：使用者在 macOS 開遊戲換彈，聽起來和遷移前一樣。

## 測試判定的變更（§14.4 的說明；這些不是受け入れ判定，也不是 DI 守衛）

| 舊判定 | 去向 | 緩的面 | 由誰守住 |
|---|---|---|---|
| `SoundCuesTests` 的偏移案例（4 個 CHECK、4 個 CHECK_FALSE） | 刪，函式已搬走 | 無：framework ES9 涵蓋同樣的案例，並多了溢位 | `tests/framework/EventSoundsTests.cpp` ES9、`es-offset-*` |
| `rejects("no animation_events")`、`"an unknown cue"`、`"a negative sync point"`、`"an empty event name"` | 刪：欄位不存在了 | 規則改由 framework 判定 | ES3～ES8，加上本批 E2 的產品路徑 |
| 「event sounds name events of the weapon's anim_events」 | 改成 E1 | 無：E1 多了 sync 不超過長度、id 核對 | E1 |

## 突變

- **退休 6 條**（`tests/object_fps_pvp/mutations.json`，`file` 是 `SoundCues.cpp` 的 `anim-events-*`）。原因：程式刪了。
  - 偏移 3 條 → framework 的 `es-offset-*`；
  - version 1 → 新的 version 2 那條；
  - unknown cue → 新的 binding 那條；
  - find-wrong-event → 新的查找那條。
- **新增**（預檢：每條 find 恰好一次）：
  1. 不核對 event_sounds 點名的 anim_events id；
  2. 綁定失敗時照樣繼續；
  3. `sfx.json` version 2 仍被接受；
  4. 事件聲音的 gain 不用檔案的值（固定 1.0）；
  5. 查事件時用錯名稱；
  6. 排程忽略 sync（直接用事件時刻）。

  實作時若發現等價突變，記錄後不列。
- **D184 重跑範圍**：find 落在本批改動檔（`SoundCues.cpp`、`PvpSound.cpp`、`PvpApplication.cpp`、`WeaponPresentationDefinition.cpp`、`apps/object_fps_pvp/CMakeLists.txt`）的既有突變；殺它的測試被改的（`SoundCuesTests.cpp`）。PR 線合併前另跑完整一次（背景）。

## 選項（附建議）

| 代號 | 問題 | 選項 | 建議 |
|---|---|---|---|
| `-listen` | 要不要實聽 E6 | (a) 要，約 5 分鐘 ／ (b) 不要，靠 E3 | (a)：排程的呼叫處沒有單元測試，上一次（`v8-tool-assets-anim-events`）也靠實聽收尾 |
| `-log` | summary 的 `reload=` | (a) 刪 `reload=`，行尾加 `event_sounds=`（事件聲音的播放次數） ／ (b) 保留 `reload=`，意思改成事件聲音的次數 | (a)：(b) 是改意義且名稱誤導；這一行不受保護、沒有讀者（事實 5） |

## 證據

主 checkout 的 `build/evidence/v8-event-sounds-migrate/`（Intel Mac）：CTest 輸出、突變輸出、grep 結果。

## 完成條件

E1～E5 成立，E6 依選項。dev_log（`docs/dev_logs/`）、HANDOFF、README 與這批同一變更提交。HANDOFF 的 event-sounds 條目標為完成。告訴 Model Viewer 會話「遊戲裡聽到」已可試。

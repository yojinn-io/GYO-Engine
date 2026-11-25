# 2026-11-26 大地圖 S4-C：建 World 時才讀地圖（`v8-arch-gyo2-big-map-s4c`）

Owner：大地圖線的任務卡。分支 `claude/gyo2-big-map-s4c-world-load`，從 gyo2 `425d192c` 分出，之後 merge `origin/gyo2`（含 #355）一次，開向 `gyo2`。作者 Opus 5.5／high。宣告：`big-map-declaration.zh-Hant.md` 第 15.6 節。

## 前提

- 使用者：「大地圖卡接著做 S4-C 和 S6」（主會話轉達）。
- 選圖接點用流程框架卡做好的 `SceneChange::map` → `ArenaOf()` → `WorldOptions::arena`；`MapLoad` 的欄位和先前說的不同（`stage` 改 enum、多 `failure`），動工前已通知流程框架卡，對方沒有異議，另外提醒四點（中止要按得到、重進帶 `room`、不反覆重進、單人回單人房間），都已照做。

## 做了什麼

1. **啟動時不再讀地圖**：`GameContent` 只留 arena 檔、規則、角色；`InstalledArena`、`ContentRef`、`PresentationContent::arenaPictures` 拿掉。arena 檔的 `map` 改成 catalog 的 id（`object_fps_pvp.map.<名>`）。
2. **每個 World 在生成期間讀自己那張圖**（`world/ArenaLoad.hpp`、`PvpWorld.cpp` 的 `LoadArena`）：
   - 向 asset 要索引；region 全到後 `CheckVoxelMap`（`ReadMap`）。
   - 只把碰撞盒複製給 worker（`WorkerTask`，Bevy async_compute 的形狀）建 `BoxSet`，同時放掉 handle。
   - 建好後檢查出生點，插入 `kArenaEntity` 的 `Collider`，寫 `wallsDigest`；權威端寫 `MatchInfo`。
   - 完成的那一幀不 `Hold()`；失敗時維持生成，`MapLoad::failure` 寫原因。
3. **固定步在生成結束前不跑**（`WorldTimeOptions::fixedStepConditions`）；`SeatPlayers` 在生成期間不讀連線訊息，牆好了才入座。
4. **Client 的 Frame World**：`ShowArena` 改成生成期間讀同一個 asset、在 worker 做 region 模型；loading 等兩邊的 `MapLoad`，region 數算進進度與文字（「地図を読み込んでいます…（n/m）」「地図を組み立てています…」）。
5. **Match 玩別張圖**：`FollowMatchArena` 只記 `ContentMismatch{reason, matchArena}`；`LeaveOnEnd` 在有那張圖時以那張重進 MultiGame（帶 `connect`、`room`），否則帶原因離開；單人的地圖失敗回單人房間並帶回 `map`。
6. **Match 的 main**：WorkerPool（1 條）＋ `AssetOptions`、`ConfigureGame` 登記 voxel_map 的 loader；地圖失敗時記錄並以 1 結束，完成時記「the map is ready (N regions, M ms)」。Client 的 `--verbose` 也記同一行。

## 測試

- 新增 `world/MapLoadTests.cpp`（5 案）：生成直到 region 與牆都好、完成那幀結束、進度不倒退、生成期間 tick 為 0；缺 region、sha 不合、出生點在牆裡 → Failed 帶原因、維持生成；生成期間連進來的 Client 牆好後才入座；Match 牆建好後不再持有地圖。
- 測試用的 asset executor：World 測試用同步執行的 `InlineJobs`（生成的幀數固定）；Client 測試照舊用 WorkerPool，開始前用 `Setup::Loaded()` 等到生成結束。
- 改寫：`world: a Client plays the arena it was built with…`（原「follows the Match's arena」）、`client: a game whose Match plays another arena is built again on it`（原「the Frame World follows…」）、region 的 `Mesh3d` 測試改成照模型的內容與盒子比對（模型不再由 `PresentationContent` 共用）、loading 的 arena 判定。
- 刪除：`client: an arena changed after the warm-up is warmed again…`（一個 Frame World 不再換圖，這個情況不存在）。
- 突變：退役 `pvp-loading-rewarm-skipped`（同上理由）；`pvp-frame-arena-kept` 改寫成 `pvp-arena-not-again`；`pvp-map-sim-ignored` 的 find 跟著改；新增 7 個（`pvp-map-not-held`、`pvp-walls-not-held`、`pvp-steps-before-walls`、`pvp-map-failure-silent`、`pvp-spawn-in-wall-unchecked`、`pvp-seat-before-walls`、`pvp-map-held-after-walls`），各守 15.6 的一個獨立決定。

## 驗證

本機（Intel Mac，`test` preset，merge `origin/gyo2` 之後）：
- 建置 0 error。`ctest --preset test`（cpu|shader，254 項）：第一次 253 通過，`object_fps_pvp.client` 的重連案例以固定 120 幀等不到遊戲開始而失敗（地圖改在 asset 的執行緒讀，幀數不再固定）。改成先等生成結束與自己的玩家出現（`Setup::Loaded()`、`FramesUntil`）後，`object_fps_pvp.client`／`.world` 各連跑 3 次通過，client 測試 6 個行程同時跑也全過。
- `object_fps_pvp.world` 33 案通過（含新增 5 案）。
- 突變（`tests/common/mutation/run_mutations.py`）：本批新增與改寫的 10 個（含 `pvp-map-sim-ignored`、`pvp-map-colliders-dropped`）10/10 killed；`cases` 指到改過的測試的既有 7 個 7/7 killed。完整重跑留到 PR 線合併前（D184）。
- 實際程式：`gyo_object_fps_pvp-match --arena kiriyu_incident_250m --ticks 64` 記「the map is ready (16 regions, 364 ms)」後照常跑完。B 階段啟動時讀 6 張圖約 1.0～1.1 s，現在只讀這一局的那張。
- 未跑：Windows／Linux（交給 L1）、GPU 的畫面、實機的連線遊玩。

## 沒做

- 串流、km² 量測（S5，要先問使用者）。
- gyo2 的 `mutations.json` 原本就有重複的 id `pvp-gw-ended-room-stays`（不是本批造成），只回報。

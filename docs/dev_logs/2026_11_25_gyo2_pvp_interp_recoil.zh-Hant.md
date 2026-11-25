# 2026-11-25 PvP 第 6 段：遠端玩家內插（PV17）與 viewmodel 後座力（`v8-product-pvp-port-interp`）

Owner：產品（`games/object_fps_pvp`）＋Engine（net 的 `ClockSync` 多存兩個值）。PvP 移植實作卡做。作者 Opus 5.5／high。
根據：[PvP 宣告](../../games/object_fps_pvp/docs/port-declaration.zh-Hant.md)第 2 節「第 6 段」；使用者 2026-11-25「照建議 (a)，合併 #338」（移植完成＝和 master 功能一樣，HANDOFF `v8-arch-gyo2-pvp-port-done-criteria`），主會話轉達範圍：只取概念、照 lightyear 對照、不照搬 master 的常數與算法；內插不改判定；動 Engine 先寫前提。

## 偵察（Opus／high 子 agent，唯讀）

- lightyear：`InterpolationTimeline` 的目標＝server 時間的估計減（延遲＋jitter 餘量），延遲＝`max(送出間隔 × 1.2, 5 ms)`，調速控制器和 input timeline 相同；每個元件一份 `ConfirmedHistory`，smart drain 只在至少 3 筆、第二舊已越過時丟；括號找 ≤ t 的最後一筆與下一筆；沒有下一筆就停住、不外插；延遲刪除；輸入帶 interpolation delay。
- master：`SnapshotTimeline` 只看封包到達時刻（最小包絡線），游標只前進、會跳；延遲一個 tick；換命／瞬移切段；聲音排在畫面的時間軸上。後座力：本機開槍的那一刻起 `振幅 × exp(−衰減 × 秒)`，只轉 viewmodel 的 pitch，參數寫死在程式裡。master 的 view tick 只當診斷記錄（沒有回溯，同 PV3）。

## 做了什麼

1. **Engine（net）**：`ClockSync` 多存 `remoteTick`（這一幀估計的 Match tick，含小數）與 `jitterTicks`；`SyncClock` 本來就算了，只是沒存。同步的行為不變。net 頁第 10 節補一句。前提：Engine 要解決（估計只有 net 算得出來，作者只能照抄 net 的公式）。內插本身照網路宣告 Q7 留在遊戲。
2. **`present/Interpolation`**（不依賴 World）：`InterpolationDelay`、`InterpolationTimeline`＋`Advance`（第一次或誤差超過 10 tick 跳；同方向超過 1 tick 連續 3 次以 1.05 倍調速；否則常速）、`BracketAt`、`Drainable`、`LerpRadiansShortest`、`BlendPose`（換命不混合）。數字是 lightyear 的預設（`ClockSyncSettings` 也是同一組），沒有 master 的。
3. **Sim → Frame**：`ClientView` 多帶 `synced`、`remoteTick`、`jitterTicks`（取自 Sim 的 `ClockSync`）。
4. **Frame**：
   - `RemoteView`：內插時間軸與每個遠端玩家的歷史（`latestConfirmed` 前進時加一筆）。`ApplySnapshot` 只直接寫自己的玩家（預測值），遠端玩家只記歷史；Sim 沒有的遠端玩家記下消失的確認 tick，延後刪。
   - `InterpolateRemotes`（Main/PreUpdate）：時間軸用 Frame 的 `RealTime` 推進，目標＝`remoteTick`＋快照到了之後過的時間－延遲；每個遠端玩家取歷史在時間軸上的值寫進複本元件（不同才寫）；越過消失的 tick 才刪。還沒同步時顯示最新的一筆（和改之前相同）。
   - `PoseCharacters`：對手的動畫時刻改用時間軸的 tick（含小數）。對手的開槍聲、受擊跟著複本元件的變化觸發，所以落在畫面的時間軸上。
5. **畫面的時間（net F2）**：Frame 的「出」把時間軸的 tick 隨控制交給 Sim（`ClientExchange::AddControls` 多一個參數），`WriteStepInput` 寫進 `ViewTime`（tick＋小數一個 byte）；還沒同步時照舊用 `latestConfirmed`。
6. **後座力**：`nailer_v2_viewmodel.json` 加 `recoil`（`kick_degrees` 4、`recovery_per_second` 18，遊戲自己定）；`ViewmodelPlacement(viewmodel, recoilRadians)` 繞相機的 X 軸轉、錨點不動；`PoseViewmodel` 在 Frame 第一次看到自己的 `lastShotTick` 變了的那一刻起算（Frame 的時鐘），每幀寫 viewmodel 部件的 `Transform`（不同才寫）。

## 行為的差別

- 對手在畫面上比之前晚約 2～3 tick（延遲＝1.2＋1＋4 × jitter tick），命中仍由 Match 用當 tick 的位置判定（PV3 不做），瞄準要多提前一點；和 master 相同。
- 對手離開後，替身等時間軸越過它離開的那個確認 tick 才消失（約晚 2～3 tick）。
- 歷史的 tick 用快照的 `latestConfirmed`。一個 tick 的複製分成多個 part 時，元件的值可能來自比 `latestConfirmed` 新的 tick（`maxAppliedTick`）；這個遊戲一個 tick 一個 part，沒有這種情況。沒有處理。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 256/256。
- `logic`：`interpolation:` 7 案（延遲、第一次跳、調速、括號、修剪、yaw 最短角度、換命不混合）。
- `client`：
  - 新案「the other player walks smoothly…」：一步四幀時對手 80 幀裡 80 幀位置都不同、80 幀都落後最新確認值；Match 收到的 `ViewTime` 有小數、從不超過確認的 tick。
  - 新案「a shot kicks the first-person weapon up…」：開槍後 viewmodel 往上轉（兩幀後約 2.3°）、一秒後回到原位；錨點不動、靜止時照定義的 rotation。
  - 既有 11 案照樣過（含「對手離開後替身消失」）。
- `net`：clock 的第一案加判定：落定後 `remoteTick` 和 Match 的 tick 最多差 0.52 tick（門檻 2）、jitter 有存。
- 突變：`nt-clock-remote-not-kept`；`pvp-interp-not-blended`、`pvp-interp-no-delay`、`pvp-interp-life-blended`、`pvp-interp-view-confirmed`、`pvp-recoil-ignored`；相關既有的 `pvp-character-not-shown`、`pvp-hit-direction-not-drawn`、`pvp-frame-mirror-kept`、`pvp-viewmodel-unturned`。共 10/10 killed。
  - `pvp-frame-mirror-kept`、`pvp-viewmodel-unturned` 的 find 因本批改寫而 stale，照新的字面改。`pvp-viewmodel-unturned` 改完後存活：`ViewmodelPlacement` 的位移改用同一個 rotation 算（錨點固定），拿掉轉向後錨點還在原位，「low on the right」那一案看不出來；那一案加判定「靜止時照定義的 rotation、錨點在 offset」後 killed。
- **沒驗**：實機（對手看起來順不順、後座力的手感）交使用者；Windows 實機。

## 設計審查（Opus／high，唯讀；作者同模型，脈絡分離：只給原始需求、宣告與 diff）

blocker 2、should 3、nit 6。

- **B1**（時間軸照 lightyear 跟「Match 現在的 tick」減延遲；`remoteTick` 已含 RTT/2，延遲只有 2.2 tick，單程時間一長時間軸就跑到所有狀態前面，停在最新一筆、每到一筆跳一次；審查者在 MemoryLink 加固定延遲實測：單程 11 Advance（RTT 約 86 ms）時 80 幀只動 60 幀，13 Advance 時 20 幀＝每步一跳；`ViewTime` 也會超前）→ 作者看了 lightyear 確認（`crates/core/sync/src/timeline/remote.rs:137-139` 加 RTT/2，`crates/replication/interpolation/src/timeline.rs:189-215` 目標＝估計－延遲）。偏離 lightyear：Engine 的 `ClockSync` 再存 `rttTicks`，目標改成 `remoteTick − rttTicks / 2 − 延遲`（審查的方案 (a)；只有 net 量得到 RTT）。測試「walks smoothly」改成無延遲與 13 Advance 兩種都跑（13 Advance 時 80 幀動 71 幀、落後 79 幀、`ViewTime` 不超前）；突變 `pvp-interp-rtt-ignored`、`nt-clock-rtt-not-kept`。
- **B2**（後座力往下踢：GYO 左手系、+Z 朝前，繞 +X 正轉是往下；測試把「朝後」當成前方，鎖住了錯的方向）→ 取負角；測試的前方改成 +Z；突變 `pvp-recoil-down`。
- **S1**（預測收回的一槍會再踢一次：`!=`）→ 改成 `lastShotTick` 變大才踢；自己的玩家換人時重設。
- **S2**（接線沒測：換命的 `lifeTick` 接到混合、延遲刪除、drain、對手動畫時刻與開槍聲在時間軸上）→ 加兩案：「the other player's new life is not blended」（在 Match 上直接把對手移到最遠的 spawn、`lifeTick` 設成現在：80 幀裡 0 幀落在中間）；「對手離開」那一案加判定 Frame 的複本比 Sim 的玩家晚消失（本機晚 3 幀）。突變 `pvp-interp-life-wiring`、`pvp-interp-despawn-at-once`。drain（只影響記憶體）、對手動畫時刻與開槍聲的時間軸沒加測試（逐欄細項）。
- **S3**、N3、N4 → 寫進宣告第 6 段設計第 8 項「不處理」。
- N1（`since` 從 Frame 套用快照起算，偏低一點）→ 不改，deadband 吸收。N2（`ClockSync` 註解）→ 改成「synced 時有效，斷線後留著舊值」。N5（跟上規則的數字和 `ClockSyncSettings` 各寫一份）→ 不改：present 不依賴 net，兩邊的數字都標了出處（lightyear 的預設）。N6（後座力衰減到 float 下溢前每幀寫 `Transform`）→ 剩不到千分之一就算靜止。
- 交主會話／使用者：(1) B1 偏離 lightyear（作者已做，理由同上）；(2) 後座力的「振幅 × exp(−衰減 × 秒)」和 master 同形（常數不同），審查者問算不算照搬；作者判斷是一階衰減的通用形式，不算照搬 master 的算法，報告寫明。

修正後：突變 net 的 `nt-clock-remote-not-kept`、`nt-clock-rtt-not-kept` 2/2；遊戲的新 4 個（`pvp-interp-rtt-ignored`、`pvp-interp-life-wiring`、`pvp-interp-despawn-at-once`、`pvp-recoil-down`）與這一段既有的 7 個（`pvp-interp-not-blended`、`-no-delay`、`-life-blended`、`-view-confirmed`、`pvp-recoil-ignored`、`pvp-frame-mirror-kept`、`pvp-viewmodel-unturned`）11/11 killed（`pvp-viewmodel-unturned` 因 B2 改了同一行而 stale，照新字面改後 killed）。

# 2026-11-28 門與交互的骨架（`v8-arch-gyo2-interact-door`）

Owner：門＋交互骨架的任務卡。分支 `claude/gyo2-interact-door-e11d33`（PR #375，開向 `gyo2`）。作者 Opus 5.5。宣告：[interact-door-declaration.zh-Hant.md](../architecture/target/v2/interact-door-declaration.zh-Hant.md)（修訂 2）。

## 前提（使用者的決定，經主會話轉達）

1. 「#375 動工核准」（HANDOFF `v8-arch-gyo2-interact-door-start-impl`）。
2. 五個決策點（`v8-arch-gyo2-interact-door-decisions`）：P1 (a)、P2（Engine 機制＋卸下／保留兩種預設）、D2 (a)、Q-b、Q-c；P3 (a)（`v8-arch-gyo2-interact-door-p3`）；#379 的 A：arena 拿掉 `sets`。

## 各步

- **第 0 步**（Opus／medium）：遊戲流程的 `Scene` → `Screen`（宣告 7.1 的表；7 個檔改名、識別字與註解一起改；`game-flow-declaration`、`gateway-client-handoff` 開頭加一行）。新檢查 `object_fps_pvp.retired_names`＋`_selftest`（`tests/object_fps_pvp/naming/`，label `cpu`）：只掃清單上的舊名與舊檔名。自我測試另持宣告 7.1 的表，清單少一個名字時失敗（手動試了兩個：拿掉 `GameScene`、拿掉 `SoloGame`，都失敗）。
  - 本機（macOS x86_64）：`object_fps_pvp` 與 `naming` 的 `cpu|shader` 12 項全過。
  - 突變（D184）：改名碰到的 48 個，find 都在檔裡恰好一次；find／replace／expect 有變的 13 個重跑，13/13 killed。
  - A/B（主 checkout `build/evidence/gyo2-interact-door/ab.py`，複製自模式配置卡並加 `--input-mapped`；基準 e700e5a3）：權威 digest 1257 行逐位元組相同，報告沒有差異。
- **merge `gyo2`**（#376 顯示名稱、#378 同詞異義表）：#376 改了舊名的 `GatewayScene.cpp`，衝突在改名後的 `GatewayScreen.cpp` 解掉；`retired_names` 抓到 #376 帶進的 `PC::Scene::Room`（`ClientTests.cpp`），改成 `Screen`。#376 的兩個突變（`pvp-label-no-fallback`、`pvp-room-label-not-used`）的 `file` 指向舊檔名，改正後 2/2 killed。`retired_names` 只讀 C++／CMake，`mutations.json` 的舊檔名是手動找的。
  - 本機 `object_fps_pvp|naming|os.` 18 項中 17 過；`os.wake_fallback` 逾時（`loses no signal sent while the waiter checks its flag` 一案；單獨重跑也逾時，load average 400，其他卡同時在建置）。這條線沒碰 os；同樣的情形見 2026-11-25、11-26 的 dev_log，交 L1 確認。
- **第 1 步**（Opus／high）：`object_fps_pvp.core_script` 每個 World 每幀多印 `rest`（權威 digest 扣掉 `PlayerInput` 那一項：照 `AuthorityRegistry` 找 `PlayerInput` 的登記、用它的 `seq` 算那一項再減）與每個玩家一行 `input`（`PlayerInput` 的欄位；四個 float 印位元）。`--check` 通過；同一幀的 `rest` 和 `d` 不同（確實扣掉了一項）。比法在 `ab.py --input-mapped`（宣告 9.3）；宣告原本寫「改 mode-config 的 `ab.py`」，實際放在這條線自己的 `build/evidence/gyo2-interact-door/ab.py`，不動前一張卡的證據。
- **第 2 步**（Opus／high）：`PlayerInput` 的按鍵收成 `presses`（`kPressJump`、`kPressFire`、`kPressReload`、`kPressInteract`）與 `holds`（`kHoldCrouch`、`kHoldReady`），加 `Pressed`／`Held`／`Press`／`Hold`；Codec 2 → 3（一格 22 → 19 bytes）；`held` 改成 `v.presses = 0`；`HashValue` hash `present`、`presses`、`holds`。讀寫處：`PvpWorld.cpp`（移動、蹲、開槍、換彈、開賽）、`ClientWorld.cpp` 的 `StepInputOf`；測試只改「怎麼建輸入」，期望值不改。新測試 `world: a held input keeps the holds and drops every press…`（`[pvp-input-held]`、`[pvp-input-codec]`，從 Match 的 `NetRegistry` 取遊戲登記的 `InputOptions`）。
  - 本機 `object_fps_pvp|naming` 12 項全過。
  - 突變：新 3 個（`pvp-held-drops-holds`、`pvp-held-keeps-presses`、`pvp-input-codec-no-holds`）＋ find 改寫的 `pvp-start-ignores-ready`，4/4 killed。宣告 9.5 的「位元重疊」要有「按換彈不會交互」的測試才殺得到，移到第 4 步。
  - A/B `--input-mapped`（基準 d0296c6f）：`rest` 2182 行逐位元組相同；`input` 2866 行照舊 → 新的對應相同；`d` 1257 行中 1247 行不同（`PlayerInput` 那一項，預期內）；報告只差 protocol hash 與 `PlayerInput` v2 → v3，dataset 不變。
- **第 3 步**（Opus／high，上段）：Engine net 的 `Placed`（`engine/modules/net/include/engine/net/Replication.hpp`）。`Placed`、`HiddenRule{Detach, Retain}`、`WhenHidden`（伺服器端；沒有＝`Detach`）、`NotReceiving`（Client 端、Engine 放與拿）；`NetModule` 登記三個元件。伺服器（`ReplicateOut`）：`Placed` 的實體第一次或再次看到送 3＝附上，看不到了送 4＝藏起（帶 1 byte 規則），帳本多 `PeerLedger::hidden`，藏起期間伺服器刪掉照樣送 2；Resync 清掉 `hidden`。Client（`ApplyStructure`）：附上要 Client 有 `Placed` 的實體（否則 `MissingTarget`、讀過值不寫）、整份 `Set`、這次沒送到的已登記複製元件拿掉（P3 (a)，K3 走 `removedToInbox`）、拿掉 `NotReceiving`、不寫 `Entered`；藏起放 `NotReceiving`，`Detach` 拿掉複製元件；`ApplyReplication` 每幀替還沒附上的 `Placed` 實體補 `NotReceiving`。`kNetProtocolVersion` 3 → 4。`net.zh-Hant.md` 第 3、14 節。
  - 測試：`net.placed` 6 案（附上與附上前的標記、`MissingTarget`、`Detach`、`Retain`、附上時對齊、看得到與藏起時的刪除）；`net.k3` 1 案（兩種規則都過 G-10，`inboxEntries` 不多、`entriesWhileAlive` 和沒有 `Placed` 時相同，`Detach` 的 `inboxRemovals` 2）。K3 一案第一次寫成 `entriesWhileAlive == 0`，但這個 sample 的玩家本來就算 1（兩端放、伺服器又生成），改成和沒有 `Placed` 的同一 sample 比。
  - 本機 `cpu|shader` 263 項全過（這次 `os.wake_fallback` 也過）。
  - 突變：新 9 個（`nt-placed-*`，`net_mutations.json`），第一次 8/9；`nt-placed-attach-unchecked` 是 wrong_failure（殺它的是沒標籤的 `attaches == 0`：沒有實體時值本來就算 `MissingTarget`，差別只在「算不算附上」），給那一條加標籤後 killed。改動範圍附近的既有 6 個（`nt-resync-ignored`、`nt-structure-missing-uncounted`、`nt-k3-exit-missing`、`nt-k3-as-k1`、`nt-vis-loss-not-removed`、`nt-k3-reentry-removed-missing`）全 killed。
- **第 4 步**（Opus／high，上段）：`interaction` 組（`games/object_fps_pvp/include/object_fps_pvp/world/Interaction.hpp`、`src/world/Interaction.cpp`）：`Interactable`、`InteractionRules{reach}`（模式配置的 `sets.interaction`：`{}` 或 `{"reach": 公尺}`，組沒開就不放）、`Interacted`（authority 的 Message）、`JudgeInteractions`（FixedUpdate/PostUpdate、`RespawnPlayers` 之後；有網路時 `OnServer`）。射擊與交互共用的視線、地板、`Playable`、`Walls` 從 `PvpWorld.cpp` 搬到 `src/world/Sight.hpp`／`.cpp`（`JudgeShots` 的地板判定改呼叫 `FloorAlong`，行為不變，A/B 證明）。`GameplaySet::needs`（宣告寫 `requires`，那是 C++20 的關鍵字）：`LoadModes` 讀到開了某組卻沒開它需要的組就讀取失敗。`GameSets()` 登記 `InteractionSet()`。
  - 測試：`world: interaction:` 6 案（`tests/object_fps_pvp/world/InteractionTests.cpp`；測試自己的種類 `tap` 讀 `Interacted`）。`GameplaySetTests` 的「核心的型別先排」一案原本拿出貨內容當「沒有組的核心」，出貨內容有了 interaction 組後前提不成立，基準改成 `CoreOnly()`（出貨內容拿掉所有組）；期望不改。
  - 本機 `cpu|shader` 263 項中 262 過，失敗的就是上一條，改正後 `object_fps_pvp.world`、`core_script` 通過。
  - 突變：新 7 個（`pvp-interact-*`、`pvp-set-needs-unchecked`；含第 2 步移來的位元重疊）＋搬檔的 `pvp-start-not-frozen`＋改動附近的 `pvp-shot-wall-ignored`，9/9 killed。
  - A/B（基準 77ee6e59）：權威 digest 1257 行、`rest` 2182 行、`input` 2866 行逐位元組相同；報告只多三端各兩列（dataset 的 `message Interacted`、`system JudgeInteractions`），protocol hash 不變（interaction 組沒有複製的元件）。
- **第 7 步的一部分（E 鍵）**（Opus／high）：第 5 步等 #379 合併，先做不靠 scene 的部分。Frame 的 `ReadControls` 讀 `E`（`JustPressed`）→ `present::ControlsFrame`／`StepInput` 的 `interact` → `Controls` 的待送按一下 → `ClientExchange` 合流（按一下的鍵相加）→ Sim 的 `StepInputOf` 放進 `kPressInteract`，取出就清、`stale` 時也清。新測試 `client: E reaches the Match as one interaction press`（`[pvp-interact-key-sent]`：按一次 E，Match 恰好一步有這個鍵）。
  - 本機 `object_fps_pvp` 的 `cpu|shader` 10 項全過。
  - 突變：新 2 個（`pvp-interact-key-not-read`、`pvp-interact-press-repeats`）＋ find 改寫的 `pvp-step-press-repeats`、`pvp-hold-kept-stale`，4/4 killed。

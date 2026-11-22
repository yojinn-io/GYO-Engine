# 2026-11-23 Resource 的變更偵測（`v8-arch-gyo2-kernel-res-changed`）

Owner：Kernel（GYO2 主會話開的任務卡）。分支 `claude/gyo2-kernel-res-changed-b8b796`（開向 `gyo2`）。作者 Opus 5.5；宣告的敵對式檢查與合併前的設計審查 Fable 5.2／high（唯讀）。

## 為什麼

網路卡（PR #275）第 2 步查狀態機卡交叉比對 F3 時發現：三家參考引擎（replicon、lightyear、Unity Netcode）都靠變更偵測讓遊戲知道「值被伺服器換掉了」；GYO 的元件有 `Changed<T>`，Resource 每個 slot 也有 `changed` stamp、寫入就打，但沒有讀法。使用者 2026-11-22 決定補，owner 是 Kernel（原話「開」）。

## 經過與結論

宣告：[kernel-res-changed-declaration.zh-Hant.md](../architecture/target/v2/kernel-res-changed-declaration.zh-Hant.md)（修訂 1，使用者 2026-11-23 核准）。

| 修訂 | 內容 | 檢查 |
|---|---|---|
| 0 | `Res`／`ResMut` 加 `Changed()`，比掛載的上次跑；`KernelSettings` 建構子打 stamp | Fable／high：blocker 1、should 5、nit 8（主 checkout `build/evidence/gyo2-kernel-res-changed/declaration-check-r1-fable.md`；沒有建置、沒有跑測試） |
| 1 | B1：計畫中的寫者（網路的預設套用、狀態機的套用）同值也寫，點名的 Feature 光靠「被寫過」做不到 → 寫成決定點 3.1；observer 不承諾；換掉錯的單機例子（`VirtualTime` 每幀被寫）；跟上 #278、#282；set 條件、拿掉時的 stamp、CCR 登記 | — |

使用者 2026-11-23 的決定：「3.1 選 (a)，其他照建議，開始」——Kernel 照 Bevy／flecs／Unity，`Changed()` 是「被寫過」；「反覆同步同一個值的寫者，值相同就不寫」登記成 CCR-8，經主會話轉給網路卡與狀態機卡；run 條件裡比掛載的上次跑（和 Bevy 不同）照現狀寫明；`is_added`、`set_if_neq`、`last_changed`、`resource_removed` 不做。決定照四樣轉給主會話（2026-11-23），HANDOFF 由主會話記。

## 做了什麼

- `engine/ecs/kernel/include/engine/kernel/Params.hpp`：`Res<T>::Changed()`、`ResMut<T>::Changed()`；兩個句柄記下呼叫的 `lastRun`；判定只在一處 `Detail::ResourceChangedSince`（有值、而且 `changed > lastRun`）。
- `engine/ecs/kernel/src/World.cpp`：World 建立時放進的 `KernelSettings` 打 `WriteStamp()`（原本 stamp 是 0，第一次跑的掛載看不到它被放進去，和「放進去也算」不一致）。
- `tests/common/kernel/ResourceChangedTests.cpp`（新，ctest `kernel.others`）：`DI-KS 28:` 九案——第一次跑與 Host 在兩次 Run 之間寫；沒有值、拿掉、放回；寫入不比值、`Read()` 不算；寫在讀者之後；自己的寫入；`Commands`、`ExclusiveWorld::Modify`／`SetResource`、`Restore`；跳過的掛載、同一個 System 掛兩處、巢狀；run 條件；`KernelSettings`。
- `docs/architecture/target/kernel.zh-Hant.md`：DI-KS 28（草稿）；DI-KS 10 的 observer 那句補「同一個同步點後面才寫的看不到」那一半；表頭。
- `docs/architecture/target/v2/system-contract-page.zh-Hant.md`：第 3 節的參數表、第 15 節一句、新第 16 節（寫法、同值不寫、比的是誰、Engine 自己每幀寫的 Resource）。
- `docs/cross-component-rules.md`：CCR-8。
- `docs/architecture/plans/kernel/scripts/mutations.json`：突變 `rc-` 五個；`we-j12-fetch-side-effect` 的 find／replace 照 `ResMut` 建構子的新寫法改、補 `guards`。
- `docs/architecture/target/v2/README.md`：宣告與本 dev_log 列入。

## 檢驗

本機 macOS（Intel，RelWithDebInfo，preset `test`）：
- `DI-KS 28:` 九案 9/9、55 個 assertion 通過；`ctest -L "cpu|shader"` 169/169。
- 突變（`mutation-rerun-scope`：新加的，以及 find 落在改動範圍——`Res`／`ResMut`、World 建構子——的既有突變）：`rc-` 五個、`we-j12-fetch-side-effect`、`ks-s2-world-while-pending`，7/7 killed，每個都是照預期的訊息算的。
- 全部 22 份突變清單裡指到 `Params.hpp`、`World.cpp` 的 257 筆，find 各命中一次（改之前 `we-j12-fetch-side-effect` 是 0 次，表示這個掃描有作用）。
- 證據：主 checkout `build/evidence/gyo2-kernel-res-changed/impl/`（`build-all.log`、`ctest.log`、`mutants.log`；git 不追蹤，這台 macOS Intel 機器）。
- 建置時唯一的 warning 在既有的 `MessageCursorTests.cpp:607`（`-Wmissing-field-initializers`），不是這一批改的。

## 突變（§14.5）

| id | 只有它抓得到的 | 守的 |
|---|---|---|
| `rc-changed-at-last-run` | 比較寫成 `>=`：`ResMut` 掛載看到自己上次的寫入（`Res` 掛載不會用自己的 stamp 寫，對它是等價突變，所以抓它的是 `[rc-own-write]`） | kernel DI-KS 28 |
| `rc-changed-ignores-present` | 不看有沒有值：拿掉的那次 stamp 也動，會被當成 changed | kernel DI-KS 28 |
| `rc-res-last-run-zero` | `Res` 拿 0 當基準：寫過一次就每次都 true | kernel DI-KS 28 |
| `rc-resmut-base-is-stamp` | `ResMut` 拿自己這次的 stamp 當基準：看不到同一次呼叫裡自己的寫入 | kernel DI-KS 28 |
| `rc-settings-unstamped` | 建構子不打 `KernelSettings` 的 stamp | kernel DI-KS 28 |

五個，未超過「變更的條文數 × 5」。判定一處（`ResourceChangedSince`），所以 `Res`／`ResMut` 的比較與有沒有值各只要一個突變。run 條件、跳過、兩個掛載用的是既有的 `lastRun` 機制，已有突變（`we-adv-skip-updates-lastrun`、`we-j52-per-system-mount-state`）守，不另加。

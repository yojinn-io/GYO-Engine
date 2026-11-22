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
- `tests/common/kernel/ResourceChangedTests.cpp`（新，ctest `kernel.others`）：`DI-KS 28:` 十二案（九案＋合併前審查補的三案）——第一次跑與 Host 在兩次 Run 之間寫；Run 第一步的讀者；Host 的 `ResourceMut`；沒有值、拿掉、放回；寫入不比值、`Read()` 不算；寫在讀者之後；自己的寫入（當場的、經 `Commands` 延後的）；`Commands`、`ExclusiveWorld::Modify`／`SetResource`、`Restore`；跳過的掛載、同一個 System 掛兩處、巢狀；run 條件；`KernelSettings`。
- `docs/architecture/target/kernel.zh-Hant.md`：DI-KS 28（草稿）；DI-KS 10 的 observer 那句補「同一個同步點後面才寫的看不到」那一半；表頭。
- `docs/architecture/target/v2/system-contract-page.zh-Hant.md`：第 3 節的參數表、第 15 節一句、新第 18 節（第 16、17 節留給開著的 #281、#276）（寫法、同值不寫、比的是誰、Engine 自己每幀寫的 Resource）。
- `docs/cross-component-rules.md`：CCR-8。
- `docs/architecture/plans/kernel/scripts/mutations.json`：突變 `rc-` 六個；`we-j12-fetch-side-effect` 的 find／replace 照 `ResMut` 建構子的新寫法改、補 `guards`。
- `docs/architecture/target/v2/README.md`：宣告與本 dev_log 列入。

## 檢驗

本機 macOS（Intel，RelWithDebInfo，preset `test`）：
- 實作 commit（5023b23f）時：`DI-KS 28:` 九案 9/9、55 個 assertion 通過；`ctest -L "cpu|shader"` 169/169。
- 突變（`mutation-rerun-scope`：新加的，以及 find 落在改動範圍——`Res`／`ResMut`、World 建構子——的既有突變）：`rc-` 五個、`we-j12-fetch-side-effect`、`ks-s2-world-while-pending`，7/7 killed，每個都是照預期的訊息算的。
- 全部 22 份突變清單裡指到 `Params.hpp`、`World.cpp` 的 257 筆，find 各命中一次（改之前 `we-j12-fetch-side-effect` 是 0 次，表示這個掃描有作用）。
- 證據：主 checkout `build/evidence/gyo2-kernel-res-changed/impl/`（`build-all.log`、`ctest.log`、`mutants.log`；git 不追蹤，這台 macOS Intel 機器）。
- 建置的 warning 都在既有檔案（`build-all.log` 18 個；kernel 測試裡只有 `MessageCursorTests.cpp:607` 一個），沒有一個在這一批改的檔。

## 突變（§14.5）

| id | 只有它抓得到的 | 守的 |
|---|---|---|
| `rc-changed-at-last-run` | 比較寫成 `>=`：`ResMut` 掛載看到自己上次的寫入（`[rc-own-write]`），Run 第一步的 `Res` 掛載把 Host 在 Ready 的寫入（取的正是那一步的 stamp）看兩次（`[rc-first-step]`）；expect 要兩個標記都出現（合わせ方）。修訂前寫「對 `Res` 是等價突變」，合併前審查 S3 指出不對 | kernel DI-KS 28 |
| `rc-changed-ignores-present` | 不看有沒有值：拿掉的那次 stamp 也動，會被當成 changed | kernel DI-KS 28 |
| `rc-res-last-run-zero` | `Res` 拿 0 當基準：寫過一次就每次都 true | kernel DI-KS 28 |
| `rc-resmut-base-is-stamp` | `ResMut` 拿自己這次的 stamp 當基準：看不到同一次呼叫裡自己的寫入 | kernel DI-KS 28 |
| `rc-settings-unstamped` | 建構子不打 `KernelSettings` 的 stamp | kernel DI-KS 28 |
| `rc-host-resourcemut-unstamped` | World API 的 `ResourceMut` 不打 stamp：Host 經指標寫入讀者看不到（合併前審查 S2：拿掉它 kernel 的 396 案全過） | kernel DI-KS 28 |

六個，未超過「變更的條文數 × 5」。判定一處（`ResourceChangedSince`），所以 `Res`／`ResMut` 的比較與有沒有值各只要一個突變。run 條件、跳過、兩個掛載用的是既有的 `lastRun` 機制，已有突變（`we-adv-skip-updates-lastrun`、`we-j52-per-system-mount-state`）守，不另加。

## 合併前的設計審查

Fable 5.2／high（唯讀），報告：主 checkout `build/evidence/gyo2-kernel-res-changed/merge-review-fable.md`（實驗在同目錄 `review-exp/`）。結果是 must 1、should 7、nit 7，結論「改了再合併」。實作照宣告 2.1 做，沒有越出範圍。

| 項 | 處理 |
|---|---|
| M1 落後 `origin/gyo2`（#277） | merge（不 rebase），`v2/README.md` 的列表衝突，兩行都留 |
| S1「自己上次跑寫的下一次是 false」太寬：經 `Commands` 延後寫的，下一次是 true | DI-KS 28 與說明頁改成分開寫（當場寫的 false、延後寫的 true，同 Bevy）；加一案 `[rc-own-deferred]` |
| S2 Host 的 `ResourceMut` 寫入路沒有測試守 | 加一案 `[rc-host-resourcemut]`、突變 `rc-host-resourcemut-unstamped`；DI-KS 28 的寫入清單補 World API |
| S3 `>=` 對 `Res` 不是等價突變（Run 第一步的讀者） | 加一案 `[rc-first-step]`，照「合わせ方」併進 `rc-changed-at-last-run`（expect 要兩個標記都出現），改 `what` 與本頁 |
| S4 CCR-8 的已知處沒有走查紀錄 | 補走查命令、日期、正例對照；列出照做的（`RestoreSet`、回滾的還原）與每次都寫的（`WorldGeneration`、回滾的輸入寫回、`AdvanceTime`）。後兩類算不算這條規則，回報主會話判；這一批不改程式 |
| S5 說明頁第 16 節和 #281（16）、#276（17）撞號 | 改用第 18 節，16、17 留給那兩個 PR |
| S6 `target/README.md` 的條數 | 改成 42／23／DI-KS 1～28；同一行的 RC 1～23（#278 漏的）一起改成 1～24 |
| S7 沒有 L1 | 推上去後看 L1，結果記在 PR |
| N1、N3、N4 | 說明頁的更新行；DI-KS 10 的括號把 `Add<T>`、`Remove<T>` 分開寫；說明頁兩處用詞 |
| N2 | 本頁的 warning 那句改正 |
| N5、N6 | 不補：N5 是由既有機制推出的主張；N6 是防誤用的句柄檢查，同一組的函式都沒有逐函式的突變 |
| N7 | 合併前核准時拿掉三處「草稿」 |

突變檢查（§14.2）：檢查者用官方執行器獨立重跑列表上的六個，6/6 killed；另外找到四處，補了兩處（S2、S3）。改完後重跑受影響的突變：`rc-changed-at-last-run`、`rc-host-resourcemut-unstamped` 2/2 killed（`impl/mutants-review.log`）。`DI-KS 28:` 十二案 12/12、67 個 assertion 通過。merge #277 之後 `ctest -L "cpu|shader"` 169/169（`impl/ctest-after-review.log`）。

# Kernel 測試的三筆舊帳（`v8-arch-gyo2-test-debts`）

日期：2026-11-16。分支 `claude/gyo2-test-debts-88cebc`（從 `gyo2` 3743eaa8 開出）。

這一批由任務卡「測試的舊帳」做（GYO2 重建主會話開出；使用者 2026-11-16 在主會話說「開個任務卡處理」「開任務卡」）。範圍只有突變清單 `docs/architecture/plans/kernel/scripts/mutations.json`、`tests/common/kernel` 與文件，不改 `engine/` 的行為。三件事照順序做，每件各開小 PR。

## 第一件：四個 `find` 對不上的突變

PR [#236](https://github.com/yojinn-io/GYO-Engine/pull/236)（目標 `gyo2`；範圍判斷為只改文件，L1 沒有跑，CI gate 通過）。

逐個查了原本要打的性質、目標檔現在的文字、讓它對不上的變更。四個的性質都還在，處置都是「照現在的文字重寫 `find`」；沒有刪掉的、也沒有重寫後存活的。

| 突變 | 守的性質 | 讓它對不上的變更 | 處置 |
|---|---|---|---|
| `ks-links-extra` | `gyo_kernel` 的連結對象正好是宣告的那些（多連一個就 configure 失敗） | 2cfe42b2 把 world-exec 的程式搬進 `gyo2`，連結多了 `GYO::EntityStore`；突變清單沒有跟著搬 | `find`、`replace`、`expect` 照 `claude/arch-kernel-world-exec` 50cb2050 的版本 |
| `ks-adv-include-allowlist-widened` | K5 的專案 include 白名單固定（多一個 `engine/os/` 就被自測抓到） | 同上：白名單多了 `engine/entity_store/`，自測改名 | 同上，照 50cb2050 |
| `ks-k5-parent-directory` | 專案 include 裡有 `..` 要被 K5 報出來 | 同上：world-exec 改成也用反斜線切（`re.split`） | 同上，照 50cb2050 |
| `rs-j17-zone-b-listed` | J-17：(b) 區的 System 不列進 zone C sources 的匯出 | 4e68b589 把路徑檢查搬進和初始值共用的 `add` lambda | `find` 改成 `continue` 加上下一行 `add(system.source, "a zone C System"` 的開頭；`expect` 不變 |

每個突變的 `what` 末尾註明了來源與原因。清單的 `description` 沒有改：它是單獨一行，K0 那張卡也在往裡面加字，改了會在同一行衝突。

### 檢證

- 本機（macOS，x86_64）`cmake --preset test` 後完整建置。
- 全清單掃描：450 個突變的 `find` 在目標檔各出現正好一次（用 Python 讀檔計數）。正例：同一支掃描對 3743eaa8 的清單跑，正好報出這四個（`find-scan-base.log`）。
- 四個突變實際跑一次：4/4 killed，都是預期的訊息（`part1-mutants.log`）。


## 第二件：刪除實體時 OnRemove 的先後（等使用者判）

查了有沒有使用者的決定：沒有。

- `World.cpp` `ApplySync` 第 2 步的做法（先對每個元件依 ComponentId 升冪叫 OnRemove，全部叫完才依 ComponentId 升冪銷毀）來自 `claude/arch-kernel-world-exec` 的 3d87d916，commit 訊息寫「架構師建議 (a)、使用者決定待ち」，選另一種讀法時 revert。
- 那條線的宣告第 6 節第 (ii) 項、dev_log、master 的 kernel HANDOFF 都記「待使用者」；後來使用者重審的是同一項的第 (i) 題（hook 佇列），hook 那一批定案；第 (ii) 題沒有再出現。
- GYO2 的決定表（`claude/gyo2-time-design` 的 HANDOFF）、`target/` 的 Design Invariants、hook 與 spawn-reject 兩批的宣告都沒有這一項的決定。`v8-arch-gyo2-sync-order-keep` 定的是各階段的先後（刪除→移除→生成…），不是刪除這一步裡 OnRemove 與銷毀的交錯。
- 規格 F.2 第 2 步「對該實體的每個元件依 ComponentId 升冪執行 OnRemove，再移除並 `destroy`」兩種讀法都說得通；W-7 只定了銷毀的順序。
- 現有測試：`WorldTests.cpp` J-32 的子案例「a despawn runs every OnRemove before destroying anything」（3d87d916 加的）只登記一個 hook，所以分得出「交錯」與「先全部叫完」，但守不到「OnRemove 依 ComponentId 升冪叫」——兩種讀法都要這個先後，現在沒有測試守。

選項已交使用者（含遊戲的例子、系譜、代價），判了才寫守衛與突變。

## 第三件：還沒搬的 world-exec 突變（清點）

來源：`claude/arch-kernel-world-exec` 的 50cb2050，`we-` 開頭的 91 個。`gyo2`（3743eaa8）有其中 37 個；沒有的 54 個裡，`we-adv-spawn-alive-assert-removed`、`we-adv-spawnwithid-allotter-range-assert-removed` 是 #235 故意拿掉的（打的斷言改成丟掉，由 `sr-assert-alive`、`sr-assert-range` 取代），所以要清點的是 52 個。另外該分支在 50cb2050 之後還有兩個（`we-despawn-onremove-interleaved`、`we-hook-queue-one-per-world`），不在這 52 個裡：前者屬第二件、等使用者的決定；後者打的是「同步點只套用自己這張 schedule 的 hook 佇列」（`ApplySync` 第 0 步；突變把它改成整個 World 一個），這個性質在 `gyo2` 還在、但沒有突變守，期待的訊息所在的案例也不在 `gyo2`；K0 宣告第 4 節第 15 項要寫測試固定「hook 的佇列每張 schedule 一個」，目標又在同步點的套用裡，所以列「等 K0」。

分類的依據：K0（`v8-arch-gyo2-nested-schedule`）宣告第 4 節要改的函式——執行器的入口、步驟迴圈與 `Fail`、成員檢查、延後的佇列與同步點的套用、存取驗證的重拍（呼叫後的比對）、句柄的過期檢查、`ExclusiveWorld`（含第 5.1、5.2 節要放寬的 J-3、J-45）、呼叫錯誤的合併（第 8 項「裡面的錯誤併進外層這次呼叫的清單」）。目標在這些函式裡的列「等 K0」。`World::Access` 整個函式列「等 K0」：K0 第 10、13 項要在「整個 World」的存取上加 Resource 的規則與「讀不給讀的 Resource」的錯誤，可能改到它，這是保守的判斷。

| 突變 | 打的性質 | `gyo2` 上的目標 | 分類 |
|---|---|---|---|
| `we-j1-insert-append` | 欄插入維持 EntityId 順序 | Storage.hpp `Column::Put` | 現在搬（已搬、已跑） |
| `we-j1-erase-swap-pop` | 欄刪除不換位（維持順序） | Storage.hpp `Column::Erase` | 現在搬（已搬、已跑） |
| `we-j2-alive-append` | 活實體清單依 EntityId 排序 | World.cpp `World::MakeAlive` | 現在搬（已搬、已跑） |
| `we-j3-structural-allowed` | 執行期間 World 介面的結構變更是斷言（W-3） | World.cpp `World::Access` | 等 K0（J-3 是 K0 第 5.1 節要放寬的守衛） |
| `we-j3-spawn-immediate` | Spawn 在呼叫裡不套用（成員檢查抓） | Params.hpp `Spawn::New`；期待的訊息是成員檢查 | 等 K0（成員檢查，K0 第 4 節第 3 項） |
| `we-j4-handle-copyable` | 句柄不能複製（建置失敗樣本） | Params.hpp `Handle` | 現在搬（已搬、已跑） |
| `we-j4-handle-check-off` | 跨呼叫留住的句柄報 StaleHandle | World.cpp `CheckHandle` | 等 K0（句柄的過期檢查，第 6 項） |
| `we-adv-j4-serial-half-dropped` | 過期檢查同時比呼叫序號與同步點計數 | World.cpp `CheckHandle` | 等 K0（同上） |
| `we-j4-boundary-off` | 呼叫後的比對找出未宣告的改動（J-4） | World.cpp 呼叫後比對的 `check` | 等 K0（存取驗證的重拍，第 5 項） |
| `we-j5-boundary-off` | 同上（J-5） | World.cpp 呼叫後比對的 `check` | 等 K0（同上） |
| `we-j5-layer1-off` | 第一層（World 介面）檢查會報 | World.cpp `World::Access` | 等 K0（`World::Access`，保守） |
| `we-j5-id-not-mode` | 第一層比對讀寫的模式，不只 id | World.cpp `World::Access` | 等 K0（同上） |
| `we-j6-no-current-call` | 執行期間沒有目前的 System 時存取是斷言（W-6） | World.cpp `World::Access` | 等 K0（同上） |
| `we-j6b-host-zone-c` | 第一次 Run 之後外面不能寫 (c) 區元件（W-6b） | World.cpp `World::Access`（Ready 分支） | 等 K0（同上） |
| `we-adv-j6b-host-zone-c-resource` | 同上，(c) 區 Resource | World.cpp `World::Access`（Ready 分支） | 等 K0（同上） |
| `we-adv-interrupted-write-allowed` | 中斷的 World 只能讀（E.5） | World.cpp `World::Access`（Interrupted 分支） | 等 K0（同上） |
| `we-j3-16-violations-dropped` | 呼叫的違規變成 Err，不被丟掉 | World.cpp `CombineCall` | 等 K0（錯誤的合併，第 8 項） |
| `we-j32b-dropped-err-lost` | System 丟掉的 Kernel Err 不會遺失 | World.cpp `CombineCall` | 等 K0（同上） |
| `we-adv-dropped-err-beside-own-err-lost` | System 回自己的 Err 時，丟掉的 Kernel Err 仍列出 | World.cpp `CombineCall` | 等 K0（同上） |
| `we-j53-after-diff-dropped` | 呼叫後比對的差異列成 BoundaryChange | World.cpp `CombineCall` | 等 K0（同上） |
| `we-j7-world-dtor-reverse` | World 解構依 ComponentId 升冪 | World.cpp `World::~World` | 現在搬（已搬、已跑） |
| `we-j7-column-dtor-descending` | 欄解構依 EntityId 升冪 | Storage.hpp `Column::~Column` | 現在搬（已搬、已跑） |
| `we-j8-allotter-wraps` | 配發器用完不繞回 | Storage.hpp `RangeAllotter::Allot` | 現在搬（已搬、已跑） |
| `we-j8-allotter-reset` | 配發器沒有 Reset | Storage.hpp `RangeAllotter` | 現在搬（已搬、已跑） |
| `we-w11-mut-no-mark` | `Mut` 取寫入的值時標記 changed（J-11） | Params.hpp `Mut::Get` | 現在搬（已搬、已跑） |
| `we-w11-mut-no-mark-j30` | 同上（J-30） | Params.hpp `Mut::Get` | 現在搬（已搬、已跑） |
| `we-j32-spawn-before-despawn` | 同步點刪除排在生成之前（重連） | World.cpp `World::ApplySync`；`find` 已對不上，期待的斷言已被 #235 改成丟掉 | 等 K0（同步點的套用；搬時 `find` 與 `expect` 都要重寫） |
| `we-j32b-untyped-unchecked` | 無型別 Commands 只能寫 (b) 區 Shared（F-10） | World.cpp `CheckUntypedCommand` | 現在搬（已搬、已跑） |
| `we-j32b-debug-only` | 同上的檢查在每種建置都做 | World.cpp `CheckUntypedCommand` | 現在搬（已搬、已跑） |
| `we-adv-untyped-zone-half` | 同上，區與寫入政策都檢查 | World.cpp `CheckUntypedCommand` | 現在搬（已搬、已跑） |
| `we-j32b-z5-record-off` | (b) 區刪除帶 (c) 區資料的實體在記錄時就是 Err（Z5） | World.cpp `CheckDespawn` | 現在搬（已搬、已跑） |
| `we-j36-no-same-tick` | 讀者在寫者之後能讀到同一 tick 的事件（F-7） | Params.hpp `EventReader::Read` | 現在搬（已搬、已跑） |
| `we-j36-swap-compare` | 上一 tick 的事件比較方向 | Params.hpp `EventReader::Read` | 現在搬（已搬、已跑） |
| `we-j36-trim-early` | 修剪不丟掉目前的 tick | World.cpp `World::TrimEvents` | 現在搬（已搬、已跑） |
| `we-adv-event-trim-never` | 修剪會丟掉舊事件（F.3） | World.cpp `World::TrimEvents` | 現在搬（已搬、已跑） |
| `we-j45-exclusive-unchecked` | `ExclusiveWorld` 只能寫 (b) 區 Shared（S-13） | World.cpp `CheckExclusiveWrite` | 等 K0（J-45 是 K0 第 5.2 節要放寬的守衛） |
| `we-j45-debug-only` | 同上的檢查在每種建置都做 | World.cpp `CheckExclusiveWrite` | 等 K0（同上） |
| `we-adv-exclusive-zone-half` | 同上，區與寫入政策都檢查 | World.cpp `CheckExclusiveWrite` | 等 K0（同上） |
| `we-j12-fetch-side-effect` | 取 `ResMut` 沒有副作用（B.4 第 7 點） | Params.hpp `ResMut` 的建構 | 現在搬（已搬、已跑） |
| `we-sym-tlcurrent-renamed` | 執行緒區域變數只允許 `tlCurrent`（符號守衛） | Verification.cpp | 現在搬（已搬、已跑）（只在 macOS／Linux） |
| `we-k5-any-engine-prefix` | K5 白名單不能放寬成整個 `engine/` | kernel_guard.py `ALLOWED_PROJECT_INCLUDES` | 現在搬（已搬、已跑） |
| `we-k5-entity-store-prefix-unbounded` | 白名單的 `engine/entity_store/` 帶結尾斜線 | kernel_guard.py `ALLOWED_PROJECT_INCLUDES` | 現在搬（已搬、已跑） |
| `we-k5-entity-store-dropped` | Kernel 真正的 entity_store include 要通過 | kernel_guard.py `ALLOWED_PROJECT_INCLUDES` | 現在搬（已搬、已跑） |
| `we-k5-entity-store-dropped-selftest` | 同上（自測） | kernel_guard.py `ALLOWED_PROJECT_INCLUDES` | 現在搬（已搬、已跑） |
| `we-k5-backslash-parent` | 反斜線路徑裡的 `..` 也要報 | kernel_guard.py `file_findings` | 現在搬（已搬、已跑） |
| `we-stamp-guard-rule-off` | entity_store 不提 stamp 的字（守衛自測） | store_stamp_guard.py | 現在搬（已搬、已跑） |
| `we-stamp-guard-store-names-stamp` | 同上（entity_store 註解提到 changed stamp） | EntityColumns.hpp | 現在搬（已搬、已跑） |
| `we-adv-stamp-guard-camel-suffix` | 同上（駝峰的 changeTick、lastChanged） | EntityColumns.hpp | 現在搬（已搬、已跑） |
| `we-adv-excluded-columns-not-reported` | 驗證報告列出排除的欄（LIM-6 條件 2） | Executor.cpp `Product::Verification` | 現在搬（已搬、已跑）（`expect` 的 `rig->app` 改成 `rig->product`） |
| `we-adv-added-is-changed` | `Added<T>` 看 added，不看 changed | Params.hpp `Fresh` | 現在搬（已搬、已跑） |
| `we-adv-hookctx-in-system-allowed` | `HookCtx` 只能用在 hook（F.4） | Params.hpp `HookCtx` 的建構 | 現在搬（已搬、已跑） |
| `we-adv-host-write-current-stamp` | 外面在 Ready 寫入帶下一個 stamp（J-30） | World.hpp | 現在搬（已搬、已跑） |

合計：現在搬 32 個、等 K0 20 個（另加上面提到的 `we-hook-queue-one-per-world`，等 K0 共 21 個）、不搬 0 個（#235 拿掉的兩個不在這 52 個裡）、搬了會存活 0 個。

「現在搬」的 32 個實際跑過：先用一份只含這 32 個的清單跑（`part3-candidates-run1.log`：31 killed，`we-adv-excluded-columns-not-reported` 是 wrong_failure——案例照樣抓到，但期待的文字還是改名前的 `rig->app`；改成 `rig->product` 後 killed，`part3-candidates-run2.log`），再從正式清單跑一次（`part3-catalog-run.log`）。新條目放在 `we-adv-hook-layer2-off` 之後，`what` 末尾註明來源。

「等 K0」的 20 個（加上 `we-hook-queue-one-per-world` 共 21 個）沒有在 `gyo2` 上跑：K0 合併後目標的文字會變，到時照合併後的程式重寫 `find`、再跑，搬了會存活的列給使用者。目標都在 K0 會改的函式裡，不在這張卡先搬，是為了不讓 K0 在實作中途替這些條目重寫 `find`。

## 證據

全部在這台 Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-test-debts/`（git 不追蹤）：`configure1.log`、`build1.log`；第一件 `find-scan-base.log`、`part1-mutants.log`；第三件 `we-missing.json`（54 個沒搬的原文）、`locate.py`（找目標所在的函式）、`part3-candidates.json`、`part3-ids.txt`、`part3-candidates-run1.log`、`part3-candidates-run2.log`、`part3-catalog-run.log`。

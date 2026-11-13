# 架構 v8：v8-arch-kernel-world-exec（World 與 Serial Executor；存取驗證兩層）

日期：2026-11-13～14。Owner：架構（架構師會話開出的任務卡 06）。計畫：[遷移計畫](../architecture/plans/kernel/migration-plan.zh-Hant.md) §3.7、§3 共通、§4 Delta-1、Delta-3、§6 BU-2；事前宣告 [world-exec-declaration.zh-Hant.md](../architecture/plans/kernel/world-exec-declaration.zh-Hant.md)（使用者 2026-11-13 核准，含實作檔位 Opus／xhigh；文末「核定後的修訂」1～7）。

- 執行：本卡會話（Opus 5.5／xhigh；commit 的 Co-Authored-By 照實寫 Opus 5.5），工作樹 `.claude/worktrees/elastic-greider-b87b30`，分支 `claude/arch-kernel-world-exec`（自 `origin/claude/arch-world-exec-decl` 開）。審查兼突變一覽的敵對檢查：Fable／xhigh 只讀，一次（讀快照 worktree，commit 882486d0）。§14.4 的驗證者：Opus／high 另一代理（K5 放寬；與作者同為 Opus 系，照實記）。
- 證據：主 checkout `build/evidence/kernel-world-exec/`（git 管理外；本機 Intel Mac i9-9980HK）：`mutations-run1.txt`（we-* 第一輪 57/60）、`mutations-run2.txt`、`mutations-k5.txt`、`mutations-stamp-guard.txt`、`mutations-rs-ks.txt`（既有 rs-／ks- 205/208）與 `mutations-rs-ks-rerun.txt`、`refine-expects.txt`（期待文字列具體化）、`mutations-final.txt`（96/96）、`mutations-pending-a1.txt`、`mutations-pending-a2.txt`、`bu2-record.txt`。L1 的 tests.xml 見文末〈L1〉。

## 件数の突き合わせ

（L1 完成後填入：宣告判定 2 的 J 條目 × 三平台）

## 做了什麼

- `engine/kernel/`（連結 `GYO::Base GYO::EntityStore`，Delta-3）：
  - **World**（`World.hpp`、`src/World.cpp`）：欄＝`std::vector<Cell<T>>`，以 `engine/entity_store` 的演算法維持升冪（target/world §6 第 2 種，定案；`engine/entity_store` 一字不改）；Resource、事件緩衝、診斷緩衝；Host 與「繞過句柄」的 World API 都過 W-6／W-3／W-6b 的常開檢查與第一層驗證；同步點依 F.2 的 0～8 步套用（排序鍵唯一並 GYO_ASSERT）、hook、Z5 的複核（含窗內 (c) 的 insert／SpawnWithId）；事件修剪；第二層快照比對（System 前後、Run 開頭的 Ready 窗口、hook 前後）；World 摘要（D-9，不含 stamp 與 KernelSettings）；`RangeAllotter`（單調、用盡回報、不回繞、無 Reset）。
  - **App 與 Serial Executor**（`App.hpp`、`src/Executor.cpp`）：E.2 的 ⓪～⑥、同步點、修剪；Err 三來源記錄一次（handler）、Interrupted、未套用的紀錄丟棄、診斷保留；例外不捕捉、以 RAII 標 Interrupted；KernelSettings 在第一次 Run 讀一次。
  - **存取驗證第一層**（`src/Verification.cpp`）：`thread_local SystemContext* tlCurrent`（K4 白名單形狀）；句柄的呼叫序號／同步 epoch 比對（W-4）。
  - **句柄的執行期**（`Params.hpp`、`detail/Context.hpp`）：Query（ForEach／Get／GetMut／Entities，依 id 升冪）、Res／ResMut、EventReader（無游標規則）／EventWriter、Commands（F-10 以 Err 回傳）、CommandsFor、Spawn（記錄時配 id）、SpawnWithId、Despawn（Z5 記錄當下）、Local（每掛點）、CurrentTick、DiagnosticsWriter、SetRead、ExclusiveWorld（S-13）、HookCtx；Extract 系 GYO_ASSERT（rollback-extract）。`Fetch` 只解析 id 與複製序號，無副作用。
  - `KernelError`：上下文鏈（`context`）與並列清單（`Siblings`）、`rule`、`world`；規則代號字串（"W-5"、"F-10"、"Z5"、"S-13"…）。
- `tests/common/kernel/`：`WorldTests.cpp`、`ExecutorTests.cpp`、`AccessCheckTests.cpp`（spike 的 AccessCheckTests 搬改：違規改 Err、一個執行檔兩個 World）、`WorldTestSupport.hpp`；ctest 加 J-1～J-8、J-26、J-29～J-36、J-32b、J-45、J-50、J-52、J-53（J-37 沿用）；`kernel.symbols` 加 `--require tlCurrent`（恰等於兩個）；try_compile 加 HANDLE_COPY；`store_stamp_guard.py`＋自測；BU-2 記錄執行檔 `WorldExecRecord.cpp`（不進 ctest）；`kernel_guard.py` K5 加 `engine/entity_store/`（使用者核准）並收緊 `..` 判斷。
- 文件：target/world §6 定案；宣告的核定後修訂；突變目錄 `we-*` 93 條＋既有 5 條改寫（ks-links-extra、ks-adv-include-allowlist-widened、ks-k5-parent-directory、rs-sample-capture-accepted、rs-sample-return-accepted）。

## 受理判定（宣告 §6）

| # | 判定 | 結果 |
|---|---|---|
| 1 | 範圍 | 成立：`git diff --name-only origin/master...HEAD`＋作業樹 33 檔全在宣告第 2 節（Python 比對；正例 `apps/demo/x.cpp` 先命中） |
| 2 | L1 三平台、J 條目 × 3 | （L1 完成後填入） |
| 3 | 存取驗證 | 本機成立：三種繞路在驗證開著時以 Err 攔下（BoundaryChange／AccessViolation，帶 world 名），關著時 Ok；同一執行檔兩個 World；報告帶 `accessVerification`、`breakOnKernelError`、排除欄；debugger 欄位以 `doctest::detail::isDebuggerActive()` 印（Windows 上即 `IsDebuggerPresent()`；不 include 平台標頭，因 OS 邊界守衛） |
| 4 | 套用順序的決定性 | 本機：J-32 golden 已取（三平台待 L1）；J-44 的 20 種排列 World 摘要相同 |
| 5 | Interrupted 與 release | 本機成立（RelWithDebInfo＝NDEBUG 定義，測試印出）；診斷緩衝在 Interrupted 後可讀 |
| 6 | 符號層 | 本機 macOS 成立（TypeKey anchor 與 tlCurrent 都在，另兩個是 Base 的 foreign）；Linux 待 L1 |
| 7 | 突變 | 成立：本批相關最終 96/96 killed（期待文字列全部是失敗的 CHECK 式或專屬訊息）；J.3 第 1、7、8、9、12、13、16 條都在；Fable 敵對檢查 47 處、存活 33 處 → 加入 26 條（不加的 7 條見下）；之後兩個待決 commit 再加 2 條，killed；既有 rs-／ks- 205/208＋3 條處置後 killed、Linux 限定 1 條 skip |
| 8 | BU-2 | 只記錄（本機 Intel Mac、RelWithDebInfo）：一次 Run（400×8 欄、20 個 (c) System＋生滅）p50 17.1 µs、p95 17.7 µs；System 派發 0.78 µs／個；空同步點 0.16 µs；生滅（4 生 4 滅、8 欄）400 實體 p95 14.0 µs、4,000 實體 p95 142 µs；驗證開著 400 實體 p95 2.9 ms、4,000 實體 p95 29.8 ms（第二層斜率 27.9 µs／KiB） |
| 9 | 既有守衛不變紅 | 本機 core 全 109 ctest 綠；L1 待 |

## 經緯

1. 開工時發現 K5 會擋 Delta-3 的 include（宣告 §5 只列了 `gyo_expect_links`）→ 停下送架構師；架構師判斷是 DI 守衛的判定變更、交使用者；使用者核准；照 §14.4 寫 1～5、自測、突變、驗證者。
2. 佈局選 Cell<T>（架構師同意）；後果是元件型別多一條 nothrow move 指派（交卡 05 同步 spec W-8）。
3. we-* 第一輪 runner 因 zsh 不分割未加引號的變數，`--only` 只收到一個字串，0/0（作廢，同檔覆寫）。第二輪 57/60：ks-links-extra 的期待被 CMake 換行切斷；兩條存活是「後段的常時檢查也會抓」——測試沒有區分是哪一段攔下，補了斷言。
4. Fable 審查：程式本體 blocker 0、should 9；突變一覽 blocker 1（we-* 的期待多數只寫 `TEST CASE:  J-n:`，等於任何失敗都算 killed，違 AGENTS §14.2）。處置：hook 的 Err 改為縱向上下文鏈（葉子保留代號與規則）、hook 前後的第二層比對、摘要排除 KernelSettings、`TakeDiagnostics`、配發器標 changed、細項；存活 33 處補測試與突變；以腳本逐條套用突變、取第一個失敗的 CHECK 式寫成期待，再用正式 runner 重跑 96/96。
5. should 3、8（hook 佇列屬哪個 schedule；despawn 時 OnRemove 與刪格的交錯）是 spec 的讀法 → 架構師交使用者（建議 (a)）；照 (a) 各以獨立 commit 先做（36612cb5、3d87d916），選 (b) 時 revert。
6. OS 邊界守衛擋了測試裡的 `<windows.h>`／`_WIN32`，改用 doctest 公開的偵測函式。

## Fable 敵對檢查加入與未加入的

- 加入 26 條（`we-adv-*`）全部 killed。
- 未加入：E.2 ⓪ 在 Serial 下到不了（Run 開頭的比對先抓到；保留給 Parallel，寫明）；W-1 檢查單獨拿掉（只有 kernel 第二個缺陷才到得了，等價）；F.2 第 7 步（Resource 操作）沒有發出者（v1 的參數型別沒有 Resource 的延後插入／移除，死碼，寫明）；Interrupted 後「未套用的紀錄丟棄」要到 rollback-extract 的 Restore 才觀察得到；insert 先於 spawn、insert 先於 remove、`Fresh` 的 `>` 改 `>=`、丟棄計數、去重恆不做（要多處改動或已由其他條目涵蓋，記為後續批可補）。
- 作者的等價主張：J-2 第一欄驅動、J-32 不去重——Fable 同意；J-3 單獨拿掉 epoch 斷言——Fable 不同意，補了測試（`Detail::ColumnAt(...).Erase`）並加突變；J-52 逐筆套用到 Err——Fable 給了可構造的突變，加入、killed。

## 範囲の限界（宣告 §7＋本批）

- 共通三條（AGENTS §14、單一問題累計 2 h、總時數上限 84 h；本批未觸）。
- LIM-3 的 L1～L8（spike）照舊；第一層在 System 自己另起的執行緒上無效（L7，threading 評估）；hook 執行期間經存下參考的寫入由 hook 前後的比對抓，但同步點套用本身造成的變化不比（spec A.6 的豁免）。
- 頁保護不在本批（U1）。回滾（H.5）、Extract（H.6）不實作：Extract 參數 GYO_ASSERT；J-35 的 Restore 半改為「重建 App」測試，「未套用紀錄丟棄」觀察不到，寫進 rollback-extract 的事前宣告。
- W-6 的 ownerThread 半在 threading（J-6 只做 Running 半）。
- LIM-6 四條件中「預設跟建置變體」「被參數覆寫」「啟動日誌」屬 Host／runner，由 host 系各批（match-host、client-sim、client-frame、工具 host 的遷移批）承接；`breakOnKernelError` 的預設掛鉤是空的（Kernel 不含平台標頭），Host 以 `World::SetBreakHook` 裝 debugger trap。
- 配發器的獨占沿用較嚴的現狀（宣告第 3 節第 3 點）；BU-2 只當參考（宣告判定 8）。
- Query 的交集是「最小欄驅動＋二分查找」，不是 spec A.6 的 leapfrog（結果相同、成本不同；BU-2 記錄）。
- `kernel.symbols` Windows 不做（前批限界）。

## 給架構師

- spec W-8 加「nothrow move 指派」（Cell<T> 包法的後果）——已轉卡 05。
- 待使用者：hook 佇列依 schedule 分桶（36612cb5）、despawn 先跑全部 OnRemove（3d87d916），建議 (a)；選 (b) 時 revert 該 commit。
- spec F.2 第 7 步的 Resource 操作沒有 v1 參數型別能發出（死碼）；要不要在 spec 補一個句柄或刪這一步，交 spec 修訂。
- E.2 ⓪ 在 Serial 下不可達（Run 開頭的 Ready 窗口比對與同步點後的重取快照涵蓋）；Parallel 實作時才有意義。

## Architecture Report

- Architecture Delta：遷移計畫 §4 Delta-3（Kernel → EntityStore 一條邊；`gyo_expect_links` 改寫；K5 白名單加 `engine/entity_store/`，使用者核准）。Delta-1 的續（World／Executor 進 `engine/kernel`）。
- 新 Dependency：`GYO::Kernel → GYO::EntityStore`。Ownership、Product Boundary：不變（kernel 內無產品名；kernel_guard 綠）。
- Data Contract：World 摘要格式 `gyo.kernel.world-summary` 版本 1（`World.cpp` 檔頭）；KernelError 多 `rule`、`world`、`context`、`siblings`。
- 發現：spec F.2 第 7 步無發出者；E.2 ⓪ 在 Serial 不可達（見上）。

## L1

（PR 後填入）

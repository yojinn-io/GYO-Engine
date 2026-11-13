# 架構 v8：v8-arch-kernel-registry-schedule（Registry、簽名推描述、Scheduler、Plan）

日期：2026-11-13。Owner：架構（架構師會話開出的任務卡）。計畫：[遷移計畫](../architecture/plans/kernel/migration-plan.zh-Hant.md) §3.5、§3 共通、§4 Delta-1、§6 BU-3；事前宣告 [registry-schedule-declaration.zh-Hant.md](../architecture/plans/kernel/registry-schedule-declaration.zh-Hant.md)（使用者 2026-11-13 核准；架構師核定後的修訂 12 點在文末）。

- 執行：本卡會話（Opus 5.5／xhigh；commit 的 Co-Authored-By 照實寫 Opus 5.5），工作樹 `.claude/worktrees/jovial-williamson-de6281`，分支 `claude/arch-kernel-registry-schedule`。審查與突變一覽的敵對檢查合成一次：Fable／xhigh 只讀（宣告 §8）。驗證者未出場（沒有改既有 golden、守衛判定或 DI 守衛；本批的 golden 是新設）。
- 證據：主 checkout `build/evidence/kernel-registry-schedule/`（git 管理外；本機 Intel Mac）：`bu3-finish-record.txt`（最終程式）、`bu3-finish-record-pre-review.txt`（審查前）、`mutations-run1.txt`～`mutations-run7.txt` 與 `mutations-run4-sym.txt`、`h7-mutations.txt`、`l1-43391922180/`（審查前最後一輪 L1 的三平台 tests.xml）、`review/`（Fable 的探針、ASan 重現、突變試驗與建議目錄 `out/proposed.json`）。

## 件数の突き合わせ

**一致**——審查前最後一輪 L1（run 43391922180，`dfc6e49e`）：宣告的 25 個 J 條目（`kernel.J-10`～`kernel.J-47`，見 §2）在 linux-x64、windows-x64、macos-arm64 三份 tests.xml 各 25、全 pass（25×3＝75）；Linux core preset 另 25 全 pass。kernel 的 ctest 總數 linux／macOS 34、windows 33（沒有 `kernel.symbols`，宣告 §7 的限界）。審查後的最終 commit 的 L1 數字見文末〈最終 L1〉。突變（本機 macOS）：本批相關 187 條＝killed 186、只在 Linux 跑 1（`rs-sym-elf-bss-unread`）、survived 0、wrong_failure 0、stale 0（第六輪＋第七輪重跑的 10 條）。h7：104/104（第一輪 103，1 條因自測反例仍寫 `game::Runtime` 存活，修正後重跑相關 2 條 killed）。

## 做了什麼

- `engine/kernel/`（INTERFACE→STATIC，仍只連 `GYO::Base`）：
  - 參數型別全部（Query 的 Read／Write／With／Without／Changed／Added、Res／ResMut、EventReader／EventWriter、Commands／CommandsFor、`Spawn<Bundle>`、`SpawnWithId<Bundle>`、Despawn、Local、CurrentTick、`DiagnosticsWriter<T>`、`SetRead`、ExclusiveWorld、`Extract<Read<T>>`、`ExtractRes<R>`）的句柄與 `ParamTraits`／Contribution（`Params.hpp`、`Access.hpp`）。句柄不可複製、不可移動、建構子私有；`Fetch` 與執行期行為在後續批。
  - 簽名的靜態檢查（`SystemTraits.hpp`）：可呼叫物形狀、回傳型別、參數、自我衝突、ExclusiveWorld 單獨、16 個上限、run 條件唯讀；各理由一個 `GYO_KERNEL_REJECT_…` 標記。
  - Registry（`Registry.hpp`、`src/Registry.cpp`）：build → requires → 解析 → plugin finish → 檢查 → Plan → 凍結；錯誤代號表（D-E1～D-E19、R-E1～R-E6、X-E1～X-E3、F-E1）一次回報，錯誤帶「代號＋步驟代號」兩欄；區規則 Z1～Z8、S-6～S-9、S-13、獨占寫者、hook、親和、事件 domain、KernelSettings（Kernel 自己的 Resource，ResourceId 0）。
  - Scheduler（`src/Scheduler.cpp`）：衝突邊與順序邊分開；(c) 歧義＝D-E8；(b) 依 registrationSeq 平手、逐條更新可達性；Kahn 以 registrationSeq 為唯一平手鍵、同步點盡量晚；D-2 自檢；事件修剪釘在它的同步點後。
  - 指紋與匯出（`src/Fingerprint.cpp`）：Plan 指紋、Registry 指紋（版本 1）、C 指紋（版本 2）；Plan 文字匯出（步驟、每條 before／after 邊、判定用的 `non_dataflow_edges` 與擴充定義的 `non_dataflow_edges_extended`、平手）；G-5 的 `gyo.zone-c-sources` v1 匯出。
- `tests/common/kernel/`：J 條目每條一個 ctest（零案例＝失敗）、Plan／Registry 指紋 golden、`gyo_kernel_tests_reversed`（反轉連結順序，J-18）、J-17 的合成 (c) 檔與 `zone_c_export_check.py`、`kernel_symbols.py`（符號層的可寫全域檢查）＋自測、try_compile 樣本 7 個變體、BU-3 記錄執行檔（不進 ctest）。`kernel_guard.py` 的 K4：白名單符號定名 `typeKeyAnchor`；新增 inline 變數規則。
- `tests/common/h7_scan/` 三檔：白名單的 Kernel 型別名 `GYO::Kernel::App`（自測反例同步改）。
- 文件：spec §0／H-7／修訂記錄、`target/runtime-composition` §1 一句、`target/scheduler` §3 與 `target/system-contract` §4 的 API 名稱（不動 DI）。
- 突變目錄 `docs/architecture/plans/kernel/scripts/mutations.json`：scaffold 79 條中的樣本 5 條改寫為本批的樣本條目、`ks-links-extra` 與 `ks-real-*` 改指向；本批新增 `rs-*`（含 J.3 第 2、3、4、5、6、11、14 條；Fable 建議 33 條；審查修正 6 條；架構師決定後 14 條）。共 209 條。

## 受理判定（宣告 §6；含核定後的修訂）

| # | 判定 | 結果 |
|---|---|---|
| 1 | 範圍 | 成立（最終 commit 前以 Python 比對 `git diff --name-only origin/master...HEAD` 對宣告 §2＋修訂 1、2；範圍外 0；正例 `apps/` 路徑先命中） |
| 2 | L1 三平台、J 條目 × 3 | 審查前最後一輪成立（上節）；最終 commit 見〈最終 L1〉 |
| 3 | J-12 全參數型別 | 成立（20 個簽名＋run 條件與 set 條件併入；型別層與 id 層逐一比對）。`Fetch` 無順序相依的半條交 world-exec（修訂 4） |
| 4 | Plan／Registry 指紋 golden 三平台相同 | 成立（三個合成 Registry 的 FixedUpdate Plan 指紋與 Registry 指紋、J-18 的 Registry 指紋；審查者在第四平台 macOS x64 也相同） |
| 5 | J-44 20 種排列、J-22 不算歧義的配對 | 成立（描述半；World 摘要的半條交 world-exec，修訂 4） |
| 6 | J-24 每種代號一個最小 Registry、一次回報 | 成立（代號或步驟代號比對，修訂 5） |
| 7 | 資料流邊比例 | 成立：合成正例 5 條中 2 條非資料流→`non_dataflow_edges 2/5 40%`（字面定義，修訂 6）；擴充定義另一個案例 2/2 對 0/2 |
| 8 | G-5 匯出由 `zone_scan.py` 讀 | 成立：兩個合成 (c) 檔、位元組序排序、`generated_by = "gyo-kernel-report"`；掃描器結束碼 1、ZS1 1／ZS2 1／ZS6 1 |
| 9 | 符號層全域檢查 | 成立（修訂 3、9 的讀法：⊆ 白名單＋`--require "TypeKey anchor"`；`inline int g` 被抓；段名清單弄錯讓正例失敗）。Windows 不做（限界） |
| 10 | 突變 | 成立：187 條相關＝186 killed＋1 只在 Linux；J.3 第 2、3、4、5、6、11、14 條都在且 killed。敵對檢查加入 33 條（Fable）＋本批修正後 20 條，全 killed |
| 11 | BU-3 | 成立：finish＠500 System，Release、本機 Intel Mac，10 次中位數 **14.37 ms**（最小 13.67、最大 16.82）≤ 50 ms。審查前同量 16.08 ms |
| 12 | 既有守衛不變紅 | 由判定 2 的 L1 涵蓋 |

## 經緯

1. 開工時向架構師確認兩件範圍事（h7 三檔、Runtime 註記的範圍）與三處判定讀法（判定 9 ⊆、J-11／J-12／J-44 的執行半、錯誤代號兩欄）；全部核定，記為宣告的核定後修訂 1～5。
2. 第一輪 L1（run 43385364704）：windows-x64 configure 失敗——try_compile 不繼承目錄的編譯選項，`engine/base/Assert.hpp` 要求 MSVC 的 `/Zc:preprocessor`；macos-arm64 的 AppleClang 對 500 項的 fold 有 256 層上限（BU-3 記錄執行檔）；linux-x64 的 `kernel.symbols` 抓到 GCC 的 `DW.ref.__gxx_personality_v0`。第二輪：macos-arm64 抓到組譯器的區段標籤 `ltmp5`～`ltmp8`。第三輪（run 43391922180）三平台綠。兩個工具鏈名字是看到 L1 後加的白名單項，架構師補核定（修訂 9）。
3. 突變第一輪 61/86：24 條 wrong_failure 是我寫的 `expect` 不對（測試先停在 `REQUIRE_FALSE(result)`、不帶代號；或突變讓 Finish 成功後測試直接呼叫 `.error()` 觸發 GYO_ASSERT），1 條 survived（J-20 的 golden 沒有「無順序約束」的 System 對，打亂容器順序的突變抓不到）。改成 `ErrorCount`／`HasError`（對成功結果安全、失敗訊息帶代號），golden 加一對無順序的 (c) System（**J-20 mixed 的 golden 因此在首次產生後重設一次**，尚未合併過）。
4. Fable 審查（報告與材料在 `review/`）：主幹沒找到錯；高 2（D-E6 的環回報越界讀取、`Registrar` 可複製→凍結後還能登記）、中 8、低中 6、低 9；敵對檢查試 98、真正漏網 82、建議 33＋4 條。本批修：A1、A2（禁止複製與移動＋建置階段 GYO_ASSERT；保存的指標在 Build 後懸空，屬契約外）、B1（set 條件完整併入）、B2（同 schedule 錯誤繼續收集，只有成環停）、B4（plugin finish 前先解析具名集合與存取）、C1（hook 補查 D-E3／D-E18／S-7／S-9／S-13／Extract）、C2（X-E1 每個 schedule 一筆）、C3（`ConditionRef` 只能由 `Condition<>` 造）、D1（不同角色重複登記報 R-E2）。架構師定的七件：B5 字面為判定＋擴充並列、B6 補 C 指紋並升版、B3 修剪釘在同步點後、B8 補白名單＋K4 inline 規則、B7 補 try_compile 四變體、C4 ingress 必須是第一個 stage、C5 不在本批定（修訂 6～12）。
5. 第五輪突變在程式改動前中途停掉（會被後續修正作廢）：停止後以目錄逐條比對還原了正在突變的 `Registry.cpp`（`rs-j3-3-event-writes-not-counted`），全部 find 唯一後才繼續。第五輪結果作廢。
6. h7 的白名單改名後，第一輪 103/104：自測 `test_suffix_with_other_qualifier_is_reported` 以 `game::Runtime` 模仿舊白名單名，改名後不再相似；照宣告 §3 第 4 點同步改成 `game::App`，並加一條 `game::App` 的產品型別反例。

## 範囲の限界（宣告 §7＋修訂）

- 共通三條（AGENTS §14、單一問題累計 2 h、總時數上限）；只驗描述，不驗函式體（LIM-3）；指紋相同不代表 (c) 結果相同（G.2 第 4 處）；J-27 只有 D-E17 那半（LIM-2）。
- J-11 的全量對增量重建、J-12 的 `Fetch` 無順序相依、J-44 的 World 摘要：交 world-exec（修訂 4；HANDOFF 已記）。
- 符號層檢查：Windows 不做；本批只有 TypeKey 的錨點在，`tlCurrent` 由 world-exec 加入後收回成「恰等於兩個」（HANDOFF 已記）；`--library-newest` 取 `build/` 下最新的庫，突變不得與其他建置並行跑。
- X-E1 只查 guard id 非空（v1 的 Parallel 本來就是 D-E17）。
- 配發器的獨占（修訂 12）：預設 Exclusive 時兩個 Spawn＝D-E2，較嚴；PvP 的 `ApplyMembership` 用 `SpawnWithId`，plan-dryrun 不受影響。
- 指紋雜湊 `sizeof`／`alignof`（spec C.2 第 5 點）：測試型別都是固定寬度；產品型別若在 Linux 與 Windows 大小不同（`long`、`wchar_t`、標準容器），H-4 的比對會假性不符（審查 C6）。
- 非 clock 的 schedule 設 `eventTrim` 目前被忽略，不報錯（審查 B3 附記；未定，交架構師）。
- G-5 匯出：登記處在標頭時列出 `.hpp`（掃描器以結束碼 2 拒收，不會靜默）；zone C hook 的登記處不匯出；匯出錯誤的代號借用 `SystemFailed`（審查 D5）。
- 測試接縫 `TypeKeyCollisionProbe`、`SchedulerShuffleProbe` 在公開標頭（審查 D6）。

## 給架構師

- spec D.2 與 E 節的代號重疊（D-E10 對 X-E2／X-E3、D-E11 對 X-E1；審查另指出 F-E1 對 D-E7、R-E4 對 D-E3）：本批以兩欄處理前兩組，後兩組仍單欄；下次修 spec 時統一。
- 資料流邊的擴充定義（延後寫、With／Without）要不要進遷移計畫 G1 (ii)：交使用者（修訂 6）。
- C5（配發器的獨占與 spec W-9）、D7 兩個 spec 空白（`Changed`／`Added` 與 `Write<T>` 沒有衝突邊；zone C 持 untyped `Commands` 在 finish 不報）、C4 已收緊、C6 sizeof 的限界、非 clock schedule 的 `eventTrim`。
- 審查低項中本批未處理的：D2（run 條件帶 `ExtractRes`、在非 extract schedule）、D3（指向沒有成員的 set）、D4（System、同步點、set 同名不報錯，System 優先）、D5、D6。

## Architecture Report

- Architecture Delta：遷移計畫 §4 Delta-1 的主體（`engine/kernel` 由 INTERFACE 改 STATIC），差分只有這一點；連結仍只有 `GYO::Base`（`gyo_expect_links` 不變）。
- 新 Dependency：無。Ownership、Product Boundary：不變。
- Data Contract：`gyo.zone-c-sources` v1 的生產端多了 Kernel 匯出（格式不變）；Plan 匯出 `gyo.kernel.plans 1` 與指紋格式（Registry 1、C 2）是本批新定，寫在 `Fingerprint.cpp` 的檔頭與 target/scheduler §3。
- 命名：spec 的 Runtime 型別名 `App`（`v8-arch-kernel-app-name`）；h7 白名單同步。

## 最終 L1

（最終 commit 推送後填。）

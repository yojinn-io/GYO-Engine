# 事前宣告：`v8-arch-kernel-registry-schedule`（Registry、簽名推描述、Scheduler、Plan）

更新：2026-11-13。Owner：架構（架構師會話寫宣告；實作建議由架構師開的任務卡執行）。**狀態：已核准（使用者 2026-11-13：「名字用 App，xhigh 同意，核准開始」）。**

出處：遷移計畫 `migration-plan.zh-Hant.md` §3.5（本批）、§3 共通、§4 Delta-1、§6 BU-3、§7 U5；`target/kernel.zh-Hant.md`（DI-KS 1、3、4、5、6、7、8、10、15 與 RC 1、3、4）；`target/scheduler.zh-Hant.md`、`target/system-contract.zh-Hant.md`、`target/naming.zh-Hant.md`、`target/world.zh-Hant.md` §3；`docs/architecture/zone-c-sources.zh-Hant.md`（`gyo.zone-c-sources` v1）；spec §B（System 契約）、§C（Registry）、§D（Scheduler）、§G.1（區規則）、J.2、J.3；kernel `HANDOFF.md` 現況「kernel-scaffold 合併後交給後續批的事」「h7-scanner 合併後交給後續批的事」。

## 1. 目的（Feature → Contract → Architecture）

- **Feature**：跨產品能力「Match 與 Client 的預測共用同一份模擬核心」（object_fps_pvp 的 Match 與 Client；玩家可見：預測不因兩端順序不同而回彈）。開發者可見：產品加 System 時，順序歧義、第二寫者、區違反在 finish 一次列出。本批結束時＝里程碑 M1 的前半：Plan 可印出、三平台指紋相同。
- **Contract**：Registry 的生命週期（build、finish、凍結）；SystemDescriptor 從簽名推出，涵蓋**全部**參數型別的描述與 Contribution；Plan 唯一、(c) 的歧義是啟動錯誤、(b) 依登記序平手；錯誤代號表一次回報；Plan 指紋與 Registry 指紋帶版本欄；區規則 Z1～Z8 在 finish 檢查；G-5 匯出的 (c) 來源檔清單照 `gyo.zone-c-sources` v1。
- **Architecture**：遷移計畫 §4 Delta-1 的主體（Engine 子系統 `engine/kernel`；使用者已同意）。只做到「描述→Plan」，不跑 System（Executor 在 world-exec）。`gyo_kernel` 因為有了 `.cpp`，由 INTERFACE 改成 STATIC；連結仍只有 `GYO::Base`。

## 2. 範圍

| 範圍 | 內容 |
|---|---|
| `engine/kernel/`（新增與修改） | Registry（`Registrar`／`FrozenRegistry`、Plugin 小介面）、元件／Resource／事件的登記參數（zone、writePolicy、role、rollback）、SystemDescriptor 與 `ParamTraits`（全部參數型別：Query 的 Read／Write／With／Without／Changed、Res／ResMut、EventReader／EventWriter、Commands／CommandsFor、`Spawn<Bundle<…>>`、`SpawnWithId<Bundle<…>>`、`SetRead`、`Local`、`ExclusiveWorld`、`CurrentTick`、`DiagnosticsWriter<T>`、`Extract<Read<T>>`、`ExtractRes<R>`；執行期行為由後續批實作）、ScheduleDef、Scheduler（衝突邊、順序邊、Plan）、錯誤代號表、Plan 與 Registry 指紋、Plan 匯出（含資料流邊比例）、G-5 的 (c) 來源檔清單匯出；`CMakeLists.txt` 改 STATIC |
| `tests/common/kernel/`（新增與修改） | J-10～J-25、J-27（D-E17 那半）、J-28（finish 那半）、J-37～J-41、J-44、J-47；Plan 指紋 golden；資料流邊比例的合成正例；J-17 的 kernel 端自測；BU-3 的記錄執行檔（不進 ctest）；符號層的可變全域檢查（第 3 節）；`kernel_guard.py` 的白名單符號定名；突變目錄 |
| spike 測試的搬改 | `spikes/kernel/tests/AccessDerivationTests.cpp` 搬改為 J-12；`RegistryWorldTests.cpp` 的 Registry 部分搬改為 J-18／J-19，其餘重寫（ci-readiness §2.2）。`spikes/kernel/` 本身不刪（刪除在 `v8-arch-ci-spike-retire`） |
| target 文件 | `target/scheduler.zh-Hant.md`、`target/system-contract.zh-Hant.md`：寫入本批定的 API 名稱（只加名字與它對應的條文，不改已核准的 Design Invariants；要改 DI 的列給使用者） |
| 其他 | `docs/architecture/plans/kernel/scripts/mutations.json`（本批的突變）；dev_log；kernel README 與 HANDOFF |
| h7 白名單（核定後的修訂 1） | `tests/common/h7_scan/` 的三檔：`h7_scan_rules.json`（白名單的 Kernel 型別名）、`test_h7_scan.py`（自測的同一名字）、`mutations.json`（h7 那條突變的 find 字串） |
| 型別名註記（核定後的修訂 2） | `spec.zh-Hant.md`（§0 用語表、H-7 白名單暫名、修訂記錄）；`target/runtime-composition.zh-Hant.md` §1「名詞」Runtime 定義後一句 |

不動：`build/`、`.github/`、`plugins/`（還不存在）、`apps/`、`tools/`、`framework/`、既有守衛（第 5 節）。範圍外還要改檔才能完成＝停止條件。

## 3. 從前兩批接手的事（kernel HANDOFF 現況已記）

1. **符號層的可變全域檢查**（kernel-scaffold 審查 A6 的前半）：J-9 是關鍵字掃描，看不到沒有 `static` 的 namespace 範圍變數（`inline int g`、匿名 namespace 變數、`extern`）。本批新增：對 `gyo_kernel` 靜態庫的可寫資料段（.data／.bss，以及 thread-local 段）列符號，白名單恰好兩個——`tlCurrent` 與 TypeKey 的那個符號。Linux 與 macOS 用 `nm`；Windows 記為限界（第 7 節）。
2. **K4 白名單的暫名 `typeKeyTag`**：本批定 TypeKey 的實作時把腳本裡的暫名換成實際符號，自測與突變跟著改。
3. **編譯失敗樣本的分工**（U5 已定）：J-10、J-11、J-13 用 try_compile（要有對照組；CMake 會把訊息折行，比對時去掉換行；include 目錄與 `GYO::Kernel` 一致），其餘用 trait＋doctest。
4. **h7-scanner 的白名單**：若本批定了 Kernel 執行器型別的名字（第 4 節），同批改 `tests/common/h7_scan/h7_scan_rules.json` 一行與它的自測反例；若名字留到 world-exec，本批不碰。

## 4. 要使用者定的事（開工前）——**已定**

**使用者 2026-11-13 定：型別名用 `App`（選項 (a)）；實作者 Opus／xhigh 同意。** kernel HANDOFF 決定行 `v8-arch-kernel-app-name`。

**Kernel 執行器型別的名字**。spec 用 `Runtime` 稱呼「一個 World、它的 Registry、它的 schedule 集合加每個 schedule 的執行策略」，但 target/naming 第 5 條禁止新程式用 `Runtime` 當型別名尾，兩者衝突（遷移計畫 §3.5 指定本批處理）。選項：

| 選項 | 名字 | 取捨 |
|---|---|---|
| (a) 建議 | `App`（`GYO::Kernel::App`） | 與 Bevy 的 `App` 同名、同義（World＋schedules＋plugins），開發者一看就懂；是名詞、不帶舊角色後綴。代價：spec 與 target 文件裡的「Runtime」字樣要逐處改，或註明「Runtime（型別名 App）」 |
| (b) | `Simulation`、`Instance` 等其他名詞 | 不帶 Bevy 的聯想；但 Client 的渲染 Runtime 不是模擬，`Simulation` 不準 |
| (c) | 保留 `Runtime` 並在 naming 第 5 條列例外 | 改動最少；但第 5 條的理由正是「命名擁有東西的物件，正是機制要消掉的形狀」，例外會削弱它，且要改已核准的 Design Invariant（Architecture Delta） |

本批只建 Registry、Scheduler 與 Plan，不建這個型別本身（它在 world-exec 才有實體）；但名字會出現在 Registry 的 API 與文件，所以在本批定。使用者選定後，本批照選項改 spec 的用語註記與 target 文件。

另一件**要使用者同意的指派**：實作者 Opus／**xhigh**（遷移計畫 §3.5 的指定：決定性與平手規則屬「気づかないうちに誤りやすい」）。navigator 規定檔位超過主會話時要同意。

## 5. 守衛與突變的去處（計畫 §3 共通）

| 對象 | 去處 | 說明 |
|---|---|---|
| `kernel_guard.py`（J-9） | **改寫**（締める面） | 白名單的暫名換成實際符號；`engine/kernel` 有了 `.cpp`，掃描範圍不變（本來就掃整個目錄）。若新程式碰到既有規則（例：K5 的 include 白名單要加某個標準標頭以外的檔），只能收緊，放寬＝停下交使用者 |
| `naming_guard.py` | **留** | 新程式照 namespace `GYO::Kernel` 與禁止後綴；本批的新型別要零命中 |
| `gyo_expect_links(gyo_kernel GYO::Base)` | **留** | INTERFACE→STATIC 不改期望 |
| 編譯失敗樣本 (a)(b) | **改寫** | 測試用的最小拒絕條件換成 J-10 的正式條目；樣本的對照組形狀保留 |
| spike 測試 | **改寫** | 搬改為 J-12、J-18、J-19（第 2 節） |
| 新守衛：J-10～J-47 本批部分、Plan 指紋 golden、資料流邊比例的正例、符號層全域檢查 | **新增** | 突變一覽照 AGENTS §14.2 受一次敵對檢查（第 8 節） |
| 既有突變清單（`docs/architecture/plans/*/scripts/*.json`） | **不碰** | 本批不改它們的對象 |

## 6. 判定（看結果前宣告）

| # | 判定 | 門檻 |
|---|---|---|
| 1 | 範圍 | `git diff --name-only origin/master...HEAD` 全部落在第 2 節的範圍內（Python 比對，先以一個已知範圍外的路徑當正例確認比對有效）；範圍外 0 檔 |
| 2 | L1 三平台 | linux-x64（含 core preset）、windows-x64、macos-arm64 全綠；第 2 節列的 J 條目在三份 `tests.xml` 各出現且通過；件数の突き合わせ寫在報告第一行（宣告的 J 條目數 × 3 ＝ 實際出現數） |
| 3 | J-12 涵蓋全部參數型別 | 第 2 節列的參數型別各至少一個簽名，推出的存取集合與手寫期望逐一相同；漏一個＝不成立 |
| 4 | Plan 指紋 golden | 三個合成 Registry（(c)／(b) 混合、延後寫、事件寫）的 Plan 指紋在三平台逐位元相同；Registry 指紋（J-18）同樣 |
| 5 | 平手與歧義 | J-44 的 20 種排列全過；J-22 的「不算歧義的配對通過」各例過 |
| 6 | 錯誤代號 | J-24：D-E1～D-E19、R-E1～R-E6、X-E1～X-E3、F-E1 每一種一個最小 Registry，報告指名雙方、多個錯誤一次回報 |
| 7 | 資料流邊比例 | 合成正例：已知 5 條 before／after 邊中 2 條非資料流邊，匯出印 40% |
| 8 | G-5 匯出 | 合成 (c) System 檔由匯出產生 `gyo.zone-c-sources` v1（`generated_by = "gyo-kernel-report"`），`tests/common/zone_scan/zone_scan.py` 讀得進，違規數與預期相同 |
| 9 | 符號層全域檢查 | Linux、macOS：可寫資料段的符號集合恰等於白名單兩個；突變「加一個 `inline int g`」被檢出 |
| 10 | 突變 | 本批突變目錄在本機全殺（預期失敗可識別才算）；必含 spec J.3 第 2、3、4、5、6、11、14 條；敵對檢查加入的處數與加入後的檢出數寫進報告 |
| 11 | BU-3 | finish（Registry 凍結＋Scheduler 分析）在 500 個合成 System 下 ≤ 50 ms（遷移計畫 §6；本機 Release，記錄執行檔、不進 ctest；量 10 次取中位數，並記錄散佈） |
| 12 | 既有守衛不變紅 | 由判定 2 的 L1 涵蓋 |

掃描類判定照 AGENTS §14.2：`git grep -P` 或 Python、不丟 stderr、以結束碼區分「無」與「失敗」、先以已知正例命中。

## 7. 範囲の限界

- 共通三條（AGENTS §14、單一問題累計超過 2 h、總時數超過估時上限 1.5 倍）。
- 只驗描述，不驗函式體的實際存取（LIM-3：存取驗證在 world-exec）。
- 指紋相同不代表 (c) 的結果相同（Math 與編譯是 spec G.2 第 4 處，Kernel 檢查不了）。
- J-27 只有 D-E17 那一半（LIM-2：v1 只有 Serial）。
- 符號層全域檢查在 Windows 不做（`nm` 不可用；MSVC 的 `dumpbin` 形狀不同，記為限界，threading 或之後的批再評估）。
- BU-3 是推定的預算，量在本機 Intel Mac；四環境量測（`v8-arch-measure-env-baseline`）的結論可能改量測機器的規則，屆時本批的數字只當參考。
- G-5 的 `source_location` 只知道登記處的 `.cpp`；產品 System 跨編譯單位時照 `gyo.zone-c-sources` 的 Replacement Criteria 改版本，不加欄硬撐。

## 8. 模型／檔位

上段（公開介面、Design Invariants、決定性）。

- 實作者：**Opus／xhigh**（待使用者同意，第 4 節）。建議由架構師開任務卡執行。
- 審查者兼突變一覽的敵對檢查者：**Fable／xhigh，只讀，一次**（與作者 Opus 不同模型；照使用者 2026-11-13 同意的臨時做法，Fable 每批只用一次，審查與敵對檢查合併由同一個 agent 做，報告分兩節）。
- 驗證者：只在改 golden、守衛判定或 DI 守衛時出場，Opus／high 另一代理；本批預期不出場（守衛都是新增或收緊）。

## 9. 估時與停止

- 估時：**21～42 h**（遷移計畫 §2.2，推定）＋守衛去處判斷 1～2 h＋接手的符號層檢查 2～4 h（推定）。總時數上限 48 h×1.5＝72 h。
- 停止：Plan 指紋在某平台不同、修兩次仍不同→停（spec K.2 第 1 條 (a)、第 4 條；計畫 §8 RC-2）；合成測試裡就出現「為消 D-E8 加的順序一半以上與資料流無關」→停（K.2 第 3 條）；範圍外還要改檔；放寬既有守衛；AGENTS §14 冒頭三條。

## 10. 若失敗

後面全部白做（World、Executor、四個 host 都假設同一 Registry 得同一 Plan、三平台相同）。plan-dryrun（閘門 G1）、world-exec、plugins-foundation 的最小形都不開工。

## 核定後的修訂（宣告錯誤的修正；架構師 2026-11-13 核定，緩める面：無，除第 4 點的縮窄另述）

1. **§2 範圍表補 h7_scan 三檔**：§3 第 4 點已授權改 h7 白名單，但 §2 漏列。補一列；判定 1 的比對清單列入。h7 自己那條突變改了 find，本機重跑 h7 的相關突變一次，結果寫進 dev_log。
2. **型別名註記的範圍**（選項 (a)）：「凡指型別處」解讀為以 C++ 型別引用它的地方。spec §0 用語表加一句、修訂記錄一行，spec 內以 C++ 型別寫的 Kernel 名改 `App`（只有 H-7 白名單一處，`git grep -nP 'Runtime(::|&|\*|<)'` 與反引號形式查，正例先命中）；target 只改範圍內的 system-contract 與 `runtime-composition` §1 一句。其餘 target 檔不動。
3. **判定 9 改成「⊆ 白名單」**：`tlCurrent` 屬 world-exec，本批不會有。條件：(i) 防空轉正例——檢查同時斷言 TypeKey 的符號確實在（`--require`）；(ii) 突變兩條：加 `inline int g` 被抓；段名清單弄錯（漏 .bss／__data）讓正例失敗。world-exec 加 `tlCurrent` 後收回成「恰等於兩個」（締める面；kernel HANDOFF 記一行）。GYO::Base 的兩個 inline 變數（`assertionHandler`、`reportingAssertion`）出現在 kernel 的目的檔裡，報告另列為「外來」，不算 kernel 的符號。
4. **J-11、J-12、J-44 的執行半交 world-exec**（遷移計畫 §3.5 受理判定的縮窄）：J-11 的「全量對增量重建」等價、J-12 的「兩種參數順序檢查 `Fetch` 無順序相依」、J-44 的「World 摘要不變」要 World 與執行，本批不做；本批做描述半（J-44＝20 種排列下全部 D-E8，或每對衝突 System 的先後不變）。world-exec 的事前宣告必須把這三個半條列進判定（kernel HANDOFF 記一行）。
5. **錯誤代號兩欄**：一筆錯誤帶「代號」與「步驟代號」。親和不符＝代號 X-E2／X-E3、步驟 D-E10；(c) Parallel 無守衛＝代號 X-E1、步驟 D-E11（v1 另報 D-E17）。J-24 以「代號或步驟代號」比對，同一問題只報一筆。X-E1 的「守衛測試存在且帶 cpu 標籤」v1 只查 guard id 非空，其餘記限界。spec D.2 與 E 節的代號重疊留給架構師下次修 spec 時統一。

6. **資料流邊比例（審查 B5）**：判定（G1 (ii)）用遷移計畫的字面定義 `writes(u) ∩ (reads(v) ∪ writes(v))`，writes＝立即寫（欄、Resource、事件、配發器），reads＝立即讀（含 run 條件與 SetRead 展開）。實作原本多算的兩種（u 的延後寫、v 的 With／Without）保留為匯出的第二個比例 `non_dataflow_edges_extended`（擴充定義，不判定）。要不要擴充定義是遷移計畫的條文，交使用者（架構師轉）。字面是較嚴的一邊。
7. **C 指紋（審查 B6）**：補 zone C 的 hook、跑 zone C System 的 schedule 的 `rollbackEnabled`／stage 序列／`eventTrim`，以及該 schedule 的 Plan 只留 zone C step 與全部同步點、事件修剪（同步點相對 (c) step 的位置）；各段先寫數量。格式版本升為 2（`Fingerprint::version`）。收緊。
8. **事件修剪的位置（審查 B3）**：`eventTrim` 指具名同步點時，修剪緊接在該同步點之後，排在它之後的節點都在修剪之後（與 stage 末的寫法同義）。golden 是本批新設：三個 golden 與 J-18 的 Registry 指紋都不含具名同步點的修剪，數值沒變（只有 J-20 mixed 的 golden 因審查前加入一對無順序約束的 System 重設過一次，見 dev_log）。
9. **判定 9 的白名單補兩個工具鏈名字（審查 B8）**：GCC 的 `DW.ref.__gxx_personality_v0`（只在 ELF 的符號表適用，例外處理的間接參考）與 arm64 Mach-O 組譯器的區段起點標籤 `^ltmp\d+$`（只在 Mach-O 的符號表適用）。各以完整比對、各附理由註解與 lookalike 自測（`DW.ref.foo`、`ltmpX` 必須被報；換了格式也必須被報）；報告另列「外來或工具鏈」，不算 kernel 的符號。另：**K4 加規則**——沒有 `static`／`thread_local` 的 `inline` 變數（命名空間範圍）＝違規，`constexpr`／`consteval`／變數本身 `const`、inline 函式、inline namespace 不算；只在標頭、kernel `.cpp` 沒 odr-use 的形狀由它抓（符號表看不到）。收緊；自測加正反例、突變兩條。既有兩個自測以 `inline int …`／`inline const char* …` 當其他規則的載體，換成 `constexpr` 載體（判定不變）。
10. **try_compile 的分工（審查 B7）**：照宣告字面，J-11（自我衝突）、J-13（條件帶寫入）、成員函式、hook 參數四個變體補成 try_compile（各自的 `GYO_KERNEL_REJECT_…` 標記；對照組照 scaffold 的做法）；trait＋doctest 仍保留。
11. **ingress stage 必須是第一個 stage（審查 C4）**：Z3 寫的條件，只比名字是漏檢；以 D-E7（規則 Z3）報。收緊。
12. **配發器的獨占（審查 C5）不在本批定**：預設 Exclusive 時兩個 `Spawn` 的 System＝D-E2（較嚴）；spec W-9 說 spawn 由衝突序列化，兩者的關係交使用者（架構師下次修 spec）。J-15 的 Spawn 案例把配發器設 Shared 以隔開這件事，測試加註。PvP 的 `ApplyMembership` 以 `SpawnWithId` 生成（spec H.2b 第 4 點），不經配發器，所以 plan-dryrun 不受影響（已對照 spec 確認）。記為本批限界。

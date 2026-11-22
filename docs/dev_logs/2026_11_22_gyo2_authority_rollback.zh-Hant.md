# 2026-11-22 權威 Module＋回滾 Module（`v8-arch-gyo2-authority-rollback`，GYO2 第 4 批）

Owner：架構（GYO2 主會話開的任務卡）。分支 `claude/gyo2-authority-rollback-bda9ba`（draft PR #259，base `gyo2`）。事前宣告：[`authority-rollback-declaration.zh-Hant.md`](../architecture/target/v2/authority-rollback-declaration.zh-Hant.md)（修訂 6）。實作者 Opus 5.5／high，審查者 Fable 5.2／high（第 4、5 步各一次）。第 8 步（完整突變、敵對式檢查、§14.4 檢證、整條線的審查）做完時在本頁末追加。

## 為什麼

使用者 2026-11-14 定 GYO2 的目標：通用引擎，PvP 的強化不影響其他遊戲、單機不增加複雜度。第 1～3 批把 Kernel 的機制、編排層與時間 Module 立起來後，Kernel 還背著舊 PvP 的權威與回滾規則（區、寫者數量、C 指紋、Z6、D-E16……）。本批把它們換成兩個作者自選的 Module：選了伺服器權威才有權威的檢查，選了回滾策略才有歷史；兩者都不選的單機 World 和以前一樣簡單（R-4、DI-A 1）。

## 做了什麼（照宣告第 14 節的步）

- **第 1 步**：第 3 批延下來的三條守衛（時鐘的識別字、`naming_guard` 的 `world_time` 列、`world_time` 的 `gyo_expect_links`）；逐型別雜湊 `Hasher`／`HashValue`（`engine/ecs/kernel/include/engine/kernel/Hash.hpp`），`World::Summary` 格式 3、缺函式回 `HashMissing`；比摘要的測試型別補函式。
- **第 2 步**：拿掉沒有新家的規則——寫者數量（D-E2）、區的規則（S-6～S-17 的區半、Z1～Z3、Z7）、`DigestSet`、D-E9、X-2；`ExclusiveWorld` 的原語 1～4（`ReadColumn`、`Entities`、`Stamp`、`ResourceStamp`、`Restore`）；`Writes<…>` 收元件；訊息寫者名單；`Resimulation` 每次準備都放 false。golden 第一次重產。A11 的清單交使用者（19.1，使用者：「我累了，你自己看著辦。」照本卡建議做）。修訂 5：`Zone` 欄位留到第 6 步。
- **第 3 步**：編排層的共用槽 `ModuleBuffer`（`Create<T>`／`Find<T>`、`Build` 後凍結）；`engine/modules/{rollback,authority}` 骨架；回滾的公開登記與裁決 `Resolve`；兩個策略的插口與 P2P 佔位（RB-E6、AU-E1）。
- **第 4 步**：回滾的官方策略 `StateRollbackStrategy`（記錄、判斷、還原、重跑、輸入歷史、權威值入口 `RollbackInbox`、`RollbackAbandoned`）；啟動檢查 R-7（Derived 的順序）、RB-E8（跨 schedule 的訊息）、RB-E9（輸入歷史短於深度）；新頁 [`rollback.zh-Hant.md`](../architecture/target/v2/rollback.zh-Hant.md)；判定 5 的量測程式與離線腳本。
- **第 5 步**：權威的登記 `AuthorityEntry` 與代為登記（`OnRegister`）；`ServerAuthorityStrategy` 的檢查 AU-E2～AU-E9（動態插入、名字、tick 只有一個來源、Z6、Z8、D-E15、Runs）；一致性指紋、digest、判定資料集、G-5 匯出 `gyo.authority-sources`；掃描工具 `zone_scan` 改名 `authority_scan`；權威側的 J-17；新頁 [`authority.zh-Hant.md`](../architecture/target/v2/authority.zh-Hant.md)。
- **第 6 步**：拿掉有新家的規則——`Zone`、`Role`、`Rollback`、`rollbackEnabled`、`notInRollback`、`ingress`、`rollbackSafe`、Z6、Z8、D-E15、D-E16、C 指紋、G-5 匯出；Registry 指紋 8→9、Plan export 6→7（Plan 指紋 8 不變，golden 原樣通過）；golden 第二次重產；突變的處置（9.4）；標題還寫著區或 Shared 的 17 案改名（9.2）。
- **第 7 步**：scenario 第 5 段（見下）；Kernel、world-time、編排層的 DI、限制頁、索引（草稿，合併前核准）；本 dev_log。
- 途中三次跟上 `gyo2`（merge，不 rebase）：#257 之後、#261（core、ecs、orchestration 不登記）、#262（os-wait；照約定改 `world-time.zh-Hant.md` 的 core/time 一行）。

## 和宣告字面不同、合併前要給使用者看的（本卡照 L17 判，條文標「實作」）

- scenario 是**第 5 段**（宣告寫第 3 段；第 3、4 段在本卡動工後被日誌、trace 用掉）。案例 S5-1～S5-8 對宣告 9.3 的 S3-1～S3-8。
- 權威：Z6 不看「重跑時不跑」的 System 與有 `rollback.Input` 歷史的輸入（審查 1-5：輸入不能回滾、步上消耗它時沒有出口）；`OneEndOnly` 可以是 System、hook、observer；一致性指紋收 set 的執行條件與 observer 看的對象、不收回滾的「重跑時不跑」條件與它帶進來的 `Resimulation` 讀取；名字標記的型別不進 digest。
- 回滾：RB-E9（輸入歷史短於深度，新碼）；入口在 C 前進而不回滾時也修剪；比對只看 C 當時存活的實體；重跑後輸入放回這一幀的值。
- 新型別：`RollbackFailure`（E2，NotImplemented）、`AuthorityFailure`（E2，NoAuthority、SourceOutsideRoot）。宣告 10.3：新型別在合併前核准時給使用者看。
- 突變的數量（README 第 8 條「變更的 DI 條數 × 5」的參考）：見下。

## 驗證

- L1（手動 dispatch，三平台 linux-x64、windows-x64、macos-arm64）：第 1、2、3 步各一次；第 4 步 5da2efc1、15da750d；第 5 步 1855d16a、20279eec；第 6 步 8d41da98——全部 success。
- 本機 macOS（x86_64，Intel i9-9980HK；對應 macos-x64，L1 不跑這個平台）`ctest -L 'cpu|shader'`：第 6 步完成時 159/159；第 7 步加 scenario 第 5 段與 `world_time.carry-over`（兩個 carry-over 案原本不在任何 ctest，CI 不跑；第 7 步補）後 161/161。
- **判定 5（回滾 Module 的預算）**，使用者的 Linux 實機（Ryzen 7 6800H，經 ssh），2026-11-21 20:21～20:37，程式 9d956ed5，條件同 11-20 的基準（governor powersave、EPP balance_performance、CPU 8、預熱 60 s、10 個行程 × 1,000 樣本）。件數的突き合わせ：一致。
  - (1) rb_record_P1 37.871 µs ≤ 156 µs：通過。
  - (2) rb_rollback_P1 708.085 µs ≤ 3,125 µs：通過（扣掉 20 × 標格 System 單獨一步的中位數）。
  - (3) P2÷P1 5.347 ≤ 1.5 × 4.018＝6.027：通過；分母兩列的四分位距比 1.7%、4.6%。
  - 中止一次：5da2efc1 的量測程式在重跑時沒有格改變、也沒扣掉標格的 System（審查 5-1，和宣告 5.4.5 不同），第 3 輪開始時中止，結果沒看。改程式（9d956ed5）後重量。
  - 未量（只記錄的項）：模式 Check 的全掃比對、「實際組成」、冷快取、部分元件、digest 與 `Summary` 的大規模時間。
  - 之後回滾的修正（審查的 1-2、1-3、1-5、1-6）不經過量測走的路徑（模式 Always、沒有權威值、沒有輸入）。
- 審查（Fable 5.2／high）：
  - 第 4 步：回滾的主幹成立；找到入口的三個洞（C 前進沒有新值時、值指向不存活的實體、入口只在回滾後修剪）、輸入的兩個缺陷、13 個臨時突變存活、量測程式的量法不符。修正前先寫會失敗的測試（Red 確認），突變補 18 個。1-1（C 之前最後的值）照 lightyear 的增量前提寫進 R-8，不改程式。
  - 第 5 步：沒有架構漂移；Windows 路徑大小寫的回歸、指紋漏收的三項、`OneEndOnly` 的矛盾、Z6 擋輸入、檢出力（18 個臨時突變一次全過、DI-A 12 的守衛是空的）。全部修，測試補 13 案，突變補 39 個。
- 突變（相關的每步都跑；完整跑在第 8 步）：回滾 47/47、權威 75/75、`authority_scan` 18/18 killed；Kernel、編排層、`world_time` 的改寫與新增各自跑過，killed。
- 證據：主 checkout 的 `build/evidence/gyo2-authority-rollback/`——`budget-v1/`（交付包、`RESULT.zh-Hant.md`、`linux-20261121/` 的輸出與中止的那次）、`step2-retired-mutants.txt`、`step6-retired-mutants.txt`、`review-fix-backup.patch`。審查的探針在會話的 scratchpad（未保存，只是出處）。

## 突變

加（本分支對 `gyo2` 的差，README 第 8 條）：

| 清單 | 加 | 退 | 現在 | 說明 |
|---|---|---|---|---|
| `rollback_mutations.json`（新） | 47 | 0 | 47 | 回滾份 R-1～R-8、RB-E1～RB-E9、入口形狀；第 4 步審查補 18 |
| `authority_mutations.json`（新） | 75 | 0 | 75 | 權威份 DI-A 1～12、AU-E1～AU-E9、指紋內容逐項、digest、G-5；第 5 步審查補 39 |
| Kernel `mutations.json` | 28 | 109 | 656 | 加：逐型別雜湊 16、原語與 `Restore` 7、訊息寫者 4、時鐘的識別字 1。退：第 2 步 47、第 6 步 62 |
| `orchestration_mutations.json` | 7 | 1 | 242 | `ModuleBuffer` 的凍結、建、找；退 `or-gi-writer-uncounted`（寫者數量拿掉） |
| `world_time_mutations.json` | 4 | 1 | 95 | 雜湊函式、`Resimulation` 不帶、`naming_guard` 的列兩個；退 `wt-tick-zone-c`（區拿掉）；`wt-links-more` 讓給 gyo2 的同名 |
| `authority_scan/mutations.json` | （18，從 `zone_scan` 搬來） | 0 | 18 | 改名，不是新增 |

權威 75 個超過「DI-A 12 條 × 5＝60」的參考：指紋的內容表有 16 項，每項是獨立的誤點（README 第 3 條），加上 AU-E 九碼各自的兩面（漏報與誤報）。合併前核准時說明。

不加的：scenario 第 5 段（它抓的錯已有回滾、權威的突變各自抓）；`Adopt` 不做（入口永遠留 C 以前最後的值、還原先用它，等價）；沒選策略時「仍加入口 System」（World 一定停在 AU-E2，從外面看不到）。**第 8 步更正**：前兩項的理由有反例——AU-E1 訊息的「is not implemented yet」只有 scenario 的 `[s5-6-placeholder]` 抓（訊息措辭，照分類不加突變）；`Adopt` 不是等價（權威值等於 World 現值時還原不寫格、重跑也不重記，歷史留著舊預測），第 8 步補了案與突變 `rb-history-not-adopted`。

退役（機能拿掉，README「事後報告」；一行一個理由，id 照字母序）：

- 第 2 步（47）——寫者數量、區的規則、`DigestSet`、D-E9、X-2 拿掉（9.1 A1～A3、A10、A11 的 A 組）：`ks-s2-re7-digest`、`ks-s2-re7-explicit-zones`、`ks-s4-annotation-counted-as-writer`、`ks-s4-annotation-in-written-keys`、`ns-decl-ingress-allowed`、`ns-decl-ingress-by-stage`、`obs-event-zone-unset-allowed`、`obs-not-a-writer`、`obs-zone-access-unchecked`、`obs-zone-c-local-allowed`、`obs-zone-unset-allowed`、`rs-adv-condition-change-filter-dropped`、`rs-adv-exclusive-resource-writers-skipped`、`rs-adv-hook-unset-zone-ok`、`rs-adv-hook-zone-rules-off`、`rs-c4-ingress-anywhere`、`rs-fix-c1-hook-local-allowed`、`rs-fix-c2-xe1-per-system`、`rs-iv-fn5-hook-zone-without-funnel`、`rs-iv-fn9-funnel-counts-as-writer`、`rs-iv-fx10-hook-funnel-counts-as-writer`、`rs-iv-fx12-declaration-Z1-dropped`、`rs-iv-fx13-declaration-Z1-exempts-input`、`rs-iv-fx14-funnel-zone-C-systems-only`、`rs-iv-fx17-ingress-input-exemption-not-through-funnel`、`rs-iv-fx5-note-dropped-Z1-site`、`rs-iv-fx6-note-dropped-Z3-site`、`rs-iv-fx7-note-dropped-Z2-site`、`rs-j14-default-zone-b`、`rs-j14-effects-allowed`、`rs-j14-local-allowed`、`rs-j16-digest-shared-allowed`、`rs-j24-de9-removed`、`rs-j27-guard-unchecked`、`rs-j3-11-skip-bundle`、`rs-j3-11-skip-deferred`、`rs-j3-11-skip-hooks`、`rs-j3-14-z1-reads-only`、`rs-j3-4-z2-removed`、`rs-j39-boundary-c-write`、`rs-j39-ingress-for-all`、`rs-j40-ingress-stage-unchecked`、`rs-j41-writes-unchecked`、`spa-reserved-short`、`spa-reserved-short-unchecked`、`sr-zone-always-b`、`sr-zone-c-own-cells`。
- 第 6 步（62）——id 與一行理由在 `build/evidence/gyo2-authority-rollback/step6-retired-mutants.txt`，分組：
  - Z6、B3、漏斗、Z8、D-E15 搬到權威（19 個，權威的 `au-z6-*`、`au-funnel-*`、`au-z8-unchecked`、`au-de15-unchecked` 接手）；
  - D-E16 搬到回滾（2 個，`rb-r7-*` 接手）；
  - C 指紋搬到權威（19 個，`au-fp-*` 接手）；
  - G-5 搬到權威（5 個，`au-g5-*` 與 `authority.sources_scan` 接手）；
  - 欄位拿掉、無物可丟（4 個）；
  - A11 的 B 組 4、C1 5、C2 1（案例併進 `ks-s4-layer2-components-only`）；
  - 區專屬的平手規則 3。
  - A11 的 C1 退役後，它們守的判定（J-6、J-45、J-57 的「任何值都寫得進」）暫時沒有突變（19.1 的「不補」）；限制不回來由型別保證（欄位不存在）。
  - **第 8 步更正**（敵對式檢查 adv-a 第 8 節）：上面寫「接手」的有 7 類其實沒人接——`hf-c-fp-*` 四個（hook 清空輪上限）、`obs-cfp-registration-order`、`obs-cfp-no-access`、`rs-iv-fb8-C-request-not-hashed`、`rs-iv-ff4-C-takes-zone-B`、`rs-j17-no-dedup`、`rs-b6-cfp-version-1`。前六類第 8 步補了案與突變（`au-fp-hook-round-limit-dropped`、`au-fp-observer-order-taken`、`au-fp-observer-access-dropped`、`au-fp-initial-request-dropped`、`au-fp-every-initial-value`、`au-g5-not-unique`）；版本常數（`rs-b6-cfp-version-1`）照分類屬低價值，不補，這裡更正為「退役、不接手」。證據檔 `step6-retired-mutants.txt` 是當時的紀錄，不改寫。

同時修好本分支前幾步就讓 find 對不上的 17 個（第 1～3 步改了程式、當時沒有重跑那些突變）：`sr-*` 五個、`we-j32-spawn-before-despawn`、`msg-registry-*` 兩個、`ks-s4-annotated-settings-refused`、`ks-s11-annotated-writes-not-hashed`、`ks-s5-*` 兩個、編排層 `g8-*` 五個與 `or-skeleton-after-units`、`sc-s2-real-time-carried`、`wt-resimulation-not-put`；以及 expect 還要求 Z1 的 `rs-j12-condition-reads-dropped`。

不是本分支的：`gyo2` 上本來就 stale 的 7 個（`el-kernel-print-back`、`ks-k6-calls-off`、`ks-k6-every-file-exempt`、`ks-s2-interrupted-stops-refusing`、`obs-exclusive-err-not-left`、`obs-host-err-not-failed`、`obs-host-no-snapshot-after`），列為後續事項。

## scenario 第 5 段「duel」

一個 Product 兩個 World：Match（伺服器權威、不回滾）與 Client（伺服器權威＋官方回滾，多登記畫面用的 `HitFlash`）。S5-1 一致性指紋兩端相同、畫面資料不影響、判定登記影響；S5-2 預測漏了伺服器的一擊、digest 不同；S5-3 第 1 步的權威值進入口、重跑 5 步、digest 相同、腳步聲只數第一次；S5-4 值和歷史相同不回滾；S5-5 預測的死亡被推翻、配發器範圍內的 id 回來、OnAdd 與 `Add<T>` observer 在全部放回後才跑；S5-6 P2P 佔位啟動錯誤；S5-7 單機不選就沒有；S5-8 同一份存檔讀兩次摘要相同。第一次寫時 S5-5 的玩家 id 在配發器範圍外、沒有驗到「範圍內的 id 復活」，改成 1001、1002 後 amend（未推）。

## 第 8 步：審查、敵對式檢查、§14.4 檢證之後的修正

檢查者都是 Fable 5.2／high：整條線的審查、權威突變清單的敵對式檢查（adv-a）、回滾＋Kernel 突變清單的敵對式檢查（adv-b）、§14.4 檢證（A1～A11）。報告：主 checkout `build/evidence/gyo2-authority-rollback/step8-checks/`（`review-line/REPORT.md`、`adv-a/REPORT.md`、`adv-b/REPORT.md`、`verify/REPORT.md`；臨時突變的目錄 `adv-tools/`、`adv-a/batch*.json`）。

改了什麼（審查的代號）：

- A1：判定 4 命中的舊註解改寫（Kernel、`world_time`）。
- A2：回滾重跑的每一步包一段 trace（`rollback`／`resimulate`，宣告 10.4）。
- A4：`StepSchedule()` 與 `RollbackOptions::frameSchedule` 的預設跟著時間 Module；選了官方策略卻沒有步的 schedule 是 RB-E10（新碼）。
- A6：拿掉權威份沒有的 `MarkComponent<T>`。
- B1～B3：權威不寫 `Kernel::Detail`；「重跑時跑不跑」逐個掛載看條件（回滾公開 `RunsWhileResimulating` 等，名字的 `skipped` 表拿掉）；同一型別的可回滾與 Derived 跨種類裁決。
- Z8 接手時鬆掉的面補回：判定的輸入登記成 Derived 也是 AU-E7；元件的輸入也查（檢證 A4）。
- D1～D4：死碼（`PayloadBytesAt`、`InputHistoryBase::Capacity`、`StartupError::step`）、時間型別的 `reserved`、沒用的測試輔助、測試名裡的區與 Shared。
- E6～E8：回滾的策略改持 `shared_ptr`；`NoAuthority` 註解；`ModuleBuffer` 與準備函式跑完就放掉（O-2）。
- C1～C8：契約頁改名 [`authority-sources.zh-Hant.md`](../architecture/authority-sources.zh-Hant.md)（條文未改）、DI-A 2 的守衛照實寫、突變的 `guards` 指到存在的節、`determinism.md`、`system-contract.zh-Hant.md`、world-time 的 ctest、本頁的機器標示。
- 宣告 9.3 漏的案：`BuildNewFirst` 失敗時舊 World 的回滾照常。

補的測試與突變（side chat 的分類：只補會讓功能出錯、寫在契約或 DI 裡的，以及自己紀錄寫了「接手」卻沒人接的）：

| 清單 | 第 7 步末 | 加 | 退 | 現在 | 內容 |
|---|---|---|---|---|---|
| `authority_mutations.json` | 75 | 40 | 0 | 115 | 掛載逐個問（1）、Z8 的元件與 Derived（2）；指紋「該不變」7（另一張 schedule、前面的同步點、一般資料的 hook／observer／`InitialValue`、observer 的先後、`OneEndOnly` 的 observer）、「該變」6（hook 清空輪上限、`Runs` 的 schedule、寫的清單、hook 的順序、observer 的存取、`InitialValue` 的請求）；檢查 7（巢狀 schedule、沒有步時的 hook、AU-E8 的範圍、名字與型別的標記、兩個掛載、observer 的 Derived）；digest 的 Resource 1；G-5 4（去重、hook、observer、一般 `InitialValue`）；`OnRegister` 3；測試已有、清單沒有的 5（a10、c05、d10、e10、b29）；漏斗走訪 3（檢證 V-E1～3）；a09 1 |
| `rollback_mutations.json` | 47 | 28 | 1 | 74 | 退 `rb-skip-not-recorded`（名字的記錄拿掉，B2）；跨種類裁決 1、trace 段 1、預設與 RB-E10 3；R-8 的邊界 13（C 早於第一筆、只告知一次、C 時比較、預測沒有元件、登記的比較函式、權威值寫進歷史、三條修剪路與兩個條件）、R-2 的歷史 5（欄的修剪、沒變的窗口起點、Resource 被取代與不在、輸入容量的邊界）、R-4 2、R-1 的 Correct 1、跨 schedule 訊息的 observer 2、每個掛載都加條件 1、放棄的通知 1、權威的移除 1。`rb-alive-not-restored` 的 expect 收窄成 Kernel 斷言的文字（§14.4：只締，舊 expect 是案名，任何失敗都算） |
| Kernel `mutations.json` | 656 | 15 | 0 | 671 | 雜湊 5（pair 的 second、string 的長度、double、符號擴展、`error.world`）、摘要的訊息 2（內容、沒有雜湊函式）、寫者名單的每個掛載 1、`Restore` 與 `ResourceStamp` 7（不叫 `Alive`、亂序與重複、Resource 相等不寫、不在時移除、stamp 的值與不在、OnAdd 看得到後面的欄）。B9（細胞不排序）對合法輸入等價，試過後不加；B7、B8 在突變下由 Kernel 的 `RestoreSet` 斷言停下，expect 用斷言的文字 |

這輪新加的突變全部 killed（各自跑：權威 33＋3＋1＋前面幾批的 4、回滾 24＋5、Kernel 15）；紀錄在 `step8-checks/au-contract-mutants.log`、`rb-contract-mutants.log`、`ks-contract-mutants.log`。第一次跑時有 5 個沒按預期，都是測試的寫法（前面的同步點沒造出來、兩個摘要的 `Flag` 本來就不同、登記表的宣告順序依元件）或 expect 的形（B7、B8），改測試或 expect 後再跑。

不補的（一行一個理由）：

- 防誤用（照「誤用不加守衛」）：權威 e07、e13、e14、e17（四個 `GYO_ASSERT`）、e12（空的策略）、e11（只有 `Ingress`／`Runs`／`OneEndOnly` 卻沒選策略的 AU-E2）；回滾 A1～A4（同一來源兩筆 options 不同、種類不同的 RB-E1）、A8、A9（不存在的型別或 schedule 的 RB-E2、RB-E4）、A44（`GYO_ASSERT`）；Kernel B6、B13（closure 裡改 membership、closure 裡 `Restore`）、B22（`GYO_ASSERT`）。
- 逐欄細項（照「不做逐欄細項」）：指紋的各欄 b03～b05、b07～b13、b18～b20、b22、b33、b41～b48、b47；digest 的 c03；資料集的 d01～d04；G-5 的 d11、d12；登記表雜湊 e05、回滾登記表雜湊 A5；被 `RemoveSystem` 拿掉的 System（a01、a02、b30：建置期的事，結果在啟動時就看得到）；Kernel B12（還原後換 stamp）、B18（hook 被標 observer）。
- 低價值：版本常數 b01、b02、d05、d14（`rs-b6-cfp-version-1` 同類）；訊息措辭 e16、A47、d16、c04、d15（只換錯誤碼的名字）；預設值 A53、e01；回滾 A6（沒有流程替作者代為登記步的 schedule）；a16（AU-E4 先報）；`rs-j17-generated-by`、`rs-j17-zone-b-listed`（多列一個讀者只是多掃一個檔）。
- 等價或只影響成本：b42～b45、e09、d06、A13、A30、A32、A51、B16。

## 第 8 步：從頭完整跑的突變（ecae9977，主 checkout `build/evidence/gyo2-authority-rollback/full-mutation-ecae9977/`）

- 回滾 74/74、權威 115/115、時間 95/95 killed。
- 編排層 237/242。本分支造成的 3 個已處理：`g8-replace-prepare-from-old-registration` 退役（E8 之後 World 的 slot 沒有準備函式，這個誤點寫不出來，README 第 7 條）；`or-insert-reruns-finish`、`or-finish-before-plans` 的 replace 跟上第 3 步給 `WorldFinish` 加的 buffer 參數（再跑 2/2 killed）。`gyo2` 上本來就這樣的 2 個列為後續：`or-failure-no-product`（被前面的補值蓋掉、等價）、`or-runner-logs-back`（`Product.cpp` 沒有含入 `Log.hpp`，建置失敗）。
- Kernel 659/671。`gyo2` 上本來就 stale 的 7 個照舊（列為後續）；`ks-k8-skips-executor-files` 的 replace 用到 `gyo2` 上已不存在的 `executor_file`（同一族，列為後續）。本分支造成的 4 個已處理：`ks-s5-no-allotter-interrupts`（第 2 步把拒絕搬進 `RefuseImmediate`，replace 改經 `Fail`）、`rs-iv-fb5-sort-tiebreak-dropped`（expect 的 C 指紋那半隨第 6 步拿掉）、`obs-fingerprint-no-observers`（第 6 步改了訊息、去掉「zone B」）——再跑 3/3 killed；`rs-iv-fd10-inserts-not-sorted` 退役（它守的「錯誤點名的請求」隨區規則拿掉，`deferredInserts` 的順序只餵權威的漏斗走訪，那裡和順序無關）。
- `authority_scan` 18/18 killed。

## 第 8 步：審查 A3 的處置（使用者選 (a2)）

- 決定：使用者 2026-11-22「同意轉給任務卡」，經主會話轉（主會話記進 HANDOFF）。內容：`Restore` 照給的內容放回，通知照跑、不補 `InitialValue`；在 Kernel 裡把「通知」和「補資料」分開，不在第 5a 步加「跳過 Restore」的特例；預測死亡的復活一併處理。
- 做法：`Detail::Derive`（`InitialValues`／`Nothing`）由套用的呼叫者給；`RestoreFrom` 給 `Nothing`，其他（一般的立即變更、同步點、hook 回合）給 `InitialValues`。第 5a 步只看這個值，不看記錄的來源。
- 測試：回滾兩案（R-2 等價、復活的實體）、Kernel 一案；先紅後綠，紀錄 `step8-checks/a2-restore-initial-value-red.txt`、`…-green.txt`。突變 `ks-restore-derives-initial-values`、`rb-restore-derives-initial-values`，都 killed。ctest 161/161。
- 文件：Kernel DI-KS 12 加一段（草稿）；回滾 R-2 寫明不必當前提；合併前核准頁第 8 節照 Architecture Delta 寫。
- 從頭完整跑的突變用的是 ecae9977（這個修正之前）；這次改到的範圍另外跑了相關的突變（D184）。

## 第 8 步：審查 A5 的處置（使用者選 (c)）

- 決定：使用者 2026-11-22，經主會話轉（原話與比過的選項在合併前核准頁第 8 節）。判定的輸入和它的紀錄綁在一起，不加 AU-E10；容量用檔位，也可直接寫數字。名字：使用者的方法是「要改就讓同一組風格統一」（經主會話轉）；一度在工作樹改成 `RecordedInput`（形容詞＋名詞，和同組不一致），沒有提交、已改回。**判斷：維持 `rollback.Input`**——同組是「Module.種類」的單一名詞（`Component`、`Resource`、`Derived`、`Input`），命名規範第 3 條要 boundary 輸入帶 `Input`，而且和權威的 `.input` 是同一型別的兩個身分、用同一個詞；同風格的候選 `Replayed<T>` 描述的是回滾怎麼處理它而不是它是什麼，又把同一個型別拆成兩個詞，不比 `Input` 好。「它是按 tick 的紀錄與寫回、不是回滾狀態」寫在入口的註解與回滾頁第 2 節。
- 做法：權威對 Resource 的 `.input` 代為登記 `rollback.Input<T>()`（`DataOps::recordInput`，拿掉特意跳過輸入的那一行；`Self`／`NoRollback` 不代為登記）。`InputHistoryLength`（`StrategyDepth` 0＝策略的深度、`Short` 20、`Medium` 32、`Long` 64）；`RollbackStrategy::DefaultInputLength()`（預設 0，官方策略回 `depth`）；準備 World 時解析長度；RB-E9 不把預設長度當成不足。
- 測試：先紅後綠（`step8-checks/q2-input-history-red.txt`、`…-green.txt`）：權威的代為登記、沒選策略時沒有紀錄、`Self`；scenario 第 5 段拿掉作者那行；回滾的檔位、數字、預設與 RB-E9；深度邊界的回滾案改用預設長度（深度 6 時剛好寫回 6 步）。`[au-z6-input-no-history]` 改用 `Self`。突變 4 個，都 killed。ctest 161/161。

## 合併前核准

- 2026-11-22，使用者原話「核准」（[合併前核准頁](../architecture/target/v2/authority-rollback-merge-approval.zh-Hant.md)第 2～8 節）。各頁的草稿標記改成核准；契約頁 DI 1 與 Replacement Criteria 的改寫建議沒有被提到，維持原文。

## 經過與教訓

- 第 4 步動工前曾把分支強推（使用者沒同意），使用者在 `~/.claude/settings.json` 禁了 `--force` 類與 `git rebase`。之後跟上 `gyo2` 一律 fetch＋merge＋一般 push。
- 第 4 步的 Linux 量測：交付包第一次在容器裡建置時沒有網路、CMake 抓不到 yyjson；改成把三個 FetchContent 的原始碼（照 repo 釘的 commit）打進包、`FETCHCONTENT_FULLY_DISCONNECTED`。腳本原本把建置失敗吞成 0，改成先記 rc。
- 第 5 步的 commit cf6947d6 的訊息寫「検査 9 件」，實際是 8 件（已推，不改）。
- 第 6 步改測試時，一次誤用 `git checkout` 把 `InitialValueTests.cpp` 還原到 HEAD；它的修改全是腳本做的，重跑後恢復，最終以 ctest 確認。
- 突變的 stale 是被「每步只跑相關突變」漏掉的：find 在改動範圍、但當步沒挑出來的那幾個。第 8 步的完整跑會再對一次。

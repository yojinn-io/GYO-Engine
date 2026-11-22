# 2026-11-22 GYO2：第 5 批開頭的兩條守衛（第 0 步）

批次代號：`v8-arch-gyo2-exchange` 第 0 步。分支 `claude/gyo2-exchange-guards`（從 `gyo2` 45f13757，#266 之後）。任務卡「多個 World、交換點」（`gyo2-exchange`），主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high；敵對式檢查者 Fable 5.2／high（唯讀）。

## 起因

第 4 批宣告 [9.5](../architecture/target/v2/authority-rollback-declaration.zh-Hant.md) 把「本批新的」綁實作的守衛延到第 5 批開頭；#265 補了回滾 R-3 的連結集與 O4 的 `..`，剩下兩條（[#265 dev_log](2026_11_22_gyo2_batch4_followups.zh-Hant.md)「後續」）：

1. Kernel「拿掉的名字不回來」：第 4 批判定 4 的一次性掃描變成常設。
2. 權威 Module 的 `gyo_expect_links`。

`naming_guard` 加 `engine/modules/authority`、`engine/modules/rollback` 那一條不在本步（命名收尾卡 `gyo2-naming-cleanup` 在搬 `naming_guard`）。`world_time` 與回滾不經連結的 include 已由使用者定「不加守衛、交設計審查」（`v8-arch-gyo2-include-bypass-to-review`），不做。

## 1. Kernel：K9 加「拿掉的名字」清單

- `tests/common/kernel/kernel_guard.py` 的 K9（DI-KS 15 的守衛，已有「搬到編排層的名字」`K9_MOVED` 與「搬到時間 Module 的時鐘」`K9_CLOCK`）加第三份 `K9_REMOVED`。照 `K9_CLOCK` 的先例，K9 讀原文（含註解），所以樣式寫成識別字的形狀：小寫的說明文字（「the kernel keeps no zone」）照樣准許；大寫的識別字（`Zone…`、`Rollback::`、`"Z1"`）在註解裡也算。
- 起點是判定 4 的 29 條。改寫與取捨：

| 判定 4 的樣式 | K9_REMOVED | 理由 |
|---|---|---|
| `\b(Zone\|WritePolicy\|Rollback)::`、`enum class (Zone\|WritePolicy\|Role\|Rollback)`、`RequireExplicitZones`、`emitterZone`、`ZoneName`、`ZoneCSourcesOptions`、`ExportZoneCSources` | `\b\w*Zone\w*`、`\b[Ww]ritePolicy\b`、`\bRollback::`、`\benum\s+class\s+(?:Role\|Rollback)\b` | 含 `Zone` 的識別字一條包住（f25837b0 還有 `ZoneOf`、`ZoneAccess`、`CheckZoneAccess`、`observerZone`、`ApplyDefaultZones`，判定 4 逐個列不全） |
| `\bzone\b` | `\bzone[A-Z]\w*`、`\.zone\b` | 小寫單字會擋說明文字；舊 Kernel 用的是 `zoneAccess`、`info.zone`。剩下沒接住的只有區域變數 `switch (zone)`（同函式的 `Zone` 型別會中） |
| `\bexternal\b` | `\bbool\s+external\b`、`\.external\b` | 同上；日後有點名的 Feature 要在 Kernel 用 `external` 這個詞時，同一個變更拿掉這兩條 |
| `\bingress(Stage)?\b` | `\bingress\w*` | 敵對式檢查 A1：初稿只留 `ingressStage`，漏了 System 旗標 `bool ingress`、`options.ingress`（Kernel 認得權威的核心，Z3）。現在的 Kernel 任何形式的 `ingress` 都是 0 次 |
| `rollbackEnabled`、`notInRollback`、`rollbackSafe`、`rollbackActive`、`Role::Input`、`Role::State`、`Rollback::Derived` | `\brollback(?:Enabled\|Safe\|Active)\b`、`\bnotInRollback\b`、`\bRole::(?:Input\|State)\b`、`\bRollback::` | 照搬（`Rollback::` 包住 `Rollback::Derived`） |
| `DigestSet`、`MarkDigestSet`、`digestMarks`、`CFingerprint`、`CFingerprintOf`、`kCFingerprintVersion` | `\b\w*DigestSet\b`、`\bdigestMarks\b`、`\bk?CFingerprint\w*` | 照搬。digest 本身是權威 Module 的（DI-A 12），只列 Kernel 舊的標記 |
| `parallelGuardTest`、`FunnelWrites`、`"Z[0-9]"` | `\bparallelGuardTest\b`、`\bFunnel\w*`、`"Z[0-9]"` | 照搬（`Funnel\w*` 另包住 `FunnelNote`）。另加舊 Kernel 的 `CheckExclusiveWriters`（任務卡點名的「寫者數量」） |
| `\bDE(2\|9\|11\|15\|16)\b`、`\bXE1\b` | 不列 | 敵對式檢查 D1：舊的啟動錯誤碼回到 `StartupCode` 已由「DI-KS 15: the StartupCode values …」（`RegistryTests.cpp`）的精確比對與 `ToString` 的 switch 擋住；K9 多抓的只有註解裡提到舊碼，那正是說明文字。重複的守衛不留（使用者「別再堆屎山了」） |

- 正例：K9_REMOVED 的每一條在 f25837b0（拿掉之前）的 `engine/ecs/kernel` 都命中，在 45f13757 全部 0 命中（Python 掃描，腳本未保存）。
- 自測：`test_k9_every_removed_name_is_reported`（每條樣式一列，README 第 4 點「清單的每一項由自測確認」）、`test_k9_removed_comments_stay_allowed`（小寫說明文字、`TypeRole`、`PlanFingerprint`、`MessageWriterInfo` 這些近鄰不中；檢查 B2 換成真正的近鄰）。
- 自測的乾淨樹 `CLEAN_HEADER` 有一行 `enum class Zone { C, B };`（拿來當普通的列舉）：現在 `Zone` 是禁名，改成 `enum class Phase`。乾淨樹的其餘內容不變，它守的 K2～K8 不受影響。
- DI-KS 15 的守衛欄加一句指向 K9 的兩份清單；權威 DI-A 2 原本寫「由第 4 批宣告 9.5 的『拿掉的名字不回來』管」，改成指向 K9。

## 2. 權威 Module 的連結集

- `tests/common/authority/CMakeLists.txt` 檔尾：`cmake_language(DEFER … CALL gyo_expect_links gyo_authority GYO::Base GYO::Kernel GYO::Orchestration GYO::WorldTime GYO::Rollback)`，和 `gyo_rollback`、`gyo_world_time` 同一個寫法。
- 守的條文：DI-A 9「不依賴狀態機」的連結那一半。比條文嚴：多連任何 target 都擋（網路也擋，對 DI-A 1 的「不傳輸」）；正當地加依賴時，改登記列與那一行。DI-A 9 的守衛欄照這樣寫，並指出反方向（回滾、時間不依賴權威）由 `gyo_rollback`（R-3）、`gyo_world_time`（WT-4）的連結集與登記檔的順序守（檢查 C1）。
- 不加 DEFER 的突變：這個呼叫的 DEFER 和 `os-g19`、`ti-x5`、`rb-r3-authority-from-later-directory` 是同一個寫法，沒觀察到漏失之前不加（README 第 4 點第 3 項；檢查 C3 更正了初稿「DEFER 是函式自己的性質」的說法）。

## 3. 突變（README 第 4、8 點）

| 突變 | 守的錯 | 結果 |
|---|---|---|
| `ks-k9-removed-dropped` | K9 不再報 K9_REMOVED（清單一個突變）。規則整條關掉是 `ks-k9-dropped`、實樹陽性對照是 `ks-real-k9-moved-out-name`、fail closed 是 `ks-k-root-empty-passes`，都是 K9 共用 | killed |
| `au-links-more` | 實樹陽性對照：登記檔 authority 列 depends 多一個 `json`（排在前面，登記檔的順序檢查放行），只有新的連結集擋 | killed |

- 本步加 2 個（改了守衛欄的條文：DI-KS 15、DI-A 2、DI-A 9，上限參考 15）。各自只捕自己的錯（寫在 `what`），都有 `guards`。
- `ks-k9-clock-dropped` 的 find／replace 跟上新的那一行（守的性質不變，README「事後報告」）。
- 反向確認：拿掉 `gyo_authority` 那一行 → `au-links-more` 存活（`reverse-au-links-more.log`）。

## 敵對式檢查

- 檢查者：Fable 5.2／high，唯讀（作者 Opus 5.5）。在暫存複本裡對 K9_REMOVED 逐項改壞 46 種，每種跑自測；取出 f25837b0 的 Kernel 逐行比對判定 4 的 29 條；三個 K9 突變實跑。探測未保存。
- 必修 1、提案 3、輕微 5。全部處理：

| # | 指出 | 處理 |
|---|---|---|
| A1 必修 | 漏了 `ingress` 旗標 | `\bingress\w*`，自測加兩列 |
| A2 | 只剩區域變數 `zone` 沒接住 | 接受（同函式的型別會中）；記在上表 |
| A3 | 「註解准許」只對小寫成立 | docstring 寫明 |
| A4 | `external` 是通用字 | 記在上表：日後有 Feature 要用時拿掉 |
| B1 提案 | `DE(9\|11\|15)` 沒有自測 | 隨 D1 拿掉 |
| B2 | 「註解准許」的測試內容是惰性的 | 換成 `TypeRole`、`TypeRole::Component`、`PlanFingerprint` |
| C1 提案 | DI-A 9 守衛欄只寫一個方向 | 補反方向的守衛 |
| C2 | 「與網路，DI-A 7」引錯條 | 改成「比條文嚴」並引 DI-A 1 |
| C3 | `au-links-more` 的 what 說 DEFER 是函式的性質 | 改寫理由 |
| D1 提案 | `DE…`、`XE1` 是重複的守衛 | 拿掉 |

## 驗證

（本機 macOS Intel，AppleClang，preset `test`；`run_mutations.py` 加 `-B`、先清 `__pycache__`）

- 初稿：K9 的四個突變 4/4 killed、`au-links-more` 1/1 killed（`run-step0-kernel.log`、`run-step0-authority.log`）；全量 ctest 161/161（`ctest-step0.log`）。
- 檢查後（`ingress`、拿掉 `DE…`／`XE1`、測試的近鄰、文字修正）：自測 93/93；K9 的四個突變 4/4 killed（`run-step0-kernel-after-check.log`）；反向確認 `au-links-more` 0/1（存活，`reverse-au-links-more.log`），復原後 1/1 killed（`run-step0-authority-after-check.log`）；重新 configure、建置後全量 ctest 161/161（`ctest-step0-after-check.log`）。
- L1：PR 上跑。

## §14.4：Design Invariants 的守衛判定

都只收緊，沒有緩める面；DI 的守衛測試照 §14.4 仍要使用者核准（合併前）。

| | 1 守的性質 | 2 緩める面 | 3 締める面 | 4 | 5 根據與證據 |
|---|---|---|---|---|---|
| K9 的 K9_REMOVED（新） | DI-KS 15：Kernel 不認得區、權威、回滾策略 | 無（新清單；K9 原有兩份不變） | `engine/ecs/kernel` 出現 K9_REMOVED 的識別字就失敗（以前通過） | — | `kernel.zh-Hant.md` DI-KS 15；`run-step0-kernel-after-check.log` |
| 乾淨樹的 `Zone` → `Phase` | 自測的乾淨樹不報任何規則 | 無：乾淨樹的其餘內容不變，它原本守的 K2～K8 的放行案照舊 | — | — | `test_kernel_guard.py` |
| `gyo_authority` 的連結集（新） | DI-A 9：權威不依賴狀態機（連結那一半） | 無（新判定） | `gyo_authority` 多連任何 target 就 configure 失敗（以前通過） | — | `authority.zh-Hant.md` DI-A 9；`run-step0-authority-after-check.log`、`reverse-au-links-more.log` |

## 證據

主 checkout 的 `build/evidence/gyo2-exchange/`（使用者的 Intel Mac，git 不追蹤）：`configure.log`、`build.log`、`run-step0-kernel.log`、`run-step0-authority.log`、`ctest-step0.log`、`run-step0-kernel-after-check.log`、`reverse-au-links-more.log`、`run-step0-authority-after-check.log`、`configure-after.log`、`build-after.log`、`ctest-step0-after-check.log`。敵對式檢查的探測未保存。

# 2026-11-22 GYO2：權威 Module 的突變 `au-z6-skipped-counted` 存活的原因與修正

批次代號：`v8-arch-gyo2-au-z6-survivor`。分支 `claude/gyo2-au-z6-survivor-327c3b`（併入 `gyo2` 879350d4，#272 合併後）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high。

## 起因

交換點卡（`v8-arch-gyo2-exchange`，#270）合併前在 0bca189d 跑完整突變，`authority_mutations.json` 裡只有 `au-z6-skipped-counted` 存活（主 checkout `build/evidence/gyo2-exchange/full-mutation-0bca189d/authority_mutations.log` 第 21 行）。交換點卡比對過第 5 批與 gyo2 65d6c131 在權威、回滾、Kernel 的程式與測試沒有差異，判定不是第 5 批造成的，轉給主會話。使用者說「開卡查」。

突變的內容：Z6（AU-E6）的 System 迴圈把 `Rollback::RunsWhileResimulating(system, resimulated)` 換成 `MountedIn(system, resimulated)`，也就是「重跑時不跑」的 System（入口）也算進 Z6。預期失敗：`AU-E8*` 的 `[au-ingress-not-skipped] the official strategy`（官方策略的入口寫判定的輸入 `Command`，會被 AU-E6 報）。

## 假設 → 查證 → 對比 → 推斷 → 總結

### 1. 先確認真的存活

在 879350d4 上建置 `gyo_authority_tests`，只跑這一個突變（另跑同 case 的 `au-de15-unchecked`、`au-official-ingress-not-added` 當對照）：`au-z6-skipped-counted: survived`，另兩個 killed（`rerun-head-879350d4.log`）。runner 套得上 find（沒有 `find not found`），不是環境或 find 失效。

手動套上突變、印出官方策略那一例的結果：`OFFICIAL=built`。也就是突變後 AU-E6 仍然不報入口 `ReadCommand` 寫的 `Command`。

### 2. 假設：`Command` 被另一條豁免拿掉了

AU-E6 的 `judged(key)` 除了訊息與 tick，還排除 `inputsKept`（`rollback.inputs` 裡有歷史的輸入）。61d6f72c（`feat(authority): 判定の入力とその記録を結び付ける（審査 A5、使用者の選択 (c)）`）讓伺服器權威替 Resource 的判定輸入代為呼叫 `rollback.Input<T>()`（`RegisterContext::RegisterInRollback` 的 `recordInput`；`Self`／`NoRollback` 不代登記）。從這個 commit 起，測試裡的 `authority.Resource<Command>("Command", {.input = true})` 自動有了輸入歷史，`Command` 進了 `inputsKept`，AU-E6 不看它。入口不管算不算進 Z6，都不會因為 `Command` 被報，所以這一例測不出「重跑時不跑」的豁免。

### 3. 查證：在歷史 commit 上重跑

| commit | 內容 | 結果 | 證據 |
|---|---|---|---|
| 5263c865（61d6f72c 的父） | 輸入不代登記歷史 | killed | `rerun-5263c865.log` |
| 61d6f72c | 代登記 `rollback.Input<T>()` | survived | `rerun-61d6f72c.log` |
| 879350d4（目前的 gyo2） | — | survived | `rerun-head-879350d4.log` |

（突變從 1855d16a 加入；兩個 commit 是同一條線上的父子，只差 61d6f72c 一個變更。）

### 4. 對比：分類

- (a) 突變過時？不是。`MountedIn` 與 `RunsWhileResimulating` 不等價：前者只問掛在不在可回滾的 schedule，後者再扣掉帶「重跑時不跑」條件的掛載。豁免本身也還活著：寫**沒有保留歷史**的判定輸入的入口（Component 的輸入——`rollback.Input<T>` 只收 Resource；或選 `Self`／`NoRollback` 而作者沒自己登記歷史的 Resource 輸入）只能靠這條豁免不被 Z6 報。文件也還寫著它：`docs/architecture/target/v2/authority.zh-Hant.md` 第 100 行「Z6 不看『重跑時不跑』的 System」，合併前核准頁 A4、第 5 列。
- (b) 測試失去檢出力：是。判定沒變，是 61d6f72c 改了測試資料的意思（`Command` 多了一條出路），這一例不再走到「只有豁免能讓它通過」的情況。
- 61d6f72c 當時的突變檢查只跑了它新加的 4 個（commit 訊息「ミュータント 4 個（killed）」），這個突變的存活要等完整重跑才看得到。

### 5. 總結

原因屬 (b)，從 61d6f72c 開始。修正是補案例，不改判定。

## 修正

- `tests/common/authority/ServerChecksTests.cpp` 的 `AU-E8 (D-E15)` 一例補一條 CHECK `[au-z6-skipped-entry]`：官方策略、步的 schedule、`authority.Component<Health>("Health", {.input = true})`、入口 `authority.Ingress<&WriteHealth>` 掛在 `FixedUpdate`，要 `built`。Component 的輸入沒有輸入歷史可登記，所以「重跑時不跑」是它不被 Z6 報的唯一出路。用 Component 而不用 `Self` 的 Resource：前者不靠作者「沒登記歷史」這個選擇，豁免是唯一的出路與型別本身綁在一起。
- 原有的兩條 CHECK（`[au-ingress-not-skipped] …`、`… the official strategy`）不動：後者仍是 `au-official-ingress-not-added`（官方策略不加入口時 AU-E4）的預期失敗，並守官方策略在 Resource 輸入的路上能建起來。
- `docs/architecture/target/v2/scripts/authority_mutations.json` 的 `au-z6-skipped-counted`：find／replace／cases／guards 不變；`expect` 改指新的 `[au-z6-skipped-entry]`；`what` 補一句為什麼用 Component 的輸入（Resource 的輸入從 61d6f72c 起另有歷史這條出路）。
- 沒有加新的突變：新 CHECK 補的是既有突變的預期失敗（`tests/common/mutation/README.md` 第 6 點：有捕到的 Test 就合到既有突變上）。
- AGENTS §14.4：沒有改既有判定（只加一條 CHECK、改突變的 expect 指向它）；緩める面：沒有。不涉及守衛判定的放寬。

## 相關突變的結果

只跑和這一例有關的（`cases` 含 `AU-E8*` 的全部 5 個）：

| 突變 | 結果 |
|---|---|
| `au-z6-skipped-counted` | killed（預期失敗 `[au-z6-skipped-entry]`） |
| `au-de15-unchecked` | killed |
| `au-de15-every-schedule` | killed |
| `au-mounted-in-all` | killed |
| `au-official-ingress-not-added` | killed |

5/5 killed（`rerun-fix.log`）。`gyo_authority_tests` 47/47 通過。沒有跑整份清單的完整重跑（卡的指示；D184）。

## 證據

主 checkout `build/evidence/gyo2-au-z6-survivor/`（git 不追蹤）：

- `rerun-head-879350d4.log`：目前 gyo2 上單獨重跑（survived）。
- `rerun-5263c865.log`、`rerun-61d6f72c.log`：歷史 commit 上單獨重跑。
- `rerun-fix.log`：修正後的 5 個相關突變。
- 手動套突變印出的 `OFFICIAL=built` 只當出處，未保存。

## 設計審查

（待填：Fable 5.2／high，唯讀。）

# 2026-11-22 GYO2：權威 Module 的突變 `au-z6-skipped-counted` 存活的原因與修正

批次代號：`v8-arch-gyo2-au-z6-survivor`。分支 `claude/gyo2-au-z6-survivor-327c3b`（基於 `gyo2` 879350d4，#272 合併後）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high。

## 起因

交換點卡（`v8-arch-gyo2-exchange`，#270）合併前在 0bca189d 跑完整突變，`authority_mutations.json` 裡只有 `au-z6-skipped-counted` 存活（主 checkout `build/evidence/gyo2-exchange/full-mutation-0bca189d/authority_mutations.log` 第 21 行，全份 117/118）。交換點卡比對過第 5 批與 gyo2 65d6c131 在權威、回滾、Kernel 的程式與測試沒有差異，判定不是第 5 批造成的，轉給主會話。使用者說「開卡查」。

突變的內容：Z6（AU-E6）的 System 迴圈把 `Rollback::RunsWhileResimulating(system, resimulated)` 換成 `MountedIn(system, resimulated)`，也就是「重跑時不跑」的 System（入口）也算進 Z6。原本的預期失敗：`AU-E8*` 的 `[au-ingress-not-skipped] the official strategy`（官方策略的入口寫判定的輸入 `Command`，突變後會被 AU-E6 報）。

## 假設 → 查證 → 對比 → 推斷 → 總結

### 1. 先確認真的存活

在 879350d4 上建置 `gyo_authority_tests`，只跑這一個突變（另跑同 case 的 `au-de15-unchecked`、`au-official-ingress-not-added` 當對照）：`au-z6-skipped-counted: survived`，另兩個 killed（`rerun-head-879350d4.log`）。runner 套得上 find（find 在檔內唯一），不是環境或 find 失效。

手動套上突變、印出官方策略那一例的結果：`built`。也就是突變後 AU-E6 仍然不報入口 `ReadCommand` 寫的 `Command`。

### 2. 假設：`Command` 被另一條豁免拿掉了

AU-E6 的 `judged(key)` 除了訊息與 tick，還排除 `inputsKept`（`rollback.inputs` 裡有歷史的輸入）。61d6f72c（`feat(authority): 判定の入力とその記録を結び付ける（審査 A5、使用者の選択 (c)）`）讓伺服器權威替 Resource 的判定輸入代為呼叫 `rollback.Input<T>()`（`RegisterContext::RegisterInRollback` 的 `recordInput`；`Self`／`NoRollback` 不代登記）。從這個 commit 起，測試裡的 `authority.Resource<Command>("Command", {.input = true})` 自動有了輸入歷史，`Command` 進了 `inputsKept`，AU-E6 不看它。入口不管算不算進 Z6，都不會因為 `Command` 被報，所以這一例測不出「重跑時不跑」的豁免。

### 3. 查證：在歷史 commit 上重跑

| commit | 內容 | 結果 | 證據 |
|---|---|---|---|
| 5263c865（61d6f72c 的父） | 輸入不代登記歷史 | killed | `rerun-5263c865.log` |
| 61d6f72c | 代登記 `rollback.Input<T>()` | survived | `rerun-61d6f72c.log` |
| 879350d4（當時的 gyo2） | — | survived | `rerun-head-879350d4.log` |

突變從 1855d16a 加入；5263c865 與 61d6f72c 是父子，只差 61d6f72c 一個變更。

### 4. 對比：分類

- (a) 突變過時？不是。`MountedIn` 與 `RunsWhileResimulating` 不等價：前者只問掛在不在可回滾的 schedule，後者再扣掉帶「重跑時不跑」條件的掛載。豁免本身也還在用：文件寫著它（`docs/architecture/target/v2/authority.zh-Hant.md` 第 100 行「Z6 不看『重跑時不跑』的 System」，合併前核准頁 A4、第 5 列）。
- (b) 突變**指定的那一例**失去檢出力：是。判定沒變，是 61d6f72c 改了測試資料的意思（`Command` 多了一條出路）。
- 測試檔整體並沒有失守：同一個檔裡 fadaeb55（第 8 步，早於 61d6f72c）加的 `[au-z6-mount-added-after-skip] every mount skipped`（`AU-E6 and AU-E8 ask each mount…` 一例：`Health` 選 NoRollback、不是輸入，`WriteHealth` 唯一的掛載帶「重跑時不跑」，要 `built`）也會抓這個突變，只是不在它的 `cases`（`AU-E8*`）內，而 `au-z6-skip-by-system` 的 expect 用 `(?!every)` 刻意排除了它。設計審查指出這點（靜態推導），實跑確認：套上突變後該例以 `AU-E6 System 'WriteHealth' … (Z6)` 失敗。
- 61d6f72c 當時只跑它新加的 4 個突變（commit 訊息「ミュータント 4 個（killed）」），沒動 `ServerAuthorityStrategy.cpp` 與 AU-E8 那一例，照 D184 本來就不會跑到這個突變；合併前的完整重跑抓到了。

### 5. 總結

原因屬 (b)，從 61d6f72c 開始：突變的 `cases`／`expect` 指的那一例不再走到「只有豁免能讓它通過」的情況。既有的另一條 CHECK 一直守著豁免，只是沒有接到這個突變上。

## 修正

照 `tests/common/mutation/README.md` 第 1、6 點（已有 Test 捕到同一個改變，就合到既有的上，不另立）：

- `docs/architecture/target/v2/scripts/authority_mutations.json` 的 `au-z6-skipped-counted`：find／replace／guards 不變；`cases` 改成 `authority checks: AU-E6 and AU-E8 ask each mount*`，`expect` 改成 `\[au\-z6\-mount\-added\-after\-skip\] every mount skipped`；`what` 補一句為什麼不再指 AU-E8 的官方策略一例。
- 測試不改。既有的兩條 AU-E8 CHECK 不動：`… the official strategy` 仍是 `au-official-ingress-not-added`（官方策略不加入口時 AU-E4）的預期失敗。
- 沒有加新的突變。

### 第一版（58c652e5）與改法

第一版在 AU-E8 (D-E15) 一例另加 CHECK `[au-z6-skipped-entry]`（官方策略、Component 判定輸入、入口寫它，要 `built`），突變的 expect 指它。設計審查後改成上面的做法，拿掉這條 CHECK，理由：

- 既有的 `every mount skipped` 不依賴輸入的語意；第一版靠「Component 的輸入沒有輸入歷史」這個現況，哪天元件輸入也有歷史，就會照同樣的方式再失去檢出力。
- 第一版會把「入口寫元件輸入、預測的 World、官方策略」定成必須 `built`。審查指出這個組合沒有輸入歷史、重跑時讀到最新值，也就是 61d6f72c 為 Resource 解掉的問題在元件上還在（見「範圍外」）；不該用測試把它固定下來。
- 第一版的 CHECK 是 Z6 的斷言，卻為了遷就 `cases` 放在 AU-E8 的例子裡。

### AGENTS §14.4

既有判定沒有改（測試檔與基準相同，只改突變指向的 case 與 expect）。原 expect 指的 CHECK 仍有 `au-official-ingress-not-added` 守著，沒有 CHECK 失去唯一的突變。緩める面：沒有。不涉及守衛判定的放寬。

## 相關突變的結果

只跑和改動有關的（新的 `cases` 一例上的突變、原 `cases` 一例上的突變）：

| 突變 | 結果 |
|---|---|
| `au-z6-skipped-counted` | killed（預期失敗 `[au-z6-mount-added-after-skip] every mount skipped`） |
| `au-z6-skip-by-system`（同一例，expect `(?!every)`） | killed |
| `au-de15-unchecked` | killed |
| `au-official-ingress-not-added` | killed |

4/4 killed（`rerun-fix2.log`）。`gyo_authority_tests` 47/47 通過。第一版時另跑過 `cases` 含 `AU-E8*` 的 5 個，5/5 killed（`rerun-fix.log`，那時測試多一條 CHECK）。沒有跑整份清單的完整重跑（卡的指示；D184）。

## 設計審查

Fable 5.2／high，唯讀（實作者 Opus 5.5）。審查的是第一版 58c652e5。

- 原因判定 (b) 成立、(a) 不成立；新 CHECK 只靠豁免才 built；改 expect 不加新突變符合 README；沒有漏報的緩める面；dev_log 沒有不實。範圍內「壞了也不報」的地方 0 處。
- 指出 1 處：既有 CHECK `every mount skipped` 也抓這個突變（上面第 4 節）。建議 A（把突變接到既有的）——採用。
- dev_log 的修正：補記既有 CHECK、填本節、證據補機器、「併入」改「基於」——都已改。
- 收緊 expect 到失敗理由的建議：改用既有 CHECK 後，其訊息本身帶 `AU-E6 System 'WriteHealth'`，沒有另外收緊（expect 照同清單其他突變的寫法只指標籤）。

## 範圍外（交主會話）

- 元件輸入沒有輸入歷史：入口寫 Component 的判定輸入時，預測的 World 重跑讀到的是最新值（61d6f72c 為 Resource 解掉的問題在元件上還在）。`authority.zh-Hant.md` 第 101 行寫元件輸入「照舊受 Z6 檢查」，但 Z6 只管重跑時會跑的寫者。要不要處理是機制問題，交使用者決定。

## 證據

使用者的 Intel Mac（x86_64），主 checkout `build/evidence/gyo2-au-z6-survivor/`（git 不追蹤）：

- `rerun-head-879350d4.log`：當時 gyo2 上單獨重跑（survived）。
- `rerun-5263c865.log`、`rerun-61d6f72c.log`：歷史 commit 上單獨重跑。
- `rerun-fix.log`：第一版的 5 個相關突變。
- `rerun-fix2.log`：最終修正的 4 個相關突變。
- `rerun-fix2-after-merge-eacab4d7.log`：merge 目前的 gyo2（eacab4d7，#270 合併後）後重跑同 4 個，4/4 killed；`gyo_authority_tests` 47/47。
- 手動套突變後印出的結果（官方策略一例 `built`、`every mount skipped` 以 AU-E6 失敗）只當出處，未保存。

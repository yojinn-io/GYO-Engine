# OS 子系統：盤點與收口提案

日期：2026-10-28。任務卡「盤點 OS 子系統並收口到支撐遊戲運行」。
**狀態：核准（D78）。** 使用者：「核准：決定 1 選 (a)、決定 2 選 (a)、決定 3 選 (a)、決定 4 選 (a)，不做對抗式檢查。開始 os-freeze-wait 與 os-closeout-docs，推送前先問我。」
os-freeze-wait 與 os-closeout-docs 已在本分支完成，並做了本機驗證（[dev_log](../../../dev_logs/2026_10_28_engine_os_closeout.zh-Hant.md)）。os-handoff-v71 提前改成雙向，已完成（消費端 D219：發行前不需要 OS 這邊做任何事）。推送後四平台 L1 通過（run 40646224836）。os-land-master 在消費端發行（PR #96）後開工，以 PR #104 併 master（2026-10-29，四平台 L1 run 40832108929，[dev_log](../../../dev_logs/2026_10_29_engine_os_land_master.zh-Hant.md)）。
以下第 0～8 節是核准前的提案原文。盤點時只讀：沒有改任何分支、沒有推送、沒有啟動量測；撤回的代價用 `git merge-tree` 在物件庫裡試算，沒有動任何 ref。

核心：讓 Engine 支撐遊戲跑起來（v7.1 發行，接著 v8 的玩法）。驗收以 CI 為主，實機只做 CI 做不到的事。

## 0. 先更正的前提（任務卡與現況不同之處）

1. **Windows 實機第 1 次不只跑到 W3。** Windows 機器在 14:20 推送了 W4～W7 的結果（`844e92b1`、`7306a142`，遠端 `origin/claude/os-windows-run` 現在是 `3ab8f09c`，D76 也已推送）：
   - W4 晚醒基準（只記錄）
   - W5 執行緒後端的 L2：**通過**（`role started` 行逐字相同）
   - W6 UDP 錯誤量測（只記錄）
   - W7 Windows 專用突變：**12／12 killed**
   - 未跑的只剩 W8（MinGW 的確認），以及續跑表的 W3-samples、W3-dll、host-state、W3-aa、worker-10x。
   - W3 仍不通過，判定不變。
2. **Linux 的背景量測已經不在跑。** 容器 `gyo-linux-run` 不存在（`docker ps -a` 沒有），也沒有 `docker exec` 程序；OS 分支上沒有任何 Linux 結果。`75418486` 記的是「X0 建置中，還沒有結果」。
3. **發行線的整合樹，自 10-26 起沒有跑過四平台 L1。**
   - 發行線最後一次手動派發的 L1 是 run 40348670097（`4a4a0ef7`）。
   - 之後併入的有：os-path-utf8-sweep 與 time-threads-invariants（`16377127`）、os-utf8-codepage（`7e92069d`）、master #98／#99（`c9c4b62f`），以及 v7.1 的 `d7e0af31`（只改測試訊息）。
   - draft PR 的推送 L1 是 skipped，例如 run 40640735972。
   - 這些批次各自在 OS 分支上的 L1 都通過（見第 2 節）。
4. **發行線上已有 D218**（`d7ecf047`）：在收口方案核准前，發行線不再併入新的 OS 批次，也不為 OS 安排實機量測。本提案以這個為前提。
5. **PR #96 的標題只寫「OS サブシステム A1～A3 と windows-long-path P1～P5」。** 實際上 D59 之後，另外 10 批 OS 工作也以 5 次 `--no-ff` 合併進來了（`32ce9b6d`、`f87b77d0`、`2aebc143`、`16377127`、`7e92069d`）。這就是範圍漂移的具體位置。

## 1. 總覽

功能代號（本檔用）：

- **F-deep**：玩家把遊戲放在深資料夾也能啟動（2026-10-16 的事故，路徑 271 字元開不了 shader）。v7.1 發行要的。
- **F-nonascii**：玩家把遊戲放在含非 ASCII 字元的資料夾也能啟動、寫出 log。D43 與 P4 屬於這一項（v7.1 的 windows-long-path 已經涵蓋讀取這一側）。

記號：✓ 留下、❄ 凍結、⏸ 等你決定、✗ 取消。

```text
OS 子系統（D40～D77）
├── 長路徑那一片（發行線）                         ✓ 留下
│   ├── os-plan-docs        ｜計畫文件               ✓（收口時改寫狀態）
│   ├── os-skeleton (A1)    ｜engine/os 與守衛       ✓ F-deep 的落點
│   ├── io-contracts (A2)   ｜io 路徑契約、修 ParseUri ✓ F-deep
│   ├── os-files-read (A3)  ｜唯讀檔案原語           ✓ F-deep
│   └── P1～P5（v7.1）      ｜長路徑修正與實機驗收   ✓ F-deep、F-nonascii
├── 已在發行線的其他 10 批                          ⏸ 決定 1（建議：全部留下）
│   ├── 無害：os-guard-wide、os-process-info、time-threads-invariants
│   ├── 支撐 F-deep／F-nonascii：os-files-write、os-trace-writers、os-path-utf8-sweep
│   └── 沒有功能支撐的 ★：os-console-shutdown、io-native-paths、os-tool-files、os-utf8-codepage
├── 只在 OS 分支                                    ⏸ 決定 2、3
│   ├── os-wake-record      ｜晚醒量測程式           ✓ 留在 OS 分支
│   ├── os-threads-backend  ｜執行緒命名與優先度搬移 ✓ 留在 OS 分支（L1、macOS、Windows L2 通過）
│   └── os-wait-backend     ｜等待原語搬移           ❄ W3 不通過，從 OS 分支撤出
├── 未開始
│   ├── os-net-errors       ｜socket 錯誤分類        ❄ 不開始；W6 的數據交 v7.1
│   └── os-close            ｜收尾                   ✗ 改成小的收口批
└── 實機
    ├── os-windows-run      ｜Windows 自動項目       第 1 次結案；續跑與第 2 次取消
    └── os-linux-run        ｜Linux（Docker）        ✗ 取消（沒有結果）
```

## 2. 逐批明細

「性質」：搬移＝行為不變；★＝事前宣告的行為變更。「L1」是紀錄裡的四平台 run，已用 `gh run view --json jobs` 確認 `L1 /` 開頭的四個 job 都是 success。

| 批次 | 白話 | 程式碼在哪 | L1 | 性質 | 支撐的功能 | 未結 | 分類 |
|---|---|---|---|---|---|---|---|
| os-plan-docs | 寫計畫交核准 | 發行線 | — | 文件 | — | 狀態要改寫 | ✓ |
| os-skeleton (A1) | 建 engine/os、GYO::Os、守衛 | 發行線 `f3a67704` | 40173730672 | 結構 | F-deep（P1 寫在這裡） | — | ✓ |
| io-contracts (A2) | io 的路徑契約（UTF-8、file://）、修 ParseUri | 發行線 `f3a67704` | 40173730672 | 契約＋bug 修正（POSIX 絕對路徑被變成相對） | F-deep（P3 經 file:// 讀 asset） | — | ✓ |
| os-files-read (A3) | 唯讀檔案原語＋NativeFileSystem | 發行線 `f3a67704` | 40173730672 | 新原語 | F-deep | — | ✓ |
| P1～P5（v7.1） | 長路徑行為、錯誤記錄、asset 接線、UTF-8 呼叫端、驗收 | 發行線 | 實機 P5-6（10-27）通過 | ★（有功能） | F-deep、F-nonascii（讀） | — | ✓（v7.1 的） |
| os-guard-wide | 守衛擴到產品、工具與驗收程式 | 發行線 `32ce9b6d` | 40194493434 | 只加守衛 | OS 差異只落在 OS 層 | — | ✓ |
| os-process-info | 執行檔路徑、CPU 時間、本地時間搬進 engine/os | 發行線 `32ce9b6d` | 40194493434 | 搬移（時間戳逐 byte 相同） | 無害的搬移 | L3（只記錄）未做 | ✓；撤掉 L3 |
| os-console-shutdown | 命令列、主控台輸出、結束要求搬進 engine/os | 發行線 `32ce9b6d` | 40205158822（`os.console_close` 在 CI 是 Skipped） | ★ Windows 伺服器端程序可優雅結束、主控台 UTF-8、關閉時最多等 4 秒 | 沒有宣告的功能（D206 只記錄） | L3 大半已做（D201，10-27，conhost：Ctrl+C 與關閉視窗都 OK，log 最後一行是 end 紀錄；Ctrl+Break 與 `os.console_close` 使用者決定不補）；突變 os-c6 存活、os-c4 未跑；開放決策 25、26 | ⏸ 決定 1 |
| os-files-write | 寫入原語；arena 的讀取與 log 的寫入改走 engine/io | 發行線 `32ce9b6d` | 40213916104 | 搬移＋長路徑寫入；D53 的路徑差異 5 項 | F-deep（深資料夾裡也寫得出 log、讀得到 arenas.json） | L3 未做 | ✓；撤掉 L3 |
| io-native-paths | file:// 規則對齊 OS（`..` 交給 OS、UNC、POSIX 的 `//`） | 發行線 `32ce9b6d` | 40295007179 | ★ 路徑契約重新設計（§14.4 判定改變，D55） | 沒有（UNC 與 `..` 不是 v7.1 的功能） | `allowAbsolutePath` 的字面站點 | ⏸ 決定 1 |
| os-tool-files | 編輯器的原子存檔、路徑比較、系統字型目錄 | 發行線 `32ce9b6d` | 40324361964 | ★ `CompareStringOrdinal`、字型候選清單 | 只影響編輯器，不在遊戲的執行路徑上 | — | ⏸ 決定 1 |
| os-trace-writers | trace 寫檔改走 engine/os | 發行線 `f87b77d0` | 40339587478 | ★ 只能附加的 handle、64 KiB 分段；長路徑可寫 | F-deep（log／trace 在深資料夾） | — | ✓ |
| os-path-utf8-sweep | 剩下的窄字串路徑改 UTF-8，PathFromUtf8 移到 engine/os | 發行線 `16377127` | 40433158383 | 行為修正（ACP 以外的字元）；Client 的非 UTF-8 參數以結束碼 2 拒絕 | F-nonascii（log、trace、Match 啟動） | — | ✓ |
| time-threads-invariants | time、threads 的 Design Invariants | 發行線 `16377127` | 40433158383 | 文件＋便宜的守衛 | 之後搬移時的檢查依據 | 無守衛的條目 | ✓ |
| os-utf8-codepage | 每個 Windows 執行檔以 manifest 宣告 UTF-8 | 發行線 `7e92069d` | 40456874710；W7 實機突變 3 條 killed | ★ 整個程序的 ANSI code page | 使用者原則 D64（程序不由環境決定）；沒有宣告的功能 | MinGW 未確認（W8）；發行線整合樹沒跑過 L1 | ⏸ 決定 1 |
| os-wake-record | 晚醒量測程式合回、基準 | **只在 OS 分支** `f6997194` | 40463273387 | 只加測試程式 | 量測工具；W4 的無視窗數據對 v7.1 的 F5／F7 有用 | Linux、arm64 的基準 | ✓ 留在 OS 分支 |
| os-threads-backend | 執行緒命名與優先度的 OS 呼叫搬進 engine/os | **只在 OS 分支** `2ec5905d` | 40484797877；L2：macOS Intel 與 Windows（W5）逐字相同 | 搬移＋★ Windows 非 ASCII 名稱修正 | 無害的搬移 | Linux 的 L2 | ✓ 留在 OS 分支（決定 3） |
| os-wait-backend | 等待原語的 OS 後端搬進 engine/os | **只在 OS 分支** `4041cf42`、`756073aa` | 40510515336（第 2 次） | 搬移＋★ 錯誤細節文字 | 沒有 | **W3 不通過**（window deadline P50 超出 0.0032 ms） | ❄ 決定 2 |
| os-net-errors | socket 錯誤碼的中立分類 | 未開始 | — | 計畫是等價搬移 | 沒有 | W6 量到 Windows 的差異 | ❄ 不開始 |
| os-close | 收尾：allowlist 清空、文件同步、完整突變 | 未開始 | — | 文件＋突變 | — | — | ✗ 改成 os-closeout-docs |
| os-windows-run | Windows 實機上不需要人操作的項目 | 分支 `claude/os-windows-run` | — | 量測 | — | W8 與續跑 5 項 | 第 1 次結案 |
| os-linux-run | Linux 的 L2、基準、專用突變（Docker） | OS 分支（只有文件） | — | 量測 | — | 沒有結果 | ✗ |

補充：

- **v7.1 自己的程式碼只用到 A1～A3 的 API。** 發行線上 v7.1 寫的 commit，只呼叫 `PathToUtf8`、`PathFromUtf8`、`MakeAbsolute`、`IsAbsolutePath`。後面各批對產品程式碼的改動，都是 OS 批次自己做的（D48：「產品呼叫端由架構批次一起改」）。
- **Windows 實機在發行線上跑的是 `3b612cf2`（10-27，D201）**：P5-6 的長路徑啟動、Windows 專用突變（OS 的 31 條：killed 23、survived 5、未跑 3；存活的 os-c6、pu-x5～pu-x7 是那台 ACP＝65001 判定不了，os-w10 之後在 W7 已 killed）、`-s` 的確認、quick-close 10 次、主控台（Ctrl+C、關閉視窗通過）。這棵樹已經包含 os-path-utf8-sweep 之前的所有 OS 批次，只差 os-utf8-codepage。
- **計時核心是不是純搬移（用 high 判讀，沒有升到 xhigh）**：
  - 兩批的 diff 形狀都是「從 `RoleThread.cpp`／`Waiter.cpp` 刪掉，在 engine/os 新增」，另外加測試。
  - os-threads-backend 另含一個 ★：Windows 的非 ASCII 執行緒名稱原本有缺陷，這批修了。
  - os-wait-backend 另含一個 ★：錯誤細節的文字。
  - D74 的分析（另一個會話做的，我沒有逐行重驗）說，Windows 的 `Block` 在兩側逐行相同，差別只有一層跨程式庫呼叫與一次間接存取。
  - 沒有升到 xhigh 的理由：不論結論如何，「os-wait-backend 沒有功能支撐，先凍結」這個建議都不會變。xhigh 判讀只在之後要撿回這一批時才影響決定。

## 3. 分類

### 留下

- 長路徑那一片：A1～A3、P1～P5。
- 已在發行線、L1 通過、沒有不通過判定的 10 批，依決定 1。其中 3 批無害，3 批支撐 F-deep／F-nonascii。
- 只在 OS 分支的 os-wake-record、os-threads-backend：留在 OS 分支，等 v7.1 發行後再併 master（決定 3）。

### 凍結

1. **os-wait-backend：從 OS 分支撤出，回到 os-threads-backend 定案時的狀態（`4ffc7c65`）。**
   - 4ffc7c65 之後動過程式碼的，只有 `4041cf42` 與 `756073aa`，已用 `git log 4ffc7c65..os-subsystem-75418486 -- <檔案>` 逐檔確認。條文（`os.md`、`time.md`、`docs/architecture.md`）也只有 `7e6592d1`、`756073aa`、`6316d657` 動過。
   - 做法：把下面的檔案從 `4ffc7c65` 取回（等同把這四個 commit 撤回，但不碰紀錄檔）：
     - `engine/CMakeLists.txt`、`engine/os/CMakeLists.txt`、`engine/os/include/engine/os/WakeSignal.hpp`（刪除）、`engine/os/src/wait/WakeSignal.cpp`（刪除）、`engine/time/CMakeLists.txt`、`engine/time/src/Waiter.cpp`
     - `tests/common/os/CMakeLists.txt`、`tests/common/os/OsHeaderHygieneTests.cpp`、`tests/common/os/WakeSignalTests.cpp`（刪除）、`tests/common/os/os_boundary_allowlist.txt`、`tests/common/time/CMakeLists.txt`
     - `docs/architecture/plans/os-subsystem/scripts/mutations.json`、`docs/architecture/plans/time-threads-trace/scripts/mutations.json`
     - `docs/architecture/os.md`、`docs/architecture/time.md`、`docs/architecture.md`（D72 定案的 time.md Invariant 8、10 與 os.md Invariant 6 的例外跟著撤回，這是 Design Invariants 的變更，要你同意）
     - `tests/common/time/wake_compare.py`（`beb14cf7`，ABAB 腳本，只服務這一批）：建議一起刪，要用時可以從 `beb14cf7` 取回。
   - 試算：用 `git revert` 撤 `756073aa` 時，只有紀錄檔 HANDOFF.md 起衝突。上面「逐檔取回」的做法不會碰到它。
   - 依賴邊：`gyo_time` 不再連 GYO::Os（`engine/time/CMakeLists.txt` 回到搬移前），`Waiter.cpp` 回到 allowlist。
   - 驗收：OS 分支派發一次四平台 L1（推送前先問你）。
   - 發行線不受影響：這一批從來沒有進過發行線。
2. **os-net-errors：不開始。**
   - socket 錯誤分類留在產品（`ClientConnection.cpp:910` 的 `TransientSocketError`）。
   - D215 的「動 `ClientConnection.cpp` 前通知 v7.1」跟著失效。
   - W6 的數據交給 v7.1，見第 5 節。
3. **os-close：取消。** 改成本提案的小收口批 os-closeout-docs（第 6 節）。全量突變在合併 master 的那條 PR 前跑一次（D184 的作法）。
4. **實機項目：**
   - os-windows-run 第 1 次照現狀結案。續跑表的 W3-samples、W3-dll、host-state、W3-aa 只在撿回 os-wait-backend 時才有意義，取消。第 2 次（os-net-errors、os-close 之後）取消。
   - os-linux-run 取消。
   - 掛在已併入批次上、還沒做的 L3（os-process-info、os-files-write）從 OS 這條線撤掉。os-console-shutdown 的主控台檢查已在 10-27 的 D201 做過（Ctrl+C、關閉視窗通過）。

### 需要量測的（預設延後）

| 項目 | 會改變什麼決定 | 工時（agent） | 機器 | 你要花的時間 | CI 做得到嗎 |
|---|---|---|---|---|---|
| M-wait-win：os-wait-backend 在 Windows 另行宣告判定（D74 的 C：Windows A/A → 新判定 → 新 A/B），Linux 在 Docker 補 | 撿不撿回 os-wait-backend | 6～10 h（含 §14.4 的 1～5、檢出力檢查、另一位審查者） | Windows 約 3 h；Docker 約 2.5 h（虛擬機，限界要照寫） | 約 1～1.5 h（核准宣告、§14.4、開 Windows 會話） | 做不到：亞毫秒的晚醒，共用 runner 量不準；L1 也規定不設時間上界 |
| M-threads-linux：os-threads-backend 的 Linux L2（X5） | 幾乎不會（L1 與另外兩個平台的 L2 都通過） | 約 1 h | Docker 約 1.5～2 h（多半是建置） | 0 | 部分：L1 跑 RoleThreadTests 與名稱讀回測試；A／B 兩次建置的逐字比較做不到（CI 一次只建一個 commit）。~~優先度有沒有讀回測試，我沒有逐行確認~~（更正，os-land-master 時）有：`tests/common/os/ThreadTests.cpp` 的「applies the priority hints as the platform reads them back」與經 RoleThread 讀回的案例，Linux 讀 nice，run 40484797877 的 linux-x64 通過 |
| M-wake-base：晚醒基準的 Linux、macOS arm64 | 不會（只記錄） | 約 0.5 h | Docker 約 1.5 h（含建置） | 0 | 做不到 |
| ~~M-console-l3：Windows 主控台用 Ctrl+C、Ctrl+Break、關閉視窗結束 Match~~ | （更正，D78 執行時）10-27 的 D201 已在 conhost 實機做了 Ctrl+C 與關閉視窗，都通過；Ctrl+Break 與 `os.console_close` 使用者決定不補。不需要再量 | — | — | — | 做不到：CI 沒有主控台 |
| M-mingw：W8 的 MinGW 確認（manifest 以 `.rc` 嵌入） | 只有 v7.1 會用 MinGW 建置發行時才會 | 約 0.5 h | Windows 約 1 h | 0 | 做不到：CI 沒有 MinGW 的 job（D66） |
| worker-10x（v7.1 D217） | v7.1 的 worker 失敗是實作還是時序 | v7.1 會話 | Windows 約 1 h | 依 v7.1 | 目前做不到：CI 的 test preset 只跑 `cpu|shader`。改成由 CI 跑是 v7.1 的決定 |

建議：只有 worker-10x 可能影響 v7.1 發行，交給 v7.1 決定（D78 執行時更正：主控台的實機檢查在 10-27 已做過）。其餘從 OS 這條線拿掉，不排進任何版本。

## 4. 等你決定

### 決定 1：已在發行線、沒有功能支撐的 ★（4 批）要怎麼處理

- **機制**：
  - os-console-shutdown、io-native-paths、os-tool-files、os-utf8-codepage 已經在發行線上，L1 都通過，沒有不通過的判定。
  - 但它們都改了行為，卻寫不出支撐哪個功能，這正是「方向偏了」的部分。
  - 它們已經「可用」，所以撤回本身也是一個沒有功能支撐的行為變更（把行為改回去）。
- **選項**：
  - **(a) 全部留下（建議）**。
    - 在 OS 的 README 把這 4 批標成「已出貨、未經功能驗收的 ★」，OS 這條線不再為它們安排 L2／L3。
    - 之後在 v7.1／v8 的試玩中觀測到問題，當成產品的缺陷，由產品會話處理。
    - 代價：文件約 0.5 h。
    - 風險：UNC 的對應、編輯器的路徑比較、整個程序的 UTF-8，會在沒有功能驗收的情況下出貨；Windows 的優雅結束在 conhost 實機驗過 Ctrl+C 與關閉視窗（D201）。它們都有 L1；除了 os-utf8-codepage，也都在 Windows 實機（`3b612cf2`）上跑過 v7.1 的驗收。
  - **(b) 只撤 os-utf8-codepage**。
    - 它是最後一次合併（`7e92069d`）。在發行線頂端試算 `revert -m 1`，文字上沒有衝突，撤回後就回到 Windows 實機驗過的 `3b612cf2` 的 OS 內容。
    - 代價：約 2～3 h。內容包括：撤回 commit、把 D212 退場的 `p4-manifest-ansi-path` 突變放回、派發一次 L1，以及 v7.1 文件（D212）的更正。
    - 這會撤銷你在 D64 定的原則「程序不應該由環境參數來決定」，Windows 執行檔重新跟著玩家機器的系統語系走。
  - **(c) 撤回到 PR #96 標題的範圍**（只剩 A1～A3＋P1～P5）。
    - 要撤 5 次合併。從 `16377127` 起就在文件與突變目錄上起衝突。
    - §14.4 核准過的判定改變（D55）會跟著倒回；OS 批次改過的產品呼叫端（例如 Client 的 `--log`、`--movement-trace`）也要倒回。
    - P5-6 的實機驗收是在含這些批次的樹上跑的，要重做。
    - 代價：agent 約 6～10 h，加 Windows 實機重驗（你約 1～2 h），加 L1，加 v7.1 的協調。不建議。
  - **(d) 重新檢視計畫前提**。例如先替這 4 批各自補寫支撐的功能（例如把 F-nonascii 擴成「任何系統語系」），寫得出來的留下、寫不出來的再選 (b) 或 (c)。代價：約 1～2 h 的討論與文件。
- **證據**：第 2 節的表；試算指令是 `git merge-tree --write-tree --merge-base=7e92069d origin/claude/pvp-v71-release 7e92069d^1`（exit 0）。

### 決定 2：os-wait-backend 怎麼凍結

- **機制**：
  - W3 的判定是事前凍結的（D71），不通過就不重跑、不放寬（§14.1）。
  - D74 你選了 (B)：先跑 Windows 的 A/A 診斷，再在撤回與另行宣告之間決定。
  - 現在 D218 不安排 OS 的實機量測，(B) 跑不了，所以要在這裡改決定。
- **選項**：
  - **(a) 從 OS 分支撤出（建議）**，做法見第 3 節「凍結」第 1 項。之後有壓力時，從 `4041cf42`、`756073aa` 撿回並重新宣告。代價約 1～1.5 h，加一次 L1。
  - **(b) 不撤，只標「判定不通過，凍結」**，留在 OS 分支的歷史上。代價為 0，但 os-threads-backend 和 os-wake-record 疊在同一條分支上，之後併 master 時會把不通過的批次一起帶進去，到時還是得撤。
  - **(c) 走 M-wait-win**：另行宣告後重量（成本見第 3 節的表）。沒有功能支撐，不建議。
  - **(d) 重新檢視前提**：計時這類 CI 量不準的搬移，是不是只靠 L1 結構測試加守衛就收？這等於放寬 D71 的驗收前提（§14.4 的放寬面），要另外寫 1～5 交你核准。
- **證據**：Windows dev_log 的 W3 節（`origin/claude/os-windows-run:docs/dev_logs/2026_10_28_engine_os_windows_run.zh-Hant.md`）、HANDOFF 的 D74。

### 決定 3：os-threads-backend 與 os-wake-record 什麼時候進 master

- **機制**：
  - 兩批都只在 OS 分支上，L1 通過；os-threads-backend 的 L2 在 macOS Intel 與 Windows 都逐字相同。
  - D69 事前宣告了 Linux 的 L2。不跑就等於改了事前宣告的驗收，但還沒看過 Linux 的結果，所以不違反 §14.1。
- **選項**：
  - **(a) v7.1 發行後，OS 分支以一條 PR 併 master（建議）**。驗收照新規則用 CI 加守衛，取消 Linux 的 L2（X5）。
    - 守住的性質是：Linux 上的命名與優先度呼叫和搬移前相同。名稱有 L1 的讀回測試守著；優先度要不要補一個讀回測試，由那條 PR 決定。（更正，os-land-master 時：優先度也已有 L1 的讀回測試，見上方 M-threads-linux 一列，不必補。）
  - **(b) 現在就併進發行線**：和 D218 衝突，也不支撐 v7.1，不建議。
  - **(c) 也凍結，留在 OS 分支**，等有壓力再併。代價為 0，但無害的搬移會一直放著，分支和 master 的差距會越來越大。
- **證據**：W5 的 `A-roles.txt`、`B-roles.txt`（Windows 機器的證據目錄）；run 40484797877。

### 決定 4：今後的規則寫在哪裡、範圍多大

見第 5 節。選項：
- **(a) 寫進 OS 計畫的 README「執行規則」與 os.md 的 Replacement Criteria（建議）**。
- **(b) 再加一條到 AGENTS.md**。這會讓規則適用到所有子系統，是比較大的變更。
- **(c) 重新檢視前提**：OS 計畫本身要不要結束，改成「OS 層」的常設 owner 文件（只有 os.md），不再有「計畫」。

### 要不要做對抗式檢查（ultracode 或換模型）

- 建議：**不做**。
- 理由：凍結的步驟已經用 git 試算，屬於機械性的證據。表裡的位置與 L1 也都是用指令查的。對抗式檢查能抓的，主要是「支撐的功能」這一欄的判斷，而這一欄本來就由你在決定 1 裡判斷。
- 如果要做：1 位不同模型的審查者（例如 Sonnet 5.5，high），只讀，對照 git 驗證第 2 節的表，約 30～45 min。

## 5. 今後的規則（提案）

1. **OS 層保留，作為 OS 差異唯一的落點。** 之後需要 OS 支撐時，直接落到這一層；沒有觀測到壓力就不擴大（AGENTS.md §2「observed pressure, not predicted pressure」）。
   - 層裡沒有壓力、也沒有功能支撐的部件，是凍結或延後的候選。
   - 層以外的 OS 程式碼（例如 `ClientConnection.cpp` 的 socket 錯誤分類、凍結後的 `Waiter.cpp` 的 `#if`），是之後要搬進來的對象，有壓力時才搬。
   - （使用者 2026-10-28，經架構健檢會話轉達。）
2. **OS 層的工作分兩類。**
   - **搬移**：行為不變，放在 OS 層的線。驗收是 CI L1 加守衛（allowlist 縮小、連結守衛、新舊等價的測試），不做實機 L2／L3，也不做 ★ 事前宣告。
   - **行為變更**：寫出支撐的 Feature（AGENTS.md §2 的推導順序），由產品版本排程與驗收，不放進 OS 層的線。程式碼的位置仍在 OS 層（位置由架構定，寫行為屬於修正）。
3. **拆批的判準。** 一批若需要實機 L2、★ 事前宣告，或要改既有測試的判定（§14.4），就不是純搬移，要拆出去，交給有功能的產品版本。搬移的途中發現需要 ★ 時，停下回報，不在同一批做。
4. **CI 量不準的搬移**（例如計時）：用 L1 的結構測試加守衛就收；精度只在有產品壓力（事故、玩法要求）時，由產品版本另排量測。
5. **OS 層的批次不併進產品的發行線**，除非那個版本的功能需要它（例如長路徑需要 A1～A3）。其餘照 OS 層的線，直接走 master。
6. **實機只做 CI 做不到的事**（D76）。實機項目要寫出「為什麼 CI 做不到」，以及「結果會改變哪個決定」；兩者都寫不出來就不排。

## 6. 核准後的執行（最小變更）

```text
os-closeout（收口）
├── os-freeze-wait   high   ｜從 OS 分支撤出 os-wait-backend（逐檔取回 4ffc7c65），L1 一次
├── os-closeout-docs medium ｜改寫 README／PLAN／HANDOFF 的狀態、寫今後的規則、dev_log；
│                            併入 os-windows-run（3ab8f09c）與兩個未推送的 commit（5edc5b23、75418486）
├── os-handoff-v71   medium ｜通知 v7.1 會話（下列五點）
└── os-land-master   medium ｜（v7.1 發行後）OS 分支以一條 PR 併 master：wake-record、threads-backend、收口文件；
                             PR 前在背景完整跑一次相關突變（D184）
```

- 估時：前三項 agent 約 3～4 h，加 L1 的等待。os-land-master 另計（約 1～2 h，加 L1）。
- 模型：繼承。os-freeze-wait 用 high，因為它動到 Design Invariants 的條文與建置圖；文件與通知用 medium。
- 推送與派發 L1 前都先問你。不 force push，不刪分支；備份分支 `os-subsystem-75418486`、`os-windows-run-3ab8f09c` 留著，由你決定去留。

要通知 v7.1 會話的五點（發行線與 PR #96 屬於它，本提案不改發行線）：

1. 決定 1 的結果。如果選 (b) 或 (c)，撤回由 v7.1 在發行線上做，或由 v7.1 同意後交給本會話。
2. 發行線的整合樹自 `4a4a0ef7` 起沒有四平台 L1。PR #96 合併前要手動派發一次（draft 的推送會讓 L1 skipped）。
3. OS 這邊不再要求 lan15 做任何事。D206 的主控台檢查、worker-10x（D217，現在沒有執行者），由 v7.1 決定留不留。
4. 產品的線索（不是 OS 這條線的工作）：
   - W4：沒有視窗的程序上，condition_variable 的晚醒 P50 是 5.28 ms，`sleep_until` 是 15.63 ms，Waiter 是 0.617 ms。這和 F5／F7 有關。
   - W6：Windows 在**未連線**的 UDP socket 上，也會因為送到關閉的埠而收到 `WSAECONNRESET`（每個被拒的 datagram 一次）。`SocketErrorActionFor` 在還沒進入遊玩時，把 connection_reset 判為 Fail（`ClientConnection.cpp:916-919`）。握手期在 Windows 上會不會因此失敗，屬於 K9 的範圍，請 v7.1 判斷。
5. os-net-errors 取消，`ClientConnection.cpp` 不會再被 OS 這條線改動（D215 的通知義務解除）。

## 7. Architecture Report（執行時）

- Architecture Delta：
  - 撤回 time.md Invariant 8、10 與 os.md Invariant 6 的例外（D72 定案的條文），要你同意。
  - 依賴邊 `gyo_time → GYO::Os` 移除。
  - `Waiter.cpp` 回到 OS 守衛的 allowlist（層外的 OS 程式碼多一處，記為之後的搬移對象）。
- 不新增依賴，不改 Ownership；發行線的 Data Contract 不變。
- 發現的 Code Smell：發行線的 PR 範圍與標題不一致（第 0 節第 5 項）。整合樹的 L1 有空窗（第 0 節第 3 項）。

## 8. 證據位置

- 計畫：`os-subsystem-75418486:docs/architecture/plans/os-subsystem/{README,PLAN,HANDOFF}.md`。
- Windows 實機：`origin/claude/os-windows-run:docs/dev_logs/2026_10_28_engine_os_windows_run.zh-Hant.md`，以及 `windows-run.md` 的「第 1 次續跑」。
- 發行線：`origin/claude/pvp-v71-release:docs/object_fps_pvp/plans/v7.1/HANDOFF.md` 的 D199～D218。
- L1：第 2 節的 run 號；發行線最後的派發是 run 40348670097。
- 架構健檢：`claude/checkup-architecture-steps:docs/checkup/architecture/2026_10_27_architecture.zh-Hant.md`。

# 文件健檢（2026-10-15）

基準：master `0f0bc91`，也就是 PR #74（v7 收尾）合併後。上一次文件整理是 PR #38（`05042fa`，2026-10-04），之後合併了 PR #39～#74，共 108 份 md 有變更。
範圍：全部 212 份追蹤中的 md，不含 `third_party/`。判斷標準與步驟見 [README](README.md)。

另有一個未合併的分支 `claude/pvp-v71-plan`（`c742836`）已修正兩處：v7 交接頁開頭的狀態，以及 v7 穩定基線的合併 commit。本報告不重複列出這兩處。

## 結論

- **機械檢查幾乎全部通過。** 1,641 個連結與圖片中，斷鏈、絕對路徑、壞錨點都是 0。四組多語文件的結構與反引號內容完全一致。大陸用語只剩「數據」，簡體字是 0。
- **主要問題是「狀態沒跟上」。** v6、v7 都已升格，但不少文件開頭還寫「未開始」「實作中」「待合併」。聯網架構文件還停在 v5／v6 的程序與執行緒結構。
- **Engine 與共通文件的問題集中在近期新增的模組。** Time、Threads、Net 三個模組，以及工具停用之後的連帶影響，有幾份文件還沒反映。
- PR 編號和合併 commit 都和 `git log --merges` 對得上，沒有寫錯的。

## 一、PvP 文件

### 高

| 位置 | 過時的內容 | 實際狀態 |
|---|---|---|
| `docs/object_fps_pvp/network-architecture.zh-Hant.md:7-8`、`68-140`、`688-716` | 開頭寫「pv6（v6 升格前是候選）……批次狀態見文末 v5 一節」。程序與執行緒圖仍是：主執行緒做輸入與固定步、Client 用 token bucket worker、IPC Host 等待 socket。修改入口表也是舊的。 | v6、v7 都已升格。v7 改變了下列結構，但文件沒有描述：Client 拆成三個角色、Match 的 Tick 用絕對期限等待、IpcHost 與 Client 連線改用 asio、Gateway 結果通道改為 30 Hz，之後再改為事件驅動轉送、本機射擊閘。現行 wire 仍是 pv6。 |
| `docs/object_fps_pvp/plans/v6/HANDOFF.md:3-4` | 「更新 2026-10-05……其餘批次都未開始；現行 wire 與玩法仍是 v5」 | v6 已於 2026-10-08 升格，v7 之後也升格了。 |
| `plans/v6/11-remote-pitch-aim.md:3` | 未開始 | 已合併（PR #61，`83440ef`） |
| `plans/v6/12-local-fire-gate.md:3` | 未開始 | 已合併（PR #60，`a9485bf`） |
| `plans/v6/13-hit-reaction.md:3` | 未開始 | 已合併（PR #62，`7886848`） |
| `plans/v7/08c-drop-report.md:3` | 實作中 | 已合併（PR #73） |

### 中

- **v7 進度頁**（`plans/v7/README.md`）：
  - `:4` 寫「P7 的 PR（#74）合併後 v7 結束」，應改為已合併（`0f0bc91`）。
  - `:91` 第 16 批那一列前面寫「L3 完成」，後面又寫「剩使用者操作的 L3」，同一格自相矛盾。
- **v7 交接頁的「現況」一節**（`plans/v7/HANDOFF.md`）：
  - `:48` 寫「其餘已決定（D56～D75）」，`:54` 的下一步還停在 PR #73，`:55` 寫「最後更新 2026-10-14」，都已過時。
  - `:14` 代號對照只到 D57，實際已編到 D85。
  - 開頭的狀態列由未合併的 v7.1 規劃分支處理，本報告不列。
- **v7 的其他批次文件開頭沒更新**：

  | 文件 | 開頭寫的 | 實際 |
  |---|---|---|
  | `09a-input-send-pinning.md:3` | 設計草案 | 已完成並合併 |
  | `09b-phase-tracking.md:3` | 規劃草案，待使用者確認 | 已完成並合併 |
  | `02-client-simulation-seam.md:3` | PR 在第 03a 批之後開 | 已合併（#70，`f3d176d`） |
  | `04-client-roles.md:3` | 待 PR 與 CI | 已合併（#71，`660310e`） |
  | `03-per-snapshot-phase.md:3` | 只寫「移到 P1b」 | 已隨 #71 合併，文件沒寫 |

- **pv6 協議文件**（`protocol-v6.zh-Hant.md`）：
  - `:185`（本機射擊閘的補償公式）、`:210`（俯仰映射範圍）、`:216`（受擊動畫優先順序）仍寫「待定，由第 12／11／13 批決定」，而這三批早已合併。
  - `:5` 寫「合併前，現行程式仍是 v5」，可以刪掉。
  - `:218` 把「死亡後的第一人稱武器」寫成第 05 批，但第 05 批沒有執行。
- **v6 交接頁的「延後項目」沒寫結局**（`plans/v6/HANDOFF.md:558-692`）：
  - 多數項目仍用 v6 開始前的程式描述「現況」，其中兩處引用的東西已經刪除：本機冷卻欄位 `localCooldownUntil`，以及輸入遷移時移除的 `HandleNativeEvent`。
  - 回合準備期已列入 v8，這裡卻沒有連結。
  - 相位估計器在 WAN 雜訊下的穩定性，在 v7.1 和 v8 都找不到。
  - 「未結事項」裡仍列著已經完成的項目。
- **v6 穩定基線的「交給 v7」**（`plans/v6/STABLE_BASELINE.md:70-79`）：
  - 前三項已在 v7 完成。
  - 對時和日誌兩項已移到 v7.1（D73、D76）。
  - 頁面沒有往 v7 穩定基線的入口。
- **v6 進度頁**（`plans/v6/README.md:94-97`）：結尾的「接下來可以開始第 11、12 批」停在第 10 批結束時的狀態。
- **v5 的入口**：
  - `plans/v5/README.md:68` 寫「v6：2026-10-04 已開始」，沒寫已升格。
  - `protocol-v5.zh-Hant.md:4-5` 仍寫三個角色共同使用 v5，沒註明現行 wire 是 pv6。

### 低

- **dev_log 的狀態行停在「待合併」或「待 CI」**：
  - v6 第 01 批（`:3`、`:98`），以及第 11、12、13、16、14a 批的 `:3`。
  - v7 第 02 批的 `:3`。
  - v6 第 02～04 批的日誌沒寫合併 commit。
- **v4 文件寫成可以照著執行的指令，但腳本已刪除**：`plans/v4/MANUAL_ACCEPTANCE.md:186,200` 的 `run_action_legal.py`、`plans/v4/HANDOFF.md:189` 的 `run_weapon_short.py`，兩者都在 v6 第 02a 批刪除。`protocol-v4.zh-Hant.md` 也沒有指向後續版本。
- **交接頁缺「代號 → 白話」對照表**：v4、v5、v6 的交接頁開頭都沒有。v6 交接頁仍常被引用，最需要補。
- **v8、v7.1 的計畫頁**：
  - `plans/v8/README.md:3` 的更新日期停在 2026-10-14，但 10-15 加了兩項候選。
  - `plans/v7/16-integration-acceptance.md:495` 說「HUD 來源 tick」是 v7.1 候選，但 v7.1 README 沒有列入。v7.1 README 由規劃中的分支處理。
- **v7 第 08、08b、08c、09、16 批沒有 dev_log。** v7 交接頁的未結事項裡已有「v7 完成後撰寫系列紀錄」，現在可以寫了。

## 二、Engine 與共通文件

### 中

- **`docs/architecture.md`**：
  - 目錄樹（`:9-39`）沒有 `time/`、`threads/`、`net/`。
  - 責任表（`:53-70`）沒有 `GYO::Net`；Net 由 PR #45 加入，負責 GYOP 24-byte 標頭的編解碼，依賴 Base。
  - `:121-127` 說 Engine 不依賴任何 wire protocol，但沒提到 Engine 現在擁有 GYOP 標頭的 C++ 版，而且和 Go 版用同一組 golden 向量。
- **`docs/tool_projects.md`**：
  - `:11-13` 的 CSV 範例把 `object_fps_preview` 寫成啟用，實際上已在 `engine/config/tools.csv` 停用（PR #48）。所以 `:24-25` 的範例指令現在會因為「not enabled」直接失敗。
  - `:30` 的遷移連結少了 `docs/architecture/plans/foundation-followups/inactive_products.md`。
- **`docs/creating_apps.md`**：
  - `:139-145` 的手動複製以 `apps/object_fps` 為來源，但它已停用而且無法編譯。
  - `:37` 的公開 target 清單少了 `GYO::Time`、`GYO::Threads`、`GYO::Net`。
- **README 三語**：
  - `:5` 的模組列舉少了 Base、Time、Threads、Net。
  - `:14` 寫 `tools/` 含本機遊戲 preview，沒說明 preview 已停用。
  - `:75` 說停用產品「在 Math／Result 統一後須先遷移」，但之後的 include 路徑統一、輸入層、Collision 改動也需要遷移。
- **`docs/object_fps/acceptance.{ja,zh-Hant}.md:11`**：狀態註記少了第三份遷移清單（foundation-followups）。`docs/object_fps_v2/*.md` 完全沒提到這個產品已停用。
- **`docs/dev_logs/2026_10_08_engine_tt1.zh-Hant.md:3`**：仍寫「尚未合併」，實際已在 PR #71（`660310e`）合併。

### 低

- **計畫 README 的日期與表格**：
  - `time-threads-trace` 的 README 與 HANDOFF 只寫合併 commit，沒寫 PR #71；README 的更新日期比內文舊。
  - `foundation-followups/README.md` 的更新日期比內文舊；第 1～6 列多了一個「）」；PR #58 沒有加連結。
  - `input-and-present/README.md` 開頭的缺口描述仍用現在式（「`Key` 只有 14 個鍵」），應改為立案時的描述；IP-2 那一列沒寫合併 commit（`db1815c`）。
- **`docs/architecture.md:61,64`**：Input 那一列沒寫依賴 Base；Input 與 PlatformSDL 兩列都沒寫輸入與呈現第一批加入的完整 scancode 與視窗互動事件。
- **`docs/rendering_architecture.{ja,zh-Hant}.md:158`**：可以補一句 SDL GPU「取得 swapchain 太慢」的診斷日誌（≥50 ms）。現有描述沒有錯。
- **`docs/architecture/math.md:5`**：遷移清單只連到 Math 那一份。

## 三、機械檢查

| 項目 | 結果 |
|---|---|
| 連結與圖片（1,641 個，含 36 個錨點） | 斷鏈 0、絕對路徑連結 0、壞錨點 0 |
| 反引號中的 repo 路徑（1,657 個） | 沒有標示為歷史、實際已不存在的 5 處，都在計畫與交接紀錄裡（見下）。其餘找不到的都是 file:line 寫法、include 路徑寫法、已標示為歷史的路徑、`build/target/`、本機日誌或佔位名稱。 |
| 繁中檔案的字形 | 日文字形 1 處：`plans/v6/HANDOFF.md:489` 寫了日文的「搖晃」（字形與假名）。日文中黑（U+30FB）2 處：`plans/v7/09b-phase-tracking.md:684,1473`。 |
| 大陸用語 | 只有「數據」16 處，集中在 v6 與 v7 的計畫和一份 dev_log。其餘詞是 0 處；「網絡」1 處、日文 3 處都是使用者原話或 commit 引用，刻意保留。 |
| 多語版本 | README 三語、`releasing`、`rendering_architecture`、`object_fps/acceptance` 都一致。 |

沒有標示為歷史、但已不存在的路徑：

- `docs/architecture/plans/result-unification/HANDOFF.md:193-194`：`PlayerController.cpp`、`EnemySystem.cpp`，已在 v6 第 08 批刪除。
- `docs/architecture/plans/foundation-followups/PLAN.md:258,263,341`、`HANDOFF.md:60`：include 路徑統一（FF-7）之前的標頭位置。
- `docs/architecture/plans/input-and-present/HANDOFF.md:105`：同上，`SdlPlatform.hpp` 的舊位置。
- `docs/architecture/plans/foundation-followups/HANDOFF.md:403`：`assets/object_fps_pvp/ui/screens.json`，已刪除。
- `docs/object_fps_pvp/plans/v4/HANDOFF.md:189`：`run_weapon_short.py`，已刪除。

## 已確認無問題

- PvP 文件與 dev_log 中所有「PR #N 對應 commit」的寫法都和 `git log --merges` 一致：v5 的 #2～#16、v6 的 #39～#65、v7 的 #69～#74。
- pv6 與程式一致：C++ 與 Go 的協議版本都是 6；proto 檔與 Go 生成目錄名稱正確；文件中寫的訊息與欄位都在程式裡。Gateway 宣告的建置平台確實只有 linux-x64、windows-x64。
- PvP 文件引用的 197 個程式路徑中，只有上面兩個 v4 腳本不存在。文件引用的 15 個 `object_fps_pvp.*` CTest 名稱都已登記。
- v7 穩定基線、第 16 批與 LAN 測試頁的狀態一致。v7 穩定基線列出的 v7.1 候選，在 v7.1 README 都找得到。
- 計畫的 PR 編號與合併 commit 都正確：foundation-followups 9 列、input-and-present、time-threads-trace、Math、Result。Engine 系列的 dev_log 除了上面的 tt1 以外都正確。
- `releasing.*` 已反映「只改文件的推送沿用 L1」，以及新建 tag 讀不到時會重讀。`ui_toolchain.md` 已加入 `item_step` 的有限性規則。gyo_gateway README 已不含產品名稱。
- `engine/config/projects.csv` 只有 object_fps_pvp 啟用，和 README 的說法一致。

# 事前宣告：hook 的紀錄在 schedule 跑完時套用（`v8-arch-gyo2-hook-flush`）

更新：2026-11-15（修訂 2）。Owner：架構（GYO2 重建會話開出的任務卡）。**狀態：草稿，待使用者核准。核准之前不改 `engine/` 與 `tests/`。**

經過：
- 使用者 2026-11-15 定了做法與批次的位置（`v8-arch-gyo2-hook-queue-flush-at-run-end`）。
- 修訂 0：寫之前先做了試做（不在本卡的分支、不提交），找出「哪些既有測試會變」（主 checkout `build/evidence/gyo2-hook-flush/spike/`）。送別模型檢查（Fable／high，唯讀，兼 §14.4 的驗證者）：blocker 4、should 11、nit 9（報告 `build/evidence/gyo2-hook-flush/declaration-check-r1-fable.md`，實驗 `check-r1-experiments/`）。
- 修訂 1：照報告改寫，把「放在哪裡」的建議從「執行器在計畫跑完後做」改成「最後一個同步點的一部分」。送第二輪檢查時，主會話轉來使用者在另一個分支會話已經定的決定，第二輪檢查中途停掉（沒有結果）。
- **修訂 2（這一份）**：照使用者的決定改寫。使用者定的與既有紀錄已涵蓋的，照寫、不再問（第 2 節）。要你核准的只剩條文的改寫與守衛測試的變更（第 3 節）。改寫後還沒送檢查（第 9 節）。

根據（HANDOFF 決定表，分支 `claude/gyo2-time-design`，還沒併進 `gyo2`）：`v8-arch-gyo2-hook-queue-flush-at-run-end`、`v8-arch-gyo2-hook-flush-not-in-plan-bump-versions`、`v8-arch-gyo2-hook-flush-limit-two-levels`、經過「hook 清空那一批的五題過完」；`v8-arch-gyo2-startup-once-by-kernel`、`v8-arch-gyo2-mechanism-not-conservative`、`v8-arch-gyo2-sync-order-keep`、`v8-arch-gyo2-err-is-bug-world-readable`；`docs/architecture/plans/kernel/HANDOFF.md:33` 的 `v8-arch-kernel-change-stamp`。規格 `docs/architecture/plans/kernel/spec.zh-Hant.md`；target `kernel.zh-Hant.md`、`world.zh-Hant.md`。

---

## 1. 這一批做什麼

### 1.1 用射擊遊戲講

子彈一出現，hook 幫它加上拖尾。

- 「hook」是登記好的函式：某個元件被加上（或移除）時，機制叫它。
- 機制只在**同步點**加元件。同步點在每個 stage 結束時。
- hook 自己也可以記「再加一個元件」。這筆紀錄現在要等**同一張 schedule 的下一個同步點**才套用。

問題出在「下一個同步點」不在這一次執行裡的時候：

| 情形 | 發生什麼 | 這一批 |
|---|---|---|
| 乙 | 子彈在固定步那張 schedule 的**最後一個**同步點出現。hook 記的「加拖尾」要等固定步那張**下一次被跑**。一幀跑 0 到數次固定步，所以每幀那張的 System 哪一幀看到拖尾不固定。 | 解 |
| 丙 | hook 在只跑一次的 schedule（Startup）的最後一個同步點記的，**永遠不套用**。K0 讓 Kernel 保證「Startup 只跑一次」之後，這一種一定發生。 | 解 |
| 甲 | 子彈由每幀那張生成，而跑固定步的 System 在同一張裡（K0 之後才會有）。hook 記的要等每幀那張的下一個同步點，也就是這一幀所有固定步之後。 | 不解，只講明（使用者已定） |

### 1.2 改成什麼

每次跑一張 schedule（`Product::Run`）：

1. 計畫的最後一個同步點照常套用完。
2. 執行器接著看這張 schedule 的 hook 佇列。空的就往下走（如果後面有清舊事件那一步，就做它）。
3. 不空：把佇列裡的紀錄完整套用一次。這叫「**一輪**」。
4. 這一輪又觸發的 hook 記下的紀錄，進同一個佇列，再套用一輪。
5. 佇列空了才往下走。輪數有上限（預設 8，可以設）；最後一輪套用完佇列還不空，就回報錯誤、World 中斷。

用例子：子彈在最後一個同步點出現 → hook 記「加拖尾」→ 第 1 輪加上拖尾 → 拖尾的 hook 如果又記了「加粒子」，第 2 輪加上 → 佇列空了，`Run` 回來。**hook 記的變更，永遠比觸發它的那一輪晚一輪生效。**

**需要它的 Feature**（AGENTS §2）：情形乙、丙（使用者點名）；K0 的「Startup 只跑一次」（沒有這一批，丙一定發生）；回滾（規格 H.5；我推的、計畫中的功能，gyo2 的 `engine/kernel` 還沒有 `Rewind`／`Restore`）——快照不含 hook 佇列（`spec.zh-Hant.md:902`），這一批之後每次 `Run` 結束時佇列一定是空的。

**為什麼是基盤**：每一個會記延後變更的 hook，都照「它的紀錄什麼時候套用」寫。

### 1.3 用到的詞

| 詞 | 意思 |
|---|---|
| 窗 | 兩個同步點之間。這段時間裡記下的延後變更，在窗結尾的同步點一起套用 |
| (b) 區、(c) 區 | 決定性區。(c) 區的資料要求兩端（例：伺服器與客戶端）逐位元相同，(b) 區不要求 |
| Z5 | 規則：(b) 區的程式不能刪掉帶 (c) 區元件的實體。記錄時查一次，套用前再查一次（「重查」） |
| stamp（記號） | 每一步執行時取的遞增號碼；元件被加上或改寫時記下當時的 stamp，System 用它判斷「上次執行之後有沒有變」 |
| `syncEpoch` | 每次套用延後變更後加一的計數；用來抓「跨過套用還在用的舊指標」 |
| 位置（`planPos`） | 一步在執行計畫裡的序號。hook 寫的事件用它判斷誰讀得到 |
| `recSeq` | 同一個佇列裡記錄的先後序號；同一格被寫兩次時，後記的勝 |
| 指紋 | 把登記的內容與執行計畫算成一串雜湊；兩端比對，不同就表示不是同一份程式。有三種：執行計畫的、C 的（只含 (c) 區）、Registry 的（全部） |

---

## 2. 行為：已定的與照既有紀錄推的（照寫，不再問）

### 2.1 使用者定的

| # | 內容 | 出處 |
|---|---|---|
| 1 | 每張 schedule 跑完時，把它的 hook 佇列套用到清空；套用時再觸發的 hook 記下的，也在同一次裡接著套用 | `hook-queue-flush-at-run-end` |
| 2 | 碰到上限：回報錯誤、World 中斷，當成程式的 bug；不靜默留到下一次 | 同上 |
| 3 | 不做「每個同步點把 hook 的連鎖當場做完」（Bevy 的做法） | 同上 |
| 4 | 獨立一批，排在 K0 前面；情形甲不解，只講明 | 同上 |
| 5 | **清空不是執行計畫裡的一步**，由執行器在最後一個同步點之後接著做，排在清舊事件（EventTrim）之前 | `hook-flush-not-in-plan-bump-versions` |
| 6 | **執行計畫指紋的版本號 1→2、C 指紋的版本號 3→4**；Registry 指紋的版本號不加（使用者：「至少能看到。」） | 同上 |
| 7 | **上限兩層**：預設 8；產品可以改預設（放進 `KernelSettings`，第一次執行前設，之後不能改）；每張 schedule 可以在宣告（`ScheduleOptions`）裡另外指定，不指定就用產品的預設；執行中不能改。最少 1、不設最大值、0 在啟動時擋下。每張 schedule 指定的值進執行計畫的指紋；產品的預設不進指紋。`KernelSettings` 仍然不給 System 讀（D-E18） | `hook-flush-limit-two-levels` |

### 2.2 既有紀錄已涵蓋的（「五題過完」那一則）

1. **「跑完」是每一次 `Run`**，包含之後 K0 巢狀跑的每一次。
2. **每一輪是一次完整的套用**：hook 的紀錄與一般的紀錄走同一段套用（`World.cpp:515-688` 的 `phase`），所以 Z5 的重查、刪除、移除、生成、插入、漏斗的補值、OnAdd、OnRemove、Resource 的操作每一輪都做；套用順序維持現狀（`sync-order-keep`）。每一輪是各自的一次套用：**跨輪不適用**「同一次套用裡先移除後加上」那一類的合併。
3. **每一輪取一個新的 stamp**（`v8-arch-kernel-change-stamp` 推出來的）。
4. **清空到一半出錯**（hook 回 Err、Z5、碰到上限、驗證抓到的）：World 中斷、剩下的紀錄丟掉、錯誤處理呼叫一次、這次 `Run` 回 Err；已經套用的那幾輪不退回；那張的 System 都已跑完、tick 已前進；清舊事件那一步不做。碰到上限的訊息列出最後一輪還在記東西的 hook。「錯誤處理只呼叫一次」在這條路上還沒人確認過——列成測試案例（第 6 節）。

### 2.3 兩件查證（`not-in-plan-bump-versions` 列為「沒確認的」）

- **Registry 指紋有沒有「兩邊比對」以外的用途？沒有。** 規格 C.2 第 5 點寫的用途是「診斷；兩端比對 FixedUpdate 的 Plan 是否相同（H-4）」（`spec.zh-Hant.md:388`）。程式裡呼叫它的只有測試與 `tests/common/kernel/FinishRecord.cpp:99`（印出來看）；`git grep` 找不到存檔、重播或相容性的用途。所以照使用者的決定不加它的版本號。附帶的事實：Registry 指紋把每張執行計畫指紋的**文字**（含版本號，`Fingerprint.cpp:309`）算進去，所以它的**值**會跟著變，只是版本號不變。
- **C 指紋有沒有包含執行計畫的版本？沒有。** C 指紋只雜湊「(c) 區的 System 步驟、同步點與清舊事件」的種類與名字（`Fingerprint.cpp:378-386`），不含執行計畫指紋。所以兩個版本號要分開加，和使用者定的一致。

### 2.4 從上面推出來的界線（寫出來是因為可以測）

- **中段不變**：計畫中間的同步點 i 觸發的 hook 記的，照舊在同一次執行的同步點 i＋1 的第 0 步套用（F.2 第 0 步、K-S10）。只有「最後一個同步點觸發的」改由輪套用。
- **輪開始時 `window_` 一定是空的**：計畫的最後一步是最後一個 stage 的同步點，或緊接在它後面的清舊事件（`Scheduler.cpp:214-226`、`:235-256`；每張 schedule 至少一個 stage，`Registry.cpp:883`）。用便宜的斷言守住。
- **新的不變量**：每次 `Run` 正常結束時，這張 schedule 的 hook 佇列是空的；所以每次 `Run` 開始時也是空的，在 `Run` 開頭用便宜、常開的斷言檢查（只看自己這張，K0 的巢狀下也成立）。
- **跨 `Run` 保留的行為消失**：現在 hook 佇列跨 `Run` 保留（`World.cpp:450-452`、`:690-698`）。依賴它的寫法搜尋結果見 5.6。
- **中途出錯的那一次不做輪**：既有的 E.5（`Executor.cpp:54-60` 的 `Fail` → `World.cpp:706-712` 的 `DiscardDeferred`）。

---

## 3. 要你核准的，與我補的細節

### 3.1 要你核准的條文改寫（AGENTS §3：Design Invariants 與規格）

照使用者定的形狀（不在計畫裡、在最後一個同步點之後、每輪新的 stamp），字面上要改的條文：

| 條文 | 改法（新句子在第 7 節） |
|---|---|
| target `kernel.zh-Hant.md` DI-KS 9 標題「結構變更延後、在宣告的同步點……套用」與第二句「hook 的變更屬下一窗」 | 標題加「與最後一個同步點之後的清空輪」；第二句加最後一個同步點的例外 |
| `world.zh-Hant.md` DI 5 標題「結構變更只在同步點」 | 加「與最後一個同步點之後的清空輪」 |
| `world.zh-Hant.md` DI 3、規格 A.1、X-5「stamp 依 Plan 順序推進」 | 補主會話指定的句子：「除了執行計畫的每一步，『跑完時清空的每一輪』也各取一個新的記號；記號仍然只往前走。」 |
| 規格 F-1「延後佇列只在 Plan 的同步點步驟套用」 | 加「與最後一個同步點之後的清空輪（F-12）」 |
| 規格 F-4「hook 記錄的變更屬於下一個同步窗」 | 加最後一個同步點的例外 |
| 規格 F-6「同步點後 `syncEpoch` +1」 | 加「清空的每一輪之後也 +1」（3.2 第 2 點） |
| 規格 F.1、F.3、F-9、J-36 的「hook 的 `planPos`／`writerPos`＝所屬同步點的位置」 | 加「清空輪裡的 hook：最後一個同步點的位置」（3.2 第 3 點） |
| 規格 F.7 第一列（不採 schedule 末的 `apply_final_deferred`，理由：明寫） | 加修訂註記：hook 佇列例外，由執行器在最後一個同步點之後套用；「明寫」由執行計畫與 C 指紋的版本號表示（使用者 2026-11-15） |
| 規格 F.7 最後一列、F.8 取捨 2（hook 的變更進下一窗、不做 hook 輪） | 加修訂註記：最後一個同步點之後做 hook 輪、有上限；中段照舊 |
| 規格 E.1（Executor 的快取）、E.2（同步點那一步）、E.5（Err 的來源） | E.1 加上限的快取；E.2 加「最後一個同步點之後的清空輪」；E.5 加「清空輪裡的錯誤與碰到上限」 |
| 規格 §讀法與 A.6 的 `KernelSettings`「內含兩個獨立參數」 | 改成三個；上限不受「四個條件」（啟動日誌等）約束，那是檢查開關的規則 |
| 規格 D.1 的 ScheduleDef、C.4 的啟動錯誤表 | 加 `hookRoundLimit`；新增 D-E20（schedule 指定 0） |
| 規格新增 F-12、J-54；target 新增 RC 14 | 新增 |
| 規格 H-4／C.2 第 5 點（指紋的內容） | 執行計畫指紋多「schedule 指定的上限」；C 指紋多同一項（3.2 第 4 點） |

不改的：規格 F-3（「沒宣告的地方不存在同步點」——清空輪不是同步點）、D.5 與 F.2 末段（「Scheduler 不自動插入」——Scheduler 沒有加步驟）。

### 3.2 我補的細節（還沒有經你判；照決定推的，請一併看）

1. **怎麼數**：第 1 輪套用「最後一個同步點套用完時佇列裡的紀錄」；第 k＋1 輪套用「第 k 輪裡被觸發的 hook 記的」。上限 N：最多 N 輪，第 N 輪後佇列仍不空就是 Err。一輪裡觸發了 hook 但沒記任何紀錄，佇列就空了，不算碰到上限。每一次 `Run` 從第 1 輪數。
2. **`syncEpoch` 每輪也加一**：它的意思是「套用過延後變更」（抓跨過套用還在用的舊指標，W-4）；每一輪會增減實體，和同步點一樣。佇列空時不做輪、不加。另一個做法是不加：輪之間沒有 System 執行，舊指標也已經因為呼叫不同而失效，結果看不出差別。我選加，理由是「每一次套用都加一」只有一種說法。
3. **位置**：輪裡被呼叫的 hook 的位置用**最後一個同步點的位置**。理由：清空不是計畫的一步，沒有自己的位置；用最後一個同步點的位置，錯誤訊息指得到一個真的步驟，事件的讀取也照舊——在「hook 所在的 schedule 就是事件的時鐘 schedule、清舊事件在預設位置」這兩個條件下，下一個 tick 每個讀者各讀到一次（讀取規則 `Params.hpp:419-420`；所有 System 都排在最後一個同步點之前），再下一個 tick 被清掉。條件不成立時（例如 Startup 的輪裡寫事件）的行為和現在「最後一個同步點的 hook 寫的事件」相同，是既有的洞（hook 不受 F-9 檢查，掃描第 26 項），說明頁講明。
4. **schedule 指定的上限放進指紋的方式**：
   - 放進執行計畫指紋（使用者定的）：`Plan` 帶上這張 schedule 的指定值（沒指定與有指定分開編碼）。
   - **也放進 C 指紋**（含 (c) 區 System 的 schedule，和 `rollbackEnabled`、stages、eventTrim 一起）。理由：分支會話向使用者講過的後果是「PvP 兩端對同一張設不同的值會連不上」，兩端比對的是 C 指紋（H-4）；只放執行計畫指紋做不到這一句。
   - **不放進 Registry 指紋的 schedule 欄位**（`Fingerprint.cpp` 開頭說「每一個 `ScheduleOptions` 的欄位」）：放進去就是 Registry 指紋的格式變了，而使用者定了 Registry 的版本號不加。Registry 指紋含每張執行計畫指紋的文字，所以這個值仍然間接被涵蓋。那一行註解改成「除了 `hookRoundLimit`，它在執行計畫指紋裡」。
5. **0 怎麼擋**：schedule 指定 0 是啟動錯誤 D-E20（`Finish` 時和其他宣告錯誤一起報）；`KernelSettings` 的預設設成 0 是斷言（Host 誤用＝前置條件），在第一次 `Run` 讀 `KernelSettings` 的地方檢查。`ScheduleOptions` 的欄位用「沒指定」與數值分開的型別（`std::optional<std::uint32_t>`），所以「沒指定」不會和 0 混在一起。
6. **錯誤碼與訊息**：新的錯誤碼 `HookRoundLimit`（加在列舉最後，`KernelError.hpp:35-60`），規則代號 `F-12`。訊息點名 schedule、上限與它來自哪一層（schedule 指定或產品預設）、剩下的筆數、最後一輪記下紀錄的 hook 名（依登記順序、去重；記在 hook 佇列旁邊，不改 `Detail::Record`）。上下文鏈最外層「Executor: hook round k after the last sync point (step N) of schedule 'X'」。輪裡別的錯誤也加這一層，葉子的錯誤碼與規則不變；`breakOnKernelError` 開著時照常叫 break 掛鉤。
7. **輪的狀態與驗證**：輪在套用狀態（`Applying`）裡做（F-5：只有 hook 能記；W-3：輪與輪之間沒有 System 執行）。驗證開著時，所有輪做完之後做一次 W-1 的結構檢查並重拍快照（不重拍的話，下一步或下一次 `Run` 的比對會把輪造成的變化當成外面改的）。

### 3.3 要你核准的守衛測試變更（AGENTS §14.4）

兩組，五點在第 5 節：

1. **三個子案例的寫法**（J-34 第 2、J-52 第 2、第 5）：新行為下它們測不到原本要測的東西，要改寫。其中 J-34 第 2 與 J-52 第 5 **有放寬的面**（5.2、5.4）；「一張 schedule 的執行不碰別張的 hook 佇列」這個性質從這一批合併到 K0 加上案例之間沒有測試守。
2. **指紋的固定值**（J-18、J-20、初始值的指紋、兩處 C 指紋版本的檢查）：判定不變、只有期待值變、沒有放寬（5.5）。

### 3.4 做不到一致的地方要事前講明（題 6）

**說明頁**（`system-contract-page.zh-Hant.md`）：第 5 節「hook」那一點補一句「hook 記的變更什麼時候生效，見第 9 節」；新增一節「hook 記的變更什麼時候生效」（K0 也要在這一頁加一節：先合併的那一批取第 9 節，後到的取下一個號）。內容：

1. 規則：
   - **hook 記的變更，永遠比觸發它的那一輪晚一輪生效。** 中段的同步點觸發的，在下一個同步點生效；最後一個同步點觸發的，在接下來的清空輪生效，所以這一次執行結束時都已生效。
   - 清空的輪有上限（預設 8；產品可以改預設，每張 schedule 可以另外指定）。碰到上限是程式的錯誤：這次執行停下、World 中斷，錯誤會點名 schedule 與 hook。
   - **Engine 的上限是保險，不是遊戲規則；連鎖幾次該停由遊戲自己的 System 來數。**
   - 提醒：每一輪清空是一次完整的套用。連鎖很長的玩法（例：棋子效果連動），更適合用自己的 System 加佇列來結算——這是作者的選擇，Engine 不擋。
2. **一個不固定的情形（甲；巢狀執行做好之後才會出現）**：每幀那張的某個 stage 生成子彈；hook 在那個 stage 的同步點記「加拖尾」；拖尾在**下一個**同步點才加上。驅動固定步的 System 如果就排在下一個 stage，這一幀的固定步都看到「有子彈、沒拖尾」；這一幀跑幾步固定步不固定（看真實時間），所以「沒拖尾的固定步有幾步」也不固定。Kernel 不替你解決：每一幀和固定步怎麼對應取決於遊戲的邏輯（使用者 2026-11-15 的理由）。
3. **要固定時的做法與代價**：

   | 做法 | 結果 | 代價 |
   |---|---|---|
   | 用初始值的宣告（說明頁第 8 節的漏斗），不用 hook | 拖尾在子彈出現的**同一個**同步點補上 | 拖尾的值只能從生成時的請求算出來；受漏斗的規則限制 |
   | 生成者和驅動固定步的 System 之間多隔一個 stage | hook 記的在驅動之前的同步點生效 | 多一個 stage；連鎖每多一層就要再多隔一個 |
   | 生成子彈的 System 直接把拖尾放進生成的內容 | 同一個同步點出現 | 生成者要知道拖尾；拖尾的寫者變成生成者 |
4. Startup 的清空輪裡 hook 寫的事件讀不到（3.2 第 3 點的條件）；要傳給之後的 schedule，用元件或 Resource。

**這一批之後還剩的不固定**：「hook 記的變更什麼時候生效」只剩甲，要等 K0 之後才會出現。同一個形狀的、System 記的變更（排在驅動 System 前面的，要等外層自己的同步點）是 K0 第 3.1 節已定的（`nested-apply-per-level`）。要不要把甲登記成一條 LIM，建議由 K0 決定。

### 3.5 和 K0 的接縫（題 7；只寫要求，不實作巢狀）

**會不會動到外層還沒套用的紀錄**：不會。外層 System 記的紀錄，K0 進入巢狀時存在每一層自己的佇列裡（K0 宣告第 4 節第 4 項）；外層 schedule 的 hook 佇列是另一張的（schedule 名字唯一，正在執行的那一張不能再被跑，K0 第 2 節第 3 項）；清空只取「這一次跑的那張」的 hook 佇列。

**替 K0 留的要求**：

1. 巢狀跑的那一次也做清空（2.2 第 1 項）。清空寫在執行器的步驟迴圈裡（最後一個同步點那一步之後）；巢狀的入口走同一個迴圈就自動有，另寫一份迴圈要保證清空也在。
2. 清空只取裡面那張自己的 hook 佇列；不碰外層存起來的紀錄與外層 schedule 的 hook 佇列。
3. 清空輪裡的 Z5 重查，照 K0 已定的「也看外面每一層還等著的紀錄與外層 schedule 的 hook 佇列」（`nested-details` 第 1 條）。輪走同一段套用（`phase`），K0 把這一點加在共用的套用裡就同時涵蓋。
4. 清空輪裡的錯誤照 K0 的規則往外傳（`RunSchedule` 回 Err、World 中斷、每一層的紀錄都丟掉）。
5. 上限照兩層取（裡面那張的指定值，沒有就用產品的預設）；每一次 `Run` 各自從第 1 輪數。
6. **K0 要加的守衛**：K0 宣告第 4 節第 14 項現在只寫了意圖（「hook 的佇列每張 schedule 一個」要寫測試固定），它的判定與突變清單裡還沒有案例。要加：案例「裡面那張的執行不套用、也不丟掉外層 schedule 的 hook 佇列」；突變「所有 schedule 共用一個 hook 佇列」「裡面那張的清空把所有 schedule 的佇列都清掉」。請主會話在 HANDOFF 留一筆。
7. `Run` 開頭的斷言只看自己那張的佇列，K0 不要改成「所有佇列都空」。
8. hook 不能跑 schedule（K0 已定），清空輪裡不會再巢狀。
9. K0 改 Kernel 的行為時，照 `not-in-plan-bump-versions` 的慣例把指紋的版本號加一（分支會話已向使用者講過）。

---

## 4. 要改的程式

| 位置 | 內容 |
|---|---|
| `engine/kernel/src/Executor.cpp` | 第一次 `Run` 讀 `KernelSettings` 時快取上限（0 是斷言）；找出計畫的最後一個同步點；那一步之後、下一步（清舊事件）之前做清空輪：每輪取新 stamp、設 `Applying`、套用、`syncEpoch` 加一；碰到上限或出錯走 `Fail`；所有輪之後驗證開著時做 W-1 與重拍快照；`Run` 開頭斷言這張的 hook 佇列是空的、輪開始時斷言 `window_` 是空的 |
| `engine/kernel/src/World.cpp`、`include/engine/kernel/World.hpp` | 套用一輪的入口（走同一段 `phase`，`recSeq` 歸零）；佇列空不空；最後一輪記紀錄的 hook 名；`World.hpp` 裡 hook 佇列的註解改寫 |
| `engine/kernel/include/engine/kernel/Registry.hpp` | `KernelSettings` 加 `hookRoundLimit = 8` |
| `engine/kernel/include/engine/kernel/Declarations.hpp` | `ScheduleOptions` 加 `std::optional<std::uint32_t> hookRoundLimit` |
| `engine/kernel/include/engine/kernel/Plan.hpp`、`src/Registry.cpp` | `Plan` 帶上 schedule 的指定值；`Finish` 檢查 0（D-E20）；`ToString` 加新的錯誤碼 |
| `engine/kernel/include/engine/kernel/KernelError.hpp` | `HookRoundLimit`；`StartupCode::DE20` |
| `engine/kernel/src/Fingerprint.cpp` | 執行計畫指紋版本 1→2，雜湊 schedule 的指定值；C 指紋版本 3→4，含 (c) 區的 schedule 加雜湊指定值；Registry 指紋版本不變、schedule 欄位不加；開頭的格式說明改寫；`ExportPlans` 印出指定值 |
| `engine/kernel/include/engine/kernel/Product.hpp` | `Run` 的步驟說明加清空輪 |
| `tests/common/kernel/HookFlushTests.cpp`（新）、`CMakeLists.txt` | 第 6 節判定 1 |
| `tests/common/kernel/WorldTests.cpp`、`ExecutorTests.cpp` | 第 5 節的三個子案例與新增的一個 |
| `tests/common/kernel/RegistryTests.cpp:41`、`SchedulerTests.cpp:129-134`、`InitialValueTests.cpp:904-906`、`:717`、`CoverageTests.cpp:504` | 指紋的固定值與版本（5.5） |
| `docs/architecture/plans/kernel/scripts/mutations.json` | 第 6 節判定 2 |
| 規格、target `kernel.zh-Hant.md`、`world.zh-Hant.md` | 3.1 的表 |
| 說明頁 `system-contract-page.zh-Hant.md` | 3.4 |
| dev_log、`target/v2/README.md` 的索引 | AGENTS §15 |

**不碰**：`.github/`；`build/` 底下受版本控制的檔；漏斗工具的行為；Z5 的規則本身；K0 的任何東西；Scheduler（不加步驟）；`Params.hpp`（`writerPos` 的寫法不變）；`DiscardDeferred`、`Fail`、`RunGuard` 的既有行為；`target/v2/HANDOFF.md`（主會話在另一個分支上改）。

`docs/cross-component-rules.md` 查過：沒有 hook 相關的規則。「hook 的連鎖要在上限內結束」是 Kernel 自己的契約，沒有第二個實作的地方，不登記。

每次 `Run` 多出來的固定費用（AGENTS §2）：開頭一次依名字找佇列並檢查空；最後一個同步點之後一次「佇列空不空」。有輪時才有套用的費用。

---

## 5. 既有條文與守衛測試的變更（AGENTS §14.4）

### 5.1 試做找到的（範圍：`gyo_kernel_tests`）

試做（`spike/`）在 `Product::Run` 結尾加清空的迴圈，其他不改。結尾佇列裡的紀錄和使用者定的位置（最後一個同步點之後、清舊事件之前）相同，所以「哪些行為測試會變」的結論也成立。

- 171 個案例：169 過、2 失敗；5 個斷言失敗，落在 3 個子案例。所有測試裡，最後佇列真的有紀錄的 `Run` 只有 3 次，正好是這 3 個子案例。
- 試做沒有改指紋，所以指紋的固定值當時沒變；照使用者定的加版本號之後會變（5.5）。

### 5.2 J-34 第 2 個子案例（`WorldTests.cpp:688-706`）——**有放寬的面**

現在：`Tick` 的最後一個同步點觸發 hook、記下 `Mode`；接著跑 `Other`；檢查 `Mode` 還沒加上；再跑 `Tick`，檢查加上了。改成：跑 `Tick` 之後立刻檢查 `Mode` 已加上；跑 `Other` 後仍在。名稱改成「a hook's records left at the last sync point are applied before Run returns」。

1. **守的性質**：(甲) 一張 schedule 的執行不碰別張的 hook 佇列（不套用、不丟棄）。(乙) 被拿掉的 `:703` 也守「hook 記的不在觸發它的同一次套用裡生效」。
2. **放寬的面**：「所有 schedule 共用一個 hook 佇列」或「結尾把所有 schedule 的佇列都清掉」——沒有巢狀時每次 `Run` 結束時佇列都空，看不出差別；「hook 的紀錄當場套用」——舊判定失敗於 `:703`，新判定通過（檢查者實驗 G1、G1o）。
3. **收緊的面**：「`Run` 結束時 hook 的紀錄還留著」（這一批之前的行為），以前通過、之後失敗。
4. **放寬的性質由誰守**：(乙) 由 J-34 第 1 子案例（`WorldTests.cpp:685`）、J-52 改寫後的第 2 子案例、`InitialValueTests.cpp:503` 抓到（實驗 G1），第 6 節的「中段不變」與「連鎖」案例也抓。(甲) 到 K0 加上案例之前沒有測試守；沒有巢狀時它不影響任何結果（3.5 第 6 點）。
5. **證據**：試做 `spike-tests.log`（舊判定在新行為下失敗於 `:703`）；5.6 的對照。

### 5.3 J-52 第 2 個子案例（`ExecutorTests.cpp:719-741`）——沒有放寬的面

現在：單一 stage、跑兩次；hook 在唯一的同步點記「刪除 1」（`:736` 檢查 1 還活著）；第二次 (c) 區的 System 往 1 插入，第 0 步的 Z5 重查回 Z5（`:738-739`）。新行為下，「刪除 1」在第一次 `Run` 的清空輪就套用了（合法），要測的第 0 步重查不再發生。

改成：兩個 stage、跑一次。stage A 的同步點觸發 hook 記「刪除 1」；stage B 的 (c) 區 System 往 1 插入；stage B 的同步點第 0 步重查，回 Z5。檢查：回 Z5、1 還活著、1 沒有 `Pos`。另**新增**一個子案例固定新的行為：原本的單一 stage 排法下，「刪除 1」在第一次 `Run` 裡套用；第二次的 (c) 區插入被丟掉、`DroppedMissingEntity` 加一、不報錯。

1. **守的性質**：(b) 區 hook 記的刪除在下一個同步點套用前，和那個窗裡 (c) 區要插入的紀錄一起查 Z5（K-S10）。被拿掉的 `:736` 由新寫法的「1 還活著」接住。
2. **放寬的面**：無。舊判定抓到的錯（`we-j52-step0-unchecked`；「當場套用」，實驗 G1）新寫法也抓到。
3. **收緊的面**：無。　4. —　5. **證據**：試做；5.6。

### 5.4 J-52 第 5 個子案例（`ExecutorTests.cpp:769-790`）——**有放寬的面**

現在：單一 stage、跑兩次；(c) 區的 hook 記「往 1 插入 `Pos`」（`:786` 檢查還沒加上）；第二次 (b) 區的 System 記「刪除 1」，同步點第 0 步加上 `Pos`、第 2 步重查回 Z5。新行為下，`Pos` 在第一次 `Run` 就加上；第二次記刪除時記錄當下的檢查就回錯——錯誤碼一樣，但走另一條路；試做中只有 `:786` 失敗、最後的錯誤碼檢查**仍然通過**（「測試還綠、但不在測原本的東西」）。

改成：兩個 stage、跑一次。stage A：(c) 區 System 生成 2 帶 `Vel`，同步點觸發 (c) 區 hook 記「往 1 插入 `Pos`」；stage B：(b) 區 System 記「刪除 1」（`Pos` 還在佇列裡，記錄當下合法）；stage B 的同步點第 0 步加上 `Pos`、第 2 步重查，回 Z5。**另加一句分辨是哪一條路回的錯**：錯誤文字含同步點重查的訊息（`at the sync point`，`World.cpp:538`），不含 System 記錄當下的訊息（`System 'ZoneBDespawnsAtOne'`，照 `ExecutorTests.cpp:351-352` 的寫法）。

1. **守的性質**：Z5 的重查看的是第 0 步套用之後的狀態（B-11、K-S10）；被拿掉的 `:786` 也守「hook 記的不當場套用」。
2. **放寬的面**：「hook 的紀錄當場套用」——舊判定失敗於 `:786`，新判定通過（實驗 G1、G1o）。
3. **收緊的面**：無。
4. **放寬的性質由誰守**：同 5.2 的 (乙)。重查那一半：舊判定抓到的 `we-j52-recheck-pending-only` 新寫法也抓到。
5. **證據**：試做；5.6；檢查者的實驗。

### 5.5 指紋的固定值（J-18、J-20、初始值的指紋、C 指紋版本的檢查）——判定不變、只有期待值變

| 位置 | 現在 | 之後 |
|---|---|---|
| `RegistryTests.cpp:41`（J-18 的 Registry 指紋） | `2:8fed…` | `2:` 開頭的新值（版本不變，值因為含執行計畫指紋的文字而變） |
| `SchedulerTests.cpp:129-134`（J-20 的三組：FixedUpdate 的執行計畫指紋、Registry 指紋） | `1:…`、`2:…` | `2:…`、`2:…`（新值） |
| `InitialValueTests.cpp:904`、`:906`（初始值的 Registry 指紋、C 指紋） | `2:0f51…`、`3:8a03…` | `2:` 新值、`4:` 新值 |
| `InitialValueTests.cpp:717`、`CoverageTests.cpp:504`（C 指紋的版本） | `== 3U` | `== 4U` |
| `RegistryTests.cpp:65`、`InitialValueTests.cpp:716`（Registry 指紋的版本） | `== 2U` | 不變 |

1. **守的性質**：指紋在三個平台上相同、只取決於登記與宣告（J-18、J-20、D-1、R-1）；版本號表示格式。
2. **放寬的面**：無。每個判定仍然是「等於一個固定值」；換掉的只是那個值。
3. **收緊的面**：無（多雜湊了 schedule 指定的上限：以前設不同上限的兩份宣告指紋相同，之後不同——這是新增的判定，不是既有判定的收緊）。
4. —
5. **證據**：新的值在實作後由三平台的 L1 確認相同；「放寬的面：無」用 gyo2 清單裡 `Fingerprint.cpp` 的 24 個既有突變重跑來證明（換值之後仍然全被抓到）。

### 5.6 突變的對照與搜尋

試做裡把三個子案例改成上面的寫法（不含 5.4 的「分辨路徑」那一句），只跑 J-34、J-52（`spike/spike-mutants.log`；「當場套用」那一列是檢查者的實驗 G1、G1o）：

| 突變 | 舊判定 | 新判定 | 讀法 |
|---|---|---|---|
| 不做清空（這一批之前的行為） | 通過 | **失敗** | 收緊的面 |
| 所有 schedule 共用一個 hook 佇列 | **失敗**（`WorldTests.cpp:703`） | 通過 | 5.2 的放寬的面 |
| hook 的紀錄當場套用 | **失敗**（`:703`、`ExecutorTests.cpp:736-739`、`:786`） | J-34 新第 2、J-52 新第 5 通過；J-52 新第 2、J-34 第 1、`InitialValueTests.cpp:503` 失敗 | 5.2、5.4 的放寬的面；由別的判定守 |
| `we-j52-step0-unchecked` | 失敗 | 失敗 | 5.3 沒有放寬 |
| `we-j52-recheck-pending-only` | 失敗 | 失敗 | 5.4 重查那一半沒有放寬 |

搜尋（`build/evidence/gyo2-hook-flush/grep-cross-run-dependency.log`；用結束碼區分「沒有命中」與失敗）：同一個樣式先命中已知的地方 `git grep -c "\.Hook<" -- tests/common/kernel`（8 個檔）；`git grep -n "\.Hook<" -- ':!tests/common/kernel' ':!docs'` 沒有命中（kernel 測試以外沒有登記 hook；gyo2 上沒有產品）；`git grep -n -E 'Run\("Other"\)' -- tests/common/kernel` 命中 `WorldTests.cpp:702`（5.2）與 `ExecutorTests.cpp:808`（沒有 hook，試做中通過）。

---

## 6. 判定（先宣告，結果出來之後不放寬）

**判定 1：行為。** `HookFlushTests.cpp`（新，J-54）全部通過，加上第 5 節的子案例。「深度 d」＝要 d 輪。帶 hook 的 schedule 一律**不是第一個登記的**。

| 案例 | 排法 | 驗什麼 |
|---|---|---|
| 乙 | 另一張先登記；`Tick`（三個 stage）最後一個 stage 的 System 生成子彈；OnAdd(子彈) 的 hook 記「加拖尾」 | `Run("Tick")` 一回來就有拖尾；接著跑另一張時鐘的 schedule，裡面的 System 數到 1。驗證開、關各一次 |
| 丙 | `startup = true` 的 schedule，最後一個同步點觸發的 hook 記「插入元件」 | 跑一次回來後元件在。驗證開、關各一次 |
| 連鎖 | 最後一個同步點觸發 hook A 記 X；X 的 OnAdd hook B 記 Y；Y 的 OnAdd hook C 記 Z（深度 3） | X、Y、Z 都在；`Stamp()` 與 `SyncEpoch()` 各比沒有 hook 時多 3；X、Y、Z 的 added stamp 依序遞增，都大於最後一個同步點的 stamp |
| 上限的邊界（預設） | 深度 8 與 9；深度 8 的第 8 輪裡另觸發一個不記紀錄的 hook | 8：Ok；9：Err |
| 產品改預設 | `KernelSettings{…, hookRoundLimit = 2}`，深度 2 與 3 | 2：Ok；3：Err |
| schedule 指定蓋過產品預設 | 產品預設 2、這張 schedule 指定 5，深度 5 與 6；另一張沒指定的 schedule 深度 3 | 5：Ok、6：Err；沒指定的那張 3：Err（用產品的 2） |
| 每次 `Run` 重新數 | 上限 2，連跑三次、每次深度 2 | 三次都 Ok |
| 第一次 `Run` 之後改 `KernelSettings` | 跑過一次後 Host 寫 `KernelSettings` | 斷言（既有的 J-50 規則） |
| 0 | schedule 指定 0；`KernelSettings` 的預設 0 | 前者 `Finish` 報 D-E20；後者第一次 `Run` 斷言 |
| 指紋 | 同一組登記：(1) 產品預設設與不設；(2) 含 (c) 區 System 的 schedule 指定與不指定；(3) 只有 (b) 區的 schedule 指定與不指定；版本 | (1) 三種指紋都相同；(2) 執行計畫指紋、C 指紋、Registry 指紋都不同；(3) 執行計畫指紋與 Registry 指紋不同、C 指紋相同；執行計畫指紋版本 2、C 指紋版本 4、Registry 指紋版本 2；`ExportPlans` 印出指定值 |
| 碰到上限的處置 | 不會停的連鎖：兩個 hook（登記順序 P、Q）掛在 OnAdd(T)，各生成帶 T 的新實體，Q 每次記兩筆 | 錯誤碼 `HookRoundLimit`、規則 `F-12`；World 中斷；**handler 恰好一次**；訊息含 schedule 名、上限與它來自哪一層、剩下的筆數、hook 名依「P、Q」各一次；第 N＋1 層沒有被套用；之後再 `Run` 是斷言 |
| break 掛鉤 | 同上，`breakOnKernelError` 開、關 | 開：叫；關：不叫 |
| 出錯時清舊事件不做 | 帶時鐘的 `Tick`（清舊事件在最後）：第 1 個 tick 寫事件；第 2 個 tick 的清空輪裡 hook 回 Err | `Run` 回 Err；第 1 個 tick 的事件還在緩衝裡（清舊事件沒做）；handler 恰好一次；已經套用的輪不退回 |
| 中段不變 | 三個 stage：stage A 的同步點觸發的 hook 記 `Mode`；stage B、C 各一個 System 數 `Mode` | B 數到 0、**C 數到 1**；`Mode` 的 added stamp 等於 stage B 那個同步點的 stamp；`Stamp()`、`SyncEpoch()` 和沒有 hook 時相同（沒有用到輪） |
| 輪在套用狀態裡 | 清空輪裡的 hook 從 System 的路徑記一筆（照 `WorldTests.cpp:707-716`） | 斷言（F-5） |
| 一輪是完整的套用 | 在輪裡：(1) 同一輪 (b) 區 hook 刪除 E、(c) 區 hook 往 E 插入；(2) 插入漏斗的請求；(3) 刪除；(4) 移除元件；(5) Resource 的插入 | (1) 回 Z5、E 沒被刪；(2) 目標補上、目標的 OnAdd 被呼叫、它記的下一輪套用；(3)(4) OnRemove 被呼叫、格子沒了；(5) Resource 在 |
| 同一輪同一格兩筆 | 同一輪兩個 hook 往同一個（實體, 元件）各插一筆 | 後記的勝 |
| 跨輪不合併 | 第 1 輪 hook 記「移除 C」、第 1 輪裡被觸發的 hook 記「加上 C」（第 2 輪）；另：第 1 輪 (b) 區 hook 刪除 E、被觸發的 (c) 區 hook 往 E 插入（第 2 輪） | 移除與加上各自生效（OnRemove 再 OnAdd）；第二組不報 Z5、插入被丟掉、`DroppedMissingEntity` 加一 |
| 驗證開著、輪之後的快照 | 輪的最後一筆變更是 Resource 的操作；接著再跑一次 | 不報 `PreCallChange` |
| hook 寫的事件 | `Tick` 是事件的時鐘 schedule、清舊事件在預設位置；清空輪裡 hook 寫事件；`Tick` 裡前後各一個讀者 | 下一個 tick 兩個讀者各讀一次；再下一個 tick 讀不到 |
| 輪裡 hook 丟例外 | 第 1 輪的 hook 丟例外 | 例外照常傳出；World 中斷；剩下的沒套用 |
| 中途出錯不做輪 | stage A 的同步點觸發的 hook 記 X；stage B 的 System 回 Err | `Run` 回 Err；X 沒套用；handler 一次 |

**判定 2：檢出力。** 突變清單在實作前由實作者（Opus）定案，交別的模型（Fable／high）檢查缺口，加進 `mutations.json`。只有預期的那個案例失敗才算抓到（期待字串寫到失敗的 CHECK 式或專用訊息）；建置失敗不算。

- **新的**（至少）：不做輪；只做一輪；上限差一；上限寫死 8（不讀兩層）；schedule 指定不蓋過產品預設；產品預設不讀 `KernelSettings`；碰到上限不回 Err、把剩下的留著；不中斷；不叫 break 掛鉤；handler 叫兩次；輪數不歸零；輪不取新 stamp；輪不加 `syncEpoch`；輪在每個同步點都做（＝不採的 (a)）；中段的第 0 步不取 hook 佇列、全部留到清空；清空排到清舊事件之後（「出錯時清舊事件不做」抓）；輪對 Startup 跳過；輪只在驗證開著時做；輪不在套用狀態裡；取第一張的佇列；`recSeq` 每筆不遞增（同一輪同一格兩筆→鍵重複的斷言）；跨輪合併成一次套用；輪的套用另寫一份只處理生成與插入；訊息不列 hook 名、不去重、或不寫上限的來源；schedule 指定 0 不報；`KernelSettings` 0 不斷言；執行計畫指紋不雜湊指定值；C 指紋不雜湊指定值；任一個版本號不加；Registry 指紋的 schedule 欄位雜湊了指定值（版本不變卻改格式）；所有輪之後不重拍快照。
- **改動範圍內的既有突變**：master 那一側的 world-exec 批次（分支 `claude/arch-kernel-world-exec`，`50cb2050`）有 91 個 `we-*` 突變，**沒有帶進 gyo2 的 `mutations.json`**（gyo2 的清單裡 `World.cpp` 只有漏斗的 5 個、`Executor.cpp` 一個也沒有）。原文落在 `Product::Run`、`ApplySync`、`DiscardDeferred`、`Fail`、`RunGuard`、`RunHook`、`RunHooks`、`HookQueueOf`、`Push` 裡的 38 個（名單在第一輪檢查報告 S6）搬進來；每一個先在 gyo2 的 base 上跑，確認能建置、被預期的案例抓到；`find`／`replace`／期待字串照 gyo2 重寫（例：`we-j34-hook-immediate` 的 replace 用了 gyo2 沒有的 `hookQueue_`）。已知的例外：`we-j7-despawn-reverse` 在 gyo2 的 base 上就沒有測試抓到（既有的缺口），記錄、報給主會話，這一批不修。
- **重跑的既有突變**：漏斗的 5 個（`rs-iv-fc*`）、`Fingerprint.cpp` 的 24 個（換指紋的值之後）。

**判定 3：沒有回歸。** 本機完整 `ctest` 全過；`gyo_kernel_tests_reversed` 全過；L1 三平台（linux-x64、windows-x64、macos-arm64）全過，三平台的新指紋值相同。既有測試除了第 5 節列的，判定一個都不改。

---

## 7. Architecture Delta、Design Invariants、Replacement Criteria

### 7.1 Architecture Delta（AGENTS §3 的七項）

1. **Feature**：乙、丙（使用者點名）；K0 的「Startup 只跑一次」；回滾的快照完整（我推的、計畫中的）；作者照玩法設上限（使用者的自走棋例子）。
2. **現在的問題**：hook 佇列跨 `Run` 保留（`World.cpp:450-452`、`:690-698`），紀錄什麼時候套用取決於那張 schedule 下一次什麼時候被跑；只跑一次的 schedule 永遠不套用。
3. **變的邊界**：執行器在最後一個同步點之後多一段（不在計畫裡）；`KernelSettings` 與 `ScheduleOptions` 各多一個欄位；執行計畫指紋與 C 指紋的格式升版；錯誤碼與啟動錯誤各多一個。
4. **影響**：只有 `engine/kernel`、`tests/common/kernel`、文件；gyo2 上沒有產品。
5. **依賴方向**：不變。
6. **Ownership**：hook 佇列仍然是 World 的狀態；什麼時候套用仍然由執行器決定；產品預設屬於 `KernelSettings`（組裝的 Host 設）、schedule 的指定屬於 schedule 的宣告。沒有新增別的 Component 的狀態的寫照。
7. **為什麼不能更小**：「只講明」與「只套用一輪」使用者都否決了；放的位置與指紋使用者已定。

### 7.2 新的句子（要你同意）

**DI-KS 9**（target `kernel.zh-Hant.md`）：

> **結構變更延後，在宣告的同步點，以及計畫最後一個同步點之後的 hook 清空輪，依「hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource」套用，每階段依 id，同鍵依 (planPos, recSeq)，鍵唯一。** Running 期間成員不變；hook 的變更屬下一窗——最後一個同步點觸發的，在接下來的清空輪一輪一輪套用到空，每一輪是一次完整的套用，連鎖超過上限是 Err（F-12）；每次 `Run` 開始與結束時，這張 schedule 的 hook 佇列都是空的；結果不依 System 之間的記錄順序。

守衛加 J-54。

**world DI 5** 標題：「結構變更只在同步點與最後一個同步點之後的 hook 清空輪、順序寫死、Despawn 不是寫者」。

**world DI 3、規格 A.1、X-5** 加：「除了執行計畫的每一步，『跑完時清空的每一輪』也各取一個新的記號；記號仍然只往前走。」

**規格 F-1** 加「與最後一個同步點之後的 hook 清空輪（F-12）」；**F-4** 加「；最後一個同步點觸發的，在接下來的清空輪套用（F-12）」；**F-6** 加「；清空的每一輪之後也 +1」。

**規格新增 F-12**：「每次 `Run` 正常結束時，這張 schedule 的 hook 佇列是空的。計畫的最後一個同步點之後、清舊事件之前，執行器把這張 schedule 的 hook 佇列一輪一輪套用到空；每一輪是一次完整的套用（F.2 的順序、Z5 的重查、漏斗的補值），取一個新的 stamp，`syncEpoch` 加一。最多 N 輪：N＝schedule 宣告的 `hookRoundLimit`，沒有就用 `KernelSettings::hookRoundLimit`（預設 8），最少 1。第 N 輪之後仍有紀錄是 Err（`HookRoundLimit`）：World 中斷、剩下的丟掉、已套用的輪不退回、清舊事件不做。清空不是執行計畫的一步；執行計畫指紋（版本 2）與 C 指紋（版本 4）表示這條規則，並含 schedule 指定的上限。」檢查：J-54。

**規格 F.7 第一列**加：「（2026-11-15 修訂，使用者：hook 佇列例外——執行器在最後一個同步點之後套用到空；不寫進計畫，以執行計畫與 C 指紋的版本號表示，`v8-arch-gyo2-hook-flush-not-in-plan-bump-versions`）」。**F.7 最後一列、F.8 取捨 2** 加：「（2026-11-15 修訂，使用者：最後一個同步點之後做 hook 輪、有上限；中段的同步點照舊，`v8-arch-gyo2-hook-queue-flush-at-run-end`）」。

### 7.3 Replacement Criteria

新增 **RC 14. hook 的連鎖上限成為負擔**（RC 12、13 已有人用，`kernel.zh-Hant.md:112-116`）：同一個版本內，為了不碰上限而把上限調到 32 以上，或把本來寫在 hook 裡的工作搬到 System 兩次以上。→ 停下來重看 hook 的模型：每個同步點把連鎖做完（使用者 2026-11-15 不採的 (a)），或 hook 不准記延後的變更（(c)）。

---

## 8. 誰做、用什麼檔位

上段（Design Invariants、Kernel 的行為、公開介面、指紋的格式）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作 | Opus／high（本卡） | 容易錯的是錯誤路徑（中斷、丟棄、快照、handler 次數）、輪的邊界與指紋的編碼；測試先寫 |
| 審查 | Fable（主會話） | 和實作者不同模型 |
| 突變清單的對抗檢查、§14.4 的驗證者 | Fable／high（唯讀子代理） | 和定清單、提案的人不同模型 |

時間的估計（agent 的時間）：實作與測試 4 到 5 小時（判定 1 有 24 個案例、指紋與兩層上限）；突變（新的約 35 個、搬進來的 38 個、重跑 29 個）與對抗檢查 2.5 到 3.5 小時；文件 1 到 1.5 小時；PR 與 L1 等待 0.5 小時。合計 8 到 10.5 小時。超過 1.5 倍就停下來回報。

---

## 9. 範圍的限界

- 修訂 2 沒有送檢查（任務卡允許兩輪，第二輪中途停掉、沒有結果）。建議實作開始前，用第二輪對修訂 2 做一次範圍較窄的複查（第 2.3、3.1、3.2、5.5、6 節）。
- 試做的形狀是「`Run` 結尾」，不是「最後一個同步點之後、清舊事件之前」；兩者結尾佇列裡的紀錄相同，所以「哪些行為測試會變」的結論成立，但使用者定的位置本身沒有試做。指紋的變更也沒有試做。
- 情形甲在 K0 之前不會發生，只在說明頁講明，沒有測試能固定它。
- 「一張 schedule 的執行不碰別張的 hook 佇列」從這一批到 K0 加上案例之間沒有測試守（5.2）。
- hook 不受 F-9 的檢查（既有的洞）；清空輪裡 hook 寫的事件仍然有（3.2 第 3 點）。
- `KernelSettings` 是整個行程共用一個值（LIM-6）：之後一個行程有多個 World 時，產品預設的上限也共用。
- `we-*` 突變裡不在準則內的 53 個沒有搬進 gyo2；要不要整批搬建議交主會話判斷。`we-j7-despawn-reverse` 在 gyo2 的 base 上存活（既有的缺口）。
- W-1 的結構檢查與 `Run` 開頭的斷言在正常的程式下不會觸發，拿掉它們的突變抓不到；它們是保險。
- 5.2、5.4 的五點：§14.4 要求在請你核准前做突變與別的模型的驗證。突變只在試做的草稿子案例上跑過；別的模型的驗證是第一輪檢查（它找出第二個放寬的面，已補進）。正式的子案例寫好後照判定 2 重跑；結果和第 5 節不同就停下來回報。

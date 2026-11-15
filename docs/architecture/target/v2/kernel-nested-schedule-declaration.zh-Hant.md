# 事前宣告：獨占 World 的 System 可以跑一張 schedule（`v8-arch-gyo2-nested-schedule`）

更新：2026-11-15（修訂 6）。Owner：架構（GYO2 重建會話）。**狀態：方向與修訂 5 第 10 節的二十三條細節，使用者都已定。還要你判的六件列在第 10 節（四個是我照你的決定補的細節，兩個是核准）。核准之前不動工。**

這一份是整份改寫：把 2026-11-15 當天定的事全部併進正文，不再用「待你判」的註記疊在舊文字上。各題當時的選項表與我建議時的理由不再抄一遍，留在修訂 5（提交 `eccda9bb` 的這個檔）；你的原話與理由以 [HANDOFF](HANDOFF.md) 的決定表為準。

經過：
- 初稿到修訂 5：四輪別模型檢查（Opus，唯讀），blocker 數 3、5、2、1；報告在主 checkout `build/evidence/gyo2-time-v2/`（`k0-declaration-check-opus.md`、`-r1-recheck-opus.md`、`-r3-check-opus.md`、`-r4-check-opus.md`）。
- 修訂 5 之後定的：第 10 節逐條過完（主會話與兩個分支會話）；三次探索、兩次對抗檢查、一次確認用的試做（[巢狀佇列的統合](nested-queue-spike-synthesis.zh-Hant.md)、[用 stamp 分先後的統合](stamp-order-spike-synthesis.zh-Hant.md)）；hook 那一批已另外做完（#234）。
- 修訂 6 改掉修訂 5 的三處設計：Startup 改由 Kernel 自己跑（不再由作者的 System 跑、不需要查詢）；閉包可以回傳值；指紋的版本號要加一（修訂 5 寫「不變」）。**修訂 6 寫完之後送了第五次檢查（Opus／high，唯讀）；結果補在這裡。**

根據：[時間與 runner 的設計](rebuild-time-design.zh-Hant.md)第 0 節；HANDOFF 的 `v8-arch-gyo2-fixed-step-in-world`（固定步由 World 裡的 System 驅動）與 `v8-arch-gyo2-time-plan-two`（獨占 World 的 System 可以跑任何一張 schedule，開放給所有作者；理由：「固定步這個我不想限制作者的遊戲邏輯。關鍵不開放給一般作者，我怕會限制Engine能力」）。

---

## 1. 這一批做什麼

用射擊遊戲當例子：每一幀依序「讀輸入 → 跑固定步（0 到數步）→ 算畫面」；遊戲開始時先跑一次開局（載入地圖、生成出生點）。

這一批讓「跑固定步」這件事可以由 World 裡的一個 System 來做：持有 `ExclusiveWorld` 參數的 System 可以在執行中跑另一張 schedule，跑幾次由它自己決定。

```cpp
// 例：驅動固定步的 System（它本身是下一批的事，這裡只是示意）
SystemResult DriveFixedSteps(ExclusiveWorld world) {
    for (int i = 0; i < steps; ++i) {
        if (auto ran = world.RunSchedule("FixedUpdate"); !ran) return Engine::Base::Err(std::move(ran).error());
    }
    return Ok();
}
```

一個 PR（使用者：「不拆」），裡面分三段審：
1. **巢狀執行**：執行器、每一層的延後紀錄、出錯的處置、存取驗證。
2. **`ExclusiveWorld` 的新介面**：在閉包裡讀寫元件與 Resource（不交出指標）、帶寫入的宣告。
3. **開局（Startup）由 Kernel 跑**：一個 World 一張，第一次執行別的 schedule 之前自動跑完。

**需要它的 Feature 與 Product**（AGENTS §2）：固定步由 World 裡的 System 驅動（①PvP、②單機，已定）。這一批只為這一件事做。

**為什麼是基盤**：它改的是 Kernel「一次只有一張 schedule 在跑」這條規則。同步點、延後的變更、存取驗證、出錯的處置都建立在這條規則上，所有 System 都跑在它上面。

**這一批不做的**：時間的 Resource、通用的時間、驅動固定步的那個 System 本身（包括它掛在哪個 stage）、runner、事件的游標、多個 World、「生成被拒時丟掉那一筆」（另一批 `v8-arch-gyo2-spawn-reject`，排在這一批前面）。回滾的重跑仍然只由外面在 World 閒置時呼叫（DI-KS 12 不變）。

---

## 2. 作者看到的行為

1. **誰能跑別的 schedule**：以 `ExclusiveWorld`（帶不帶寫入的宣告都算）為參數的 System。既有規則：它是那個 System 唯一的參數，只能是 (b) 區。
   - run 條件不能：它不能持有 `ExclusiveWorld`（編譯期就擋）。
   - hook 不能：hook 是在同步點套用到一半時被呼叫的，這時再跑一張 schedule 會改到正在走的資料。
   - 一般的 System 繞路（例如經全域指標）呼叫 `Product::Run`：照舊斷言。
2. **能跑哪一張**：這個 World 登記過的 schedule，除了開局那一張與 extract。巢狀可以多層（A 跑 B，B 裡的獨占 System 再跑 C）；沒有另設層數上限，自然的上限是 schedule 的張數（正在執行的那一張不能再跑）。
3. **被拒絕的呼叫**——`RunSchedule` 回 Err：跑正在執行中的那一張（自己，或外面任何一層的）、名字不存在、hook 裡呼叫、讀寫的閉包裡呼叫、跑開局那一張、開局執行中跑別的 schedule、extract。
   - 這些都是程式的 bug（Programmer Error），照 Kernel 既有的做法處理：錯誤記在這次呼叫上，**外層吞掉它回 Ok，World 也會中斷**。和 Bevy 的 `try_run_schedule`（可以接住繼續跑）不同。
   - 訊息要講明機制。例：開局執行中跑別的 schedule 時，訊息指出是哪個 System、想跑哪一張、原因（開局的 System 不算寫者，所以開局要單獨跑完）。
4. **裡面那張照常完整執行**：它的 System、同步點、hook、跑完時清空 hook 記的變更、tick 的推進（它是帶時鐘的 schedule 時）、事件的修剪。和從外面跑它沒有差別。
5. **開局（Startup）**（第 3.3 節）：
   - 作者把開局要做的事寫成 System，登記到打了 Startup 記號的那一張 schedule；先後用現有的 before／after。一個 World 只准一張。
   - 什麼時候跑不由作者決定：這個 World 第一次執行別的 schedule 時，Kernel 在第一個 System 之前先把開局跑完，之後不再跑。
   - 主程式可以在第一幀之前自己先呼叫一次（例如測試或工具只想跑開局再檢查）；Kernel 看到已經跑過就不再跑。
   - 開局裡有 System 回報錯誤：觸發它的那一次執行回報錯誤，World 中斷。載入的時間算在第一幀。
6. **讀寫資料的形狀**（第 3.2、3.4 節）：
   - 元件與 Resource 都在閉包裡讀寫：`world.Read<T>(實體, f)`、`world.Modify<T>(實體, f)`；Resource 不帶實體。介面不交出指標。
   - 閉包可以回傳值，例如 `int hp = …Read<Health>(玩家, [](const Health& h) { return h.value; })`。指標、參考、檢視這類「指回 World 裡面」的型別在編譯期擋掉；整份複本可以。
   - 元件：讀任何元件；寫只准 (b) 區、標 `Shared` 的（既有規則）。
   - Resource：讀任何 Resource（`KernelSettings` 除外）。標 `Shared` 的可以直接寫，不用列；單一寫者的要列在宣告 `ExclusiveWorld<Writes<…>>` 裡才能寫，列了就算進寫者。只能讀寫值，不能新增或移除 Resource。
   - 實體沒有那個元件、或 Resource 不存在：不呼叫 f，結果說明「沒有」。
7. **跑完之後外層的 System 繼續執行**：手上的 `ExclusiveWorld` 照常可用；之後的寫入用新取的變更記號（stamp），別的 System 的變更偵測看得到。
8. **裡面那張出錯**：
   - World 立刻中斷，`RunSchedule` 回 Err；外層的 System 應該把它往上回傳。
   - 錯誤處理的函式只被呼叫一次（最外層），拿到的錯誤包含裡面那一步與外層的 System。外層回傳的是另一個 Err 時，裡面那個會併進外層這次呼叫的錯誤清單。多層時留最裡面先發生的那個。
   - 外層吞掉錯誤回 Ok：World 仍然是中斷的，錯誤照樣報出一次。
   - 中斷之後再呼叫 `RunSchedule`（例如寫成迴圈的）：不執行，直接回傳留下的那個錯誤。
   - 裡面那張丟出例外、外層接住：World 已經中斷。外層的 System 回來後，執行器看到 World 中斷就停下，不跑後面的步驟；沒有留下錯誤時補一個「巢狀執行被例外中止」，錯誤處理呼叫一次。
   - 中斷時，每一層還沒套用的延後變更都丟掉（和現在中斷時的處置相同）。
9. **延後的變更**（第 3.1 節）：外層還沒套用的留在外層，等外層自己的同步點；裡面那張看不到。獨占的 System 自己沒有記延後變更的路徑；它回來之後，排在它後面的 System 照常記進外層的佇列。

---

## 3. 已定的事

| 題目 | 定下的 | HANDOFF 代號 | 比過、沒有採用的 |
|---|---|---|---|
| 套用時機 | 各層的紀錄各自套用：記下的變更在記它的那張 schedule 的同步點套用 | `v8-arch-gyo2-nested-apply-per-level` | 佇列不空就報錯；一律自動加同步點；照 Bevy 的條件自動加；呼叫時當場套用 |
| 規則 Z5 在巢狀時 | 重查也看外面每一層留著的紀錄，連外層 schedule 的 hook 佇列 | `v8-arch-gyo2-nested-details` | 不補、寫成已知的後果 |
| 外層記了刪除、裡面用同一個編號生成 | Engine 不加機制；寫法在說明頁講明 | `v8-arch-gyo2-nested-same-id-respawn-not-engine` | 用 stamp 分先後；「先刪再生」延伸到巢狀；掃外層後回報錯誤 |
| 往還活著的編號生成 | 機制丟掉那一筆、留計數、給理由（另一批） | `v8-arch-gyo2-spawn-reject-drop-count-reason` | 維持斷言；整次執行回報錯誤；丟掉不留痕跡 |
| Resource 的讀寫 | `ExclusiveWorld` 帶寫入的宣告；只有單一寫者的要列 | `v8-arch-gyo2-exclusive-declared-writes`、`v8-arch-gyo2-shared-means-no-owner` | 只准寫 `Shared` 的；放寬「唯一的參數」；`Shared` 的也要列 |
| 開局（Startup） | Kernel 保證只跑一次，而且由 Kernel 自己跑；一個 World 一張；執行中不准跑別的；不開「開局之後」那一格 | `v8-arch-gyo2-startup-once-by-kernel`、`v8-arch-gyo2-startup-run-by-kernel` | 不准巢狀跑它；放行重跑；由作者的 System 跑並提供查詢；Kernel 內建固定名字的一張；多張各跑一次 |
| extract | 巢狀跑它回 Err，到多個 World 那一批（T5）再定 | —（沒有選項：這一批做不出來） | — |
| 跑完之後的舊指標 | 介面不交出指標：用閉包；閉包可以回傳值，指回 World 裡面的型別在編譯期擋 | `v8-arch-gyo2-exclusive-no-pointers`、`v8-arch-gyo2-closure-may-return-values` | 維持既有的限界；拿出去的東西自己檢查；讀回傳複本；閉包不能回傳任何東西；回傳不檢查 |
| 讀到執行緒親和不合的 Resource | 執行期回 Err，World 中斷 | `v8-arch-gyo2-nested-details-15-17` | 斷言；不檢查 |
| hook 記的變更 | 每張 schedule 跑完時清空（已另一批完成，#234） | `v8-arch-gyo2-hook-queue-flush-at-run-end` | — |
| 這一批的大小 | 不拆，一個 PR | `v8-arch-gyo2-nested-details-15-17` | 先新介面後巢狀；先巢狀後新介面（違反已定的事） |
| 指紋 | 改 Kernel 的執行行為時版本號加一 | `v8-arch-gyo2-hook-flush-not-in-plan-bump-versions` | — |

### 3.1 套用時機：要知道的後果

規則只有一句：**記下的變更，在記它的那張 schedule 的下一個同步點套用。** 巢狀沒有多出新的規則，但有幾個後果要事前講明（都寫進說明頁，第 11 節）：

- 和跑固定步的 System 同一個 stage、排在它前面的 System 記的變更，裡面那張看不到，要等這一幀所有固定步之後才套用。照建議的排法（跑固定步的 System 自己佔一個 stage）就不會碰到。
- 外層 (b) 區的 System 記了往實體 E 的插入，裡面那張刪了 E：外層的同步點丟掉那一筆，有計數（和現在「往已經不在的實體插入」相同）。
- 外層 **(c) 區**記了往 E 的插入（或用指定的編號生成 E），裡面那張 (b) 區刪 E：裡面那張的同步點照 Z5 回報錯誤，E 不刪。這是補上的那條（不補的話 (c) 區的結果會跟著 (b) 區走而沒有訊息）。錯誤訊息要指出那筆紀錄在哪一層、哪張 schedule。
- 反過來（外層 (b) 區記了刪除 E，裡面那張 (c) 區往 E 插入）：裡面先套用，外層的同步點重查時回報 Z5。晚一個同步點、訊息不同。
- 外層記了刪除編號 X、裡面那張用同一個編號生成 X：Engine 不為它加機制。重入的標準寫法是在同一個 System 裡先記刪除、再用同一個編號生成（同一個同步點，固定先刪再生）。被丟掉或被外層那筆刪掉的情形在 `v8-arch-gyo2-spawn-reject` 合併之後照它的行為。
- 同步點觸發的 hook 再記的變更，進的是「正在套用的那張 schedule」的 hook 佇列，在那張跑完時清空。

存放的形式（架構師定，兩個試做證明和「另存堆疊」幾乎等價）：一個容器，這一層的紀錄從尾端切出；不加層的標記。

### 3.2 Resource 的讀寫

- **只有單一寫者的 Resource 要列；標 `Shared` 的不用列**（使用者：「shared的情況本身就是不需要知道誰寫的。要保證誰寫，別開shared。」）。列了就算進「只有一個寫者」的啟動檢查；沒列就寫單一寫者的 Resource，執行期回 Err（Resource 是不是 `Shared` 要到登記時才知道，所以在執行期擋，和元件現在的檢查同一種）。
- **讀不必列**：讀任何 Resource。兩個例外回 Err：`KernelSettings`（驗證的開關不能影響模擬結果，D-E18）；執行緒親和和這個 World 的 runner 不合的 Resource（一般的 System 讀它會在啟動時被擋；獨占的 System 的讀不在簽名上，只能執行期擋。訊息指出哪個 System、哪個 Resource、為什麼）。
- **列出來的必須是 (b) 區的 Resource**，否則啟動時報錯（既有規則 S-13）。
- **元件照舊**：只能寫 (b) 區、標 `Shared` 的，不用列。
- 它仍然是那個 System 唯一的參數（守衛 J-10 不變）。只能讀寫值，不能新增或移除 Resource。
- 獨占的 System 不能同時是 ingress；hook 不能用帶宣告的寫法。
- 存取驗證的兩層照舊把「整個 World」的存取當成可以寫 (b) 區 `Shared` 的資料，另外加上列出來的單一寫者 Resource。
- 已知的限制（你定之前看過）：通用的時間有兩個寫者，仍然得標 `Shared`；第三個人來寫，啟動時不會報錯。

### 3.3 開局（Startup）

背景：只掛在 Startup 的 System 不算寫者（DI-KS 6），理由是「它只跑一次、在別的 System 之前」。Kernel 以前不檢查這件事，靠外面只呼叫一次。

**定下的規則**（`v8-arch-gyo2-startup-run-by-kernel`；使用者：「如果是系統必須的，就不讓作者碰。」「排是Kernel負責，但是實現依然是作者。作者無法改變排的順序」）：

1. 一個 World 只准一張打上 Startup 記號的 schedule，名字由組裝的人取（Kernel 不內建名字，DI-KS 15 不動）。打第二張是啟動錯誤。所有 Plugin 與主程式把開局的 System 登記到這一張；Plugin 帶來的是 System，不是 schedule。
2. 這個 World 第一次執行別的 schedule 時，Kernel 在那張的第一個 System 之前先把 Startup 跑完，之後不再跑。（「別的 schedule」指哪些，見第 10 節第 1 件。）沒有登記 Startup 的 World 什麼都不發生。
3. 主程式可以在第一幀之前先明確呼叫一次；Kernel 之後看到已經跑過就不再自動跑。
4. System 裡巢狀跑 Startup：回 Err。
5. Startup 執行中不准跑任何別的 schedule：回 Err。物理預熱這類「開局之後先空跑幾步」由作者自己做一個 System（每幀那張裡的獨占 System，加一個旗標 Resource 記著做過了沒）；說明頁給寫法。
6. Startup 裡的 System 回報錯誤：觸發它的那一次執行回報錯誤，World 中斷，觸發的那張 schedule 一個 System 都不跑。
7. 不提供「Startup 跑完了沒」的查詢（作者不需要知道）。

**記下的假設（之後可能變更；改寫自你看過的那一句）**：Startup 由 Kernel 在這個 World 第一次執行別的 schedule 之前自動跑一次。原句「由外面的呼叫者或外層的獨占 System 來跑」作廢。「Plugin 怎麼知道那一張的名字」留到定外層骨架時一起解決。

**Bevy 的做法**（分支會話讀了 `bevy_app/src/main_schedule.rs`）：內建開局清單 PreStartup／Startup／PostStartup，最外層的 `Main` 第一次執行時先跑完開局清單；清單是 Resource，作者其實可以插入。我們和它不同的兩處：不內建名字；作者改不了位置。

**查過的**：gyo2 上打 Startup 記號的只有三處測試（`HookFlushTests.cpp:333`、`:715`、`ZoneRuleTests.cpp:192`），各自只登記一張，其中兩處明確呼叫一次（仍然合法）；沒有產品用它。

### 3.4 不交出指標：閉包

背景：以前同步點不會發生在一個 System 呼叫的中途。這一批之後，`auto* p = world.Get<T>(e); world.RunSchedule(...); 用 p` 這種正常的寫法就會用到懸空的指標。所以介面改成不交出指標。

- 讀寫都在閉包裡：`world.Read<T>(e, f)`、`world.Modify<T>(e, f)`；Resource 用同一種寫法，不帶實體。
- **閉包可以回傳值**（使用者：輸出參數「是非常老派的做法」，而且看不出那段程式會不會改到傳進去的變數）。編譯期擋掉「指回 World 裡面」的回傳型別；整份複本可以。擋哪些型別見第 10 節第 4 件。
- 擋不住的：藏在作者結構裡的指標、把位址寫進閉包捕獲的變數。這是刻意繞路，歸在既有的限界（LIM-3），說明頁講明。
- 對象不存在時不呼叫閉包，結果說明「沒有」。
- 閉包裡呼叫 `RunSchedule` 回 Err。閉包裡可以再開閉包。
- 變更的記號由 `Modify` 在呼叫當下設，一律記變更（f 沒有真的改值也算）。

---

## 4. 要改的程式

| # | 位置 | 現在 | 改成 |
|---|---|---|---|
| 1 | `Executor.cpp` 的 `Product::Run` 入口 | 斷言 World 是閒置 | 從外面呼叫照舊。巢狀的入口另外一個，只接受「目前正在執行的呼叫是獨占的 System」；hook 裡回 Err。正在執行中的 schedule 記成一個堆疊（放在成員裡，不用 static），重複就回 Err。名字不存在、Startup、extract 的 schedule，也在這個入口回 Err |
| 2 | 同檔：狀態的切換與 `RunGuard` | 跑完設回閒置 | 記深度；裡面那張跑完回到「執行中」，最外層跑完才回閒置。作者的 System 接住例外時也要還原（RAII） |
| 3 | 同檔：每個 System 跑完的成員檢查 | 實體有增減就斷言 | 進入巢狀前先比對一次（到這裡為止不能有增減——抓得到獨占 System 自己繞路改的）；回來後以跑完的狀態為新的基準，再往下比（第 5.1 節） |
| 4 | `World.cpp`：延後的佇列 | 整個 World 一個 | 一個容器，每一層執行用自己的那一段：進入巢狀時記下目前的長度，裡面那張的紀錄加在後面，它的同步點只套用自己的那一段並切掉；回來後外層的那一段原樣還在，用外層的位置、在外層的同步點套用。**中斷時整個清掉。** 規則 Z5 在同步點的重查，也看前面每一層留著的紀錄，加上執行堆疊上每張 schedule 的 hook 佇列（外層 (c) 區用指定的編號生成也算）；錯誤訊息指出那筆紀錄在哪一層、哪張 schedule |
| 5 | `Executor.cpp`、`World.cpp`：存取驗證 | 裡面那張開始時的比對會把外層 System 的寫入當成「World 介面以外的改動」 | 進入巢狀前先把到目前為止的改動算在外層 System 頭上並檢查（有違規就記進這次呼叫的錯誤清單，不進入巢狀），再重拍快照；回來後再重拍。「整個 World」的存取照舊算可以寫 (b) 區 `Shared` 的資料；另外加上列在宣告裡的單一寫者 Resource |
| 6 | `Params.hpp`：句柄的過期檢查 | 同步點之後句柄算過期 | `RunSchedule` 回來後更新這個 `ExclusiveWorld` 的記號；成功或失敗的返回路徑都更新 |
| 7 | 同檔：寫入用的 stamp | 進場時的 stamp | `RunSchedule` 回來後取一個新的 stamp（計數加一，不是沿用）。`Modify` 在呼叫當下用句柄現在的 stamp 記變更 |
| 8 | `Executor.cpp` 的 `Fail` 與步驟的迴圈 | 設成中斷並呼叫錯誤處理；步驟之間不看狀態 | 巢狀時設成中斷、留下錯誤，不呼叫處理函式；裡面的錯誤併進外層這次呼叫的清單；最外層統一呼叫一次。每個 System 回來後看 World 有沒有中斷，中斷就停；沒有留下錯誤（例外）時補一個。中斷的 World 再被要求跑：不執行，回傳留下的錯誤 |
| 9 | `detail/Context.hpp`、`Product.hpp` | System 拿不到執行器 | 呼叫的上下文帶執行器的指標；`ExclusiveWorld::RunSchedule(名字)`。註：T2 會把 `Product::Run` 改名，T5 會讓一個 `Product` 有多個 World，這個指標要指到「這個 World 的執行器」 |
| 10 | `Params.hpp`、`SystemTraits.hpp`、`World.cpp`、`Registry.cpp` | `ExclusiveWorld` 沒有 Resource 的介面，也不帶任何宣告 | 新的寫法 `ExclusiveWorld<Writes<…>>`；不帶宣告的 `ExclusiveWorld` 照舊可用（兩個名字怎麼並存實作時定，原則是既有的簽名不必改）。列出來的 Resource 用現有的「寫 Resource」這一種存取記在 System 上，所以「只有一個寫者」的啟動檢查（D-E2）、(c) 區的擋下（Z2）、列 `KernelSettings` 的擋下（D-E18）、執行緒親和的檢查、Registry 指紋都自動成立。另外加兩條啟動檢查：獨占的 System 不能同時是 ingress；hook 不能用帶宣告的寫法。執行期的規則照第 3.2 節 |
| 11 | `Params.hpp` | `Get` 回傳指標、`GetMut` 回傳 `Mut` | 換成 `Read<T>(實體, f)`、`Modify<T>(實體, f)`（Resource 不帶實體）。f 可以回傳值；回傳型別屬於第 10 節第 4 件那張清單的，用 `requires` 約束擋掉（測試用 `requires` 運算式確認，不靠建置失敗）。結果帶三種情形：有（連同 f 的回傳值）、沒有、錯誤；確切的型別實作時定。f 執行期間呼叫 `RunSchedule` 回 Err（句柄上記深度） |
| 12 | `Executor.cpp`、`World.cpp`、`Registry.cpp` | 執行器不知道 Startup 跑過沒有；Startup 的張數沒有限制 | World 記兩件事：Startup 跑過了沒、Startup 是不是正在執行中。從外面執行別的 schedule 時，還沒跑過就先把 Startup 跑完（它自己的同步點、hook、跑完時的清空都照常），出錯就不跑原本要跑的那張。主程式明確呼叫 Startup：還沒跑過就跑，並記成跑過。巢狀跑 Startup、Startup 執行中巢狀跑別的：回 Err。啟動檢查加一條：一個 World 有第二張 Startup 就報錯。**從外面呼叫的行為會變**（以前不呼叫就不會跑、重跑不會被擋） |
| 13 | `KernelError.hpp`、`Registry.cpp` 的 `ToString` | — | 新增錯誤碼（加在列舉的最後）：schedule 正在執行中、名字不存在、作者的 System 不能跑 Startup、Startup 執行中不能跑別的 schedule、extract 不能巢狀跑、hook 裡不能跑、閉包裡不能跑、沒列在宣告裡的 Resource（讀 `KernelSettings` 也用這一個）、執行緒親和不合的 Resource、巢狀執行被例外中止。啟動錯誤加一個：第二張 Startup |
| 14 | `Fingerprint.cpp` | 共用的版本 2、C 指紋的版本 4 | 各加一（3、5）；只改這兩個常數。守衛 J-18、J-20 的固定值跟著換，判定不變（第 10 節第 2 件） |
| 15 | 不改、但要寫測試固定下來的 | tick 由裡面那張推進；hook 的佇列每張 schedule 一個；裡面那張的執行不套用、也不丟掉外層 schedule 的 hook 佇列（hook 那一批交過來的）；事件每個 tick 修剪；巢狀的同步點期間，呼叫的上下文會還原成外層的 | 行為不變 |

測試：新檔 `tests/common/kernel/NestedRunTests.cpp`。既有測試要跟著介面改寫法的：守衛 J-45 的五個函式（`ExecutorTests.cpp:502-527`）；`ExclusiveWorld` 如果做成樣板，`SignatureTests.cpp`、`CoverageTests.cpp`、`ZoneRuleTests.cpp` 裡用到它的簽名也要動（判定都不變；實作時優先選不必動它們的做法）。

文件：[說明頁](system-contract-page.zh-Hant.md)加第 11 節列的內容、HANDOFF、dev_log；`docs/cross-component-rules.md` 查有沒有同樣的規則，沒有就登記（裡面那張回 Err 要往上傳；hook 不跑 schedule；裡面那張看不到外層還沒套用的變更）。

**作者的負擔（不改程式，但要寫明）**：獨占的 System 和同一個 stage 的每個 System 都算衝突。打開嚴格模式時，同一個 stage 的每個 System 都得和它寫明先後；(c) 區的 System 不論有沒有嚴格模式都要寫。

**不碰**：`World::ApplySync` 套用的順序、事件的讀法、`Scheduler.cpp`、漏斗工具、`.github/`、`build/`。`Fingerprint.cpp` 只動第 14 項的兩個常數；`Registry.cpp` 只動第 10、12 項的三條啟動檢查與第 13 項的 `ToString`。

**和別的分支的先後**：從併入 #234（hook 的清空）與 `v8-arch-gyo2-spawn-reject` 之後的 `gyo2` 開工。

---

## 5. 既有條文與守衛測試的變更（AGENTS §3、§14.4）

### 5.1 守衛 J-3：「System 執行期間實體不能增減」（放寬，要你核准）

1. **它守的性質**：一個 System 執行的期間，World 裡有哪些實體、哪些實體有哪些元件，不會變；變更只在同步點發生（DI-KS 9、規格 W-3）。所以 System 手上的查詢結果與指標在它執行期間一直有效。
2. **放寬的面**：獨占的 System 在執行中跑了一張 schedule，那張 schedule 的同步點改了成員——以前斷言失敗，之後通過。
3. **收緊的面**：無。
4. **放寬之後，原本的性質誰還守著**：
   - 一般的 System 不能跑 schedule，它們執行期間成員仍然不變，原本的檢查照舊。
   - 獨占的 System：進入巢狀之前比對一次、回來之後從新的基準再比。所以它自己繞過同步點改成員（不是經由跑 schedule），不論發生在跑 schedule 之前或之後，仍然抓得到。
   - 獨占的 System 手上的舊指標：介面不交出指標（已定），正常的寫法拿不到可以帶過去的指標。刻意把閉包裡的位址存到外面的，沒有東西守著，屬於既有的限界（LIM-3）。
   - System 經 World 的公開介面直接改成員，仍然由 `World::Access` 的斷言擋下（`World.cpp:143-144`），這條不動。
5. **根據與證據**：設計第 0 節；第一份報告 K0 表第 3 項（`Executor.cpp:216-217`、`WorldTests.cpp:197-215`）；檢查報告的 B1、NB5。

### 5.2 守衛 J-45：「`ExclusiveWorld` 寫不到單一寫者的資料」（放寬，要你核准）

1. **它守的性質**：`ExclusiveWorld` 只能寫 (b) 區、標 `Shared` 的資料；寫 (c) 區的、或沒標 `Shared`（單一寫者）的，在每一種建置都回 Err（DI-KS 2、規格 S-13）。所以「只有一個寫者」的資料不會被一個啟動檢查數不到的人寫。
2. **放寬的面**（你定的，`v8-arch-gyo2-exclusive-declared-writes`）：
   - 規則上：獨占的 System 寫單一寫者的 Resource，以前不准（S-13），之後列在宣告裡就可以。
   - 程式上：`ExclusiveWorld` 現在根本沒有 Resource 的介面，今天唯一走得到的是繞路（經 World 的公開介面寫）。繞路寫「列出來的、單一寫者的 R」，以前驗證的兩層都會報，之後 R 算已宣告，不報。
3. **收緊的面**：無。標 `Shared` 的 Resource 照舊可以寫，不用列（使用者 2026-11-15 改定，`v8-arch-gyo2-shared-means-no-owner`）。


4. **放寬之後，原本的性質誰還守著**：列出來的 Resource 算進寫者的計數，別人也寫它就在啟動時報「有第二個寫者」（D-E2，守衛 J-15）。也就是從「寫不到」變成「寫了會被數到」。元件的規則不變。(c) 區的仍然寫不到（Z2）。例外照舊：只掛在 Startup 的獨占 System，它列出來的 Resource 不計（Startup 的豁免）。
5. **根據與證據**：HANDOFF `v8-arch-gyo2-exclusive-declared-writes`；規格 `spec.zh-Hant.md:261`、`:336`、`:1094`；`target/kernel.zh-Hant.md:22`；檢查報告 `k0-declaration-r3-check-opus.md` 的 B1。

兩條的程序相同：現在請你同意方向。實作後我附上突變的結果與別的模型對這五點的驗證（用實際的測試跑），你核准了才合併。「同意方向」不算核准。

### 5.3 其他

「DI」＝Design Invariants 的變更，照 AGENTS §3 要你同意（第 10 節第 6 件）。

| 條文／守衛 | 變更 | 性質 |
|---|---|---|
| 概念稿 E5「System 執行中再跑 System 是 Programmer Error」 | 拿掉 | 跟著你定的 `v8-arch-gyo2-time-plan-two` |
| 規格 E.2 第 1 步「`Run` 斷言 World 是閒置」；規格 A 的狀態「執行中（哪張、第幾步）」 | 加上「或由獨占 System 發起」；狀態變成一疊 | 跟著上一條 |
| target `kernel` **DI-KS 1**、規格 S-10、S-13 | 加上「可以跑 schedule」；「讀全部」補上「`KernelSettings` 與親和不合的 Resource 除外」（規格 §讀法 `:21` 說它讀得到 `KernelSettings`「無害」，那一句要改）；加上「單一寫者的 Resource 列在宣告裡才能寫，列出來的算寫者」。S-10「視為寫全部 (b) 區 `Shared`」不變，另加列出來的。S-13 的理由句「寫者的規則不必列舉它」要改 | **DI** |
| target `kernel` **DI-KS 2**（`ExclusiveWorld` 對 (c) 區或單一寫者資料的寫入一律回 Err） | 改成：元件照舊；Resource 標 `Shared` 的照舊可以寫，單一寫者的列在宣告裡才能寫 | **DI**（有放寬，見 5.2） |
| target `kernel` **DI-KS 6**、規格 B.3、`Declarations.hpp` 的註解（「只有一個寫者」的計數；Startup 的 System 不算寫者，它在別的 schedule 之前跑一次） | 計數的對象加上「獨占的 System 列出來的 Resource」。Startup 的豁免不變；它的理由從靠外面只呼叫一次，變成 Kernel 自己跑、自己擋；條文加上第 3.3 節的規則與記下的假設。規格 `:261`「`ExclusiveWorld` 寫不到單一寫者的欄，所以不計」要改 | **DI** |
| 規格 D.2 的啟動檢查 | 加一條：一個 World 有第二張 Startup | 只收緊（現有的三處測試都只有一張） |
| target `kernel` **DI-KS 15**（Kernel 不內建 schedule 名、Host 決定什麼時候跑哪一張） | 名字照舊不內建。補一句例外：Startup 什麼時候跑由 Kernel 決定（Host 可以在第一幀之前先呼叫一次） | **DI**（文字要補） |
| target `kernel` **DI-KS 9**、規格 W-3、target `world` 第 41 行（Running 期間成員不變；變更只在宣告的同步點套用；結果不依記錄順序） | 「Running 期間成員不變」改成「一般的 System 執行期間成員不變；獨占的 System 只在它跑的 schedule 的同步點變」。加一句「記下的變更在記它的那張 schedule 的同步點、用那張的位置套用」。其餘不變 | **DI**（放寬的面就是 5.1） |
| 規格 **W-4**、A.4 與守衛 J-4 | 獨占 System 的句柄在巢狀回來後重新有效（第 6 項）。J-4 加巢狀的案例 | 放寬，實作後列五點請你核准 |
| **LIM-3**、規格 B.5 | 註明 `ExclusiveWorld` 沒有「呼叫內的舊指標」這個形狀；刻意把閉包裡的位址帶到外面的（存進捕獲的變數、藏在回傳的結構裡）仍在限界內 | 你同意過的限界的文字變更 |
| target `world` DI 3、規格 E.2「stamp 依執行計畫的順序推進」 | 文字和 hook 那一批寫成同一句：除了執行計畫的每一步，「跑完時清空的每一輪」與「巢狀回來之後」也各取一個新的 stamp；stamp 只往前走、執行歷史相同則序列相同 | **DI**（文字要補） |
| 規格 Z5（(b) 區的刪除碰上 (c) 區對同一個實體的插入要報錯） | 重查的範圍加上「外面每一層留著的紀錄與 hook 佇列」（第 4 項） | 只收緊 |
| 規格 E.2（步驟的迴圈） | 每個 System 回來後看 World 有沒有中斷，中斷就停（現在步驟之間不看狀態） | 行為變更（只在巢狀出錯時有差別） |
| 規格 E.5（例外與中斷的處置） | 外層的 System 接住裡面丟出的例外時，執行器補一個錯誤、呼叫一次錯誤處理的函式。是 DI-KS 2「記一次」的延伸 | **DI**（文字要補） |
| 規格 X-9（中斷的 World 再被要求跑是斷言） | 從外面照舊是斷言；巢狀時改成回傳留下的錯誤 | 行為變更 |
| 守衛 J-35（出錯只記一次） | 加巢狀的案例 | 只收緊 |
| 守衛 J-50（`KernelSettings` 不給 System 讀） | 加一案：執行期經 `ExclusiveWorld` 讀它回 Err | 只收緊 |
| 守衛 J-10（`ExclusiveWorld` 是唯一的參數） | 判定不變；加一案：帶宣告的寫法也必須是唯一的參數 | 只收緊 |
| 守衛 J-15（Startup 的寫者不算；`Shared` 的兩個寫者可以） | 判定不變；加一案：獨占的 System 列出來的單一寫者 Resource 有第二個寫者，報錯 | 只收緊 |
| 守衛 J-54（hook 那一批；其中兩案明確呼叫一次 Startup） | 不變：主程式明確呼叫一次仍然合法 | — |
| 執行計畫、Registry、C 指紋的固定值（J-18、J-20） | 版本號加一，固定值跟著換；判定不變。除了版本號，內容不變：不碰 `Scheduler.cpp`；不帶宣告的 `ExclusiveWorld` 存取種類不變；新的錯誤碼加在最後 | 跟著 `v8-arch-gyo2-hook-flush-not-in-plan-bump-versions` |
| 「同步點只在 stage 結尾與宣告的位置」（規格 D.2） | 不變 | — |
| 規格 `:1308`（編輯器檢視器用 `ExclusiveWorld` 的說明） | 讀寫的寫法跟著改 | 文字 |
| 規格 H.5 的 H-A1、H-9、DI-KS 12（回滾與重跑只由外面在閒置時呼叫） | **不變**，不在這一批 | — |

---

## 6. 判定（先宣告，結果出來之後不放寬）

**判定 1：行為。** `NestedRunTests.cpp` 的案例全部通過：

| 案例 | 驗什麼 |
|---|---|
| 跑 k 次 | 獨占 System 跑一張帶時鐘的 schedule 3 次：裡面的 System 各執行 3 次，tick 前進 3 |
| 回到外層 | 跑完後外層 schedule 後面的 System 照常執行；整個 `Run` 結束後 World 是閒置 |
| 裡面改了成員 | 裡面那張生成、刪除實體：不斷言；外層後面的 System 看得到 |
| 一般 System 直接改成員 | 照舊斷言（原有的 J-3 案例不改、照過） |
| 獨占 System 自己繞路改成員 | 在跑 schedule 之前改、之後改，各一案：都斷言 |
| 一般 System 繞路跑 schedule | 經全域指標呼叫 `Product::Run`：照舊斷言 |
| 外層留著的變更 | 同一個 stage 前面的 System 記了延後的變更：裡面那張看不到、不出錯；外層的同步點照外層的位置套用，結果和不跑巢狀時相同；排在獨占 System 後面的 System 記的變更也在同一個同步點套用 |
| 外層的插入碰上裡面的刪除 | (1) 外層 (b) 區的 System 記了往 E 的插入，裡面那張刪了 E：外層的同步點丟掉那一筆，計數加一，不出錯。(2) 外層 (c) 區的 System 記了往 E 的插入，裡面那張的 (b) 區 System 刪 E：裡面那張的同步點回報 Z5 的錯誤，E 沒有被刪，訊息指出那筆紀錄在外層的哪張 schedule。(3) 三層（A 跑 B、B 跑 C），紀錄在最外層：一樣回報。(4) 那筆插入是外層 schedule 的 (c) 區 hook 記的（還在 hook 的佇列裡）：一樣回報。(5) 外層 (c) 區用指定的編號生成 E：一樣回報 |
| 外層的 hook 佇列不被裡面那張動到 | 外層 schedule 的 hook 佇列裡有紀錄時跑裡面那張：裡面那張跑完，外層的那幾筆還在、沒有被套用，等外層自己跑完時才套用 |
| 外層刪除後裡面用同一個編號生成 | 外層記了刪除 E（E 還活著），裡面那張用指定的編號生成 E：固定實際的行為。`v8-arch-gyo2-spawn-reject` 合併之後的期待是「裡面那筆生成被丟掉並計數，外層的刪除照常刪掉舊的」；開工時照那一批合併後的行為寫定 |
| 存取驗證不誤報 | 驗證打開：獨占 System 在跑 schedule 的前後各寫一次 (b) 區 `Shared` 的元件與列出來的 Resource，不報錯 |
| 存取驗證照樣抓得到 | 驗證打開：(1) 裡面那張的 System 寫了沒宣告的資料，報錯並點名它；(2) 獨占 System 在進入巢狀前寫了不准寫的資料，記在它自己頭上，裡面那張沒有跑；(3) 獨占 System 經 World 的公開介面繞路寫沒列的單一寫者 Resource：在存取當下報（W-5）；(4) 經存下來的位址寫它：在 System 的邊界比對時報 |
| 句柄與 stamp | 裡面那張跑兩次；外層在兩次之間用 `Modify` 寫入，裡面那張第二次執行時帶 `Changed` 的 System 看得到。驗證打開時，裡面那張出錯後外層再用句柄，不會多報一個「句柄過期」 |
| 閉包 | f 回傳整份複本或數值：可以，拿得到值。f 回傳第 10 節第 4 件清單上的型別（每一種各一案）：不被接受（用 `requires` 運算式在測試裡確認）。實體沒有那個元件：不呼叫 f，結果說明沒有。閉包裡再開閉包讀另一個元件：可以。閉包裡跑 schedule：回 Err。介面上沒有 `Get`、`GetMut` 這兩個名字 |
| 出錯記一次 | 裡面那張的 System 回 Err：錯誤處理只被呼叫一次，錯誤裡有裡面的 System、裡面的 schedule、外層的 System；World 中斷 |
| 外層回別的 Err | 裡面的錯誤仍然在清單裡 |
| 吞掉錯誤 | 外層 System 收到 Err 卻回 Ok：World 仍然中斷，錯誤仍然報出一次。裡面出錯、跑自己、名字不存在、跑 Startup、Startup 執行中跑別的、extract、hook 裡跑、閉包裡跑，各一案 |
| 中斷後再跑 | 迴圈裡第二次呼叫 `RunSchedule`：不執行、回傳留下的錯誤、錯誤處理沒有多被呼叫 |
| 裡面丟出例外 | 外層接住後回 Ok：深度與堆疊已還原；外層後面的步驟沒有跑；錯誤處理呼叫一次，拿到「被例外中止」 |
| 跑自己 | 跑正在執行中的 schedule：回 Err。A 跑 B、B 再跑 A 也是 |
| 兩層 | A 跑 B、B 裡的獨占 System 跑 C：順序與 tick 都對 |
| hook | 裡面那張的同步點觸發 hook，照常執行；它記的變更在裡面那張跑完時清空；hook 裡呼叫 `RunSchedule`：回 Err |
| Resource：讀 | 讀任何 Resource；讀 `KernelSettings` 回 Err；讀執行緒親和不合的 Resource 回 Err、World 中斷、訊息裡有 System 與 Resource 的名字；Resource 不存在：不呼叫 f |
| Resource：寫 | 列在宣告裡的單一寫者 Resource 寫得進去；標 `Shared` 的不用列也寫得進去（帶不帶宣告的 `ExclusiveWorld` 都可以）；沒列的單一寫者 Resource 回 Err |
| Resource：寫者的計數 | 獨占的 System 列了一個單一寫者的 Resource，另一個 System 也用 `ResMut` 寫它：啟動時報 D-E2，兩個 System 都被點名 |
| Resource：宣告的檢查 | 列了 (c) 區的 Resource：啟動時報 Z2。列了 `KernelSettings`：報 D-E18。獨占的 System 設了 ingress 的旗標並掛在 ingress 的 stage：報新的那一條（指定規則代號）；沒設旗標、只是掛在那個 stage 的：通過。hook 用帶宣告的寫法：報新的那一條。帶宣告的 `ExclusiveWorld` 加上第二個參數：編譯不過 |
| Startup：自動跑 | 登記一張 Startup 與一張帶時鐘的 schedule，主程式只跑後者兩次：Startup 的 System 執行一次，而且在後者第一個 System 之前；第二次不再跑。沒有登記 Startup：照常執行 |
| Startup：主程式先呼叫 | 主程式先明確跑一次 Startup，再跑別的：Startup 的 System 總共只執行一次 |
| Startup：被拒絕 | (1) 獨占的 System 巢狀跑 Startup：回 Err（Startup 跑過之後一案；用主程式先跑別的 schedule 觸發的那一次裡再呼叫一案）。(2) Startup 裡的獨占 System 巢狀跑別的 schedule：回 Err，訊息裡有那個 System 與那張 schedule 的名字。(3) 登記兩張 Startup：啟動時報錯（指定規則代號）。(4) 主程式的呼叫時機不對：照第 10 節第 3 件定的反應 |
| Startup：出錯 | Startup 裡的 System 回 Err：觸發它的那次 `Run` 回 Err、World 中斷、原本要跑的那張一個 System 都沒有執行 |
| Startup：哪些 schedule 會觸發 | 照第 10 節第 1 件定的範圍，各一案 |
| 指紋 | 有獨占 System（不帶宣告）的 schedule：執行計畫與三種指紋，除了版本號以外的內容和加這一批之前相同（把版本號換回舊值重算，和舊的固定值相同） |

**判定 2：檢出力。** 突變清單在實作前由實作者（Opus）定案，交別的模型（Fable）檢查——找出「把實作改壞而測試仍然全過」的地方並補上。至少包含：
- 執行器：跑完設回閒置；成員檢查整個拿掉；跑過 schedule 的 System 完全不查成員；進入巢狀前只重拍、不先比對；任何呼叫都可以進入巢狀；例外之後深度不還原；跑自己不擋；跑自己只查最內層；hook 裡可以跑；閉包裡可以跑；extract 也放行；名字不存在改成斷言。
- 每層的紀錄：外層的紀錄在裡面那張的同步點被套用；回來後外層的那一段不見了；Z5 的重查不看外層的紀錄；只看最近一層；不看外層的 hook 佇列；不看外層用指定編號的生成；裡面那張套用了外層 schedule 的 hook 佇列；裡面那張丟掉了外層 schedule 的 hook 佇列。
- 驗證、句柄、stamp：巢狀前不重拍快照；巢狀後不更新句柄；只在成功時更新句柄；巢狀後不換 stamp。
- 出錯：錯誤處理呼叫兩次；吞掉錯誤後 World 恢復；外層回別的 Err 時丟掉裡面那個；中斷的 World 照跑。
- 閉包：清單上的每一種型別各一個「不擋了」的突變。
- Resource：沒列的單一寫者 Resource 也給寫；第一層驗證把沒列的單一寫者 Resource 當成已宣告；第二層驗證把它當成已宣告；`Shared` 的 Resource 反而不給寫；親和不合的 Resource 也給讀；列出來的不算進寫者的計數；獨占的 System 可以是 ingress；`KernelSettings` 也給讀。
- Startup：不自動跑；自動跑了兩次；自動跑排在觸發的那張的第一個 System 之後；主程式先呼叫過仍然再自動跑一次；作者的 System 可以跑 Startup；Startup 執行中可以跑別的；兩張 Startup 不報錯；Startup 出錯後仍然跑原本要跑的那張。

只有預期的那個案例失敗才算抓到（啟動檢查的案例要指定規則代號）；建置失敗不算。

**判定 3：沒有回歸。** 本機完整 ctest 全過；三平台 L1（linux-x64、windows-x64、macos-arm64）全過。既有測試除了第 5 節列的，一個判定都不改。

**判定 4：守衛測試的判定變更**（5.1 的 J-3、5.2 的 J-45、5.3 的 W-4／J-4）各有五點、突變、別的模型的驗證，並經你核准。

---

## 7. 誰做、用什麼檔位

這一批是上段（動到 Design Invariants、公開介面、Kernel 的執行規則）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作 | Opus／high；執行器的狀態、出錯的處置、存取驗證的重拍、每層的紀錄四處用 xhigh | 這四處改的是順序與狀態，容易錯而且不容易發現；其餘是介面與測試 |
| 審查 | Fable（主會話） | 和實作者不同模型 |
| 突變清單的對抗檢查、第 5 節五點的驗證 | Fable／high | 和定清單的人（Opus）不同模型 |

時間的估計：實作與測試 8 到 12 小時（agent 的時間），審查與對抗檢查 2 小時左右。超過估計的 1.5 倍，或同一個問題換了兩次做法還沒結論，就停下來回報（AGENTS §14）。

---

## 8. Architecture Delta（和設計第 8 節的差分）

設計第 8 節已經列了七項。這一批的差分：

- **邊界**：Kernel 的執行從「一次一張 schedule」變成「可以巢狀」；`ExclusiveWorld` 的能力多了「跑 schedule」與 Resource 的讀寫（寫要宣告），讀寫的介面換形狀；Kernel 開始自己跑 Startup。
- **依賴方向**：不變。
- **Ownership**：「什麼時候跑哪一張 schedule」原本完全屬於 Kernel 外面的呼叫者；之後 World 裡的獨占 System 也能決定。「Startup 什麼時候跑、只跑一次」原本屬於外面的呼叫者，之後由 Kernel 持有（外面只剩「可以提前呼叫一次」）。
- **為什麼不能更小**：比它小的做法有兩個（登記式、由執行計畫重複執行），你已經選了開放給所有作者的這一個。

**Design Invariants**（實作時寫進 Kernel 的文件，附守衛）：

1. 一張 schedule 同一時間只有一份在執行。守衛：「跑自己」。
2. 巢狀執行的錯誤只報一次，而且 World 一定中斷；被拒絕的呼叫也一樣。守衛：「出錯記一次」「吞掉錯誤」「中斷後再跑」「裡面丟出例外」。
3. 一般的 System 執行期間成員不變；獨占的 System 只有在它跑的 schedule 裡成員才會變。守衛：J-3 原有的案例、「獨占 System 自己繞路改成員」。
4. 記下的延後變更，只在記它的那張 schedule 的同步點、用那張的位置套用；巢狀不會讓它在別張的同步點被套用，hook 的佇列也一樣；中斷時每一層的都丟掉（這一句是實作的約定：中斷的 World 不會再執行，丟不丟從外面觀察不到，所以沒有守衛）。守衛：「外層留著的變更」「外層的 hook 佇列不被裡面那張動到」。
5. 只有獨占的 System 能進入巢狀；hook、run 條件、一般的 System 不能。守衛：「一般 System 繞路跑 schedule」「hook」。
6. `ExclusiveWorld` 不交出指回 World 裡面的東西：元件與 Resource 都只在閉包裡存取；閉包的回傳型別不能是清單上的那幾種；閉包裡不能跑 schedule。藏在作者結構裡的指標、寫進捕獲變數的位址攔不到，屬於既有的限界（LIM-3），說明頁要給「需要的值在閉包裡取出來回傳」的寫法。守衛：「閉包」。C++ 沒辦法一般性地檢查「介面上沒有任何回傳指標的函式」，其餘靠審查。
7. 一個 World 至多一張 Startup。它由 Kernel 在這個 World 第一次執行別的 schedule 之前跑完一次，之後不再跑；作者的 System 不能跑它，也改不了它的位置；它執行中不能跑別的 schedule。（記下的假設見第 3.3 節。）守衛：五個 Startup 的案例。
8. 獨占的 System 寫單一寫者的 Resource 一定有宣告：列出來的算寫者，沒列的寫不進去，驗證的兩層也不把它當成已宣告。標 `Shared` 的資料不在這條裡（「要保證誰寫，別開 shared」）。守衛：「Resource：寫」「Resource：寫者的計數」「存取驗證照樣抓得到 (3)(4)」。

**Replacement Criteria**：實作時發現巢狀執行另外需要的特例，超出這份宣告第 4 節的清單，累計到三個；或巢狀執行的缺陷在合併後於兩個不同的地方各出現一次。碰到就停下來，重新比較登記式與由執行計畫重複執行兩個做法。

---

## 9. 範圍的限界

- 這份宣告的主體是照讀程式寫的。實際跑過的只有試做涵蓋的部分：每層的紀錄的兩種存法、Z5 看外層、外層刪除與裡面生成同一個編號的幾種做法（都沒有突變、沒有換模型檢查；報告在主 checkout `build/evidence/gyo2-nested-queue-spike/`、`gyo2-stamp-order-spike/`）。Startup 由 Kernel 跑、閉包回傳值的型別檢查，沒有試做。
- Bevy 的做法：巢狀執行那一部分只根據偵察報告，其中一部分是偵察者讀碼的推論；Startup 那一部分分支會話讀了原始碼。
- **巢狀的組成不在 C 指紋裡。** C 指紋只涵蓋有 (c) System 的 schedule。「哪個 (b) 區的獨占 System、跑哪一張 (c) 的 schedule、跑幾次、它相對 (c) 區 System 排在哪裡」不在裡面。只在 Client 多跑一次固定步，規格 H-4 抓不到。這一批不處理，留給 PvP 移植時定。
- 帶宣告的寫法與不帶宣告的 `ExclusiveWorld` 兩個名字怎麼並存、`Read`／`Modify` 結果的確切型別、閉包同時讀寫多個元件的寫法，要到實作時才定。
- 回滾、讀檔、Inspector、復原都還沒設計。這個入口夠不夠它們用，沒有分析。已知的兩點：「Startup 跑過了沒」這個旗標不在回滾的快照裡；讀檔（用指定的編號放回去）設計時要把「外層刪除、裡面用同一個編號生成」列進檢查。
- 載入的時間算在第一幀；載入進度畫面是另一個題目，這一批不處理。
- 效能沒有量。

---

## 10. 還要你判的六件

前四件是我照你的決定補的細節，你沒有判過；每一件的建議都只是我的。後兩件是核准。

### 第 1 件：開局在「第一次執行哪些 schedule」之前跑

例子：射擊遊戲登記了開局（載入地圖）、每幀那張（帶時鐘，會推進 tick），還有一張同一個時鐘域但不推進 tick 的（例如專門整理畫面資料的一張）。主程式某一次先跑了不推進 tick 的那一張。

| | 只看帶時鐘的那幾張 | 任何一張不是 Startup 的 schedule |
|---|---|---|
| 上面的例子 | 那一張的 System 先執行，看到的是還沒載入地圖的 World；之後跑每幀那張時才跑開局 | 那一張執行之前就先跑開局 |
| 開局「不算寫者」的理由（在別的寫者之前） | 這個情形靠主程式自律 | Kernel 守住 |
| 開局會不會在巢狀裡被觸發 | 會：外層是不推進 tick 的 schedule、裡面的獨占 System 跑帶時鐘的那張時，開局在外層跑到一半時才跑 | 不會：開局一定從最外層、在第一個 System 之前跑 |
| 系譜 | 現有的註解與規格 B.3 的字面（「在帶時鐘的 schedule 之前」）；分支會話轉達時也是這個字面 | 新導出的；Bevy 的做法近似（最外層的 `Main` 第一次執行時先跑開局清單） |
| 需要它的 Feature | 固定步用得到的兩者都滿足；沒有已點名的 Feature 需要「先跑別的、再跑開局」 | 同左 |

我的建議：任何一張不是 Startup 的 schedule。它讓「開局一定最先」沒有例外，巢狀時怎麼算的問題也消失。

### 第 2 件：自動跑開局不寫進執行計畫，版本號加一

例子：Match 和 Client 連線時互相比對指紋，確認兩邊的執行方式相同。

照你在 hook 那一批定的慣例（`v8-arch-gyo2-hook-flush-not-in-plan-bump-versions`，「至少能看到。」）：別張 schedule 的執行計畫裡不會多一步「先跑開局」；把共用的指紋版本從 2 加到 3、C 指紋的從 4 加到 5，一次涵蓋這一批全部的行為變更（巢狀、自動跑開局）。守衛 J-18、J-20 的固定值跟著換，判定不變。

另一個做法是把「還沒跑過就先跑開局」寫成每張計畫的第一步；計畫的內容會變得看執行期的狀態，沒有先例。我的建議：照慣例。

### 第 3 件：主程式呼叫開局的時機不對時怎麼反應

指的是：主程式明確呼叫了第二次，或 Kernel 已經自動跑過之後才呼叫。只有主程式自己的程式碼走得到這裡；System 走不到（System 裡呼叫一律回 Err），外面送進來的資料也碰不到。

| | 斷言（程式終止） | 回報錯誤 | 當作已經跑過，什麼都不做 |
|---|---|---|---|
| 行為 | 當場終止，指出位置 | `Run` 回 Err，World 中斷 | 回 Ok |
| 系譜 | 從外面呼叫的其他錯（名字不存在、World 不是閒置）現在都是斷言 | 和 System 裡被拒絕的呼叫同一種 | 新導出的 |
| 代價 | 無 | 主程式要多處理一種錯誤 | 主程式寫錯（例如以為開局會再載入一次）時沒有任何訊息 |

我的建議：斷言。

### 第 4 件：閉包不准回傳哪些型別

```cpp
int hp   = world.Read<Health>(player, [](const Health& h) { return h.value; });   // 可以：數值
Health c = world.Read<Health>(player, [](const Health& h) { return h; });         // 可以：整份複本
auto* p  = world.Read<Health>(player, [](const Health& h) { return &h; });        // 編譯不過：指標
```
（示意；結果還帶「有沒有這個元件」，確切的型別實作時定。）

你定的是「指標、參考、標準庫的檢視型別」，完整的清單沒有定。我擬的清單：

- 指標；參考；`Mut`；`std::reference_wrapper`。
- 標準庫的檢視：用標準的概念 `std::ranges::view` 判斷，涵蓋 `std::string_view`、`std::span` 與 ranges 的各種 view。
- 迭代器（例如 `return h.items.begin();`）：用標準的概念 `std::input_or_output_iterator` 判斷。

擋不住、在說明頁講明的（LIM-3）：包在 `std::optional`、`std::pair`、`std::tuple` 或作者自己的結構裡的指標；捕獲了參考的 lambda。

另一個做法是連 `optional`、`pair`、`tuple` 裡面一層也檢查；多一些樣板，作者的結構仍然擋不住。我的建議：只擋直接是清單上的型別。

### 第 5 件：核准兩條守衛放寬的方向

第 5.1 節（J-3：獨占的 System 跑的 schedule 可以改成員）與第 5.2 節（J-45：單一寫者的 Resource 列在宣告裡就可以寫）。現在請你同意方向；實作後我附上突變的結果與別的模型對五點的驗證，你核准了才合併。

### 第 6 件：核准要動的 Design Invariants

第 5.3 節標「DI」的七列，與第 8 節要新增的八條。

### 修訂 5 第 10 節二十三條的結果（紀錄）

| 條 | 結果 |
|---|---|
| 1、18 | 已定：Z5 的重查看外面每一層留著的紀錄與 hook 佇列 |
| 2、19 | 照做：中斷時每一層的紀錄都丟；不另加測試用的介面 |
| 3、4、7 | 已定：只有單一寫者的 Resource 要列；元件照舊；驗證的兩層跟著 |
| 5、6 | 照修訂 5 的文字：只能讀寫值；獨占的 System 不能是 ingress、hook 不能用帶宣告的寫法（逐條過的時候沒有改動） |
| 8 | 改定：開局由 Kernel 跑（原本的「執行完」問題消失） |
| 9 | 已定：開局執行中不准跑任何別的 schedule；不開「開局之後」 |
| 10 | 改定：一個 World 只准一張開局 |
| 11 | 刪除：不需要查詢 |
| 12 | 已定：閉包，不採複本 |
| 13 | 改定：閉包可以回傳值 |
| 14 | 已定：Resource 用同一種寫法 |
| 15 | 已定：不拆 |
| 16 | 已定：Engine 不加機制；往活著的編號生成另開一批 |
| 17 | 已定：回 Err |
| 20 | 已完成（#234） |
| 21 | 跟著慣例：版本號加一（本節第 2 件請你確認） |
| 22 | stamp 的條文和 hook 那一批寫成同一句（第 5.3 節） |
| 23 | 列進第 4 節第 15 項與判定的案例、突變 |

---

## 11. 說明頁要寫的（這一批實作時寫進[說明頁](system-contract-page.zh-Hant.md)）


### 說明頁要加的三張表（已定要寫；這一批實作時寫進[說明頁](system-contract-page.zh-Hant.md)）

用射擊遊戲當例子：每一幀依序「讀輸入 → 跑固定步（0 到數步）→ 算畫面」。

**每次都一樣的**

| 情形 | 結果 |
|---|---|
| 「讀輸入」排在跑固定步的前一個 stage，它記下「生成子彈」 | 下一個固定步就有子彈 |
| 固定步裡面自己記的變更 | 同一個固定步的下一個 stage 就看得到 |
| 排在跑固定步後面的 System 記的變更 | 下一幀的固定步看得到 |

**不保證每次一樣的，與避開的排法**

| 情形 | 會怎樣 | 怎麼排可以避開 | 代價 |
|---|---|---|---|
| 「讀輸入」和跑固定步放在同一個 stage、排在它前面 | 這一幀跑了幾步，子彈就晚幾步出現 | 跑固定步的 System 自己佔一個 stage，或在它前面宣告一個同步點。Engine 自己提供的那個 System 照這樣放 | 不用改 Kernel；作者要照這個排法 |
| 每幀那張生成的東西觸發的 hook 再記的變更 | 晚幾個固定步生效不固定；上一列的排法救不了 | 沒有不花成本的排法。要每次一樣，改由固定步裡的 System 來做這件事 | — |
| 一幀對應幾個固定步 | 取決於真實經過的時間 | 輸入帶著「在第幾個固定步生效」交給固定步（像網路遊戲的輸入緩衝） | 另一套機制，Engine 這一批不提供 |

**每次都一樣、但要先知道的規則**

和跑固定步的 System 同一個 stage、排在它前面的 System 記的變更，會在這一幀所有固定步之後才套用。所以：外層拿掉 C、裡面加上 C，最後沒有 C；外層寫 1、裡面寫 2，最後是 1；外層生成的實體，這一幀的固定步裡還不存在。照上面第一列的排法就不會碰到。

### 說明頁另外要加的兩段（第 16 條；分支會話擬的，使用者看過、沒有逐字確認）

用同一個例子：玩家離開又用同一個編號加入。

- **重入的標準寫法**：處理加入的 System 不管舊的角色在不在，先記刪除那個編號、再用同一個編號生成，兩筆配成一對。兩筆在同一個同步點，固定的順序是先刪再生，所以舊的被刪、新的留下；寫在固定步裡也一樣。只寫生成而舊的還在：那一筆生成會被機制丟掉並留下紀錄（`v8-arch-gyo2-spawn-reject`）。
- **要避開的排法**：外層（每幀那張）記了刪除某個編號，同一幀的固定步裡不要用同一個編號生成——外層那筆刪除在固定步之後才套用，新的會被它刪掉。把處理離開與處理加入放在同一張 schedule 就不會碰到。

### 開局與閉包（修訂 6 新增）

- **開局**：開局要做的事寫成 System，登記到打了 Startup 記號的那一張；什麼時候跑由 Kernel 決定（第一次執行別的 schedule 之前），作者與 Plugin 都改不了。開局裡不能跑別的 schedule。
- **開局之後要先空跑幾步（例如物理預熱）**：自己寫一個 System——每幀那張裡的獨占 System，讀一個旗標 Resource，還沒做過就跑幾次固定步、把旗標設起來。
- **閉包**：需要的值在閉包裡取出來回傳；不要回傳指標、參考、檢視、迭代器（編譯不過），也不要把它們藏在結構裡或存到外面的變數（編譯會過，但 `RunSchedule` 之後就是懸空的）。

# 事前宣告：獨占 World 的 System 可以跑一張 schedule（`v8-arch-gyo2-nested-schedule`）

更新：2026-11-15（修訂 1）。Owner：架構（GYO2 重建會話）。**狀態：草稿，待使用者核准。核准之前不動工。**

經過：
- 初稿（d5ac969e）送了別模型的檢查（Opus／xhigh，唯讀）：blocker 3、should 8、nit 11。報告在主 checkout `build/evidence/gyo2-time-v2/k0-declaration-check-opus.md`。
- 主要的錯：(1) 我寫「獨占的 System 手上沒有會失效的東西」，不對——它在跑 schedule 之前拿到的指標，之後會懸空；(2) 決定甲的表不公平：Bevy 的系譜引錯、另一邊的代價寫重、我建議的那邊漏列了四項代價（其中兩項碰到決定性與 PvP 兩端相容）；(3) 要你同意的條文漏了五條。
- 這一份照報告改寫。我對過程式的：指標與 stamp（`Params.hpp:107-128`、`:724-739`，`Storage.hpp:113-138`）、同一窗裡移除先於插入（`World.cpp:567-629`）、C 指紋含同步點（`Fingerprint.cpp:340-364`）、(b) 區的 hook 可以持有 `ExclusiveWorld`（`Registry.cpp:1093-1103`）、hook 的佇列每張 schedule 一個（`World.cpp:450-452`、`:690-698`）、Startup 不算寫者（`Registry.cpp:1347-1356`）。其餘照報告寫，沒有逐條重讀。
- **改寫後的選項表（第 3 節）還沒有再送檢查。**

根據：[時間與 runner 的設計](rebuild-time-design.zh-Hant.md)第 0 節；[HANDOFF](HANDOFF.md) 的 `v8-arch-gyo2-fixed-step-in-world`（固定步由 World 裡的 System 驅動）與 `v8-arch-gyo2-time-plan-two`（獨占 World 的 System 可以跑任何一張 schedule，開放給所有作者；理由：「固定步這個我不想限制作者的遊戲邏輯。關鍵不開放給一般作者，我怕會限制Engine能力」）。

事實的出處：上一輪的檢查報告 `time-design-v2r3-b-check-opus.md`（現有程式在巢狀執行時會出錯的十四處）；Bevy 的做法 `scout-c-bevy-eight-items.md` 第八節；這一輪的檢查報告（上面那份）。都在主 checkout `build/evidence/gyo2-time-v2/`。

---

## 1. 這一批做什麼

讓持有 `ExclusiveWorld` 參數的 System 可以在執行中跑另一張 schedule，跑幾次由它自己決定。

```cpp
// 例：驅動固定步的 System（它本身是下一批的事，這裡只是示意）
SystemResult DriveFixedSteps(ExclusiveWorld world) {
    for (int i = 0; i < steps; ++i) {
        if (auto ran = world.RunSchedule("FixedUpdate"); !ran) return Engine::Base::Err(std::move(ran).error());
    }
    return Ok();
}
```

**需要它的 Feature 與 Product**（AGENTS §2）：固定步由 World 裡的 System 驅動（①PvP、②單機，已定）。這一批只為這一件事做。

**為什麼是基盤**：它改的是 Kernel「一次只有一張 schedule 在跑」這條規則。同步點、延後的變更、存取驗證、出錯的處置都建立在這條規則上，所有 System 都跑在它上面。

**這一批不做的**：時間的 Resource、通用的時間、驅動固定步的那個 System 本身、runner、事件的游標、多個 World。它們是後面的批次（設計第 11 節）。回滾的重跑仍然只由外面在 World 閒置時呼叫（DI-KS 12 不變）。

---

## 2. 作者看到的行為

1. **誰能跑**：以 `ExclusiveWorld` 為參數的 System（既有規則：它是那個 System 唯一的參數，只能是 (b) 區）。
   - run 條件不能：它不能持有 `ExclusiveWorld`（編譯期就擋）。
   - hook 不能：(b) 區的 hook 現在可以持有 `ExclusiveWorld`，所以這一批要加檢查——hook 裡呼叫 `RunSchedule` 回 Err。理由：hook 是在同步點套用到一半時被呼叫的，這時再跑一張 schedule 會改到正在走的資料。
   - 一般的 System 繞路（例如經全域指標）呼叫 `Product::Run`：照舊斷言。
2. **能跑哪一張**：見第 3 節決定丙。巢狀可以多層（A 跑 B，B 裡的獨占 System 再跑 C）。
3. **不能跑正在執行中的那一張**（自己，或外面任何一層的那一張）。回 Err。Bevy 也不行。
4. **名字不存在**：回 Err（Bevy 的 `try_run_schedule` 相同）。從外面呼叫 `Product::Run` 給不存在的名字，照舊斷言。
5. **裡面那張照常完整執行**：它的 System、同步點、hook、tick 的推進（它是帶時鐘的 schedule 時）、事件的修剪。和從外面跑它沒有差別。
6. **跑完之後外層的 System 繼續執行**：手上的 `ExclusiveWorld` 照常可用；之後的寫入用新取的變更記號（stamp），所以變更偵測看得到。**但它在 `RunSchedule` 之前拿到的指標與 `Mut` 不能再用**——見第 3 節決定丁。
7. **裡面那張出錯**：
   - World 立刻中斷，`RunSchedule` 回 Err；外層的 System 應該把它往上回傳。
   - 錯誤處理的函式只被呼叫一次（最外層），拿到的錯誤包含裡面那一步與外層的 System。外層回傳的是另一個 Err 時，裡面那個會併進外層這次呼叫的錯誤清單，不會不見。
   - 外層吞掉錯誤回 Ok：World 仍然是中斷的，錯誤照樣報出一次。
   - 中斷之後再呼叫 `RunSchedule`（例如寫成迴圈的）：不執行，直接回傳留下的那個錯誤，不再呼叫錯誤處理。
   - 出錯之後外層繼續做的寫入：經 `ExclusiveWorld` 的照樣會寫進去（不擋）；World 已經中斷，之後只供診斷用。
8. **延後的變更**：見第 3 節決定甲。

和 Bevy 的差別：Bevy 的獨占 system 拿 `&mut World`，呼叫 `run_schedule(&mut self)` 的那一刻，編譯器不准手上還留著從 World 借出來的參考——第 6 點的問題在 Bevy 是編譯錯誤。C++ 沒有這個檢查。另外 Bevy 跑到一半 panic 時那張 schedule 不會被放回；第 7 點是 GYO 多做的。

---

## 3. 要你定的四件事

每個選項寫了系譜（從哪來）、代價、需要它的 Feature。我的建議寫在各表後面，理由只有我自己的，不是你說過的。

### 決定甲：排在獨占 System 前面的 System 記下的變更，什麼時候套用

現況：同步點只在每個 stage 結束時，以及 schedule 自己宣告的位置。所以同一個 stage 裡，排在獨占 System 前面的 System 生成的實體、加上的元件，獨占 System 執行時還沒套用。整個 World 只有一個延後的佇列，如果這時候進入巢狀，外層還沒套用的紀錄會在裡面那張的同步點、用裡面的位置被套用——這不對，四個選項是四種處理法。

| | 甲一：獨占 System 前面自動加同步點 | 甲二：不加；佇列不是空的就回 Err | 甲三：不加；每一層各一個佇列 | 甲四：呼叫 `RunSchedule` 時當場套用 |
|---|---|---|---|---|
| 做法 | 排程時在每個獨占 System 前面插一個同步點（前一步已經是同步點時不插） | `RunSchedule` 進入時檢查，佇列有東西就回 Err，訊息指出要把生成者移到前一個 stage，或在中間宣告一個同步點（schedule 本來就可以宣告） | 外層的紀錄留著，等外層自己的同步點；裡面那張用自己的佇列 | 進入前先把外層的紀錄套用掉 |
| 裡面那張看不看得到外層剛記的變更 | 看得到 | 不會發生（作者被要求先排好） | 看不到 | 看得到 |
| 作者要知道什麼 | 不必知道同步點；但要知道「加一個獨占 System 會改變它前後變更的套用時機」 | 要知道獨占 System 前面要有同步點；錯了有明確的錯誤 | 要知道裡面那張看不到同一個 stage 前面記的變更；錯了沒有訊息 | 不必知道 |
| 系譜 | 比 Bevy 強。Bevy 只在「兩者之間有作者寫的先後」而且「前一個確實有延後的寫入」時才插（`auto_insert_apply_deferred.rs:146-149`）；沒寫先後的一對，Bevy 不保證 | 上一輪檢查報告提的做法；作者自己安排同步點，近似 Bevy 對沒寫先後的那一對的態度 | 現在的程式照原樣延伸 | 新導出的 |
| 改到什麼 | 執行計畫多一步。**它作用在所有的獨占 System 上，包括從不跑 schedule 的**（例如檢視器） | 不改執行計畫 | 不改執行計畫；Kernel 多一個每層一份的結構 | 不改執行計畫；但套用的時機隨「這一輪有沒有跑」而變 |
| 對決定性與 PvP 的影響 | **有**（下面三點） | 無 | 無 | **有**：套用點隨執行期變動，(c) 區的結果會跟著變 |
| 需要它的 Feature | 作者把獨占 System 和生成者放同一個 stage 的情形。固定步本身不需要：驅動的 System 自己佔一個 stage 時，前面本來就是同步點 | 同左 | 同左 | 同左 |

甲一對決定性的三點（都是把「一個窗切成兩個」造成的）：
- **同一輪的結果會變。** 例：前面的 System 加元件 C，後面的 System 移除 C。現在兩筆在同一個窗，規則是先移除後插入，結果有 C；切成兩個窗之後先插入後移除，結果沒有 C。這牴觸 DI-KS 9「結果不依 System 之間的記錄順序」。
- **hook 提早觸發**，hook 寫的事件當個 tick 就被後面的讀者讀到（原本是下一個 tick）。
- **C 指紋會變。** 指紋包含「有 (c) System 的 schedule 的每一個同步點」。只在 Client 加一個除錯用的獨占 System（不在 stage 開頭），Client 與 Match 的 C 指紋就不同，違反規格 H-4。會在連線時被指紋擋下，不是靜默出錯；但等於多了一條限制。

另外，不論選哪個，「獨占 System 看到的一定是全部套用完的 World」都不成立：同步點觸發的 hook 記下的變更，要到這張 schedule 的下一個同步點才套用。

**我的建議：甲二。** 理由：(1) 已點名的 Feature（固定步）不需要自動加同步點；(2) 甲二之後要改成甲一，原本會回 Err 的寫法變成可以跑，不會弄壞任何人，反過來從甲一退回就會；(3) 不碰執行計畫與指紋。代價是自己寫獨占 System 來跑 schedule 的作者要多懂一件事，錯了會拿到一個指名道姓的錯誤。如果你比較看重「作者不必懂同步點」，那是甲一，代價是上面三點。

### 決定乙：獨占 World 的 System 怎麼讀寫 Resource

現況：`ExclusiveWorld` 只有元件的介面（讀任何元件；寫只准 (b) 區、標成 `Shared` 的），沒有 Resource 的介面；而且它必須是唯一的參數，不能另外宣告 `Res`、`ResMut`。驅動固定步的 System 一定要讀寫 Resource（還差多少時間、這一輪跑幾步），照現況寫不出來。

| | 乙一：`ExclusiveWorld` 加 Resource 的讀與寫 | 乙二：放寬「唯一的參數」 | 乙三：`ExclusiveWorld` 帶寫入的宣告 |
|---|---|---|---|
| 做法 | 讀任何 Resource；寫只准 (b) 區、標成 `Shared` 的——和它對元件的規則相同。只能讀寫值，不能新增或移除 Resource | 獨占的 System 可以另外宣告 `ResMut<T>` 等參數 | 例如 `ExclusiveWorld<Writes<FixedTime>>`：列出來的算進寫者的計數，可以寫沒標 `Shared` 的 |
| 「只有一個寫者」的啟動檢查 | 它寫的 Resource 必須標 `Shared`，所以保護不到 | 照常 | 照常 |
| 系譜 | 現有的規則（規格 S-13）延伸到 Resource | Bevy：`&mut World` 只能和不碰 World 的參數並存，要並用得另外用 `SystemState` | 新導出的（檢查者的提議） |
| 代價 | 下一批的 `FixedTime` 要標 `Shared`，別的 System 也寫它不會在啟動時報錯 | 兩種存取同時在一個 System 手上，存取驗證要處理重疊；「唯一的參數」的規則與守衛 J-10 要改 | 多一種參數的寫法；從簽名推存取與啟動檢查都要改（會碰 `Registry.cpp`） |
| 需要它的 Feature | 固定步的驅動 | 同左 | 同左，外加「時間的 Resource 維持單一寫者」 |

共同的兩條（哪個選項都要）：
- `KernelSettings` 不給讀。它是驗證的開關，現在的規則不准任何 System 讀（D-E18），理由是開關不能影響模擬結果。
- 經這個介面拿到的參考，同樣受決定丁管。

**我的建議：乙一。** 理由：規則一致（獨占的 System 一律只能寫 `Shared` 的資料）；之後要加乙三是在旁邊多一種寫法，不會弄壞乙一的用法。通用的時間本來就會有兩個寫者（時間的 System 與驅動固定步的 System），不論選哪個都得標 `Shared`；乙三多保住的只有 `FixedTime` 一個。這一點到時間那一批的事前宣告我會再拿出來。

### 決定丙：「任何一張 schedule」包不包含 Startup 與 extract

你定的是「可以跑任何一張 schedule」。有兩張特別的，照字面放行會撞到既有的規則，所以要問：

| | Startup（啟動時跑一次的那張） | extract（從另一個 World 讀資料的那張） |
|---|---|---|
| 照字面放行會怎樣 | 只掛在 Startup 的 System 不算寫者（DI-KS 6），理由是「它只跑一次、在所有帶時鐘的 schedule 之前」。可以重跑的話，它就成了啟動檢查沒數到的第二個寫者 | 它的參數要知道來源的 World 是誰。現在單獨跑它會斷言；多個 World 是後面的批次 |
| 丙一：這一批先不准，回 Err | 收窄了「任何一張」 | 等多個 World 那一批（T5）一起定 |
| 丙二：放行 | 要改 DI-KS 6：Startup 的 System 也算寫者，既有的產品可能因此在啟動時報錯 | 這一批做不出來（沒有來源的 World） |
| 系譜 | 丙一：規格 B.3 的豁免照舊。丙二：Bevy 可以再跑 Startup，它沒有「單一寫者」的啟動檢查 | — |
| 需要它的 Feature | 沒有已點名的 | Extract（T5） |

**我的建議：兩張都丙一。** 理由：重跑 Startup 沒有已點名的 Feature；之後要放行是從 Err 變成可以跑，不會弄壞人。extract 這一批本來就做不出來。

### 決定丁：獨占 System 在 `RunSchedule` 之前拿到的指標

現況：`ExclusiveWorld::Get` 回傳指向元件的指標，`GetMut` 回傳 `Mut`（裡面是指標加當時的 stamp）。元件存在連續的陣列裡，增刪會搬動。所以裡面那張 schedule 只要增刪了同一種元件，外層先前拿到的指標就指到別的實體或懸空；`Mut` 還會用舊的 stamp 寫，變更偵測漏掉。規格 W-4 寫的是「同步點後所有參考失效」，現在的檢查只在呼叫 `ExclusiveWorld` 的方法時做，拿到手的指標與 `Mut` 沒有檢查。

| | 丁一：只寫成作者的規則 | 丁二：拿到的東西自己會檢查 | 丁三：不交出指標 |
|---|---|---|---|
| 做法 | 文件寫明「跑完 schedule 之後重新拿」；登記到跨元件規則頁 | `Get` 改回傳一個會檢查的參考型別；`Mut` 帶同步點的記號。過期後使用回 Err 或斷言 | 讀回傳值的複本；寫用 `Set(實體, 值)`。沒有東西可以帶過去 |
| 錯了會怎樣 | 記憶體錯誤或漏掉變更，沒有訊息 | 明確的錯誤 | 寫不出錯的程式 |
| 系譜 | Bevy 靠編譯器擋；C++ 沒有，所以只剩文件 | 現有句柄的過期檢查（W-4）延伸到拿出去的東西 | 新導出的 |
| 代價 | 無實作成本 | 每次經它讀寫多一次比較；`ExclusiveWorld` 現有的 `Get`／`GetMut` 的回傳型別要變（守衛 J-45 的測試要跟著改寫法，判定不變） | 讀大的元件要複製；現有的 `Get`／`GetMut` 要換掉（J-45 同上） |
| 需要它的 Feature | 這一批（開放給所有作者之後，這個坑人人踩得到） | 同左 | 同左 |

**我的建議：丁二。** 理由：這個介面是開放給所有作者的，錯了是記憶體錯誤而且沒有訊息，我認為該由機制擋；丁三更乾淨，但改掉現有介面的形狀，而且複製的成本落在每一次讀上。丁二的範圍先限在 `ExclusiveWorld` 交出去的東西，一般 System 的查詢不動。

---

## 4. 要改的程式

以下照我的建議（甲二、乙一、丙一、丁二）寫；你選別的，這張表跟著改。

| # | 位置 | 現在 | 改成 |
|---|---|---|---|
| 1 | `Executor.cpp` 的 `Product::Run` 入口 | 斷言 World 是閒置 | 從外面呼叫照舊。巢狀的入口另外一個，只接受「目前正在執行的呼叫是獨占的 System」；hook 裡回 Err。正在執行中的 schedule 記成一個堆疊（放在成員裡，不用 static），重複就回 Err |
| 2 | 同檔：狀態的切換與 `RunGuard` | 跑完設回閒置 | 記深度；裡面那張跑完回到「執行中」，最外層跑完才回閒置。作者的 System 接住例外時也要還原（RAII） |
| 3 | 同檔：每個 System 跑完的成員檢查 | 實體有增減就斷言 | 進入巢狀前先比對一次（到這裡為止不能有增減——抓得到獨占 System 自己繞路改的）；回來後以跑完的狀態為新的基準，再往下比（第 5.1 節） |
| 4 | 巢狀入口 | — | 決定甲二：延後的佇列不是空的就回 Err（新的錯誤碼） |
| 5 | `Executor.cpp`、`World.cpp`：存取驗證的第二層 | 裡面那張開始時的比對會把外層 System 的寫入當成「World 介面以外的改動」 | 進入巢狀前先把到目前為止的改動算在外層 System 頭上並檢查（有違規就記進這次呼叫的錯誤清單，不進入巢狀），再重拍快照；回來後再重拍 |
| 6 | `Params.hpp`：句柄的過期檢查 | 同步點之後句柄算過期 | `RunSchedule` 回來後更新這個 `ExclusiveWorld` 的記號 |
| 7 | 同檔：寫入用的 stamp | 進場時的 stamp | `RunSchedule` 回來後取一個新的 stamp（計數加一，不是沿用） |
| 8 | `Executor.cpp` 的 `Fail` | 設成中斷並呼叫錯誤處理 | 巢狀時設成中斷、留下錯誤，不呼叫處理函式；裡面的錯誤併進外層這次呼叫的清單；最外層統一呼叫一次。多層時留最裡面先發生的那個。中斷的 World 再被要求跑：不執行，回傳留下的錯誤 |
| 9 | `detail/Context.hpp`、`Product.hpp` | System 拿不到執行器 | 呼叫的上下文帶執行器的指標；`ExclusiveWorld::RunSchedule(名字)`。註：T2 會把 `Product::Run` 改名，T5 會讓一個 `Product` 有多個 World，這個指標要指到「這個 World 的執行器」 |
| 10 | `Params.hpp`、`World.cpp` | `ExclusiveWorld` 沒有 Resource 的介面；「只能寫 (b) 區 `Shared`」的檢查只認得元件 | 決定乙一：加讀與寫值；檢查補上 Resource 的版本；`KernelSettings` 回 Err |
| 11 | `Params.hpp` | `Get` 回傳指標、`GetMut` 回傳 `Mut` | 決定丁二：改回傳會檢查的型別 |
| 12 | `KernelError.hpp`、`Registry.cpp` 的 `ToString` | — | 新增錯誤碼：schedule 正在執行中、名字不存在、這張不能巢狀跑（Startup／extract）、佇列不是空的、hook 裡不能跑、參考已過期 |
| 13 | 不改、但要寫測試固定下來的 | tick 由裡面那張推進；hook 的佇列每張 schedule 一個；事件每個 tick 修剪；巢狀的同步點期間，呼叫的上下文會還原成外層的 | 行為不變 |

測試：新檔 `tests/common/kernel/NestedRunTests.cpp`。文件：[說明頁](system-contract-page.zh-Hant.md)加一節、HANDOFF、dev_log；`docs/cross-component-rules.md` 查有沒有同樣的規則，沒有就登記三條（裡面那張回 Err 要往上傳；指標不帶過 `RunSchedule`；hook 不跑 schedule）。

**作者的負擔（不改程式，但要寫明）**：獨占的 System 和同一個 stage 的每個 System 都算衝突。打開嚴格模式時，同一個 stage 的每個 System 都得和它寫明先後；(c) 區的 System 不論有沒有嚴格模式都要寫。所以 Engine 那個驅動固定步的 System 適合自己佔一個 stage（它掛在哪裡是下一批的事）。

**不碰**：`World::ApplySync` 的內容、事件的讀法、`Scheduler.cpp`（甲二不改執行計畫）、`Registry.cpp` 的啟動檢查（只加 `ToString` 的幾行）、漏斗工具、`.github/`、`build/`。

**和別的分支的先後**：PR #229 與 #233（`claude/gyo2-funnel-followup`，疊在 #229 上）都改 `Registry.cpp`、`Fingerprint.cpp`、golden 與突變清單 `mutations.json`。這一批等它們合併後才開工。

---

## 5. 既有條文與守衛測試的變更（AGENTS §3、§14.4）

### 5.1 守衛 J-3：「System 執行期間實體不能增減」（放寬，要你核准）

1. **它守的性質**：一個 System 執行的期間，World 裡有哪些實體、哪些實體有哪些元件，不會變；變更只在同步點發生（DI-KS 9、規格 W-3）。所以 System 手上的查詢結果與指標在它執行期間一直有效。
2. **放寬的面**：
   - 獨占的 System 在執行中跑了一張 schedule，那張 schedule 的同步點改了成員——以前斷言失敗，之後通過。
   - 連帶的後果：獨占的 System 在 `RunSchedule` 之前拿到的指標與 `Mut`，之後不再有效。這由決定丁處理。
3. **收緊的面**：無。
4. **放寬之後，原本的性質誰還守著**：
   - 一般的 System 不能跑 schedule，它們執行期間成員仍然不變，原本的檢查照舊。
   - 獨占的 System：進入巢狀之前比對一次、回來之後從新的基準再比。所以它自己繞過同步點改成員（不是經由跑 schedule），不論發生在跑 schedule 之前或之後，仍然抓得到。
   - 獨占的 System 手上的指標：決定丁二時由拿到的東西自己檢查；丁一時**沒有東西守著**，只有文件。
   - System 經 World 的公開介面直接改成員，仍然由 `World::Access` 的斷言擋下（`World.cpp:143-144`），這條不動。
5. **根據與證據**：設計第 0 節；上一輪報告 K0 表第 3 項（`Executor.cpp:216-217`、`WorldTests.cpp:197-215`）；這一輪報告 B1。

程序：現在請你同意方向。實作後我附上突變的結果與別的模型對這五點的驗證（用實際的測試跑），你核准了才合併。「同意方向」不算核准。

### 5.2 其他

「DI」＝Design Invariants 的變更，照 AGENTS §3 要你同意。

| 條文／守衛 | 變更 | 性質 |
|---|---|---|
| 概念稿 E5「System 執行中再跑 System 是 Programmer Error」 | 拿掉 | 跟著你定的 `v8-arch-gyo2-time-plan-two`（選固定步方案一的直接結果） |
| 規格 E.2 第 1 步「`Run` 斷言 World 是閒置」；規格 A 的狀態「執行中（哪張、第幾步）」 | 加上「或由獨占 System 發起」；狀態變成一疊 | 跟著上一條 |
| target `kernel` **DI-KS 1**、規格 S-13（`ExclusiveWorld` 的存取，以及 S-13 的理由句「獨占的 System 不碰 (c) 資料，所以寫者的規則不必列舉它」） | 加上「可以跑 schedule」。理由句要補一句：它跑的 schedule 裡的 System 各自照自己的宣告被數，不經由它 | **DI** |
| target `kernel` **DI-KS 9**（變更只在宣告的同步點套用；結果不依記錄順序） | 加上例外：獨占 System 跑的 schedule 有它自己的同步點。甲二時其餘不變；甲一時「宣告的同步點」與「不依記錄順序」都要改 | **DI** |
| target `kernel` **DI-KS 6**（Startup 的 System 不算寫者） | 丙一：不變。丙二：要改 | 丙二時是 **DI** |
| 規格 **W-4**「同步點後所有參考失效」與守衛 J-4 | 獨占 System 的句柄在巢狀回來後重新有效（第 6 項）；它拿出去的參考照 W-4 失效，丁二時加檢查。J-4 加巢狀的案例 | 句柄那一半是放寬，實作後列五點請你核准 |
| target `world` DI 3、規格 E.2「stamp 依執行計畫的順序推進」 | 巢狀回來後在計畫的步驟之外多取一個 stamp（第 7 項）。stamp 仍然只增不減、執行歷史相同則序列相同 | **DI**（文字要補） |
| 守衛 J-45（`ExclusiveWorld` 只能寫 (b) 區 `Shared` 的資料） | 乙一：加 Resource 的案例。丁二／丁三：既有案例的寫法跟著介面改，判定不變 | 只收緊 |
| 守衛 J-35（出錯只記一次） | 加巢狀的案例 | 只收緊 |
| 守衛 J-10（`ExclusiveWorld` 是唯一的參數） | 乙一、乙三：不變。乙二：要改，另列五點 | — |
| 「同步點只在 stage 結尾與宣告的位置」（規格 D.2） | 甲二、甲三、甲四：不變。甲一：加上「以及每個獨占 System 之前」 | 甲一時是行為變更 |
| 執行計畫、Registry、C 指紋的 golden（J-18 等） | **不變**。既有的 golden 用的 Registry 都沒有獨占的 System（唯一能建出計畫的是 J-45，它是 stage 裡唯一的 System）。#229 會把這些 golden 改成版本 2，合併後再確認一次 | 已查 |
| 規格 H.5 的 H-A1、H-9、DI-KS 12（回滾與重跑只由外面在閒置時呼叫） | **不變**，不在這一批。「不能跑正在執行中的那一張」是這一批的新規則，另給編號 | — |

---

## 6. 判定（先宣告，結果出來之後不放寬）

**判定 1：行為。** `NestedRunTests.cpp` 的案例全部通過（照建議的選項；你選別的，對應的案例跟著換）：

| 案例 | 驗什麼 |
|---|---|
| 跑 k 次 | 獨占 System 跑一張帶時鐘的 schedule 3 次：裡面的 System 各執行 3 次，tick 前進 3 |
| 回到外層 | 跑完後外層 schedule 後面的 System 照常執行；整個 `Run` 結束後 World 是閒置 |
| 裡面改了成員 | 裡面那張生成、刪除實體：不斷言；外層後面的 System 看得到 |
| 一般 System 直接改成員 | 照舊斷言（原有的 J-3 案例不改、照過） |
| 獨占 System 自己繞路改成員 | 在跑 schedule 之前改、之後改，各一案：都斷言 |
| 一般 System 繞路跑 schedule | 經全域指標呼叫 `Product::Run`：照舊斷言 |
| 佇列不是空的（甲二） | 同一個 stage 前面的 System 記了延後的變更：`RunSchedule` 回 Err，裡面那張一步都沒跑；中間宣告了同步點的版本可以跑 |
| 存取驗證不誤報 | 驗證打開：獨占 System 在跑 schedule 的前後各寫一次 (b) 區 `Shared` 的資料，不報錯 |
| 存取驗證照樣抓得到 | 驗證打開：(1) 裡面那張的 System 寫了沒宣告的資料，報錯並點名它；(2) 獨占 System 在進入巢狀前寫了不准寫的資料，記在它自己頭上 |
| 句柄與 stamp | 裡面那張跑兩次；每次回來後 `ExclusiveWorld` 照常可用，之後的寫入別的 System 的變更偵測看得到 |
| 過期的參考（丁二） | 跑 schedule 之前拿的參考，之後使用：回報錯誤，不是讀到別的實體 |
| 出錯記一次 | 裡面那張的 System 回 Err：錯誤處理只被呼叫一次，錯誤裡有裡面的 System、裡面的 schedule、外層的 System；World 中斷 |
| 外層回別的 Err | 裡面的錯誤仍然在清單裡 |
| 吞掉錯誤 | 外層 System 收到 Err 卻回 Ok：World 仍然中斷，錯誤仍然報出一次 |
| 中斷後再跑 | 迴圈裡第二次呼叫 `RunSchedule`：不執行、回傳留下的錯誤、錯誤處理沒有多被呼叫 |
| 裡面丟出例外 | 外層接住之後，深度與堆疊已還原 |
| 跑自己 | 跑正在執行中的 schedule：回 Err，不當掉。A 跑 B、B 再跑 A 也是 |
| 兩層 | A 跑 B、B 裡的獨占 System 跑 C：順序與 tick 都對 |
| hook | 裡面那張的同步點觸發 hook，照常執行；hook 裡呼叫 `RunSchedule`：回 Err |
| 名字不存在、Startup、extract | 各回對應的 Err |
| Resource（乙一） | 讀任何 Resource；寫 `Shared` 的成功；寫沒標 `Shared` 的或 (c) 區的回 Err；讀 `KernelSettings` 回 Err |
| 指紋不變 | 有獨占 System 的 schedule，執行計畫與加這一批之前相同 |

**判定 2：檢出力。** 突變清單在實作前由實作者（Opus）定案，交別的模型（Fable）檢查——找出「把實作改壞而測試仍然全過」的地方並補上。至少包含：跑完設回閒置；成員檢查整個拿掉；跑過 schedule 的 System 完全不查成員；進入巢狀前只重拍、不先比對；任何呼叫都可以進入巢狀；佇列不空也照跑；巢狀前不重拍快照；巢狀後不更新句柄；巢狀後不換 stamp；錯誤處理呼叫兩次；吞掉錯誤後 World 恢復；中斷的 World 照跑；跑自己不擋；跑自己只查最內層；hook 裡可以跑；Resource 的寫入不查；`KernelSettings` 也給讀；過期的參考不查。只有預期的那個案例失敗才算抓到；建置失敗不算。

**判定 3：沒有回歸。** 本機完整 ctest 全過；三平台 L1（linux-x64、windows-x64、macos-arm64）全過。既有測試除了第 5 節列的，一個判定都不改。

**判定 4：守衛測試的判定變更**（第 5.1 節的 J-3，以及 5.2 節 W-4／J-4 句柄那一半）各有五點、突變、別的模型的驗證，並經你核准。

---

## 7. 誰做、用什麼檔位

這一批是上段（動到 Design Invariants、公開介面、Kernel 的執行規則）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作 | Opus／high；執行器的狀態、出錯的處置、存取驗證的重拍三處用 xhigh | 這三處改的是順序與狀態，容易錯而且不容易發現；其餘是介面與測試 |
| 審查 | Fable（主會話） | 和實作者不同模型 |
| 突變清單的對抗檢查、第 5 節五點的驗證 | Fable／high | 和定清單的人（Opus）不同模型 |

時間的估計：實作與測試 5 到 8 小時（agent 的時間；比初稿多，因為清單變長了），審查與對抗檢查 1 到 2 小時。超過估計的 1.5 倍，或同一個問題換了兩次做法還沒結論，就停下來回報（AGENTS §14）。

---

## 8. Architecture Delta（和設計第 8 節的差分）

設計第 8 節已經列了七項。這一批的差分：

- **邊界**：Kernel 的執行從「一次一張 schedule」變成「可以巢狀」；`ExclusiveWorld` 的能力多了「跑 schedule」與 Resource 的讀寫。
- **依賴方向**：不變。
- **Ownership**：「什麼時候跑哪一張 schedule」原本完全屬於 Kernel 外面的呼叫者；之後 World 裡的獨占 System 也能決定。
- **為什麼不能更小**：比它小的做法有兩個（登記式、由執行計畫重複執行），你已經選了開放給所有作者的這一個。

**Design Invariants**（實作時寫進 Kernel 的文件，附守衛）：

1. 一張 schedule 同一時間只有一份在執行。守衛：「跑自己」。
2. 巢狀執行的錯誤只報一次，而且 World 一定中斷。守衛：「出錯記一次」「吞掉錯誤」「中斷後再跑」。
3. 一般的 System 執行期間成員不變；獨占的 System 只有在它跑的 schedule 裡成員才會變，它在那之前拿到的參考之後失效（丁二：使用時回報錯誤）。守衛：J-3 原有的案例、「獨占 System 自己繞路改成員」「過期的參考」。
4. 進入巢狀時，整個 World 共用的那個延後佇列是空的（甲二：不是空的就回 Err）。外層 schedule 的 hook 佇列可以有東西，它只在外層自己的同步點套用。守衛：「佇列不是空的」。
5. 只有獨占的 System 能進入巢狀；hook、run 條件、一般的 System 不能。守衛：「一般 System 繞路跑 schedule」「hook」。

**Replacement Criteria**：實作時發現巢狀執行另外需要的特例，超出這份宣告第 4 節的清單，累計到三個；或巢狀執行的缺陷在合併後於兩個不同的地方各出現一次。碰到就停下來，重新比較登記式與由執行計畫重複執行兩個做法。（初稿寫「超出清單就算」，而檢查已經在清單外找到六個，都補進第 4 節了；所以改成從這一版的清單起算。）

---

## 9. 範圍的限界

- 這份宣告是照讀程式寫的，沒有試做。兩輪檢查也都是讀程式推的，沒有實際跑過；懸空指標、同一窗的移除與插入兩個例子沒有寫測試重現。
- 第 3 節改寫後的四張選項表沒有再送檢查。
- Bevy 的系譜只核對了 `auto_insert_apply_deferred.rs:140-164` 與偵察報告第八節。
- 丁二的做法只寫到方向（會檢查的參考型別、`Mut` 帶記號），型別的形狀要到實作時才定；如果做出來要動到一般 System 的查詢，先停下來回報。
- 回滾、Inspector、復原都還沒設計。這個入口夠不夠它們用，沒有分析。
- 效能沒有量。

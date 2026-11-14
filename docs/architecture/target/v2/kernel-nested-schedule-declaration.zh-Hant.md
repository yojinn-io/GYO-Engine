# 事前宣告：獨占 World 的 System 可以跑一張 schedule（`v8-arch-gyo2-nested-schedule`）

更新：2026-11-14。Owner：架構（GYO2 重建會話）。**狀態：草稿，待使用者核准。核准之前不動工。**

根據：[時間與 runner 的設計](rebuild-time-design.zh-Hant.md)第 0 節；[HANDOFF](HANDOFF.md) 的 `v8-arch-gyo2-fixed-step-in-world`（固定步由 World 裡的 System 驅動）與 `v8-arch-gyo2-time-plan-two`（獨占 World 的 System 可以跑任何一張 schedule，開放給所有作者；理由：「固定步這個我不想限制作者的遊戲邏輯。關鍵不開放給一般作者，我怕會限制Engine能力」）。

事實的出處：檢查報告 `build/evidence/gyo2-time-v2/time-design-v2r3-b-check-opus.md`（主 checkout；現有程式在巢狀執行時會出錯的十四處，附行號）；Bevy 的做法 `scout-c-bevy-eight-items.md` 第八節。

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

**需要它的 Feature 與 Product**（AGENTS §2）：固定步由 World 裡的 System 驅動（①②，已定）；之後的回滾重跑（①）、工具的 Inspector 與復原（③）也會用同一個入口，但它們不是這一批的理由，也不在這一批做。

**為什麼是基盤**：它改的是 Kernel「一次只有一張 schedule 在跑」這條規則。同步點、延後的變更、存取驗證、出錯的處置都建立在這條規則上，所有 System 都跑在它上面。

**這一批不做的**：時間的 Resource、通用的時間、驅動固定步的那個 System 本身、runner、事件的游標、多個 World。它們是後面的批次（設計第 11 節）。

---

## 2. 作者看到的行為

1. **誰能跑**：任何以 `ExclusiveWorld` 為參數的 System（既有規則：它是那個 System 唯一的參數，只能是 (b) 區）。hook 與 run 條件不能。
2. **能跑哪一張**：這個 World 登記過的任何一張 schedule，次數不限。巢狀可以多層（A 跑 B，B 裡的獨占 System 再跑 C）。
3. **不能跑正在執行中的那一張**（自己，或外面幾層的任何一張）。回傳 Err。Bevy 也一樣不行。
4. **裡面那張照常完整執行**：它的 System、同步點、hook、tick 的推進（它是帶時鐘的 schedule 時）、事件的修剪。和從外面跑它沒有差別。
5. **跑完之後外層的 System 繼續執行**：手上的 `ExclusiveWorld` 照常可用；之後的寫入用新的變更記號（stamp），所以變更偵測看得到。
6. **裡面那張出錯**：World 立刻中斷，`RunSchedule` 回傳 Err。外層的 System 應該把它往上回傳。錯誤處理的函式只會被呼叫一次，拿到完整的鏈（裡面哪一步、外層哪個 System）。外層的 System 如果吞掉錯誤回傳 Ok，World 仍然是中斷的，錯誤照樣報出來。
7. **延後的變更**：見第 3 節決定甲。

和 Bevy 的對照：Bevy 是 `world.run_schedule(label)`，獨占的 system 拿 `&mut World`；跑自己會失敗；跑到一半 panic 時那張 schedule 不會被放回（沒有防護）。第 6 點是 GYO 多做的。

---

## 3. 要你定的兩件事

### 決定甲：排在獨占 System 前面的 System 記下的變更，什麼時候套用

現況：同步點只在每個 stage 結束時（以及 schedule 自己宣告的位置）。所以同一個 stage 裡，排在獨占 System 前面的 System 生成的實體、加上的元件，獨占 System 執行時**還沒套用**——它跑的 schedule 也看不到。漏斗工具補的元件同理。

| | 甲一（建議）：獨占 System 前面自動加一個同步點 | 甲二：不加，照現況 |
|---|---|---|
| 做法 | 排程時，每個持有 `ExclusiveWorld` 的 System 前面插一個同步點（它是 stage 的第一個時不必） | 不動 |
| 作者 | 不必知道這件事：獨占 System 看到的一定是套用完的 World | 要自己把生成者排在前一個 stage，否則這一輪看不到 |
| 系譜 | Bevy：有先後關係而且目的地是獨占 system 時，一定插套用點，連「忽略延後」的標記都擋不住（`auto_insert_apply_deferred.rs:150-164`）。GYO 的獨占 System 和同一個 stage 的每個 System 都有先後，所以等於「一定插」 | 現在的程式 |
| Kernel 的實作 | 進入巢狀時延後的佇列一定是空的（剛套用完，而獨占 System 自己不能記延後的變更），所以**不必**為每一層各開一個佇列 | 要為每一層各開一個佇列，否則外層記的會在裡面那張的同步點、用裡面的位置被套用 |
| 代價 | 有獨占 System 的 schedule，執行計畫多一步；同步點變多（每個要走一遍這段期間記下的變更，通常很少）。計畫的指紋會變，如果既有測試的 golden 涵蓋到這種 schedule 就要改 | 作者會踩坑；Kernel 多一個每層一份的結構 |

建議甲一：作者不必多懂一件事，Kernel 反而更簡單，而且和 Bevy 的實際行為一致。它是機制替作者做的事，不限制作者能做什麼。

### 決定乙：獨占 World 的 System 怎麼讀寫 Resource

現況：`ExclusiveWorld` 只有元件的介面（讀任何元件；寫只准 (b) 區、標成可多寫的），沒有 Resource 的介面；而且規定它必須是唯一的參數，所以也不能另外宣告 `Res`、`ResMut`。驅動固定步的 System 一定要讀寫 Resource（還差多少時間、這一輪跑幾步），照現況寫不出來。

| | 乙一（建議）：`ExclusiveWorld` 加上 Resource 的介面 | 乙二：放寬「唯一的參數」 |
|---|---|---|
| 做法 | 讀任何 Resource；寫只准 (b) 區、標成可多寫的——和它對元件的規則相同。執行期的檢查其實已經照這條規則寫好了（`World.cpp:146-156`），缺的只是介面 | 讓獨占的 System 可以另外宣告 `ResMut<T>` 等參數 |
| 寫者的計數 | 經 `ExclusiveWorld` 的寫入不算寫者（現況就是這樣）。所以它寫的 Resource 必須標成可多寫，「只有一個寫者」的檢查保護不到它 | 另外宣告的參數照常算寫者，Resource 可以維持獨占 |
| 系譜 | 現有的規則（規格 S-13）延伸到 Resource | Bevy：`&mut World` 只能和不碰 World 的參數並存；要兩者並用得另外用 `SystemState`（`system_param.rs:1005-1026`） |
| 代價 | 下一批的 `FixedTime` 與通用的時間要標成可多寫，別的 System 也去寫它們不會在啟動時報錯 | 兩種存取（整個 World 的、型別化的）同時在一個 System 手上，存取驗證要處理兩者重疊；「唯一的參數」的規則與它的守衛（J-10）要改 |

建議乙一：改動小，規則一致（獨占的 System 一律只能寫可多寫的資料）。代價是下一批時間的 Resource 少了「只有一個寫者」的啟動檢查——那時我會在那一批的事前宣告裡再把這一點拿出來。

---

## 4. 要改的程式

| # | 位置 | 現在 | 改成 |
|---|---|---|---|
| 1 | `Executor.cpp` 的 `Product::Run` 入口 | 斷言 World 是閒置 | 從外面呼叫照舊；從獨占 System 呼叫時允許在執行中進入。正在執行中的 schedule 記成一個堆疊，重複就回 Err（新的錯誤碼） |
| 2 | 同檔：狀態的切換與 `RunGuard` | 跑完設回閒置 | 記深度；裡面那張跑完回到「執行中」，最外層跑完才回閒置 |
| 3 | 同檔：每個 System 跑完的成員檢查 | 實體有增減就斷言 | 這次呼叫裡跑過 schedule 的，改以跑完後的狀態為準（第 5 節，守衛 J-3 的判定變更） |
| 4 | `Scheduler.cpp` | 同步點只在 stage 結尾與宣告的位置 | 決定甲一：獨占 System 前面插同步點。進入巢狀時斷言延後的佇列是空的 |
| 5 | `Executor.cpp`、`World.cpp`：存取驗證的第二層 | 裡面那張開始時的比對會把外層 System 的寫入當成「World 介面以外的改動」 | 進入巢狀前先把到目前為止的改動算在外層 System 頭上並檢查，再重拍快照；回來後再重拍 |
| 6 | `Params.hpp`：句柄的過期檢查 | 同步點之後句柄算過期 | `RunSchedule` 回來後更新這個 `ExclusiveWorld` 的記號 |
| 7 | 同檔：寫入用的 stamp | 進場時的 stamp | `RunSchedule` 回來後取新的 stamp |
| 8 | `Executor.cpp` 的 `Fail` | 設成中斷並呼叫錯誤處理 | 巢狀時設成中斷、留下錯誤，但不呼叫處理函式；最外層統一呼叫一次。外層 System 回 Ok 而 World 已中斷時，用留下的錯誤 |
| 9 | `detail/Context.hpp`、`Product.hpp` | System 拿不到執行器 | 呼叫的上下文帶執行器的指標；`ExclusiveWorld::RunSchedule(名字)` |
| 10 | `Params.hpp` | `ExclusiveWorld` 沒有 Resource 的介面 | 決定乙一：加讀與寫 |
| 11 | `KernelError.hpp` | — | 新增一個錯誤碼：schedule 正在執行中 |
| 12 | 不改、但要寫測試固定下來的 | tick 由裡面那張推進；hook 的佇列每張 schedule 一個；事件每個 tick 修剪；獨占 System 和同一個 stage 的每個 System 衝突 | 行為不變 |

測試：新檔 `tests/common/kernel/NestedRunTests.cpp`。文件：[說明頁](system-contract-page.zh-Hant.md)加一節、HANDOFF、dev_log。

**不碰**：`World::ApplySync` 的內容、事件的讀法、`Registry.cpp` 的啟動檢查（除了第 4 項需要的）、漏斗工具（PR #229、#231 正在改 `Registry.cpp` 與它的測試；這一批等它們合併後才開工，避免兩邊改同一個檔）、`.github/`、`build/`。

---

## 5. 既有條文與守衛測試的變更（AGENTS §14.4）

### 5.1 守衛 J-3：「System 執行期間實體不能增減」（放寬，要你核准）

1. **它守的性質**：一個 System 執行的期間，World 裡有哪些實體、哪些實體有哪些元件，不會變；變更只在同步點發生。（target `kernel` DI-KS 9、規格 W-3。）所以 System 手上的查詢結果在它執行期間一直有效。
2. **放寬的面**：獨占的 System 在執行中跑了一張 schedule，那張 schedule 的同步點改了成員——以前斷言失敗，之後通過。
3. **收緊的面**：無。
4. **放寬之後，原本的性質誰還守著**：
   - 一般的 System（沒有 `ExclusiveWorld`）不能跑 schedule，所以它們執行期間成員仍然不變，原本的檢查照舊。
   - 獨占的 System 沒有查詢，不持有會因成員改變而失效的結果；它經 `ExclusiveWorld` 的每次存取都是當場查。
   - System 繞過同步點直接改成員（經 World 的介面），仍然由 `World::Access` 的斷言擋下（`World.cpp:144`），這條不動。
5. **根據與證據**：設計第 0 節；檢查報告 K0 表第 3 項（`Executor.cpp:216-217`、`WorldTests.cpp:207-215`）。

這一條的程序：現在請你同意方向。實作後我附上突變的結果與別的模型對這五點的驗證（用實際的測試跑），你核准了才合併。

### 5.2 其他

| 條文／守衛 | 變更 | 性質 |
|---|---|---|
| 概念稿 E5「System 執行中再跑 System 是 Programmer Error」 | 拿掉 | 你定的（`v8-arch-gyo2-time-plan-two`） |
| 規格 E.2 第 1 步「`Run` 斷言 World 是閒置」 | 加上「或由獨占 System 發起」 | 跟著上一條 |
| 規格 H.5 的 H-A1「巢狀執行同一張 schedule 是斷言」 | 保留這個性質；形式從斷言改成回傳 Err（和 Bevy 的 `try_run_schedule` 一樣，作者拿得到錯誤） | 程式現在沒有實作它，這一批補上 |
| target `kernel` DI-KS 1、規格 S-13（`ExclusiveWorld` 的存取） | 加上「可以跑 schedule」；決定乙一時加上 Resource | 延伸 |
| 守衛 J-45（`ExclusiveWorld` 只能寫 (b) 區可多寫的資料） | 決定乙一：加 Resource 的案例 | 只收緊 |
| 守衛 J-35（出錯只記一次） | 加巢狀的案例 | 只收緊 |
| 守衛 J-10（`ExclusiveWorld` 是唯一的參數） | 決定乙一：不變。決定乙二：要改，另列五點 | — |
| 「同步點只在 stage 結尾與宣告的位置」（規格 D.2） | 決定甲一：加上「以及每個獨占 System 之前」 | 行為變更 |
| 執行計畫與 Registry 指紋的 golden（J-18 等） | 決定甲一時，如果 golden 用的 Registry 裡有不在 stage 開頭的獨占 System，值會變。實作時先查；會變的話另列五點請你核准 | 待查 |
| 規格 H-9、DI-KS 12（回滾的重跑只由 Host 呼叫） | **不變**，不在這一批 | — |

---

## 6. 判定（先宣告，結果出來之後不放寬）

**判定 1：行為。** `NestedRunTests.cpp` 的案例全部通過：

| 案例 | 驗什麼 |
|---|---|
| 跑 k 次 | 獨占 System 跑一張帶時鐘的 schedule 3 次：裡面的 System 各執行 3 次，tick 前進 3 |
| 回到外層 | 跑完後外層 schedule 後面的 System 照常執行；整個 `Run` 結束後 World 是閒置 |
| 裡面改了成員 | 裡面那張生成、刪除實體：不斷言；外層後面的 System 看得到 |
| 直接改成員仍然不行 | 一般 System 繞過同步點改成員：照舊斷言（原有的 J-3 案例不改、照過） |
| 決定甲一 | 同一個 stage 裡排在前面的 System 生成的實體，獨占 System 跑的 schedule 看得到；執行計畫裡看得到那個同步點 |
| 存取驗證不誤報 | 驗證打開：獨占 System 在跑 schedule 的前後各寫一次 (b) 區可多寫的資料，不報錯 |
| 存取驗證照樣抓得到 | 驗證打開：裡面那張的 System 寫了沒宣告的資料，報錯並點名那個 System |
| 句柄與 stamp | 跑完後 `ExclusiveWorld` 照常可用；之後的寫入，別的 System 的變更偵測看得到 |
| 出錯記一次 | 裡面那張的 System 回 Err：錯誤處理只被呼叫一次，鏈上有裡面的 System、裡面的 schedule、外層的 System；World 中斷 |
| 吞掉錯誤 | 外層 System 收到 Err 卻回 Ok：World 仍然中斷，錯誤仍然報出一次 |
| 跑自己 | 跑正在執行中的 schedule：回 Err，不當掉 |
| 兩層 | A 跑 B、B 裡的獨占 System 跑 C：順序與 tick 都對 |
| hook | 裡面那張的同步點觸發 hook，照常執行 |
| Resource（決定乙一） | 讀任何 Resource；寫可多寫的成功；寫獨占的或 (c) 區的回 Err |

**判定 2：檢出力。** 突變清單在實作前定案、交別的模型檢查（找出「把實作改壞而測試仍然全過」的地方並補上）。至少包含：跑完設回閒置；成員檢查整個拿掉（一般 System 也不查）；不插同步點；巢狀前不重拍快照；巢狀後不更新句柄；巢狀後不換 stamp；錯誤處理呼叫兩次；吞掉錯誤後 World 恢復；跑自己不擋；Resource 的寫入不查。只有預期的那個案例失敗才算抓到；建置失敗不算。

**判定 3：沒有回歸。** 本機完整 ctest 全過；三平台 L1（linux-x64、windows-x64、macos-arm64）全過。既有測試除了第 5 節列的，一個判定都不改。

**判定 4：守衛測試的判定變更**（第 5.1 節與 5.2 節「待查」那一列）有五點、突變、別的模型的驗證，並經你核准。

---

## 7. 誰做、用什麼檔位

這一批是上段（動到 Design Invariants、公開介面、Kernel 的執行規則）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作 | Opus／xhigh | 改的是執行器的狀態與順序，容易錯而且不容易發現 |
| 審查 | Fable（主會話） | 和實作者不同模型 |
| 突變清單的對抗檢查、第 5.1 節五點的驗證 | Fable／high | 和實作者不同模型 |

時間的估計：實作與測試 4 到 6 小時（agent 的時間），審查與對抗檢查 1 到 2 小時。超過估計的 1.5 倍，或同一個問題換了兩次做法還沒結論，就停下來回報（AGENTS §14）。

---

## 8. Architecture Delta（和設計第 8 節的差分）

設計第 8 節已經列了七項。這一批的差分：

- **邊界**：Kernel 的執行從「一次一張 schedule」變成「可以巢狀」；`ExclusiveWorld` 的能力多了「跑 schedule」（與決定乙一的 Resource）。
- **依賴方向**：不變。
- **Ownership**：「什麼時候跑哪一張 schedule」原本完全屬於 Kernel 外面的呼叫者；之後 World 裡的獨占 System 也能決定。
- **為什麼不能更小**：比它小的做法有兩個（登記式、由執行計畫重複執行），你已經選了開放給所有作者的這一個。

**Design Invariants**（實作時寫進 Kernel 的文件，附守衛）：

1. 一張 schedule 同一時間只有一份在執行。守衛：「跑自己」的案例。
2. 巢狀執行的錯誤只報一次，而且 World 一定中斷。守衛：「出錯記一次」「吞掉錯誤」。
3. 一般的 System 執行期間成員不變（J-3 原有的案例）；獨占的 System 跑 schedule 是唯一的例外。
4. 進入巢狀時沒有還沒套用的延後變更（決定甲一）。守衛：斷言，加上「決定甲一」的案例。

**Replacement Criteria**：巢狀執行另外需要的特例，超出這份宣告第 4 節列的清單；或巢狀執行的缺陷在兩個不同的地方各出現一次。碰到就停下來，重新比較登記式與由執行計畫重複執行兩個做法。

---

## 9. 範圍的限界

- 這份宣告是照讀程式寫的，沒有試做。第 4 節的清單來自別的模型的檢查，它也是讀程式推的，沒有實際跑過。實作時可能發現清單以外的地方；照 Replacement Criteria 處理。
- 決定甲一「進入巢狀時延後的佇列一定是空的」依賴兩件事：獨占 System 自己不能記延後的變更（它唯一的參數是 `ExclusiveWorld`，沒有記延後變更的介面）；hook 記的變更放在各 schedule 自己的佇列。這兩點我讀過程式，但沒有用測試確認，實作的第一步先確認。
- 回滾、Inspector、復原都還沒設計。這個入口夠不夠它們用，沒有分析。
- 效能沒有量。巢狀執行本身不增加每個 System 的成本；決定甲一多出的同步點有成本，通常很小，接上第一個真的系統之後再量。
- 這份宣告交給你之前經過別的模型檢查的情形，寫在交付時的訊息裡。

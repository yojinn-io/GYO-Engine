# 2026-11-22 GYO2：狀態機 Module（`v8-arch-gyo2-state-machine`，第 5 批）

Owner：架構（GYO2 主會話開出的任務卡 gyo2-state-machine-r8）。宣告：[state-machine-declaration.zh-Hant.md](../architecture/target/v2/state-machine-declaration.zh-Hant.md)（修訂 8）；條文頁：[state-machine.zh-Hant.md](../architecture/target/v2/state-machine.zh-Hant.md)。分支 `claude/gyo2-state-machine-r8-2afaeb`。

## 做了什麼

- **宣告重做成修訂 8**：11-16 核准的修訂 7 建立在「Kernel 管組裝、限制在 Kernel」的舊前提上。修訂 8 開頭列前提表（P1～P18），只改建立在舊前提上的地方，遊戲看得到的行為照舊（回收清單第 1.1 節）。
- **敵對式檢查**（Fable 5.2／high，唯讀）：blocker 4、should 11、nit 13，全部照改（宣告第 11 節）。
- **使用者動工前核准**（2026-11-22）：「照建議，三件都核准，開始」。三件的結果：
  - `RequestSilent` 這一批不做，和網路 Module 一起定（之後網路宣告的 Q8 定了 (b)，由後合併的一方加）。
  - 讀檔時子狀態也能 `SetInitial`。
  - 條件擋住的讀者照 (A) 處理：不改契約，寫說明頁。
- **多家交叉比對**：使用者問「你有交叉比對其他家的實作嗎？」——修訂 8 原本只對照了 Bevy。三個偵察（Opus／high）讀本機的 bevy_replicon、lightyear、Unity Netcode、flecs、Unity Entities、MassSample、Bevy。兩個分歧交使用者，使用者回「照建議，1 保留，2 選 A＋，開始」：
  - 「兩端同規則」回滾時照舊重跑轉換，副作用由作者讀 `Resimulation` 擋。
  - 另開事件游標重評卡。那張卡之後定案為作者的事、不改 Kernel（`v8-arch-gyo2-event-cursor-reeval-author-side`）。
- **實作**：
  - 新 Module `engine/modules/state_machine`（`GYO::StateMachine`，登記檔一列，`library_only=yes`、`default=no`）。
  - 名字表 `StateMachineTraits<S>` 與 `RootState`／`SubState` 約束。
  - `StateMachine<S>`（封閉的型別，`SetInitial` 只在第一次套用之前有用）、`StateMachineRequest<S>`。
  - `RegisterStateMachine<S>`：自己插 stage `<S>.Apply`，預設在 `Main` 的 `PreUpdate` 之後；第一個值由準備函式放。
  - 套用的 System（第 5.1 節的順序）、`InState`／`NotInState`。
  - 不改 Kernel 與編排層的任何檔。
- **文件**：
  - 條文頁 `state-machine.zh-Hant.md`（DI 1～5、7～10，RC 1～8）。
  - [System 契約說明頁](../architecture/target/v2/system-contract-page.zh-Hant.md)第 15 節。事件游標重評卡也加第 15 節，誰後合併誰改號。
  - 用詞表加「State Machine（狀態機）」一列；README 加索引。
  - `docs/cross-component-rules.md` 查過：沒有同樣的規則；套用的 System 照 CCR-2（巢狀 `RunSchedule` 的 Err 往上傳）。

## 驗證與經過

- **判定 1（行為）**：`state_machine.behaviour` 29 個案例（設計審查後加 1）。這個 target 只連 `GYO::StateMachine`，它編得過也是 DI 8 的守衛之一。
- **組合的案例**：`state_machine.combination` 8 個。
  - 進入裡生成的實體同一幀看得到。
  - 登記兩次是 R-E2。
  - 替換 World 與 `ReplaceRequest.extra` 的 `SetInitial`。
  - 兩棵樹的實際先後。
  - 沒有條件時生成中第一幀就進初始值。
  - 進入那張的讀者：後果 3。
  - 條件擋住的讀者：後果 1、2，照事件游標重評的結論固定現況。
  - (c) 區「兩端同規則」：伺服器權威＋狀態回滾，不報 AU-E4／E6／E9；值換了 digest 跟著變；回滾退回 6 步重跑時「進入下雨」再跑一次，`Resimulation` 擋住的音效沒有重發。
- **判定 2（檢出力）**：突變清單 [state_machine_mutations.json](../architecture/target/v2/scripts/state_machine_mutations.json)，最後 63 個，全部 killed（設計審查後退 2 加 1，見〈設計審查〉）。
  - 第一輪 48 個：47 killed。`sm-registration-no-child-values` 是 wrong_failure：缺子的值時前面的案例先斷言中止。把 cases 縮成它自己的案例後 killed（README 第 1 條）。
  - 清單交 Fable 5.2／high 做敵對式檢查（作者 Opus 5.5）：契約要補 18、誤用不補 5、細節不補 9。
  - 「契約要補」的處理：
    - 補了 16 個突變與對應的斷言，新的突變 expect 指到斷言。
    - 修了 `sm-conditions-ignored`：它原本是被另一個案例的斷言中止判成 killed，cases 已縮窄。
    - 合併用 OR 的六個改成 AND。
    - 修了 `sm-first-applies-request` 依賴未定義行為的寫法。
    - 改正幾個 `what` 與 `guards`。
  - 改過與新加的 24 個重跑：24/24 killed。
  - 檢查報告裡沒加的：
    - G5「沒有 `SetInitial` 以外的改值成員」：只能列名字驗，沒有一般的寫法。**偏離宣告第 7 節**（表上寫「留」），由設計審查看。
    - G18：舊的 32 個 expect 仍是案例名（專案現行的寫法，例 `world_time_mutations.json`），只改了被點名漏判的幾個。
    - 宣告表上「待用值用完沒清」：單點的改法和原程式等價（之後子出現一律傳 `nullopt`），照 §14.5 不加。**偏離宣告第 7 節**。
    - 「子的離開出錯後照樣往下跑」：Kernel 中斷後下一個 `RunSchedule` 立刻回 Err，等價，不加。
    - 「準備函式蓋掉已有的值」：沒有案例能獨自抓到，不加（宣告已寫）。
  - 數量：README 第 8 條的參考是「變更的 DI 條數 × 5」，新 Module 有 9 條 DI，約 45；這一批 64 個，超過。理由：套用的 System 的順序與子狀態有很多獨立的決定（第 5.1 節逐步都可能各自錯），檢查也照「每個獨立決定一個」補。合併前核准時說明。
- **判定 3（沒有回歸）**：本機完整建置與 ctest：171/171（加測試之前那一次）。改完測試後兩個 target 都重跑過。三平台 L1 看 PR 的 CI。
- **AGENTS §14 的停止條件**：沒有碰到。

## 設計審查

Fable 5.2／high，唯讀，對象 d614a34c；報告 `design-review-fable.md`。

**結論**：可以合併，沒有要重做的地方。職責與依賴沒有問題（只用 Kernel 與編排層的公開標頭，兩者沒有一行提到狀態機，連結集有釘）；套用的順序、第一次套用、子狀態、`keepOnParentReenter`、`SetInitial`、預設插法、條件、名字都照宣告；沒有替誤用加的守衛或突變。

**建議改 6 條，全部已改**：
- `StateMachineRequest` 的 `==` 改回比全部欄位：Kernel 的回滾還原「相等就不寫回」，只比待處理的要求時計數不會還原。拿掉 `once == twice` 那句與突變 `sm-request-eq-counter`。
- D9：`Leave` 把值重設成初始值，不存在的子不留之前的值（`Hash.hpp` 不雜湊殘留的原則；Bevy 是把 `State<S>` 整個移掉）。加案例「an absent child is the same whatever value it had」與突變 `sm-leave-keeps-value`；`sm-child-reappears-last-value` 因此變成等價，退掉。
- M3：判成誤用，程式不改；DI 1 的字面寫明「`Detail` 之外」，「沒有公開的預設建構」的理由改成「作者只能放回存過的值、造不出沒存過的值」。
- 索引改放〈給作者看的〉與〈計畫與批次的紀錄〉。
- 條文頁：表補 `Pending()`；(c) 區的例子用 `ApplySystemName`／`EnterScheduleName`；宣告第 4 節第 4、5 點寫進第 3 節；特性的成員名和宣告字面的差別寫明。
- 拿掉沒用到的 `Detail::kAlwaysFalse`。

改後重跑相關的突變：`sm-leave-keeps-value`、`sm-overwritten-reset-on-apply`、`sm-child-not-marked-absent` 3/3 killed（`mutations-run4-review.txt`）。突變清單共 63 個。

**要使用者定的 1 條**：判定 2 有兩列沒照宣告加突變（「`SetInitial` 以外的改值成員」、「子的待用值用完沒清」），是放寬事前宣告的判定。審查者的看法：兩列都接受。合併前核准時交使用者。

**記下交之後的**：
- 網路那一批：計數與自訂比較、`Access` 的形狀要一起看。
- 讀檔卡：`SetInitial` 不驗值，說明頁寫「先用 `NameOf` 驗」；「存了再讀摘要相同」靠 D9 的改法。
- 第 15 節撞號：後合併的一方改號。
- 父的要求不合法時子的要求沒被取走，World 已中斷，沒有後果。
- MSVC 能不能編樣板參數的 lambda，看 L1。

## 交給設計審查與之後的

- `RequestSilent` 與 Request／RequestSilent 兩個套用函式：網路宣告 Q8 (b)，狀態機先合併時由網路卡在它的第 2 步加。

## 證據

這台 Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-state-machine-r8/`（git 不追蹤）：
- 宣告的檢查報告：`declaration-check-r8-fable.md`。
- 交叉比對：`crosscheck-network-adoption.md`、`crosscheck-initial-load.md`、`crosscheck-message-retention.md`。
- 突變：`mutation-check-fable.md`（清單的敵對式檢查）、`mutations-run1.txt`、`mutations-run2-registration.txt`、`mutations-run3.txt`、`mutations-run4-review.txt`。
- 設計審查：`design-review-fable.md`。
- 建置與測試：`build-full.txt`、`ctest-full.txt`。

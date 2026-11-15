# GYO2 重建：漏斗工具 `Registrar::InitialValue`（`v8-arch-rebuild`）

日期：2026-11-14。Owner：架構（GYO2 重建會話）。分支 `claude/gyo2-funnel`（從 `claude/gyo2-mechanism` 切出，併入 `gyo2` 與文件分支）。決定：[HANDOFF](../architecture/target/v2/HANDOFF.md) 的 `v8-arch-gyo2-spawn-writer-funnel`、`v8-arch-gyo2-count-reverted-funnel-a`；用法與規則：[System 契約說明頁](../architecture/target/v2/system-contract-page.zh-Hant.md)第 8 節。

## 做了什麼

- `Registrar::InitialValue<Target, Fn>()`：元件的擁有者宣告「`Target` 的第一個值從請求元件、用純函式 `Target Fn(const Request&)` 算」。
- 同步點：在加上元件（第 5 步）之後、OnAdd（第 6 步）之前，對這個同步點新出現的每個請求補上元件；補上的 cell 算新增，OnAdd 與 `Added<T>` 照常；串起來的宣告在同一個同步點補完。
- 啟動檢查：型別沒登記（R-E4）、不是擁有者宣告（R-E4）、同一個宣告寫兩次（R-E2）、(c) 區的元件從非 (c) 區的請求取值（D-E7／Z1）。
- 寫者的數法沒有改：補值的不是 System，不進寫者的計數。
- 骨架測試改成漏斗的寫法：生成者只生成 `Arrival`，Position、Velocity、Health 回到預設的獨占。

## 經過

- 架構師（主會話 Fable）實作。形式是使用者選的選項 A（照 Bevy 的 required components 調整：宣告在目標元件這一側、初始值由請求的資料算）。
- 架構師自己定的細節有十條（說明頁第 8 節），交使用者看：已經有的不蓋、覆寫請求不重補、請求留著、可以串、多個請求時取 ComponentId 小的、只有擁有者能宣告、(c) 區的限制、Host 的 API 不補等。
- 寫測試時碰到：每個有 `Spawn` 參數的 System 都算配發器的寫者，兩個生成者時配發器要標 Shared。現況，沒有改，記在說明頁第 7 節。
- **沒做**：Registry 指紋與 C 指紋沒有把這個宣告算進去。算進去要改 J-18 的 golden 與指紋格式版本，屬守衛測試的判定變更，等使用者核准。在那之前，兩個只差初始值宣告的 Registry 指紋相同。

## 驗證

本機 Intel Mac（i9-9980HK），preset `core`（RelWithDebInfo）。

- `tests/common/kernel/InitialValueTests.cpp` 8 個案例通過；骨架測試的行為部分 7 個案例通過（手算的八個 tick 結果不變）。
- 突變 8 條，每條都由預期的那個案例失敗抓到：蓋掉已有的元件、不做第二輪（串接斷掉）、補上的 cell 不算新增、處理順序倒過來、拿掉擁有者檢查、拿掉區的檢查、拿掉重複檢查、整步不執行。
- 完整 ctest 93／93。
- 三平台的 L1：在這條分支的 PR 上跑，結果記在 PR。
- 沒有做別模型的對抗檢查。

## 證據

主 checkout（這台 Intel Mac）`/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-mechanism/`：`funnel-mutants.log`、`funnel-ctest-full.log`。突變的腳本未保存（會話的暫存區）。

## 補記：對抗檢查之後（2026-11-14～15）

- 任務卡 `v8-arch-gyo2-funnel-check`（Opus／xhigh）檢查這一批：blocker 3（經請求可以繞過區的規則 Z2、Z3、Z6）、should 3、nit 3；補了測試（PR #229）。
- 主會話先提了一個修法（PR #231：元件與請求的區、role、rollback 三樣要相同）。卡在使用者的指示下改成另一種：**寫請求的 System 或 hook，在區的檢查裡當作也寫了它補出來的元件**，直接用原本的區檢查，不另外抄一份規則；同一個 PR 也把宣告算進了 Registry 指紋與 C 指紋。卡另外請 Fable 做了一輪檢查，找到抄規則時失真的地方，這也是改做法的原因。**PR #231 因此關閉，不合併。**
- 這個後續分支（`claude/gyo2-funnel-followup`，疊在 PR #229 上）只補 #231 裡沒有被取代的三小項：(c) 區元件的初始值宣告所在的檔案列進 (c) 區原始檔清單（檢查的 S1）；區域別名 `Shape` 改名 `Signature`（N1，避免遮住私有的 `Registrar::Shape`）；說明頁與註解把「取 ComponentId 小的」更正為只在同一輪成立（S2），並加一個固定這個行為的測試。
- 驗證：本機 Intel Mac，preset `core`。`InitialValueTests.cpp` 31 個案例通過（其中 3 個是這次加的）；突變 2 條都由預期的案例抓到；完整 ctest 93／93。證據在主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-funnel-followup/`（`tests.log`、`mutants.log`、`ctest-full.log`、突變腳本）。
- 先前「沒有做別模型的對抗檢查」一句因此更新：已做兩輪（Opus 查主會話的實作；Fable 查卡的修法）。這個後續分支的三小項沒有另外檢查。

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

## 補記：對抗檢查與區規則的修正（同日）

- 任務卡 `v8-arch-gyo2-funnel-check`（Opus／xhigh）檢查這一批，找到 3 個 blocker、3 個 should、3 個 nit；補 6 個測試（PR #229）。
- blocker 都是同一個原因：啟動檢查只查「(c) 的目標要 (c) 的請求」，System 與 hook 平常要過的區規則沒有套到機制代寫的元件上。
- 修正（分支 `claude/gyo2-funnel-zone-fix`）：元件與請求的區、role、rollback 必須都相同（D-E7：Z1／Z2、Z3、Z6）；(c) 區的宣告的檔案列進 (c) 區原始檔清單；區域別名 `Shape` 改名（避免遮住私有的 `Registrar::Shape`）；說明頁的規則更正兩條。
- 驗證：新增 7 個案例；其中 6 個在修正前失敗（`red.log`），修正後全過（`green.log`）；突變 7 條全部由預期的案例抓到（`mutants.log`）；完整 ctest 93／93（`ctest-full.log`）。證據在主 checkout（這台 Intel Mac）`/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-funnel-zone-fix/`；檢查卡的證據在同層的 `gyo2-funnel-check/`。
- 上面「沒有做別模型的對抗檢查」一句因此更新：已做（Opus）。修正本身沒有再做一次別模型的檢查。

# 漏斗工具的對抗檢查與區規則（`v8-arch-gyo2-funnel-check`）

日期：2026-11-14～15。分支 `gyo2`。PR [#229](https://github.com/yojinn-io/GYO-Engine/pull/229)（合併 `5d01b6f5`）；後續 PR #233。

這一批由任務卡「漏斗工具的對抗檢查」做。卡片結束時沒有寫文件，這一篇由 GYO2 主會話照卡片的完成回報、PR #229 的說明與證據資料夾補寫。

## 做了什麼

1. **三輪對抗檢查**（Opus 一輪、Fable 兩輪），對象是 #226 的 `Registrar::InitialValue`。
2. **使用者決定擋下三個繞路**（B1～B3）：ingress 經 Input 的請求造出 (c) 區 State 的資料；(c) 區的 System 造出 Input 的資料；開回滾的 schedule 裡出現不回滾的 (c) 區元件。
3. **區規則改成一條原則**：插入或生成請求的 System／hook，在區的檢查（Z1、Z2、Z3、Z6）裡也算寫了它補出來的元件；串接一路往下算；不計入寫者的數量（D-E2）。不再為漏斗另抄一份規則。實作在 `Registry.cpp` 的 `FunnelWrites`／`ZoneAccess`。
4. **就地寫或移除請求不算**（第二次 Fable 檢查之後）。為了分辨插入與移除，新增存取項目 `Access::DeferredInsert` 與公開結構的欄位 `AccessSet::deferredInserts`（不進指紋）。
5. **指紋**：Registry 指紋格式 1→2，算進所有宣告；C 指紋格式 2→3，只算目標在 (c) 區的宣告。J-18／J-20 的 golden 與版本號跟著更新（判定相同，只有期待值變；三平台同值）。

## 檢證

- 完整建置；`ctest -L "cpu|shader"` 93/93；`gyo_kernel_tests_reversed` 通過（macOS arm64）。L1 三平台通過後合併。
- 突變：`docs/architecture/plans/kernel/scripts/mutations.json` 新增 `rs-iv-` 29 條；更新失效的 `rs-b6-cfp-version-1` 與 `rs-adv-hook-zone-rules-off`；改動範圍與被改的測試相關的 140 條重跑，140/140 被抓到。
- 既有的問題（不是這一批造成的）：`ks-links-extra`、`ks-adv-include-allowlist-widened`、`ks-k5-parent-directory` 三條在合併基底就已經失效。

## 經過

- 第一輪（Opus）找到我實作的漏斗讓工具繞過區規則（Z2、Z3、Z6）。第一版修法是在宣告的檢查裡把規則抄一份（#231，後來關掉）；卡片改成上面那一條原則。
- 第二輪 Fable 檢查沒有 blocker，也沒找到「直接寫會被擋、經漏斗卻放行」的路徑；找到的都在另一邊——擋得太寬（只改請求的值也被算成寫了元件）、一條既有的守衛突變因為改了那一行而悄悄失效、四個行為突變測試抓不到。
- 教訓（卡片記的）：改了程式之後，只重跑新登記的突變不夠，落在改動範圍內的舊突變也要重跑。

## 需要產品測試

使用者 2026-11-15 決定先合併，並標記為需要產品測試。區規則沒有實際資料驗證；清單與回頭驗證的時機在[說明頁](../architecture/target/v2/system-contract-page.zh-Hant.md)第 8 節。

## 卡片留下、交給主會話的事

- 兩件當時未定的小事，PR #233 已處理：(c) 區的原始檔清單沒有列入初始值的宣告；說明頁「取 ComponentId 小的」只在同一輪成立。
- 卡片的建議（還沒有人定）：凡是「機制替 System 做事」的東西（hook、漏斗、之後的回滾還原、網路複製），設計時先決定「用誰的權限做」。要不要寫進規格由使用者決定。
- 文件裡還有 56 處指向舊的突變執行器路徑 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py`，實際已經在 `tests/common/mutation/run_mutations.py`。沒有處理。

## 證據

主 checkout（這台 Mac）`build/evidence/gyo2-funnel-check/`：建置與 ctest 的紀錄、各輪突變的清單與結果。

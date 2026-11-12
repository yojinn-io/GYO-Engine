# 事前宣告：`v8-arch-kernel-spike-pageguard`（頁保護：存取驗證的第三層能不能攔住 L1、L6）

更新：2026-11-12。Owner：架構（架構師會話）。**狀態：使用者 2026-11-12「門檻照你定的，xhigh 同意，開始」；執行中。**

決定來源：HANDOFF `v8-arch-kernel-spike-pageguard`（使用者 2026-11-11，側邊會話轉述：只對寫設保護、只保護獨占寫入的欄、只在 (c) 區 System 執行時）；B-9 結案後歸「存取驗證（可關）」（`v8-arch-kernel-verification-not-assert`）：頁錯誤處理器把違規記進本次呼叫的 context、System 以 Err 回傳，不是 GYO_ASSERT。

## 目的

spike 的範圍の限界 L1（System 內寫了又在返回前寫回原值）與 L6（host 在同一視窗內經 World API 碰過同一欄後的繞路寫入）是現有兩層驗證（存取當下查宣告、System 邊界比位元組）結構上攔不到的：前者沒有淨變化，後者的基準被 `MarkWritten` 更新。頁保護從另一個方向攔——**寫入動作本身**觸發頁錯誤，與資料的淨變化無關。本批回答三題：(1) 頁保護能不能攔 L1、L6 並讓 System 繼續到返回、以 Err 回傳；(2) 三個 L1 平台都做得到；(3) 成本（切換時間、記憶體）在「驗證模式」下可不可接受。回答之後才決定它要不要進規格 A.6 成為第三層、LIM-3 的 L1／L6 能不能標 `Lifted`。

## 範圍

只在 `spikes/kernel/`（獨立 CMake 專案，不進 GYO build graph，不 include GYO 模組）。不改 `engine/`、`framework/`、`apps/`、契約、preset；`.github/workflows/cross-platform.yml` 的 kernel spike 步驟已會建置並跑 `spikes/kernel/` 的全部測試，本批不改 workflow。

1. **頁對齊的欄儲存**：`World` 加執行期選項 `pageGuard`（spike 用建構參數代替規格的 `KernelSettings`；關閉時用原本的 `operator new`，不多配一頁、不呼叫任何保護 API——規格四條件的「關閉零成本」）。開啟時每個**獨占寫入**（spike 現有的 Exclusive 標記；Shared 欄不保護）的欄以頁對齊配置（`mmap`／`posix_memalign` 或 `VirtualAlloc`），每欄佔整頁數，頁大小執行期查（`sysconf(_SC_PAGESIZE)`／`GetSystemInfo`），不假設 4K（Apple Silicon 是 16K）。
2. **保護時機**：`Run` 進入 Running 時把所有獨占欄設唯讀；每個 **(c) System** 呼叫前只解除它宣告寫的欄、返回後重新保護；(b) System 執行期間全部解除（spike 以 System 的 zone 標記判斷；(b) 的寫入照舊只靠兩層）；`Run` 結束回到可寫（host 在 Ready 的寫入不受影響）。成本因此是每個 (c) System 2×|writes| 次保護呼叫，不是 2×|欄|。
3. **頁錯誤處理器**：POSIX 用 `sigaction(SIGSEGV | SIGBUS, SA_SIGINFO)`，Windows 用 `AddVectoredExceptionHandler`。處理器只做：查故障位址屬於哪個受保護欄（預先排好的區間表，二分搜尋，不配置記憶體）→ 把違規（欄 id、System、位址）寫進目前 System 的 context（`tlCurrent`，固定大小槽）→ 解除該欄保護（讓指令重試成功）→ 返回。位址不在任何受保護欄 → 交回預設處理（真正的 segfault 照舊）。`tlCurrent` 為空（host 執行緒之外或 Running 之外的故障）→ 記為「無 System 的違規」並 abort（spike 採 GYO_ASSERT 的形狀；規格怎麼歸類是本批報告要提的問題，不是本批決定）。
4. **跳板**：System 返回後若 context 有頁保護違規，`Run` 回傳 Err（帶欄 id、System 名、位址），與現有的存取當下與邊界比對違規並列（spec B-9 的 Siblings 形狀，spike 以簡單清單代替）。
5. **測試**（`tests/PageGuardTests.cpp`，同一份原始碼進 debug 與 release 兩個目標；release 目標 `pageGuard` 關）：
   - Bypass：L1（存參考在 System 內寫 `x+1` 再寫回 `x`）、L6（host 經 `GetMut` 碰過欄後，System 內經存下的指標寫另一列）、原 (b1)–(b3)（裸指標寫）在頁保護下被攔。
   - Control：宣告 `Write<T>` 的欄在 System 內寫不故障；Shared 欄寫不故障；(b) System 寫任何欄不故障；`pageGuard` 關時零故障且保護 API 呼叫計數為 0。
   - Continue：故障後 System 執行到返回；同一 System 同一欄第二次寫不再故障、違規只記一筆；`Run` 回 Err；之後 World 可再 `Run`（spike 無 Interrupted 狀態，不測）。
   - Cost：見判定 4。
6. **突變**（加進 `mutations.json`，照現有格式）：移除處理器的記違規；移除「只在 (c) System 開保護」；移除「只保護獨占欄」；移除故障後的解除保護（預期：處理器第二次收到同一位址的故障時 abort，測試行程非零退出——本批宣告**非零退出也算該突變的預期失敗**，因為沒有解除保護就只有無限重試或 abort 兩種結果）；移除 `pageGuard` 關時的早退（Control「呼叫計數為 0」失敗）。
7. **報告**：`spike-pageguard-report.zh-Hant.md`（判定、成本數字、三平台結果、對規格 A.6／LIM-3 的建議、未解的問題）；dev_log 同一 PR。

## 判定（看結果前宣告）

1. **攔截**：L1、L6 與 (b1)–(b3) 的每個 Bypass 測試在 `pageGuard` 開時都得到 Err，違規記錄的欄 id 是被寫的欄、位址落在該欄的頁範圍內。任一不成立＝判定 1 不成立，不改測試去湊。
2. **繼續執行與單次記錄**：Continue 的三條都成立。
3. **三平台**：PR 的 L1 三平台（linux-x64 gcc、windows-x64 MSVC、macos-arm64 AppleClang）kernel spike 步驟全綠，且本機 Intel Mac（AppleClang）兩組態全過。任一平台紅＝判定 3 不成立；不以「本機過了」代替。
4. **成本**（驗證模式的可接受性；release 預設關，所以門檻是測試模式的）：debug 目標內計時，64 個獨占欄各一頁、一個 (c) System 宣告寫 2 欄，`Run` 一次；1000 次取中位數，本機 Intel Mac。宣告門檻：單次保護 API 呼叫中位 **≤ 10 µs**；一個 System 的切換（2 解除＋2 重保護）中位 **≤ 50 µs**；`Run` 進入／結束的全欄保護（64 欄）中位 **≤ 1 ms**。記憶體：每欄至少一頁，報告以 1000 欄 × 本機頁大小估算並寫出數字（不設門檻，給規格決定）。超過門檻＝「不可作為預設測試模式」，但不否定判定 1～3。門檻看結果後不放寬。
5. **突變**：第 6 點的五條突變全部 killed（照 `run_mutations.py` 的判定，第四條含非零退出）。

件数の突き合わせ：報告以「測試數（新加／既有）、突變數（新加／killed）、三平台 ctest 的 tests／failures」三組數字開頭；三平台數字從 L1 artifact 的 `kernel-spike-tests.xml` 讀，不從日誌摘要。

## 範囲の限界（本批不解、報告要列）

- 承自 spike：L2（經存下的參考**讀**——頁保護只對寫設保護，不解除）、L3（有 `Write<T>` 宣告的 System 繞過句柄寫——欄本來就解除了保護，change stamp 仍會漏）、L4（元件內堆積成員：堆積不在受保護頁上）、L5（全域狀態）、L7（其他執行緒的故障：`tlCurrent` 為空，spike 一律 abort；Scheduler 批再評估）、L8（懸空參考：ASan）。
- 本批新增：只保護**欄**；Resource 槽與事件緩衝不保護（事件緩衝會重新配置；Resource 的保護留給規格決定是否值得）。頁粒度＝一欄一頁以上，小欄浪費記憶體（報告給數字）。處理器是行程級、唯一一個；與同時裝 SIGSEGV 處理器的工具（ASan、某些 crash reporter）互斥，本批不在 sanitizer 下跑；lldb／gdb 會先攔 SIGSEGV，手動除錯要設 `process handle SIGSEGV -s false -n false`。Windows 的 VEH 在 debugger 附加時行為相同，但 `/EHsc` 不會把存取違規當 C++ 例外，不依賴 SEH 轉換。
- spike 仍用建構參數代替規格的 `KernelSettings` 與啟動參數；四條件中的「狀態進證據」「預設跟建置變體」不在 spike 範圍。
- LIM-3 的用詞：本批結果出來前，LIM-3 仍完整有效；解除 L1／L6 要等規格 A.6 收進第三層、且遷移批的實作通過同一組測試。
- 承自 -spec-v03：對抗檢查報告原文未保存的作法本批不沿用——本批的敵對檢查報告存進主 checkout `build/evidence/kernel-pageguard/`（git 管理外）並在報告寫路徑。

## 模型／檔位

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 主會話（宣告、協調、處置、報告） | Fable／使用者設定 | 控制權在主會話（`v8-arch-kernel-control`） |
| 實作者（子代理，獨立 worktree，只 commit 不 push） | **Opus／xhigh** | 上段（驗證機制＝Design Invariants 的守衛；信號處理、頁保護、三平台分支是「気づかないうちに誤りやすいロジック」）；與 -spike 同模型，經驗沿用 |
| 敵對檢查者（只讀） | **Fable／xhigh**（`xhigh-reviewer` 類型） | 與實作者不同模型（盲點不重疊）；xhigh 需使用者同意 |
| 本機建置、ctest、突變執行 | Haiku 或主會話／low | 定型作業 |

估時：實作 6 小時（上限 9 小時）、敵對檢查 1.5 小時、處置與報告 1.5 小時。單一問題累計超過 2 小時（例如某平台的處理器行為）就停下報告（AGENTS §14、time box）。證據：`build/evidence/kernel-pageguard/`（本機計時原始輸出、突變結果、L1 artifact 的 `kernel-spike-tests.xml` 三份）。

## 檢查：這是不是在為舊形狀找位置？

不是。頁保護不依賴任何現有模組，針對的是機制自己的驗證層的已知上限（L1、L6），形狀由使用者 2026-11-11 定；它不替代審查（L2～L5 仍靠審查），也不替代兩層驗證（只補寫入動作這一面）。

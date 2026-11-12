# `-spike-pageguard` 報告：頁保護作為存取驗證的第三層

日期：2026-11-12。Owner：架構（kernel 研究線）。事前宣告：[spike-pageguard-declaration.zh-Hant.md](spike-pageguard-declaration.zh-Hant.md)。程式：`spikes/kernel/`（`PageGuard.hpp`／`src/PageGuard.cpp`／`tests/PageGuardTests.cpp`，commit `85312dc2`＋`a1e82e46`）。證據：主 checkout `build/evidence/kernel-pageguard/`（git 管理外；含實作者兩輪、敵對檢查報告 `adversary-report.md`、主會話驗證 `verify-main-session-r2.txt`）。

件数の突き合わせ：一致——測試新加 19（第一輪 16＋r2 3）／既有 33，debug 目標 52 過、release 目標 51 過 1 skip（Cost 只在 debug）；突變新加 7／killed 7，全目錄 41/41；本機 Intel Mac 兩組態 ctest tests 2／failures 0（實作者、敵對檢查者、主會話三方各自重跑一致）。**三平台：待 PR #184 的 L1 artifact `kernel-spike-tests.xml`**，本報告不以本機代替。

## 判定（照宣告原文）

| 判定 | 結果 | 證據 |
|---|---|---|
| 1 攔截（L1、L6、(b1)–(b3)） | **不成立（照原文）**：L1 ✓、(b1) ✓、(b2) ✓、宣告形狀的 L6 ✓；**(b3) ✗**——Resource 不在保護頁上，頁保護一筆都沒記（測試 `PageGuard Bypass (b3), not caught` 固定現況） | `ctest-*config.txt`；敵對檢查 B.1：違規的欄 id 與位址斷言有檢出力（探測突變 `adv-address-not-recorded` 被殺）、第二層關掉時 16 案例仍過（`adversary-layer2-off.txt`）、L1 的 volatile 寫在 `-O3` 仍到記憶體（`adversary-asm-L1-O3.txt`） |
| 2 繼續執行、單次記錄 | 成立 | Continue 三案例；敵對檢查重跑確認；>16 欄的 dropped 路徑 r2 加測試 |
| 3 三平台 | 本機 Intel Mac AppleClang 兩組態過、零警告；**三平台待 L1** | `build-*-r2.txt`、`verify-main-session-r2.txt`；敵對檢查讀碼未見會讓 gcc／MSVC 紅的點（B.5） |
| 4 成本 | 成立（門檻：單次 ≤ 10 µs、切換 ≤ 50 µs、全欄 ≤ 1 ms） | 見下表 |
| 5 突變 | 成立：宣告五條全 killed，且各由宣告的性質殺掉（不是順便） | `adversary-mutation-probe.txt`；突變 4 以 SIGABRT 非零退出＋訊息判 killed，照宣告 |

判定 4 的數字（µs，中位數，1000 次，64 獨占欄各一頁，頁 4 KiB；三方量測）：

| 量測者／組態 | protect（單次） | switch（(c) System 2 解除＋2 重保護） | full（Run 進入＋結束，128 次） | zone_b（(b) System 全開全關，128 次） |
|---|---|---|---|---|
| 實作者 Debug | 1.71→r2 1.91 | 6.30→6.88 | 207→202 | — →201 |
| 實作者 Release | 1.99→1.99 | 7.25→7.23 | 197→199 | — →193 |
| 敵對檢查 Debug／Release | 2.86／2.24 | 10.4／7.1 | 271／200 | — |
| 主會話 Debug／Release（r2） | 2.13／2.07 | 7.95／7.28 | 210／201 | 205／196 |

記憶體（1000 欄）：Intel Mac 4 KiB 頁 ≈ 3.9 MiB；Apple Silicon 16 KiB ≈ 15.6 MiB；Windows commit 4 KiB ≈ 3.9 MiB 但每欄保留 64 KiB 位址空間 ≈ 62.5 MiB。

### 判定 1 的讀法（使用者 2026-11-12 決定：**(i) 照原文**；AGENTS §14：判定不成立先報告，不改測試去湊）

宣告自相矛盾（架構師的錯）：判定 1 列了 (b3)，範圍の限界卻寫「Resource 槽與事件緩衝不保護」。(b3) 在這個範圲下不可能成立。另外宣告 §5 的 L6 測試形狀（host `GetMut` 碰過欄後、**System 內**經存下指標寫）第二層本來就攔得到；LIM-3 原始的 L6（host 在 Ready 經存下指標寫）頁保護**攔不到**（Ready 期間不設保護；測試 `PageGuard Limit (L6)`）。

- **讀法 (i) 照原文（採用）**：判定 1 不成立。頁保護暫不進 A.6，LIM-3 的 L1／L6 都不標 Lifted；下一批要重新宣告、重跑同一組測試才能用。
- 讀法 (ii)（未採）改讀為「欄的 Bypass」（L1、(b1)、(b2)、宣告形狀的 L6）：成立；(b3) 降為 Limit。是修宣告矛盾，不是放寬既有判定，但屬「看結果後改判定文字」，必須使用者明示並在宣告檔標為「宣告錯誤的修正」。
- 兩種讀法下對 A.6 的建議相同：**L1 可由頁保護補（限 Exclusive 欄、(c) System、開關開）；L6 不能**。

## 做了什麼（與宣告的差異）

實作照宣告 §1～6；spike 原本沒有的加了最小版本：`WritePolicy`（Exclusive 預設）、`Zone`（**預設 C，違反規格 S-6「無預設」**，為不動既有呼叫點；比 S-6 嚴不鬆）、`Executor::Run`＋`RunResult`／`Violation` 清單。差異：
1. Run 內第二層的差異收進 Err（原本斷言），否則 debug 目標的 (b1)/(b2) 會先被斷言丟出；**第一層仍在 Run 內斷言、未收進 Err**（敵對檢查 S4；既有 33 測試依賴它；規格要求兩層都以 Err 回傳——spike 與規格的差距，留遷移批）。
2. Run 在第一個 Err 停（對應 Interrupted）。
3. 「保護 API 呼叫計數」算的是 World 對每欄的保護變更請求（含無儲存的欄），讓「關閉時計數 0」能驗到早退。
4. 同欄同呼叫第二次故障 → abort（保護沒解除的唯一可觀測結果；突變 4 因此確定）。
5. `RunContext` 固定 16 槽，超出只計數並於返回後全部重保護。
6. CMake 加 `DOCTEST_CONFIG_NO_POSIX_SIGNALS`：doctest 每案例換 SIGSEGV handler 會搶走故障（Linux 的故障是 SIGSEGV）；代價是 crash 時沒有 doctest 的致命信號報告。
7. L1／Continue 的寫入用 volatile（否則 `-O3` 可能折掉「寫了再寫回」）。
8. r2（敵對檢查處置）：外來故障直接交給前任 handler、自己保持安裝，`EnterRun` 檢查處置是否仍是自己、不是就重裝並計數（`HandlerStats::reinstalls`——「狀態進證據」）；`Violation` 帶 World 身分、跨 World 的故障欄也重保護；加「Write 宣告者返回後重保護」「>16 欄 dropped」「跨 World」三測試與兩突變；Cost 加 (b) System 成本。

經緯：第一輪約 45 分鐘、r2 約 15 分鐘（宣告估 6 h）。macOS `raise(SIGSEGV)` 的 si_code 是 0 不是 `SI_USER`，實作者以探測程式發現並修正（`SentByProcess`）。本機 Intel Mac 的保護故障是 **SIGBUS**（SIGSEGV 與 SIGBUS 都要裝；探測突變證明只裝 SIGSEGV 行程會死）。

## 敵對檢查（Fable／xhigh，只讀；`adversary-report.md`）

0 blocker／7 should／4 nit。S1 外來故障後 handler 永久卸除、靜默失效（**r2 已修**，重跑對抗方的 `prev_handler` 探測：上一輪 exit 138 → 現在兩次 Run 都攔到）；S2 跨 World 故障無 World 身分、`Print` 越界、另一 World 的欄保持可寫（**r2 已修**）；S3 「返回後重保護」與 dropped 路徑無行為測試（**r2 已修**；刪 `ProtectFaulted` 的突變只被跨 World 測試殺到——本 World 的解除因「下一次 Run 全部重保護」不可觀測，寫在 mutations.json note）；S4 第一層仍斷言（寫進差異）；S5 成本模型缺 (b) 與頁數尺度（**r2 量了 (b)**；大欄的頁數成本未量）；S6 宣告的 L6 形狀不是 LIM-3 的 L6（本報告如實記）；S7 HANDOFF 的判定 4 組態標反（已改）。N1 Zone 預設 C；N2 constinit 註解過度宣稱（r2 改）；N3 POSIX 分不出寫入與執行故障（r2 註明）；N4 區間表 4096 上限。

## 範囲の限界（本批後）

- L1：**本機頁保護開著時攔得到**（限 Exclusive 欄、(c) System）。LIM-3 不變，解除要等三平台綠、規格 A.6 收進第三層、遷移批實作通過同組測試（判定 1 的讀法已定為 (i)：本批不解除任何一條）。
- L2、L3（r2 加測試 `PageGuard Limit (L3)` 固定）、L4、L5、L8：不變。
- **L6：未解除**。原始形狀在 Ready 不受保護；要攔它，host 的寫入 API 得是閉包形（呼叫期間解除、返回後重保護），`GetMut` 回 `T&` 的形狀做不到——A.6／W-6b 層級的設計問題。
- L7：其他執行緒故障時 `tlCurrent` 為空 → abort；macOS TLV 在該執行緒第一次存取可能 malloc；跨執行緒寫別的 World 會解除那欄直到該 World 下一個保護步驟。
- 新增：Resource 槽與事件緩衝不保護（(b3) 攔不到）；(b) System 與 Ready 的寫入只靠第二層；單獨 `System::Run` 不啟用；第一層仍斷言；區間表無鎖讀／有鎖寫在多執行緒是競態（Scheduler 批前要換成一次 atomic 發佈的不可變陣列）；`mprotect` 不在 POSIX async-signal-safe 清單（實務可行）；不在 sanitizer 下跑、doctest 的 POSIX 信號處理關閉；Windows 與 gcc 本機未驗（L1 判）；Windows 每欄 64 KiB 位址空間；dropped 的別 World 欄不重保護；POSIX 把對保護頁的指令擷取當成寫入。

## 對規格的建議與問題（給 A.6、LIM-3；不在本批決定）

1. **第三層的範圍**：頁保護只對「Exclusive 欄、(c) System、開關開」解 L1；寫進 A.6 時要如此限定，不寫成「攔所有繞路寫」。
2. **Resource 要不要保護**：Running 期間 Resource 不搬位址，可照欄的方式做，代價每個 Resource 至少一頁；決定後 (b3) 才有結論。
3. **(b) System 的成本是 2×|Exclusive 欄|**（64 欄 ≈ 200 µs；1000 欄 ≈ 3～4 ms／每個 (b) System）：規格若採用，`ColumnInfo` 層級的開關要同樣適用於頁保護，或讓 (b) 不全開全關（只開它宣告寫的欄＋Shared）。
4. **與其他 SIGSEGV／SIGBUS 使用者共存**：r2 的串接策略（交給前任、保持安裝、`EnterRun` 重裝並計數）要寫進 A.6，`reinstalls` 進「驗證開著」的證據。
5. **多 World**：違規帶 World 身分；跨 World 的解除由 Executor 重保護；區間表改不可變陣列原子發佈。
6. **欄重新配置與保護狀態**：spike 的 `Reserve` 只在 Ready；規格 F.2 在 Run 內套用延後命令會讓欄成長，新頁必須依目前狀態設唯讀、`AddRange` 要在 handler 可能同時讀表時安全。
7. **`tlCurrent` 為空的故障**：現在 abort（GYO_ASSERT 形狀）。歸 GYO_ASSERT 還是比照呼叫前比對歸 Kernel 層級 Err？「一個事件只有一種性質」下要定一邊。
8. **第一層改 Err**：spike 的第一層仍斷言，規格要求 Err；遷移批實作時直接照規格。
9. **頁大小與粒度**：Apple Silicon 16 KiB、Windows 64 KiB 保留；小欄浪費 100～1000 倍；規格的記憶體估算分平台寫。
10. **平台層的位置**：handler／mmap／VirtualProtect 進 Engine 時應放 OS 子系統（OS 層原意＝adaptor），Kernel 只持介面。
11. **第二層能不能減**：不能整層拿掉（Shared 欄、Resource、事件、(b) System、呼叫前比對、開關關閉都靠它）；頁保護開著時 (c) System 的呼叫後比對可跳過受保護的欄。

# 輸入與呈現：交接

更新：2026-10-22（ip3c 判定變更核准，D35）；2026-10-08。**狀態：IP-3 進行中（ip3b）；IP-2 完成（縮小交付），PR [#53](https://github.com/yojinn-io/GYO-Engine/pull/53) 已合併（`db1815c`）；IP-1 完成，PR [#50](https://github.com/yojinn-io/GYO-Engine/pull/50) 已合併（`56e0033`）。** 2026-10-04 由提出需求的消費端在其規劃批次中建立。
2026-10-05：消費端完成第3項的重現與阻塞取得下的量測基線（B0），摘要見第3項「消費端重現（B0）」；縮放時的停頓位置與先前的前提不同，見未結事項。
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。

## 閱讀入口

1. [進度與執行規則](README.md)。
2. [批次計畫](PLAN.md)：IP-1、IP-2 的範圍、驗收、平台表、Architecture Delta、停止條件。
3. 本文件：正式項目清單（現況、證據、方向）、決策、消費端需求、未結事項。
4. 相關 Engine 計畫：[基礎後續整理](../foundation-followups/README.md)（FF-2 是 IP-2 的前置；遷移清單在 [foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md) 的「IP」節）。
5. 消費端的需求以文字摘要寫在本文件「消費端需求」節；依 D12，本夾不連結消費端的文件。

## 決策紀錄

| 日期 | 決策 | 來源 |
|---|---|---|
| 2026-10-04 | D0：Engine 平台的兩個項目（渲染阻塞主迴圈、輸入層缺口）與其他群組一起納入消費端的規劃 | 使用者 |
| 2026-10-04 | D1：Engine 工作放在兩個 Engine 計畫夾（本夾 IP、foundation-followups FF），正式清單移到 Engine 夾；消費端的交接只保留自己的項目並連結到這裡 | 使用者 |
| 2026-10-04 | D7：渲染阻塞先量測再選修法。先拆開量 fence 等待與 `nextDrawable`；停頓在 `nextDrawable`（SDL 內部）就停下，帶證據請使用者在「視窗移動／縮放期間暫停 acquire（用 IP-1 的視窗事件）」與「分執行緒」之間選擇 | 使用者 |
| 2026-10-04 | D8：ultracode 與高於主對話的檔位，逐批開始時說明並徵求同意 | 使用者 |
| 2026-10-04 | D11②：文字輸入與剪貼簿維持候選，不在 IP-1 範圍 | 使用者 |
| 2026-10-04 | D11⑧：`object_fps_preview` 在 `engine/config/tools.csv` 停用（由 FF-6 執行）；它的輸入遷移需求只寫進遷移清單 | 使用者 |
| 2026-10-04 | D11⑩：驗收平台比照消費端既有的驗收：實機只有 macOS Intel／Metal；CI 四平台跑 L1；GPU 測試只在 Linux lavapipe；Windows／Linux 實機、macOS arm64 實機標「未執行」 | 使用者 |
| 2026-10-04 | D12：Engine 計畫文件（本夾與 foundation-followups）完全匿名、不連結：一律寫「提出需求的消費端」，不寫產品名，不連結消費端的文件。程式檔案路徑與證據路徑作為資料保留。反向（消費端文件連到本夾）不受限 | 使用者 |
| 2026-10-04 | D13：IP-2 的 L2 必須在合併前完成。消費端的呈現分析器必須修改才能處理「`Skipped` 變常見」時，IP-2 停下；先由消費端做驗收工具修正批（在 IP-2 的 base commit 上，仍是阻塞取得），再回到 IP-2 跑 L2。消費端重取 B1 時的停止路徑只適用於「IP-2 的 L2 已通過，但重取 B1 時才暴露問題」 | 使用者 |
| 2026-10-04 | 合併順序只是建議（減少衝突、方便歸因），不是依賴；後合併的一方 rebase。例外：FF-2 必須先於 IP-2；FF-7 依賴 IP-1、IP-2、FF-4、FF-5 | 主對話依既有決定定案 |
| 2026-10-04 | IP-2 的 before 在 IP-2 自己的 base commit 上以同一流程重量，B0 只作歷史參照；IP-2 的實機量測不與消費端改用輸入那一批的量測同時進行；檔位 high、局部 xhigh | 主對話依既有決定定案 |
| 2026-10-05 | D16：IP-2 同時處理拖動標題列（render 側）與縮放（事件泵側）的停頓。擴大後的計畫見 [PLAN](PLAN.md) 的「2026-10-05 範圍擴大」節（IP-2a 拆分量測、IP-2b live frame、IP-2c 拖動），確認後開始 | 使用者 |
| 2026-10-05 | D17：IP-2 的取得／節流／`Skipped` 語意與 live frame 的重入／時間連續性，交給 1 個 xhigh 子 agent 做對抗式審查（高於主對話檔位，依 D8 經使用者同意）；實作本身用 high | 使用者 |
| 2026-10-05 | （編號）本夾 2026-10-05 新增的決策原本編為 D14～D16，與消費端交接既有的 D14、D15 撞號，同日改為 D16～D18；commit 訊息中的舊編號不改 | 主對話 |
| 2026-10-05 | D18：PR 以功能線為單位，盡量少開（每次合併觸發全量 CI）。IP-2a～c 同一個 PR；合併後的狀態同步併進同一條線的 PR，不另開 docs PR | 使用者 |
| 2026-10-05 | D19：L2 之後，D7 選 B（分執行緒），但**延到消費端的下一個版本**另立計畫，不在 IP-2 實作：按下縮放角但尚未拖動的空窗（macOS 不送 live resize 通知）與 `nextDrawable` 停頓，在單執行緒上無法一一補齊。主執行緒（事件＋畫面）、模擬、網路三個角色的分離屬新的 Engine 計畫。IP-2 只交付 2b 與取得的拆分診斷；2c 不實作 | 使用者 |
| 2026-10-08 | D27：修訂 D19。多角色（主執行緒＝事件＋畫面、模擬、網路）的執行緒由 [時間、執行緒與 Trace](../time-threads-trace/README.md) 的 `GYO::Threads` 與 `GYO::Time` 提供；角色本身（迴圈、交接、政策）由消費端在產品內做。依 SDL 3.4 的契約，swapchain 的 claim／acquire／present 與事件 pump 只能在視窗執行緒（`SDL_gpu.h:4066-4094`、`:4202-4218`），所以不另開 render 執行緒 | 使用者 |
| 2026-10-08 | D28：SDL 隔離與顯示接續在本計畫，編為 IP-3～IP-5（owner 模組相同；IP-5 另含 `engine/io` 的原子寫入），不另開計畫夾 | 使用者 |
| 2026-10-09 | 消費端 D41 的連帶：消費端在這個版本以命令列選項讓玩家選擇解析度、視窗模式與 FPS 上限，設定 UI 與設定的保存移到下一個版本。IP-5 的 `UserDataDirectory` 與 `engine/io` 的 `AtomicWriteFile` 只服務設定的保存，延後到那時；其餘項目在 IP-5 開始時依「只有命令列選項」重新核對 | 使用者 |
| 2026-10-14 | 消費端 D76 的連帶：IP-3、IP-4 的第一個消費端（去 SDL 的遷移）移到消費端的下一個版本（重構版）；IP-5 的第一個消費端（以命令列選項選擇解析度、視窗模式與 FPS 上限）移到再下一個版本，和設定 UI 與保存（D41 的連帶）一起做。IP-3～IP-5 的範圍與檔位不變，不重新規劃；時點跟著第一個消費端。IP-3 仍依賴時間、執行緒與 Trace 計畫的 TT-2（它的消費端也移到同一個重構版） | 使用者 |
| 2026-10-21 | D29：IP-3、IP-4 與提出需求的消費端的遷移同一個 PR，IP-3、IP-4 是 PR 內的里程碑（各自寫 dev_log）。依據：消費端既有的決定「標頭去 SDL、PRIVATE 連結與產品遷移必須一起合併」，以及消費端 2026-10-21 核准的 PR 線；符合 D18 | 使用者（經消費端的規劃） |
| 2026-10-21 | D30：IP-3／IP-4 的規劃用 ultracode（2 位盤點、3 個獨立方案、1 位評審合成），公開介面由 1 位 xhigh 審查（README 的 IP-3 列），另做 1 次對抗式檢查；依 D8 經使用者同意 | 使用者 |
| 2026-10-21 | D31：核准 IP-3／IP-4 計畫（[PLAN](PLAN.md) 文末），U1～U10 全部照建議：接受約 2 倍的估時（各子批開始前重估，超過 1.5 倍就停）；ui_editor 做機械修改；README 以外新增的 6 項；時間戳型別用 `std::chrono::steady_clock::time_point` 並 `static_assert` 與 `Time::TimePoint` 相同（不新增 Input→Time 邊）；`NativeEventObserver` 由 `PlatformEvent` 承接；凍結 oracle 改 golden（走 §14.4 的核准流程）；X11 加中立查詢；新建英文的 `platform-sdl.md`；capture 放 GPU 後端；IP-4 的介面另做 1 次 xhigh 審查。另：設計提前在 ip3a 寫成架構文件（`docs/architecture/platform-sdl.md`、`input.md`） | 使用者（「照計劃執行，記得把這個設計落到架構文件中去」） |
| 2026-10-21 | D32：核准 ip3a 的事前宣告（[ip3-declaration.md](ip3-declaration.md)）；之後不放寬。消費端的 A 級受擊回饋行改走 Base 的日誌 facade、位元組不變（消費端的決定）。開始 ip3b | 使用者 |
| 2026-10-21 | D33：巢狀 `PumpEvents` 保留（IP-2 已保證並有測試）。時鐘配對隨 pump 範圍保存與還原：每次 pump（含巢狀）各自取配對，內層返回時還原外層的配對。修改 [platform-sdl.md](../../platform-sdl.md) 的 Design Invariant 7 與時間戳契約（使用者同意的 Design Invariants 變更）；事前宣告補一條判定與突變 m-nest（只收緊）。L1 通過後開始 ip3c | 使用者 |
| 2026-10-22 | D34：ip3c 的判定變更選 (b)：`SampleClockPair` 加讀取注入的接縫，用精確等式守住讀取順序與取中點，再照 τ（測試量到的走差＋兩組 bracket＋2 ns）放寬；照 §14.4 重做突變與獨立核對後交使用者核准。時間戳契約條文 (2) 改成「SDL 時間非零且不遞減的事件，換算後不遞減」（使用者同意的契約文字變更） | 使用者 |
| 2026-10-22 | D35：核准 ip3c 的判定變更（[ip3c-judgment-change.md](ip3c-judgment-change.md) 第 3 版，§14.4 的 1～5，獨立核對 3 輪）：整合判定的容許由 `β/2` 改為 `τ = |drift| + P0.bracket + P1.bracket + 2 ns` | 使用者 |
| 2026-10-22 | D36：ip3e 的 P1～P3。P1、P2 合成一條：`docs/architecture/error-handling.md` 加「平台邊界的例外」條款並註明理由（兩條規則都是為了不讓程式錯誤在測試中消失；在 Engine 直接呼叫 C 函式庫的那一層照字面遵守反而會讓錯誤不可見或行為未定義），允許解構子以 GYO_ASSERT 檢查自己持有的 C 全域狀態（解構子維持 noexcept，以 death test 驗證），以及進入點與 C 回呼使用 `catch (...)`（必須記錄並轉成明確的結果）。P1 的實作：SdlLogBridge 解構子回到 noexcept，斷言的測試改 death test。P2：RunEntry 不改。P3：宣告的「每一輪守恆」改為「逐執行緒守恆」，§14.4 的 1～5 見 [ip3-declaration.md](ip3-declaration.md) 第 6 節 | 使用者（經分支會話轉達：「其實P1和P2是一樣的問題呀。」「嗯，可以，但是需要註明為何會加這個條款。（我的理解是，P1、P2主要是測試的問題。如果不改規則，會讓程式錯誤不可見，就無法測試。」；本會話：「處理完P1-P3後，處理ip3f，L3排最後。」） |
| 2026-10-22 | D37：IP-4 介面審查（U10）後的修正。Design Invariant 7 改寫：查詢類（`QueryWindow` 等）在 observer 與 live frame 內允許；操作類在 observer、live frame、watch 回呼內斷言；`AddEventWatch`、`EventWatch` 解構、注入在 live frame 與 watch 回呼內斷言（thread_local 的分派深度）；`EventWatch` 只記 window ID、不參照 platform，可以比 platform 晚解構。回傳型別：`QueryWindow` 回 `WindowState`、`Raise` 回 void、`Sync` 回 bool、`AddEventWatch` 回 `EventWatch`（記憶體不足丟 bad_alloc）；刪除 ip4b 的「`AddEventWatch` 失敗路徑」驗收（§14.4 的放寬，使用者核准）。其餘審查意見（M3～M8）照主對話的處理 | 使用者：「核准：DI 7 照建議改寫（H1 查詢類／操作類分開、H2 watch 不參照 platform、M1 分派深度的斷言）；M2 照建議改回傳型別、刪除 AddEventWatch 失敗路徑的驗收。其餘意見照你的處理，宣告後做 ip4a。」 |
| 2026-10-22 | D38：ip4b 的 xhigh 審查之後。F4：時間戳判定採 [ip4b-judgment-change.md](ip4b-judgment-change.md) 的方案 B（區間＋兩條精確等式；§14.4，獨立核對後）。F10：Design Invariant 7 加「`EventWatch` 只在主執行緒解構」（`GYO_ASSERT(SDL_IsMainThread())`＋death test `watch-off-main`；SDL 結束事件子系統時不持鎖就銷毀 watcher 清單）。Design Invariant 3 的守衛清單補上 watch 路徑 | 使用者：「核准：F4 採方案 B（區間＋兩條精確等式）；F10 加 DI：EventWatch 只在主執行緒解構（GYO_ASSERT＋death test）；DI 3 守衛清單補上 watch 路徑。然後提交、推上 PR #85。」 |

## 正式項目清單

本夾是以下兩項 Engine 部分的正式來源。原本記在消費端交接的第3項與第11項，2026-10-04 移入並套用規劃時查證的更正。
數字未標「實機」者為靜態分析。

### 第3項：Engine 渲染阻塞主迴圈（macOS）→ IP-2

- 來源：消費端前一版的 L3 人工驗收（2026-10-02），使用者在拖動、縮放視窗後看到 `CONNECTION POOR`。
- 證據（實機，macOS Intel／Metal）：
  - 檔案：`build/target/_build/test/logs/pvp-v5-batch04-20261002-manual-2/client-1.log`、`client-2.log`，以及 `pvp-v5-batch04-20261002-manual/client-2.log`。git 忽略；原本只在另一個 worktree，消費端在規劃批次中複製到本 worktree。SHA-256：
    - `manual-2/client-1.log`：`6ce77f739cf6aba5edf0180c60378805ef63d833650467bc24e50d4b28963d80`
    - `manual-2/client-2.log`：`e952471518e7a6ab024b3daa5cd95799cbd177d7a104f4ac71505f91ca208a12`
    - `manual/client-2.log`：`33b0f4a9e730dd5f77217e3cedd778c77207938cd2617eadf2ce765d5dcc9b39`
  - `manual-2` 兩個 client 合計 12 筆 render_ms ≥250 ms，範圍 362.8–1199.5 ms（不是早先記錄的 5 次）：
    - client-2：1199.0、1198.8、698.6、1199.5、785.1。
    - client-1：385.0、365.2、363.9、370.9、364.9、881.4、362.8。
    - `manual`（第一組）client-2 另有一次 1134.2 ms。
  - 停頓全在 render_ms 內，事件泵沒有另外停住。例：render_ms=1199.0 的幀 frame_gap_ms=16.9；下一幀 frame_gap_ms=1202.1、render_ms=0.7。
  - 所有長幀都記為 `presented=1`。
  - client-2 的 5 筆中有 4 筆，在停頓幀之後約 3–4 ms 記錄到「釋放指標」，也就是視窗互動事件在 render 返回後才被事件泵處理。
- 消費端重現（B0，2026-10-05，實機 macOS Intel／Metal，阻塞取得，來源與 2026-10-04 的計畫基準相同，沒有合併任何 IP／FF 批次）：
  - 證據：`build/target/_build/test/logs/pvp-v6-batch04-20261005/7-manual/`（git 忽略）：兩個正式 Client 與 Match 的日誌、movement trace（三者都有 `trace_end`）、`commands.txt`、`artifacts.sha256`。
  - 操作（使用者）：一個 Client 拖動標題列 3 次；另一個 Client 拖動右下角縮放 3 次；兩個視窗並排。
  - **拖動標題列**：3 次中 2 次出現 render 停頓，`render_ms` 1196.8、1196.9 ms，`presented=1`；同一幀 frame_gap_ms 約 17 ms，下一幀約 1199 ms。與前一版的證據同型（停頓在 render 內）。第 3 次沒有 ≥100 ms 的幀。
  - **縮放**：停頓**不在 render**。消費端在事件處理（`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:765-780`，計時涵蓋 `SdlPlatform::PumpEvents` 的 `SDL_PollEvent` 迴圈與消費端自己的事件回呼）記錄到 394.9、2974.4、1006.4 ms，該幀 `render_ms` 約 1 ms。
    另有一次 `render_ms` 477.6 ms（同一時刻另一個 Client 也有 212 ms 的幀間隔），對應哪個操作未確認。`SDL_PollEvent` 與回呼各占多少沒有拆分量測。
  - 替代比例（由 Match 的 movement trace 依權威 Tick 每 600 Tick 重算；窗口起點不保證與 Host 內部的品質窗口相同）：
    - 拖動的 Client：兩個窗口各 5.7%。
    - 縮放的 Client：兩個窗口 14.8%、6.8%（門檻 5%）。
    - Match 共 4 次 starvation reset，兩個 Client 各 2 次。
    - 使用者這次沒有看到 `CONNECTION POOR`；trace 重算的窗口超過門檻與畫面不一致的原因未查。
  - 長幀之外的幀時間分布：兩個 Client 的 presentation P50 8.3 ms、P99 17.3 ms（trace 的 `frame_seconds`）。
- 原因：
  - `engine/render/backend/sdl_gpu/src/SdlGpuRenderDevice.cpp:937` 以阻塞的 `SDL_WaitAndAcquireGPUSwapchainTexture` 取得 swapchain。
  - 主迴圈單執行緒（`engine/runtime/src/RuntimeLoop.cpp:18-44`），render 停住期間不處理事件、不更新。
  - 消費端的後果：停頓期間不產生移動命令；1.2 秒約 72 Tick 被 Host 以 Held 替代，占 10 秒判定窗口 12%（門檻 5%），該窗口不合格而顯示警告。警告判定正確；移出需要連續 3 個不合格窗口，單次停頓不會移出。
- SDL 3.4.0 Metal 的事實（更正「改用不阻塞取得就能解決」的前提）：
  - `third_party/sdl3/CMakeLists.txt` 為 `GIT_TAG release-3.4.0`。
  - `SDL_gpu_metal.m:3880-3899`：不阻塞模式只略過 fence 等待。
  - `:3908`：兩種模式都呼叫 `[layer nextDrawable]`；沒有可用 drawable 時最多阻塞約 1 秒，與 1199 ms 吻合。
  - `:3922`：一律回傳非空的 texture container，所以 Metal 長幀仍回報 `Presented`。
  - render_ms 同時包含 `AcquireFrame` 與提交，兩者的拆分尚未量測。
- Engine 已有的路徑：`SdlGpuRenderDevice.cpp:942-946` 回傳 `std::nullopt` → `engine/render/src/Renderer.cpp:318` 轉成 `PresentStatus::Skipped`。
- 影響範圍（更正）：ui_editor 使用 SDL_Renderer，不受影響。目前啟用的 sdl_gpu 消費端只有提出需求的消費端；另有共通 GPU 測試（`MeshUpdateSmoke`）與未啟用產品。
- 跨平台：
  - CI 四平台＝windows-x64、linux-x64、macos-arm64、macos-x64（`build/ci/common/app_registry.py:28-36`）。
  - GPU 測試只在 Linux lavapipe 執行（`.github/workflows/build-and-validate.yml:219-220`）；macOS 只做 Metal 離線編譯；Windows D3D12 沒有執行。
  - Windows 拖動標題列可能進入 Win32 modal 迴圈，停頓可能落在事件泵，根因可能不同。沒有證據也沒有量測。
- 方向：依 D7。先拆分量測；停頓在 fence → 不阻塞取得＋`Skipped`＋節流；停頓在 `nextDrawable` → 停下請使用者選擇（暫停 acquire／分執行緒）。詳見 [PLAN](PLAN.md) 的 IP-2 節。
- 對應批次：IP-2（修正）。重現與基線由消費端提供（B0、B1）。

### 第11項：Engine 輸入層的缺口（Engine 部分）→ IP-1

- 來源：[架構漂移健檢（2026-10-04）](../../../checkup/architecture/2026_10_04_architecture.zh-Hant.md)「應收進 Engine 的功能」中優先度最高的一項；使用者 2026-10-04 決定處理。
- 現況：
  - `engine/input/include/engine/input/PhysicalInputFrame.hpp:8-24` 的 `Key` 是 14 鍵的封閉清單，沒有 Tab 與數字鍵。過去 F3、H、R 都是為了產品需求直接改 Engine 的 enum 加進去的。
  - SDL 轉換在 `engine/input/backend/sdl/src/SdlInput.cpp`，不在 `engine/platform/sdl`。
  - 失焦已由 `PhysicalInputFrame::windowFocused` 提供（`SdlInput.cpp:80,99-107`）。
  - `SdlInput` 已對鍵盤、滑鼠、焦點事件比對本視窗 ID（`SdlInput.cpp:96-139`）；`SdlPlatform::PumpEvents`（`SdlPlatform.cpp:61-83`）只對關閉請求過濾本視窗，傳給 observer 的原生事件不過濾。
  - `SdlInput.cpp:64-72` 的 `ApplyTransition` 在同一幀內按下又放開時，把 `pressed` 覆寫成 false，上升沿消失。
  - 真正缺的是：Tab、數字鍵等其餘按鍵；同一幀內的上升沿次數（或依序的事件清單）、事件先後順序、點擊座標與是否在視窗內；移動、縮放（含新寬高）、縮小、還原事件的 Engine 表示與本視窗過濾。
  - 兩種獨立消費端繞過 Engine：
    - 遊戲（提出需求的消費端）：在原生事件回呼中處理 Tab 切換指標擷取、失焦／移動／縮放／縮小時釋放指標、windowID 過濾、點擊擷取（只收視窗內的點擊），並從縮放事件更新視窗寬高。它以 `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:209-217` 的 `pendingShotEdge` 保留同一幀的點擊上升沿，這個判斷依賴它與 `windowInteraction`、`pointerAcquiredThisFrame` 的先後順序。
    - 工具 `tools/object_fps_preview/ViewmodelPreview.cpp:74-82`：直接讀 `SDL_SCANCODE_1..6` 等。preview 依賴停用的 `object_fps`（`requires_apps.game=object_fps`，選取時 `build/cmake/GyoTools.cmake:41` FATAL_ERROR），目前無法建置驗證。
  - `Key` 只在程式碼中使用，沒有資料檔以名稱引用；以加法擴充不涉及 Data Contract。
- `NativeEventObserver`（`engine/platform/sdl/include/platform/sdl/SdlPlatform.hpp:43,56`，FF-7 前的路徑）必須保留：
  - ui_editor 的 ImGui（`tools/ui_editor/src/ImGuiSdlRendererHost.cpp:218`）需要原生事件。
  - preview（`ViewmodelPreview.cpp:74`）與未啟用產品的 `ObjectFpsRuntimeClient.cpp`、`build/acceptance/object_fps*/` 也經由它。
  - `tests/common/runtime_sdl/main.cpp:44`、`tests/common/render/sdl_gpu/MeshUpdateSmoke.cpp:239` 呼叫 `PumpEvents()` 但不帶 observer。
- 方向：Engine 提供完整的鍵盤 scancode 與視窗互動事件；何時擷取或釋放指標由各消費端決定。以加法擴充。文字輸入與剪貼簿不在範圍（D11②）。詳見 [PLAN](PLAN.md) 的 IP-1 節。
- 與第3項同樣牽涉視窗事件：若 D7 選「暫停 acquire」，IP-2 會使用 IP-1 的視窗事件。
- 對應批次：IP-1（Engine）。消費端改用在其自身批次；preview 只寫遷移清單。

## 消費端需求（摘要）

提出需求的消費端是一個線上遊戲（依 D12 不寫產品名、不連結其文件）。本節以文字記錄它的需求，刪除該消費端後仍可讀。

- 輸入（IP-1）：
  - 需要 Tab 鍵與視窗互動事件（失焦、移動、縮放含新寬高、縮小），且只收本視窗的事件。失焦、移動、縮放、縮小都會讓它釋放指標；縮放時它也要更新寬高。
  - 需要每幀的按下上升沿次數（或依序的事件清單）與點擊當下的座標，以保留「同一幀內按下又放開」的上升沿；需要事件的先後順序（視窗互動、取得指標、點擊在同一幀內的先後會改變結果）；需要知道點擊是否在視窗內。目前它以原生事件自行保留上升沿（見第11項）。
  - 它自己決定擷取政策：Tab 切換擷取、點擊視窗內擷取、視窗互動時釋放。這些不進 Engine。
  - 改用後，同一串合成 SDL 事件必須產生相同的輸入狀態與命令序列。特別要比對 `SdlInput.cpp:94-107` 的兩個既有行為：取得焦點後壓制一次相對位移、失焦時 `ReleaseAll`。差異由消費端逐條事前宣告。
  - 它的位址輸入框使用文字輸入與剪貼簿，繼續直接處理 SDL（候選例外）。
- 呈現（IP-2）：
  - 拖動／縮放視窗時，主迴圈不能停到使 10 秒窗口的 Held 超過 5%。
  - 它已有 `Skipped` 的分支，但目前 `Skipped` 很少出現；`Skipped` 變常見後，它的呈現分析器是否仍正確，要在 IP-2 的 L2（合併前）確認。分析器必須修改時依 D13：IP-2 停下，消費端在 IP-2 的 base commit 上做驗收工具修正批，再回到 IP-2 跑 L2。
  - 它提供 macOS 的拖動重現與阻塞取得下的量測基線（B0，IP-2 只作歷史參照；IP-2 的 before 在自己的 base commit 重量），IP-2 合併後重取 B1。
  - 它改用 Engine 輸入那一批的實機量測，不與 IP-2 的實機量測同時進行。
- 權威：兩批都不得改變它的權威結果；它提供同機兩樹的權威 digest 比對作為附帶檢查。
- SDL 隔離（IP-3、IP-4，2026-10-21 規劃時整理）：
  - 它的完成條件以 SDL 符號判定：產品、它的驗收 probe 與產品測試都不直接使用 SDL（不是移除 SDL）；例外清單從空開始。只有它自己的 CTest 檢查這件事，只在選擇它時啟用。
  - 位址輸入框（只在大廳）需要文字輸入、剪貼簿，以及按住 Backspace、Ctrl+A／V、Return／Esc；文字與按鍵要依到達順序。
  - 下一版的音效要以輸入事件的時間戳排程，只需要 `InputEvent` 的時間戳。
  - 它的 probe 需要事件注入、帶時間戳的觀測、視窗與顯示器的查詢與操作、capture 存檔、相對滑鼠模式的查詢、平台指紋（video driver、refresh rate、X11 window number）。
  - 它現在把 SDL 的日誌 tee 到自己的日誌檔，也把 Engine 的日誌轉回 SDL；改走 Base 的日誌 facade 後，經 SDL 的行要和以前相同（它的日誌契約的 B 級）。
  - 它的 L2 在整個 PR 的 base 與頭交錯跑一次；它的工作、守衛與估時寫在它自己的計畫。

## IP-1 紀錄（記錄器）

- 2026-10-05：使用者離開前指示「把 Engine 側的功能完成」。分支 `claude/engine-ip1`，疊在 FF-5 上（`InputActionMap.cpp` 的 FF-3 已先完成）。
  檔位 high。計畫建議公開介面以 ultracode 審查，但依 D8 需要使用者同意，使用者不在，所以**沒有做 ultracode 審查**；改以下列約束自行設計，並在 PR 請使用者重點審查介面。
- 2026-10-05：介面決定（計畫中「介面審查時決定」的兩點）：
  - 完整 scancode 的表示：延伸 `Key` 列舉，在 `F3` 之後追加完整鍵盤（字母、數字列、F1～F12、編輯與導覽、修飾鍵、標點、ISO 與國際鍵含 JIS、數字鍵盤、系統鍵），原 14 鍵名稱與值不變；`MouseButton` 追加 `X1`、`X2`。
  - 同一幀的表示：選「依序的事件清單」。`PhysicalInputFrame::events` 是本視窗的事件，依到達順序：按鍵與滑鍵的按下／放開（滑鍵含座標與是否在視窗內）、失焦／取得焦點、移動（新位置）、縮放（新寬高）、縮小、還原。
    它同時涵蓋同一幀按下再放開的上升沿、事件先後與點擊座標；`pressed`／`released`／`held` 的語意不變（仍是該幀最後一次轉換）。
    按鍵重複不是事件；失焦時對按住鍵套用的放開是狀態變化，不是事件。
  - `NativeEventObserver` 保留；擷取政策仍在消費端。
- 2026-10-05：測試（`tests/common/input/SdlInputTests.cpp`）：
  - 全部 SDL scancode 掃一次，每個 `Key` 恰有一個來源（`Enter` 兩個）；原 14 鍵的值以 `static_assert` 固定、對應不變；
  - 同一幀按下再放開保留兩個事件與座標；視窗內外判定；
  - 視窗事件序列含資料；其他視窗的事件被過濾；
  - 失焦放開不算事件。
  突變（不記錄放開、縮放不過濾）都被抓到。CTest 56／56；沒有改 CMake，依賴邊不變；權威 digest 兩樹比對 35／35 相同（附帶檢查）。完成，開 PR。
- 2026-10-05：使用者指示依序合併。與已合併批次的文件衝突以合併 origin/master 解決，CI 四平台通過後合併到 master（`56e0033`）。

## IP-2 紀錄（記錄器）

- 2026-10-05：使用者決定 D16～D18 並確認擴大後的計畫；FF-7 先做（使用者決定）。分支 `claude/engine-ip2`，疊在 FF-7（`b2fda8c`）上；FF-7 合併後 rebase 到 master。檔位 high；xhigh 審查 agent 經使用者同意（D17）。
  - 審查 agent 的定義檔放在使用者層 `~/.claude/agents/xhigh-reviewer.md`（使用者決定）。
  - 這個定義要等 session 重新載入後才能使用，所以審查排在 IP-2c 之後，一次審 2b 與 2c。
- 2026-10-05：IP-2b 實作（commit `2f76e59`）：
  - `RuntimeLoop::RunLiveFrame`：在 `ProcessEvents` 進行中執行一組 `Update`／`Render`。
    - frame 編號以 `Update`／`Render` 的組為單位；delta 為與前一組開始的時間差。
    - 沒有 live frame 時，行為與先前逐一相同。
    - live frame 中要求 Stop 時，在 `ProcessEvents` 返回後結束。
    - `Run` 每次從 frame 0 開始。
  - `SdlPlatform::SetLiveFrameHandler`：只在設定了處理函式時才註冊 event watch。
    - 只處理本視窗的 `EXPOSED`（`data1`＝1）；先確認是主執行緒，才讀取平台狀態。
    - 只在 `PumpEvents` 執行中呼叫，而且不會重入。
  - 消費端接線（消費端的部分）：
    - live frame 中使用空的輸入幀：不消費按鍵、指標與 UI 輸入，也不切換相對滑鼠模式。
    - `pump` 前段的點擊與 R 邊緣保留給 `pump` 之後的正規更新。
    - 慢事件處理的日誌加上 `observer_ms`（消費端自己的處理時間）與 `live_frames`。
- 2026-10-05：IP-2a 量測碼：sdl_gpu 的取得拆成 `SDL_WaitForGPUSwapchain`（fence）與 `SDL_AcquireGPUSwapchainTexture`（drawable），兩者合計 ≥50 ms 時記錄 `fence_wait_ms`、`drawable_ms`。去留在量測後決定。
- 2026-10-05：L1：
  - CTest 57／57：新增 `platform_sdl_tests`；`engine_tests` 加了 4 個 live frame 案例。
  - 突變 9 個全部被抓到：
    - platform 4 個：不檢查 `data1`、`pumping_`、視窗、重入。
    - runtime 5 個：不檢查 `ProcessEvents` 中、正規組沿用 `ProcessEvents` 的 context（兩種寫法）、忽略 live 的 Stop、組開始時間不前進。
  - 「組開始時間不前進」一開始沒被抓到，所以補了 delta 上限的檢查。
  - 兩個突變一開始讓測試無限迴圈，所以替測試的假 client 加了呼叫次數上限。
  - 權威 digest 兩樹比對 35 個情境 0 不同（附帶檢查）。
- 2026-10-05：L2 準備：
  - before＝`b2fda8c`，在 scratch worktree `GYO-Engine-v6b04` 建置；after＝本分支。
  - 流程：雙 Client 並排，A 拖動標題列 10 次、B 縮放 10 次。
  - 使用者指定稍後進行。
- 2026-10-05：L2（macOS Intel／Metal 實機，使用者操作），事前宣告與證據在 `build/target/_build/test/logs/engine-ip2-20261005/`（`declare.txt`、`l2-before/`、`l2-after/`，各含日誌、trace、`artifacts.sha256`、`logs.sha256`）。
  使用者在兩輪中都對兩個 Client 做了縮放；A＝先加入的 Client。
  - before（`b2fda8c`，阻塞取得、沒有 live frame）：
    - 縮放：事件處理停頓 A 8 次（702～2293 ms）、B 6 次（346～3909 ms）。
    - 最差 10 秒窗口的替代比例：30.5%、46.5%；Match 有 13 次 starvation reset。
    - 使用者在兩個 Client 都看到 `CONNECTION POOR`。
    - 拖動標題列：沒有 ≥100 ms 的 render 停頓（B0 是 3 次中 2 次）。
  - after（本分支）：
    - 縮放拖動中，每次 pump 插入 34～366 個 live frame（扣掉開始前的空窗，約 60 Hz）；這段期間沒有長串的替代。
    - **按下縮放角到開始拖動之間沒有 live frame**：macOS 從按下就進入追蹤迴圈，但開始縮放才送 `windowWillStartLiveResize`。
      - 空窗：A 為 388、421、655、1338 ms，B 為 541、1838 ms。
      - B 另有兩次按住不動：3057 ms、4323 ms，0 個 live frame。
      - 使用者也觀察到「只按住不拖時畫面停住，真的縮放後才恢復」。
    - 最差窗口 7.7%、15.9%；Match 有 5 次 starvation reset。
      - 超過 5% 的窗口，替代都來自 21～29 Tick 的連續替代。
      - 這些連續替代與上述空窗一一對應；29 Tick 後 Match 以 starvation reset 中斷。
    - 使用者在兩個 Client 仍看到 `CONNECTION POOR`。
    - `nextDrawable` 停頓 3 次，`fence_wait_ms` 都是 0：
      - 1016.2 ms 與 1016.9 ms：兩個 Client 在 lobby **同一時刻**停住。
      - 461.8 ms：A，在 B 的 4.3 秒按住結束後。
      - 都不是拖動標題列引起；觸發原因未查明。拖動標題列兩輪都沒有停頓。
  - 判斷：
    - 2b 有效，但只涵蓋縮放拖動中。
    - 剩下的空窗與 `nextDrawable` 停頓，在單執行緒上無法消除；D7 的 A'（移動／縮放期間暫停 acquire）對這兩者都無效。
    - 使用者決定 D19。
- 2026-10-05：收尾：
  - 2a 的拆分改為常駐診斷（只在 ≥50 ms 時記錄），註解改寫。
  - 2c 不實作。
  - xhigh 審查只審 2b，需要 session 重新載入後執行。
- 2026-10-05：FF-7（#52）合併後 rebase 到 master `ef12037`，沒有衝突。
  - 使用者把主對話調為 xhigh，所以審查改用一般子 agent（繼承主對話檔位），不必重新載入 session。
  - before 用的 scratch worktree `GYO-Engine-v6b04` 經使用者同意已移除；證據中的 `artifacts.sha256` 記錄了當時的雜湊值。
- 2026-10-05：審查（一般子 agent，繼承主對話的 xhigh；D17）。結論：沒有高嚴重度缺陷；`RuntimeLoop` 的時間與編號、watch 的執行緒與生命週期、GPU 取得拆分在 Metal／Vulkan／D3D12 上與阻塞版等價，都確認正確。依結果修正：
  - 【中】live frame 用舊的視窗大小排版 HUD 與 lobby：SDL 送出縮放事件時已更新視窗大小，但事件要等 pump 返回才處理。修正：消費端在 live frame 前以 `SDL_GetWindowSize` 更新寬高。
  - 【低】handler 的例外會穿過 SDL 與 OS 的 modal 迴圈。修正：
    - platform 保存例外，pump 處理完這一輪後再 rethrow；出錯後同一輪不再呼叫 handler。
    - 消費端的 `liveFrame` 旗標與 handler 的移除改用 RAII。
    - 提早跳出輪詢會讓 SDL 的「一輪結束」標記留在佇列，截斷下一次 pump，所以改為處理完這一輪才 rethrow；這一點由測試發現。
  - 【低】旗標 scope 不能巢狀。修正：platform 與 `RuntimeLoop` 都改為還原舊值。
  - 註解：
    - Windows 開著系統選單時也會跑 live frame；
    - Vulkan／D3D12 的 `drawable_ms` 也包含 swapchain 重建；
    - 刪除「pump 前段的邊緣保留」這個不成立的前提。
  - 補測試：
    - live frame 之後下一幀的 delta 與共用 context；
    - 第 0 幀內的 live frame；
    - live Render 回 Stop；
    - live Stop 後再 Run；
    - handler 例外由 `PumpEvents` rethrow 且旗標復原；
    - 巢狀 `PumpEvents`；
    - 更換 handler 時只保留一個 watch。
  - 新測試的突變 5 個全部被抓到；CTest 57／57。
  - 未補：消費端 live frame 的單元測試（接線由 L2 驗證）、跨執行緒推送事件的測試。

- 2026-10-05：PR #53 的 CI 通過，經使用者同意合併到 master（`db1815c`）。

## IP-3／IP-4 規劃紀錄（記錄器）

- 2026-10-21：規劃（D30）。
  - 做法：Engine 端與消費端各 1 位盤點；3 個獨立方案（最小加法、風險優先、消費端遷移優先）；1 位評審評分（6.5／8.0／6.5）並以風險優先為基底合成草案；1 位 xhigh 審查公開介面；1 次對抗式檢查。各 agent 的原始輸出留在規劃會話的工作檔，不放進 repo；草案是 [PLAN](PLAN.md) 文末的 IP-3／IP-4 節。
  - xhigh 審查的判定：**有條件接受**。沒有任何一項推翻三個設計前提（中立的 `PlatformEvent`、每個後端一個 `*Native` 目標、每次 pump 一個時鐘配對）。必修 6 項已併入草案：
    - F1：時間戳的整合判定原本被 clamp 吸收，正負號反轉與 delta 處理錯誤的突變殺不掉。改成夾住 push 的窗口，另立「不晚於交付時刻」。
    - F2：日誌橋的語意（tee 或取代）、跨執行緒死鎖的前提、`SDL_Quit` 後鎖不存在、還原會蓋掉別人的設定。改成預設 tee、寫明前提、解構時先確認再還原。
    - F3：Native 目標的接線會造成循環；`injected` 旗標放在 input 後端會反向依賴。改成後端以 PRIVATE include 取得自己的 Native 標頭；旗標由 platform 擁有。
    - F4：時間戳的契約缺順序、timestamp 0、平台精度。改成四個可以檢查的條件與已知限制。
    - F5：`TextEvent` 缺按下／放開的區分、`layoutKey` 的語意與可測性、修飾鍵的型別。
    - F8：進入點的例外處理、argv 編碼、`PlatformSDLMain` 的連結關鍵字。
  - 其餘審查項目也已併入：F6（相對滑鼠模式移到 platform；文字輸入狀態不保存副本）、F7（`BaseDirectory` 的名稱、UTF-8 路徑、主執行緒）、F9（`PlatformEvent` 不可複製、pump 不可重入）、F10（觀測的命名對齊、失敗路徑、鎖與生命週期）、F11（注入的限制）、F12（守衛範圍）、F13（移除的符號表）、F14（DI／RC 的細節、U4 的第三個選項）。
  - 對抗式檢查：
    - 高：H1 同 F1；H2 PR 結構與消費端「一起合併」的既有決定衝突 → D29；H3 消費端的符號守衛沒有範圍與突變 → 移到消費端的計畫（掃描只看 C++、去掉字串與註解、加 SDL 的整數 typedef、連結圖兩層、宣告突變）。
    - 中：M1 消費端的工作清單寫進 Engine PLAN 屬 Ownership 漂移並違反 D12 → 移到消費端的計畫；M2 消費端曾決定移除產品公開的 `SdlPlatform&`，與 probe 的取得途徑衝突 → 消費端計畫的待決項；M3 範圍擴大沒有明列 → 「README 原列 vs 規劃新增」表（U3）；M4 估時偏樂觀 → ip3b 改 8～12 h、ip3f 改 3～4 h、IP-4 加總更正；M5 機率性測試 → ip3a 宣告；M6 同 F3；M7 oracle 改 golden 預設視為有放寬的一面（U6）；M8 DI 的歸屬（輸入幀的條文移到 input owner）與 RC 的矛盾（限定 pump 路徑）。
    - 低：capture 的位置（U9）、X11（U7）、`TextEvent.timestamp` 沒有功能需要 → 拿掉、測試縫只在 Native 標頭、IP-4 的介面審查（U10）、Metal 長幀的未結事項不宣稱、L2 門檻的對抗式檢查、日誌行的 B 級核對（ip3a）。
  - 估時：Engine 37～46.5 h，加消費端約 19～26 h；原本估 26～34 h，約 2 倍（U1）。
  - 停在 ip3a 關卡前，等使用者核准草案與 U1～U10。
- 2026-10-21：使用者核准（D31）。ip3a 開始（檔位 high，估 3～4 h）：設計寫成架構文件 `docs/architecture/platform-sdl.md`（SDL 後端的分層、契約、Design Invariants 9 條、Replacement Criteria 6 條）與 `input.md`（輸入幀的契約、Design Invariants 4 條、Replacement Criteria 3 條；對抗式檢查 M8：輸入幀的條文屬 `GYO::Input` 的 owner）。兩份都標明「已核准、尚未實作」，每條寫出由哪個里程碑成立、由哪個守衛守住。
- 2026-10-21：ip3a 完成，停在關卡（[ip3-declaration.md](ip3-declaration.md)，等使用者核准）。
  - 簽章定案的更正兩項（查 SDL 3.4.0 原始碼）：`ClipboardText()` 回 `std::string`（SDL 失敗時也回空字串，分不出來）；`Sync()` 回 `bool`（`SDL_SyncWindow` 只回 true／false）。新增錯誤碼 6 個。
  - 宣告：ip3b～ip3e 的判定（不用常數門檻）、突變 16 個（m6a、m6b、m1～m4、m-zero、m7～m9、m-up、m-layout、m10～m12、m-throw，逐項寫明由哪個判定殺掉）、機率性測試 2 個的試行規則（各 200 輪、60 秒、不重試）。
  - 消費端日誌契約的核對：A 級行中只有 1 個行族（受擊回饋行）經 SDL 寫出，ip3e 的停止條件會成立；處理方式交消費端，在本關卡一起決定。
  - 實際約 1 h（估 3～4 h）。
- 2026-10-21：ip3a 核准（D32）。ip3b 開始（檔位 high，估 8～12 h，超過 18 h 停下）：G1／G2 先 Red、`SdlPlatform` 的 pimpl、`PlatformEvent`、三個 Native 目標、機械修改。`PlatformEvent::Timestamp()` 與 `HandleNativeEvent` 的時間參數在 ip3c 加入（同一個 PR）。
- 2026-10-21：ip3b 實作與本機驗收。
  - G1、G2（`tests/common/platform_sdl/HeaderHygieneTests.cpp`，CTest `platform_sdl_header_tests`，標籤 cpu）：先在現狀上 Red（G1 抓到 SDL 標頭；G2 列出 12 處，註解中的 SDL 正確略過），改完後 Green。兩者都以測試失敗呈現，不用 `#error`，讓突變以訊息判定。
  - `SdlPlatform`、`SdlRenderer` 改 pimpl；新增 `PlatformEvent`（不可複製、移動）、`NativeAccess`；三個 Native 目標（`GYO::PlatformSDLNative`、`GYO::InputBackendSDLNative`、`GYO::RenderBackendSDLNative`，INTERFACE）；後端以 PRIVATE include 讀自己的 Native 標頭，input 與兩個 render 後端以 PRIVATE 連 `GYO::PlatformSDLNative`；`SdlGpuRenderDevice.hpp` 補 `<string>`。
  - 機械修改：`tests/common`（`SdlInputTests`、`SdlPlatformTests`）、ui_editor、提出需求的消費端（產品、probe、pointer capture 測試；暫時改連 Native，IP-4 再移除）。共 52 處 `NativeWindow()` 呼叫改成自由函式。未啟用產品不改，寫進遷移清單（破損 3 處）。
  - 突變：TT 的 `run_mutations.py` 加 `--catalog`（預設不變），新目錄 `scripts/mutations.json`；m6a、m6b 各對 G1、G2 兩次，4／4 killed。
  - 本機：全量建置通過；`test` preset 78／78；`input_sdl_tests`、`platform_sdl_tests`、`object_fps_pvp.pointer_capture`、`runtime.sdl_smoke`、`render.sdl_gpu_mesh_smoke` 通過。`object_fps_pvp.worker` 失敗，原因與 master 相同（mock 缺 `maximum_shot_damage`，消費端另一條分支已修），與本批無關。
  - 同機兩樹的權威 digest（base＝master `6fbb6a9`）：scale 1、3 各 36 個情境，0 差異。
  - **發現（ip3c 前要決定）**：IP-2 的測試「SdlPlatform keeps running live frames after a nested PumpEvents」（`SdlPlatformTests.cpp`）明確支援巢狀 pump，和 [platform-sdl.md](../../platform-sdl.md) Design Invariant 7「`PumpEvents` 不可重入」與 PLAN 的 `GYO_ASSERT` 衝突。ip3b 不改 pump 行為；ip3c 每次 pump 取時鐘配對時，巢狀 pump 會蓋掉外層的配對，需要選：保留巢狀（配對隨 pump 範圍保存與還原，改 DI 7）或禁止巢狀（刪改 IP-2 的這個測試，屬放寬既有保證的判定變更）。
- 2026-10-21：ip3b 的四平台 L1 #548（run 39603056923，`5e8b353`）成功。D33（巢狀 pump 保留）。消費端開 draft PR #85（IP-3、IP-4 與消費端遷移同一個 PR，D29）。
- 2026-10-21：ip3c 實作與本機驗收（檔位 high，換算局部 xhigh）。
  - `PlatformEvent::Timestamp()`；`InputEvent::timestamp`（`std::chrono::steady_clock::time_point`，後端 `static_assert` 與 `Time::TimePoint` 相同，`GYO::Input` 不新增依賴）；`SdlInput` 把 platform 事件的時間蓋到它新增的每個 `InputEvent`。
  - Native 層：`ClockPair`、`SampleClockPair()`、純函式 `ConvertSdlTimestamp`；測試縫 `SetClockPairSourceForTesting`、`CurrentClockPairForTesting`（宣告寫的 `SampleClockPairForTesting` 改成：取樣函式本身公開為 `SampleClockPair()`，測試以可注入的來源包住它來讀 β）。`HandleNativeEvent` 必須帶時間，沒有預設值。
  - 配對存在 `Impl::pumpPair`，由 `ValueScope` 隨 pump 範圍保存與還原（D33）。
  - 測試：`tests/common/platform_sdl/EventTimeTests.cpp` 7 個（純函式 3、整合 4），另加輸入端 1 個。`platform_sdl_tests` 連跑 50 次 0 失敗。
  - 突變：m1、m2（純函式／整合）、m3（純函式／整合）、m4（純函式／整合）、m-zero、m-nest，9／9 killed。執行時環境要 `SDL_VIDEODRIVER=dummy`。
- 2026-10-21：ip3c 的 xhigh 審查（有條件接受）與後續，**停下等使用者決定**（工作樹未提交）。
  - 審查：換算邏輯正確；但事前宣告的整合判定 `p0 − β/2 ≤ ts ≤ p1 + β/2` 在 Linux（Engine 讀 `CLOCK_MONOTONIC`、SDL 讀 `CLOCK_MONOTONIC_RAW`，速率不同）與 Apple Silicon（24 MHz 計數器的取整）上對正確實作也可能失敗；本機 Intel 不會發生，所以 50 次都通過。
  - 已處理（只收緊或回到核准設計）：測試縫改回唯讀（拿掉可寫的配對來源，那是繞過 DI 3 的後門）；精確等式的整合測試改用讀出的配對；巢狀 pump 的內層也檢查自己的配對；`ConvertSdlTimestamp` 去掉理論上的 UB；配對的組成拆成純函式 `MakeClockPair` 並加測試；新增「取樣落在呼叫者讀取之間」的測試。突變目錄 18 個全部 killed（試過的 x-order 存活，移除並記為缺口）。
  - 判定變更提案（放寬，§14.4）：容許改為 `τ = |drift| + P0.bracket + P1.bracket + 2 ns`（drift 由測試在 push 前自己取的配對與 pump 的配對量出）。獨立核對兩次：第 3 項（沒有收緊）成立；第 2、4 項指出 τ 有自我參照，`SampleClockPair` 內部怎麼用三次讀取（順序、取中點）偏差在約一個 bracket 以內的錯誤，舊判定殺得掉（例：29／40、40／40）、新判定殺不掉，是這次放寬新增的缺口。
  - 依 §14「同一問題改了 2 次判定方法仍未收斂就回報」停下。待決：(a) 照提案放寬並接受該缺口；(b) 另加讀取注入的接縫，讓 `SampleClockPair` 的讀取順序與取中點可以用精確等式檢查，再放寬；契約條文 (2) 改成「SDL 時間非零且不遞減」。
- 2026-10-22：ip3c 收尾（D34、D35）。
  - D34 的接縫：`SampleClockPairWith(const ClockReaders&)` 依序讀 Engine、SDL、Engine 再交給 `MakeClockPair`；`SampleClockPair()` 傳真實時鐘。新增假讀取的精確等式測試。
  - 判定變更（D35）照第 3 版：獨立核對 3 輪（第 1 輪：τ 有自我參照、取樣錯誤會撐大 τ；第 2 輪：`SampleClockPair` 內部的錯誤是放寬新增的缺口 → D34 的接縫；第 3 輪：可以交給使用者，兩處措辭已改）。
  - 突變目錄 22 個全部 killed。第一次執行時 4 個顯示存活，原因是測試名稱含逗號，doctest 的 `-tc` 把逗號當分隔，篩選對到 0 個測試；改名後重跑 4／4 killed，並確認每個篩選剛好對到 1 個測試。突變工具的這個陷阱只會讓結果變成「存活」，不會誤判 killed。
  - 守不住、記為已知缺口：正式路徑兩個讀取 lambda 偏差在一個 bracket 以內的錯誤（舊判定同樣守不住）。
  - 契約條文 (2) 改成「SDL 時間非零且不遞減」（D34）。
  - 宣告的更正（xhigh 審查第 7 點）：m2-int 由整合判定的上界殺掉（宣告寫下界）；m1 的實作是「每個程序取一次」而非「在 Create 取一次」。
  - 實際約 6 h（估 4～5 h，含審查與三輪核對；未超過 1.5 倍）。
  - 提交 `b56f59e`，推上 PR #85；四平台 L1 #550（run 39630112677）。同一次推送另外觸發 #551 與 Package trial #389。文件狀態行的同步在下一個只改文件的 commit（腳本中途失敗，`b56f59e` 只帶到 HANDOFF 的這段紀錄）。
- 2026-10-22：ip3c 的 L1 #550（run 39630112677，`b56f59e`）四平台成功，含放寬後的判定第一次在 linux-x64、macos-arm64 上執行。
- 2026-10-22：ip3d 實作與本機驗收（檔位 high）。
  - 輸入幀加法擴充：`KeyModifiers`、`TextEvent{Text|Key}`、`PhysicalInputFrame::text`（`BeginFrame` 清空）。`SdlInput::SetTextInput`（新錯誤碼 `TextInputFailed`）；收集條件在事件到達時查 `SDL_TextInputActive`，不保存副本（DI 6）。`Kind::Key` 只記按下（含 repeat），repeat 不進 `events`。`layoutKey`：有字元的 keycode 查固定表，沒有字元的 keycode 經它帶的 scancode。`SdlPlatform::ClipboardText()` 回 `std::string`。
  - 測試 4 個（`input_sdl_tests`）：關閉時不收集、只記按下且依到達順序、`layoutKey` 與實體鍵分開、剪貼簿（dummy driver 下可讀回）。
  - 突變 m7、m8、m9、m-up、m-layout，5／5 killed（目錄共 27 個）。
  - 消費端的位址輸入改用 `SetTextInput`、`text`、`ClipboardText`（消費端計畫的 ip3d 列）。`test` preset 78／78，SDL 相關 6 個通過。
  - 消費端看得到的差異 1 項：`Key::Enter` 依 IP-1 同時代表 Return 與數字鍵盤的 Enter，所以數字鍵盤的 Enter 現在也會確認位址（以前只有 Return）。
- 2026-10-22：ip3d 的 L1 #553（run 39633694569，`6ed2952`）四平台成功（**更正**，2026-10-22：PR #85 是 draft，推送觸發的 run 只做範圍判斷，四平台的 L1 job 全部跳過；這裡原本寫的「四平台成功」不實。ip3d～ip4b 的 L1 由手動觸發的 #561（`b3f6386`）補跑）。使用者：「我去睡覺了。可以的話吧IP-3、IP-4跑完。」——照「無人時做到下一個關卡就停」進行：L3（要人在場）、消費端 L2 的門檻（§14.1）、§14.4 的放寬、計畫外的 Architecture Delta、停止條件，都停下來等使用者。
- 2026-10-22：ip3e 實作（檔位 high，日誌橋局部 xhigh）。
  - Engine：`EntryPoint.hpp`（`GyoMain`）、`GYO::PlatformSDLMain`（OBJECT，`main` 經 `RunEntry` 呼叫 `GyoMain`，例外記錄後回 1）、`BaseDirectory`、`PathFromUtf8`、`PlatformName`、`SdlLogBridge`（預設 tee，同執行緒重入只送原輸出）。`tests/common/runtime_sdl` 改用 `GyoMain`。
  - xhigh 審查（有條件接受）：重現了一個競態——解構時先放開「一次一個」的旗標再還原輸出，另一條執行緒在空檔建橋會把橋自己當成原輸出，tee 模式下在 SDL 的日誌鎖內無限遞迴（卡死）。已修（先還原再放旗標），加「兩條執行緒輪流建橋」測試與突變 m-race。其他已修：斷言條件的副作用、先建立 SDL 的日誌鎖、`Forward` 不信任 userdata、`RunEntry` 的記錄不配置記憶體、`PathFromUtf8` 的空輸入、文件措辭（argv、鎖的順序）、測試缺口（userdata、被換掉的輸出保持原樣、分類表全部）。
  - 並行測試獨立成 CTest `platform_sdl_log_bridge_concurrency`（60 秒，照宣告）；主執行檔維持 15 秒。
  - 消費端：`main.cpp` 改為 `GyoMain`，執行檔目錄與平台名走 Engine，刪除 `ForwardEngineLog`／`ForwardSdlLog`，產品的 Base handler 以 `[level] text` 寫進日誌檔（Engine 的行前面帶分類，與以前經 SDL 時相同），再建立 `SdlLogBridge`；`PvpApplication.cpp` 的 `SDL_Log*` 改為 printf 風格的 `ClientLog`（S3：受擊回饋行位元組不變），`SDL_GetTicks` 改為程式啟動後的毫秒數，`SDL_Delay` 改為 `Waiter`；`PlayerPresentationTests` 改用 `BaseDirectory`、拿掉直接連 SDL3。
  - 本機：全量建置（printf 格式檢查無警告）；`test` preset 79／79；SDL 相關 7 個；並行測試連跑 30 次 0 失敗。
  - 突變目錄 36 個全部 killed（ip3e：m10、m11、m-assert、m-tee、m12、m-throw、m-race、m-userdata、m-overwrite；m12 的殺掉判定改由 probe 的結束碼 7／0，原本宣告的 runtime_sdl 結束碼殺不掉它，因為 smoke 本來就回 0；m-tee、m-assert 等是補加的，只收緊）。
  - 暫停（2026-10-22，使用者：「你跑完手頭上的任務就先暫停一下吧。」）；之後 P1～P3 經分支會話決定（D36）。
  - **待使用者決定**（實作照目前寫法，PR 不合併）：
    - P1：解構子含 GYO_ASSERT 並標 `noexcept(false)`，違反 `docs/architecture/error-handling.md` 的「解構子與執行緒入口不含斷言」（計畫與該規則互相矛盾，xhigh 審查第 2 項）。選項：(a) 解構子回到 noexcept，以 death test 驗證中止，error-handling.md 加例外條款；(b) 改成記錄 Error 而非斷言（宣告的判定要變更，§14.4）。
    - P2：`RunEntry` 的 `catch (...)` 與 error-handling.md 第 6 條「never catch (...)」字面衝突；進入點需要攔下所有例外（Design Invariant 8），建議在 error-handling.md 加例外條款。
    - P3：宣告的「每一輪的行數守恆」在不暫停推送執行緒的情況下量不到；實作為「逐執行緒守恆」（比原本的總和更嚴，但字面上與宣告不同），照 §14.4 交使用者確認。
- 2026-10-22：ip3e 的 L1 #554（`97e2a22`）四平台成功（**更正**，2026-10-22：PR #85 是 draft，推送觸發的 run 只做範圍判斷，四平台的 L1 job 全部跳過；這裡原本寫的「四平台成功」不實。ip3d～ip4b 的 L1 由手動觸發的 #561（`b3f6386`）補跑）。依 D36 處理 P1～P3：`error-handling.md` 加平台邊界的例外條款；SdlLogBridge 解構子回到 noexcept，斷言改以 death test（`platform_sdl_entry_probe replaced-bridge`，assertion handler 以結束碼 41／42 回報別人的輸出是否原封不動）；宣告改為逐執行緒守恆並記 §14.4 的 1～5（`ip3-declaration.md` 第 6 節）。m-race 在 noexcept 解構子下有時由橋自己的 GYO_ASSERT（測試的 throwing handler 下 terminate）抓到，預期訊息改為兩者擇一。
- 2026-10-22：ip3f（檔位 medium）：突變目錄 36／36 killed；`test` preset 79／79，SDL 相關 6 個；寫 [dev_log](../../../dev_logs/2026_10_22_engine_ip3.zh-Hant.md)；`platform-sdl.md` 的狀態、遷移清單（ip3e 的選用遷移）、PLAN／README 的狀態。**IP-3 實作完成，L3 待使用者**（使用者：「處理完P1-P3後，處理ip3f，L3排最後。」）。下一步 IP-4：先做介面的 xhigh 審查（U10），再宣告 m13～m19。
- 2026-10-22：ip3f 的 L3（macOS Intel，使用者操作；證據 `build/target/_build/test/logs/pvp-v71-ip3f-l3-20261022/client.log`）：**通過，附兩項觀察**。
  - 位址輸入（按住 Backspace、Ctrl+A、Ctrl+V、Return、Esc）、live resize、關閉都正常（使用者：「其他功能是正常的」）。
  - 日誌檔的格式與修改前（`pvp-v7-lan-kit-20261014/smoke/mac-client-logs/client-20261014-235210.log`）相同：`[info] PvP client start …`、`[info] PvP graphics ready …`、Engine 的 `[info] SDL_GPU slow swapchain acquire …`。
  - 觀察 1：輸入「:」時忽然卡住（使用者：「不知道是不是輸入法的問題」）。日誌沒有對應的異常；推論是輸入法的組字（SDL 的 TEXT_EDITING 不顯示，以前也一樣），沒有重現，不當成缺陷。
  - 觀察 2：「操作手感怪怪的」。日誌有 9 次 `drawable_ms≈66` 的取得停頓（約 4 幀）；修改前的日誌也有，是 IP-2 已知、由執行緒分離處理的問題（D19、未結事項），不是本批造成的。
  - 使用者另外要求：v8 排一個任務，把這些按鈕和輸入框元件化，順便修改 ui_editor（記在消費端 v8 的候選）。
- **IP-3 完成**（L1、突變、L3）。下一步 IP-4：先做介面的 xhigh 審查（U10）。
- 2026-10-22：**IP-4 開始**（使用者：「開始 IP-4：先做介面的 xhigh 審查（U10），再宣告 m13～m19，然後做 ip4a。」）。檔位 high；介面審查 1 位 xhigh（U10，D31 核准）；ip4b 的 watch 執行緒與生命週期局部 xhigh。估 Engine 11～12.5 h。
- 2026-10-22：IP-4 的介面 xhigh 審查（U10）：**接受，但需修改**；沒有讓 ip4c 的 PRIVATE 連結做不到的問題。ip4a 前必修 H1、H2、M1、M2、M3；ip4b 前 M5、M6、M8；ip4c 前 M7。審查也提出 ip4a～ip4c 的突變建議（約 30 個）。**停下等使用者**：
  - H1：草案禁止在 observer 與 live frame 內呼叫任何 platform 成員，但產品的 live frame 要讀視窗大小（`PvpApplication.cpp:1218-1225`，消費端計畫要改成 `QueryWindow`）。照草案，第一次 live resize 就會中止。要把 DI 7 拆成查詢類（主執行緒，observer／live frame 內允許）與操作類（observer、live frame、watch 回呼內斷言）。
  - H2：「EventWatch 先於 SdlPlatform 解構（GYO_ASSERT）」和 probe 的宣告順序衝突（`player_short.hpp:102` 的 watch 在 application 之外），例外路徑會中止、寫不出證據；斷言本身還要讓 watch 持有指回 platform 的參照（懸空的來源）。SDL 3.4 下 platform 先解構無害（events 子系統關閉時清空 watcher 清單）。建議 watch 只記 window ID、不參照 platform，DI 7 改成「platform 先解構時之後不再收到該視窗的事件；之後解構 watch 是安全的」。
  - M1：SDL 的 `dispatching` 是 bool 不是深度，在回呼內或 live frame 內解構 watch 可能 use-after-free。契約：AddEventWatch、EventWatch 解構、注入不得在 watch 回呼或 live frame 內呼叫（thread_local 的分派深度做斷言）；「回呼不得阻塞」改成可判斷的規則。
  - M2：QueryWindow、Raise、AddEventWatch 在 SDL 3.4 沒有 Runtime Error 的來源（錯誤碼會是死碼），建議改回傳值型別；PLAN 的 `Sync` 仍寫 `Result<bool>`（宣告已定為 bool）。刪除 ip4b 的「AddEventWatch 失敗路徑」驗收屬放寬（§14.4），要核准。
- 2026-10-22：D37 套用到 PLAN 的 IP-4 草圖與 `platform-sdl.md`（DI 7、Pump／observers／watches 一節）；IP-4 的事前宣告 [ip4-declaration.md](ip4-declaration.md)（ip4a 完整；ip4b 的時間戳判定與並行解構、ip4c 的 G3 擴充先宣告）。
- 2026-10-22：ip4a 實作與本機驗收（檔位 high）。
  - platform：`Rect`、`WindowBorders`、`WindowFlags`（含相對滑鼠模式）、`WindowState`、`DisplayState`；查詢 `QueryWindow`、`QueryDisplay`（`DisplayUnavailable`）、`VideoDriverName`、`X11WindowNumber`；操作 `SetWindowSize`、`SetWindowPosition`（`WindowOperationFailed`）、`Raise`、`Sync`。操作在 observer、live frame、watch 回呼內斷言；thread_local 的分派深度（live frame 的 watch 內遞增）。Native：`PushInjectedEvent`（thread_local 的注入旗標，供 ip4b 的 watch 讀）。
  - input 後端：`SdlInputInjector`（`SdlInputInjection.hpp`，`InjectionFailed`）：key（沿用 scancode 表反查，Enter 用 Return，keycode 取 `SDL_GetKeyFromScancode`）、滑鼠鍵、motion、focus。
  - 測試：`input_sdl_tests` 6 個（只走 SDL 佇列、每個 Key 的往返、Enter→Return、方向、motion、live frame 內注入斷言）、`platform_sdl_tests` 5 個（查詢每次讀 SDL、操作、live frame 內查詢不斷言、observer 內操作斷言、顯示器與驅動）。測試先 pump 一幀清掉視窗建立時的事件。
  - 突變 m13、m14、m-enter、m-table、m-motion、m-focus、m26、m28、m29、m-depth、m-x11，11／11 killed。
  - 消費端：live frame 讀視窗大小改用 `QueryWindow`（H1 的情境）；產品本體（`main.cpp`、`PvpApplication.cpp`）的 SDL 符號歸零，拿掉產品對 `GYO::PlatformSDLNative` 的暫時連結。probe 與 pointer capture 測試仍暫時連 Native（消費端 IP-4 遷移）。
  - 本機：全量建置；`test` preset 79／79；SDL 相關 7 個；SDL 測試連跑 20 次 0 失敗。
- 2026-10-22：ip4a 的 L1 #558（`d82364e`）四平台成功（**更正**，2026-10-22：PR #85 是 draft，推送觸發的 run 只做範圍判斷，四平台的 L1 job 全部跳過；這裡原本寫的「四平台成功」不實。ip3d～ip4b 的 L1 由手動觸發的 #561（`b3f6386`）補跑）。
- 2026-10-22：ip4b 實作與本機驗收（檔位 high，watch 的執行緒與生命週期局部 xhigh）。
  - `ObservedEventKind`（12 種）、`ObservedEvent`、`EventWatchCallback`、`EventWatch`（可搬移，以 `unique_ptr<Node>` 持有，只記 window ID）、`SdlPlatform::AddEventWatch`。回呼：先過濾種類與視窗，再取一個配對，`observedAt` 是第二次 Engine 讀取，`timestamp` 以它為上限；`injected` 讀 thread_local。解構在分派深度內斷言（D36 的平台邊界例外）。Native 的唯讀接縫 `LastWatchClockPairForTesting`。
  - capture：GPU 後端的 `SaveCaptureBmp`（路徑以 UTF-8 傳給 SDL）；Render 的公開錯誤碼加 `FileWriteFailed`（Architecture Delta，PLAN 已預告）。
  - 測試 11 個（`EventWatchTests.cpp`）加 death test 1 個（probe 的 `watch-in-live-frame`）。宣告的時間戳判定在執行前更正（見 [ip4-declaration.md](ip4-declaration.md) 第 4 節）。連跑 30 次 0 失敗。
  - 突變 12 個全部 killed（m15、m15b、m15c、m16、m-kind、m17、m-order、m20、m21、m22、m-depth-watch、m18）。
  - 本機：全量建置；`test` preset 79／79；SDL 相關 7 個。
- 2026-10-22：ip4b 的局部 xhigh 審查（F1～F14）與修正，詳見 [ip4-declaration.md](ip4-declaration.md) 第 4 節「xhigh 審查之後」。
  - 主要修改：
    - 回呼改用 `SampleClockPair()`；
    - injected 改成比對事件位址：push 內 live frame 造成的真實事件不再被標記；
    - 註冊計數的接縫：搬移指派確定性地檢查；
    - capture 測試移到 `render.sdl_gpu_capture_file`（cpu），platform 測試不再連 GPU 後端；
    - 併發測試獨立成 `platform_sdl_concurrency`；
    - watch 比 platform 活得久拆成兩個測試；
    - 回呼內解構的 death test。
  - 突變目錄 69 個：第一次 67／69。m-depth 的 find 過時；m-watch-engine 存活，補「`observedAt` 落在 push 之內」後殺掉；兩者與時間戳相關的突變重跑，6／6 殺掉。
  - 本機：`test` preset 80／80；SDL 相關 8 個連跑 3 次 0 失敗。
  - D38 之後（方案 B 加上主執行緒斷言）：
    - 突變目錄 74 個。第一次全跑 73／74：m-depth-watch 的 find 因為插入主執行緒的註解而過時；修正後與 m-main-thread 一起重跑，2／2 殺掉，合計 74／74。
    - `test` preset 80／80。
    - platform／input／capture 測試連跑 10 次，0 失敗。
    - 紀錄：scratchpad 的 `mutants-ip4b-final.log`。
  - L1：推送觸發的 #560 只做範圍判斷（draft PR），L1 job 跳過；手動觸發 #561（`b3f6386`），涵蓋 ip3d～ip4b。
  - 使用者核准 D38：
    - F4 採方案 B；
    - F10 加入 DI 7 的主執行緒規則，以 death test `watch-off-main` 與突變 m-main-thread 檢查；
    - DI 3 的守衛清單補上 watch 路徑。
  - 另回報 F11：m-order 依賴配置器，ASan 下才確定。
- 2026-10-21：Architecture Delta：規劃本身只改文件。草案的 Delta 見 PLAN 的 IP-3 節（七點）與 IP-4 補充。

## 消費端回報（文字摘要，D12）

- 2026-10-05：提出需求的消費端已改用 IP-1：
  - 遊戲中的指標擷取政策改為在 pump 後依序讀 `PhysicalInputFrame::events`。
  - 以舊的原生處理當基準做 characterization，0 不一致。
  - 宣告的差異只有三點：
    - 處理時機（同一幀內、`Update` 之前）；
    - 視窗內判定改用 `insideWindow`；
    - 每個事件當下的焦點以開始時的焦點加上 events 重建，證明與舊行為相同。
  - IP-1 介面足以保持既有行為，沒有發現缺口。
  - 仍直接使用 SDL 的部分：
    - 文字輸入與剪貼簿（D11② 的候選）；
    - 驗收 probe 的事件注入與 event watch。probe 需要 timestamp、裝置 id、相對位移，以及 occluded／exposed／hidden 這幾種視窗事件，IP-1 的事件沒有這些。
- 2026-10-06：提出需求的消費端在 IP-2 合併後的 master 重取了量測基線（B1）：
  - 短測沒有任何 `Skipped` 幀；
  - 雙 GUI 短測的延遲與先前基線相當或略低；
  - 25 案矩陣在消費端修正一個驗收工具的誤判後全數通過。
  - 驗收工具不需要為了 IP-2 修改。D13 的停止條件（分析器無法處理常見的 `Skipped`）沒有發生。

## 未結事項

- IP-2：停頓位置已量測：在 `nextDrawable`（fence 0 ms）。觸發原因未查明（這次不是拖動標題列）；依 D19（2026-10-08 由 D27 修訂）由消費端分執行緒處理，等待與執行緒的機制見 [時間、執行緒與 Trace](../time-threads-trace/README.md)。
- IP-2：縮放時的停頓在事件處理（B0）。IP-2b 的 live frame 已消除「縮放拖動中」的停頓；消費端回呼的時間是 0.0 ms，停頓全在 SDL 的輪詢（追蹤迴圈）內。
  **按下縮放角到開始拖動之間的空窗**仍會停住（實測最長 4.3 秒），依 D19（D27 修訂）由消費端分執行緒處理（同上）。
- IP-2：Metal 長幀仍回報 `Presented`，Engine 如何辨識「沒有真正呈現」，待查。
- IP-2：D7 的「暫停 acquire」不採用（D19）；原本記錄的事件時序與傳遞路徑問題隨之結案。
- IP-2：Windows（Win32 modal 迴圈，拖動與縮放都走 live frame 路徑）、Linux 與 macOS arm64 實機都未執行。Windows 按下標題列或邊框時 SDL 立即開始計時器（`WM_ENTERSIZEMOVE`），依原始碼推斷不會有 macOS 那種空窗，但沒有實測。
- IP-1：完整 scancode 的表示方式（延伸 enum 或其他）在介面審查時決定；限制是保留既有名稱與列舉值。
- IP-1：同一幀上升沿的表示方式（次數或依序的事件清單）在介面審查時決定；限制是既有 `pressed`／`released`／`held` 的語意不變。
- 遷移清單：本夾不另建，寫入 [foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md) 的「IP」節（該節已存在）。
- IP-3～IP-5（2026-10-08 規劃，D28）：範圍見 [README](README.md) 的進度表；詳細的 PLAN 在各批開始時撰寫。**（2026-10-21：IP-3／IP-4 的草案已寫，見 PLAN 文末，等核准。）**補充（2026-10-14，消費端 D76）：IP-3、IP-4 的消費端移到消費端的下一個版本，IP-5 的移到再下一個版本。

# 輸入與呈現：交接

更新：2026-10-05。**狀態：IP-2 完成（縮小交付，PR 待開）；IP-1 完成，PR [#50](https://github.com/yojinn-io/GYO-Engine/pull/50) 已合併（`56e0033`）。** 2026-10-04 由提出需求的消費端在其規劃批次中建立。
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

- 來源：[架構漂移健檢（2026-10-04）](../../../checkup/2026_10_04_architecture.zh-Hant.md)「應收進 Engine 的功能」中優先度最高的一項；使用者 2026-10-04 決定處理。
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
- `NativeEventObserver`（`engine/platform/sdl/include/platform/sdl/SdlPlatform.hpp:43,56`）必須保留：
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

## 未結事項

- IP-2：停頓位置已量測：在 `nextDrawable`（fence 0 ms）。觸發原因未查明（這次不是拖動標題列）；依 D19 延到分執行緒的計畫。
- IP-2：縮放時的停頓在事件處理（B0）。IP-2b 的 live frame 已消除「縮放拖動中」的停頓；消費端回呼的時間是 0.0 ms，停頓全在 SDL 的輪詢（追蹤迴圈）內。
  **按下縮放角到開始拖動之間的空窗**仍會停住（實測最長 4.3 秒），依 D19 延到分執行緒的計畫。
- IP-2：Metal 長幀仍回報 `Presented`，Engine 如何辨識「沒有真正呈現」，待查。
- IP-2：D7 的「暫停 acquire」不採用（D19）；原本記錄的事件時序與傳遞路徑問題隨之結案。
- IP-2：Windows（Win32 modal 迴圈，拖動與縮放都走 live frame 路徑）、Linux 與 macOS arm64 實機都未執行。Windows 按下標題列或邊框時 SDL 立即開始計時器（`WM_ENTERSIZEMOVE`），依原始碼推斷不會有 macOS 那種空窗，但沒有實測。
- IP-1：完整 scancode 的表示方式（延伸 enum 或其他）在介面審查時決定；限制是保留既有名稱與列舉值。
- IP-1：同一幀上升沿的表示方式（次數或依序的事件清單）在介面審查時決定；限制是既有 `pressed`／`released`／`held` 的語意不變。
- 遷移清單：本夾不另建，寫入 [foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md) 的「IP」節（該節已存在）。

# sdl-isolation J1：pointer capture 的凍結 oracle 改成 golden（AGENTS §14.4 的 1～5）

- 狀態：**核准**（v7.1 D169，2026-10-23：golden 全欄位版；v-s08～s10 選 (a)）。方向在 D168（P3＝A）已核准；本文件是判定變更本身的核准，附實測、突變與獨立核對。
- 對象：`tests/object_fps_pvp/PointerCaptureCharacterizationTests.cpp`（CTest `object_fps_pvp.pointer_capture`）。這是 v6 第 06 批的特性測試，不是 Design Invariants 的守衛；但宣告預設它有放寬的一面，所以照 §14.4 走完整流程。
- 依據：[sdl-isolation-ip4-declaration.md](sdl-isolation-ip4-declaration.md) 第 6 節 J1；Engine PLAN 的 U6。
- 工作樹狀態：新測試、golden 檔、Engine 補的 s06 守衛都已完成；**舊 oracle 的刪除尚未提交**，等本文件核准。Engine 的 s06 守衛不依賴本核准，已先提交（`8d7182e`）。

## 新舊對照

| | 舊（凍結 oracle） | 新（golden） |
|---|---|---|
| 參考 | 第 06 批之前 `HandleNativeEvent` 的遊戲中分支，原樣複製成可執行的 oracle | 在 `8d7182e` 錄下的 golden 檔（`fixtures/pointer_capture_golden.tsv`）：錄製時 oracle 與現行政策在全部 15014 幀 0 不一致 |
| 輸入 | 同一個 `SDL_Event` 經 Engine 的 `SdlInput` 交給政策，同時交給 oracle | policy 邊界的 `InputEvent` 序列（`SdlInput` 實際產生的，除時間戳以外的全部欄位；政策不讀時間）加每幀的 `focusedAtStart` |
| 範圍 | 手寫 14 幀＋種子 5 組 × 3000 幀；宣告差異 2 的單獨測試；釋放計數的純函式測試 | 同一批序列（同樣的 15014 幀）；部分幀開始時預設 reload 邊緣（T2）；釋放計數的純函式測試保留 |
| 比對 | 每幀 6 個欄位＋釋放次數 | 每幀 6 個欄位＋釋放次數＋`releasesWhileCaptured`（T1） |
| 需要 | SDL、Native、dummy driver | 不需要 SDL |

產生器（一次性，用到 Native）不留在樹裡，連同舊測試的原始碼放在證據夾 `build/target/_build/test/logs/pvp-v71-ip4-capture-golden-20261023/`。

## 1. 這個判定守的性質

產品的指標擷取政策（`apps/object_fps_pvp/src/Pvp/PointerCapture.cpp`）與第 06 批之前的行為相同：

- 何時擷取：有焦點、這一幀沒有視窗互動、在視窗內點左鍵；或按 Tab。
- 何時釋放：失焦、移動、縮放、縮小、Tab。釋放時清掉擷取、兩個 pending 邊緣、左鍵按住狀態。
- 哪一次點擊算射擊。

舊判定另外順帶守了 Engine 的 `SdlInput` 把 SDL 事件轉成 `InputEvent` 的語意（焦點、insideWindow、本視窗過濾、按鍵重複）。

## 2. 放寬的面

舊判定失敗、新判定通過的情況：

- **L1 Engine 轉換層的錯誤**：例如重複鍵沒有過濾、insideWindow 恆為真、其他視窗的按鍵或失焦被當成本視窗的。新判定不經過 Engine 的轉換，所以本測試抓不到。
- **L2 insideWindow 用了過期的視窗大小**（突變 s06）。
- **L3 `SdlInput` 逐事件焦點語意的改變**。
- **L4 參考的獨立性下降**：從「和第 06 批之前的程式相等」，變成「和錄製時的實作相同」。如果日後政策改了又重錄 golden，重錄的來源就是實作本身。
- **L5 golden 只是投影**（獨立核對指出）：第一版只記政策當時讀的欄位（kind、key、button、insideWindow），座標、視窗位置與大小重播時都是 0。「政策開始讀 windowX」這類改動（v-p31）舊判定殺、新判定存活；反過來，在真實輸入上等價的改動（v-p33）會被新判定誤判。**已處理**：golden 重錄為除時間戳以外的全部欄位（仍是 15014 幀、0 不一致），v-p31 殺掉，v-p33 通過。剩下的投影只有時間戳；政策不讀時間。

## 3. 收緊的面

- **T1**：每一幀都比 `releasesWhileCaptured`（以前只在純函式測試裡比一次）。
- **T2**：部分幀開始時預設 `pendingReloadEdge=true`（每個種子序列 273 幀），釋放時必須清掉。殺掉舊判定下存活的突變 p24（釋放時不清 reload 邊緣）。附註（獨立核對）：現行產品在每次 Update 都重新指定 reload 邊緣（`PvpApplication.cpp:1005`），所以 p24 在產品層目前是等價的；T2 守的是政策自己的契約（`PointerCapture.hpp`：釋放時清掉兩個 pending 邊緣）與 oracle 等價性。

## 4. 放寬之後，原本的性質由哪些判定繼續守住

- **政策本身**（性質的主體）：新判定。突變結果見第 5 點。
- **L1**：Engine 的 `input_sdl_tests`（`tests/common/input/SdlInputTests.cpp`）殺掉 s01～s05：重複鍵、insideWindow 恆為真、其他視窗的按鍵、其他視窗的失焦、邊界改成含等號。獨立核對另外造出 v-s11～s15（MOVED／按鍵／取得焦點不過濾視窗、BeginFrame 不清事件、只檢查 x），全部由 Engine 測試殺掉。
- **L2**：本批在 Engine 補了「點擊交給 `SdlInput` 時，用當下的視窗大小判定 insideWindow」（`tests/common/input/SdlInputTests.cpp`，`8d7182e`），並把這個約束寫進 `docs/architecture/input.md` 的 Contracts。突變 `m-inside-stale`（等同 s06）殺掉。
- **L3**：policy 側由 golden 殺掉 p29（忽略 focusedAtStart）、p30（忽略取得焦點）。Engine 側：
  - 獨立核對發現原本沒有任何測試檢查「取得焦點後 `windowFocused` 為真」（v-s07 舊判定殺、Engine 存活）。**已處理**：Engine 補測試「focus events of this window set and clear windowFocused」，突變 `m-focus-gain-state` 殺掉。
  - **未處理、交你決定**（v-s08～s10）：Engine 會不會把「狀態沒變」的事件去重，例如已失焦又收到 FocusLost，或沒有 repeat 標記的重複按下（按鍵、滑鼠鍵）。這三個突變舊判定殺、新判定與 Engine 測試都存活。產品依賴「不去重」：Tab 每按一次切換一次，FocusLost 一定釋放。選項見文末。
- **L4**：golden 的檔頭寫明重錄規則：只在事前宣告的政策變更時重錄，並保留舊檔，比照 `authority_golden.txt`。舊 oracle 的原始碼與產生器保留在證據夾。
- **產品接線**（政策在產品裡被正確呼叫）：新舊判定都不涵蓋，由 L2 的 GUI 短測負責（action short 的 REQUIRED_CHECKS 含 capture、focus、Tab、drag）。

## 5. 根據與證據

- **錄製**：產生器在同一次執行裡跑 oracle 與現行政策，15014 幀 0 不一致（`CHECK(writer.mismatches == 0)` 通過）。全欄位版在 L5 處理後重錄，同樣 0 不一致。產生器是「第 06 批測試＋錄製用的幾行」，放在被 git 忽略的證據夾；舊 oracle 可以從 git 歷史取回（本檔改動之前的 `PointerCaptureCharacterizationTests.cpp`），所以清掉建置目錄後仍能重做。涵蓋：每個種子序列約 500 次擷取、100 次以上射擊、1000 次以上釋放；手寫序列 4 次擷取、2 次射擊、5 次釋放。
- **突變**（產品目錄 `tests/object_fps_pvp/mutations.json`，批次 `v71-sdl-isolation`；新判定上執行，2026-10-23）：33／33 殺掉。
  - policy 突變 p02～p30（29 個）全部被新判定殺掉。其中 p24 在舊判定下存活，現在由 T2 殺掉。
  - p01（失焦時不設 `focused=false`）是等價突變，不列入：失焦一定釋放，之後的事件在同一幀內不會讓結果不同。
  - 守衛的 g1～g4 都以預期訊息失敗。g2 是 configure 時的連結守衛，產品的突變工具為此加了 `expect_build_failure`，工具的單元測試補了一個案例。
  - 紀錄：scratchpad 的 `mutants-sdl-j1.log`、`mutants-sdl-j1.json`。
- **舊判定的突變**：唯讀調查時在 scratch 量測（腳本 `scratchpad/mut/run.py`）。PointerCapture.cpp 的 30 個突變殺 28 個：p01 是等價突變，p24 存活。SdlInput 的 s01～s04、s06 由舊判定殺掉，s05 存活。
- **突變（L5、L3 處理後重跑）**：產品 34／34（加 v-p31）；Engine `m-inside-stale`、`m-focus-gain-state` 2／2；v-p33（等價）在新判定下通過。紀錄：scratchpad 的 `mutants-sdl-j1-full.log`。
- **獨立核對**（另一位審查者，2026-10-23）：
  - 自寫錄製程式，以 oracle 的狀態（而非政策的）作期待值，並由 oracle 獨立計數 `releasesWhileCaptured`；結果與 fixture 的資料列逐位元組相同。
  - 每幀清除、T2 的處理與產品、舊 harness 等價。
  - 第 1、3 點成立；第 2、4、5 點部分成立。指出的 L5、v-s07～s10、p24 的產品層等價、過時的宣告文字，都已在本版處理或列為待決。
  - 紀錄：scratchpad 的 `verify-j1/`。

## 決定（D169）

使用者：「核准 J1 的判定變更（golden 全欄位版）；v-s08～s10 選 (a)：Engine 契約寫明不去重，補 3 個測試與突變。」

- **(a) 的實作**：
  - `docs/architecture/input.md` 的 Contracts 加一句：本視窗的焦點喪失、按鍵按下、滑鼠鍵事件，即使狀態沒變也照實報告，只丟掉按鍵 repeat。
  - `tests/common/input/SdlInputTests.cpp` 加「events of this window are not deduplicated when the state does not change」。
  - 突變 `m-dedup-focus-lost`、`m-dedup-key`、`m-dedup-button`，3／3 殺掉。
- **L3 的 Engine 側**：v-s07～s10 現在都由 Engine 測試守住。

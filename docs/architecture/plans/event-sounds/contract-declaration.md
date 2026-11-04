# v8-arch-event-sounds-impl 事前宣告

- 狀態：**核准**（2026-11-04，使用者：「照建議核准」；D-es-1～4 都採建議）。第 1 批 v8-arch-event-sounds-contract-doc 交出。核准後，這裡的判定、突變與試行規則不再放寬（AGENTS §14.1）。
- 依據：[契約](../../../framework/event-sounds.md) 的 Validation 1～8、Using it、Design Invariants 1～6；framework 的 DI 2、3、4（`docs/framework/architecture.md`）；`docs/architecture.md`「Layers: admission and format ownership」。
- 執行：`docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --catalog docs/architecture/plans/event-sounds/scripts/event_sounds_mutations.json --build-dir build/target/_build/test`。只有「失敗而且輸出符合預期訊息」才算 killed。doctest 的 `-tc` 過濾不含逗號。
- 突變的 `find` 在第 2 批寫好程式後補上；補上時做兩項預檢：每條 `find` 在目標檔恰好出現一次；預期訊息的正規表示式對照實際的輸出格式（CMake 與 unittest 會折行）。補 `find` 不改這裡宣告的「改壞什麼」與「由哪個判定殺掉」。

## 1. 要你決定的事

決定：四項都照建議（使用者 2026-11-04「照建議核准」）。

| 代號 | 問題 | 建議 | 會改變 |
|---|---|---|---|
| D-es-1 | 事件名的字元規則（1～64 字元 `[a-z0-9_.]`）：(a) framework 自己再寫一份；(b) 解析只檢查非空，字元規則由綁定的「名字必須出現在所引用集合裡」間接保證；(c) Engine 公開 `ValidEventName`（本卡不改 Engine，要另開 Engine 批） | (b)。Engine 的規則在 `engine/model/src/AnimationEvents.cpp` 的匿名 namespace；(a) 會碰到 framework 的 Replacement Criteria「copies an Engine rule that no shared fixture checks」。代價：只解析不綁定時（工具編輯中），`Magazine In` 這種名字到綁定才報錯，錯誤路徑仍指向那個鍵 | 契約 Validation 4、7 的寫法；ES4、ES7 的內容 |
| D-es-2 | §14.4 検証者用 Fable 5.1／high（卡片指定），比主會話（Opus 5.5）強 | 同意 Fable 5.1／high。守衛的正向清單有放寬面，検証者要和実装者不同模型；Opus 5.5 的審査者與実装者同模型，盲點可能重疊 | 第 2 批結束前的驗證 |
| D-es-3 | 卡片沒有的 DI 6「One schedule」（排程只有 `EventSoundOffsetSeconds` 一份，消費端不留副本） | 加。這個函式進 framework 的理由就是預覽與遊戲落在同一點；不寫成約束，PvP 遷移後再長出一份也看不出來 | 契約的 DI 6、突變 `es-offset-*` |
| D-es-4 | 第二個讀者（Model Viewer）還沒建，今天只有 PvP 讀這個對應；Layers 檢查 3 說「只有一個產品讀的組合屬於那個產品」 | 照你 2026-11-04 的決定由 framework 持有，契約的 Layers checks 寫明這是使用者接受的預測壓力（比照 framework-rework D-8） | 契約的 Layers checks 一段 |

## 2. 規則與判定（`tests/framework/EventSoundsTests.cpp`，一條規則一組）

合成資料，不引用任何產品的資產（AGENTS §7）：asset id 用 `test.*`，anim_events 集合在測試裡直接組成 `Engine::Model::AnimationEventSet`（兩個 clip，事件名分佈在不同 clip）。每個拒絕都斷言 code 與 detail（出錯值的路徑）。

| 組 | 契約規則 | 判定 |
|---|---|---|
| ES0 | 讀得到的文件 | 三個事件（B）與一個事件（A）的文件都讀得到；`sounds` 依事件名排序；每個欄位的值等於文件；綁定後 `Find` 找得到每個名字，找不到的名字回空 |
| ES1 | 1 欄位精確、鍵不重複 | 頂層多一個欄位、少一個欄位各拒絕；項目多一個、少一個欄位各拒絕；重複的鍵在頂層、`events` 內（同一事件名兩次）、項目內各拒絕；文件不是物件、不是 JSON 都拒絕 |
| ES2 | 2 schema、version | `gyo.event_sound`、`gyo.anim_events` 拒絕；version `2`、`"1"`、`1.5`、`1.0` 拒絕，`1` 接受 |
| ES3 | 3 `animation_events_asset_id` | 空字串、數字拒絕 |
| ES4 | 4 `events` | `{}`、`[]`、字串拒絕；鍵為空字串拒絕；值是數字、陣列拒絕（D-es-1 採 (b) 時，解析接受 `Magazine In`，ES7 拒絕它） |
| ES5 | 5 `sound_asset_id` | 空字串、數字拒絕 |
| ES6 | 6 `sync_seconds`、`gain` | `sync_seconds` −0.01、字串拒絕，0 接受；`gain` −0.1、8.0001、`1e39`（float 裝不下；轉型前先拒絕，因為超出範圍的轉型是未定義行為。實作時更正措辭，判定不變）、字串拒絕，0 與 8 接受；結構層（不經文字）的 NaN、inf 的 `sync_seconds` 與 NaN 的 `gain` 由 `CheckEventSoundSet` 拒絕 |
| ES7 | 7 綁定：事件名 | 只出現在第二個 clip 的名字接受；不在任何 clip 的名字拒絕（detail `events."<名>"`，code `UnboundEventSounds`）；大小寫不同拒絕 |
| ES8 | 8 綁定：聲音 | `sync_seconds` 等於長度接受、大於長度拒絕；回呼回答 `nullopt`、NaN、負數都拒絕（detail `events."<名>".sound_asset_id` 或 `.sync_seconds`）；回呼收到的是該項的 `sound_asset_id`；一項失敗時整體失敗，沒有部分結果 |
| ES9 | Using it 的偏移 | 與 PvP 現有判定相同的數值：(2, 3, 1.5, 0) = 1、(2, 3, 1.5, 0.25) = 0.75、(2, 1, 4, 0) = 8、(0.1, 1, 1, 0.3) = −0.2；clip 長度 0 或負、動作長度 0 或負、任一參數 NaN 或 inf 時沒有值（四個參數各一次） |

另外：`gyo_framework.core` 照舊執行這些測試（`cpu;editor` 標籤，L1 會跑）。

## 3. 突變（`scripts/event_sounds_mutations.json`，全部要 killed、stale 0）

| id | 改壞的是什麼 | 由哪組殺掉 |
|---|---|---|
| es-repeated-key-kept | 重複的鍵被默默接受（後者勝） | ES1 |
| es-unknown-field-ignored | 頂層的未知欄位被忽略 | ES1 |
| es-entry-field-ignored | 項目的未知欄位被忽略 | ES1 |
| es-missing-field-accepted | 項目少一個欄位也接受（用預設值） | ES1 |
| es-other-schema-read | 別的 schema 照讀 | ES2 |
| es-other-version-read | 別的 version 照讀 | ES2 |
| es-version-not-integer | `1.0` 當作 1 | ES2 |
| es-empty-anim-id | 空的 `animation_events_asset_id` 接受 | ES3 |
| es-empty-events | 空的 `events` 接受 | ES4 |
| es-empty-name | 空的事件名接受 | ES4 |
| es-empty-sound-id | 空的 `sound_asset_id` 接受 | ES5 |
| es-negative-sync | 負的 `sync_seconds` 接受 | ES6 |
| es-nonfinite-sync | 結構層不檢查 `sync_seconds` 是否有限 | ES6 |
| es-gain-unchecked | 不呼叫 `IsValidGain` | ES6 |
| es-gain-clamped | 超出範圍的 gain 夾到 kMaxGain 而不是拒絕 | ES6 |
| es-bind-unknown-event | 不在任何 clip 的事件名接受 | ES7 |
| es-bind-first-clip-only | 只找第一個 clip 的事件名 | ES7 |
| es-bind-sync-beyond | 不比較 `sync_seconds` 與長度 | ES8 |
| es-bind-sync-boundary | 等於長度也拒絕（`>=`） | ES8 |
| es-bind-unanswered | 回呼回答 `nullopt` 的項目接受 | ES8 |
| es-bind-drop | 失敗的項目被丟掉，其餘照綁 | ES8 |
| es-bind-wrong-id | 回呼收到事件名而不是 `sound_asset_id` | ES8 |
| es-offset-no-stretch | 偏移不乘動作／clip 的比例 | ES9 |
| es-offset-sign | 加上而不是減去對齊點 | ES9 |
| es-offset-zero-clip | clip 長度 0 也算出值 | ES9 |
| es-offset-nonfinite | 不檢查參數是否有限 | ES9 |
| es-guard-audio-backend-listed | 守衛的核心正向清單加了 `GYO::AudioBackendSDL` | 守衛自測（下一節） |
| es-link-model-json-public | `GYO::Model` 把 nlohmann 改成 PUBLIC（暫時改 Engine 的 CMake，只在突變執行時） | configure 時的連結檢查（`expect_build_failure`） |

共 28 條。另外，現有的 framework 突變目錄（`docs/architecture/plans/framework-rework/scripts/` 與 `docs/tools/plans/v8/scripts/framework_mutations.json`）裡，find 落在這批改動範圍的、或殺它的測試被改的，依 D184 重跑，判定照它們原本的宣告；第 2 批開始時列出這些 id。

## 4. 守衛的變更（AGENTS §14.4）

**變更**：`tests/framework/framework_dependency_guard.py` 的核心正向清單（`CORE["targets"]`）加 `GYO::Model`、`GYO::Audio`。核心 `framework/CMakeLists.txt` 以 PUBLIC 連 `GYO::Model`（公開標頭用 `AnimationEventSet`），PRIVATE 連 `GYO::Audio`（只在 .cpp 用 `IsValidGain`）。JSON 綁定的清單不變（它經 `GYO::ToolFramework` 拿到，CMake 不必具名）。include 規則不變（`engine/` 本來就允許，`*Native` 仍擋）。

1. **守住的性質**：核心的 CMake 只具名清單上的 target；每一條新的依賴邊都留下紀錄，SDL、`*Native` 與第三方 target 進來就改變 DI 2。
2. **放寬面**：核心的 CMake 具名 `GYO::Model` 或 `GYO::Audio`，以前失敗，之後通過。
3. **收緊面**：無。
4. **放寬後，原性質由誰守住**：兩者都是非 `*Native` 的 Engine target，DI 2 明文允許（「It may use any Engine module」；加入正向清單照 DI 2，守衛變更照 §14.4）。經由它們間接帶進的東西仍由 configure 時的連結檢查守住：`GYO::Model` 的 nlohmann 是 PRIVATE（`$<LINK_ONLY:>`，不往下追），改成 PUBLIC 就失敗（突變 es-link-model-json-public）；`GYO::Audio` 只連 `GYO::Base`、`GYO::Time`。`GYO::AudioBackendSDL`（帶 SDL）仍不在核心清單上（守衛自測新增「核心具名 `GYO::AudioBackendSDL` 失敗」，突變 es-guard-audio-backend-listed）。DI 1 不受影響：兩者都在 `engine/CMakeLists.txt` 無條件建置（`add_subdirectory(audio)`、`add_subdirectory(model)` 不在 `if` 裡），framework 不因此多出建置條件。
5. **根據與證據**：`docs/framework/architecture.md` DI 2；`engine/model/CMakeLists.txt:4`（`PRIVATE nlohmann_json::nlohmann_json`）；`engine/audio/CMakeLists.txt:16`（`PUBLIC GYO::Base GYO::Time`）。第 2 批的自測、突變與 configure 輸出存到使用者的 Mac，主 checkout `build/evidence/event-sounds/`。

有放寬面，所以照 §14.4：第 2 批做突變與検証者（D-es-2）的核對後，交使用者核准，核准前不提交守衛。

## 5. 受け入れ（第 2 批結束的條件）

- 突變 28/28 killed、stale 0；D184 的重跑照原宣告全部 killed。第一次的結果不成立時先回報，不重跑（§14.1）。
- test 樹（`build/target/_build/test`）建置成功、全量 CTest 全過；守衛與自測通過。
- 不選工具、只選一個工具的兩種組態 configure 成功（framework 的建置條件沒變）。
- 審査者（Opus 5.5／high）的 blocker、major 都處理；検証者（D-es-2）核對本節 4 的 1～5 與突變結果。
- 契約文件與程式一致：Validation 1～8、DI 1～6 的守衛都指得到實際的測試與突變。

## 範圍的限界

- 只有 PvP 讀今天的對應；Model Viewer 尚未開工（D-es-4）。第二個讀者出現前，契約裡「兩個產品」的一半是預測。
- 寫入（序列化）不在第 1 版：Model Viewer 要編輯時，寫入函式另開一批（framework 回傳文字，不寫檔）。
- PvP 的 `SoundCuesTests.cpp`「event sounds name events of the weapon's gyo.anim_events」是產品側的對齊測試，遷移後由 framework 的綁定取代；那是產品會話的事。

## 補記（第 2 批實作時，2026-11-04）

- **突變的增減**（只收緊；宣告的「改壞什麼」與「由哪組殺掉」沒有放寬）：
  - 審査者之後補 4 條：es-order-unchecked（ES4）、es-events-not-object（ES4）、es-string-type（ES3）、es-number-type（ES6）。審査者另提的「文件／項目是物件」兩條是等價突變（拿掉後 `RequireExactKeys` 以相同路徑拒絕），不列。
  - 使用者核准把偏移函式收緊為「結果不是有限值就沒有值」（「照建議處理」）：新增 es-offset-overflow（ES9）；es-offset-zero-clip 在收緊後變成等價（長度 0 的結果是 inf 或 NaN，由結果檢查擋下），改成「拿掉 clip 長度的檢查」，由負長度的案例殺掉；es-offset-no-stretch、es-offset-sign 改錨到新的那一行。共 33 條。
- **ES6 的措辭**：`1e39` 不是「轉成 float 是 inf」，float 裝不下的數在轉型前就拒絕（超出範圍的轉型是未定義行為）。判定不變。
- **ES9 的收緊**：加 (1e300, 1e-10, 1e10, 0) 與 (1, 1e-300, 1e300, 0) 兩個溢位案例，都要沒有值。這是使用者核准的語意變更（DI 6 的函式），和 PvP 現有版本不同；PvP 遷移時改用 framework 的版本。
- **D184 的判定不成立與處理**：第一次重跑 stale 3（fw-guard-core-json-link 在本批之前就已 stale；fw-guard-core-ui-dropped、fw-cat-json-missing 是本批改了錨點）。使用者核准改錨（「照建議處理」）：三條的意圖與預期訊息不變，只改 find／replace 到現在的文字，預檢後只重跑這三條。第一次的結果保留在證據資料夾（§14.1）。
- **守衛變更**：使用者核准（「照建議處理」）。

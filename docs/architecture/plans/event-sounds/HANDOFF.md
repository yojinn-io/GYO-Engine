# 動畫事件 → 聲音（`gyo.event_sounds`）：交接

## 代號對照

| 代號 | 白話 |
|---|---|
| v8-arch-event-sounds-contract-doc | 第 1 批：契約文件、DI、事前宣告 |
| v8-arch-event-sounds-impl | 第 2 批：核心＋JSON 綁定＋測試＋突變＋守衛 |

## 現況

- 2026-11-04 13:31 JST 開工（本卡由架構會話代為轉達，使用者 2026-11-04 的三點決定見 [README](README.md)）。
- 基底：PR #142（`claude/arch-layer-formats`）**尚未合併**（state OPEN）。本分支 `claude/recursing-dijkstra-38a360` 自 master `e15e5e41` fast-forward 到 #142 的 `ad348a0c`，以它為基底。PR 先以 `claude/arch-layer-formats` 為 base；#142 合併後改回 master。
- 讀過：AGENTS.md、agents/navigator.md、agents/effort-gearbox.md、docs/framework/architecture.md（#142 版）、docs/architecture/animation-events.md、#142 的 diff、工具線的請求全文、PvP 現況程式（SoundCues、PvpSound、PvpApplication 換彈段）、framework 的 catalog 先例、守衛與連結檢查、anim_events 的測試與突變清單。
- 第 1 批的事前陳述已交使用者（本檔「關卡」），核准前不寫契約文件。
- 2026-11-04，使用者把主會話的模型切成 Opus 5.5，並說「照這份計劃開工」。第 1 批開工（檔位 high）。
- 第 1 批的發現：Engine 的事件名規則（`engine/model/src/AnimationEvents.cpp` 的 `ValidEventName`）在匿名 namespace，沒有公開。framework 照卡片自己再寫一份，會碰到 framework 的 Replacement Criteria「copies an Engine rule that no shared fixture checks」。宣告提出替代：解析時只檢查非空，字元與長度由綁定的「事件名必須等於所引用集合裡的名字」間接保證（那些名字已由 Engine 驗證）。等使用者決定。
- 第 1 批的調整：綁定結果改為「驗證過的對應表＋`Find(eventName)`」，不再依 clip 列出事件（兩個呼叫者都已經從 Engine 拿到每個 clip 的事件，只需要逐名查聲音；依 clip 再列一份等於複製 Engine 的資料）。README 的調整表已改。

- 2026-11-04 13:50 JST：第 1 批的文件寫好，停在關卡 2（工作樹，未提交）：[契約](../../../framework/event-sounds.md)（DI 1～6、Replacement Criteria 1～4、Layers 檢查 2～4）、`docs/framework/architecture.md` 的 tool part 與 Using it、`docs/architecture.md` 的 framework 列、[事前宣告](contract-declaration.md)（ES0～ES9、突變 28 條、守衛變更的 §14.4 1～5、待決 D-es-1～4）。第 1 批至今約 0.5 h（估 1.5～2.5 h）。核准後與 dev_log 同一變更提交。
- 2026-11-04，使用者：「照建議核准」。事前宣告與 D-es-1～4 都採建議（D-es-1 (b)：事件名的字元規則由綁定間接保證；D-es-2：検証者 Fable 5.1／high；D-es-3：加 DI 6；D-es-4：Model Viewer 是使用者接受的預測壓力）。第 1 批完成，實際約 0.5 h。第 2 批 v8-arch-event-sounds-impl 開工（13:50 JST）。

- 第 2 批進度：核心（`EventSounds.hpp`／`.cpp`、`FrameworkError.hpp` 的 `EventSoundsErrorCode`）、JSON 綁定（`EventSoundsText.hpp`／`.cpp`）、CMake（核心 PUBLIC `GYO::Model`、PRIVATE `GYO::Audio`）、測試 ES0～ES9、守衛正向清單＋自測 3 條、突變目錄 28 條（預檢：每條 find 恰好一次）。守衛 Red＝新自測對舊守衛 43 條中 1 條失敗（`test_model_and_audio_targets_in_the_core_pass`），Green＝43/43、真實守衛通過（`guard_red_selftest.txt`、`guard_green_selftest.txt`）。test 樹建置中；審査者（Opus 5.5／high）已啟動。
- 實作中的判斷：JSON 數字轉成 float 前先確認 float 裝得下（`1e39` 轉 float 是未定義行為，不是 inf）；這是「能不能表示」的檢查，範圍仍由 Engine 的 `IsValidGain` 判定。宣告 ES6 寫「轉成 float 是 inf」不精確，判定（拒絕、路徑 `events."a".gain`）不變。
- D184 的重跑範圍：既有 framework 突變中，find 落在本批改動檔案的，或由守衛自測、`tests/framework/CMakeLists.txt` 的 configure 檢查殺掉的（兩者本批都改了）。只因 `gyo_framework_tests` 多了一個測試檔而命中、殺它的測試案例沒改的不重跑。

- 審査者（Opus 5.5／high，唯讀）：0 blocker、0 major、7 minor、6 nit。處理：
  - 改了：標頭寫明 `events` 必須是 Engine `ParseAnimationEvents` 的結果、`soundSeconds` 的呼叫順序與例外不攔、`Find` 依賴順序；契約規則 6 寫明 gain 是「float 裝得下、四捨五入成 float 後由 `IsValidGain` 判定」（`8.0000001` → 8 接受、`-1e-46` → −0 接受）；規則 7 寫明前提；Using it 的公式照程式的運算順序；測試的偏移先 REQUIRE 有值再比較；測試裡的 `Magazine In` 改成 `Some Event`（DI 1 的合成資料）；宣告 ES6 的 1e39 措辭更正（判定不變）。
  - 補突變 4 條（只收緊，§14.4）：es-order-unchecked、es-events-not-object、es-string-type、es-number-type。審査者另提的「文件／項目是物件」兩條是等價突變（拿掉後 `RequireExactKeys` 以相同路徑拒絕），不列。突變目錄 32 條。
  - 記為限界：float 範圍檢查（`EventSoundsText.cpp`）沒有突變能殺（IEEE 上超範圍轉型實際得 inf，`IsValidGain` 仍拒絕），只由審查守住；陣列內物件的錯誤路徑不精確（結果仍是拒絕）；嚴格 JSON 的輔助函式和 Engine 的 `AnimationEvents.cpp` 幾乎一樣（第二次出現，code smell，本批不動）。
  - **交使用者決定**：`EventSoundOffsetSeconds(1e300, 1e-10, 1e10, 0)` 這類有限輸入會溢位成 inf 並回傳（PvP 現有版本相同）。改成「結果不是有限值就沒有值」是收緊，但會改 DI 6 的函式語意與卡片的「語意照 PvP」，本批不改。
- 驗證：test 樹建置成功、全量 CTest 125/125（77.6 s，`ctest_full.txt`）；ES 10 個案例 188 個斷言通過。不選工具的 dev 樹 configure 成功、沒有 framework target。
- 突變（事前宣告）：event_sounds 32/32 killed、stale 0（`mutations_event_sounds.txt`）。
- 環境問題兩件（§14.1，都不是受け入れ判定的失敗，記錄保留）：
  - 只選 audio_editor 的 configure 第一次失敗（連結檢查報核心經 Model 拿到 nlohmann）。原因：和突變執行同時跑，執行器當時把 `engine/model/CMakeLists.txt` 暫時改成 es-link-model-json-public 的版本（同一個工作樹）。突變結束、工作樹還原後重做 configure 成功（`configure_audio_only_during_mutations.txt`、`configure_audio_only.txt`）。之後 configure、建置不和突變並行。
  - D184 第一次 0/0（計測 0 件）：zsh 不拆沒加引號的變數，`--only` 收到一整串、沒有選到任何突變。呼叫寫錯，不是突變結果。改用 `${=ids}` 重跑（`mutations_d184_*_zero_selected.txt` 保留）。

- **停止（§14.1：宣告的判定不成立）**。D184 重跑：framework_shell 5/5、framework 16/18、framework_catalog 1/2 killed；stale 3（`mutations_d184_*.txt`）。宣告的判定是「照原宣告全部 killed」，不成立，重跑或改突變定義前等使用者決定。
  - fw-guard-core-json-link：find `    PUBLIC GYO::Engine\n` 在 HEAD 就不存在（`7224db3d`，v8-input-ui-adapter IU-1 改了那一行），本批之前就已 stale，不是本批造成。
  - fw-guard-core-ui-dropped（守衛核心清單）、fw-cat-json-missing（JSON 綁定的 `add_library`）：本批改了它們錨定的那一行（HEAD 各 1 次、現在 0 次）。
  - 參考（未套用）：三條都改錨到現在的文字，突變的意圖與預期訊息不變；改完預檢（每條 find 恰好一次）後只重跑這三條。
- 検証者（Fable 5.1／high，唯讀）：宣告第 4 節的 1～5 全部 CONFIRMED。新舊守衛各跑 691 個合成輸入：舊失敗→新通過 9 個，消失的發現只有三種（核心的 CMake 具名 `GYO::Model`、`GYO::Audio`，含 `.cmake` 與註解）；舊通過→新失敗 0 個。綁定裡具名兩者、`GYO::AudioBackendSDL`、`*Native`、SDL3、nlohmann 新舊都失敗。es-link-model-json-public 確實測到「經 Model 帶進 nlohmann」這條路。突變計數與 stale 3 條的原因和我的一致。比對腳本與輸出存到 `build/evidence/event-sounds/verifier/`；検証者的原始回報未保存。
- 停在關卡 3：(1) stale 3 條怎麼處理；(2) 偏移函式的溢位；(3) 守衛變更的核准（§14.4，有放寬面）。核准前不提交。（已解除，見下）
- 第 2 批至今約 0.5 h（13:50 起；估 3.5～5.5 h）。

- 2026-11-04，使用者：「照建議處理」。三件事：(1) stale 3 條改錨後只重跑這三條；(2) 偏移函式收緊為「結果不是有限值就沒有值」（ES9 加兩個溢位案例、es-offset-overflow，es-offset-zero-clip 因收緊變等價而重新定義）；(3) 守衛變更核准。
- 結果：test 樹重建成功、全量 CTest 125/125（54.3 s，`ctest_full_2.txt`）、ES 10 案例 190 斷言、守衛通過。改錨 3 條 3/3 killed（`mutations_reanchored_*.txt`）；偏移 5 條 5/5（`mutations_offset_tightened.txt`）；最終程式上本批 33 條完整重跑 33/33 killed、stale 0（`mutations_event_sounds_final.txt`）。D184 合計 25/25 killed（第一次 22 killed＋stale 3，改錨後 3/3）。
- **第 2 批完成**（14:45 JST）。實際約 1 h（13:50 起；估 3.5～5.5 h）；整卡約 1.5 h（估 5～8 h）。dev_log 與本批同一變更提交；PR 的 base 是 `claude/arch-layer-formats`（#142 未合併），#142 合併後改回 master。

## 估時（開工前，2026-11-04）

| 批 | 估 | 依據 |
|---|---|---|
| v8-arch-event-sounds-contract-doc | 1.5～2.5 h | 契約文件比照 animation-events.md 的結構；宣告的規則清單與突變清單可從 anim_events 的 `ae-*`（21 條）對照寫 |
| v8-arch-event-sounds-impl | 3.5～5.5 h | 與 v8-tool-assets-anim-events 同量級（解析＋綁定＋一條規則一組測試＋突變）；多一個綁定模組的檔案與守衛正向清單的 §14.4 流程 |
| 合計 | 5～8 h | 超過 1.5 倍（12 h）或單一問題累計超過 2 h 就停下回報 |

## 模型與檔位（照 agents/navigator.md；上段：新 Data Contract、Design Invariants、守衛正向清單）

| 批 | 実装者 | 審査者 | 検証者（§14.4） |
|---|---|---|---|
| contract-doc | 主會話 Opus 5.5／high | — | — |
| impl | 主會話 Opus 5.5／high | Opus 5.5／high（code review 子 agent，卡片指定） | Fable 5.1／high（卡片指定；只核對 §14.4 的 1～5 與突變結果，不改程式；比主會話強，啟動前取得使用者同意） |

2026-11-04 使用者把主會話從 Fable 5.1 切到 Opus 5.5，與卡片一致。

## 關卡

1. **第 1 批開工前**（現在）：使用者核准第 1 批的事前陳述（本檔與 README 的調整表；主會話的回覆）。
2. **第 2 批開工前**：使用者核准第 1 批交出的事前宣告（`contract-declaration.md`：規則清單、突變清單、守衛變更的 §14.4 1～5）。
3. **第 2 批結束**：審査者與検証者的結果、突變全部 killed、全量 CTest；使用者核准守衛變更後才提交守衛。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/event-sounds/`（git 不追蹤）。第 2 批的突變輸出、CTest 輸出放這裡；審査者與検証者的原始輸出若不保存，在 dev_log 寫明。

# framework v8：v8-arch-event-sounds-impl（`gyo.event_sounds` 的核心、JSON 綁定、測試、突變與守衛）

日期：2026-11-04。分支 `claude/recursing-dijkstra-38a360`（基底 PR #142 `ad348a0c`，#142 尚未合併）。上段（新 Data Contract、Design Invariants、守衛正向清單）。実装者：主會話 Opus 5.5／high；審査者：Opus 5.5／high 子 Agent（唯讀）；§14.4 検証者：Fable 5.1／high 子 Agent（唯讀，使用者同意 D-es-2）。估 3.5～5.5 h，實際約 1 h。

**狀態：** 完成（Mac 上的 test 樹建置、全量 CTest、兩種組態的 configure、突變）。CI 的 L1（run 41781301280）linux-x64、macos-arm64、windows-x64 第一次就全部 success。

件數の突き合わせ：一致（本批突變：宣告 33・執行 33・killed 33・stale 0。D184 重跑：宣告 25・執行 25・killed 25，其中第一次 stale 3，經使用者核准改錨後 3/3）

相關文件：[契約](../framework/event-sounds.md)、[事前宣告與補記](../architecture/plans/event-sounds/contract-declaration.md)、[計畫](../architecture/plans/event-sounds/README.md)、[HANDOFF](../architecture/plans/event-sounds/HANDOFF.md)。

## 做了什麼

- **核心**（`GYO::ToolFramework`）：`gyo/framework/EventSounds.hpp`／`src/EventSounds.cpp`：`EventSound`、`EventSoundSet`、`CheckEventSoundSet`（值的規則；gain 由 `Engine::Audio::IsValidGain` 判定）、`BindEventSounds`（事件名對 Engine 的 `AnimationEventSet`、片段長度由呼叫者的回呼回答，整體成敗）、`BoundEventSounds::Find`、`EventSoundOffsetSeconds`。`FrameworkError.hpp` 加 `EventSoundsErrorCode`（`InvalidEventSounds`、`UnboundEventSounds`）。核心 PUBLIC 連 `GYO::Model`、PRIVATE 連 `GYO::Audio`。
- **JSON 綁定**（`GYO::ToolFrameworkJson`）：`gyo/framework/json/EventSoundsText.hpp`／`src/EventSoundsText.cpp`：`ParseEventSounds`（欄位精確、重複鍵報出它的路徑、型別），解析後呼叫核心的 `CheckEventSoundSet`。
- **測試**：`tests/framework/EventSoundsTests.cpp` ES0～ES9（合成資料）。
- **守衛**：核心正向清單加 `GYO::Model`、`GYO::Audio`；自測 3 條（核心可連兩者、核心連 `GYO::AudioBackendSDL` 失敗、JSON 綁定具名 `GYO::Model` 失敗）。
- **突變**：`docs/architecture/plans/event-sounds/scripts/event_sounds_mutations.json` 33 條；framework-rework 目錄的 3 條改錨。

## 驗證與經緯

- **建置與測試**：test 樹建置成功、全量 CTest 125/125；ES 10 案例 190 斷言。不選工具的 dev 樹 configure 成功、沒有 framework target；只選 audio_editor 的樹 configure 成功。
- **守衛的 Red／Green**：新自測對舊守衛 43 條中只有 `test_model_and_audio_targets_in_the_core_pass` 失敗；新守衛 43/43、真實守衛通過。
- **審査**（Opus 5.5）：0 blocker、0 major、7 minor、6 nit。改了標頭的前提與例外說明、契約規則 6（gain 四捨五入成 float 後才判定）與 7（集合必須來自 Engine 的解析）、公式的運算順序、測試的解參考與中立名稱；補 4 條突變（只收緊）；等價的 2 條不列。
- **検証**（Fable 5.1）：§14.4 的 1～5 全部確認。新舊守衛各跑 691 個合成輸入：舊失敗→新通過 9 個，消失的發現只有「核心的 CMake 具名 `GYO::Model`、`GYO::Audio`」；舊通過→新失敗 0 個。
- **宣告的判定不成立一次**（§14.1，先回報再依使用者指示處理）：D184 重跑 stale 3。fw-guard-core-json-link 的錨點在 `7224db3d`（IU-1）就不存在，本批之前已 stale；另兩條是本批改了錨點那一行。使用者「照建議處理」：改錨、預檢、只重跑三條，3/3。
- **使用者核准的收緊**：審査者指出偏移函式對有限輸入也會溢位成 inf 並回傳（PvP 的版本相同）。改成結果不是有限值就沒有值；ES9 加兩個案例、新增 es-offset-overflow；es-offset-zero-clip 因此變等價，重新定義為「拿掉 clip 長度的檢查」。
- **環境問題兩件**（不是判定的失敗，記錄保留）：
  - 只選 audio_editor 的 configure 第一次失敗：它和突變執行同時跑，讀到暫時改成 es-link-model-json-public 的 `engine/model/CMakeLists.txt`（同一個工作樹）。工作樹還原後重做成功。之後 configure、建置不和突變並行。
  - D184 第一次 0/0：zsh 不拆沒加引號的變數，`--only` 沒選到任何突變。改用 `${=ids}` 重跑。

## 範圍的限界與留下的事

- `EventSoundsText.cpp` 的「float 裝得下」檢查沒有能殺的突變（IEEE 上超範圍轉型實際得 inf，`IsValidGain` 仍拒絕），只由審查守住。
- 陣列內物件的重複鍵錯誤路徑不精確（結果仍是拒絕）。
- 嚴格 JSON 的輔助函式在 Engine 的 `AnimationEvents.cpp` 與本批的 `EventSoundsText.cpp` 各一份（第二次出現，code smell，記為之後的候選）。
- 第二個讀者（Model Viewer）還沒建（D-es-4）；寫入函式等它要編輯時另開一批。
- 下游的遷移（PvP、Model Viewer）見計畫 README「給下游的交接」。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/event-sounds/`（git 不追蹤）：configure／build／ctest 的輸出、守衛的 Red／Green、全部突變輸出（含第一次 stale 與 0/0 的檔案）、`verifier/` 的比對腳本與輸出。審査者與検証者的原始回報在會話暫存區，未保存。

# 工具 v8：v8-tool-model-viewer-event-sounds

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`（PR #146），接在 `v8-framework-close-request` 之後。開工前用 app 的同步把 master（含 PR #145 的 `gyo.event_sounds`）併進分支（`f5f76a5e`）。

- 段：上段（寫出 framework 格式的檔）。
- 實作者：Opus 5.5／high（主會話）。
- 審查者：Opus 5.5／high（子 agent，一次）。
- 檢驗者（§14.4，守衛的第 2 次放寬）：Opus 5.5／high（子 agent），和審查者、實作者都不是同一個 agent。沒有另一個模型可用。
- 估時 3～5 h；實際約 3 h。

**狀態：**
- 程式、測試、突變、審查都完成。
- 守衛的判定變更（第 2 次），你已核准（「核准」）。
- L1 的結果記在 HANDOFF。

相關文件：
- [事前宣告](../tools/plans/v8/model-viewer.md)：「`-event-sounds`」與「`-event-sounds` 實作後的結果」；
- [守衛的判定變更](../tools/plans/v8/model-viewer-guard-judgment-change.md)，第 2 次；
- [docs/model_viewer/architecture.md](../model_viewer/architecture.md)「Window」。

## 做了什麼

- 核心（`cpu` 可測）：
  - 讀寫：
    - `WriteEventSounds`：sync_seconds 用最短往返的 double 寫，gain 用最短往返的 float 寫；
    - `CheckedEventSoundsText`：寫出的文字交給 framework 的 `ParseEventSounds` 與 `BindEventSounds`，再逐欄比對；
    - `OpenEventSounds`、`SaveEventSounds`、`CreateEventSounds`：開啟、存檔、新建。新建時把新檔登錄進 catalog，這段和 `CreateEvents` 共用 `WriteNewText`（從 `CreateEvents` 抽出來的）。
  - 排程：
    - `ScheduleSounds`：只用 `EventSoundOffsetSeconds` 算出每個聲音在一輪裡的開始時刻；
    - `SoundPlayback`：決定每一輪什麼時候把聲音送進混音器。循環播放時，照最長的提前量提前送幾輪；跳時間、改速度、改循環、改聲音或事件時，先停掉已送出的再重送；暫停就停；夾住模式播到結尾時，讓聲音放完；
    - `SoundSink`：讓測試能接住送出的聲音。
  - `ModelContent::EventSoundsFor`、`LoadSound`：列出聲音檔、經 Engine 的 sound loader 載入聲音。
- 試聽：`PreviewSounds`，用 Engine 的 `Mixer` 加 `SdlAudioOutput` 出聲。
- 視窗的 Sounds 面板：
  - 開啟現有的聲音檔，或新建一個；
  - 為選取的事件新增、移除聲音；
  - 改聲音 id、sync、gain；
  - 存檔。事件檔還沒存檔時拒絕存聲音檔。
- 依賴守衛：正面清單加 `GYO::Audio`、`GYO::AudioBackendSDL`；目標名稱的比對也把後面接 `::` 的名字算成另一個目標（收緊）。

## 驗證與經過

- 本機 macOS，`build/target/_build/ui`：
  - `gyo_model_viewer.core` 33 個 case 通過，其中 S 開頭的 9 個；
  - GPU 預覽通過；
  - `cpu|shader` 119／119 通過（`ctest-all.txt`）。
- 件數の突き合わせ：一致。最後一輪共 9 條：
  - 宣告 3 條（`mvs-own-formula`、`mvs-save-unparsed`、`mvs-gain-as-double`）；
  - 審查後 3 條（`mvs-round-start-without-speed`、`mvs-edit-not-restarted`、`mvs-no-long-lead`）；
  - 守衛 3 條；
  - 9／9 killed。
  - 另外，`CreateEvents` 抽出 `WriteNewText` 之後，`-core` 那 6 條 find 落在改動範圍內的突變也重跑了，6／6 killed（第 2 輪）。
- 突變的經過：
  - 第 1 輪：宣告的 3 條，3／3 killed。
  - 第 2 輪：宣告的 3 條，加上 `-core` 的 6 條，9／9 killed。
  - 第 3 輪：`mvs-no-long-lead` 是 wrong_failure。原因是測試的 `REQUIRE` 擋在預期訊息之前：有殺到突變，但訊息不對。把那個檢查改成直接看預期的時刻（收緊，沒有放寬）後重跑，就是上面的最後一輪。
- 測試本身的錯：S2 裡「晚到的聲音不送」那個 case，我第一次把時間寫錯（0.4 s 和 0.92 s 都在 1 s 之前）。改成 0.5 s；實作沒動。
- 審查的指出與處理：
  1. （高）播放中編輯會讓聲音排兩次：重新綁定時清掉了排程狀態，下一幀就沒有先停掉舊的。修正：排程搬進核心的 `SoundPlayback`，用版本號判斷要不要重排，重排前一定先停。加了突變 `mvs-edit-not-restarted`。
  2. （中）播放中切換循環時，判斷沒有看循環。修正：循環也納入判斷。
  3. （中）聲音的長度和混音器裡的聲音一直留著，不會更新。修正：換內容、開聲音檔時清掉重讀。
  4. （中）混音器拒絕某個聲音時，錯誤訊息被清掉，聲音就悄悄不出聲。修正：另外存成「not heard」的警告。
  5. （中）數值要按 Enter 才生效，直接點存檔會存到舊值。修正：輸入時就生效。
  6. （中）聲音檔會對照還沒存檔的事件來存。修正：事件檔有未存檔的修改時，拒絕存聲音檔。
  7. （低）晚到不超過混音器延遲的聲音被丟掉。修正：允許晚到 60 ms。
  8. （低）提前超過一輪的聲音永遠不出聲。修正：照提前量多送幾輪。加了突變 `mvs-no-long-lead`。
  9. （低）S3 在視窗裡碰不到：視窗只列出指向開著那份事件檔的聲音檔。這一點寫進了宣告的結果。
  10. （低）註解與訊息寫得不對。修正。
  11. （低）視窗裡的排程邏輯沒有測試。修正：搬進核心的 `SoundPlayback`，加 3 個 case；加了突變 `mvs-round-start-without-speed`。
- 守衛（§14.4）：
  - 檢驗者用合成樹比對了新舊守衛，結論是 1、3、5 成立；2、4 有一個例外：允許的名稱後面接 `::` 的名字也會通過。這個舊漏洞已經收緊。
  - 你核准了（「核准」）。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/event-sounds/`，有：
  - `mutations.txt`（9／9）、`mutations-round1.txt`、`mutations-round2.txt`、`mutations-round3.txt`、`mutations-guard.txt`；
  - `guard-verify/`（檢驗者的比對腳本與結果）；
  - `ctest-all.txt`。

## 和事前宣告不同的地方

- 加了新建聲音檔，以及為事件新增、移除聲音（L3 第 5 項需要，而 PvP 沒有現成的聲音檔）。
- 事件檔還沒存檔時，聲音檔不能存。
- 守衛的第 2 次判定變更。

## 發現（不在本批範圍）

- Engine 的 `SoundLoader` 收任何取樣率的聲音，但混音器只收 48 kHz。不是 48 kHz 的聲音，遊戲裡也同樣放不出來。要不要在載入時就拒絕，由 audio 的負責方決定。

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

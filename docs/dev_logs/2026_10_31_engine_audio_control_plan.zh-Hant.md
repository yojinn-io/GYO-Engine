# 2026-10-31 音效控制層重建的計畫（`v8-engine-audio-control-plan`）

Owner：Engine（`GYO::Audio`）。計畫：[CONTROL](../architecture/plans/audio/CONTROL.md)。狀態：草稿，等使用者核准。

## 為什麼做

- 工具線的資產工具 `audio_editor` 要預覽音效。它需要三件事：停止已送出的聲音、輸出執行中換上新轉出的片段、讀出播放位置畫播放頭。
- 這三件事碰到 [audio.md](../architecture/audio.md) 的 Replacement Criteria 1、2，以及 Design Invariant 7（輸出執行中片段不變）。
- 依規則，修補或重建由使用者決定。使用者選「重建控制層」：保留樣本域與時間換算，重新設計命令的路徑與片段的生命期。
- 規劃方式由使用者核准用 ultracode。

## 做了什麼

- 開分支 `claude/v8-engine-audio-control`（切自 origin/master `16326f78`）。
- 跑 workflow `v8-engine-audio-control-plan`（run `wf_38a2c8c8-6db`，約 22 分鐘，5 個 agent）：
  - 提案 3 個，引擎與檔位繼承（Opus 5.5）。角度分別是：先參考成熟方案（miniaudio、SoLoud、FMOD、Wwise、Web Audio、Unity、Bencina／Doumler 的即時規則）、最小改動、即時安全與決定性優先。
  - 審查者 1 個（繼承）：以「最小改動」為基底，嫁接其他兩案的播放頭寫入順序、特性測試、不配置守衛與 64 位 id。審查者也找出提案本身的 5 個缺陷（D1～D5）。
  - 對抗式檢查 1 個（`xhigh-reviewer`，xhigh，同引擎）：blocker 1、major 7、minor 12，另列 7 個測試缺口。
- 把綜合計畫與檢查結果整理成 [CONTROL](../architecture/plans/audio/CONTROL.md)，並逐項處置檢查結果（CONTROL「對抗式檢查的處置」）：
  - blocker：交還片段槽時沒有重設租約，重用的槽會被提早交還。
  - major：handle 前綴溢位、無效 handle 越界讀取、停止的分類順序、Invariant 8 文字矛盾、交錯測試殺不掉寫端突變、卸載沒有收支、租約寫進 `VoiceMixer`（隱藏的 Ownership 變更）。
- 更新 README（進度表）與 HANDOFF（現況、代號、9 項待決事項、時間紀錄）。
- audio.md 與程式都沒有改：Invariants 的修改要等使用者同意（AGENTS §3）。

## 驗證與經緯

- 本批只寫文件，沒有建置與測試。
- 對抗式檢查引用的程式行號抽查過三處，都和 master 相符：`Mixer.cpp:57` 的 `kMaxSubmitters`、`OnsetRecord.cpp:102` 的 `const bool accepted`、`run_mutations.py:110` 的唯一性要求。
- effort-gearbox 建議對抗式檢查用另一個引擎。這次沒有換（同一引擎可能共有盲點），在 HANDOFF 的 `-gears` 建議 `-playhead` 批的檢查改用另一個引擎。

## 證據

- 證據檔案放在主 checkout（`/Users/karasu/Code/Source/GYO-Engine`，Intel Mac）的 `build/evidence/audio-control-20261031/`（git 不追蹤）：
  - `plan-workflow-journal.jsonl`：workflow `wf_38a2c8c8-6db` 每個 agent 的回傳值。
  - `audio-control-plan-synthesis.en.md`：綜合計畫的原文。
  - `audio-control-plan-attack.zh-Hant.md`：對抗式檢查的原文。
- 需求來源：工具線的 `docs/tools/plans/v8/assets.md`（PR #117 的分支 `claude/v8-tool-handoff-cleanup`）。

## 核准之後（同日）

- 使用者：「Approve as recommended」。9 項全部照建議，記入 HANDOFF 的決策紀錄。
- [audio.md](../architecture/audio.md) 的改動：
  - 寫入核准的 Invariants 3、4、6、7、8 與新增的 10、11，以及改寫後的 Replacement Criteria 1～8。
  - 尚未實作的部分標「planned」，並寫明由哪一批成立。
  - Status 下加一段，說明哪些是計畫中的文字。
  - Contracts 與「What it is」仍描述現有程式。
- PLAN.md：範圍的「做」與「不做」加註移入 CONTROL 的項目。
- 偏離計畫一處：`docs/architecture.md` 的 Audio 列與段落描述的是現有程式，改到 `-live` 批再更新。CONTROL 的分批表已同步修改。
- 本批到此結束。實作批 `v8-engine-audio-control-baseline` 等使用者指定。

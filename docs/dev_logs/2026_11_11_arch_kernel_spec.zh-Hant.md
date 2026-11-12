# 2026-11-11 架構：`v8-arch-kernel-spec`（規格 v0.1 → v0.2）

Owner：架構（架構師會話）。計畫：`docs/architecture/plans/kernel/`（README 批次表、HANDOFF）。**本 dev_log 於 2026-11-12 補寫**（圖書管理員 2026-11-12 健檢指出缺漏；AGENTS §15 要求 dev_log 與批同一變更，本批當時漏了）。

## 做了什麼

- ultracode workflow：提案者 Opus×3／high（三個角度：資料模型優先、契約與排程優先、Host 與多 World 優先）、統合者 Fable／high、對抗檢查者 Opus／xhigh、修訂 Fable。產出 `spec.zh-Hant.md` v0.1（1319 行）：A～M 十三節，每節「資料模型／操作／不變量／C++ 實現手段／Bevy 對照與不採的理由」；守衛 J-1～J-47、Design Invariants（DI-KS 15 條）、Replacement Criteria 13 條；K.3 列 17 項待使用者決定。
- 對抗檢查 34 條（7 blocker）全部處置，逐條在文末；B6（時間來源的契約衝突）交使用者，後成 K.3 第 11 項。
- 架構師逐節讀過；K.3 的 17 項由使用者在側邊會話逐項決定（2026-11-11），每項一個 commit 反映進 spec 與 target 部件文件（第 6 項新設 `target/runtime-composition`、第 12／14／15／17 項新設 `target/system-contract`、`target/error-handling`、第 13 項把 17 項對照表放 `target/README`），spec 升 v0.2。
- 合併：PR #179（`claude/arch-kernel-mechanism`，merge `ab7fa224`）。v0.3 的五輪對抗／複核另見 `2026_11_12_arch_kernel_spec_v03.zh-Hant.md`。

## 驗證與經緯

- 受理判定（README `-spec` 列）：對抗檢查者找不到「規格允許、但會破壞 (c) 區四處契約或獨占寫入」的反例；找到則回提案。結果：7 條 blocker 都是規格內部矛盾或漏寫（Host 定義、多寫者、時間來源等），無一是「規格允許卻破壞契約」的反例；全部以修訂處置，不改程式。
- 當時的停點：B6 需使用者決定，K.3 共 17 項；規格在使用者決定前不視為定稿（README 當時標「待使用者定 K.3」）。

## 證據

- 對抗檢查 34 條與處置：`spec.zh-Hant.md` 文末「對抗檢查」節（repo 內）。
- K.3 的 17 項與決定：`docs/architecture/target/README.md` 對照表；HANDOFF 決定表 `v8-arch-kernel-*` 各列。
- ultracode 的提案與統合逐字稿：**未保存**（當時沒有存進 `build/evidence/`；AGENTS §15 要求之後的批次照做，-spec-v03 起已改為每輪報告存 `build/evidence/`）。

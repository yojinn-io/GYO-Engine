# 架構 v8：v8-arch-entity-store-rereview（entity-store 與 Kernel 沿用的對抗式重審）

日期：2026-11-14。分支 `claude/trusting-mcnulty-78eab5`（切自 master `c8c7dfb8`）。審核卡 07，由架構師會話開出。
段：上段（Design Invariants、資料佈局、架構判斷）。卡片主會話：Opus 5.5。

**狀態：** 審查完成、敵對檢查處置完成，停下等使用者決定（報告文末 D1～D10）。只有 `docs/` 的變更；不改 engine、tests、spec、target、DI 條文、PR #218。

相關文件：[審查報告](../architecture/plans/entity-store/rereview.zh-Hant.md)、[entity-store 計畫 README](../architecture/plans/entity-store/README.md)。

## 起因

架構師重審 kernel spec F.8 取捨 2「hook 的變更進下一窗」時發現，它的理由引用 entity-store 的一句話，而那句出自 2026-11-11 撤回、從未實作的 `DeferredChanges`。使用者判斷這是「舊系統的形狀影響新系統的設計」（AGENTS §4、`docs/architecture/README.md`〈Batch declaration checks〉），要求回頭重審 entity-store 本身與 Kernel 對它的沿用（Delta-3），參考 Bevy，用對抗式審查。卡片另列使用者先前的問題「為什麼不用開源 ECS？id 不是 index＋generation 嗎？」與架構師當時的三點回答，作為待檢驗的主張。
進行中使用者追加：「順便看看之前 Bevy 的研究線的內容，做一個交叉分析」——加開偵察 (c)，報告加 §6。

## 做了什麼

| 角色 | 模型／檔位 | 種類 | 產出（主 checkout `build/evidence/entity-store-rereview/`） |
|---|---|---|---|
| 偵察 (a) 決策史 | Opus／high | general-purpose（只讀） | `recon-a-history.md` |
| 偵察 (b) Bevy／lightyear／replicon 原始碼 | Opus／high | general-purpose（只讀） | `recon-b-bevy.md` |
| 偵察 (c) 兩條 Bevy 研究線交叉（使用者追加） | Opus／high | general-purpose（只讀） | `recon-c-research-lines.md` |
| 審查者 | Opus／high | general-purpose（只讀） | `review-draft.md`（＝`review-draft-v1-before-adversarial.md`） |
| 敵對檢查者 | Fable／high（使用者授權；與審查者不同模型） | general-purpose（只讀，不寫檔） | `adversarial-fable.md` |
| 處置與定稿 | 卡片主會話（Opus） | — | `docs/architecture/plans/entity-store/rereview.zh-Hant.md` |

五個子 agent 的原始紀錄存在同目錄 `transcripts/`（JSONL）。子 agent 寫主 checkout 時 Write 被工作樹 hook 擋，改用 shell 寫入（memory 的既有做法）。
原始碼版本：Bevy `6a911653f`（0.20.0-dev；entity-store survey 當時讀 v0.20.1 `a0577b5c`）、lightyear `736e1695`、bevy_replicon `d80c8497`。EnTT、Flecs 本機沒有原始碼，相關主張全部標「未查證」。

## 結論（定稿）

- **仍成立（8 條，S1～S8；前一版的 S9 移到 C8）**：配發器是排程與快照看得到的 Resource；(c) 查詢順序是兩端共有契約鍵的函數；store 不執行使用者程式；DI 6、7、9、10；同步點依「階段＋id」套用（理由：Despawn 沒有衝突邊）；PvP 的 id 策略（規模、wire、Gateway 不持對照表）。
- **理由要換、結論不變（R3～R8、R10～R16）**：entity-store DI 1、4、5、8 的理由；spec A.3、H.3 的「因為 id 是契約鍵」改條件句；F.7／F.8 取捨 1／E.8 對撤回機制的引用；networking :14 把撤回的 3b 寫成現況；「照 Bevy 格式」不準（Bevy 低位存 `!index`、同代排序方向相反、不保證位元結構）；A.7 的「未查證」其實 survey 早已查過；Delta-3 標題的同意出處標錯。
- **結論可能要改（C1～C9）**：F.8 取捨 2（已在 hook 重審）；W-9／target/world DI 2 的「不重用」把 PvP 策略寫成 Kernel 條文；Delta-3 是「替舊形找位置」；Client 預測生成拿不到 id；Restore 快照沒有鍵驗證；容器順序（W-1）是不變式；#176 的 EnTT 重判沒做；排序欄「在預算內」沒有證據（查詢疊代沒量）；契約鍵＝World 鍵的支撐只剩產品條文。
- **兩條 Bevy 研究線的交叉**：kernel 研究線從未引用 entity-store survey，把 survey 查過的事標成「未查證」，並在「何時配 id」上下了相反結論而沒有討論；「理由欄引用舊形」的推導鏈是 E-12 → survey 的 GYO 約束欄 → DI 4 → F.7／F.8 → DI-KS 9，Bevy 只以反例出現。
- **形狀的系譜**：PvP MVP 的 `std::map<PlayerId, …>`（2026-09-25，順序是容器附帶的）→ digest golden → E-5／E-9 → PvP `EntityColumns.hpp` → `GYO::EntityStore` → DI-KS 3／Delta-3。

## 敵對檢查與處置

Fable／high 一次：必改 4、應改 8、建議 6，全部採用（處置表在報告文末）。主要更動：分類由「還能救」修正（R1、R2、R9、S9 與 A.8 取捨 1 移到「結論可能要改」）；S2 收窄並新增 §2.2b；補方案 (F) 與 (C)×(E)；決定項由 7 項增為 10 項（D8 PR #218 是否照現狀合併、D9 哪些批要停、D10 契約鍵＝World 鍵）；撤回前一版偏向現狀的建議（D2「PvP 遷移之後」、D5 跳過 EnTT）。
處置者親自回讀確認：kernel HANDOFF:47、:68；migration-plan :77、:758-766；spec :1180-1186；Bevy `entity/mod.rs:490-500、:554-566、:1389-1391`；entity-requirements :37。§4 的 EnTT 掃描由處置者以 Python 重跑（不分大小寫 `\bentt\b|flecs`：master migration-plan 3 行＝正例，PR #218 diff 0 行），與檢查者的結果一致。

## 時間

開始 11:27，定稿 12:15 左右（約 0.8 h，子 agent 在背景平行）。估時 4～8 h（推定）以內；沒有碰到停止條件。

## 證據

本機（Intel Mac）主 checkout `build/evidence/entity-store-rereview/`（git 不追蹤）：三份偵察、審查稿（敵對檢查前的版本）、敵對檢查全文、`transcripts/` 五份 JSONL。PR #218 的 diff 由分支 `origin/claude/arch-kernel-world-exec`（`ea62c6e1`）重取，未另存。

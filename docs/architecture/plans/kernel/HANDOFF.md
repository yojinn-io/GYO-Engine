# Kernel 研究線：交接

## 現況

- 2026-11-10 使用者提出願景稿並要求架構師分析；討論定案：Kernel 應進 Engine（中性），framework 強而 Engine 弱的味道＝framework 在代替不存在的 Kernel；Match 的 Stage 是固定序列；C++ 以顯式存取宣告代替借用檢查；協定先設計、實作薄、先遷移一個 host 量成本。
- 2026-11-10 三個只讀子代理（Opus）審閱 bevy_app／bevy_ecs、bevy_replicon 0.45.5、lightyear 0.31.0；報告在 `build/evidence/kernel-study/`。結論：八條不變量；Bevy 的實作選擇（插入順序平手、ambiguity 預設關、多執行緒 executor、Observer 依雜湊順序、ComponentId 依首次使用、並行 entity allocator、變更 tick≠遊戲 tick）不是設計思想，照抄會在核心埋債。
- 2026-11-10 網路設計稿（networking.zh-Hant.md）；五個子代理看渲染、資產、平台、能力 plugin、OS／IO；blueprint.zh-Hant.md 寫成（DI-K1～K7 候選）。
- 2026-11-11 **架構師的誤讀與更正**：架構師默認「改造現有 GYO」並畫了產品掛接圖，沒把「GYO2 還是改造」擺成判斷；被糾正後又當成「決定了 GYO2」。使用者澄清：要的是 Bevy 的抽象能力帶入 GYO，改現有不是 GYO2，判斷的是中間層逐模組包或重寫；改造完成前功能線停。使用者說「先不定案」，之後只對理解。
- 2026-11-11 使用者提供「GYO 架構設計思想」v0.2；架構師依理解畫三張圖（Runtime 機制、網路作為 System、控制面修訂）；使用者提供審查（4 修正 2 補充：多寫者非錯誤、衝突邊≠順序邊、決定性≠序列化、契約分兩半、World 懂儲存契約、邊界措辭與事件時機）；全部採納並加架構師兩條主張（(c) 區歧義＝啟動錯誤；(c) 區並行只在有逐位元守衛時）。寫成 mechanism.zh-Hant.md，blueprint 降為輸入。
- 2026-11-11 決定性等級卡完成（PR #177）：一條邊界六區，不是全部一級；預測核心今天也不是 (c)（win1 2 步 ULP 差異，原因未明，E0 偵測批）。mechanism §3.3 引用。

## 決定（使用者）

| 代號 | 內容 |
|---|---|
| `v8-arch-kernel-foundation-first` | 2026-11-10。基盤的協定不等壓力、先設計；功能層才等壓力。寫進 AGENTS §2 的修訂待風紀委員處理（使用者同意後）。 |
| `v8-arch-kernel-bring-into-gyo` | 2026-11-11。要的是 Bevy 的抽象能力（World／System Contract／Scheduler／Executor）帶入現有 GYO，不是 GYO2；架構改造完成前功能線停。 |
| `v8-arch-kernel-pause-mode` | 2026-11-11。停功能線的定義＝(a)：已開工的卡做到下一個關卡停、更新 HANDOFF；不開新功能卡。事故修正是否繼續仍未定。 |
| `v8-arch-kernel-delegation` | 2026-11-11。此次架構調整（含 spike）由架構師全權負責：各批自開自跑、每批關卡回報；本線 docs-only PR 在 CI 綠後架構師直接合併並回報；碰 Design Invariants、`spikes/` 以外的程式、Delta 七項仍先給使用者。使用者封存原 spike 卡（task_8f7c5a80），spike 改由架構師會話的子代理執行。 |
| `v8-arch-kernel-control` | 2026-11-11。使用者：GYO 最大的架構調整，**會話的控制權在主會話**（架構師會話），不交給別的會話或任務卡，避免會話間傳遞造成語意偏差；子代理可以開（使用者澄清「不開並行」指的是控制權，不是禁止子代理）。架構師：各批由主會話開子代理執行、主會話收結果並逐項讀過；原核准的「規格與 spike 同時開」照舊。 |
| `v8-arch-kernel-spike-verdict` | 2026-11-11（側邊會話轉述）。spike 判定 2 照事前宣告原文記「成立」；實作者多發現的攔不到形狀（寫了又寫回、經存下參考只讀、堆積內容、全域狀態、host 碰過同欄、其他執行緒）全部記為**範囲の限界**（spike-report 專節 L1～L8），後續批次的事前宣告必須寫上，直到解除。 |
| `v8-arch-kernel-limits-accept` | 2026-11-11（側邊會話轉述）。上限的處置：接受；World 以外的資料靠 code review（mechanism §3.5 三層）；不改計畫前提。-audit 的第二問改為兩個可檢查的問題：有沒有跨 tick 的指標或參考、有沒有全域可變狀態。 |
| `v8-arch-kernel-spike-pageguard` | 2026-11-11（側邊會話轉述）。新增批 `-spike-pageguard`：debug 版頁保護（mprotect／VirtualProtect）；形狀已定：只對寫設保護、讀放行；只保護標「獨占寫入」的欄；只在 (c) 區 System 執行時開；只在 debug。排在 -spec 之後（欄與 System 的標記要照規格的契約欄位寫）；開工前事前宣告。 |
| `v8-arch-kernel-entity-key` | 2026-11-11（側邊會話轉述，spec K.3 第 1 項）。EntityId 格式＝u64，低 32 index、高 32 generation（Bevy 格式）；Kernel 不看兩段意義，疊代依整數值升冪；配發策略是產品的 Resource，PvP 選單調不重用、generation 恆 0（值與今天逐位元相同，wire／digest 不改）；stale 句柄偵測能力取決於策略。spec A.1、A.8、K.1 第 3／4 條已改，K.2 第 2 條取消；部件文件 `docs/architecture/target/world.zh-Hant.md`。 |
| `v8-arch-kernel-change-stamp` | 2026-11-11（同上，K.3 第 2 項）。接受規格：依 Plan 位置取值的單調 u64 stamp（不重置、不回滾、跨世代不比較）；tick 視角以 tickStamps 換算；(c) 區禁變更過濾；blueprint DI-K5 與 networking §1 第 7 條各改一句。 |
| `v8-arch-docs-next-folder` | 2026-11-11（同上）。新版架構文件與舊版分開、至少分資料夾、不混放：現況留 `docs/architecture/`，目標架構放 `docs/architecture/target/`（圖書管理員 2026-11-11 定：平鋪、檔名＝搬回後的檔名、英文 Status 行）；每份標「目標架構，尚未實作」或「已實作於哪批」；部件遷移完成後搬回、舊的刪或標廢止，不並存；計畫／HANDOFF／dev_log 留 plans/kernel。 |
| `v8-arch-kernel-exclusive-default` | 2026-11-11（側邊會話轉述，spec K.3 第 3 項）。欄與 Resource 預設獨占，登記標 Shared 允許多寫者；進 digest 禁 Shared。獨占＝啟動時數寫者、執行期零成本、非鎖；獨占欄預設由唯一寫者直接寫；多來源影響同一欄時才用漏斗（事件或中間欄，可選；使用者更正：不是標準進料方式）；(c) 區多個 EventWriter 仍要顯式順序；整欄替換只用於 Client 收權威狀態。不採資料庫式執行期仲裁（慢 20～100 倍、買不到決定性）。spec B.3.1 新增含三類資料的表。 |
| `v8-arch-kernel-despawn-not-writer` | 2026-11-11（側邊會話轉述，spec K.3 第 4 項）。採規格：Despawn 不算獨占寫者、不產生衝突邊；(c) 由 Z5 保護；F.2 套用順序寫死（hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource），despawn 依 id 升冪、冪等、對窗開始不存在的 id 無作用；despawn 先於 spawn 為同窗同 id 重連。案例（玩家 10 死亡又退出、玩家 7 擊殺成立）寫進 target/world §4。 |
| `v8-arch-kernel-b-reads-c-ordered` | 2026-11-11（側邊會話轉述，spec K.3 第 5 項）。採規格：讀 (c) 的 (b) System 必須與 (c) 寫者有顯式順序，否則 D-E8；慣例「呈現放模擬之後的 stage」記為限制 LIM-1（成本在呈現 System 數量，產品線遷移時確認；修改條件 K.2 第 3 條）。使用者另要求新架構文件有「限制與修改條件」集中頁 → `target/limits.zh-Hant.md`（LIM-1～LIM-4；位置待圖書管理員）。 |
| `v8-arch-kernel-first-batches` | 2026-11-11。規格（`v8-arch-kernel-spec`，ultracode）、spike（`v8-arch-kernel-spike`）、審查（`v8-arch-kernel-audit`）同時開，audit 等 spec；全部由主會話控制（`v8-arch-kernel-control`）。模型／檔位見 README。 |

## 未定

- 改造期間事故修正（lag win1）是否繼續。
- AGENTS §2／§3 條文：風紀委員已在分支 `claude/judgment-agents-foundation-pressure` 起草（使用者 11-10 直接指示）；架構師 11-11 轉交的三點追加（實作薄、先一個 host 量成本；§3 七項計畫層寫一次；跨產品能力當 Feature）待使用者同意。
- 改造期間事故修正（lag win1）是否繼續。
- 網路：帳本放 Match 還是 Gateway；兩通道分流採不採。
- 決定性 DET-0～DET-3（determinism.md §0）。

## 現況（續）

- 2026-11-11 PR #175、#177、#176、#174 依序合併（#176 合併前加註：ECS 列待 mechanism 規格後重判、D1～D4／D7／D8 延後；determinism.md E4 由 mechanism 吸收、DET-3 時機改 (c) 區遷移前、§4.7 指向 World）。
- 2026-11-11 使用者：「後續我們直接先處理架構問題」→ 定停法 (a)、規格與 spike 同時開。本分支 `claude/arch-kernel-mechanism` 承接規格的產出；spike 在獨立卡的工作樹。

- 2026-11-11 停法 (a) 通知後各線回報：產品線無進行中實作（#167、#168 已合併；#173 待使用者）；工具線無進行中卡（PathDialogRequests 交接未開工，維持）；voxel 線停於 ve-free-camera（#171）。三線 HANDOFF 的停線記錄各自在下次變更時補，不單開 PR。spike 卡 task_8f7c5a80 已掛出；規格 ultracode 背景執行中。

- 2026-11-11 spike 完成並經 Fable／xhigh 對抗審查（R-01～R-14，全部處置或記錄）；三輪突變 32→34 條全殺；分支 `claude/arch-kernel-spike`（5b473017 起）未開 PR。判定 1 待 L1（L1 不建 spikes/，改 workflow 待使用者 R-06）。使用者在側邊會話逐項對審查，R-01 與上限處置已定，R-06 之後待續。

- 2026-11-11 `-spec` 完成：ultracode（提案 Opus×3／high、統合 Fable／high、對抗 Opus／xhigh、修訂 Fable）產出 `spec.zh-Hant.md` 1319 行；對抗檢查 34 條（7 blocker）全部處置，B6（時間來源的契約衝突）待使用者；架構師逐節讀過，判讀見會話回報。K.3 列 17 項待使用者決定（第 16 項已由 R-01 決定）。PR 待開。

## 下一步

- 使用者定 K.3（至少影響 -audit 的第 1、7、8、9、10、11 項）→ `v8-arch-kernel-audit`（事前宣告要列範囲の限界 L1～L8）→ `-spike-pageguard` → 改造批次計畫（§3 Delta 計畫層寫一次）。
- 判定 1 的三平台：等使用者對 spike R-06（是否改 L1 workflow 建 spikes/kernel/）。

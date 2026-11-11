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
| `v8-arch-kernel-first-batches` | 2026-11-11。規格（`v8-arch-kernel-spec`，ultracode）與 spike（`v8-arch-kernel-spike`）同時開；審查（`v8-arch-kernel-audit`）等規格。模型／檔位見 README。 |

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

## 下一步

- `v8-arch-kernel-spec`（ultracode，本會話背景）→ 產出 `spec.zh-Hant.md`，停下交使用者。
- `v8-arch-kernel-spike`（卡）→ 事前宣告見 README；結果回報本線。
- 兩者齊 → `v8-arch-kernel-audit`（逐模組留／包／重寫）→ 中間層成本 → 改造批次計畫（§3 Delta 計畫層寫一次）。

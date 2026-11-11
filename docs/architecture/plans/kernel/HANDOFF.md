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
| `v8-arch-kernel-bring-into-gyo` | 2026-11-11。要的是 Bevy 的抽象能力（World／System Contract／Scheduler／Executor）帶入現有 GYO，不是 GYO2；架構改造完成前功能線停。**停的定義未定**（已開工的做到下一個關卡停，或立即停）。 |

## 未定（使用者說「先不定案」）

- 停功能線的定義；進行中的卡（產品線 pa-broadphase／pm-detect、voxel 編輯器 window 批、framework 檔案對話框交接）與開放 PR（#174、#175、#176、#177）的處置。
- AGENTS §2「不預測壓力」與 §3 Delta 的條文修訂（風紀委員）。
- 改造期間事故修正（lag win1）是否繼續。
- 網路：帳本放 Match 還是 Gateway；兩通道分流採不採。
- 決定性 DET-0～DET-3（determinism.md §0）。
- 判斷所需三樣輸入（mechanism §1）的開工與模型／檔位：規格（ultracode：提案 Opus、統合 Fable、對抗 Opus）、spike（xhigh 局部）、審查（Opus／high）。

## 下一步

使用者讀 mechanism.zh-Hant.md 與三張圖，指出理解偏差；定上面「未定」的項目；說「開始」才開卡。

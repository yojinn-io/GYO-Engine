# Kernel 研究線：交接

## 現況

- 2026-11-10 使用者提出願景稿並要求架構師分析；討論定案：Kernel 應進 Engine（中性），framework 強而 Engine 弱的味道＝framework 在代替不存在的 Kernel；Match 的 Stage 是固定序列；C++ 以顯式存取宣告代替借用檢查；協定先設計、實作薄、先遷移一個 host 量成本。
- 2026-11-10 三個只讀子代理（Opus）審閱 bevy_app／bevy_ecs、bevy_replicon 0.45.5、lightyear 0.31.0；報告在 `build/evidence/kernel-study/`。結論：八條不變量（見 README 與 networking.zh-Hant.md）；Bevy 的實作選擇（插入順序平手、ambiguity 預設關、多執行緒 executor、Observer 依雜湊順序、ComponentId 依首次使用、並行 entity allocator、變更 tick≠遊戲 tick）不是設計思想，照抄會在核心埋債；兩個網路 plugin 自己都在繞過這些。
- 2026-11-10 使用者：網路部分先記成設計稿（本線 networking.zh-Hant.md）；再用同樣規則看 Bevy 的其他部分（渲染、資產、平台、能力 plugin），四個子代理進行中。

## 決定（使用者）

| 代號 | 內容 |
|---|---|
| `v8-arch-kernel-foundation-first` | 2026-11-10。基盤的協定不等壓力、先設計；功能層才等壓力。寫進 AGENTS §2 的修訂待風紀委員處理（使用者同意後）。 |

## 下一步

四份報告回來 → 寫 blueprint.zh-Hant.md（八條不變量＋偏離＋守衛＋RC）→ 交使用者；之後才談 spike 與遷移順序。

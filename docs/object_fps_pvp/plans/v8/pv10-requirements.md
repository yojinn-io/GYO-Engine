# pv10 的需求紀錄（未排）

更新：2026-11-07。Owner：`object_fps_pvp`。**狀態：需求紀錄，不是設計、沒有排期。** 使用者 2026-11-07：「這些需求先紀錄，目前要做的是把地圖處理好。」
前置：[多通道評估](multi-channel-evaluation.md)（2026-10-30，結論「現在不開多通道」）、[protocol-v8](../../protocol-v8.zh-Hant.md) §2a 預留號碼與附錄 B／C、[entity-model](../../entity-model.zh-Hant.md)、[entity-requirements](entity-requirements.md)、場景線的需求 [scene](../../../architecture/plans/scene/README.md)。

## 為什麼現在記

使用者 2026-11-07 提出技能與寵物，擔心「現在不設計後面怕撐不住」。架構師的看法：要現在留的不是通道數，是 **pv10 的結構**；但照使用者決定，先記錄、不設計。
記錄的目的是讓 pv10 設計開始時，功能清單與各自的定義已經在，不必再問一輪（pv8 的教訓：假設 HitPart 夠用而漏了 hitbox，見 memory `protocol-design-covers-named-features`）。

## pv10 設計要逐一點名的功能（使用者 2026-11-07 確認「就這些」）

| # | 功能 | 使用者的定義（2026-11-07） | 初步看到的 wire 需求（設計時驗證） |
|---|---|---|---|
| 1 | 互動物件：門、拉桿、陷阱、生成、邏輯組合 | 見 scene/README | 世界實體的出現／消失／狀態改變走可靠事件；門狀態可進 snapshot（≤ 50 bit）；互動請求走 ShotRequest 預留 10～12 的 oneof |
| 2 | 物品／容器／背包 | 真的物品與背包（塔科夫式）；容器內容只給開啟者 | 可靠事件（出現、消失）；私人資料（背包、開著的容器）只進收件者那一份；撿丟裁決照附錄 B-3 |
| 3 | 技能 | **戰術動作**（加速、瞬間移動、二段跳狀態）＋四類都有：瞬發／指向（射線、投擲）、範圔／持續（領域、buff）、召喚／建造（放牆、放陷阱）、被動／裝備（數值修改） | 動作請求的 oneof 一個分支；冷卻＝私人狀態；結果＝事件或投射物／效果實體；戰術動作會改 Movement 的狀態機（與 pm-vertical 的垂直移動有關） |
| 4 | 寵物 | **類似裝備**：纏繞在第一人稱畫面上，第三人稱（對方視角）也看得到 | 不是 AI 實體：玩家的裝備／表現狀態（PlayerState 的欄或 catalog id），走 snapshot；無腦、無戰鬥 |
| 5 | AI 小怪 | entity-requirements：3 種 ≤ 20，靠 brain | 實體基礎（配發、digest v5、3b）；Body 100 m 的相關性選擇 |
| 6 | Boss | entity-requirements：1 隻 | WorldSnapshot 7／6 預留 BossState |
| 7 | 對局規則 | 計分、擊殺訊息、勝負與結算 | multi-channel-evaluation 的 ⑤：先縮帳面預算、再可靠事件串流 |

## 結構性的四件（架構師的看法，設計時驗證）

1. **每收件者的可靠有序事件串流**（Match→Client；重送狀態在 Match，Gateway 無狀態；評估的選項 C）。
2. **每收件者的相關性選擇**：4 人＋AI＋數百物件放不進 1200 B；Match 依距離與優先度為每個收件者選內容（entity-model §4 的 interest 設計，差 wire 欄位）。
3. **私人資料**：背包、開著的容器、技能冷卻只送本人；不另開傳輸通道，塞進那個人的 snapshot 或事件串流。
4. **動作請求的 oneof**：射擊、互動、用物品、放技能是同一種「某 tick 要做某事」的請求（預留 10～12）。

## 設計時的規則

- 一次設計、一次合併（v7.1 的做法 (a)）；實作分切片（場景線建議的第一條切片：一根拉桿開一扇門）。
- 每項功能推出 wire 需求並對照參考實作；清單上的七項都要出現在設計文件裡，不得「假設夠用」。
- multi-channel-evaluation 的前提（實際流量不緊、先縮預算）要重新驗證：物件與物品把事件量從「擊殺訊息」變成「每局兩萬件」（entity-requirements）。
- 上段；候選 ultracode（提案者 Opus×3、統合 Fable、對抗 Opus；先問使用者）；排在場景 survey 之後，survey 給 1、2 的輸入。

## 追加（架構師 2026-11-11，使用者決定）

- **Match 的 ready 事件**：Client 在收到 Match 的 ready 前停在 loading 狀態；ready 是 wire 上的事件（pv10 的事件串流，或 Welcome 的欄位），之後 FixedUpdate 的 tick 從 0 起算。來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` H.2b（Loading schedule）。
- **Peer 與 Player 分開**：每收件者的 Ledger／Visibility 屬 Peer 實體，與 Player 實體不合一（觀戰者等非 Player 的 Peer）；wire 上的收件者身分對應 Peer。來源：`networking.zh-Hant.md` §3b。

# 事前宣告：World 摘要與訊息（`v8-arch-gyo2-kernel-summary-messages`）

更新：2026-11-23（修訂 2）。Owner：Kernel（GYO2 主會話開的任務卡 `local_b3d28b59`）。**狀態：使用者 2026-11-23 選 (b)（原話「選 (b)」），同日說「開始」，實作中（dev_log [`2026_11_23_gyo2_kernel_summary_messages.zh-Hant.md`](../../../dev_logs/2026_11_23_gyo2_kernel_summary_messages.zh-Hant.md)）。**

根據：
- asset 宣告 [`asset-declaration.zh-Hant.md`](asset-declaration.zh-Hant.md) 第 15 節第 8 項；批次表〈後續小事〉同一列。
- 開卡：使用者 2026-11-23「開 audio 那一側的小批和 Kernel 兩件」（HANDOFF `v8-arch-gyo2-audio-clip-and-kernel-summary-start`）。

讀的版本：`origin/gyo2` 8ed476dd；網路卡分支 `claude/gyo2-net-module-da73cd`（PR #275，未合併）。參考實作：bevy 6a911653f、lightyear 736e1695、bevy_replicon d80c849（本機 `~/Code/GitHome`）。

經過：
- 修訂 0：建議 (e)「訊息與游標都不進摘要」。換模型的敵對式檢查（Fable／high，唯讀，沒有建置）：blocker 2、should 5、nit 4。報告：主 checkout `build/evidence/gyo2-kernel-summary-msg/declaration-check-r0-fable.md`。
- 修訂 1（本版）：照報告改。
  - B1：例一原本用 `PeerConnected`，但 #275 每發一筆都會生成一個實體，(e) 也解決不了。改用不帶生成的 `NetLinkFailed`。
  - B2：「內容進摘要」原本就是規格 D-9 與摘要版本 1；統合卡只加了序號與游標。(b) 就是回到版本 1。
  - S1：GYO 的訊息靠游標可以留很久，「還沒讀的訊息」是真的狀態。
  - **建議因此從 (e) 改成 (b)。**
  - 其餘照報告補：(d) 的真正代價、漏掉的選項 (f)(g)、§14.4 表的寫法、漏列的檔、AGENTS §2／§3／§4 的項目。
- 修訂 2：主會話要求對修訂 1 做窄的複查（AGENTS §4）。Fable／high，唯讀，沒有建置：blocker 0、should 4、nit 6，結論「(b) 站得住」。報告：主 checkout `build/evidence/gyo2-kernel-summary-msg/declaration-check-r1-narrow-fable.md`。照報告補「(b) 還看不見什麼、什麼條件下才解決」（第 4、5、6 節），修正第 8、9 節的幾句（處理表在第 13 節）。

## 1. 問題（先用遊戲講）

### 1.1 例一：網路斷線的通知

Client 的網路 Module 發現連線出錯時，往 World 寫一筆 `NetLinkFailed` 訊息（#275 `NetModule.cpp:50-55`）。出錯的時刻每次不同，也可能只是一瞬間的錯誤。

兩次跑同一場對戰的重播：其中一次中途有一瞬間的連線錯誤，寫了一筆 `NetLinkFailed`。UI 讀了它、顯示一下提示，紀錄也修剪掉了。之後兩邊所有角色的位置、血量都一樣，但從那一刻起，每一幀的 World 摘要都不同。

原因：摘要雜湊每種訊息的**寫入序號**（到目前為止一共寫過幾筆），以及每個讀者的游標**讀到第幾號**。這兩個數字只會往上加，不會回頭。

現在的測試就把這個行為當成規格守著：`tests/common/kernel/MessageCursorTests.cpp:491` 的 `[msg-summary-seq]`——兩份 World 的紀錄都已修剪掉，只差在其中一份多寫過一筆，斷言摘要**不同**。

不在本題範圍的：玩家連線、斷線時，#275 會生成、刪除 `Peer` 實體。實體 id 與配發器本來就進摘要，所以「有人重連」的那一次重播，摘要不管選哪個選項都會不同。那是真的狀態差異，不是本題。

### 1.2 例二：回滾重跑

PvP 的固定步裡，「命中判定」寫一筆 `Hit` 訊息，同一步的「扣血」讀它。回滾的前提 R-2 第 4 條允許這樣寫：訊息在同一步內寫完讀完（`rollback.zh-Hant.md`，使用者 2026-11-22 隨回滾 Module 核准）。

回滾時，第 5～9 步重跑一次，`Hit` 又寫了一次。訊息不回滾（DI-KS 10），所以寫入序號和游標的位置都比直跑的那份多。

結果：血量、位置、存活的實體都和直跑一樣，R-2 成立；但兩份 World 的摘要不同。回滾的等價測試是拿摘要來比的（`rollback.zh-Hant.md` R-2 的守衛），所以會判成「不等價」。

實測（本機 macOS Intel，2026-11-23）：
- 設定照回滾的等價測試 `rollback strategy: R-2`，加一個固定步裡寫 `Hit`、同一步讀它的 System。
- 跑 12 幀；回滾的那份在第 9 幀後確認第 4 步，重跑第 5～9 步。
- 結果：tick 都是 12，兩個實體的血量都一樣（122、72），緩衝裡都沒有留下紀錄，**但摘要不同**（`3:1dc2121f…` 對 `3:51ebe777…`）。
- 檢查者照原始碼對過：差異只在 `nextSeq` 與游標讀到的序號（17 對 12）。
- 實驗的測試沒有 commit；原始碼與輸出在主 checkout `build/evidence/gyo2-kernel-summary-msg/`（`exp-r2-message-test.cpp`、`exp-r2-message.log`）。

### 1.3 用機制的詞說

`World::Summary`（格式版本 3，`engine/ecs/kernel/src/World.cpp:1190-1205`）對訊息雜湊這些：

1. 每種訊息的下一個寫入序號 `nextSeq`；
2. 緩衝裡每筆紀錄的序號，以及經型別雜湊函式算出的內容；
3. 游標表：每個讀者的鍵（訊息、System、掛載點、條件的位置），以及讀到的序號。

1、2 的序號、3 的「讀到的序號」都是歷史的計數，不是現在的狀態。只要歷史裡多寫過一筆，就算那筆早已讀完、修剪掉，摘要也永遠不同。

## 2. 前提

### 2.1 這是 Engine 的事，屬於 Kernel 的機制

- 序號與游標是 Kernel 內部的東西。作者看不到，也改不了；作者唯一能做的是「不要用 `Message`」。
- Engine 自己的 Module 也在寫訊息：回滾的 `RollbackAbandoned`、時間的 `TimeDiscarded`、網路的 `NetLinkFailed` 等（#275）。作者不用訊息也躲不開。
- 所以這不是「作者處理得了，Engine 只要寫清楚」的那一類（對照 `v8-arch-gyo2-event-cursor-reeval-author-side`）。

### 2.2 更正卡片的前提：權威的一致性比對不受影響

卡片寫「權威和回滾的一致性比對會受影響」。查過之後，**權威那一半不成立**：

- 兩端比的是權威 Module 的 digest（`engine/modules/authority/include/engine/authority/Consistency.hpp:7-12`），不是 `World::Summary`。digest 只收決策用的元件與 Resource，**不收訊息**（`Consistency.cpp:145-149`）。
- Engine 裡沒有任何地方呼叫 `World::Summary`。呼叫它的全是測試：
  - Kernel 的決定性測試（D-9、J-44）；
  - 回滾的等價測試（R-2，6 處）；
  - 讀檔（`RestoreFlowTests`）；
  - 生成世界、world_time、world_asset、entity_ids、scenario。

受影響的是**拿摘要來判「兩份 World 一樣」的測試**，包括作者自己寫的。例二就是這種。

### 2.3 系譜：哪一部分是誰定的

| 摘要裡的訊息部分 | 從哪裡來 | 誰定的 |
|---|---|---|
| 每筆紀錄的**內容**（加上筆數） | 規格 D-9 的定義「World 摘要＝所有欄的 keys 與值、Resource 的值、事件 payload」（`plans/kernel/spec.zh-Hant.md:520`，N6）；摘要**版本 1** 照這樣寫（f6b08661 之前的 `World.cpp`：「record count, each payload's bytes. No … seq is hashed」） | 架構師的規格 |
| **寫入序號、紀錄的序號、游標表** | 事件游標宣告 [`event-cursor-declaration.zh-Hant.md`](event-cursor-declaration.zh-Hant.md) 第 3.2 節「驗證與摘要」一列：「World 摘要含游標與寫入序號（摘要涵蓋『World 裡的資料』）」；摘要版本 2（f6b08661，第 3 批第 3 步） | **統合卡照 L17 判**（修訂 16），不是使用者 |
| 逐型別雜湊 | 版本 3（第 4 批） | 使用者 2026-11-20 選 (d)（`v8-arch-gyo2-authority-rollback-missing-hash`） |

之後第 4 批把序號與游標寫進 DI-KS 10、DI-KS 27（使用者 2026-11-22 核准的是第 4 批整份條文，不是逐句挑過這一點）。

GYO 其他地方對訊息的處理：

| 地方 | 訊息算不算 |
|---|---|
| 回滾（DI-KS 10「訊息不回滾」；R-2 的等價定義只有欄、Resource、存活集合） | 不算 |
| 讀檔（存檔不含訊息） | 不算 |
| 權威 Module 的 digest | 不算 |
| 摘要裡的 `KernelSettings`、stamp、tick | 已經以「不是模擬的資料」為由排除（DI-KS 27） |
| World 摘要 | 算（內容、序號、游標都算） |

### 2.4 為什麼當成基盤（AGENTS §2）

`World::Summary` 是 Kernel 的公開介面，它的內容是一個 Format（有版本號）。上面已經疊了：回滾的 R-2 守衛、讀檔、生成世界、world_time、world_asset、entity_ids、scenario 的測試，一個寫死的摘要值（`WorldTests.cpp:519`），還有作者自己的決定性測試。內容每改一次，這些都要跟著重看；疊得越多越貴。所以當成基盤處理。

## 3. 別家怎麼做（主參考 Bevy）

出處在主 checkout `build/evidence/gyo2-kernel-summary-msg/recon-reference-messages-checksums.md`（子 Agent，Opus／high，唯讀；檢查者抽查五處相符）。

### 3.1 Bevy

- `Messages<M>` 是一個 Resource：兩個緩衝，加一個只增不減的 `message_count`（`messages.rs:93-101`）。`clear()` 也不讓計數器回頭。讀者的游標只有 `last_message_count`（`message_cursor.rs:54-57`）。
- 訊息最多留兩次 update，之後靜默丟掉（有 `TimePlugin` 時另有例外，見事件游標宣告頂端的重評註記）。
- **Bevy 本體沒有 World 的雜湊或 checksum。** 序列化的 World 實際上也不含訊息：`Messages<M>` 是泛型、沒有登記型別（`dynamic_world_builder.rs:391-400`）。

### 3.2 Bevy 生態

- **lightyear**：
  - 預測錯誤逐元件比（`prediction/src/registry.rs:595-613`）。
  - 決定性模式有整體 checksum，只雜湊登記了雜湊函式的元件，只算帶 `Deterministic` 標記的實體；Resource 與訊息都不雜湊。
  - 回滾重跑會再寫一次訊息，沒有人還原；有副作用的 System 用 `is_in_rollback` 擋。
  - 網路收到的訊息放在每條連線的元件裡、每幀清掉，不進 Bevy 的 `Messages`。
- **bevy_replicon**：沒有狀態雜湊。Client 送來的訊息寫進 Bevy 的 `Messages<FromClient<M>>`。
- **GGRS／bevy_ggrs**（本機沒有原始碼，憑記憶）：checksum 只收作者選入的元件與 Resource，訊息預設不收。

### 3.3 對照之後能說的、不能說的

- 能說：找到的比對方式都不收訊息緩衝、序號或游標。它們對應的是 GYO 的**權威 digest**，而 digest 已經不收訊息。
- 不能說：「摘要該照 Bevy 不收訊息」。別家**沒有**「整個 World 的摘要」，也沒有「讀者拉住保留」（GYO 的訊息可以一直留到每個讀者都讀過，上限每種 1024 筆；Bevy 最多兩次 update）。摘要與保留都是 GYO 自己的設計，選哪一個選項都是 GYO 自己的定義，和 Bevy 都沒有衝突、不用膠水。
- Bevy 的計數器也只增不減。如果有人把它雜湊進去，會遇到和例一、例二一樣的問題。

## 4. 選項

| | 做法 | 系譜 | 例一 | 例二 | 看不見的差異（代價） | 其他代價 |
|---|---|---|---|---|---|---|
| **(a)** 訊息型別可選「不進摘要」 | `MessageOptions` 加旗標 | asset 第 15 節第 8 項與批次表原本問的 | 有標的解決 | 步裡的訊息也要標 | 有標的型別的一切 | 每個發外部事件的型別都要記得標，包括 Engine 的 Module；沒標的照舊；多一個選項 |
| **(b)** 只雜湊內容（回到版本 1） | 拿掉 `nextSeq`、紀錄的序號、游標表；留下每種訊息的筆數與每筆內容 | 規格 D-9、摘要版本 1；撤回統合卡加的兩樣 | 解決 | 解決，**條件**：比的時候沒有讀者還拉著重跑寫的紀錄（見下） | 誰讀過、誰還沒讀；讀者跑過沒有（見下） | 訊息型別仍要寫雜湊函式（現況） |
| **(c)** 照現況，說明頁要作者對外部事件用 Resource | 不改 Kernel | asset 5.7 的繞法 | 每個 Module 自己用 Resource 做一條佇列 | **不解決**：R-2 允許步裡寫訊息，等價測試卻會失敗 | — | 同一個機制分散到各 Module；#275 要改；R-2 與摘要互相矛盾 |
| **(d)** 只雜湊相對位置 | 不雜湊絕對序號；雜湊內容，以及每個讀者「還沒讀的筆數」 | 修訂 0 新提 | 解決 | 解決 | 幾乎沒有 | 游標的鍵含 System 的索引（`World.cpp:99`、`:979`、`:1202`），照程式讀來跟登記順序走，和 D-9／J-44「對登記順序不敏感」相衝（版本 3 現在就有；J-44 的案例沒有讀者，所以沒測到；**檢查者只讀程式，沒有跑**）。要先換掉游標的鍵，範圍變大 |
| **(e)** 訊息與游標都不進摘要 | 摘要只收存活實體、欄、Resource | R-2 的等價定義、權威 digest | 解決 | 解決 | **還沒讀的訊息**（見下） | 訊息型別不必為摘要寫雜湊函式；`TimeDiscarded`、`RollbackAbandoned`、#275 的四個雜湊函式變成沒人呼叫，要同批拿掉 |
| **(f)** (e)，另給一個彙總未讀訊息的函式 | 摘要照 (e)；另一個唯讀函式雜湊所有未讀訊息 | 檢查者提 | 解決 | 解決 | 要比的人自己記得呼叫第二個函式 | 兩個出口，測試要各比一次；現有的呼叫者都要決定要不要加 |
| **(g)** 只改 R-2 的等價測試，不用摘要 | 回滾的測試照 R-2 自己的定義比 | 檢查者提 | **不解決** | 解決 | — | 摘要的缺陷還在，其他拿摘要比的測試照樣會踩到 |

**(e) 看不見的差異**（GYO 的訊息會留很久，所以這不是小事）：
- 例 A，讀檔：`Hit` 在 PostUpdate 寫，扣血的 System 下一幀 PreUpdate 才讀。存檔不含訊息。測試「讀檔後立刻和原本的 World 比摘要」：(e) 判相同，但那一擊其實丟了；(b)、(d) 判不同。
- 例 B：讀者只有 World 外的效果（播音效、送封包），差異永遠不進摘要。
- 例 C：讀者被 run 條件擋住（暫停），整段暫停期間都看不出來。

**(b) 看不見、或判法和 (e) 不同的地方**（修訂 2 照複查補）：
- 例 D，讀者跑過沒有：沒跑過的讀者不拉住保留（`World.cpp:1045-1053`），所以之後留下哪些紀錄，取決於游標存不存在，而游標表不進 (b) 的摘要。讀檔時：原本的 World 裡某個讀者跑過、之後被暫停擋住；讀檔出來的 World 沒有它的游標。兩邊緩衝都是空的，(b) 判相同。暫停中寫一筆：原本的 World 留著、恢復後讀到；讀檔的那份在 Run 結束就修剪掉。差異要到寫入的那一幀才進摘要。
- 例 E，例二的條件：`Hit` 另有一個 Main 裡的特效讀者，排在 FixedUpdate 之前。回滾的那一幀結束時，回滾的那份還留著重跑寫的 6 筆，直跑的只留 1 筆。R-2 的資料相同，(b) 判**不同**，(e) 判相同。照字面這在 R-2 前提 4 之外（訊息不是在同一步內讀完），但送法原則允許這種讀者。

## 5. 建議：(b)

理由：

1. **只拿掉缺陷**：例一、例二的原因都是「歷史的計數」（序號、游標讀到的位置）。(b) 剛好拿掉這些，其他不動。
2. **最小的撤回，而且撤回的是非使用者的判定**：序號與游標是統合卡照 L17 加的（第 2.3 節）；內容進摘要是規格 D-9 原本的定義。(b) 回到版本 1、照 D-9 的字面。
3. **還沒讀的訊息仍看得見**：GYO 的訊息會留到每個讀者都讀過，「還沒讀的 `Hit`」會影響之後的世界。(e) 看不見（第 4 節例 A～C），(b) 看得見。
4. **游標表跟著拿掉也有實在的理由**：游標的鍵含 System 的索引，會跟登記順序走（(d) 那一列）。
5. 不用新的選項、不用作者記得標什麼；拿掉的程式比加的多。

(b) 的代價：看不出游標——「誰讀過、誰還沒讀」，以及「讀者跑過沒有」（第 4 節例 D）。兩份 World 有同樣的紀錄，只差在游標，摘要相同；差異要等之後的行為改到紀錄、元件或 Resource 才看得出來。這一點寫成 Replacement Criteria（第 8.2 節），出現時重評 (d)。另外，例二要「比的時候沒有讀者還拉著重跑寫的紀錄」才相等（例 E）；拉著的時候 (b) 判不同，這是 (b) 和 (e) 的另一個差別。

和 (e) 比：(e) 更簡單，也讓訊息型別不必寫雜湊函式；但它看不見還沒讀的訊息，而 GYO 的保留比別家長。如果你認為「摘要只管狀態、訊息不是狀態」（和回滾、讀檔、digest 一致），選 (e)；如果你認為「還留著、之後會被讀到的訊息算狀態」，選 (b)。

## 6. 要使用者決定的

1. **【決定】摘要要不要看得到「還沒讀的訊息」？** 看得到 → (b)（建議）；看不到 → (e)。其他選項 (a)(c)(d)(f)(g) 的退法見第 4 節。
   - 選 (b) 時連帶接受：回滾後若有讀者還拉著重跑寫的紀錄，摘要和直跑不同（第 4 節例 E）；新的回滾守衛只在「沒有讀者拉著」的設定下斷言相等。
   - 選 (e) 時連帶接受：還沒讀的訊息完全看不見（第 4 節例 A～C）。
2. **【告知】這會推翻統合卡的判定**：事件游標宣告第 3.2 節「摘要含游標與寫入序號」，以及第 4 批的 DI-KS 10、DI-KS 27 那幾句（第 8 節）。選 (e) 時還會改掉規格 D-9「事件 payload」的原定義。
3. **【告知】守衛的判定會變**（AGENTS §14.4：Design Invariants 的守衛不論有沒有緩的面，都要核准）。五點寫在第 9 節；核准前照 §14.4 做突變測試與另一位檢證者的檢證。
4. **【告知，選 (e) 時】** 使用者的決定 `v8-arch-gyo2-authority-rollback-missing-hash`（缺雜湊函式時摘要報錯）的範圍會縮小：訊息型別不再報錯。選 (b) 時不變。

## 7. 和事件游標的契約對齊

- `v8-arch-gyo2-event-cursor-reeval-author-side`（讀者被長期跳過時的寫法是作者的事）與 RC 24：(b)、(e) 都不改游標的行為、保留、修剪、補讀，只改摘要讀不讀它們。System 契約說明頁第 15 節的寫法不變。
- 說明頁第 15 節「跨過狀態轉換的資料放 Resource」講的是「要留下來的狀態」，和本題不同。本題是「訊息本來就該是訊息，只是摘要不該收歷史的計數」。選 (b) 之後，外部事件照送法原則（`v8-arch-gyo2-event-delivery-rule`）用 `Message` 就好，不必因為摘要改用 Resource。
- J-36 的三案（`[msg-summary-cursors]`、`[msg-summary-seq]`、`[msg-summary-cursor-position]`）是 DI-KS 10 的守衛；選 (b) 時要改（第 9 節）。

## 8. 選 (b) 時要改的（實作在使用者說「開始」之後）

### 8.1 程式

- `engine/ecs/kernel/src/World.cpp`：`Summary` 拿掉 `nextSeq`、每筆的序號、游標表；每種訊息留下筆數與每筆內容（依緩衝順序）。格式版本 3 → 4（`kSummaryVersion`）。`HashMissing` 照舊檢查訊息型別。
- `engine/ecs/kernel/include/engine/kernel/World.hpp`、`Hash.hpp`、`detail/Storage.hpp`（游標的註解，`:265-266`；`:207-210` 的 `HashAt` 在 (b) 下不用改）：註解跟著改。
- `engine/modules/world_asset/include/engine/world_asset/AssetView.hpp:156-157`：「不用 Kernel 的訊息，所以序號不進摘要」的理由過時，註解改寫；asset 宣告 `:538`、`:998` 加更正註記。
- 訊息型別的雜湊函式照舊需要（`TimeDiscarded`、`RollbackAbandoned`、#275 的四個都留著）。

### 8.2 條文（Design Invariants 的變更，Architecture Delta）

- `target/kernel.zh-Hant.md` **DI-KS 10**：「World 摘要（版本 3）含游標表（連同每個游標讀到的序號）與寫入序號；每筆紀錄雜湊序號與內容」改成「World 摘要（版本 4）對每種訊息雜湊筆數與每筆內容（經型別的雜湊函式），不含寫入序號與紀錄的序號（歷史的計數），也不含游標表（游標的有無與位置是狀態，但不收：鍵跟登記順序走，見 Replacement Criteria）」；守衛列（`:54`）跟著改。
- **DI-KS 27**：版本 3 → 4；「Kernel 自己的型別（游標表、訊息紀錄）」改成「訊息紀錄」；守衛清單跟著改。
- 規格 `plans/kernel/spec.zh-Hant.md` **D-9**：不改（「事件 payload」本來就是 (b) 的定義），補一句「不含序號與游標」。
- **Replacement Criteria 新增一條（草案）**：兩份 World 摘要相同、之後才分岔，追到原因是游標（有沒有、讀到哪裡），在產品或 Engine 的 Module 的測試裡觀察到兩次以上；或有 Feature 需要讀者跑過沒有、讀到哪裡也算進「兩份 World 一樣」——重評 (d)（先要讓游標的鍵不跟登記順序走）。（修訂 1 寫的「反覆另外比游標或 `MessagePayloads`」拿掉：游標沒有公開的讀法，`MessagePayloads` 的內容 (b) 已經收。）
- 事件游標宣告第 3.2 節：只在頂端加更正註記（照重評宣告的先例），內文不改寫。
- `rollback.zh-Hant.md` R-2 的守衛列：加新的回滾案。

### 8.3 測試與突變

見第 9 節。

## 9. 判定的變更（AGENTS §14.4，選 (b) 時）

| 判定 | 1. 守的性質 | 2. 緩的面 | 3. 緊的面 | 4. 緩掉的性質還有誰守 | 處理 |
|---|---|---|---|---|---|
| `[msg-summary-cursors]` | 游標表進摘要 | 兩份 World 只差在有沒有某個讀者、**而且留著的紀錄相同**時，摘要相同（讀者排在寫者前、拉住紀錄時，留著的不同，(b) 仍判不同） | 收游標表的實作會失敗 | **無，決定拿掉** | 改成相反：現有的形狀（兩份都修剪光）斷言相同 |
| `[msg-summary-seq]` | 寫入序號進摘要 | 兩份 World 只差在寫過幾筆（都已修剪），摘要相同 | 收寫入序號的實作會失敗 | **無，決定拿掉**（這正是例一要的） | 改成相反 |
| `[msg-summary-cursor-position]` | 游標讀到的位置進摘要 | 只差在某個讀者讀到哪裡，摘要相同 | 收讀到位置的實作會失敗 | **無，決定拿掉**（寫成 Replacement Criteria） | 改成相反 |
| `[ks-summary-message-payload]` | 留著的紀錄的內容進摘要 | 無 | 無 | — | 不變 |
| `[ks-hash-missing-message]` | 沒有雜湊函式的訊息型別有紀錄時，摘要報錯 | 無 | 無 | — | 不變 |
| J-32 的寫死值（`WorldTests.cpp:519`，`3:3f782d82…`） | 摘要格式固定 | 無（只換值） | 無 | — | 換成版本 4 的值 |
| 新增：同樣的未讀紀錄、不同的歷史 | 紀錄的序號不進摘要 | — | 收紀錄序號的實作會失敗 | — | 新增（Kernel，`MessageCursorTests.cpp`） |
| 新增：例二的回滾案 | R-2 在步裡有訊息時，摘要也相等 | — | 摘要收歷史計數的實作會失敗 | — | 新增（`tests/common/rollback/StrategyTests.cpp`） |

突變：
- 退掉 `msg-summary-no-cursors`、`msg-summary-no-next-seq`、`msg-summary-no-cursor-position`（守的性質拿掉了，`tests/common/mutation/README.md`〈加える基準〉第 7 項）。
- `msg-summary-version-not-bumped` 改指版本 4。
- 新增「摘要又收了歷史的計數」，三條獨立的路，各一個突變：`nextSeq` → 改過的 `[msg-summary-seq]`；紀錄的序號 → 新的「同樣的未讀紀錄」案（讀者排在寫者前，一份跑兩次、一份跑一次，都留一筆同內容的紀錄）；游標表 → 改過的 `[msg-summary-cursors]`。只收回「讀到的位置」的改法一定也被 `[msg-summary-cursors]` 抓到，不算獨立的一條；改過的 `[msg-summary-cursor-position]` 一起抓，用「合わせ方」併進游標表那一個。回滾案和 Kernel 的案抓的是同樣的路，所以回滾那邊不另加突變（`tests/common/mutation/README.md`〈加える基準〉第 1 項）。

## 10. Architecture Delta（AGENTS §3 的七項）

1. **需要它的 Feature 與 Contract**：
   - PvP 回滾：R-2 第 4 條（使用者 2026-11-22 隨回滾 Module 核准）允許步裡寫訊息，等價測試卻因摘要而失敗。
   - 網路 Module（#275）：`NetLinkFailed` 依出錯的時刻發。
   - 時間 Module：`TimeDiscarded`。
   - SDL 輸入事件要不要用 `Message` 還沒定，不當根據。
2. **現在的問題**：摘要收了歷史的計數，同樣的狀態會因時序得到不同摘要；R-2 允許的寫法讓等價測試失敗（第 1 節）。
3. **改的邊界**：`World::Summary` 的內容（Kernel 公開介面的語意與 Format 版本，不改簽名）。
4. **影響的 Module／Product／Tool**：
   - Kernel。
   - 拿摘要比對的測試：回滾、讀檔、生成世界、world_time、world_asset、entity_ids、scenario。只有 `WorldTests.cpp:519` 寫死摘要值。
   - world_asset 的一句註解。
   - gyo2 上沒有 Product 與 Tool。
5. **Dependency Direction**：不變。
6. **Ownership**：不變（摘要仍歸 Kernel）。
7. **更小的改法為什麼不行**：
   - (g) 只解例二。
   - (a)、(c) 是在 Kernel 外面補。
   - (b) 已是最小的撤回。

## 11. 協調

- **網路卡**（`gyo2-net-module`，#275）：選 (b) 時不必改程式。選 (e) 時，`PeerConnected` 等的雜湊函式要同批拿掉。選 (c) 時要改成 Resource。決定之後經主會話轉告。
- **SDL 宣告卡**（`gyo2-sdl-declaration`）的子批 KR（Kernel 加「關係」）：會改 Kernel 的元件與刪除，不碰摘要裡的訊息那一段。改到同一個檔（`World.cpp`）時，後合併的一方 merge 跟上 gyo2。輸入事件要不要用 `Message`，等本題決定後經主會話轉告。
- 本卡第一件（負向編譯樣本的判法，PR #286）和本題無關。

## 12. 範圍的限界與停止條件

- 不改游標、保留、修剪、補讀的行為；不改權威 Module 的 digest；不改回滾的 R-2 定義。
- 「游標的鍵跟登記順序走、和 D-9／J-44 相衝」只是讀程式推的，沒有跑。
- `Summary` 在 Interrupted 也能呼叫；Run 回 Err 之後不修剪（`Executor.cpp:463-467`），那時緩衝含已讀的紀錄，(b) 會把它們算進去。這是現況的行為，本題不改。
- 紀錄還留著時，內容本身帶的累計數（例：`NetLinkFailed::failures`）會經內容進摘要；那是內容，不是本題的歷史計數。選 (b) 時游標表離開摘要，這個衝突在摘要裡消失；游標本身的鍵不在本題範圍。
- 實驗只在本機 macOS（Intel）跑；Windows、Linux 由 L1 看。
- 停止條件照 AGENTS §14 開頭；動到第 8 節以外的 Design Invariants 時，先停下回報。

## 13. 檢查與核准

- 修訂 0 的換模型敵對式檢查：Fable／high，blocker 2、should 5、nit 4（`declaration-check-r0-fable.md`）。處理見頂端「經過」與下表。
- 修訂 1 的窄複查：Fable／high，blocker 0、should 4、nit 6（`declaration-check-r1-narrow-fable.md`）。處理見下面第二張表。
- 使用者的決定（2026-11-23，本卡）：原話「選 (b)」。第 6 節第 1 項選 (b)，連帶接受第 6 節第 1 項 (b) 那一行（回滾後若有讀者還拉著重跑寫的紀錄，摘要和直跑不同）。第 2～4 項是告知。實作等「開始」。

| 項 | 處理 |
|---|---|
| B1 例一用 `PeerConnected` 不成立 | 換成 `NetLinkFailed`；寫明連線本身生成的實體不在本題 |
| B2 系譜不完整，(b)＝版本 1 | 第 2.3 節改寫；建議改成 (b) |
| S1 (e) 的代價寫輕了 | 第 4 節例 A～C；這是改建議的主要理由 |
| S2 退 (d) 的理由分不出 (d) 和 (e) | 第 3.3 節改寫；(d) 的實在代價（游標的鍵跟登記順序走）寫進表 |
| S3 漏掉的選項 | 加 (f)、(g) |
| S4 第 9 節第 4 欄、漏列的檔 | 改寫成「無，決定拿掉」；第 8.1 節補 `Storage.hpp`、`AssetView.hpp`、守衛列、`rollback.zh-Hant.md`。選 (e) 時雜湊函式要同批拿掉，寫在第 4 節與第 11 節 |
| S5 AGENTS §2／§3／§4 的項目 | 第 2.4 節（基盤的理由）、第 4 節的系譜欄、第 10 節第 1 項（拿掉 SDL）、第 8.2 節的 Replacement Criteria、第 6 節第 4 項 |
| N1 呼叫者漏 `EntityIdsTests.cpp` | 第 2.2 節補上 |
| N2 查證報告的行號與路徑 | 報告是證據檔，不改；本頁引用處沒有用到錯的行號 |
| N3 「緊的面」的寫法 | 第 9 節改成每列寫清楚 |
| N4 新突變有幾條路 | 第 9 節寫成四條，各配一案 |

修訂 1 的窄複查：

| 項 | 處理 |
|---|---|
| S1 (b) 的代價少寫「讀者跑過沒有」 | 第 4 節例 D；第 5 節代價；Replacement Criteria 寫「有沒有、讀到哪裡」 |
| S2 例二「解決」有條件 | 第 4 節表與例 E；第 5 節；第 6 節第 1 項寫明連帶接受的 |
| S3 `[msg-summary-cursors]` 的緩的面寫太寬 | 第 9 節加條件「留著的紀錄相同」 |
| S4 新 RC 的第二個觸發在 (b) 下不會發生 | 改成觀察得到的訊號（兩次以上），拿掉原句並寫明理由 |
| N1 DI-KS 10 草稿「歷史的計數」對游標不準 | 改寫：序號是歷史的計數，游標是狀態但明知不收 |
| N2 `Storage.hpp` 的行號 | 改指 `:265-266` |
| N3 不是四條獨立的路 | 改成三條；讀到的位置併進游標表那一個 |
| N4 §14.5 的編號 | 寫成 `tests/common/mutation/README.md`〈加える基準〉 |
| N5 Interrupted 時不修剪 | 第 12 節記下 |
| N6 `NetLinkFailed::failures` 是累計數 | 第 12 節記下 |

# 時間、固定步、runner、跨時鐘事件：設計提案 v2

更新：2026-11-14。Owner：架構（GYO2 重建會話）。**狀態：提案，待使用者決定第 2 節的七件事。還沒有實作。** v1 沒有通過別模型的檢查（blocker 7），已作廢；v1 與檢查報告在主 checkout `build/evidence/gyo2-mechanism/`（`time-design-v1.md`、`time-design-checker-opus.md`）。

這一份的寫法和 v1 不同的地方：
- 每件事分三種標明：**你定過的**（附原話的出處）、**規格或我寫的、沒有單獨問過你的**、**沒判過的**。沒判過的寫成選項，不寫成「以前定過」。
- runner 的順序依四種組合分開寫，不用 Match 的順序代表全部。
- 會改到的既有條文逐條列在第 8 節。

寫之前收集的資料（兩份偵察，Opus／high，唯讀）：主 checkout `build/evidence/gyo2-time-v2/scout-a-judgments.md`（既有判定的原文與出處）、`scout-b-code-facts.md`（程式的事實）。下文的出處多半引自這兩份。

---

## 1. 這份設計要解決什麼

機制的核心已經在 `gyo2` 上，但有五件事只做了一半，而且都卡在同一個地方——**沒有人負責「現在是什麼時間、這一輪要跑哪些 schedule 幾次」**：

| 只做一半的 | 現況（`scout-b` 第一節） |
|---|---|
| runner | plugin 可以登記 runner 的名字，機制不呼叫它；`Product` 只有 `Run(schedule)`，迴圈要外面自己寫 |
| 時間 | World 裡沒有任何時間的 Resource；`engine/time` 只有時鐘與等待器；固定步的累加器沒有帶過來 |
| 交換點 | Resource 可以標 `external`，但「外部資料只在同步點進入」沒有專門的機制 |
| 跨時鐘的事件 | 啟動時直接擋掉（F-E1）；你已決定放開 |
| Extract | 能宣告、啟動時會檢查；參數一被取用就斷言 |

**需要它的 Feature 與 Product**（AGENTS §2 Derivation Order）：

| Feature | Product | 需要的東西 |
|---|---|---|
| 固定頻率的模擬，不被畫面拖慢 | ① PvP 的 Match（沒畫面）、Client 的模擬執行緒 | 固定步的累加、對準 tick 格點的等待 |
| 模擬固定步、畫面不鎖幀率、兩者同一條執行緒 | ② 單機 | 一輪跑 0 到多個固定步再跑一幀；內插用的「離上一步多遠」 |
| 暫停、慢動作，UI 照常動 | ② 單機；③ 工具的預覽 | 可暫停可縮放的時間，和不受影響的真實時間分開 |
| 有輸入才更新 | ③ 工具 | runner 等 OS 事件，不是等 tick |
| 固定步發生的事讓畫面那一側知道（落地聲、命中特效）；UI 的指令送進模擬 | ①②③ | 跨時鐘的事件 |
| 測試與重放不靠真實時鐘 | ①②③ | 可注入的時鐘 |
| 聲音的起音跟著事件發生的時刻，不落在幀格點 | ①②（你 2026-10-30 的決定） | 事件帶 tick；tick 能換成時間點 |

**為什麼當基盤處理**（AGENTS §2）：時間的 Resource 與 runner 的契約一旦定了，每個 System（讀哪個時間）、每個產品的入口（怎麼跑）、每個接外部資料的 plugin（交換點）都會照它寫。之後要改，上面積多少就要改多少。

---

## 2. 需要你決定的七件事（先看這裡）

每件的細節在後面的章節。「建議」是我的判斷，附理由；選別的也做得出來。

| # | 要定什麼 | 選項 | 我的建議 | 細節 |
|---|---|---|---|---|
| 決定 1 | **固定步由誰累加、由誰驅動**（概念稿的 U7，沒判過） | A＝runner；B＝World 裡一個獨占的 System（Bevy）；C＝狀態在 World、驅動在 runner | **A** | 第 4 節 |
| 決定 2 | **一個 Product 有幾個 World**（U8，沒判過） | 甲＝一個 Product 可以有多個 World；乙＝一個 Product 一個 World，一個遊戲由幾個 Product 組成 | **甲** | 第 5.1 節 |
| 決定 3 | **追不上時怎麼辦**（E9b，沒判過） | 甲＝只在「可縮放的時間」那一層截斷一次，並記下丟了多少；乙＝累加器丟整步（舊做法）；丙＝不截斷 | **甲** | 第 3.4 節 |
| 決定 4 | **跨時鐘事件怎麼讀**（方向你定了：放開、照 Bevy；機制沒定） | 甲＝以「讀的那張 schedule 跑完」為界，不加每個讀者的游標；乙＝每個讀者一個游標（Bevy 原樣） | **甲**，並加兩條限制（有上限、可回滾的 schedule 不開放） | 第 6 節 |
| 決定 5 | **暫停與倍率怎麼下達** | 甲＝System 寫一個請求的 Resource，runner 下一輪套用；乙＝System 直接改時間的 Resource | **甲** | 第 3.3 節 |
| 決定 6 | **三個名字** | `GameTime` 改叫 `VirtualTime`；`ScheduleOptions.runner` 改叫 `threadRole`；runner 的程式放 Engine 的 Module `runner` | 照表 | 第 9 節 |
| 決定 7 | **同意第 8 節列的既有條文變更** | — | — | 第 8 節 |

決定 1 和決定 2 是地基：其他幾件的寫法都跟著它們。

---

## 3. 時間

### 3.1 一個來源，三個投影

**你定過的**（HANDOFF `v8-arch-gyo2-unified-time`）：時間的來源必須統一，runner 與 System 用同一個時間標準；參考 Bevy 重構——一個來源、導出幾個時鐘，暫停與慢動作改的是遊戲時間那一層，UI 讀真實時間。Engine 的時間基準是 `GYO::Time`（D29，2026-10-08）。

設計：

```
Time::MonotonicClock（唯一的來源；D29）
        │  runner 每次醒來取樣一次
        ▼
   RealTime        真實時間。不受暫停與倍率影響。UI、工具、診斷讀它。
        │  × 倍率；暫停時為 0；截斷（決定 3）
        ▼
   VirtualTime     可暫停、可縮放的時間。玩法的每幀邏輯讀它。
        │  餵給累加器
        ▼
   FixedTime       固定步的步長，與「離上一個固定步過了多久」（內插用）。
```

- 三個都是 **World 裡的 Resource**，只有 runner 寫，在交換點寫（第 5.3 節）。System 只讀。
- 它們是**投影**：權威的狀態（取樣、倍率、累加的餘數）在 runner；World 裡是這一輪的結果。System 讀哪一個，就決定它跟不跟著暫停——作者不必用狀態旗標控制誰停誰不停。
- **System 不自己讀時鐘**。不是 System 的 Engine 程式照舊讀（等待器醒來後重讀、音效後端每塊取樣、IO 的到達時刻）。
- 全部用整數奈秒（`Time::Duration`）。倍率是浮點數，只在算「這一輪前進多少」時乘一次，乘完取整數。

每個 Resource 的欄位：

| Resource | 欄位 | 說明 |
|---|---|---|
| `RealTime` | `now`（時間點）、`delta`、`elapsed` | 第一輪的 `delta` 是 0 |
| `VirtualTime` | `delta`、`elapsed`、`scale`（這一輪實際用的倍率）、`paused`、`discarded`（累計丟掉的量） | 暫停時 `delta` 是 0；拿它當除數的程式要自己處理 |
| `FixedTime` | `step`、`overstep` | **不放 tick**。現在第幾個 tick 只有一個來源：World 的 domain tick，經 `CurrentTick` 讀 |

**`FixedTime` 不放 tick 的理由**（規格 G.7 取捨 3、H-1；規格寫的，我認為理由獨立成立）：tick 要和事件緩衝一起被還原，放進 Resource 就有兩個來源。累加器自己也不記 tick（舊的 `FixedTickRuntime` 有 `tickId_`，新的不帶）。

### 3.2 決定性區的時間不變

**你定過的**（K.3 第 11 項，2026-11-11）：(c) 區的時間只來自 World 的 domain tick 與被記錄的 boundary 輸入；System 自己讀時鐘禁止。

這份設計不改它：
- 三個時間 Resource 都標 `external`、(b) 區，(c) 區的 System 讀不到（既有的 Z1 檢查）。
- PvP 的 `HostClockSample` 是產品那一層的 boundary 輸入，由 PvP 的 plugin 在交換點寫，用的是 runner **同一次取樣**（第 5.3 節）。機制保證的只有「一次醒來只有一個取樣值，交換點的函式都拿到它」。
- (c) 區的步長照規格 G-6 是產品的 boundary（`FixedStep{stepNs}`，整數）。`FixedTime.step` 是給畫面那一側用的副本。
- 沒有呼叫 `RequireExplicitZones` 的產品（單機、工具）沒有 (c) 區，不受這一節限制。

### 3.3 暫停與倍率（決定 5）

**你定過的**：暫停與慢動作改的是遊戲時間那一層，UI 讀真實時間。**沒判過的**：誰來改、何時生效；選單、遊戲中、暫停這類「狀態的轉換」（概念稿 U15）。

建議（甲）：
- 產品登記一個請求的 Resource `TimeControl{paused, scale}`。System 寫它（例如暫停選單的 System）。
- runner 在下一輪的交換點讀它，套到 `VirtualTime`。**下一輪生效**，和 Bevy 相同（`virt.rs:28-40`）。
- `VirtualTime` 因此只有一個寫者（runner）。

為什麼不選乙（System 直接改 `VirtualTime`）：runner 每輪也要寫它，就變成兩種寫者，其中 runner 那一邊「獨占寫入」的檢查看不到（圖的評審 5.4 對 v1 的批評）。

**可不可以暫停是組裝時的結構，不是執行時的旗標**：
- 產品沒有登記 `TimeControl`，就沒有 System 能要求暫停——PvP 的 Match 與模擬 World 不登記它，「本機的暫停不能影響對局的固定步」由啟動檢查保證（用到沒登記的型別是啟動錯誤）。
- PvP Client 的對時（微調固定步的快慢）不走 `TimeControl`：那是 runner 那一層的時鐘協調，由 PvP 的 plugin 提供自己的「前進量」算法（第 4.3 節），沿用現在的相位修正（每輪最多套用經過時間的 25%）。

暫停時的行為（v1 沒寫清楚的）：
- `VirtualTime.delta` 是 0 → 累加器不前進 → 固定步的 schedule 不跑。每幀的 schedule 照跑。
- 取消暫停的 System 必須在每幀那一側（固定步不跑時讀不到請求）。
- 只有固定步、沒有每幀的產品（沒畫面）暫停時，runner 改用組裝時給的閒置間隔等待。

狀態的轉換（U15）這份設計**不處理**：它和時間分層可以並存，沒有它也能做暫停。

### 3.4 追不上時怎麼辦（決定 3）

**沒判過的**（概念稿 E9b「時間被截斷時不可以讓時間消失」）。舊做法：累加器最多追 5 步，多的整步丟掉。Bevy：在可縮放的那一層設上限（`max_delta` 250 ms），多的丟掉。blueprint 把「max_delta 截斷讓時間消失」列為不照抄的反例（架構師 2026-11-10 寫的候選，沒有單獨問過你）。

| 選項 | 做法 | 代價 |
|---|---|---|
| 甲（建議） | 只在 `VirtualTime` 那一層截斷：一輪最多前進 `maxDelta`；超過的量加進 `VirtualTime.discarded` 並記一筆診斷 | 卡住之後遊戲裡的時間比真實時間少；少多少查得到 |
| 乙 | `VirtualTime` 不截斷，累加器丟整步（舊做法） | `VirtualTime` 與固定步永遠錯開。例：卡 2 秒，`VirtualTime` 前進 2 秒，固定步只前進 83 ms |
| 丙 | 都不截斷 | 追不上時越追越慢（death spiral） |

建議甲的理由：
- 截斷只有一處，`VirtualTime` 與固定步前進的量永遠一致（Bevy 的保證：`fixed.rs:52-54`）。
- 「時間消失」的問題在於**靜默**。甲把丟掉的量記下來，`RealTime` 不受影響，所以「真實過了多久」與「遊戲裡過了多久」都查得到。這符合「不靜默」的原則，也是我對 E9b 的讀法——**這個讀法要你確認**。
- 一輪最多跑幾個固定步＝`maxDelta ÷ step`，由組裝時給。PvP 現在的「最多追 5 步」寫成 `maxDelta = 5 × step`，行為相同。

---

## 4. 固定步由誰驅動（決定 1）

**你定過的**：v0.2 §10.1「如果 Fixed Tick 被 Frame Update 直接驅動，就可能出現不理想的執行時序及延遲」「兩者可以共享部分資料，但不應因此被迫共享同一個執行節奏」「跨時間域的資料同步仍然需要明確設計，不能只靠 Scheduler 自動解決」；`FixedTickRuntime` 可以帶過來但名字要改。

**沒判過的**（概念稿 U7）：累加的狀態與驅動放在哪裡。規格放在 runner（G-6、H.2、H.7），那是規格寫的，沒有單獨問過你。全局審計把它判成「結論沿用舊形」（O-結），所以這裡不能用「以前定過」當理由，要有獨立的理由。

### 4.1 三個選項

| | A：runner 累加、runner 驅動 | B：World 裡一個獨占的 System（Bevy） | C：狀態在 World、驅動在 runner |
|---|---|---|---|
| 做法 | runner 持有累加器；算出這一輪 k 步，呼叫 k 次「跑固定步的 schedule」 | 每幀的 schedule 裡有一個獨占 World 的 System，它累加並重跑固定步的 schedule | 累加器的狀態是 World 的 Resource；runner 讀它、驅動 |
| 系譜 | v0.2 §10.1（你的原稿）；blueprint P3「單一權威驅動點，Time 只是投影」；v7 把模擬搬到自己的執行緒 | Bevy `bevy_time/src/fixed.rs:243-258` | v1 的提議 |
| 沒畫面的產品 | 直接成立 | 仍要一個外層迴圈去跑「每幀」的 schedule（Bevy 的 `ScheduleRunnerPlugin`） | 成立 |
| 固定步在自己的執行緒 | 成立：那條執行緒的迴圈就是它的驅動 | 表達不了：固定步掛在每幀的 schedule 底下 | 成立 |
| 對準 tick 格點的等待 | 等的人就是算餘數的人 | 等的是外層迴圈，餘數在 World 裡，要再讀出來 | 同 B |
| 機制要加什麼 | runner 可以被呼叫（本來就要做） | System 執行中再跑一張 schedule（現在是斷言；`ExclusiveWorld` 現在只能讀寫資料，不能跑 schedule） | 無 |
| 作者要寫什麼 | 無（選一個現成的 runner） | 無 | 無 |
| 弱點 | 暫停、倍率、手動時間要經 runner（第 3.3 節與第 5.4 節已經給了路） | 固定步被每幀驅動，正是 v0.2 §10.1 說不要的；v7 量到的問題（主執行緒卡住時模擬停擺）會回來 | 兩種寫者；回滾、摘要、存檔都要把這個 Resource 排除 |

### 4.2 建議 A，理由（不靠「舊程式就是這樣」）

1. **你的原稿**要的是兩個時鐘各有節奏。B 讓固定步掛在每幀底下，結構上就是「Fixed 被 Frame 驅動」。
2. **等待與累加是同一件事的兩半**。要對準下一個 tick 的格點醒來，就得知道累加器還差多少；決定睡多久的是迴圈的主人。兩者分屬兩邊，每輪就要多一次往返。Bevy 沒有這個需求（它的沒畫面迴圈是「至少隔多久」，不是對準格點：`schedule_runner.rs:97-119`），PvP 的 Match 有。
3. **固定步在自己的執行緒**（PvP 的 Client，或單機想把模擬和畫面拆開）在 A 是同一套零件換個組法；B 做不到。
4. B 唯一多出的好處是「累加的狀態在 World 裡，暫停與倍率很自然」。A 用投影加請求（第 3 節）拿到同樣的作者體驗。

被否決的選項與決勝理由：B——固定步被每幀驅動（v0.2 §10.1）。C——時間的狀態有兩種寫者。

### 4.3 累加器：`TickAccumulator`

**你定過的**：帶過來、改名。名字照命名規範由我訂（`v8-arch-gyo2-contract-review-replies`）。

和舊的 `FixedTickRuntime` 相比：

| | 舊 | 新 |
|---|---|---|
| 時間 | `double` 秒 | 整數奈秒 |
| tick 計數 | 有（`tickId_`） | 沒有；tick 只在 World |
| 追趕上限 | 自己丟整步 | 沒有；上限在 `VirtualTime` 那一層（決定 3） |
| 介面 | `Advance(elapsed, callback)` | `Add(delta)` 回傳這一輪幾步；`Overstep()`；`UntilNextStep()`；`Reset()` |
| 依賴 | `engine/math`（違反 `time.md` DI 10） | 只用標準函式庫，放 `engine/time` |

「這一輪前進多少」由一個可換的算法給累加器：
- 預設：`VirtualTime.delta`（真實的 delta × 倍率，暫停為 0，截斷）。
- PvP 的 Client：自己的相位修正（沿用現在的 `LocalPlayerPrediction` 的做法），由 PvP 的 plugin 在組裝時換上。它是 runner 那一層的時鐘協調，不是 System（規格 H.4.2 的說法，我認為成立：它決定的是「何時跑下一步」）。

---

## 5. runner

### 5.1 一個 Product 有幾個 World（決定 2）

**你定過的**：模擬的 World 與渲染的 World 分開，渲染的有自己的 Registry，只經 Extract 唯讀拿資料（K.3 第 6 項）；產品分 Game 與 Tool，型別叫 `Product`。**沒判過的**（概念稿 U8）：一個遊戲的幾個 World 在不在同一個 `Product` 物件裡。

現況：`Product` 只有一個 World、一個 Registry。但你已定「模擬與渲染分開」，所以任何有畫面的遊戲至少兩個 World。

| | 甲：一個 Product 多個 World | 乙：一個 Product 一個 World |
|---|---|---|
| 一個遊戲是 | 一個 `Product` | 幾個 `Product`，由 `main` 排順序 |
| 和用詞的關係 | 「Product＝Game 或 Tool」字面成立 | 一個 Game 是三個 `Product`，和你定的詞義衝突 |
| runner | 一個 Product 一個（既有規則），它看得到全部的 World | 每個 Product 各一個，跨 Product 的交換沒有人負責 |
| Extract、World 之間的交換 | Product 內部的事 | 要另外定一層 |

建議甲。每個 World 有自己的名字、Registry 與執行緒角色；既有的啟動檢查「同一個 World 的 schedule 綁同一條執行緒」（H-2）不變。只有一個 World 的產品（沒畫面的測試、Match）寫法和現在一樣。

**有幾個 World、各在哪條執行緒，是產品的決定，不是 Engine 的通則**（全局審計指出「Client 三個 World」被寫成通則是 PvP 滲入）。Engine 只提供「可以有多個、彼此只在交換點交換」。

### 5.2 runner 是什麼

**你定過的**：runner 由 plugin 在 finish 時安裝，一個 Product 只能有一個（`v8-arch-framework-retire`，側邊會話轉述）；Host 換成「組裝（setup）與 runner」。

設計：
- runner＝**一個函式，擁有迴圈**（Bevy 的 `set_runner`：`bevy_app/src/app.rs:203-227`）。`Product::Run()`（不帶參數）把控制權交給它，它回傳時產品結束。
- 現有的 `Product::Run(schedule)` 是 runner 用的原語（跑一張 schedule 一次），保留，改名讓兩者不混（建議 `RunSchedule`）。沒有安裝 runner 的產品（測試）照舊自己呼叫它。
- Kernel 不擁有執行緒、不讀時鐘（既有的 DI-KS 15 的精神，不變）。這些都在 runner。

「plugin 安裝」與「Module」的關係（偵察 A 第 14 節第 11 點指出的用詞問題）：`Plugin` 這個**型別**是登記的單位（`Build`、`Finish`）；Module 與 Plugin 是**程式放哪一層**。Engine 的 Module 也可以提供 `Plugin` 型別。所以「runner 由 plugin 安裝」和「runner 的程式在 Engine 的 Module `runner`」不衝突：沒畫面的 runner 是 Engine 的 Module `runner` 提供的一個 `Plugin` 型別；有視窗的 runner 是平台那個 Module（SDL 的 adapter）提供的。

### 5.3 交換點

**規格寫的**（H.1；我認為概念成立，全局審計也判為有獨立理由）：交換點＝runner 在兩次 schedule 執行之間、World 是 Ready 時，把資料搬進或搬出 World 的位置；只有這裡可以上鎖。

設計：
- 一個 World 每一輪有兩個交換點：**進**（跑 schedule 之前）與**出**（跑完之後）。
- plugin 在 finish 時登記交換函式：`在 World X 的「進」或「出」做這件事`。runner 依 plugin 清單的順序呼叫。
- 每個交換函式拿到這一輪的資訊（取樣的時間點、第幾輪）。
- 寫時間的三個 Resource 是第一個交換函式，由 Module `runner` 自己登記。

誰會用（名指的）：時間的投影（Engine）；`HostClockSample`（PvP）；網路的收發緩衝；平台事件；資產 IO 的完成通知；World 之間的信箱；診斷的輸出。

既有的執行期規則已經配合：World 在 Ready 且跑過之後，Host 只能寫 `external` 的 Resource 或 (c) 區的 Input（`World.cpp:196-201`）。

### 5.4 可注入的時鐘

runner 不直接呼叫 `Time::Now()` 與等待器，經兩個可換的零件：
- **時鐘來源**：預設是真實時鐘；測試與重放換成腳本時鐘。
- **等待**：預設是 `Time::Waiter`（絕對期限）；測試換成「不等，直接把腳本時鐘推到期限」。

這補上 v7 就有的需求（注入時鐘的 L1 測試：`04-client-roles.md:53`、`:116`）與規格的重放 host（H.4.4）。Bevy 的對應是 `TimeUpdateStrategy`。

### 5.5 四種組合的順序

零件相同，組法不同。「取樣」一輪只有一次。

**(一) 沒畫面、只有固定步**（Match、專用伺服器、多數測試）。一條執行緒：

```
每次醒來：
  1. 取樣
  2. 交換點「進」（寫時間、收外部資料）
  3. Loading 還沒好：跑 Loading 一批。好了：累加，跑 k 次固定步
  4. 交換點「出」
  5. 等到「取樣時刻 ＋ 離下一步還差的時間」（絕對期限；暫停時用閒置間隔）
```

期限換算：離下一步還差的是遊戲時間，除以倍率才是真實時間；倍率為 0 或暫停時沒有期限，改用閒置間隔。

**(二) 有視窗、固定步與每幀同一個 World、同一條執行緒**（單機的預設）：

```
每一幀：
  1. 處理 OS 事件
  2. 取樣
  3. 交換點「進」
  4. 累加，跑 k 次固定步（0 到多次）
  5. 跑每幀的 schedule（內插用 FixedTime.overstep）
  6. 交換點「出」
  7. Extract 到渲染的 World，跑渲染的 schedule，送出畫面
  8. 等待由畫面決定（垂直同步或幀率上限），不等 tick
```

幀率不被 tick 頻率鎖住。固定步在這裡是由 runner 在同一條執行緒上跑的，不是由每幀的 schedule 驅動——兩張 schedule 仍然各有自己的時鐘，只是共用一條執行緒。主執行緒卡住時模擬也會停，這是這個組合的代價；不能接受的產品用（三）。

**(三) 固定步在自己的執行緒**（PvP 的 Client；想把模擬與畫面拆開的單機）：

```
模擬的執行緒：同（一），驅動模擬的 World
主執行緒：同（二）但沒有第 4 步，驅動畫面的 World 與渲染的 World
兩個 World 之間：只在各自的交換點，經信箱交換（第 6.3 節）
```

兩條執行緒各自取樣（v7 第 04 批的設計）。這和「一輪只取樣一次」不衝突：一輪指的是一條執行緒的一次醒來。

**(四) 有輸入才更新**（工具）：

```
  1. 等 OS 事件（可以加一個逾時）
  2～7 同（二），通常沒有固定步
```

OS 的 modal loop 期間要繼續畫（live frame）是有視窗的 runner 自己的事，不在這份設計。

**現在做得出來的**：（一）和它的測試；（二）（三）的迴圈邏輯可以用腳本時鐘與假的「畫面等待」測。真正有視窗的 runner 要等平台的 Module（SDL）帶回來，那屬於移植，要你同意才動。

---

## 6. 跨時鐘的事件（決定 4）

**你定過的**（`v8-arch-gyo2-cross-clock-events-open`）：「放開：照 Bevy，讓事件留到另一側讀過為止。我認為這個機制，還需要考慮一下如何設計。不光要考慮單機，也要兼容網絡部分。」事件只在所屬時鐘的 schedule 裡讀寫，是 K.3 第 15 項；其中「同一時鐘下的另一張 schedule」那一半是你選的，這份設計不動它。

現況：事件登記時屬於一個時鐘（`Fixed`、`Frame` 或 `Loading`）。讀者與寫者都必須在那個時鐘的 schedule 裡，否則啟動錯誤。讀法沒有游標：依讀者在執行計畫裡的位置，讀「本 tick 排在它前面的寫者」與「上個 tick 排在它後面的寫者」的紀錄；緩衝只留兩個 tick。

### 6.1 同一個 World 裡（單機）

放開之後有兩個方向：
- 固定步寫、每幀讀（落地聲、命中特效）。一幀可能跑 0 個、1 個或多個 tick。
- 每幀寫、固定步讀（UI 的指令）。這一幀可能一個 tick 都沒跑。

**做法（建議甲）**：
- 讀者與事件在**同一個時鐘**：照現在的讀法，完全不變。
- 讀者在**另一個時鐘**的 schedule：讀到「自從它那張 schedule 上一次跑完之後寫進來的全部紀錄」，依（tick、寫者位置、序號）排序。
- 每個（事件、讀的那個時鐘）記一個水位：那張 schedule 每次跑完，把水位推到當時的最新序號。
- 紀錄留到兩個條件都滿足才丟：原本的保留期過了，**而且**每個會讀它的另一側都跑完過一次。

結果：
- 一幀跑多個 tick：每幀那一側一次讀到全部，不漏。
- 一幀沒跑 tick：固定步那一側等到它下次跑才讀到，不漏。
- 每筆紀錄對每個**有執行的**讀者恰好一次。

和 Bevy 的差別，以及另一個選項：

| | 甲（建議）：以 schedule 跑完為界 | 乙：每個讀者一個游標（Bevy 原樣） |
|---|---|---|
| 記在哪 | World，每個（事件、時鐘）一個數字 | World，每個（讀者、事件）一個數字 |
| 讀者被 run 條件跳過那一次 | 錯過（和現在同一個時鐘的規則一致） | 不會錯過，下次補讀 |
| 和既有規則的關係 | 同時鐘的讀法不變；只多一條跨時鐘的 | 推翻規格「事件無游標」的取捨；同時鐘與跨時鐘兩套讀法並存 |
| 保留期 | 有明確的界線（另一側跑完） | Bevy 靠「輪替兩次」；讀者長期不讀就漏，只能回報漏了幾筆 |

建議甲的理由：它做到你要的「留到另一側讀過為止」，而且不引入第二種讀法。「被 run 條件跳過就錯過」是現在就有的規則，甲只是讓它在跨時鐘時也一樣。你說的是「照 Bevy」，所以乙列出來給你選：如果你要的是「帶條件的讀者也不能漏」，那是乙。

### 6.2 兩條限制（甲、乙都需要）

**(1) 有上限。** 另一側一直不跑時（暫停時每幀一直寫、固定步不讀；Loading 期間），紀錄會一直累積。每種事件在登記時給「最多留幾筆等另一側讀」（有預設值）。超過時丟最舊的並記一筆診斷，不靜默。Bevy 沒有這個上限（`messages.rs:79`：「will grow indefinitely」）。

**(2) 可回滾的 schedule 所屬的時鐘，它的事件不開放跨時鐘讀。** 啟動時檢查。理由：回滾會把那個時鐘的事件緩衝還原再重跑，另一側已經讀過的會重新發出，被推翻的事件另一側也收不回來。這些是產品的語意（預測的命中被推翻時特效怎麼辦），機制替不了產品決定。會碰到這條的只有「同一個 World 既回滾又要跨時鐘」的產品；PvP 的 Client 模擬與畫面本來就在不同的 World，走第 6.3 節。

另外：只有帶時鐘的 schedule 可以跨時鐘讀；Startup 仍然不能讀寫事件。

### 6.3 兩個 World 之間（網路；固定步在自己執行緒的單機）

兩個 World 之間不共用事件緩衝（一個 World 一條執行緒）。做法是**橋**：

```
模擬的 World                              畫面的 World
  事件 E（Fixed）                           事件 E（Frame）
     │ 交換點「出」                            ▲ 交換點「進」
     ▼                                        │
     └──────── 有上限的佇列（帶來源的 tick）──────┘
```

- 產品在組裝時宣告「事件 E 從 World S 送到 World F」。
- S 的交換點「出」把這一輪新寫的紀錄放進佇列，每筆帶它的 tick。
- F 的交換點「進」把佇列裡的寫進 F 自己的事件緩衝。F 那邊的 System 用一般的 `EventReader` 讀，拿得到來源的 tick。
- 佇列有上限；滿了丟最舊的並計數。
- 回滾重跑時重新發出的紀錄**預設不送**（只送第一次模擬時發出的）；被推翻怎麼處理是產品的事。這一點等回滾的機制帶上來時一起實作，現在 `gyo2` 上沒有回滾。

對作者來說，單機（同一個 World）與網路（兩個 World）寫 System 的方式一樣：寫的一邊 `EventWriter`，讀的一邊 `EventReader`。差別只在組裝時有沒有宣告橋。這是我對「單機與網路都要兼容」的回應。

**PvP 現在的呈現資料不走這條路**，也不必改：模擬到畫面是「最新一份」的信箱（v7 的設計），遠端的射擊與命中是畫面那一側從快照偵測的。橋是多一條路，給真的要逐筆送的事件用。

---

## 7. 聲音的時刻

**你定過的**（2026-10-30；HANDOFF 寫明「音效，不要丟」）：本機射擊＝按下事件的時間戳；遠端射擊、命中、受擊＝把事件的 tick 換成那個 tick 被顯示的時間（PvP 用產品自己的時間軸 `SnapshotTimeline::DisplayTime`）。混音器要的是 Engine 的真實時間點（`PlayRequest::at`）。

這份設計**不改 PvP 的做法**：PvP 的 tick 是權威的 tick，不是本機固定步的 tick，只有產品的時間軸換得對。v1 想用 `FixedTime` 換算，是錯的。

機制保證的只有三樣資料存在，夠產品自己換算：
- 事件的紀錄帶 tick，讀者拿得到。
- `RealTime.now`：這一輪的取樣時間點。
- `FixedTime.step` 與 `overstep`。

單機的換算（給之後接音效時參考，這份設計不實作）：最新一個 tick 的名義時刻＝`RealTime.now − overstep ÷ 倍率`；往前第 n 個 tick 再減 `n × step ÷ 倍率`。只在同一輪、倍率不為 0 時成立；暫停時沒有定義，改用 `RealTime.now`。追趕時較早的 tick 可能比混音器的排程延遲（60 ms）還早，會被當成遲到——這是已知的限界。

---

## 8. 會改到的既有條文（決定 7）

`gyo2` 上沒有 master 的 target 條文（R0 決定不帶，等重寫），但程式是照它們做的。下面列的是「程式現在的行為」與「它原本依據的條文」。

| 條文 | 現在 | 改成 | 誰定的 |
|---|---|---|---|
| 規格 F.3 的 domain 規則、F.8 取捨 4、F-9 的跨 domain 那一半；master DI-KS 10「不跨 clock domain」、DI-KS 14「每個 domain 的事件只在它的 clock schedule 內」 | 跨時鐘讀寫＝啟動錯誤 F-E1 | 帶時鐘的 schedule 可以讀別的時鐘的事件（第 6 節）；可回滾的時鐘除外 | 禁止是規格寫的；放開是你 2026-11-14 定的 |
| 規格 F.3「跨 domain 的傳遞是 Host 交換點的職責」 | — | 同一個 World 內由 Kernel 做；兩個 World 之間仍在交換點（橋） | 規格寫的 |
| 規格 F.3「緩衝只存兩個 tick」 | 是 | 有跨時鐘讀者的事件，多留到另一側跑完，有上限 | 規格寫的 |
| 規格 F.8 取捨 3「事件無游標」 | 是 | 選甲：不變。選乙：跨時鐘的讀者有游標 | 規格寫的 |
| K.3 第 15 項「同一時鐘下的另一張 schedule 用元件或 Resource」 | 是 | **不變** | 你選的 |
| 規格 G-6、H.2、H.7「固定步累加在 Host／runner」 | 沒實作 | 選 A：照做，但理由換成第 4.2 節。選 B：推翻 | 規格寫的 |
| 規格 G-6「累加器的 tick 計數是簿記」 | — | 累加器不記 tick | 規格寫的 |
| `FixedTickRuntime` 的型別與語義（`double`、自己丟整步） | 沒帶 | 改名 `TickAccumulator`、整數、不丟步 | 改名是你定的；其餘是這份提案 |
| blueprint §4「max_delta 截斷讓時間消失」是反例；概念稿 E9b | 未判 | 選甲：截斷但記下丟掉的量 | 架構師寫的候選；未判 |
| `time.md` DI 1「One time base」 | steady_clock | **不變**，並加一句：System 經時間的 Resource 讀，不自己讀時鐘 | 你定的（D29） |
| `time.md` DI 10「`GYO::Time` 只連 Base 與 Threads」 | 是 | **不變**（`TickAccumulator` 不依賴 math） | 你 2026-10-27 照草稿同意的 |
| `time.md` Replacement Criterion 4「排程需要第二個時間基準時要明決」 | — | 沒有碰到：期限仍然是真實時鐘上的時間點，遊戲時間只用來算「還差多少」 | 同上 |
| `Product` 只有一個 World | 是 | 選甲：可以有多個 | 現在的程式 |
| `Product::Run(schedule)` | 產品自己呼叫 | 改名 `RunSchedule`；新增不帶參數的 `Run()` 交給 runner | 現在的程式 |
| `FinishContext::InstallRunner(名字)` | 只記名字 | 連函式一起安裝 | 規則是你定的；做一半是現況 |
| 規格 H.2 的 `FrameTime{frameIndex, deltaSeconds}` | 沒實作 | 由 `RealTime`／`VirtualTime` 取代；第幾幀用 `CurrentTick` | 規格寫的 |
| master runtime-composition「Client 有三個 Runtime」寫成 Engine 的通則 | 不在 gyo2 | 不帶進 Engine；是 PvP 產品的組合 | 三個 World 是你為 PvP 定的；寫成通則是文件的問題 |

**Architecture Delta 的七項**（AGENTS §3）：

1. 需要它的 Feature：第 1 節的表。
2. 現在的問題：沒有人負責時間與迴圈，五件事只做一半（第 1 節）。
3. 變動的邊界：新增 Engine 的 Module `runner`；Kernel 的事件多一條跨時鐘的讀法；`Product` 可以有多個 World、runner 可以被呼叫。
4. 影響：`engine/kernel`、`engine/time`（加 `TickAccumulator`）、新的 `engine/runner`；現有的測試（`Product::Run` 改名）。產品與工具目前都不在 `gyo2` 上。
5. 依賴方向：`runner` → `kernel`、`time`、`threads`。`kernel` 不依賴 `runner`、`time`（Kernel 不讀時鐘）。`time` 照舊只依賴 base。
6. Ownership：時間的狀態（取樣、倍率、累加的餘數）歸 runner，World 裡是投影；「現在第幾個 tick」歸 World，不變；同一個 World 內跨時鐘事件的傳遞從「Host 的職責」變成 Kernel 的職責。
7. 為什麼不能更小：只補 runner 而不定時間，每個產品會各寫一套時間的 Resource；只開放跨時鐘事件而不加保留的界線，一幀多個 tick 時就會漏。

**新 Module `runner` 的 Design Invariants 與 Replacement Criteria**（草案，實作前定稿）：

- DI 1：一條執行緒的一次醒來只取樣一次時鐘；這一輪所有的時間 Resource 與交換函式用同一個值。
- DI 2：時間的 Resource 只有 runner 寫，只在交換點寫。
- DI 3：runner 不排 System 的順序：它只呼叫「跑一張 schedule」與交換函式。
- DI 4：runner 讀時鐘與等待都經可注入的零件；測試不靠真實時間。
- DI 5：`VirtualTime` 與固定步前進的量一致：截斷只發生在 `VirtualTime`。
- Replacement Criteria：(a) 某個產品的迴圈用這四種組合的零件拼不出來，必須改零件本身；(b) 交換函式需要知道別的交換函式的內部狀態才能正確；(c) 時間的 Resource 需要第二個寫者。

---

## 9. 名字（決定 6）

| 東西 | 名字 | 說明 |
|---|---|---|
| 累加器 | `TickAccumulator` | 已訂 |
| 真實時間 | `RealTime` | 已訂 |
| 可暫停可縮放的時間 | **`VirtualTime`**（原訂 `GameTime`） | 「Game」已經定為產品的一種（Game 與 Tool），工具也會用到這個時間；照「一個詞只指一件事」改。`VirtualTime` 是 Bevy 的詞 |
| 固定步的時間 | `FixedTime` | 已訂 |
| 暫停與倍率的請求 | `TimeControl` | 新 |
| Module | `runner`（`engine/runner`，`GYO::Engine::Runner`） | 已訂。時間的 Resource 也放這裡，因為 runner 是它們唯一的寫者；避開 `GYO::Time` |
| `ScheduleOptions.runner` | **`threadRole`** | 它現在的意思是「哪個執行緒角色呼叫 Run」，和「runner＝擁有迴圈的函式」撞名 |
| `Product::Run(schedule)` | `RunSchedule` | 讓出 `Run()` 給 runner |
| 事件 | 維持 `Event` | 不改叫 Message：會撞到 base 的 `LogMessage` 與 wire 的訊息 |
| PvP 的 `HostClockSample` | 不在這份設計改 | 名字裡的「Host」是被換掉的角色詞，但它是你的命名規範第 3 條舉的例子，而且屬於 PvP 那一層；移植 PvP 時再定 |

---

## 10. 不在這份設計裡的

- **狀態的轉換**（選單、遊戲中、暫停；概念稿 U15）：未判，不需要它也能暫停。
- **回滾**（快照、還原、重跑）：`gyo2` 上還沒有。這份設計只留下兩個接點：可回滾的時鐘不開放跨時鐘讀（第 6.2 節）；橋預設不送重跑時發出的紀錄（第 6.3 節）。
- **Loading 的分批細節**：runner 會跑 Loading 的 schedule 直到產品說好了，然後才開始累加固定步（對局的 tick 從 0 開始）；資產的收件與套用是資產那一批的設計。
- **有視窗的 runner 的實作**：要平台的 Module（SDL）。移植要你同意。
- **並行執行**：第一版只有 Serial，不變。
- **PvP 的對時算法本身**：沿用現在的，移植時處理。
- **帶過來的 `time`、`threads`、`audio` 的重審**（R0 清單要求的「哪些 DI 是 PvP 時序的後果」）：這份設計只確認了 `time.md` 的 DI 1、DI 10 與 Replacement Criterion 4 不受影響，其餘沒有審。

---

## 11. 你決定之後的批次

| 批 | 內容 | 依賴 |
|---|---|---|
| T1 | `TickAccumulator`（`engine/time`）與測試 | 決定 1、3 |
| T2 | Module `runner`：可注入的時鐘與等待、三個時間 Resource、`TimeControl`、交換點的登記、可呼叫的 runner、組合（一）與測試 | 決定 1、3、5、6 |
| T3 | Kernel：跨時鐘事件（水位、保留、上限、啟動檢查） | 決定 4 |
| T4 | `Product` 多個 World；Extract 的執行；組合（二）（三）的迴圈邏輯（用腳本時鐘測） | 決定 2 |
| T5 | World 之間的事件橋 | T3、T4 |
| 之後 | 有視窗的 runner（等 SDL）；回滾 | 移植要你同意 |

每一批開工前各有事前宣告；判定用的測試先列突變。

---

## 12. 範圍的限界

- 這是設計提案，沒有寫程式，沒有量測。第 5.5 節的順序是從現有程式（master 的 Match 與 Client 迴圈）與 Bevy 讀出來再整理的，沒有跑過。
- Bevy 的行為引自偵察 B 的讀碼（Bevy 6a911653f）；「暫停時事件無限累積」是讀碼推論。
- 你的原話一律引自 `HANDOFF.md` 的轉錄，沒有重讀 11 月 11 日側邊會話的原始紀錄檔。
- 「我對 E9b 的讀法」（第 3.4 節）與「我對『單機與網路都要兼容』的回應」（第 6.3 節）是我的解讀，要你確認。
- 你說的「以前的做法是用狀態控制，但是這個做法明顯適合 GYO」那一句，讀法一直沒有確認過；這份設計沒有依賴它的任何一種讀法。

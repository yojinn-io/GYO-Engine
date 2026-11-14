# 時間、固定步與跨時鐘的事件：設計提案（`v8-arch-rebuild`）

更新：2026-11-14。Owner：架構（GYO2 重建會話）。**狀態：提案 v1，待別模型檢查與使用者確認。沒有動任何程式。**

這份把三件連在一起的事一起提：
- 時間與固定步（使用者 2026-11-14，`v8-arch-gyo2-unified-time`）：「首先時間來源必須統一。不管是 runner 也好還是 System 也好，都需要一個統一的時間標準。FixedTickRuntime 帶過來可以但是名字需要修改。」「可以參考 Bevy 重構 GYO::Time」。
- 跨時鐘的事件（`v8-arch-gyo2-cross-clock-events-open`）：「放開：照 Bevy，讓事件留到另一側讀過為止。我認為這個機制，還需要考慮一下如何設計。不光要考慮單機，也要兼容網絡部分。」
- 接 audio 時，播放請求上的時刻怎麼來。

兩個決定的原話、選項、理由、出處在 kernel `HANDOFF.md` 的決定表，這裡不重抄。

## 1. 現況（查過的事實）

| 東西 | 現在的樣子 | 出處 |
|---|---|---|
| 時間的來源 | 一個：`Engine::Time::MonotonicClock`（`steady_clock`）。全域函式 `Now()`，誰都可以叫 | 帶過來的 `engine/time/MonotonicClock.hpp`；`time.md` DI 1；v7 D29（使用者 2026-10-08） |
| 固定步的累加 | `FixedTickRuntime`：純累加器。呼叫者給經過的秒數，它算出要跑幾步、最多追 5 步、回報丟掉的秒數與離下一步還有多久。不讀時鐘、不睡、沒有迴圈 | master `engine/runtime/FixedTickRuntime.hpp`；gyo2 上沒有帶 |
| 誰驅動固定步 | 舊的 Match：自己的迴圈取 `Now()`、算間隔、餵累加器、等到下一步 | master `MatchRuntimeHost.cpp:527-540` |
| Kernel 裡的時間 | 每個 `Domain` 一個 tick 的計數，`CurrentTick` 參數讀它。沒有「時間」的 Resource，沒有暫停或倍率 | 分支 `claude/gyo2-mechanism` |
| 事件 | 屬於一個時鐘；寫者與讀者都要在那個時鐘的 schedule 裡，否則是啟動錯誤。排在寫者後面的讀者當個 tick 讀到，留到下一個 tick 結束 | 同上；規格 F.3 |
| 音效的時刻 | 播放請求帶一個 Engine 時間點（`PlayRequest::at`），混音器把它換成取樣位置 | 帶過來的 `engine/audio/Mixer.hpp:39-44`；`audio.md` |
| 音效的時刻從哪來（PvP，10 月 30 日定的） | 本機射擊＝按下那次輸入的時間戳；遠端射擊、命中、受擊＝把事件的 tick 換成「那個 tick 被顯示的時間」 | master `docs/dev_logs/2026_10_30_pvp_v8_product_sfx.zh-Hant.md` |
| Bevy | 一個來源，導出三個時鐘，都是 Resource：真實時間→可暫停可調倍率的虛擬時間→固定步。暫停的是虛擬時間；UI 讀真實時間 | 側邊會話讀的 `bevy_time/src`（real.rs、virt.rs、fixed.rs、time.rs），引文在決定表 |

## 2. 提案

### 2.1 三個時鐘，都是 World 裡的 Resource

| 時鐘（暫名） | 是什麼 | 誰讀 | 會不會被暫停、調倍率 |
|---|---|---|---|
| `RealTime` | 這一次取樣到的 Engine 時間點，以及離上一次取樣過了多久 | UI、選單的動畫、算音效時刻的 System | 不會 |
| `GameTime` | 真實時間乘上倍率之後的遊戲時間；可以暫停 | 每幀更新的玩法與呈現（鏡頭、粒子） | 會 |
| `FixedTime` | 步長、現在是第幾個 tick、離下一個 tick 還差多少 | 固定步的 System；把 tick 換成時間點的人 | 跟著 `GameTime` |

一條鏈：`MonotonicClock` → `RealTime` → `GameTime` → `FixedTime`。來源只有最前面那一個。

### 2.2 誰做什麼

**runner 每醒來一次**，照這個順序：

1. 讀一次 `Now()`，寫進 `RealTime`。這是整個 Product 這一輪唯一一次讀時鐘。
2. 照倍率與暫停算出 `GameTime` 前進多少。
3. 把這段遊戲時間交給固定步的累加器，得到「這一輪跑幾個 tick」。
4. 呼叫固定步的 schedule 那麼多次；每跑一次，tick 加一。
5. 呼叫每幀的 schedule 一次。
6. 用累加器回報的「離下一個 tick 還有多久」算出要等到什麼時候，交給 `Waiter` 等。

**System 不自己讀時鐘**，只讀上面三個 Resource。

固定步由 runner 驅動，不照 Bevy 做成 World 裡的一個 System。理由是以前定過的：模擬的時鐘和畫面的時鐘要能各自驅動自己的 schedule（v0.2 §10.1；v7 把模擬搬到自己的執行緒），Match 根本沒有每幀的 schedule。

### 2.3 暫停、慢動作、UI

- **暫停**：把 `GameTime` 設成暫停。第 3 步交出去的遊戲時間是零，固定步自然不跑；讀 `GameTime` 的 System 看到「沒有前進」。
- **慢動作**：改 `GameTime` 的倍率。
- **UI**：讀 `RealTime`，不受上面兩者影響。

作者不必用狀態去控制誰停誰不停；每個 System 讀哪一個時鐘，就決定了它停不停。

### 2.4 網路

- **Match**：倍率固定是 1，不暫停。
- **Client**：對時是微調 `GameTime` 的倍率（原本的設計就是「寫 Time 的速度 ±10%，步長不動」）。
- **「遊戲時間可不可以暫停」由產品決定**：產品在組裝時宣告不可暫停，之後任何 System 去暫停都是 Programmer Error。PvP 宣告不可暫停。
- **決定性區的規則不變**（K.3 第 11 項）：決定性區的 System 只讀 tick 與被記錄的輸入。`RealTime` 與 `GameTime` 屬於一般的資料，決定性區讀不到。

### 2.5 追不上的時候

沿用 `FixedTickRuntime` 的做法：一輪最多追固定的步數，超過的整步丟掉，不足一步的零頭留著。丟掉多少要記數並讓產品讀得到，不靜默。Bevy 的做法（虛擬時間一次最多前進一個上限）是同一個用意。

### 2.6 音效的時刻

混音器的介面不改：播放請求仍然帶一個 Engine 時間點。改的是 System 從哪裡拿到這個時間點：

- UI 的聲音：`RealTime` 的取樣時間。
- 本機輸入觸發的聲音：那次輸入的時間戳（10 月 30 日的決定，不變）。
- 模擬裡發生的事觸發的聲音：把事件的 tick 換成時間點。`FixedTime` 提供這個換算。

暫停時固定步不跑，就不會有新的模擬事件，也就不會排出新的聲音。已經排進混音器、還沒播完的聲音不受影響。

### 2.7 跨時鐘的事件

事件仍然屬於一個時鐘。放開的是：另一個時鐘的 schedule 也可以讀它。

**固定步寫、每幀讀**（模擬告訴呈現：落地了、中彈了）：
- World 替每一張「會讀別的時鐘的事件」的 schedule 記一個記號：我上次跑的時候，對方跑到第幾個 tick。
- 每幀的讀者讀到的是「記號之後的所有 tick 的事件」，依 tick 的先後。一幀跑了三個 tick 就讀到三個 tick 的；跑了零個就沒有新的。
- 事件留到每一張會讀它的 schedule 都跑過為止。
- 記號是每張 schedule 一個，放在 World 裡，不是每個讀者自己的游標。這延續規格當時「System 不持有游標」的取捨。

**每幀寫、固定步讀**（UI 告訴模擬：按了開始、選了難度）：
- 同樣的做法，方向相反：固定步的 schedule 記「對方跑到第幾幀」。
- 一幀沒有跑到固定步時，事件留著，等固定步下次跑的時候讀。
- 一幀跑了好幾個 tick 時，第一個 tick 讀到，後面的 tick 沒有新的。

**回滾**（PvP）：
- 固定步寫、每幀讀：重跑時事件會重新發出，但它們的 tick 不大於每幀那一側的記號，所以不會被讀第二次。記號不跟著回滾。
- 每幀寫、固定步讀：這等於是模擬的輸入。重跑第 T 個 tick 時，必須讀到和第一次一樣的東西。PvP 已經有輸入的路（以 tick 為鍵、不回滾的輸入暫存）。**提議第一版**：開了回滾的固定步 schedule 不准讀每幀的事件，是啟動錯誤，訊息指向輸入的那條路。沒開回滾的產品不受限。

**被 run 條件跳過的讀者**會錯過那一次的事件。這和現在同一個時鐘內的規則相同。

### 2.8 程式怎麼放

- **`engine/time`**（Module）：留著 `MonotonicClock`、`Waiter` 與它們的九條 Design Invariants，不動。加進三個時鐘的型別，以及固定步的累加器（從 master 的 `engine/runtime` 帶過來、改名）。都是值與純函式，沒有全域狀態、沒有迴圈。
- **`Now()`**：仍然是唯一的來源。可以叫它的只有 runner，以及 World 之外的東西（IO 的執行緒、追蹤）。登記在 Kernel 上的 System 不叫它；這一條先靠審查，之後用掃描守。
- **預設的 runner**：一個新的 Module（暫名 runner），在 Kernel 與 time 之上。它需要知道哪一張 schedule 是固定步的、哪一張是每幀的；這由產品在組裝時告訴它，Kernel 仍然不內建任何 schedule 的名字。Match 這種沒有每幀 schedule 的產品，用同一個 runner、不給每幀的那一張。
- **Kernel**：加上 2.7 的記號與事件的保留規則；把「事件只能在自己時鐘的 schedule 裡讀」這條啟動錯誤改成 2.7 的規則。

## 3. 名字

| 東西 | 候選 | 建議 |
|---|---|---|
| `FixedTickRuntime` 的新名字 | `TickAccumulator`、`FixedStepAccumulator`、`FixedTicker` | `TickAccumulator`：它做的就是把時間累加成 tick。「Runtime」照同詞異義表不用來命名新東西 |
| 三個時鐘的 Resource | `RealTime`／`GameTime`／`FixedTime`；或照 Bevy `Time<Real>`／`Time<Virtual>`／`Time<Fixed>` | 前者：名詞、各自一個型別，照命名規範 N19。「Virtual」對作者不直覺，改叫 Game |
| 新的 Module | `runner`、`loop` | `runner`：和同詞異義表的用詞一致 |

## 4. 系譜、被退的選項、誰需要

| 提案的部分 | 從哪裡來 | 被退的選項與理由 | 誰需要 |
|---|---|---|---|
| 一個來源、三個時鐘的 Resource | Bevy（使用者指定參考）；來源本身是 v7 D29 | 每個 System 自己叫 `Now()`：暫停與慢動作沒有地方下手，測試也換不掉時間 | ②暫停、慢動作；③UI 與預覽播放；①對時 |
| 固定步由 runner 驅動 | 舊的 Match 迴圈；規格 H.2；v0.2 §10.1 | Bevy 的做法（World 裡一個獨占的 System 每幀順便跑固定步）：模擬就綁在畫面的迴圈上，Match 沒有畫面的迴圈 | ①Match 與 Client；②③用預設的 runner，不必自己寫 |
| 累加器帶過來、改名 | master `FixedTickRuntime`（使用者說可以帶、要改名） | 重寫一個：它是純函式、有測試，沒有理由重寫 | ①② |
| 暫停改的是遊戲時間，UI 讀真實時間 | Bevy | 用狀態控制誰停誰不停（以前的做法；使用者提到它）：每加一個 System 都要記得接上狀態 | ②③ |
| 可不可以暫停由產品宣告 | 新導的 | 不設防：PvP 裡有人寫了暫停，兩端就不一致，而且沒有東西會報 | ① |
| 音效的介面不改 | 帶過來的 audio；10 月 30 日的決定 | 讓混音器改吃 tick：混音器就得認識固定步，Engine 的 Module 反過來依賴玩法的時鐘 | ①②③ |
| 跨時鐘的事件用「每張 schedule 一個記號」 | 新導的；保留期的想法來自 Bevy | Bevy 的做法（每個讀者一個游標，放在 System 自己的狀態裡）：規格當時為了回滾選了不用游標；記號放 World、每張 schedule 一個，重跑時不必處理 | ①②③ |
| 開了回滾的 schedule 不准讀每幀的事件 | 新導的 | 由機制替它依 tick 記下來：等於再做一條輸入的路，PvP 已經有一條 | ①；②③不受限 |

## 5. 會動到的既有約束

- **不變**：`time.md` 的九條 Design Invariants；K.3 第 11 項（決定性區的時間來源）；固定步由 runner 負責（規格 H.7）；混音器的介面與時刻換算。
- **改**：K.3 第 15 項裡「跨時鐘禁止」的那一半（使用者已定方向）。
- **新增**：三個時鐘的 Resource；一個新的 Module（runner）；Kernel 的事件多一個記號與保留規則。新的 Module 與 Kernel 公開行為的改變是 Architecture Delta，這份文件就是它的說明。

## 6. 要你定的

1. **三個時鐘與 runner 的分工**（2.1、2.2）對不對。
2. **可不可以暫停由產品宣告**（2.4）要不要。
3. **跨時鐘事件的做法**（2.7），特別是：開了回滾的 schedule 第一版不准讀每幀的事件。
4. **名字**（第 3 節）：累加器、三個時鐘、新的 Module。
5. **預設的 runner 放在一個新的 Module** 可以嗎。

## 範囲の限界

- Bevy 的時間套件是側邊會話讀的，這個會話只讀了事件輪替的那一段（`bevy_ecs/src/message/update.rs`、`bevy_time/src/lib.rs:103-106`）。
- 2.7 沒有實作過，也沒有測過。「重跑時不會讀第二次」是從記號的定義推出來的。
- 舊的 Client 的對時與相位策略（規格 H.2 提到）沒有重讀；2.4 只寫到「微調倍率」。
- 多個固定步的時鐘（例如物理和玩法不同步長）沒有考慮；目前的 `Domain` 只有一個 Fixed。
- Loading 這個時鐘怎麼放進這條鏈，沒有談。
- 沒有經過別模型檢查。

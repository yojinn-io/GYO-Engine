# 時間、固定步、runner、跨時鐘事件：設計提案 v2（修訂 3）

更新：2026-11-14。Owner：架構（GYO2 重建會話）。**狀態：提案，待使用者決定第 2 節的事。還沒有實作。**

經過：
- v1 沒有通過別模型的檢查（blocker 7），作廢。v1 與報告在主 checkout `build/evidence/gyo2-mechanism/`。
- v2 初稿（commit 5785c4d0）也送了檢查（Opus／xhigh）：blocker 5、should 16、nit 12。主要問題是**我把被否決的選項寫歪了**（決定 1、2、4 的選項表）、兩個保證對 PvP 不成立、跨 World 的橋有洞而且沒有使用者。報告在主 checkout `build/evidence/gyo2-time-v2/time-design-v2-checker-opus.md`。
- 修訂 1（46b8246a）照那份報告改：第 4、5.1、6 節的選項改用實際的差異來比，我原先的幾個理由撤回。
- 修訂 1 送了範圍較窄的複查（Opus／high）：上一輪的 blocker 大多解決，另有 blocker 3（決定 1 的表有一列是我修訂時新寫錯的；選項 C 被我默默刪了；你同日定的 Module／Plugin 兩種型別沒有反映）、should 10、nit 8。報告在同一個資料夾的 `time-design-v2r1-recheck-opus.md`。
- 修訂 2 照複查改過，改完沒有再送檢查。
- **決定 1 已定：B**（使用者 2026-11-14，側邊會話，`v8-arch-gyo2-fixed-step-in-world`）。這一份（修訂 3）把跟著決定 1 的部分照 B 改寫：第 3.1、3.3、4.2、5.2、5.5、8、11 節。**這些改寫沒有送檢查。**

寫法：每件事分三種標明——**你定過的**（附出處）、**規格或架構師寫的、沒有單獨問過你的**、**沒判過的**。沒判過的寫成選項。

資料（兩份偵察，Opus／high，唯讀）：主 checkout `build/evidence/gyo2-time-v2/scout-a-judgments.md`（既有判定的原文與出處）、`scout-b-code-facts.md`（程式的事實）。

用詞：這份文件裡「時間基準」指 `Time::MonotonicClock`；「時鐘」指事件與 schedule 所屬的 `Domain`（`Fixed`、`Frame`、`Loading`，舊稱 clock domain）；「tick」只指固定步的步數，其他的寫「計數」。

---

## 1. 這份設計要解決什麼

機制的核心已經在 `gyo2` 上，但有五件事只做了一半，都卡在同一個地方——**沒有人負責「現在是什麼時間、這一輪要跑哪些 schedule 幾次」**：

| 只做一半的 | 現況（`scout-b` 第一節） |
|---|---|
| runner | plugin 可以登記 runner 的名字，機制不呼叫它；`Product` 只有 `Run(schedule)`，迴圈要外面自己寫 |
| 時間 | World 裡沒有任何時間的 Resource；`engine/time` 只有時間基準與等待器；固定步的累加器沒有帶過來 |
| 交換點 | 沒有專門的機制；現在只能在兩次 `Run` 之間由外面直接寫 World |
| 跨時鐘的事件 | 啟動時直接擋掉（F-E1）；你已決定放開 |
| Extract | 能宣告、啟動時會檢查；參數一被取用就斷言 |

**需要它的 Feature 與 Product**（AGENTS §2）：

| Feature | Product | 需要的東西 |
|---|---|---|
| 固定頻率的模擬，不被畫面拖慢 | ① PvP 的 Match（沒畫面）、Client 的模擬執行緒 | 固定步的累加、對準 tick 格點的等待 |
| 模擬固定步、畫面不鎖幀率 | ② 單機 | 一輪跑 0 到多個固定步再跑一幀；內插用的「離上一步多遠」 |
| 暫停、慢動作，UI 照常動 | ② 單機；③ 工具的預覽 | 可暫停可縮放的時間，和真實時間分開 |
| 固定步發生的事讓畫面那一側知道；UI 的指令送進模擬 | ①②③ | 跨時鐘的事件（你 2026-11-14 定的方向） |
| 測試與重放不靠真實的時間基準 | ①②③ | 可注入的時間 |

沒有列進來的：「有輸入才更新的工具」——master 的工具是持續更新加垂直同步（`framework/src/WindowShell.cpp:44-67`），沒有名指這個 Feature 的出處，這份設計不做（第 10 節）。「聲音的時刻」——你 2026-10-30 的決定是 PvP 的做法，見第 7 節。

**為什麼當基盤處理**（AGENTS §2）：時間的 Resource 與 runner 的契約一旦定了，每個 System（讀哪個時間）、每個產品的入口（怎麼跑）、每個接外部資料的 plugin（交換點）都會照它寫。之後要改，上面積多少就要改多少。

---

## 2. 需要你決定的事（先看這裡）

「建議」是我的判斷，附理由與代價；選別的也做得出來。決定 1（已定）與決定 2 是地基。

| # | 要定什麼 | 選項 | 我的建議 | 建議的代價 | 細節 |
|---|---|---|---|---|---|
| 決定 1 | **固定步由誰累加、由誰驅動** | — | **已定：B**（World 裡一個獨占的 System，Bevy 的做法）。我原本建議 A | Kernel 要先支援「System 執行中再跑一張 schedule」 | 第 4 節 |
| 決定 1b | **要不要有一個「通用的時間」**（選 B 帶出來的；側邊會話提過，你還沒回答） | 甲＝做：System 讀一個通用的時間，在固定步裡它自動是固定步的值（Bevy）；乙＝不做：System 自己選讀哪一個 | 甲 | 多一個意思會隨所在位置改變的 Resource | 第 3.1 節 |
| 決定 2 | **多個 World 由誰持有**（U8，沒判過） | 甲＝型別 `Product` 可以有多個 World；乙＝`Product` 維持一個 World，多個 `Product` 由組裝的程式與 runner 管 | 甲 | `Product` 變大；有四個問題實作前要先定（第 5.1 節） | 第 5.1 節 |
| 決定 3 | **追不上時怎麼辦**（E9b，沒判過） | 甲＝超過上限的整步從可縮放的時間扣掉並記帳；乙＝累加器自己丟（舊）；丙＝不設上限；丁＝設上限但不丟，欠著往後追 | 甲 | 卡住之後遊戲裡的時間比真實時間少；倍率大於 1 時快轉會被壓 | 第 3.4 節 |
| 決定 4 | **事件的讀者怎麼記「讀到哪裡」**（方向你定了：放開、照 Bevy；機制沒定） | 丙＝全部的讀者都有游標，可回滾的 schedule 例外；乙＝只有跨時鐘的讀者有游標；甲＝跨時鐘以整張 schedule 為單位記一個水位 | 丙 | 同一個時鐘的讀法也變；長期沒執行的讀者重新執行時會讀到舊事件；守衛測試的判定要改 | 第 6 節 |
| 決定 5 | **暫停與倍率寫在哪個 Resource**（選 B 之後剩下的小題） | 甲＝另一個「想要的暫停與倍率」的 Resource，時間的 System 讀它；乙＝直接寫 `VirtualTime`，把它標成可以多個 System 寫 | 甲 | 多一個 Resource | 第 3.3 節 |
| 決定 6 | **兩個放哪裡** | (a) 沒畫面的 runner：算 Engine 的 Module，還是 Engine 外的 Plugin；(b) 時間的型別：`engine/time`，還是新的 Module | (a) Module；(b) `engine/time` | (a) Engine 多一個 Module；(b) `engine/time` 多了會被放進 World 的型別 | 第 9 節 |
| 決定 7 | **兩個 World 之間的事件現在做不做** | 甲＝這一輪只定形狀，PvP 移植時名指了事件再做；乙＝現在就做 | 甲 | 「兼容網路」這一輪只有形狀與規則，沒有可以跑的東西 | 第 6.4 節 |
| 決定 8 | **同意第 8 節列的既有條文與測試的變更** | — | — | — | 第 8 節 |

名字（`VirtualTime` 等）你已授權照命名規範訂，列在第 9 節，只回報、不另外問。

---

## 3. 時間

### 3.1 一個來源，三個投影

**你定過的**（HANDOFF `v8-arch-gyo2-unified-time`）：「首先時間來源必須統一。不管是 runner 也好還是 Sytem 也好，都需要一個統一的時間標準。」「可以參考 Bevy 重構 GYO::Time」。`GYO::Time` 是 Engine 的時間基準（time-threads-trace `HANDOFF.md:19` 的 D29，2026-10-08）。

**方向（參考 Bevy，側邊會話向你說明過、你回「可以參考」）**：一個來源導出幾個時間；暫停與慢動作改的是中間那一層；UI 讀真實時間。

```
Time::MonotonicClock（唯一的時間基準）
        │  每條執行緒的每次醒來取樣一次
        ▼
   RealTime        真實時間。不受暫停與倍率影響。
        │  × 倍率；暫停時為 0；上限（決定 3）
        ▼
   VirtualTime     可暫停、可縮放的時間。
        │  餵給累加器
        ▼
   FixedTime       固定步的步長，與「離上一個固定步過了多久」（內插用）。
```

| Resource | 欄位 | 說明 |
|---|---|---|
| `RealTime` | `now`（時間點）、`delta`、`elapsed` | 第一輪的 `delta` 是 0 |
| `VirtualTime` | `delta`、`elapsed`、`scale`、`paused`、`discarded`（累計扣掉的量） | `scale`、`paused` 是這一輪**實際套用**的值。暫停時 `delta` 是 0，拿它當除數的程式要自己處理 |
| `FixedTime` | `step`、`overstep` | 不放 tick |

- 全部用整數奈秒。倍率是浮點數，只在算「這一輪前進多少」時乘一次，乘完取整數。
- 三個都是 (b) 區的 Resource。**誰寫（決定 1：B）**：runner 每輪只把取樣的時間點寫進 World（一個對外交換用的 Resource，在交換點「進」）；外層 schedule 最前面的「時間的 System」從它算出 `RealTime` 與 `VirtualTime`；驅動固定步的 System 更新 `FixedTime`。這兩個 System 由 Engine 提供。
- **`FixedTime` 不放 tick**（規格 G.7 取捨 3、H-1；規格寫的，我認為理由獨立成立）：tick 要和事件緩衝一起被還原，放進 Resource 就有兩個來源。現在第幾個 tick 只經 `CurrentTick` 讀。累加器也不記 tick。
- 步長只在一處手動維持：組裝時給 runner 的設定。`FixedTime.step` 與 PvP 的 `FixedStep{stepNs}` 都由它寫入。
- **System 不自己讀時間基準**。不是 System 的 Engine 程式照舊讀（等待器醒來後重讀、音效後端每塊取樣、IO 的到達時刻）。

**通用的時間（決定 1b）**

選 B 之後多出一個要定的事，側邊會話提過、你還沒回答：

- Bevy 另有一個通用的 `Time`：平常它等於 Virtual 的值；進固定步之前被換成 Fixed 的值，跑完換回來。作者的 System 只讀這一個，放在每幀或固定步都是對的。
- 不做的話，作者要自己選讀 `VirtualTime` 還是 `FixedTime`。固定步裡的 System 如果讀了 `VirtualTime.delta`，拿到的是這一輪的量，一輪跑 k 步就被用了 k 次。

| | 甲（建議）：做通用的時間 | 乙：不做 |
|---|---|---|
| 作者 | 一般只讀通用的那一個；要特定的才讀 `RealTime`／`VirtualTime`／`FixedTime` | 自己選；選錯沒有人擋，除非另外加啟動檢查 |
| 機制 | 驅動固定步的 System 每跑一步前寫它、跑完還原（它本來就獨占 World） | 無 |
| 代價 | 多一個 Resource，而且它的意思隨所在的位置不同 | 作者要多懂一件事 |

建議甲：作者的負擔不能增加，這是 Bevy 選它的理由，也適用這裡。名字待定（不能叫 `Time`，會和 `GYO::Time` 撞）。

**作者仍然要自己管的一件事**：「讀哪個時間就決定跟不跟著暫停」只對讀時間的 System 成立。靠輸入動的 System（滑鼠轉視角）不讀時間，暫停時照樣在動。

### 3.2 決定性區的時間不變

**你定過的**（K.3 第 11 項，2026-11-11）：(c) 區的時間只來自 World 的 domain tick 與被記錄的 boundary 輸入；System 自己讀時間基準禁止。

這份設計不改它：
- 三個時間 Resource 都是 (b) 區，(c) 區的 System 讀不到（既有的 Z1 檢查，`ZoneRuleTests.cpp` J-39）。
- PvP 的 `HostClockSample` 是產品那一層的 boundary 輸入，由 PvP 的 plugin 在交換點寫，用的是同一次取樣。機制保證的只有「一次醒來只有一個取樣值」。
- 沒有呼叫 `RequireExplicitZones` 的產品沒有 (c) 區，不受這一節限制。

### 3.3 暫停與倍率（決定 5）

決定 1 選 B 之後：暫停與倍率由 System 改，時間的 System 下一輪套用（和 Bevy 相同：`virt.rs:28-40`）。

側邊會話向你說「決定 5 不用定」。我核對後發現**還剩一個小題**，因為 GYO 有一條 Bevy 沒有的規則：Resource 預設只准一個 System 寫。時間的 System 每輪要寫 `VirtualTime`，暫停選單的 System 也要寫暫停——兩個寫者。

| | 甲（建議） | 乙 |
|---|---|---|
| 做法 | 「想要的暫停與倍率」放另一個 Resource（暫稱 `TimeControl`），產品的 System 寫它；時間的 System 讀它、套到 `VirtualTime` | 產品的 System 直接寫 `VirtualTime`；把它標成可以多個 System 寫 |
| 寫者 | 各只有一個（`TimeControl` 本身標成可多寫：暫停選單、失焦、除錯台） | `VirtualTime` 有 Engine 的與產品的兩種寫者 |
| PvP「本機的暫停不能影響對局的固定步」 | 模擬的 World 不登記 `TimeControl`，就沒有人能暫停；啟動檢查保證 | `VirtualTime` 一定存在，擋不住；要靠審查 |

建議甲。這不是地基，可以到實作那一批的事前宣告再定。

細節（兩個選項都一樣）：
- 取消暫停後第一輪的量是一輪的真實經過時間，不會把暫停期間的時間一次灌進來。
- 暫停時固定步不跑，但外層的 schedule 每輪照跑，所以取消暫停的 System 放在外層就讀得到。
- **PvP Client 的對時**（把固定步調快調慢）：方向是走倍率——11 月 10 日網路設計稿 N6 的做法（「改 virtual 速度，步長不動」，lightyear 也是；N6 自己標「要變形」，還包括大誤差時直接跳 tick）。它要的是只有倍率、沒有暫停的東西，**移植 PvP 時定，這份設計沒有做**。
- 現在 PvP 的相位修正和倍率**只有瞬時的比例相同**（每輪最多套用經過時間的 25% ≈ 倍率 0.75 到 1.25）。有三點不同，移植時要重新設計：(1) 舊的是有目標量的位移，在同一輪套用、修到就停；倍率下一輪才生效，會修過頭，要另外的控制；(2) 舊的對「加快」有追趕容量的限制，就是為了不讓加快變成丟步——倍率 1.25 碰到長的一輪時會超過上限，被決定 3 甲扣掉，正是舊程式要防的；(3) 舊的在剛起步與空窗時另有一處截斷，照原樣搬過來會讓截斷出現第二處。

### 3.4 追不上時怎麼辦（決定 3）

**沒判過的**（概念稿 E9b「時間被截斷時不可以讓時間消失」）。blueprint 把 Bevy 的「max_delta 截斷讓時間消失」列為不照抄的反例（架構師 2026-11-10 寫的候選，沒有單獨問過你）。

| 選項 | 做法 | 代價 |
|---|---|---|
| 甲（建議） | 一輪最多跑 N 個固定步。超過的**整步**從 `VirtualTime` 這一輪的量裡扣掉，加進 `discarded` 並記一筆診斷 | 卡住之後遊戲裡的時間比真實時間少；少多少查得到。倍率大於 1 時，上限也會讓快轉變慢（同樣記在 `discarded`） |
| 乙 | `VirtualTime` 不扣，累加器自己丟整步（舊做法） | `VirtualTime` 與固定步永遠錯開。例：卡 2 秒，`VirtualTime` 前進 2 秒，固定步只前進 83 ms |
| 丙 | 不設上限 | 追不上時越追越慢 |
| 丁 | 一輪最多 N 步，但不丟，欠的留到之後幾輪追（欠的量也設上限） | 是 E9b 字面上的讀法；卡住之後遊戲會快轉一陣子追回來，欠的超過上限時仍然要丟 |

建議甲的理由：
- `VirtualTime` 與固定步前進的量永遠一致。
- 扣的是整步，所以 tick 格點對真實時間的相位不變，**和舊的累加器逐點相同**（v2 初稿寫「`maxDelta = 5 × step` 行為相同」是錯的：先夾總量會移動相位；檢查用兩個數字例子指出，已改成扣整步）。
- 丟掉的量有記錄，`RealTime` 不受影響。這是我對 E9b 的讀法——「不可以消失」讀成「不可以靜默」。**這個讀法要你確認**；如果你的意思是字面上的不丟，那是丁。

沒有固定步的 World（只有每幀）：`VirtualTime` 一輪的量設一個上限，超過的同樣記進 `discarded`。

決定 1 選 B 之後，甲由時間的 System 做：它讀得到 `FixedTime`（還差多少、步長），所以算得出這一輪會超過幾步，在寫 `VirtualTime` 之前就扣掉。Bevy 是在 Virtual 那一層設一個總量的上限（250 ms）並丟掉，沒有記帳，也不保整步。

---

## 4. 固定步由誰驅動（決定 1：已定 B）

**你定過的**：`FixedTickRuntime` 可以帶過來但名字要改。v0.2 §10.1：「GYO 的 Match Simulation 需要維持獨立的 60Hz 更新節奏。如果 Fixed Tick 被 Frame Update 直接驅動，就可能出現不理想的執行時序及延遲。新架構需要能夠表達：…Simulation Clock…Frame Clock…兩者可以共享部分資料，但不應因此被迫共享同一個執行節奏。」

**沒判過的**（概念稿 U7）：累加的狀態與驅動放在哪裡。

**我在 v2 初稿用來否決 B 的三個理由，檢查後撤回**：
- 「B 表達不了固定步在自己的執行緒」——錯。B 可以把模擬的 World 放在自己的執行緒，由那條執行緒每次醒來跑一張外層的 schedule，裡面的獨占 System 跑固定步。Bevy 沒畫面時就是這樣（`schedule_runner.rs:97-119`）。
- 「v0.2 §10.1 否決了 B」——讀寬了。原文要的是「能夠表達」兩個時鐘各有節奏，講的是 Match。A 和 B 都能做到。在同一條執行緒上，A 和 B 的固定步都是在一輪裡跑 0 到多次；差別只在固定步前面能不能排一段每幀的程式（下表）。
- 「v7 量到的問題（主執行緒卡住時模擬停擺）在 B 會回來」——那是模擬放在哪條執行緒的問題，A 把兩者放同一條時一樣會停。

### 4.1 A 和 B 實際差在哪裡

| | A：runner 累加、runner 驅動 | B：World 裡一個獨占的 System（Bevy） | C：狀態在 World、runner 驅動 |
|---|---|---|---|
| 做法 | runner 持有累加器；算出這一輪 k 步，呼叫 k 次「跑固定步的 schedule」 | 每一輪跑一張外層的 schedule；裡面一個獨占 World 的 System 累加，並跑 k 次固定步的 schedule | 累加的狀態是 World 裡的 Resource；runner 每輪讀它、算 k 步、呼叫 k 次、寫回 |
| 系譜 | **舊的形狀**：規格 G-6、H.2，舊的 `FixedTickRuntime` 與 Match 的迴圈。全局審計把「固定步累加留 Host」判成「結論沿用舊形」；R0 帶過去清單（你以底稿核准）把「Host 持有固定步累加器」列在「不帶的核心」 | Bevy `bevy_time/src/fixed.rs:243-258`。R0 的盤點（Opus，供參考的起點、不是結論）對這一項寫「未定，傾向照 Bevy 放在 World 內；要和預測回滾的重入一起比較」 | 規格 H.2 的驅動（runner 呼叫 k 次）加 World 裡的狀態；圖的評審寫「沒有 U10 的話在 A 與 C 之間選」 |
| 累加的狀態在哪 | runner（World 外）。World 裡是投影 | World 裡的 Resource。有回滾、摘要、存檔的產品要把它排除在外 | 同 B |
| 暫停與倍率 | 經請求（決定 5） | System 直接寫時間的 Resource | System 直接寫；但同一個 Resource 有兩種寫者（runner 與 System），runner 那一邊「獨占寫入」的檢查看不到 |
| Kernel 要加什麼 | runner 可以被呼叫（本來就要做）；「System 不能寫」的 Resource 登記 | **System 執行中再跑一張 schedule**。現在是斷言（`Run` 要求 World 是 Ready）；`ExclusiveWorld` 現在能讀全部、只能寫 (b) 區標成可多寫的元件，不能跑 schedule。這是概念稿的未判項 U10；圖的評審說工具的 Inspector、復原很可能也需要它 | runner 可以被呼叫 |
| 每幀寫的指令同一輪進固定步 | 可以，但每幀那一側要**拆成固定步之前、之後兩張 schedule**。現在的規則是一個時鐘只有一張帶時鐘的 schedule，另一張不能讀寫事件，指令要用 Resource 或元件傳。不拆（第 5.5 節組合（二）的排法）就晚一輪。直接從 OS 來的輸入不受影響：它在交換點寫進 World，固定步當輪就讀得到 | 一張 schedule 裡用 stage 就做到（Bevy：PreUpdate → 固定步 → Update） | 同 A |
| 回滾時重跑固定步由誰發動 | runner，在兩次 schedule 之間。System 發現預測錯了只能留下請求，等這一輪跑完 | System 當場重跑（同樣需要 U10） | 同 A |
| 對準 tick 格點的等待 | runner 自己知道還差多少 | runner 在 World 是 Ready 時讀一次 `FixedTime`，成本很小 | 同 B |
| 作者要寫什麼 | 無 | 無 | 無 |
| 固定步在自己的執行緒 | 可以 | 可以 | 可以 |

### 4.2 結果：B（使用者 2026-11-14）

原話（側邊會話轉達，`v8-arch-gyo2-fixed-step-in-world`）：「我看了下架構，A的做法其實沒有沒World都能成立。反而world變成的多餘的東西了。B反而是不錯的選擇。」「選擇B，轉回去。」

我原本建議 A，理由是 Kernel 不必支援「跑到一半再跑一張 schedule」、時間與迴圈只有一個主人。你沒有採。上面的比較表留作紀錄。

B 要做的事：

1. **Kernel：獨占 World 的 System 可以在執行中跑一張 schedule。** 現在 `Run` 要求 World 是 Ready，System 執行中是 Running，所以這是斷言。要重新確認的地方：同步點與延後的變更（外層那張跑到一半，裡面那張有自己的同步點）、hook 的佇列（現在每張 schedule 一個）、存取驗證的快照、裡面那張回 Err 時外層怎麼中斷、tick 的推進與事件的修剪。這一批排在最前面，事前宣告先給你看。
2. **這同時是概念稿未判項 U10 的一部分。** 你選的是 B；U10（獨占整個 World 的存取）你沒有另外表態。這份設計只取 U10 裡「獨占的 System 可以跑一張 schedule」這一點，而且先只給 Engine 自己那個驅動固定步的 System 用。U10 的其餘（獨占的 System 能寫什麼、給不給一般作者用）仍未判。
3. **外層的 schedule。** 有固定步的 World 都有一張每輪跑一次的外層 schedule，驅動固定步的 System 掛在它的某個 stage。沒畫面的產品也一樣（外層可以只有這一個 System）。固定步之前、之後各能排什麼，由 stage 決定——所以「UI 的指令同一輪進固定步」不必拆 schedule。
4. **累加的狀態在 World**（`FixedTime`，(b) 區）。做回滾、摘要、存檔的產品要把它排除。
5. **概念稿 E5**「System 執行中再跑一個 System 是 Programmer Error」推翻（限於上面第 2 點的範圍）。

### 4.3 累加器：`TickAccumulator`

**你定過的**：帶過來、改名。選 B 之後，累加的狀態（步長、餘數）放在 `FixedTime` 這個 Resource；`TickAccumulator` 是算術的那一部分，由驅動固定步的 System 用。

| | 舊的 `FixedTickRuntime` | 新 |
|---|---|---|
| 時間 | `double` 秒 | 整數奈秒 |
| tick 計數 | 有 | 沒有；tick 只在 World |
| 追趕上限 | 自己丟整步，不回報給時間那一層 | 上限由呼叫的人給；超過的整步從累加器拿掉並**回報數量**，上一層照這個數量從 `VirtualTime` 扣（決定 3 甲） |
| 介面 | `Advance(elapsed, callback)` | `Add(delta, 上限)` 回傳這一輪跑幾步、拿掉幾步；`Overstep()`；`UntilNextStep()`；`Reset()` |
| 放哪裡 | `engine/runtime`，用到 `engine/math` | `engine/time`，只用標準函式庫（`time.md` DI 10 不變） |

---

## 5. runner（決定 1 選 B 之後，runner 是薄的）

### 5.1 多個 World 由誰持有（決定 2）

**事實**（檢查更正了我的說法）：
- 型別 `Product` 就是規格的 Runtime：一個 World、它的 Registry、它的 schedule（`Product.hpp:3-5`；規格:42）。它是從 `App` 改名來的。
- 你定的「產品分 Game 與 Tool」講的是產品的種類；同詞異義表對這個型別寫的是「它裡面有什麼、含幾個 World、runner 歸誰，登記表判，這裡不定義」。所以「一個遊戲幾個 `Product` 物件」**不違反你定過的詞義**，只是型別名和產品同名會讓人混淆——那是命名的問題，另外處理。
- 「runner 一個」的規則，原文是「一個 Runtime 只能有一個」（側邊會話轉述；規格:387），也就是每個 World 一個。
- **你定過的**：PvP 的 Client 是三個 World（K.3 第 6 項，問的是 Client 的畫面與渲染要不要分開）。「單機也至少兩個 World」是架構師的推定，不是你定的。

**沒判過的**（概念稿 U8）：一個產品有多個 World 時，誰持有它們、誰負責它們之間的交換、一個 World 是不是綁一條執行緒。

| | 甲：`Product` 可以有多個 World | 乙：`Product` 維持一個 World |
|---|---|---|
| 一個遊戲是 | 一個 `Product`，裡面幾個具名的 World，各有自己的 Registry | 幾個 `Product`；組裝的程式建立它們，一個 runner 驅動它們 |
| 系譜 | Bevy：一個 App 有主 World 與渲染的 SubApp，各有自己的 World，一個 runner（`sub_app.rs`） | 規格：Runtime 是一個 World；Host（runner 的 plugin 加 `main`）建立多個 Runtime、負責它們的交換點（規格 H.4.2） |
| World 之間的交換與 Extract | `Product` 內部 | runner 與組裝的程式 |
| 「runner 一個」的規則 | 範圍從「每個 World」變成「每個 `Product`」（要改條文） | 字面不變，但實際上一個 runner 要驅動幾個 `Product`，規則的寫法也得補 |
| 代價 | `Product` 會吸收組裝的職責（你的 v0.2 §10.4 提醒過 Host 不要隨功能膨脹）。四個問題要定：runner 由哪一份 Registry 的登記單位安裝；登記單位怎麼替別的 World 登記交換函式；World 重建（Match 的 Reset）怎麼做；同一個 `Product` 物件被兩條執行緒使用時（第 5.5 節組合（三））怎樣才安全 | 型別名 `Product` 與產品同名的混淆留著；同樣的四個問題換成「跨 `Product`」，一樣要定 |

兩邊都需要「一個 World 之上、runner 驅動的那個東西」。差別是它叫 `Product`（甲）還是留在組裝的程式裡（乙）。

建議甲，理由：runner、World 之間的交換、Extract 的來源都有一個明確的持有者；型別名和產品的意思一致。弱點是 `Product` 變大，上表「代價」那四個問題在實作那一批（T5）之前要先定，這份設計還沒有定。

「一個 World 綁一條執行緒」（既有的啟動檢查 H-2）這份設計照舊，不改。**有幾個 World、各在哪條執行緒，是產品的決定，不是 Engine 的通則。**

### 5.2 runner 是什麼

**你定過的**：runner 由 plugin 在 finish 時安裝，一個 Runtime 一個（2026-11-11，側邊會話轉述）；Host 換成「組裝（setup）與 runner」；登記的單位拆成 `GYO::Kernel::Module` 與 `GYO::Kernel::Plugin` 兩種型別，Module 先登記、Plugin 可以依賴 Module、反過來不行（2026-11-14，`v8-arch-gyo2-module-plugin-registers`；程式還沒改）。

下文說「登記單位」時，指這兩種型別的任一種。11 月 11 日那條規則裡的「plugin」是在拆成兩種型別之前說的，所以「runner 由登記單位在 finish 時安裝」是我照新的決定改寫的讀法。

- runner＝**一個函式，擁有迴圈**（Bevy 的 `set_runner`：`bevy_app/src/app.rs:203-227`）。`Product::Run()`（不帶參數）把控制權交給它，它回傳時產品結束。
- 現有的 `Product::Run(schedule)` 是 runner 用的原語，保留並改名 `RunSchedule`。沒有安裝 runner 的產品（測試）照舊自己呼叫它。
- Kernel 不擁有執行緒、不讀時間基準。
- runner 每輪做三件事：取樣、跑外層的那張 schedule、等待。固定步由外層 schedule 裡的 System 跑，runner 不管。哪一張是外層，由組裝時告訴 runner。「schedule 的清單由誰給」（概念稿 U17）其餘的部分仍然沒判。

舊 Host 的職責（規格 H.7 的表）現在歸誰：

| 職責 | 歸誰 |
|---|---|
| 建 World、選 plugin 清單 | 組裝的程式 |
| 固定步累加、這一輪跑幾次 | World 裡驅動固定步的 System（決定 1：B） |
| 取樣、等待 | runner |
| 建立執行緒、停止與 join 的順序 | runner（它建的執行緒它負責收；`RoleThread` 的契約要求擁有者決定順序） |
| 交換點 | runner 呼叫；內容由各 plugin 登記（第 5.3 節） |
| 出錯之後 | runner 拿到 Err 就停下所有迴圈，把 Err 從 `Product::Run()` 回傳；World 留著可以讀（你定過：中斷後可讀，工具要能把未存的內容讀出來另存）。之後怎麼辦（結束、另存）是組裝的程式的事 |
| World 的重建（Match 的 Reset） | 沒有定。跟著決定 2 |
| 回滾的觸發 | 不在這份設計（`gyo2` 上還沒有回滾） |
| 平台事件、OS 的 modal loop 期間繼續畫 | 有視窗的 runner（不在這份設計） |

### 5.3 交換點

**規格寫的**（H.1；全局審計判為有獨立理由）：交換點＝在兩次 schedule 執行之間、World 是 Ready 時，把資料搬進或搬出 World 的位置；只有這裡可以上鎖。

- 一個 World 每一輪有兩個交換點：**進**（跑 schedule 之前）與**出**（跑完之後）。
- 登記單位在 finish 時登記交換函式。runner 依登記的順序呼叫（Module 的在 Plugin 的前面）。這是 Kernel 的新登記（列在第 8 節）。
- 每個交換函式拿到這一輪的資訊（取樣的時間點、第幾輪）。交換函式不等待別的執行緒，只用短的鎖。

誰會用（名指的）：時間的投影（Engine）；`HostClockSample`（PvP）；網路的收發緩衝；平台事件；資產 IO 的完成通知；World 之間的資料；診斷的輸出。

現況：World 在 Ready 時，外面可以寫任何 (b) 區的 Resource 與元件；跑過之後 (c) 區只准寫 Input 與 `external` 的 Resource（`World.cpp:192-203`）。所以「時間的 Resource 只有 runner 寫」現在沒有東西擋 System 去寫，要加一條登記（第 8 節）。

### 5.4 可注入的時間

runner 不直接呼叫 `Time::Now()` 與等待器，經兩個可換的零件：
- **取樣**：預設讀真實的時間基準；測試與重放換成腳本。
- **等待**：預設是 `Time::Waiter`；測試換成「不等，直接把腳本的時間推到期限」。

出處：v7 的注入測試（`04-client-roles.md:53`、`:116`）、規格的重放 host（H.4.4）；Bevy 的對應是 `TimeUpdateStrategy`。

限界：音效後端自己取樣真實的時間基準。用腳本時間時，聲音的時刻和 `RealTime.now` 不同源。

### 5.5 三種組合的順序

Loading 在三種裡都一樣：好之前外層每輪跑 Loading 的 schedule 一批；產品說好了之後，`FixedTime` 的餘數歸零，才開始跑固定步。由驅動固定步的 System 判斷（它讀產品的一個 Resource），不是 runner。工具（③）目前用（二），通常沒有固定步。

**(一) 沒畫面**（Match、專用伺服器、多數測試）。一條執行緒：

```
每次醒來：
  1. 取樣
  2. 交換點「進」（寫取樣的時間點、收外部資料）
  3. 跑外層的 schedule：
       時間的 System → 驅動固定步的 System（Loading 一批，或跑 k 次固定步）→ 其餘
  4. 交換點「出」
  5. 讀 FixedTime，等到「取樣時刻 ＋ 離下一步還差的時間」
```

第 5 步：還差的是可縮放的時間，除以倍率才是真實時間；暫停或倍率為 0 時改用組裝時給的閒置間隔。期限是時間基準上的絕對時間點，和舊的 Match 相同（`MatchRuntimeHost.cpp:531-534`）。Bevy 沒畫面的 runner 是「至少隔多久」，不對準格點；這一步是 GYO 多做的。

**(二) 有視窗、固定步與每幀同一個 World、同一條執行緒**（單機最簡單的組法）：

```
每一幀：
  1. 處理 OS 事件
  2. 取樣
  3. 交換點「進」
  4. 跑每幀的 schedule（它就是外層）：
       時間的 System → 輸入、UI → 驅動固定步的 System（0 到多次）→ 其餘的每幀邏輯
  5. 交換點「出」
  6. Extract 到渲染的 World，跑渲染的 schedule，送出畫面
  7. 等待由畫面決定（垂直同步或幀率上限），不等 tick
```

- 幀率不被 tick 頻率鎖住。主執行緒卡住時模擬也會停，不能接受的產品用（三）。
- 第一幀、或這一幀跑 0 個固定步：內插要的「上一個狀態」由產品自己留（機制不替它存）。
- 沒寫的：第 6 步渲染的 World 拿到的時間是哪一輪的值、由誰寫。Extract 那一批定。

**(三) 固定步在自己的執行緒**（PvP 的 Client）：

```
模擬的執行緒：同（一），驅動模擬的 World
主執行緒：同（二）但外層裡沒有驅動固定步的 System
兩個 World 之間：只在各自的交換點交換
```

- 兩條執行緒各自取樣（v7 第 04 批的設計）。「主執行緒停 250 ms 時模擬仍每步產生命令」（v7 的判定）在這裡成立，條件是交換函式不等待另一條執行緒。
- runner 建立並負責收模擬的執行緒。
- **沒寫的**：暫停的請求在畫面的 World、固定步在模擬的 World，怎麼傳；兩個 World 之間的資料是「只留最新一份」還是佇列、誰持有；兩條執行緒的 `RealTime` 不同步時，跨 World 的時間以誰為準。這些在 PvP 移植、名指了要傳什麼的時候定。

**現在做得出來的**：（一）和它的測試；（二）（三）的迴圈邏輯可以用腳本時間與假的畫面等待來測。真正有視窗的 runner 要等平台的 Module（SDL）帶回來——你已經定了 KeepTool 那一批（`v8-arch-gyo2-keeptool`），它會帶回 SDL。

---

## 6. 事件（決定 4）

**你定過的**（`v8-arch-gyo2-cross-clock-events-open`）：「放開：照 Bevy，讓事件留到另一側讀過為止。我認為這個機制，還需要考慮一下如何設計。不光要考慮單機，也要兼容網絡部分。」K.3 第 15 項裡「同一時鐘下的另一張 schedule 用元件或 Resource」那一半是你選的，這份設計不動。

**規格寫的、沒有單獨問過你的**：事件沒有游標、依讀者在執行計畫裡的位置讀（F.8 取捨 3，理由是回滾不必處理游標）；緩衝只留兩個 tick。全局審計把「事件無游標、不跨時鐘」判成「PvP 需要、但對所有 World 生效」。R0 帶過去清單（你以底稿核准）列了「事件讀者的兩種模式」這個名字；它的內容——預設有游標，可回滾的 schedule 強制無游標——寫在 R0 的盤點裡（Opus 寫的，自己註明是供參考的起點、不是結論），你沒有看過那個定義。

現況：事件登記時屬於一個時鐘。讀者與寫者都必須在那個時鐘的 schedule 裡。讀者讀到「本 tick 排在它前面的寫者」與「上個 tick 排在它後面的寫者」的紀錄。讀者某一次被 run 條件跳過，那一次該讀的就錯過了。

### 6.1 要定的其實是三件事

v2 初稿把它們綁成「甲或乙」，而且把甲寫成「不加游標」。檢查指出：甲的水位就是一個放在 World 裡的游標，只是比較粗；甲和乙都有兩套讀法。重新分開：

| 軸 | 可以怎麼選 |
|---|---|
| 一、「讀到哪裡」記多細 | (i) 每個讀者一個游標（Bevy）；(ii) 每個（事件、讀的那個時鐘）一個水位；(iii) 不記，依位置讀（現況） |
| 二、紀錄留到什麼時候 | (i) 留到所有會讀它的讀者都讀過，或碰到上限；(ii) 固定留兩個 tick（現況）；(iii) 固定輪替兩次、由時間那一側發訊號決定輪替（Bevy） |
| 三、碰到上限怎麼辦 | 丟最舊的並記一筆；或（對不能丟的事件）當成錯誤 |

### 6.2 三個選項

| | 丙（建議）：全部的讀者都有游標 | 乙：只有跨時鐘的讀者有游標 | 甲：跨時鐘用水位 |
|---|---|---|---|
| 同一個時鐘的讀者 | 游標 | 照現況（依位置） | 照現況（依位置） |
| 另一個時鐘的讀者 | 游標 | 游標 | 每個（事件、時鐘）一個水位 |
| 可回滾的 schedule | 維持依位置讀、不參與跨時鐘（第 6.3 節） | 同左 | 同左 |
| 讀者被 run 條件跳過那一次 | 不會錯過，下次補讀（在上限以內） | 同時鐘：錯過；跨時鐘：補讀 | 都錯過 |
| 讀者長期沒執行、之後又執行 | 一次讀到累積的舊事件（最多到上限）；它沒讀的期間，紀錄一直留著 | 跨時鐘的讀者同左 | 沒有這個問題（以 schedule 為單位，schedule 有跑就算讀過） |
| 作者要記幾套規則 | 一套（只有做回滾的產品要多記一套） | 兩套，依讀者和事件的關係 | 兩套 |
| 系譜 | 游標的粒度是 Bevy 的（`message_cursor.rs`）；保留的方式不是（見下）。R0 盤點的「兩種模式」；審計對「無游標」的判定 | — | v1 的提議 |
| 改到什麼 | 同時鐘的讀法也變：帶條件的讀者不再漏讀（改善），但長期沒執行的讀者會讀到舊事件、並拉住保留（代價）。Match 不回滾，它的讀法也跟著變。現有的事件測試 J-36、J-37 的判定要改，照規則要你核准 | 只多一條跨時鐘的讀法；J-37 的跨時鐘案例要改 | 同乙 |
| 每幀寫、固定步讀，一輪跑 3 個 tick | 第一個 tick 讀完，後兩個讀不到新的 | 同左 | 同左 |

三個選項的保留都是軸二的 (i)：留到會讀它的都讀過，或碰到上限。選 (i) 是照你的原話「讓事件留到另一側讀過為止」的字面。**這和 Bevy 不同**：Bevy 是 (iii)，緩衝固定輪替兩次、固定步那一側跑過才輪替；讀者超過兩次輪替沒讀就漏，而且沒有上限。我沒有把 (iii) 列成選項，理由是它不保證「讀過為止」；如果你說的「照 Bevy」包含保留的方式，請告訴我。軸三（碰到上限怎麼辦）不分選項，都照 6.3 (1)。

建議丙的理由：
- 游標的粒度照 Bevy（每個讀者一個）。這一條分不出丙和乙：兩者在跨時鐘都是每個讀者一個游標。你的原話講的是跨時鐘的保留，沒有講同一個時鐘的讀者，所以「同一個時鐘也改」是我的建議，不是你定過的。
- 「帶條件的讀者會漏讀」是現在作者會踩的坑；丙把它拿掉，而且不分同時鐘、跨時鐘，作者只記一套。
- 「無游標」的理由只有回滾。丙把它收回到真的做回滾的 schedule 上，其他產品不必承擔。這也是 R0 清單那一列的意思。

丙的代價：改動比乙大，現有的同時鐘行為會變（往作者有利的方向），守衛測試的判定要改。如果你要先小步走，選乙，之後再決定要不要走到丙。

游標的細節（三個選項裡有游標的都適用）：
- 游標是每個（讀者的掛載點、事件）一個序號，放在 World。序號單調遞增，不回滾。
- 讀者讀到「序號比游標大」的全部紀錄，依（tick、寫者位置、序號）排序，然後把游標推到當時的最新序號。同一輪裡在它之後才寫進來的（包括 hook 寫的）下次讀到。
- 讀者第一次執行時游標是 0，會讀到保留著的全部紀錄（上限以內）。**沒定的小題**：跨時鐘的讀者要不要跳過「它那張 schedule 開始跑之前」的舊紀錄（例如固定步第一次跑時，Loading 期間每幀那一側寫的 UI 事件）。我傾向跳過，實作那一批定。
- 讀者拿得到每筆紀錄的 tick（事件所屬時鐘的計數）。現在的 `EventReader::Read()` 只回內容，要加介面（第 8 節）。

### 6.3 兩條限制

**(1) 有上限。** 每種事件登記時給「最多留幾筆」（有預設值）與「滿了怎麼辦」：丟最舊的並記一筆診斷（預設），或當成錯誤。讀者長期不執行（暫停時固定步不讀；runner 從來不跑的 schedule；丙之下長期被條件關掉的讀者）就會碰到上限。**「當成錯誤」要小心用**：錯誤會讓 World 中斷。例如把 UI 的指令設成「滿了是錯誤」，暫停時每幀照常寫、固定步不讀，碰到上限整個 World 就停了。會被暫停擋住的讀者，它讀的事件不該用這個設定；怎麼在啟動時提醒，實作那一批定。Bevy 沒有上限（`messages.rs:79`）。

**(2) 可回滾的 schedule 不參與。** 它的事件不給別的時鐘讀，它也不讀別的時鐘的事件；它自己的讀者維持依位置讀。啟動時檢查。理由：
- 回滾會還原那個時鐘的事件緩衝再重跑：另一側讀過的會重新發出，被推翻的另一側收不回來。
- 反方向：重跑時游標已經前進，讀不到；游標若也被還原，就會重讀。兩者都讓重跑的結果和第一次不同。
- 每幀那一側要送資料進可回滾的固定步，照規格既有的規則走 Ingress（規格 H.2）。

### 6.4 兩個 World 之間（決定 7）

兩個 World 不共用事件緩衝。**沒有名指的使用者**：PvP 現在模擬到畫面走的是「只留最新一份」的資料，遠端的射擊與命中是畫面那一側從快照偵測的，不逐筆送事件；「模擬在自己執行緒的單機」是假設。照 AGENTS §2，沒有名指 Feature 的項目延後。

所以建議（甲）：**這一輪只定形狀，不實作**。形狀是——World 之間的事件是「另一個 World 裡的讀者」：來源那邊有自己的游標與保留（和 6.2 同一套），在來源的交換點「出」取走、在目的地的交換點「進」寫入；每筆帶來源的 tick 與來源那一輪的取樣時間；佇列有上限、丟了要記。v2 初稿的寫法有兩個洞（一輪跑多個 tick 時，前面幾個 tick 的紀錄在取走之前就被修剪；目的地的紀錄沒有地方放來源的 tick）；上面的形狀針對這兩點改了，細節要到實作時才定。

我對「也要兼容網絡部分」的回應是這三點，**請你確認夠不夠**。照實說：這一輪對網路給的是規則與形狀，沒有可以跑的東西。
1. 可回滾的 schedule 不參與（6.3），維持依位置讀。所以 PvP Client 的模擬不受新的讀法影響。Match 不回滾，它的讀法照決定 4 改。
2. 單機與網路用同一組介面（`EventWriter`、`EventReader`，紀錄帶 tick）。但做回滾的 schedule 要多記一套讀法；跨 World 的事件還沒做。
3. 跨 World 的形狀定了（來源的 tick 放在目的地紀錄的哪個欄位、經哪個介面讀，實作時定），等 PvP 移植時名指要送哪些事件再做。重跑時重新發出的紀錄送不送、被推翻的預測事件怎麼辦，是那時候要你定的事，這份設計沒有替你定。

如果你要現在就做（乙），要先名指至少一個要逐筆跨 World 送的事件。

---

## 7. 聲音的時刻

**你定過的**（2026-10-30，PvP）：本機射擊＝按下事件的時間戳；遠端射擊、命中、受擊＝把事件的 tick 換成那個 tick 被顯示的時間，用產品自己的時間軸（`SnapshotTimeline::DisplayTime`）。HANDOFF 寫明「音效，不要丟」。混音器要的是時間基準上的時間點（`audio.md` 的契約）。

- 這份設計**不改 PvP 的做法**。PvP 的 tick 是權威的 tick，只有產品的時間軸換得對。
- 單機的聲音時刻**這份設計沒有做**。v2 初稿給的換算公式撤回：畫面那一側現在讀不到固定步是第幾個 tick；公式算的是模擬的時刻，不是「被顯示的時間」。
- 這份設計留給它的只有：事件的紀錄帶 tick、讀者拿得到（6.2）；`RealTime.now`；`FixedTime`。接音效那一批再設計換算，那時候要定「被顯示的時間」在單機是什麼。

---

## 8. 會改到的既有條文、程式與測試（決定 8）

master 的 target 條文在 `gyo2` 上是「R0 決定不帶，但檔案還在、程式的註解仍然引用」（`docs/architecture/target/` 有 10 頁）。下面一併列。

**條文**

| 條文 | 改成 | 誰定的 |
|---|---|---|
| 規格 F.3 的跨時鐘禁止、F.8 取捨 4、F-9 的跨時鐘那一半；target `kernel` DI-KS 10、DI-KS 14、`world` DI 6 | 帶時鐘的 schedule 可以讀別的時鐘的事件；可回滾的除外 | 禁止是規格寫的；放開是你定的 |
| 規格 F.8 取捨 3「事件無游標」、F-7（修剪後讀不到）、F.7 的 Bevy 對照 | 選丙：只剩可回滾的 schedule 無游標。選乙、甲：跨時鐘的讀者有游標或水位 | 規格寫的 |
| 規格 F.3「緩衝只存兩個 tick」 | 留到讀過或碰到上限 | 規格寫的 |
| 規格 F.3「跨時鐘的傳遞是 Host 交換點的職責」 | 同一個 World 內由 Kernel 做 | 規格寫的 |
| K.3 第 15 項「同一時鐘下的另一張 schedule 用元件或 Resource」 | **不變** | 你選的 |
| 規格 H.2「共用 World 時每幀寫、固定步讀的資料必須經 Ingress」 | 只對可回滾的 schedule 留著 | 規格寫的 |
| 規格 G-6、H.2、H.7「固定步累加在 Host」；target `determinism` §4.7b；規格 H-9「重跑只能在 Ready、不在任何 System 內」的精神 | 推翻（決定 1：B）：累加在 World 裡的 System；獨占的 System 可以跑一張 schedule | 規格寫的；推翻是你 2026-11-14 定的 |
| `FixedTickRuntime` 的型別與語義 | `TickAccumulator`：整數、不記 tick | 改名是你定的 |
| blueprint §4「max_delta 截斷讓時間消失」；概念稿 E9b | 決定 3 | 架構師寫的候選；未判 |
| 「一個 Runtime 一個 runner」（規格:387；target `runtime-composition` DI 2） | 選甲：一個 `Product` 一個 | 側邊會話轉述 |
| 規格 H.2 的 `FrameTime{frameIndex, deltaSeconds}` | 由 `RealTime`／`VirtualTime` 取代 | 規格寫的 |
| target `runtime-composition` §2「Client 有三個 Runtime」寫成 Engine 的通則 | 不當通則；是 PvP 的組合 | 三個 World 是你為 PvP 定的 |
| `time.md` DI 1、DI 10、Replacement Criterion 4 | **不變**（期限仍是時間基準上的時間點） | 你定的／照草稿同意的 |
| 概念稿的未判項 U4、U7、U8、E9b、E10a、E10d | 由決定 1～4 收口；U10、U15、U17 仍未判 | — |
| 概念稿 E5「System 執行中再跑一個 System 是 Programmer Error」 | 推翻，範圍限於「獨占 World 的 System 跑一張 schedule」 | 架構師寫的；推翻跟著決定 1 |
| 概念稿 U10「獨占整個 World 的存取」 | 取其中一點（跑一張 schedule），其餘仍未判 | 你沒有對 U10 另外表態 |
| blueprint P3「多個時鐘、單一權威驅動點，Time 只是投影」；P6「跨固定步的事件要留多久由時間那一側負責」；§5「BeforeFixed 累積、AfterFixed 清除寫進契約」 | P3：決定 1。P6 與 §5 那一句：不採（保留改成「留到讀過或碰到上限」，由 Kernel 負責） | 架構師從 Bevy 的審閱抽出的候選 |
| target `scheduler` 的 `ScheduleOptions` 欄位表（`runner`）；target `world` 談跨時鐘禁止的本文 | 跟著上面的改名與放開 | — |
| R0 帶過去清單「不帶的核心：Host 持有固定步累加器」「事件讀者的兩種模式」 | 決定 1、決定 4 | 你以底稿核准 |

**新的作者規則**（從契約直接導出的；照 AGENTS §1 要登記到 `docs/cross-component-rules.md`）：`VirtualTime.delta` 可能是 0；取消暫停的 System 要在每幀那一側；固定步裡不讀 `VirtualTime`。

**程式**

| 位置 | 改動 |
|---|---|
| `engine/time` | 新增 `TickAccumulator`；新增三個時間的型別與 `TimeControl`（決定 6） |
| 新 Module `engine/runner` | 取樣與等待的零件、交換點的呼叫、薄的迴圈；時間的 System 與驅動固定步的 System |
| Kernel 的執行 | 獨占 World 的 System 可以在執行中跑一張 schedule（`Product`、Executor、`World` 的狀態、同步點、hook 佇列、存取驗證、出錯的處置） |
| `Product` | `Run(schedule)` 改名 `RunSchedule`；新增 `Run()`；選甲時可以有多個 World。`Product.hpp`、`World.hpp` 註解裡的 Host、`Run` 跟著改 |
| `FinishContext::InstallRunner` | 連函式一起安裝；新增交換函式的登記。登記單位拆成 `Module`／`Plugin` 兩種型別是另一個已定的變更（`v8-arch-gyo2-module-plugin-registers`），不在這份設計的批次裡，但這裡的登記要在它之後或一起做 |
| `ScheduleOptions.runner` | 改名 `threadRole`（它的意思是「哪個執行緒角色呼叫」）。會碰到 `Declarations.hpp`、D-E13 與 X-3 的訊息、指紋的文字輸出 |
| 事件 | 游標或水位、保留與上限、`EventReader` 給 tick、修剪的規則、啟動檢查 |
| Resource 的登記 | 修訂 2 提的兩種新登記（「System 不能寫」「某個時鐘裡不能讀」）選 B 之後不需要：時間的 Resource 由 System 寫，照一般的寫者規則；讀錯時間的問題由決定 1b 處理 |

**測試與守衛**（判定會變的，各批開工前照 AGENTS §14.4 列五點請你核准）

- J-37 的跨時鐘案例（`ZoneRuleTests.cpp`）：從「啟動錯誤」變成通過——放寬。
- 選丙時 J-36、J-37 的同時鐘讀法。
- J-24 的 R-E6（`RegistryTests.cpp`）與 `InstallRunner` 的簽名。
- 用到 `ScheduleOptions.runner` 的測試（`SchedulerTests.cpp`）。
- 新的登記欄位（事件的上限、交換函式）要進 Registry 指紋，J-18 的 golden 會變。
- 「`Run` 要求 World 是 Ready」的斷言測試（巢狀執行現在是斷言）：判定要改。

**Architecture Delta 的七項**（AGENTS §3）：

1. 需要它的 Feature：第 1 節的表。
2. 現在的問題：沒有人負責時間與迴圈，五件事只做一半。
3. 變動的邊界：新增 Engine 的 Module `runner`；Kernel 新增「獨占的 System 跑一張 schedule」、交換函式的登記、事件的游標與保留；選甲時 `Product` 持有多個 World 與多份 Registry。
4. 影響：`engine/kernel`、`engine/time`、新的 `engine/runner`；上面列的測試。產品與工具目前都不在 `gyo2` 上。
5. 依賴方向：`runner` → `kernel`、`time`、`threads`。`kernel` 不依賴 `runner`、`time`。`time` 照舊只依賴 base。之後 audio 等 Module 要讀 `RealTime` 時，依賴的是 `time` 裡的型別，不是 `runner`。
6. Ownership：取樣歸 runner；時間的三個 Resource、累加的餘數歸 World，由 Engine 的兩個 System 寫（決定 1：B）；「現在第幾個 tick」歸 World，不變；「這一輪跑幾次固定步」從 Host 的職責變成 World 裡一個 System 的職責；同一個 World 內跨時鐘事件的傳遞從 Host 的職責變成 Kernel 的職責。
7. 為什麼不能更小：只補 runner 而不定時間，每個產品會各寫一套時間的 Resource；只開放跨時鐘事件而不改保留，一輪多個 tick 時就會漏。多 World 的持有者（決定 2）現在定，是因為 Extract 與交換點都要知道「另一個 World 是誰」；可以更小的是跨 World 的事件，所以延後（決定 7）。

**Design Invariants 與 Replacement Criteria**（草案，各批實作前定稿並附守衛）：

Module `runner`：
- DI 1：一條執行緒的一次醒來只取樣一次；這一輪的時間 Resource 與交換函式都用同一個值。
- DI 2：`RealTime`、`VirtualTime`、`FixedTime` 各只有一個寫者，都是 Engine 的 System（既有的獨占寫入檢查就是守衛）；runner 只寫取樣的時間點。
- DI 3：runner 不排 System 的順序，只呼叫「跑一張 schedule」與交換函式。
- DI 4：取樣與等待都經可注入的零件。
- DI 5（只對有固定步的 World）：`VirtualTime` 前進的量等於固定步前進的量加上餘數的變化；超過上限的只在一處扣，而且扣整步。
- Replacement Criteria：(a) 某個產品的迴圈用這些零件拼不出來；(b) 交換函式要知道別的交換函式的內部狀態才能正確；(c) 時間的 Resource 需要第二個寫者；(d) 「獨占的 System 跑一張 schedule」讓 Kernel 的同步點、存取驗證或出錯處置出現修不完的特例——那時重新比較決定 1 的 A、C。

Kernel 的事件（取代 DI-KS 10）：每筆紀錄對每個有游標的讀者恰好一次；保留有上限，超過的處置在登記時指定，丟棄一定有記錄；可回滾的 schedule 不參與跨時鐘、不用游標；游標的序號不回滾。

`TickAccumulator`（加進 `time.md`）：整數；不記 tick；不讀時間基準；拿掉的整步全部回報，不靜默。

---

## 9. 名字與放哪裡

**名字**（你已授權照命名規範訂；這裡是回報）：

| 東西 | 名字 | 說明 |
|---|---|---|
| 累加器 | `TickAccumulator` | 已訂 |
| 真實時間 | `RealTime` | 已訂 |
| 可暫停可縮放的時間 | `VirtualTime`（原訂 `GameTime`） | 「Game」已定為產品的一種，工具也會用到這個時間；照「一個詞只指一件事」改。Bevy 的詞 |
| 固定步的時間 | `FixedTime` | 已訂 |
| 暫停與倍率的請求 | `TimeControl` | 新 |
| `ScheduleOptions.runner` | `threadRole` | 和「runner＝擁有迴圈的函式」撞名 |
| `Product::Run(schedule)` | `RunSchedule` | 讓出 `Run()` |
| 事件 | 維持 `Event` | 不改叫 Message：會撞到 base 的 `LogMessage` 與 wire 的訊息 |
| PvP 的 `HostClockSample` | 這份設計不改 | 名字裡的「Host」是被換掉的角色詞，但它是你的命名規範舉的例子，屬於 PvP 那一層；移植時定 |

**放哪裡**（決定 6，這兩個是放置與 Ownership，不是命名）：

(a) 沒畫面的 runner 算 Module 還是 Plugin：
- 你定過的：判準是「少了它，完整的 Game 成不成立」；登記的單位有 `Module` 與 `Plugin` 兩種型別。
- 我的套用（不是你定的）：沒有東西跑迴圈，遊戲不成立，所以沒畫面的 runner 是 Engine 的 Module（`engine/runner`），由一個 `Module` 型別（例如 `RunnerModule`）安裝；有視窗的 runner 由平台那個 Module（SDL）提供。
- 另一個選項：放 Engine 外當 Plugin。那樣 Engine 自己的測試就沒有 runner 可用，除非測試各自寫迴圈。

(b) 時間的型別（`RealTime` 等）：
- 你說的是「參考 Bevy 重構 **GYO::Time**」。同詞異義表只要求名字避開 `GYO::Time`（不要叫 `Time`），沒說位置。
- 選項：放 `engine/time`（它們是不依賴 Kernel 的普通型別，`time.md` DI 10 不受影響），由 runner 的 `Plugin` 登記成 Resource、由 runner 寫；或放新的 Module `runner`。
- 建議 `engine/time`：符合你的原話，而且之後 audio 等 Module 要讀 `RealTime` 時不必依賴 `runner`。

---

## 10. 不在這份設計裡的

- **狀態的轉換**（選單、遊戲中、暫停；U15）：未判。
- **獨占整個 World 的存取、System 執行中跑 schedule**（U10）：未判；決定 1 選 B 才需要。
- **schedule 的清單由誰給**（U17）：未判。這份設計只用到「組裝時告訴 runner Loading 何時算好」。
- **回滾**：`gyo2` 上還沒有。這份設計只留一條規則：可回滾的 schedule 不參與跨時鐘事件。
- **兩個 World 之間的事件**：只定形狀（決定 7）。
- **有輸入才更新的工具**：沒有名指的 Feature。
- **有視窗的 runner、OS modal loop 期間繼續畫**：要平台的 Module（SDL），移植要你同意。有游標的讀者被跳過時不會漏讀，所以它不會反過來要求改事件的設計；對 runner 契約的影響沒有分析。
- **單機的聲音時刻**：接音效那一批。
- **PvP 的對時算法怎麼對到倍率**：移植時。
- **帶過來的 `time`、`threads`、`audio` 的重審**：這份設計只確認了 `time.md` 的 DI 1、DI 10、Replacement Criterion 4 不受影響。

---

## 11. 批次（決定 1：B；其餘照我建議的選項排，你改了就重排）

| 批 | 內容 | 依賴 |
|---|---|---|
| K0 | Kernel：獨占 World 的 System 可以在執行中跑一張 schedule。**事前宣告先給你看** | 決定 1（已定） |
| T1 | `TickAccumulator` 與時間的型別（`engine/time`） | 決定 3、6 |
| T2 | Kernel：可呼叫的 runner、交換函式的登記、`threadRole` 與 `RunSchedule` 的改名 | 決定 8 |
| T3 | Module `runner`：取樣與等待的零件、薄的迴圈、時間的 System、驅動固定步的 System、組合（一）與測試 | K0、T1、T2、決定 1b、3、5 |
| T4 | Kernel：事件的游標、保留、上限、啟動檢查 | 決定 4、8 |
| T5 | `Product` 多個 World；Extract 的執行；組合（二）（三）的迴圈邏輯 | 決定 2；先定 5.1 表裡的四個問題 |
| 另外已定、還沒排的 | 登記單位拆成 `Module`／`Plugin` 兩種型別與兩份登記檔（`v8-arch-gyo2-module-plugin-registers`）；KeepTool（`v8-arch-gyo2-keeptool`，會帶回 SDL 與 ImGui，需要有視窗的 runner） | 建議在 T2 之前或一起做型別拆分；KeepTool 在 T3、T5 之後 |
| 之後 | World 之間的事件；回滾 | 名指的 Feature |

每一批開工前各有事前宣告；判定用的測試先列突變。K0、T1、T4 彼此不相依，可以並行。

---

## 12. 範圍的限界

- 這是設計提案，沒有寫程式，沒有量測。第 5.5 節的順序是從現有程式與 Bevy 讀出來整理的，沒有跑過。
- 這份設計送過兩輪別模型的檢查（Opus／xhigh 一輪、Opus／high 複查一輪）。修訂 2 照複查改完之後沒有再送檢查；修訂 3 照決定 1：B 改寫的部分也**沒有送檢查**。修訂 1 就曾經在修改時引入新的錯，這一份也可能有。
- 兩輪檢查都是在「建議 A」的前提下做的。B 的做法只有比較表那幾列被查過；第 4.2 節列的 B 要做的事、第 5.5 節照 B 改的順序，是我寫的，沒有人查。K0 的事前宣告會另外送檢查。
- Bevy 的行為引自偵察 B 與檢查者的讀碼（Bevy 6a911653f）。
- 你的原話一律引自 `HANDOFF.md`、同詞異義表與你的原稿，沒有重讀 11 月 11 日側邊會話的原始紀錄檔。
- 我的解讀、要你確認的有兩處：E9b 讀成「不可以靜默」（第 3.4 節）；「也要兼容網絡部分」的三點回應（第 6.4 節）。
- 你說的「以前的做法是用狀態控制，但是這個做法明顯適合 GYO」那一句，讀法一直沒有確認過；這份設計沒有依賴它的任何一種讀法。
- 沒有定、但實作前一定要定的：決定 2 的四個問題（runner 由哪份 Registry 安裝、跨 World 登記交換函式、World 重建、一個 `Product` 被兩條執行緒使用）；組合（三）跨 World 的三個問題（第 5.5 節）；PvP Client 的對時怎麼給倍率（第 3.3 節）；跨時鐘的讀者要不要跳過舊紀錄（第 6.2 節）。

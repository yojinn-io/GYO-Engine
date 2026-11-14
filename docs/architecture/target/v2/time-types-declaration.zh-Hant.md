# 事前宣告：累加器與時間的型別（`v8-arch-gyo2-time-types`，設計裡的 T1）

更新：2026-11-15 夜。Owner：架構（GYO2 重建會話）。**狀態：草稿，待使用者核准。核准之前不動工。**

這一份是使用者夜間離開時（「你經量往前推，需要我的等明天」）先起草的。它和 Kernel 那一批（K0）不互相依賴，核准後可以先做。

根據：[時間與 runner 的設計](rebuild-time-design.zh-Hant.md)第 0 節、第 3 節、第 4.3 節、第 9 節；[HANDOFF](HANDOFF.md) 的 `v8-arch-gyo2-unified-time`（「首先時間來源必須統一」「FixedTickRuntime 帶過來可以但是名字需要修改」「可以參考 Bevy 重構 GYO::Time」）與 `v8-arch-gyo2-time-plan-two`（通用的時間；追不上時扣整步並記錄；暫停與倍率用另一個 Resource；型別留在 `engine/time`）。

---

## 1. 這一批做什麼

在 `engine/time` 加上純資料的型別與純算術，全部不碰 Kernel、不讀時間基準：

| 東西 | 是什麼 |
|---|---|
| `RealTime` | 真實時間：這一輪取樣的時間點、和上一輪的差、累計 |
| `VirtualTime` | 可暫停、可縮放的時間：這一輪的量、累計、這一輪實際套用的倍率與暫停、累計扣掉的量 |
| `FixedTime` | 固定步：步長、離上一個固定步過了多久（內插用）、累計。它裡面裝著累加器，狀態只有一份 |
| `CurrentTime`（名字見第 3 節） | 通用的時間：這一輪的量與累計。平常等於 `VirtualTime` 的值，固定步裡等於 `FixedTime` 的值 |
| `TimeControl` | 想要的暫停與倍率（產品寫它，時間的 System 下一輪套用） |
| `TickAccumulator` | 累加器：加進一段時間、一次取走一步、回報餘數。整數奈秒，不記 tick |
| `AdvanceVirtual(…)` | 一個純函式：給這一輪的真實經過時間、`TimeControl`、上限、累加器現在的狀態，算出 `VirtualTime` 這一輪該前進多少、扣掉多少 |

**需要它的 Feature 與 Product**（AGENTS §2）：固定步（①PvP、②單機）、暫停與慢動作（②單機、③工具的預覽）、UI 讀真實時間（①②③）。這些是設計第 1 節點名的。

**為什麼是基盤**：每個讀時間的 System 都會照這些型別的欄位寫。

**這一批不做的**（都在後面的批次）：把它們登記成 World 的 Resource；時間的 System 與驅動固定步的 System；進出固定步時換 `CurrentTime` 的值；runner；取樣。這一批做完，World 裡還沒有時間。

---

## 2. 行為（都是已定的，這裡寫成可以測的形狀）

1. **整數奈秒**。倍率是浮點數，只在算「這一輪前進多少」時乘一次，四捨五入取整數。
2. **累加器**：
   - `Add(量)`：加進餘數，回傳現在有幾個整步可以跑。
   - `TakeStep()`：有整步就取走一步、回傳真；沒有就回傳假。驅動的迴圈是「取得到就跑一次」。
   - `Overstep()`：餘數（不足一步的部分，加上還沒取走的整步）。`UntilNextStep()`：離下一步還差多少。`Reset()`：餘數歸零。
   - 它不丟時間，也不記 tick。tick 只在 World。
3. **追不上時**（你定的：超過上限的整步從可縮放的時間扣掉，丟了多少有記錄）：
   - 有固定步：`AdvanceVirtual` 先算「加進去之後會有幾個整步」；超過上限 N 的那幾個**整步**從這一輪的量扣掉，記進 `discarded`。所以 `VirtualTime` 前進的量，永遠等於固定步前進的量加上餘數的變化。
   - 沒有固定步：這一輪的量超過上限（一個時間長度）的部分扣掉，同樣記進 `discarded`。
   - `RealTime` 不受影響。
4. **暫停**：這一輪的量是 0。取消暫停後第一輪的量是那一輪的真實經過時間，不會把暫停期間的時間一次灌進來（因為每一輪只用「這一輪」的真實經過時間，暫停期間的每一輪都已經各自算成 0）。
5. **`VirtualTime` 上的倍率與暫停是這一輪實際套用的值**，不是想要的值；想要的在 `TimeControl`。

和舊的 `FixedTickRuntime` 的對照（設計第 4.3 節）：舊的用 `double` 秒、自己記 tick、追不上時自己丟整步而不告訴別人。新的行為在「哪幾個時刻跑固定步」上和舊的逐點相同（扣的是整步，所以相位不變）；判定 2 用一個照舊邏輯寫的整數版對照實作來驗。

---

## 3. 要你知道或定的

1. **通用的時間的名字：`CurrentTime`**（你授權過照命名規範訂，這裡是回報）。理由：Kernel 已經有 `CurrentTick` 這個參數，「現在第幾步」與「現在的時間」成對；它的意思正是「你現在所在的地方的時間」。不能叫 `Time`（和 `GYO::Time` 這個 Module 撞名）；`GameTime` 已經因為「Game 是產品的一種」退掉。比過的：`ScheduleTime`（準確但長）、`StepTime`（容易誤會成固定步專用）。你不喜歡就換，只是改名。
2. **上限的預設值**：固定步一輪最多 5 步（舊的預設）；沒有固定步時一輪最多 250 ms（Bevy 的預設）。兩個都是組裝時可以給的設定，這一批只定預設。
3. **扣整步的算術放在 `engine/time`（`AdvanceVirtual`），不放在時間的 System 裡。** 設計原本寫「扣整步是時間的 System 的事」。我把算術抽成純函式放這裡，System（T3）只負責讀寫 Resource 與呼叫它。理由：這條規則（設計的 DI 5）可以不經 Kernel 單獨測；不這樣做，它只能在 T3 用整個 World 來測。**這是我對設計的一個調整，請你確認。**
4. **`FixedTime` 裡裝著累加器**，作者經 `FixedTime` 的函式讀步長與餘數。理由：獨占的 System 不能有自己的狀態，累加的狀態一定要在 Resource 裡；放兩份（Resource 一份、累加器一份）就會有兩個來源。

---

## 4. 要改的程式

| 位置 | 內容 |
|---|---|
| `engine/time/include/engine/time/TimeTypes.hpp`（新） | `RealTime`、`VirtualTime`、`FixedTime`、`CurrentTime`、`TimeControl`、上限的設定 |
| `engine/time/include/engine/time/TickAccumulator.hpp`（新） | `TickAccumulator`、`AdvanceVirtual` |
| `engine/time/CMakeLists.txt` | 只在需要 `.cpp` 時才動；連結的東西不變 |
| `tests/common/time/TimeTypesTests.cpp`（新）、`tests/common/time/CMakeLists.txt` | 測試 |
| `docs/architecture/time.md`（英文） | 加上這幾個型別的說明與 Design Invariants（第 6 節） |

**不碰**：`MonotonicClock.hpp`、`Waiter`、`LateWakeStats`；`engine/kernel`；`engine/runtime` 的舊 `FixedTickRuntime`（gyo2 上還在不在、什麼時候刪，照搬遷清單，不在這一批）；`.github/`、`build/`。

依賴方向不變：`GYO::Time` 仍然只連 `GYO::Base` 與執行緒函式庫（`time.md` 的 Design Invariant 10 與它的守衛不動）。這幾個型別不引用 Kernel 的任何東西；把它們登記成 Resource 的是 T3 的 Module。

---

## 5. 判定（先宣告，結果出來之後不放寬）

**判定 1：行為。** `TimeTypesTests.cpp` 全部通過：

| 案例 | 驗什麼 |
|---|---|
| 累加與取走 | 加 2.5 步的量：`Add` 回 2；`TakeStep` 兩次真、第三次假；餘數是 0.5 步；`UntilNextStep` 是 0.5 步 |
| 餘數跨輪保留 | 每輪加 0.4 步，連續 10 輪：一共跑 4 步，哪幾輪跑的和手算一致 |
| 不記 tick、不丟時間 | 介面上沒有 tick；加進去的總量＝取走的步數×步長＋餘數（任意序列） |
| 扣整步 | 上限 5、這一輪會有 8 步：`AdvanceVirtual` 扣掉 3 個整步，`discarded` 增加 3 步的量，之後 `Add` 回 5，餘數和不扣時相同 |
| 守恆（設計 DI 5） | 隨機的序列（固定種子）：每一輪「`VirtualTime` 的量」＝「這一輪取走的步數×步長＋餘數的變化」，逐輪成立 |
| 和舊累加器逐點相同 | 同一串真實經過時間（含卡住 2 秒、連續小步、剛好整步）餵給新的組合與照舊邏輯寫的整數版對照實作：每一輪跑的步數相同、餘數相同、扣掉的量相同 |
| 暫停 | 暫停的每一輪量是 0、累加器不前進；取消後第一輪的量等於那一輪的真實經過時間 |
| 倍率 | 0.5 倍、2 倍、0 倍；乘完四捨五入；2 倍而且超過上限時，多的記進 `discarded` |
| 沒有固定步 | 這一輪 400 ms、上限 250 ms：量是 250 ms，`discarded` 增加 150 ms |
| `RealTime` | 第一輪的差是 0；之後是兩次取樣的差；不受暫停、倍率、上限影響 |
| 前置條件 | 負的量、步長是 0、上限是 0、倍率是負數或不是有限的數：斷言 |

**判定 2：檢出力。** 突變清單在實作前由實作者（Opus）定案，交別的模型（Fable）檢查。至少包含：`TakeStep` 不減餘數；`Add` 把餘數清掉；扣的不是整步（改成夾總量）；扣了卻不記 `discarded`；暫停時仍然前進；取消暫停後把暫停期間的量補回來；倍率乘兩次；四捨五入改成無條件捨去；沒有固定步時不設上限；`RealTime` 跟著暫停。只有預期的那個案例失敗才算抓到；建置失敗不算。

**判定 3：沒有回歸。** 本機完整 ctest 全過；三平台 L1 全過；`GYO::Time` 的連結檢查（Design Invariant 10 的守衛）照過。既有測試一個判定都不改。

沒有要放寬的守衛測試。

---

## 6. Architecture Delta

設計第 8 節已經列了七項（一次寫完）。這一批的差分：

- **邊界**：`engine/time` 原本只有時間基準、等待器、統計；之後多了「時間的型別與算術」。它仍然是只靠作業系統與 `GYO::Base` 的葉子。
- **依賴方向、Ownership**：不變。型別屬於 `engine/time`；World 裡的值由之後的 System 寫（T3），不在這一批。
- **為什麼不能更小**：只做累加器、不做型別的話，T3 會自己定一套欄位；型別就是這一批要先定的契約。

**Design Invariants**（實作時用英文加進 `docs/architecture/time.md`，附守衛）：

1. 累加器只做整數的算術：不讀時間基準、不記 tick、不丟時間。加進去的總量恆等於取走的步數乘步長加上餘數。守衛：「不記 tick、不丟時間」。
2. 超過上限的量只在 `AdvanceVirtual` 一處扣；有固定步時扣的一定是整步；扣掉的一定記進 `discarded`。守衛：「扣整步」「守恆」。
3. 這些型別不引用 Kernel。守衛：既有的連結檢查（Invariant 10）。

**Replacement Criteria**：時間的 System（T3）要正確，必須知道累加器內部的狀態（而不是經它的函式）；或 `CurrentTime` 需要第三種來源（不是可縮放的、也不是固定步的）。碰到就停下來重看型別的切法。

---

## 7. 誰做、用什麼檔位

上段（公開介面、Design Invariants）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作 | Opus／high | 純算術與測試；容易錯的是邊界（整步、四捨五入、暫停前後），測試先寫 |
| 審查 | Fable（主會話） | 和實作者不同模型 |
| 突變清單的對抗檢查 | Fable／high | 和定清單的人不同模型 |

時間的估計：實作與測試 2 到 3 小時（agent 的時間），審查與檢查 1 小時。超過 1.5 倍就停下來回報。

---

## 8. 範圍的限界

- 這份宣告是照設計與舊的 `FixedTickRuntime`（`master` 的 `engine/runtime/include/engine/runtime/FixedTickRuntime.hpp`）寫的，沒有試做。
- 「和舊累加器逐點相同」比的是整數版的對照實作，不是舊的 `double` 版本身；`double` 版有一個相對容差（步長的十億分之一），整數版沒有，所以剛好落在容差內的輸入兩者可能差一步。這個差異是刻意的（改用整數就是為了拿掉容差）。
- 倍率用浮點數乘一次再取整數：同樣的輸入在三個平台結果相同（IEEE 的乘法與四捨五入），但 `VirtualTime` 本來就是 (b) 區的資料，不進決定性的範圍。
- 這份宣告送別的模型檢查的結果，寫在交付時的訊息與 HANDOFF。

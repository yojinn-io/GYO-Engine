# System 契約

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §B（System 契約）、§E.5（例外與錯誤）。
本文件只寫**使用者已裁決**的部分；規格中仍標「待使用者」的條目不在這裡。

## 1. 描述從簽名推出，一份（mechanism §3.1 審查修正 4）

- System＝一個無狀態的可呼叫物加一份描述。描述的一半從參數型別在編譯期推出（Component 讀寫、Resource 讀寫、事件讀寫、延後佇列句柄、bundle 的延後寫），另一半登記時顯式宣告（schedule、stage、run 條件、before／after、執行緒親和、外部副作用、決定性區）。登記 API 不接受手寫的第二份存取宣告。
- 參數只能是 `ParamTraits` 特化過的句柄型別；`World&`、`T&`、裸指標、`int` 編譯失敗。句柄不可複製、不可移動、建構子私有，System 存不走它。
- spike `v8-arch-kernel-spike` 判定 1：18 種簽名推出的集合與手寫期望逐一相同（本機 AppleClang；三平台看 PR #180 的 L1）。

## 2. 回傳 `Result<void, KernelError>`（使用者 2026-11-11 決定，spec K.3 第 17 項）

- **System 的簽名回 `Result<void, KernelError>`**，不是 `void`；`static_assert` 擋其他回傳型別（spec S-2、J-10）。**這是回傳契約，不是錯誤通道**（使用者 2026-11-12 強調）：Ok 是正常路徑，每個 System 每次執行都回傳它；Err 是帶上下文的 Programmer Error。使用者接受每個 System 多一行回傳的代價。
- 以**執行期資料**才能偵測的 Programmer Error——untyped `Commands`、`Despawn`、`ExclusiveWorld` 的寫入違反區規則（Z5）或獨占（F-10、S-13），以及**存取驗證**（可關；A.6 兩層與句柄 epoch）抓到的違規——Kernel 的 API 回 `Err(KernelError)`，錯誤帶完整上下文：System 名、Plan 位置、命令種類、EntityId、ComponentId、違反的規則代號。
- System 收到錯誤後**包一層自己的上下文回傳**（Go 的 `%w` 鏈），不自己記錄、不吞掉。
- **Executor 是最外層，只處理一次**：記錄完整鏈路一次（日誌開關在 Executor 的 handler，`Base::Result` 仍是純值）、World 標 `Interrupted`、本次 `Run` 結束。release 也如此，**不靜默**。記錯當下不呼叫 AssertionHandler；現場停由 `breakOnKernelError` 掛鉤提供（§2b）。
- 這是 error-handling 契約的擴充（見 `error-handling.md`）：最終處置仍是 Programmer Error；Err 攜帶診斷鏈，不表示可恢復。Result 本身是回傳契約，不是為錯誤另開的通道。原稿的「常開檢查巨集」不採。

### 2b. 兩種機制，不多（使用者 2026-11-12 決定，`v8-arch-kernel-verification-not-assert`）

- **GYO_ASSERT**：便宜的前置條件與內部不變量，在每個建置組態都評估、失敗即 abort（現有契約，不動）。
- **Result**：執行期資料才看得出的 Programmer Error，含存取驗證抓到的。`Result<void, KernelError>` 是 System 的**回傳契約**，不是錯誤通道：Ok 是正常路徑，Err 是帶上下文的 Programmer Error。
- **一個事件只有一種性質**（使用者 2026-11-12 決定，`v8-arch-kernel-break-hook`，借鑑 Go）：一個錯誤要嘛是 GYO_ASSERT（前置條件、常開、終結），要嘛是以 Err 回傳的 Programmer Error；Err 的產生與回傳路上**不呼叫 AssertionHandler、不呼叫任何終結出口**。現場停改為啟動參數 **`breakOnKernelError`**：在 `KernelError` 建構處（記錯當下）放一個**會返回**的 break 掛鉤（預設 debugger trap，無 debugger 時無作用），返回後照常走 Err；它不是 AssertionHandler、不沿用其契約；與建置組態無關，release 二進位也帶。`breakOnKernelError` 放在 `KernelSettings`，與驗證開關獨立（驗證關而 break 開時只對便宜那類生效），吃同樣的四條件，啟動日誌多一欄。參考出處（Go）：error 值回傳＋最外層處理一次＋結束工作單元而非行程（`net/http` 的 handler、Go blog「Error handling and Go」）；panic 對應 GYO_ASSERT；Go 刻意沒有 assert、沒有 debug／release 二分（Go FAQ「Why does Go not have assertions?」）；`runtime.Breakpoint()` 包在旗標後的慣例＝本掛鉤；`GODEBUG` 執行期偵測開關的形狀＝驗證開關的四條件。不照搬的兩點：GYO_ASSERT 不可 recover（C++ 沒有 Go 的 panic 邊界保證）；GYO 的「API 誤用／執行期資料」分界比 Go 標準庫的 panic 更細，保留。
- **驗證不叫斷言**：A.6 兩層存取核對與句柄 epoch 比對的成本與 World 大小成正比，不能用 GYO_ASSERT 實作；有**執行期開關，由啟動參數決定**（程式碼永遠在；Host／runner 把啟動參數寫進 Kernel 的設定 Resource；關閉時每次存取只多一個分支；同一二進位可開著跑場次測試），開關狀態啟動時寫日誌並放成 Resource，「無違規」報告必附「驗證開著」；release 預設關、測試版預設開，皆明寫設定；關閉時只是不去查、System 仍回 Ok；關閉時這一類不被偵測（limits LIM-3）。便宜那類（untyped `Commands`、Z5、S-13）永遠查。


**啟動參數開關的四個條件與兩個限制**（使用者 2026-11-12 同意，側邊會話轉述；缺任一條件就是新的靜默來源）：

1. **啟動後凍結**：參數在 Registry 凍結前讀一次，於第一次 `Run` 前的 Ready 窗口寫進 `KernelSettings`；第一次 `Run` 之後不可改（Host 再寫是 GYO_ASSERT；邊界比對的上一次快照會對不上）。
2. **狀態進證據**：開關值與被 `ColumnInfo` 開關排除的大欄清單，出現在啟動日誌、場次記錄 header、任何「無違規」報告旁；零 Err 不等於乾淨。
3. **預設跟建置變體，參數只能覆寫**：release 預設關、測試版預設開（寫在建置變體）；啟動參數可在 release 開（場次）或在測試版關（量純效能），兩個方向都在日誌標「被參數覆寫」。
4. **關閉時零配置成本**：邊界比對的快照緩衝只在開著時配置；關閉只剩每次存取一個分支。

**「驗證開著」報告的欄位**（條件 2 的固定內容；`v8-arch-a6-revision`）：驗證開關的值與是否被參數覆寫、`breakOnKernelError` 的值、被 `ColumnInfo` 開關排除的大欄清單。若採用存取驗證第三層（頁保護；spec A.6 的條件化條文，採用與否待使用者在閘門 G2a 後決定），再加兩欄：「第三層開著」與 `reinstalls`（行程內的故障處理器被別人取代後重裝的次數；大於 0 時報告不得省略）。

**與 `field-session.md` Design Invariant 3 的關係**（澄清；`v8-arch-legacy-impact-dispositions` 衝突 3）：那一條（「Test code apart from release code… A runtime flag inside the release binary is not separation」）的主詞是場次的**收集碼**——超出產品正常記錄的高頻記錄。Kernel 的存取驗證與 `breakOnKernelError` 不是收集碼，是 Engine 的正確性檢查，程式碼永遠在；它們的執行期開關不違反那一條，`field-session.md` 不改。反過來也成立：場次的高頻記錄（每 tick、每幀）仍照那一條走編譯期變體，不得以這個開關為理由放進 release。開關的值是場次事實的一部分，進場次記錄 header（條件 2）。

限制（`limits.zh-Hant.md` LIM-6）：參數是行程級，三個 Runtime 共用同一值，無 per-Runtime 粒度（目前無需求，列為 Replacement Criteria 候選）；參數不是 (c) 資料——不影響模擬結果，只影響會不會查到違規，不進 digest、不進快照。

## 3. 無狀態與禁止的形狀（spec S-1、B.4 第 1 點）

- 可呼叫物是非成員函式（`AddSystem<&Fn>()`，非型別模板引數）或無捕獲 lambda（以型別傳入）。有捕獲的 lambda、成員函式、泛型 lambda、多載的 `operator()` 編譯失敗：捕獲與 `this` 是 World 以外的隱藏狀態與繞路通道。
- (c) 區的 System 不得有 `Local<T>`、不得宣告外部副作用（**唯一例外 `Diagnostics`**：只寫不讀回的診斷句柄 `DiagnosticsWriter<T>`，不進存取集合、不產生衝突邊；spec S-8 v0.3）、不得用變更過濾、不得持 `ExclusiveWorld`（spec Z4）。

## Design Invariants

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。本節的變更是 Architecture Delta（AGENTS §3），要使用者同意。

條號不變。標「受 kernel DI-KS n 約束」的條，全文只在 kernel 頁的那一條；其餘是本部件特有的條文。

1. **描述從簽名推出，一份**——受 kernel [DI-KS 1](kernel.zh-Hant.md#di-ks-1) 約束：**全文與守衛見該條，改動前必讀**。
2. **System 回 `Result<void, KernelError>`，不靜默**——受 kernel [DI-KS 2](kernel.zh-Hant.md#di-ks-2) 約束：**全文與守衛見該條，改動前必讀**。
3. **System 無狀態；(c) 區無 `Local`**——受 kernel [DI-KS 1](kernel.zh-Hant.md#di-ks-1)、[DI-KS 12](kernel.zh-Hant.md#di-ks-12) 約束：**全文與守衛見該條，改動前必讀**。

### Replacement Criteria

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。

- LIM-3：存取驗證（可關）攔不到的形狀，以及驗證關閉時攔不到的全部（詳細在 `limits.zh-Hant.md`）。
- LIM-6：驗證開關是行程級，三個 Runtime 共用；出現 per-Runtime 粒度的需求時置換開關的放法。
- **契約不可靠**——受 kernel [RC 1](kernel.zh-Hant.md#rc-1) 約束，改動前必讀該條；三分項與目前狀態在那一頁。

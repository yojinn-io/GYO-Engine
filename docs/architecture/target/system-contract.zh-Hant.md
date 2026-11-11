# System 契約

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §B（System 契約）、§E.5（例外與錯誤）。
本文件只寫**使用者已裁決**的部分；規格中仍標「待使用者」的條目不在這裡。

## 1. 描述從簽名推出，一份（mechanism §3.1 審查修正 4）

- System＝一個無狀態的可呼叫物加一份描述。描述的一半從參數型別在編譯期推出（Component 讀寫、Resource 讀寫、事件讀寫、延後佇列句柄、bundle 的延後寫），另一半登記時顯式宣告（schedule、stage、run 條件、before／after、執行緒親和、外部副作用、決定性區）。登記 API 不接受手寫的第二份存取宣告。
- 參數只能是 `ParamTraits` 特化過的句柄型別；`World&`、`T&`、裸指標、`int` 編譯失敗。句柄不可複製、不可移動、建構子私有，System 存不走它。
- spike `v8-arch-kernel-spike` 判定 1：18 種簽名推出的集合與手寫期望逐一相同（本機 AppleClang；三平台看 PR #180 的 L1）。

## 2. 回傳 `Result<void, KernelError>`（使用者 2026-11-11 決定，spec K.3 第 17 項）

- **System 的簽名回 `Result<void, KernelError>`**，不是 `void`；`static_assert` 擋其他回傳型別（spec S-2、J-10）。使用者接受每個 System 多一行回傳的代價。
- 以**執行期資料**才能偵測的 Programmer Error——untyped `Commands`、`Despawn`、`ExclusiveWorld` 的寫入違反區規則（Z5）或獨占（F-10、S-13），以及**存取驗證**（可關；A.6 兩層與句柄 epoch）抓到的違規——Kernel 的 API 回 `Err(KernelError)`，錯誤帶完整上下文：System 名、Plan 位置、命令種類、EntityId、ComponentId、違反的規則代號。
- System 收到錯誤後**包一層自己的上下文回傳**（Go 的 `%w` 鏈），不自己記錄、不吞掉。
- **Executor 是最外層，只處理一次**：記錄完整鏈路一次（日誌開關在 Executor 的 handler，`Base::Result` 仍是純值）、World 標 `Interrupted`、本次 `Run` 結束。release 也如此，**不靜默**。debug 建置另在記錯當下呼叫 assertion handler（現場停）。
- 這是 error-handling 契約的擴充（見 `error-handling.md`）：最終處置仍是 Programmer Error，Result 在此是診斷鏈的載具，不表示可恢復。原稿的「常開檢查巨集」不採。

### 2b. 兩種機制，不多（使用者 2026-11-12 決定，`v8-arch-kernel-verification-not-assert`）

- **GYO_ASSERT**：便宜的前置條件與內部不變量，在每個建置組態都評估、失敗即 abort（現有契約，不動）。
- **Result**：執行期資料才看得出的 Programmer Error，含存取驗證抓到的。`Result<void, KernelError>` 是 System 的**回傳契約**，不是錯誤通道：Ok 是正常路徑，Err 是帶上下文的 Programmer Error。
- **驗證不叫斷言**：A.6 兩層存取核對與句柄 epoch 比對的成本與 World 大小成正比，不能用 GYO_ASSERT 實作；有執行期開關（同一二進位可開著跑場次測試），開關狀態啟動時寫日誌並放成 Resource，「無違規」報告必附「驗證開著」；release 預設關、測試版預設開，皆明寫設定；關閉時只是不去查、System 仍回 Ok；關閉時這一類不被偵測（limits LIM-3）。便宜那類（untyped `Commands`、Z5、S-13）永遠查。

## 3. 無狀態與禁止的形狀（spec S-1、B.4 第 1 點）

- 可呼叫物是非成員函式（`AddSystem<&Fn>()`，非型別模板引數）或無捕獲 lambda（以型別傳入）。有捕獲的 lambda、成員函式、泛型 lambda、多載的 `operator()` 編譯失敗：捕獲與 `this` 是 World 以外的隱藏狀態與繞路通道。
- (c) 區的 System 不得有 `Local<T>`、不得宣告外部副作用（**唯一例外 `Diagnostics`**：只寫不讀回的診斷句柄 `DiagnosticsWriter<T>`，不進存取集合、不產生衝突邊；spec S-8 v0.3）、不得用變更過濾、不得持 `ExclusiveWorld`（spec Z4）。

## Design Invariants（候選；核准後成為正式條文）

1. **描述從簽名推出，一份**；登記 API 不接受手寫存取宣告；句柄不可複製、不可移動。守衛：spec J-10～J-12。
2. **System 回 `Result<void, KernelError>`，錯誤帶上下文、逐層包裝、Executor 只處理一次並 Interrupted，任何組態都不靜默**。守衛：spec J-10（回傳型別）、J-32b、J-45（release 目標也要看到 Err 與 Interrupted）、J-35。
3. **System 無狀態**：無捕獲、無 `this`；(c) 區無 `Local`。守衛：spec J-10、J-14。

### Replacement Criteria（候選）

- LIM-3：debug 核對攔不到的形狀（詳細在 `limits.zh-Hant.md`）。
- **契約不可靠**（spec K.2 第 1 條，使用者 2026-11-11 核准的三分項）：(a) 判定 1（簽名推存取集合）在任一 L1 編譯器不成立 → 退回「顯式宣告＋debug 核對」；(b) System 自己執行緒上經 World API 的存取在 debug 攔不到 → 核對層重寫；(c) 上線後一個版本內兩次以上「宣告與實際存取不符、兩層核對與掃描都攔不到、且不屬 LIM-3 已列形狀」→ 評估頁保護或縮小「包」的範圍。目前狀態：(a) 三平台成立（L1 run 43018979148）；(b) 成立（spike (c1)–(c5)）；(c) 尚無案例。

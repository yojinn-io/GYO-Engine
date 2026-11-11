# Runtime 的組合

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。規格來源：`docs/architecture/plans/kernel/spec.zh-Hant.md` §H（多 World 與 schedule 的組合）、§0 用語。
本文件只寫**使用者已裁決**的部分；規格中仍標「待使用者」的條目不在這裡。

## 1. 名詞（spec §0）

- **Runtime**＝一個 World、它的 Registry、它的 schedule 集合，加每個 schedule 的 Executor 策略與執行緒綁定。
- **Host**＝行程的入口程式：建立一或多個 Runtime，擁有執行緒、時鐘、外部服務與交換點。
- 一個 World 一條擁有執行緒；兩個時鐘在不同執行緒就是兩個 World，經 Host 的交換點交換（spec H-2）。

## 2. Client：Frame World 與 Render World 分開（使用者 2026-11-11 決定，spec K.3 第 6 項）

- Client 有三個 Runtime：`ClientSim`（World S，FixedUpdate，runner＝`gyo-client-sim`）、`ClientFrame`（World F，FrameUpdate，runner＝Main）、`ClientRender`（World R，`Extract` → `RenderSubmit`，與 F 同在主執行緒）。
- **Render World 自己的 Registry**（各自凍結；render plugin 只登記到渲染 Registry，主 Registry 不因渲染而變 fingerprint）。
- **渲染只經 Extract 以 const 方式讀來源 World，無反饋**：extract System 的參數 `Extract<Read<T>>`／`ExtractRes<R>` 只讀來源；`Extract<Write<…>>` 不存在（編譯期擋下）；debug 下來源 World 在 extract 期間任何寫入都斷言；渲染 Runtime 不得以任何參數寫回來源，含 boundary Resource；來源 Registry 必須先凍結（spec H-6、J-43；DI-KS 13）。
- 代價：多一個 World 與每幀一次 extract。使用者的理由：合起來看著簡單，代價很高——合併後「渲染不改模擬」只能靠 RenderSubmit 唯讀的存取宣告守，而宣告的真實性在 C++ 攔不到（LIM-3）；分開則由機制守。
- 交換點：意圖 F → S、呈現 S → F 經 Host 擁有的 Mailbox，只在 schedule 之間存取；Link 經 Host 與網路執行緒在 S 的 schedule 前後交換。

## 3. Match：Join／Leave 在 tick 內（使用者 2026-11-11 決定，spec K.3 第 10 項）

- Match 的 FixedUpdate：`Ingress`（zone=B，Z3）把 Link 收到的加入／離開請求、ack、動作、輸入、到達時刻全部以資料寫進 `InputRing[tick]`，**不生成、不刪除實體**；`Simulate`（zone=C）的 `ApplyMembership` 讀 ring 的請求，以 `SpawnWithId<Bundle<…>>`（id 由 Gateway 配，E-2）生成、以 `Despawn` 刪除（它是 (c)，Z5 允許）。
- 不為 Host 開 Z2／Z5 的例外：Host 在 World Ready 時只寫 boundary 與 external Resource，不直接生成或刪除帶 (c) 欄的實體。
- 後果記為限制 LIM-5（`limits.zh-Hant.md`）：加入與離開晚最多一個 tick；golden 可能差一 tick、要重設基準；產品線在遷移批確認。entity-model §5.2「Join／Leave 在 Tick 外」改寫，排進改造的第一個文件批。

## 4. framework 退役；組合與第三方對接由 plugins 層承擔（使用者 2026-11-11 決定，audit §3.1）

- **三層改為**：Engine（Kernel＋能力）／**plugins**（`plugins/`：組合與第三方對接，每個 plugin 可選可刪）／產品與工具（plugin 清單＋Run）。層與目錄叫 plugins，單個才叫 plugin（使用者 2026-11-11）。framework 是舊架構的產物，其兩個核心職責（組合 Engine 能力、對接第三方）在 plugin 機制下都由 plugin 承擔，概念淘汰；`docs/framework/` 的 Design Invariants 整節廢止（第一個文件批）。
- **runner plugin**：Editor host 的骨架（行程入口、pump、交換點）是一個在 finish 安裝 runner 的 plugin（照 Bevy `WinitPlugin::set_runner`）；一個 Runtime 只能有一個 runner（spec R-E6）；沒有 plugin 安裝時由 Host 程式自己跑。
- **標準 Editor stage 序**＝plugin 群在 build 插入 stage（spec C.2 第 2 點）；ImGui、SDL 的相容在各自的 plugin 內實現，全域 context 明列例外。
- **`plugins/` 是新的 top-level**（AGENTS §10 Delta，已同意）：每個 plugin 一個 target、自己的依賴正向清單；Engine 永不 include 它；產品與工具只在 main 列它。
- 「兩個以上產品以同一方式組合者先進 framework」→「先成為共用 plugin」。架構師保留：四個工具重複的選擇窗記帳與關窗提示必須真的進共用 plugin，否則是 framework 的老問題換名。
- 組合格式（catalog plan、event sounds）是資料契約，owner 是提供該 plugin 的人；純函式（EventSounds、CatalogPlan、PreviewCamera、UiTextInput、json binding）是函式庫，與層無關。

## Design Invariants（候選；核准後成為正式條文）

1. **渲染只經 Extract 以 const 方式讀來源 World，無反饋**；渲染 World 有自己的 Registry；來源 Registry 先凍結。守衛：spec J-43（含編譯失敗測試 `Extract<Write>`）、J-16。
2. **一個 Runtime 只有一個 runner，由 plugin 在 finish 安裝或 Host 自跑**；Engine 不 include 任何 plugin；產品與工具只在 main 列 plugin。守衛：spec R-E6；依賴方向掃描（`engine/` 不 include `plugins/`）。
3. **一個 World 一條執行緒**：同一 World 的所有 schedule 綁同一執行緒；不同執行緒上的時鐘是不同的 World，只經 Host 交換點交換。守衛：spec J-15、J-6；finish 檢查 H-2。

### Replacement Criteria（候選）

- LIM-5：Join／Leave 在 tick 內，加入與離開晚最多一 tick；不解除（詳細在 `limits.zh-Hant.md`）。
- 渲染需要寫回模擬 World（例如渲染驅動的 LOD 狀態要進判定）→ 不是放寬 DI 1，而是把那份狀態搬到來源 World 由模擬側的 System 寫，渲染仍只讀。
- 第一個真實能力接進機制時仍要改 Host 迴圈或 Kernel 本體，或 Host 的協調邏輯在兩個以上 host 重複 → 「Host 又長回來」，停下（spec K.2 第 7 條）。

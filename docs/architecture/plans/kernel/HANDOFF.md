# Kernel 研究線：交接

## 現況

- 2026-11-10 使用者提出願景稿並要求架構師分析；討論定案：Kernel 應進 Engine（中性），framework 強而 Engine 弱的味道＝framework 在代替不存在的 Kernel；Match 的 Stage 是固定序列；C++ 以顯式存取宣告代替借用檢查；協定先設計、實作薄、先遷移一個 host 量成本。
- 2026-11-10 三個只讀子代理（Opus）審閱 bevy_app／bevy_ecs、bevy_replicon 0.45.5、lightyear 0.31.0；報告在 `build/evidence/kernel-study/`。結論：八條不變量；Bevy 的實作選擇（插入順序平手、ambiguity 預設關、多執行緒 executor、Observer 依雜湊順序、ComponentId 依首次使用、並行 entity allocator、變更 tick≠遊戲 tick）不是設計思想，照抄會在核心埋債。
- 2026-11-10 網路設計稿（networking.zh-Hant.md）；五個子代理看渲染、資產、平台、能力 plugin、OS／IO；blueprint.zh-Hant.md 寫成（DI-K1～K7 候選）。
- 2026-11-11 **架構師的誤讀與更正**：架構師默認「改造現有 GYO」並畫了產品掛接圖，沒把「GYO2 還是改造」擺成判斷；被糾正後又當成「決定了 GYO2」。使用者澄清：要的是 Bevy 的抽象能力帶入 GYO，改現有不是 GYO2，判斷的是中間層逐模組包或重寫；改造完成前功能線停。使用者說「先不定案」，之後只對理解。
- 2026-11-11 使用者提供「GYO 架構設計思想」v0.2；架構師依理解畫三張圖（Runtime 機制、網路作為 System、控制面修訂）；使用者提供審查（4 修正 2 補充：多寫者非錯誤、衝突邊≠順序邊、決定性≠序列化、契約分兩半、World 懂儲存契約、邊界措辭與事件時機）；全部採納並加架構師兩條主張（(c) 區歧義＝啟動錯誤；(c) 區並行只在有逐位元守衛時）。寫成 mechanism.zh-Hant.md，blueprint 降為輸入。
- 2026-11-11 決定性等級卡完成（PR #177）：一條邊界六區，不是全部一級；預測核心今天也不是 (c)（win1 2 步 ULP 差異，原因未明，E0 偵測批）。mechanism §3.3 引用。

## 觀測到的壓力（AGENTS §2 的根據；使用者本人的觀測，2026-11-11，側邊會話轉述）

使用者原話：

> 雖然工作量爆炸，但是我卻沒有之前 GYO 那種改不動的感覺。第一次改不動是 10/10 的事故，第二次還沒碰到，但是我感覺到架構組織其實有壞味道，我沒有辦法使用統一的邏輯來執行工作。

架構師的對應：-audit 量到的「重寫集中在組織層、每個 host 各自一套手排順序、四個工具 client 各自一套、framework 在中間補」（audit §1.4、§3.1、framework-tools 觀察 3）就是這個壞味道的量化。改造的收斂方向是**每遷一個 host 少一套規矩**，與 v7.1 事故後「在舊形狀上加特例」相反。

## 決定（使用者）

| 代號 | 內容 |
|---|---|
| `v8-arch-kernel-foundation-first` | 2026-11-10。基盤的協定不等壓力、先設計；功能層才等壓力。寫進 AGENTS §2 的修訂待風紀委員處理（使用者同意後）。 |
| `v8-arch-kernel-bring-into-gyo` | 2026-11-11。要的是 Bevy 的抽象能力（World／System Contract／Scheduler／Executor）帶入現有 GYO，不是 GYO2；架構改造完成前功能線停。 |
| `v8-arch-kernel-pause-mode` | 2026-11-11。停功能線的定義＝(a)：已開工的卡做到下一個關卡停、更新 HANDOFF；不開新功能卡。事故修正是否繼續仍未定。 |
| `v8-arch-kernel-delegation` | 2026-11-11。此次架構調整（含 spike）由架構師全權負責：各批自開自跑、每批關卡回報；本線 docs-only PR 在 CI 綠後架構師直接合併並回報；碰 Design Invariants、`spikes/` 以外的程式、Delta 七項仍先給使用者。使用者封存原 spike 卡（task_8f7c5a80），spike 改由架構師會話的子代理執行。 |
| `v8-arch-kernel-control` | 2026-11-11。使用者：GYO 最大的架構調整，**會話的控制權在主會話**（架構師會話），不交給別的會話或任務卡，避免會話間傳遞造成語意偏差；子代理可以開（使用者澄清「不開並行」指的是控制權，不是禁止子代理）。架構師：各批由主會話開子代理執行、主會話收結果並逐項讀過；原核准的「規格與 spike 同時開」照舊。 |
| `v8-arch-kernel-spike-verdict` | 2026-11-11（側邊會話轉述）。spike 判定 2 照事前宣告原文記「成立」；實作者多發現的攔不到形狀（寫了又寫回、經存下參考只讀、堆積內容、全域狀態、host 碰過同欄、其他執行緒）全部記為**範囲の限界**（spike-report 專節 L1～L8），後續批次的事前宣告必須寫上，直到解除。 |
| `v8-arch-kernel-limits-accept` | 2026-11-11（側邊會話轉述）。上限的處置：接受；World 以外的資料靠 code review（mechanism §3.5 三層）；不改計畫前提。-audit 的第二問改為兩個可檢查的問題：有沒有跨 tick 的指標或參考、有沒有全域可變狀態。 |
| `v8-arch-kernel-spike-pageguard` | 2026-11-11（側邊會話轉述）。新增批 `-spike-pageguard`：debug 版頁保護（mprotect／VirtualProtect）；形狀已定：只對寫設保護、讀放行；只保護標「獨占寫入」的欄；只在 (c) 區 System 執行時開；原文「只在 debug」，B-9 結案（`v8-arch-kernel-verification-not-assert`）後歸「存取驗證（可關）」：隨同一個啟動參數開關，頁錯誤處理器把違規記進本次呼叫的 context、System 以 Err 回傳，不是 GYO_ASSERT。排在 -spec 之後（欄與 System 的標記要照規格的契約欄位寫）；開工前事前宣告。 |
| `v8-arch-kernel-entity-key` | 2026-11-11（側邊會話轉述，spec K.3 第 1 項）。EntityId 格式＝u64，低 32 index、高 32 generation（Bevy 格式）；Kernel 不看兩段意義，疊代依整數值升冪；配發策略是產品的 Resource，PvP 選單調不重用、generation 恆 0（值與今天逐位元相同，wire／digest 不改）；stale 句柄偵測能力取決於策略。spec A.1、A.8、K.1 第 3／4 條已改，K.2 第 2 條取消；部件文件 `docs/architecture/target/world.zh-Hant.md`。 |
| `v8-arch-kernel-change-stamp` | 2026-11-11（同上，K.3 第 2 項）。接受規格：依 Plan 位置取值的單調 u64 stamp（不重置、不回滾、跨世代不比較）；tick 視角以 tickStamps 換算；(c) 區禁變更過濾；blueprint DI-K5 與 networking §1 第 7 條各改一句。 |
| `v8-arch-docs-next-folder` | 2026-11-11（同上）。新版架構文件與舊版分開、至少分資料夾、不混放：現況留 `docs/architecture/`，目標架構放 `docs/architecture/target/`（圖書管理員 2026-11-11 定：平鋪、檔名＝搬回後的檔名、英文 Status 行）；每份標「目標架構，尚未實作」或「已實作於哪批」；部件遷移完成後搬回、舊的刪或標廢止，不並存；計畫／HANDOFF／dev_log 留 plans/kernel。 |
| `v8-arch-kernel-exclusive-default` | 2026-11-11（側邊會話轉述，spec K.3 第 3 項）。欄與 Resource 預設獨占，登記標 Shared 允許多寫者；進 digest 禁 Shared。獨占＝啟動時數寫者、執行期零成本、非鎖；獨占欄預設由唯一寫者直接寫；多來源影響同一欄時才用漏斗（事件或中間欄，可選；使用者更正：不是標準進料方式）；(c) 區多個 EventWriter 仍要顯式順序；整欄替換只用於 Client 收權威狀態。不採資料庫式執行期仲裁（慢 20～100 倍、買不到決定性）。spec B.3.1 新增含三類資料的表。 |
| `v8-arch-kernel-despawn-not-writer` | 2026-11-11（側邊會話轉述，spec K.3 第 4 項）。採規格：Despawn 不算獨占寫者、不產生衝突邊；(c) 由 Z5 保護；F.2 套用順序寫死（hook 佇列 → despawn → remove → spawn → insert → OnAdd → resource），despawn 依 id 升冪、冪等、對窗開始不存在的 id 無作用；despawn 先於 spawn 為同窗同 id 重連。案例（玩家 10 死亡又退出、玩家 7 擊殺成立）寫進 target/world §4。 |
| `v8-arch-kernel-b-reads-c-ordered` | 2026-11-11（側邊會話轉述，spec K.3 第 5 項）。採規格：讀 (c) 的 (b) System 必須與 (c) 寫者有顯式順序，否則 D-E8；慣例「呈現放模擬之後的 stage」記為限制 LIM-1（成本在呈現 System 數量，產品線遷移時確認；修改條件 K.2 第 3 條）。使用者另要求新架構文件有「限制與修改條件」集中頁 → `target/limits.zh-Hant.md`（LIM-1～LIM-4；位置待圖書管理員）。 |
| `v8-arch-kernel-render-world-separate` | 2026-11-11（側邊會話轉述，spec K.3 第 6 項）。Client 的 Frame World 與 Render World 分開；渲染 World 自己的 Registry、只經 Extract 唯讀來源、無反饋由機制守（H-6、J-43、DI-KS 13）。理由：合起來看著簡單、代價很高。部件文件 target/runtime-composition.zh-Hant.md。 |
| `v8-arch-kernel-delta-approved` | 2026-11-11（側邊會話轉述，spec K.3 第 7 項）。使用者同意 Architecture Delta 三處：architecture.md:136 禁令改為「Kernel 進 Engine（World／System／Scheduler／Executor）；plugin ABI 與動態載入仍禁」；entity-store DI 4 延伸到 insert／remove、DI 5 仍成立、DeferredChanges（3b）撤回；determinism.md §4.7 正式化。七項以 spec／mechanism 為計畫層一次書寫、各批引用。文件修改排進改造的第一個文件批。 |
| `v8-arch-remove-object-fps` | 2026-11-11（側邊會話轉述，spec K.3 第 8 項）。移除 apps/object_fps、apps/object_fps_v2 與 tools/object_fps_preview 的依賴，照 AGENTS §7 完整刪除、不留相容層；理由：PvP 已有自己的生態。排成改造前的獨立批，兼驗 §7 removability（刪後共通層需要改就記 Delta）。待事前宣告。 |
| `v8-arch-kernel-writer-unit-system` | 2026-11-11（側邊會話轉述，spec K.3 第 9 項）。獨占寫者以 System 計、Startup 不計；PvP 的 Body 依 entity-model 第 139 行分類拆欄，不併 System、不改 Shared、不採以 owner plugin 計。使用者：不拆的話架構改造就沒有意義。拆欄範圍與 golden 影響由 -audit 量。 |
| `v8-arch-kernel-membership-in-tick` | 2026-11-11（側邊會話轉述，spec K.3 第 10 項）。採規格：Join／Leave 進 tick 內，Ingress 只寫請求進暫存（v0.3 N-8 起是 `MembershipRequests`，原記 InputRing），(c) 的 ApplyMembership 以 SpawnWithId／Despawn 生成與刪除；不為 Host 開 Z2／Z5 例外。後果（晚最多一 tick、golden 如 combat-rejoin 可能差一 tick 需重設基準）記為 LIM-5，產品線在遷移批確認；entity-model §5.2 改寫排進第一個文件批。 |
| `v8-arch-kernel-time-sources` | 2026-11-11（側邊會話轉述，spec K.3 第 11 項，B6 結案）。(c) 區的時間只來自 World 的 domain tick 與被記錄的 boundary 輸入（HostClockSample、隨輸入進 ring 的到達時刻）；(c) System 自己讀時鐘仍禁止。mechanism §3.3 Executor 列已改（含圖）；determinism.md DI 4 不動；規則寫進 target/determinism.md。 |
| `v8-arch-kernel-k3-12-14-15` | 2026-11-11（側邊會話轉述）。第 12 項：同步點套用順序照 F.2（與第 4 項一起）；第 14 項：v1 任何 Parallel 宣告＝D-E17，含只有 (b) 的 schedule（LIM-2）；第 15 項：事件只在該 domain 的 clock schedule 內讀寫（F-9）。 |
| `v8-arch-kernel-result-errors` | 2026-11-11（側邊會話轉述，spec K.3 第 17 項，有變更）。不採常開檢查巨集，改 Go 式錯誤處理（原則：不靜默）：untyped 路徑回 Result<void, KernelError> 帶完整上下文；System 簽名改回 Result<void, KernelError>，包一層上下文回傳、不自己記錄；Executor 最外層只處理一次（記錄完整鏈一次、World Interrupted；日誌開關在 Executor 的 handler，Base::Result 仍純值）；release 也如此；~~debug 另在記錯當下呼叫 assertion handler~~（2026-11-12 刪，見 `v8-arch-kernel-break-hook`）。error-handling.md 加一列（Delta 已同意，排第一個文件批）。使用者接受 System 多一行回傳。部件文件 target/system-contract.zh-Hant.md、target/error-handling.md。 |
| `v8-arch-kernel-rc1-rewrite` | 2026-11-11（側邊會話轉述，spec K.3 第 13 項）。使用者核准 RC 第 1 條改寫為三分項（(a) 判定 1 不成立→退回顯式宣告；(b) 經 World 的存取 debug 攔不到→核對層重寫；(c) 上線後兩次以上不屬範囲の限界的不符→頁保護或縮小「包」）。**K.3 全部 17 項定完**；對照表在 target/README。使用者：接下來照計畫，規格與文件更新 → -audit（事前宣告列 L1～L8）→ -spike-pageguard。 |
| `v8-arch-kernel-audit-third-party` | 2026-11-11（側邊會話轉述）。第三方庫的開源方向在新架構下重核：併進 -audit（third_party/ 每庫一列，I.3 四問判葉子／包／服務，附親和與 zone；結論回寫 #176 判定總表標「依 spec v0.2 重核」）；World 儲存層借 EnTT 或自寫不在 audit 做，等 World 實作批用 W-1～W-11 核對；#176 D2 等 DET-1。 |
| `v8-arch-kernel-determinism-recheck` | 2026-11-11（側邊會話轉述）。決定性分級契約在新架構下成立，不另開核對卡；四處小改照第一個文件批。-audit 產出加一節：逐模組 zone 判定回對 determinism.md §4 的區表，列漏的模組與標錯的區，交使用者，不改契約。 |
| `v8-arch-framework-retire` | 2026-11-11（側邊會話轉述，audit §3.1）。framework 退役，不改 DI 2／6：組合與第三方對接由 plugin 承擔；Editor host 骨架＝提供 runner 的 plugin（finish 安裝，一 Runtime 一個，R-E6）；標準 Editor stage＝plugin 在 build 插入；新 top-level `plugins/`（§10 Delta 已同意；Engine 永不 include、產品工具只在 main 列）；三層改為 Engine／plugins／產品與工具；framework DI 整節廢止、docs/framework/ 標廢止；spec L.2、H.4.3、target/runtime-composition 改寫。使用者原則：思想不變，修改就是在破船上打補丁。架構師保留：選擇窗記帳與關窗提示必須進共用 plugin。 |
| `v8-arch-kernel-verification-not-assert` | 2026-11-12（側邊會話轉述，spec-v03 停止報告 B-9 結案）。GYO_ASSERT 不動（常開、abort；Kernel 的便宜前置條件照用）。A.6 兩層存取核對與句柄 epoch 比對**不叫斷言，叫「驗證」（可關）**，成本與 World 大小成正比；驗證抓到的違規以 Err 作為 System 的回傳值（第 17 項；記進本次呼叫的 SystemContext——描述不可變，第二輪 N-6——跳板回 Err 帶 System 名／Plan 位置／id／模式／規則代號、Executor 記一次、World Interrupted；~~debug 另在記錯當下呼叫 assertion handler~~ 2026-11-12 刪，見 `v8-arch-kernel-break-hook`）。機制只剩兩種：GYO_ASSERT 與 Result。用詞：`Result<void, KernelError>` 是 System 的回傳契約（Ok 正常、Err 帶上下文的 Programmer Error）；驗證關掉時仍回 Ok；便宜那類（untyped Commands、Z5、S-13）永遠查。開關：**使用者定為啟動參數的執行期開關**（不是編譯期；程式碼永遠在，Host／runner 寫進 Kernel 設定 Resource，關閉時每次存取只多一個分支；A.6 兩層與句柄 epoch 都掛它）；開關狀態啟動時寫日誌並放成 Resource，「無違規」報告必附「驗證開著」；release 預設關、測試版預設開，皆明寫設定；關閉時此類不被偵測列入 LIM-3。使用者：「嗯，就是這個」。第 17 項理由改為「相對 GYO_ASSERT 的 abort，給出上下文鏈與 Interrupted」。 2026-11-12 追加（側邊會話轉述，使用者同意）：開關的四個條件（啟動後凍結、狀態進證據、預設跟建置變體且參數只能覆寫、關閉時零配置成本）與兩個限制（行程級共用、不是 (c) 資料），寫進 spec A.6、target/system-contract §2b、limits LIM-6。 |
| `v8-arch-kernel-break-hook` | 2026-11-12（側邊會話轉述，第三輪 B-8 結案）。刪第 17 項「debug 另在記錯當下呼叫 assertion handler」。規則**一個事件只有一種性質**：GYO_ASSERT（終結）或以 Err 回傳的 Programmer Error，Err 的產生與回傳路上不呼叫 AssertionHandler、不呼叫終結出口。現場停改為啟動參數 `breakOnKernelError`（KernelSettings 的第二個參數，與驗證開關獨立、吃同樣四條件）：KernelError 建構處一個會返回的 break 掛鉤，返回後照常走 Err；不綁建置組態。參考出處（Go：error 值回傳＋最外層處理一次、panic＝GYO_ASSERT、無 assert 無 debug／release 二分、`runtime.Breakpoint()` 包旗標、`GODEBUG`）與不照搬的兩點寫進 spec §讀法／K.3 第 17 項、target/system-contract §2b、target/error-handling、error-handling.md。 |
| `v8-arch-kernel-pending-paused` | 2026-11-12（使用者，主會話）。HANDOFF 未定的三項（lag win1 修正是否繼續、網路帳本位置與兩通道分流、DET-0～3）**暫停**：在新架構下是否成立都還不知道，改造完成前不討論、不排程；移到「暫停」節。 |
| `v8-arch-parallel-lines` | 2026-11-12（使用者，主會話）。重構工作不全壓在架構師線：先分出兩條並行線——**J-17 掃描器線**（交接卡 `j17-scanner.zh-Hant.md`；掃描規則與清單格式 v1、ctest 自測與突變、pvp 基線報告；清單格式由架構師核定）與**圖書管理員**（交接 `docs/checkup/documentation/2026_11_12_handoff_from_architect.zh-Hant.md`：逾期健檢、Object_FPS 字樣、跨元件規則登記頁、新舊文件分隔核對、dev_log 缺漏）。核心修訂（spec、kernel 本體）仍在架構師線；pvp「位置＝區」整理等掃描器有基線後再開。 |
| `v8-arch-kernel-pageguard-reading` | 2026-11-12（使用者，主會話）。`-spike-pageguard` 判定 1 採**讀法 (i) 照原文：不成立**（宣告列了 (b3) 而範圍排除 Resource，是宣告自相矛盾；不事後改判定文字）。頁保護暫不進 A.6，LIM-3 的 L1／L6 不標 Lifted；要用它得在遷移批重新宣告、重跑同組測試。對 A.6 的建議照報告：L1 可由頁保護補（Exclusive 欄、(c) System、開關開），L6 不能。L1 綠後合併 #184。 |
| `v8-arch-determinism-table-fix` | 2026-11-11（側邊會話轉述）。區表三漏一錯採 (a)；條目照歸屬命名；PlayerAnimation 推進先由決定性線核對。（原記在現況區，依對抗檢查 S-10 補進決定表） |
| `v8-arch-loading-schedule` | 2026-11-11（側邊會話轉述）。Loading schedule 契約七點（見現況區 2026-11-11 條目）。 |
| `v8-arch-naming` | 2026-11-11（側邊會話轉述）。命名規範五條（target/naming）。 |
| `v8-arch-kernel-first-batches` | 2026-11-11。規格（`v8-arch-kernel-spec`，ultracode）、spike（`v8-arch-kernel-spike`）、審查（`v8-arch-kernel-audit`）同時開，audit 等 spec；全部由主會話控制（`v8-arch-kernel-control`）。模型／檔位見 README。 |

## 未定

- AGENTS §2／§3 條文：風紀委員已在分支 `claude/judgment-agents-foundation-pressure` 起草（使用者 11-10 直接指示）；架構師 11-11 轉交的三點追加（實作薄、先一個 host 量成本；§3 七項計畫層寫一次；跨產品能力當 Feature）待使用者同意。

## 暫停（使用者 2026-11-12：在新架構下是否成立都還不知道，改造完成前不討論、不排程）

- 改造期間事故修正（lag win1）是否繼續。
- 網路：帳本放 Match 還是 Gateway；兩通道分流採不採。
- 決定性 DET-0～DET-3（determinism.md §0）。

## 現況（續）

- 2026-11-11 PR #175、#177、#176、#174 依序合併（#176 合併前加註：ECS 列待 mechanism 規格後重判、D1～D4／D7／D8 延後；determinism.md E4 由 mechanism 吸收、DET-3 時機改 (c) 區遷移前、§4.7 指向 World）。
- 2026-11-11 使用者：「後續我們直接先處理架構問題」→ 定停法 (a)、規格與 spike 同時開。本分支 `claude/arch-kernel-mechanism` 承接規格的產出；spike 在獨立卡的工作樹。

- 2026-11-11 停法 (a) 通知後各線回報：產品線無進行中實作（#167、#168 已合併；#173 待使用者）；工具線無進行中卡（PathDialogRequests 交接未開工，維持）；voxel 線停於 ve-free-camera（#171）。三線 HANDOFF 的停線記錄各自在下次變更時補，不單開 PR。spike 卡 task_8f7c5a80 已掛出；規格 ultracode 背景執行中。

- 2026-11-11 spike 完成並經 Fable／xhigh 對抗審查（R-01～R-14，全部處置或記錄）；三輪突變 32→34 條全殺；分支 `claude/arch-kernel-spike`（5b473017 起）未開 PR。判定 1 待 L1（L1 不建 spikes/，改 workflow 待使用者 R-06）。使用者在側邊會話逐項對審查，R-01 與上限處置已定，R-06 之後待續。

- 2026-11-11 `-spec` 完成：ultracode（提案 Opus×3／high、統合 Fable／high、對抗 Opus／xhigh、修訂 Fable）產出 `spec.zh-Hant.md` 1319 行；對抗檢查 34 條（7 blocker）全部處置，B6（時間來源的契約衝突）待使用者；架構師逐節讀過，判讀見會話回報。K.3 列 17 項待使用者決定（第 16 項已由 R-01 決定）。PR 待開。

- 2026-11-11 `-audit` 事前宣告寫成（audit-declaration.zh-Hant.md）；使用者：「寫好事前宣告後先等等，我有需要確認的事項」→ 未啟動；PR #179 也暫不合併，等使用者。

- 2026-11-11 使用者「開始」→ `-audit` 啟動。派工：審查者 Opus／high ×3（① engine；② framework＋tools；③ object_fps_pvp＋third_party，含 (a)～(d) 四節），各在獨立 worktree 寫 `audit-part-<範圍>.zh-Hant.md`；掃描腳本 `scripts/audit_scan.py` 由主會話先寫（全域一跑：44 模組、645 命中，`build/evidence/kernel-audit/scan-full/`）；對抗複核 Fable／high 等三份合併後抽 5 個模組；(e) 決定性區表回對由主會話合併時做。

- 2026-11-11 -audit 第一份（framework-tools，commit 33f4d402 在 agent worktree）回來：件数一致（8＝7＋1）。發現事前宣告的判法「Q1 為是→組織層」比 spec I.3 粗；主會話採 I.3 的讀法（Q1 只排除留；組織層＝排序其他元件）、修正宣告、通知另兩個審查者對齊。另記：WindowShell／ImGuiShell 要重寫骨架會改 framework DI 2／DI 6（K.3 第 7 項未列的 Delta，待使用者）；Editor 預覽 readback 回 ImGui::Image 與 H-6 衝突（候選：ImGui System 全放文件 World、影像經 Host 交換點，預覽晚一幀；交工具線與使用者）；四工具 host 骨架重複（framework 標準 plugin 候選）。

- 2026-11-11 -audit 第二份（engine，commit 77201f03 在 agent worktree）回來：件数一致（22＝22＋0）；判定 留 8／包 5／Host 5／服務 2／重寫 1／不適用 1。採 P（被動）／A（主動）讀法，與主會話的對齊規則一致。與 spec L.1 不同處：input 後端→Host；platform→Host；FixedTickRuntime→Host；IRuntimePort→刪除候選（待使用者）；ui 加 UiRenderer 局部重寫（跨 World 指標；AssetManager 屬 Frame 還是 Render World 未定）；asset 包但要先收攏五個協作者的參考；model 的 ufbx loader 是 (c)。掃描漏兩類：經別名讀時鐘（Time::Now／Clock::now）、匿名命名空間非 const 變數——不改共用腳本（已引用的 CSV 列號不能變），記為 J-17 掃描清單的修正項。兩份部分報告已複製進本分支。
- 2026-11-11 engine 份依對齊後的判法重核（commit e185c864）：只動一列，input Host→包（先拆：SDL 全域查詢與 platform 指標移到 Host 的 pump，取樣結果進 boundary Resource）；render、text 改寫為「包（先收攏）」；os／time／threads／platform／FixedTickRuntime 仍 Host（依 spec §L「只持 OS 資源、只由 Host 使用」與 G-6）。人工補看三類：engine 無 ImGui；pimpl 跨幀裸指標 26 筆，指向他元件的 4 處（Renderer.cpp:134-135、SdlGpuRenderDevice.cpp:117、UiRenderer.cpp:147-149、ModelRenderer.cpp:22）；跨幀捕獲 this 的只在產品 PvpApplication.cpp:1389。新分佈：留 8／包 6／Host 4／服務 2／重寫 1／不適用 1。
- 主會話待決／待交使用者（合併報告時列出）：(1) framework DI 2／DI 6 的 Delta（WindowShell／ImGuiShell 重寫骨架；K.3 第 7 項未列）；(2) Editor 預覽 readback 回 ImGui 與 H-6 的衝突，候選「ImGui System 全在文件 World、影像經 Host 交換點、預覽晚一幀」（交工具線與使用者）；(3) IRuntimePort 刪除；(4) AssetManager 屬 Frame 或 Render World。

- 2026-11-11 第三份（product-thirdparty，commit d8b45012）回來：件数一致（14＝14＋0；另未審 3、事實列 5）；子列重寫 6；第三方無改判、§2.1 收窄四處；(a) Body 拆欄後 Life 仍兩寫者且同 tick 讀後寫（無結論，交遷移批）；(c) G.4 第 1 條應改 HostClockSample、補第 4（RequestMovementResync）、第 5（IPC 讀權威）；規格衝突：S-8／Z4 使 (c) 不能輸出診斷。主會話合併成 audit.zh-Hant.md（§1 判定、§3 跨部件、§4.5 區表回對：漏 3 標錯候選 1、§6 規格修正七項、§7 判定 1／2／4／5 成立、3 進行中）；對抗複核 Fable／high 抽 5 模組進行中。

- 2026-11-11 -audit **完成**：對抗複核 Fable／high 4/5 一致（engine/input 後端不一致→主會話重判 Host）；判法五處模糊的定義補進 audit §0；dev_log 寫成；PR 待開。

- 2026-11-11 使用者問前提改變要不要重跑審查；架構師：不重跑（四問與判定詞不因去處而變），寫補遺 audit §10（去處改成 plugins 層、P11／S1 形狀改為 runner plugin、§0 敘述修正、plugin 邊界這個新問題交計畫批並附切法候選十個）。命名：層與目錄叫 plugins、單個叫 plugin（使用者）。

- 2026-11-11（側邊會話轉述，audit 待決第 2 項）`v8-arch-editor-forward-only`：Editor 預覽不是影像回 UI，資料只往前流（照 bevy_egui）：文件 World 出場景＋ImGui draw list → Extract → 渲染 World 合成（場景到 render target、再畫 UI，預覽視窗＝貼圖 id）；H-6 原樣、無例外、無晚一幀；選取用 CPU 射線；Editor 與 Client 同形；ImGui 與 SDL 一樣是 plugin、拆兩半。審查的三個選項全部不採（使用者原則：不用舊思路處理新架構）。audit §3.2／§10 改寫、runtime-composition §3b、spec H.4.3 註。

- 2026-11-11（側邊會話轉述，audit 待決第 3 項）`v8-arch-remove-runtimeport-animinstance`：刪除 engine/runtime 的 IRuntimePort 與 engine/model 的 AnimationInstance，含 RuntimePortTests.cpp 與 ModelTests.cpp 的對應部分；併入 `v8-arch-remove-object-fps` 批做 §7 完整清理。取樣函式與 ufbx loader 不動。使用者：不光概念不同，連使用者都不存在了。

- 2026-11-11（側邊會話轉述，audit 待決第 4 項）`v8-arch-assets-bevy-model`：AssetManager 不選 World，照 Bevy 拆開——請求狀態 Resource、每型別 Assets<T>（Handle 引用計數）在文件／Frame World；loader 由 plugin 登記（ufbx 是函式庫）；載入是外部服務（IO 執行緒＋交換點，與 Link 同形）、完成依請求順序在同步點套用；內容身分 (c)、Startup 全載完才凍結；GPU 資源是渲染 World 的 Resource，Extract 搬變更。AssetManager 物件消失；UiRenderer／WeaponViewModel 跨 World 指標問題消失。使用者另要求命名規範（待定）：這類名字遷移時都要改。部件文件 target/assets.zh-Hant.md。

- 2026-11-11（側邊會話轉述，audit 待決第 5 項）`v8-arch-loading-schedule`：初始成員不走 Startup；新增 Loading schedule 契約（Startup 只放不依內容的登記與固定 Resource；Loading stage 固定序地圖→場景→可互動物件→敵人→玩家，可分批由 Host 驅動；玩家在「玩家」stage 由 ApplyMembership 以 SpawnWithId 生成；對局 tick 於 ready 後從 0 起算；Client 收到 ready 前停 loading，ready 是 wire 事件→pv10-requirements；內容身分 (c)、載入順序是 Plan 的函數）。Feature＝loading 頁面與分批載入。golden 34 個以對局開始為基準預期不變——遷移批實測。spec H.2b、D.1 domain 加 Loading、H.4.1；runtime-composition §3a、DI 3；limits LIM-5 補註。
- 2026-11-11（同上附帶）`v8-arch-peer-player-split`：Player 實體（玩家段 id；Body／Life／InputCursor／Connection 診斷）與 Peer 實體（Ledger、Visibility）分開，為觀戰者留位置；Link 是 external Resource 不是實體。networking §3b、spec H.4.1、pv10-requirements。

- 2026-11-11（側邊會話轉述，audit 待決第 6 項）`v8-arch-determinism-table-fix`：區表三漏一錯採 (a)，四處接受、排第一個文件批（與 E4 一起）；PlayerAnimation 推進先由決定性線核對 digest 編碼；條目名照歸屬改（固定步累加器→runner plugin (b)；時鐘樣本→HostClockSample；Sha256／StrictJson→「內容身分」「契約資料解析」兩條 (c) 路徑；以欄與 System 命名）。
- 2026-11-11（同上）`v8-arch-naming`：命名規範五條（歸屬＝namespace 與目錄；分類＝詞性照 Bevy；World＝前綴；區＝位置非名字；禁止 Manager／Renderer／ViewModel／Host／Shell／Runtime／Controller 後綴），target/naming.zh-Hant.md；遷移批事前宣告列改名清單。**audit 六項待決全部定完。**

- 2026-11-11 PR #181 合併（f8b6f74b）；三個審查者與 spike 的 agent worktree 已清。使用者「開始 spec-v03」→ `v8-arch-kernel-spec-v03` 啟動（事前宣告 spec-v03-declaration.zh-Hant.md；主會話自寫、對抗 Opus／high）。

- 2026-11-11 **-spec-v03 停止（AGENTS §14：原因改變前提）**。對抗檢查 Opus／high 回 9 blocker、11 should、5 nit；判定 1 不成立、2 有條件、3 不成立、4 成立。停止理由 B-9：規格 §讀法把「debug 斷言」定義為「`GYO_ASSERT`，只在 debug 有效、release 不攔」，但 `Assert.hpp:21` 與 error-handling.md 第 15 行寫明 GYO_ASSERT **在每個建置組態都評估**，架構師已核實。影響：K.3 第 17 項「不採常開檢查巨集、改 Result」的對比對象（常開巨集）其實就是現有 GYO_ASSERT；規格裡所有「debug 斷言…release 不攔」條文（W-4、W-5、W-6、F.2 第 4 步等）的機制名錯；本批已把第 17 項寫進現況 error-handling.md，與同檔「每個建置都跑 handler」並存。其餘 blocker（B-1 §0 Host 定義未改、B-2 H.4.1 (a) 漏改、B-3 H-7 呼叫點例子把 FixedTickRuntime::Advance 當違規、B-4 target 兩份未跟 Diagnostics 例外、B-5 (c) 診斷仍無合法路徑、B-6 ApplyMembership 掛兩個 schedule＝兩個寫者、B-7 entity-store DI 4 內部矛盾、B-8 F.1 untyped Commands 仍寫呼叫 assertion handler）可由架構師修，但依 §14 停下不 commit 修正，先報使用者。分支 claude/arch-kernel-spec-v03 保留現狀，未開 PR。→ 2026-11-12 B-9 由使用者結案（`v8-arch-kernel-verification-not-assert`），解除停止，繼續修 B-1～B-8 與 should。

- 2026-11-12 對抗檢查 S-8 的解讀（架構師）：`v8-arch-framework-retire` 廢止的是 framework **自己的**結構 DI（docs/framework/architecture.md）；event-sounds、preview 的資料契約 DI 不廢止，owner 隨 plugin 搬（audit §10.1）。文件標示照此改。framework DI「整節廢止」與 target README「未遷移照現況 DI」的關係：廢止＝不再新增或修改、隨遷移批刪；遷移前現有程式仍照它。
- 2026-11-12 v0.3 對抗檢查 9 blocker／11 should／5 nit 全部處置（spec 修訂記錄「v0.3 對抗檢查後」）。
- 2026-11-12 第二輪（Opus／high）4 blocker／10 should／4 nit（N-1～N-18）→ commit 929ca78e 處置；開關四條件 c64282d9。
- 2026-11-12 第三輪（Opus／high）2 blocker／11 should／4 nit（B-1～B-17），並指出第二輪 9 項只修一半 → 除 B-8 外全部處置（spec 修訂記錄「第三輪對抗檢查後」）。**B-8 待使用者決定**（見未定）。
- 2026-11-12 第四輪只讀複核（Opus／high）0 blocker／10 should／4 nit（N1～N14：`CurrentTick` 參數、Link inbound 分種類、序號本地配發、`ApplyStagedAssets`＋`LoadingState`、`KernelSettings` 時間點、E.2 第 ⑤ 步、Z5 複核範圍、stamp 改述、ClientSim 加 Loading、H-A1 移出啟動表、J-48～J-52）→ 全部處置（spec 修訂記錄「第四輪只讀複核後」）。B-8 由使用者決定（`v8-arch-kernel-break-hook`）並寫入。第五輪只對第四輪的 diff 複核：1 blocker（`LoadingProgress` 多寫者）／11 should／9 nit → 全部處置（spec 修訂記錄「第五輪差異複核後」）。對抗檢查到此收口（第四、五輪 blocker 分別 0、1，且都是前一輪新條文的收尾），開 docs PR。
- 2026-11-12 **PR #182 已合併**（`eb501731`；使用者「合併 #182」）。dev_log 補在 PR #183（已合併 `4bcc4bd8`）。
- 2026-11-12 使用者「可以下一批了」→ `-spike-pageguard` 事前宣告寫好（`spike-pageguard-declaration.zh-Hant.md`，branch `claude/arch-kernel-spike-pageguard`，draft PR），使用者同日「門檻照你定的，xhigh 同意，開始」→ 實作者子代理（Opus／xhigh，獨立 worktree，只 commit）已啟動；敵對檢查 Fable／xhigh 只讀排在實作回來後。估時實作 6 h（上限 9 h）。
- 2026-11-12 實作者回來（約 45 分鐘；commit `ed9657f9` → 分支 `85312dc2`）。件数：新測試 16／既有 33，debug 49 過、release 48 過 1 skip；突變新 5／killed 5（全目錄 39/39）。**判定 1 照原文不成立**：(b3) Resource 裸指標寫攔不到（宣告範圍排除 Resource，但判定 1 列了 (b3)——宣告自相矛盾，架構師的錯）；宣告的 L6 形狀第二層本來就攔得到、原始 L6（host 在 Ready 寫）頁保護攔不到。L1、(b1)、(b2) 攔到；判定 2、5 成立；判定 4 中位數（µs，4 KiB 頁）：Debug 組態 protect 1.71／switch 6.30／full 207；Release 組態 1.99／7.25／197——全在門檻內（第一次記錄把兩組態標反，敵對檢查 S7 指出，已改）。敵對檢查（Fable／xhigh 只讀）回來：0 blocker／7 should／4 nit，報告 `build/evidence/kernel-pageguard/adversary-report.md`（主 checkout，git 管理外）；重現兩組態 tests 2／failures 0；判定 2、5 經重跑確認，突變五條都由宣告的性質殺掉；S1（外來故障後 handler 永久卸除、靜默失效）、S2（跨 World 故障歸因）、S3（返回後重保護與 dropped 路徑無行為測試）、S5（(b) System 成本 2×|欄| 未量）交回實作者修（r2）；S4、S6、N1～N4 寫進報告。敵對檢查者對判定 1 的看法：實作者「照原文不成立」是正確讀法；兩種讀法下對 A.6 的建議相同——L1 可由頁保護補、L6 不能。
- 2026-11-12 r2（`defb759c`→分支 `a1e82e46`，15 分鐘）：S1／S2／S3／S5／N2／N3 已修；測試 52／51+1skip、突變 41/41；主會話獨立重跑兩組態一致（`verify-main-session-r2.txt`）。報告 `spike-pageguard-report.zh-Hant.md` 與 dev_log 已寫；PR #184 轉正式跑 L1（判定 3）；LIM-3 未改。
- 2026-11-12 使用者定判定 1 讀法 (i) 照原文（`v8-arch-kernel-pageguard-reading`）；宣告檔狀態、報告、dev_log 同步。判定 3 **成立**（L1 run 43169204148 三平台 xml 各 2／0；artifact 存主 checkout `build/evidence/kernel-pageguard/l1-43169204148/`）。#184 合併（使用者授權「L1 綠後合併」）；之後下一批由使用者排（`-remove-object-fps` 事前宣告，或遷移批規劃）。

- 2026-11-12 `-remove-object-fps` 盤點（master `56b8dda3`）：待刪 307 檔（apps 71＋78、tools 5、tests 26＋11、acceptance 14＋5、assets 41＋49、docs 2＋3、IRuntimePort.hpp、RuntimePortTests.cpp）、修約 26 檔；pvp 經負向前瞻掃描零引用（只共用 namespace 名 RetroFPS）；共通層（build/cmake、build/ci、.github、頂層 CMake、tests/common）沒有一處寫死 object_fps——產品由兩份 CSV 列舉、tests／acceptance／assets／tools 目錄照名字慣例發現，所以預期共通層零修改，寫成判定 2（不成立＝§7 不成立，停下記 Delta）。3d-assets.md 記的素材包授權仍被 pvp 用（UAL mannequin），處置待使用者。宣告草案 commit 在分支 `claude/arch-remove-object-fps`。

- 2026-11-12 `-remove-object-fps` 實作者（Opus／high，27 分鐘）回來，**停止：判定 3 不成立**（照宣告 C 沒修）。三個 commit 已 cherry-pick 到分支（`0d05cc4e` 刪 305 改 2、`6beee0f7` 刪 2 改 4、`4d807cd5` 刪 1 改 26 新 1）。判定 1 成立（刪 308 檔逐檔相同；修 32 檔全在清單內，宣告估「約 26」低估了文件數）；判定 2 **成立**（共通層零修改，只有兩份 CSV）；判定 3 的其他項成立（grep 只剩禁字表、空目錄 0、`-DGYO_APPS=object_fps`／`object_fps_v2`／`-DGYO_TOOLS=object_fps_preview` 都以「not registered」失敗）；本機 preset test 全建 0 error、ctest 138/138、doctest engine_tests 140→139、gyo_model_tests 24→21。**不成立處**：`tests/object_fps_pvp/` 有 6 行提到已刪產品（HitboxSurvey.cpp 兩行註解、test_asset_catalog.py 三行守衛自測的假路徑、fixtures/hair_buns/README 一行說明）＋ 2 處仍指向已刪的 `3d-assets.md`（asset_sources.json 第 62 行、hair_buns/README 第 9 行）。宣告前提「pvp 零引用」是架構師掃描的錯：第一次寬掃描有列出 `test_asset_catalog.py`，被誤判為 `RetroFPS` 的命中；第二次負向前瞻掃描用了 `xargs grep -P`，macOS 的 BSD grep 沒有 `-P`，錯誤被 `2>/dev/null` 吞掉、輸出為空，被讀成「零引用」。判定用的 grep 以後用 `git grep -P` 或 Python，且不丟棄 stderr。實作者另記：ModelTests.cpp 原有 15 個 TEST_CASE 不是 17；刪第三個案例後 `PlaybackInterval::Crossed` 在 tests/common 無直接測試（model_viewer 的 CrossedEvents 間接涵蓋）；`Animation.hpp` 的 `<memory>` 保留（pvp 三檔可能間接依賴）；現行文件還有大小寫不同的「Object_FPS」字樣交圖書管理員。證據：主 checkout `build/evidence/remove-object-fps/`（before-* 為主會話基準 152 ctest 全過；impl/ 為實作者的）。**待使用者決定**：(a) 允許改 pvp 測試目錄那 8 行（註解、字串、說明、兩個出處路徑）當第四個 commit；(b) 判定 3 照原文不成立、8 行留給產品線。

- 2026-11-12 使用者選 (a)：主會話以 commit `4bb944cb` 改寫 pvp 測試目錄 8 行，判定 3 成立。主會話判定 4、5（基準環境）：ctest 152→152 名單相同全過、doctest 只差 engine_tests −1／gyo_model_tests −3、pvp 57 項案例數相同、空目錄 0。PR #185 開成正式跑 L1。審查者（Fable／high，只讀；`build/evidence/remove-object-fps/review-report.md`）：0 blocker／5 should／6 nit，判定 1～3 獨立重做成立。S1：刪掉的第三個 ModelTests 案例主要在測保留的 `PlaybackInterval::Crossed`，整個刪掉是 §14.4 的緩める面且宣告沒寫明 → 主會話把 5 個 CHECK 以同名案例留回（第五個 commit；gyo_model_tests 24→22，沒有緩める面，事後報告）。S2～S5、N2～N4 已處置；N1 保留；N6 交圖書管理員。
- 2026-11-12 使用者問有沒有可並行的任務；架構師列表後，使用者決定先開 J-17 掃描器線與圖書管理員兩條，交接卡已寫（分支 `claude/arch-handoffs-j17-librarian`）。

## 下一步

- `v8-arch-remove-object-fps`：判定 1～5 全部成立（三平台（L1 run 43188319281，commit `e9324903`）：linux-x64 ctest 138/138、windows-x64 142 過 1 skip（`os.console_close`，既有的 OS 子系統 skip、與本批無關）、macos-arm64 138/138；`engine_tests` 139、`gyo_model_tests` 22 三平台一致（artifact `tests.xml` 存主 checkout `build/evidence/remove-object-fps/l1-43188319281/<platform>/`）。）；使用者 2026-11-12「#185、#186 都 OK」→ 合併。下一批：遷移批規劃（§3 Delta 計畫層七項、A.6 修訂吸收 pageguard 11 點、plugin 邊界）。
- ~~改造的第一個文件批~~ 已於 -spec-v03 做完（architecture.md、entity-store、determinism §4.7／4.7a／4.7b／4.2a、entity-model §5.2、error-handling、framework 廢止標示、mechanism §3.3）。
- ~~-audit~~ 完成，六項待決已定 → 規格修訂（audit §6 七項）與第一個文件批合併規劃 → `-spike-pageguard`（事前宣告待寫）→ 改造批次計畫（§3 Delta 計畫層寫一次；以 audit 的成本形狀分批）。
- 判定 1 的三平台：**成立**（L1 run 43018979148，PR #180；步驟第一次誤加在 build-and-validate.yml、已改到 cross-platform.yml）。PR #180 含 `spikes/` 以外的 `.github/workflows/cross-platform.yml` 改動，合併由使用者決定。

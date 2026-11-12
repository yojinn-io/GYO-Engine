# 架構 v8：v8-arch-kernel-spec-v03（規格 v0.3：audit 九項修訂、承認済み Delta 的第一個文件批、五輪對抗／複核）

日期：2026-11-12。Owner：架構（kernel 研究線）。計畫：[kernel README](../architecture/plans/kernel/README.md) 批次表的 `-spec-v03` 列、[事前宣告](../architecture/plans/kernel/spec-v03-declaration.zh-Hant.md)。PR [#182](https://github.com/yojinn-io/GYO-Engine/pull/182)（docs-only，24 個檔，合併 commit `eb501731`）。
- 執行：架構師主會話（Fable／high）自寫；對抗檢查與複核由只讀子代理（Opus／high，general-purpose）做，每輪一個，不修檔。
- 性質：文件。不改程式、preset、CI。

## 起因

`-audit` §6／§10.5 列出九項規格修訂，使用者在側邊會話把 audit 六項待決定完；K.3 第 1～17 項已決定但 target 與現況文件還沒反映（`docs/architecture.md` 的禁止條文、entity-store DI 4、determinism §4.7、entity-model §5.2）。這批把兩件事合在一條 PR 線做完，並用對抗檢查把規格推到可以開 `-spike-pageguard` 與遷移批規劃的程度。

## 做了什麼

- spec v0.2 → v0.3：九項修訂；第一個文件批（architecture.md 禁止條文改寫、entity-store DI 4／`DeferredChanges` 撤回、determinism §4.7／4.7a／4.7b／4.2a、error-handling 新列、framework 三份文件的廢止 Status、entity-model §5.2、pv10-requirements 的 ready 事件與 Peer 身分）。
- 五輪對抗／複核後的設計確定（條文位置在 spec 末修訂記錄「v0.3 對抗檢查後」～「第五輪差分複核後」）：Loading schedule 的 Ingress／資產 stage 與 role=Input 暫存（`MembershipRequests` 以本地序號為鍵＋`MembershipCursor` rollback=Yes、`AssetStaging`＋`AssetCursor` 連續前綴、`LoadingStageDone<Stage>`／`Finish<Stage>`／`LoadingState`）；ingress System 三分割、Link 家族的收件 Resource 各一份；`CurrentTick` 參數（Kind `DomainTick`）；多掛點的 `Local` 與「上次執行」以掛點為單位；Z5 記錄當下判定＋F.2 第 0／2 步複核；E.2 的呼叫前後比對與 Siblings 錯誤清單；`KernelSettings` 的三個時間點與 Executor 的 `hasRun`；Client 的 Loading（manifest 同源序號、成員與 tick 經 Restore）；J-48～J-53。
- 使用者決定（側邊會話轉述）落檔：驗證＝啟動參數執行期開關（四條件、二限制，LIM-6；`v8-arch-kernel-verification-not-assert`）；`Result<void, KernelError>` 是回傳契約不是錯誤通道；B-8＝「一個事件只有一種性質」、刪 debug 的 AssertionHandler 呼叫、改 `breakOnKernelError` 掛鉤、Go 參考（`v8-arch-kernel-break-hook`）。
- 用詞：「Result 回報」→「以 Err 回傳的 Programmer Error」；「debug 核對」→「存取驗證（可關）」；「只在 debug」全部歸類為 GYO_ASSERT 或驗證。

## 驗證與經緯

件数の突き合わせ：本批無計測；受け入れは五輪の只讀檢查で判定。

| 輪 | 範圍 | blocker／should／nit | 處置 commit |
|---|---|---|---|
| 一 | spec v0.3 全文 | 9／11／5 | 停止報告 B-9（規格誤寫 GYO_ASSERT 只在 debug；`Assert.hpp:21` 常開）→ 使用者結案後套用 |
| 二 | 全文 | 4／10／4（N-1～N-18） | `929ca78e` |
| 三 | 全文＋第二輪逐項確認 | 2／11／4（B-1～B-17）；指出第二輪 9 項只修一半 | `7ebf9b9b`；B-8 交使用者 |
| 四 | 全文＋第三輪逐項確認 | 0／10／4（N1～N14） | `0ca0f3c6` |
| 五 | 只看第四輪 diff | 1／11／9（K1、K-S1～S11） | `c155ef24` |

- 收口判斷：第四、五輪的 blocker 分別 0、1，且都是前一輪新條文的收尾（第五輪的 K1 是 `LoadingProgress` 被多個 stage 的 System 寫、會被 D-E2 擋下），不再開新輪。
- 第一輪停止（AGENTS §14「原因改變前提」）：B-9 指出規格把 GYO_ASSERT 寫成 debug-only，與 `Assert.hpp` 契約相反；停下報告，使用者在側邊會話定「驗證不叫斷言、以啟動參數開關」後續做。
- 第三輪 B-8 牴觸 K.3 第 17 項的使用者決定（debug 另呼叫 assertion handler 會讓 Err 路徑在 debug 永遠走不到），寫進 HANDOFF 未定、不自行改；使用者在側邊會話選 (b)＋明文規則後落檔。
- 證據：五輪報告在會話內交付，原文**未保存**（會話 scratchpad 不算證據所在）；每項發現的處置與條文位置寫在 spec 末修訂記錄，可逐項對照。
- 估時：事前宣告未估（規格批）；實際一個工作日（含等側邊會話的決定）。

## 架構報告

- Architecture Delta：全部是已承認 Delta 的文書化（spec K.3、HANDOFF 決定表）；本批沒有新 Delta。
- Data Contract：pv10-requirements 追加 ready 事件與 Peer 身分（target，未實作）。
- 留給後批：`-spike-pageguard` 歸「存取驗證」第三層；`-remove-object-fps`；遷移批規劃（§3 七項在計畫層寫一次）。

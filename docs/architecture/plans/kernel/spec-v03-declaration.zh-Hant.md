# 事前宣告：`v8-arch-kernel-spec-v03`（規格修訂 v0.3 與第一個文件批）

更新：2026-11-11。Owner：架構（架構師會話）。**狀態：使用者 2026-11-11「開始」；執行中（主會話自寫，docs-only）。**

## 目的

把 -audit 抓到的九項規格修正（audit §6 七項＋§10.5 兩項）寫進 spec，並做使用者在 K.3 第 7 項與 audit 待決裡同意的 Delta 文件修改（第一個文件批）。不改程式。

## 範圍

**spec v0.2 → v0.3**（`plans/kernel/spec.zh-Hant.md`）：
1. (c) 區的診斷輸出路徑：S-8／Z4 開唯一例外 `Diagnostics` effect（只寫不讀回、(c)→(b) 單向；比照 hook 與 I-5）；G.4 第 3 條改法對齊。
2. J-17 掃描規則：掃 include（`engine/time/MonotonicClock.hpp`、`<chrono>`）而非函式名；(c) 檔的時間型別用整數 ns、不 include `<chrono>`；掃匿名命名空間非 const 變數；掃第三方全域呼叫（`ImGui::`、SDL 全域查詢）；掃 `Time::Now` 別名。
3. H-7 守衛：加呼叫點規則（host 原始碼不呼叫產品模組的 `Tick`／`Advance`／`Update`），不只掃 include。
4. G.4：第 1 條改 `HostClockSample`；第 3 條位置更正；補第 4（`RequestMovementResync`）、第 5（IPC 讀權威）。
5. L.2：WindowShell 改 Editor host（已於 audit 批改）；確認。
6. target/runtime-composition 的 AssetManager 歸屬（已定：拆開，assets 文件）；確認 spec L.1 一致。
7. 第三方「依附在服務內的函式庫隨服務判」（已補宣告）；spec I.1 表加一句。
8. H.4.2：Client host 的骨架由 runner plugin 提供（正式文字，不只註）。
9. I.1／H.7：「Host」改寫為「runner plugin 加產品 main」；H.7 的職責不變、持有者變。
加：§讀法指向 `target/naming.zh-Hant.md`；狀態改 v0.3；修訂記錄一節。

**第一個文件批**（現況文件的 Delta 修改，使用者已同意：K.3 第 7 項、audit §3.1、§3.6、§4.5、第 17 項）：
- `docs/architecture.md`：Engine 不得有 system／scheduler／world 的禁令 → 「Kernel 進 Engine（World／System／Scheduler／Executor）；plugin ABI 與動態載入仍禁止」。
- `docs/architecture/entity-store.md`：DI 4 延伸到 insert／remove；`DeferredChanges`（proposed，3b）撤回由 World 延後佇列取代；DI 5 對 store 本身仍成立。
- `docs/architecture/determinism.md`：§4.7 正式化（columns → World storage）；§4 補內容身分與契約資料解析兩條 (c) 路徑、固定步累加器（runner plugin，(b)）、`HostClockSample`（(c) 輸入）；`PlayerAnimation` 推進標「待決定性線核對 digest 編碼」。
- `docs/object_fps_pvp/entity-model.zh-Hant.md` §5.2：Join／Leave 在 tick 內（LIM-5）；E-12 的待辦由 World 的延後佇列承擔。
- `docs/architecture/error-handling.md`：加「以執行期資料偵測的 Programmer Error」一列（target/error-handling.md 的內容）。
- `docs/framework/*.md`：標廢止（framework 退役；現有程式在遷移前仍照它）。
- `docs/architecture/plans/kernel/mechanism.zh-Hant.md` §3.3：已改（時間來源）。

## 判定（看結果前宣告）

1. 九項每一項在 spec 都有對應的修改處，且 audit §6／§10.5 的條目逐一標「已修」。
2. 第一個文件批的每份文件：修改處引用決定代號（HANDOFF 的 `v8-arch-*`），沒有未經同意的 Delta。
3. 對抗檢查（Opus／high，只讀，與撰寫者 Fable 不同模型）：在 spec v0.3 與 target/ 之間找不到互相矛盾的句子；找到 blocker 就回修。
4. `git diff` 只有 `docs/`。

## 範囲の限界

- LIM-3：規格文字改了，掃描規則的檢出力要在實作 J-17 時用突變證明；本批不證。
- 不碰 DET-0～3、網路兩個決定、事故修正是否繼續（使用者未定）。

## 模型／檔位

主會話 Fable／high 自寫；對抗 Opus／high 只讀。估 3～4 小時；超 1.5 倍停。

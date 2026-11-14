# R0：帶過去清單（`v8-arch-rebuild`）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：已核准（使用者 2026-11-14「1 到 9 照建議」）。使用者同時指示：master 不併進 gyo2，gyo2 做好後再搬到 master——第 7 節的 R0b-1 取消。v2 修訂後沒有再檢查。**

出處：[rebuild 事前宣告](rebuild-declaration.zh-Hant.md) §4。對象是 `origin/gyo2` = master 08566260。

過程：
- 四份盤點（Opus／high，只讀）→ 統合 v1（主會話 Fable）→ 別模型檢查（Opus／xhigh，一次）：blocker 5、should 15、nit 8，全部採用。
- 另採原架構師會話轉來的四點提醒（使用者指示轉達）：帶過去是污染的入口、守衛會把舊形狀釘回、gyo2 少了 #223、舊決定紀錄不直接帶。
- 證據在主 checkout `build/evidence/rebuild-r0/`（git 不追蹤）：四份盤點、v1、檢查報告。本檔的判斷以附錄的表為準，不依賴未追蹤的檔。

三類產品：①PvP、②2D 橫版單機（合成對象）、③Editor 與工具。

## 0. 件数の突き合わせ

一致。以 `git ls-tree origin/gyo2` 重數；命中用的已知項目：`engine/kernel`、`third_party/sdl3/patches/gyo-cocoa-ime-composing-keydown.patch`、`apps/object_fps_pvp`、`build/cmake/GyoBuild.cmake`、`tests/common/h7_scan/h7_scan.py`。

| 範圍 | 件數 |
|---|---|
| 全 repo | 1988 檔 |
| 頂層目錄／頂層檔案 | 13／7 |
| `engine/` 模組 | 23 |
| `third_party/` | 10；另有不在 third_party 的外部組件 10 列（SDL_shadercross、DXC、SPIRV-Cross、Go 模組等） |
| 產品 | 6 |
| `build/`、`.github/`、`tests/`、`assets/`、`agents/`、`docs/` | 139、5、414、36、2、714 檔 |
| `tests/common` 子目錄 | 30，其中模組測試 22 |
| Kernel 設計元素 | 319，另有條文 118 列 |
| Kernel 程式 | master 的 `engine/kernel` 15 檔、`tests/common/kernel` 20 檔 |

## 1. 原則（檢查後補上）

1. **帶過去是污染的入口。** 「在底層、看起來中性」不是帶的理由。每個帶過去的項目都寫五欄（附錄）。
2. **測試、守衛、突變逐條判。** 守通用性質的帶；守舊形狀的不帶或改寫。characterization 與 golden 也算。
3. **決勝理由只用 Feature 與 Contract。** 「已驗證」「已核准」「測試依賴它」「變更少」只當系譜或成本。
4. **R0b 不做設計改動。** 需要改設計才能帶的東西，不在 R0b 帶。

## 2. engine 模組

| 處置 | 模組 |
|---|---|
| 帶過去 | base（不含 StrictJson）、math、os |
| 帶過去並重審（重審點見附錄） | time、threads、io、audio 的 core |
| 邊界項，交使用者 | collision、model |
| 不帶 | kernel 的程式（第 5 節）、asset、entity_store、input、map_markers、net、platform、render、runtime、text、trace、ui、voxel_mesh；base 的 `StrictJson.hpp` |
| 資料清空 | config 的 `projects.csv`、`tools.csv`（留表頭） |

- 帶過去的集合，engine 原始碼層的依賴都在集合內（盤點 a 的含入掃描：14 條邊；掃描輸出未保存，R0b 開工時重跑並存進 `build/evidence/`）。
- 測試層有指向不帶項目的邊，R0b 要處理：`tests/common/time/WakeRecord.cpp:13` 含入 platform；`tests/common/support/RenderDeviceStub.hpp` 含入 render。
- input、text、trace 不帶：形狀由 R1 推導，master 上的程式當參考。

## 3. 第三方、產品、資產

- 第三方只帶 doctest，其餘 9 個不帶。
- 產品 6 個、framework、`assets/`、`spikes/` 不帶。產品在 R3～R5 依新架構搬回；各產品用到的模組與第三方見盤點 (a) 表 3。

## 4. 建置、CI、測試基礎設施

| 項目 | 處置 |
|---|---|
| 產品登記、打包、驗收、發佈、CI 腳本、native-setup、package-trial、prepare-release | 帶過去 |
| `GyoBuild.cmake`、元件對照表、Go module 機制、`cross-platform.yml`、`build-and-validate.yml` | 帶過去並重審（附錄） |
| `-ffp-contract=off`（`GyoBuild.cmake:36-42`） | 單列交使用者（第 8 節第 5 題） |
| shader 建置、render 選項、兩個產品的 acceptance 目錄 | 不帶 |
| `tests/common` 的 support、ci、cmake、assembly、fixtures | 帶過去並重審 |
| `tests/common/scripts/neutral_names_guard.py` | 不帶：gyo2 上沒有呼叫者；詞表是地圖玩法的詞，`zone`、`extract` 會和新詞彙相撞 |
| zone_scan、h7_scan | 不帶 |
| 共通突變執行器（現在放在 `docs/architecture/plans/time-threads-trace/scripts/`） | 帶過去並搬到 `tests/common/mutation/` |
| 突變清單 | 只帶對象是帶過去檔案的條目，放到各模組的測試目錄 |
| `gyo_expect_links` | 留在原處（time 帶過去，不必搬） |

模組測試跟著模組。帶過去模組的測試與守衛，R0b 之前逐條判（原則 2）。已知要判的：
- `MathCharacterizationTests`：逐位元凍結舊數值，等於舊形狀的 golden。
- os 的 boundary allowlist：列了不帶的檔。
- collision 若帶：CollisionCorpus 是 PvP 決定性的 golden。

## 5. 舊 Kernel

### 5.1 設計：當 R1 的輸入，不在 R0b 落成條文

盤點 (c1) 把設計拆成 319 個元素與 118 列條文：

| 處置 | 設計元素 | 條文 |
|---|---|---|
| 照帶 | 169 | 55 |
| 帶設計、換位置 | 78 | 37 |
| 帶設計、換理由 | 21 | 12 |
| 不帶 | 31 | 11 |
| 未判 | 20 | 3 |

- **照帶的方向**：簽名推出存取、Result 錯誤契約、兩段登記與一個型別一個 owner、衝突與順序分開、(b) 依登記序平手、延後結構變更、每格 stamp、一個 World 一條執行緒與交換點、Extract 單向、Kernel 中性。
- **換位置的去處**：決定性 plugin、預測（回滾）plugin、網路與複製 plugin、PvP 產品。
- **Kernel 要為此提供的擴充點，不只 7 個**：
  - 型別屬性袋
  - finish 時的驗證 hook
  - 存取集合與歧義清單的匯出
  - 延後記錄當下的驗證 hook
  - Registry 匯出含來源位置
  - 快照與還原原語
  - plugin 依賴宣告
  - 重跑 schedule 的入口
  - schedule 執行後的 hook
  - 事件讀者的兩種模式
  - 可替換的預設配發器
  - 開放的 domain
  - plugin 可登記 schedule 與 stage
  - 用同一份凍結 Registry 重建 World
- **換位置的代價**（全局審計第 2 節）：
  - 擴充點本身是新的基盤 API。
  - 產品忘了選決定性 plugin 時，(c) 的檢查整個消失。
  - 有序與無序兩種疊代並存。
  - 未查證的反例：Photon Quantum 這類多人引擎把決定性放在核心。
- **不帶的核心**：依 EntityId 排序的儲存與 RC 6；World 不配發；封閉的 domain；Host 持有固定步累加器與 H-7 白名單；以舊模組為單位的判準；把被否決的替代案寫成必殺突變。
- **要單獨給使用者看的**：
  - (c1) 推翻全局審計的 13 處（附錄 A.4）。
  - 兩層存取驗證的第二層成本未判：4,000 實體 p95 29.8 ms／Run，驗證開著時玩家感受得到。
  - 未判的 20 項。
- 這張表是 R1 的底稿。R1 要對每一項重新寫系譜、被否決的替代案、需要它的產品（盤點 c1 的各列缺「被否決」欄），並以 Bevy 對照。

### 5.2 程式：R0b 不帶，R2 整段搬回

v1 建議 R0b 先帶「零 PvP 的一組」。檢查否決了，理由成立：
- 那一組的測試全部建在 Registry 上：`SignatureTests.cpp` 經 `KernelTestSupport.hpp` 含入總標頭；compile-fail 樣本走 `Registrar::AddSystem`。`KernelError.hpp` 的函式定義在 `Registry.cpp`。要拆測試、改 target 型別才帶得動，那是設計與守衛的改動，不是刪除與建置修補。
- 「零 PvP」不實：`KernelError.hpp` 的錯誤碼含 zone 與 rollback 的碼；`Params.hpp` 有三處以 zone 定義語意。行數是 910，不是約 800。
- 參數詞彙會預定 R1 的 System 契約：`SetRead`（複製用）、`SpawnWithId`（網路）、`CurrentTick` 與 `DomainTickRead`（封閉 domain）、`Spawn` 寫 World 的配發器、`Commands` 只對 zone B、`ExclusiveWorld`（寫入範圍未判）。

處置：
- **R0b**：`engine/kernel`、`tests/common/kernel`、kernel 的突變清單都不帶。
- **R2**：R1 定了宣告面之後，依盤點 (c2) 的元件表從 master 整段複製程式與測試，逐段審，不重打。可用的量：
  - 帶程式：31 個元件，約 1,770 行。
  - 帶程式並刪改：36 個元件，約 3,030 行，其中刪改 550～650 行。
- PvP 概念在 Registry 與 Scheduler 裡集中在可整塊切掉的區段（約 229／1336 與 52／563 行），織進主邏輯的只有兩處。這是 R2 的成本估計依據，不是決勝理由。
- #218 的 Executor、存取驗證、KernelError 鏈：設計可用，程式與 World 儲存沒有分開。R2 時先切介面再搬。
- 不帶：cFingerprint 與卡 08 的 patch、zone-C 來源清單匯出、dataflow 量測欄位、#218 的 World 儲存、entity_store、spike。
- 守衛 K0～K7、N1、N3、符號檢查隨 kernel 在 R2 回來，屆時逐條判。三份盤點對 K2（全面禁 unordered）立場不同：
  - (b)：守舊形狀，不帶。
  - (c1)：換理由，只禁影響結果的雜湊疊代。
  - (c2)：帶，理由改寫。

  交 R1 定。

| 被否決的帶法 | 決勝理由 |
|---|---|
| 整包帶進 gyo2 再刪改 | Kernel 的宣告面是 R1 要從三類 Feature 推出的 Contract（宣告 §1）；先帶就是先定 |
| R0b 先帶零 PvP 的一組 | 同上；而且不改設計就帶不動（原則 4） |

## 6. 文件與決定紀錄

檢查指出 v1 的帶法會斷鏈：
- 三份 target 頁的 DI 全文都指向不帶的 kernel 頁。
- 入口 `docs/architecture/README.md` 的四個固定節全部指向 target。
- `math.md` 與 legacy 的 `error-handling.md` 沒有 Design Invariants 節。

v2 的方案：

| 項目 | 處置 |
|---|---|
| `docs/architecture/target/` 全部 11 頁 | 不帶。R1 寫新的。三份 v1 想先帶的頁（error-handling、naming、system-contract）裡有 PvP 的詞與指向 kernel 頁的 DI，改寫等於改 DI，屬 R1 |
| 入口 `docs/architecture/README.md` | 帶過去。四個固定節在 R1 完成前的指向，由圖書管理員（檔案 owner）依第 8 節第 7 題的決定改寫 |
| 「事前宣告的檢查項」條文 | gyo2 上的 `target/README.md` 有 #222 之前的版本；#223 的細目在 master、不併過來。R0b-4 把 gyo2 現有的條文搬到不在 target 下的規則頁，並依 AGENTS §4 補上三欄的細目（內容與 #223 一致，重寫進 gyo2，不是合併）；位置交圖書管理員 |
| 帶過去模組的架構文件：os、time、threads、io、audio；collision 若帶則 collision | 帶過去，開頭加註：這是 master 08566260 的條文，連到 plans 的連結以 `master@08566260:<path>` 讀；重審點見本檔附錄 |
| `math.md`、legacy `error-handling.md` | 帶過去（base 與 math 的契約說明）。兩者沒有 Design Invariants 節，R1 補 |
| AGENTS.md、`agents/`、cross-component-rules（CCR-1 那一列留 master）、checkup 的三份 README、`creating_apps`、`tool_projects`、`releasing.*`、`field-session.md` | 帶過去；後幾份的產品例子在 R0b 的文件 PR 改 |
| dev_logs、checkup 的報告、`docs/architecture/plans/` 的舊計畫、其餘 legacy 頂層文件、產品與工具文件、`docs/framework` 等 | 不帶 |
| gyo2 自己的計畫 | 新資料夾 `docs/architecture/plans/rebuild/`：README、HANDOFF（新的決定紀錄）、全局審計、rebuild 宣告、本清單 |
| README 三語 | R0b 的文件 PR 重寫 |

決定紀錄：
- gyo2 的 HANDOFF 從 2026-11-14 的五條決定開始。
- 舊的 kernel HANDOFF 與 spec K.3 留在 master 當歷史；要沿用時逐條重新決定，並附系譜與被否決的選項（盤點 c1 §3.6 已把 K.3 的 17 項分成使用者主動改的與「採規格」通過的）。
- gyo2 的文件引用舊決定時，寫 `master@08566260:<path>` 加決定代號。

同詞異義表（20 個詞，盤點 b 第 4 節）放在 `plans/rebuild/` 當 R1 的輸入，不放進入口（入口不持有規則）。v1 的建議有兩處要改：
- 「禁用 Runtime」和錯誤契約的「Runtime Error」相撞：改成只禁它當概念名與型別後綴。
- 「Domain → clock」把 Loading 也當成時鐘：封閉 domain 本身不帶，新名交 R1。

## 7. R0b 的做法與判定

v1 說「R0b 只改 CMake」不實。R0b 拆成三個 PR，都進 gyo2（原訂的 R0b-1「把 master 併進 gyo2」已由使用者取消）：

| PR | 內容 | 性質 |
|---|---|---|
| R0b-2 | 刪除不帶的目錄與檔；讓剩下的集合建置並通過 L1 | 刪除與建置修補。io 另立 target 是 Build Graph 的變更，照 AGENTS §3 寫 Architecture Delta |
| R0b-3 | 守衛與 CI 政策測試的判定變更 | AGENTS §14.4：逐條寫 1～5，緩める面交使用者核准後才 commit |
| R0b-4 | 文件：入口、`plans/rebuild/`、README 三語、模組文件加註、說明文件的例子、突變執行器搬家 | 文件與搬移 |

R0b-3 已知的項目（開工時補全）：
- `test_workflow_gates.py`：
  - `assertTrue(tools)`（:361）與 `assertTrue(owners)`（:1121-1128）：沒有產品時要嘛失敗，要嘛空跑。
  - Engine 名義的 Go module（:1099-1108）。
  - render 的 GPU 命令（:381-391、:684-704）。
- `app_copy_integration.py:19`（services）。
- `os_boundary_guard.py`：ROOTS、render 的排除路徑；allowlist 是只減不增的名單。
- `naming_guard.py:44`（framework）。
- workflow 的 kernel spike 步驟與 GPU 步驟。
- 未查證：工具數為零時 toolchain 的打包會不會失敗。

**宣告的判定要改兩條（交使用者核准，AGENTS §14.1：結果出來前定）**

- **判定 4**：原文「第一個 PR 只做刪除與讓建置通過」。改成上表的三個 PR，L1 三平台綠的對象是 R0b-2 與 R0b-3。
- **判定 5**：原文「被刪的目錄名與模組名，在新分支中零命中」。
  - 這條無法機械判定：render、input、text、model、net、ui、asset、runtime 都是一般英文詞。
  - 新判定：
    - pattern 用路徑與識別字形式：`engine/<被刪模組>/`、`tests/common/<被刪目錄>/`、`framework/`、`spikes/`、被刪的 CMake target 名（`GYO::<Name>`）、產品 id。
    - 掃描範圍：`engine/`、`build/`、`tests/`、`.github/`、頂層檔案。
    - 文件（`docs/`）另列一張「提到被刪路徑的文件」清單，不判零命中。
  - 守的性質：帶過去的程式與建置不依賴被刪的東西。
  - 緩める面：文件、註解與字串裡以一般詞提到舊模組的地方不再算失敗。
  - 這一面由 R0b-2 的 L1 建置守住：真的依賴就編不過。
  - 已知的情形：盤點 (b) 已看到建置層依登記慣例組出 `apps/<id>` 路徑；檢查者的探索掃描在帶過去的檔裡看到約 321 筆路徑形式的殘留（多在文件與測試註解；輸出未保存，不當判定的根據）。所以這是看過部分結果之後的修改，照實寫明。

v1 提的「push 到 gyo2 時也跑 CI」拿掉：和宣告 §2「不直接推」不合，也要改兩條 CI 政策測試；PR 已在合併 commit 上跑 L1。

## 8. 要使用者定的事

1. **engine 的帶過去集合**（第 2 節與附錄 A.1）：base（不含 StrictJson）、math、os；time、threads、io、audio core 帶過去並重審。
2. **邊界項**：collision 與 model，現在帶，或等搬 PvP 時取回。兩者你都判過「固化、留 Engine」。
   - collision 是只依賴 base 與 math 的葉子，但形狀是 3D FPS 角色。
   - model 用 nlohmann 解析，又和 asset 耦合，現在帶不動。
   - 建議：兩者都等用到它的產品搬回時再取回。
3. **第三方只帶 doctest。**
4. **Kernel 程式 R0b 不帶，R2 依 (c2) 的表整段搬回**（第 5.2 節）；Kernel 設計的表當 R1 的底稿（第 5.1 節）。
5. **`-ffp-contract=off` 留不留**：
   - 系譜是 PvP 的跨平台決定性（2026-10-03 你的 B1b 決定）。
   - 作者不必寫任何東西，代價只有性能。
   - 建議：留，並在 R1 的決定性文件寫明系譜。
6. **文件方案**（第 6 節）：target 11 頁都不帶；dev_logs 與舊計畫不帶；模組文件加註帶；新的 `plans/rebuild/` 與新的決定紀錄。這表示 34 條已核准的目標 DI 在 gyo2 上都不存在，直到 R1 重寫。
7. **入口四個固定節在 R1 完成前指向哪裡**：
   - (a) 寫「R1 進行中，規則暫以 `master@08566260` 的 target 為參考，不具約束力」。
   - (b) 把 target 當凍結參考整份帶進 gyo2 並加標記。
   - 建議 (a)：凍結參考放在 gyo2 上，就是舊形狀留在手邊。
8. **R0b 拆成四個 PR，並修改宣告的判定 4 與判定 5**（第 7 節）。
9. **`MathCharacterizationTests`**：照帶，或 R0b 不帶、R1 重設基準。
   - 它凍結的是 math 統一時的舊數值。
   - 建議：帶，但在附錄標明它是 characterization，不當設計的判官。

## 9. 範囲の限界

- 盤點是靜態分析，沒有建置與測試；「能單獨建置」要到 R0b-2 的 L1 才確認。
- 未查證：
  - 工具數為零時的打包。
  - gyo2 的 PR 會不會跑 Quick。
  - audio 的 Submitter 執行緒契約與 R1 交換點模型的相容性。
  - 相對路徑的含入沒有掃。
  - 突變的 find 字串沒有重新驗證。
- 盤點 (a) 的掃描輸出沒有保存；R0b 重跑。
- ②是合成對象，②欄多為推定。
- 盤點 Opus、統合 Fable、檢查 Opus／xhigh 一次；v2 沒有再檢查。

## 附錄 A：五欄表

### A.1 engine：帶過去與邊界項

| 項目 | 系譜 | 需要它的 Feature 與產品 | 處置；被否決的處置與決勝理由 | 依賴 | 重審點 |
|---|---|---|---|---|---|
| base：Assert、Result、Error、Log facade、Span、Fnv1a、Sha256 | 新導出：Assert＝Programmer Error、Result＝Runtime Error（R0～R6，使用者定） | ①②③：任何程式都要回報錯誤；io、audio 用 Span | 帶過去。否決「只帶 Assert／Result」：io 與 audio 的介面用 Span 與 Fnv1a，少了就不能表達它們的 Contract | 只有標準庫 | — |
| base 的 `StrictJson.hpp` | 以 nlohmann 的 SAX API 為形狀的模板；註解寫 voxel_mesh、map_markers | gyo2 上沒有使用者 | 不帶。否決「帶過去並重審」：使用者已決定 JSON 走 yyjson 與 Engine 自己的介面；留著是沒有 adapter 的外部組件形狀（AGENTS §6） | nlohmann 的型別 | — |
| math | 新導出：GYO::Math 統一（2026-10-03，使用者定） | ①②③：位置、向量、座標慣例 | 帶過去。否決「不帶」：三類產品的任何模組都以它表達幾何 | base | `math.md` 沒有 DI 節；characterization 測試（第 8 節第 9 題） |
| os | 新導出：OS adaptor（使用者的原意；2026-10-28 收口） | ①②③：檔案、行程、執行緒命名、主控台 | 帶過去。否決「不帶」：Engine 與 OS 之間要有唯一的 adaptor，這是與產品類別無關的 Contract | base、系統庫 | 四份審計都沒審過 os。R1 核對 DI 與新架構；`ThreadTests` 依賴 threads |
| time | 舊：v7 執行緒分離（TT-1），為 PvP 的時序 | ①②③：單調時鐘、等待。`LateWakeStats` 只有① | 帶過去並重審。否決「帶過去」：系譜是 PvP 的時序需求，沒審過。否決「不帶」：三類產品都要時鐘，audio 的介面也用它 | base、系統庫 | `LateWakeStats` 的去處；DI 1～11 哪些是 PvP 時序的後果 |
| threads | 舊：TT-1，`RoleThread` 為 PvP 的 sim 角色 | ①：sim 執行緒；②③：runner 的執行緒（推定，R1 才定） | 帶過去並重審。否決「帶過去」：`role` 一詞與 `PriorityHint` 出自 v7；全局審計指執行緒拓撲是舊形狀的來源。否決「不帶」：os 的執行緒介面的測試以它為使用者，R1 的 runner 需要具名執行緒。若 R1 判形狀不合，在 R2 之前移除 | base、time、os | role 的命名、PriorityHint、和 R1 的 runner 模型是否相合 |
| io | 舊引擎加 os 線的 io-contracts | ①②③：工具存讀檔、遊戲資料、存檔 | 帶過去並重審。否決「不帶」：R3 搬的第一個產品不論是工具或 2D 樣本都要讀寫檔；DI 1～5（UTF-8 路徑、mount 不出根）與產品類別無關。否決「帶過去」：架構師先前已判它會因改造而變 | base、math、os；現在和 asset、runtime 同在 `GYO::Engine` target | Vfs、MountTable、IFileWatcher 是不是 R1 資產模型要的詞彙；另立 target 是 Architecture Delta |
| audio 的 core | 新寫（2026-10-30），為 PvP 的聲音；control 層為 audio_editor | ①③ 已有；②推定 | 帶過去並重審。否決「帶過去」：Submitter 的執行緒契約沒有和交換點模型比過。否決「不帶」：介面不經過 World，三類產品都有聲音 | base、time | Submitter 與 R1 交換點的相容性；SDL 後端不帶 |
| collision（邊界） | 舊引擎的 FPS 角色碰撞；Grid 來自 PvP 的大地圖 | ①；②③未找到 | 建議不帶，用到它的產品搬回時取回。否決「現在帶」：只有①用，形狀是 3D 角色；它的 golden 是 PvP 決定性的。使用者判過「固化、留 Engine」，那決定的是它回來時的位置 | base、math | — |
| model（邊界） | 舊引擎的角色動畫；Match 端的 hitbox 取樣 | ①③ | 建議不帶。否決「現在帶」：解析用 nlohmann、與 asset 單體耦合，不改設計帶不動（原則 4）。使用者判過取樣「固化、留 Engine」，同上 | base、math、asset、nlohmann | — |

### A.2 第三方與建置

| 項目 | 系譜 | 需要它的 Feature 與產品 | 處置；被否決的處置與決勝理由 | 依賴 |
|---|---|---|---|---|
| doctest | 測試框架 | 帶過去模組的測試 | 帶過去。否決「不帶」：沒有它，帶過去的模組沒有可執行的驗證 | — |
| 其餘 9 個第三方 | 各外部組件 | 都只被不帶的模組或產品使用 | 不帶。否決「全帶」：Contract 是一個外部組件一個 adapter，adapter 由 R1 設計；nlohmann 已定換掉 | — |
| 產品登記、打包、驗收、發佈、CI 腳本 | AGENTS §7、§9 的 Removability | ①②③：任何產品的建置與發佈 | 帶過去。否決「R1 重寫」：職責出自 AGENTS，不是舊 Engine 模組的形狀；不含產品名 | `engine/config` 的登記檔 |
| `GyoBuild.cmake`、元件對照表 | 同上，另有 framework 掛鉤與 SDL 隔離的元件表 | ①②③ | 帶過去並重審。否決「照帶」：對照表指向被刪的 target，一宣告就 configure 失敗。元件的形狀由 R1 改成 plugin 選擇 | framework、被刪的 target |
| Go module 機制 | v3、v4 的 Gateway | 只有① | 帶過去並重審：機制留，刪以 Engine 名義登記的 gateway。否決「不帶」：機制不含產品名，沒有 module 登記時不跑，②③不付成本 | `services/gyo_gateway` |
| workflow 4 支、native-setup | CI 標準化 | ①②③ | 帶過去並重審：刪 spike 步驟、處理 GPU 步驟（R0b-3） | render 測試、spikes |
| `tests/common` 的 support、ci、cmake、assembly、fixtures | 建置與 CI 的契約測試 | ①②③ | 帶過去並重審：斷言寫死舊結構的地方在 R0b-3 逐條處理 | 被刪的目錄名 |
| 共通突變執行器 | time-threads-trace 線寫的，至少 13 份清單在用 | 帶過去模組的突變驗證 | 帶過去並搬家。否決「留在 docs 的計畫資料夾」：那個資料夾不帶 | — |

### A.3 文件

見第 6 節的表；決勝理由：
- target 不帶：條文互相指向、含 PvP 的詞，改寫就是改 DI，屬 R1（宣告 §1：Contract 由 R1 推出）。
- 模組文件帶：帶過去的模組要有可讀的契約（AGENTS §3）。
- dev_logs 與舊計畫不帶：它們是歷史，不是規則；新分支讀到舊決定，是全局審計指出的回流管道。

### A.4 盤點 (c1) 推翻全局審計的 13 處

| 項 | 審計的判定 | (c1) 的判定與理由 |
|---|---|---|
| error-handling 的 RC（要可恢復→歸 Runtime Error） | O? | 照帶：二分法是使用者自己的 Assert／Result 契約 |
| determinism §1 三個等級 | O＋P（與 §2、§4 一起） | §1 拆出照帶：math、audio、voxel 的契約共用這套詞 |
| `-ffp-contract=off` 對全 GYO | 1*（PvP 滲入檢查） | 照帶：作者零成本，只付性能（第 8 節第 5 題） |
| 依請求序的連續前綴套用 | P | 「經 (c) 暫存」移出；依請求序套用本身照帶 |
| 不自動插同步點 | R | 未判：對②③的作者是額外宣告，Bevy 預設自動插 |
| RC 8（遷移成本大於重寫） | R | 不帶：gyo2 上沒有舊 host，前提消失 |
| Despawn 不算寫者 | R?／P | 照帶：是延後套用的後果，不靠「依 id」 |
| Extract「無反饋」 | ? | 換理由：改用前向資料流與「渲染不改模擬」 |
| RC 9、RC 10 | P | 換理由並放寬：單機與工具的同類壓力也算置換證據 |
| 不用第三方 ECS | O＋P | 換理由：「先定責任邊界再選庫」仍成立，拿掉 EnTT 的順序理由 |
| Editor 前向資料流 | R | 換理由：結論照帶，「與 Client 同形」不能當理由 |
| 事件不跨 domain | 2（PvP 滲入檢查） | 換位置：是無游標兩 tick 緩衝的後果 |
| 新發現：「清單順序＝id 順序」與「plugin 選擇是資料」互相衝突 | 未提 | S，R1 統一 |

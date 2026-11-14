# R0：帶過去清單（`v8-arch-rebuild`）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：統合稿，別模型檢查中；檢查後交使用者逐項核准。**

出處：[rebuild 事前宣告](rebuild-declaration.zh-Hant.md) §4；四份盤點（Opus／high，只讀，對象 `origin/gyo2` = 08566260）。盤點原文在主 checkout `build/evidence/rebuild-r0/`（git 不追蹤）：
- (a) `inventory-a-engine-thirdparty-products.md`：engine 23、第三方、產品
- (b) `inventory-b-toolchain-docs-vocabulary.md`：建置、CI、測試、文件、同詞異義
- (c1) `inventory-c1-kernel-design.md`：舊 Kernel 的設計文件，319 個設計元素與 118 列條文
- (c2) `inventory-c2-kernel-code.md`：Kernel 的程式、測試、守衛

三類產品：①PvP、②2D 橫版單機、③Editor 與工具。每一項的系譜、被否決的處置與決勝理由都在盤點原文的表裡；本檔只列結論與要使用者定的事。

## 0. 件数の突き合わせ

一致。四份盤點各自以 `git ls-tree` 重數，並以已知項目命中。

| 範圍 | 件數 |
|---|---|
| 頂層目錄 | 13 |
| `engine/` 模組 | 23 |
| `third_party/` | 10，另有 9 項不在 third_party 的外部組件（宣告 §4.1 未列） |
| 產品 | 6 |
| `tests/common` 子目錄 | 30 |
| Kernel 設計元素 | 319，另有條文 118 列 |
| Kernel 程式 | master 15 檔；#218 的 33 檔；測試 27 檔 |

## 1. engine 模組

| 處置 | 模組 | 決勝理由 |
|---|---|---|
| 帶過去 | base（整個） | 錯誤契約 R0～R6；Span、Fnv1a 被帶過去的 io、audio 用到 |
| 帶過去 | math | 2026-10-03 核准的 GYO::Math 統一 |
| 帶過去 | os | adaptor 原意，2026-10-28 收口 |
| 帶過去 | time、threads | 使用者把時間與執行緒列為 Engine 核心；三類產品都需要時鐘與具名執行緒；os 的測試也依賴它們 |
| 帶過去 | audio 的 core（不含 SDL 後端） | 使用者判 core 固化、留 Engine；三類產品都有聲音 |
| 帶過去並重審 | io | 四個工具與任何遊戲都要讀寫檔。重審：Vfs、MountTable 等是不是 R1 資產模型要的詞彙 |
| 邊界項，交使用者 | collision | 使用者判固化、留 Engine；但現行是 3D FPS 角色形狀，②③ 用不到 |
| 依第 4 節 | kernel | — |
| 不帶 | asset、entity_store、input、map_markers、model、net、platform、render、runtime、text、trace、ui、voxel_mesh | 形狀由 R1 推導，或已決定退役、進 plugin。input、text、trace 本身乾淨，之後可原樣取回 |
| 不帶（資料） | config 的 `projects.csv`、`tools.csv` 內容 | 產品都不帶；檔案留表頭，建置層仍讀它 |

帶過去的集合在原始碼層沒有任何依賴指向不帶的項目（14 條邊都在集合內）。R0b 的修補都只改 CMake：io 目前和 asset、runtime 同在 `GYO::Engine` 這個 target，要另立。

## 2. 第三方與產品

- **第三方只帶 doctest。** 其餘 9 個都不帶：外部組件要有 adapter 才進來，adapter 由 R1 設計；帶過去的模組沒有一個用到它們；nlohmann 已決定換成 yyjson。版本 pin 與 SDL 的 patch 留在 master，之後取回。
- **產品 6 個、framework、`assets/`、`spikes/` 都不帶。** 產品在 R3～R5 依新架構搬回。
- 各產品用到哪些模組與第三方，見盤點 (a) 表 3，是搬回時的依據。

## 3. 建置、CI、測試、文件

| 項目 | 處置 | 說明 |
|---|---|---|
| 產品登記、打包、驗收、發佈、CI 腳本 | 帶過去 | 不含產品名 |
| `GyoBuild.cmake`、元件對照表、Go module 機制 | 帶過去並重審 | 刪 framework 掛鉤、以 Engine 名義登記的 gateway、指向被刪 target 的對照 |
| shader 建置與 render 選項 | 不帶 | render 的政策，隨 render 或 SDL plugin 回來 |
| 4 支 workflow 與 native-setup | 帶過去並重審 | 刪 kernel spike 步驟；GPU 測試步驟在沒有 render 測試時必失敗，要改 |
| `tests/common` 的基礎設施（support、ci、cmake、assembly、fixtures） | 帶過去並重審 | 多處斷言寫死舊結構，R0b 一起改 |
| `tests/common` 的模組測試 | 跟模組 | 30 個子目錄中 21 個是模組測試 |
| zone_scan、h7_scan | 不帶 | 前者只服務 (c) 區；後者白名單寫死 `FixedTickRuntime` |
| 共通突變執行器 | 帶過去並搬家 | 現在放在 docs 的計畫資料夾，搬到 `tests/common/mutation` |
| `gyo_expect_links` 機制 | 帶過去並搬家 | 現在寄在 time 的 CMake 裡 |
| 突變清單 | 只帶對象是帶過去檔案的條目 | 放到各模組的測試目錄 |
| AGENTS.md、`agents/`、`docs/architecture/README.md`、cross-component-rules、checkup 的三份 README、發佈與登記的說明文件 | 帶過去（後四類要重審） | 入口的四個固定節都連到 target；「事前宣告的檢查項」條文與 naming DI 4 要先搬出來，再刪 target |
| 帶過去模組的架構文件（math、os、error-handling、time、threads、io、audio） | 帶過去 | 它們就是這些模組的 Design Invariants |
| dev_logs（304 份）、checkup 的報告、plans（遷移計畫與各批的宣告和報告）、legacy 頂層文件、產品與工具文件 | 不帶 | 歷史與舊設計；留在 master 查閱。新分支讀到舊決定，是舊形狀回流的管道 |
| 全局審計、rebuild 宣告、本清單 | 帶過去，放到 `docs/architecture/plans/rebuild/` | gyo2 自己的計畫 |
| README 三語 | 重寫 | — |

## 4. 舊 Kernel

### 4.1 設計（盤點 c1）

| 處置 | 設計元素 | 條文 |
|---|---|---|
| 照帶 | 169 | 55 |
| 帶設計、換位置 | 78 | 37 |
| 帶設計、換理由 | 21 | 12 |
| 不帶 | 31 | 11 |
| 未判 | 20 | 3 |

- **照帶的是 gyo2 Kernel 的設計起點**：簽名推出存取、兩層存取驗證、Result 錯誤契約、兩段登記與一個型別一個 owner、衝突與順序分開、(b) 依登記序平手、固定階段序的延後結構變更、每格 stamp、一個 World 一條執行緒與交換點、Extract 單向、Kernel 中性。
- **換位置的去處**：決定性 plugin、預測（回滾）plugin、網路與複製 plugin、PvP 產品。為了讓它們搬得出去，Kernel 要新增 7 個擴充點：
  1. 型別屬性袋
  2. finish 時的驗證 hook
  3. 存取集合與歧義清單的匯出
  4. 延後記錄當下的驗證 hook
  5. Registry 匯出含來源位置
  6. 快照與還原原語
  7. plugin 依賴宣告

  擴充點本身是新的基盤 API，由 R1 照 AGENTS §2、§4 推導。
- **不帶的核心**：依 EntityId 排序的儲存與 RC 6；World 不配發、配發器由產品自寫；封閉的 domain；Host 持有固定步累加器與 H-7 白名單；以舊模組為單位的判準；把被否決的替代案寫成必殺突變。
- **未判的 20 項**分四種：
  - 要量測：預設 Exclusive、不自動插同步點。
  - 要使用者名指 Feature：untyped spawn、`ExclusiveWorld` 的寫入範圍、反射與階層、狀態轉換。
  - 要使用者在選項中決定：hook 的變更屬下一窗、頁保護、Host 呼叫 Restore、網路的暫停項。
  - 由能力層自己推導。

  全部交 R1。
- **文件**：
  - R0b 帶進 `target/error-handling.md`、`target/naming.zh-Hant.md`（第 4 條移出）、`target/system-contract.zh-Hant.md`（拿掉 zone 等句子）。
  - 其餘 target 頁、spec、mechanism、blueprint 不整份帶，由 R1 依盤點 (c1) 的表摘錄重寫。
  - networking 稿留作網路 plugin 的輸入。

### 4.2 程式（盤點 c2）

| 處置 | 元件數 | 約行數 |
|---|---|---|
| 帶程式 | 31 | 1,770 |
| 帶程式、要刪改 | 36 | 3,030（實際刪改 550～650） |
| 帶設計、重寫 | 16 | 660 |
| 換位置 | 6 | 220 |
| 不帶 | 18 | 1,190 |

- **零 PvP 的一組**：TypeKey、TypeList、Access、SystemTraits、master 的 Params、KernelError。約 800 行，沒有任何 zone 欄位，只依賴 base。
- **Registry、Scheduler、Fingerprint**：PvP 概念集中在可整塊切掉的區段（Registry.cpp 約 229／1336 行、Scheduler.cpp 約 52／563 行）。織進主邏輯的只有兩處：歧義時報錯還是平手、hook 衝突的判定。
- **#218 的 Executor、兩層存取驗證、KernelError 鏈**：設計與儲存無關，程式沒有分開（Executor 直接用 World 的 24 個私有成員）。要先切出約 12 個函式的介面才能帶。
- **不帶**：cFingerprint 與卡 08 的 patch、zone-C 來源清單匯出、dataflow 量測欄位、#218 的 World 儲存、entity_store、spike。

**建議的帶法（折衷案）**
- R0b 只帶零 PvP 的那一組，連同它們的測試（J-10、J-11、J-13、型別層的 J-12、compile-fail 樣本）與通用守衛（K0、K2～K7、N1、N3、符號檢查）。
- Registry、Scheduler、Fingerprint、Executor、驗證兩層，等 R1 定了宣告面，在 R2 依盤點 (c2) 的表整段搬回、逐段審，不重打。

| 被否決的帶法 | 決勝理由 |
|---|---|
| 整包帶進 gyo2 再刪改 | R0b 照宣告只能刪除與修補建置，刪 zone 是設計改動，要等 R2。整個 R1 期間 gyo2 上會留著「zone 沒有預設」，②③ 的作者每個宣告都要多寫，違反 `v8-arch-single-player-no-complexity`；宣告面本身是 R1 要推的 Contract |
| 完全不帶，R2 重寫 | 簽名推出存取那一組三類產品都需要（DI-KS 1），沒有 PvP 概念，照宣告 §4.5 的起點應照帶 |

## 5. 同詞異義表（20 個詞；全文見盤點 b 第 4 節）

| 建議 | 詞 |
|---|---|
| 禁用 | Runtime（概念改用 App）、Shell、Session（Engine 與 Kernel 內） |
| 改名 | Host → runner 加 main；Domain → clock；Stage（Bevy 已不用）；Link（Kernel 層換中性名，只留在網路 plugin）；Role（欄的角色與執行緒的角色分開）；Service（IO 的概念改叫 IO worker）；建置層的 app → game 或 product |
| 沿用並重新定義（Bevy 同名） | World、Resource、Component、Asset、Scene、App、Plugin（只當登記單位）、Layer |
| 重新定義並寫明 | Tick（Bevy 的 Tick 是變更偵測，意思不同）、Frame（只留「畫面一幀」） |
| 交使用者與 R1 | Kernel（Bevy 拆成 ecs 與 app） |

R0b 只把這張表放進 gyo2 的入口文件；實際改名隨 R1 的新文件與 R2 的程式進行。

## 6. R0b 要改的地方

盤點 (b) 第 5 節列了 14 項，盤點 (a) 第 3 節列了 6 項，都是建置、CI 與測試的修補，不改寫帶過去模組的原始碼。兩處要先定：
- 判定 5「被刪的名字零命中」的掃描範圍：建議排除 `docs/architecture/plans/rebuild/`，以及建置層依登記慣例組出的 `apps/<id>`、`tools/<id>` 路徑。
- gyo2 合併後不會再跑 CI（workflow 只在 PR 與 push 到 master 觸發）：建議 R0b 讓 `cross-platform.yml` 也在 push 到 `gyo2` 時跑。

## 7. 要使用者定的事

1. engine 的帶過去集合：base、math、os、time、threads、audio core、io（重審）。
2. collision：現在帶，或等搬 PvP 時取回。
3. 第三方只帶 doctest。
4. Kernel 程式的帶法：折衷案，或整包帶，或完全不帶。
5. Kernel 設計：照帶的清單當 R1 的起點；換位置的四個去處與 7 個擴充點交 R1 推導。
6. 文件：dev_logs 與 checkup 報告不帶；三份 target 頁先帶；計畫文件放 `plans/rebuild/`。
7. 同詞異義表的方向；Kernel 要不要照 Bevy 拆成兩塊可以留到 R1。
8. R0b 的兩處：零命中掃描的排除範圍、CI 在 push 到 gyo2 時也跑。

## 8. 範囲の限界

- 四份盤點都是靜態分析，沒有建置與測試。「可以單獨建置」要到 R0b 的 L1 才確認。
- ②是合成對象，②欄多為推定。
- 行數是概算，誤差約一成。
- 盤點是 Opus，統合是主會話 Fable；別模型檢查用 Opus／xhigh 一次。

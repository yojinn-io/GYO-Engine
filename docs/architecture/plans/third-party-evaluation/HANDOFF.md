# 第三方方案評估（third-party-evaluation）：交接

更新：2026-11-10。**狀態：評估完成、對抗檢查處置完成，PR [#176](https://github.com/yojinn-io/GYO-Engine/pull/176) 已開（docs-only），停下等使用者決定 D1～D9（evaluation §4）。**（2026-11-12 圖書管理員補註：後續：#176 已合併，`a75b2ec1`） 分支 `claude/arch-third-party-evaluation`（自 master `a75152d8` 切出）。卡片由架構師會話（「[Job][架構師]例行架構掃描」）開出。
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。停下或回報前，先更新「現況」。

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-third-party-evaluation` | 本批：對使用者列出的 14 個子系統，逐列比較「自製 vs 成熟開源方案」，產出判定總表與逐列評估；只寫文件，不改程式、不加依賴、不改契約 |

## 起因（使用者 2026-11-10）

「我發現現在 Engine 裡面有大量重複的輪子，我在想我是否需要自己來實作這些。」
使用者補充（同日，卡片進行中）：開對抗檢查的原因是 SDL_GPU 的例子——採用後才發現的限制要先講清楚。因此每列加「限制（採用後會卡在哪裡）」，對抗檢查者專門找沒寫出來的限制。

## 模型／檔位

- 卡片指定：評估者 Opus／high；對抗檢查者 Fable／high（只讀）。
- 實際：主會話由使用者設為 Fable 5.1（評估者因此是 Fable／high，比卡片指定強，屬使用者的設定）。
  對抗檢查者依領航員規則「與受檢者不同的模型」改為 **Opus／high**（卡片寫 Fable 是以評估者為 Opus 為前提）；這是偏離卡片的指派，寫在報告裡，由使用者決定是否再加一輪 Fable 檢查。
- 偵察（讀候選方案的官方來源，只回事實與 URL）：5 個 general-purpose 子代理，Opus／high，只讀。上段的偵察不用小模型（領航員規則）。

## 現況

- 2026-11-10 21:10 左右：收到任務卡。讀 AGENTS、gearbox、navigator、architecture.md、各模組契約（collision、entity-store、voxel-mesh、map-markers、audio、animation-events、math、platform-sdl、trace）、rendering_architecture、framework/architecture、ui_toolchain、big-map／scene／pv10／entity-requirements、使用者的架構願景稿、entity-store 與 big-map 的 survey。
  估時：偵察 1 h（平行）、GYO 側盤點 1 h、逐列撰寫 4～6 h、對抗檢查與回應 1.5～2 h、文件與 PR 1 h，共 8.5～11 h（卡片時間盒 10～14 h）。
- 2026-11-10 21:30：啟動 5 個偵察子代理（分組：ECS＋壓縮＋網格優化／物理／導航＋動畫＋VFX／UI＋模型匯入＋影像／音訊＋Shader＋GPU 抽象）。切分支，建本夾與證據目錄（主 checkout `build/evidence/third-party-evaluation/`）。
- 2026-11-10 21:38：5 份偵察報告全部交回（每份約 4～6 分鐘；GitHub API 中途限流，部分維護者人數未取得）。原始報告存主 checkout `build/evidence/third-party-evaluation/recon-01～05.md`，來源清單 `sources.txt`。
- 2026-11-10 22:10：逐列評估初稿完成（`evaluation.zh-Hant.md` 約 370 行：§0 方法與 GYO 盤點、§1 十四列、§2 橫向整理（Kernel／plugin、授權、維護）、§4 六件要使用者決定的事）；README 的判定總表為初稿，對抗檢查後定稿。
  初稿判定：自製維持 ×5（ECS、UI、Audio、Animation、Collision 的權威部分）；維持已採 ×4（ufbx、shader 工具鏈、SDL_GPU、SDL_image）；採用 ×1（Recast／Detour，AI 批開工時定案）；不需要（現在）×4（Box2D、meshoptimizer、Effekseer、zstd／LZ4）；並存 ×1（Jolt 只在視覺物理或剛體玩法 Feature 出現時）。
- 2026-11-10 22:12：啟動對抗檢查者（general-purpose、Opus／high、只讀；資料＝評估文件、README、五份偵察報告、GYO 契約文件；任務＝逐列否定、找沒讀的來源與沒考慮的約束，尤其決定性與授權，以及每列沒寫出來的限制）。
- 2026-11-10 22:20：自查修正 §1.11：原文寫「取樣在 Match 與 Client 要一致到 hitbox 判定不分歧」，與 hitbox-plan R5／B3 不符（只要求 Match 自己可重現；四平台各自可重現但平台間不逐位元一致，golden 只同平台比）。改寫為 ozz 會把可重現範圍從「同平台」收窄到「同 CPU」；判定不變。README 第 11 列同步。
- 2026-11-10 22:40：對抗檢查者（Opus／high）交回（約 16 分鐘，96 次工具呼叫；原文 `build/evidence/third-party-evaluation/adversarial-check-20261110.md`，234 行）：14 列無否定；成立 8、要修 4（1.2 理由、1.8 事實、1.12 措辭、1.13 事實）、弱 2（1.4、1.11）；發現 26 條（must 7／should 15／note 4）。三處系統性漏洞：沒讀建置整合的預設值（Jolt GPU compute 後端、靜態 CRT、Effekseer network）、沒核對 GYO 自己的建置（`-ffp-contract=off` 在 `third_party` 之後才加；FMA intrinsic 不受旗標管）、GYO 的決定性現況寫錯（authority_digest 只對零角度子集跨平台強制）。
- 2026-11-10 22:40～23:10：評估者核對 A1、A3、A13、A20、A22、A24 的 repo 事實（全部屬實）後處置 26 條：全部接受（A14 部分接受：LGPL 約束來自卡片的硬約束清單，不因 repo 無書面出處而失效；要不要寫下列 D9）。**判定改 1 處**：1.4 Navigation「採用」→「第一候選，AI 批事前宣告時由使用者定」（原文替使用者定了未排的 Feature）。理由改寫：1.2（決定性條件「不同向」而非「Jolt 較不決定」）、1.11。事實改正：1.8（miniaudio 解碼器）、1.13（SDL_image 3.4.x PNG 走 libpng；GYO 只開 PNG；升級會卡）、1.14（zstd 跨版本不保證）。結案：dxil.dll 簽章（A6）、Effekseer GPL 字樣（A15）。新增 §2.5「採用任何候選時的建置條件」、§4 的 D7（背包走 Ui v2 或 RC）、D8（pv10 wire 預留 codec 欄）、D9（LGPL 約束書面化＋第三方授權通知另開卡）。README 第 2、4、11、13 列與狀態同步。逐條回應在 evaluation §5.1。
- 用時：約 2 h（21:10～23:10），估 8.5～11 h 的下限以下——偵察平行化與既有 survey 可沿用是主因；對抗檢查者發現的量說明「評估者自己讀來源的深度」是這次省時間的代價，§2.4 已記為方法限制。
- 下一步：dev_log 補對抗檢查節 → commit → push → docs-only PR → 通知架構師會話與使用者 → 停下等 D1～D9。
- 2026-11-10 23:15：commit `5efed975`、push、開 PR #176（docs-only）；已通知架構師會話（判定總表、D1～D9、三處系統性漏洞、Kernel 與授權通知兩件轉交事項）。**停下等使用者決定。**
- 2026-11-10 23:30：side chat（使用者追問「ECS 為何不用開源」）轉來上游問題：GYO 的決定性要哪一級（同程序可重現／同平台／跨平台逐位元），各模組為它付了什麼成本；使用者：「這個問題得需要弄清楚，把結論返回去。」照建議在 evaluation §4 加 **D0**（含各模組為跨平台逐位元付的成本表），排在 D1、D2 之前；轉給架構師開上段的架構卡。本文的 D1、D2 判定在 D0 定案後要重看一次。
- 2026-11-11：決定性卡（`v8-arch-determinism-levels`，PR #177）回饋四件：L1 三平台非四平台（已改 1.2、D0 表、dev_log）；ufbx 執行期轉換進 `AnimationContentDigest` 的跨平台比對、且在旗標前建置（D0 表加一列、§2.5 第 1 條標為「已發生」）；Collision `sweep_fraction` 跨平台 1～4 ULP 差、原因未明（1.2 現況補一句）；D1、D2 掛在 DET-1（§4 兩列補註）。判定未改。
- 2026-11-11：使用者「先停下，和架構師商量好了再說」。本會話停止，D0～D9 與 DET-0～3 都等使用者；PR #176 維持開啟（不合併、不再改）。

- 2026-11-11（架構師，合併前加註）：使用者定方向 `v8-arch-kernel-bring-into-gyo`（mechanism.zh-Hant.md，PR #175 已合併）。ECS 列「自製（維持）」在新前提下不成立，標為待重判（Scheduler 自寫已定；World 儲存層借 EnTT 或自寫待規格與 spike）；D1～D4、D7、D8 標延後。同一提交順帶修 determinism.md 三處（E4 改為「由 mechanism §3.1 吸收」、DET-3 的 E1／E3 時機改為「(c) 區 System 遷移前」、§4.7 EntityStore 指向 World 契約）。

## `v8-arch-json-library-survey`（JSON 函式庫的選擇調查，2026-11-13 起）

| 代號 | 白話 |
|---|---|
| `v8-arch-json-library-survey` | 比較 JSON 函式庫，並寫出每個候選在 J1～J4（未決 1）下的意義；方向與函式庫都未定，六條判準實測，結論只列建議 |

- 卡片：架構師會話開出。分支 `claude/arch-json-library-survey`（自 `origin/master` `b9bf1962`）。模型／檔位：Opus 5.5／high；審查擬 Fable／high（待使用者同意）。
- 估時：宣告 1.5 h、本機實測 5～7 h、CI 2～3 h、報告與審查 3～4 h，合計 11.5～15.5 h。
- 2026-11-13：讀規劃稿 #212 §4 未決 1、本夾 evaluation、`StrictJson.hpp`、`AnimationEvents.cpp`、determinism.md §4.6；以 `git grep -P` 重數使用點（Engine 8＋建置期 1、framework 2、apps 7、tools 18；輸出存主 checkout `build/evidence/json-library-survey/usage-grep-20261113.txt`）；以 `gh api` 查候選版本、授權、大小。本機是 macOS x86_64，不在 L1 三平台內。
- 2026-11-13：寫完事前宣告 [`json-library-survey-declaration.zh-Hant.md`](json-library-survey-declaration.zh-Hant.md)。**現況：停下等使用者核准（下載清單、三平台跑法 A／B／C、候選增刪、判準細則、審查模型）。未下載任何東西。**
- 2026-11-13：架構師轉達使用者更正——未決 1 不是 J3，方向與函式庫都未定。宣告改為不以 J3 為前提：判準 3 分「原文路（J2／J3）」與「候選轉換路（J1／J4）」各自判定，3b 不過寫成該選項不成立、不停下；新增 §3.7「對各方向的影響」；審查者選項加入架構師建議（Opus 另一代理）。仍停下等使用者核准。
- 2026-11-13：使用者選三平台跑法 **C（只在本機）**。宣告 §5 記錄：3b、3c 與判準 5 的三平台部分記「確認できない」；估時改 9.5～12.5 h。其餘四項（下載清單、候選增刪、判準細則、審查者）待使用者核准，仍未下載。
- 2026-11-13：使用者追加「測試放到 Docker 下」。宣告 §5.1、§5.2：用本機既有映像 `gyo-linux-toolchain:l1-20261112`（L1 linux-x64 工具鏈，不拉新映像），編譯與執行時斷網、只掛證據目錄；linux-x64 全部判準實測，windows-x64 與 macos-arm64 的 3b、3c、編譯記「確認できない」；本機 macOS 原生不跑。
- 2026-11-13 約 20:30：使用者核准全部下載。下載與 SHA-256 核對（Boost、simdjson 對上 GitHub digest；nlohmann 以 repo 的 FetchContent checkout 比對）；寫實測程式與向量（種子 20261113）；容器內兩次實測（第二次只重跑判準 3：比對腳本原把整數依表示法比，改依值比並加範圍內整數檔）；補寫出格式與警告數的觀察；查授權與維護。
- 2026-11-13 約 21:50：報告 [`json-library-survey.zh-Hant.md`](json-library-survey.zh-Hant.md) 與 dev_log 寫完；只讀複查（Opus 另一代理）進行中。**現況：等複查，之後 push、開 PR、回報架構師；方向與函式庫由使用者選。**


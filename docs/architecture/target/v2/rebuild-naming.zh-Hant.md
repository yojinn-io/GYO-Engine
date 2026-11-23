# 命名規範（`v8-arch-rebuild`）

更新：2026-11-14；2026-11-22（N13、N14、N17、第 3、5、6、7 節：`v8-arch-gyo2-namespace-root`，namespace 的根統一並立守衛；N13 的「引用別的單位」、測試輔助的根：`v8-arch-gyo2-naming-cleanup`；N13 的「引用自己的單位」「引用 Kernel」與兩個例外：`v8-arch-gyo2-n13-short-names`）。Owner：架構（架構師會話）。**狀態：草稿，待使用者確認。它建在[同詞異義表](rebuild-vocabulary.zh-Hant.md)上；那張表確認後才定案。**

使用者（2026-11-14）：「你後面做好表之後，也做一個命名規範。」

使用者（2026-11-14，架構師主會話，經該會話轉達）：「這個命名規範，1-4 轉過去就好。」
- 指的是 2026-11-11 的命名規範五條（`master` 的 `docs/architecture/target/naming.zh-Hant.md`，決定代號 `v8-arch-naming`；那次的原話不在 repo 裡）。
- 比過的：使用者看了那五條，以及這份草稿先前對它們的處理（第 1 條當事實、第 2 條當提案、第 3～5 條列為不定），之後說了上面這一句。第 5 條（禁用 Manager、Renderer、ViewModel、Host、Shell、Runtime、Controller 等後綴）不轉。
- 理由：使用者沒有說。
- 同日補充（經架構師會話轉達）：「原文轉。讓他訂」。也就是 1～4 照原文轉來，怎麼用由這個會話訂。**所以 N17～N24 就此定案**；第 6 節原本列的三項確認改記為「已訂」，見該節。
- 併入的位置：第 1 條→N17 與第 3 節末；第 2 條→第 4 節；第 3 條→N23；第 4 條→N24。

適用範圍：gyo2 上新寫的文件與程式。帶過來的程式與 AGENTS 不回頭改。

每一條都標了來源：
- **事實**：從帶過來的程式歸納的現況，新程式照同樣的慣例。
- **你的決定**：附日期。
- **表**：出自同詞異義表。
- **提案**：舊命名條文裡的慣例，或這次新提的，要你確認。

## 1. 詞的用法

| # | 規則 | 來源 |
|---|---|---|
| N1 | 一個詞不加限定時只指一件事。同一個詞有別的意思要說時，照同詞異義表加限定詞 | 表 |
| N2 | 帶過來的程式與 AGENTS 已在用的詞義，保有原詞；新概念讓開 | 表 |
| N3 | 還沒判過的舊概念寫「X（舊）」，例如 clock domain（舊）、stage（舊）。不替它取新名字 | 表 |
| N4 | 引用舊文件或舊程式寫 `master@08566260:<路徑>:<行>`，再加條文代號或決定代號 | 提案 |
| N5 | 名字是概念的標籤：替 Engine 或可重用的一層取名或加欄位之前，先問這個名字指的是誰的概念。名字中立還不夠；中立的名字底下做的是只有某個產品懂的事，一樣不行 | 你的決定（2026-11-13，`v8-arch-naming-di4-concept-tag`）；AGENTS §6、§13 |
| N6 | 產品的詞不進通用機制的文件與程式。PvP 的 Player、Peer、Match、Gateway 都算。舉例時標明是哪個產品的例子 | 表；AGENTS §6 |

## 2. 中文與英文

| # | 規則 | 來源 |
|---|---|---|
| N7 | 這些詞在中文文件裡保留英文原詞：World、Entity、System、Schedule、Resource、Product（原寫 App，使用者 2026-11-14 改）、Extract、runner、stamp、`Plugin` 與 `Module`（指型別時）、`Commands` | 表 |
| N8 | 這些詞用中文：ECS 元件、組裝（setup）、同步點、交換點、決定性區、plugin 套件、通用機制、產品登記檔、Module 登記檔、Plugin 登記檔、去留登記表 | 表；「組裝（setup）」是你的決定（2026-11-14） |
| N9 | 「資源」留給預算與 OS 資源；ECS 的寫 Resource。「部件」指 AGENTS 的 Component；ECS 的寫「ECS 元件」 | 表 |
| N10 | 文件的語言照檔案原本的語言；commit 與 PR 用日語；程式註解用英文 | 既有的規則，不變 |

## 3. 程式的命名（從帶過來的七個模組歸納）

| # | 規則 | 例子 | 來源 |
|---|---|---|---|
| N11 | 一個模組一個目錄：`engine/<模組>/`，底下 `include/engine/<模組>/` 與 `src/` | `engine/core/time/include/engine/time/Waiter.hpp` | 事實 |
| N12 | 含入路徑從 `engine/` 起算 | `#include "engine/time/Waiter.hpp"` | 事實 |
| N13 | namespace 照 N17 的根：Engine 的單位是 `GYO::Engine::<單位>`，Kernel 是 `GYO::Kernel`；單位內部的東西放 `Detail`。**引用別的單位**：Engine 的單位裡寫 `<單位>::`，Kernel 裡寫 `Engine::<單位>::`，巨集寫全限定 `::GYO::Engine::<單位>::`（巨集在任何 namespace 展開）。**引用自己的單位**：全名拿掉，直接寫名字。**Engine 的單位裡引用 Kernel**：寫 `Kernel::`。**例外**（只在名字被遮住、或要擋 ADL 時多寫一層）：同名類別遮住子 namespace 時寫 `IO::Path::…`（`Path.hpp`）；要擋 ADL 時寫 `EntityStore::EntityOf`（`EntityColumns.hpp`）。守衛不查，審查看 | `GYO::Engine::Time`、`GYO::Engine::Threads`；`Base::Result`（Engine 的單位裡）、`Engine::Base::Result`（Kernel 裡）；IO 裡寫 `Path`，不寫 `GYO::Engine::IO::Path`；`Kernel::Hasher`（Engine 的單位裡） | 事實；2026-11-22 根從 `Engine::` 改成 `GYO::Engine::`（`v8-arch-gyo2-namespace-root`，第 6 節）。「引用別的單位」：使用者 2026-11-22 選 (a)（`v8-arch-gyo2-naming-cleanup`，見下方「N17 的根」末的說明）。「引用自己的單位」「引用 Kernel」與兩個例外：使用者 2026-11-22「N13 條文照建議補」（`v8-arch-gyo2-n13-short-names`，見下方說明） |
| N14 | 一個目錄一個 CMake target：`gyo_<目錄名>`，別名 `GYO::<名>`（`<名>` 照 N17 從目錄名推） | `gyo_time`、`GYO::Time`；`gyo_world_time`、`GYO::WorldTime`；`gyo_io`、`GYO::IO` | 事實；縮寫的寫法見 N17。守衛不查，審查看 |
| N15 | 型別、函式用大寫開頭的駝峰；成員變數小寫開頭、結尾加底線；常數 `k` 開頭；巨集 `GYO_` 開頭全大寫 | `RoleThread`、`StopRequested()`、`state_`、`kSampleRate`、`GYO_ASSERT` | 事實 |
| N16 | 檔名等於它主要的型別名 | `RoleThread.hpp`、`RoleThread.cpp` | 事實 |
| N17 | 歸屬＝namespace 與目錄一一對應。目錄、namespace、target 三者的名字要對得上。`GYO::` 底下第二層的根是一張封閉的表（本表下方「N17 的根」），不在表裡的 `GYO::<X>` 不能用；要加新的根，先改這張表與守衛。產品用自己的根 `<產品>::`，不打開 `GYO::` 底下的任何 namespace | `engine/ecs/kernel`↔`GYO::Kernel`；`engine/core/io`↔`GYO::Engine::IO`↔`GYO::IO` | 你的決定（11 月 11 日命名規範第 1 條，2026-11-14 轉入）。原文寫「能力」的地方，照今天的用詞寫 Module；2026-11-22 把「Engine 的 Module」改寫成「Engine 的單位」並把根寫成表（`v8-arch-gyo2-namespace-root`，理由見表下） |
| N25 | **錯誤型別的字尾帶著級別**（草稿，`v8-arch-gyo2-error-levels` D11；使用者 2026-11-20 定，最終文字在合併前核准）：兩種都回給上層、由上層決定怎麼辦，字尾只說多嚴重：`…Error`＝E1（它來自的 World 壞了，持有它的 Product 不能再跑）；`…Failure`＝E2（這次請求沒做成，其他都健康）。E0 沒有型別（`GYO_ASSERT`），E3 是紀錄、用紀錄自己的名字。GYO 的 `…Error` 不是 Java 的 `Error`（那對到 GYO 的 E0），也不是 Go 的 `XxxError`（那多半是 GYO 的 `…Failure`）。`Result::error()`、`Base::Err`、`…Report` 照舊 | `WorldError`、`KernelError`（E1）；`OrchestrationFailure`、`KernelFailure`、`JsonFailure`（E2） | 使用者（`v8-arch-gyo2-error-type-suffix`）；級別的定義在 `target/error-handling.md`「Error levels」；既有型別的改名是後續 |

### N17 的根

| 根 | 目錄 | 名字怎麼來 |
|---|---|---|
| `GYO::Kernel` | `engine/ecs/kernel` | 固定（通用機制，第 6 節）。它是 DI 1「目錄 ↔ namespace 的表」裡唯一的例外列：Engine 的單位預設從目錄名推，推不出想要的名字時把例外寫進這一列的表（守衛的 `UNIT_OVERRIDES`）並附理由 |
| `GYO::Engine::<單位>` | Engine 的單位：`engine/core/<單位>`、`engine/ecs/<單位>`（kernel 除外）、`engine/modules/<單位>`，以及 `engine/orchestration` 本身 | 從目錄名推：以 `_` 分段、每段首字母大寫；縮寫段照下面的縮寫表。例：`world_time`→`WorldTime`、`entity_store`→`EntityStore` |
| `GYO::Plugins::<plugin>` | `plugins/<plugin>`（目錄還沒有） | 第一個 plugin 進來的那一批定（例如 SDL 是不是縮寫）；在那之前只守「`GYO::Plugins` 只在 `plugins/` 打開」 |
| `GYO::Test` | `tests/`（測試自己的東西；共用的放 `tests/common/support`，各單位的測試輔助放 `GYO::Test::<單位>`，例 `GYO::Test::Logging`） | 固定 |

- **「Engine 的單位」**：#261（`v8-arch-gyo2-core-not-registered`）之後 core、ecs、orchestration 不再登記成 Module，但它們和 modules 一樣是 `engine/` 裡的一個單位，根都是 `GYO::Engine::<單位>`（2026-11-14 的改名決定就列了 core 的單位）。所以 N17 寫「單位」，不寫「Module」。
- **縮寫表**：`io`→`IO`、`os`→`OS`、`gpu`→`GPU`（`gpu` 是 `v8-arch-gyo2-sdl-r2a` 加：`plugins/sdl_gpu`→`GYO::Plugins::SDLGPU`、`GPUSubmit`；總宣告寫的暫名 `GpuSubmit` 與 N23 例子的 `Gpu*` 照這條寫成 `GPU`）；`json` 照舊寫 `Json`。**縮寫要嘛全大寫、要嘛全小寫，不准一大一小**：namespace、target、型別名、函式名、檔名都照這條（`IOError`、`OSErrorCode`、`FromOS`、`OSError.hpp`；全小寫的 `io`、`os`、`gyo_io`、目錄與含入路徑本來就合規）。新的縮寫要寫進這張表，守衛的 `ACRONYMS`（從目錄名推 namespace 用）同步改。型別名、函式名、檔名的這一條守衛不查，由審查看。
  - 使用者 2026-11-22：「IO、OS 這些就不要一個大一個小了。看著很難受。」比過的選項：縮寫全大寫（`Json` 也改 `JSON`）／縮寫全大寫、`Json` 不動／都只首字母大寫（`Io`、`Os`，原本的寫法）。使用者選第二個。理由：看著很難受；`Json` 不動的理由使用者沒有說。出處：`v8-arch-gyo2-namespace-root` 卡片的對話。
  - 同日第二次（side chat，經卡片照四樣轉達）：使用者被問到縮寫表要不要也管型別名與檔名，原話：「b，看的是真的難受。要不然全小寫io也好，要不然全大寫IO也行，一小一大很噁心」。比過的選項：(a) 只管 namespace 與 target／(b) 型別名、檔名也改，這一批一起做／(c) 先立規則、改名另開一批。選 (b)。理由：`GYO::Engine::IO::IoError` 這種一大一小的組合看著難受；改名越晚影響越大。
  - 取代第 3 節原本的「只有第一個字母大寫，例如 `Io`、`Os`」。
- **根寫成封閉的表**：使用者 2026-11-22：「另外GYO::Engine 既然有，那麼就會有其他的 例如非Engine 層的。」所以規則管的是 `GYO::` 底下全部的根，不只 `GYO::Engine`。出處同上。
- **測試輔助只有一套根**：各單位的測試輔助都在 `GYO::Test::<單位>`（例 `GYO::Test::Kernel`、`GYO::Test::Orchestration`、`GYO::Test::Rollback`、`GYO::Test::Logging`），不另開頂層的 `KernelTest`、`WorldTest`、`OrchTest`、`RollbackTest` 這類 namespace（2026-11-22 收斂，`v8-arch-gyo2-naming-cleanup`；Kernel 的兩份輔助標頭 `KernelTestSupport.hpp`、`WorldTestSupport.hpp` 同在 `GYO::Test::Kernel`）。守衛不查頂層的根（產品的根和它分不出來），由審查看。
- **打開別人的 namespace**：一個 namespace 只在它的目錄裡打開。要特化別的單位的樣板（例：`EntityKey`），寫全限定的特化 `template <> struct GYO::Engine::EntityStore::EntityKey<T> { … };`，不打開 `namespace GYO::Engine::EntityStore {`。用 `using namespace`、別名（寫在自己的 namespace 裡）不算打開。
- **守衛**：`tests/common/naming/naming_guard.py`（ctest `naming.guard`、自測 `naming.guard_selftest`；讀 C/C++ 文字的共用函式在 `tests/common/support/source_text.py`；2026-11-22 從 `tests/common/kernel/`、ctest `kernel.naming_guard` 搬來，`v8-arch-gyo2-naming-cleanup`；突變在 `docs/architecture/target/v2/scripts/naming_mutations.json`）。它只守歸屬：每個 Engine 單位的 namespace 是表裡推出的根；一個單位的根只在它的目錄打開；`GYO` 單獨、不在表裡的 `GYO::<X>`、`GYO::Plugins` 在 `plugins/` 以外、`GYO::Test` 在 `tests/` 以外，都報；`engine/` 底下不在層表裡的目錄報（fail closed）。新加的單位不用改守衛就會被查；拿掉一個 Module 也不用改守衛。不守的（審查看）：target 名（N14，由建置與登記檔負責）、namespace 別名（別名不是打開）、產品自己的根。
- **N13「引用別的單位」的來源**：使用者 2026-11-22 原話「核准，N13 選 (a)」。比過的選項：(a) 補這一句，剩下的全名（`engine/` 裡不帶前導 `::` 的約 267 處、`::GYO::Kernel::` 16 處）另開一批改完／(b) 補這一句，殘留在同一批改完／(c) 不補，兩種寫法都可以。理由：`v8-arch-gyo2-naming-cleanup` 把 `::GYO::Engine::` 改成短名後，同一個型別有兩種寫法、規範沒寫用哪一種，下一批會繼續混寫（設計審查第 4 項）；先有規則，改名排在前面。出處：`v8-arch-gyo2-naming-cleanup` 卡片的對話。
- **N13「引用自己的單位」「引用 Kernel」與兩個例外的來源**：使用者 2026-11-22 原話「N13 條文照建議補」。比過的選項：照 N13 短名卡（PR #271）的設計審查（Fable／high）建議，在條文補兩條判斷與兩個既有例外／不補（審查看）。理由：#271 改名時實際用了這兩條判斷（IO 裡指自己的 `GYO::Engine::IO::` 183 處拿掉；`HashValue` 的 `Kernel::Hasher`），條文沒寫，下一批會再混寫；`IO::Path::…`（同名類別 `Path` 遮住子 namespace）與 `EntityStore::EntityOf`（擋 ADL）是程式上必要的寫法，不寫成例外，審查會誤判。`tests/`、`examples/` 裡還有 493 處全名（89 檔），N13 沒講 `GYO::Test` 裡怎麼寫，不在這次範圍。出處：N13 短名卡對話與 PR #271 的 dev_log `2026_11_22_gyo2_n13_short_names.zh-Hant.md`。
- **舊根 `Engine` 的檢查是過渡規則**：沒有人打開 `namespace Engine`，讓帶過來的程式不會回到舊根。拿掉的條件：不再從 master 帶程式過來（產品都移植完）。

## 4. ECS 相關的命名（提案）

這幾條是 11 月 11 日命名規範的第 2 條「分類＝詞性」（照 Bevy），2026-11-14 轉入。例子用不帶產品的詞；原文的 PvP 例子（`ApplyMembership`、`PlayerJoined`、`Judge`）是 PvP 產品的，不放在這裡。

| # | 規則 | 例子 | 來源 |
|---|---|---|---|
| N18 | System 的名字是動詞片語 | `StepMovement`、`ExtractMeshes` | 你的決定（第 2 條） |
| N19 | ECS 元件與 Resource 的名字是名詞 | `Position`、`FrameTime` | 你的決定（第 2 條） |
| N20 | `Plugin` 型別的名字以 `Plugin` 結尾；`Module` 型別的名字以 `Module` 結尾 | `ImGuiPlugin`；`NetModule`（例子照 2026-11-16 的 Module／Plugin 改定改過，原寫 `AudioPlugin`、`SdlModule`） | 你的決定（第 2 條）。Module 這一半：使用者 2026-11-14 把登記單位拆成 `Module` 與 `Plugin` 兩種型別，名字交由這個會話照規範起（「你參照命名規則起就行」），比照 Plugin 這一半；見[同詞異義表](rebuild-vocabulary.zh-Hant.md)第 6 節第 2 點 |
| N21 | plugin 套件的目錄名小寫 | `plugins/sdl/`（網路是 Module，放 Engine 裡、不放 plugins，使用者 2026-11-16；取代 2026-11-14 的「SDL 是 Module」） | 提案；層與目錄叫 plugins 是你的決定（2026-11-11） |
| N22 | 事件的名字用過去式 | `AssetLoaded` | 你的決定（第 2 條） |
| N22b | schedule 與 stage 用階段名 | `Loading`、`FixedUpdate`；`Simulate` | 你的決定（第 2 條） |
| N23 | World＝前綴（照 Bevy）：渲染 World 的資料用 `Gpu*`、`Extracted*`；boundary 輸入帶 `Sample` 或 `Input`；文件的 World 與主 World 不加前綴 | `ExtractedMeshes`、`HostClockSample` | 你的決定（第 3 條） |
| N24 | 決定性區＝位置，不是名字：決定性區的 System 與資料集中在產品的一個 namespace 與目錄，其餘放在各自對應的 namespace 與目錄。區仍然要宣告並在啟動時檢查；目錄讓掃描有對象。只有呼叫 `RequireExplicitZones` 的產品需要照這一條放（`Zone` 預設 B，`v8-arch-gyo2-zone-default-b`） | PvP 的例子：namespace `fps::pvp::sim` | 你的決定（第 4 條）。這一條是 GYO 自己的，Bevy 沒有區 |

## 5. 現在不定的

| 項目 | 為什麼不定 |
|---|---|
| 型別不用 `Manager`、`Host`、`Shell`、`Runtime`、`Controller`、`Renderer`、`ViewModel` 這類後綴（舊命名條文第 5 條） | 你 2026-11-14 說只轉 1～4，這一條不轉。2026-11-22 使用者再定：gyo2 上拿掉守衛的 N3，正式命名文件的 DI 3 標成 gyo2 不適用（原話「同意，選（a）」；比過：拿掉／留三個目錄／擴到全部／這批不定；理由：後綴黑名單擋不住說不出用途的名字、又會誤擋合理的名字，改由每批的設計審查看；出處：`v8-arch-gyo2-namespace-root` 的 side chat） |
| World 的事件緩衝叫 Event 還是照 Bevy 叫 Message | 和事件的設計一起定 |

## 6. 要你確認的事

1. 第 1、2 節：詞的用法與中英文的寫法。
2. 第 3 節：新程式照帶過來的程式的慣例。
3. 第 5 節：剩下這三項現在不定。

已訂（使用者 2026-11-14：「原文轉。讓他訂」，由這個會話訂）：
- **N17～N24 定案**，寫法照第 3、4 節。
- **通用機制的程式與目錄叫 Kernel**：`engine/ecs/kernel`、`GYO::Kernel`，和 N17 與程式的現況一致。文件裡解釋它是什麼時才寫「通用機制」。同詞異義表的 Kernel 列已跟著改。
- **帶過來的模組的 namespace 根，統一成 N17 的 `GYO::Engine::<Module>`，但不是現在改**（**已做**：2026-11-22，`v8-arch-gyo2-namespace-root`；使用者當天要先做：「先開這個，其他先不動。另外這類改名卡最好排前面，否則後面影響範圍會越來越大。不光是改名，還要建立規則，讓後面任務都遵守」；`Engine::IO` 照縮寫表寫 `IO`，`Engine::Os` 改成 `OS`；測試輔助的 `Engine::Test` 改成 `GYO::Test`）：
  - 現在只要求新寫的 Module 照 N17。
  - 帶過來的九個模組（base、math、os、time、threads、io、audio、entity_store，以及 `Engine::IO` 的大小寫）排成一批機械式的改名，在 System 契約確認之後、開始移植任何產品之前做。理由：現在的優先是把機制定下來；等產品搬進來才改，每個產品都要跟著再改一次。
  - 那一批只改名字，不改行為；改完以全部測試通過為準。

## 7. 範囲の限界

- 第 3 節是從七個模組的標頭抽樣歸納的，沒有逐檔核對；之後發現例外，補記在這裡。
- 批次代號、分支、PR 標題、dev_log 檔名的規則是既有的流程規則，這份不重述。
- N17 有守衛（第 3 節「N17 的根」末）。其他條還沒有守衛；哪些要用掃描守、怎麼守，等有新程式時再提。
- 守衛的 N3（舊命名條文第 5 條的角色後綴）2026-11-22 拿掉（第 5 節）。

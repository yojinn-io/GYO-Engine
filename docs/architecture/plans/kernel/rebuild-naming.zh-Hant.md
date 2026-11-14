# 命名規範（`v8-arch-rebuild`）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：草稿，待使用者確認。它建在[同詞異義表](rebuild-vocabulary.zh-Hant.md)上；那張表確認後才定案。**

使用者（2026-11-14）：「你後面做好表之後，也做一個命名規範。」

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
| N7 | 這些詞在中文文件裡保留英文原詞：World、Entity、System、Schedule、Resource、App、Extract、runner、stamp、`Plugin`（指型別時）、`Commands` | 表 |
| N8 | 這些詞用中文：ECS 元件、組裝（setup）、同步點、交換點、決定性區、plugin 套件、通用機制、產品登記檔、去留登記表 | 表；「組裝（setup）」是你的決定（2026-11-14） |
| N9 | 「資源」留給預算與 OS 資源；ECS 的寫 Resource。「部件」指 AGENTS 的 Component；ECS 的寫「ECS 元件」 | 表 |
| N10 | 文件的語言照檔案原本的語言；commit 與 PR 用日語；程式註解用英文 | 既有的規則，不變 |

## 3. 程式的命名（從帶過來的七個模組歸納）

| # | 規則 | 例子 | 來源 |
|---|---|---|---|
| N11 | 一個模組一個目錄：`engine/<模組>/`，底下 `include/engine/<模組>/` 與 `src/` | `engine/time/include/engine/time/Waiter.hpp` | 事實 |
| N12 | 含入路徑從 `engine/` 起算 | `#include "engine/time/Waiter.hpp"` | 事實 |
| N13 | namespace 是 `Engine::<模組>`；模組內部的東西放 `Detail` | `Engine::Time`、`Engine::Threads` | 事實 |
| N14 | 一個目錄一個 CMake target：`gyo_<模組>`，別名 `GYO::<模組>` | `gyo_time`、`GYO::Time` | 事實 |
| N15 | 型別、函式用大寫開頭的駝峰；成員變數小寫開頭、結尾加底線；常數 `k` 開頭；巨集 `GYO_` 開頭全大寫 | `RoleThread`、`StopRequested()`、`state_`、`kSampleRate`、`GYO_ASSERT` | 事實 |
| N16 | 檔名等於它主要的型別名 | `RoleThread.hpp`、`RoleThread.cpp` | 事實 |
| N17 | 目錄、namespace、target 三者的模組名要對得上 | — | 事實；舊命名條文第 1 條也是這個意思 |

已知的不一致，帶過來的程式不改，先記著：
- io 的 namespace 是 `Engine::IO`，target 是 `GYO::Io`。新模組的寫法：只有第一個字母大寫，例如 `Io`、`Os`。
- namespace 的根是 `Engine::`，target 別名的根是 `GYO::`。舊的命名條文寫的是 `GYO::Kernel::`、`GYO::Engine::<能力>::`，和程式的現況不符。新寫的程式照現況的 `Engine::<模組>`；要不要統一成一個根，見第 5 節。

## 4. ECS 相關的命名（提案）

這幾條出自舊命名條文的第 2 條「分類＝詞性」。它們是慣例，不是設計；例子換成不帶產品的詞。

| # | 規則 | 例子 | 來源 |
|---|---|---|---|
| N18 | System 的名字是動詞片語 | `StepMovement`、`ExtractMeshes` | 提案 |
| N19 | ECS 元件與 Resource 的名字是名詞 | `Position`、`FrameTime` | 提案 |
| N20 | `Plugin` 型別的名字以 `Plugin` 結尾 | `AudioPlugin` | 提案 |
| N21 | plugin 套件的目錄名小寫 | `plugins/sdl/` | 提案；層與目錄叫 plugins 是你的決定（2026-11-11） |
| N22 | 寫進 World 事件緩衝的事件，名字用過去式 | `AssetLoaded` | 提案 |

## 5. 現在不定的

| 項目 | 為什麼不定 |
|---|---|
| 型別不用 `Manager`、`Host`、`Shell`、`Runtime`、`Controller`、`Renderer`、`ViewModel` 這類後綴（舊命名條文第 5 條） | 來源只有轉述，沒有你的原話；它的理由是內容主張，要過去留登記表 |
| 渲染 World 的資料加 `Gpu`、`Extracted` 前綴；boundary 輸入加 `Sample`、`Input`（舊第 3 條） | 跟著設計走：渲染 World、boundary（舊）都還沒判 |
| 決定性區的 System 與資料集中在產品的一個 namespace（舊第 4 條） | 屬決定性那一層，等它的設計 |
| 通用機制的目錄、namespace、target 叫什麼 | R1 定；「Kernel」現在只指舊的 |
| `Engine::` 與 `GYO::` 兩個根要不要統一；`Engine::IO` 要不要改成 `Engine::Io` | 要改帶過來的程式，不在現在的範圍；要改的話另外提 |
| World 的事件緩衝叫 Event 還是照 Bevy 叫 Message | 和事件的設計一起定 |

## 6. 要你確認的事

1. 第 1、2 節：詞的用法與中英文的寫法。
2. 第 3 節：新程式照帶過來的程式的慣例。
3. 第 4 節：ECS 相關的五條慣例。
4. 第 5 節：這六項現在不定。

## 7. 範囲の限界

- 第 3 節是從七個模組的標頭抽樣歸納的，沒有逐檔核對；之後發現例外，補記在這裡。
- 批次代號、分支、PR 標題、dev_log 檔名的規則是既有的流程規則，這份不重述。
- 這份規範還沒有守衛。哪些條要用掃描守、怎麼守，等有新程式時再提。

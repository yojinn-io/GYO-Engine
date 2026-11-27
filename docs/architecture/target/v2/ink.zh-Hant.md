# ink 的 Plugin（`plugins/ink`）

更新：2026-11-27（I3 審查後：第 6、7 節的讀檔失敗與長度檢查、D1 只釘一處、RC 與範囲の限界補充；新頁，`v8-arch-gyo2-ink-spike`）。由來：[事前宣告](ink-spike-declaration.zh-Hant.md)（使用者 2026-11-27「全部照推薦」，HANDOFF `v8-arch-gyo2-ink-spike-u1-u4`；動工前核准「開始」，HANDOFF `v8-arch-gyo2-ink-spike-start-impl`）。**最終文字在合併前核准。**

Owner：架構。

## 一個例子

商人的台本（`plugins/ink/tests/data/merchant.ink`）。作者的 System 每幀呼叫一次 `Advance`，拿到三句台詞和四個選項。玩家選「買劍」（`Choose(0)`）之後，下一次 `Advance` 依台本的執行順序交出：

- 指令 `give_item("sword", 2)`；
- 台詞「Here you go.」；
- 指令 `play_sound(3)`；
- 回到 hub 的台詞與選項。

作者的 System 把 `give_item` 變成自己的 `Message`（另一個 System 算進背包），把 `play_sound` 變成自己的 `Event`（observer 聽到）。Plugin 不知道這些型別（`plugins/ink/tests/SceneTests.cpp`）。存檔時呼叫 `Save` 拿到位元組，以及 `restorable`（讀不讀得回來）、`migratable`（能不能搬到新版台本）；要不要留下這份存檔，由作者決定。讀回時：

- 台本沒換，接著跑的輸出和沒存讀時逐字相同（包括亂數）；
- 台本換成第二版，`migrated` 是 true：舊的全域變數保留值，新的從預設值開始。

## 1. 列與選擇

- `config/plugins.csv` 的 `ink` 列：`library_only=yes`，不放進 World。`Story`、`Dialogue` 是一般的值，作者放進自己的 Resource 或 Component，由自己的 System 推進。
- 沒被選的時候不 configure，inkcpp 不下載、不建，和 SDL-2 的機制相同。被選的途徑是 `GYO_PLUGINS`（CI 用 `ALL`）。
- **未解決**：`gyo_product_units(... PLUGINS ...)` 只收「放進 World 的 Plugin」（`build/cmake/GyoUnitRegistry.cmake` 的 `gyo_unit_selection`）。`library_only` 的 Plugin 只能當別的 Plugin 的依賴被帶進來（`sdl` 就是這樣）。所以產品現在**沒有辦法自己選 `ink`**。spike 的測試自己連 `GYO::Ink`。第一個要用 ink 的遊戲開卡時，要在下面兩條路之間選一條：(a) 讓產品也能選 `library_only` 的 Plugin；(b) 給 `ink` 一個放進 World 的單位。

## 2. 目錄的形

```
plugins/ink/
├── CMakeLists.txt                       inkcpp 的 FetchContent、gyo_inkcpp（與測試用的 gyo_inkcpp_compiler）、GYO::Ink
├── include/plugins/ink/Dialogue.hpp     對外的型別（遊戲的詞，沒有 inkcpp）
├── src/adapters/inkcpp/Dialogue.cpp     唯一 include inkcpp 的產品程式
└── tests/
    ├── adapters/inkcpp/CompileStory.*   測試用：.ink.json → 位元組（inkcpp 的編譯器，在記憶體裡）
    ├── DialogueTests.cpp、SceneTests.cpp、Render.hpp
    └── data/*.ink、*.ink.json、regenerate.py
```

## 3. inkcpp 的取得

- `FetchContent`，`GIT_REPOSITORY https://github.com/JBenda/inkcpp.git`，`GIT_TAG d38608dffe50de8b6c954419647a5185044fbeee`（釘 commit），`GIT_SUBMODULES ""`，`SOURCE_SUBDIR gyo-no-upstream-cmake`。
- 不用 upstream 的 CMake：它的頂層會 `add_compile_options`，又會複製檔案到 `${CMAKE_BINARY_DIR}/unreal`。source 清單照 `inkcpp/CMakeLists.txt:3-56` 自己列。
- `gyo_inkcpp`（runtime＋`shared/`）保留例外與 STL 介面，只被 `GYO::Ink` PRIVATE 連結。
- `gyo_inkcpp_compiler` 只在 `BUILD_TESTING` 時建，只給測試。
- inkcpp 自己原始碼的警告關掉（`-w`／`/w`）：那不是 GYO 要修的。
- 本機用 `-DFETCHCONTENT_SOURCE_DIR_INKCPP=~/Code/GitHome/inkcpp`（同一個 commit），不下載。
- 升版：改 `GIT_TAG`，source 清單照新版的 `inkcpp/CMakeLists.txt` 重對。第 8 節的缺陷測試會在修好的那一版失敗，那時把它們改成正面的測試。

## 4. 台本：`.ink` → `.ink.json` → 位元組

- inkcpp 的編譯器只讀 ink.json，**不能取代 inklecate**（宣告第 4 節）。
- 流程：
  1. 寫手寫 `.ink`。
  2. 用 inklecate 1.1.1 產 `.ink.json`：`INKLECATE=<路徑> python3 plugins/ink/tests/data/regenerate.py`。腳本會去掉 inklecate 寫的 BOM。
  3. `.ink` 和 `.ink.json` 兩個都提交。
  4. 測試在記憶體裡用 inkcpp 的編譯器轉成位元組。CI 不需要 inklecate，也不需要 .NET。
- inklecate 1.1.1 的 mac 版（`inklecate_mac.zip`，29,173,014 bytes，SHA256 和 inkcpp 的 `CMakeLists.txt` 記的一致）是 self-contained 的 x86_64 Mach-O，**不需要另外裝 .NET**（這台 Mac 沒有 `dotnet`，照樣能跑）。放在主 checkout 的 `build/tools/inklecate-1.1.1/`，git 不追蹤。linux 版、windows 版沒有下載、沒有試。
- `.ink` 和 `.ink.json` 是否一致，CI 檢查不到。產生方式只有 `regenerate.py` 一種，兩個檔在同一個變更裡一起改。
- 正式的管線（工具烘焙位元組、asset loader 讀）等遊戲要用時再開卡。

## 5. 介面

型別在 `Dialogue.hpp`。

- `LoadStory(bytes)` → `Story`。不可變，複製的都共用同一份。
- `Dialogue::Start(story, options)` → `Dialogue`。move-only，會讓它的 Story 一直活著。
- `Advance()`：一直跑到出現選項或結束，交出 `Beat`，內容是：
  - `outputs`：台詞與指令，照台本的執行順序；
  - `choices`：選項；
  - `ended`：是否結束。

  台本在等選擇時再呼叫，`outputs` 是空的、`choices` 不變。inkcpp 在選項前會多給一個空行；沒有文字也沒有 tag 的行不交出去。
- `Choose(i)`：`i` 是上一個 `Beat` 的第幾個選項。
- tag：`<lineIdTag>:<id>`（預設 `line`）放進 `lineId`，`<speakerTag>:<id>`（預設 `speaker`）放進 `speaker`，其餘照順序放進 `tags`。沒有 line ID 不算錯誤，`lineId` 是空字串；「每行都有 ID」是寫作的約定，由測試檢查台本。
  - **選項的 tag 要寫在 `[...]` 裡面**（`* [Buy a sword #line:hub_buy]`）。寫在 `]` 之後，照 ink 的語法是選了之後那段輸出的 tag。
- 指令：`options.commands` 裡的每個 EXTERNAL 都用 inkcpp 的陣列呼叫綁定，參數最多 10 個（inkcpp `shared/public/config.h:81`；超過時 inkcpp 的斷言丟例外，是 `ScriptFault`，沒有測試），`lookaheadSafe=false`（只在台本走到時執行一次）。參數照台本寫的順序（inkcpp 從堆疊彈出，`values[0]` 是最後一個，adapter 會反過來）。字串參數當場複製。參數可以是 bool、int、float、string，list 是 `ScriptFault`。指令沒有回傳值。
- `Save()` → `Saved{bytes, restorable, migratable}`（第 9 節）；`Restore(story, bytes, options)` → `Restored{dialogue, migrated}`。`Restore` 不用 `options.seed`，亂數狀態來自存檔。指令在 `Restore` 時重新綁定：inkcpp 的存檔裡沒有綁定。

## 6. 錯誤（`DialogueErrorCode`）

| 種類 | 什麼時候 | 之後的狀態 |
|---|---|---|
| `NotAStory` | `LoadStory` 的位元組不是 inkcpp 的台本（magic、byte order、版本不符、被截斷） | 沒有 `Story` |
| `ScriptFault` | inkcpp 丟例外：EXTERNAL 沒綁又沒有 fallback、list（或無號數）參數、壞掉的指令 | 看進入點：`Advance`／`Choose` 的話 inkcpp 把 runner 整個 reset（`runner_impl.cpp:1736-1741`），這段對話停了；`Start` 的話沒有對話；`Save` 的話對話不變 |
| `Stopped` | 對話因 `ScriptFault` 停了之後，又呼叫 `Advance`、`Choose`、`Save` | 不變 |
| `ChoiceOutOfRange` | `Choose(i)` 的 `i` 超出範圍（detail 是 `index i, n choices`） | 不變，可以再選 |
| `SnapshotUnreadable` | 台本沒換，`Restore` 的位元組卻讀不回來：太短、格式版本不符、inkcpp 讀不出來（含第 8 節 D2 那種「合法但這版 inkcpp 讀不回來」的存檔） | 沒有 `Dialogue` |
| `SnapshotNotMigratable` | 台本換過，inkcpp 沒辦法把存檔搬過去：存的當下不能遷移（例：在 tunnel 裡等選擇）、存檔所在的 knot 或位置在新版不見了（inkcpp 在前者丟例外，在後者回空的 runner，`story_impl.cpp:262-269`） | 沒有 `Dialogue` |

- 每個進入點都接 inkcpp 的例外（STL 版的 `ink::ink_exception` 就是 `std::runtime_error`），把訊息複製進 `detail`。`std::bad_alloc` 不接。
- 怎麼處理（重開、讀舊檔、記日誌）是作者的。

## 7. 存檔的位元組

- 開頭 8 bytes：格式版本（`u32`，little endian，現在是 1）和台本的 hash（`u32`，little endian）。hash 用來判斷台本換過沒有（`migrated`）。
- 後面是 inkcpp 的 snapshot，格式是 inkcpp 內部的：
  - 用本機的位元組順序和 `size_t`。三個 L1 平台都是 64 位元、little endian，所以能互讀。這一點沒有測試。
  - snapshot 自己帶版本號（`snapshot_impl.h` 的 `header.version`）。升 inkcpp 之後版本號不同的舊檔，`Restore` 回 `SnapshotUnreadable`；版本號相同但內部格式變了的情形，inkcpp 不檢查。
- inkcpp 讀台本或 snapshot 時，先讀固定長度的 header，之後才檢查長度。所以 adapter 先檢查長度：台本至少 `sizeof(ink::internal::header)`（inkcpp 的 `shared/private/header.h`）；snapshot 至少 5 個 `size_t`（inkcpp 私有的 `snapshot_impl.h` 的 header 是兩個 `size_t`、一個 hash、一個 bool、一個 `size_t`，這是它的上限，只寫在註解）。比這短的直接回 `NotAStory`／`SnapshotUnreadable`。
- 存檔綁定平台的佈局（位元組順序、`size_t`）：開頭的 8 bytes 是明確的 little endian，後面的 snapshot 不是。
- 內容損壞但 header 正確的 snapshot，inkcpp 不完全檢查（例：`string_table` 讀到 0 才停）；那是未定義行為，不在契約內。封套沒有加 checksum：存檔放在哪、要不要驗證是作者的。

## 8. inkcpp d38608d 的缺陷（沒有打補丁）

四個缺陷都用素的 inkcpp 重現過，各有一個測試把現狀釘住（D1 用自己的台本 `once.ink`，主台本 `merchant.ink` 不碰它）。inkcpp 修好時，只有那個測試會失敗。

- **D1 跳過的選項的 tag 給了下一個選項**：一次性選項（`*`）已經選過、或條件為假時，`runner_impl.cpp:1554-1573` 在 `assign_tags` 之前就 `break`。已經算好的 tag 沒有被收走，於是跟到下一個選項上。line ID 的約定會因此拿錯。
- **D2 在選項處存的檔，台本裡有 sequence 就讀不回來**：等選擇時的 snapshot 不能遷移，讀它時 inkcpp 要求每個計數的 container 都有具名的 hash（`globals_impl.cpp:294-297`）。可是台本裡任何地方只要有 sequence（`{a|b}`、`{&a|b}`、`{!a|b}`、`{~a|b}`），就有沒有具名 hash 的 container，連讀回**同一個**台本都失敗。用小台本逐一試過：plain、`RANDOM`、一次性選項、tunnel、EXTERNAL 都沒事，四種 sequence 都會失敗，sequence 放在沒走到的 knot 也一樣。
- **D3 第一次 `Advance` 之前存的檔，讀回來已經結束**。
- **D4 tunnel 裡選完之後存的檔，inkcpp 說能遷移，搬到新版之後 tunnel 回到別的地方**（I4 設計審查發現）：`runner_impl::can_be_migrated`（`runner_impl.cpp:695-717`）不看堆疊上 tunnel 的返回點，`migrate_to`（`runner_impl.cpp:870-916`）也不清堆疊，`->->` 用舊版台本的位移返回。`snapshot.h` 自己寫的是 tunnel 用著時不能遷移，旗標和文件不一致。實例：商人台本 Haggle → Insist 之後存，讀進第二版，「No.」之後跑進「買劍」那條分支的尾巴，多出台詞「Here you go, friend.」和玩家沒選的指令 `play_sound(3)`。讀回**同一個**台本沒有這個問題。

**能存的時間點**（這一版 inkcpp）：選完之後、下一次 `Advance` 之前。讀回同一個台本會逐字接上，tunnel 裡也一樣；讀進新版台本能遷移，但 tunnel 裡存的會遇到 D4（測試「a save taken after a choice…」「…inside a tunnel…」「…next version…」、D4 的測試）。使用者的決定（宣告 11.1 U5）：存檔時機照 inkcpp 作者在 #146 的建議，選完之後、下一次 `Advance` 之前存，選項畫面不存。這是**遊戲的規則**，adapter 不擋：在選項處存仍然回位元組，`Save` 把 `restorable` 標成 false（第 9 節，宣告 11.3 U7 (a)），`Restore` 讀它時回錯誤。D2、D4 由另一張卡回報 upstream，不打補丁。

## 9. `Save` 說的事（U7 (a)）

`Saved` 有三樣：

- `bytes`：存檔的位元組。不管在哪裡存都給；不擋、不改。
- `restorable`：`Restore` 用**同一個**台本讀回，是不是同一段對話。
- `migratable`：`Restore` 用**另一版**台本讀回時，inkcpp 說它能不能搬過去。

怎麼判斷：

- `restorable`：`Save` 當場在同一個台本上照 `Restore` 的做法重建一次 globals 與 runner，再比「還能不能往下跑」。這是試讀一次的判斷，不是逐字比對；位置只比 `can_continue` 這一個值，為的是 D3。
  - 重建失敗（D2：inkcpp 丟例外）是 false。
  - 重建成功、但「還能不能往下跑」（`can_continue`）和原本的對話不同，也是 false（D3：讀回來已經結束）。
  - 理由：D2 取決於台本裡有沒有 sequence、D3 取決於有沒有 `Advance` 過，只看位置猜不準；讓 inkcpp 自己讀一次最準。代價是每次 `Save` 多建一份 globals 與 runner。
- `migratable`：`restorable`，而且 inkcpp 的 `snapshot::can_be_migrated()` 是 true。inkcpp 判 false 的條件（`runner_impl.cpp:695-717`）：在等選擇、剛進 knot（`_entered_knot`）、位置沒有名字、在 thread 裡，以及輸出、堆疊等元件在 lookahead 的保存狀態。
  - inkcpp 說能搬，搬的時候仍可能失敗：存檔所在的 knot 或選項在新版不見了（`SnapshotNotMigratable`，第 6 節）。
  - 也可能 `Restore` 成功、卻搬到別的地方：tunnel 裡存的（D4，第 8 節）。adapter 照 inkcpp 給的值交出，沒有自己再判：要知道是不是在 tunnel 裡，得讀 inkcpp 私有的 `runner_impl` 堆疊，會打破 adapter 只用 inkcpp 公開介面的界線。
  - `Save` 不知道下一版台本長什麼樣，所以不保證。

這一版 inkcpp 實際的結果（測試「Save says whether Restore on the same story gives the dialogue back…」逐點用 `Restore` 核對）：

| 存的時間點 | `restorable` | `migratable` |
|---|---|---|
| 選完之後、下一次 `Advance` 之前 | true | true |
| tunnel 裡選完之後 | true | true（D4：搬過去後 tunnel 回到別處） |
| 等選擇，台本有 sequence（D2；tunnel 裡也一樣） | false | false |
| 等選擇，台本沒有 sequence（`once.ink`） | true | false |
| 第一次 `Advance` 之前（D3） | false | false |

- 只比 `can_continue`，不比選項的個數：這一版 inkcpp 沒有「讀回成功、往不往下跑相同、選項個數不同」的情形，比了也測不到（突變存活），所以拿掉。
- 不支援的參數（U7 (d)）照程式核對過：list 參數丟例外，`Advance` 回 `ScriptFault`，不會被默默忽略（測試 `[ink-list-argument]`、突變 `ink-list-argument-accepted`）。無號數走同一條路，但 ink 的台本寫不出無號數，沒有測試。

## Design Invariants

- **INK-1 inkcpp 只在 adapter 目錄**：inkcpp 的標頭只被 `plugins/ink/src/adapters/inkcpp/` 與 `plugins/ink/tests/adapters/inkcpp/` 底下的檔案 include。`Dialogue.hpp` 沒有 inkcpp 的型別，`gyo_inkcpp` 是 PRIVATE 連結。守衛：審查（沒有原始碼掃描）。
- **INK-2 錯誤是 `Result`，只說發生了什麼**：每個進入點都接 inkcpp 的例外，翻成第 6 節的種類，process 不會因為台本而結束。守衛：測試（7.2 的各案）、突變 `ink-advance-uncaught`、`ink-stopped-not-kept`、`ink-choice-range-unchecked`、`ink-list-argument-accepted`、`ink-save-version-unchecked`、`ink-not-migratable-unchecked`、`ink-empty-runner-unchecked`、`ink-migration-failure-unreadable`。
- **INK-3 狀態由外面控制**：亂數的種子由 `DialogueOptions::seed` 給（預設是固定值，不是時鐘）；`Restore` 用存檔裡的亂數狀態；存讀是位元組，交給作者。守衛：`[ink-seed]`（釘住的文字，三平台一樣）、`[ink-restore]`；突變 `ink-seed-unset`、`ink-restore-reseeded`、`ink-migrated-inverted`、`ink-restore-commands-unbound`。
- **INK-4 thread 的契約**：同一個 `Story`、它的複本、從它開始或讀回的所有 `Dialogue` 是一家，同一時間只給一條 thread 用；交給別的 thread 時兩邊之間要有同步。各自讀進來的 `Story` 是不同的家。理由：inkcpp 的參照計數不是 atomic（`story_ptr.h:25`），沒有 `thread_local`，也沒有可寫的全域狀態。放進 World 時當一般 Resource（使用者 U4），不用 `threadBound`。守衛：`[ink-threads-apart]`、`[ink-threads-in-turn]`；同時碰同一家的情形是未定義行為，測不出可靠的失敗，由契約與審查負責。
- **INK-5 `"C"` locale**：process 的 `LC_NUMERIC` 必須是 `"C"`（inkcpp 用 `snprintf("%.7f")` 寫浮點）。adapter 不檢查。守衛：`[ink-locale]`。
- **INK-6 不假設單機**：adapter 裡沒有本地玩家、UI、輸入、視窗、牆鐘；`Beat`、`Choice` 是一般資料，可以複製給 Client；選項編號當不可信的輸入檢查（`ChoiceOutOfRange`）。守衛：審查。
- **INK-7 `Save` 說讀不讀得回來**：`Saved::restorable` 是 `Save` 當場用同一個台本試讀一次的結果：試讀失敗（D2）或讀回已結束（D3）是 false。`migratable` 只在 `restorable` 時才可能是 true，值是 inkcpp 給的（D4 時是錯的，第 8 節）。位元組照給，留不留由作者決定。守衛：測試 `[ink-save-restorable]`、`[ink-save-migratable]`、`[ink-save-bytes]`；突變 `ink-save-restorable-unchecked`、`ink-save-place-uncompared`、`ink-save-migratable-from-restorable`、`ink-save-migratable-from-inkcpp`。

## Replacement Criteria

- **RC-INK1**：第 8 節的 D2 讓需要的存檔點存不了（例：遊戲要在選項處存檔），或 D4 讓 tunnel 裡的存檔搬不了版本（例：遊戲在 tunnel 裡存檔、又要更新台本）→ 在「給 inkcpp 打補丁」「回報 upstream 等修」「換 runtime」之間選。
- **RC-INK2**：第二個遊戲要用台本、遊戲需要「放進 World 的對話單位」、或幾段對話要共用台本變數（例：多個 NPC 記得同一件事；現在每個 `Dialogue` 有自己的 globals、自己的存檔，inkcpp 的 Unreal 整合是一個 runtime 共用 globals）→ 重看第 1 節的「未解決」與宣告的 Q5（多後端）。
- **RC-INK3**：遊戲需要有回傳值的外部函式、或要讀寫 ink 變數 → 在「query 回呼」和「遊戲寫 ink 變數」之間選（宣告第 5.4 節）。這時也要重看 `Advance`「一次跑到選項」的粒度：同一次 `Advance` 裡，指令之後的 query 會讀到指令變成的 `Message` 還沒被處理時的狀態（Unreal 有 `Yield`／`Resume`）。
- **RC-INK5**：換 runtime → 資產的格式也跟著換（`LoadStory` 讀的是 inkcpp 的二進位格式，資料契約綁在它上面；ink.json 是 ink 官方的中間格式，不綁）。
- **RC-INK4**：第二種語言 → 字串表（研究 Q4，line ID 的約定已定）。

## 範囲の限界

- 本機只在 Intel Mac 跑過（一般建置，以及 ASan＋UBSan 的 Debug 建置）。Windows、Linux 的編譯與測試只靠 L1。
- ASan：27 案沒有記憶體錯誤（I4 後重跑）（macOS 上沒有開 leak 檢查）。UBSan 報了 6 種，全部在 inkcpp 裡（adapter 沒有）：讀位元組碼時位址沒有對齊（`runner_impl.cpp:179`、`story_impl.cpp:263`），以及在 null 指標上加位移（`array.h:277`、`runner_impl.h:397`、`snapshot_interface.h:33`、`list_table.cpp:956`）。在 x86-64 與 arm64 上照樣能跑；沒有打補丁。
- 存檔在三平台之間能不能互讀，沒有測試（第 7 節）。
- `ink_assert` 帶格式參數時 `malloc` 的訊息不會釋放（`shared/public/system.h:258-261`）：每次 `ScriptFault` 漏一小段。沒有打補丁。
- `.ink` 與 `.ink.json` 的一致性 CI 不檢查（第 4 節）。
- 沒有選項的無窮迴圈台本會讓 `Advance` 一直跑（inkcpp 沒有步數上限），在權威端就是卡住模擬執行緒。沒有加守衛：那是台本的錯，由寫作的工具或審查看。
- 長度檢查拿掉時（太短的位元組交給 inkcpp），結果是讀出界的未定義行為，一般建置的突變殺不穩，所以沒有放突變；`[ink-not-a-story]` 的 16 bytes 案與 `[ink-save-unreadable]` 的 8 bytes 案在有檢查時會過。
- SDL-2 那種「拿掉 Plugin 之後重建」的 CI 步驟，只對 `sdl` 這一家做，`ink` 沒有做。

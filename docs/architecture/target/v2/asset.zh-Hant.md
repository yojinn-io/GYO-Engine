# 資產（`engine/modules/asset`、`engine/modules/world_asset`）

更新：2026-11-22（新頁，`v8-arch-gyo2-asset` 第 6 步，宣告 R10）；2026-11-23（合併前設計審查的處理，宣告 30.3）。由來：[事前宣告](asset-declaration.zh-Hant.md)（`v8-arch-gyo2-asset`，使用者 2026-11-22 動工前核准，核准的版本 55a14825）第 5 節、第 7 節；實作時的調整在宣告第 30 節。**Design Invariants 的最終文字：使用者 2026-11-23 合併前核准。**

Owner：架構。取代舊頁 [target/assets.zh-Hant.md](../assets.zh-Hant.md)（11-11 的方向與 11-13 的 DI 1～3）。

程式：
- `engine/modules/asset/`（`GYO::Engine::Asset`，target `GYO::Asset`，library_only，不認得 ECS）。
- `engine/modules/world_asset/`（`GYO::Engine::WorldAsset`，target `GYO::WorldAsset`，登記單位 `WorldAssetModule`，`default=yes`）。

行為測試：`tests/common/asset/`（只連 `GYO::Asset`）、`tests/common/world_asset/`。突變清單：`tests/common/asset/mutations.json`、`tests/common/world_asset/mutations.json`。

**核心一句**：asset 是機制。作者只說「我要 id 為 X 的內容」；載入、就緒、生命週期由 Engine 提供。asset 告訴外面三件事——載到哪了、成了沒有、為什麼沒成——並讓 World 唯讀地看到載好的值。要不要等載完、失敗後怎麼辦、內容身分怎麼比、檔案是什麼格式，asset 都不決定。

## 一個例子

FPS 的 Client 進對局之前要載規則的 JSON。內容根目錄的 `content.json` 列出 catalog，catalog 寫「id `rules.movement` 是 `text`，在 `rules/movement_rules.json`」。作者的生成 System 拿 `Res<AssetRequests>`，呼叫 `Load<Text>(AssetId("rules.movement"))`，當場拿到 `Handle<Text>`。讀檔與解碼在 World 執行緒之外跑；下一個套用點（預設 `Main`/`First`）之後，`Res<AssetView>` 的 `State(handle)` 是 `Loaded`、`Get(handle)` 回 `const Text*`。作者用 `Ready({...})` 看一組是不是都好了，好了才不再 `Hold()` 生成。檔案不在時狀態是 `Failed`、`Failure(handle)` 的碼是 `NotFound`；asset 不記日誌、不重試，World 照跑。

## 1. 範圍與位置

```text
GYO::Base ◄── GYO::Asset ◄── GYO::WorldAsset ──► GYO::Kernel、GYO::Orchestration、GYO::Threads
                 │ private
                 ├── GYO::IO
                 └── GYO::Json
```

| 單位 | 內容 |
|---|---|
| asset | `AssetId`；catalog 的讀、寫、驗證（`Catalog.hpp`）；內容路徑的規則（`ContentPath.hpp`）；loader 的介面（`AssetLoader` concept、`LoadContext`、`LoaderFailure`）；整檔讀取 `ReadWholeFile`；文字 loader（`Text`、`TextLoader`）；`AssetStore`、`Handle<T>`、`AssetRef<T>`、`AssetLoaderRegistry`、`AssetStoreDiagnostics`、`AssetExecutor`、`AssetSnapshot`；`AssetId`／`Handle<T>`／`AssetRef<T>` 的雜湊函式 |
| world_asset | `WorldAssetModule`：在 `Configure` 建 Product 的 `AssetStore`（單位自己持有）；每個 World 的 `AssetRequests`、`AssetView`；套用 System `ApplyAssets` 與準備函式；設定 `AssetOptions`（Product 一份）、`WorldAssetOptions`（每個 World）；`WorkerPoolExecutor`（把 threads 的 `WorkerPool` 接在 `AssetExecutor` 後面，只轉接） |

- 登記檔：asset 是 `library_only=yes`、`default=no`、`private_depends=json`；world_asset 是 `library_only=no`、`default=yes`、`depends=asset`。base、io、kernel、orchestration 的 link 寫在各自的 `CMakeLists.txt`。
- 拆兩個的理由：工具（模型檢視器的 `info`、audio_editor 寫 catalog）與擁有型別的 Module 只要 loader、catalog 與 store，不該為此連上 kernel、orchestration。
- 擁有型別的 Module 也照這個拆法：loader 與型別特性的函式庫只依賴 asset；World 端的單位依賴 world_asset。
- 不在範圍：其他 loader（模型、圖片、字型、音效）、image Module 與 stb_image、GPU 資源、事先轉檔的工具、大地圖的串流、熱重載、子 asset（宣告 2.2）。

## 2. id

`AssetId` 是 catalog 裡的字串鍵，**它就是持久鍵**：作者在程式、內容檔、存檔、判定資料裡都只記它。

- 字元集與長度：ASCII `[a-z0-9._-]`，1～128 字元（`IsValidAssetId`、`kMaxAssetIdLength`）。`AssetId` 本身可以裝任何字串；不合規是讀 catalog 時的錯誤，不在 id 被查的地方判。
- 等值看字串，沒有「只看雜湊」的等值。
- 改名、搬移、事先轉檔：只改 catalog 項目的 `path` 或 `format`，id 不變。
- store 內部的索引依 catalog 的順序而異，不出現在任何對外的值裡（AS-6）。

## 3. catalog（asset 擁有的 Data Contract）

**格式**（JSON）。入口是內容根目錄的 `content.json`：

```json
{"version": 1, "catalogs": ["asset_catalog.json"], "shader_bundles": ...}
```

每份 catalog：

```json
{"version": 1, "assets": [{"id": "char.hero", "type": "text", "path": "characters/hero.json", "format": "json", "settings": {}}]}
```

- `id`、`type`、`path` 必填；`format`、`settings` 可選。`type` 是內容載成的 C++ 型別的型別名（`AssetTypeName<T>::value`）。
- `path` 是內容根目錄下的相對路徑，規則照 master 的 `IsRelativeContentPath`：相對、`/` 分隔、沒有空段或 `.`、`..`，沒有 `\`、`:`、控制字元，不能是 `content.json`。
- `format` 省略時取 `path` 的副檔名：最後一段裡最後一個點之後，轉小寫；目錄名裡的點不算（`EffectiveFormat`、`PathExtension`）。
- `shader_bundles` 屬 render：可省；有的話照收、不驗。
- 不認得的頂層鍵、項目裡多的欄位：照寫的樣子保留（`PreservedMember`），不解讀；寫出時原樣寫回。所以新加可選欄位不升版，舊讀者照樣收得下新內容。
- master 的 `assets/object_fps_pvp` 的 `content.json` 與 `asset_catalog.json`（origin/master 3b411e21）原樣通過。

**Version**：整數，`content.json`（`kContentManifestVersion`）與 catalog（`kCatalogVersion`）各有自己的，目前都是 1。新加可選欄位不升版；改了既有欄位的意思、或新增必填欄位才升版。讀到不認得的 version 是錯誤。

**Validation Rule (A)：資料契約的規則**（只看資料本身；只連 asset 的工具也查得了；`CatalogFailureCode`）：

| # | 規則 | 碼 |
|---|---|---|
| 1 | `content.json`：JSON 解析得了；`version` 是整數、本版認得；`catalogs` 是陣列，每筆是合內容路徑規則的相對路徑、彼此不重複、不是 `content.json`；列出的 catalog 檔讀得到 | `NotJson`、`BadShape`、`BadVersion`、`UnknownVersion`、`BadCatalogPath`、`DuplicateCatalog`、`Unreadable` |
| 2 | catalog：JSON 解析得了；`version` 是整數、本版認得；`assets` 是陣列，每筆是物件，`id`、`type`、`path` 是非空字串 | `NotJson`、`BadShape`、`BadVersion`、`UnknownVersion` |
| 3 | id 在全部 catalog 裡唯一 | `DuplicateId` |
| 4 | id 合 `[a-z0-9._-]`、1～128 字元 | `BadId` |
| 5 | `path` 合內容路徑規則、不是 `content.json` | `BadPath` |
| 6 | `path` 沒有副檔名時要寫 `format` | `NoFormat` |
| 7 | `format` 有寫時是 `[a-z0-9]+`；`settings` 有寫時是 JSON 物件 | `BadFormat`、`SettingsNotObject` |

- 讀到第一個違反就停，回那一條的碼。
- 寫出（`WriteContentManifest`、`WriteCatalog`）前檢查同一組規則，所以寫出的東西讀得回來、而且相等；另外檢查保留欄位不和已知鍵重複、本身是 JSON（`WriteFailed`）。
- **刻意不查的**：沒有 loader 的型別（合法；請求時才是 `NoLoader`）；檔案存不存在（載入時才是 `NotFound`）；兩個 id 指向同一個檔（合法；經路徑反查遇到它才是 `AmbiguousReference`）；由副檔名推出的 `format` 的字元集。
- **範例集**：`tests/common/asset/contract/`。`invalid/<規則>-<名字>` 各違反一條 (A) 規則（29 份）；`valid/` 是規則刻意收下的內容與 master 的原檔（5 份）。C++ 的驗證、日後的工具與組裝管線都對同一組。

**Validation Rule (B)：登記時的檢查**（看登記進來的 loader，不是資料本身的規則，不進範例集；`LoaderRegistrationFailureCode`）：

| # | 檢查 | 結果 |
|---|---|---|
| 1 | 同一（`type`、`format`）已有別的 loader | 登記回 `FormatTaken`，那個 loader 不登記 |
| 2 | loader 登記時，對 catalog 裡它的（`type`、`format`）的每個項目解讀 `settings` | 解讀不了的那幾個 id，請求時是 id 級 `Failed(BadSettings)`；loader 照樣登記 |
| 3 | （`type`、`format`）沒有 loader 也沒有預先登記 | 啟動時不報；請求時是 id 級 `Failed(NoLoader)` |
| 4 | 兩個 C++ 型別宣告同一個型別名 | 登記回 `TypeNameTaken` |
| 5 | 預先登記與登記的衝突（三個方向） | 預先登記期間別的 loader 型別來登記同一格 → `FormatPreregistered`；已登記時別的 loader 型別來預先登記 → `FormatTaken`；兩個 loader 型別先後預先登記同一格 → `FormatPreregistered`。有任何一格衝突就整筆不記；同一個 loader 型別重複預先登記是冪等的 |

**誰產生**：手寫、Script、工具都一樣有效；執行期讀，不需要 build 的產物。asset 提供讀、寫、驗證；工具的登記政策（例：「id 已屬別的型別就拒絕」）歸工具。

**`content.json` 的 owner**：asset 擁有它的格式與 Version；其中 `shader_bundles` 的內容歸 render。

## 4. store（照 Bevy `bevy_asset` 的演算法）

`AssetStore` 每個 Product 一份，在 World 外。演算法逐處照 Bevy，改裝的地方和理由在宣告 5.5 末的對照表。

**表與鎖**：建 store 時把相對的 `contentRoot` 對當下的工作目錄轉成絕對路徑（之後工作目錄變了，各執行緒上的工作仍讀同一批檔）；項目的檔是「內容根目錄＋相對路徑」（`JoinContentRoot`，catalog 的讀取也用它）。每個 catalog id 一筆 catalog 項目（建 store 時建，永久；記生命序號、跨生命遞增的嘗試序號、loader 登記時算出的 settings 解讀結果）。項目底下**活著時才有一筆 info**（狀態、值 `shared_ptr<const void>`、失敗、原始位元組雜湊、依賴邊、「待略過」計數、對強核心的弱參照）。整張表與 loader 表由**一把讀寫鎖**保護：拉視圖拿讀鎖；請求、工作完成、維護、登記拿寫鎖。鎖記著獨占持有者的執行緒，同一條執行緒重入是 `GYO_ASSERT`。

**Handle**：`Handle<T>` 共有一個強核心（`shared_ptr`）；複製是原子加一。最後一個放掉時把（id、生命序號）推進「丟了」佇列，不拿 store 的鎖。一段生命可以先後有多個核心（救回）。Handle 的相等看 id。Handle 比 store 活得久也安全（推送變成空操作）。作者只看得到 `Id()` 與 `RequestFailure()`；核心（`Body()`）與生命序號（`Life()`）是私有的，store、world_asset 與測試經 `Detail::HandleAccess` 拿（AS-11）。

**請求** `Request<T>(id)`：
1. 請求級的檢查（不拿鎖、不碰 info）：id 不在 catalog → `UnknownId`；`T` 的型別名不是項目的 `type` → `TypeMismatch`。回帶失敗、不帶核心的 Handle，放掉時不推「丟了」。
2. 拿寫鎖，三支窮舉：
   - 沒有 info：建 info（新生命），依序判：settings 壞 → `Failed(BadSettings)`；有 loader：沒有執行器 → `Failed(NoExecutor)`，有 → `Queued`、嘗試序號加一；沒有 loader：有預先登記 → `WaitingForLoader`，沒有 → `Failed(NoLoader)`。
   - 有 info、弱參照升級得了：共用同一個核心（命中）。
   - 有 info、升級不了：新核心、「待略過」加一，值留著（救回）。
   - 後兩支先判 `Failed`：`BadSettings` 留著；`NoLoader` 看現在有沒有 loader 或預先登記（有預先登記就轉 `WaitingForLoader`）；其餘改回 `Queued`、嘗試序號加一。
   - 依賴邊走的是同一個請求的三支（第 5 節）。所以一個已經 `Failed` 的依賴，會在之後完成的每個父項加邊時被重判、重讀（A、B 都引用 X，X 讀失敗之後 B 才完成，X 再讀一次）；父項在依賴還排隊時完成只是命中。這是核准的 5.5，和 Bevy 的 `load_dependency` 相同；件數照「引用」與「工作」各自的等式記（AS-8）。
3. 放鎖之後才把工作交給執行器。

**維護** `Pump()`：拿寫鎖，把「丟了」佇列處理到空。每筆：沒有 info 或生命序號不同 → 過期、略過；「待略過」> 0 → 減一；否則移除 info（項目的生命序號加一），值在放鎖後才解構。

**工作**（讀檔＋解碼，在執行器上）：開工前拿鎖比生命序號，對不上（還沒開工就被移除）就不讀檔、名額交給下一筆；開工後狀態 `Loading`，讀檔與解碼不持鎖（讀檔或算雜湊丟例外，例如大檔的 `std::bad_alloc`，是 id 級 `ReadFailed`，名額照還）；完成時拿寫鎖，生命序號相同才寫入值或失敗、加依賴邊並對依賴走一次請求的三支、重算受影響的強連通分量、把在飛名額交給下一筆 `Queued`；不同就算取消。

**在飛上限**：`maxInFlight`（0＝不設限，預設）。`Queued` 照請求先後排隊。被取消的工作做完才放名額。

**值**：不可變、共享唯讀（AS-10）。

**原始位元組雜湊**：工作讀檔時對讀到的位元組算 SHA-256（`engine/base` 的 `Sha256`），每個 id 一個，只算自己的檔。預設開，`sourceDigest = false` 時不算（讀出來是「沒算」）。限界：只代表檔的位元組；catalog 項目的 `format`、`settings` 不同時要另比。

**執行器** `AssetExecutor`（只有 `Submit`）：asset 用自己的話定，不依賴 threads。契約：工作跑一次（之後或當場、任何執行緒），停了之後 `Submit` 是安全的空操作；工作裡可以 `Submit`，當場跑完的執行器要排隊、不遞迴；store 呼叫 `Submit` 時不持自己的鎖。**沒有執行器時**，需要排工作的請求是 id 級 `Failed(NoExecutor)`，store 不在呼叫者的執行緒上代跑。今天 gyo2 上沒有正式的執行器（threads 的 `WorkerPool` 還沒合進來）：產品自己寫接 `AssetExecutor` 的 adapter；測試用控制執行器與測試自己的執行緒池（第 9 節）。

**關閉** `Shutdown(Drain | Discard)`：`Drain` 先等到沒有排隊與在飛的工作（含完成的工作排進來的依賴），執行器要還在跑；`Discard` 不等排隊的。之後不再開工或提交，排隊的清空，沒完成的計入丟棄，所有 info 移除，之後放掉的 Handle 不推任何東西。**兩種模式都等已開工、還沒完成的工作**（在讀檔或在 loader 裡的）跑完：它們發現 store 已關，什麼都不留，值在 `Shutdown` 返回前解構；所以 world_asset 的 `OnDestroy` 返回時 store 的工作已停（編排層 O-15）。交給執行器、還沒開工的不跑 loader。只有第一次呼叫有作用；不能從工作裡呼叫，也不能在持有 loader 會等的東西時呼叫。解構子是 `Shutdown(Discard)`。

**不建 World 的使用者**（工具）：`Request<T>`、`Pump()`、`Read<T>(handle)`（讀鎖下拿值的共享指標；別的 store 的 Handle 讀出空）。`Probe(id)`（測試與工具用的讀法，唯一會出現 `NotRequested` 的地方）、`Counts()`、`Version()`、`Snapshot()`、`PullIfChanged(version)`。`Diagnostics()` 給 `AssetStoreDiagnostics`：只有 `Counts()` 與 `Probe(id)` 兩個讀法，沒有 `Request`／`Read`／`Pump`／`Snapshot`／`Shutdown`。工具也可以完全不經 store，直接呼叫 loader。

## 5. loader 的介面與依賴

**介面**（`AssetLoader` concept）：

```cpp
struct TextureLoader {
    using Output = Texture;          // AssetTypeName<Texture>::value 是 "texture"
    using Settings = TextureSettings;
    using Code = TextureLoaderCode;  // 從 1 開始的 enum，有 ToString
    static constexpr std::string_view kTypeName = "texture";   // 必須等於 Output 的型別名（編譯期檢查）
    static constexpr std::array<std::string_view, 2> kFormats{"png", "jpg"};  // 至少一個
    static constexpr std::size_t kDefaultMaxBytes = 64u * 1024u * 1024u;
    Base::Result<Settings, LoaderFailure<Code>> ParseSettings(std::optional<std::string_view> json) const;
    Base::Result<Texture, LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                    const Settings& settings, LoadContext& context) const;
};
```

- 型別名綁在 C++ 型別上：擁有型別的 Module 特化 `AssetTypeName<T>`，和 loader 有沒有登記無關。所以請求級的 `TypeMismatch` 只用 catalog 與這個特性就能判。
- `ParseSettings` 收項目的 `settings`（JSON 物件的緊湊文字，沒寫時是空）；store 在 loader 登記時對每個項目呼叫一次，在鎖外。
- `Load` 沒有 World、沒有時鐘、沒有全域可變狀態：同樣的位元組與 settings 得到同樣的值與同樣的引用清單。`Output` 要能多執行緒同時讀、在任何執行緒解構（AS-10 (c)）。
- 選 loader 的鍵是（`type`、`format`）。整檔進記憶體，帶上限；產品可以用 `byteLimits` 依（`type`、`format`）覆寫（放寬或收緊），超過是 `TooLarge`。
- 登記經 `AssetLoaderRegistry`（只有 `Register<L>()`、`Preregister<L>(formats)`），任何時點都可以。同一個 loader 型別重複登記是冪等的。

**文字 loader**：型別名 `text`，認 `txt`、`json`，只驗 UTF-8（`NotUtf8`），位元組原樣交出，預設上限 16 MiB。world_asset 在 `Configure` 登記它。

**預先登記**：擁有型別的 Module 可以先宣告「（`type`、`format`）的 loader 會來」（照 Bevy 的 `preregister_asset_loader`；鍵是 loader 的 C++ 型別＋`format` 清單）。之後的請求是 `WaitingForLoader`、不排工作；loader 登記時在同一次寫鎖內把等它的轉成 `Queued`（壞 settings 的轉成 `Failed(BadSettings)`），清單裡它沒認的 `format` 的等待者變成 `NoLoader`。loader 永遠不來時，請求一直是 `WaitingForLoader`，就緒查詢另列「在等 loader 的 id」。已登記的 loader 型別再預先登記它沒認的 `format` 是 `FormatNotTaken`（什麼都不記；否則那個 `format` 的請求會等一個不會來的 loader），預先登記它認的 `format` 成功、不記（已經有 loader）；同一個 loader 型別再登記時也清掉它的預先登記。**會晚登記的 loader 必須預先登記**（否則依賴會被永久略過）。

**依賴（Engine 推出）**：loader 經 `LoadContext` 宣告引用，拿到**不持有的** `AssetRef<U>`：
- `Reference<U>(AssetId)`；
- `ReferencePath<U>(相對路徑)`：相對於父項的檔所在目錄解析；絕對路徑、反斜線、超出根目錄的 `..`、解不成內容路徑的是 `BadReference`。store 給 `LoadContext` 注入 catalog 的路徑索引（`ContentPathIndex`），路徑換成 id；沒有項目是這個路徑 → `UnlistedReference`，兩個以上 → `AmbiguousReference`。直接呼叫 loader（沒有索引）時只記路徑、id 空。

一條引用在 store 裡的判法（先後固定）：1. 經路徑的先解成 id；2. id 在 catalog（否則 `UnknownId`）；3. `U` 的型別名合項目的 `type`（否則 `TypeMismatch`）；4. 有那個（`type`、`format`）的 loader 或預先登記；兩者都沒有就**略過**（不載、不算失敗）。1～3 的失敗是邊級的依賴失敗。通過的邊立刻持有依賴、需要時排工作。

- **持有依賴的是 store 的依賴邊，不是值**；父項那段生命被移除時，它的邊一起放掉。
- **循環**：每次加邊就對受影響的邊重算強連通分量（已標成循環的邊也算進圖），同一個分量裡的邊（含自我引用）全部標 `CyclicReference`、都不持有。標記與略過都是單調的：直到父項那段生命被移除都不重新持有。
- **連同依賴的狀態**（`DependencyState`，每次由邊重新推導）：自己或任何遞迴依賴還沒結束 → `Incomplete`；全部結束、自己或某個遞迴依賴失敗 → `Failed`（帶 `DependencyFailure{parent, reference, writtenPath, code}`）；全部 `Loaded` 或略過 → `Complete`。父項自己的狀態不因依賴失敗而變；失敗的依賴之後重載成功，父項回到 `Complete`（和 Bevy 不同）。
- 依賴在父項解碼完、在工作完成裡才排（Bevy 是 loader 執行中就啟動）；父項解碼失敗時依賴不載。

## 6. World 端：`AssetRequests`、`AssetView`、套用點

**設定**：
- `AssetOptions`（Product 一份，經 `ProductDescription::options`；沒給用預設）：`contentRoot`（空＝catalog 空、每個請求都是 `UnknownId`，不是啟動錯誤）、`sourceDigest`（預設開）、`maxInFlight`（預設 0＝不設限）、`byteLimits`、`executor`（不持有，要活得比 Product 久；空＝沒有執行器）。world_asset 不建執行緒：產品建 threads 的 `WorkerPool`（用哪個池、開幾條執行緒由產品選），用 world_asset 的 `WorkerPoolExecutor(WorkerPool&)` 接在 `AssetExecutor` 後面；也可以給別的執行器。`WorkerPoolExecutor` 只轉接，不開執行緒、不持有池。world_asset 因此連 `GYO::Threads`（core，手寫在它的 CMake，不走 O-6）；asset 函式庫照舊不依賴 threads。
- `WorldAssetOptions`（每個 World，經 `WorldDescription::options`）：`applySchedule`、`applyStage`，預設 `Main`/`First`。預設骨架關掉的 World 自己指定；沒指定時是 Kernel 的 D-E1，啟動報告寫「System 'ApplyAssets' is mounted in schedule 'Main', which this Product does not have」，看得出是 world_asset 的 System（和 world_time 的先例相同）。

**生命週期**：
- `Configure`（Product 層一次）：讀 `AssetOptions`、讀 catalog；建 `AssetStore`，**由 `WorldAssetModule` 自己持有**（只有 world_asset 請求、讀、維護它；登記單位比共用處活得久，O-15）；在 Product 的共用處只 `Create` `AssetLoaderRegistry`（擁有型別的單位在自己的 `Configure` `Find` 它來登記 loader）與唯讀的 `AssetStoreDiagnostics`（件數，給測試與工具）；登記文字 loader；`OnDestroy` 時 `Shutdown(Discard)`（返回時沒有工作還在 loader 裡）。catalog 讀不了是 `ProductSetup::Report` 的 `WA-E1`（啟動錯誤，不建任何 World；store 照樣以空 catalog 建好，登記 loader 的單位找得到它）；文字 loader 登記失敗是 `WA-E2`。
- `Build`（每個 World）：登記 `AssetRequests`、`AssetView` 兩個 Resource，掛 `ApplyAssets`，給準備函式。
- 準備函式：先跑一次維護（`DropOldFirst` 時舊 World 的「丟了」在這裡處理掉），再放 `AssetRequests` 與拉一份 `AssetView`。之後的準備函式、`beforeSwap`、遊戲初始化裡命中已載好的值當場讀得到；新送出的請求下一個套用點才進視圖。
- `ApplyAssets`（每幀，在套用點；生成期間照跑）：維護，然後只在 store 的版本號變了時重拉視圖。`AssetView` 不在（作者拿掉或換掉）就重建；`AssetRequests` 不在就什麼都不做。

**`AssetRequests`**：請求入口，`Load<T>(AssetId)`、`Load<T>(AssetRef<T>)`，都是 `const`，System 以 `Res<AssetRequests>` 呼叫（store 有自己的鎖）。回傳只有 Handle，不帶「命中與否」；沒有讀 store 當下狀態的入口，也不給出 store。不寫進雜湊。

**`AssetView`**：store 裡**所有活著的項目**的狀態與值（照 Bevy 的 `Assets<A>` 裝全部），只在套用點與準備函式換，一幀之內的 System 都看到同一份。可移動、不可複製、建構子私有、沒有可變操作。讀法：
- `State(handle)`：請求級失敗 → `Failed`；Handle 有核心、而視圖裡沒有相同的（id、生命序號）→ `Queued`（本幀剛送出的、舊視圖配新生命、從別的 World 帶來或 Extract 來而建在本 World 上次拉取之後的）；Handle 不指向任何 id → `NotRequested`；其餘照視圖。
- `Failure(handle)`：`Failed` 背後的失敗（請求級或 id 級），否則空。
- `Get(handle)`、`Get(AssetRef<U>)`：`Loaded` 且 C++ 型別相合時回 `const T*`，否則空；指標只在這次 System 執行期間有效。父項的連同依賴是 `Complete` 時，非循環、非略過的依賴一定讀得到。
- `SourceDigest(handle)`：SHA-256 的文字；沒算或沒讀檔時空。
- `Ready(一組 UntypedHandle)` → `AssetReadiness`：`allFinished`（每一筆 `Loaded` 或失敗；`WaitingForLoader`、`Queued` 不算結束）、`complete`（每一筆 `Loaded` 且連同依賴 `Complete`）、`dependenciesFinished`、`loaded`／`failed` 筆數、`failures`（`AssetReadyFailure`：屬於這組的哪個 Handle、父項、id、碼）、`skipped`、`waitingForLoader`、`ids`（整串）。`UntypedHandle` 從任何 `Handle<T>` 隱式轉來。
- `Unfinished(一組 UntypedHandle)`：還沒結束的筆數。
- `Failures()`：所有活著、狀態 `Failed` 的項目（id、嘗試序號、碼）。日誌記下看過的，比一比就知道哪些是新的失敗。

讀取畫面、淡出、等哪些才開始 tick 是作者的事；單筆到了就看得到，「先看到地面才看到樹」要作者用就緒查詢擋住。

**換 World、多 World**：store 在 World 外，Handle 的引用計數跨 World 有效。兩個 World 請求同一個 id 只載一次、只存一份。`BuildNewFirst` 的新 World 請求同樣的 id 一定命中；`DropOldFirst` 時舊 World 先銷毀、準備函式先維護，沒有別的 World 持有的就重讀——作者要不重讀，就把 Handle 放進自己標 `CarryOver` 的 Resource。回主選單另建 Product 時重新載入。

## 7. 狀態、錯誤與件數

**狀態**（`AssetState`）：`NotRequested`、`WaitingForLoader`、`Queued`、`Loading`、`Loaded`、`Failed`。

**錯誤**（`AssetLoadFailureCode`，都是 E2：回報，store 不記日誌、不重試、不讓 World 中斷）：

| 級 | 碼 | 怎麼出現 |
|---|---|---|
| 請求級 | `UnknownId`、`TypeMismatch` | Handle 帶失敗、不帶核心，不碰項目 |
| id 級 | `NoLoader`、`BadSettings`、`NoExecutor`、`NotFound`、`ReadFailed`（含讀檔或算雜湊時丟的例外）、`TooLarge`、`LoaderFailed`（detail 帶 loader 的碼）、`LoaderThrew`（`catch (...)`，detail 帶 `what()`） | 項目 `Failed`；再請求照第 4 節重判 |
| 邊級 | `BadReference`、`UnlistedReference`、`AmbiguousReference`、`UnknownId`、`TypeMismatch`、`CyclicReference`；依賴本身的失敗帶它自己的碼 | 父項自己的狀態不變；連同依賴是 `Failed`，帶 `DependencyFailure` |
| 啟動錯誤 | catalog 的 (A) 規則（`WA-E1`）、文字 loader 登記失敗（`WA-E2`） | `ProductSetup::Report`，收進 `ProductStartupReport` |
| 登記 | `FormatTaken`、`FormatPreregistered`、`TypeNameTaken`；預先登記另有 `FormatNotTaken` | 登記的回傳值；那個 loader 不登記（預先登記什麼都不記）。回傳值只是設定錯誤的回報，System 不拿它分支判定 |
| E0 | 同一條執行緒重入 store 的鎖；建 store 時 catalog 項目有重複 id | `GYO_ASSERT` |

要不要因為載入失敗結束遊戲，由作者的 System 決定（回 `Err` 就是作者把 E2 升成 E1）。

**件數**（`AssetStoreCounts`，AS-8）：
- 請求：`requests == hits + revives + newLives + requestFailures`；`newLives == newLivesQueued + newLivesWaiting + newLivesNoLoader + newLivesBadSettings + newLivesNoExecutor`。
- 引用：`references == referenceHits + referenceRevives + referenceNewLives + referenceSkipped + referenceFailures + referenceCyclic`；後來被標成循環的邊從原本那一類移到 `referenceCyclic`。
- 工作：`jobsQueued == jobsLoaded + jobsFailed + jobsCancelled + jobsDiscarded + jobsWaiting + jobsInFlight`；沒有排隊與在飛時就是宣告 5.8 的等式。
- 維護：`dropsTaken == dropsStale + dropsSkipped + removals`。
- 丟棄件數只留在 store 的計數裡：Product 結束時 World 已經沒了，E3 的記錄沒有可寫的地方。

## 8. 決定性與雜湊函式

- id 是 catalog 的字串，和請求順序、完成時序、World 的歷史都無關。全部結束之後，store 的內容和完成順序無關；哪一筆在哪一幀讓 World 看到依完成時序。
- asset 的視圖不是判定資料，回滾 Module 不存它、不倒回它。到達時刻要影響判定，走 `rollback.Input<T>`；否則判定用的內容要在進判定之前載完，而且 Handle 要一直持有（AS-7）。
- 雜湊函式（給 `World::Summary`）：

| 型別 | 寫什麼 | 在哪 |
|---|---|---|
| `AssetId` | id 的字串 | asset，`HashValue(Sink&, const AssetId&)` |
| `Handle<T>` | id 的字串（不含型別、不含 store 的索引） | asset，經 `AssetId` 的那一個 |
| `AssetRef<T>` | id 的字串 | asset，經 `AssetId` 的那一個 |
| `AssetRequests` | 什麼都不寫 | world_asset |
| `AssetView` | 什麼都不寫（依完成時序變；告知用輪詢，不用 Kernel 的 `Message`） | world_asset |

asset 的三個是受約束的泛型：`Sink` 要能用自己的雜湊函式寫字串（`Detail::HashesText<Sink>`；Kernel 的 `Hasher` 符合），所以 asset 不依賴 Kernel，經 ADL 接上 Kernel 的客製點。

## Design Invariants

守衛欄的寫法：行為測試寫 doctest 的案例名（ctest 的名字是冒號前的群組：`asset.<群組>`、`world_asset.<群組>`）；負向編譯樣本寫標記（`tests/common/world_asset/compile_fail/AssetContractSample.cpp`，判法是「靜態斷言失敗＋標記在同一行」，configure 時跑）；突變寫 id（清單見頁首）。「延後」照 `v8-arch-gyo2-guards-after-mechanism-stable`：綁實作方式的掃描守衛，等 scenario test 通過才立。

- **AS-1 資產管理在 World 外；System 不持指標。**
  - 每個 Product 一個 `AssetStore`，由 world_asset 在 `Configure` 建、由 `WorldAssetModule` 持有；Product 的共用處只放它的 `AssetLoaderRegistry` 與唯讀的 `AssetStoreDiagnostics`。值只在 store 裡。
  - World 端只有 `AssetRequests` 與 `AssetView` 兩種 Resource；元件裡只有 `Handle<T>`、`AssetRef<T>` 或 `AssetId`。
  - GPU 資源是 render 的 Resource；渲染 World 讀別的 World 的資料只經 Extract。store 不是哪個 World 的資料，渲染 World 也經自己的 `AssetView` 讀它，不算跨 World 讀。
  - 守衛：行為測試 `scenario: generate, share, load a save (BuildNewFirst, DropOldFirst), a missing id, the counts`（兩個 World 請求同一個 id 讀檔計數不增加、讀到相等的值；`BuildNewFirst`、用 `CarryOver` 持有的 `DropOldFirst` 換 World 後不重讀；不持有的 `DropOldFirst` 重讀一次）；`scenario: two Worlds of a Product share one store: one read, one value (AS-1)`。專屬突變 `aw-one-store-per-world`（每個 World 各建一個 store：兩個 World 讀到不同的物件）。掃描延後。
- **AS-2 核心不認得格式。** asset 與 world_asset 不解析任何內容格式（catalog 的 JSON 是 asset 自己的契約；文字 loader 只驗 UTF-8）。格式由擁有型別的 Module 登記 loader；第三方只在該 Module 的 `src/adapters/<組件>/`，私有 link。
  - 守衛：`catalog: what the rules deliberately accept reads`（範例 `valid/type-without-loader`）；`store: a loader can come after a request failed with NoLoader`；`store: registration refuses a taken pair and a taken type name; the same loader again is idempotent`；`loader: the text loader checks UTF-8 and hands the bytes over unchanged`。突變 `as-store-format-taken`、`al-utf8-overlong`、`al-utf8-surrogate`、`al-utf8-continuation`。
- **AS-3 loader 是不依賴 ECS、可直接呼叫的純函式。** 輸入位元組、settings、`LoadContext`（id、路徑、上限）；輸出值或失敗；不碰 World、不讀時鐘、不持全域可變狀態；只連 `GYO::Asset` 就能呼叫。
  - 守衛：測試執行檔 `gyo_asset_tests` 只連 `GYO::Asset`；`loader: the same input twice gives the same result`；`loader: two threads of 1000 calls each give what one thread gives`；`loader: a loader that declares references gives the same list twice`。突變 `ar-ref-id-recorded`。CMake 掃描延後。
- **AS-4 IO 不碰 World；值只在 store 的鎖下、由完成的工作寫進表；World 只在套用點拉視圖。** 讀檔與解碼在 World 執行緒之外；`AssetView` 只在 `ApplyAssets` 與 world_asset 的準備函式換，一幀之內看到同一份。
  - 守衛：`view: a job that finishes in the middle of a frame is seen at the next apply point, not in that frame`；`view: an older view against a newer life is Queued, with no value (5.7)`；`view: an AssetView an author removed is built again by the apply System`；scenario test 的 `beforeSwap`（準備函式拉的視圖）；樣本 `GYO_ASSET_SAMPLE_VIEW_HAS_NO_MUTATION`。突變 `aw-apply-mid-frame`、`aw-view-life`、`aw-sample-view-mutation`、`as-store-in-flight-unlimited`、`aw-prepare-no-view`、`aw-view-rebuilt-empty`、`aw-unseen-dependencies-finished`。
- **AS-5 身分在請求時給，而且就是 id。** Handle 在請求時同步給出、指向 id；全部結束之後，任何完成順序、任何發佈切分都得到相同的 store 內容（不比生命序號與嘗試序號）。
  - 守衛：`equiv: every completion order ends in the same store contents`；`equiv: dropping and requesting while jobs run ends as when every job runs at once`；`equiv: a cycle and a self reference end the same in both completion orders`；`threads: threads requesting, dropping and taking maintenance at once end as the controlled run`；`depend: a cycle closed through an edge already marked is marked too`。突變 `as-store-cycle-skips-marked-edges`。
- **AS-6 持久鍵是 id 字串；store 內部索引只在行程內。** id 可進內容、存檔、權威資料；`Handle<T>`、`AssetRef<T>`、`AssetId` 的雜湊只用 id 字串。
  - 守衛：`summary: two catalogs listing the entries in different orders give the same Summary for the same Handles`；`summary: a new game and a save loaded into the same level give the same Summary`；`store: a Handle of another store reads as null`（Handle 裡的索引只對它的 store 有意義）。突變 `aw-hash-index`、`as-store-read-other-store`。
- **AS-7 「何時載完」與內容身分不是 asset 的規則；asset 提供能力。** asset 給狀態與就緒查詢（連同依賴，輪詢），保證同位元組同值（AS-3），並給每個 id 的原始位元組雜湊（預設開、可關）。要不要等、轉場、比不比雜湊與不同時怎麼辦，屬作者或網路 Module；到達時刻要影響判定，走 `rollback.Input<T>`。**判定用內容的 Handle 要一直持有**，放在沒有用 `rollback.` 登記的 Resource 裡；否則回滾倒回後再 `Load`，別的 World 的維護可能已經移除它，得到 `Queued`。
  - 守衛：`store: the source digest is the SHA-256 of the bytes read, or not computed`（開：同位元組同雜湊、改一個位元組不同、等於 `Sha256` 直接算的值；關：讀出空）。突變 `as-store-digest-always`。「asset 不拿雜湊做判斷」靠本頁與審查。
- **AS-8 錯誤只回報。** 請求級與 id 級的失敗是 E2，在狀態與就緒查詢裡；依賴的失敗帶 `DependencyFailure`；catalog 的資料規則是啟動錯誤，loader 登記的檢查回給登記的一方；不寫日誌、不自己重試、不讓 World 中斷；工作、請求、引用、維護的件數等式成立（第 7 節）。
  - 守衛：`store: a request is refused at the request level without the lock and without an info`；`store: id-level failures of a job carry their codes`；`store: without an executor a request that needs a job is NoExecutor`；`store: Shutdown discards or drains`；`store: Shutdown waits for a job that is already in its loader, in both modes`；`view: request-level failures are the Handle's, and touch no item`（請求級失敗的一組不是 `complete`）；`view: content that breaks the catalog's rules is a startup error reported from Configure`；scenario test 的件數突き合わせ（第 8 段）與 `OnDestroy` 的丟棄件數。突變 `as-store-request-type-unchecked`、`as-store-discard-forgets-in-flight`、`as-store-cancel-uncounted`、`as-store-cyclic-count-not-moved`、`as-store-shutdown-waits-running`、`aw-no-on-destroy-shutdown`、`aw-request-failure-complete`。
- **AS-9 介面與格式、位置分開。** 作者只用 id；catalog 把 id 對到（`type`、`path`、`format`、`settings`）；選 loader 用（`type`、`format`）；加一種格式（含引擎格式）＝登記一個 loader＋改 catalog 項目，不改請求的 API 與核心。
  - 守衛：`store: the same id moved to an engine format gives the same value`（同一 id 從 `.txt` 改指到 `gyotxt`）；`store: registration refuses a taken pair and a taken type name; the same loader again is idempotent`。突變 `al-type-name`、`as-store-same-loader-refused`。
- **AS-10 載好的值不可變。**
  - **(a) 物件不改**：發佈之後不再改變；World 端只拿得到 `const` 存取。守衛：樣本 `GYO_ASSET_SAMPLE_VIEW_GET_IS_READ_ONLY`、`GYO_ASSET_SAMPLE_VIEW_HAS_NO_MUTATION`、`GYO_ASSET_SAMPLE_VIEW_IS_NOT_COPY_ASSIGNABLE`、`GYO_ASSET_SAMPLE_VIEW_IS_NOT_CONSTRUCTIBLE`（各有對照組）。突變 `aw-sample-view-get-writable`、`aw-sample-view-mutation`、`aw-sample-view-copy-assign`、`aw-sample-view-construct`。
  - **(b) 綁定只有 store 換，本批不換**：一段生命只發佈一次 `Loaded` 的值；`Failed` 之後重載、成功時才發佈，不算第二次；熱重載回來時是「換一個值」，由 store 做（RC-AS4）。守衛：`store: the result of an old life arriving late is cancelled`。突變 `as-store-complete-ignores-life`。
  - **(c) loader 的 `T` 要 const 並行安全、解構不綁執行緒**：沒有 `mutable` 成員、沒有惰性快取、沒有非原子的內部計數；解構可以在任何執行緒發生。作者要改就複製一份；字型的 glyph 快取這類可變狀態放在擁有型別的 Module 自己的 Resource。守衛：`store: the last holder of a value on another thread destroys it, once`；其餘只靠審查（C++ 擋不住 `mutable`）。
- **AS-11 World 的資料不上鎖；store 有自己的一把鎖。** World 的資料只經套用點與交換點批次進入，World 內不需要鎖。System 呼叫 store（`Load`、套用點、登記）時拿 store 自己的鎖；持鎖只做表的操作（查、改欄位、加依賴邊、重算受影響的強連通分量、建視圖內容），不讀檔、不解碼、不呼叫 loader 作者的函式、不交工作給執行器、不解構值。成立的條件：
  - Handle 與 `AssetRequests` 不提供讀 store 當下狀態的入口，`AssetRequests` 不給出 store；Handle 的核心與生命序號是私有的（`Detail::HandleAccess`）；store 不在 Product 的共用處（只有 world_asset 持有），共用處的 `AssetStoreDiagnostics` 只有件數與 `Probe`；
  - `Load` 的回傳除了 Handle 帶的生命序號（只用來判「視圖比 Handle 舊 → `Queued`」）之外不帶 store 的當下狀態；
  - 登記與預先登記的回傳值只是設定錯誤的回報，System 不拿它分支判定；
  - 讀狀態一律經 `AssetView`；System 不呼叫 store 的讀取與維護入口（`Request`／`Read`／`Pump`／`Probe`，給不建 World 的使用者；靠審查），登記經 `AssetLoaderRegistry` 可以。
  - 守衛：樣本 `GYO_ASSET_SAMPLE_HANDLE_HAS_NO_STATE`、`GYO_ASSET_SAMPLE_HANDLE_HAS_NO_GET`、`GYO_ASSET_SAMPLE_HANDLE_BODY_IS_PRIVATE`、`GYO_ASSET_SAMPLE_HANDLE_LIFE_IS_PRIVATE`、`GYO_ASSET_SAMPLE_REQUESTS_HAVE_NO_STATE`、`GYO_ASSET_SAMPLE_REQUESTS_GIVE_NO_STORE`；`store: a job decoding does not block a request`；`store: the store holds no lock while it submits, destroys a value or reads settings`（測試執行器的 `Submit`、測試型別的解構子、測試 loader 的 settings 解讀各回呼 store，持鎖時就是重入斷言）。突變 `aw-sample-handle-state`、`aw-sample-handle-get`、`aw-sample-handle-body`、`aw-sample-handle-life`、`aw-sample-requests-state`、`aw-sample-requests-store`、`as-store-request-submits-locked`、`as-store-decodes-locked`、`as-store-completion-submits-locked`、`as-store-destroys-locked`、`as-store-settings-locked`。「持鎖讀檔」與樣本以外的「沒有這種成員」靠審查。跨組件的通則登記在 `docs/cross-component-rules.md` 的 CCR-6。
- **AS-12 catalog 是 asset 擁有的 Data Contract。** 格式、Version、Validation Rule 在本頁第 3 節；工具、Script、手寫產生的 catalog 一樣有效；執行期讀，不需要 build 的產物；asset 函式庫提供讀、寫、驗證，工具照它寫。
  - 守衛：`catalog: every invalid example is refused with the code of its rule`（`contract/invalid/` 29 份）；`catalog: what the rules deliberately accept reads`；`catalog: master's content passes as it was`；`catalog: what is written reads back equal`；`catalog: writing checks the same rules as reading`；`catalog: one catalog read alone is checked for repeated ids`；(B) 組：`store: registration refuses a taken pair and a taken type name; the same loader again is idempotent`、`store: preregistration conflicts in three directions`、`store: preregistering a format its registered loader does not take is refused`、`store: BadSettings both ways`。突變 `as-id-charset`、`as-id-length`、`as-id-length-edge`、`as-path-dotdot`、`as-path-backslash`、`as-entry-content-json`、`as-catalog-content-json`、`as-catalog-twice`、`as-version-unknown`、`as-unreadable-code`、`as-id-twice-within`、`as-id-twice-across`、`as-no-format`、`as-extension-last-segment`、`as-format-charset`、`as-settings-object`、`as-shader-bundles-required`、`as-unknown-dropped`、`as-unknown-refused`、`as-write-format`、`as-write-settings`、`as-write-preserved`、`as-write-validates`、`as-store-preregister-not-taken`。
- **AS-13 依賴由 Engine 推出。** loader 經 `LoadContext` 宣告引用（id 或 catalog 裡的路徑），拿到不持有的 `AssetRef`；持有依賴的是 store 的依賴邊；就緒查詢的「好了」包含遞迴的全部依賴；父項自己的狀態不因依賴失敗而變；依賴的（`type`、`format`）在這個 Product 沒有 loader 也沒有預先登記就略過，略過是單調的，所以會晚登記的 loader 必須預先登記；同一個強連通分量裡的邊全部是 E2 `CyclicReference`、都不持有，結果和完成順序無關。
  - 守衛：`depend:` 群組 14 案（鏈、鏈頭放掉、菱形、循環兩種順序、自我引用、經已標循環的邊閉合、依賴失敗與父項、經路徑引用、邊級 `UnknownId` 與 `TypeMismatch`、略過與單調、祖先回復、邊級失敗不回復、父項解碼失敗、`AssetRef` 不持有）；`loader: ReferencePath refuses what cannot name a content file`；`loader: with a path index, ReferencePath maps a path to an id or names why not`；`view: readiness with dependencies, a loader still to come, a skipped and a failed reference`；`depend: a failed dependency is read again by each parent that finishes after it (5.5, as Bevy)`；`store: a Failed(NoLoader) asset waits once its loader is preregistered and it is requested again`。突變 `ar-ref-bad-recorded`、`as-store-cycle-not-marked`、`as-store-cycle-skips-marked-edges`、`as-store-skip-ignores-preregistration`、`as-store-edge-type-unchecked`、`as-snapshot-finished-early`、`as-store-edge-reloads-failed`、`as-store-noloader-then-waits`、`as-snapshot-root-failed`、`as-store-pump-one-round`、`as-store-edge-holds-nothing`。

突變的 `guards` 欄沒有寫 DI、只守宣告第 5 節規則的 32 個（守的條文寫在各自的 `guards` 欄）：catalog 與 loader `as-extension-lowercase`、`al-formats`、`al-text-unchanged`；整檔讀取 `rw-too-large-code`、`rw-limit-edge`、`rw-not-found`；經路徑的引用 `ar-ref-above-root`、`ar-ref-absolute`、`ar-ref-parent-directory`、`ar-ref-dotdot`、`ar-path-unlisted`、`ar-path-ambiguous`；store 的演算法 `as-store-skip-ignored`、`as-store-skip-not-used-up`、`as-store-request-level-core`、`as-store-start-ignores-life`、`as-store-no-executor-code`、`as-store-orphan-waits`、`as-store-drain-does-not-wait`、`as-store-pull-always`、`as-store-removal-keeps-version`、`as-store-read-ignores-type`、`as-snapshot-failure-parent`；(B) 組 `as-store-bad-settings-not-first`、`as-store-register-ignores-preregistration`、`as-store-preregister-ignores-loader`、`as-store-preregister-ignores-preregistration`、`as-store-type-name-taken`；World 端（合併前設計審查加的）`aw-prepare-no-maintenance`、`aw-apply-no-maintenance`、`aw-waiting-finished`、`aw-world-options-ignored`。

### Replacement Criteria

符合任一條時，停下來交使用者決定「修還是重建」：

- **RC-AS1**：日後的地圖 Module 用 asset 的 id 與上限表達不了它的串流（例：region 要在對局中進碰撞，而 `rollback.Input<T>` 表達不了），而必須在 asset 核心加特例。
- **RC-AS2**：每加一種格式都要改 asset 核心，或某個 loader 必須碰 World 才寫得出來。
- **RC-AS3**：Product 一份的代價超出預算：回主選單另建 Product 的重載時間，或多個 Product 要共用同一份內容（今天沒有這種 Feature）。這時才看行程級。
- **RC-AS4**：熱重載或子 asset 成為點名的 Feature（這時才做 `Modified`、`#label`）；或某個 loader 的依賴必須在解碼中途拿到依賴的值才解得下去（今天的引用只宣告、不等值）。
- **RC-AS5**：World 看到結果時，交換點或編排層需要知道 asset 的型別或語意（和 RC-O8 同類）。
- **RC-AS6**：事先轉檔的路回來時，請求的 API、Handle 或 id 必須改（違反 AS-9）；或經路徑的依賴引用因為被引用的檔轉檔而解不到，工具又無法連父項一起轉。
- **RC-AS7**：某個 Feature 需要在最後一個 Handle 消失後保留值（寬限），或渲染那批要在上傳 GPU 之後丟掉 CPU 的副本（第二段釋放）。
- **RC-AS8**：執行期讀 catalog 的成本（啟動時間、記憶體）超出預算，或 catalog 必須由 build 產生才能保證一致（像 Unity）。
- **RC-AS9**：World 的資料需要鎖才保得住；或 store 的鎖必須含讀檔或解碼；或 `Load` 等鎖、套用點持鎖的時間超出預算；或某個點名的型別做不到 AS-10 (c)。日後 schedule 改成 Parallel 時，多個 System 同時 `Load` 的 `AssetRequests` 要重看。
- **RC-IM1**（日後 image Module 的 DI 節）：不可信來源的圖成為 Feature，或 stb_image 的缺陷影響我們的內容，就換 adapter。

RC-AS10（凍結後只准相同）隨凍結機制撤回（宣告第 22 節，使用者決定 13）。

## 9. 範圍的限界

- 只有文字 loader；依賴追蹤只用測試用的 loader 驗。第一個真正的使用者（模型引用貼圖）在產品搬回或 SDL 那批。
- 執行器 adapter：world_asset 的 `WorkerPoolExecutor`（使用者 2026-11-23「adapter 照建議放 world_asset」，核准後加入）。用真 pool 的案例是 `scenario: threads' WorkerPool behind an AssetExecutor loads what two Worlds request`（兩個 World、兩條執行緒）；`Shutdown` 的兩種模式由 store 層的案例擔保，沒有用真 pool 的案例。
- 預算延後：沒有載入時間、記憶體、catalog 讀取時間、雜湊成本、等鎖時間的判定。
- World 層的真執行緒：經 Product 入口今天不能兩條執行緒各跑一個 World，由 store 層的真執行緒案例擔保。
- 回滾重跑步裡的 `Load` 與「同一幀的重跑步與新步看到同一份」沒有專門的案例。
- 別的 World 的維護可能在本 World 的幀中途移除歸零的項目（自訂 runner 交錯 schedule、或日後 World 並行時）：同一幀放掉再 `Load` 不保證救回。
- 不套用的 World 留住舊值：它的 `AssetView` 讓已移除的值不釋放，直到下一次套用。
- 同型別名、只有一個有 loader：登記時查不到，`Load<T2>` 通過、`Get<T2>` 永遠回空。
- 載入中的 World 的 `Summary`：視圖不寫進摘要，比摘要要在「全部完成」之後比，且兩次之間 catalog 沒換。
- 資料競爭：CI 沒有 TSan；判定靠控制模式的案例。
- 負向編譯樣本的判法：L1 三平台的 configure 都照判法通過（windows-x64 的 MSVC 也是）；linux-x64、macos-arm64 的 L1 全部通過；windows-x64 第一次跑時建置在 `LoaderTests.cpp` 少 `<ostream>` 失敗（合併前設計審查 B1，已修），修正後看 L1。

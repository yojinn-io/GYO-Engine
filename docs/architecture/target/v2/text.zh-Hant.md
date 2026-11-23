# text Module（`engine/modules/text`）

Owner：架構。來源：事前宣告 [text-declaration.zh-Hant.md](text-declaration.zh-Hant.md)（修訂 1，2026-11-23 動工前核准；第 7 節）。最終文字在 `v8-arch-gyo2-sdl-t` 的合併前核准定稿。

Engine 的字型與文字：載入字型、shaping、換行、排版、點陣化進圖集。GYO 自己的介面（`FontData`、`FontLoader`、`TextContext`、`ShapedText`、`TextLayout`、`Caret`、`GlyphAtlas`）後面，是三個外部組件：FreeType（點陣化）、HarfBuzz（shaping、字型的度量）、libunibreak（UAX #14 換行、UAX #29 字素；使用者 2026-11-23 第 1 題選 (丁)）。只依賴 Base 與 Asset（字型的 loader），不進 World（Module 登記檔 `library_only=yes`）。狀態（字型、字形快取、圖集）由呼叫者持有：照 Bevy（`FontCx`、`FontAtlasSet` 是 main world 的 Resource）由 U、R2b 放進 World（跨部件規則 CCR-9：字形快取在擁有它的 Module 自己的 Resource）。

**同詞異義**（N1）：asset 已有型別 `"text"`（`Asset::Text`，整份文字檔）。本頁單寫「`text`」指這個 Module；asset 的型別寫「asset 型別 `"text"`」。

目錄：`include/engine/text/` 是介面；`src/` 是不碰外部組件的部分（UTF-8 解碼、分段與 fallback、換行的寬度演算法、對齊、游標查詢、圖集）；`src/adapters/<組件>/` 是直接碰外部組件的程式，一個組件一個子目錄（`freetype/`、`harfbuzz/`、`libunibreak/`）；`src/unicode/` 是由 Unicode 資料生成的表（`Default_Ignorable_Code_Point`，生成腳本 `scripts/generate_default_ignorable.py`）。外部組件的宣告在 `third_party/freetype/`、`third_party/harfbuzz/`、`third_party/libunibreak/`（FetchContent 抓官方 release tarball、釘 SHA-256，GYO 自建 target），原始碼在建置目錄的 `_deps/`，不進 repo。

**外部組件的數法**（AGENTS §6）：FreeType 一個、HarfBuzz 一個、libunibreak 一個，各一個 adapter。Unicode 的資料表是生成的我們的程式，不是外部組件。HarfBuzz 的 hb-ft（接 FreeType 的膠水）不用：兩個 adapter 各自從 `FontData` 的位元組建自己的物件。

## 資料契約 `"font"`（版本 1）

1. 檔是 sfnt（TrueType 輪廓 `0x00010000`／`true`，或 CFF 輪廓 `OTTO`）或 sfnt 的集合檔（`ttcf`）。WOFF、WOFF2、Type 1、點陣字型不收。（`NotSfnt`，TX-E1）
2. 集合檔至少一個 face（`EmptyCollection`，TX-E2）；每個 face 都能被 HarfBuzz 與 FreeType 開啟（`FaceUnreadable`，TX-E3）、字形數大於 0（`NoGlyphs`，TX-E4）、有 Unicode 的字元對應表（`NoUnicodeCmap`，TX-E5）。
3. 沒有 settings（face 在 `TextContext::AddFont` 時選）；有 `settings` 也收下、忽略。
4. 違反回 `TextFailureCode`（`Base::Error`，E2）。另外兩個值不屬資料契約：`FaceIndexOutOfRange`（TX-E6，`AddFont` 的 face 序號超出）、`OutOfMemory`（TX-E7）。

asset 型別 `"font"`、格式 `ttf`、`otf`、`ttc`、`otc`；loader `FontLoader`（`kDefaultMaxBytes` 64 MiB）。`FontData::FromBytes` 檢查整份契約，所以 loader 收了的字型，`AddFont` 只會因記憶體不足失敗。

## Design Invariants

守衛欄：行為的測試照常是守衛；綁實作方式的含入掃描在情境測試通過之後才立（AGENTS §3「ガードを立てる時期」）。

<a id="tx-1"></a>
**TX-1 唯一的 adapter。** FreeType 的標頭、型別、呼叫只在 `engine/modules/text/src/adapters/freetype/`，HarfBuzz 的只在 `src/adapters/harfbuzz/`，libunibreak 的只在 `src/adapters/libunibreak/`；`include/` 不含、不露出它們的型別；hb-ft 哪裡都不用；只有 `gyo_text` PRIVATE 連 `gyo_freetype`、`gyo_harfbuzz`、`gyo_libunibreak`。測試也只用 adapter 自己的介面（`AdapterTests.cpp` 含入 `src/` 的 adapter 標頭，不含入外部組件的標頭）。
守衛：`gyo_expect_links(gyo_text GYO::Base GYO::Asset gyo_freetype gyo_harfbuzz gyo_libunibreak)` 與「不露出給使用者」（`tests/common/text/CMakeLists.txt`）；ctest `text.adapter_guard`（`tests/common/text/text_guard.py`：四條含入規則〔含 hb-ft 哪裡都不准〕、三條名字規則、連結規則；根是 `engine/`、`tests/`〔必須存在〕、`build/cmake/` 與根目錄的 `CMakeLists.txt`；每條規則的正例、根不存在時失敗）與 `text.adapter_guard_selftest`。守衛只守 Engine 這一側（使用者的決定，`docs/architecture/plans/kernel/adapter-plugins-plan.zh-Hant.md` 第 15 行；本 Module 2026-11-23 第 B 題再確認）：`plugins/`、產品不掃，Plugin 要不要直接用 FreeType（例：imgui_freetype）是那個 Plugin 的批交使用者的事（AGENTS §6 的例外）。

<a id="tx-2"></a>
**TX-2 度量一個來源。** 字形的前進量、偏移、行的上下高度只來自 HarfBuzz adapter（內建的 OpenType 字型函式，讀 `hmtx`、`hhea`／`OS/2`，未 hinting）；點陣圖與 bearing 只來自 FreeType adapter；不用 FreeType 回報的前進量（light hinting 會把它捨成整數像素）。所以游標的位置和畫出來的位置一致。
守衛：`gyo_text_adapter_tests`（「HarfBuzz and FreeType read the same advances in font units」：三個測試字型的每個字形，`FT_LOAD_NO_SCALE` 的前進量等於 HarfBuzz 以 upem 為 scale 的前進量）；精確值的排版測試。

<a id="tx-3"></a>
**TX-3 原文範圍。** `TextLayout` 的每個字形帶 `[byteBegin, byteEnd)`；cluster 依原文順序單調、不重疊、蓋滿整段（每個位元組恰好屬於一個 cluster）；連字＝一個字形多個字，拆分與結合字元＝多個字形同一範圍（HarfBuzz 的 `MONOTONE_GRAPHEMES`）。游標只停在字素邊界（`graphemeBoundaries`）、帶 affinity（軟換行處分上一行的行尾與下一行的行首）；連字內的游標照字素數平分前進量。`CaretAt`、`HitTest`、`RangeRects` 只從排版結果算，不再 shaping、不碰字型。
守衛：`gyo_text_tests`（`ShapeTests.cpp` 的精確範圍、隨機字串的蓋滿性質；`CaretTests.cpp`；`ScenarioTests.cpp` 的文字框）。

<a id="tx-4"></a>
**TX-4 換行與字素。** 換行機會照 UAX #14、字素照 UAX #29（Unicode 17.0，libunibreak 8.0.1）。換行的語言與禁則強度是呼叫者的選擇（`ShapeOptions`，整段一個；使用者 2026-11-23 第 A 題）：`language`（BCP 47）決定模糊類 `AI`（中日韓當 `ID`、其他當 `AL`），並交給 HarfBuzz；`strictness` 的 `Strict` 讓 `CJ` 當 `NS`（小假名、長音符不放行首），`Normal` 當 `ID`。預設是不帶語言＋`Strict`，即 UAX #14 的預設解析。硬換行（`BK`、`CR`、`LF`、`NL`）結束一行，文末的硬換行之後留一個空行。依寬度斷行時退回最後一個換行機會、沒有就溢出（不硬切）；行尾的空白（U+0020、U+3000、tab、換行字元）不算寬度、也不觸發斷行——這個集合與「硬換行字元」（`BK`、`CR`、`LF`、`NL` 的字元，shaping 時換成空白、不畫）是 `text` 的固定規則（`src/TextContext.cpp`），換行機會本身只來自 libunibreak。斷點落在 HarfBuzz 標「不安全斷開」的地方時，兩側重新 shaping。對齊（左、中、右）以 `maxWidth`、沒有時以最寬的一行為基準。行高 `RelativeToFont(k)`（字級的 k 倍）或 `Px`，上下各分一半的 leading（half-leading，26.6 定點，向下取整到上方）。
守衛：`gyo_text_adapter_tests`（Unicode 官方 `LineBreakTest.txt` 19,338 件、`GraphemeBreakTest.txt` 766 件全部，件數斷言；檔案的 SHA-256 等於 unicode.org 的）；`LayoutTests.cpp`（日文禁則、語言與強度、混排、行尾空白、溢出、不安全斷開、對齊、行高）。

<a id="tx-5"></a>
**TX-5 Fallback 與缺字。** 每個字素選字型清單裡第一個有全部字形的字型（default-ignorable 的字元不算）；都沒有就用主字型（清單的第一個），缺字畫它的 `.notdef`。不合法的 UTF-8，每個最大的不合法子序列換成一個 U+FFFD 去 shaping，範圍保留原本的位元組。不查系統字型。
守衛：`ShapeTests.cpp`（fallback、`.notdef`、default-ignorable、三種不合法 UTF-8）；`AdapterTests.cpp`（Unicode 第 3 章的例子）。

<a id="tx-6"></a>
**TX-6 圖集。** 同一個快取鍵（字型、字形、字級〔26.6〕、點陣化方式）只畫一次、只進圖集一次；一個圖集只服務一個 `TextContext`（第一次 `Rasterize` 綁定，以 context 的身分物件比，不比位址）。頁是 Alpha8（coverage，顏色由畫的人乘）、row 0 在上、row pitch＝寬；每個字形周圍留 padding；放好的字形不移動、不回收；字型內嵌的點陣不用（`FT_LOAD_NO_BITMAP`）。`Revision()` 只在圖集改變時加一；`ChangesSince(r)` 給 r 之後所有變過的矩形（新頁是整頁），紀錄超過上限時較舊的讀者拿整頁；讀的一方不清除任何東西（DI-KS 13）。比一頁大的字形回 `TooLarge`、不畫，不是錯誤；不畫的字形（換行、tab、default-ignorable）回 `Empty`。
守衛：`AtlasTests.cpp`（只畫一次、版號、佈局與 padding、跟著變更紀錄的讀者和圖集逐位元組相同〔含紀錄合併之後〕、`TooLarge`、不畫的字形、綁定）；`ScenarioTests.cpp` 的 HUD（1,000 幀×30 段）。

<a id="tx-7"></a>
**TX-7 執行緒與狀態。** 沒有全域可變狀態：每個 `TextContext`、每次 `FontData::FromBytes`（loader）自己建 FreeType 與 HarfBuzz 的物件；環境變數不改行為（HarfBuzz 以 `HB_NO_GETENV` 建、FreeType 關掉 `FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES` 並逐 library 明定 hinting 的屬性）。`TextContext` 同一時間只給一個使用者（可以換執行緒）；`FontData`、`ShapedText`、`TextLayout`、`GlyphAtlas` 的 const 操作可以多執行緒同時做。
守衛：`ScenarioTests.cpp`（8 條執行緒各自一個 context 共用 `FontData`，結果與單執行緒相同；8 條執行緒同時載入字型）；`gyo_text_environment`（自己設 `HB_SHAPER_LIST`、`HB_OPTIONS`、`FREETYPE_PROPERTIES` 再排版與點陣化，kerning、連字、點陣圖不變）。ThreadSanitizer 不在 L1。

<a id="tx-8"></a>
**TX-8 跨平台相同。** 同樣的字型、文字、選項，排版結果（每個字形的 id、位置、範圍、行、字素邊界）與圖集的位元組在 L1 三平台相同。排版內部用 26.6 定點（HarfBuzz 的單位），最後才轉 `float`（精確）；HarfBuzz 的語言明定（`ShapeOptions::language`，沒有時 `und`；不讀 process 的 locale）；不為某個平台開分支。
守衛：`GoldenTests.cpp`（BIZ UDPGothic 子集 12、17、24、45 px，合成的 TrueType 與 CFF 字型；排版與圖集的 SHA-256）；`gyo_text_environment`。

<a id="tx-9"></a>
**TX-9 不知道 ECS 與產品。** 只依賴 Base 與 Asset；不依賴 Kernel、編排層、時間；不進 World（`library_only=yes`）；名字與註解不含遊戲或產品的概念（N5、N6）。
守衛：`gyo_expect_links`（同 TX-1）、`cmake.UnitRegistry`；審查。

<a id="tx-10"></a>
**TX-10 資料契約 `"font"` 版本 1。** 上面「資料契約」一節的四條。
守衛：`FontTests.cpp`（每條規則一個拒絕的案例，位元組在測試裡改；loader 的 failure 帶 catalog 項目與路徑；集合檔的每個 face）。

### Replacement Criteria

- **RC-TX1 方向與 script**：點名雙向文字或直排的 Feature 出現，而 TX-3 的「依原文順序單調」表達不了視覺順序與邏輯順序的分離 → 停下，交使用者重看排版結果的形狀。
- **RC-TX2 圖集與快取的記憶體**：有點名的 Feature 量到圖集超過它宣告的記憶體預算，或需要移除字型，或字級連續變化（動畫）讓 HarfBuzz 的已設字級字型快取（以 face 與字級為鍵、沒有上限）一直長 → 加回收與字型的移除；TX-6 的「不移動、不回收」要改，交使用者。
- **RC-TX7 tab stop**：有 Feature 點名 tab 的對齊 → 在 `LayoutOptions` 加 tab stop 的欄位（現在 tab 當一個空白）。
- **RC-TX3 兩個 adapter 的度量**：TX-2 守不住（某種字型 FreeType 與 HarfBuzz 的度量不同，必須逐字形修正），或需要 hb-ft → 停下，交使用者（hb-ft 是 AGENTS §6 的例外）。
- **RC-TX4 換行的規則**：要的換行超出 UAX #14 的預設解析（日文的寬鬆禁則切換、泰文的字典切詞），且要在 `text` 裡長出第二套規則 → 評估換成更完整的實作（ICU 一類），交使用者。
- **RC-TX5 每幀的成本**：宣告判定 7 的停止條件觸發，或 U、R2b 量到排版超過它們的幀時間預算 → 停下重看（排版結果的快取、shaping 的快取），不在呼叫端各自繞。
- **RC-TX6 彩色字形**：點名的 Feature 要 emoji → 加 RGBA 頁（`AtlasFormat::Rgba8`）與 FreeType 的彩色字形；TX-6 的格式條文要改。

## 使用 `text` 的一方要照做的（登記在 `docs/cross-component-rules.md`）

1. 畫字形時左上角＝`round(glyph.x) + placement.left`、`round(glyph.y) - placement.top`（`round`＝`floor(x + 0.5)`）。排版與點陣化用**乘過顯示縮放的字級**，排版座標就是最終目標的像素；在邏輯像素排好再放大，四捨五入會落在錯的座標空間、點陣也會糊（Bevy 在 pipeline 帶 scale factor：字級乘上去、位置再除回來）。R2b 的固定解析度縮放與相機縮放照這條接。
2. 圖集頁是 coverage：顏色由畫的人乘（shader 是「coverage × 顏色」）。上下方向在 UV 轉換的單一邊界處理（master 坑 17）。
3. 渲染側自己記上傳到的版號、用 `ChangesSince` 拿變更，不清除圖集的紀錄。
4. 一個 `GlyphAtlas` 只配一個 `TextContext`；`ShapedText` 只交給建立它的 context `Layout`；`TextLayout` 只交給排它的 context `Rasterize`。

## 範囲の限界

- 雙向文字、直排、彩色字形、可變字型的軸、系統字型、兩端對齊、字距、描邊都沒有做（RTL 的文字照 LTR 排，是錯的）。
- 只有 light hinting＋灰階反鋸齒一種點陣化方式；不做 subpixel 的位置（照 Bevy）。
- tab 當一個空白（沒有 tab stop）。
- 字型被當成作者的內容、不是任意的外部輸入；惡意字型的防護靠 FreeType、HarfBuzz 自己。
- 「換函式庫」要到第二個實作出現才算驗證過。

## 授權告示的義務

連到 `text` 的產品附：FreeType（FTL：「Portions of this software are copyright © ⟨year⟩ The FreeType Project (https://freetype.org). All rights reserved.」，`docs/FTL.TXT`）、HarfBuzz（"Old MIT"，`COPYING`）、libunibreak（zlib，`LICENCE`）、Unicode 資料（Unicode License v3，`src/unicode/DefaultIgnorable.cpp` 的來源）。產品自己帶的字型檔附它們自己的授權（例：BIZ UDPGothic 的 OFL 1.1）。打包時收集在 K（tools 線打包）。

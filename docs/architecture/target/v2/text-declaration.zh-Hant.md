# 事前宣告：`text` Module（SDL 線子批 T，`v8-arch-gyo2-sdl-t`）

更新：2026-11-23（修訂 1）。Owner：架構（GYO2 主會話開的任務卡，SDL 線子批 T）。**狀態：待動工前核准；核准前不動程式。**

經過：修訂 0（8bc5c17d）送 Opus／high 敵對式檢查（general-purpose、只讀；**作者與檢查者同為 Opus**：Fable 用量不足，照主會話的指示）：blocker 1、should 13、nit 9（`declaration-r0-adversarial-check.md`），全部採用，處理對照在第 17 節。修訂 1 沒有再送檢查。

性質：實作批的事前宣告（AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta、新 Component 的 Design Invariants 與 Replacement Criteria、§6 的外部組件 adapter）。SDL 總宣告（`sdl-declaration.zh-Hant.md` 修訂 21，分支 `claude/gyo2-sdl-declaration-7e7a1f`，動工前核准 `v8-arch-gyo2-sdl-declaration-approved`）第 10 節的子批 T。本頁只寫 T 的差分；整條線的七項在總宣告第 11 節。

根據：
- 使用者的決定（HANDOFF）：`v8-arch-gyo2-sdl-q6-freetype-harfbuzz-module`（「OK，那用FreeType + HarfBuzz 放Module」；`text` 的介面＝字型、量字、換行、排版結果、點陣圖；換行、fallback 字體、字形快取在 `text` 的 `src/`；FreeType、HarfBuzz 各一個 `src/adapters/`；授權告示 FreeType〔FTL 要標 credit〕與 HarfBuzz）、`v8-arch-gyo2-sdl-text-no-thin-start`（「照建議就行。」：FreeType＋HarfBuzz 一起實作，「字形對回原文的範圍」由 HarfBuzz 驗過）、`v8-arch-gyo2-sdl-declaration-approved`（總宣告整份核准，T 列的範圍與驗收骨幹）、`-hierarchy-bevy-and-primary-reference`（Bevy 為主參考，偏離寫衝突點與膠水量）。
- 總宣告：第 3.2 節 `GYO::Text` 列、第 3.3 節 `text` 列（純函式庫、狀態由呼叫者持有）、第 9.1 節末（FreeType、HarfBuzz 在 `third_party/`、每個組態都建、T 量建置時間、選用依賴要關）、第 10.1～10.3 節（T 不等別批；和 I 同改 `config/modules.csv`、`third_party/CMakeLists.txt`）、第 10.2 節 T 列、第 14 節 Q6（卡片補的：預設不用 hb-ft、FreeType 物件不能兩條執行緒同時用、ImGui 不走 imgui_freetype）、第 16 節（T 寫釘哪一版、從哪裡抓）。
- AGENTS §2、§3、§4、§6、§14；`docs/architecture/error-handling.md`（N25：`…Failure`＝E2）；命名規範 `rebuild-naming.zh-Hant.md` N11～N17、N25；批次表〈每批的設計審查〉。
- 跨部件規則（AGENTS §1）：`docs/cross-component-rules.md` CCR-9（loader 產出的值多執行緒唯讀、沒有 `mutable`；「字型的字形快取放在擁有它的 Module 自己的 Resource」）——本頁的 `FontData` 照它（不可變），字形快取與圖集在 `TextContext`／`GlyphAtlas`，由呼叫者當 Resource 持有。本頁新產生的使用側規則列在第 15 節，合併時登記。
- 先例：JSON Module 宣告（`json-module-declaration.zh-Hant.md`：外部組件以 FetchContent 釘 commit、GYO 自建 target、`src/adapters/<組件>/`、J-1 的守衛形式）；audio 的 `WavLoader`（Module 自帶自己型別的 loader、依賴 `asset`、輸出是共享的不可變資料）。
- 程式：`origin/gyo2` 545b5a67。

**標記**：寫「L17」的，是卡片照命名規範、別家原始碼、已定的原則自己判的，不是使用者的話，可以推翻。交使用者的只有第 13 節。

證據（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-sdl-text/`，git 不追蹤）：`bevy-text-survey.md`（Bevy 6a911653f 的 `bevy_text`，偵察者 Opus／high）、`master-text-object-connect-survey.md`（master 的 `GYO::Text`、`gyo.ui`、Object_Connect ca4482c，偵察者 Opus／high）、`upstream-metadata.md`（FreeType、HarfBuzz 的版本、commit、授權；`gh api` 只讀，未下載）。

---

## 0. 時間估計

| 段 | 估計 | 內容 |
|---|---|---|
| 設計段 | 5～6 h | 讀總宣告與現況、兩份偵察、本頁、敵對式檢查與修訂 |
| 實作段 | 36～54 h（推定；第 13 節第 1 題選 (丁) 約 33～45 h） | 第 10 節的 8 步：下載與查證 3～4、骨架與兩個 adapter 的建置 3～4、字型與 loader 3～4、shaping 與原文範圍 6～8、換行與字素 6～13（依第 1 題）、點陣化與圖集 5～7、情境測試與守衛與量測 3～5、突變與文件與收尾 7～9 |

超過 1.5 倍（設計段 9 h、實作段照選定的選項的上限乘 1.5）或單一問題累計超過 2 小時，停下回報（AGENTS §14 冒頭）。

## 1. 這一批做什麼（先用遊戲講）

- **Object_Connect 的 HUD**（總宣告 Q5；偵察第 5 節）：每幀把「残り 3 / 12」重組一次，用 BIZ UDPGothic 的 24 px 置中畫在 1280×720 的畫面上；關卡編號先量寬度，太寬就縮小字級。`text` 給：載入字型、把一段 UTF-8 排成「字形＋位置」、量出寬、高、第一條 baseline、行高；第一次出現的字形經 FreeType 畫進圖集，之後同一個字不再畫；圖集只有新畫的那一塊要上傳。
- **`gyo.ui` 的文字框**（子批 U；之後的 KeepTool 以外的工具與 PvP 大廳）：使用者用日文輸入法打「がっこう」，組字中的字要畫底線、游標停在「っ」後面、滑鼠點在「こ」上要把游標放到對的位置。`text` 的排版結果裡，每個字形都帶它對回原文的位元組範圍（一個字形可以對多個字、多個字形可以對同一個字），游標、選取、組字底線、點擊都只從這份資料算（master 是「把游標前的文字點陣化量寬度」，偵察第 2 節）。
- **一段說明文字自動換行**：框寬 300 px，日文與英文混排，「。」「」」不能出現在行首、「「」不能出現在行尾、英文單字不從中間切。`text` 照 Unicode 的換行規則（UAX #14）找可以斷的位置（第 13 節第 1 題）。
- **字型裡沒有的字**：英文字型裡沒有「漢」，就從呼叫者給的 fallback 字型清單裡找第一個有的；都沒有就畫主字型的缺字符號（`.notdef`）。

## 2. 範圍

### 2.1 這一批做的（T1～T9）

| 代號 | 內容 |
|---|---|
| T1 | `third_party` 加 FreeType 與 HarfBuzz（第 12 節，使用者同意後才下載）；各一個 GYO 的 target（`gyo_freetype`、`gyo_harfbuzz`），只給 `gyo_text` PRIVATE 連；FreeType 只建要的模組、關掉選用依賴（zlib、bzip2、PNG、Brotli、HarfBuzz）；HarfBuzz 用它自己的單檔彙整（`src/harfbuzz.cc`）、不定義 `HAVE_FREETYPE`（不建 hb-ft） |
| T2 | 新 Module `engine/modules/text`：Module 登記檔一列（`library_only`）、target `gyo_text`／`GYO::Text`、namespace `GYO::Engine::Text`；`src/adapters/freetype/`、`src/adapters/harfbuzz/` |
| T3 | 字型：`FontData`（共享的不可變位元組）、字型的 loader（asset 型別 `"font"`，第 5.2 節）、`TextContext::AddFont` |
| T4 | Shaping：分段（依 span、fallback 字型、script）、HarfBuzz shaping、kerning 與連字、每個字形的原文範圍（第 5.4 節） |
| T5 | 換行與字素：UAX #14 的換行機會、UAX #29 的字素邊界（做法照第 13 節第 1 題）、硬換行、依寬度斷行、對齊、行高（第 5.5 節） |
| T6 | 排版結果的查詢：游標位置、點擊對回原文、範圍的矩形（選取與組字底線用）（第 5.6 節） |
| T7 | 點陣化與圖集：FreeType 點陣化、字形快取、Alpha8 的多頁圖集、變更紀錄（第 5.7 節） |
| T8 | 測試、測試用字型、Unicode 的換行一致性測試、情境測試、含入守衛、建置時間與每幀成本的量測、突變清單（第 8 節） |
| T9 | 文件：新頁 `target/v2/text.zh-Hant.md`（Design Invariants TX-1～TX-10、Replacement Criteria、資料契約 `"font"`、外部組件的數法、授權告示的義務）、v2 README 索引、`docs/cross-component-rules.md` 的新登記（第 15 節）、dev_log、合併前核准頁 |

### 2.2 不在這一批

| 項目 | 留到 | 為什麼 |
|---|---|---|
| 把 `TextContext`、圖集放進哪個 World、用哪個 System 排版與點陣化 | U、R2b | `text` 是純函式庫（總宣告第 3.3 節）；傾向照 Bevy（來源 World 的 Resource，渲染側經 Extract 唯讀圖集；第 4 節） |
| 圖集上傳到 GPU、UV 的上下方向的轉換 | R2a（CPU 像素貼圖的建立與局部更新）、R2b（master 坑 17：在 UV 轉換的單一邊界修） | T 只定圖集的像素佈局（第 5.7 節） |
| 雙向文字（阿拉伯文、希伯來文）、直排 | 有點名的 Feature 時 | 沒有點名的 Feature；介面不堵死（run 帶方向欄位、原文範圍是位元組區間），RC-TX1 |
| 彩色字形（emoji 的 COLR、CBDT、sbix） | 有點名的 Feature 時 | Object_Connect、`gyo.ui` 都不用；圖集頁帶格式欄位，RC-TX6 |
| 可變字型的軸（variable font） | 有點名的 Feature 時 | 同上；快取鍵留欄位 |
| 系統字型的探索（CoreText、DirectWrite、fontconfig） | 有點名的 Feature 時 | 遊戲自帶字型、經 asset 載入；Bevy 也是選用的 feature（`system_font_discovery`） |
| 圖集的回收（eviction） | RC-TX2 | Bevy 也不回收（偵察第 5 點）；有長時間的動態文字量到記憶體超過預算時再做 |
| 兩端對齊（justify）、字距（letter spacing）、描邊 | 有點名的 Feature 時 | Object_Connect 的陰影是呼叫者畫兩次（偵察第 5 節），不需要描邊 |
| 編輯操作（插入、刪除、依字詞移動、復原） | U | Bevy 放在 parley 的 `PlainEditor`；GYO 的文字框在 `gyo.ui`，T 只給查詢（T6） |
| ImGui 的字型 | K | 總宣告 Q6：ImGui 預設用自帶的點陣化，imgui_freetype 是 §6 的例外，K 的宣告再交 |
| 授權告示收進產品的打包 | K（tools 線打包） | 總宣告第 9.1 節末；T 在 `text.zh-Hant.md` 列義務 |

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

`text` 的介面上會積：`gyo.ui` 的 layout（同步量字）與文字框（游標、選取、組字）、R2b 的疊層文字、Object_Connect 的 HUD、之後 PvP Client 與工具的 UI。排版結果的形狀（尤其「原文範圍」）、換行的規則、圖集的佈局，一旦被這些用上，改它每個使用者都要改；換行規則一變，所有已排好的畫面的斷行位置跟著變。所以照基盤處理（AGENTS §2「層の区別」）。

### 3.2 予見的根據 → 基盤的要求（Feature → Contract → Architecture）

根據的種類：1＝名指的 Feature、2＝成熟的參考實作、3＝規模與預算。

| # | Feature | 根據 | Contract | Architecture | 本批 |
|---|---|---|---|---|---|
| F1 | Object_Connect 的 HUD、選單、標題（日文、17～45 px、置中與靠左、每幀重組、量寬縮字級） | 1：總宣告 Q5、P24；偵察第 5 節（27 處 `QueueText`）；3：1280×720、每幀約 30 段字串 | 量字（寬、高、第一條 baseline、行高、行數）；kerning；水平三種對齊、垂直對齊要的度量；缺字畫 `.notdef`、不合法 UTF-8 換成 U+FFFD；硬換行；任意整數字級；用到才點陣化、同一字形只進圖集一次、局部上傳；輸出 coverage、顏色由畫的人乘 | T3～T5、T7 | 實作 |
| F2 | `gyo.ui` 的 layout 同步量字、文字框（子批 U） | 1：總宣告第 3.3 節 `ui` 列（「layout 同步量字」）、P17；master 的痛點（偵察第 2 節：拿點陣化代替量字、點擊不對應字）；2：Bevy 的 taffy measure（shaping 一次、斷行多次，偵察第 8 點） | Shape 與斷行分開：同一份 shaping 結果可以用不同寬度反覆斷行；每個字形帶原文的位元組範圍；游標、點擊、範圍矩形從排版結果算 | T4～T6 | 實作 |
| F3 | 多行的說明文字、日文換行（總宣告第 10.2 節 T 列的驗收「日文換行」） | 1：總宣告已核准的驗收骨幹；2：Bevy／parley 用 ICU4X 的 UAX #14（偵察第 3 點） | 換行機會照 UAX #14；硬換行；依寬度斷行 | T5 | 實作（範圍見第 13 節第 1 題） |
| F4 | fallback（日文與英文字型混用；總宣告第 10.2 節驗收「fallback 換字型」） | 1：總宣告；2：Bevy `FontSource::List`（偵察第 4 點） | 呼叫者給字型清單；每個字素依序找第一個有字的字型 | T4 | 實作 |
| F5 | R2b 的疊層文字、R2a 的 CPU 貼圖局部更新 | 1：總宣告第 10.2 節 R2a、R2b 列 | 圖集是純資料（像素＋頁＋變更紀錄），渲染側唯讀就能上傳（DI-KS 13：渲染 World 不寫回） | T7 | 實作 |
| F6 | 字型經 asset 載入 | 1：總宣告第 3.3 節 `text` 列（「字型檔經 asset 的 loader 來的話依賴 `asset`，T 定」）；2：Bevy `FontLoader`；先例 audio `WavLoader` | 資料契約 `"font"`（第 5.2 節） | T3 | 實作 |
| F7 | 文字框的組字、雙向文字、彩色 emoji、直排 | 1：組字＝KeepTool／U；其餘沒有點名 | 組字：範圍矩形（T6）；其餘：介面不堵死（2.2 節） | — | 組字＝實作；其餘只定形 |
| — | 跨平台 | 3：L1 三平台 | 同樣的輸入在三平台得到相同的排版與點陣圖（TX-8） | — | 實作 |

導不出的要求：沒有。每幀成本的預算見第 9 節判定 7（停止條件）。

### 3.3 Feature 與 Product

- 用到本批的 Product：Game（Object_Connect 的移植、之後的 PvP Client）、Tool（之後的工具的 `gyo.ui`）。本批沒有產品程式；F1～F5 由情境測試代表（第 8.5 節），真正的使用者是 U 與 R2b。
- 支撐的下一批 Feature：U（`gyo.ui`）、R2b（疊層文字、Object_Connect）。

## 4. 別家怎麼做（Bevy 為主參考，P26 的判法）

Bevy 本機 6a911653f（`bevy_text` 用 parley 0.11＋swash 0.2.6）；行號在 `bevy-text-survey.md`。parley 的細節查 docs.rs，標〔知〕的沒有查證。

| 技術 | Bevy（`bevy_text`） | master `GYO::Text` | Object_Connect | GYO 本批 | 處置 |
|---|---|---|---|---|---|
| 狀態放哪 | 全在 main world 的 Resource（`FontCx`、`LayoutCx`、`ScaleCx`、`FontAtlasSet`），函式收 `&mut` context（`pipeline.rs:71-85`） | 每次呼叫重開字型 | `FontSystem` 物件 | `TextContext`（字型、shaping、點陣化）＋`GlyphAtlas`（純資料）由呼叫者持有；放 World 由 U、R2b 定 | 照 Bevy（膠水：少） |
| 字型資料 | `Font` asset＝`Blob<u8>`（Arc 共享），loader 只收 ttf／otf | `FontAsset` 存位元組 | 從路徑載入 | `FontData`＝共享的不可變位元組；loader 收 ttf、otf、ttc、otc（第 5.2 節） | 照 Bevy；多收集合檔（Bevy 的 loader 不收 `.ttc`、但 collection 會註冊所有 face——GYO 讓 loader 也收，日文字型常是集合檔） |
| Shaping | parley（內部是 harfrust＝HarfBuzz 的 Rust 移植） | 無 | 無（只有 kerning） | HarfBuzz | 同一套 shaping 模型 |
| 原文範圍 | `PositionedGlyph` 不帶；對回原文靠留著的 parley `Layout`（`Cluster::text_range`） | 無 | 無 | **每個字形帶原文的位元組範圍**（TX-3） | **偏離**：GYO 沒有 parley，排版結果就是唯一的資料；衝突點：Bevy 的編輯能力（hit test、選取、組字）在 parley，GYO 要自己寫 T6 的三個查詢——膠水：中（約 300 行，三個函式＋測試）。不偏離的代價是在 GYO 裡重做一個 parley 的 `Layout` 物件，更多 |
| Shape 與斷行 | shaping 一次、斷行多次（`create_text_measure`；taffy 的 measure 只重跑 `break_all_lines`） | — | — | `Shape` → `ShapedText`；`Layout(ShapedText, 寬度…)` → `TextLayout` | 照 Bevy |
| 換行與字素 | ICU4X 的 UAX #14（字素也是 ICU4X）；沒有日文禁則的嚴格度選項；`complex_script_segmentation` 才做字典切詞 | 無 | 只有硬換行 | UAX #14＋UAX #29；自己實作或用 libunibreak（第 13 節第 1 題）；預設解析、不做字典切詞 | 照 Bevy 的規則；做法交使用者 |
| Fallback | fontique（family、generic、系統字型選用） | 無 | 無 | 呼叫者給字型清單，依序找有字的（第 5.4 節） | 照 Bevy 的 `FontSource::List`；不做 family 名查詢與系統字型（2.2 節） |
| 雙向、script 分段 | parley 做 | 無 | 無 | script 分段做；雙向不做（只 LTR） | 偏離（範圍）：RC-TX1 |
| 快取鍵 | 字型＋face＋大小＋可變軸＋hinting＋smoothing；字形只看 id、**沒有 subpixel** | 整串文字 | 字形＋大小 | 字型＋字形＋大小（1/64 px）＋點陣化方式（第 5.7 節） | 照 Bevy |
| 字形位置 | 小數直接用（smoothing=None 才 floor） | — | 整數 | 排版結果是小數；點陣圖的原點對到整數像素（第 5.7 節） | **偏離**：衝突點只在 R2b 畫的那一步（四捨五入一次）；理由：點陣圖本來就在整數格點上畫，小數位置取樣會糊，且三平台逐位元相同（TX-8）較好驗——膠水：少（一行） |
| 圖集 | `Image` asset、一律 `Rgba8UnormSrgb`、滿了開新頁、不回收 | 無（整串 RGBA8） | 1024² R8、padding 1、多頁 | 純資料、**Alpha8**、預設 1024²、padding 1、多頁、不回收；頁帶格式欄位；變更紀錄給渲染側唯讀（第 5.7 節） | **偏離**：格式 Alpha8（單色字形記憶體四分之一，Object_Connect 同；彩色字形在 RC-TX6 才加 RGBA 頁）；衝突點：渲染側的 shader 是「coverage × 顏色」而不是「貼圖顏色 × 顏色」（Object_Connect `TextPS.hlsl` 就是）——膠水：少（R2b 的 shader 一行）。圖集不是 `Image` asset：GYO 的 `text` 是函式庫、沒有 World 的資產型別，交像素＋頁＋變更紀錄（偵察衝突點 4，膠水：少～中，在 R2a） |
| 執行緒 | Resource 靠 `ResMut` 序列化，單一 System 循序 | — | — | `TextContext` 同一時間只給一個使用者（可以換執行緒）；`FontData`、`ShapedText`、`TextLayout`、`GlyphAtlas` 可以多執行緒同時讀（TX-7） | 照 Bevy |

**歷史的參考**（偵察第 9 點）：`bevy_text` 2020 起用 ab_glyph／glyph_brush_layout（沒有 shaping），2024-07 換 cosmic-text（script 與連字、系統字型），2026-02 換 parley（理由是內部較簡單、要 font collection，不是 shaping 或效能）。換了兩次整層，支持「介面一開始就帶 shaping 與原文範圍」（使用者已定不薄起步）。

## 5. 設計（Contract）

### 5.1 位置與名字（L17，照 N11～N17）

- 目錄 `engine/modules/text/`，標頭 `include/engine/text/`，實作 `src/`。碰外部組件的程式只在 `src/adapters/freetype/`、`src/adapters/harfbuzz/`；adapter 給 `src/` 其他檔用的介面標頭用我們的詞、不含外部組件的型別（同 JSON 的 `ParseWith.hpp`）。Unicode 的換行資料表是生成的、我們的程式（不是外部組件），放 `src/unicode/`，生成腳本 `engine/modules/text/scripts/generate_line_break_table.py`。
- target `gyo_text`（STATIC）、別名 `GYO::Text`；namespace `GYO::Engine::Text`。**同詞異義**（N1）：asset 已有型別 `"text"`（`Asset::Text`，整份文字檔，`engine/modules/asset/include/engine/asset/Text.hpp`）；Module 名 `text` 是總宣告已核准的名字（第 3.3 節），兩者不同：本頁單寫「`text`」指 Module，asset 的型別寫「asset 型別 `"text"`」；`text.zh-Hant.md` 也寫這一句。
- Module 登記檔一列：`text,modules/text,GYO::Text,"Fonts, shaping, line breaking, layout and glyph rasterization into an atlas behind the engine's own interface",yes,no,asset,,,`——`library_only=yes`、`default=no`、依賴 `asset`（字型的 loader；同 audio）。
- 外部組件：`third_party/freetype/`、`third_party/harfbuzz/`，照 JSON 的先例（`SOURCE_SUBDIR` 指向不存在的目錄以不跑上游 CMake、GYO 自建 target、`SYSTEM` 含入），**但下載用官方的 release tarball＋`URL_HASH SHA256`**，不用 git：`GIT_TAG` 填 commit 時不能淺層 clone、HarfBuzz 的 repo 很大，L1 每次全量 clone 會吃進建置時間（第 12 節）。FreeType 用 GYO 自己的 `ftmodule.h` 與選項標頭（`FT_CONFIG_MODULES_H`、`FT_CONFIG_OPTIONS_H` 指向 `third_party/freetype/config/`）：模組只要 sfnt、truetype、cff、psaux、pshinter、psnames、smooth、autofit；關掉 zlib、bzip2、PNG、Brotli、HarfBuzz（所以不收 WOFF／WOFF2，資料契約只收 sfnt）；**關掉 `FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES`**（否則環境變數 `FREETYPE_PROPERTIES` 能改直譯器版本與 hinting 引擎），每個 `FT_Library` 建好後用 `FT_Property_Set` 明定 hinting 相關的屬性（TX-7、TX-8）。HarfBuzz 編 `src/harfbuzz.cc`，不定義任何 `HAVE_*`（hb-ft、CoreText、DirectWrite、GDI、glib、ICU 都不建；Unicode 用它內建的 `hb-ucd`）；HarfBuzz 會讀環境變數 `HB_OPTIONS`，第 1 步查能不能以編譯定義關掉（例：`HB_NO_GETENV`〔知〕），關不掉就停下回報；MSVC 多半要 `/bigobj`，第 1 步查證。
- **建置的後果**（總宣告第 9.1 節末）：`third_party/` 無條件加入、Module 全部都建，所以每個組態（含 Match、三平台 L1）第一次 configure 都下載、都建這兩個函式庫；`harfbuzz.cc` 是一個很重的編譯單位〔知〕。第 9 節判定 6 量。

### 5.2 字型資料與資料契約 `"font"`

```cpp
namespace GYO::Engine::Text {
// Shared immutable bytes of one font file (one face or a collection).
class FontData final {
public:
    static Base::Result<FontData, TextFailure> FromBytes(std::vector<std::byte>&& bytes);
    std::uint32_t FaceCount() const noexcept;
    std::span<const std::byte> Bytes() const noexcept;
private:
    std::shared_ptr<const Detail::FontBytes> data_;   // same shape as Audio::Clip
};

// asset type "font"; AssetTypeName<FontData> is specialized to "font" (the concept requires it)
class FontLoader final {   // Asset::AssetLoader concept
public:
    using Output = FontData; using Settings = FontSettings; using Code = FontLoaderCode;
    static constexpr std::string_view kTypeName = "font";
    static constexpr std::array<std::string_view, 4> kFormats{"ttf", "otf", "ttc", "otc"};
    static constexpr std::size_t kDefaultMaxBytes = 64u * 1024u * 1024u;
    // ParseSettings / Load
};
}
```

**資料契約 `"font"`（版本 1；寫進 `text.zh-Hant.md`）**：
1. 檔是 sfnt（TrueType 輪廓或 CFF 輪廓）或 sfnt 的集合檔；WOFF、WOFF2、Type 1、點陣字型（`.fon`、`.pcf`）不收。
2. 集合檔至少一個 face；每個 face 都能被 HarfBuzz 與 FreeType 開啟、字形數大於 0、有 Unicode 的 `cmap`。
3. 沒有 settings（face 在 `AddFont` 時選）；有 `settings` 也收下、忽略（同 `WavLoader.hpp:76`、`TextLoader` 的先例）。
4. 違反回 `FontLoaderCode`（`NotSfnt`、`EmptyCollection`、`FaceUnreadable`、`NoGlyphs`、`NoUnicodeCmap`；碼的名字在實作的第 3 步定）。文件裡的錯誤碼前綴 TX-E（總宣告第 9.2 節末「每個 Module 的 DI 與錯誤碼用自己的前綴」）：`FontLoaderCode` 與 `TextFailureCode` 的每個值在 `text.zh-Hant.md` 以 TX-E1 起編號。
- `Load` 照 asset 的規則：沒有 World、沒有時鐘、沒有全域可變狀態；每次呼叫用自己的暫時物件（自己的 `FT_Library`、`hb_blob`／`hb_face`），所以可以在 asset 的執行緒上同時跑（L17：兩個 adapter 都驗，loader 收了的字型 `AddFont` 只會因記憶體不足而失敗）。
- 字型檔不可信的程度：字型來自產品自己的資產（作者的內容），不是網路上的任意輸入；FreeType、HarfBuzz 都有 fuzzing〔知〕。範囲の限界寫明。

### 5.3 `TextContext`、字型的登記、樣式

```cpp
struct FontId { std::uint32_t value; };            // index inside one TextContext

class TextContext final {                          // one user at a time; may change threads
public:
    TextContext();                                 // owns its FT_Library; no global state
    Base::Result<FontId, TextFailure> AddFont(const FontData& data, std::uint32_t faceIndex);
    FontInfo Info(FontId) const;                   // family and style names, units per em
    ShapedText Shape(std::string_view utf8, std::span<const TextSpan> spans);
    TextLayout Layout(const ShapedText& shaped, const LayoutOptions& options);
    RasterResult Rasterize(const TextLayout& layout, GlyphAtlas& atlas);
};

struct TextStyle {
    std::span<const FontId> fonts;   // fallback order; fonts[0] is the primary
    float sizePx;                    // pixels per em
    LineHeight lineHeight;           // RelativeToFont(1.2) by default, or Px
};
struct TextSpan { std::uint32_t byteBegin, byteEnd; TextStyle style; };   // cover the text in order
struct LayoutOptions { std::optional<float> maxWidth; HorizontalAlign align; /* Left, Center, Right */ };
```

- 一段文字可以有多個 span（不同字型清單、大小、行高）；顏色不在 `text`——每個字形帶它的 span 序號，畫的人照序號上色（同 Bevy 的 `section_index`）。L17：照 Bevy 的 rich text；介面一開始就收 span，免得 `ui` 要多種樣式時改簽名。
- 單位是像素、y 向下、原點在排版框的左上角。字級是呼叫者給的像素（DPI 的縮放由呼叫者乘，總宣告 `window` 的「邏輯／像素大小」）。
- 不合法的 UTF-8：每個最大的不合法子序列換成 U+FFFD 去 shaping，原文範圍仍是那幾個位元組（Unicode 建議的做法；Object_Connect 同）。不是錯誤（L17：外部來的字串很常見，排版不失敗）。
- span 沒有照順序蓋滿整段、`FontId` 不屬於這個 context、空的字型清單、`sizePx` 不是正的有限數：Programmer Error（`GYO_ASSERT`）。
- **context 的身分**：`FontId` 只在建立它的 `TextContext` 裡有意義。`TextContext` 持有一個堆上的身分物件（`std::shared_ptr` 的控制區塊），`ShapedText` 與 `GlyphAtlas` 記它的 `std::weak_ptr`，以 owner 的等價比較——不比位址（context 放進 Resource 會被搬移；銷毀後同一位址會被重用），也不用全域的計數器（TX-7）。`ShapedText` 拿去別的 context `Layout`、`GlyphAtlas` 交給別的 context `Rasterize`（圖集在第一次 `Rasterize` 時綁定那個 context，快取鍵裡的 `FontId` 才不會撞），都是斷言。
- **`ShapedText` 自己擁有樣式**：`TextStyle::fonts` 是不擁有的 `std::span`，`Shape` 把每個 span 的字型清單與行高複製進 `ShapedText`，之後 `Layout` 不再讀呼叫者的記憶體。
- 字型不能移除（v1）。Bevy 移除時整個 collection 重建；GYO 有點名的需要時再加（RC-TX2 一起看）。

### 5.4 Shaping 與原文範圍

1. **分段**：依 span → 依字素（UAX #29 的字素邊界，資料來源照第 13 節第 1 題；HarfBuzz 沒有公開的字素切分）選字型：字型清單裡第一個「字素裡每個字元都有字形」的字型，**檢查時跳過 default-ignorable 的字元**（ZWJ、變體選擇符、IVS 等，`Default_Ignorable_Code_Point`；HarfBuzz 本來就把它們藏起來）；都沒有就用主字型（缺字畫 `.notdef`）→ 依 script（Common、Inherited 跟著前後；HarfBuzz 的 `hb_unicode_script`）。
2. **Shaping**：每段一次 HarfBuzz（`HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES`；方向 LTR；語言由 span 帶、預設不設）。kerning、連字照字型的 `GPOS`／`GSUB` 預設 feature。
3. **度量只有一個來源**（TX-2）：前進量、偏移、行的上下高度（ascent、descent、line gap）只來自 HarfBuzz（內建的 OpenType 字型函式，讀 `hmtx` 等表、未 hinting）；FreeType 只畫點陣圖與它的 bearing。FreeType 的 light hinting 只調垂直的輪廓，但它回報的前進量會捨成整數像素〔FreeType 文件〕——本設計**不用 FreeType 的前進量**，所以游標的位置與畫出來的位置照樣一致。不用 hb-ft（總宣告 Q6）。
4. **定點**：排版內部全部用 26.6 的整數（HarfBuzz 的單位）算，包括行高、half-leading、對齊的位移；最後才轉成 `float`（26.6 的值轉 `float` 是精確的，範圍內）。所以三平台結果相同（TX-8），測試也能用精確等式（第 8.4 節）。
5. **原文範圍**（TX-3）：`ShapedText` 與 `TextLayout` 的每個字形帶 `[byteBegin, byteEnd)`；相鄰的 cluster 不重疊、依原文順序單調、合起來蓋滿整段原文（每個位元組恰好屬於一個 cluster）。一個字形對多個字（連字 `fi`）＝那個字形的範圍有兩個字；一個字對多個字形（拆分、加結合符）＝那些字形共用同一個範圍。

```cpp
struct PositionedGlyph {
    FontId font; std::uint32_t glyph;          // glyph id in that face
    float x, y;                                // final pen position on the baseline in the layout box
                                               // (the line's alignment offset is already added)
    float advance;
    std::uint32_t byteBegin, byteEnd;          // the cluster in the source text
    std::uint32_t span, line;
};
struct LayoutLine {
    float top, baseline, height, width, x;     // x = the line's alignment offset (already in each glyph's x)
    std::uint32_t glyphBegin, glyphEnd, byteBegin, byteEnd;
};
struct TextLayout {
    std::vector<PositionedGlyph> glyphs;       // logical order
    std::vector<LayoutLine> lines;
    float width, height, firstBaseline;
};
```

### 5.5 換行

- **換行機會**：照 UAX #14（第 1 步時的最新 Unicode 版本〔多半是 18.0，知〕；做法照第 13 節第 1 題）。自己實作時，表由 Unicode 的資料檔生成（`LineBreak.txt`、`EastAsianWidth.txt`、`emoji-data.txt` 的 `Extended_Pictographic`、`DerivedGeneralCategory.txt`；字素另要 `GraphemeBreakProperty.txt`、`DerivedCoreProperties.txt`），生成的表放進 repo、腳本也放進 repo、來源檔的 SHA-256 寫在生成檔的檔頭；用 libunibreak 時，換行與字素都經 `src/adapters/libunibreak/`，`Default_Ignorable_Code_Point` 的小表仍由 `DerivedCoreProperties.txt` 生成。UAX #14 的預設解析：`CJ`（小的假名、長音符）當 `NS`——行首不放小的「ゃ」「っ」「ー」（嚴格的禁則）；`AI`、`SG`、`XX` 當 `AL`；`SA` 依一般類別當 `CM` 或 `AL`（不做字典切詞，同 Bevy 沒開 `complex_script_segmentation`）。
- **硬換行**：`BK`、`CR`、`LF`、`NL`（`CR LF` 是一個）。
- **依寬度斷行**：在 `ShapedText` 上累加前進量，超過 `maxWidth` 時退回最後一個換行機會；沒有換行機會（一個比框還寬的字詞）就在那裡溢出（不硬切；CSS 的 `overflow-wrap: normal`）。斷點落在 HarfBuzz 標「不安全斷開」（`HB_GLYPH_FLAG_UNSAFE_TO_BREAK`）的字形上時，該行重新 shaping 那一段（HarfBuzz 建議的做法）。行尾的空白不算寬度。
- **對齊**：每行依 `align` 算 `x`；基準寬度是 `maxWidth`，沒有給 `maxWidth` 時是最寬那一行的寬度（`TextLayout::width`）。垂直對齊（上、中、下、第一條 baseline）是呼叫者的事：`TextLayout` 給 `height`、`firstBaseline` 與每行的度量（Object_Connect 的四種都算得出來）。
- **行高**：`RelativeToFont(k)`＝該行最大字級×k；`Px(v)`＝固定；baseline 放在行高的中線依 ascent、descent 的比例（L17，同 CSS 的 half-leading）。

### 5.6 排版結果的查詢（游標、點擊、範圍）

```cpp
enum class CaretAffinity : std::uint8_t { Downstream = 0, Upstream = 1 };
struct Caret { std::uint32_t byteOffset; CaretAffinity affinity; };
CaretRect CaretAt(const TextLayout&, Caret caret);                          // x, top, height, line
Caret HitTest(const TextLayout&, float x, float y);                         // nearest caret
void RangeRects(const TextLayout&, std::uint32_t byteBegin, std::uint32_t byteEnd,
                std::vector<Rect>& out);                                     // selection, IME underline
```

- 只從 `TextLayout` 的字形範圍、字素邊界與行算（TX-3），不再 shaping、不碰字型。`TextLayout` 帶字素邊界的清單（`Shape` 時算好）。
- **affinity**：軟換行處同一個位元組位置可以是上一行的行尾（`Upstream`）或下一行的行首（`Downstream`）；`CaretAt` 照 affinity 選行，`HitTest` 點在行尾的右邊時回 `Upstream`（同 parley 的 `Cursor` 帶 affinity〔知〕）。現在就放進簽名，`ui` 的文字框之後不必改。
- 游標只停在字素邊界（不停在基底字與結合字元之間）；一個 cluster 包含多個字素（連字 `fi`）時，cluster 內的位置照字素數平分那個字形的前進量（同 Chrome、parley 對連字的做法〔知〕）。位元組位置不在字素邊界上是斷言。
- `HitTest`：先找 y 落在的行（上方的取第一行、下方的取最後一行），再在行內找最近的游標位置。
- 組字的底線＝`RangeRects(組字的範圍)`；選取＝`RangeRects(選取的範圍)`（Bevy 的組字底線也是「組字的位元組範圍 ∩ cluster 的範圍」，偵察第 2 點）。

### 5.7 點陣化與圖集

```cpp
enum class AtlasFormat : std::uint8_t { Alpha8 = 1 /* Rgba8 reserved, RC-TX6 */ };
class GlyphAtlas final {                       // plain data; many readers at once
public:
    explicit GlyphAtlas(AtlasOptions options = {});   // pageSize 1024, padding 1
    std::size_t PageCount() const;
    AtlasPageView Page(std::size_t index) const;      // format, width, height, row pitch, pixels (row 0 = top)
    std::uint64_t Revision() const;                   // grows with every change
    AtlasChanges ChangesSince(std::uint64_t revision) const;   // rects per page, or "whole page"
};
struct GlyphPlacement { std::uint32_t page; Rect atlasRect; float left, top; GlyphRasterStatus status; };
struct RasterResult { std::vector<GlyphPlacement> placements; /* one per layout glyph */ std::uint32_t newGlyphs, tooLarge; };
```

- 快取鍵＝(字型〔綁定的 context 裡的 `FontId`〕、字形、字級〔26.6〕、點陣化方式〔light hinting＋灰階反鋸齒；只有這一種，欄位留著〕)。同一個鍵只畫一次、只進圖集一次（TX-6）。不做 subpixel 的位置（同 Bevy）。一個圖集只服務一個 context（第 5.3 節）。
- FreeType 載入字形一律加 `FT_LOAD_NO_BITMAP`：字型內嵌的點陣（EBDT 的 1-bit、CBDT 的 BGRA）不用，只從輪廓畫，格式才一定是 Alpha8（彩色字形在 RC-TX6）。
- 點陣圖的原點對到整數像素：`GlyphPlacement` 給 bearing（`left`、`top`），畫的人把 `round(glyph.x) + left`、`round(baseline) - top` 當左上角（第 4 節的偏離）。
- 頁的佈局：Alpha8、row 0 在上、y 向下、row pitch＝寬（TX-6）。上下翻轉不在 T（R2b 在 UV 轉換的單一邊界處理，master 坑 17）。
- 打包：shelf（一排一排放），每個字形周圍留 `padding`；頁滿了開新頁；放好的字形不移動、不回收（v1）。
- 字形比一頁還大（極大的字級）：`status = TooLarge`、不畫、計數；不是錯誤（照「告訴外面發生了什麼」；畫的人可以選擇不畫或換做法）。
- **變更紀錄**：圖集每次改變 `Revision()` 加一，並記下（新版號、頁、矩形）。渲染側記住自己上傳到的版號，用 `ChangesSince` 拿之後的矩形上傳——渲染側只讀、不清除（DI-KS 13：渲染 World 經 Extract 唯讀）。紀錄超過上限（預設 4096 筆）時，寫的一方把較舊的合併成「這一頁整頁變了（到版號 r）」；版號比 r 舊的讀者整頁上傳。這是 R2a 的「CPU 像素貼圖的局部更新」要的輸入（L17；Bevy 是 `Image` asset 改變就整張重傳）。

### 5.8 錯誤、執行緒、記憶體

- **錯誤**（N25：E2 用 `…Failure`）：`TextFailure`（`Base::Error<TextFailureCode>`）給 `FontData::FromBytes`、`AddFont`（字型讀不進、face 序號超出、記憶體不足）。排版與點陣化不回錯誤：缺字畫 `.notdef`、不合法 UTF-8 換 U+FFFD、太大的字形回狀態。配置失敗在 adapter 的邊界攔成 `OutOfMemory`（`AddFont`）；排版時的配置失敗照 C++ 的例外往外（同 Engine 其他的容器操作）。
- **執行緒**（TX-7）：沒有全域可變狀態（FreeType 的 `FT_Library` 由每個 `TextContext`、每次 `Load` 自己建；環境變數不改行為，第 5.1 節）；`TextContext` 同一時間只給一個使用者；`FontData`、`ShapedText`、`TextLayout`、`GlyphAtlas` 的 const 操作可以多執行緒同時做。放進 World 時是一般的 Resource（寫的人獨占即可），不需要 `threadBound`（FreeType 不在乎哪條執行緒，只在乎不同時用）。
- **記憶體**：圖集不回收（2.2 節），範囲の限界與 RC-TX2；每頁 1 MB（1024² Alpha8）。

## 6. Architecture Delta（只寫和總宣告第 11 節的差分）

總宣告第 11 節已列 `text` 這一列與「外部組件 FreeType、HarfBuzz 在 `third_party/`」。本批的差分：
1. **Feature**：F1、F2（第 3.2 節）。
2. **問題**：`gyo2` 沒有字型與文字；master 的 `GYO::Text` 只有整串點陣化、`gyo.ui` 拿點陣化代替量字。
3. **變的邊界**：新 Module `engine/modules/text`；新資料契約 `"font"`（版本 1）；`third_party` 多兩個不分建置種類都加入的組件；Module 登記檔一列。
4. **影響**：新增 `engine/modules/text`、`third_party/freetype`、`third_party/harfbuzz`、`config/modules.csv` 一列、`tests/common/text`。既有 Module 都不改。
5. **依賴**：`GYO::Text → GYO::Asset`（登記檔的 `depends`）、`GYO::Text → GYO::Base`（Asset 對 Json 是 `private_depends`，`config/modules.csv:5`，`text` 看不到 Json）；`gyo_text → gyo_freetype、gyo_harfbuzz`（PRIVATE）。沒有循環；還沒有 Module 依賴 `text`（U 的 `ui` 之後依賴它）。
6. **Ownership**：字型、shaping、換行、排版、點陣化、圖集的佈局歸 `text`；FreeType、HarfBuzz 的版本與建置歸 `third_party`，Engine 裡的使用者只有 `text`。
7. **為什麼不能更小**：見總宣告第 11 節第 7 項與 Q6、`-text-no-thin-start`（FreeType＋HarfBuzz 一起實作是使用者定的）。

**Batch declaration checks**（AGENTS §4）：

| 選擇 | 系譜 | 退掉的選項與決定理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 排版結果每個字形帶原文的位元組範圍 | 新導出（HarfBuzz 的 cluster）；Bevy 經 parley 的 `Cluster::text_range` 取得同樣的資訊 | 照 master 只給點陣圖：拿點陣化代替量字的坑（偵察第 2 節）；照 Bevy 的 `PositionedGlyph` 不帶範圍：GYO 沒有 parley 可以回查 | F2；`gyo.ui`、KeepTool 以外的工具、PvP 大廳 |
| Shape 與斷行分開 | Bevy（shaping 一次、斷行多次） | 一個 `Layout(text, width)`：`ui` 同步量字時每換一次寬度就重新 shaping | F2 |
| 換行照 UAX #14、字素照 UAX #29 | Bevy／parley 的 ICU4X；Unicode 的規格 | 做法（自己實作／libunibreak／簡化／只硬換行）：第 13 節第 1 題 | F2（游標停在字素）、F3 |
| 游標帶 affinity | parley 的 `Cursor` | 只用位元組位置：軟換行處分不出上一行的行尾與下一行的行首，之後要改簽名 | F2 |
| 圖集是純資料、Alpha8、變更紀錄 | Object_Connect（R8、多頁、局部上傳）；Bevy（多頁、不回收） | Bevy 的 RGBA8 `Image`：記憶體四倍、`text` 是函式庫沒有資產型別；渲染側清除 dirty：違反 DI-KS 13 | F1、F5 |
| 字型的 loader 放 `text`、`text` 依賴 `asset` | audio `WavLoader` 的先例；Bevy `FontLoader` 在 `bevy_text` | loader 放 U／R2b：資料契約 `"font"` 的知識（sfnt、集合檔）會離開擁有字型的 Module | F6 |
| 外部組件以 FetchContent 抓官方 release tarball＋SHA-256、GYO 自建 target | JSON Module 的 yyjson、fast_float（那裡用 git＋commit） | 跑上游 CMake：FreeType 的上游 CMake 會自動找 zlib、PNG、HarfBuzz（〔知〕），組態之間結果不同；用 git＋commit：不能淺層 clone、HarfBuzz 的全史很大 | — |

既有的測試、守衛、golden 都不受影響；「不必改測試」沒有被當成決定的理由。

## 7. Design Invariants（新頁 `target/v2/text.zh-Hant.md`，見出し `Design Invariants`）

本批新增，最終文字在合併前核准定稿。條號前綴 TX-（總宣告第 9.2 節末的分配）。行為的測試與 `gyo_expect_links` 和機構同時立；綁實作方式的含入掃描在情境測試通過之後立（AGENTS §3「ガードを立てる時期」）。

| 條 | 內容 | 守衛 |
|---|---|---|
| **TX-1 唯一的 adapter** | FreeType 的標頭、型別、呼叫只在 `engine/modules/text/src/adapters/freetype/`（與它自己的測試 `tests/common/text/adapters/freetype/`），HarfBuzz 的只在 `src/adapters/harfbuzz/`（與 `tests/common/text/adapters/harfbuzz/`）；第 13 節第 1 題選 libunibreak 時同樣加 `adapters/libunibreak/`；`include/` 不含、不露出它們的型別；不用 hb-ft（兩個 adapter 各自從 `FontData` 的位元組建自己的物件，`src/` 的其他檔只經兩個 adapter 的介面相接）；只有 `gyo_text`（與 adapter 自己的測試）連 `gyo_freetype`、`gyo_harfbuzz`。數法：FreeType 一個、HarfBuzz 一個；Unicode 的資料表是生成的我們的程式，不是外部組件 | `gyo_expect_links(gyo_text GYO::Base GYO::Asset gyo_freetype gyo_harfbuzz)` 與 interface 不露出；一般的測試只拿到 `include/` 的路徑（公開標頭含外部標頭就編不過）；含入與連結掃描（情境測試之後）：根是整個 repo 的原始碼與 CMake（`engine/`、`plugins/`、`games/`、`tools/`、`tests/`、`examples/`），`ft2build.h`／`freetype/`、`hb.h`／`hb-*.h` 只准在各自的兩處，`hb-ft.h` 哪裡都不准，`gyo_freetype`／`gyo_harfbuzz` 只准出現在 `third_party/` 與 `engine/modules/text/`、`tests/common/text/adapters/` 的 CMake（`plugins/imgui` 走 imgui_freetype 就會被抓到） |
| **TX-2 度量一個來源** | 字形的前進量、偏移、行的上下高度只來自 HarfBuzz adapter；點陣圖與 bearing 只來自 FreeType adapter；不用 FreeType 回報的前進量（light hinting 會把它捨成整數像素） | adapter 的測試（`tests/common/text/adapters/`）：在 font units 上比——FreeType 用 `FT_LOAD_NO_SCALE` 讀的前進量等於 HarfBuzz 以 scale＝upem 給的前進量（兩邊都讀 `hmtx`），對測試字型的每個字形；排版的寬度等於 HarfBuzz 前進量的和 |
| **TX-3 原文範圍** | 每個字形帶 `[byteBegin, byteEnd)`；cluster 依原文順序單調、不重疊、蓋滿整段（每個位元組恰好屬於一個 cluster）；連字＝一個字形多個字，拆分＝多個字形同一範圍；游標只停在字素邊界、帶 affinity；`CaretAt`、`HitTest`、`RangeRects` 只從排版結果算 | 合成測試字型的連字、拆分、結合字元、fallback 交界、不合法 UTF-8、軟換行處的 affinity 的案例，逐字形比範圍（精確等式）；性質測試：任意字串的 cluster 蓋滿與單調 |
| **TX-4 換行與字素** | 換行機會照 UAX #14、字素照 UAX #29（版本寫在文件與生成檔）的預設解析；硬換行；寬度斷行退回最後一個換行機會、沒有就溢出；行尾空白不算寬度 | Unicode 官方的 `LineBreakTest.txt`、`GraphemeBreakTest.txt` 全部通過（件數斷言；不論自己實作或 libunibreak）；日文禁則與混排的案例；生成檔的來源雜湊 |
| **TX-5 Fallback 與缺字** | 每個字素選字型清單裡第一個有全部字形的字型；都沒有用主字型的 `.notdef`；不合法 UTF-8 的每個最大不合法子序列換成 U+FFFD、範圍保留原位元組；不查系統字型 | 測試（合成字型 A 只有拉丁、B 只有假名；第三種字元都沒有） |
| **TX-6 圖集** | 同一個快取鍵只畫一次、只進圖集一次；頁是 Alpha8、row 0 在上、row pitch＝寬；放好的字形不移動、不回收；`Revision` 單調增加，`ChangesSince(r)` 給 r 之後所有變過的矩形（或整頁）；比一頁大的字形回 `TooLarge`、不是錯誤 | 測試：同一段文字點陣化兩次、第二次 `newGlyphs == 0` 且版號不變；`ChangesSince` 拼回的頁＝實際的頁（含紀錄合併之後）；`TooLarge` |
| **TX-7 執行緒與狀態** | 沒有全域可變狀態；每個 `TextContext`、每次 loader 的 `Load` 自己建 FreeType 與 HarfBuzz 的物件；`TextContext` 同一時間只給一個使用者；`FontData`、`ShapedText`、`TextLayout`、`GlyphAtlas` 的 const 操作可多執行緒同時做 | 多執行緒：8 條執行緒各自一個 `TextContext` 同時排版同一份 `FontData`、結果與單執行緒相同；loader 在 asset 的執行緒上同時 `Load`（ThreadSanitizer 不在 L1，範囲の限界） |
| **TX-8 跨平台相同** | 同樣的字型、文字、選項，排版結果（每個字形的 id、位置、範圍、行）與點陣圖的位元組在 L1 三平台相同；排版內部用 26.6 定點；環境變數（`FREETYPE_PROPERTIES`、`HB_OPTIONS`）不改結果；不為某個平台開分支 | 排版結果與圖集頁的 SHA-256 golden（L1 三平台） |
| **TX-9 不知道 ECS 與產品** | 只依賴 Base 與 Asset（Asset 再依賴 Json）；不依賴 Kernel、編排層、時間；不進 World（`library_only=yes`）；名字與註解不含遊戲或產品的概念（N5、N6） | `gyo_expect_links`（同 TX-1）、`cmake.UnitRegistry`；審查 |
| **TX-10 資料契約 `"font"` 版本 1** | 第 5.2 節的四條；loader 收了的字型 `AddFont` 只會因記憶體不足失敗 | 每條規則一個拒絕的測試（位元組在測試裡造）；收集合檔的每個 face |

### Replacement Criteria（新 Component，寫在積上去之前）

- **RC-TX1 方向與 script**：點名雙向文字或直排的 Feature 出現，而 TX-3 的「依原文順序單調」表達不了視覺順序與邏輯順序的分離 → 停下，交使用者重看排版結果的形狀（不在查詢函式裡塞特例）。
- **RC-TX2 圖集的記憶體**：有點名的 Feature 量到圖集超過它宣告的記憶體預算，或需要移除字型 → 加回收與字型的移除；TX-6 的「不移動、不回收」要改，交使用者。
- **RC-TX3 兩個 adapter 的度量**：TX-2 守不住（例：某種字型 FreeType 與 HarfBuzz 的度量不同，必須逐字形修正），或需要 hb-ft → 停下，交使用者（hb-ft 是 AGENTS §6 的例外）。
- **RC-TX4 換行的規則**：要的換行超出 UAX #14 的預設解析（例：日文的寬鬆禁則切換、泰文的字典切詞），且要在 `text` 裡長出第二套規則 → 評估換成更完整的實作（ICU 一類），交使用者；選了自己實作而 Unicode 升版時一致性測試的修補反覆出現 → 評估改用 libunibreak。
- **RC-TX5 每幀的成本**：第 9 節判定 7 的停止條件觸發，或 U、R2b 量到排版超過它們的幀時間預算 → 停下重看（排版結果的快取、shaping 的快取），不在呼叫端各自繞。
- **RC-TX6 彩色字形**：點名的 Feature 要 emoji → 加 RGBA 頁（`AtlasFormat::Rgba8`）與 FreeType 的彩色字形；TX-6 的格式條文要改。

## 8. 測試、守衛、突變

### 8.1 測試先行

每一步先寫會失敗的測試（Red，記下失敗的輸出）再實作（Green）。新測試在 `tests/common/text/`，執行檔 `gyo_text_tests`，標籤 `cpu`。

### 8.2 測試用字型

- **合成字型**（精確值的根據）：用 fontTools（第 12 節，開發時用、L1 不需要）寫的生成腳本 `tests/common/text/fonts/generate_test_fonts.py` 產生；`unitsPerEm` 1024（2 的冪：HarfBuzz 的縮放 `x_mult=(scale<<16)/upem` 在 upem 為 1000 時會截斷，期望值不好推）、每個字形的前進量與輪廓（方塊）由腳本定，期望值照 HarfBuzz 的定點算法從設計值推出來，不靠 Agent 的理解（AGENTS §14.2）。內容：TrueType 輪廓版與 CFF 輪廓版各一、兩者合成的集合檔一個；`GSUB` 的連字（`f`＋`i`→一個字形）與拆分（一個字→兩個字形）、`GPOS` 的 kerning（`A`＋`V`）、結合字元；字型 A 只有拉丁與標點、字型 B 只有假名與漢字（fallback 用）。生成檔放進 repo，腳本與 fontTools 的版本寫在 `fonts/README.md`。
- **實際的字型**：BIZ UDPGothic（Object_Connect 用的那份，本機 `~/Code/GitHome/Object_Connect/NoviceResources/fonts/`，OFL 1.1、沒有 Reserved Font Name）的子集（只留測試用到的約 100 字），同 OFL 授權檔放在 `tests/common/text/fonts/`。用來驗真的 TrueType 日文字型的 hinting 與點陣圖 golden。只給測試用，不隨產品打包。

### 8.3 Unicode 的換行一致性

- `LineBreakTest.txt`、`GraphemeBreakTest.txt`（第 1 步時最新 Unicode 版本的官方測試，第 12 節）轉成 `tests/common/text/vectors/`（同時放 Unicode 的授權告示），測試逐行比對換行機會與字素邊界；件數寫成常數、測試斷言讀到的件數（防截斷）。第 1 步先讀兩個檔的檔頭有沒有寫 tailoring（舊版的 `LineBreakTest` 曾用過 LB25 的 tailoring），有的話照檔頭寫的規則實作 tailoring 或在判定 3 寫明哪幾行、為什麼——在看結果之前寫進 dev_log。
- 生成的表與測試向量都放進 repo（L1 不連 unicode.org）；生成腳本可以在本機重跑，產物的 SHA-256 不變。

### 8.4 精確值與 golden

- 量字：合成字型的寬、高、baseline、行高、每個字形的位置與範圍，期望值由字型的設計值照 HarfBuzz 的定點算法與本 Module 的 26.6 規則（第 5.4 節第 4 點）推出（精確等式；排版的值都是 1/64 的倍數，轉 `float` 精確）。
- 點陣圖：合成字型與 BIZ UDPGothic 子集在 12、17、24、45 px 的指定字串，圖集頁的 SHA-256 golden；第一次本機算出，L1 三平台綠才算成立（TX-8）。
- 不重複進圖集：同一段文字點陣化兩次，第二次 `newGlyphs == 0`。

### 8.5 情境測試

- 「HUD 每幀重組」（F1）：1,000 幀，每幀用 BIZ UDPGothic 子集排 30 段像「残り N / M」的字串（N、M 每幀變）、置中、點陣化；從第 2 幀起新字形只有沒出現過的數字；圖集的頁數與變更紀錄的矩形數對得上；量寬縮字級的流程（寬度超過就把字級減 1 再量）得到預期的字級。
- 「文字框」（F2）：一段日英混排、含連字的字串；組字中插入「がっこう」，`RangeRects` 給的底線矩形覆蓋組字的字形；每個位元組位置 `CaretAt` → `HitTest` 往返回到同一個位置；在連字中間的游標落在平分的位置。
- 「說明文字換行」（F3）：一段日英混排的文字在寬 300 px 斷行，禁則與英文字詞的斷點符合期望；換一個寬度只重跑 `Layout`、不再呼叫 `Shape`（計數的是 `Shape` 的呼叫次數；`Layout` 在不安全斷開處的局部重新 shaping 另計、照記錄）。軟換行處的游標在兩種 affinity 下落在不同的行。

### 8.6 守衛（AGENTS §3「ガードを立てる時期」）

- 和測試同時立：`gyo_expect_links(gyo_text …)` 與 interface 不露出；一般的測試執行檔只拿到 `include/` 的路徑，adapter 自己的測試（`tests/common/text/adapters/<組件>/`）另一個執行檔（TX-1、TX-9）。
- 情境測試通過之後立：TX-1 的含入與連結掃描（Python，照 JSON 的 J-1 掃描）。範圍與規則見 TX-1（整個 repo 的原始碼與 CMake，含 `plugins/`）。自我測試三件：規則全關的突變被抓到、實際樹上的正例被數到、掃描根不存在時失敗（AGENTS §14.2：跨平台、不丟 stderr、先用正例命中）。

### 8.7 突變（AGENTS §14.2、§14.5、`tests/common/mutation/README.md`）

- 清單 `tests/common/text/mutations.json`，以 `tests/common/mutation/run_mutations.py` 跑；每個突變寫預期失敗的測試名或訊息片段、`guards` 寫守的條文（例：`"text TX-3"`）。
- 時點：每一步寫它的測試時一起寫突變，那一步只跑相關的（D184）；第 8 步把整份清單交 **Opus／high** 敵對式檢查（Fable 用量不足，作者與檢查者同模型，報告寫明），補進去、完整跑一次。
- 條數的估計：TX-1 守衛 6（含連結規則）、TX-2 2、TX-3 約 10（單調、蓋滿、連字、拆分、平分、字素邊界、affinity、`HitTest` 的行選擇、範圍矩形的頭尾、U+FFFD 的範圍）、TX-4 約 8（預設解析的四類、硬換行、退回、溢出、行尾空白）、TX-5 4（含 default-ignorable）、TX-6 約 6、TX-7 2（含環境變數）、TX-10 5——約 43，在 README 的「10 條 × 5＝50」之內。

### 8.8 既有測試與守衛

不改任何既有的判定（AGENTS §14.4 不適用）。`cmake.UnitRegistry` 讀實際的登記檔，`engine/modules/text` 有 `CMakeLists.txt` 就要有登記列——本批加列，判定不變。

## 9. 判定（先宣告，結果出來之後不放寬）

1. **通過**：全量 `ctest` 本機綠；L1 三平台（linux-x64、windows-x64、macos-arm64）綠。
2. **精確值**：第 8.4 節的量字與原文範圍全部精確相等（三平台）。
3. **換行與字素的一致性**：`LineBreakTest.txt`、`GraphemeBreakTest.txt` 全部通過（件數＝常數）；不跳過任何一行（檔頭寫明的 tailoring 照第 8.3 節在看結果之前處理）。不通過的停下回報，不加例外清單。
4. **跨平台**：排版結果與點陣圖的 golden 在 L1 三平台 **0 不一致**（TX-8）。任一平台不一致→停、回報，不為平台開分支。
5. **檢出力**：突變清單（Opus 檢查補過後）全部 killed（預期的那個失敗）；沒殺的停下回報。
6. **建置時間（停止條件，不是受理的閘門；門檻在第 13 節第 3 題交使用者）**：本機（macOS x86_64）乾淨建置，從 `.ninja_log` 讀 `gyo_freetype`、`gyo_harfbuzz` 各物件檔的編譯時間（合計與最長的一個）並記錄；L1 三平台的 job 時間只記錄（CI 的時間雜訊大，不當門檻）。**停止條件**：本機兩個函式庫的編譯時間合計超過 5 分鐘，或單一物件檔（`harfbuzz.cc`）超過 3 分鐘 → 停下回報（每個組態都要付這筆，總宣告第 9.1 節末）。
7. **每幀成本（停止條件；門檻在第 13 節第 3 題交使用者）**：本機量 BIZ UDPGothic 子集、24 px、約 12 字的日文字串的 `Shape`＋`Layout`（圖集已熱）的中位數。**停止條件**：超過 50 µs → RC-TX5，停下回報。**門檻的出處**：計畫裡沒有文字排版的預算（總宣告第 4.1 節：幀時間預算由 R2a、R2b、K 的宣告各自寫）；50 µs 是卡片推的——Object_Connect 每幀約 30 段字串（偵察第 5 節），30 段＝1.5 ms，是 60 fps 一幀（16.7 ms）的 9%，留給畫面與遊戲邏輯。R2b 的宣告定正式的預算時，這一條跟著改。
8. **件數的突き合わせ**：`LineBreakTest`、`GraphemeBreakTest` 的件數＝常數＝比對數；情境 1,000 幀×30 段；突變 killed／總數。不一致時照 AGENTS §14 冒頭。

## 10. 實作的步驟（動工前核准之後）

段：**上段**（公開介面、資料契約 `"font"`、外部組件 adapter；navigator）。模型／檔位：實作者 Opus／high（本卡）；審查者（設計審查）Opus／high；突變清單的敵對式檢查 Opus／high（**Fable 用量不足期間，設計審查與突變檢查都和作者同為 Opus**——批次表〈每批的設計審查〉要求審查者換模型、navigator 要求檢查者換模型，做不到，照 navigator「同じモデルで行い、報告に書く」寫進合併前核准頁；Fable 恢復時改回 Fable）；§14.4 的檢證者不需要（沒有判定變更）。碰到原因不明的失敗照 §14 停下回報，不自己升檔。

| 步 | 內容 | 完成的樣子 |
|---|---|---|
| 第 1 步 | 下載與查證（第 12 節，使用者同意後）：tarball 的 SHA-256（與上游公布的簽章或雜湊對照）；FreeType 的模組清單、選項標頭、`FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES` 關掉、三平台編譯（`SYSTEM`、GYO 的編譯選項不套）；HarfBuzz 單檔彙整在三平台編譯、不定義 `HAVE_*`、`HB_OPTIONS` 能否關、MSVC 的 `/bigobj`；FTL 的 credit 原文；Unicode 的最新版本、資料檔與兩個測試檔的檔頭（tailoring）；（選 (丁) 時）libunibreak 的版本與它支援的 Unicode 版本；fontTools 裝進 `build/` 底下的 venv；推 draft PR、手動 dispatch L1 看三平台 | 本機與 L1 三平台編得過；查證記 dev_log。不過＝停、回報 |
| 第 2 步 | 骨架：登記檔一列、CMake、`gyo_expect_links`、`TextFailure`；先讓 `cmake.UnitRegistry` 抓到缺列（Red）再加列；建置時間量測（判定 6） | 全量 ctest 綠 |
| 第 3 步 | 字型：測試字型的生成與子集、`FontData`、`FontLoader`（資料契約四條）、`AddFont`、`Info` | TX-10、TX-7 的 loader 部分 |
| 第 4 步 | Shaping：分段、fallback、script、HarfBuzz、原文範圍、U+FFFD、`TX-2` 的度量比對 | TX-2、TX-3、TX-5 |
| 第 5 步 | 換行與字素：（照第 13 節第 1 題）生成腳本與表、UAX #14／#29 的規則或 libunibreak 的 adapter、兩個一致性測試、寬度斷行、不安全斷開時重新 shaping、對齊、行高；查詢函式（T6，含 affinity） | TX-4、判定 3；TX-3 的查詢部分 |
| 第 6 步 | 點陣化與圖集：FreeType 點陣化、快取鍵、shelf 打包、多頁、變更紀錄與合併、`TooLarge`、golden | TX-6、判定 4 的本機部分 |
| 第 7 步 | 情境測試（8.5）→ TX-1 含入掃描與自我測試（8.6）；多執行緒測試；每幀成本量測（判定 7） | 判定 7；守衛綠 |
| 第 8 步 | 突變清單交 Opus 檢查 → 補 → 完整跑；設計審查（批次表〈每批的設計審查〉，Opus／high）；全量 ctest；文件（`text.zh-Hant.md`、v2 README、`cross-component-rules.md` 的登記、dev_log）；合併前核准頁；L1 三平台 | 判定 1～8；交使用者合併前核准 |

**和其他子批共用的檔**：`config/modules.csv`、`third_party/CMakeLists.txt`（I 也改）、`tests/common/CMakeLists.txt`、`docs/architecture/target/v2/README.md`。各只加幾行；後合併的一方 fetch＋merge `gyo2` 跟上，不 rebase。

## 11. 卡片照 L17 判的（不是使用者的話；可以推翻）

| # | 判了什麼 | 根據 |
|---|---|---|
| L-1 | 位置與名字（`engine/modules/text`、`GYO::Engine::Text`、`gyo_text`／`GYO::Text`、`TextContext`、`GlyphAtlas`、`FontData`、`FontLoader`、登記列） | N11～N17、N25；Bevy 的 `FontCx`／`FontAtlasSet`／`Font` |
| L-2 | 字型的 loader 放 `text`、`text` 依賴 `asset`；loader 收集合檔；loader 用兩個 adapter 都驗；settings 收下忽略 | 總宣告「T 定」；audio `WavLoader`（`WavLoader.hpp:76`）；日文字型常是集合檔 |
| L-3 | 介面收多個 span（rich text）、顏色不在 `text`、字形帶 span 序號 | Bevy 的 `TextSpan`／`section_index`；免得 `ui` 之後改簽名 |
| L-4 | Shape 與斷行分開 | Bevy 的 measure（偵察第 8 點） |
| L-5 | 每個字形帶原文範圍（偏離 Bevy，膠水：中） | 第 4 節；GYO 沒有 parley |
| L-6 | 度量只來自 HarfBuzz、FreeType 用 light hinting、不用 hb-ft | 總宣告 Q6 的預設；TX-2 |
| L-7 | 不合法 UTF-8 換 U+FFFD、缺字畫 `.notdef`、太大的字形回狀態，都不是錯誤 | Unicode 建議做法；Object_Connect；「告訴外面發生了什麼」 |
| L-8 | 圖集 Alpha8、row 0 在上、shelf、padding 1、預設 1024²、不回收、變更紀錄（偏離 Bevy 的 RGBA8 `Image`，膠水：少） | Object_Connect；DI-KS 13 |
| L-9 | 點陣圖原點對整數像素、不做 subpixel（位置部分偏離 Bevy，膠水：少） | 第 4 節；TX-8 |
| L-10 | 只做 LTR、不做雙向與直排、不做系統字型、不做可變字型與彩色字形 | 沒有點名的 Feature；介面不堵死 |
| L-11 | 換行的預設解析（`CJ`→`NS`＝嚴格的禁則）；沒有換行機會時溢出、不硬切 | UAX #14 的預設；CSS `overflow-wrap: normal` |
| L-12 | 行高 `RelativeToFont(1.2)` 預設、half-leading | Bevy `LineHeight`；CSS |
| L-13 | 連字內的游標照字元數平分 | Chrome、parley 的做法〔知〕 |
| L-14 | 測試字型：fontTools 生成的合成字型＋BIZ UDPGothic 子集，放進 repo | AGENTS §14.2（期望值不靠 Agent 的理解）；Object_Connect 的實際字型 |
| L-15 | （判定 6、7 的門檻改交使用者，第 13 節第 3 題） | — |
| L-16 | context 的身分用堆上的身分物件、圖集綁定一個 context；`ShapedText` 複製樣式 | 第 5.3 節；修訂 0 檢查 S1、S2 |
| L-17 | 游標帶 affinity、只停在字素邊界、default-ignorable 不參與 fallback 的覆蓋檢查 | parley 的 `Cursor`；HarfBuzz 隱藏 default-ignorable |
| L-18 | 外部組件用官方 release tarball＋SHA-256、關掉兩個函式庫讀環境變數的路、`FT_LOAD_NO_BITMAP`、排版用 26.6 定點 | TX-7、TX-8；修訂 0 檢查 S5～S7、S11 |

## 12. 外部組件與資料的下載（要使用者同意才下載）

FreeType、HarfBuzz（與選 (丁) 時的 libunibreak）在 CMake 設定時下載官方的 release tarball（`FetchContent` 的 `URL`＋`URL_HASH SHA256`；雜湊在第 1 步算出、和上游公布的簽章或雜湊對照後寫死）；本機每個新建置目錄第一次 configure 一次，L1 每次跑一次。Unicode 的資料與 fontTools 只在開發時下載一次（本機），產物放進 repo，L1 不下載。

| 組件 | 來源（官方） | 版本 | 授權 | 用的部分 |
|---|---|---|---|---|
| FreeType | 官方的 release tarball（`download.savannah.gnu.org/releases/freetype/freetype-2.15.0.tar.xz`〔知；第 1 步確認官方的發布位置〕；git 的官方鏡像 `github.com/freetype/freetype` 對應 commit `37515cf56ba3349656049edbed89ffc6f6466f2d`） | **2.15.0**（2026-10-11，最新） | FTL 或 GPLv2 二選一；**GYO 選 FTL**（產品的文件要寫 credit，原文含年份欄位：「Portions of this software are copyright © ⟨year⟩ The FreeType Project (www.freetype.org). All rights reserved.」〔第 1 步照 `docs/FTL.TXT` 原文查證〕） | `src/base` 的必要檔＋sfnt、truetype、cff、psaux、pshinter、psnames、smooth、autofit；不用 zlib、bzip2、PNG、Brotli、HarfBuzz |
| HarfBuzz | 官方 GitHub release 的 `harfbuzz-15.2.0.tar.xz`（commit `ed51d202cfd0c29372d3ee056eb0dc715688a410`） | **15.2.0**（2026-11-19，最新；發布才 4 天——第 1 步若發現回歸，退到 15.1.0〔2026-11-04〕並回報） | "Old MIT" | 只編 `src/harfbuzz.cc`；不定義 `HAVE_*`（沒有 hb-ft、CoreText、DirectWrite、ICU、glib） |
| libunibreak（**只在第 13 節第 1 題選 (丁) 時**） | 官方 GitHub release（`adah1972/libunibreak`） | **8.0.1**（2026-11-16，最新） | zlib | 換行（UAX #14）與字素（UAX #29）；在 `src/adapters/libunibreak/` |
| Unicode 資料 | `https://www.unicode.org/Public/<版本>/ucd/`：`LineBreak.txt`、`EastAsianWidth.txt`、`emoji/emoji-data.txt`、`extracted/DerivedGeneralCategory.txt`、`auxiliary/GraphemeBreakProperty.txt`、`DerivedCoreProperties.txt`、`auxiliary/LineBreakTest.txt`、`auxiliary/GraphemeBreakTest.txt`（選 (丁) 時只要 `DerivedCoreProperties.txt` 與兩個測試檔） | 第 1 步時的最新版（多半是 **18.0.0**〔知〕；選 (丁) 時和 libunibreak 支援的版本對齊） | Unicode License v3 | 開發時下載一次；生成的表與轉換過的測試向量放進 repo（附授權告示），原始檔放主 checkout `build/evidence/gyo2-sdl-text/ucd/` |
| fontTools | PyPI `fonttools`（官方） | 第 1 步時的最新穩定版（寫進 `fonts/README.md`） | MIT | 開發時裝進 `build/` 底下的 venv（不放 home），只用來生成測試字型與做子集；L1 不需要 |
| BIZ UDPGothic | 本機 `~/Code/GitHome/Object_Connect/NoviceResources/fonts/BIZUDPGothic-Regular.ttf`（不下載） | 1.051（Object_Connect 記的 SHA-256 `22f41ce6…`） | SIL OFL 1.1（沒有 Reserved Font Name） | 子集放進 `tests/common/text/fonts/`，附 OFL 全文；只給測試 |

查詢方式：`gh api repos/<repo>/releases`、`tags`、`git/ref/tags`、`git/tags`（2026-11-23，只讀 metadata，沒有下載；`upstream-metadata.md`）。

**授權告示的義務**（寫進 `text.zh-Hant.md`，打包在 K 做）：連到 `text` 的產品附 FreeType（FTL 的 credit）、HarfBuzz（Old MIT）、Unicode 資料（Unicode License v3）、（選 (丁) 時）libunibreak（zlib）的告示；產品自己帶的字型檔附它們自己的授權（例：BIZ UDPGothic 的 OFL）。

## 13. 交使用者的

### 第 1 題：換行與字素邊界怎麼做

**例子**：框寬 300 px 的說明文字「操作方法：ゲームパッドの「A」ボタンで決定します。Press Start to begin.」要自動換行：「。」「」」不能在行首、「「」不能在行尾、英文單字不從中間切。文字框裡按右方向鍵，游標要一次跳過「が」（か＋濁點兩個字元時也一樣）與一個 emoji 組合（👨‍👩‍👧 是 5 個字元），不能停在中間。前者是 UAX #14（換行），後者是 UAX #29（字素）。

**背景**：Q6 定了「換行、fallback 字體、字形快取在 `text` 的 `src/`」（理由裡「禁則、fallback、快取不論選哪個都要自己做」，講的是 FreeType、HarfBuzz、SDL_ttf 都不做這些）；總宣告已核准的 T 驗收骨幹有「日文換行」。HarfBuzz 不給換行、也不給公開的字素切分（修訂 0 檢查 S3）。目前點名的 Feature 裡**沒有人需要自動換行**（Object_Connect 只有硬換行、master 的 `gyo.ui` 文字框是單行，偵察第 2、5 節），但**字素邊界是 U 的文字框一定要的**（游標、刪除）。Bevy 兩件都用成熟的函式庫（parley 內的 ICU4X）。

- **(丁) 用 libunibreak 當第三個外部組件 ←建議**：成熟的 C 函式庫（zlib 授權、2026-11-16 還有發布、8.0.1），同時做 UAX #14 與 UAX #29（還有字詞邊界，之後 U 的「依字詞移動」用得到），有語言別的 tailoring〔知〕；放 `src/adapters/libunibreak/`，照 AGENTS §6 一個 adapter。官方的 `LineBreakTest`、`GraphemeBreakTest` 照樣跑（期望值不靠我們）。好處：自己寫的只有 adapter（約 200 行）；和 Bevy「規則交給成熟實作」同一個做法；Unicode 升版只換版本。代價：多一個外部組件（下載、授權告示、每個組態都建——它是小的 C 程式，建置時間可忽略〔知〕）；換行在 `text` 的 `src/`，但不是自己寫的，這一點是 Q6 理由那句話的另一種讀法，所以交你；Unicode 版本跟著 libunibreak，可能比 HarfBuzz 的晚一版。估時：第 5 步 3～5 h。
- **(甲) 自己實作 UAX #14 與 UAX #29，表由 Unicode 資料生成**：約 30 條換行規則、40 多個類別，加字素的十幾條規則；兩個官方測試全部通過。好處：沒有第三個外部組件；Q6 那句話照字面。代價：規則與生成腳本約 1,200～1,600 行要我們維護，Unicode 每次升版都要重生、可能補規則（RC-TX4）；第 5 步 9～13 h。
- **(乙) 簡化規則**：換行＝空白後可斷、CJK 之間可斷＋日文禁則表；字素＝基底字＋結合字元。好處：4～5 h。代價：期望值只能靠我們自己寫的測試；數字與單位、引號、emoji 組合、韓文、泰文都不對；之後換成 (丁) 或 (甲) 時，所有排好的畫面斷行位置會變。
- **(丙) 這一批只做硬換行與簡化的字素，自動換行延到 U**：好處：T 最小。代價：改了已核准的 T 驗收骨幹（「日文換行」）；U 再做一次這一題。
- **改變的決定**：TX-1（多一個 adapter）、TX-4、第 5 步的估時、第 12 節的下載。
- **證據**：`bevy-text-survey.md` 第 3 點；`master-text-object-connect-survey.md` 第 2、5 節；`declaration-r0-adversarial-check.md` S3、S12；libunibreak 的發布紀錄（`gh api repos/adah1972/libunibreak/releases`，2026-11-23）。

### 第 2 題：同意第 12 節的下載

FreeType 2.15.0、HarfBuzz 15.2.0（官方 release tarball，CMake 設定時下載，每個組態、L1 每次）；選 (丁) 時 libunibreak 8.0.1（同）；Unicode 的資料檔（開發時一次）；fontTools（開發時一次，裝在 `build/` 底下）。BIZ UDPGothic 用本機 Object_Connect 的那份做子集，不下載。

### 第 3 題：判定 6、7 的門檻（AGENTS §14.1：事前宣告的判定交使用者核准）

- 判定 6（建置時間的停止條件）：本機 `.ninja_log` 兩個函式庫合計超過 5 分鐘、或 `harfbuzz.cc` 單檔超過 3 分鐘就停下回報。出處：沒有計畫裡的預算；卡片推的——每個組態（含 Match）都要付，比整個 gyo2 現在的乾淨建置（第 1 步量）明顯多就該重看建置圖。
- 判定 7（每幀成本的停止條件）：約 12 字的日文 `Shape`＋`Layout` 中位數超過 50 µs 就停下回報。出處：Object_Connect 每幀約 30 段（偵察第 5 節）→ 1.5 ms＝60 fps 一幀的 9%；計畫裡沒有文字的預算，R2b 定正式預算時跟著改。
- 兩個都是停止條件，不是受理的閘門；結果出來之後不放寬。

## 14. 範囲の限界

- 「換函式庫」要到第二個實作出現才算驗證過；本批只以 TX-1 的守衛與審查支撐。
- 雙向文字、直排、彩色字形、可變字型、系統字型都沒有做（2.2 節）；RTL 的文字會照 LTR 排（錯的）。
- 字型被當成作者的內容、不是任意的外部輸入；惡意字型的防護靠 FreeType、HarfBuzz 自己（〔知〕兩者都在 OSS-Fuzz）。
- ThreadSanitizer 不在 L1；TX-7 的多執行緒測試只證明「沒當掉、結果對」，不證明沒有資料競爭。
- 效能只在本機量一次，是停止條件，不是受理的閘門；實際的幀時間預算由 U、R2b 宣告。
- 圖集不回收：長時間跑大量不同字形（例：聊天室的任意文字）記憶體會一直長（RC-TX2）。
- 真的上傳到 GPU、畫在畫面上的樣子不在本批（R2a、R2b）；本批的 golden 是 CPU 上的圖集位元組（總宣告第 10.2 節：T 的驗收不需要畫）。
- 日文換行的品質只到 UAX #14 的預設（第 13 節第 1 題）；JIS X 4051 的完整日文排版（文字間距的調整、ぶら下げ）不做。

## 15. 觀察到、不在本批的（報給主會話）

- **I 與 T 同改兩個檔**：`config/modules.csv`、`third_party/CMakeLists.txt`；後合併的一方 merge 跟上（總宣告第 10.1 節）。
- **授權告示**：master 從來沒有告示機制（偵察第 4 節，`docs/architecture/plans/third-party-evaluation/evaluation.zh-Hant.md:353,400` 的 D9 未解決）；`gyo2` 現在也沒有。K 的 tools 線打包要一起做，yyjson、fast_float 的 MIT 告示也還沒收（JSON 宣告第 15 節）。
- **要登記的跨部件規則**（AGENTS §1；owner＝`text`，合併時寫進 `docs/cross-component-rules.md`，`CCR-9` 已涵蓋 `FontData` 的不可變與快取的歸屬）：使用 `text` 的一方都要照做的四件——(a) 畫字形時左上角＝`round(glyph.x) + left`、`round(baseline) - top`；(b) 圖集頁是 coverage，顏色由畫的人乘（shader 是「coverage × 顏色」）；(c) 渲染側自己記上傳到的版號、用 `ChangesSince` 拿變更，不清除圖集的紀錄；(d) 一個 `GlyphAtlas` 只配一個 `TextContext`。第一批使用者是 R2a、R2b、U。
- **每個組態都建 FreeType、HarfBuzz**：`third_party/` 無條件加入（總宣告第 9.1 節末）；判定 6 若觸發，可能要重看「Module 全部都建」或讓 `third_party` 的組件跟著有人用才建——那是建置圖的 Architecture Delta，交主會話。

## 16. 停止條件

- 第 1 步的查證任一項不過（版本、授權、三平台編譯、FreeType 選項關不掉、HarfBuzz 不帶 `HAVE_*` 編不過、Unicode 資料版本與測試不一致）→ 停、回報。
- 判定 3、4 不成立；判定 6、7 的停止條件觸發 → 停、回報（RC-TX5 等）。
- 原因不明的失敗、同一問題換兩次方法仍無結論、超過時間（第 0 節）→ AGENTS §14 冒頭的報告形式停下。
- 要碰 Kernel、編排層、`asset` 本身、SDL 的檔 → 停（T 不碰 SDL，總宣告第 10.1 節）。
- 要用 hb-ft、或 FreeType、HarfBuzz（選 (丁) 時加 libunibreak）要出現在各自的 adapter 以外的地方 → 停，交使用者（AGENTS §6 的例外）。
- 兩個函式庫讀環境變數的路關不掉（第 5.1 節）→ 停、回報（TX-7、TX-8 守不住）。

## 17. 修訂 0 的敵對式檢查：處理對照

檢查者 Opus／high（作者同模型），`declaration-r0-adversarial-check.md`。全部採用。

| # | 指摘 | 處理 |
|---|---|---|
| B1 | TX-2 的測試要讀 FreeType，和 TX-1「測試只拿到 `include/`」矛盾；16.16 對 26.6 不精確 | TX-1 准許 `tests/common/text/adapters/<組件>/`；TX-2 改在 font units 比（`FT_LOAD_NO_SCALE` 對 scale＝upem） |
| S1 | `FontId` 是 context 內的，圖集的快取鍵會撞；context 不能用位址認 | 第 5.3 節：堆上的身分物件＋`weak_ptr`；圖集綁定一個 context；第 5.7 節快取鍵；L-16 |
| S2 | `TextStyle::fonts` 是 span，`ShapedText` 會懸空 | 第 5.3 節：`Shape` 複製樣式 |
| S3 | 字素邊界沒有資料來源；default-ignorable | 第 5.4 節第 1 點、第 5.6 節；資料來源併進第 13 節第 1 題；TX-4 加 `GraphemeBreakTest` |
| S4 | 游標沒有 affinity | 第 5.6 節 `Caret`／`CaretAffinity`；TX-3；情境測試；L-17 |
| S5 | 1/64 的精確等式不成立 | 第 5.4 節第 4 點（內部 26.6 定點）；合成字型 upem 1024；期望值照 HarfBuzz 的定點算法推 |
| S6 | 沒寫 `FT_LOAD_NO_BITMAP` | 第 5.7 節 |
| S7 | `FREETYPE_PROPERTIES`、`HB_OPTIONS`；light hinting 會捨入前進量 | 第 5.1 節（關掉、明定屬性、第 1 步查 `HB_OPTIONS`）；第 5.4 節第 3 點改寫；TX-2、TX-8；停止條件 |
| S8 | 掃描根沒有 `plugins/`、沒查連結 | TX-1 的守衛：整個 repo、加連結規則 |
| S9 | 引錯 `WavLoader` 的 settings 先例 | 第 5.2 節第 3 條改成「收下、忽略」 |
| S10 | 沒查 CCR | 根據加 CCR-9；第 15 節列要登記的四件；T9、第 8 步 |
| S11 | `GIT_TAG`＝commit 不能淺層 clone；`/bigobj`；判定 6 用 CI 時間雜訊大 | 改 release tarball＋`URL_HASH`；第 1 步查 `/bigobj`；判定 6 改讀 `.ninja_log` |
| S12 | 第 1 題少了現成函式庫 | 加 (丁) libunibreak 並建議；(甲) 改成含 UAX #29 |
| S13 | 判定 7 的門檻沒有出處 | 判定 7 寫出處；判定 6、7 的門檻改成第 13 節第 3 題 |
| nit | asset 型別 `"text"` 同詞異義 | 第 5.1 節 |
| nit | Asset→Json 是 PRIVATE | 第 6 節第 5 項 |
| nit | `FontLoader` 沒有 `public:`、`AssetTypeName` 特化 | 第 5.2 節的程式片段 |
| nit | 錯誤碼的前綴 | 第 5.2 節第 4 條（TX-E） |
| nit | Unicode 版本、FTL 年份、HarfBuzz 才 4 天 | 第 12 節 |
| nit | `LineBreakTest` 檔頭的 tailoring | 第 8.3 節、判定 3 |
| nit | F3 計數的對象 | 第 8.5 節 |
| nit | 設計審查也同模型 | 頁首「經過」、第 10 節 |
| nit | `x` 是否已疊加、沒有 `maxWidth` 的置中基準 | 第 5.4 節的程式片段、第 5.5 節 |

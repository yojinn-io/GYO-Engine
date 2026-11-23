# SDL 線子批 T：text Module（`v8-arch-gyo2-sdl-t`）

日期：2026-11-23。分支 `claude/gyo2-sdl-t-text`（從 `gyo2` 545b5a67），draft PR #297。**狀態：實作完成，等合併前核准**（[合併前核准頁](../architecture/target/v2/text-merge-approval.zh-Hant.md)）。

這一批由任務卡做（GYO2 主會話開出，SDL 總宣告 `sdl-declaration.zh-Hant.md` 修訂 21 第 10 節的子批 T）。事前宣告：[text-declaration.zh-Hant.md](../architecture/target/v2/text-declaration.zh-Hant.md) 修訂 1；使用者 2026-11-23 在卡片「照建議，1 選丁，2 同意下載，3 門檻照建議」（動工前核准）。修訂 0 送 Opus／high 敵對式檢查（blocker 1、should 13、nit 9，作者同模型），修訂 1 全部採用。Design Invariants：[text.zh-Hant.md](../architecture/target/v2/text.zh-Hant.md)。

## 做了什麼

| 步 | 提交 | 內容 |
|---|---|---|
| 設計段 | 8bc5c17d、7ff583ab、af779f23 | 事前宣告修訂 0、修訂 1、動工前核准的紀錄 |
| 第 1～6 步 | 6e323779 | `third_party/freetype`（2.15.0）、`harfbuzz`（15.2.0）、`libunibreak`（8.0.1）：官方 release tarball＋`URL_HASH SHA256`、GYO 自建 target、FreeType 用自己的模組表與選項標頭（關掉環境變數、zlib 等）、HarfBuzz 以 `HB_NO_GETENV` 建單檔彙整；`engine/modules/text`（三個 adapter、`FontData`／`FontLoader`、`TextContext`、`ShapedText`、`TextLayout`、`Caret`、`GlyphAtlas`、`Default_Ignorable` 的生成表）；測試字型（fontTools 生成的合成字型三個、BIZ UDPGothic 子集）；`gyo_text_tests`、`gyo_text_adapter_tests` |
| 第 7 步 | 451824c3、0cd5a435 | golden（TX-8）、情境（HUD 1,000 幀、量寬縮字級、文字框、說明文字換行）、多執行緒、每幀成本的量測程式；TX-1 的含入與連結掃描與自我測試；環境變數的測試；突變清單；`Utf8.cpp` 的 U+FFFD 改成 ASCII 跳脫（MSVC C2015） |
| 第 8 步 | （本提交） | 突變的修正與完整執行、`IsCollection` 的長度檢查、環境變數改由 CTest 給、文件（`text.zh-Hant.md`、CCR-12～14、v2 README、本頁、合併前核准頁） |

和宣告不同的地方（照實記）：

- **測試先行沒有照第 8.1 節逐步做**：第 1～6 步先寫了實作再寫測試，沒有逐步記下 Red。檢出力改由突變證明（第 8 步，全部 killed）。
- **TX-1 收緊**：宣告准許 `tests/common/text/adapters/<組件>/` 含入外部組件；實際上測試只用 adapter 自己的介面，守衛不留這個例外（締める面）。
- **錯誤碼一個 enum**：宣告寫 `FontLoaderCode` 與 `TextFailureCode` 兩個；loader 的 `Code` 直接用 `TextFailureCode`（同一份資料契約的規則，分兩個沒有內容）。
- **`PositionedGlyph` 多了 `penX`、`sizePx`、`visible`，`LayoutLine` 多了 `hardBreak`**：游標要 pen 位置（不含 shaping 偏移）、圖集的快取鍵要字級、不畫的字形要標出來、affinity 要知道行是不是硬換行結束。
- **Unicode 版本 17.0**：宣告寫「第 1 步時的最新版（多半 18.0）」；選 (丁) 時和 libunibreak 對齊（8.0.1 支援 17.0），第 12 節已寫這個條件。
- **HarfBuzz 把缺的空白換成 invisible 字形**：只有假名字型時換行字元（shaping 時是 U+0020）也是 invisible、前進量為字級的 1/4（HarfBuzz 的 space fallback）。

## 驗證與經過

**件數の突き合わせ**：一致——`LineBreakTest.txt` 19,338 件＝常數＝比對數（Python 從原檔數過）、`GraphemeBreakTest.txt` 766 件；情境 1,000 幀×30 段；突變 killed／總數見下。

- **下載與查證（第 1 步）**：HarfBuzz tarball 的 SHA-256 等於 GitHub 公布的 digest；FreeType tarball 和官方 git 鏡像 commit `37515cf5` 的樹逐檔比對 blob 雜湊（764 檔一致、0 不一致；tarball 多的 `src/dlg` 是上游的子模組，不編）——本機沒有 gpg，沒驗簽章；libunibreak 8.0.1 附的 `LineBreakTest.txt`、`GraphemeBreakTest.txt` 和 unicode.org 17.0.0 的逐位元相同；libunibreak 的官方測試用 `"-strict"`（`CJ`→`NS`，即 UAX #14 的預設解析），adapter 也固定用它；HarfBuzz 有 `HB_NO_GETENV`；FreeType 的 `FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES` 預設開著，選項標頭關掉；FTL 的 credit 原文確認（網址 `https://freetype.org`）。
- **判定 6（建置時間，停止條件）**：本機乾淨建置的 `.ninja_log`：`harfbuzz.cc` 172.6 s（門檻 180 s）、FreeType 15 物件 35.4 s、合計 208.0 s（門檻 300 s）——**沒觸發，但 HarfBuzz 離門檻只差 4%**（16 個工作並行時量的）。libunibreak 3.0 s。加入前 gyo2 的乾淨建置 448 s（基準）。
- **判定 7（每幀成本，停止條件）**：BIZ UDPGothic 子集、24 px、約 12 字的 `Shape`＋`Layout` 中位數 8.94～9.11 µs（三次，各 19,000 樣本；門檻 50 µs）。
- **判定 3**：兩個 Unicode 測試全部通過、不跳過。
- **判定 4（跨平台）**：golden 在本機（macOS x86_64）算出；L1 三平台見合併前核准頁。
- **L1**：第一次手動 dispatch（run 45129587421）windows-x64 編譯失敗：`Utf8.cpp` 的 `U'…'` 裡寫成了實際的 U+FFFD 字元（MSVC 沒有 `/utf-8` 讀成多個字元，C2015），改成 `U'�'`（0cd5a435）；第二次（run 45131303066）macos-arm64 與 linux 的 `os.boundary_guard` 失敗：`EnvironmentTests.cpp` 用了 `#if defined(_WIN32)`（R2：平台分支只准在 `core/os`），改成由 CTest 的 `ENVIRONMENT` 給變數。第一次是第 1 步的查證、不是受理判定；兩次都照 §14.1 記在這裡。
- **本機全量 ctest**：`build.ci` 失敗——測試在建置目錄裡跑 `git rev-parse HEAD`，而本機的建置目錄放在 repo 外的暫存區（環境，不是實作；L1 與 repo 內的建置目錄不會遇到）。其餘全過。
- **突變（第 1 次完整執行 44/57）**：13 個沒殺，分類——清單寫錯 8 個（`cases` 名稱有逗號，doctest 的 `-tc=` 把逗號當分隔、一個 case 都沒跑；改用 `*`）；`expect` 對錯行 1 個（HarfBuzz 讀環境變數：連字消失，先在 `REQUIRE(size == 3)` 失敗）；測試的缺口 1 個（UTF-8 的第二位元組範圍：`ED A0 80` 照樣被上限擋，補 `E0 80 80`）；突變不改變值 1 個（FreeType 不帶 `FT_LOAD_NO_SCALE` 而沒設字級時回同樣的值：換成「HarfBuzz 以 1000 為 scale」）；等價 2 個（畫不可見字形：不可見字形一定是 HarfBuzz 的 invisible id 或空白，畫出來都是 Empty；FreeType 讀環境變數：會影響這條路的屬性都由 `FT_Property_Set` 重設，其他屬性〔含 `increase-x-height=40`〕沒改變任何像素）——照 README 不留，理由寫在清單的 description。另外 `tx10-sfnt-unchecked` 讓 `FromBytes` 崩潰：`IsCollection` 沒先檢查長度（平常被 `IsSfnt` 擋在前面），補了長度檢查。
- **突變（修正後）**：55／55 → 敵對式檢查（Opus／high，作者同模型；`mutation-check.md`）找到 21 處沒被抓的改法：16 處補測試與突變、2 處只補測試、3 處測不到或低優先（理由在清單 description）；檢查者「畫不可見字形不等價」的反例推論不成立（本機探針：缺空白時 HarfBuzz 給的是 invisible id）；清單問題（`expect` 不專一 3、id 與 guards 不一致、環境變數兩處維護）已修 → 79 個，完整執行 78／79，`tx4-width-after-break` 補假名斷行測試後 79／79。
- **設計審查**（Opus／high，作者同模型；`design-review.md`）：沒有 blocker；第 1、2 條交使用者（第 A、B 題），其餘本批改了文件或記為後續。
- **使用者定第 A、B 題**（「第 A、B 兩題，照建議」）：`ShapeOptions`（`language`、`strictness`）已加、預設不變；TX-1 守衛縮回 Engine 側、`tg-plugins-not-scanned` 退役。完整重跑突變 79／79（`mutations-run7.log`）。
- **跟上 `gyo2`**：merge 兩次（`AGENTS.md` 核心版 #296、KR #295），沒有衝突；本機全量 ctest 只有 `build.ci`（環境）失敗。

## 下一步

- 合併前核准（使用者）。
- U、R2a、R2b 照 CCR-12～14 用 `text`；R2a 的 CPU 貼圖局部更新吃 `ChangesSince`。
- 授權告示收進打包（K）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-sdl-text/`（本機 Intel Mac，git 不追蹤）：`bevy-text-survey.md`、`master-text-object-connect-survey.md`、`upstream-metadata.md`、`declaration-r0-adversarial-check.md`、`downloads/`（三個 tarball 與 FreeType 的 `.sig`）、`ucd/`（Unicode 17.0.0 的三個檔）、`venv/`（fontTools 4.68.0）、`build-time/`（基準與加入後的 `.ninja_log`、`measurements.md`）、`mutations-run*.log`。

# 合併前核准：text Module（`v8-arch-gyo2-sdl-t`）

件數の突き合わせ：一致（`LineBreakTest.txt` 19,338 件＝常數＝比對數、`GraphemeBreakTest.txt` 766 件＝常數＝比對數〔件數由 Python 從原檔數過〕；情境 1,000 幀×30 段；突變 79 killed／79）。

日期：2026-11-23。分支 `claude/gyo2-sdl-t-text`（draft PR #297）。事前宣告：[text-declaration.zh-Hant.md](text-declaration.zh-Hant.md) 修訂 1（2026-11-23 動工前核准）。經過與證據：[dev_log](../../../dev_logs/2026_11_23_gyo2_sdl_t_text.zh-Hant.md)。Design Invariants：[text.zh-Hant.md](text.zh-Hant.md)。

這張卡照開卡時的舊 AGENTS 做完（使用者「照你建議。」，HANDOFF `v8-arch-gyo2-core-agents-transition`）。

**請你核准的**：第 3 節 Design Invariants 的最終文字、第 4 節突變的數目超過基準、第 5 節和宣告不同的地方。第 6 節兩題已定（下表），已照決定改。你說「合併」才合併。

**核准紀錄（2026-11-23，使用者在卡片）**：

| 件 | 原話 | 結果 |
|---|---|---|
| 第 6 節 第 A 題 換行的語言與禁則強度 | 「第 A、B 兩題，照建議」 | (a)：`ShapeOptions`（`language`、`strictness`）已加，預設不變；測試與突變 `tx4-strictness-ignored`、`tx4-language-not-passed`。已照四樣轉主會話 |
| 第 6 節 第 B 題 TX-1 守衛的範圍 | 同上 | (a)：縮回 `engine/`＋`tests/`（與 `build/cmake/`、根的 `CMakeLists.txt`）；突變 `tg-plugins-not-scanned` 與它的自我測試退役。§14.4 的 1～5 在第 6 節。已照四樣轉主會話 |

## 1. 判定的結果（宣告第 9 節）

| 判定 | 結果 |
|---|---|
| 1 通過 | 本機：text 相關 ctest 全過（`gyo_text_tests` 64 case、`gyo_text_adapter_tests`、`gyo_text_environment`、`text.adapter_guard`、`text.adapter_guard_selftest`）；全量 ctest 見第 7 節（`build.ci` 的環境因素）。L1 三平台：run 45136146149（ff52d410）linux-x64／windows-x64／macos-arm64 的 `L1 /` job 全部 success；第 A、B 題與兩次 merge `gyo2`（含 #296、#295）之後：run 45145909517（c92c9d2f）三平台的 `L1 /` job、Quick acceptance、CI gate 全部 success |
| 2 精確值 | 量字、原文範圍、偏移、行高的精確等式全部成立（三平台） |
| 3 換行與字素的一致性 | `LineBreakTest.txt` 19,338 件、`GraphemeBreakTest.txt` 766 件全部通過、不跳過；兩個檔的 SHA-256 等於 unicode.org 17.0.0 的 |
| 4 跨平台 | golden（BIZ UDPGothic 12／17／24／45 px、合成 TrueType 與 CFF 字型）在 L1 三平台 0 不一致（run 45136146149；測試字型改過之後 run 45145909517 再確認） |
| 5 檢出力 | 突變 79／79 killed（預期的那個失敗；第 A、B 題之後完整重跑，`mutations-run7.log`）；清單交 Opus／high 敵對式檢查並補過（第 4 節） |
| 6 建置時間（停止條件） | `harfbuzz.cc` 172.6 s（門檻 180 s）、FreeType＋HarfBuzz 208.0 s（門檻 300 s）：沒觸發，但 HarfBuzz 只差 4%；已報主會話，記在批次表後續小事（`e4d62c50`） |
| 7 每幀成本（停止條件） | 中位數 8.94～9.11 µs（門檻 50 µs） |
| 8 件數 | 見頁首 |

## 2. 判定的變更（AGENTS §14.4）

本批的測試都是本批新寫的，沒有改既有的判定。本批內有兩處期望值在寫好之後改過，記在這裡：

- **Latin golden 的排版 digest**（`GoldenTests.cpp`）：測試字型加了「連字號＋V」的位置調整（為了驗 shaping 偏移與斷點兩側的重新 shaping），輸入變了，排版 digest 跟著變（`f1b2cb07…` → `bd284d6e…`）；點陣 digest 不變。判定（逐位元相等）沒有放寬。L1 三平台在 run 45145909517 驗過。
- **環境變數測試的圖集 digest**：測試多點陣化一個 CFF 字型，期望值改成沒有環境變數時的 digest（`5e0b19f6…`，在沒有 FreeType 變數的情形下算出）。判定沒有放寬。

## 3. Design Invariants 的最終文字

[text.zh-Hant.md](text.zh-Hant.md) 的 TX-1～TX-10 與 RC-TX1～RC-TX7。和宣告第 7 節的差（都只緊不緩，或補寫）：

- TX-1：測試不留「adapter 自己的測試目錄可以含入外部組件」的例外（測試只用 adapter 的介面）；守衛加名字規則、連結規則的正例（fail closed）。**範圍是第 6 節第 B 題**。
- TX-2：明寫「不用 FreeType 回報的前進量（light hinting 會捨成整數）」。
- TX-3：加 affinity、游標只停在字素邊界。
- TX-4：硬換行的字元與 hang 的字元集合寫明是 `text` 的固定規則；文末硬換行之後留一個空行。
- TX-6：不畫的字形回 `Empty`；`FT_LOAD_NO_BITMAP`。
- TX-7、TX-8：環境變數不改行為（`HB_NO_GETENV`、關掉 FreeType 的環境變數選項）；HarfBuzz 的語言明定。
- RC-TX2 加「HarfBuzz 已設字級字型快取沒有上限」；新增 RC-TX7（tab stop）。
- 使用的一方要照做的四件登記為 CCR-12～14（`docs/cross-component-rules.md`）；CCR-12 補「用乘過顯示縮放的字級排版，座標是最終像素」（設計審查第 4 條）。

## 4. 突變的數目（基準 8：超過「變的條文數 × 5＝50」）

79 個（依 id 前綴）：守衛 `tg-` 15、TX-2 4、TX-3 12、TX-4 19、TX-5 9、TX-6 10、TX-7 1、TX-8 1、TX-9 1、TX-10 7。超過的理由：

- 守衛（15）照 README 基準 4「每條規則三件」：四條含入規則、三條名字規則、連結規則各一個「全關」，真實樹上的正例三個（三個 adapter）、連結規則的正例一個，fail closed 一個，列表項目（`build/cmake/`）一個，必要根一個（第 B 題之後 `plugins/` 那個退役）。
- 換行（TX-4，19，含第 A 題的語言與強度兩個）是本批最大的演算法：硬換行、寬度斷行、退回、溢出、行尾 hang、不安全斷開的兩側、對齊、行高與 half-leading、空行，每個獨立決定一個。
- 敵對式檢查（Opus／high，作者同模型；`build/evidence/gyo2-sdl-text/mutation-check.md`）找到 21 處「改掉後測試仍會通過」：16 處補了測試與突變；2 處只補測試（突變會寫出頁外〔未定義行為〕、或沒有單一改動的突變）；2 處這些字型測不到、審查看（HarfBuzz 的語言、`FT_LOAD_NO_BITMAP`）；1 處低優先不補（一個 cluster 多個字形且中間有字素邊界的字型）。
- 不留的等價突變 3 個，理由寫在清單的 description（畫不可見字形——本機探針證明是 HarfBuzz 的 invisible id，檢查者的反例推論不成立；FreeType 不帶 `FT_LOAD_NO_SCALE`；FreeType 讀環境變數）。
- `guards` 欄位全部有。

## 5. 和宣告不同的地方

- **測試先行沒有逐步做**（宣告第 8.1 節）：先寫實作再寫測試，沒有逐步記 Red；檢出力由突變（79／79）證明。
- **錯誤碼一個 enum**：loader 的 `Code` 直接用 `TextFailureCode`。
- **介面多了幾個欄位**：`PositionedGlyph` 的 `penX`、`sizePx`、`visible`，`LayoutLine` 的 `hardBreak`（設計審查第 7 條確認應保留）。
- **Unicode 17.0**：和 libunibreak 8.0.1 對齊（宣告第 12 節的條件）。
- **環境變數由 `run_with_environment.py` 給**：本來在測試裡設，用了平台分支，違反 os.md R2（L1 的 `os.boundary_guard` 抓到），改成腳本。
- **語言是整段一個，不是每個 span 一個**（宣告第 5.4 節第 2 點寫「語言由 span 帶」）：第 6 節第 A 題的建議與決定是段落層級的 `ShapeOptions`，因為換行機會由 libunibreak 對整段一次算出。

## 6. 設計審查（批次表〈每批的設計審查〉；Opus／high，作者同模型）與要你定的兩題

報告：`build/evidence/gyo2-sdl-text/design-review.md`。結論「需交使用者」：沒有 blocker；第 1、2 條要你定，其餘 should／nit 的處理在報告末行（第 3 條報主會話；第 4、5、7、9 條本批改了；第 6 條〔context 身分物件〕、第 7 條的程式精簡、第 8 條〔只回尺寸的量字〕不改，理由：身分物件是宣告修訂 1 依檢查 S1 加的、已核准；精簡與量字路徑等 U 量到再做〔RC-TX5〕）。

### 第 A 題：換行的語言與禁則強度，要不要現在就做成選項

- **(a) 現在加（建議）**：`Shape` 加段落層級的 `language`（交給 HarfBuzz 與 libunibreak）與禁則強度（嚴格／一般）；預設維持現在（不帶語言＋嚴格＝UAX #14 的預設），遊戲自己設 `ja`。理由：換行規則一變所有排好的畫面跟著變，要在 U 積上去之前把介面定下；宣告第 5.4 節本來就寫了。代價：約 2～3 h。
- **(b) 維持固定**，等 U 或有日文多行的 Feature 再加。

### 第 B 題：TX-1 守衛的範圍

你以前定過「守衛只守 Engine 這一側（Engine 側的含入與連結掃描、標頭衛生），產品側的棘輪與白名單全部刪」（`docs/architecture/plans/kernel/adapter-plugins-plan.zh-Hant.md:15`，JSON 的 J-1 照此）。本批的守衛多掃 `plugins/`、`games/`、`tools/`、`examples/`，等於推翻它，也會提前擋掉 K 可能要用的 imgui_freetype（Q6 留給 K 交你的 AGENTS §6 例外）。宣告雖已核准，但沒有點明這一點。

- **(a) 縮回 `engine/`＋`tests/`（建議）**：照舊決定，imgui_freetype 留給 K。這是 DI 守衛的放寬（§14.4）：(1) 守的性質：外部組件只在 adapter；(2) 緩的面：Plugin 與產品的原始碼不再掃；(3) 締的面：沒有；(4) Engine 側照舊由同一個守衛守；(5) 根據：上面的行號、`json-module-declaration.zh-Hant.md` 第 70、371 行。突變 `tg-plugins-not-scanned` 退役。
- **(b) 維持擴大的範圍**：明確推翻那條決定。

## 7. 範囲の限界

- 本機全量 ctest 的 `build.ci` 失敗：測試在建置目錄跑 `git rev-parse HEAD`，本機建置目錄在 repo 外的暫存區（環境；L1 沒有這個問題）。
- ThreadSanitizer 不在 L1；多執行緒測試只證明「沒當掉、結果對」。
- FreeType 的 tarball 沒驗簽章（本機沒有 gpg），改以和官方 git 鏡像的樹逐檔比對 blob 雜湊（764 檔一致）。
- 效能只在本機量；真正的幀時間預算由 U、R2b 宣告。
- 雙向文字、直排、彩色字形、可變字型、系統字型都沒做（text.zh-Hant.md 的範囲の限界）。
- 突變的敵對式檢查與設計審查都和作者同模型（Fable 暫停）。

## 8. 交主會話的

- HarfBuzz 單檔編譯時間（已轉，批次表後續小事 `e4d62c50`）。
- 授權告示收進打包（K）：FreeType 的 FTL credit、HarfBuzz、libunibreak、Unicode 資料、字型檔自己的授權。
- 第 6 節兩題的決定，照四樣轉主會話記 HANDOFF。

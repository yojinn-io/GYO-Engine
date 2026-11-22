# 2026-11-22 N13 短名（`v8-arch-gyo2-n13-short-names`）

Owner：架構（GYO2 主會話開的任務卡）。分支 `claude/gyo2-n13-short-names`（PR #271，開向 `gyo2`）。實作者 Opus 5.5／high；設計審查 Fable 5.2／high（唯讀）。

## 為什麼

`v8-arch-gyo2-naming-cleanup`（#269，65d6c131）把 `::GYO::Engine::` 改成短名，並在命名規範 N13 補了「引用別的單位」：Engine 的單位裡寫 `<單位>::`，Kernel 裡寫 `Engine::<單位>::`，巨集全限定。使用者 2026-11-22「核准，N13 選 (a)」（HANDOFF `v8-arch-gyo2-naming-cleanup-approved`，記在分支 `claude/gyo2-time-design` 的 `docs/architecture/target/v2/HANDOFF.md`；原話也在 `rebuild-naming.zh-Hant.md` 的「N13『引用別的單位』的來源」）——剩下的全名另開一批改完。這一批就是那一批：只改名字，不改行為。照「改名類工作排前面」，在交換點的實作（#270）之前做。

## 做了什麼

`engine/` 的 C/C++ 程式裡，不在註解、字串、前置處理行、巨集本體、namespace 宣告裡的 `GYO::Engine::<單位>::` 與 `(::)GYO::Kernel::`，照所在的單位改寫（27 個檔案、248 行、281 處）；另外刪掉兩行多餘的 namespace 別名（見下）：

| 所在 | 原本 | 改成 | 處數 |
|---|---|---|---|
| IO（`engine/core/io`）裡指自己 | `GYO::Engine::IO::` | 拿掉（`IOErrorCode`、`Path::Uri`、`Stream::IStream`、`FS::IFileSystem`） | 183 |
| Kernel 裡指 Base | `GYO::Engine::Base::` | `Engine::Base::` | 75 |
| IO 裡指 Base | `GYO::Engine::Base::` | `Base::` | 9 |
| world_time（13）、world_logging（1）的 `HashValue` | `::GYO::Kernel::Hasher` | `Kernel::Hasher` | 14 |

- 單位指自己的全名（IO 的 183 處）N13 沒有明寫；照 N13 的意思取最短、且和同檔已有的寫法一致的形式（IO 本來就寫 `IOResult`、`IOError`、`Base::Err`，不寫 `IO::`）。
- `::GYO::Kernel::` brief 估 16 處，實際 14 處（TimeTypes.hpp 13、LogWorldLabel.hpp 1）；其餘是巨集（`Params.hpp` 的 `friend struct ::GYO::Kernel::…`），照 N13 保留全限定。
- 設計審查找到的改漏：兩行 namespace 別名 `namespace Json = GYO::Engine::Json;`（`engine/modules/logging/src/JsonLines.cpp`，在 `GYO::Engine::Logging` 裡）、`namespace Logging = GYO::Engine::Logging;`（`engine/modules/trace/src/Trace.cpp`，在 `GYO::Engine::Trace` 裡）。照 N13，`Json::`、`Logging::` 不靠別名就解得到，別名多餘，刪掉。改寫與掃描的樣式要求全名後面接 `::`，別名那行以 `;` 結尾所以漏掉；另寫一個掃描（`scan-unit-name-alone.log`：全名後面不接 `::` 的用法）：`origin/gyo2` 命中這 2 行（正例），刪掉後 0。
- 沒改的：巨集（`Assert.hpp` 的 `GYO_ASSERT`／`GYO_UNREACHABLE`、`Params.hpp` 的兩行 `friend`）；namespace 宣告；註解裡的全名（17 行，是說明文字、多半寫給產品作者看的例子，例 `Hash.hpp` 的 `HashValue(GYO::Kernel::Hasher& h, …)` 寫在產品的程式裡就得全名；N13 管的是程式的寫法）；CMake 的 target 別名 `GYO::Kernel`（N14，是 target 不是 namespace）。
- 改寫用的腳本（`n13.py`、`n13lib.py`）在證據資料夾；改寫的判斷用 `tests/common/support/source_text.py` 的 `strip_comments_and_strings` 遮掉註解與字串。

### 突變的 find／replace 跟著對齊（25 個）

照主會話轉來的規則（使用者在 #269：「你跑了什麼鬼東西一個小時？你改名還需要突變嗎？」）：純改名的批次不做完整重跑，只處理、只跑 find 或 expect 真的受改名影響的突變。

- 靜態檢查（`affected.py`）：gyo2 在用的清單裡，file 在本批改動檔案上的突變，逐一比對 find 在改名前後各出現幾次、replace／expect 是否帶舊寫法。
  - find 失效（改名前 1 次、改名後 0 次）：16 個 → find（與 replace）照同一規則改寫。
  - find 仍有效、但 replace 寫舊全名：8 個 → replace 改寫（不改也能編譯；為了突變插進去的程式和規範一致）。
  - 合計 24 個：Kernel 清單 20、os-subsystem 1（`io-m7-mount-escape`）、world_time 1（`wt-hash-resimulation-missing`）、trace 2。
  - 設計審查補的 1 個：`ns-handle-epoch-success-only`（file 是 `Params.hpp`，本批沒動，所以靜態檢查沒挑出來）的 replace 寫 `GYO::Engine::Base::Result`，插進去的位置在 Kernel 裡 → `Engine::Base::Result`，和同類 8 個一致。
  - expect 帶 `GYO::Engine`／`GYO::Kernel` 的 9 個不受影響：Kernel 的 5 個（`kernel_symbols.py`）比對的是編譯後的符號名（符號名永遠是全名），naming 的 4 個比對守衛訊息（namespace 宣告的根）；它們的 find 在本批前後都剛好一次。
- 對齊後：gyo2 在用的清單裡，file 在 `engine/` 的突變 find 全部剛好出現一次（`bad 0`）。
- 加的突變：0。退的：0（README 第 8 條不適用）。

### 守衛：不加

N13 這條是寫法統一：寫成全名或短名，指到的是同一個實體，歸屬、依賴方向、行為都不變；寫錯的結果最多是同一型別兩種寫法，不會讓別的單位的東西混進來。照「守衛只守本質、不擋誤用」，不立守衛，交每批的設計審查看（N13 條文本來就寫「守衛不查，審查看」）。遮蔽（短名指到別的東西）的結果是編譯錯誤，不是靜默換義，不需要守衛代勞。

## 驗證

本機 macOS Intel，AppleClang，preset `test`；`run_mutations.py` 加 `-B`、先清 `__pycache__`。

- 掃描（`scan.py`，對 git 的樹讀，遮掉註解與字串）：
  - 正例：`origin/gyo2`（65d6c131）的 `engine/` 正向命中 281 處——掃描式會命中。
  - 正向（程式裡還寫舊全名）：本批的樹 0 處。
  - 反向（巨集本體寫了短名，在使用方的 namespace 展開會解錯）：本批的樹 0 處；正例——把 `GYO_UNREACHABLE` 改成 `Base::Detail::…` 的臨時樹命中 1 處。
  - 紀錄：`scan.log`（`2be23809…` 是本批改完、未 commit 時的樹，內容等於 da27a6dd 的 `engine/`；`34c913c7…` 是反向正例的臨時樹；審查者對 HEAD 重跑結果相同）。全名不接 `::` 的用法另見上面的 `scan-unit-name-alone.log`。
- 只動了限定詞：改動的 248 行，每一行拿掉 `(::)GYO::`、`GYO::Engine::`、`Engine::`、`IO::` 之後和原本那行相同（`diff-only-qualifiers.log`，非限定詞的改動 0）。
- configure、build 通過（`configure-1.log`、`build-1.log`；審查後的修正 `build-2.log`）。
- 全量 ctest 161/161（`ctest-full-1.log`；跑的時候 load average 約 109，同機有別的建置，沒有逾時）。審查後的修正（刪兩行別名）再跑一次 161/161（`ctest-full-2.log`）。
- 受影響的突變 24 個：24/24 killed（Kernel 20、os-subsystem 1、world_time 1、trace 2；`mutations/run-{kernel,os,wt,trace}.log`）。其他突變不重跑：本批只改限定詞，測試的檢出力不變；find 與 expect 都已靜態查過。
- 審查補的 `ns-handle-epoch-success-only`：1/1 killed（`mutations/run-kernel-review.log`）。合計 25/25。
- L1：PR 上跑。

## 設計審查（每批固定的一步）

- 審查者：Fable 5.2／high，唯讀（實作者 Opus 5.5）。報告：主 checkout `build/evidence/gyo2-n13-short-names/design-review/REPORT.md`。審查者自己另寫腳本重驗，不沿用作者的結論。
- 結論：可以合併。11 項，保留 10、改 1。

| # | 項目 | 審查的判斷 | 處置 |
|---|---|---|---|
| 1 | IO 指自己的全名直接拿掉 | 其他單位在自己目錄裡都不寫 `<自己>::`；拿掉後仍是 `Path::`、`Stream::`、`FS::`、`IOErrorCode::` 限定名，ADL 與重載決議不變 | 保留 |
| 2 | 註解裡的全名不改 | 說明文字；`Hash.hpp` 是寫給產品作者的例子 | 保留 |
| 3 | `HashValue` 寫 `Kernel::Hasher` | 和 authority、rollback 既有寫法一致；`engine/` 沒有 `GYO::Engine::Kernel` | 保留 |
| 4 | 兩行 namespace 別名還寫全名 | 照 N13 已多餘 | 改：刪掉（見「做了什麼」） |
| 5 | 巨集本體仍全限定 | 對 HEAD 重跑 `scan.py`：forward 0、reverse 0 | 保留 |
| 6 | 全域範圍被改成短名 | 沒有：248 行都在該單位自己的 namespace 裡 | 保留 |
| 7 | 短名被遮蔽而靜默換義 | 沒有：唯一同名的是 `IO::Path` 裡的 `class Path`，本批在那裡改的 6 行只寫 `IOErrorCode::` | 保留 |
| 8 | 突變清單對齊 | 只動 24 個、只差限定詞；21 份清單 file 在 `engine/` 的 1560 個 find 都剛好一次 | 保留 |
| 9 | 只跑受影響的突變 | 成立：只改限定詞＋名字解到同一實體，檢出力不變 | 保留 |
| 10 | 不加守衛 | 成立 | 保留 |
| 11 | 更簡單／堆屎山 | 沒有：repo 沒有新增腳本、守衛、突變 | 保留 |

- 小點：`ns-handle-epoch-success-only` 的 replace → 已改（見「突變」）；dev_log 的 HANDOFF 出處、`scan.log` 的 rev → 已補。
- 交主會話（規則文件，給使用者看過再改）：N13 條文沒寫本批用的兩條判斷——「單位指自己的全名拿掉」與「Engine 的單位裡引用 Kernel 寫 `Kernel::`」；#270 跟上時要靠它們。審查建議在 N13 的例子欄補半句，順帶寫既有的兩個例外：同名類別遮住子 namespace 時從單位名寫起（`engine/core/io/include/engine/io/path/Path.hpp` 的 `IO::Path::…`）、為了擋 ADL 的自我限定（`engine/ecs/entity_store/include/engine/entity_store/EntityColumns.hpp` 的 `EntityStore::EntityOf`）。本卡沒有改規則。

## 證據

主 checkout 的 `build/evidence/gyo2-n13-short-names/`（使用者的 Intel Mac，git 不追蹤）：`configure-1.log`、`build-1.log`、`n13.py`、`n13lib.py`、`align.py`、`affected.py`、`scan.py`、`scan.log`、`diff-only-qualifiers.log`、`ids.txt`（靜態檢查挑出的 24 個突變）、`scan-unit-name-alone.log`、`build-2.log`、`ctest-full-1.log`、`ctest-full-2.log`、`mutations/`、`design-review/REPORT.md`。

## 後續（範圍外，沒有做）

- 交換點的實作 #270 也改 `engine/`：誰後合併，誰 merge 跟上，並把對方新加的全名改成短名（主會話的約定）。交換點分支改的是 `engine/orchestration/`；跟上後再跑一次改寫腳本即可（`n13.py`，在證據資料夾）。
- `tests/`、`examples/` 還有 493 處全名（89 檔）：N13 只講 Engine 的單位與 Kernel，沒講 `GYO::Test` 裡怎麼寫；要不要管是規則層的決定（審查的範圍外發現）。
- 兩行過時的註解：`engine/core/io/src/stream/BufferedStream.cpp:10`（註解掉的舊簽名）、`engine/core/io/include/engine/io/fs/MountPoint.hpp:7`（include 註解）。

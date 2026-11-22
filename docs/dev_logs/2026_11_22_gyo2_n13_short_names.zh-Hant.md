# 2026-11-22 N13 短名（`v8-arch-gyo2-n13-short-names`）

Owner：架構（GYO2 主會話開的任務卡）。分支 `claude/objective-zhukovsky-080798`（PR 開向 `gyo2`）。實作者 Opus 5.5／high；設計審查見下。

## 為什麼

`v8-arch-gyo2-naming-cleanup`（#269，65d6c131）把 `::GYO::Engine::` 改成短名，並在命名規範 N13 補了「引用別的單位」：Engine 的單位裡寫 `<單位>::`，Kernel 裡寫 `Engine::<單位>::`，巨集全限定。使用者 2026-11-22「核准，N13 選 (a)」（HANDOFF `v8-arch-gyo2-naming-cleanup-approved`）——剩下的全名另開一批改完。這一批就是那一批：只改名字，不改行為。照「改名類工作排前面」，在交換點的實作（#270）之前做。

## 做了什麼

`engine/` 的 C/C++ 程式裡，不在註解、字串、前置處理行、巨集本體、namespace 宣告裡的 `GYO::Engine::<單位>::` 與 `(::)GYO::Kernel::`，照所在的單位改寫（27 個檔案、248 行、281 處）：

| 所在 | 原本 | 改成 | 處數 |
|---|---|---|---|
| IO（`engine/core/io`）裡指自己 | `GYO::Engine::IO::` | 拿掉（`IOErrorCode`、`Path::Uri`、`Stream::IStream`、`FS::IFileSystem`） | 183 |
| Kernel 裡指 Base | `GYO::Engine::Base::` | `Engine::Base::` | 75 |
| IO 裡指 Base | `GYO::Engine::Base::` | `Base::` | 9 |
| world_time（13）、world_logging（1）的 `HashValue` | `::GYO::Kernel::Hasher` | `Kernel::Hasher` | 14 |

- 單位指自己的全名（IO 的 183 處）N13 沒有明寫；照 N13 的意思取最短、且和同檔已有的寫法一致的形式（IO 本來就寫 `IOResult`、`IOError`、`Base::Err`，不寫 `IO::`）。
- `::GYO::Kernel::` brief 估 16 處，實際 14 處（TimeTypes.hpp 13、LogWorldLabel.hpp 1）；其餘是巨集（`Params.hpp` 的 `friend struct ::GYO::Kernel::…`），照 N13 保留全限定。
- 沒改的：巨集（`Assert.hpp` 的 `GYO_ASSERT`／`GYO_UNREACHABLE`、`Params.hpp` 的兩行 `friend`）；namespace 宣告；註解裡的全名（17 行，是說明文字、多半寫給產品作者看的例子，例 `Hash.hpp` 的 `HashValue(GYO::Kernel::Hasher& h, …)` 寫在產品的程式裡就得全名；N13 管的是程式的寫法）；CMake 的 target 別名 `GYO::Kernel`（N14，是 target 不是 namespace）。
- 改寫用的腳本（`n13.py`、`n13lib.py`）在證據資料夾；改寫的判斷用 `tests/common/support/source_text.py` 的 `strip_comments_and_strings` 遮掉註解與字串。

### 突變的 find／replace 跟著對齊（24 個）

照主會話轉來的規則（使用者在 #269：「你跑了什麼鬼東西一個小時？你改名還需要突變嗎？」）：純改名的批次不做完整重跑，只處理、只跑 find 或 expect 真的受改名影響的突變。

- 靜態檢查（`affected.py`）：gyo2 在用的清單裡，file 在本批改動檔案上的突變，逐一比對 find 在改名前後各出現幾次、replace／expect 是否帶舊寫法。
  - find 失效（改名前 1 次、改名後 0 次）：16 個 → find（與 replace）照同一規則改寫。
  - find 仍有效、但 replace 寫舊全名：8 個 → replace 改寫（不改也能編譯；為了突變插進去的程式和規範一致）。
  - 合計 24 個：Kernel 清單 20、os-subsystem 1（`io-m7-mount-escape`）、world_time 1（`wt-hash-resimulation-missing`）、trace 2。
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
  - 紀錄：`scan.log`。
- 只動了限定詞：改動的 248 行，每一行拿掉 `(::)GYO::`、`GYO::Engine::`、`Engine::`、`IO::` 之後和原本那行相同（`diff-only-qualifiers.log`，非限定詞的改動 0）。
- configure、build 通過（`configure-1.log`、`build-1.log`）。
- 全量 ctest 161/161（`ctest-full-1.log`；跑的時候 load average 約 109，同機有別的建置，沒有逾時）。
- 受影響的突變 24 個：24/24 killed（Kernel 20、os-subsystem 1、world_time 1、trace 2；`mutations/run-{kernel,os,wt,trace}.log`）。其他突變不重跑：本批只改限定詞，測試的檢出力不變；find 與 expect 都已靜態查過。
- L1：PR 上跑。

## 設計審查（每批固定的一步）

（待填）

## 證據

主 checkout 的 `build/evidence/gyo2-n13-short-names/`（使用者的 Intel Mac，git 不追蹤）：`configure-1.log`、`build-1.log`、`n13.py`、`n13lib.py`、`align.py`、`affected.py`、`scan.py`、`scan.log`、`diff-only-qualifiers.log`、`ids.txt`（受影響的 24 個突變）、`ctest-full-1.log`、`mutations/`。

## 後續（範圍外，沒有做）

- 交換點的實作 #270 也改 `engine/`：誰後合併，誰 merge 跟上，並把對方新加的全名改成短名（主會話的約定）。

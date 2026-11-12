# (c) 區來源檔清單（資料契約 `gyo.zone-c-sources` v1）

> Status: target, not implemented

草案：2026-11-12（J-17 掃描器線，`v8-arch-j17-scanner`）；**架構師 2026-11-12 核定**（含 `owner_root`／`include_roots` 兩欄、同 owner 多份清單）。Owner：架構（契約的審核者是架構師會話；掃描器與自測由 J-17 掃描器線持有）。消費端（掃描器）在 `v8-arch-j17-scanner` 實作；生產端（Kernel 的 G-5 匯出）尚未實作，所以 Status 仍是 target。
出處：spec G-5（啟動時匯出「(c) 區 System 來源檔清單」）、J-17（讀清單並掃描）、J.4（產品守衛）、B.5（(c) 檔的文字掃描清單）；交接卡 `plans/kernel/j17-scanner.zh-Hant.md` §2 的 v1 草案。AGENTS §8：契約資料可由手工、Script、別的工具產生；Runtime 不依賴產生它的工具。

## 1. 用途與兩端

```text
Kernel（G-5 匯出；尚未實作）──┐
手寫（v1 的來源）─────────────┼──→ gyo.zone-c-sources ──→ zone_scan.py（產品守衛，tests/common/zone_scan/）
產品的腳本 ──────────────────┘
```

- 生產端只負責列出「哪些來源檔含 zone=C 的 System」。它不展開標頭、不判規則。
- 消費端（掃描器）讀清單、沿 include 展開到同一 owner 內的標頭、掃六條規則、產出報告。
- 清單是契約（AGENTS §8）：Kernel 的匯出落地後只是換生產端，格式與掃描器不變；格式要變就進版本。

## 2. 格式 v1

```json
{
  "format": "gyo.zone-c-sources",
  "version": 1,
  "generated_by": "manual",
  "repo_commit": "2b4618f9",
  "owner_root": "apps/object_fps_pvp",
  "include_roots": ["apps/object_fps_pvp/include"],
  "files": [
    "apps/object_fps_pvp/src/Collision/CharacterCollision.cpp",
    "apps/object_fps_pvp/src/Pvp/Movement.cpp"
  ]
}
```

| 欄 | 型別 | 必填 | 規則 |
|---|---|---|---|
| `format` | string | 是 | 固定 `"gyo.zone-c-sources"`。別的值＝契約錯誤。 |
| `version` | integer | 是 | 本稿為 `1`。掃描器只接受它認得的版本（見 §4）。 |
| `generated_by` | string | 是 | `"manual"`，或產生它的工具名（例 `"gyo-kernel-report"`）。只進報告標頭，掃描器不據此分支。 |
| `repo_commit` | string | 是 | 產生清單時的 repo commit（7～40 位十六進位）。只進報告標頭；掃描器**不**與 HEAD 比對（清單在分支上會落後，比對會讓守衛因無關的 commit 失敗）。 |
| `owner_root` | string | 是 | 這份清單所屬 owner 的目錄（repo 相對、正斜線、無尾斜線）。標頭只在它之下展開（§3）。 |
| `include_roots` | array of string | 是（可空） | 解析引號 include 的根目錄（repo 相對、正斜線）。每一項都必須在 `owner_root` 之下。空陣列＝只以含入檔所在目錄解析。 |
| `files` | array of string | 是（可空） | 來源檔（副檔名 `.cpp`／`.cc`／`.cxx`）。repo 相對、正斜線、無 `..`、無絕對路徑；**已排序（位元組序）、已去重**；每一項都在 `owner_root` 之下、在磁碟上存在。空陣列合法（owner 還沒有 (c) System），掃描器報「0 檔」並以 0 結束。 |

- 未列在表內的頂層欄＝契約錯誤（嚴格；要加欄就進版本，§4）。
- 只列來源檔，不列標頭：Kernel 從 `source_location`（spec B.1 的 `source`）只知道登記處所在的 `.cpp`。標頭由掃描器展開。
- 一份清單只列一個 owner 的檔；同一 owner 可有多份清單（例：已確認的 (c) 與候選的 (c)，架構師 2026-11-12），掃描器各跑一次、報告分開。多個 owner 各有自己的清單（Product Removability：刪 owner＝刪它的清單與 ctest 列，共通層不改）。

## 3. 標頭展開（消費端的規則，寫進契約以免兩端各解釀一套）

- 展開對象：`files` 內每個檔，以及展開到的每個標頭，其中**引號形式**的 `#include "X"`（遮掉註解與字串後仍是 include 的行）。
- 解析順序：先以含入檔所在目錄，再依序以 `include_roots` 各項；第一個存在的檔勝出（與 MSVC、gcc、clang 對引號 include 的順序一致）。
- 範圍：解析出的路徑**必須在 `owner_root` 之下**才展開並掃描；之外的（Engine 標頭、第三方）不展開、不掃描——Engine 模組由各自的 Design Invariants 守衛（例 `tests/common/entity_store/entity_store_guard.py`），第三方由 determinism.md DET-2 處理。
- 角括號 `#include <X>` 一律不展開（標準庫、系統、第三方）；它本身是不是違規由 ZS1 判。
- 深度：**不限（傳遞閉包）**，同一檔只掃一次。理由：spec J-17 點名的 `Combat.hpp` 從 `PvpMatch.cpp` 要經 `PvpMatch.hpp` 才到（深度 2）；而 (c) 程式能呼叫到的全域狀態就在它傳遞含入的標頭裡（spec G.4 第 3 條「全域在呼叫點」），限制深度會漏。
- 解析不到的引號 include（例 `"nlohmann/json.hpp"`）寫進報告的「未展開」清單，不算錯誤、不算違規。
- 報告中每個被展開的標頭附一條含入鏈（第一次到達它的路徑），讓產品批知道它為什麼被掃。

## 4. 版本規則

- `version` 是整數；掃描器維護「認得的版本集合」（本稿 `{1}`）。
- 不認得的版本：不猜、不降級，印一行錯誤並以結束碼 2 結束（與「有違規＝1」、「乾淨＝0」分開）：

  ```text
  zone_scan: unsupported list version 2 in <path> (supported: 1); refusing to scan
  ```

- `format` 不符、缺必填欄、多出未知欄、路徑不合規則（未排序、重複、反斜線、`..`、不在 `owner_root` 下、磁碟上不存在）：同樣結束碼 2，一行一個錯誤，列完全部再結束。
- 改格式時：加欄、改欄意、改展開規則＝版本 +1，掃描器同一變更加入新版本的讀法；舊版本在掃描器內保留至少到 Kernel 的匯出落地（v1 是手寫清單的版本，之後由 Kernel 匯出接手時再決定是否停用）。

## 5. 掃描器的輸出（契約的另一端；供 ctest 與產品批閱讀）

- 文字（stdout）：一行一個發現，`<repo 相對路徑>:<行>: <規則> <文字>`；候選列（ZS5）用 `ZS5?`。最後一行摘要：`zone_scan: <N> file(s) listed, <M> scanned (<M−N> expanded headers), <L> lines, <H> finding(s), <C> candidate(s)`。
- JSON 報告（`--report FILE`）：`{ "list": {...清單標頭四欄...}, "files_listed", "files_scanned", "lines", "findings": [{file, line, rule, text}], "candidates": [...], "per_rule": {"ZS1": n, ...}, "expanded": [{file, via}], "unresolved_includes": [{file, line, include}] }`。這是 `zone_scan.baseline` 存進證據目錄的檔。
- 結束碼：0 乾淨（候選不算）、1 有違規、2 契約或用法錯誤。`--no-fail` 把 1 改成 0（基線用：只報告不判失敗）。

## 6. 與其他契約的關係

- 這份清單不是 G-5 的另一份報告「(c) 區資料集」（對照 `DigestFields`）；那份另定。
- 規則本體（ZS1～ZS7；ZS＝zone scan，與 spec G.1 的 Kernel 區規則 Z1～Z8 區分）屬掃描器，不屬清單；規則變了清單不變。規則的資料檔（第三方名稱、時鐘別名）放掃描器目錄，不放這裡。

## Design Invariants（草案；待核定後生效）

1. 清單只說「哪些檔」，不說「怎麼判」：生產端不展開標頭、不預判規則；消費端不讀 CMake、不讀 Kernel 內部結構。
2. `version` 進格式；消費端拒絕不認得的版本，不猜。
3. 清單與掃描器都不含產品名的分支（AGENTS §9）：owner 由 `owner_root` 給，產品差異只在資料。

Replacement Criteria：Kernel 的 G-5 匯出落地後，若匯出的內容無法以本格式表達（例：一個 System 跨多個編譯單位、或需要列標頭），不加欄硬撐，改版本或換契約。

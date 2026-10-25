# Engine io-contracts：engine/io 的路徑契約

日期：2026-10-25。**本機 L1 完成**；四平台 L1 要等分支推送。
計畫見 [io-contracts](../architecture/plans/os-subsystem/PLAN.md)（第 3.4、7.2 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「io-contracts」。

## 做了什麼

- `docs/architecture/io.md`：engine/io 的路徑契約（D43、D44、D46）。
  - Engine 的路徑字串一律是 UTF-8。
  - file:// 與原生絕對路徑只有一種對應：`file:///Users/x` ↔ `/Users/x`、`file:///C:/x` ↔ `C:/x`；localhost 等同空 authority；UNC 這一輪回 NotSupported。
  - 路徑不做百分比編碼；`?` 與 `#` 屬於路徑。
  - 長路徑前綴不出現在 engine/io 的字串裡。
  - MountTable 的請求不准越出 mount 根目錄。
- `Uri.cpp`：
  - ParseUri 的 file:// 由獨立分支處理；ToString 印出正規形式；ParseUriLoose 對 file:// 套同一規則。
  - 新增 `FileUriFromNativePath`、`NativePathFromFileUri`。
  - 非 file scheme 的行為不變。
- `MountTable::Resolve`：請求有 `..` 段時回 PathEscapesRoot。

## 驗證與經緯

- 先寫 Red（§14.3）：在舊碼上 12 個案例中 11 個失敗，例如 `file:///Users/x` 得到 `Users/x`，`#` 被拆成 fragment。修正後全部轉綠。
- 契約審查（ultracode，1 位審查者＋1 位對抗檢查者並行）：兩位都判「先修再提交」，共 12 個 major，重疊很多。主要有：
  - ParseUriLoose 對被拒的 file URI 退回舊規則，經 MountTable 往返後變成有效路徑；
  - 四斜線 UNC 被當成本機路徑；
  - `/C:/x` 與含反斜線的 URI 往返後指向另一個檔案；
  - 新的正規化讓 MountTable 越出 mount 根目錄；
  - io.md 有一句與事實不符。
  全部在這一批修正並補測試。只有一個既有問題沒修，記在 HANDOFF：Loose 對未知 scheme 不設定 schemeText。
- 本機 macOS Intel 的 core preset：engine_tests 103／103（UriTests 23 個案例）、ctest 30／30、守衛通過。
- 突變：io-m1～io-m11 共 11／11 killed；整份目錄 22／22 killed。

## 證據

本機執行輸出（git 忽略）。重跑：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json`

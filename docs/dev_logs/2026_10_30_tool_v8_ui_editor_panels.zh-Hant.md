# 工具 v8：v8-tool-ui-editor-panels（EditorApp 的面板拆檔）

日期：2026-10-30。分支 `claude/v8-tool-ui-editor-gizmo`。檔位 medium，引擎 Opus 5.5。估 3～5 h，實際約 0.5 h。

**狀態：** 完成。純搬移，不改行為（審查 S-1）。

相關文件：[ui_editor 審查](../tools/plans/v8/ui-editor-review.md) S-1，[工具線 README](../tools/plans/v8/README.md)「順序與依賴」。

## 做了什麼

`EditorApp.cpp`（2,350 行）拆成：

| 檔 | 行數 | 內容 |
|---|---|---|
| `EditorApp.cpp` | 457 | 初始化、每幀的協調、選單、診斷面板、狀態列、文件命令 |
| `EditorAppHierarchy.cpp` | 592 | Hierarchy 面板與它專用的元素樹操作（新增、刪除、移動、id 重編） |
| `EditorAppCanvas.cpp` | 178 | Canvas 面板（預覽、格線、gizmo 的寫回） |
| `EditorAppInspector.cpp` | 911 | Inspector 面板與它專用的欄位小工具 |
| `EditorAppDialogs.cpp` | 278 | 各個對話框 |
| `EditorDocumentEdits.hpp`／`.cpp` | 27／113 | 兩個以上的面板共用的 7 個函式（路徑欄位、找節點、找 canvas、不重複的宣告 id、改名時更新參照），在 `Detail` 命名空間，放在 `src/`，不公開 |

- `EditorApp` 類別、成員與標頭都沒改。各面板仍是 `EditorApp` 的成員函式，只是定義分到各自的檔。
- 只有一個面板用的函式跟著那個面板走，留在該檔的匿名命名空間。

## 為什麼不拆成各自的類別

AGENTS §4：同等的選項中，選可逆的。把面板拆成擁有自己狀態的類別，要決定哪個面板擁有哪些狀態（選取、交易、對話框的欄位），是另一個設計決定。這一批只做「改一個面板只需打開一個檔」。之後 element-schema 要改的部分，現在都落在 `EditorAppInspector.cpp`。

## 驗證

- 搬移前後逐行比對（每一行出現的次數，略過 include、namespace、using 行）：原本的行一行都沒少，多出的只有共用標頭的 7 個宣告與註解。
- 建置沒有錯誤也沒有警告：匿名命名空間裡若有沒用到的函式，編譯器會警告，所以這也確認了每個函式都放到了用它的檔。
- `ctest -R "gyo_ui_editor|^ui\.|gyo_ui_tests|os\.header_hygiene|os\.boundary_guard"`：14/14 通過（macOS x86_64），包含 `gyo_ui_editor.dependency_guard`。
- 這批是純搬移，所以不做突變；行為由上面的逐行比對與既有測試保證。

## 留下的問題

- `DrawInspector` 本身仍約 790 行，是下一批 `v8-tool-ui-editor-element-schema` 的對象。
- 共用標頭不屬於公開的 include 目錄，只給 ui_editor 自己的原始碼用。

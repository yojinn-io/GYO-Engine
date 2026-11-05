# PvP v8：大廳的 UI action 改成對照表，啟動時和 UI 檔的宣告雙向比對（v8-product-ui-action-check）

日期：2026-11-05。分支 `claude/v8-tool-line-records`（v8 工具線，切自 master `3cf63a87`）。事前宣告：[ui-navigation.md](../tools/plans/v8/ui-navigation.md)（2026-11-05 核准，`v8-product-ui-action-check-approved`）。

**狀態：完成（本機）。L1 待 PR。**

## 為什麼

- `v8-tool-ui-navigation` 你選了 (A)：產品內改成對照表，不改 `gyo.ui`。先做 PvP。
- Engine 已經檢查「元素與 `cancel_action` 用到的 action 都有宣告、payload 型別相符」（`engine/ui/src/UiValidation.cpp`）。
  沒人檢查的是 UI 檔和產品程式之間：宣告了卻沒處理（按了沒反應）、處理了卻沒宣告（字串打錯，那段程式永遠不跑）、處理時假設的 payload 型別和宣告不同。

## 做了什麼

- 新標頭 `apps/object_fps_pvp/include/RetroFPS/Pvp/LobbyActions.hpp`（只有標頭）：
  - `kLobbyActions`：action id → 處理（`LobbyAction`）＋期望的 payload 型別，6 筆。
  - `CheckLobbyActions`：雙向比對，列出所有對不上的 id，分四類：宣告了沒處理、處理了沒宣告、payload 型別不同、處理表裡重複。
  - `ParseLobbyDocument`：解析後立刻比對；任一失敗就回傳空值與錯誤訊息。
  - `ExplainUi` 從 `PvpApplication.cpp` 的匿名 namespace 搬到這個標頭（`ParseLobbyDocument` 也要用），內容不變。
- `PvpApplication.cpp`：
  - `Execute` 的 if／else 換成查表＋`switch`。每個處理的內容照搬。
  - 載入大廳 UI 檔改呼叫 `ParseLobbyDocument`，在 `ui.Initialize` 之前；失敗走原本的 `error = …; return false`。
- 測試 `tests/object_fps_pvp/LobbyActionsTests.cpp`（10 個 case），加進 `object_fps_pvp.cpu`；測試目標多連結 `GYO::Ui`（產品測試 → Engine，方向允許）。
- 突變 6 條，加進 `tests/object_fps_pvp/mutations.json`（批次 `v8-product-ui-action-check`）。

## 行為是否不變

- 對所有到得了 `Execute` 的輸入，新舊相同：
  - `submit_address` 不清 `localStatus`，其他都先清；
  - `leave`／`quit` 不看階段；
  - `refresh`／`create`／`join` 只在 `Lobby` 階段作用。
- 唯一的差別：表裡沒有的 id。舊版會清 `localStatus` 再返回，新版直接返回。這種 id 到不了這裡：
  - `UiRuntime` 只送出元素的 action 或 `cancel_action`；
  - Engine 驗證這兩者都有宣告；
  - 新的啟動比對保證每個宣告都有處理。
- **由審查確認、沒有測試涵蓋的兩件事**（審査者指出）：
  - `Execute` 每個 case 的內容與舊版相同；
  - 啟動時確實呼叫 `ParseLobbyDocument`。

  原因是 `PvpApplication.cpp` 在 `app_support`，沒有任何測試呼叫 `Execute` 或 `InitializeContent`。id 對應到哪個處理，已由新測試固定（見判定 2）。

## 判定（事前宣告的 1～5）

1. **行為不變**：通過。
   - wide 建置樹全建置後，`ctest -L "cpu|shader" -R "^object_fps_pvp\."`：50／50，既有測試沒有改。
   - 這次執行是在審查修正之前（commit `6764f65f`）。修正只動標頭的比對與測試；修正後 `object_fps_pvp.cpu` 由突變執行器開始前的基準檢查再跑一次。
2. **比對抓得到三種不一致**：通過，大廳的 10 個 case、41 個斷言全過。
   - 宣告多一個、處理多一個、payload 不同（none 對 string、none 對 number）：失敗，訊息含該 id；
   - 一致：通過，宣告順序不影響；
   - 真的 `pvp_lobby.json` 和真的對照表：通過；
   - 審查後追加（只收緊，不放寬）：
     - 處理表重複列同一個 id：失敗，id 只列一次；
     - 6 個 id 各自的處理與 payload 型別固定；
     - 解析失敗時，錯誤訊息等於 codec 的訊息。
3. **ミューテーションテスト**：通過，宣告的 4 條加審查後的 2 條，6／6 killed（見下方「突變」）。
4. **L1**：待 PR。
5. **L3**：不需要（事前宣告）。

## 突變

件數的突き合わせ：一致（宣告 6 條、killed 6、survived 0、stale 0；執行器開始前 `object_fps_pvp.cpu` 基準通過）。執行：`python3 tests/object_fps_pvp/run_mutations.py --build-dir build/target/_build/wide --batch v8-product-ui-action-check`，commit `f38c6e43`。

| id | 改壞的是什麼 | 結果 |
|---|---|---|
| `ui-action-check-no-unhandled` | 不報「宣告了沒處理」 | killed |
| `ui-action-check-no-undeclared` | 不報「處理了沒宣告」 | killed |
| `ui-action-check-no-payload` | 不比 payload 型別 | killed |
| `ui-action-check-not-called` | `ParseLobbyDocument` 不呼叫比對 | killed |
| `ui-action-check-no-repeated` | 不報處理表的重複（審查後追加） | killed |
| `ui-action-table-swapped` | 表把 `pvp.create` 指到 join 的處理（審查後追加） | killed |

- 事前宣告寫 4 條，審查後追加 2 條。追加只收緊判定，不放寬。
- D184 的相關範圍：既有突變中 `file` 是本批改動檔的只有 `16-observed-falls-back`、`sdl-g1-product-sdl-call`（`PvpApplication.cpp`），`find` 仍各出現一次、不在改動範圍、殺它們的測試沒改，所以不重跑。
- **和事前宣告的寫法不同之處（§14.1 記錄）**：
  - 宣告說突變 4（啟動時不呼叫比對）由「真的 UI 檔少宣告一個時，啟動失敗」的測試殺。實作改成「真的 UI 檔**多**宣告一個 `pvp.unhandled`」。
  - 原因：少宣告一個按鈕還在用的 action，Engine 的驗證就會先拒絕（`UiValidation.cpp`），失敗的是解析，不是比對，突變因此抓不到。
  - 另外，突變改的是 `ParseLobbyDocument` 內部，證明的是「這個函式會比對」。`PvpApplication` 呼叫的是不是這個函式，靠審查確認（見上）。

## 審查（Opus 5.5／high，子 agent）

沒有阻擋性問題。指出的事與處理：

| 指出 | 處理 |
|---|---|
| 行為不變只靠審查；id 對應到哪個處理沒有測試 | 加測試固定 6 個 id 的處理與 payload，並加突變 `ui-action-table-swapped`；啟動呼叫點與 case 內容寫在上面「由審查確認」 |
| 處理表重複列同一個 id 不會被發現 | `CheckLobbyActions` 加「處理表裡重複」一類，並加測試與突變 `ui-action-check-no-repeated` |
| 突變 4 的測試和宣告的寫法不同 | 記在上面「和事前宣告的寫法不同之處」 |
| 兩個斷言比測試名稱弱 | 收緊：payload 的第二個檢查要含 id；解析失敗要等於 codec 的訊息 |

- 審查另外確認了這些，都沒有問題：
  - 可攜性：`switch` 涵蓋所有 case；`std::span` 的轉換；`inline` 標頭；`false &&` 的突變可以編譯。
  - 架構：沒有變更。
- 附帶一點：`apps/object_fps_pvp/include` 是所有 PvP 函式庫的公開 include 路徑，其中 `match_domain` 沒連結 `GYO::Ui`。目前只有 `app_support` 和測試 include 這個標頭，所以不會壞。

## Architecture

- 沒有 Architecture Delta：
  - `gyo.ui`、Engine、framework 都沒改；
  - 對照表與比對都在 PvP 內。
- 新的依賴邊只有一條：PvP 的 cpu 測試目標 → `GYO::Ui`（產品測試 → Engine）。
- 不做的事照事前宣告：
  - 頁面切換 (B)；
  - `object_fps_v2`（有同樣的問題，要做另排一批）；
  - 把比對搬進 framework。

## 證據

主 checkout 的 `build/evidence/v8-product-ui-action-check/`（本機 Mac，git 不追蹤）：

- `build.txt`、`build-all.txt`：建置紀錄。
- `lobby-tests.txt`：審查前大廳測試的結果。
- `ctest-pvp-cpu-shader.txt`：判定 1 的 50／50。
- `mutations.txt`、`mutations-run.txt`：突變結果。

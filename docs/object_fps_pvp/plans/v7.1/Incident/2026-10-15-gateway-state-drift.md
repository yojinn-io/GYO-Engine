# 2026-10-15 設計事故：Gateway 由無狀態的邊緣漂移成持有 Match 決策狀態

## 1. 摘要

Gateway 最早的設計意圖（使用者，2026-10-15 說明）是：

- 對各種外部 Client 各自提供接口；
- 內部只用一套抽象協議和 Match 對接；
- Gateway 本身無狀態。

v3 起，為了迭代上的需求，Gateway 陸續加入 Match 決策狀態的副本與遊戲語意的處理：`lastResolved` 副本、動作帳本與重送、30 Hz 節拍、結果退役、拒絕線與已寫出去重。這些改動當時都沒有被當成 Architecture Delta（Ownership 改變）提出。

後果：
- 2026-10-10 的事故（[Gateway 悄悄丟掉已解析的命令](../../v7/Incident/2026-10-10-gateway-silent-drop.md)）直接來自 v3 加入的 `lastResolved` 副本。
- v7.1 的檢測範圍有一大塊是為這個有狀態的 Gateway 而做（丟棄回報、Gateway 職責、停留時間、A3／A5），pv7-line 的估計因此膨脹到約 111 h（[pv7-design](../pv7-design.md)「重估的工時」）。

本紀錄的類型是**設計事故**：不是量測或流程出錯，而是設計意圖沒有被寫成可檢查的約束，在一連串各自合理的修改中漂移，最後讓我們在偏離的架構上投入大量檢測與驗證成本。

## 2. 狀態與嚴重度

- **狀態：處置中。** 2026-10-15 認定。2026-10-16 決定方向：Gateway 回到無狀態邊緣（[v7.1 交接](../HANDOFF.md) D94、D95；[方向評估](../gateway-direction.md)）。結案條件見 v7.1 README 的 `incident-close`。
- **嚴重度：高（設計前提）。** 理由：
  - 它關係到 Gateway 與 Match 的責任分工，也就是 D49「Gateway 執行，Match 決定」的前提。
  - 回到原意要同時改 Gateway、Match 與內部協議，而 pv7 是 v7.1 唯一一次 wire 變更：先照有狀態的 Gateway 定了 wire，之後再變薄，就等於第二次改 wire。
  - 這可能屬於 stable-version-first 所說的「重大架構問題」（推翻設計前提，並需要跨模組調整）。是否這樣處理，由使用者判斷。
- **嚴重度的界線（它不是什麼）：**
  - 權威狀態與裁決沒有因此出錯；權威 digest 不經過 Gateway。
  - 「解析並轉換成內部協議」這個最初的要求本身不是問題，它只規定 Gateway 檢查格式與轉換，沒有要它持有遊戲狀態。

## 3. 影響

- **事故**：2026-10-10 事故的根因是 Gateway 依 `lastResolved` 副本丟掉命令，Match 不知道（D51）。v5 收尾、v6 全部與 v7 的驗收都建在被截斷的資訊流上（見該事故紀錄）。
- **重複的判斷與知情權分裂**：Gateway 用帳本副本驗證動作（A3），Match 對同一批也做同樣的接納檢查；Gateway 的丟棄 Match 不知道，才需要 D49 第二階段的丟棄回報。
- **v7.1 的成本**：丟棄回報（約 16 h）、Gateway 職責（約 6 h）、停留時間的窗口語意、A3／A5 的決定，都是因為 Gateway 有狀態才需要。
- **偏離原本的目標**：Gateway 原本要成為多種外部 Client 的無狀態接口；現在要新增一種 Client，就得在新的接口裡重做同樣的遊戲語意處理。
- **相關但不同的成本**：兩份 proto 加 adapter 的轉換，讓每個欄位都要定義兩次（pv7 草案中每個新欄位都「兩份 proto 同號同型」）。這來自「轉換」本身，不是狀態漂移；是否保留轉換，可以在第 7 節的評估中一起決定。

## 4. 時間線

| 日期 | commit／批次 | 事件 |
|---|---|---|
| 2026-09-24 | `bbc1832`（MVP） | 建立 Client → Go Gateway → Adapter → C++ Match 的邊界；Client 與 Runtime 兩份獨立的 proto；adapter 只驗證格式、版本、數值範圍，以 session 對應 PlayerId；公共的 `services/gyo_gateway` 只提供 HTTP、Session、framing（[MVP dev_log](../../../../dev_logs/2026_09_24_object_fps_pvp.md)） |
| 2026-09-26 | `391ca00`（v3，輸入停滯的恢復） | Gateway 加入 `lastResolved` 副本，用來截短、合併輸入窗口（第一個 Match 決策狀態的副本） |
| 2026-09-28 | `edb6de4`（v4，權威射擊） | Gateway 加入每位玩家的動作窗口：帳本、重送、30 Hz 節拍、以帳本副本驗證（A3） |
| 2026-10-02 | `a4ccaa5` | 相位追蹤常數在被截斷的樣本上決定（見 2026-10-10 事故） |
| 2026-10-09 | `a954aa3`（v7 第 08b 批） | 結果改成事件驅動轉送，Gateway 加入結果退役的狀態（`retiredSent`、`retirementAsked`） |
| 2026-10-10 | 事故認定 | Gateway 悄悄丟掉已解析的命令；D49 寫下「Gateway 執行，Match 決定」，D51 重新定義問題 |
| 2026-10-11 | `8a36855`（v7 第 08c 批 F1a） | 已解析的命令照常轉送，並加入拒絕線與已寫出去重（為修事故，在 Gateway 再加一層狀態） |
| 2026-10-15 | v7.1 的 `pv7-design` | pv7-line 重估為約 111 h；使用者質疑 Gateway 的必要性，說明最早的無狀態意圖；本紀錄認定 |

## 5. 根因

**結構性原因：外部與內部的送達語意不同，而轉換放在 Gateway。**

- 外部（Client→Gateway）是有損的 UDP：輸入窗口重複送，動作請求一直重送到收到確認，所以會有重複、過期、亂序。
- 內部（Gateway→Match）的協議被設計成乾淨、可靠的串流，每個命令恰好送一次。
- 在這兩種語意之間轉換，Gateway 必須去重；要去重得正確，就得知道 Match 已經有什麼，於是需要 Match 狀態的副本。
- 之後每一次「可靠送達」相關的需求（停滯恢復、射擊送達、結果轉送、事故修正）都落在 Gateway，因為窗口是在那裡組裝與重送的。

換句話說，內部協議抽象得太乾淨，把可靠送達的責任放進了轉換這一步，Gateway 不可能保持無狀態。

## 6. 流程上的因素（不歸咎個人）

- **意圖沒有寫成約束**：「Gateway 無狀態」只存在於設計者的想法裡，沒有寫進 `network-architecture.zh-Hant.md` 的「不可破壞的契約」，也沒有測試或檢查可以發現違反。
- **意圖無法跨 agent 傳遞**：Gateway 的狀態是由不同的 agent 在不同版本加入的。v3（`391ca00`）與 v4（`edb6de4`）由 Codex 實作（使用者回憶；這兩個 commit 沒有標註），v7 第 08b 批（`a954aa3`）與第 08c 批（`8a36855`）由 Claude 實作（commit 有 Co-Authored-By）。每個 agent 接手時，只看得到程式碼、文件與眼前的任務；「Gateway 無狀態」沒有寫在任何 agent 會讀的地方，所以每一次加狀態，在當時的 agent 看來都是局部合理的修正。
- **AGENTS.md 當時已有通則，但沒有這條約束**：AGENTS.md 從 `caad130`（2026-09-21）起就存在，`e1afcc7`（09-22）時已把「Module Ownership 的變更」列為 Architecture Delta。但「在 Gateway 存一份 `lastResolved` 算不算 Ownership 的變更」，要先知道 Gateway 原本該是無狀態的，才判斷得出來。只有通則，抓不到這種漂移。
- **狀態的加入沒有被當成 Architecture Delta**：每一次都是局部、合理的修正（修延遲、做射擊、改事件驅動、修事故），審查的焦點放在該批的正確性，沒有問「這是否改變了 Gateway 的 Ownership」。
- **事故後只寫了原則，沒有回頭檢查前提**：D49 寫下了「Gateway 執行，Match 決定」，但之後的規劃仍然是圍繞有狀態的 Gateway 補檢測，直到估計膨脹才回頭質疑。

## 7. 偵測

v7.1 的 `pv7-design` 把 pv7-line 重估為約 111 h 之後，使用者質疑「還不如把 Godot 的網路部分抄過來」。討論中比對了 Godot 的模式（伺服器單一程序，可靠通道在伺服器內，去重用自己的狀態），並從 git 歷史追出 Gateway 狀態的加入順序。使用者隨後說明了最早的無狀態意圖。

## 8. 處置與行動項目

| # | 行動 | 負責 | 狀態 |
|---|---|---|---|
| 1 | 在 pv7 的 wire 定稿之前，評估 Gateway 的方向並交使用者決定。候選：(a) 回到無狀態邊緣，內部協議保留身分與冪等性（命令序號、action id），Gateway 原樣轉送，Match 用自己的狀態去重，Gateway 只留連線與 session 的軟狀態；(b) 控制面（大廳、session 發放）留在 Go，遊戲 UDP 直接到 Match；(c) 維持現狀 | v7.1 規劃會話 | 完成（2026-10-16，D94：選 (a)） |
| 2 | 依第 1 項的結果，重新評估 v7.1 的計畫：pv7-design 的 D-1～D-7、pv7-line 的批次與工時、事故後遺症的處置 | v7.1 規劃會話 | 完成（2026-10-16，D95；pv7-line 約 170～179 h） |
| 3 | 把 Gateway 的約束寫進 `network-architecture.zh-Hant.md` 的「不可破壞的契約」。草案：Gateway 只能持有連線與 session 的軟狀態，不得持有 Match 決策狀態的副本，只檢查格式、不依內容做決定 | 依第 1 項 | 排定：v7.1 的 `edge-contract`（條文見 protocol-v7 草案 §2a） |
| 4 | 建議（流程）：在 AGENTS.md 加入各元件的設計約束（或從 AGENTS.md 明確連到寫有約束的契約文件），並把「在邊緣元件加入遊戲決策狀態」列為 Architecture Delta 的一種 | 使用者決定是否交給文件會話 | 交給文件會話（2026-10-16，使用者決定） |

## 9. 學到的事

- **AGENTS.md 是唯一所有 agent 都會讀的地方。** Claude 在每個會話開始時讀入它，Codex 也依慣例讀它。跨 agent、跨版本要保住的設計意圖，必須寫進 AGENTS.md，或從 AGENTS.md 明確連到寫有約束的契約文件（例如 `network-architecture.zh-Hant.md` 的「不可破壞的契約」）。通則告訴 agent「Ownership 改變要提出」；只有寫下來的意圖，才能讓 agent 認出「這就是 Ownership 改變」。
- **設計意圖要寫成可檢查的約束。** 只存在於想法裡的意圖，會在一連串合理的修改中消失。
- **在不同送達語意之間轉換時，內部協議要保留身分與冪等性。** 讓邊緣「清理」串流，等於把可靠送達的狀態放到邊緣。
- **局部合理的修正會累積成漂移。** 在邊緣元件加入狀態，就是 Ownership 的改變，即使改動很小。
- **建在漂移架構上的工作，檢測與驗證成本會膨脹。** v7.1 的估計是這次被注意到的訊號。

## 10. 證據位置

- 原始設計：[MVP dev_log](../../../../dev_logs/2026_09_24_object_fps_pvp.md)「目的」「實作與 Architecture Delta」；[network-architecture](../../../network-architecture.zh-Hant.md) §3「兩份獨立 Protocol」。
- Gateway 的狀態（`95bac31`）：`apps/object_fps_pvp/gateway/server.go:50`（`lastResolved`）、`:583-601`（依副本判斷）；`apps/object_fps_pvp/gateway/action_delivery.go:16`（`actionSendInterval`）、`:28-29`（`retiredSent`、`retirementAsked`）、`:225-252`（動作批次的節拍）。
- 事故與原則：[2026-10-10 事故](../../v7/Incident/2026-10-10-gateway-silent-drop.md)；[v7 交接](../../v7/HANDOFF.md) D49、D51。
- v7.1 的成本：[pv7-design](../pv7-design.md)「重估的工時」；[protocol-v7 草案](../../../protocol-v7.zh-Hant.md) §4。

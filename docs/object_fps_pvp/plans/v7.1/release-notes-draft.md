# v7.1 發行說明草稿（PR #96）

供 PR #96 的說明與發行說明使用；發行前由使用者確認。

## 已出貨、未經功能驗收的 OS 行為變更（D219，Engine D78）

下面 4 批在發行線上，沒有支撐它們的遊戲功能驗收；OS 這邊不再為它們安排 L2／L3。各批的行為摘要由「盤點 OS 子系統並收口到支撐遊戲運行」會話提供（依據：OS 分支 PLAN §7.7、§7.8b、§7.9、§7.18，Engine D49～D57、D64～D67；實機部分是 v7.1 的 D201）。

1. **os-console-shutdown**：Windows 上在主控台用 Ctrl+C、Ctrl+Break 或關閉主控台視窗結束伺服器端程序（Match）時，程序會正常結束並寫完 log（關閉視窗時最多等 4 秒）；主控台上的非 ASCII 文字能正確顯示；Linux 與 macOS 不變。
   - 實機：D201 在 conhost 驗過 Ctrl+C 與關閉視窗。
   - 已知：結束要求後再按一次 Ctrl+C 會立刻結束，log 可能是 0 B（Engine 開放決策 25）；Ctrl+Break 與 Windows Terminal 下的關閉沒有實機驗過。
2. **io-native-paths**：路徑照作業系統的規則解析。`..` 交給 OS 處理（POSIX 上會跟著 symlink）；Windows 的 UNC 網路路徑（`\\server\share\...`）可以對應；POSIX 上以 `//` 開頭的路徑不再被拒。
   - 已知：沒有在實機上從 UNC 路徑啟動遊戲驗證過（P5-6 用的是本機磁碟）。
3. **os-tool-files**：只影響 UI 編輯器，不影響遊戲本身。Windows 上判斷路徑是否在 asset 根目錄內，改用 OS 的大小寫規則（`CompareStringOrdinal`）；系統字型改用不分 OS 的候選清單，也會到使用者的字型資料夾找。
4. **os-utf8-codepage**：每個 Windows 執行檔都把程序的文字編碼（ANSI code page）固定為 UTF-8，不再隨玩家機器的系統語系改變。例如系統錯誤訊息的文字編碼可能和之前不同。
   - Windows 10 1903 起生效；更舊的 Windows 會忽略這項設定，回到系統語系。
   - 已知：MinGW 建置的 manifest 嵌入沒有在 Windows 上確認（CI 只驗 MSVC）。發行包是 CI 的 MSVC 建置。

## 已知限界：Windows 上的 worker 驗收（D231）

worker 驗收（`object_fps_pvp.worker`，Client 網路執行緒的 mock 測試）在 Windows 上不通過，這次不算發行條件。量測與判讀見 [worker-windows-timing.md](worker-windows-timing.md)。

- 出貨的 Client 會開 Engine 的 SDL 視窗。在這個條件下，未確認窗口的重送約 57 次／秒，射擊等 action 在送出要求後約 1 ms 內送出（Windows 實機，D229、D230）。
- 沒有 SDL 的程序裡（例如 worker 探針本身），Client 的重送只有約 43～53 次／秒：Windows 預設的計時器刻度約 15.6 ms，Client 的 60 Hz 重送依賴 SDL 提高計時器解析度（D228）。
- 驗收裡另外兩個失敗看起來在測試端：mock 遇到 WSAECONNRESET 會讓驗收中止（約 5%），以及 token 等待的檢查用 mock 輪詢的約 1 ms 刻度量、超過上限 0.03～0.16 ms（約四成）。這兩點排在 v7.1 之後處理。
- worker 驗收不在 CI 跑（任何平台），macOS 本機每次都通過。

## 新的人物模型與第一人稱武器

- 人物換成 Player_V19，第一人稱武器換成 nailer_v2（取代 PvP 的 Mark23）。
- 已知問題（D240）：Windows 的 LAN 場次中看到模型的外觀有問題，判斷在模型本身（不是 Engine），修正後另發版本。

## 已知限界：Windows LAN 場次（lan15，D240）

完整 pv7 在 Windows 實機和真人一起跑的場次做了一次（2026-10-29，Mac 主持，2 台 Windows），但沒有照事前宣告完成，記為「確認不了」。詳見 [lan15-declaration.md](lan15-declaration.md) 最後一節。

- 做到的：2 台 Windows 對戰約 6 分鐘。觀察結果：
  - Gateway 與 Match 的命令守恆通過；
  - 兩台的射擊與換彈計數和 Match 的裁決完全一致；
  - 沒有中斷。
  - 有 1 次「CONNECTION POOR」警告，10 秒後恢復，沒有被移除。
- 沒有驗證的：
  - 15 分鐘以上的連續對戰；
  - Windows 上的斷線恢復（拔線 2 秒／7 秒）；
  - 關閉視窗後重新加入；
  - 房間已滿（第 5 人）；
  - Windows 主持 Match 的 Tick 精度。
- 連線品質窗口判失敗時沒有寫出原因（觀測的缺口，之後補）。

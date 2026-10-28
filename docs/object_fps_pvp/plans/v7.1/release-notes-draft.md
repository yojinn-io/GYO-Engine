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

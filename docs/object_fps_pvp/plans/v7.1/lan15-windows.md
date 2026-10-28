# lan15-windows：Windows LAN 場次（規劃草案）

更新：2026-10-29。Owner：`object_fps_pvp`。**狀態：規劃中。第 2～6 項已決定（D232），第 1 項（日期與人數）等使用者。** 使用者：「選 1：…接著規劃 lan15。」（D231）

## 目的（Feature → Contract → Architecture）

- **Feature**：對外的玩家用 Windows。完整 pv7（三個原樣轉送、A1、token bucket 限速）從沒有在 Windows 實機上和真人一起跑過（STABLE_BASELINE「已知問題」第 1 項）。這個場次確認 Windows 玩家在 LAN 上能穩定對戰，以及斷線、重新加入、房間已滿這些玩家會碰到的情況。
- **Contract**：不改 wire（pv7 定稿）、不改日誌契約。場次只讀既有的 log、JSONL 紀錄與判定工具。
- **Architecture**：不改。工具的修正（K10）限在驗收工具內。

## 範圍（來源）

| 代號 | 內容 | 判定方式 | 來源 |
|---|---|---|---|
| S1 | 完整 pv7 的對戰場次：Windows Client 對 Mac 上的 Match＋Gateway，至少 15 分鐘 | L2：事前宣告的門檻（門檻來源做對抗式檢查，D45） | after-re-evaluate 第 106、218 行；STABLE_BASELINE |
| S2 | 斷線恢復（outage-resync 的 L3）：Windows 與 macOS 各拔網路線或關 Wi-Fi 2 s 與 7 s 各一次。2 s 不回 Lobby；7 s 約 5 s 後回 Lobby，顯示 `Gateway connection timed out`。記錄 `transient_socket_errors`（WSAECONNRESET）與 endpoint 有沒有改變 | L3（使用者觀察＋log 核對） | D179；after-re-evaluate 第 218 行 |
| S3 | v7 留下的 L3：關閉視窗後重新加入；`room_full`（第 5 人加入被拒） | L3 | STABLE_BASELINE「跳過與未驗證」；v7 D85 |
| S4 | F7：Windows 上的 Match Tick 與 IPC 精度（只記錄） | 只記錄 | INVENTORY F7（目前只有 16 個窗口的數據） |
| S5 | 兩機的時鐘偏移與漂移（時間回聲，≥15 分鐘，只記錄） | 只記錄 | v7 D73；STABLE_BASELINE「平台」 |
| S6 | 握手期的 WSAECONNRESET（K9／W6）：觀察 | 只記錄 | D219 Q5 |
| T1 | ~~K10~~：移到 v7.1 之後（D233：LAN 場次不經 `run_case`） | — | D181、D233 |

## 範圍的限界（AGENTS.md §14 開頭；照 HANDOFF「後續宣告要寫的範圍的限界」照抄，直到解消）

- Windows 的 symlink 案例（MinGW 與沒有開發人員模式的 MSVC 都提早返回）。
- `os.console_close`（CI 與使用者的機器都 Skipped）與 Ctrl+Break（使用者的鍵盤沒有 Break 鍵）。
- quick-close 量的是產品的網路層，不是整個 Client 程序（D202）；`close_timing` 不在 CI 跑（D204）。
- ACP 非 UTF-8 時的行為在 manifest 之下不再測得到（D212）。
- OS 的 ★ 4 批（os-console-shutdown、io-native-paths、os-tool-files、os-utf8-codepage）已出貨、未經功能驗收（D219）。
- Windows 上 worker 驗收不算發行條件（D231）：沒有 SDL 的程序裡 Client 的重送約 43～53 Hz；mock 遇到 WSAECONNRESET 會讓驗收中止（約 5%）；「a token wait outlived four commands」以 mock 輪詢的約 1 ms 刻度量，開著 SDL 視窗時約四成失敗、超過量 0.03～0.16 ms；D226 的 2 次計時器喚醒來源不明；ACK 檢查的相位問題沒有量；D227 只在 macOS 用突變確認過。
- 真實互聯網（WAN）不在範圍（N-phase-estimator-wan-noise）。

## 批次（草案）

```text
lan15-windows
├── lan15-plan      medium｜本文件：範圍、構成、要決定的事
├── lan15-tools     high｜K10（若 S1 用 run_case）；LAN 場次 log 的判定入口
├── lan15-declare   high＋對抗式檢查 1 人｜S1 的 L2 門檻與來源、S2／S3 的 L3 步驟、S4～S6 的記錄方式
├── lan15-kit       medium｜測試包與手冊（v7 LAN_TEST 的 v7.1 版）
├── lan15-session   使用者（＋朋友）｜場次本身，約 1～1.5 h
└── lan15-judge     medium｜照宣告判定、dev_log、STABLE_BASELINE 與 PR #96
```

| 批次 | 估時（我） | 驗收點 |
|---|---|---|
| lan15-plan | 0.5 h | 使用者決定構成與範圍 |
| lan15-tools | 1～2 h | 本機用既有的 LAN 紀錄或合成紀錄跑過判定入口；K10 的 Red→Green |
| lan15-declare | 2～3 h | 對抗式檢查的發現已處理；使用者核准宣告 |
| lan15-kit | 1 h | 使用者在 Windows 上照手冊啟動成功 |
| lan15-session | — | 場次的 log 齊全（收支） |
| lan15-judge | 1 h | 判定與紀錄 |

合計約 5.5～7.5 h（after-re-evaluate 估 7～8 h）。lan15-session 要在 lan15-declare 核准、lan15-kit 完成之後。

## 要決定的事

1. **日期與參加的人**：明天（2026-10-29）的哪個時段；Windows 機器幾台（使用者的 Windows、朋友的 Windows）；Mac 要不要當第 3 位玩家。這決定 S1 的人數、S3 的 `room_full` 怎麼湊到第 5 人（例如用 bot 或 probe 補人）。
2. **構成**：Match＋Gateway 放 Mac（v7 LAN 的構成），還是讓 Windows 主持 Match（S4 的 F7 要 Windows Match 才量得到；Match 不連結 SDL）。兩種都要的話，場次分兩段。
3. **測試包的來源**：
   - (a) 最新發行線的四平台 L1 產物（PR #96 合併前本來就要派發）；
   - (b) Windows 機器自己建置（MSVC），只適用使用者自己的機器；
   - (c) 像 v7 一樣發一個 GitHub snapshot（對外發布，要使用者同意）。
   朋友的機器需要 (a) 或 (c)。
4. **S1 的判定範圍**：哪些項目要有門檻（例如 session 完整、D44 的 epoch 重設、轉送與死信的計數、斷線之外的 `transient_socket_errors`），哪些只記錄。門檻在 lan15-declare 定，來源做對抗式檢查。
5. **F5、N-tt1-l2-wake-record**：after-re-evaluate 說規劃 lan15 時一起決定。W4 已經有 Windows 上 Waiter 的晚醒數據（沒有 SDL 時 P50 0.617 ms），建議 F5 維持「不加自旋」並以 W4 結案，N-tt1-l2-wake-record 不在這次量。
6. **lan15-declare 的對抗式檢查**：1 位審查者，查門檻的來源（D45）。建議用 xhigh-reviewer（xhigh，比主會話高，要使用者同意），或繼承主會話的設定。

## 決定（D232，2026-10-29）

使用者：「2～6 照建議（Mac 主持、L1 產物、F5 以 W4 結案、審查者用 xhigh-reviewer）；第 1 項的日期與人數我另外說。」

- 2：Match＋Gateway 放 Mac。S4（F7，Windows Match 的 Tick 精度）這次量不到，照實記為「未量」，列入範圍的限界。
- 3：測試包用最新發行線的四平台 L1 產物（PR #96 合併前派發的那次）。
- 4：S1 的判定範圍在 lan15-declare 和門檻一起定。
- 5：F5 以 W4 結案（維持不加自旋）；N-tt1-l2-wake-record 這次不量。
- 6：lan15-declare 的對抗式檢查用 xhigh-reviewer（1 人，xhigh，高於主會話，使用者同意）。
- 1：日期與人數，使用者另外說。
- D233：K10 移到 v7.1 之後；lan15-tools 只剩 LAN 場次 log 的判定入口。lan15-declare 開始。

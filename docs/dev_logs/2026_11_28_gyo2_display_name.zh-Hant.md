# 2026-11-28 玩家的顯示名稱（`v8-arch-gyo2-display-name`）

Owner：顯示名稱的任務卡。分支 `claude/gyo2-display-name-d94fe2`（PR #376，開向 `gyo2`）。作者 Opus 5.5／high。宣告：[display-name-declaration.zh-Hant.md](../architecture/target/v2/display-name-declaration.zh-Hant.md)。

## 前提（使用者的決定，經主會話轉達）

1. 「名字讀主機名字可以嗎？如果可就這樣來」「開，顯示名稱那張卡」：開卡。
2. 「照建議，全部核准」：H1 (a) `gethostname`、N1 (I) 登入時帶並簽進 session token、S1 (a) 服務不存、L1 (b) 32 個 code point、D1 (a) 允許重名；R1（正式版的預設）不定。#367 已合併（99e20ae3），照宣告第 10 節動工。

## 各步

- **第 1 步**（`c311d74a`）：`OS::Machine::HostName()`（`engine/core/os/include/engine/os/Machine.hpp`；POSIX `sys/Machine.cpp`、Windows `sys/windows/Machine.cpp`）。字串原樣回報。T1：POSIX 等於 `uname` 的 nodename；Windows `COMPUTERNAME` 是前綴（不分大小寫）。`os.md` 補 Machine 的元件與契約。
- **第 2 步**（`5aa7c781`）：Go。`identity.NormalizeName`（一條規則）、session token v2（名字在簽名內，v1 → `ErrVersion`）、Gateway 的 `X-GYO-Name`（base64url）、rooms 的 `member.name`、`matches.Player{UserID, Name}`、Local 的 `--player-names-stdin`（標準輸入第二行 JSON）。
- **第 3 步**（`fec935a1`）：Client 與 Match。`ClientSettings::name`（選填）、`PlayerNameCandidate`（設定的名字，否則主機名字去 `.local`；256 bytes 內、在 UTF-8 邊界截）、`ClientStart::name`、`SessionRequest(deviceId, name)`、`GatewaySession::name`、`RoomMember::name`、`PlayerLabel(userId, name)`（房間畫面改用它）。Match：`match::ParsePlayerNames`、`WorldOptions::playerNames`、`PlayerNames`（Resource）、`PlayerName`（component，codec v1：長度 u8＋位元組，讀到超過 128 拒）、`NamePlayers`（Main／Update，`OnServer`，在 `SeatPlayers` 之後）。名字相關的登記放在 gameplay sets 之後。

## 第 4 步：A/B、審查、突變

- A/B（macOS，主 checkout `build/evidence/gyo2-display-name/ab.py`，複製自模式配置卡；基準 1620731c 的樹在暫存區）：權威 digest 1257 行逐位元組相同；報告只差 `c PlayerName v1` 與 protocol hash。審查處置後重跑，一樣。
- 設計審查（Opus／high，唯讀）：blocker 0、should 2、nit 6。純淨 Code Review（Fable 5.2／high，只給 git 範圍與程式檔）：12 條。兩份都指出的：名字上限散在多處、最遠端失敗最重；codec 重複手寫；`PlayerNames` 在 Client 端；`GatewaySession::name` 沒人讀。處置與不採用的理由：宣告第 13 節。
- 突變：新增 34 個＋OS 1 個，範圍內既有 6 個、find 過時改寫 2 個，全部 killed。

## 檢驗

- 本機（macOS arm64）：`cpu|shader` 260 項全過（處置前）；處置後相關 11 項、Go vet／test／race 全過。
- 沒跑：Linux 的 A/B、T10 實機（交使用者）；Windows 只靠 L1。

## 宣告的更正

- 第 8 節 T9：world 測試用 net Module 的記憶體連線、標籤 cpu，所以在 L1 跑；原本寫「只在本機」是誤判。
- 其他和宣告不同的地方（名字規則的順序、token 版面、截而不拒、名單一律送、存檔服務不收名字、主機名字取第一個 label）：宣告 13.1。

# 玩家的顯示名稱：事前宣告（`v8-arch-gyo2-display-name`）

任務卡，2026-11-28。**2026-11-28 使用者核准（「照建議，全部核准」）**：H1 (a)、N1 (I)、S1 (a)、L1 (b) 32 個 code point、D1 (a)；R1 照舊不定。#367 已合併（99e20ae3），照第 10 節動工。原本：這一階段只寫事前宣告，交使用者核准；核准前不寫實作程式。#367（模式配置，`v8-arch-gyo2-mode-config`）改的是同一個遊戲的檔案（`rooms.go`、`local.go`、`match_main.cpp`、`PvpWorld.cpp`、`RoomScene.cpp` 都在它的範圍裡），**它合併之前不動程式**。作者 Opus 5.5／high。

行號以 `gyo2` ad28dfa4 為準；#367 合併後行號會動，實作時重對。標「#372」的是頭頂 UI 卡的宣告（`claude/gyo2-overhead-ui-f702b2`，`overhead-ui-declaration.zh-Hant.md`）。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| 名字預設用機器的主機名字，可以用 `settings.json` 的 `name` 覆寫；名字從登入一路帶到房間、Match，再同步給每個 Client；頭頂 UI 和房間畫面都用它 | 卡片 brief（使用者：「名字讀主機名字可以嗎？如果可就這樣來」「開，顯示名稱那張卡」） |
| 頭頂 UI 顯示名字（隊友也顯示）、玩家沒有血條；名字暫用 (N1)「Player 12」，之後只換來源 | #372 第 3.2 節、第 9 節 |
| Platform 相關的只在 OS 層；外部組件只在 adapter；Engine 只給機制、遊戲做選擇；錯誤用 `Result` 只告知；不對誤用加守衛 | AGENTS §1 |
| Gateway 無狀態：只驗 session token、把它認出的使用者放進內部請求（D4） | `games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md`；`services/gateway/gateway.go:1-5` |
| OS 只給事實、遊戲選用途（例：#360 可攜模式，OS 只給執行檔路徑，遊戲決定資料放哪） | `engine/core/os/include/engine/os/Process.hpp:14-20`；`games/object_fps_pvp/src/client_main.cpp:685-709` |
| 核心 PvP 的證明用 A/B：同一台機器、改前改後跑同一支核心 PvP 腳本，每一步的權威 digest 逐位元相同 | #367 宣告第 5 節 |

### 0.2 結論（一段話）

**Engine 只加一個 OS 查詢；其他全在遊戲。** OS 層加 `OS::Machine::HostName()`（POSIX `gethostname`、Windows `GetComputerNameExW`），只回報 OS 給的字串。遊戲決定拿它當名字：`settings.json` 有 `name` 就用它，沒有就用主機名字去掉 `.local`。Client 在登入時把這個**候選名字**送給身分服務；身分服務照**一條名字規則**（Go，只有這一份）整理後簽進 session token（v2）。Gateway 驗 token 時把名字和 userId 一起放進內部請求的 header；房間服務記下每個成員的名字，房間畫面顯示它；開始時名字隨成員清單交給 matches，Local 經 Match 的標準輸入交給 Match。Match 用一個**只在 Match 端寫的 component `PlayerName`** 同步給每個 Client。名字是空的就顯示「Player N」（(N1) 留作退路）。`PlayerName` 不是決策資料：**權威 digest 與 consistency fingerprint 不變**，net 的 protocol hash 會變（多一個複製的 component，兩端一起換）。

---

## 1. 現況（核對過）

| 項目 | 現況 | 出處 |
|---|---|---|
| 登入 | Client 只送 `deviceId`；身分服務回 `{userId, sessionToken, expiresAt}`，不存名字 | `services/identity/identity.go:74-78,98-113`；`src/client/Gateway.cpp:69-75` |
| session token | `version u8｜userId u64｜expiresAt u64`＋HMAC；Go 簽、Go 驗，Client 當不透明字串 | `services/token/token.go:96-136` |
| Gateway | 驗 token 後只把 userId 放進 `X-GYO-User` | `services/gateway/gateway.go:104-127`；`services/internal/api/api.go:15-19` |
| 房間成員 | `member{user, ready, ticket}`；畫面上的成員 `{userId, ready}` | `services/rooms/rooms.go:61-65,112-115,500-510` |
| 分配 Match | `matches.Request.Players []uint64` → `Spec.Players` → Local 的 `--expect-users 7,9`；金鑰走標準輸入第一行 | `services/rooms/rooms.go:368-376`；`services/matches/matches.go:24-36,90-96`；`services/matches/local.go:66-77` |
| Match 的玩家 | `SeatPlayers` 為每個連進來的使用者生一個玩家，`Owner{userId}`；`Owner` 是普通 component（不是決策資料）、有複製 | `src/world/PvpWorld.cpp:648-713,1097,1128` |
| 字串的 Codec | `MatchInfo.arenaId`：長度 u16＋位元組，讀時上限 64 | `src/world/PvpWorld.cpp:225-247` |
| 房間畫面的名字 | `"Player " + userId` | `src/client/RoomScene.cpp:70` |
| 設定檔 | `{"version":1, "resolution", "windowMode", "vsync"}`；一台機器一份（不是每個 profile 一份） | `include/object_fps_pvp/client/Settings.hpp:1-7`；`src/client_main.cpp:685-712` |
| 字型 | UI 字型 BIZ UDPGothic（拉丁、假名、日本漢字；沒有 emoji） | `assets/object_fps_pvp/asset_catalog.json:63` |
| 這台 Mac 的名字（只記形狀，不記內容） | `gethostname`＝LocalHostName＋`.local`，30 字；ComputerName 27 字、含非 ASCII；HostName 沒設 | 本機 `hostname`、`scutil --get …`（2026-11-28） |

---

## 2. 別家怎麼做

| 誰 | 名字從哪來 | 上限、退路、重名 | 出處與等級 |
|---|---|---|---|
| Unreal | Client 在加入的 URL 帶 `?Name=`，伺服器在 `InitNewPlayer` 讀；沒帶就用 `DefaultPlayerName`（＋玩家編號）；名字放在複製的 `PlayerState` | 舊版 `InitNewPlayer` 用 `.Left(20)` 截成 20 字（社群回報，說 4.9.2 改過） | API 文件 `AGameModeBase`（官方，`DefaultPlayerName` 的說明）；[Unreal Answers 305882](https://answers.unrealengine.com/questions/305882/view.html)（社群）。原始碼拿不到，**未讀原始碼** |
| Unreal（OSS Null，沒有線上服務時） | 編輯器裡玩家名字是「DESKTOP-######」這種機器名 | — | [論壇 485543](https://forums.unrealengine.com/t/player-name-shows-as-a-number-instead-of-desktop-in-editor/485543)、[UE-24849](https://issues.unrealengine.com/issue/UE-24849)（社群、issue tracker）：「用主機名字當預設名」有前例。**未確認**是哪個函式 |
| bevy_replicon | 文件範例：Client 連上後在授權訊息裡送 `player_name`，伺服器驗證後才開始複製 | 驗證是遊戲的事 | `~/Code/GitHome/bevy_replicon/src/shared.rs:27-93`（原始碼） |
| lightyear 範例 | 玩家實體只掛 Bevy 的 `Name`（除錯用的名字），沒有玩家自取的名字 | — | `~/Code/GitHome/lightyear/examples/fps/src/server.rs:82` 等 |
| Steam | 帳號上的 persona name；不唯一（身分靠 SteamID） | 多家整理說 32 字元；社群回報非拉丁字母更早被截，可能是 32 bytes | [Steam 論壇](https://steamcommunity.com/discussions/forum/1/3875966763780417327)（社群）。**官方頁沒找到** |
| Go 標準庫 `os.Hostname` | Linux：`uname` 的 nodename；BSD／macOS：`sysctl kern.hostname`；Windows：`GetComputerNameEx(ComputerNamePhysicalDnsHostname)` | — | `/usr/local/go/src/os/sys_linux.go:12-24`、`sys_bsd.go:11-17`、`sys_windows.go:12-24`（Go 1.27.1 原始碼） |
| Source／GoldSrc | 重名時伺服器在前面加「(1)」 | — | **憑記憶，沒找到出處**，只當參考，不當根據 |

讀到的共同點：(1) 沒有帳號名字的遊戲，名字是 Client 宣告、伺服器整理（Unreal、replicon）；(2) 名字和身分分開，身分用 id，名字可以重複（Steam、Unreal）；(3) 有上限、有退路（Unreal 的 20 字與 `DefaultPlayerName`）。

---

## 3. OS 層：主機名字的查詢

**前提：只有 Engine 做得到**（平台 API 只能在 OS 層，AGENTS §1.5）。

```cpp
// engine/core/os/include/engine/os/Machine.hpp
namespace GYO::Engine::OS::Machine {
// The name this machine has on the network, as the OS reports it: gethostname on macOS and Linux (macOS may end it
// with ".local", or give the name the network handed it), GetComputerNameExW(ComputerNamePhysicalDnsHostname) on
// Windows. Asked again on every call. InvalidEncoding when it has no UTF-8 form; IOFailed with the native error when
// the OS refuses.
[[nodiscard]] Base::Result<std::string, OSError> HostName();
}
```

- **新檔 `Machine.hpp`，不放進 `Process.hpp`**：`Process.hpp` 寫的是「這個行程的事實」（`Process.hpp:11-12`），主機名字是機器的事實。
- 實作：POSIX 共用一份（`sys/Machine.cpp`，`gethostname` 的緩衝 256＋保證結尾），Windows 一份（`sys/windows/Machine.cpp`，照 Go 的作法遇到 `ERROR_MORE_DATA` 加大緩衝重試，UTF-16 轉 UTF-8 用既有的 `Wide.hpp`）。字串原樣回報：**不去 `.local`、不改大小寫、不截長度**——那些是遊戲的選擇。
- 不另加「macOS 的 ComputerName（"某某的 MacBook Pro"）」：它要連 SystemConfiguration，Linux、Windows 沒有同一個概念。要不要加是決策點 **H1**。

---

## 4. 名字的規則

**前提：作者能處理的**（遊戲的規則）。規則只寫一份，在身分服務（Go）；Client 不重複實作，只顯示服務整理後的結果。

### 4.1 候選名字（Client，遊戲的選擇）

1. `settings.json` 有 `name` 欄位 → 用它（**空字串也算**：使用者明說不要用主機名字，顯示會退回「Player N」）。
2. 沒有 `name` → `OS::Machine::HostName()`，去掉結尾的 `.local`（macOS 的 Bonjour 後綴）。
3. 查詢失敗 → 空字串，記一行 Warning（照 `client_main.cpp:689-693` 處理執行檔路徑的方式）。

候選名字原樣送出；Client 只把它限制在 256 bytes 以內（請求本體的上限是 4096，`api.go:22-24`）。

### 4.2 規則（身分服務整理，建議值）

| 步驟 | 內容 | 理由 |
|---|---|---|
| 1 | 拿掉控制字元與格式字元（Unicode `Cc`、`Cf`，含 RTL 改向、零寬字元）與行、段分隔（`Zl`、`Zp`） | 換行、改向字元會讓別人的畫面亂掉或冒充別人。Go 解 JSON 時已把不合法的 UTF-8 換成 U+FFFD |
| 2 | 連續的空白（`unicode.IsSpace`）換成一個半形空白，頭尾去掉 | 看起來一樣的名字寫法一樣 |
| 3 | 超過上限就從後面截（以 code point 算，不切半個字） | 截而不拒：登入不因名字失敗（Unreal 的 `.Left(20)` 也是截） |
| 4 | 結果是空的 → 名字是空字串 | 退路在顯示端：「Player N」（N＝userId，(N1) 的字串） |

- **上限**：建議 **32 個 code point**（最多 128 bytes）。決策點 **L1**。這台 Mac 的 LocalHostName 是 24 字，Windows 的電腦名稱常見 15 字以內。
- **允許的字元**：步驟 1 以外都允許（中日文可以）。字型沒有的字（例如 emoji）會畫成缺字框：那是字型的事，不在規則裡擋。
- **重名**：允許。身分是 userId，名字只是表現。決策點 **D1**。同一台機器開兩個 profile 測試（`acceptance_macos.sh` 的 a、b）會是同一個名字，因為設定檔是一台機器一份。

---

## 5. 路徑

```
Client（settings.name 或 HostName） ─POST /v1/session {deviceId, name}→ Gateway → identity
identity：整理名字 → session token v2（userId, expiresAt, name）→ 回 {userId, name, sessionToken, expiresAt}
Gateway：驗 token → X-GYO-User＋X-GYO-Name → rooms
rooms：create／join 時記下成員的名字 → 房間畫面的 members[].name
rooms start → matches.Request.Players [{userId, name}] → Spec.Players → Local：標準輸入第二行（名單）
Match：PlayerNames（Resource）→ NamePlayers 為每個新玩家插入 PlayerName → net 複製 → 每個 Client
Client：RoomScene 與頭頂 UI 都呼叫 PlayerLabel(userId, name)
```

### 5.1 改到的地方

| 段 | 改什麼 | 相容性 |
|---|---|---|
| 外部協議 `POST /v1/session` | 請求多 `name`（選填）；回答多 `name`（整理後） | 只多欄位。Gateway 的 `ProtocolVersion` 留 1：服務與 Client 同一版一起發 |
| identity | `login` 讀 `name`、整理、簽進 token。**不存檔**（決策點 **S1**） | `identity.json` 不變 |
| session token | v2：`version u8(=2)｜userId u64｜expiresAt u64｜nameLen u8｜name`＋HMAC（**名字在簽名範圍內**） | v1 的 token 被拒（`ErrVersion` → 401）；Client 既有的「401 就重新登入一次」（`Gateway.hpp:10-11`）接住。Go 內部格式，C++ 不讀 |
| 內部協議 | `api.HeaderName = "X-GYO-Name"`，值是名字的 base64url（和 token 同一種編碼；HTTP header 只放 ASCII）；`api.NameOf(r)` | rooms 以外的服務不讀它 |
| rooms | `member.name`；`create`、`join` 從 header 記下；`Member` 多 `name`；`start` 把名字放進 `Request.Players` | 外部看到的房間 JSON 只多欄位 |
| matches | `Request.Players`、`Spec.Players` 從 `[]uint64` 改成 `[]Player{UserID, Name}`；`match.players` 照舊只記 userId | 內部協議，rooms 與 matches 同一個 Go module、同一個變更 |
| Local | `--expect-users` 不變；有名字時加 `--player-names-stdin`，標準輸入第二行是 JSON `{"players":[{"userId":7,"name":"…"}]}` | 名字不放命令列：不在行程清單裡被看到（第 6 節），也不用處理逗號與 Unicode 的跳脫 |
| Match（`match_main.cpp`） | 讀第二行 → `PlayerNames`（Resource，userId → 名字；不是決策資料、不複製） | 沒有這個旗標（手動起的 Match、`--connect`）→ 名單是空的 |
| Match 的 World | `PlayerName{std::string text}`：`r.Component`＋`net.Replicate<PlayerName>("PlayerName", codec v1)`，**登記在所有核心登記之後**；新的 System `NamePlayers`（Main／Update，`OnServer`，在 `SeatPlayers` 之後）：有 `Owner`、沒有 `PlayerName` 的玩家 → 插入名單裡的名字（找不到就是空字串） | 不改 `SeatPlayers` 的生成 bundle（理由在 5.2） |
| Codec | 長度 u8＋位元組；讀時超過 128 就拒（照 `MatchInfoCodec` 的做法） | — |
| Client | `ClientSettings::name`（`std::optional<std::string>`；讀寫都保留，`version` 留 1）；`SessionRequest(deviceId, name)`；`GatewaySession::name`；`RoomMember::name`；`PlayerLabel(userId, name)`：名字空就是 `"Player " + userId` | 舊版讀到新設定檔：`DecodeSettings` 只取它認得的欄位（實作時用測試確認不拒絕多的 key） |
| Local World（單人） | 不變：沒有別的玩家，不需要名字 | — |

### 5.2 hash 的影響

| hash | 會不會變 | 為什麼 |
|---|---|---|
| net 的 protocol hash（`NetRegistry::Hash`，`engine/modules/net/src/Replication.cpp:23-33`） | **會**：多一行 `c:PlayerName:1;` | 兩端同一版，handshake 照樣對上；舊 Client 連新 Match 會被 `ProtocolMismatch` 拒（本來就該拒） |
| 核心型別的 wire id | 不變 | `PlayerName` 排在核心的 `Replicate` 之後（#367 宣告第 3 節「登記順序」的同一條約定） |
| 權威 digest（`Authority::Digest`） | 不變 | `PlayerName` 沒有用 `authority.Component` 登記，不是決策資料（`Consistency.hpp:7-13`） |
| consistency fingerprint | 不變 | fingerprint 收的是「寫決策資料的 System」的 access（`ServerAuthorityStrategy.cpp:287-330`）。名字若放進 `SeatPlayers` 的生成 bundle，它的 access 會多一個 component，fingerprint 的文字就變；所以另開 `NamePlayers`，它只寫 `PlayerName`，不在 fingerprint 裡 |
| 內容比對（`wallsDigest`、#367 的模式 digest） | 不變 | 名字不是內容 |

**名字不進任何判定**：Match 的規則（`StartMatch`、`MovePlayers`、`JudgeShots` …）不讀 `PlayerName`；就算名字錯了、空了，遊戲照樣進行。

### 5.3 和頭頂 UI（#372）對齊

#372 第 3.2 節的介面是 `PlayerLabel(userId)`，房間與頭頂共用。這張卡把它改成 **`PlayerLabel(userId, name)`**，同一個退路字串。頭頂的 `TagPlayerOverheads` 從玩家實體讀 `Owner` 與 `PlayerName`，呼叫 `PlayerLabel(owner.userId, name.text)`；`PlayerName` 還沒到的那一兩幀，名字是空的，顯示「Player N」。

- 哪一張先合併都可以：#372 先合併 → 這張卡把它的呼叫處改成兩個參數；這張卡先合併 → #372 直接用新的。`Overhead.label` 的形狀不變。
- 不另做「名字變了就更新」：名字在一場裡不變（登入時定）。

---

## 6. 隱私（請使用者知道）

- **主機名字常帶本人的名字**：macOS 預設的電腦名稱是「某某的 MacBook Pro」，LocalHostName（`gethostname` 拿到的）是「某某s-MacBook-Pro」，兩個都從帳號的全名來。Windows 預設是「DESKTOP-亂數」，比較不會；Linux 看安裝時取的名字。
- **它會被誰看到**：身分服務（簽進 token）、房間裡每一個成員、同一場 Match 的每一個玩家。LAN 測試時這些都是自己的機器；如果之後有外部的人連進來，他們也看得到。
- **不會留在哪裡**：服務不存檔（S1 (a)）；Gateway 與各服務的 log 照舊只記 userId，不記名字；Match 的 log 照舊記「user N joined」。Match 的命令列不放名字（行程清單看不到）。
- **想不用主機名字**：在 `settings.json` 寫 `"name": ""`（顯示「Player N」）或任何別的名字。
- **正式版要不要改預設**：決策點 **R1**，這張卡不定。

---

## 7. 證明核心 PvP 沒變

照 #367 第 5 節的 A/B（#367 合併後它的核心 PvP 腳本 `tests/object_fps_pvp/script/CorePvpScript.cpp` 就在 `gyo2` 上）：

| 檢查 | 做法 | 通過的條件 | 在哪跑 |
|---|---|---|---|
| 權威 digest | 這條線分出的那一點與改後各建一次、同一台機器跑同一支腳本，輸出逐位元組比 | 每一步的權威 digest、Client 收到的值、consistency fingerprint **完全相同**；protocol 的登記文字**只差一行 `c:PlayerName:1;`**（預期的差，寫進報告） | macOS（本機）與 Linux 主機；輸出放主 checkout `build/evidence/gyo2-display-name/` |
| Windows | 不做 A/B（照 #367：建兩次太重），只跑 L1 | 寫明未跑 | — |

---

## 8. 外部可驗的測試

| # | 測什麼 | 通過的條件 | 在哪跑 |
|---|---|---|---|
| T1 | `OS::Machine::HostName()` 三個平台 | 成功、非空、合法 UTF-8；POSIX 等於 `uname()` 的 nodename；Windows 和環境變數 `COMPUTERNAME` 不分大小寫時，`COMPUTERNAME` 是它的前綴（NetBIOS 名字最多 15 字） | L1（cpu 標籤，三平台） |
| T2 | 名字規則（Go） | 一組向量：控制字元、RTL 改向、零寬、連續空白、頭尾空白、超長（中日文、拉丁）、全是空白 → 空字串 | L1（Go 的 vet、test、race test） |
| T3 | session token v2（Go） | 簽了再驗拿回同一個名字；v1 被拒（`ErrVersion`）；改了名字的任何一個位元組 → `ErrMAC`；名字長度欄位超過剩下的位元組 → `ErrLength` | 同上 |
| T4 | 登入一路到房間（Go 的 stack 測試，兩個使用者） | 登入回的 `name` 是整理後的；兩人進同一間房，`members[].name` 各是自己的；開始後交給 Provider 的 `Spec.Players` 帶著兩個名字 | 同上 |
| T5 | Local 的標準輸入（Go） | 第二行是名單 JSON、命令列有 `--player-names-stdin`、命令列裡沒有名字；沒有名字的 Spec 不加旗標也不寫第二行（`local_test.go:24` 的期望照舊） | 同上 |
| T6 | Client 的候選名字（C++） | `name` 有值 → 用它；`"name": ""` → 空；沒有 `name` → 主機名字去掉 `.local`；查詢失敗 → 空＋警告；設定檔讀寫保留 `name`；舊設定檔（沒有 `name`）照讀 | L1 |
| T7 | `PlayerLabel` 與房間畫面（C++） | 名字非空 → 名字；空 → 「Player 12」；房間的成員行用它 | L1 |
| T8 | Match 讀名單（C++） | `--player-names-stdin` 第二行 → `PlayerNames`；不是 JSON、userId 不是數字 → 啟動失敗、log 說哪一行；沒有旗標 → 空名單 | L1 |
| T9 | **名字傳到另一個 Client**（C++ world 測試：Match＋兩個 Client 的記憶體連線） | Match 的名單有 A、B → Client B 看到 A 的玩家實體上 `PlayerName == A`，反之亦然；名單沒有的使用者 → `PlayerName` 是空字串；Codec 讀到超過 128 bytes → 拒 | 照現在的分類：用到 network 的只在本機 |
| T10 | 實機一次（`acceptance_macos.sh` 的流程） | 兩個 Client 的 log 各記下房間裡看到的名字；頭頂 UI 合併後交使用者 playtest 看畫面 | 本機；畫面交使用者 |

---

## 9. 相關突變（只列方向，實際在各步定；D184：新的、find 在改動範圍的）

- 名字規則：控制字元或格式字元沒拿掉；頭尾空白沒去；截斷差一（以 byte 截、切半個字）；空名字沒有變成空字串。
- token：名字不在簽名範圍內；版本沒升；長度欄位沒檢查。
- Gateway 沒帶 `X-GYO-Name`；rooms 不記名字或記成別人的；`start` 沒把名字放進 `Request.Players`。
- Local 沒寫第二行、或把名字放上命令列。
- Match：讀名單時忽略錯誤；`NamePlayers` 不跑（條件恆假）或蓋掉已有的名字。
- Codec：上限沒檢查。
- Client：`"name": ""` 被當成沒有（又用了主機名字）；`.local` 沒去；存設定時把 `name` 丟掉；`PlayerLabel` 不退回「Player N」。

---

## 10. 分批

**一條線、一個 draft PR**（文件同步併在同一個 PR）。動工在 #367 合併、使用者核准這份宣告之後。

```
v8-arch-gyo2-display-name
├─ 第 1 步｜Opus/high  ｜上段｜OS：Machine::HostName（三平台）＋T1
├─ 第 2 步｜Opus/high  ｜上段｜服務：名字規則、token v2、Gateway 的 header、rooms、matches／Local＋T2～T5
├─ 第 3 步｜Opus/high  ｜上段｜Client 與 Match：設定、候選名字、登入、房間畫面、PlayerLabel、名單、PlayerName 同步＋T6～T9
└─ 第 4 步｜Opus/medium｜下段｜A/B（macOS、Linux）、T10、設計審查、純淨 Code Review、突變、dev_log
```

| 步 | 上段／下段與根據 | 實作者 | 審查者 | 檢證者 |
|---|---|---|---|---|
| 1 | **上段**：Engine 的公開介面（OS 層新的 API） | Opus／high | Opus／high（第 4 步的設計審查） | Opus／high |
| 2 | **上段**：外部協議的欄位、token 格式、內部協議的 header 與 `Request` 的形狀 | Opus／high | 同上 | Opus／high |
| 3 | **上段**：wire（新的複製 component、protocol hash）、設定檔的資料契約 | Opus／high | 同上 | Opus／high |
| 4 | 下段（執行與紀錄）；檢驗的是上段的成果，檢驗者不用小模型 | Opus／medium | 純淨 Code Review：Fable／high（只給 git 範圍） | Opus／medium |

- 第 1 步和第 2 步互不依賴（檔案不重疊），可以同時做；第 3 步要兩者都在。
- 沒有 xhigh：沒有需要調查的時序或決定性問題（名字不進判定）。

---

## 11. 要使用者決定的

**結果（2026-11-28，使用者：「照建議，全部核准」）**：H1 (a)、N1 (I)、S1 (a)、L1 (b)、D1 (a) 都照建議；R1 不定。

| # | 問題 | 選項 | 建議與理由 |
|---|---|---|---|
| **H1** | macOS 用哪一個名字 | (a) `gethostname`（「某某s-MacBook-Pro」，遊戲去掉 `.local`；網路有時會給別的名字）(b) 電腦名稱（「某某的 MacBook Pro」，可含中文；要連 SystemConfiguration，只有 macOS 有） | **(a)**：三個平台同一個概念，Go 的 `os.Hostname` 也是它（第 2 節）。要好看的名字用 `settings.json` 覆寫 |
| **N1** | 名字在哪裡交給服務 | (I) 登入時送，簽進 session token，Gateway 放進 header（這份宣告寫的）(J) 建房、進房時在請求本體送，rooms 整理 | **(I)**：名字屬於「這個使用者」，和 userId 一起放在 Gateway 認出的身分裡（D4）；之後別的服務要名字時不用每個 API 都送一次。(J) 改得少（不動 identity、token、Gateway），但名字和身分分開，之後有帳號名字時要整個換掉 |
| **S1** | 服務要不要存名字 | (a) 不存，每次登入由 Client 帶 (b) identity 存在 `identity.json` | **(a)**：名字的來源是 Client 的設定或主機名字，每次啟動都會帶；存了反而要處理「存的和這次送的哪個算數」。玩家要能在別處改名時再存 |
| **L1** | 長度上限 | (a) 20（Unreal 舊版）(b) 32（Steam 常見說法）(c) 16 | **(b) 32 個 code point**：一般的 Mac 預設名字放得下；頭頂 UI 要不要再因寬度縮短是 #372 的表現選擇 |
| **D1** | 重名 | (a) 允許（Steam、Unreal）(b) 房間給後來的人加「(2)」（Source 一派，**憑記憶**） | **(a)**：身分是 userId，名字只是表現；(b) 會讓房間和 Match 的名字跟玩家設定的不一樣。同一台機器兩個 profile 測試會同名，靠 userId 分 |
| **R1** | 正式版的預設（現在不定，列著） | (a) 維持主機名字 (b) 改成「Player N」＋第一次啟動請玩家取名 (c) 只有可攜模式或開發版用主機名字 | 這張卡不定。理由是第 6 節：主機名字常帶本人的名字，對外發布時要重新判 |

---

## 12. 不做

- 不做改名的畫面（`settings.json` 手改即可）；不做名字的髒話過濾、檢舉。
- 不做隊伍、敵我顏色（#372 的事）。
- 單人（Local World）不加名字。
- 不改 `--connect` 的直連路徑：沒有服務，名字一律退回「Player N」。

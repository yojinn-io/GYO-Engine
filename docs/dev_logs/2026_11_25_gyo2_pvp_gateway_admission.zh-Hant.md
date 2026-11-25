# 2026-11-25 PvP Gateway 移植第 2 段：Match 驗 admission token（`v8-product-pvp-gateway`）

Owner：Object_FPS_PVP（產品）。GYO2 主會話開的任務卡（PvP Gateway 移植）。作者 Opus 5.5／high。疊在第 1 段（PR #335）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md)（第 1 節第 2 段、D5、D6、第 4 節 L5、L6）。PvP 卡 2026-11-25 回覆：Match 與座位的檔案它沒在改，直接做。

## 做了什麼

1. **C++ 的 admission token 驗證**（`logic`：`include/object_fps_pvp/match/AdmissionToken.hpp`、`src/match/AdmissionToken.cpp`）：57 bytes 的格式、HMAC-SHA256（RFC 2104，用 Engine 的 `Base::Sha256`）、常數時間比較、檢查順序和 Go 一致（長度、版本、MAC、Match、user、到期）。
2. **兩邊對同一份向量**：Go 的 `services/token/vectors_test.go` 產生並比對 `services/token/testdata/admission_vectors.txt`（`GYO_UPDATE_VECTORS=1` 重寫），C++ 的 `object_fps_pvp.logic` 讀同一份檔、逐條比結果。
3. **座位**（`PvpWorld`）：`Seats::Take(userId, endpoint)`／`Release(userId)`，同一 userId 從別的 endpoint 來的第二條連線拒絕（「the user is already in the match」）、同一 endpoint 的重連照收（按連線計數）；`SeatPlayers` 改呼叫 `Release`。`MatchAdmission(seats, AdmissionCheck{key, matchId, nowUnixSeconds})`：先驗 token（拒絕的 detail 是失敗的欄位），再拿座位。沒有金鑰的 Match 照舊 `SeatAdmission`。
4. **Match 的參數**：`--capacity N`、`--match-id ID` ＋ `--admission-key-stdin`（金鑰是標準輸入第一行的 hex；兩個要一起給）、`--idle-seconds N`（沒人在 Match 裡 N 秒就結束：開始後沒人來、或最後一人離開後；matches 給 60）。
5. **matches 的本機 provider**：金鑰寫進 Match 的 stdin（宣告原本寫環境變數；C++ 讀環境要 `getenv`，MSVC 會出 C4996，改成 stdin）；`Running.Stop`（本機 provider 用 Kill）。
6. **地圖**：`all`／`rooms` 給了 `--content` 時從 `maps/arenas.json` 讀出 Match 能玩的 arena id（`pvp_training_v1` 等；檔名不是 id）。network 測試先抓到這件：用 `pvp_arena` 起 Match 時它回「no installed arena」結束。

## 驗證

- 本機（Intel Mac）：`object_fps_pvp.logic`（向量 11 條、RFC 4231 case 2、hex）、`object_fps_pvp.world`（座位、帶金鑰的 admission：別人的 token、沒 token、自己的、用兩次、到期、別的 Match；沒金鑰的只看座位）。
- `object_fps_pvp.match_process`（network 標籤，本機）：本機 provider 起真的 Match（金鑰經 stdin），等它印出「on UDP port P」（0.39 秒）；`--idle-seconds 1` 沒人來就自己結束；壞的金鑰回 2、不啟動。（最初用「送 datagram 等 ICMP refused」判斷 listen，審查 S3 指出 Windows 上無效，改掉。）
- 突變：61 bytes 格式時 11/11 killed；第 1 段的審查結論（S3：token 拿掉 `reserved`）merge 進來、C++ 改 57 bytes、向量重產後 10/10 killed；第 2 段審查修正後見下。

## 沒驗

- Client 帶 token 連上真的 Match：移到第 5 段（Client 的連線參數要等 #332 改成 `NetConnectTarget`）。
- `match_main.cpp` 把 `MatchAdmission` 接進 `NetProductOptions::admission` 那一行、`--capacity` 傳給 `Seats`：沒有測試殺得掉（第 5 段的端到端會走到）。
- `--idle-seconds` 的「最後一人離開後」那條（和「沒人來」是同一個計時，只測了沒人來）；`SeatPlayers` 對 peer 0 的 Release。

## 設計審查（Opus／high，唯讀；作者同模型，Fable 暫停中）

blocker 0、should 6、nit 8、要使用者定的 0。報告：主 checkout `build/evidence/v8-product-pvp-gateway/segment2-design-review.zh-Hant.md`。審查者沒有跑建置、測試、突變（只讀遠端分支）。

- S1（座位和 ServerHost 的連線表不同步：同一 endpoint 的較新握手，伺服器先關舊連線、立刻問 admission，但座位下一幀才還 → 新舊都斷；連線在 Peer 生成前結束時 `peer == 0`、座位永遠不還）→ 方案 A（只改產品）：座位記 endpoint 並按連線計數，同一 endpoint 放行；Release 不再看 `peer != 0`。方案 B（Engine 在 `End()` 同步呼叫產品的釋放鉤子、或照 netcode 把「同一 client id 一條」做進傳輸層）列給主會話參考，本卡不做。
- S2（`--exit-when-empty` 沒有寬限期，和「比賽多長都能重連」衝突）→ `--idle-seconds N` 一個計時兩條路（沒人來、人走光），matches 給 60；寬限期可參數化，所以「沒人來」測得快（1 秒）。
- S3（ICMP 探測在 Windows 一定通過：Go 在 Windows 關掉 `SIO_UDP_CONNRESET`）→ `Local.Output` 可注入，測試等 Match 印的「on UDP port P」那一行（Product 建好、socket 已開之後才印）。
- S4（D6 承諾量 Match 啟動時間沒做）→ 量了：約 0.39 秒，寫進宣告 D6。
- S5（SHA-256 只給 hex，三處解回位元組）→ 第 3 段的 Engine commit 在 Base 加位元組版本。
- S6（驗證紀錄缺口）→ 本 dev_log 補；C++ 的 Length、Version 加突變；接線與 `--capacity` 列進〈沒驗〉。
- N1 → `Seats` 改 class、只留一份上鎖的狀態（`Taken()`），註解改寫理由（Match 主迴圈讀、World 可能在自己的執行緒）。N2 → 不改：`CHECK_MESSAGE` 的訊息是失敗時印的，「a user who left cannot come back」描述的就是失敗。N3 → `DecodeHex` 移到 `data/Hex.hpp`（Match 與第 5 段的 Client 共用）。N4 → arena 沒有 id 時拒絕（第二份讀 arena 的實作照留，只讀 id）。N5 → 不改：`Stop` 只有測試用；第 4 段要在離場時寫檔，屆時 Match 正常結束靠 `--idle-seconds`，matches 本身不殺 Match。N6 → 向量檔的資料夾加 `.gitattributes`（`* -text`）。N7 → 不改（`expiresAt ≥ 2^63` 不會發生）。N8 → `AdmissionKey` 改名 `AdmissionCheck`；向量數改成剛好 11。

修正後：`object_fps_pvp.logic`、`.world`、`.client`、`.match`、`.match_process`、`-L go` 全過；突變 15/15 killed（新增 `cpp-length`、`cpp-version`、`seat-reconnect-refused`、`match-never-idle`）。

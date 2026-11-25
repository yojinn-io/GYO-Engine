# 2026-11-25 PvP Gateway 移植第 2 段：Match 驗 admission token（`v8-product-pvp-gateway`）

Owner：Object_FPS_PVP（產品）。GYO2 主會話開的任務卡（PvP Gateway 移植）。作者 Opus 5.5／high。疊在第 1 段（PR #335）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md)（第 1 節第 2 段、D5、D6、第 4 節 L5、L6）。PvP 卡 2026-11-25 回覆：Match 與座位的檔案它沒在改，直接做。

## 做了什麼

1. **C++ 的 admission token 驗證**（`logic`：`include/object_fps_pvp/match/AdmissionToken.hpp`、`src/match/AdmissionToken.cpp`）：57 bytes 的格式、HMAC-SHA256（RFC 2104，用 Engine 的 `Base::Sha256`）、常數時間比較、檢查順序和 Go 一致（長度、版本、MAC、Match、user、到期）。
2. **兩邊對同一份向量**：Go 的 `services/token/vectors_test.go` 產生並比對 `services/token/testdata/admission_vectors.txt`（`GYO_UPDATE_VECTORS=1` 重寫），C++ 的 `object_fps_pvp.logic` 讀同一份檔、逐條比結果。
3. **座位**（`PvpWorld`）：`Seats::Take(userId)`／`Release(userId)`，同一 userId 第二條連線拒絕（「the user is already in the match」）；`SeatPlayers` 改呼叫 `Release`。`MatchAdmission(seats, AdmissionKey{key, matchId, nowUnixSeconds})`：先驗 token（拒絕的 detail 是失敗的欄位），再拿座位。沒有金鑰的 Match 照舊 `SeatAdmission`。
4. **Match 的參數**：`--capacity N`、`--match-id ID` ＋ `--admission-key-stdin`（金鑰是標準輸入第一行的 hex；兩個要一起給）、`--exit-when-empty`（最後一個玩家離開就結束；第一分鐘沒人來也結束）。
5. **matches 的本機 provider**：金鑰寫進 Match 的 stdin（宣告原本寫環境變數；C++ 讀環境要 `getenv`，MSVC 會出 C4996，改成 stdin）；`Running.Stop`（本機 provider 用 Kill）。
6. **地圖**：`all`／`rooms` 給了 `--content` 時從 `maps/arenas.json` 讀出 Match 能玩的 arena id（`pvp_training_v1` 等；檔名不是 id）。network 測試先抓到這件：用 `pvp_arena` 起 Match 時它回「no installed arena」結束。

## 驗證

- 本機（Intel Mac）：`object_fps_pvp.logic`（向量 11 條、RFC 4231 case 2、hex）、`object_fps_pvp.world`（座位、帶金鑰的 admission：別人的 token、沒 token、自己的、用兩次、到期、別的 Match；沒金鑰的只看座位）。
- `object_fps_pvp.match_process`（network 標籤，本機）：本機 provider 起真的 Match（金鑰經 stdin），它在 port 上 listen（送一個 datagram：關著的 port 會回 ICMP 讓 connected UDP 讀到 refused，開著的只會逾時；對關著的 port 驗過探測回 false）；壞的金鑰回 2、不啟動。
- 突變：C++ 與 Match 的 11 項（`pvp-gw-cpp-*`、`seat-*`、`admission-token-ignored`、`match-bad-key`）＋ `local-port-kept` 11/11 killed（第 1 段審查前的 61 bytes 格式時跑的；改 57 bytes 後重跑見下）。
- 第 1 段的審查結論（S3：token 拿掉 `reserved`）merge 進來後：C++ 改 57 bytes、向量重產。

## 沒驗

- Client 帶 token 連上真的 Match：移到第 5 段（Client 的連線參數要等 #332 改成 `NetConnectTarget`）。
- `--exit-when-empty` 的兩條結束路徑沒有自動測試（「第一分鐘沒人來」要等一分鐘）；第 5 段的端到端會走到「最後一個玩家離開」。

## 設計審查

（審查後補）

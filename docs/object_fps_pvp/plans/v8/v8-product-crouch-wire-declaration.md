# v8-product-crouch 的事前宣告（wire 接入）

更新：2026-10-30。Owner：`object_fps_pvp`。**狀態：草稿，等你核准（含下面「等你決定」的 4 項）。** 契約：[protocol-v8](../../protocol-v8.zh-Hant.md) §1～§5、§8（已定稿，`pv8-final-approve-round2`）。不碰 wire 的部分已完成（[事前宣告](v8-product-crouch-declaration.md)、`fd950ea3`、`347fa9ba`）。檔位 high（預測與 wire 轉換局部 xhigh）｜Opus 5.5，估時 8～12 h（`v8-product-crouch` 全體 19～25 h 扣掉已完成的部分；超過 18 h 停下回報）。

## 範圍

這批之後，蹲下在真的連線中可以用。pv8-line 的 A 層到此做完。

1. **版本值 7→8**：`Wire.hpp`、`adapter.go`、`acceptance_protocol.hpp`、`acceptance_util.py` 四處；突變 `09-version-python` 的 find／replace 同步改。
2. **proto**（兩份同號）：`MovementCommand.crouch_held`＝7、`sprint_held`＝8；`PlayerState.crouched`＝20、`sprinting`＝21；Ready 10～13／Welcome 12～15 的 4 個 float（`crouch_body_height`、`crouch_eye_height`、`crouch_speed_scale`、`sprint_speed_scale`）。
3. **Gateway**：逐欄複製上面的欄位；`ReadyFitsWelcome` 把 4 個 float 算進去。沒有新狀態、新判斷、新 DropReason；`state_whitelist_test.go` 不變。
4. **C++ 轉換**：IpcHost（Match↔Gateway）與 ClientConnection（Client↔Gateway）的 MovementCommand、PlayerState、Ready、Welcome。新欄位一律接在 struct 最後（位置式 brace-init 加錯位置仍能編譯，protocol-v8 §2）。
5. **移動規則資料**（新的產品內 Data Contract，§3）：Match 啟動時讀，驗證不過就明確報錯、不退回預設值；放進 Ready。
6. **Client**：收到 Welcome 時驗證 4 個值並放進 `StanceRules`，預測用 Welcome 的值（`SetStanceRules`）；同一 session 內改變就報 `Match movement rules changed within a session`。`sprint_held` 一律送 false（② 才有行為）。
7. **按鍵**：蹲鍵接到 `crouchHeld`（鍵位與方式見「等你決定」）；操作說明的那一行加上蹲鍵。
8. **呈現**：
   - 本機相機的眼高依姿態（`StanceEyeHeight`），只在呈現層做時間平滑；
   - 遠端玩家依 `crouched` 選 `Crouch_Idle_Loop`／`Crouch_Fwd_Loop`（`UAL1_Standard.fbx` 內已有），模型縮放照舊用站立的 `bodyHeight`（`v8-product-crouch-pose`）。
9. **大小**：§6 的釘全部重釘（下面 U2）。

不做：衝刺的行為（②）、L2／L3（見「範圍的限界」）。

## 受入判定（看結果之前宣告）

### W1：站立路徑的權威結果不變

- base＝`58af2861`（本批之前），branch＝本批，同一台 Intel Mac，`compare_authority_trees.py` 在 scale 1、3 各 1 次。
- 判定：**既有的 38 個情境 × 2 個 scale，差異 0 個**。Match 改讀規則資料，但資料檔的值等於現在的產品常數（`DefaultStanceRules`），所以結果不變。不重跑；有差異就停下回報。
- 新增的情境（「等你決定」的第 3 項）只加新 golden 行，舊行不改。

### W2：衝刺在 pv8 沒有行為

- 所有情境的 `sprinting` 恆為 false。
- 同一情境 `sprint_held` 送 true 與 false，digest 相同（新增一對情境，或一個 digest 測試直接比兩次的結果）。

### W3：連線上的蹲下

- C++：IpcHost、ClientConnection 的往返，新欄位全部相等（`crouch_held`、`sprint_held`、`crouched`、`sprinting`、4 個 float）。
- Go：adapter 的逐欄複製（input、snapshot、Ready→Welcome）；protoreflect 守衛把 4 個 float 列進 Ready／Welcome 的欄位表。
- worker 探針（`object_fps_pvp.worker`）：Client 按住蹲鍵送出 `crouch_held`＝true，收到的權威 `crouched`＝true；放開且有空間時回到 false。

### W4：移動規則資料

- 每一條驗證規則的違規都拒收（有限值；0 < 眼高 ≤ 蹲高 < arena 身高；蹲高 ≥ 2·半徑；0 < 蹲速 ≤ 1；0 < 衝刺 ≤ 3），Match 不啟動並寫出原因。
- Client 收到違規的 Welcome 參數時報錯；同一 session 內參數改變時報錯；預測用 Welcome 的值（把資料檔改成和常數不同的值，Client 的預測跟著變）。

### W5：大小

- 以 `proto.Size` 實測，數值寫進 protocol-v8 §6，各處釘成完全相等，每項加 24 B 標頭都 ≤1200 B。
- 推論：PlayerState 多 2 個 bool（號碼 20、21，各 3 B），每人 +6 B；MovementCommand 多 2 個 bool（各 2 B），12 個命令 +48 B；Welcome 多 4 個 float（各 5 B），+20 B。只當參考。

### W6：呈現

- 蹲下前後模型的縮放相同。
- 遠端玩家 `crouched` 時選蹲姿 clip，站立時照舊（`PlayerPresentationTests`）。

### W7：突變

照 protocol-v8 §8 列的（只登記和這批有關的）：預測改用本機常數；模型縮放改用蹲姿高度；Gateway 刪掉任一個新欄位的複製；C++ 轉換漏掉任一個新欄位；資料檔的任一條驗證拿掉；`sprint_held` 被 Match 採用；大小的等式改成 ≤。只在預期失敗時算 killed。D184 的相關突變（find 落在改動的行上，或判定測試被改的）全部 killed。

### W8：既有測試

`test` preset 的全部 CTest 與 `go test ./...` 通過。四平台 L1 在 PR 時跑。

## 會改判定的既有測試（AGENTS §14.4，先列出）

| # | 測試 | 守的性質 | 改法 | 寬鬆面 | 收緊面 |
|---|---|---|---|---|---|
| U1 | 版本位元組的 golden：`WireTests.cpp`（標頭 bytes 4～5）、`heartbeat_test.go:362-363`、`IpcHostTests.cpp:1271-1272`（`\x08\x07`）；突變 `09-version-python` | 每種訊息的位元組固定，只有版本欄位隨版本改 | 版本位元組 7→8，其餘位元組不變（pv6→pv7 的先例） | 無 | 無（同等） |
| U2 | 大小釘：`capacity_test.go`（40＋171N、42＋189N）、`worker_main.cpp`（724）、`network_main.cpp`（674）、`adapter_test.go`（402）；`actions_test.go` 的 Welcome＝1200 邊界 | 最壞情況 ≤1200 B，任何欄位增大都會被抓到 | 改成 W5 的實測值，仍是完全相等；Welcome 邊界的邏輯不變；`maximumSnapshot` 的 bool 取 true | 無 | 無（完全相等） |

開工後若發現本表以外的測試要改判定，停下回報。

## 等你決定

### `v8-product-crouch-key`：蹲鍵

- (a) 按住 Left Ctrl 蹲、放開站起（建議：主流 FPS 的預設，和 `crouch_held` 的「按住型意圖」直接對應，不必在 Client 記切換狀態）；
- (b) 按 C 切換；
- (c) 兩者都有（Left Ctrl 按住、C 切換）。

### `v8-product-crouch-rules-file`：移動規則資料檔的位置

- (a) `assets/object_fps_pvp/movement_rules.json`（`version` 1），Match 以 `--movement-rules path` 讀，預設用正式版的檔案，和 `--arena` 同一個模式（建議：倍率和地圖無關，§3 寫的就是這個形狀）；
- (b) 每張地圖各自帶，放進 `arenas.json`。地圖可以有不同的蹲姿參數，但規則資料和地圖內容的版本綁在一起。

### `v8-product-crouch-scenarios`：protocol-v8 §8 列的 12 個新情境

不碰 wire 的部分已有 `CrouchTests.cpp` 的 6 個測試與 digest 的 2 個情境（`move-crouch/contract`、`move-crouch-slab/slab`）。

- (a) 對照表：已被既有測試涵蓋的寫出對應（例如 `crouch-jump-blocked` → `PvP crouch jump: … never jumps from under a slab`），只把沒有涵蓋的加成 digest 情境：`crouch-walk-under`、`crouch-jump-hold`、`crouch-epoch-reseed`，以及 W2 的 `sprint_held` 一對（建議：digest 情境是跨平台 golden 的來源，只加會走到新路徑的）；
- (b) 12 個都加成 digest 情境（重複的覆蓋較多，golden 檔會多 12 行）。

### `v8-product-crouch-run-quad-mixed`：`run_quad` 的 mixed 模式

它以 `PROTOCOL_VERSION - 1` 寫成通用形式，給它 pv7 的 base 執行檔就能檢查「pv7 的 Client／Gateway／Match 對 pv8 明確失敗」。

- (a) 保留：只改註解（「the older Match closes with its own (pv6) reason」改成 pv7），在 L2 有機會時跑（建議：不改程式就能用）；
- (b) 退役：從 `run_quad.py` 刪掉 mixed，寫明理由。

## 範圍的限界

- 本機只有 Intel Mac；W1 只在這台比，跨平台靠 golden 子集與 L1。
- 蹲下的手感與遠端蹲姿的觀感 CI 看不到。這批不做 L3；之後在 Windows 實機確認手感時另排（照「CI 是主要標準」）。
- 混版（① 的 Client 對 ② 的 Match）不偵測（`pv8-design-remaining-picks` D-5 (a)）。

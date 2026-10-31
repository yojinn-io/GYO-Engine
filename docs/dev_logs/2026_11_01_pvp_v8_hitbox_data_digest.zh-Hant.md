# PvP v8：v8-product-hitbox-data-digest（D3：動畫內容的校準 `animation_digest`）

日期：2026-11-01。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 5～6 h，實際約 2 h。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-data-declaration.md)的 D3（`v8-product-hitbox-digest-scope` (a)）。`v8-product-hitbox-data` 三個子批全部完成。

## 做了什麼

- **`AnimationContentDigest`**（`player_content` 的新檔 `PlayerAnimationDigest.cpp`）：Match 與 Client 共用。依序寫入：
  1. 固定文字 `object_fps_pvp.animation_digest.v1`；
  2. `presentation.json` 影響姿勢的值：`reference_speed`、兩個 native speed、`transition_seconds`、`max_frame_delta_seconds`、四個動作的跨度、aim 的兩個角度、上半身遮罩的根骨頭名稱；
  3. 15 個 clip，依定義的固定順序：名稱、長度，以及每個 track 的節點名稱和 translation／rotation／scale 的 key（時間與值）；
  4. 骨架：每個節點的名稱、父節點（根是 0xFFFFFFFF）、參考姿勢。
  - 數值都寫 IEEE 754 或整數的位元，little-endian；字串寫 u32 長度加位元組。
  - 雜湊：`Engine::Base::Sha256`，取前 8 個位元組，little-endian 讀成 uint64（`AnimationDigestFromSha256`），結果是 0 時改成 1。
  - **比宣告多放的一項**：`max_frame_delta_seconds`。Match 的動畫規則（`ReadPlayerAnimationSource`）也讀它，不放進去的話，兩邊這個值不同時偵測不到。
- **proto**：`Ready.animation_digest`＝18、`Welcome.animation_digest`＝20（fixed64）。Go 綁定用 protoc 36.2 與 protoc-gen-go v1.36.11 重產。
- **Match**（`match_main.cpp`）：啟動時用 `LoadPlayerPoseDefinition(Assets 資料夾, arena 的身高)` 載入姿勢定義，算出 digest，寫一行 `animation_digest=` 的 log，交給 `IpcHost`。`IpcHost` 多一個建構參數，放進 Ready；0 時拋出例外，和 arena digest 一樣。
- **Gateway**：
  - `runtime_link.go`：Ready 的 `animation_digest` 是 0 就拒絕。
  - `server.go`：Ready → Welcome 照抄。
  - `ReadyFitsWelcome`：最壞情況的 Welcome 也帶上非 0 的 `animation_digest`。
- **Client**：
  - `ClientConnection::SetAnimationDigest`（0 拋出例外）。收到 Welcome 時，值不同（包括 0）就失敗：`Match animation content differs from this client (Match …, this Client …)`，不進入對局。沒有設定時不比對，和 arena 的作法相同。
  - `PlayerPresentation::AnimationDigest()`：用 Client 載入的定義算出；`PvpApplication` 在 `Initialize` 之後交給連線。
- **文件**：protocol-v9 §6 的實測；`protocol/README.md` 的 `animation_digest` 段落。

## 判定

- **H5：通過。**「PvP animation digest …」4 個測試（`PlayerAnimationDigestTests.cpp`）：
  - Client 路徑（完整載入）和 Match 路徑（`LoadPlayerPoseDefinition`）的 digest 相同，而且不是 0。
  - 下面每一項各改一個，digest 都改變：
    - presentation 的 12 個值（含上半身的根骨頭）；
    - 一個 translation 的值、一個 rotation 的值、一個 key 的時間、少一個 key、track 的節點、clip 的長度、clip 的名稱；
    - clip 的順序（idle 與 walk 互換）；
    - 骨架的一個父節點、一個參考姿勢、一個節點名稱。
  - 不變的：拿掉武器並改掛點；加一個配件並拿掉材質綁定。形狀不是 `AnimationContentDigest` 的輸入，所以本來就不受影響。
  - 前 8 個位元組的讀法：`01 02 … 08` 讀成 `0x0807060504030201`；0 改成 1。
- **H6：本機的值 `6274bea50a363a99`**（Intel Mac）。四平台的值由 L1 印出（`anim_bottlenecks` 的摘要行與 `PlayerAnimationDigestTests` 的 `animation_digest=` 行），在 pv9 的 L1 比對；不同就停下回報。
- **H7：通過。**
  - Gateway（Go）：Ready 的 `animation_digest` 是 0 時不接受；Welcome 帶著同一個值（`TestHTTPReservationHandshakeInputAndSnapshot`）；`ReadyFitsWelcome` 的邊界。
  - Client（worker）：Welcome 的值不同、或是 0，都回 `Match animation content differs from this client`，不進入 Playing；值相同就照常進入。`SetAnimationDigest(0)` 被拒絕。
  - IpcHost：Ready 帶的值等於建構時給的值；0 拋出例外。
  - 往返是分段證明的，和 A3 的 E6 相同：C++ → runtime（IpcHost 測試）、runtime → client（Go 的照抄）、client → Client（worker）。真的 Match 加上真的 Client 的整段，在 L2 試玩時才會走到。
- **H8：通過，實測。**
  - Welcome 多 10 B（號碼 20 的 tag 是 2 B）。最大仍是 1200 B（原本的釘不變），arena id 的上限由 1052 B 縮成 **1042 B**，加了一個釘。
  - Ready 也多 10 B；Ready 走 TCP 框，沒有大小的釘。

## 改了判定的既有測試（AGENTS §14.4）

| # | 測試 | 改法 | 寬鬆面 | 收緊面 |
|---|---|---|---|---|
| W1 | `TestReadyRequiresRulesAndBoundsWorstCaseWelcome` | 最壞的 Welcome 多帶 digest；新增 arena id 長度＝1042 的釘 | 無 | 有：多釘一個長度 |
| W3 | Go 測試裡 5 處 Ready、`TestV1ClientAndAllOldRuntimeVersionsAreRejected` 的案例表、IpcHost 測試的建構、worker 的 Welcome | 多帶一個非 0 的 digest；案例表多一列「沒有 animation digest」 | 無 | 有：多拒絕一種 Ready |

只有收緊面，照 §14.4 由我改，事後揭露。

## 突變

**27／27 killed**（只在預期的訊息失敗時才算 killed），在未提交的工作樹上用 `--allow-dirty` 跑。

- 本批新增 16 個（`batch` 為 `v8-product-hitbox-data-digest`）：
  - 雜湊漏掉一項：`transition_seconds`、上半身的根骨頭、translation 的 key、track 的節點、父節點，各一個；clip 的順序不照定義（idle 與 walk 依索引排序）；雜湊讀進武器的掛點；
  - 0 沒有改成 1；前 8 個位元組讀成 big-endian；
  - Gateway：不照抄、接受 0、`ReadyFitsWelcome` 不算它；
  - IpcHost：不送出、接受 0；
  - Client：不比對、把 0 當成相同。
- D184 相關的既有突變 11 個（find 落在改動範圍，或抓它的測試被改）：`09-arena-client-welcome`、`09-arena-gateway-zero`、`09-arena-gateway-welcome`、`16-gateway-ready-at-least`、`v71ve-ipc-hosts-small-arena`、`v71te-welcome-without-echo`、`v71te-welcome-bound-without-echo`、`pv8sn-welcome-fit-small-id`、`v8cw-gateway-drops-crouch-held`、`v8cw-gateway-drops-welcome-rule`、`v8cw-gateway-fit-ignores-rules`。
- 第一輪 19／27，8 個是 `wrong_failure`（測試失敗了，但不是預期的訊息）。程式都沒有錯，原因是測試或清單：
  1. D3 的雜湊 4 個：測試的 `INFO(name)` 印出的是字串的指標位址，不是名稱，預期的文字比對不到。改成 `INFO(std::string(name))`。
  2. `09-arena-gateway-zero`：我改了 Go 測試的失敗訊息，舊的預期文字比對不到。改回保留原句 `with digest 0 admitted`，後面再接動畫 digest。
  3. `v71ve-ipc-hosts-small-arena`：預期的正規式是 `IpcHost(host, small)`，建構多了一個參數。正規式改成帶上新參數。
  4. `v71te-welcome-bound-without-echo`：我新加的 arena id 長度釘比原本 1200 B 的檢查先失敗。把長度釘移到 1200 B 的檢查後面；`d3-fits-welcome-uncounted` 的預期因此是 `maximum admitted Welcome=1210`。
  5. `09-arena-client-welcome`（worker）：截斷的輸出只看得到 `Gateway connection timed out`。沒有改任何會影響它的東西，第二輪就 killed。**原因分不出來**（實作、測試本身還是環境）；A3 也記過 worker 偶發失敗一次，L1 時一起看。
  - 修完之後，這 9 個（含 `d3-fits-welcome-uncounted`）單獨重跑，9／9；抓它們的 Go 測試改過順序與訊息，所以第一輪已經 killed 的 4 個（`16-gateway-ready-at-least`、`pv8sn-welcome-fit-small-id`、`v8cw-gateway-fit-ignores-rules`、`d3-gateway-zero-admitted`）也重跑，4／4。
- 清單的維護：`09-arena-gateway-welcome` 的 find 改成新的那一行（Welcome 多了 `AnimationDigest`）；`v71ve-ipc-hosts-small-arena` 的預期正規式見上。兩者抓的東西不變。

## 其他測試

- `test` preset 的 CTest 111／111、`go test ./...` 都通過。

## 範圍的限界

- **網格不在 digest 裡。** 身體的 `scale` 和 `anchor` 由網格的參考高度算出，判定在世界座標裡會用到它們。宣告的範圍沒有網格；而網格經蒙皮算出的值，跨平台不一定逐位元相同（B3 的 H-pose 就不同），放進去可能讓 H6 失敗。所以兩邊的網格不同時，這個 digest 偵測不到。
- `bodyHeight` 來自 arena，由 arena digest 涵蓋。
- H6 的四平台比對要等 pv9 的 L1。

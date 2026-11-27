# D2 調查：交互鍵放 `PlayerInput` 還是另一個輸入型別（`v8-arch-gyo2-interact-door` 附錄）

任務卡，2026-11-27。主會話轉交的調查（HANDOFF `v8-arch-gyo2-interact-d2-direction`）。**這份只報告調查結果，D2 還沒定**；使用者核准之後才寫進宣告（`interact-door-declaration.zh-Hant.md` 10.2）。作者 Opus 5.5／high。

程式的行號：Engine 照 `gyo2` 85df84da；遊戲照 #367 的分支 1baf9122；技能研究照 #374 的分支 617020d5。參考實作照 `~/Code/GitHome` 的本地源碼。

## 0. 結論

1. **E 照開槍的路走得通**：開槍是 `PlayerInput::fire`（一次性）→ Match 在固定步用同一步的視線打射線（`JudgeShots`）；E 一樣是 `PlayerInput` 的一個一次性欄位 → Match 的 `JudgeInteractions` 用同一條視線（第 1 節）。
2. **成本很小**：`PlayerInput` 沒有位元欄，每個按鍵各佔 1 byte；加 E 是每格多 1 byte、每則輸入訊息多 5 bytes（重送 5 格），Codec 2 → 3。(b) 的另一個輸入型別是每 Client 每幀多一則約 60 bytes 的訊息（第 2 節）。
3. **#374 7.3 (a) 的比法能證明核心 PvP 沒變**：比「扣掉 `PlayerInput` 的權威 digest」，再逐步比對新舊 `PlayerInput` 的欄位（新的 `interact` 恆為 0）。我原本推薦 (b) 的理由「核心 PvP 的 digest 會變」因此不成立（第 3 節）。
4. **別家**：每 tick 的輸入流（Unity Netcode for Entities、lightyear）把一次性按鍵放進輸入；RPC／message 給「不綁 tick、不預測」的事（加入隊伍、聊天、大廳流程）（第 4 節）。
5. **「按性質分通道」大致成立，但「性質」要寫成「綁不綁某一步、要不要預測」**，不是功能主題：背包裡的「快捷鍵用藥、切武器」綁某一步、要預測，應該走每 tick 的輸入；「拖曳整理背包、購買」才是可靠訊息（第 5 節）。

**和原推薦 (b) 比：現在推薦 (a)，`interact` 放進 `PlayerInput`。**(b) 剩下的好處只有「組沒開時玩家身上連這個欄位都沒有」，抵不過 (a) 的三點：和開槍同一條路、訊息小一個數量級、不必走 Engine 沒測過的「一個 World 兩種輸入」（宣告第 1 步的 Engine 測試可以拿掉）。

## 1. 開槍怎麼送、怎麼判；E 走不走得通

| 步 | 開槍 | E（(a)） |
|---|---|---|
| 欄位 | `PlayerInput::fire`，`std::uint8_t`，一次性（`games/object_fps_pvp/include/object_fps_pvp/world/Components.hpp:103-120`） | `PlayerInput::interact`，同型、同語意 |
| Client 寫入 | Frame 的 `ReadControls` → `present::StepInput::fire` → Sim 的 `StepInputOf` 取出並清掉按下（`games/object_fps_pvp/src/client/ClientWorld.cpp:586-607`）→ `WriteStepInput` 寫進這一步的槽（`:614-629`） | 同一條；`StepInputOf` 多清一個欄位 |
| wire | `InputOptions` 的 Codec 逐欄寫（`games/object_fps_pvp/src/world/PvpWorld.cpp:198-216`，版本 2）；`held` 在缺一格時清掉 `jump`、`fire`、`reload`（`:218-223`，C-IN3） | Codec 多寫一個欄位、版本 3；`held` 多清 `interact` |
| 判定 | `UseWeapons`（兩端都跑，Client 預測自己的子彈數與冷卻，`PvpWorld.cpp:445-471`）寫 `lastShotTick`；`JudgeShots`（只在 Match，`OnServer`）用 `ShotRay` 的視線打射線（`:474-477`、`:486-561`） | `JudgeInteractions`（只在 Match 與 Local，`OnServer`）用同一個 `ShotRay` 打射線；Client 不預測交互的結果，等 Match 複製門的 `DoorState` |
| 開賽前 | `Playable` 把輸入換成空的（`PvpWorld.cpp:400-403`） | 同一個 `Playable` |

走得通。唯一的不同：開槍有 Client 預測的一半（`UseWeapons`），交互沒有，只在權威端判。這不需要不同的輸入路。

## 2. `PlayerInput` 有沒有空位、加 E 的成本

- **沒有位元欄**：`present`、`jump`、`crouch`、`fire`、`reload`、`ready` 六個 `std::uint8_t`，各佔 1 byte（`Components.hpp:111-118`）；`NetWriter::Value` 照型別的寬度寫（`engine/modules/net/include/engine/net/Codec.hpp:22-38`）。一格的值現在是 4×4＋6＝22 bytes。
- **加 E**：一格 23 bytes；一則輸入訊息重送 5 格（`InputOptions::redundancy` 預設 5，`engine/modules/net/include/engine/net/Replication.hpp:518`），**多 5 bytes**。Codec 2 → 3；`HashValue(PlayerInput)` 多一個欄位（`PvpWorld.cpp:94-105`）。
- **(b) 的成本**：每種輸入每幀各送一則訊息（`engine/modules/net/src/Replication.cpp:1179-1182`），每格 1＋4＋1＋2＋值（`Replication.hpp:862-874`），1 byte 的值重送 5 格是 45 bytes，加上訊息頭約 60 bytes；也就是 sandbox 模式每 Client 每幀多一則訊息。
- **可選**：Codec 升版的那一次，把七個按鍵（含 `interact`）收成一個位元組的位元欄：一格從 23 bytes 變 17 bytes，之後再加按鍵（#374 S1 的 `jumpHeld`、技能鍵）只是用掉一個位元，Codec 仍要升版但大小不變。這不是 D2 的一部分，列出來給使用者選（第 6 節 Q-b）。

## 3. #374 7.3 (a) 的比法能不能證明「加 E 之後核心 PvP 沒變」

#374 7.3（`skill-research.zh-Hant.md`，分支 617020d5）：(a) 比「扣掉 `PlayerInput` 的 digest」（`PlayerBody`、`PlayerCombat`、`ShotRecord`、`HitRecord`），並逐步比對新舊 `PlayerInput` 的對應。

- **做得到**：權威 digest 是每個決策型別各自的加總，再相加（mod 2^64；`engine/modules/authority/include/engine/authority/Registrations.hpp:246-264`；`Consistency.hpp` 開頭的說明）。核心 PvP 腳本現在印 `A::Digest`（#367 的 `tests/object_fps_pvp/script/CorePvpScript.cpp:149-153`）。改成也印「`PlayerInput` 那一項」與「其餘的和」：`PlayerInput` 那一項可以用 `Authority::Detail::ComponentDigest<PlayerInput>`（`Registrations.hpp:246`，`Detail` 底下，只在測試用）配上它的 authority 登記序號算，或由腳本自己對其他四個型別各算一次。
- **能證明什麼**：核心 PvP 腳本沒有按 E，新的 `interact` 每一步都是 0。「其餘的和」逐位元相同＝除了輸入的 hash，每一筆決策資料都沒變；再逐步比「新 `PlayerInput` 去掉 `interact`」＝「舊 `PlayerInput`」，就證明輸入本身也只多了一個恆為 0 的欄位。兩者合起來就是「加 E 不改核心 PvP 的行為」。
- **蓋不到的**：核心 PvP 裡玩家真的按了 E（Client 會送 `interact` = 1，Match 的 `interaction` 組沒開、不判）。這由宣告的 `[pvp-interact-off]` 測試蓋：按了 E，除了 `PlayerInput` 之外的決策資料都和沒按時相同。
- **要不要和 S1 合成一次 Codec 升版**：
  - 兩條線都等 arena 規則拆分（門線的 scene 欄、技能線的 S0），合併的時間可能接近。
  - 合成一次的好處只有「A/B 的新比法只做一次」與「Codec 少升一號」；升版本身沒有成本（開發期，沒有舊 Client 要相容）。
  - 綁在一起的代價：兩條線互等。
  - **推薦：不綁**。先到的那一條做出 7.3 (a) 的比法（腳本與 `ab.py` 的改動），後到的沿用；如果使用者選了第 2 節的位元欄，由先到的那一條一起做。

## 4. 別家把交互鍵、一次性按鍵放哪

| 家 | 一次性按鍵 | RPC／message 給什麼 | 理由（原文的意思） | 出處 |
|---|---|---|---|---|
| Unity Netcode for Entities | 放在每 tick 的輸入：`IInputComponentData` 裡用 `InputEvent`，例如 `GetKeyDown` 收到的按下；就算那一 tick 的輸入在路上掉了，伺服器也恰好收到一次 | 「高層的遊戲流程事件」與「一次性、**不預測**的指令」：加入隊伍、聊天、取消靜音、要求離開 | RPC 不綁任何 tick、收到的那一幀處理、不能預測與回滾；要持久的狀態（例：寶箱的開關）放 ghost component，RPC 送了重連就沒了 | `com.unity.netcode` `Documentation~/command-stream.md:32`；`Documentation~/rpcs.md:18-35`（寶箱的例子 `:27-29`）；`Runtime/Command/IInputComponentData.cs:27-56` |
| lightyear | 遊戲的動作（含 `Fire`）是輸入：`PlayerActions{…, Fire}`、`register_input_action::<Shoot>()` | 大廳流程：`StartGame`、`JoinLobby`、`ExitLobby` 是 message；同一個 Plugin 裡輸入另外 `InputPlugin::<Inputs>` | 文件沒逐字講理由；分法和 Unity 相同 | `demos/spaceships/src/protocol.rs:154-159`；`demos/projectiles/src/protocol.rs:123`；`examples/lobby/src/protocol.rs:173-180` |
| bevy_replicon | **沒有每 tick 的輸入緩衝**；Client → Server 只有 client event／message（`client_trigger`） | 全部 | replicon 只做複製，預測與輸入交給別的套件 | `src/shared/message/client_event.rs:152-162`（#361 1.3） |
| Unreal GAS／Lyra | ability 的啟動走可靠的 RPC，帶 prediction key 讓 Client 先預測 | — | 本機沒有源碼，憑記憶（**未確認**） | — |

**交互鍵本身**：四家的文件都沒有點名「交互鍵」放哪一邊（#361 1.4 已記：Unity 的 `InputEvent` 適合交互是推論）。能確定的是分法：**綁某一步、要和那一步的狀態一起判的 → 每 tick 的輸入；不綁 tick、不預測的 → RPC／message。**交互鍵要和「那一步的視線」一起判（Q1 (a)），屬前者。

## 5. 「按性質分通道」有沒有反例

主會話轉來的分法：每 tick 的輸入（含 E、技能鍵）走 `PlayerInput`；可靠的離散指令（背包、裝備、丟棄、購買、選單）走可靠訊息；自動拾取不需要輸入。

| 情況 | 照「背包、裝備＝可靠訊息」 | 實際該走哪 | 為什麼 |
|---|---|---|---|
| 快捷鍵用藥、切武器（1／2／3） | 可靠訊息 | **每 tick 的輸入** | 要在某一步生效、Client 要預測（動畫、彈匣）；可靠訊息不綁 tick（Unity `rpcs.md:33`），預測錯了沒辦法照步重跑。#367 宣告第 6 節的 W3「背包切換」也是放在每步的狀態 |
| 拖曳整理背包、丟棄、購買 | 可靠訊息 | 可靠訊息 | 不綁 tick、不預測、順序重要、內容可能大 |
| 台本的選項 | — | 可靠訊息比較合（不綁 tick）；那條通道做好之前，放輸入的一個 byte 也行（#361 1.7） | 過渡 |
| Unreal 的 ability 啟動 | — | 可靠 RPC＋prediction key | **GYO 用不上的反例**：它能把要預測的動作放可靠 RPC，是因為有 prediction key 把 RPC 和預測對起來；GYO 的預測與回滾照 tick 走，沒有這個（未確認，見第 4 節） |
| 掉包時的延遲 | — | — | 有序可靠的通道（`engine/modules/net/include/engine/net/Connection.hpp:39-45` 的 `OrderedReliable`）掉一包，後面的都要等重送；每 tick 的輸入有重送 5 格，不用等。這也是「要即時的放輸入」的理由 |

**結論**：分通道成立，但判準要寫成「**綁不綁某一步、要不要預測、內容大不大**」，而不是功能主題（背包、選單）。同一個主題（背包）裡兩種都有。這和 Unity 的分法（第 4 節）一致。

現況（轉來的，已核對）：傳輸層支援多條通道（`Connection.hpp:39-45` 兩種 `ChannelKind`），net 只開兩條自用（`engine/modules/net/src/NetModule.cpp:170-171`；`engine/modules/net/include/engine/net/NetModule.hpp:76-77`），遊戲還不能登記自己的可靠訊息（#361 Q6 的缺口，這一批不碰）。

## 6. 要使用者決定的點

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **D2** | 交互鍵怎麼上 wire | (a) `PlayerInput` 加 `interact`（Codec 2 → 3，A/B 用 #374 7.3 (a) 的比法）(b) 另一個輸入型別 `InteractInput` | **(a)**（第 0 節最後一段） |
| Q-b | Codec 升版的那一次，要不要把按鍵收成位元欄 | (a) 收（一格 23 → 17 bytes，之後加按鍵不變大）(b) 不收（照現在一個按鍵 1 byte） | (a)，但不急：由先改 `PlayerInput` 的那一條線做 |
| Q-c | 要不要和技能線 S1 合成一次升版 | (a) 不綁，先到的做比法、後到的沿用 (b) 綁在同一批 | (a)（第 3 節） |

D2 選 (a) 時，宣告要跟著改的地方（核准後才改）：第 2.1 節 `InteractInput` 那一列、第 2.2 節的三個 System（`GiveInteractInput`、`ControlOwnInteract`、`WriteStepInteract`）拿掉，改成 `StepInputOf` 多取一個欄位；第 9.1、9.2 節的表；第 10.1 節第 1 步的 Engine 測試拿掉，A/B 改用 7.3 (a) 的比法。

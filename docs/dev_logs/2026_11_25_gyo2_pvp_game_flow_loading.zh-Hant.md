# 2026-11-25 遊戲流程框架第 4 段：loading 與就緒、Match 開始前的相關性（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（遊戲流程框架）。作者 Opus 5.5／high。分支疊在第 3 段（#349）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md)第 3.5 節與第 4 節第 4 段；使用者 `v8-arch-gyo2-loading-screen-and-relevance`。

## 做了什麼

1. **Match 的開始**（`world/PvpWorld.hpp`）：`MatchInfo::phase`（`Waiting`／`Playing`，照常複製）；`PlayerInput::ready`；`StartMatch`（權威端，`FixedUpdate/Update`，`MovePlayers` 之前）：等待的使用者都有玩家且輸入 `ready`，或第一個玩家來了之後超過期限 → `Playing`。`Waiting` 時 `MovePlayers`、`UseWeapons` 拿不到輸入（不動、不轉、不開槍；兩端同一條，Client 照複製來的 phase 預測）。Match 沒有等待的使用者（手動起、`--connect`、測試）時一開始就 `Playing`；Local World 等的是本機玩家、沒有期限。
2. **相關性**：`ShowPlayers`（Match，`Main/Update`，`SeatPlayers` 之後）：`Waiting` 時每個 Peer 的 `PeerVisibility` 只有它控制的玩家（別人的位置不送），`Playing` 時拿掉（看見全部）。net 現有的 C-VIS1，net 不改。
3. **Match 與 Go**：`--expect-users ID,…`、`--start-deadline-seconds N`（預設 60）；matches 的 `Spec.Players`（rooms 開始時已經給成員）→ `local.go` 加 `--expect-users`。
4. **Client 的就緒**（`TrackReadiness`）：(1) 遊戲的資產連同依賴（`AssetView::Ready`：角色、武器、viewmodel、貼圖、聲音、字型、地圖模型、overlay 文件；失敗也算完成，不卡住）→ 進度 0～0.8；(2) 預熱：資產齊了之後 `DrawnFrames::shown` 前進兩幀且這期間沒有略過的 draw（程式經 Frame World 的交換函式從 `SDLGPUStatus` 寫進；沒有 GPU 的測試不等）；(3) 自己的玩家從 Match（單人：本機）到了；(4) 再畫完一幀 → `ready`，經操作的 `StepInput::ready` 進 `PlayerInput::ready`。
5. **loading 畫面**（`pvp_game.json` 的 `loading` canvas）：不透明的底、進度條（`horizontal_slider` 綁 `progress`）、現在在做什麼（資料、描畫準備、連線、第一個畫面、等其他玩家），蓋到 `ready` 且 `Playing`；「中止」：多人照撤退（離開房間回大廳），單人回單人房間、不結算。單人的暫停只在開始之後才停 Solo World 的時間。

## 驗證

- 本機（Intel Mac）`ctest -L "cpu|gpu"` 256/256、`ctest -L "go|network"` 5/5、`go test ./services/...` 全過。
- world：兩個 Client、等待 {1,2}：開始前各自只看得到自己、一個準備好的 Client 走路不動；兩個都準備好 → `Playing`、互相看得到、能走；只來一個時期限到了也開始。
- client：loading 畫面在第一幀、進度單調遞增且經過 0～1 之間、載完畫完自己的玩家到了才 `ready`、Match 那邊這個 Client 的玩家 `ready == 1`、之後畫面拿掉；單人一輪先 loading 再開始。端到端（`object_fps_pvp.gateway_client`）：Match 等房間的成員準備好才開始，Client 在開始後走暫停畫面撤退。
- 突變：`pvp-start-ignores-ready`、`pvp-start-not-hidden`、`pvp-start-not-frozen`、`pvp-start-no-deadline`、`pvp-loading-not-shown`、`pvp-loading-not-told`，加 find 改了的 `pvp-pause-not-shown`、`pvp-pause-not-stopping`、`pvp-gw-evacuate-no-leave`：9/9 killed。

## 設計審查（Opus／high，作者同模型，審查者只拿成果物與原始要求）

blocker 1、should 9、nit 12。

- B1（就緒的資產一步先鎖住：Client 先載自己的第一張地圖，Match 的地圖還沒來時就判定齊了；換地圖後新模型沒載完也 `ready`）→ 每一幀重算到 `ready`，新模型的載入讓這一步與之後重來；新案「Match 很慢、地圖不是第一張：就緒時那張地圖的模型已載入」。
- S1（每幀都略過 draw 時預熱不結束）→ 最多 120 幀。S2（自己的身體、別人的身體與第三人稱武器在 loading 期間沒畫過）→ 不改：要放替身或 Engine 的 pipeline 預熱（Bevy `PipelineCache`），記成後續。S3（進度分子分母單位不同）→ 頂層 Handle 的比例、只增不減。S4（空 Handle 讓 loading 不結束）→ 空的不算。S5（失敗不回上一個場景，偏離宣告）→ 宣告修訂 4 寫理由。S6（overlay 的文件出來前遊戲露出）→ 相機先什麼都不看、清成黑色，overlay 出來才給回。S7（Client 重算時 phase 拿最新值）→ `MatchInfo::startTick`，以 tick 判。S8（測試缺口）→ 開始前每一幀都沒送過、Client 預測也不動、開始前不能開槍、準備好但還沒開始時仍蓋著、沒有自己的玩家不會 `ready`；突變 `pvp-start-fires-before`、`pvp-loading-no-wait-start`、`pvp-loading-skip-network`。宣告列的「`pvp-loading-arena-follow`」（資產一步鎖住）在本機載入快過握手時分不出，留給上面的案與審查。S9（loading 期間點擊拿走指標）→ loading 期間點擊與 Tab 不拿。
- nit：N1 具名常數；N2 期限預設值一處；N3 Go 註解位置；N6 等待的使用者去重；N8「対戦に接続しています…」；N9 註解記下「開始時整個拿掉 `PeerVisibility`，別的相關性規則要加入」。不改並記下：N4、N5（進度條可聚焦：宣告修訂 4）、N7（多人開始前中止，Match 等到期限：照位子保留）、N10（CI 上多人「撤退する」由共用的分支守）、N11（HUD 與 overlay 在同一台相機上的先後：實機看）、N12（就緒放在輸入：宣告修訂 4、net 的後續）。
- 審查後：world 28/28、client 23/23；突變第 4 段 9/9 killed。

## 沒驗

- 真的 GPU 上的預熱（`DrawnFrames` 從 `SDLGPUStatus` 來的那條）、畫面上 loading 蓋住的樣子與進度條：交使用者試玩。

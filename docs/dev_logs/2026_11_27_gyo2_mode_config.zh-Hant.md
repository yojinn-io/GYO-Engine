# 2026-11-27 模式配置（`v8-arch-gyo2-mode-config`）

Owner：模式配置的任務卡。分支 `claude/gyo2-mode-config-declaration`（PR #367，開向 `gyo2`）。作者 Opus 5.5／high。宣告：[mode-config-declaration.zh-Hant.md](../architecture/target/v2/mode-config-declaration.zh-Hant.md)，結果寫在第 11 節。

## 前提（使用者的決定，經主會話轉達）

1. 「全部照推薦，空殼刪掉」：M1 (a)，卡片自判的六項沒有推翻（HANDOFF `v8-arch-gyo2-mode-config-m1`）。
2. 「開始」：宣告算動工前核准，照第 7 節分批實作（HANDOFF `v8-arch-gyo2-mode-config-start-impl`）。

## 各步

- **第 0 步**（`6be715a6`）：核心 PvP 腳本程式 `gyo_object_fps_pvp_core_script`（`tests/object_fps_pvp/script/`）。四段（Match 直接操作、落下、Match＋2 Client 的記憶體連線、Local），每幀印每個 World 的權威 digest；`--report` 印 net 的登記與決策資料集，`--check` 確認走過頭、軀幹、腿的命中、擊殺、重生、換彈、落下死、著地傷害、Client 的走路與命中。ctest `object_fps_pvp.core_script`（cpu）跑 `--check`。A/B 腳本在主 checkout `build/evidence/gyo2-mode-config/ab.py`（Python，跨平台；基準樹放 `trees/`、重用）。
- **第 1、3 步**（`b8082841`；兩步改到同一批檔案，合成一個 commit）：
  - `assets/object_fps_pvp/modes/`（`modes.json`、`core_pvp.json`）；`world/Modes.hpp`：讀取、錯誤（不認得的規則種類／組／arena、沒有 movement、id 重複、規則檔不在 content 內）、`RulesDigest`（解析後的值經 Kernel 的 Hasher：模式 id、組與其設定、arenas、movement、`CombatRules`、命中形狀與 scale／anchor）。
  - `MatchInfo` 加 `modeId`、`rulesDigest`（Codec 2 → 3）；`FollowMatchArena` 先比模式、再比 digest、再比 arena 與牆；`ContentMismatch::matchMode`。Client 有那個模式（且它玩那個 arena）就重建，同 id 不同 digest 回大廳。Match `--mode`；`SceneChange::mode`；`ClientOptions::mode`（放在最後，避免既有的位置初始化錯位）。
  - M1 (a)：房間服務的模式清單照地圖讀 Match 的 content（`--content`，或 `--modes`；都沒有：任何 id），matches 起 Match 帶 `--mode`；大廳在已安裝模式之間切換、顯示模式檔的 `name`；單人用清單第一個；房間開始帶房間的模式。`coop`／`survival` 從程式、測試、game-flow 宣告（修訂 8）與 Gateway 交接文件拿掉。
- **第 2 步**（`ab6dede5`）：`world/GameplaySets.hpp`：組的表（名字、登記函式、模式設定與 arena 區塊的讀取函式）、`SetData`（digest＋放進 World 的函式）、條件 `pvp.mode.<組>`（slot 當模板參數，最多 32 組）、Kernel set `pvp.<組>.fixed`／`.frame`；`ActiveMode` 移到這裡、帶 `on`。所有組在每個模式都登記、排在核心之後；開著的組才放設定與 arena 區塊。遊戲的表目前是空的。
- **第 3 步的另一件**（`797471c1`）：`object_fps_pvp.client` 拆成 `object_fps_pvp.client.1`～`.4`（doctest `--first`／`--last`，最後一份不設上限），macOS 牆鐘 42.5 s → 17.5 s。

## 檢驗

- 既有測試不改期望值。改的只有三類：`GameContent::stance` 搬到模式（一處引用）、單人房間的模式文字（M1 改了行為）、測試資料裡的 `coop`／`survival` 換成 `core_pvp`。
- 新測試：world 的模式 4 案、組 4 案、MatchInfo 1 案；client 的模式重建 1 案、單人與大廳的模式各加檢查；Go 的已安裝模式 1 案；Match `--mode` 不存在的 ctest（cpu）。
- 本機（macOS arm64）：`object_fps_pvp.*` 的 cpu 與 network 共 12 項全過（含真的 Client 經 Gateway 的 `gateway_client`）；Go `./services/...` 全過。
- A/B（macOS）：第 1 步、第 2 步後各跑一次，權威 digest 1257 行逐位元組相同；報告的差異只有 `MatchInfo` 的 Codec 版本與因此變的 protocol hash，決策資料集（指紋的文字）相同。
- 突變（D184：新的與 find 在改動範圍的）：新增 23 個（模式 15、組 6、digest 涵蓋 2；退役 `pvp-solo-mode-not-survival`），find 過時的 5 個照新程式改寫，加上鄰近的既有 5 個，全部 killed。途中修了兩件：`refused` 的訊息沒印出期待文字（測試有失敗、訊息對不上）；matches 的 Local 測試原本不看假 Match 的抱怨（參數錯也通過，既有的缺口），改成收輸出並檢查。
- `組排到核心登記之前`：由測試 `[pvp-set-after-core]`（決策型別的登記序號）守，沒有突變（移動一段程式不是 find／replace 能表達的）。

## 設計審查與純淨 Code Review

- 設計審查（Opus／high，唯讀，只給宣告、批次表的五項、AGENTS §1、Bevy）：blocker 0、should 5、nit 7。純淨 Code Review（Fable／high，只給 git 範圍）：14 條。兩份在主 checkout `build/evidence/gyo2-mode-config/`。
- 兩份都指出的四件，全部採用：arena 檔的組區塊沒進比對值（`GameContent::MatchRulesDigest`）、「模式玩不玩這個 arena」分散（`GameContent::Plays` 一個出口，大廳與單人按模式過濾地圖）、`ContentMismatch` 只有文字（`MismatchKind`，Client 只在 Mode／Arena 時重建）、digest 逐欄手寫（每個型別一個 `HashValue`，值不變）。
- 另採用：組的設定每端各自放、不複製（probe 改成單一來源，複製改由只在 Match 寫的 `ProbeTally` 驗）；模式 id 1～64；`FollowMatchContent` 改名；測試名；`ModeFolder` 收成一份；超過 32 組的檢查移到 `LoadModes`；拿掉 `modes.empty()` 與 `arenaSets` 的防禦。
- 宣告 1.5「沒有衝突點」更正：Kernel 的條件不能捕獲（Bevy 能），slot 表是膠水。
- 不採與部分採用的理由、要使用者決定的三點（U1 骨架與動畫進不進比對值、U2 Kernel 條件要不要能捕獲、U3 房間服務要不要查組合）：宣告 11.5、11.6。
- 處置後：`object_fps_pvp.*` 15 項全過、Go 全過、A/B 仍逐位元組相同；突變再加 4 個，和模式有關的 30 個重跑。

## L1

- 第一次 dispatch（run 45971988646，head `797471c1`）：windows-x64 過；linux-x64 有 1 項失敗：`object_fps_pvp.client.3` 的 `client: single player: the room, a game without a server...` 在 `[pvp-solo-playing]`（只等 60 幀讓地圖讀完）。本機重現不了（單跑、同時 12 份與 40 份都過）。判斷：拆成 4 段之後，CI 上 4 個 client 程序和其他測試並行、地圖檔第一次讀，資產執行緒慢於 60 幀。改成和 `Setup::Loaded` 一樣的等法（最多 2000 幀、每幀 1 ms），斷言不動。**沒有重現就改**，記為未確認的原因。

## 待補

（Linux 的 A/B 與 S5、L1 的結果。）

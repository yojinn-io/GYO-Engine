# 架構 v8：v8-arch-h7-scanner（H-7 呼叫點掃描器）

件数の突き合わせ：一致（五個 owner 各三條：(a) 發現數＝H7-a＋H7-b、(b) 掃描檔＝清單＋展開標頭、(c) `git grep -nP` 的獨立計數逐行相等；見「件数の突き合わせ」節）。

日期：2026-11-13。Owner：架構（架構師會話開出的任務卡）。計畫：遷移計畫 [migration-plan.zh-Hant.md](../architecture/plans/kernel/migration-plan.zh-Hant.md) §3.4；事前宣告 [h7-scanner-declaration.zh-Hant.md](../architecture/plans/kernel/h7-scanner-declaration.zh-Hant.md)（架構師 2026-11-13 核定，附 5 點修訂）。
- 執行：本卡會話（開卡時 Fable，使用者為節省 Fable 用量改 Opus 5.5；依卡：實作 Opus／high、型別近似 xhigh；審查與突變一覽的敵對檢查合成一次 Fable／high 只讀）。工作樹 `.claude/worktrees/dreamy-mcnulty-c2ba1b`，分支 `claude/arch-h7-scanner`（自 origin/master `899b3e38`）。
- 規格出處：spec H-7、H.7、J.4；`target/kernel.zh-Hant.md` DI-KS 15（已遷移的 host 只呼叫 `Run`、`Rewind`／`Replay` 與交換點）。

## 起因

H-7 要求 host 不排 System 順序。只掃 include 會漏掉「不 include `RuntimeLoop` 卻手排模組更新」的 host（`MatchRuntimeHost` 的 `match_.Tick`、`ClientSimulationRole` 的 `loop_.Run`），所以 spec v0.3 加了呼叫點規則，白名單依型別。這是 match-host、client-frame 與工具四批的歸零目標：沒有基線，那幾批的 H-7 判定只剩審查。

## 做了什麼

- 共通層 `tests/common/h7_scan/`：`h7_scan.py`（掃描器；格式 `gyo.h7-hosts` v1 的契約寫在 docstring）、`h7_scan_rules.json`（H7-a 標頭名、H7-b 方法名、白名單、剝殼清單）、`test_h7_scan.py`（合成自測）、`mutations.json`＋`run_mutations.py`（包裝 zone_scan 的執行器，不複製）、`CMakeLists.txt`（`h7_scan.selftest`、`h7_scan.empty`）。`tests/common/CMakeLists.txt` +1 行。
- 遮罩、引號 include 解析、路徑檢查 import `tests/common/zone_scan/zone_scan.py` 的三個函式；zone_scan 缺失或缺名時結束碼 2（自測兩條）；`tests/common/zone_scan/` 零改動。
- 五個 owner：`tests/<owner>/h7-hosts.json`（18 檔）與各自 CMake 的 `<owner>.h7_scan`（`--no-fail`，只報告）；object_fps_pvp 以 `--expect-finding` 釘住計畫判定 (6) 的兩點。
- 不改 `apps/`、`tools/`、`engine/`、`framework/`、spec。

## 受理判定（宣告 §5；看結果後不緩）

| # | 判定 | 結果 |
|---|---|---|
| 1 | 合成自測 | **成立**：124 條全綠（審查前 48、審查後補 72、三個動詞補 4）；宣告的正例 13（`test_p01`～`test_p13`）、反例 11（`test_n01`～`test_n11`）逐一對應，另有三個動詞的正例 `test_p12b`～`test_p12d` 與反例 `test_n04b`。Python 3.14 與 3.9.6 都實跑通過 |
| 2 | 突變 | **成立**：最終 **104/104 killed**（宣告下限 16；審查前 22/22，敵對檢查後擴為 98，第二輪 95/98、補強後第三輪 98/98；架構師加三個動詞後加 6 條＝104）。敵對檢查者 Fable，找到 64 處，加入後的檢出 98/98：見「審查與敵對檢查」節 |
| 3 | 掃描方法 | **成立**：Python `-I -B`；非 UTF-8 與讀檔失敗結束碼 2（自測）；獨立計數用 `git grep -nP`，先以 `MatchRuntimeHost.cpp:441` 正例命中、結束碼 0／1 以外視為失敗、stderr 不丟 |
| 4 | 共通層、無 owner | **成立**：`h7_scan.empty` passed（`PASS_REGULAR_EXPRESSION`）；`tests/common/` 既有檔只有 `tests/common/CMakeLists.txt` +1 行；`apps/`、`tools/`、`engine/`、`framework/` 0 變更；owner 的既有檔只動各自 `CMakeLists.txt` |
| 5 | Removability | **成立**：自測 `test_removing_one_owner_changes_nothing_else`（合成 owner alpha／beta，刪 alpha 後 beta 報告逐欄相同、共通層位元組不變、`nothing to scan` 仍綠）；實際建置只選 voxel_map_editor（`GYO_APPS=`、`GYO_TOOLS=voxel_map_editor`）時 `ctest -R h7_scan` 3/3 passed |
| 6 | 五個 owner 的基線 | **成立**：本機 7 個 h7 ctest passed；pvp 兩個 `--expect-finding` 在報告中；`MatchRuntimeHost.cpp:385` 的 `ticker_.Advance` 在 `whitelisted`。基線見下表 |
| 7 | 件数の突き合わせ | **一致**（下節） |
| 8 | 三平台 | 待 L1（PR 後補 run id） |

## 基線（只報告；歸零目標）

| owner | 清單 N | 掃描 M | H7-a | H7-b | 白名單放行 | 發現 |
|---|---|---|---|---|---|---|
| object_fps_pvp | 8 | 65 | 2 | 7 | 1 | `PvpApplication.hpp:5`（經 `PvpApplication.cpp`）與 `PvpApplication.cpp:34` 的 include；`main.cpp:167` `application.Run`、`ClientSimulation.cpp:65` `prediction_.Advance`、`ClientSimulationRole.cpp:196` `loop_.Run`、`MatchRuntimeHost.cpp:441` `match_.Tick`、`PvpApplication.cpp:1142` `assets.Update`、`:1193` `ui.Update`、`:1403` `loop.Run` |
| voxel_map_editor | 3 | 13 | 0 | 0 | 0 | 無（經 framework 間接使用，限界 L1） |
| model_viewer | 3 | 16 | 0 | 3 | 0 | `PreviewMeasure.cpp:113`、`ViewerWindow.cpp:125` 的 `clock.Advance`（`PlaybackClock`）、`ViewerWindow.cpp:426` `playback_.Update` |
| audio_editor | 2 | 12 | 0 | 1 | 0 | `AudioEditorWindow.cpp:117` `player_->Update`（`std::optional<PreviewPlayer>`） |
| ui_editor | 2 | 15 | 0 | 0 | 0 | 無（同 L1） |

合計 H7-a 2、H7-b 11。voxel 與 ui_editor 為 0 不表示合規：兩者經 framework 的 shell 間接用 `RuntimeLoop`，host 檔本身不 include 也不呼叫（宣告限界 L1，由 framework 退役的批處理）。

## 件数の突き合わせ

| owner | N | M | n(H7-a) | n(H7-b) | W | this | grep 呼叫 C | C −（H7-b＋W＋this） | grep include | 判定 |
|---|---|---|---|---|---|---|---|---|---|---|
| object_fps_pvp | 8 | 65 | 2 | 7 | 1 | 0 | 8 | 0 | 2 | 一致 |
| voxel_map_editor | 3 | 13 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 一致 |
| model_viewer | 3 | 16 | 0 | 3 | 0 | 0 | 3 | 0 | 0 | 一致 |
| audio_editor | 2 | 12 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 一致 |
| ui_editor | 2 | 15 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 一致 |

逐行集合比對（掃描器的「發現＋白名單＋this」與 grep 的行）差集為空，遮罩差 0 行。

## 經緯

1. 送 parent 時 `send_message` 回「session parent not found」（任務卡第三次），改直送「[Job][架構師]例行架構掃描」，同日核定。修訂：命名空間 `GYO::Kernel`（暫定 `GYO::Kernel::Runtime`，naming 第 5 條使最終名一定不同，定名批改資料一行）、pvp 清單加 `ClientSimulation.cpp`（P5「Host 的時鐘」）、審查與敵對檢查合成一次 Fable、zone_scan 缺失的自測。
2. 白名單比對比宣告 §2 第 3 步 (ii) **更嚴**：宣告寫「帶至少一段限定的尾段即可」，實作改為尾段也要有對應的 `using namespace <其餘命名空間>;` 才放行（`Runtime::FixedTickRuntime` 沒有 `using namespace Engine;` 就報；自測 `test_partial_qualification_without_using_is_reported`）。理由：產品若在自己的命名空間定義 `Runtime::FixedTickRuntime` 或 `Kernel::Runtime`，原寫法會誤放（fail open）。只締不緩。
3. 實掃時兩處型別描述帶多個型別：`match_` 解析出 `Hold,PvpMatch`（`NetworkStatistics.hpp:138` 另一個類別的同名成員）、`ui` 解析為 `?`（同檔 `const auto ui = …` 的區域變數）。都是「同名宣告不分作用域、全部相符才放行」的保守結果（限界 L4），照樣報出、不誤放。
4. 第一次設定 `-DGYO_TOOLS=a,b,c,d` 失敗（工具選擇不接受逗號清單）；改用預設 `AUTO`（四工具全選）。

## 審查與敵對檢查（Fable／high，只讀，一次；架構師修訂 1）

### 審查：blocker 1／should 8／nit 5，全部處置（只收緊，沒有放寬任何判定）

| 項 | 內容 | 處置 |
|---|---|---|
| B-1 | 宣告「所有不確定都往多報；漏報只有 L1、L3、L4」不成立：(a) 接收者的真正宣告看不到、閉包內另有同名的白名單宣告就放行（多宣告子的第二個、range-for、structured binding、lambda 捕獲、標頭裡函式原型的參數、繼承自閉包外的基底）；(b) `using` 證據不分作用域、可來自展開標頭 | 照審查者第 2 案收緊：接收者名出現在掃描讀不了型別的宣告位置（逗號後、單冒號前、方括號內）一律報 `?`；參數形宣告（以 `,`／`)` 結尾）只認清單檔本身；`using` 證據只認清單檔本身。剩下的漏報（同名白名單成員在閉包內、真正宣告在閉包外或是掃描讀不了的形）改寫成限界 L4，並以 `test_known_gap_same_name_whitelisted_member_elsewhere` 釘住（行為變了就會被看到）。L4 的條文改寫要報架構師（宣告 §7 已核定的字樣） |
| S-1 | f-string 運算式內有反斜線，Python < 3.12 直接 SyntaxError | 改成 `spaced()` 輔助函式；`/usr/bin/python3`（3.9.6）解析通過 |
| S-2 | 限界 L5 說「owner 把標頭列進清單」，但清單拒收標頭 | 清單接受標頭副檔名；自測 `test_listed_header_is_scanned_for_h7b` |
| S-3 | H7-a 的漏報形：BOM、註解夾在記號間、角括號含入的 owner 標頭不展開 | 讀檔用 `utf-8-sig`；include 目標改從遮罩後的指令位置往原文讀（註解夾在中間也讀得到）；角括號 include 以 include_roots 解析並展開。`#include MACRO`、`#include_next`、`#import`、反斜線續行寫進 L3 |
| S-4 | H7-b 的漏報形：`Product{}.Run()`、`x.Base::Tick()`、`x.template Run<int>()`、含括號或三層的模板引數、運算子前後換行 | 接收者加 `}`（型別 `?`）；限定的成員呼叫一律報；`template` 關鍵字與模板引數放寬比對；各一條自測 |
| S-5 | 清單或規則型別錯、報告寫不出時以 traceback 結束碼 1 結束，和「缺釘住的發現」同碼 | 型別檢查進契約；報告寫入失敗結束碼 2；自測 24 條（清單、規則、讀檔、命令列） |
| S-6 | `auto` 一跳接受 `s.ticks_` 這種成員鏈，比宣告寬 | 拿掉，只追裸識別字；自測 `test_auto_copy_of_member_chain_is_reported` |
| S-7 | 反例 `test_n03`、`test_n04` 是空測（`Rewind`／`Replay` 不在 H7b 方法名單，永遠不是候選）；`test_n08` 的 include 半邊不測遮罩 | n03／n04 改成明寫「不是候選」並斷言 `whitelisted` 為空（見下方「給架構師的觀察」）；n08 加區塊註解裡的 `#include` |
| S-8 | ctest 沒帶 `-I` | 共通兩個與五個 owner 的 `add_test` 都加 `-I -B` |
| N-1 | 白名單尾段比對比宣告嚴 | 記在「與計畫／規格的差異」 |
| N-2 | `--report` 形狀與宣告 §3 不同（外包 `reports`、型別在 text 的 `[type=…]`、多 `this_calls`、`list_path`） | 記在差異；docstring 寫實際形狀 |
| N-3 | 多餘的 `(?<!->)` | 刪 |
| N-4 | `constEngine::…` 與 `decltype(k)::Runtime` 會放行 | 剝 cv 只剝獨立的字；型別起點前不可是 `)`；自測 `test_glued_const_is_not_peeled` |
| N-5 | `std::optional<FIXED*>` 會放行 | 剝殼不再去尾端 `*&`；自測 `test_pointer_inside_wrapper_is_reported` |

### 敵對檢查（AGENTS §14.2）：找到 64 處「改壞而自測仍綠」

- 檢查者 Fable；一覽作者 Opus。審查者在暫存複本實測，每處另跑一個差異輸入確認不是等價突變。分層：漏報方向 23、契約與結束碼 20、誤報方向與報告欄位 21；另有試過不計入 9（等價或只對編不過的碼有差）。
- 處置：一覽擴為 **98** 條（原 22 條重新對位到修正後的程式＋新增 76 條：審查者第 1 層 23 條全加、第 2 層 20 條全加、第 3 層加 14 條，另加收緊後的新路徑）。每條的 `expect` 是一個測試名（只有 `report-scanned-count` 用兩個測試名的正規式）。
- 結果：第二輪 **95/98** killed，存活 3：`hard-alias-untyped-dropped`（自測輸入在更早的檢查就被擋下）、`hard-call-result-allowed`（另一行測試碼產生的非白名單假宣告掩蓋了它）、`expansion-cycle`（突變在深度 50 自停、不是無窮迴圈）。前兩者補自測、第三者把突變改成真的拿掉防重入檢查、自測改用 30 秒逾時的子行程。第三輪 **98/98 killed**。
- 不另加的：第 3 層 `a25f`（摘要的展開數算法，與 `report-scanned-count` 同一路徑、同一自測已殺）；試過不計入的 9 處照審查者判斷（等價或規格未定）。

### 給架構師的觀察與處置

- 觀察：spec H-7 的 (b) 禁止名單是 `Tick`／`Advance`／`Update`／`Step`／`Run`，白名單卻寫 Kernel 的 `Run`／`Rewind`／`Replay`；`Rewind`／`Replay` 不在禁止名單，所以白名單那兩項沒有作用。
- **架構師 2026-11-13 處置（締める面，在 #207 內做）**：`H7b.methods` 加 `Rewind`、`Replay`、`Restore`（target/kernel DI-KS 12 的「Rewind／Restore／Replay 只由 Host 在 Ready 呼叫」指 Kernel 的那一組；host 對產品自己的型別呼叫同名動詞，就是在 Kernel 之外手寫回滾）；Kernel 白名單的 methods 同步為 `Run`、`Rewind`、`Replay`、`Restore`。自測加正例 3（產品型別上的三個動詞被報：`test_p12b`～`test_p12d`）、反例 n03／n04 恢復為 Kernel 型別上不報並加 `test_n04b`（`Restore`）；突變加 6（刪三個動詞各一、Kernel 白名單少三個動詞各一）＝ **104** 條。五個 owner 的基線重數：**不變**（host 清單裡沒有這三個動詞的呼叫；`LocalPlayerPrediction::Reconcile` 的回滾不在 host 清單）。件数の突き合わせ的獨立 grep 樣式加這三個詞，仍一致。
- 限界 L4 的改寫：架構師同意（如實揭露、不是放寬）。

## 與計畫／規格的差異

- 掃描器放 `tests/common/h7_scan/`，spec H-7 寫 `plans/kernel/scripts/h7_scan.py`；spec 的字樣由架構師下次碰 spec 時改（Q5）。
- H7-b 禁止名單比 spec H-7 多 `Rewind`、`Replay`、`Restore`（架構師 2026-11-13 決定；spec 的字樣由架構師下次碰 spec 時補）。
- 限界 L4：宣告 §7 的原字樣「同名宣告不分作用域，只有兩個白名單型別才會誤放」→ 新字樣「真正宣告在閉包外或讀不了的形、閉包內恰有同名白名單成員時放行」；原因是審查 B-1 收緊後剩下的形（架構師同意）。
- owner 清單不帶白名單（計畫 §3.4 字面「傳自己的檔案清單與白名單」）；白名單只有共通規則資料一份，比計畫字面更嚴（Q2）。
- 白名單比對比宣告更嚴（經緯 2）；審查後再收緊（未解析的宣告位置、參數形宣告與 `using` 證據只認清單檔、限定的成員呼叫一律報），限界 L4 的條文隨之改寫（待架構師確認）。
- 清單接受標頭（宣告 §3 寫只收來源檔；L5 的補救要靠它）。
- `--report` 的形狀：外包 `{"reports": [...], "per_rule"}`、型別寫在 H7-b 的 text `[type=…]`、多 `this_calls` 與 `list_path`（宣告 §3 的寫法較簡）。
- ctest 以 `-I -B` 執行（與宣告判定 1、3 一致；審查 S-8 前只有 `-B`）。

## 守衛與突變的去處

既有守衛：無。新增：`h7_scan` 掃描器、規則資料、合成自測（target 側，DI-KS 15 的守衛）；五個 owner 的 `<owner>.h7_scan`（產品守衛，owner 持有）。突變一覽的敵對檢查者 Fable／high。

## 範囲の限界

宣告 §7 的 L1～L7，其中 L3、L4、L5 照審查改寫（L4 待架構師確認）：

- L1 經 framework 的間接使用：抓不到。
- L2 型別近似：宣告找不到、二跳以上、`decltype`、命名空間別名、`typedef` 改名，一律報（多報）。
- L3 不掃的形：無 receiver 的呼叫、限定名的自由呼叫（`Foo::Run(`）、函式指標與 `std::invoke`、巨集展開、運算子與方法名之間的反斜線續行、`#include MACRO`、`#include_next`、`#import`、反斜線續行的 include 指令。
- **L4（改寫）已知漏報**：接收者的真正宣告在閉包外（例：繼承自別的 owner 的基底）或是掃描讀不了的形（`std::function` 成員、巨集宣告、回傳白名單型別的函式），而閉包內恰有同名的白名單變數或成員時放行。自測 `test_known_gap_same_name_whitelisted_member_elsewhere` 釘住此行為。
- L5 標頭裡的 host 程式：owner 把標頭列進清單（清單已接受標頭）。
- L6 手寫清單。
- L7 Kernel 條目是暫定名。

## 證據

主 checkout `build/evidence/h7-scanner/`（git 管理外，本機 macOS）：`reconcile.py`、`reconcile.txt`、`reconcile-detail.json`、五個 owner 的 `*.report.json`／`*.stdout.txt`、`mutations-round1.json`、`removability-configure.log`、`removability-ctest.txt`、`adversarial-survivors.json`（審查者的 64 處存活一覽）、`mutations-round2.{json,txt}`、`mutations-round3.{json,txt}`、`mutations-round4.{json,txt}`。

## 下一步

- L1 三平台（判定 8）；合併由使用者決定。
- match-host 批拿掉 `match_.Tick` 的 pin、client-sim／client-frame 批拿掉 `loop_.Run` 的 pin；Kernel 定名批改白名單一行。

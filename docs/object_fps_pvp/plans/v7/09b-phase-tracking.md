# 第 09b 批：相位追蹤的既有缺陷與延遲位移

狀態：**規劃完成（草案），待使用者確認「需要決定的事」**（2026-10-10）。PR 線 P2（PR #73）。排在第 09a 批之後、第 09 批的 M0 與常數凍結之前（D47⑤ 的更正）。
檔位：規劃 ultracode（使用者 2026-10-10「09b 的規劃開 ultracode，檔位照建議」）：方案 3 位 high（缺陷、延遲位移、交互與風險）、評審 high、對抗式檢查 xhigh。

這份草案依使用者的要求（memory「decision points for learning」），每一項都寫出機制、為什麼重要、選項與代價、決定它的證據在哪裡，以及途中被推翻的說法。

## 先讀這一段：最重要的發現在 09a，不在 09b

- **機制**：Client 相位追蹤的 late 修正，在遲到來自傳輸路徑時會被夾在 2 Tick（−33.3 ms，`LocalPlayerPrediction.cpp:97-98`、`Movement.hpp:59`），命令領先從 2 變成 4。v7 的穩態每 Tick 排隊 2 個命令，30 Tick 合計 60（v6 是每 Tick 1 個、合計 30），所以 backlog 門檻 105 的餘裕只有 45（v6 是 75）。領先 4 時約 23 Tick 後合計 ≥105，epoch 重設，這就是 D44 的推論鏈。
- **新的證據**（對抗式檢查重新分類 `defects/late-outcomes.jsonl`）：真正的 late 修正有 5 次（另外 4 筆是停頓 reseed 之後的首次決定，原本被誤算進來）。送出被釘住的 4 次，30 Tick 合計停在 89～99，沒有重設；沒被釘住的 1 次就是 D44（合計 108，Tick 622 重設）。被釘住時命令晚送 3～10 ms，吃掉了一部分多出來的領先。
- **為什麼重要**：第 09a 批要修的正是「送出被釘住」。修好之後，這個「意外的緩衝」就沒了，傳輸型的 late 修正很可能每次都變成 backlog 重設。這符合 D44 的提前條件（`09-firegate-c2.md`：「決定 1 選了修正，而修正確認會改變遲到修正或 backlog 的行為」）。樣本只有 5 次，結論是方向一致的小樣本，不是證明。
- **所以 09a 的實作先停下**，等你決定 D44 要不要提前（見「需要決定的事」1）。
- **證據位置**：`build/target/_build/test/logs/pvp-v7-09b-plan-20261010/adversarial/step_context*.txt`、`queue_recount.txt`；`defects/late-outcomes.jsonl`（原始 9 列）。D44 的完整 trace 在 `pvp-v7-batch08-20261009/dev-backpressure/host-ipc-250ms/`。

## D48 第①步：既有證據的普查（2026-10-10，只讀）

分析 1 位（high）＋對抗式核對 1 位（high）。證據：`build/target/_build/test/logs/pvp-v7-d48-step1-20261010/`（偵測器 `detector.py` 在普查前凍結，sha256 `445271ce…`；核對在 `adversarial/`）。

**結論：既有證據已經用盡，「被釘住擋住 D44」的 4 對 1 無法檢定；但它背後的機制有了更精確、可以被推翻的形式。**

1. **樣本沒有擴大**：498 個有 trace 的 v7 跑次中，tracking 型、約 −33 ms 的 late 修正只有 09b 已知的那 5 筆，而且 M1／M2 是同一跑次、同一次主機停頓，實際只有 4 個獨立事件。閒置主機的 111 個跑次（3,550 玩家·秒）是 0 筆。另有 23 個 `run_network` 跑次沒有命令 trace，其中包括 D44 的第 1 次（08 `dev-network`），無法分類。
2. **「被釘住」用錯了變數**（被推翻的說法）：09b 的 4 對 1 用的是修正**之後**的送出等待。依 D48 凍結的定義（修正**前** 1 秒的中位數 >2 ms），5 筆都沒有被釘住，2×2 表的「被釘住」那一列是空的。而修正後的等待是 slew 本身造成的：slew 期間命令以 75 Hz 產生、token 以 60 Hz 補充，等待每個命令增加約 3.3 ms，到約 16.7 ms 繞回；slew 結束時的值就凍結下來（09a 的中性穩定）。所以「修正後被釘住」不是獨立的暴露變數。
3. **真正的變數是到達 Match 的領先**：Match 端領先（resolved − host_accepted）≈ 修正前的領先＋33.3 − W_post（W_post＝slew 結束時凍結的送出等待）。它超過約 66.7 ms（4 Tick）時，每 Tick 排隊 4 個，約 25 Tick 內 30 Tick 合計 ≥105。5 筆和這條邊界單調一致：M1／M2 57.6／57.8 ms→合計 90；M5 60.7→97；M4 63.5→66.1→89（trace 1.24 秒後結束，右設限）；M3（D44）69.7→108、重設。模型對 M3 的預測 69.7、實測 69.7，其餘差 1～3 ms。
4. **M5 不是乾淨的反例**（被推翻的說法）：分析原本把 M5（network20，修正後等待 4.38 ms 仍然重設）當成反例；核對發現它的重設發生在 network20 故障解除後 246 ms：單程延遲約 13 ms 消失，Match 端領先從約 60 跳到 73 ms。是「殘留的領先＋外部觸發」的複合事件。
5. **網路延遲下降或釘住解除，也會把領先推高 16～24 ms**（network40 的故障解除、08b L2 gateway-250ms r2 的釘住解除，都在沒有 late 修正時讓合計到 90）。v7 的 backlog 餘裕 45 只容得下約 29 ms 的到達提前。
6. **5 筆在修正前 1 秒內都有 27～76 ms 的產生空檔**（這類間隔在 v7 只占 0.006～0.17％），所以它們也無法支持「只有傳輸型遲到才夾在 2 Tick」。M1／M2 的修正量也被輔助偵測器高估，實際約 32.0／30.6 ms，仍是 2 Tick 的上限。
7. **對 09a 的意涵**（仍是假說）：09a 會讓 W_post 在 4 個命令內歸零。若上面的模型成立，之後每次 tracking 型 late 修正都會越過 66.7 ms。現在的頭若 W_post 在 0～16.7 ms 近似均勻，越線機率約 20～25％，實測 1／5。W_post 的分布只有 5 個值，修正當下的 token 水位也沒有記錄在 trace 裡，這兩點都沒有驗證。
8. **檢定力**：(4,1) 的 Fisher 雙尾最小 p 是 0.2，任何效果都無法在 α＝0.05 下檢出。比例比較（第③步）每棵 tree 需要約 8 次（若真實是 0.25 對 1.0）到 18 次（0.25 對 0.75）late 修正；自然發生在閒置主機是 0，只能靠注入。用連續量（W_post、Match 端領先）做預測檢查，比 2×2 有效。
9. **事後的成分**（如實記錄）：5 筆在凍結前都已知；延伸觀察視窗是在已知 network20 的時間線之後設計的；MAIN 的規則（tracking 且 ≤−20 ms）寫在普查之後。凍結時的表不受這兩點影響。

**第②步若要做，必須量的東西**（核對的建議）：注入要能穩定產生 tracking 型 −33 ms 的 late 修正，並記錄注入有沒有造成產生空檔；每次修正記錄 W_pre（1 秒與 0.25 秒）、slew 期間每個命令的等待與 W_post、修正前後的 Match 端領先、每 Tick 的排隊與 30 Tick 合計的軌跡；修正後 2～3 秒內不能有排程中的故障解除或延遲改變，並標記右設限；事前宣告可被推翻的預測：「重設 ⇔ Match 端領先 >66.7 ms ⇔ W_post 小於（修正前領先 − 33.4），約 3.6～3.9 ms」。

## D48 第②步：試跑時碰到停止條件（2026-10-10）

工具與試跑 1 位（high）。證據：`build/target/_build/test/logs/pvp-v7-d48-step2-dev-20261010/`（12 次開發跑次全部保留，沒有重跑；主機沒有閒置閘門，只供開發）。新的 runner `build/acceptance/object_fps_pvp/run_short_stall.py` 與它的測試還沒有 commit；凍結檔沒有改動（S1 的 `analyzers.sha256` 試跑前後都吻合）。

**停止的理由：原本的注入（relay 擋住 client→Gateway 的封包 30～150 ms 再放出）在現在的頭，機制上就產生不了「成對遲到 → late 修正」。** 試跑 0／5 個跑次（每位玩家擋下 1～7 個命令，都落在同一次發布裡）。

- **機制**（主對話已核對程式）：
  1. Gateway 收到 Match 的 snapshot 就更新 `lastResolved`（`gateway/server.go:519-531`），之後封包裡 ≤`lastResolved` 的命令直接略過，不轉給 Match（`:420-422`）。所以 client→Gateway 路徑上晚到的命令，只有落在「Match 替代它」到「Gateway 處理那個 snapshot」之間的那一小段才會到 Match、產生負的 slack 樣本；晚得更多的會被 Gateway 吞掉，連樣本都沒有。
  2. 擋住再放出的封包只會落在一次發布裡，而 late 修正需要連續 2 次發布都帶負樣本（`MovementPhaseLateSamples`＝2，`Movement.hpp:58`、`LocalPlayerPrediction.cpp:134`）。即使改成「連續 4 個命令各晚 12～15 ms、分在 4 次發布」（原型 1），仍然 0 次，因為被 Gateway 略過。
  3. 第①步的 M1～M5 都帶有產生空檔，和這兩點不矛盾。推論（未驗證）：host-ipc 跑次的 relay 讓 Match→Gateway 的 snapshot 變慢，拉長了那一小段，這可能是 D44（M3）出現在 host-ipc 跑次的原因。
- **意涵**（未結，要寫進 D44）：client 端的網路遲到幾乎不會觸發 late 修正；真正的觸發來源是伺服器端的輸入延遲（Gateway→Match）或邊界巧合。
- **新的注入可以重現**：在 Gateway→Match 的 IPC 輸入路徑加延遲（原型 2，δ 40～50 ms、視窗 100 ms），tracking 型、−33.3 ms 的 late 修正 8／8 事件；故障前後沒有 >20 ms 的產生空檔、沒有 runtime_gap；注入在 slew 結束前就結束。模型預覽：8 個單次修正的 Match 端領先，實測與預測（修正前＋33.33−W_post）相差 ≤0.3 ms，全部沒有重設（領先 58.6～64.3 ms）。
- **但脆弱窗沒有碰到**：W_post 6.5～12.0 ms，0／4 個跑次落在約 <3.6～3.9 ms。同一跑次兩位玩家的 W_post 相差 ≤0.2 ms，所以獨立單位是跑次。經驗擬合（不是推導）：W_post ≈ (26.8−16.667·x_before) mod 16.667 ms，x_before 約 1.39～1.60 時才會碰到脆弱窗；既有現頭跑次估計每個跑次的機率約 0.24。
- **第二條重設途徑**（1 個跑次，δ 80 ms、視窗 150 ms）：兩次 late 修正串接（缺陷 B），共 −66.7 ms，Match 端領先約 94 ms，兩位玩家都 backlog 重設。不管 W_post 多少都會重設，09a 管不到。
- **量測腳本**（草稿，未凍結）：`measure_draft.py`，定義寫在 docstring；用第①步的 M3 驗證：前／後／預測領先 36.89／69.71／69.72，與第①步一致。

## D48 第②步的事前宣告（待使用者核准）

下面是證據目錄 `build/target/_build/test/logs/pvp-v7-d48-step2-dev-20261010/declaration-draft.md`（SHA-256 `34df9e43…`）的全文。核准時複製到 session 的證據目錄，雜湊寫進 `declaration.sha256`。準備：runner 依 D50 (b1) 定稿（測試 12 個通過）、量測腳本 `measure.py` 凍結候選（`045714f1…`）；D45 的 xhigh 對抗式檢查判定「修正後可用」，修正已套用，主對話再依檢查原文逐段核對替換文字。

### D48 第②步 宣告（草稿）：Gateway→Match IPC 輸入延遲的注入與模型驗證

- 狀態：**草稿，未執行**。D45 的對抗式檢查（xhigh，專門核對每個門檻的來源）已完成，判定「修正後可用」；本版已套用它的修正（證據在 `adversarial/`）。下一步送使用者核准。主對話依對抗式檢查的原文逐段比對過替換文字，補正了 P1'／P1''、fault_at 的射擊延遲、V2 的措辭、`relay_error` 的註、停止條件、第 12 節三列與 D50② 的措辭。核准前不跑 session。
- 依據的頭：`claude/pvp-v7-p2` 的 `cdd3a59`。產品程式碼＝`a954aa3`（`git diff --name-only a954aa3 cdd3a59` 在 `docs/` 以外是空的）。
- 依據的決定：D48（第②步）、D50（注入改為 Gateway→Match 的 IPC 延遲、實作 b1、四個條件）、D48 第②步的決定 3＝(iii)。
- 本草稿的工具、試跑與數字都在 `build/target/_build/test/logs/pvp-v7-d48-step2-dev-20261010/`（`commands.txt`、`sha256.txt`）。

#### 1. 目的（決定 3＝(iii)）

第②步只驗證三件事，**不**驗證脆弱窗那一側：

1. **注入乾淨**（V1）：注入不造成產生空檔、runtime_gap 或重新播種，而且注入在 slew 結束之前就結束。
2. **穩定重現**（V2）：每次注入都產生 tracking 型、約 −33.3 ms（2 Tick）的單次 late 修正。
3. **模型在不越線那一側的精度**（V3、V4）：Match 端領先 after ≈ 修正前的領先＋33.33−W_post；每個合格事件也照「重設 ⇔ 領先 >66.7 ms」檢驗。偶然越線的事件同樣照預測檢驗，但**不要求**一定碰到越線。

越線那一側（W_post 落在脆弱窗、領先 >66.7 ms）交給第③步：09a 原型在設計上會讓 W_post≈0。本步不檢定 09a／B2，不判斷 D44 是否提前，不做三棵 tree 的對照。

#### 2. 產物與建置

- 產品程式碼 `a954aa3`。session 開始前在 `docs/` 以外只能多出 runner 與它的測試兩個檔（見下；若核准時決定把測試登記進 CTest，`tests/object_fps_pvp/CMakeLists.txt` 的那幾行也算，Match 與 probe 的雜湊仍必須不變）。
- **建置方式（選定）：先 commit，再在主 checkout 原地建置。**
  1. 核准後，把 `build/acceptance/object_fps_pvp/run_short_stall.py` 與 `test_run_short_stall.py` 和本宣告相關的文件更新一起 commit 到 `claude/pvp-v7-p2`（是否推送由使用者決定，與 session 無關）。這個 commit 記為 session 頭 H。
  2. **從 commit H 到 session 結束，主 checkout 不能有任何變動**：HEAD＝H，`git status --porcelain` 必須是空的（追蹤檔沒有變更，也沒有未被 ignore 的 untracked 檔）。HANDOFF 的「現況」更新併進 H，或等 session 結束後再寫。理由：Go 在 repository 根目錄跑 `git status --porcelain` 來決定 `vcs.modified`（Go 1.27.1 `src/cmd/go/internal/vcs/vcs.go:231`），任何位置的 untracked、未 ignore 的檔都會讓它變成 true。建置前、開跑前、結束時各核對一次；runner 每回之前另核對 HEAD 與追蹤檔（`--untracked-files=no`）。
  3. `cmake --build build/target/_build/test --target gyo_object_fps_pvp_action_probe gyo_object_fps_pvp-match gyo_object_fps_pvp-gateway`，log 寫進證據目錄。
  4. **建置後的兩個 preflight**（任一不成立就停下：不寫 `artifacts.sha256`，不開跑）：
     - P1：`go version -m` 的 Gateway 顯示 `vcs.revision=H`、`vcs.modified=false`。
     - P1'：新 Gateway 的 `go version -m` 與試跑時的 Gateway（`ef5b9779…`）比較，除了 `vcs.revision`、`vcs.time` 以外全部相同。
     - P1''：`git diff --name-only a954aa3 H` 在 `docs/` 以外只有 runner 與它的測試（登記 CTest 時再加 `tests/object_fps_pvp/CMakeLists.txt`）。
     - P2：Match、probe、arena 的雜湊和試跑相同（Match `c9cc6617…`＝第 08b 批 L2 的 match-after，probe `d45a1f0b…`，arena `0026013c…`；預期 probe 與 Match 不重新編譯），凍結工具的雜湊等於下面列出的值。
  5. P1、P1'、P1''、P2 都成立後才寫 `artifacts.sha256`（Gateway 的雜湊要在 H 上建置完才知道），開跑前與結束時用 `shasum -a 256 -c` 核對。
- **不選 detached worktree 的理由**：從零建置會換掉路徑，Match 與 probe 不再和試跑、第 08b 批 L2 逐位元組相同（S1 為此需要另做路徑替換的核對並請使用者核准）。原地建置讓試跑與本 session 只差 Gateway 的 VCS 戳記；試跑時 Gateway 是在 `fe25ba6`（`vcs.modified=false`）建置的，`fe25ba6..cdd3a59` 只改文件。
- **風險**：主 checkout 的建置目錄是共用的。核准到 session 之間若有其他建置，雜湊就會不同；由 P2、開跑前的 `shasum -a 256 -c artifacts.sha256`、每個跑次的 `artifacts.json` 對 `plan.json`（量測腳本的停止條件）攔下。
- **`artifacts.sha256`（建置與 P1、P2 之後寫入，開跑前與結束時核對）**：Match、Gateway、probe、arena；`run_short_stall.py`、`action_probe.py`（`59002b43…`）、`backpressure_probe.py`（`c88ceb89…`）、`gameplay_evidence.py`（`01a45228…`）、`command_evidence.py`（`5fa9779b…`）、`acceptance_util.py`（`227d021f…`）、`run_network.py`（`494b0987…`）、`impaired_network.py`（`abe49647…`）；`sleeper.py`（C1 版 `98105dbf…`）；`measure.py`；第①步的 `detector.py`（`445271ce…`）；本宣告。

#### 3. 工具

- **runner**：`build/acceptance/object_fps_pvp/run_short_stall.py`（sha256 `b5a6d33c…`；測試 `test_run_short_stall.py` `835cdbd3…`，12 個測試通過，含對抗式檢查補上的測試：window_ms 與 release 的核對、只改追蹤檔也會停、所有延遲片段送出後才 release、視窗結束後的位元組立即轉送；對抗式檢查的 7 個突變全部被殺掉）。依 D50 (b1)：
  - 在自己的行程裡把 `action_probe.IpcPause` 換成子類別 `IpcDelay`，每回結束就還原；呼叫不變的 `run_case(options, 'host-ipc', window_ms)`。凍結檔不改。
  - 開頭核對實際匯入的 `action_probe.py` 與 `backpressure_probe.py` 的 sha256，不符就拒絕執行，什麼都不寫（D50 條件③）。
  - 每回結束後讀該跑次 `result.json` 的 fault，必須有 `mechanism=ipc-input-delay`、`delay_ms`、`window_ms`（等於宣告值）、`delayed_chunks ≥1`、`start_ns`、`release_ns > start_ns`，證明替換真的生效；缺了就記為 `injection_unproven` 並停下（D50 條件③）。
  - `run_case` 的判定只記錄（`run_case.recorded_only`），判定來自凍結的量測腳本（D50 條件④）。
  - 輸出目錄必須是新的或空的；寫 `plan.json`（參數、排程、工具與產物雜湊、sleeper、repository 狀態）、每回一行的 `results.jsonl`、`idle_gate.jsonl`、`summary.json`（含結束時的 `repository_end`）。失敗的跑次保留，不重跑。
  - 停下（其餘回記為 `not_run`）：runner 錯誤（`run_case` 例外，沒有 artifacts）、`injection_unproven`、HEAD 或追蹤檔改變（每回之前核對）、sleeper 失敗、閒置閘門 30 分鐘不成立、SIGINT／SIGTERM。
  - **中斷的注意事項**：SIGINT／SIGTERM 只有送到 runner 自己的 PID（例如 `kill -TERM <pid>`）時才會做完當回再停；在終端機按 Ctrl-C 時，SIGINT 會送到整個前景行程群組，Match、Gateway、probe 也會收到，當回會變成 runner 錯誤。不論哪一種，中斷都照停止條件處理（`summary.json` 的 `interrupted`），保留證據、不重跑。
- **量測腳本**：`measure.py`（sha256 `045714f12dd5020d88b4d34e0df07d85a6e6607d17651f76ee08263fc6fadf14`；以對抗式檢查的修正版為底，另在 docstring 補上新欄位的說明）。完整定義寫在它的 docstring；核准時以這個 sha256 凍結，複製到證據目錄。判定寫在最後一行 `judgement`，其中 `step2` 欄位是第②步的結論（第 8 節）。
  - 開頭核對第①步偵測器的 sha256（`445271ce…`），不符就拒絕（結束碼 2）；偵測器以唯讀方式載入，不寫 bytecode。
  - 判定模式讀 runner 的 `plan.json`，參數必須等於宣告值（δ 50 ms、視窗 100 ms、fault_at［2.0, 11.5］、8 回、60 FPS），否則拒絕。
- **sleeper**：C1 的 `sleeper.py`（`98105dbf…`，與 S1 相同），複製到證據目錄。

#### 4. 主機

- 閒置：session 期間不做開發；runner 以 `caffeinate -dims` 執行；前景沒有其他使用者程式（照 C1 第 2 次：關掉 Chrome、ChatGPT／Codex）；**暫停 PR #73 的 auto-fix**；背景的其他分析也先停下，跑完再恢復。
- 閒置閘門（C1 第 2 次 session 的附註，與 S1 相同；runner 的 `--idle-gate`）：第 1 回之前連續 3 次 5 秒 sleeper，每次最大值都 <10 ms。不成立就每 30 秒重試，最多 30 分鐘，仍不成立就停下。
- 每回前後各一次 5 秒 sleeper，並記錄 CPU 最高的 5 個程序（runner 的 `--sleeper`）。
- probe 的 TimerBaseline（`result.json` 的 `timer_baseline`）只記錄，不分層、不判定。

#### 5. 注入

- 機制 `ipc-input-delay`：δ＝50 ms，視窗＝100 ms；fault_at 交錯 2.0／11.5 秒（第 k 回：k 奇數 2.0、偶數 11.5），每個跑次只注入一次；v5 gameplay probe、60 FPS。
- **D50 條件①：延遲的是 Gateway→Match 方向的全部流量。** 移動輸入、動作、ACK、控制都在同一條 IPC 串流上，一起延遲，不只是輸入。視窗內 relay 讀到的每個位元組在讀到後 50 ms 才送出；位元組順序不變，所以視窗之後讀到的位元組排在被延遲的後面（TCP 串流不能超車），整條串流實際上要到約「視窗＋δ」才恢復（試跑 `release_ns − start_ns`＝140～147 ms）。**Match→Gateway（snapshot、結果）永遠不延遲、不暫停**（`IpcDelay.arm` 不設定基底類別的 `duration`，`backpressure_probe.py:79-106` 的下行迴圈照常轉送）。
- **D50 條件②：Python relay 整場都在 Gateway 與 Match 之間。** `run_case` 的 host-ipc 模式在 Gateway 啟動之前就建立 relay，Gateway 連到 relay 的埠（`action_probe.py:531-533`），每個跑次從開始到結束都經過它。這**不是正式的部署拓撲**；視窗之外，每個 Gateway→Match 的位元組也經過 relay 的讀取執行緒與送出執行緒的交接，Match→Gateway 也整場經過 relay 的逐框讀取（標頭與內容分兩次讀，並檢查協議版本）。第③步必須讓三棵 tree 都用同一份 runner（同一個 sha256）、經過同一個 relay，三者才彼此可比（D50②）；這要寫進第③步的宣告。數字不能當成正式拓撲的值。
- 選 δ 50／視窗 100 的理由（試跑）：δ 40～50 ms、視窗 100 ms 的 5 個跑次、10 個事件全部是單次、約 −33.3 ms 的 tracking 型 late 修正；δ 80／視窗 150 的 1 個跑次產生兩次串接的 late 修正（缺陷 B，共 −66.7 ms，領先約 94 ms，兩位玩家都 backlog 重設）。串接途徑不管 W_post 都會重設，09a 管不到；本步不跑它，只作為第③步的背景。
- 選 fault_at 2.0／11.5 的理由與限制：
  - gameplay 計畫是確定的（`gameplay-plan.json` `65bad3b1…`）：玩家 2 在約 9.77 秒 life_respawn（試跑全部相同），之後 60 Tick（約到 10.77 秒）不能再重設（`Movement.hpp:40`）。
  - 2.0 秒：事件視窗［t_c−1 秒, t_c＋3 秒］約為［1.1, 5.1］秒，離 respawn 很遠。
  - 11.5 秒：修正前的視窗從約 10.6 秒開始，和重設冷卻的尾巴重疊。冷卻只管能不能重設，而結果視窗［t_c, t_c＋3 秒］從約 11.6 秒開始，在冷卻之後；life 設限看的［t_c−1 秒, 結果時刻）也不含 9.77 秒的 respawn。玩家 2 這時是新的 epoch，修正前的領先由新 epoch 的命令算出。
  - late 修正之後約 4.1 秒會有下一次視窗修正，若是正向，產生間隔是 22.2 ms（>20 ms）：試跑中在約 6.2 秒與約 15.7 秒，都比 V1 與事件視窗的結束晚約 1.1 秒；trace 在約 16.0 秒結束（t_c＋3 秒約 14.6 秒）。
  - 兩個時刻的注入視窗內都有動作的裁決（試跑約 2.02 秒、約 11.58 秒），所以條件①的「動作也一起延遲」實際會發生：2.0 秒那一個裁決的 Tick 在 δ 50 時是 305，在 δ 40／80 時變成 308／310。玩家 1 的計畫射擊剛好在 2.0 與 11.5 秒，這一發是否落進視窗取決於競爭：y1／y3／z1 沒有被延遲，y2 被延遲約 70 ms（解析 Tick 間隔 18／11，權威冷卻 10 Tick）。`run_case` 的判定因此可能受影響，只記錄。
  - 兩個時刻的自然 token 水位分布都量過（`natural_x.txt`）。

#### 6. 跑次

- **8 次**（4 次 2.0 秒、4 次 11.5 秒），約 16 個玩家事件。不隨機化，不補跑。
- 理由：
  - 重現率：8 次中至少 7 次「重現」（V2）。不乾淨的跑次算不重現；越線而重設的跑次仍算重現。V2 容許的是 1 次不伴隨 V1 問題的未重現（修正量、串接）；伴隨 >20 ms 產生空檔、seed 或 runtime_gap 的主機雜訊由 V1（8／8 乾淨）判為不成立。真實的每跑次重現率若是 0.95，P(≥7/8)＝0.94；0.80 時 0.50；0.70 時 0.26。
  - 模型精度：約 16 個事件、至少 12 個合格，而且至少來自 6 個跑次（V3）。同一跑次的兩位玩家 W_post 相差 ≤0.6 ms，**獨立單位是跑次**。
  - 自然越線（只記錄）：`natural_x.txt` 的 10／42（兩個時刻各一次）混合了 clean-30（30 FPS）、250 ms 故障案例與 S1 的 p2 建置；條件和本 session 最接近的 clean-60（60 FPS）是兩個時刻各 2／6。所以每跑次約 0.24～0.33，8 次至少碰到一次約 0.89～0.96。樣本小，W_hat 的擬合也沒有驗證，只記錄、不要求碰到。開發跑次 δ 40～50 的 5 個跑次碰到 1 次（z1，見第 13 節）。
  - 再多跑也不會改變第②步的結論：越線那一側本來就交給第③步。
- 時間：每回約 30 秒（`run_case` 約 20 秒＋兩次 sleeper），8 回約 4～5 分鐘，另加閒置閘門。

#### 7. 定義（摘要；完整定義與常數在 `measure.py` 的 docstring）

- **事件**：第①步凍結偵測器的 P 事件（`action-frames.jsonl` 的 `phase_late_corrections` 增加），tracking 型（前一幀的 `phase_state` 不是 Acquiring），配對到偵測器的負向產生相位步（`first_seq`～`last_seq`＝被 slew 的命令，`size_ms`＝淨位移）。
  - `in_span`：幀時刻落在［注入開始, 開始＋1 秒］，而且不是重設之後的事件。
  - `post_reset`：該玩家在事件前 3 秒內有非 respawn 的重設（例如重設後的重新擷取）；只列出。
  - `chained`：increment >1，或同一玩家 0.5 秒內有另一個（非 post_reset 的）P 事件（缺陷 B 的途徑）。
  - `qualifies`：tracking、increment＝1、不是 chained、|size−(−33.33)| ≤1.0 ms。
- **等待**：w(s)＝第一次 `sent`（在 `generated` 之後）−`generated`，真實（非 seed）命令。W_pre 是修正前 1 秒與 0.25 秒的中位數；slew 期間逐命令列出（`first_seq−2`～`last_seq+2`）。
- **W_post**：t_post＝`last_seq` 的產生時刻；W_post＝［t_post, t_post＋0.25 秒）內命令等待的中位數（至少 8 個）。
- **Match 端領先**：lead(s)＝Match `resolved`（source actual）−`host_accepted`。lead_before＝［t_c−1 秒, t_c）內產生的命令的中位數；lead_after＝［t_post, t_post＋0.25 秒）內產生、而且算得出領先的命令的中位數，**至少 8 個**（`n_lead_after`；不足時為 null，事件記為「lead_after／lead_pred 無法計算」而不合格），另列 P10／P90 與 1 秒版。lead_pred＝lead_before＋33.333−W_post；model_diff＝lead_after−lead_pred。
- **排隊**：照 `PvpMatch.cpp:427-451` 重播每 Tick 的 `queued` 與 30 Tick 合計（換 epoch 或重設時清空），列出軌跡、最大值、第一次 ≥105 的 Tick。
- **結果**：reset＝［t_c, t_c＋3 秒］內該玩家的 backlog 重設。結果在第一個非 respawn 重設時就決定，所以下面的設限只看［t_c−1 秒, 結果時刻）：重設本身造成的重新播種與重新擷取不會把它設限掉（這一點是開發跑次 z1 找到的，見第 13 節）。
- **設限**（任一成立就不合格，列出）：trace 提早結束（沒有重設時）、注入邊界落在 t_post 之後、產生間隔 >20 ms、Client runtime_gap（dropped >1 ms 或 frame ≥50 ms）、任何 Match runtime_gap、死亡或 respawn、**結果時刻的那個重設（第一個非 respawn 重設）是 starvation／sequence_exhausted**、另一次 phase 修正。
- **合格事件**（V3、V4 的對象）：乾淨跑次中 `in_span` 且 `qualifies`、沒有設限、lead_before／lead_after／W_post 都算得出來的事件。
- **token 水位**：從 `sent` 封包重建（`ClientConnection.cpp:595-605` 的規則），**標記為重建值**（欄位名以 `reconstructed_` 開頭），只記錄。

#### 8. 判定

- **V1 注入乾淨**（每個跑次）。乾淨＝以下都成立：
  - delay 欄位齊全且等於宣告值；`relay_error` 為 null；frames 有相位計數器；
  - 每位玩家在［注入開始−1 秒, end_p）內：沒有 >20 ms 的產生間隔、沒有 `seeded_neutral` 的產生、沒有 Client runtime_gap（同上的門檻）；end_p＝該玩家在［開始, release＋3 秒］內第一個非 respawn 重設，沒有就是 release＋3 秒；
  - ［開始−1 秒, 最晚的 end_p）內沒有 Match runtime_gap；
  - 每個 `in_span` 事件的 `release_ns < t_post`（注入在 slew 結束前就結束）。
  - **V1 成立 ⇔ 8 個跑次都完成而且都乾淨。**
  - 註：這裡的 `relay_error` 是 `result.json` fault 中 IPC relay（IpcDelay）的 socket 錯誤，只記到 release 為止（`run_case` 在 release 時取 evidence）；之後的 relay 錯誤會讓 probe 失敗，歸到 runner 錯誤（E）。UDP relay（ActionRelay）的錯誤在 `relay.json`，只進 `run_case` 的判定（只記錄）。
- **V2 重現**：跑次「重現」＝乾淨，而且兩位玩家各有剛好一個 `in_span` 的 P 事件，且它 `qualifies`。**V2 成立 ⇔ 重現的跑次 ≥7／8。**
- **V3 模型精度**（合格事件）：|model_diff| ≤1.0 ms。
  - 成立 ⇔ 合格事件 ≥12、來自 ≥6 個跑次，而且全部在容許內。
  - 不成立 ⇔ 任一合格事件超出 1.0 ms。
  - 無法判定 ⇔ 合格事件 <12 或跑次 <6，而且沒有超出的。
- **V4 邊界**（合格事件）：門檻 66.667 ms（4 Tick），帶寬 ±1.0 ms。兩側分開判定：
  - 帶內（|lead_after−66.667| ≤1.0）：只列出、不檢定。
  - 不越線那一側（lead_after <65.667）：有合格事件重設了就是「推翻」；否則至少有一個這一側的合格事件就是「一致」，一個都沒有就是「無法判定」。
  - 越線那一側（lead_after >67.667）：有合格事件沒有 backlog 重設就是「推翻」；否則有重設的合格事件就是「一致」，沒有就是「無法判定（交給第③步）」。
  - V4 整體：任一側推翻就是「推翻」，否則「一致」。
- **「推翻」「不成立」「無法判定」的意義**：
  - 推翻（V4）：出現與預測相反的決定性觀測。模型或邊界錯了，D44 的推論鏈要重看；停在第②步回報，不進第③步。
  - 不成立（V1、V2、V3）：注入不符合前提（V1、V2），或模型精度不如宣告（V3）。注入不能照設計當第③步的工具，或第③步不能用預測值代替實測；回報，由使用者決定。
  - 無法判定：越線側沒有碰到、合格事件或跑次不足、帶內事件、被設限的事件。不視為支持，也不視為反證；不補跑。
- **結論欄位 `step2`**（`judgement` 行，依序）：任一停止條件 →「stopped」；否則 V4 推翻 →「refuted (V4)」；否則 V1、V2、V3 成立**而且不越線那一側「一致」**→「passed」；其餘 →「not passed」。
- **第②步通過 ⇔ `step2`＝passed**（越線那一側「一致」或「無法判定」都可以）。通過代表這個注入可以當第③步的工具。
- 判定由凍結的 `measure.py` 的 `judgement` 行機械地給出；`run_case` 的判定不參與。

#### 9. 只記錄

- `run_case` 的判定與錯誤（凍結分析器的 host-ipc 規則）；TimerBaseline；每回前後的 sleeper 與 CPU 最高的程序。
- W_pre（1 秒、0.25 秒）、slew 期間逐命令的等待、W_post_first、lead_after 的 P10／P90、`n_lead_after` 與 1 秒版。
- 排隊軌跡、30 Tick 合計的最大值、第一次 ≥105 的 Tick、重設發生在 t_c 之後幾 ms。
- link B：「重設 ⇔ lead_pred >66.667」（以預測代替實測的版本）。
- 範圍之外的自然 late 修正、post_reset 事件、chained 事件、Acquiring 型事件、被設限的事件與原因；被設限的越線事件另列（`censored_crossing`）。
- token 水位重建：x（slew 前一個命令、最後一個 slew 命令）、重建的漂移，以及經驗擬合 W_hat＝(26.8−16.667·x) mod 16.667 對 W_post 的差（擬合是試跑得出的，不是推導）。
- 注入本身：`delayed_chunks`、`delayed_bytes`、`last_delayed_sent_ns`、`maximum_send_lateness_ms`（送出比預定晚多少；z1 為 2.15 ms）。
- 越線事件（若有）的完整時間線：這是第③步規劃的輸入。

#### 10. 停止條件（立即停下，保留全部證據，不重跑，交使用者決定）

1. 範圍需要擴大：要改權威（authority digest）、wire、FireGate 常數、late 修正上限、target、lead、backlog 門檻，或要改凍結檔，或要把注入升級成凍結工具的正式模式（D50 的 (c)）。
2. runner 錯誤（E）：`run_case` 例外（沒有 artifacts）、`injection_unproven`、sleeper 失敗、閒置閘門 30 分鐘不成立。
3. 分析錯誤（A）：`measure.py` 對任一跑次丟出例外，或 frames 沒有相位計數器。凍結分析器在結構上拒絕 trace 時（`command_evidence.read_trace_events` 丟出 ValueError，`gameplay_evidence.analyze` 不攔），`run_case` 會在內部捕捉、回傳沒有 artifacts 的結果，runner 記為 `runner_error`，歸到 E。
4. 雜湊不符：runner 開頭的凍結檔核對（拒絕執行）；開跑前與結束時的 `artifacts.sha256`；`measure.py` 的偵測器核對；任一跑次的 `artifacts.json` 與 `plan.json` 不同；`plan.json` 的凍結檔雜湊與凍結值不同。
5. 主 checkout 改變：runner 每回之前核對的 HEAD 或追蹤檔改變；`summary.json` 的 `repository_end` 與 `plan.json` 的 `repository` 不同（`measure.py` 的停止）；開跑前或結束時 `git status --porcelain` 不是空的（含未 ignore 的 untracked 檔，第 2 節）。
6. 建置後的 preflight（P1、P1'、P1''、P2，第 2 節）不成立；或 session 被中斷（SIGINT／SIGTERM，`summary.json` 的 `interrupted`，第 3 節）；或 runner 異常結束、沒有 `summary.json`（`measure.py` 拒絕，結束碼 2）。
7. 任一跑次出現權威 Cooldown 拒絕（`actions.jsonl` 中 rejection＝3，`Combat.hpp:51-53`；P2 的停止條件，與 S1 相同）。回報時標明那一發是否在注入視窗內被延遲。
- 失敗或被設限的跑次照樣保留並列入彙總；不補跑。

#### 11. 證據

`build/target/_build/test/logs/pvp-v7-d48-step2-<YYYYMMDD>/`（session 當天的日期）：

- `declaration.md`（本宣告的核准版）與 `declaration.sha256`；`artifacts.sha256`；`measure.py`、`sleeper.py`（複本）；`build.log`；`commands.txt`；`preflight.txt`（建置前的 `git status --porcelain`、P1、P2、開跑前的 `shasum -c` 與 `git status --porcelain`）。
- `session/`：runner 的輸出（`plan.json`、`idle_gate.jsonl`、`results.jsonl`、`summary.json`、8 個跑次目錄）；`run.stdout.txt`。
- `measure.jsonl`：`python3 -I measure.py <第①步 detector.py> session` 的輸出（每個跑次、每個事件、最後一行 judgement，含 `step2`）。
- 結束時的 `sha256.txt`（全部檔案）。

預定的命令（核准並 commit H 之後；`E`＝證據目錄，`L`＝`build/target/_build/test/logs`）：

```
git status --porcelain                                    # 必須是空的
/Users/karasu/Code/Runtime/Builder/cmake-4.4.3/CMake.app/Contents/bin/cmake --build build/target/_build/test \
  --target gyo_object_fps_pvp_action_probe gyo_object_fps_pvp-match gyo_object_fps_pvp-gateway > $E/build.log
/usr/local/go/bin/go version -m build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway   # P1
shasum -a 256 <Match> <probe> <arena> <凍結工具>                                                        # P2
#（P1、P2 成立後寫 $E/artifacts.sha256）
shasum -a 256 -c $E/artifacts.sha256 && git status --porcelain
caffeinate -dims python3 build/acceptance/object_fps_pvp/run_short_stall.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_action_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output $E/session --delay-ms 50 --window-ms 100 --fault-at 2.0 --fault-at 11.5 --rounds 8 \
  --sleeper $E/sleeper.py --idle-gate > $E/run.stdout.txt
shasum -a 256 -c $E/artifacts.sha256 && git status --porcelain
python3 -I $E/measure.py $L/pvp-v7-d48-step1-20261010/detector.py $E/session > $E/measure.jsonl
```

#### 12. 門檻的來源（D45）

建置欄的「a954aa3」指：Match `c9cc6617…`、Gateway `ef5b9779…`（`fe25ba6`，`vcs.modified=false`）、probe `d45a1f0b…`、arena `0026013c…`。「試跑」＝`pvp-v7-d48-step2-dev-20261010/` 的 y1（2 次）、y2、y3、y4 與 z1；主機沒有閒置閘門（z1 的 sleeper 最大 4.07／4.24 ms）。

| 門檻 | 值 | 來源 | 建置 | 指標 | 流量 |
|---|---|---|---|---|---|
| δ／視窗 | 50／100 ms | 試跑：δ 40～50、視窗 100 的 5 個跑次、10 個事件都是單次 −33.3 ms；δ 80／視窗 150 串接（缺陷 B） | a954aa3 | `action-frames.jsonl` 的 `phase_late_corrections`＋偵測器的產生相位步 | v5 gameplay probe 60 FPS；relay 整場在 Gateway 與 Match 之間；注入只動 Gateway→Match |
| fault_at | 2.0／11.5 秒 | 玩家 2 的 life_respawn 約 9.77 秒（試跑全部相同，`gameplay-plan.json` `65bad3b1…`）；重設冷卻 60 Tick（`Movement.hpp:40`），11.5 秒時只和修正前視窗重疊（第 5 節）；下一次視窗修正約在 late 修正後 4.1 秒；trace 約 16.0 秒結束 | a954aa3 | Match `reset`（life_respawn）、最後一筆 `resolved`、Client `generated` 間隔 | 同上 |
| 閒置閘門 | 3 次 sleeper 最大 <10 ms，30 秒重試、30 分鐘上限 | C1 第 2 次 session（使用者核准的附註），S1 沿用 | 獨立的 sleeper 程序 | sleeper 的 1/60 秒絕對期限晚醒 | 無 session 流量 |
| 產生空檔 | >20 ms | Client 的步長只有 16.67／13.33（T/1.25）／22.22 ms（T/0.75）；>20 ms 是主機停頓或正向 slew。D48 第①步的 0.006～0.17％ 是 27～76 ms 空檔的比例（5 筆自然事件修正前都有），不是這個門檻的頻率。閒置主機的基準：第 08b 批 L2 的 clean-60（12 個跑次，before／after 兩種 Match）在 2.0／11.5 秒的兩個 V1 視窗內，0／24 有任何 V1 問題（`adversarial/analysis/gaprate.txt`）。注意正向 slew 的產生間隔是 22.2 ms，也會超過門檻：試跑中出現在開始後約 0.17 秒（首次決定）、respawn 後的首次決定（約 9.9 秒）、late 修正後約 4.1 秒的視窗修正（約 6.2 秒、約 15.7 秒；y1 玩家 1 再過 4 秒於 10.3 秒），都在 V1 與事件視窗之外（最近的差約 1.1 秒）；y2 在 5.7 秒有 30～38 ms 的主機停頓（非閒置主機，在視窗外） | a954aa3 的 Client（模擬角色在步邊界產生） | 每位玩家真實命令 `generated` 的相鄰間隔（穩態 16.7 ms；負向 slew 13.3 ms；正向 slew 22.2 ms） | 每位玩家，不分 epoch |
| Client runtime_gap | dropped >1 ms 或 frame ≥50 ms | 試跑與 z1 共 13 個跑次的 21 筆 Client runtime_gap 都是 epoch 開始的 seed clamp（sequence 3，dropped ≤0.093 ms，frame ≤16.8 ms；初稿寫的 24 筆把 synthetic-session 的 symlink 重複算了）；模擬角色在 frame ≥100 ms、dropped >0、frame 被夾（frame > elapsed）或擋住步時才寫（`LocalPlayerPrediction.cpp:332-338`）；分析器 v7 對 <100 ms 的 seed clamp 也豁免 | a954aa3 | Client `runtime_gap` | 每位玩家 |
| Match runtime_gap | 任何一筆 | Match 只在 elapsed ≥100 ms 或 dropped >0 時寫（`MatchRuntimeHost.cpp:314-316`）；試跑與 z1 的 13 個跑次 0 筆 | a954aa3 | Match `runtime_gap` | Match 全體 |
| 修正量 | −33.333±1.0 ms | 上限 2 Tick（`LocalPlayerPrediction.cpp:97-98`、`Movement.hpp:59`）；試跑 10 個事件 −33.29～−33.39 ms | a954aa3 | 偵測器的產生相位步淨位移（前後各 9 個命令的中位數） | 每位玩家、事件的 epoch |
| V2 重現 | ≥7／8 跑次 | 試跑 5／5 跑次（10／10 事件；δ 50／視窗 100 為 4／4 跑次、8／8 事件）。5／5 的單側 95％ 下界只有 0.55，所以 7／8 不是由試跑推出的，是宣告的要求：第③步需要可預期的 late 修正產量，只容許 1 次不伴隨 V1 問題的未重現 | a954aa3 | P 事件（同上） | 同上 |
| V3 容許 | model_diff 的絕對值 ≤1.0 ms | 試跑 10 個事件最大 0.312 ms（全部為正，+0.11～+0.31，中位約 +0.2），約 3 倍餘裕。第①步 5 筆自然事件的差是 0.01～2.94 ms（M3 0.01，其餘 1.0～2.9），但 5 筆都有產生空檔（本定義下全部被 gen_gap 設限），不用來定容許 | a954aa3 | Match `resolved`−`host_accepted` 的中位數（after：t_post 之後 0.25 秒產生的命令，至少 8 個）對 lead_before＋33.333−W_post | 只算 actual 解析的移動命令；不含動作 |
| V3 最少事件與跑次 | 12 個事件，來自 ≥6 個跑次 | 8 跑次×2 位玩家＝16，容許 4 個不合格；同一跑次的兩位玩家不獨立（W_post 相差 ≤0.6 ms），所以另要求跑次數 | — | — | — |
| V4 門檻 | 66.667 ms（4 Tick） | 每 Tick 的 `queued`≈floor(領先÷16.667 ms)：領先在 3～4 Tick 時排隊 3 個（30 Tick 合計 90）、超過 4 Tick 時 4 個（120）（09b 第①步第 3 點）；backlog 門檻 30 Tick 合計 ≥105（`Movement.hpp:38-39`、`PvpMatch.cpp:427-451`）。第①步 5 筆單調一致；試跑：64.3 ms 合計 90 不重設，z1 68.9／69.5 ms 合計 108 重設 | a954aa3 | Match `resolved` 的 `queued` 重播與 `reset` | 每位玩家 |
| V4 帶寬 | ±1.0 ms | 試跑 after 視窗內逐命令領先的 P10～P90 寬度 ≤0.63 ms（半寬 ≤0.31）；模型誤差 ≤0.31；第①步 M4（66.1 ms，右設限）會落在帶內 | a954aa3 | 同 V3 | 同 V3 |
| 視窗長度 | 修正前 1 秒；after 0.25 秒；結果 3 秒；串接 0.5 秒；範圍 1 秒 | 修正前 1 秒＝第①步凍結的「被釘住」定義；結果 3 秒：單次修正的越線約在 t_c 後 0.5 秒重設（z1 530 ms、M3 488 ms）；上界是下一次視窗決定（約 4.1 秒，它的 22.2 ms 正向 slew 會被當成產生空檔），3 秒在它之前約 1 秒；串接：缺陷 B 的第二次修正在第一次之後 133 ms（y4）；範圍：試跑的修正都在注入開始後 96～136 ms。after 0.25 秒（約 15 個命令）的註：試跑中 after 視窗最後一個命令在 t_c 之後 426～476 ms 解析，單次修正的最早重設在 t_c 之後 509～530 ms（y4 第一次、z1），餘裕 33～100 ms；串接的第二次修正（y4）在 379 ms 重設，after 視窗只剩 9 個命令（原本 15～16 個，`adversarial/analysis/lead_window_vs_reset.txt`）。所以 lead_after 要求至少 8 個命令（`n_lead_after`），不足時不合格 | a954aa3 | 時刻都是 probe 的 C++ steady clock | — |
| W_post 與 lead_after 最少命令 | 8 | 試跑每個事件 15～18 個（串接的第二次修正除外：lead_after 9 個） | a954aa3 | Client `sent`−`generated`；Match `resolved`−`host_accepted` | 每位玩家、事件的 epoch |

#### 13. 開發跑次的觀察（只供參考，不判定、不計入）

- **最終 runner 的 1 次開發跑次 z1**（δ 50、視窗 100、fault_at 2.0；主機沒有閒置閘門）碰到了越線：兩位玩家 W_post 1.57／2.14 ms，lead_after 69.47／68.89 ms（預測 69.36／68.75，差 +0.11／+0.14），t_c 之後 530 ms 兩位都 backlog 重設（30 Tick 合計 108，第 30 Tick 首次 ≥105）。「重設 ⇔ 領先 >66.7 ms」與模型在這個事件都成立。同一跑次的兩位玩家不獨立，所以這是 1 個單位。
- 量測腳本的初版把這個事件設限掉了（重設本身造成的重新播種與重新擷取落在設限視窗內），也讓 V1 判它不乾淨，等於越線那一側在結構上永遠無法判定。凍結版改為「結果決定之前」才設限（第 7 節），這是從開發跑次修正的定義，如實記錄。
- 試跑（原型 runner）與 z1 的 10 個事件：W_post 1.57～11.99 ms；model_diff +0.11～+0.31 ms；注入在 slew 結束前 64～93 ms 就結束；token 重建的 W_hat 與 W_post 的差 0.01～0.08 ms（y2 玩家 1 有重建漂移，差 3.79 ms）。
- 對第①步證據的乾跑：M3（host-ipc-250ms）前／後／預測 36.89／69.71／69.72 ms，與第①步相同。這個跑次的 frames 沒有相位計數器，所以 M3 是偵測器的 S 候選（`validation_only`，不會被判定計入）。5 筆（含 M3）都被 gen_gap 設限，M4 另有 trace_end。
- 對抗式檢查後的版本在同樣的三組乾跑中，每個跑次與事件的值都和初版相同，只多了 `n_lead_after` 欄位；試跑的預覽結論是 `step2`＝not passed（只有 4 個符合宣告參數的跑次，V1、V2 達不到 8 次的要求；這是預期的，不是判定）。

#### 14. 核准時要決定的事與未決事項

1. 本宣告的核准，以及 commit runner 與測試（session 頭 H）後原地建置的做法。
2. `test_run_short_stall.py` 要不要登記進 CTest（`tests/object_fps_pvp/CMakeLists.txt`，照 `action_runner` 的寫法）。目前沒有登記，本草稿不改 CMake。登記會讓 L1 執行它，其中 2 個測試用 loopback socket 與實際時間（下界 49 ms、上界 40 ms 的寬容值）。
3. runner 放在 `build/acceptance/object_fps_pvp/`（產品 owner 的 acceptance 工具），但它只服務 D48；D50 已寫明之後若要成為常設回歸再走 (c)。第③步結束後是否保留，屆時決定。
4. V3 的系統性偏差：10 個事件的 model_diff 全部為正（+0.11～+0.31 ms）。模型沒有扣掉 W_pre（0.08～0.12 ms），扣掉之後還剩 +0.01～+0.19 ms，其餘原因沒有查（hypothesis）。不改模型（決定 3 照第①步的形式寫），只記錄；1.0 ms 的容許已涵蓋這個偏差。
5. relay 送出比預定晚的值（`maximum_send_lateness_ms`，z1 為 2.15 ms）在非閒置主機上量到；只記錄。若閒置主機上仍明顯，第③步要把它列進可比性的前提。

## 缺陷 A：settling 期間收到的舊樣本留在下一個視窗

- **機制**：`Correct()` 只在修正開始時清掉樣本（`LocalPlayerPrediction.cpp:97-101`）。settling 期間 `:117-130` 照常收樣本，用的是舊的 `settledAfter_`；settle 時（`:323-328`）只前移 `settledAfter_`，不清視窗。所以修正後滿 240 個樣本時，視窗裡還有 m 個「修正前相位」的樣本。`Percentile` 取 `sorted[216]`（`:24-30`）。
  - 正向修正時舊樣本落在頂端，P90 實際約 P93.1（m＝8）、P93.5（m＝9）；負向修正時落在底端，約 P89.7，量級只有前者的約 1/9。主要往「相位更晚、slack 更少」的方向偏。
  - 影響的不只是滿窗的那一次決策：滿窗之後每次 TrackPhase 都重算，所以影響前 m＋1 次決策。
- **為什麼重要**：P90 是 FireGate guard 推導的前提（相位追蹤控制到達餘裕的 P90）。偏了，guard 的餘裕就跟著偏。
- **量級（v7 很小）**：兩種獨立方法都在同一量級。開環重播：1049 個修正後的視窗，clean 的 P90 偏移最大 0.27 ms，決策改變 clean 0／379、全部 1／1049（開發 network40）；Match 端的 slack 代理值：08b L2 的 P93−P90 中位 0.02～0.03、最大 0.08 ms。原因是 v7 的 slack 很集中（08b L2 clean after 的 P10／50／90＝36.9／37.3／37.6 ms）。限制：重播的視窗決策只吻合 72／131（55％），0／379 是開環近似，不是實機的決策。
- **這是 v6 就有的程式**：同一段 settle 程式從 `a4ccaa5`（v6）起就存在，`fee92ff`、`03b2ea7`、`805bd11`、`a954aa3` 都相同。所以它不可能是任何新舊兩棵 tree 差異的原因。
- **選項**：
  - A1【建議】settle 時刪掉視窗前段，只保留結尾 `min(lateSamples_, size)` 個樣本（約 2 行）。late 的判斷只讀結尾 `lateSamples_` 個樣本（`:134-135`），A1 正好保留它們，所以在第一次視窗決策分歧之前，late 決策與現行相同（開環性質檢查：3000 條串流中 0／1251 的第一次分歧是 late 決策）。分歧之後的 late 修正可能不同（開環：clean 0／379，network40 1／1049）。另外 `lateSamples_==1` 時會殘留 1 個舊樣本在底端，偏向保守。
  - A2 settle 時連 `lateSamples_` 一起清空：同時改變缺陷 B（見下），不建議。
  - A3 依每個命令「還沒吸收的修正量」換算：精確，但單獨用來修 A 是過度設計；它是 B2 的基礎。
- **途中被推翻的說法**：
  - 「約 P93 是最壞情況」：百分位的口徑混用（`217/(240−m)` 是 (idx＋1)/n，P89.7 是 idx/n）。統一後是「m≈8 時約 P93.1」。
  - 「A1 由構造保證 late 路徑不變」說得太大：只在第一次視窗分歧之前成立。

## 缺陷 B：settling 期間累積的 late 樣本，在 settle 時觸發第二次 late 修正

- **機制**：`:128` 在 settling 中照樣累加 `lateSamples_`（`slack<0 ? +1 : 0`，任何一個準時樣本就歸零）；settle 不重設；下一次 TrackPhase 走 `:134-137`，用沒有換算的舊相位樣本再修正一次，同樣夾在 2 Tick。觸發條件是 settle 前最後 ≥2 個樣本連續遲到，持續延遲或間歇性的成對遲到都可能。`:113-116` 的註解與 `PredictionTests.cpp:1229-1238`（far 測試，−150 ms → 2 次）把它寫成設計意圖。
- **為什麼重要**：多修一次就是多 2 Tick 的領先，可能直接推向 backlog。
- **證據**：v7 建置中有相位觀測的 late 修正只有 8 次，其中 0 次串接成第二次；唯一有完整 trace 的 D44 是單次修正。模型（`judge/sim_judge.txt`）在 60～250 ms 的短停頓下也只修 1 次。但模型每個命令送 1 個樣本，實機是「每次發布取最小值」（`MatchRuntimeHost.cpp:110`、`:312`、`:403`），模型高估了遲到樣本數，所以「短停頓不發生」在實機只會更成立。檢定力很低，寫成「未觀察到」，不寫「排除」。
- **持續延遲時，第二次修正其實有用**（模型，hypothesis）：持續 +60／+70 ms 的延遲，現行碼修正 2 次，最後 slack 44.0／34.0 ms（目標 37.3）；「清空」或「只換算」的修法只修 1 次，slack 只剩 10.7／0.7 ms，維持到約 4 秒後的視窗修正。也就是說，直接把 B 修掉會更差。L1 雖然執行了 94 次缺陷 B，但只有 far 測試有斷言，所以「持續延遲下有用」只有模型支持。
- **選項**：
  - B0【建議】不改程式，把限制寫進文件。
  - B′（只換算 late 旗標）、A2（清空）：持續延遲下更差，不建議。
  - B2（補償換算＋late 修正後以首次決定規則重新擷取，8 個樣本、0.5 ms 死區，不清 `phaseDecided_`）：模型中短停頓後 1 次 slew 撤回 2 Tick，30 Tick 合計 120→74，持續延遲收斂到 37.3。但它會改變 late 修正與 backlog 的行為，屬 D44 的提前條件；只有模型證據，抖動與間歇性遲到下會不會來回修正沒量過。它的前提「slack 隨產生時刻 1:1 移動」在被釘住時不成立，所以只能排在 09a 之後。

## 延遲位移：不是缺陷，是設計造成的結果

- **分解**（08b L2 clean，Client＝`a954aa3`，actual 解析的移動命令）：產生→第一次送出 0.1 ms，送出→Match 收下 0.4 ms，Match 端 slack 37.3 ms，產生→執行（G2E）37.8 ms。兩種主機狀態相同。37.3＝領先 2 步（33.33 ms）＋目標 4 ms（`Movement.hpp:37`、`:45`；`LocalPlayerPrediction.cpp:123-124`）。
- **為什麼 v6 是 21～25 ms**：兩棵 tree 的樣本公式相同（v6final `:121-122`＝現行 `:123-124`），追蹤保持的都是「步邊界→執行」≈37.3 ms。但 v6 的 generated 是畫面幀發布的時刻（邊界＋age），v7 由角色在步邊界醒來時記錄（`ClientSimulationRole.cpp:125`、`:185`）。所以 13～17 ms 的差＝v6 的幀 age 抵扣，兩棵 tree 的「產生→執行」原點不同、不能直接比。v6 的實際餘裕是 37.33−age，30 FPS 時可以低到約 4 ms，這也是 v6 在 clean 會出現 late 修正、30 FPS 有 Held 的原因。
- **使用者實際感受到的**：「輸入取樣→執行」v7 多約 1 幀（C1 s2 4 ms 狀態：clean-60 v6 25.0 對 v7 41.4 ms，clean-30 21.1 對 54.0 ms；受 probe 錨定影響，只記錄）。
- **代價與收益**：v7 把餘裕固定在 37.3 ms，C1 clean-30 的 Client 替代比率因此 0.010％（v6 0.570％）；代價是多 1 幀的輸入延遲，以及 backlog 餘裕減半（75→45），這正是 D44 在 v7 只要一次 late 修正就閉合的結構原因。
- **能動的槓桿**：只有「追蹤的 lead」與 target。`InitialCommandLead` 不能直接改：SeedLead 的中立命令會改變權威 digest，凍結分析器也寫死 2（`command_evidence.py` 的 `INITIAL_COMMAND_LEAD = 2`、`start_phase_evidence.py`）。
- **選項**：
  - 只修文件【建議】：寫明跨樹不可比、I2E 多約 1 幀是步邊界產生的代價、backlog 餘裕 75→45。
  - 拆出「追蹤 lead」＝1（seed 仍為 2，X 37.3→20.7 ms，60 FPS 的 I2E 約 −16.7 ms，backlog 穩態回到 30）：Held 餘裕降到約 v6 60 FPS 的水準（抖動網路下暴露明顯上升），Windows 的角色晚醒未知（第 15 批），`FireGate.cpp:22` 必須同步改（否則 earliest 早 1 Tick），改變 backlog 穩態，觸發 D44 停下詢問。不建議放進 09b。

## 其他發現（列進未結事項，不在 09b 處理）

- **停頓 reseed 依據過時的 snapshot 播種，領先實際上歸零**：reseed 之後新命令的 G2E 只有約 0.2～6 ms（`adversarial/step_context_06l2_gw250_r6.txt`），直到首次決定或 Acquiring 的 late 修正才把領先補回。（規劃時把它描述成「舊命令遲到」，被對抗式檢查推翻。）
- **權威 digest 不保護相位追蹤**：RunLink 不經 TrackSlack，35 個場景的相位樣本都是 0。
- **late 修正並非一律 2 Tick**：角色自己停頓、補步時 age 很大，修正很小（C1 s1 r4 約 −4.5 ms）。只有命令準時產生、遲到在傳輸路徑時才夾在 2 Tick，而 D44 的危險情境正是後者。
- **clean 的 late 修正**：閒置主機的 session（C1 s2、08b L2）是 0；非閒置主機的 C1 s1 有 4 次（r1 因此延遲門檻失敗）。所以 09b 與之後的 L2 都要沿用 C1 s2 的閒置檢查作為前提。

## 實作與驗收（決定確認後）

- **09b-1：A1 與 L1**（high；settle 與 late 交界的局部 xhigh，加 1 位 xhigh 實作審查）：
  - `:323-328` 的刪除與 `:109-116` 的註解。
  - T-A1 視窗衛生（HEAD 要失敗、修改後通過；PhaseTwins 在 settle 那一幀的樣本可能在 Advance 之後被過濾，實際收進的舊樣本可能是 4 個，實作者要照 `:24-30` 的索引核對）；T-A2 late 路徑不變（far 測試不改斷言，另加「settle 時結尾 1 個 late，再來 1 個新 late → 第二次修正」）；T-A3 負向修正與「`lateSamples_==1` 時只殘留 1 個舊樣本，決策不變」。
  - 突變（batch `v7-09b`）：`settle-keeps-window`、`settle-clears-all`、`settle-erases-tail`（寫清楚由哪一個測試殺掉；`lateSamples_=0` 時等於 HEAD）。
  - 全量 CTest、權威兩樹比對 35／35（只當 L1 閘門，它不保護相位追蹤）、TSan。
- **09b-2：開發跑次**（clean-30／60、network20／40、backpressure host-ipc-250ms），用開跑前凍結的偵測器記錄 late 修正（大於 20 ms 的負向步之前 30 Tick 內有 held 或負 slack，而且同一 epoch 之前沒有 seeded_neutral），先在 08b L2、C1 s1／s2 上重算基準並記錄 sha256。
- **09b-3：L2**（宣告 high＋xhigh 對抗式檢查，使用者核准）。要點（草案，對抗式檢查已修正）：
  - before＝09a 的頭，after＝09a＋09b 的頭，各在 detached worktree 從零建置；Gateway 逐位元組相同。
  - 判定：clean-30 ×12、clean-60 ×6 交錯；只記錄：network20、network40、backpressure host-ipc-250ms 各 3 輪。
  - 前提：沿用 C1 s2 的閒置檢查。
  - J1：通過率 after ≥ before−1/N（C1 規則 3 的形式）；「after 全部通過」改為停下回報的條件。
  - J2：G2E 每位玩家 P50 的合併中位數，|after−before| ≤1.0 ms（不退步的檢查，不是 A1 的證明）；來源值要用宣告的同一個腳本、同一個視窗在 08b L2 上重算；任一位玩家每輪的 P50 落在 [36.8, 38.8] ms 之外就停下。
  - J3／J4：領先跳升與非 respawn 重設，after 出現就停下；before 與 after 並列計數；歸因規則事先寫明（before 也出現，或同一跑次沒有視窗決策分歧時，歸到 09a／D44）。
  - J5：Client 替代比率，0.05％ 依本批的跑次長度換算成每輪筆數；排除 Match 停頓的根據是「A1 只改 Client 的私有狀態，Match 行為相同」。
  - 限制：A1 的效果（≤0.3 ms）低於 L2 的解析度，L2 只證明沒有退步，A1 的證明在 L1。
- **之後**：第 09 批的 M0 在 09a＋09b 的頭上量。

## 本批的停止條件

1. 範圍需要擴大到 late 路徑、late 修正上限、target、lead、backlog 門檻或 FireGate 常數，但使用者沒有核准（D44 提前條件）。
2. A1 實作後 far 測試不是 2 次，或 `PredictionTests.cpp:1241-1257` 出現 late 或修正。
3. MovementRecoveryTests 或任何 Held 斷言需要放寬；`worker_main` 的斷言需要放寬。
4. 權威兩樹比對不是 35／35；需要改 wire。
5. 突變沒有被殺，或既有突變變成 stale。
6. L2 的 after 出現 J3、J4 或權威 Cooldown 拒絕；J2 超出門檻或出現雙峰。
7. 凍結分析器拒絕 trace。
8. 開發跑次或 L2 中，late 修正後 30 Tick 合計 ≥105 的比例高於 before。
9. FireGate 常數有任何需要大於 v6 的跡象。
10. 09a 尚未完成，或 09a 的 L2 發現與相位追蹤有關的退步。

## 需要決定的事

1. **（2026-10-10 使用者決定 D48：先用測試確認，再選 (a) 或 (b)；分三步，見 HANDOFF 的 D48）（D44 提前條件）09a 修好送出被釘住之後，D44 可能變多。** 現有 5 次真正的 late 修正：被釘住的 4 次沒有重設，沒被釘住的 1 次就是 D44。你可以從 `adversarial/step_context*.txt` 看到每一次的前後命令時間線。選項：
   - (a) 照原計畫做 09a，但在 09a 的開發跑次與 L2 用凍結的偵測器記錄「每次 late 修正後的 30 Tick 合計」，比例高於 09a 之前就停下；D44 仍在第 16 批。代價：可能在 09a 的 L2 才碰到重設。
   - (b) 把 D44 的一部分提前：09a 與 09b 一起處理，在 09b 加入 B2（late 修正後重新擷取，讓短停頓後撤回 2 Tick）或其他讓 late 修正不閉合 backlog 的方式，再規劃一次。代價：範圍擴大、B2 只有模型證據、L2 要加故障案例與短停頓注入（要改 acceptance 工具）。
   - (c) 09a 暫緩，先查清楚 D44（第 16 批的工作提前到 P2）。代價：P2 的順序大幅改變。
   - 我的傾向是 (a)：先用量測確認「被釘住在擋 D44」是不是真的，樣本只有 5 次，不足以支撐 (b) 的範圍擴大；若 09a 的開發跑次就出現，再停下改走 (b)。
2. **09b 的範圍**：只做 A1 加文件更正（建議）。
3. **缺陷 B**：B0，不改程式、寫明限制（建議）。若決定 1 選 (b)，B2 在那時重新規劃。
4. **延遲位移**：只修文件，常數不變（建議）。拆出追蹤 lead＝1 是替代方案，會觸發 D44 停下，不建議放進 09b。
5. **文件更正**（建議同意）：
   - HANDOFF「C1 之後要決定的事」：「約 P93」改成「m≈8 時約 P93.1；v7 clean 偏移 ≤0.3 ms」；缺陷 B 寫成「未觀察到、hypothesis 層級」，不寫「排除」。
   - 03a「晚約 1 幀、方向相同」不成立：主要理由是兩棵 tree 的 settle 程式完全相同，缺陷不可能造成兩者的差；「方向相反」只是輔助；「probe 幀相位與 age 繞回」標為 hypothesis。
6. **D44 未結事項補上**（建議）：v7 穩態 30 Tick 合計 60 對 v6 的 30；傳輸型的 late 修正才夾在 2 Tick；一次傳輸型 late 修正就會 ≥105（模型 120，實測 1／1 是 108）；送出被釘住時是 89～99（4／4）。
7. **其他發現列進未結事項**（建議）：停頓 reseed 後領先歸零；權威 digest 不保護相位追蹤。
8. **09b 的 L2**：單獨一個 session，讓 M0 的頭明確等於 09a＋09b（建議）；或併進 M0 的前段（同一份建置，加上凍結的 J2～J4），少一次 session。
9. **檔位**：09b-1 主力 high、局部 xhigh，加 1 位 xhigh 實作審查；L2 宣告 high 加 xhigh 對抗式檢查；文件與執行 medium。

## 規劃的過程與證據

- 三份方案的主要結論一致：A1、B0、延遲只修文件、D44 維持第 16 批。評審否決了 risk 方案「短停頓下領先 2→6、合計 180」（沒有證據，與模型矛盾）與 latency 方案把「追蹤 lead＝1」列為建議。
- 對抗式檢查的結論：主要建議可以採用，但多項事實描述要先更正；**最重要的遺漏是 09a 的交互**（上面第一段），應在 09a 實作前問你。已併入的更正：late 修正並非一律 2 Tick；clean 的 late 修正只在閒置主機為 0；late-outcomes 的分類（真正的 late 修正 5 次）；Acquiring late 的機制；「構造保證」的範圍；百分位口徑；J1、J2、J5 的來源與缺少的 late 偵測基準；03a 更正的主要理由。
- 證據：`build/target/_build/test/logs/pvp-v7-09b-plan-20261010/`（`defects/`、`latency/`、`risk/`、`judge/`、`adversarial/`，各有 `commands.txt` 與 `sha256.txt`）。

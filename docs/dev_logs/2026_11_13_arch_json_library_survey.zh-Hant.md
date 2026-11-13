# 架構 v8：v8-arch-json-library-survey（JSON 函式庫的選擇調查）

件數の突き合わせ：不一致——simdjson 的整數檔送 10,010、收 1（整份被拒，屬候選行為；列為計數した破棄，另以宣告外的範圍內 10,008 個重量，送收一致）；其餘各判準送收一致。7 候選 × 6 判準＝42 格（實測 36、文件判定 1、不評 5）。依 AGENTS §14 冒頭，判定全部等使用者確認。

日期：2026-11-13。分支 `claude/arch-json-library-survey`（自 `origin/master` `b9bf1962`）。卡片由架構師會話（「[Job][架構師]例行架構掃描」）開出。
作者：Opus 5.5／high（卡片指定）。複查：Opus 另一代理／high（只讀；架構師建議，與作者同模型，見報告 §7）。

**狀態：** 實測完成，報告寫完，等使用者選方向（未決 1）與函式庫。只改文件；函式庫與實測程式都在 git 管理外，沒有加進 repo 的建置。

相關文件：[事前宣告](../architecture/plans/third-party-evaluation/json-library-survey-declaration.zh-Hant.md)、[報告](../architecture/plans/third-party-evaluation/json-library-survey.zh-Hant.md)、[HANDOFF](../architecture/plans/third-party-evaluation/HANDOFF.md)。

## 起因

後端 plugin 規劃批（PR #212）§4 未決 1 列了 JSON 的四個方向（J1～J4）。使用者：「nlohmann 問題在調查後決定」；並更正任務卡的背景：「未決 1：就是未決，方向不是 J3，等調查完畢之後在決定。」所以本批不以任何方向為前提，量六條判準（使用者確認、不增減），並寫出每個候選在 J1～J4 下的意義。

## 經過

- 寫事前宣告（候選、每條判準的實測方法與判定、下載清單、平台）。架構師轉達更正後，改成不以 J3 為前提：判準 3 分「原文路（J2／J3）」與「候選轉換路（J1／J4）」各自判；新增「對各方向的影響」一節。
- 使用者選平台 C（只在本機），並要求測試放在 Docker 下：改用本機既有的 `gyo-linux-toolchain:l1-20261112`（L1 linux-x64 工具鏈），編譯與執行時斷網、只掛證據目錄。使用者核准全部下載。
- 下載 6 個函式庫與 JSONTestSuite（共 70 MB；Boost 解壓 493 MB，比估的 200 MB 大）。Boost 與 simdjson 對上 GitHub 發行檔的 SHA-256；nlohmann 沒有 gpg 可驗簽，改以解壓後的 `json.hpp` 和 repo 現用 v3.12.0 的 FetchContent checkout 比對（SHA-256 相同）。
- 寫實測程式（每候選一支，模式 strict／dup／raw／conv／order／rt／perf）、`from_chars` 程式、向量生成（種子 20261113）與比對腳本。
- 證據目錄的 Write 被 hook 擋（記憶 no-home-dir-evidence 已記）：先寫到 scratchpad 再用 shell 複製。第一次 `docker run` 把函式庫掛在 `/lib`，蓋掉了容器的 `/lib`（bash 找不到），改掛 `/jlib`。
- 第一次建置有 7 個警告，全部來自實測程式（未用參數、simdjson 的 `BIGINT` 型別沒處理）；修好後 7 支 0 警告。函式庫本身的標頭，`-isystem` 與一般 `-I` 都是 0 警告。
- 第一次全跑後發現比對腳本的錯：整數用表示法比（`u:0`≠`i:0`），而且 simdjson 會因檔中 2 個超出範圍的整數拒絕整份。改成依值比、另加範圍內整數的檔，只重跑判準 3。浮點部分兩次相同。
- 補兩項只記錄的觀察：寫出時的數字格式、函式庫標頭不加 `-isystem` 的警告數。
- 查授權（讀下載後的檔）與維護狀態（`gh api`，2026-11-13）。
- 只讀複查（general-purpose、Opus 5.5／high、只讀；與作者同模型）：73 格，14 條（must 4、should 6、note 4），全部接受。改判 1 格：jsoncons 判準 1 條件通過→不通過（不成對的代理跳脫被靜默改寫，初稿寫的補法不成立）。另依複查 #2 補跑 yyjson 在原文旗標下的判準 1（只記錄）：多接受 5 個溢位數字，與 Boost.JSON 相同。其餘是理由的更正（§4 的 J1／J4 理由、Boost 不是多人維護）、揭露（判準 3 第二次的判定範圍變更）、補記（容器執行紀錄 `results/docker-runs.txt`、旗標、下溢的行為差異、未測項目）。逐條在報告 §7。

## 結果（linux-x64，Docker；windows-x64、macos-arm64 確認できない）

- 判準 1：yyjson、simdjson 通過（yyjson 以 `NOFLAG` 量；原文旗標下多接受 5 個溢位數字，只記錄）；Boost.JSON 條件通過；jsoncons 不通過（代理跳脫被靜默改寫）；nlohmann（今天的 `StrictJson`）與 RapidJSON 因接受 `123`＋NUL（JSONTestSuite 的 `n_`）判不通過。repo 內 70 個 JSON，6 個候選與基準結果相同。
- 判準 2：6 個全對；偵測都能放在 adapter（handler／SAX／visitor 或走訪保留重複成員的 DOM）。
- 判準 3：`std::from_chars`（libstdc++）對 Python 參考值 0 件不一致（固定 36、生成 200,000、整數 10,008）。原文路：Boost.JSON、yyjson、simdjson 0 不一致；nlohmann、jsoncons 交不出整數原文（`-0` 失去符號）；nlohmann、RapidJSON 拒絕含超大數的整份文件。候選轉換路：生成集上 6 個都 0 不一致，差異只在 `-0`（6 個都當整數 0）與超出範圍的數。repo 內的 JSON 沒有 `-0`。下溢（`1e-400` 等）在原文路會變成錯誤（今天讀成 `+0.0`）。
- 判準 4：6 個都保留順序；simdjson 沒有可改的樹，條件通過。
- 判準 5：linux 編譯 6 個都通過；授權都寬鬆；都有人維護，RapidJSON 實質停止（12 個月 1 commit、release 停在 2016）。glaze 要 C++23，不通過。
- 判準 6（只記錄）：yyjson 與 simdjson 最快，nlohmann 最慢（biome 1.4 MB：3.8／4.3 對 41.2 ms）。

## 證據

主 checkout `build/evidence/json-library-survey/`（本機 macOS x86_64，git 管理外）：`downloads.txt`、`usage-grep-20261113.txt`、`maintenance-20261113.json`、`probe/`、`vectors/`、`results/linux-x64-docker/`（`container.txt`、`build/`、`raw/`、`summary-run1-c1-c2-c3-c4-c6.json`、`summary-c3.json`）、`results/extra/`、`lib/`、`dl/`、`corpus/`。

## 範圍的限制（後續批的事前宣告要照寫）

- 判準 3 的三平台判定、判準 5 的 MSVC 與 macos-arm64 編譯：確認できない（使用者選 C）。
- 判定照宣告字面；`-0`、超出 double 範圍的數字、`123`＋NUL 的字面判定與實際影響不同，交使用者決定要不要改判準的範圍（報告 §6）。
- 宣告中的「有人維護」定義、判準 1「條件通過」分級沒有明確回覆，照宣告寫法判定。

## 用時

宣告約 1.5 h；實測（下載、程式、兩次容器實測、補充觀察）約 1 h；報告與 dev_log 約 1 h。估 9.5～12.5 h 的下限以下：既有的 L1 映像可直接用、候選 API 的事實大多一次讀到。

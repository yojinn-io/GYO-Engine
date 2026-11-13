# 架構 v8：v8-arch-kernel-scaffold（kernel 進 L1 的腳手架）

日期：2026-11-13。Owner：架構（架構師會話開出的任務卡）。計畫：[遷移計畫](../architecture/plans/kernel/migration-plan.zh-Hant.md) §3.3、§3 共通、§4 Delta-1、§7 U5；事前宣告 [kernel-scaffold-declaration.zh-Hant.md](../architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md)（架構師 2026-11-13 核定，附 6 點修訂）。
- 執行：本卡會話（Opus 5.5／high；commit 的 Co-Authored-By 照實寫 Opus 5.5），工作樹 `.claude/worktrees/objective-mclaren-370123`，分支 `claude/arch-kernel-scaffold`，PR #204。審查與敵對檢查合成一次：Fable／high 只讀（核定修訂第 5 點）。
- 證據：主 checkout `build/evidence/kernel-scaffold/`（git 管理外；本機 Intel Mac）：`configure-test.log`、`ctest-kernel.txt`、`os-boundary-guard.txt`、`mutations-run1.txt`～`mutations-run6.txt`、`review/`（審查報告摘要、審查者的 `mut.py`／`probe.py`）、`l1-43352865028/`（第一輪 L1：三平台 artifact 的 tests.xml 與 job log）、`l1-43355513501/`（第二輪 L1，最終程式 `0a2b2f7b`）。

## 件数の突き合わせ

**一致**——L1 裡 kernel 的 5 個 ctest（最終 commit `eb47b6ec` 的 run 43357263283 attempt 2：三平台＋core 共 20 個全 pass；attempt 1 windows-x64 因已登記的產品測試逾時失敗，見〈停止〉）。前幾輪：第一輪（`b31bae5a`，run 43352865028）linux-x64 5＋core 5、windows-x64 5、macos-arm64 5，共 20 個全 pass、0 skip；第二輪（最終程式 `0a2b2f7b`，run 43355513501）同樣 20 個全 pass、0 skip。突變：79 條＝killed 79、survived 0、wrong_failure 0、stale 0、skipped 0（第六輪，A8 修訂後）。審查者的敵對檢查：試 34 個突變，killed 7、存活 27（其中 22 處檢出力下降、1 處只增誤報、4 處等價或造作）；22＋1 處全部補了自測與突變。

## 做了什麼

- `engine/kernel/`：INTERFACE target `gyo_kernel`／`GYO::Kernel`（只連 `GYO::Base`），空殼 `Kernel.hpp`（`namespace GYO::Kernel`）。
- `engine/CMakeLists.txt`、`tests/common/CMakeLists.txt` 各 1 行 `add_subdirectory(kernel)`（`entity_store` 之後）。
- `tests/common/kernel/`：
  - `gyo_kernel_tests`：smoke 與 J-10 形狀 (b)（trait＋doctest；含 (a) 沒建的參考捕獲案例）。
  - `kernel_guard.py`＋自測 70：spec J-9（target/kernel DI-KS 3、15；`docs/architecture/target/naming.zh-Hant.md` DI 4；target/error-handling DI 2）。K0～K7。
  - `naming_guard.py`＋自測 31：target/naming DI 1（目錄↔namespace 一張表，正反兩向；反向根不存在＝跳過、`engine/kernel` 不存在＝失敗）與 DI 3（禁止後綴）。
  - J-10 形狀 (a)：configure 時 `try_compile` 兩次（對照組必須編過；拒絕組必須失敗且輸出含 `GYO_KERNEL_SAMPLE_REJECTED`），`NO_CACHE`，樣本檔列為 configure 依賴。
  - `gyo_expect_links(gyo_kernel GYO::Base)`（DEFER）。
- `docs/architecture/plans/kernel/scripts/mutations.json`：79 條。

## 受理判定（事前宣告 §4）

| # | 判定 | 結果 |
|---|---|---|
| 1 | 清單外零檔 | 成立：`git diff --name-only origin/master...HEAD` 對 17 檔清單，清單外 0（Python 比對，正例 `engine/CMakeLists.txt` 先命中） |
| 2 | L1 三平台綠、5 個 ctest × 3 份、Linux core 另驗 | 成立：最終 commit `eb47b6ec` 的 run 43357263283 attempt 2 三平台 success，kernel 測試 3×5＋core 5 全 pass。attempt 1 的 windows-x64 失敗（產品測試逾時）記在〈停止〉。前兩輪（b31bae5a、0a2b2f7b）也成立 |
| 3 | 兩種樣本先過 windows-x64 | 成立：windows-x64 configure log 兩輪都是對照組編過、拒絕組以 `GYO_KERNEL_SAMPLE_REJECTED` 失敗（745／411 ms、621／286 ms），(b) 的 case 在 `gyo_kernel_tests` 過。三平台各兩次，(a)(b) 失敗 0 次 |
| 4 | J-9 自測正反例 | 成立（70 個；K1 三組各有正例、`Despawn`／`ExtractRes`／`zone`／`matches`／`Matcher`／`rematch`／`nearest` 等反例不誤報；白名單恰 2 個） |
| 5 | naming 自測 | 成立（31 個；兩種根的行為各一例） |
| 6 | `gyo_expect_links` 反例 | 成立：突變 `ks-links-extra`（多連 `GYO::Math`）configure 失敗、訊息命中 |
| 7 | 突變 n/n；Fable 敵對檢查 | 成立：79/79。敵對檢查找到 22＋1 處，全部加入後 78/78；A8 修訂再加 1 條 |
| 8 | 既有守衛不因本批變紅 | 成立（第一輪 L1 全綠；本機 `os_boundary_guard` 通過） |

## U5 的分工（交 registry-schedule 的宣告）

兩種形狀在三平台各跑兩次（兩輪 L1），都沒有失敗；(a) 在 configure 多花 Windows 約 1.2 s、Linux 約 0.3 s、macOS 約 0.5 s，(b) 無額外成本。沒有觀察到 (a) 在 Windows 不穩，所以照 U5 已定：**J-10 核心三條（帶捕獲的 lambda、`World&` 參數、回傳型別）用 (a) `try_compile`，其餘用 (b) trait＋doctest**。給下一批的三個注意：

- (a) 必須有對照組並以訊息辨識失敗原因；只看「編不過」會把 include 或旗標錯誤當成預期失敗（突變 `ks-sample-a-wrong-failure` 證明辨識有效）。
- CMake 會把 `FATAL_ERROR` 訊息折行，突變的 `expect` 要取折行前的片語（第二輪曾因此 `wrong_failure`）。
- (a) 的 include 目錄要與 `GYO::Kernel` 給使用者的一致（kernel＋base）；審查 A4 指出初版只傳 kernel，Kernel.hpp 一旦 include base 對照組就會編不過。已補。

## 經緯

1. 宣告送 `parent` 回「session parent not found」，改用 `list_sessions` 找到架構師會話直接送；架構師核定並附 6 點修訂，全部改入宣告後開工。之後架構師另告知 Q10 定案（`neutral_names_guard` 不動），去處表同步改為定案。
2. 本機第一次跑突變時，背景任務的完成通知比執行器早到；我以為已結束又在前景跑了一輪，還手動還原了正在被測的 `engine/kernel/CMakeLists.txt`。兩個執行器重疊，第一輪結果作廢（`mutations-run1.txt` 留作記錄）。教訓：等背景執行器用 `ps` 確認行程消失，不看通知；這個環境 `kill -0` 拿不到權限，會誤判成已結束。
3. 第一輪作廢結果裡 `ks-sample-a-wrong-failure` 存活：突變把變數換成未宣告的名字，但 `static_assert` 仍被實例化，標記照樣出現。這是突變寫錯，不是檢查錯；改成讓拒絕組在進 `Register` 前就失敗。第二輪變成 `wrong_failure`（CMake 折行），縮短 `expect` 後第三輪 37/37。
4. Fable 審查（報告摘要在 `review/review-report.md`）：程式碼 11 項、敵對檢查 22＋1 處存活。本批修：A1（漏掃副檔名→K0）、A2（`..`）、A3（include 解析 fail-open、BOM、行錯位）、A4（樣本 include 目錄）、A5（K4 對合法函式形狀的誤報）、A6 後半（白名單改成比對宣告形狀）、A7（K2／K3／K6／K7 缺口）、A9（naming 的 linkage 區塊、屬性、typedef、限定名、`inline` 削字）。自測迭代守衛自己常數的地方改成固定清單（否則縮小常數的突變會存活）。第四輪 77/78：新加的 `ks-n3-typedef-dropped` 替換後語法錯誤（突變寫錯），修正後第五輪 78/78。

## 交架構師判斷的兩項（架構師 2026-11-13 定）

- **A8：K1 對登記 id 的子字串比對 → 本批改成詞邊界**（宣告 §9 的核定後修訂）。id `arena` 時 `Marenal`、`parenage` 不命中，`FrameArena`、`ArenaHost` 仍命中；架構師不採「只比對完整識別字」，因為會漏掉 `ObjectFpsPvpHost` 這種以產品名當前綴的形狀。命中訊息寫兩種讀法與處置（改 Kernel，或改新產品的登記 id），撞名的代價落在新產品命名而不是共通層。自測 67→70、突變 78→79（`ks-k1-word-boundary-dropped`），第六輪 79/79，L1 重跑。
- **A6 前半：沒有 `static` 關鍵字的可變全域 → 本批不擴大，記為限界**（宣告 LIM（本批）-7）。

## 給 registry-schedule

- **符號層的可寫全域檢查**（架構師 2026-11-13 定，承接審查 A6 前半）：J-9 是關鍵字制的文字掃描，看不到 namespace 範圍的 `inline int g = 0;`、匿名 namespace 變數與 `extern`；用文字近似語法誤報與漏報都多，而 scaffold 的 kernel 是 INTERFACE、沒有目的檔。`v8-arch-kernel-registry-schedule` 是第一個有 `.cpp` 的批，它的事前宣告必須新增：對 kernel 靜態庫看可寫資料段（.data／.bss）的符號，白名單恰好是 `tlCurrent` 與 `TypeKey` 的那個符號（屆時定名，取代暫名 `typeKeyTag`）；Linux 與 macOS 用 `nm`，Windows 先記為限界。
- J-10 的分工與三個注意見上節「U5 的分工」。

## 停止（2026-11-13，AGENTS §14.1）

A8 修訂後的第三輪 L1（`eb47b6ec`，run 43357263283）windows-x64 失敗，判定 2 在最終 commit 上不成立。依 §14.1「宣告的判定不成立時，重跑或修正前先報告使用者」，沒有重跑。

- 失敗的只有產品測試 `object_fps_pvp.cpu`（Timeout，60.01 s）。kernel 的 5 個測試在三平台的 tests.xml 都 pass，linux-x64、macos-arm64 全綠。
- 同一測試在 windows-x64 前兩輪的耗時是 54.13 s、30.93 s；Linux 約 17～37 s、macOS 約 16 s。Windows 一直貼著 60 s 的上限。
- 第二輪到第三輪之間只改了 `kernel_guard.py`、它的自測、突變目錄與文件，沒有 C++ 或產品檔。kernel 測試總共約 2 s，ctest 並行時也只多佔一個 slot。
- 這是已登記的觀察項，不是新現象：kernel HANDOFF〈觀察（改造完成後重查）〉與決定行 `v8-arch-ci-windows-observation`（使用者 2026-11-12：改造期間不追查，由 plugins-foundation／client-link 收口、closeout 重掃）。統計在主 checkout `build/evidence/ci-windows-cpu-flaky/tally.md`；只改文件的 PR #190（run 43200345263）也出現過同一測試 60 s 逾時。本批三輪的耗時已追加到該 tally（2026-11-13 補記）。
- 切分（§14.1 的三類）：實作（本批）的可能性低，理由如上，且只改文件的 PR 也出現過（獨立根據）；環境（runner 速度）的可能性高，理由是同一 commit 系列裡耗時在 31～60 s 之間大幅波動；測試自身（60 s 上限偏緊）也有可能。只靠這三輪無法斷定，屬「切分不了」。
- 證據：`build/evidence/kernel-scaffold/l1-43357263283/`（三平台 tests.xml、三個 job log）。

**解除（使用者決定 (a)，經架構師轉達）**：重跑一次、兩次都記錄、判定 2 的文字不改。

| 次 | run | windows-x64 `object_fps_pvp.cpu` | kernel 的 5 個測試 | L1 |
|---|---|---|---|---|
| 第一次 | 43357263283 attempt 1 | ***Timeout 60.01 s | 三平台＋core 全 pass | windows-x64 failure |
| 第二次 | 43357263283 attempt 2（同 commit `eb47b6ec`） | Passed 38.17 s | 三平台＋core 共 20 個全 pass | 三平台 success、CI gate success |

`gh run rerun --failed` 實際重跑了三個 L1 job（不只 windows-x64）。attempt 2 的 log 在 `l1-43357263283/attempt2/`；兩次的耗時已追加到 `ci-windows-cpu-flaky/tally.md`。判定 2 以最終 commit 的 attempt 2 成立，attempt 1 的失敗保留在本節（§14.1：不以後來的成功覆蓋）。

## 範囲の限界（宣告 §6）

STATIC＋PRIVATE 連 `GYO::Os` 不在本批；MSVC `/W4` 零警告只由 L1 判，且 repo 沒有 `/WX`，所以是讀 build log 確認（兩輪 windows-x64 log 中 kernel 測試都是 0 警告）；樣本是測試用最小版、非 J-10 條目；J-9 看不到無 `static` 的可變全域（LIM-7，交 registry-schedule 的符號層檢查）；naming 只有一列根表、DI 2 不做、產品列由產品側資料給（match-sim 提）；`typeKeyTag` 是暫名；突變只在本機跑。

## Architecture Report

- Architecture Delta：遷移計畫 §4 Delta-1 的第一步（目錄與 build 2 行），無差分。
- 新 Dependency：`GYO::Kernel → GYO::Base`（INTERFACE）。
- Ownership、Product Boundary、Data Contract：不變。K1 讀 `engine/config/projects.csv`、`tools.csv`（共通登記資料），產品別名 4 個逐行註出處（架構師核定）。

# 2026-11-21 GYO2：os-subsystem 清單裡 6 個在基準上跑不起來的突變

批次代號：`v8-arch-gyo2-stale-mutants`。分支 `claude/gyo2-stale-mutants-b80896`（從 `gyo2` f25837b0）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。

## 起因

#257（`engine/` 分層、登記檔搬到頂層 `config/`）的突變重跑留下 6 個在 `gyo2` 基準上就跑不起來的突變（`docs/dev_logs/2026_11_21_gyo2_engine_layers.zh-Hant.md` 的「突變重跑」一節）。使用者 2026-11-21：「asset 和那 6 個突變的小清理，連實作都可以現在開」。

範圍：只碰 `docs/architecture/plans/os-subsystem/scripts/mutations.json` 裡這 6 個，以及敵對式檢查補上的。同一份清單裡其他對不上的（master 時代的 `engine/trace`、`engine/asset` 等）與 kernel 清單的 3 個（`ks-k6-*`、`ks-k8-*`，第 4 批正在改）不碰。

## 每個突變的處置

先問：它要證明哪個守衛有檢出力？那個守衛還在不在？

| 突變 | 守的東西 | 守衛現況 | 處置 |
|---|---|---|---|
| `os-g8-link` | os.md DI 3：`GYO::Os` 只連 `GYO::Base` 與系統函式庫 | `gyo_expect_os_links`（`tests/common/os/CMakeLists.txt`）還在 | 改寫：改 `config/modules.csv` 的 os 列 depends `base` → `base;math` |
| `ti-x1-time-links-more` | time.md DI 10：`GYO::Time` 只連 `GYO::Base`、`Threads::Threads` | `gyo_expect_time_and_thread_links`（`tests/common/time/CMakeLists.txt`）還在 | 改寫：time 列 depends `base` → `base;math` |
| `ti-x2-threads-links-more` | threads.md DI 9：`GYO::Threads` 的連結集合 | 同上 | 改寫：threads 列 depends `time` → `time;math` |
| `th-x1-threads-os-public` | threads.md DI 9：`GYO::Os` 只能 PRIVATE | 同上的「exposes GYO::Os」檢查還在 | 改寫：threads 列把 `os` 從 private_depends 移到 depends |
| `os-g13-definition-name` | os.md DI 1：非 os 的 target 不得有辨識 OS 的編譯定義（名字分支、target） | OS definition guard（`tests/common/os/CMakeLists.txt`）還在 | 改寫：在 `engine/core/time/CMakeLists.txt` 給 `gyo_time` 加 `GYO_ENGINE_POSIX=1`（原本改的 `engine` target 已不存在） |
| `os-g14-definition-platform-id` | os.md DI 1：`$<PLATFORM_ID>` 分支、目錄層級 | 同上 | 改寫：在 `engine/CMakeLists.txt` 開頭加目錄層級的 `$<$<PLATFORM_ID:SunOS>:GYO_FAST_PATH>`（原本 find 的 `add_subdirectory(base)` 已不存在；平台由 Windows 改 SunOS 見下一節） |

- 4 個連結突變沒有退役。理由：守衛（`gyo_expect_links` 的連結集合檢查）沒有被取代，只是 Engine 內連結的輸入從各單位的 CMakeLists 換成登記檔（O-6）。新機制本身的突變（`or-guard-real-o6-hand-link`：手寫連結被 O6 抓；`ur-private-as-public`、`g8s3-public-as-private`：登記檔到連結的轉換）守的是「連結從哪裡來」，不守「os／time／threads 的連結集合是不是這幾個」。登記檔允許的新依賴只有 `gyo_expect_links` 會擋，所以把突變改打登記檔，比照 `or-links-more` 的先例。
- `os-g8-link` 原本加的是 `GYO::Time`（os 之上的 Module）。登記檔照建置順序檢查，os 列寫 `time` 會先被「comes later」擋下、訊息不同，所以改用排在 os 之前的 `math`。「os 連到它之上的 Module」改由下一節的 `os-g19` 表達。
- expect 都收緊到這個突變自己的完整訊息（實際與期待的整串連結清單，或 owner、屬性、條目），空格改 `\s+`（CMake 會折行）。`os-g8-link` 的 expect 用 `(?:Threads::Threads|shell32)` 同時接受 POSIX 與 Windows 的系統函式庫；Windows 這一側沒有在本機跑過。
- 6 個都補上 `guards`（`tests/common/mutation/README.md` 第 9 點：觸碰時補）。

## 敵對式檢查

- 檢查者：Fable 5.2／high，唯讀（作者是 Opus 5.5）。檢查者另在暫存區用同樣的守衛文字搭小型 CMake 專案探測，未保存。
- 找到 4 處「守衛改了、突變全部照樣 killed」，加 1 處守衛本身過期：

| # | 缺口 | 處置 |
|---|---|---|
| 1 | `os-g14` 用 `PLATFORM_ID:Windows`，大寫後的 `:WINDOWS>` 也中名字清單，`PLATFORM_ID` 分支刪掉照樣 killed | 改寫 `os-g14`：平台改 SunOS（不在名字清單裡） |
| 2 | 三個 `cmake_language(DEFER …)` 改成當場呼叫，所有突變照樣 killed（它們都改 `engine/` 或登記檔，比 `tests/common` 先 configure；O6 只掃 `engine/`） | 新增 `os-g19-links-from-later-directory`、`ti-x5-links-from-later-directory`、`os-g20-definition-from-later-directory`，都從較晚 configure 的 `tests/common/audio/CMakeLists.txt` 動手。`os-g20` 也是唯一落在 `tests` 掃描根下的突變 |
| 3 | `-D`／`/D` 編譯選項的分支沒有突變走到 | 新增 `os-g21-definition-option`（`target_compile_options(gyo_time PRIVATE -DGYO_LINUX=1)`） |
| 4 | `GYO_OS_DEFINITION_EXEMPT` 還寫 `engine/os`，os 已在 `engine/core/os`（f82a8e36 改了同檔第 16 行、漏了這行） | 不改，列為後續：修正會讓 `gyo_os` 的 OS 定義由失敗變通過，屬於放寬 DI 守衛（AGENTS §14.4），要使用者核准，也超出本卡範圍 |

- 檢查者另提、本卡不做的：
  - `INTERFACE_COMPILE_DEFINITIONS` 這一項沒有突變。README 第 4 點「清單的突變一個就夠」，屬性清單已有 `os-g13`（COMPILE_DEFINITIONS）、`os-g21`（COMPILE_OPTIONS），不再加。
  - 掃描根沒有 `examples/`、`plugins/`；名字清單沒有 MAC、WIN 等。擴大掃描根或名字清單是規則變更（README「尋ねるもの」），列為後續。
  - `th-x2-os-without-thread-library`、`os-g18-windows-extra-link` 的 expect 寬鬆（`gyo_os links .*expected…` 也會中別的突變的失敗）。不在這 6 個裡，列為後續。
- 加的 4 個逐一對照 README 第 1、3、9 點：各自捕捉一個清單裡沒有別的突變捕捉的錯誤（各寫在 `what`），都有 `guards`。

## 重跑結果（本機 macOS Intel，AppleClang，preset `test`）

- 改寫的 6 個加新增的 4 個：10/10 killed（每次都先確認未改動的樹通過，`run_mutations.py` 用 `-B`、先清 `__pycache__`）。
- 反向確認（新突變真的釘住那幾行）：把守衛暫時改壞，再跑對應的突變，5 個都變 survived：刪 `PLATFORM_ID` 分支 → `os-g14` survived；三個 DEFER 各改成當場呼叫 → `os-g19`、`ti-x5`、`os-g20` survived；`-D` 解析改成 `if(TRUE)` → `os-g21` survived。改完都還原，工作樹只剩清單的變更。
- 清單：149 → 153（改 6、加 4、退 0）。
- 證據：主 checkout 的 `build/evidence/gyo2-stale-mutants/`（`run-six.log`、`run-ten.log`、`reverse-check.log`、`reverse.py`、configure 與建置的 log），git 不追蹤，在使用者的 Intel Mac 上。

## 後續（不在本卡）

- OS definition guard 的豁免路徑 `engine/os` → `engine/core/os`（放寬 DI 守衛，§14.4 要使用者核准）。
- OS definition guard 的掃描根（`examples/`、`plugins/`）與名字清單（MAC、WIN 等）要不要擴大：規則變更，由使用者決定。
- `th-x2-os-without-thread-library`、`os-g18-windows-extra-link` 的 expect 收緊。
- 清單的 `description` 指向已不存在的 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py`（現在是 `tests/common/mutation/run_mutations.py`）；`docs/architecture/plans/entity-store/scripts/mutations.json` 的 `es-row9-links-math`、`docs/architecture/plans/audio/scripts/mutations.json` 的 `au-link-engine` 的 find 看起來也還是手寫連結的寫法，沒有跑過、沒有確認。
- `2026_11_21_gyo2_engine_layers.zh-Hant.md` 沒有列在 `docs/architecture/target/v2/README.md` 的 dev_log 一覽（圖書管理員）。

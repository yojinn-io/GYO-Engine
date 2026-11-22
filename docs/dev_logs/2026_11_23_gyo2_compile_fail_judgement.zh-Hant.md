# 2026-11-23 負向編譯樣本的判法（`v8-arch-gyo2-compile-fail-judgement`）

Owner：Kernel（GYO2 主會話開的任務卡 `local_b3d28b59` 的第一件）。分支 `claude/gyo2-kernel-summary-msg-763842`（開向 `gyo2`）。作者 Opus 5.5。

## 為什麼

asset 宣告 [`asset-declaration.zh-Hant.md`](../architecture/target/v2/asset-declaration.zh-Hant.md) 第 15 節第 12 項回報：Kernel 的負向編譯樣本（`tests/common/kernel/CMakeLists.txt`）只在 try_compile 的輸出裡找標記。編譯器會把出錯的那一行原始碼印出來；如果標記所在的那一行另有一個無關的硬錯誤（例：型別名打錯），輸出裡就有標記，樣本照樣通過，守衛失去檢出力。asset 自己改用「靜態斷言失敗＋標記在同一行」。批次表把它列在後續小事，主會話開卡（使用者 2026-11-23「開 audio 那一側的小批和 Kernel 兩件」）。

## 先重現（AGENTS §14.3）

1. 在 Kernel 的突變清單加 `rs-sample-marker-on-error-line`：把樣本 `HANDLE_COPY` 的 `static_assert` 改成一行，條件裡的型別名打錯（`SamplePosTypo`），標記仍在同一行。（合併前審查 S1 之後，前面再加一行無關的 `static_assert(false, …)`，見〈突變〉。）
2. 舊判法下跑它：**survived**（0/1 killed）。樣本照樣通過，空過成立。
3. 直接用 clang 編譯同一份改過的樣本：只有 `use of undeclared identifier 'SamplePosTypo'` 一個錯誤，沒有靜態斷言失敗；但 clang 印出的原始碼行裡有 `GYO_KERNEL_SAMPLE_HANDLE_NOT_COPYABLE`。

證據：主 checkout `build/evidence/gyo2-kernel-summary-msg/`（`old-judgement-marker-on-error-line.log`、`mutated-sample-clang-output.txt`；git 不追蹤，這台 macOS Intel 機器，Apple clang 21.0.0）。

## 做了什麼

- 新檔 `tests/common/support/ExpectStaticAssert.cmake`：`gyo_expect_static_assert_failure(<check> <variant> <marker> <output>)`。輸出裡要有一行「靜態斷言失敗」（clang、gcc 是 `static assertion failed`，MSVC 是 `static_assert failed`），而且同一行後面有標記，否則停下 configure。判法和 asset 的相同，正規式也相同。
- 三處負向編譯樣本都改呼叫它：`tests/common/kernel/CMakeLists.txt`（原本只找標記）、`tests/common/world_logging/CMakeLists.txt`（同上）、`tests/common/world_asset/CMakeLists.txt`（原本就是這個判法，換成共用函式）。
- 錯誤訊息保留「variant X failed without MARKER」開頭，既有突變的 expect 不必改。
- `engine/ecs/kernel/include/engine/kernel/SystemTraits.hpp`：一句註解跟著改（「look for the marker on a static assertion's failure line」）。
- 掃過全 repo：負向編譯的判定只有這三處。`git grep -i "compile.fail"` 限 `CMakeLists.txt` 只命中這三個檔；`git grep try_compile` 在 `docs` 以外命中這三個檔、新的 `.cmake` 與兩句註解（`SystemTraits.hpp`、Kernel 的樣本）。三處都在範圍內：卡片的指示是「所有用這套判法的樣本都要檢查一遍：Kernel 的，以及 world_logging 等其他用同一判法的」。

為什麼做成共用函式：這個漏洞正是複製判法時各寫各的造成的（asset 改了，Kernel 與 world_logging 沒改）。同一件事只留一個出口（批次表〈每批的設計審查〉第 1 項）。

## 判定的變更（AGENTS §14.4）

1. **守的性質**：負向編譯樣本只在「預期的靜態斷言失敗」時算通過（§14.2）。
2. **緩的面**：無。舊判法要「輸出裡有標記」；新判法要「輸出裡有一行靜態斷言失敗，而且同一行有標記」，後者一定滿足前者。所以舊判法判失敗的，新判法也判失敗。
3. **緊的面**：標記只出現在別的行（例：編譯器印出的原始碼行），沒有對應的靜態斷言失敗時，以前通過，現在失敗。
4. **緩掉的性質由誰守**：沒有緩的面。
5. **根據與證據**：
   - 舊判法放行、新判法擋下：`rs-sample-marker-on-error-line` 舊判法 survived、新判法 killed。
   - 舊判法判失敗的全部仍判失敗：既有的負向編譯突變全部重跑，都 killed（下節）。
   - 現行三處樣本在新判法下全部通過（`configure-new-judgement.log`：kernel 12 個變體、world_asset 10 個、world_logging 2 個）。

這是 Design Invariants 的守衛（Kernel 的 J-10 等、logging 的 LG-3、asset 的 AS-10／AS-11）的判定。AGENTS §14.4 規定 Design Invariants 的守衛不論有沒有緩的面都要使用者核准，所以上面五點在合併前交使用者核准（初版寫成「事後報告」，合併前審查 M1 指出不對）。

## 檢驗

本機 macOS（Intel，RelWithDebInfo，preset `test`）。

突變（`mutation-rerun-scope`：殺它們的判定改了，所以重跑所有負向編譯的突變）：
- Kernel 清單 16 個：`rs-sample-*`（含新加的）、`rs-j10-member-accepted`、`rs-j11-self-conflict-off`、`rs-j13-condition-write-allowed`、`rs-adv-*`、`we-j4-handle-copyable`、`sr-static-assert-removed`、`msg-hook-reader-accepted`、`obs-*` → 16/16 killed（`new-judgement-kernel-mutants.log`）。
- logging 清單 `lg-forward-readonly-assert` → 1/1 killed（`new-judgement-logging-mutants.log`）。
- world_asset 清單 `aw-sample-*` 10 個 → 10/10 killed（`new-judgement-world-asset-mutants.log`）。
- 合計 27/27，每個都照它自己的 expect 算。
- 合併前審查 S1 之後改了 `rs-sample-marker-on-error-line` 的 `replace`，重跑它：現行判法 killed（`s1-current-judgement.log`）。

## 突變（§14.5）

| id | 只有它抓得到的 | 守的 |
|---|---|---|
| `rs-sample-marker-on-error-line` | 判法不要求「標記在靜態斷言失敗的**那一行**」。樣本先有一個無關的靜態斷言失敗，下一行是帶標記、型別名打錯的硬錯誤：只找標記的判法放行，「有靜態斷言失敗、之後任何地方有標記」（`.*`）的判法也放行。`rs-sample-wrong-failure` 的錯誤行上沒有標記，抓不到這種 | asset-declaration 第 10 節「負向編譯的判法」；AGENTS §14.2 |

一個。三處樣本呼叫同一個函式，判法本身由這一個突變守；world_logging、world_asset 不另加（§14.5「同一個誤り」）。

## 合併前的設計審查

Fable 5.2／high（唯讀，沒有建置）。報告：主 checkout `build/evidence/gyo2-kernel-summary-msg/merge-review-fable.md`。結論：設計合理，判法收成一個出口是對的，沒有擋誤用的守衛；must 1、should 4、nit 4。

| 項 | 處理 |
|---|---|
| M1 Design Invariants 的守衛要使用者核准，不能事後報告 | 〈判定的變更〉改寫；五點在合併前交使用者核准 |
| S1 突變只證明「要有靜態斷言失敗」，沒守到「同一行」（`[^\n]*` 換成 `.*` 仍會 killed） | 突變的 `replace` 前面加一行無關的 `static_assert(false, …)`。實跑三種判法：現行判法 killed，`.*` 判法 survived，舊判法（只找標記）survived（主 checkout `build/evidence/gyo2-kernel-summary-msg/s1-*.log`） |
| S2 `guards` 只寫 AGENTS §14.2，日後搜不到 | 加上 `asset-declaration 第 10 節「負向編譯的判法」` |
| S3 判法假設編譯器訊息是英文；MSVC 17.x 之前的 `error C2338: <訊息>` 不符合 | `ExpectStaticAssert.cmake` 註解加一句前提（方向是多擋、不會空過）；不加程式。現行 CI 的三個編譯器字樣都符合 |
| S4 判法以外的骨架（對照組、迴圈、MSVC 旗標）仍是三份 | 不在本批；回報主會話記進後續小事 |
| N1 「無緩的面」的前提是標記沒有正規式字元 | 現有 23 種標記都是 `[A-Z_]+`；寫進 `.cmake` 的註解 |
| N2 掃描的描述少算 | 〈做了什麼〉改正 |
| N3 重現只在 Apple clang 做過 | 記下：gcc、MSVC 由 L1 跑既有樣本，空過的重現沒有在那兩家做 |
| N4 本節佔位、批次表那列 | 本節補上；批次表由主會話維護（gyo2 直接寫），回報主會話 |

# Kernel 測試的三筆舊帳（`v8-arch-gyo2-test-debts`）

日期：2026-11-16。分支 `claude/gyo2-test-debts-88cebc`（從 `gyo2` 3743eaa8 開出）。

這一批由任務卡「測試的舊帳」做（GYO2 重建主會話開出；使用者 2026-11-16 在主會話說「開個任務卡處理」「開任務卡」）。範圍只有突變清單 `docs/architecture/plans/kernel/scripts/mutations.json`、`tests/common/kernel` 與文件，不改 `engine/` 的行為。三件事照順序做，每件各開小 PR。

## 第一件：四個 `find` 對不上的突變

PR：（待開）。

逐個查了原本要打的性質、目標檔現在的文字、讓它對不上的變更。四個的性質都還在，處置都是「照現在的文字重寫 `find`」；沒有刪掉的、也沒有重寫後存活的。

| 突變 | 守的性質 | 讓它對不上的變更 | 處置 |
|---|---|---|---|
| `ks-links-extra` | `gyo_kernel` 的連結對象正好是宣告的那些（多連一個就 configure 失敗） | 2cfe42b2 把 world-exec 的程式搬進 `gyo2`，連結多了 `GYO::EntityStore`；突變清單沒有跟著搬 | `find`、`replace`、`expect` 照 `claude/arch-kernel-world-exec` 50cb2050 的版本 |
| `ks-adv-include-allowlist-widened` | K5 的專案 include 白名單固定（多一個 `engine/os/` 就被自測抓到） | 同上：白名單多了 `engine/entity_store/`，自測改名 | 同上，照 50cb2050 |
| `ks-k5-parent-directory` | 專案 include 裡有 `..` 要被 K5 報出來 | 同上：world-exec 改成也用反斜線切（`re.split`） | 同上，照 50cb2050 |
| `rs-j17-zone-b-listed` | J-17：(b) 區的 System 不列進 zone C sources 的匯出 | 4e68b589 把路徑檢查搬進和初始值共用的 `add` lambda | `find` 改成 `continue` 加上下一行 `add(system.source, "a zone C System"` 的開頭；`expect` 不變 |

每個突變的 `what` 末尾註明了來源與原因。清單的 `description` 沒有改：它是單獨一行，K0 那張卡也在往裡面加字，改了會在同一行衝突。

## 檢證

- 本機（macOS，x86_64）`cmake --preset test` 後完整建置。
- 全清單掃描：450 個突變的 `find` 在目標檔各出現正好一次（用 Python 讀檔計數）。正例：同一支掃描對 3743eaa8 的清單跑，正好報出這四個（`find-scan-base.log`）。
- 四個突變實際跑一次：4/4 killed，都是預期的訊息（`part1-mutants.log`）。

## 證據

全部在這台 Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-test-debts/`（git 不追蹤）：`configure1.log`、`build1.log`、`find-scan-base.log`、`part1-mutants.log`。

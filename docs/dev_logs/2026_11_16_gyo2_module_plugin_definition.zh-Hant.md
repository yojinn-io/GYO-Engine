# Module／Plugin 的定義收斂（`v8-arch-gyo2-module-plugin-definition`）

日期：2026-11-16。分支 `gyo2`。由 GYO2 主會話提出的任務卡做。

## 做了什麼

使用者 2026-11-16 改了 Module／Plugin 的定義（決定表 `v8-arch-gyo2-module-plugin-owner-definition`）：名字不換，判準從「少了它，完整的遊戲還成不成立」改成**誰維護、介面是誰的**。Module＝我們自己維護、介面是我們的（外部函式庫完全藏在我們介面後面的也算，例：JSON）；Plugin＝第三方官方維護、接進來用的（例：SDL 家族、ImGui）。網路改為 Module（`NetModule`）。這一批把定義與引用它的地方一次改齊，避免定義反覆改動。

- `AGENTS.md` §6：判斷的一句與兩條定義換成新判準。依使用者 2026-11-13「AGENTS 只放設計思想」的原則，例子與歸屬拿掉，改成「どの部品がどちらに当たるかは、架構の文書に書く」。11-14 加進去的例子（JSON、SDL、網路、ImGui）就是這次要跟著改的具體設計。只改 gyo2 的 AGENTS，master 的不動。
- [同詞異義表](../architecture/target/v2/rebuild-vocabulary.zh-Hant.md)：第 0 節末加「2026-11-16 改定」（原話、新定義、歸屬、參照的 O3DE／Unity 例子）；11-14 的段落保留原話，加註被取代；Module、Plugin 兩列改寫；第 6 節加第 5 點（未確認的三件事）；附錄 A、B 補上。
- 權威檢查：照主會話轉來的同日決定（`v8-arch-gyo2-authority-is-game-design`、`-authority-own-registration`）寫成「策略式的 Module，由作者選策略」，不是「產品選用」。
- [命名規範](../architecture/target/v2/rebuild-naming.zh-Hant.md) N20、N21 的例子；[時間設計](../architecture/target/v2/rebuild-time-design.zh-Hant.md)的「平台的 Module（SDL）」與 runner 歸屬的註；[r0b 報告](../architecture/target/v2/rebuild-r0b-report.zh-Hant.md)與[概念層](../architecture/target/v2/rebuild-concept.zh-Hant.md) X1 加註（紀錄不改寫）。

## 沒有改的

- v2 之前的 target 文件（`runtime-composition.zh-Hant.md` §4、`docs/architecture/README.md` 的〈Layers and dependencies〉）：使用者 2026-11-16 說「現在架構文檔是連到v2去了，之前就先不管了。後面統一處理」。§4 仍寫網路是 plugin、「Engine 自有 plugin」，等統一處理時改。
- 「plugin」當登記單位（Bevy 式 `Plugin` 型別）的用法：留給拆型別那一批（`v8-arch-gyo2-module-plugin-registers`）。
- 程式與註解：`Registry.hpp` 的 `Plugin` 註解沒有寫定義；`NetModule` 的程式還不在 gyo2 上，實作時照新名字。
- `docs/architecture/plans/` 下的舊計畫。

## 檢證

- 只改文件。用 `git grep` 掃 `Plugin|plugin|Module` 與舊說法（「成不成立」「Engine 外／內」「NetPlugin」「plugins/net」等），先以 `AGENTS.md:383` 命中確認掃描有效。掃描輸出未保存。
- `git diff --check` 通過。

## 還沒確認的（列給使用者，轉給主會話）

1. 產品（作者）能不能只選部分 Module：作者選不選權威 Module、選哪個策略，以它為前提。前例：O3DE `enabled_gems.cmake`、Unity 可關的內建模組。
2. SDL 的 adapter（我們寫、實作 Engine 定的後端介面）歸哪。讀法之一：第三方型別露出我們的介面→整包是 Plugin。
3. ufbx 歸哪。

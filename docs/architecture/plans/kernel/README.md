# Kernel 研究線：GYO 的組合協定（設計圖）

更新：2026-11-10。Owner：架構（架構師會話）。**狀態：研究與設計稿，未開工。** 起點是使用者的願景稿「GYO-Engine 架構思想研究」（以 Bevy 為參照的 Kernel／IoC／Plugin／Scheduler 思想）。
使用者 2026-11-10 的立場：**基盤的協定不能等壓力出現才改**（事後補的代價最高，是技術債的主要來源）；功能層的擴充才等壓力。本線的目的是把「設計圖」寫對——核心思想對了，功能是遲早的事。

## 方法

不列功能，找不變量：讀 Bevy 與兩個網路 plugin 的原始碼，問「這些功能都靠哪幾條不變量才成立」，再逐條問 GYO 有沒有、GYO 的約束（決定性、狀態的權威、Engine 中性、C++ 無反射與借用檢查）會不會讓它變形、變形後還守不守得住效果。

證據（只讀審閱的報告，每條有檔名行號）：主 checkout `build/evidence/kernel-study/`（`bevy_core.md`、`replicon.md`、`lightyear.md`，`render.md`、`assets.md`、`platform.md`、`capabilities.md`、`os_io.md`）。原始碼在 `~/Code/GitHome/{bevy, bevy_replicon, lightyear}`（使用者 2026-11-10 放入）。

## 文件

- [networking.zh-Hant.md](networking.zh-Hant.md)：網路側的設計稿——網路要成為 Kernel 上的 plugin，Kernel 必須提供什麼；網路層自己的設計圖；對 GYO 現有 Match／Gateway／pv9／預測／回溯的對照。
- [blueprint.zh-Hant.md](blueprint.zh-Hant.md)：設計圖——Kernel 八條不變量、各層補充的不變量（R／A／P／C／O／N）、GYO 刻意的偏離（候選 DI-K1～K7）、反例清單、現況對照、Replacement Criteria、場景驗證。
- [HANDOFF.md](HANDOFF.md)：記錄器。

## 與其他線的關係

- 第三方評估卡（`v8-arch-third-party-evaluation`，進行中）：每個候選方案要能當 Kernel 下的 plugin。
- 場景線（`docs/architecture/plans/scene/README.md`）：第一個非玩家實體與邏輯連線，是 Kernel 第一個真實的壓力來源。
- pv10 需求（`docs/object_fps_pvp/plans/v8/pv10-requirements.md`）：網路設計稿的輸入。
- AGENTS §2 的修訂候選：「基盤の協定は先に設計する」（交風紀委員，待使用者同意）。

# Kernel 研究線：GYO 的組合協定（設計圖）

更新：2026-11-11。Owner：架構（架構師會話）。**狀態：設計稿，未定案、未開工。**
起點是使用者的願景稿「GYO-Engine 架構思想研究」（2026-11-10）與「GYO 架構設計思想」Concept Draft v0.2（2026-11-11；Bevy ECS／Scheduler、Service Mesh、Control／Data Plane）。

使用者 2026-11-10：**基盤的協定不能等壓力出現才改**；功能層的擴充才等壓力。
使用者 2026-11-11：**要的是 Bevy 那種抽象能力（World／System Contract／Scheduler／Executor），帶入現有 GYO，不是 GYO2；架構改造完成前，功能線停。** 先不定案，目前在對理解。

## 方法

不列功能，找不變量：讀 Bevy 與兩個網路 plugin 的原始碼，問「這些功能都靠哪幾條不變量才成立」，再逐條問 GYO 有沒有、GYO 的約束（決定性、狀態的權威、Engine 中性、C++ 無反射與借用檢查）會不會讓它變形、變形後還守不守得住效果。

證據（只讀審閱的報告，每條有檔名行號）：主 checkout `build/evidence/kernel-study/`（`bevy_core.md`、`replicon.md`、`lightyear.md`，`render.md`、`assets.md`、`platform.md`、`capabilities.md`、`os_io.md`）。原始碼在 `~/Code/GitHome/{bevy, bevy_replicon, lightyear}`（使用者 2026-11-10 放入）。

## 文件

- [mechanism.zh-Hant.md](mechanism.zh-Hant.md)：**本線的基準**。對 v0.2 的理解、機制骨架、契約與控制面（含使用者提供的審查修正與架構師兩條主張）、網路作為 System、規則變機制、不解決什麼、改造的性質與風險、判準。三張圖在 `diagrams/`。
- [networking.zh-Hant.md](networking.zh-Hant.md)：網路側的設計稿——網路要成為機制上的 System，機制必須提供的八條；網路層自己的設計圖 N1–N12；對 GYO 現況的對照。
- [blueprint.zh-Hant.md](blueprint.zh-Hant.md)：**輸入**（2026-11-11 降級）——Kernel 八條不變量、各層補充的不變量（R／A／P／C／O／N）、GYO 刻意的偏離（候選 DI-K1～K7）、反例清單、現況對照、Replacement Criteria。它沒有契約分類與 Executor 的分離，遷移順序一節假設了改造路線；以 mechanism 為準。
- [HANDOFF.md](HANDOFF.md)：記錄器。

## 與其他線的關係

- 決定性等級（PR #177，`docs/architecture/determinism.md`）：(c)／(b) 區的定義；mechanism §3.3 的四處契約引用它。
- 第三方評估卡（PR #176）：每個候選方案要能當機制上的 System；選庫在責任邊界確認之後。
- 場景線（`docs/architecture/plans/scene/README.md`）：第一個非玩家實體與邏輯連線，是機制第一個真實的壓力來源。
- pv10 需求（`docs/object_fps_pvp/plans/v8/pv10-requirements.md`）：網路設計稿的輸入。
- AGENTS §2 的修訂候選：「基盤の協定は先に設計する」（交風紀委員，待使用者同意）。
